/*
 * DSound Audio Renderer (CLSID_DSoundRender)
 *
 * FIXME
 *  - implement IReferenceClock.
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
#include "audren2.h"
#include "seekpass.h"
#include "sysclock.h"


static const WCHAR QUARTZ_DSoundRender_Name[] =
{ 'D','S','o','u','n','d',' ','R','e','n','d','e','r',0 };
static const WCHAR QUARTZ_DSoundRenderPin_Name[] =
{ 'I','n',0 };



/***************************************************************************
 *
 *	CDSoundRendererImpl dsound methods (internal)
 *
 */

static
void CDSoundRendererImpl_dsoundReset(
	CDSoundRendererImpl* This )
{
	if ( !This->m_dsbuffer )
		return;

	IDirectSoundBuffer_Stop(This->m_dsbuffer);
	SetEvent( This->m_hEventRender );
}

static
void CDSoundRendererImpl_dsoundUninit(
	CDSoundRendererImpl* This )
{
	TRACE("(%p)\n",This);

	if ( !This->m_dsbuffer )
		return;

	IDirectSoundBuffer_Stop(This->m_dsbuffer);

	SetEvent( This->m_hEventRender );

	IDirectSoundBuffer_Release(This->m_dsbuffer); This->m_dsbuffer = NULL;
	IDirectSoundBuffer_SetFormat(This->m_dsprimary, &This->m_wforig);
	IDirectSoundBuffer_Release(This->m_dsprimary); This->m_dsprimary = NULL;
	IDirectSound_Release(This->m_dsound); This->m_dsound = NULL;

	if ( This->m_hEventRender != (HANDLE)NULL )
	{
		CloseHandle( This->m_hEventRender );
		This->m_hEventRender = (HANDLE)NULL;
	}
}

static
HRESULT CDSoundRendererImpl_dsoundInit(
	CDSoundRendererImpl* This, WAVEFORMATEX* pwfx )
{
	HRESULT hr;
	DSBUFFERDESC desc;
	LPVOID data;
	DWORD len;

	if ( This->m_dsbuffer )
		return NOERROR;

	if ( pwfx == NULL )
		return E_POINTER;
	if ( pwfx->nBlockAlign == 0 )
		return E_INVALIDARG;

	This->m_hEventRender = (HANDLE)NULL;
	This->m_bufsize = 0;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);

	hr = DirectSoundCreate(NULL, &This->m_dsound, NULL);
	if ( FAILED(hr) )
		return hr;
	/* create primary buffer */
	desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	hr = IDirectSound_CreateSoundBuffer(This->m_dsound, &desc, &This->m_dsprimary, NULL);
	if ( SUCCEEDED(hr) )
		hr = IDirectSoundBuffer_GetFormat(This->m_dsprimary, &This->m_wforig, sizeof(This->m_wforig), NULL);
	if ( FAILED(hr) ) {
		IDirectSound_Release(This->m_dsound); This->m_dsound = NULL;
		return hr;
	}
	/* create play buffer */
	desc.dwFlags = DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2;
	desc.dwBufferBytes = pwfx->nAvgBytesPerSec * 4;
	desc.lpwfxFormat = pwfx;
	hr = IDirectSound_CreateSoundBuffer(This->m_dsound, &desc, &This->m_dsbuffer, NULL);
	if ( FAILED(hr) ) {
		IDirectSoundBuffer_Release(This->m_dsprimary); This->m_dsprimary = NULL;
		IDirectSound_Release(This->m_dsound); This->m_dsound = NULL;
		return hr;
	}

	/* set the format of the primary buffer to match the audio stream */
	if (IDirectSoundBuffer_SetFormat(This->m_dsprimary, pwfx) == DSERR_PRIOLEVELNEEDED) {
		/* if too low priority, increase it and try again */
		IDirectSound_SetCooperativeLevel(This->m_dsound, 0, DSSCL_PRIORITY);
		IDirectSoundBuffer_SetFormat(This->m_dsprimary, pwfx);
	}

	/* set volume and pan */
	IDirectSoundBuffer_SetVolume(This->m_dsbuffer, This->m_lAudioVolume);
	IDirectSoundBuffer_SetPan(This->m_dsbuffer, This->m_lAudioBalance);

	/* clean the play buffer before we begin */
	IDirectSoundBuffer_Lock(This->m_dsbuffer, 0, 0, &data, &len, NULL, NULL, DSBLOCK_ENTIREBUFFER);
	memset(data, 0, len);
	IDirectSoundBuffer_Unlock(This->m_dsbuffer, data, len, NULL, 0);

	This->m_wpos = 0;
	This->m_wlen = 0;
	This->m_ppos = 0;
	This->m_bufsize = desc.dwBufferBytes;

	This->m_hEventRender = CreateEventA(
		NULL, TRUE, TRUE, NULL );
	if ( This->m_hEventRender == (HANDLE)NULL )
	{
		hr = E_OUTOFMEMORY;
		goto err;
	}

	return S_OK;
err:
	CDSoundRendererImpl_dsoundUninit(This);
	return hr;
}

static HRESULT CDSoundRendererImpl_dsoundRun( CDSoundRendererImpl* This )
{
	if ( !This->m_dsbuffer )
		return E_UNEXPECTED;

	return IDirectSoundBuffer_Play( This->m_dsbuffer, 0, 0, DSBPLAY_LOOPING);
}

static
HRESULT CDSoundRendererImpl_dsoundWriteData(
	CDSoundRendererImpl* This,
	const BYTE* pData, DWORD cbData, DWORD* pcbWritten )
{
	DWORD	dwPlay, dwWrite, dwP, dwM, dwW;
	DWORD	cbAvail;
	LPBYTE	data1, data2;
	DWORD	len1, len2;

	*pcbWritten = 0;

	if ( !This->m_dsbuffer )
		return E_UNEXPECTED;

	if ( cbData == 0 )
		return S_OK;

	/* update play position */
	EnterCriticalSection(&This->m_csTime);
	IDirectSoundBuffer_GetCurrentPosition(This->m_dsbuffer, &dwPlay, &dwWrite);
	dwP = This->m_ppos;
	dwM = ((dwPlay < dwP) ? This->m_bufsize : 0) + dwPlay - dwP;
	dwW = This->m_wlen;
	if (dwM > dwW) {
		if (This->m_recover && dwM > This->m_bufsize / 2) {
			/* still not recovered */
			dwM = 0;
			/* leave This->m_ppos pointing to writepos set below */
		}
		else {
			if (dwW) ERR("underrun\n"); /* else it's the first sample */
			This->m_wlen = 0;
#if 1			/* skip to writepos */
			This->m_wpos = dwWrite;
			This->m_ppos = dwWrite;
			This->m_recover = TRUE;
#endif
		}
	}
	else {
		This->m_ppos = dwPlay;
		This->m_wlen -= dwM;
		This->m_recover = FALSE;
	}
	LeaveCriticalSection(&This->m_csTime);

	TRACE("played %u, pos=%u, ppos=%u, wlen=%u (new %u)\n", dwM, dwPlay, dwP, dwW, This->m_wlen);
	if (dwM > dwW) {
#if 1
		/* clear the skipped area */
		dwW = ((dwWrite < dwP) ? This->m_bufsize : 0) + dwWrite - dwP;
		IDirectSoundBuffer_Lock(This->m_dsbuffer, dwP, dwW, &data1, &len1, &data2, &len2, 0);
		memset(data1, 0, len1);
		if (data2) memset(data2, 0, len2);
		IDirectSoundBuffer_Unlock(This->m_dsbuffer, data1, len1, data2, len2);
#endif
	}
	else if (dwM > 0) {
		/* clear the played area */
		IDirectSoundBuffer_Lock(This->m_dsbuffer, dwP, dwM, &data1, &len1, &data2, &len2, 0);
		memset(data1, 0, len1);
		if (data2) memset(data2, 0, len2);
		IDirectSoundBuffer_Unlock(This->m_dsbuffer, data1, len1, data2, len2);
	}

	cbAvail = ((dwPlay < This->m_wpos) ? This->m_bufsize : 0 ) + dwPlay - This->m_wpos;
	if (!cbAvail && !This->m_wlen) cbAvail = This->m_bufsize;

	if ( !cbAvail )
		return S_FALSE;

	if ( cbAvail > cbData )
		cbAvail = cbData;

	IDirectSoundBuffer_Lock(This->m_dsbuffer, This->m_wpos, cbAvail, &data1, &len1, &data2, &len2, 0);
	if ( pData ) {
		memcpy(data1, pData, len1 );
		if (data2) memcpy(data2, pData+len1, len2 );
	}
	else {
		memset(data1, 0, len1 );
		if (data2) memset(data2, 0, len2 );
	}
	IDirectSoundBuffer_Unlock(This->m_dsbuffer, data1, len1, data2, len2);

	dwW = This->m_wpos + cbAvail;
	while (dwW > This->m_bufsize) dwW -= This->m_bufsize;
	EnterCriticalSection(&This->m_csTime);
	This->m_wpos = dwW;
	This->m_wlen += cbAvail;
	LeaveCriticalSection(&This->m_csTime);
	TRACE("written %u, wpos=%u, wlen=%u\n", cbAvail, This->m_wpos, This->m_wlen);
	*pcbWritten = cbAvail;

	return S_OK;
}

static HRESULT CDSoundRendererImpl_GetTime( IUnknown* punk, REFERENCE_TIME* prtTime )
{
	CDSoundRendererImpl_THIS(punk,unk);
	WAVEFORMATEX*	pwfx;
	LONGLONG played;
	DWORD dwPlay;
	REFERENCE_TIME rtTime;

	if ( !This->m_dsbuffer ) {
		*prtTime = 0;
		return S_OK;
	}

	pwfx = (WAVEFORMATEX*)This->pPin->pin.pmtConn->pbFormat;
	if ( pwfx == NULL )
		return E_FAIL;

	EnterCriticalSection(&This->m_csTime);

	IDirectSoundBuffer_GetCurrentPosition(This->m_dsbuffer, &dwPlay, NULL);
	if (dwPlay > This->m_SavedPlayPos)
	{
		This->m_BytesPlayed += (dwPlay - This->m_SavedPlayPos);
	}
	else if (dwPlay < This->m_SavedPlayPos)
	{
		This->m_BytesPlayed += (This->m_bufsize - This->m_SavedPlayPos) + dwPlay;
		TRACE("current position %u is less than saved position %u. Adjusting bytes played to %lld.\n", dwPlay, This->m_SavedPlayPos, This->m_BytesPlayed);
	}
	This->m_SavedPlayPos = dwPlay;
	rtTime = (This->m_BytesPlayed * (REFERENCE_TIME)QUARTZ_TIMEUNITS / (REFERENCE_TIME)pwfx->nAvgBytesPerSec) + This->basefilter.rtStart;

	played = This->m_BytesPlayed;
	if (rtTime < This->m_rtLastTime) rtTime = This->m_rtLastTime;
	This->m_rtLastTime = rtTime;
	LeaveCriticalSection(&This->m_csTime);

	*prtTime = rtTime;

	TRACE("total played %lld, returning %lld\n", played, *prtTime);

	return S_OK;
}

static HRESULT CDSoundRendererImpl_UpdateVolume( CDSoundRendererImpl* This )
{
	if (This->m_dsbuffer)
		return IDirectSoundBuffer_SetVolume(This->m_dsbuffer, This->m_lAudioVolume);
	else
		return DS_OK;
}

static HRESULT CDSoundRendererImpl_UpdateBalance( CDSoundRendererImpl* This )
{
	if (This->m_dsbuffer)
		return IDirectSoundBuffer_SetPan(This->m_dsbuffer, This->m_lAudioBalance);
	else
		return DS_OK;
}



/***************************************************************************
 *
 *	CDSoundRendererImpl methods
 *
 */


static HRESULT CDSoundRendererImpl_OnActive( CBaseFilterImpl* pImpl )
{
	CDSoundRendererImpl_THIS(pImpl,basefilter);
	HRESULT hr;

	TRACE( "(%p)\n", This );

	if ( This->pPin->pin.pmtConn == NULL )
		return NOERROR;

	This->m_fInFlush = FALSE;

	hr = CDSoundRendererImpl_dsoundRun(This);
	if ( FAILED(hr) )
		return hr;

	return NOERROR;
}

static HRESULT CDSoundRendererImpl_OnInactive( CBaseFilterImpl* pImpl )
{
	CDSoundRendererImpl_THIS(pImpl,basefilter);
	WAVEFORMATEX*	pwfx;
	HRESULT hr;

	TRACE( "(%p)\n", This );

	if ( This->pPin->pin.pmtConn == NULL )
		return NOERROR;

	pwfx = (WAVEFORMATEX*)This->pPin->pin.pmtConn->pbFormat;
	if ( pwfx == NULL )
		return E_FAIL;

	This->m_fInFlush = FALSE;

	hr = CDSoundRendererImpl_dsoundInit(This,pwfx);
	if ( FAILED(hr) )
		return hr;

	return NOERROR;
}

static HRESULT CDSoundRendererImpl_OnStop( CBaseFilterImpl* pImpl )
{
	CDSoundRendererImpl_THIS(pImpl,basefilter);

	TRACE( "(%p)\n", This );

	This->m_fInFlush = TRUE;

	CDSoundRendererImpl_dsoundUninit(This);

	This->m_fInFlush = FALSE;

	TRACE("returned\n" );

	return NOERROR;
}

static const CBaseFilterHandlers filterhandlers =
{
	CDSoundRendererImpl_OnActive, /* pOnActive */
	CDSoundRendererImpl_OnInactive, /* pOnInactive */
	CDSoundRendererImpl_OnStop, /* pOnStop */
};

/***************************************************************************
 *
 *	CDSoundRendererPinImpl methods
 *
 */

static HRESULT CDSoundRendererPinImpl_OnDisconnect( CPinBaseImpl* pImpl )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);

	TRACE("(%p)\n",This);

	if ( This->meminput.pAllocator != NULL )
	{
		IMemAllocator_Decommit(This->meminput.pAllocator);
		IMemAllocator_Release(This->meminput.pAllocator);
		This->meminput.pAllocator = NULL;
	}

	return NOERROR;
}


static HRESULT CDSoundRendererPinImpl_CheckMediaType( CPinBaseImpl* pImpl, const AM_MEDIA_TYPE* pmt )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);
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

static HRESULT render_audio(CDSoundRendererImpl*This, BYTE* pData, DWORD dwDataLength)
{
	HRESULT hr;
	WAVEFORMATEX*	pwfx;
	DWORD dwWritten;

	pwfx = (WAVEFORMATEX*)This->pPin->pin.pmtConn->pbFormat;

	while ( 1 )
	{
		int sleep_ms;

		TRACE("trying to write %u bytes\n",dwDataLength);

		if ( This->m_fInFlush )
			return S_FALSE;

		ResetEvent( This->m_hEventRender );
		hr = CDSoundRendererImpl_dsoundWriteData(
			This,pData,dwDataLength,&dwWritten);
		if ( FAILED(hr) )
			break;
		if (hr != S_FALSE)
		{
			if (pData) pData += dwWritten;
			dwDataLength -= dwWritten;
			if ( dwDataLength == 0 )
				break;
			sleep_ms = 50;
		}
		else
			sleep_ms = 100;

		WaitForSingleObject( This->m_hEventRender, sleep_ms );
	}
	return hr;
}

static HRESULT CDSoundRendererPinImpl_Receive( CPinBaseImpl* pImpl, IMediaSample* pSample )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);
	WAVEFORMATEX*	pwfx;
	HRESULT hr, thr = VFW_E_SAMPLE_TIME_NOT_SET;
	BYTE*	pData = NULL;
	DWORD	dwDataLength;
	REFERENCE_TIME	rtStart, rtStop;

	TRACE( "(%p,%p)\n",This,pSample );

	if ( !This->pRender->m_dsbuffer )
		return E_UNEXPECTED;
	if ( pSample == NULL )
		return E_POINTER;

	pwfx = (WAVEFORMATEX*)This->pin.pmtConn->pbFormat;
	if ( pwfx == NULL )
		return E_FAIL;

	thr = IMediaSample_GetTime( pSample, &rtStart, &rtStop );

	if ( SUCCEEDED(thr) ) {
		if ( rtStart > This->pRender->m_rtLastPos ) {
			/* need to insert silence */
			REFERENCE_TIME rtGap = rtStart - This->pRender->m_rtLastPos;
			dwDataLength = rtGap * (REFERENCE_TIME)pwfx->nAvgBytesPerSec / (REFERENCE_TIME)QUARTZ_TIMEUNITS;
			dwDataLength -= dwDataLength % pwfx->nBlockAlign;

			hr = render_audio( This->pRender, NULL, dwDataLength);
			if ( FAILED(hr) )
				return hr;
			This->pRender->m_rtLastPos = rtStart;
		}
		if (rtStart < This->pRender->m_rtLastPos ) {
			ERR("audio going backwards? huh?\n");
		}
	}

	hr = IMediaSample_GetPointer( pSample, &pData );
	if ( FAILED(hr) )
		return hr;
	dwDataLength = (DWORD)IMediaSample_GetActualDataLength( pSample );

	hr = render_audio( This->pRender, pData, dwDataLength);
	if ( FAILED(hr) )
		return hr;

	if ( thr == S_OK )
		This->pRender->m_rtLastPos = rtStop;

	return hr;
}

static HRESULT CDSoundRendererPinImpl_ReceiveCanBlock( CPinBaseImpl* pImpl )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	/* might block. */
	return S_OK;
}

static HRESULT CDSoundRendererPinImpl_EndOfStream( CPinBaseImpl* pImpl )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);

	FIXME( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;

	/* FIXME - don't notify twice until stopped or seeked. */
	return CBaseFilterImpl_MediaEventNotify(
		&This->pRender->basefilter, EC_COMPLETE,
		(LONG_PTR)S_OK, (LONG_PTR)(IBaseFilter*)(This->pRender) );
}

static HRESULT CDSoundRendererPinImpl_BeginFlush( CPinBaseImpl* pImpl )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	This->pRender->m_fInFlush = TRUE;
	CDSoundRendererImpl_dsoundReset(This->pRender);

	return NOERROR;
}

static HRESULT CDSoundRendererPinImpl_EndFlush( CPinBaseImpl* pImpl )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;

	return NOERROR;
}

static HRESULT CDSoundRendererPinImpl_NewSegment( CPinBaseImpl* pImpl, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double rate )
{
	CDSoundRendererPinImpl_THIS(pImpl,pin);

	FIXME( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;

	return NOERROR;
}

static const CBasePinHandlers pinhandlers =
{
	NULL, /* pOnPreConnect */
	NULL, /* pOnPostConnect */
	CDSoundRendererPinImpl_OnDisconnect, /* pOnDisconnect */
	CDSoundRendererPinImpl_CheckMediaType, /* pCheckMediaType */
	NULL, /* pQualityNotify */
	CDSoundRendererPinImpl_Receive, /* pReceive */
	CDSoundRendererPinImpl_ReceiveCanBlock, /* pReceiveCanBlock */
	CDSoundRendererPinImpl_EndOfStream, /* pEndOfStream */
	CDSoundRendererPinImpl_BeginFlush, /* pBeginFlush */
	CDSoundRendererPinImpl_EndFlush, /* pEndFlush */
	CDSoundRendererPinImpl_NewSegment, /* pNewSegment */
};


/***************************************************************************
 *
 *	new/delete CDSoundRendererImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry FilterIFEntries[] =
{
  { &IID_IPersist, offsetof(CDSoundRendererImpl,basefilter)-offsetof(CDSoundRendererImpl,unk) },
  { &IID_IMediaFilter, offsetof(CDSoundRendererImpl,basefilter)-offsetof(CDSoundRendererImpl,unk) },
  { &IID_IBaseFilter, offsetof(CDSoundRendererImpl,basefilter)-offsetof(CDSoundRendererImpl,unk) },
  { &IID_IBasicAudio, offsetof(CDSoundRendererImpl,basaud)-offsetof(CDSoundRendererImpl,unk) },
};

static HRESULT CDSoundRendererImpl_OnQueryInterface(
	IUnknown* punk, const IID* piid, void** ppobj )
{
	CDSoundRendererImpl_THIS(punk,unk);

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

	if ( IsEqualGUID( &IID_IReferenceClock, piid ) )
	{
		TRACE( "IReferenceClock is queried\n" );
		if (!This->m_refClock) {
			IUnknown *refClock, *oldClock;
			HRESULT hr;
			hr = QUARTZ_CreateReferenceClock(punk, (void**)&refClock,
				&CDSoundRendererImpl_GetTime);
			if (FAILED(hr))
				return hr;
			TRACE("aggregated clock: %p\n", refClock);
			if (refClock) {
				oldClock = (IUnknown *)InterlockedCompareExchangePointer(
					(PVOID)&This->m_refClock,
					(PVOID)refClock,
					(PVOID)NULL);
				if (oldClock) {
					IUnknown_Release(refClock);
					TRACE("using old clock: %p\n", oldClock);
				}
			}
			else return E_NOINTERFACE;
		}
		return IUnknown_QueryInterface( This->m_refClock, piid, ppobj );
	}

	return E_NOINTERFACE;
}

static void QUARTZ_DestroyDSoundRenderer(IUnknown* punk)
{
	CDSoundRendererImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	if ( This->m_refClock != NULL )
	{
		IUnknown_Release(This->m_refClock);
		This->m_refClock = NULL;
	}
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

	CDSoundRendererImpl_UninitIBasicAudio(This);
	CBaseFilterImpl_UninitIBaseFilter(&This->basefilter);

	DeleteCriticalSection( &This->m_csReceive );
	DeleteCriticalSection( &This->m_csTime );
}

HRESULT QUARTZ_CreateDSoundRenderer(IUnknown* punkOuter,void** ppobj)
{
	CDSoundRendererImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p)\n",punkOuter,ppobj);

	This = (CDSoundRendererImpl*)
		QUARTZ_AllocObj( sizeof(CDSoundRendererImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;
	This->pSeekPass = NULL;
	This->pPin = NULL;
	This->m_dsound = NULL;
	This->m_dsbuffer = NULL;
	This->m_dsprimary = NULL;
	This->m_rtLastPos = (REFERENCE_TIME)0;
	This->m_rtLastTime = (REFERENCE_TIME)0;
	This->m_fInFlush = FALSE;
	This->m_recover = FALSE;
	This->m_lAudioVolume = 0;
	This->m_lAudioBalance = 0;
	This->m_hEventRender = (HANDLE)NULL;
	This->m_refClock = NULL;
	This->m_SavedPlayPos = 0;
	This->m_BytesPlayed = 0;
	CRITICAL_SECTION_DEFINE( &This->m_csTime );

	QUARTZ_IUnkInit( &This->unk, punkOuter );
	This->qiext.pNext = NULL;
	This->qiext.pOnQueryInterface = &CDSoundRendererImpl_OnQueryInterface;
	QUARTZ_IUnkAddDelegation( &This->unk, &This->qiext );

	hr = CBaseFilterImpl_InitIBaseFilter(
		&This->basefilter,
		This->unk.punkControl,
		&CLSID_DSoundRender,
		QUARTZ_DSoundRender_Name,
		&filterhandlers );
	if ( SUCCEEDED(hr) )
	{
		hr = CDSoundRendererImpl_InitIBasicAudio(This);
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
	This->unk.pOnFinalRelease = QUARTZ_DestroyDSoundRenderer;

	CRITICAL_SECTION_DEFINE( &This->m_csReceive );

	hr = QUARTZ_CreateDSoundRendererPin(
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
 *	new/delete CDSoundRendererPinImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry PinIFEntries[] =
{
  { &IID_IPin, offsetof(CDSoundRendererPinImpl,pin)-offsetof(CDSoundRendererPinImpl,unk) },
  { &IID_IMemInputPin, offsetof(CDSoundRendererPinImpl,meminput)-offsetof(CDSoundRendererPinImpl,unk) },
};

static void QUARTZ_DestroyDSoundRendererPin(IUnknown* punk)
{
	CDSoundRendererPinImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	CPinBaseImpl_UninitIPin( &This->pin );
	CMemInputPinBaseImpl_UninitIMemInputPin( &This->meminput );
}

HRESULT QUARTZ_CreateDSoundRendererPin(
	CDSoundRendererImpl* pFilter,
	CRITICAL_SECTION* pcsPin,
	CRITICAL_SECTION* pcsPinReceive,
	CDSoundRendererPinImpl** ppPin)
{
	CDSoundRendererPinImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p,%p,%p)\n",pFilter,pcsPin,pcsPinReceive,ppPin);

	This = (CDSoundRendererPinImpl*)
		QUARTZ_AllocObj( sizeof(CDSoundRendererPinImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;

	QUARTZ_IUnkInit( &This->unk, NULL );
	This->pRender = pFilter;

	hr = CPinBaseImpl_InitIPin(
		&This->pin,
		This->unk.punkControl,
		pcsPin, pcsPinReceive,
		&pFilter->basefilter,
		QUARTZ_DSoundRenderPin_Name,
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
	This->unk.pOnFinalRelease = QUARTZ_DestroyDSoundRendererPin;

	*ppPin = This;

	TRACE("returned successfully.\n");

	return S_OK;
}


/***************************************************************************
 *
 *	CDSoundRendererImpl::IBasicAudio
 *
 */

static HRESULT WINAPI
IBasicAudio_fnQueryInterface(IBasicAudio* iface,REFIID riid,void** ppobj)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IBasicAudio_fnAddRef(IBasicAudio* iface)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IBasicAudio_fnRelease(IBasicAudio* iface)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IBasicAudio_fnGetTypeInfoCount(IBasicAudio* iface,UINT* pcTypeInfo)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicAudio_fnGetTypeInfo(IBasicAudio* iface,UINT iTypeInfo, LCID lcid, ITypeInfo** ppobj)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicAudio_fnGetIDsOfNames(IBasicAudio* iface,REFIID riid, LPOLESTR* ppwszName, UINT cNames, LCID lcid, DISPID* pDispId)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicAudio_fnInvoke(IBasicAudio* iface,DISPID DispId, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarRes, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	CDSoundRendererImpl_THIS(iface,basaud);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}


static HRESULT WINAPI
IBasicAudio_fnput_Volume(IBasicAudio* iface,long lVol)
{
	CDSoundRendererImpl_THIS(iface,basaud);
	HRESULT hr;

	TRACE("(%p)->(%ld)\n",This,lVol);

	if ( lVol > 0 || lVol < -10000 )
		return E_INVALIDARG;

	EnterCriticalSection( &This->basefilter.csFilter );
	This->m_lAudioVolume = lVol;
	hr = CDSoundRendererImpl_UpdateVolume( This );
	LeaveCriticalSection( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IBasicAudio_fnget_Volume(IBasicAudio* iface,long* plVol)
{
	CDSoundRendererImpl_THIS(iface,basaud);

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
	CDSoundRendererImpl_THIS(iface,basaud);
	HRESULT hr;

	TRACE("(%p)->(%ld)\n",This,lBalance);

	if ( lBalance > 0 || lBalance < -10000 )
		return E_INVALIDARG;

	EnterCriticalSection( &This->basefilter.csFilter );
	This->m_lAudioBalance = lBalance;
	hr = CDSoundRendererImpl_UpdateBalance( This );
	LeaveCriticalSection( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IBasicAudio_fnget_Balance(IBasicAudio* iface,long* plBalance)
{
	CDSoundRendererImpl_THIS(iface,basaud);

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


HRESULT CDSoundRendererImpl_InitIBasicAudio( CDSoundRendererImpl* This )
{
	TRACE("(%p)\n",This);
	ICOM_VTBL(&This->basaud) = &ibasicaudio;

	return NOERROR;
}

void CDSoundRendererImpl_UninitIBasicAudio( CDSoundRendererImpl* This )
{
	TRACE("(%p)\n",This);
}

