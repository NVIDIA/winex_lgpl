/*
 * Implements IBaseFilter for transform filters. (internal)
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 * Copyright (c) 2013-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef	WINE_DSHOW_XFORM_H
#define	WINE_DSHOW_XFORM_H

#include "iunk.h"
#include "basefilt.h"
#include "seekpass.h"


typedef struct CTransformBaseImpl CTransformBaseImpl;
typedef struct CTransformBaseInPinImpl CTransformBaseInPinImpl;
typedef struct CTransformBaseOutPinImpl CTransformBaseOutPinImpl;
typedef struct TransformBaseHandlers	TransformBaseHandlers;

/* {301056D0-6DFF-11D2-9EEB-006008039E37} */
DEFINE_GUID(CLSID_quartzMJPGDecompressor,
0x301056D0,0x6DFF,0x11D2,0x9E,0xEB,0x00,0x60,0x08,0x03,0x9E,0x37);
/* {FDFE9681-74A3-11D0-AFA7-00AA00B67A42} */
DEFINE_GUID(CLSID_quartzQuickTimeDecompressor,
0xFDFE9681,0x74A3,0x11D0,0xAF,0xA7,0x00,0xAA,0x00,0xB6,0x7A,0x42);
DEFINE_GUID(CLSID_quartzFFMAWrapper,
0xABBABDB2,0xAB05,0x11D6,0x80,0x64,0x00,0x50,0x04,0xE9,0x6E,0xAE);
DEFINE_GUID(CLSID_quartzFFMVWrapper,
0x9602BE56,0xABC4,0x11D6,0x9F,0xD9,0x00,0x50,0x04,0xE9,0x6E,0xAE);


struct CTransformBaseImpl
{
	QUARTZ_IUnkImpl	unk;
	CBaseFilterImpl	basefilter;

	CTransformBaseInPinImpl*	pInPin;
	CTransformBaseOutPinImpl*	pOutPin;
	CSeekingPassThru*	pSeekPass;

	CRITICAL_SECTION	csReceive;
	IMemAllocator*	m_pOutPinAllocator;
	BOOL	m_bPreCopy; /* sample must be copied */
	BOOL	m_bReuseSample; /* sample must be reused */
	BOOL	m_bInFlush;
	IMediaSample*	m_pSample;

	BOOL	m_bFiltering;
	const TransformBaseHandlers*	m_pHandler;
	void*	m_pUserData;
};

struct CTransformBaseInPinImpl
{
	QUARTZ_IUnkImpl	unk;
	CPinBaseImpl	pin;
	CMemInputPinBaseImpl	meminput;

	CTransformBaseImpl*	pFilter;
};

struct CTransformBaseOutPinImpl
{
	QUARTZ_IUnkImpl	unk;
	CPinBaseImpl	pin;
	CQualityControlPassThruImpl	qcontrol;
	QUARTZ_IFDelegation	qiext;

	CTransformBaseImpl*	pFilter;
};

struct TransformBaseHandlers
{
	/* all methods must be implemented */

	HRESULT (*pInit)( CTransformBaseImpl* pImpl );
	HRESULT (*pCleanup)( CTransformBaseImpl* pImpl );

	/* pmtOut may be NULL */
	HRESULT (*pCheckMediaType)( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut );
	/* get output types */
	HRESULT (*pGetOutputTypes)( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE** ppmtAcceptTypes, ULONG* pcAcceptTypes );
	/* get allocator properties */
	HRESULT (*pGetAllocProp)( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut, ALLOCATOR_PROPERTIES* pProp, BOOL* pbTransInPlace, BOOL* pbTryToReuseSample );

	/* prepare the filter */
	HRESULT (*pBeginTransform)( CTransformBaseImpl* pImpl, const AM_MEDIA_TYPE* pmtIn, const AM_MEDIA_TYPE* pmtOut, BOOL bReuseSample );
	/* process a sample */
	HRESULT (*pProcessReceive)( CTransformBaseImpl* pImpl, IMediaSample* pSampIn ); /* override Transform */
	HRESULT (*pTransform)( CTransformBaseImpl* pImpl, IMediaSample* pSampIn, IMediaSample* pSampOut );
	/* unprepare the filter */
	HRESULT (*pEndTransform)( CTransformBaseImpl* pImpl );
};

#define	CTransformBaseImpl_THIS(iface,member)	CTransformBaseImpl*	This = ((CTransformBaseImpl*)(((char*)iface)-offsetof(CTransformBaseImpl,member)))
#define	CTransformBaseInPinImpl_THIS(iface,member)	CTransformBaseInPinImpl*	This = ((CTransformBaseInPinImpl*)(((char*)iface)-offsetof(CTransformBaseInPinImpl,member)))
#define	CTransformBaseOutPinImpl_THIS(iface,member)	CTransformBaseOutPinImpl*	This = ((CTransformBaseOutPinImpl*)(((char*)iface)-offsetof(CTransformBaseOutPinImpl,member)))


HRESULT QUARTZ_CreateTransformBase(
	IUnknown* punkOuter,void** ppobj,
	const CLSID* pclsidTransformBase,
	LPCWSTR pwszTransformBaseName,
	LPCWSTR pwszInPinName,
	LPCWSTR pwszOutPinName,
	const TransformBaseHandlers* pHandler );
HRESULT QUARTZ_CreateTransformBaseInPin(
	CTransformBaseImpl* pFilter,
	CRITICAL_SECTION* pcsPin,
	CRITICAL_SECTION* pcsPinReceive,
	CTransformBaseInPinImpl** ppPin,
	LPCWSTR pwszPinName );
HRESULT QUARTZ_CreateTransformBaseOutPin(
	CTransformBaseImpl* pFilter,
	CRITICAL_SECTION* pcsPin,
	CTransformBaseOutPinImpl** ppPin,
	LPCWSTR pwszPinName );


HRESULT QUARTZ_CreateAVIDec(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateAVIDraw(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateColour(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateACMWrapper(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateCMpegAudioCodec(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateCMpegVideoCodec(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateMJPGDecompressor(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateQuickTimeDecompressor(IUnknown* punkOuter,void** ppobj);


#endif	/* WINE_DSHOW_XFORM_H */
