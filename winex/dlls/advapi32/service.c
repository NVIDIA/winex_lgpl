/*
 * Win32 advapi functions
 *
 * Copyright 1995 Sven Verdoolaege
 */

#include <string.h>
#include <time.h>

#include "windef.h"
#include "winsvc.h"
#include "winerror.h"
#include "winreg.h"
#include "wine/unicode.h"
#include "heap.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(advapi);

static DWORD   start_dwNumServiceArgs;
static LPWSTR *start_lpServiceArgVectors;

/******************************************************************************
 * EnumServicesStatusA [ADVAPI32.@]
 */
BOOL WINAPI
EnumServicesStatusA( SC_HANDLE hSCManager, DWORD dwServiceType,
                     DWORD dwServiceState, LPENUM_SERVICE_STATUSA lpServices,
                     DWORD cbBufSize, LPDWORD pcbBytesNeeded,
                     LPDWORD lpServicesReturned, LPDWORD lpResumeHandle )
{	FIXME("%x type=%x state=%x %p %x %p %p %p\n", hSCManager,
		dwServiceType, dwServiceState, lpServices, cbBufSize,
		pcbBytesNeeded, lpServicesReturned,  lpResumeHandle);
	SetLastError (ERROR_ACCESS_DENIED);
	return FALSE;
}

/******************************************************************************
 * EnumServicesStatusW [ADVAPI32.@]
 */
BOOL WINAPI
EnumServicesStatusW( SC_HANDLE hSCManager, DWORD dwServiceType,
                     DWORD dwServiceState, LPENUM_SERVICE_STATUSW lpServices,
                     DWORD cbBufSize, LPDWORD pcbBytesNeeded,
                     LPDWORD lpServicesReturned, LPDWORD lpResumeHandle )
{	FIXME("%x type=%x state=%x %p %x %p %p %p\n", hSCManager,
		dwServiceType, dwServiceState, lpServices, cbBufSize,
		pcbBytesNeeded, lpServicesReturned,  lpResumeHandle);
	SetLastError (ERROR_ACCESS_DENIED);
	return FALSE;
}

/******************************************************************************
 * StartServiceCtrlDispatcherA [ADVAPI32.@]
 */
BOOL WINAPI
StartServiceCtrlDispatcherA( LPSERVICE_TABLE_ENTRYA servent )
{
    LPSERVICE_MAIN_FUNCTIONA fpMain;
    HANDLE wait;
    DWORD  dwNumServiceArgs ;
    LPWSTR *lpArgVecW;
    LPSTR  *lpArgVecA;
    int i;

    TRACE("(%p)\n", servent);
    wait = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "ADVAPI32_ServiceStartData");
    if(wait == 0)
    {
        ERR("Couldn't find wait semaphore\n");
        ERR("perhaps you need to start services using StartService\n");
        return FALSE;
    }

    dwNumServiceArgs = start_dwNumServiceArgs;
    lpArgVecW        = start_lpServiceArgVectors;

    ReleaseSemaphore(wait, 1, NULL);

    /* Convert the Unicode arg vectors back to ASCII */
    if(dwNumServiceArgs)
        lpArgVecA = (LPSTR*) HeapAlloc( GetProcessHeap(), 0,
                                   dwNumServiceArgs*sizeof(LPSTR) );
    else
        lpArgVecA = NULL;

    for(i=0; i<dwNumServiceArgs; i++)
        lpArgVecA[i]=HEAP_strdupWtoA(GetProcessHeap(), 0, lpArgVecW[i]);

    /* FIXME: should we blindly start all services? */
    while (servent->lpServiceName) {
        TRACE("%s at %p)\n", debugstr_a(servent->lpServiceName),servent);
        fpMain = servent->lpServiceProc;

        /* try to start the service */
        fpMain( dwNumServiceArgs, lpArgVecA);

        servent++;
    }

    if(dwNumServiceArgs)
    {
        /* free arg strings */
        for(i=0; i<dwNumServiceArgs; i++)
            HeapFree(GetProcessHeap(), 0, lpArgVecA[i]);
        HeapFree(GetProcessHeap(), 0, lpArgVecA);
    }

    return TRUE;
}

/******************************************************************************
 * StartServiceCtrlDispatcherW [ADVAPI32.@]
 *
 * PARAMS
 *   servent []
 */
BOOL WINAPI
StartServiceCtrlDispatcherW( LPSERVICE_TABLE_ENTRYW servent )
{
    LPSERVICE_MAIN_FUNCTIONW fpMain;
    HANDLE wait;
    DWORD  dwNumServiceArgs ;
    LPWSTR *lpServiceArgVectors ;

    TRACE("(%p)\n", servent);
    wait = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "ADVAPI32_ServiceStartData");
    if(wait == 0)
    {
        ERR("Couldn't find wait semaphore\n");
        ERR("perhaps you need to start services using StartService\n");
        return FALSE;
    }

    dwNumServiceArgs    = start_dwNumServiceArgs;
    lpServiceArgVectors = start_lpServiceArgVectors;

    ReleaseSemaphore(wait, 1, NULL);

    /* FIXME: should we blindly start all services? */
    while (servent->lpServiceName) {
        TRACE("%s at %p)\n", debugstr_w(servent->lpServiceName),servent);
        fpMain = servent->lpServiceProc;

        /* try to start the service */
        fpMain( dwNumServiceArgs, lpServiceArgVectors);

        servent++;
    }

    return TRUE;
}

/******************************************************************************
 * RegisterServiceCtrlHandlerA [ADVAPI32.@]
 */
SERVICE_STATUS_HANDLE WINAPI
RegisterServiceCtrlHandlerA( LPCSTR lpServiceName,
                             LPHANDLER_FUNCTION lpfHandler )
{	FIXME("%s %p\n", lpServiceName, lpfHandler);
	return 0xcacacafe;
}

/******************************************************************************
 * RegisterServiceCtrlHandlerW [ADVAPI32.@]
 *
 * PARAMS
 *   lpServiceName []
 *   lpfHandler    []
 */
SERVICE_STATUS_HANDLE WINAPI
RegisterServiceCtrlHandlerW( LPCWSTR lpServiceName,
                             LPHANDLER_FUNCTION lpfHandler )
{	FIXME("%s %p\n", debugstr_w(lpServiceName), lpfHandler);
	return 0xcacacafe;
}

/******************************************************************************
 * SetServiceStatus [ADVAPI32.@]
 *
 * PARAMS
 *   hService []
 *   lpStatus []
 */
BOOL WINAPI
SetServiceStatus( SERVICE_STATUS_HANDLE hService, LPSERVICE_STATUS lpStatus )
{	FIXME("0x%x %p\n",hService, lpStatus);
	TRACE("\tType:%x\n",lpStatus->dwServiceType);
	TRACE("\tState:%x\n",lpStatus->dwCurrentState);
	TRACE("\tControlAccepted:%x\n",lpStatus->dwControlsAccepted);
	TRACE("\tExitCode:%x\n",lpStatus->dwWin32ExitCode);
	TRACE("\tServiceExitCode:%x\n",lpStatus->dwServiceSpecificExitCode);
	TRACE("\tCheckPoint:%x\n",lpStatus->dwCheckPoint);
	TRACE("\tWaitHint:%x\n",lpStatus->dwWaitHint);
	return TRUE;
}

/******************************************************************************
 * OpenSCManagerA [ADVAPI32.@]
 */
SC_HANDLE WINAPI
OpenSCManagerA( LPCSTR lpMachineName, LPCSTR lpDatabaseName,
                  DWORD dwDesiredAccess )
{
    LPWSTR lpMachineNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpMachineName);
    LPWSTR lpDatabaseNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpDatabaseName);
    SC_HANDLE ret = OpenSCManagerW(lpMachineNameW,lpDatabaseNameW,
                                 dwDesiredAccess);
    HeapFree(GetProcessHeap(),0,lpDatabaseNameW);
    HeapFree(GetProcessHeap(),0,lpMachineNameW);
    return ret;
}

/******************************************************************************
 * OpenSCManagerW [ADVAPI32.@]
 * Establishes a connection to the service control manager and opens database
 *
 * NOTES
 *   This should return a SC_HANDLE
 *
 * PARAMS
 *   lpMachineName   [I] Pointer to machine name string
 *   lpDatabaseName  [I] Pointer to database name string
 *   dwDesiredAccess [I] Type of access
 *
 * RETURNS
 *   Success: Handle to service control manager database
 *   Failure: NULL
 */
SC_HANDLE WINAPI
OpenSCManagerW( LPCWSTR lpMachineName, LPCWSTR lpDatabaseName,
                  DWORD dwDesiredAccess )
{
    HKEY hKey;
    LONG r;

    TRACE("(%s,%s,0x%08x)\n", debugstr_w(lpMachineName),
          debugstr_w(lpDatabaseName), dwDesiredAccess);

    /*
     * FIXME: what is lpDatabaseName?
     * It should be set to "SERVICES_ACTIVE_DATABASE" according to
     * docs, but what if it isn't?
     */

    r = RegConnectRegistryW(lpMachineName,HKEY_LOCAL_MACHINE,&hKey);
    if (r!=ERROR_SUCCESS)
        return 0;

    TRACE("returning %x\n",hKey);

    return hKey;
}


/******************************************************************************
 * AllocateLocallyUniqueId [ADVAPI32.@]
 *
 * PARAMS
 *   lpluid []
 */
BOOL WINAPI
AllocateLocallyUniqueId( PLUID lpluid )
{
	lpluid->LowPart = time(NULL);
	lpluid->HighPart = 0;
	return TRUE;
}


/******************************************************************************
 * ControlService [ADVAPI32.@]
 * Sends a control code to a Win32-based service.
 *
 * PARAMS
 *   hService        []
 *   dwControl       []
 *   lpServiceStatus []
 *
 * RETURNS STD
 */
BOOL WINAPI
ControlService( SC_HANDLE hService, DWORD dwControl,
                LPSERVICE_STATUS lpServiceStatus )
{
    FIXME("(%d,%u,%p): stub\n",hService,dwControl,lpServiceStatus);
    return TRUE;
}


/******************************************************************************
 * CloseServiceHandle [ADVAPI32.@]
 * Close handle to service or service control manager
 *
 * PARAMS
 *   hSCObject [I] Handle to service or service control manager database
 *
 * RETURNS STD
 */
BOOL WINAPI
CloseServiceHandle( SC_HANDLE hSCObject )
{
    TRACE("(%x)\n", hSCObject);

    RegCloseKey(hSCObject);

    return TRUE;
}


/******************************************************************************
 * OpenServiceA [ADVAPI32.@]
 */
SC_HANDLE WINAPI
OpenServiceA( SC_HANDLE hSCManager, LPCSTR lpServiceName,
                DWORD dwDesiredAccess )
{
    LPWSTR lpServiceNameW = HEAP_strdupAtoW(GetProcessHeap(),0,lpServiceName);
    SC_HANDLE ret;

    if(lpServiceName)
        TRACE("Request for service %s\n",lpServiceName);
    else
        return FALSE;
    ret = OpenServiceW( hSCManager, lpServiceNameW, dwDesiredAccess);
    HeapFree(GetProcessHeap(),0,lpServiceNameW);
    return ret;
}


/******************************************************************************
 * OpenServiceW [ADVAPI32.@]
 * Opens a handle to an existing service
 *
 * PARAMS
 *   hSCManager      []
 *   lpServiceName   []
 *   dwDesiredAccess []
 *
 * RETURNS
 *    Success: Handle to the service
 *    Failure: NULL
 */
SC_HANDLE WINAPI
OpenServiceW(SC_HANDLE hSCManager, LPCWSTR lpServiceName,
               DWORD dwDesiredAccess)
{
    const char *str = "System\\CurrentControlSet\\Services\\";
    WCHAR lpServiceKey[80]; /* FIXME: this should be dynamically allocated */
    HKEY hKey;
    long r;

    TRACE("(%d,%p,%u)\n",hSCManager, lpServiceName,
          dwDesiredAccess);

    MultiByteToWideChar( CP_ACP, 0, str, -1, lpServiceKey, sizeof(lpServiceKey)/sizeof(WCHAR) );
    strcatW(lpServiceKey,lpServiceName);

    TRACE("Opening reg key %s\n", debugstr_w(lpServiceKey));

    /* FIXME: dwDesiredAccess may need some processing */
    r = RegOpenKeyExW(hSCManager, lpServiceKey, 0, dwDesiredAccess, &hKey );
    if (r!=ERROR_SUCCESS)
        return 0;

    TRACE("returning %x\n",hKey);

    return hKey;
}

/******************************************************************************
 * CreateServiceW [ADVAPI32.@]
 */
SC_HANDLE WINAPI
CreateServiceW( SC_HANDLE hSCManager, LPCWSTR lpServiceName,
                  LPCWSTR lpDisplayName, DWORD dwDesiredAccess,
                  DWORD dwServiceType, DWORD dwStartType,
                  DWORD dwErrorControl, LPCWSTR lpBinaryPathName,
                  LPCWSTR lpLoadOrderGroup, LPDWORD lpdwTagId,
                  LPCWSTR lpDependencies, LPCWSTR lpServiceStartName,
                  LPCWSTR lpPassword )
{
    FIXME("(%u,%s,%s,...)\n", hSCManager, debugstr_w(lpServiceName), debugstr_w(lpDisplayName));
    return 0;
}


/******************************************************************************
 * CreateServiceA [ADVAPI32.@]
 */
SC_HANDLE WINAPI
CreateServiceA( SC_HANDLE hSCManager, LPCSTR lpServiceName,
                  LPCSTR lpDisplayName, DWORD dwDesiredAccess,
                  DWORD dwServiceType, DWORD dwStartType,
                  DWORD dwErrorControl, LPCSTR lpBinaryPathName,
                  LPCSTR lpLoadOrderGroup, LPDWORD lpdwTagId,
                  LPCSTR lpDependencies, LPCSTR lpServiceStartName,
                  LPCSTR lpPassword )
{
    HKEY hKey;
    LONG r;
    DWORD dp;

    TRACE("(%u,%s,%s,...)\n", hSCManager, debugstr_a(lpServiceName), debugstr_a(lpDisplayName));

    r = RegCreateKeyExA(hSCManager, lpServiceName, 0, NULL,
                       REG_OPTION_NON_VOLATILE, dwDesiredAccess, NULL, &hKey, &dp);
    if (r!=ERROR_SUCCESS)
        return 0;
    if (dp != REG_CREATED_NEW_KEY)
        return 0;

    if(lpDisplayName)
    {
        r = RegSetValueExA(hKey, "DisplayName", 0, REG_SZ, (LPBYTE)lpDisplayName, strlen(lpDisplayName) );
        if (r!=ERROR_SUCCESS)
            return 0;
    }

    r = RegSetValueExA(hKey, "Type", 0, REG_DWORD, (LPVOID)&dwServiceType, sizeof (DWORD) );
    if (r!=ERROR_SUCCESS)
        return 0;

    r = RegSetValueExA(hKey, "Start", 0, REG_DWORD, (LPVOID)&dwStartType, sizeof (DWORD) );
    if (r!=ERROR_SUCCESS)
        return 0;

    r = RegSetValueExA(hKey, "ErrorControl", 0, REG_DWORD,
                           (LPVOID)&dwErrorControl, sizeof (DWORD) );
    if (r!=ERROR_SUCCESS)
        return 0;

    if(lpBinaryPathName)
    {
        r = RegSetValueExA(hKey, "ImagePath", 0, REG_SZ,
                           (LPBYTE)lpBinaryPathName,strlen(lpBinaryPathName)+1 );
        if (r!=ERROR_SUCCESS)
            return 0;
    }

    if(lpLoadOrderGroup)
    {
        r = RegSetValueExA(hKey, "Group", 0, REG_SZ,
                           (LPBYTE)lpLoadOrderGroup, strlen(lpLoadOrderGroup)+1 );
        if (r!=ERROR_SUCCESS)
            return 0;
    }

    r = RegSetValueExA(hKey, "ErrorControl", 0, REG_DWORD,
                       (LPVOID)&dwErrorControl, sizeof (DWORD) );
    if (r!=ERROR_SUCCESS)
        return 0;

    if(lpDependencies)
    {
        DWORD len = 0;

        /* determine the length of a double null terminated multi string */
        do {
            len += (strlen(&lpDependencies[len])+1);
        } while (lpDependencies[len++]);

        /* FIXME: this should be unicode */
        r = RegSetValueExA(hKey, "Dependencies", 0, REG_MULTI_SZ,
                           (LPBYTE)lpDependencies, len );
        if (r!=ERROR_SUCCESS)
            return 0;
    }

    if(lpPassword)
    {
        FIXME("Don't know how to add a Password for a service.\n");
    }

    if(lpServiceStartName)
    {
        FIXME("Don't know how to add a ServiceStartName for a service.\n");
    }

    return hKey;
}


/******************************************************************************
 * DeleteService [ADVAPI32.@]
 *
 * PARAMS
 *    hService [I] Handle to service
 *
 * RETURNS STD
 *
 */
BOOL WINAPI
DeleteService( SC_HANDLE hService )
{
    FIXME("(%d): stub\n",hService);
    return TRUE;
}


/******************************************************************************
 * StartServiceA [ADVAPI32.@]
 *
 */
BOOL WINAPI
StartServiceA( SC_HANDLE hService, DWORD dwNumServiceArgs,
                 LPCSTR *lpServiceArgVectors )
{
    LPWSTR *lpwstr=NULL;
    int i;

    TRACE("(%d,%u,%p)\n",hService,dwNumServiceArgs,lpServiceArgVectors);

    if(dwNumServiceArgs)
        lpwstr = (LPWSTR*) HeapAlloc( GetProcessHeap(), 0,
                                   dwNumServiceArgs*sizeof(LPWSTR) );
    else
        lpwstr = NULL;

    for(i=0; i<dwNumServiceArgs; i++)
        lpwstr[i]=HEAP_strdupAtoW(GetProcessHeap(), 0, lpServiceArgVectors[i]);

    StartServiceW(hService, dwNumServiceArgs, (LPCWSTR *)lpwstr);

    if(dwNumServiceArgs)
    {
        for(i=0; i<dwNumServiceArgs; i++)
            HeapFree(GetProcessHeap(), 0, lpwstr[i]);
        HeapFree(GetProcessHeap(), 0, lpwstr);
    }

    return TRUE;
}


/******************************************************************************
 * StartServiceW [ADVAPI32.@]
 * Starts a service
 *
 * PARAMS
 *   hService            [I] Handle of service
 *   dwNumServiceArgs    [I] Number of arguments
 *   lpServiceArgVectors [I] Address of array of argument string pointers
 *
 * NOTES
 *
 * NT implements this function using an obscure RPC call...
 *
 * Might need to do a "setenv SystemRoot \\WINNT" in your .cshrc
 *   to get things like %SystemRoot%\\System32\\service.exe to load.
 *
 * Will only work for shared address space. How should the service
 *  args be transferred when address spaces are separated?
 *
 * Can only start one service at a time.
 *
 * Has no concept of privilege.
 *
 * RETURNS STD
 *
 */
BOOL WINAPI
StartServiceW( SC_HANDLE hService, DWORD dwNumServiceArgs,
                 LPCWSTR *lpServiceArgVectors )
{
    CHAR path[MAX_PATH],str[MAX_PATH];
    DWORD type,size;
    long r;
    HANDLE data,wait;
    PROCESS_INFORMATION procinfo;
    STARTUPINFOA startupinfo;

    TRACE("(%d,%u,%p)\n",hService,dwNumServiceArgs,
          lpServiceArgVectors);

    size = sizeof str;
    r = RegQueryValueExA(hService, "ImagePath", NULL, &type, (LPVOID)str, &size);
    if (r!=ERROR_SUCCESS)
        return FALSE;
    ExpandEnvironmentStringsA(str,path,sizeof path);

    TRACE("Starting service %s\n", debugstr_a(path) );

    data = CreateSemaphoreA(NULL,1,1,"ADVAPI32_ServiceStartData");
    if(data == ERROR_INVALID_HANDLE)
    {
        data = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "ADVAPI32_ServiceStartData");
        if(data == 0)
        {
            ERR("Couldn't create data semaphore\n");
            return FALSE;
        }
    }
    wait = CreateSemaphoreA(NULL,0,1,"ADVAPI32_WaitServiceStart");
    {
        wait = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "ADVAPI32_ServiceStartData");
        if(wait == 0)
        {
            ERR("Couldn't create wait semaphore\n");
            return FALSE;
        }
    }

    /*
     * FIXME: lpServiceArgsVectors need to be stored and returned to
     *        the service when it calls StartServiceCtrlDispatcher
     *
     * Chuck these in a global (yuk) so we can pass them to
     * another process - address space separation will break this.
     */

    r = WaitForSingleObject(data,INFINITE);

    if( r == WAIT_FAILED)
        return FALSE;

    FIXME("problematic because of address space separation.\n");
    start_dwNumServiceArgs    = dwNumServiceArgs;
    start_lpServiceArgVectors = (LPWSTR *)lpServiceArgVectors;

    ZeroMemory(&startupinfo,sizeof(STARTUPINFOA));
    startupinfo.cb = sizeof(STARTUPINFOA);

    r = CreateProcessA(path,
                   NULL,
                   NULL,  /* process security attribs */
                   NULL,  /* thread security attribs */
                   FALSE, /* inherit handles */
                   0,     /* creation flags */
                   NULL,  /* environment */
                   NULL,  /* current directory */
                   &startupinfo,  /* startup info */
                   &procinfo); /* process info */

    if(r == FALSE)
    {
        ERR("Couldn't start process\n");
        /* ReleaseSemaphore(data, 1, NULL);
        return FALSE; */
    }

    /* docs for StartServiceCtrlDispatcher say this should be 30 sec */
    r = WaitForSingleObject(wait,30000);

    ReleaseSemaphore(data, 1, NULL);

    if( r == WAIT_FAILED)
        return FALSE;

    return TRUE;
}

/******************************************************************************
 * QueryServiceStatus [ADVAPI32.@]
 *
 * PARAMS
 *   hService        []
 *   lpservicestatus []
 *
 */
BOOL WINAPI
QueryServiceStatus( SC_HANDLE hService, LPSERVICE_STATUS lpservicestatus )
{
    SERVICE_STATUS_PROCESS servicestatus;

    TRACE("(%x,%p)\n",hService,lpservicestatus);

    if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO,
        (LPBYTE)&servicestatus, sizeof(servicestatus),
        NULL))
        return FALSE;

    memcpy(lpservicestatus, &servicestatus, sizeof(*lpservicestatus));
    return TRUE;
}

/******************************************************************************
 * QueryServiceStatusEx [ADVAPI32.@]
 */
BOOL WINAPI
QueryServiceStatusEx( SC_HANDLE hService,
  SC_STATUS_TYPE InfoLevel,
  LPBYTE lpBuffer,
  DWORD cbBufSize,
  LPDWORD pcbBytesNeeded )
{
    LONG r;
    DWORD type, val, size;
    LPSERVICE_STATUS_PROCESS lpservicestatus;

    FIXME("(%x,%x,%p,%u,%p) partial\n",hService,InfoLevel,lpBuffer,cbBufSize,pcbBytesNeeded);

    if (InfoLevel != SC_STATUS_PROCESS_INFO) {
        FIXME("unknown level %d\n", InfoLevel);
        SetLastError(ERROR_INVALID_LEVEL);
        return FALSE;
    }

    lpservicestatus = (LPSERVICE_STATUS_PROCESS)lpBuffer;
    if (cbBufSize < sizeof(*lpservicestatus)) {
        *pcbBytesNeeded = sizeof(*lpservicestatus);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    /* read the service type from the registry */
    size = sizeof val;
    r = RegQueryValueExA(hService, "Type", NULL, &type, (LPBYTE)&val, &size);
    if(type!=REG_DWORD)
    {
        ERR("invalid Type\n");
        return FALSE;
    }
    lpservicestatus->dwServiceType = val;
    /* FIXME: how are these determined or read from the registry? */
    /* SERVICE: unavailable=0, stopped=1, starting=2, running=3? */;
    lpservicestatus->dwCurrentState            = 1;
    lpservicestatus->dwControlsAccepted        = 0;
    lpservicestatus->dwWin32ExitCode           = NO_ERROR;
    lpservicestatus->dwServiceSpecificExitCode = 0;
    lpservicestatus->dwCheckPoint              = 0;
    lpservicestatus->dwWaitHint                = 0;
    lpservicestatus->dwProcessId               = 0;
    lpservicestatus->dwServiceFlags            = 0;

    return TRUE;
}

/******************************************************************************
 * LockServiceDatabase [ADVAPI32.@]
 * Keep the service control manager from starting a service.
 *
 * NOTES
 *   This should return a SC_LOCK.
 *   The caller must have SC_MANAGER_LOCK rights and the database must not
 *   already be locked.
 *   If the calling process dies before unlocking the database, it is
 *   automatically unlocked.
 *
 * PARAMS
 *   hDatabase [I] Handle to the service control database
 *
 * RETURNS
 *   Success: Handle to a lock for hDatabase
 *   Failure: NULL
 */
SC_LOCK WINAPI
LockServiceDatabase( SC_HANDLE hDatabase )
{
    FIXME("pretending to lock the service database for 0x%08x\n", hDatabase);
    return (SC_LOCK)1;
}

/******************************************************************************
 * UnlockServiceDatabase [ADVAPI32.@]
 * Allow the service control manager to start services again.
 *
 * NOTES
 *   The specified database lock is released.
 *
 * PARAMS
 *   hLock [I] Handle to a SC database lock
 *
 * RETURNS STD
 */
BOOL WINAPI
UnlockServiceDatabase( SC_LOCK hLock )
{
    FIXME("pretending to unlock the service database\n");
    return TRUE;
}


/******************************************************************************
 *  ChangeServiceConfigA [ADVAPI32.@]
 *  Allow the service control manager to start services again.
 *
 */
BOOL WINAPI ChangeServiceConfigA( SC_HANDLE hService,
  DWORD dwServiceType,
  DWORD dwStartType,
  DWORD dwErrorControl,
  LPCSTR lpBinaryPathName,
  LPCSTR lpLoadOrderGroup,
  LPDWORD lpdwTagId,
  LPCSTR lpDependencies,
  LPCSTR lpServiceStartName,
  LPCSTR lpPassword,
  LPCSTR lpDisplayName )
{
  FIXME( "%x %x %x %x %s %s %p %s %s %s %s\n",
         hService, dwServiceType, dwStartType, dwErrorControl,
         debugstr_a(lpBinaryPathName), debugstr_a(lpLoadOrderGroup),
         lpdwTagId, debugstr_a(lpDependencies), debugstr_a(lpServiceStartName),
         debugstr_a(lpPassword), debugstr_a(lpDisplayName) );

  return TRUE;
}

/******************************************************************************
 * ChangeServiceConfigW [ADVAPI32.@]
 * Allow the service control manager to start services again.
 *
 */
BOOL WINAPI ChangeServiceConfigW( SC_HANDLE hService,
  DWORD dwServiceType,
  DWORD dwStartType,
  DWORD dwErrorControl,
  LPCWSTR lpBinaryPathName,
  LPCWSTR lpLoadOrderGroup,
  LPDWORD lpdwTagId,
  LPCWSTR lpDependencies,
  LPCWSTR lpServiceStartName,
  LPCWSTR lpPassword,
  LPCWSTR lpDisplayName )
{
  FIXME( "%x %x %x %x %s %s %p %s %s %s %s\n",
          hService, dwServiceType, dwStartType, dwErrorControl,
          debugstr_w(lpBinaryPathName), debugstr_w(lpLoadOrderGroup),
          lpdwTagId, debugstr_w(lpDependencies), debugstr_w(lpServiceStartName),
          debugstr_w(lpPassword), debugstr_w(lpDisplayName) );

  return TRUE;
}


/******************************************************************************
 * ChangeServiceConfig2A [ADVAPI32.@]
 *
 */
BOOL WINAPI ChangeServiceConfig2A(
  SC_HANDLE hService,
  DWORD dwInfoLevel,
  LPVOID lpInfo )
{
  FIXME( "(%x,0x%08x,%p:stub\n", hService, dwInfoLevel, lpInfo );
  return TRUE;
}

/******************************************************************************
 * ChangeServiceConfig2W [ADVAPI32.@]
 *
 */
BOOL WINAPI ChangeServiceConfig2W(
  SC_HANDLE hService,
  DWORD dwInfoLevel,
  LPVOID lpInfo )
{
  FIXME( "(%x,0x%08x,%p:stub\n", hService, dwInfoLevel, lpInfo );
  return TRUE;
}

