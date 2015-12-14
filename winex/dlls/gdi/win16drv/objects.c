/*
 * GDI objects
 *
 * Copyright 1993 Alexandre Julliard
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include "win16drv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);


/***********************************************************************
 *           WIN16DRV_SelectObject
 */
HGDIOBJ WIN16DRV_SelectObject( DC *dc, HGDIOBJ handle )
{
    TRACE("hdc=%04x %04x\n", dc->hSelf, handle );

    switch(GetObjectType( handle ))
    {
    case OBJ_PEN:    return WIN16DRV_PEN_SelectObject( dc, handle );
    case OBJ_BRUSH:  return WIN16DRV_BRUSH_SelectObject( dc, handle );
    case OBJ_FONT:   return WIN16DRV_FONT_SelectObject( dc, handle );
    case OBJ_REGION: return (HGDIOBJ)SelectClipRgn( dc->hSelf, handle );
    case OBJ_BITMAP:
        FIXME("BITMAP not implemented\n");
        return 1;
    }
    return 0;
}
