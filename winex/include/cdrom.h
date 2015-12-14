/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * Header file for CD-ROM support
 *
 * Copyright 1994 Martin Ayotte
 * Copyright 1999 Eric Pouech
 * Copyright 2000 Andreas Mohr
 */

#ifndef __WINE_CDROM_H__
#define __WINE_CDROM_H__

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <stdlib.h>
#include <unistd.h>
#include "windef.h"
#include "wine/windef16.h"

#ifdef HAVE_LINUX_CDROM_H
# include <linux/cdrom.h>
#endif
#ifdef HAVE_LINUX_UCDROM_H
# include <linux/ucdrom.h>
#endif
#ifdef HAVE_SYS_CDIO_H
# include <sys/cdio.h>
#endif

typedef struct {
    const char			*devname;
#if defined(linux)
    struct cdrom_subchnl	sc;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    struct cd_sub_channel_info	sc;
#endif
    /* those data reflect the cdaudio structure and
     * don't change while playing
     */
    BOOL			bTrackInfoGood;
    UINT16			nTracks;
    UINT16			nFirstTrack;
    UINT16			nLastTrack;
    LPDWORD			lpdwTrackLen;
    LPDWORD			lpdwTrackPos;
    LPBYTE			lpbTrackFlags;
    DWORD			dwFirstFrame;
    DWORD			dwLastFrame;
    /* those data change while playing */
    int				cdaMode;
    UINT16			nCurTrack;
    DWORD			dwCurFrame;
} WINE_CDAUDIO;

#define	WINE_CDA_DONTKNOW		0x00
#define	WINE_CDA_NOTREADY		0x01
#define	WINE_CDA_OPEN			0x02
#define	WINE_CDA_PLAY			0x03
#define	WINE_CDA_STOP			0x04
#define	WINE_CDA_PAUSE			0x05

int	CDROM_Open(WINE_CDAUDIO* wcda, int drive);
int	CDROM_OpenDev(WINE_CDAUDIO* wcda);
int	CDROM_GetMediaType(WINE_CDAUDIO* wcda, int parentdev);
int	CDROM_CloseDev(int dev);
int	CDROM_Close(WINE_CDAUDIO* wcda);
int	CDROM_Reset(WINE_CDAUDIO* wcda, int parentdev);
int	CDROM_Audio_Play(WINE_CDAUDIO* wcda, DWORD start, DWORD stop, int parentdev);
int	CDROM_Audio_Stop(WINE_CDAUDIO* wcda, int parentdev);
int	CDROM_Audio_Pause(WINE_CDAUDIO* wcda, int pauseOn, int parentdev);
int	CDROM_Audio_Seek(WINE_CDAUDIO* wcda, DWORD at, int parentdev);
int	CDROM_SetDoor(WINE_CDAUDIO* wcda, int open, int parentdev);
UINT16 	CDROM_Audio_GetNumberOfTracks(WINE_CDAUDIO* wcda, int parentdev);
BOOL 	CDROM_Audio_GetTracksInfo(WINE_CDAUDIO* wcda, int parentdev);
BOOL	CDROM_Audio_GetCDStatus(WINE_CDAUDIO* wcda, int parentdev);
WORD	CDROM_Data_FindBestVoldesc(int fd);
DWORD	CDROM_Audio_GetSerial(WINE_CDAUDIO* wcda);
DWORD	CDROM_Data_GetSerial(WINE_CDAUDIO* wcda, int parentdev);
DWORD	CDROM_GetSerial(int drive);
DWORD	CDROM_GetLabel(int drive, char *label);
BOOL CDROM_ReadSector(int drive, LPVOID buffer, DWORD sector_number,
		      DWORD sector_count);
BOOL CDROM_ReadRawSector(int drive, LPVOID buffer, DWORD sector_number,
			 DWORD sector_count);

#define CDFRAMES_PERSEC 		75
#define SECONDS_PERMIN	 		60
#define CDFRAMES_PERMIN 		((CDFRAMES_PERSEC) * (SECONDS_PERMIN))

#ifndef CDROM_DATA_TRACK
#define CDROM_DATA_TRACK 0x04
#endif

#define CDROM_MSF_MINUTE(msf)           ((BYTE)(msf))
#define CDROM_MSF_SECOND(msf)           ((BYTE)(((WORD)(msf)) >> 8))
#define CDROM_MSF_FRAME(msf)            ((BYTE)((msf)>>16))

#define CDROM_MAKE_MSF(m, s, f)         ((DWORD)(((BYTE)(m) | \
                                                  ((WORD)(s)<<8)) | \
                                                 (((DWORD)(BYTE)(f))<<16)))

/* values borrowed from Linux 2.2.x cdrom.h */
#define CDS_NO_INFO			0
#define CDS_AUDIO			100
#define CDS_DATA_1                      101
#define CDS_DATA_2                      102
#define CDS_XA_2_1                      103
#define CDS_XA_2_2                      104
#define CDS_MIXED                       105

#endif

