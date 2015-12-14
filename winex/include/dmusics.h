#ifndef _DMUSICS_
#define _DMUSICS_

#include "dmusicc.h"

#define REGSTR_PATH_SOFTWARESYNTHS  "Software\\Microsoft\\DirectMusic\\SoftwareSynths"

struct IDirectMusicSynth;
struct IDirectMusicSynth8;
struct IDirectMusicSynthSink;

#ifndef __cplusplus

typedef struct IDirectMusicSynth IDirectMusicSynth;
typedef struct IDirectMusicSynth8 IDirectMusicSynth8;
typedef struct IDirectMusicSynthSink IDirectMusicSynthSink;

#endif

#ifndef _DMUS_VOICE_STATE_DEFINED
#define _DMUS_VOICE_STATE_DEFINED

typedef struct _DMUS_VOICE_STATE
{
    BOOL                bExists;
    SAMPLE_POSITION     spPosition;
} DMUS_VOICE_STATE; 

#endif /* _DMUS_VOICE_STATE_DEFINED */

#define REFRESH_F_LASTBUFFER        0x00000001

/*****************************************************************************
 * IDirectMusicSynth interface
 */

/* FIXME: Our macros can't handle HRESULT  CALLBACK (*lpFreeHandle)(HANDLE,HANDLE) in parm list */

#define ICOM_INTERFACE IDirectMusicSynth
#define IDirectMusicSynth_METHODS \
    ICOM_METHOD1( HRESULT,Open, LPDMUS_PORTPARAMS,pPortParams ) \
    ICOM_METHOD ( HRESULT,Close ) \
    ICOM_METHOD1( HRESULT,SetNumChannelGroups, DWORD,dwGroups ) \
    ICOM_METHOD3( HRESULT,Download, LPHANDLE,phDownload, LPVOID,pvData, LPBOOL,pbFree ) \
    ICOM_METHOD3( HRESULT,Unload, HANDLE,hDownload,/*FIXME*/void*,FIXME, HANDLE,hUserData ) \
    ICOM_METHOD3( HRESULT,PlayBuffer, REFERENCE_TIME,rt, LPBYTE,pbBuffer, DWORD,cbBuffer ) \
    ICOM_METHOD1( HRESULT,GetRunningStats, LPDMUS_SYNTHSTATS,pStats ) \
    ICOM_METHOD1( HRESULT,GetPortCaps, LPDMUS_PORTCAPS,pCaps) \
    ICOM_METHOD1( HRESULT,SetMasterClock, IReferenceClock*,pClock ) \
    ICOM_METHOD1( HRESULT,GetLatencyClock, IReferenceClock**,ppClock ) \
    ICOM_METHOD1( HRESULT,Activate, BOOL,fEnable )\
    ICOM_METHOD1( HRESULT,SetSynthSink, IDirectMusicSynthSink*,pSynthSink ) \
    ICOM_METHOD3( HRESULT,Render, short*,pBuffer, DWORD,dwLength, LONGLONG,llPosition ) \
    ICOM_METHOD3( HRESULT,SetChannelPriority, DWORD,dwChannelGroup, DWORD,dwChannel, DWORD,dwPriority ) \
    ICOM_METHOD3( HRESULT,GetChannelPriority, DWORD,dwChannelGroup, DWORD,dwChannel, LPDWORD,pdwPriority ) \
    ICOM_METHOD2( HRESULT,GetFormat, LPWAVEFORMATEX,pWaveFormatEx,  LPDWORD, pdwWaveFormatExSize ) \
    ICOM_METHOD1( HRESULT,GetAppend, DWORD*,pdwAppend )

#define IDirectMusicSynth_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicSynth_METHODS

ICOM_DEFINE(IDirectMusicSynth,IUnknown)
#undef ICOM_INTERFACE


/*****************************************************************************
 * IDirectMusicSynth8 interface
 */
#define ICOM_INTERFACE IDirectMusicSynth8
#define IDirectMusicSynth8_METHODS \
    ICOM_METHOD10( HRESULT,PlayVoice, REFERENCE_TIME,rt, DWORD,dwVoiceId, DWORD,dwChannelGroup, \
                                     DWORD,dwChannel, DWORD,dwDLId, long,prPitch, long,vrVolume, \
                                     SAMPLE_TIME,stVoiceStart, SAMPLE_TIME,stLoopStart, SAMPLE_TIME,stLoopEnd ) \
    ICOM_METHOD2( HRESULT,StopVoice, REFERENCE_TIME,rt, DWORD,dwVoiceId ) \
    ICOM_METHOD3( HRESULT,GetVoiceState, DWORD*,dwVoice, DWORD,cbVoice, DMUS_VOICE_STATE*,dwVoiceState ) \
    ICOM_METHOD2( HRESULT,Refresh, DWORD,dwDownloadID, DWORD,dwFlags) \
    ICOM_METHOD4( HRESULT,AssignChannelToBuses, DWORD,dwChannelGroup, DWORD,dwChannel, LPDWORD,pdwBuses, DWORD,cBuses)


#define IDirectMusicSynth8_IMETHODS \
    IDirectMusicSynth_METHODS \
    IDirectMusicSynth8_METHODS
ICOM_DEFINE(IDirectMusicSynth8,IDirectMusicSynth)
#undef ICOM_INTERFACE

/*****************************************************************************
 * IDirectMusicSynthSink interface
 */
#define ICOM_INTERFACE IDirectMusicSynthSink
#define IDirectMusicSynthSink_METHODS \
    ICOM_METHOD1( HRESULT,Init, IDirectMusicSynth*,pSynth ) \
    ICOM_METHOD1( HRESULT,SetMasterClock, IReferenceClock*,pClock) \
    ICOM_METHOD1( HRESULT,GetLatencyClock, IReferenceClock**,ppClock ) \
    ICOM_METHOD1( HRESULT,Activate, BOOL,fEnable ) \
    ICOM_METHOD2( HRESULT,SampleToRefTime, LONGLONG,llSampleTime, REFERENCE_TIME*,prfTime ) \
    ICOM_METHOD2( HRESULT,RefTimeToSample, REFERENCE_TIME,rfTime, LONGLONG*,pllSampleTime ) \
    ICOM_METHOD2( HRESULT,SetDirectSound, LPDIRECTSOUND,pDirectSound, LPDIRECTSOUNDBUFFER,pDirectSoundBuffer ) \
    ICOM_METHOD1( HRESULT,GetDesiredBufferSize, LPDWORD,pdwBufferSizeInSamples )     

#define IDirectMusicSynthSink_IMETHODS \
    IUnknown_METHODS \
    IDirectMusicSynthSink_METHODS
ICOM_DEFINE(IDirectMusicSynthSink,IUnknown)
#undef ICOM_INTERFACE

/* GUIDs */
DEFINE_GUID(IID_IDirectMusicSynth, 0x9823661,  0x5c85, 0x11d2, 0xaf, 0xa6, 0x0, 0xaa, 0x0, 0x24, 0xd8, 0xb6);
DEFINE_GUID(IID_IDirectMusicSynth8,0x53cab625, 0x2711, 0x4c9f, 0x9d, 0xe7, 0x1b, 0x7f, 0x92, 0x5f, 0x6f, 0xc8);
DEFINE_GUID(IID_IDirectMusicSynthSink,0x9823663, 0x5c85, 0x11d2, 0xaf, 0xa6, 0x0, 0xaa, 0x0, 0x24, 0xd8, 0xb6);

/* Prop GUIDs */
DEFINE_GUID(GUID_DMUS_PROP_SetSynthSink,0x0a3a5ba5, 0x37b6, 0x11d2, 0xb9, 0xf9, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12);
DEFINE_GUID(GUID_DMUS_PROP_SinkUsesDSound, 0xbe208857, 0x8952, 0x11d2, 0xba, 0x1c, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12); 

#endif /* _DMUSICS_ */
