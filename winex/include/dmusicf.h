/* FIXME: Lacking */
#ifndef _DMUSICF_
#define _DMUSICF_

#define COM_NO_WINDOWS_H
#include "objbase.h"

#include "mmsystem.h"

/* #include <pshpack8.h> */

#ifdef __cplusplus
extern "C" {
#endif

#define DMUS_FOURCC_GUID_CHUNK			mmioFOURCC('g','u','i','d')
#define DMUS_FOURCC_INFO_LIST			mmioFOURCC('I','N','F','O')
#define DMUS_FOURCC_UNFO_LIST			mmioFOURCC('U','N','F','O')
#define DMUS_FOURCC_UNAM_CHUNK			mmioFOURCC('U','N','A','M')
#define DMUS_FOURCC_UART_CHUNK			mmioFOURCC('U','A','R','T')
#define DMUS_FOURCC_UCOP_CHUNK			mmioFOURCC('U','C','O','P')
#define DMUS_FOURCC_USBJ_CHUNK			mmioFOURCC('U','S','B','J')
#define DMUS_FOURCC_UCMT_CHUNK			mmioFOURCC('U','C','M','T')
#define DMUS_FOURCC_CATEGORY_CHUNK		mmioFOURCC('c','a','t','g')
#define DMUS_FOURCC_VERSION_CHUNK		mmioFOURCC('v','e','r','s')

typedef struct _DMUS_IO_SEQ_ITEM
{
    MUSIC_TIME    mtTime;
    MUSIC_TIME    mtDuration;
    DWORD         dwPChannel;
    short         nOffset; 
    BYTE          bStatus;
    BYTE          bByte1;
    BYTE          bByte2;
} DMUS_IO_SEQ_ITEM;


typedef struct _DMUS_IO_CURVE_ITEM
{
    MUSIC_TIME  mtStart;
    MUSIC_TIME  mtDuration;
    MUSIC_TIME  mtResetDuration;
    DWORD       dwPChannel;
    short       nOffset;
    short       nStartValue;
    short       nEndValue;
    short       nResetValue;
    BYTE        bType;
    BYTE        bCurveShape;
    BYTE        bCCData;
    BYTE        bFlags;
    WORD        wParamType;
    WORD        wMergeIndex;
} DMUS_IO_CURVE_ITEM;


typedef struct _DMUS_IO_TEMPO_ITEM {
  MUSIC_TIME lTime;
  DWORD _padding_; /* so we don't need pshpack8.h... */
  double dblTempo;
} DMUS_IO_TEMPO_ITEM;

typedef struct _DMUS_TEMPO_PARAM {
  MUSIC_TIME mtTime;
  double dblTempo;
} DMUS_TEMPO_PARAM;

#define DMUS_FOURCC_WAVEHEADER_CHUNK		mmioFOURCC('w','a','v','h')

typedef struct _DMUS_IO_WAVE_HEADER {
  REFERENCE_TIME rtReadAhead;
  DWORD dwFlags;
} DMUS_IO_WAVE_HEADER;

#define DMUS_FOURCC_WAVETRACK_LIST		mmioFOURCC('w','a','v','t')
#define DMUS_FOURCC_WAVETRACK_CHUNK		mmioFOURCC('w','a','t','h')
#define DMUS_FOURCC_WAVEPART_LIST		mmioFOURCC('w','a','v','p')
#define DMUS_FOURCC_WAVEPART_CHUNK		mmioFOURCC('w','a','p','h')
#define DMUS_FOURCC_WAVEITEM_LIST		mmioFOURCC('w','a','v','i')
#define DMUS_FOURCC_WAVE_LIST			mmioFOURCC('w','a','v','e')
#define DMUS_FOURCC_WAVEITEM_CHUNK		mmioFOURCC('w','a','i','h')

typedef struct _DMUS_IO_WAVE_TRACK_HEADER {
  long lVolume;
  DWORD dwFlags;
} DMUS_IO_WAVE_TRACK_HEADER;

#define DMUS_WAVETRACKF_SYNC_VAR        0x1
#define DMUS_WAVETRACKF_PERSIST_CONTROL 0x2

typedef struct _DMUS_IO_WAVE_PART_HEADER {
  long lVolume;
  DWORD dwVariations;
  DWORD dwPChannel;
  DWORD dwLockToPart;
  DWORD dwFlags;
  DWORD dwIndex;
} DMUS_IO_WAVE_PART_HEADER;

typedef struct _DMUS_IO_WAVE_ITEM_HEADER {
  long lVolume;
  long lPitch;
  DWORD dwVariations;
  DWORD _padding_; /* so we don't need pshpack8.h... */
  REFERENCE_TIME rtTime;
  REFERENCE_TIME rtStartOffset;
  REFERENCE_TIME rtReserved;
  REFERENCE_TIME rtDuration;
  MUSIC_TIME mtLogicalTime;
  DWORD dwLoopStart;
  DWORD dwLoopEnd;
  DWORD dwFlags;
} DMUS_IO_WAVE_ITEM_HEADER;

#define DMUS_FOURCC_CONTAINER_FORM		mmioFOURCC('D','M','C','N')
#define DMUS_FOURCC_CONTAINER_CHUNK		mmioFOURCC('c','o','n','h')
#define DMUS_FOURCC_CONTAINED_ALIAS_CHUNK	mmioFOURCC('c','o','b','a')
#define DMUS_FOURCC_CONTAINED_OBJECT_CHUNK	mmioFOURCC('c','o','b','h')
#define DMUS_FOURCC_CONTAINED_OBJECTS_LIST	mmioFOURCC('c','o','s','l')
#define DMUS_FOURCC_CONTAINED_OBJECT_LIST	mmioFOURCC('c','o','b','l')

typedef struct _DMUS_IO_CONTAINER_HEADER {
  DWORD dwFlags;
} DMUS_IO_CONTAINER_HEADER;

#define DMUS_CONTAINER_NOLOADS (1 << 1)

typedef struct _DMUS_IO_CONTAINED_OBJECT_HEADER {
  GUID guidClassID;
  DWORD dwFlags;
  FOURCC ckid;
  FOURCC fccType;
} DMUS_IO_CONTAINED_OBJECT_HEADER;

#define DMUS_CONTAINED_OBJF_KEEP 1

#define DMUS_FOURCC_SEGMENT_FORM		mmioFOURCC('D','M','S','G')
#define DMUS_FOURCC_SEGMENT_CHUNK		mmioFOURCC('s','e','g','h')
#define DMUS_FOURCC_TRACK_LIST			mmioFOURCC('t','r','k','l')
#define DMUS_FOURCC_TRACK_FORM			mmioFOURCC('D','M','T','K')
#define DMUS_FOURCC_TRACK_CHUNK			mmioFOURCC('t','r','k','h')
#define DMUS_FOURCC_TRACK_EXTRAS_CHUNK		mmioFOURCC('t','r','k','x')

typedef struct _DMUS_IO_SEGMENT_HEADER {
  DWORD       dwRepeats;
  MUSIC_TIME  mtLength;
  MUSIC_TIME  mtPlayStart;
  MUSIC_TIME  mtLoopStart;
  MUSIC_TIME  mtLoopEnd;
  DWORD       dwResolution;
  /* DX8 stuff */
  REFERENCE_TIME rtLength;
  DWORD       dwFlags;
  DWORD       dwReserved;
} DMUS_IO_SEGMENT_HEADER;

#define DMUS_SEGIOF_REFLENGTH 1

typedef struct _DMUS_IO_TRACK_HEADER {
  GUID guidClassID;
  DWORD dwPosition;
  DWORD dwGroup;
  FOURCC ckid;
  FOURCC fccType;
} DMUS_IO_TRACK_HEADER;

typedef struct _DMUS_IO_TRACK_EXTRAS_HEADER {
  DWORD dwFlags;
  DWORD dwPriority;
} DMUS_IO_TRACK_EXTRAS_HEADER;

#define DMUS_FOURCC_REF_LIST        mmioFOURCC('D','M','R','F')
#define DMUS_FOURCC_REF_CHUNK       mmioFOURCC('r','e','f','h')
#define DMUS_FOURCC_DATE_CHUNK      mmioFOURCC('d','a','t','e')
#define DMUS_FOURCC_NAME_CHUNK      mmioFOURCC('n','a','m','e')
#define DMUS_FOURCC_FILE_CHUNK      mmioFOURCC('f','i','l','e')

typedef struct _DMUS_IO_REFERENCE {
  GUID guidClassID;
  DWORD dwValidData;
} DMUS_IO_REFERENCE;

#define DMUS_FOURCC_TEMPO_TRACK     mmioFOURCC('t','e','t','r')

#define DMUS_FOURCC_SEQ_TRACK       mmioFOURCC('s','e','q','t')
#define DMUS_FOURCC_SEQ_LIST        mmioFOURCC('e','v','t','l')
#define DMUS_FOURCC_CURVE_LIST      mmioFOURCC('c','u','r','l')

#define DMUS_FOURCC_SYSEX_TRACK     mmioFOURCC('s','y','e','x')

#define DMUS_FOURCC_TIMESIGNATURE_TRACK mmioFOURCC('t','i','m','s')

typedef struct _DMUS_IO_TIMESIGNATURE_ITEM {
  MUSIC_TIME lTime;
  BYTE bBeatsPerMeasure;
  BYTE bBeat;
  WORD wGridsPerBeat;
} DMUS_IO_TIMESIGNATURE_ITEM;

#define DMUS_FOURCC_TIMESIGTRACK_LIST   mmioFOURCC('T','I','M','S')
#define DMUS_FOURCC_TIMESIG_CHUNK       DMUS_FOURCC_TIMESIGNATURE_TRACK

#ifdef __cplusplus
}; /* extern "C" */
#endif

/* #include <poppack.h> */

#endif /* _DMUSICF_ */
