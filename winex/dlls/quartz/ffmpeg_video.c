/*
 * FFmpeg video wrapper.
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
#include "vfw.h" /* ? */
#include "strmif.h"
#include "control.h"
#include "amvideo.h"
#include "vfwmsgs.h"
#include "uuids.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "xform.h"

#include "avcodec.h"

#define SKIP_TIME (QUARTZ_TIMEUNITS/100)	/* skip frames decoded less than 0.01s before presentation time */
#define MAX_SKIP 50 /* max number of frames to skip */


static const WCHAR FFMVWrapper_FilterName[] =
{'F','F','M','V',' ','W','r','a','p','p','e','r',0};

typedef struct CFFMVWrapperImpl
{
	AVCodecContext	ctx;
	AM_MEDIA_TYPE m_mtOut;
	BITMAPINFO* m_pbiIn;
	BITMAPINFO* m_pbiOut;
	BYTE* m_pOutBuf;
	CRITICAL_SECTION m_cs;
	REFERENCE_TIME rtCur;
	REFERENCE_TIME rtInternal;
	DWORD skipFrames;
} CFFMVWrapperImpl;

static struct {
	DWORD dwFourCC;
	enum CodecID codec;
} ff_codecs[] = {
	{mmioFOURCC('U','2','6','3'), CODEC_ID_H263},
	{mmioFOURCC('I','2','6','3'), CODEC_ID_H263I},
	{mmioFOURCC('D','I','V','3'), CODEC_ID_MSMPEG4V3},
	{mmioFOURCC('D','I','V','X'), CODEC_ID_MPEG4},
	{mmioFOURCC('D','X','5','0'), CODEC_ID_MPEG4},
	{mmioFOURCC('M','P','4','2'), CODEC_ID_MSMPEG4V2},
	{mmioFOURCC('M','J','P','G'), CODEC_ID_MJPEG},
	{mmioFOURCC('P','I','M','1'), CODEC_ID_MPEG1VIDEO},
	{mmioFOURCC('B','L','Z','0'), CODEC_ID_MPEG4},
	{0}
};

/***************************************************************************
 *
 *	CFFMVWrapperImpl methods
 *
 */

static void FFMVWrapper_ReleaseDIBBuffers(CFFMVWrapperImpl* This)
{
	TRACE("(%p)\n",This);

	if ( This->m_pbiIn != NULL )
	{
		QUARTZ_FreeMem(This->m_pbiIn); This->m_pbiIn = NULL;
	}
	if ( This->m_pbiOut != NULL )
	{
		QUARTZ_FreeMem(This->m_pbiOut); This->m_pbiOut = NULL;
	}
	if ( This->m_pOutBuf != NULL )
	{
		QUARTZ_FreeMem(This->m_pOutBuf); This->m_pOutBuf = NULL;
	}
}

static BITMAPINFO* FFMVWrapper_DuplicateBitmapInfo(const BITMAPINFO* pbi)
{
	DWORD dwSize;
	BITMAPINFO*	pbiRet;

	dwSize = pbi->bmiHeader.biSize;
	if ( dwSize < sizeof(BITMAPINFOHEADER) )
		return NULL;
	if ( pbi->bmiHeader.biBitCount <= 8 )
	{
		if ( pbi->bmiHeader.biClrUsed == 0 )
			dwSize += sizeof(RGBQUAD)*(1<<pbi->bmiHeader.biBitCount);
		else
			dwSize += sizeof(RGBQUAD)*pbi->bmiHeader.biClrUsed;
	}
	if ( pbi->bmiHeader.biCompression == 3 &&
		 dwSize == sizeof(BITMAPINFOHEADER) )
		dwSize += sizeof(DWORD)*3;

	pbiRet = (BITMAPINFO*)QUARTZ_AllocMem(dwSize);
	if ( pbiRet != NULL )
		memcpy( pbiRet, pbi, dwSize );

	return pbiRet;
}

static HRESULT FFMVWrapper_Init( CTransformBaseImpl* pImpl )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;

	TRACE("(%p)\n",This);

	if ( This != NULL )
		return NOERROR;

	This = (CFFMVWrapperImpl*)QUARTZ_AllocMem( sizeof(CFFMVWrapperImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;
	ZeroMemory( This, sizeof(CFFMVWrapperImpl) );
	pImpl->m_pUserData = This;
	/* construct */
	memset( &This->ctx, 0, sizeof(This->ctx) );
	ZeroMemory( &This->m_mtOut, sizeof(AM_MEDIA_TYPE) );
	This->m_pbiIn = NULL;
	This->m_pbiOut = NULL;
	This->m_pOutBuf = NULL;
	CRITICAL_SECTION_DEFINE( &This->m_cs );

	return NOERROR;
}

static HRESULT FFMVWrapper_Cleanup( CTransformBaseImpl* pImpl )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return NOERROR;

	/* destruct */
	EnterCriticalSection( &This->m_cs );
	if ( This->ctx.codec )
	{
		avcodec_close( &This->ctx );
		memset( &This->ctx, 0, sizeof(This->ctx) );
	}
	LeaveCriticalSection( &This->m_cs );

	QUARTZ_MediaType_Free( &This->m_mtOut );

	FFMVWrapper_ReleaseDIBBuffers(This);

	DeleteCriticalSection( &This->m_cs );

	QUARTZ_FreeMem( This );
	pImpl->m_pUserData = NULL;

	return NOERROR;
}

static HRESULT FFMVWrapper_CheckMediaType( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;
	HRESULT hr;
	BITMAPINFO*	pbiIn = NULL;
	BITMAPINFO*	pbiOut = NULL;
	LONG	width, height;
	DWORD	dwFourCC;
	GUID stOut;
	int	i;

	TRACE("(%p)\n",This);
	if ( This == NULL )
		return E_UNEXPECTED;

	if ( !IsEqualGUID( &pmtIn->majortype, &MEDIATYPE_Video ) )
		return E_FAIL;
	if ( IsEqualGUID( &pmtIn->formattype, &FORMAT_VideoInfo ) )
	{
		pbiIn = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtIn->pbFormat)->bmiHeader);
		dwFourCC = pbiIn->bmiHeader.biCompression;
	}
	else if ( IsEqualGUID( &pmtIn->formattype, &FORMAT_MPEGVideo ) )
	{
		pbiIn = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtIn->pbFormat)->bmiHeader);
		dwFourCC = mmioFOURCC('P','I','M','1');
	}
	else return E_FAIL;

	width = pbiIn->bmiHeader.biWidth;
	height = (pbiIn->bmiHeader.biHeight < 0) ?
		 -pbiIn->bmiHeader.biHeight :
		  pbiIn->bmiHeader.biHeight;

	if ( pmtOut != NULL )
	{
		if ( !IsEqualGUID( &pmtOut->majortype, &MEDIATYPE_Video ) )
			return E_FAIL;
		if ( !IsEqualGUID( &pmtOut->formattype, &FORMAT_VideoInfo ) )
			return E_FAIL;
		pbiOut = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtOut->pbFormat)->bmiHeader);
		if ( pbiOut->bmiHeader.biCompression != 0 &&
			 pbiOut->bmiHeader.biCompression != 3 )
			return E_FAIL;
		hr = QUARTZ_MediaSubType_FromBitmap( &stOut, &pbiOut->bmiHeader );
		if ( hr != S_OK || !IsEqualGUID( &pmtOut->subtype, &stOut ) )
			return E_FAIL;
		if ( pbiOut->bmiHeader.biWidth != width ||
		     pbiOut->bmiHeader.biHeight != -height ||
		     pbiIn->bmiHeader.biPlanes != 1 ||
		     pbiOut->bmiHeader.biPlanes != 1 )
			return E_FAIL;
		if ( pbiOut->bmiHeader.biBitCount != 24 && pbiOut->bmiHeader.biBitCount != 32 )
			return E_FAIL;
	}

	for (i=0; ff_codecs[i].dwFourCC && ff_codecs[i].dwFourCC != dwFourCC; i++);
	if (!ff_codecs[i].dwFourCC)
	{
		TRACE("format %4.4s not supported\n", (char*)&dwFourCC);
		return E_FAIL;
	}
	TRACE("format %4.4s supported\n", (char*)&dwFourCC);

	return NOERROR;
}

static HRESULT FFMVWrapper_GetOutputTypes( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE** ppmtAcceptTypes, ULONG* pcAcceptTypes )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;
	HRESULT hr;
	BITMAPINFO*	pbiIn = NULL;
	BITMAPINFO*	pbiOut = NULL;
	LONG	width, height;

	TRACE("(%p)\n",This);
	hr = FFMVWrapper_CheckMediaType( pImpl, pmtIn, NULL );
	if ( FAILED(hr) )
		return hr;

	pbiIn = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtIn->pbFormat)->bmiHeader);

	width = pbiIn->bmiHeader.biWidth;
	height = (pbiIn->bmiHeader.biHeight < 0) ?
		 -pbiIn->bmiHeader.biHeight :
		  pbiIn->bmiHeader.biHeight;

	QUARTZ_MediaType_Free( &This->m_mtOut );
	ZeroMemory( &This->m_mtOut, sizeof(AM_MEDIA_TYPE) );

	memcpy( &This->m_mtOut.majortype, &MEDIATYPE_Video, sizeof(GUID) );
	memcpy( &This->m_mtOut.formattype, &FORMAT_VideoInfo, sizeof(GUID) );
	This->m_mtOut.cbFormat = sizeof(VIDEOINFO);
	This->m_mtOut.pbFormat = (BYTE*)CoTaskMemAlloc(This->m_mtOut.cbFormat);
	if ( This->m_mtOut.pbFormat == NULL )
		return E_OUTOFMEMORY;
	ZeroMemory( This->m_mtOut.pbFormat, This->m_mtOut.cbFormat );

	pbiOut = (BITMAPINFO*)(&((VIDEOINFOHEADER*)This->m_mtOut.pbFormat)->bmiHeader);

	/* suggest 24bpp RGB output */
	pbiOut->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbiOut->bmiHeader.biWidth = width;
	pbiOut->bmiHeader.biHeight = -height;
	pbiOut->bmiHeader.biPlanes = 1;
	pbiOut->bmiHeader.biBitCount = 24;
	pbiOut->bmiHeader.biSizeImage = DIBSIZE(pbiOut->bmiHeader);
	memcpy( &This->m_mtOut.subtype, &MEDIASUBTYPE_RGB24, sizeof(GUID) );

	This->m_mtOut.bFixedSizeSamples = 1;
	This->m_mtOut.lSampleSize = DIBSIZE(pbiOut->bmiHeader);

	TRACE("(%p) - return format\n",This);
	*ppmtAcceptTypes = &This->m_mtOut;
	*pcAcceptTypes = 1;

	return NOERROR;
}

static HRESULT FFMVWrapper_GetAllocProp( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut, ALLOCATOR_PROPERTIES* pProp, BOOL* pbTransInPlace, BOOL* pbTryToReuseSample )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;
	BITMAPINFO*	pbiOut = NULL;
	HRESULT hr;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return E_UNEXPECTED;

	hr = FFMVWrapper_CheckMediaType( pImpl, pmtIn, pmtOut );
	if ( FAILED(hr) )
		return hr;

	pbiOut = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtOut->pbFormat)->bmiHeader);

	pProp->cBuffers = 1;
	if ( pbiOut->bmiHeader.biCompression == 0 )
		pProp->cbBuffer = DIBSIZE(pbiOut->bmiHeader);
	else
		pProp->cbBuffer = pbiOut->bmiHeader.biSizeImage;

	*pbTransInPlace = FALSE;
	*pbTryToReuseSample = TRUE;

	return NOERROR;
}

static HRESULT FFMVWrapper_BeginTransform( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut, BOOL bReuseSample )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;
	BITMAPINFO*	pbiIn = NULL;
	BITMAPINFO*	pbiOut = NULL;
	LONG	width, height;
	DWORD	dwFourCC;
	AVCodec*	codec;
	HRESULT hr;
	int	i;

	TRACE("(%p,%p,%p,%d)\n",This,pmtIn,pmtOut,bReuseSample);

	if ( This == NULL || This->ctx.codec )
		return E_UNEXPECTED;

	hr = FFMVWrapper_CheckMediaType( pImpl, pmtIn, pmtOut );
	if ( FAILED(hr) )
		return hr;

	FFMVWrapper_ReleaseDIBBuffers(This);

	if ( IsEqualGUID( &pmtIn->formattype, &FORMAT_VideoInfo ) )
	{
		pbiIn = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtIn->pbFormat)->bmiHeader);
		dwFourCC = pbiIn->bmiHeader.biCompression;
	}
	else if ( IsEqualGUID( &pmtIn->formattype, &FORMAT_MPEGVideo ) )
	{
		pbiIn = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtIn->pbFormat)->bmiHeader);
		dwFourCC = mmioFOURCC('P','I','M','1');
	}
	else return E_FAIL;

	width = pbiIn->bmiHeader.biWidth;
	height = (pbiIn->bmiHeader.biHeight < 0) ?
		 -pbiIn->bmiHeader.biHeight :
		  pbiIn->bmiHeader.biHeight;

	pbiOut = (BITMAPINFO*)(&((VIDEOINFOHEADER*)pmtOut->pbFormat)->bmiHeader);

	This->m_pbiIn = FFMVWrapper_DuplicateBitmapInfo(pbiIn);
	This->m_pbiOut = FFMVWrapper_DuplicateBitmapInfo(pbiOut);
	if ( This->m_pbiIn == NULL || This->m_pbiOut == NULL )
		return E_OUTOFMEMORY;
	if ( This->m_pbiOut->bmiHeader.biCompression == 0 || This->m_pbiOut->bmiHeader.biCompression == 3 )
		This->m_pbiOut->bmiHeader.biSizeImage = DIBSIZE(This->m_pbiOut->bmiHeader);

	for (i=0; ff_codecs[i].dwFourCC && ff_codecs[i].dwFourCC != dwFourCC; i++);
	if (!ff_codecs[i].dwFourCC)
	{
		TRACE("couldn't find codec format\n");
		return E_FAIL;
	}

	codec = avcodec_find_decoder(ff_codecs[i].codec);
	if (!codec)
	{
		TRACE("couldn't open codec\n");
		return E_FAIL;
	}

	if ( !bReuseSample )
	{
		This->m_pOutBuf = QUARTZ_AllocMem(This->m_pbiOut->bmiHeader.biSizeImage);
		if ( This->m_pOutBuf == NULL )
			return E_OUTOFMEMORY;
		ZeroMemory( This->m_pOutBuf, This->m_pbiOut->bmiHeader.biSizeImage );
	}

	This->rtCur = 0;
	This->rtInternal = 0;

	EnterCriticalSection( &This->m_cs );

	avcodec_get_context_defaults( &This->ctx );
	This->ctx.width = width;
	This->ctx.height = height;
	if (codec->id == CODEC_ID_MPEG1VIDEO || codec->id == CODEC_ID_H264)
		This->ctx.flags |= CODEC_FLAG_TRUNCATED;
	TRACE("opening codec for %dx%d video\n", This->ctx.width, This->ctx.height);

	if (avcodec_open( &This->ctx, codec) < 0) {
		TRACE("couldn't open codec\n");
		return E_FAIL;
	}

	LeaveCriticalSection( &This->m_cs );

	return NOERROR;
}

static HRESULT FFMVWrapper_ProcessReceive( CTransformBaseImpl* pImpl, IMediaSample* pSampIn )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;
	BYTE*	pDataIn = NULL;
	LONG	lDataInLen;
	IMediaSample*	pSampOut = NULL;
	BYTE*	pOutBuf;
	HRESULT hr;
	AVFrame tmp_pic;
	AVPicture dst_pic;
	int	nOut, got_pic;
	LONG	width, height;
	REFERENCE_TIME rtStart, rtStop, rtNow;
	BOOL 	skip;

	TRACE("(%p)\n",This);

	if ( This == NULL || !This->ctx.codec ||
		 This->m_pbiIn == NULL ||
		 This->m_pbiOut == NULL )
		return E_UNEXPECTED;

	hr = IMediaSample_GetPointer( pSampIn, &pDataIn );
	if ( FAILED(hr) )
		return hr;
	lDataInLen = IMediaSample_GetActualDataLength( pSampIn );
	if ( lDataInLen < 0 )
		return E_FAIL;

	EnterCriticalSection( &This->m_cs );

	if ( !This->ctx.codec )
	{
		hr = E_UNEXPECTED;
		goto failed;
	}

	if ( IMediaSample_IsDiscontinuity( pSampIn ) == S_OK )
		avcodec_flush_buffers( &This->ctx );

	width = This->m_pbiIn->bmiHeader.biWidth;
	height = (This->m_pbiIn->bmiHeader.biHeight < 0) ?
		 -This->m_pbiIn->bmiHeader.biHeight :
		  This->m_pbiIn->bmiHeader.biHeight;

	while ( TRUE )
	{
		nOut = avcodec_decode_video( &This->ctx, &tmp_pic, &got_pic,
					     (void*)pDataIn, lDataInLen );
		if ( nOut < 0 )
		{
			TRACE("decoding error\n");
			goto fail;
		}

		TRACE("used %d of %d bytes\n", nOut, lDataInLen);

		if ( nOut > lDataInLen )
		{
			WARN("arrgh - FFmpeg read too much\n");
			nOut = lDataInLen;
		}

		pDataIn += nOut;
		lDataInLen -= nOut;

		if (!got_pic)
		{
			TRACE("no frame decoded\n");
			if (lDataInLen) continue;
			LeaveCriticalSection( &This->m_cs );
			return S_OK;
		}

		TRACE("frame decoded\n");
		This->rtInternal ++;
		hr = IMediaSample_GetTime( pSampIn, &rtStart, &rtStop );
		if ( hr == S_OK )
		{
			/* if the parser gives us a timestamp, the data
			 * we got from it should be a single frame */
			if ( lDataInLen ) {
				ERR("excessive data in compressed frame\n");
				lDataInLen = 0;
			}
		}
		else {
			/* compute our own timestamp */
			rtStart = This->rtCur;
			This->rtCur = This->rtInternal * (REFERENCE_TIME)QUARTZ_TIMEUNITS * This->ctx.frame_rate_base / This->ctx.frame_rate;
			rtStop = This->rtCur;
		}
		TRACE("frame start=%lld, stop=%lld\n", rtStart, rtStop);
		skip = FALSE;
		hr = IReferenceClock_GetTime(pImpl->basefilter.pClock, &rtNow);
		if (SUCCEEDED(hr))
		{
			rtNow -= pImpl->basefilter.rtStart;
			TRACE("time=%lld\n", rtNow);
			if (rtStart < rtNow + SKIP_TIME)
			{
				skip = TRUE;
				if ( ++This->skipFrames >= MAX_SKIP ) {
					This->skipFrames = 0;
					TRACE("frame late, but max skip exceeded\n");
					skip = FALSE;
				}
			}
		}
		if (skip)
		{
			TRACE("skipping late frame\n");
			if ( lDataInLen == 0 )
			{
				LeaveCriticalSection( &This->m_cs );
				return S_OK;
			}
		}
		else {
			/* process frame */
			hr = IMemAllocator_GetBuffer(
				pImpl->m_pOutPinAllocator,
				&pSampOut, &rtStart, &rtStop, 0 );
			if ( FAILED(hr) )
				goto failed;
			hr = IMediaSample_GetPointer( pSampOut, &pOutBuf );
			if ( FAILED(hr) )
				goto failed;

			dst_pic.data[0] = ( This->m_pOutBuf != NULL ) ? This->m_pOutBuf : pOutBuf;
			dst_pic.linesize[0] = DIBWIDTHBYTES(This->m_pbiOut->bmiHeader);

			/* convert to RGB (or BGR) */
			switch (This->m_pbiOut->bmiHeader.biBitCount)
			{
			case 24:
				img_convert( &dst_pic, PIX_FMT_BGR24,
					     (AVPicture*)&tmp_pic, This->ctx.pix_fmt,
					     width, height );
				break;
			case 32:
				/* RGBA32 is misnamed (is actually cpu-endian ARGB, which means BGRA on x86),
				 * might get renamed in future ffmpeg snapshots */
				img_convert( &dst_pic, PIX_FMT_RGBA32,
					     (AVPicture*)&tmp_pic, This->ctx.pix_fmt,
					     width, height );
				break;
			default:
				TRACE("bad bpp\n");
				goto fail;
			}

			if ( This->m_pOutBuf != NULL )
				memcpy( pOutBuf, This->m_pOutBuf,
					This->m_pbiOut->bmiHeader.biSizeImage );

			IMediaSample_SetActualDataLength( pSampOut, This->m_pbiOut->bmiHeader.biSizeImage );

			/* FIXME: discontinuity and sync point */

			LeaveCriticalSection( &This->m_cs );

			hr = CPinBaseImpl_SendSample(
				&pImpl->pOutPin->pin,
				pSampOut );
			if ( FAILED(hr) )
				return hr;

			IMediaSample_Release( pSampOut );
			pSampOut = NULL;

			if ( lDataInLen == 0 )
				return S_OK;

			EnterCriticalSection( &This->m_cs );

			if ( !This->ctx.codec )
			{
				hr = E_UNEXPECTED;
				goto failed;
			}
		}
	}

fail:
	hr = E_FAIL;
failed:
	LeaveCriticalSection( &This->m_cs );
	return hr;
}

static HRESULT FFMVWrapper_EndTransform( CTransformBaseImpl* pImpl )
{
	CFFMVWrapperImpl*	This = pImpl->m_pUserData;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return E_UNEXPECTED;
	if ( !This->ctx.codec )
		return NOERROR;

	EnterCriticalSection( &This->m_cs );

	avcodec_close( &This->ctx );
	memset( &This->ctx, 0, sizeof(This->ctx) );

	LeaveCriticalSection( &This->m_cs );

	FFMVWrapper_ReleaseDIBBuffers(This);

	return NOERROR;
}


static const TransformBaseHandlers transhandlers =
{
	FFMVWrapper_Init,
	FFMVWrapper_Cleanup,
	FFMVWrapper_CheckMediaType,
	FFMVWrapper_GetOutputTypes,
	FFMVWrapper_GetAllocProp,
	FFMVWrapper_BeginTransform,
	FFMVWrapper_ProcessReceive,
	NULL,
	FFMVWrapper_EndTransform,
};


HRESULT QUARTZ_CreateFFMVWrapper(IUnknown* punkOuter,void** ppobj)
{
	return QUARTZ_CreateTransformBase(
		punkOuter,ppobj,
		&CLSID_quartzFFMVWrapper,
		FFMVWrapper_FilterName,
		NULL, NULL,
		&transhandlers );
}

