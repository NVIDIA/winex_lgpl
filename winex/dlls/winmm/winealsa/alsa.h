/* Definition for ALSA drivers : wine multimedia system */

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#define ALSA_PCM_OLD_HW_PARAMS_API
#define ALSA_PCM_OLD_SW_PARAMS_API

#ifdef interface
  /* objbase.h has a line "#define interface struct"
   * however some versions of alsa have a function paramater called "interface" and for some
   * strange reason these two pieces of code don't get along well together... 
   * Lets just undefine it before including the alsa headers to be safe. */
  #undef interface
#endif

#if defined(HAVE_ALSA_ASOUNDLIB_H)
#include <alsa/asoundlib.h>
#elif defined(HAVE_SYS_ASOUNDLIB_H)
#include <sys/asoundlib.h>
#endif
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif

extern LONG ALSA_WaveInit(void);
extern LONG ALSA_MidiInit(void);
