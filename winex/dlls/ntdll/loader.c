#include "winbase.h"
#include "winternl.h"
#include "winnt.h"

NTSTATUS WINAPI LdrDisableThreadCalloutsForDll(HANDLE hModule)
{
    if (DisableThreadLibraryCalls(hModule))
	return STATUS_SUCCESS;
    else
	return STATUS_DLL_NOT_FOUND;
}
