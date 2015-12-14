/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright 2000-2004 TransGaming Technologies, Inc.
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdio.h>
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
#include "wine/profile.h"
#include "dsound.h"
#include "dsdriver.h"
#include "dsound_private.h"

/* #define DUMP_SOUND_DATA */
#define DUMP_PRIMARY
#define DUMP_SECONDARY
#define DUMP_PATH "/tmp/"

WINE_DEFAULT_DEBUG_CHANNEL(dsound);

#ifdef DUMP_SOUND_DATA
typedef struct {
	FILE* fil;
	DWORD hdrsiz, len;
} t_dump_state;
#endif

void dump_sound_data(IDirectSoundBufferImpl *dsb, DWORD start, DWORD len)
{
#if defined(DUMP_SOUND_DATA) && defined(DUMP_SECONDARY)
	t_dump_state* st = dsb->dump_state;
	DWORD t;
	if (!st) {
		static int count;
		char name[128];
		DWORD riff = FOURCC_RIFF;
		DWORD wave = mmioFOURCC('W','A','V','E');
		DWORD fmt = mmioFOURCC('f','m','t',' ');
		DWORD data = mmioFOURCC('d','a','t','a');
		st = HeapAlloc(dsound_heap, HEAP_ZERO_MEMORY, sizeof(t_dump_state));
		sprintf(name, DUMP_PATH "ds_%d.wav", ++count);
		TRACE("dumping stream %p to %s\n", dsb, name);
		st->fil = fopen(name, "w+");
		/* 0x00: "RIFF" chunk fourcc */
		fwrite(&riff, sizeof(riff), 1, st->fil);
		/* 0x04: size of RIFF chunk (file size minus RIFF header (8 bytes)) */
		t = 0x24;
		fwrite(&t, sizeof(t), 1, st->fil);
		/* 0x08: "WAVE" fourcc */
		fwrite(&wave, sizeof(wave), 1, st->fil);
		/* 0x0c: "fmt" fourcc */
		fwrite(&fmt, sizeof(fmt), 1, st->fil);
		/* 0x10: size of fmt chunk */
		t = 0x10; /* sizeof(PCMWAVEFORMAT) */
		fwrite(&t, sizeof(t), 1, st->fil);
		/* 0x14: wave format */
		fwrite(&dsb->wfx, t, 1, st->fil);
		/* 0x24: "data" fourcc */
		fwrite(&data, sizeof(data), 1, st->fil);
		/* 0x28: size of data chunk (file size minus headers (0x2c bytes)) */
		t = 0;
		fwrite(&t, sizeof(t), 1, st->fil);
		/* 0x2c: waveform data */
		st->hdrsiz = 0x2c;
		st->len = 0;
		dsb->dump_state = st;
	}
	TRACE("%p (%ld, %ld): offset=%ld, sample=%ld (%fs)\n",
	      dsb, start, len,
	      st->len, st->len/dsb->wfx.nBlockAlign,
	      (double)(st->len/dsb->wfx.nBlockAlign)/dsb->wfx.nSamplesPerSec);
	/* write raw sound data */
	t = (start + len > dsb->buflen) ? (dsb->buflen - start) : len;
	if (t) st->len += fwrite(dsb->swbuf->buffer + start, 1, t, st->fil);
	t = len - t;
	if (t) st->len += fwrite(dsb->swbuf->buffer, 1, t, st->fil);
	/* update header sizes */
	fseek(st->fil, 4, SEEK_SET);
	t = st->hdrsiz + st->len - 8;
	fwrite(&t, sizeof(t), 1, st->fil);
	fseek(st->fil, st->hdrsiz - 4, SEEK_SET);
	t = st->len;
	fwrite(&t, sizeof(t), 1, st->fil);
	/* seek back to next write position */
	fseek(st->fil, st->hdrsiz + st->len, SEEK_SET);
	/* fflush(st->fil); */
#endif
}

void dump_sound_end(IDirectSoundBufferImpl *dsb)
{
#if defined(DUMP_SOUND_DATA) && defined(DUMP_SECONDARY)
	t_dump_state* st = dsb->dump_state;
	if (st) {
		fclose(st->fil);
		HeapFree(dsound_heap, 0, st);
		dsb->dump_state = NULL;
	}
#endif
}

void dump_sound_output_data(IDirectSoundImpl *dsound, DWORD start, DWORD len)
{
#if defined(DUMP_SOUND_DATA) && defined(DUMP_PRIMARY)
	t_dump_state* st = dsound->dump_state;
	DWORD t;
	if (!st) {
		static int count;
		char name[128];
		DWORD riff = FOURCC_RIFF;
		DWORD wave = mmioFOURCC('W','A','V','E');
		DWORD fmt = mmioFOURCC('f','m','t',' ');
		DWORD data = mmioFOURCC('d','a','t','a');
		st = HeapAlloc(dsound_heap, HEAP_ZERO_MEMORY, sizeof(t_dump_state));
		sprintf(name, DUMP_PATH "ds_out_%d.wav", ++count);
		TRACE("dumping output from %p to %s\n", dsound, name);
		st->fil = fopen(name, "w+");
		/* 0x00: "RIFF" chunk fourcc */
		fwrite(&riff, sizeof(riff), 1, st->fil);
		/* 0x04: size of RIFF chunk (file size minus RIFF header (8 bytes)) */
		t = 0x24;
		fwrite(&t, sizeof(t), 1, st->fil);
		/* 0x08: "WAVE" fourcc */
		fwrite(&wave, sizeof(wave), 1, st->fil);
		/* 0x0c: "fmt" fourcc */
		fwrite(&fmt, sizeof(fmt), 1, st->fil);
		/* 0x10: size of fmt chunk */
		t = 0x10; /* sizeof(PCMWAVEFORMAT) */
		fwrite(&t, sizeof(t), 1, st->fil);
		/* 0x14: wave format */
		fwrite(&dsound->wfx, t, 1, st->fil);
		/* 0x24: "data" fourcc */
		fwrite(&data, sizeof(data), 1, st->fil);
		/* 0x28: size of data chunk (file size minus headers (0x2c bytes)) */
		t = 0;
		fwrite(&t, sizeof(t), 1, st->fil);
		/* 0x2c: waveform data */
		st->hdrsiz = 0x2c;
		st->len = 0;
		dsound->dump_state = st;
	}
	TRACE("%p (%ld, %ld): offset=%ld, sample=%ld (%fs)\n",
	      dsound, start, len,
	      st->len, st->len/dsound->wfx.nBlockAlign,
	      (double)(st->len/dsound->wfx.nBlockAlign)/dsound->wfx.nSamplesPerSec);
	/* write raw sound data */
	t = (start + len > dsound->buflen) ? (dsound->buflen - start) : len;
	if (t) st->len += fwrite(dsound->swbuf->buffer + start, 1, t, st->fil);
	t = len - t;
	if (t) st->len += fwrite(dsound->swbuf->buffer, 1, t, st->fil);
	/* update header sizes */
	fseek(st->fil, 4, SEEK_SET);
	t = st->hdrsiz + st->len - 8;
	fwrite(&t, sizeof(t), 1, st->fil);
	fseek(st->fil, st->hdrsiz - 4, SEEK_SET);
	t = st->len;
	fwrite(&t, sizeof(t), 1, st->fil);
	/* seek back to next write position */
	fseek(st->fil, st->hdrsiz + st->len, SEEK_SET);
	/* fflush(st->fil); */
#endif
}

void dump_sound_output_end(IDirectSoundImpl *dsound)
{
#if defined(DUMP_SOUND_DATA) && defined(DUMP_PRIMARY)
	t_dump_state* st = dsound->dump_state;
	if (st) {
		fclose(st->fil);
		HeapFree(dsound_heap, 0, st);
		dsound->dump_state = NULL;
	}
#endif
}

void DSOUND_RecalcVolPan(PDSVOLUMEPAN volpan)
{
 double temp;

	/* The AmpFactors are expressed in 0.16 fixed point so we can think about going 16 bit MMX operations.
  * This is doable as the largest number we ever have (0dB attenuation) is just 1.000000. The only
  * thing we lose is that 0dB is closer to -0.00001dB but it's consistent for everything so shouldn't
  * be noticable */

 temp = pow(2.0, volpan->lVolume / 600.0);
 if( temp >= 1.0 ) { temp = 0.99999; }
	volpan->dwVolAmpFactor = (ULONG) (temp * 65536);
	/* FIXME: dwPan{Left|Right}AmpFactor */

	/* FIXME: use calculated vol and pan ampfactors */
	temp = (double) (volpan->lVolume - (volpan->lPan > 0 ? volpan->lPan : 0));
 temp = pow(2.0, temp / 600.0);
 if( temp >= 1.0 ) { temp = 0.99999; }
	volpan->dwTotalLeftAmpFactor = (ULONG) ( temp * 65536);

	temp = (double) (volpan->lVolume + (volpan->lPan < 0 ? volpan->lPan : 0));
 temp = pow(2.0, temp / 600.0);
 if( temp >= 1.0 ) { temp = 0.99999;}
	volpan->dwTotalRightAmpFactor = (ULONG) ( temp * 65536 );

	TRACE("left = %lx, right = %lx\n", volpan->dwTotalLeftAmpFactor, volpan->dwTotalRightAmpFactor);
}

void DSOUND_RecalcFormat(IDirectSoundBufferImpl *dsb)
{
	DWORD sw;

	sw = dsb->wfx.nChannels * (dsb->wfx.wBitsPerSample / 8);
	/* calculate the 10ms write lead */
	dsb->writelead = (dsb->freq / 100) * sw;
}

void DSOUND_CheckEvent(IDirectSoundBufferImpl *dsb, int len)
{
	int			i;
	DWORD			offset;
	LPDSBPOSITIONNOTIFY	event;

	if (dsb->positions == NULL || dsb->positions->nrofnotifies == 0)
		return;

	TRACE("(%p) buflen = %ld, playpos = %ld, len = %d\n",
		dsb, dsb->buflen, dsb->playpos, len);
	for (i = 0; i < dsb->positions->nrofnotifies ; i++) {
		event = dsb->positions->notifies + i;
		offset = event->dwOffset;
		TRACE("checking %d, position %ld, event = %d\n",
			i, offset, event->hEventNotify);
		/* DSBPN_OFFSETSTOP has to be the last element. So this is */
		/* OK. [Inside DirectX, p274] */
		/*  */
		/* This also means we can't sort the entries by offset, */
		/* because DSBPN_OFFSETSTOP == -1 */
		if (offset == DSBPN_OFFSETSTOP) {
			if (dsb->state == STATE_STOPPED) {
				SetEvent(event->hEventNotify);
				TRACE("signalled event %d (%d)\n", event->hEventNotify, i);
				return;
			} else
				return;
		}
		if ((dsb->playpos + len) >= dsb->buflen) {
			if ((offset < ((dsb->playpos + len) % dsb->buflen)) ||
			    (offset >= dsb->playpos)) {
				TRACE("signalled event %d (%d)\n", event->hEventNotify, i);
				SetEvent(event->hEventNotify);
			}
		} else {
			if ((offset >= dsb->playpos) && (offset < (dsb->playpos + len))) {
				TRACE("signalled event %d (%d)\n", event->hEventNotify, i);
				SetEvent(event->hEventNotify);
			}
		}
	}
}

/* WAV format info can be found at: */
/* */
/*	http://www.cwi.nl/ftp/audio/AudioFormats.part2 */
/*	ftp://ftp.cwi.nl/pub/audio/RIFF-format */
/* */
/* Import points to remember: */
/* */
/*	8-bit WAV is unsigned */
/*	16-bit WAV is signed */

static inline INT16 cvtU8toS16(BYTE byte)
{
	return (byte - 128) << 8;
}

static inline BYTE cvtS16toU8(INT16 word)
{
	return (word + 32768) >> 8;
}


/* We should be able to optimize these two inline functions */
/* so that we aren't doing 8->16->8 conversions when it is */
/* not necessary. But this is still a WIP. Optimize later. */
static inline void get_fields(const IDirectSoundBufferImpl *dsb, const BYTE *buf, INT *fl, INT *fr)
{
	INT16	*bufs = (INT16 *) buf;

	/* TRACE("(%p)\n", buf); */
	if ((dsb->wfx.wBitsPerSample == 8) && dsb->wfx.nChannels == 2) {
		*fl = cvtU8toS16(*buf);
		*fr = cvtU8toS16(*(buf + 1));
		return;
	}

	if ((dsb->wfx.wBitsPerSample == 16) && dsb->wfx.nChannels == 2) {
		*fl = *bufs;
		*fr = *(bufs + 1);
		return;
	}

	if ((dsb->wfx.wBitsPerSample == 8) && dsb->wfx.nChannels == 1) {
		*fl = cvtU8toS16(*buf);
		*fr = *fl;
		return;
	}

	if ((dsb->wfx.wBitsPerSample == 16) && dsb->wfx.nChannels == 1) {
		*fl = *bufs;
		*fr = *bufs;
		return;
	}

	FIXME("get_fields found an unsupported configuration\n");
	*fl = 0;
	*fr = 0;
	return;
}

static inline void set_fields(BYTE *buf, INT fl, INT fr)
{
	INT16 *bufs = (INT16 *) buf;

	if ((dsound->wfx.wBitsPerSample == 8) && (dsound->wfx.nChannels == 2)) {
		*buf = cvtS16toU8(fl);
		*(buf + 1) = cvtS16toU8(fr);
		return;
	}

	if ((dsound->wfx.wBitsPerSample == 8) && (dsound->wfx.nChannels == 1)) {
		*buf = cvtS16toU8((fl + fr) >> 1);
		return;
	}

	if ((dsound->wfx.wBitsPerSample == 16) && (dsound->wfx.nChannels == 2)) {
		*bufs = fl;
		*(bufs + 1) = fr;
		return;
	}

	if ((dsound->wfx.wBitsPerSample == 16) && (dsound->wfx.nChannels == 1)) {
		*bufs = (fl + fr) >> 1;
		return;
	}
	FIXME("set_fields found an unsupported configuration\n");
	return;
}

static inline BOOL is_silent( const IDirectSoundBufferImpl *dsb )
{
  LONG lVol;

  if (dsb->dsbd.dwFlags & DSBCAPS_CTRL3D)
    lVol = dsb->ds3db->lVolume;
  else
    lVol = dsb->volpan.lVolume;

  return lVol == DSBVOLUME_MIN;
}

/* Now with PerfectPitch (tm) technology */
static INT DSOUND_MixerNorm(IDirectSoundBufferImpl *dsb, BYTE *buf, INT len)
{
	INT	i, size, ipos, ilen, fieldL, fieldR;
	BYTE	*ibp, *obp;
	INT	iAdvance = dsb->wfx.nBlockAlign;
	INT	oAdvance = dsb->dsound->wfx.nBlockAlign;

	ibp = dsb->swbuf->buffer + dsb->buf_mixpos;
	obp = buf;

	TRACE("(%p, %p, %p), buf_mixpos=%ld\n", dsb, ibp, obp, dsb->buf_mixpos);
	/* Check for the best case */
	if ((dsb->freq == dsb->dsound->wfx.nSamplesPerSec) &&
	    (dsb->wfx.wBitsPerSample == dsb->dsound->wfx.wBitsPerSample) &&
	    (dsb->wfx.nChannels == dsb->dsound->wfx.nChannels)) {
	        DWORD bytesleft = dsb->buflen - dsb->buf_mixpos;
		TRACE("(%p) Best case\n", dsb);
		if (len <= bytesleft ) {
			memcpy(obp, ibp, len);
		}
		else { /* wrap */
			memcpy(obp, ibp, bytesleft );
			memcpy(obp + bytesleft, dsb->swbuf->buffer, len - bytesleft);
		}
		return len;
	}

	/* Check for same sample rate */
	if (dsb->freq == dsb->dsound->wfx.nSamplesPerSec) {
		TRACE("(%p) Same sample rate %ld = primary %ld\n", dsb,
			dsb->freq, dsb->dsound->wfx.nSamplesPerSec);
		ilen = 0;
		for (i = 0; i < len; i += oAdvance) {
			get_fields(dsb, ibp, &fieldL, &fieldR);
			ibp += iAdvance;
			ilen += iAdvance;
			set_fields(obp, fieldL, fieldR);
			obp += oAdvance;
			if (ibp >= (BYTE *)(dsb->swbuf->buffer + dsb->buflen))
				ibp = dsb->swbuf->buffer;	/* wrap */
		}
		return (ilen);
	}

	/* Mix in different sample rates */
	/* */
	/* New PerfectPitch(tm) Technology (c) 1998 Rob Riggs */
	/* Patent Pending :-] */

	/* Patent enhancements (c) 2000 Ove Kåven,
	 * TransGaming Technologies Inc. */

	FIXME("(%p) Adjusting frequency: %ld -> %ld (need optimization)\n",
		dsb, dsb->freq, dsb->dsound->wfx.nSamplesPerSec);

	size = len / oAdvance;
	ilen = 0;
	ipos = dsb->buf_mixpos;
	for (i = 0; i < size; i++) {
		get_fields(dsb, (dsb->swbuf->buffer + ipos), &fieldL, &fieldR);
		set_fields(obp, fieldL, fieldR);
		obp += oAdvance;

		dsb->freqAcc += dsb->freqAdjust;
		if (dsb->freqAcc >= (1<<DSOUND_FREQSHIFT))
		{
			ULONG adv = (dsb->freqAcc>>DSOUND_FREQSHIFT) * iAdvance;

			dsb->freqAcc &= (1<<DSOUND_FREQSHIFT)-1;

			ipos += adv;
			ilen += adv;

   COMPUTE_WRAPAROUND( ipos, dsb->buflen );
		}
	}
	return ilen;
}

static void DSOUND_MixerVol(IDirectSoundBufferImpl *dsb, BYTE *buf, INT len)
{
	INT	i, inc = dsb->dsound->wfx.wBitsPerSample >> 3;
	BYTE	*bpc = buf;
	INT16	*bps = (INT16 *) buf;
	INT	val;

	TRACE("(%p) left = %lx, right = %lx\n", dsb,
		dsb->cvolpan.dwTotalLeftAmpFactor, dsb->cvolpan.dwTotalRightAmpFactor);
	if ((!(dsb->dsbd.dwFlags & DSBCAPS_CTRLPAN) || (dsb->cvolpan.lPan == 0)) &&
	    (!(dsb->dsbd.dwFlags & DSBCAPS_CTRLVOLUME) || (dsb->cvolpan.lVolume == 0)) &&
	    !(dsb->dsbd.dwFlags & DSBCAPS_CTRL3D))
		return;		/* Nothing to do */

 WINE_START_TIMER( "dsound_mix:mixervol" );

	/* If we end up with some bozo coder using panning or 3D sound */
	/* with a mono primary buffer, it could sound very weird using */
	/* this method. Oh well, tough patooties. */

	switch (inc)
	{

		case 1:
			for (i = 0; i < len; i += inc)
			{
				/* 8-bit WAV is unsigned, but we need to operate */
				/* on signed data for this to work properly */
				val = *bpc - 128;
				val = ((val * (i & inc ? dsb->cvolpan.dwTotalRightAmpFactor : dsb->cvolpan.dwTotalLeftAmpFactor)) >> 16);
				*bpc = val + 128;
				bpc++;
			}
			break;
		case 2:
		{
			/* 16-bit WAV is signed -- much better */
			INT val2;
			INT leftampfactor = dsb->cvolpan.dwTotalLeftAmpFactor;
			INT rightampfactor = dsb->cvolpan.dwTotalRightAmpFactor;
			for (i = 0; i < len; i += (inc<<1))
			{
				val = (bps[0] * leftampfactor) >> 16;
				val2 = (bps[1] * rightampfactor) >> 16;
				bps[0] = val;
				bps[1] = val2;

				bps+=2;
			}
			break;
		}
		default:
			/* Very ugly! */
			FIXME("MixerVol had a nasty error\n");
	}

 WINE_START_TIMER( "dsound_mix:mixervol" );

}

static void *tmp_buffer;
static size_t tmp_buffer_len = 0;

static void *DSOUND_tmpbuffer(size_t len)
{
  if (len>tmp_buffer_len) {
    void *new_buffer = realloc(tmp_buffer, len);
    if (new_buffer) {
      tmp_buffer = new_buffer;
      tmp_buffer_len = len;
    }
    return new_buffer;
  }
  return tmp_buffer;
}

static DWORD DSOUND_MixInBuffer_DifferentSampleRate16(IDirectSoundBufferImpl *dsb, DWORD writepos, DWORD len)
{
	INT16	*obufs;
	const INT16* ibufs;

	UINT16 ampfactors[2] = { 0xffff, 0xffff };
#define leftampfactor ampfactors[0]
#define rightampfactor ampfactors[1]

	INT iAdvance = dsb->wfx.nBlockAlign;
	INT	i, size, ilen, temp, field, field2;
	INT	fieldL, fieldR;

	const INT16* endibufs, *endobuf;
	
	/* The increments will be used for smoothing between previous and curent amp factors.
		The maxsmooth represents the maximum number of sound samples that will be smoothed.
		If the sound to be mixed has less samples, the smoothing will be sharper. */
	int leftIncrement, leftFinalPlayedAmp, leftPreviousPlayedAmp;
	int rightIncrement, rightFinalPlayedAmp, rightPreviousPlayedAmp;
	int maxsmooth = 500;

	TRACE("MixInBuffer (%p) len = %ld, dest = %ld, bps = %d\n", dsb, len, writepos, dsb->dsound->wfx.wBitsPerSample);

	if ((dsb->dsbd.dwFlags & DSBCAPS_CTRLPAN) ||
	    (dsb->dsbd.dwFlags & DSBCAPS_CTRLVOLUME))
	{
		leftampfactor = dsb->cvolpan.dwTotalLeftAmpFactor;
		rightampfactor = dsb->cvolpan.dwTotalRightAmpFactor;
	}

	obufs = (INT16 *) (dsb->dsound->buffer + writepos);
	endobuf = (INT16 *) (dsb->dsound->buffer + dsb->dsound->buflen);

	ibufs = (INT16*)(((BYTE*)(dsb->swbuf->buffer)) + dsb->buf_mixpos);
	endibufs = (INT16*)(((BYTE*)(dsb->swbuf->buffer)) + dsb->buflen);

	ilen = 0;

	size = len / 4;
	temp = (endobuf - obufs) / 2;

	if( is_silent( dsb ) )
	{
		dsb->freqAcc += (dsb->freqAdjust * size);
		ilen = (dsb->freqAcc>>DSOUND_FREQSHIFT) * iAdvance;
		dsb->freqAcc &= (1<<DSOUND_FREQSHIFT)-1;
		goto skip_mix;
	}
	
	/* When the previous amp factor is -1, we simply do not smooth by setting previous == newamp factor */
	leftPreviousPlayedAmp = dsb->leftPreviousAmpFactor;
	if (leftPreviousPlayedAmp == -1)
		leftPreviousPlayedAmp = leftampfactor;
	leftFinalPlayedAmp = leftampfactor;
	rightPreviousPlayedAmp = dsb->rightPreviousAmpFactor;
	if (rightPreviousPlayedAmp == -1)
		rightPreviousPlayedAmp = rightampfactor;
	rightFinalPlayedAmp = rightampfactor;
	
	if (maxsmooth > temp)
		maxsmooth = temp;
	if (maxsmooth < 1)
		maxsmooth = 1;

	/* we precalculate the increment as an integer value. Since integer division is rough, a final adjustment
	    up to the value of temp-1 (the divider above) might hapen after the first loop, but that is
		less of an issue than the previous non-smoothed amplification changes */
	leftIncrement = (leftFinalPlayedAmp - leftPreviousPlayedAmp) / maxsmooth;
	rightIncrement = (rightFinalPlayedAmp - rightPreviousPlayedAmp) / maxsmooth;
	leftampfactor = leftPreviousPlayedAmp;
	rightampfactor = rightPreviousPlayedAmp;
	/* we need to save the current amplification for the next mixing runs */
	dsb->leftPreviousAmpFactor = leftFinalPlayedAmp;
	dsb->rightPreviousAmpFactor = rightFinalPlayedAmp;


	WINE_START_TIMER("dsound_mix:dsr16");

wrapmix:
	if (temp > size) temp = size;

#if defined( __i386__ )

	if( bMmxExtensionsAvailable && (dsb->wfx.wBitsPerSample == 16) )
	{
	    if (dsb->wfx.nChannels == 2)
	    {
		for (i = 0; i < temp; i++)
		{
			if (i < maxsmooth)
			{
				/* increment left and right for the first maxsmooth samples */
				leftampfactor += leftIncrement;
				rightampfactor += rightIncrement;
			}

			/* Assuming u0.16 fixedpoint numbers. NOTE that we temporarily LOSE a bit of precision
   			 * as we use signed by signed mmx multiplication and we need to turn into s0.15 for the
			 * multiplication. Other than that it's the same algo as the non mmx case except we operate
			 * on both the left and right channels at the same time. */
			asm volatile(
			  "movd (%2), %%mm6     \t\n" /* mm6: x|x|rightamp|leftamp */
			  "movd (%0), %%mm4     \t\n" /* mm4: x|x|fieldR|fieldL */

			  "psrlw $1, %%mm6      \t\n" /* mm6: Convert u0.16 ampfactors to s0.15 */

			  "movd (%1), %%mm5     \t\n" /* mm5: x|x|obufsR|obufsL */

			  "pmulhw %%mm6, %%mm4  \t\n" /* mm4: x|x|ampfieldR|ampfieldL */

			  "psllw $1, %%mm4      \t\n" /* mm4: Fix precision loss. */

			  "paddsw %%mm5, %%mm4  \t\n" /* mm4 + mm5 with saturation to 16bits */ 
			  "movd %%mm4, (%1)     \t\n" /* mm4: -> obufs */

			  : /* No outputs */
			  : "r"(ibufs), "r"(obufs), "r"(ampfactors) /* Inputs */
#if defined( __MMX__ )
			  : "%mm4", "%mm5", "%mm6" /* clobbers */
#endif
           );

			obufs += 2;

			dsb->freqAcc += dsb->freqAdjust;
			if (dsb->freqAcc >= (1<<DSOUND_FREQSHIFT)) {
				ULONG adv = (dsb->freqAcc >> DSOUND_FREQSHIFT) * iAdvance;
				dsb->freqAcc &= (1<<DSOUND_FREQSHIFT)-1;
				ilen += adv;
				ibufs = (INT16*)(((BYTE*)ibufs) + adv);
				if( ibufs >= endibufs ) ibufs = (INT16*)(((BYTE*)ibufs) - dsb->buflen);
			}
		}
	    } else {
		//DWORD replicated;
		for (i = 0; i < temp; i++)
		{
			if (i < maxsmooth)
			{
				/* increment left and right for the first maxsmooth samples */
				leftampfactor += leftIncrement;
				rightampfactor += rightIncrement;
			}

			//replicated = (*ibufs << 16) | *ibufs;

			/* Assuming u0.16 fixedpoint numbers. NOTE that we temporarily LOSE a bit of precision
   			 * as we use signed by signed mmx multiplication and we need to turn into s0.15 for the
			 * multiplication. Other than that it's the same algo as the non mmx case except we operate
			 * on both the left and right channels at the same time. */
			asm volatile(
			  "movd %3, %%mm7       \t\n" /* mm7: x|x|0|FFFF */
			  "movd (%0), %%mm5     \t\n" /* mm5: x|x|garbage|mono */
			  "movd (%0), %%mm4     \t\n" /* mm4: x|x|garbage|mono */
			  "pand %%mm7, %%mm5    \t\n" /* mm5: x|x|0|mono */
			  "psllq $0x10, %%mm4   \t\n" /* mm4: x|garbage|mono|0 */
			  "movd (%2), %%mm6     \t\n" /* mm6: x|x|rightamp|leftamp */
			  "por  %%mm5, %%mm4    \t\n" /* mm4: x|garbage|mono|mono */

			  "psrlw $1, %%mm6      \t\n" /* mm6: Convert u0.16 ampfactors to s0.15 */

			  "movd (%1), %%mm5     \t\n" /* mm5: x|x|obufsR|obufsL */

			  "pmulhw %%mm6, %%mm4  \t\n" /* mm4: x|x|ampfieldR|ampfieldL */

			  "psllw $1, %%mm4      \t\n" /* mm4: Fix precision loss. */

			  "paddsw %%mm5, %%mm4  \t\n" /* mm4 + mm5 with saturation to 16bits */ 
			  "movd %%mm4, (%1)     \t\n" /* mm4: -> obufs */

			  : /* No outputs */
			  : "r"(ibufs), "r"(obufs), "r"(ampfactors), "r"(0xFFFF) /* Inputs */
#if defined( __MMX__ )
			  : "%mm4", "%mm5", "%mm6", "%mm7" /* clobbers */
#endif
           );

			obufs += 2;

			dsb->freqAcc += dsb->freqAdjust;
			if (dsb->freqAcc >= (1<<DSOUND_FREQSHIFT)) {
				ULONG adv = (dsb->freqAcc >> DSOUND_FREQSHIFT) * iAdvance;
				dsb->freqAcc &= (1<<DSOUND_FREQSHIFT)-1;
				ilen += adv;
				ibufs = (INT16*)(((BYTE*)ibufs) + adv);
				if( ibufs >= endibufs ) ibufs = (INT16*)(((BYTE*)ibufs) - dsb->buflen);
			}
		}
	    }
	}
	else
 
#endif /* __i386__ */

	for (i = 0; i < temp; i++)
	{
		if (i < maxsmooth)
		{
			/* increment left and right for the first maxsmooth samples */
			leftampfactor += leftIncrement;
			rightampfactor += rightIncrement;
		}
	
		get_fields(dsb, (BYTE*)ibufs, &fieldL, &fieldR);

		field = ((fieldL * leftampfactor) >> 16) + obufs[0];
		field2 = ((fieldR * rightampfactor) >> 16) + obufs[1];

		field = CLAMP(field, -32768, 32767);
		field2 = CLAMP(field2, -32768, 32767);

		obufs[0] = field;
		obufs[1] = field2;

		obufs += 2;

		dsb->freqAcc += dsb->freqAdjust;
		if (dsb->freqAcc >= (1<<DSOUND_FREQSHIFT)) {
			ULONG adv = (dsb->freqAcc>>DSOUND_FREQSHIFT) * iAdvance;
			dsb->freqAcc &= (1<<DSOUND_FREQSHIFT)-1;
			ilen += adv;
			ibufs = (INT16*)(((BYTE*)ibufs) + adv);
			if( ibufs >= endibufs ) ibufs = (INT16*)(((BYTE*)ibufs) - dsb->buflen);
		}

	}

	/* further loops managed through the goto below will be at the final amp factors and not be incremented */
	leftampfactor = leftFinalPlayedAmp;
	rightampfactor = rightFinalPlayedAmp;
	leftIncrement = 0;
	rightIncrement = 0;
	
	if (obufs >= endobuf)
		obufs = (INT16*)dsb->dsound->buffer;
	size -= temp;
	temp = (endobuf - obufs) / 2;
	if (size) goto wrapmix;

#if defined( __i386__ )
	if( bMmxExtensionsAvailable && (dsb->wfx.wBitsPerSample == 16) )
	{
	   asm volatile("emms");
	}
#endif /* __i386__ */

	WINE_STOP_TIMER("dsound_mix:dsr16");

skip_mix:

	if (dsb->leadin && (dsb->startpos > dsb->buf_mixpos) && (dsb->startpos <= dsb->buf_mixpos + ilen)) {
		/* HACK... leadin should be reset when the PLAY position reaches the startpos,
		 * not the MIX position... but if the sound buffer is bigger than our prebuffering
		 * (which must be the case for the streaming buffers that need this hack anyway)
		 * plus DS_HEL_MARGIN or equivalent, then this ought to work anyway. */
		dsb->leadin = FALSE;
	}

	dsb->buf_mixpos += ilen;

	if (dsb->buf_mixpos >= dsb->buflen) {
		if (dsb->playflags & DSBPLAY_LOOPING) {
			/* wrap */
			COMPUTE_WRAPAROUND( dsb->buf_mixpos, dsb->buflen );
			if (dsb->leadin && (dsb->startpos <= dsb->buf_mixpos))
				dsb->leadin = FALSE; /* HACK: see above */
		}
	}

	return len;
#undef leftampfactor
#undef rightampfactor

}

static DWORD DSOUND_MixInBuffer_DifferentBPS16(IDirectSoundBufferImpl *dsb, DWORD writepos, DWORD len)
{
	const BYTE	*ibp;
	INT16	*obufs;

	UINT16 ampfactors[2] = { 0xffff, 0xffff };
#define leftampfactor ampfactors[0]
#define rightampfactor ampfactors[1]

	INT iAdvance = dsb->wfx.nBlockAlign;

	INT	i, size, ilen, temp, field, field2;
	INT	fieldL, fieldR;

	const BYTE* endibuf;
	INT16* endobuf;

	INT ulen;
	/* The increments will be used for smoothing between previous and curent amp factors.
		The maxsmooth represents the maximum number of sound samples that will be smoothed.
		If the sound to be mixed has less samples, the smoothing will be sharper. */
	int leftIncrement, leftFinalPlayedAmp, leftPreviousPlayedAmp;
	int rightIncrement, rightFinalPlayedAmp, rightPreviousPlayedAmp;
	int maxsmooth = 500;
	
	TRACE("MixInBuffer (%p) len = %ld, dest = %ld, bps = %d\n", dsb, len, writepos, dsb->dsound->wfx.wBitsPerSample);

	if ((dsb->dsbd.dwFlags & DSBCAPS_CTRLPAN) ||
	    (dsb->dsbd.dwFlags & DSBCAPS_CTRLVOLUME))
	{
		leftampfactor = dsb->cvolpan.dwTotalLeftAmpFactor;
		rightampfactor = dsb->cvolpan.dwTotalRightAmpFactor;
	}

	ibp = dsb->swbuf->buffer + dsb->buf_mixpos;
	endibuf = dsb->swbuf->buffer + dsb->buflen;

	obufs = (INT16 *) (dsb->dsound->buffer + writepos);

	ilen = 0;

	size = len / 4;
	endobuf = (INT16 *) (dsb->dsound->buffer + dsb->dsound->buflen);
	temp = (endobuf - obufs) / 2;

	if( is_silent( dsb ) )
	{
		ilen = size * iAdvance;
		goto skip_mix;
	}
		
	/* When the previous amp factor is -1, we simply do not smooth by setting previous == newamp factor */
	leftPreviousPlayedAmp = dsb->leftPreviousAmpFactor;
	if (leftPreviousPlayedAmp == -1)
		leftPreviousPlayedAmp = leftampfactor;
	leftFinalPlayedAmp = leftampfactor;
	rightPreviousPlayedAmp = dsb->rightPreviousAmpFactor;
	if (rightPreviousPlayedAmp == -1)
		rightPreviousPlayedAmp = rightampfactor;
	rightFinalPlayedAmp = rightampfactor;
	
	if (maxsmooth > temp)
		maxsmooth = temp;
	if (maxsmooth < 1)
		maxsmooth = 1;
	/* we precalculate the increment as an integer value. Since integer division is rough, a final adjustment
	    up to the value of temp-1 (the divider above) might hapen after the first loop, but that is
		less of an issue than the previous non-smoothed amplification changes */
	leftIncrement = (leftFinalPlayedAmp - leftPreviousPlayedAmp) / maxsmooth;
	rightIncrement = (rightFinalPlayedAmp - rightPreviousPlayedAmp) / maxsmooth;
	leftampfactor = leftPreviousPlayedAmp;
	rightampfactor = rightPreviousPlayedAmp;
	/* we need to save the current amplification for the next mixing runs */
	dsb->leftPreviousAmpFactor = leftFinalPlayedAmp;
	dsb->rightPreviousAmpFactor = rightFinalPlayedAmp;
		
	WINE_START_TIMER("dsound_mix:dbps16");

wrapmix:
	ulen = (endibuf - ibp) / iAdvance;
	if (temp > ulen) temp = ulen;
	if (temp > size) temp = size;

#if defined( __i386__ )

	if( bMmxExtensionsAvailable && (dsb->wfx.wBitsPerSample == 16) )
	{
	    if (dsb->wfx.nChannels == 2)
	    {
		for (i = 0; i < temp; i++)
		{
			if (i < maxsmooth)
			{
				/* increment left and right for the first maxsmooth samples */
				leftampfactor += leftIncrement;
				rightampfactor += rightIncrement;
			}

			/* Assuming u0.16 fixedpoint numbers. NOTE that we temporarily LOSE a bit of precision
			 * as we use signed by signed mmx multiplication and we need to turn into s0.15 for the
			 * multiplication. Other than that it's the same algo as the non mmx case except we operate
			 * on both the left and right channels at the same time. */
			asm volatile(
			  "movd (%2), %%mm6     \t\n" /* mm6: x|x|rightamp|leftamp */
			  "movd (%0), %%mm4     \t\n" /* mm4: x|x|fieldR|fieldL */

			  "psrlw $1, %%mm6      \t\n" /* mm6: Convert u0.16 ampfactors to s0.15 */

			  "movd (%1), %%mm5     \t\n" /* mm5: x|x|obufsR|obufsL */

			  "pmulhw %%mm6, %%mm4  \t\n" /* mm4: x|x|ampfieldR|ampfieldL */

			  "psllw $1, %%mm4      \t\n" /* mm4: Fix precision loss. */

			  "paddsw %%mm5, %%mm4  \t\n" /* mm4 + mm5 with saturation to 16bits */ 
			  "movd %%mm4, (%1)     \t\n" /* mm4: -> obufs */

			  : /* No outputs */
			  : "r"(ibp), "r"(obufs), "r"(ampfactors) /* Inputs */
#if defined( __MMX__ )
			  : "%mm4", "%mm5", "%mm6" /* clobbers */
#endif
           );

			obufs += 2;
			ibp += iAdvance;
		}
	    } else {
		//DWORD replicated;
		for (i = 0; i < temp; i++)
		{
			if (i < maxsmooth)
			{
				/* increment left and right for the first maxsmooth samples */
				leftampfactor += leftIncrement;
				rightampfactor += rightIncrement;
			}

			//replicated = (*ibp << 16) | *ibp;

			/* Assuming u0.16 fixedpoint numbers. NOTE that we temporarily LOSE a bit of precision
   			 * as we use signed by signed mmx multiplication and we need to turn into s0.15 for the
			 * multiplication. Other than that it's the same algo as the non mmx case except we operate
			 * on both the left and right channels at the same time. */
			asm volatile(
			  "movd %3, %%mm7       \t\n" /* mm7: x|x|0|FFFF */
			  "movd (%0), %%mm5     \t\n" /* mm5: x|x|garbage|mono */
			  "movd (%0), %%mm4     \t\n" /* mm4: x|x|garbage|mono */
			  "pand %%mm7, %%mm5    \t\n" /* mm5: x|x|0|mono */
			  "psllq $0x10, %%mm4   \t\n" /* mm4: x|garbage|mono|0 */
			  "movd (%2), %%mm6     \t\n" /* mm6: x|x|rightamp|leftamp */
			  "por  %%mm5, %%mm4    \t\n" /* mm4: x|garbage|mono|mono */

			  "psrlw $1, %%mm6      \t\n" /* mm6: Convert u0.16 ampfactors to s0.15 */

			  "movd (%1), %%mm5     \t\n" /* mm5: x|x|obufsR|obufsL */

			  "pmulhw %%mm6, %%mm4  \t\n" /* mm4: x|x|ampfieldR|ampfieldL */

			  "psllw $1, %%mm4      \t\n" /* mm4: Fix precision loss. */

			  "paddsw %%mm5, %%mm4  \t\n" /* mm4 + mm5 with saturation to 16bits */ 
			  "movd %%mm4, (%1)     \t\n" /* mm4: -> obufs */

			  : /* No outputs */
			  : "r"(ibp), "r"(obufs), "r"(ampfactors), "r"(0xFFFF) /* Inputs */
#if defined( __MMX__ )
			  : "%mm4", "%mm5", "%mm6", "%mm7" /* clobbers */
#endif
           );

			obufs += 2;
			ibp += iAdvance;
		}
	    }
	}
	else
 
#endif

	for (i = 0; i < temp; i++)
	{
		if (i < maxsmooth)
		{
			/* increment left and right for the first maxsmooth samples */
			leftampfactor += leftIncrement;
			rightampfactor += rightIncrement;
		}
		get_fields(dsb, ibp, &fieldL, &fieldR);

		field = ((fieldL * leftampfactor) >> 16) + obufs[0];
		field2 = ((fieldR * rightampfactor) >> 16) + obufs[1];

		field = CLAMP(field, -32768, 32767);
		field2 = CLAMP(field2, -32768, 32767);

		obufs[0] = field;
		obufs[1] = field2;

		obufs += 2;
		ibp += iAdvance;
	}

	ilen += (temp * iAdvance);

	/* further loops managed through the goto below will be at the final amp factors and not be incremented */
	leftampfactor = leftFinalPlayedAmp;
	rightampfactor = rightFinalPlayedAmp;
	leftIncrement = 0;
	rightIncrement = 0;
	if (ibp >= endibuf)
		ibp = dsb->swbuf->buffer;	/* wrap */
	if (obufs >= endobuf)
		obufs = (INT16*)dsb->dsound->buffer;
	size -= temp;
	temp = (endobuf - obufs) / 2;
	if (size) goto wrapmix;

#if defined( __i386__ )
	if( bMmxExtensionsAvailable && (dsb->wfx.wBitsPerSample == 16) )
	{
	   asm volatile("emms");
	}
#endif /* __i386__ */

	WINE_STOP_TIMER("dsound_mix:dbps16");

skip_mix:

	if (dsb->leadin && (dsb->startpos > dsb->buf_mixpos) && (dsb->startpos <= dsb->buf_mixpos + ilen)) {
		/* HACK... leadin should be reset when the PLAY position reaches the startpos,
		 * not the MIX position... but if the sound buffer is bigger than our prebuffering
		 * (which must be the case for the streaming buffers that need this hack anyway)
		 * plus DS_HEL_MARGIN or equivalent, then this ought to work anyway. */
		dsb->leadin = FALSE;
	}

	dsb->buf_mixpos += ilen;

	if (dsb->buf_mixpos >= dsb->buflen) {
		if (dsb->playflags & DSBPLAY_LOOPING) {
			/* wrap */
			COMPUTE_WRAPAROUND( dsb->buf_mixpos, dsb->buflen );
			if (dsb->leadin && (dsb->startpos <= dsb->buf_mixpos))
				dsb->leadin = FALSE; /* HACK: see above */
		}
	}

	return len;

#undef leftampfactor
#undef rightampfactor

}


static DWORD DSOUND_MixInBuffer(IDirectSoundBufferImpl *dsb, DWORD writepos, DWORD fraglen)
{
	INT	i, len, size, ilen, temp, field, field2;
	INT	advance = dsb->dsound->wfx.wBitsPerSample >> 3;
	BYTE	*buf, *ibuf, *obuf;
	INT16	*ibufs, *obufs;
	LPVOID	endbuf;

	len = fraglen;
	if (!(dsb->playflags & DSBPLAY_LOOPING)) {
		temp = MulDiv(dsb->dsound->wfx.nAvgBytesPerSec, dsb->buflen,
			dsb->nAvgBytesPerSec) -
		       MulDiv(dsb->dsound->wfx.nAvgBytesPerSec, dsb->buf_mixpos,
			dsb->nAvgBytesPerSec);
		len = (len > temp) ? temp : len;
	}
	len &= ~3;				/* 4 byte alignment */

	if (len <= 0) {
		/* This should only happen if we aren't looping and temp < 4 */
		return 0;
	}

#ifndef USE_SLOW_16BIT_MIXING
	/* use optimized mixing for certain paths */
	if ((dsb->dsound->wfx.wBitsPerSample == 16) && (dsb->dsound->wfx.nChannels == 2))
	{
		if (dsb->freq == dsb->dsound->wfx.nSamplesPerSec)
			return DSOUND_MixInBuffer_DifferentBPS16(dsb,  writepos,  len);
		else
			return DSOUND_MixInBuffer_DifferentSampleRate16(dsb,  writepos,  len);
	}
#endif

	/* Been seeing segfaults in malloc() for some reason... */
	TRACE("allocating buffer (size = %d)\n", len);
	if ((buf = ibuf = (BYTE *) DSOUND_tmpbuffer(len)) == NULL)
		return 0;

	TRACE("MixInBuffer (%p) len = %d, dest = %ld, bps = %d\n", dsb, len, writepos, dsb->dsound->wfx.wBitsPerSample);

	ilen = DSOUND_MixerNorm(dsb, ibuf, len);
	if ((dsb->dsbd.dwFlags & DSBCAPS_CTRLPAN) ||
	    (dsb->dsbd.dwFlags & DSBCAPS_CTRLVOLUME))
		DSOUND_MixerVol(dsb, ibuf, len);

	obuf = dsb->dsound->buffer + writepos;

	obufs = (INT16 *) obuf;
	ibufs = (INT16 *) ibuf;

	size = len / advance;
	endbuf = dsb->dsound->buffer + dsb->dsound->buflen;
	temp = (dsb->dsound->buflen - writepos) / advance;
wrapmix:
	if (temp > size) temp = size;

	if (dsb->dsound->wfx.wBitsPerSample == 8)
	{
		INT ulen = temp/2;
		for (i = 0; i < ulen; i++)
		{
			/* 8-bit WAV is unsigned */
			field = (ibuf[0] - 128) + (obuf[0] - 128);
			field2 = (ibuf[1] - 128) + (obuf[1] - 128);

			field = CLAMP(field, -128, 127);
			field2 = CLAMP(field2, -128, 127);

			obuf[0] = field + 128;
			obuf[1] = field2 + 128;

			ibuf += 2;
			obuf += 2;
		}
		if (temp&1)
		{
			field = (ibuf[0] - 128) + (obuf[0] - 128);
			field = CLAMP(field, -128, 127);
			obuf[0] = field;
		}
		if (obuf >= (BYTE *)endbuf)
			obuf = dsb->dsound->buffer;
	}
	else
	{
		INT ulen = temp/2;
		for (i = 0; i < ulen; i++)
		{
			/* 16-bit WAV is signed */
			field = ibufs[0] + obufs[0];
			field2 = ibufs[1] + obufs[1];

			field = CLAMP(field, -32768, 32767);
			field2 = CLAMP(field2, -32768, 32767);

			obufs[0] = field;
			obufs[1] = field2;

			obufs += 2;
			ibufs += 2;
		}
		if (temp&1)
		{
			field = ibufs[0] + obufs[0];
			field = CLAMP(field, -32768, 32767);
			obufs[0] = field;
			obufs += 1;
			ibufs += 1;
		}
		if (obufs >= (INT16 *)endbuf)
			obufs = (INT16*)dsb->dsound->buffer;
	}
	size -= temp;
	temp = dsb->dsound->buflen / advance;
	if (size) goto wrapmix;

	/* free(buf); */

	if (dsb->leadin && (dsb->startpos > dsb->buf_mixpos) && (dsb->startpos <= dsb->buf_mixpos + ilen)) {
		/* HACK... leadin should be reset when the PLAY position reaches the startpos,
		 * not the MIX position... but if the sound buffer is bigger than our prebuffering
		 * (which must be the case for the streaming buffers that need this hack anyway)
		 * plus DS_HEL_MARGIN or equivalent, then this ought to work anyway. */
		dsb->leadin = FALSE;
	}

	dsb->buf_mixpos += ilen;

	if (dsb->buf_mixpos >= dsb->buflen) {
		if (dsb->playflags & DSBPLAY_LOOPING) {
			/* wrap */
			while (dsb->buf_mixpos >= dsb->buflen)
				dsb->buf_mixpos -= dsb->buflen;
			if (dsb->leadin && (dsb->startpos <= dsb->buf_mixpos))
				dsb->leadin = FALSE; /* HACK: see above */
		}
	}

	return len;
}

static void DSOUND_PhaseCancel(IDirectSoundBufferImpl *dsb, DWORD writepos, DWORD len)
{
	INT     i, ilen, field;
	INT     advance = dsb->dsound->wfx.wBitsPerSample >> 3;
	BYTE	*buf, *ibuf, *obuf;
	INT16	*ibufs, *obufs;

	len &= ~3;				/* 4 byte alignment */

	TRACE("allocating buffer (size = %ld)\n", len);
	if ((buf = ibuf = (BYTE *) DSOUND_tmpbuffer(len)) == NULL)
		return;

	TRACE("PhaseCancel (%p) len = %ld, dest = %ld\n", dsb, len, writepos);

	/* *********** */
	EnterCriticalSection(&dsb->dsound->mixlock);
	ilen = DSOUND_MixerNorm(dsb, ibuf, len);
	if ((dsb->dsbd.dwFlags & DSBCAPS_CTRLPAN) ||
	    (dsb->dsbd.dwFlags & DSBCAPS_CTRLVOLUME))
		DSOUND_MixerVol(dsb, ibuf, len);

	/* subtract instead of add, to phase out premixed data */
	obuf = dsb->dsound->buffer + writepos;
	for (i = 0; i < len; i += advance) {
		obufs = (INT16 *) obuf;
		ibufs = (INT16 *) ibuf;
		if (dsb->dsound->wfx.wBitsPerSample == 8) {
			/* 8-bit WAV is unsigned */
			field = (*obuf - 128);
			field -= (*ibuf - 128);
			field = field > 127 ? 127 : field;
			field = field < -128 ? -128 : field;
			*obuf = field + 128;
		} else {
			/* 16-bit WAV is signed */
			field = *obufs;
			field -= *ibufs;
			field = field > 32767 ? 32767 : field;
			field = field < -32768 ? -32768 : field;
			*obufs = field;
		}
		ibuf += advance;
		obuf += advance;
		if (obuf >= (BYTE *)(dsb->dsound->buffer + dsb->dsound->buflen))
			obuf = dsb->dsound->buffer;
	}
	/* free(buf); */
	LeaveCriticalSection(&dsb->dsound->mixlock);
	/* *********** */
}

static void DSOUND_MixCancel(IDirectSoundBufferImpl *dsb, DWORD writepos, BOOL cancel)
{
	DWORD   len, rem;
	/* determine buffer position to cancel from */
	DWORD buf_writepos = DSOUND_CalcPlayPosition(dsb, dsb->state & dsb->dsound->state, writepos,
						     writepos, dsb->primary_mixpos, dsb->buf_mixpos,
						     dsb->freqAcc, &rem);

	TRACE("(%p, %ld), buf_mixpos=%ld\n", dsb, writepos, dsb->buf_mixpos);

	while (dsb->freqAcc < rem) {
		if (buf_writepos < dsb->wfx.nBlockAlign)
			buf_writepos += dsb->buflen;
		buf_writepos -= dsb->wfx.nBlockAlign;
		dsb->freqAcc += 1<<DSOUND_FREQSHIFT;
	}
	dsb->freqAcc -= rem;

	/* calculate length for PhaseCancel */
	len = ((dsb->buf_mixpos < buf_writepos) ? dsb->buflen : 0) +
		dsb->buf_mixpos - buf_writepos;

	dsb->buf_mixpos = buf_writepos;

	/* assuming here that the sound didn't start "after" writepos
	 * (may matter for PhaseCancel), but that shouldn't happen */
	dsb->primary_mixpos = writepos;

	TRACE("new buf_mixpos=%ld, primary_mixpos=%ld (len=%ld)\n",
	      dsb->buf_mixpos, dsb->primary_mixpos, len);

	if (cancel) DSOUND_PhaseCancel(dsb, writepos, len);
}

void DSOUND_MixCancelAt(IDirectSoundBufferImpl *dsb, DWORD buf_writepos)
{
#if 0
	DWORD   i, size, flen, len, npos, nlen;
	INT	iAdvance = dsb->wfx.nBlockAlign;
	INT	oAdvance = dsb->dsound->wfx.nBlockAlign;
	/* determine amount of premixed data to cancel */
	DWORD buf_done =
		((dsb->buf_mixpos < buf_writepos) ? dsb->buflen : 0) +
		dsb->buf_mixpos - buf_writepos;
	DWORD writepos;
#endif

	WARN("(%p, %ld), buf_mixpos=%ld\n", dsb, buf_writepos, dsb->buf_mixpos);
	/* since this is not implemented yet, just cancel *ALL* prebuffering for now
	 * (which is faster anyway when there's only a single secondary buffer) */
#if 1
	dsb->dsound->need_remix = TRUE;
#else
	IDirectSoundBufferImpl_GetCurrentPosition(dsb->dsound, NULL, &writepos);
	MixCancel(dsb, writepos, TRUE);
#endif
}

void DSOUND_ForceRemix(IDirectSoundBufferImpl *dsb)
{
	EnterCriticalSection(&dsb->lock);
	if (dsb->state == STATE_PLAYING) {
#if 0 /* this may not be quite reliable yet */
		dsb->need_remix = TRUE;
#else
		dsb->dsound->need_remix = TRUE;
#endif
	}
	LeaveCriticalSection(&dsb->lock);
}

static DWORD DSOUND_MixOne(IDirectSoundBufferImpl *dsb, DWORD playpos, DWORD writepos, DWORD mixlen)
{
	DWORD len, slen, rem;
	/* determine this buffer's write position */
	DWORD buf_writepos = DSOUND_CalcPlayPosition(dsb, dsb->state & dsb->dsound->state, writepos,
						     writepos, dsb->primary_mixpos, dsb->buf_mixpos,
						     dsb->freqAcc, &rem);
	/* determine how much already-mixed data exists */
	DWORD buf_done =
		((dsb->buf_mixpos < buf_writepos) ? dsb->buflen : 0) +
		dsb->buf_mixpos - buf_writepos;
	DWORD primary_done =
		((dsb->primary_mixpos < writepos) ? dsb->dsound->buflen : 0) +
		dsb->primary_mixpos - writepos;
	DWORD adv_done =
		((dsb->dsound->mixpos < writepos) ? dsb->dsound->buflen : 0) +
		dsb->dsound->mixpos - writepos;
	DWORD played =
		((buf_writepos < dsb->playpos) ? dsb->buflen : 0) +
		buf_writepos - dsb->playpos;
	DWORD buf_left = dsb->buflen - buf_writepos;
	int still_behind;

	TRACE("buf_writepos=%ld, primary_writepos=%ld\n", buf_writepos, writepos);
	TRACE("buf_done=%ld, primary_done=%ld\n", buf_done, primary_done);
	TRACE("buf_mixpos=%ld (acc=%ld,+%ld), primary_mixpos=%ld, mixlen=%ld\n", dsb->buf_mixpos,
	      dsb->freqAcc, dsb->freqAdjust, dsb->primary_mixpos, mixlen);
	TRACE("looping=%ld, startpos=%ld, leadin=%ld\n", dsb->playflags, dsb->startpos, dsb->leadin);

	/* check for notification positions */
	if (dsb->dsbd.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY &&
	    dsb->state != STATE_STARTING) {
		DSOUND_CheckEvent(dsb, played);
	}

#ifdef DUMP_SOUND_DATA
	/* dump the sound data that's just been successfully played */
	dump_sound_data(dsb, dsb->playpos, played);
#endif

	/* save write position for non-GETCURRENTPOSITION2... */
	dsb->playpos = buf_writepos;

	/* check whether CalcPlayPosition detected a mixing underrun */
	if ((buf_done == 0) && (dsb->primary_mixpos != writepos)) {
		/* it did, but did we have more to play? */
		if ((dsb->playflags & DSBPLAY_LOOPING) ||
		    (dsb->buf_mixpos < dsb->buflen)) {
			/* yes, have to recover */
			WARN("underrun on sound buffer %p\n", dsb);
			TRACE("recovering from underrun: primary_mixpos=%ld\n", writepos);
		}
		dsb->primary_mixpos = writepos;
		primary_done = 0;
	}
	/* determine how far ahead we should mix */
	if (((dsb->playflags & DSBPLAY_LOOPING) ||
	     (dsb->leadin && (dsb->probably_valid_to != 0))) &&
	    !(dsb->dsbd.dwFlags & DSBCAPS_STATIC)) {
		/* if this is a streaming buffer, it typically means that
		 * we should defer mixing past probably_valid_to as long
		 * as we can, to avoid unnecessary remixing */
		/* the heavy-looking calculations shouldn't be that bad,
		 * as any game isn't likely to be have more than 1 or 2
		 * streaming buffers in use at any time anyway... */
		DWORD probably_valid_left =
			(dsb->probably_valid_to == (DWORD)-1) ? dsb->buflen :
			((dsb->probably_valid_to < buf_writepos) ? dsb->buflen : 0) +
			dsb->probably_valid_to - buf_writepos;
		DWORD acc = dsb->freqAcc;
		/* check for leadin condition */
		if ((probably_valid_left == 0) &&
		    (dsb->probably_valid_to == dsb->startpos) &&
		    dsb->leadin)
			probably_valid_left = dsb->buflen;
		TRACE("streaming buffer probably_valid_to=%ld, probably_valid_left=%ld\n",
		      dsb->probably_valid_to, probably_valid_left);
		/* check whether the app's time is already up */
		if (probably_valid_left < dsb->writelead) {
			WARN("probably_valid_to now within writelead, possible streaming underrun\n");
			/* once we pass the point of no return,
			 * no reason to hold back anymore */
			dsb->probably_valid_to = (DWORD)-1;
			/* we just have to go ahead and mix what we have,
			 * there's no telling what the app is thinking anyway */
		} else {
			/* adjust for our sample size */
			ULONGLONG left = probably_valid_left / dsb->wfx.nBlockAlign;
			TRACE("probably_valid_left=%ld, rem=%ld, acc=%ld\n", probably_valid_left, rem, acc);
			left <<= DSOUND_FREQSHIFT;
			/* adjust for partial samples */
			if (rem >= acc) {
				left += rem - acc;
			} else {
				DWORD sub = acc - rem;
				if (sub > left) left = 0;
				else left -= sub;
			}
			/* adjust for our frequency */
			probably_valid_left = left / dsb->freqAdjust;
			probably_valid_left *= dsb->dsound->wfx.nBlockAlign;
			/* check whether to clip mix_len */
			if (probably_valid_left < mixlen) {
				TRACE("clipping to adjusted probably_valid_left=%ld\n", probably_valid_left);
				mixlen = probably_valid_left;
			}
		}
	}
	/* cut mixlen with what's already been mixed */
	if (mixlen < primary_done) {
		/* huh? and still CalcPlayPosition didn't
		 * detect an underrun? */
		FIXME("problem with underrun detection (mixlen=%ld < primary_done=%ld)\n", mixlen, primary_done);
		return 0;
	}
	len = mixlen - primary_done;
	len &= ~3; /* 4-byte alignment */
	TRACE("remaining mixlen=%ld\n", len);

	if (len < dsb->dsound->fraglen) {
		/* smaller than a fragment, wait until it gets larger
		 * before we take the mixing overhead */
		TRACE("mixlen not worth it, deferring mixing\n");
		still_behind = 1;
		goto post_mix;
	}

	/* ok, we know how much to mix, let's go */
	still_behind = (adv_done > primary_done);
	while (len) {
		slen = dsb->dsound->buflen - dsb->primary_mixpos;
		if (slen > len) slen = len;
		slen = DSOUND_MixInBuffer(dsb, dsb->primary_mixpos, slen);

		if ((dsb->primary_mixpos < dsb->dsound->mixpos) &&
		    (dsb->primary_mixpos + slen >= dsb->dsound->mixpos))
			still_behind = FALSE;

		dsb->primary_mixpos += slen; len -= slen;
		COMPUTE_WRAPAROUND( dsb->primary_mixpos, dsb->dsound->buflen );

		if ((dsb->state == STATE_STOPPED) || !slen) break;
	}
	TRACE("new primary_mixpos=%ld, primary_advbase=%ld\n", dsb->primary_mixpos, dsb->dsound->mixpos);
	TRACE("mixed data len=%ld, still_behind=%d\n", mixlen-len, still_behind);

post_mix:
	/* check if buffer should be considered complete */
	if (buf_left < dsb->writelead &&
	    !(dsb->playflags & DSBPLAY_LOOPING)) {
		dsb->state = STATE_STOPPED;
		dsb->playpos = 0;
		dsb->last_playpos = 0;
		dsb->buf_mixpos = 0;
		dsb->leadin = FALSE;
		dsb->need_remix = FALSE;
		DSOUND_CheckEvent(dsb, buf_left);
	}

	/* return how far we think the primary buffer can
	 * advance its underrun detector...*/
	if (still_behind) return 0;
	if ((mixlen - len) < primary_done) return 0;
	slen = ((dsb->primary_mixpos < dsb->dsound->mixpos) ?
		dsb->dsound->buflen : 0) + dsb->primary_mixpos -
		dsb->dsound->mixpos;
	if (slen > mixlen) {
		/* the primary_done and still_behind checks above should have worked */
		FIXME("problem with advancement calculation (advlen=%ld > mixlen=%ld)\n", slen, mixlen);
		slen = 0;
	}
	return slen;
}

static inline DWORD DSOUND_LockPrimary(DWORD writepos, DWORD mixlen)
{
	if (dsound->hwbuf && !(dsound->drvdesc.dwFlags & DSDDESC_DONTNEEDPRIMARYLOCK)) {
		LPVOID buf1 = NULL, buf2 = NULL;
		DWORD len1 = 0, len2 = 0;
		IDsDriverBuffer_Lock(dsound->hwbuf, &buf1, &len1,
				     &buf2, &len2, writepos, mixlen, 0);
		dsound->buffer = ((LPBYTE)buf1) - writepos;
		if (buf2 && buf2 != dsound->buffer) {
			FIXME("Argh! Driver breaks assumption about fixed looping buffer!\n");
			/* could be handled by saving buf2 somewhere MixOne and helpers can
			 * use it, but it's probably not necessary at this time */
			len2 = 0;
		}
		mixlen = len1 + len2;
	}
	return mixlen;
}

static inline void DSOUND_UnlockPrimary(DWORD writepos, DWORD mixlen)
{
	if (dsound->hwbuf && !(dsound->drvdesc.dwFlags & DSDDESC_DONTNEEDPRIMARYLOCK)) {
		LPVOID buf1 = NULL, buf2 = NULL;
		DWORD len1 = 0, len2 = 0;
		buf1 = ((LPBYTE)dsound->buffer) + writepos;
		len1 = mixlen;
		if ((writepos + len1) > dsound->buflen) {
			buf2 = dsound->buffer;
			len2 = (writepos + len1) - dsound->buflen;
			len1 = dsound->buflen - writepos;
		}
		IDsDriverBuffer_Unlock(dsound->hwbuf, buf1, len1,
				       buf2, len2);
		dsound->buffer = NULL;
	}
}

static DWORD DSOUND_MixToPrimary(DWORD playpos, DWORD writepos, DWORD mixlen, BOOL recover)
{
	INT			i, len, maxlen = 0;
	IDirectSoundBufferImpl	*dsb;

	TRACE("(%ld,%ld,%ld)\n", playpos, writepos, mixlen);

	for (i = dsound->nrofbuffers - 1; i >= 0; i--) {
		dsb = dsound->buffers[i];

		if (!dsb || !(ICOM_VTBL(dsb)))
			continue;
		if (dsb->buflen && dsb->state && !dsb->hwbuf) {
			TRACE("Checking %p, mixlen=%ld\n", dsb, mixlen);
			EnterCriticalSection(&(dsb->lock));
			if (dsb->state == STATE_STOPPING) {
				DSOUND_MixCancel(dsb, writepos, TRUE);
				dsb->state = STATE_STOPPED;
				DSOUND_CheckEvent(dsb, 0);
			} else {
				if ((dsb->state == STATE_STARTING) || recover) {
					dsb->primary_mixpos = writepos;
					memcpy(&dsb->cvolpan, &dsb->volpan, sizeof(dsb->cvolpan));
					dsb->need_remix = FALSE;
				}
				else if (dsb->need_remix) {
					DSOUND_MixCancel(dsb, writepos, TRUE);
					memcpy(&dsb->cvolpan, &dsb->volpan, sizeof(dsb->cvolpan));
					dsb->need_remix = FALSE;
				}
				len = DSOUND_MixOne(dsb, playpos, writepos, mixlen);
				if (dsb->state == STATE_STARTING)
					dsb->state = STATE_PLAYING;
				maxlen = (len > maxlen) ? len : maxlen;
			}
			LeaveCriticalSection(&(dsb->lock));
		}
	}

	return maxlen;
}

static void DSOUND_MixWipe(DWORD writepos, DWORD mixlen)
{
	int nfiller;

	/* the sound of silence */
	nfiller = dsound->wfx.wBitsPerSample == 8 ? 128 : 0;

	if ((writepos + mixlen) > dsound->buflen) {
		memset(dsound->buffer + writepos, nfiller, dsound->buflen - writepos);
		memset(dsound->buffer, nfiller, mixlen - (dsound->buflen - writepos));
	} else {
		memset(dsound->buffer + writepos, nfiller, mixlen);
	}
}

static DWORD DSOUND_MixReset(DWORD writepos)
{
	INT			i;
	IDirectSoundBufferImpl	*dsb;
	DWORD mixlen;
	int nfiller;

	TRACE("(%ld)\n", writepos);

	/* the sound of silence */
	nfiller = dsound->wfx.wBitsPerSample == 8 ? 128 : 0;

	/* reset all buffer mix positions */
	for (i = dsound->nrofbuffers - 1; i >= 0; i--) {
		dsb = dsound->buffers[i];

		if (!dsb || !(ICOM_VTBL(dsb)))
			continue;
		if (dsb->buflen && dsb->state && !dsb->hwbuf) {
			TRACE("Resetting %p\n", dsb);
			EnterCriticalSection(&(dsb->lock));
			if (dsb->state == STATE_STOPPING) {
				dsb->state = STATE_STOPPED;
			}
			else if (dsb->state == STATE_STARTING) {
				/* nothing */
			} else {
				DSOUND_MixCancel(dsb, writepos, FALSE);
				memcpy(&dsb->cvolpan, &dsb->volpan, sizeof(dsb->cvolpan));
				dsb->need_remix = FALSE;
			}
			LeaveCriticalSection(&(dsb->lock));
		}
	}

	mixlen = ((dsound->mixpos < writepos) ? dsound->buflen : 0) + dsound->mixpos - writepos;

	/* reset primary mix position */
	dsound->mixpos = writepos;

	return mixlen;
}

static DWORD DSOUND_CheckReset(IDirectSoundImpl *dsound, DWORD writepos)
{
	DWORD wipe = 0;
	if (dsound->need_remix) {
		wipe = DSOUND_MixReset(writepos);
		dsound->need_remix = FALSE;
		/* maximize Half-Life performance */
		dsound->prebuf = DS_SND_QUEUE_MIN;
		dsound->precount = 0;
	} else {
		dsound->precount++;
		if (dsound->precount >= 4) {
			if (dsound->prebuf < DS_SND_QUEUE_MAX)
				dsound->prebuf++;
			dsound->precount = 0;
		}
	}
	TRACE("premix adjust: %d\n", dsound->prebuf);
	return wipe;
}

void DSOUND_WaveQueue(IDirectSoundImpl *dsound, DWORD mixq)
{
	if (mixq + dsound->pwqueue > DS_HEL_QUEUE) mixq = DS_HEL_QUEUE - dsound->pwqueue;
	TRACE("queueing %ld buffers, starting at %d\n", mixq, dsound->pwwrite);
	for (; mixq; mixq--) {
		waveOutWrite(dsound->hwo, dsound->pwave[dsound->pwwrite], sizeof(WAVEHDR));
		dsound->pwwrite++;
		if (dsound->pwwrite >= DS_HEL_FRAGS) dsound->pwwrite = 0;
		dsound->pwqueue++;
	}
}

/* #define SYNC_CALLBACK */

/* Called with ds->lock held for reading */
static void DSOUND_PerformMix(IDirectSoundImpl *ds)
{
	BOOL forced;
	HRESULT hres;
	DWORD curmixtime = GetTickCount();

#ifdef __APPLE__
	Sleep_Poll();
#endif

	if ((ds->lastmixtime) == curmixtime ||
	    (ds->lastmixtime+1) == curmixtime)
		return; /* avoid hogging the CPU for nothing */

	WINE_START_TIMER("dsound_mix");

	/* whether the primary is forced to play even without secondary buffers */
	forced = ((ds->state == STATE_PLAYING) || (ds->state == STATE_STARTING));

	TRACE("entering at %ld\n", curmixtime);
	if (ds->priolevel != DSSCL_WRITEPRIMARY) {
		BOOL paused = ((ds->state == STATE_STOPPED) || (ds->state == STATE_STARTING));
		/* FIXME: document variables */
 		DWORD playpos, writepos, inq, maxq, frag, wipe, lock;
 		if (ds->hwbuf) {
			hres = IDsDriverBuffer_GetPosition(ds->hwbuf, &playpos, &writepos);
			if (hres) {
			    goto profile_stop;
			}
			/* Well, we *could* do Just-In-Time mixing using the writepos,
			 * but that's a little bit ambitious and unnecessary... */
			/* rather add our safety margin to the writepos, if we're playing */
			if (!paused) {
				writepos += ds->writelead;
    COMPUTE_WRAPAROUND( writepos, ds->buflen );
			} else writepos = playpos;
		}
		else {
 			playpos = ds->pwplay * ds->fraglen;
 			writepos = playpos;
 			if (!paused) {
	 			writepos += DS_HEL_MARGIN * ds->fraglen;
	 			COMPUTE_WRAPAROUND( writepos, ds->buflen );
	 		}
		}
		TRACE("primary playpos=%ld, writepos=%ld, clrpos=%ld, mixpos=%ld\n",
		      playpos,writepos,ds->playpos,ds->mixpos);
		/* wipe out just-played sound data */
		frag = ((playpos < ds->playpos) ? dsound->buflen : 0) + playpos - ds->playpos;
		lock = DSOUND_LockPrimary(ds->playpos, frag);
#ifdef DUMP_SOUND_DATA
		/* dump the sound data that's just been successfully played */
		if (playpos < ds->playpos) {
			dump_sound_output_data(ds, ds->playpos, ds->buflen - ds->playpos);
			dump_sound_output_data(ds, 0, playpos);
		} else {
			dump_sound_output_data(ds, ds->playpos, playpos - ds->playpos);
		}
#endif
		DSOUND_MixWipe(ds->playpos, lock);
		DSOUND_UnlockPrimary(ds->playpos, lock);
		ds->playpos = playpos;

		EnterCriticalSection(&(ds->mixlock));

		/* reset mixing if necessary */
		wipe = DSOUND_CheckReset(dsound, writepos);

		/* check how much prebuffering is left */
		inq = ds->mixpos;
		if (inq < writepos)
			inq += ds->buflen;
		inq -= writepos;

		/* find the maximum we can prebuffer */
		if (!paused) {
			maxq = playpos;
			if (maxq < writepos)
				maxq += ds->buflen;
			maxq -= writepos;
		} else maxq = ds->buflen;

		/* clip maxq to ds->prebuf */
		frag = ds->prebuf * ds->fraglen;
		if (maxq > frag) maxq = frag;

		/* check for consistency */
		if (inq > maxq) {
			/* the playback position must have passed our last
			 * mixed position, i.e. it's an underrun, or we have
			 * nothing more to play */
			TRACE("reached end of mixed data (inq=%ld, maxq=%ld)\n", inq, maxq);
			inq = 0;
			/* stop the playback now, to allow buffers to refill */
			if (ds->state == STATE_PLAYING) {
				ds->state = STATE_STARTING;
			}
			else if (ds->state == STATE_STOPPING) {
				ds->state = STATE_STOPPED;
			}
			else {
				/* how can we have an underrun if we aren't playing? */
				WARN("unexpected primary state (%ld)\n", ds->state);
			}
#ifdef SYNC_CALLBACK
			/* DSOUND_callback may need this lock */
			LeaveCriticalSection(&(ds->mixlock));
#endif
			DSOUND_PrimaryStop(dsound);
#ifdef SYNC_CALLBACK
			EnterCriticalSection(&(ds->mixlock));
#endif
			if (ds->hwbuf) {
				/* the Stop is supposed to reset play position to beginning of buffer */
				/* unfortunately, OSS is not able to do so, so get current pointer */
				hres = IDsDriverBuffer_GetPosition(ds->hwbuf, &playpos, NULL);
				if (hres) {
					LeaveCriticalSection(&(ds->mixlock));
					goto profile_stop;
				}
			} else {
	 			playpos = ds->pwplay * ds->fraglen;
			}
			writepos = playpos;
			ds->playpos = playpos;
			ds->mixpos = writepos;
			inq = 0;
			maxq = ds->buflen;
			if (maxq > frag) maxq = frag;
			wipe = ds->buflen;
			paused = TRUE;
		}

		/* do the mixing */
		lock = DSOUND_LockPrimary(writepos, (wipe > maxq) ? wipe : maxq);
		if (wipe) DSOUND_MixWipe(writepos, wipe);
		frag = DSOUND_MixToPrimary(playpos, writepos, maxq, paused);
		DSOUND_UnlockPrimary(writepos, lock);
		if (forced) frag = maxq - inq;
		ds->mixpos += frag;
		COMPUTE_WRAPAROUND( ds->mixpos, ds->buflen );

		if (frag) {
			/* buffers have been filled, restart playback */
			if (ds->state == STATE_STARTING) {
				ds->state = STATE_PLAYING;
			}
			else if (ds->state == STATE_STOPPED) {
				/* the dsound is supposed to play if there's something to play
				 * even if it is reported as stopped, so don't let this confuse you */
				ds->state = STATE_STOPPING;
			}
			LeaveCriticalSection(&(ds->mixlock));
			if (paused) {
				DSOUND_PrimaryPlay(dsound);
				TRACE("starting playback\n");
			}
		}
		else
			LeaveCriticalSection(&(ds->mixlock));
	} else {
		/* in the DSSCL_WRITEPRIMARY mode, the app is totally in charge... */
		if (ds->state == STATE_STARTING) {
			DSOUND_PrimaryPlay(dsound);
			ds->state = STATE_PLAYING;
		}
		else if (ds->state == STATE_STOPPING) {
			DSOUND_PrimaryStop(dsound);
			ds->state = STATE_STOPPED;
		}
	}
	ds->lastmixtime = GetTickCount();
	TRACE("completed processing at %ld\n", ds->lastmixtime);

profile_stop:
	WINE_STOP_TIMER("dsound_mix");
}

void CALLBACK DSOUND_timer(UINT timerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	IDirectSoundImpl *ds;

	EnterCriticalSection(&dsound_crit);
	ds = dsound;
	if (!ds) {
		TRACE("dsound is dead\n");
		LeaveCriticalSection(&dsound_crit);
		return;
	}

	TRACE("entered\n");
	RtlAcquireResourceShared(&(ds->lock), TRUE);
	LeaveCriticalSection(&dsound_crit);
	if (ds->ref)
		DSOUND_PerformMix(ds);
	RtlReleaseResource(&(ds->lock));
}

void CALLBACK DSOUND_callback(HWAVEOUT hwo, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
        IDirectSoundImpl* This = (IDirectSoundImpl*)dwUser;
	TRACE("entering at %ld, msg=%08x\n", GetTickCount(), msg);
	if (msg == MM_WOM_DONE) {
		DWORD inq, mixq, fraglen, buflen, pwplay, playpos, mixpos;
		if (This->pwqueue == (DWORD)-1) {
			TRACE("completed due to reset\n");
			return;
		}
/* it could be a bad idea to enter critical section here... if there's lock contention,
 * the resulting scheduling delays might obstruct the winmm player thread */
#ifdef SYNC_CALLBACK
		EnterCriticalSection(&(This->mixlock));
#endif
		/* retrieve current values */
		fraglen = This->fraglen;
		buflen = This->buflen;
		pwplay = This->pwplay;
		playpos = pwplay * fraglen;
		mixpos = This->mixpos;
		/* check remaining mixed data */
		inq = ((mixpos < playpos) ? buflen : 0) + mixpos - playpos;
		mixq = inq / fraglen;
		if ((inq - (mixq * fraglen)) > 0) mixq++;
		/* complete the playing buffer */
		TRACE("done playing primary pos=%ld\n", playpos);
		pwplay++;
		if (pwplay >= DS_HEL_FRAGS) pwplay = 0;
		/* write new values */
		This->pwplay = pwplay;
		This->pwqueue--;
		/* queue new buffer if we have data for it */
		if (inq>1) DSOUND_WaveQueue(This, inq-1);
#ifdef SYNC_CALLBACK
		LeaveCriticalSection(&(This->mixlock));
#endif
	}
	TRACE("completed\n");
}
