/*
 * Implementation of IMediaFilter for FilterGraph.
 *
 * FIXME - stub.
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
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
#include "vfwmsgs.h"
#include "evcode.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "fgraph.h"


#define	WINE_QUARTZ_POLL_INTERVAL	10

/*****************************************************************************/

/* enum used to label vertices in filter graph for topological sort */
enum FG_VertexColour {
	FG_Vertex_White, /* white vertices haven't been visited yet */
	FG_Vertex_Grey,  /* grey vertices are currently being visited */
	FG_Vertex_Black  /* black vertices have been added to sorted list */
};

static HRESULT CFilterGraph_TopologicalSort_Visit( int vertex,
	CFilterGraph* This, IBaseFilter** pFilters, DWORD* pcFilters,
	enum FG_VertexColour* VertexColours );

/*
 * Call the following function with pFilters pointing to a buffer large enough
 * to contain an array with the same number of elements as m_pActiveFilters.
 *
 * Upon return, pFilters will be an array of IBaseFilter*,
 * representing an ordering of filters such that the output of each filter
 * will not be connected to the input of a preceding filter.
 *
 * Each IBaseFilter* in pFilters should be released by the caller.
 */
HRESULT CFilterGraph_TopologicalSort( CFilterGraph* This,
	IBaseFilter** pFilters )
{
	HRESULT hr;
	int index;
	DWORD cFilters = 0;
	enum FG_VertexColour * VertexColours;

	/* initialise the pFilters array to nulls (in case we hit an error) */
	memset( pFilters, 0, This->m_cActiveFilters * sizeof(IBaseFilter*) );

	/* allocate an array for keeping track of vertex colours */
	VertexColours = QUARTZ_AllocMem(
		This->m_cActiveFilters * sizeof(enum FG_VertexColour) );
	if ( VertexColours == NULL )
		return E_OUTOFMEMORY;

	/* initialise all vertices to be coloured white */
	for ( index = 0; index < This->m_cActiveFilters; ++index )
		VertexColours[index] = FG_Vertex_White;

	hr = S_OK;

	/* keep visiting white vertices until the sort is complete */
	for ( index = 0; index < This->m_cActiveFilters; ++index )
	{
		if ( VertexColours[index] == FG_Vertex_White )
		{
			hr = CFilterGraph_TopologicalSort_Visit( index,
				This, pFilters, &cFilters, VertexColours );
			if ( FAILED(hr) )
				break;
		}
	}

	QUARTZ_FreeMem( VertexColours ); 
	return hr;
}

#define FETCH_PINS 5 /* how many pins we read per call to IEnumPins_Next */

static HRESULT CFilterGraph_TopologicalSort_Visit( int vertex,
	CFilterGraph* This, IBaseFilter** pFilters, DWORD* pcFilters,
	enum FG_VertexColour* VertexColours)
{
	HRESULT hr;
	IEnumPins* pEnumOutPins;
	ULONG cFetchedOutPins, PinIndex;
	IBaseFilter* pVisitingFilter;
	IPin* pOutPins[FETCH_PINS];

	pVisitingFilter = This->m_pActiveFilters[vertex].pFilter;

	/* colour this vertex grey */
	VertexColours[vertex] = FG_Vertex_Grey;

	/* get an IEnumPins for enumerating over all adjacent vertices */
	hr = IBaseFilter_EnumPins( pVisitingFilter, &pEnumOutPins );
	if ( FAILED(hr) )
		return hr;
	
	/* visit each adjacent vertex recursively */
	do
	{
		/* get the next few pins attached to this vertex */
		hr = IEnumPins_Next( pEnumOutPins, FETCH_PINS, pOutPins,
			&cFetchedOutPins );
		if ( FAILED(hr) )
			break;
		if ( cFetchedOutPins == 0 )
			break;

		/* for each pin that we just got ... */
		for ( PinIndex = 0; PinIndex < cFetchedOutPins; ++PinIndex )
		{
			IPin *pInPin = NULL;
			PIN_INFO OutPinInfo, InPinInfo;
			ULONG FilterIndex;
	
			/* check whether this is an output pin */
			hr = IPin_QueryPinInfo(pOutPins[PinIndex], &OutPinInfo);
			if ( FAILED(hr) )
				break;
			IBaseFilter_Release( OutPinInfo.pFilter );
			if ( OutPinInfo.dir != PINDIR_OUTPUT )
				continue; /* ignore input pins */

			/* get the input pin of the next filter
			 * to which this output pin connects.      */
			hr = IPin_ConnectedTo( pOutPins[PinIndex], &pInPin );
			if ( hr == VFW_E_NOT_CONNECTED ) {
				hr = S_OK;
				continue;
			}
			if ( FAILED(hr) )
				break;

			/* get the filter to which this input pin belongs */
			hr = IPin_QueryPinInfo(pInPin, &InPinInfo);
			IPin_Release( pInPin );
			if ( FAILED(hr) )
				break;
			if ( InPinInfo.pFilter == NULL )
				continue;

			/* find the index of this filter so we can visit it */
			for ( FilterIndex = 0;
				FilterIndex < This->m_cActiveFilters;
				++FilterIndex )
			{
    				if ( This->m_pActiveFilters[FilterIndex].pFilter 
					== InPinInfo.pFilter )
				{
					break;
				}
			}

			if ( FilterIndex == This->m_cActiveFilters ) {
				hr = E_UNEXPECTED;
				IBaseFilter_Release( InPinInfo.pFilter );
				break;
			}

			/* visit the filter if it is coloured white in graph */
			if ( VertexColours[FilterIndex] == FG_Vertex_White )
			{
				CFilterGraph_TopologicalSort_Visit(
					FilterIndex, This, pFilters, pcFilters,
					VertexColours );
			}
			else if ( VertexColours[FilterIndex] == FG_Vertex_Grey )
			{
				WARN("Cyclic filter graph detected\n");
			}

			IBaseFilter_Release( InPinInfo.pFilter );

		} /* end for (PinIndex ... */
	
		/* release pins */
		for ( PinIndex = 0; PinIndex < cFetchedOutPins; ++PinIndex )
		{
			IPin_Release( pOutPins[PinIndex] );
		}

	} while ( cFetchedOutPins == FETCH_PINS && ! FAILED( hr ));

	if ( ! FAILED( hr ) )
	{
		/* add this vertex to front of output list */
		++*pcFilters;
		pFilters[This->m_cActiveFilters - *pcFilters] = pVisitingFilter;
		IBaseFilter_AddRef( pVisitingFilter );

		/* colour this vertex black */
		VertexColours[vertex] = FG_Vertex_Black;

		hr = S_OK;
	}

	IEnumPins_Release( pEnumOutPins );
	return hr;
}

#undef FETCH_PINS
		
static
HRESULT CFilterGraph_PollGraphState(
	CFilterGraph* This,
	FILTER_STATE* pState)
{
	HRESULT	hr;
	DWORD	n;

	hr = S_OK;
	*pState = State_Stopped;

	EnterCriticalSection( &This->m_csGraphState );
	EnterCriticalSection( &This->m_csFilters );

	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		hr = IBaseFilter_GetState( This->m_pActiveFilters[n].pFilter, (DWORD)0, pState );
		if ( hr != S_OK )
			break;
	}

	LeaveCriticalSection( &This->m_csFilters );
	LeaveCriticalSection( &This->m_csGraphState );

	TRACE( "returns %08x, state %d\n",
		hr, *pState );

	return hr;
}

static
HRESULT CFilterGraph_StopGraph(
	CFilterGraph* This )
{
	HRESULT	hr;
	HRESULT	hrFilter;
	DWORD	n;
	IBaseFilter** pFilters;

	hr = S_OK;

	EnterCriticalSection( &This->m_csFilters );

	pFilters = QUARTZ_AllocMem( This->m_cActiveFilters
		* sizeof(IBaseFilter*) );
	if ( pFilters == NULL )
		return E_OUTOFMEMORY;
	
	CFilterGraph_TopologicalSort( This, pFilters );
	
	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		hrFilter = IBaseFilter_Stop( pFilters[n] );
		if ( hrFilter != S_OK )
		{
			if ( SUCCEEDED(hr) )
				hr = hrFilter;
		}

		IBaseFilter_Release( pFilters[n] );
	}

	QUARTZ_FreeMem( pFilters );

	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static
HRESULT CFilterGraph_PauseGraph(
	CFilterGraph* This )
{
	HRESULT	hr;
	HRESULT	hrFilter;
	DWORD	n;
	IBaseFilter** pFilters;

	hr = S_OK;

	EnterCriticalSection( &This->m_csFilters );

	pFilters = QUARTZ_AllocMem( This->m_cActiveFilters
		* sizeof(IBaseFilter*) );
	if ( pFilters == NULL )
		return E_OUTOFMEMORY;
	
	CFilterGraph_TopologicalSort( This, pFilters );
	
	for ( n = This->m_cActiveFilters; n > 0; n-- )
	{
		hrFilter = IBaseFilter_Pause( pFilters[n-1] );
		if ( hrFilter != S_OK )
		{
			if ( SUCCEEDED(hr) )
				hr = hrFilter;
		}

		IBaseFilter_Release( pFilters[n-1] );
	}

	QUARTZ_FreeMem( pFilters );

	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static
HRESULT CFilterGraph_RunGraph(
	CFilterGraph* This, REFERENCE_TIME rtStart )
{
	HRESULT	hr;
	HRESULT	hrFilter;
	DWORD	n;
	IBaseFilter** pFilters;

	hr = S_OK;

	EnterCriticalSection( &This->m_csFilters );

	pFilters = QUARTZ_AllocMem( This->m_cActiveFilters
		* sizeof(IBaseFilter*) );
	if ( pFilters == NULL )
		return E_OUTOFMEMORY;

	CFilterGraph_TopologicalSort( This, pFilters );
	
	for ( n = This->m_cActiveFilters; n > 0; n-- )
	{
		hrFilter = IBaseFilter_Run( pFilters[n-1], rtStart );
		if ( hrFilter != S_OK )
		{
			if ( SUCCEEDED(hr) )
				hr = hrFilter;
		}

		IBaseFilter_Release( pFilters[n-1] );
	}

	QUARTZ_FreeMem( pFilters );

	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static
HRESULT CFilterGraph_SetSyncSourceGraph(
	CFilterGraph* This, IReferenceClock* pClock )
{
	HRESULT	hr;
	HRESULT	hrFilter;
	DWORD	n;

	hr = S_OK;

	EnterCriticalSection( &This->m_csFilters );

	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		hrFilter = IBaseFilter_SetSyncSource( This->m_pActiveFilters[n].pFilter, pClock );
		if ( hrFilter == E_NOTIMPL )
			hrFilter = S_OK;
		if ( hrFilter != S_OK )
		{
			if ( SUCCEEDED(hr) )
				hr = hrFilter;
		}
	}

	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}



/*****************************************************************************/

static HRESULT WINAPI
IMediaFilter_fnQueryInterface(IMediaFilter* iface,REFIID riid,void** ppobj)
{
	CFilterGraph_THIS(iface,mediafilter);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IMediaFilter_fnAddRef(IMediaFilter* iface)
{
	CFilterGraph_THIS(iface,mediafilter);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IMediaFilter_fnRelease(IMediaFilter* iface)
{
	CFilterGraph_THIS(iface,mediafilter);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}


static HRESULT WINAPI
IMediaFilter_fnGetClassID(IMediaFilter* iface,CLSID* pclsid)
{
	CFilterGraph_THIS(iface,mediafilter);

	TRACE("(%p)->()\n",This);

	return IPersist_GetClassID(
		CFilterGraph_IPersist(This),pclsid);
}

static HRESULT WINAPI
IMediaFilter_fnStop(IMediaFilter* iface)
{
	CFilterGraph_THIS(iface,mediafilter);
	HRESULT	hr;

	TRACE("(%p)->()\n",This);

	hr = S_OK;

	EnterCriticalSection( &This->m_csGraphState );

	if ( This->m_stateGraph != State_Stopped )
	{
		/* IDistributorNotify_Stop() */

		hr = CFilterGraph_StopGraph(This);

		This->m_cCompletes = 0;
		This->m_stateGraph = State_Stopped;
	}

	LeaveCriticalSection( &This->m_csGraphState );

	return hr;
}

static HRESULT WINAPI
IMediaFilter_fnPause(IMediaFilter* iface)
{
	CFilterGraph_THIS(iface,mediafilter);
	HRESULT	hr;

	TRACE("(%p)->()\n",This);

	hr = S_OK;

	EnterCriticalSection( &This->m_csGraphState );

	if (This->m_defClock && This->m_pClock == NULL)
		IFilterGraph2_SetDefaultSyncSource(CFilterGraph_IFilterGraph2(This));

	if ( This->m_stateGraph != State_Paused )
	{
		/* IDistributorNotify_Pause() */

		hr = CFilterGraph_PauseGraph(This);
		if ( SUCCEEDED(hr) )
			This->m_stateGraph = State_Paused;
		else
			(void)CFilterGraph_StopGraph(This);
	}

	LeaveCriticalSection( &This->m_csGraphState );

	return hr;
}

static HRESULT WINAPI
IMediaFilter_fnRun(IMediaFilter* iface,REFERENCE_TIME rtStart)
{
	CFilterGraph_THIS(iface,mediafilter);
	HRESULT	hr;
	IReferenceClock*	pClock;

	TRACE("(%p)->()\n",This);

	EnterCriticalSection( &This->m_csGraphState );

	if ( This->m_stateGraph == State_Stopped )
	{
		hr = IMediaFilter_Pause(iface);
		if ( FAILED(hr) )
			goto end;
	}

        /* handle the special time. */
        if ( rtStart == (REFERENCE_TIME)0 )
        {
                hr = IMediaFilter_GetSyncSource(iface,&pClock);
                if ( hr == S_OK && pClock != NULL )
                {
                        IReferenceClock_GetTime(pClock,&rtStart);
                        IReferenceClock_Release(pClock);
			/* DirectShow adds some extra time */
			rtStart += 2 * QUARTZ_TIMEUNITS; /* 2s */
                }
        }

	hr = NOERROR;

	if ( This->m_stateGraph != State_Running )
	{
		/* IDistributorNotify_Run() */

		hr = CFilterGraph_RunGraph(This,rtStart);

		if ( SUCCEEDED(hr) )
			This->m_stateGraph = State_Running;
		else
			(void)CFilterGraph_StopGraph(This);
	}

end:
	LeaveCriticalSection( &This->m_csGraphState );

	return hr;
}

static HRESULT WINAPI
IMediaFilter_fnGetState(IMediaFilter* iface,DWORD dwTimeOut,FILTER_STATE* pState)
{
	CFilterGraph_THIS(iface,mediafilter);
	HRESULT	hr;
	DWORD	dwTickStart;
	DWORD	dwTickUsed;

	TRACE("(%p)->(%p)\n",This,pState);
	if ( pState == NULL )
		return E_POINTER;

	dwTickStart = GetTickCount();

	while ( 1 )
	{
		hr = CFilterGraph_PollGraphState( This, pState );
		if ( hr != VFW_S_STATE_INTERMEDIATE )
			break;
		if ( dwTimeOut == 0 )
			break;

		Sleep( (dwTimeOut >= WINE_QUARTZ_POLL_INTERVAL) ?
			WINE_QUARTZ_POLL_INTERVAL : dwTimeOut );

		dwTickUsed = GetTickCount() - dwTickStart;

		dwTickStart += dwTickUsed;
		if ( dwTimeOut <= dwTickUsed )
			dwTimeOut = 0;
		else
			dwTimeOut -= dwTickUsed;
	}

	EnterCriticalSection( &This->m_csGraphState );
	*pState = This->m_stateGraph;
	LeaveCriticalSection( &This->m_csGraphState );

	return hr;
}

static HRESULT WINAPI
IMediaFilter_fnSetSyncSource(IMediaFilter* iface,IReferenceClock* pobjClock)
{
	CFilterGraph_THIS(iface,mediafilter);
	HRESULT hr = NOERROR;

	TRACE("(%p)->(%p)\n",This,pobjClock);

	/* IDistributorNotify_SetSyncSource() */

	EnterCriticalSection( &This->m_csClock );

	hr = CFilterGraph_SetSyncSourceGraph( This, pobjClock );

	if ( SUCCEEDED(hr) )
	{
		if ( This->m_pClock != NULL )
		{
			IReferenceClock_Release(This->m_pClock);
			This->m_pClock = NULL;
		}
		This->m_pClock = pobjClock;
		This->m_defClock = FALSE;
		if ( pobjClock != NULL )
			IReferenceClock_AddRef( pobjClock );
		IMediaEventSink_Notify(CFilterGraph_IMediaEventSink(This),
			EC_CLOCK_CHANGED, 0, 0);
	}
	else
	{
		(void)CFilterGraph_SetSyncSourceGraph( This, This->m_pClock );
	}

	LeaveCriticalSection( &This->m_csClock );

	TRACE( "hr = %08x\n", hr );

	return hr;
}

static HRESULT WINAPI
IMediaFilter_fnGetSyncSource(IMediaFilter* iface,IReferenceClock** ppobjClock)
{
	CFilterGraph_THIS(iface,mediafilter);
	HRESULT hr = VFW_E_NO_CLOCK;

	TRACE("(%p)->(%p)\n",This,ppobjClock);

	if ( ppobjClock == NULL )
		return E_POINTER;

	EnterCriticalSection( &This->m_csClock );
	*ppobjClock = This->m_pClock;
	if ( This->m_pClock != NULL )
	{
		hr = NOERROR;
		IReferenceClock_AddRef( This->m_pClock );
	}
	LeaveCriticalSection( &This->m_csClock );

	TRACE( "hr = %08x\n", hr );

	return hr;
}



static ICOM_VTABLE(IMediaFilter) imediafilter =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IMediaFilter_fnQueryInterface,
	IMediaFilter_fnAddRef,
	IMediaFilter_fnRelease,
	/* IPersist fields */
	IMediaFilter_fnGetClassID,
	/* IMediaFilter fields */
	IMediaFilter_fnStop,
	IMediaFilter_fnPause,
	IMediaFilter_fnRun,
	IMediaFilter_fnGetState,
	IMediaFilter_fnSetSyncSource,
	IMediaFilter_fnGetSyncSource,
};

HRESULT CFilterGraph_InitIMediaFilter( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);

	ICOM_VTBL(&pfg->mediafilter) = &imediafilter;

	CRITICAL_SECTION_DEFINE( &pfg->m_csGraphState );
	CRITICAL_SECTION_DEFINE( &pfg->m_csClock );
	pfg->m_stateGraph = State_Stopped;
	pfg->m_pClock = NULL;
	pfg->m_defClock = TRUE;

	return NOERROR;
}

void CFilterGraph_UninitIMediaFilter( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);

	if ( pfg->m_pClock != NULL )
	{
		IReferenceClock_Release( pfg->m_pClock );
		pfg->m_pClock = NULL;
	}

	DeleteCriticalSection( &pfg->m_csGraphState );
	DeleteCriticalSection( &pfg->m_csClock );
}
