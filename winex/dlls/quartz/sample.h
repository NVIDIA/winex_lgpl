#ifndef	WINE_DSHOW_SAMPLE_H
#define	WINE_DSHOW_SAMPLE_H

/*
		implements CMemMediaSample.

	- At least, the following interfaces should be implemented:

	IUnknown - IMediaSample - IMediaSample2

 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

typedef struct CMemMediaSample
{
	ICOM_VFIELD(IMediaSample2);

	/* IUnknown fields */
	LONG	ref;
	/* IMediaSample2 fields */
	IMemAllocator*	pOwner; /* not addref-ed. */
	BOOL	fMediaTimeIsValid;
	LONGLONG	llMediaTimeStart;
	LONGLONG	llMediaTimeEnd;
	AM_SAMPLE2_PROPERTIES	prop;
} CMemMediaSample;


HRESULT QUARTZ_CreateMemMediaSample(
	BYTE* pbData, DWORD dwDataLength,
	IMemAllocator* pOwner,
	CMemMediaSample** ppSample );
void QUARTZ_DestroyMemMediaSample(
	CMemMediaSample* pSample );


HRESULT QUARTZ_IMediaSample_GetProperties(
	IMediaSample* pSample,
	AM_SAMPLE2_PROPERTIES* pProp );
HRESULT QUARTZ_IMediaSample_SetProperties(
	IMediaSample* pSample,
	const AM_SAMPLE2_PROPERTIES* pProp );
HRESULT QUARTZ_IMediaSample_Copy(
	IMediaSample* pDstSample,
	IMediaSample* pSrcSample,
	BOOL bCopyData );



#endif	/* WINE_DSHOW_SAMPLE_H */
