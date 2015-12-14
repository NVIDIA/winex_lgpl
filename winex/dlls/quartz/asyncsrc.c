/*
 * Implements Asynchronous File/URL Source.
 *
 * FIXME - URL source is not implemented yet.
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
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
#include "vfwmsgs.h"
#include "uuids.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "asyncsrc.h"
#include "memalloc.h"



const WCHAR QUARTZ_wszAsyncFileSourceName[] =
{'F','i','l','e',' ','S','o','u','r','c','e',' ','(','A','s','y','n','c','.',')',0};
const WCHAR QUARTZ_wszAsyncFileSourcePinName[] =
{'O','u','t',0};
const WCHAR QUARTZ_wszAsyncURLSourceName[] =
{'F','i','l','e',' ','S','o','u','r','c','e',' ','(','U','R','L',')',0};
const WCHAR QUARTZ_wszAsyncURLSourcePinName[] =
{'O','u','t',0};



/***************************************************************************
 *
 *	CAsyncReaderImpl internal methods
 *
 */

static
AsyncSourceRequest* CAsyncReaderImpl_AllocRequest( CAsyncReaderImpl* This )
{
	AsyncSourceRequest* pReq;

	EnterCriticalSection( &This->m_csFree );
	pReq = This->m_pFreeFirst;
	if ( pReq != NULL )
		This->m_pFreeFirst = pReq->pNext;
	LeaveCriticalSection( &This->m_csFree );

	if ( pReq == NULL )
	{
		pReq = (AsyncSourceRequest*)QUARTZ_AllocMem(
			sizeof(AsyncSourceRequest) );
		if ( pReq == NULL )
			return NULL;
	}

	pReq->pNext = NULL;
	pReq->pSample = NULL;
	pReq->dwContext = 0;
	pReq->hr = E_FAIL;

	return pReq;
}

static
void CAsyncReaderImpl_FreeRequest( CAsyncReaderImpl* This, AsyncSourceRequest* pReq, BOOL bReleaseMem )
{
	if ( !bReleaseMem )
	{
		EnterCriticalSection( &This->m_csFree );
		pReq->pNext = This->m_pFreeFirst;
		This->m_pFreeFirst = pReq;
		LeaveCriticalSection( &This->m_csFree );
	}
	else
	{
		QUARTZ_FreeMem( pReq );
	}
}

static
AsyncSourceRequest* CAsyncReaderImpl_GetReply( CAsyncReaderImpl* This )
{
	AsyncSourceRequest*	pReq;

	EnterCriticalSection( &This->m_csReply );
	pReq = This->m_pReplyFirst;
	if ( pReq != NULL )
	{
		This->m_pReplyFirst = pReq->pNext;
		if (!This->m_pReplyFirst)
			This->m_pReplyLast = NULL;
	}
	LeaveCriticalSection( &This->m_csReply );

	return pReq;
}

static
void CAsyncReaderImpl_PostReply( CAsyncReaderImpl* This, AsyncSourceRequest* pReq )
{
	AsyncSourceRequest* pLast;

	EnterCriticalSection( &This->m_csReply );
	pLast = This->m_pReplyLast;
	if (pLast) pLast->pNext = pReq;
	This->m_pReplyLast = pReq;
	if (!pLast) This->m_pReplyFirst = pReq;
	LeaveCriticalSection( &This->m_csReply );
}

static
void CAsyncReaderImpl_ReleaseReqList( CAsyncReaderImpl* This, AsyncSourceRequest** ppReq, AsyncSourceRequest** ppTail, BOOL bReleaseMem )
{
	AsyncSourceRequest* pReq;
	AsyncSourceRequest* pReqNext;

	TRACE("(%p,%p,%d)\n",This,*ppReq,bReleaseMem);
	pReq = *ppReq;
	if (ppTail) *ppTail = NULL;
	*ppReq = NULL;
	while ( pReq != NULL )
	{
		pReqNext = pReq->pNext;
		CAsyncReaderImpl_FreeRequest(This,pReq,bReleaseMem);
		pReq = pReqNext;
	}
}

/***************************************************************************
 *
 *	CAsyncReaderImpl methods
 *
 */

static HRESULT WINAPI
CAsyncReaderImpl_fnQueryInterface(IAsyncReader* iface,REFIID riid,void** ppobj)
{
	ICOM_THIS(CAsyncReaderImpl,iface);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->punkControl,riid,ppobj);
}

static ULONG WINAPI
CAsyncReaderImpl_fnAddRef(IAsyncReader* iface)
{
	ICOM_THIS(CAsyncReaderImpl,iface);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->punkControl);
}

static ULONG WINAPI
CAsyncReaderImpl_fnRelease(IAsyncReader* iface)
{
	ICOM_THIS(CAsyncReaderImpl,iface);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->punkControl);
}

static HRESULT WINAPI
CAsyncReaderImpl_fnRequestAllocator(IAsyncReader* iface,IMemAllocator* pAlloc,ALLOCATOR_PROPERTIES* pProp,IMemAllocator** ppAllocActual)
{
	ICOM_THIS(CAsyncReaderImpl,iface);
	HRESULT hr;
	ALLOCATOR_PROPERTIES	propActual;
	IUnknown* punk = NULL;

	TRACE("(%p)->(%p,%p,%p)\n",This,pAlloc,pProp,ppAllocActual);

	if ( pAlloc == NULL || pProp == NULL || ppAllocActual == NULL )
		return E_POINTER;

	IMemAllocator_AddRef(pAlloc);
	hr = IMemAllocator_SetProperties( pAlloc, pProp, &propActual );
	if ( SUCCEEDED(hr) )
	{
		*ppAllocActual = pAlloc;
		return S_OK;
	}
	IMemAllocator_Release(pAlloc);

	hr = QUARTZ_CreateMemoryAllocator(NULL,(void**)&punk);
	if ( FAILED(hr) )
		return hr;
	hr = IUnknown_QueryInterface( punk, &IID_IMemAllocator, (void**)&pAlloc );
	IUnknown_Release(punk);
	if ( FAILED(hr) )
		return hr;

	hr = IMemAllocator_SetProperties( pAlloc, pProp, &propActual );
	if ( SUCCEEDED(hr) )
	{
		*ppAllocActual = pAlloc;
		return S_OK;
	}
	IMemAllocator_Release(pAlloc);

	return hr;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnRequest(IAsyncReader* iface,IMediaSample* pSample,DWORD_PTR dwContext)
{
	ICOM_THIS(CAsyncReaderImpl,iface);
	AsyncSourceRequest* pReq;
	HRESULT hr = NOERROR;

	/*
	 * before implementing asynchronous I/O,
	 * please check patents by yourself
	 */
	WARN("(%p,%p,%lu) no async I/O\n",This,pSample,dwContext);

	hr = IAsyncReader_SyncReadAligned(iface,pSample);
	if ( FAILED(hr) )
		return hr;
	pReq = CAsyncReaderImpl_AllocRequest(This);
	if ( pReq == NULL )
		return E_OUTOFMEMORY;
	pReq->pSample = pSample;
	pReq->dwContext = dwContext;
	pReq->hr = hr;
	CAsyncReaderImpl_PostReply( This, pReq );

	return NOERROR;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnWaitForNext(IAsyncReader* iface,DWORD dwTimeout,IMediaSample** ppSample,DWORD_PTR* pdwContext)
{
	ICOM_THIS(CAsyncReaderImpl,iface);
	HRESULT hr = NOERROR;
	AsyncSourceRequest*	pReq;

	/*
	 * before implementing asynchronous I/O,
	 * please check patents by yourself
	 */
	WARN("(%p)->(%u,%p,%p) no async I/O\n",This,dwTimeout,ppSample,pdwContext);
	EnterCriticalSection( &This->m_csRequest );
	if ( This->m_bInFlushing )
		hr = VFW_E_TIMEOUT;
	LeaveCriticalSection( &This->m_csRequest );

	if ( hr == NOERROR )
	{
		pReq = CAsyncReaderImpl_GetReply(This);
		if ( pReq != NULL )
		{
			*ppSample = pReq->pSample;
			*pdwContext = pReq->dwContext;
			hr = pReq->hr;
			CAsyncReaderImpl_FreeRequest( This, pReq, FALSE );
		}
		else
		{
			hr = VFW_E_TIMEOUT;
		}
	}

	return hr;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnSyncReadAligned(IAsyncReader* iface,IMediaSample* pSample)
{
	ICOM_THIS(CAsyncReaderImpl,iface);
	HRESULT hr;
	REFERENCE_TIME	rtStart;
	REFERENCE_TIME	rtEnd;
	BYTE*	pData = NULL;
	LONGLONG	llStart;
	LONG	lLength;
	LONG	lActual;

	TRACE("(%p)->(%p)\n",This,pSample);

	hr = IMediaSample_GetPointer(pSample,&pData);
	if ( SUCCEEDED(hr) )
		hr = IMediaSample_GetTime(pSample,&rtStart,&rtEnd);
	if ( FAILED(hr) )
		return hr;

	llStart = rtStart / QUARTZ_TIMEUNITS;
	lLength = (LONG)(rtEnd / QUARTZ_TIMEUNITS - rtStart / QUARTZ_TIMEUNITS);
	lActual = 0;
	if ( lLength > IMediaSample_GetSize(pSample) )
	{
		FIXME("invalid length\n");
		return E_FAIL;
	}

	EnterCriticalSection( &This->m_csReader );
	hr = This->pSource->m_pHandler->pRead( This->pSource, llStart, lLength, pData, &lActual, (HANDLE)NULL );
	LeaveCriticalSection( &This->m_csReader );

	if ( hr == NOERROR )
	{
		hr = IMediaSample_SetActualDataLength(pSample,lActual);
		if ( hr == S_OK )
		{
			rtStart = llStart * QUARTZ_TIMEUNITS;
			rtEnd = (llStart + lActual) * QUARTZ_TIMEUNITS;
			hr = IMediaSample_SetTime(pSample,&rtStart,&rtEnd);
		}
		if ( hr == S_OK && lActual != lLength )
			hr = S_FALSE;
	}

	return hr;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnSyncRead(IAsyncReader* iface,LONGLONG llPosStart,LONG lLength,BYTE* pbBuf)
{
	ICOM_THIS(CAsyncReaderImpl,iface);
	HRESULT hr;
	LONG lActual;

	TRACE("(%p)->()\n",This);

	EnterCriticalSection( &This->m_csReader );
	hr = This->pSource->m_pHandler->pRead( This->pSource, llPosStart, lLength, pbBuf, &lActual, (HANDLE)NULL );
	LeaveCriticalSection( &This->m_csReader );

	if ( hr == S_OK && lLength != lActual )
		hr = S_FALSE;

	return hr;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnLength(IAsyncReader* iface,LONGLONG* pllTotal,LONGLONG* pllAvailable)
{
	ICOM_THIS(CAsyncReaderImpl,iface);
	HRESULT hr;

	TRACE("(%p)->()\n",This);

	hr = This->pSource->m_pHandler->pGetLength( This->pSource, pllTotal, pllAvailable );

	return hr;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnBeginFlush(IAsyncReader* iface)
{
	ICOM_THIS(CAsyncReaderImpl,iface);

	TRACE("(%p)->()\n",This);

	EnterCriticalSection( &This->m_csRequest );
	This->m_bInFlushing = TRUE;
	CAsyncReaderImpl_ReleaseReqList(This,&This->m_pReplyFirst,&This->m_pReplyLast,FALSE);
	LeaveCriticalSection( &This->m_csRequest );

	return NOERROR;
}

static HRESULT WINAPI
CAsyncReaderImpl_fnEndFlush(IAsyncReader* iface)
{
	ICOM_THIS(CAsyncReaderImpl,iface);

	TRACE("(%p)->()\n",This);

	EnterCriticalSection( &This->m_csRequest );
	This->m_bInFlushing = FALSE;
	LeaveCriticalSection( &This->m_csRequest );

	return NOERROR;
}


static ICOM_VTABLE(IAsyncReader) iasyncreader =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	CAsyncReaderImpl_fnQueryInterface,
	CAsyncReaderImpl_fnAddRef,
	CAsyncReaderImpl_fnRelease,

	/* IAsyncReader fields */
	CAsyncReaderImpl_fnRequestAllocator,
	CAsyncReaderImpl_fnRequest,
	CAsyncReaderImpl_fnWaitForNext,
	CAsyncReaderImpl_fnSyncReadAligned,
	CAsyncReaderImpl_fnSyncRead,
	CAsyncReaderImpl_fnLength,
	CAsyncReaderImpl_fnBeginFlush,
	CAsyncReaderImpl_fnEndFlush,
};

HRESULT CAsyncReaderImpl_InitIAsyncReader(
	CAsyncReaderImpl* This, IUnknown* punkControl,
	CAsyncSourceImpl* pSource )
{
	TRACE("(%p,%p)\n",This,punkControl);

	if ( punkControl == NULL )
	{
		ERR( "punkControl must not be NULL\n" );
		return E_INVALIDARG;
	}

	ICOM_VTBL(This) = &iasyncreader;
	This->punkControl = punkControl;
	This->pSource = pSource;
	This->m_bInFlushing = FALSE;
	This->m_pReplyFirst = NULL;
	This->m_pReplyLast = NULL;
	This->m_pFreeFirst = NULL;

	CRITICAL_SECTION_DEFINE( &This->m_csReader );
	CRITICAL_SECTION_DEFINE( &This->m_csRequest );
	CRITICAL_SECTION_DEFINE( &This->m_csReply );
	CRITICAL_SECTION_DEFINE( &This->m_csFree );

	return NOERROR;
}

void CAsyncReaderImpl_UninitIAsyncReader(
	CAsyncReaderImpl* This )
{
	TRACE("(%p) enter\n",This);

	CAsyncReaderImpl_ReleaseReqList(This,&This->m_pReplyFirst,&This->m_pReplyLast,TRUE);
	CAsyncReaderImpl_ReleaseReqList(This,&This->m_pFreeFirst,NULL,TRUE);

	DeleteCriticalSection( &This->m_csReader );
	DeleteCriticalSection( &This->m_csRequest );
	DeleteCriticalSection( &This->m_csReply );
	DeleteCriticalSection( &This->m_csFree );

	TRACE("(%p) leave\n",This);
}

/***************************************************************************
 *
 *	CFileSourceFilterImpl
 *
 */

static HRESULT WINAPI
CFileSourceFilterImpl_fnQueryInterface(IFileSourceFilter* iface,REFIID riid,void** ppobj)
{
	ICOM_THIS(CFileSourceFilterImpl,iface);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->punkControl,riid,ppobj);
}

static ULONG WINAPI
CFileSourceFilterImpl_fnAddRef(IFileSourceFilter* iface)
{
	ICOM_THIS(CFileSourceFilterImpl,iface);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->punkControl);
}

static ULONG WINAPI
CFileSourceFilterImpl_fnRelease(IFileSourceFilter* iface)
{
	ICOM_THIS(CFileSourceFilterImpl,iface);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->punkControl);
}

static HRESULT WINAPI
CFileSourceFilterImpl_fnLoad(IFileSourceFilter* iface,LPCOLESTR pFileName,const AM_MEDIA_TYPE* pmt)
{
	ICOM_THIS(CFileSourceFilterImpl,iface);
	HRESULT hr;

	TRACE("(%p)->(%s,%p)\n",This,debugstr_w(pFileName),pmt);

	if ( pFileName == NULL )
		return E_POINTER;

	if ( This->m_pwszFileName != NULL )
		return E_UNEXPECTED;

	This->m_cbFileName = sizeof(WCHAR)*(lstrlenW(pFileName)+1);
	This->m_pwszFileName = (WCHAR*)QUARTZ_AllocMem( This->m_cbFileName );
	if ( This->m_pwszFileName == NULL )
		return E_OUTOFMEMORY;
	memcpy( This->m_pwszFileName, pFileName, This->m_cbFileName );

	if ( pmt != NULL )
	{
		hr = QUARTZ_MediaType_Copy( &This->m_mt, pmt );
		if ( FAILED(hr) )
			goto err;
	}
	else
	{
		ZeroMemory( &This->m_mt, sizeof(AM_MEDIA_TYPE) );
		memcpy( &This->m_mt.majortype, &MEDIATYPE_Stream, sizeof(GUID) );
		memcpy( &This->m_mt.subtype, &MEDIASUBTYPE_NULL, sizeof(GUID) );
		This->m_mt.lSampleSize = 1;
		memcpy( &This->m_mt.formattype, &FORMAT_None, sizeof(GUID) );
	}

	hr = This->pSource->m_pHandler->pLoad( This->pSource, pFileName );
	if ( FAILED(hr) )
		goto err;

	This->pSource->pPin->pin.pmtAcceptTypes = &This->m_mt;
	This->pSource->pPin->pin.cAcceptTypes = 1;

	return NOERROR;
err:;
	return hr;
}

static HRESULT WINAPI
CFileSourceFilterImpl_fnGetCurFile(IFileSourceFilter* iface,LPOLESTR* ppFileName,AM_MEDIA_TYPE* pmt)
{
	ICOM_THIS(CFileSourceFilterImpl,iface);
	HRESULT hr = E_NOTIMPL;

	TRACE("(%p)->(%p,%p)\n",This,ppFileName,pmt);

	if ( ppFileName == NULL || pmt == NULL )
		return E_POINTER;

	if ( This->m_pwszFileName == NULL )
		return E_FAIL;

	hr = QUARTZ_MediaType_Copy( pmt, &This->m_mt );
	if ( FAILED(hr) )
		return hr;

	*ppFileName = (WCHAR*)CoTaskMemAlloc( This->m_cbFileName );
	if ( *ppFileName == NULL )
	{
		QUARTZ_MediaType_Free(pmt);
		ZeroMemory( pmt, sizeof(AM_MEDIA_TYPE) );
		return E_OUTOFMEMORY;
	}

	memcpy( *ppFileName, This->m_pwszFileName, This->m_cbFileName );

	return NOERROR;
}

static ICOM_VTABLE(IFileSourceFilter) ifilesource =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	CFileSourceFilterImpl_fnQueryInterface,
	CFileSourceFilterImpl_fnAddRef,
	CFileSourceFilterImpl_fnRelease,
	/* IFileSourceFilter fields */
	CFileSourceFilterImpl_fnLoad,
	CFileSourceFilterImpl_fnGetCurFile,
};

HRESULT CFileSourceFilterImpl_InitIFileSourceFilter(
	CFileSourceFilterImpl* This, IUnknown* punkControl,
	CAsyncSourceImpl* pSource,
	CRITICAL_SECTION* pcsFileSource )
{
	TRACE("(%p,%p)\n",This,punkControl);

	if ( punkControl == NULL )
	{
		ERR( "punkControl must not be NULL\n" );
		return E_INVALIDARG;
	}

	ICOM_VTBL(This) = &ifilesource;
	This->punkControl = punkControl;
	This->pSource = pSource;
	This->pcsFileSource = pcsFileSource;
	This->m_pwszFileName = NULL;
	This->m_cbFileName = 0;
	ZeroMemory( &This->m_mt, sizeof(AM_MEDIA_TYPE) );

	return NOERROR;
}

void CFileSourceFilterImpl_UninitIFileSourceFilter(
	CFileSourceFilterImpl* This )
{
	TRACE("(%p)\n",This);

	This->pSource->m_pHandler->pCleanup( This->pSource );
	if ( This->m_pwszFileName != NULL )
		QUARTZ_FreeMem( This->m_pwszFileName );
	QUARTZ_MediaType_Free( &This->m_mt );
}

/***************************************************************************
 *
 *	CAsyncSourcePinImpl methods
 *
 */


static HRESULT CAsyncSourcePinImpl_OnPreConnect( CPinBaseImpl* pImpl, IPin* pPin )
{
	CAsyncSourcePinImpl_THIS(pImpl,pin);

	TRACE("(%p,%p)\n",This,pPin);

	This->bAsyncReaderQueried = FALSE;

	return NOERROR;
}

static HRESULT CAsyncSourcePinImpl_OnPostConnect( CPinBaseImpl* pImpl, IPin* pPin )
{
	CAsyncSourcePinImpl_THIS(pImpl,pin);

	TRACE("(%p,%p)\n",This,pPin);

	if ( !This->bAsyncReaderQueried )
		return E_FAIL;

	return NOERROR;
}

static HRESULT CAsyncSourcePinImpl_OnDisconnect( CPinBaseImpl* pImpl )
{
	CAsyncSourcePinImpl_THIS(pImpl,pin);

	TRACE("(%p)\n",This);

	This->bAsyncReaderQueried = FALSE;

	return NOERROR;
}

static HRESULT CAsyncSourcePinImpl_CheckMediaType( CPinBaseImpl* pImpl, const AM_MEDIA_TYPE* pmt )
{
	CAsyncSourcePinImpl_THIS(pImpl,pin);

	TRACE("(%p,%p)\n",This,pmt);
	if ( pmt == NULL )
		return E_POINTER;

	if ( !IsEqualGUID( &pmt->majortype, &MEDIATYPE_Stream ) )
		return E_FAIL;

	return NOERROR;
}

static const CBasePinHandlers outputpinhandlers =
{
	CAsyncSourcePinImpl_OnPreConnect, /* pOnPreConnect */
	CAsyncSourcePinImpl_OnPostConnect, /* pOnPostConnect */
	CAsyncSourcePinImpl_OnDisconnect, /* pOnDisconnect */
	CAsyncSourcePinImpl_CheckMediaType, /* pCheckMediaType */
	NULL, /* pQualityNotify */
	NULL, /* pReceive */
	NULL, /* pReceiveCanBlock */
	NULL, /* pEndOfStream */
	NULL, /* pBeginFlush */
	NULL, /* pEndFlush */
	NULL, /* pNewSegment */
};

/***************************************************************************
 *
 *	CAsyncSourceImpl methods
 *
 */

static HRESULT CAsyncSourceImpl_OnStop( CBaseFilterImpl* pImpl )
{
	CAsyncSourceImpl_THIS(pImpl,basefilter);
	CAsyncReaderImpl*	pReader;

	TRACE( "(%p)\n", This );

	if ( This->pPin != NULL )
	{
		pReader = &This->pPin->async;
		CAsyncReaderImpl_ReleaseReqList(pReader,&pReader->m_pReplyFirst,&pReader->m_pReplyLast,FALSE);
	}

	return NOERROR;
}

static const CBaseFilterHandlers filterhandlers =
{
	NULL, /* pOnActive */
	NULL, /* pOnInactive */
	CAsyncSourceImpl_OnStop, /* pOnStop */
};

/***************************************************************************
 *
 *	new/delete CAsyncSourceImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry FilterIFEntries[] =
{
  { &IID_IPersist, offsetof(CAsyncSourceImpl,basefilter)-offsetof(CAsyncSourceImpl,unk) },
  { &IID_IMediaFilter, offsetof(CAsyncSourceImpl,basefilter)-offsetof(CAsyncSourceImpl,unk) },
  { &IID_IBaseFilter, offsetof(CAsyncSourceImpl,basefilter)-offsetof(CAsyncSourceImpl,unk) },
  { &IID_IFileSourceFilter, offsetof(CAsyncSourceImpl,filesrc)-offsetof(CAsyncSourceImpl,unk) },
};

static void QUARTZ_DestroyAsyncSource(IUnknown* punk)
{
	CAsyncSourceImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	if ( This->pPin != NULL )
	{
		IUnknown_Release(This->pPin->unk.punkControl);
		This->pPin = NULL;
	}

	This->m_pHandler->pCleanup( This );

	CFileSourceFilterImpl_UninitIFileSourceFilter(&This->filesrc);
	CBaseFilterImpl_UninitIBaseFilter(&This->basefilter);

	DeleteCriticalSection( &This->csFilter );
}

HRESULT QUARTZ_CreateAsyncSource(
	IUnknown* punkOuter,void** ppobj,
	const CLSID* pclsidAsyncSource,
	LPCWSTR pwszAsyncSourceName,
	LPCWSTR pwszOutPinName,
	const AsyncSourceHandlers* pHandler )
{
	CAsyncSourceImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p)\n",punkOuter,ppobj);

	This = (CAsyncSourceImpl*)
		QUARTZ_AllocObj( sizeof(CAsyncSourceImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;

	This->pPin = NULL;
	This->m_pHandler = pHandler;
	This->m_pUserData = NULL;

	QUARTZ_IUnkInit( &This->unk, punkOuter );

	hr = CBaseFilterImpl_InitIBaseFilter(
		&This->basefilter,
		This->unk.punkControl,
		pclsidAsyncSource,
		pwszAsyncSourceName,
		&filterhandlers );
	if ( SUCCEEDED(hr) )
	{
		/* construct this class. */
		hr = CFileSourceFilterImpl_InitIFileSourceFilter(
			&This->filesrc, This->unk.punkControl,
			This, &This->csFilter );
		if ( FAILED(hr) )
		{
			CBaseFilterImpl_UninitIBaseFilter(&This->basefilter);
		}
	}

	if ( FAILED(hr) )
	{
		QUARTZ_FreeObj(This);
		return hr;
	}

	This->unk.pEntries = FilterIFEntries;
	This->unk.dwEntries = sizeof(FilterIFEntries)/sizeof(FilterIFEntries[0]);
	This->unk.pOnFinalRelease = QUARTZ_DestroyAsyncSource;
	CRITICAL_SECTION_DEFINE( &This->csFilter );

	/* create the output pin. */
	hr = QUARTZ_CreateAsyncSourcePin(
		This, &This->csFilter,
		&This->pPin, pwszOutPinName );
	if ( SUCCEEDED(hr) )
		hr = QUARTZ_CompList_AddComp(
			This->basefilter.pOutPins,
			(IUnknown*)&(This->pPin->pin),
			NULL, 0 );

	if ( FAILED(hr) )
	{
		IUnknown_Release( This->unk.punkControl );
		return hr;
	}

	*ppobj = (void*)&(This->unk);

	return S_OK;
}

/***************************************************************************
 *
 *	new/delete CAsyncSourcePinImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry OutPinIFEntries[] =
{
  { &IID_IPin, offsetof(CAsyncSourcePinImpl,pin)-offsetof(CAsyncSourcePinImpl,unk) },
  /***{ &IID_IAsyncReader, offsetof(CAsyncSourcePinImpl,async)-offsetof(CAsyncSourcePinImpl,unk) },***/
};

static HRESULT CAsyncSourceImpl_OnQueryInterface(
	IUnknown* punk, const IID* piid, void** ppobj )
{
	CAsyncSourcePinImpl_THIS(punk,unk);

	if ( ppobj == NULL )
		return E_POINTER;
	
	if ( IsEqualGUID( &IID_IAsyncReader, piid ) )
	{
		TRACE("IAsyncReader has been queried.\n");
		*ppobj = (void*)&This->async;
		IUnknown_AddRef(punk);
		This->bAsyncReaderQueried = TRUE;
		return S_OK;
	}

	*ppobj = NULL;
	return E_NOINTERFACE;
}

static void QUARTZ_DestroyAsyncSourcePin(IUnknown* punk)
{
	CAsyncSourcePinImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	CAsyncReaderImpl_UninitIAsyncReader( &This->async );
	CPinBaseImpl_UninitIPin( &This->pin );
}

HRESULT QUARTZ_CreateAsyncSourcePin(
	CAsyncSourceImpl* pFilter,
	CRITICAL_SECTION* pcsPin,
	CAsyncSourcePinImpl** ppPin,
	LPCWSTR pwszPinName )
{
	CAsyncSourcePinImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p,%p)\n",pFilter,pcsPin,ppPin);

	This = (CAsyncSourcePinImpl*)
		QUARTZ_AllocObj( sizeof(CAsyncSourcePinImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;

	QUARTZ_IUnkInit( &This->unk, NULL );
	This->qiext.pNext = NULL;
	This->qiext.pOnQueryInterface = &CAsyncSourceImpl_OnQueryInterface;
	QUARTZ_IUnkAddDelegation( &This->unk, &This->qiext );

	This->bAsyncReaderQueried = FALSE;
	This->pSource = pFilter;

	hr = CPinBaseImpl_InitIPin(
		&This->pin,
		This->unk.punkControl,
		pcsPin, NULL,
		&pFilter->basefilter,
		pwszPinName,
		TRUE,
		&outputpinhandlers );

	if ( SUCCEEDED(hr) )
	{
		hr = CAsyncReaderImpl_InitIAsyncReader(
			&This->async,
			This->unk.punkControl,
			pFilter );
		if ( FAILED(hr) )
		{
			CPinBaseImpl_UninitIPin( &This->pin );
		}
	}

	if ( FAILED(hr) )
	{
		QUARTZ_FreeObj(This);
		return hr;
	}

	This->unk.pEntries = OutPinIFEntries;
	This->unk.dwEntries = sizeof(OutPinIFEntries)/sizeof(OutPinIFEntries[0]);
	This->unk.pOnFinalRelease = QUARTZ_DestroyAsyncSourcePin;

	*ppPin = This;

	TRACE("returned successfully.\n");

	return S_OK;
}



/***************************************************************************
 *
 *	Implements File Source.
 *
 */

typedef struct AsyncSourceFileImpl
{
	HANDLE	hFile;
	LONGLONG	llTotal;
} AsyncSourceFileImpl;


static HRESULT AsyncSourceFileImpl_Load( CAsyncSourceImpl* pImpl, LPCWSTR lpwszSourceName )
{
	AsyncSourceFileImpl*	This = (AsyncSourceFileImpl*)pImpl->m_pUserData;
	DWORD	dwLow;
	DWORD	dwHigh;

	if ( This != NULL )
		return E_UNEXPECTED;
	This = (AsyncSourceFileImpl*)QUARTZ_AllocMem( sizeof(AsyncSourceFileImpl) );
	pImpl->m_pUserData = (void*)This;
	if ( This == NULL )
		return E_OUTOFMEMORY;
	This->hFile = INVALID_HANDLE_VALUE;
	This->llTotal = 0;

	This->hFile = CreateFileW( lpwszSourceName,
		GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL );
	if ( This->hFile == INVALID_HANDLE_VALUE )
		return E_FAIL;

	SetLastError(NO_ERROR);
	dwLow = GetFileSize( This->hFile, &dwHigh );
	if ( dwLow == 0xffffffff && GetLastError() != NO_ERROR )
		return E_FAIL;

	This->llTotal = (LONGLONG)dwLow | ((LONGLONG)dwHigh << 32);

	return NOERROR;
}

static HRESULT AsyncSourceFileImpl_Cleanup( CAsyncSourceImpl* pImpl )
{
	AsyncSourceFileImpl*	This = (AsyncSourceFileImpl*)pImpl->m_pUserData;

	if ( This == NULL )
		return NOERROR;

	if ( This->hFile != INVALID_HANDLE_VALUE )
		CloseHandle(This->hFile);

	QUARTZ_FreeMem(This);
	pImpl->m_pUserData = NULL;

	return NOERROR;
}

static HRESULT AsyncSourceFileImpl_GetLength( CAsyncSourceImpl* pImpl, LONGLONG* pllTotal, LONGLONG* pllAvailable )
{
	AsyncSourceFileImpl*	This = (AsyncSourceFileImpl*)pImpl->m_pUserData;

	if ( This == NULL )
		return E_UNEXPECTED;

	*pllTotal = This->llTotal;
	*pllAvailable = This->llTotal;

	return NOERROR;
}

static HRESULT AsyncSourceFileImpl_Read( CAsyncSourceImpl* pImpl, LONGLONG llOfsStart, LONG lLength, BYTE* pBuf, LONG* plReturned, HANDLE hEventCancel )
{
	AsyncSourceFileImpl*	This = (AsyncSourceFileImpl*)pImpl->m_pUserData;
	LONG	lReturned;
	LONG	lBlock;
	LONG	lOfsLow;
	LONG	lOfsHigh;
	DWORD	dw;
	HRESULT hr = S_OK;

	if ( This == NULL || This->hFile == INVALID_HANDLE_VALUE )
		return E_UNEXPECTED;

	lReturned = 0;

	lOfsLow = (LONG)(llOfsStart & 0xffffffff);
	lOfsHigh = (LONG)(llOfsStart >> 32);
	SetLastError(NO_ERROR);
	lOfsLow = SetFilePointer( This->hFile, lOfsLow, &lOfsHigh, FILE_BEGIN );
	if ( lOfsLow == (LONG)0xffffffff && GetLastError() != NO_ERROR )
		return E_FAIL;

	while ( lLength > 0 )
	{
		if ( hEventCancel != (HANDLE)NULL &&
			 WaitForSingleObject( hEventCancel, 0 ) == WAIT_OBJECT_0 )
		{
			hr = S_FALSE;
			break;
		}

		lBlock = ( lLength > ASYNCSRC_FILE_BLOCKSIZE ) ?
			ASYNCSRC_FILE_BLOCKSIZE : lLength;

		if ( !ReadFile(This->hFile,pBuf,(DWORD)lBlock,&dw,NULL) )
		{
			hr = E_FAIL;
			break;
		}
		pBuf += dw;
		lReturned += (LONG)dw;
		lLength -= (LONG)dw;
		if ( lBlock > (LONG)dw )
			break;
	}

	*plReturned = lReturned;

	return hr;
}

static const struct AsyncSourceHandlers asyncsrc_file =
{
	AsyncSourceFileImpl_Load,
	AsyncSourceFileImpl_Cleanup,
	AsyncSourceFileImpl_GetLength,
	AsyncSourceFileImpl_Read,
};

HRESULT QUARTZ_CreateAsyncReader(IUnknown* punkOuter,void** ppobj)
{
	return QUARTZ_CreateAsyncSource(
		punkOuter, ppobj,
		&CLSID_AsyncReader,
		QUARTZ_wszAsyncFileSourceName,
		QUARTZ_wszAsyncFileSourcePinName,
		&asyncsrc_file );
}

/***************************************************************************
 *
 *	Implements URL Source.
 *
 */

typedef struct AsyncSourceURLImpl
{
	DWORD dwDummy;
} AsyncSourceURLImpl;


static HRESULT AsyncSourceURLImpl_Load( CAsyncSourceImpl* pImpl, LPCWSTR lpwszSourceName )
{
	AsyncSourceURLImpl*	This = (AsyncSourceURLImpl*)pImpl->m_pUserData;

	FIXME("(%p,%p) stub!\n", pImpl, lpwszSourceName);

	if ( This != NULL )
		return E_UNEXPECTED;
	This = (AsyncSourceURLImpl*)QUARTZ_AllocMem( sizeof(AsyncSourceURLImpl) );
	pImpl->m_pUserData = (void*)This;
	if ( This == NULL )
		return E_OUTOFMEMORY;

	return E_NOTIMPL;
}

static HRESULT AsyncSourceURLImpl_Cleanup( CAsyncSourceImpl* pImpl )
{
	AsyncSourceURLImpl*	This = (AsyncSourceURLImpl*)pImpl->m_pUserData;

	FIXME("(%p) stub!\n", This);

	if ( This == NULL )
		return NOERROR;

	QUARTZ_FreeMem(This);
	pImpl->m_pUserData = NULL;

	return NOERROR;
}

static HRESULT AsyncSourceURLImpl_GetLength( CAsyncSourceImpl* pImpl, LONGLONG* pllTotal, LONGLONG* pllAvailable )
{
	AsyncSourceURLImpl*	This = (AsyncSourceURLImpl*)pImpl->m_pUserData;

	FIXME("(%p,%p,%p) stub!\n", This, pllTotal, pllAvailable);

	if ( This == NULL )
		return E_UNEXPECTED;

	return E_NOTIMPL;
}

static HRESULT AsyncSourceURLImpl_Read( CAsyncSourceImpl* pImpl, LONGLONG llOfsStart, LONG lLength, BYTE* pBuf, LONG* plReturned, HANDLE hEventCancel )
{
	AsyncSourceURLImpl*	This = (AsyncSourceURLImpl*)pImpl->m_pUserData;

	FIXME("(%p) stub!\n", This);

	if ( This == NULL )
		return E_UNEXPECTED;

	return E_NOTIMPL;
}

static const struct AsyncSourceHandlers asyncsrc_url =
{
	AsyncSourceURLImpl_Load,
	AsyncSourceURLImpl_Cleanup,
	AsyncSourceURLImpl_GetLength,
	AsyncSourceURLImpl_Read,
};


HRESULT QUARTZ_CreateURLReader(IUnknown* punkOuter,void** ppobj)
{
	return QUARTZ_CreateAsyncSource(
		punkOuter, ppobj,
		&CLSID_URLReader,
		QUARTZ_wszAsyncURLSourceName,
		QUARTZ_wszAsyncURLSourcePinName,
		&asyncsrc_url );
}
