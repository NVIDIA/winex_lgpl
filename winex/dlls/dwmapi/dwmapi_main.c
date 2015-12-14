/*
 * Dwmapi
 *
 * Copyright 2007 Andras Kovacs
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
 *
 */

#include "config.h"
#include <stdarg.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "dwmapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwmapi);


/* At process attach */
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch(fdwReason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hInstDLL );
        break;
    }
    return TRUE;
}

/**********************************************************************
 *           DwmIsCompositionEnabled         (DWMAPI.@)
 */
DWMAPI DwmIsCompositionEnabled(BOOL* pfEnabled)
{
    static int once;
    if (!once)
    {
        FIXME("%p\n", pfEnabled);
        once = 1;
    }
    else
        TRACE("%p\n", pfEnabled);

    *pfEnabled = FALSE;
    return S_OK;
}

/**********************************************************************
 *           DwmEnableComposition         (DWMAPI.102)
 */
DWMAPI DwmEnableComposition(UINT uCompositionAction)
{
    FIXME("(%d) stub\n", uCompositionAction);

    return S_OK;
}

/**********************************************************************
 *           DwmExtendFrameIntoClientArea    (DWMAPI.@)
 */
DWMAPI DwmExtendFrameIntoClientArea(HWND hWnd, const MARGINS* pMarInset)
{
    FIXME("(%p, %p) stub\n", (void *)hWnd, pMarInset);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmGetColorizationColor      (DWMAPI.@)
 */
DWMAPI DwmGetColorizationColor(DWORD* pcrColorization, BOOL* pfOpaqueBlend)
{
    FIXME("(%p, %p) stub\n", pcrColorization, pfOpaqueBlend);

    return E_NOTIMPL;
}

/**********************************************************************
 *                  DwmFlush              (DWMAPI.@)
 */
DWMAPI DwmFlush(void)
{
    FIXME("() stub\n");

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmSetWindowAttribute         (DWMAPI.@)
 */
DWMAPI DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
{
    FIXME("(%p, %x, %p, %x) stub\n", (void *)hwnd, dwAttribute, pvAttribute, cbAttribute);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmGetGraphicsStreamClient         (DWMAPI.@)
 */
DWMAPI DwmGetGraphicsStreamClient(UINT uIndex, UUID *pClientUuid)
{
    FIXME("(%d, %p) stub\n", uIndex, pClientUuid);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmGetTransportAttributes         (DWMAPI.@)
 */
DWMAPI DwmGetTransportAttributes(BOOL *pfIsRemoting, BOOL *pfIsConnected, DWORD *pDwGeneration)
{
    FIXME("(%p, %p, %p) stub\n", pfIsRemoting, pfIsConnected, pDwGeneration);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmUnregisterThumbnail         (DWMAPI.@)
 */
DWMAPI DwmUnregisterThumbnail(HTHUMBNAIL hThumbnailId)
{
    FIXME("(%p) stub\n", (void *)hThumbnailId);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmEnableMMCSS         (DWMAPI.@)
 */
DWMAPI DwmEnableMMCSS(BOOL fEnableMMCSS)
{
    FIXME("(%d) stub\n", fEnableMMCSS);

    return S_OK;
}

/**********************************************************************
 *           DwmGetGraphicsStreamTransformHint         (DWMAPI.@)
 */
DWMAPI DwmGetGraphicsStreamTransformHint(UINT uIndex, MIL_MATRIX3X2D *pTransform)
{
    FIXME("(%d, %p) stub\n", uIndex, pTransform);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmEnableBlurBehindWindow         (DWMAPI.@)
 */
DWMAPI DwmEnableBlurBehindWindow(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind)
{
    FIXME("%p, %p\n", (void *)hWnd, pBlurBehind);

    return E_NOTIMPL;
}

/**********************************************************************
 *           DwmDefWindowProc         (DWMAPI.@)
 */
DWMAPI_(BOOL) DwmDefWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult)
{
    static int i;

    if (!i++) FIXME("stub\n");

    return FALSE;
}

/**********************************************************************
 *           DwmGetWindowAttribute         (DWMAPI.@)
 */
DWMAPI DwmGetWindowAttribute(HWND hwnd, DWORD dwAttribute, PVOID pvAttribute, DWORD cbAttribute)
{
    FIXME("(%p, %d, %p, %d) stub\n", (void *)hwnd, dwAttribute, pvAttribute, cbAttribute);

    return E_NOTIMPL;
}
