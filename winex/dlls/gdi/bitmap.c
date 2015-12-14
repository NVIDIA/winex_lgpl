/*
 * GDI bitmap objects
 *
 * Copyright 1993 Alexandre Julliard
 *           1998 Huw D M Davies
 */

#include <stdlib.h>
#include <string.h>

#include "wine/winbase16.h"
#include "gdi.h"
#include "bitmap.h"
#include "wine/debug.h"
#include "wine/winuser16.h"

WINE_DEFAULT_DEBUG_CHANNEL(bitmap);

BITMAP_DRIVER *BITMAP_Driver = NULL;


/***********************************************************************
 *           BITMAP_GetWidthBytes
 *
 * Return number of bytes taken by a scanline of 16-bit aligned Windows DDB
 * data.
 */
INT BITMAP_GetWidthBytes( INT bmWidth, INT bpp )
{
    switch(bpp)
    {
    case 1:
	return 2 * ((bmWidth+15) >> 4);

    case 24:
	bmWidth *= 3; /* fall through */
    case 8:
	return bmWidth + (bmWidth & 1);

    case 32:
	return bmWidth * 4;

    case 16:
    case 15:
	return bmWidth * 2;

    case 4:
	return 2 * ((bmWidth+3) >> 2);

    default:
	WARN("Unknown depth %d, please report.\n", bpp );
    }
    return -1;
}

/***********************************************************************
 *           CreateUserBitmap    (GDI.407)
 */
HBITMAP16 WINAPI CreateUserBitmap16( INT16 width, INT16 height, UINT16 planes,
                                     UINT16 bpp, LPCVOID bits )
{
    return CreateBitmap16( width, height, planes, bpp, bits );
}

/***********************************************************************
 *           CreateUserDiscardableBitmap    (GDI.409)
 */
HBITMAP16 WINAPI CreateUserDiscardableBitmap16( WORD dummy,
                                                INT16 width, INT16 height )
{
    HDC hdc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
    HBITMAP16 ret = CreateCompatibleBitmap16( hdc, width, height );
    DeleteDC( hdc );
    return ret;
}


/***********************************************************************
 *           CreateBitmap    (GDI.48)
 */
HBITMAP16 WINAPI CreateBitmap16( INT16 width, INT16 height, UINT16 planes,
                                 UINT16 bpp, LPCVOID bits )
{
    return CreateBitmap( width, height, planes, bpp, bits );
}


/******************************************************************************
 * CreateBitmap [GDI32.@]  Creates a bitmap with the specified info
 *
 * PARAMS
 *    width  [I] bitmap width
 *    height [I] bitmap height
 *    planes [I] Number of color planes
 *    bpp    [I] Number of bits to identify a color
 *    bits   [I] Pointer to array containing color data
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: 0
 */
HBITMAP WINAPI CreateBitmap( INT width, INT height, UINT planes,
                                 UINT bpp, LPCVOID bits )
{
    BITMAPOBJ *bmp;
    HBITMAP hbitmap;

    planes = (BYTE)planes;
    bpp    = (BYTE)bpp;

      /* Check parameters */
    if (!height || !width)
    {
        height = 1;
        width  = 1;
        planes = 1;
        bpp    = 1;
    }
    if (planes != 1) {
        FIXME("planes = %d\n", planes);
	return 0;
    }
    if (height < 0) height = -height;
    if (width < 0) width = -width;

      /* Create the BITMAPOBJ */
    if (!(bmp = GDI_AllocObject( sizeof(BITMAPOBJ), BITMAP_MAGIC, &hbitmap ))) return 0;

    TRACE("%dx%d, %d planes, %d bpp, returning %08x\n", width, height,
	  planes, bpp, hbitmap);

    bmp->size.cx = 0;
    bmp->size.cy = 0;
    bmp->bitmap.bmType = 0;
    bmp->bitmap.bmWidth = width;
    bmp->bitmap.bmHeight = height;
    bmp->bitmap.bmPlanes = planes;
    bmp->bitmap.bmBitsPixel = bpp;
    bmp->bitmap.bmWidthBytes = BITMAP_GetWidthBytes( width, bpp );
    bmp->bitmap.bmBits = NULL;

    bmp->funcs = NULL;
    bmp->physBitmap = NULL;
    bmp->dib = NULL;
    bmp->segptr_bits = 0;
    bmp->gdi_owned_bitmap_bits = FALSE;

    if (bits) /* Set bitmap bits */
	SetBitmapBits( hbitmap, height * bmp->bitmap.bmWidthBytes,
                         bits );
    GDI_ReleaseObj( hbitmap );
    return hbitmap;
}


/***********************************************************************
 *           CreateCompatibleBitmap    (GDI.51)
 */
HBITMAP16 WINAPI CreateCompatibleBitmap16(HDC16 hdc, INT16 width, INT16 height)
{
    return CreateCompatibleBitmap( hdc, width, height );
}


/******************************************************************************
 * CreateCompatibleBitmap [GDI32.@]  Creates a bitmap compatible with the DC
 *
 * PARAMS
 *    hdc    [I] Handle to device context
 *    width  [I] Width of bitmap
 *    height [I] Height of bitmap
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: 0
 */
HBITMAP WINAPI CreateCompatibleBitmap( HDC hdc, INT width, INT height)
{
    HBITMAP hbmpRet = 0;
    DC *dc;

    TRACE("(%04x,%d,%d) = \n", hdc, width, height );
    if (!(dc = DC_GetDCPtr( hdc ))) return 0;
    if ((width >= 0x10000) || (height >= 0x10000)) {
	FIXME("got bad width %d or height %d, please look for reason\n",
	      width, height );
    } else {
        /* MS doc says if width or height is 0, return 1-by-1 pixel, monochrome bitmap */
        if (!width || !height)
	   hbmpRet = CreateBitmap( 1, 1, 1, 1, NULL );
	else
	   hbmpRet = CreateBitmap( width, height, 1, dc->bitsPerPixel, NULL );
	if(dc->funcs->pCreateBitmap)
	    dc->funcs->pCreateBitmap( hbmpRet );
    }
    TRACE("\t\t%04x\n", hbmpRet);
    GDI_ReleaseObj(hdc);
    return hbmpRet;
}


/***********************************************************************
 *           CreateBitmapIndirect    (GDI.49)
 */
HBITMAP16 WINAPI CreateBitmapIndirect16( const BITMAP16 * bmp )
{
    return CreateBitmap16( bmp->bmWidth, bmp->bmHeight, bmp->bmPlanes,
                           bmp->bmBitsPixel, MapSL( bmp->bmBits ) );
}


/******************************************************************************
 * CreateBitmapIndirect [GDI32.@]  Creates a bitmap with the specifies info
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: NULL
 */
HBITMAP WINAPI CreateBitmapIndirect(
    const BITMAP * bmp) /* [in] Pointer to the bitmap data */
{
    return CreateBitmap( bmp->bmWidth, bmp->bmHeight, bmp->bmPlanes,
                           bmp->bmBitsPixel, bmp->bmBits );
}


/***********************************************************************
 *           GetBitmapBits    (GDI.74)
 */
LONG WINAPI GetBitmapBits16( HBITMAP16 hbitmap, LONG count, LPVOID buffer )
{
    return GetBitmapBits( hbitmap, count, buffer );
}


/***********************************************************************
 * GetBitmapBits [GDI32.@]  Copies bitmap bits of bitmap to buffer
 *
 * RETURNS
 *    Success: Number of bytes copied
 *    Failure: 0
 */
LONG WINAPI GetBitmapBits(
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    LONG count,        /* [in]  Number of bytes to copy */
    LPVOID bits)       /* [out] Pointer to buffer to receive bits */
{
    BITMAPOBJ *bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    LONG height, ret;

    if (!bmp) return 0;

    /* If the bits vector is null, the function should return the read size */
    if(bits == NULL)
    {
        ret = bmp->bitmap.bmWidthBytes * bmp->bitmap.bmHeight;
        goto done;
    }

    if (count < 0) {
	WARN("(%ld): Negative number of bytes passed???\n", count );
	count = -count;
    }

    /* Only get entire lines */
    height = count / bmp->bitmap.bmWidthBytes;
    if (height > bmp->bitmap.bmHeight) height = bmp->bitmap.bmHeight;
    count = height * bmp->bitmap.bmWidthBytes;
    if (count == 0)
      {
	WARN("Less than one entire line requested\n");
        ret = 0;
        goto done;
      }


    TRACE("(%08x, %ld, %p) %dx%d %d colors fetched height: %ld\n",
          hbitmap, count, bits, bmp->bitmap.bmWidth, bmp->bitmap.bmHeight,
          1 << bmp->bitmap.bmBitsPixel, height );

    if(bmp->funcs) {

        TRACE("Calling device specific BitmapBits\n");
	if(bmp->funcs->pBitmapBits)
	    ret = bmp->funcs->pBitmapBits(hbitmap, bits, count, DDB_GET);
	else {
	    ERR("BitmapBits == NULL??\n");
	    ret = 0;
	}

    } else {

        if(!bmp->bitmap.bmBits) {
	    WARN("Bitmap is empty\n");
	    ret = 0;
	} else {
	    memcpy(bits, bmp->bitmap.bmBits, count);
	    ret = count;
	}

    }
 done:
    GDI_ReleaseObj( hbitmap );
    return ret;
}


/***********************************************************************
 *           SetBitmapBits    (GDI.106)
 */
LONG WINAPI SetBitmapBits16( HBITMAP16 hbitmap, LONG count, LPCVOID buffer )
{
    return SetBitmapBits( hbitmap, count, buffer );
}


/******************************************************************************
 * SetBitmapBits [GDI32.@]  Sets bits of color data for a bitmap
 *
 * RETURNS
 *    Success: Number of bytes used in setting the bitmap bits
 *    Failure: 0
 */
LONG WINAPI SetBitmapBits(
    HBITMAP hbitmap, /* [in] Handle to bitmap */
    LONG count,        /* [in] Number of bytes in bitmap array */
    LPCVOID bits)      /* [in] Address of array with bitmap bits */
{
    BITMAPOBJ *bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    LONG height, ret;

    if ((!bmp) || (!bits))
	return 0;

    if (count < 0) {
	WARN("(%ld): Negative number of bytes passed???\n", count );
	count = -count;
    }

    /* Only get entire lines */
    height = count / bmp->bitmap.bmWidthBytes;
    if (height > bmp->bitmap.bmHeight) height = bmp->bitmap.bmHeight;
    count = height * bmp->bitmap.bmWidthBytes;

    TRACE("(%08x, %ld, %p) %dx%d %d colors fetched height: %ld\n",
          hbitmap, count, bits, bmp->bitmap.bmWidth, bmp->bitmap.bmHeight,
          1 << bmp->bitmap.bmBitsPixel, height );

    if(bmp->funcs) {

        TRACE("Calling device specific BitmapBits\n");
	if(bmp->funcs->pBitmapBits)
	    ret = bmp->funcs->pBitmapBits(hbitmap, (void *) bits, count, DDB_SET);
	else {
	    ERR("BitmapBits == NULL??\n");
	    ret = 0;
	}

    } else {

        if(!bmp->bitmap.bmBits) /* Alloc enough for entire bitmap */
        {
           bmp->bitmap.bmBits = HeapAlloc( GetProcessHeap(), 0, count );
           bmp->gdi_owned_bitmap_bits = TRUE;
        }
	if(!bmp->bitmap.bmBits) {
	    WARN("Unable to allocate bit buffer\n");
	    ret = 0;
	} else {
	    memcpy(bmp->bitmap.bmBits, bits, count);
	    ret = count;
	}
    }

    GDI_ReleaseObj( hbitmap );
    return ret;
}

/**********************************************************************
 *		BITMAP_CopyBitmap
 *
 */
HBITMAP BITMAP_CopyBitmap(HBITMAP hbitmap)
{
    BITMAPOBJ *bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    HBITMAP res = 0;
    BITMAP bm;

    if(!bmp) return 0;

    bm = bmp->bitmap;
    bm.bmBits = NULL;
    res = CreateBitmapIndirect(&bm);

    if(res) {
        char *buf = HeapAlloc( GetProcessHeap(), 0, bm.bmWidthBytes *
			       bm.bmHeight );
        GetBitmapBits (hbitmap, bm.bmWidthBytes * bm.bmHeight, buf);
	SetBitmapBits (res, bm.bmWidthBytes * bm.bmHeight, buf);
	HeapFree( GetProcessHeap(), 0, buf );
    }

    GDI_ReleaseObj( hbitmap );
    return res;
}

/***********************************************************************
 *           BITMAP_DeleteObject
 */
BOOL BITMAP_DeleteObject( HBITMAP16 hbitmap, BITMAPOBJ * bmp )
{
    if (bmp->funcs && bmp->funcs->pDeleteObject)
        bmp->funcs->pDeleteObject( hbitmap );
    
    if( bmp->bitmap.bmBits && bmp->gdi_owned_bitmap_bits)
    {
        HeapFree( GetProcessHeap(), 0, bmp->bitmap.bmBits );
        bmp->bitmap.bmBits = NULL;
    }

    DIB_DeleteDIBSection( bmp );

    return GDI_FreeObject( hbitmap, bmp );
}


/***********************************************************************
 *           BITMAP_GetObject16
 */
INT16 BITMAP_GetObject16( BITMAPOBJ * bmp, INT16 count, LPVOID buffer )
{
    if (bmp->dib)
    {
        if ( count <= sizeof(BITMAP16) )
        {
            BITMAP *bmp32 = &bmp->dib->dsBm;
	    BITMAP16 bmp16;
	    bmp16.bmType       = bmp32->bmType;
	    bmp16.bmWidth      = bmp32->bmWidth;
	    bmp16.bmHeight     = bmp32->bmHeight;
	    bmp16.bmWidthBytes = bmp32->bmWidthBytes;
	    bmp16.bmPlanes     = bmp32->bmPlanes;
	    bmp16.bmBitsPixel  = bmp32->bmBitsPixel;
	    bmp16.bmBits       = (SEGPTR)0;
	    memcpy( buffer, &bmp16, count );
	    return count;
        }
        else
        {
	    FIXME("not implemented for DIBs: count %d\n", count);
	    return 0;
        }
    }
    else
    {
	BITMAP16 bmp16;
	bmp16.bmType       = bmp->bitmap.bmType;
	bmp16.bmWidth      = bmp->bitmap.bmWidth;
	bmp16.bmHeight     = bmp->bitmap.bmHeight;
	bmp16.bmWidthBytes = bmp->bitmap.bmWidthBytes;
	bmp16.bmPlanes     = bmp->bitmap.bmPlanes;
	bmp16.bmBitsPixel  = bmp->bitmap.bmBitsPixel;
	bmp16.bmBits       = (SEGPTR)0;
	if (count > sizeof(bmp16)) count = sizeof(bmp16);
	memcpy( buffer, &bmp16, count );
	return count;
    }
}


/***********************************************************************
 *           BITMAP_GetObject
 */
INT BITMAP_GetObject( BITMAPOBJ * bmp, INT count, LPVOID buffer )
{
    if (bmp->dib)
    {
	if (count < sizeof(DIBSECTION))
	{
	    if (count > sizeof(BITMAP)) count = sizeof(BITMAP);
	}
	else if (count != sizeof(DIBSECTIONOBJ))
	{
	    if (count > sizeof(DIBSECTION)) count = sizeof(DIBSECTION);
	}

	memcpy( buffer, bmp->dib, count );
	return count;
    }
    else
    {
	if (count > sizeof(BITMAP)) count = sizeof(BITMAP);
	memcpy( buffer, &bmp->bitmap, count );
	return count;
    }
}


/***********************************************************************
 *           CreateDiscardableBitmap    (GDI.156)
 */
HBITMAP16 WINAPI CreateDiscardableBitmap16( HDC16 hdc, INT16 width,
                                            INT16 height )
{
    return CreateCompatibleBitmap16( hdc, width, height );
}


/******************************************************************************
 * CreateDiscardableBitmap [GDI32.@]  Creates a discardable bitmap
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: NULL
 */
HBITMAP WINAPI CreateDiscardableBitmap(
    HDC hdc,    /* [in] Handle to device context */
    INT width,  /* [in] Bitmap width */
    INT height) /* [in] Bitmap height */
{
    return CreateCompatibleBitmap( hdc, width, height );
}


/***********************************************************************
 *           GetBitmapDimensionEx    (GDI.468)
 *
 * NOTES
 *    Can this call GetBitmapDimensionEx?
 */
BOOL16 WINAPI GetBitmapDimensionEx16( HBITMAP16 hbitmap, LPSIZE16 size )
{
    BITMAPOBJ * bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    if (!bmp) return FALSE;
    size->cx = bmp->size.cx;
    size->cy = bmp->size.cy;
    GDI_ReleaseObj( hbitmap );
    return TRUE;
}


/******************************************************************************
 * GetBitmapDimensionEx [GDI32.@]  Retrieves dimensions of a bitmap
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetBitmapDimensionEx(
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    LPSIZE size)     /* [out] Address of struct receiving dimensions */
{
    BITMAPOBJ * bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    if (!bmp) return FALSE;
    *size = bmp->size;
    GDI_ReleaseObj( hbitmap );
    return TRUE;
}


/***********************************************************************
 *           GetBitmapDimension    (GDI.162)
 */
DWORD WINAPI GetBitmapDimension16( HBITMAP16 hbitmap )
{
    SIZE16 size;
    if (!GetBitmapDimensionEx16( hbitmap, &size )) return 0;
    return MAKELONG( size.cx, size.cy );
}


/***********************************************************************
 *           SetBitmapDimensionEx    (GDI.478)
 */
BOOL16 WINAPI SetBitmapDimensionEx16( HBITMAP16 hbitmap, INT16 x, INT16 y,
                                      LPSIZE16 prevSize )
{
    BITMAPOBJ * bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    if (!bmp) return FALSE;
    if (prevSize)
    {
        prevSize->cx = bmp->size.cx;
        prevSize->cy = bmp->size.cy;
    }
    bmp->size.cx = x;
    bmp->size.cy = y;
    GDI_ReleaseObj( hbitmap );
    return TRUE;
}


/******************************************************************************
 * SetBitmapDimensionEx [GDI32.@]  Assignes dimensions to a bitmap
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI SetBitmapDimensionEx(
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    INT x,           /* [in]  Bitmap width */
    INT y,           /* [in]  Bitmap height */
    LPSIZE prevSize) /* [out] Address of structure for orig dims */
{
    BITMAPOBJ * bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    if (!bmp) return FALSE;
    if (prevSize) *prevSize = bmp->size;
    bmp->size.cx = x;
    bmp->size.cy = y;
    GDI_ReleaseObj( hbitmap );
    return TRUE;
}


/***********************************************************************
 *           SetBitmapDimension    (GDI.163)
 */
DWORD WINAPI SetBitmapDimension16( HBITMAP16 hbitmap, INT16 x, INT16 y )
{
    SIZE16 size;
    if (!SetBitmapDimensionEx16( hbitmap, x, y, &size )) return 0;
    return MAKELONG( size.cx, size.cy );
}

