/* Definition for OSS drivers : wine multimedia system */

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#if defined(HAVE_SYS_SOUNDCARD_H)
# include <sys/soundcard.h>
#elif defined(HAVE_MACHINE_SOUNDCARD_H)
# include <machine/soundcard.h>
#elif defined(HAVE_SOUNDCARD_H)
# include <soundcard.h>
#endif

#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif

extern LONG OSS_WaveInit(void);
extern BOOL OSS_MidiInit(void);
extern void OSS_WaveFini(void);

#define WINE_OSS_MODE_MONO   0
#define WINE_OSS_MODE_STEREO 1

