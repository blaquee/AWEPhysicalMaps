#pragma once


#include "stdafx.h"


#ifdef _DEBUG 
#define dprintf printf
#else
#define dprintf nop_printf
static int nop_printf(char *szFormat, ...) { return(0); }
#endif

TCHAR GlobalMapName[] = TEXT("Local\\PFNDataMem");

#pragma pack(1)
typedef struct PFNData
{
	PVOID TargetMem; // Address the data was written into
	SIZE_T LenData; // Length of the data
	HANDLE InheritedHandle;
}PFNData;
