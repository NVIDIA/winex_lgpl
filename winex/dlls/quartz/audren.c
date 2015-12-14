/*
 * Audio Renderer (CLSID_AudioRender)
 *
 * FIXME
 *  - implement IReferenceClock.
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
#include "mmsystem.h"
#include "winerror.h"
#include "strmif.h"
#include "control.h"
#include "vfwmsgs.h"
#include "uuids.h"
#include "evcode.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "audren.h"
#include "seekpass.h"


static const WCHAR QUARTZ_AudioRender_Name[] =
{ 'A','u','d','i','o',' ','R','e','n','d','e','r',0 };
static const WCHAR QUARTZ_AudioRenderPin_Name[] =
{ 'I','n',0 };



/***************************************************************************
 *
 *	CAudioRendererImpl waveOut methods (internal)
 *
 */

static
HRESULT QUARTZ_HRESULT_From_MMRESULT( MMRESULT mr )
{
	HRESULT hr = E_FAIL;

	switch ( mr )
	{
	case MMSYSERR_NOERROR:
		hr = S_OK;
		break;
	case MMSYSERR_NOMEM:
		hr = E_OUTOFMEMORY;
		break;
	}

	return hr;
}

void CAudioRendererImpl_waveOutEventCallback(
	HWAVEOUT hwo, UINT uMsg,
	DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
	CAudioRendererImpl* This = (CAudioRendererImpl*)dwInstance;

	if ( uMsg == WOM_DONE )
		SetEvent( This->m_hEventRender );
}

static
void CAudioRendererImpl_waveOutReset(
	CAudioRendererImpl* This )
{
	if ( !This->m_fWaveOutInit )
		return;

	waveOutReset( This->m_hWaveOut );
	SetEvent( This->m_hEventRender );
}

static
void CAudioRendererImpl_waveOutUninit(
	CAudioRendererImpl* This )
{
	DWORD i;

	TRACE("(%p)\n",This);

	if ( !This->m_fWaveOutInit )
		return;

	waveOutReset( This->m_hWaveOut );
	SetEvent( This->m_hEventRender );

	for ( i = 0; i < WINE_QUARTZ_WAVEOUT_COUNT; i++ )
	{
		if ( This->m_hdr[i].dwFlags & WHDR_PREPARED )
		{
			waveOutUnprepareHeader(
				This->m_hWaveOut,
				&This->m_hdr[i], sizeof(WAVEHDR) );
			This->m_hdr[i].dwFlags = 0;
		}
		if ( This->m_hdr[i].lpData != NULL )
		{
			QUARTZ_FreeMem( This->m_hdr[i].lpData );
			This->m_hdr[i].lpData = NULL;
		}
	}

	waveOutClose( This->m_hWaveOut );
	This->m_hWaveOut = (HWAVEOUT)NULL;
	if ( This->m_hEventRender != (HANDLE)NULL )
	{
		CloseHandle( This->m_hEventRender );
		This->m_hEventRender = (HANDLE)NULL;
	}

	This->m_fWaveOutInit = FALSE;
}

static
HRESULT CAudioRendererImpl_waveOutInit(
	CAudioRendererImpl* This, WAVEFORMATEX* pwfx )
{
	MMRESULT mr;
	HRESULT hr;
	DWORD i;
	DWORD dwBlockSize;

	if ( This->m_fWaveOutInit )
		return NOERROR;

	if ( pwfx == NULL )
		return E_POINTER;
	if ( pwfx->nBlockAlign == 0 )
		return E_INVALIDARG;

	This->m_hEventRender = (HANDLE)NULL;
	This->m_hWaveOut = (HWAVEOUT)NULL;
	This->m_dwBlockSize = 0;
	This->m_phdrCur = NULL;
	ZeroMemory( &This->m_hdr, sizeof(This->m_hdr) );


	mr = waveOutOpen(
		&This->m_hWaveOut, WAVE_MAPPER, pwfx,
		(DWORD_PTR)CAudioRendererImpl_waveOutEventCallback, (DWORD_PTR)This,
		CALLBACK_FUNCTION );
	hr = QUARTZ_HRESULT_From_MMRESULT( mr );
	if ( FAILED(hr) )
		return hr;
	This->m_fWaveOutInit = TRUE;

	This->m_hEventRender = CreateEventA(
		NULL, TRUE, TRUE, NULL );
	if ( This->m_hEventRender == (HANDLE)NULL )
	{
		hr = E_OUTOFMEMORY;
		goto err;
	}

	dwBlockSize = pwfx->nAvgBytesPerSec / pwfx->nBlockAlign;
	if ( dwBlockSize == 0 )
		dwBlockSize = 1;
	dwBlockSize *= pwfx->nBlockAlign;
	This->m_dwBlockSize = dwBlockSize;

	for ( i = 0; i < WINE_QUARTZ_WAVEOUT_COUNT; i++ )
	{
		This->m_hdr[i].lpData = (CHAR*)QUARTZ_AllocMem( dwBlockSize );
		if ( This->m_hdr[i].lpData == NULL )
		{
			hr = E_OUTOFMEMORY;
			goto err;
		}
		mr = waveOutPrepareHeader(
				This->m_hWaveOut,
				&This->m_hdr[i], sizeof(WAVEHDR) );
		hr = QUARTZ_HRESULT_From_MMRESULT( mr );
		if ( FAILED(hr) )
			goto err;
		This->m_hdr[i].dwFlags |= WHDR_DONE;
		This->m_hdr[i].dwBufferLength = dwBlockSize;
		This->m_hdr[i].dwUser = i;
	}

	return S_OK;
err:
	CAudioRendererImpl_waveOutUninit(This);
	return hr;
}

static HRESULT CAudioRendererImpl_waveOutPause( CAudioRendererImpl* This )
{
	if ( !This->m_fWaveOutInit )
		return E_UNEXPECTED;

	return QUARTZ_HRESULT_From_MMRESULT( waveOutPause(
			This->m_hWaveOut ) );
}

static HRESULT CAudioRendererImpl_waveOutRun( CAudioRendererImpl* This )
{
	if ( !This->m_fWaveOutInit )
		return E_UNEXPECTED;

	return QUARTZ_HRESULT_From_MMRESULT( waveOutRestart(
			This->m_hWaveOut ) );
}

static
WAVEHDR* CAudioRendererImpl_waveOutGetBuffer(
	CAudioRendererImpl* This )
{
	DWORD i;

	if ( !This->m_fWaveOutInit )
		return NULL;

	if ( This->m_phdrCur != NULL )
		return This->m_phdrCur;

	for ( i = 0; i < WINE_QUARTZ_WAVEOUT_COUNT; i++ )
	{
		if ( This->m_hdr[i].dwFlags & WHDR_DONE )
		{
			This->m_phdrCur = &(This->m_hdr[i]);
			This->m_phdrCur->dwFlags &= ~WHDR_DONE;
			This->m_phdrCur->dwBufferLength = 0;
			return This->m_phdrCur;
		}
	}

	return NULL;
}

static
HRESULT CAudioRendererImpl_waveOutWriteData(
	CAudioRendererImpl* This,
	const BYTE* pData, DWORD cbData, DWORD* pcbWritten )
{
	DWORD	cbAvail;

	*pcbWritten = 0;

	if ( !This->m_fWaveOutInit )
		return E_UNEXPECTED;

	if ( cbData == 0 )
		return S_OK;

	if ( CAudioRendererImpl_waveOutGetBuffer(This) == NULL )
		return S_FALSE;

	cbAvail = This->m_dwBlockSize - This->m_phdrCur->dwBufferLength;
	if ( cbAvail > cbData )
		cbAvail = cbData;
	memcpy( This->m_phdrCur->lpData, pData, cbAvail );
	pData += cbAvail;
	cbData -= cbAvail;
	This->m_phdrCur->dwBufferLength += cbAvail;

	*pcbWritten = cbAvail;

	return S_OK;
}

static
HRESULT CAudioRendererImpl_waveOutFlush(
	CAudioRendererImpl* This )
{
	MMRESULT mr;
	HRESULT hr;

	if ( !This->m_fWaveOutInit )
		return E_UNEXPECTED;
	if ( This->m_phdrCur == NULL )
		return E_UNEXPECTED;

	if ( This->m_phdrCur->dwBufferLength == 0 )
		return S_OK;

	mr = waveOutWrite(
		This->m_hWaveOut,
		This->m_phdrCur, sizeof(WAVEHDR) );
	hr = QUARTZ_HRESULT_From_MMRESULT( mr );
	if ( FAILED(hr) )
		return hr;

	This->m_phdrCur = NULL;
	return S_OK;
}

#if 0
/* FIXME: Not used for now */

static
HRESULT CAudioRendererImpl_waveOutGetVolume(
	CAudioRendererImpl* This,
	DWORD* pdwLeft, DWORD* pdwRight )
{
	MMRESULT mr;
	HRESULT hr;
	DWORD	dwVol;

	if ( !This->m_fWaveOutInit )
		return E_UNEXPECTED;

	mr = waveOutGetVolume(
		This->m_hWaveOut, &dwVol );
	hr = QUARTZ_HRESULT_From_MMRESULT( mr );
	if ( FAILED(hr) )
		return hr;

	*pdwLeft = LOWORD(dwVol);
	*pdwRight = HIWORD(dwVol);

	return NOERROR;
}

#endif

static
HRESULT CAudioRendererImpl_waveOutSetVolume(
	CAudioRendererImpl* This,
	DWORD dwLeft, DWORD dwRight )
{
	MMRESULT mr;
	DWORD	dwVol;

	if ( !This->m_fWaveOutInit )
		return E_UNEXPECTED;

	dwVol = dwLeft | (dwRight<<16);

	mr = waveOutSetVolume(
		This->m_hWaveOut, dwVol );
	return QUARTZ_HRESULT_From_MMRESULT( mr );
}

static HRESULT CAudioRendererImpl_UpdateVolume( CAudioRendererImpl* This )
{
	HRESULT hr;
	long	leftlevel;
	long	rightlevel;

	if ( This->m_lAudioBalance >= 0 )
	{
		leftlevel = This->m_lAudioVolume - This->m_lAudioBalance;
		rightlevel = This->m_lAudioVolume;
	}
	else
	{
		leftlevel = This->m_lAudioVolume;
		rightlevel = This->m_lAudioVolume + This->m_lAudioBalance;
	}
	leftlevel = QUARTZ_DBToAmpFactor( leftlevel );
	rightlevel = QUARTZ_DBToAmpFactor( rightlevel );

	hr = CAudioRendererImpl_waveOutSetVolume(
		This, (DWORD)leftlevel, (DWORD)rightlevel );
	if ( hr == E_UNEXPECTED )
		hr = S_OK;

	return hr;
}


/***************************************************************************
 *
 *	CAudioRendererImpl methods
 *
 */


static HRESULT CAudioRendererImpl_OnActive( CBaseFilterImpl* pImpl )
{
	CAudioRendererImpl_THIS(pImpl,basefilter);
	HRESULT hr;

	TRACE( "(%p)\n", This );

	if ( This->pPin->pin.pmtConn == NULL )
		return NOERROR;

	This->m_fInFlush = FALSE;

	hr = CAudioRendererImpl_waveOutRun(This);
	if ( FAILED(hr) )
		return hr;

	return NOERROR;
}

static HRESULT CAudioRendererImpl_OnInactive( CBaseFilterImpl* pImpl )
{
	CAudioRendererImpl_THIS(pImpl,basefilter);
	WAVEFORMATEX*	pwfx;
	HRESULT hr;

	TRACE( "(%p)\n", This );

	if ( This->pPin->pin.pmtConn == NULL )
		return NOERROR;

	pwfx = (WAVEFORMATEX*)This->pPin->pin.pmtConn->pbFormat;
	if ( pwfx == NULL )
		return E_FAIL;

	This->m_fInFlush = FALSE;

	hr = CAudioRendererImpl_waveOutInit(This,pwfx);
	if ( FAILED(hr) )
		return hr;

	hr = CAudioRendererImpl_waveOutPause(This);
	if ( FAILED(hr) )
		return hr;

	return NOERROR;
}

static HRESULT CAudioRendererImpl_OnStop( CBaseFilterImpl* pImpl )
{
	CAudioRendererImpl_THIS(pImpl,basefilter);

	TRACE( "(%p)\n", This );

	This->m_fInFlush = TRUE;

	CAudioRendererImpl_waveOutUninit(This);

	This->m_fInFlush = FALSE;

	TRACE("returned\n" );

	return NOERROR;
}

static const CBaseFilterHandlers filterhandlers =
{
	CAudioRendererImpl_OnActive, /* pOnActive */
	CAudioRendererImpl_OnInactive, /* pOnInactive */
	CAudioRendererImpl_OnStop, /* pOnStop */
};

/***************************************************************************
 *
 *	CAudioRendererPinImpl methods
 *
 */

static HRESULT CAudioRendererPinImpl_OnDisconnect( CPinBaseImpl* pImpl )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);

	TRACE("(%p)\n",This);

	if ( This->meminput.pAllocator != NULL )
	{
		IMemAllocator_Decommit(This->meminput.pAllocator);
		IMemAllocator_Release(This->meminput.pAllocator);
		This->meminput.pAllocator = NULL;
	}

	return NOERROR;
}


static HRESULT CAudioRendererPinImpl_CheckMediaType( CPinBaseImpl* pImpl, const AM_MEDIA_TYPE* pmt )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);
	const WAVEFORMATEX* pwfx;

	TRACE("(%p,%p)\n",This,pmt);

	if ( !IsEqualGUID( &pmt->majortype, &MEDIATYPE_Audio ) )
	{
		TRACE("not audio\n");
		return E_FAIL;
	}
	if ( !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_NULL ) &&
		 !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_PCM ) )
	{
		TRACE("not PCM\n");
		return E_FAIL;
	}
	if ( !IsEqualGUID( &pmt->formattype, &FORMAT_WaveFormatEx ) )
	{
		TRACE("not WAVE\n");
		return E_FAIL;
	}

	TRACE("testing WAVE header\n");

	if ( pmt->cbFormat < (sizeof(WAVEFORMATEX)-sizeof(WORD)) )
		return E_FAIL;

	pwfx = (const WAVEFORMATEX*)pmt->pbFormat;
	if ( pwfx == NULL )
		return E_FAIL;
	if ( pwfx->wFormatTag != 1 )
		return E_FAIL;

	TRACE("returned successfully.\n");

	return NOERROR;
}

static HRESULT CAudioRendererPinImpl_Receive( CPinBaseImpl* pImpl, IMediaSample* pSample )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);
	HRESULT hr;
	BYTE*	pData = NULL;
	DWORD	dwDataLength;
	DWORD	dwWritten;

	TRACE( "(%p,%p)\n",This,pSample );

	if ( !This->pRender->m_fWaveOutInit )
		return E_UNEXPECTED;
	if ( pSample == NULL )
		return E_POINTER;

	hr = IMediaSample_GetPointer( pSample, &pData );
	if ( FAILED(hr) )
		return hr;
	dwDataLength = (DWORD)IMediaSample_GetActualDataLength( pSample );

	while ( 1 )
	{
		TRACE("trying to write %u bytes\n",dwDataLength);

		if ( This->pRender->m_fInFlush )
			return S_FALSE;

		ResetEvent( This->pRender->m_hEventRender );
		hr = CAudioRendererImpl_waveOutWriteData(
			This->pRender,pData,dwDataLength,&dwWritten);
		if ( FAILED(hr) )
			break;
		if ( hr == S_FALSE )
		{
			WaitForSingleObject( This->pRender->m_hEventRender, INFINITE );
			continue;
		}
		pData += dwWritten;
		dwDataLength -= dwWritten;
		hr = CAudioRendererImpl_waveOutFlush(This->pRender);
		if ( FAILED(hr) )
			break;
		if ( dwDataLength == 0 )
			break;
	}

	return hr;
}

static HRESULT CAudioRendererPinImpl_ReceiveCanBlock( CPinBaseImpl* pImpl )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	/* might block. */
	return S_OK;
}

static HRESULT CAudioRendererPinImpl_EndOfStream( CPinBaseImpl* pImpl )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);

	FIXME( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;

	/* FIXME - don't notify twice until stopped or seeked. */
	return CBaseFilterImpl_MediaEventNotify(
		&This->pRender->basefilter, EC_COMPLETE,
		(LONG_PTR)S_OK, (LONG_PTR)(IBaseFilter*)(This->pRender) );
}

static HRESULT CAudioRendererPinImpl_BeginFlush( CPinBaseImpl* pImpl )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	This->pRender->m_fInFlush = TRUE;
	CAudioRendererImpl_waveOutReset(This->pRender);

	return NOERROR;
}

static HRESULT CAudioRendererPinImpl_EndFlush( CPinBaseImpl* pImpl )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;

	return NOERROR;
}

static HRESULT CAudioRendererPinImpl_NewSegment( CPinBaseImpl* pImpl, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double rate )
{
	CAudioRendererPinImpl_THIS(pImpl,pin);

	FIXME( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;

	return NOERROR;
}

static const CBasePinHandlers pinhandlers =
{
	NULL, /* pOnPreConnect */
	NULL, /* pOnPostConnect */
	CAudioRendererPinImpl_OnDisconnect, /* pOnDisconnect */
	CAudioRendererPinImpl_CheckMediaType, /* pCheckMediaType */
	NULL, /* pQualityNotify */
	CAudioRendererPinImpl_Receive, /* pReceive */
	CAudioRendererPinImpl_ReceiveCanBlock, /* pReceiveCanBlock */
	CAudioRendererPinImpl_EndOfStream, /* pEndOfStream */
	CAudioRendererPinImpl_BeginFlush, /* pBeginFlush */
	CAudioRendererPinImpl_EndFlush, /* pEndFlush */
	CAudioRendererPinImpl_NewSegment, /* pNewSegment */
};


/***************************************************************************
 *
 *	new/delete CAudioRendererImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry FilterIFEntries[] =
{
  { &IID_IPersist, offsetof(CAudioRendererImpl,basefilter)-offsetof(CAudioRendererImpl,unk) },
  { &IID_IMediaFilter, offsetof(CAudioRendererImpl,basefilter)-offsetof(CAudioRendererImpl,unk) },
  { &IID_IBaseFilter, offsetof(CAudioRendererImpl,basefilter)-offsetof(CAudioRendererImpl,unk) },
  { &IID_IBasicAudio, offsetof(CAudioRendererImpl,basaud)-offsetof(CAudioRendererImpl,unk) },
};

static HRESULT CAudioRendererImpl_OnQueryInterface(
	IUnknown* punk, const IID* piid, void** ppobj )
{
	CAudioRendererImpl_THIS(punk,unk);

	if ( ppobj == NULL )
		return E_POINTER;
	*ppobj = NULL;

	if ( This->pSeekPass == NULL )
		return E_NOINTERFACE;

	if ( IsEqualGUID( &IID_IMediaPosition, piid ) ||
		 IsEqualGUID( &IID_IMediaSeeking, piid ) )
	{
		TRACE( "IMediaSeeking(or IMediaPosition) is queried\n" );
		return IUnknown_QueryInterface( (IUnknown*)(&This->pSeekPass->unk), piid, ppobj );
	}

	return E_NOINTERFACE;
}

static void QUARTZ_DestroyAudioRenderer(IUnknown* punk)
{
	CAudioRendererImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	if ( This->pPin != NULL )
	{
		IUnknown_Release(This->pPin->unk.punkControl);
		This->pPin = NULL;
	}
	if ( This->pSeekPass != NULL )
	{
		IUnknown_Release((IUnknown*)&This->pSeekPass->unk);
		This->pSeekPass = NULL;
	}

	CAudioRendererImpl_UninitIBasicAudio(This);
	CBaseFilterImpl_UninitIBaseFilter(&This->basefilter);

	DeleteCriticalSection( &This->m_csReceive );
}

HRESULT QUARTZ_CreateAudioRenderer(IUnknown* punkOuter,void** ppobj)
{
	CAudioRendererImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p)\n",punkOuter,ppobj);

	This = (CAudioRendererImpl*)
		QUARTZ_AllocObj( sizeof(CAudioRendererImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;
	This->pSeekPass = NULL;
	This->pPin = NULL;
	This->m_fInFlush = FALSE;
	This->m_lAudioVolume = 0;
	This->m_lAudioBalance = 0;
	This->m_fWaveOutInit = FALSE;
	This->m_hEventRender = (HANDLE)NULL;

	QUARTZ_IUnkInit( &This->unk, punkOuter );
	This->qiext.pNext = NULL;
	This->qiext.pOnQueryInterface = &CAudioRendererImpl_OnQueryInterface;
	QUARTZ_IUnkAddDelegation( &This->unk, &This->qiext );

	hr = CBaseFilterImpl_InitIBaseFilter(
		&This->basefilter,
		This->unk.punkControl,
		&CLSID_AudioRender,
		QUARTZ_AudioRender_Name,
		&filterhandlers );
	if ( SUCCEEDED(hr) )
	{
		hr = CAudioRendererImpl_InitIBasicAudio(This);
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
	This->unk.pOnFinalRelease = QUARTZ_DestroyAudioRenderer;

	CRITICAL_SECTION_DEFINE( &This->m_csReceive );

	hr = QUARTZ_CreateAudioRendererPin(
		This,
		&This->basefilter.csFilter,
		&This->m_csReceive,
		&This->pPin );
	if ( SUCCEEDED(hr) )
		hr = QUARTZ_CompList_AddComp(
			This->basefilter.pInPins,
			(IUnknown*)&This->pPin->pin,
			NULL, 0 );
	if ( SUCCEEDED(hr) )
		hr = QUARTZ_CreateSeekingPassThruInternal(
			(IUnknown*)&(This->unk), &This->pSeekPass,
			TRUE, (IPin*)&(This->pPin->pin) );

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
 *	new/delete CAudioRendererPinImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry PinIFEntries[] =
{
  { &IID_IPin, offsetof(CAudioRendererPinImpl,pin)-offsetof(CAudioRendererPinImpl,unk) },
  { &IID_IMemInputPin, offsetof(CAudioRendererPinImpl,meminput)-offsetof(CAudioRendererPinImpl,unk) },
};

static void QUARTZ_DestroyAudioRendererPin(IUnknown* punk)
{
	CAudioRendererPinImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	CPinBaseImpl_UninitIPin( &This->pin );
	CMemInputPinBaseImpl_UninitIMemInputPin( &This->meminput );
}

HRESULT QUARTZ_CreateAudioRendererPin(
	CAudioRendererImpl* pFilter,
	CRITICAL_SECTION* pcsPin,
	CRITICAL_SECTION* pcsPinReceive,
	CAudioRendererPinImpl** ppPin)
{
	CAudioRendererPinImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p,%p,%p)\n",pFilter,pcsPin,pcsPinReceive,ppPin);

	This = (CAudioRendererPinImpl*)
		QUARTZ_AllocObj( sizeof(CAudioRendererPinImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;

	QUARTZ_IUnkInit( &This->unk, NULL );
	This->pRender = pFilter;

	hr = CPinBaseImpl_InitIPin(
		&This->pin,
		This->unk.punkControl,
		pcsPin, pcsPinReceive,
		&pFilter->basefilter,
		QUARTZ_AudioRenderPin_Name,
		FALSE,
		&pinhandlers );

	if ( SUCCEEDED(hr) )
	{
		hr = CMemInputPinBaseImpl_InitIMemInputPin(
			&This->meminput,
			This->unk.punkControl,
			&This->pin );
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

	This->unk.pEntries = PinIFEntries;
	This->unk.dwEntries = sizeof(PinIFEntries)/sizeof(PinIFEntries[0]);
	This->unk.pOnFinalRelease = QUARTZ_DestroyAudioRendererPin;

	*ppPin = This;

	TRACE("returned successfully.\n");

	return S_OK;
}


/***************************************************************************
 *
 *	CAudioRendererImpl::IBasicAudio
 *
 */

static HRESULT WINAPI
IBasicAudio_fnQueryInterface(IBasicAudio* iface,REFIID riid,void** ppobj)
{
	CAudioRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IBasicAudio_fnAddRef(IBasicAudio* iface)
{
	CAudioRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IBasicAudio_fnRelease(IBasicAudio* iface)
{
	CAudioRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IBasicAudio_fnGetTypeInfoCount(IBasicAudio* iface,UINT* pcTypeInfo)
{
	CAudioRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicAudio_fnGetTypeInfo(IBasicAudio* iface,UINT iTypeInfo, LCID lcid, ITypeInfo** ppobj)
{
	CAudioRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicAudio_fnGetIDsOfNames(IBasicAudio* iface,REFIID riid, LPOLESTR* ppwszName, UINT cNames, LCID lcid, DISPID* pDispId)
{
	CAudioRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicAudio_fnInvoke(IBasicAudio* iface,DISPID DispId, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarRes, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	CAudioRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}


static HRESULT WINAPI
IBasicAudio_fnput_Volume(IBasicAudio* iface,long lVol)
{
	CAudioRendererImpl_THIS(iface,basaud);
	HRESULT hr;

	TRACE("(%p)->(%ld)\n",This,lVol);

	if ( lVol > 0 || lVol < -10000 )
		return E_INVALIDARG;

	EnterCriticalSection( &This->basefilter.csFilter );
	This->m_lAudioVolume = lVol;
	hr = CAudioRendererImpl_UpdateVolume( This );
	LeaveCriticalSection( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IBasicAudio_fnget_Volume(IBasicAudio* iface,long* plVol)
{
	CAudioRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->(%p)\n",This,plVol);

	if ( plVol == NULL )
		return E_POINTER;

	EnterCriticalSection( &This->basefilter.csFilter );
	*plVol = This->m_lAudioVolume;
	LeaveCriticalSection( &This->basefilter.csFilter );

	return S_OK;
}

static HRESULT WINAPI
IBasicAudio_fnput_Balance(IBasicAudio* iface,long lBalance)
{
	CAudioRendererImpl_THIS(iface,basaud);
	HRESULT hr;

	TRACE("(%p)->(%ld)\n",This,lBalance);

	if ( lBalance > 0 || lBalance < -10000 )
		return E_INVALIDARG;

	EnterCriticalSection( &This->basefilter.csFilter );
	This->m_lAudioBalance = lBalance;
	hr = CAudioRendererImpl_UpdateVolume( This );
	LeaveCriticalSection( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IBasicAudio_fnget_Balance(IBasicAudio* iface,long* plBalance)
{
	CAudioRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->(%p)\n",This,plBalance);

	if ( plBalance == NULL )
		return E_POINTER;

	EnterCriticalSection( &This->basefilter.csFilter );
	*plBalance = This->m_lAudioBalance;
	LeaveCriticalSection( &This->basefilter.csFilter );

	return S_OK;
}


static ICOM_VTABLE(IBasicAudio) ibasicaudio =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IBasicAudio_fnQueryInterface,
	IBasicAudio_fnAddRef,
	IBasicAudio_fnRelease,
	/* IDispatch fields */
	IBasicAudio_fnGetTypeInfoCount,
	IBasicAudio_fnGetTypeInfo,
	IBasicAudio_fnGetIDsOfNames,
	IBasicAudio_fnInvoke,
	/* IBasicAudio fields */
	IBasicAudio_fnput_Volume,
	IBasicAudio_fnget_Volume,
	IBasicAudio_fnput_Balance,
	IBasicAudio_fnget_Balance,
};


HRESULT CAudioRendererImpl_InitIBasicAudio( CAudioRendererImpl* This )
{
	TRACE("(%p)\n",This);
	ICOM_VTBL(&This->basaud) = &ibasicaudio;

	return NOERROR;
}

void CAudioRendererImpl_UninitIBasicAudio( CAudioRendererImpl* This )
{
	TRACE("(%p)\n",This);
}

