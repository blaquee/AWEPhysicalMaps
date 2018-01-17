// RemotePhysMap.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


#define MEM_REQUESTED 1024 * 1024
#define PROCNAME L"C:\\Windows\\System32\\notepad.exe"

BOOL LoggedSetLockPagesPrivilege(HANDLE hProcess, BOOL bEnable)
{
	struct 
	{
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

DWORD GetRemoteProcessPID(const wchar_t* ProcName)
{
	PROCESSENTRY32 Entry;
	DWORD Pid = 0;
	Entry.dwSize = sizeof(PROCESSENTRY32);
	ZeroMemory(&Entry, sizeof(PROCESSENTRY32));

	HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(Snapshot, &Entry))
	{
		do
		{
			if (_tcscmp(ProcName, L"notepad.exe") == 0)
			{
				Pid = Entry.th32ProcessID;
				break;
			}
		} while (Process32Next(Snapshot, &Entry));
	}
	CloseHandle(Snapshot);
	return Pid;
}

int main()
{	
	SYSTEM_INFO SysInfo;
	ULONG_PTR NumPages, NumPagesInitial;
	ULONG_PTR* PfnArray = NULL;
	DWORD NotePadPid;
	HANDLE NotePadHandle;
	int PfnArraySize;

	LPVOID MemAllocEx = NULL;

	STARTUPINFO SInfo = { sizeof(SInfo) };
	PROCESS_INFORMATION PInfo;


	printf("Setting Privilege\n");
	if (!LoggedSetLockPagesPrivilege(GetCurrentProcess(), TRUE))
	{
		printf("Failed to set Priv for own process");
		getchar();
		return 0;
	}

	GetSystemInfo(&SysInfo);
	printf("Page Size = %d\n", SysInfo.dwPageSize);


	// Create notepad process
	BOOL bCreated = CreateProcess(L"C:\\Windows\\System32\\notepad.exe", NULL, NULL, NULL, NULL, CREATE_SUSPENDED,
		NULL, NULL, &SInfo, &PInfo);
	if (!bCreated)
	{
		printf("Failed to create child process\n");
		getchar();
		return 0;
	}
	// Save Handle
	NotePadHandle = PInfo.hProcess;


	NumPages = MEM_REQUESTED / SysInfo.dwPageSize;
	printf("Number of pages being requested = %d\n", NumPages);

	PfnArraySize = NumPages * sizeof(ULONG_PTR);
	printf("pfnArraySize = %d\n", PfnArraySize);

	/*
	//Get notepad PID and ProcHandle
	NotePadPid = GetRemoteProcessPID(L"notepad.exe");
	
	// Open the Process Handle
	NotePadHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, NotePadPid);
	*/

	//Allocate space for the PFN Array
	PfnArray = (ULONG_PTR*)HeapAlloc(GetProcessHeap(), 0, PfnArraySize);
	if (!PfnArray)
	{
		printf("Couldn't allocate heap for PfnArray\n");
		CloseHandle(NotePadHandle);
		return 0;
	}
	
	/*
	//Allocate Pfn Array to remote process
	PfnArray = (ULONG_PTR*)VirtualAllocEx(NotePadHandle, NULL, PfnArraySize, MEM_COMMIT, PAGE_READWRITE);
	if (!PfnArray)
	{
		printf("No VirtualAllocEx\n");
		CloseHandle(NotePadHandle);
		getchar();
		return 0;
	}
	*/

	NumPagesInitial = NumPages;
	BOOL bRes = AllocateUserPhysicalPages(NotePadHandle, &NumPages, PfnArray);
	if (!bRes || (NumPagesInitial != NumPages))
	{
		printf("AllocateUser PAges failed. GLE = %X\n", GetLastError());
		getchar();
		return 0;
	}

	MemAllocEx = VirtualAllocEx(NotePadHandle, NULL, MEM_REQUESTED,
		MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (!MemAllocEx)
	{
		printf("Failed to alloc\n");
		getchar();
		return 0;
	}

	printf("Allocation Addr: %X\n", MemAllocEx);
	getchar();

	printf("Mapping Phsy Mem\n");
	// Need to make this shellcode to inject into child proc.
	BOOL MapRet = MapUserPhysicalPages(MemAllocEx, NumPages, PfnArray);
	if (!MapRet)
	{
		printf("Map failed, duh\n");
		getchar();
		return 0;
	}

	//reeUserPhysicalPages(NotePadHandle,)
	getchar();
    return 0;
}

