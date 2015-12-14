/*
 * Main function.
 *
 * Copyright 1994 Alexandre Julliard
 */

#include "config.h"

#include <locale.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <unistd.h>

#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "winternl.h"
#include "winnls.h"
#include "winerror.h"
#include "options.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(file);

#if defined(__APPLE__) && defined(__i386__)

#ifdef TYPE_ASM_SUPPORTED
# define __ASM_FUNC(name) ".type " __ASM_NAME(name) ",@function"
#elif defined(NEED_TYPE_IN_DEF)
# define __ASM_FUNC(name) ".def " __ASM_NAME(name) "; .scl 2; .type 32; .endef"
#else
# define __ASM_FUNC(name)
#endif

#ifdef NEED_UNDERSCORE_PREFIX
# define __ASM_NAME(name) "_" name
#else
# define __ASM_NAME(name) name
#endif

# define __ASM_GLOBAL_FUNC(name,code) \
      __asm__( ".align 4\n\t" \
               ".data\n\t" \
               ".globl " __ASM_NAME(#name) "\n\t" \
               __ASM_FUNC(#name) "\n" \
               __ASM_NAME(#name) ":\n\t" \
               code);

__ASM_GLOBAL_FUNC( NtCurrentTeb, ".byte 0x64\n\tmovl 0x18,%eax\n\tret" );
#endif

/***********************************************************************
 *           Beep   (KERNEL32.@)
 */
BOOL WINAPI Beep( DWORD dwFreq, DWORD dwDur )
{
    static char beep = '\a';
    /* dwFreq and dwDur are ignored by Win95 */
    if (isatty(2)) write( 2, &beep, 1 );
    return TRUE;
}


/***********************************************************************
*	FileCDR (KERNEL.130)
*/
FARPROC16 WINAPI FileCDR16(FARPROC16 x)
{
	FIXME_(file)("(0x%8x): stub\n", (int) x);
	return (FARPROC16)TRUE;
}
