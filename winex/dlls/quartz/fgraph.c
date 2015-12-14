/*
 * Implementation of CLSID_FilterGraph.
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "strmif.h"
#include "control.h"
#include "uuids.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "fgraph.h"

/***************************************************************************
 *
 *	new/delete for CFilterGraph
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry IFEntries[] =
{
  { &IID_IPersist, offsetof(CFilterGraph,persist)-offsetof(CFilterGraph,unk) },
  { &IID_IDispatch, offsetof(CFilterGraph,disp)-offsetof(CFilterGraph,unk) },
  { &IID_IFilterGraph, offsetof(CFilterGraph,fgraph)-offsetof(CFilterGraph,unk) },
  { &IID_IGraphBuilder, offsetof(CFilterGraph,fgraph)-offsetof(CFilterGraph,unk) },
  { &IID_IFilterGraph2, offsetof(CFilterGraph,fgraph)-offsetof(CFilterGraph,unk) },
  { &IID_IGraphVersion, offsetof(CFilterGraph,graphversion)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaControl, offsetof(CFilterGraph,mediacontrol)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaFilter, offsetof(CFilterGraph,mediafilter)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaEvent, offsetof(CFilterGraph,mediaevent)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaEventEx, offsetof(CFilterGraph,mediaevent)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaEventSink, offsetof(CFilterGraph,mediaeventsink)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaPosition, offsetof(CFilterGraph,mediaposition)-offsetof(CFilterGraph,unk) },
  { &IID_IMediaSeeking, offsetof(CFilterGraph,mediaseeking)-offsetof(CFilterGraph,unk) },
  { &IID_IBasicVideo, offsetof(CFilterGraph,basvid)-offsetof(CFilterGraph,unk) },
  { &IID_IBasicAudio, offsetof(CFilterGraph,basaud)-offsetof(CFilterGraph,unk) },
  { &IID_IVideoWindow, offsetof(CFilterGraph,vidwin)-offsetof(CFilterGraph,unk) },
};


struct FGInitEntry
{
	HRESULT (*pInit)(CFilterGraph*);
	void (*pUninit)(CFilterGraph*);
};

static const struct FGInitEntry FGRAPH_Init[] =
{
	#define	FGENT(a)	{&CFilterGraph_Init##a,&CFilterGraph_Uninit##a},

	FGENT(IPersist)
	FGENT(IDispatch)
	FGENT(IFilterGraph2)
	FGENT(IGraphVersion)
	FGENT(IMediaControl)
	FGENT(IMediaFilter)
	FGENT(IMediaEventEx)
	FGENT(IMediaEventSink)
	FGENT(IMediaPosition)
	FGENT(IMediaSeeking)
	FGENT(IBasicVideo)
	FGENT(IBasicAudio)
	FGENT(IVideoWindow)

	#undef	FGENT
	{ NULL, NULL },
};


static void QUARTZ_DestroyFilterGraph(IUnknown* punk)
{
	CFilterGraph_THIS(punk,unk);
	int	i;

	TRACE( "(%p)\n", punk );

	/* At first, call Stop. */
	IMediaControl_Stop( CFilterGraph_IMediaControl(This) );
	TRACE(" sent Stop 1\n");
	IMediaFilter_Stop( CFilterGraph_IMediaFilter(This) );
	TRACE(" sent Stop 2\n");

	i = 0;
	while ( FGRAPH_Init[i].pInit != NULL )
	{
		FGRAPH_Init[i].pUninit( This );
		i++;
	}

	TRACE( "succeeded.\n" );
}

HRESULT QUARTZ_CreateFilterGraph(IUnknown* punkOuter,void** ppobj)
{
	CFilterGraph*	pfg;
	HRESULT	hr;
	int	i;

	TRACE("(%p,%p)\n",punkOuter,ppobj);

	pfg = (CFilterGraph*)QUARTZ_AllocObj( sizeof(CFilterGraph) );
	if ( pfg == NULL )
		return E_OUTOFMEMORY;

	QUARTZ_IUnkInit( &pfg->unk, punkOuter );

	i = 0;
	hr = NOERROR;
	while ( FGRAPH_Init[i].pInit != NULL )
	{
		hr = FGRAPH_Init[i].pInit( pfg );
		if ( FAILED(hr) )
			break;
		i++;
	}

	if ( FAILED(hr) )
	{
		while ( --i >= 0 )
			FGRAPH_Init[i].pUninit( pfg );
		QUARTZ_FreeObj( pfg );
		return hr;
	}

	pfg->unk.pEntries = IFEntries;
	pfg->unk.dwEntries = sizeof(IFEntries)/sizeof(IFEntries[0]);
	pfg->unk.pOnFinalRelease = QUARTZ_DestroyFilterGraph;

	*ppobj = (void*)(&pfg->unk);

	return S_OK;
}


/***************************************************************************
 *
 *	CFilterGraph::IPersist
 *
 */

static HRESULT WINAPI
IPersist_fnQueryInterface(IPersist* iface,REFIID riid,void** ppobj)
{
	CFilterGraph_THIS(iface,persist);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IPersist_fnAddRef(IPersist* iface)
{
	CFilterGraph_THIS(iface,persist);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IPersist_fnRelease(IPersist* iface)
{
	CFilterGraph_THIS(iface,persist);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}


static HRESULT WINAPI
IPersist_fnGetClassID(IPersist* iface,CLSID* pclsid)
{
	CFilterGraph_THIS(iface,persist);

	TRACE("(%p)->()\n",This);

	if ( pclsid == NULL )
		return E_POINTER;
	memcpy( pclsid, &CLSID_FilterGraph, sizeof(CLSID) );

	return E_NOTIMPL;
}


static ICOM_VTABLE(IPersist) ipersist =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IPersist_fnQueryInterface,
	IPersist_fnAddRef,
	IPersist_fnRelease,
	/* IPersist fields */
	IPersist_fnGetClassID,
};

HRESULT CFilterGraph_InitIPersist( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
	ICOM_VTBL(&pfg->persist) = &ipersist;

	return NOERROR;
}

void CFilterGraph_UninitIPersist( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
}

/***************************************************************************
 *
 *	CFilterGraph::IDispatch
 *
 */

static HRESULT WINAPI
IDispatch_fnQueryInterface(IDispatch* iface,REFIID riid,void** ppobj)
{
	CFilterGraph_THIS(iface,disp);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IDispatch_fnAddRef(IDispatch* iface)
{
	CFilterGraph_THIS(iface,disp);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IDispatch_fnRelease(IDispatch* iface)
{
	CFilterGraph_THIS(iface,disp);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IDispatch_fnGetTypeInfoCount(IDispatch* iface,UINT* pcTypeInfo)
{
	CFilterGraph_THIS(iface,disp);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IDispatch_fnGetTypeInfo(IDispatch* iface,UINT iTypeInfo, LCID lcid, ITypeInfo** ppobj)
{
	CFilterGraph_THIS(iface,disp);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IDispatch_fnGetIDsOfNames(IDispatch* iface,REFIID riid, LPOLESTR* ppwszName, UINT cNames, LCID lcid, DISPID* pDispId)
{
	CFilterGraph_THIS(iface,disp);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IDispatch_fnInvoke(IDispatch* iface,DISPID DispId, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarRes, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	CFilterGraph_THIS(iface,disp);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}



static ICOM_VTABLE(IDispatch) idispatch =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IDispatch_fnQueryInterface,
	IDispatch_fnAddRef,
	IDispatch_fnRelease,
	/* IDispatch fields */
	IDispatch_fnGetTypeInfoCount,
	IDispatch_fnGetTypeInfo,
	IDispatch_fnGetIDsOfNames,
	IDispatch_fnInvoke,
};


HRESULT CFilterGraph_InitIDispatch( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
	ICOM_VTBL(&pfg->disp) = &idispatch;

	return NOERROR;
}

void CFilterGraph_UninitIDispatch( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
}



