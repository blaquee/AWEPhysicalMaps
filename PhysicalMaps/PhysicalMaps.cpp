// PhysicalMaps.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


// 2MB
#define MEM_REQUESTED (1024 * 1024)


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
	LPVOID memAlloc;
	BOOL mRet;
	BOOL vpRet;
	ULONG_PTR nNumPages;
	ULONG_PTR nModNumPages;
	ULONG_PTR nNumPagesInitial;
	ULONG_PTR *aPFN;
	SYSTEM_INFO sysInfo;
	int pfnArraySize;
	// Get PageSize information from system

	IsSystemResumeAutomatic();

	GetSystemInfo(&sysInfo);
	printf("Page Size = %d\n", sysInfo.dwPageSize);

	nNumPages = MEM_REQUESTED / sysInfo.dwPageSize;
	printf("Number of pages being requested = %d\n", nNumPages);

	nModNumPages = (MEM_REQUESTED / sysInfo.dwPageSize) - sysInfo.dwPageSize + 1;
	printf("Bad Number of Pages requested = %d\nPress Key..\n", nModNumPages);

	getchar();

	pfnArraySize = nNumPages * sizeof(ULONG_PTR);
	printf("pfnArraySize = %d\n", pfnArraySize);

	aPFN = (ULONG_PTR*)HeapAlloc(GetProcessHeap(), 0, pfnArraySize);
	if (aPFN == NULL)
	{
		printf("Failed to allocate heap\n");
		return 0;
	}

	if (!LoggedSetLockPagesPrivilege(GetCurrentProcess(), TRUE))
	{
		printf("Priv failed\n");
		return 0;
	}

	//Allocate the physical pages
	nNumPagesInitial = nNumPages;
	BOOL bRes = AllocateUserPhysicalPages(GetCurrentProcess(), &nNumPages, aPFN);
	if (nNumPagesInitial != nNumPages)
	{
		printf("Mismatch allocation of page sizes. Allocated = %d\n", nNumPages);
		return 0;
	}

	printf("Page Frame Numbers:\n\t");
	for (int i = 0; i < nNumPages; i++)
	{
		printf("Addr = %X\n", aPFN[i]);
	}
	//allocate some memory with valloc
	memAlloc = VirtualAlloc(NULL, MEM_REQUESTED, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (memAlloc == NULL)
	{
		printf("Failed Mem Alloc\n");
		return 0;
	}
	printf("Allocation Addr = %llx\n", memAlloc);

	mRet = MapUserPhysicalPages(memAlloc, nNumPages, aPFN);
	if (mRet == FALSE)
	{
		printf("Unmapping failed\n");
		getchar();
		return 0;
	}

	getchar();
	printf("Writing to memory\n");
	//Write to Mem
	CHAR buffer = 0x41;
	SIZE_T bytesWritten;
	BOOL bWritten;

	bWritten = WriteProcessMemory(GetCurrentProcess(), memAlloc, (LPCVOID)&buffer, sizeof(buffer), &bytesWritten);
	if (!bWritten || (bytesWritten != sizeof(buffer)))
	{
		printf("Failed to Write to Memory. GLE = %X\n", GetLastError());
		getchar();
		return 0;
	}

	printf("Attempting to VirtualProtect +X\n");
	vpRet = VirtualProtect(memAlloc, MEM_REQUESTED, PAGE_EXECUTE, NULL);
	if (vpRet == FALSE)
	{
		printf("Failed to VP: GLE=%X\n", GetLastError());
	}
	printf("fin\n");
	getchar();
    return 0;
}

