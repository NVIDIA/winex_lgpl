/*
 * Main initialization code
 */
#include "config.h"

#include <locale.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#ifdef MALLOC_DEBUGGING
# include <malloc.h>
#endif
#include "windef.h"
#include "wine/winbase16.h"
#include "drive.h"
#include "wine/file.h"
#include "options.h"
#include "module.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(server);

extern void SHELL_LoadRegistry(void);
extern int wine_dbg_switch_trace(BOOL init, BOOL switchIt);

/***********************************************************************
 *           Main initialisation routine
 */
BOOL MAIN_MainInit(void)
{
#ifdef MALLOC_DEBUGGING
    char *trace;

    mcheck(NULL);
    if (!(trace = getenv("MALLOC_TRACE")))
        MESSAGE( "MALLOC_TRACE not set. No trace generated\n" );
    else
    {
        MESSAGE( "malloc trace goes to %s\n", trace );
        mtrace();
    }
#endif
    setbuf(stdout,NULL);
    setbuf(stderr,NULL);
    setlocale(LC_CTYPE,"");

    /* Load the configuration file */
    if (!PROFILE_LoadWineIni()) return FALSE;

    /* Initialise DOS drives */
    if (!DRIVE_Init()) return FALSE;

    /* Initialise DOS directories */
    if (!DIR_Init()) return FALSE;

    /* Registry initialisation */
    SHELL_LoadRegistry();

    /* Initialize module loadorder */
    if (CLIENT_IsBootThread()) MODULE_InitLoadOrder();

    /* Global boot finished, the rest is process-local */
    CLIENT_BootDone( (wine_dbg_switch_trace(FALSE,FALSE) != 0) ? (TRACE_ON(server)?1:0) : 0 );

    return TRUE;
}


/***********************************************************************
 *           ExitKernel (KERNEL.2)
 *
 * Clean-up everything and exit the Wine process.
 *
 */
void WINAPI ExitKernel16( void )
{
    /* Do the clean-up stuff */

    WriteOutProfiles16();
    TerminateProcess( GetCurrentProcess(), 0 );
}

