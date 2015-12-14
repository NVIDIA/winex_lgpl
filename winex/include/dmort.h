/*
 * Copyright (C) 2002 Hidenori Takeshima
 */

#ifndef __WINE_DMORT_H_
#define __WINE_DMORT_H_


HRESULT WINAPI MoCopyMediaType( DMO_MEDIA_TYPE* pmtDst, const DMO_MEDIA_TYPE* pmtSrc );
HRESULT WINAPI MoCreateMediaType( DMO_MEDIA_TYPE** ppmt, DWORD cbFormat );
HRESULT WINAPI MoDeleteMediaType( DMO_MEDIA_TYPE* pmt );
HRESULT WINAPI MoDuplicateMediaType( DMO_MEDIA_TYPE** ppmtDest, const DMO_MEDIA_TYPE* pmtSrc );
HRESULT WINAPI MoFreeMediaType( DMO_MEDIA_TYPE* pmt );
HRESULT WINAPI MoInitMediaType( DMO_MEDIA_TYPE* pmt, DWORD cbFormat );


#endif	/* __WINE_DMORT_H_ */
