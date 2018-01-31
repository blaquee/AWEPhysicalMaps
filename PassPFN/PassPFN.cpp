// PassPFN.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "PassPFN.h"

TCHAR GlobalMapName[] = TEXT("Local\\PFNDataMem");

// Get the information from the shared Memory and Remap that shiz
BOOL GetMappedDataAndReMap()
{
	HANDLE MapFile;
	ULONG_PTR* PfnBuffer;
	SIZE_T PfnBufferLen;
	PFNData *PfnDataMem = NULL;
	ULONG_PTR NumPages;
	PVOID RemappedData;
	SIZE_T ReadDataLen;
	BOOL MapRet = FALSE;
	BOOL AllocRes = FALSE;

	//static buffer for RPM
	char buffer[10] = { 0 };
	DebugBreak();

	MapFile = OpenFileMapping(FILE_MAP_READ,
		TRUE,
		L"Local\\PFNDataMem");
	if (MapFile == NULL)
	{
		printf("Mapping Object not found!\n");
		return FALSE;
	}

	PfnDataMem = (PFNData*)MapViewOfFile(MapFile,
		FILE_MAP_READ,
		0,
		0,
		sizeof(PFNData));
	if (PfnDataMem == NULL)
	{
		printf("No data in mapped region\n");
		return FALSE;
	}

	// We've got our struct, lets find the PfnArray
	PVOID PfnLocation = PfnDataMem->TargetMem;
	PfnBufferLen = PfnDataMem->LenData;
	// naive check for size
	if (PfnBufferLen == 0)
	{
		printf("Bad size for PfnBuffer\n");
		UnmapViewOfFile(PfnDataMem);
		return FALSE;
	}
	
	// Make room for that buffer ( to hold the PFN Array)
	PfnBuffer = (ULONG_PTR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, PfnBufferLen);
	if (PfnBuffer == NULL)
	{
		printf("Could not allocate heap memory\n");
		UnmapViewOfFile(PfnDataMem);
		return FALSE;
	}

	//Create a dummy AWEInfo struct for this process
	ULONG_PTR PfnFakeBuffer[256];
	ULONG_PTR PfnFakeNumPages = 256;
	AllocRes = AllocateUserPhysicalPages(GetCurrentProcess(), &PfnFakeNumPages, PfnFakeBuffer);
	if (!AllocRes)
	{
		printf("Could allocate AWE\n");
		UnmapViewOfFile(PfnDataMem);
		return FALSE;
	}

	// Copy the data (PFN Array)
	RtlCopyMemory(PfnBuffer, PfnLocation, PfnBufferLen);

	//Calculate the NumPages needed to NtMapUserPhysicalPages. Its just PfnBufferLen/sizeof(ULONG_PTR)
	NumPages = PfnBufferLen / sizeof(ULONG_PTR);

	// Let's try to remap the memory. First lets allocate a Virtual Address for the remapped data
	RemappedData = VirtualAlloc(NULL, MEM_REQUESTED, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (RemappedData == NULL)
	{
		printf("VirtualAlloc failed for remapped VA\n");
		UnmapViewOfFile(PfnDataMem);
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
	}

	//Remap the data from the remote process
	MapRet = MapUserPhysicalPages(RemappedData, NumPages, PfnBuffer);
	if (!MapRet)
	{
		PrintError("Remapping Failed:", GetLastError());
		UnmapViewOfFile(PfnDataMem);
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
		return FALSE;
	}

	if (!ReadProcessMemory(GetCurrentProcess(), RemappedData, buffer, 10, &ReadDataLen))
	{
		PrintError("RPM Failed", GetLastError());
		UnmapViewOfFile(PfnDataMem);
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
		return FALSE;
	}

	printf("Printing read memory...\n");
	printf("Data: %.*s\n", 10, buffer);

	if (PfnDataMem)
		UnmapViewOfFile(PfnDataMem);
	if (PfnBuffer)
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
	if (RemappedData)
		VirtualFree(RemappedData, MEM_REQUESTED, MEM_RELEASE);

	printf("Done..\n");
	return TRUE;

}

