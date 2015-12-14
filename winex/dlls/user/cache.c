/*
 * Wine internally cached objects to speedup some things and prevent
 * infinite duplication of trivial code and data.
 *
 * Copyright 1997 Bertho A. Stultiens
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "windef.h"
#include "wingdi.h"
#include "user.h"

static const WORD wPattern55AA[] =
{
    0x5555, 0xaaaa, 0x5555, 0xaaaa,
    0x5555, 0xaaaa, 0x5555, 0xaaaa
};

static HBRUSH  hPattern55AABrush = 0;
static HBITMAP hPattern55AABitmap = 0;


/*********************************************************************
 *	CACHE_GetPattern55AABrush
 */
HBRUSH CACHE_GetPattern55AABrush(void)
{
    if (!hPattern55AABrush)
    {
        hPattern55AABitmap = CreateBitmap( 8, 8, 1, 1, wPattern55AA );
        hPattern55AABrush = CreatePatternBrush( hPattern55AABitmap );
    }
    return hPattern55AABrush;
}
