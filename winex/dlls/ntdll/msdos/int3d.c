/*
 * DOS interrupt 3d handler.
 * Copyright 1997 Len White
 */

#include <stdlib.h>
#include "msdos.h"
#include "miscemu.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

/**********************************************************************
 *          INT_Int3dHandler (WPROCS.161)
 *
 * Handler for int 3d (FLOATING POINT EMULATION - STANDALONE FWAIT).
 */
void WINAPI INT_Int3dHandler(CONTEXT86 *context)
{
    switch(AH_reg(context))
    {
    case 0x00:
        break;

    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0xb:
        AH_reg(context) = 0;
        break;

    default:
        INT_BARF( context, 0x3d );
    }
}

