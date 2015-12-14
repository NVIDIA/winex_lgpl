/*
 *      psapi.h        -       Declarations for PSAPI
 */

#ifndef __WINE_PSAPI_H
#define __WINE_PSAPI_H

#include "windef.h"

typedef struct _MODULEINFO {
  LPVOID lpBaseOfDll;
  DWORD SizeOfImage;
  LPVOID EntryPoint;
} MODULEINFO, *LPMODULEINFO;

typedef struct _PROCESS_MEMORY_COUNTERS {
  DWORD cb;
  DWORD PageFaultCount;
  DWORD PeakWorkingSetSize;
  DWORD WorkingSetSize;
  DWORD QuotaPeakPagedPoolUsage;
  DWORD QuotaPagedPoolUsage;
  DWORD QuotaPeakNonPagedPoolUsage;
  DWORD QuotaNonPagedPoolUsage;
  DWORD PagefileUsage;
  DWORD PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;
typedef PROCESS_MEMORY_COUNTERS *PPROCESS_MEMORY_COUNTERS;

typedef struct _PSAPI_WS_WATCH_INFORMATION {
  LPVOID FaultingPc;
  LPVOID FaultingVa;
} PSAPI_WS_WATCH_INFORMATION, *PPSAPI_WS_WATCH_INFORMATION;

typedef struct _ENUM_PAGE_FILE_INFORMATION {
  DWORD cb;
  DWORD Reserved;
  SIZE_T TotalSize;
  SIZE_T TotalInUse;
  SIZE_T PeakUsage;
} ENUM_PAGE_FILE_INFORMATION, *PENUM_PAGE_FILE_INFORMATION;

typedef BOOL (*EnumPageFilesProcA) (LPVOID pContext,
                                    PENUM_PAGE_FILE_INFORMATION pPageFileInfo,
                                    LPCSTR lpFilename);

typedef BOOL (*EnumPageFilesProcW) (LPVOID pContext,
                                    PENUM_PAGE_FILE_INFORMATION pPageFileInfo,
                                    LPWSTR lpFilename);

typedef EnumPageFilesProcA PENUM_PAGE_CALLBACKA;
typedef EnumPageFilesProcW PENUM_PAGE_CALLBACKW;
DECL_WINELIB_TYPE_AW(EnumPageFilesProc);

BOOL WINAPI EnumPageFilesA(PENUM_PAGE_CALLBACKA pCallback,
                           LPVOID lpContext);
BOOL WINAPI EnumPageFilesW(PENUM_PAGE_CALLBACKW pCallback,
                           LPVOID lpContext);
#define EnumPageFiles WINELIB_NAME_AW(EnumPageFiles);

BOOL WINAPI EnumProcessModules(HANDLE hProcess,
                               HMODULE *lphModule,
                               DWORD cb,
                               LPDWORD lpcbNeeded);

BOOL WINAPI EnumProcesses(DWORD *   pProcessIds, 
                          DWORD     cb, 
                          DWORD *   pBytesReturned);


DWORD WINAPI GetModuleBaseNameA(HANDLE hProcess,
                                HMODULE hModule,
                                LPSTR lpBaseName,
                                DWORD nSize);
DWORD WINAPI GetModuleBaseNameW(HANDLE hProcess,
                                HMODULE hModule,
                                LPWSTR lpBaseName,
                                DWORD nSize);
#define GetModuleBaseName WINELIB_NAME_AW(GetModuleBaseName);

DWORD WINAPI GetModuleFileNameExA(HANDLE hProcess,
                                  HMODULE hModule,
                                  LPSTR lpFilename,
                                  DWORD nSize);
DWORD WINAPI GetModuleFileNameExW(HANDLE hProcess,
                                  HMODULE hModule,
                                  LPWSTR lpFilename,
                                  DWORD nSize);
#define GetModuleFileNameEx WINELIB_NAME_AW(GetModuleFileNameEx);

BOOL WINAPI GetModuleInformation(HANDLE hProcess,
                                 HMODULE hModule,
                                 LPMODULEINFO lpmodinfo,
                                 DWORD cb);

#endif  /* __WINE_PSAPI_H */
