/*
 * Implementation of IEnumRegFilters.
 *
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "strmif.h"

#include "wine/unicode.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "devenum.h"
#include "enumreg.h"
#include "iunk.h"



typedef struct IEnumRegFiltersImpl
{
	ICOM_VFIELD(IEnumRegFilters);
} IEnumRegFiltersImpl;

typedef struct
{
	QUARTZ_IUnkImpl	unk;
	IEnumRegFiltersImpl	enumreg;
	QUARTZ_CompList*	pCompList;
	QUARTZ_CompListItem*	pItemCur;
} CEnumRegFilters;

#define	CEnumRegFilters_THIS(iface,member)		CEnumRegFilters*	This = ((CEnumRegFilters*)(((char*)iface)-offsetof(CEnumRegFilters,member)))



static QUARTZ_IFEntry EnumRegIFEntries[] =
{
  { &IID_IEnumRegFilters, offsetof(CEnumRegFilters,enumreg)-offsetof(CEnumRegFilters,unk) },
};


static HRESULT WINAPI
IEnumRegFilters_fnQueryInterface(IEnumRegFilters* iface,REFIID riid,void** ppobj)
{
	CEnumRegFilters_THIS(iface,enumreg);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IEnumRegFilters_fnAddRef(IEnumRegFilters* iface)
{
	CEnumRegFilters_THIS(iface,enumreg);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IEnumRegFilters_fnRelease(IEnumRegFilters* iface)
{
	CEnumRegFilters_THIS(iface,enumreg);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IEnumRegFilters_fnNext(IEnumRegFilters* iface,ULONG cReq,REGFILTER** ppreg,ULONG* pcFetched)
{
	CEnumRegFilters_THIS(iface,enumreg);
	HRESULT	hr;
	ULONG	cFetched;

	TRACE("(%p)->(%u,%p,%p)\n",This,cReq,ppreg,pcFetched);

	if ( pcFetched == NULL && cReq > 1 )
		return E_INVALIDARG;
	if ( ppreg == NULL )
		return E_POINTER;

	QUARTZ_CompList_Lock( This->pCompList );

	hr = NOERROR;
	cFetched = 0;
	while ( cReq > 0 )
	{
		IMoniker* pMon;
		REGFILTER* pReg;
		LPOLESTR pName;

		if ( This->pItemCur == NULL )
		{
			hr = S_FALSE;
			break;
		}

		pMon = (IMoniker*)QUARTZ_CompList_GetItemPtr( This->pItemCur );
		/* I assume we're supposed to get the display name */
		pName = NULL;
		IMoniker_GetDisplayName(pMon, NULL, NULL, &pName);

		/* the caller is supposed to free the structure with CoTaskMemFree,
		 * so alloc it with CoTaskMemAlloc here. */
		if (pName) {
			pReg = CoTaskMemAlloc( sizeof(REGFILTER) + (strlenW(pName)+1)*sizeof(WCHAR) );
			pReg->Name = (LPWSTR)(pReg+1);
			strcpyW(pReg->Name, pName);
		} else {
			pReg = CoTaskMemAlloc( sizeof(REGFILTER) );
			pReg->Name = NULL;
		}
		QUARTZ_GetCLSIDFromMoniker(pMon, &pReg->Clsid);

		/* free display name */
		CoTaskMemFree(pName);

		ppreg[ cFetched ] = pReg;
		This->pItemCur =
			QUARTZ_CompList_GetNext( This->pCompList, This->pItemCur );
		cFetched ++;
		cReq --;
	}

	QUARTZ_CompList_Unlock( This->pCompList );

	if ( pcFetched != NULL )
		*pcFetched = cFetched;

	return hr;
}

static HRESULT WINAPI
IEnumRegFilters_fnSkip(IEnumRegFilters* iface,ULONG cSkip)
{
	CEnumRegFilters_THIS(iface,enumreg);
	HRESULT	hr;

	TRACE("(%p)->()\n",This);

	QUARTZ_CompList_Lock( This->pCompList );

	hr = NOERROR;
	while ( cSkip > 0 )
	{
		if ( This->pItemCur == NULL )
		{
			hr = S_FALSE;
			break;
		}
		This->pItemCur =
			QUARTZ_CompList_GetNext( This->pCompList, This->pItemCur );
		cSkip --;
	}

	QUARTZ_CompList_Unlock( This->pCompList );

	return hr;
}

static HRESULT WINAPI
IEnumRegFilters_fnReset(IEnumRegFilters* iface)
{
	CEnumRegFilters_THIS(iface,enumreg);

	TRACE("(%p)->()\n",This);

	QUARTZ_CompList_Lock( This->pCompList );

	This->pItemCur = QUARTZ_CompList_GetFirst( This->pCompList );

	QUARTZ_CompList_Unlock( This->pCompList );

	return NOERROR;
}

static HRESULT WINAPI
IEnumRegFilters_fnClone(IEnumRegFilters* iface,IEnumRegFilters** ppunk)
{
	CEnumRegFilters_THIS(iface,enumreg);
	HRESULT	hr;

	TRACE("(%p)->()\n",This);

	if ( ppunk == NULL )
		return E_POINTER;

	QUARTZ_CompList_Lock( This->pCompList );

	hr = QUARTZ_CreateEnumRegFilters(
		(void**)ppunk,
		This->pCompList );
	FIXME( "current pointer must be seeked correctly\n" );

	QUARTZ_CompList_Unlock( This->pCompList );

	return hr;
}


static ICOM_VTABLE(IEnumRegFilters) ienumreg =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IEnumRegFilters_fnQueryInterface,
	IEnumRegFilters_fnAddRef,
	IEnumRegFilters_fnRelease,
	/* IEnumRegFilters fields */
	IEnumRegFilters_fnNext,
	IEnumRegFilters_fnSkip,
	IEnumRegFilters_fnReset,
	IEnumRegFilters_fnClone,
};

void QUARTZ_DestroyEnumRegFilters(IUnknown* punk)
{
	CEnumRegFilters_THIS(punk,unk);

	if ( This->pCompList != NULL )
		QUARTZ_CompList_Free( This->pCompList );
}

HRESULT QUARTZ_CreateEnumRegFilters(
	void** ppobj, const QUARTZ_CompList* pCompList )
{
	CEnumRegFilters*	penum;
	QUARTZ_CompList*	pCompListDup;

	TRACE("(%p,%p)\n",ppobj,pCompList);

	pCompListDup = QUARTZ_CompList_Dup( pCompList, FALSE );
	if ( pCompListDup == NULL )
		return E_OUTOFMEMORY;

	penum = (CEnumRegFilters*)QUARTZ_AllocObj( sizeof(CEnumRegFilters) );
	if ( penum == NULL )
	{
		QUARTZ_CompList_Free( pCompListDup );
		return E_OUTOFMEMORY;
	}

	QUARTZ_IUnkInit( &penum->unk, NULL );
	ICOM_VTBL(&penum->enumreg) = &ienumreg;

	penum->pCompList = pCompListDup;
	penum->pItemCur = QUARTZ_CompList_GetFirst( pCompListDup );

	penum->unk.pEntries = EnumRegIFEntries;
	penum->unk.dwEntries = 1;
	penum->unk.pOnFinalRelease = QUARTZ_DestroyEnumRegFilters;

	*ppobj = (void*)(&penum->enumreg);

	return S_OK;
}

