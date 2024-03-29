// PhysicalMaps.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


// 2MB
#define MEM_REQUESTED (1024 * 4)


BOOL
LoggedSetLockPagesPrivilege(HANDLE hProcess,
	BOOL bEnable)
{
	struct {
		DWORD Count;
		LUID_AND_ATTRIBUTES Privilege[1];
	} Info;

	HANDLE Token;
	BOOL Result;

	// Open the token.

	Result = OpenProcessToken(hProcess,
		TOKEN_ADJUST_PRIVILEGES,
		&Token);

	if (Result != TRUE)
	{
		_tprintf(_T("Cannot open process token.\n"));
		return FALSE;
	}

	// Enable or disable?

	Info.Count = 1;
	if (bEnable)
	{
		Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
	}
	else
	{
		Info.Privilege[0].Attributes = 0;
	}

	// Get the LUID.

	Result = LookupPrivilegeValue(NULL,
		SE_LOCK_MEMORY_NAME,
		&(Info.Privilege[0].Luid));

	if (Result != TRUE)
	{
		_tprintf(_T("Cannot get privilege for %s.\n"), SE_LOCK_MEMORY_NAME);
		return FALSE;
	}

	// Adjust the privilege.

	Result = AdjustTokenPrivileges(Token, FALSE,
		(PTOKEN_PRIVILEGES)&Info,
		0, NULL, NULL);

	// Check the result.

	if (Result != TRUE)
	{
		_tprintf(_T("Cannot adjust token privileges (%u)\n"), GetLastError());
		return FALSE;
	}
	else
	{
		if (GetLastError() != ERROR_SUCCESS)
		{
			_tprintf(_T("Cannot enable the SE_LOCK_MEMORY_NAME privilege; "));
			_tprintf(_T("please check the local policy.\n"));
			return FALSE;
		}
	}

	CloseHandle(Token);

	return TRUE;
}



int main()
{
	LPVOID MemAlloc, MemAlloc2;
	BOOL MapRet;
	ULONG_PTR NumPages;
	ULONG_PTR ModNumPages;
	ULONG_PTR NumPagesInitial;
	ULONG_PTR *PfnArrayMDL;
	SYSTEM_INFO sysInfo;
	ULONG_PTR pfnArraySize;


	// Get privs
	if (!LoggedSetLockPagesPrivilege(GetCurrentProcess(), TRUE))
	{
		printf("Priv failed\n");
		return 0;
	}

	// Get PageSize information from system
	GetSystemInfo(&sysInfo);
	printf("Page Size = %d\n", sysInfo.dwPageSize);

	NumPages = MEM_REQUESTED / sysInfo.dwPageSize;
	printf("Number of pages being requested = %d\n", NumPages);

	ModNumPages = (MEM_REQUESTED / sysInfo.dwPageSize) - sysInfo.dwPageSize + 1;
	printf("Bad Number of Pages requested = %d\nPress Key..\n", ModNumPages);

	getchar();

	pfnArraySize = NumPages * sizeof(ULONG_PTR);
	printf("pfnArraySize = %d\n", pfnArraySize);

	PfnArrayMDL = (ULONG_PTR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pfnArraySize);
	if (PfnArrayMDL == NULL)
	{
		printf("Failed to allocate heap\n");
		return 0;
	}

	//Allocate the physical pages
	NumPagesInitial = NumPages;
	BOOL bRes = AllocateUserPhysicalPages(GetCurrentProcess(), &NumPages, PfnArrayMDL);
	if (!bRes)
	{
		printf("Failed to Allocate User Pages\n");
		return 0;
	}

	// Allocate the Virtual Memory we're going to map onto the physical pages
	MemAlloc = VirtualAlloc(NULL, MEM_REQUESTED, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (MemAlloc == NULL)
	{
		printf("Failed Mem Alloc\n");
		return 0;
	}

	// Allocate	a second Virtual Address space
	MemAlloc2 = VirtualAlloc(NULL, MEM_REQUESTED, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (MemAlloc2 == NULL)
	{
		printf("Couldnt allocate regular memory\n");
		return 0;
	}
	printf("Allocation Addr For Physical Pages = %X\nAllocation Addr for Regular Memory = %X\n", MemAlloc, MemAlloc2);

	printf("Mapping Physical..\n");
	// Map the first VA to the physical maps
	MapRet = MapUserPhysicalPages(MemAlloc, NumPages, PfnArrayMDL);
	if (MapRet == FALSE)
	{
		printf("mapping failed\n");
		getchar();
		return 0;
	}

	printf("Writing to Memory\n");

	// Write data into the first Mapped VA
	char buffer[10] = { 'A' };
	SIZE_T written;
	BOOL bret = WriteProcessMemory(GetCurrentProcess(), MemAlloc, &buffer, 10, &written);
	if (!bret)
	{
		printf("Failed o WPM!\n");
		getchar();
		return 0;
	}
	
	printf("Wrote Memory...\n");
	getchar();

	printf("Unmapping original mapping\n");
	// Unmap the VA from the Physical Pages (this reverts the VA back to Reserved memory)
	MapRet = MapUserPhysicalPages(MemAlloc, NumPages, NULL);
	if (MapRet == FALSE)
	{
		printf("mapping to unmap failed\n");
		getchar();
		return 0;
	}

	printf("Remapping new VA\n");
	// Remap the other VA to the same Physical Pages, this remaps the previous data into this new VA
	MapRet = MapUserPhysicalPages(MemAlloc2, NumPages, PfnArrayMDL);
	if (MapRet == FALSE)
	{
		printf("mapping 2 failed\n");
		getchar();
		return 0;
	}

	printf("Read Memory from remapped VA\n");
	// Read the Memory from the newly mapped VA
	char* buf = NULL;
	buf = (char*)malloc(10);
	SIZE_T szRead;
	BOOL ret = ReadProcessMemory(GetCurrentProcess(), (LPVOID)MemAlloc2, (LPVOID)buf, 10, &szRead);
	if (!ret)
	{
		printf("Failed to RPM\nGLE= %X\n", GetLastError());
		getchar();
		return 0;
	}

	printf("Read memory %.*s\n", 10, buf);
	printf("fin\n");
	getchar();
    return 0;
}

