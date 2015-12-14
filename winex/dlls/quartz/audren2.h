/*
 * Audio Renderer (CLSID_DSoundRender)
 *
 * FIXME
 *  - implements IRefereneceClock.
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
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

#ifndef	WINE_DSHOW_AUDREN2_H
#define	WINE_DSHOW_AUDREN2_H

#include "iunk.h"
#include "basefilt.h"
#include "seekpass.h"

#include "dsound.h"

#define WINE_QUARTZ_WAVEOUT_COUNT	4

typedef struct CDSoundRendererImpl	CDSoundRendererImpl;
typedef struct CDSoundRendererPinImpl	CDSoundRendererPinImpl;


typedef struct DSRen_IBasicAudioImpl
{
	ICOM_VFIELD(IBasicAudio);
} DSRen_IBasicAudioImpl;

struct CDSoundRendererImpl
{
	QUARTZ_IUnkImpl	unk;
	CBaseFilterImpl	basefilter;
	DSRen_IBasicAudioImpl	basaud;
	QUARTZ_IFDelegation	qiext;

	CSeekingPassThru*	pSeekPass;
	CDSoundRendererPinImpl* pPin;

	CRITICAL_SECTION	m_csReceive;
	CRITICAL_SECTION	m_csTime;
	REFERENCE_TIME		m_rtLastPos;
	REFERENCE_TIME		m_rtLastTime;
	BOOL	m_fInFlush;

	/* for directsound */
	LPDIRECTSOUND		m_dsound;
	LPDIRECTSOUNDBUFFER	m_dsbuffer;
	LPDIRECTSOUNDBUFFER	m_dsprimary;
	WAVEFORMATEX		m_wforig;
	DWORD			m_bufsize;
	DWORD			m_wpos, m_wlen, m_ppos;
	BOOL			m_recover;
	long		m_lAudioVolume;
	long		m_lAudioBalance;
	HANDLE		m_hEventRender;
	DWORD		m_SavedPlayPos;
	LONGLONG	m_BytesPlayed;

	IUnknown*	m_refClock;
};

struct CDSoundRendererPinImpl
{
	QUARTZ_IUnkImpl	unk;
	CPinBaseImpl	pin;
	CMemInputPinBaseImpl	meminput;

	CDSoundRendererImpl* pRender;
};

#define	CDSoundRendererImpl_THIS(iface,member)	CDSoundRendererImpl*	This = ((CDSoundRendererImpl*)(((char*)iface)-offsetof(CDSoundRendererImpl,member)))
#define	CDSoundRendererPinImpl_THIS(iface,member)	CDSoundRendererPinImpl*	This = ((CDSoundRendererPinImpl*)(((char*)iface)-offsetof(CDSoundRendererPinImpl,member)))


HRESULT CDSoundRendererImpl_InitIBasicAudio( CDSoundRendererImpl* This );
void CDSoundRendererImpl_UninitIBasicAudio( CDSoundRendererImpl* This );

HRESULT QUARTZ_CreateDSoundRenderer(IUnknown* punkOuter,void** ppobj);
HRESULT QUARTZ_CreateDSoundRendererPin(
        CDSoundRendererImpl* pFilter,
        CRITICAL_SECTION* pcsPin,
	CRITICAL_SECTION* pcsPinReceive,
        CDSoundRendererPinImpl** ppPin);



#endif	/* WINE_DSHOW_AUDREN2_H */
