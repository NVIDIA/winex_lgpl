/*
 * Copyright 2006-2007 Jacek Caban for CodeWeavers
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
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"
#include "commctrl.h"
#include "advpub.h"
#include "wininet.h"
#include "shellapi.h"

#include "wine/debug.h"
#include "wine/unicode.h"
#include "wine/library.h"

#include "mshtml_private.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

BOOL install_wine_gecko(BOOL silent)
{
    HANDLE hsem;

    SetLastError(ERROR_SUCCESS);
    hsem = CreateSemaphoreA( NULL, 0, 1, "mshtml_install_semaphore");

    if(GetLastError() == ERROR_ALREADY_EXISTS) {
        WaitForSingleObject(hsem, INFINITE);
    }else {
        /* This is pretty much all going away... */
        FIXME("Handle installation/setup of Gecko component if needed.\n");
    }

    ReleaseSemaphore(hsem, 1, NULL);
    CloseHandle(hsem);

    return TRUE;
}
