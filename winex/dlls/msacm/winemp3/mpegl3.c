/*
 * MPEG Layer 3 handling
 *
 *      Copyright (C) 2002		Eric Pouech
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 *
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

#include <assert.h>
#include <string.h>
#include "winnls.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "msacm.h"
#include "mmreg.h"
#include "../msacmdrv.h"
#include "mpg123.h"
#include "mpglib.h"

#include "wine/format_string_types.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mpeg3);

/***********************************************************************
 *           MPEG3_drvOpen
 */
static	DWORD	MPEG3_drvOpen(LPCSTR str)
{
    return 1;
}

/***********************************************************************
 *           MPEG3_drvClose
 */
static	DWORD	MPEG3_drvClose(DWORD_PTR dwDevID)
{
    return 1;
}

typedef struct tagAcmMpeg3Data
{
    void (*convert)(PACMDRVSTREAMINSTANCE adsi,
		    const unsigned char*, LPDWORD, unsigned char*, LPDWORD);
    struct mpstr mp;
} AcmMpeg3Data;

/* table to list all supported formats... those are the basic ones. this
 * also helps given a unique index to each of the supported formats
 */
typedef	struct
{
    int		nChannels;
    int		nBits;
    int		rate;
} Format;

static Format PCM_Formats[] =
{
    {1,  8,  8000}, {2,  8,  8000}, {1, 16,  8000}, {2, 16,  8000},
    {1,  8, 11025}, {2,  8, 11025}, {1, 16, 11025}, {2, 16, 11025},
    {1,  8, 16000}, {2,  8, 16000}, {1, 16, 16000}, {2, 16, 16000},
    {1,  8, 22050}, {2,  8, 22050}, {1, 16, 22050}, {2, 16, 22050},
    {1,  8, 44100}, {2,  8, 44100}, {1, 16, 44100}, {2, 16, 44100},
    {1,  8, 48000}, {2,  8, 48000}, {1, 16, 48000}, {2, 16, 48000},
};

static Format MPEG3_Formats[] =
{
    {1,  0,  8000}, {2,	0,  8000},  {1,  0, 11025}, {2,	 0, 11025},
    {1,  0, 16000}, {2,	0, 16000},  {1,  0, 22050}, {2,	 0, 22050},
    {1,  0, 44100}, {2,	0, 44100},  {1,  0, 48000}, {2,	 0, 48000},
};

#define	NUM_PCM_FORMATS		(sizeof(PCM_Formats) / sizeof(PCM_Formats[0]))
#define	NUM_MPEG3_FORMATS	(sizeof(MPEG3_Formats) / sizeof(MPEG3_Formats[0]))

/***********************************************************************
 *           MPEG3_GetFormatIndex
 */
static	DWORD	MPEG3_GetFormatIndex(LPWAVEFORMATEX wfx)
{
    int 	i, hi;
    Format*	fmts;

    switch (wfx->wFormatTag)
    {
    case WAVE_FORMAT_PCM:
	hi = NUM_PCM_FORMATS;
	fmts = PCM_Formats;
	break;
    case WAVE_FORMAT_MPEGLAYER3:
	hi = NUM_MPEG3_FORMATS;
	fmts = MPEG3_Formats;
	break;
    default:
	return 0xFFFFFFFF;
    }

    for (i = 0; i < hi; i++)
    {
	if (wfx->nChannels == fmts[i].nChannels &&
	    wfx->nSamplesPerSec == fmts[i].rate &&
	    wfx->wBitsPerSample == fmts[i].nBits)
	    return i;
    }

    return 0xFFFFFFFF;
}

static void mp3_horse(PACMDRVSTREAMINSTANCE adsi,
                      const unsigned char* src, LPDWORD nsrc,
                      unsigned char* dst, LPDWORD ndst)
{
    AcmMpeg3Data*       amd = (AcmMpeg3Data*)adsi->dwDriver;
    int                 size, ret;
    DWORD               dpos = 0;

    ret = decodeMP3(&amd->mp, (char*)src, *nsrc, (char *)dst, *ndst, &size);
    if (ret != MP3_OK)
    {
        *ndst = *nsrc = 0;
        return;
    }
    do {
        dpos += size;
        if (*ndst - dpos < 4608) break;
        ret = decodeMP3(&amd->mp, NULL, 0,
                        (char *)dst + dpos, *ndst - dpos, &size);
    } while (ret == MP3_OK);
    *ndst = dpos;
}

/***********************************************************************
 *           MPEG3_DriverDetails
 *
 */
static	LRESULT MPEG3_DriverDetails(PACMDRIVERDETAILSW add)
{
    add->fccType = ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC;
    add->fccComp = ACMDRIVERDETAILS_FCCCOMP_UNDEFINED;
    add->wMid = 0xFF;
    add->wPid = 0x00;
    add->vdwACM = 0x01000000;
    add->vdwDriver = 0x01000000;
    add->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
    add->cFormatTags = 2; /* PCM, MPEG3 */
    add->cFilterTags = 0;
    add->hicon = (HICON)0;
    MultiByteToWideChar( CP_ACP, 0, "WINE-MPEG3", -1,
                         add->szShortName, sizeof(add->szShortName)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, "Wine MPEG3 decoder", -1,
                         add->szLongName, sizeof(add->szLongName)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, "Brought to you by the Wine team (based on mpglib by Michael Hipp)...", -1,
                         add->szCopyright, sizeof(add->szCopyright)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, "Refer to LICENSE file", -1,
                         add->szLicensing, sizeof(add->szLicensing)/sizeof(WCHAR) );
    add->szFeatures[0] = 0;

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           MPEG3_FormatTagDetails
 *
 */
static	LRESULT	MPEG3_FormatTagDetails(PACMFORMATTAGDETAILSW aftd, DWORD dwQuery)
{
    static WCHAR szPcm[]={'P','C','M',0};
    static WCHAR szMpeg3[]={'M','P','e','g','3',0};

    switch (dwQuery)
    {
    case ACM_FORMATTAGDETAILSF_INDEX:
	if (aftd->dwFormatTagIndex >= 2) return ACMERR_NOTPOSSIBLE;
	break;
    case ACM_FORMATTAGDETAILSF_LARGESTSIZE:
	if (aftd->dwFormatTag == WAVE_FORMAT_UNKNOWN)
        {
            aftd->dwFormatTagIndex = 1; /* WAVE_FORMAT_MPEGLAYER3 is bigger than PCM */
	    break;
	}
	/* fall thru */
    case ACM_FORMATTAGDETAILSF_FORMATTAG:
	switch (aftd->dwFormatTag)
        {
	case WAVE_FORMAT_PCM:		aftd->dwFormatTagIndex = 0; break;
	case WAVE_FORMAT_MPEGLAYER3:    aftd->dwFormatTagIndex = 1; break;
	default:			return ACMERR_NOTPOSSIBLE;
	}
	break;
    default:
	WARN("Unsupported query %08x\n", dwQuery);
	return MMSYSERR_NOTSUPPORTED;
    }

    aftd->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
    switch (aftd->dwFormatTagIndex)
    {
    case 0:
	aftd->dwFormatTag = WAVE_FORMAT_PCM;
	aftd->cbFormatSize = sizeof(PCMWAVEFORMAT);
	aftd->cStandardFormats = NUM_PCM_FORMATS;
        lstrcpyW(aftd->szFormatTag, szPcm);
        break;
    case 1:
	aftd->dwFormatTag = WAVE_FORMAT_MPEGLAYER3;
	aftd->cbFormatSize = sizeof(MPEGLAYER3WAVEFORMAT);
	aftd->cStandardFormats = NUM_MPEG3_FORMATS;
        lstrcpyW(aftd->szFormatTag, szMpeg3);
	break;
    }
    return MMSYSERR_NOERROR;
}

static void fill_in_wfx(unsigned cbwfx, WAVEFORMATEX* wfx, unsigned bit_rate)
{
    MPEGLAYER3WAVEFORMAT*   mp3wfx = (MPEGLAYER3WAVEFORMAT*)wfx;

    wfx->nAvgBytesPerSec = bit_rate / 8;
    if (cbwfx >= sizeof(WAVEFORMATEX))
        wfx->cbSize = sizeof(MPEGLAYER3WAVEFORMAT) - sizeof(WAVEFORMATEX);
    if (cbwfx >= sizeof(MPEGLAYER3WAVEFORMAT))
    {
        mp3wfx->wID = MPEGLAYER3_ID_MPEG;
        mp3wfx->fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
        mp3wfx->nBlockSize = (bit_rate * 144) / wfx->nSamplesPerSec;
        mp3wfx->nFramesPerBlock = 1;
        mp3wfx->nCodecDelay = 0x0571;
    }
}

/***********************************************************************
 *           MPEG3_FormatDetails
 *
 */
static	LRESULT	MPEG3_FormatDetails(PACMFORMATDETAILSW afd, DWORD dwQuery)
{
    switch (dwQuery)
    {
    case ACM_FORMATDETAILSF_FORMAT:
	if (MPEG3_GetFormatIndex(afd->pwfx) == 0xFFFFFFFF) return ACMERR_NOTPOSSIBLE;
	break;
    case ACM_FORMATDETAILSF_INDEX:
	afd->pwfx->wFormatTag = afd->dwFormatTag;
	switch (afd->dwFormatTag)
        {
	case WAVE_FORMAT_PCM:
	    if (afd->dwFormatIndex >= NUM_PCM_FORMATS) return ACMERR_NOTPOSSIBLE;
	    afd->pwfx->nChannels = PCM_Formats[afd->dwFormatIndex].nChannels;
	    afd->pwfx->nSamplesPerSec = PCM_Formats[afd->dwFormatIndex].rate;
	    afd->pwfx->wBitsPerSample = PCM_Formats[afd->dwFormatIndex].nBits;
	    /* native MSACM uses a PCMWAVEFORMAT structure, so cbSize is not accessible
	     * afd->pwfx->cbSize = 0;
	     */
	    afd->pwfx->nBlockAlign =
		(afd->pwfx->nChannels * afd->pwfx->wBitsPerSample) / 8;
	    afd->pwfx->nAvgBytesPerSec =
		afd->pwfx->nSamplesPerSec * afd->pwfx->nBlockAlign;
	    break;
	case WAVE_FORMAT_MPEGLAYER3:
	    if (afd->dwFormatIndex >= NUM_MPEG3_FORMATS) return ACMERR_NOTPOSSIBLE;
	    afd->pwfx->nChannels = MPEG3_Formats[afd->dwFormatIndex].nChannels;
	    afd->pwfx->nSamplesPerSec = MPEG3_Formats[afd->dwFormatIndex].rate;
	    afd->pwfx->wBitsPerSample = MPEG3_Formats[afd->dwFormatIndex].nBits;
	    afd->pwfx->nBlockAlign = 1;
            fill_in_wfx(afd->cbwfx, afd->pwfx, 192000);
	    break;
	default:
	    WARN("Unsupported tag %08x\n", afd->dwFormatTag);
	    return MMSYSERR_INVALPARAM;
	}
	break;
    default:
	WARN("Unsupported query %08x\n", dwQuery);
	return MMSYSERR_NOTSUPPORTED;
    }
    afd->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
    afd->szFormat[0] = 0; /* let MSACM format this for us... */

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           MPEG3_FormatSuggest
 *
 */
static	LRESULT	MPEG3_FormatSuggest(PACMDRVFORMATSUGGEST adfs)
{
    /* some tests ... */
    TRACE("got here yeah, src=%u, dst=%u\n", adfs->cbwfxSrc, adfs->cbwfxDst);
    if (adfs->cbwfxSrc < sizeof(PCMWAVEFORMAT) ||
	adfs->cbwfxDst < sizeof(PCMWAVEFORMAT) ||
	MPEG3_GetFormatIndex(adfs->pwfxSrc) == 0xFFFFFFFF) return ACMERR_NOTPOSSIBLE;
    /* FIXME: should do those tests against the real size (according to format tag */

    TRACE("flags: 0x%x\n", adfs->fdwSuggest);
    TRACE("source: channels=%d, bits=%d\n",
          adfs->pwfxSrc->nChannels, adfs->pwfxSrc->wBitsPerSample);
    TRACE("dest: channels=%d, bits=%d\n",
          adfs->pwfxDst->nChannels, adfs->pwfxSrc->wBitsPerSample);

    /* If no suggestion for destination, then copy source value */
    if (!(adfs->fdwSuggest & ACM_FORMATSUGGESTF_NCHANNELS))
	adfs->pwfxDst->nChannels = adfs->pwfxSrc->nChannels;
    if (!(adfs->fdwSuggest & ACM_FORMATSUGGESTF_NSAMPLESPERSEC))
        adfs->pwfxDst->nSamplesPerSec = adfs->pwfxSrc->nSamplesPerSec;

    if (!(adfs->fdwSuggest & ACM_FORMATSUGGESTF_WBITSPERSAMPLE))
    {
	if (adfs->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM)
            adfs->pwfxDst->wBitsPerSample = 4;
        else
            adfs->pwfxDst->wBitsPerSample = 16;
    }
    if (!(adfs->fdwSuggest & ACM_FORMATSUGGESTF_WFORMATTAG))
    {
	if (adfs->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM)
            adfs->pwfxDst->wFormatTag = WAVE_FORMAT_MPEGLAYER3;
        else
            adfs->pwfxDst->wFormatTag = WAVE_FORMAT_PCM;
    }

    /* check if result is ok */
    if (MPEG3_GetFormatIndex(adfs->pwfxDst) == 0xFFFFFFFF) return ACMERR_NOTPOSSIBLE;

    /* recompute other values */
    switch (adfs->pwfxDst->wFormatTag)
    {
    case WAVE_FORMAT_PCM:
        adfs->pwfxDst->nBlockAlign = (adfs->pwfxDst->nChannels * adfs->pwfxDst->wBitsPerSample) / 8;
        adfs->pwfxDst->nAvgBytesPerSec = adfs->pwfxDst->nSamplesPerSec * adfs->pwfxDst->nBlockAlign;
        break;
    case WAVE_FORMAT_MPEGLAYER3:
        adfs->pwfxDst->nBlockAlign = 1;
        fill_in_wfx(adfs->cbwfxDst, adfs->pwfxDst, 192000);
        break;
    default:
        FIXME("\n");
        break;
    }

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           MPEG3_Reset
 *
 */
static	void	MPEG3_Reset(PACMDRVSTREAMINSTANCE adsi, AcmMpeg3Data* aad)
{
}

/***********************************************************************
 *           MPEG3_StreamOpen
 *
 */
static	LRESULT	MPEG3_StreamOpen(PACMDRVSTREAMINSTANCE adsi)
{
    AcmMpeg3Data *aad;

    /* Asynchronous mode not supported */
    if (adsi->fdwOpen & ACM_STREAMOPENF_ASYNC)
        return ACMERR_NOTPOSSIBLE;

    if (MPEG3_GetFormatIndex(adsi->pwfxSrc) == 0xFFFFFFFF ||
        MPEG3_GetFormatIndex(adsi->pwfxDst) == 0xFFFFFFFF)
        return ACMERR_NOTPOSSIBLE;

    if ((adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_MPEGLAYER3 && adsi->pwfxSrc->cbSize != MPEGLAYER3_WFX_EXTRA_BYTES) ||
        (adsi->pwfxDst->wFormatTag == WAVE_FORMAT_MPEGLAYER3 && adsi->pwfxDst->cbSize != MPEGLAYER3_WFX_EXTRA_BYTES))
        return ACMERR_NOTPOSSIBLE;

    if ((adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM && adsi->pwfxSrc->nSamplesPerSec * adsi->pwfxSrc->nBlockAlign != adsi->pwfxSrc->nAvgBytesPerSec) ||
        (adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM && adsi->pwfxDst->nSamplesPerSec * adsi->pwfxDst->nBlockAlign != adsi->pwfxDst->nAvgBytesPerSec))
        return ACMERR_NOTPOSSIBLE;

    if (adsi->pwfxSrc->wFormatTag != WAVE_FORMAT_MPEGLAYER3 || adsi->pwfxDst->wFormatTag != WAVE_FORMAT_PCM)
    {
        FIXME("Encoding not yet supported: 0x%08x -> 0x%08x\n",
              adsi->pwfxSrc->wFormatTag, adsi->pwfxDst->wFormatTag);

        return MMSYSERR_NOTSUPPORTED;
    }

    /* resampling or mono <=> stereo not available
     * MPEG3 algo only define 16 bit per sample output
     */
    if (adsi->pwfxSrc->nSamplesPerSec != adsi->pwfxDst->nSamplesPerSec ||
        adsi->pwfxSrc->nChannels != adsi->pwfxDst->nChannels ||
        adsi->pwfxDst->wBitsPerSample != 16)
    {
        FIXME("Resampling not yet supported.\n");
        return MMSYSERR_NOTSUPPORTED;
    }

    aad = HeapAlloc(GetProcessHeap(), 0, sizeof(AcmMpeg3Data));
    if (aad == NULL)
    {
        ERR("Heap allocation failure.\n");
        return MMSYSERR_NOMEM;
    }

    adsi->dwDriver = (DWORD_PTR)aad;
    aad->convert = mp3_horse;

    InitMP3(&aad->mp);
    MPEG3_Reset(adsi, aad);

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           MPEG3_StreamClose
 *
 */
static	LRESULT	MPEG3_StreamClose(PACMDRVSTREAMINSTANCE adsi)
{
    ExitMP3(&((AcmMpeg3Data*)adsi->dwDriver)->mp);
    HeapFree(GetProcessHeap(), 0, (void*)adsi->dwDriver);
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           MPEG3_StreamSize
 *
 */
static	LRESULT MPEG3_StreamSize(PACMDRVSTREAMINSTANCE adsi, PACMDRVSTREAMSIZE adss)
{
    switch (adss->fdwSize)
    {
    case ACM_STREAMSIZEF_DESTINATION:
        TRACE("input src bytes/sec=%u\n", adsi->pwfxSrc->nAvgBytesPerSec);
        TRACE("      dst len=%u, bytes/sec=%u\n", adss->cbDstLength, adsi->pwfxDst->nAvgBytesPerSec);

        if (adss->cbDstLength == 0)
            return ACMERR_NOTPOSSIBLE;

        /* cbDstLength => cbSrcLength */
        if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM &&
            adsi->pwfxDst->wFormatTag == WAVE_FORMAT_MPEGLAYER3)
        {
            /* don't take block size into account, doesn't matter too much */
            adss->cbSrcLength = adss->cbDstLength * adsi->pwfxSrc->nAvgBytesPerSec / adsi->pwfxDst->nAvgBytesPerSec;
        }
        else if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_MPEGLAYER3 &&
                 adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM)
        {
            /* FIXME("misses the block header overhead\n"); */
            DWORD dstlen = adss->cbDstLength;
            dstlen -= dstlen%4608; /* round down to output block size */
            adss->cbSrcLength = dstlen * adsi->pwfxSrc->nAvgBytesPerSec / adsi->pwfxDst->nAvgBytesPerSec;
        }
        else
        {
            return MMSYSERR_NOTSUPPORTED;
        }
        TRACE("output src len=%u\n", adss->cbSrcLength);
        break;
    case ACM_STREAMSIZEF_SOURCE:
        TRACE("input src len=%u, bytes/sec=%u\n", adss->cbSrcLength, adsi->pwfxSrc->nAvgBytesPerSec);
        TRACE("      dst bytes/sec=%u\n", adsi->pwfxDst->nAvgBytesPerSec);

        if (adss->cbSrcLength == 0)
            return ACMERR_NOTPOSSIBLE;

        /* cbSrcLength => cbDstLength */
        if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM &&
            adsi->pwfxDst->wFormatTag == WAVE_FORMAT_MPEGLAYER3)
        {
            FIXME("misses the block header overhead\n");
            adss->cbDstLength = adss->cbSrcLength * adsi->pwfxDst->nAvgBytesPerSec / adsi->pwfxSrc->nAvgBytesPerSec;
        }
        else if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_MPEGLAYER3 &&
                 adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM)
        {
            /* As observed, the input stream is using blocks as its unit of measure */
            DWORD blockSize = 1152 * adsi->pwfxSrc->nAvgBytesPerSec / adsi->pwfxSrc->nSamplesPerSec;
            DWORD nblocks = adss->cbSrcLength / blockSize;

            /* The block is then rounded up to ensure the entire stream is contained */
            if (adss->cbSrcLength % blockSize)
                nblocks++;

            /* As testing demonstrates, the MP3 sample per second value is
             * being used while converting the stream size for the PCM format
             */
            adss->cbDstLength = nblocks * 1152 * adsi->pwfxDst->nAvgBytesPerSec / adsi->pwfxSrc->nSamplesPerSec;
        }
        else
        {
            return MMSYSERR_NOTSUPPORTED;
        }
        TRACE("output dst len=%u\n", adss->cbDstLength);
        break;
    default:
        WARN("Unsupported query %08x\n", adss->fdwSize);
        return MMSYSERR_NOTSUPPORTED;
    }
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           MPEG3_StreamConvert
 *
 */
static LRESULT MPEG3_StreamConvert(PACMDRVSTREAMINSTANCE adsi, PACMDRVSTREAMHEADER adsh)
{
    AcmMpeg3Data*	aad = (AcmMpeg3Data*)adsi->dwDriver;
    DWORD		nsrc = adsh->cbSrcLength;
    DWORD		ndst = adsh->cbDstLength;

    if (adsh->fdwConvert &
	~(ACM_STREAMCONVERTF_BLOCKALIGN|
	  ACM_STREAMCONVERTF_END|
	  ACM_STREAMCONVERTF_START))
    {
	FIXME("Unsupported fdwConvert (%08x), ignoring it\n", adsh->fdwConvert);
    }
    /* ACM_STREAMCONVERTF_BLOCKALIGN
     *	currently all conversions are block aligned, so do nothing for this flag
     * ACM_STREAMCONVERTF_END
     *	no pending data, so do nothing for this flag
     */
    if ((adsh->fdwConvert & ACM_STREAMCONVERTF_START))
    {
	MPEG3_Reset(adsi, aad);
    }

    aad->convert(adsi, adsh->pbSrc, &nsrc, adsh->pbDst, &ndst);
    adsh->cbSrcLengthUsed = nsrc;
    adsh->cbDstLengthUsed = ndst;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			MPEG3_DriverProc			[exported]
 */
LRESULT CALLBACK	MPEG3_DriverProc(DWORD_PTR dwDevID, HDRVR hDriv, UINT wMsg,
					 LPARAM dwParam1, LPARAM dwParam2)
{
    TRACE("(" DWORD_PTR_FMT " %p %04x " DWORD_PTR_FMT " " DWORD_PTR_FMT ");\n",
	  dwDevID, hDriv, wMsg, dwParam1, dwParam2);

    SetLastError(ERROR_SUCCESS);

    switch (wMsg)
    {
    case DRV_LOAD:		return 1;
    case DRV_FREE:		return 1;
    case DRV_OPEN:		return MPEG3_drvOpen((LPSTR)dwParam1);
    case DRV_CLOSE:		return MPEG3_drvClose(dwDevID);
    case DRV_ENABLE:		return 1;
    case DRV_DISABLE:		return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "MPEG3 filter !", "Wine Driver", MB_OK); return 1;
    case DRV_INSTALL:		return DRVCNF_RESTART;
    case DRV_REMOVE:		return DRVCNF_RESTART;

    case ACMDM_DRIVER_NOTIFY:
	/* no caching from other ACM drivers is done so far */
	return MMSYSERR_NOERROR;

    case ACMDM_DRIVER_DETAILS:
	return MPEG3_DriverDetails((PACMDRIVERDETAILSW)dwParam1);

    case ACMDM_FORMATTAG_DETAILS:
	return MPEG3_FormatTagDetails((PACMFORMATTAGDETAILSW)dwParam1, dwParam2);

    case ACMDM_FORMAT_DETAILS:
	return MPEG3_FormatDetails((PACMFORMATDETAILSW)dwParam1, dwParam2);

    case ACMDM_FORMAT_SUGGEST:
	return MPEG3_FormatSuggest((PACMDRVFORMATSUGGEST)dwParam1);

    case ACMDM_STREAM_OPEN:
	return MPEG3_StreamOpen((PACMDRVSTREAMINSTANCE)dwParam1);

    case ACMDM_STREAM_CLOSE:
	return MPEG3_StreamClose((PACMDRVSTREAMINSTANCE)dwParam1);

    case ACMDM_STREAM_SIZE:
	return MPEG3_StreamSize((PACMDRVSTREAMINSTANCE)dwParam1, (PACMDRVSTREAMSIZE)dwParam2);

    case ACMDM_STREAM_CONVERT:
	return MPEG3_StreamConvert((PACMDRVSTREAMINSTANCE)dwParam1, (PACMDRVSTREAMHEADER)dwParam2);

    case ACMDM_HARDWARE_WAVE_CAPS_INPUT:
    case ACMDM_HARDWARE_WAVE_CAPS_OUTPUT:
	/* this converter is not a hardware driver */
    case ACMDM_FILTERTAG_DETAILS:
    case ACMDM_FILTER_DETAILS:
	/* this converter is not a filter */
    case ACMDM_STREAM_RESET:
	/* only needed for asynchronous driver... we aren't, so just say it */
	return MMSYSERR_NOTSUPPORTED;
    case ACMDM_STREAM_PREPARE:
    case ACMDM_STREAM_UNPREPARE:
	/* nothing special to do here... so don't do anything */
	return MMSYSERR_NOERROR;

    default:
	return DefDriverProc(dwDevID, hDriv, wMsg, dwParam1, dwParam2);
    }
    return 0;
}
