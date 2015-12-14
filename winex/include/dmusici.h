/* FIXME: A very lacking header file */

#ifndef _DMUSICI_
#define _DMUSICI_

#include <winnt.h>

#define COM_NO_WINDOWS_H
#include <objbase.h>

#include <mmsystem.h>
#include <dmusicc.h>

/* #include <pshpack8.h> */

#ifdef __cplusplus
extern "C" {
#endif

typedef WORD            TRANSITION_TYPE;
/* typedef __int64         REFERENCE_TIME; FIXME: Redefinition with strmif.h */
typedef long            MUSIC_TIME;

#define MT_MIN          0x80000000  /* Minimum music time value. */
#define MT_MAX          0x7FFFFFFF  /* Maximum music time value. */

#define DMUS_PPQ        768     /* parts per quarter note */

struct IDirectMusicTrack;
struct IDirectMusicPerformance;
struct IDirectMusicPerformance8;
struct IDirectMusicTool;
struct IDirectMusicSegment;
struct IDirectMusicSegment8;
struct IDirectMusicSegmentState;
struct IDirectMusicSegmentState8;
struct IDirectMusicGraph;
struct IDirectMusicInstrument;
struct IDirectMusicDownloadedInstrument;
struct IDirectMusicBand;
struct IDirectMusicChordMap;
struct IDirectMusicLoader;
struct IDirectMusicLoader8;
struct IDirectMusicGetLoader;
struct IDirectMusicScript;
struct IDirectMusicObject;
struct IDirectMusicStyle; /* Missing in MS headers */
struct IDirectMusicStyle8;
struct IDirectMusicPatternTrack;
struct IDirectMusicContainer;
struct IDirectMusicTool8;
struct IDirectMusicTrack8;
struct IDirectMusicSong;
struct IDirectMusicAudioPath;

#ifndef __cplusplus
typedef struct IDirectMusicTrack IDirectMusicTrack;
typedef struct IDirectMusicPerformance IDirectMusicPerformance;
typedef struct IDirectMusicPerformance8 IDirectMusicPerformance8;
typedef struct IDirectMusicTool IDirectMusicTool;
typedef struct IDirectMusicSegment IDirectMusicSegment;
typedef struct IDirectMusicSegment8 IDirectMusicSegment8;
typedef struct IDirectMusicSegmentState IDirectMusicSegmentState;
typedef struct IDirectMusicSegmentState8 IDirectMusicSegmentState8;
typedef struct IDirectMusicGraph IDirectMusicGraph;
typedef struct IDirectMusicInstrument IDirectMusicInstrument;
typedef struct IDirectMusicDownloadedInstrument IDirectMusicDownloadedInstrument;
typedef struct IDirectMusicBand IDirectMusicBand;
typedef struct IDirectMusicChordMap IDirectMusicChordMap;
typedef struct IDirectMusicObject IDirectMusicObject;
typedef struct IDirectMusicLoader IDirectMusicLoader;
typedef struct IDirectMusicLoader8 IDirectMusicLoader8;
typedef struct IDirectMusicGetLoader IDirectMusicGetLoader;
typedef struct IDirectMusicScript IDirectMusicScript;
typedef struct IDirectMusicStyle IDirectMusicStyle; /* Missing in MS headers */
typedef struct IDirectMusicStyle8 IDirectMusicStyle8;
typedef struct IDirectMusicPatternTrack IDirectMusicPatternTrack;
typedef struct IDirectMusicContainer IDirectMusicContainer;
typedef struct IDirectMusicTool8 IDirectMusicTool8;
typedef struct IDirectMusicTrack8 IDirectMusicTrack8;
typedef struct IDirectMusicSong IDirectMusicSong;
typedef struct IDirectMusicAudioPath IDirectMusicAudioPath;
#endif

typedef enum enumDMUS_STYLET_TYPES
{
  DMUS_STYLET_PATTERN         = 0,
  DMUS_STYLET_MOTIF           = 1,
  DMUS_STYLET_FRAGMENT        = 2,
} DMUS_STYLET_TYPES;

typedef enum enumDMUS_COMMANDT_TYPES
{
  DMUS_COMMANDT_GROOVE            = 0,
  DMUS_COMMANDT_FILL              = 1,
  DMUS_COMMANDT_INTRO             = 2,
  DMUS_COMMANDT_BREAK             = 3,
  DMUS_COMMANDT_END               = 4,
  DMUS_COMMANDT_ENDANDINTRO       = 5
} DMUS_COMMANDT_TYPES;

typedef enum enumDMUS_SHAPET_TYPES
{
  DMUS_SHAPET_FALLING             = 0,
  DMUS_SHAPET_LEVEL               = 1,
  DMUS_SHAPET_LOOPABLE            = 2,
  DMUS_SHAPET_LOUD                = 3,
  DMUS_SHAPET_QUIET               = 4,
  DMUS_SHAPET_PEAKING             = 5,
  DMUS_SHAPET_RANDOM              = 6,
  DMUS_SHAPET_RISING              = 7,
  DMUS_SHAPET_SONG                = 8
} DMUS_SHAPET_TYPES;

typedef enum enumDMUS_COMPOSEF_FLAGS
{
  DMUS_COMPOSEF_NONE              = 0,
  DMUS_COMPOSEF_ALIGN             = 0x1,
  DMUS_COMPOSEF_OVERLAP           = 0x2,
  DMUS_COMPOSEF_IMMEDIATE         = 0x4,
  DMUS_COMPOSEF_GRID              = 0x8,
  DMUS_COMPOSEF_BEAT              = 0x10,
  DMUS_COMPOSEF_MEASURE           = 0x20,
  DMUS_COMPOSEF_AFTERPREPARETIME  = 0x40,
  DMUS_COMPOSEF_VALID_START_BEAT  = 0x80,
  DMUS_COMPOSEF_VALID_START_GRID  = 0x100,
  DMUS_COMPOSEF_VALID_START_TICK  = 0x200,
  DMUS_COMPOSEF_SEGMENTEND        = 0x400,
  DMUS_COMPOSEF_MARKER            = 0x800,
  DMUS_COMPOSEF_MODULATE          = 0x1000,
  DMUS_COMPOSEF_LONG              = 0x2000,
  DMUS_COMPOSEF_ENTIRE_TRANSITION = 0x4000,
  DMUS_COMPOSEF_1BAR_TRANSITION   = 0x8000,
  DMUS_COMPOSEF_ENTIRE_ADDITION   = 0x10000,
  DMUS_COMPOSEF_1BAR_ADDITION     = 0x20000,
  DMUS_COMPOSEF_VALID_START_MEASURE = 0x40000,
  DMUS_COMPOSEF_DEFAULT           = 0x80000,
  DMUS_COMPOSEF_NOINVALIDATE      = 0x100000,
  DMUS_COMPOSEF_USE_AUDIOPATH     = 0x200000,
  DMUS_COMPOSEF_INVALIDATE_PRI    = 0x400000
} DMUS_COMPOSEF_FLAGS;

#define DMUS_PMSG_PART \
  DWORD               dwSize; \
  DWORD               _padding_; /* so we don't need pshpack8.h */ \
  REFERENCE_TIME      rtTime; \
  MUSIC_TIME          mtTime; \
  DWORD               dwFlags; \
  DWORD               dwPChannel; \
  DWORD               dwVirtualTrackID; \
  IDirectMusicTool*   pTool; \
  IDirectMusicGraph*  pGraph; \
  DWORD               dwType; \
  DWORD               dwVoiceID; \
  DWORD               dwGroupID; \
  IUnknown*           punkUser;

typedef struct _DMUS_PMSG
{
  DMUS_PMSG_PART
} DMUS_PMSG;

#define DMUS_PCHANNEL_BROADCAST_PERFORMANCE     0xFFFFFFFF
#define DMUS_PCHANNEL_BROADCAST_AUDIOPATH       0xFFFFFFFE
#define DMUS_PCHANNEL_BROADCAST_SEGMENT         0xFFFFFFFD
#define DMUS_PCHANNEL_BROADCAST_GROUPS          0xFFFFFFFC

#define DMUS_PATH_SEGMENT           0x1000
#define DMUS_PATH_SEGMENT_TRACK     0x1100
#define DMUS_PATH_SEGMENT_GRAPH     0x1200
#define DMUS_PATH_SEGMENT_TOOL      0x1300
#define DMUS_PATH_AUDIOPATH         0x2000
#define DMUS_PATH_AUDIOPATH_GRAPH   0x2200
#define DMUS_PATH_AUDIOPATH_TOOL    0x2300
#define DMUS_PATH_PERFORMANCE       0x3000
#define DMUS_PATH_PERFORMANCE_GRAPH 0x3200
#define DMUS_PATH_PERFORMANCE_TOOL  0x3300
#define DMUS_PATH_PORT              0x4000
#define DMUS_PATH_BUFFER            0x6000
#define DMUS_PATH_BUFFER_DMO        0x6100
#define DMUS_PATH_MIXIN_BUFFER      0x7000
#define DMUS_PATH_MIXIN_BUFFER_DMO  0x7100
#define DMUS_PATH_PRIMARY_BUFFER    0x8000

#define DMUS_PCHANNEL_ALL           0xFFFFFFFB

#define DMUS_APATH_SHARED_STEREOPLUSREVERB   1
#define DMUS_APATH_DYNAMIC_3D                6
#define DMUS_APATH_DYNAMIC_MONO              7
#define DMUS_APATH_DYNAMIC_STEREO            8

typedef struct _DMUS_AUDIOPARAMS
{
  DWORD   dwSize;
  BOOL    fInitNow;
  DWORD   dwValidData;
  DWORD   dwFeatures;
  DWORD   dwVoices;
  DWORD   dwSampleRate;
  CLSID   clsidDefaultSynth;
} DMUS_AUDIOPARAMS;

/* dwFeatures flags */
#define DMUS_AUDIOF_3D          0x1
#define DMUS_AUDIOF_ENVIRON     0x2
#define DMUS_AUDIOF_EAX         0x4
#define DMUS_AUDIOF_DMOS        0x8
#define DMUS_AUDIOF_STREAMING   0x10
#define DMUS_AUDIOF_BUFFERS     0x20
#define DMUS_AUDIOF_ALL         0x3F

/* dwValidData flags */
#define DMUS_AUDIOPARAMS_FEATURES       0x00000001
#define DMUS_AUDIOPARAMS_VOICES         0x00000002
#define DMUS_AUDIOPARAMS_SAMPLERATE     0x00000004
#define DMUS_AUDIOPARAMS_DEFAULTSYNTH   0x00000008

typedef enum enumDMUS_PMSGF_FLAGS
{
  DMUS_PMSGF_REFTIME          = 1,
  DMUS_PMSGF_MUSICTIME        = 2,
  DMUS_PMSGF_TOOL_IMMEDIATE   = 4,
  DMUS_PMSGF_TOOL_QUEUE       = 8,
  DMUS_PMSGF_TOOL_ATTIME      = 0x10,
  DMUS_PMSGF_TOOL_FLUSH       = 0x20,
  DMUS_PMSGF_LOCKTOREFTIME    = 0x40,
  DMUS_PMSGF_DX8              = 0x80
} DMUS_PMSGF_FLAGS;

typedef enum enumDMUS_PMSGT_TYPES
{
  DMUS_PMSGT_MIDI             = 0,
  DMUS_PMSGT_NOTE             = 1,
  DMUS_PMSGT_SYSEX            = 2,
  DMUS_PMSGT_NOTIFICATION     = 3,
  DMUS_PMSGT_TEMPO            = 4,
  DMUS_PMSGT_CURVE            = 5,
  DMUS_PMSGT_TIMESIG          = 6,
  DMUS_PMSGT_PATCH            = 7,
  DMUS_PMSGT_TRANSPOSE        = 8,
  DMUS_PMSGT_CHANNEL_PRIORITY = 9,
  DMUS_PMSGT_STOP             = 10,
  DMUS_PMSGT_DIRTY            = 11,
  DMUS_PMSGT_WAVE             = 12,
  DMUS_PMSGT_LYRIC            = 13,
  DMUS_PMSGT_SCRIPTLYRIC      = 14,
  DMUS_PMSGT_USER             = 255
} DMUS_PMSGT_TYPES;

typedef enum enumDMUS_SEGF_FLAGS
{
  DMUS_SEGF_REFTIME           = 1<<6,   /* 0x40 */
  DMUS_SEGF_SECONDARY         = 1<<7,   /* 0x80 */
  DMUS_SEGF_QUEUE             = 1<<8,   /* 0x100 */
  DMUS_SEGF_CONTROL           = 1<<9,   /* 0x200 */
  DMUS_SEGF_AFTERPREPARETIME  = 1<<10,  /* 0x400 */
  DMUS_SEGF_GRID              = 1<<11,  /* 0x800 */
  DMUS_SEGF_BEAT              = 1<<12,  /* 0x1000 */
  DMUS_SEGF_MEASURE           = 1<<13,  /* 0x2000 */
  DMUS_SEGF_DEFAULT           = 1<<14,  /* 0x4000 */
  DMUS_SEGF_NOINVALIDATE      = 1<<15,  /* 0x8000 */
  DMUS_SEGF_ALIGN             = 1<<16,  /* 0x10000 */
  DMUS_SEGF_VALID_START_BEAT  = 1<<17,  /* 0x20000 */
  DMUS_SEGF_VALID_START_GRID  = 1<<18,  /* 0x40000 */
  DMUS_SEGF_VALID_START_TICK  = 1<<19,  /* 0x80000 */
  DMUS_SEGF_AUTOTRANSITION    = 1<<20,  /* 0x100000 */
  DMUS_SEGF_AFTERQUEUETIME    = 1<<21,  /* 0x200000 */
  DMUS_SEGF_AFTERLATENCYTIME  = 1<<22,  /* 0x400000 */
  DMUS_SEGF_SEGMENTEND        = 1<<23,  /* 0x800000 */
  DMUS_SEGF_MARKER            = 1<<24,  /* 0x1000000 */
  DMUS_SEGF_TIMESIG_ALWAYS    = 1<<25,  /* 0x2000000 */
  DMUS_SEGF_USE_AUDIOPATH     = 1<<26,  /* 0x4000000 */
  DMUS_SEGF_VALID_START_MEASURE = 1<<27, /* 0x8000000 */
  DMUS_SEGF_INVALIDATE_PRI    = 1<<28   /* 0x10000000 */
} DMUS_SEGF_FLAGS;

#define DMUS_SEG_REPEAT_INFINITE    0xFFFFFFFF
#define DMUS_SEG_ALLTRACKS          0x80000000
#define DMUS_SEG_ANYTRACK           0x80000000

typedef enum enumDMUS_TIME_RESOLVE_FLAGS
{
  DMUS_TIME_RESOLVE_AFTERPREPARETIME  = DMUS_SEGF_AFTERPREPARETIME,
  DMUS_TIME_RESOLVE_AFTERQUEUETIME    = DMUS_SEGF_AFTERQUEUETIME,
  DMUS_TIME_RESOLVE_AFTERLATENCYTIME  = DMUS_SEGF_AFTERLATENCYTIME,
  DMUS_TIME_RESOLVE_GRID              = DMUS_SEGF_GRID,
  DMUS_TIME_RESOLVE_BEAT              = DMUS_SEGF_BEAT,
  DMUS_TIME_RESOLVE_MEASURE           = DMUS_SEGF_MEASURE,
  DMUS_TIME_RESOLVE_MARKER            = DMUS_SEGF_MARKER,
  DMUS_TIME_RESOLVE_SEGMENTEND        = DMUS_SEGF_SEGMENTEND,
} DMUS_TIME_RESOLVE_FLAGS;

typedef enum enumDMUS_CHORDKEYF_FLAGS
{
  DMUS_CHORDKEYF_SILENT = 1
} DMUS_CHORDKEYF_FLAGS;

#define DMUS_MAXSUBCHORD 8

typedef struct _DMUS_SUBCHORD
{
  DWORD   dwChordPattern;
  DWORD   dwScalePattern;
  DWORD   dwInversionPoints;
  DWORD   dwLevels;
  BYTE    bChordRoot;
  BYTE    bScaleRoot;
} DMUS_SUBCHORD;

typedef struct _DMUS_CHORD_KEY
{
  WCHAR           wszName[16];
  WORD            wMeasure;
  BYTE            bBeat;
  BYTE            bSubChordCount;
  DMUS_SUBCHORD   SubChordList[DMUS_MAXSUBCHORD];
  DWORD           dwScale;
  BYTE            bKey;
  BYTE            bFlags;
} DMUS_CHORD_KEY;

typedef struct _DMUS_NOTE_PMSG
{
  DMUS_PMSG_PART

  MUSIC_TIME mtDuration;
  WORD    wMusicValue;
  WORD    wMeasure;
  short   nOffset;
  BYTE    bBeat;
  BYTE    bGrid;
  BYTE    bVelocity;
  BYTE    bFlags;
  BYTE    bTimeRange;
  BYTE    bDurRange;
  BYTE    bVelRange;
  BYTE    bPlayModeFlags;
  BYTE    bSubChordLevel;
  BYTE    bMidiValue;
  char    cTranspose;
} DMUS_NOTE_PMSG;

typedef enum enumDMUS_NOTEF_FLAGS
{
  DMUS_NOTEF_NOTEON = 1,

  DMUS_NOTEF_NOINVALIDATE = 2,
  DMUS_NOTEF_NOINVALIDATE_INSCALE = 4,
  DMUS_NOTEF_NOINVALIDATE_INCHORD = 8,
  DMUS_NOTEF_REGENERATE = 0x10
} DMUS_NOTEF_FLAGS;

typedef enum enumDMUS_PLAYMODE_FLAGS
{
  DMUS_PLAYMODE_KEY_ROOT          = 1,
  DMUS_PLAYMODE_CHORD_ROOT        = 2,
  DMUS_PLAYMODE_SCALE_INTERVALS   = 4,
  DMUS_PLAYMODE_CHORD_INTERVALS   = 8,
  DMUS_PLAYMODE_NONE              = 16
} DMUS_PLAYMODE_FLAGS;

#define DMUS_PLAYMODE_FIXED             0
#define DMUS_PLAYMODE_FIXEDTOKEY        DMUS_PLAYMODE_KEY_ROOT
#define DMUS_PLAYMODE_FIXEDTOCHORD      DMUS_PLAYMODE_CHORD_ROOT
#define DMUS_PLAYMODE_PEDALPOINT        (DMUS_PLAYMODE_KEY_ROOT | DMUS_PLAYMODE_SCALE_INTERVALS)
#define DMUS_PLAYMODE_MELODIC           (DMUS_PLAYMODE_CHORD_ROOT | DMUS_PLAYMODE_SCALE_INTERVALS)
#define DMUS_PLAYMODE_NORMALCHORD       (DMUS_PLAYMODE_CHORD_ROOT | DMUS_PLAYMODE_CHORD_INTERVALS)
#define DMUS_PLAYMODE_ALWAYSPLAY        (DMUS_PLAYMODE_MELODIC | DMUS_PLAYMODE_NORMALCHORD)
#define DMUS_PLAYMODE_PEDALPOINTCHORD   (DMUS_PLAYMODE_KEY_ROOT | DMUS_PLAYMODE_CHORD_INTERVALS)
#define DMUS_PLAYMODE_PEDALPOINTALWAYS  (DMUS_PLAYMODE_PEDALPOINT | DMUS_PLAYMODE_PEDALPOINTCHORD)

#define DMUS_PLAYMODE_PURPLEIZED        DMUS_PLAYMODE_ALWAYSPLAY
#define DMUS_PLAYMODE_SCALE_ROOT        DMUS_PLAYMODE_KEY_ROOT
#define DMUS_PLAYMODE_FIXEDTOSCALE      DMUS_PLAYMODE_FIXEDTOKEY

typedef struct _DMUS_MIDI_PMSG
{
  DMUS_PMSG_PART

  BYTE    bStatus;
  BYTE    bByte1;
  BYTE    bByte2;
  BYTE    bPad[1];
} DMUS_MIDI_PMSG;

typedef struct _DMUS_PATCH_PMSG
{
  DMUS_PMSG_PART

  BYTE    byInstrument;
  BYTE    byMSB;
  BYTE    byLSB;
  BYTE    byPad[1];
} DMUS_PATCH_PMSG;

typedef struct _DMUS_TRANSPOSE_PMSG
{
  DMUS_PMSG_PART

  short   nTranspose;
  WORD            wMergeIndex;
} DMUS_TRANSPOSE_PMSG;

typedef struct _DMUS_CHANNEL_PRIORITY_PMSG
{
  DMUS_PMSG_PART

  DWORD   dwChannelPriority;
} DMUS_CHANNEL_PRIORITY_PMSG;

typedef struct _DMUS_TEMPO_PMSG
{
  DMUS_PMSG_PART

  double  dblTempo;                       /* the tempo */
} DMUS_TEMPO_PMSG;

#define DMUS_TEMPO_MAX          1000
#define DMUS_TEMPO_MIN          1

#define DMUS_MASTERTEMPO_MAX    100.0f
#define DMUS_MASTERTEMPO_MIN    0.01f

typedef struct _DMUS_SYSEX_PMSG
{
  DMUS_PMSG_PART

  DWORD   dwLen;          /* length of the data */
  BYTE    abData[1];      /* array of data, length equal to dwLen */
} DMUS_SYSEX_PMSG;

typedef struct _DMUS_CURVE_PMSG
{
  DMUS_PMSG_PART

  MUSIC_TIME      mtDuration;
  MUSIC_TIME      mtOriginalStart;
  MUSIC_TIME      mtResetDuration;
  short           nStartValue;
  short           nEndValue;
  short           nResetValue;
  WORD            wMeasure;
  short           nOffset;
  BYTE            bBeat;
  BYTE            bGrid;
  BYTE            bType;
  BYTE            bCurveShape;
  BYTE            bCCData;
  BYTE            bFlags;

  WORD            wMergeIndex;
} DMUS_CURVE_PMSG;

typedef enum enumDMUS_CURVE_FLAGS
{
  DMUS_CURVE_RESET = 1,
  DMUS_CURVE_START_FROM_CURRENT = 2
} DMUS_CURVE_FLAGS;

#define DMUS_CURVE_RESET    1

enum
{
  DMUS_CURVES_LINEAR  = 0,
  DMUS_CURVES_INSTANT = 1,
  DMUS_CURVES_EXP     = 2,
  DMUS_CURVES_LOG     = 3,
  DMUS_CURVES_SINE    = 4
};

#define DMUS_CURVET_PBCURVE      0x03
#define DMUS_CURVET_CCCURVE      0x04
#define DMUS_CURVET_MATCURVE     0x05
#define DMUS_CURVET_PATCURVE     0x06
#define DMUS_CURVET_RPNCURVE     0x07
#define DMUS_CURVET_NRPNCURVE    0x08

typedef struct _DMUS_TIMESIG_PMSG
{
  DMUS_PMSG_PART

  BYTE    bBeatsPerMeasure;
  BYTE    bBeat;
  WORD    wGridsPerBeat;
} DMUS_TIMESIG_PMSG;

#define DMUS_NOTIFICATION_SEGSTART       0
#define DMUS_NOTIFICATION_SEGEND         1
#define DMUS_NOTIFICATION_SEGALMOSTEND   2
#define DMUS_NOTIFICATION_SEGLOOP        3
#define DMUS_NOTIFICATION_SEGABORT       4

#define DMUS_NOTIFICATION_MUSICSTARTED   0
#define DMUS_NOTIFICATION_MUSICSTOPPED   1
#define DMUS_NOTIFICATION_MUSICALMOSTEND 2

#define DMUS_NOTIFICATION_MEASUREBEAT    0

#define DMUS_NOTIFICATION_CHORD          0

#define DMUS_NOTIFICATION_GROOVE         0
#define DMUS_NOTIFICATION_EMBELLISHMENT  1

#define DMUS_NOTIFICATION_RECOMPOSE          0

typedef struct _DMUS_NOTIFICATION_PMSG
{
  DMUS_PMSG_PART

  GUID    guidNotificationType;
  DWORD   dwNotificationOption;
  DWORD   dwField1;
  DWORD   dwField2;
} DMUS_NOTIFICATION_PMSG;

typedef struct _DMUS_WAVE_PMSG
{
  DMUS_PMSG_PART

  REFERENCE_TIME rtStartOffset;
  REFERENCE_TIME rtDuration;
  long    lOffset;
  long    lVolume;
  long    lPitch;
  BYTE    bFlags;
} DMUS_WAVE_PMSG;

#define DMUS_WAVEF_OFF           1
#define DMUS_WAVEF_STREAMING     2
#define DMUS_WAVEF_NOINVALIDATE  4
#define DMUS_WAVEF_NOPREROLL     8

typedef struct _DMUS_LYRIC_PMSG
{
  DMUS_PMSG_PART

  WCHAR    wszString[1];
} DMUS_LYRIC_PMSG;

#define DMUS_MAX_NAME           64
#define DMUS_MAX_CATEGORY       64
#define DMUS_MAX_FILENAME       MAX_PATH

typedef struct _DMUS_VERSION {
  DWORD    dwVersionMS;
  DWORD    dwVersionLS;
} DMUS_VERSION, *LPDMUS_VERSION;

typedef struct _DMUS_TIMESIGNATURE
{
  MUSIC_TIME mtTime;
  BYTE    bBeatsPerMeasure;
  BYTE    bBeat;
  WORD    wGridsPerBeat;
} DMUS_TIMESIGNATURE;

typedef struct _DMUS_VALID_START_PARAM
{
  MUSIC_TIME mtTime;
} DMUS_VALID_START_PARAM;

typedef struct _DMUS_PLAY_MARKER_PARAM
{
  MUSIC_TIME mtTime;
} DMUS_PLAY_MARKER_PARAM;

typedef struct _DMUS_OBJECTDESC
{
  DWORD          dwSize;
  DWORD          dwValidData;
  GUID           guidObject;
  GUID           guidClass;
  FILETIME       ftDate;
  DMUS_VERSION   vVersion;
  WCHAR          wszName[DMUS_MAX_NAME];
  WCHAR          wszCategory[DMUS_MAX_CATEGORY];
  WCHAR          wszFileName[DMUS_MAX_FILENAME];
  LONGLONG       llMemLength;
  LPBYTE         pbMemData;
  IStream *      pStream;
} DMUS_OBJECTDESC, *LPDMUS_OBJECTDESC;

#define DMUS_OBJ_OBJECT         (1 << 0)
#define DMUS_OBJ_CLASS          (1 << 1)
#define DMUS_OBJ_NAME           (1 << 2)
#define DMUS_OBJ_CATEGORY       (1 << 3)
#define DMUS_OBJ_FILENAME       (1 << 4)
#define DMUS_OBJ_FULLPATH       (1 << 5)
#define DMUS_OBJ_URL            (1 << 6)
#define DMUS_OBJ_VERSION        (1 << 7)
#define DMUS_OBJ_DATE           (1 << 8)
#define DMUS_OBJ_LOADED         (1 << 9)
#define DMUS_OBJ_MEMORY         (1 << 10)
#define DMUS_OBJ_STREAM         (1 << 11)

typedef struct _DMUS_SCRIPT_ERRORINFO
{
  DWORD dwSize;
  HRESULT hr;
  ULONG ulLineNumber;
  LONG ichCharPosition;
  WCHAR wszSourceFile[DMUS_MAX_FILENAME];
  WCHAR wszSourceComponent[DMUS_MAX_FILENAME];
  WCHAR wszDescription[DMUS_MAX_FILENAME];
  WCHAR wszSourceLineText[DMUS_MAX_FILENAME];
} DMUS_SCRIPT_ERRORINFO;

#define DMUS_TRACKCONFIG_OVERRIDE_ALL           1
#define DMUS_TRACKCONFIG_OVERRIDE_PRIMARY       2
#define DMUS_TRACKCONFIG_FALLBACK               4
#define DMUS_TRACKCONFIG_CONTROL_ENABLED        8
#define DMUS_TRACKCONFIG_PLAY_ENABLED           0x10
#define DMUS_TRACKCONFIG_NOTIFICATION_ENABLED   0x20
#define DMUS_TRACKCONFIG_PLAY_CLOCKTIME         0x40
#define DMUS_TRACKCONFIG_PLAY_COMPOSE           0x80
#define DMUS_TRACKCONFIG_LOOP_COMPOSE           0x100
#define DMUS_TRACKCONFIG_COMPOSING              0x200
#define DMUS_TRACKCONFIG_TRANS1_FROMSEGSTART    0x400
#define DMUS_TRACKCONFIG_TRANS1_FROMSEGCURRENT  0x800
#define DMUS_TRACKCONFIG_TRANS1_TOSEGSTART      0x1000
#define DMUS_TRACKCONFIG_CONTROL_PLAY           0x10000
#define DMUS_TRACKCONFIG_CONTROL_NOTIFICATION   0x20000

#define DMUS_TRACKCONFIG_DEFAULT    (DMUS_TRACKCONFIG_CONTROL_ENABLED | DMUS_TRACKCONFIG_PLAY_ENABLED | DMUS_TRACKCONFIG_NOTIFICATION_ENABLED)

#define DMUS_MAX_FRAGMENTLABEL 20

#define DMUS_FRAGMENTF_USE_REPEAT      0x1
#define DMUS_FRAGMENTF_REJECT_REPEAT   (0x1 << 1)
#define DMUS_FRAGMENTF_USE_LABEL       (0x1 << 2)

#define DMUS_CONNECTIONF_INTERVALS     (0x1 << 1)
#define DMUS_CONNECTIONF_OVERLAP       (0x1 << 2)

typedef struct _DMUS_COMMAND_PARAM
{
  BYTE bCommand;
  BYTE bGrooveLevel;
  BYTE bGrooveRange;
  BYTE bRepeatMode;
} DMUS_COMMAND_PARAM;

typedef struct _DMUS_COMMAND_PARAM_2
{
  MUSIC_TIME mtTime;
  BYTE bCommand;
  BYTE bGrooveLevel;
  BYTE bGrooveRange;
  BYTE bRepeatMode;
} DMUS_COMMAND_PARAM_2;

typedef struct _DMUS_CONNECTION_RULE
{
  DWORD       dwFlags;
  DWORD       dwIntervals;
} DMUS_CONNECTION_RULE;

typedef struct _DMUS_MELODY_FRAGMENT
{
  MUSIC_TIME  mtTime;
  DWORD       dwID;
  WCHAR       wszVariationLabel[DMUS_MAX_FRAGMENTLABEL];
  DWORD       dwVariationFlags;
  DWORD       dwRepeatFragmentID;
  DWORD       dwFragmentFlags;
  DWORD       dwPlayModeFlags;
  DWORD       dwTransposeIntervals;
  DMUS_COMMAND_PARAM      Command;
  DMUS_CONNECTION_RULE    ConnectionArc;
} DMUS_MELODY_FRAGMENT;

typedef IDirectMusicObject *LPDMUS_OBJECT;
typedef IDirectMusicLoader *LPDMUS_LOADER;
typedef IDirectMusicBand *LPDMUS_BAND;

#define DMUSB_LOADED    (1 << 0)
#define DMUSB_DEFAULT   (1 << 1)

#define ICOM_INTERFACE IDirectMusicBand
#define IDirectMusicBand_METHODS \
    ICOM_METHOD1(HRESULT, CreateSegment, IDirectMusicSegment**,ppSegment) \
    ICOM_METHOD1(HRESULT, Download, IDirectMusicPerformance*,pPerformance) \
    ICOM_METHOD1(HRESULT, Unload, IDirectMusicPerformance*, pPerformance)

#define IDirectMusicBand_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicBand_METHODS
ICOM_DEFINE(IDirectMusicBand,IUnknown)
typedef IDirectMusicBand IDirectMusicBand8;
#undef ICOM_INTERFACE


/*****************************************************************************
 * IDirectMusicObject interface
 */
#define ICOM_INTERFACE IDirectMusicObject
#define IDirectMusicObject_METHODS \
    ICOM_METHOD1(HRESULT,GetDescriptor, LPDMUS_OBJECTDESC, pDesc) \
    ICOM_METHOD1(HRESULT,SetDescriptor, LPDMUS_OBJECTDESC, pDesc) \
    ICOM_METHOD2(HRESULT,ParseDescriptor, LPSTREAM, pStream, LPDMUS_OBJECTDESC,pDesc )
#define IDirectMusicObject_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicObject_METHODS
ICOM_DEFINE(IDirectMusicObject,IUnknown)
typedef IDirectMusicObject IDirectMusicObject8;
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicObject_QueryInterface(p,a,b)  ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicObject_AddRef(p)              ICOM_CALL (AddRef,p)
#define IDirectMusicObject_Release(p)             ICOM_CALL (Release,p)
    /*** IDirectMusicObject methods ***/
#define IDirectMusicObject_GetDescriptor(p,a)     ICOM_CALL1(GetDescriptor,p,a)
#define IDirectMusicObject_SetDescriptor(p,a)     ICOM_CALL1(SetDescriptor,p,a)
#define IDirectMusicObject_ParseDescriptor(p,a,b) ICOM_CALL2(ParseDescriptor,p,a,b)

/*****************************************************************************
 * IDirectMusicLoader interface
 */
#define ICOM_INTERFACE IDirectMusicLoader
#define IDirectMusicLoader_METHODS \
    ICOM_METHOD3(HRESULT,GetObject,          LPDMUS_OBJECTDESC,pDesc, REFIID,riid, LPVOID*,ppv) \
    ICOM_METHOD1(HRESULT,SetObject,          LPDMUS_OBJECTDESC,pDesc) \
    ICOM_METHOD3(HRESULT,SetSearchDirectory, REFGUID,rguidClass, WCHAR*,pwzPath, BOOL,fClear) \
    ICOM_METHOD3(HRESULT,ScanDirectory,      REFGUID,rguidClass, WCHAR*,pwzFileExtension, WCHAR*,pwzScanFileName) \
    ICOM_METHOD1(HRESULT,CacheObject,        IDirectMusicObject*,pObject) \
    ICOM_METHOD1(HRESULT,ReleaseObject,      IDirectMusicObject*,pObject) \
    ICOM_METHOD1(HRESULT,ClearCache,         REFGUID,rguidClass) \
    ICOM_METHOD2(HRESULT,EnableCache,        REFGUID,rguidClass, BOOL,fEnable) \
    ICOM_METHOD3(HRESULT,EnumObject,         REFGUID,rguidClass, DWORD,dwIndex, LPDMUS_OBJECTDESC,pDesc)
#define IDirectMusicLoader_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicLoader_METHODS
ICOM_DEFINE(IDirectMusicLoader,IUnknown)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicLoader_QueryInterface(p,a,b)       ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicLoader_AddRef(p)                   ICOM_CALL (AddRef,p)
#define IDirectMusicLoader_Release(p)                  ICOM_CALL (Release,p)
    /*** IDirectMusicLoader methods ***/
#define IDirectMusicLoader_GetObject(p,a,b,c)          ICOM_CALL3(GetObject,p,a,b,c)
#define IDirectMusicLoader_SetObject(p,a)              ICOM_CALL1(SetObject,p,a)
#define IDirectMusicLoader_SetSearchDirectory(p,a,b,c) ICOM_CALL3(SetSearchDirectory,p,a,b,c)
#define IDirectMusicLoader_ScanDirectory(p,a,b,c)      ICOM_CALL3(ScanDirectory,p,a,b,c)
#define IDirectMusicLoader_CacheObject(p,a)            ICOM_CALL1(CacheObject,p,a)
#define IDirectMusicLoader_ReleaseObject(p,a)          ICOM_CALL1(ReleaseObject,p,a)
#define IDirectMusicLoader_ClearCache(p,a)             ICOM_CALL1(ClearCache,p,a)
#define IDirectMusicLoader_EnableCache(p,a,b)          ICOM_CALL2(EnableCache,p,a,b)
#define IDirectMusicLoader_EnumObject(p,a,b,c)         ICOM_CALL3(EnumObject,p,a,b,c)

/*****************************************************************************
 * IDirectMusicLoader8 interface
 */
#define ICOM_INTERFACE IDirectMusicLoader8
#define IDirectMusicLoader8_METHODS \
    ICOM_METHOD(void,CollectGarbage) \
    ICOM_METHOD1(HRESULT,ReleaseObjectByUnknown, IUnknown*,pObject) \
    ICOM_METHOD4(HRESULT,LoadObjectFromFile,     REFGUID,rguidClassID, REFIID,iidInterfaceID, WCHAR*,pwzFilePath, void**,ppObject)
#define IDirectMusicLoader8_IMETHODS \
    IDirectMusicLoader_IMETHODS \
    IDirectMusicLoader8_METHODS
ICOM_DEFINE(IDirectMusicLoader8,IDirectMusicLoader)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicLoader8_QueryInterface(p,a,b)         ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicLoader8_AddRef(p)                     ICOM_CALL (AddRef,p)
#define IDirectMusicLoader8_Release(p)                    ICOM_CALL (Release,p)
    /*** IDirectMusicLoader methods ***/
#define IDirectMusicLoader8_GetObject(p,a,b,c)            ICOM_CALL3(GetObject,p,a,b,c)
#define IDirectMusicLoader8_SetObject(p,a)                ICOM_CALL1(SetObject,p,a)
#define IDirectMusicLoader8_SetSearchDirectory(p,a,b,c)   ICOM_CALL3(SetSearchDirectory,p,a,b,c)
#define IDirectMusicLoader8_ScanDirectory(p,a,b,c)        ICOM_CALL3(ScanDirectory,p,a,b,c)
#define IDirectMusicLoader8_CacheObject(p,a)              ICOM_CALL1(CacheObject,p,a)
#define IDirectMusicLoader8_ReleaseObject(p,a)            ICOM_CALL1(ReleaseObject,p,a)
#define IDirectMusicLoader8_ClearCache(p,a)               ICOM_CALL1(ClearCache,p,a)
#define IDirectMusicLoader8_EnableCache(p,a,b)            ICOM_CALL2(EnableCache,p,a,b)
#define IDirectMusicLoader8_EnumObject(p,a,b,c)           ICOM_CALL3(EnumObject,p,a,b,c)
    /*** IDirectMusicLoader8 methods ***/
#define IDirectMusicLoader8_CollectGarbage(p)             ICOM_CALL (CollectGarbage,p)
#define IDirectMusicLoader8_ReleaseObjectByUnknown(p,a)   ICOM_CALL1(ReleaseObjectByUnknown,p,a)
#define IDirectMusicLoader8_LoadObjectFromFile(p,a,b,c,d) ICOM_CALL4(LoadObjectFromFile,p,a,b,c,d)

/*****************************************************************************
 * IDirectMusicGetLoader interface
 */
#define ICOM_INTERFACE IDirectMusicGetLoader
#define IDirectMusicGetLoader_METHODS \
    ICOM_METHOD1(HRESULT,GetLoader, IDirectMusicLoader**,ppLoader)
#define IDirectMusicGetLoader_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicGetLoader_METHODS
ICOM_DEFINE(IDirectMusicGetLoader,IUnknown)
typedef IDirectMusicGetLoader IDirectMusicGetLoader8;
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicGetLoader_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicGetLoader_AddRef(p)             ICOM_CALL (AddRef,p)
#define IDirectMusicGetLoader_Release(p)            ICOM_CALL (Release,p)
    /*** IDirectMusicGetLoader methods ***/
#define IDirectMusicGetLoader_GetLoader(p,a)        ICOM_CALL1(GetLoader,p,a)

/*****************************************************************************
 * IDirectMusicAudioPath interface
 */
#define ICOM_INTERFACE IDirectMusicAudioPath
#define IDirectMusicAudioPath_METHODS \
    ICOM_METHOD7(HRESULT,GetObjectInPath, DWORD,dwPChannel, DWORD,dwStage, DWORD,dwBuffer, REFGUID,guidObject, DWORD,dwIndex, REFGUID,iidInterface, void**,ppObject) \
    ICOM_METHOD1(HRESULT,Activate,        BOOL,fActivate) \
    ICOM_METHOD2(HRESULT,SetVolume,       long,lVolume, DWORD,dwDuration) \
    ICOM_METHOD2(HRESULT,ConvertPChannel, DWORD,dwPChannelIn, DWORD*, pdwPChannelOut)
#define IDirectMusicAudioPath_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicAudioPath_METHODS
ICOM_DEFINE(IDirectMusicAudioPath,IUnknown)
typedef IDirectMusicAudioPath IDirectMusicAudioPath8;
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicAudioPath_QueryInterface(p,a,b)  ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicAudioPath_AddRef(p)              ICOM_CALL (AddRef,p)
#define IDirectMusicAudioPath_Release(p)             ICOM_CALL (Release,p)
    /*** IDirectMusicAudioPath methods ***/
#define IDirectMusicAudioPath_GetObjectInPath(p,a,b,c,d,e,f,g) ICOM_CALL7(GetObjectInPath,p,a,b,c,d,e,f,g)
#define IDirectMusicAudioPath_Activate(p,a)          ICOM_CALL1(Activate,p,a)
#define IDirectMusicAudioPath_SetVolume(p,a,b)       ICOM_CALL2(SetVolume,p,a,b)
#define IDirectMusicAudioPath_ConvertPChannel(p,a,b) ICOM_CALL2(ConvertPChannel,p,a,b)


#define ICOM_INTERFACE IDirectMusicChordMap
#define IDirectMusicChordMap_METHODS \
	ICOM_METHOD1(HRESULT,GetScale, DWORD*,pdwScale )

#define IDirectMusicChordMap_IMETHODS \
          IUnknown_IMETHODS \
	  IDirectMusicChordMap_METHODS

ICOM_DEFINE(IDirectMusicChordMap,IUnknown)
typedef IDirectMusicChordMap IDirectMusicChordMap8;
#undef ICOM_INTERFACE


/*****************************************************************************
 * IDirectMusicSegment interface
 */
#define ICOM_INTERFACE IDirectMusicSegment 
#define IDirectMusicSegment_METHODS \
    ICOM_METHOD1(HRESULT,GetLength, MUSIC_TIME*,pmtLength) \
    ICOM_METHOD1(HRESULT,SetLength, MUSIC_TIME,mtLength) \
    ICOM_METHOD1(HRESULT,GetRepeats, DWORD*,pdwRepeats) \
    ICOM_METHOD1(HRESULT,SetRepeats, DWORD,dwRepeats) \
    ICOM_METHOD1(HRESULT,GetDefaultResolution, DWORD*,pdwResolution) \
    ICOM_METHOD1(HRESULT,SetDefaultResolution, DWORD,dwResolution) \
    ICOM_METHOD4(HRESULT,GetTrack, REFGUID,rguidType, DWORD,dwGroupBits, DWORD,dwIndex, IDirectMusicTrack**,ppTrack) \
    ICOM_METHOD2(HRESULT,GetTrackGroup, IDirectMusicTrack*,pTrack, DWORD*,pdwGroupBits) \
    ICOM_METHOD2(HRESULT,InsertTrack, IDirectMusicTrack*,pTrack, DWORD,dwGroupBits) \
    ICOM_METHOD1(HRESULT,RemoveTrack, IDirectMusicTrack*,pTrack) \
    ICOM_METHOD3(HRESULT,InitPlay, IDirectMusicSegmentState**,ppSegState, IDirectMusicPerformance*,pPerformance, DWORD,dwFlags) \
    ICOM_METHOD1(HRESULT,GetGraph, IDirectMusicGraph**,ppGraph) \
    ICOM_METHOD1(HRESULT,SetGraph, IDirectMusicGraph*,pGraph) \
    ICOM_METHOD1(HRESULT,AddNotificationType, REFGUID,rguidNotificationType) \
    ICOM_METHOD1(HRESULT,RemoveNotificationType, REFGUID,rguidNotificationType) \
    ICOM_METHOD6(HRESULT,GetParam, REFGUID,rguidType, DWORD,dwGroupBits, DWORD,dwIndex, MUSIC_TIME,mtTime, MUSIC_TIME*,pmtNext, void*,pParam) \
    ICOM_METHOD5(HRESULT,SetParam, REFGUID,rguidType, DWORD,dwGroupBits, DWORD,dwIndex, MUSIC_TIME,mtTime, void*,pParam) \
    ICOM_METHOD3(HRESULT,Clone, MUSIC_TIME,mtStart, MUSIC_TIME,mtEnd, IDirectMusicSegment**,ppSegment) \
    ICOM_METHOD1(HRESULT,SetStartPoint, MUSIC_TIME,mtStart) \
    ICOM_METHOD1(HRESULT,GetStartPoint, MUSIC_TIME*,pmtStart) \
    ICOM_METHOD2(HRESULT,SetLoopPoints, MUSIC_TIME,mtStart, MUSIC_TIME,mtEnd) \
    ICOM_METHOD2(HRESULT,GetLoopPoints, MUSIC_TIME*,pmtStart, MUSIC_TIME*,pmtEnd) \
    ICOM_METHOD2(HRESULT,SetPChannelsUsed, DWORD,dwNumPChannels, DWORD*,paPChannels)
#define IDirectMusicSegment_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicSegment_METHODS
ICOM_DEFINE(IDirectMusicSegment,IUnknown)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicSegment_QueryInterface(p,a,b)       ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicSegment_AddRef(p)                   ICOM_CALL (AddRef,p)
#define IDirectMusicSegment_Release(p)                  ICOM_CALL (Release,p)
    /*** IDirectMusicSegment methods ***/
#define IDirectMusicSegment_GetLength(p,a)              ICOM_CALL1(GetLength,p,a)
#define IDirectMusicSegment_SetLength(p,a)              ICOM_CALL1(SetLength,p,a)
#define IDirectMusicSegment_GetRepeats(p,a)             ICOM_CALL1(GetRepeats,p,a)
#define IDirectMusicSegment_SetRepeats(p,a)             ICOM_CALL1(SetRepeats,p,a)
#define IDirectMusicSegment_GetDefaultResolution(p,a)   ICOM_CALL1(GetDefaultResolution,p,a)
#define IDirectMusicSegment_SetDefaultResolution(p,a)   ICOM_CALL1(SetDefaultResolution,p,a)
#define IDirectMusicSegment_GetTrack(p,a,b,c,d)         ICOM_CALL4(GetTrack,p,a,b,c,d)
#define IDirectMusicSegment_GetTrackGroup(p,a,b)        ICOM_CALL2(GetTrackGroup,p,a,b)
#define IDirectMusicSegment_InsertTrack(p,a,b)          ICOM_CALL2(InsertTrack,p,a,b)
#define IDirectMusicSegment_RemoveTrack(p,a)            ICOM_CALL1(RemoveTrack,p,a)
#define IDirectMusicSegment_InitPlay(p,a,b,c)           ICOM_CALL3(InitPlay,p,a,b,c)
#define IDirectMusicSegment_GetGraph(p,a)               ICOM_CALL1(GetGraph,p,a)
#define IDirectMusicSegment_SetGraph(p,a)               ICOM_CALL1(SetGraph,p,a)
#define IDirectMusicSegment_AddNotificationType(p,a)    ICOM_CALL1(AddNotificationType,p,a)
#define IDirectMusicSegment_RemoveNotificationType(p,a) ICOM_CALL1(RemoveNotificationType,p,a)
#define IDirectMusicSegment_GetParam(p,a,b,c,d,e,f)     ICOM_CALL6(GetParam,p,a,b,c,d,e,f)
#define IDirectMusicSegment_SetParam(p,a,b,c,d,e)       ICOM_CALL5(SetParam,p,a,b,c,d,e)
#define IDirectMusicSegment_Clone(p,a,b,c)              ICOM_CALL3(Clone,p,a,b,c)
#define IDirectMusicSegment_SetStartPoint(p,a)          ICOM_CALL1(SetStartPoint,p,a)
#define IDirectMusicSegment_GetStartPoint(p,a)          ICOM_CALL1(GetStartPoint,p,a)
#define IDirectMusicSegment_SetLoopPoints(p,a,b)        ICOM_CALL2(SetLoopPoints,p,a,b)
#define IDirectMusicSegment_GetLoopPoints(p,a,b)        ICOM_CALL2(GetLoopPoints,p,a,b)
#define IDirectMusicSegment_SetPChannelsUsed(p,a,b)     ICOM_CALL2(SetPChannelsUsed,p,a,b)

/*****************************************************************************
 * IDirectMusicSegment8 interface
 */
#define ICOM_INTERFACE IDirectMusicSegment8
#define IDirectMusicSegment8_METHODS \
    ICOM_METHOD5(HRESULT,SetTrackConfig, REFGUID,rguidTrackClassID, DWORD,dwGroupBits, DWORD,dwIndex, DWORD,dwFlagsOn, DWORD,dwFlagsOff) \
    ICOM_METHOD1(HRESULT,GetAudioPathConfig, IUnknown**,ppAudioPathConfig) \
    ICOM_METHOD4(HRESULT,Compose, MUSIC_TIME,mtTime, IDirectMusicSegment*,pFromSegment, IDirectMusicSegment*,pToSegment, \
		                  IDirectMusicSegment**,ppComposedSegment) \
    ICOM_METHOD1(HRESULT,Download, IUnknown*,pAudioPath) \
    ICOM_METHOD1(HRESULT,Unload, IUnknown*,pAudioPath)
#define IDirectMusicSegment8_IMETHODS \
  IDirectMusicSegment_IMETHODS \
  IDirectMusicSegment8_METHODS
ICOM_DEFINE(IDirectMusicSegment8,IDirectMusicSegment)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicSegment8_QueryInterface(p,a,b)       ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicSegment8_AddRef(p)                   ICOM_CALL (AddRef,p)
#define IDirectMusicSegment8_Release(p)                  ICOM_CALL (Release,p)
    /*** IDirectMusicSegment8 methods ***/
#define IDirectMusicSegment8_GetLength(p,a)              ICOM_CALL1(GetLength,p,a)
#define IDirectMusicSegment8_SetLength(p,a)              ICOM_CALL1(SetLength,p,a)
#define IDirectMusicSegment8_GetRepeats(p,a)             ICOM_CALL1(GetRepeats,p,a)
#define IDirectMusicSegment8_SetRepeats(p,a)             ICOM_CALL1(SetRepeats,p,a)
#define IDirectMusicSegment8_GetDefaultResolution(p,a)   ICOM_CALL1(GetDefaultResolution,p,a)
#define IDirectMusicSegment8_SetDefaultResolution(p,a)   ICOM_CALL1(SetDefaultResolution,p,a)
#define IDirectMusicSegment8_GetTrack(p,a,b,c,d)         ICOM_CALL4(GetTrack,p,a,b,c,d)
#define IDirectMusicSegment8_GetTrackGroup(p,a,b)        ICOM_CALL2(GetTrackGroup,p,a,b)
#define IDirectMusicSegment8_InsertTrack(p,a,b)          ICOM_CALL2(InsertTrack,p,a,b)
#define IDirectMusicSegment8_RemoveTrack(p,a)            ICOM_CALL1(RemoveTrack,p,a)
#define IDirectMusicSegment8_InitPlay(p,a,b,c)           ICOM_CALL3(InitPlay,p,a,b,c)
#define IDirectMusicSegment8_GetGraph(p,a)               ICOM_CALL1(GetGraph,p,a)
#define IDirectMusicSegment8_SetGraph(p,a)               ICOM_CALL1(SetGraph,p,a)
#define IDirectMusicSegment8_AddNotificationType(p,a)    ICOM_CALL1(AddNotificationType,p,a)
#define IDirectMusicSegment8_RemoveNotificationType(p,a) ICOM_CALL1(RemoveNotificationType,p,a)
#define IDirectMusicSegment8_GetParam(p,a,b,c,d,e,f)     ICOM_CALL6(GetParam,p,a,b,c,d,e,f)
#define IDirectMusicSegment8_SetParam(p,a,b,c,d,e)       ICOM_CALL5(SetParam,p,a,b,c,d,e)
#define IDirectMusicSegment8_Clone(p,a,b,c)              ICOM_CALL3(Clone,p,a,b,c)
#define IDirectMusicSegment8_SetStartPoint(p,a)          ICOM_CALL1(SetStartPoint,p,a)
#define IDirectMusicSegment8_GetStartPoint(p,a)          ICOM_CALL1(GetStartPoint,p,a)
#define IDirectMusicSegment8_SetLoopPoints(p,a,b)        ICOM_CALL2(SetLoopPoints,p,a,b)
#define IDirectMusicSegment8_GetLoopPoints(p,a,b)        ICOM_CALL2(GetLoopPoints,p,a,b)
#define IDirectMusicSegment8_SetPChannelsUsed(p,a,b)     ICOM_CALL2(SetPChannelsUsed,p,a,b)
    /*** IDirectMusicSegment8 methods ***/
#define IDirectMusicSegment8_SetTrackConfig(p,a,b,c,d,e) ICOM_CALL5(SetTrackConfig,p,a,b,c,d,e)
#define IDirectMusicSegment8_GetAudioPathConfig(p,a)     ICOM_CALL1(GetAudioPathConfig,p,a)
#define IDirectMusicSegment8_Compose(p,a,b,c,d)          ICOM_CALL4(Compose,p,a,b,c,d)
#define IDirectMusicSegment8_Download(p,a)               ICOM_CALL1(Download,p,a)
#define IDirectMusicSegment8_Unload(p,a)                 ICOM_CALL1(Unload,p,a)

/*****************************************************************************
 * IDirectMusicSegmentState interface
 */
#define ICOM_INTERFACE IDirectMusicSegmentState
#define IDirectMusicSegmentState_METHODS \
    ICOM_METHOD1(HRESULT,GetRepeats,   DWORD*,pdwRepeats) \
    ICOM_METHOD1(HRESULT,GetSegment,   IDirectMusicSegment**,ppSegment) \
    ICOM_METHOD1(HRESULT,GetStartTime, MUSIC_TIME*,pmtStart) \
    ICOM_METHOD1(HRESULT,GetSeek,      MUSIC_TIME*,pmtSeek) \
    ICOM_METHOD1(HRESULT,GetStartPoint,MUSIC_TIME*,pmtStart)
#define IDirectMusicSegmentState_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicSegmentState_METHODS
ICOM_DEFINE(IDirectMusicSegmentState,IUnknown)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicSegmentState_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicSegmentState_AddRef(p)             ICOM_CALL (AddRef,p)
#define IDirectMusicSegmentState_Release(p)            ICOM_CALL (Release,p)
    /*** IDirectMusicSegmentState methods ***/
#define IDirectMusicSegmentState_GetRepeats(p,a)       ICOM_CALL1(GetRepeats,p,a)
#define IDirectMusicSegmentState_GetSegment(p,a)       ICOM_CALL1(GetSegment,p,a)
#define IDirectMusicSegmentState_GetStartTime(p,a)     ICOM_CALL1(GetStartTime,p,a)
#define IDirectMusicSegmentState_GetSeek(p,a)          ICOM_CALL1(GetSeek,p,a)
#define IDirectMusicSegmentState_GetStartPoint(p,a)    ICOM_CALL1(GetStartPoint,p,a)

/*****************************************************************************
 * IDirectMusicSegmentState8 interface
 */
#define ICOM_INTERFACE IDirectMusicSegmentState8
#define IDirectMusicSegmentState8_METHODS \
    ICOM_METHOD5(HRESULT,SetTrackConfig, REFGUID,rguidTrackClassID,DWORD,dwGroupBits,DWORD,dwIndex,DWORD,dwFlagsOn,DWORD,dwFlagsOff) \
    ICOM_METHOD7(HRESULT,GetObjectInPath,DWORD,dwPChannel,DWORD,dwStage,DWORD,dwBuffer,REFGUID,guidObject,DWORD,dwIndex,REFGUID,iidInterface,void**,ppObj)
#define IDirectMusicSegmentState8_IMETHODS \
    IDirectMusicSegmentState_IMETHODS \
    IDirectMusicSegmentState8_METHODS
ICOM_DEFINE(IDirectMusicSegmentState8,IDirectMusicSegmentState)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicSegmentState8_QueryInterface(p,a,b)            ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicSegmentState8_AddRef(p)                        ICOM_CALL (AddRef,p)
#define IDirectMusicSegmentState8_Release(p)                       ICOM_CALL (Release,p)
    /*** IDirectMusicSegmentState methods ***/
#define IDirectMusicSegmentState8_GetRepeats(p,a)                  ICOM_CALL1(GetRepeats,p,a)
#define IDirectMusicSegmentState8_GetSegment(p,a)                  ICOM_CALL1(GetSegment,p,a)
#define IDirectMusicSegmentState8_GetStartTime(p,a)                ICOM_CALL1(GetStartTime,p,a)
#define IDirectMusicSegmentState8_GetSeek(p,a)                     ICOM_CALL1(GetSeek,p,a)
#define IDirectMusicSegmentState8_GetStartPoint(p,a)               ICOM_CALL1(GetStartPoint,p,a)
    /*** IDirectMusicSegmentState8 methods ***/
#define IDirectMusicSegmentState8_SetTrackConfig(p,a,b,c,d,e)      ICOM_CALL5(SetTrackConfig,p,a,b,c,d,e)
#define IDirectMusicSegmentState8_GetObjectInPath(p,a,b,c,d,e,f,g) ICOM_CALL7(GetObjectInPath,p,a,b,c,d,e,f,g)


#define ICOM_INTERFACE IDirectMusicStyle
#define IDirectMusicStyle_METHODS \
    ICOM_METHOD2(HRESULT,GetBand, WCHAR*,pwszName, IDirectMusicBand**,ppBand) \
    ICOM_METHOD2(HRESULT,EnumBand, DWORD,dwIndex, WCHAR*,pwszName) \
    ICOM_METHOD1(HRESULT,GetDefaultBand, IDirectMusicBand**,ppBand) \
    ICOM_METHOD2(HRESULT,EnumMotif, DWORD,dwIndex, WCHAR*,pwszName) \
    ICOM_METHOD2(HRESULT,GetMotif, WCHAR*,pwszName, IDirectMusicSegment**,ppSegment) \
    ICOM_METHOD1(HRESULT,GetDefaultChordMap, IDirectMusicChordMap**,ppChordMap) \
    ICOM_METHOD2(HRESULT,EnumChordMap, DWORD,dwIndex, WCHAR*,pwszName) \
    ICOM_METHOD2(HRESULT,GetChordMap, WCHAR*,pwszName, IDirectMusicChordMap**,ppChordMap) \
    ICOM_METHOD1(HRESULT,GetTimeSignature, DMUS_TIMESIGNATURE*,pTimeSig) \
    ICOM_METHOD4(HRESULT,GetEmbellishmentLength, DWORD,dwType, DWORD,dwLevel, DWORD*,pdwMin, DWORD*,pdwMax) \
    ICOM_METHOD1(HRESULT,GetTempo, double*,pTempo)

#define IDirectMusicStyle_IMETHODS \
  IUnknown_IMETHODS \
  IDirectMusicStyle_METHODS
			        
ICOM_DEFINE(IDirectMusicStyle,IUnknown)
#undef ICOM_INTERFACE

#define ICOM_INTERFACE IDirectMusicStyle8
#define IDirectMusicStyle8_METHODS \
    ICOM_METHOD3(HRESULT,EnumPattern, DWORD,dwIndex, DWORD,dwPatternType, WCHAR*,pwszName)
	    
#define IDirectMusicStyle8_IMETHODS \
  IDirectMusicStyle_IMETHODS \
  IDirectMusicStyle8_METHODS

ICOM_DEFINE(IDirectMusicStyle8,IDirectMusicStyle)
#undef ICOM_INTERFACE


/*****************************************************************************
 * IDirectMusicPerformance interface
 */
#define ICOM_INTERFACE IDirectMusicPerformance
#define IDirectMusicPerformance_METHODS \
    ICOM_METHOD3(HRESULT,Init, IDirectMusic**,ppDirectMusic, LPDIRECTSOUND,pDirectSound, HWND,hWnd) \
    ICOM_METHOD4(HRESULT,PlaySegment, IDirectMusicSegment*,pSegment, DWORD,dwFlags, __int64,i64StartTime, IDirectMusicSegmentState**,ppSegmentState) \
    ICOM_METHOD4(HRESULT,Stop, IDirectMusicSegment*,pSegment, IDirectMusicSegmentState*,pSegmentState, MUSIC_TIME,mtTime, DWORD,dwFlags) \
    ICOM_METHOD2(HRESULT,GetSegmentState, IDirectMusicSegmentState**,ppSegmentState, MUSIC_TIME,mtTime) \
    ICOM_METHOD1(HRESULT,SetPrepareTime, DWORD,dwMilliSeconds) \
    ICOM_METHOD1(HRESULT,GetPrepareTime, DWORD*,pdwMilliSeconds) \
    ICOM_METHOD1(HRESULT,SetBumperLength, DWORD,dwMilliSeconds) \
    ICOM_METHOD1(HRESULT,GetBumperLength, DWORD*,pdwMilliSeconds) \
    ICOM_METHOD1(HRESULT,SendPMsg, DMUS_PMSG*,pPMSG) \
    ICOM_METHOD2(HRESULT,MusicToReferenceTime, MUSIC_TIME,mtTime, REFERENCE_TIME*,prtTime) \
    ICOM_METHOD2(HRESULT,ReferenceToMusicTime, REFERENCE_TIME,rtTime, MUSIC_TIME*,pmtTime) \
    ICOM_METHOD2(HRESULT,IsPlaying, IDirectMusicSegment*,pSegment, IDirectMusicSegmentState*,pSegState) \
    ICOM_METHOD2(HRESULT,GetTime, REFERENCE_TIME*,prtNow, MUSIC_TIME*,pmtNow) \
    ICOM_METHOD2(HRESULT,AllocPMsg, ULONG,cb, DMUS_PMSG**,ppPMSG) \
    ICOM_METHOD1(HRESULT,FreePMsg, DMUS_PMSG*,pPMSG) \
    ICOM_METHOD1(HRESULT,GetGraph, IDirectMusicGraph**,ppGraph) \
    ICOM_METHOD1(HRESULT,SetGraph, IDirectMusicGraph*,pGraph) \
    ICOM_METHOD2(HRESULT,SetNotificationHandle, HANDLE,hNotification, REFERENCE_TIME,rtMinimum) \
    ICOM_METHOD1(HRESULT,GetNotificationPMsg, DMUS_NOTIFICATION_PMSG**,ppNotificationPMsg) \
    ICOM_METHOD1(HRESULT,AddNotificationType, REFGUID,rguidNotificationType) \
    ICOM_METHOD1(HRESULT,RemoveNotificationType, REFGUID,rguidNotificationType) \
    ICOM_METHOD1(HRESULT,AddPort, IDirectMusicPort*,pPort) \
    ICOM_METHOD1(HRESULT,RemovePort, IDirectMusicPort*,pPort ) \
    ICOM_METHOD3(HRESULT,AssignPChannelBlock, DWORD,dwBlockNum, IDirectMusicPort*,pPort, DWORD,dwGroup ) \
    ICOM_METHOD4(HRESULT,AssignPChannel, DWORD,dwPChannel, IDirectMusicPort*,pPort, DWORD,dwGroup, DWORD,dwMChannel ) \
    ICOM_METHOD4(HRESULT,PChannelInfo, DWORD,dwPChannel, IDirectMusicPort**,ppPort, DWORD*,pdwGroup, DWORD*,pdwMChannel ) \
    ICOM_METHOD8(HRESULT,DownloadInstrument, IDirectMusicInstrument*,pInst, DWORD,dwPChannel, \
		                             IDirectMusicDownloadedInstrument**,ppDownInst, DMUS_NOTERANGE*,pNoteRanges, \
					     DWORD,dwNumNoteRanges, IDirectMusicPort**,ppPort, DWORD*,pdwGroup, DWORD*,pdwMChannel ) \
    ICOM_METHOD2(HRESULT,Invalidate, MUSIC_TIME,mtTime, DWORD,dwFlags) \
    ICOM_METHOD6(HRESULT,GetParam, REFGUID,rguidType, DWORD,dwGroupBits, DWORD,dwIndex, MUSIC_TIME,mtTime, MUSIC_TIME*,pmtNext, void*,pParam) \
    ICOM_METHOD5(HRESULT,SetParam, REFGUID,rguidType, DWORD,dwGroupBits, DWORD,dwIndex, MUSIC_TIME,mtTime, void*,pParam) \
    ICOM_METHOD3(HRESULT,GetGlobalParam, REFGUID,rguidType, void*,pParam, DWORD,dwSize) \
    ICOM_METHOD3(HRESULT,SetGlobalParam, REFGUID,rguidType, void*,pParam, DWORD,dwSize) \
    ICOM_METHOD1(HRESULT,GetLatencyTime, REFERENCE_TIME*,prtTime) \
    ICOM_METHOD1(HRESULT,GetQueueTime, REFERENCE_TIME*,prtTime) \
    ICOM_METHOD1(HRESULT,AdjustTime, REFERENCE_TIME,rtAmount) \
    ICOM_METHOD(HRESULT,CloseDown) \
    ICOM_METHOD3(HRESULT,GetResolvedTime, REFERENCE_TIME,rtTime, REFERENCE_TIME*,prtResolved, DWORD,dwTimeResolveFlags) \
    ICOM_METHOD5(HRESULT,MIDIToMusic, BYTE,bMIDIValue, DMUS_CHORD_KEY*,pChord, BYTE,bPlayMode, BYTE,bChordLevel, WORD*,pwMusicValue) \
    ICOM_METHOD5(HRESULT,MusicToMIDI, WORD,wMusicValue, DMUS_CHORD_KEY*,pChord, BYTE,bPlayMode, BYTE,bChordLevel, BYTE*,pbMIDIValue) \
    ICOM_METHOD6(HRESULT,TimeToRhythm, MUSIC_TIME,mtTime, DMUS_TIMESIGNATURE*,pTimeSig, WORD*,pwMeasure, BYTE*,pbBeat, BYTE*,pbGrid, short*,pnOffset) \
    ICOM_METHOD6(HRESULT,RhythmToTime, WORD,wMeasure, BYTE,bBeat, BYTE,bGrid, short,nOffset, DMUS_TIMESIGNATURE*,pTimeSig, MUSIC_TIME*,pmtTime)
#define IDirectMusicPerformance_IMETHODS \
    IUnknown_IMETHODS \
    IDirectMusicPerformance_METHODS
ICOM_DEFINE(IDirectMusicPerformance,IUnknown)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicPerformance_QueryInterface(p,a,b)        ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicPerformance_AddRef(p)                    ICOM_CALL (AddRef,p)
#define IDirectMusicPerformance_Release(p)                   ICOM_CALL (Release,p)
    /*** IDirectMusicPerformance methods */
#define IDirectMusicPerformance_Init(p,a,b,c)                ICOM_CALL3(Init,p,a,b,c)
#define IDirectMusicPerformance_PlaySegment(p,a,b,c,d)       ICOM_CALL4(PlaySegment,p,a,b,c,d)
#define IDirectMusicPerformance_Stop(p,a,b,c,d)              ICOM_CALL4(Stop,p,a,b,c,d)
#define IDirectMusicPerformance_GetSegmentState(p,a,b)       ICOM_CALL2(GetSegmentState,p,a,b)
#define IDirectMusicPerformance_SetPrepareTime(p,a)          ICOM_CALL1(SetPrepareTime,p,a)
#define IDirectMusicPerformance_GetPrepareTime(p,a)          ICOM_CALL1(GetPrepareTime,p,a)
#define IDirectMusicPerformance_SetBumperLength(p,a)         ICOM_CALL1(SetBumperLength,p,a)
#define IDirectMusicPerformance_GetBumperLength(p,a)         ICOM_CALL1(GetBumperLength,p,a)
#define IDirectMusicPerformance_SendPMsg(p,a)                ICOM_CALL1(SendPMsg,p,a)
#define IDirectMusicPerformance_MusicToReferenceTime(p,a,b)  ICOM_CALL2(MusicToReferenceTime,p,a,b)
#define IDirectMusicPerformance_ReferenceToMusicTime(p,a,b)  ICOM_CALL2(ReferenceToMusicTime,p,a,b)
#define IDirectMusicPerformance_IsPlaying(p,a,b)             ICOM_CALL2(IsPlaying,p,a,b)
#define IDirectMusicPerformance_GetTime(p,a,b)               ICOM_CALL2(GetTime,p,a,b)
#define IDirectMusicPerformance_AllocPMsg(p,a,b)             ICOM_CALL2(AllocPMsg,p,a,b)
#define IDirectMusicPerformance_FreePMsg(p,a)                ICOM_CALL1(FreePMsg,p,a)
#define IDirectMusicPerformance_GetGraph(p,a)                ICOM_CALL1(GetGraph,p,a)
#define IDirectMusicPerformance_SetGraph(p,a)                ICOM_CALL1(SetGraph,p,a)
#define IDirectMusicPerformance_SetNotificationHandle(p,a,b) ICOM_CALL2(SetNotificationHandle,p,a,b)
#define IDirectMusicPerformance_GetNotificationPMsg(p,a)     ICOM_CALL1(GetNotificationPMsg,p,a)
#define IDirectMusicPerformance_AddNotificationType(p,a)     ICOM_CALL1(AddNotificationType,p,a)
#define IDirectMusicPerformance_RemoveNotificationType(p,a)  ICOM_CALL1(RemoveNotificationType,p,a)
#define IDirectMusicPerformance_AddPort(p,a)                 ICOM_CALL1(AddPort,p,a)
#define IDirectMusicPerformance_RemovePort(p,a)              ICOM_CALL1(RemovePort,p,a)
#define IDirectMusicPerformance_AssignPChannelBlock(p,a,b,c) ICOM_CALL3(AssignPChannelBlock,p,a,b,c)
#define IDirectMusicPerformance_AssignPChannel(p,a,b,c,d)    ICOM_CALL4(AssignPChannel,p,a,b,c,d)
#define IDirectMusicPerformance_PChannelInfo(p,a,b,c,d)      ICOM_CALL4(PChannelInfo,p,a,b,c,d)
#define IDirectMusicPerformance_DownloadInstrument(p,a,b,c,d,e,f,g,h) ICOM_CALL8(DownloadInstrument,p,a,b,c,d,e,f,g,h)
#define IDirectMusicPerformance_Invalidate(p,a,b)            ICOM_CALL2(Invalidate,p,a,b)
#define IDirectMusicPerformance_GetParam(p,a,b,c,d,e,f)      ICOM_CALL6(GetParam,p,a,b,c,d,e,f)
#define IDirectMusicPerformance_SetParam(p,a,b,c,d,e)        ICOM_CALL5(SetParam,p,a,b,c,d,e)
#define IDirectMusicPerformance_GetGlobalParam(p,a,b,c)      ICOM_CALL3(GetGlobalParam,p,a,b,c)
#define IDirectMusicPerformance_SetGlobalParam(p,a,b,c)      ICOM_CALL3(SetGlobalParam,p,a,b,c)
#define IDirectMusicPerformance_GetLatencyTime(p,a)          ICOM_CALL1(GetLatencyTime,p,a)
#define IDirectMusicPerformance_GetQueueTime(p,a)            ICOM_CALL1(GetQueueTime,p,a)
#define IDirectMusicPerformance_AdjustTime(p,a)              ICOM_CALL1(AdjustTime,p,a)
#define IDirectMusicPerformance_CloseDown(p)                 ICOM_CALL (CloseDown,p)
#define IDirectMusicPerformance_GetResolvedTime(p,a,b,c)     ICOM_CALL3(GetResolvedTime,p,a,b,c)
#define IDirectMusicPerformance_MIDIToMusic(p,a,b,c,d,e)     ICOM_CALL5(MIDIToMusic,p,a,b,c,d,e)
#define IDirectMusicPerformance_MusicToMIDI(p,a,b,c,d,e)     ICOM_CALL5(MusicToMIDI,p,a,b,c,d,e)
#define IDirectMusicPerformance_TimeToRhythm(p,a,b,c,d,e,f)  ICOM_CALL6(TimeToRhythm,p,a,b,c,d,e,f)
#define IDirectMusicPerformance_RhythmToTime(p,a,b,c,d,e,f)  ICOM_CALL6(RhythmToTime,p,a,b,c,d,e,f)

/*****************************************************************************
 * IDirectMusicPerformance8 interface
 */
#define ICOM_INTERFACE IDirectMusicPerformance8
#define IDirectMusicPerformance8_METHODS \
    ICOM_METHOD7(HRESULT,InitAudio, IDirectMusic**,ppDirectMusic, IDirectSound**,ppDirectSound, HWND,hWnd, \
		                    DWORD,dwDefaultPathType, DWORD,dwPChannelCount, DWORD,dwFlags, DMUS_AUDIOPARAMS*,pParams) \
    ICOM_METHOD8(HRESULT,PlaySegmentEx, IUnknown*,pSource, WCHAR*,pwzSegmentName, IUnknown*,pTransition, DWORD,dwFlags, \
		                        __int64,i64StartTime, IDirectMusicSegmentState**,ppSegmentState, IUnknown*,pFrom, IUnknown*,pAudioPath) \
    ICOM_METHOD3(HRESULT,StopEx, IUnknown*,pObjectToStop, __int64,i64StopTime, DWORD,dwFlags) \
    ICOM_METHOD2(HRESULT,ClonePMsg, DMUS_PMSG*,pSourcePMSG, DMUS_PMSG**,ppCopyPMSG) \
    ICOM_METHOD3(HRESULT,CreateAudioPath, IUnknown*,pSourceConfig, BOOL,fActivate, IDirectMusicAudioPath**,ppNewPath) \
    ICOM_METHOD4(HRESULT,CreateStandardAudioPath, DWORD,dwType, DWORD,dwPChannelCount, BOOL,fActivate, IDirectMusicAudioPath**,ppNewPath) \
    ICOM_METHOD1(HRESULT,SetDefaultAudioPath, IDirectMusicAudioPath*,pAudioPath) \
    ICOM_METHOD1(HRESULT,GetDefaultAudioPath, IDirectMusicAudioPath**,ppAudioPath) \
    ICOM_METHOD7(HRESULT,GetParamEx, REFGUID,rguidType, DWORD,dwTrackID, DWORD,dwGroupBits, DWORD,dwIndex, MUSIC_TIME,mtTime,\
		                     MUSIC_TIME*,pmtNext, void*,pParam)
#define IDirectMusicPerformance8_IMETHODS \
    IDirectMusicPerformance_IMETHODS \
    IDirectMusicPerformance8_METHODS
ICOM_DEFINE(IDirectMusicPerformance8,IDirectMusicPerformance)
#undef ICOM_INTERFACE

    /*** IUnknown methods ***/
#define IDirectMusicPerformance8_QueryInterface(p,a,b)        ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectMusicPerformance8_AddRef(p)                    ICOM_CALL (AddRef,p)
#define IDirectMusicPerformance8_Release(p)                   ICOM_CALL (Release,p)
    /*** IDirectMusicPerformance methods */
#define IDirectMusicPerformance8_Init(p,a,b,c)                ICOM_CALL3(Init,p,a,b,c)
#define IDirectMusicPerformance8_PlaySegment(p,a,b,c,d)       ICOM_CALL4(PlaySegment,p,a,b,c,d)
#define IDirectMusicPerformance8_Stop(p,a,b,c,d)              ICOM_CALL4(Stop,p,a,b,c,d)
#define IDirectMusicPerformance8_GetSegmentState(p,a,b)       ICOM_CALL2(GetSegmentState,p,a,b)
#define IDirectMusicPerformance8_SetPrepareTime(p,a)          ICOM_CALL1(SetPrepareTime,p,a)
#define IDirectMusicPerformance8_GetPrepareTime(p,a)          ICOM_CALL1(GetPrepareTime,p,a)
#define IDirectMusicPerformance8_SetBumperLength(p,a)         ICOM_CALL1(SetBumperLength,p,a)
#define IDirectMusicPerformance8_GetBumperLength(p,a)         ICOM_CALL1(GetBumperLength,p,a)
#define IDirectMusicPerformance8_SendPMsg(p,a)                ICOM_CALL1(SendPMsg,p,a)
#define IDirectMusicPerformance8_MusicToReferenceTime(p,a,b)  ICOM_CALL2(MusicToReferenceTime,p,a,b)
#define IDirectMusicPerformance8_ReferenceToMusicTime(p,a,b)  ICOM_CALL2(ReferenceToMusicTime,p,a,b)
#define IDirectMusicPerformance8_IsPlaying(p,a,b)             ICOM_CALL2(IsPlaying,p,a,b)
#define IDirectMusicPerformance8_GetTime(p,a,b)               ICOM_CALL2(GetTime,p,a,b)
#define IDirectMusicPerformance8_AllocPMsg(p,a,b)             ICOM_CALL2(AllocPMsg,p,a,b)
#define IDirectMusicPerformance8_FreePMsg(p,a)                ICOM_CALL1(FreePMsg,p,a)
#define IDirectMusicPerformance8_GetGraph(p,a)                ICOM_CALL1(GetGraph,p,a)
#define IDirectMusicPerformance8_SetGraph(p,a)                ICOM_CALL1(SetGraph,p,a)
#define IDirectMusicPerformance8_SetNotificationHandle(p,a,b) ICOM_CALL2(SetNotificationHandle,p,a,b)
#define IDirectMusicPerformance8_GetNotificationPMsg(p,a)     ICOM_CALL1(GetNotificationPMsg,p,a)
#define IDirectMusicPerformance8_AddNotificationType(p,a)     ICOM_CALL1(AddNotificationType,p,a)
#define IDirectMusicPerformance8_RemoveNotificationType(p,a)  ICOM_CALL1(RemoveNotificationType,p,a)
#define IDirectMusicPerformance8_AddPort(p,a)                 ICOM_CALL1(AddPort,p,a)
#define IDirectMusicPerformance8_RemovePort(p,a)              ICOM_CALL1(RemovePort,p,a)
#define IDirectMusicPerformance8_AssignPChannelBlock(p,a,b,c) ICOM_CALL3(AssignPChannelBlock,p,a,b,c)
#define IDirectMusicPerformance8_AssignPChannel(p,a,b,c,d)    ICOM_CALL4(AssignPChannel,p,a,b,c,d)
#define IDirectMusicPerformance8_PChannelInfo(p,a,b,c,d)      ICOM_CALL4(PChannelInfo,p,a,b,c,d)
#define IDirectMusicPerformance8_DownloadInstrument(p,a,b,c,d,e,f,g,h) ICOM_CALL8(DownloadInstrument,p,a,b,c,d,e,f,g,h)
#define IDirectMusicPerformance8_Invalidate(p,a,b)            ICOM_CALL2(Invalidate,p,a,b)
#define IDirectMusicPerformance8_GetParam(p,a,b,c,d,e,f)      ICOM_CALL6(GetParam,p,a,b,c,d,e,f)
#define IDirectMusicPerformance8_SetParam(p,a,b,c,d,e)        ICOM_CALL5(SetParam,p,a,b,c,d,e)
#define IDirectMusicPerformance8_GetGlobalParam(p,a,b,c)      ICOM_CALL3(GetGlobalParam,p,a,b,c)
#define IDirectMusicPerformance8_SetGlobalParam(p,a,b,c)      ICOM_CALL3(SetGlobalParam,p,a,b,c)
#define IDirectMusicPerformance8_GetLatencyTime(p,a)          ICOM_CALL1(GetLatencyTime,p,a)
#define IDirectMusicPerformance8_GetQueueTime(p,a)            ICOM_CALL1(GetQueueTime,p,a)
#define IDirectMusicPerformance8_AdjustTime(p,a)              ICOM_CALL1(AdjustTime,p,a)
#define IDirectMusicPerformance8_CloseDown(p)                 ICOM_CALL (CloseDown,p)
#define IDirectMusicPerformance8_GetResolvedTime(p,a,b,c)     ICOM_CALL3(GetResolvedTime,p,a,b,c)
#define IDirectMusicPerformance8_MIDIToMusic(p,a,b,c,d,e)     ICOM_CALL5(MIDIToMusic,p,a,b,c,d,e)
#define IDirectMusicPerformance8_MusicToMIDI(p,a,b,c,d,e)     ICOM_CALL5(MusicToMIDI,p,a,b,c,d,e)
#define IDirectMusicPerformance8_TimeToRhythm(p,a,b,c,d,e,f)  ICOM_CALL6(TimeToRhythm,p,a,b,c,d,e,f)
#define IDirectMusicPerformance8_RhythmToTime(p,a,b,c,d,e,f)  ICOM_CALL6(RhythmToTime,p,a,b,c,d,e,f)
    /*** IDirectMusicPerformance8 methods */
#define IDirectMusicPerformance8_InitAudio(p,a,b,c,d,e,f,g)   ICOM_CALL7(InitAudio,p,a,b,c,d,e,f,g)
#define IDirectMusicPerformance8_PlaySegmentEx(p,a,b,c,d,e,f,g,h) ICOM_CALL8(PlaySegmentEx,p,a,b,c,d,e,f,g,h)
#define IDirectMusicPerformance8_StopEx(p,a,b,c)              ICOM_CALL3(StopEx,p,a,b,c)
#define IDirectMusicPerformance8_ClonePMsg(p,a,b)             ICOM_CALL2(ClonePMsg,p,a,b)
#define IDirectMusicPerformance8_CreateAudioPath(p,a,b,c)     ICOM_CALL3(CreateAudioPath,p,a,b,c)
#define IDirectMusicPerformance8_CreateStandardAudioPath(p,a,b,c,d) ICOM_CALL4(CreateStandardAudioPath,p,a,b,c,d)
#define IDirectMusicPerformance8_SetDefaultAudioPath(p,a)     ICOM_CALL1(SetDefaultAudioPath,p,a)
#define IDirectMusicPerformance8_GetDefaultAudioPath(p,a)     ICOM_CALL1(GetDefaultAudioPath,p,a)
#define IDirectMusicPerformance8_GetParamEx(p,a,b,c,d,e,f,g)  ICOM_CALL7(GetParamEx,p,a,b,c,d,e,f,g)


#define ICOM_INTERFACE IDirectMusicScript
#define IDirectMusicScript_METHODS \
    ICOM_METHOD2(HRESULT,Init,               IDirectMusicPerformance*,pPerformance, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD2(HRESULT,CallRoutine,        WCHAR*,pwszRoutineName, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD4(HRESULT,SetVariableVariant, WCHAR*,pwszVariableName, VARIANT,varValue, BOOL,fSetRef, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD3(HRESULT,GetVariableVariant, WCHAR*,pwszVariableName, VARIANT*,pvarValue, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD3(HRESULT,SetVariableNumber,  WCHAR*,pwszVariableName, LONG,lValue, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD3(HRESULT,GetVariableNumber,  WCHAR*,pwszVariableName, LONG*,plValue, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD3(HRESULT,SetVariableObject,  WCHAR*,pwszVariableName, IUnknown*,punkValue, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD4(HRESULT,GetVariableObject,  WCHAR*,pwszVariableName, REFIID,riid, LPVOID*,ppv, DMUS_SCRIPT_ERRORINFO*,pErrorInfo) \
    ICOM_METHOD2(HRESULT,EnumRoutine,        DWORD,dwIndex, WCHAR*,pwszName) \
    ICOM_METHOD2(HRESULT,EnumVariable,       DWORD,dwIndex, WCHAR*,pwszName)

#define IDirectMusicScript_IMETHODS \
          IUnknown_IMETHODS \
          IDirectMusicScript_METHODS

ICOM_DEFINE(IDirectMusicScript,IUnknown)
typedef IDirectMusicScript IDirectMusicScript8;
#undef ICOM_INTERFACE


DEFINE_GUID(CLSID_DirectMusicPerformance,0xd2ac2881, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicSegment,0xd2ac2882, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicSegmentState,0xd2ac2883, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicGraph,0xd2ac2884, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicStyle,0xd2ac288a, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicChordMap,0xd2ac288f, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicComposer,0xd2ac2890, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicLoader,0xd2ac2892, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicBand,0x79ba9e00, 0xb6ee, 0x11d1, 0x86, 0xbe, 0x0, 0xc0, 0x4f, 0xbf, 0x8f, 0xef);

DEFINE_GUID(CLSID_DirectMusicPatternTrack,0xd2ac2897, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(CLSID_DirectMusicScript,0x810b5013, 0xe88d, 0x11d2, 0x8b, 0xc1, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xb6); /* {810B5013-E88D-11d2-8BC1-00600893B1B6} */
DEFINE_GUID(CLSID_DirectMusicContainer,0x9301e380, 0x1f22, 0x11d3, 0x82, 0x26, 0xd2, 0xfa, 0x76, 0x25, 0x5d, 0x47);
DEFINE_GUID(CLSID_DirectSoundWave,0x8a667154, 0xf9cb, 0x11d2, 0xad, 0x8a, 0x0, 0x60, 0xb0, 0x57, 0x5a, 0xbc);
DEFINE_GUID(CLSID_DirectMusicSong, 0xaed5f0a5, 0xd972, 0x483d, 0xa3, 0x84, 0x64, 0x9d, 0xfe, 0xb9, 0xc1, 0x81);
DEFINE_GUID(CLSID_DirectMusicAudioPathConfig,0xee0b9ca0, 0xa81e, 0x11d3, 0x9b, 0xd1, 0x0, 0x80, 0xc7, 0x15, 0xa, 0x74);

DEFINE_GUID(GUID_DirectMusicAllTypes,0xd2ac2893, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);

DEFINE_GUID(GUID_NOTIFICATION_SEGMENT,0xd2ac2899, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_NOTIFICATION_PERFORMANCE,0x81f75bc5, 0x4e5d, 0x11d2, 0xbc, 0xc7, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(GUID_NOTIFICATION_MEASUREANDBEAT,0xd2ac289a, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_NOTIFICATION_CHORD,0xd2ac289b, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_NOTIFICATION_COMMAND,0xd2ac289c, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);

DEFINE_GUID(GUID_CommandParam,0xd2ac289d, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_CommandParam2, 0x28f97ef7, 0x9538, 0x11d2, 0x97, 0xa9, 0x0, 0xc0, 0x4f, 0xa3, 0x6e, 0x58);
DEFINE_GUID(GUID_CommandParamNext, 0x472afe7a, 0x281b, 0x11d3, 0x81, 0x7d, 0x0, 0xc0, 0x4f, 0xa3, 0x6e, 0x58);
DEFINE_GUID(GUID_ChordParam,0xd2ac289e, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_RhythmParam,0xd2ac289f, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_IDirectMusicStyle,0xd2ac28a1, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_TimeSignature,0xd2ac28a4, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_TempoParam,0xd2ac28a5, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_Valid_Start_Time,0x7f6b1760, 0x1fdb, 0x11d3, 0x82, 0x26, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0);
DEFINE_GUID(GUID_Play_Marker,0xd8761a41, 0x801a, 0x11d3, 0x9b, 0xd1, 0xda, 0xf7, 0xe1, 0xc3, 0xd8, 0x34);
DEFINE_GUID(GUID_BandParam,0x2bb1938, 0xcb8b, 0x11d2, 0x8b, 0xb9, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xb6);
DEFINE_GUID(GUID_IDirectMusicBand,0xd2ac28ac, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_IDirectMusicChordMap,0xd2ac28ad, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_MuteParam,0xd2ac28af, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_Download,0xd2ac28a7, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_Unload,0xd2ac28a8, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_ConnectToDLSCollection, 0x1db1ae6b, 0xe92e, 0x11d1, 0xa8, 0xc5, 0x0, 0xc0, 0x4f, 0xa3, 0x72, 0x6e);
DEFINE_GUID(GUID_Enable_Auto_Download,0xd2ac28a9, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_Disable_Auto_Download,0xd2ac28aa, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_Clear_All_Bands,0xd2ac28ab, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_StandardMIDIFile, 0x6621075, 0xe92e, 0x11d1, 0xa8, 0xc5, 0x0, 0xc0, 0x4f, 0xa3, 0x72, 0x6e);
#define GUID_IgnoreBankSelectForGM      GUID_StandardMIDIFile
DEFINE_GUID(GUID_DisableTimeSig, 0x45fc707b, 0x1db4, 0x11d2, 0xbc, 0xac, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(GUID_EnableTimeSig, 0x45fc707c, 0x1db4, 0x11d2, 0xbc, 0xac, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(GUID_DisableTempo, 0x45fc707d, 0x1db4, 0x11d2, 0xbc, 0xac, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(GUID_EnableTempo, 0x45fc707e, 0x1db4, 0x11d2, 0xbc, 0xac, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(GUID_SeedVariations, 0x65b76fa5, 0xff37, 0x11d2, 0x81, 0x4e, 0x0, 0xc0, 0x4f, 0xa3, 0x6e, 0x58);
DEFINE_GUID(GUID_MelodyFragment, 0xb291c7f2, 0xb616, 0x11d2, 0x97, 0xfa, 0x0, 0xc0, 0x4f, 0xa3, 0x6e, 0x58);
DEFINE_GUID(GUID_Clear_All_MelodyFragments, 0x8509fee6, 0xb617, 0x11d2, 0x97, 0xfa, 0x0, 0xc0, 0x4f, 0xa3, 0x6e, 0x58);
DEFINE_GUID(GUID_Variations, 0x11f72cce, 0x26e6, 0x4ecd, 0xaf, 0x2e, 0xd6, 0x68, 0xe6, 0x67, 0x7, 0xd8);
DEFINE_GUID(GUID_DownloadToAudioPath,0x9f2c0341, 0xc5c4, 0x11d3, 0x9b, 0xd1, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0);
DEFINE_GUID(GUID_UnloadFromAudioPath,0x9f2c0342, 0xc5c4, 0x11d3, 0x9b, 0xd1, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0);

DEFINE_GUID(GUID_PerfMasterTempo,0xd2ac28b0, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_PerfMasterVolume,0xd2ac28b1, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_PerfMasterGrooveLevel,0xd2ac28b2, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(GUID_PerfAutoDownload, 0xfb09565b, 0x3631, 0x11d2, 0xbc, 0xb8, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);


DEFINE_GUID(GUID_DefaultGMCollection, 0xf17e8673, 0xc3b4, 0x11d1, 0x87, 0xb, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);

DEFINE_GUID(IID_IDirectMusicLoader, 0x2ffaaca2, 0x5dca, 0x11d2, 0xaf, 0xa6, 0x0, 0xaa, 0x0, 0x24, 0xd8, 0xb6);
DEFINE_GUID(IID_IDirectMusicGetLoader,0x68a04844, 0xd13d, 0x11d1, 0xaf, 0xa6, 0x0, 0xaa, 0x0, 0x24, 0xd8, 0xb6);
DEFINE_GUID(IID_IDirectMusicObject,0xd2ac28b5, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(IID_IDirectMusicSegment, 0xf96029a2, 0x4282, 0x11d2, 0x87, 0x17, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(IID_IDirectMusicSegmentState, 0xa3afdcc7, 0xd3ee, 0x11d1, 0xbc, 0x8d, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(IID_IDirectMusicPerformance,0x7d43d03, 0x6523, 0x11d2, 0x87, 0x1d, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(IID_IDirectMusicGraph,0x2befc277, 0x5497, 0x11d2, 0xbc, 0xcb, 0x0, 0xa0, 0xc9, 0x22, 0xe6, 0xeb);
DEFINE_GUID(IID_IDirectMusicStyle,0xd2ac28bd, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(IID_IDirectMusicChordMap,0xd2ac28be, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(IID_IDirectMusicComposer,0xd2ac28bf, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);
DEFINE_GUID(IID_IDirectMusicBand,0xd2ac28c0, 0xb39b, 0x11d1, 0x87, 0x4, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);

DEFINE_GUID(IID_IDirectMusicPerformance2,0x6fc2cae0, 0xbc78, 0x11d2, 0xaf, 0xa6, 0x0, 0xaa, 0x0, 0x24, 0xd8, 0xb6);
DEFINE_GUID(IID_IDirectMusicSegment2, 0xd38894d1, 0xc052, 0x11d2, 0x87, 0x2f, 0x0, 0x60, 0x8, 0x93, 0xb1, 0xbd);

DEFINE_GUID(IID_IDirectMusicLoader8, 0x19e7c08c, 0xa44, 0x4e6a, 0xa1, 0x16, 0x59, 0x5a, 0x7c, 0xd5, 0xde, 0x8c);
DEFINE_GUID(IID_IDirectMusicPerformance8, 0x679c4137, 0xc62e, 0x4147, 0xb2, 0xb4, 0x9d, 0x56, 0x9a, 0xcb, 0x25, 0x4c);
DEFINE_GUID(IID_IDirectMusicSegment8,0xc6784488, 0x41a3, 0x418f, 0xaa, 0x15, 0xb3, 0x50, 0x93, 0xba, 0x42, 0xd4);
DEFINE_GUID(IID_IDirectMusicSegmentState8, 0xa50e4730, 0xae4, 0x48a7, 0x98, 0x39, 0xbc, 0x4, 0xbf, 0xe0, 0x77, 0x72);
DEFINE_GUID(IID_IDirectMusicStyle8, 0xfd24ad8a, 0xa260, 0x453d, 0xbf, 0x50, 0x6f, 0x93, 0x84, 0xf7, 0x9, 0x85);
DEFINE_GUID(IID_IDirectMusicPatternTrack, 0x51c22e10, 0xb49f, 0x46fc, 0xbe, 0xc2, 0xe6, 0x28, 0x8f, 0xb9, 0xed, 0xe6);
#define IID_IDirectMusicPatternTrack8 IID_IDirectMusicPatternTrack
DEFINE_GUID(IID_IDirectMusicScript, 0x2252373a, 0x5814, 0x489b, 0x82, 0x9, 0x31, 0xfe, 0xde, 0xba, 0xf1, 0x37); /* {2252373A-5814-489b-8209-31FEDEBAF137} */
#define IID_IDirectMusicScript8 IID_IDirectMusicScript
DEFINE_GUID(IID_IDirectMusicContainer, 0x9301e386, 0x1f22, 0x11d3, 0x82, 0x26, 0xd2, 0xfa, 0x76, 0x25, 0x5d, 0x47);
#define IID_IDirectMusicContainer8 IID_IDirectMusicContainer
DEFINE_GUID(IID_IDirectMusicSong, 0xa862b2ec, 0x3676, 0x4982, 0x85, 0xa, 0x78, 0x42, 0x77, 0x5e, 0x1d, 0x86);
#define IID_IDirectMusicSong8 IID_IDirectMusicSong
DEFINE_GUID(IID_IDirectMusicAudioPath,0xc87631f5, 0x23be, 0x4986, 0x88, 0x36, 0x5, 0x83, 0x2f, 0xcc, 0x48, 0xf9);
#define IID_IDirectMusicAudioPath8 IID_IDirectMusicAudioPath
#define IID_IDirectMusicGetLoader8 IID_IDirectMusicGetLoader
#define IID_IDirectMusicChordMap8 IID_IDirectMusicChordMap
#define IID_IDirectMusicGraph8 IID_IDirectMusicGraph
#define IID_IDirectMusicBand8 IID_IDirectMusicBand
#define IID_IDirectMusicObject8 IID_IDirectMusicObject
#define IID_IDirectMusicComposer8 IID_IDirectMusicComposer


#ifdef __cplusplus
}; /* extern "C" */
#endif

/* #include <poppack.h> */

#include <dmplugin.h>

#endif /* _DMUSICI_ */
