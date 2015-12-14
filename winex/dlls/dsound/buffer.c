/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright (c) 2000-2015 NVIDIA CORPORATION. All rights reserved.
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



/*******************************************************************************
 *		IDirectSoundBuffer
 */

static HRESULT WINAPI IDirectSoundBufferImpl_SetFormat(
	LPDIRECTSOUNDBUFFER8 iface,LPCWAVEFORMATEX wfex
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);

	TRACE("(%p,%p)\n",This,wfex);
	/* This method is not available on secondary buffers */
	return DSERR_INVALIDCALL;
}

static HRESULT WINAPI IDirectSoundBufferImpl_SetVolume(
	LPDIRECTSOUNDBUFFER8 iface,LONG vol
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	LONG oldVol;

	TRACE("(%p,%ld)\n",This,vol);

	if (!(This->dsbd.dwFlags & DSBCAPS_CTRLVOLUME))
		return DSERR_CONTROLUNAVAIL;

	if ((vol > DSBVOLUME_MAX) || (vol < DSBVOLUME_MIN))
		return DSERR_INVALIDPARAM;

	/* **** */
	EnterCriticalSection(&(This->lock));

	if (This->dsbd.dwFlags & DSBCAPS_CTRL3D) {
		oldVol = This->ds3db->lVolume;
		This->ds3db->lVolume = vol;
		if (vol != oldVol) DSOUND_Recalc3DBuffer(This);
	} else {
		oldVol = This->volpan.lVolume;
		This->volpan.lVolume = vol;
		if (vol != oldVol) DSOUND_RecalcVolPan(&(This->volpan));
	}

	if (vol != oldVol) {
		if (This->hwbuf) {
			IDsDriverBuffer_SetVolumePan(This->hwbuf, &(This->volpan));
		}
		else DSOUND_ForceRemix(This);
	}

	LeaveCriticalSection(&(This->lock));
	/* **** */

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetVolume(
	LPDIRECTSOUNDBUFFER8 iface,LPLONG vol
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%p)\n",This,vol);

	if (vol == NULL)
		return DSERR_INVALIDPARAM;

	if (This->dsbd.dwFlags & DSBCAPS_CTRL3D)
		*vol = This->ds3db->lVolume;
	else
		*vol = This->volpan.lVolume;
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_SetFrequency(
	LPDIRECTSOUNDBUFFER8 iface,DWORD freq
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD oldFreq;

	TRACE("(%p,%ld)\n",This,freq);

	if (!(This->dsbd.dwFlags & DSBCAPS_CTRLFREQUENCY))
		return DSERR_CONTROLUNAVAIL;

	if (freq == DSBFREQUENCY_ORIGINAL)
		freq = This->wfx.nSamplesPerSec;

	if ((freq < DSBFREQUENCY_MIN) || (freq > DSBFREQUENCY_MAX))
		return DSERR_INVALIDPARAM;

	/* **** */
	EnterCriticalSection(&(This->lock));

	oldFreq = This->freq;
	This->freq = freq;
	if (freq != oldFreq) {
		This->freqAdjust = (freq << DSOUND_FREQSHIFT) / This->dsound->wfx.nSamplesPerSec;
		This->nAvgBytesPerSec = freq * This->wfx.nBlockAlign;
		DSOUND_RecalcFormat(This);
		if (!This->hwbuf) DSOUND_ForceRemix(This);
	}

	LeaveCriticalSection(&(This->lock));
	/* **** */

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_Play(
	LPDIRECTSOUNDBUFFER8 iface,DWORD reserved1,DWORD reserved2,DWORD flags
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%08lx,%08lx,%08lx)\n",
		This,reserved1,reserved2,flags
	);

	/* **** */
	EnterCriticalSection(&(This->lock));

	This->playflags = flags;
	if (This->state == STATE_STOPPED) {
		This->leadin = TRUE;
		This->startpos = This->buf_mixpos;
		This->state = STATE_STARTING;
	} else if (This->state == STATE_STOPPING)
		This->state = STATE_PLAYING;
	if (This->hwbuf) {
		IDsDriverBuffer_Play(This->hwbuf, 0, 0, This->playflags);
		This->state = STATE_PLAYING;
	}

	LeaveCriticalSection(&(This->lock));
	/* **** */

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_Stop(LPDIRECTSOUNDBUFFER8 iface)
{
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p)\n",This);

	/* **** */
	EnterCriticalSection(&(This->lock));

	if (This->state == STATE_PLAYING)
		This->state = STATE_STOPPING;
	else if (This->state == STATE_STARTING)
		This->state = STATE_STOPPED;
	if (This->hwbuf) {
		IDsDriverBuffer_Stop(This->hwbuf);
		This->state = STATE_STOPPED;
	}
	DSOUND_CheckEvent(This, 0);
	/* sometimes an app calls SetCurrentPosition(0) immediately 
	 * after stopping play, which can result in a bit of leftover sound being played
	 * because the mixer thinks that its still in the leadin.  However if we are stopping
	 * the sound, i think its fairly safe to say that we are no longer in the leadin.  */
	This->leadin = FALSE;

	LeaveCriticalSection(&(This->lock));
	/* **** */

	return DS_OK;
}

static DWORD WINAPI IDirectSoundBufferImpl_AddRef(LPDIRECTSOUNDBUFFER8 iface) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD ref;

	TRACE("(%p) ref was %ld, thread is %lx\n",This, This->ref, GetCurrentThreadId());

	IDirectSoundBufferImpl_AddRefAggregate(iface);

	ref = InterlockedIncrement(&(This->ref));

	return ref;
}

static DWORD WINAPI IDirectSoundBufferImpl_Release(LPDIRECTSOUNDBUFFER8 iface) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD ref=0;

	TRACE("(%p) ref was %ld, thread is %lx\n",This, This->ref, GetCurrentThreadId());

	/* NOTE: due to the aggregation of SoundBuffers and Sound3DBuffers, it is possible
	   that there are aggregate refs, but no actual refs on this buffer, so to 
	   prevent negative refcounts... (as tested on WinXP) */
	if( This->ref > 0 )
		ref = InterlockedDecrement(&(This->ref));


	/* this will handle any actual deletion if necessary */
	IDirectSoundBufferImpl_ReleaseAggregate(iface);

	return ref;
}

/* maintains the total outstanding refcounts between the DirectSoundBuffer and the 
   DirectSound3DBuffer classes */
DWORD WINAPI IDirectSoundBufferImpl_AddRefAggregate(LPDIRECTSOUNDBUFFER8 iface) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD ref;

	TRACE("(%p) ref was %ld, thread is %lx\n", This, This->aggref, GetCurrentThreadId());

	ref = InterlockedIncrement(&(This->aggref));
	if (!ref) {
		FIXME("thread-safety alert! AddRef-ing with a zero refcount!\n");
	}
	return ref;
}

DWORD WINAPI IDirectSoundBufferImpl_ReleaseAggregate(LPDIRECTSOUNDBUFFER8 iface) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	int	i;
	DWORD ref;

	TRACE("(%p) ref was %ld, thread is %lx\n", This, This->aggref, GetCurrentThreadId());

	ref = InterlockedDecrement(&(This->aggref));
	if (ref) return ref;

	RtlAcquireResourceExclusive(&(This->dsound->lock), TRUE);
	for (i=0;i<This->dsound->nrofbuffers;i++)
		if (This->dsound->buffers[i] == This)
			break;

	if (i < This->dsound->nrofbuffers) {
		/* Put the last buffer of the list in the (now empty) position */
		This->dsound->buffers[i] = This->dsound->buffers[This->dsound->nrofbuffers - 1];
		This->dsound->nrofbuffers--;
		This->dsound->buffers = HeapReAlloc(dsound_heap,0,This->dsound->buffers,sizeof(LPDIRECTSOUNDBUFFER8)*This->dsound->nrofbuffers);
		TRACE("buffer count is now %d\n", This->dsound->nrofbuffers);
		IDirectSound_Release((LPDIRECTSOUND)This->dsound);
	}
	RtlReleaseResource(&(This->dsound->lock));

	dump_sound_end(This);

	DeleteCriticalSection(&(This->lock));
	if (This->hwbuf)
		IDsDriverBuffer_Release(This->hwbuf);

	if (This->dsound->drvdesc.dwFlags & DSDDESC_USESYSTEMMEMORY && This->swbuf){
	    TRACE("checking if the shared buffer %p should be destroyed {ref = %ld}\n", This->swbuf, This->swbuf->ref);

	    if (InterlockedDecrement((LONG *)(&This->swbuf->ref)) <= 0){
	        TRACE("destroying the software data buffer %p\n", This->swbuf->buffer);
		    HeapFree(dsound_heap,0,This->swbuf->buffer);
	        HeapFree(dsound_heap, 0, This->swbuf);

	        This->swbuf = NULL;
	    }
	}

	if (This->ds3db) {
		DeleteCriticalSection(&This->ds3db->lock);
		HeapFree(dsound_heap, 0, This->ds3db);
	}

	/* manage the ref count on the positions list object */
	if (This->positions)
	{
	    InterlockedDecrement((LONG *)&(This->positions->refCount));

	    if (This->positions->refCount <= 0)
	    {
	        if (This->positions->notifies)
	            HeapFree(dsound_heap, 0, This->positions->notifies);

	        This->positions->notifies = NULL;
	        This->positions->nrofnotifies = 0;

	        HeapFree(dsound_heap, 0, This->positions);

	        This->positions = NULL;
	    }
	}


	if (This->iks) {
		HeapFree(dsound_heap, 0, This->iks);
	}

	HeapFree(dsound_heap,0,This);

	return 0;
}

DWORD DSOUND_CalcPlayPosition(IDirectSoundBufferImpl *This,
			      DWORD state, DWORD pplay, DWORD pwrite, DWORD pmix,
			      DWORD bmix, DWORD bacc, DWORD *rem)
{
	DWORD bplay;
	BOOL leadin;

	TRACE("primary playpos=%ld, mixpos=%ld\n", pplay, pmix);
	TRACE("this mixpos=%ld, acc=%ld, time=%ld\n", bmix, bacc, GetTickCount());

	/* the actual primary play position (pplay) is always behind last mixed (pmix),
	 * unless the computer is too slow or something */
	/* we need to know how far away we are from there */
#if 0 /* we'll never fill the primary entirely */
	if (pmix == pplay) {
		if ((state == STATE_PLAYING) || (state == STATE_STOPPING)) {
			/* wow, the software mixer is really doing well,
			 * seems the entire primary buffer is filled! */
			pmix += This->dsound->buflen;
		}
		/* else: the primary buffer is not playing, so probably empty */
	}
#endif
	if (pmix < pplay) pmix += This->dsound->buflen; /* wraparound */
	pmix -= pplay;
	/* detect buffer underrun */
	if (pwrite < pplay) pwrite += This->dsound->buflen; /* wraparound */
	pwrite -= pplay;
	if (pmix > (DS_SND_QUEUE_MAX * This->dsound->fraglen + pwrite + This->dsound->writelead)) {
		WARN("detected an underrun: primary queue was %ld\n",pmix);
		pmix = 0;
	}
	/* divide the offset by its sample size */
	pmix /= This->dsound->wfx.nBlockAlign;
	TRACE("primary back-samples=%ld\n",pmix);
	/* adjust for our frequency */
	pmix = pmix * This->freqAdjust;
	if (rem) *rem = pmix & ((1 << DSOUND_FREQSHIFT)-1);
	pmix = pmix >> DSOUND_FREQSHIFT;
	/* multiply by our own sample size */
	pmix *= This->wfx.nBlockAlign;
	TRACE("this back-offset=%ld\n", pmix);
	/* subtract from our last mixed position */
	bplay = bmix;
	while (bplay < pmix) bplay += This->buflen; /* wraparound */
	bplay -= pmix;
	if (pmix / This->buflen > 1 )
		WARN("Wraparound detected! Secondary buffer may loop!\n");
	if (This->leadin) {
		if (bmix < This->startpos) {
			leadin = (bplay > bmix) && (bplay < This->startpos);
		} else {
			leadin = (bplay < This->startpos) || (bplay > bmix);
		}
	}
	else leadin = FALSE;
	if (leadin) {
		/* seems we haven't started playing yet */
		TRACE("this still in lead-in phase\n");
		bplay = This->startpos;
		if (rem) *rem = 0;
	}
	if (pmix && bplay == This->probably_valid_to) {
		/* some movie playback systems hate when we return
		 * a value equal to probably_valid_to, let's offset
		 * it a little bit to avoid this condition */
		bplay += This->wfx.nBlockAlign;
		while (bplay >= This->buflen) bplay -= This->buflen; /* wraparound */
	}
	/* return the result */
	return bplay;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetCurrentPosition(
	LPDIRECTSOUNDBUFFER8 iface,LPDWORD playpos,LPDWORD writepos
) {
	HRESULT	hres;
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD	fake_playpos;

	TRACE("(%p,%p,%p)\n",This,playpos,writepos);

	if (This->hwbuf) {
		hres=IDsDriverBuffer_GetPosition(This->hwbuf,playpos,writepos);
		if (hres)
		    return hres;
	}
	else {
		DWORD pplay, pwrite, lplay, splay, sacc, pstate;
		if (playpos==NULL) playpos=&fake_playpos;
		if (This->state != STATE_PLAYING) {
			/* we haven't been merged into the primary buffer (yet) */
			*playpos = This->buf_mixpos;
		}
		else {
			/* let's get this exact; first, recursively call GetPosition on the primary */
			EnterCriticalSection(&(This->dsound->mixlock));
			EnterCriticalSection(&(This->lock));
			DSOUND_PrimaryGetPosition(This->dsound, &pplay, &pwrite);
			/* detect HEL mode underrun */
			pstate = This->dsound->state;
			if (!(This->dsound->hwbuf || This->dsound->pwqueue)) {
				TRACE("detected an underrun\n");
				/* pplay = ? */
				if (pstate == STATE_PLAYING)
					pstate = STATE_STARTING;
				else if (pstate == STATE_STOPPING)
					pstate = STATE_STOPPED;
			}
			/* get data for ourselves while we still have the lock */
			pstate &= This->state;
			lplay = This->primary_mixpos;
			splay = This->buf_mixpos;
			sacc = This->freqAcc;
			if ((This->dsbd.dwFlags & DSBCAPS_GETCURRENTPOSITION2) || This->dsound->hwbuf) {
				/* calculate play position using this */
				*playpos = DSOUND_CalcPlayPosition(This, pstate, pplay, pwrite, lplay, splay, sacc, NULL);
			} else {
				/* (unless the app isn't using GETCURRENTPOSITION2) */
				/* don't know exactly how this should be handled...
				 * the docs says that play cursor is reported as directly
				 * behind write cursor, hmm... */
				/* let's just do what might work for Half-Life */
				DWORD wp;
				wp = (This->dsound->pwplay + DS_HEL_MARGIN) * This->dsound->fraglen;
				while (wp >= This->dsound->buflen)
					wp -= This->dsound->buflen;
				*playpos = DSOUND_CalcPlayPosition(This, pstate, wp, pwrite, lplay, splay, sacc, NULL);
			}
			LeaveCriticalSection(&(This->lock));
			LeaveCriticalSection(&(This->dsound->mixlock));
		}
		/* Give the buffer's already mixed position if we're using Steam/HL, since we can't guarantee
		 * we'll be able to remix the start of what's been changed if it's too close to the play
		 * position.
		 */
		TRACE("%p, %p\n", writepos, This);
		TRACE("%ld\n", This->dsbd.dwFlags);
		TRACE("%p\n", This->dsound);
		if (writepos) {
			if ((This->dsbd.dwFlags & DSBCAPS_GETCURRENTPOSITION2) || This->dsound->hwbuf)
				*writepos = *playpos;
			else
				*writepos = This->buf_mixpos;
		}
	}
	if (writepos) {
		if (This->state != STATE_STOPPED)
		{
			/* The idea is to get the delay as small as we possibly can.  Unfortunately, we
			 * need to accomadate the mixing thread and the queueing (in the non-mmap case.)
			 * Hence the DS_THREADWAIT_MARGIN (60ms) and DS_HEL_QUEUE (50ms).
			 */
			if( This->dsound->hwbuf )
				*writepos += This->writelead * DS_THREADWAIT_MARGIN;
			else
				*writepos += This->writelead * DS_HEL_QUEUE + This->writelead * DS_THREADWAIT_MARGIN;
		}
  COMPUTE_WRAPAROUND( *writepos, This->buflen );
	}
	EnterCriticalSection(&(This->lock));
	if (playpos) This->last_playpos = *playpos;
	LeaveCriticalSection(&(This->lock));
	TRACE("playpos = %ld, writepos = %ld (%p, time=%ld)\n", playpos?*playpos:0, writepos?*writepos:0, This, GetTickCount());
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetStatus(
	LPDIRECTSOUNDBUFFER8 iface,LPDWORD status
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%p), thread is %lx\n",This,status,GetCurrentThreadId());

	if (status == NULL)
		return DSERR_INVALIDPARAM;

	*status = 0;
	if ((This->state == STATE_STARTING) || (This->state == STATE_PLAYING)) {
		*status |= DSBSTATUS_PLAYING;
		if (This->playflags & DSBPLAY_LOOPING)
			*status |= DSBSTATUS_LOOPING;
	}

	TRACE("status=%lx\n", *status);
	return DS_OK;
}


static HRESULT WINAPI IDirectSoundBufferImpl_GetFormat(
	LPDIRECTSOUNDBUFFER8 iface,LPWAVEFORMATEX lpwf,DWORD wfsize,LPDWORD wfwritten
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%p,%ld,%p)\n",This,lpwf,wfsize,wfwritten);

	if (wfsize>sizeof(This->wfx))
		wfsize = sizeof(This->wfx);
	if (lpwf) {	/* NULL is valid */
		memcpy(lpwf,&(This->wfx),wfsize);
		if (wfwritten)
			*wfwritten = wfsize;
	} else
		if (wfwritten)
			*wfwritten = sizeof(This->wfx);
		else
			return DSERR_INVALIDPARAM;

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_Lock(
	LPDIRECTSOUNDBUFFER8 iface,DWORD writecursor,DWORD writebytes,LPVOID lplpaudioptr1,LPDWORD audiobytes1,LPVOID lplpaudioptr2,LPDWORD audiobytes2,DWORD flags
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD write_end;

	TRACE("(%p,%ld,%ld,%p,%p,%p,%p,0x%08lx) at %ld\n",
		This,
		writecursor,
		writebytes,
		lplpaudioptr1,
		audiobytes1,
		lplpaudioptr2,
		audiobytes2,
		flags,
		GetTickCount()
	);

	if (flags & DSBLOCK_FROMWRITECURSOR) {
		DWORD writepos;
		/* GetCurrentPosition does too much magic to duplicate here */
		IDirectSoundBufferImpl_GetCurrentPosition(iface, NULL, &writepos);
		writecursor += writepos;
	}
 COMPUTE_USER_WRAPAROUND( writecursor, This->buflen );
	if (flags & DSBLOCK_ENTIREBUFFER)
		writebytes = This->buflen;
	if (writebytes > This->buflen)
		writebytes = This->buflen;

	write_end = (writecursor + writebytes) % This->buflen;

	assert(audiobytes1!=audiobytes2);
	assert(lplpaudioptr1!=lplpaudioptr2);

	EnterCriticalSection(&This->lock);

	if ((writebytes == This->buflen) &&
	    ((This->state == STATE_STARTING) ||
	     (This->state == STATE_PLAYING)))
		/* some games, like Half-Life, try to be clever (not) and
		 * keep one secondary buffer, and mix sounds into it itself,
		 * locking the entire buffer every time... so we can just forget
		 * about tracking the last-written-to-position... */
		This->probably_valid_to = (DWORD)-1;
	else
	{
		/* don't update probably_valid_to if it looks like the app is just
	 	* clearing what's just finished playing */
		if (This->probably_valid_to == (DWORD)-1 || write_end != This->last_playpos)
			This->probably_valid_to = writecursor;
	}

	if (!(This->dsound->drvdesc.dwFlags & DSDDESC_DONTNEEDSECONDARYLOCK) && This->hwbuf) {
		IDsDriverBuffer_Lock(This->hwbuf,
				     lplpaudioptr1, audiobytes1,
				     lplpaudioptr2, audiobytes2,
				     writecursor, writebytes,
				     0);
	}
	else {
		BOOL remix = FALSE;
		DWORD remix_pos = 0;	/* only remix what we have to */
		if (writecursor+writebytes <= This->buflen) {
			*(LPBYTE*)lplpaudioptr1 = This->swbuf->buffer+writecursor;
			*audiobytes1 = writebytes;
			if (lplpaudioptr2)
				*(LPBYTE*)lplpaudioptr2 = NULL;
			if (audiobytes2)
				*audiobytes2 = 0;
			TRACE("->%ld.0\n",writebytes);
		} else {
			*(LPBYTE*)lplpaudioptr1 = This->swbuf->buffer+writecursor;
			*audiobytes1 = This->buflen-writecursor;
			if (lplpaudioptr2)
				*(LPBYTE*)lplpaudioptr2 = This->swbuf->buffer;
			if (audiobytes2)
				*audiobytes2 = writebytes-(This->buflen-writecursor);
			TRACE("->%ld.%ld\n",*audiobytes1,audiobytes2?*audiobytes2:0);
		}
		/* This all covers the case where we have more mixed than what the
		 * app might expect.  Since we've changed the mixing size to approximate
		 * that on windows, we shouldn't need this stuff anymore.
		 */
		if (This->state == STATE_PLAYING) {
			/* Use 50ms before play position for the play lead unless hardware (mmap) */
			DWORD play_lead;
			if( This->dsound->hwbuf )
				play_lead = This->last_playpos + This->writelead;
			else
				play_lead = This->last_playpos + This->writelead * DS_HEL_QUEUE;
    COMPUTE_WRAPAROUND( play_lead, This->buflen );

			if (writebytes == This->buflen)
			{
				/* If they've locked the entire buffer, then we really have no choice;
				 * we have to cancel everything..
				 */
				DSOUND_MixCancelAt(This, This->last_playpos);
			}
			else
			{
				/* if the segment between playpos+writelead (writepos) and buf_mixpos
				* is touched, we need to cancel some mixing */
				/* we'll assume that the app always calls GetCurrentPosition before
				* locking a playing buffer, so that last_playpos is up-to-date */

				TRACE("play: %ld  lead: %ld  mix: %ld  -  write: %ld  bytes: %ld  w_end: %ld\n",
				This->last_playpos, play_lead, This->buf_mixpos,
				writecursor, writebytes, write_end);

				/* Make sure play_lead is behind buf_mixpos */
				if ((This->last_playpos < play_lead) && (play_lead <= This->buf_mixpos))
				{
					/* play_lead at start */
					if ((This->buf_mixpos > writecursor && writecursor >= play_lead) ||
					(This->last_playpos < writecursor && play_lead <= write_end &&
					write_end < This->buf_mixpos))
					{
						if (writecursor < play_lead || writecursor > This->buf_mixpos)
							remix_pos = play_lead;
						else
							remix_pos = writecursor;
						remix = TRUE;
					}
				}
				else
				if (This->last_playpos >= This->buf_mixpos) {
					if (This->last_playpos < play_lead)
					{
						/* play_lead at end */
						if ((play_lead <= writecursor || writecursor < This->buf_mixpos) ||
						(This->last_playpos < write_end &&
						(write_end >= play_lead || This->buf_mixpos > write_end)))
						{
							if (This->buf_mixpos < writecursor && writecursor < play_lead)
								remix_pos = play_lead;
							else
								remix_pos = writecursor;
							remix = TRUE;
						}
					}
					if (play_lead <= This->buf_mixpos)
					{
						/* play_lead at start */
						if ((play_lead <= writecursor && writecursor < This->buf_mixpos) ||
						((writecursor >= This->last_playpos || writecursor < play_lead) &&
						(write_end >= play_lead && This->buf_mixpos > write_end)))
						{
							if (writecursor < play_lead || writecursor > This->buf_mixpos)
								remix_pos = play_lead;
							else
								remix_pos = writecursor;
							remix = TRUE;
						}
					}
				}
				if (remix) {
					TRACE("locking prebuffered region, ouch\n");
					DSOUND_MixCancelAt(This, remix_pos);
				}
			}
		}
	}
	TRACE("probably_valid_to=%ld\n", This->probably_valid_to);
	LeaveCriticalSection(&This->lock);
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_SetCurrentPosition(
	LPDIRECTSOUNDBUFFER8 iface,DWORD newpos
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%ld)\n",This,newpos);

 COMPUTE_USER_WRAPAROUND( newpos, This->buflen );

	/* **** */
	EnterCriticalSection(&(This->lock));
	This->buf_mixpos = newpos & ~(This->wfx.nBlockAlign-1);
	if( This->state == STATE_PLAYING )
	{
		This->leadin = TRUE;
		This->startpos = This->buf_mixpos;
		This->state = STATE_STARTING;
	}
	if (This->hwbuf)
		IDsDriverBuffer_SetPosition(This->hwbuf, This->buf_mixpos);
	else
		This->need_remix = TRUE;
	LeaveCriticalSection(&(This->lock));
	/* **** */


	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_SetPan(
	LPDIRECTSOUNDBUFFER8 iface,LONG pan
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	LONG oldPan;

	TRACE("(%p,%ld)\n",This,pan);

	if ((pan > DSBPAN_RIGHT) || (pan < DSBPAN_LEFT))
		return DSERR_INVALIDPARAM;

	/* You cannot use both pan and 3D controls */
	if (!(This->dsbd.dwFlags & DSBCAPS_CTRLPAN) ||
	    (This->dsbd.dwFlags & DSBCAPS_CTRL3D))
		return DSERR_CONTROLUNAVAIL;

	/* **** */
	EnterCriticalSection(&(This->lock));

	oldPan = This->volpan.lPan;
	This->volpan.lPan = pan;

	if (pan != oldPan) {
		DSOUND_RecalcVolPan(&(This->volpan));

		if (This->hwbuf) {
			IDsDriverBuffer_SetVolumePan(This->hwbuf, &(This->volpan));
		}
		else DSOUND_ForceRemix(This);
	}

	LeaveCriticalSection(&(This->lock));
	/* **** */

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetPan(
	LPDIRECTSOUNDBUFFER8 iface,LPLONG pan
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%p)\n",This,pan);

	if (pan == NULL)
		return DSERR_INVALIDPARAM;

	*pan = This->volpan.lPan;

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_Unlock(
	LPDIRECTSOUNDBUFFER8 iface,LPVOID p1,DWORD x1,LPVOID p2,DWORD x2
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD write_start, probably_valid_to;

	TRACE("(%p,%p,%ld,%p,%ld):stub\n", This,p1,x1,p2,x2);

#if 0
	/* Preprocess 3D buffers... */

	/* This is highly experimental and liable to break things */
	if (This->dsbd.dwFlags & DSBCAPS_CTRL3D)
		DSOUND_Create3DBuffer(This);
#endif

	/* *** */
	EnterCriticalSection(&This->lock);

	if (!(This->dsound->drvdesc.dwFlags & DSDDESC_DONTNEEDSECONDARYLOCK) && This->hwbuf) {
		IDsDriverBuffer_Unlock(This->hwbuf, p1, x1, p2, x2);
	}

	write_start = ((LPBYTE)p1)-This->swbuf->buffer;
	if (p2) probably_valid_to = (((LPBYTE)p2)-This->swbuf->buffer) + x2;
	else probably_valid_to = (((LPBYTE)p1)-This->swbuf->buffer) + x1;
	while (probably_valid_to >= This->buflen)
		probably_valid_to -= This->buflen;
	if ((probably_valid_to == 0) && ((x1+x2) == This->buflen) &&
	    ((This->state == STATE_STARTING) ||
	     (This->state == STATE_PLAYING)))
		/* see IDirectSoundBufferImpl_Lock */
		probably_valid_to = (DWORD)-1;
	/* don't update probably_valid_to if it looks like the app is just
	 * clearing what's just finished playing */
	if (This->probably_valid_to == write_start ||
	    probably_valid_to != This->last_playpos)
		This->probably_valid_to = probably_valid_to;

	LeaveCriticalSection(&This->lock);
	/* *** */

	TRACE("probably_valid_to=%ld\n", This->probably_valid_to);

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_Restore(
	LPDIRECTSOUNDBUFFER8 iface
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	FIXME("(%p):stub\n",This);
	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetFrequency(
	LPDIRECTSOUNDBUFFER8 iface,LPDWORD freq
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	TRACE("(%p,%p)\n",This,freq);

	if (freq == NULL)
		return DSERR_INVALIDPARAM;

	*freq = This->freq;
	TRACE("-> %ld\n", *freq);

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_SetFX(
	LPDIRECTSOUNDBUFFER8 iface,DWORD dwEffectsCount,LPDSEFFECTDESC pDSFXDesc,LPDWORD pdwResultCodes
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD u;

	FIXME("(%p,%lu,%p,%p): stub\n",This,dwEffectsCount,pDSFXDesc,pdwResultCodes);

	if (pdwResultCodes)
		for (u=0; u<dwEffectsCount; u++) pdwResultCodes[u] = DSFXR_UNKNOWN;

	return DSERR_CONTROLUNAVAIL;
}

static HRESULT WINAPI IDirectSoundBufferImpl_AcquireResources(
	LPDIRECTSOUNDBUFFER8 iface,DWORD dwFlags,DWORD dwEffectsCount,LPDWORD pdwResultCodes
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	DWORD u;

	FIXME("(%p,%08lu,%lu,%p): stub\n",This,dwFlags,dwEffectsCount,pdwResultCodes);

	if (pdwResultCodes)
		for (u=0; u<dwEffectsCount; u++) pdwResultCodes[u] = DSFXR_UNKNOWN;

	return DSERR_CONTROLUNAVAIL;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetObjectInPath(
	LPDIRECTSOUNDBUFFER8 iface,REFGUID rguidObject,DWORD dwIndex,REFGUID rguidInterface,LPVOID* ppObject
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);

	FIXME("(%p,%s,%lu,%s,%p): stub\n",This,debugstr_guid(rguidObject),dwIndex,debugstr_guid(rguidInterface),ppObject);

	return DSERR_CONTROLUNAVAIL;
}

static HRESULT WINAPI IDirectSoundBufferImpl_Initialize(
	LPDIRECTSOUNDBUFFER8 iface,LPDIRECTSOUND8 dsound,LPDSBUFFERDESC dbsd
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
	FIXME("(%p,%p,%p):stub\n",This,dsound,dbsd);
	DPRINTF("Re-Init!!!\n");
	return DSERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI IDirectSoundBufferImpl_GetCaps(
	LPDIRECTSOUNDBUFFER8 iface,LPDSBCAPS caps
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);
  	TRACE("(%p)->(%p)\n",This,caps);

	if (caps == NULL)
		return DSERR_INVALIDPARAM;


	if (caps->dwSize != sizeof(*caps)){
		ERR("invalid structure size!\n");

		return DSERR_INVALIDPARAM;
	}


	caps->dwFlags = This->dsbd.dwFlags;
	if (This->hwbuf) caps->dwFlags |= DSBCAPS_LOCHARDWARE;
	else caps->dwFlags |= DSBCAPS_LOCSOFTWARE;

	caps->dwBufferBytes = This->buflen;

	/* This value represents the speed of the "unlock" command.
	   As unlock is quite fast (it does not do anything), I put
	   4096 ko/s = 4 Mo / s */
	/* FIXME: hwbuf speed */
	caps->dwUnlockTransferRate = 4096;
	caps->dwPlayCpuOverhead = 0;

	return DS_OK;
}

static HRESULT WINAPI IDirectSoundBufferImpl_QueryInterface(
	LPDIRECTSOUNDBUFFER8 iface,REFIID riid,LPVOID *ppobj
) {
	ICOM_THIS(IDirectSoundBufferImpl,iface);

	TRACE("(%p,%s,%p)\n",This,debugstr_guid(riid),ppobj);

	if ( IsEqualGUID( &IID_IUnknown, riid ) ||
	     IsEqualGUID( &IID_IDirectSoundBuffer, riid ) ||
	     IsEqualGUID( &IID_IDirectSoundBuffer8, riid ) ) {
		IDirectSoundBuffer8_AddRef(iface);
		*ppobj = (LPVOID)iface;
		return S_OK;
	}


	if ( IsEqualGUID( &IID_IDirectSoundNotify, riid ) ) {
	    IDirectSoundNotifyImpl *dsn;


	    TRACE("grabbing a notification object for the buffer %p\n", This);

	    /* the buffer was not created with notification capabilities => fail */
		if (!(This->dsbd.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY)) {
			ERR("notification capabilities aren't enabled for this buffer\n");
			*ppobj = NULL;

			return E_NOINTERFACE;
		}


	    /* create a new notification object for the caller to play with */
	    IDirectSoundNotifyImpl_Create(This, NULL, &dsn);

	    if (dsn == NULL) {
			ERR("could not create a IDirectSoundNotify object!\n");

			*ppobj = NULL;

			return E_FAIL;
		}


	    *ppobj = (LPVOID)dsn;

		return S_OK;
	}


	if ( IsEqualGUID( &IID_IDirectSound3DBuffer, riid ) ) {
		if (!(This->dsbd.dwFlags & DSBCAPS_CTRL3D)) {
			*ppobj = NULL;
			return E_NOINTERFACE;
		}
		if (!This->ds3db) {
			ERR("3DBuffer should have been created in SecondaryBuffer_Create\n");
			*ppobj = NULL;
			return E_FAIL;
		}
		*ppobj = This->ds3db;
		if (*ppobj) {
			IDirectSound3DBuffer_AddRef((LPDIRECTSOUND3DBUFFER)*ppobj);
			return S_OK;
		}
		return E_FAIL;
	}

        if ( IsEqualGUID( &IID_IDirectSound3DListener, riid ) ) {
		ERR("app requested IDirectSound3DListener on secondary buffer\n");
		*ppobj = NULL;
		return E_FAIL;
	}

	if ( IsEqualGUID( &IID_IKsPropertySet, riid ) ) {
		if (!This->iks)
			IKsPropertySetImpl_Create(This, &This->iks);
		*ppobj = This->iks;
		if (*ppobj) {
			IKsPropertySet_AddRef((LPKSPROPERTYSET)*ppobj);
			return S_OK;
		}
		return E_FAIL;
	}

	FIXME( "Unknown IID %s\n", debugstr_guid( riid ) );

	*ppobj = NULL;

	return E_NOINTERFACE;
}

static ICOM_VTABLE(IDirectSoundBuffer8) dsbvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IDirectSoundBufferImpl_QueryInterface,
	IDirectSoundBufferImpl_AddRef,
	IDirectSoundBufferImpl_Release,
	IDirectSoundBufferImpl_GetCaps,
	IDirectSoundBufferImpl_GetCurrentPosition,
	IDirectSoundBufferImpl_GetFormat,
	IDirectSoundBufferImpl_GetVolume,
	IDirectSoundBufferImpl_GetPan,
        IDirectSoundBufferImpl_GetFrequency,
	IDirectSoundBufferImpl_GetStatus,
	IDirectSoundBufferImpl_Initialize,
	IDirectSoundBufferImpl_Lock,
	IDirectSoundBufferImpl_Play,
	IDirectSoundBufferImpl_SetCurrentPosition,
	IDirectSoundBufferImpl_SetFormat,
	IDirectSoundBufferImpl_SetVolume,
	IDirectSoundBufferImpl_SetPan,
	IDirectSoundBufferImpl_SetFrequency,
	IDirectSoundBufferImpl_Stop,
	IDirectSoundBufferImpl_Unlock,
	IDirectSoundBufferImpl_Restore,
	IDirectSoundBufferImpl_SetFX,
	IDirectSoundBufferImpl_AcquireResources,
	IDirectSoundBufferImpl_GetObjectInPath
};

HRESULT WINAPI SecondaryBuffer_Create(
	IDirectSoundImpl *This,
	IDirectSoundBufferImpl **pdsb,
	LPCDSBUFFERDESC dsbd)
{
	IDirectSoundBufferImpl *dsb;
	LPWAVEFORMATEX          wfex = dsbd->lpwfxFormat;
	HRESULT                 err = DS_OK;
	DWORD                   capf = 0;
	DWORD                   bufferBytes;
	int                     use_hw;


	if (dsbd->dwBufferBytes < DSBSIZE_MIN || dsbd->dwBufferBytes > DSBSIZE_MAX) {
		ERR("invalid sound buffer size %ld\n", dsbd->dwBufferBytes);
		return DSERR_INVALIDPARAM; /* FIXME: which error? */
	}


	if (dsbd->dwBufferBytes & (wfex->nBlockAlign - 1)){
	    ERR("ERROR-> the buffer size is not aligned to the block size! {bufferBytes = %ld, blockAlign = %d}\n",
	            dsbd->dwBufferBytes, wfex->nBlockAlign);
	}

	/* make sure the buffer size is aligned to the wave format's channel and bit count */
	bufferBytes = (dsbd->dwBufferBytes + (wfex->nBlockAlign - 1)) & ~(wfex->nBlockAlign - 1);


	dsb = (IDirectSoundBufferImpl*)HeapAlloc(dsound_heap,HEAP_ZERO_MEMORY,sizeof(*dsb));
	dsb->ref = dsb->aggref = 1;
	dsb->dsound = This;
	ICOM_VTBL(dsb) = &dsbvt;

	memcpy(&dsb->dsbd, dsbd, sizeof(*dsbd));

	/* make sure the correct buffer size is copied into the object */
	dsb->dsbd.dwBufferBytes = bufferBytes;

	if (wfex)
		memcpy(&dsb->wfx, wfex, sizeof(dsb->wfx));


	TRACE("Created buffer at %p\n", dsb);

	dsb->buflen = bufferBytes;
	dsb->freq = dsbd->lpwfxFormat->nSamplesPerSec;
	dsb->leftPreviousAmpFactor = -1;	/*disable smoothing when next mixing that sound on the left channel*/
	dsb->rightPreviousAmpFactor = -1;   /*disable smoothing when next mixing that sound on the right channel*/

	/* Check necessary hardware mixing capabilities */
	if (wfex->nChannels==2) capf |= DSCAPS_SECONDARYSTEREO;
	else capf |= DSCAPS_SECONDARYMONO;
	if (wfex->wBitsPerSample==16) capf |= DSCAPS_SECONDARY16BIT;
	else capf |= DSCAPS_SECONDARY8BIT;
	use_hw = (This->drvcaps.dwFlags & capf) == capf;

	/* FIXME: check hardware sample rate mixing capabilities */
	/* FIXME: check app hints for software/hardware buffer (STATIC, LOCHARDWARE, etc) */
	/* FIXME: check whether any hardware buffers are left */
	/* FIXME: handle DSDHEAP_CREATEHEAP for hardware buffers */

	/* Allocate system memory if applicable */
	if ((This->drvdesc.dwFlags & DSDDESC_USESYSTEMMEMORY) || !use_hw) {
	    dsb->swbuf = (IDirectSoundSWBuffer *)HeapAlloc(dsound_heap, 0, sizeof(IDirectSoundSWBuffer));
	    dsb->swbuf->ref = 1;


	    if (dsb->swbuf == NULL)
	        err = DSERR_OUTOFMEMORY;

	    else{
	        dsb->swbuf->buffer = (LPBYTE)HeapAlloc(dsound_heap,0,dsb->buflen);

	        if (dsb->swbuf->buffer == NULL)
	            err = DSERR_OUTOFMEMORY;

	        else
	        {
	            /* Some games do not follow the DirectSound spec, which specifically states
	             * that no game should rely on a freshly-created sound buffer being filled
	             * with silence.
	             */
	            if (wfex->wBitsPerSample==8)
	                memset(dsb->swbuf->buffer, 128, dsb->buflen);
	            else
	                memset(dsb->swbuf->buffer, 0, dsb->buflen);
	        }
	    }
	}

	/* Allocate the hardware buffer */
	if (use_hw && (err == DS_OK)) {
	    err = IDsDriver_CreateSoundBuffer(  This->driver,
	                                        wfex,
	                                        dsbd->dwFlags,
	                                        0,
	                                        &(dsb->buflen), 
	                                        (dsb->swbuf) ? (&(dsb->swbuf->buffer)) : NULL,
	                                        (LPVOID*)&(dsb->hwbuf));
	}

	if (err != DS_OK) {
	    if (dsb->swbuf){
	        if (dsb->swbuf->buffer)
	            HeapFree(dsound_heap,0,dsb->swbuf->buffer);

	        HeapFree(dsound_heap, 0, dsb->swbuf);
	    }
		HeapFree(dsound_heap,0,dsb);
		dsb = NULL;
		return err;
	}
	/* calculate fragment size and write lead */
	DSOUND_RecalcFormat(dsb);

	/* It's not necessary to initialize values to zero since */
	/* we allocated this structure with HEAP_ZERO_MEMORY... */
	dsb->playpos = 0;
	dsb->buf_mixpos = 0;
	dsb->state = STATE_STOPPED;

	dsb->freqAdjust = (dsb->freq << DSOUND_FREQSHIFT) /
		This->wfx.nSamplesPerSec;
	dsb->nAvgBytesPerSec = dsb->freq *
		dsbd->lpwfxFormat->nBlockAlign;

	if (dsbd->dwFlags & DSBCAPS_CTRL3D) {
		IDirectSound3DBufferImpl_Create(dsb, &dsb->ds3db);
		DSOUND_Recalc3DBuffer(dsb);
	}
	else
		DSOUND_RecalcVolPan(&(dsb->volpan));


	/* create the positions list object */
	if (dsbd->dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY)
	{
	    TRACE("creating a notification positions array for the buffer object %p\n", dsb);
	    dsb->positions = (IDirectSoundNotifyPositions *)HeapAlloc(dsound_heap, 0, sizeof(IDirectSoundNotifyPositions));
	    dsb->positions->refCount = 1;
	    dsb->positions->notifies = NULL;
	    dsb->positions->nrofnotifies = 0;
	}


	CRITICAL_SECTION_DEFINE(&(dsb->lock));

	/* register buffer */
	RtlAcquireResourceExclusive(&(This->lock), TRUE);
	if (!(dsbd->dwFlags & DSBCAPS_PRIMARYBUFFER)) {
		IDirectSoundBufferImpl **newbuffers = (IDirectSoundBufferImpl**)HeapReAlloc(dsound_heap,0,This->buffers,sizeof(IDirectSoundBufferImpl*)*(This->nrofbuffers+1));
		if (newbuffers) {
			This->buffers = newbuffers;
			This->buffers[This->nrofbuffers] = dsb;
			This->nrofbuffers++;
			TRACE("buffer count is now %d\n", This->nrofbuffers);
		} else {
			ERR("out of memory for buffer list! Current buffer count is %d\n", This->nrofbuffers);
			err = DSERR_OUTOFMEMORY;
		}
	}
	RtlReleaseResource(&(This->lock));
	IDirectSound8_AddRef((LPDIRECTSOUND8)This);

	if (err != DS_OK) {
		/* oops... */
		IDirectSoundBuffer8_Release((LPDIRECTSOUNDBUFFER8)dsb);
		*pdsb = NULL;
		return err;
	}

	*pdsb = dsb;
	return S_OK;
}
