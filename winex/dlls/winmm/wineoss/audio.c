/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * Sample Wine Driver for Open Sound System (featured in Linux and FreeBSD)
 *
 * Copyright 1994 Martin Ayotte
 *           1999 Eric Pouech (async playing in waveOut/waveIn)
 *	     2000 Eric Pouech (loops in waveOut)
 *	     2002 Eric Pouech (full duplex)
 *           2003 TransGaming Technologies
 */
/*
 * FIXME:
 *	pause in waveOut does not work correctly in loop mode
 */

/*#define EMULATE_SB16*/

/* unless someone makes a wineserver kernel module, Unix pipes are faster than win32 events */
#define USE_PIPE_SYNC

/* an exact wodGetPosition is usually not worth the extra context switches,
 * as we're going to have near fragment accuracy anyway */
/* #define EXACT_WODPOSITION */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#include <sys/poll.h>
#include "windef.h"
#include "wingdi.h"
#include "winreg.h"
#include "winerror.h"
#include "winbase.h"
#include "wine/winuser16.h"
#include "mmddk.h"
#include "msacm.h"
#include "dsound.h"
#include "dsdriver.h"
#include "wine/server.h"
#include "oss.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wave);

/* Allow 2% deviation for sample rates */
#define NEAR_MATCH(rate1,rate2) (((50*((int)(rate1)-(int)(rate2)))/(rate1))==0)

#ifdef HAVE_OSS

#define MAX_WAVEDRV 	(3)

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
    WINE_WM_UPDATE, WINE_WM_BREAKLOOP, WINE_WM_CLOSING
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
} OSS_MSG;

/* implement an in-process message ring for better performance
 * (compared to passing thru the server)
 * this ring will be used by the input (resp output) record (resp playback) routine
 */
typedef struct {
#define OSS_RING_BUFFER_SIZE	128
    int                         entries; /* # of entries in the ring */
    OSS_MSG			* messages;
    int				msg_tosave;
    int				msg_toget;
#ifdef USE_PIPE_SYNC
    int				msg_pipe[2];
#endif
    HANDLE			msg_event;
    CRITICAL_SECTION		msg_crst;
} OSS_MSG_RING;

typedef struct tagOSS_DEVICE {
    const char*                 dev_name;
    const char*                 mixer_name;
    unsigned                    open_count;
    WAVEOUTCAPSA                out_caps;
    WAVEINCAPSA		        in_caps;
    unsigned                    open_access;
    int                         fd;
    DWORD                       owner_tid;
    unsigned                    sample_rate;
    unsigned                    stereo;
    unsigned                    format;
    int                         audio_fragment;
    BOOL                        full_duplex, trigger;
    int                         rec_enable, enable;
    unsigned                    map_rate, map_format;
    DWORD                       dwDsOutCaps;

    /* This needs to be taken anywhere that the output thread can change
       something from under the capture thread's feet. Note that the assumption
       is that the capture thread is a 2nd class citizen, and won't do
       the same to the output thread */
    CRITICAL_SECTION            recCritSec;
} OSS_DEVICE;

static OSS_DEVICE   OSS_Devices[MAX_WAVEDRV];

typedef struct {
    OSS_DEVICE*                 ossdev;
    volatile int		state;			/* one of the WINE_WS_ manifest constants */
    WAVEOPENDESC		waveDesc;
    WORD			wFlags;
    WAVEFORMATEXTENSIBLE	format;

    /* OSS information */
    DWORD			dwFragmentSize;		/* size of OSS buffer fragment */
    DWORD                       dwBufferSize;           /* size of whole OSS buffer in bytes */
    LPWAVEHDR			lpQueuePtr;		/* start of queued WAVEHDRs (waiting to be notified) */
    LPWAVEHDR			lpPlayPtr;		/* start of not yet fully played buffers */
    DWORD			dwPartialOffset;	/* Offset of not yet written bytes in lpPlayPtr */

    LPWAVEHDR			lpLoopPtr;              /* pointer of first buffer in loop, if any */
    DWORD			dwLoops;		/* private copy of loop counter */

    DWORD			dwPlayedTotal;		/* number of bytes actually played since opening */
    DWORD                       dwWrittenTotal;         /* number of bytes written to OSS buffer since opening */
    BOOL                        bNeedPost;              /* whether audio still needs to be physically started */

    /* synchronization stuff */
    HANDLE			hStartUpEvent;
    HANDLE			hThread;
    DWORD			dwThreadID;
    OSS_MSG_RING		msgRing;

    /* DirectSound stuff */
    LPBYTE			mapping;
    DWORD			maplen;
} WINE_WAVEOUT;

typedef struct {
    OSS_DEVICE*                 ossdev;
    volatile int		state;
    DWORD			dwFragmentSize;		/* OpenSound '/dev/dsp' give us that size */
    WAVEOPENDESC		waveDesc;
    WORD			wFlags;
    WAVEFORMATEXTENSIBLE	format;
    HACMSTREAM                  hConv;
    LPWAVEHDR			lpQueuePtr;
    DWORD			dwTotalRecorded;
    BOOL                        bTriggerSupport;

    /* synchronization stuff */
    HANDLE			hThread;
    DWORD			dwThreadID;
    HANDLE			hStartUpEvent;
    OSS_MSG_RING		msgRing;
} WINE_WAVEIN;

static WINE_WAVEOUT	WOutDev   [MAX_WAVEDRV];
static WINE_WAVEIN	WInDev    [MAX_WAVEDRV];
static unsigned         numOutDev;
static unsigned         numInDev;

#ifdef USE_PIPE_SYNC
static BOOL             use_pipes;
#endif

static DWORD wodDsCreate(UINT wDevID, PIDSDRIVER* drv);

/* These strings used only for tracing */
static const char *wodPlayerCmdString[] = {
    "WINE_WM_PAUSING",
    "WINE_WM_RESTARTING",
    "WINE_WM_RESETTING",
    "WINE_WM_HEADER",
    "WINE_WM_UPDATE",
    "WINE_WM_BREAKLOOP",
    "WINE_WM_CLOSING",
};

/*======================================================================*
 *                  Low level WAVE implementation			*
 *======================================================================*/

/******************************************************************
 *		OSS_RawOpenDevice
 *
 * Low level device opening (from values stored in ossdev)
 */
static int      OSS_RawOpenDevice(OSS_DEVICE* ossdev, int* frag,
                                  BOOL bForceParms)
{
    int fd, val;

    TRACE("Opening %s\n", debugstr_a(ossdev->dev_name));

    if ((fd = open(ossdev->dev_name, ossdev->open_access|O_NDELAY, 0)) == -1)
    {
        int errsave = errno; /* save this in case the warn stamps it */
        WARN("Couldn't open out %s (%s)\n", ossdev->dev_name, strerror(errno));
        errno = errsave;
        return -1;
    }
    fcntl(fd, F_SETFD, 1); /* set close on exec flag */
    /* turn full duplex on if it has been requested */
    if (ossdev->open_access == O_RDWR && ossdev->full_duplex)
        ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0);

    if (ossdev->audio_fragment)
    {
        ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &ossdev->audio_fragment);
    }

    /* First size and stereo then samplerate */
    if (ossdev->format)
    {
        val = ossdev->format;
        if ((ioctl(fd, SNDCTL_DSP_SETFMT, &val) != 0) ||
          ((val != ossdev->format) && bForceParms)) {
            ERR("Can't set format to %d (%d)\n", ossdev->format, val);
            goto error;
        }
    }
    if (ossdev->stereo != -1)
    {
        val = ossdev->stereo;
        if ((ioctl(fd, SNDCTL_DSP_STEREO, &val) != 0) ||
          ((val != ossdev->stereo) && bForceParms)) {
            ERR("Can't set stereo to %u (%d)\n", ossdev->stereo, val);
            goto error;
        }
    }
    if (ossdev->sample_rate)
    {
        val = ossdev->sample_rate;
        if ((ioctl(fd, SNDCTL_DSP_SPEED, &ossdev->sample_rate) != 0) ||
          ((!NEAR_MATCH(val, ossdev->sample_rate)) && bForceParms)) {
            ERR("Can't set sample_rate to %u (%d)\n", ossdev->sample_rate, val);
            goto error;
        }
    }
    return fd;

error:

    close( fd );
    return -1;
}

/******************************************************************
 *		OSS_OpenDevice
 *
 * since OSS has poor capabilities in full duplex, we try here to let a program
 * open the device for both waveout and wavein streams...
 * this is hackish, but it's the way OSS interface is done...
 *
 * bForceParms is TRUE if we're not allowed to deviate from the provided sample_rate,
 * stereo and fmt settings. This may cause failures if the underlying hardware doesn't
 * support these settings.
 */
static DWORD	OSS_OpenDevice(OSS_DEVICE* ossdev, unsigned req_access,
                               int* frag, int sample_rate, int stereo, int fmt,
                               BOOL bForceParms)
{
    TRACE( "(count:%d) -> %u, %p, %d, %d, %d\n",
             ossdev->open_count, req_access, frag, sample_rate, stereo, fmt );

    if (ossdev->full_duplex && (req_access == O_RDONLY || req_access == O_WRONLY))
        req_access = O_RDWR;

    /* FIXME: this should be protected, and it also contains a race with OSS_CloseDevice */
    if (ossdev->open_count == 0)
    {
        if (access(ossdev->dev_name, 0) != 0) return MMSYSERR_NODRIVER;

        ossdev->enable = PCM_ENABLE_OUTPUT | PCM_ENABLE_INPUT;
        ossdev->audio_fragment = (frag) ? *frag : 0;
        ossdev->sample_rate = sample_rate;
        ossdev->stereo = stereo;
        ossdev->format = fmt;
        ossdev->rec_enable = 0;
        ossdev->open_access = req_access;
        ossdev->owner_tid = GetCurrentThreadId();

        if ((ossdev->fd = OSS_RawOpenDevice(ossdev, frag, bForceParms)) == -1)
            return MMSYSERR_ERROR;
    }
    else
    {
        /* check we really open with the same parameters */
        if (ossdev->open_access != req_access)
        {
            WARN("Mismatch in access...\n");
            return WAVERR_BADFORMAT;
        }
        if (ossdev->audio_fragment != (frag ? *frag : 0) ||
            ossdev->sample_rate != sample_rate ||
            ossdev->stereo != stereo ||
            ossdev->format != fmt)
        {
            WARN("FullDuplex: mismatch in PCM parameters for input and output\n"
                 "OSS doesn't allow us different parameters\n"
                 "audio_frag(%x/%x) sample_rate(%d/%d) stereo(%d/%d) fmt(%d/%d)\n",
                 ossdev->audio_fragment, frag ? *frag : 0,
                 ossdev->sample_rate, sample_rate,
                 ossdev->stereo, stereo,
                 ossdev->format, fmt);
            return WAVERR_BADFORMAT;
        }
        if (GetCurrentThreadId() != ossdev->owner_tid)
        {
            WARN("Another thread is trying to access audio...\n");
#if 0 /* we may not be 100% threadsafe, but this shouldn't be an error */
            return MMSYSERR_ERROR;
#endif
        }
    }

    ossdev->open_count++;

    return MMSYSERR_NOERROR;
}

/******************************************************************
 *		OSS_CloseDevice
 *
 *
 */
static void	OSS_CloseDevice(OSS_DEVICE* ossdev)
{
    if (--ossdev->open_count == 0) close(ossdev->fd);
}

/******************************************************************
 *		OSS_ResetDevice
 *
 * Resets the device. OSS Commercial requires the device to be closed
 * after a SNDCTL_DSP_RESET ioctl call... this function implements
 * this behavior...
 *
 * NOTE - you should be holding ossdev->recCritSec while calling
 * this function!
 */
static int     OSS_ResetDevice(OSS_DEVICE* ossdev, BOOL bForceParms)
{
    int retry = 0;
    if (ioctl(ossdev->fd, SNDCTL_DSP_RESET, NULL) == -1)
    {
	perror("ioctl SNDCTL_DSP_RESET");
        return -1;
    }
    TRACE("Changing fd from %d to ", ossdev->fd);
    close(ossdev->fd);
    ossdev->fd = OSS_RawOpenDevice(ossdev, &ossdev->audio_fragment,
                                   bForceParms);
    while ((ossdev->fd == -1) && (retry < 10) && 
           ((errno == EAGAIN) || (errno == EBUSY) || (errno == EINTR)))
    {
        Sleep(1);
        retry++;
        WARN("retry %i..\n", retry);
        ossdev->fd = OSS_RawOpenDevice(ossdev, &ossdev->audio_fragment,
                                       bForceParms);
    }
    TRACE("%d\n", ossdev->fd);
    if (ossdev->rec_enable)
    {
        int val = ossdev->enable & PCM_ENABLE_OUTPUT;
        ioctl(ossdev->fd, SNDCTL_DSP_SETTRIGGER, &val);
        if (val != ossdev->enable)
            ioctl(ossdev->fd, SNDCTL_DSP_SETTRIGGER, &ossdev->enable);
    }
    return ossdev->fd;
}

/******************************************************************
 *		OSS_TriggerDevice
 *
 *
 */
static void	OSS_TriggerDevice(OSS_DEVICE* ossdev, int s, int c)
{
    int enable;

    /* FIXME: threadsafety */
    enable = (ossdev->enable & ~c) | s;
    ossdev->enable = enable;
    TRACE("(set=%d, clear=%d, result=%d)\n", s, c, enable);
    if (ioctl(ossdev->fd, SNDCTL_DSP_SETTRIGGER, &enable) < 0)
    {
        ERR("ioctl(SNDCTL_DSP_SETTRIGGER) failed (%d)\n", errno);
    }
}

/******************************************************************
 *		OSS_WaveMapTest
 */
static BOOL     OSS_WaveMapTest(OSS_DEVICE* ossdev, int sample_rate, int stereo, int fmt)
{
    size_t maplen;
    audio_buf_info info;
    void *mapping;
    BOOL ret = FALSE;

    if (OSS_OpenDevice(ossdev, O_RDWR, NULL, sample_rate, stereo, fmt, TRUE) != 0) return FALSE;
    if (!NEAR_MATCH(ossdev->sample_rate, sample_rate)) goto error;
    if (ioctl(ossdev->fd, SNDCTL_DSP_GETOSPACE, &info) < 0) goto error;
    maplen = info.fragstotal * info.fragsize;
    mapping = mmap(NULL, maplen, PROT_WRITE, MAP_SHARED, ossdev->fd, 0);
    if (mapping == (void*)-1) goto error;
    TRACE("Success: %d Hz\n", sample_rate);
    ret = TRUE; /* success */
    munmap(mapping, maplen);

error:
    if (!ret)
        TRACE("Failure: %d Hz\n", sample_rate);
    OSS_CloseDevice(ossdev);
    return ret;
}


#define NUM_SAMPLE_RATES 5
#define NUM_SAMPLE_FRMTS 2
#define NUM_STEREO_MODES 2

static const int sample_rates [ NUM_SAMPLE_RATES ] = { 96000, 48000, 44100, 22050, 11025 };
static const int sample_format[ NUM_SAMPLE_FRMTS ] = { AFMT_U8, AFMT_S16_LE };
static const int stereo_modes [ NUM_STEREO_MODES ] = { WINE_OSS_MODE_MONO, WINE_OSS_MODE_STEREO };

static const int wave_caps[ NUM_SAMPLE_RATES ][ NUM_SAMPLE_FRMTS ][ NUM_STEREO_MODES ] =
   { { { WAVE_FORMAT_96M08, WAVE_FORMAT_96S08 }, { WAVE_FORMAT_96M16, WAVE_FORMAT_96S16 } },
     { { WAVE_FORMAT_48M08, WAVE_FORMAT_48S08 }, { WAVE_FORMAT_48M16, WAVE_FORMAT_48S16 } },
     { { WAVE_FORMAT_4M08 , WAVE_FORMAT_4S08  }, { WAVE_FORMAT_4M16 , WAVE_FORMAT_4S16  } },
     { { WAVE_FORMAT_2M08 , WAVE_FORMAT_2S08  }, { WAVE_FORMAT_2M16 , WAVE_FORMAT_2S16  } },
     { { WAVE_FORMAT_1M08 , WAVE_FORMAT_1S08  }, { WAVE_FORMAT_1M16 , WAVE_FORMAT_1S16  } }
   };

static void OSS_CheckFormats( int dev, int format_mask, LPDWORD lpdwFormats, LPDWORD lpdwDsCaps )
{
    int i,j,k;    
    int	smplrate;

    /* Assume all caps are supported unless we figure out otherwise */
    *lpdwDsCaps = DSCAPS_PRIMARYMONO | DSCAPS_PRIMARYSTEREO | DSCAPS_PRIMARY8BIT | DSCAPS_PRIMARY16BIT;
    
    /* Interestingly the OSS API says that one should set the rate after the stereo since the
     * mono and stero rates may be different.
     */
    for( j = 0; j < NUM_SAMPLE_FRMTS; j++ )
    {
      if( !(format_mask & sample_format[j]) )
      {
         if( sample_format[j] == AFMT_U8 )
         {
            TRACE( "Reporting no 8 bit to dsound\n" );
            *lpdwDsCaps &= ~DSCAPS_PRIMARY8BIT;
         }
         else
         {
            TRACE( "Reporting no 16 bit to dsound\n" );
            *lpdwDsCaps &= ~DSCAPS_PRIMARY16BIT;
         }
         continue;
      }

      for( k = 0; k < NUM_STEREO_MODES; k++ )
      {
        int arg = stereo_modes[k];

        if( ioctl(dev, SNDCTL_DSP_STEREO, &arg) != 0 ) continue;
        if( arg != stereo_modes[k] )
        {
          if( stereo_modes[k] == WINE_OSS_MODE_MONO )
          {
            TRACE( "Reporting no mono to dsound\n" );
            *lpdwDsCaps &= ~DSCAPS_PRIMARYMONO; 
          }
          else
          {
            TRACE( "Reporting no stereo to dsound\n" );
            *lpdwDsCaps &= ~DSCAPS_PRIMARYSTEREO;
          }
          continue;
        }

        for( i = 0; i < NUM_SAMPLE_RATES; i++ )
        {
          smplrate = sample_rates[i];
      
          if (ioctl(dev, SNDCTL_DSP_SPEED, &smplrate) != 0) continue;

          /* Check the returned rate, deviation of a few % is supposedly not audable. */
          if( !NEAR_MATCH( smplrate, sample_rates[i] ) ) 
	  {
            TRACE( "Reporting sample rate %i unavailable, using closest rate %i\n", sample_rates[i], smplrate );
            continue;
	  }
	  else
	  {
	    TRACE( "Reporting sample rate %i available\n", smplrate );
	  }
	   
          /* Seems to be supported */
          *lpdwFormats |= wave_caps[ i ][ j ][ k ];
        } 
      }
    }
}


/******************************************************************
 *		OSS_WaveOutInit
 *
 *
 */
static void     OSS_WaveOutInit(unsigned int devID, OSS_DEVICE* ossdev, BOOL use_mmap)
{
    int	                samplesize = 16;
    int	                dsp_stereo = 1;
    int	                bytespersmpl;
    int                 caps;
    int                 mask;
    DWORD               dwReturn;


    WOutDev[devID].state = WINE_WS_CLOSED;
    WOutDev[devID].ossdev = ossdev;
    memset(&ossdev->out_caps, 0, sizeof(ossdev->out_caps));

    dwReturn = OSS_OpenDevice(WOutDev[devID].ossdev, O_WRONLY, NULL, 0, -1, 0, TRUE);
    if ( dwReturn != 0)
    {
      if (devID)
        WARN( "OpenDevice failed (%ld) on device %d\n", dwReturn, devID );
      else
        ERR( "OpenDevice failed (%ld)\n", dwReturn );
      return;
    }
    numOutDev++;

    ioctl(ossdev->fd, SNDCTL_DSP_RESET, 0);

    /* FIXME: some programs compare this string against the content of the registry
     * for MM drivers. The names have to match in order for the program to work
     * (e.g. MS win9x mplayer.exe)
     */
#ifdef EMULATE_SB16
    ossdev->out_caps.wPid = 0x0104;
    strcpy(ossdev->out_caps.szPname, "SB16 Wave Out");
#else
    ossdev->out_caps.wMid = 0x00FF; 	/* Manufac ID */
    ossdev->out_caps.wPid = 0x0001; 	/* Product ID */
    /*    strcpy(ossdev->out_caps.szPname, "OpenSoundSystem WAVOUT Driver");*/
    strcpy(ossdev->out_caps.szPname, "CS4236/37/38");
#endif

    ossdev->out_caps.vDriverVersion = 0x0100;
    ossdev->out_caps.dwFormats = 0x00000000;
    ossdev->out_caps.dwSupport = WAVECAPS_VOLUME;

    ioctl(ossdev->fd, SNDCTL_DSP_GETFMTS, &mask);
    TRACE("OSS dsp out mask=%08x\n", mask);

    /* First bytespersampl, then stereo */
    bytespersmpl = (ioctl(ossdev->fd, SNDCTL_DSP_SAMPLESIZE, &samplesize) != 0) ? 1 : 2;

    /* Num channels available */
    dsp_stereo = WINE_OSS_MODE_STEREO;
    if( ( ioctl(ossdev->fd, SNDCTL_DSP_STEREO, &dsp_stereo) != -1 ) &&
        ( dsp_stereo == WINE_OSS_MODE_STEREO ) )
    {
      ossdev->out_caps.wChannels = 2;
    }
    else
    {
      ossdev->out_caps.wChannels = 1;
    }

    if (ossdev->out_caps.wChannels > 1) ossdev->out_caps.dwSupport |= WAVECAPS_LRVOLUME;
    
    
    OSS_CheckFormats( ossdev->fd, mask, &ossdev->out_caps.dwFormats, &ossdev->dwDsOutCaps );

    TRACE("OSS DS out caps=0x%08lx\n", ossdev->dwDsOutCaps );

    if (ioctl(ossdev->fd, SNDCTL_DSP_GETCAPS, &caps) == 0) {
	TRACE("OSS dsp out caps=0x%08X\n", caps);
	if ((caps & DSP_CAP_REALTIME) && !(caps & DSP_CAP_BATCH)) {
	    ossdev->out_caps.dwSupport |= WAVECAPS_SAMPLEACCURATE;
	}

	/* well, might as well use the DirectSound cap flag for something */
	if ((caps & DSP_CAP_TRIGGER) && (caps & DSP_CAP_MMAP) &&
	    !(caps & DSP_CAP_BATCH) && use_mmap)
	    ossdev->out_caps.dwSupport |= WAVECAPS_DIRECTSOUND;
    }

    OSS_CloseDevice(ossdev);
    TRACE("out dwFormats = %08lX, dwSupport = %08lX\n",
	  ossdev->out_caps.dwFormats, ossdev->out_caps.dwSupport);
}

/******************************************************************
 *		OSS_WaveInInit
 *
 *
 */
static void OSS_WaveInInit(unsigned devID, OSS_DEVICE* ossdev)
{
    int	                samplesize = 16;
    int	                dsp_stereo = WINE_OSS_MODE_STEREO;
    int                 caps;
    int                 mask;
    DWORD               dwJunk;

    samplesize = 16;
    dsp_stereo = 1;

    WInDev[devID].state = WINE_WS_CLOSED;
    WInDev[devID].ossdev = ossdev;

    memset(&ossdev->in_caps, 0, sizeof(ossdev->in_caps));

    if (OSS_OpenDevice(WInDev[devID].ossdev, O_RDONLY, NULL, 0, -1, 0, TRUE) != 0) return;
    numInDev++;

    ioctl(ossdev->fd, SNDCTL_DSP_RESET, 0);

#ifdef EMULATE_SB16
    ossdev->in_caps.wMid = 0x0002;
    ossdev->in_caps.wPid = 0x0004;
    strcpy(ossdev->in_caps.szPname, "SB16 Wave In");
#else
    ossdev->in_caps.wMid = 0x00FF; 	/* Manufac ID */
    ossdev->in_caps.wPid = 0x0001; 	/* Product ID */
    strcpy(ossdev->in_caps.szPname, "OpenSoundSystem WAVIN Driver");
#endif
    ossdev->in_caps.dwFormats = 0x00000000;
    ossdev->in_caps.wChannels = (ioctl(ossdev->fd, SNDCTL_DSP_STEREO, &dsp_stereo) != 0) ? 1 : 2;

    WInDev[devID].bTriggerSupport = FALSE;
    if (ioctl(ossdev->fd, SNDCTL_DSP_GETCAPS, &caps) == 0) {
	TRACE("OSS dsp in caps=%08X\n", caps);
	if (caps & DSP_CAP_TRIGGER)
            WInDev[devID].bTriggerSupport = TRUE;
    }

    ioctl(ossdev->fd, SNDCTL_DSP_GETFMTS, &mask);
    TRACE("OSS in dsp mask=%08x\n", mask);
    
    OSS_CheckFormats( ossdev->fd, mask, &ossdev->in_caps.dwFormats, &dwJunk );

    OSS_CloseDevice(ossdev);
    TRACE("in dwFormats = %08lX\n", ossdev->in_caps.dwFormats);
}

/******************************************************************
 *		OSS_WaveMapInit
 *
 *
 */
static void OSS_WaveMapInit(OSS_DEVICE* ossdev)
{
    int stereo, fmt;
    BOOL s16, s08, m48000, m44100, m22050, m11025;

    ossdev->map_rate = 0;
    ossdev->map_format = 0;
    /* check for DirectSound capability */
    if (ossdev->out_caps.dwSupport & WAVECAPS_DIRECTSOUND) {

	stereo = (ossdev->out_caps.wChannels > 1) ? 1 : 0;
	s16 = (ossdev->out_caps.dwFormats & (WAVE_FORMAT_1M16 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4M16 |
					     WAVE_FORMAT_1S16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_4S16 |
					     WAVE_FORMAT_48M16 | WAVE_FORMAT_96M16 |
					     WAVE_FORMAT_48S16 | WAVE_FORMAT_96M16)) != 0;
	s08 = (ossdev->out_caps.dwFormats & (WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4M08 |
					     WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 |
					     WAVE_FORMAT_48M08 | WAVE_FORMAT_96M08 |
					     WAVE_FORMAT_48S08 | WAVE_FORMAT_96S08)) != 0;
	fmt = s16 ? 16 : 8;

	/* well, might as well use the DirectSound cap flag for something */
	ossdev->out_caps.dwSupport |= WAVECAPS_DIRECTSOUND;

	/* check which sample rates allow mmap */
	m48000 = OSS_WaveMapTest(ossdev, 48000, stereo, fmt);
	m44100 = OSS_WaveMapTest(ossdev, 44100, stereo, fmt);
	m22050 = OSS_WaveMapTest(ossdev, 22050, stereo, fmt);
	m11025 = OSS_WaveMapTest(ossdev, 11025, stereo, fmt);
	if (m48000) {
	    /* many ac97 chips can only do 48kHz */
	    if (!m44100 && !m22050 && !m11025)
		ossdev->map_rate = 48000;
	    /* most ac97 chips can only do 16 bit */
	    if (fmt == 16) {
		if (!s08 || !OSS_WaveMapTest(ossdev, 48000, stereo, 8))
		    ossdev->map_format = fmt;
	    }
	}
	else {
	    /* make sure we can do mmap at all */
	    if (!m44100 && !m22050 && !m11025)
		ossdev->out_caps.dwSupport &= ~WAVECAPS_DIRECTSOUND;
	}
    }
    TRACE("map sample rate = %d, format = %d\n", ossdev->map_rate, ossdev->map_format);
}

/******************************************************************
 *		OSS_WaveFullDuplexInit
 *
 *
 */
static void OSS_WaveFullDuplexInit(OSS_DEVICE* ossdev)
{
    int 	caps;

    if (OSS_OpenDevice(ossdev, O_RDWR, NULL, 0, -1, 0, TRUE) != 0) return;
    if (ioctl(ossdev->fd, SNDCTL_DSP_GETCAPS, &caps) == 0)
    {
	ossdev->full_duplex = (caps & DSP_CAP_DUPLEX);
	ossdev->trigger = (caps & DSP_CAP_TRIGGER);
    }
    OSS_CloseDevice(ossdev);
}

#define IS_OPTION_TRUE(ch) \
    ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')
#define IS_OPTION_FALSE(ch) \
    ((ch) == 'n' || (ch) == 'N' || (ch) == 'f' || (ch) == 'F' || (ch) == '0')

/******************************************************************
 *		OSS_WaveInit
 *
 * Initialize internal structures from OSS information
 */
LONG OSS_WaveInit(void)
{
    int 	i;
    HKEY	hkey;
    BYTE	buffer[16];
    DWORD	count;
    BOOL	use_mmap = FALSE;
    BOOL	full_duplex = FALSE;

#ifdef USE_PIPES
    use_pipes = !server_scheduler_active();
#endif

    /* FIXME: only one device is supported */
    memset(&OSS_Devices, 0, sizeof(OSS_Devices));
    /* FIXME: should check that dsp actually points to dsp0, or that dsp0 exists
     * we should also be able to configure (bitmap) which devices we want to use...
     * - or even store the name of all drivers in our configuration
     */
    OSS_Devices[0].dev_name   = "/dev/dsp";
    OSS_Devices[0].mixer_name = "/dev/mixer";
    OSS_Devices[1].dev_name   = "/dev/dsp1";
    OSS_Devices[1].mixer_name = "/dev/mixer1";
    OSS_Devices[2].dev_name   = "/dev/dsp2";
    OSS_Devices[2].mixer_name = "/dev/mixer2";

    if (!RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\wineoss", 0, NULL,
                         REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL)) {
        count = sizeof(buffer);
        if (!RegQueryValueExA(hkey, "UseMMap", 0, NULL, buffer, &count)) {
            use_mmap = IS_OPTION_TRUE(buffer[0]);
        }
        count = sizeof(buffer);
        if (!RegQueryValueExA(hkey, "FullDuplex", 0, NULL, buffer, &count)) {
            full_duplex = IS_OPTION_TRUE(buffer[0]);
        }
        for (i=0; i<MAX_WAVEDRV; i++) {
            char key[16];
            sprintf(key, "dsp%d", i);
            count = sizeof(buffer);
            if (!RegQueryValueExA(hkey, key, 0, NULL, buffer, &count)) {
                char *st = malloc(count+1);
                memcpy(st, buffer, count);
                st[count] = 0;
                OSS_Devices[i].dev_name = st;
            }
            sprintf(key, "mixer%d", i);
            count = sizeof(buffer);
            if (!RegQueryValueExA(hkey, key, 0, NULL, buffer, &count)) {
                char *st = malloc(count+1);
                memcpy(st, buffer, count);
                st[count] = 0;
                OSS_Devices[i].mixer_name = st;
            }
        }
        RegCloseKey(hkey);
    }

    /* start with output device, starting at 0 which is the default playback device */
    for (i = 0; i < MAX_WAVEDRV; ++i)
        OSS_WaveOutInit(i, &OSS_Devices[i], use_mmap);

    /* then do input device */
    for (i = 0; i < MAX_WAVEDRV; ++i)
        OSS_WaveInInit(i, &OSS_Devices[i]);

    /* check directsound */
    for (i = 0; i < MAX_WAVEDRV; ++i)
        OSS_WaveMapInit(&OSS_Devices[i]);

    if (full_duplex) {
        /* finish with the full duplex bits */
        for (i = 0; i < MAX_WAVEDRV; i++)
            OSS_WaveFullDuplexInit(&OSS_Devices[i]);
    }

    for (i = 0; i < MAX_WAVEDRV; i++)
        InitializeCriticalSection (&OSS_Devices[i].recCritSec);

    return 0;
}

/******************************************************************
 *		OSS_WaveFini
 *
 * Clean up internal OSS structures
 */
void OSS_WaveFini (void)
{
    int i;

    for (i = 0; i < MAX_WAVEDRV; i++)
        DeleteCriticalSection (&OSS_Devices[i].recCritSec);
}


/******************************************************************
 *		OSS_InitRingMessage
 *
 * Initialize the ring of messages for passing between driver's caller and playback/record
 * thread
 */
static int OSS_InitRingMessage(OSS_MSG_RING* omr)
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
    omr->entries = OSS_RING_BUFFER_SIZE;
    omr->messages = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, omr->entries * sizeof(OSS_MSG));
    InitializeCriticalSection(&omr->msg_crst);
    return 0;
}

/******************************************************************
 *		OSS_DestroyRingMessage
 *
 */
static int OSS_DestroyRingMessage(OSS_MSG_RING* omr)
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
 *		OSS_AddRingMessage
 *
 * Inserts a new message into the ring (should be called from DriverProc derivated routines)
 */
static int OSS_AddRingMessage(OSS_MSG_RING* omr, enum win_wm_message msg, DWORD param, BOOL wait)
{
    HANDLE	hEvent = INVALID_HANDLE_VALUE;

    EnterCriticalSection(&omr->msg_crst);
    if ((omr->msg_toget == ((omr->msg_tosave + 1) % omr->entries))) /* buffer overflow */
    {
        omr->entries += OSS_RING_BUFFER_SIZE;
        omr->messages = HeapReAlloc(GetProcessHeap(), 0, omr->messages, omr->entries * sizeof(OSS_MSG));
        TRACE("Growing ring buffer to %d\n", omr->entries);
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
 *		OSS_RetrieveRingMessage
 *
 * Get a message from the ring. Should be called by the playback/record thread.
 */
static int OSS_RetrieveRingMessage(OSS_MSG_RING* omr,
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
	    !DriverCallback(wwo->waveDesc.dwCallback, wwo->wFlags,
			    (HDRVR)wwo->waveDesc.hWave, wMsg,
			    wwo->waveDesc.dwInstance, dwParam1, dwParam2)) {
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
static BOOL wodUpdatePlayedTotal(WINE_WAVEOUT* wwo, audio_buf_info* info)
{
    audio_buf_info dspspace;
    if (!info) info = &dspspace;

    if (ioctl(wwo->ossdev->fd, SNDCTL_DSP_GETOSPACE, info) < 0) {
        /* assume nothing is playing */
        wwo->dwPlayedTotal = wwo->dwWrittenTotal;
        if (info) {
            info->fragstotal = wwo->dwBufferSize / wwo->dwFragmentSize;
            info->fragments = info->fragstotal;
            info->fragsize = wwo->dwFragmentSize;
            info->bytes = wwo->dwBufferSize;
        }
        if (errno == EPIPE) {
            /* I suspect ALSA gives this when the buffer runs out */
            ERR("ioctl 'SNDCTL_DSP_GETOSPACE' returned EPIPE\n");
            return TRUE; /* it might be recoverable */
        }
        ERR("ioctl can't 'SNDCTL_DSP_GETOSPACE' (%s)!\n", strerror(errno));
        return FALSE;
    }
    wwo->dwPlayedTotal = wwo->dwWrittenTotal - (wwo->dwBufferSize - info->bytes);
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
 * Returns the number of milliseconds to wait for the DSP buffer to write
 * one fragment.
 */
static DWORD wodPlayer_DSPWait(const WINE_WAVEOUT *wwo)
{
    /* time for one fragment to be played */
    return wwo->dwFragmentSize * 1000 / wwo->format.Format.nAvgBytesPerSec;
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
 * Writes the maximum number of bytes possible to the DSP and returns
 * TRUE iff the current playPtr has been fully played
 */
static BOOL wodPlayer_WriteMaxFrags(WINE_WAVEOUT* wwo, DWORD* bytes)
{
    DWORD       dwLength = wwo->lpPlayPtr->dwBufferLength - wwo->dwPartialOffset;
    DWORD       toWrite = min(dwLength, *bytes);
    int         written;
    BOOL        ret = FALSE;

    TRACE("Writing wavehdr %p.%lu[%lu]/%lu\n",
          wwo->lpPlayPtr, wwo->dwPartialOffset, wwo->lpPlayPtr->dwBufferLength, toWrite);

    if (toWrite > 0)
    {
        written = write(wwo->ossdev->fd, wwo->lpPlayPtr->lpData + wwo->dwPartialOffset, toWrite);
	if( written == -1 )
	{
	  perror( "wodPlayer_WriteMaxFrags write" );
	}
        if (written <= 0) return FALSE;
    }
    else
        written = 0;

    if (written >= dwLength) {
        /* If we wrote all current wavehdr, skip to the next one */
        wodPlayer_PlayPtrNext(wwo);
        ret = TRUE;
    } else {
        /* Remove the amount written */
        wwo->dwPartialOffset += written;
    }
    *bytes -= written;
    wwo->dwWrittenTotal += written;

    return ret;
}


/**************************************************************************
 * 				wodPlayer_NotifyCompletions	[internal]
 *
 * Notifies and remove from queue all wavehdrs which have been played to
 * the speaker (ie. they have cleared the OSS buffer).  If force is true,
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
static	void	wodPlayer_Reset(WORD uDevID, WINE_WAVEOUT* wwo, BOOL reset)
{
    wodUpdatePlayedTotal(wwo, NULL);
    /* updates current notify list */
    wodPlayer_NotifyCompletions(wwo, FALSE);

    /* flush all possible output */
    /* NOTE: bad for fullduplex */
    EnterCriticalSection (&wwo->ossdev->recCritSec);
    if (OSS_ResetDevice(wwo->ossdev, FALSE) == -1)
    {
	LeaveCriticalSection (&wwo->ossdev->recCritSec);
	wwo->hThread = 0;
	wwo->state = WINE_WS_STOPPED;
	ExitThread(-1);
    }
    LeaveCriticalSection (&wwo->ossdev->recCritSec);

    if (reset) {
        enum win_wm_message	msg;
        DWORD		        param;
        HANDLE		        ev;

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
        while (OSS_RetrieveRingMessage(&wwo->msgRing, &msg, &param, &ev))
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
    } else {
        if (wwo->lpLoopPtr) {
            /* complicated case, not handled yet (could imply modifying the loop counter */
            FIXME("Pausing while in loop isn't correctly handled yet, except strange results\n");
            wwo->lpPlayPtr = wwo->lpLoopPtr;
            wwo->dwPartialOffset = 0;
            wwo->dwWrittenTotal = wwo->dwPlayedTotal; /* this is wrong !!! */
        } else {
            LPWAVEHDR   ptr;
            DWORD       sz = wwo->dwPartialOffset;

            /* reset all the data as if we had written only up to lpPlayedTotal bytes */
            /* compute the max size playable from lpQueuePtr */
            for (ptr = wwo->lpQueuePtr; ptr != wwo->lpPlayPtr; ptr = ptr->lpNext) {
                sz += ptr->dwBufferLength;
            }
            /* because the reset lpPlayPtr will be lpQueuePtr */
            if (wwo->dwWrittenTotal > wwo->dwPlayedTotal + sz) ERR("grin\n");
            wwo->dwPartialOffset = sz - (wwo->dwWrittenTotal - wwo->dwPlayedTotal);
            wwo->dwWrittenTotal = wwo->dwPlayedTotal;
            wwo->lpPlayPtr = wwo->lpQueuePtr;
        }
	wwo->state = WINE_WS_PAUSED;
    }
}

/**************************************************************************
 * 		      wodPlayer_ProcessMessages			[internal]
 */
static void wodPlayer_ProcessMessages(WORD uDevID, WINE_WAVEOUT* wwo)
{
    LPWAVEHDR           lpWaveHdr;
    enum win_wm_message	msg;
    DWORD		param;
    HANDLE		ev;

    while (OSS_RetrieveRingMessage(&wwo->msgRing, &msg, &param, &ev)) {
	TRACE("Received %s %lx\n", wodPlayerCmdString[msg - WM_USER - 1], param);
	switch (msg) {
	case WINE_WM_PAUSING:
	    wodPlayer_Reset(uDevID, wwo, FALSE);
	    SetEvent(ev);
	    break;
	case WINE_WM_RESTARTING:
            if (wwo->state == WINE_WS_PAUSED)
            {
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
	    wodPlayer_Reset(uDevID, wwo, TRUE);
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
    audio_buf_info dspspace;
    DWORD       availInQ;

    wodUpdatePlayedTotal(wwo, &dspspace);
    availInQ = dspspace.bytes;
    TRACE("fragments=%d/%d, fragsize=%d, bytes=%d\n",
	  dspspace.fragments, dspspace.fragstotal, dspspace.fragsize, dspspace.bytes);

    /* input queue empty and output buffer with less than one fragment to play */
    if (!wwo->lpPlayPtr && wwo->dwBufferSize < availInQ + wwo->dwFragmentSize) {
	TRACE("Run out of wavehdr:s...\n");
        return INFINITE;
    }

    /* no more room... no need to try to feed */
    if (dspspace.fragments != 0) {
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

        if (wwo->bNeedPost) {
            /* OSS doesn't start before it gets either 2 fragments or a SNDCTL_DSP_POST;
             * if it didn't get one, we give it the other */
            if (wwo->dwBufferSize < availInQ + 2 * wwo->dwFragmentSize)
                ioctl(wwo->ossdev->fd, SNDCTL_DSP_POST, 0);
            wwo->bNeedPost = FALSE;
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
	wodPlayer_ProcessMessages(uDevID, wwo);
	if (wwo->state == WINE_WS_PLAYING) {
	    dwNextFeedTime = wodPlayer_FeedDSP(wwo);
	    dwNextNotifyTime = wodPlayer_NotifyCompletions(wwo, FALSE);
	    if (dwNextFeedTime == INFINITE) {
		/* FeedDSP ran out of data, but before flushing, */
		/* check that a notification didn't give us more */
		wodPlayer_ProcessMessages(uDevID, wwo);
		if (!wwo->lpPlayPtr) {
		    TRACE("flushing\n");
		    ioctl(wwo->ossdev->fd, SNDCTL_DSP_SYNC, 0);
		    wwo->dwPlayedTotal = wwo->dwWrittenTotal;
		}
		else {
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

    if (wDevID >= MAX_WAVEDRV) {
      TRACE("MAX_WAVDRV reached !\n");
      return MMSYSERR_BADDEVICEID;
    }

    memcpy(lpCaps, &OSS_Devices[wDevID].out_caps, min(dwSize, sizeof(*lpCaps)));

    TRACE( "dwFormats=0x%08lx, wChannels=%d dwSupport=0x%08lx\n",
             OSS_Devices[wDevID].out_caps.dwFormats, OSS_Devices[wDevID].out_caps.wChannels,
             OSS_Devices[wDevID].out_caps.dwSupport );

    return MMSYSERR_NOERROR;
}

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
static int ConfigureCaptureAndOpenDevice (WORD wDevID, int outputSampleRate,
                                          int outputStereo, int outputFmt,
                                          int outputFragment, int reqAccess,
                                          BOOL bResetDevice, BOOL bForceParms)
{
    WINE_WAVEIN *wwi = &WInDev[wDevID];
    OSS_DEVICE* ossdev = wwi->ossdev;
    WAVEFORMATEX format, tmp_format;
    int bestRate = -1;
    int lowestDeviation = 0;
    int i, ret = WAVERR_BADFORMAT;
    HACMSTREAM hConv = 0;
    int oldFragment, oldSampleRate, oldStereo, oldFmt;
    BOOL bDeviceOpened = FALSE;

    EnterCriticalSection (&ossdev->recCritSec);

    oldFragment = ossdev->audio_fragment;
    oldFmt = ossdev->format;
    oldStereo = ossdev->stereo;
    oldSampleRate = ossdev->sample_rate;

    if (bResetDevice)
    {
        /* Set OSS device to new format */
        ossdev->audio_fragment = outputFragment;
        ossdev->format = outputFmt;
        ossdev->stereo = outputStereo;
        ossdev->sample_rate = outputSampleRate;

        /* Reset OSS device */
        if (OSS_ResetDevice (ossdev, bForceParms) == -1)
            goto error;
    }

    ret = OSS_OpenDevice (ossdev, reqAccess, &outputFragment,
                          outputSampleRate, outputStereo, outputFmt,
                          bForceParms);
    if (ret != MMSYSERR_NOERROR)
        goto error;

    bDeviceOpened = TRUE;

    /* find closest standard sample rate to real sample rate,
     * since ACM only accepts such standard rates;
     * we currently define "closest" as lowest percent-wise
     * deviation from standard rate */
    for (i = 0; i < NUM_SAMPLE_RATES; i++)
    {
        int currentDeviation = (ossdev->sample_rate - sample_rates[i]) * 100;
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
    format.nChannels = ossdev->stereo ? 2 : 1;
    format.nSamplesPerSec = sample_rates[bestRate];
    format.wBitsPerSample = (ossdev->format == AFMT_S16_LE) ? 16 : 8;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
    format.cbSize = 0;
    TRACE ("Output: %ld/%d/%d\n", format.nSamplesPerSec, format.wBitsPerSample,
           format.nChannels);
    TRACE ("Capture: %ld/%d/%d\n", wwi->format.Format.nSamplesPerSec,
           wwi->format.Format.wBitsPerSample, wwi->format.Format.nChannels);

    /* If output format is "worse" than capture format, return error */
    ret = WAVERR_BADFORMAT;
    if ((format.nSamplesPerSec < wwi->format.Format.nSamplesPerSec) ||
        (format.nChannels < wwi->format.Format.nChannels) ||
        (format.wBitsPerSample < wwi->format.Format.wBitsPerSample))
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

    LeaveCriticalSection (&ossdev->recCritSec);
    return MMSYSERR_NOERROR;

 error:
    if (hConv)
        acmStreamClose (hConv, 0);

    if (bDeviceOpened)
        OSS_CloseDevice (ossdev);

    if (bResetDevice)
    {
        ossdev->audio_fragment = oldFragment;
        ossdev->format = oldFmt;
        ossdev->stereo = oldStereo;
        ossdev->sample_rate = oldSampleRate;
        OSS_ResetDevice (ossdev, FALSE);
    }

    ERR ("Unable to reopen device with new audio format!\n");

    LeaveCriticalSection (&ossdev->recCritSec);
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
    WARN("Unsupported format: tag=%04X nChannels=%d nSamplesPerSec=%ld !\n",
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
    int			audio_fragment;
    WINE_WAVEOUT*	wwo;
    audio_buf_info      info;
    int                 dsound, sample_rate;
    DWORD               ret;
    unsigned int        req_access;
    int                 stereo, fmt;
    BOOL                bForceParms;

    TRACE("(%u, %p, %08lX);\n", wDevID, lpDesc, dwFlags);
    if (lpDesc == NULL) {
	WARN("Invalid Parameter !\n");
	return MMSYSERR_INVALPARAM;
    }
    if (wDevID >= MAX_WAVEDRV) {
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
    
    sample_rate = lpDesc->lpFormat->nSamplesPerSec;

    dsound = (dwFlags & WAVE_DIRECTSOUND) && (wwo->ossdev->out_caps.dwSupport & WAVECAPS_DIRECTSOUND);

    if (dwFlags & WAVE_REOPEN)
    {
        /* need to reopen the device to allow the format to be changed. First,
           unmap the device if needed then close the device */
        if (wwo->mapping)
        {
            TRACE("sound device forcibly unmapped\n");
            if (ioctl(wwo->ossdev->fd, SNDCTL_DSP_RESET, 0) == -1)
                perror("ioctl SNDCTL_DSP_RESET");
            munmap(wwo->mapping, wwo->maplen);
            wwo->mapping = NULL;
        }
        OSS_CloseDevice(wwo->ossdev);
        wwo->state = WINE_WS_CLOSED;
    }

    if (dsound) {
        if (wwo->ossdev->out_caps.dwSupport & WAVECAPS_SAMPLEACCURATE)
	    /* we have realtime DirectSound, fragments just waste our time,
	     * but a large buffer is good, so choose 64KB (32 * 2^11) */
	    audio_fragment = 0x0020000B;
	else
	    /* to approximate realtime, we must use small fragments,
	     * let's try to fragment the above 64KB (256 * 2^8) */
	    audio_fragment = 0x01000008;
	if (wwo->ossdev->map_rate)
	    sample_rate = wwo->ossdev->map_rate;
	if (wwo->ossdev->map_format) {
	    /* our dsound will want the sample format written back */
	    lpDesc->lpFormat->wBitsPerSample = wwo->ossdev->map_format;
	    lpDesc->lpFormat->nBlockAlign = lpDesc->lpFormat->nChannels *
					    ((lpDesc->lpFormat->wBitsPerSample + 7) / 8);
	    /* nAvgBytesPerSec will be adjusted further down */
	}
    } else {
	/* shockwave player uses only 4 1k-fragments at a rate of 22050 bytes/sec
	 * thus leading to 46ms per fragment, and a turnaround time of 185ms
	 */
	/* 16 fragments max, 2^10=1024 bytes per fragment */
	audio_fragment = 0x000F000A;
    }
    if (wwo->state != WINE_WS_CLOSED) return MMSYSERR_ALLOCATED;

    /* we want to be able to mmap() the device, which means it must be opened readable,
     * otherwise mmap() will fail (at least under Linux) */
    req_access = (dsound ? O_RDWR : O_WRONLY);
    stereo = ((lpDesc->lpFormat->nChannels > 1) ? WINE_OSS_MODE_STEREO : WINE_OSS_MODE_MONO);
    fmt = ((lpDesc->lpFormat->wBitsPerSample == 16) ? AFMT_S16_LE : AFMT_U8);
    bForceParms = ((dwFlags & WAVE_DIRECTSOUND) ? FALSE : TRUE);

    ret = OSS_OpenDevice(wwo->ossdev, req_access, &audio_fragment, sample_rate,
                         stereo, fmt, bForceParms);
    if ((ret == WAVERR_BADFORMAT) && wwo->ossdev->rec_enable)
        ret = ConfigureCaptureAndOpenDevice (wDevID, sample_rate, stereo, fmt,
                                             audio_fragment, req_access,
                                             TRUE, bForceParms);

    if (ret != MMSYSERR_NOERROR) return ret;
    wwo->state = WINE_WS_STOPPED;

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
    /* Read output space info for future reference */
    if (ioctl(wwo->ossdev->fd, SNDCTL_DSP_GETOSPACE, &info) < 0) {
	ERR("ioctl can't 'SNDCTL_DSP_GETOSPACE' !\n");
        OSS_CloseDevice(wwo->ossdev);
	wwo->state = WINE_WS_CLOSED;
	return MMSYSERR_NOTENABLED;
    }

    /* Check that fragsize is correct per our settings above */
    if ((info.fragsize > 1024) && (LOWORD(audio_fragment) <= 10)) {
	/* we've tried to set 1K fragments or less, but it didn't work */
	ERR("fragment size set failed, size is now %d\n", info.fragsize);
	MESSAGE("Your Open Sound System driver did not let us configure small enough sound fragments.\n");
	MESSAGE("This may cause delays and other problems in audio playback with certain applications.\n");
    }

    /* Remember fragsize and total buffer size for future use */
    wwo->dwFragmentSize = info.fragsize;
    wwo->dwBufferSize = info.fragstotal * info.fragsize;
    wwo->dwPlayedTotal = 0;
    wwo->dwWrittenTotal = 0;
    wwo->bNeedPost = TRUE;

    /* the actual sample rate may be different from the requested,
     * we need to use the actual one for our timing. */
    wwo->format.Format.nSamplesPerSec = wwo->ossdev->sample_rate;
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
        OSS_InitRingMessage(&wwo->msgRing);

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

    TRACE("fd=%d fragmentSize=%ld\n",
	  wwo->ossdev->fd, wwo->dwFragmentSize);
    if (wwo->dwFragmentSize % wwo->format.Format.nBlockAlign)
    {
	ERR("Fragment doesn't contain an integral number of data blocks (%lu %% %d)\n",
	      wwo->dwFragmentSize, wwo->format.Format.nBlockAlign);
    }

    TRACE("wBitsPerSample=%u, nAvgBytesPerSec=%lu, nSamplesPerSec=%lu, nChannels=%u nBlockAlign=%u!\n",
	  wwo->format.Format.wBitsPerSample, wwo->format.Format.nAvgBytesPerSec,
	  wwo->format.Format.nSamplesPerSec, wwo->format.Format.nChannels,
	  wwo->format.Format.nBlockAlign);
    TRACE("actual sample_rate=%u\n", wwo->ossdev->sample_rate);

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

    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    wwo = &WOutDev[wDevID];
    if (wwo->lpQueuePtr) {
	WARN("buffers still playing !\n");
	ret = WAVERR_STILLPLAYING;
    } else {
	if (wwo->hThread != INVALID_HANDLE_VALUE) {
	    OSS_AddRingMessage(&wwo->msgRing, WINE_WM_CLOSING, 0, TRUE);
	}
	if (wwo->mapping) {
	    TRACE("sound device forcibly unmapped\n");
	    if (ioctl(wwo->ossdev->fd, SNDCTL_DSP_RESET, 0) == -1)
		perror("ioctl SNDCTL_DSP_RESET");
	    munmap(wwo->mapping, wwo->maplen);
	    wwo->mapping = NULL;
	}

        OSS_DestroyRingMessage(&wwo->msgRing);

        OSS_CloseDevice(wwo->ossdev);
	wwo->state = WINE_WS_CLOSED;
	wwo->dwFragmentSize = 0;
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
    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
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

    if ((lpWaveHdr->dwBufferLength % WOutDev[wDevID].format.Format.nBlockAlign) != 0)
    {
        ERR("WaveHdr length isn't a multiple of the PCM block size(buf=%lu align=%d)\n",
	       lpWaveHdr->dwBufferLength, WOutDev[wDevID].format.Format.nBlockAlign );
        lpWaveHdr->dwBufferLength -= lpWaveHdr->dwBufferLength % WOutDev[wDevID].format.Format.nBlockAlign;
    }

    OSS_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_HEADER, (DWORD)lpWaveHdr, FALSE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodPrepare			[internal]
 */
static DWORD wodPrepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEDRV) {
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

    if (wDevID >= MAX_WAVEDRV) {
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

    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    OSS_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_PAUSING, 0, TRUE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			wodRestart				[internal]
 */
static DWORD wodRestart(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);

    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    OSS_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_RESTARTING, 0, TRUE);

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

    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    OSS_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_RESETTING, 0, TRUE);

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

    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }

    if (lpTime == NULL)	return MMSYSERR_INVALPARAM;

    wwo = &WOutDev[wDevID];
#ifdef EXACT_WODPOSITION
    OSS_AddRingMessage(&wwo->msgRing, WINE_WM_UPDATE, 0, TRUE);
#endif
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

    if (wDevID >= MAX_WAVEDRV || WOutDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("bad device ID !\n");
	return MMSYSERR_BADDEVICEID;
    }
    OSS_AddRingMessage(&WOutDev[wDevID].msgRing, WINE_WM_BREAKLOOP, 0, TRUE);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodGetVolume			[internal]
 */
static DWORD wodGetVolume(WORD wDevID, LPDWORD lpdwVol)
{
    int 	mixer;
    int		volume;
    DWORD	left, right;

    TRACE("(%u, %p);\n", wDevID, lpdwVol);

    if (lpdwVol == NULL)
	return MMSYSERR_NOTENABLED;
    if (wDevID >= MAX_WAVEDRV) return MMSYSERR_INVALPARAM;

    if ((mixer = open(OSS_Devices[wDevID].mixer_name, O_RDONLY|O_NDELAY)) < 0) {
	WARN("mixer device not available !\n");
	return MMSYSERR_NOTENABLED;
    }
    if (ioctl(mixer, SOUND_MIXER_READ_PCM, &volume) == -1) {
	WARN("unable to read mixer !\n");
	return MMSYSERR_NOTENABLED;
    }
    close(mixer);
    left = LOBYTE(volume);
    right = HIBYTE(volume);
    TRACE("left=%ld right=%ld !\n", left, right);
    *lpdwVol = ((left * 0xFFFFl) / 100) + (((right * 0xFFFFl) / 100) << 16);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodSetVolume			[internal]
 */
static DWORD wodSetVolume(WORD wDevID, DWORD dwParam)
{
    int 	mixer;
    int		volume;
    DWORD	left, right;

    TRACE("(%u, %08lX);\n", wDevID, dwParam);

    left  = (LOWORD(dwParam) * 100) / 0xFFFFl;
    right = (HIWORD(dwParam) * 100) / 0xFFFFl;
    volume = left + (right << 8);

    if (wDevID >= MAX_WAVEDRV) return MMSYSERR_INVALPARAM;

    if ((mixer = open(OSS_Devices[wDevID].mixer_name, O_WRONLY|O_NDELAY)) < 0) {
	WARN("mixer device not available !\n");
	return MMSYSERR_NOTENABLED;
    }
    if (ioctl(mixer, SOUND_MIXER_WRITE_PCM, &volume) == -1) {
	WARN("unable to set mixer !\n");
	return MMSYSERR_NOTENABLED;
    } else {
	TRACE("volume=%04x\n", (unsigned)volume);
    }
    close(mixer);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				wodMessage (WINEOSS.7)
 */
DWORD WINAPI OSS_wodMessage(UINT wDevID, UINT wMsg, DWORD dwUser,
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
    case WODM_WRITE:	 	return wodWrite		(wDevID, (LPWAVEHDR)dwParam1,		dwParam2);
    case WODM_PAUSE:	 	return wodPause		(wDevID);
    case WODM_GETPOS:	 	return wodGetPosition	(wDevID, (LPMMTIME)dwParam1, 		dwParam2);
    case WODM_BREAKLOOP: 	return wodBreakLoop     (wDevID);
    case WODM_PREPARE:	 	return wodPrepare	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WODM_UNPREPARE: 	return wodUnprepare	(wDevID, (LPWAVEHDR)dwParam1, 		dwParam2);
    case WODM_GETDEVCAPS:	return wodGetDevCaps	(wDevID, (LPWAVEOUTCAPSA)dwParam1,	dwParam2);
    case WODM_GETNUMDEVS:	return numOutDev;
    case WODM_GETPITCH:	 	return MMSYSERR_NOTSUPPORTED;
    case WODM_SETPITCH:	 	return MMSYSERR_NOTSUPPORTED;
    case WODM_GETPLAYBACKRATE:	return MMSYSERR_NOTSUPPORTED;
    case WODM_SETPLAYBACKRATE:	return MMSYSERR_NOTSUPPORTED;
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
    DWORD		ref;
    /* IDsDriverBufferImpl fields */
    IDsDriverImpl*	drv;
    DWORD		buflen;
};

static HRESULT DSDB_MapPrimary(IDsDriverBufferImpl *dsdb)
{
    WINE_WAVEOUT *wwo = &(WOutDev[dsdb->drv->wDevID]);
    if (!wwo->mapping) {
	wwo->mapping = mmap(NULL, wwo->maplen, PROT_WRITE, MAP_SHARED,
			    wwo->ossdev->fd, 0);
	if (wwo->mapping == (LPBYTE)-1) {
	    ERR("(%p): Could not map sound device for direct access (%s)\n", dsdb, strerror(errno));
	    return DSERR_GENERIC;
	}
	TRACE("(%p): sound device has been mapped for direct access at %p, size=%ld\n", dsdb, wwo->mapping, wwo->maplen);

	/* for some reason, es1371 and sblive! sometimes have junk in here.
	 * clear it, or we get junk noise */
	/* some libc implementations are buggy: their memset reads from the buffer...
	 * to work around it, we have to zero the block by hand. We don't do the expected:
	 * memset(wwo->mapping,0, wwo->maplen);
	 */
	{
	    unsigned char*	p1 = wwo->mapping;
	    unsigned	len = wwo->maplen;

	    if (len >= 16) /* so we can have at least a 4 long area to store... */
	    {
		/* the mmap:ed value is (at least) dword aligned
		 * so, start filling the complete unsigned long:s
		 */
		int		b = len >> 2;
		unsigned long*	p4 = (unsigned long*)p1;

		while (b--) *p4++ = 0;
		/* prepare for filling the rest */
		len &= 3;
		p1 = (unsigned char*)p4;
	    }
	    /* in all cases, fill the remaining bytes */
	    while (len-- != 0) *p1++ = 0;
	}
    }
    return DS_OK;
}

static HRESULT DSDB_UnmapPrimary(IDsDriverBufferImpl *dsdb)
{
    WINE_WAVEOUT *wwo = &(WOutDev[dsdb->drv->wDevID]);
    if (wwo->mapping) {
	if (munmap(wwo->mapping, wwo->maplen) < 0) {
	    ERR("(%p): Could not unmap sound device (errno=%d)\n", dsdb, errno);
	    return DSERR_GENERIC;
	}
	wwo->mapping = NULL;
	TRACE("(%p): sound device unmapped\n", dsdb);
    }
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_QueryInterface(PIDSDRIVERBUFFER iface, REFIID riid, LPVOID *ppobj)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    FIXME("(%p): stub!\n", iface);
    return DSERR_UNSUPPORTED;
}

static ULONG WINAPI IDsDriverBufferImpl_AddRef(PIDSDRIVERBUFFER iface)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    TRACE("(%p): ref was %ld\n", iface, This->ref);
    This->ref++;
    return This->ref;
}

static ULONG WINAPI IDsDriverBufferImpl_Release(PIDSDRIVERBUFFER iface)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    TRACE("(%p): ref was %ld\n", iface, This->ref);
    if (--This->ref)
	return This->ref;
    if (This == This->drv->primary)
	This->drv->primary = NULL;
    TRACE("releasing device\n");
    DSDB_UnmapPrimary(This);
    HeapFree(GetProcessHeap(),0,This);
    return 0;
}

static HRESULT WINAPI IDsDriverBufferImpl_Lock(PIDSDRIVERBUFFER iface,
					       LPVOID*ppvAudio1,LPDWORD pdwLen1,
					       LPVOID*ppvAudio2,LPDWORD pdwLen2,
					       DWORD dwWritePosition,DWORD dwWriteLen,
					       DWORD dwFlags)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    /* since we (GetDriverDesc flags) have specified DSDDESC_DONTNEEDPRIMARYLOCK,
     * and that we don't support secondary buffers, this method will never be called */
    TRACE("(%p): stub\n",iface);
    return DSERR_UNSUPPORTED;
}

static HRESULT WINAPI IDsDriverBufferImpl_Unlock(PIDSDRIVERBUFFER iface,
						 LPVOID pvAudio1,DWORD dwLen1,
						 LPVOID pvAudio2,DWORD dwLen2)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */
    TRACE("(%p): stub\n",iface);
    return DSERR_UNSUPPORTED;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetFormat(PIDSDRIVERBUFFER iface,
						    LPWAVEFORMATEX pwfx)
{
    /* ICOM_THIS(IDsDriverBufferImpl,iface); */

    TRACE("(%p,%p)\n",iface,pwfx);
    /* On our request (GetDriverDesc flags), DirectSound has by now used
     * waveOutClose/waveOutOpen to set the format...
     * unfortunately, this means our mmap() is now gone...
     * so we need to somehow signal to our DirectSound implementation
     * that it should completely recreate this HW buffer...
     * this unexpected error code should do the trick... */
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
    count_info info;
    DWORD ptr;

    TRACE("(%p)\n",iface);
    if (WOutDev[This->drv->wDevID].state == WINE_WS_CLOSED) {
	ERR("device not open, but accessing?\n");
	return DSERR_UNINITIALIZED;
    }
    if (ioctl(WOutDev[This->drv->wDevID].ossdev->fd, SNDCTL_DSP_GETOPTR, &info) < 0) {
	ERR("ioctl failed (%d)\n", errno);
	return DSERR_GENERIC;
    }
    ptr = info.ptr & ~3; /* align the pointer, just in case */
    if (lpdwPlay) *lpdwPlay = ptr;
    if (lpdwWrite) {
	/* add some safety margin (not strictly necessary, but...) */
	if (OSS_Devices[This->drv->wDevID].out_caps.dwSupport & WAVECAPS_SAMPLEACCURATE)
	    *lpdwWrite = ptr + 32;
	else
	    *lpdwWrite = ptr + WOutDev[This->drv->wDevID].dwFragmentSize;
	while (*lpdwWrite > This->buflen)
	    *lpdwWrite -= This->buflen;
    }
    TRACE("playpos=%ld, writepos=%ld\n", lpdwPlay?*lpdwPlay:0, lpdwWrite?*lpdwWrite:0);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Play(PIDSDRIVERBUFFER iface, DWORD dwRes1, DWORD dwRes2, DWORD dwFlags)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    TRACE("(%p,%lx,%lx,%lx)\n",iface,dwRes1,dwRes2,dwFlags);
    OSS_TriggerDevice(WOutDev[This->drv->wDevID].ossdev, PCM_ENABLE_OUTPUT, 0);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Stop(PIDSDRIVERBUFFER iface)
{
    ICOM_THIS(IDsDriverBufferImpl,iface);
    TRACE("(%p)\n",iface);
    /* no more playing */
    OSS_TriggerDevice(WOutDev[This->drv->wDevID].ossdev, 0, PCM_ENABLE_OUTPUT);
#if 0
    /* the play position must be reset to the beginning of the buffer */
    if (ioctl(WOutDev[This->drv->wDevID].unixdev, SNDCTL_DSP_RESET, 0) < 0) {
	ERR("ioctl failed (%d)\n", errno);
	return DSERR_GENERIC;
    }
#endif
    /* Most OSS drivers just can't stop the playback without closing the device...
     * so we need to somehow signal to our DirectSound implementation
     * that it should completely recreate this HW buffer...
     * this unexpected error code should do the trick... */
    return DSERR_BUFFERLOST;
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
    This->ref++;
    return This->ref;
}

static ULONG WINAPI IDsDriverImpl_Release(PIDSDRIVER iface)
{
    ICOM_THIS(IDsDriverImpl,iface);
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
	DSDDESC_USESYSTEMMEMORY | DSDDESC_DONTNEEDPRIMARYLOCK;
    strcpy(pDesc->szDesc,"WineOSS DirectSound Driver");
    strcpy(pDesc->szDrvName,"wineoss.drv");
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
    ICOM_THIS(IDsDriverImpl,iface);

    TRACE("(%p)\n",iface);
    /* make sure the card doesn't start playing before we want it to */
    OSS_TriggerDevice(WOutDev[This->wDevID].ossdev, 0, PCM_ENABLE_OUTPUT);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_Close(PIDSDRIVER iface)
{
    ICOM_THIS(IDsDriverImpl,iface);
    TRACE("(%p)\n",iface);
    if (This->primary) {
	ERR("problem with DirectSound: primary not released\n");
	return DSERR_GENERIC;
    }
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_GetCaps(PIDSDRIVER iface, PDSDRIVERCAPS pCaps)
{
    ICOM_THIS(IDsDriverImpl,iface);
    TRACE("(%p,%p)\n",iface,pCaps);
    memset(pCaps, 0, sizeof(*pCaps));

    pCaps->dwFlags = OSS_Devices[This->wDevID].dwDsOutCaps;

    pCaps->dwPrimaryBuffers = 1;

    /* the other fields only apply to secondary buffers, which we don't support
     * (unless we want to mess with wavetable synthesizers and MIDI) */
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
    HRESULT err;
    audio_buf_info info;

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

    /* check how big the DMA buffer is now */
    if (ioctl(WOutDev[This->wDevID].ossdev->fd, SNDCTL_DSP_GETOSPACE, &info) < 0) {
	ERR("ioctl failed (%d)\n", errno);
	HeapFree(GetProcessHeap(),0,*ippdsdb);
	*ippdsdb = NULL;
	return DSERR_GENERIC;
    }
    WOutDev[This->wDevID].maplen = (*ippdsdb)->buflen = info.fragstotal * info.fragsize;

    /* map the DMA buffer */
    err = DSDB_MapPrimary(*ippdsdb);
    if (err != DS_OK) {
	HeapFree(GetProcessHeap(),0,*ippdsdb);
	*ippdsdb = NULL;
	return err;
    }

    /* primary buffer is ready to go */
    *pdwcbBufferSize	= WOutDev[This->wDevID].maplen;
    *ppbBuffer		= WOutDev[This->wDevID].mapping;

    /* some drivers need some extra nudging after mapping */
    OSS_TriggerDevice(WOutDev[This->wDevID].ossdev, 0, PCM_ENABLE_OUTPUT);

    This->primary = *ippdsdb;

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

    /* the HAL isn't much better than the HEL if we can't do mmap() */
    if (!(OSS_Devices[wDevID].out_caps.dwSupport & WAVECAPS_DIRECTSOUND)) {

/* Disabled mmapped DIRECTSOUND for now, since it causes massive
   trouble on some systems, esp once with ALSA drivers */
#if 0
       ERR("DirectSound flag not set\n");
       MESSAGE("This sound card's driver does not support direct access\n");
       MESSAGE("The (slower) DirectSound HEL mode will be used instead.\n");
#endif
       return MMSYSERR_NOTSUPPORTED;
    }

    *idrv = (IDsDriverImpl*)HeapAlloc(GetProcessHeap(),0,sizeof(IDsDriverImpl));
    if (!*idrv)
    {
      ERR( "No memory\n" );
      return MMSYSERR_NOMEM;
    }

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
	    !DriverCallback(wwi->waveDesc.dwCallback, wwi->wFlags,
			    (HDRVR)wwi->waveDesc.hWave, wMsg,
			    wwi->waveDesc.dwInstance, dwParam1, dwParam2)) {
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

    if (wDevID >= MAX_WAVEDRV) {
	TRACE("MAX_WAVDRV reached !\n");
	return MMSYSERR_BADDEVICEID;
    }

    memcpy(lpCaps, &OSS_Devices[wDevID].in_caps, min(dwSize, sizeof(*lpCaps)));
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widRecorder			[internal]
 */
static	DWORD	CALLBACK	widRecorder(LPVOID pmt)
{
    WORD		uDevID = (DWORD)pmt;
    WINE_WAVEIN*	wwi = (WINE_WAVEIN*)&WInDev[uDevID];
    WAVEHDR*		lpWaveHdr;
    DWORD		nBytesPerSec;
    DWORD		dwSleepTime;
    DWORD		bytesRead;
    ACMSTREAMHEADER	acmHdr;
    DWORD		acmFlag = ACM_STREAMCONVERTF_START;
    LPVOID		buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wwi->dwFragmentSize);
    LPVOID		pOffset = buffer;
    audio_buf_info 	info;
#if 0
    int 		xs;
#endif
    enum win_wm_message msg;
    DWORD		param;
    HANDLE		ev;
    BOOL		overrun = FALSE;

    wwi->state = WINE_WS_STOPPED;
    wwi->dwTotalRecorded = 0;

    SetEvent(wwi->hStartUpEvent);

    EnterCriticalSection (&wwi->ossdev->recCritSec);
    if (wwi->bTriggerSupport) {
        wwi->ossdev->rec_enable++;
        /* trigger off, so that triggering it on starts recording */
        OSS_TriggerDevice(wwi->ossdev, 0, PCM_ENABLE_INPUT);
        /* start recording now */
        OSS_TriggerDevice(wwi->ossdev, PCM_ENABLE_INPUT, 0);
    }
    else {
        /* older drivers can be told to start recording with select(),
         * maybe that would be better than this? */
        read(wwi->ossdev->fd, buffer, wwi->dwFragmentSize);
    }
    LeaveCriticalSection (&wwi->ossdev->recCritSec);

	/* make sleep time to be # of ms to output a fragment */
    nBytesPerSec = wwi->ossdev->sample_rate *
                   (wwi->ossdev->stereo ? 2 : 1) *
                   ((wwi->ossdev->format == AFMT_S16_LE) ? 2 : 1);
    dwSleepTime = (wwi->dwFragmentSize * 1000) / nBytesPerSec;
    TRACE("sleeptime=%ld ms\n", dwSleepTime);

    for (;;) {
	/* wait for dwSleepTime or an event in thread's queue */
	/* FIXME: could improve wait time depending on queue state,
	 * ie, number of queued fragments
	 */

        lpWaveHdr = wwi->lpQueuePtr;

        EnterCriticalSection (&wwi->ossdev->recCritSec);
	ioctl(wwi->ossdev->fd, SNDCTL_DSP_GETISPACE, &info);
        TRACE("info={frag=%d fsize=%d ftotal=%d bytes=%d}\n", info.fragments, info.fragsize, info.fragstotal, info.bytes);
        if (info.fragments == info.fragstotal) overrun = TRUE;

        /* read all the fragments accumulated so far */
        while (info.fragments > 0)
        {
            info.fragments --;

force_recover:
            if (lpWaveHdr && wwi->state == WINE_WS_PLAYING &&
                lpWaveHdr->dwBufferLength - lpWaveHdr->dwBytesRecorded >= wwi->dwFragmentSize && !wwi->hConv)
            {
                /* directly read fragment in wavehdr */
                bytesRead = read(wwi->ossdev->fd,
                                 lpWaveHdr->lpData + lpWaveHdr->dwBytesRecorded,
                                 wwi->dwFragmentSize);

                TRACE("bytesRead=%ld (direct)\n", bytesRead);
                if (bytesRead != (DWORD) -1)
                {
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

			widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);
			lpWaveHdr = wwi->lpQueuePtr = lpNext;
		    }
                }
            }
            else
            {
                /* read the fragment in a local buffer */
                bytesRead = read(wwi->ossdev->fd, buffer, wwi->dwFragmentSize);
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
                while (bytesRead != (DWORD) -1 && bytesRead > 0)
                {
                    DWORD dwToWrite = lpWaveHdr->dwBufferLength - lpWaveHdr->dwBytesRecorded;
                    DWORD dwRead, dwWritten;

		    if (wwi->hConv) {
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
		    }
		    else {
			dwRead = dwWritten = min(bytesRead, dwToWrite);
			memcpy(lpWaveHdr->lpData + lpWaveHdr->dwBytesRecorded,
			       pOffset, dwRead);
			TRACE("copy=%ld\n", dwRead);
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

                        widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);

			wwi->lpQueuePtr = lpWaveHdr = lpNext;
			if (!lpNext && bytesRead) {
                            /* no more buffer to copy data to, but we did read more.
                             * what hasn't been copied will be dropped
                             */
                            TRACE("buffer underrun! %lu bytes dropped.\n", bytesRead);
                            wwi->lpQueuePtr = NULL;
                            acmFlag = ACM_STREAMCONVERTF_START;
                            break;
                        }
                    }
                }
            }
        }
        if (overrun) {
            TRACE("trying overrun recovery\n");
            acmFlag = ACM_STREAMCONVERTF_START;
            if (wwi->bTriggerSupport) {
                /* maybe some drivers will let us recover this way */
                OSS_TriggerDevice(wwi->ossdev, 0, PCM_ENABLE_INPUT);
                OSS_TriggerDevice(wwi->ossdev, PCM_ENABLE_INPUT, 0);
            }
            else goto force_recover;
        }
        overrun = FALSE;

        LeaveCriticalSection (&wwi->ossdev->recCritSec);

	WAIT_OMR(&wwi->msgRing, dwSleepTime);

	while (OSS_RetrieveRingMessage(&wwi->msgRing, &msg, &param, &ev))
	{

            TRACE("msg=0x%x param=0x%lx\n", msg, param);
	    switch (msg) {
	    case WINE_WM_PAUSING:
		wwi->state = WINE_WS_PAUSED;

		/* return current buffer to the app */
		lpWaveHdr = wwi->lpQueuePtr;
		if (lpWaveHdr && lpWaveHdr->dwBytesRecorded) {
		    TRACE("stop %p %p\n", lpWaveHdr, lpWaveHdr->lpNext);
		    lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
		    lpWaveHdr->dwFlags |= WHDR_DONE;
		    wwi->lpQueuePtr = lpWaveHdr->lpNext;

		    widNotifyClient(wwi, WIM_DATA, (DWORD)lpWaveHdr, 0);
		}
		SetEvent(ev);
		break;
	    case WINE_WM_RESTARTING:
		wwi->state = WINE_WS_PLAYING;
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
	    case WINE_WM_RESETTING:
		wwi->state = WINE_WS_STOPPED;

		/* return all buffers to the app */
		for (lpWaveHdr = wwi->lpQueuePtr; lpWaveHdr; lpWaveHdr = lpWaveHdr->lpNext) {
		    TRACE("reset %p %p\n", lpWaveHdr, lpWaveHdr->lpNext);
		    lpWaveHdr->dwFlags &= ~WHDR_INQUEUE;
		    lpWaveHdr->dwFlags |= WHDR_DONE;

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
		goto finito;
		/* shouldn't go here */
	    default:
		FIXME("unknown message %d\n", msg);
		break;
	    }
	}
    }
finito:
    if (wwi->hConv) {
	acmStreamClose(wwi->hConv, 0);
	wwi->hConv = 0;
    }
    if (wwi->bTriggerSupport) wwi->ossdev->rec_enable--;
    /* we could trigger recording off but OSS/Commercial wouldn't honor that,
     * so we'll just let it be, the card should stop recording when it overruns
     */
    ExitThread(0);
    /* just for not generating compilation warnings... should never be executed */
    return 0;
}


/**************************************************************************
 * 				widOpen				[internal]
 */
static DWORD widOpen(WORD wDevID, LPWAVEOPENDESC lpDesc, DWORD dwFlags)
{
    WINE_WAVEIN*	wwi;
    int                 fragment_size;
    int                 audio_fragment;
    int                 sample_rate, stereo, fmt;
    DWORD               ret;

    TRACE("(%u, %p, %08lX);\n", wDevID, lpDesc, dwFlags);
    if (lpDesc == NULL) {
	WARN("Invalid Parameter !\n");
	return MMSYSERR_INVALPARAM;
    }
    if (wDevID >= MAX_WAVEDRV) return MMSYSERR_BADDEVICEID;

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

    if (wwi->state != WINE_WS_CLOSED) return MMSYSERR_ALLOCATED;
    /* This is actually hand tuned to work so that my SB Live:
     * - does not skip
     * - does not buffer too much
     * when sending with the Shoutcast winamp plugin
     */
    /* 15 fragments max, 2^10 = 1024 bytes per fragment */
    audio_fragment = 0x000F000A;
    sample_rate = lpDesc->lpFormat->nSamplesPerSec;
    stereo = (lpDesc->lpFormat->nChannels > 1) ? 1 : 0;
    fmt = (lpDesc->lpFormat->wBitsPerSample == 16) ? AFMT_S16_LE : AFMT_U8;
    ret = OSS_OpenDevice(wwi->ossdev, O_RDONLY, &audio_fragment,
                         sample_rate, stereo, fmt, TRUE);
    if (ret != 0 && ret != WAVERR_BADFORMAT) return ret;

    memcpy(&wwi->waveDesc, lpDesc, sizeof(WAVEOPENDESC));
    copy_wave_format(&wwi->format.Format, lpDesc->lpFormat);

    if (wwi->format.Format.wBitsPerSample == 0) {
	WARN("Resetting zeroed wBitsPerSample\n");
	wwi->format.Format.wBitsPerSample = 8 *
	    (wwi->format.Format.nAvgBytesPerSec /
	     wwi->format.Format.nSamplesPerSec) /
	    wwi->format.Format.nChannels;
    }

    if (ret == WAVERR_BADFORMAT) {
	OSS_DEVICE* ossdev = wwi->ossdev;

        ret = ConfigureCaptureAndOpenDevice (wDevID, ossdev->sample_rate,
                                             ossdev->stereo,
                                             ossdev->format,
                                             ossdev->audio_fragment, O_RDONLY,
                                             FALSE, TRUE);
        if (ret != MMSYSERR_NOERROR)
        {
            TRACE ("Rejecting full duplex\n");
            return ret;
        }
    }

    wwi->state = WINE_WS_STOPPED;
    if (wwi->lpQueuePtr) {
	WARN("Should have an empty queue (%p)\n", wwi->lpQueuePtr);
	wwi->lpQueuePtr = NULL;
    }
    wwi->dwTotalRecorded = 0;
    wwi->wFlags = HIWORD(dwFlags & CALLBACK_TYPEMASK);

    ioctl(wwi->ossdev->fd, SNDCTL_DSP_GETBLKSIZE, &fragment_size);
    if (fragment_size == -1) {
	WARN("IOCTL can't 'SNDCTL_DSP_GETBLKSIZE' !\n");
        OSS_CloseDevice(wwi->ossdev);
	wwi->state = WINE_WS_CLOSED;
	return MMSYSERR_NOTENABLED;
    }
    wwi->dwFragmentSize = fragment_size;

    TRACE("wBitsPerSample=%u, nAvgBytesPerSec=%lu, nSamplesPerSec=%lu, nChannels=%u nBlockAlign=%u!\n",
	  wwi->format.Format.wBitsPerSample, wwi->format.Format.nAvgBytesPerSec,
	  wwi->format.Format.nSamplesPerSec, wwi->format.Format.nChannels,
	  wwi->format.Format.nBlockAlign);

    OSS_InitRingMessage(&wwi->msgRing);

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
    wwi->hStartUpEvent = INVALID_HANDLE_VALUE;

    return widNotifyClient(wwi, WIM_OPEN, 0L, 0L);
}

/**************************************************************************
 * 				widClose			[internal]
 */
static DWORD widClose(WORD wDevID)
{
    WINE_WAVEIN*	wwi;

    TRACE("(%u);\n", wDevID);
    if (wDevID >= MAX_WAVEDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't close !\n");
	return MMSYSERR_INVALHANDLE;
    }

    wwi = &WInDev[wDevID];

    if (wwi->lpQueuePtr != NULL) {
	WARN("still buffers open !\n");
	return WAVERR_STILLPLAYING;
    }

    OSS_AddRingMessage(&wwi->msgRing, WINE_WM_CLOSING, 0, TRUE);
    OSS_CloseDevice(wwi->ossdev);
    wwi->state = WINE_WS_CLOSED;
    wwi->dwFragmentSize = 0;
    OSS_DestroyRingMessage(&wwi->msgRing);
    return widNotifyClient(wwi, WIM_CLOSE, 0L, 0L);
}

/**************************************************************************
 * 				widAddBuffer		[internal]
 */
static DWORD widAddBuffer(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't do it !\n");
	return MMSYSERR_INVALHANDLE;
    }
    if (!(lpWaveHdr->dwFlags & WHDR_PREPARED)) {
	TRACE("never been prepared !\n");
	return WAVERR_UNPREPARED;
    }
    if (lpWaveHdr->dwFlags & WHDR_INQUEUE) {
	TRACE("header already in use !\n");
	return WAVERR_STILLPLAYING;
    }

    lpWaveHdr->dwFlags |= WHDR_INQUEUE;
    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    lpWaveHdr->dwBytesRecorded = 0;
    lpWaveHdr->lpNext = NULL;

    OSS_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_HEADER, (DWORD)lpWaveHdr, FALSE);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widPrepare			[internal]
 */
static DWORD widPrepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);

    if (wDevID >= MAX_WAVEDRV) return MMSYSERR_INVALHANDLE;

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags |= WHDR_PREPARED;
    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    lpWaveHdr->dwBytesRecorded = 0;
    TRACE("header prepared !\n");
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widUnprepare			[internal]
 */
static DWORD widUnprepare(WORD wDevID, LPWAVEHDR lpWaveHdr, DWORD dwSize)
{
    TRACE("(%u, %p, %08lX);\n", wDevID, lpWaveHdr, dwSize);
    if (wDevID >= MAX_WAVEDRV) return MMSYSERR_INVALHANDLE;

    if (lpWaveHdr->dwFlags & WHDR_INQUEUE)
	return WAVERR_STILLPLAYING;

    lpWaveHdr->dwFlags &= ~WHDR_PREPARED;
    lpWaveHdr->dwFlags |= WHDR_DONE;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			widStart				[internal]
 */
static DWORD widStart(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);
    if (wDevID >= MAX_WAVEDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't start recording !\n");
	return MMSYSERR_INVALHANDLE;
    }

    OSS_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_RESTARTING, 0, TRUE);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			widStop					[internal]
 */
static DWORD widStop(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);
    if (wDevID >= MAX_WAVEDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't stop !\n");
	return MMSYSERR_INVALHANDLE;
    }
    /* FIXME: reset aint stop */
    OSS_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_PAUSING, 0, TRUE);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			widReset				[internal]
 */
static DWORD widReset(WORD wDevID)
{
    TRACE("(%u);\n", wDevID);
    if (wDevID >= MAX_WAVEDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't reset !\n");
	return MMSYSERR_INVALHANDLE;
    }
    OSS_AddRingMessage(&WInDev[wDevID].msgRing, WINE_WM_RESETTING, 0, TRUE);
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widGetPosition			[internal]
 */
static DWORD widGetPosition(WORD wDevID, LPMMTIME lpTime, DWORD uSize)
{
    int			time;
    WINE_WAVEIN*	wwi;

    TRACE("(%u, %p, %lu);\n", wDevID, lpTime, uSize);

    if (wDevID >= MAX_WAVEDRV || WInDev[wDevID].state == WINE_WS_CLOSED) {
	WARN("can't get pos !\n");
	return MMSYSERR_INVALHANDLE;
    }
    if (lpTime == NULL)	return MMSYSERR_INVALPARAM;

    wwi = &WInDev[wDevID];

    TRACE("wType=%04X !\n", lpTime->wType);
    TRACE("wBitsPerSample=%u\n", wwi->format.Format.wBitsPerSample);
    TRACE("nSamplesPerSec=%lu\n", wwi->format.Format.nSamplesPerSec);
    TRACE("nChannels=%u\n", wwi->format.Format.nChannels);
    TRACE("nAvgBytesPerSec=%lu\n", wwi->format.Format.nAvgBytesPerSec);
    switch (lpTime->wType) {
    case TIME_BYTES:
	lpTime->u.cb = wwi->dwTotalRecorded;
	TRACE("TIME_BYTES=%lu\n", lpTime->u.cb);
	break;
    case TIME_SAMPLES:
	lpTime->u.sample = wwi->dwTotalRecorded * 8 /
	    wwi->format.Format.wBitsPerSample;
	TRACE("TIME_SAMPLES=%lu\n", lpTime->u.sample);
	break;
    case TIME_SMPTE:
	time = wwi->dwTotalRecorded /
	    (wwi->format.Format.nAvgBytesPerSec / 1000);
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
    case TIME_MS:
	lpTime->u.ms = wwi->dwTotalRecorded /
	    (wwi->format.Format.nAvgBytesPerSec / 1000);
	TRACE("TIME_MS=%lu\n", lpTime->u.ms);
	break;
    default:
	FIXME("format not supported (%u) ! use TIME_MS !\n", lpTime->wType);
	lpTime->wType = TIME_MS;
    }
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				widMessage (WINEOSS.6)
 */
DWORD WINAPI OSS_widMessage(WORD wDevID, WORD wMsg, DWORD dwUser,
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
    case WIDM_OPEN:		return widOpen       (wDevID, (LPWAVEOPENDESC)dwParam1, dwParam2);
    case WIDM_CLOSE:		return widClose      (wDevID);
    case WIDM_ADDBUFFER:	return widAddBuffer  (wDevID, (LPWAVEHDR)dwParam1, dwParam2);
    case WIDM_PREPARE:		return widPrepare    (wDevID, (LPWAVEHDR)dwParam1, dwParam2);
    case WIDM_UNPREPARE:	return widUnprepare  (wDevID, (LPWAVEHDR)dwParam1, dwParam2);
    case WIDM_GETDEVCAPS:	return widGetDevCaps (wDevID, (LPWAVEINCAPSA)dwParam1, dwParam2);
    case WIDM_GETNUMDEVS:	return numInDev;
    case WIDM_GETPOS:		return widGetPosition(wDevID, (LPMMTIME)dwParam1, dwParam2);
    case WIDM_RESET:		return widReset      (wDevID);
    case WIDM_START:		return widStart      (wDevID);
    case WIDM_STOP:		return widStop       (wDevID);
    default:
	FIXME("unknown message %u!\n", wMsg);
    }
    return MMSYSERR_NOTSUPPORTED;
}

#else /* !HAVE_OSS */

/**************************************************************************
 * 				wodMessage (WINEOSS.7)
 */
DWORD WINAPI OSS_wodMessage(WORD wDevID, WORD wMsg, DWORD dwUser,
			    DWORD dwParam1, DWORD dwParam2)
{
    FIXME("(%u, %04X, %08lX, %08lX, %08lX):stub\n", wDevID, wMsg, dwUser, dwParam1, dwParam2);
    return MMSYSERR_NOTENABLED;
}

/**************************************************************************
 * 				widMessage (WINEOSS.6)
 */
DWORD WINAPI OSS_widMessage(WORD wDevID, WORD wMsg, DWORD dwUser,
			    DWORD dwParam1, DWORD dwParam2)
{
    FIXME("(%u, %04X, %08lX, %08lX, %08lX):stub\n", wDevID, wMsg, dwUser, dwParam1, dwParam2);
    return MMSYSERR_NOTENABLED;
}

#endif /* HAVE_OSS */
