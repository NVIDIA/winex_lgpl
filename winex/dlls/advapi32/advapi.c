/*
 * Win32 advapi functions
 *
 * Copyright 1995 Sven Verdoolaege
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

#include "winbase.h"
#include "windef.h"
#include "winnls.h"
#include "winerror.h"
#include "crypt.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(advapi);

/******************************************************************************
 * GetUserNameA [ADVAPI32.@]
 *
 * NOTE: lpSize returns the total length of the username, including the
 * terminating null character.
 */
BOOL WINAPI
GetUserNameA( LPSTR lpszName, LPDWORD lpSize )
{
  size_t len;
  char *name;

  struct passwd *pwd = getpwuid( getuid() );
  if (!pwd)
  {
    ERR("Username lookup failed: %s\n", strerror(errno));
    return 0;
  }

  name = pwd->pw_name;

  /* We need to include the null character when determining the size of the buffer. */
  len = strlen(name) + 1;
  if (len > *lpSize)
  {
    SetLastError(ERROR_MORE_DATA);
    *lpSize = len;
    return 0;
  }

  *lpSize = len;
  strcpy(lpszName, name);
  return 1;
}

/******************************************************************************
 * GetUserNameW [ADVAPI32.@]
 *
 * PARAMS
 *   lpszName []
 *   lpSize   []
 */
BOOL WINAPI
GetUserNameW( LPWSTR lpszName, LPDWORD lpSize )
{
	LPSTR name = (LPSTR)HeapAlloc( GetProcessHeap(), 0, *lpSize );
	DWORD	size = *lpSize;
	BOOL res = GetUserNameA(name,lpSize);

        /* FIXME: should set lpSize in WCHARs */
        if (size && !MultiByteToWideChar( CP_ACP, 0, name, -1, lpszName, size ))
            lpszName[size-1] = 0;
        HeapFree( GetProcessHeap(), 0, name );
	return res;
}


BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        init_csp_handle_table();
        break;
    case DLL_PROCESS_DETACH:
        deinit_csp_handle_table();
        break;
    }

    return TRUE;
}
