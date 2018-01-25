#pragma once


#include "stdafx.h"

#define GLOBALMEM TEXT("Global\PFNData")

typedef struct
{
	PVOID TargetMem; // Address the data was written into
	SIZE_T LenData; // Length of the data
}PFNData;
