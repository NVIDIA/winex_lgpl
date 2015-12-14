/*
 *	free threaded marshaler
 *
 *  Copyright 2002  Juergen Schmied
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "winbase.h"
#include "objbase.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

typedef struct _FTMarshalImpl {
	ICOM_VFIELD (IUnknown);
	DWORD ref;
	ICOM_VTABLE (IMarshal) * lpvtblFTM;

	IUnknown *pUnkOuter;
} FTMarshalImpl;

#define _IFTMUnknown_(This)(IUnknown*)&(This->lpVtbl)
#define _IFTMarshal_(This) (IMarshal*)&(This->lpvtblFTM)

#define _IFTMarshall_Offset ((int)(&(((FTMarshalImpl*)0)->lpvtblFTM)))
#define _ICOM_THIS_From_IFTMarshal(class, name) class* This = (class*)(((char*)name)-_IFTMarshall_Offset);

/* inner IUnknown to handle aggregation */
HRESULT WINAPI IiFTMUnknown_fnQueryInterface (IUnknown * iface, REFIID riid, LPVOID * ppv)
{

    ICOM_THIS (FTMarshalImpl, iface);

    TRACE ("\n");
    *ppv = NULL;

    if (IsEqualIID (&IID_IUnknown, riid))
	*ppv = _IFTMUnknown_ (This);
    else if (IsEqualIID (&IID_IMarshal, riid))
	*ppv = _IFTMarshal_ (This);
    else {
	FIXME ("No interface for %s.\n", debugstr_guid (riid));
	return E_NOINTERFACE;
    }
    IUnknown_AddRef ((IUnknown *) * ppv);
    return S_OK;
}

ULONG WINAPI IiFTMUnknown_fnAddRef (IUnknown * iface)
{

    ICOM_THIS (FTMarshalImpl, iface);

    TRACE ("\n");
    return InterlockedIncrement (&This->ref);
}

ULONG WINAPI IiFTMUnknown_fnRelease (IUnknown * iface)
{

    ICOM_THIS (FTMarshalImpl, iface);

    TRACE ("\n");
    if (InterlockedDecrement (&This->ref))
	return This->ref;
    HeapFree (GetProcessHeap (), 0, This);
    return 0;
}

static ICOM_VTABLE (IUnknown) iunkvt =
{
        ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IiFTMUnknown_fnQueryInterface,
	IiFTMUnknown_fnAddRef,
	IiFTMUnknown_fnRelease
};

HRESULT WINAPI FTMarshalImpl_QueryInterface (LPMARSHAL iface, REFIID riid, LPVOID * ppv)
{

    _ICOM_THIS_From_IFTMarshal (FTMarshalImpl, iface);

    TRACE ("(%p)->(\n\tIID:\t%s,%p)\n", This, debugstr_guid (riid), ppv);
    return IUnknown_QueryInterface (This->pUnkOuter, riid, ppv);
}

ULONG WINAPI FTMarshalImpl_AddRef (LPMARSHAL iface)
{

    _ICOM_THIS_From_IFTMarshal (FTMarshalImpl, iface);

    TRACE ("\n");
    return IUnknown_AddRef (This->pUnkOuter);
}

ULONG WINAPI FTMarshalImpl_Release (LPMARSHAL iface)
{

    _ICOM_THIS_From_IFTMarshal (FTMarshalImpl, iface);

    TRACE ("\n");
    return IUnknown_Release (This->pUnkOuter);
}

HRESULT WINAPI FTMarshalImpl_GetUnmarshalClass (LPMARSHAL iface, REFIID riid, void *pv, DWORD dwDestContext,
						void *pvDestContext, DWORD mshlflags, CLSID * pCid)
{
    FIXME ("(), stub!\n");
    return S_OK;
}

HRESULT WINAPI FTMarshalImpl_GetMarshalSizeMax (LPMARSHAL iface, REFIID riid, void *pv, DWORD dwDestContext,
						void *pvDestContext, DWORD mshlflags, DWORD * pSize)
{

    IMarshal *pMarshal = NULL;
    HRESULT hres;

    _ICOM_THIS_From_IFTMarshal (FTMarshalImpl, iface);

    FIXME ("(), stub!\n");

    /* if the marshaling happends inside the same process the interface pointer is
       copied between the appartments */
    if (dwDestContext == MSHCTX_INPROC || dwDestContext == MSHCTX_CROSSCTX) {
	*pSize = sizeof (This);
	return S_OK;
    }

    /* use the standard marshaler to handle all other cases */
    CoGetStandardMarshal (riid, pv, dwDestContext, pvDestContext, mshlflags, &pMarshal);
    hres = IMarshal_GetMarshalSizeMax (pMarshal, riid, pv, dwDestContext, pvDestContext, mshlflags, pSize);
    IMarshal_Release (pMarshal);
    return hres;

    return S_OK;
}

HRESULT WINAPI FTMarshalImpl_MarshalInterface (LPMARSHAL iface, IStream * pStm, REFIID riid, void *pv,
					       DWORD dwDestContext, void *pvDestContext, DWORD mshlflags)
{

    IMarshal *pMarshal = NULL;
    HRESULT hres;

    _ICOM_THIS_From_IFTMarshal (FTMarshalImpl, iface);

    FIXME ("(), stub!\n");

    /* if the marshaling happends inside the same process the interface pointer is
       copied between the appartments */
    if (dwDestContext == MSHCTX_INPROC || dwDestContext == MSHCTX_CROSSCTX) {
	return IStream_Write (pStm, This, sizeof (This), 0);
    }

    /* use the standard marshaler to handle all other cases */
    CoGetStandardMarshal (riid, pv, dwDestContext, pvDestContext, mshlflags, &pMarshal);
    hres = IMarshal_MarshalInterface (pMarshal, pStm, riid, pv, dwDestContext, pvDestContext, mshlflags);
    IMarshal_Release (pMarshal);
    return hres;
}

HRESULT WINAPI FTMarshalImpl_UnmarshalInterface (LPMARSHAL iface, IStream * pStm, REFIID riid, void **ppv)
{
    FIXME ("(), stub!\n");
    return S_OK;
}

HRESULT WINAPI FTMarshalImpl_ReleaseMarshalData (LPMARSHAL iface, IStream * pStm)
{
    FIXME ("(), stub!\n");
    return S_OK;
}

HRESULT WINAPI FTMarshalImpl_DisconnectObject (LPMARSHAL iface, DWORD dwReserved)
{
    FIXME ("(), stub!\n");
    return S_OK;
}

ICOM_VTABLE (IMarshal) ftmvtbl =
{
        ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	FTMarshalImpl_QueryInterface,
	FTMarshalImpl_AddRef,
	FTMarshalImpl_Release,
	FTMarshalImpl_GetUnmarshalClass,
	FTMarshalImpl_GetMarshalSizeMax,
	FTMarshalImpl_MarshalInterface,
	FTMarshalImpl_UnmarshalInterface,
	FTMarshalImpl_ReleaseMarshalData,
	FTMarshalImpl_DisconnectObject
};

/***********************************************************************
 *          CoCreateFreeThreadedMarshaler [OLE32.5]
 *
 */
HRESULT WINAPI CoCreateFreeThreadedMarshaler (LPUNKNOWN punkOuter, LPUNKNOWN * ppunkMarshal)
{

    FTMarshalImpl *ftm;

    TRACE ("(%p %p)\n", punkOuter, ppunkMarshal);

    ftm = (FTMarshalImpl *) HeapAlloc (GetProcessHeap (), 0, sizeof (FTMarshalImpl));
    if (!ftm)
	return E_OUTOFMEMORY;

    ICOM_VTBL (ftm) = &iunkvt;
    ftm->lpvtblFTM = &ftmvtbl;
    ftm->ref = 1;
    ftm->pUnkOuter = punkOuter;

    *ppunkMarshal = _IFTMUnknown_ (ftm);
    return S_OK;
}
