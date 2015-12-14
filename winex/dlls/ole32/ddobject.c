
/*
 *	Drag & Drop Target-only IDataObject implementation
 *
 *  Copyright 2008 TransGaming Inc.
 */

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ddobject.h"

#include "wine/winbase16.h"
#include "wine/wingdi16.h"
#include "wine/winuser16.h"
#include "ole32_main.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(oledd);


/******************************************************************************
 * Drag & Drop Target IDataObject container
 */
typedef struct
{
    /* IUnknown fields */
    ICOM_VFIELD(IDataObject);
    DWORD		ref;

    /* IDataObject fields */
    FORMATETC * pFormatEtc;
    STGMEDIUM * pStgMedium;
    LONG        count;
} IDataObjectImpl;

static struct ICOM_VTABLE(IDataObject) dtovt;



HRESULT OLEDD_createDataObject(FORMATETC *format, STGMEDIUM *medium, int count, IDataObject **ppDataObject)
{
    IDataObjectImpl *dto;
    

    TRACE("{format = %p, medium = %p, count = %d, ppDataObject = %p}\n",
            format, medium, count, ppDataObject);

    if (ppDataObject == NULL || format == NULL || medium == NULL || count <= 0)
        return E_INVALIDARG;


    dto = HeapAlloc(GetProcessHeap(), 0, sizeof(IDataObjectImpl));

    if (dto)
    {
        dto->ref = 1;
        ICOM_VTBL(dto) = &dtovt;


        dto->pFormatEtc = HeapAlloc(GetProcessHeap(), 0, sizeof(FORMATETC) * count);
        dto->pStgMedium = HeapAlloc(GetProcessHeap(), 0, sizeof(STGMEDIUM) * count);

        if (dto->pFormatEtc == NULL || dto->pStgMedium == NULL){
            ERR("out of memory! {count = %d}\n", count);

            if (dto->pFormatEtc)
                HeapFree(GetProcessHeap(), 0, dto->pFormatEtc);
            
            if (dto->pStgMedium)
                HeapFree(GetProcessHeap(), 0, dto->pStgMedium);


            HeapFree(GetProcessHeap(), 0, dto);

            return E_OUTOFMEMORY;
        }

        else
            TRACE("created the data object %p {pFormatEtc = %p, pStgMedium = %p}\n", dto, dto->pFormatEtc, dto->pStgMedium);

        memcpy(dto->pFormatEtc, format, sizeof(FORMATETC) * count);
        memcpy(dto->pStgMedium, medium, sizeof(STGMEDIUM) * count);
        dto->count = count;
    }

    *ppDataObject = (IDataObject *)dto;

    TRACE("created the data object %p {pFormatEtc = %p, pStgMedium = %p}\n", dto, dto->pFormatEtc, dto->pStgMedium);
    return (*ppDataObject) ? S_OK : E_OUTOFMEMORY;
}



/**************************************************************************
 *  IDataObject_QueryInterface
 */
static HRESULT WINAPI IDataObject_fnQueryInterface(LPDATAOBJECT iface, REFIID riid, LPVOID * ppvObj)
{
    ICOM_THIS(IDataObjectImpl,iface);


    TRACE("(%p)->(\n\tIID:\t%s,%p)\n", This, debugstr_guid(riid), ppvObj);

    if (ppvObj == NULL)
        return E_NOINTERFACE;


    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown))          /*IUnknown*/
      *ppvObj = This;
    
    else if (IsEqualIID(riid, &IID_IDataObject))  /*IDataObject*/
      *ppvObj = (IDataObject *)This;

    if(*ppvObj)
    {
      IUnknown_AddRef((IUnknown*)*ppvObj);
      TRACE("-- Interface: (%p)->(%p)\n",ppvObj,*ppvObj);
      return S_OK;
    }

    TRACE("-- Interface: E_NOINTERFACE\n");
    return E_NOINTERFACE;
}

/**************************************************************************
*  IDataObject_AddRef
*/
static ULONG WINAPI IDataObject_fnAddRef(LPDATAOBJECT iface)
{
    ICOM_THIS(IDataObjectImpl,iface);

    TRACE("(%p)->(count=%lu)\n", This, This->ref);

    return ++(This->ref);
}

/**************************************************************************
*  IDataObject_Release
*/
static ULONG WINAPI IDataObject_fnRelease(LPDATAOBJECT iface)
{
    ICOM_THIS(IDataObjectImpl,iface);
    TRACE("(%p)->()\n",This);


    if (!--(This->ref))
    {
        TRACE(" destroying IDataObject(%p) {pFormatEtc = %p, pStgMedium = %p}\n", This, This->pFormatEtc, This->pStgMedium);

        HeapFree(GetProcessHeap(), 0, This->pFormatEtc);
        HeapFree(GetProcessHeap(), 0, This->pStgMedium);
        HeapFree(GetProcessHeap(), 0, This);

        return 0;
    }

    return This->ref;
}


static int IDataObject_fnLookupFormatEtc(IDataObjectImpl *This, FORMATETC *pFormatEtc)
{
    int     i;


    TRACE("(%p)->(%p)\n", This, pFormatEtc);

    for (i = 0; i < This->count; i++){
        if ((This->pFormatEtc[i].tymed & pFormatEtc->tymed) &&
            This->pFormatEtc[i].cfFormat == pFormatEtc->cfFormat &&
            This->pFormatEtc[i].dwAspect == pFormatEtc->dwAspect)
        {
            TRACE("found object with type 0x%02lx and format %d at index %d\n", pFormatEtc->tymed, pFormatEtc->cfFormat, i);

            return i;
        }
    }

    TRACE("could not find an object with type 0x%02lx and format %d\n", pFormatEtc->tymed, pFormatEtc->cfFormat);
    return -1;
}

static const char *IDataObject_fnGetTymedName(TYMED t){
#define GETNAME(t)  case t: return #t
    switch (t){
	    GETNAME(TYMED_FILE);
	    GETNAME(TYMED_ISTREAM);
	    GETNAME(TYMED_ISTORAGE);
	    GETNAME(TYMED_GDI);
	    GETNAME(TYMED_MFPICT);
	    GETNAME(TYMED_ENHMF);
	    GETNAME(TYMED_NULL);
        default:
            return "<unknownTymed>";
    }
#undef GETNAME
}


static HRESULT WINAPI IDataObject_fnQueryGetData(LPDATAOBJECT iface, FORMATETC *pFormatEtc)
{
    ICOM_THIS(IDataObjectImpl, iface);


    TRACE("(%p)->(%p)\n", This, pFormatEtc);

    if (pFormatEtc == NULL)
        return E_INVALIDARG;

    return (IDataObject_fnLookupFormatEtc(This, pFormatEtc) == -1) ? DV_E_FORMATETC : S_OK;
}


static HRESULT WINAPI IDataObject_fnGetData(LPDATAOBJECT iface, FORMATETC *pFormatEtc, STGMEDIUM *pStgMedium)
{
    ICOM_THIS(IDataObjectImpl, iface);
    int     i;

    
    TRACE("(%p)->(%p, %p)\n", This, pFormatEtc, pStgMedium);

    if (pFormatEtc == NULL || pStgMedium == NULL)
        return E_INVALIDARG;

    
    /* see if there is a matching data format in this object */
    i = IDataObject_fnLookupFormatEtc(This, pFormatEtc);

    if (i == -1)
        return DV_E_FORMATETC;


    /* found a matching object => copy it to the buffer */
    pStgMedium->tymed           = This->pFormatEtc[i].tymed;
    pStgMedium->pUnkForRelease  = 0;
    
    switch (This->pFormatEtc[i].tymed){
        case TYMED_HGLOBAL:
            pStgMedium->u.hGlobal = This->pStgMedium[i].u.hGlobal;
            break;

        default:
            FIXME("trying to retrieve data with a TYMED value of '%s' (%d)\n", 
                    IDataObject_fnGetTymedName(This->pFormatEtc[i].tymed),
                    This->pFormatEtc[i].tymed);

            return DV_E_FORMATETC;
    }
    
    return S_OK;
}

static HRESULT WINAPI IDataObject_fnGetDataHere(LPDATAOBJECT iface, FORMATETC *pFormatEtc, STGMEDIUM *pMedium)
{
    ICOM_THIS(IDataObjectImpl, iface);


    TRACE("(%p)->(%p, %p)\n", This, pFormatEtc, pMedium);
    FIXME("stub!\n");

    return DATA_E_FORMATETC;
}


static HRESULT WINAPI IDataObject_fnEnumFormatEtc(LPDATAOBJECT iface, DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc)
{
    ICOM_THIS(IDataObjectImpl, iface);


    TRACE("(%p)->(%ld, %p)\n", This, dwDirection, ppEnumFormatEtc);
    FIXME("stub!\n");


    return E_NOTIMPL;
}


static HRESULT WINAPI IDataObject_fnDAdvise(LPDATAOBJECT iface, FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
    TRACE("(%p)->(%p, %ld, %p, %p)\n", iface, pFormatEtc, advf, pAdvSink, pdwConnection);
    FIXME("stub!\n");

    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT WINAPI IDataObject_fnDUnadvise(LPDATAOBJECT iface, DWORD dwConnection)
{
    TRACE("(%p)->(%ld)\n", iface, dwConnection);
    FIXME("stub!\n");

    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT WINAPI IDataObject_fnEnumDAdvise(LPDATAOBJECT iface, IEnumSTATDATA **ppEnumAdvise)
{
    TRACE("(%p)->(%p)\n", iface, ppEnumAdvise);
    FIXME("stub!\n");

    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT WINAPI IDataObject_fnGetCanonicalFormatEtc(LPDATAOBJECT iface, FORMATETC *pFormatEtc, FORMATETC *pFormatEtcOut)
{
    TRACE("(%p)->(%p, %p)\n", iface, pFormatEtc, pFormatEtcOut);
    FIXME("stub!\n");


    /* apparently we have to set this field to NULL even though we don't do anything else */
    if (pFormatEtcOut)
        pFormatEtcOut->ptd = NULL;

    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnSetData(LPDATAOBJECT iface, FORMATETC *pFormatEtc, STGMEDIUM *pMedium,  BOOL fRelease)
{
    TRACE("(%p)->(%p, %p, %s)\n", iface, pFormatEtc, pMedium, fRelease ? "TRUE" : "FALSE");
    FIXME("stub!\n");

    return E_NOTIMPL;
}


static struct ICOM_VTABLE(IDataObject) dtovt =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IDataObject_fnQueryInterface,
    IDataObject_fnAddRef,
    IDataObject_fnRelease,
    IDataObject_fnGetData,
    IDataObject_fnGetDataHere,
    IDataObject_fnQueryGetData,
    IDataObject_fnGetCanonicalFormatEtc,
    IDataObject_fnSetData,
    IDataObject_fnEnumFormatEtc,
    IDataObject_fnDAdvise,
    IDataObject_fnDUnadvise,
    IDataObject_fnEnumDAdvise
};
