/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * Sample MCI ANIMATION Wine Driver for Linux
 *
 * Copyright 1994 Martin Ayotte
 */

#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "mmddk.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mcianim);

#define ANIMFRAMES_PERSEC 	30
#define ANIMFRAMES_PERMIN 	1800
#define SECONDS_PERMIN	 	60

typedef struct {
        UINT16		wDevID;
        int     	nUseCount;          /* Incremented for each shared open */
        BOOL16  	fShareable;         /* TRUE if first open was shareable */
        WORD    	wNotifyDeviceID;    /* MCI device ID with a pending notification */
        HANDLE16 	hCallback;          /* Callback handle for pending notification */
	MCI_OPEN_PARMSA openParms;
	DWORD		dwTimeFormat;
	int		mode;
	UINT16		nCurTrack;
	DWORD		dwCurFrame;
	UINT16		nTracks;
	DWORD		dwTotalLen;
	LPDWORD		lpdwTrackLen;
	LPDWORD		lpdwTrackPos;
} WINE_MCIANIM;

/*-----------------------------------------------------------------------*/

/**************************************************************************
 * 				MCIANIM_drvOpen			[internal]
 */
static	DWORD	MCIANIM_drvOpen(LPSTR str, LPMCI_OPEN_DRIVER_PARMSA modp)
{
    WINE_MCIANIM*	wma;

    if (!modp) return 0xFFFFFFFF;

    wma = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WINE_MCIANIM));

    if (!wma)
	return 0;

    wma->wDevID = modp->wDeviceID;
    mciSetDriverData(wma->wDevID, (DWORD)wma);
    modp->wCustomCommandTable = MCI_NO_COMMAND_TABLE;
    modp->wType = MCI_DEVTYPE_DIGITAL_VIDEO;
    return modp->wDeviceID;
}

/**************************************************************************
 * 				MCIANIM_drvClose		[internal]
 */
static	DWORD	MCIANIM_drvClose(DWORD dwDevID)
{
    WINE_MCIANIM*  wma = (WINE_MCIANIM*)mciGetDriverData(dwDevID);

    if (wma) {
	HeapFree(GetProcessHeap(), 0, wma);
	return 1;
    }
    return (dwDevID == 0xFFFFFFFF) ? 1 : 0;
}

/**************************************************************************
 * 				MCIANIM_mciGetOpenDrv		[internal]
 */
static WINE_MCIANIM*  MCIANIM_mciGetOpenDrv(UINT16 wDevID)
{
    WINE_MCIANIM*	wma = (WINE_MCIANIM*)mciGetDriverData(wDevID);

    if (wma == NULL || wma->nUseCount == 0) {
	WARN("Invalid wDevID=%u\n", wDevID);
	return 0;
    }
    return wma;
}

/**************************************************************************
 * 				MCIANIM_mciOpen			[internal]
 */
static DWORD MCIANIM_mciOpen(UINT16 wDevID, DWORD dwFlags, LPMCI_OPEN_PARMSA lpOpenParms)
{
    DWORD		dwDeviceID;
    WINE_MCIANIM*	wma = (WINE_MCIANIM*)mciGetDriverData(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpOpenParms);

    if (lpOpenParms == NULL) 	return MCIERR_INTERNAL;
    if (wma == NULL)		return MCIERR_INVALID_DEVICE_ID;

    if (wma->nUseCount > 0) {
	/* The driver already open on this channel */
	/* If the driver was opened shareable before and this open specifies */
	/* shareable then increment the use count */
	if (wma->fShareable && (dwFlags & MCI_OPEN_SHAREABLE))
	    ++wma->nUseCount;
	else
	    return MCIERR_MUST_USE_SHAREABLE;
    } else {
	wma->nUseCount = 1;
	wma->fShareable = dwFlags & MCI_OPEN_SHAREABLE;
    }

    dwDeviceID = lpOpenParms->wDeviceID;

    TRACE("wDevID=%04X\n", wDevID);
    /* FIXME this is not consistent with other implementations */
    lpOpenParms->wDeviceID = wDevID;

    /*TRACE("lpParms->wDevID=%04X\n", lpParms->wDeviceID);*/
    if (dwFlags & MCI_OPEN_ELEMENT) {
	TRACE("MCI_OPEN_ELEMENT '%s' !\n", lpOpenParms->lpstrElementName);
	if (lpOpenParms->lpstrElementName && strlen(lpOpenParms->lpstrElementName) > 0) {
	}
	FIXME("element is not opened\n");
    }
    memcpy(&wma->openParms, lpOpenParms, sizeof(MCI_OPEN_PARMSA));
    wma->wNotifyDeviceID = dwDeviceID;
    wma->mode = 0;
    wma->dwTimeFormat = MCI_FORMAT_TMSF;
    wma->nCurTrack = 0;
    wma->nTracks = 0;
    wma->dwTotalLen = 0;
    wma->lpdwTrackLen = NULL;
    wma->lpdwTrackPos = NULL;
    /*
      Moved to mmsystem.c mciOpen routine

      if (dwFlags & MCI_NOTIFY) {
      TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n",
      lpParms->dwCallback);
      mciDriverNotify((HWND16)LOWORD(lpParms->dwCallback),
      wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
      }
    */
    return 0;
}

/**************************************************************************
 * 				MCIANIM_mciClose		[internal]
 */
static DWORD MCIANIM_mciClose(UINT16 wDevID, DWORD dwParam, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);

    TRACE("(%u, %08lX, %p);\n", wDevID, dwParam, lpParms);

    if (wma == NULL)	 return MCIERR_INVALID_DEVICE_ID;

    if (--wma->nUseCount == 0) {
	/* do the actual clean-up */
    }
    return 0;
}

/**************************************************************************
 * 				MCIANIM_mciGetDevCaps	[internal]
 */
static DWORD MCIANIM_mciGetDevCaps(UINT16 wDevID, DWORD dwFlags,
				LPMCI_GETDEVCAPS_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);
    DWORD		ret;

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_NULL_PARAMETER_BLOCK;
    if (wma == NULL)	 return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_GETDEVCAPS_ITEM) {
	TRACE("MCI_GETDEVCAPS_ITEM dwItem=%08lX;\n", lpParms->dwItem);

	switch(lpParms->dwItem) {
	case MCI_GETDEVCAPS_CAN_RECORD:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_HAS_AUDIO:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_HAS_VIDEO:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_DEVICE_TYPE:
	    lpParms->dwReturn = MAKEMCIRESOURCE(MCI_DEVTYPE_ANIMATION, MCI_DEVTYPE_ANIMATION);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_USES_FILES:
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_COMPOUND_DEVICE:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_EJECT:
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_PLAY:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_SAVE:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	default:
	    FIXME("Unknown capability (%08lx) !\n", lpParms->dwItem);
	    return MCIERR_UNRECOGNIZED_COMMAND;
	}
    } else {
	WARN("No GETDEVCAPS_ITEM !\n");
	return MCIERR_UNRECOGNIZED_COMMAND;
    }
    TRACE("lpParms->dwReturn=%08lX;\n", lpParms->dwReturn);
    return ret;
}


/**************************************************************************
 * 				MCIANIM_CalcTime			[internal]
 */
static DWORD MCIANIM_CalcTime(WINE_MCIANIM* wma, DWORD dwFormatType, DWORD dwFrame, LPDWORD lpRet)
{
    DWORD	dwTime = 0;
    UINT16	wTrack;
    UINT16	wMinutes;
    UINT16	wSeconds;
    UINT16	wFrames;

    TRACE("(%p, %08lX, %lu);\n", wma, dwFormatType, dwFrame);

    switch (dwFormatType) {
    case MCI_FORMAT_MILLISECONDS:
	dwTime = dwFrame / ANIMFRAMES_PERSEC * 1000;
	*lpRet = 0;
	TRACE("MILLISECONDS %lu\n", dwTime);
	break;
    case MCI_FORMAT_MSF:
	wMinutes = dwFrame / ANIMFRAMES_PERMIN;
	wSeconds = (dwFrame - ANIMFRAMES_PERMIN * wMinutes) / ANIMFRAMES_PERSEC;
	wFrames = dwFrame - ANIMFRAMES_PERMIN * wMinutes -
	    ANIMFRAMES_PERSEC * wSeconds;
	dwTime = MCI_MAKE_MSF(wMinutes, wSeconds, wFrames);
	TRACE("MSF %02u:%02u:%02u -> dwTime=%lu\n",wMinutes, wSeconds, wFrames, dwTime);
	*lpRet = MCI_COLONIZED3_RETURN;
	break;
    default:
	/* unknown format ! force TMSF ! ... */
	dwFormatType = MCI_FORMAT_TMSF;
    case MCI_FORMAT_TMSF:
	for (wTrack = 0; wTrack < wma->nTracks; wTrack++) {
	    /*				dwTime += wma->lpdwTrackLen[wTrack - 1];
					TRACE("Adding trk#%u curpos=%u \n", dwTime);
					if (dwTime >= dwFrame) break; */
	    if (wma->lpdwTrackPos[wTrack - 1] >= dwFrame) break;
	}
	wMinutes = dwFrame / ANIMFRAMES_PERMIN;
	wSeconds = (dwFrame - ANIMFRAMES_PERMIN * wMinutes) / ANIMFRAMES_PERSEC;
	wFrames = dwFrame - ANIMFRAMES_PERMIN * wMinutes -
	    ANIMFRAMES_PERSEC * wSeconds;
	dwTime = MCI_MAKE_TMSF(wTrack, wMinutes, wSeconds, wFrames);
	*lpRet = MCI_COLONIZED4_RETURN;
	TRACE("%02u-%02u:%02u:%02u\n", wTrack, wMinutes, wSeconds, wFrames);
	break;
    }
    return dwTime;
}


/**************************************************************************
 * 				MCIANIM_CalcFrame			[internal]
 */
static DWORD MCIANIM_CalcFrame(WINE_MCIANIM* wma, DWORD dwFormatType, DWORD dwTime)
{
    DWORD	dwFrame = 0;
    UINT16	wTrack;

    TRACE("(%p, %08lX, %lu);\n", wma, dwFormatType, dwTime);

    switch (dwFormatType) {
    case MCI_FORMAT_MILLISECONDS:
	dwFrame = dwTime * ANIMFRAMES_PERSEC / 1000;
	TRACE("MILLISECONDS %lu\n", dwFrame);
	break;
    case MCI_FORMAT_MSF:
	TRACE("MSF %02u:%02u:%02u\n",
	      MCI_MSF_MINUTE(dwTime), MCI_MSF_SECOND(dwTime),
	      MCI_MSF_FRAME(dwTime));
	dwFrame += ANIMFRAMES_PERMIN * MCI_MSF_MINUTE(dwTime);
	dwFrame += ANIMFRAMES_PERSEC * MCI_MSF_SECOND(dwTime);
	dwFrame += MCI_MSF_FRAME(dwTime);
	break;
    default:
	/* unknown format ! force TMSF ! ... */
	dwFormatType = MCI_FORMAT_TMSF;
    case MCI_FORMAT_TMSF:
	wTrack = MCI_TMSF_TRACK(dwTime);
	TRACE("TMSF %02u-%02u:%02u:%02u\n",
	      MCI_TMSF_TRACK(dwTime), MCI_TMSF_MINUTE(dwTime),
	      MCI_TMSF_SECOND(dwTime), MCI_TMSF_FRAME(dwTime));
	TRACE("TMSF trackpos[%u]=%lu\n",
	      wTrack, wma->lpdwTrackPos[wTrack - 1]);
	dwFrame = wma->lpdwTrackPos[wTrack - 1];
	dwFrame += ANIMFRAMES_PERMIN * MCI_TMSF_MINUTE(dwTime);
	dwFrame += ANIMFRAMES_PERSEC * MCI_TMSF_SECOND(dwTime);
	dwFrame += MCI_TMSF_FRAME(dwTime);
	break;
    }
    return dwFrame;
}

/**************************************************************************
 * 				MCIANIM_mciInfo			[internal]
 */
static DWORD MCIANIM_mciInfo(UINT16 wDevID, DWORD dwFlags, LPMCI_INFO_PARMSA lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);
    LPSTR		str = 0;
    DWORD		ret = 0;

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL || lpParms->lpstrReturn == NULL)
	return MCIERR_NULL_PARAMETER_BLOCK;

    if (wma == NULL)
	return MCIERR_INVALID_DEVICE_ID;

    TRACE("buf=%p, len=%lu\n", lpParms->lpstrReturn, lpParms->dwRetSize);

    switch(dwFlags) {
    case MCI_INFO_PRODUCT:
	str = "Wine's animation";
	break;
    case MCI_INFO_FILE:
	str = wma->openParms.lpstrElementName;
	break;
    case MCI_ANIM_INFO_TEXT:
	str = "Animation Window";
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
 * 				MCIANIM_mciStatus		[internal]
 */
static DWORD MCIANIM_mciStatus(UINT16 wDevID, DWORD dwFlags, LPMCI_STATUS_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);
    DWORD		ret = MCIERR_UNRECOGNIZED_COMMAND;

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_INTERNAL;
    if (wma == NULL) return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);

	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    if (dwFlags & MCI_STATUS_ITEM) {
	switch(lpParms->dwItem) {
	case MCI_STATUS_CURRENT_TRACK:
	    lpParms->dwReturn = wma->nCurTrack;
	    TRACE("CURRENT_TRACK=%lu!\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_LENGTH:
	    if (dwFlags & MCI_TRACK) {
		TRACE("MCI_TRACK #%lu LENGTH=??? !\n", lpParms->dwTrack);
		if (lpParms->dwTrack > wma->nTracks)
		    return MCIERR_OUTOFRANGE;
		lpParms->dwReturn = wma->lpdwTrackLen[lpParms->dwTrack];
	    }
	    else
		lpParms->dwReturn = wma->dwTotalLen;
	    lpParms->dwReturn = MCIANIM_CalcTime(wma, wma->dwTimeFormat, lpParms->dwReturn, &ret);
	    TRACE("LENGTH=%lu !\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_MODE:
	    TRACE("MCI_STATUS_MODE=%04X !\n", wma->mode);
	    lpParms->dwReturn = MAKEMCIRESOURCE(wma->mode, wma->mode);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_STATUS_MEDIA_PRESENT:
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    TRACE("MCI_STATUS_MEDIA_PRESENT !\n");
	    break;
	case MCI_STATUS_NUMBER_OF_TRACKS:
	    lpParms->dwReturn = 1;
	    TRACE("MCI_STATUS_NUMBER_OF_TRACKS = %lu !\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_POSITION:
	    lpParms->dwReturn = wma->dwCurFrame;
	    if (dwFlags & MCI_STATUS_START) {
		lpParms->dwReturn = 0;
		TRACE("get MCI_STATUS_START !\n");
	    }
	    if (dwFlags & MCI_TRACK) {
		if (lpParms->dwTrack > wma->nTracks)
		    return MCIERR_OUTOFRANGE;
		lpParms->dwReturn = wma->lpdwTrackPos[lpParms->dwTrack - 1];
		TRACE("get MCI_TRACK #%lu !\n", lpParms->dwTrack);
	    }
	    lpParms->dwReturn = MCIANIM_CalcTime(wma, wma->dwTimeFormat, lpParms->dwReturn, &ret);
	    TRACE("MCI_STATUS_POSITION=%08lX !\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_READY:
	    TRACE("MCI_STATUS_READY !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    return 0;
	case MCI_STATUS_TIME_FORMAT:
	    TRACE("MCI_STATUS_TIME_FORMAT !\n");
	    lpParms->dwReturn = MAKEMCIRESOURCE(MCI_FORMAT_MILLISECONDS, MCI_FORMAT_MILLISECONDS);
	    TRACE("MCI_STATUS_TIME_FORMAT => %u\n", LOWORD(lpParms->dwReturn));
	    ret = MCI_RESOURCE_RETURNED;
	    return 0;
	default:
	    FIXME("Unknown command %08lX !\n", lpParms->dwItem);
	    return MCIERR_UNRECOGNIZED_COMMAND;
	}
    } else {
	WARN("No MCI_STATUS_ITEM !\n");
	return MCIERR_UNRECOGNIZED_COMMAND;
    }
    return ret;
}


/**************************************************************************
 * 				MCIANIM_mciPlay			[internal]
 */
static DWORD MCIANIM_mciPlay(UINT16 wDevID, DWORD dwFlags, LPMCI_PLAY_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);
    int 	start, end;

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_INTERNAL;
    if (wma == NULL) return MCIERR_INVALID_DEVICE_ID;

    start = 0; 		end = wma->dwTotalLen;
    wma->nCurTrack = 1;
    if (dwFlags & MCI_FROM) {
	start = MCIANIM_CalcFrame(wma, wma->dwTimeFormat, lpParms->dwFrom);
        TRACE("MCI_FROM=%08lX -> %u \n", lpParms->dwFrom, start);
    }
    if (dwFlags & MCI_TO) {
	end = MCIANIM_CalcFrame(wma, wma->dwTimeFormat, lpParms->dwTo);
	TRACE("MCI_TO=%08lX -> %u \n", lpParms->dwTo, end);
    }
    wma->mode = MCI_MODE_PLAY;
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n",
	      lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCIANIM_mciStop			[internal]
 */
static DWORD MCIANIM_mciStop(UINT16 wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_INTERNAL;
    if (wma == NULL) return MCIERR_INVALID_DEVICE_ID;

    wma->mode = MCI_MODE_STOP;
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);

	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCIANIM_mciPause		[internal]
 */
static DWORD MCIANIM_mciPause(UINT16 wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);
    if (lpParms == NULL) return MCIERR_INTERNAL;
    wma->mode = MCI_MODE_PAUSE;
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);

	mciDriverNotify((HWND16)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCIANIM_mciResume		[internal]
 */
static DWORD MCIANIM_mciResume(UINT16 wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);
    if (lpParms == NULL) return MCIERR_INTERNAL;
    wma->mode = MCI_MODE_STOP;
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);

	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCIANIM_mciSeek			[internal]
 */
static DWORD MCIANIM_mciSeek(UINT16 wDevID, DWORD dwFlags, LPMCI_SEEK_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);
    DWORD	dwRet;
    MCI_PLAY_PARMS PlayParms;

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_INTERNAL;
    wma->mode = MCI_MODE_SEEK;
    switch (dwFlags) {
    case MCI_SEEK_TO_START:
	PlayParms.dwFrom = 0;
	break;
    case MCI_SEEK_TO_END:
	PlayParms.dwFrom = wma->dwTotalLen;
	break;
    case MCI_TO:
	PlayParms.dwFrom = lpParms->dwTo;
	break;
    }
    dwRet = MCIANIM_mciPlay(wDevID, MCI_WAIT | MCI_FROM, &PlayParms);
    if (dwRet != 0) return dwRet;
    dwRet = MCIANIM_mciStop(wDevID, MCI_WAIT, (LPMCI_GENERIC_PARMS)&PlayParms);
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);

	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return dwRet;
}


/**************************************************************************
 * 				MCIANIM_mciSet			[internal]
 */
static DWORD MCIANIM_mciSet(UINT16 wDevID, DWORD dwFlags, LPMCI_SET_PARMS lpParms)
{
    WINE_MCIANIM*	wma = MCIANIM_mciGetOpenDrv(wDevID);

    TRACE("(%u, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_INTERNAL;
    if (wma == NULL) return MCIERR_INVALID_DEVICE_ID;
    /*
      TRACE("(dwTimeFormat=%08lX)\n", lpParms->dwTimeFormat);
      TRACE("(dwAudio=%08lX)\n", lpParms->dwAudio);
    */
    if (dwFlags & MCI_SET_TIME_FORMAT) {
	switch (lpParms->dwTimeFormat) {
	case MCI_FORMAT_MILLISECONDS:
	    TRACE("MCI_FORMAT_MILLISECONDS !\n");
	    break;
	case MCI_FORMAT_MSF:
	    TRACE("MCI_FORMAT_MSF !\n");
	    break;
	case MCI_FORMAT_TMSF:
	    TRACE("MCI_FORMAT_TMSF !\n");
	    break;
	default:
	    WARN("Bad time format !\n");
	    return MCIERR_BAD_TIME_FORMAT;
	}
	wma->dwTimeFormat = lpParms->dwTimeFormat;
    }
    if (dwFlags & MCI_SET_VIDEO) return MCIERR_UNSUPPORTED_FUNCTION;
    if (dwFlags & MCI_SET_ON) return MCIERR_UNSUPPORTED_FUNCTION;
    if (dwFlags & MCI_SET_OFF) return MCIERR_UNSUPPORTED_FUNCTION;
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wma->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				DriverProc (MCIANIM.@)
 */
LONG WINAPI MCIANIM_DriverProc(DWORD dwDevID, HDRVR hDriv, DWORD wMsg,
			       DWORD dwParam1, DWORD dwParam2)
{
    switch (wMsg) {
    case DRV_LOAD:		return 1;
    case DRV_FREE:		return 1;
    case DRV_OPEN:		return MCIANIM_drvOpen((LPSTR)dwParam1, (LPMCI_OPEN_DRIVER_PARMSA)dwParam2);
    case DRV_CLOSE:		return MCIANIM_drvClose(dwDevID);
    case DRV_ENABLE:		return 1;
    case DRV_DISABLE:		return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "Sample MultiMedia Driver !", "Wine Driver", MB_OK); return 1;
    case DRV_INSTALL:		return DRVCNF_RESTART;
    case DRV_REMOVE:		return DRVCNF_RESTART;
    }

    if (dwDevID == 0xFFFFFFFF) return MCIERR_UNSUPPORTED_FUNCTION;

    switch (wMsg) {
    case MCI_OPEN_DRIVER:	return MCIANIM_mciOpen(dwDevID, dwParam1, (LPMCI_OPEN_PARMSA)dwParam2);
    case MCI_CLOSE_DRIVER:	return MCIANIM_mciClose(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_GETDEVCAPS:	return MCIANIM_mciGetDevCaps(dwDevID, dwParam1, (LPMCI_GETDEVCAPS_PARMS)dwParam2);
    case MCI_INFO:		return MCIANIM_mciInfo(dwDevID, dwParam1, (LPMCI_INFO_PARMSA)dwParam2);
    case MCI_STATUS:		return MCIANIM_mciStatus(dwDevID, dwParam1, (LPMCI_STATUS_PARMS)dwParam2);
    case MCI_SET:		return MCIANIM_mciSet(dwDevID, dwParam1, (LPMCI_SET_PARMS)dwParam2);
    case MCI_PLAY:		return MCIANIM_mciPlay(dwDevID, dwParam1, (LPMCI_PLAY_PARMS)dwParam2);
    case MCI_STOP:		return MCIANIM_mciStop(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_PAUSE:		return MCIANIM_mciPause(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_RESUME:		return MCIANIM_mciResume(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_SEEK:		return MCIANIM_mciSeek(dwDevID, dwParam1, (LPMCI_SEEK_PARMS)dwParam2);
    case MCI_LOAD:
    case MCI_SAVE:
    case MCI_FREEZE:
    case MCI_PUT:
    case MCI_REALIZE:
    case MCI_UNFREEZE:
    case MCI_UPDATE:
    case MCI_WHERE:
    case MCI_WINDOW:
    case MCI_STEP:
    case MCI_SPIN:
    case MCI_ESCAPE:
    case MCI_COPY:
    case MCI_CUT:
    case MCI_DELETE:
    case MCI_PASTE:
	FIXME("Unsupported message [%lu]\n", wMsg);
	break;
    case MCI_OPEN:
    case MCI_CLOSE:
	ERR("Shouldn't receive a MCI_OPEN or CLOSE message\n");
	break;
    default:
	TRACE("Sending msg [%lu] to default driver proc\n", wMsg);
	return DefDriverProc(dwDevID, hDriv, wMsg, dwParam1, dwParam2);
    }
    return MCIERR_UNRECOGNIZED_COMMAND;
}

/*-----------------------------------------------------------------------*/
