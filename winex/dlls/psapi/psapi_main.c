/*
 *      PSAPI library
 *
 *      Copyright 1998  Patrik Stridvall
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "winbase.h"
#include "winnls.h"
#include "windef.h"
#include "winerror.h"
#include "wine/debug.h"
#include "tlhelp32.h"
#include "psapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(psapi);

#include <string.h>

/***********************************************************************
 *           EmptyWorkingSet (PSAPI.@)
 */
BOOL WINAPI EmptyWorkingSet(HANDLE hProcess)
{
    TRACE("(hProcess = 0x%08x)", hProcess);

    return SetProcessWorkingSetSize(hProcess, 0xFFFFFFFF, 0xFFFFFFFF);
}

/***********************************************************************
 *           EnumDeviceDrivers (PSAPI.@)
 */
BOOL WINAPI EnumDeviceDrivers(
    LPVOID *lpImageBase, 
    DWORD   cb, 
    LPDWORD lpcbNeeded)
{
    FIXME("(lpImageBase = %p, cb = %ld, lpcbNeeded = %p): stub\n", lpImageBase, cb, lpcbNeeded);

    if(lpcbNeeded)
        *lpcbNeeded = 0;

    return TRUE;
}


/***********************************************************************
 *           EnumPageFilesA (PSAPI.@)
 */
BOOL WINAPI EnumPageFilesA(PENUM_PAGE_CALLBACKA pCallback,
                           LPVOID lpContext)
{
   /* We don't have any pagefiles, so no callbacks occur */
   return TRUE;
}


/***********************************************************************
 *           EnumPageFilesW (PSAPI.@)
 */
BOOL WINAPI EnumPageFilesW(PENUM_PAGE_CALLBACKW pCallback,
                           LPVOID lpContext)
{
   /* We don't have any pagefiles, so no callbacks occur */
   return TRUE;
}


/***********************************************************************
 *           EnumProcesses (PSAPI.@)
 */
BOOL WINAPI EnumProcesses(DWORD *pProcessIds, DWORD cb, DWORD *pBytesReturned)
{
  HANDLE snapshot;
  DWORD count = 0, max = cb / sizeof(DWORD);
  BOOL ret;
  DWORD errcode;
  PROCESSENTRY32 pe32;

  TRACE("(%p, %ld, %p)\n", pProcessIds, cb, pBytesReturned);

  snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
  if( snapshot == INVALID_HANDLE_VALUE ) return FALSE;

  pe32.dwSize = sizeof( PROCESSENTRY32 );
  ret = Process32First( snapshot, &pe32 );

  TRACE("ret = %d, count = %ld\n", ret, count);
  while (ret && (count < max)) {
    pProcessIds[count] = pe32.th32ProcessID;
    TRACE("pid[%ld]=%ld, %s\n", count, pProcessIds[count], pe32.szExeFile);
    count++;
    
    ret = Process32Next( snapshot, &pe32 );
    TRACE("ret = %d, count = %ld\n", ret, count);
  }

  if( (errcode = GetLastError()) == ERROR_NO_MORE_FILES ) 
    SetLastError(NO_ERROR);
  TRACE("errcode=0x%08lx\n", errcode);
    
  CloseHandle( snapshot );

  if(pBytesReturned)
    *pBytesReturned = count * sizeof(DWORD);

  return TRUE;
}

/***********************************************************************
 *           EnumProcessModules (PSAPI.@)
 */
BOOL WINAPI EnumProcessModules(
  HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded)
{
  BOOL ret;
  DWORD count = 0, actual = 0, max = cb / sizeof(HMODULE);
  DWORD errcode;
  HANDLE snapshot;
  MODULEENTRY32 me32;
  DWORD pid;

  TRACE("(hProcess=0x%08x, %p, %ld, %p)\n", hProcess, lphModule, cb, lpcbNeeded );

  pid = GetProcessId( hProcess );
  TRACE("using pid=%ld\n", pid);
  snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, pid);
  if( snapshot == INVALID_HANDLE_VALUE ) return FALSE;

  me32.dwSize = sizeof( MODULEENTRY32 );
  ret = Module32First( snapshot, &me32 );

  TRACE("ret = %d, count = %ld\n", ret, count);
  while( ret ) {
    if( actual < max ) {
      lphModule[actual++] = me32.hModule;
    }
    TRACE("hModule[%ld]=0x%08lx, %s, %s, pid=%ld\n", count, me32.hModule, me32.szExePath, 
	                                         me32.szModule, me32.th32ProcessID);
    count++;
    
    ret = Module32Next( snapshot, &me32 );
    TRACE("ret = %d, count = %ld\n", ret, count);
  }

  if( (errcode = GetLastError()) == ERROR_NO_MORE_FILES ) 
    SetLastError(NO_ERROR);
  TRACE("errcode=0x%08lx\n", errcode);
    
  CloseHandle( snapshot );

  if(lpcbNeeded)
    *lpcbNeeded = count * sizeof(HMODULE);

  return TRUE;
}

/***********************************************************************
 *          GetDeviceDriverBaseNameA (PSAPI.@)
 */
DWORD WINAPI GetDeviceDriverBaseNameA(
    LPVOID  ImageBase, 
    LPSTR   lpBaseName, 
    DWORD   nSize)
{
    FIXME("(ImageBase = %p, lpBaseName = %p, nSize = %ld): stub\n",
            ImageBase, lpBaseName, nSize);


    if(lpBaseName && nSize)
        lpBaseName[0] = '\0';

    return 0;
}

/***********************************************************************
 *           GetDeviceDriverBaseNameW (PSAPI.@)
 */
DWORD WINAPI GetDeviceDriverBaseNameW(
    LPVOID  ImageBase, 
    LPWSTR  lpBaseName, 
    DWORD   nSize)
{
    FIXME("(ImageBase = %p, lpBaseName = %p, nSize = %ld): stub\n",
            ImageBase, lpBaseName, nSize);


    if(lpBaseName && nSize)
        lpBaseName[0] = '\0';

    return 0;
}

/***********************************************************************
 *           GetDeviceDriverFileNameA (PSAPI.@)
 */
DWORD WINAPI GetDeviceDriverFileNameA(
    LPVOID  ImageBase, 
    LPSTR   lpFilename, 
    DWORD   nSize)
{
    FIXME("(ImageBase = %p, lpFilename = %p, nSize = %ld}: stub\n",
            ImageBase, lpFilename, nSize);


    if(lpFilename && nSize)
        lpFilename[0] = '\0';

    return 0;
}

/***********************************************************************
 *           GetDeviceDriverFileNameW (PSAPI.@)
 */
DWORD WINAPI GetDeviceDriverFileNameW(
    LPVOID  ImageBase, 
    LPWSTR  lpFilename, 
    DWORD   nSize)
{
    FIXME("(ImageBase = %p, lpFilename = %p, nSize = %ld): stub\n",
            ImageBase, lpFilename, nSize);


    if(lpFilename && nSize)
        lpFilename[0] = '\0';

    return 0;
}

/***********************************************************************
 *           GetMappedFileNameA (PSAPI.@)
 */
DWORD WINAPI GetMappedFileNameA(
    HANDLE  hProcess, 
    LPVOID  lpv, 
    LPSTR   lpFilename, 
    DWORD   nSize)
{
    FIXME("(hProcess = 0x%08x, lpv = %p, lpFilename = %p, nSize = %ld): stub\n",
            hProcess, lpv, lpFilename, nSize);


    if(lpFilename && nSize)
        lpFilename[0] = '\0';

    return 0;
}

/***********************************************************************
 *           GetMappedFileNameW (PSAPI.@)
 */
DWORD WINAPI GetMappedFileNameW(
    HANDLE  hProcess, 
    LPVOID  lpv, 
    LPWSTR  lpFilename, 
    DWORD   nSize)
{
    FIXME("(hProcess = 0x%08x, lpv = %p, lpFilename = %p, nSize = %ld): stub\n",
            hProcess, lpv, lpFilename, nSize);


    if(lpFilename && nSize)
        lpFilename[0] = '\0';

    return 0;
}


/***********************************************************************
 *           PSAPI_GetModuleFileNameExA (internal)
 */
static DWORD WINAPI PSAPI_GetModuleFileNameExA(
    HANDLE  hProcess, 
    HMODULE hModule, 
    LPSTR   lpFilename, 
    DWORD   nSize,
    BOOL    baseNameOnly)
{
    HANDLE          snapshot;
    MODULEENTRY32   me32;
    DWORD           pid;
    BOOL            ret;
    DWORD           len = 0;
    DWORD           errcode;


    TRACE("(hProcess = 0x%08x, hModule = 0x%08x, lpFilename = %p, nSize = %ld, baseNameOnly = %d)\n",
          hProcess, hModule, lpFilename, nSize, baseNameOnly);
    FIXME("swap this function's body with PSAPI_GetModuleFileNameExW() and fix Module32FirstW/NextW()!\n");


    if (lpFilename && nSize)
        lpFilename[0] = '\0'; 

    pid = GetProcessId( hProcess );
    TRACE("using pid = %ld\n", pid);


    /* the current process was requested => just grab the module filename from this process instead */
    if (pid == GetCurrentProcessId()){

        /* requested the base name only => read the filename into a temp buffer and parse out the base name */
        if (baseNameOnly){
            char    tmpName[MAX_PATH];
            char *  baseName;


            /* try to find the module's filename */
            if (GetModuleFileNameA(hModule, tmpName, MAX_PATH) == 0){
                ERR("could not retrieve the filename for the module 0x%08x in this process\n", hModule);

                return 0;
            }

            
            /* see if a path element was provided with the filename */
            baseName = strrchr(tmpName, '\\');

            /* a path separator was found => copy only the filename portion */
            if (baseName)
                strncpy(lpFilename, baseName + 1, nSize);

            /* no path separator was found => copy the entire filename */
            else
                strncpy(lpFilename, tmpName, nSize);

            /* make sure the buffer is terminated! */   
            lpFilename[nSize - 1] = 0;
            len = strlen(lpFilename);
            
            return len;
        }

        /* requested the full path of the module's file => just read directly into the output buffer */
        else
            return GetModuleFileNameA(hModule, lpFilename, nSize);
    }


    if (!hModule) 
        FIXME("get the EXE name of the hProcess!\n");

    snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, pid);
    if ( snapshot == INVALID_HANDLE_VALUE ) 
        return FALSE;

    me32.dwSize = sizeof( MODULEENTRY32 );
    ret = Module32First( snapshot, &me32 );

    TRACE("ret = %d\n", ret);
    while (ret){

        if( hModule == me32.hModule ) {
            TRACE("found hModule = 0x%08lx, exePath = '%s', module = '%s', pid = %ld\n", 
                    me32.hModule, me32.szExePath, me32.szModule, me32.th32ProcessID);


            if (lpFilename && nSize) {
                if (baseNameOnly)
                    strncpy(lpFilename, me32.szModule, nSize);

                else
                    strncpy(lpFilename, me32.szExePath, nSize);

                lpFilename[nSize - 1] = '\0';
                len = strlen(lpFilename);
            }

            break;
        }
      
        ret = Module32Next( snapshot, &me32 );
        TRACE("ret = %d\n", ret);
    }

    if ( (errcode = GetLastError()) == ERROR_NO_MORE_FILES ) 
        SetLastError(NO_ERROR);


    TRACE("errcode=0x%08lx\n", errcode);
      
    CloseHandle( snapshot );

    TRACE("returning filename='%s'\n", lpFilename);
    
    return len;
}

/***********************************************************************
 *           PSAPI_GetModuleFileNameExW (internal)
 */
static DWORD WINAPI PSAPI_GetModuleFileNameExW(
    HANDLE  hProcess, 
    HMODULE hModule, 
    LPWSTR  lpFilename, 
    DWORD   nSize,
    BOOL    baseNameOnly)
{
    char *buffer;
    DWORD result;


    TRACE("(hProcess = 0x%08x, hModule = 0x%08x, lpFilename = %p, nSize = %ld, baseNameOnly = %d)\n",
            hProcess, hModule, lpFilename, nSize, baseNameOnly);

    if(lpFilename && nSize)
        lpFilename[0] = '\0';


    /* !! This is really bad practice to do, but necessary for the moment.  The 'A' version of a 
          function should always call through to the 'W' version then convert the result instead
          of the other way around.  However, there is currently an issue with the Module32First()
          and Module32Next() functions - there are no 'W' equivalents actually built or exported.
    */
    /* FIXME: this function and PSAPI_GetModuleFileNameExA() should have their bodies swapped 
              so that the 'A' version calls this one and this one performs the actual work. 
              Need Module32First/Next() fixed with wide character support first though. */
    FIXME("swap this function's body with PSAPI_GetModuleFileNameExA() and fix Module32FirstW/NextW()!\n");


    buffer = (char *)HeapAlloc(GetProcessHeap(), 0, nSize * sizeof(char));

    if (buffer == NULL){
        ERR("not enough memory to allocate a temporary buffer!\n");
        SetLastError(ERROR_OUTOFMEMORY);

        return 0;
    }


    result = PSAPI_GetModuleFileNameExA(hProcess, hModule, buffer, nSize, baseNameOnly);

    /* call succeeded => convert the filename to a wide string */
    if (result != 0){
        if (!MultiByteToWideChar(CP_ACP, 0, buffer, -1, lpFilename, nSize)){
            ERR("could not convert the string '%s' to a wide string!\n", buffer);
            HeapFree(GetProcessHeap(), 0, buffer);

            return 0;
        }
    }

    else
        ERR("could not retrieve the filename of the module 0x%08x\n", hModule);


    HeapFree(GetProcessHeap(), 0, buffer);

    return result;
}


/***********************************************************************
 *           GetModuleBaseNameA (PSAPI.@)
 */
DWORD WINAPI GetModuleBaseNameA(
    HANDLE  hProcess, 
    HMODULE hModule, 
    LPSTR   lpBaseName, 
    DWORD   nSize)
{
    TRACE("(hProcess=0x%08x, hModule=0x%08x, lpBaseName = %p, nSize = %ld)\n",
            hProcess, hModule, lpBaseName, nSize);


    return PSAPI_GetModuleFileNameExA(hProcess, hModule, lpBaseName, nSize, TRUE);
}

/***********************************************************************
 *           GetModuleBaseNameW (PSAPI.@)
 */
DWORD WINAPI GetModuleBaseNameW(
    HANDLE  hProcess, 
    HMODULE hModule, 
    LPWSTR  lpBaseName, 
    DWORD   nSize)
{
    TRACE("(hProcess = 0x%08x, hModule = 0x%08x, lpBaseName = %p, nSize = %ld)\n",
            hProcess, hModule, lpBaseName, nSize);


    return PSAPI_GetModuleFileNameExW(hProcess, hModule, lpBaseName, nSize, TRUE);
}


/***********************************************************************
 *           GetModuleFileNameExA (PSAPI.@)
 */
DWORD WINAPI GetModuleFileNameExA(
    HANDLE  hProcess, 
    HMODULE hModule, 
    LPSTR   lpFilename, 
    DWORD   nSize)
{
    TRACE("(hProcess = 0x%08x, hModule = 0x%08x, lpFilename = %p, nSize = %ld)\n",
            hProcess, hModule, lpFilename, nSize);


    return PSAPI_GetModuleFileNameExA(hProcess, hModule, lpFilename, nSize, FALSE);
}

/***********************************************************************
 *           GetModuleFileNameExW (PSAPI.@)
 */
DWORD WINAPI GetModuleFileNameExW(
    HANDLE  hProcess, 
    HMODULE hModule, 
    LPWSTR  lpFilename, 
    DWORD   nSize)
{
    TRACE("(hProcess = 0x%08x, hModule = 0x%08x, lpFilename = %p, nSize = %ld)\n",
            hProcess, hModule, lpFilename, nSize);


    return PSAPI_GetModuleFileNameExW(hProcess, hModule, lpFilename, nSize, FALSE);
}



/***********************************************************************
 *           GetModuleInformation (PSAPI.@)
 */
BOOL WINAPI GetModuleInformation(
  HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb)
{
  HANDLE snapshot;
  MODULEENTRY32 me32;
  DWORD pid;
  BOOL ret, res;
  DWORD errcode;

  TRACE("(hProcess = 0x%08x, hModule = 0x%08x, lpmodinfo = %p, cb = %ld)\n",
        hProcess, hModule, lpmodinfo, cb);

  memset(lpmodinfo, 0, cb);
  res = FALSE;

  if( !hModule ) 
    FIXME("get EXE name of hProcess\n");

  pid = GetProcessId( hProcess );
  TRACE("using pid=%ld\n", pid);
  snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, pid);
  if( snapshot == INVALID_HANDLE_VALUE ) return FALSE;

  me32.dwSize = sizeof( MODULEENTRY32 );
  ret = Module32First( snapshot, &me32 );

  TRACE("ret = %d\n", ret);
  while( ret ) {

    if( hModule == me32.hModule ) {
      DWORD lfanew, entry;
      TRACE("found hModule=0x%08lx, %s, %s, pid=%ld\n", 
	  me32.hModule, me32.szExePath, me32.szModule, me32.th32ProcessID);

      lpmodinfo->lpBaseOfDll = me32.modBaseAddr;
      lpmodinfo->SizeOfImage = me32.modBaseSize;
      lpmodinfo->EntryPoint = NULL;
      if (!ReadProcessMemory(hProcess, &((IMAGE_DOS_HEADER*)me32.modBaseAddr)->e_lfanew,
                             &lfanew, sizeof(lfanew), NULL))
        return FALSE;
      if (!ReadProcessMemory(hProcess, &((IMAGE_NT_HEADERS*)(me32.modBaseAddr+lfanew))->OptionalHeader.AddressOfEntryPoint,
                             &entry, sizeof(entry), NULL))
        return FALSE;
      /* I'm unsure whether it's up to us or the caller to add the module base address...
       * Until proven otherwise, I'll believe it's the former. */
      lpmodinfo->EntryPoint = entry ? (me32.modBaseAddr + entry) : NULL;
      res = TRUE;
      break;
    }
    
    ret = Module32Next( snapshot, &me32 );
    TRACE("ret = %d\n", ret);
  }

  if( (errcode = GetLastError()) == ERROR_NO_MORE_FILES ) 
    SetLastError(NO_ERROR);
  TRACE("errcode=0x%08lx\n", errcode);
    
  CloseHandle( snapshot );

  TRACE("returning %d\n", res);
  
  return res;

}

/***********************************************************************
 *           GetProcessMemoryInfo (PSAPI.@)
 */
BOOL WINAPI GetProcessMemoryInfo(
    HANDLE                      Process, 
    PPROCESS_MEMORY_COUNTERS    ppsmemCounters, 
    DWORD                       cb)
{
    FIXME("(hProcess = 0x%08x, ppsmemCounters = %p, cb = %ld): stub\n",
            Process, ppsmemCounters, cb);

    memset(ppsmemCounters, 0, cb);

    return TRUE;
}

/***********************************************************************
 *           GetWsChanges (PSAPI.@)
 */
BOOL WINAPI GetWsChanges(
    HANDLE                      hProcess, 
    PPSAPI_WS_WATCH_INFORMATION lpWatchInfo, 
    DWORD                       cb)
{
    FIXME("(hProcess = 0x%08x, lpWatchInfo = %p, cb = %ld): stub\n",
            hProcess, lpWatchInfo, cb);

    memset(lpWatchInfo, 0, cb);

    return TRUE;
}

/***********************************************************************
 *           InitializeProcessForWsWatch (PSAPI.@)
 */
BOOL WINAPI InitializeProcessForWsWatch(HANDLE hProcess)
{
    FIXME("(hProcess = 0x%08x): stub\n", hProcess);

    return TRUE;
}

/***********************************************************************
 *           QueryWorkingSet (PSAPI.?)
 * FIXME
 *     I haven't been able to find the ordinal for this function,
 *     This means it can't be called from outside the DLL.
 */
BOOL WINAPI QueryWorkingSet(HANDLE hProcess, LPVOID pv, DWORD cb)
{
    FIXME("(hProcess = 0x%08x, pv = %p, cb = %ld)\n", hProcess, pv, cb);

    if(pv && cb)
        ((DWORD *) pv)[0] = 0; /* Empty WorkingSet */

    return TRUE;
}
