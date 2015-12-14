/*
 * NetBIOS interrupt handling
 *
 * Copyright 1995 Alexandre Julliard, Alex Korobka
 */

#include "miscemu.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);


/***********************************************************************
 *           NetBIOSCall      (KERNEL.103)
 *           INT_Int5cHandler (WPROCS.192)
 *
 * Also handler for interrupt 5c.
 */
void WINAPI NetBIOSCall16( CONTEXT86 *context )
{
    BYTE* ptr;
    ptr = MapSL( MAKESEGPTR(context->SegEs,BX_reg(context)) );
    FIXME("(%p): command code %02x (ignored)\n",context, *ptr);
    AL_reg(context) = *(ptr+0x01) = 0xFB; /* NetBIOS emulator not found */
}

