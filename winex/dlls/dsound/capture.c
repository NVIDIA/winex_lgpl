/*  			DirectSoundCapture
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright 2000-2001 TransGaming Technologies, Inc.
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

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "mmsystem.h"
#include "winternl.h"
#include "wine/debug.h"
#include "dsound.h"
#include "dsdriver.h"
#include "dsound_private.h"

#define DS_CAP_DEL 20 /* duration of HEL capture fragment */

WINE_DEFAULT_DEBUG_CHANNEL(dsound);


static ICOM_VTABLE(IDirectSoundCapture) dscvt;
static ICOM_VTABLE(IDirectSoundCaptureBuffer8) dscbvt;

static void DSCAPTURE_WaveQueue(IDirectSoundCaptureBufferImpl *This, DWORD rdq);
static void CALLBACK DSCAPTURE_callback(HWAVEIN hwi, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2);
static HRESULT WINAPI IDirectSoundCaptureImpl_Initialize(LPDIRECTSOUNDCAPTURE iface, LPCGUID lpcGUID);


/***************************************************************************
 * DSOUND_DirectSoundCaptureCreate
 *
 * Create a DirectSoundCapture interface
 *
 * RETURNS
 *    Success: DS_OK
 *    Failure: DSERR_NOAGGREGATION, DSERR_INVALIDPARAM, DSERR_OUTOFMEMORY
 */
HRESULT WINAPI DSOUND_DirectSoundCaptureCreate(
	LPDIRECTSOUNDCAPTURE* lplpDSC,
	LPUNKNOWN pUnkOuter )
{
    IDirectSoundCaptureImpl** ippDSC = (IDirectSoundCaptureImpl**)lplpDSC;

    TRACE("(%p,%p)\n", lplpDSC, pUnkOuter);

    if (lplpDSC == NULL)
        return DSERR_INVALIDPARAM;

    if (pUnkOuter)
        return DSERR_NOAGGREGATION;

    *ippDSC = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                        sizeof(IDirectSoundCaptureImpl));
    if (*ippDSC == NULL)
        return DSERR_OUTOFMEMORY;

    ICOM_VTBL(*ippDSC) = &dscvt;
    (*ippDSC)->ref = 0;
    (*ippDSC)->dnDevNode = -1;

    IDirectSoundCapture_AddRef(*lplpDSC);

    return DS_OK;
}

/***************************************************************************
 * DirectSoundCaptureCreate [DSOUND.6]
 *
 * Create and initialize a DirectSoundCapture interface
 *
 * RETURNS
 *    Success: DS_OK
 *    Failure: DSERR_NOAGGREGATION, DSERR_ALLOCATED, DSERR_INVALIDPARAM,
 *             DSERR_OUTOFMEMORY
 */
HRESULT WINAPI DirectSoundCaptureCreate8(
	LPCGUID lpcGUID,
	LPDIRECTSOUNDCAPTURE* lplpDSC,
	LPUNKNOWN pUnkOuter )
{
    HRESULT hr;

    TRACE("(%s,%p,%p)\n", debugstr_guid(lpcGUID), lplpDSC, pUnkOuter);

    /* accept a lpcGUID of NULL (default), the default capture device or the 
       default voice capture device */
    if (lpcGUID && !IsEqualGUID(lpcGUID, &DSDEVID_DefaultCapture)
        && !IsEqualGUID(lpcGUID, &DSDEVID_DefaultVoiceCapture))
        return DSERR_NODRIVER;

    hr = DSOUND_DirectSoundCaptureCreate(lplpDSC, pUnkOuter);
    if (hr == DS_OK)
    {
        hr = IDirectSoundCaptureImpl_Initialize(*lplpDSC, lpcGUID);
        if (hr != DS_OK)
        {
            IDirectSoundCapture_Release(*lplpDSC);
            *lplpDSC = NULL;
        }
    }

    return hr;
}

/***************************************************************************
 * DirectSoundCaptureEnumerateA [DSOUND.7]
 *
 * Enumerate all DirectSound drivers installed in the system
 *
 * RETURNS
 *    Success: DS_OK
 *    Failure: DSERR_INVALIDPARAM
 */
HRESULT WINAPI DirectSoundCaptureEnumerateA(
        LPDSENUMCALLBACKA lpDSEnumCallback,
        LPVOID lpContext)
{
	TRACE("(%p,%p)\n", lpDSEnumCallback, lpContext );

	if ( lpDSEnumCallback )
	{
		if (lpDSEnumCallback(NULL,"WINE Primary Sound Capture Driver",
                                     "SoundCap",lpContext))
                    lpDSEnumCallback((LPGUID)&DSDEVID_DefaultCapture,
                                     "Default Capture Device", "SoundCap",
                                     lpContext);
	}


	return DS_OK;
}

/***************************************************************************
 * DirectSoundCaptureEnumerateW [DSOUND.8]
 *
 * Enumerate all DirectSound drivers installed in the system
 *
 * RETURNS
 *    Success: DS_OK
 *    Failure: DSERR_INVALIDPARAM
 */
static WCHAR WPSCD[] = {'T', 'G', ' ', 'P', 'r', 'i', 'm', 'a', 'r', 'y', ' ',
                        'S', 'o', 'u', 'n', 'd', ' ', 'C', 'a', 'p', 't',
                        'u', 'r', 'e', ' ', 'D', 'r', 'i', 'v', 'e', 'r', 0};
static WCHAR WSoundCap[] = {'S', 'o', 'u', 'n', 'd', 'C', 'a', 'p', 0};
static WCHAR WDCD[] = {'D', 'e', 'f', 'a', 'u', 'l', 't', ' ', 'C', 'a', 'p',
                       't', 'u', 'r', 'e', ' ', 'D', 'e', 'v', 'i', 'c', 'e', 0};

HRESULT WINAPI DirectSoundCaptureEnumerateW(
        LPDSENUMCALLBACKW lpDSEnumCallback,
        LPVOID lpContext)
{
   TRACE ("(%p,%p)\n", lpDSEnumCallback, lpContext);

   if (lpDSEnumCallback)
   {
      if (lpDSEnumCallback (NULL, WPSCD, WSoundCap, lpContext))
         lpDSEnumCallback ((LPGUID)&DSDEVID_DefaultCapture,
                           WDCD, WSoundCap, lpContext);
   }

   return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureImpl_QueryInterface(
	LPDIRECTSOUNDCAPTURE iface,
	REFIID riid,
	LPVOID* ppobj )
{
	ICOM_THIS(IDirectSoundCaptureImpl,iface);

	TRACE( "(%p)->(%s,%p)\n", This, debugstr_guid(riid), ppobj );

	if ( IsEqualGUID( &IID_IUnknown, riid ) ||
	     IsEqualGUID( &IID_IDirectSoundCapture, riid) ) {
		IDirectSoundCapture_AddRef(iface);
		*ppobj = (LPVOID)iface;
		return S_OK;
	}

	FIXME( "Unknown IID %s\n", debugstr_guid( riid ) );

	*ppobj = NULL;

	return E_NOINTERFACE;
}

static ULONG WINAPI
IDirectSoundCaptureImpl_AddRef( LPDIRECTSOUNDCAPTURE iface )
{
        ICOM_THIS(IDirectSoundCaptureImpl,iface);

	TRACE( "(%p) ref was 0x%08lx\n", This, This->ref );
	return InterlockedIncrement(&(This->ref));
}

static ULONG WINAPI
IDirectSoundCaptureImpl_Release( LPDIRECTSOUNDCAPTURE iface )
{
	ICOM_THIS(IDirectSoundCaptureImpl,iface);
	DWORD ref;

	TRACE( "(%p) ref was 0x%08lx\n", This, This->ref );
	ref = InterlockedDecrement(&(This->ref));

	if (!ref) {
		HeapFree( GetProcessHeap(), 0, This );
	}

	return ref;
}

static HRESULT WINAPI
IDirectSoundCaptureImpl_CreateCaptureBuffer(
	LPDIRECTSOUNDCAPTURE iface,
	LPCDSCBUFFERDESC lpcDSCBufferDesc,
	LPDIRECTSOUNDCAPTUREBUFFER* lplpDSCaptureBuffer,
	LPUNKNOWN pUnk )
{
	ICOM_THIS(IDirectSoundCaptureImpl,iface);
	LPWAVEFORMATEX wfex;
	IDirectSoundCaptureBufferImpl** ippDSCB = (IDirectSoundCaptureBufferImpl**)lplpDSCaptureBuffer;
	DWORD sw, fraglen;
	HRESULT err = DS_OK;

	TRACE( "(%p)->(%p,%p,%p)\n", This, lpcDSCBufferDesc, lplpDSCaptureBuffer, pUnk );

	if ( pUnk ) {
		return DSERR_INVALIDPARAM;
	}

	if ((lpcDSCBufferDesc == NULL)
		|| (lpcDSCBufferDesc->dwBufferBytes == 0))
	{
		return DSERR_INVALIDPARAM;
	}

	if (TRACE_ON(dsound)) {
		TRACE("(structsize=%ld)\n",lpcDSCBufferDesc->dwSize);
		TRACE("(flags=0x%08lx)\n",lpcDSCBufferDesc->dwFlags);
		TRACE("(bufferbytes=%ld)\n",lpcDSCBufferDesc->dwBufferBytes);
		TRACE("(lpwfxFormat=%p)\n",lpcDSCBufferDesc->lpwfxFormat);
		TRACE("(fxcount=%ld)\n",lpcDSCBufferDesc->dwFXCount);
	}

	wfex = lpcDSCBufferDesc->lpwfxFormat;
	if (wfex)
	{
		TRACE("(formattag=0x%04x,chans=%d,samplerate=%ld,"
		   "bytespersec=%ld,blockalign=%d,bitspersamp=%d,cbSize=%d)\n",
		   wfex->wFormatTag, wfex->nChannels, wfex->nSamplesPerSec,
		   wfex->nAvgBytesPerSec, wfex->nBlockAlign,
		   wfex->wBitsPerSample, wfex->cbSize);
	}
	else
	{
		*ippDSCB = NULL;
		return DSERR_INVALIDPARAM;
	}

	*ippDSCB = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( IDirectSoundCaptureBufferImpl ) );
	if (*ippDSCB == NULL)
		return DSERR_OUTOFMEMORY;

	ICOM_VTBL(*ippDSCB) = &dscbvt;
	(*ippDSCB)->ref = 1;

	memcpy(&((*ippDSCB)->wfx), wfex, sizeof(*wfex));
	(*ippDSCB)->flags = lpcDSCBufferDesc->dwFlags;
	(*ippDSCB)->buflen = lpcDSCBufferDesc->dwBufferBytes;

	wfex = &((*ippDSCB)->wfx);

	/* calculate capture fragment size */
	sw = wfex->nChannels * (wfex->wBitsPerSample / 8);
	/* let fragment size approximate the timer delay */
	fraglen = (wfex->nSamplesPerSec * DS_CAP_DEL / 1000) * sw;
	/* reduce fragment size until an integer number of them fits in the buffer */
	while ((*ippDSCB)->buflen % fraglen) fraglen -= sw;
	(*ippDSCB)->fraglen = fraglen;
	(*ippDSCB)->frags = (*ippDSCB)->buflen / fraglen;

	TRACE("fraglen = %ld\n", (*ippDSCB)->fraglen);

	if (TRUE) {
		unsigned c;
		(*ippDSCB)->buffer = HeapAlloc( GetProcessHeap(), 0, (*ippDSCB)->buflen );
		if ((*ippDSCB)->buffer == NULL) {
			err = DSERR_OUTOFMEMORY;
			goto fail;
		}
		(*ippDSCB)->pwave = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WAVEHDR) * (*ippDSCB)->frags );
		if ((*ippDSCB)->pwave == NULL) {
			err = DSERR_OUTOFMEMORY;
			goto fail;
		}
		for (c=0; c<(*ippDSCB)->frags; c++) {
			((*ippDSCB)->pwave)[c] = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WAVEHDR));
			if (((*ippDSCB)->pwave)[c] == NULL) {
				err = DSERR_OUTOFMEMORY;
				goto fail;
			}
		}
	}

	err = mmErr(waveInOpen(&((*ippDSCB)->hwi), This->dnDevNode, wfex,
		    (DWORD)DSCAPTURE_callback, (DWORD)(*ippDSCB),
		    CALLBACK_FUNCTION | (((*ippDSCB)->flags & DSCBCAPS_WAVEMAPPED) ? WAVE_MAPPED : 0)));

	if (err != DS_OK)
		goto fail;

	if (TRUE) {
		unsigned c;
		LPWAVEHDR *pwave = (*ippDSCB)->pwave;

		for (c=0; c<(*ippDSCB)->frags; c++) {
			pwave[c]->lpData = (LPVOID)((*ippDSCB)->buffer + c*(*ippDSCB)->fraglen);
			pwave[c]->dwBufferLength = (*ippDSCB)->fraglen;
			pwave[c]->dwUser = (DWORD)(*ippDSCB);
			pwave[c]->dwFlags = 0;
			pwave[c]->dwLoops = 0;
			err = mmErr(waveInPrepareHeader((*ippDSCB)->hwi, pwave[c], sizeof(WAVEHDR)));
			if (err != DS_OK) {
				while (c--)
					waveInUnprepareHeader((*ippDSCB)->hwi, pwave[c], sizeof(WAVEHDR));
				goto fail;
			}
		}

		(*ippDSCB)->pwcap = 0;
		(*ippDSCB)->pwread = 0;
		(*ippDSCB)->pwqueue = 0;
		DSCAPTURE_WaveQueue(*ippDSCB, (DWORD)-1);
	}

	if (err != DS_OK)
		goto fail;

	InitializeCriticalSection( &(*ippDSCB)->lock );

	TRACE("success => %p\n", *ippDSCB);
	return DS_OK;
fail:
	if ((*ippDSCB)->pwave) {
		unsigned c;
		for (c=0; c<(*ippDSCB)->frags; c++) {
			if (((*ippDSCB)->pwave)[c])
				HeapFree( GetProcessHeap(), 0, ((*ippDSCB)->pwave)[c]);
		}
		HeapFree( GetProcessHeap(), 0, (*ippDSCB)->pwave );
	}
	if ((*ippDSCB)->buffer) HeapFree( GetProcessHeap(), 0, (*ippDSCB)->buffer );
	HeapFree( GetProcessHeap(), 0, *ippDSCB );
	*ippDSCB = NULL;
	TRACE("error => 0x%08lx\n", err);
	return err;
}

static HRESULT WINAPI
IDirectSoundCaptureImpl_GetCaps(
	LPDIRECTSOUNDCAPTURE iface,
	LPDSCCAPS lpDSCCaps )
{
	ICOM_THIS(IDirectSoundCaptureImpl,iface);

	TRACE( "(%p)->(%p)\n", This, lpDSCCaps );

	if (lpDSCCaps == NULL)
		return DSERR_INVALIDPARAM;

	if (lpDSCCaps->dwSize < sizeof(*lpDSCCaps))
		return DSERR_INVALIDPARAM;

	if (This->dnDevNode == -1)
        return DSERR_UNINITIALIZED;

	lpDSCCaps->dwFlags = This->dwCapFlags;
	lpDSCCaps->dwFormats = This->dwCapFormats;
	lpDSCCaps->dwChannels = This->dwCapChannels;

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureImpl_Initialize(
	LPDIRECTSOUNDCAPTURE iface,
	LPCGUID lpcGUID )
{
    ICOM_THIS(IDirectSoundCaptureImpl,iface);
    WAVEINCAPSA wcaps;
    unsigned wid, widn;

    TRACE( "(%p)->(%p)\n", This, lpcGUID );

    /* FIXME - check the input lpcGUID? */

    if (This->dnDevNode != -1)
        return DSERR_ALREADYINITIALIZED;

    /* Enumerate WINMM audio devices */
    widn = waveInGetNumDevs();
    if (!widn)
        return DSERR_NODRIVER;

    /* FIXME: how do we find the GUID? */
    /* Just use first device for now... */
    wid = 0;
    /* Get capture device caps */
    waveInGetDevCapsA(wid, &wcaps, sizeof(wcaps));

    This->dnDevNode = wid;
    This->dwCapFlags = DSCCAPS_EMULDRIVER;
    This->dwCapFormats = wcaps.dwFormats;
    This->dwCapChannels = wcaps.wChannels;

    return DS_OK;
}


static ICOM_VTABLE(IDirectSoundCapture) dscvt =
{
        ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
        /* IUnknown methods */
        IDirectSoundCaptureImpl_QueryInterface,
        IDirectSoundCaptureImpl_AddRef,
        IDirectSoundCaptureImpl_Release,

        /* IDirectSoundCapture methods */
        IDirectSoundCaptureImpl_CreateCaptureBuffer,
        IDirectSoundCaptureImpl_GetCaps,
        IDirectSoundCaptureImpl_Initialize
};

HRESULT WINAPI
IDirectSoundCaptureBufferImpl_QueryInterface(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
        REFIID riid,
        LPVOID* ppobj )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)->(%s,%p)\n", This, debugstr_guid(riid), ppobj );

	if ( IsEqualGUID( &IID_IUnknown, riid ) ||
	     IsEqualGUID( &IID_IDirectSoundCaptureBuffer, riid) ||
	     IsEqualGUID( &IID_IDirectSoundCaptureBuffer8, riid) ) {
		IDirectSoundCaptureBuffer8_AddRef(iface);
		*ppobj = (LPVOID)iface;
		return S_OK;
	}

	/* NOTE: this behaviour seems to be completely undocumented.  It does, however,
	         succeed under native dsound. */
	if ( IsEqualGUID( &IID_IDirectSoundNotify, riid ) ) {
	    IDirectSoundNotifyImpl *dsn;


	    /* create a new notification object for the caller to play with */
	    IDirectSoundNotifyImpl_Create(NULL, This, &dsn);

		*ppobj = (LPVOID)dsn;

		return S_OK;
	}

	FIXME( "Unknown IID %s\n", debugstr_guid( riid ) );

	*ppobj = NULL;

	return E_NOINTERFACE;
}

ULONG WINAPI
IDirectSoundCaptureBufferImpl_AddRef( LPDIRECTSOUNDCAPTUREBUFFER8 iface )
{
        ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p) ref was 0x%08lx\n", This, This->ref );
	return InterlockedIncrement(&(This->ref));
}

ULONG WINAPI
IDirectSoundCaptureBufferImpl_Release( LPDIRECTSOUNDCAPTUREBUFFER8 iface )
{
        ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);
        DWORD ref;

        EnterCriticalSection( &This->lock );

	TRACE( "(%p) ref was 0x%08lx\n", This, This->ref );
	ref = InterlockedDecrement(&(This->ref));

        if ( !ref ) {
		unsigned c;
		if (TRUE) {
			This->pwqueue = (DWORD)-1; /* resetting queues */
			waveInReset(This->hwi);
			for (c=0; c<This->frags; c++) {
				waveInUnprepareHeader(This->hwi, (This->pwave)[c], sizeof(WAVEHDR));
			}
			for (c=0; c<This->frags; c++) {
				HeapFree(GetProcessHeap(), 0, (This->pwave)[c]);
			}
			HeapFree(GetProcessHeap(), 0, This->pwave);
			HeapFree(GetProcessHeap(), 0, This->buffer);
		}
		waveInClose( This->hwi );
		DeleteCriticalSection( &This->lock );
		HeapFree( GetProcessHeap(), 0, This );
        }

        return ref;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_GetCaps(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	LPDSCBCAPS lpDSCBCaps )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)->(%p)\n", This, lpDSCBCaps );

	if (lpDSCBCaps == NULL)
		return DSERR_INVALIDPARAM;

	if (lpDSCBCaps->dwSize < sizeof(*lpDSCBCaps))
		return DSERR_INVALIDPARAM;

	lpDSCBCaps->dwFlags = This->flags;
	lpDSCBCaps->dwBufferBytes = This->buflen;
	lpDSCBCaps->dwReserved = 0;

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_GetCurrentPosition(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	LPDWORD lpdwCapturePosition,
	LPDWORD lpdwReadPosition )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);
	DWORD rpos, cpos;

	TRACE( "(%p)->(%p,%p)\n", This, lpdwCapturePosition, lpdwReadPosition );

	if (TRUE) {
#if 0
		/* Querying the driver like this is only useful if we're not
		 * doing any sample rate or format conversion through the wave
		 * mapper. If we do, we won't have anything more than the buffers
		 * it has completed anyway, making it meaningless (and our wave
		 * mapper implementation doesn't properly keep track of the position
		 * anyway). */
		MMTIME wpos;
		wpos.wType = TIME_BYTES;
		waveInGetPosition(This->hwi, &wpos, sizeof(wpos));
		rpos = wpos.u.cb % This->buflen;
#else
		rpos = This->pwread * This->fraglen;
#endif
		/* Not sure what we really should use for capturepos.
		 * I'm assuming most apps won't care much, so just do this. */
		cpos = This->pwcap * This->fraglen;
	}

	TRACE("capturepos=%ld, readpos=%ld\n", cpos, rpos);

	if (lpdwReadPosition) *lpdwReadPosition = rpos;
	if (lpdwCapturePosition) *lpdwCapturePosition = cpos;

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_GetFormat(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
        LPWAVEFORMATEX lpwfxFormat,
        DWORD dwSizeAllocated,
        LPDWORD lpdwSizeWritten )
{
    ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

    TRACE("(%p)->(%p,0x%08lx,%p): stub\n", This, lpwfxFormat, dwSizeAllocated,
          lpdwSizeWritten );

    if (!lpwfxFormat)
    {
        if (!lpdwSizeWritten)
            return DSERR_INVALIDPARAM;
        else
        {
            /* requesting size of buffer */
            *lpdwSizeWritten = sizeof(WAVEFORMATEX) + This->wfx.cbSize;
        }
    }
    else
    {
        /* truncate the size returned to the size of the buffer provided */
        if (dwSizeAllocated > (sizeof(WAVEFORMATEX) + This->wfx.cbSize))
            dwSizeAllocated = sizeof(WAVEFORMATEX) + This->wfx.cbSize;
        CopyMemory(lpwfxFormat, &This->wfx, dwSizeAllocated);
        if (lpdwSizeWritten)
            *lpdwSizeWritten = dwSizeAllocated;
    }
    return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_GetStatus(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	LPDWORD lpdwStatus )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)->(%p)\n", This, lpdwStatus );

	if (lpdwStatus == NULL)
		return DSERR_INVALIDPARAM;

	*lpdwStatus = This->capflags;

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_Initialize(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	LPDIRECTSOUNDCAPTURE lpDSC,
	LPCDSCBUFFERDESC lpcDSCBDesc )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	FIXME( "(%p)->(%p,%p): stub\n", This, lpDSC, lpcDSCBDesc );

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_Lock(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	DWORD dwReadCursor,
	DWORD dwReadBytes,
	LPVOID* lplpvAudioPtr1,
	LPDWORD lpdwAudioBytes1,
	LPVOID* lplpvAudioPtr2,
	LPDWORD lpdwAudioBytes2,
	DWORD dwFlags )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)->(%08lu,%08lu,%p,%p,%p,%p,0x%08lx)\n", This,
	       dwReadCursor, dwReadBytes,
	       lplpvAudioPtr1, lpdwAudioBytes1,
	       lplpvAudioPtr2, lpdwAudioBytes2, dwFlags );

COMPUTE_USER_WRAPAROUND( dwReadCursor, This->buflen );
	if (dwFlags & DSCBLOCK_ENTIREBUFFER)
		dwReadBytes = This->buflen;
	if (dwReadBytes > This->buflen)
		dwReadBytes = This->buflen;

	if (TRUE) {
		if (dwReadCursor+dwReadBytes <= This->buflen) {
			*(LPBYTE*)lplpvAudioPtr1 = This->buffer+dwReadCursor;
			*lpdwAudioBytes1 = dwReadBytes;
			if (lplpvAudioPtr2)
				*(LPBYTE*)lplpvAudioPtr2 = NULL;
			if (lpdwAudioBytes2)
				*lpdwAudioBytes2 = 0;
			TRACE("->%ld.0\n",dwReadBytes);
		} else {
			*(LPBYTE*)lplpvAudioPtr1 = This->buffer+dwReadCursor;
			*lpdwAudioBytes1 = This->buflen-dwReadCursor;
			if (lplpvAudioPtr2)
				*(LPBYTE*)lplpvAudioPtr2 = This->buffer;
			if (lpdwAudioBytes2)
				*lpdwAudioBytes2 = dwReadBytes-(This->buflen-dwReadCursor);
			TRACE("->%ld.%ld\n",dwReadBytes,lpdwAudioBytes2?*lpdwAudioBytes2:0);
		}
	}

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_Start(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	DWORD dwFlags )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)->(0x%08lx)\n", This, dwFlags );

	This->capflags = dwFlags | DSCBSTATUS_CAPTURING;
	waveInStart( This->hwi );

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_Stop( LPDIRECTSOUNDCAPTUREBUFFER8 iface )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)\n", This );

	waveInStop( This->hwi );
	This->capflags = 0;

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_Unlock(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
	LPVOID lpvAudioPtr1,
	DWORD dwAudioBytes1,
	LPVOID lpvAudioPtr2,
	DWORD dwAudioBytes2 )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	TRACE( "(%p)->(%p,%08lu,%p,%08lu): stub\n", This,
	       lpvAudioPtr1, dwAudioBytes1,
	       lpvAudioPtr2, dwAudioBytes2 );

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_GetObjectInPath(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
        REFGUID rguidObject,
        DWORD dwIndex,
        REFGUID rguidInterface,
        LPVOID* ppObject )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	FIXME( "(%p)->(%s,%lu,%s,%p): stub\n", This, debugstr_guid(rguidObject), dwIndex, debugstr_guid(rguidInterface), ppObject );

	return DS_OK;
}

static HRESULT WINAPI
IDirectSoundCaptureBufferImpl_GetFXStatus(
        LPDIRECTSOUNDCAPTUREBUFFER8 iface,
        DWORD dwFXCount,
        LPDWORD pdwFXStatus )
{
	ICOM_THIS(IDirectSoundCaptureBufferImpl,iface);

	FIXME( "(%p)->(%lu,%p): stub\n", This, dwFXCount, pdwFXStatus );

	return DS_OK;
}


static ICOM_VTABLE(IDirectSoundCaptureBuffer8) dscbvt =
{
        ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
        /* IUnknown methods */
        IDirectSoundCaptureBufferImpl_QueryInterface,
        IDirectSoundCaptureBufferImpl_AddRef,
        IDirectSoundCaptureBufferImpl_Release,

        /* IDirectSoundCaptureBuffer methods */
        IDirectSoundCaptureBufferImpl_GetCaps,
        IDirectSoundCaptureBufferImpl_GetCurrentPosition,
        IDirectSoundCaptureBufferImpl_GetFormat,
        IDirectSoundCaptureBufferImpl_GetStatus,
        IDirectSoundCaptureBufferImpl_Initialize,
        IDirectSoundCaptureBufferImpl_Lock,
        IDirectSoundCaptureBufferImpl_Start,
        IDirectSoundCaptureBufferImpl_Stop,
        IDirectSoundCaptureBufferImpl_Unlock,

        /* IDirectSoundCaptureBuffer methods */
        IDirectSoundCaptureBufferImpl_GetObjectInPath,
        IDirectSoundCaptureBufferImpl_GetFXStatus
};


static void DSCAPTURE_CheckEvent(IDirectSoundCaptureBufferImpl *dscb, DWORD readpos, int len)
{
	int			i;
	DWORD			offset;
	LPDSBPOSITIONNOTIFY	event;

	if (dscb->positions == NULL || dscb->positions->nrofnotifies == 0)
		return;

	TRACE("(%p) buflen = %ld, readpos = %ld, len = %d\n",
		dscb, dscb->buflen, readpos, len);
	for (i = 0; i < dscb->positions->nrofnotifies ; i++) {
		event = dscb->positions->notifies + i;
		offset = event->dwOffset;
		TRACE("checking %d, position %ld, event = %d\n",
			i, offset, event->hEventNotify);
		/* DSBPN_OFFSETSTOP has to be the last element. So this is */
		/* OK. [Inside DirectX, p274] */
		/*  */
		/* This also means we can't sort the entries by offset, */
		/* because DSBPN_OFFSETSTOP == -1 */
		if (offset == DSBPN_OFFSETSTOP) {
			if (dscb->capflags == 0) {
				SetEvent(event->hEventNotify);
				TRACE("signalled event %d (%d)\n", event->hEventNotify, i);
				return;
			} else
				return;
		}
		if ((readpos + len) >= dscb->buflen) {
			if ((offset < ((readpos + len) % dscb->buflen)) ||
			    (offset >= readpos)) {
				TRACE("signalled event %d (%d)\n", event->hEventNotify, i);
				SetEvent(event->hEventNotify);
			}
		} else {
			if ((offset >= readpos) && (offset < (readpos + len))) {
				TRACE("signalled event %d (%d)\n", event->hEventNotify, i);
				SetEvent(event->hEventNotify);
			}
		}
	}
}

static void DSCAPTURE_WaveQueue(IDirectSoundCaptureBufferImpl *This, DWORD rdq)
{
	if (rdq + This->pwqueue > DS_HEL_QUEUE) rdq = DS_HEL_QUEUE - This->pwqueue;
	if ((!(This->capflags & DSCBSTART_LOOPING)) &&
	    (rdq + This->pwcap > This->frags)) rdq = This->frags - This->pwcap;

	TRACE("queueing %ld buffers, starting at %d\n", rdq, This->pwcap);
	for (; rdq; rdq--) {
		waveInAddBuffer(This->hwi, (This->pwave)[This->pwcap], sizeof(WAVEHDR));
		This->pwcap++;
		if (This->pwcap >= This->frags) This->pwcap = 0;
		This->pwqueue++;
	}
}

static void CALLBACK DSCAPTURE_callback(HWAVEIN hwi, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	IDirectSoundCaptureBufferImpl* This = (IDirectSoundCaptureBufferImpl*)dwUser;
	TRACE("entering at %ld, msg=%08x\n", GetTickCount(), msg);
	if (msg == MM_WIM_DATA) {
		DWORD pwread, fraglen, frags, flags, readpos;
		if (This->pwqueue == (DWORD)-1) {
			TRACE("completed due to reset\n");
			return;
		}
		/* retrieve current values */
		fraglen = This->fraglen;
		frags = This->frags;
		pwread = This->pwread;
		readpos = pwread * fraglen;
		flags = This->capflags;
		/* complete current buffer */
		TRACE("done recording pos=%ld\n", readpos);
		pwread++;
		if (pwread >= frags) {
			pwread = 0;
			if (!(flags & DSCBSTART_LOOPING)) flags = 0;
		}
		/* write new values */
		This->pwread = pwread;
		This->pwqueue--;
		This->capflags = flags;
		/* check for notifications */
		DSCAPTURE_CheckEvent(This, readpos, fraglen);
		/* queue new buffer if desired */
		if (flags) DSCAPTURE_WaveQueue(This, 1);
	}
	TRACE("completed\n");
}
