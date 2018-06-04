/*
 * Copyright (c) 2010-2015 NVIDIA CORPORATION. All rights reserved.
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

#include "wine/debug.h"
#include "shdocvw.h"
#include "taskbarlist.h"

WINE_DEFAULT_DEBUG_CHANNEL(shdocvw);

static HRESULT WINAPI TaskbarList_QueryInterface(ITaskbarList *iface, REFIID riid, void **ppv)
{
    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(IID_IUnknown %p)\n", ppv);
        *ppv = iface;
    }else if(IsEqualGUID(&IID_ITaskbarList, riid)) {
        TRACE("(IID_IUrlHistoryStg %p)\n", ppv);
        *ppv = iface;
    }
    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    WARN("(%s %p)\n", debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}


static ULONG WINAPI TaskbarList_AddRef(ITaskbarList *iface)
{
    SHDOCVW_UnlockModule();
    return 2;
}

static ULONG WINAPI TaskbarList_Release(ITaskbarList *iface)
{
    SHDOCVW_UnlockModule();
    return 1;
}

static HRESULT WINAPI TaskbarList_ActivateTab(ITaskbarList *iface, HWND hwnd)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI TaskbarList_AddTab(ITaskbarList *iface, HWND hwnd)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI TaskbarList_DeleteTab(ITaskbarList *iface, HWND hwnd)
{
    FIXME("()\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI TaskbarList_HrInit(ITaskbarList *iface)
{
    FIXME("()\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI TaskbarList_SetActiveAlt(ITaskbarList *iface, HWND hwnd)
{
    FIXME("()\n");
    return E_NOTIMPL;
}



static ITaskbarListVtbl TaskbarListVtbl = {
    TaskbarList_QueryInterface,
    TaskbarList_AddRef,
    TaskbarList_Release,
    TaskbarList_HrInit,
    TaskbarList_AddTab,
    TaskbarList_DeleteTab,
    TaskbarList_ActivateTab,
    TaskbarList_SetActiveAlt
};

static ITaskbarList TaskbarList = { &TaskbarListVtbl };

HRESULT TaskbarList_Create(IUnknown *pOuter, REFIID riid, void **ppv)
{
    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    return ITaskbarList_QueryInterface(&TaskbarList, riid, ppv);
}
