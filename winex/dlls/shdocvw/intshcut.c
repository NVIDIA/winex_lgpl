/*
 * Copyright 2008 Damjan Jovanovic
 * Copyright (c) 2010-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * ShellLink's barely documented cousin that handles URLs.
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

/*
 * TODO:
 * fd.o desktop and menu integration
 * Implement the IShellLinkA/W interfaces
 * Handle the SetURL flags
 * Loading .url files
 * Implement any other interfaces? Does any software actually use them?
 *
 * The installer for the Zuma Deluxe Popcap game is good for testing.
 */

#include "wine/debug.h"
#include "shdocvw.h"
#include "objidl.h"
#include "shobjidl.h"
#include "intshcut.h"

WINE_DEFAULT_DEBUG_CHANNEL(shdocvw);

typedef struct
{
    IUniformResourceLocatorA uniformResourceLocatorA;
    IUniformResourceLocatorW uniformResourceLocatorW;
    IPersistFile persistFile;

    LONG refCount;

    WCHAR *url;
    BOOLEAN isDirty;
    LPOLESTR currentFile;
} InternetShortcut;

/* utility functions */

static inline InternetShortcut* impl_from_IUniformResourceLocatorA(IUniformResourceLocatorA *iface)
{
    return (InternetShortcut*)((char*)iface - FIELD_OFFSET(InternetShortcut, uniformResourceLocatorA));
}

static inline InternetShortcut* impl_from_IUniformResourceLocatorW(IUniformResourceLocatorW *iface)
{
    return (InternetShortcut*)((char*)iface - FIELD_OFFSET(InternetShortcut, uniformResourceLocatorW));
}

static inline InternetShortcut* impl_from_IPersistFile(IPersistFile *iface)
{
    return (InternetShortcut*)((char*)iface - FIELD_OFFSET(InternetShortcut, persistFile));
}

/* interface functions */

static HRESULT WINAPI Unknown_QueryInterface(InternetShortcut *This, REFIID riid, PVOID *ppvObject)
{
    TRACE("(%p, %s, %p)\n", This, debugstr_guid(riid), ppvObject);
    *ppvObject = NULL;
    if (IsEqualGUID(&IID_IUnknown, riid))
        *ppvObject = &This->uniformResourceLocatorA;
    else if (IsEqualGUID(&IID_IUniformResourceLocatorA, riid))
        *ppvObject = &This->uniformResourceLocatorA;
    else if (IsEqualGUID(&IID_IUniformResourceLocatorW, riid))
        *ppvObject = &This->uniformResourceLocatorW;
    else if (IsEqualGUID(&IID_IPersistFile, riid))
        *ppvObject = &This->persistFile;
    else if (IsEqualGUID(&IID_IShellLinkA, riid))
    {
        FIXME("The IShellLinkA interface is not yet supported by InternetShortcut\n");
        return E_NOINTERFACE;
    }
    else if (IsEqualGUID(&IID_IShellLinkW, riid))
    {
        FIXME("The IShellLinkW interface is not yet supported by InternetShortcut\n");
        return E_NOINTERFACE;
    }
    else
    {
        FIXME("Interface with GUID %s not yet implemented by InternetShortcut\n", debugstr_guid(riid));
        return E_NOINTERFACE;
    }
    IUnknown_AddRef((IUnknown*)*ppvObject);
    return S_OK;
}

static ULONG WINAPI Unknown_AddRef(InternetShortcut *This)
{
    TRACE("(%p)\n", This);
    return InterlockedIncrement(&This->refCount);
}

static ULONG WINAPI Unknown_Release(InternetShortcut *This)
{
    ULONG count;
    TRACE("(%p)\n", This);
    count = InterlockedDecrement(&This->refCount);
    if (count == 0)
    {
        CoTaskMemFree(This->url);
        CoTaskMemFree(This->currentFile);
        heap_free(This);
        SHDOCVW_UnlockModule();
    }
    return count;
}

static HRESULT WINAPI UniformResourceLocatorW_QueryInterface(IUniformResourceLocatorW *url, REFIID riid, PVOID *ppvObject)
{
    InternetShortcut *This = impl_from_IUniformResourceLocatorW(url);
    TRACE("(%p, %s, %p)\n", url, debugstr_guid(riid), ppvObject);
    return Unknown_QueryInterface(This, riid, ppvObject);
}

static ULONG WINAPI UniformResourceLocatorW_AddRef(IUniformResourceLocatorW *url)
{
    InternetShortcut *This = impl_from_IUniformResourceLocatorW(url);
    TRACE("(%p)\n", url);
    return Unknown_AddRef(This);
}

static ULONG WINAPI UniformResourceLocatorW_Release(IUniformResourceLocatorW *url)
{
    InternetShortcut *This = impl_from_IUniformResourceLocatorW(url);
    TRACE("(%p)\n", url);
    return Unknown_Release(This);
}

static HRESULT WINAPI UniformResourceLocatorW_SetUrl(IUniformResourceLocatorW *url, LPCWSTR pcszURL, DWORD dwInFlags)
{
    WCHAR *newURL = NULL;
    InternetShortcut *This = impl_from_IUniformResourceLocatorW(url);
    TRACE("(%p, %s, 0x%x)\n", url, debugstr_w(pcszURL), dwInFlags);
    if (dwInFlags != 0)
        FIXME("ignoring unsupported flags 0x%x\n", dwInFlags);
    if (pcszURL != NULL)
    {
        newURL = co_strdupW(pcszURL);
        if (newURL == NULL)
            return E_OUTOFMEMORY;
    }
    CoTaskMemFree(This->url);
    This->url = newURL;
    This->isDirty = TRUE;
    return S_OK;
}

static HRESULT WINAPI UniformResourceLocatorW_GetUrl(IUniformResourceLocatorW *url, LPWSTR *ppszURL)
{
    HRESULT hr = S_OK;
    InternetShortcut *This = impl_from_IUniformResourceLocatorW(url);
    TRACE("(%p, %p)\n", url, ppszURL);
    if (This->url == NULL)
        *ppszURL = NULL;
    else
    {
        *ppszURL = co_strdupW(This->url);
        if (*ppszURL == NULL)
            hr = E_OUTOFMEMORY;
    }
    return hr;
}

static HRESULT WINAPI UniformResourceLocatorW_InvokeCommand(IUniformResourceLocatorW *url, PURLINVOKECOMMANDINFOW pCommandInfo)
{
    FIXME("(%p, %p): stub\n", url, pCommandInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI UniformResourceLocatorA_QueryInterface(IUniformResourceLocatorA *url, REFIID riid, PVOID *ppvObject)
{
    InternetShortcut *This = impl_from_IUniformResourceLocatorA(url);
    TRACE("(%p, %s, %p)\n", url, debugstr_guid(riid), ppvObject);
    return Unknown_QueryInterface(This, riid, ppvObject);
}

static ULONG WINAPI UniformResourceLocatorA_AddRef(IUniformResourceLocatorA *url)
{
    InternetShortcut *This = impl_from_IUniformResourceLocatorA(url);
    TRACE("(%p)\n", url);
    return Unknown_AddRef(This);
}

static ULONG WINAPI UniformResourceLocatorA_Release(IUniformResourceLocatorA *url)
{
    InternetShortcut *This = impl_from_IUniformResourceLocatorA(url);
    TRACE("(%p)\n", url);
    return Unknown_Release(This);
}

static HRESULT WINAPI UniformResourceLocatorA_SetUrl(IUniformResourceLocatorA *url, LPCSTR pcszURL, DWORD dwInFlags)
{
    WCHAR *newURL = NULL;
    InternetShortcut *This = impl_from_IUniformResourceLocatorA(url);
    TRACE("(%p, %s, 0x%x)\n", url, debugstr_a(pcszURL), dwInFlags);
    if (dwInFlags != 0)
        FIXME("ignoring unsupported flags 0x%x\n", dwInFlags);
    if (pcszURL != NULL)
    {
        newURL = co_strdupAtoW(pcszURL);
        if (newURL == NULL)
            return E_OUTOFMEMORY;
    }
    CoTaskMemFree(This->url);
    This->url = newURL;
    This->isDirty = TRUE;
    return S_OK;
}

static HRESULT WINAPI UniformResourceLocatorA_GetUrl(IUniformResourceLocatorA *url, LPSTR *ppszURL)
{
    HRESULT hr = S_OK;
    InternetShortcut *This = impl_from_IUniformResourceLocatorA(url);
    TRACE("(%p, %p)\n", url, ppszURL);
    if (This->url == NULL)
        *ppszURL = NULL;
    else
    {
        *ppszURL = co_strdupWtoA(This->url);
        if (*ppszURL == NULL)
            hr = E_OUTOFMEMORY;
    }
    return hr;
}

static HRESULT WINAPI UniformResourceLocatorA_InvokeCommand(IUniformResourceLocatorA *url, PURLINVOKECOMMANDINFOA pCommandInfo)
{
    FIXME("(%p, %p): stub\n", url, pCommandInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_QueryInterface(IPersistFile *pFile, REFIID riid, PVOID *ppvObject)
{
    InternetShortcut *This = impl_from_IPersistFile(pFile);
    TRACE("(%p, %s, %p)\n", pFile, debugstr_guid(riid), ppvObject);
    return Unknown_QueryInterface(This, riid, ppvObject);
}

static ULONG WINAPI PersistFile_AddRef(IPersistFile *pFile)
{
    InternetShortcut *This = impl_from_IPersistFile(pFile);
    TRACE("(%p)\n", pFile);
    return Unknown_AddRef(This);
}

static ULONG WINAPI PersistFile_Release(IPersistFile *pFile)
{
    InternetShortcut *This = impl_from_IPersistFile(pFile);
    TRACE("(%p)\n", pFile);
    return Unknown_Release(This);
}

static HRESULT WINAPI PersistFile_GetClassID(IPersistFile *pFile, CLSID *pClassID)
{
    TRACE("(%p, %p)\n", pFile, pClassID);
    *pClassID = CLSID_InternetShortcut;
    return S_OK;
}

static HRESULT WINAPI PersistFile_IsDirty(IPersistFile *pFile)
{
    InternetShortcut *This = impl_from_IPersistFile(pFile);
    TRACE("(%p)\n", pFile);
    return This->isDirty ? S_OK : S_FALSE;
}

static HRESULT WINAPI PersistFile_Load(IPersistFile *pFile, LPCOLESTR pszFileName, DWORD dwMode)
{
    FIXME("(%p, %p, 0x%x): stub\n", pFile, pszFileName, dwMode);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_Save(IPersistFile *pFile, LPCOLESTR pszFileName, BOOL fRemember)
{
    HRESULT hr = S_OK;
    INT len;
    CHAR *url;
    InternetShortcut *This = impl_from_IPersistFile(pFile);

    TRACE("(%p, %s, %d)\n", pFile, debugstr_w(pszFileName), fRemember);

    if (pszFileName != NULL && fRemember)
    {
        LPOLESTR oldFile = This->currentFile;
        This->currentFile = co_strdupW(pszFileName);
        if (This->currentFile == NULL)
        {
            This->currentFile = oldFile;
            return E_OUTOFMEMORY;
        }
        CoTaskMemFree(oldFile);
    }
    if (This->url == NULL)
        return E_FAIL;

    /* Windows seems to always write:
     *   ASCII "[InternetShortcut]" headers
     *   ASCII names in "name=value" pairs
     *   An ASCII (probably UTF8?) value in "URL=..."
     */
    len = WideCharToMultiByte(CP_UTF8, 0, This->url, -1, NULL, 0, 0, 0);
    url = heap_alloc(len);
    if (url != NULL)
    {
        HANDLE file;
        WideCharToMultiByte(CP_UTF8, 0, This->url, -1, url, len, 0, 0);
        file = CreateFileW(pszFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD bytesWritten;
            char str_header[] = "[InternetShortcut]";
            char str_URL[] = "URL=";
            char str_eol[] = "\r\n";

            WriteFile(file, str_header, lstrlenA(str_header), &bytesWritten, NULL);
            WriteFile(file, str_eol, lstrlenA(str_eol), &bytesWritten, NULL);
            WriteFile(file, str_URL, lstrlenA(str_URL), &bytesWritten, NULL);
            WriteFile(file, url, lstrlenA(url), &bytesWritten, NULL);
            WriteFile(file, str_eol, lstrlenA(str_eol), &bytesWritten, NULL);
            CloseHandle(file);
            if (pszFileName == NULL || fRemember)
                This->isDirty = FALSE;
        }
        else
            hr = E_FAIL;
        heap_free(url);
    }
    else
        hr = E_OUTOFMEMORY;

    return hr;
}

static HRESULT WINAPI PersistFile_SaveCompleted(IPersistFile *pFile, LPCOLESTR pszFileName)
{
    FIXME("(%p, %p): stub\n", pFile, pszFileName);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_GetCurFile(IPersistFile *pFile, LPOLESTR *ppszFileName)
{
    HRESULT hr = S_OK;
    InternetShortcut *This = impl_from_IPersistFile(pFile);
    TRACE("(%p, %p)\n", pFile, ppszFileName);
    if (This->currentFile == NULL)
        *ppszFileName = NULL;
    else
    {
        *ppszFileName = co_strdupW(This->currentFile);
        if (*ppszFileName == NULL)
            hr = E_OUTOFMEMORY;
    }
    return hr;
}



static const IUniformResourceLocatorWVtbl uniformResourceLocatorWVtbl = {
    UniformResourceLocatorW_QueryInterface,
    UniformResourceLocatorW_AddRef,
    UniformResourceLocatorW_Release,
    UniformResourceLocatorW_SetUrl,
    UniformResourceLocatorW_GetUrl,
    UniformResourceLocatorW_InvokeCommand
};

static const IUniformResourceLocatorAVtbl uniformResourceLocatorAVtbl = {
    UniformResourceLocatorA_QueryInterface,
    UniformResourceLocatorA_AddRef,
    UniformResourceLocatorA_Release,
    UniformResourceLocatorA_SetUrl,
    UniformResourceLocatorA_GetUrl,
    UniformResourceLocatorA_InvokeCommand
};

static const IPersistFileVtbl persistFileVtbl = {
    PersistFile_QueryInterface,
    PersistFile_AddRef,
    PersistFile_Release,
    PersistFile_GetClassID,
    PersistFile_IsDirty,
    PersistFile_Load,
    PersistFile_Save,
    PersistFile_SaveCompleted,
    PersistFile_GetCurFile
};

HRESULT InternetShortcut_Create(IUnknown *pOuter, REFIID riid, void **ppv)
{
    InternetShortcut *This;
    HRESULT hr;

    TRACE("(%p, %s, %p)\n", pOuter, debugstr_guid(riid), ppv);

    *ppv = NULL;

    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(InternetShortcut));
    if (This)
    {
        This->uniformResourceLocatorA.lpVtbl = &uniformResourceLocatorAVtbl;
        This->uniformResourceLocatorW.lpVtbl = &uniformResourceLocatorWVtbl;
        This->persistFile.lpVtbl = &persistFileVtbl;
        This->refCount = 0;
        hr = Unknown_QueryInterface(This, riid, ppv);
        if (SUCCEEDED(hr))
            SHDOCVW_LockModule();
        else
            heap_free(This);
        return hr;
    }
    else
        return E_OUTOFMEMORY;
}
