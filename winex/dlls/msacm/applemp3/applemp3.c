/*
 * Apple CoreAudio based MP3 MSACM driver
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 *      Portions Copyright (C) 2002 Eric Pouech, used under the ReWind license
 *
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
#include "wine/debug.h"

DEFAULT_DEBUG_CHANNEL(applemp3);

#ifdef __APPLE__
#include "mp3_audioconverter.h"


CRITICAL_SECTION        CoreAudio_crit;

static void create_cs(void)
{
    CRITICAL_SECTION_DEFINE( &CoreAudio_crit );
}

static void destroy_cs(void)
{
    DeleteCriticalSection( &CoreAudio_crit );
}

void LockCoreAudio() {
     EnterCriticalSection(&CoreAudio_crit);
}
void UnlockCoreAudio() {
     LeaveCriticalSection(&CoreAudio_crit);
}

typedef struct tagAcmAppleMP3Data
{
    void (*convert)(PACMDRVSTREAMINSTANCE adsi, const unsigned char*, LPDWORD, unsigned char*, LPDWORD);
    void *pIntAppleMP3Data;
} AcmAppleMP3Data;

/* table to list all supported formats... those are the basic ones. this
 * also helps given a unique index to each of the supported formats
 */
typedef struct
{
    int		nChannels;
    int		nBits;
    int		rate;
} Format;

static Format PCM_Formats[] =
{
    {1,  8,  8000}, {2,  8,  8000}, {1, 16,  8000}, {2, 16,  8000},
    {1,  8, 11025}, {2,  8, 11025}, {1, 16, 11025}, {2, 16, 11025},
    {1,  8, 22050}, {2,  8, 22050}, {1, 16, 22050}, {2, 16, 22050},
    {1,  8, 44100}, {2,  8, 44100}, {1, 16, 44100}, {2, 16, 44100},
};

static Format APPLEMP3_Formats[] =
{
    {1,  0,  8000}, {2,	0,  8000},  {1,  0, 11025}, {2,	 0, 11025},
    {1,  0, 22050}, {2,	0, 22050},  {1,  0, 44100}, {2,	 0, 44100},
};

#define NUM_PCM_FORMATS        (sizeof(PCM_Formats) / sizeof(PCM_Formats[0]))
#define NUM_APPLEMP3_FORMATS   (sizeof(APPLEMP3_Formats) / sizeof(APPLEMP3_Formats[0]))


/***********************************************************************
 *           APPLEMP3_AudioConverterDecodeData
 */
static void APPLEMP3_AudioConverterDecodeDataWrapper(PACMDRVSTREAMINSTANCE adsi,
                      const unsigned char* src, LPDWORD nsrc,
                      unsigned char* dst, LPDWORD ndst)
{
    AcmAppleMP3Data*    amd = (AcmAppleMP3Data*)adsi->dwDriver;
    APPLEMP3_AudioConverterDecodeData(amd->pIntAppleMP3Data, *nsrc, (unsigned char*)src, ndst, dst);
}




/**********************************************************************
***********************************************************************
***********************************************************************
***********************************************************************/

/***********************************************************************
 *           APPLEMP3_drvOpen
 */
static DWORD APPLEMP3_drvOpen(LPCSTR str)
{
	create_cs();
    return 1;
}

/***********************************************************************
 *           APPLEMP3_drvClose
 */
static DWORD APPLEMP3_drvClose(DWORD dwDevID)
{
	destroy_cs();
    return 1;
}




/***********************************************************************
 *           APPLEMP3_GetFormatIndex
 */
static DWORD APPLEMP3_GetFormatIndex(LPWAVEFORMATEX wfx)
{
    int      i, hi;
    Format*  fmts;

    switch (wfx->wFormatTag)
    {
        case WAVE_FORMAT_PCM:
            hi = NUM_PCM_FORMATS;
            fmts = PCM_Formats;
            break;
        case WAVE_FORMAT_MPEGLAYER3:
            hi = NUM_APPLEMP3_FORMATS;
            fmts = APPLEMP3_Formats;
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

/***********************************************************************
 *           R16
 *
 * Read a 16 bit sample (correctly handles endianess)
 */
static inline short  R16(const unsigned char* src)
{
    return (short)((unsigned short)src[0] | ((unsigned short)src[1] << 8));
}

/***********************************************************************
 *           W16
 *
 * Write a 16 bit sample (correctly handles endianess)
 */
static inline void  W16(unsigned char* dst, short s)
{
#ifndef __PPC__
    dst[0] = LOBYTE(s);
    dst[1] = HIBYTE(s);
#else
    /* Not quite sure why, but for some reson we need to 
       byteswap the output data - this shouldn't be needed
       in theory, but we must have something else wrong in the code */
    dst[1] = LOBYTE(s);
    dst[0] = HIBYTE(s);
#endif
}


/***********************************************************************
 *           APPLEMP3_DriverDetails
 *
 */
static LRESULT APPLEMP3_DriverDetails(PACMDRIVERDETAILSW add)
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
    MultiByteToWideChar( CP_ACP, 0, "WineX-Apple-MPEG3", -1,
                         add->szShortName, sizeof(add->szShortName)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, "WineX Apple MPEG3 decoder", -1,
                         add->szLongName, sizeof(add->szLongName)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, "Brought to you by NVIDIA CORPORATION...", -1,
                         add->szCopyright, sizeof(add->szCopyright)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, "Refer to NVIDIA license information that came with your software product", -1,
                         add->szLicensing, sizeof(add->szLicensing)/sizeof(WCHAR) );
    add->szFeatures[0] = 0;

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           APPLEMP3_FormatTagDetails
 *
 */
static LRESULT APPLEMP3_FormatTagDetails(PACMFORMATTAGDETAILSW aftd, DWORD dwQuery)
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
                case WAVE_FORMAT_PCM:        aftd->dwFormatTagIndex = 0; break;
                case WAVE_FORMAT_MPEGLAYER3:     aftd->dwFormatTagIndex = 1; break;
                default:            return ACMERR_NOTPOSSIBLE;
            }
            break;
        default:
            WARN("Unsupported query %08lx\n", dwQuery);
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
            aftd->cStandardFormats = NUM_APPLEMP3_FORMATS;
            lstrcpyW(aftd->szFormatTag, szMpeg3);
        break;
    }
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           APPLEMP3_FormatDetails
 *
 */
static LRESULT APPLEMP3_FormatDetails(PACMFORMATDETAILSW afd, DWORD dwQuery)
{
    switch (dwQuery)
    {
        case ACM_FORMATDETAILSF_FORMAT:
    	if (APPLEMP3_GetFormatIndex(afd->pwfx) == 0xFFFFFFFF) return ACMERR_NOTPOSSIBLE;
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
	            if (afd->dwFormatIndex >= NUM_APPLEMP3_FORMATS) return ACMERR_NOTPOSSIBLE;
	            afd->pwfx->nChannels = APPLEMP3_Formats[afd->dwFormatIndex].nChannels;
	            afd->pwfx->nSamplesPerSec = APPLEMP3_Formats[afd->dwFormatIndex].rate;
	            afd->pwfx->wBitsPerSample = APPLEMP3_Formats[afd->dwFormatIndex].nBits;
	            afd->pwfx->nBlockAlign = 1024;
	            
                    {
                        MPEGLAYER3WAVEFORMAT*   mp3wfx = (MPEGLAYER3WAVEFORMAT*)afd->pwfx;
                        unsigned int bit_rate = 192000;
                        
                        afd->pwfx->nAvgBytesPerSec = afd->pwfx->nChannels * bit_rate / 16 ;
                        if (afd->cbwfx >= sizeof(WAVEFORMATEX))
                            afd->pwfx->cbSize = sizeof(MPEGLAYER3WAVEFORMAT) - sizeof(WAVEFORMATEX);
                        if (afd->cbwfx >= sizeof(MPEGLAYER3WAVEFORMAT))
                        {
                            mp3wfx->wID = MPEGLAYER3_ID_MPEG;
                            mp3wfx->fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
                            mp3wfx->nBlockSize = (bit_rate * 144) / afd->pwfx->nSamplesPerSec;
                            mp3wfx->nFramesPerBlock = 1;
                            mp3wfx->nCodecDelay = 0x0571;
                        }
                    }
	        break;

	        default:
	            WARN("Unsupported tag %08lx\n", afd->dwFormatTag);
	            return MMSYSERR_INVALPARAM;
	    }
	break;
	
        default:
	    WARN("Unsupported query %08lx\n", dwQuery);
	    return MMSYSERR_NOTSUPPORTED;
    }
    
    afd->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
    afd->szFormat[0] = 0; /* let MSACM format this for us... */

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           APPLEMP3_FormatSuggest
 *
 */
static	LRESULT	APPLEMP3_FormatSuggest(PACMDRVFORMATSUGGEST adfs)
{
    /* some tests ... */
    if (adfs->cbwfxSrc < sizeof(PCMWAVEFORMAT) ||
	adfs->cbwfxDst < sizeof(PCMWAVEFORMAT) ||
	APPLEMP3_GetFormatIndex(adfs->pwfxSrc) == 0xFFFFFFFF) return ACMERR_NOTPOSSIBLE;
    /* FIXME: should do those tests against the real size (according to format tag */

    TRACE("flags: 0x%lx\n", adfs->fdwSuggest);
    TRACE("source: channels=%d, bits=%d\n", adfs->pwfxSrc->nChannels, adfs->pwfxSrc->wBitsPerSample);
    TRACE("dest: channels=%d, bits=%d\n", adfs->pwfxDst->nChannels, adfs->pwfxSrc->wBitsPerSample);

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
    if (APPLEMP3_GetFormatIndex(adfs->pwfxDst) == 0xFFFFFFFF) return ACMERR_NOTPOSSIBLE;

    /* recompute other values */
    switch (adfs->pwfxDst->wFormatTag)
    {
        case WAVE_FORMAT_PCM:
            adfs->pwfxDst->nBlockAlign = (adfs->pwfxDst->nChannels * adfs->pwfxDst->wBitsPerSample) / 8;
            adfs->pwfxDst->nAvgBytesPerSec = adfs->pwfxDst->nSamplesPerSec * adfs->pwfxDst->nBlockAlign;
        break;
        
        case WAVE_FORMAT_MPEGLAYER3:
            adfs->pwfxDst->nBlockAlign = 1;
	    {
	        MPEGLAYER3WAVEFORMAT*   mp3wfx = (MPEGLAYER3WAVEFORMAT*)adfs->pwfxDst;
	        unsigned int bit_rate = 192000;
	        
	        adfs->pwfxDst->nAvgBytesPerSec = adfs->pwfxDst->nChannels * bit_rate / 16;
	        if (adfs->cbwfxDst >= sizeof(WAVEFORMATEX))
	            adfs->pwfxDst->cbSize = sizeof(MPEGLAYER3WAVEFORMAT) - sizeof(WAVEFORMATEX);
	        if (adfs->cbwfxDst >= sizeof(MPEGLAYER3WAVEFORMAT))
	        {
	            mp3wfx->wID = MPEGLAYER3_ID_MPEG;
	            mp3wfx->fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
	            mp3wfx->nBlockSize = (bit_rate * 144) / adfs->pwfxDst->nSamplesPerSec;
	            mp3wfx->nFramesPerBlock = 1;
	            mp3wfx->nCodecDelay = 0x0571;
	        }
	    }
        
        break;

        default:
            TRACE("\n");
        break;
    }

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           APPLEMP3_Reset
 *
 */
static void APPLEMP3_Reset(PACMDRVSTREAMINSTANCE adsi, AcmAppleMP3Data* aad)
{
    APPLEMP3_AudioConverterReset(&(aad->pIntAppleMP3Data));	
}

/***********************************************************************
 *           APPLEMP3_StreamOpen
 *
 */
static LRESULT APPLEMP3_StreamOpen(PACMDRVSTREAMINSTANCE adsi)
{
    AcmAppleMP3Data*	aad;

    assert(!(adsi->fdwOpen & ACM_STREAMOPENF_ASYNC));

    if (APPLEMP3_GetFormatIndex(adsi->pwfxSrc) == 0xFFFFFFFF ||
	APPLEMP3_GetFormatIndex(adsi->pwfxDst) == 0xFFFFFFFF)
	return ACMERR_NOTPOSSIBLE;

    aad = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(AcmAppleMP3Data));
    if (aad == 0) return MMSYSERR_NOMEM;

    adsi->dwDriver = (DWORD)aad;

    if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM &&
	adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM)
    {
	goto theEnd;
    }
    else if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_MPEGLAYER3 &&
             adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM)
    {
	/* resampling or mono <=> stereo not available
         * MPEG3 algo only define 16 bit per sample output
         */
	if (adsi->pwfxSrc->nSamplesPerSec != adsi->pwfxDst->nSamplesPerSec ||
	    adsi->pwfxSrc->nChannels != adsi->pwfxDst->nChannels ||
            adsi->pwfxDst->wBitsPerSample != 16)
	    goto theEnd;

	if (!APPLEMP3_AudioConverterInit(&aad->pIntAppleMP3Data, adsi->pwfxSrc->nSamplesPerSec, adsi->pwfxSrc->nChannels))
	    goto theEnd;
	    
        aad->convert = APPLEMP3_AudioConverterDecodeDataWrapper;

    }
    /* no encoding yet
    else if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM &&
             adsi->pwfxDst->wFormatTag == WAVE_FORMAT_MPEGLAYER3)
    */
    else goto theEnd;
    APPLEMP3_Reset(adsi, aad);

    return MMSYSERR_NOERROR;

 theEnd:
    HeapFree(GetProcessHeap(), 0, aad);
    adsi->dwDriver = 0L;
    return MMSYSERR_NOTSUPPORTED;
}

/***********************************************************************
 *           APPLEMP3_StreamClose
 *
 */
static	LRESULT	APPLEMP3_StreamClose(PACMDRVSTREAMINSTANCE adsi)
{
    AcmAppleMP3Data*	aad = (AcmAppleMP3Data*)(adsi->dwDriver);
    
    APPLEMP3_AudioConverterUnInit(&(aad->pIntAppleMP3Data));
    HeapFree(GetProcessHeap(), 0, (void*)adsi->dwDriver);
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           APPLEMP3_round
 *
 */
static	inline DWORD	APPLEMP3_round(DWORD a, DWORD b, DWORD c)
{
    assert(a && b && c);
    /* to be sure, always return an entire number of c... */
    return ((double)a * (double)b + (double)c - 1) / (double)c;
}

/***********************************************************************
 *           APPLEMP3_StreamSize
 *
 */
static	LRESULT APPLEMP3_StreamSize(PACMDRVSTREAMINSTANCE adsi, PACMDRVSTREAMSIZE adss)
{
    switch (adss->fdwSize)
    {
    case ACM_STREAMSIZEF_DESTINATION:
	TRACE("input src bytes/sec=%ld\n", adsi->pwfxSrc->nAvgBytesPerSec);
	TRACE("      dst len=%ld, bytes/sec=%ld\n", adss->cbDstLength, adsi->pwfxDst->nAvgBytesPerSec);
	/* cbDstLength => cbSrcLength */
	if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM &&
	    adsi->pwfxDst->wFormatTag == WAVE_FORMAT_MPEGLAYER3)
        {
	    /* don't take block overhead into account, doesn't matter too much */
    	    adss->cbSrcLength = adss->cbDstLength * adsi->pwfxSrc->nAvgBytesPerSec / adsi->pwfxDst->nAvgBytesPerSec;
	}
        else if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_MPEGLAYER3 &&
                 adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM)
        {
	    TRACE("misses the block header overhead\n");
	    DWORD dstlen = adss->cbDstLength;
	    dstlen -= dstlen%4608; /* round down to output block size */
	    adss->cbSrcLength = dstlen * adsi->pwfxSrc->nAvgBytesPerSec / adsi->pwfxDst->nAvgBytesPerSec;	    
	}
        else
        {
	    return MMSYSERR_NOTSUPPORTED;
	}
	TRACE("output src len=%ld\n", adss->cbSrcLength);
	break;
    case ACM_STREAMSIZEF_SOURCE:
	TRACE("input src len=%ld, bytes/sec=%ld\n", adss->cbSrcLength, adsi->pwfxSrc->nAvgBytesPerSec);
	TRACE("      dst bytes/sec=%ld\n", adsi->pwfxDst->nAvgBytesPerSec);
	/* cbSrcLength => cbDstLength */
	if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM &&
	    adsi->pwfxDst->wFormatTag == WAVE_FORMAT_MPEGLAYER3)
        {
	    TRACE("misses the block header overhead\n");
	    adss->cbDstLength = adss->cbSrcLength * adsi->pwfxDst->nAvgBytesPerSec / adsi->pwfxSrc->nAvgBytesPerSec;
	}
        else if (adsi->pwfxSrc->wFormatTag == WAVE_FORMAT_MPEGLAYER3 &&
                 adsi->pwfxDst->wFormatTag == WAVE_FORMAT_PCM)
        {
	    /* don't take block overhead into account, doesn't matter too much */
            adss->cbDstLength = adss->cbSrcLength * adsi->pwfxDst->nAvgBytesPerSec / adsi->pwfxSrc->nAvgBytesPerSec;
	}
        else
        {
	    return MMSYSERR_NOTSUPPORTED;
	}
	TRACE("output dst len=%ld\n", adss->cbDstLength);
	break;
    default:
	WARN("Unsupported query %08lx\n", adss->fdwSize);
	return MMSYSERR_NOTSUPPORTED;
    }
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           APPLEMP3_StreamConvert
 *
 */
static LRESULT APPLEMP3_StreamConvert(PACMDRVSTREAMINSTANCE adsi, PACMDRVSTREAMHEADER adsh)
{
    AcmAppleMP3Data*	aad = (AcmAppleMP3Data*)adsi->dwDriver;
    DWORD		nsrc = adsh->cbSrcLength;
    DWORD		ndst = adsh->cbDstLength;

    if (adsh->fdwConvert &
	~(ACM_STREAMCONVERTF_BLOCKALIGN|
	  ACM_STREAMCONVERTF_END|
	  ACM_STREAMCONVERTF_START))
    {
	FIXME("Unsupported fdwConvert (%08lx), ignoring it\n", adsh->fdwConvert);
    }
    /* ACM_STREAMCONVERTF_BLOCKALIGN
     *	currently all conversions are block aligned, so do nothing for this flag
     * ACM_STREAMCONVERTF_END
     *	no pending data, so do nothing for this flag
     */
    if ((adsh->fdwConvert & ACM_STREAMCONVERTF_START))
    {
	APPLEMP3_Reset(adsi, aad);
    }

    aad->convert(adsi, adsh->pbSrc, &nsrc, adsh->pbDst, &ndst);
    adsh->cbSrcLengthUsed = nsrc;
    adsh->cbDstLengthUsed = ndst;
    
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			APPLEMP3_DriverProc			[exported]
 */
LRESULT CALLBACK	APPLEMP3_DriverProc(DWORD dwDevID, HDRVR hDriv, UINT wMsg,
					 LPARAM dwParam1, LPARAM dwParam2)
{
    TRACE("(%08lx %08lx %04x %08lx %08lx);\n",
	  dwDevID, (DWORD)hDriv, wMsg, dwParam1, dwParam2);

    switch (wMsg)
    {
    case DRV_LOAD:				return 1;
    case DRV_FREE:				return 1;
    case DRV_OPEN:				return APPLEMP3_drvOpen((LPSTR)dwParam1);
    case DRV_CLOSE:				return APPLEMP3_drvClose(dwDevID);
    case DRV_ENABLE:			return 1;
    case DRV_DISABLE:			return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:			return 1;
    case DRV_INSTALL:			return DRVCNF_RESTART;
    case DRV_REMOVE:			return DRVCNF_RESTART;

    case ACMDM_DRIVER_NOTIFY:
	/* no caching from other ACM drivers is done so far */
	return MMSYSERR_NOERROR;

    case ACMDM_DRIVER_DETAILS:
	return APPLEMP3_DriverDetails((PACMDRIVERDETAILSW)dwParam1);

    case ACMDM_FORMATTAG_DETAILS:
	return APPLEMP3_FormatTagDetails((PACMFORMATTAGDETAILSW)dwParam1, dwParam2);

    case ACMDM_FORMAT_DETAILS:
	return APPLEMP3_FormatDetails((PACMFORMATDETAILSW)dwParam1, dwParam2);

    case ACMDM_FORMAT_SUGGEST:
	return APPLEMP3_FormatSuggest((PACMDRVFORMATSUGGEST)dwParam1);

    case ACMDM_STREAM_OPEN:
	return APPLEMP3_StreamOpen((PACMDRVSTREAMINSTANCE)dwParam1);

    case ACMDM_STREAM_CLOSE:
	return APPLEMP3_StreamClose((PACMDRVSTREAMINSTANCE)dwParam1);

    case ACMDM_STREAM_SIZE:
	return APPLEMP3_StreamSize((PACMDRVSTREAMINSTANCE)dwParam1, (PACMDRVSTREAMSIZE)dwParam2);

    case ACMDM_STREAM_CONVERT:
	return APPLEMP3_StreamConvert((PACMDRVSTREAMINSTANCE)dwParam1, (PACMDRVSTREAMHEADER)dwParam2);

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

#else /* __APPLE__ */

/**************************************************************************
 * 			APPLEMP3_DriverProc			[exported]
 */
LRESULT CALLBACK	APPLEMP3_DriverProc(DWORD dwDevID, HDRVR hDriv, UINT wMsg,
					 LPARAM dwParam1, LPARAM dwParam2)
{
    FIXME("Not supported on non MacOS X platforms!\n");
    return 0;
}

#endif /* __APPLE__ */



