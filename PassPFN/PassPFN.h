// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the PASSPFN_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// PASSPFN_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef PASSPFN_EXPORTS
#define PASSPFN_API __declspec(dllexport)
#else
#define PASSPFN_API __declspec(dllimport)
#endif

#define GLOBALMEM TEXT("Global\PFNData")

typedef struct
{
	PVOID TargetMem; // Address the data was written into
	SIZE_T LenData; // Length of the data
}PFNData;


PASSPFN_API BOOL fnPassPFN(ULONG_PTR*, SIZE_T);
