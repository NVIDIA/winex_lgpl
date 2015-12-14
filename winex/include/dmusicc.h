/* FIXME: Lacking */
#ifndef _DMUSICC_
#define _DMUSICC_

#define COM_NO_WINDOWS_H
#include "objbase.h"

#include "strmif.h" /* FIXME: Not correct */

#include "mmsystem.h"
#include "dls1.h"
#include "dmerror.h"
#include "dmdls.h"
#include "dsound.h"
#include "dmusbuff.h"

typedef ULONGLONG    SAMPLE_TIME;
typedef ULONGLONG    SAMPLE_POSITION;   
typedef SAMPLE_TIME *LPSAMPLE_TIME;

#define DMUS_MAX_DESCRIPTION 128
#define DMUS_MAX_DRIVER 128

typedef struct _DMUS_BUFFERDESC *LPDMUS_BUFFERDESC;
typedef struct _DMUS_BUFFERDESC
{
  DWORD dwSize;
  DWORD dwFlags;
  GUID guidBufferFormat;
  DWORD cbBuffer;
} DMUS_BUFFERDESC;

#define DMUS_EFFECT_NONE             0x00000000
#define DMUS_EFFECT_REVERB           0x00000001
#define DMUS_EFFECT_CHORUS           0x00000002

#define DMUS_PC_INPUTCLASS       (0)
#define DMUS_PC_OUTPUTCLASS      (1)

#define DMUS_PC_DLS              (0x00000001)
#define DMUS_PC_EXTERNAL         (0x00000002)
#define DMUS_PC_SOFTWARESYNTH    (0x00000004)
#define DMUS_PC_MEMORYSIZEFIXED  (0x00000008)
#define DMUS_PC_GMINHARDWARE     (0x00000010)
#define DMUS_PC_GSINHARDWARE     (0x00000020)
#define DMUS_PC_XGINHARDWARE     (0x00000040)
#define DMUS_PC_DIRECTSOUND      (0x00000080)
#define DMUS_PC_SHAREABLE        (0x00000100)
#define DMUS_PC_DLS2             (0x00000200)
#define DMUS_PC_AUDIOPATH        (0x00000400)
#define DMUS_PC_WAVE             (0x00000800)

#define DMUS_PC_SYSTEMMEMORY     (0x7FFFFFFF)

typedef struct _DMUS_PORTCAPS
{
  DWORD   dwSize;
  DWORD   dwFlags;
  GUID    guidPort;
  DWORD   dwClass;
  DWORD   dwType;
  DWORD   dwMemorySize;
  DWORD   dwMaxChannelGroups;
  DWORD   dwMaxVoices;
  DWORD   dwMaxAudioChannels;
  DWORD   dwEffectFlags;
  WCHAR   wszDescription[DMUS_MAX_DESCRIPTION];
} DMUS_PORTCAPS;

typedef DMUS_PORTCAPS *LPDMUS_PORTCAPS;

#define DMUS_PORT_WINMM_DRIVER      (0)
#define DMUS_PORT_USER_MODE_SYNTH   (1)
#define DMUS_PORT_KERNEL_MODE       (2)
    
#define DMUS_PORTPARAMS_VOICES           0x00000001
#define DMUS_PORTPARAMS_CHANNELGROUPS    0x00000002
#define DMUS_PORTPARAMS_AUDIOCHANNELS    0x00000004
#define DMUS_PORTPARAMS_SAMPLERATE       0x00000008
#define DMUS_PORTPARAMS_EFFECTS          0x00000020
#define DMUS_PORTPARAMS_SHARE            0x00000040
#define DMUS_PORTPARAMS_FEATURES         0x00000080

typedef struct _DMUS_PORTPARAMS
{
  DWORD   dwSize;
  DWORD   dwValidParams;
  DWORD   dwVoices;
  DWORD   dwChannelGroups;
  DWORD   dwAudioChannels;
  DWORD   dwSampleRate;
  DWORD   dwEffectFlags;
  BOOL    fShare;
} DMUS_PORTPARAMS7;

typedef struct _DMUS_PORTPARAMS8
{
  DWORD   dwSize;
  DWORD   dwValidParams;
  DWORD   dwVoices;
  DWORD   dwChannelGroups;
  DWORD   dwAudioChannels;
  DWORD   dwSampleRate;
  DWORD   dwEffectFlags;
  BOOL    fShare;
  DWORD   dwFeatures;
} DMUS_PORTPARAMS8;

#define DMUS_PORT_FEATURE_AUDIOPATH     0x00000001
#define DMUS_PORT_FEATURE_STREAMING     0x00000002

typedef DMUS_PORTPARAMS8 DMUS_PORTPARAMS;
typedef DMUS_PORTPARAMS *LPDMUS_PORTPARAMS;

typedef struct _DMUS_SYNTHSTATS *LPDMUS_SYNTHSTATS;
typedef struct _DMUS_SYNTHSTATS8 *LPDMUS_SYNTHSTATS8;
typedef struct _DMUS_SYNTHSTATS
{
  DWORD   dwSize;
  DWORD   dwValidStats;
  DWORD   dwVoices;
  DWORD   dwTotalCPU;
  DWORD   dwCPUPerVoice;
  DWORD   dwLostNotes;
  DWORD   dwFreeMemory;
  long    lPeakVolume;
} DMUS_SYNTHSTATS;

typedef struct _DMUS_SYNTHSTATS8
{
  DWORD   dwSize;
  DWORD   dwValidStats;
  DWORD   dwVoices;
  DWORD   dwTotalCPU;
  DWORD   dwCPUPerVoice;
  DWORD   dwLostNotes;
  DWORD   dwFreeMemory;
  long    lPeakVolume;
  DWORD   dwSynthMemUse;
} DMUS_SYNTHSTATS8;

#define DMUS_SYNTHSTATS_VOICES          (1 << 0)
#define DMUS_SYNTHSTATS_TOTAL_CPU       (1 << 1)
#define DMUS_SYNTHSTATS_CPU_PER_VOICE   (1 << 2)
#define DMUS_SYNTHSTATS_LOST_NOTES      (1 << 3)
#define DMUS_SYNTHSTATS_PEAK_VOLUME     (1 << 4)
#define DMUS_SYNTHSTATS_FREE_MEMORY     (1 << 5)

#define DMUS_SYNTHSTATS_SYSTEMMEMORY    DMUS_PC_SYSTEMMEMORY

typedef struct _DMUS_WAVES_REVERB_PARAMS
{
  float   fInGain;
  float   fReverbMix;
  float   fReverbTime;
  float   fHighFreqRTRatio;
} DMUS_WAVES_REVERB_PARAMS;

typedef enum
{
  DMUS_CLOCK_SYSTEM = 0,
  DMUS_CLOCK_WAVE = 1
} DMUS_CLOCKTYPE;

#define DMUS_CLOCKF_GLOBAL              0x00000001

typedef struct _DMUS_CLOCKINFO7 *LPDMUS_CLOCKINFO7;
typedef struct _DMUS_CLOCKINFO7
{   
  DWORD           dwSize;
  DMUS_CLOCKTYPE  ctType;
  GUID            guidClock;
  WCHAR           wszDescription[DMUS_MAX_DESCRIPTION];
} DMUS_CLOCKINFO7;

typedef struct _DMUS_CLOCKINFO8 *LPDMUS_CLOCKINFO8;
typedef struct _DMUS_CLOCKINFO8
{
  DWORD           dwSize;
  DMUS_CLOCKTYPE  ctType;
  GUID            guidClock;
  WCHAR           wszDescription[DMUS_MAX_DESCRIPTION];
  DWORD           dwFlags;
} DMUS_CLOCKINFO8;

typedef DMUS_CLOCKINFO8 DMUS_CLOCKINFO;
typedef DMUS_CLOCKINFO *LPDMUS_CLOCKINFO;
    
#define DSBUSID_FIRST_SPKR_LOC              0
#define DSBUSID_FRONT_LEFT                  0
#define DSBUSID_LEFT                        0  
#define DSBUSID_FRONT_RIGHT                 1
#define DSBUSID_RIGHT                       1
#define DSBUSID_FRONT_CENTER                2
#define DSBUSID_LOW_FREQUENCY               3
#define DSBUSID_BACK_LEFT                   4
#define DSBUSID_BACK_RIGHT                  5
#define DSBUSID_FRONT_LEFT_OF_CENTER        6
#define DSBUSID_FRONT_RIGHT_OF_CENTER       7
#define DSBUSID_BACK_CENTER                 8
#define DSBUSID_SIDE_LEFT                   9
#define DSBUSID_SIDE_RIGHT                 10
#define DSBUSID_TOP_CENTER                 11
#define DSBUSID_TOP_FRONT_LEFT             12
#define DSBUSID_TOP_FRONT_CENTER           13
#define DSBUSID_TOP_FRONT_RIGHT            14
#define DSBUSID_TOP_BACK_LEFT              15
#define DSBUSID_TOP_BACK_CENTER            16
#define DSBUSID_TOP_BACK_RIGHT             17
#define DSBUSID_LAST_SPKR_LOC              17

#define DSBUSID_IS_SPKR_LOC(id) ( ((id) >= DSBUSID_FIRST_SPKR_LOC) && ((id) <= DSBUSID_LAST_SPKR_LOC) )

#define DSBUSID_REVERB_SEND                64
#define DSBUSID_CHORUS_SEND                65

#define DSBUSID_DYNAMIC_0                 512
#define DSBUSID_NULL                       0xFFFFFFFF

typedef struct IDirectMusic IDirectMusic,*LPDIRECTMUSIC;;
typedef struct IDirectMusic8 IDirectMusic8,*LPDIRECTMUSIC8;
typedef struct IDirectMusicPort IDirectMusicPort,*LPDIRECTMUSICPORT;
typedef struct IDirectMusicThru IDirectMusicThru,*LPDIRECTMUSICTHRU;
typedef struct IDirectMusicBuffer IDirectMusicBuffer,*LPDIRECTMUSICBUFFER;

#define ICOM_INTERFACE IDirectMusic
#define IDirectMusic_METHODS \
    ICOM_METHOD2(HRESULT,EnumPort,              DWORD, dwIndex, LPDMUS_PORTCAPS, lpPortCap) \
    ICOM_METHOD3(HRESULT,CreateMusicBuffer,     LPDMUS_BUFFERDESC, lpBufferDesc, LPDIRECTMUSICBUFFER*, lppBuffer, IUnknown*, pUnkOuter) \
    ICOM_METHOD4(HRESULT,CreatePort,            REFCLSID, rclsidPort, LPDMUS_PORTPARAMS, lpPortParams, LPDIRECTMUSICPORT*, lppPort, IUnknown*, pUnkOuter) \
    ICOM_METHOD2(HRESULT,EnumMasterClock,       DWORD, dwIndex, LPDMUS_CLOCKINFO, lpClockInfo) \
    ICOM_METHOD2(HRESULT,GetMasterClock,        LPGUID, pguidClock, IReferenceClock**, lppReferenceClock) \
    ICOM_METHOD1(HRESULT,SetMasterClock,        REFGUID, rguidClock) \
    ICOM_METHOD1(HRESULT,Activate,              BOOL, bEnable) \
    ICOM_METHOD1(HRESULT,GetDefaultPort,        LPGUID, lpguidPort) \
    ICOM_METHOD2(HRESULT,SetDirectSound,        LPDIRECTSOUND, lpDirectSound, HWND, hWnd)
#define IDirectMusic_IMETHODS \
      IUnknown_IMETHODS \
      IDirectMusic_METHODS
ICOM_DEFINE(IDirectMusic,IUnknown)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusic_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusic_AddRef(p)                  ICOM_CALL (AddRef,p)
#define IDirectMusic_Release(p)                 ICOM_CALL (Release,p)
	    /*** IDirectMusic methods ***/
#define IDirectMusic_EnumPort(p,a,b)            ICOM_CALL2(EnumPort,p,a,b)
#define IDirectMusic_CreateMusicBuffer(p,a,b,c) ICOM_CALL3(CreateMusicBuffer,p,a,b,c)
#define IDirectMusic_CreatePort(p,a,b,c,d)      ICOM_CALL4(CreatePort,p,a,b,c,d)
#define IDirectMusic_EnumMasterClock(p,a,b)     ICOM_CALL2(EnumMasterClock,p,a,b)
#define IDirectMusic_GetMasterClock(p,a,b)      ICOM_CALL2(GetMasterClock,p,a,b)
#define IDirectMusic_SetMasterClock(p,a)        ICOM_CALL1(SetMasterClock,p,a)
#define IDirectMusic_Activate(p,a)              ICOM_CALL1(Activate,p,a)
#define IDirectMusic_GetDefaultPort(p,a)        ICOM_CALL1(GetDefaultPort,p,a)
#define IDirectMusic_SetDirectSound(p,a,b)      ICOM_CALL2(SetDirectSound,p,a,b)

DEFINE_GUID(CLSID_DirectMusic,0x636b9f10,0x0c7d,0x11d1,0x95,0xb2,0x00,0x20,0xaf,0xdc,0x74,0x21);
DEFINE_GUID(CLSID_DirectMusicSynth,0x58C2B4D0,0x46E7,0x11D1,0x89,0xAC,0x00,0xA0,0xC9,0x05,0x41,0x29);
DEFINE_GUID(IID_IDirectMusic,0x6536115a,0x7b2d,0x11d2,0xba,0x18,0x00,0x00,0xf8,0x75,0xac,0x12);
DEFINE_GUID(CLSID_DirectMusicCollection,0x480ff4b0,0x28b2,0x11d1,0xbe,0xf7,0x0,0xc0,0x4f,0xbf,0x8f,0xef);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _DMUSICC_ */
