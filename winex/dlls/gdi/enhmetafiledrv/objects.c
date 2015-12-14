/*
 * Enhanced MetaFile objects
 *
 * Copyright 1999 Huw D M Davies
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bitmap.h"
#include "enhmetafiledrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(enhmetafile);

/***********************************************************************
 *           EMFDRV_BITMAP_SelectObject
 */
static HBITMAP EMFDRV_BITMAP_SelectObject( DC * dc, HBITMAP hbitmap )
{
    return 0;
}


/***********************************************************************
 *           EMFDRV_CreateBrushIndirect
 */
DWORD EMFDRV_CreateBrushIndirect( DC *dc, HBRUSH hBrush )
{
    DWORD index = 0;
    LOGBRUSH logbrush;

    if (!GetObjectA( hBrush, sizeof(logbrush), &logbrush )) return 0;

    switch (logbrush.lbStyle) {
    case BS_SOLID:
    case BS_HATCHED:
    case BS_NULL:
      {
	EMRCREATEBRUSHINDIRECT emr;
	emr.emr.iType = EMR_CREATEBRUSHINDIRECT;
	emr.emr.nSize = sizeof(emr);
	emr.ihBrush = index = EMFDRV_AddHandleDC( dc );
	emr.lb = logbrush;

	if(!EMFDRV_WriteRecord( dc, &emr.emr ))
	    index = 0;
      }
      break;
    case BS_DIBPATTERN:
      {
	EMRCREATEDIBPATTERNBRUSHPT *emr;
	DWORD bmSize, biSize, size;
	BITMAPINFO *info = GlobalLock16(logbrush.lbHatch);

	if (info->bmiHeader.biCompression)
            bmSize = info->bmiHeader.biSizeImage;
        else
	    bmSize = DIB_GetDIBImageBytes(info->bmiHeader.biWidth,
					  info->bmiHeader.biHeight,
					  info->bmiHeader.biBitCount);
	biSize = DIB_BitmapInfoSize(info, LOWORD(logbrush.lbColor));
	size = sizeof(EMRCREATEDIBPATTERNBRUSHPT) + biSize + bmSize;
	emr = HeapAlloc( GetProcessHeap(), 0, size );
	if(!emr) break;
	emr->emr.iType = EMR_CREATEDIBPATTERNBRUSHPT;
	emr->emr.nSize = size;
	emr->ihBrush = index = EMFDRV_AddHandleDC( dc );
	emr->iUsage = LOWORD(logbrush.lbColor);
	emr->offBmi = sizeof(EMRCREATEDIBPATTERNBRUSHPT);
	emr->cbBmi = biSize;
	emr->offBits = sizeof(EMRCREATEDIBPATTERNBRUSHPT) + biSize;
	memcpy((char *)emr + sizeof(EMRCREATEDIBPATTERNBRUSHPT), info,
	       biSize + bmSize );

	if(!EMFDRV_WriteRecord( dc, &emr->emr ))
	    index = 0;
	HeapFree( GetProcessHeap(), 0, emr );
	GlobalUnlock16(logbrush.lbHatch);
      }
      break;

    case BS_PATTERN:
        FIXME("Unsupported style %x\n",
	      logbrush.lbStyle);
        break;
    default:
        FIXME("Unknown style %x\n", logbrush.lbStyle);
	break;
    }
    return index;
}


/***********************************************************************
 *           EMFDRV_BRUSH_SelectObject
 */
static HBRUSH EMFDRV_BRUSH_SelectObject(DC *dc, HBRUSH hBrush )
{
    EMRSELECTOBJECT emr;
    DWORD index;
    HBRUSH hOldBrush;
    int i;

    /* If the object is a stock brush object, do not need to create it.
     * See definitions in  wingdi.h for range of stock brushes.
     * We do however have to handle setting the higher order bit to
     * designate that this is a stock object.
     */
    for (i = WHITE_BRUSH; i <= NULL_BRUSH; i++)
    {
        if (hBrush == GetStockObject(i))
        {
            index = i | 0x80000000;
            goto found;
        }
    }
    if (!(index = EMFDRV_CreateBrushIndirect(dc, hBrush ))) return 0;

 found:
    emr.emr.iType = EMR_SELECTOBJECT;
    emr.emr.nSize = sizeof(emr);
    emr.ihObject = index;
    if(!EMFDRV_WriteRecord( dc, &emr.emr ))
        return FALSE;

    hOldBrush = dc->hBrush;
    dc->hBrush = hBrush;
    return hOldBrush;
}


/******************************************************************
 *         EMFDRV_CreateFontIndirect
 */
static BOOL EMFDRV_CreateFontIndirect(DC *dc, HFONT hFont )
{
    DWORD index = 0;
    EMREXTCREATEFONTINDIRECTW emr;
    int i;

    if (!GetObjectW( hFont, sizeof(emr.elfw.elfLogFont), &emr.elfw.elfLogFont )) return 0;

    emr.emr.iType = EMR_EXTCREATEFONTINDIRECTW;
    emr.emr.nSize = (sizeof(emr) + 3) / 4 * 4;
    emr.ihFont = index = EMFDRV_AddHandleDC( dc );
    emr.elfw.elfFullName[0] = '\0';
    emr.elfw.elfStyle[0]    = '\0';
    emr.elfw.elfVersion     = 0;
    emr.elfw.elfStyleSize   = 0;
    emr.elfw.elfMatch       = 0;
    emr.elfw.elfReserved    = 0;
    for(i = 0; i < ELF_VENDOR_SIZE; i++)
        emr.elfw.elfVendorId[i] = 0;
    emr.elfw.elfCulture                 = PAN_CULTURE_LATIN;
    emr.elfw.elfPanose.bFamilyType      = PAN_NO_FIT;
    emr.elfw.elfPanose.bSerifStyle      = PAN_NO_FIT;
    emr.elfw.elfPanose.bWeight          = PAN_NO_FIT;
    emr.elfw.elfPanose.bProportion      = PAN_NO_FIT;
    emr.elfw.elfPanose.bContrast        = PAN_NO_FIT;
    emr.elfw.elfPanose.bStrokeVariation = PAN_NO_FIT;
    emr.elfw.elfPanose.bArmStyle        = PAN_NO_FIT;
    emr.elfw.elfPanose.bLetterform      = PAN_NO_FIT;
    emr.elfw.elfPanose.bMidline         = PAN_NO_FIT;
    emr.elfw.elfPanose.bXHeight         = PAN_NO_FIT;

    if(!EMFDRV_WriteRecord( dc, &emr.emr ))
        index = 0;
    return index;
}


/***********************************************************************
 *           EMFDRV_FONT_SelectObject
 */
static HFONT EMFDRV_FONT_SelectObject( DC * dc, HFONT hFont )
{
    EMRSELECTOBJECT emr;
    DWORD index;
    int i;

    /* If the object is a stock font object, do not need to create it.
     * See definitions in  wingdi.h for range of stock fonts.
     * We do however have to handle setting the higher order bit to
     * designate that this is a stock object.
     */

    for (i = OEM_FIXED_FONT; i <= DEFAULT_GUI_FONT; i++)
    {
        if (i != DEFAULT_PALETTE && hFont == GetStockObject(i))
        {
            index = i | 0x80000000;
            goto found;
        }
    }
    if (!(index = EMFDRV_CreateFontIndirect(dc, hFont ))) return GDI_ERROR;
 found:
    emr.emr.iType = EMR_SELECTOBJECT;
    emr.emr.nSize = sizeof(emr);
    emr.ihObject = index;
    if(!EMFDRV_WriteRecord( dc, &emr.emr ))
        return GDI_ERROR;

    return FALSE;
}



/******************************************************************
 *         EMFDRV_CreatePenIndirect
 */
static HPEN EMFDRV_CreatePenIndirect(DC *dc, HPEN hPen )
{
    EMRCREATEPEN emr;
    DWORD index = 0;

    if (!GetObjectA( hPen, sizeof(emr.lopn), &emr.lopn )) return 0;

    emr.emr.iType = EMR_CREATEPEN;
    emr.emr.nSize = sizeof(emr);
    emr.ihPen = index = EMFDRV_AddHandleDC( dc );

    if(!EMFDRV_WriteRecord( dc, &emr.emr ))
        index = 0;
    return index;
}

/******************************************************************
 *         EMFDRV_PEN_SelectObject
 */
static HPEN EMFDRV_PEN_SelectObject(DC *dc, HPEN hPen )
{
    EMRSELECTOBJECT emr;
    DWORD index;
    HPEN hOldPen;
    int i;

    /* If the object is a stock pen object, do not need to create it.
     * See definitions in  wingdi.h for range of stock pens.
     * We do however have to handle setting the higher order bit to
     * designate that this is a stock object.
     */

    for (i = WHITE_PEN; i <= NULL_PEN; i++)
    {
        if (hPen == GetStockObject(i))
        {
            index = i | 0x80000000;
            goto found;
        }
    }
    if (!(index = EMFDRV_CreatePenIndirect(dc, hPen ))) return 0;
 found:
    emr.emr.iType = EMR_SELECTOBJECT;
    emr.emr.nSize = sizeof(emr);
    emr.ihObject = index;
    if(!EMFDRV_WriteRecord( dc, &emr.emr ))
        return FALSE;

    hOldPen = dc->hPen;
    dc->hPen = hPen;
    return hOldPen;
}


/***********************************************************************
 *           EMFDRV_SelectObject
 */
HGDIOBJ EMFDRV_SelectObject( DC *dc, HGDIOBJ handle )
{
    GDIOBJHDR * ptr = GDI_GetObjPtr( handle, MAGIC_DONTCARE );
    HGDIOBJ ret = 0;

    if (!ptr) return 0;
    TRACE("hdc=%04x %04x\n", dc->hSelf, handle );

    switch(GDIMAGIC(ptr->wMagic))
    {
      case PEN_MAGIC:
	  ret = EMFDRV_PEN_SelectObject( dc, handle );
	  break;
      case BRUSH_MAGIC:
	  ret = EMFDRV_BRUSH_SelectObject( dc, handle );
	  break;
      case FONT_MAGIC:
	  ret = EMFDRV_FONT_SelectObject( dc, handle );
	  break;
      case BITMAP_MAGIC:
	  ret = EMFDRV_BITMAP_SelectObject( dc, handle );
	  break;
    }
    GDI_ReleaseObj( handle );
    return ret;
}


