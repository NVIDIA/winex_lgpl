/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * Wine Wave mapper driver
 *
 * Copyright 	1999,2001 Eric Pouech
 */

/* TODOs
 *	+ better protection against evilish dwUser parameters
 *	+ use asynchronous ACM conversion
 *	+ don't use callback functions when none is required in open
 *	+ the buffer sizes may not be accurate, so there may be some
 *	  remaining bytes in src and dst buffers after ACM conversions...
 *		those should be taken care of...
 */

#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "mmddk.h"
#include "msacm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msacm);

typedef	struct tagWAVEMAPDATA {
    struct tagWAVEMAPDATA*	self;
    HWAVE	hOuterWave;
    HWAVE	hInnerWave;
    HACMSTREAM	hAcmStream;
    /* needed data to filter callbacks. Only needed when hAcmStream is not 0 */
    DWORD	dwCallback;
    DWORD	dwClientInstance;
    DWORD	dwFlags;
} WAVEMAPDATA;

static	BOOL	WAVEMAP_IsData(WAVEMAPDATA* wm)
{
    return (!IsBadReadPtr(wm, sizeof(WAVEMAPDATA)) && wm->self == wm);
}

/*======================================================================*
 *                  WAVE OUT part                                       *
 *======================================================================*/

static void	CALLBACK wodCallback(HWAVE hWave, UINT uMsg, DWORD dwInstance,
				     DWORD dwParam1, DWORD dwParam2)
{
    WAVEMAPDATA*	wom = (WAVEMAPDATA*)dwInstance;

    TRACE("(0x%x %u %ld %lx %lx);\n", hWave, uMsg, dwInstance, dwParam1, dwParam2);

    if (!WAVEMAP_IsData(wom)) {
	ERR("Bad data\n");
	return;
    }

    if (hWave != wom->hInnerWave && uMsg != WOM_OPEN)
	ERR("Shouldn't happen (%08x %08x)\n", hWave, wom->hInnerWave);

    switch (uMsg) {
    case WOM_OPEN:
    case WOM_CLOSE:
	/* dwParam1 & dwParam2 are supposed to be 0, nothing to do */
	break;
    case WOM_DONE:
	if (wom->hAcmStream) {
	    LPWAVEHDR		lpWaveHdrDst = (LPWAVEHDR)dwParam1;
	    PACMSTREAMHEADER	ash = (PACMSTREAMHEADER)((LPSTR)lpWaveHdrDst - sizeof(ACMSTREAMHEADER));
	    LPWAVEHDR		lpWaveHdrSrc = (LPWAVEHDR)ash->dwUser;

	    lpWaveHdrSrc->dwFlags &= ~WHDR_INQUEUE;
	    lpWaveHdrSrc->dwFlags |= WHDR_DONE;
	    dwParam1 = (DWORD)lpWaveHdrSrc;
	}
	break;
    default:
	ERR("Unknown msg %u\n", uMsg);
    }

    DriverCallback(wom->dwCallback, HIWORD(wom->dwFlags), (HDRVR)wom->hOuterWave,
		   uMsg, wom->dwClientInstance, dwParam1, dwParam2);
}

static	DWORD	wodOpenHelper(WAVEMAPDATA* wom, UINT idx,
			      LPWAVEOPENDESC lpDesc, LPWAVEFORMATEX lpwfx,
			      DWORD dwFlags)
{
    DWORD	ret;

    /* destination is always PCM, so the formulas below apply */
    lpwfx->nBlockAlign = (lpwfx->nChannels * lpwfx->wBitsPerSample) / 8;
    lpwfx->nAvgBytesPerSec = lpwfx->nSamplesPerSec * lpwfx->nBlockAlign;
    if (dwFlags & WAVE_FORMAT_QUERY) {
	ret = acmStreamOpen(NULL, 0, lpDesc->lpFormat, lpwfx, NULL, 0L, 0L, ACM_STREAMOPENF_QUERY);
    } else {
	ret = acmStreamOpen(&wom->hAcmStream, 0, lpDesc->lpFormat, lpwfx, NULL, 0L, 0L, 0L);
    }
    if (ret == MMSYSERR_NOERROR) {
	ret = waveOutOpen(&wom->hInnerWave, idx, lpwfx, (DWORD)wodCallback,
			  (DWORD)wom, (dwFlags & ~CALLBACK_TYPEMASK) | CALLBACK_FUNCTION);
	if (ret != MMSYSERR_NOERROR && !(dwFlags & WAVE_FORMAT_QUERY)) {
	    acmStreamClose(wom->hAcmStream, 0);
	    wom->hAcmStream = 0;
	}
    }
    return ret;
}

static	DWORD	wodOpen(LPDWORD lpdwUser, LPWAVEOPENDESC lpDesc, DWORD dwFlags)
{
    UINT 		ndlo, ndhi;
    UINT		i;
    WAVEMAPDATA*	wom = HeapAlloc(GetProcessHeap(), 0, sizeof(WAVEMAPDATA));

    TRACE("(%p %p %08lx\n", lpdwUser, lpDesc, dwFlags);

    if (!wom)
	return MMSYSERR_NOMEM;

    ndhi = waveOutGetNumDevs();
    if (dwFlags & WAVE_MAPPED) {
	if (lpDesc->uMappedDeviceID >= ndhi) return MMSYSERR_INVALPARAM;
	ndlo = lpDesc->uMappedDeviceID;
	ndhi = ndlo + 1;
	dwFlags &= ~WAVE_MAPPED;
    } else {
	ndlo = 0;
    }
    wom->self = wom;
    wom->dwCallback = lpDesc->dwCallback;
    wom->dwFlags = dwFlags;
    wom->dwClientInstance = lpDesc->dwInstance;
    wom->hOuterWave = lpDesc->hWave;

    for (i = ndlo; i < ndhi; i++) {
	/* if no ACM stuff is involved, no need to handle callbacks at this
	 * level, this will be done transparently
	 */
	if (waveOutOpen(&wom->hInnerWave, i, lpDesc->lpFormat, (DWORD)wodCallback,
			(DWORD)wom, (dwFlags & ~CALLBACK_TYPEMASK) | CALLBACK_FUNCTION | WAVE_FORMAT_DIRECT) == MMSYSERR_NOERROR) {
	    wom->hAcmStream = 0;
	    goto found;
	}
    }

    if ((dwFlags & WAVE_FORMAT_DIRECT) == 0) {
        WAVEFORMATEX	wfx;

        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.cbSize = 0; /* normally, this field is not used for PCM format, just in case */
        /* try some ACM stuff */

#define	TRY(sps,bps)    wfx.nSamplesPerSec = (sps); wfx.wBitsPerSample = (bps); \
                        if (wodOpenHelper(wom, i, lpDesc, &wfx, dwFlags | WAVE_FORMAT_DIRECT) == MMSYSERR_NOERROR) goto found;

        /* try format conversion without resampling first */
        for (i = ndlo; i < ndhi; i++) {
            wfx.nChannels = lpDesc->lpFormat->nChannels;
            TRY(lpDesc->lpFormat->nSamplesPerSec, 16);
            TRY(lpDesc->lpFormat->nSamplesPerSec, 8);
            wfx.nChannels ^= 3;
            TRY(lpDesc->lpFormat->nSamplesPerSec, 16);
            TRY(lpDesc->lpFormat->nSamplesPerSec, 8);
        }

        for (i = ndlo; i < ndhi; i++) {
            /* first try with same stereo/mono option as source */
            wfx.nChannels = lpDesc->lpFormat->nChannels;
            TRY(96000, 16);
            TRY(48000, 16);
            TRY(44100, 16);
            TRY(22050, 16);
            TRY(11025, 16);

            /* 2^3 => 1, 1^3 => 2, so if stereo, try mono (and the other way around) */
            wfx.nChannels ^= 3;
            TRY(96000, 16);
            TRY(48000, 16);
            TRY(44100, 16);
            TRY(22050, 16);
            TRY(11025, 16);

            /* first try with same stereo/mono option as source */
            wfx.nChannels = lpDesc->lpFormat->nChannels;
            TRY(96000, 8);
            TRY(48000, 8);
            TRY(44100, 8);
            TRY(22050, 8);
            TRY(11025, 8);

            /* 2^3 => 1, 1^3 => 2, so if stereo, try mono (and the other way around) */
            wfx.nChannels ^= 3;
            TRY(96000, 8);
            TRY(48000, 8);
            TRY(44100, 8);
            TRY(22050, 8);
            TRY(11025, 8);
        }
#undef TRY
    }

    HeapFree(GetProcessHeap(), 0, wom);
    return MMSYSERR_ALLOCATED;
found:
    if (dwFlags & WAVE_FORMAT_QUERY) {
	*lpdwUser = 0L;
	HeapFree(GetProcessHeap(), 0, wom);
    } else {
	*lpdwUser = (DWORD)wom;
    }
    return MMSYSERR_NOERROR;
}

static	DWORD	wodClose(WAVEMAPDATA* wom)
{
    DWORD ret = waveOutClose(wom->hInnerWave);

    if (ret == MMSYSERR_NOERROR) {
	if (wom->hAcmStream) {
	    ret = acmStreamClose(wom->hAcmStream, 0);
	}
	if (ret == MMSYSERR_NOERROR) {
	    HeapFree(GetProcessHeap(), 0, wom);
	}
    }
    return ret;
}

static	DWORD	wodWrite(WAVEMAPDATA* wom, LPWAVEHDR lpWaveHdrSrc, DWORD dwParam2)
{
    PACMSTREAMHEADER	ash;
    LPWAVEHDR		lpWaveHdrDst;

    if (!wom->hAcmStream) {
	return waveOutWrite(wom->hInnerWave, lpWaveHdrSrc, dwParam2);
    }

    lpWaveHdrSrc->dwFlags |= WHDR_INQUEUE;
    ash = (PACMSTREAMHEADER)lpWaveHdrSrc->reserved;
    /* acmStreamConvert will actually check that the new size is less than initial size */
    ash->cbSrcLength = lpWaveHdrSrc->dwBufferLength;
    if (acmStreamConvert(wom->hAcmStream, ash, 0L) != MMSYSERR_NOERROR)
	return MMSYSERR_ERROR;

    lpWaveHdrDst = (LPWAVEHDR)((LPSTR)ash + sizeof(ACMSTREAMHEADER));
    if (ash->cbSrcLength > ash->cbSrcLengthUsed)
        FIXME("Not all src buffer has been written, expect bogus sound\n");
    else if (ash->cbSrcLength < ash->cbSrcLengthUsed)
        ERR("CoDec has read more data than it is allowed to\n");

    if (ash->cbDstLengthUsed == 0)
    {
        /* something went wrong in decoding */
        FIXME("Got 0 length\n");
        return MMSYSERR_ERROR;
    }
    lpWaveHdrDst->dwBufferLength = ash->cbDstLengthUsed;
    return waveOutWrite(wom->hInnerWave, lpWaveHdrDst, sizeof(*lpWaveHdrDst));
}

static	DWORD	wodPrepare(WAVEMAPDATA* wom, LPWAVEHDR lpWaveHdrSrc, DWORD dwParam2)
{
    PACMSTREAMHEADER	ash;
    DWORD		size;
    DWORD		dwRet;
    LPWAVEHDR		lpWaveHdrDst;

    if (!wom->hAcmStream) {
	return waveOutPrepareHeader(wom->hInnerWave, lpWaveHdrSrc, dwParam2);
    }
    if (acmStreamSize(wom->hAcmStream, lpWaveHdrSrc->dwBufferLength, &size, ACM_STREAMSIZEF_SOURCE) != MMSYSERR_NOERROR)
	return MMSYSERR_ERROR;

    ash = HeapAlloc(GetProcessHeap(), 0, sizeof(ACMSTREAMHEADER) + sizeof(WAVEHDR) + size);
    if (ash == NULL)
	return MMSYSERR_NOMEM;

    ash->cbStruct = sizeof(*ash);
    ash->fdwStatus = 0L;
    ash->dwUser = (DWORD)lpWaveHdrSrc;
    ash->pbSrc = lpWaveHdrSrc->lpData;
    ash->cbSrcLength = lpWaveHdrSrc->dwBufferLength;
    /* ash->cbSrcLengthUsed */
    ash->dwSrcUser = lpWaveHdrSrc->dwUser; /* FIXME ? */
    ash->pbDst = (LPSTR)ash + sizeof(ACMSTREAMHEADER) + sizeof(WAVEHDR);
    ash->cbDstLength = size;
    /* ash->cbDstLengthUsed */
    ash->dwDstUser = 0; /* FIXME ? */
    dwRet = acmStreamPrepareHeader(wom->hAcmStream, ash, 0L);
    if (dwRet != MMSYSERR_NOERROR)
	goto errCleanUp;

    lpWaveHdrDst = (LPWAVEHDR)((LPSTR)ash + sizeof(ACMSTREAMHEADER));
    lpWaveHdrDst->lpData = ash->pbDst;
    lpWaveHdrDst->dwBufferLength = size; /* conversion is not done yet */
    lpWaveHdrDst->dwFlags = 0;
    lpWaveHdrDst->dwLoops = 0;
    dwRet = waveOutPrepareHeader(wom->hInnerWave, lpWaveHdrDst, sizeof(*lpWaveHdrDst));
    if (dwRet != MMSYSERR_NOERROR)
	goto errCleanUp;

    lpWaveHdrSrc->reserved = (DWORD)ash;
    lpWaveHdrSrc->dwFlags = WHDR_PREPARED;
    TRACE("=> (0)\n");
    return MMSYSERR_NOERROR;
errCleanUp:
    TRACE("=> (%ld)\n", dwRet);
    HeapFree(GetProcessHeap(), 0, ash);
    return dwRet;
}

static	DWORD	wodUnprepare(WAVEMAPDATA* wom, LPWAVEHDR lpWaveHdrSrc, DWORD dwParam2)
{
    PACMSTREAMHEADER	ash;
    LPWAVEHDR		lpWaveHdrDst;
    DWORD		dwRet1, dwRet2;

    if (!wom->hAcmStream) {
	return waveOutUnprepareHeader(wom->hInnerWave, lpWaveHdrSrc, dwParam2);
    }
    ash = (PACMSTREAMHEADER)lpWaveHdrSrc->reserved;
    dwRet1 = acmStreamUnprepareHeader(wom->hAcmStream, ash, 0L);

    lpWaveHdrDst = (LPWAVEHDR)((LPSTR)ash + sizeof(ACMSTREAMHEADER));
    dwRet2 = waveOutUnprepareHeader(wom->hInnerWave, lpWaveHdrDst, sizeof(*lpWaveHdrDst));

    HeapFree(GetProcessHeap(), 0, ash);

    lpWaveHdrSrc->dwFlags &= ~WHDR_PREPARED;
    return (dwRet1 == MMSYSERR_NOERROR) ? dwRet2 : dwRet1;
}

static	DWORD	wodGetPosition(WAVEMAPDATA* wom, LPMMTIME lpTime, DWORD dwParam2)
{
    if (wom->hAcmStream)
	FIXME("No position conversion done for PCM => non-PCM, returning PCM position\n");
    return waveOutGetPosition(wom->hInnerWave, lpTime, dwParam2);
}

static	DWORD	wodGetDevCaps(UINT wDevID, WAVEMAPDATA* wom, LPWAVEOUTCAPSA lpWaveCaps, DWORD dwParam2)
{
    /* if opened low driver, forward message */
    if (WAVEMAP_IsData(wom))
	return waveOutGetDevCapsA(wom->hInnerWave, lpWaveCaps, dwParam2);
    /* otherwise, return caps of mapper itself */
    if (wDevID == (UINT)-1 || wDevID == (UINT16)-1) {
	lpWaveCaps->wMid = 0x00FF;
	lpWaveCaps->wPid = 0x0001;
	lpWaveCaps->vDriverVersion = 0x0100;
	strcpy(lpWaveCaps->szPname, "Wine wave out mapper");
	lpWaveCaps->dwFormats =
	    WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4S16 |
	    WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
	    WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16;
	lpWaveCaps->wChannels = 2;
	lpWaveCaps->dwSupport = WAVECAPS_VOLUME | WAVECAPS_LRVOLUME;

	return MMSYSERR_NOERROR;
    }
    ERR("This shouldn't happen\n");
    return MMSYSERR_ERROR;
}

static	DWORD	wodGetVolume(UINT wDevID, WAVEMAPDATA* wom, LPDWORD lpVol)
{
    if (WAVEMAP_IsData(wom))
	return waveOutGetVolume(wom->hInnerWave, lpVol);
    return MMSYSERR_NOERROR;
}

static	DWORD	wodSetVolume(UINT wDevID, WAVEMAPDATA* wom, DWORD vol)
{
    if (WAVEMAP_IsData(wom))
	return waveOutSetVolume(wom->hInnerWave, vol);
    return MMSYSERR_NOERROR;
}

static	DWORD	wodPause(WAVEMAPDATA* wom)
{
    return waveOutPause(wom->hInnerWave);
}

static	DWORD	wodRestart(WAVEMAPDATA* wom)
{
    return waveOutRestart(wom->hInnerWave);
}

static	DWORD	wodReset(WAVEMAPDATA* wom)
{
    return waveOutReset(wom->hInnerWave);
}

static	DWORD	wodBreakLoop(WAVEMAPDATA* wom)
{
    return waveOutBreakLoop(wom->hInnerWave);
}

static  DWORD	wodMapperStatus(WAVEMAPDATA* wom, DWORD flags, LPVOID ptr)
{
    UINT	id;
    DWORD	ret = MMSYSERR_NOTSUPPORTED;

    switch (flags) {
    case WAVEOUT_MAPPER_STATUS_DEVICE:
	ret = waveOutGetID(wom->hInnerWave, &id);
	*(LPDWORD)ptr = id;
	break;
    case WAVEOUT_MAPPER_STATUS_MAPPED:
	FIXME("Unsupported flag=%ld\n", flags);
	*(LPDWORD)ptr = 0; /* FIXME ?? */
	break;
    case WAVEOUT_MAPPER_STATUS_FORMAT:
	FIXME("Unsupported flag=%ld\n", flags);
	/* ptr points to a WAVEFORMATEX struct - before or after streaming ? */
	*(LPDWORD)ptr = 0;
	break;
    default:
	FIXME("Unsupported flag=%ld\n", flags);
	*(LPDWORD)ptr = 0;
	break;
    }
    return ret;
}

/**************************************************************************
 * 				wodMessage (MSACMMAP.@)
 */
DWORD WINAPI WAVEMAP_wodMessage(UINT wDevID, UINT wMsg, DWORD dwUser,
				DWORD dwParam1, DWORD dwParam2)
{
    TRACE("(%u, %04X, %08lX, %08lX, %08lX);\n",
	  wDevID, wMsg, dwUser, dwParam1, dwParam2);

    switch (wMsg) {
    case DRVM_INIT:
    case DRVM_EXIT:
    case DRVM_ENABLE:
    case DRVM_DISABLE:
	/* FIXME: Pretend this is supported */
	return 0;
    case WODM_OPEN:	 	return wodOpen		((LPDWORD)dwUser,      (LPWAVEOPENDESC)dwParam1,dwParam2);
    case WODM_CLOSE:	 	return wodClose		((WAVEMAPDATA*)dwUser);
    case WODM_WRITE:	 	return wodWrite		((WAVEMAPDATA*)dwUser, (LPWAVEHDR)dwParam1,	dwParam2);
    case WODM_PAUSE:	 	return wodPause		((WAVEMAPDATA*)dwUser);
    case WODM_GETPOS:	 	return wodGetPosition	((WAVEMAPDATA*)dwUser, (LPMMTIME)dwParam1, 	dwParam2);
    case WODM_BREAKLOOP: 	return wodBreakLoop	((WAVEMAPDATA*)dwUser);
    case WODM_PREPARE:	 	return wodPrepare	((WAVEMAPDATA*)dwUser, (LPWAVEHDR)dwParam1, 	dwParam2);
    case WODM_UNPREPARE: 	return wodUnprepare	((WAVEMAPDATA*)dwUser, (LPWAVEHDR)dwParam1, 	dwParam2);
    case WODM_GETDEVCAPS:	return wodGetDevCaps	(wDevID, (WAVEMAPDATA*)dwUser, (LPWAVEOUTCAPSA)dwParam1,dwParam2);
    case WODM_GETNUMDEVS:	return 1;
    case WODM_GETPITCH:	 	return MMSYSERR_NOTSUPPORTED;
    case WODM_SETPITCH:	 	return MMSYSERR_NOTSUPPORTED;
    case WODM_GETPLAYBACKRATE:	return MMSYSERR_NOTSUPPORTED;
    case WODM_SETPLAYBACKRATE:	return MMSYSERR_NOTSUPPORTED;
    case WODM_GETVOLUME:	return wodGetVolume	(wDevID, (WAVEMAPDATA*)dwUser, (LPDWORD)dwParam1);
    case WODM_SETVOLUME:	return wodSetVolume	(wDevID, (WAVEMAPDATA*)dwUser, dwParam1);
    case WODM_RESTART:		return wodRestart	((WAVEMAPDATA*)dwUser);
    case WODM_RESET:		return wodReset		((WAVEMAPDATA*)dwUser);
    case WODM_MAPPER_STATUS:	return wodMapperStatus  ((WAVEMAPDATA*)dwUser, dwParam1, (LPVOID)dwParam2);
    default:
	FIXME("unknown message %d!\n", wMsg);
    }
    return MMSYSERR_NOTSUPPORTED;
}

/*======================================================================*
 *                  WAVE IN part                                        *
 *======================================================================*/

static void	CALLBACK widCallback(HWAVE hWave, UINT uMsg, DWORD dwInstance,
				     DWORD dwParam1, DWORD dwParam2)
{
    WAVEMAPDATA*	wim = (WAVEMAPDATA*)dwInstance;

    TRACE("(0x%x %u %ld %lx %lx);\n", hWave, uMsg, dwInstance, dwParam1, dwParam2);

    if (!WAVEMAP_IsData(wim)) {
	ERR("Bad data\n");
	return;
    }

    if (hWave != wim->hInnerWave && uMsg != WIM_OPEN)
	ERR("Shouldn't happen (%08x %08x)\n", hWave, wim->hInnerWave);

    switch (uMsg) {
    case WIM_OPEN:
    case WIM_CLOSE:
	/* dwParam1 & dwParam2 are supposed to be 0, nothing to do */
	break;
    case WIM_DATA:
	if (wim->hAcmStream) {
	    LPWAVEHDR		lpWaveHdrSrc = (LPWAVEHDR)dwParam1;
	    PACMSTREAMHEADER	ash = (PACMSTREAMHEADER)((LPSTR)lpWaveHdrSrc - sizeof(ACMSTREAMHEADER));
	    LPWAVEHDR		lpWaveHdrDst = (LPWAVEHDR)ash->dwUser;

	    /* convert data just gotten from waveIn into requested format */
	    if (acmStreamConvert(wim->hAcmStream, ash, 0L) != MMSYSERR_NOERROR) {
		ERR("ACM conversion failed\n");
		return;
	    } else {
		TRACE("Converted %ld bytes into %ld\n", ash->cbSrcLengthUsed, ash->cbDstLengthUsed);
	    }
	    /* and setup the wavehdr to return accordingly */
	    lpWaveHdrDst->dwFlags &= ~WHDR_INQUEUE;
	    lpWaveHdrDst->dwFlags |= WHDR_DONE;
	    lpWaveHdrDst->dwBytesRecorded = ash->cbDstLengthUsed;
	    dwParam1 = (DWORD)lpWaveHdrDst;
	}
	break;
    default:
	ERR("Unknown msg %u\n", uMsg);
    }

    DriverCallback(wim->dwCallback, HIWORD(wim->dwFlags), (HDRVR)wim->hOuterWave,
		   uMsg, wim->dwClientInstance, dwParam1, dwParam2);
}

static	DWORD	widOpenHelper(WAVEMAPDATA* wim, UINT idx,
			      LPWAVEOPENDESC lpDesc, LPWAVEFORMATEX lpwfx,
			      DWORD dwFlags)
{
    DWORD	ret;

    /* source is always PCM, so the formulas below apply */
    lpwfx->nBlockAlign = (lpwfx->nChannels * lpwfx->wBitsPerSample) / 8;
    lpwfx->nAvgBytesPerSec = lpwfx->nSamplesPerSec * lpwfx->nBlockAlign;
    if (dwFlags & WAVE_FORMAT_QUERY) {
	ret = acmStreamOpen(NULL, 0, lpwfx, lpDesc->lpFormat, NULL, 0L, 0L, ACM_STREAMOPENF_QUERY);
    } else {
	ret = acmStreamOpen(&wim->hAcmStream, 0, lpwfx, lpDesc->lpFormat, NULL, 0L, 0L, 0L);
    }
    if (ret == MMSYSERR_NOERROR) {
	ret = waveInOpen(&wim->hInnerWave, idx, lpwfx, (DWORD)widCallback,
			 (DWORD)wim, (dwFlags & ~CALLBACK_TYPEMASK) | CALLBACK_FUNCTION);
	if (ret != MMSYSERR_NOERROR && !(dwFlags & WAVE_FORMAT_QUERY)) {
	    acmStreamClose(wim->hAcmStream, 0);
	    wim->hAcmStream = 0;
	}
    }
    return ret;
}

static	DWORD	widOpen(LPDWORD lpdwUser, LPWAVEOPENDESC lpDesc, DWORD dwFlags)
{
    UINT 		ndlo, ndhi;
    UINT		i;
    WAVEMAPDATA*	wim = HeapAlloc(GetProcessHeap(), 0, sizeof(WAVEMAPDATA));

    TRACE("(%p %p %08lx)\n", lpdwUser, lpDesc, dwFlags);

    if (!wim)
	return MMSYSERR_NOMEM;

    wim->self = wim;
    wim->dwCallback = lpDesc->dwCallback;
    wim->dwFlags = dwFlags;
    wim->dwClientInstance = lpDesc->dwInstance;
    wim->hOuterWave = lpDesc->hWave;

    ndhi = waveOutGetNumDevs();
    if (dwFlags & WAVE_MAPPED) {
	if (lpDesc->uMappedDeviceID >= ndhi) return MMSYSERR_INVALPARAM;
	ndlo = lpDesc->uMappedDeviceID;
	ndhi = ndlo + 1;
	dwFlags &= ~WAVE_MAPPED;
    } else {
	ndlo = 0;
    }

    for (i = ndlo; i < ndhi; i++) {
	if (waveInOpen(&wim->hInnerWave, i, lpDesc->lpFormat, (DWORD)widCallback,
                       (DWORD)wim, (dwFlags & ~CALLBACK_TYPEMASK) | CALLBACK_FUNCTION | WAVE_FORMAT_DIRECT) == MMSYSERR_NOERROR) {
	    wim->hAcmStream = 0;
	    goto found;
	}
    }

    if ((dwFlags & WAVE_FORMAT_DIRECT) == 0)
    {
        WAVEFORMATEX	wfx;

        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.cbSize = 0; /* normally, this field is not used for PCM format, just in case */
        /* try some ACM stuff */

#define	TRY(sps,bps)    wfx.nSamplesPerSec = (sps); wfx.wBitsPerSample = (bps); \
                        if (widOpenHelper(wim, i, lpDesc, &wfx, dwFlags | WAVE_FORMAT_DIRECT) == MMSYSERR_NOERROR) goto found;

        /* try format conversion without resampling first */
        for (i = ndlo; i < ndhi; i++) {
            wfx.nChannels = lpDesc->lpFormat->nChannels;
            TRY(lpDesc->lpFormat->nSamplesPerSec, 16);
            TRY(lpDesc->lpFormat->nSamplesPerSec, 8);
            wfx.nChannels ^= 3;
            TRY(lpDesc->lpFormat->nSamplesPerSec, 16);
            TRY(lpDesc->lpFormat->nSamplesPerSec, 8);
        }

        for (i = ndlo; i < ndhi; i++) {
            /* first try with same stereo/mono option as source */
            wfx.nChannels = lpDesc->lpFormat->nChannels;
            TRY(96000, 16);
            TRY(48000, 16);
            TRY(44100, 16);
            TRY(22050, 16);
            TRY(11025, 16);

            /* 2^3 => 1, 1^3 => 2, so if stereo, try mono (and the other way around) */
            wfx.nChannels ^= 3;
            TRY(96000, 16);
            TRY(48000, 16);
            TRY(44100, 16);
            TRY(22050, 16);
            TRY(11025, 16);

            /* first try with same stereo/mono option as source */
            wfx.nChannels = lpDesc->lpFormat->nChannels;
            TRY(96000, 8);
            TRY(48000, 8);
            TRY(44100, 8);
            TRY(22050, 8);
            TRY(11025, 8);

            /* 2^3 => 1, 1^3 => 2, so if stereo, try mono (and the other way around) */
            wfx.nChannels ^= 3;
            TRY(96000, 8);
            TRY(48000, 8);
            TRY(44100, 8);
            TRY(22050, 8);
            TRY(11025, 8);
        }
#undef TRY
    }

    HeapFree(GetProcessHeap(), 0, wim);
    return MMSYSERR_ALLOCATED;
found:
    if (dwFlags & WAVE_FORMAT_QUERY) {
	*lpdwUser = 0L;
	HeapFree(GetProcessHeap(), 0, wim);
    } else {
	*lpdwUser = (DWORD)wim;
    }
    TRACE("Ok (stream=%08lx)\n", (DWORD)wim->hAcmStream);
    return MMSYSERR_NOERROR;
}

static	DWORD	widClose(WAVEMAPDATA* wim)
{
    DWORD ret = waveInClose(wim->hInnerWave);

    if (ret == MMSYSERR_NOERROR) {
	if (wim->hAcmStream) {
	    ret = acmStreamClose(wim->hAcmStream, 0);
	}
	if (ret == MMSYSERR_NOERROR) {
	    HeapFree(GetProcessHeap(), 0, wim);
	}
    }
    return ret;
}

static	DWORD	widAddBuffer(WAVEMAPDATA* wim, LPWAVEHDR lpWaveHdrDst, DWORD dwParam2)
{
    PACMSTREAMHEADER	ash;
    LPWAVEHDR		lpWaveHdrSrc;

    if (!wim->hAcmStream) {
	return waveInAddBuffer(wim->hInnerWave, lpWaveHdrDst, dwParam2);
    }

    lpWaveHdrDst->dwFlags |= WHDR_INQUEUE;
    ash = (PACMSTREAMHEADER)lpWaveHdrDst->reserved;

    lpWaveHdrSrc = (LPWAVEHDR)((LPSTR)ash + sizeof(ACMSTREAMHEADER));
    return waveInAddBuffer(wim->hInnerWave, lpWaveHdrSrc, sizeof(*lpWaveHdrSrc));
}

static	DWORD	widPrepare(WAVEMAPDATA* wim, LPWAVEHDR lpWaveHdrDst, DWORD dwParam2)
{
    PACMSTREAMHEADER	ash;
    DWORD		size;
    DWORD		dwRet;
    LPWAVEHDR		lpWaveHdrSrc;

    if (!wim->hAcmStream) {
	return waveInPrepareHeader(wim->hInnerWave, lpWaveHdrDst, dwParam2);
    }
    if (acmStreamSize(wim->hAcmStream, lpWaveHdrDst->dwBufferLength, &size,
		      ACM_STREAMSIZEF_DESTINATION) != MMSYSERR_NOERROR)
	return MMSYSERR_ERROR;

    ash = HeapAlloc(GetProcessHeap(), 0, sizeof(ACMSTREAMHEADER) + sizeof(WAVEHDR) + size);
    if (ash == NULL)
	return MMSYSERR_NOMEM;

    ash->cbStruct = sizeof(*ash);
    ash->fdwStatus = 0L;
    ash->dwUser = (DWORD)lpWaveHdrDst;
    ash->pbSrc = (LPSTR)ash + sizeof(ACMSTREAMHEADER) + sizeof(WAVEHDR);
    ash->cbSrcLength = size;
    /* ash->cbSrcLengthUsed */
    ash->dwSrcUser = 0L; /* FIXME ? */
    ash->pbDst = lpWaveHdrDst->lpData;
    ash->cbDstLength = lpWaveHdrDst->dwBufferLength;
    /* ash->cbDstLengthUsed */
    ash->dwDstUser = lpWaveHdrDst->dwUser; /* FIXME ? */
    dwRet = acmStreamPrepareHeader(wim->hAcmStream, ash, 0L);
    if (dwRet != MMSYSERR_NOERROR)
	goto errCleanUp;

    lpWaveHdrSrc = (LPWAVEHDR)((LPSTR)ash + sizeof(ACMSTREAMHEADER));
    lpWaveHdrSrc->lpData = ash->pbSrc;
    lpWaveHdrSrc->dwBufferLength = size; /* conversion is not done yet */
    lpWaveHdrSrc->dwFlags = 0;
    lpWaveHdrSrc->dwLoops = 0;
    dwRet = waveInPrepareHeader(wim->hInnerWave, lpWaveHdrSrc, sizeof(*lpWaveHdrSrc));
    if (dwRet != MMSYSERR_NOERROR)
	goto errCleanUp;

    lpWaveHdrDst->reserved = (DWORD)ash;
    lpWaveHdrDst->dwFlags = WHDR_PREPARED;
    TRACE("=> (0)\n");
    return MMSYSERR_NOERROR;
errCleanUp:
    TRACE("=> (%ld)\n", dwRet);
    HeapFree(GetProcessHeap(), 0, ash);
    return dwRet;
}

static	DWORD	widUnprepare(WAVEMAPDATA* wim, LPWAVEHDR lpWaveHdrDst, DWORD dwParam2)
{
    PACMSTREAMHEADER	ash;
    LPWAVEHDR		lpWaveHdrSrc;
    DWORD		dwRet1, dwRet2;

    if (!wim->hAcmStream) {
	return waveInUnprepareHeader(wim->hInnerWave, lpWaveHdrDst, dwParam2);
    }
    ash = (PACMSTREAMHEADER)lpWaveHdrDst->reserved;
    dwRet1 = acmStreamUnprepareHeader(wim->hAcmStream, ash, 0L);

    lpWaveHdrSrc = (LPWAVEHDR)((LPSTR)ash + sizeof(ACMSTREAMHEADER));
    dwRet2 = waveInUnprepareHeader(wim->hInnerWave, lpWaveHdrSrc, sizeof(*lpWaveHdrSrc));

    HeapFree(GetProcessHeap(), 0, ash);

    lpWaveHdrDst->dwFlags &= ~WHDR_PREPARED;
    return (dwRet1 == MMSYSERR_NOERROR) ? dwRet2 : dwRet1;
}

static	DWORD	widGetPosition(WAVEMAPDATA* wim, LPMMTIME lpTime, DWORD dwParam2)
{
    if (wim->hAcmStream)
	FIXME("No position conversion done for PCM => non-PCM, returning PCM position\n");
    return waveInGetPosition(wim->hInnerWave, lpTime, dwParam2);
}

static	DWORD	widGetDevCaps(UINT wDevID, WAVEMAPDATA* wim, LPWAVEINCAPSA lpWaveCaps, DWORD dwParam2)
{
    /* if opened low driver, forward message */
    if (WAVEMAP_IsData(wim))
	return waveInGetDevCapsA(wim->hInnerWave, lpWaveCaps, dwParam2);
    /* otherwise, return caps of mapper itself */
    if (wDevID == (UINT)-1 || wDevID == (UINT16)-1) {
	lpWaveCaps->wMid = 0x00FF;
	lpWaveCaps->wPid = 0x0001;
	lpWaveCaps->vDriverVersion = 0x0001;
	strcpy(lpWaveCaps->szPname, "Wine wave in mapper");
	lpWaveCaps->dwFormats =
	    WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4S16 |
	    WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
	    WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16;
	lpWaveCaps->wChannels = 2;
	return MMSYSERR_NOERROR;
    }
    ERR("This shouldn't happen\n");
    return MMSYSERR_ERROR;
}

static	DWORD	widStop(WAVEMAPDATA* wim)
{
    return waveInStop(wim->hInnerWave);
}

static	DWORD	widStart(WAVEMAPDATA* wim)
{
    return waveInStart(wim->hInnerWave);
}

static	DWORD	widReset(WAVEMAPDATA* wim)
{
    return waveInReset(wim->hInnerWave);
}

static  DWORD	widMapperStatus(WAVEMAPDATA* wim, DWORD flags, LPVOID ptr)
{
    UINT	id;
    DWORD	ret = MMSYSERR_NOTSUPPORTED;

    switch (flags) {
    case WAVEIN_MAPPER_STATUS_DEVICE:
	ret = waveInGetID(wim->hInnerWave, &id);
	*(LPDWORD)ptr = id;
	break;
    case WAVEIN_MAPPER_STATUS_MAPPED:
	FIXME("Unsupported yet flag=%ld\n", flags);
	*(LPDWORD)ptr = 0; /* FIXME ?? */
	break;
    case WAVEIN_MAPPER_STATUS_FORMAT:
	FIXME("Unsupported flag=%ld\n", flags);
	/* ptr points to a WAVEFORMATEX struct  - before or after streaming ? */
	*(LPDWORD)ptr = 0; /* FIXME ?? */
	break;
    default:
	FIXME("Unsupported flag=%ld\n", flags);
	*(LPDWORD)ptr = 0;
	break;
    }
    return ret;
}

/**************************************************************************
 * 				widMessage (MSACMMAP.@)
 */
DWORD WINAPI WAVEMAP_widMessage(WORD wDevID, WORD wMsg, DWORD dwUser,
				DWORD dwParam1, DWORD dwParam2)
{
    TRACE("(%u, %04X, %08lX, %08lX, %08lX);\n",
	  wDevID, wMsg, dwUser, dwParam1, dwParam2);

    switch (wMsg) {
    case DRVM_INIT:
    case DRVM_EXIT:
    case DRVM_ENABLE:
    case DRVM_DISABLE:
	/* FIXME: Pretend this is supported */
	return 0;

    case WIDM_OPEN:		return widOpen          ((LPDWORD)dwUser,     (LPWAVEOPENDESC)dwParam1, dwParam2);
    case WIDM_CLOSE:		return widClose         ((WAVEMAPDATA*)dwUser);

    case WIDM_ADDBUFFER:	return widAddBuffer     ((WAVEMAPDATA*)dwUser, (LPWAVEHDR)dwParam1, 	dwParam2);
    case WIDM_PREPARE:		return widPrepare       ((WAVEMAPDATA*)dwUser, (LPWAVEHDR)dwParam1, 	dwParam2);
    case WIDM_UNPREPARE:	return widUnprepare     ((WAVEMAPDATA*)dwUser, (LPWAVEHDR)dwParam1, 	dwParam2);
    case WIDM_GETDEVCAPS:	return widGetDevCaps    (wDevID, (WAVEMAPDATA*)dwUser, (LPWAVEINCAPSA)dwParam1, dwParam2);
    case WIDM_GETNUMDEVS:	return 1;
    case WIDM_GETPOS:		return widGetPosition   ((WAVEMAPDATA*)dwUser, (LPMMTIME)dwParam1, 	dwParam2);
    case WIDM_RESET:		return widReset         ((WAVEMAPDATA*)dwUser);
    case WIDM_START:		return widStart         ((WAVEMAPDATA*)dwUser);
    case WIDM_STOP:		return widStop          ((WAVEMAPDATA*)dwUser);
    case WIDM_MAPPER_STATUS:	return widMapperStatus  ((WAVEMAPDATA*)dwUser, dwParam1, (LPVOID)dwParam2);
    default:
	FIXME("unknown message %u!\n", wMsg);
    }
    return MMSYSERR_NOTSUPPORTED;
}

/*======================================================================*
 *                  Driver part                                         *
 *======================================================================*/

static	struct WINE_WAVEMAP* oss = NULL;

/**************************************************************************
 * 				WAVEMAP_drvOpen			[internal]
 */
static	DWORD	WAVEMAP_drvOpen(LPSTR str)
{
    if (oss)
	return 0;

    /* I know, this is ugly, but who cares... */
    oss = (struct WINE_WAVEMAP*)1;
    return 1;
}

/**************************************************************************
 * 				WAVEMAP_drvClose		[internal]
 */
static	DWORD	WAVEMAP_drvClose(DWORD dwDevID)
{
    if (oss) {
	oss = NULL;
	return 1;
    }
    return 0;
}

/**************************************************************************
 * 				DriverProc (MSACMMAP.@)
 */
LONG CALLBACK	WAVEMAP_DriverProc(DWORD dwDevID, HDRVR hDriv, DWORD wMsg,
				   DWORD dwParam1, DWORD dwParam2)
{
    TRACE("(%08lX, %04X, %08lX, %08lX, %08lX)\n",
	  dwDevID, hDriv, wMsg, dwParam1, dwParam2);

    switch(wMsg) {
    case DRV_LOAD:		return 1;
    case DRV_FREE:		return 1;
    case DRV_OPEN:		return WAVEMAP_drvOpen((LPSTR)dwParam1);
    case DRV_CLOSE:		return WAVEMAP_drvClose(dwDevID);
    case DRV_ENABLE:		return 1;
    case DRV_DISABLE:		return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "WAVEMAP MultiMedia Driver !", "OSS Driver", MB_OK);	return 1;
    case DRV_INSTALL:		return DRVCNF_RESTART;
    case DRV_REMOVE:		return DRVCNF_RESTART;
    default:
	return DefDriverProc(dwDevID, hDriv, wMsg, dwParam1, dwParam2);
    }
}


