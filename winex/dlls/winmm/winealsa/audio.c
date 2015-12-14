/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * Sample Wine Driver for Advanced Linux Sound System (ALSA)
 *      Based on version <final> of the ALSA API
 *
 * Copyright    2002 Eric Pouech
 *              2002 Marco Pietrobono
 *              2003 Christian Costa : WaveIn support
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
 */

/* unless someone makes a wineserver kernel module, Unix pipes are faster than win32 events */
#define USE_PIPE_SYNC

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_VALUES_H
#include <values.h>
#undef MAXSHORT
#undef MAXLONG
#undef MINSHORT
#undef MINLONG
#endif

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "winbase.h"
#include "windef.h"
#include "wingdi.h"
#include "winreg.h"
#include "winerror.h"
#include "winuser.h"
#include "mmddk.h"
#include "mmreg.h"
#include "msacm.h"
#include "dsound.h"
#include "dsdriver.h"
#define ALSA_PCM_OLD_HW_PARAMS_API
#define ALSA_PCM_OLD_SW_PARAMS_API
#include "alsa.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "wine/port.h"

WINE_DEFAULT_DEBUG_CHANNEL(wave);

#ifndef SIZEOFARRAY
# define SIZEOFARRAY(x)         (sizeof(x) / sizeof((x)[0]))
#endif

/* Allow 2% deviation for sample rates */
#define NEAR_MATCH(rate1,rate2) (((50*((int)(rate1)-(int)(rate2)))/(rate1))==0)


#if defined(HAVE_ALSA) && ((SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9) || SND_LIB_MAJOR >= 1)

/* internal ALSALIB functions */
snd_pcm_uframes_t _snd_pcm_mmap_hw_ptr(snd_pcm_t *pcm);


static void *alsa_so_handle;
static int (*psnd_pcm_hw_params_set_rate_resample)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);

#define MAX_WAVEOUTDRV 	(1)
#define MAX_WAVEINDRV 	(1)

/* state diagram for waveOut writing:
 *
 * +---------+-------------+---------------+---------------------------------+
 * |  state  |  function   |     event     |            new state	     |
 * +---------+-------------+---------------+---------------------------------+
 * |	     | open()	   |		   | STOPPED		       	     |
 * | PAUSED  | write()	   | 		   | PAUSED		       	     |
 * | STOPPED | write()	   | <thrd create> | PLAYING		  	     |
 * | PLAYING | write()	   | HEADER        | PLAYING		  	     |
 * | (other) | write()	   | <error>       |		       		     |
 * | (any)   | pause()	   | PAUSING	   | PAUSED		       	     |
 * | PAUSED  | restart()   | RESTARTING    | PLAYING (if no thrd => STOPPED) |
 * | (any)   | reset()	   | RESETTING     | STOPPED		      	     |
 * | (any)   | close()	   | CLOSING	   | CLOSED		      	     |
 * +---------+-------------+---------------+---------------------------------+
 */

/* states of the playing device */
#define	WINE_WS_PLAYING		0
#define	WINE_WS_PAUSED		1
#define	WINE_WS_STOPPED		2
#define WINE_WS_CLOSED		3

/* events to be send to device */
enum win_wm_message {
    WINE_WM_PAUSING = WM_USER + 1, WINE_WM_RESTARTING, WINE_WM_RESETTING, WINE_WM_HEADER,
    WINE_WM_UPDATE, WINE_WM_BREAKLOOP, WINE_WM_CLOSING, WINE_WM_STARTING, WINE_WM_STOPPING
};

#ifdef USE_PIPE_SYNC
#define SIGNAL_OMR(omr) do { if (use_pipes) { int x = 0; write((omr)->msg_pipe[1], &x, sizeof(x)); } \
                             else SetEvent((omr)->msg_event); } while (0)
#define CLEAR_OMR(omr) do { if (use_pipes) { int x = 0; read((omr)->msg_pipe[0], &x, sizeof(x)); } } while (0)  
#define RESET_OMR(omr) do { if (!use_pipes) ResetEvent((omr)->msg_event); } while (0)
#define WAIT_OMR(omr, sleep) \
  do { \
    if (use_pipes) { \
      struct pollfd pfd; \
      int sleep_for = sleep; \
\
      pfd.fd = (omr)->msg_pipe[0]; \
      pfd.events = POLLIN; \
      if( poll(&pfd, 1, sleep_for) < 0 ) \
      { \
        if( errno == EINTR ) \
        { \
          /* interrupted, sleep again for 1/4 of the time to avoid infinite loop */ \
          sleep_for >>= 2; \
          continue; \
        } \
        else ERR( "poll returned %d:%s\n", errno, strerror(errno) ); \
      } \
    } \
    else WaitForSingleObject((omr)->msg_event, sleep); \
  } while (0)
#else
#define SIGNAL_OMR(omr) do { SetEvent((omr)->msg_event); } while (0)
#define CLEAR_OMR(omr) do { } while (0)
#define RESET_OMR(omr) do { ResetEvent((omr)->msg_event); } while (0)
#define WAIT_OMR(omr, sleep) \
  do { WaitForSingleObject((omr)->msg_event, sleep); } while (0)
#endif

typedef struct {
    enum win_wm_message 	msg;	/* message identifier */
    DWORD	                param;  /* parameter for this message */
    HANDLE	                hEvent;	/* if message is synchronous, handle of event for synchro */
} ALSA_MSG;

/* implement an in-process message ring for better performance
 * (compared to passing thru the server)
 * this ring will be used by the input (resp output) record (resp playback) routine
 */
typedef struct {
#define ALSA_RING_BUFFER_SIZE	128
    int				entries; /* # of entries in the ring */
    ALSA_MSG			* messages;
    int				msg_tosave;
    int				msg_toget;
#ifdef USE_PIPE_SYNC
    int                         msg_pipe[2];
#endif
    HANDLE                      msg_event;
    CRITICAL_SECTION		msg_crst;
} ALSA_MSG_RING;

typedef struct {
    /* Windows information */
    volatile int		state;			/* one of the WINE_WS_ manifest constants */
    WAVEOPENDESC		waveDesc;
    WORD			wFlags;
    WAVEFORMATEXTENSIBLE	format;
    WAVEOUTCAPSA		caps;

    /* ALSA information (ALSA 0.9/1.x uses two different devices for playback/capture) */
    char *			device;
    snd_pcm_t*                  p_handle;                 /* handle to ALSA playback device */
    snd_pcm_t*                  c_handle;                 /* handle to ALSA capture device */
    snd_pcm_hw_params_t *	hw_params;		/* ALSA Hw params */

    char *			ctl_device;
    snd_ctl_t *                 ctl;                    /* control handle for the playback volume */
    snd_ctl_elem_id_t *         playback_eid;		/* element id of the playback volume control */
    snd_ctl_elem_value_t *      playback_evalue;	/* element value of the playback volume control */
    snd_ctl_elem_info_t *       playback_einfo;         /* element info of the playback volume control */

    snd_pcm_sframes_t           (*write)(snd_pcm_t *, const void *, snd_pcm_uframes_t );

    DWORD                       dwBufferSize;           /* size of whole ALSA buffer in bytes */
    LPWAVEHDR			lpQueuePtr;		/* start of queued WAVEHDRs (waiting to be notified) */
    LPWAVEHDR			lpPlayPtr;		/* start of not yet fully played buffers */
    DWORD			dwPartialOffset;	/* Offset of not yet written bytes in lpPlayPtr */

    LPWAVEHDR			lpLoopPtr;              /* pointer of first buffer in loop, if any */
    DWORD			dwLoops;		/* private copy of loop counter */

    DWORD			dwPlayedTotal;		/* number of bytes actually played since opening */
    DWORD			dwWrittenTotal;		/* number of bytes written to ALSA buffer since opening */

    /* synchronization stuff */
    HANDLE			hStartUpEvent;
    HANDLE			hThread;
    DWORD			dwThreadID;
    ALSA_MSG_RING		msgRing;

    /* DirectSound stuff */

} WINE_WAVEOUT;

#define DEVICE_NAME_LEN 32

typedef struct {
    /* Windows information */
    volatile int		state;			/* one of the WINE_WS_ manifest constants */
    WAVEOPENDESC		waveDesc;
    WORD			wFlags;
    WAVEFORMATEXTENSIBLE	format;
    HACMSTREAM			hConv;
    WAVEOUTCAPSA		caps;

    /* ALSA information (ALSA 0.9/1.x uses two different devices for playback/capture) */
    char			device[DEVICE_NAME_LEN];
    snd_pcm_t*                  p_handle;                 /* handle to ALSA playback device */
    snd_pcm_t*                  c_handle;                 /* handle to ALSA capture device */
    snd_pcm_hw_params_t *	hw_params;		/* ALSA Hw params */

    snd_ctl_t *                 ctl;                    /* control handle for the playback volume */
    snd_ctl_elem_id_t *         playback_eid;		/* element id of the playback volume control */
    snd_ctl_elem_value_t *      playback_evalue;	/* element value of the playback volume control */
    snd_ctl_elem_info_t *       playback_einfo;         /* element info of the playback volume control */

    snd_pcm_sframes_t           (*read)(snd_pcm_t *, void *, snd_pcm_uframes_t );

    struct pollfd		*ufds;
    int				count;

    DWORD			dwPeriodSize;		/* size of OSS buffer period */
    DWORD                       dwBufferSize;           /* size of whole ALSA buffer in bytes */
    LPWAVEHDR			lpQueuePtr;		/* start of queued WAVEHDRs (waiting to be notified) */
    LPWAVEHDR			lpPlayPtr;		/* start of not yet fully played buffers */

    LPWAVEHDR			lpLoopPtr;              /* pointer of first buffer in loop, if any */
    DWORD			dwLoops;		/* private copy of loop counter */

    /*DWORD			dwPlayedTotal; */
    DWORD			dwTotalRecorded;

    /* synchronization stuff */
    HANDLE			hStartUpEvent;
    HANDLE			hThread;
    DWORD			dwThreadID;
    ALSA_MSG_RING		msgRing;

    /* DirectSound stuff */
    DSDRIVERDESC                ds_desc;
    GUID                        ds_guid;
} WINE_WAVEIN;

static WINE_WAVEOUT	WOutDev   [MAX_WAVEOUTDRV];
static DWORD            ALSA_WodNumDevs;
static WINE_WAVEIN	WInDev   [MAX_WAVEINDRV];
static DWORD            ALSA_WidNumDevs;

#ifdef USE_PIPE_SYNC
static BOOL             use_pipes;
#endif

static DWORD wodDsCreate(UINT wDevID, PIDSDRIVER* drv);

/* These strings used only for tracing */
#if 1
static const char *wodPlayerCmdString[] = {
    "WINE_WM_PAUSING",
    "WINE_WM_RESTARTING",
    "WINE_WM_RESETTING",
    "WINE_WM_HEADER",
    "WINE_WM_UPDATE",
    "WINE_WM_BREAKLOOP",
    "WINE_WM_CLOSING",
    "WINE_WM_STARTING",
    "WINE_WM_STOPPING",
};
#endif

/*======================================================================*
 *                  Low level WAVE implementation			*
 *======================================================================*/

/**************************************************************************
 * 			ALSA_InitializeVolumeCtl		[internal]
 *
 * used to initialize the PCM Volume Control
 */
static int ALSA_InitializeVolumeCtl(WINE_WAVEOUT * wwo)
{
    snd_ctl_t *                 ctl = NULL;
    snd_ctl_card_info_t *	cardinfo;
    snd_ctl_elem_list_t *       elemlist;
    snd_ctl_elem_id_t *         e_id;
    snd_ctl_elem_info_t *       einfo;
    snd_hctl_t *                hctl = NULL;
    snd_hctl_elem_t *           elem;
    int                         nCtrls;
    int                         i;

    snd_ctl_card_info_alloca(&cardinfo);
    memset(cardinfo,0,snd_ctl_card_info_sizeof());

    snd_ctl_elem_list_alloca(&elemlist);
    memset(elemlist,0,snd_ctl_elem_list_sizeof());

    snd_ctl_elem_id_alloca(&e_id);
    memset(e_id,0,snd_ctl_elem_id_sizeof());

    snd_ctl_elem_info_alloca(&einfo);
    memset(einfo,0,snd_ctl_elem_info_sizeof());

#define EXIT_ON_ERROR(f,txt) do \
{ \
    int err; \
    if ( (err = (f) ) < 0) \
    { \
	ERR(txt ": %s\n", snd_strerror(err)); \
	if (hctl) \
	    snd_hctl_close(hctl); \
	if (ctl) \
	    snd_ctl_close(ctl); \
	return -1; \
    } \
} while(0)

    EXIT_ON_ERROR( snd_ctl_open(&ctl,wwo->ctl_device,0) , "ctl open failed" );
    EXIT_ON_ERROR( snd_ctl_card_info(ctl, cardinfo), "card info failed");
    EXIT_ON_ERROR( snd_ctl_elem_list(ctl, elemlist), "elem list failed");

    nCtrls = snd_ctl_elem_list_get_count(elemlist);

    EXIT_ON_ERROR( snd_hctl_open(&hctl,wwo->ctl_device,0), "hctl open failed");
    EXIT_ON_ERROR( snd_hctl_load(hctl), "hctl load failed" );

    elem=snd_hctl_first_elem(hctl);
    for ( i= 0; i<nCtrls; i++) {

	memset(e_id,0,snd_ctl_elem_id_sizeof());

	snd_hctl_elem_get_id(elem,e_id);
/*
	TRACE("ctl: #%d '%s'%d\n",
				   snd_ctl_elem_id_get_numid(e_id),
				   snd_ctl_elem_id_get_name(e_id),
				   snd_ctl_elem_id_get_index(e_id));
*/
	if ( !strcmp("PCM Playback Volume", snd_ctl_elem_id_get_name(e_id)) )
	{
	    EXIT_ON_ERROR( snd_hctl_elem_info(elem,einfo), "hctl elem info failed" );

	    /* few sanity checks... you'll never know... */
	    if ( snd_ctl_elem_info_get_type(einfo) != SND_CTL_ELEM_TYPE_INTEGER )
	    	WARN("playback volume control is not an integer\n");
	    if ( !snd_ctl_elem_info_is_readable(einfo) )
	    	WARN("playback volume control is readable\n");
	    if ( !snd_ctl_elem_info_is_writable(einfo) )
	    	WARN("playback volume control is readable\n");

	    TRACE("   ctrl range: min=%ld  max=%ld  step=%ld\n",
	         snd_ctl_elem_info_get_min(einfo),
	         snd_ctl_elem_info_get_max(einfo),
	         snd_ctl_elem_info_get_step(einfo));

	    EXIT_ON_ERROR( snd_ctl_elem_id_malloc(&wwo->playback_eid), "elem id malloc failed" );
	    EXIT_ON_ERROR( snd_ctl_elem_info_malloc(&wwo->playback_einfo), "elem info malloc failed" );
	    EXIT_ON_ERROR( snd_ctl_elem_value_malloc(&wwo->playback_evalue), "elem value malloc failed" );

	    /* ok, now we can safely save these objects for later */
	    snd_ctl_elem_id_copy(wwo->playback_eid, e_id);
	    snd_ctl_elem_info_copy(wwo->playback_einfo, einfo);
	    snd_ctl_elem_value_set_id(wwo->playback_evalue, wwo->playback_eid);
	    wwo->ctl = ctl;
	}

	elem=snd_hctl_elem_next(elem);
    }
    snd_hctl_close(hctl);
#undef EXIT_ON_ERROR
    return 0;
}

/**************************************************************************
 * 			ALSA_XRUNRecovery		[internal]
 *
 * used to recovery from XRUN errors (buffer underflow/overflow)
 */
static int ALSA_XRUNRecovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
             ERR( "underrun recovery failed. prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            Sleep(100);       /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                ERR("recovery from suspend failed, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

/**************************************************************************
 * 			ALSA_TraceParameters		[internal]
 *
 * used to trace format changes, hw and sw parameters
 */
static void ALSA_TraceParameters(snd_pcm_hw_params_t * hw_params, snd_pcm_sw_params_t * sw, int full)
{
    snd_pcm_format_t   format = snd_pcm_hw_params_get_format(hw_params);
    snd_pcm_access_t   access = snd_pcm_hw_params_get_access(hw_params);

#define X(x) ((x)? "true" : "false")
    if (full)
	TRACE("FLAGS: sampleres=%s overrng=%s pause=%s resume=%s syncstart=%s batch=%s block=%s double=%s "
    	      "halfd=%s joint=%s \n",
	      X(snd_pcm_hw_params_can_mmap_sample_resolution(hw_params)),
	      X(snd_pcm_hw_params_can_overrange(hw_params)),
	      X(snd_pcm_hw_params_can_pause(hw_params)),
	      X(snd_pcm_hw_params_can_resume(hw_params)),
	      X(snd_pcm_hw_params_can_sync_start(hw_params)),
	      X(snd_pcm_hw_params_is_batch(hw_params)),
	      X(snd_pcm_hw_params_is_block_transfer(hw_params)),
	      X(snd_pcm_hw_params_is_double(hw_params)),
	      X(snd_pcm_hw_params_is_half_duplex(hw_params)),
	      X(snd_pcm_hw_params_is_joint_duplex(hw_params)));
#undef X

    if (access >= 0)
	TRACE("access=%s\n", snd_pcm_access_name(access));
    else
    {
	snd_pcm_access_mask_t * acmask;
	snd_pcm_access_mask_alloca(&acmask);
	snd_pcm_hw_params_get_access_mask(hw_params, acmask);
	for ( access = SND_PCM_ACCESS_MMAP_INTERLEAVED; access <= SND_PCM_ACCESS_LAST; access++)
	    if (snd_pcm_access_mask_test(acmask, access))
		TRACE("access=%s\n", snd_pcm_access_name(access));
    }

    if (format >= 0)
    {
	TRACE("format=%s\n", snd_pcm_format_name(format));

    }
    else
    {
	snd_pcm_format_mask_t *     fmask;

	snd_pcm_format_mask_alloca(&fmask);
	snd_pcm_hw_params_get_format_mask(hw_params, fmask);
	for ( format = SND_PCM_FORMAT_S8; format <= SND_PCM_FORMAT_LAST ; format++)
	    if ( snd_pcm_format_mask_test(fmask, format) )
		TRACE("format=%s\n", snd_pcm_format_name(format));
    }

#define X(x) do { \
int n = snd_pcm_hw_params_get_##x(hw_params); \
if (n<0) \
    TRACE(#x "_min=%ld " #x "_max=%ld\n", \
        (long int)snd_pcm_hw_params_get_##x##_min(hw_params), \
	(long int)snd_pcm_hw_params_get_##x##_max(hw_params)); \
else \
    TRACE(#x "=%d\n", n); \
} while(0)
    X(channels);
    X(buffer_size);
#undef X

#define X(x) do { \
int n = snd_pcm_hw_params_get_##x(hw_params,0); \
if (n<0) \
    TRACE(#x "_min=%ld " #x "_max=%ld\n", \
        (long int)snd_pcm_hw_params_get_##x##_min(hw_params,0), \
	(long int)snd_pcm_hw_params_get_##x##_max(hw_params,0)); \
else \
    TRACE(#x "=%d\n", n); \
} while(0)
    X(rate);
    X(buffer_time);
    X(periods);
    X(period_size);
    X(period_time);
    X(tick_time);
#undef X

    if (!sw)
	return;


}

/* Find the first valid ALSA input device, starting with "default", and then "hw" */
static void ALSA_GetInputDeviceName(WINE_WAVEIN* wwi)
{
    snd_pcm_t*                  h = NULL;

    /* First, try 'default' (provides some rate conversions and such things for us) */
    strncpy(wwi->device, "default", DEVICE_NAME_LEN);
    snd_pcm_open(&h, wwi->device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);

    if (!h) {
        /* Lastly try 'hw' (direct hardware) */
        strncpy(wwi->device, "hw", DEVICE_NAME_LEN);
        snd_pcm_open(&h, wwi->device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    }
    if (h) {
        /* Now, we know the device name we'll be using */
        snd_pcm_close(h);
    }
}

snd_pcm_t *ALSA_findSoundDevice(const char *baseName, unsigned int mode, unsigned int flags){
    snd_pcm_t * h = NULL;
    int         err;


    TRACE("trying to open the device %s {mode = %s, flags = 0x%08x}\n", 
                debugstr_a(baseName), 
                mode == SND_PCM_STREAM_PLAYBACK ? "PLAYBACK" : "CAPTURE",
                flags);

    /* always try to open the device by the given base name first */
    err = snd_pcm_open(&h, baseName, mode, flags);
        
        
    /* couldn't open the device by the default name => fall back to a device search */
    if (err < 0){
        ERR("open pcm: %s\n", snd_strerror(err));
            
        if (h)
            snd_pcm_close(h);
         
        h = NULL;
    }


    /* the config file didn't specify an explicit device or the specific device couldn't be opened =>
         try searching for the first available device */    
    if (h == NULL){
        const char *devices[] = {baseName, "hw", "plughw"};
        const int   maxDevice = 10;
        int         devicesToTry = SIZEOFARRAY(devices);
        int         firstDevice = 0;
        char *      tmpDevice;
        size_t      len = 0;
        int         i, d;



        /* a specific device was tried earlier and failed => we need to try only the general device names */
        if (strchr(baseName, ':') || !strcasecmp(baseName, "default"))
            firstDevice = 1;


        /* make sure the allocated buffer size will still be large enough */
        for (d = 0; d < devicesToTry; d++)
            len = max(len, strlen(devices[d]));            

        /* allocate a temp buffer to hold the base device name plus a card/device specifier */
        tmpDevice = (char *)HeapAlloc(GetProcessHeap(), 0, len + 4 + 1);
        
        if (tmpDevice == NULL){
            ERR("could not allocate %d bytes for a scratch buffer (?!?!)\n", len + 4 + 1);
            
            return NULL;
        }


        /* search all available devices until we find a usable one */
        for (d = firstDevice; d < devicesToTry && h == NULL; d++){
        
            /* copy the base name and add the card/device numbering.  The format of the
               device string is <deviceName>:<cardNumber>,<deviceNumber>.  Both <cardNumber>
               and <deviceNumber> are 0-based. */
            len = strlen(devices[d]);
            memcpy(tmpDevice, devices[d], len);
            tmpDevice[len++] = ':';
            tmpDevice[len++] = '0';
            tmpDevice[len++] = ',';
            tmpDevice[len] =   '0';
            tmpDevice[len + 1] = 0;
        
            /* search for a numbered device that will successfully open */
            for (i = 0; i < maxDevice; i++){
                tmpDevice[len] = i + '0';
                    
                TRACE("trying to open numbered device '%s'\n", tmpDevice);
                err = snd_pcm_open(&h, tmpDevice, mode, flags);
                    
                /* couldn't open the device => keep looking */
                if (err < 0){
                    ERR("could not open the device '%s' {err = %s (%d)}\n", tmpDevice, snd_strerror(err), err);

                    if (h)
                        snd_pcm_close(h);

                    h = NULL;

                    continue;
                }

                /* found an available device => stop searching */
                else
                    break;
            }
        }
        
        
        HeapFree(GetProcessHeap(), 0, tmpDevice);
    }


    return h;
}


#define IS_OPTION_TRUE(ch) \
  ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')
#define IS_OPTION_FALSE(ch) \
  ((ch) == 'n' || (ch) == 'N' || (ch) == 'f' || (ch) == 'F' || (ch) == '0')

/******************************************************************
 *		ALSA_WaveInit
 *
 * Initialize internal structures from ALSA information
 */
LONG ALSA_WaveInit(void)
{
    snd_pcm_t*                  h = NULL;
    snd_pcm_info_t *            info;
    snd_pcm_hw_params_t *       hw_params;
    WINE_WAVEOUT*	        wwo;
    WINE_WAVEIN*	        wwi;
    DWORD	count;
    HKEY	hkey;
    char	buffer[16];
    BOOL	use_mmap = TRUE;
    unsigned int nChannels;

#ifdef USE_PIPES
    use_pipes = !server_scheduler_active();
#endif

    wwo = &WOutDev[0];

    /* FIXME: use better values */
    wwo->device = "hw";
    wwo->ctl_device = "hw";
    wwo->state = WINE_WS_CLOSED;
    wwo->caps.wMid = 0x0002;
    wwo->caps.wPid = 0x0104;
    strcpy(wwo->caps.szPname, "SB16 Wave Out");
    wwo->caps.vDriverVersion = 0x0100;
    wwo->caps.dwFormats = 0x00000000;
    wwo->caps.dwSupport = WAVECAPS_VOLUME;

    if (!RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\winealsa", 0, NULL,
                         REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL)) {
        count = sizeof(buffer);
        if (!RegQueryValueExA(hkey, "UseMMap", 0, NULL, (BYTE *)buffer, &count)) {
            use_mmap = IS_OPTION_TRUE(buffer[0]);
        }
        count = sizeof(buffer);
        if (!RegQueryValueExA(hkey, "pcm0", 0, NULL, (BYTE *)buffer, &count)) {
            char *st = HeapAlloc(GetProcessHeap(), 0, count+1);

            
            if (st == NULL){
                ERR("could not allocate %lu bytes for a pcm0 name buffer\n", count + 1);
                
                return -1;
            }
            
            memcpy(st, buffer, count);
            st[count] = 0;
            wwo->device = st;
        }
        count = sizeof(buffer);
        if (!RegQueryValueExA(hkey, "ctl0", 0, NULL, (BYTE *)buffer, &count)) {
            char *st = HeapAlloc(GetProcessHeap(), 0, count+1);

            
            if (st == NULL){
                ERR("could not allocate %lu bytes for a ctl0 name buffer\n", count + 1);
                
                return -1;
            }
            
            memcpy(st, buffer, count);
            st[count] = 0;
            wwo->ctl_device = st;
        }
        RegCloseKey(hkey);
    }

    alsa_so_handle = wine_dlopen("libasound.so.2", RTLD_LAZY|RTLD_GLOBAL, NULL, 0);
    if (!alsa_so_handle)
    {
        ERR("Error: ALSA lib needs to be loaded with flags RTLD_LAZY and RTLD_GLOBAL.\n");
        return -1;
    }

    psnd_pcm_hw_params_set_rate_resample = wine_dlsym(alsa_so_handle, "snd_pcm_hw_params_set_rate_resample", NULL, 0);

    snd_pcm_info_alloca(&info);
    snd_pcm_hw_params_alloca(&hw_params);

#define EXIT_ON_ERROR(f,txt) do { int err; if ( (err = (f) ) < 0) { ERR(txt ": %s\n", snd_strerror(err)); if (h) snd_pcm_close(h); return -1; } } while(0)

    ALSA_WodNumDevs = 0;

    h = ALSA_findSoundDevice(wwo->device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        
    if (h == NULL){
        ERR("could not find a PCM device to open\n");
        
        return -1;
    }
    
    ALSA_WodNumDevs++;

    EXIT_ON_ERROR( snd_pcm_info(h, info) , "pcm info" );

    TRACE("dev=%d id=%s name=%s subdev=%d subdev_name=%s subdev_avail=%d subdev_num=%d stream=%s subclass=%s \n",
       snd_pcm_info_get_device(info),
       snd_pcm_info_get_id(info),
       snd_pcm_info_get_name(info),
       snd_pcm_info_get_subdevice(info),
       snd_pcm_info_get_subdevice_name(info),
       snd_pcm_info_get_subdevices_avail(info),
       snd_pcm_info_get_subdevices_count(info),
       snd_pcm_stream_name(snd_pcm_info_get_stream(info)),
       (snd_pcm_info_get_subclass(info) == SND_PCM_SUBCLASS_GENERIC_MIX ? "GENERIC MIX": "MULTI MIX"));

    EXIT_ON_ERROR( snd_pcm_hw_params_any(h, hw_params) , "pcm hw params" );
#undef EXIT_ON_ERROR

    if (TRACE_ON(wave))
	ALSA_TraceParameters(hw_params, NULL, TRUE);

    {
	snd_pcm_format_mask_t *     fmask;
	int ratemin = snd_pcm_hw_params_get_rate_min(hw_params, 0);
	int ratemax = snd_pcm_hw_params_get_rate_max(hw_params, 0);
	int chmin = snd_pcm_hw_params_get_channels_min(hw_params); \
	int chmax = snd_pcm_hw_params_get_channels_max(hw_params); \

	snd_pcm_format_mask_alloca(&fmask);
	snd_pcm_hw_params_get_format_mask(hw_params, fmask);

#define X(r,v) \
       if ( (r) >= ratemin && ( (r) <= ratemax || ratemax == -1) ) \
       { \
          if (snd_pcm_format_mask_test( fmask, SND_PCM_FORMAT_U8)) \
          { \
              if (chmin <= 1 && 1 <= chmax) \
                  wwo->caps.dwFormats |= WAVE_FORMAT_##v##M08; \
              if (chmin <= 2 && 2 <= chmax) \
                  wwo->caps.dwFormats |= WAVE_FORMAT_##v##S08; \
          } \
          if (snd_pcm_format_mask_test( fmask, SND_PCM_FORMAT_S16_LE)) \
          { \
              if (chmin <= 1 && 1 <= chmax) \
                  wwo->caps.dwFormats |= WAVE_FORMAT_##v##M16; \
              if (chmin <= 2 && 2 <= chmax) \
                  wwo->caps.dwFormats |= WAVE_FORMAT_##v##S16; \
          } \
       }
       X(11025,1);
       X(22050,2);
       X(44100,4);
       X(48000,48);
       X(96000,96);
#undef X
    }

    if ((nChannels = snd_pcm_hw_params_get_channels_min (hw_params)) > 1)
        TRACE("channels_min on output is > 1. Value: %u\n", nChannels);
    wwo->caps.wChannels = (snd_pcm_hw_params_get_channels_max(hw_params) >= 2) ? 2 : 1;
    if (snd_pcm_hw_params_get_channels_min(hw_params) <= 2 && 2 <= snd_pcm_hw_params_get_channels_max(hw_params))
        wwo->caps.dwSupport |= WAVECAPS_LRVOLUME;

    /* FIXME: always true ? */
    wwo->caps.dwSupport |= WAVECAPS_SAMPLEACCURATE;

    {
	snd_pcm_access_mask_t *     acmask;
	snd_pcm_access_mask_alloca(&acmask);
	snd_pcm_hw_params_get_access_mask(hw_params, acmask);

	/* FIXME: NONITERLEAVED and COMPLEX are not supported right now */
	if ( snd_pcm_access_mask_test( acmask, SND_PCM_ACCESS_MMAP_INTERLEAVED ) && use_mmap )
            wwo->caps.dwSupport |= WAVECAPS_DIRECTSOUND;
    }

    TRACE("Configured with dwFmts=%08lx dwSupport=%08lx\n",
          wwo->caps.dwFormats, wwo->caps.dwSupport);

    snd_pcm_close(h);
    h = NULL;

    ALSA_InitializeVolumeCtl(wwo);

    /* Initialize the input capture device */
    /* FIXME: Support multiple cards */
    wwi = &WInDev[0];

    ALSA_GetInputDeviceName(wwi);

    wwi->state = WINE_WS_CLOSED;
    wwi->caps.wMid = 0x0002;
    wwi->caps.wPid = 0x0104;
    strcpy(wwi->caps.szPname, "SB16 Wave In");
    wwi->caps.vDriverVersion = 0x0100;
    wwi->caps.dwFormats = 0x00000000;
    wwi->caps.dwSupport = WAVECAPS_VOLUME;
    strcpy(wwi->ds_desc.szDesc, "WineALSA DirectSound Driver");
    strcpy(wwi->ds_desc.szDrvName, "winealsa.drv");
    wwi->ds_guid = DSDEVID_DefaultCapture;

    snd_pcm_info_alloca(&info);
    snd_pcm_hw_params_alloca(&hw_params);

#define EXIT_ON_ERROR(f,txt) do { int err; if ( (err = (f) ) < 0) { ERR(txt ": %s\n", snd_strerror(err)); if (h) snd_pcm_close(h); return -1; } } while(0)

    ALSA_WidNumDevs = 0;

    h = ALSA_findSoundDevice(wwi->device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
        
    if (h == NULL){
        ERR("could not find a PCM capture device to open\n");
        
        return -1;
    }

    ALSA_WidNumDevs++;


    EXIT_ON_ERROR( snd_pcm_info(h, info) , "pcm info" );

    TRACE("dev=%d id=%s name=%s subdev=%d subdev_name=%s subdev_avail=%d subdev_num=%d stream=%s subclass=%s \n",
       snd_pcm_info_get_device(info),
       snd_pcm_info_get_id(info),
       snd_pcm_info_get_name(info),
       snd_pcm_info_get_subdevice(info),
       snd_pcm_info_get_subdevice_name(info),
       snd_pcm_info_get_subdevices_avail(info),
       snd_pcm_info_get_subdevices_count(info),
       snd_pcm_stream_name(snd_pcm_info_get_stream(info)),
       (snd_pcm_info_get_subclass(info) == SND_PCM_SUBCLASS_GENERIC_MIX ? "GENERIC MIX": "MULTI MIX"));

    EXIT_ON_ERROR( snd_pcm_hw_params_any(h, hw_params) , "pcm hw params" );
#undef EXIT_ON_ERROR

    if (TRACE_ON(wave))
	ALSA_TraceParameters(hw_params, NULL, TRUE);

    {
	snd_pcm_format_mask_t *     fmask;
	int ratemin = snd_pcm_hw_params_get_rate_min(hw_params, 0);
	int ratemax = snd_pcm_hw_params_get_rate_max(hw_params, 0);
	int chmin = snd_pcm_hw_params_get_channels_min(hw_params); \
	int chmax = snd_pcm_hw_params_get_channels_max(hw_params); \

	snd_pcm_format_mask_alloca(&fmask);
	snd_pcm_hw_params_get_format_mask(hw_params, fmask);

#define X(r,v) \
       if ( (r) >= ratemin && ( (r) <= ratemax || ratemax == -1) ) \
       { \
          if (snd_pcm_format_mask_test( fmask, SND_PCM_FORMAT_U8)) \
          { \
              if (chmin <= 1 && 1 <= chmax) \
                  wwi->caps.dwFormats |= WAVE_FORMAT_##v##M08; \
              if (chmin <= 2 && 2 <= chmax) \
                  wwi->caps.dwFormats |= WAVE_FORMAT_##v##S08; \
          } \
          if (snd_pcm_format_mask_test( fmask, SND_PCM_FORMAT_S16_LE)) \
          { \
              if (chmin <= 1 && 1 <= chmax) \
                  wwi->caps.dwFormats |= WAVE_FORMAT_##v##M16; \
              if (chmin <= 2 && 2 <= chmax) \
                  wwi->caps.dwFormats |= WAVE_FORMAT_##v##S16; \
          } \
       }
       X(11025,1);
       X(22050,2);
       X(44100,4);
       X(48000,48);
       X(96000,96);
#undef X
    }

    if ((nChannels = snd_pcm_hw_params_get_channels_min (hw_params)) > 1)
        TRACE("channels_min on input is > 1. Value: %u\n", nChannels);
    wwi->caps.wChannels = (snd_pcm_hw_params_get_channels_max(hw_params) >= 2) ? 2 : 1;
    if (snd_pcm_hw_params_get_channels_min(hw_params) <= 2 && 2 <= snd_pcm_hw_params_get_channels_max(hw_params))
        wwi->caps.dwSupport |= WAVECAPS_LRVOLUME;

    /* FIXME: always true ? */
    wwi->caps.dwSupport |= WAVECAPS_SAMPLEACCURATE;

    {
	snd_pcm_access_mask_t *     acmask;
	snd_pcm_access_mask_alloca(&acmask);
	snd_pcm_hw_params_get_access_mask(hw_params, acmask);

	/* FIXME: NONITERLEAVED and COMPLEX are not supported right now */
	if ( snd_pcm_access_mask_test( acmask, SND_PCM_ACCESS_MMAP_INTERLEAVED ) )
            wwi->caps.dwSupport |= WAVECAPS_DIRECTSOUND;
    }

    TRACE("Configured with dwFmts=%08lx dwSupport=%08lx\n",
          wwi->caps.dwFormats, wwi->caps.dwSupport);

    snd_pcm_close(h);
    
    return 0;
}

/******************************************************************
 *		ALSA_InitRingMessage
 *
 * Initialize the ring of messages for passing between driver's caller and playback/record
 * thread
 */
static int ALSA_InitRingMessage(ALSA_MSG_RING* omr)
{
    omr->msg_toget = 0;
    omr->msg_tosave = 0;

#ifdef USE_PIPE_SYNC
    if (use_pipes) {
        if (pipe(omr->msg_pipe) < 0) {
            omr->msg_pipe[0] = -1;
            omr->msg_pipe[1] = -1;
            ERR("could not create pipe, error=%s\n", strerror(errno));
        }
    }
    else {
        omr->msg_pipe[0] = -1;
        omr->msg_pipe[1] = -1;
    }
#endif
    omr->msg_event = CreateEventA(NULL, FALSE, FALSE, NULL);

    omr->entries = ALSA_RING_BUFFER_SIZE;
    omr->messages = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, omr->entries * sizeof(ALSA_MSG));
    CRITICAL_SECTION_DEFINE(&omr->msg_crst);
    return 0;
}

/******************************************************************
 *		ALSA_DestroyRingMessage
 *
 */
static int ALSA_DestroyRingMessage(ALSA_MSG_RING* omr)
{
#ifdef USE_PIPE_SYNC
    if (use_pipes) {
        close(omr->msg_pipe[0]);
        close(omr->msg_pipe[1]);
    }
#endif
    CloseHandle(omr->msg_event);
    DeleteCriticalSection(&omr->msg_crst);
    return 0;
}

/******************************************************************
 *		ALSA_AddRingMessage
 *
 * Inserts a new message into the ring (should be called from DriverProc derivated routines)
 */
static int ALSA_AddRingMessage(ALSA_MSG_RING* omr, enum win_wm_message msg, DWORD param, BOOL wait)
{
    HANDLE	hEvent = INVALID_HANDLE_VALUE;

    EnterCriticalSection(&omr->msg_crst);
    if ((omr->msg_toget == ((omr->msg_tosave + 1) % omr->entries))) /* buffer overflow ? */
    {
        omr->entries += ALSA_RING_BUFFER_SIZE;
        omr->messages = HeapReAlloc(GetProcessHeap(), 0, omr->messages, omr->entries * sizeof(ALSA_MSG));
        TRACE("Growing audio ring buffer to %d\n", omr->entries);
    }
    if (wait)
    {
        hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (hEvent == INVALID_HANDLE_VALUE)
        {
            ERR("can't create event !?\n");
            LeaveCriticalSection(&omr->msg_crst);
            return 0;
        }
        if (omr->msg_toget != omr->msg_tosave && omr->messages[omr->msg_toget].msg != WINE_WM_HEADER)
            FIXME("two fast messages in the queue!!!!\n");

        /* fast messages have to be added at the start of the queue */
        omr->msg_toget = (omr->msg_toget + omr->entries - 1) % omr->entries;

        omr->messages[omr->msg_toget].msg = msg;
        omr->messages[omr->msg_toget].param = param;
        omr->messages[omr->msg_toget].hEvent = hEvent;
    }
    else
    {
        omr->messages[omr->msg_tosave].msg = msg;
        omr->messages[omr->msg_tosave].param = param;
        omr->messages[omr->msg_tosave].hEvent = INVALID_HANDLE_VALUE;
        omr->msg_tosave = (omr->msg_tosave + 1) % omr->entries;
    }
    LeaveCriticalSection(&omr->msg_crst);
    /* signal a new message */
    SIGNAL_OMR(omr);
    if (wait)
    {
        /* wait for playback/record thread to have processed the message */
        WaitForSingleObject(hEvent, INFINITE);
        CloseHandle(hEvent);
    }
    return 1;
}

/******************************************************************
 *		ALSA_RetrieveRingMessage
 *
 * Get a message from the ring. Should be called by the playback/record thread.
 */
static int ALSA_RetrieveRingMessage(ALSA_MSG_RING* omr,
                                   enum win_wm_message *msg, DWORD *param, HANDLE *hEvent)
{
    EnterCriticalSection(&omr->msg_crst);

    if (omr->msg_toget == omr->msg_tosave) /* buffer empty ? */
    {
        LeaveCriticalSection(&omr->msg_crst);
	return 0;
    }

    *msg = omr->messages[omr->msg_toget].msg;
    omr->messages[omr->msg_toget].msg = 0;
    *param = omr->messages[omr->msg_toget].param;
    *hEvent = omr->messages[omr->msg_toget].hEvent;
    omr->msg_toget = (omr->msg_toget + 1) % omr->entries;
    CLEAR_OMR(omr);
    LeaveCriticalSection(&omr->msg_crst);
    return 1;
}

/******************************************************************
 *              ALSA_PeekRingMessage
 *
 * Peek at a message from the ring but do not remove it.
 * Should be called by the playback/record thread.
 */
static int ALSA_PeekRingMessage(ALSA_MSG_RING* omr,
                               enum win_wm_message *msg,
                               DWORD *param, HANDLE *hEvent)
{
    EnterCriticalSection(&omr->msg_crst);

    if (omr->msg_toget == omr->msg_tosave) /* buffer empty ? */
    {
	LeaveCriticalSection(&omr->msg_crst);
	return 0;
    }

    *msg = omr->messages[omr->msg_toget].msg;
    *param = omr->messages[omr->msg_toget].param;
    *hEvent = omr->messages[omr->msg_toget].hEvent;
    LeaveCriticalSection(&omr->msg_crst);
    return 1;
}

/*======================================================================*
 *                  Low level WAVE OUT implementation			*
 *======================================================================*/

/**************************************************************************
 * 			wodNotifyClient			[internal]
 */
static DWORD wodNotifyClient(WINE_WAVEOUT* wwo, WORD wMsg, DWORD dwParam1, DWORD dwParam2)
{
    TRACE("wMsg = 0x%04x dwParm1 = %04lX dwParam2 = %04lX\n", wMsg, dwParam1, dwParam2);

    switch (wMsg) {
    case WOM_OPEN:
    case WOM_CLOSE:
    case WOM_DONE:
	if (wwo->wFlags != DCB_NULL &&
	    !DriverCallback(wwo->waveDesc.dwCallback, wwo->wFlags, wwo->waveDesc.hWave,
			    wMsg, wwo->waveDesc.dwInstance, dwParam1, dwParam2)) {
	    WARN("can't notify client !\n");
	    return MMSYSERR_ERROR;
	}
	break;
    default:
	FIXME("Unknown callback message %u\n", wMsg);
        return MMSYSERR_INVALPARAM;
    }
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodUpdatePlayedTotal	[internal]
 *
 */
static BOOL wodUpdatePlayedTotal(WINE_WAVEOUT* wwo, snd_pcm_status_t* ps)
{
   snd_pcm_sframes_t delay = 0;
   snd_pcm_delay(wwo->p_handle, &delay);
   /* 'delay' is gibberish unless state is running. See:
      http://www.mail-archive.com/alsa-devel@lists.sourceforge.net/msg12704.html */
   if (snd_pcm_state (wwo->p_handle) != SND_PCM_STATE_RUNNING)
       delay = 0;

   wwo->dwPlayedTotal = wwo->dwWrittenTotal - snd_pcm_frames_to_bytes(wwo->p_handle, delay);
   return TRUE;
}

/**************************************************************************
 * 				wodPlayer_BeginWaveHdr          [internal]
 *
 * Makes the specified lpWaveHdr the currently playing wave header.
 * If the specified wave header is a begin loop and we're not already in
 * a loop, setup the loop.
 */
static void wodPlayer_BeginWaveHdr(WINE_WAVEOUT* wwo, LPWAVEHDR lpWaveHdr)
{
    wwo->lpPlayPtr = lpWaveHdr;

    if (!lpWaveHdr) return;

    if (lpWaveHdr->dwFlags & WHDR_BEGINLOOP) {
	if (wwo->lpLoopPtr) {
	    WARN("Already in a loop. Discarding loop on this header (%p)\n", lpWaveHdr);
	} else {
	    TRACE("Starting loop (%ldx) with %p\n", lpWaveHdr->dwLoops, lpWaveHdr);
	    wwo->lpLoopPtr = lpWaveHdr;
	    /* Windows does not touch WAVEHDR.dwLoops,
	     * so we need to make an internal copy */
	    wwo->dwLoops = lpWaveHdr->dwLoops;
	}
    }
    wwo->dwPartialOffset = 0;
}

/**************************************************************************
 * 				wodPlayer_PlayPtrNext	        [internal]
 *
 * Advance the play pointer to the next waveheader, looping if required.
 */
static LPWAVEHDR wodPlayer_PlayPtrNext(WINE_WAVEOUT* wwo)
{
    LPWAVEHDR lpWaveHdr = wwo->lpPlayPtr;

    wwo->dwPartialOffset = 0;
    if ((lpWaveHdr->dwFlags & WHDR_ENDLOOP) && wwo->lpLoopPtr) {
	/* We're at the end of a loop, loop if required */
	if (--wwo->dwLoops > 0) {
	    wwo->lpPlayPtr = wwo->lpLoopPtr;
	} else {
	    /* Handle overlapping loops correctly */
	    if (wwo->lpLoopPtr != lpWaveHdr && (lpWaveHdr->dwFlags & WHDR_BEGINLOOP)) {
		FIXME("Correctly handled case ? (ending loop buffer also starts a new loop)\n");
		/* shall we consider the END flag for the closing loop or for
		 * the opening one or for both ???
		 * code assumes for closing loop only
		 */
	    } else {
                lpWaveHdr = lpWaveHdr->lpNext;
            }
            wwo->lpLoopPtr = NULL;
            wodPlayer_BeginWaveHdr(wwo, lpWaveHdr);
	}
    } else {
	/* We're not in a loop.  Advance to the next wave header */
	wodPlayer_BeginWaveHdr(wwo, lpWaveHdr = lpWaveHdr->lpNext);
    }

    return lpWaveHdr;
}

/**************************************************************************
 * 			     wodPlayer_DSPWait			[internal]
 * Returns the number of milliseconds to wait for the DSP buffer to play a
 * period
 */
static DWORD wodPlayer_DSPWait(const WINE_WAVEOUT *wwo)
{
    /* time for one period to be played */
    return snd_pcm_hw_params_get_period_time(wwo->hw_params, 0) / 1000;
}

/**************************************************************************
 * 			     wodPlayer_NotifyWait               [internal]
 * Returns the number of milliseconds to wait before attempting to notify
 * completion of the specified wavehdr.
 * This is based on the number of bytes remaining to be written in the
 * wave.
 */
static DWORD wodPlayer_NotifyWait(const WINE_WAVEOUT* wwo, LPWAVEHDR lpWaveHdr)
{
    DWORD dwMillis;

    if (lpWaveHdr->reserved < wwo->dwPlayedTotal) {
        dwMillis = 1;
    } else {
        dwMillis = (lpWaveHdr->reserved - wwo->dwPlayedTotal) * 1000 / wwo->format.Format.nAvgBytesPerSec;
        if (!dwMillis) dwMillis = 1;
    }

    return dwMillis;
}


/**************************************************************************
 * 			     wodPlayer_WriteMaxFrags            [internal]
 * Writes the maximum number of frames possible to the DSP and returns
 * the number of frames written.
 */
static int wodPlayer_WriteMaxFrags(WINE_WAVEOUT* wwo, DWORD* frames)
{
    /* Only attempt to write to free frames */
    LPWAVEHDR lpWaveHdr = wwo->lpPlayPtr;
    DWORD dwLength = snd_pcm_bytes_to_frames(wwo->p_handle, lpWaveHdr->dwBufferLength - wwo->dwPartialOffset);
    int toWrite = min(dwLength, *frames);
    int written;

    TRACE("Writing wavehdr %p.%lu[%lu]\n", lpWaveHdr, wwo->dwPartialOffset, lpWaveHdr->dwBufferLength);

    written = (wwo->write)(wwo->p_handle, lpWaveHdr->lpData + wwo->dwPartialOffset, toWrite);
    if ( written < 0)
    {
    	/* XRUN occurred. let's try to recover */
        ALSA_XRUNRecovery(wwo->p_handle, written);
	written = (wwo->write)(wwo->p_handle, lpWaveHdr->lpData + wwo->dwPartialOffset, toWrite);
    }
    if (written <= 0)
    {
        /* still in error */
        ERR("Error in writing wavehdr. Reason: %s\n", snd_strerror(written));
        return written;
    }

    wwo->dwPartialOffset += snd_pcm_frames_to_bytes(wwo->p_handle, written);
    if ( wwo->dwPartialOffset >= lpWaveHdr->dwBufferLength) {
	/* this will be used to check if the given wave header has been fully played or not... */
        wwo->dwPartialOffset = lpWaveHdr->dwBufferLength;
        /* If we wrote all current wavehdr, skip to the next one */
        wodPlayer_PlayPtrNext(wwo);
    }
    *frames -= written;
    wwo->dwWrittenTotal += snd_pcm_frames_to_bytes(wwo->p_handle, written);

    return written;
}


/**************************************************************************
 * 				wodPlayer_NotifyCompletions	[internal]
 *
 * Notifies and remove from queue all wavehdrs which have been played to
 * the speaker (ie. they have cleared the ALSA buffer).  If force is true,
 * we notify all wavehdrs and remove them all from the queue even if they
 * are unplayed or part of a loop.
 */
static DWORD wodPlayer_NotifyCompletions(WINE_WAVEOUT* wwo, BOOL force)
{
    LPWAVEHDR		lpWaveHdr;

    /* Start from lpQueuePtr and keep notifying until:
     * - we hit an unwritten wavehdr
     * - we hit the beginning of a running loop
     * - we hit a wavehdr which hasn't finished playing
     */
#if 0
    while ((lpWaveHdr = wwo->lpQueuePtr) &&
           (force ||
            (lpWaveHdr != wwo->lpPlayPtr &&
             lpWaveHdr != wwo->lpLoopPtr &&
             lpWaveHdr->reserved <= wwo->dwPlayedTotal))) {

	wwo->lpQueuePtr = lpWaveHdr->lpNext;

	lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
	lpWaveHdr->dwFlags |= WHDR_DONE;

	wodNotifyClient(wwo, WOM_DONE, (DWORD)lpWaveHdr, 0);
    }
#else
    for (;;)
    {
        lpWaveHdr = wwo->lpQueuePtr;
        if (!lpWaveHdr) {TRACE("Empty queue\n"); break;}
        if (!force)
        {
            if (lpWaveHdr == wwo->lpPlayPtr) {TRACE("play %p\n", lpWaveHdr); break;}
            if (lpWaveHdr == wwo->lpLoopPtr) {TRACE("loop %p\n", lpWaveHdr); break;}
            if (lpWaveHdr->reserved > wwo->dwPlayedTotal){TRACE("still playing %p (%lu/%lu)\n", lpWaveHdr, lpWaveHdr->reserved, wwo->dwPlayedTotal);break;}
        }
	wwo->lpQueuePtr = lpWaveHdr->lpNext;

	lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
	lpWaveHdr->dwFlags |= WHDR_DONE;

	wodNotifyClient(wwo, WOM_DONE, (DWORD)lpWaveHdr, 0);
    }
#endif
    return  (lpWaveHdr && lpWaveHdr != wwo->lpPlayPtr && lpWaveHdr != wwo->lpLoopPtr) ?
        wodPlayer_NotifyWait(wwo, lpWaveHdr) : INFINITE;
}

/**************************************************************************
 * 				wodPlayer_Reset			[internal]
 *
 * wodPlayer helper. Resets current output stream.
 */
static	void	wodPlayer_Reset(WINE_WAVEOUT* wwo)
{
    enum win_wm_message	msg;
    DWORD		        param;
    HANDLE		        ev;
    int                         err;

    wodUpdatePlayedTotal(wwo, NULL);
    /* updates current notify list */
    wodPlayer_NotifyCompletions(wwo, FALSE);

    if ( (err = snd_pcm_drop(wwo->p_handle)) < 0) {
	FIXME("flush: %s\n", snd_strerror(err));
	wwo->hThread = 0;
	wwo->state = WINE_WS_STOPPED;
	ExitThread(-1);
    }
    if ( (err = snd_pcm_prepare(wwo->p_handle)) < 0 )
        ERR("pcm prepare failed: %s\n", snd_strerror(err));

    /* remove any buffer */
    wodPlayer_NotifyCompletions(wwo, TRUE);

    wwo->lpPlayPtr = wwo->lpQueuePtr = wwo->lpLoopPtr = NULL;
    wwo->state = WINE_WS_STOPPED;
    wwo->dwPlayedTotal = wwo->dwWrittenTotal = 0;
    /* Clear partial wavehdr */
    wwo->dwPartialOffset = 0;

    /* remove any existing message in the ring */
    EnterCriticalSection(&wwo->msgRing.msg_crst);
    /* return all pending headers in queue */
    while (ALSA_RetrieveRingMessage(&wwo->msgRing, &msg, &param, &ev))
    {
        if (msg != WINE_WM_HEADER)
        {
            FIXME("shouldn't have headers left\n");
            SetEvent(ev);
            continue;
        }
        ((LPWAVEHDR)param)->dwFlags &= ~WHDR_INQUEUE;
        ((LPWAVEHDR)param)->dwFlags |= WHDR_DONE;

        wodNotifyClient(wwo, WOM_DONE, param, 0);
    }
    RESET_OMR(&wwo->msgRing);
    LeaveCriticalSection(&wwo->msgRing.msg_crst);
}

/**************************************************************************
 * 		      wodPlayer_ProcessMessages			[internal]
 */
static void wodPlayer_ProcessMessages(WINE_WAVEOUT* wwo)
{
    LPWAVEHDR           lpWaveHdr;
    enum win_wm_message	msg;
    DWORD		param;
    HANDLE		ev;
    int                 err;

    while (ALSA_RetrieveRingMessage(&wwo->msgRing, &msg, &param, &ev)) {
     TRACE("Received %s %lx\n", wodPlayerCmdString[msg - WM_USER - 1], param); 

	switch (msg) {
	case WINE_WM_PAUSING:
	    if ( snd_pcm_state(wwo->p_handle) == SND_PCM_STATE_RUNNING )
	     {
		err = snd_pcm_pause(wwo->p_handle, 1);
		if ( err < 0 )
		    ERR("pcm_pause failed: %s\n", snd_strerror(err));
	     }
	    wwo->state = WINE_WS_PAUSED;
	    SetEvent(ev);
	    break;
	case WINE_WM_RESTARTING:
            if (wwo->state == WINE_WS_PAUSED)
            {
		if ( snd_pcm_state(wwo->p_handle) == SND_PCM_STATE_PAUSED )
		 {
		    err = snd_pcm_pause(wwo->p_handle, 0);
		    if ( err < 0 )
		        ERR("pcm_pause failed: %s\n", snd_strerror(err));
		 }
                wwo->state = WINE_WS_PLAYING;
            }
	    SetEvent(ev);
	    break;
	case WINE_WM_HEADER:
	    lpWaveHdr = (LPWAVEHDR)param;

	    /* insert buffer at the end of queue */
	    {
		LPWAVEHDR*	wh;
		for (wh = &(wwo->lpQueuePtr); *wh; wh = &((*wh)->lpNext));
		*wh = lpWaveHdr;
	    }
            if (!wwo->lpPlayPtr)
                wodPlayer_BeginWaveHdr(wwo,lpWaveHdr);
	    if (wwo->state == WINE_WS_STOPPED)
		wwo->state = WINE_WS_PLAYING;
	    break;
	case WINE_WM_RESETTING:
	    wodPlayer_Reset(wwo);
	    SetEvent(ev);
	    break;
        case WINE_WM_UPDATE:
            wodUpdatePlayedTotal(wwo, NULL);
	    SetEvent(ev);
            break;
        case WINE_WM_BREAKLOOP:
            if (wwo->state == WINE_WS_PLAYING && wwo->lpLoopPtr != NULL) {
                /* ensure exit at end of current loop */
                wwo->dwLoops = 1;
            }
	    SetEvent(ev);
            break;
	case WINE_WM_CLOSING:
	    /* sanity check: this should not happen since the device must have been reset before */
	    if (wwo->lpQueuePtr || wwo->lpPlayPtr) ERR("out of sync\n");
	    wwo->hThread = 0;
	    wwo->state = WINE_WS_CLOSED;
	    SetEvent(ev);
	    ExitThread(0);
	    /* shouldn't go here */
	default:
	    FIXME("unknown message %d\n", msg);
	    break;
	}
    }
}

/**************************************************************************
 * 			     wodPlayer_FeedDSP			[internal]
 * Feed as much sound data as we can into the DSP and return the number of
 * milliseconds before it will be necessary to feed the DSP again.
 */
static DWORD wodPlayer_FeedDSP(WINE_WAVEOUT* wwo)
{
    DWORD               availInQ;

    wodUpdatePlayedTotal(wwo, NULL);
    availInQ = snd_pcm_avail_update(wwo->p_handle);

#if 0
    /* input queue empty and output buffer with less than one fragment to play */
    if (!wwo->lpPlayPtr && wwo->dwBufferSize < availInQ + wwo->dwFragmentSize) {
	TRACE("Run out of wavehdr:s...\n");
        return INFINITE;
    }
#endif

    /* no more room... no need to try to feed */
    if (availInQ > 0) {
        /* Feed from partial wavehdr */
        if (wwo->lpPlayPtr && wwo->dwPartialOffset != 0) {
            wodPlayer_WriteMaxFrags(wwo, &availInQ);
        }

        /* Feed wavehdrs until we run out of wavehdrs or DSP space */
        if (wwo->dwPartialOffset == 0 && wwo->lpPlayPtr) {
            do {
                TRACE("Setting time to elapse for %p to %lu\n",
                      wwo->lpPlayPtr, wwo->dwWrittenTotal + wwo->lpPlayPtr->dwBufferLength);
                /* note the value that dwPlayedTotal will return when this wave finishes playing */
                wwo->lpPlayPtr->reserved = wwo->dwWrittenTotal + wwo->lpPlayPtr->dwBufferLength;
            } while (wodPlayer_WriteMaxFrags(wwo, &availInQ) && wwo->lpPlayPtr && availInQ > 0);
        }
    }

    return wodPlayer_DSPWait(wwo);
}

/**************************************************************************
 * 				wodPlayer			[internal]
 */
static	DWORD	CALLBACK	wodPlayer(LPVOID pmt)
{
    WORD	  uDevID = (DWORD)pmt;
    WINE_WAVEOUT* wwo = (WINE_WAVEOUT*)&WOutDev[uDevID];
    DWORD         dwNextFeedTime = INFINITE;   /* Time before DSP needs feeding */
    DWORD         dwNextNotifyTime = INFINITE; /* Time before next wave completion */
    DWORD         dwSleepTime;

    wwo->state = WINE_WS_STOPPED;
    SetEvent(wwo->hStartUpEvent);

    for (;;) {
        /** Wait for the shortest time before an action is required.  If there
         *  are no pending actions, wait forever for a command.
         */
        dwSleepTime = min(dwNextFeedTime, dwNextNotifyTime);
        TRACE("waiting %lums (%lu,%lu)\n", dwSleepTime, dwNextFeedTime, dwNextNotifyTime);
        WAIT_OMR(&wwo->msgRing, dwSleepTime);
	wodPlayer_ProcessMessages(wwo);
	if (wwo->state == WINE_WS_PLAYING) {
	    dwNextFeedTime = wodPlayer_FeedDSP(wwo);
	    dwNextNotifyTime = wodPlayer_NotifyCompletions(wwo, FALSE);
	    if (dwNextFeedTime == INFINITE) {
		/* FeedDSP ran out of data, but before giving up, */
		/* check that a notification didn't give us more */
		wodPlayer_ProcessMessages(wwo);
		if (wwo->lpPlayPtr) {
		    TRACE("recovering\n");
		    dwNextFeedTime = wodPlayer_FeedDSP(wwo);
		}
	    }
	} else {
	    dwNextFeedTime = dwNextNotifyTime = INFINITE;
	}
    }
}

/**************************************************************************
 * 			wodGetDevCaps				[internal]
 */
static DWORD wodGetDevCaps(WORD wDevID, LPWAVEOUTCAPSA lpCaps, DWORD dwSize)
{
    TRACE("(%u, %p, %lu);\n", wDevID, lpCaps, dwSize);

    if (lpCaps == NULL) return MMSYSERR_NOTENABLED;

    if (wDevID >= MAX_WAVEOUTDRV) {
	TRACE("MAX_WAVOUTDRV reached !\n");
	return MMSYSERR_BADDEVICEID;
    }

    memcpy(lpCaps, &WOutDev[wDevID].caps, min(dwSize, sizeof(*lpCaps)));
    return MMSYSERR_NOERROR;
}

#define NUM_SAMPLE_RATES 5

static const int sample_rates [ NUM_SAMPLE_RATES ] = { 96000, 48000, 44100, 22050, 11025 };

/**************************************************************************
 *                     ConfigureCaptureAndOpenDevice            [internal]
 *
 * Attempts to configure the (possibly already-running) capture thread
 * on the given device to use the audio parameters contained in the
 * wwi structure by inserting an ACM.
 * Note that this can be called either by the setup for the capture
 * thread, or by another thread while the capture thread is already running
 *
 * Preconditions:
 *    - desired capture parameters are set in WInDev[wDevID]
 */
static int ConfigureCaptureAndOpenDevice (WORD wDevID, int givenSampleRate)
{
    WINE_WAVEIN *wwi = &WInDev[wDevID];
    WAVEFORMATEX format, tmp_format;
    int bestRate = -1;
    int lowestDeviation = 0;
    int i, ret = WAVERR_BADFORMAT;
    HACMSTREAM hConv = 0;

     /* find closest standard sample rate to real sample rate,
     * since ACM only accepts such standard rates;
     * we currently define "closest" as lowest percent-wise
     * deviation from standard rate */
    for (i = 0; i < NUM_SAMPLE_RATES; i++)
    {
        int currentDeviation = (givenSampleRate - sample_rates[i]) * 100;
        if (currentDeviation < 0)
            currentDeviation = -currentDeviation;
        currentDeviation /= sample_rates[i];
        if ((bestRate == -1) || (currentDeviation < lowestDeviation))
        {
            bestRate = i;
            lowestDeviation = currentDeviation;
        }
    }

    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = wwi->format.Format.nChannels;
    format.nSamplesPerSec = sample_rates[bestRate];
    format.wBitsPerSample = wwi->format.Format.wBitsPerSample;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
    format.cbSize = 0;
    TRACE ("Output: %ld/%d/%d\n", format.nSamplesPerSec, format.wBitsPerSample,
           format.nChannels);
    TRACE ("Capture: %ld/%d/%d\n", wwi->format.Format.nSamplesPerSec,
           wwi->format.Format.wBitsPerSample, wwi->format.Format.nChannels);

    /* If actual rate is "worse" than requested capture format, return error */
    if (format.nSamplesPerSec < wwi->format.Format.nSamplesPerSec)
        goto error;

    /* FIXME: hack - we should properly support WAVE_FORMAT_EXTENSIBLE in all
     * of its permutations */
    memcpy(&tmp_format, &wwi->format, sizeof(WAVEFORMATEX));
    if (tmp_format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        FIXME("Blindly accepting WAVE_FORMAT_EXTENSIBLE as PCM\n");
        tmp_format.wFormatTag = WAVE_FORMAT_PCM;
    }

    /* Create ACM between output format and capture format */
    ret = MMSYSERR_ALLOCATED;
    if (acmStreamOpen (&hConv, 0, &format, &tmp_format,
                       NULL, 0, 0, 0) != MMSYSERR_NOERROR)
        goto error;

    /* Set ACM device */
    if (wwi->hConv)
        acmStreamClose (wwi->hConv, 0);
    wwi->hConv = hConv;

    return MMSYSERR_NOERROR;

 error:
    if (hConv)
        acmStreamClose (hConv, 0);

    ERR ("Unable to reopen device with new audio format!\n");

    return WAVERR_BADFORMAT;
}

static BOOL is_format_supported(LPWAVEFORMATEX lpFormat)
{
    if (lpFormat->wFormatTag == WAVE_FORMAT_PCM &&
	lpFormat->nChannels != 0 &&
	lpFormat->nSamplesPerSec != 0)
        return TRUE;
    if (lpFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        lpFormat->nChannels != 0 &&
        lpFormat->nSamplesPerSec != 0) {
        WAVEFORMATEXTENSIBLE* wfExt = (WAVEFORMATEXTENSIBLE*)lpFormat;
        FIXME("Blindly accepting WAVE_FORMAT_EXTENSIBLE: GUID: %s\n", debugstr_guid(&wfExt->SubFormat));
        return TRUE;
    }
    WARN("Bad format: tag=%04X nChannels=%d nSamplesPerSec=%ld !\n",
         lpFormat->wFormatTag, lpFormat->nChannels,
         lpFormat->nSamplesPerSec);
    return FALSE;
}

static void copy_wave_format(LPWAVEFORMATEX dst, const LPWAVEFORMATEX src)
{
    if (src->wFormatTag == WAVE_FORMAT_PCM) {
        memset(dst, 0, sizeof(WAVEFORMATEX));
        memcpy(dst, src, sizeof(PCMWAVEFORMAT));
    } else if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        memcpy(dst, src, sizeof(WAVEFORMATPCMEX));
    } else {
        FIXME("unsupported format: %x\n", src->wFormatTag);
    }
}

/**************************************************************************
 * 				wodOpen				[internal]
 */
static DWORD wodOpen(WORD wDevID, LPWAVEOPENDESC lpDesc, DWORD dwFlags)
{
    WINE_WAVEOUT*	        wwo;
    int                         dsound;
    snd_pcm_hw_params_t *       hw_params;
    snd_pcm_sw_params_t *       sw_params;
    snd_pcm_access_t            access;
    snd_pcm_format_t            format;
    int                         rate;
    unsigned int                buffer_time = 500000;
    unsigned int                period_time = 10000;
    int                         buffer_size;
    snd_pcm_uframes_t           period_size;
    int                         flags;
    snd_pcm_t *                 pcm;
    int                         err;

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_sw_params_alloca(&sw_params);

    TRACE("(%u, %p, %08lX);\n", wDevID, lpDesc, dwFlags);
    if (lpDesc == NULL) {
	WARN("Invalid Parameter !\n");
	return MMSYSERR_INVALPARAM;
    }
    if (wDevID >= MAX_WAVEOUTDRV) {
	TRACE("MAX_WAVOUTDRV reached !\n");
	return MMSYSERR_BADDEVICEID;
    }

    /* only PCM format is supported so far... */
    if (!is_format_supported(lpDesc->lpFormat))
	return WAVERR_BADFORMAT;

    if (dwFlags & WAVE_FORMAT_QUERY) {
	TRACE("Query format: tag=%04X nChannels=%d nSamplesPerSec=%ld !\n",
	     lpDesc->lpFormat->wFormatTag, lpDesc->lpFormat->nChannels,
	     lpDesc->lpFormat->nSamplesPerSec);
	return MMSYSERR_NOERROR;
    }

    wwo = &WOutDev[wDevID];

    dsound = (dwFlags & WAVE_DIRECTSOUND) && (wwo->caps.dwSupport & WAVECAPS_DIRECTSOUND);

    if (dwFlags & WAVE_REOPEN)
    {
        /* need to reopen the device to allow the format to be changed. First,
           unmap the device if needed then close the device */
        snd_pcm_hw_params_free(wwo->hw_params);
        wwo->hw_params = NULL;

        snd_pcm_close(wwo->p_handle);
        wwo->p_handle = NULL;
        wwo->state = WINE_WS_CLOSED;
    }

    if (wwo->p_handle) return MMSYSERR_ALLOCATED;

    wwo->p_handle = 0;
    flags = SND_PCM_NONBLOCK;
    if (dsound)
    	flags |= SND_PCM_ASYNC;

    pcm = ALSA_findSoundDevice(wwo->device, SND_PCM_STREAM_PLAYBACK, flags);
    
    if (pcm == NULL)
    {
        ERR("Error open: %s\n", snd_strerror(errno));
	return MMSYSERR_NOTENABLED;
    }

    wwo->wFlags = HIWORD(dwFlags & CALLBACK_TYPEMASK);

    memcpy(&wwo->waveDesc, lpDesc, sizeof(WAVEOPENDESC));
    copy_wave_format(&wwo->format.Format, lpDesc->lpFormat);

    if (wwo->format.Format.wBitsPerSample == 0) {
	WARN("Resetting zeroed wBitsPerSample\n");
	wwo->format.Format.wBitsPerSample = 8 *
	    (wwo->format.Format.nAvgBytesPerSec /
	     wwo->format.Format.nSamplesPerSec) /
	    wwo->format.Format.nChannels;
    }

    snd_pcm_hw_params_any(pcm, hw_params);

#define EXIT_ON_ERROR(f,e,txt) do \
{ \
    int err; \
    if ( (err = (f) ) < 0) \
    { \
	ERR(txt ": %s\n", snd_strerror(err)); \
	snd_pcm_close(pcm); \
	return e; \
    } \
} while(0)

    access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
    if ( ( err = snd_pcm_hw_params_set_access(pcm, hw_params, access ) ) < 0) {
        WARN("mmap not available. switching to standard write.\n");
        access = SND_PCM_ACCESS_RW_INTERLEAVED;
	EXIT_ON_ERROR( snd_pcm_hw_params_set_access(pcm, hw_params, access ), MMSYSERR_INVALPARAM, "unable to set access for playback");
	wwo->write = snd_pcm_writei;
    }
    else
	wwo->write = snd_pcm_mmap_writei;

    if (dwFlags & WAVE_DIRECTSOUND) {
	/* change format to something the sound card likes if necessary. */
	snd_pcm_format_mask_t *fmask;
	int chmin = snd_pcm_hw_params_get_channels_min(hw_params); \
	int chmax = snd_pcm_hw_params_get_channels_max(hw_params); \

	snd_pcm_format_mask_alloca(&fmask);
	snd_pcm_hw_params_get_format_mask(hw_params, fmask);

	if (wwo->format.Format.nChannels < chmin)
	    wwo->format.Format.nChannels = chmin;
	if (wwo->format.Format.nChannels > chmax)
	    wwo->format.Format.nChannels = chmax;
	if (wwo->format.Format.wBitsPerSample == 8 &&
	    !snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8) &&
	    snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16_LE))
	    wwo->format.Format.wBitsPerSample = 16;
	if (wwo->format.Format.wBitsPerSample == 16 &&
	    !snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16_LE) &&
	    snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8))
	    wwo->format.Format.wBitsPerSample = 8;

	/* we want to let dsound do the resampling */
	if (psnd_pcm_hw_params_set_rate_resample)
	    (*psnd_pcm_hw_params_set_rate_resample)(pcm, hw_params, 0);
    }

    EXIT_ON_ERROR( snd_pcm_hw_params_set_channels(pcm, hw_params, wwo->format.Format.nChannels), MMSYSERR_INVALPARAM, "unable to set required channels");

    format = (wwo->format.Format.wBitsPerSample == 16) ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8;
    EXIT_ON_ERROR( snd_pcm_hw_params_set_format(pcm, hw_params, format), MMSYSERR_INVALPARAM, "unable to set required format");

#if 0
    EXIT_ON_ERROR( snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, buffer_size), MMSYSERR_NOMEM, "unable to get required buffer");
    buffer_size = snd_pcm_hw_params_get_buffer_size(hw_params);

    EXIT_ON_ERROR( snd_pcm_hw_params_set_period_size_near(pcm, hw_params, buffer_size/num_periods, 0), MMSYSERR_ERROR, "unable to set required period size");
    period_size = snd_pcm_hw_params_get_period_size(hw_params, 0);

    TRACE("buffer_size=%d, period_size=%ld\n", buffer_size, period_size);
#endif

    rate = snd_pcm_hw_params_set_rate_near(pcm, hw_params, wwo->format.Format.nSamplesPerSec, 0);
    if (rate < 0) {
	ERR("Rate %ld Hz not available for playback: %s\n", wwo->format.Format.nSamplesPerSec, snd_strerror(rate));
	snd_pcm_close(pcm);
        return WAVERR_BADFORMAT;
    }
    if ((!NEAR_MATCH(wwo->format.Format.nSamplesPerSec, rate)) && !(dwFlags & WAVE_DIRECTSOUND)) {
	ERR("Rate doesn't match (requested %ld Hz, got %d Hz)\n", wwo->format.Format.nSamplesPerSec, rate);
	snd_pcm_close(pcm);
        return WAVERR_BADFORMAT;
    }
    
    EXIT_ON_ERROR( snd_pcm_hw_params_set_buffer_time_near(pcm, hw_params, buffer_time, 0), MMSYSERR_INVALPARAM, "unable to set buffer time");
    EXIT_ON_ERROR( snd_pcm_hw_params_set_period_time_near(pcm, hw_params, period_time, 0), MMSYSERR_INVALPARAM, "unable to set period time");

    EXIT_ON_ERROR( snd_pcm_hw_params(pcm, hw_params), MMSYSERR_INVALPARAM, "unable to set hw params for playback");
    
    period_size = snd_pcm_hw_params_get_period_size(hw_params, 0);
    buffer_size = snd_pcm_hw_params_get_buffer_size(hw_params);

    snd_pcm_sw_params_current(pcm, sw_params);
    EXIT_ON_ERROR( snd_pcm_sw_params_set_start_threshold(pcm, sw_params, dsound ? MAXINT : 1 ), MMSYSERR_ERROR, "unable to set start threshold");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_silence_size(pcm, sw_params, 0), MMSYSERR_ERROR, "unable to set silence size");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size), MMSYSERR_ERROR, "unable to set avail min");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_xfer_align(pcm, sw_params, 1), MMSYSERR_ERROR, "unable to set xfer align");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_silence_threshold(pcm, sw_params, 0), MMSYSERR_ERROR, "unable to set silence threshold");
    if (dsound) {
        /* no ALSA xruns in dsound mode, we'll detect them ourselves */
        EXIT_ON_ERROR( snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, MAXINT), MMSYSERR_ERROR, "unable to set stop threshold");
    }
    EXIT_ON_ERROR( snd_pcm_sw_params(pcm, sw_params), MMSYSERR_ERROR, "unable to set sw params for playback");
#undef EXIT_ON_ERROR

    snd_pcm_prepare(pcm);

    if (TRACE_ON(wave))
	ALSA_TraceParameters(hw_params, sw_params, FALSE);

    /* now, we can save all required data for later use... */
    if ( wwo->hw_params )
    	snd_pcm_hw_params_free(wwo->hw_params);
    snd_pcm_hw_params_malloc(&(wwo->hw_params));
    snd_pcm_hw_params_copy(wwo->hw_params, hw_params);

    wwo->dwBufferSize = buffer_size;
    wwo->lpQueuePtr = wwo->lpPlayPtr = wwo->lpLoopPtr = NULL;
    wwo->p_handle = pcm;
    wwo->state = WINE_WS_STOPPED;

    /* the actual sample rate may be different from the requested,
     * we need to use the actual one for our timing. */
    wwo->format.Format.nSamplesPerSec = rate;
    wwo->format.Format.nAvgBytesPerSec = wwo->format.Format.nSamplesPerSec *
                                         (wwo->format.Format.wBitsPerSample / 8) *
                                         wwo->format.Format.nChannels;

    if (dwFlags & WAVE_DIRECTSOUND) {
	/* our dsound will want the real sample rate written back */
        copy_wave_format(lpDesc->lpFormat, &wwo->format.Format);
    }

    /* skip setting up the message ring and wodPlayer thread if reopening
       since those will have been set up already */
    if ((dwFlags & WAVE_REOPEN) == 0)
    {
        ALSA_InitRingMessage(&wwo->msgRing);

        if (!dsound) {
            wwo->hStartUpEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
            wwo->hThread = CreateThread(NULL, 0, wodPlayer, (LPVOID)(DWORD)wDevID, CREATE_SUSPENDED, &(wwo->dwThreadID));
            SERVER_START_REQ( set_scheduling_mode )
            {
                req->handle = wwo->hThread;
                req->mode = SCDM_INTERNAL;
                wine_server_call( req );
            }
            SERVER_END_REQ;
            SetThreadPriority(wwo->hThread, THREAD_PRIORITY_TIME_CRITICAL);
            ResumeThread(wwo->hThread);
            WaitForSingleObject(wwo->hStartUpEvent, INFINITE);
            CloseHandle(wwo->hStartUpEvent);
        } else {
            wwo->hThread = INVALID_HANDLE_VALUE;
            wwo->dwThreadID = 0;
        }
        wwo->hStartUpEvent = INVALID_HANDLE_VALUE;
    }

    TRACE("handle=%08lx \n", (DWORD)wwo->p_handle);
/*    if (wwo->dwFragmentSize % wwo->format.Format.nBlockAlign)
	ERR("Fragment doesn't contain an integral number of data blocks\n");
*/
    TRACE("wBitsPerSample=%u, nAvgBytesPerSec=%lu, nSamplesPerSec=%lu, nChannels=%u nBlockAlign=%u!\n",
	  wwo->format.Format.wBitsPerSample, wwo->format.Format.nAvgBytesPerSec,
	  wwo->format.Format.nSamplesPerSec, wwo->format.Format.nChannels,
	  wwo->format.Format.nBlockAlign);

    return wodNotifyClient(wwo, WOM_OPEN, 0L, 0L);
}


/**************************************************************************
 * 				wodClose			[internal]
 */
static DWORD wodClose(WORD wDevID)
{
    DWORD		ret = MMSYSERR_NOERROR;
    WINE_WAVEOUT*	wwo;

    TRACE("(%u);\n", wDevID);

    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    wwo = &WOutDev[wDevID];
    if (wwo->lpQueuePtr) {
	WARN("buffers still playing !\n");
	ret = WAVERR_STILLPLAYING;
    } else {
	if (wwo->hThread != INVALID_HANDLE_VALUE) {
	    ALSA_AddRingMessage(&wwo->msgRing, WINE_WM_CLOSING, 0, TRUE);
	}
        ALSA_DestroyRingMessage(&wwo->msgRing);

	snd_pcm_hw_params_free(wwo->hw_params);
	wwo->hw_params = NULL;

        snd_pcm_close(wwo->p_handle);
	wwo->p_handle = NULL;

	ret = wodNotifyClient(wwo, WOM_CLOSE, 0L, 0L);
    }
    return ret;
}


/**************************************************************************
 * 				wodWrite			[internal]
 *
 */
static DWORD wodWrite(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    /* first, do the sanity checks... */
    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
        WARN("bad dev ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpWaveHdr->lpData == NULL || !(lpWaveHdr->dwFlags & WHDR_PREPARED))
	return WAVERR_UNPREPARED;

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    lpWaveHdr->dwFlags |= WHDR_INQUEUE;
    lpWaveHdr->lpNext = 0;

    ALSA_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_HEADER, (DWORD)lpWaveHdr, FALSE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodPrepare			[internal]
 */
static DWORD wodPrepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEOUTDRV) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags |= WHDR_PREPARED;
    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodUnprepare			[internal]
 */
static DWORD wodUnprepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEOUTDRV) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags &= ~WHDR_PREPARED;
    lpWaveHdr->dwFlags |= WHDR_DONE;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			wodPause				[internal]
 */
static DWORD wodPause(WORD wDevID)
{
    TRACE("(%u);!\n", wDevID);

    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    ALSA_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_PAUSING, 0, TRUE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			wodRestart				[internal]
 */
static DWORD wodRestart(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);

    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (WOutDev[wDevID].state == WINE_WS_PAUSED) {
	ALSA_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_RESTARTING, 0, TRUE);
    }

    /* FIXME: is NotifyClient with WOM_DONE right ? (Comet Busters 1.3.3 needs this notification) */
    /* FIXME: Myst crashes with this ... hmm -MM
       return wodNotifyClient(wwo, WOM_DONE, 0L, 0L);
    */

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			wodReset				[internal]
 */
static DWORD wodReset(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);

    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    ALSA_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_RESETTING, 0, TRUE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodGetPosition			[internal]
 */
static DWORD wodGetPosition(WORD wDevID, LPMMTIME lpTime, DWORD uSize)
{
    int			time;
    DWORD		val;
    WINE_WAVEOUT*	wwo;

    TRACE("(%u, %p, %lu);\n", wDevID, lpTime, uSize);

    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpTime == NULL)	return MMSYSERR_INVALPARAM;

    wwo = &WOutDev[wDevID];
    ALSA_AddRingMessage(&wwo->msgRing, WINE_WM_UPDATE, 0, TRUE);
    val = wwo->dwPlayedTotal;

    TRACE("wType=%04X wBitsPerSample=%u nSamplesPerSec=%lu nChannels=%u nAvgBytesPerSec=%lu\n",
	  lpTime->wType, wwo->format.Format.wBitsPerSample,
	  wwo->format.Format.nSamplesPerSec, wwo->format.Format.nChannels,
	  wwo->format.Format.nAvgBytesPerSec);
    TRACE("dwPlayedTotal=%lu\n", val);

    switch (lpTime->wType) {
    case TIME_BYTES:
	lpTime->u.cb = val;
	TRACE("TIME_BYTES=%lu\n", lpTime->u.cb);
	break;
    case TIME_SAMPLES:
	lpTime->u.sample = val * 8 / wwo->format.Format.wBitsPerSample /wwo->format.Format.nChannels;
	TRACE("TIME_SAMPLES=%lu\n", lpTime->u.sample);
	break;
    case TIME_SMPTE:
	time = val / (wwo->format.Format.nAvgBytesPerSec / 1000);
	lpTime->u.smpte.hour = time / 108000;
	time -= lpTime->u.smpte.hour * 108000;
	lpTime->u.smpte.min = time / 1800;
	time -= lpTime->u.smpte.min * 1800;
	lpTime->u.smpte.sec = time / 30;
	time -= lpTime->u.smpte.sec * 30;
	lpTime->u.smpte.frame = time;
	lpTime->u.smpte.fps = 30;
	TRACE("TIME_SMPTE=%02u:%02u:%02u:%02u\n",
	      lpTime->u.smpte.hour, lpTime->u.smpte.min,
	      lpTime->u.smpte.sec, lpTime->u.smpte.frame);
	break;
    default:
	FIXME("Format %d not supported ! use TIME_MS !\n", lpTime->wType);
	lpTime->wType = TIME_MS;
    case TIME_MS:
	lpTime->u.ms = val / (wwo->format.Format.nAvgBytesPerSec / 1000);
	TRACE("TIME_MS=%lu\n", lpTime->u.ms);
	break;
    }
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodBreakLoop			[internal]
 */
static DWORD wodBreakLoop(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);

    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }
    ALSA_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_BREAKLOOP, 0, TRUE);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodGetVolume			[internal]
 */
static DWORD wodGetVolume(WORD wDevID, LPDWORD lpdwVol)
{
    WORD	       left, right;
    WINE_WAVEOUT*      wwo;
    int                count;
    long               min, max;

    TRACE("(%u, %p);\n", wDevID, lpdwVol);
    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }
    wwo = &WOutDev[wDevID];
    count = snd_ctl_elem_info_get_count(wwo->playback_einfo);
    min = snd_ctl_elem_info_get_min(wwo->playback_einfo);
    max = snd_ctl_elem_info_get_max(wwo->playback_einfo);

#define VOLUME_ALSA_TO_WIN(x) (((x)-min) * 65536 /(max-min))
    if (lpdwVol == NULL)
	return MMSYSERR_NOTENABLED;

    switch (count)
    {
   	case 2:
	    left = VOLUME_ALSA_TO_WIN(snd_ctl_elem_value_get_integer(wwo->playback_evalue, 0));
	    right = VOLUME_ALSA_TO_WIN(snd_ctl_elem_value_get_integer(wwo->playback_evalue, 1));
	    break;
	case 1:
	    left = right = VOLUME_ALSA_TO_WIN(snd_ctl_elem_value_get_integer(wwo->playback_evalue, 0));
	    break;
	default:
	    WARN("%d channels mixer not supported\n", count);
	    return MMSYSERR_NOERROR;
     }
#undef VOLUME_ALSA_TO_WIN

    TRACE("left=%d right=%d !\n", left, right);
    *lpdwVol = MAKELONG( left, right );
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodSetVolume			[internal]
 */
static DWORD wodSetVolume(WORD wDevID, DWORD dwParam)
{
    WORD	       left, right;
    WINE_WAVEOUT*      wwo;
    int                count, err;
    long               min, max;

    TRACE("(%u, %08lX);\n", wDevID, dwParam);
    if (wDevID >= MAX_WAVEOUTDRV || WOutDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }
    wwo = &WOutDev[wDevID];
    count=snd_ctl_elem_info_get_count(wwo->playback_einfo);
    min = snd_ctl_elem_info_get_min(wwo->playback_einfo);
    max = snd_ctl_elem_info_get_max(wwo->playback_einfo);

    left  = LOWORD(dwParam);
    right = HIWORD(dwParam);

#define VOLUME_WIN_TO_ALSA(x) ( (((x) * (max-min)) / 65536) + min )
    switch (count)
    {
   	case 2:
	    snd_ctl_elem_value_set_integer(wwo->playback_evalue, 0, VOLUME_WIN_TO_ALSA(left));
	    snd_ctl_elem_value_set_integer(wwo->playback_evalue, 1, VOLUME_WIN_TO_ALSA(right));
	    break;
	case 1:
	    snd_ctl_elem_value_set_integer(wwo->playback_evalue, 0, VOLUME_WIN_TO_ALSA(left));
	    break;
	default:
	    WARN("%d channels mixer not supported\n", count);
     }
#undef VOLUME_WIN_TO_ALSA
    if ( (err = snd_ctl_elem_write(wwo->ctl, wwo->playback_evalue)) < 0)
    {
	ERR("error writing snd_ctl_elem_value: %s\n", snd_strerror(err));
    }
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodGetNumDevs			[internal]
 */
static	DWORD	wodGetNumDevs(void)
{
    return ALSA_WodNumDevs;
}


/**************************************************************************
 * 				wodMessage (WINEALSA.7)
 */
DWORD WINAPI ALSA_wodMessage(UINT wDevID, UINT wMsg, DWORD dwUser,
                             DWORD dwParam1, DWORD dwParam2)
{
    TRACE("(%u, %04X, %08lX, %08lX, %08lX);\n",
	  wDevID, wMsg, dwUser, dwParam1, dwParam2);

    switch (wMsg) {
    case DRVM_INIT:
    case DRVM_EXIT:
    case DRVM_ENABLE:
    case DRVM_DISABLE:
	/* FIXME: Pretend this is supported */
	return 0;
    case WODM_OPEN:	 	return wodOpen		(wDevID, (LPWAVEOPENDESC)dwParam1,	dwParam2);
    case WODM_CLOSE:	 	return wodClose		(wDevID);
    case WODM_GETDEVCAPS:	return wodGetDevCaps	(wDevID, (LPWAVEOUTCAPSA)dwParam1,	dwParam2);
    case WODM_GETNUMDEVS:	return wodGetNumDevs	();
    case WODM_GETPITCH:	 	return MMSYSERR_NOTSUPPORTED;
    case WODM_SETPITCH:	 	return MMSYSERR_NOTSUPPORTED;
    case WODM_GETPLAYBACKRATE:	return MMSYSERR_NOTSUPPORTED;
    case WODM_SETPLAYBACKRATE:	return MMSYSERR_NOTSUPPORTED;
    case WODM_WRITE:	 	return wodWrite		(wDevID, (LPWAVEHDR)dwParam1,		dwParam2);
    case WODM_PAUSE:	 	return wodPause		(wDevID);
    case WODM_GETPOS:	 	return wodGetPosition	(wDevID, (LPMMTIME)dwParam1, 		dwParam2);
    case WODM_BREAKLOOP: 	return wodBreakLoop     (wDevID);
    case WODM_PREPARE:	 	return wodPrepare	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WODM_UNPREPARE: 	return wodUnprepare	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WODM_GETVOLUME:	return wodGetVolume	(wDevID, (LPDWORD)dwParam1);
    case WODM_SETVOLUME:	return wodSetVolume	(wDevID, dwParam1);
    case WODM_RESTART:		return wodRestart	(wDevID);
    case WODM_RESET:		return wodReset		(wDevID);
    case DRV_QUERYDSOUNDIFACE:	return wodDsCreate(wDevID, (PIDSDRIVER*)dwParam1);
    default:
	FIXME("unknown message %d!\n", wMsg);
    }
    return MMSYSERR_NOTSUPPORTED;
}

/*======================================================================*
 *                  Low level DSOUND implementation			*
 *======================================================================*/

typedef struct IDsDriverImpl IDsDriverImpl;
typedef struct IDsDriverBufferImpl IDsDriverBufferImpl;

struct IDsDriverImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDsDriver);
    DWORD		ref;
    /* IDsDriverImpl fields */
    UINT		wDevID;
    IDsDriverBufferImpl*primary;
};

struct IDsDriverBufferImpl
{
    /* IUnknown fields */
    ICOM_VFIELD(IDsDriverBuffer);
    DWORD		      ref;
    /* IDsDriverBufferImpl fields */
    IDsDriverImpl*	      drv;

    CRITICAL_SECTION          mmap_crst;
    LPVOID                    mmap_buffer;
    DWORD                     mmap_buflen_bytes;
    snd_pcm_uframes_t         mmap_buflen_frames;
    snd_pcm_channel_area_t *  mmap_areas;
    snd_async_handler_t *     mmap_async_handler;
    snd_pcm_uframes_t         mmap_app_position;
    const snd_pcm_channel_area_t * mmap_areas_last;
    snd_pcm_uframes_t         mmap_async_commit;
    LONG                      mmap_copy_pending;
    BOOL playing;
};

#define INIT_COPY(dsdb) \
  do { \
    EnterCriticalSection(&(dsdb)->mmap_crst); \
    DSDB_MMAPCopy(dsdb); \
    LeaveCriticalSection(&(dsdb)->mmap_crst); \
  } while (0)
#define ENTER_LOCK(dsdb) \
  do { \
    EnterCriticalSection(&(dsdb)->mmap_crst); \
  } while (0)
#define LEAVE_LOCK(dsdb) \
  do { \
    LONG recursion = (dsdb)->mmap_crst.RecursionCount; \
    LeaveCriticalSection(&(dsdb)->mmap_crst); \
    if (recursion <= 1) { \
      while (InterlockedExchange(&(dsdb)->mmap_copy_pending, 0)) \
        INIT_COPY(dsdb); \
    } \
  } while (0)

static inline snd_pcm_t* DSDB_get_handle(IDsDriverBufferImpl* pdbi)
{
    return WOutDev[pdbi->drv->wDevID].p_handle;
}

static inline snd_pcm_hw_params_t* DSDB_get_hw_params(IDsDriverBufferImpl* pdbi)
{
    return WOutDev[pdbi->drv->wDevID].hw_params;
}

static BOOL DSDB_CheckXRUN(IDsDriverBufferImpl* pdbi)
{
    snd_pcm_t *        handle = DSDB_get_handle(pdbi);
    snd_pcm_state_t    state = snd_pcm_state(handle);
    BOOL               reset = FALSE;

    if ( state == SND_PCM_STATE_XRUN )
    {
	int            err = snd_pcm_prepare(handle);
	TRACE("xrun occurred\n");
	if ( err < 0 )
	    ERR("recovery from xrun failed, prepare failed: %s\n", snd_strerror(err));
	reset = TRUE;
    }
    else if ( state == SND_PCM_STATE_SUSPENDED )
    {
	int            err = snd_pcm_resume(handle);
	TRACE("recovery from suspension occurred\n");
	if (err < 0 && err != -EAGAIN){
	    err = snd_pcm_prepare(handle);
	    if (err < 0)
		ERR("recovery from suspend failed, prepare failed: %s\n", snd_strerror(err));
	    reset = TRUE;
	}
    }
    if (reset) {
	pdbi->mmap_app_position = 0;
	if (pdbi->mmap_async_commit) {
	    /* reinitialize mmap_areas_last */
	    snd_pcm_uframes_t frames = 0;
	    snd_pcm_uframes_t offset = 0;
	    int err = snd_pcm_mmap_begin(handle, &pdbi->mmap_areas_last, &offset, &frames);
	    if (err < 0)
		ERR("mmap_begin failed, reason: %s\n", snd_strerror(err));
	}
    }
    return reset;
}

static void DSDB_MMAPCopy(IDsDriverBufferImpl* pdbi)
{
    snd_pcm_t *        handle = DSDB_get_handle(pdbi);
    snd_pcm_hw_params_t * hw_params = DSDB_get_hw_params(pdbi);
    int                channels;
    snd_pcm_format_t   format;
    snd_pcm_uframes_t  period_size, commit_size;
    snd_pcm_sframes_t  avail;
    snd_pcm_sframes_t  delay;
    snd_pcm_state_t    state;

    if ( !hw_params || !handle)
    	return;

    channels = snd_pcm_hw_params_get_channels(hw_params);
    format = snd_pcm_hw_params_get_format(hw_params);
    period_size = snd_pcm_hw_params_get_period_size(hw_params, 0);
    avail = snd_pcm_avail_update(handle);

    DSDB_CheckXRUN(pdbi);

    if (snd_pcm_delay(handle, &delay) < 0)
	delay = 0;
    state = snd_pcm_state(handle);
    if (state != SND_PCM_STATE_RUNNING) {
	/* the buffer is supposed to be empty here */
	delay = 0;
    }
    if (delay < 0) {
        /* shouldn't happen, ALSA should return XRUN error, but theory and practice differ */
        ERR("ALSA returned negative delay: %d\n", (int)delay);
    }

    if (delay < 0 || pdbi->mmap_async_commit >= delay)
	commit_size = pdbi->mmap_async_commit - delay;
    else
	commit_size = 0;

    if (commit_size > avail)
	commit_size = avail;

    TRACE("avail=%d delay=%d commit=%d state=%s format=%s channels=%d\n",
          (int)avail, (int)delay, (int)commit_size,
          snd_pcm_state_name(state), snd_pcm_format_name(format), channels );

    while (commit_size >= period_size)
    {
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t     ofs;
	snd_pcm_uframes_t     frames;
	int                   err;

	frames = commit_size / period_size * period_size; /* round down to a multiple of period_size */

	EnterCriticalSection(&pdbi->mmap_crst);

	snd_pcm_mmap_begin(handle, &areas, &ofs, &frames);
	TRACE("offset=%d frames=%d\n", (int)ofs, (int)frames);
	if (pdbi->mmap_buffer)
	    snd_pcm_areas_copy(areas, ofs, pdbi->mmap_areas, ofs, channels, frames, format);
	err = snd_pcm_mmap_commit(handle, ofs, frames);
	if (err > 0)
	    pdbi->mmap_app_position += err;

	LeaveCriticalSection(&pdbi->mmap_crst);

	if ( err != (snd_pcm_sframes_t) frames)
	    ERR("mmap partially failed.\n");

	avail = snd_pcm_avail_update(handle);

	commit_size -= frames;
	if (commit_size > avail)
	    commit_size = avail;
    }
}

static void DSDB_PCMCallback(snd_async_handler_t *ahandler)
{
    /* snd_pcm_t *               handle = snd_async_handler_get_pcm(ahandler); */
    IDsDriverBufferImpl*      pdbi = snd_async_handler_get_callback_private(ahandler);
    TRACE("callback called\n");
    InterlockedIncrement(&pdbi->mmap_copy_pending);
    /* try to acquire critical section (must not block) */
    if (!TryEnterCriticalSection(&pdbi->mmap_crst)) {
        return;
    }
    /* don't accept a recursive lock, it means we interrupted something */
    if (pdbi->mmap_crst.RecursionCount > 1) {
        LeaveCriticalSection(&pdbi->mmap_crst);
        return;
    }
    InterlockedExchange(&pdbi->mmap_copy_pending, 0);
    DSDB_MMAPCopy(pdbi);
    LeaveCriticalSection(&pdbi->mmap_crst);
}

static int DSDB_CreateMMAP(IDsDriverBufferImpl* pdbi)
 {
    snd_pcm_t *               handle = DSDB_get_handle(pdbi);
    snd_pcm_type_t            pcm_type = snd_pcm_type(handle);
    snd_pcm_hw_params_t *     hw_params = DSDB_get_hw_params(pdbi);
    snd_pcm_format_t          format = snd_pcm_hw_params_get_format(hw_params);
    snd_pcm_uframes_t         period_size = snd_pcm_hw_params_get_period_size(hw_params, 0);
    snd_pcm_uframes_t         frames = snd_pcm_hw_params_get_buffer_size(hw_params);
    snd_pcm_uframes_t         offset = 0;
    snd_pcm_sframes_t         move;
    int                       channels = snd_pcm_hw_params_get_channels(hw_params);
    unsigned int              bits_per_sample = snd_pcm_format_physical_width(format);
    unsigned int              bits_per_frame = bits_per_sample * channels;
#if 0
    snd_pcm_channel_area_t *  a;
    unsigned int              c;
#endif
    int                       err;

    if (TRACE_ON(wave))
	ALSA_TraceParameters(hw_params, NULL, FALSE);

    TRACE("format=%s  frames=%ld  channels=%d  bits_per_sample=%d  bits_per_frame=%d\n",
          snd_pcm_format_name(format), frames, channels, bits_per_sample, bits_per_frame);

    pdbi->mmap_app_position = 0;
    pdbi->mmap_buflen_frames = frames;
    pdbi->mmap_buflen_bytes = snd_pcm_frames_to_bytes( handle, frames );
    pdbi->mmap_async_commit = 0;
    pdbi->mmap_copy_pending = 0;
#if 0
    pdbi->mmap_buffer = HeapAlloc(GetProcessHeap(),0,pdbi->mmap_buflen_bytes);
    if (!pdbi->mmap_buffer)
	return DSERR_OUTOFMEMORY;

    snd_pcm_format_set_silence(format, pdbi->mmap_buffer, frames );
#else
    pdbi->mmap_buffer = NULL;
#endif

    TRACE("created mmap buffer of %ld frames (%ld bytes) at %p\n",
        frames, pdbi->mmap_buflen_bytes, pdbi->mmap_buffer);

#if 0
    pdbi->mmap_areas = HeapAlloc(GetProcessHeap(),0,channels*sizeof(snd_pcm_channel_area_t));
    if (!pdbi->mmap_areas)
	return DSERR_OUTOFMEMORY;

    a = pdbi->mmap_areas;
    for (c = 0; c < channels; c++, a++)
    {
	a->addr = pdbi->mmap_buffer;
	a->first = bits_per_sample * c;
	a->step = bits_per_frame;
	TRACE("Area %d: addr=%p  first=%d  step=%d\n", c, a->addr, a->first, a->step);
    }
#endif

    /* make sure buffer is filled with silence */
    /* since dmix does not support rewind, only do this for hw */
    if (pdbi->mmap_buffer || pcm_type == SND_PCM_TYPE_HW) {
	frames = snd_pcm_hw_params_get_buffer_size(hw_params);
	err = snd_pcm_mmap_begin(handle, &pdbi->mmap_areas_last, &offset, &frames);
	if (err < 0) {
	    ERR("mmap_begin failed, reason: %s\n", snd_strerror(err));
	    return E_FAIL;
	}
	snd_pcm_areas_silence(pdbi->mmap_areas_last, offset, channels, frames, format);
	move = snd_pcm_mmap_commit(handle, offset, frames);
	snd_pcm_rewind(handle, move);
    }
    else {
	/* just initialize mmap_areas_last */
	frames = 0;
	err = snd_pcm_mmap_begin(handle, &pdbi->mmap_areas_last, &offset, &frames);
	if (err < 0) {
	    ERR("mmap_begin failed, reason: %s\n", snd_strerror(err));
	    return E_FAIL;
	}
	frames = snd_pcm_hw_params_get_buffer_size(hw_params);
	snd_pcm_areas_silence(pdbi->mmap_areas_last, offset, channels, frames, format);
    }

    TRACE("PCM type: %d (%s)\n", pcm_type, snd_pcm_type_name(pcm_type));
    if (pdbi->mmap_buffer || pcm_type != SND_PCM_TYPE_HW) {
	/* keep 3 periods ahead */
	TRACE("enabling async commit\n");
	pdbi->mmap_async_commit = 2 * period_size;
    }

    CRITICAL_SECTION_DEFINE(&pdbi->mmap_crst);

    if (pdbi->mmap_async_commit) {
	err = snd_async_add_pcm_handler(&pdbi->mmap_async_handler, handle, DSDB_PCMCallback, pdbi);
	if ( err < 0 )
	{
	    ERR("add_pcm_handler failed. reason: %s\n", snd_strerror(err));
	    return DSERR_GENERIC;
	}
    }

    return DS_OK;
 }

static void DSDB_DestroyMMAP(IDsDriverBufferImpl* pdbi)
{
    TRACE("mmap buffer %p destroyed\n", pdbi->mmap_buffer);
#if 0
    HeapFree(GetProcessHeap(), 0, pdbi->mmap_areas);
#endif
    HeapFree(GetProcessHeap(), 0, pdbi->mmap_buffer);
    pdbi->mmap_areas = NULL;
    pdbi->mmap_buffer = NULL;
    DeleteCriticalSection(&pdbi->mmap_crst);
}


static HRESULT WINAPI IDsDriverBufferImpl_QueryInterface(PIDSDRIVERBUFFER iface, REFIID riid, LPVOID *ppobj)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    FIXME("(): stub!\n");
    return DSERR_UNSUPPORTED;
}

static ULONG WINAPI IDsDriverBufferImpl_AddRef(PIDSDRIVERBUFFER iface)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    TRACE("(%p)\n",iface);
    return ++This->ref;
}

static ULONG WINAPI IDsDriverBufferImpl_Release(PIDSDRIVERBUFFER iface)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    TRACE("(%p)\n",iface);
    if (--This->ref)
	return This->ref;
    if (This == This->drv->primary)
	This->drv->primary = NULL;
    DSDB_DestroyMMAP(This);
    HeapFree(GetProcessHeap(), 0, This);
    return 0;
}

static HRESULT WINAPI IDsDriverBufferImpl_HwLock(PIDSDRIVERBUFFER iface,
						 LPVOID*ppvAudio1,LPDWORD pdwLen1,
						 LPVOID*ppvAudio2,LPDWORD pdwLen2,
						 DWORD dwWritePosition,DWORD dwWriteLen,
						 DWORD dwFlags)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    snd_pcm_t * handle;
    snd_pcm_sframes_t delay = 0, avail, move;
    snd_pcm_uframes_t play_pos, write_pos, offset, frames, total;
    snd_pcm_uframes_t start_pos, end_pos, left;
    DWORD dwPlayPos, dwDist, dwLen;
    int err, fmode;

    TRACE("(%p,...,%ld,%ld,0x%lx)\n",iface,dwWritePosition,dwWriteLen,dwFlags);

    DSDB_CheckXRUN(This);

    ENTER_LOCK(This);
    handle = DSDB_get_handle(This);

    /* get playing position */
#if 0
    /* this simply retrieves the playing position from the hardware...
     * only works reliably when using "hw" without userspace plugins */
    snd_pcm_delay(handle, &delay);
    if (snd_pcm_state (handle) != SND_PCM_STATE_RUNNING)
        delay = 0;
    avail = This->mmap_buflen_frames - delay;
#else
    /* this propagates playing position from the hardware to ALSA's
     * application layer, thus ensuring that the mmap_begins in here
     * works OK even when userspace plugins are used. */
    snd_pcm_hwsync(handle);
    avail = snd_pcm_avail_update(handle);
    if (avail < 0)
    {
        ALSA_XRUNRecovery (handle, avail);
        avail = snd_pcm_avail_update (handle);
        if (avail < 0)
            avail = 0;
    }

    /* in theory, delay + avail == This->mmap_buflen_frames,
     * but apparently only when using "hw" directly */
    /* delay = This->mmap_buflen_frames - avail; */
    snd_pcm_delay(handle, &delay);
    if (snd_pcm_state (handle) != SND_PCM_STATE_RUNNING)
        delay = 0;
#endif

    play_pos = This->mmap_app_position - delay;
    TRACE("playpos=%ld, pos=%ld\n", play_pos, This->mmap_app_position);

    /* we should ensure that delay + avail == This->mmap_buflen_frames, perhaps. */

    dwPlayPos = snd_pcm_frames_to_bytes(handle, play_pos) % This->mmap_buflen_bytes;

    /* assuming that writepos is always temporally ahead of playing position,
     * compute the ALSA frame position that correspond to requested write position */
    dwDist = ((dwWritePosition < dwPlayPos) ? This->mmap_buflen_bytes : 0) + dwWritePosition - dwPlayPos;
    write_pos = play_pos + snd_pcm_bytes_to_frames(handle, dwDist);

    total = snd_pcm_bytes_to_frames(handle, dwWriteLen);

    /* if the locked region runs across the playpos, lock the region
     * in two pieces: first the region ahead of the playpos, then the region
     * trailing it. */
    if ((write_pos + total) > (play_pos + This->mmap_buflen_frames))
	fmode = 1; /* 1 = playpos -> endpos, 2 = writepos -> playpos+buflen */
    else
	fmode = 3; /* 3 = writepos -> endpos */

    while (fmode) {
	TRACE("fmode=%d\n", fmode);

	switch (fmode) {
	case 1:
	    start_pos = play_pos;
	    end_pos = write_pos + total - This->mmap_buflen_frames;
	    fmode = 2;
	    break;
	case 2:
	    start_pos = write_pos;
	    end_pos = play_pos + This->mmap_buflen_frames;
	    fmode = 0;
	    break;
	case 3:
	default:
	    start_pos = write_pos;
	    end_pos = write_pos + total;
	    fmode = 0;
	    break;
	}

	left = end_pos - start_pos;

	/* adjust ALSA app position to point to starting frame position,
	 * so that mmap_begin returns the desired buffer position */
	if (start_pos < This->mmap_app_position) {
	    TRACE("rew=%ld\n", This->mmap_app_position - start_pos);
	    move = snd_pcm_rewind(handle, This->mmap_app_position - start_pos);
	    if (move > 0) This->mmap_app_position -= move;
	    /* pcm_rewind may not have rewound all the way (because
	     * playpos may have moved since last check), compensate */
	    move = This->mmap_app_position - start_pos;
	    if (move >= left) left = 0;
	    else left -= move;
	    start_pos += move;
	    play_pos += move;
	}
	else if (start_pos > This->mmap_app_position) {
	    TRACE("fwd=%ld\n", start_pos - This->mmap_app_position);
	    move = snd_pcm_forward(handle, start_pos - This->mmap_app_position);
	    if (move > 0) This->mmap_app_position += move;
	    /* even if some ALSA bug cause pcm_forward to seek too far,
	     * we may as well still try to avoid assertion failures */
	    move = This->mmap_app_position - start_pos;
	    if (move >= left) left = 0;
	    else if (move >= 0) left -= move;
	}
	TRACE("app_pos=%ld\n", This->mmap_app_position);

	/* request the mmap-ed area */
	while (left) {
	    offset = 0;
	    frames = left;
	    TRACE("begin: playpos=%ld, pos=%ld, left=%ld\n", play_pos, This->mmap_app_position, left);
	    err = snd_pcm_mmap_begin(handle, &This->mmap_areas_last, &offset, &frames);
	    if (err < 0) {
		ERR("mmap_begin failed, reason: %s\n", snd_strerror(err));
		LEAVE_LOCK(This);
		return E_FAIL;
	    }

	    TRACE("mmap_begin returns: offset=%ld, frames=%ld\n", offset, frames);

	    if (offset != (This->mmap_app_position % This->mmap_buflen_frames)) {
		ERR("returned offset=%ld, expected=%ld\n", offset, This->mmap_app_position % This->mmap_buflen_frames);
	    }

	    if (!frames) {
		ERR("mmap_begin didn't return any frames, likely ALSA bug! (play_pos=%ld, app_pos=%ld, start_pos=%ld)\n",
		    play_pos, This->mmap_app_position, start_pos);
		ERR("aborting lock, audio glitches may occur.\n");
		break;
	    }

	    left -= frames;
	    if (!left) break; /* mmap complete */

	    /* mmap_begin wrapped, seek forward to next region */
	    TRACE("fwd=%ld (wrap?)\n", frames);
	    move = snd_pcm_forward(handle, frames);
	    if (move > 0) This->mmap_app_position += move;
	}
    }

    /* seek back to write_pos; not terribly useful, but makes me feel better */
    if (write_pos < This->mmap_app_position) {
	TRACE("rew=%ld\n", This->mmap_app_position - write_pos);
	move = snd_pcm_rewind(handle, This->mmap_app_position - write_pos);
	if (move > 0) This->mmap_app_position -= move;
    }
    else if (write_pos > This->mmap_app_position) {
	TRACE("fwd=%ld\n", write_pos - This->mmap_app_position);
	move = snd_pcm_forward(handle, write_pos - This->mmap_app_position);
	if (move > 0) This->mmap_app_position += move;
    }

    dwLen = dwWriteLen;

    /* generate pointers to return */
    *ppvAudio1 = ((LPBYTE)This->mmap_areas_last[0].addr) + dwWritePosition;
    if ((dwWritePosition + dwLen) > This->mmap_buflen_bytes)
	*pdwLen1 = This->mmap_buflen_bytes - dwWritePosition;
    else
	*pdwLen1 = dwLen;
    dwLen -= *pdwLen1;

    if (dwLen) {
	if (ppvAudio2) *ppvAudio2 = ((LPBYTE)This->mmap_areas_last[0].addr);
	if (pdwLen2) *pdwLen2 = dwLen;
    } else {
	if (ppvAudio2) *ppvAudio2 = NULL;
	if (pdwLen2) *pdwLen2 = 0;
    }

    LEAVE_LOCK(This);

    TRACE("=>(%p,%ld,%p,%ld)\n", *ppvAudio1, *pdwLen1,
				 ppvAudio2 ? *ppvAudio2 : NULL,
				 pdwLen2 ? *pdwLen2 : 0);

    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_HwUnlock(PIDSDRIVERBUFFER iface,
						   LPVOID pvAudio1,DWORD dwLen1,
						   LPVOID pvAudio2,DWORD dwLen2)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    snd_pcm_t * handle;
    snd_pcm_state_t    state;
    snd_pcm_sframes_t delay = 0, avail, move;
    snd_pcm_uframes_t play_pos, write_pos, offset, total;
    snd_pcm_uframes_t start_pos, end_pos, left;
    DWORD dwWritePos, dwPlayPos, dwDist;
    int fmode;

    TRACE("(%p,%p,%ld,%p,%ld)\n",iface,pvAudio1,dwLen1,pvAudio2,dwLen2);

    DSDB_CheckXRUN (This);

    ENTER_LOCK(This);
    handle = DSDB_get_handle(This);

    dwWritePos = ((LPBYTE)pvAudio1) - ((LPBYTE)This->mmap_areas_last[0].addr);

    /* get playing position */
#if 0
    snd_pcm_delay(handle, &delay);
    if (snd_pcm_state (handle) != SND_PCM_STATE_RUNNING)
        delay = 0;
    play_pos = This->mmap_app_position - delay;
    avail = This->mmap_buflen_frames - delay;
#else
    snd_pcm_hwsync(handle);
    avail = snd_pcm_avail_update(handle);
    if (avail < 0)
    {
        ALSA_XRUNRecovery (handle, avail);
        avail = snd_pcm_avail_update (handle);
        if (avail < 0)
            avail = 0;
    }

    /* delay = This->mmap_buflen_frames - avail; */
    snd_pcm_delay(handle, &delay);
    if (snd_pcm_state (handle) != SND_PCM_STATE_RUNNING)
        delay = 0;
#endif

    play_pos = This->mmap_app_position - delay;
    TRACE("playpos=%ld, pos=%ld\n", play_pos, This->mmap_app_position);

    dwPlayPos = snd_pcm_frames_to_bytes(handle, play_pos) % This->mmap_buflen_bytes;

    dwDist = ((dwWritePos < dwPlayPos) ? This->mmap_buflen_bytes : 0) + dwWritePos - dwPlayPos;
    write_pos = play_pos + snd_pcm_bytes_to_frames(handle, dwDist);

    total = snd_pcm_bytes_to_frames(handle, dwLen1 + dwLen2);

    if ((write_pos + total) > (play_pos + This->mmap_buflen_frames))
	fmode = 1; /* 1 = playpos -> endpos, 2 = writepos -> playpos+buflen */
    else
	fmode = 3; /* 3 = writepos -> endpos */

    while (fmode) {
	TRACE("fmode=%d\n", fmode);

	switch (fmode) {
	case 1:
	    start_pos = play_pos;
	    end_pos = write_pos + total - This->mmap_buflen_frames;
	    fmode = 2;
	    break;
	case 2:
	    start_pos = write_pos;
	    end_pos = play_pos + This->mmap_buflen_frames;
	    fmode = 0;
	    break;
	case 3:
	default:
	    start_pos = write_pos;
	    end_pos = write_pos + total;
	    fmode = 0;
	    break;
	}

	left = end_pos - start_pos;

	if (start_pos < This->mmap_app_position) {
	    TRACE("rew=%ld\n", This->mmap_app_position - start_pos);
	    move = snd_pcm_rewind(handle, This->mmap_app_position - start_pos);
	    if (move > 0) This->mmap_app_position -= move;
	    /* pcm_rewind may not have rewound all the way (because
	     * playpos may have moved since last check), compensate */
	    move = This->mmap_app_position - start_pos;
	    if (move >= left) left = 0;
	    else left -= move;
	    start_pos += move;
	    play_pos += move;
	}
	else if (start_pos > This->mmap_app_position) {
	    TRACE("fwd=%ld\n", start_pos - This->mmap_app_position);
	    move = snd_pcm_forward(handle, start_pos - This->mmap_app_position);
	    if (move > 0) This->mmap_app_position += move;
	    /* even if some ALSA bug cause pcm_forward to seek too far,
	     * we may as well still try to avoid assertion failures */
	    move = This->mmap_app_position - start_pos;
	    if (move >= left) left = 0;
	    else if (move >= 0) left -= move;
	}
	TRACE("app_pos=%ld\n", This->mmap_app_position);

	/* commit the mmap-ed area */
	while (left) {
	    offset = This->mmap_app_position % This->mmap_buflen_frames;
	    TRACE("commit: left=%ld, playpos=%ld, offset=%ld, pos=%ld\n", left,
		  play_pos, offset, This->mmap_app_position);
	    move = snd_pcm_mmap_commit(handle, offset, left);
	    if (move < 0) {
		ERR("mmap_commit failed, reason: %s\n", snd_strerror(move));
		LEAVE_LOCK(This);
		return E_FAIL;
	    }
	    if (!move) {
		ERR("mmap_commit didn't process any frames, likely ALSA bug! (play_pos=%ld, app_pos=%ld, start_pos=%ld)\n",
		    play_pos, This->mmap_app_position, start_pos);
		ERR("aborting unlock, audio glitches may occur.\n");
		break;
	    }
	    This->mmap_app_position += move;
	    left -= move;
	}
    }
    TRACE("app_pos=%ld\n", This->mmap_app_position);

    LEAVE_LOCK(This);

    state = snd_pcm_state(handle);
    if ( state == SND_PCM_STATE_PREPARED && This->playing )
    {
	/* finish xrun recovery */
	TRACE("restarting playback\n");
	snd_pcm_start(handle);
    }

    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Lock(PIDSDRIVERBUFFER iface,
					       LPVOID*ppvAudio1,LPDWORD pdwLen1,
					       LPVOID*ppvAudio2,LPDWORD pdwLen2,
					       DWORD dwWritePosition,DWORD dwWriteLen,
					       DWORD dwFlags)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    LPVOID buffer;

    TRACE("(%p,...,%ld,%ld,0x%lx)\n",iface,dwWritePosition,dwWriteLen,dwFlags);
    if (!(This->mmap_buffer || This->mmap_async_commit)) {
	return IDsDriverBufferImpl_HwLock(iface, ppvAudio1, pdwLen1, ppvAudio2, pdwLen2,
					  dwWritePosition, dwWriteLen, dwFlags);
    }

    if (This->mmap_buffer)
	buffer = This->mmap_buffer;
    else
	buffer = This->mmap_areas_last[0].addr;

    *ppvAudio1 = ((LPBYTE)buffer) + dwWritePosition;
    if ((dwWritePosition + dwWriteLen) > This->mmap_buflen_bytes)
	*pdwLen1 = This->mmap_buflen_bytes - dwWritePosition;
    else
	*pdwLen1 = dwWriteLen;
    dwWriteLen -= *pdwLen1;

    if (dwWriteLen) {
	if (ppvAudio2) *ppvAudio2 = (LPBYTE)buffer;
	if (pdwLen2) *pdwLen2 = dwWriteLen;
    } else {
	if (ppvAudio2) *ppvAudio2 = NULL;
	if (pdwLen2) *pdwLen2 = 0;
    }

    TRACE("=>(%p,%ld,%p,%ld)\n", *ppvAudio1, *pdwLen1,
                                 ppvAudio2 ? *ppvAudio2 : NULL,
                                 pdwLen2 ? *pdwLen2 : 0);

    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Unlock(PIDSDRIVERBUFFER iface,
						 LPVOID pvAudio1,DWORD dwLen1,
						 LPVOID pvAudio2,DWORD dwLen2)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    DWORD dwWritePos, dwWriteLen;
    LPVOID buffer;
    LPBYTE ptr1, ptr2;
    DWORD len1, len2, len;
    HRESULT hr;

    TRACE("(%p,%p,%ld,%p,%ld)\n",iface,pvAudio1,dwLen1,pvAudio2,dwLen2);
    if (!(This->mmap_buffer || This->mmap_async_commit)) {
	return IDsDriverBufferImpl_HwUnlock(iface, pvAudio1, dwLen1, pvAudio2, dwLen2);
    }

    if (This->mmap_buffer)
	buffer = This->mmap_buffer;
    else
	buffer = This->mmap_areas_last[0].addr;

    dwWritePos = ((LPBYTE)pvAudio1) - ((LPBYTE)buffer);
    dwWriteLen = dwLen1 + dwLen2;

    if (This->mmap_async_commit || !This->mmap_buffer)
	return DS_OK;

    /* transfer data to hw buffer */

    hr = IDsDriverBufferImpl_HwLock(iface, (LPVOID*)&ptr1, &len1, (LPVOID*)&ptr2, &len2,
				    dwWritePos, dwWriteLen, 0);
    if (FAILED(hr)) return hr;

    /* do not assume that dwWritePos is the same writepos given to HwLock,
     * since this may need to change in the future (to implement SetPosition). */
    len = This->mmap_buflen_bytes - dwWritePos;
    if (len1 < len) len = len1;
    memcpy(ptr1, ((LPBYTE)buffer) + dwWritePos, len);
    ptr1 += len;
    dwWritePos += len;
    len1 -= len;
    if (dwWritePos == This->mmap_buflen_bytes)
	dwWritePos = 0;
    if (len1) {
	assert(dwWritePos == 0);
	memcpy(ptr1, buffer, len1);
	dwWritePos += len1;
    }
    len = This->mmap_buflen_bytes - dwWritePos;
    if (len2 < len) len = len2;
    memcpy(ptr2, ((LPBYTE)buffer) + dwWritePos, len);
    ptr2 += len;
    dwWritePos += len;
    len2 -= len;
    if (dwWritePos == This->mmap_buflen_bytes)
	dwWritePos = 0;
    if (len2) {
	assert(dwWritePos == 0);
	memcpy(ptr2, buffer, len2);
	dwWritePos += len2;
    }

    hr = IDsDriverBufferImpl_HwUnlock(iface, ptr1, len1, ptr2, len2);
    if (FAILED(hr)) return hr;

    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetFormat(PIDSDRIVERBUFFER iface,
						    LPWAVEFORMATEX pwfx)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    TRACE("(%p,%p)\n",iface,pwfx);
    return DSERR_BUFFERLOST;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetFrequency(PIDSDRIVERBUFFER iface, DWORD dwFreq)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    TRACE("(%p,%ld): stub\n",iface,dwFreq);
    return DSERR_UNSUPPORTED;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetVolumePan(PIDSDRIVERBUFFER iface, PDSVOLUMEPAN pVolPan)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    FIXME("(%p,%p): stub!\n",iface,pVolPan);
    return DSERR_UNSUPPORTED;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetPosition(PIDSDRIVERBUFFER iface, DWORD dwNewPos)
{
    /* ICOM_THIS(IDsDriverImpl,iface); */
    TRACE("(%p,%ld): stub\n",iface,dwNewPos);
    return DSERR_UNSUPPORTED;
}

static HRESULT WINAPI IDsDriverBufferImpl_GetPosition(PIDSDRIVERBUFFER iface,
						      LPDWORD lpdwPlay, LPDWORD lpdwWrite)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    snd_pcm_t *         handle;
    snd_pcm_sframes_t   delay = 0;
    snd_pcm_uframes_t   hw_ptr;
/*    snd_pcm_hw_params_t * hw_params = DSDB_get_hw_params(This); */
/*    snd_pcm_uframes_t   period_size = snd_pcm_hw_params_get_period_size(hw_params, 0); */
    snd_pcm_state_t     state;

    /** we need to track down buffer underruns */
    DSDB_CheckXRUN(This);

    ENTER_LOCK(This);
    handle = DSDB_get_handle(This);

    if (!handle) {
	TRACE("no hw_ptr, returning zero\n");
	if (lpdwPlay)
	    *lpdwPlay = 0;
	if (lpdwWrite)
	    *lpdwWrite = 0;
	LEAVE_LOCK(This);
	return DS_OK;
    }

#if 0
    hw_ptr = _snd_pcm_mmap_hw_ptr(handle);
#else
    snd_pcm_delay(handle, &delay);
   /* 'delay' is gibberish unless state is running. See:
      http://www.mail-archive.com/alsa-devel@lists.sourceforge.net/msg12704.html */
    if (snd_pcm_state (handle) != SND_PCM_STATE_RUNNING)
        delay = 0;

    TRACE("(%p): app_pos=%ld, delay=%ld\n", iface, This->mmap_app_position, delay);
    hw_ptr = This->mmap_app_position - delay;
#endif
    state = snd_pcm_state(handle);
#if 0
    if (lpdwPlay)
	*lpdwPlay = snd_pcm_frames_to_bytes(handle, hw_ptr / period_size * period_size ) % This->mmap_buflen_bytes;
    if (lpdwWrite)
	*lpdwWrite = snd_pcm_frames_to_bytes(handle, (hw_ptr / period_size + 1) * period_size ) % This->mmap_buflen_bytes;
#else
    if (lpdwPlay)
	*lpdwPlay = snd_pcm_frames_to_bytes(handle, hw_ptr ) % This->mmap_buflen_bytes;
    if (lpdwWrite) {
	/* add some safety margin (not strictly necessary, but...) */
#if 0 /* seems to cause too many audio glitches */
	if (state == SND_PCM_STATE_RUNNING) {
	    if (This->mmap_async_commit)
		*lpdwWrite = snd_pcm_frames_to_bytes(handle, (hw_ptr + This->mmap_async_commit) / period_size * period_size ) % This->mmap_buflen_bytes;
	    else
		*lpdwWrite = snd_pcm_frames_to_bytes(handle, hw_ptr + 8 ) % This->mmap_buflen_bytes;
	}
	else
#endif
	    *lpdwWrite = snd_pcm_frames_to_bytes(handle, hw_ptr) % This->mmap_buflen_bytes;
    }
#endif
    LEAVE_LOCK(This);

    TRACE("hw_ptr=%ld, playpos=%ld, writepos=%ld\n", hw_ptr, lpdwPlay?*lpdwPlay:-1, lpdwWrite?*lpdwWrite:-1);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Play(PIDSDRIVERBUFFER iface, DWORD dwRes1, DWORD dwRes2, DWORD dwFlags)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    snd_pcm_t *          handle = DSDB_get_handle(This);
    snd_pcm_state_t      state;
    int                  err = 0;

    TRACE("(%p,%lx,%lx,%lx)\n",iface,dwRes1,dwRes2,dwFlags);

    state = snd_pcm_state(handle);
    if ( state == SND_PCM_STATE_SETUP )
    {
	err = snd_pcm_prepare(handle);
	state = snd_pcm_state(handle);
    }
    if ( state == SND_PCM_STATE_PREPARED )
    {
	if (This->mmap_async_commit)
	    INIT_COPY(This);
	err = snd_pcm_start(handle);
    }

    if (err < 0)
    {
        ERR ("Error while starting pcm: %s\n", snd_strerror (err));
        return DSERR_GENERIC;
    }

    This->playing = TRUE;

    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Stop(PIDSDRIVERBUFFER iface)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    snd_pcm_t *       handle = DSDB_get_handle(This);
    int               err;

    TRACE("(%p)\n",iface);

    This->playing = FALSE;

    if ( ( err = snd_pcm_drop(handle)) < 0 )
    {
   	ERR("error while stopping pcm: %s\n", snd_strerror(err));
	return DSERR_GENERIC;
    }

    This->mmap_app_position = 0;
    snd_pcm_prepare(handle);

    if (This->mmap_async_commit) {
	/* reinitialize mmap_areas_last */
	snd_pcm_uframes_t frames = 0;
	snd_pcm_uframes_t offset = 0;
	err = snd_pcm_mmap_begin(handle, &This->mmap_areas_last, &offset, &frames);
	if (err < 0) {
	    ERR("mmap_begin failed, reason: %s\n", snd_strerror(err));
	    return E_FAIL;
	}
    }

    return DS_OK;
}

static ICOM_VTABLE(IDsDriverBuffer) dsdbvt =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IDsDriverBufferImpl_QueryInterface,
    IDsDriverBufferImpl_AddRef,
    IDsDriverBufferImpl_Release,
    IDsDriverBufferImpl_Lock,
    IDsDriverBufferImpl_Unlock,
    IDsDriverBufferImpl_SetFormat,
    IDsDriverBufferImpl_SetFrequency,
    IDsDriverBufferImpl_SetVolumePan,
    IDsDriverBufferImpl_SetPosition,
    IDsDriverBufferImpl_GetPosition,
    IDsDriverBufferImpl_Play,
    IDsDriverBufferImpl_Stop
};

static HRESULT WINAPI IDsDriverImpl_QueryInterface(PIDSDRIVER iface, REFIID riid, LPVOID *ppobj)
{
    /* ICOM_THIS(IDsDriverImpl,iface); */
    FIXME("(%p): stub!\n",iface);
    return DSERR_UNSUPPORTED;
}

static ULONG WINAPI IDsDriverImpl_AddRef(PIDSDRIVER iface)
{
    ICOM_THIS(IDsDriverImpl,iface);
    TRACE("(%p)\n",iface);
    This->ref++;
    return This->ref;
}

static ULONG WINAPI IDsDriverImpl_Release(PIDSDRIVER iface)
{
    ICOM_THIS(IDsDriverImpl,iface);
    TRACE("(%p)\n",iface);
    if (--This->ref)
	return This->ref;
    HeapFree(GetProcessHeap(),0,This);
    return 0;
}

static HRESULT WINAPI IDsDriverImpl_GetDriverDesc(PIDSDRIVER iface, PDSDRIVERDESC pDesc)
{
    ICOM_THIS(IDsDriverImpl,iface);
    TRACE("(%p,%p)\n",iface,pDesc);
    pDesc->dwFlags = DSDDESC_DOMMSYSTEMOPEN | DSDDESC_DOMMSYSTEMSETFORMAT |
	DSDDESC_USESYSTEMMEMORY;
    strcpy(pDesc->szDesc, "WineALSA DirectSound Driver");
    strcpy(pDesc->szDrvName, "winealsa.drv");
    pDesc->dnDevNode		= WOutDev[This->wDevID].waveDesc.dnDevNode;
    pDesc->wVxdId		= 0;
    pDesc->wReserved		= 0;
    pDesc->ulDeviceNum		= This->wDevID;
    pDesc->dwHeapType		= DSDHEAP_NOHEAP;
    pDesc->pvDirectDrawHeap	= NULL;
    pDesc->dwMemStartAddress	= 0;
    pDesc->dwMemEndAddress	= 0;
    pDesc->dwMemAllocExtra	= 0;
    pDesc->pvReserved1		= NULL;
    pDesc->pvReserved2		= NULL;
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_Open(PIDSDRIVER iface)
{
    /* ICOM_THIS(IDsDriverImpl,iface); */
    TRACE("(%p)\n",iface);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_Close(PIDSDRIVER iface)
{
    /* ICOM_THIS(IDsDriverImpl,iface); */
    TRACE("(%p)\n",iface);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_GetCaps(PIDSDRIVER iface, PDSDRIVERCAPS pCaps)
{
    ICOM_THIS(IDsDriverImpl,iface);
    TRACE("(%p,%p)\n",iface,pCaps);
    memset(pCaps, 0, sizeof(*pCaps));

    pCaps->dwFlags = DSCAPS_PRIMARYMONO;
    if ( WOutDev[This->wDevID].caps.wChannels >= 2 )
        pCaps->dwFlags |= DSCAPS_PRIMARYSTEREO;

    if ( WOutDev[This->wDevID].caps.dwFormats & (WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 |
                                                 WAVE_FORMAT_48S08 | WAVE_FORMAT_96S08) )
	pCaps->dwFlags |= DSCAPS_PRIMARY8BIT;

    if ( WOutDev[This->wDevID].caps.dwFormats & (WAVE_FORMAT_1S16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_4S16 |
                                                 WAVE_FORMAT_48S16 | WAVE_FORMAT_96S16) )
	pCaps->dwFlags |= DSCAPS_PRIMARY16BIT;

    pCaps->dwPrimaryBuffers = 1;
    TRACE("caps=0x%X\n",(unsigned int)pCaps->dwFlags);

    /* the other fields only apply to secondary buffers, which we don't support yet */
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_CreateSoundBuffer(PIDSDRIVER iface,
						      LPWAVEFORMATEX pwfx,
						      DWORD dwFlags, DWORD dwCardAddress,
						      LPDWORD pdwcbBufferSize,
						      LPBYTE *ppbBuffer,
						      LPVOID *ppvObj)
{
    ICOM_THIS(IDsDriverImpl,iface);
    IDsDriverBufferImpl** ippdsdb = (IDsDriverBufferImpl**)ppvObj;
    int err;

    TRACE("(%p,%p,%lx,%lx)\n",iface,pwfx,dwFlags,dwCardAddress);
    /* we only support primary buffers */
    if (!(dwFlags & DSBCAPS_PRIMARYBUFFER))
	return DSERR_UNSUPPORTED;
    if (This->primary)
	return DSERR_ALLOCATED;
    if (dwFlags & (DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN))
	return DSERR_CONTROLUNAVAIL;

    *ippdsdb = (IDsDriverBufferImpl*)HeapAlloc(GetProcessHeap(),0,sizeof(IDsDriverBufferImpl));
    if (*ippdsdb == NULL)
	return DSERR_OUTOFMEMORY;
    ICOM_VTBL(*ippdsdb) = &dsdbvt;
    (*ippdsdb)->ref	= 1;
    (*ippdsdb)->drv	= This;
    (*ippdsdb)->playing = FALSE;

    err = DSDB_CreateMMAP((*ippdsdb));
    if ( err != DS_OK )
    {
	HeapFree(GetProcessHeap(), 0, *ippdsdb);
	*ippdsdb = NULL;
	return err;
    }
    /* *ppbBuffer = (*ippdsdb)->mmap_buffer; */
    *ppbBuffer = NULL;
    *pdwcbBufferSize = (*ippdsdb)->mmap_buflen_bytes;

    This->primary = *ippdsdb;

    /* buffer is ready to go */
    TRACE("buffer created at %p\n", *ippdsdb);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_DuplicateSoundBuffer(PIDSDRIVER iface,
							 PIDSDRIVERBUFFER pBuffer,
							 LPVOID *ppvObj)
{
    /* ICOM_THIS(IDsDriverImpl,iface); */
    TRACE("(%p,%p): stub\n",iface,pBuffer);
    return DSERR_INVALIDCALL;
}

static ICOM_VTABLE(IDsDriver) dsdvt =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IDsDriverImpl_QueryInterface,
    IDsDriverImpl_AddRef,
    IDsDriverImpl_Release,
    IDsDriverImpl_GetDriverDesc,
    IDsDriverImpl_Open,
    IDsDriverImpl_Close,
    IDsDriverImpl_GetCaps,
    IDsDriverImpl_CreateSoundBuffer,
    IDsDriverImpl_DuplicateSoundBuffer
};

static DWORD wodDsCreate(UINT wDevID, PIDSDRIVER* drv)
{
    IDsDriverImpl** idrv = (IDsDriverImpl**)drv;

    TRACE("driver created\n");

    /* the HAL isn't much better than the HEL if we can't do mmap() */
    if (!(WOutDev[wDevID].caps.dwSupport & WAVECAPS_DIRECTSOUND)) {
	ERR("DirectSound flag not set\n");
	MESSAGE("This sound card's driver does not support direct access\n");
	MESSAGE("The (slower) DirectSound HEL mode will be used instead.\n");
	return MMSYSERR_NOTSUPPORTED;
    }

    *idrv = (IDsDriverImpl*)HeapAlloc(GetProcessHeap(),0,sizeof(IDsDriverImpl));
    if (!*idrv)
	return MMSYSERR_NOMEM;
    ICOM_VTBL(*idrv)	= &dsdvt;
    (*idrv)->ref	= 1;

    (*idrv)->wDevID	= wDevID;
    (*idrv)->primary	= NULL;
    return MMSYSERR_NOERROR;
}

/*======================================================================*
*                  Low level WAVE IN implementation			*
*======================================================================*/

/**************************************************************************
* 			widNotifyClient			[internal]
*/
static DWORD widNotifyClient(WINE_WAVEIN* wwi, WORD wMsg, DWORD dwParam1, DWORD dwParam2)
{
   TRACE("wMsg = 0x%04x dwParm1 = %04lX dwParam2 = %04lX\n", wMsg, dwParam1, dwParam2);

   switch (wMsg) {
   case WIM_OPEN:
   case WIM_CLOSE:
   case WIM_DATA:
       if (wwi->wFlags != DCB_NULL &&
	   !DriverCallback(wwi->waveDesc.dwCallback, wwi->wFlags, (HDRVR)wwi->waveDesc.hWave,
			   wMsg, wwi->waveDesc.dwInstance, dwParam1, dwParam2)) {
	   WARN("can't notify client !\n");
	   return MMSYSERR_ERROR;
       }
       break;
   default:
       FIXME("Unknown callback message %u\n", wMsg);
       return MMSYSERR_INVALPARAM;
   }
   return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			widGetDevCaps				[internal]
 */
static DWORD widGetDevCaps(WORD wDevID, LPWAVEINCAPSA lpCaps, DWORD dwSize)
{
    TRACE("(%u, %p, %lu);\n", wDevID, lpCaps, dwSize);

    if (lpCaps == NULL) return MMSYSERR_NOTENABLED;

    if (wDevID >= MAX_WAVEINDRV) {
	TRACE("MAX_WAVOUTDRV reached !\n");
	return MMSYSERR_BADDEVICEID;
    }

    memcpy(lpCaps, &WInDev[wDevID].caps, min(dwSize, sizeof(*lpCaps)));
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widRecorder_ReadHeaders		[internal]
 */
static void widRecorder_ReadHeaders(WINE_WAVEIN * wwi)
{
    enum win_wm_message tmp_msg;
    DWORD		tmp_param;
    HANDLE		tmp_ev;
    WAVEHDR*		lpWaveHdr;

    while (ALSA_RetrieveRingMessage(&wwi->msgRing, &tmp_msg, &tmp_param, &tmp_ev)) {
        if (tmp_msg == WINE_WM_HEADER) {
	    LPWAVEHDR*	wh;
	    lpWaveHdr = (LPWAVEHDR)tmp_param;
	    lpWaveHdr->lpNext = 0;

	    if (wwi->lpQueuePtr == 0)
		wwi->lpQueuePtr = lpWaveHdr;
	    else {
	        for (wh = &(wwi->lpQueuePtr); *wh; wh = &((*wh)->lpNext));
	        *wh = lpWaveHdr;
	    }
	} else {
            ERR("should only have headers left\n");
        }
    }
}

/**************************************************************************
 * 				widRecorder			[internal]
 */
static	DWORD	CALLBACK	widRecorder(LPVOID pmt)
{
    WORD		uDevID = (DWORD)pmt;
    WINE_WAVEIN*	wwi = (WINE_WAVEIN*)&WInDev[uDevID];
    ACMSTREAMHEADER	acmHdr;
    DWORD		acmFlag = ACM_STREAMCONVERTF_START;
    WAVEHDR*		lpWaveHdr;
    DWORD		dwSleepTime;
    DWORD		bytesRead;
    LPVOID		buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wwi->dwPeriodSize);
    LPVOID              pOffset = buffer;
    enum win_wm_message msg;
    DWORD		param;
    HANDLE		ev;
    DWORD               frames_per_period;

    wwi->state = WINE_WS_STOPPED;
    wwi->dwTotalRecorded = 0;
    wwi->lpQueuePtr = NULL;

    SetEvent(wwi->hStartUpEvent);

    /* make sleep time to be # of ms to output a period */
    dwSleepTime = (1024 /*wwi-dwPeriodSize => overrun!*/ * 1000) / wwi->format.Format.nAvgBytesPerSec;
    frames_per_period = snd_pcm_bytes_to_frames(wwi->p_handle, wwi->dwPeriodSize); 
    TRACE("sleeptime=%ld ms\n", dwSleepTime);

    for (;;) {
	/* wait for dwSleepTime or an event in thread's queue */
	/* FIXME: could improve wait time depending on queue state,
	 * ie, number of queued fragments
	 */
	if (wwi->lpQueuePtr != NULL && wwi->state == WINE_WS_PLAYING)
        {
	    ssize_t periods;
	    snd_pcm_sframes_t frames;
	    ssize_t bytes;
	    snd_pcm_sframes_t read;

            lpWaveHdr = wwi->lpQueuePtr;
            /* read all the fragments accumulated so far */
	    frames = snd_pcm_avail_update(wwi->p_handle);
	    if (frames < 0)
	    {
		ALSA_XRUNRecovery (wwi->p_handle, frames);
		frames = snd_pcm_avail_update(wwi->p_handle);
		if (frames < 0)
		    goto sleep;
	    }
	    bytes = snd_pcm_frames_to_bytes(wwi->p_handle, frames);
	    TRACE("frames = %ld  bytes = %d\n", frames, bytes);
	    periods = bytes / wwi->dwPeriodSize;
            while ((periods > 0) && (wwi->lpQueuePtr))
            {
		periods--;
		bytes = wwi->dwPeriodSize;
	        TRACE("bytes = %d\n",bytes);
                if (lpWaveHdr->dwBufferLength - lpWaveHdr->dwBytesRecorded >= wwi->dwPeriodSize && !wwi->hConv)
                {
                    /* directly read fragment in wavehdr */
                    read = wwi->read(wwi->p_handle, lpWaveHdr->lpData + lpWaveHdr->dwBytesRecorded, frames_per_period);
                    if (read < 0)
                    {
                        ALSA_XRUNRecovery (wwi->p_handle, read);
                        read = wwi->read(wwi->p_handle, lpWaveHdr->lpData + lpWaveHdr->dwBytesRecorded, frames_per_period);
                        if (read < 0)
                            break;
                    }
		    bytesRead = snd_pcm_frames_to_bytes(wwi->p_handle, read);
			
                    TRACE("bytesRead=%ld (direct)\n", bytesRead);
		    if (bytesRead >= 0)
		    {
                        TRACE("buflen=%ld, rec=%ld\n",
                              lpWaveHdr->dwBufferLength,
                              lpWaveHdr->dwBytesRecorded);

			/* update number of bytes recorded in current buffer and by this device */
                        lpWaveHdr->dwBytesRecorded += bytesRead;
			wwi->dwTotalRecorded       += bytesRead;

			/* buffer is full. notify client */
			if (lpWaveHdr->dwBytesRecorded == lpWaveHdr->dwBufferLength)
			{
			    /* must copy the value of next waveHdr, because we have no idea of what
			     * will be done with the content of lpWaveHdr in callback
			     */
			    LPWAVEHDR	lpNext = lpWaveHdr->lpNext;

			    lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
			    lpWaveHdr->dwFlags |=  WHDR_DONE;

			    wwi->lpQueuePtr = lpNext;
			    widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);
			    lpWaveHdr = lpNext;
			}
                    }
                }
		else
		{
                    /* read the fragment in a local buffer */
		    read = wwi->read(wwi->p_handle, buffer, frames_per_period);
		    if (read < 0)
		    {
                        ALSA_XRUNRecovery (wwi->p_handle, read);
                        read = wwi->read(wwi->p_handle, buffer, frames_per_period);
                        if (read < 0)
                            break;
		    }
		    bytesRead = snd_pcm_frames_to_bytes(wwi->p_handle, read);
                    pOffset = buffer;

                    TRACE("bytesRead=%ld (local)\n", bytesRead);

                    if (!lpWaveHdr || wwi->state != WINE_WS_PLAYING)
                    {
                        /* this is normal in Counter-Strike when voice comms is not used */
                        TRACE("no buffers! Entire fragment dropped.\n");
                        acmFlag = ACM_STREAMCONVERTF_START;
                        continue;
                    }

                    /* copy data in client buffers */
                    while (bytesRead > 0)
                    {
                        DWORD dwToWrite = min (bytesRead, lpWaveHdr->dwBufferLength - lpWaveHdr->dwBytesRecorded);
                        DWORD dwRead, dwWritten;

                        if (wwi->hConv) { /* Use an ACM filter to convert */
                            memset(&acmHdr, 0, sizeof(acmHdr));
                            acmHdr.cbStruct = sizeof(acmHdr);
                            acmHdr.pbSrc = pOffset;
                            acmHdr.cbSrcLength = bytesRead;
                            acmHdr.pbDst = (LPBYTE)(lpWaveHdr->lpData + lpWaveHdr->dwBytesRecorded);
                            acmHdr.cbDstLength = dwToWrite;
                            acmStreamPrepareHeader(wwi->hConv, &acmHdr, 0);
                            acmStreamConvert(wwi->hConv, &acmHdr, acmFlag);
                            acmStreamUnprepareHeader(wwi->hConv, &acmHdr, 0);
                            dwRead = acmHdr.cbSrcLengthUsed;
                            dwWritten = acmHdr.cbDstLengthUsed;
                            TRACE("convert: src=%ld, dst=%ld, in=%ld, out=%ld\n",
                                  bytesRead, dwToWrite, dwRead, dwWritten);
                            acmFlag = 0;
                            if (!dwRead && !dwWritten) {
                                WARN("conversion error\n");
                                break;
                            }

                        } else { /* No ACM conversion necessary */

                            dwRead = dwWritten = min(bytesRead, dwToWrite);
                            memcpy(lpWaveHdr->lpData + lpWaveHdr->dwBytesRecorded,
                                   pOffset,
                                   dwToWrite);

                            TRACE("buflen=%ld, rec=%ld, copy=%lu\n",
                                  lpWaveHdr->dwBufferLength,
                                  lpWaveHdr->dwBytesRecorded,
                                  dwToWrite);
                        }

                        /* update number of bytes recorded in current buffer and by this device */
                        lpWaveHdr->dwBytesRecorded += dwWritten;
                        wwi->dwTotalRecorded += dwWritten;
                        bytesRead -= dwRead;
                        pOffset   += dwRead;

                        /* client buffer is full. notify client */
                        if (lpWaveHdr->dwBytesRecorded == lpWaveHdr->dwBufferLength)
                        {
			    /* must copy the value of next waveHdr, because we have no idea of what
			     * will be done with the content of lpWaveHdr in callback
			     */
			    LPWAVEHDR	lpNext = lpWaveHdr->lpNext;
			    TRACE("lpNext=%p\n", lpNext);

                            lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
                            lpWaveHdr->dwFlags |=  WHDR_DONE;

			    wwi->lpQueuePtr = lpNext;
                            widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);

			    lpWaveHdr = lpNext;
			    if (!lpNext && bytesRead) {
				/* before we give up, check for more header messages */
				while (ALSA_PeekRingMessage(&wwi->msgRing, &msg, &param, &ev))
				{
				    if (msg == WINE_WM_HEADER) {
					LPWAVEHDR hdr;
					ALSA_RetrieveRingMessage(&wwi->msgRing, &msg, &param, &ev);
					hdr = ((LPWAVEHDR)param);
					TRACE("msg = %s, hdr = %p, ev = %p\n", wodPlayerCmdString[msg - WM_USER - 1], hdr, (void *)ev);
					hdr->lpNext = 0;
					if (lpWaveHdr == 0) {
					    /* new head of queue */
					    wwi->lpQueuePtr = lpWaveHdr = hdr;
					} else {
					    /* insert buffer at the end of queue */
					    LPWAVEHDR*  wh;
					    for (wh = &(wwi->lpQueuePtr); *wh; wh = &((*wh)->lpNext));
					    *wh = hdr;
					}
				    } else
					break;
				}

				if (lpWaveHdr == 0) {
                                    /* no more buffer to copy data to, but we did read more.
                                     * what hasn't been copied will be dropped
                                     */
                                    WARN("buffer under run! %ld bytes dropped.\n", bytesRead);
                                    wwi->lpQueuePtr = NULL;
                                    acmFlag = ACM_STREAMCONVERTF_START;
                                    break;
				}
                            }
                        }
		    }
		}
            }
	}

    sleep:
        WAIT_OMR(&wwi->msgRing, dwSleepTime);

	while (ALSA_RetrieveRingMessage(&wwi->msgRing, &msg, &param, &ev))
	{
            TRACE("msg=%s param=0x%lx\n", wodPlayerCmdString[msg - WM_USER - 1], param);
	    switch (msg) {
	    case WINE_WM_PAUSING:
		wwi->state = WINE_WS_PAUSED;
                /*FIXME("Device should stop recording\n");*/
		SetEvent(ev);
		break;
	    case WINE_WM_STARTING:
		wwi->state = WINE_WS_PLAYING;
		snd_pcm_start(wwi->p_handle);
		SetEvent(ev);
		break;
	    case WINE_WM_HEADER:
		lpWaveHdr = (LPWAVEHDR)param;
		lpWaveHdr->lpNext = 0;

		/* insert buffer at the end of queue */
		{
		    LPWAVEHDR*	wh;
		    for (wh = &(wwi->lpQueuePtr); *wh; wh = &((*wh)->lpNext));
		    *wh = lpWaveHdr;
		}
		break;
	    case WINE_WM_STOPPING:
		if (wwi->state != WINE_WS_STOPPED)
		{
		    snd_pcm_drain(wwi->p_handle);

		    /* read any headers in queue */
		    widRecorder_ReadHeaders(wwi);

		    /* return current buffer to app */
		    lpWaveHdr = wwi->lpQueuePtr;
		    if (lpWaveHdr)
		    {
		        LPWAVEHDR	lpNext = lpWaveHdr->lpNext;
		        TRACE("stop %p %p\n", lpWaveHdr, lpWaveHdr->lpNext);
		        lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
		        lpWaveHdr->dwFlags |= WHDR_DONE;
		        wwi->lpQueuePtr = lpNext;
		        widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);
		    }
		}
		wwi->state = WINE_WS_STOPPED;
		SetEvent(ev);
		break;
	    case WINE_WM_RESETTING:
		if (wwi->state != WINE_WS_STOPPED)
		{
		    snd_pcm_drain(wwi->p_handle);
		}
		wwi->state = WINE_WS_STOPPED;
    		wwi->dwTotalRecorded = 0;

		/* read any headers in queue */
		widRecorder_ReadHeaders(wwi);

		/* return all buffers to the app */
		for (lpWaveHdr = wwi->lpQueuePtr; lpWaveHdr; lpWaveHdr = lpWaveHdr->lpNext) {
		    TRACE("reset %p %p\n", lpWaveHdr, lpWaveHdr->lpNext);
		    lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
		    lpWaveHdr->dwFlags |= WHDR_DONE;
                    wwi->lpQueuePtr = lpWaveHdr->lpNext;
		    widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);
		}

		wwi->lpQueuePtr = NULL;
		SetEvent(ev);
		break;
	    case WINE_WM_CLOSING:
		wwi->hThread = 0;
		wwi->state = WINE_WS_CLOSED;
		SetEvent(ev);
		HeapFree(GetProcessHeap(), 0, buffer);
		ExitThread(0);
		/* shouldn't go here */
	    case WINE_WM_UPDATE:
		SetEvent(ev);
		break;

	    default:
		FIXME("unknown message %d\n", msg);
		break;
	    }
	}
    }

    /* Close the ACM stream */
    if (wwi->hConv) {
	acmStreamClose(wwi->hConv, 0);
	wwi->hConv = 0;
    }

    ExitThread(0);
    /* just for not generating compilation warnings... should never be executed */
    return 0;
}

/**************************************************************************
 * 				widOpen				[internal]
 */
static DWORD widOpen(WORD wDevID, LPWAVEOPENDESC lpDesc, DWORD dwFlags)
{
    WINE_WAVEIN*	        wwi;
    snd_pcm_hw_params_t *       hw_params;
    snd_pcm_sw_params_t *       sw_params;
    snd_pcm_access_t            access;
    snd_pcm_format_t            format;
    int                         rate;
    unsigned int                buffer_time = 500000;
    unsigned int                period_time = 10000;
    int                         buffer_size;
    snd_pcm_uframes_t           period_size;
    int                         flags;
    snd_pcm_t *                 pcm;
    int                         err;

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_sw_params_alloca(&sw_params);

    TRACE("(%u, %p, %08lX);\n", wDevID, lpDesc, dwFlags);
    if (lpDesc == NULL) {
	WARN("Invalid Parameter !\n");
	return MMSYSERR_INVALPARAM;
    }
    if (wDevID >= MAX_WAVEOUTDRV) {
	TRACE("MAX_WAVOUTDRV reached !\n");
	return MMSYSERR_BADDEVICEID;
    }

    /* only PCM format is supported so far... */
    if (!is_format_supported(lpDesc->lpFormat))
	return WAVERR_BADFORMAT;

    if (dwFlags & WAVE_FORMAT_QUERY) {
	TRACE("Query format: tag=%04X nChannels=%d nSamplesPerSec=%ld !\n",
	     lpDesc->lpFormat->wFormatTag, lpDesc->lpFormat->nChannels,
	     lpDesc->lpFormat->nSamplesPerSec);
	return MMSYSERR_NOERROR;
    }

    wwi = &WInDev[wDevID];

    if ((dwFlags & WAVE_DIRECTSOUND) && !(wwi->caps.dwSupport & WAVECAPS_DIRECTSOUND))
	/* not supported, ignore it */
	dwFlags &= ~WAVE_DIRECTSOUND;

    if (wwi->p_handle) return MMSYSERR_ALLOCATED;

    wwi->p_handle = 0;
    flags = SND_PCM_NONBLOCK;
    if ( dwFlags & WAVE_DIRECTSOUND )
    	flags |= SND_PCM_ASYNC;

    pcm = ALSA_findSoundDevice(wwi->device, SND_PCM_STREAM_CAPTURE, dwFlags);
    
    if (pcm == NULL)
    {
        ERR("Error open: %s\n", snd_strerror(errno));
	return MMSYSERR_NOTENABLED;
    }

    wwi->wFlags = HIWORD(dwFlags & CALLBACK_TYPEMASK);

    memcpy(&wwi->waveDesc, lpDesc, sizeof(WAVEOPENDESC));
    copy_wave_format(&wwi->format.Format, lpDesc->lpFormat);

    if (wwi->format.Format.wBitsPerSample == 0) {
	WARN("Resetting zeroed wBitsPerSample\n");
	wwi->format.Format.wBitsPerSample = 8 *
	    (wwi->format.Format.nAvgBytesPerSec /
	     wwi->format.Format.nSamplesPerSec) /
	    wwi->format.Format.nChannels;
    }

    snd_pcm_hw_params_any(pcm, hw_params);

#define EXIT_ON_ERROR(f,e,txt) do \
{ \
    int err; \
    if ( (err = (f) ) < 0) \
    { \
	ERR(txt ": %s\n", snd_strerror(err)); \
	snd_pcm_close(pcm); \
	return e; \
    } \
} while(0)

    access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
    if ( ( err = snd_pcm_hw_params_set_access(pcm, hw_params, access ) ) < 0) {
        WARN("mmap not available. switching to standard write.\n");
        access = SND_PCM_ACCESS_RW_INTERLEAVED;
	EXIT_ON_ERROR( snd_pcm_hw_params_set_access(pcm, hw_params, access ), MMSYSERR_INVALPARAM, "unable to set access for playback");
	wwi->read = snd_pcm_readi;
    }
    else
	wwi->read = snd_pcm_mmap_readi;

    EXIT_ON_ERROR( snd_pcm_hw_params_set_channels(pcm, hw_params, wwi->format.Format.nChannels), WAVERR_BADFORMAT, "unable to set required channels");

    format = (wwi->format.Format.wBitsPerSample == 16) ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8;
    EXIT_ON_ERROR( snd_pcm_hw_params_set_format(pcm, hw_params, format), WAVERR_BADFORMAT, "unable to set required format");

    rate = snd_pcm_hw_params_set_rate_near(pcm, hw_params, wwi->format.Format.nSamplesPerSec, 0);
    if (rate < 0) {
	ERR("Rate %ld Hz not available for playback: %s\n", wwi->format.Format.nSamplesPerSec, snd_strerror(rate));
	snd_pcm_close(pcm);
        return WAVERR_BADFORMAT;
    }
    if (rate != wwi->format.Format.nSamplesPerSec) {
	WARN("Rate doesn't match (requested %ld Hz, got %d Hz)\n", wwi->format.Format.nSamplesPerSec, rate);
        if (MMSYSERR_NOERROR != ConfigureCaptureAndOpenDevice (wDevID, rate)) {
            ERR("Rate doesn't match (requested %ld Hz, got %d Hz), and can't force a match\n",
                wwi->format.Format.nSamplesPerSec, rate);
            snd_pcm_close(pcm);
            return WAVERR_BADFORMAT;
        }
    }
    
    EXIT_ON_ERROR( snd_pcm_hw_params_set_buffer_time_near(pcm, hw_params, buffer_time, 0), MMSYSERR_INVALPARAM, "unable to set buffer time");
    EXIT_ON_ERROR( snd_pcm_hw_params_set_period_time_near(pcm, hw_params, period_time, 0), MMSYSERR_INVALPARAM, "unable to set period time");

    EXIT_ON_ERROR( snd_pcm_hw_params(pcm, hw_params), MMSYSERR_INVALPARAM, "unable to set hw params for playback");
    
    period_size = snd_pcm_hw_params_get_period_size(hw_params, 0);
    buffer_size = snd_pcm_hw_params_get_buffer_size(hw_params);

    snd_pcm_sw_params_current(pcm, sw_params);
    EXIT_ON_ERROR( snd_pcm_sw_params_set_start_threshold(pcm, sw_params, dwFlags & WAVE_DIRECTSOUND ? INT_MAX : 1 ), MMSYSERR_ERROR, "unable to set start threshold");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_silence_size(pcm, sw_params, 0), MMSYSERR_ERROR, "unable to set silence size");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size), MMSYSERR_ERROR, "unable to set avail min");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_xfer_align(pcm, sw_params, 1), MMSYSERR_ERROR, "unable to set xfer align");
    EXIT_ON_ERROR( snd_pcm_sw_params_set_silence_threshold(pcm, sw_params, 0), MMSYSERR_ERROR, "unable to set silence threshold");
    EXIT_ON_ERROR( snd_pcm_sw_params(pcm, sw_params), MMSYSERR_ERROR, "unable to set sw params for playback");
#undef EXIT_ON_ERROR

    snd_pcm_prepare(pcm);

    if (TRACE_ON(wave))
	ALSA_TraceParameters(hw_params, sw_params, FALSE);

    /* now, we can save all required data for later use... */
    if ( wwi->hw_params )
    	snd_pcm_hw_params_free(wwi->hw_params);
    snd_pcm_hw_params_malloc(&(wwi->hw_params));
    snd_pcm_hw_params_copy(wwi->hw_params, hw_params);

    wwi->dwBufferSize = buffer_size;
    wwi->lpQueuePtr = wwi->lpPlayPtr = wwi->lpLoopPtr = NULL;
    wwi->p_handle = pcm;
    wwi->state = WINE_WS_STOPPED;

    ALSA_InitRingMessage(&wwi->msgRing);

    wwi->count = snd_pcm_poll_descriptors_count (wwi->p_handle);
    if (wwi->count <= 0) {
	ERR("Invalid poll descriptors count\n");
	return MMSYSERR_ERROR;
    }

    wwi->ufds = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY, sizeof(struct pollfd) * wwi->count);
    if (wwi->ufds == NULL) {
	ERR("No enough memory\n");
	return MMSYSERR_NOMEM;
    }
    if ((err = snd_pcm_poll_descriptors(wwi->p_handle, wwi->ufds, wwi->count)) < 0) {
	ERR("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
	return MMSYSERR_ERROR;
    }

    wwi->dwPeriodSize = period_size;
    /*if (wwi->dwFragmentSize % wwi->format.Format.nBlockAlign)
	ERR("Fragment doesn't contain an integral number of data blocks\n");
    */
    TRACE("dwPeriodSize=%lu\n", wwi->dwPeriodSize);
    TRACE("wBitsPerSample=%u, nAvgBytesPerSec=%lu, nSamplesPerSec=%lu, nChannels=%u nBlockAlign=%u!\n",
	  wwi->format.Format.wBitsPerSample, wwi->format.Format.nAvgBytesPerSec,
	  wwi->format.Format.nSamplesPerSec, wwi->format.Format.nChannels,
	  wwi->format.Format.nBlockAlign);

    if (!(dwFlags & WAVE_DIRECTSOUND)) {
	wwi->hStartUpEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	wwi->hThread = CreateThread(NULL, 0, widRecorder, (LPVOID)(DWORD)wDevID, CREATE_SUSPENDED, &(wwi->dwThreadID));
	SERVER_START_REQ( set_scheduling_mode )
	{
	    req->handle = wwi->hThread;
	    req->mode = SCDM_INTERNAL;
	    wine_server_call( req );
	}
	SERVER_END_REQ;
	SetThreadPriority(wwi->hThread, THREAD_PRIORITY_TIME_CRITICAL);
	ResumeThread(wwi->hThread);
	WaitForSingleObject(wwi->hStartUpEvent, INFINITE);
	CloseHandle(wwi->hStartUpEvent);
    } else {
	wwi->hThread = INVALID_HANDLE_VALUE;
	wwi->dwThreadID = 0;
    }
    wwi->hStartUpEvent = INVALID_HANDLE_VALUE;

    return widNotifyClient(wwi, WIM_OPEN, 0L, 0L);
}


/**************************************************************************
 * 				widClose			[internal]
 */
static DWORD widClose(WORD wDevID)
{
    DWORD		ret = MMSYSERR_NOERROR;
    WINE_WAVEIN*	wwi;

    TRACE("(%u);\n", wDevID);

    if (wDevID >= MAX_WAVEINDRV || WInDev[wDevID].p_handle == NULL) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    wwi = &WInDev[wDevID];
    if (wwi->lpQueuePtr) {
	WARN("buffers still playing !\n");
	ret = WAVERR_STILLPLAYING;
    } else {
	if (wwi->hThread != INVALID_HANDLE_VALUE) {
	    ALSA_AddRingMessage(&wwi->msgRing, WINE_WM_CLOSING, 0, TRUE);
	}
        ALSA_DestroyRingMessage(&wwi->msgRing);

	snd_pcm_hw_params_free(wwi->hw_params);
	wwi->hw_params = NULL;

        snd_pcm_close(wwi->p_handle);
	wwi->p_handle = NULL;

	ret = widNotifyClient(wwi, WIM_CLOSE, 0L, 0L);
    }

    HeapFree(GetProcessHeap(), 0, wwi->ufds);
    return ret;
}

/**************************************************************************
 * 				widAddBuffer			[internal]
 *
 */
static DWORD widAddBuffer(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    /* first, do the sanity checks... */
    if (wDevID >= MAX_WAVEINDRV || WInDev[wDevID].p_handle == NULL) {
        WARN("bad dev ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpWaveHdr->lpData == NULL || !(lpWaveHdr->dwFlags & WHDR_PREPARED))
	return WAVERR_UNPREPARED;

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    lpWaveHdr->dwFlags |= WHDR_INQUEUE;
    lpWaveHdr->dwBytesRecorded = 0;
    lpWaveHdr->lpNext = 0;

    ALSA_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_HEADER, (DWORD)lpWaveHdr, FALSE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widPrepare			[internal]
 */
static DWORD widPrepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEINDRV) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags |= WHDR_PREPARED;
    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widUnprepare			[internal]
 */
static DWORD widUnprepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEINDRV) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags &= ~WHDR_PREPARED;
    lpWaveHdr->dwFlags |= WHDR_DONE;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widStart			[internal]
 *
 */
static DWORD widStart(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    /* first, do the sanity checks... */
    if (wDevID >= MAX_WAVEINDRV || WInDev[wDevID].p_handle == NULL) {
        WARN("bad dev ID !\n");
	return MMSYSERR_BADDEVICEID;
    }
    
    ALSA_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_STARTING, 0, TRUE);

    Sleep(500);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widStop			[internal]
 *
 */
static DWORD widStop(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    /* first, do the sanity checks... */
    if (wDevID >= MAX_WAVEINDRV || WInDev[wDevID].p_handle == NULL) {
        WARN("bad dev ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    ALSA_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_STOPPING, 0, TRUE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			widReset				[internal]
 */
static DWORD widReset(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);
    if (wDevID >= MAX_WAVEINDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't reset !\n");
	return MMSYSERR_INVALHANDLE;
    }
    ALSA_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_RESETTING, 0, TRUE);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widGetPosition			[internal]
 */
static DWORD widGetPosition(WORD wDevID, LPMMTIME lpTime, DWORD uSize)
{
    int			time;
    WINE_WAVEIN*	wwi;

    FIXME("(%u, %p, %lu);\n", wDevID, lpTime, uSize);

    if (wDevID >= MAX_WAVEINDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't get pos !\n");
	return MMSYSERR_INVALHANDLE;
    }
    if (lpTime == NULL)	return MMSYSERR_INVALPARAM;

    wwi = &WInDev[wDevID];
    ALSA_AddRingMessage(&wwi->msgRing, WINE_WM_UPDATE, 0, TRUE);

    TRACE("wType=%04X !\n", lpTime->wType);
    TRACE("wBitsPerSample=%u\n", wwi->format.Format.wBitsPerSample);
    TRACE("nSamplesPerSec=%lu\n", wwi->format.Format.nSamplesPerSec);
    TRACE("nChannels=%u\n", wwi->format.Format.nChannels);
    TRACE("nAvgBytesPerSec=%lu\n", wwi->format.Format.nAvgBytesPerSec);
    FIXME("dwTotalRecorded=%lu\n",wwi->dwTotalRecorded);
    switch (lpTime->wType) {
    case TIME_BYTES:
	lpTime->u.cb = wwi->dwTotalRecorded;
	TRACE("TIME_BYTES=%lu\n", lpTime->u.cb);
	break;
    case TIME_SAMPLES:
	lpTime->u.sample = wwi->dwTotalRecorded * 8 /
	    wwi->format.Format.wBitsPerSample / wwi->format.Format.nChannels;
	TRACE("TIME_SAMPLES=%lu\n", lpTime->u.sample);
	break;
    case TIME_SMPTE:
	time = wwi->dwTotalRecorded /
	    (wwi->format.Format.nAvgBytesPerSec / 1000);
	lpTime->u.smpte.hour = time / (60 * 60 * 1000);
	time -= lpTime->u.smpte.hour * (60 * 60 * 1000);
	lpTime->u.smpte.min = time / (60 * 1000);
	time -= lpTime->u.smpte.min * (60 * 1000);
	lpTime->u.smpte.sec = time / 1000;
	time -= lpTime->u.smpte.sec * 1000;
	lpTime->u.smpte.frame = time * 30 / 1000;
	lpTime->u.smpte.fps = 30;
	TRACE("TIME_SMPTE=%02u:%02u:%02u:%02u\n",
	      lpTime->u.smpte.hour, lpTime->u.smpte.min,
	      lpTime->u.smpte.sec, lpTime->u.smpte.frame);
	break;
    default:
	FIXME("format not supported (%u) ! use TIME_MS !\n", lpTime->wType);
	lpTime->wType = TIME_MS;
    case TIME_MS:
	lpTime->u.ms = wwi->dwTotalRecorded /
	    (wwi->format.Format.nAvgBytesPerSec / 1000);
	TRACE("TIME_MS=%lu\n", lpTime->u.ms);
	break;
    }
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widGetNumDevs			[internal]
 */
static	DWORD	widGetNumDevs(void)
{
    return ALSA_WidNumDevs;
}

/**************************************************************************
 * 				widMessage (WINEALSA.@)
 */
DWORD WINAPI ALSA_widMessage(UINT wDevID, UINT wMsg, DWORD dwUser,
                             DWORD dwParam1, DWORD dwParam2)
{
    TRACE("(%u, %04X, %08lX, %08lX, %08lX);\n",
	  wDevID, wMsg, dwUser, dwParam1, dwParam2);

    switch (wMsg) {
    case DRVM_INIT:
    case DRVM_EXIT:
    case DRVM_ENABLE:
    case DRVM_DISABLE:
	/* FIXME: Pretend this is supported */
	return 0;
    case WIDM_OPEN:	 	return widOpen		(wDevID, (LPWAVEOPENDESC)dwParam1,	dwParam2);
    case WIDM_CLOSE:	 	return widClose		(wDevID);
    case WIDM_ADDBUFFER:	return widAddBuffer	(wDevID, (LPWAVEHDR)dwParam1,		dwParam2);
    case WIDM_PREPARE:	 	return widPrepare	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WIDM_UNPREPARE: 	return widUnprepare	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WIDM_GETDEVCAPS:	return widGetDevCaps	(wDevID, (LPWAVEINCAPSA)dwParam1,	dwParam2);
    case WIDM_GETNUMDEVS:	return widGetNumDevs	();
    case WIDM_GETPOS:	 	return widGetPosition	(wDevID, (LPMMTIME)dwParam1, 		dwParam2);
    case WIDM_RESET:		return widReset		(wDevID);
    case WIDM_START: 		return widStart	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WIDM_STOP: 		return widStop	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    /*case DRV_QUERYDEVICEINTERFACESIZE: return wdDevInterfaceSize       (wDevID, (LPDWORD)dwParam1);
    case DRV_QUERYDEVICEINTERFACE:     return wdDevInterface           (wDevID, (PWCHAR)dwParam1, dwParam2);
    case DRV_QUERYDSOUNDIFACE:	return widDsCreate   (wDevID, (PIDSCDRIVER*)dwParam1);
    case DRV_QUERYDSOUNDDESC:	return widDsDesc     (wDevID, (PDSDRIVERDESC)dwParam1);
    case DRV_QUERYDSOUNDGUID:	return widDsGuid     (wDevID, (LPGUID)dwParam1);*/
    default:
	FIXME("unknown message %d!\n", wMsg);
    }
    return MMSYSERR_NOTSUPPORTED;
}

#endif

#if !(defined(HAVE_ALSA) && ((SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9) || SND_LIB_MAJOR >= 1))

/**************************************************************************
 * 				widMessage (WINEALSA.@)
 */
DWORD WINAPI ALSA_widMessage(WORD wDevID, WORD wMsg, DWORD dwUser,
                             DWORD dwParam1, DWORD dwParam2)
{
    FIXME("(%u, %04X, %08lX, %08lX, %08lX):stub\n", wDevID, wMsg, dwUser, dwParam1, dwParam2);
    return MMSYSERR_NOTENABLED;
}

#endif

#ifndef HAVE_ALSA

/**************************************************************************
 * 				wodMessage (WINEALSA.7)
 */
DWORD WINAPI ALSA_wodMessage(WORD wDevID, WORD wMsg, DWORD dwUser,
                             DWORD dwParam1, DWORD dwParam2)
{
    FIXME("(%u, %04X, %08lX, %08lX, %08lX):stub\n", wDevID, wMsg, dwUser, dwParam1, dwParam2);
    return MMSYSERR_NOTENABLED;
}

#endif /* HAVE_ALSA */
