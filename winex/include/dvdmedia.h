/*
 * Copyright (C) 2002 Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 */

#ifndef __WINE_DVDMEDIA_H_
#define __WINE_DVDMEDIA_H_


enum AM_MPEG2Level
{
	AM_MPEG2Level_Low = 1,
	AM_MPEG2Level_Main = 2,
	AM_MPEG2Level_High1440 = 3,
	AM_MPEG2Level_High = 4,
};

enum AM_MPEG2Profile
{
	AM_MPEG2Profile_Simple = 1,
	AM_MPEG2Profile_Main = 2,
	AM_MPEG2Profile_SNRScalable = 3,
	AM_MPEG2Profile_SpatiallyScalable = 4,
	AM_MPEG2Profile_High = 5,
};

typedef struct
{
	RECT			rcSource;
	RECT			rcTarget;
	DWORD			dwBitRate;
	DWORD			dwBitErrorRate;
	REFERENCE_TIME		AvgTimePerFrame;
	DWORD			dwInterlaceFlags;
	DWORD			dwCopyProtectFlags;
	DWORD			dwPictAspectRatioX;
	DWORD			dwPictAspectRatioY;
	DWORD			dwReserved1;
	DWORD			dwReserved2;
	BITMAPINFOHEADER	bmiHeader;
} VIDEOINFOHEADER2;

typedef struct
{
	VIDEOINFOHEADER2	hdr;
	DWORD			dwStartTimeCode;
	DWORD			cbSequenceHeader;
	DWORD			dwProfile;
	DWORD			dwLevel;
	DWORD			dwFlags;
	DWORD			dwSequenceHeader[1];
} MPEG2VIDEOINFO;


#define AMINTERLACE_IsInterlaced		0x00000001
#define AMINTERLACE_1FieldPerSample		0x00000002
#define AMINTERLACE_Field1First			0x00000004

#define AMINTERLACE_FieldPatternMask		0x00000030
#define AMINTERLACE_FieldPatField1Only		0x00000000
#define AMINTERLACE_FieldPatField2Only		0x00000010
#define AMINTERLACE_FieldPatBothRegular		0x00000020
#define AMINTERLACE_FieldPatBothIrregular	0x00000030

#define AMINTERLACE_DisplayModeMask		0x000000C0
#define AMINTERLACE_DisplayModeBobOnly		0x00000000
#define AMINTERLACE_DisplayModeWeaveOnly	0x00000040
#define AMINTERLACE_DisplayModeBobOrWeave	0x00000080

#define AMCOPYPROTECT_RestrictDuplication	0x1

#define AMMPEG2_DoPanScan		0x00000001
#define AMMPEG2_DVDLine21Field1		0x00000002
#define AMMPEG2_DVDLine21Field2		0x00000004
#define AMMPEG2_SourceIsLetterboxed	0x00000008
#define AMMPEG2_FilmCameraMode		0x00000010
#define AMMPEG2_LetterboxAnalogOut	0x00000020



#endif  /* __WINE_DVDMEDIA_H_ */
