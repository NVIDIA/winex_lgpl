/*
 * FFmpeg Parser(Splitter).
 *
 *	FIXME - no seeking
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
#include "vfw.h"
#include "winerror.h"
#include "strmif.h"
#include "control.h"
#include "vfwmsgs.h"
#include "amvideo.h"
#include "mmreg.h"
#include "uuids.h"
#include "dvdmedia.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "parser.h"
#include "mtype.h"

#include "avformat.h"


static const WCHAR QUARTZ_FFMParser_Name[] =
{ 'F','F','M',' ','S','p','l','i','t','t','e','r',0 };
static const WCHAR QUARTZ_FFMParserInPin_Name[] =
{ 'I','n',0 };
static const WCHAR QUARTZ_FFMParserOutPin_VideoPinName[] =
{ 'V','i','d','e','o',0 };
static const WCHAR QUARTZ_FFMParserOutPin_AudioPinName[] =
{ 'A','u','d','i','o',0 };
	/* FIXME */
static const WCHAR QUARTZ_FFMParserOutPin_UnknownTypePinName[] =
{ 'O','u','t',0 };



/****************************************************************************
 *
 *	CFFMParseImpl
 */


typedef struct CFFMParseImpl CFFMParseImpl;
typedef struct CFFMParseStream CFFMParseStream;

struct CFFMParseImpl
{
	AVFormatContext*	ctx;
	CParserImpl*		parser;
	LONGLONG	llPosNext, llPosTotal;
	BOOL		do_req;
	LONGLONG	llReqStart;
	LONG		lReqLen;
	UINT8*		pReqMem;
	DWORD		cStreams;
	CFFMParseStream*	pStreams;
};

struct CFFMParseStream
{
	AVCodecContext	init;
	int		rfps;
	REFERENCE_TIME	rtCur;
	REFERENCE_TIME	rtInternal;
	BOOL	bDataDiscontinuity;
};

static int quartz_open(URLContext* h, const char* filename, int flags)
{
	CFFMParseImpl* This = NULL;
	LONGLONG dummy;
	sscanf(filename, "quartz:%p", &This);
	TRACE("(%p,%d)\n", This, flags);
	h->priv_data = This;
	This->llPosNext = 0;
	This->llPosTotal = 0;
	IAsyncReader_Length( This->parser->m_pReader,
			     &This->llPosTotal,
			     &dummy );
	TRACE("pos=0, length=%lld\n", This->llPosTotal);
	return 0;
}

static int quartz_read(URLContext* h, unsigned char* buf, int size)
{
	CFFMParseImpl* This = h->priv_data;
	LONG lLength = size;
	HRESULT hr;
	TRACE("(%p,%p,%d)\n", This, buf, size);
	if ( This->llPosNext >= This->llPosTotal ) return 0;
	if ( This->llPosNext + lLength > This->llPosTotal )
		lLength = This->llPosTotal - This->llPosNext;
	TRACE("reading %d bytes\n", lLength);
	hr = IAsyncReader_SyncRead( This->parser->m_pReader,
				    This->llPosNext,
				    lLength,
				    buf );
	if (FAILED(hr)) {
		TRACE("failed\n");
		return -1;
	}
	/* SyncRead doesn't return the actual size read...
	 * let's hope for the best */
	if ( hr != S_OK ) {
		ERR("unexpected data read error\n");
		/* proceed anyway */
	}
	This->llPosNext += lLength;
	TRACE("pos=%lld\n", This->llPosNext);
	return lLength;
}

static int quartz_write(URLContext* h, unsigned char* buf, int size)
{
	CFFMParseImpl* This = h->priv_data;
	TRACE("(%p,%p,%d)\n", This, buf, size);
	return -1;
}

static offset_t quartz_seek(URLContext* h, offset_t pos, int whence)
{
	CFFMParseImpl* This = h->priv_data;
	TRACE("(%p,%lld,%d)\n", This, pos, whence);
	switch (whence) {
	case SEEK_SET:
		This->llPosNext = pos;
		break;
	case SEEK_END:
		This->llPosNext = 0;
		IAsyncReader_Length( This->parser->m_pReader,
				     &This->llPosNext,
				     NULL );
		/* fall through */
	case SEEK_CUR:
		This->llPosNext += pos;
		break;
	}
	TRACE("pos=%lld\n", This->llPosNext);
	return This->llPosNext;
}

static int quartz_close(URLContext* h)
{
	CFFMParseImpl* This = h->priv_data;
	TRACE("(%p)\n", This);
	This->llPosNext = 0;
	This->llPosTotal = 0;
	return 0;
}

static int quartz_get_buffer(URLContext* h, unsigned char *buf, offset_t pos, int size)
{
	CFFMParseImpl* This = h->priv_data;
	TRACE("(%p,%p,%lld,%d)\n", This, buf, pos, size);
	This->llReqStart = pos;
	This->lReqLen = size;
	This->pReqMem = buf;
	if (This->do_req) {
		LONGLONG oldPos = This->llPosNext;
		int len;
		This->llPosNext = pos;
		len = quartz_read(h, buf, size);
		This->llPosNext = oldPos;
		return len;
	}
	else
		return size;
}

static URLProtocol quartz_protocol = {
	"quartz",
	quartz_open,
	quartz_read,
	quartz_write,
	quartz_seek,
	quartz_close,
	quartz_get_buffer
};

static HRESULT CFFMParseImpl_InitParser( CParserImpl* pImpl, ULONG* pcStreams )
{
	CFFMParseImpl*	This = NULL;
	DWORD	n;
	char	fname[64];
	static BOOL ffm_init = FALSE;

	TRACE("(%p,%p)\n",pImpl,pcStreams);

	if (!ffm_init) {
		ffm_init = TRUE;
		register_protocol(&quartz_protocol);
		/* register FFmpeg demuxers */
		/* let's only support MPEG for now */
		mpegps_init();
		mpegts_init();
		avcodec_register_all();
	}

	This = (CFFMParseImpl*)QUARTZ_AllocMem( sizeof(CFFMParseImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;
	pImpl->m_pUserData = This;
	ZeroMemory( This, sizeof(CFFMParseImpl) );
	This->parser = pImpl;
	This->do_req = TRUE;

	sprintf( fname, "quartz:%p", This );
	/* open for stream scanning */
	TRACE("detecting format\n");
	if (av_open_input_file( &This->ctx, fname, NULL, 0, NULL) < 0)
	{
		TRACE("format not detected by FFmpeg\n");
		return E_FAIL;
	}
	TRACE("detecting streams\n");
	if (av_find_stream_info( This->ctx ) < 0)
	{
		TRACE("streams not detected by FFmpeg\n");
		av_close_input_file( This->ctx );
		This->ctx = NULL;
		return E_FAIL;
	}
	TRACE("found %d streams\n", This->ctx->nb_streams);
	This->cStreams = This->ctx->nb_streams;
	This->pStreams = (CFFMParseStream*)QUARTZ_AllocMem( sizeof(CFFMParseStream) * This->cStreams );
	if ( This->pStreams == NULL )
	{
		av_close_input_file( This->ctx );
		This->ctx = NULL;
		return E_OUTOFMEMORY;
	}
	for (n=0; n<This->ctx->nb_streams; n++)
	{
		memcpy( &This->pStreams[n].init, &This->ctx->streams[n]->codec, sizeof(AVCodecContext) );
		This->pStreams[n].rfps = This->ctx->streams[n]->r_frame_rate;
		This->pStreams[n].rtCur = 0;
		This->pStreams[n].rtInternal = 0;
		This->pStreams[n].bDataDiscontinuity = TRUE;
	}
	av_close_input_file( This->ctx );
	This->ctx = NULL;

	/* reopen for actual reading */
	TRACE("initializing reader\n");
	if (av_open_input_file( &This->ctx, fname, NULL, 0, NULL) < 0)
	{
		TRACE("format not detected by FFmpeg\n");
		return E_FAIL;
	}

	This->do_req = FALSE;

	*pcStreams = This->cStreams;
	return S_OK;
}

static HRESULT CFFMParseImpl_UninitParser( CParserImpl* pImpl )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;
	ULONG	nIndex;

	TRACE("(%p)\n",This);

	if ( This == NULL )
		return NOERROR;

	/* destruct */

	if ( This->ctx )
	{
		av_close_input_file( This->ctx );
		This->ctx = NULL;
	}

	if ( This->pStreams != NULL )
	{
		for ( nIndex = 0; nIndex < This->cStreams; nIndex++ )
		{
			/* release this stream */


		}
		QUARTZ_FreeMem( This->pStreams );
		This->pStreams = NULL;
	}

	QUARTZ_FreeMem( This );
	pImpl->m_pUserData = NULL;

	return NOERROR;
}

static LPCWSTR CFFMParseImpl_GetOutPinName( CParserImpl* pImpl, ULONG nStreamIndex )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;

	TRACE("(%p,%u)\n",This,nStreamIndex);

	if ( This == NULL || nStreamIndex >= This->cStreams )
		return NULL;

	switch ( This->pStreams[nStreamIndex].init.codec_type )
	{
	case CODEC_TYPE_VIDEO:
		return QUARTZ_FFMParserOutPin_VideoPinName;
	case CODEC_TYPE_AUDIO:
		return QUARTZ_FFMParserOutPin_AudioPinName;
	default:
		FIXME("unknown FFmpeg stream type %d\n", This->pStreams[nStreamIndex].init.codec_type);
	}

	/* FIXME */
	return QUARTZ_FFMParserOutPin_UnknownTypePinName;
}

static HRESULT CFFMParseImpl_GetStreamType( CParserImpl* pImpl, ULONG nStreamIndex, AM_MEDIA_TYPE* pmt )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;
	AVCodecContext*	init;

	TRACE("(%p,%u,%p)\n",This,nStreamIndex,pmt);

	if ( This == NULL )
		return E_UNEXPECTED;
	if ( nStreamIndex >= This->cStreams )
		return E_INVALIDARG;

	ZeroMemory( pmt, sizeof(AM_MEDIA_TYPE) );

	init = &This->pStreams[nStreamIndex].init;

	switch ( init->codec_type )
	{
	case CODEC_TYPE_VIDEO:
		memcpy( &pmt->majortype, &MEDIATYPE_Video, sizeof(GUID) );
		switch ( init->codec_id )
		{
		case CODEC_ID_MPEG1VIDEO:
			switch ( init->sub_id )
			{
			case 1: /* MPEG 1 */
			{
				MPEG1VIDEOINFO*	info;
				TRACE("video MPEG-1\n");
				memcpy( &pmt->subtype, &MEDIASUBTYPE_MPEG1Payload, sizeof(GUID) );
			mpeg1:
				memcpy( &pmt->formattype, &FORMAT_MPEGVideo, sizeof(GUID) );
				pmt->bFixedSizeSamples = 0;
				pmt->bTemporalCompression = 1;
				pmt->lSampleSize = 0;
				pmt->cbFormat = sizeof(MPEG1VIDEOINFO);
				pmt->pbFormat = (BYTE*)CoTaskMemAlloc( sizeof(MPEG1VIDEOINFO) );
				if ( pmt->pbFormat == NULL )
					return E_OUTOFMEMORY;
				ZeroMemory( pmt->pbFormat, sizeof(MPEG1VIDEOINFO) );
				info = (MPEG1VIDEOINFO*)pmt->pbFormat;
				info->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				info->hdr.bmiHeader.biWidth = init->width;
				info->hdr.bmiHeader.biHeight = -init->height;
				info->hdr.bmiHeader.biPlanes = 1;
				info->dwStartTimeCode = 0;
				/* FIXME: sequence header? */
				info->cbSequenceHeader = 0;
				return S_OK;
			}
			case 2: /* MPEG 2 */
			{
				MPEG2VIDEOINFO*	info;
				TRACE("video MPEG-2\n");
				memcpy( &pmt->subtype, &MEDIASUBTYPE_MPEG2_VIDEO, sizeof(GUID) );

				/* the MPEG2 specific format fields aren't supported yet,
				 * so use the MPEG1 format for now */
				goto mpeg1;

				memcpy( &pmt->formattype, &FORMAT_MPEG2_VIDEO, sizeof(GUID) );
				pmt->bFixedSizeSamples = 0;
				pmt->bTemporalCompression = 1;
				pmt->lSampleSize = 0;
				pmt->cbFormat = sizeof(MPEG2VIDEOINFO);
				pmt->pbFormat = (BYTE*)CoTaskMemAlloc( sizeof(MPEG2VIDEOINFO) );
				if ( pmt->pbFormat == NULL )
					return E_OUTOFMEMORY;
				ZeroMemory( pmt->pbFormat, sizeof(MPEG2VIDEOINFO) );
				info = (MPEG2VIDEOINFO*)pmt->pbFormat;
				info->hdr.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				info->hdr.bmiHeader.biWidth = init->width;
				info->hdr.bmiHeader.biHeight = -init->height;
				info->hdr.dwInterlaceFlags = AMINTERLACE_FieldPatBothRegular; /* FIXME? */
				info->hdr.dwCopyProtectFlags = AMCOPYPROTECT_RestrictDuplication; /* FIXME? */
				info->hdr.dwPictAspectRatioX = 1; /* FIXME? */
				info->hdr.dwPictAspectRatioY = 1; /* FIXME? */
				info->dwStartTimeCode = 0;
				/* FIXME: sequence header? */
				info->cbSequenceHeader = 0;
				info->dwProfile = AM_MPEG2Profile_Simple; /* FIXME */
				info->dwLevel = AM_MPEG2Level_Low; /* FIXME */
				info->dwFlags = 0; /* FIXME? */
				return S_OK;
			}
			default:
				ERR("unknown MPEG version\n");
				break;
			}
		default:
			ERR("unknown FFmpeg video codec\n");
			break;
		}
		return E_FAIL;
	case CODEC_TYPE_AUDIO:
		memcpy( &pmt->majortype, &MEDIATYPE_Audio, sizeof(GUID) );
		memcpy( &pmt->formattype, &FORMAT_WaveFormatEx, sizeof(GUID) );

		switch ( init->codec_id )
		{
		case CODEC_ID_MP2:
		{
			MPEG1WAVEFORMAT*	info;
			TRACE("audio MPEG\n");
			memcpy( &pmt->subtype, &MEDIASUBTYPE_MPEG1AudioPayload, sizeof(GUID) );
			pmt->bFixedSizeSamples = 0;
			pmt->bTemporalCompression = 1;
			pmt->lSampleSize = 0;
			pmt->cbFormat = sizeof(MPEG1WAVEFORMAT);
			pmt->pbFormat = (BYTE*)CoTaskMemAlloc( sizeof(MPEG1WAVEFORMAT) );
			if ( pmt->pbFormat == NULL )
				return E_OUTOFMEMORY;
			ZeroMemory( pmt->pbFormat, sizeof(MPEG1WAVEFORMAT) );
			info = (MPEG1WAVEFORMAT*)pmt->pbFormat;
			info->dwHeadBitrate = init->bit_rate;
			/* FFmpeg doesn't report back the layer or mode
			 * (I guess we could hack it though?) */
			info->fwHeadLayer = ACM_MPEG_LAYER3; /* FIXME */
			if (init->channels > 1)
				info->fwHeadMode = ACM_MPEG_STEREO; /* FIXME */
			else
				info->fwHeadMode = ACM_MPEG_SINGLECHANNEL;
			info->fwHeadModeExt = 0; /* FIXME */
			info->wHeadEmphasis = 0; /* FIXME */
			info->fwHeadFlags = ACM_MPEG_ID_MPEG1; /* FIXME */
			info->dwPTSLow = 0;
			info->dwPTSHigh = 0;

			info->wfx.wFormatTag = WAVE_FORMAT_MPEG;
			info->wfx.nChannels = init->channels;
			info->wfx.nSamplesPerSec = init->sample_rate;
			info->wfx.nAvgBytesPerSec = info->dwHeadBitrate >> 3;
			if (info->fwHeadLayer == ACM_MPEG_LAYER3 )
				info->wfx.nBlockAlign = 1;
			else
				info->wfx.nBlockAlign = init->frame_size;
			info->wfx.wBitsPerSample = 0;
			info->wfx.cbSize = sizeof(MPEG1WAVEFORMAT) - sizeof(WAVEFORMATEX);
			if ( info->fwHeadLayer != ACM_MPEG_LAYER3 )
			{
				pmt->bFixedSizeSamples = 1;
				pmt->lSampleSize = info->wfx.nBlockAlign;
			}
			return S_OK;
		}
		default:
			ERR("unknown FFmpeg audio codec\n");
			break;
		}
		return E_FAIL;
	default:
		FIXME("unknown FFmpeg stream type %d\n", init->codec_type);
		break;
	}

	FIXME("stub\n");
	return E_NOTIMPL;
}

static HRESULT CFFMParseImpl_CheckStreamType( CParserImpl* pImpl, ULONG nStreamIndex, const AM_MEDIA_TYPE* pmt )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;
	HRESULT hr;
	AM_MEDIA_TYPE	mt;

	TRACE("(%p,%u,%p)\n",This,nStreamIndex,pmt);

	if ( This == NULL )
		return E_UNEXPECTED;
	if ( nStreamIndex >= This->cStreams )
		return E_INVALIDARG;

	hr = CFFMParseImpl_GetStreamType( pImpl, nStreamIndex, &mt );
	if ( FAILED(hr) )
		return hr;
	if ( !IsEqualGUID( &pmt->majortype, &mt.majortype ) ||
		 !IsEqualGUID( &pmt->subtype, &mt.subtype ) ||
		 !IsEqualGUID( &pmt->formattype, &mt.formattype ) )
	{
		hr = E_FAIL;
		goto end;
	}

	TRACE("check format\n");
	hr = S_OK;

	switch ( This->pStreams[nStreamIndex].init.codec_type )
	{
	case CODEC_TYPE_VIDEO:
		if ( IsEqualGUID( &mt.formattype, &FORMAT_MPEGVideo ) )
		{
			MPEG1VIDEOINFO *info = (MPEG1VIDEOINFO*)mt.pbFormat;
			MPEG1VIDEOINFO *check = (MPEG1VIDEOINFO*)pmt->pbFormat;
			/* MPEG-1 Video */
			if ( pmt->cbFormat != mt.cbFormat ||
				 pmt->pbFormat == NULL )
			{
				hr = E_FAIL;
				goto end;
			}
			if ( memcmp( info, check, sizeof(MPEG1VIDEOINFO) ) != 0 )
			{
				hr = E_FAIL;
				goto end;
			}
		}
		else
		if ( IsEqualGUID( &mt.formattype, &FORMAT_MPEG2_VIDEO ) )
		{
			MPEG2VIDEOINFO *info = (MPEG2VIDEOINFO*)mt.pbFormat;
			MPEG2VIDEOINFO *check = (MPEG2VIDEOINFO*)pmt->pbFormat;
			/* MPEG-2 Video */
			if ( pmt->cbFormat != mt.cbFormat ||
				 pmt->pbFormat == NULL )
			{
				hr = E_FAIL;
				goto end;
			}
			if ( memcmp( info, check, sizeof(MPEG2VIDEOINFO) ) != 0 )
			{
				hr = E_FAIL;
				goto end;
			}
		}
		else
		{
			hr = E_FAIL;
			goto end;
		}
		break;
	case CODEC_TYPE_AUDIO:
		if ( IsEqualGUID( &mt.formattype, &FORMAT_WaveFormatEx ) )
		{
			WAVEFORMATEX *info = (WAVEFORMATEX*)mt.pbFormat;
			WAVEFORMATEX *check = (WAVEFORMATEX*)pmt->pbFormat;
			if ( mt.cbFormat != pmt->cbFormat ||
				 pmt->pbFormat == NULL )
			{
				hr = E_FAIL;
				goto end;
			}

			if ( memcmp( info, check, sizeof(WAVEFORMATEX) ) != 0 )
			{
				hr = E_FAIL;
				goto end;
			}
		}
		else
		{
			hr = E_FAIL;
			goto end;
		}

		break;
	default:
		FIXME( "unknown FFmpeg stream type %d\n", This->pStreams[nStreamIndex].init.codec_type );
		hr = E_FAIL;
		goto end;
	}

	hr = S_OK;
end:
	QUARTZ_MediaType_Free( &mt );

	TRACE("%08x\n",hr);

	return hr;
}

static HRESULT CFFMParseImpl_GetAllocProp( CParserImpl* pImpl, ALLOCATOR_PROPERTIES* pReqProp )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;

	TRACE("(%p,%p)\n",This,pReqProp);

	if ( This == NULL )
		return E_UNEXPECTED;

	ZeroMemory( pReqProp, sizeof(ALLOCATOR_PROPERTIES) );
	pReqProp->cBuffers = This->cStreams;
	pReqProp->cbBuffer = AVCODEC_MAX_AUDIO_FRAME_SIZE;

	TRACE("buf %ld size %ld\n", pReqProp->cBuffers, pReqProp->cbBuffer);

	return S_OK;
}

static HRESULT CFFMParseImpl_GetNextRequest( CParserImpl* pImpl, ULONG* pnStreamIndex, LONGLONG* pllStart, LONG* plLength, REFERENCE_TIME* prtStart, REFERENCE_TIME* prtStop, DWORD* pdwSampleFlags )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;
	REFERENCE_TIME	rtNext;
	AVPacket	pkt;
	CFFMParseStream*pStream;

	if ( This == NULL )
		return E_UNEXPECTED;
	*pdwSampleFlags = AM_SAMPLE_SPLICEPOINT;

	TRACE("(%p)\n",This);

restart:
	This->llReqStart = -1;
	This->lReqLen = 0;

	if (av_read_packet( This->ctx, &pkt ) < 0) {
		return S_FALSE;
	}

	/* this ugliness should be fixed someday */

	if (This->llReqStart == -1) {
		ERR("no bulk read requested by FFmpeg, can't handle\n");
		return E_FAIL;
	}

	if (This->lReqLen != pkt.size) {
		ERR("packet size doesn't match bulk read, can't handle\n");
		return E_FAIL;
	}

	if (This->pReqMem != pkt.data) {
		ERR("packet address doesn't match bulk read, can't handle\n");
		return E_FAIL;
	}

	if (pkt.size > AVCODEC_MAX_AUDIO_FRAME_SIZE) {
		/* fix GetAllocProp if this happens */
		ERR("bulk read too large, can't handle\n");
		return E_FAIL;
	}

	TRACE("got packet: stream %d, size %d\n", pkt.stream_index, pkt.size);

	if (pkt.stream_index >= This->cStreams) {
		ERR("data from unknown stream %d! (id %d, type %d, codec %d)\n", pkt.stream_index,
		    This->ctx->streams[pkt.stream_index]->id,
		    This->ctx->streams[pkt.stream_index]->codec.codec_type,
		    This->ctx->streams[pkt.stream_index]->codec.codec_id);
		goto restart;
	}

	*pnStreamIndex = pkt.stream_index;
	*pllStart = This->llReqStart;
	*plLength = This->lReqLen;

	pStream = &This->pStreams[pkt.stream_index];

	if ( pStream->bDataDiscontinuity )
	{
		*pdwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;
		pStream->bDataDiscontinuity = FALSE;
	}


	rtNext = pStream->rtCur;

	*prtStart = rtNext;
	*prtStop = rtNext;

	switch ( pStream->init.codec_type )
	{
	case CODEC_TYPE_VIDEO:
		TRACE("video - frame time (%f fps, real %f)\n", pStream->init.frame_rate / (double)pStream->init.frame_rate_base, pStream->rfps / (double)pStream->init.frame_rate_base);
		/* unfortunately, not every packet is a frame, so we'll have to leave the timing to the codec */
		break;
	case CODEC_TYPE_AUDIO:
		TRACE("audio - bitrate time (%d bps)\n", pStream->init.bit_rate);
		pStream->rtInternal += (REFERENCE_TIME)*plLength;
		rtNext = pStream->rtInternal * (REFERENCE_TIME)QUARTZ_TIMEUNITS * 8 / (REFERENCE_TIME)pStream->init.bit_rate;
		break;
	default:
		FIXME("unknown FFmpeg stream type %d\n", pStream->init.codec_type);
	}

	pStream->rtCur = rtNext;
	*prtStop = rtNext;

	TRACE("return %u / %#x / %ld-%d / %lu-%lu\n",
		*pnStreamIndex,*pdwSampleFlags, (long)*pllStart,*plLength,
		(unsigned long)((*prtStart)*1000/QUARTZ_TIMEUNITS),
		(unsigned long)((*prtStop)*1000/QUARTZ_TIMEUNITS));

	return S_OK;
}

static HRESULT CFFMParseImpl_ProcessSample( CParserImpl* pImpl, ULONG nStreamIndex, LONGLONG llStart, LONG lLength, IMediaSample* pSample )
{
	CFFMParseImpl*	This = (CFFMParseImpl*)pImpl->m_pUserData;

	TRACE("(%p,%u,%ld,%d,%p)\n",This,nStreamIndex,(long)llStart,lLength,pSample);

	if ( This == NULL )
		return E_UNEXPECTED;

	switch ( This->pStreams[nStreamIndex].init.codec_type )
	{
	case CODEC_TYPE_VIDEO:
		IMediaSample_SetTime(pSample, NULL, NULL);
		break;
	case CODEC_TYPE_AUDIO:
	default:
		break;
	}

#if 0
	{
		BYTE *data;
		long len;
		IMediaSample_GetPointer(pSample, &data);
		len = IMediaSample_GetActualDataLength(pSample);
		av_hex_dump(data, len);
	}
#endif

	return NOERROR;
}


static const struct ParserHandlers CFFMParseImpl_Handlers =
{
	CFFMParseImpl_InitParser,
	CFFMParseImpl_UninitParser,
	CFFMParseImpl_GetOutPinName,
	CFFMParseImpl_GetStreamType,
	CFFMParseImpl_CheckStreamType,
	CFFMParseImpl_GetAllocProp,
	CFFMParseImpl_GetNextRequest,
	CFFMParseImpl_ProcessSample,

	/* for IQualityControl */
	NULL, /* pQualityNotify */

	/* for seeking */
	NULL, /* pGetSeekingCaps */
	NULL, /* pIsTimeFormatSupported */
	NULL, /* pGetCurPos */
	NULL, /* pSetCurPos */
	NULL, /* pGetDuration */
	NULL, /* pGetStopPos */
	NULL, /* pSetStopPos */
	NULL, /* pGetPreroll */
};

HRESULT QUARTZ_CreateFFMSplitter(IUnknown* punkOuter,void** ppobj)
{
	return QUARTZ_CreateParser(
		punkOuter,ppobj,
		&CLSID_quartzFFMSplitter,
		QUARTZ_FFMParser_Name,
		QUARTZ_FFMParserInPin_Name,
		&CFFMParseImpl_Handlers );
}


