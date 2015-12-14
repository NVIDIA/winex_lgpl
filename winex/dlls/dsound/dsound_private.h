/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright (c) 2000-2015 NVIDIA CORPORATION. All rights reserved.
 */

/* these are eligible for tuning... they must be high on slow machines... */
/* some stuff may get more responsive with lower values though... */
#define DS_EMULDRIVER 0 /* some games (Quake 2, UT) refuse to accept
				emulated dsound devices. set to 0 ! */
#define DS_HEL_FRAGS 48 /* HEL only: number of waveOut fragments in primary buffer
			 * (changing this won't help you) */
#define DS_HEL_MARGIN 5 /* HEL only: number of waveOut fragments ahead to mix in new buffers
			 * (keep this close or equal to DS_HEL_QUEUE for best results) */
#define DS_HEL_QUEUE  5 /* HEL only: number of waveOut fragments ahead to queue to driver
			 * (this will affect HEL sound reliability and latency) */

#define DS_THREADWAIT_MARGIN 6 /* Linux isn't realtime, so we need to buffer some extra time
				* to make sure the mixing thread gets enough time. */

#define DS_SND_QUEUE_MAX 28 /* max number of fragments to prebuffer */
#define DS_SND_QUEUE_MIN 12 /* min number of fragments to prebuffer */

/* Linux does not support better timing than 10ms */
#define DS_TIME_RES 10  /* Resolution of multimedia timer */
#define DS_TIME_DEL 10  /* Delay of multimedia timer callback, and duration of HEL fragment */

/*****************************************************************************
 * Predeclare the interface implementation structures
 */
typedef struct IDirectSoundImpl IDirectSoundImpl;
typedef struct IDirectSoundBufferImpl IDirectSoundBufferImpl;
typedef struct IDirectSoundNotifyImpl IDirectSoundNotifyImpl;
typedef struct IDirectSound3DListenerImpl IDirectSound3DListenerImpl;
typedef struct IDirectSound3DBufferImpl IDirectSound3DBufferImpl;
typedef struct IDirectSoundCaptureBufferImpl IDirectSoundCaptureBufferImpl;
typedef struct IKsPropertySetImpl IKsPropertySetImpl;
typedef struct PrimaryBufferImpl PrimaryBufferImpl;
typedef struct IDirectSoundNotifyPositions IDirectSoundNotifyPositions;
typedef struct IDirectSoundCaptureImpl IDirectSoundCaptureImpl;

/*****************************************************************************
 * IDirectSound implementation structure
 */
struct IDirectSoundImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSound8);
    DWORD                      ref;
    /* IDirectSoundImpl fields */
    PIDSDRIVER                  driver;
    DSDRIVERDESC                drvdesc;
    DSDRIVERCAPS                drvcaps;
    DWORD                       priolevel;
    WAVEFORMATEX                wfx; /* current main waveformat */
    HWAVEOUT                    hwo;
    LPWAVEHDR                   pwave[DS_HEL_FRAGS];
    UINT                        timerID, pwplay, pwwrite, pwqueue, prebuf, precount;
    DWORD                       fraglen;
    PIDSDRIVERBUFFER            hwbuf;
    LPBYTE                      buffer;
    DWORD                       writelead, buflen, state, playpos, mixpos;
    BOOL                        need_remix;
    int                         nrofbuffers;
    IDirectSoundBufferImpl**    buffers;
    IDirectSound3DListenerImpl*	listener;
    RTL_RWLOCK			lock;
    CRITICAL_SECTION		mixlock;
    DSVOLUMEPAN			volpan;
    DWORD			lastmixtime;
    void*                       dump_state; /* for debugging */
};


/*****************************************************************************
 * IDirectSoundCapture implementation structure
 */
struct IDirectSoundCaptureImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSoundCapture);
    DWORD                              ref;

    /* IDirectSoundCaptureImpl fields */
    DWORD                   dnDevNode;
    DWORD                   dwCapFlags;
    DWORD                   dwCapFormats;
    DWORD                   dwCapChannels;
};

/*****************************************************************************
 * IDirectSoundCaptureBuffer implementation structure
 */
struct IDirectSoundCaptureBufferImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSoundCaptureBuffer8);
    DWORD                              ref;

    /* IDirectSoundCaptureBufferImpl fields */
    CRITICAL_SECTION        lock;
    WAVEFORMATEX            wfx;
    DWORD                   flags;
    DWORD                   buflen;
    DWORD                   fraglen;
    DWORD                   frags;
    HWAVEIN                 hwi;
    LPBYTE                  buffer;
    LPWAVEHDR              *pwave;
    UINT                    pwcap, pwread, pwqueue;
    DWORD                   capflags;

    /* IDirectSoundNotifyImpl fields */
    IDirectSoundNotifyPositions *positions;
};


/*****************************************************************************
 * IDirectSoundBuffer implementation structure
 */
typedef struct _IDirectSoundSWBuffer{
    DWORD       ref;
    BYTE *      buffer;
} IDirectSoundSWBuffer;

struct IDirectSoundBufferImpl
{
    /* FIXME: document */
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSoundBuffer8);
    DWORD                     ref;
    DWORD                     aggref;
    /* IDirectSoundBufferImpl fields */
    IDirectSoundImpl*         dsound;
    IDirectSound3DBufferImpl* ds3db;
    IKsPropertySetImpl*       iks;
    CRITICAL_SECTION          lock;
    PIDSDRIVERBUFFER          hwbuf;
    WAVEFORMATEX              wfx;
    IDirectSoundSWBuffer *    swbuf;
    DWORD                     playflags,state,leadin;
    DWORD                     playpos,startpos,writelead,buflen;
    DWORD                     nAvgBytesPerSec;
    DWORD                     freq;
    DSVOLUMEPAN               volpan, cvolpan;
    DSBUFFERDESC              dsbd;
    void*                     dump_state; /* for debugging */
    /* used for frequency conversion (PerfectPitch) */
    ULONG                     freqAdjust, freqAcc;
    /* used for intelligent (well, sort of) prebuffering */
    DWORD                     probably_valid_to, last_playpos;
    DWORD                     primary_mixpos, buf_mixpos;
    BOOL                      need_remix;

    /* IDirectSoundNotifyImpl fields */
    IDirectSoundNotifyPositions *positions;

    /* when we mix sounds, for which amplifications have changed, we try to smooth it out
       over a number of samples from the last previously played Ampfactor, to avoid static*/
    int                       leftPreviousAmpFactor;
    int                       rightPreviousAmpFactor;
};

HRESULT WINAPI IDirectSoundNotifyImpl_Create(
	IDirectSoundBufferImpl *        buffer,
	IDirectSoundCaptureBufferImpl * capture,
	IDirectSoundNotifyImpl **       pnotify);

HRESULT WINAPI IDirectSoundNotifyImpl_Duplicate(
	IDirectSoundBufferImpl *    original,
	IDirectSoundBufferImpl *    owner);

HRESULT WINAPI SecondaryBuffer_Create(
	IDirectSoundImpl *This,
	IDirectSoundBufferImpl **pdsb,
	LPCDSBUFFERDESC dsbd);

struct PrimaryBufferImpl {
	ICOM_VFIELD(IDirectSoundBuffer8);
	DWORD			ref;
	IDirectSoundImpl*	dsound;
	DSBUFFERDESC		dsbd;
};

HRESULT WINAPI PrimaryBuffer_Create(
	IDirectSoundImpl *This,
	PrimaryBufferImpl **pdsb,
	LPCDSBUFFERDESC dsbd);


/*****************************************************************************
 * IDirectSoundNotify implementation structure
 */
struct IDirectSoundNotifyPositions{
    DWORD                   refCount;

    LPDSBPOSITIONNOTIFY     notifies;
    int                     nrofnotifies;
};

struct IDirectSoundNotifyImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSoundNotify);
    DWORD                           ref;

    /* IDirectSoundNotifyImpl fields */
    IDirectSoundBufferImpl *        dsb;
    IDirectSoundCaptureBufferImpl * dscb;
};

/*****************************************************************************
 *  IDirectSound3DListener implementation structure
 */
struct IDirectSound3DListenerImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSound3DListener);
    DWORD                                ref;
    /* IDirectSound3DListenerImpl fields */
    PrimaryBufferImpl*      dsb;
    DS3DLISTENER            ds3dl;
    D3DVECTOR               vOrientRight;
    CRITICAL_SECTION        lock;
    BOOL                    need_recalc;
};

HRESULT WINAPI IDirectSound3DListenerImpl_Create(
	PrimaryBufferImpl *This,
	IDirectSound3DListenerImpl **pdsl);

/*****************************************************************************
 *  IKsPropertySet implementation structure
 */
struct IKsPropertySetImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IKsPropertySet);
    DWORD			ref;
    /* IKsPropertySetImpl fields */
    IDirectSoundBufferImpl*	dsb;
};

HRESULT WINAPI IKsPropertySetImpl_Create(
	IDirectSoundBufferImpl *This,
	IKsPropertySetImpl **piks);

/*****************************************************************************
 * IDirectSound3DBuffer implementation structure
 */
struct IDirectSound3DBufferImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDirectSound3DBuffer);
    DWORD                   ref;
    /* IDirectSound3DBufferImpl fields */
    IDirectSoundBufferImpl* dsb;
    DS3DBUFFER              ds3db;
    LONG                    lVolume;
    CRITICAL_SECTION        lock;
    BOOL                    need_recalc;
};

HRESULT WINAPI IDirectSound3DBufferImpl_Create(
	IDirectSoundBufferImpl *This,
	IDirectSound3DBufferImpl **pds3db);

void DSOUND_RecalcVolPan(PDSVOLUMEPAN volpan);
void DSOUND_RecalcFormat(IDirectSoundBufferImpl *dsb);
void DSOUND_Recalc3DBuffer(IDirectSoundBufferImpl *dsb);

/* primary.c */

HRESULT DSOUND_PrimaryCreate(IDirectSoundImpl *This);
HRESULT DSOUND_PrimaryDestroy(IDirectSoundImpl *This);
HRESULT DSOUND_PrimaryPlay(IDirectSoundImpl *This);
HRESULT DSOUND_PrimaryStop(IDirectSoundImpl *This);
HRESULT DSOUND_PrimaryGetPosition(IDirectSoundImpl *This, LPDWORD playpos, LPDWORD writepos);

/* buffer.c */

DWORD DSOUND_CalcPlayPosition(IDirectSoundBufferImpl *This,
			      DWORD state, DWORD pplay, DWORD pwrite, DWORD pmix,
			      DWORD bmix, DWORD bacc, DWORD *rem);
DWORD WINAPI IDirectSoundBufferImpl_AddRefAggregate(LPDIRECTSOUNDBUFFER8 iface);
DWORD WINAPI IDirectSoundBufferImpl_ReleaseAggregate(LPDIRECTSOUNDBUFFER8 iface);

/* capture.c */

HRESULT WINAPI  IDirectSoundCaptureBufferImpl_QueryInterface(LPDIRECTSOUNDCAPTUREBUFFER8 iface, REFIID riid, LPVOID* ppobj );
ULONG WINAPI    IDirectSoundCaptureBufferImpl_AddRef( LPDIRECTSOUNDCAPTUREBUFFER8 iface );
ULONG WINAPI    IDirectSoundCaptureBufferImpl_Release( LPDIRECTSOUNDCAPTUREBUFFER8 iface );
HRESULT WINAPI  DSOUND_DirectSoundCaptureCreate( LPDIRECTSOUNDCAPTURE* lplpDSC, LPUNKNOWN pUnkOuter );

/* mixer.c */

void dump_sound_data(IDirectSoundBufferImpl *dsb, DWORD start, DWORD len);
void dump_sound_end(IDirectSoundBufferImpl *dsb);
void dump_sound_output_data(IDirectSoundImpl *dsound, DWORD start, DWORD len);
void dump_sound_output_end(IDirectSoundImpl *dsound);

void DSOUND_CheckEvent(IDirectSoundBufferImpl *dsb, int len);
void DSOUND_ForceRemix(IDirectSoundBufferImpl *dsb);
void DSOUND_MixCancelAt(IDirectSoundBufferImpl *dsb, DWORD buf_writepos);
void DSOUND_WaveQueue(IDirectSoundImpl *dsound, DWORD mixq);
void CALLBACK DSOUND_timer(UINT timerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2);
void CALLBACK DSOUND_callback(HWAVEOUT hwo, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2);

#define STATE_STOPPED  0
#define STATE_STARTING 1
#define STATE_PLAYING  2
#define STATE_STOPPING 3

#define DSOUND_FREQSHIFT (14)

extern IDirectSoundImpl* dsound;
extern CRITICAL_SECTION dsound_crit;
extern HANDLE dsound_heap;

struct PrimaryBuffer {
	DWORD ref;
	PIDSDRIVERBUFFER hwbuf;
	DWORD state;
};

extern HRESULT mmErr(UINT err);

#define CLAMP(x,m1,m2) ( WINE_EXPECT( (x)>(m2), 0 )?(m2):(WINE_EXPECT( (x)<(m1), 0)?(m1):(x)))

#define COMPUTE_WRAPAROUND(pos,maxlen) if( WINE_EXPECT( (pos) >= (maxlen), 0 ) ) (pos) -= (maxlen)
#define COMPUTE_USER_WRAPAROUND(pos,maxlen) while( (pos) >= (maxlen) ) (pos) -= (maxlen)

extern BOOL bMmxExtensionsAvailable;
