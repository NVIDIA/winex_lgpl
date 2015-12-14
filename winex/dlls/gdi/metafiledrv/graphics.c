/*
 * Metafile driver graphics functions
 *
 * Copyright 1993, 1994 Alexandre Julliard
 */

#include <stdlib.h>
#include <string.h>

#include "gdi.h"
#include "region.h"
#include "metafiledrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(metafile);

/**********************************************************************
 *	     MFDRV_MoveTo
 */
BOOL
MFDRV_MoveTo(DC *dc, INT x, INT y)
{
    return MFDRV_MetaParam2(dc,META_MOVETO,x,y);
}

/***********************************************************************
 *           MFDRV_LineTo
 */
BOOL
MFDRV_LineTo( DC *dc, INT x, INT y )
{
     return MFDRV_MetaParam2(dc, META_LINETO, x, y);
}


/***********************************************************************
 *           MFDRV_Arc
 */
BOOL
MFDRV_Arc( DC *dc, INT left, INT top, INT right, INT bottom,
           INT xstart, INT ystart, INT xend, INT yend )
{
     return MFDRV_MetaParam8(dc, META_ARC, left, top, right, bottom,
			     xstart, ystart, xend, yend);
}


/***********************************************************************
 *           MFDRV_Pie
 */
BOOL
MFDRV_Pie( DC *dc, INT left, INT top, INT right, INT bottom,
           INT xstart, INT ystart, INT xend, INT yend )
{
    return MFDRV_MetaParam8(dc, META_PIE, left, top, right, bottom,
			    xstart, ystart, xend, yend);
}


/***********************************************************************
 *           MFDRV_Chord
 */
BOOL
MFDRV_Chord( DC *dc, INT left, INT top, INT right, INT bottom,
             INT xstart, INT ystart, INT xend, INT yend )
{
    return MFDRV_MetaParam8(dc, META_CHORD, left, top, right, bottom,
			    xstart, ystart, xend, yend);
}

/***********************************************************************
 *           MFDRV_Ellipse
 */
BOOL
MFDRV_Ellipse( DC *dc, INT left, INT top, INT right, INT bottom )
{
    return MFDRV_MetaParam4(dc, META_ELLIPSE, left, top, right, bottom);
}

/***********************************************************************
 *           MFDRV_Rectangle
 */
BOOL
MFDRV_Rectangle(DC *dc, INT left, INT top, INT right, INT bottom)
{
    return MFDRV_MetaParam4(dc, META_RECTANGLE, left, top, right, bottom);
}

/***********************************************************************
 *           MFDRV_RoundRect
 */
BOOL
MFDRV_RoundRect( DC *dc, INT left, INT top, INT right,
                 INT bottom, INT ell_width, INT ell_height )
{
    return MFDRV_MetaParam6(dc, META_ROUNDRECT, left, top, right, bottom,
			    ell_width, ell_height);
}

/***********************************************************************
 *           MFDRV_SetPixel
 */
COLORREF
MFDRV_SetPixel( DC *dc, INT x, INT y, COLORREF color )
{
    return MFDRV_MetaParam4(dc, META_SETPIXEL, x, y,HIWORD(color),
			    LOWORD(color));
}


/******************************************************************
 *         MFDRV_MetaPoly - implements Polygon and Polyline
 */
static BOOL MFDRV_MetaPoly(DC *dc, short func, LPPOINT16 pt, short count)
{
    BOOL ret;
    DWORD len;
    METARECORD *mr;

    len = sizeof(METARECORD) + (count * 4);
    if (!(mr = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len )))
	return FALSE;

    mr->rdSize = len / 2;
    mr->rdFunction = func;
    *(mr->rdParm) = count;
    memcpy(mr->rdParm + 1, pt, count * 4);
    ret = MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
    HeapFree( GetProcessHeap(), 0, mr);
    return ret;
}


/**********************************************************************
 *          MFDRV_Polyline
 */
BOOL
MFDRV_Polyline( DC *dc, const POINT* pt, INT count )
{
    register int i;
    LPPOINT16	pt16;
    BOOL16	ret;

    pt16 = (LPPOINT16)HeapAlloc( GetProcessHeap(), 0, sizeof(POINT16)*count );
    if(!pt16) return FALSE;
    for (i=count;i--;) CONV_POINT32TO16(&(pt[i]),&(pt16[i]));
    ret = MFDRV_MetaPoly(dc, META_POLYLINE, pt16, count);

    HeapFree( GetProcessHeap(), 0, pt16 );
    return ret;
}


/**********************************************************************
 *          MFDRV_Polygon
 */
BOOL
MFDRV_Polygon( DC *dc, const POINT* pt, INT count )
{
    register int i;
    LPPOINT16	pt16;
    BOOL16	ret;

    pt16 = (LPPOINT16) HeapAlloc( GetProcessHeap(), 0, sizeof(POINT16)*count );
    if(!pt16) return FALSE;
    for (i=count;i--;) CONV_POINT32TO16(&(pt[i]),&(pt16[i]));
    ret = MFDRV_MetaPoly(dc, META_POLYGON, pt16, count);

    HeapFree( GetProcessHeap(), 0, pt16 );
    return ret;
}


/**********************************************************************
 *          MFDRV_PolyPolygon
 */
BOOL
MFDRV_PolyPolygon( DC *dc, const POINT* pt, const INT* counts, UINT polygons)
{
    int		i,j;
    LPPOINT16	pt16;
    const POINT* curpt=pt;
    BOOL	ret;

    for (i=0;i<polygons;i++) {
    	pt16=(LPPOINT16)HeapAlloc( GetProcessHeap(), 0,
				   sizeof(POINT16) * counts[i] );
	if(!pt16) return FALSE;
	for (j=counts[i];j--;) CONV_POINT32TO16(&(curpt[j]),&(pt16[j]));
	ret = MFDRV_MetaPoly(dc, META_POLYGON, pt16, counts[i]);
	HeapFree( GetProcessHeap(), 0, pt16 );
	if (!ret)
	    return FALSE;
	curpt+=counts[i];
    }
    return TRUE;
}


/**********************************************************************
 *          MFDRV_ExtFloodFill
 */
BOOL
MFDRV_ExtFloodFill( DC *dc, INT x, INT y, COLORREF color, UINT fillType )
{
    return MFDRV_MetaParam4(dc,META_FLOODFILL,x,y,HIWORD(color),
			    LOWORD(color));
}


/******************************************************************
 *         MFDRV_CreateRegion
 *
 * For explanation of the format of the record see MF_Play_MetaCreateRegion in
 * objects/metafile.c
 */
static INT16 MFDRV_CreateRegion(DC *dc, HRGN hrgn)
{
    DWORD len;
    METARECORD *mr;
    RGNDATA *rgndata;
    RECT *pCurRect, *pEndRect;
    WORD Bands = 0, MaxBands = 0;
    WORD *Param, *StartBand;
    BOOL ret;

    len = GetRegionData( hrgn, 0, NULL );
    if( !(rgndata = HeapAlloc( GetProcessHeap(), 0, len )) ) {
        WARN("Can't alloc rgndata buffer\n");
	return -1;
    }
    GetRegionData( hrgn, len, rgndata );

    /* Overestimate of length:
     * Assume every rect is a separate band -> 6 WORDs per rect
     */
    len = sizeof(METARECORD) + 20 + (rgndata->rdh.nCount * 12);
    if( !(mr = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len )) ) {
        WARN("Can't alloc METARECORD buffer\n");
	HeapFree( GetProcessHeap(), 0, rgndata );
	return -1;
    }

    Param = mr->rdParm + 11;
    StartBand = NULL;

    pEndRect = (RECT *)rgndata->Buffer + rgndata->rdh.nCount;
    for(pCurRect = (RECT *)rgndata->Buffer; pCurRect < pEndRect; pCurRect++)
    {
        if( StartBand && pCurRect->top == *(StartBand + 1) )
        {
	    *Param++ = pCurRect->left;
	    *Param++ = pCurRect->right;
	}
	else
	{
	    if(StartBand)
	    {
	        *StartBand = Param - StartBand - 3;
		*Param++ = *StartBand;
		if(*StartBand > MaxBands)
		    MaxBands = *StartBand;
		Bands++;
	    }
	    StartBand = Param++;
	    *Param++ = pCurRect->top;
	    *Param++ = pCurRect->bottom;
	    *Param++ = pCurRect->left;
	    *Param++ = pCurRect->right;
	}
    }
    len = Param - (WORD *)mr;

    mr->rdParm[0] = 0;
    mr->rdParm[1] = 6;
    mr->rdParm[2] = 0x1234;
    mr->rdParm[3] = 0;
    mr->rdParm[4] = len * 2;
    mr->rdParm[5] = Bands;
    mr->rdParm[6] = MaxBands;
    mr->rdParm[7] = rgndata->rdh.rcBound.left;
    mr->rdParm[8] = rgndata->rdh.rcBound.top;
    mr->rdParm[9] = rgndata->rdh.rcBound.right;
    mr->rdParm[10] = rgndata->rdh.rcBound.bottom;
    mr->rdFunction = META_CREATEREGION;
    mr->rdSize = len / 2;
    ret = MFDRV_WriteRecord( dc, mr, mr->rdSize * 2 );
    HeapFree( GetProcessHeap(), 0, mr );
    HeapFree( GetProcessHeap(), 0, rgndata );
    if(!ret)
    {
        WARN("MFDRV_WriteRecord failed\n");
	return -1;
    }
    return MFDRV_AddHandleDC( dc );
}


/**********************************************************************
 *          MFDRV_PaintRgn
 */
BOOL
MFDRV_PaintRgn( DC *dc, HRGN hrgn )
{
    INT16 index;
    index = MFDRV_CreateRegion( dc, hrgn );
    if(index == -1)
        return FALSE;
    return MFDRV_MetaParam1( dc, META_PAINTREGION, index );
}


/**********************************************************************
 *          MFDRV_InvertRgn
 */
BOOL
MFDRV_InvertRgn( DC *dc, HRGN hrgn )
{
    INT16 index;
    index = MFDRV_CreateRegion( dc, hrgn );
    if(index == -1)
        return FALSE;
    return MFDRV_MetaParam1( dc, META_INVERTREGION, index );
}


/**********************************************************************
 *          MFDRV_FillRgn
 */
BOOL
MFDRV_FillRgn( DC *dc, HRGN hrgn, HBRUSH hbrush )
{
    INT16 iRgn, iBrush;
    iRgn = MFDRV_CreateRegion( dc, hrgn );
    if(iRgn == -1)
        return FALSE;
    iBrush = MFDRV_CreateBrushIndirect( dc, hbrush );
    if(iBrush == -1)
        return FALSE;
    return MFDRV_MetaParam2( dc, META_FILLREGION, iRgn, iBrush );
}

/**********************************************************************
 *          MFDRV_FrameRgn
 */
BOOL
MFDRV_FrameRgn( DC *dc, HRGN hrgn, HBRUSH hbrush, INT x, INT y )
{
    INT16 iRgn, iBrush;
    iRgn = MFDRV_CreateRegion( dc, hrgn );
    if(iRgn == -1)
        return FALSE;
    iBrush = MFDRV_CreateBrushIndirect( dc, hbrush );
    if(iBrush == -1)
        return FALSE;
    return MFDRV_MetaParam4( dc, META_FRAMEREGION, iRgn, iBrush, x, y );
}


/**********************************************************************
 *          MFDRV_SetBkColor
 */
COLORREF
MFDRV_SetBkColor( DC *dc, COLORREF color )
{
    return MFDRV_MetaParam2(dc, META_SETBKCOLOR, HIWORD(color), LOWORD(color));
}


/**********************************************************************
 *          MFDRV_SetTextColor
 */
COLORREF
MFDRV_SetTextColor( DC *dc, COLORREF color )
{
    return MFDRV_MetaParam2(dc, META_SETTEXTCOLOR, HIWORD(color),
			    LOWORD(color));
}


/**********************************************************************
 *          MFDRV_PolyBezier
 * Since MetaFiles don't record Beziers and they don't even record
 * approximations to them using lines, we need this stub function.
 */
BOOL
MFDRV_PolyBezier( DC *dc, const POINT *pts, DWORD count )
{
    return FALSE;
}

/**********************************************************************
 *          MFDRV_PolyBezierTo
 * Since MetaFiles don't record Beziers and they don't even record
 * approximations to them using lines, we need this stub function.
 */
BOOL
MFDRV_PolyBezierTo( DC *dc, const POINT *pts, DWORD count )
{
    return FALSE;
}
