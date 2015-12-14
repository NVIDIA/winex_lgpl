/*
 * DOS interrupt 25h handler
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "msdos.h"
#include "miscemu.h"
#include "drive.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);


/**********************************************************************
 *	    INT_Int25Handler (WPROCS.137)
 *
 * Handler for int 25h (absolute disk read).
 */
void WINAPI INT_Int25Handler( CONTEXT86 *context )
{
    BYTE *dataptr = CTX_SEG_OFF_TO_LIN( context, context->SegDs, context->Ebx );
    DWORD begin, length;

    if (!DRIVE_IsValid(LOBYTE(context->Eax)))
    {
        SET_CFLAG(context);
        AX_reg(context) = 0x0201;        /* unknown unit */
        return;
    }

    if (LOWORD(context->Ecx) == 0xffff)
    {
        begin   = *(DWORD *)dataptr;
        length  = *(WORD *)(dataptr + 4);
        dataptr = (BYTE *)CTX_SEG_OFF_TO_LIN( context,
					*(WORD *)(dataptr + 8), *(DWORD *)(dataptr + 6) );
    }
    else
    {
        begin  = LOWORD(context->Edx);
        length = LOWORD(context->Ecx);
    }
    TRACE("int25: abs diskread, drive %d, sector %ld, "
                 "count %ld, buffer %p\n",
          LOBYTE(context->Eax), begin, length, dataptr);

    DRIVE_RawRead(LOBYTE(context->Eax), begin, length, dataptr, TRUE);
    RESET_CFLAG(context);
}

