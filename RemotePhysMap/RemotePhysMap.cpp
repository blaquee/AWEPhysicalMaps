// RemotePhysMap.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Defines.h"

#define MEM_REQUESTED 1024 * 1024
#define PROCNAME L"C:\\Windows\\System32\\notepad.exe"
const char DLLNAME[] = "PassPFN.dll";

void PrintError(const char *szMsg, DWORD ddMsgId)
{
	char *szError;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ddMsgId,
		0,
		(LPSTR)&szError,
		0,
		NULL
	);
	dprintf("[E] %s: %s", szMsg, szError);
	LocalFree(szError);
	return;
}

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

BOOL InjectDLL(HANDLE hProc, const char* DllName)
{
	LPVOID RemoteLibString, LoadLibAddress;
	char DllPath[260];

	//Prepare our Dll Path to write
	GetCurrentDirectoryA(_countof(DllPath), DllPath);
	PathAppendA(DllPath, DllName);
	printf("Dll Path: %s\n", DllPath);

	LoadLibAddress = (LPVOID)GetProcAddress(GetModuleHandle(TEXT("Kernel32.dll")), "LoadLibraryA");
	
	//Allocate room for the string
	RemoteLibString = VirtualAllocEx(hProc, NULL, strlen(DllPath), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!RemoteLibString)
	{
		printf("Failed to allocate remote lib string\n");
		return FALSE;
	}
	
	// Write the string in memory
	if (!WriteProcessMemory(hProc, RemoteLibString, DllPath, strlen(DllPath), NULL))
	{
		PrintError("WPm Failed", GetLastError());
		return FALSE;
	}
	// Call the thread to load the remote lib
	CreateRemoteThread(hProc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddress, RemoteLibString,
		NULL, NULL);

	ResumeThread(hProc);
	printf("Remote Thread Executed\n");
	return TRUE;
}

BOOL MapSharedObject(PVOID Object)
{
	HANDLE MapFile;
	PFNData* data = NULL;

	MapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
		0, sizeof(PFNData), L"Local\\PFNDataMem");
	if (MapFile == NULL)
	{
		printf("Creating File Map failed\n");
		return FALSE;
	}

	// Map the ivew into our process to write to
	data = (PFNData*)MapViewOfFile(MapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PFNData));
	if (data == NULL)
	{
		printf("Failed to map view of file\n");
		CloseHandle(MapFile);
		return FALSE;
	}
	// Copy the data
	RtlCopyMemory(data, Object, sizeof(PFNData));

	UnmapViewOfFile(data);
	//CloseHandle(MapFile);

	return TRUE;
}

void Privilege(const char* pszPrivilege, BOOL bEnable)
{
	HANDLE           hToken;
	TOKEN_PRIVILEGES tp;
	BOOL             status;
	DWORD            error;

	// open process token
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		PrintError("OpenProcessToken", GetLastError());

	// get the luid
	if (!LookupPrivilegeValueA(NULL, pszPrivilege, &tp.Privileges[0].Luid))
		PrintError("LookupPrivilegeValue", GetLastError());

	tp.PrivilegeCount = 1;

	// enable or disable privilege
	if (bEnable)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	// enable or disable privilege
	status = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

	// It is possible for AdjustTokenPrivileges to return TRUE and still not succeed.
	// So always check for the last error value.
	error = GetLastError();
	if (!status || (error != ERROR_SUCCESS))
		PrintError("AdjustTokenPrivileges", GetLastError());

	// close the handle
	if (!CloseHandle(hToken))
		PrintError("CloseHandle", GetLastError());
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
	ULONG_PTR NumPages, NumPagesInitial = 0;
	ULONG_PTR* PfnArray;
	HANDLE NotePadHandle = NULL;
	BOOL bAllocRes = FALSE;
	BOOL MapRet = FALSE;
	BOOL WpmRet = FALSE;
	BOOL bCreated = FALSE;
	int PfnArraySize;
	PFNData pfnData = { 0 };
	LPVOID MemAllocEx = NULL;
	LPVOID MemAllocOurs = NULL;

	//static buffers
	char buffer[10] = { 'A' };

	STARTUPINFO SInfo = { sizeof(SInfo) };
	PROCESS_INFORMATION PInfo;

	// Need this privilege to do anything with AWE
	printf("Setting Privilege\n");
	if (!LoggedSetLockPagesPrivilege(GetCurrentProcess(), TRUE))
	{
		printf("Failed to set Priv for own process");
		getchar();
		return 0;
	}
	// Privilege("SeCreateGlobalPrivilege", TRUE);


	printf("Starting child process\n");

	// Calculate how many pages we want to allocate (MemSize/PageSize)
	GetSystemInfo(&SysInfo);
	printf("Page Size = %d\n", SysInfo.dwPageSize);

	NumPages = MEM_REQUESTED / SysInfo.dwPageSize;
	printf("Number of pages being requested = %d\n", NumPages);

	PfnArraySize = NumPages * sizeof(ULONG_PTR);
	printf("pfnArraySize = %d\n", PfnArraySize);

	//Allocate space for the PFN Array
	PfnArray = (ULONG_PTR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, PfnArraySize);
	if (!PfnArray)
	{
		printf("Couldn't allocate heap for PfnArray\n");
		CloseHandle(NotePadHandle);
		goto failed;
	}

	// Allocate the physical pages for this process
	bAllocRes = AllocateUserPhysicalPages(GetCurrentProcess(), &NumPages, PfnArray);
	if (!bAllocRes)
	{
		printf("AllocateUser PAges failed. GLE = %X\n", GetLastError());
		getchar();
		goto failed;
	}

	// Allocate memory for our own data
	MemAllocOurs = VirtualAlloc(NULL, MEM_REQUESTED, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (MemAllocOurs == NULL)
	{
		printf("Failed Mem Alloc\n");
		goto failed;
	}
	printf("Allocation Address for our VA: %X\nKey to Cont..\n", MemAllocOurs);
	getchar();

	// Map our VA to the physical pages
	MapRet = MapUserPhysicalPages(MemAllocOurs, NumPages, PfnArray);
	if (MapRet == FALSE)
	{
		PrintError("Map Physical Page Failed", GetLastError());
		getchar();
		goto failed;
	}

	// Write some data to that memory location
	SIZE_T written;
	WpmRet = WriteProcessMemory(GetCurrentProcess(), MemAllocOurs, &buffer, 10, &written);
	if (!WpmRet)
	{
		printf("Failed o WPM!\n");
		getchar();
		return 0;
	}

	printf("Wrote Memory...\n");
	getchar();

	// Unmap the VA from the Physical Pages (this reverts the VA back to Reserved memory)
	MapRet = MapUserPhysicalPages(MemAllocOurs, NumPages, NULL);
	if (MapRet == FALSE)
	{
		printf("mapping to unmap failed\n");
		getchar();
		return 0;
	}

	// Create notepad process
	bCreated = CreateProcess(PROCNAME, NULL, NULL, NULL, TRUE, CREATE_SUSPENDED,
		NULL, NULL, &SInfo, &PInfo);
	if (!bCreated)
	{
		printf("Failed to create child process\n");
		getchar();
		return 0;
	}
	// Save Handle
	NotePadHandle = PInfo.hProcess;

	// Allocate remote memory to write the PfnArray Data to
	MemAllocEx = VirtualAllocEx(NotePadHandle, NULL, PfnArraySize,
		MEM_COMMIT, PAGE_READWRITE);
	if (!MemAllocEx)
	{
		printf("Failed to alloc\n");
		getchar();
		goto failed;
	}

	printf("Remote Allocation Addr: %X\n", MemAllocEx);
	getchar();

	printf("Creating the Shared Memory to write the struct to\n");
	//Duplicate our handle
	DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), 
		NotePadHandle, &pfnData.InheritedHandle, 0, TRUE, DUPLICATE_SAME_ACCESS);

	// Prepare the structure to write to remote process
	pfnData.TargetMem = MemAllocEx;
	pfnData.LenData = PfnArraySize;

	if (!MapSharedObject(&pfnData))
	{
		printf("Failed to Map Remote Object\n");
		goto failed;
	}

	printf("Mapped Object Written..Ready To Inject\n");
	getchar();

	//Now inject the remote library that will remap the previously written data
	if (!InjectDLL(NotePadHandle, DLLNAME))
	{
		printf("Opps, something went wrong with injecting the dll\n");
		goto failed;
	}

	// resume the process
	ResumeThread(PInfo.hThread);

	printf("finito\n");
	getchar();


failed:
	if (PfnArray)
		HeapFree(GetProcessHeap(), 0, PfnArray);
	if (MemAllocOurs)
		VirtualFree(MemAllocOurs, MEM_REQUESTED, MEM_RELEASE);
	if (MemAllocEx)
		VirtualFreeEx(NotePadHandle, MemAllocEx, PfnArraySize, MEM_RELEASE);
	if (NotePadHandle)
		CloseHandle(NotePadHandle);

	getchar();
    return 0;
}

