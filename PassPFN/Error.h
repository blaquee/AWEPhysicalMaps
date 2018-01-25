
#include <Windows.h>

#ifdef _DEBUG 
#define dprintf printf
#else
#define dprintf nop_printf
static int nop_printf(char *szFormat, ...) { return(0); }
#endif

/*
PrintError is a function to pretty print Windows errors.
Args:
szMsg - a message to include with the windows error description
ddMsgId - an error value as returned by GetLastError
Return:
None
*/
void PrintError(char *szMsg, DWORD ddMsgId);