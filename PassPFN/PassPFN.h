#ifdef PASSPFN_EXPORTS
#define PASSPFN_API __declspec(dllexport)
#else
#define PASSPFN_API __declspec(dllimport)
#endif


#define MEM_REQUESTED 1024 * 1024

#pragma pack(1)
typedef struct
{
	PVOID TargetMem; // Address the data was written into
	SIZE_T LenData; // Length of the data
	HANDLE InheritedHandle;
}PFNData;

BOOL GetMappedDataAndReMap();
