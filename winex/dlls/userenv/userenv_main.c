/*
 * Implementation of userenv.dll
 *
 * Copyright 2006 Mike McCormack for CodeWeavers
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
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

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "userenv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL( userenv );

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("%p %d %p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

BOOL WINAPI CreateEnvironmentBlock( LPVOID* lpEnvironment,
                     HANDLE hToken, BOOL bInherit )
{
    NTSTATUS status;


    TRACE("{lpEnvironment = %p, hToken = %p, bInherit = %d}\n", lpEnvironment, hToken, bInherit );

    if (lpEnvironment == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }


    status = RtlCreateEnvironment(bInherit, lpEnvironment);

    if (status != STATUS_SUCCESS)
    {
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    /* inheriting the environment from the requested user.  This needs to pull in that user's
       environment block and add it to the returned block.  Note that if the token is invalid
       this function should be setting the ERROR_ENVVAR_NOT_FOUND error code.  Also note that
       this task is not handled by the rtl level function. */
    if (hToken != NULL)
        FIXME("handle user token!\n");

    return TRUE;
}

BOOL WINAPI DestroyEnvironmentBlock(LPVOID lpEnvironment)
{
    TRACE("{lpEnvironment = %p}\n", lpEnvironment);

    if (lpEnvironment == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    RtlDestroyEnvironment(lpEnvironment);
    return TRUE;
}

BOOL WINAPI ExpandEnvironmentStringsForUserA( HANDLE hToken, LPCSTR lpSrc,
                     LPSTR lpDest, DWORD dwSize )
{
    BOOL ret;

    TRACE("%p %s %p %d\n", hToken, debugstr_a(lpSrc), lpDest, dwSize);

    ret = ExpandEnvironmentStringsA( lpSrc, lpDest, dwSize );
    TRACE("<- %s\n", debugstr_a(lpDest));
    return ret;
}

BOOL WINAPI ExpandEnvironmentStringsForUserW( HANDLE hToken, LPCWSTR lpSrc,
                     LPWSTR lpDest, DWORD dwSize )
{
    BOOL ret;

    TRACE("%p %s %p %d\n", hToken, debugstr_w(lpSrc), lpDest, dwSize);

    ret = ExpandEnvironmentStringsW( lpSrc, lpDest, dwSize );
    TRACE("<- %s\n", debugstr_w(lpDest));
    return ret;
}

BOOL WINAPI GetUserProfileDirectoryA( HANDLE hToken, LPSTR lpProfileDir,
                     LPDWORD lpcchSize )
{
    FIXME("%p %p %p\n", hToken, lpProfileDir, lpcchSize );
    return FALSE;
}

BOOL WINAPI GetUserProfileDirectoryW( HANDLE hToken, LPWSTR lpProfileDir,
                     LPDWORD lpcchSize )
{
    FIXME("%p %p %p\n", hToken, lpProfileDir, lpcchSize );
    return FALSE;
}

BOOL WINAPI GetProfilesDirectoryA( LPSTR lpProfilesDir, LPDWORD lpcchSize )
{
    FIXME("%p %p\n", lpProfilesDir, lpcchSize );
    return FALSE;
}

BOOL WINAPI GetProfilesDirectoryW( LPWSTR lpProfilesDir, LPDWORD lpcchSize )
{
    FIXME("%p %p\n", lpProfilesDir, lpcchSize );
    return FALSE;
}

BOOL WINAPI GetProfileType( DWORD *pdwFlags )
{
    FIXME("%p\n", pdwFlags );
    *pdwFlags = 0;
    return TRUE;
}

BOOL WINAPI LoadUserProfileA( HANDLE hToken, LPPROFILEINFOA lpProfileInfo )
{
    FIXME("%p %p\n", hToken, lpProfileInfo );
    lpProfileInfo->hProfile = HKEY_CURRENT_USER;
    return TRUE;
}

BOOL WINAPI RegisterGPNotification( HANDLE event, BOOL machine )
{
    FIXME("%p %d\n", event, machine );
    return TRUE;
}

BOOL WINAPI UnregisterGPNotification( HANDLE event )
{
    FIXME("%p\n", event );
    return TRUE;
}

BOOL WINAPI UnloadUserProfile( HANDLE hToken, HANDLE hProfile )
{
    FIXME("(%p, %p): stub\n", hToken, hProfile);
    return FALSE;
}
