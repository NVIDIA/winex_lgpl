/*
 * FFmpeg audio wrapper.
 *
 *	FIXME - no encoding
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
#include "mmsystem.h"
#include "mmreg.h"
#include "strmif.h"
#include "control.h"
#include "amvideo.h"
#include "vfwmsgs.h"
#include "uuids.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "xform.h"
#include "mtype.h"

#include "avcodec.h"


static const WCHAR FFMAWrapper_FilterName[] =
{'F','F','M','A',' ','W','r','a','p','p','e','r',0};


typedef struct CFFMAWrapperImpl
{
	AVCodecContext	ctx;
	WAVEFORMATEX*	pwfxIn;
	AM_MEDIA_TYPE*	pmtOuts;
	DWORD		cOuts;
	BYTE*		pConvBuf;
	DWORD		cbConvBlockSize;
	DWORD		cbConvCached;
	DWORD		cbConvAllocated;
} CFFMAWrapperImpl;


static struct {
	WORD wFormatTag;
	enum CodecID codec;
} ff_codecs[] = {
	{WAVE_FORMAT_ADPCM,	CODEC_ID_ADPCM_MS},
	{WAVE_FORMAT_ALAW,	CODEC_ID_PCM_ALAW},
	{WAVE_FORMAT_MULAW,	CODEC_ID_PCM_MULAW},
	{WAVE_FORMAT_IMA_ADPCM, CODEC_ID_ADPCM_IMA_WAV},
	{WAVE_FORMAT_MPEG,	CODEC_ID_MP2},
	{WAVE_FORMAT_MPEGLAYER3,CODEC_ID_MP2}, /* or CODEC_ID_MP3LAME, it's the same FFmpeg codec */
	{0}
};

static
void FFMAWrapper_CleanupMTypes( CFFMAWrapperImpl* This )
{
	DWORD	n;

	if ( This->pmtOuts == NULL ) return;
	for ( n = 0; n < This->cOuts; n++ )
	{
		QUARTZ_MediaType_Free( &This->pmtOuts[n] );
	}
	QUARTZ_FreeMem( This->pmtOuts );
	This->pmtOuts = NULL;
	This->cOuts = 0;
}

static
void FFMAWrapper_CleanupConvBuf( CFFMAWrapperImpl* This )
{
	if ( This->pConvBuf != NULL )
	{
		QUARTZ_FreeMem( This->pConvBuf );
		This->pConvBuf = NULL;
	}
	This->cbConvBlockSize = 0;
	This->cbConvCached = 0;
	This->cbConvAllocated = 0;
}

static
const WAVEFORMATEX* FFMAWrapper_GetAudioFmt( const AM_MEDIA_TYPE* pmt )
{
	const WAVEFORMATEX*	pwfx;

	if ( !IsEqualGUID( &pmt->majortype, &MEDIATYPE_Audio ) )
		return NULL;
	if ( !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_NULL ) &&
		 !QUARTZ_MediaSubType_IsFourCC( &pmt->subtype ) )
		return NULL;
	if ( !IsEqualGUID( &pmt->formattype, &FORMAT_WaveFormatEx ) )
		return NULL;
	if ( pmt->pbFormat == NULL ||
		 pmt->cbFormat < (sizeof(WAVEFORMATEX)-sizeof(WORD)) )
		return NULL;

	pwfx = (const WAVEFORMATEX*)pmt->pbFormat;

	if ( pwfx->wFormatTag != 1 && pmt->cbFormat < sizeof(WAVEFORMATEX) )
		return NULL;

	return pwfx;
}

static
HRESULT FFMAWrapper_SetupAudioFmt(
	AM_MEDIA_TYPE* pmt,
	DWORD cbFormat, WORD wFormatTag, DWORD dwBlockAlign )
{
	ZeroMemory( pmt, sizeof(AM_MEDIA_TYPE) );
	memcpy( &pmt->majortype, &MEDIATYPE_Audio, sizeof(GUID) );
	QUARTZ_MediaSubType_FromFourCC( &pmt->subtype, (DWORD)wFormatTag );
	pmt->bFixedSizeSamples = 1;
	pmt->bTemporalCompression = 1;
	pmt->lSampleSize = dwBlockAlign;
	memcpy( &pmt->formattype, &FORMAT_WaveFormatEx, sizeof(GUID) );
	pmt->pUnk = NULL;
	pmt->cbFormat = cbFormat;
	pmt->pbFormat = (BYTE*)CoTaskMemAlloc( cbFormat );
	if ( pmt->pbFormat == NULL )
		return E_OUTOFMEMORY;

	return S_OK;
}

static
void FFMAWrapper_FillFmtPCM(
	WAVEFORMATEX* pwfxOut,
	const WAVEFORMATEX* pwfxIn,
	WORD wBitsPerSampOut )
{
	pwfxOut->wFormatTag = 1;
	pwfxOut->nChannels = pwfxIn->nChannels;
	pwfxOut->nSamplesPerSec = pwfxIn->nSamplesPerSec;
	pwfxOut->nAvgBytesPerSec = ((DWORD)pwfxIn->nSamplesPerSec * (DWORD)pwfxIn->nChannels * (DWORD)wBitsPerSampOut) >> 3;
	pwfxOut->nBlockAlign = (pwfxIn->nChannels * wBitsPerSampOut) >> 3;
	pwfxOut->wBitsPerSample = wBitsPerSampOut;
	pwfxOut->cbSize = 0;
}

static
BOOL FFMAWrapper_IsSupported(
	WAVEFORMATEX* pwfxOut,
	WAVEFORMATEX* pwfxIn )
{
	int i;

	/* only support decoding for now */
	if ( pwfxIn->wFormatTag == 1 )
	{
		TRACE("pwfxIn is a linear PCM\n");
		return FALSE;
	}

	for (i=0; ff_codecs[i].wFormatTag && ff_codecs[i].wFormatTag != pwfxIn->wFormatTag; i++);
	if (!ff_codecs[i].wFormatTag)
	{
		TRACE("format not found\n");
		return FALSE;
	}

	/* we can only decode to PCM for now */
	if ( pwfxOut->wFormatTag != 1 )
	{
		TRACE("pwfxOut is not a linear PCM\n");
		return FALSE;
	}
	if ( pwfxIn->nChannels != pwfxOut->nChannels ||
	     pwfxIn->nSamplesPerSec != pwfxOut->nSamplesPerSec )
	{
		TRACE("nSamplesPerSec is not matched\n");
		return FALSE;
	}
	/* FFmpeg decoder output is 16-bit */
	if ( pwfxOut->wBitsPerSample != 16 )
	{
		TRACE("wBitsPerSample is not 16\n");
		return FALSE;
	}
	return TRUE;
}

static
HRESULT FFMAWrapper_StreamOpen(
	AVCodecContext* pctx,
	WAVEFORMATEX* pwfxOut,
	WAVEFORMATEX* pwfxIn )
{
	AVCodec*	codec;
	int i;

	for (i=0; ff_codecs[i].wFormatTag && ff_codecs[i].wFormatTag != pwfxIn->wFormatTag; i++);
	if (!ff_codecs[i].wFormatTag) {
		TRACE("couldn't find codec format\n");
		return E_FAIL;
	}

	codec = avcodec_find_decoder(ff_codecs[i].codec);
	if (!codec) {
		TRACE("couldn't find codec implementation\n");
		return E_FAIL;
	}

	avcodec_get_context_defaults(pctx);
	if (avcodec_open(pctx, codec) < 0) {
		TRACE("couldn't open codec\n");
		return E_FAIL;
	}
	return S_OK;
}

static
HRESULT FFMAWrapper_StreamClose(
	AVCodecContext* pctx )
{
	avcodec_close(pctx);
	memset(pctx, 0, sizeof(*pctx));
	return S_OK;
}

/***************************************************************************
 *
 *	CFFMAWrapperImpl methods
 *
 */

static void FFMAWrapper_Close( CFFMAWrapperImpl* This )
{
	if ( This->ctx.codec )
	{
		FFMAWrapper_StreamClose( &This->ctx );
	}
}

static HRESULT FFMAWrapper_Init( CTransformBaseImpl* pImpl )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;

	TRACE("(%p)\n",This);

	if ( This != NULL )
		return NOERROR;

	avcodec_init();
	avcodec_register_all();

	This = (CFFMAWrapperImpl*)QUARTZ_AllocMem( sizeof(CFFMAWrapperImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;
	ZeroMemory( This, sizeof(CFFMAWrapperImpl) );
	pImpl->m_pUserData = This;

	/* construct */
	memset( &This->ctx, 0, sizeof(This->ctx) );
	This->pwfxIn = NULL;
	This->pmtOuts = NULL;
	This->cOuts = 0;
	This->pConvBuf = NULL;

	return S_OK;
}

static HRESULT FFMAWrapper_Cleanup( CTransformBaseImpl* pImpl )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return NOERROR;

	/* destruct */
	FFMAWrapper_Close( This );
	QUARTZ_FreeMem( This->pwfxIn );
	FFMAWrapper_CleanupMTypes( This );
	FFMAWrapper_CleanupConvBuf( This );

	QUARTZ_FreeMem( This );
	pImpl->m_pUserData = NULL;

	return S_OK;
}

static HRESULT FFMAWrapper_CheckMediaType( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;
	const WAVEFORMATEX*	pwfxIn;
	const WAVEFORMATEX*	pwfxOut;
	WAVEFORMATEX	wfx;

	TRACE("(%p)\n",This);
	if ( This == NULL )
		return E_UNEXPECTED;

	pwfxIn = FFMAWrapper_GetAudioFmt(pmtIn);
	if ( pwfxIn == NULL ||
	     pwfxIn->wFormatTag == 0 ||
	     pwfxIn->wFormatTag == 1 )
	{
		TRACE("pwfxIn is not a compressed audio\n");
		return E_FAIL;
	}
	if ( pmtOut != NULL )
	{
		pwfxOut = FFMAWrapper_GetAudioFmt(pmtOut);
		if ( pwfxOut == NULL || pwfxOut->wFormatTag != 1 )
		{
			TRACE("pwfxOut is not a linear PCM\n");
			return E_FAIL;
		}
		if ( pwfxIn->nChannels != pwfxOut->nChannels ||
		     pwfxIn->nSamplesPerSec != pwfxOut->nSamplesPerSec )
		{
			TRACE("nChannels or nSamplesPerSec is not matched\n");
			return E_FAIL;
		}
		if ( !FFMAWrapper_IsSupported((WAVEFORMATEX*)pwfxOut,(WAVEFORMATEX*)pwfxIn) )
		{
			TRACE("specified formats are not supported by ACM\n");
			return E_FAIL;
		}
	}
	else
	{
		FFMAWrapper_FillFmtPCM(&wfx,pwfxIn,8);
		if ( FFMAWrapper_IsSupported(&wfx,(WAVEFORMATEX*)pwfxIn) )
		{
			TRACE("compressed audio - can be decoded to 8bit\n");
			return S_OK;
		}
		FFMAWrapper_FillFmtPCM(&wfx,pwfxIn,16);
		if ( FFMAWrapper_IsSupported(&wfx,(WAVEFORMATEX*)pwfxIn) )
		{
			TRACE("compressed audio - can be decoded to 16bit\n");
			return S_OK;
		}

		TRACE("unhandled audio %04x\n",(unsigned)pwfxIn->wFormatTag);
		return E_FAIL;
	}

	return S_OK;
}

static HRESULT FFMAWrapper_GetOutputTypes( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE** ppmtAcceptTypes, ULONG* pcAcceptTypes )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;
	HRESULT hr;
	const WAVEFORMATEX*	pwfxIn;
	AM_MEDIA_TYPE*		pmtTry;
	WAVEFORMATEX*		pwfxTry;

	FIXME("(%p)\n",This);
	hr = FFMAWrapper_CheckMediaType( pImpl, pmtIn, NULL );
	if ( FAILED(hr) )
		return hr;
	pwfxIn = (const WAVEFORMATEX*)pmtIn->pbFormat;

	FFMAWrapper_CleanupMTypes( This );
	This->pmtOuts = QUARTZ_AllocMem( sizeof(AM_MEDIA_TYPE) * 2 );
	if ( This->pmtOuts == NULL )
		return E_OUTOFMEMORY;
	This->cOuts = 0;

	pmtTry = &This->pmtOuts[This->cOuts];
	hr = FFMAWrapper_SetupAudioFmt(
		pmtTry,
		sizeof(WAVEFORMATEX), 1,
		(pwfxIn->nChannels * 8) >> 3 );
	if ( FAILED(hr) ) goto err;
	pwfxTry = (WAVEFORMATEX*)pmtTry->pbFormat;
	FFMAWrapper_FillFmtPCM( pwfxTry, pwfxIn, 8 );
	if ( FFMAWrapper_IsSupported( pwfxTry, (WAVEFORMATEX*)pwfxIn ) )
		This->cOuts ++;

        pmtTry = &This->pmtOuts[This->cOuts];
        hr = FFMAWrapper_SetupAudioFmt(
                pmtTry,
                sizeof(WAVEFORMATEX), 1,
                (pwfxIn->nChannels * 16) >> 3 );
        if ( FAILED(hr) ) goto err;
        pwfxTry = (WAVEFORMATEX*)pmtTry->pbFormat;
        FFMAWrapper_FillFmtPCM( pwfxTry, pwfxIn, 16 );
        if ( FFMAWrapper_IsSupported( pwfxTry, (WAVEFORMATEX*)pwfxIn ) )
                This->cOuts ++;

	*ppmtAcceptTypes = This->pmtOuts;
	*pcAcceptTypes = This->cOuts;

	return S_OK;
err:
	FFMAWrapper_CleanupMTypes( This );
	return hr;
}

static HRESULT
FFMAWrapper_GetConvBufSize(
	CTransformBaseImpl* pImpl,
	CFFMAWrapperImpl* This,
	DWORD* pcbInput, DWORD* pcbOutput,
	const AM_MEDIA_TYPE* pmtOut, const AM_MEDIA_TYPE* pmtIn )
{
        HRESULT hr;
        const WAVEFORMATEX* pwfxIn;
        const WAVEFORMATEX* pwfxOut;
        DWORD   cbInput;
        DWORD   cbOutput;

        if ( This == NULL )
                return E_UNEXPECTED;

        hr = FFMAWrapper_CheckMediaType( pImpl, pmtIn, pmtOut );
        if ( FAILED(hr) )
                return hr;
        pwfxIn = (const WAVEFORMATEX*)pmtIn->pbFormat;
        pwfxOut = (const WAVEFORMATEX*)pmtOut->pbFormat;

        /* FFmpeg provides no way to query the codec without actual data,
         * but this should be a good estimate for 1s of sound data */
	/* make output buffer 25% larger to have room for surprises */

        cbInput = (pwfxIn->nAvgBytesPerSec + pwfxIn->nBlockAlign - 1) / pwfxIn->nBlockAlign * pwfxIn->nBlockAlign;
        cbOutput = ((pwfxOut->nAvgBytesPerSec + pwfxOut->nBlockAlign - 1)*5/4) / pwfxOut->nBlockAlign * pwfxOut->nBlockAlign;

        TRACE("size(%p) %u -> %u\n", This, cbInput, cbOutput);

	if ( pcbInput != NULL ) *pcbInput = cbInput;
	if ( pcbOutput != NULL ) *pcbOutput = cbOutput;

	return S_OK;
}


static HRESULT FFMAWrapper_GetAllocProp( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut, ALLOCATOR_PROPERTIES* pProp, BOOL* pbTransInPlace, BOOL* pbTryToReuseSample )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;
	HRESULT hr;
	DWORD	cbOutput;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return E_UNEXPECTED;

	hr = FFMAWrapper_GetConvBufSize(
		pImpl, This, NULL, &cbOutput, pmtOut, pmtIn );
	if ( FAILED(hr) )
		return hr;

	pProp->cBuffers = 1;
	pProp->cbBuffer = cbOutput;

	*pbTransInPlace = FALSE;
	*pbTryToReuseSample = FALSE;

	return S_OK;
}

static HRESULT FFMAWrapper_BeginTransform( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut, BOOL bReuseSample )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;
	HRESULT	hr;
	const WAVEFORMATEX*	pwfxIn;
	const WAVEFORMATEX*	pwfxOut;
	DWORD	cbInput;

	TRACE("(%p,%p,%p,%d)\n",This,pmtIn,pmtOut,bReuseSample);

	if ( This == NULL )
		return E_UNEXPECTED;

	FFMAWrapper_Close( This );
	FFMAWrapper_CleanupMTypes( This );
	FFMAWrapper_CleanupConvBuf( This );

	hr = FFMAWrapper_GetConvBufSize(
		pImpl, This, &cbInput, NULL, pmtOut, pmtIn );
        if ( FAILED(hr) )
                return hr;
        pwfxIn = (const WAVEFORMATEX*)pmtIn->pbFormat;
        pwfxOut = (const WAVEFORMATEX*)pmtOut->pbFormat;

	This->pConvBuf = (BYTE*)QUARTZ_AllocMem( cbInput );
	if ( This->pConvBuf == NULL )
		return E_OUTOFMEMORY;
	This->cbConvBlockSize = pwfxIn->nBlockAlign;
	This->cbConvCached = 0;
	This->cbConvAllocated = cbInput;

	hr = FFMAWrapper_StreamOpen(
		&This->ctx,
		(WAVEFORMATEX*)pwfxOut, (WAVEFORMATEX*)pwfxIn );
	if ( FAILED(hr) )
		return E_FAIL;

	return S_OK;
}

static HRESULT FFMAWrapper_Convert(
	CTransformBaseImpl* pImpl,
	CFFMAWrapperImpl* This,
	BYTE* pbSrc, DWORD cbSrcLen,
	DWORD dwConvFlags,
	REFERENCE_TIME* rtStart, REFERENCE_TIME* rtStop )
{
	HRESULT hr = E_FAIL;
	DWORD	cb;
	IMediaSample*	pSampOut = NULL;
	BYTE*	pOutBuf;
	LONG	lOutBufLen;
	int	nInBufUsed, nOutBufUsed, iused, oused;

	TRACE("(%p,%p,%u,%x)\n", This, pbSrc, cbSrcLen, dwConvFlags);

	if ( This->pConvBuf == NULL )
		return E_UNEXPECTED;

	if ( dwConvFlags & AM_SAMPLE_DATADISCONTINUITY )
	{
		avcodec_flush_buffers( &This->ctx );
		This->cbConvCached = 0;
	}

	while ( 1 )
	{
		cb = cbSrcLen + This->cbConvCached;
		if ( cb > This->cbConvAllocated )
			cb = This->cbConvAllocated;
		cb -= This->cbConvCached;
		if ( cb > 0 )
		{
			memcpy( This->pConvBuf+This->cbConvCached,
				pbSrc, cb );
			pbSrc += cb;
			cbSrcLen -= cb;
			This->cbConvCached += cb;
		}

		cb = This->cbConvCached / This->cbConvBlockSize * This->cbConvBlockSize;
		if ( cb == 0 )
		{
			if ( dwConvFlags & AM_SAMPLE_ENDOFSTREAM )
			{
				cb = This->cbConvCached;
			}
			if ( cb == 0 )
			{
				hr = S_OK;
				break;
			}
		}

		hr = IMemAllocator_GetBuffer(
			pImpl->m_pOutPinAllocator,
			&pSampOut, rtStart, rtStop, 0 );
		if ( FAILED(hr) )
			break;
		hr = IMediaSample_SetSyncPoint( pSampOut, TRUE );
		if ( FAILED(hr) )
			break;
		if ( dwConvFlags & AM_SAMPLE_DATADISCONTINUITY )
		{
			hr = IMediaSample_SetDiscontinuity( pSampOut, TRUE );
			if ( FAILED(hr) )
				break;
		}
		hr = IMediaSample_GetPointer( pSampOut, &pOutBuf );
		if ( FAILED(hr) )
			break;
		lOutBufLen = IMediaSample_GetSize( pSampOut );
		if ( lOutBufLen <= 0 )
		{
			hr = E_FAIL;
			break;
		}

		TRACE("InBuf=%u, Cached=%u, Allocated=%u\n", cb, This->cbConvCached, This->cbConvAllocated);
		nInBufUsed = 0;
		nOutBufUsed = 0;
		iused = 0;
		while ( nInBufUsed < cb && nOutBufUsed < lOutBufLen )
		{
			oused = lOutBufLen - nOutBufUsed;
			iused = avcodec_decode_audio( &This->ctx,
						      (void*)(pOutBuf + nOutBufUsed), &oused,
						      This->pConvBuf + nInBufUsed, cb - nInBufUsed );
			if ( iused < 0 )
			{
				nInBufUsed = iused;
				break;
			}
			if ( !iused && !oused ) break;
			if (oused > 0) nOutBufUsed += oused;
			nInBufUsed += iused;
		}
		TRACE("InBufUsed=%d, OutBufUsed=%d, OutBufLen=%d\n", nInBufUsed, nOutBufUsed, lOutBufLen);

		if ( nInBufUsed < 0 )
		{
			hr = E_FAIL;
			break;
		}

		if ( nOutBufUsed > lOutBufLen )
		{
			WARN("arrgh - FFmpeg wrote too much\n");
			ERR("ALERT! ALERT! BUFFER OVERRUN! CRASH IMMINENT!\n");
		}

		if ( nOutBufUsed > 0 )
		{
			hr = IMediaSample_SetActualDataLength( pSampOut, nOutBufUsed );
			if ( FAILED(hr) )
				break;

			hr = CPinBaseImpl_SendSample(
				&pImpl->pOutPin->pin,
				pSampOut );
			if ( FAILED(hr) )
				break;
			/* hack to unconfuse our audio renderer */
			rtStart = NULL;
			rtStop = NULL;
		}

		if ( nInBufUsed > This->cbConvCached )
		{
			WARN("arrgh - FFmpeg read too much\n");
			nInBufUsed = This->cbConvCached;
		}

		if ( This->cbConvCached == nInBufUsed )
		{
			This->cbConvCached = 0;
		}
		else
		{
			This->cbConvCached -= nInBufUsed;
			memmove( This->pConvBuf,
				 This->pConvBuf + nInBufUsed,
				 This->cbConvCached );
		}

		IMediaSample_Release( pSampOut ); pSampOut = NULL;
		dwConvFlags &= ~AM_SAMPLE_DATADISCONTINUITY;
	}

	if ( pSampOut != NULL )
		IMediaSample_Release( pSampOut );
	TRACE("complete\n");

	return hr;
}

static HRESULT FFMAWrapper_ProcessReceive( CTransformBaseImpl* pImpl, IMediaSample* pSampIn )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;
	BYTE*	pDataIn = NULL;
	LONG	lDataInLen;
	HRESULT hr;
	DWORD	dwConvFlags = 0;
	REFERENCE_TIME rtStart, rtStop;

	TRACE("(%p)->(%p)\n",This,pSampIn);

	if ( This == NULL || !This->ctx.codec )
		return E_UNEXPECTED;

	hr = IMediaSample_GetPointer( pSampIn, &pDataIn );
	if ( FAILED(hr) )
		return hr;
	lDataInLen = IMediaSample_GetActualDataLength( pSampIn );
	if ( lDataInLen < 0 )
		return E_FAIL;
	if ( IMediaSample_IsDiscontinuity( pSampIn ) == S_OK )
		dwConvFlags |= AM_SAMPLE_DATADISCONTINUITY;
	hr = IMediaSample_GetTime( pSampIn, &rtStart, &rtStop );
	if ( hr == S_OK )
		return FFMAWrapper_Convert(
			pImpl, This, pDataIn, (DWORD)lDataInLen,
			dwConvFlags, &rtStart, &rtStop );
	else
		return FFMAWrapper_Convert(
			pImpl, This, pDataIn, (DWORD)lDataInLen,
			dwConvFlags, NULL, NULL );
}

static HRESULT FFMAWrapper_EndTransform( CTransformBaseImpl* pImpl )
{
	CFFMAWrapperImpl*	This = pImpl->m_pUserData;
	HRESULT hr;
	DWORD	dwConvFlags = AM_SAMPLE_ENDOFSTREAM;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return E_UNEXPECTED;

	hr = FFMAWrapper_Convert(
		pImpl, This, NULL, 0,
		dwConvFlags, NULL, NULL );

	FFMAWrapper_Close( This );
	FFMAWrapper_CleanupMTypes( This );
	FFMAWrapper_CleanupConvBuf( This );

	return hr;
}

static const TransformBaseHandlers transhandlers =
{
	FFMAWrapper_Init,
	FFMAWrapper_Cleanup,
	FFMAWrapper_CheckMediaType,
	FFMAWrapper_GetOutputTypes,
	FFMAWrapper_GetAllocProp,
	FFMAWrapper_BeginTransform,
	FFMAWrapper_ProcessReceive,
	NULL,
	FFMAWrapper_EndTransform,
};

HRESULT QUARTZ_CreateFFMAWrapper(IUnknown* punkOuter,void** ppobj)
{
	return QUARTZ_CreateTransformBase(
		punkOuter,ppobj,
		&CLSID_quartzFFMAWrapper,
		FFMAWrapper_FilterName,
		NULL, NULL,
		&transhandlers );
}


