/* Copyright 2005 Ulrich Czekalla
 * Copyright (c) 2011-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "wtsapi32.h"
#include "psapi.h"
#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(wtsapi);

static HMODULE WTSAPI32_hModule;

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("%p,%x,%p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hinstDLL);
            WTSAPI32_hModule = hinstDLL;
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            break;
        }
    }

    return TRUE;
}

/************************************************************
 *                WTSCloseServer  (WTSAPI32.@)
 */
void WINAPI WTSCloseServer(HANDLE hServer)
{
    FIXME("Stub %p\n", hServer);
}

/************************************************************
 *                WTSDisconnectSession  (WTSAPI32.@)
 */
BOOL WINAPI WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
    FIXME("Stub %p 0x%08x %d\n", hServer, SessionId, bWait);
    return TRUE;
}

/************************************************************
 *                WTSEnumerateProcessesA  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
    FIXME("Stub %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppProcessInfo, pCount);

    if (!ppProcessInfo || !pCount) return FALSE;

    *pCount = 0;
    *ppProcessInfo = NULL;

    return TRUE;
}

/************************************************************
 *                WTSEnumerateProcessesW  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateProcessesW(HANDLE               hServer,
                                   DWORD                Reserved,
                                   DWORD                Version,
                                   PWTS_PROCESS_INFOW * ppProcessInfo,
                                   DWORD *              pCount)
{
    BOOL                result;
    DWORD               ids[256];
    DWORD               bytes;
    DWORD               count;
    DWORD               i;
    DWORD               len;
    HANDLE              process;
    DWORD               outCount;
    DWORD               strlength;
    WTS_PROCESS_INFOW * info = NULL;
    LPWSTR              string = NULL;
    WCHAR               filename[MAX_PATH];


    TRACE("partial stub {hServer = %p, Reserved = 0x%08x, Version = 0x%08x, ppProcessInfo = %p, pCount = %p}\n",
            hServer, Reserved, Version, ppProcessInfo, pCount);

    if (hServer != WTS_CURRENT_SERVER_HANDLE)
    {
        FIXME("non-local server not supported!\n");
        return FALSE;
    }

    if (ppProcessInfo == NULL || pCount == NULL)
        return FALSE;

    *pCount = 0;
    *ppProcessInfo = NULL;


    /* grab a list of all local process IDs */
    result = EnumProcesses(ids, sizeof(ids), &bytes);

    if (!result)
    {
        ERR("failed to enumerate process IDs\n");
        return FALSE;
    }


    count = bytes / sizeof(ids[0]);
    bytes = 0;
    outCount = 0;
    strlength = 0;

    /* walk through the list of process IDs twice - once to calculate how much space is needed
       for the output buffer, and once to actually collect the info. */
TryAgain:
    for (i = 0; i < count; i++)
    {
        /* attempt to open a handle to the process so we can retrieve it's name */
        process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ids[i]);

        if (process == NULL)
        {
            ERR("failed to open a handle for the process %d {error = %d}\n", ids[i], GetLastError());
            continue;
        }

        len = GetModuleFileNameExW(process, NULL, filename, sizeof(filename) / sizeof(filename[0]));

        /* the info object has been allocated => fill in the record for this process. */
        if (info != NULL)
        {
            info[outCount].SessionId = 1;
            info[outCount].pUserSid = (PSID)0xdeadbeef;
            info[outCount].ProcessId = ids[i];
            info[outCount].pProcessName = string;

            lstrcpynW(string, filename, strlength);
            string += len + 1;
            strlength -= len + 1;
        }

        /* the info object hasn't been allocated => count up the bytes required for this process. */
        else
        {
            /* this process entry requires one entry and enough space to store the filename. */
            bytes += sizeof(WTS_PROCESS_INFOW) + ((len + 1) * sizeof(WCHAR));
            strlength += len + 1;
        }

        outCount++;
        CloseHandle(process);
    }

    /* attempt to allocate an info array for the output.  This buffer will include space for each
       of the process entries, plus space at the end for all of the required filename strings.
       The strings must be stored as part of the same buffer since it is freed using the common
       WTSFreeMemory() function (which has no knowledge of this structure). */
    if (info == NULL)
    {
        TRACE("allocating %d bytes for the output buffer {outCount = %d}\n", bytes, outCount);
        info = HeapAlloc(GetProcessHeap(), 0, bytes);

        if (info == NULL)
        {
            ERR("failed to allocate memory for the output buffer\n");
            return FALSE;
        }

        /* set the string pointer to the start of the space after the info table. */
        string = (LPWSTR)(info + outCount);
        outCount = 0;
        goto TryAgain;
    }


    *pCount = outCount;
    *ppProcessInfo = info;

    return TRUE;
}

/************************************************************
 *                WTSEnumerateEnumerateSessionsA  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
    FIXME("Stub %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppSessionInfo, pCount);

    if (!ppSessionInfo || !pCount) return FALSE;

    *pCount = 0;
    *ppSessionInfo = NULL;

    return TRUE;
}

/************************************************************
 *                WTSEnumerateEnumerateSessionsW  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
    FIXME("Stub %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppSessionInfo, pCount);

    if (!ppSessionInfo || !pCount) return FALSE;

    *pCount = 0;
    *ppSessionInfo = NULL;

    return TRUE;
}

/************************************************************
 *                WTSFreeMemory (WTSAPI32.@)
 */
void WINAPI WTSFreeMemory(PVOID pMemory)
{
    FIXME("Stub %p\n", pMemory);
    if (pMemory != NULL)
        HeapFree(GetProcessHeap(), 0, pMemory);
}

/************************************************************
 *                WTSOpenServerA (WTSAPI32.@)
 */
HANDLE WINAPI WTSOpenServerA(LPSTR pServerName)
{
    FIXME("(%s) stub\n", debugstr_a(pServerName));
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

/************************************************************
 *                WTSOpenServerW (WTSAPI32.@)
 */
HANDLE WINAPI WTSOpenServerW(LPWSTR pServerName)
{
    FIXME("(%s) stub\n", debugstr_w(pServerName));
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

/************************************************************
 *                WTSQuerySessionInformationA  (WTSAPI32.@)
 */
BOOL WINAPI WTSQuerySessionInformationA(
    HANDLE hServer,
    DWORD SessionId,
    WTS_INFO_CLASS WTSInfoClass,
    LPSTR* Buffer,
    DWORD* BytesReturned)
{
    /* FIXME: Forward request to winsta.dll::WinStationQueryInformationA */
    FIXME("Stub %p 0x%08x %d %p %p\n", hServer, SessionId, WTSInfoClass,
        Buffer, BytesReturned);

    return FALSE;
}

/************************************************************
 *                WTSQuerySessionInformationW  (WTSAPI32.@)
 */
BOOL WINAPI WTSQuerySessionInformationW(
    HANDLE hServer,
    DWORD SessionId,
    WTS_INFO_CLASS WTSInfoClass,
    LPWSTR* Buffer,
    DWORD* BytesReturned)
{
    /* FIXME: Forward request to winsta.dll::WinStationQueryInformationW */
    FIXME("Stub %p 0x%08x %d %p %p\n", hServer, SessionId, WTSInfoClass,
        Buffer, BytesReturned);

    return FALSE;
}

/************************************************************
 *                WTSWaitSystemEvent (WTSAPI32.@)
 */
BOOL WINAPI WTSWaitSystemEvent(HANDLE hServer, DWORD Mask, DWORD* Flags)
{
    /* FIXME: Forward request to winsta.dll::WinStationWaitSystemEvent */
    FIXME("Stub %p 0x%08x %p\n", hServer, Mask, Flags);
    return FALSE;
}

/************************************************************
 *                WTSRegisterSessionNotification (WTSAPI32.@)
 */
BOOL WINAPI WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
    FIXME("Stub %p 0x%08x\n", hWnd, dwFlags);
    return TRUE;
}

/************************************************************
 *                WTSUnRegisterSessionNotification (WTSAPI32.@)
 */
BOOL WINAPI WTSUnRegisterSessionNotification(HWND hWnd)
{
    FIXME("Stub %p\n", hWnd);
    return TRUE;
}
