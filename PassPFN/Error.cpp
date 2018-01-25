#include "stdafx.h"

void PrintError(char *szMsg, DWORD ddMsgId)
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