/*
 * Copyright (C) 2002 Hidenori Takeshima
 */

#ifndef __WINE_MEDIAOBJ_H_
#define __WINE_MEDIAOBJ_H_

typedef struct IDMOQualityControl IDMOQualityControl;
typedef struct IDMOVideoOutputOptimizations IDMOVideoOutputOptimizations;
typedef struct IEnumDMO IEnumDMO;
typedef struct IMediaBuffer IMediaBuffer;
typedef struct IMediaObject IMediaObject;
typedef struct IMediaObjectInPlace IMediaObjectInPlace;



typedef struct
{
	GUID	majortype;
	GUID	subtype;
	BOOL	bFixedSizeSamples;
	BOOL	bTemporalCompression;
	ULONG	lSampleSize;
	GUID	formattype;
	IUnknown*	pUnk;
	ULONG	cbFormat;
	BYTE*	pbFormat;
} DMO_MEDIA_TYPE;



#endif	/* __WINE_MEDIAOBJ_H_ */
