/*
 * Copyright (C) Hidenori TAKESHIMA
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

#ifndef	WINE_DSHOW_FGRAPH_H
#define	WINE_DSHOW_FGRAPH_H

/*
		implements CLSID_FilterGraph.

	- At least, the following interfaces should be implemented:

	IUnknown
		+ IPersist
		+ IDispatch
		+ IFilterGraph - IGraphBuilder - IFilterGraph2
		+ IGraphVersion
		+ IGraphConfig
		+ IDispatch - IMediaControl
		+ IPersist - IMediaFilter
		+ IDispatch - IMediaEvent - IMediaEventEx
		+ IMediaEventSink
		+ IDispatch - IMediaPosition
		+ IMediaSeeking
		+ IDispatch - IBasicVideo (pass to a renderer)
		+ IDispatch - IBasicAudio (pass to a renderer)
		+ IDispatch - IVideoWindow  (pass to a renderer)
	(following interfaces are not implemented)
		+ IMarshal
		+ IFilterMapper2
		FIXME - Are there any missing interfaces???
 */

#include "iunk.h"
#include "complist.h"


typedef struct FG_IPersistImpl
{
	ICOM_VFIELD(IPersist);
} FG_IPersistImpl;

typedef struct FG_IDispatchImpl
{
	ICOM_VFIELD(IDispatch);
} FG_IDispatchImpl;

typedef struct FG_IFilterGraph2Impl
{
	ICOM_VFIELD(IFilterGraph2);
} FG_IFilterGraph2Impl;

typedef struct FG_IGraphVersionImpl
{
	ICOM_VFIELD(IGraphVersion);
} FG_IGraphVersionImpl;

typedef struct FG_IMediaControlImpl
{
	ICOM_VFIELD(IMediaControl);
} FG_IMediaControlImpl;

typedef struct FG_IMediaFilterImpl
{
	ICOM_VFIELD(IMediaFilter);
} FG_IMediaFilterImpl;

typedef struct FG_IMediaEventImpl
{
	ICOM_VFIELD(IMediaEventEx);
} FG_IMediaEventImpl;

typedef struct FG_IMediaEventSinkImpl
{
	ICOM_VFIELD(IMediaEventSink);
} FG_IMediaEventSinkImpl;

typedef struct FG_IMediaPositionImpl
{
	ICOM_VFIELD(IMediaPosition);
} FG_IMediaPositionImpl;

typedef struct FG_IMediaSeekingImpl
{
	ICOM_VFIELD(IMediaSeeking);
} FG_IMediaSeekingImpl;

typedef struct FG_IBasicVideoImpl
{
	ICOM_VFIELD(IBasicVideo);
} FG_IBasicVideoImpl;

typedef struct FG_IBasicAudioImpl
{
	ICOM_VFIELD(IBasicAudio);
} FG_IBasicAudioImpl;

typedef struct FG_IVideoWindowImpl
{
	ICOM_VFIELD(IVideoWindow);
} FG_IVideoWindowImpl;


typedef struct FG_FilterData
{
	IBaseFilter*	pFilter;
	IMediaPosition*	pPosition;
	IMediaSeeking*	pSeeking;
	WCHAR*	pwszName;
	DWORD	cbName;
} FG_FilterData;

typedef struct FilterGraph_MEDIAEVENT	FilterGraph_MEDIAEVENT;

typedef struct CFilterGraph
{
	QUARTZ_IUnkImpl	unk;
	FG_IPersistImpl	persist;
	FG_IDispatchImpl	disp;
	FG_IFilterGraph2Impl	fgraph;
	FG_IGraphVersionImpl	graphversion;
	FG_IMediaControlImpl	mediacontrol;
	FG_IMediaFilterImpl	mediafilter;
	FG_IMediaEventImpl	mediaevent;
	FG_IMediaEventSinkImpl	mediaeventsink;
	FG_IMediaPositionImpl	mediaposition;
	FG_IMediaSeekingImpl	mediaseeking;
	FG_IBasicVideoImpl	basvid;
	FG_IBasicAudioImpl	basaud;
	FG_IVideoWindowImpl	vidwin;

	/* IDispatch fields. */
	/* IFilterGraph2 fields. */
	CRITICAL_SECTION	m_csFilters;
	DWORD	m_cActiveFilters;
	FG_FilterData*		m_pActiveFilters;
	DWORD	m_cRenders;
	DWORD	m_cCompletes;
	/* IGraphVersion fields. */
	LONG	m_lGraphVersion;
	/* IMediaControl fields. */
	/* IMediaFilter fields. */
	CRITICAL_SECTION	m_csGraphState;
	FILTER_STATE	m_stateGraph; /* must NOT accessed directly! */
	CRITICAL_SECTION	m_csClock;
	IReferenceClock*	m_pClock;
	BOOL			m_defClock;
	/* IMediaEvent fields. */
	HANDLE	m_hMediaEvent;
	CRITICAL_SECTION	m_csMediaEvents;
	FilterGraph_MEDIAEVENT*	m_pMediaEvents;
	ULONG	m_cbMediaEventsPut;
	ULONG	m_cbMediaEventsGet;
	ULONG	m_cbMediaEventsMax;
	HWND	m_hwndEventNotify;
	long	m_lEventNotifyMsg;
	LONG_PTR	m_lEventNotifyParam;
	long	m_lEventNotifyFlags;
	/* IMediaEventSink fields. */
	/* IMediaPosition fields. */
	/* IMediaSeeking fields. */
	/* IBasicVideo fields. */
	/* IBasicAudio fields. */
	/* IVideoWindow fields. */
} CFilterGraph;

#define	CFilterGraph_THIS(iface,member)		CFilterGraph*	This = ((CFilterGraph*)(((char*)iface)-offsetof(CFilterGraph,member)))
#define	CFilterGraph_IPersist(th)		((IPersist*)&((th)->persist))
#define	CFilterGraph_IDispatch(th)		((IDispatch*)&((th)->disp))
#define	CFilterGraph_IFilterGraph2(th)		((IFilterGraph2*)&((th)->fgraph))
#define	CFilterGraph_IMediaControl(th)		((IMediaControl*)&((th)->mediacontrol))
#define	CFilterGraph_IMediaFilter(th)		((IMediaFilter*)&((th)->mediafilter))
#define	CFilterGraph_IMediaEventEx(th)		((IMediaEventEx*)&((th)->mediaevent))
#define	CFilterGraph_IMediaEventSink(th)		((IMediaEventSink*)&((th)->mediaeventsink))

HRESULT QUARTZ_CreateFilterGraph(IUnknown* punkOuter,void** ppobj);

HRESULT CFilterGraph_InitIPersist( CFilterGraph* pfg );
void CFilterGraph_UninitIPersist( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIDispatch( CFilterGraph* pfg );
void CFilterGraph_UninitIDispatch( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIFilterGraph2( CFilterGraph* pfg );
void CFilterGraph_UninitIFilterGraph2( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIGraphVersion( CFilterGraph* pfg );
void CFilterGraph_UninitIGraphVersion( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIMediaControl( CFilterGraph* pfg );
void CFilterGraph_UninitIMediaControl( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIMediaFilter( CFilterGraph* pfg );
void CFilterGraph_UninitIMediaFilter( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIMediaEventEx( CFilterGraph* pfg );
void CFilterGraph_UninitIMediaEventEx( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIMediaEventSink( CFilterGraph* pfg );
void CFilterGraph_UninitIMediaEventSink( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIMediaPosition( CFilterGraph* pfg );
void CFilterGraph_UninitIMediaPosition( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIMediaSeeking( CFilterGraph* pfg );
void CFilterGraph_UninitIMediaSeeking( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIBasicVideo( CFilterGraph* pfg );
void CFilterGraph_UninitIBasicVideo( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIBasicAudio( CFilterGraph* pfg );
void CFilterGraph_UninitIBasicAudio( CFilterGraph* pfg );
HRESULT CFilterGraph_InitIVideoWindow( CFilterGraph* pfg );
void CFilterGraph_UninitIVideoWindow( CFilterGraph* pfg );


#endif	/* WINE_DSHOW_FGRAPH_H */
