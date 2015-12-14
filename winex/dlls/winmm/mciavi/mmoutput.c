/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 * Digital video MCI Wine Driver
 *
 * Copyright 1999, 2000 Eric POUECH
 */

#include "private_mciavi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mciavi);

static BOOL MCIAVI_GetInfoAudio(WINE_MCIAVI* wma, const MMCKINFO* mmckList)
{
    MMCKINFO	mmckInfo;

    mmckInfo.ckid = ckidSTREAMHEADER;
    if (mmioDescend(wma->hFile, &mmckInfo, mmckList, MMIO_FINDCHUNK) != 0) {
	WARN("Can't find 'strh' chunk\n");
	return FALSE;
    }

    mmioRead(wma->hFile, (LPSTR)&wma->ash_audio, sizeof(wma->ash_audio));

    TRACE("ash.fccType='%c%c%c%c'\n", 		LOBYTE(LOWORD(wma->ash_audio.fccType)),
	                                        HIBYTE(LOWORD(wma->ash_audio.fccType)),
	                                        LOBYTE(HIWORD(wma->ash_audio.fccType)),
	                                        HIBYTE(HIWORD(wma->ash_audio.fccType)));
    TRACE("ash.fccHandler='%c%c%c%c'\n",	LOBYTE(LOWORD(wma->ash_audio.fccHandler)),
	                                        HIBYTE(LOWORD(wma->ash_audio.fccHandler)),
	                                        LOBYTE(HIWORD(wma->ash_audio.fccHandler)),
	                                        HIBYTE(HIWORD(wma->ash_audio.fccHandler)));
    TRACE("ash.dwFlags=%ld\n", 			wma->ash_audio.dwFlags);
    TRACE("ash.wPriority=%d\n", 		wma->ash_audio.wPriority);
    TRACE("ash.wLanguage=%d\n", 		wma->ash_audio.wLanguage);
    TRACE("ash.dwInitialFrames=%ld\n", 		wma->ash_audio.dwInitialFrames);
    TRACE("ash.dwScale=%ld\n", 			wma->ash_audio.dwScale);
    TRACE("ash.dwRate=%ld\n", 			wma->ash_audio.dwRate);
    TRACE("ash.dwStart=%ld\n", 			wma->ash_audio.dwStart);
    TRACE("ash.dwLength=%ld\n", 		wma->ash_audio.dwLength);
    TRACE("ash.dwSuggestedBufferSize=%ld\n", 	wma->ash_audio.dwSuggestedBufferSize);
    TRACE("ash.dwQuality=%ld\n", 		wma->ash_audio.dwQuality);
    TRACE("ash.dwSampleSize=%ld\n", 		wma->ash_audio.dwSampleSize);
    TRACE("ash.rcFrame=(%d,%d,%d,%d)\n", 	wma->ash_audio.rcFrame.top, wma->ash_audio.rcFrame.left,
	  wma->ash_audio.rcFrame.bottom, wma->ash_audio.rcFrame.right);

    mmioAscend(wma->hFile, &mmckInfo, 0);

    mmckInfo.ckid = ckidSTREAMFORMAT;
    if (mmioDescend(wma->hFile, &mmckInfo, mmckList, MMIO_FINDCHUNK) != 0) {
	WARN("Can't find 'strh' chunk\n");
	return FALSE;
    }
    if (mmckInfo.cksize < sizeof(WAVEFORMAT)) {
	WARN("Size of strf chunk (%ld) < audio format struct\n", mmckInfo.cksize);
	return FALSE;
    }
    wma->lpWaveFormat = HeapAlloc(GetProcessHeap(), 0, mmckInfo.cksize);
    if (!wma->lpWaveFormat) {
	WARN("Can't alloc WaveFormat\n");
	return FALSE;
    }

    mmioRead(wma->hFile, (LPSTR)wma->lpWaveFormat, mmckInfo.cksize);

    TRACE("waveFormat.wFormatTag=%d\n",		wma->lpWaveFormat->wFormatTag);
    TRACE("waveFormat.nChannels=%d\n", 		wma->lpWaveFormat->nChannels);
    TRACE("waveFormat.nSamplesPerSec=%ld\n",	wma->lpWaveFormat->nSamplesPerSec);
    TRACE("waveFormat.nAvgBytesPerSec=%ld\n",	wma->lpWaveFormat->nAvgBytesPerSec);
    TRACE("waveFormat.nBlockAlign=%d\n",	wma->lpWaveFormat->nBlockAlign);
    TRACE("waveFormat.wBitsPerSample=%d\n",	wma->lpWaveFormat->wBitsPerSample);
    if (mmckInfo.cksize >= sizeof(WAVEFORMATEX))
	TRACE("waveFormat.cbSize=%d\n", 		wma->lpWaveFormat->cbSize);

    mmioAscend(wma->hFile, &mmckInfo, 0);

    return TRUE;
}

static BOOL MCIAVI_GetInfoVideo(WINE_MCIAVI* wma, const MMCKINFO* mmckList)
{
    MMCKINFO	mmckInfo;

    mmckInfo.ckid = ckidSTREAMHEADER;
    if (mmioDescend(wma->hFile, &mmckInfo, mmckList, MMIO_FINDCHUNK) != 0) {
	WARN("Can't find 'strh' chunk\n");
	return FALSE;
    }

    mmioRead(wma->hFile, (LPSTR)&wma->ash_video, sizeof(wma->ash_video));

    TRACE("ash.fccType='%c%c%c%c'\n", 		LOBYTE(LOWORD(wma->ash_video.fccType)),
	                                        HIBYTE(LOWORD(wma->ash_video.fccType)),
	                                        LOBYTE(HIWORD(wma->ash_video.fccType)),
	                                        HIBYTE(HIWORD(wma->ash_video.fccType)));
    TRACE("ash.fccHandler='%c%c%c%c'\n",	LOBYTE(LOWORD(wma->ash_video.fccHandler)),
	                                        HIBYTE(LOWORD(wma->ash_video.fccHandler)),
	                                        LOBYTE(HIWORD(wma->ash_video.fccHandler)),
	                                        HIBYTE(HIWORD(wma->ash_video.fccHandler)));
    TRACE("ash.dwFlags=%ld\n", 			wma->ash_video.dwFlags);
    TRACE("ash.wPriority=%d\n", 		wma->ash_video.wPriority);
    TRACE("ash.wLanguage=%d\n", 		wma->ash_video.wLanguage);
    TRACE("ash.dwInitialFrames=%ld\n", 		wma->ash_video.dwInitialFrames);
    TRACE("ash.dwScale=%ld\n", 			wma->ash_video.dwScale);
    TRACE("ash.dwRate=%ld\n", 			wma->ash_video.dwRate);
    TRACE("ash.dwStart=%ld\n", 			wma->ash_video.dwStart);
    TRACE("ash.dwLength=%ld\n", 		wma->ash_video.dwLength);
    TRACE("ash.dwSuggestedBufferSize=%ld\n", 	wma->ash_video.dwSuggestedBufferSize);
    TRACE("ash.dwQuality=%ld\n", 		wma->ash_video.dwQuality);
    TRACE("ash.dwSampleSize=%ld\n", 		wma->ash_video.dwSampleSize);
    TRACE("ash.rcFrame=(%d,%d,%d,%d)\n", 	wma->ash_video.rcFrame.top, wma->ash_video.rcFrame.left,
	  wma->ash_video.rcFrame.bottom, wma->ash_video.rcFrame.right);

    mmioAscend(wma->hFile, &mmckInfo, 0);

    mmckInfo.ckid = ckidSTREAMFORMAT;
    if (mmioDescend(wma->hFile, &mmckInfo, mmckList, MMIO_FINDCHUNK) != 0) {
	WARN("Can't find 'strh' chunk\n");
	return FALSE;
    }

    wma->inbih = HeapAlloc(GetProcessHeap(), 0, mmckInfo.cksize);
    if (!wma->inbih) {
	WARN("Can't alloc input BIH\n");
	return FALSE;
    }

    mmioRead(wma->hFile, (LPSTR)wma->inbih, mmckInfo.cksize);

    TRACE("bih.biSize=%ld\n", 		wma->inbih->biSize);
    TRACE("bih.biWidth=%ld\n", 		wma->inbih->biWidth);
    TRACE("bih.biHeight=%ld\n", 	wma->inbih->biHeight);
    TRACE("bih.biPlanes=%d\n", 		wma->inbih->biPlanes);
    TRACE("bih.biBitCount=%d\n", 	wma->inbih->biBitCount);
    TRACE("bih.biCompression=%lx\n", 	wma->inbih->biCompression);
    TRACE("bih.biSizeImage=%ld\n", 	wma->inbih->biSizeImage);
    TRACE("bih.biXPelsPerMeter=%ld\n", 	wma->inbih->biXPelsPerMeter);
    TRACE("bih.biYPelsPerMeter=%ld\n", 	wma->inbih->biYPelsPerMeter);
    TRACE("bih.biClrUsed=%ld\n", 	wma->inbih->biClrUsed);
    TRACE("bih.biClrImportant=%ld\n", 	wma->inbih->biClrImportant);

    mmioAscend(wma->hFile, &mmckInfo, 0);

    return TRUE;
}

struct AviListBuild {
    DWORD	numVideoFrames;
    DWORD	numAudioAllocated;
    DWORD	numAudioBlocks;
    DWORD	inVideoSize;
    DWORD	inAudioSize;
};

static BOOL	MCIAVI_AddFrame(WINE_MCIAVI* wma, LPMMCKINFO mmck,
				struct AviListBuild* alb)
{
    if (mmck->ckid == ckidAVIPADDING) return TRUE;

    switch (TWOCCFromFOURCC(mmck->ckid)) {
    case cktypeDIBbits:
    case cktypeDIBcompressed:
    case cktypePALchange:
	TRACE("Adding video frame[%ld]: %ld bytes\n",
	      alb->numVideoFrames, mmck->cksize);
	if (alb->numVideoFrames < wma->dwPlayableVideoFrames) {
	    wma->lpVideoIndex[alb->numVideoFrames].dwOffset = mmck->dwDataOffset;
	    wma->lpVideoIndex[alb->numVideoFrames].dwSize = mmck->cksize;
	    if (alb->inVideoSize < mmck->cksize)
		alb->inVideoSize = mmck->cksize;
	    alb->numVideoFrames++;
	} else {
	    WARN("Too many video frames\n");
	}
	break;
    case cktypeWAVEbytes:
	TRACE("Adding audio frame[%ld]: %ld bytes\n",
	      alb->numAudioBlocks, mmck->cksize);
	if (wma->lpWaveFormat) {
	    if (alb->numAudioBlocks >= alb->numAudioAllocated) {
		alb->numAudioAllocated += 32;
		wma->lpAudioIndex = HeapReAlloc(GetProcessHeap(), 0,
						wma->lpAudioIndex,
						alb->numAudioAllocated * sizeof(struct MMIOPos));
		if (!wma->lpAudioIndex) return FALSE;
	    }
	    wma->lpAudioIndex[alb->numAudioBlocks].dwOffset = mmck->dwDataOffset;
	    wma->lpAudioIndex[alb->numAudioBlocks].dwSize = mmck->cksize;
	    if (alb->inAudioSize < mmck->cksize)
		alb->inAudioSize = mmck->cksize;
	    alb->numAudioBlocks++;
	} else {
	    WARN("Wave chunk without wave format... discarding\n");
	}
	break;
    default:
	WARN("Unknown frame type %04x\n", TWOCCFromFOURCC(mmck->ckid));
	break;
    }
    return TRUE;
}

BOOL MCIAVI_GetInfo(WINE_MCIAVI* wma)
{
    MMCKINFO		ckMainRIFF;
    MMCKINFO		mmckHead;
    MMCKINFO		mmckList;
    MMCKINFO		mmckInfo;
    struct AviListBuild alb;

    if (mmioDescend(wma->hFile, &ckMainRIFF, NULL, 0) != 0) {
	WARN("Can't find 'RIFF' chunk\n");
	return FALSE;
    }

    if ((ckMainRIFF.ckid != FOURCC_RIFF) || (ckMainRIFF.fccType != formtypeAVI)) {
	WARN("Can't find 'AVI ' chunk\n");
	return FALSE;
    }

    mmckHead.fccType = listtypeAVIHEADER;
    if (mmioDescend(wma->hFile, &mmckHead, &ckMainRIFF, MMIO_FINDLIST) != 0) {
	WARN("Can't find 'hdrl' list\n");
	return FALSE;
    }

    mmckInfo.ckid = ckidAVIMAINHDR;
    if (mmioDescend(wma->hFile, &mmckInfo, &mmckHead, MMIO_FINDCHUNK) != 0) {
	WARN("Can't find 'avih' chunk\n");
	return FALSE;
    }

    mmioRead(wma->hFile, (LPSTR)&wma->mah, sizeof(wma->mah));

    TRACE("mah.dwMicroSecPerFrame=%ld\n", 	wma->mah.dwMicroSecPerFrame);
    TRACE("mah.dwMaxBytesPerSec=%ld\n", 	wma->mah.dwMaxBytesPerSec);
    TRACE("mah.dwPaddingGranularity=%ld\n", 	wma->mah.dwPaddingGranularity);
    TRACE("mah.dwFlags=%ld\n", 			wma->mah.dwFlags);
    TRACE("mah.dwTotalFrames=%ld\n", 		wma->mah.dwTotalFrames);
    TRACE("mah.dwInitialFrames=%ld\n", 		wma->mah.dwInitialFrames);
    TRACE("mah.dwStreams=%ld\n", 		wma->mah.dwStreams);
    TRACE("mah.dwSuggestedBufferSize=%ld\n",	wma->mah.dwSuggestedBufferSize);
    TRACE("mah.dwWidth=%ld\n", 			wma->mah.dwWidth);
    TRACE("mah.dwHeight=%ld\n", 		wma->mah.dwHeight);

    mmioAscend(wma->hFile, &mmckInfo, 0);

    mmckList.fccType = listtypeSTREAMHEADER;
    if (mmioDescend(wma->hFile, &mmckList, &mmckHead, MMIO_FINDLIST) != 0) {
	WARN("Can't find 'strl' list\n");
	return FALSE;
    }

    if (!MCIAVI_GetInfoVideo(wma, &mmckList)) {
	return FALSE;
    }

    mmioAscend(wma->hFile, &mmckList, 0);

    mmckList.fccType = listtypeSTREAMHEADER;
    if (mmioDescend(wma->hFile, &mmckList, &mmckHead, MMIO_FINDLIST) == 0) {
	if (!MCIAVI_GetInfoAudio(wma, &mmckList)) {
	    return FALSE;
	}
	mmioAscend(wma->hFile, &mmckList, 0);
    }

    mmioAscend(wma->hFile, &mmckHead, 0);

    /* no need to read optional JUNK chunk */

    mmckList.fccType = listtypeAVIMOVIE;
    if (mmioDescend(wma->hFile, &mmckList, &ckMainRIFF, MMIO_FINDLIST) != 0) {
	WARN("Can't find 'movi' list\n");
	return FALSE;
    }

    wma->dwPlayableVideoFrames = wma->mah.dwTotalFrames;
    wma->lpVideoIndex = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				  wma->dwPlayableVideoFrames * sizeof(struct MMIOPos));
    if (!wma->lpVideoIndex) {
	WARN("Can't alloc video index array\n");
	return FALSE;
    }
    wma->dwPlayableAudioBlocks = 0;
    wma->lpAudioIndex = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				  wma->dwPlayableVideoFrames * sizeof(struct MMIOPos));
    if (!wma->lpAudioIndex) {
	WARN("Can't alloc audio index array\n");
	return FALSE;
    }

    alb.numAudioBlocks = alb.numVideoFrames = 0;
    alb.inVideoSize = alb.inAudioSize = 0;
    alb.numAudioAllocated = 0;

    while (mmioDescend(wma->hFile, &mmckInfo, &mmckList, 0) == 0) {
	if (mmckInfo.fccType == listtypeAVIRECORD) {
	    MMCKINFO	tmp;

	    while (mmioDescend(wma->hFile, &tmp, &mmckInfo, 0) == 0) {
		MCIAVI_AddFrame(wma, &tmp, &alb);
		mmioAscend(wma->hFile, &tmp, 0);
	    }
	} else {
	    MCIAVI_AddFrame(wma, &mmckInfo, &alb);
	}

	mmioAscend(wma->hFile, &mmckInfo, 0);
    }
    if (alb.numVideoFrames != wma->dwPlayableVideoFrames) {
	WARN("Found %ld video frames (/%ld), reducing playable frames\n",
	     alb.numVideoFrames, wma->dwPlayableVideoFrames);
	wma->dwPlayableVideoFrames = alb.numVideoFrames;
    }
    wma->dwPlayableAudioBlocks = alb.numAudioBlocks;

    if (alb.inVideoSize > wma->ash_video.dwSuggestedBufferSize) {
	WARN("inVideoSize=%ld suggestedSize=%ld\n", alb.inVideoSize, wma->ash_video.dwSuggestedBufferSize);
	wma->ash_video.dwSuggestedBufferSize = alb.inVideoSize;
    }
    if (alb.inAudioSize > wma->ash_audio.dwSuggestedBufferSize) {
	WARN("inAudioSize=%ld suggestedSize=%ld\n", alb.inAudioSize, wma->ash_audio.dwSuggestedBufferSize);
	wma->ash_audio.dwSuggestedBufferSize = alb.inAudioSize;
    }

    wma->indata = HeapAlloc(GetProcessHeap(), 0, wma->ash_video.dwSuggestedBufferSize);
    if (!wma->indata) {
	WARN("Can't alloc input buffer\n");
	return FALSE;
    }

    return TRUE;
}

BOOL    MCIAVI_OpenVideo(WINE_MCIAVI* wma)
{
    DWORD	outSize;
    FOURCC	fcc = wma->ash_video.fccHandler;

    /* check uncompressed AVI */
    if ((fcc == mmioFOURCC('D','I','B',' ')) ||
	(fcc == mmioFOURCC('R','L','E',' '))) {
	wma->hic = 0;
	MCIAVI_DrawFrame(wma);
	return TRUE;
    }

    /* get the right handle */
    if (fcc == 0) fcc = wma->inbih->biCompression;
    if (fcc == mmioFOURCC('C','R','A','M')) fcc = mmioFOURCC('M','S','V','C');

    /* try to get a decompressor for that type */
    wma->hic = ICLocate(ICTYPE_VIDEO, fcc, wma->inbih, NULL, ICMODE_DECOMPRESS);
    if (!wma->hic) {
	WARN("Can't locate codec for the file\n");
	return FALSE;
    }

    outSize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);

    wma->outbih = HeapAlloc(GetProcessHeap(), 0, outSize);
    if (!wma->outbih) {
	WARN("Can't alloc output BIH\n");
	return FALSE;
    }
    if (!ICGetDisplayFormat(wma->hic, wma->inbih, wma->outbih, 0, 0, 0)) {
	WARN("Can't open decompressor\n");
	return FALSE;
    }

    TRACE("bih.biSize=%ld\n", 		wma->outbih->biSize);
    TRACE("bih.biWidth=%ld\n", 		wma->outbih->biWidth);
    TRACE("bih.biHeight=%ld\n", 	wma->outbih->biHeight);
    TRACE("bih.biPlanes=%d\n", 		wma->outbih->biPlanes);
    TRACE("bih.biBitCount=%d\n", 	wma->outbih->biBitCount);
    TRACE("bih.biCompression=%lx\n", 	wma->outbih->biCompression);
    TRACE("bih.biSizeImage=%ld\n", 	wma->outbih->biSizeImage);
    TRACE("bih.biXPelsPerMeter=%ld\n", 	wma->outbih->biXPelsPerMeter);
    TRACE("bih.biYPelsPerMeter=%ld\n", 	wma->outbih->biYPelsPerMeter);
    TRACE("bih.biClrUsed=%ld\n", 	wma->outbih->biClrUsed);
    TRACE("bih.biClrImportant=%ld\n", 	wma->outbih->biClrImportant);

    wma->outdata = HeapAlloc(GetProcessHeap(), 0, wma->outbih->biSizeImage);
    if (!wma->outdata) {
	WARN("Can't alloc output buffer\n");
	return FALSE;
    }

    if (ICSendMessage(wma->hic, ICM_DECOMPRESS_BEGIN,
		      (DWORD)wma->inbih, (DWORD)wma->outbih) != ICERR_OK) {
	WARN("Can't begin decompression\n");
	return FALSE;
    }

    MCIAVI_DrawFrame(wma);

    return TRUE;
}

static void CALLBACK MCIAVI_waveCallback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance,
					 DWORD dwParam1, DWORD dwParam2)
{
    WINE_MCIAVI*	wma = (WINE_MCIAVI*)dwInstance;

    switch (uMsg) {
    case WOM_OPEN:
    case WOM_CLOSE:
	break;
    case WOM_DONE:
	InterlockedIncrement(&wma->dwEventCount);
	TRACE("Returning waveHdr=%lx\n", dwParam1);
	SetEvent(wma->hEvent);
	break;
    default:
	ERR("Unknown uMsg=%d\n", uMsg);
    }
}

DWORD MCIAVI_OpenAudio(WINE_MCIAVI* wma, unsigned* nHdr, LPWAVEHDR* pWaveHdr)
{
    DWORD	dwRet;
    LPWAVEHDR	waveHdr;
    unsigned	i;

    dwRet = waveOutOpen(&wma->hWave, WAVE_MAPPER, wma->lpWaveFormat,
			(DWORD)MCIAVI_waveCallback, (DWORD)wma, CALLBACK_FUNCTION);
    if (dwRet != 0) {
	TRACE("Can't open low level audio device %ld\n", dwRet);
	dwRet = MCIERR_DEVICE_OPEN;
	wma->hWave = 0;
	goto cleanUp;
    }

    /* FIXME: should set up a heuristic to compute the number of wave headers
     * to be used...
     */
    *nHdr = 7;
    waveHdr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			*nHdr * (sizeof(WAVEHDR) + wma->ash_audio.dwSuggestedBufferSize));
    if (!waveHdr) {
	TRACE("Can't alloc wave headers\n");
	dwRet = MCIERR_DEVICE_OPEN;
	goto cleanUp;
    }

    for (i = 0; i < *nHdr; i++) {
	/* other fields are zero:ed on allocation */
	waveHdr[i].lpData = (char*)waveHdr +
	    *nHdr * sizeof(WAVEHDR) + i * wma->ash_audio.dwSuggestedBufferSize;
	waveHdr[i].dwBufferLength = wma->ash_audio.dwSuggestedBufferSize;
	if (waveOutPrepareHeader(wma->hWave, &waveHdr[i], sizeof(WAVEHDR))) {
	    dwRet = MCIERR_INTERNAL;
	    goto cleanUp;
	}
    }

    if (wma->dwCurrVideoFrame != 0 && wma->lpWaveFormat) {
	FIXME("Should recompute dwCurrAudioBlock, except unsynchronized sound & video\n");
    }
    wma->dwCurrAudioBlock = 0;

    wma->hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    wma->dwEventCount = *nHdr - 1;
    *pWaveHdr = waveHdr;
 cleanUp:
    return dwRet;
}

void MCIAVI_PlayAudioBlocks(WINE_MCIAVI* wma, unsigned nHdr, LPWAVEHDR waveHdr)
{
    TRACE("%ld (ec=%lu)\n", wma->lpAudioIndex[wma->dwCurrAudioBlock].dwOffset, wma->dwEventCount);

    /* push as many blocks as possible => audio gets priority */
    while (wma->dwStatus != MCI_MODE_STOP && wma->dwStatus != MCI_MODE_NOT_READY &&
	   wma->dwCurrAudioBlock < wma->dwPlayableAudioBlocks) {
	unsigned	whidx = wma->dwCurrAudioBlock % nHdr;

	ResetEvent(wma->hEvent);
	if (InterlockedDecrement(&wma->dwEventCount) < 0 ||
	    !wma->lpAudioIndex[wma->dwCurrAudioBlock].dwOffset)
	    break;

	mmioSeek(wma->hFile, wma->lpAudioIndex[wma->dwCurrAudioBlock].dwOffset, SEEK_SET);
	mmioRead(wma->hFile, waveHdr[whidx].lpData, wma->lpAudioIndex[wma->dwCurrAudioBlock].dwSize);

	waveHdr[whidx].dwFlags &= ~WHDR_DONE;
	waveHdr[whidx].dwBufferLength = wma->lpAudioIndex[wma->dwCurrAudioBlock].dwSize;
	waveOutWrite(wma->hWave, &waveHdr[whidx], sizeof(WAVEHDR));
	wma->dwCurrAudioBlock++;
    }
    InterlockedIncrement(&wma->dwEventCount);
}

LRESULT MCIAVI_PaintFrame(WINE_MCIAVI* wma, HDC hDC)
{
    void* 		pBitmapData = NULL;
    LPBITMAPINFO	pBitmapInfo = NULL;
    HDC 		hdcMem;
    HBITMAP		hbmOld;
    int 		nWidth;
    int 		nHeight;

    if (!hDC || !wma->inbih)
	return TRUE;

    TRACE("Painting frame %lu\n", wma->dwCurrVideoFrame);

    if (wma->hic) {
        pBitmapData = wma->outdata;
        pBitmapInfo = (LPBITMAPINFO)wma->outbih;

        nWidth = wma->outbih->biWidth;
        nHeight = wma->outbih->biHeight;
    } else {
        pBitmapData = wma->indata;
        pBitmapInfo = (LPBITMAPINFO)wma->inbih;

        nWidth = wma->inbih->biWidth;
        nHeight = wma->inbih->biHeight;
    }

    if (!wma->hbmFrame)
        wma->hbmFrame = CreateCompatibleBitmap(hDC, nWidth, nHeight);

    SetDIBits(hDC, wma->hbmFrame, 0, nHeight, pBitmapData, pBitmapInfo, DIB_RGB_COLORS);

    hdcMem = CreateCompatibleDC(hDC);
    hbmOld = SelectObject(hdcMem, wma->hbmFrame);

    BitBlt(hDC, 0, 0, nWidth, nHeight, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    return TRUE;
}

LRESULT MCIAVI_DrawFrame(WINE_MCIAVI* wma)
{
    HDC		hDC;

    TRACE("Drawing frame %lu\n", wma->dwCurrVideoFrame);

    if (!wma->lpVideoIndex[wma->dwCurrVideoFrame].dwOffset)
	return FALSE;

    EnterCriticalSection(&wma->cs);

    mmioSeek(wma->hFile, wma->lpVideoIndex[wma->dwCurrVideoFrame].dwOffset, SEEK_SET);
    mmioRead(wma->hFile, wma->indata, wma->lpVideoIndex[wma->dwCurrVideoFrame].dwSize);

    /* FIXME ? */
    wma->inbih->biSizeImage = wma->lpVideoIndex[wma->dwCurrVideoFrame].dwSize;

    if (wma->hic &&
	ICDecompress(wma->hic, 0, wma->inbih, wma->indata,
		     wma->outbih, wma->outdata) != ICERR_OK) {
	LeaveCriticalSection(&wma->cs);
	WARN("Decompression error\n");
	return FALSE;
    }

    if (IsWindowVisible(wma->hWnd) && (hDC = GetDC(wma->hWnd)) != 0) {
	MCIAVI_PaintFrame(wma, hDC);
	ReleaseDC(wma->hWnd, hDC);
    }

    LeaveCriticalSection(&wma->cs);

    return TRUE;
}

