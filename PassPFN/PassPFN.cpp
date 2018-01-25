// PassPFN.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "PassPFN.h"


// Get the information from the shared Memory and Remap that shiz
BOOL GetMappedDataAndReMap()
{
	HANDLE MapFile;
	ULONG_PTR* PfnBuffer;
	SIZE_T PfnBufferLen;
	PFNData *PfnDataMem = NULL;
	ULONG_PTR NumPages;
	PVOID RemappedData;

	MapFile = OpenFileMapping(FILE_MAP_READ,
		FALSE,
		GLOBALMEM);
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

	// Need to write here
	if (!VirtualProtect(PfnBuffer, PfnBufferLen, PAGE_READWRITE, NULL))
	{
		printf("Can't Write to new Buffer\n");
		UnmapViewOfFile(PfnDataMem);
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
		return FALSE;
	}

	// Copy the data (PFN Array)
	RtlCopyMemory(PfnBuffer, PfnLocation, PfnBufferLen);

	//Calculate the NumPages needed to NtMapUserPhysicalPages. Its just PfnBufferLen/sizeof(ULONG_PTR)
	NumPages = PfnBufferLen / sizeof(ULONG_PTR);

	// Let's try to remap the memory. First lets allocate a Virtual Address for the remapped data
	RemappedData = VirtualAlloc(NULL, 0x1000, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
	if (RemappedData == NULL)
	{
		printf("VirtualAlloc failed for remapped VA\n");
		UnmapViewOfFile(PfnDataMem);
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
	}
	//Remap the data from the remote process
	BOOL MapRet = MapUserPhysicalPages(RemappedData, NumPages, PfnBuffer);
	if (!MapRet)
	{
		PrintError("Remapping Failed:", GetLastError());
		UnmapViewOfFile(PfnDataMem);
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
		return FALSE;
	}

	if (PfnDataMem)
		UnmapViewOfFile(PfnDataMem);
	if (PfnBuffer)
		HeapFree(GetProcessHeap(), 0, PfnBuffer);
	if (RemappedData)
		VirtualFree(RemappedData, 0x1000, MEM_RELEASE);

	printf("Done..\n");
	return TRUE;

}

