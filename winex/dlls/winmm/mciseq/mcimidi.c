/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 * Sample MIDI Wine Driver for Linux
 *
 * Copyright 	1994 Martin Ayotte
 *		1999 Eric Pouech
 */

/*
 * Eric POUECH :
 * 98/7 changes for making this MIDI driver work on OSS
 * 	current support is limited to MIDI ports of OSS systems
 * 98/9	rewriting MCI code for MIDI
 * 98/11 splitted in midi.c and mcimidi.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "mmddk.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mcimidi);

#define MIDI_NOTEOFF             0x80
#define MIDI_NOTEON              0x90

typedef struct {
    DWORD		dwFirst;		/* offset in file of track */
    DWORD		dwLast;			/* number of bytes in file of track */
    DWORD		dwIndex;		/* current index in file (dwFirst <= dwIndex < dwLast) */
    DWORD		dwLength;		/* number of pulses in this track */
    DWORD		dwEventPulse;		/* current pulse # (event) pointed by dwIndex */
    DWORD		dwEventData;		/* current data    (event) pointed by dwIndex */
    WORD		wEventLength;		/* current length  (event) pointed by dwIndex */
    WORD		wStatus : 1,		/* 1 : playing, 0 : done */
	                wTrackNr : 7,
	                wLastCommand : 8;	/* last MIDI command on track */
} MCI_MIDITRACK;

typedef struct tagWINE_MCIMIDI {
    UINT		wDevID;			/* the MCI one */
    HMIDI		hMidi;
    int			nUseCount;          	/* Incremented for each shared open          */
    WORD		wNotifyDeviceID;    	/* MCI device ID with a pending notification */
    HANDLE 		hCallback;         	/* Callback handle for pending notification  */
    HMMIO		hFile;	            	/* mmio file handle open as Element          */
    LPSTR		lpstrElementName;	/* Name of file */
    LPSTR		lpstrCopyright;
    LPSTR		lpstrName;
    WORD		dwStatus;		/* one from MCI_MODE_xxxx */
    DWORD		dwMciTimeFormat;	/* One of the supported MCI_FORMAT_xxxx */
    WORD		wFormat;		/* Format of MIDI hFile (0, 1 or 2) */
    WORD		nTracks;		/* Number of tracks in hFile */
    WORD		nDivision;		/* Number of division in hFile PPQN or SMPTE */
    WORD		wStartedPlaying;
    DWORD		dwTempo;		/* Tempo (# of 1/4 note per second */
    MCI_MIDITRACK*     	tracks;			/* Content of each track */
    DWORD		dwPulse;
    DWORD		dwPositionMS;
    DWORD		dwStartTicks;
} WINE_MCIMIDI;

/* ===================================================================
 * ===================================================================
 * FIXME: should be using the new mmThreadXXXX functions from WINMM
 * instead of those
 * it would require to add a wine internal flag to mmThreadCreate
 * in order to pass a 32 bit function instead of a 16 bit
 * ===================================================================
 * =================================================================== */

struct SCA {
    UINT 	wDevID;
    UINT 	wMsg;
    DWORD 	dwParam1;
    DWORD 	dwParam2;
};

/* EPP DWORD WINAPI mciSendCommandA(UINT wDevID, UINT wMsg, DWORD dwParam1, DWORD dwParam2); */

/**************************************************************************
 * 				MCI_SCAStarter			[internal]
 */
static DWORD CALLBACK	MCI_SCAStarter(LPVOID arg)
{
    struct SCA*	sca = (struct SCA*)arg;
    DWORD		ret;

    TRACE("In thread before async command (%08x,%u,%08lx,%08lx)\n",
	  sca->wDevID, sca->wMsg, sca->dwParam1, sca->dwParam2);
    ret = mciSendCommandA(sca->wDevID, sca->wMsg, sca->dwParam1 | MCI_WAIT, sca->dwParam2);
    TRACE("In thread after async command (%08x,%u,%08lx,%08lx)\n",
	  sca->wDevID, sca->wMsg, sca->dwParam1, sca->dwParam2);
    HeapFree(GetProcessHeap(), 0, sca);
    ExitThread(ret);
    WARN("Should not happen ? what's wrong \n");
    /* should not go after this point */
    return ret;
}

/**************************************************************************
 * 				MCI_SendCommandAsync		[internal]
 */
static	DWORD MCI_SendCommandAsync(UINT wDevID, UINT wMsg, DWORD dwParam1,
				   DWORD dwParam2, UINT size)
{
    HANDLE handle;
    struct SCA*	sca = HeapAlloc(GetProcessHeap(), 0, sizeof(struct SCA) + size);

    if (sca == 0)
	return MCIERR_OUT_OF_MEMORY;

    sca->wDevID   = wDevID;
    sca->wMsg     = wMsg;
    sca->dwParam1 = dwParam1;

    if (size && dwParam2) {
	sca->dwParam2 = (DWORD)sca + sizeof(struct SCA);
	/* copy structure passed by program in dwParam2 to be sure
	 * we can still use it whatever the program does
	 */
	memcpy((LPVOID)sca->dwParam2, (LPVOID)dwParam2, size);
    } else {
	sca->dwParam2 = dwParam2;
    }

    if ((handle = CreateThread(NULL, 0, MCI_SCAStarter, sca, 0, NULL)) == 0) {
	WARN("Couldn't allocate thread for async command handling, sending synchonously\n");
	return MCI_SCAStarter(&sca);
    }
    CloseHandle(handle);
    return 0;
}

/*======================================================================*
 *                  	    MCI MIDI implemantation			*
 *======================================================================*/

static DWORD MIDI_mciResume(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms);

/**************************************************************************
 * 				MIDI_drvOpen			[internal]
 */
static	DWORD	MIDI_drvOpen(LPSTR str, LPMCI_OPEN_DRIVER_PARMSA modp)
{
    WINE_MCIMIDI*	wmm;

    if (!modp) return 0xFFFFFFFF;

    wmm = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WINE_MCIMIDI));

    if (!wmm)
	return 0;

    wmm->wDevID = modp->wDeviceID;
    mciSetDriverData(wmm->wDevID, (DWORD)wmm);
    modp->wCustomCommandTable = MCI_NO_COMMAND_TABLE;
    modp->wType = MCI_DEVTYPE_SEQUENCER;
    return modp->wDeviceID;
}

/**************************************************************************
 * 				MCIMIDI_drvClose		[internal]
 */
static	DWORD	MIDI_drvClose(DWORD dwDevID)
{
    WINE_MCIMIDI*  wmm = (WINE_MCIMIDI*)mciGetDriverData(dwDevID);

    if (wmm) {
	HeapFree(GetProcessHeap(), 0, wmm);
	mciSetDriverData(dwDevID, 0);
	return 1;
    }
    return (dwDevID == 0xFFFFFFFF) ? 1 : 0;
}

/**************************************************************************
 * 				MIDI_mciGetOpenDev		[internal]
 */
static WINE_MCIMIDI*  MIDI_mciGetOpenDev(UINT wDevID)
{
    WINE_MCIMIDI*	wmm = (WINE_MCIMIDI*)mciGetDriverData(wDevID);

    if (wmm == NULL || wmm->nUseCount == 0) {
	WARN("Invalid wDevID=%u\n", wDevID);
	return 0;
    }
    return wmm;
}

/**************************************************************************
 * 				MIDI_mciReadByte		[internal]
 */
static DWORD MIDI_mciReadByte(WINE_MCIMIDI* wmm, BYTE *lpbyt)
{
    DWORD	ret = 0;

    if (lpbyt == NULL ||
	mmioRead(wmm->hFile, (HPSTR)lpbyt, sizeof(BYTE)) != (long)sizeof(BYTE)) {
	WARN("Error reading wmm=%p\n", wmm);
	ret = MCIERR_INVALID_FILE;
    }

    return ret;
}

/**************************************************************************
 * 				MIDI_mciReadWord		[internal]
 */
static DWORD MIDI_mciReadWord(WINE_MCIMIDI* wmm, LPWORD lpw)
{
    BYTE	hibyte, lobyte;
    DWORD	ret = MCIERR_INVALID_FILE;

    if (lpw != NULL &&
	MIDI_mciReadByte(wmm, &hibyte) == 0 &&
	MIDI_mciReadByte(wmm, &lobyte) == 0) {
	*lpw = ((WORD)hibyte << 8) + lobyte;
	ret = 0;
    }
    return ret;
}

/**************************************************************************
 * 				MIDI_mciReadLong		[internal]
 */
static DWORD MIDI_mciReadLong(WINE_MCIMIDI* wmm, LPDWORD lpdw)
{
    WORD	hiword, loword;
    DWORD	ret = MCIERR_INVALID_FILE;

    if (lpdw != NULL &&
	MIDI_mciReadWord(wmm, &hiword) == 0 &&
	MIDI_mciReadWord(wmm, &loword) == 0) {
	*lpdw = MAKELONG(loword, hiword);
	ret = 0;
    }
    return ret;
}

/**************************************************************************
 *  				MIDI_mciReadVaryLen		[internal]
 */
static WORD MIDI_mciReadVaryLen(WINE_MCIMIDI* wmm, LPDWORD lpdw)
{
    BYTE	byte;
    DWORD	value = 0;
    WORD	ret = 0;

    if (lpdw == NULL) {
	ret = MCIERR_INVALID_FILE;
    } else {
	do {
	    if (MIDI_mciReadByte(wmm, &byte) != 0) {
		return 0;
	    }
	    value = (value << 7) + (byte & 0x7F);
	    ret++;
	} while (byte & 0x80);
	*lpdw = value;
	/*
	  TRACE("val=%08lX \n", value);
	*/
    }
    return ret;
}

/**************************************************************************
 * 				MIDI_mciReadNextEvent		[internal]
 */
static DWORD	MIDI_mciReadNextEvent(WINE_MCIMIDI* wmm, MCI_MIDITRACK* mmt)
{
    BYTE	b1, b2 = 0, b3;
    WORD	hw = 0;
    DWORD	evtPulse;
    DWORD	evtLength;
    DWORD	tmp;

    if (mmioSeek(wmm->hFile, mmt->dwIndex, SEEK_SET) != mmt->dwIndex) {
	WARN("Can't seek at %08lX \n", mmt->dwIndex);
	return MCIERR_INVALID_FILE;
    }
    evtLength = MIDI_mciReadVaryLen(wmm, &evtPulse) + 1;	/* > 0 */
    MIDI_mciReadByte(wmm, &b1);
    switch (b1) {
    case 0xF0:
    case 0xF7:
	evtLength += MIDI_mciReadVaryLen(wmm, &tmp);
	evtLength += tmp;
	break;
    case 0xFF:
	MIDI_mciReadByte(wmm, &b2);	evtLength++;

	evtLength += MIDI_mciReadVaryLen(wmm, &tmp);
	if (evtLength >= 0x10000u) {
	    /* this limitation shouldn't be a problem */
	    WARN("Ouch !! Implementation limitation to 64k bytes for a MIDI event is overflowed\n");
	    hw = 0xFFFF;
	} else {
	    hw = LOWORD(evtLength);
	}
	evtLength += tmp;
	break;
    default:
	if (b1 & 0x80) { /* use running status ? */
	    mmt->wLastCommand = b1;
	    MIDI_mciReadByte(wmm, &b2);	evtLength++;
	} else {
	    b2 = b1;
	    b1 = mmt->wLastCommand;
	}
	switch ((b1 >> 4) & 0x07) {
	case 0:	case 1:	case 2: case 3: case 6:
	    MIDI_mciReadByte(wmm, &b3);	evtLength++;
	    hw = b3;
	    break;
	case 4:	case 5:
	    break;
	case 7:
	    WARN("Strange indeed b1=0x%02x\n", b1);
	}
	break;
    }
    if (mmt->dwIndex + evtLength > mmt->dwLast)
	return MCIERR_INTERNAL;

    mmt->dwEventPulse += evtPulse;
    mmt->dwEventData   = (hw << 16) + (b2 << 8) + b1;
    mmt->wEventLength  = evtLength;

    /*
      TRACE("[%u] => pulse=%08lx(%08lx), data=%08lx, length=%u\n",
      mmt->wTrackNr, mmt->dwEventPulse, evtPulse,
      mmt->dwEventData, mmt->wEventLength);
    */
    return 0;
}

/**************************************************************************
 * 				MIDI_mciReadMTrk		[internal]
 */
static DWORD MIDI_mciReadMTrk(WINE_MCIMIDI* wmm, MCI_MIDITRACK* mmt)
{
    DWORD		toberead;
    FOURCC		fourcc;

    if (mmioRead(wmm->hFile, (HPSTR)&fourcc, (long)sizeof(FOURCC)) !=
	(long)sizeof(FOURCC)) {
	return MCIERR_INVALID_FILE;
    }

    if (fourcc != mmioFOURCC('M', 'T', 'r', 'k')) {
	WARN("Can't synchronize on 'MTrk' !\n");
	return MCIERR_INVALID_FILE;
    }

    if (MIDI_mciReadLong(wmm, &toberead) != 0) {
	return MCIERR_INVALID_FILE;
    }
    mmt->dwFirst = mmioSeek(wmm->hFile, 0, SEEK_CUR); /* >= 0 */
    mmt->dwLast = mmt->dwFirst + toberead;

    /* compute # of pulses in this track */
    mmt->dwIndex = mmt->dwFirst;
    mmt->dwEventPulse = 0;

    while (MIDI_mciReadNextEvent(wmm, mmt) == 0 && LOWORD(mmt->dwEventData) != 0x2FFF) {
	char	buf[1024];
	WORD	len;

	mmt->dwIndex += mmt->wEventLength;

	switch (LOWORD(mmt->dwEventData)) {
	case 0x02FF:
	case 0x03FF:
	    /* position after meta data header */
	    mmioSeek(wmm->hFile, mmt->dwIndex + HIWORD(mmt->dwEventData), SEEK_SET);
	    len = mmt->wEventLength - HIWORD(mmt->dwEventData);

	    if (len >= sizeof(buf)) {
		WARN("Buffer for text is too small (%d bytes, when %u are needed)\n", sizeof(buf) - 1, len);
		len = sizeof(buf) - 1;
	    }
	    if (mmioRead(wmm->hFile, (HPSTR)buf, len) == len) {
		buf[len] = 0;	/* end string in case */
		switch (HIBYTE(LOWORD(mmt->dwEventData))) {
		case 0x02:
		    if (wmm->lpstrCopyright) {
			WARN("Two copyright notices (%s|%s)\n", wmm->lpstrCopyright, buf);
		    } else {
                        wmm->lpstrCopyright = HeapAlloc( GetProcessHeap(), 0, strlen(buf)+1 );
                        strcpy( wmm->lpstrCopyright, buf );
		    }
		    break;
		case 0x03:
		    if (wmm->lpstrName) {
			WARN("Two names (%s|%s)\n", wmm->lpstrName, buf);
		    } else {
			wmm->lpstrName = HeapAlloc( GetProcessHeap(), 0, strlen(buf)+1 );
                        strcpy( wmm->lpstrName, buf );
		    }
		    break;
		}
	    }
	    break;
	}
    }
    mmt->dwLength = mmt->dwEventPulse;

    TRACE("Track %u has %lu bytes and %lu pulses\n", mmt->wTrackNr, toberead, mmt->dwLength);

    /* reset track data */
    mmt->wStatus = 1;	/* ok, playing */
    mmt->dwIndex = mmt->dwFirst;
    mmt->dwEventPulse = 0;

    if (mmioSeek(wmm->hFile, 0, SEEK_CUR) != mmt->dwLast) {
	WARN("Ouch, out of sync seek=%lu track=%lu\n",
	     mmioSeek(wmm->hFile, 0, SEEK_CUR), mmt->dwLast);
	/* position at end of this track, to be ready to read next track */
	mmioSeek(wmm->hFile, mmt->dwLast, SEEK_SET);
    }

    return 0;
}

/**************************************************************************
 * 				MIDI_mciReadMThd		[internal]
 */
static DWORD MIDI_mciReadMThd(WINE_MCIMIDI* wmm, DWORD dwOffset)
{
    DWORD	toberead;
    FOURCC	fourcc;
    WORD	nt;

    TRACE("(%p, %08lX);\n", wmm, dwOffset);

    if (mmioSeek(wmm->hFile, dwOffset, SEEK_SET) != dwOffset) {
	WARN("Can't seek at %08lX begin of 'MThd' \n", dwOffset);
	return MCIERR_INVALID_FILE;
    }
    if (mmioRead(wmm->hFile, (HPSTR)&fourcc,
		   (long) sizeof(FOURCC)) != (long) sizeof(FOURCC))
	return MCIERR_INVALID_FILE;

    if (fourcc != mmioFOURCC('M', 'T', 'h', 'd')) {
	WARN("Can't synchronize on 'MThd' !\n");
	return MCIERR_INVALID_FILE;
    }

    if (MIDI_mciReadLong(wmm, &toberead) != 0 || toberead < 3 * sizeof(WORD))
	return MCIERR_INVALID_FILE;

    if (MIDI_mciReadWord(wmm, &wmm->wFormat) != 0 ||
	MIDI_mciReadWord(wmm, &wmm->nTracks) != 0 ||
	MIDI_mciReadWord(wmm, &wmm->nDivision) != 0) {
	return MCIERR_INVALID_FILE;
    }

    TRACE("toberead=0x%08lX, wFormat=0x%04X nTracks=0x%04X nDivision=0x%04X\n",
	  toberead, wmm->wFormat, wmm->nTracks, wmm->nDivision);

    /* MS doc says that the MIDI MCI time format must be put by default to the format
     * stored in the MIDI file...
     */
    if (wmm->nDivision > 0x8000) {
	/* eric.pouech@lemel.fr 98/11
	 * In did not check this very code (pulses are expressed as SMPTE sub-frames).
	 * In about 40 MB of MIDI files I have, none was SMPTE based...
	 * I'm just wondering if this is widely used :-). So, if someone has one of
	 * these files, I'd like to know about.
	 */
	FIXME("Handling SMPTE time in MIDI files has not been tested\n"
	      "Please report to comp.emulators.ms-windows.wine with MIDI file !\n");

	switch (HIBYTE(wmm->nDivision)) {
	case 0xE8:	wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_24;	break;	/* -24 */
	case 0xE7:	wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_25;	break;	/* -25 */
	case 0xE3:	wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_30DROP;	break;	/* -29 */ /* is the MCI constant correct ? */
	case 0xE2:	wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_30;	break;	/* -30 */
	default:
	    WARN("Unsupported number of frames %d\n", -(char)HIBYTE(wmm->nDivision));
	    return MCIERR_INVALID_FILE;
	}
	switch (LOBYTE(wmm->nDivision)) {
	case 4:	/* MIDI Time Code */
	case 8:
	case 10:
	case 80: /* SMPTE bit resolution */
	case 100:
	default:
	    WARN("Unsupported number of sub-frames %d\n", LOBYTE(wmm->nDivision));
	    return MCIERR_INVALID_FILE;
	}
    } else if (wmm->nDivision == 0) {
	WARN("Number of division is 0, can't support that !!\n");
	return MCIERR_INVALID_FILE;
    } else {
	wmm->dwMciTimeFormat = MCI_FORMAT_MILLISECONDS;
    }

    switch (wmm->wFormat) {
    case 0:
	if (wmm->nTracks != 1) {
	    WARN("Got type 0 file whose number of track is not 1. Setting it to 1\n");
	    wmm->nTracks = 1;
	}
	break;
    case 1:
    case 2:
	break;
    default:
	WARN("Handling MIDI files which format = %d is not (yet) supported\n"
	     "Please report with MIDI file !\n", wmm->wFormat);
	return MCIERR_INVALID_FILE;
    }

    if (wmm->nTracks & 0x8000) {
	/* this shouldn't be a problem... */
	WARN("Ouch !! Implementation limitation to 32k tracks per MIDI file is overflowed\n");
	wmm->nTracks = 0x7FFF;
    }

    if ((wmm->tracks = HeapAlloc(GetProcessHeap(), 0, sizeof(MCI_MIDITRACK) * wmm->nTracks)) == NULL) {
	return MCIERR_OUT_OF_MEMORY;
    }

    toberead -= 3 * sizeof(WORD);
    if (toberead > 0) {
	TRACE("Size of MThd > 6, skipping %ld extra bytes\n", toberead);
	mmioSeek(wmm->hFile, toberead, SEEK_CUR);
    }

    for (nt = 0; nt < wmm->nTracks; nt++) {
	wmm->tracks[nt].wTrackNr = nt;
	if (MIDI_mciReadMTrk(wmm, &wmm->tracks[nt]) != 0) {
	    WARN("Can't read 'MTrk' header \n");
	    return MCIERR_INVALID_FILE;
	}
    }

    wmm->dwTempo = 500000;

    return 0;
}

/**************************************************************************
 * 			MIDI_ConvertPulseToMS			[internal]
 */
static	DWORD	MIDI_ConvertPulseToMS(WINE_MCIMIDI* wmm, DWORD pulse)
{
    DWORD	ret = 0;

    /* FIXME: this function may return false values since the tempo (wmm->dwTempo)
     * may change during file playing
     */
    if (wmm->nDivision == 0) {
	FIXME("Shouldn't happen. wmm->nDivision = 0\n");
    } else if (wmm->nDivision > 0x8000) { /* SMPTE, unchecked FIXME? */
	int	nf = -(char)HIBYTE(wmm->nDivision);	/* number of frames     */
	int	nsf = LOBYTE(wmm->nDivision);		/* number of sub-frames */
	ret = (pulse * 1000) / (nf * nsf);
    } else {
	ret = (DWORD)((double)pulse * ((double)wmm->dwTempo / 1000) /
		      (double)wmm->nDivision);
    }

    /*
      TRACE("pulse=%lu tempo=%lu division=%u=0x%04x => ms=%lu\n",
      pulse, wmm->dwTempo, wmm->nDivision, wmm->nDivision, ret);
    */

    return ret;
}

#define TIME_MS_IN_ONE_HOUR	(60*60*1000)
#define TIME_MS_IN_ONE_MINUTE	(60*1000)
#define TIME_MS_IN_ONE_SECOND	(1000)

/**************************************************************************
 * 			MIDI_ConvertTimeFormatToMS		[internal]
 */
static	DWORD	MIDI_ConvertTimeFormatToMS(WINE_MCIMIDI* wmm, DWORD val)
{
    DWORD	ret = 0;

    switch (wmm->dwMciTimeFormat) {
    case MCI_FORMAT_MILLISECONDS:
	ret = val;
	break;
    case MCI_FORMAT_SMPTE_24:
	ret =
	    (HIBYTE(HIWORD(val)) * 125) / 3 +             LOBYTE(HIWORD(val)) * TIME_MS_IN_ONE_SECOND +
	    HIBYTE(LOWORD(val)) * TIME_MS_IN_ONE_MINUTE + LOBYTE(LOWORD(val)) * TIME_MS_IN_ONE_HOUR;
	break;
    case MCI_FORMAT_SMPTE_25:
	ret =
	    HIBYTE(HIWORD(val)) * 40 + 		  	  LOBYTE(HIWORD(val)) * TIME_MS_IN_ONE_SECOND +
	    HIBYTE(LOWORD(val)) * TIME_MS_IN_ONE_MINUTE + LOBYTE(LOWORD(val)) * TIME_MS_IN_ONE_HOUR;
	break;
    case MCI_FORMAT_SMPTE_30:
	ret =
	    (HIBYTE(HIWORD(val)) * 100) / 3 + 		  LOBYTE(HIWORD(val)) * TIME_MS_IN_ONE_SECOND +
	    HIBYTE(LOWORD(val)) * TIME_MS_IN_ONE_MINUTE + LOBYTE(LOWORD(val)) * TIME_MS_IN_ONE_HOUR;
	break;
    default:
	WARN("Bad time format %lu!\n", wmm->dwMciTimeFormat);
    }
    /*
      TRACE("val=%lu=0x%08lx [tf=%lu] => ret=%lu\n", val, val, wmm->dwMciTimeFormat, ret);
    */
    return ret;
}

/**************************************************************************
 * 			MIDI_ConvertMSToTimeFormat		[internal]
 */
static	DWORD	MIDI_ConvertMSToTimeFormat(WINE_MCIMIDI* wmm, DWORD _val)
{
    DWORD	ret = 0, val = _val;
    DWORD	h, m, s, f;

    switch (wmm->dwMciTimeFormat) {
    case MCI_FORMAT_MILLISECONDS:
	ret = val;
	break;
    case MCI_FORMAT_SMPTE_24:
    case MCI_FORMAT_SMPTE_25:
    case MCI_FORMAT_SMPTE_30:
	h = val / TIME_MS_IN_ONE_HOUR;
	m = (val -= h * TIME_MS_IN_ONE_HOUR)   / TIME_MS_IN_ONE_MINUTE;
	s = (val -= m * TIME_MS_IN_ONE_MINUTE) / TIME_MS_IN_ONE_SECOND;
	switch (wmm->dwMciTimeFormat) {
	case MCI_FORMAT_SMPTE_24:
	    /* one frame is 1000/24 val long, 1000/24 == 125/3 */
	    f = (val * 3) / 125; 	val -= (f * 125) / 3;
	    break;
	case MCI_FORMAT_SMPTE_25:
	    /* one frame is 1000/25 ms long, 1000/25 == 40 */
	    f = val / 40; 		val -= f * 40;
	    break;
	case MCI_FORMAT_SMPTE_30:
	    /* one frame is 1000/30 ms long, 1000/30 == 100/3 */
	    f = (val * 3) / 100; 	val -= (f * 100) / 3;
	    break;
	default:
	    FIXME("There must be some bad bad programmer\n");
	    f = 0;
	}
	/* val contains the number of ms which cannot make a complete frame */
	/* FIXME: is this correct ? programs seem to be happy with that */
	ret = (f << 24) | (s << 16) | (m << 8) | (h << 0);
	break;
    default:
	WARN("Bad time format %lu!\n", wmm->dwMciTimeFormat);
    }
    /*
      TRACE("val=%lu [tf=%lu] => ret=%lu=0x%08lx\n", _val, wmm->dwMciTimeFormat, ret, ret);
    */
    return ret;
}

/**************************************************************************
 * 			MIDI_GetMThdLengthMS			[internal]
 */
static	DWORD	MIDI_GetMThdLengthMS(WINE_MCIMIDI* wmm)
{
    WORD	nt;
    DWORD	ret = 0;

    for (nt = 0; nt < wmm->nTracks; nt++) {
	if (wmm->wFormat == 2) {
	    ret += wmm->tracks[nt].dwLength;
	} else if (wmm->tracks[nt].dwLength > ret) {
	    ret = wmm->tracks[nt].dwLength;
	}
    }
    /* FIXME: this is wrong if there is a tempo change inside the file */
    return MIDI_ConvertPulseToMS(wmm, ret);
}

/**************************************************************************
 * 				MIDI_mciOpen			[internal]
 */
static DWORD MIDI_mciOpen(UINT wDevID, DWORD dwFlags, LPMCI_OPEN_PARMSA lpParms)
{
    DWORD		dwRet = 0;
    DWORD		dwDeviceID;
    WINE_MCIMIDI*	wmm = (WINE_MCIMIDI*)mciGetDriverData(wDevID);

    TRACE("(%04x, %08lX, %p)\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) 	return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmm == NULL)		return MCIERR_INVALID_DEVICE_ID;
    if (dwFlags & MCI_OPEN_SHAREABLE)
	return MCIERR_HARDWARE;

    if (wmm->nUseCount > 0) {
	/* The driver is already opened on this channel
	 * MIDI sequencer cannot be shared
	 */
	return MCIERR_DEVICE_OPEN;
    }
    wmm->nUseCount++;

    wmm->hFile = 0;
    wmm->hMidi = 0;
    dwDeviceID = lpParms->wDeviceID;

    TRACE("wDevID=%04X (lpParams->wDeviceID=%08lX)\n", wDevID, dwDeviceID);
    /*	lpParms->wDeviceID = wDevID;*/

    if (dwFlags & MCI_OPEN_ELEMENT) {
	TRACE("MCI_OPEN_ELEMENT '%s' !\n", lpParms->lpstrElementName);
	if (lpParms->lpstrElementName && strlen(lpParms->lpstrElementName) > 0) {
	    wmm->hFile = mmioOpenA(lpParms->lpstrElementName, NULL,
				   MMIO_ALLOCBUF | MMIO_READ | MMIO_DENYWRITE);
	    if (wmm->hFile == 0) {
		WARN("Can't find file '%s' !\n", lpParms->lpstrElementName);
		wmm->nUseCount--;
		return MCIERR_FILE_NOT_FOUND;
	    }
	} else {
	    wmm->hFile = 0;
	}
    }
    TRACE("hFile=%u\n", wmm->hFile);

    /* FIXME: should I get a strdup() of it instead? */
    wmm->lpstrElementName = HeapAlloc( GetProcessHeap(), 0, strlen(lpParms->lpstrElementName)+1 );
    strcpy( wmm->lpstrElementName, lpParms->lpstrElementName );
    wmm->lpstrCopyright = NULL;
    wmm->lpstrName = NULL;

    wmm->wNotifyDeviceID = dwDeviceID;
    wmm->dwStatus = MCI_MODE_NOT_READY;	/* while loading file contents */
    /* spec says it should be the default format from the MIDI file... */
    wmm->dwMciTimeFormat = MCI_FORMAT_MILLISECONDS;

    if (wmm->hFile != 0) {
	MMCKINFO	ckMainRIFF;
	MMCKINFO	mmckInfo;
	DWORD		dwOffset = 0;

	if (mmioDescend(wmm->hFile, &ckMainRIFF, NULL, 0) != 0) {
	    dwRet = MCIERR_INVALID_FILE;
	} else {
	    TRACE("ParentChunk ckid=%.4s fccType=%.4s cksize=%08lX \n",
		  (LPSTR)&ckMainRIFF.ckid, (LPSTR)&ckMainRIFF.fccType, ckMainRIFF.cksize);

	    if (ckMainRIFF.ckid == FOURCC_RIFF && ckMainRIFF.fccType == mmioFOURCC('R', 'M', 'I', 'D')) {
		mmckInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioSeek(wmm->hFile, ckMainRIFF.dwDataOffset + ((ckMainRIFF.cksize + 1) & ~1), SEEK_SET);
		if (mmioDescend(wmm->hFile, &mmckInfo, &ckMainRIFF, MMIO_FINDCHUNK) == 0) {
		    TRACE("... is a 'RMID' file \n");
		    dwOffset = mmckInfo.dwDataOffset;
		} else {
		    dwRet = MCIERR_INVALID_FILE;
		}
	    }
	    if (dwRet == 0 && MIDI_mciReadMThd(wmm, dwOffset) != 0) {
		WARN("Can't read 'MThd' header \n");
		dwRet = MCIERR_INVALID_FILE;
	    }
	}
    } else {
	TRACE("hFile==0, setting #tracks to 0; is this correct ?\n");
	wmm->nTracks = 0;
	wmm->wFormat = 0;
	wmm->nDivision = 1;
    }
    if (dwRet != 0) {
	wmm->nUseCount--;
	if (wmm->hFile != 0)
	    mmioClose(wmm->hFile, 0);
	wmm->hFile = 0;
    } else {
	wmm->dwPositionMS = 0;
	wmm->dwStatus = MCI_MODE_STOP;
    }
    return dwRet;
}

/**************************************************************************
 * 				MIDI_mciStop			[internal]
 */
static DWORD MIDI_mciStop(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);
    DWORD		dwRet = 0;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmm == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (wmm->dwStatus != MCI_MODE_STOP) {
	int	oldstat = wmm->dwStatus;

	wmm->dwStatus = MCI_MODE_NOT_READY;
	if (oldstat == MCI_MODE_PAUSE)
	    dwRet = midiOutReset((HMIDIOUT)wmm->hMidi);

	while (wmm->dwStatus != MCI_MODE_STOP)
	    Sleep(10);
    }

    /* sanitiy reset */
    wmm->dwStatus = MCI_MODE_STOP;

    TRACE("wmm->dwStatus=%d\n", wmm->dwStatus);

    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MIDI_mciClose			[internal]
 */
static DWORD MIDI_mciClose(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmm == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (wmm->dwStatus != MCI_MODE_STOP) {
	MIDI_mciStop(wDevID, MCI_WAIT, lpParms);
    }

    wmm->nUseCount--;
    if (wmm->nUseCount == 0) {
	if (wmm->hFile != 0) {
	    mmioClose(wmm->hFile, 0);
	    wmm->hFile = 0;
	    TRACE("hFile closed !\n");
	}
	HeapFree(GetProcessHeap(), 0, wmm->tracks);
	HeapFree(GetProcessHeap(), 0, (LPSTR)wmm->lpstrElementName);
	HeapFree(GetProcessHeap(), 0, (LPSTR)wmm->lpstrCopyright);
	HeapFree(GetProcessHeap(), 0, (LPSTR)wmm->lpstrName);
    } else {
	TRACE("Shouldn't happen... nUseCount=%d\n", wmm->nUseCount);
	return MCIERR_INTERNAL;
    }

    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MIDI_mciFindNextEvent		[internal]
 */
static MCI_MIDITRACK*	MIDI_mciFindNextEvent(WINE_MCIMIDI* wmm, LPDWORD hiPulse)
{
    WORD		cnt, nt;
    MCI_MIDITRACK*	mmt;

    *hiPulse = 0xFFFFFFFFul;
    cnt = 0xFFFFu;
    for (nt = 0; nt < wmm->nTracks; nt++) {
	mmt = &wmm->tracks[nt];

	if (mmt->wStatus == 0)
	    continue;
	if (mmt->dwEventPulse < *hiPulse) {
	    *hiPulse = mmt->dwEventPulse;
	    cnt = nt;
	}
    }
    return (cnt == 0xFFFFu) ? 0 /* no more event on all tracks */
	: &wmm->tracks[cnt];
}

/**************************************************************************
 * 				MIDI_mciPlay			[internal]
 */
static DWORD MIDI_mciPlay(UINT wDevID, DWORD dwFlags, LPMCI_PLAY_PARMS lpParms)
{
    DWORD		dwStartMS, dwEndMS;
    DWORD		dwRet = 0;
    WORD		doPlay, nt;
    MCI_MIDITRACK*	mmt;
    DWORD		hiPulse;
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmm == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (wmm->hFile == 0) {
	WARN("Can't play: no file '%s' !\n", wmm->lpstrElementName);
	return MCIERR_FILE_NOT_FOUND;
    }

    if (wmm->dwStatus != MCI_MODE_STOP) {
	if (wmm->dwStatus == MCI_MODE_PAUSE) {
	    /* FIXME: parameters (start/end) in lpParams may not be used */
	    return MIDI_mciResume(wDevID, dwFlags, (LPMCI_GENERIC_PARMS)lpParms);
	}
	WARN("Can't play: device is not stopped !\n");
	return MCIERR_INTERNAL;
    }

    if (!(dwFlags & MCI_WAIT)) {
	/** FIXME: I'm not 100% sure that wNotifyDeviceID is the right value in all cases ??? */
	return MCI_SendCommandAsync(wmm->wNotifyDeviceID, MCI_PLAY, dwFlags, (DWORD)lpParms, sizeof(LPMCI_PLAY_PARMS));
    }

    if (lpParms && (dwFlags & MCI_FROM)) {
	dwStartMS = MIDI_ConvertTimeFormatToMS(wmm, lpParms->dwFrom);
    } else {
	dwStartMS = wmm->dwPositionMS;
    }

    if (lpParms && (dwFlags & MCI_TO)) {
	dwEndMS = MIDI_ConvertTimeFormatToMS(wmm, lpParms->dwTo);
    } else {
	dwEndMS = 0xFFFFFFFFul;
    }

    TRACE("Playing from %lu to %lu\n", dwStartMS, dwEndMS);

    /* init tracks */
    for (nt = 0; nt < wmm->nTracks; nt++) {
	mmt = &wmm->tracks[nt];

	mmt->wStatus = 1;	/* ok, playing */
	mmt->dwIndex = mmt->dwFirst;
	if (wmm->wFormat == 2 && nt > 0) {
	    mmt->dwEventPulse = wmm->tracks[nt - 1].dwLength;
	} else {
	    mmt->dwEventPulse = 0;
	}
	MIDI_mciReadNextEvent(wmm, mmt); /* FIXME == 0 */
    }

    dwRet = midiOutOpen((LPHMIDIOUT)&wmm->hMidi, MIDIMAPPER, 0L, 0L, CALLBACK_NULL);
    /*	dwRet = midiInOpen(&wmm->hMidi, MIDIMAPPER, 0L, 0L, CALLBACK_NULL);*/
    if (dwRet != MMSYSERR_NOERROR) {
	return dwRet;
    }

    wmm->dwPulse = 0;
    wmm->dwTempo = 500000;
    wmm->dwStatus = MCI_MODE_PLAY;
    wmm->dwPositionMS = 0;
    wmm->wStartedPlaying = FALSE;

    while (wmm->dwStatus != MCI_MODE_STOP && wmm->dwStatus != MCI_MODE_NOT_READY) {
	/* it seems that in case of multi-threading, gcc is optimizing just a little bit
	 * too much. Tell gcc not to optimize status value using volatile.
	 */
	while (((volatile WINE_MCIMIDI*)wmm)->dwStatus == MCI_MODE_PAUSE);

	doPlay = (wmm->dwPositionMS >= dwStartMS && wmm->dwPositionMS <= dwEndMS);

	TRACE("wmm->dwStatus=%d, doPlay=%c\n", wmm->dwStatus, doPlay ? 'T' : 'F');

	if ((mmt = MIDI_mciFindNextEvent(wmm, &hiPulse)) == NULL)
	    break;  /* no more event on tracks */

	/* if starting playing, then set StartTicks to the value it would have had
	 * if play had started at position 0
	 */
	if (doPlay && !wmm->wStartedPlaying) {
	    wmm->dwStartTicks = GetTickCount() - MIDI_ConvertPulseToMS(wmm, wmm->dwPulse);
	    wmm->wStartedPlaying = TRUE;
	    TRACE("Setting dwStartTicks to %lu\n", wmm->dwStartTicks);
	}

	if (hiPulse > wmm->dwPulse) {
	    wmm->dwPositionMS += MIDI_ConvertPulseToMS(wmm, hiPulse - wmm->dwPulse);
	    if (doPlay) {
		DWORD	togo = wmm->dwStartTicks + wmm->dwPositionMS;
		DWORD	tc = GetTickCount();

		TRACE("Pulses hi=0x%08lx <> cur=0x%08lx\n", hiPulse, wmm->dwPulse);
		TRACE("Wait until %lu => %lu ms\n",
		      tc - wmm->dwStartTicks, togo - wmm->dwStartTicks);
		if (tc < togo)
		    Sleep(togo - tc);
	    }
	    wmm->dwPulse = hiPulse;
	}

	switch (LOBYTE(LOWORD(mmt->dwEventData))) {
	case 0xF0:
	case 0xF7:	/* sysex events */
	    {
		FIXME("Not handling SysEx events (yet)\n");
	    }
	    break;
	case 0xFF:
	    /* position after meta data header */
	    mmioSeek(wmm->hFile, mmt->dwIndex + HIWORD(mmt->dwEventData), SEEK_SET);
	    switch (HIBYTE(LOWORD(mmt->dwEventData))) {
	    case 0x00: /* 16-bit sequence number */
		if (TRACE_ON(mcimidi)) {
		    WORD	twd;

		    MIDI_mciReadWord(wmm, &twd);	/* == 0 */
		    TRACE("Got sequence number %u\n", twd);
		}
		break;
	    case 0x01: /* any text */
	    case 0x02: /* Copyright Message text */
	    case 0x03: /* Sequence/Track Name text */
	    case 0x04: /* Instrument Name text */
	    case 0x05: /* Lyric text */
	    case 0x06: /* Marker text */
	    case 0x07: /* Cue-point text */
		if (TRACE_ON(mcimidi)) {
		    char	buf[1024];
		    WORD	len = mmt->wEventLength - HIWORD(mmt->dwEventData);
		    static	char*	info[8] = {"", "Text", "Copyright", "Seq/Trk name",
						   "Instrument", "Lyric", "Marker", "Cue-point"};
		    WORD	idx = HIBYTE(LOWORD(mmt->dwEventData));

		    if (len >= sizeof(buf)) {
			WARN("Buffer for text is too small (%d bytes, when %u are needed)\n", sizeof(buf) - 1, len);
			len = sizeof(buf) - 1;
		    }
		    if (mmioRead(wmm->hFile, (HPSTR)buf, len) == len) {
			buf[len] = 0;	/* end string in case */
			TRACE("%s => \"%s\"\n", (idx < 8 ) ? info[idx] : "", buf);
		    } else {
			WARN("Couldn't read data for %s\n", (idx < 8) ? info[idx] : "");
		    }
		}
		break;
	    case 0x20:
		/* MIDI channel (cc) */
		if (FIXME_ON(mcimidi)) {
		    BYTE	bt;

		    MIDI_mciReadByte(wmm, &bt);	/* == 0 */
		    FIXME("NIY: MIDI channel=%u, track=%u\n", bt, mmt->wTrackNr);
		}
		break;
	    case 0x21:
		/* MIDI port (pp) */
		if (FIXME_ON(mcimidi)) {
		    BYTE	bt;

		    MIDI_mciReadByte(wmm, &bt);	/* == 0 */
		    FIXME("NIY: MIDI port=%u, track=%u\n", bt, mmt->wTrackNr);
		}
		break;
	    case 0x2F: /* end of track */
		mmt->wStatus = 0;
		break;
	    case 0x51:/* set tempo */
		/* Tempo is expressed in �-seconds per midi quarter note
		 * for format 1 MIDI files, this can only be present on track #0
		 */
		if (mmt->wTrackNr != 0 && wmm->wFormat == 1) {
		    WARN("For format #1 MIDI files, tempo can only be changed on track #0 (%u)\n", mmt->wTrackNr);
		} else {
		    BYTE	tbt;
		    DWORD	value = 0;

		    MIDI_mciReadByte(wmm, &tbt);	value  = ((DWORD)tbt) << 16;
		    MIDI_mciReadByte(wmm, &tbt);	value |= ((DWORD)tbt) << 8;
		    MIDI_mciReadByte(wmm, &tbt);	value |= ((DWORD)tbt) << 0;
		    TRACE("Setting tempo to %ld (BPM=%ld)\n", wmm->dwTempo, (value) ? (60000000l / value) : 0);
		    wmm->dwTempo = value;
		}
		break;
	    case 0x54: /* (hour) (min) (second) (frame) (fractional-frame) - SMPTE track start */
		if (mmt->wTrackNr != 0 && wmm->wFormat == 1) {
		    WARN("For format #1 MIDI files, SMPTE track start can only be expressed on track #0 (%u)\n", mmt->wTrackNr);
		} if (mmt->dwEventPulse != 0) {
		    WARN("SMPTE track start can only be expressed at start of track (%lu)\n", mmt->dwEventPulse);
		} else {
		    BYTE	h, m, s, f, ff;

		    MIDI_mciReadByte(wmm, &h);
		    MIDI_mciReadByte(wmm, &m);
		    MIDI_mciReadByte(wmm, &s);
		    MIDI_mciReadByte(wmm, &f);
		    MIDI_mciReadByte(wmm, &ff);
		    FIXME("NIY: SMPTE track start %u:%u:%u %u.%u\n", h, m, s, f, ff);
		}
		break;
	    case 0x58: /* file rythm */
		if (TRACE_ON(mcimidi)) {
		    BYTE	num, den, cpmc, _32npqn;

		    MIDI_mciReadByte(wmm, &num);
		    MIDI_mciReadByte(wmm, &den);		/* to notate e.g. 6/8 */
		    MIDI_mciReadByte(wmm, &cpmc);		/* number of MIDI clocks per metronome click */
		    MIDI_mciReadByte(wmm, &_32npqn);		/* number of notated 32nd notes per MIDI quarter note */

		    TRACE("%u/%u, clock per metronome click=%u, 32nd notes by 1/4 note=%u\n", num, 1 << den, cpmc, _32npqn);
		}
		break;
	    case 0x59: /* key signature */
		if (TRACE_ON(mcimidi)) {
		    BYTE	sf, mm;

		    MIDI_mciReadByte(wmm, &sf);
		    MIDI_mciReadByte(wmm, &mm);

		    if (sf >= 0x80) 	TRACE("%d flats\n", -(char)sf);
		    else if (sf > 0) 	TRACE("%d sharps\n", (char)sf);
		    else 		TRACE("Key of C\n");
		    TRACE("Mode: %s\n", (mm = 0) ? "major" : "minor");
		}
		break;
	    default:
		WARN("Unknown MIDI meta event %02x. Skipping...\n", HIBYTE(LOWORD(mmt->dwEventData)));
		break;
	    }
	    break;
	default:
	    if (doPlay) {
		dwRet = midiOutShortMsg((HMIDIOUT)wmm->hMidi, mmt->dwEventData);
	    } else {
		switch (LOBYTE(LOWORD(mmt->dwEventData)) & 0xF0) {
		case MIDI_NOTEON:
		case MIDI_NOTEOFF:
		    dwRet = 0;
		    break;
		default:
		    dwRet = midiOutShortMsg((HMIDIOUT)wmm->hMidi, mmt->dwEventData);
		}
	    }
	}
	mmt->dwIndex += mmt->wEventLength;
	if (mmt->dwIndex < mmt->dwFirst || mmt->dwIndex >= mmt->dwLast) {
	    mmt->wStatus = 0;
	}
	if (mmt->wStatus) {
	    MIDI_mciReadNextEvent(wmm, mmt);
	}
    }

    midiOutReset((HMIDIOUT)wmm->hMidi);

    dwRet = midiOutClose((HMIDIOUT)wmm->hMidi);
    /* to restart playing at beginning when it's over */
    wmm->dwPositionMS = 0;

    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }

    wmm->dwStatus = MCI_MODE_STOP;
    return dwRet;
}

/**************************************************************************
 * 				MIDI_mciRecord			[internal]
 */
static DWORD MIDI_mciRecord(UINT wDevID, DWORD dwFlags, LPMCI_RECORD_PARMS lpParms)
{
    int			start, end;
    MIDIHDR		midiHdr;
    DWORD		dwRet;
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmm == 0)	return MCIERR_INVALID_DEVICE_ID;

    if (wmm->hFile == 0) {
	WARN("Can't find file='%s' !\n", wmm->lpstrElementName);
	return MCIERR_FILE_NOT_FOUND;
    }
    start = 1; 		end = 99999;
    if (lpParms && (dwFlags & MCI_FROM)) {
	start = lpParms->dwFrom;
	TRACE("MCI_FROM=%d \n", start);
    }
    if (lpParms && (dwFlags & MCI_TO)) {
	end = lpParms->dwTo;
	TRACE("MCI_TO=%d \n", end);
    }
    midiHdr.lpData = (LPSTR) HeapAlloc(GetProcessHeap(), 0, 1200);
    if (!midiHdr.lpData)
	return MCIERR_OUT_OF_MEMORY;
    midiHdr.dwBufferLength = 1024;
    midiHdr.dwUser = 0L;
    midiHdr.dwFlags = 0L;
    dwRet = midiInPrepareHeader((HMIDIIN)wmm->hMidi, &midiHdr, sizeof(MIDIHDR));
    TRACE("After MIDM_PREPARE \n");
    wmm->dwStatus = MCI_MODE_RECORD;
    /* FIXME: there is no buffer added */
    while (wmm->dwStatus != MCI_MODE_STOP) {
	TRACE("wmm->dwStatus=%p %d\n",
	      &wmm->dwStatus, wmm->dwStatus);
	midiHdr.dwBytesRecorded = 0;
	dwRet = midiInStart((HMIDIIN)wmm->hMidi);
	TRACE("midiInStart => dwBytesRecorded=%lu\n", midiHdr.dwBytesRecorded);
	if (midiHdr.dwBytesRecorded == 0) break;
    }
    TRACE("Before MIDM_UNPREPARE \n");
    dwRet = midiInUnprepareHeader((HMIDIIN)wmm->hMidi, &midiHdr, sizeof(MIDIHDR));
    TRACE("After MIDM_UNPREPARE \n");
    if (midiHdr.lpData != NULL) {
	HeapFree(GetProcessHeap(), 0, midiHdr.lpData);
	midiHdr.lpData = NULL;
    }
    wmm->dwStatus = MCI_MODE_STOP;
    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MIDI_mciPause			[internal]
 */
static DWORD MIDI_mciPause(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmm == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (wmm->dwStatus == MCI_MODE_PLAY) {
	/* stop all notes */
	unsigned chn;
	for (chn = 0; chn < 16; chn++)
	    midiOutShortMsg((HMIDIOUT)(wmm->hMidi), 0x78B0 | chn);
	wmm->dwStatus = MCI_MODE_PAUSE;
    }
    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }

    return 0;
}

/**************************************************************************
 * 				MIDI_mciResume			[internal]
 */
static DWORD MIDI_mciResume(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmm == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (wmm->dwStatus == MCI_MODE_PAUSE) {
	wmm->wStartedPlaying = FALSE;
	wmm->dwStatus = MCI_MODE_PLAY;
    }
    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MIDI_mciSet			[internal]
 */
static DWORD MIDI_mciSet(UINT wDevID, DWORD dwFlags, LPMCI_SET_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL)	return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmm == NULL)		return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_SET_TIME_FORMAT) {
	switch (lpParms->dwTimeFormat) {
	case MCI_FORMAT_MILLISECONDS:
	    TRACE("MCI_FORMAT_MILLISECONDS !\n");
	    wmm->dwMciTimeFormat = MCI_FORMAT_MILLISECONDS;
	    break;
	case MCI_FORMAT_SMPTE_24:
	    TRACE("MCI_FORMAT_SMPTE_24 !\n");
	    wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_24;
	    break;
	case MCI_FORMAT_SMPTE_25:
	    TRACE("MCI_FORMAT_SMPTE_25 !\n");
	    wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_25;
	    break;
	case MCI_FORMAT_SMPTE_30:
	    TRACE("MCI_FORMAT_SMPTE_30 !\n");
	    wmm->dwMciTimeFormat = MCI_FORMAT_SMPTE_30;
	    break;
	default:
	    WARN("Bad time format %lu!\n", lpParms->dwTimeFormat);
	    return MCIERR_BAD_TIME_FORMAT;
	}
    }
    if (dwFlags & MCI_SET_VIDEO) {
	TRACE("No support for video !\n");
	return MCIERR_UNSUPPORTED_FUNCTION;
    }
    if (dwFlags & MCI_SET_DOOR_OPEN) {
	TRACE("No support for door open !\n");
	return MCIERR_UNSUPPORTED_FUNCTION;
    }
    if (dwFlags & MCI_SET_DOOR_CLOSED) {
	TRACE("No support for door close !\n");
	return MCIERR_UNSUPPORTED_FUNCTION;
    }
    if (dwFlags & MCI_SET_AUDIO) {
	if (dwFlags & MCI_SET_ON) {
	    TRACE("MCI_SET_ON audio !\n");
	} else if (dwFlags & MCI_SET_OFF) {
	    TRACE("MCI_SET_OFF audio !\n");
	} else {
	    WARN("MCI_SET_AUDIO without SET_ON or SET_OFF\n");
	    return MCIERR_BAD_INTEGER;
	}

	if (lpParms->dwAudio & MCI_SET_AUDIO_ALL)
	    TRACE("MCI_SET_AUDIO_ALL !\n");
	if (lpParms->dwAudio & MCI_SET_AUDIO_LEFT)
	    TRACE("MCI_SET_AUDIO_LEFT !\n");
	if (lpParms->dwAudio & MCI_SET_AUDIO_RIGHT)
	    TRACE("MCI_SET_AUDIO_RIGHT !\n");
    }

    if (dwFlags & MCI_SEQ_SET_MASTER)
	TRACE("MCI_SEQ_SET_MASTER !\n");
    if (dwFlags & MCI_SEQ_SET_SLAVE)
	TRACE("MCI_SEQ_SET_SLAVE !\n");
    if (dwFlags & MCI_SEQ_SET_OFFSET)
	TRACE("MCI_SEQ_SET_OFFSET !\n");
    if (dwFlags & MCI_SEQ_SET_PORT)
	TRACE("MCI_SEQ_SET_PORT !\n");
    if (dwFlags & MCI_SEQ_SET_TEMPO)
	TRACE("MCI_SEQ_SET_TEMPO !\n");
    return 0;
}

/**************************************************************************
 * 				MIDI_mciStatus			[internal]
 */
static DWORD MIDI_mciStatus(UINT wDevID, DWORD dwFlags, LPMCI_STATUS_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);
    DWORD		ret = 0;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) 	return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmm == NULL)		return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_STATUS_ITEM) {
	switch (lpParms->dwItem) {
	case MCI_STATUS_CURRENT_TRACK:
	    /* FIXME in Format 2 */
	    lpParms->dwReturn = 1;
	    TRACE("MCI_STATUS_CURRENT_TRACK => %lu\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_LENGTH:
	    if ((dwFlags & MCI_TRACK) && wmm->wFormat == 2) {
		if (lpParms->dwTrack >= wmm->nTracks)
		    return MCIERR_BAD_INTEGER;
		/* FIXME: this is wrong if there is a tempo change inside the file */
		lpParms->dwReturn = MIDI_ConvertPulseToMS(wmm, wmm->tracks[lpParms->dwTrack].dwLength);
	    } else {
		lpParms->dwReturn = MIDI_GetMThdLengthMS(wmm);
	    }
	    lpParms->dwReturn = MIDI_ConvertMSToTimeFormat(wmm, lpParms->dwReturn);
	    TRACE("MCI_STATUS_LENGTH => %lu\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_MODE:
	    TRACE("MCI_STATUS_MODE => %u\n", wmm->dwStatus);
 	    lpParms->dwReturn = MAKEMCIRESOURCE(wmm->dwStatus, wmm->dwStatus);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_STATUS_MEDIA_PRESENT:
	    TRACE("MCI_STATUS_MEDIA_PRESENT => TRUE\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_STATUS_NUMBER_OF_TRACKS:
	    lpParms->dwReturn = (wmm->wFormat == 2) ? wmm->nTracks : 1;
	    TRACE("MCI_STATUS_NUMBER_OF_TRACKS => %lu\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_POSITION:
	    /* FIXME: do I need to use MCI_TRACK ? */
	    lpParms->dwReturn = MIDI_ConvertMSToTimeFormat(wmm,
							   (dwFlags & MCI_STATUS_START) ? 0 : wmm->dwPositionMS);
	    TRACE("MCI_STATUS_POSITION %s => %lu\n",
		  (dwFlags & MCI_STATUS_START) ? "start" : "current", lpParms->dwReturn);
	    break;
	case MCI_STATUS_READY:
	    lpParms->dwReturn = (wmm->dwStatus == MCI_MODE_NOT_READY) ?
		MAKEMCIRESOURCE(FALSE, MCI_FALSE) : MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    TRACE("MCI_STATUS_READY = %u\n", LOWORD(lpParms->dwReturn));
	    break;
	case MCI_STATUS_TIME_FORMAT:
	    lpParms->dwReturn = MAKEMCIRESOURCE(wmm->dwMciTimeFormat, wmm->dwMciTimeFormat);
	    TRACE("MCI_STATUS_TIME_FORMAT => %u\n", LOWORD(lpParms->dwReturn));
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_SEQ_STATUS_DIVTYPE:
	    TRACE("MCI_SEQ_STATUS_DIVTYPE !\n");
	    if (wmm->nDivision > 0x8000) {
		switch (wmm->nDivision) {
		case 0xE8:	lpParms->dwReturn = MCI_SEQ_DIV_SMPTE_24;	break;	/* -24 */
		case 0xE7:	lpParms->dwReturn = MCI_SEQ_DIV_SMPTE_25;	break;	/* -25 */
		case 0xE3:	lpParms->dwReturn = MCI_SEQ_DIV_SMPTE_30DROP;	break;	/* -29 */ /* is the MCI constant correct ? */
		case 0xE2:	lpParms->dwReturn = MCI_SEQ_DIV_SMPTE_30;	break;	/* -30 */
		default:	FIXME("There is a bad bad programmer\n");
		}
	    } else {
		lpParms->dwReturn = MCI_SEQ_DIV_PPQN;
	    }
	    lpParms->dwReturn = MAKEMCIRESOURCE(lpParms->dwReturn,lpParms->dwReturn);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_SEQ_STATUS_MASTER:
	    TRACE("MCI_SEQ_STATUS_MASTER !\n");
	    lpParms->dwReturn = 0;
	    break;
	case MCI_SEQ_STATUS_SLAVE:
	    TRACE("MCI_SEQ_STATUS_SLAVE !\n");
	    lpParms->dwReturn = 0;
	    break;
	case MCI_SEQ_STATUS_OFFSET:
	    TRACE("MCI_SEQ_STATUS_OFFSET !\n");
	    lpParms->dwReturn = 0;
	    break;
	case MCI_SEQ_STATUS_PORT:
	    TRACE("MCI_SEQ_STATUS_PORT (%u)!\n", wmm->wDevID);
	    lpParms->dwReturn = MIDI_MAPPER;
	    break;
	case MCI_SEQ_STATUS_TEMPO:
	    TRACE("MCI_SEQ_STATUS_TEMPO !\n");
	    lpParms->dwReturn = wmm->dwTempo;
	    break;
	default:
	    FIXME("Unknowm command %08lX !\n", lpParms->dwItem);
	    return MCIERR_UNRECOGNIZED_COMMAND;
	}
    } else {
	WARN("No Status-Item!\n");
	return MCIERR_UNRECOGNIZED_COMMAND;
    }
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return ret;
}

/**************************************************************************
 * 				MIDI_mciGetDevCaps		[internal]
 */
static DWORD MIDI_mciGetDevCaps(UINT wDevID, DWORD dwFlags,
				LPMCI_GETDEVCAPS_PARMS lpParms)
{
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);
    DWORD		ret;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) 	return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmm == NULL)		return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_GETDEVCAPS_ITEM) {
	switch (lpParms->dwItem) {
	case MCI_GETDEVCAPS_DEVICE_TYPE:
	    TRACE("MCI_GETDEVCAPS_DEVICE_TYPE !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(MCI_DEVTYPE_SEQUENCER, MCI_DEVTYPE_SEQUENCER);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_HAS_AUDIO:
	    TRACE("MCI_GETDEVCAPS_HAS_AUDIO !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_HAS_VIDEO:
	    TRACE("MCI_GETDEVCAPS_HAS_VIDEO !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_USES_FILES:
	    TRACE("MCI_GETDEVCAPS_USES_FILES !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_COMPOUND_DEVICE:
	    TRACE("MCI_GETDEVCAPS_COMPOUND_DEVICE !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_EJECT:
	    TRACE("MCI_GETDEVCAPS_CAN_EJECT !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_PLAY:
	    TRACE("MCI_GETDEVCAPS_CAN_PLAY !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_RECORD:
	    TRACE("MCI_GETDEVCAPS_CAN_RECORD !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_SAVE:
	    TRACE("MCI_GETDEVCAPS_CAN_SAVE !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	default:
	    FIXME("Unknown capability (%08lx) !\n", lpParms->dwItem);
	    return MCIERR_UNRECOGNIZED_COMMAND;
	}
    } else {
	WARN("No GetDevCaps-Item !\n");
	return MCIERR_UNRECOGNIZED_COMMAND;
    }
    return ret;
}

/**************************************************************************
 * 				MIDI_mciInfo			[internal]
 */
static DWORD MIDI_mciInfo(UINT wDevID, DWORD dwFlags, LPMCI_INFO_PARMSA lpParms)
{
    LPCSTR		str = 0;
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);
    DWORD		ret = 0;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL || lpParms->lpstrReturn == NULL)
	return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmm == NULL) return MCIERR_INVALID_DEVICE_ID;

    TRACE("buf=%p, len=%lu\n", lpParms->lpstrReturn, lpParms->dwRetSize);

    switch (dwFlags & ~(MCI_WAIT|MCI_NOTIFY)) {
    case MCI_INFO_PRODUCT:
	str = "Wine's MIDI sequencer";
	break;
    case MCI_INFO_FILE:
	str = wmm->lpstrElementName;
	break;
    case MCI_INFO_COPYRIGHT:
	str = wmm->lpstrCopyright;
	break;
    case MCI_INFO_NAME:
	str = wmm->lpstrName;
	break;
    default:
	WARN("Don't know this info command (%lu)\n", dwFlags);
	return MCIERR_UNRECOGNIZED_COMMAND;
    }
    if (str) {
	if (lpParms->dwRetSize <= strlen(str)) {
	    lstrcpynA(lpParms->lpstrReturn, str, lpParms->dwRetSize - 1);
	    ret = MCIERR_PARAM_OVERFLOW;
	} else {
	    strcpy(lpParms->lpstrReturn, str);
	}
    } else {
	*lpParms->lpstrReturn = 0;
    }
    return ret;
}

/**************************************************************************
 * 				MIDI_mciSeek			[internal]
 */
static DWORD MIDI_mciSeek(UINT wDevID, DWORD dwFlags, LPMCI_SEEK_PARMS lpParms)
{
    DWORD		ret = 0;
    WINE_MCIMIDI*	wmm = MIDI_mciGetOpenDev(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) {
	ret = MCIERR_NULL_PARAMETER_BLOCK;
    } else if (wmm == NULL) {
	ret = MCIERR_INVALID_DEVICE_ID;
    } else {
	MIDI_mciStop(wDevID, MCI_WAIT, 0);

	if (dwFlags & MCI_SEEK_TO_START) {
	    wmm->dwPositionMS = 0;
	} else if (dwFlags & MCI_SEEK_TO_END) {
	    wmm->dwPositionMS = 0xFFFFFFFF; /* FIXME */
	} else if (dwFlags & MCI_TO) {
	    wmm->dwPositionMS = MIDI_ConvertTimeFormatToMS(wmm, lpParms->dwTo);
	} else {
	    WARN("dwFlag doesn't tell where to seek to...\n");
	    return MCIERR_MISSING_PARAMETER;
	}

	TRACE("Seeking to position=%lu ms\n", wmm->dwPositionMS);

	if (dwFlags & MCI_NOTIFY) {
	    TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	    mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			    wmm->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
	}
    }
    return ret;
}

/*======================================================================*
 *                  	    MIDI entry points 				*
 *======================================================================*/

/**************************************************************************
 * 				DriverProc (MCISEQ.@)
 */
LONG CALLBACK	MCIMIDI_DriverProc(DWORD dwDevID, HDRVR hDriv, DWORD wMsg,
				   DWORD dwParam1, DWORD dwParam2)
{
    switch (wMsg) {
    case DRV_LOAD:		return 1;
    case DRV_FREE:		return 1;
    case DRV_ENABLE:		return 1;
    case DRV_DISABLE:		return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "Sample Midi Driver !", "OSS Driver", MB_OK); return 1;
    case DRV_INSTALL:		return DRVCNF_RESTART;
    case DRV_REMOVE:		return DRVCNF_RESTART;
    case DRV_OPEN:		return MIDI_drvOpen((LPSTR)dwParam1, (LPMCI_OPEN_DRIVER_PARMSA)dwParam2);
    case DRV_CLOSE:		return MIDI_drvClose(dwDevID);
    }

    if (dwDevID == 0xFFFFFFFF) return MCIERR_UNSUPPORTED_FUNCTION;

    switch (wMsg) {
    case MCI_OPEN_DRIVER:	return MIDI_mciOpen      (dwDevID, dwParam1, (LPMCI_OPEN_PARMSA)     dwParam2);
    case MCI_CLOSE_DRIVER:	return MIDI_mciClose     (dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)   dwParam2);
    case MCI_PLAY:		return MIDI_mciPlay      (dwDevID, dwParam1, (LPMCI_PLAY_PARMS)      dwParam2);
    case MCI_RECORD:		return MIDI_mciRecord    (dwDevID, dwParam1, (LPMCI_RECORD_PARMS)    dwParam2);
    case MCI_STOP:		return MIDI_mciStop      (dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)   dwParam2);
    case MCI_SET:		return MIDI_mciSet       (dwDevID, dwParam1, (LPMCI_SET_PARMS)       dwParam2);
    case MCI_PAUSE:		return MIDI_mciPause     (dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)   dwParam2);
    case MCI_RESUME:		return MIDI_mciResume    (dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)   dwParam2);
    case MCI_STATUS:		return MIDI_mciStatus    (dwDevID, dwParam1, (LPMCI_STATUS_PARMS)    dwParam2);
    case MCI_GETDEVCAPS:	return MIDI_mciGetDevCaps(dwDevID, dwParam1, (LPMCI_GETDEVCAPS_PARMS)dwParam2);
    case MCI_INFO:		return MIDI_mciInfo      (dwDevID, dwParam1, (LPMCI_INFO_PARMSA)     dwParam2);
    case MCI_SEEK:		return MIDI_mciSeek      (dwDevID, dwParam1, (LPMCI_SEEK_PARMS)      dwParam2);
    /* commands that should be supported */
    case MCI_LOAD:
    case MCI_SAVE:
    case MCI_FREEZE:
    case MCI_PUT:
    case MCI_REALIZE:
    case MCI_UNFREEZE:
    case MCI_UPDATE:
    case MCI_WHERE:
    case MCI_STEP:
    case MCI_SPIN:
    case MCI_ESCAPE:
    case MCI_COPY:
    case MCI_CUT:
    case MCI_DELETE:
    case MCI_PASTE:
	WARN("Unsupported command [%lu]\n", wMsg);
	break;
    /* commands that should report an error */
    case MCI_WINDOW:
	TRACE("Unsupported command [%lu]\n", wMsg);
	break;
    case MCI_OPEN:
    case MCI_CLOSE:
	FIXME("Shouldn't receive a MCI_OPEN or CLOSE message\n");
	break;
    default:
	TRACE("Sending msg [%lu] to default driver proc\n", wMsg);
	return DefDriverProc(dwDevID, hDriv, wMsg, dwParam1, dwParam2);
    }
    return MCIERR_UNRECOGNIZED_COMMAND;
}


