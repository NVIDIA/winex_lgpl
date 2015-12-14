/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * MCI driver for audio CD (MCICDA)
 *
 * Copyright 1994    Martin Ayotte
 * Copyright 1998-99 Eric Pouech
 * Copyright 2000    Andreas Mohr
 */

#include "config.h"
#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "mmddk.h"
#include "winioctl.h"
#include "ntddstor.h"
#include "ntddcdrm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mcicda);

#define CDFRAMES_PERSEC                 75
#define CDFRAMES_PERMIN                 (CDFRAMES_PERSEC * 60)
#define FRAME_OF_ADDR(a) ((a)[1] * CDFRAMES_PERMIN + (a)[2] * CDFRAMES_PERSEC + (a)[3])
#define FRAME_OF_TOC(toc, idx)  FRAME_OF_ADDR((toc).TrackData[idx - (toc).FirstTrack].Address)

typedef struct {
    UINT		wDevID;
    int     		nUseCount;          /* Incremented for each shared open */
    BOOL  		fShareable;         /* TRUE if first open was shareable */
    WORD    		wNotifyDeviceID;    /* MCI device ID with a pending notification */
    HANDLE 		hCallback;          /* Callback handle for pending notification */
    DWORD		dwTimeFormat;
    HANDLE              handle;
} WINE_MCICDAUDIO;

/*-----------------------------------------------------------------------*/

/**************************************************************************
 * 				MCICDA_drvOpen			[internal]
 */
static	DWORD	MCICDA_drvOpen(LPSTR str, LPMCI_OPEN_DRIVER_PARMSA modp)
{
    WINE_MCICDAUDIO*	wmcda;

    if (!modp) return 0xFFFFFFFF;

    wmcda = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,  sizeof(WINE_MCICDAUDIO));

    if (!wmcda)
	return 0;

    wmcda->wDevID = modp->wDeviceID;
    mciSetDriverData(wmcda->wDevID, (DWORD)wmcda);
    modp->wCustomCommandTable = MCI_NO_COMMAND_TABLE;
    modp->wType = MCI_DEVTYPE_CD_AUDIO;
    return modp->wDeviceID;
}

/**************************************************************************
 * 				MCICDA_drvClose			[internal]
 */
static	DWORD	MCICDA_drvClose(DWORD dwDevID)
{
    WINE_MCICDAUDIO*  wmcda = (WINE_MCICDAUDIO*)mciGetDriverData(dwDevID);

    if (wmcda) {
	HeapFree(GetProcessHeap(), 0, wmcda);
	mciSetDriverData(dwDevID, 0);
    }
    return (dwDevID == 0xFFFFFFFF) ? 1 : 0;
}

/**************************************************************************
 * 				MCICDA_GetOpenDrv		[internal]
 */
static WINE_MCICDAUDIO*  MCICDA_GetOpenDrv(UINT wDevID)
{
    WINE_MCICDAUDIO*	wmcda = (WINE_MCICDAUDIO*)mciGetDriverData(wDevID);

    if (wmcda == NULL || wmcda->nUseCount == 0) {
	WARN("Invalid wDevID=%u\n", wDevID);
	return 0;
    }
    return wmcda;
}

/**************************************************************************
 * 				MCICDA_GetStatus		[internal]
 */
static	DWORD    MCICDA_GetStatus(WINE_MCICDAUDIO* wmcda)
{
    CDROM_SUB_Q_DATA_FORMAT     fmt;
    SUB_Q_CHANNEL_DATA          data;
    DWORD                       br;
    DWORD                       mode = MCI_MODE_NOT_READY;

    fmt.Format = IOCTL_CDROM_CURRENT_POSITION;
    if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_Q_CHANNEL, &fmt, sizeof(fmt),
                         &data, sizeof(data), &br, NULL)) {
        if (GetLastError() == STATUS_NO_MEDIA_IN_DEVICE) mode = MCI_MODE_OPEN;
    } else {
        switch (data.CurrentPosition.Header.AudioStatus)
        {
        case AUDIO_STATUS_IN_PROGRESS:          mode = MCI_MODE_PLAY;   break;
        case AUDIO_STATUS_PAUSED:               mode = MCI_MODE_PAUSE;  break;
        case AUDIO_STATUS_NO_STATUS:
        case AUDIO_STATUS_PLAY_COMPLETE:        mode = MCI_MODE_STOP;   break;
        case AUDIO_STATUS_PLAY_ERROR:
        case AUDIO_STATUS_NOT_SUPPORTED:
        default:
            break;
        }
    }
    return mode;
}

/**************************************************************************
 * 				MCICDA_GetError			[internal]
 */
static	int	MCICDA_GetError(WINE_MCICDAUDIO* wmcda)
{
    switch (GetLastError())
    {
    case STATUS_NO_MEDIA_IN_DEVICE:     return MCIERR_DEVICE_NOT_READY;
    case STATUS_IO_DEVICE_ERROR:        return MCIERR_HARDWARE;
    default:
	FIXME("Unknown mode %lx\n", GetLastError());
    }
    return MCIERR_DRIVER_INTERNAL;
}

/**************************************************************************
 * 			MCICDA_CalcFrame			[internal]
 */
static DWORD MCICDA_CalcFrame(WINE_MCICDAUDIO* wmcda, DWORD dwTime)
{
    DWORD	dwFrame = 0;
    UINT	wTrack;
    CDROM_TOC   toc;
    DWORD       br;
    BYTE*       addr;

    TRACE("(%p, %08lX, %lu);\n", wmcda, wmcda->dwTimeFormat, dwTime);

    switch (wmcda->dwTimeFormat) {
    case MCI_FORMAT_MILLISECONDS:
	dwFrame = ((dwTime - 1) * CDFRAMES_PERSEC + 500) / 1000;
	TRACE("MILLISECONDS %lu\n", dwFrame);
	break;
    case MCI_FORMAT_MSF:
	TRACE("MSF %02u:%02u:%02u\n",
	      MCI_MSF_MINUTE(dwTime), MCI_MSF_SECOND(dwTime), MCI_MSF_FRAME(dwTime));
	dwFrame += CDFRAMES_PERMIN * MCI_MSF_MINUTE(dwTime);
	dwFrame += CDFRAMES_PERSEC * MCI_MSF_SECOND(dwTime);
	dwFrame += MCI_MSF_FRAME(dwTime);
	break;
    case MCI_FORMAT_TMSF:
    default: /* unknown format ! force TMSF ! ... */
	wTrack = MCI_TMSF_TRACK(dwTime);
        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                             &toc, sizeof(toc), &br, NULL))
            return 0;
        if (wTrack < toc.FirstTrack || wTrack > toc.LastTrack)
            return 0;
        TRACE("MSF %02u-%02u:%02u:%02u\n",
              MCI_TMSF_TRACK(dwTime), MCI_TMSF_MINUTE(dwTime),
              MCI_TMSF_SECOND(dwTime), MCI_TMSF_FRAME(dwTime));
        addr = toc.TrackData[wTrack - toc.FirstTrack].Address;
        TRACE("TMSF trackpos[%u]=%d:%d:%d\n",
              wTrack, addr[0], addr[1], addr[2]);
        dwFrame = CDFRAMES_PERMIN * (addr[0] + MCI_TMSF_MINUTE(dwTime)) +
            CDFRAMES_PERSEC * (addr[1] + MCI_TMSF_SECOND(dwTime)) +
            addr[2] + MCI_TMSF_FRAME(dwTime);
	break;
    }
    return dwFrame;
}

/**************************************************************************
 * 			MCICDA_CalcTime				[internal]
 */
static DWORD MCICDA_CalcTime(WINE_MCICDAUDIO* wmcda, DWORD tf, DWORD dwFrame, LPDWORD lpRet)
{
    DWORD	dwTime = 0;
    UINT	wTrack;
    UINT	wMinutes;
    UINT	wSeconds;
    UINT	wFrames;
    CDROM_TOC   toc;
    DWORD       br;

    TRACE("(%p, %08lX, %lu);\n", wmcda, tf, dwFrame);

    switch (tf) {
    case MCI_FORMAT_MILLISECONDS:
	dwTime = (dwFrame * 1000) / CDFRAMES_PERSEC + 1;
	TRACE("MILLISECONDS %lu\n", dwTime);
	*lpRet = 0;
	break;
    case MCI_FORMAT_MSF:
	wMinutes = dwFrame / CDFRAMES_PERMIN;
	wSeconds = (dwFrame - CDFRAMES_PERMIN * wMinutes) / CDFRAMES_PERSEC;
	wFrames = dwFrame - CDFRAMES_PERMIN * wMinutes - CDFRAMES_PERSEC * wSeconds;
	dwTime = MCI_MAKE_MSF(wMinutes, wSeconds, wFrames);
	TRACE("MSF %02u:%02u:%02u -> dwTime=%lu\n",
	      wMinutes, wSeconds, wFrames, dwTime);
	*lpRet = MCI_COLONIZED3_RETURN;
	break;
    case MCI_FORMAT_TMSF:
    default:	/* unknown format ! force TMSF ! ... */
        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                             &toc, sizeof(toc), &br, NULL))
            return 0;
	if (dwFrame < FRAME_OF_TOC(toc, toc.FirstTrack) ||
            dwFrame > FRAME_OF_TOC(toc, toc.LastTrack + 1)) {
	    ERR("Out of range value %lu [%u,%u]\n",
		dwFrame, FRAME_OF_TOC(toc, toc.FirstTrack),
                FRAME_OF_TOC(toc, toc.LastTrack + 1));
	    *lpRet = 0;
	    return 0;
	}
	for (wTrack = toc.FirstTrack; wTrack <= toc.LastTrack; wTrack++) {
	    if (FRAME_OF_TOC(toc, wTrack) > dwFrame)
		break;
	}
        wTrack--;
	dwFrame -= FRAME_OF_TOC(toc, wTrack);
	wMinutes = dwFrame / CDFRAMES_PERMIN;
	wSeconds = (dwFrame - CDFRAMES_PERMIN * wMinutes) / CDFRAMES_PERSEC;
	wFrames = dwFrame - CDFRAMES_PERMIN * wMinutes - CDFRAMES_PERSEC * wSeconds;
	dwTime = MCI_MAKE_TMSF(wTrack, wMinutes, wSeconds, wFrames);
	TRACE("%02u-%02u:%02u:%02u\n", wTrack, wMinutes, wSeconds, wFrames);
	*lpRet = MCI_COLONIZED4_RETURN;
	break;
    }
    return dwTime;
}

static DWORD MCICDA_Seek(UINT wDevID, DWORD dwFlags, LPMCI_SEEK_PARMS lpParms);
static DWORD MCICDA_Stop(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms);

/**************************************************************************
 * 				MCICDA_Open			[internal]
 */
static DWORD MCICDA_Open(UINT wDevID, DWORD dwFlags, LPMCI_OPEN_PARMSA lpOpenParms)
{
    DWORD		dwDeviceID;
    DWORD               ret = MCIERR_HARDWARE;
    WINE_MCICDAUDIO* 	wmcda = (WINE_MCICDAUDIO*)mciGetDriverData(wDevID);
    char                root[7];
    int                 count;
    char                drive = 0;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpOpenParms);

    if (lpOpenParms == NULL) 		return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmcda == NULL)			return MCIERR_INVALID_DEVICE_ID;

    dwDeviceID = lpOpenParms->wDeviceID;

    if (wmcda->nUseCount > 0) {
	/* The driver is already open on this channel */
	/* If the driver was opened shareable before and this open specifies */
	/* shareable then increment the use count */
	if (wmcda->fShareable && (dwFlags & MCI_OPEN_SHAREABLE))
	    ++wmcda->nUseCount;
	else
	    return MCIERR_MUST_USE_SHAREABLE;
    } else {
	wmcda->nUseCount = 1;
	wmcda->fShareable = dwFlags & MCI_OPEN_SHAREABLE;
    }
    if (dwFlags & MCI_OPEN_ELEMENT) {
        if (dwFlags & MCI_OPEN_ELEMENT_ID) {
            WARN("MCI_OPEN_ELEMENT_ID %8lx ! Abort\n", (DWORD)lpOpenParms->lpstrElementName);
            return MCIERR_NO_ELEMENT_ALLOWED;
        }
        if (!isalpha(lpOpenParms->lpstrElementName[0]) || lpOpenParms->lpstrElementName[1] != ':' ||
            lpOpenParms->lpstrElementName[2])
        {
            WARN("MCI_OPEN_ELEMENT unsupported format: %s\n", lpOpenParms->lpstrElementName);
            ret = MCIERR_NO_ELEMENT_ALLOWED;
            goto the_error;
        }
        drive = toupper(lpOpenParms->lpstrElementName[0]);
        strcpy(root, "A:\\");
        root[0] = drive;
        if (GetDriveTypeA(root) != DRIVE_CDROM)
        {
            ret = MCIERR_INVALID_DEVICE_NAME;
            goto the_error;
        }
    }
    else
    {
        /* drive letter isn't passed... get the dwDeviceID'th cdrom in the system */
        strcpy(root, "A:\\");
        for (count = 0; root[0] <= 'Z'; root[0]++)
        {
            if (GetDriveTypeA(root) == DRIVE_CDROM && ++count >= dwDeviceID)
            {
                drive = root[0];
                break;
            }
        }
        if (!drive)
        {
            ret = MCIERR_INVALID_DEVICE_ID;
            goto the_error;
        }
    }

    wmcda->wNotifyDeviceID = dwDeviceID;
    wmcda->dwTimeFormat = MCI_FORMAT_MSF;

    /* now, open the handle */
    strcpy(root, "\\\\.\\A:");
    root[4] = drive;
    wmcda->handle = CreateFileA(root, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
    if (wmcda->handle != INVALID_HANDLE_VALUE)
        return 0;

 the_error:
    --wmcda->nUseCount;
    return ret;
}

/**************************************************************************
 * 				MCICDA_Close			[internal]
 */
static DWORD MCICDA_Close(UINT wDevID, DWORD dwParam, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwParam, lpParms);

    if (wmcda == NULL) 	return MCIERR_INVALID_DEVICE_ID;

    if (--wmcda->nUseCount == 0) {
	CloseHandle(wmcda->handle);
    }
    return 0;
}

/**************************************************************************
 * 				MCICDA_GetDevCaps		[internal]
 */
static DWORD MCICDA_GetDevCaps(UINT wDevID, DWORD dwFlags,
				   LPMCI_GETDEVCAPS_PARMS lpParms)
{
    DWORD	ret = 0;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_NULL_PARAMETER_BLOCK;

    if (dwFlags & MCI_GETDEVCAPS_ITEM) {
	TRACE("MCI_GETDEVCAPS_ITEM dwItem=%08lX;\n", lpParms->dwItem);

	switch (lpParms->dwItem) {
	case MCI_GETDEVCAPS_CAN_RECORD:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_HAS_AUDIO:
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_HAS_VIDEO:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_DEVICE_TYPE:
	    lpParms->dwReturn = MAKEMCIRESOURCE(MCI_DEVTYPE_CD_AUDIO, MCI_DEVTYPE_CD_AUDIO);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_USES_FILES:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
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
	    lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_GETDEVCAPS_CAN_SAVE:
	    lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	default:
	    ERR("Unsupported %lx devCaps item\n", lpParms->dwItem);
	    return MCIERR_UNRECOGNIZED_COMMAND;
	}
    } else {
	TRACE("No GetDevCaps-Item !\n");
	return MCIERR_UNRECOGNIZED_COMMAND;
    }
    TRACE("lpParms->dwReturn=%08lX;\n", lpParms->dwReturn);
    return ret;
}

static DWORD CDROM_Audio_GetSerial(CDROM_TOC* toc)
{
    unsigned long serial = 0;
    int i;
    WORD wMagic;
    DWORD dwStart, dwEnd;

    /*
     * wMagic collects the wFrames from track 1
     * dwStart, dwEnd collect the beginning and end of the disc respectively, in
     * frames.
     * There it is collected for correcting the serial when there are less than
     * 3 tracks.
     */
    wMagic = toc->TrackData[0].Address[3];
    dwStart = FRAME_OF_TOC(*toc, toc->FirstTrack);

    for (i = 0; i <= toc->LastTrack - toc->FirstTrack; i++) {
        serial += (toc->TrackData[i].Address[1] << 16) |
            (toc->TrackData[i].Address[2] << 8) | toc->TrackData[i].Address[3];
    }
    dwEnd = FRAME_OF_TOC(*toc, toc->LastTrack + 1);

    if (toc->LastTrack - toc->FirstTrack + 1 < 3)
        serial += wMagic + (dwEnd - dwStart);

    return serial;
}


/**************************************************************************
 * 				MCICDA_Info			[internal]
 */
static DWORD MCICDA_Info(UINT wDevID, DWORD dwFlags, LPMCI_INFO_PARMSA lpParms)
{
    LPSTR		str = NULL;
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD		ret = 0;
    char		buffer[16];

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL || lpParms->lpstrReturn == NULL)
	return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmcda == NULL) return MCIERR_INVALID_DEVICE_ID;

    TRACE("buf=%p, len=%lu\n", lpParms->lpstrReturn, lpParms->dwRetSize);

    if (dwFlags & MCI_INFO_PRODUCT) {
	str = "Wine's audio CD";
    } else if (dwFlags & MCI_INFO_MEDIA_UPC) {
	ret = MCIERR_NO_IDENTITY;
    } else if (dwFlags & MCI_INFO_MEDIA_IDENTITY) {
	DWORD	    res = 0;
        CDROM_TOC   toc;
        DWORD       br;

        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                             &toc, sizeof(toc), &br, NULL)) {
	    return MCICDA_GetError(wmcda);
	}

	res = CDROM_Audio_GetSerial(&toc);
	sprintf(buffer, "%lu", res);
	str = buffer;
    } else {
	WARN("Don't know this info command (%lu)\n", dwFlags);
	ret = MCIERR_UNRECOGNIZED_COMMAND;
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
    TRACE("=> %s (%ld)\n", lpParms->lpstrReturn, ret);
    return ret;
}

/**************************************************************************
 * 				MCICDA_Status			[internal]
 */
static DWORD MCICDA_Status(UINT wDevID, DWORD dwFlags, LPMCI_STATUS_PARMS lpParms)
{
    WINE_MCICDAUDIO*	        wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD                       idx;
    DWORD	                ret = 0;
    CDROM_SUB_Q_DATA_FORMAT     fmt;
    SUB_Q_CHANNEL_DATA          data;
    CDROM_TOC                   toc;
    DWORD                       br;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL) return MCIERR_NULL_PARAMETER_BLOCK;
    if (wmcda == NULL) return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    if (dwFlags & MCI_STATUS_ITEM) {
	TRACE("dwItem = %lx\n", lpParms->dwItem);
	switch (lpParms->dwItem) {
	case MCI_STATUS_CURRENT_TRACK:
            fmt.Format = IOCTL_CDROM_CURRENT_POSITION;
            if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_Q_CHANNEL, &fmt, sizeof(fmt),
                                 &data, sizeof(data), &br, NULL))
            {
		return MCICDA_GetError(wmcda);
	    }
	    lpParms->dwReturn = data.CurrentPosition.TrackNumber;
	    TRACE("CURRENT_TRACK=%lu!\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_LENGTH:
            if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                                 &toc, sizeof(toc), &br, NULL)) {
                WARN("error reading TOC !\n");
                return MCICDA_GetError(wmcda);
	    }
	    if (dwFlags & MCI_TRACK) {
		TRACE("MCI_TRACK #%lu LENGTH=??? !\n", lpParms->dwTrack);
		if (lpParms->dwTrack < toc.FirstTrack || lpParms->dwTrack > toc.LastTrack)
		    return MCIERR_OUTOFRANGE;
                idx = lpParms->dwTrack - toc.FirstTrack;
		lpParms->dwReturn = FRAME_OF_TOC(toc, lpParms->dwTrack + 1) -
                    FRAME_OF_TOC(toc, lpParms->dwTrack);
		/* Windows returns one frame less than the total track length for the
		   last track on the CD.  See CDDB HOWTO.  Verified on Win95OSR2. */
		if (lpParms->dwTrack == toc.LastTrack)
		    lpParms->dwReturn--;
	    } else {
		/* Sum of the lengths of all of the tracks.  Inherits the
		   'off by one frame' behavior from the length of the last track.
		   See above comment. */
		lpParms->dwReturn = FRAME_OF_TOC(toc, toc.LastTrack + 1) -
                    FRAME_OF_TOC(toc, toc.FirstTrack) - 1;
	    }
	    lpParms->dwReturn = MCICDA_CalcTime(wmcda,
						 (wmcda->dwTimeFormat == MCI_FORMAT_TMSF)
						    ? MCI_FORMAT_MSF : wmcda->dwTimeFormat,
						 lpParms->dwReturn,
						 &ret);
	    TRACE("LENGTH=%lu !\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_MODE:
            lpParms->dwReturn = MCICDA_GetStatus(wmcda);
	    TRACE("MCI_STATUS_MODE=%08lX !\n", lpParms->dwReturn);
	    lpParms->dwReturn = MAKEMCIRESOURCE(lpParms->dwReturn, lpParms->dwReturn);
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_STATUS_MEDIA_PRESENT:
	    lpParms->dwReturn = (MCICDA_GetStatus(wmcda) == MCI_MODE_OPEN) ?
		MAKEMCIRESOURCE(FALSE, MCI_FALSE) : MAKEMCIRESOURCE(TRUE, MCI_TRUE);
	    TRACE("MCI_STATUS_MEDIA_PRESENT =%c!\n", LOWORD(lpParms->dwReturn) ? 'Y' : 'N');
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_STATUS_NUMBER_OF_TRACKS:
            if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                                 &toc, sizeof(toc), &br, NULL)) {
                WARN("error reading TOC !\n");
                return MCICDA_GetError(wmcda);
	    }
	    lpParms->dwReturn = toc.LastTrack - toc.FirstTrack + 1;
	    TRACE("MCI_STATUS_NUMBER_OF_TRACKS = %lu !\n", lpParms->dwReturn);
	    if (lpParms->dwReturn == (WORD)-1)
		return MCICDA_GetError(wmcda);
	    break;
	case MCI_STATUS_POSITION:
	    if (dwFlags & MCI_STATUS_START) {
                if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                                     &toc, sizeof(toc), &br, NULL)) {
                    WARN("error reading TOC !\n");
                    return MCICDA_GetError(wmcda);
                }
		lpParms->dwReturn = FRAME_OF_TOC(toc, toc.FirstTrack);
		TRACE("get MCI_STATUS_START !\n");
	    } else if (dwFlags & MCI_TRACK) {
                if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                                     &toc, sizeof(toc), &br, NULL)) {
                    WARN("error reading TOC !\n");
                    return MCICDA_GetError(wmcda);
                }
		if (lpParms->dwTrack < toc.FirstTrack || lpParms->dwTrack > toc.LastTrack)
		    return MCIERR_OUTOFRANGE;
		lpParms->dwReturn = FRAME_OF_TOC(toc, lpParms->dwTrack);
		TRACE("get MCI_TRACK #%lu !\n", lpParms->dwTrack);
            } else {
                fmt.Format = IOCTL_CDROM_CURRENT_POSITION;
                if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_Q_CHANNEL, &fmt, sizeof(fmt),
                                     &data, sizeof(data), &br, NULL)) {
                    return MCICDA_GetError(wmcda);
                }
                lpParms->dwReturn = FRAME_OF_ADDR(data.CurrentPosition.AbsoluteAddress);
            }
	    lpParms->dwReturn = MCICDA_CalcTime(wmcda, wmcda->dwTimeFormat, lpParms->dwReturn, &ret);
	    TRACE("MCI_STATUS_POSITION=%08lX !\n", lpParms->dwReturn);
	    break;
	case MCI_STATUS_READY:
	    TRACE("MCI_STATUS_READY !\n");
            switch (MCICDA_GetStatus(wmcda))
            {
            case MCI_MODE_NOT_READY:
            case MCI_MODE_OPEN:
                lpParms->dwReturn = MAKEMCIRESOURCE(FALSE, MCI_FALSE);
                break;
            default:
                lpParms->dwReturn = MAKEMCIRESOURCE(TRUE, MCI_TRUE);
                break;
            }
	    TRACE("MCI_STATUS_READY=%u!\n", LOWORD(lpParms->dwReturn));
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case MCI_STATUS_TIME_FORMAT:
	    lpParms->dwReturn = MAKEMCIRESOURCE(wmcda->dwTimeFormat, wmcda->dwTimeFormat);
	    TRACE("MCI_STATUS_TIME_FORMAT=%08x!\n", LOWORD(lpParms->dwReturn));
	    ret = MCI_RESOURCE_RETURNED;
	    break;
	case 4001: /* FIXME: for bogus FullCD */
	case MCI_CDA_STATUS_TYPE_TRACK:
	    if (!(dwFlags & MCI_TRACK))
		ret = MCIERR_MISSING_PARAMETER;
	    else {
                if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                                     &toc, sizeof(toc), &br, NULL)) {
                    WARN("error reading TOC !\n");
                    return MCICDA_GetError(wmcda);
                }
		if (lpParms->dwTrack < toc.FirstTrack || lpParms->dwTrack > toc.LastTrack)
		    ret = MCIERR_OUTOFRANGE;
		else
		    lpParms->dwReturn = (toc.TrackData[lpParms->dwTrack - toc.FirstTrack].Control & 0x04) ?
                                         MCI_CDA_TRACK_OTHER : MCI_CDA_TRACK_AUDIO;
	    }
	    TRACE("MCI_CDA_STATUS_TYPE_TRACK[%ld]=%08lx\n", lpParms->dwTrack, lpParms->dwReturn);
	    break;
	default:
	    FIXME("unknown command %08lX !\n", lpParms->dwItem);
	    return MCIERR_UNRECOGNIZED_COMMAND;
	}
    } else {
	WARN("not MCI_STATUS_ITEM !\n");
    }
    return ret;
}

/**************************************************************************
 * 				MCICDA_Play			[internal]
 */
static DWORD MCICDA_Play(UINT wDevID, DWORD dwFlags, LPMCI_PLAY_PARMS lpParms)
{
    WINE_MCICDAUDIO*	        wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD		        ret = 0, start, end;
    CDROM_TOC                   toc;
    DWORD                       br;
    CDROM_PLAY_AUDIO_MSF        play;
    CDROM_SUB_Q_DATA_FORMAT     fmt;
    SUB_Q_CHANNEL_DATA          data;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (lpParms == NULL)
	return MCIERR_NULL_PARAMETER_BLOCK;

    if (wmcda == NULL)
	return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_FROM) {
	start = MCICDA_CalcFrame(wmcda, lpParms->dwFrom);
	TRACE("MCI_FROM=%08lX -> %lu \n", lpParms->dwFrom, start);
    } else {
        fmt.Format = IOCTL_CDROM_CURRENT_POSITION;
        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_Q_CHANNEL, &fmt, sizeof(fmt),
                             &data, sizeof(data), &br, NULL)) {
            return MCICDA_GetError(wmcda);
        }
        start = FRAME_OF_ADDR(data.CurrentPosition.AbsoluteAddress);
    }
    if (dwFlags & MCI_TO) {
	end = MCICDA_CalcFrame(wmcda, lpParms->dwTo);
	TRACE("MCI_TO=%08lX -> %lu \n", lpParms->dwTo, end);
    } else {
        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                             &toc, sizeof(toc), &br, NULL)) {
            WARN("error reading TOC !\n");
            return MCICDA_GetError(wmcda);
        }
	end = FRAME_OF_TOC(toc, toc.LastTrack + 1) - 1;
    }
    TRACE("Playing from %lu to %lu\n", start, end);
    play.StartingM = start / CDFRAMES_PERMIN;
    play.StartingS = (start / CDFRAMES_PERSEC) % 60;
    play.StartingF = start % CDFRAMES_PERSEC;
    play.EndingM   = end / CDFRAMES_PERMIN;
    play.EndingS   = (end / CDFRAMES_PERSEC) % 60;
    play.EndingF   = end % CDFRAMES_PERSEC;
    if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_PLAY_AUDIO_MSF, &play, sizeof(play),
                         NULL, 0, &br, NULL)) {
	ret = MCIERR_HARDWARE;
    } else if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	/*
	  mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
	  wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
	*/
    }
    return ret;
}

/**************************************************************************
 * 				MCICDA_Stop			[internal]
 */
static DWORD MCICDA_Stop(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD               br;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmcda == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, &br, NULL))
	return MCIERR_HARDWARE;

    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCICDA_Pause			[internal]
 */
static DWORD MCICDA_Pause(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD               br;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmcda == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_PAUSE_AUDIO, NULL, 0, NULL, 0, &br, NULL))
	return MCIERR_HARDWARE;

    if (lpParms && (dwFlags & MCI_NOTIFY)) {
        TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCICDA_Resume			[internal]
 */
static DWORD MCICDA_Resume(UINT wDevID, DWORD dwFlags, LPMCI_GENERIC_PARMS lpParms)
{
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD               br;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmcda == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_RESUME_AUDIO, NULL, 0, NULL, 0, &br, NULL))
	return MCIERR_HARDWARE;

    if (lpParms && (dwFlags & MCI_NOTIFY)) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCICDA_Seek			[internal]
 */
static DWORD MCICDA_Seek(UINT wDevID, DWORD dwFlags, LPMCI_SEEK_PARMS lpParms)
{
    DWORD		        at;
    WINE_MCICDAUDIO*	        wmcda = MCICDA_GetOpenDrv(wDevID);
    CDROM_SEEK_AUDIO_MSF        seek;
    CDROM_TOC                   toc;
    DWORD                       br;

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmcda == NULL)	return MCIERR_INVALID_DEVICE_ID;
    if (lpParms == NULL) return MCIERR_NULL_PARAMETER_BLOCK;

    switch (dwFlags & ~(MCI_NOTIFY|MCI_WAIT)) {
    case MCI_SEEK_TO_START:
	TRACE("Seeking to start\n");
        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                             &toc, sizeof(toc), &br, NULL)) {
            WARN("error reading TOC !\n");
            return MCICDA_GetError(wmcda);
        }
        at = FRAME_OF_TOC(toc, toc.FirstTrack);
	break;
    case MCI_SEEK_TO_END:
	TRACE("Seeking to end\n");
        if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_READ_TOC, NULL, 0,
                             &toc, sizeof(toc), &br, NULL)) {
            WARN("error reading TOC !\n");
            return MCICDA_GetError(wmcda);
        }
	at = FRAME_OF_TOC(toc, toc.LastTrack + 1) - 1;
	break;
    case MCI_TO:
	TRACE("Seeking to %lu\n", lpParms->dwTo);
	at = lpParms->dwTo;
	break;
    default:
	TRACE("Unknown seek action %08lX\n",
	      (dwFlags & ~(MCI_NOTIFY|MCI_WAIT)));
	return MCIERR_UNSUPPORTED_FUNCTION;
    }
    seek.M = at / CDFRAMES_PERMIN;
    seek.S = (at / CDFRAMES_PERSEC) % 60;
    seek.F = at % CDFRAMES_PERSEC;
    if (!DeviceIoControl(wmcda->handle, IOCTL_CDROM_SEEK_AUDIO_MSF, &seek, sizeof(seek),
                         NULL, 0, &br, NULL))
	return MCIERR_HARDWARE;

    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n", lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			  wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 				MCICDA_SetDoor			[internal]
 */
static DWORD	MCICDA_SetDoor(UINT wDevID, BOOL open)
{
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);
    DWORD               br;

    TRACE("(%04x, %s) !\n", wDevID, (open) ? "OPEN" : "CLOSE");

    if (wmcda == NULL) return MCIERR_INVALID_DEVICE_ID;

    if (!DeviceIoControl(wmcda->handle,
                         (open) ? IOCTL_STORAGE_EJECT_MEDIA : IOCTL_STORAGE_LOAD_MEDIA,
                         NULL, 0, NULL, 0, &br, NULL))
	return MCIERR_HARDWARE;

    return 0;
}

/**************************************************************************
 * 				MCICDA_Set			[internal]
 */
static DWORD MCICDA_Set(UINT wDevID, DWORD dwFlags, LPMCI_SET_PARMS lpParms)
{
    WINE_MCICDAUDIO*	wmcda = MCICDA_GetOpenDrv(wDevID);

    TRACE("(%04X, %08lX, %p);\n", wDevID, dwFlags, lpParms);

    if (wmcda == NULL)	return MCIERR_INVALID_DEVICE_ID;

    if (dwFlags & MCI_SET_DOOR_OPEN) {
	MCICDA_SetDoor(wDevID, TRUE);
    }
    if (dwFlags & MCI_SET_DOOR_CLOSED) {
	MCICDA_SetDoor(wDevID, FALSE);
    }

    /* only functions which require valid lpParms below this line ! */
    if (lpParms == NULL) return MCIERR_NULL_PARAMETER_BLOCK;
    /*
      TRACE("dwTimeFormat=%08lX\n", lpParms->dwTimeFormat);
      TRACE("dwAudio=%08lX\n", lpParms->dwAudio);
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
	    WARN("bad time format !\n");
	    return MCIERR_BAD_TIME_FORMAT;
	}
	wmcda->dwTimeFormat = lpParms->dwTimeFormat;
    }
    if (dwFlags & MCI_SET_VIDEO) return MCIERR_UNSUPPORTED_FUNCTION;
    if (dwFlags & MCI_SET_ON) return MCIERR_UNSUPPORTED_FUNCTION;
    if (dwFlags & MCI_SET_OFF) return MCIERR_UNSUPPORTED_FUNCTION;
    if (dwFlags & MCI_NOTIFY) {
	TRACE("MCI_NOTIFY_SUCCESSFUL %08lX !\n",
	      lpParms->dwCallback);
	mciDriverNotify((HWND)LOWORD(lpParms->dwCallback),
			wmcda->wNotifyDeviceID, MCI_NOTIFY_SUCCESSFUL);
    }
    return 0;
}

/**************************************************************************
 * 			DriverProc (MCICDA.@)
 */
LONG CALLBACK	MCICDA_DriverProc(DWORD dwDevID, HDRVR hDriv, DWORD wMsg,
				      DWORD dwParam1, DWORD dwParam2)
{
    switch(wMsg) {
    case DRV_LOAD:		return 1;
    case DRV_FREE:		return 1;
    case DRV_OPEN:		return MCICDA_drvOpen((LPSTR)dwParam1, (LPMCI_OPEN_DRIVER_PARMSA)dwParam2);
    case DRV_CLOSE:		return MCICDA_drvClose(dwDevID);
    case DRV_ENABLE:		return 1;
    case DRV_DISABLE:		return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "MCI audio CD driver !", "Wine Driver", MB_OK); return 1;
    case DRV_INSTALL:		return DRVCNF_RESTART;
    case DRV_REMOVE:		return DRVCNF_RESTART;
    }

    if (dwDevID == 0xFFFFFFFF) return MCIERR_UNSUPPORTED_FUNCTION;

    switch (wMsg) {
    case MCI_OPEN_DRIVER:	return MCICDA_Open(dwDevID, dwParam1, (LPMCI_OPEN_PARMSA)dwParam2);
    case MCI_CLOSE_DRIVER:	return MCICDA_Close(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_GETDEVCAPS:	return MCICDA_GetDevCaps(dwDevID, dwParam1, (LPMCI_GETDEVCAPS_PARMS)dwParam2);
    case MCI_INFO:		return MCICDA_Info(dwDevID, dwParam1, (LPMCI_INFO_PARMSA)dwParam2);
    case MCI_STATUS:		return MCICDA_Status(dwDevID, dwParam1, (LPMCI_STATUS_PARMS)dwParam2);
    case MCI_SET:		return MCICDA_Set(dwDevID, dwParam1, (LPMCI_SET_PARMS)dwParam2);
    case MCI_PLAY:		return MCICDA_Play(dwDevID, dwParam1, (LPMCI_PLAY_PARMS)dwParam2);
    case MCI_STOP:		return MCICDA_Stop(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_PAUSE:		return MCICDA_Pause(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_RESUME:		return MCICDA_Resume(dwDevID, dwParam1, (LPMCI_GENERIC_PARMS)dwParam2);
    case MCI_SEEK:		return MCICDA_Seek(dwDevID, dwParam1, (LPMCI_SEEK_PARMS)dwParam2);
    /* FIXME: I wonder if those two next items are really called ? */
    case MCI_SET_DOOR_OPEN:	FIXME("MCI_SET_DOOR_OPEN called. Please report this.\n");
				return MCICDA_SetDoor(dwDevID, TRUE);
    case MCI_SET_DOOR_CLOSED:	FIXME("MCI_SET_DOOR_CLOSED called. Please report this.\n");
				return MCICDA_SetDoor(dwDevID, FALSE);
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
	FIXME("Unsupported yet command [%lu]\n", wMsg);
	break;
    /* commands that should report an error */
    case MCI_WINDOW:
	TRACE("Unsupported command [%lu]\n", wMsg);
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
