/*
 * Win32 miscellaneous functions
 *
 * Copyright 1995 Thomas Sandford (tdgsandf@prds-grn.demon.co.uk)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

/* Misc. new functions - they should be moved into appropriate files
at a later date. */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32);
WINE_DECLARE_DEBUG_CHANNEL(debug);

/****************************************************************************
 *		FlushInstructionCache (KERNEL32.@)
 */
BOOL WINAPI FlushInstructionCache(HANDLE hProcess, LPCVOID lpBaseAddress, DWORD dwSize)
{
#ifdef __i386__
    return TRUE;
#else
    FIXME_(debug)("(0x%08lx,%p,0x%08lx): stub\n",(DWORD)hProcess, lpBaseAddress, dwSize);
    return TRUE;
#endif
}

/***********************************************************************
 *           GetSystemPowerStatus      (KERNEL32.@)
 */
BOOL WINAPI GetSystemPowerStatus(LPSYSTEM_POWER_STATUS sps_ptr)
{
    return FALSE;   /* no power management support */
}


/***********************************************************************
 *           SetSystemPowerState      (KERNEL32.@)
 */
BOOL WINAPI SetSystemPowerState(BOOL suspend_or_hibernate,
                                  BOOL force_flag)
{
    /* suspend_or_hibernate flag: w95 does not support
       this feature anyway */

    for ( ;0; )
    {
        if ( force_flag )
        {
        }
        else
        {
        }
    }
    return TRUE;
}


/******************************************************************************
 * CreateMailslotA [KERNEL32.@]
 */
HANDLE WINAPI CreateMailslotA( LPCSTR lpName, DWORD nMaxMessageSize,
                                   DWORD lReadTimeout, LPSECURITY_ATTRIBUTES sa)
{
    FIXME("(%s,%ld,%ld,%p): stub\n", debugstr_a(lpName),
          nMaxMessageSize, lReadTimeout, sa);
    return 1;
}


/******************************************************************************
 * CreateMailslotW [KERNEL32.@]  Creates a mailslot with specified name
 *
 * PARAMS
 *    lpName          [I] Pointer to string for mailslot name
 *    nMaxMessageSize [I] Maximum message size
 *    lReadTimeout    [I] Milliseconds before read time-out
 *    sa              [I] Pointer to security structure
 *
 * RETURNS
 *    Success: Handle to mailslot
 *    Failure: INVALID_HANDLE_VALUE
 */
HANDLE WINAPI CreateMailslotW( LPCWSTR lpName, DWORD nMaxMessageSize,
                                   DWORD lReadTimeout, LPSECURITY_ATTRIBUTES sa )
{
    FIXME("(%s,%ld,%ld,%p): stub\n", debugstr_w(lpName),
          nMaxMessageSize, lReadTimeout, sa);
    return 1;
}


/******************************************************************************
 * GetMailslotInfo [KERNEL32.@]  Retrieves info about specified mailslot
 *
 * PARAMS
 *    hMailslot        [I] Mailslot handle
 *    lpMaxMessageSize [O] Address of maximum message size
 *    lpNextSize       [O] Address of size of next message
 *    lpMessageCount   [O] Address of number of messages
 *    lpReadTimeout    [O] Address of read time-out
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetMailslotInfo( HANDLE hMailslot, LPDWORD lpMaxMessageSize,
                               LPDWORD lpNextSize, LPDWORD lpMessageCount,
                               LPDWORD lpReadTimeout )
{
    FIXME("(%04x): stub\n",hMailslot);
    if (lpMaxMessageSize) *lpMaxMessageSize = (DWORD)NULL;
    if (lpNextSize) *lpNextSize = (DWORD)NULL;
    if (lpMessageCount) *lpMessageCount = (DWORD)NULL;
    if (lpReadTimeout) *lpReadTimeout = (DWORD)NULL;
    return TRUE;
}


/******************************************************************************
 * GetCompressedFileSizeA [KERNEL32.@]
 *
 * NOTES
 *    This should call the W function below
 */
DWORD WINAPI GetCompressedFileSizeA(
    LPCSTR lpFileName,
    LPDWORD lpFileSizeHigh)
{
    FIXME("(...): stub\n");
    return 0xffffffff;
}


/******************************************************************************
 * GetCompressedFileSizeW [KERNEL32.@]
 *
 * RETURNS
 *    Success: Low-order doubleword of number of bytes
 *    Failure: 0xffffffff
 */
DWORD WINAPI GetCompressedFileSizeW(
    LPCWSTR lpFileName,     /* [in]  Pointer to name of file */
    LPDWORD lpFileSizeHigh) /* [out] Receives high-order doubleword of size */
{
    FIXME("(%s,%p): stub\n",debugstr_w(lpFileName),lpFileSizeHigh);
    return 0xffffffff;
}


/******************************************************************************
 * SetComputerNameA [KERNEL32.@]
 */
BOOL WINAPI SetComputerNameA( LPCSTR lpComputerName )
{
    BOOL ret;
    DWORD len = MultiByteToWideChar( CP_ACP, 0, lpComputerName, -1, NULL, 0 );
    LPWSTR nameW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );

    MultiByteToWideChar( CP_ACP, 0, lpComputerName, -1, nameW, len );
    ret = SetComputerNameW( nameW );
    HeapFree( GetProcessHeap(), 0, nameW );
    return ret;
}


/******************************************************************************
 * SetComputerNameW [KERNEL32.@]
 *
 * PARAMS
 *    lpComputerName [I] Address of new computer name
 *
 * RETURNS STD
 */
BOOL WINAPI SetComputerNameW( LPCWSTR lpComputerName )
{
    FIXME("(%s): stub\n", debugstr_w(lpComputerName));
    return TRUE;
}
