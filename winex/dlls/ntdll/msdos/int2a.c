/*
 * DOS interrupt 2ah handler
 */

#include <stdlib.h>
#include "msdos.h"
#include "miscemu.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

/**********************************************************************
 *	    INT_Int2aHandler (WPROCS.142)
 *
 * Handler for int 2ah (network).
 */
void WINAPI INT_Int2aHandler( CONTEXT86 *context )
{
    switch(AH_reg(context))
    {
    case 0x00:                             /* NETWORK INSTALLATION CHECK */
        break;

    default:
	INT_BARF( context, 0x2a );
    }
}
