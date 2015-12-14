/*
 * UrlMon
 *
 * Copyright 1999 Ulrich Czekalla for Corel Corporation
 * Copyright 2002 Huw D M Davies for CodeWeavers
 * Copyright 2005 Jacek Caban for CodeWeavers
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

#include <stdio.h>

#include "urlmon_main.h"

#include "winreg.h"
#include "winternl.h"
#include "wininet.h"
#include "shlwapi.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(urlmon);

/* native urlmon.dll uses this key, too */
static WCHAR BSCBHolder[] = { '_','B','S','C','B','_','H','o','l','d','e','r','_',0 };

/*static BOOL registered_wndclass = FALSE;*/

typedef struct {
    const IBindingVtbl *lpVtbl;

    LONG ref;

    LPWSTR URLName;

    HWND hwndCallback;
    IBindCtx *pBC;
    HINTERNET hinternet, hconnect, hrequest;
    HANDLE hCacheFile;
    IUMCacheStream *pstrCache;
    IBindStatusCallback *pbscb;
    DWORD total_read, expected_size;
} Binding;

static HRESULT WINAPI Binding_QueryInterface(IBinding* iface, REFIID riid, void **ppvObject)
{
    Binding *This = (Binding*)iface;

    TRACE("(%p)->(%s,%p)\n", This, debugstr_guid(riid), ppvObject);

    if((This == NULL) || (ppvObject == NULL))
	return E_INVALIDARG;

    if (IsEqualIID(&IID_IUnknown, riid) || IsEqualIID(&IID_IBinding, riid)) {
        *ppvObject = iface;
        IBinding_AddRef(iface);
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI Binding_AddRef(IBinding* iface)
{
    Binding *This = (Binding*)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI Binding_Release(IBinding* iface)
{
    Binding *This = (Binding*)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n",This, ref);

    if(!ref) {
        heap_free(This->URLName);
        if (This->hCacheFile)
            CloseHandle(This->hCacheFile);
        if (This->pstrCache)
        {
            UMCloseCacheFileStream(This->pstrCache);
            IStream_Release((IStream *)This->pstrCache);
        }
        if (This->pbscb)
            IBindStatusCallback_Release(This->pbscb);

        heap_free(This);

        URLMON_UnlockModule();
    }

    return ref;
}

static HRESULT WINAPI Binding_Abort(IBinding* iface)
{
    Binding *This = (Binding*)iface;

    FIXME("(%p): stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI Binding_GetBindResult(IBinding* iface, CLSID* pclsidProtocol, DWORD* pdwResult, LPOLESTR* pszResult, DWORD* pdwReserved)
{
    Binding *This = (Binding*)iface;

    FIXME("(%p)->(%p, %p, %p, %p): stub\n", This, pclsidProtocol, pdwResult, pszResult, pdwReserved);

    return E_NOTIMPL;
}

static HRESULT WINAPI Binding_GetPriority(IBinding* iface, LONG* pnPriority)
{
    Binding *This = (Binding*)iface;

    FIXME("(%p)->(%p): stub\n", This, pnPriority);

    return E_NOTIMPL;
}

static HRESULT WINAPI Binding_Resume(IBinding* iface)
{
    Binding *This = (Binding*)iface;

    FIXME("(%p): stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI Binding_SetPriority(IBinding* iface, LONG nPriority)
{
    Binding *This = (Binding*)iface;

    FIXME("(%p)->(%d): stub\n", This, nPriority);

    return E_NOTIMPL;
}

static HRESULT WINAPI Binding_Suspend(IBinding* iface)
{
    Binding *This = (Binding*)iface;

    FIXME("(%p): stub\n", This);

    return E_NOTIMPL;
}

static void Binding_CloseCacheDownload(Binding *This)
{
    CloseHandle(This->hCacheFile);
    This->hCacheFile = 0;
    UMCloseCacheFileStream(This->pstrCache);
    IStream_Release((IStream *)This->pstrCache);
    This->pstrCache = 0;
}

static HRESULT Binding_MoreCacheData(Binding *This, const char *buf, DWORD dwBytes)
{
    DWORD written;

    if (WriteFile(This->hCacheFile, buf, dwBytes, &written, NULL) && written == dwBytes)
    {
	HRESULT hr;

	This->total_read += written;
        hr = IBindStatusCallback_OnProgress(This->pbscb,
					    This->total_read + written,
					    This->expected_size,
					    (This->total_read == written) ?
					        BINDSTATUS_BEGINDOWNLOADDATA :
					        BINDSTATUS_DOWNLOADINGDATA,
					    This->URLName);
	if (hr == S_OK)
	{
	    STGMEDIUM stg;
	    FORMATETC fmt;

            fmt.cfFormat = 0;
            fmt.ptd = NULL;
            fmt.dwAspect = 0;
            fmt.lindex = -1;
            fmt.tymed = TYMED_ISTREAM;

	    stg.tymed = TYMED_ISTREAM;
	    stg.u.pstm = (IStream *)This->pstrCache;
	    stg.pUnkForRelease = NULL;

            hr = IBindStatusCallback_OnDataAvailable(This->pbscb,
			    			     (This->total_read == written) ?
							 BSCF_FIRSTDATANOTIFICATION :
						         BSCF_INTERMEDIATEDATANOTIFICATION,
						     This->total_read + written,
                                                     &fmt,
						     &stg);
	}
	if (written < dwBytes)
	    return STG_E_MEDIUMFULL;
	else
	    return hr;
    }
    return HRESULT_FROM_WIN32(GetLastError());
}

static void Binding_FinishedDownload(Binding *This, HRESULT hr)
{
    STGMEDIUM stg;
    FORMATETC fmt;

    fmt.ptd = NULL;
    fmt.dwAspect = 0;
    fmt.lindex = -1;
    fmt.tymed = TYMED_ISTREAM;

    stg.tymed = TYMED_ISTREAM;
    stg.u.pstm = (IStream *)This->pstrCache;
    stg.pUnkForRelease = NULL;

    IBindStatusCallback_OnProgress(This->pbscb, This->total_read, This->expected_size,
                                   BINDSTATUS_ENDDOWNLOADDATA, This->URLName);
    IBindStatusCallback_OnDataAvailable(This->pbscb, BSCF_LASTDATANOTIFICATION, This->total_read, &fmt, &stg);
    if (hr)
    {
	WCHAR *pwchError = 0;

        FormatMessageW (FORMAT_MESSAGE_FROM_SYSTEM |
	                 FORMAT_MESSAGE_ALLOCATE_BUFFER,
                        NULL, (DWORD) hr,
		        0, (LPWSTR) &pwchError,
		        0, NULL);
	if (!pwchError)
	{
	    static const WCHAR achFormat[] = { '%', '0', '8', 'x', 0 };

	    pwchError =(WCHAR *) LocalAlloc(LMEM_FIXED, sizeof(WCHAR) * 9);
	    wsprintfW(pwchError, achFormat, hr);
	}
        IBindStatusCallback_OnStopBinding(This->pbscb, hr, pwchError);
	LocalFree((HLOCAL)pwchError);
    }
    else
    {
        IBindStatusCallback_OnStopBinding(This->pbscb, hr, NULL);
    }
    IBindStatusCallback_Release(This->pbscb);
    This->pbscb = 0;
}

static const IBindingVtbl BindingVtbl =
{
    Binding_QueryInterface,
    Binding_AddRef,
    Binding_Release,
    Binding_Abort,
    Binding_Suspend,
    Binding_Resume,
    Binding_SetPriority,
    Binding_GetPriority,
    Binding_GetBindResult
};

/* filemoniker data structure */
typedef struct {

    const IMonikerVtbl* lpvtbl;  /* VTable relative to the IMoniker interface.*/

    LONG ref; /* reference counter for this object */

    LPOLESTR URLName; /* URL string identified by this URLmoniker */
} URLMonikerImpl;

/*******************************************************************************
 *        URLMoniker_QueryInterface
 *******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_QueryInterface(IMoniker* iface,REFIID riid,void** ppvObject)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;

    TRACE("(%p)->(%s,%p)\n",This,debugstr_guid(riid),ppvObject);

    /* Perform a sanity check on the parameters.*/
    if ( (This==0) || (ppvObject==0) )
	return E_INVALIDARG;

    /* Initialize the return parameter */
    *ppvObject = 0;

    /* Compare the riid with the interface IDs implemented by this object.*/
    if (IsEqualIID(&IID_IUnknown, riid)      ||
        IsEqualIID(&IID_IPersist, riid)      ||
        IsEqualIID(&IID_IPersistStream,riid) ||
        IsEqualIID(&IID_IMoniker, riid)
       )
        *ppvObject = iface;

    /* Check that we obtained an interface.*/
    if ((*ppvObject)==0)
        return E_NOINTERFACE;

    /* Query Interface always increases the reference count by one when it is successful */
    IMoniker_AddRef(iface);

    return S_OK;
}

/******************************************************************************
 *        URLMoniker_AddRef
 ******************************************************************************/
static ULONG WINAPI URLMonikerImpl_AddRef(IMoniker* iface)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%u\n",This, refCount);

    return refCount;
}

/******************************************************************************
 *        URLMoniker_Release
 ******************************************************************************/
static ULONG WINAPI URLMonikerImpl_Release(IMoniker* iface)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%u\n",This, refCount);

    /* destroy the object if there's no more reference on it */
    if (!refCount) {
        heap_free(This->URLName);
        heap_free(This);

        URLMON_UnlockModule();
    }

    return refCount;
}


/******************************************************************************
 *        URLMoniker_GetClassID
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_GetClassID(IMoniker* iface,
						CLSID *pClassID)/* Pointer to CLSID of object */
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;

    TRACE("(%p,%p)\n",This,pClassID);

    if (pClassID==NULL)
        return E_POINTER;
    /* Windows always returns CLSID_StdURLMoniker */
    *pClassID = CLSID_StdURLMoniker;
    return S_OK;
}

/******************************************************************************
 *        URLMoniker_IsDirty
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_IsDirty(IMoniker* iface)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    /* Note that the OLE-provided implementations of the IPersistStream::IsDirty
       method in the OLE-provided moniker interfaces always return S_FALSE because
       their internal state never changes. */

    TRACE("(%p)\n",This);

    return S_FALSE;
}

/******************************************************************************
 *        URLMoniker_Load
 *
 * NOTE
 *  Writes a ULONG containing length of unicode string, followed
 *  by that many unicode characters
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_Load(IMoniker* iface,IStream* pStm)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    
    HRESULT res;
    ULONG size;
    ULONG got;
    TRACE("(%p,%p)\n",This,pStm);

    if(!pStm)
        return E_INVALIDARG;

    res = IStream_Read(pStm, &size, sizeof(ULONG), &got);
    if(SUCCEEDED(res)) {
        if(got == sizeof(ULONG)) {
            heap_free(This->URLName);
            This->URLName = heap_alloc(size);
            if(!This->URLName)
                res = E_OUTOFMEMORY;
            else {
                res = IStream_Read(pStm, This->URLName, size, NULL);
                This->URLName[size/sizeof(WCHAR) - 1] = 0;
            }
        }
        else
            res = E_FAIL;
    }
    return res;
}

/******************************************************************************
 *        URLMoniker_Save
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_Save(IMoniker* iface,
					  IStream* pStm,/* pointer to the stream where the object is to be saved */
					  BOOL fClearDirty)/* Specifies whether to clear the dirty flag */
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;

    HRESULT res;
    ULONG size;
    TRACE("(%p,%p,%d)\n",This,pStm,fClearDirty);

    if(!pStm)
        return E_INVALIDARG;

    size = (strlenW(This->URLName) + 1)*sizeof(WCHAR);
    res=IStream_Write(pStm,&size,sizeof(ULONG),NULL);
    if(SUCCEEDED(res))
        res=IStream_Write(pStm,This->URLName,size,NULL);
    return res;

}

/******************************************************************************
 *        URLMoniker_GetSizeMax
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_GetSizeMax(IMoniker* iface,
						ULARGE_INTEGER* pcbSize)/* Pointer to size of stream needed to save object */
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;

    TRACE("(%p,%p)\n",This,pcbSize);

    if(!pcbSize)
        return E_INVALIDARG;

    pcbSize->QuadPart = sizeof(ULONG) + ((strlenW(This->URLName)+1) * sizeof(WCHAR));
    return S_OK;
}

/******************************************************************************
 *                  URLMoniker_BindToObject
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_BindToObject(IMoniker* iface,
						  IBindCtx* pbc,
						  IMoniker* pmkToLeft,
						  REFIID riid,
						  VOID** ppv)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    IRunningObjectTable *obj_tbl;
    HRESULT hres;

    TRACE("(%p)->(%p,%p,%s,%p): stub\n", This, pbc, pmkToLeft, debugstr_guid(riid), ppv);

    hres = IBindCtx_GetRunningObjectTable(pbc, &obj_tbl);
    if(SUCCEEDED(hres)) {
        FIXME("use running object table\n");
        IRunningObjectTable_Release(obj_tbl);
    }

    return bind_to_object(iface, This->URLName, pbc, riid, ppv);
}

/******************************************************************************
 *        URLMoniker_BindToStorage
 ******************************************************************************/
static HRESULT URLMonikerImpl_BindToStorage_hack(LPCWSTR URLName, IBindCtx* pbc, VOID** ppvObject)
{
    HRESULT hres;
    BINDINFO bi;
    DWORD bindf;
    WCHAR szFileName[MAX_PATH + 1];
    Binding *bind;
    int len;

    WARN("(%s %p %p)\n", debugstr_w(URLName), pbc, ppvObject);

    bind = heap_alloc_zero(sizeof(Binding));
    bind->lpVtbl = &BindingVtbl;
    bind->ref = 1;
    URLMON_LockModule();

    len = lstrlenW(URLName)+1;
    bind->URLName = heap_alloc(len*sizeof(WCHAR));
    memcpy(bind->URLName, URLName, len*sizeof(WCHAR));

    hres = UMCreateStreamOnCacheFile(bind->URLName, 0, szFileName, &bind->hCacheFile, &bind->pstrCache);

    if(SUCCEEDED(hres)) {
        TRACE("Created stream...\n");

        *ppvObject = (void *) bind->pstrCache;
        IStream_AddRef((IStream *) bind->pstrCache);

        hres = IBindCtx_GetObjectParam(pbc, BSCBHolder, (IUnknown**)&bind->pbscb);
        if(SUCCEEDED(hres)) {
            TRACE("Got IBindStatusCallback...\n");

            memset(&bi, 0, sizeof(bi));
            bi.cbSize = sizeof(bi);
            bindf = 0;
            hres = IBindStatusCallback_GetBindInfo(bind->pbscb, &bindf, &bi);
            if(SUCCEEDED(hres)) {
                URL_COMPONENTSW url;
                WCHAR *host, *path, *user, *pass;
                DWORD lensz = sizeof(bind->expected_size);
                DWORD dwService = 0;
                BOOL bSuccess;

                TRACE("got bindinfo. bindf = %08x extrainfo = %s bindinfof = %08x bindverb = %08x iid %s\n",
                      bindf, debugstr_w(bi.szExtraInfo), bi.grfBindInfoF, bi.dwBindVerb, debugstr_guid(&bi.iid));
                hres = IBindStatusCallback_OnStartBinding(bind->pbscb, 0, (IBinding*)bind);
                TRACE("OnStartBinding rets %08x\n", hres);

                bind->expected_size = 0;
                bind->total_read = 0;

                memset(&url, 0, sizeof(url));
                url.dwStructSize = sizeof(url);
                url.dwSchemeLength = url.dwHostNameLength = url.dwUrlPathLength = url.dwUserNameLength = url.dwPasswordLength = 1;
                InternetCrackUrlW(URLName, 0, ICU_ESCAPE, &url);
                host = heap_alloc((url.dwHostNameLength + 1) * sizeof(WCHAR));
                memcpy(host, url.lpszHostName, url.dwHostNameLength * sizeof(WCHAR));
                host[url.dwHostNameLength] = '\0';
                path = heap_alloc((url.dwUrlPathLength + 1) * sizeof(WCHAR));
                memcpy(path, url.lpszUrlPath, url.dwUrlPathLength * sizeof(WCHAR));
                path[url.dwUrlPathLength] = '\0';
                if (url.dwUserNameLength)
                {
                    user = heap_alloc(((url.dwUserNameLength + 1) * sizeof(WCHAR)));
                    memcpy(user, url.lpszUserName, url.dwUserNameLength * sizeof(WCHAR));
                    user[url.dwUserNameLength] = 0;
                }
                else
                {
                    user = 0;
                }
                if (url.dwPasswordLength)
                {
                    pass = heap_alloc(((url.dwPasswordLength + 1) * sizeof(WCHAR)));
                    memcpy(pass, url.lpszPassword, url.dwPasswordLength * sizeof(WCHAR));
                    pass[url.dwPasswordLength] = 0;
                }
                else
                {
                    pass = 0;
                }


                do {
                    bind->hinternet = InternetOpenA("User Agent", 0, NULL, NULL, 0);
                    if (!bind->hinternet)
                    {
                            hres = HRESULT_FROM_WIN32(GetLastError());
                            break;
                    }

                    switch ((DWORD) url.nScheme)
                    {
                    case INTERNET_SCHEME_FTP:
                        if (!url.nPort)
                            url.nPort = INTERNET_DEFAULT_FTP_PORT;
                        dwService = INTERNET_SERVICE_FTP;
                        break;
    
                    case INTERNET_SCHEME_GOPHER:
                        if (!url.nPort)
                            url.nPort = INTERNET_DEFAULT_GOPHER_PORT;
                        dwService = INTERNET_SERVICE_GOPHER;
                        break;

                    case INTERNET_SCHEME_HTTPS:
                        if (!url.nPort)
                            url.nPort = INTERNET_DEFAULT_HTTPS_PORT;
                        dwService = INTERNET_SERVICE_HTTP;
                        break;
                    }

                    bind->hconnect = InternetConnectW(bind->hinternet, host, url.nPort, user, pass,
                                                      dwService, 0, (DWORD)bind);
                    if (!bind->hconnect)
                    {
                            hres = HRESULT_FROM_WIN32(GetLastError());
                            CloseHandle((HANDLE)bind->hinternet);
                            break;
                    }

                    hres = IBindStatusCallback_OnProgress(bind->pbscb, 0, 0, 0x22, NULL);
                    hres = IBindStatusCallback_OnProgress(bind->pbscb, 0, 0, BINDSTATUS_FINDINGRESOURCE, NULL);
                    hres = IBindStatusCallback_OnProgress(bind->pbscb, 0, 0, BINDSTATUS_CONNECTING, NULL);
                    hres = IBindStatusCallback_OnProgress(bind->pbscb, 0, 0, BINDSTATUS_SENDINGREQUEST, NULL);

                    bSuccess = FALSE;

                    switch (dwService)
                    {
                    case INTERNET_SERVICE_GOPHER:
                        bind->hrequest = GopherOpenFileW(bind->hconnect,
                                                         path,
                                                         0,
                                                         INTERNET_FLAG_RELOAD,
                                                         0);
                        if (bind->hrequest)
                                bSuccess = TRUE;
                        else
                                hres = HRESULT_FROM_WIN32(GetLastError());
                        break;

                    case INTERNET_SERVICE_FTP:
                        bind->hrequest = FtpOpenFileW(bind->hconnect,
                                                      path,
                                                      GENERIC_READ,
                                                      FTP_TRANSFER_TYPE_BINARY |
                                                       INTERNET_FLAG_TRANSFER_BINARY |
                                                       INTERNET_FLAG_RELOAD,
                                                      0);
                        if (bind->hrequest)
                                bSuccess = TRUE;
                        else
                                hres = HRESULT_FROM_WIN32(GetLastError());
                        break;

                    case INTERNET_SERVICE_HTTP:
                        bind->hrequest = HttpOpenRequestW(bind->hconnect, NULL, path, NULL, NULL, NULL, 0, (DWORD)bind);
                        if (!bind->hrequest)
                        {
                                hres = HRESULT_FROM_WIN32(GetLastError());
                        }
                        else if (!HttpSendRequestW(bind->hrequest, NULL, 0, NULL, 0))
                        {
                                hres = HRESULT_FROM_WIN32(GetLastError());
                                InternetCloseHandle(bind->hrequest);
                        }
                        else
                        {
                                HttpQueryInfoW(bind->hrequest,
                                               HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                                               &bind->expected_size,
                                               &lensz,
                                               NULL);
                                bSuccess = TRUE;
                        }
                        break;
                    }
                    if(bSuccess)
                    {
                        TRACE("res = %d gle = %u url len = %d\n", hres, GetLastError(), bind->expected_size);

                        IBindStatusCallback_OnProgress(bind->pbscb, 0, 0, BINDSTATUS_CACHEFILENAMEAVAILABLE, szFileName);

                        while(1) {
                            char buf[4096];
                            DWORD bufread;
                            if(InternetReadFile(bind->hrequest, buf, sizeof(buf), &bufread)) {
                                TRACE("read %d bytes %s...\n", bufread, debugstr_an(buf, 10));
                                if(bufread == 0) break;
                                hres = Binding_MoreCacheData(bind, buf, bufread);
                            } else
                                break;
                        }
                        InternetCloseHandle(bind->hrequest);
                            hres = S_OK;
                    }
            
                    InternetCloseHandle(bind->hconnect);
                    InternetCloseHandle(bind->hinternet);
                } while(0);

                Binding_FinishedDownload(bind, hres);
                Binding_CloseCacheDownload(bind);

                heap_free(user);
                heap_free(pass);
                heap_free(path);
                heap_free(host);
            }
        }
    }

    IBinding_Release((IBinding*)bind);

    return hres;
}

static HRESULT WINAPI URLMonikerImpl_BindToStorage(IMoniker* iface,
                                                   IBindCtx* pbc,
						   IMoniker* pmkToLeft,
						   REFIID riid,
						   VOID** ppvObject)
{
    URLMonikerImpl *This = (URLMonikerImpl*)iface;
    WCHAR schema[64];
    BOOL bret;

    URL_COMPONENTSW url = {sizeof(URL_COMPONENTSW), schema,
        sizeof(schema)/sizeof(WCHAR), 0, NULL, 0, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0};

    if(pmkToLeft)
        FIXME("Unsupported pmkToLeft\n");

    bret = InternetCrackUrlW(This->URLName, 0, ICU_ESCAPE, &url);
    if(!bret) {
        ERR("InternetCrackUrl failed: %u\n", GetLastError());
        return E_FAIL;
    }

    if(IsEqualGUID(&IID_IStream, riid) &&
       (  url.nScheme == INTERNET_SCHEME_HTTPS
       || url.nScheme == INTERNET_SCHEME_FTP
       || url.nScheme == INTERNET_SCHEME_GOPHER))
        return URLMonikerImpl_BindToStorage_hack(This->URLName, pbc, ppvObject);

    TRACE("(%p)->(%p %p %s %p)\n", This, pbc, pmkToLeft, debugstr_guid(riid), ppvObject);

    return bind_to_storage(This->URLName, pbc, riid, ppvObject);
}

/******************************************************************************
 *        URLMoniker_Reduce
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_Reduce(IMoniker* iface,
					    IBindCtx* pbc,
					    DWORD dwReduceHowFar,
					    IMoniker** ppmkToLeft,
					    IMoniker** ppmkReduced)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    
    TRACE("(%p,%p,%d,%p,%p)\n",This,pbc,dwReduceHowFar,ppmkToLeft,ppmkReduced);

    if(!ppmkReduced)
        return E_INVALIDARG;

    URLMonikerImpl_AddRef(iface);
    *ppmkReduced = iface;
    return MK_S_REDUCED_TO_SELF;
}

/******************************************************************************
 *        URLMoniker_ComposeWith
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_ComposeWith(IMoniker* iface,
						 IMoniker* pmkRight,
						 BOOL fOnlyIfNotGeneric,
						 IMoniker** ppmkComposite)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    FIXME("(%p)->(%p,%d,%p): stub\n",This,pmkRight,fOnlyIfNotGeneric,ppmkComposite);

    return E_NOTIMPL;
}

/******************************************************************************
 *        URLMoniker_Enum
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_Enum(IMoniker* iface,BOOL fForward, IEnumMoniker** ppenumMoniker)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    TRACE("(%p,%d,%p)\n",This,fForward,ppenumMoniker);

    if(!ppenumMoniker)
        return E_INVALIDARG;

    /* Does not support sub-monikers */
    *ppenumMoniker = NULL;
    return S_OK;
}

/******************************************************************************
 *        URLMoniker_IsEqual
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_IsEqual(IMoniker* iface,IMoniker* pmkOtherMoniker)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    CLSID clsid;
    LPOLESTR urlPath;
    IBindCtx* bind;
    HRESULT res;

    TRACE("(%p,%p)\n",This,pmkOtherMoniker);

    if(pmkOtherMoniker==NULL)
        return E_INVALIDARG;

    IMoniker_GetClassID(pmkOtherMoniker,&clsid);

    if(!IsEqualCLSID(&clsid,&CLSID_StdURLMoniker))
        return S_FALSE;

    res = CreateBindCtx(0,&bind);
    if(FAILED(res))
        return res;

    res = S_FALSE;
    if(SUCCEEDED(IMoniker_GetDisplayName(pmkOtherMoniker,bind,NULL,&urlPath))) {
        int result = lstrcmpiW(urlPath, This->URLName);
        CoTaskMemFree(urlPath);
        if(result == 0)
            res = S_OK;
    }
    IUnknown_Release(bind);
    return res;
}


/******************************************************************************
 *        URLMoniker_Hash
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_Hash(IMoniker* iface,DWORD* pdwHash)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    
    int  h = 0,i,skip,len;
    int  off = 0;
    LPOLESTR val;

    TRACE("(%p,%p)\n",This,pdwHash);

    if(!pdwHash)
        return E_INVALIDARG;

    val = This->URLName;
    len = lstrlenW(val);

    if(len < 16) {
        for(i = len ; i > 0; i--) {
            h = (h * 37) + val[off++];
        }
    }
    else {
        /* only sample some characters */
        skip = len / 8;
        for(i = len; i > 0; i -= skip, off += skip) {
            h = (h * 39) + val[off];
        }
    }
    *pdwHash = h;
    return S_OK;
}

/******************************************************************************
 *        URLMoniker_IsRunning
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_IsRunning(IMoniker* iface,
					       IBindCtx* pbc,
					       IMoniker* pmkToLeft,
					       IMoniker* pmkNewlyRunning)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    FIXME("(%p)->(%p,%p,%p): stub\n",This,pbc,pmkToLeft,pmkNewlyRunning);

    return E_NOTIMPL;
}

/******************************************************************************
 *        URLMoniker_GetTimeOfLastChange
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_GetTimeOfLastChange(IMoniker* iface,
							 IBindCtx* pbc,
							 IMoniker* pmkToLeft,
							 FILETIME* pFileTime)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    FIXME("(%p)->(%p,%p,%p): stub\n",This,pbc,pmkToLeft,pFileTime);

    return E_NOTIMPL;
}

/******************************************************************************
 *        URLMoniker_Inverse
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_Inverse(IMoniker* iface,IMoniker** ppmk)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    TRACE("(%p,%p)\n",This,ppmk);

    return MK_E_NOINVERSE;
}

/******************************************************************************
 *        URLMoniker_CommonPrefixWith
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_CommonPrefixWith(IMoniker* iface,IMoniker* pmkOther,IMoniker** ppmkPrefix)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    FIXME("(%p)->(%p,%p): stub\n",This,pmkOther,ppmkPrefix);

    return E_NOTIMPL;
}

/******************************************************************************
 *        URLMoniker_RelativePathTo
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_RelativePathTo(IMoniker* iface,IMoniker* pmOther, IMoniker** ppmkRelPath)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    FIXME("(%p)->(%p,%p): stub\n",This,pmOther,ppmkRelPath);

    return E_NOTIMPL;
}

/******************************************************************************
 *        URLMoniker_GetDisplayName
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_GetDisplayName(IMoniker* iface,
						    IBindCtx* pbc,
						    IMoniker* pmkToLeft,
						    LPOLESTR *ppszDisplayName)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    
    int len;
    
    TRACE("(%p,%p,%p,%p)\n",This,pbc,pmkToLeft,ppszDisplayName);
    
    if(!ppszDisplayName)
        return E_INVALIDARG;
    
    /* FIXME: If this is a partial URL, try and get a URL moniker from SZ_URLCONTEXT in the bind context,
        then look at pmkToLeft to try and complete the URL
    */
    len = lstrlenW(This->URLName)+1;
    *ppszDisplayName = CoTaskMemAlloc(len*sizeof(WCHAR));
    if(!*ppszDisplayName)
        return E_OUTOFMEMORY;
    lstrcpyW(*ppszDisplayName, This->URLName);
    return S_OK;
}

/******************************************************************************
 *        URLMoniker_ParseDisplayName
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_ParseDisplayName(IMoniker* iface,
						      IBindCtx* pbc,
						      IMoniker* pmkToLeft,
						      LPOLESTR pszDisplayName,
						      ULONG* pchEaten,
						      IMoniker** ppmkOut)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    FIXME("(%p)->(%p,%p,%p,%p,%p): stub\n",This,pbc,pmkToLeft,pszDisplayName,pchEaten,ppmkOut);

    return E_NOTIMPL;
}

/******************************************************************************
 *        URLMoniker_IsSystemMoniker
 ******************************************************************************/
static HRESULT WINAPI URLMonikerImpl_IsSystemMoniker(IMoniker* iface,DWORD* pwdMksys)
{
    URLMonikerImpl *This = (URLMonikerImpl *)iface;
    TRACE("(%p,%p)\n",This,pwdMksys);

    if(!pwdMksys)
        return E_INVALIDARG;

    *pwdMksys = MKSYS_URLMONIKER;
    return S_OK;
}

/********************************************************************************/
/* Virtual function table for the URLMonikerImpl class which  include IPersist,*/
/* IPersistStream and IMoniker functions.                                       */
static const IMonikerVtbl VT_URLMonikerImpl =
{
    URLMonikerImpl_QueryInterface,
    URLMonikerImpl_AddRef,
    URLMonikerImpl_Release,
    URLMonikerImpl_GetClassID,
    URLMonikerImpl_IsDirty,
    URLMonikerImpl_Load,
    URLMonikerImpl_Save,
    URLMonikerImpl_GetSizeMax,
    URLMonikerImpl_BindToObject,
    URLMonikerImpl_BindToStorage,
    URLMonikerImpl_Reduce,
    URLMonikerImpl_ComposeWith,
    URLMonikerImpl_Enum,
    URLMonikerImpl_IsEqual,
    URLMonikerImpl_Hash,
    URLMonikerImpl_IsRunning,
    URLMonikerImpl_GetTimeOfLastChange,
    URLMonikerImpl_Inverse,
    URLMonikerImpl_CommonPrefixWith,
    URLMonikerImpl_RelativePathTo,
    URLMonikerImpl_GetDisplayName,
    URLMonikerImpl_ParseDisplayName,
    URLMonikerImpl_IsSystemMoniker
};

/******************************************************************************
 *         URLMoniker_Construct (local function)
 *******************************************************************************/
static HRESULT URLMonikerImpl_Construct(URLMonikerImpl* This, LPCOLESTR lpszLeftURLName, LPCOLESTR lpszURLName)
{
    HRESULT hres;
    DWORD sizeStr = 0;

    TRACE("(%p,%s,%s)\n",This,debugstr_w(lpszLeftURLName),debugstr_w(lpszURLName));

    This->lpvtbl = &VT_URLMonikerImpl;
    This->ref = 0;

    This->URLName = heap_alloc(INTERNET_MAX_URL_LENGTH*sizeof(WCHAR));

    if(lpszLeftURLName)
        hres = CoInternetCombineUrl(lpszLeftURLName, lpszURLName, URL_FILE_USE_PATHURL,
                This->URLName, INTERNET_MAX_URL_LENGTH, &sizeStr, 0);
    else
        hres = CoInternetParseUrl(lpszURLName, PARSE_CANONICALIZE, URL_FILE_USE_PATHURL,
                This->URLName, INTERNET_MAX_URL_LENGTH, &sizeStr, 0);

    if(FAILED(hres)) {
        heap_free(This->URLName);
        return hres;
    }

    URLMON_LockModule();

    if(sizeStr != INTERNET_MAX_URL_LENGTH)
        This->URLName = heap_realloc(This->URLName, (sizeStr+1)*sizeof(WCHAR));

    TRACE("URLName = %s\n", debugstr_w(This->URLName));

    return S_OK;
}

/***********************************************************************
 *           CreateURLMonikerEx (URLMON.@)
 *
 * Create a url moniker.
 *
 * PARAMS
 *    pmkContext [I] Context
 *    szURL      [I] Url to create the moniker for
 *    ppmk       [O] Destination for created moniker.
 *    dwFlags    [I] Flags.
 *
 * RETURNS
 *    Success: S_OK. ppmk contains the created IMoniker object.
 *    Failure: MK_E_SYNTAX if szURL is not a valid url, or
 *             E_OUTOFMEMORY if memory allocation fails.
 */
HRESULT WINAPI CreateURLMonikerEx(IMoniker *pmkContext, LPCWSTR szURL, IMoniker **ppmk, DWORD dwFlags)
{
    URLMonikerImpl *obj;
    HRESULT hres;
    LPOLESTR lefturl = NULL;

    TRACE("(%p, %s, %p, %08x)\n", pmkContext, debugstr_w(szURL), ppmk, dwFlags);

    if (dwFlags & URL_MK_UNIFORM) FIXME("ignoring flag URL_MK_UNIFORM\n");

    if(!(obj = heap_alloc(sizeof(*obj))))
	return E_OUTOFMEMORY;

    if(pmkContext) {
        IBindCtx* bind;
        DWORD dwMksys = 0;
        IMoniker_IsSystemMoniker(pmkContext, &dwMksys);
        if(dwMksys == MKSYS_URLMONIKER && SUCCEEDED(CreateBindCtx(0, &bind))) {
            IMoniker_GetDisplayName(pmkContext, bind, NULL, &lefturl);
            TRACE("lefturl = %s\n", debugstr_w(lefturl));
            IBindCtx_Release(bind);
        }
    }
        
    hres = URLMonikerImpl_Construct(obj, lefturl, szURL);
    CoTaskMemFree(lefturl);
    if(SUCCEEDED(hres))
	hres = URLMonikerImpl_QueryInterface((IMoniker*)obj, &IID_IMoniker, (void**)ppmk);
    else
	heap_free(obj);
    return hres;
}

/**********************************************************************
 *           CreateURLMoniker (URLMON.@)
 *
 * Create a url moniker.
 *
 * PARAMS
 *    pmkContext [I] Context
 *    szURL      [I] Url to create the moniker for
 *    ppmk       [O] Destination for created moniker.
 *
 * RETURNS
 *    Success: S_OK. ppmk contains the created IMoniker object.
 *    Failure: MK_E_SYNTAX if szURL is not a valid url, or
 *             E_OUTOFMEMORY if memory allocation fails.
 */
HRESULT WINAPI CreateURLMoniker(IMoniker *pmkContext, LPCWSTR szURL, IMoniker **ppmk)
{
    return CreateURLMonikerEx(pmkContext, szURL, ppmk, URL_MK_LEGACY);
}

/***********************************************************************
 *           IsAsyncMoniker (URLMON.@)
 */
HRESULT WINAPI IsAsyncMoniker(IMoniker *pmk)
{
    IUnknown *am;
    
    TRACE("(%p)\n", pmk);
    if(!pmk)
        return E_INVALIDARG;
    if(SUCCEEDED(IMoniker_QueryInterface(pmk, &IID_IAsyncMoniker, (void**)&am))) {
        IUnknown_Release(am);
        return S_OK;
    }
    return S_FALSE;
}

/***********************************************************************
 *           BindAsyncMoniker (URLMON.@)
 *
 * Bind a bind status callback to an asynchronous URL Moniker.
 *
 * PARAMS
 *  pmk           [I] Moniker object to bind status callback to
 *  grfOpt        [I] Options, seems not used
 *  pbsc          [I] Status callback to bind
 *  iidResult     [I] Interface to return
 *  ppvResult     [O] Resulting asynchronous moniker object
 *
 * RETURNS
 *    Success: S_OK.
 *    Failure: E_INVALIDARG, if any argument is invalid, or
 *             E_OUTOFMEMORY if memory allocation fails.
 */
HRESULT WINAPI BindAsyncMoniker(IMoniker *pmk, DWORD grfOpt, IBindStatusCallback *pbsc, REFIID iidResult, LPVOID *ppvResult)
{
    LPBC pbc = NULL;
    HRESULT hr = E_INVALIDARG;

    TRACE("(%p %08x %p %s %p)\n", pmk, grfOpt, pbsc, debugstr_guid(iidResult), ppvResult);

    if (pmk && ppvResult)
    {
        *ppvResult = NULL;

        hr = CreateAsyncBindCtx(0, pbsc, NULL, &pbc);
        if (hr == NOERROR)
        {
            hr = IMoniker_BindToObject(pmk, pbc, NULL, iidResult, ppvResult);
            IBindCtx_Release(pbc);
        }
    }
    return hr;
}

/***********************************************************************
 *           MkParseDisplayNameEx (URLMON.@)
 */
HRESULT WINAPI MkParseDisplayNameEx(IBindCtx *pbc, LPCWSTR szDisplayName, ULONG *pchEaten, LPMONIKER *ppmk)
{
    TRACE("(%p %s %p %p)\n", pbc, debugstr_w(szDisplayName), pchEaten, ppmk);

    if(is_registered_protocol(szDisplayName)) {
        HRESULT hres;

        hres = CreateURLMoniker(NULL, szDisplayName, ppmk);
        if(SUCCEEDED(hres)) {
            *pchEaten = strlenW(szDisplayName);
            return hres;
        }
    }

    return MkParseDisplayName(pbc, szDisplayName, pchEaten, ppmk);
}


/***********************************************************************
 *           URLDownloadToCacheFileA (URLMON.@)
 */
HRESULT WINAPI URLDownloadToCacheFileA(LPUNKNOWN lpUnkCaller, LPCSTR szURL, LPSTR szFileName,
        DWORD dwBufLength, DWORD dwReserved, LPBINDSTATUSCALLBACK pBSC)
{
    LPWSTR url = NULL, file_name = NULL;
    int len;
    HRESULT hres;

    TRACE("(%p %s %p %d %d %p)\n", lpUnkCaller, debugstr_a(szURL), szFileName,
            dwBufLength, dwReserved, pBSC);

    if(szURL) {
        len = MultiByteToWideChar(CP_ACP, 0, szURL, -1, NULL, 0);
        url = heap_alloc(len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, szURL, -1, url, -1);
    }

    if(szFileName)
        file_name = heap_alloc(dwBufLength*sizeof(WCHAR));

    hres = URLDownloadToCacheFileW(lpUnkCaller, url, file_name, dwBufLength*sizeof(WCHAR),
            dwReserved, pBSC);

    if(SUCCEEDED(hres) && file_name)
        WideCharToMultiByte(CP_ACP, 0, file_name, -1, szFileName, dwBufLength, NULL, NULL);

    heap_free(url);
    heap_free(file_name);

    return hres;
}

/***********************************************************************
 *           URLDownloadToCacheFileW (URLMON.@)
 */
HRESULT WINAPI URLDownloadToCacheFileW(LPUNKNOWN lpUnkCaller, LPCWSTR szURL, LPWSTR szFileName,
                DWORD dwBufLength, DWORD dwReserved, LPBINDSTATUSCALLBACK pBSC)
{
    WCHAR cache_path[MAX_PATH + 1];
    FILETIME expire, modified;
    HRESULT hr;
    LPWSTR ext;

    static WCHAR header[] = {
        'H','T','T','P','/','1','.','0',' ','2','0','0',' ',
        'O','K','\\','r','\\','n','\\','r','\\','n',0
    };

    TRACE("(%p, %s, %p, %d, %d, %p)\n", lpUnkCaller, debugstr_w(szURL),
          szFileName, dwBufLength, dwReserved, pBSC);

    if (!szURL || !szFileName)
        return E_INVALIDARG;

    ext = PathFindExtensionW(szURL);

    if (!CreateUrlCacheEntryW(szURL, 0, ext, cache_path, 0))
        return E_FAIL;

    hr = URLDownloadToFileW(lpUnkCaller, szURL, cache_path, 0, pBSC);
    if (FAILED(hr))
        return hr;

    expire.dwHighDateTime = 0;
    expire.dwLowDateTime = 0;
    modified.dwHighDateTime = 0;
    modified.dwLowDateTime = 0;

    if (!CommitUrlCacheEntryW(szURL, cache_path, expire, modified, NORMAL_CACHE_ENTRY,
                              header, sizeof(header), NULL, NULL))
        return E_FAIL;

    if (lstrlenW(cache_path) > dwBufLength)
        return E_OUTOFMEMORY;

    lstrcpyW(szFileName, cache_path);

    return S_OK;
}

/***********************************************************************
 *           HlinkSimpleNavigateToString (URLMON.@)
 */
HRESULT WINAPI HlinkSimpleNavigateToString( LPCWSTR szTarget,
    LPCWSTR szLocation, LPCWSTR szTargetFrameName, IUnknown *pUnk,
    IBindCtx *pbc, IBindStatusCallback *pbsc, DWORD grfHLNF, DWORD dwReserved)
{
    FIXME("%s\n", debugstr_w( szTarget ) );
    return E_NOTIMPL;
}

/***********************************************************************
 *           HlinkNavigateString (URLMON.@)
 */
HRESULT WINAPI HlinkNavigateString( IUnknown *pUnk, LPCWSTR szTarget )
{
    TRACE("%p %s\n", pUnk, debugstr_w( szTarget ) );
    return HlinkSimpleNavigateToString( 
               szTarget, NULL, NULL, pUnk, NULL, NULL, 0, 0 );
}

/***********************************************************************
 *           GetSoftwareUpdateInfo (URLMON.@)
 */
HRESULT WINAPI GetSoftwareUpdateInfo( LPCWSTR szDistUnit, LPSOFTDISTINFO psdi )
{
    FIXME("%s %p\n", debugstr_w(szDistUnit), psdi );
    return E_FAIL;
}
