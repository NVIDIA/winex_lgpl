/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright 2000-2002 TransGaming Technologies, Inc.
 */
/*
 * Most thread locking is complete. There may be a few race
 * conditions still lurking.
 *
 * Tested with a Soundblaster clone, a Gravis UltraSound Classic,
 * and a Turtle Beach Tropez+.
 *
 * TODO:
 *	Implement SetCooperativeLevel properly (need to address focus issues)
 *	Implement DirectSound3DBuffers (stubs in place)
 *	Use hardware 3D support if available
 *      Add critical section locking inside Release and AddRef methods
 *      Handle static buffers - put those in hardware, non-static not in hardware
 *      Hardware DuplicateSoundBuffer
 *      Proper volume calculation, and setting volume in HEL primary buffer
 *      Optimize WINMM and negotiate fragment size, decrease DS_HEL_MARGIN
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>	/* Insomnia - pow() function */

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "mmsystem.h"
#include "winternl.h"
#include "mmddk.h"
#include "wine/windef16.h"
#include "wine/debug.h"
#include "dsound.h"
#include "dsdriver.h"
#include "dsound_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dsound);


IDirectSoundImpl*	dsound = NULL;
HANDLE			dsound_heap;
CRITICAL_SECTION	dsound_crit;
BOOL bMmxExtensionsAvailable = FALSE;

static void create_cs(void)
{
    InitializeCriticalSection( &dsound_crit );
    CRITICAL_SECTION_NAME( &dsound_crit, "dsound" );
}

static void destroy_cs(void)
{
    DeleteCriticalSection( &dsound_crit );
}


/***************************************************************************
 * DirectSoundEnumerateA [DSOUND.2]
 *
 * Enumerate all DirectSound drivers installed in the system
 *
 * RETURNS
 *    Success: DS_OK
 *    Failure: DSERR_INVALIDPARAM
 */
HRESULT WINAPI DirectSoundEnumerateA(
	LPDSENUMCALLBACKA lpDSEnumCallback,
	LPVOID lpContext)
{
	TRACE("lpDSEnumCallback = %p, lpContext = %p\n",
		lpDSEnumCallback, lpContext);

	if (lpDSEnumCallback != NULL) {
		if (lpDSEnumCallback(NULL, "WINE DirectSound",
				     "sound", lpContext))
		    lpDSEnumCallback((LPGUID)&DSDEVID_DefaultPlayback, "Default Playback Device",
				     "sound", lpContext);
	}

	return DS_OK;
}

/***************************************************************************
 * DirectSoundEnumerateW [DSOUND.3]
 *
 * Enumerate all DirectSound drivers installed in the system
 *
 * RETURNS
 *    Success: DS_OK
 *    Failure: DSERR_INVALIDPARAM
 */
static WCHAR WTGDS[] = {'T', 'G', ' ', 'D', 'i', 'r', 'e', 'c', 't', 'S', 'o',
                        'u', 'n', 'd', 0};
static WCHAR WSound[] = {'s', 'o', 'u', 'n', 'd', 0};
static WCHAR WDPD[] = {'D', 'e', 'f', 'a', 'u', 'l', 't', ' ', 'P', 'l', 'a',
                       'y', 'b', 'a', 'c', 'k', ' ', 'D', 'e', 'v', 'i', 'c',
                       'e', 0};

HRESULT WINAPI DirectSoundEnumerateW(
	LPDSENUMCALLBACKW lpDSEnumCallback,
	LPVOID lpContext )
{
   TRACE ("lpDSEnumCallback = %p, lpContext = %p\n", lpDSEnumCallback,
          lpContext);

   if (lpDSEnumCallback != NULL) {
      if (lpDSEnumCallback (NULL, WTGDS, WSound, lpContext))
         lpDSEnumCallback ((LPGUID)&DSDEVID_DefaultPlayback, WDPD,
                           WSound, lpContext);
   }

   return DS_OK;
}


static void _dump_DSBCAPS(DWORD xmask) {
	struct {
		DWORD	mask;
		char	*name;
	} flags[] = {
#define FE(x) { x, #x },
		FE(DSBCAPS_PRIMARYBUFFER)
		FE(DSBCAPS_STATIC)
		FE(DSBCAPS_LOCHARDWARE)
		FE(DSBCAPS_LOCSOFTWARE)
		FE(DSBCAPS_CTRL3D)
		FE(DSBCAPS_CTRLFREQUENCY)
		FE(DSBCAPS_CTRLPAN)
		FE(DSBCAPS_CTRLFX)
		FE(DSBCAPS_CTRLVOLUME)
		FE(DSBCAPS_CTRLPOSITIONNOTIFY)
		FE(DSBCAPS_CTRLDEFAULT)
		FE(DSBCAPS_CTRLALL)
		FE(DSBCAPS_STICKYFOCUS)
		FE(DSBCAPS_GLOBALFOCUS)
		FE(DSBCAPS_GETCURRENTPOSITION2)
		FE(DSBCAPS_MUTE3DATMAXDISTANCE)
#undef FE
	};
	int	i;

	for (i=0;i<sizeof(flags)/sizeof(flags[0]);i++)
		if ((flags[i].mask & xmask) == flags[i].mask)
			DPRINTF("%s ",flags[i].name);
}

/*******************************************************************************
 *		IDirectSound
 */

static HRESULT WINAPI IDirectSoundImpl_SetCooperativeLevel(
	LPDIRECTSOUND8 iface,HWND hwnd,DWORD level
) {
	ICOM_THIS(IDirectSoundImpl,iface);

	FIXME("(%p,%08lx,%ld):stub\n",This,(DWORD)hwnd,level);

	This->priolevel = level;

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundImpl_CreateSoundBuffer(
	LPDIRECTSOUND8 iface, LPCDSBUFFERDESC dsbd, LPLPDIRECTSOUNDBUFFER ppdsb, LPUNKNOWN lpunk
) {
	ICOM_THIS(IDirectSoundImpl,iface);
	LPWAVEFORMATEX	wfex;

	TRACE("(%p,%p,%p,%p)\n",This,dsbd,ppdsb,lpunk);

	if ((This == NULL) || (dsbd == NULL) || (ppdsb == NULL))
		return DSERR_INVALIDPARAM;

	if (TRACE_ON(dsound)) {
		TRACE("(structsize=%ld)\n",dsbd->dwSize);
		TRACE("(flags=0x%08lx:\n",dsbd->dwFlags);
		_dump_DSBCAPS(dsbd->dwFlags);
		DPRINTF(")\n");
		TRACE("(bufferbytes=%ld)\n",dsbd->dwBufferBytes);
		TRACE("(lpwfxFormat=%p)\n",dsbd->lpwfxFormat);
		if( dsbd->dwSize >= 36 ) 
		   TRACE("(guid3DAlgorithm=%s\n", debugstr_guid(&dsbd->guid3DAlgorithm));
	}

	wfex = dsbd->lpwfxFormat;

	if (wfex)
		TRACE("(formattag=0x%04x,chans=%d,samplerate=%ld,"
		   "bytespersec=%ld,blockalign=%d,bitspersamp=%d,cbSize=%d)\n",
		   wfex->wFormatTag, wfex->nChannels, wfex->nSamplesPerSec,
		   wfex->nAvgBytesPerSec, wfex->nBlockAlign,
		   wfex->wBitsPerSample, wfex->cbSize);
	
	/* If DSBCAPS_CTRL3D is not set in dwFlags, this member must be GUID_NULL (DS3DALG_DEFAULT) */
	if ((dsbd->dwSize >= 36) && !(dsbd->dwFlags & DSBCAPS_CTRL3D) &&
	    !IsEqualGUID(&GUID_NULL, &dsbd->guid3DAlgorithm))
		return DSERR_INVALIDPARAM;

	if (dsbd->dwFlags & DSBCAPS_PRIMARYBUFFER)
		return PrimaryBuffer_Create(This, (PrimaryBufferImpl**)ppdsb, dsbd);
	else
		return SecondaryBuffer_Create(This, (IDirectSoundBufferImpl**)ppdsb, dsbd);
}

static HRESULT WINAPI IDirectSoundImpl_DuplicateSoundBuffer(
	LPDIRECTSOUND8 iface,LPDIRECTSOUNDBUFFER pdsb,LPLPDIRECTSOUNDBUFFER ppdsb
) {
	ICOM_THIS(IDirectSoundImpl,iface);
	IDirectSoundBufferImpl* ipdsb=(IDirectSoundBufferImpl*)pdsb;
	IDirectSoundBufferImpl** ippdsb=(IDirectSoundBufferImpl**)ppdsb;
	TRACE("(%p,%p,%p)\n",This,ipdsb,ippdsb);

	if ((pdsb == NULL) || (ppdsb == NULL))
		return DSERR_INVALIDPARAM;
        
	if (ipdsb->dsbd.dwFlags & DSBCAPS_PRIMARYBUFFER) {
		ERR("trying to duplicate primary buffer\n");
		return DSERR_INVALIDCALL;
	}

	if (ipdsb->hwbuf) {
		FIXME("need to duplicate hardware buffer\n");
	}

	*ippdsb = (IDirectSoundBufferImpl*)HeapAlloc(dsound_heap,HEAP_ZERO_MEMORY,sizeof(IDirectSoundBufferImpl));

	IDirectSoundBuffer8_AddRef(pdsb);
	memcpy(*ippdsb, ipdsb, sizeof(IDirectSoundBufferImpl));
	(*ippdsb)->ref = 1;
	(*ippdsb)->aggref = 1;
	(*ippdsb)->state = STATE_STOPPED;
	(*ippdsb)->playpos = 0;
	(*ippdsb)->buf_mixpos = 0;
	(*ippdsb)->dsound = This;
	(*ippdsb)->hwbuf = NULL;
	(*ippdsb)->ds3db = NULL; /* FIXME? */
	(*ippdsb)->iks = NULL; /* FIXME? */
	memcpy(&((*ippdsb)->wfx), &(ipdsb->wfx), sizeof((*ippdsb)->wfx));

	/* update the ref count on the shared software buffer */
	if (ipdsb->swbuf)
	    InterlockedIncrement((LONG *)&ipdsb->swbuf->ref);


	/* create a shared notification object for the new buffer */
	if ((ipdsb->dsbd.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) && ipdsb->positions)
	    IDirectSoundNotifyImpl_Duplicate(ipdsb, (*ippdsb));

	else
	    (*ippdsb)->positions = NULL;


	InitializeCriticalSection(&(*ippdsb)->lock);

	if (ipdsb->dsbd.dwFlags & DSBCAPS_CTRL3D) {
		IDirectSound3DBufferImpl** ippds3db = &(*ippdsb)->ds3db;
		*ippds3db = (IDirectSound3DBufferImpl*)HeapAlloc(dsound_heap,0,sizeof(IDirectSound3DBufferImpl));
		memcpy(*ippds3db, ipdsb->ds3db, sizeof(IDirectSound3DBufferImpl));
		(*ippds3db)->ref = 0;
		(*ippds3db)->dsb = (*ippdsb);
		InitializeCriticalSection(&(*ippds3db)->lock);
	}

	/* register buffer */
	RtlAcquireResourceExclusive(&(This->lock), TRUE);
	{
		IDirectSoundBufferImpl **newbuffers = (IDirectSoundBufferImpl**)HeapReAlloc(dsound_heap,0,This->buffers,sizeof(IDirectSoundBufferImpl**)*(This->nrofbuffers+1));
		if (newbuffers) {
			This->buffers = newbuffers;
			This->buffers[This->nrofbuffers] = *ippdsb;
			This->nrofbuffers++;
			TRACE("buffer count is now %d\n", This->nrofbuffers);
		} else {
			ERR("out of memory for buffer list! Current buffer count is %d\n", This->nrofbuffers);
			/* FIXME: release buffer */
		}
	}
	RtlReleaseResource(&(This->lock));
	IDirectSound_AddRef(iface);
	IDirectSoundBuffer8_Release(pdsb);
	return DS_OK;
}


static HRESULT WINAPI IDirectSoundImpl_GetCaps(LPDIRECTSOUND8 iface,LPDSCAPS caps) {
	ICOM_THIS(IDirectSoundImpl,iface);
	TRACE("(%p,%p)\n",This,caps);

	if (caps == NULL)
		return DSERR_INVALIDPARAM;

	if (caps->dwSize < sizeof(*caps))
		return DSERR_INVALIDPARAM;

	TRACE("(flags=0x%08lx)\n",caps->dwFlags);

	/* We should check this value, not set it. See Inside DirectX, p215. */
	caps->dwSize = sizeof(*caps);

	caps->dwFlags = This->drvcaps.dwFlags;

	/* FIXME: copy caps from This->drvcaps */
	caps->dwMinSecondarySampleRate		= DSBFREQUENCY_MIN;
	caps->dwMaxSecondarySampleRate		= DSBFREQUENCY_MAX;

	caps->dwPrimaryBuffers			= 1;

	caps->dwMaxHwMixingAllBuffers		= 0;
	caps->dwMaxHwMixingStaticBuffers	= 0;
	caps->dwMaxHwMixingStreamingBuffers	= 0;

	caps->dwFreeHwMixingAllBuffers		= 0;
	caps->dwFreeHwMixingStaticBuffers	= 0;
	caps->dwFreeHwMixingStreamingBuffers	= 0;

	caps->dwMaxHw3DAllBuffers		= 0;
	caps->dwMaxHw3DStaticBuffers		= 0;
	caps->dwMaxHw3DStreamingBuffers		= 0;

	caps->dwFreeHw3DAllBuffers		= 0;
	caps->dwFreeHw3DStaticBuffers		= 0;
	caps->dwFreeHw3DStreamingBuffers	= 0;

	caps->dwTotalHwMemBytes			= 0;

	caps->dwFreeHwMemBytes			= 0;

	caps->dwMaxContigFreeHwMemBytes		= 0;

	caps->dwUnlockTransferRateHwBuffers	= 4096;	/* But we have none... */

	caps->dwPlayCpuOverheadSwBuffers	= 1;	/* 1% */

	return DS_OK;
}

static ULONG WINAPI IDirectSoundImpl_AddRef(LPDIRECTSOUND8 iface) {
	ICOM_THIS(IDirectSoundImpl,iface);
	return ++(This->ref);
}

static ULONG WINAPI IDirectSoundImpl_Release(LPDIRECTSOUND8 iface) {
	ICOM_THIS(IDirectSoundImpl,iface);
	TRACE("(%p), ref was %ld\n",This,This->ref);
	if (!--(This->ref)) {
		UINT i;

		EnterCriticalSection(&dsound_crit);
		timeKillEvent(This->timerID);
		timeEndPeriod(DS_TIME_RES);

		if (dsound == This)
			dsound = NULL;
		LeaveCriticalSection(&dsound_crit);

		/* wait until the mixer thread is done with us */
		RtlAcquireResourceExclusive(&This->lock, TRUE);
		/* ok, we're clear */

		if (This->buffers) {
			for( i=0;i<This->nrofbuffers;i++)
				IDirectSoundBuffer8_Release((LPDIRECTSOUNDBUFFER8)This->buffers[i]);
		}

		DSOUND_PrimaryDestroy(This);

		RtlReleaseResource(&This->lock);
		RtlDeleteResource(&This->lock);
		DeleteCriticalSection(&This->mixlock);
		if (This->driver) {
			IDsDriver_Close(This->driver);
		} else {
			unsigned c;
			for (c=0; c<DS_HEL_FRAGS; c++)
				HeapFree(dsound_heap,0,This->pwave[c]);
		}
		if (This->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMOPEN) {
			waveOutClose(This->hwo);
		}
		if (This->driver)
			IDsDriver_Release(This->driver);

		HeapFree(dsound_heap,0,This);
		return 0;
	}
	return This->ref;
}

static HRESULT WINAPI IDirectSoundImpl_SetSpeakerConfig(
	LPDIRECTSOUND8 iface,DWORD config
) {
	ICOM_THIS(IDirectSoundImpl,iface);
	FIXME("(%p,0x%08lx):stub\n",This,config);
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundImpl_QueryInterface(
	LPDIRECTSOUND8 iface,REFIID riid,LPVOID *ppobj
) {
	ICOM_THIS(IDirectSoundImpl,iface);
	TRACE("(%p)->(%s,%p)\n", iface, debugstr_guid(riid), ppobj);

	if ( IsEqualGUID( &IID_IDirectSound3DListener, riid ) ) {
	    ERR("app requested IDirectSound3DListener on dsound object\n");
	    *ppobj = NULL;
	    return E_FAIL;
	} else if (IsEqualGUID(&IID_IUnknown, riid) ||
		   IsEqualGUID(&IID_IDirectSound, riid) ||
		   IsEqualGUID(&IID_IDirectSound8, riid)) {
	    This->ref++;
	    *ppobj = This;
	    return S_OK;
	} else {
	    FIXME("(%p)->(%s): no interface", iface, debugstr_guid(riid));
	    *ppobj = NULL;
	    return E_NOINTERFACE;
	}
}

static HRESULT WINAPI IDirectSoundImpl_Compact(
	LPDIRECTSOUND8 iface)
{
	ICOM_THIS(IDirectSoundImpl,iface);
	TRACE("(%p)\n", This);
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundImpl_GetSpeakerConfig(
	LPDIRECTSOUND8 iface,
	LPDWORD lpdwSpeakerConfig)
{
	ICOM_THIS(IDirectSoundImpl,iface);

	if (lpdwSpeakerConfig == NULL)
		return DSERR_INVALIDPARAM;

	TRACE("(%p, %p)\n", This, lpdwSpeakerConfig);
	*lpdwSpeakerConfig = DSSPEAKER_STEREO | (DSSPEAKER_GEOMETRY_NARROW << 16);
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundImpl_Initialize(
	LPDIRECTSOUND8 iface,
	LPCGUID lpcGuid)
{
	ICOM_THIS(IDirectSoundImpl,iface);
	TRACE("(%p, %p)\n", This, lpcGuid);
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundImpl_VerifyCertification(
	LPDIRECTSOUND8 iface,
	LPDWORD pdwCertified)
{
	ICOM_THIS(IDirectSoundImpl,iface);
	TRACE("(%p, %p)\n", This, pdwCertified);
	*pdwCertified = DS_CERTIFIED;
	return DS_OK;
}

static ICOM_VTABLE(IDirectSound8) dsvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IDirectSoundImpl_QueryInterface,
	IDirectSoundImpl_AddRef,
	IDirectSoundImpl_Release,
	IDirectSoundImpl_CreateSoundBuffer,
	IDirectSoundImpl_GetCaps,
	IDirectSoundImpl_DuplicateSoundBuffer,
	IDirectSoundImpl_SetCooperativeLevel,
	IDirectSoundImpl_Compact,
	IDirectSoundImpl_GetSpeakerConfig,
	IDirectSoundImpl_SetSpeakerConfig,
	IDirectSoundImpl_Initialize,
	IDirectSoundImpl_VerifyCertification
};


/*******************************************************************************
 *		DirectSoundCreate (DSOUND.1)
 */
HRESULT WINAPI DirectSoundCreate8(REFGUID lpGUID,LPDIRECTSOUND8 *ppDS,IUnknown *pUnkOuter )
{
	IDirectSoundImpl** ippDS=(IDirectSoundImpl**)ppDS;
	PIDSDRIVER drv = NULL;
	WAVEOUTCAPSA wcaps;
	unsigned wod, wodn;
	HRESULT err = DS_OK;

	if (lpGUID)
		TRACE("(%p,%p,%p)\n",lpGUID,ippDS,pUnkOuter);
	else
		TRACE("DirectSoundCreate (%p)\n", ippDS);

	if (ippDS == NULL)
		return DSERR_INVALIDPARAM;

	EnterCriticalSection(&dsound_crit);
	if (dsound) {
		IDirectSound_AddRef((LPDIRECTSOUND)dsound);
		*ippDS = dsound;
		LeaveCriticalSection(&dsound_crit);
		return DS_OK;
	}
	LeaveCriticalSection(&dsound_crit);

	/* Enumerate WINMM audio devices and find the one we want */
	wodn = waveOutGetNumDevs();
	if (!wodn) return DSERR_NODRIVER;

	/* FIXME: How do we find the GUID of an audio device? */
	/* Well, just use the first available device for now... */
	wod = 0;
	/* Get output device caps */
	waveOutGetDevCapsA(wod, &wcaps, sizeof(wcaps));
	/* DRV_QUERYDSOUNDIFACE is a "Wine extension" to get the DSound interface */
	waveOutMessage((HWAVEOUT)wod, DRV_QUERYDSOUNDIFACE, (DWORD)&drv, 0);

	/* Allocate memory */
	*ippDS = (IDirectSoundImpl*)HeapAlloc(dsound_heap,HEAP_ZERO_MEMORY,sizeof(IDirectSoundImpl));
	if (*ippDS == NULL)
		return DSERR_OUTOFMEMORY;

	ICOM_VTBL(*ippDS)	= &dsvt;
	(*ippDS)->ref		= 1;

	(*ippDS)->driver	= drv;
	(*ippDS)->priolevel	= DSSCL_NORMAL;
	(*ippDS)->fraglen	= 0;
	(*ippDS)->hwbuf		= NULL;
	(*ippDS)->buffer	= NULL;
	(*ippDS)->buflen	= 0;
	(*ippDS)->writelead	= 0;
	(*ippDS)->state		= STATE_STOPPED;
	(*ippDS)->nrofbuffers	= 0;
	(*ippDS)->buffers	= NULL;
/*	(*ippDS)->primary	= NULL; */
	(*ippDS)->listener	= NULL;

	(*ippDS)->prebuf	= DS_SND_QUEUE_MAX;

	/* Get driver description */
	if (drv) {
		IDsDriver_GetDriverDesc(drv,&((*ippDS)->drvdesc));
	} else {
		/* if no DirectSound interface available, use WINMM API instead */
		(*ippDS)->drvdesc.dwFlags = DSDDESC_DOMMSYSTEMOPEN | DSDDESC_DOMMSYSTEMSETFORMAT;
		(*ippDS)->drvdesc.dnDevNode = wod; /* FIXME? */
	}

	/* Set default wave format (may need it for waveOutOpen) */
	(*ippDS)->wfx.wFormatTag	= WAVE_FORMAT_PCM;
	/* default to stereo, if the sound card can do it */
	if (wcaps.wChannels > 1)
		(*ippDS)->wfx.nChannels		= 2;
	else
		(*ippDS)->wfx.nChannels		= 1;

	/* DirectSound is supposed to default to 8 bits,
	 * at least if the sound card can do it */
#if 0   /* but we're currently best optimized for 16-bit */
	if (wcaps.dwFormats & (WAVE_FORMAT_4M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_1M08 |
			       WAVE_FORMAT_4S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_1S08 |
			       WAVE_FORMAT_48M08 | WAVE_FORMAT_96M08 |
			       WAVE_FORMAT_48S08 | WAVE_FORMAT_96S08)) {
		(*ippDS)->wfx.wBitsPerSample	= 8;
		(*ippDS)->wfx.nBlockAlign	= 1 * (*ippDS)->wfx.nChannels;
	} else {
		/* it's probably a 16-bit-only card */
		(*ippDS)->wfx.wBitsPerSample	= 16;
		(*ippDS)->wfx.nBlockAlign	= 2 * (*ippDS)->wfx.nChannels;
	}
#else
	/* default to 16, if the sound card can do it */
	if (wcaps.dwFormats & (WAVE_FORMAT_4M16 | WAVE_FORMAT_2M16 | WAVE_FORMAT_1M16 |
			       WAVE_FORMAT_4S16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_1S16 |
			       WAVE_FORMAT_48M16 | WAVE_FORMAT_96M16 |
			       WAVE_FORMAT_48S16 | WAVE_FORMAT_96S16)) {
		(*ippDS)->wfx.wBitsPerSample	= 16;
		(*ippDS)->wfx.nBlockAlign	= 2 * (*ippDS)->wfx.nChannels;
	} else {
		/* it's probably a 8-bit-only card */
		(*ippDS)->wfx.wBitsPerSample	= 8;
		(*ippDS)->wfx.nBlockAlign	= 1 * (*ippDS)->wfx.nChannels;
	}
#endif
	(*ippDS)->wfx.nSamplesPerSec	= 22050;
	(*ippDS)->wfx.nAvgBytesPerSec	= 22050 * (*ippDS)->wfx.nBlockAlign;

	/* If the driver requests being opened through MMSYSTEM
	 * (which is recommended by the DDK), it is supposed to happen
	 * before the DirectSound interface is opened */
	if ((*ippDS)->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMOPEN) {
		/* FIXME: is this right? */
		err = mmErr(waveOutOpen(&((*ippDS)->hwo), (*ippDS)->drvdesc.dnDevNode, &((*ippDS)->wfx),
					(DWORD)DSOUND_callback, (DWORD)(*ippDS),
					CALLBACK_FUNCTION | WAVE_DIRECTSOUND));
	}

	if (drv && (err == DS_OK))
		err = IDsDriver_Open(drv);

	/* FIXME: do we want to handle a temporarily busy device? */
	if (err != DS_OK) {
		HeapFree(dsound_heap,0,*ippDS);
		*ippDS = NULL;
		return err;
	}

	/* the driver is now open, so it's now allowed to call GetCaps */
	if (drv) {
		IDsDriver_GetCaps(drv,&((*ippDS)->drvcaps));
	} else {
		unsigned c;

		/* FIXME: look at wcaps */
		FIXME( "Check wcaps\n" );
		(*ippDS)->drvcaps.dwFlags =
			DSCAPS_PRIMARY16BIT | DSCAPS_PRIMARYSTEREO;
		if (DS_EMULDRIVER)
		    (*ippDS)->drvcaps.dwFlags |= DSCAPS_EMULDRIVER;

		/* Allocate memory for HEL buffer headers */
		for (c=0; c<DS_HEL_FRAGS; c++) {
			(*ippDS)->pwave[c] = HeapAlloc(dsound_heap,HEAP_ZERO_MEMORY,sizeof(WAVEHDR));
			if (!(*ippDS)->pwave[c]) {
				/* Argh, out of memory */
				while (c--) {
					HeapFree(dsound_heap,0,(*ippDS)->pwave[c]);
					waveOutClose((*ippDS)->hwo);
					HeapFree(dsound_heap,0,*ippDS);
					*ippDS = NULL;
					return DSERR_OUTOFMEMORY;
				}
			}
		}
	}

	DSOUND_RecalcVolPan(&((*ippDS)->volpan));

	InitializeCriticalSection(&((*ippDS)->mixlock));
	RtlInitializeResource(&((*ippDS)->lock));

	EnterCriticalSection(&dsound_crit);
	if (!dsound) {
		dsound = (*ippDS);
		DSOUND_PrimaryCreate(dsound);
		timeBeginPeriod(DS_TIME_RES);
		dsound->timerID = timeSetEvent(DS_TIME_DEL, DS_TIME_RES, DSOUND_timer,
					       (DWORD)dsound, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);
	}
	LeaveCriticalSection(&dsound_crit);
	return DS_OK;
}

HRESULT WINAPI GetDeviceID(LPCGUID lpGuidSrc, LPGUID lpGuidDest)
{
	TRACE("(%s,%p)\n", debugstr_guid(lpGuidSrc), lpGuidDest);

        if (IsEqualGUID (lpGuidSrc, &DSDEVID_DefaultVoiceCapture))
           memcpy (lpGuidDest, &DSDEVID_DefaultCapture, sizeof (GUID));
        else if (IsEqualGUID (lpGuidSrc, &DSDEVID_DefaultVoicePlayback))
           memcpy (lpGuidDest, &DSDEVID_DefaultPlayback, sizeof (GUID));
        else
        {
           /* lpGuidSrc can be stuff like DSDEVID_DefaultPlayback and friends,
              and in that case lpGuidDest received the "real" device guid.
              But our device don't have any particular GUIDs, so just copy
              the default. */
           memcpy(lpGuidDest, lpGuidSrc, sizeof(GUID));
        }
	return DS_OK;
}

BOOL WINAPI DSOUND_DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
	create_cs();
	dsound_heap = GetProcessHeap();
#if defined( __i386__ )
	bMmxExtensionsAvailable = IsProcessorFeaturePresent( PF_MMX_INSTRUCTIONS_AVAILABLE );
#endif
	break;
    case DLL_PROCESS_DETACH:
	destroy_cs();
	break;
    }

    return TRUE;
}


/*******************************************************************************
 * DirectSound ClassFactory
 */
typedef struct
{
    /* IUnknown fields */
    ICOM_VFIELD(IClassFactory);
    DWORD                       ref;
} IClassFactoryImpl;

static HRESULT WINAPI
DSCF_QueryInterface(LPCLASSFACTORY iface,REFIID riid,LPVOID *ppobj) {
	ICOM_THIS(IClassFactoryImpl,iface);

	FIXME("(%p)->(%s,%p),stub!\n",This,debugstr_guid(riid),ppobj);
	*ppobj = NULL;
	return E_NOINTERFACE;
}

static ULONG WINAPI
DSCF_AddRef(LPCLASSFACTORY iface) {
	ICOM_THIS(IClassFactoryImpl,iface);
	return ++(This->ref);
}

static ULONG WINAPI DSCF_Release(LPCLASSFACTORY iface) {
	ICOM_THIS(IClassFactoryImpl,iface);
	/* static class, won't be  freed */
	return --(This->ref);
}

static HRESULT WINAPI DSCF_CreateInstance(
	LPCLASSFACTORY iface,LPUNKNOWN pOuter,REFIID riid,LPVOID *ppobj
) {
	ICOM_THIS(IClassFactoryImpl,iface);

	TRACE("(%p)->(%p,%s,%p)\n",This,pOuter,debugstr_guid(riid),ppobj);
	if ( IsEqualGUID( &IID_IDirectSound, riid ) ||
	     IsEqualGUID( &IID_IDirectSound8, riid ) ) {
		/* FIXME: reuse already created dsound if present? */
		return DirectSoundCreate8(riid,(LPDIRECTSOUND8*)ppobj,pOuter);
	}
	else if (IsEqualGUID(&IID_IDirectSoundCapture, riid))
	{
		return DSOUND_DirectSoundCaptureCreate((LPDIRECTSOUNDCAPTURE*)ppobj,
			pOuter);
	}
	*ppobj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI DSCF_LockServer(LPCLASSFACTORY iface,BOOL dolock) {
	ICOM_THIS(IClassFactoryImpl,iface);
	FIXME("(%p)->(%d),stub!\n",This,dolock);
	return S_OK;
}

static ICOM_VTABLE(IClassFactory) DSCF_Vtbl = {
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	DSCF_QueryInterface,
	DSCF_AddRef,
	DSCF_Release,
	DSCF_CreateInstance,
	DSCF_LockServer
};
static IClassFactoryImpl DSOUND_CF = {&DSCF_Vtbl, 1 };

/*******************************************************************************
 * DllGetClassObject [DSOUND.5]
 * Retrieves class object from a DLL object
 *
 * NOTES
 *    Docs say returns STDAPI
 *
 * PARAMS
 *    rclsid [I] CLSID for the class object
 *    riid   [I] Reference to identifier of interface for class object
 *    ppv    [O] Address of variable to receive interface pointer for riid
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: CLASS_E_CLASSNOTAVAILABLE, E_OUTOFMEMORY, E_INVALIDARG,
 *             E_UNEXPECTED
 */
DWORD WINAPI DSOUND_DllGetClassObject(REFCLSID rclsid,REFIID riid,LPVOID *ppv)
{
    TRACE("(%p,%p,%p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    if ( IsEqualCLSID( &IID_IClassFactory, riid )
        || IsEqualCLSID(&IID_IUnknown, riid)) {
    	*ppv = (LPVOID)&DSOUND_CF;
	IClassFactory_AddRef((IClassFactory*)*ppv);
	return S_OK;
    }

    FIXME("(%p,%p,%p): no interface found.\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}


/*******************************************************************************
 * DllCanUnloadNow [DSOUND.4]  Determines whether the DLL is in use.
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: S_FALSE
 */
DWORD WINAPI DSOUND_DllCanUnloadNow(void)
{
    FIXME("(void): stub\n");
    return S_FALSE;
}
