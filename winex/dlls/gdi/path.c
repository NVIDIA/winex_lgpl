/*
 * Graphics paths (BeginPath, EndPath etc.)
 *
 * Copyright 1997, 1998 Martin Boehme
 *                 1999 Huw D M Davies
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#if defined(HAVE_FLOAT_H)
#include <float.h>
#endif

#include "winbase.h"
#include "wingdi.h"
#include "winerror.h"

#include "gdi.h"
#include "wine/debug.h"
#include "path.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);

/* Notes on the implementation
 *
 * The implementation is based on dynamically resizable arrays of points and
 * flags. I dithered for a bit before deciding on this implementation, and
 * I had even done a bit of work on a linked list version before switching
 * to arrays. It's a bit of a tradeoff. When you use linked lists, the
 * implementation of FlattenPath is easier, because you can rip the
 * PT_BEZIERTO entries out of the middle of the list and link the
 * corresponding PT_LINETO entries in. However, when you use arrays,
 * PathToRegion becomes easier, since you can essentially just pass your array
 * of points to CreatePolyPolygonRgn. Also, if I'd used linked lists, I would
 * have had the extra effort of creating a chunk-based allocation scheme
 * in order to use memory effectively. That's why I finally decided to use
 * arrays. Note by the way that the array based implementation has the same
 * linear time complexity that linked lists would have since the arrays grow
 * exponentially.
 *
 * The points are stored in the path in device coordinates. This is
 * consistent with the way Windows does things (for instance, see the Win32
 * SDK documentation for GetPath).
 *
 * The word "stroke" appears in several places (e.g. in the flag
 * GdiPath.newStroke). A stroke consists of a PT_MOVETO followed by one or
 * more PT_LINETOs or PT_BEZIERTOs, up to, but not including, the next
 * PT_MOVETO. Note that this is not the same as the definition of a figure;
 * a figure can contain several strokes.
 *
 * I modified the drawing functions (MoveTo, LineTo etc.) to test whether
 * the path is open and to call the corresponding function in path.c if this
 * is the case. A more elegant approach would be to modify the function
 * pointers in the DC_FUNCTIONS structure; however, this would be a lot more
 * complex. Also, the performance degradation caused by my approach in the
 * case where no path is open is so small that it cannot be measured.
 *
 * Martin Boehme
 */

/* FIXME: A lot of stuff isn't implemented yet. There is much more to come. */

#define NUM_ENTRIES_INITIAL 16  /* Initial size of points / flags arrays  */
#define GROW_FACTOR_NUMER    2  /* Numerator of grow factor for the array */
#define GROW_FACTOR_DENOM    1  /* Denominator of grow factor             */


static BOOL PATH_PathToRegion(GdiPath *pPath, INT nPolyFillMode,
   HRGN *pHrgn);
static void   PATH_EmptyPath(GdiPath *pPath);
static BOOL PATH_ReserveEntries(GdiPath *pPath, INT numEntries);
static BOOL PATH_DoArcPart(GdiPath *pPath, FLOAT_POINT corners[],
   double angleStart, double angleEnd, BOOL addMoveTo);
static void PATH_ScaleNormalizedPoint(FLOAT_POINT corners[], double x,
   double y, POINT *pPoint);
static void PATH_NormalizePoint(FLOAT_POINT corners[], const FLOAT_POINT
   *pPoint, double *pX, double *pY);
static BOOL PATH_CheckCorners(DC *dc, POINT corners[], INT x1, INT y1, INT x2, INT y2);


/***********************************************************************
 *           BeginPath    (GDI.512)
 */
BOOL16 WINAPI BeginPath16(HDC16 hdc)
{
   return (BOOL16)BeginPath((HDC)hdc);
}


/***********************************************************************
 *           BeginPath    (GDI32.@)
 */
BOOL WINAPI BeginPath(HDC hdc)
{
    BOOL ret = TRUE;
    DC *dc = DC_GetDCPtr( hdc );

    if(!dc) return FALSE;

    if(dc->funcs->pBeginPath)
        ret = dc->funcs->pBeginPath(dc);
    else
    {
        /* If path is already open, do nothing */
        if(dc->path.state != PATH_Open)
        {
            /* Make sure that path is empty */
            PATH_EmptyPath(&dc->path);

            /* Initialize variables for new path */
            dc->path.newStroke=TRUE;
            dc->path.state=PATH_Open;
        }
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           EndPath    (GDI.514)
 */
BOOL16 WINAPI EndPath16(HDC16 hdc)
{
   return (BOOL16)EndPath((HDC)hdc);
}


/***********************************************************************
 *           EndPath    (GDI32.@)
 */
BOOL WINAPI EndPath(HDC hdc)
{
    BOOL ret = TRUE;
    DC *dc = DC_GetDCPtr( hdc );

    if(!dc) return FALSE;

    if(dc->funcs->pEndPath)
        ret = dc->funcs->pEndPath(dc);
    else
    {
        /* Check that path is currently being constructed */
        if(dc->path.state!=PATH_Open)
        {
            SetLastError(ERROR_CAN_NOT_COMPLETE);
            ret = FALSE;
        }
        /* Set flag to indicate that path is finished */
        else dc->path.state=PATH_Closed;
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           AbortPath    (GDI.511)
 */
BOOL16 WINAPI AbortPath16(HDC16 hdc)
{
   return (BOOL16)AbortPath((HDC)hdc);
}


/******************************************************************************
 * AbortPath [GDI32.@]
 * Closes and discards paths from device context
 *
 * NOTES
 *    Check that SetLastError is being called correctly
 *
 * PARAMS
 *    hdc [I] Handle to device context
 *
 * RETURNS STD
 */
BOOL WINAPI AbortPath( HDC hdc )
{
    BOOL ret = TRUE;
    DC *dc = DC_GetDCPtr( hdc );

    if(!dc) return FALSE;

    if(dc->funcs->pAbortPath)
        ret = dc->funcs->pAbortPath(dc);
    else /* Remove all entries from the path */
        PATH_EmptyPath( &dc->path );
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           CloseFigure    (GDI.513)
 */
BOOL16 WINAPI CloseFigure16(HDC16 hdc)
{
   return (BOOL16)CloseFigure((HDC)hdc);
}


/***********************************************************************
 *           CloseFigure    (GDI32.@)
 *
 * FIXME: Check that SetLastError is being called correctly
 */
BOOL WINAPI CloseFigure(HDC hdc)
{
    BOOL ret = TRUE;
    DC *dc = DC_GetDCPtr( hdc );

    if(!dc) return FALSE;

    if(dc->funcs->pCloseFigure)
        ret = dc->funcs->pCloseFigure(dc);
    else
    {
        /* Check that path is open */
        if(dc->path.state!=PATH_Open)
        {
            SetLastError(ERROR_CAN_NOT_COMPLETE);
            ret = FALSE;
        }
        else
        {
            /* FIXME: Shouldn't we draw a line to the beginning of the
               figure? */
            /* Set PT_CLOSEFIGURE on the last entry and start a new stroke */
            if(dc->path.numEntriesUsed)
            {
                dc->path.pFlags[dc->path.numEntriesUsed-1]|=PT_CLOSEFIGURE;
                dc->path.newStroke=TRUE;
            }
        }
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           GetPath    (GDI.517)
 */
INT16 WINAPI GetPath16(HDC16 hdc, LPPOINT16 pPoints, LPBYTE pTypes,
   INT16 nSize)
{
   FIXME("(%d,%p,%p): stub\n",hdc,pPoints,pTypes);

   return 0;
}


/***********************************************************************
 *           GetPath    (GDI32.@)
 */
INT WINAPI GetPath(HDC hdc, LPPOINT pPoints, LPBYTE pTypes,
   INT nSize)
{
   INT ret = -1;
   GdiPath *pPath;
   DC *dc = DC_GetDCPtr( hdc );

   if(!dc) return -1;

   pPath = &dc->path;

   /* Check that path is closed */
   if(pPath->state!=PATH_Closed)
   {
      SetLastError(ERROR_CAN_NOT_COMPLETE);
      goto done;
   }

   if(nSize==0)
      ret = pPath->numEntriesUsed;
   else if(nSize<pPath->numEntriesUsed)
   {
      SetLastError(ERROR_INVALID_PARAMETER);
      goto done;
   }
   else
   {
      memcpy(pPoints, pPath->pPoints, sizeof(POINT)*pPath->numEntriesUsed);
      memcpy(pTypes, pPath->pFlags, sizeof(BYTE)*pPath->numEntriesUsed);

      /* Convert the points to logical coordinates */
      if(!DPtoLP(hdc, pPoints, pPath->numEntriesUsed))
      {
	 /* FIXME: Is this the correct value? */
         SetLastError(ERROR_CAN_NOT_COMPLETE);
	goto done;
      }
     else ret = pPath->numEntriesUsed;
   }
 done:
   GDI_ReleaseObj( hdc );
   return ret;
}

/***********************************************************************
 *           PathToRegion    (GDI.518)
 */
HRGN16 WINAPI PathToRegion16(HDC16 hdc)
{
  return (HRGN16) PathToRegion((HDC) hdc);
}

/***********************************************************************
 *           PathToRegion    (GDI32.@)
 *
 * FIXME
 *   Check that SetLastError is being called correctly
 *
 * The documentation does not state this explicitly, but a test under Windows
 * shows that the region which is returned should be in device coordinates.
 */
HRGN WINAPI PathToRegion(HDC hdc)
{
   GdiPath *pPath;
   HRGN  hrgnRval = 0;
   DC *dc = DC_GetDCPtr( hdc );

   /* Get pointer to path */
   if(!dc) return -1;

    pPath = &dc->path;

   /* Check that path is closed */
   if(pPath->state!=PATH_Closed) SetLastError(ERROR_CAN_NOT_COMPLETE);
   else
   {
       /* FIXME: Should we empty the path even if conversion failed? */
       if(PATH_PathToRegion(pPath, GetPolyFillMode(hdc), &hrgnRval))
           PATH_EmptyPath(pPath);
       else
           hrgnRval=0;
   }
   GDI_ReleaseObj( hdc );
   return hrgnRval;
}

static BOOL PATH_FillPath(DC *dc, GdiPath *pPath)
{
   INT   mapMode, graphicsMode;
   SIZE  ptViewportExt, ptWindowExt;
   POINT ptViewportOrg, ptWindowOrg;
   XFORM xform;
   HRGN  hrgn;

   if(dc->funcs->pFillPath)
       return dc->funcs->pFillPath(dc);

   /* Check that path is closed */
   if(pPath->state!=PATH_Closed)
   {
      SetLastError(ERROR_CAN_NOT_COMPLETE);
      return FALSE;
   }

   /* Construct a region from the path and fill it */
   if(PATH_PathToRegion(pPath, dc->polyFillMode, &hrgn))
   {
      /* Since PaintRgn interprets the region as being in logical coordinates
       * but the points we store for the path are already in device
       * coordinates, we have to set the mapping mode to MM_TEXT temporarily.
       * Using SaveDC to save information about the mapping mode / world
       * transform would be easier but would require more overhead, especially
       * now that SaveDC saves the current path.
       */

      /* Save the information about the old mapping mode */
      mapMode=GetMapMode(dc->hSelf);
      GetViewportExtEx(dc->hSelf, &ptViewportExt);
      GetViewportOrgEx(dc->hSelf, &ptViewportOrg);
      GetWindowExtEx(dc->hSelf, &ptWindowExt);
      GetWindowOrgEx(dc->hSelf, &ptWindowOrg);

      /* Save world transform
       * NB: The Windows documentation on world transforms would lead one to
       * believe that this has to be done only in GM_ADVANCED; however, my
       * tests show that resetting the graphics mode to GM_COMPATIBLE does
       * not reset the world transform.
       */
      GetWorldTransform(dc->hSelf, &xform);

      /* Set MM_TEXT */
      SetMapMode(dc->hSelf, MM_TEXT);
      SetViewportOrgEx(dc->hSelf, 0, 0, NULL);
      SetWindowOrgEx(dc->hSelf, 0, 0, NULL);

      /* Paint the region */
      PaintRgn(dc->hSelf, hrgn);
      DeleteObject(hrgn);
      /* Restore the old mapping mode */
      SetMapMode(dc->hSelf, mapMode);
      SetViewportExtEx(dc->hSelf, ptViewportExt.cx, ptViewportExt.cy, NULL);
      SetViewportOrgEx(dc->hSelf, ptViewportOrg.x, ptViewportOrg.y, NULL);
      SetWindowExtEx(dc->hSelf, ptWindowExt.cx, ptWindowExt.cy, NULL);
      SetWindowOrgEx(dc->hSelf, ptWindowOrg.x, ptWindowOrg.y, NULL);

      /* Go to GM_ADVANCED temporarily to restore the world transform */
      graphicsMode=GetGraphicsMode(dc->hSelf);
      SetGraphicsMode(dc->hSelf, GM_ADVANCED);
      SetWorldTransform(dc->hSelf, &xform);
      SetGraphicsMode(dc->hSelf, graphicsMode);
      return TRUE;
   }
   return FALSE;
}

/***********************************************************************
 *           FillPath    (GDI.515)
 */
BOOL16 WINAPI FillPath16(HDC16 hdc)
{
  return (BOOL16) FillPath((HDC) hdc);
}

/***********************************************************************
 *           FillPath    (GDI32.@)
 *
 * FIXME
 *    Check that SetLastError is being called correctly
 */
BOOL WINAPI FillPath(HDC hdc)
{
    DC *dc = DC_GetDCPtr( hdc );
    BOOL bRet = FALSE;

    if(!dc) return FALSE;

    if(dc->funcs->pFillPath)
        bRet = dc->funcs->pFillPath(dc);
    else
    {
        bRet = PATH_FillPath(dc, &dc->path);
        if(bRet)
        {
            /* FIXME: Should the path be emptied even if conversion
               failed? */
            PATH_EmptyPath(&dc->path);
        }
    }
    GDI_ReleaseObj( hdc );
    return bRet;
}

/***********************************************************************
 *           SelectClipPath    (GDI.519)
 */
BOOL16 WINAPI SelectClipPath16(HDC16 hdc, INT16 iMode)
{
  return (BOOL16) SelectClipPath((HDC) hdc, iMode);
}

/***********************************************************************
 *           SelectClipPath    (GDI32.@)
 * FIXME
 *  Check that SetLastError is being called correctly
 */
BOOL WINAPI SelectClipPath(HDC hdc, INT iMode)
{
   GdiPath *pPath;
   HRGN  hrgnPath;
   BOOL  success = FALSE;
   DC *dc = DC_GetDCPtr( hdc );

   if(!dc) return FALSE;

   if(dc->funcs->pSelectClipPath)
     success = dc->funcs->pSelectClipPath(dc, iMode);
   else
   {
       pPath = &dc->path;

       /* Check that path is closed */
       if(pPath->state!=PATH_Closed)
           SetLastError(ERROR_CAN_NOT_COMPLETE);
       /* Construct a region from the path */
       else if(PATH_PathToRegion(pPath, GetPolyFillMode(hdc), &hrgnPath))
       {
           success = ExtSelectClipRgn( hdc, hrgnPath, iMode ) != ERROR;
           DeleteObject(hrgnPath);

           /* Empty the path */
           if(success)
               PATH_EmptyPath(pPath);
           /* FIXME: Should this function delete the path even if it failed? */
       }
   }
   GDI_ReleaseObj( hdc );
   return success;
}


/***********************************************************************
 * Exported functions
 */

/* PATH_InitGdiPath
 *
 * Initializes the GdiPath structure.
 */
void PATH_InitGdiPath(GdiPath *pPath)
{
   assert(pPath!=NULL);

   pPath->state=PATH_Null;
   pPath->pPoints=NULL;
   pPath->pFlags=NULL;
   pPath->numEntriesUsed=0;
   pPath->numEntriesAllocated=0;
}

/* PATH_DestroyGdiPath
 *
 * Destroys a GdiPath structure (frees the memory in the arrays).
 */
void PATH_DestroyGdiPath(GdiPath *pPath)
{
   assert(pPath!=NULL);

   if (pPath->pPoints) HeapFree( GetProcessHeap(), 0, pPath->pPoints );
   if (pPath->pFlags) HeapFree( GetProcessHeap(), 0, pPath->pFlags );
}

/* PATH_AssignGdiPath
 *
 * Copies the GdiPath structure "pPathSrc" to "pPathDest". A deep copy is
 * performed, i.e. the contents of the pPoints and pFlags arrays are copied,
 * not just the pointers. Since this means that the arrays in pPathDest may
 * need to be resized, pPathDest should have been initialized using
 * PATH_InitGdiPath (in C++, this function would be an assignment operator,
 * not a copy constructor).
 * Returns TRUE if successful, else FALSE.
 */
BOOL PATH_AssignGdiPath(GdiPath *pPathDest, const GdiPath *pPathSrc)
{
   assert(pPathDest!=NULL && pPathSrc!=NULL);

   /* Make sure destination arrays are big enough */
   if(!PATH_ReserveEntries(pPathDest, pPathSrc->numEntriesUsed))
      return FALSE;

   /* Perform the copy operation */
   memcpy(pPathDest->pPoints, pPathSrc->pPoints,
      sizeof(POINT)*pPathSrc->numEntriesUsed);
   memcpy(pPathDest->pFlags, pPathSrc->pFlags,
      sizeof(BYTE)*pPathSrc->numEntriesUsed);

   pPathDest->state=pPathSrc->state;
   pPathDest->numEntriesUsed=pPathSrc->numEntriesUsed;
   pPathDest->newStroke=pPathSrc->newStroke;

   return TRUE;
}

/* PATH_MoveTo
 *
 * Should be called when a MoveTo is performed on a DC that has an
 * open path. This starts a new stroke. Returns TRUE if successful, else
 * FALSE.
 */
BOOL PATH_MoveTo(DC *dc)
{
   GdiPath *pPath = &dc->path;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      /* FIXME: Do we have to call SetLastError? */
      return FALSE;

   /* Start a new stroke */
   pPath->newStroke=TRUE;

   return TRUE;
}

/* PATH_LineTo
 *
 * Should be called when a LineTo is performed on a DC that has an
 * open path. This adds a PT_LINETO entry to the path (and possibly
 * a PT_MOVETO entry, if this is the first LineTo in a stroke).
 * Returns TRUE if successful, else FALSE.
 */
BOOL PATH_LineTo(DC *dc, INT x, INT y)
{
   GdiPath *pPath = &dc->path;
   POINT point, pointCurPos;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   /* Convert point to device coordinates */
   point.x=x;
   point.y=y;
   if(!LPtoDP(dc->hSelf, &point, 1))
      return FALSE;

   /* Add a PT_MOVETO if necessary */
   if(pPath->newStroke)
   {
      pPath->newStroke=FALSE;
      pointCurPos.x = dc->CursPosX;
      pointCurPos.y = dc->CursPosY;
      if(!LPtoDP(dc->hSelf, &pointCurPos, 1))
         return FALSE;
      if(!PATH_AddEntry(pPath, &pointCurPos, PT_MOVETO))
         return FALSE;
   }

   /* Add a PT_LINETO entry */
   return PATH_AddEntry(pPath, &point, PT_LINETO);
}

/* PATH_RoundRect
 *
 * Should be called when a call to RoundRect is performed on a DC that has
 * an open path. Returns TRUE if successful, else FALSE.
 *
 * FIXME: it adds the same entries to the path as windows does, but there
 * is an error in the bezier drawing code so that there are small pixel-size
 * gaps when the resulting path is drawn by StrokePath()
 */
BOOL PATH_RoundRect(DC *dc, INT x1, INT y1, INT x2, INT y2, INT ell_width, INT ell_height)
{
   GdiPath *pPath = &dc->path;
   POINT corners[2], pointTemp;
   FLOAT_POINT ellCorners[2];

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   if(!PATH_CheckCorners(dc,corners,x1,y1,x2,y2))
      return FALSE;

   /* Add points to the roundrect path */
   ellCorners[0].x = corners[1].x-ell_width;
   ellCorners[0].y = corners[0].y;
   ellCorners[1].x = corners[1].x;
   ellCorners[1].y = corners[0].y+ell_height;
   if(!PATH_DoArcPart(pPath, ellCorners, 0, -M_PI_2, TRUE))
      return FALSE;
   pointTemp.x = corners[0].x+ell_width/2;
   pointTemp.y = corners[0].y;
   if(!PATH_AddEntry(pPath, &pointTemp, PT_LINETO))
      return FALSE;
   ellCorners[0].x = corners[0].x;
   ellCorners[1].x = corners[0].x+ell_width;
   if(!PATH_DoArcPart(pPath, ellCorners, -M_PI_2, -M_PI, FALSE))
      return FALSE;
   pointTemp.x = corners[0].x;
   pointTemp.y = corners[1].y-ell_height/2;
   if(!PATH_AddEntry(pPath, &pointTemp, PT_LINETO))
      return FALSE;
   ellCorners[0].y = corners[1].y-ell_height;
   ellCorners[1].y = corners[1].y;
   if(!PATH_DoArcPart(pPath, ellCorners, M_PI, M_PI_2, FALSE))
      return FALSE;
   pointTemp.x = corners[1].x-ell_width/2;
   pointTemp.y = corners[1].y;
   if(!PATH_AddEntry(pPath, &pointTemp, PT_LINETO))
      return FALSE;
   ellCorners[0].x = corners[1].x-ell_width;
   ellCorners[1].x = corners[1].x;
   if(!PATH_DoArcPart(pPath, ellCorners, M_PI_2, 0, FALSE))
      return FALSE;

   /* Close the roundrect figure */
   if(!CloseFigure(dc->hSelf))
      return FALSE;

   return TRUE;
}

/* PATH_Rectangle
 *
 * Should be called when a call to Rectangle is performed on a DC that has
 * an open path. Returns TRUE if successful, else FALSE.
 */
BOOL PATH_Rectangle(DC *dc, INT x1, INT y1, INT x2, INT y2)
{
   GdiPath *pPath = &dc->path;
   POINT corners[2], pointTemp;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   if(!PATH_CheckCorners(dc,corners,x1,y1,x2,y2))
      return FALSE;

   /* Close any previous figure */
   if(!CloseFigure(dc->hSelf))
   {
      /* The CloseFigure call shouldn't have failed */
      assert(FALSE);
      return FALSE;
   }

   /* Add four points to the path */
   pointTemp.x=corners[1].x;
   pointTemp.y=corners[0].y;
   if(!PATH_AddEntry(pPath, &pointTemp, PT_MOVETO))
      return FALSE;
   if(!PATH_AddEntry(pPath, corners, PT_LINETO))
      return FALSE;
   pointTemp.x=corners[0].x;
   pointTemp.y=corners[1].y;
   if(!PATH_AddEntry(pPath, &pointTemp, PT_LINETO))
      return FALSE;
   if(!PATH_AddEntry(pPath, corners+1, PT_LINETO))
      return FALSE;

   /* Close the rectangle figure */
   if(!CloseFigure(dc->hSelf))
   {
      /* The CloseFigure call shouldn't have failed */
      assert(FALSE);
      return FALSE;
   }

   return TRUE;
}

/* PATH_Ellipse
 *
 * Should be called when a call to Ellipse is performed on a DC that has
 * an open path. This adds four Bezier splines representing the ellipse
 * to the path. Returns TRUE if successful, else FALSE.
 */
BOOL PATH_Ellipse(DC *dc, INT x1, INT y1, INT x2, INT y2)
{
   return( PATH_Arc(dc, x1, y1, x2, y2, x1, (y1+y2)/2, x1, (y1+y2)/2,0) &&
           CloseFigure(dc->hSelf) );
}

/* PATH_Arc
 *
 * Should be called when a call to Arc is performed on a DC that has
 * an open path. This adds up to five Bezier splines representing the arc
 * to the path. When 'lines' is 1, we add 1 extra line to get a chord,
 * and when 'lines' is 2, we add 2 extra lines to get a pie.
 * Returns TRUE if successful, else FALSE.
 */
BOOL PATH_Arc(DC *dc, INT x1, INT y1, INT x2, INT y2,
   INT xStart, INT yStart, INT xEnd, INT yEnd, INT lines)
{
   GdiPath     *pPath = &dc->path;
   double      angleStart, angleEnd, angleStartQuadrant, angleEndQuadrant=0.0;
               /* Initialize angleEndQuadrant to silence gcc's warning */
   double      x, y;
   FLOAT_POINT corners[2], pointStart, pointEnd;
   POINT       centre;
   BOOL      start, end;
   INT       temp;

   /* FIXME: This function should check for all possible error returns */
   /* FIXME: Do we have to respect newStroke? */

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   /* Check for zero height / width */
   /* FIXME: Only in GM_COMPATIBLE? */
   if(x1==x2 || y1==y2)
      return TRUE;

   /* Convert points to device coordinates */
   corners[0].x=(FLOAT)x1;
   corners[0].y=(FLOAT)y1;
   corners[1].x=(FLOAT)x2;
   corners[1].y=(FLOAT)y2;
   pointStart.x=(FLOAT)xStart;
   pointStart.y=(FLOAT)yStart;
   pointEnd.x=(FLOAT)xEnd;
   pointEnd.y=(FLOAT)yEnd;
   INTERNAL_LPTODP_FLOAT(dc, corners);
   INTERNAL_LPTODP_FLOAT(dc, corners+1);
   INTERNAL_LPTODP_FLOAT(dc, &pointStart);
   INTERNAL_LPTODP_FLOAT(dc, &pointEnd);

   /* Make sure first corner is top left and second corner is bottom right */
   if(corners[0].x>corners[1].x)
   {
      temp=corners[0].x;
      corners[0].x=corners[1].x;
      corners[1].x=temp;
   }
   if(corners[0].y>corners[1].y)
   {
      temp=corners[0].y;
      corners[0].y=corners[1].y;
      corners[1].y=temp;
   }

   /* Compute start and end angle */
   PATH_NormalizePoint(corners, &pointStart, &x, &y);
   angleStart=atan2(y, x);
   PATH_NormalizePoint(corners, &pointEnd, &x, &y);
   angleEnd=atan2(y, x);

   /* Make sure the end angle is "on the right side" of the start angle */
   if(dc->ArcDirection==AD_CLOCKWISE)
   {
      if(angleEnd<=angleStart)
      {
         angleEnd+=2*M_PI;
	 assert(angleEnd>=angleStart);
      }
   }
   else
   {
      if(angleEnd>=angleStart)
      {
         angleEnd-=2*M_PI;
	 assert(angleEnd<=angleStart);
      }
   }

   /* In GM_COMPATIBLE, don't include bottom and right edges */
   if(dc->GraphicsMode==GM_COMPATIBLE)
   {
      corners[1].x--;
      corners[1].y--;
   }

   /* Add the arc to the path with one Bezier spline per quadrant that the
    * arc spans */
   start=TRUE;
   end=FALSE;
   do
   {
      /* Determine the start and end angles for this quadrant */
      if(start)
      {
         angleStartQuadrant=angleStart;
	 if(dc->ArcDirection==AD_CLOCKWISE)
	    angleEndQuadrant=(floor(angleStart/M_PI_2)+1.0)*M_PI_2;
	 else
	    angleEndQuadrant=(ceil(angleStart/M_PI_2)-1.0)*M_PI_2;
      }
      else
      {
	 angleStartQuadrant=angleEndQuadrant;
	 if(dc->ArcDirection==AD_CLOCKWISE)
	    angleEndQuadrant+=M_PI_2;
	 else
	    angleEndQuadrant-=M_PI_2;
      }

      /* Have we reached the last part of the arc? */
      if((dc->ArcDirection==AD_CLOCKWISE &&
         angleEnd<angleEndQuadrant) ||
	 (dc->ArcDirection==AD_COUNTERCLOCKWISE &&
	 angleEnd>angleEndQuadrant))
      {
	 /* Adjust the end angle for this quadrant */
         angleEndQuadrant=angleEnd;
	 end=TRUE;
      }

      /* Add the Bezier spline to the path */
      PATH_DoArcPart(pPath, corners, angleStartQuadrant, angleEndQuadrant,
         start);
      start=FALSE;
   }  while(!end);

   /* chord: close figure. pie: add line and close figure */
   if(lines==1)
   {
      if(!CloseFigure(dc->hSelf))
         return FALSE;
   }
   else if(lines==2)
   {
      centre.x = (corners[0].x+corners[1].x)/2;
      centre.y = (corners[0].y+corners[1].y)/2;
      if(!PATH_AddEntry(pPath, &centre, PT_LINETO | PT_CLOSEFIGURE))
         return FALSE;
   }

   return TRUE;
}

BOOL PATH_PolyBezierTo(DC *dc, const POINT *pts, DWORD cbPoints)
{
   GdiPath     *pPath = &dc->path;
   POINT       pt;
   INT         i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   /* Add a PT_MOVETO if necessary */
   if(pPath->newStroke)
   {
      pPath->newStroke=FALSE;
      pt.x = dc->CursPosX;
      pt.y = dc->CursPosY;
      if(!LPtoDP(dc->hSelf, &pt, 1))
         return FALSE;
      if(!PATH_AddEntry(pPath, &pt, PT_MOVETO))
         return FALSE;
   }

   for(i = 0; i < cbPoints; i++) {
       pt = pts[i];
       if(!LPtoDP(dc->hSelf, &pt, 1))
	   return FALSE;
       PATH_AddEntry(pPath, &pt, PT_BEZIERTO);
   }
   return TRUE;
}

BOOL PATH_PolyBezier(DC *dc, const POINT *pts, DWORD cbPoints)
{
   GdiPath     *pPath = &dc->path;
   POINT       pt;
   INT         i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   for(i = 0; i < cbPoints; i++) {
       pt = pts[i];
       if(!LPtoDP(dc->hSelf, &pt, 1))
	   return FALSE;
       PATH_AddEntry(pPath, &pt, (i == 0) ? PT_MOVETO : PT_BEZIERTO);
   }
   return TRUE;
}

BOOL PATH_Polyline(DC *dc, const POINT *pts, DWORD cbPoints)
{
   GdiPath     *pPath = &dc->path;
   POINT       pt;
   INT         i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   for(i = 0; i < cbPoints; i++) {
       pt = pts[i];
       if(!LPtoDP(dc->hSelf, &pt, 1))
	   return FALSE;
       PATH_AddEntry(pPath, &pt, (i == 0) ? PT_MOVETO : PT_LINETO);
   }
   return TRUE;
}

BOOL PATH_PolylineTo(DC *dc, const POINT *pts, DWORD cbPoints)
{
   GdiPath     *pPath = &dc->path;
   POINT       pt;
   INT         i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   /* Add a PT_MOVETO if necessary */
   if(pPath->newStroke)
   {
      pPath->newStroke=FALSE;
      pt.x = dc->CursPosX;
      pt.y = dc->CursPosY;
      if(!LPtoDP(dc->hSelf, &pt, 1))
         return FALSE;
      if(!PATH_AddEntry(pPath, &pt, PT_MOVETO))
         return FALSE;
   }

   for(i = 0; i < cbPoints; i++) {
       pt = pts[i];
       if(!LPtoDP(dc->hSelf, &pt, 1))
	   return FALSE;
       PATH_AddEntry(pPath, &pt, PT_LINETO);
   }

   return TRUE;
}


BOOL PATH_Polygon(DC *dc, const POINT *pts, DWORD cbPoints)
{
   GdiPath     *pPath = &dc->path;
   POINT       pt;
   INT         i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   for(i = 0; i < cbPoints; i++) {
       pt = pts[i];
       if(!LPtoDP(dc->hSelf, &pt, 1))
	   return FALSE;
       PATH_AddEntry(pPath, &pt, (i == 0) ? PT_MOVETO :
		     ((i == cbPoints-1) ? PT_LINETO | PT_CLOSEFIGURE :
		      PT_LINETO));
   }
   return TRUE;
}

BOOL PATH_PolyPolygon( DC *dc, const POINT* pts, const INT* counts,
		       UINT polygons )
{
   GdiPath     *pPath = &dc->path;
   POINT       pt, startpt;
   INT         poly, point, i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   for(i = 0, poly = 0; poly < polygons; poly++) {
       for(point = 0; point < counts[poly]; point++, i++) {
	   pt = pts[i];
	   if(!LPtoDP(dc->hSelf, &pt, 1))
	       return FALSE;
	   if(point == 0) startpt = pt;
	   PATH_AddEntry(pPath, &pt, (point == 0) ? PT_MOVETO : PT_LINETO);
       }
       /* win98 adds an extra line to close the figure for some reason */
       PATH_AddEntry(pPath, &startpt, PT_LINETO | PT_CLOSEFIGURE);
   }
   return TRUE;
}

BOOL PATH_PolyPolyline( DC *dc, const POINT* pts, const DWORD* counts,
			DWORD polylines )
{
   GdiPath     *pPath = &dc->path;
   POINT       pt;
   INT         poly, point, i;

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   for(i = 0, poly = 0; poly < polylines; poly++) {
       for(point = 0; point < counts[poly]; point++, i++) {
	   pt = pts[i];
	   if(!LPtoDP(dc->hSelf, &pt, 1))
	       return FALSE;
	   PATH_AddEntry(pPath, &pt, (point == 0) ? PT_MOVETO : PT_LINETO);
       }
   }
   return TRUE;
}

/***********************************************************************
 * Internal functions
 */

/* PATH_CheckCorners
 *
 * Helper function for PATH_RoundRect() and PATH_Rectangle()
 */
static BOOL PATH_CheckCorners(DC *dc, POINT corners[], INT x1, INT y1, INT x2, INT y2)
{
   INT temp;

   /* Convert points to device coordinates */
   corners[0].x=x1;
   corners[0].y=y1;
   corners[1].x=x2;
   corners[1].y=y2;
   if(!LPtoDP(dc->hSelf, corners, 2))
      return FALSE;

   /* Make sure first corner is top left and second corner is bottom right */
   if(corners[0].x>corners[1].x)
   {
      temp=corners[0].x;
      corners[0].x=corners[1].x;
      corners[1].x=temp;
   }
   if(corners[0].y>corners[1].y)
   {
      temp=corners[0].y;
      corners[0].y=corners[1].y;
      corners[1].y=temp;
   }

   /* In GM_COMPATIBLE, don't include bottom and right edges */
   if(dc->GraphicsMode==GM_COMPATIBLE)
   {
      corners[1].x--;
      corners[1].y--;
   }

   return TRUE;
}

/* PATH_AddFlatBezier
 */
static BOOL PATH_AddFlatBezier(GdiPath *pPath, POINT *pt, BOOL closed)
{
    POINT *pts;
    INT no, i;

    pts = GDI_Bezier( pt, 4, &no );
    if(!pts) return FALSE;

    for(i = 1; i < no; i++)
        PATH_AddEntry(pPath, &pts[i],
	    (i == no-1 && closed) ? PT_LINETO | PT_CLOSEFIGURE : PT_LINETO);
    HeapFree( GetProcessHeap(), 0, pts );
    return TRUE;
}

/* PATH_FlattenPath
 *
 * Replaces Beziers with line segments
 *
 */
static BOOL PATH_FlattenPath(GdiPath *pPath)
{
    GdiPath newPath;
    INT srcpt;

    memset(&newPath, 0, sizeof(newPath));
    newPath.state = PATH_Open;
    for(srcpt = 0; srcpt < pPath->numEntriesUsed; srcpt++) {
        switch(pPath->pFlags[srcpt] & ~PT_CLOSEFIGURE) {
	case PT_MOVETO:
	case PT_LINETO:
	    PATH_AddEntry(&newPath, &pPath->pPoints[srcpt],
			  pPath->pFlags[srcpt]);
	    break;
	case PT_BEZIERTO:
	  PATH_AddFlatBezier(&newPath, &pPath->pPoints[srcpt-1],
			     pPath->pFlags[srcpt+2] & PT_CLOSEFIGURE);
	    srcpt += 2;
	    break;
	}
    }
    newPath.state = PATH_Closed;
    PATH_AssignGdiPath(pPath, &newPath);
    PATH_EmptyPath(&newPath);
    return TRUE;
}

/* PATH_PathToRegion
 *
 * Creates a region from the specified path using the specified polygon
 * filling mode. The path is left unchanged. A handle to the region that
 * was created is stored in *pHrgn. If successful, TRUE is returned; if an
 * error occurs, SetLastError is called with the appropriate value and
 * FALSE is returned.
 */
static BOOL PATH_PathToRegion(GdiPath *pPath, INT nPolyFillMode,
   HRGN *pHrgn)
{
   int    numStrokes, iStroke, i;
   INT  *pNumPointsInStroke;
   HRGN hrgn;

   assert(pPath!=NULL);
   assert(pHrgn!=NULL);

   PATH_FlattenPath(pPath);

   /* FIXME: What happens when number of points is zero? */

   /* First pass: Find out how many strokes there are in the path */
   /* FIXME: We could eliminate this with some bookkeeping in GdiPath */
   numStrokes=0;
   for(i=0; i<pPath->numEntriesUsed; i++)
      if((pPath->pFlags[i] & ~PT_CLOSEFIGURE) == PT_MOVETO)
         numStrokes++;

   /* Allocate memory for number-of-points-in-stroke array */
   pNumPointsInStroke=(int *)HeapAlloc( GetProcessHeap(), 0,
					sizeof(int) * numStrokes );
   if(!pNumPointsInStroke)
   {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
   }

   /* Second pass: remember number of points in each polygon */
   iStroke=-1;  /* Will get incremented to 0 at beginning of first stroke */
   for(i=0; i<pPath->numEntriesUsed; i++)
   {
      /* Is this the beginning of a new stroke? */
      if((pPath->pFlags[i] & ~PT_CLOSEFIGURE) == PT_MOVETO)
      {
         iStroke++;
	 pNumPointsInStroke[iStroke]=0;
      }

      pNumPointsInStroke[iStroke]++;
   }

   /* Create a region from the strokes */
   hrgn=CreatePolyPolygonRgn(pPath->pPoints, pNumPointsInStroke,
      numStrokes, nPolyFillMode);
   if(hrgn==(HRGN)0)
   {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
   }

   /* Free memory for number-of-points-in-stroke array */
   HeapFree( GetProcessHeap(), 0, pNumPointsInStroke );

   /* Success! */
   *pHrgn=hrgn;
   return TRUE;
}

/* PATH_EmptyPath
 *
 * Removes all entries from the path and sets the path state to PATH_Null.
 */
static void PATH_EmptyPath(GdiPath *pPath)
{
   assert(pPath!=NULL);

   pPath->state=PATH_Null;
   pPath->numEntriesUsed=0;
}

/* PATH_AddEntry
 *
 * Adds an entry to the path. For "flags", pass either PT_MOVETO, PT_LINETO
 * or PT_BEZIERTO, optionally ORed with PT_CLOSEFIGURE. Returns TRUE if
 * successful, FALSE otherwise (e.g. if not enough memory was available).
 */
BOOL PATH_AddEntry(GdiPath *pPath, const POINT *pPoint, BYTE flags)
{
   assert(pPath!=NULL);

   /* FIXME: If newStroke is true, perhaps we want to check that we're
    * getting a PT_MOVETO
    */
   TRACE("(%ld,%ld) - %d\n", pPoint->x, pPoint->y, flags);

   /* Check that path is open */
   if(pPath->state!=PATH_Open)
      return FALSE;

   /* Reserve enough memory for an extra path entry */
   if(!PATH_ReserveEntries(pPath, pPath->numEntriesUsed+1))
      return FALSE;

   /* Store information in path entry */
   pPath->pPoints[pPath->numEntriesUsed]=*pPoint;
   pPath->pFlags[pPath->numEntriesUsed]=flags;

   /* If this is PT_CLOSEFIGURE, we have to start a new stroke next time */
   if((flags & PT_CLOSEFIGURE) == PT_CLOSEFIGURE)
      pPath->newStroke=TRUE;

   /* Increment entry count */
   pPath->numEntriesUsed++;

   return TRUE;
}

/* PATH_ReserveEntries
 *
 * Ensures that at least "numEntries" entries (for points and flags) have
 * been allocated; allocates larger arrays and copies the existing entries
 * to those arrays, if necessary. Returns TRUE if successful, else FALSE.
 */
static BOOL PATH_ReserveEntries(GdiPath *pPath, INT numEntries)
{
   INT   numEntriesToAllocate;
   POINT *pPointsNew;
   BYTE    *pFlagsNew;

   assert(pPath!=NULL);
   assert(numEntries>=0);

   /* Do we have to allocate more memory? */
   if(numEntries > pPath->numEntriesAllocated)
   {
      /* Find number of entries to allocate. We let the size of the array
       * grow exponentially, since that will guarantee linear time
       * complexity. */
      if(pPath->numEntriesAllocated)
      {
	 numEntriesToAllocate=pPath->numEntriesAllocated;
	 while(numEntriesToAllocate<numEntries)
	    numEntriesToAllocate=numEntriesToAllocate*GROW_FACTOR_NUMER/
	       GROW_FACTOR_DENOM;
      }
      else
         numEntriesToAllocate=numEntries;

      /* Allocate new arrays */
      pPointsNew=(POINT *)HeapAlloc( GetProcessHeap(), 0,
				     numEntriesToAllocate * sizeof(POINT) );
      if(!pPointsNew)
         return FALSE;
      pFlagsNew=(BYTE *)HeapAlloc( GetProcessHeap(), 0,
				   numEntriesToAllocate * sizeof(BYTE) );
      if(!pFlagsNew)
      {
         HeapFree( GetProcessHeap(), 0, pPointsNew );
	 return FALSE;
      }

      /* Copy old arrays to new arrays and discard old arrays */
      if(pPath->pPoints)
      {
         assert(pPath->pFlags);

	 memcpy(pPointsNew, pPath->pPoints,
	     sizeof(POINT)*pPath->numEntriesUsed);
	 memcpy(pFlagsNew, pPath->pFlags,
	     sizeof(BYTE)*pPath->numEntriesUsed);

	 HeapFree( GetProcessHeap(), 0, pPath->pPoints );
	 HeapFree( GetProcessHeap(), 0, pPath->pFlags );
      }
      pPath->pPoints=pPointsNew;
      pPath->pFlags=pFlagsNew;
      pPath->numEntriesAllocated=numEntriesToAllocate;
   }

   return TRUE;
}

/* PATH_DoArcPart
 *
 * Creates a Bezier spline that corresponds to part of an arc and appends the
 * corresponding points to the path. The start and end angles are passed in
 * "angleStart" and "angleEnd"; these angles should span a quarter circle
 * at most. If "addMoveTo" is true, a PT_MOVETO entry for the first control
 * point is added to the path; otherwise, it is assumed that the current
 * position is equal to the first control point.
 */
static BOOL PATH_DoArcPart(GdiPath *pPath, FLOAT_POINT corners[],
   double angleStart, double angleEnd, BOOL addMoveTo)
{
   double  halfAngle, a;
   double  xNorm[4], yNorm[4];
   POINT point;
   int     i;

   assert(fabs(angleEnd-angleStart)<=M_PI_2);

   /* FIXME: Is there an easier way of computing this? */

   /* Compute control points */
   halfAngle=(angleEnd-angleStart)/2.0;
   if(fabs(halfAngle)>1e-8)
   {
      a=4.0/3.0*(1-cos(halfAngle))/sin(halfAngle);
      xNorm[0]=cos(angleStart);
      yNorm[0]=sin(angleStart);
      xNorm[1]=xNorm[0] - a*yNorm[0];
      yNorm[1]=yNorm[0] + a*xNorm[0];
      xNorm[3]=cos(angleEnd);
      yNorm[3]=sin(angleEnd);
      xNorm[2]=xNorm[3] + a*yNorm[3];
      yNorm[2]=yNorm[3] - a*xNorm[3];
   }
   else
      for(i=0; i<4; i++)
      {
	 xNorm[i]=cos(angleStart);
	 yNorm[i]=sin(angleStart);
      }

   /* Add starting point to path if desired */
   if(addMoveTo)
   {
      PATH_ScaleNormalizedPoint(corners, xNorm[0], yNorm[0], &point);
      if(!PATH_AddEntry(pPath, &point, PT_MOVETO))
         return FALSE;
   }

   /* Add remaining control points */
   for(i=1; i<4; i++)
   {
      PATH_ScaleNormalizedPoint(corners, xNorm[i], yNorm[i], &point);
      if(!PATH_AddEntry(pPath, &point, PT_BEZIERTO))
         return FALSE;
   }

   return TRUE;
}

/* PATH_ScaleNormalizedPoint
 *
 * Scales a normalized point (x, y) with respect to the box whose corners are
 * passed in "corners". The point is stored in "*pPoint". The normalized
 * coordinates (-1.0, -1.0) correspond to corners[0], the coordinates
 * (1.0, 1.0) correspond to corners[1].
 */
static void PATH_ScaleNormalizedPoint(FLOAT_POINT corners[], double x,
   double y, POINT *pPoint)
{
   pPoint->x=GDI_ROUND( (double)corners[0].x +
      (double)(corners[1].x-corners[0].x)*0.5*(x+1.0) );
   pPoint->y=GDI_ROUND( (double)corners[0].y +
      (double)(corners[1].y-corners[0].y)*0.5*(y+1.0) );
}

/* PATH_NormalizePoint
 *
 * Normalizes a point with respect to the box whose corners are passed in
 * "corners". The normalized coordinates are stored in "*pX" and "*pY".
 */
static void PATH_NormalizePoint(FLOAT_POINT corners[],
   const FLOAT_POINT *pPoint,
   double *pX, double *pY)
{
   *pX=(double)(pPoint->x-corners[0].x)/(double)(corners[1].x-corners[0].x) *
      2.0 - 1.0;
   *pY=(double)(pPoint->y-corners[0].y)/(double)(corners[1].y-corners[0].y) *
      2.0 - 1.0;
}

/*******************************************************************
 *      FlattenPath [GDI.516]
 *
 *
 */
BOOL16 WINAPI FlattenPath16(HDC16 hdc)
{
  return (BOOL16) FlattenPath((HDC) hdc);
}

/*******************************************************************
 *      FlattenPath [GDI32.@]
 *
 *
 */
BOOL WINAPI FlattenPath(HDC hdc)
{
    BOOL ret = FALSE;
    DC *dc = DC_GetDCPtr( hdc );

    if(!dc) return FALSE;

    if(dc->funcs->pFlattenPath) ret = dc->funcs->pFlattenPath(dc);
    else
    {
	GdiPath *pPath = &dc->path;
        if(pPath->state != PATH_Closed)
	    ret = PATH_FlattenPath(pPath);
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


static BOOL PATH_StrokePath(DC *dc, GdiPath *pPath)
{
    INT i;
    POINT ptLastMove = {0,0};
    POINT ptViewportOrg, ptWindowOrg;
    SIZE szViewportExt, szWindowExt;
    DWORD mapMode, graphicsMode;
    XFORM xform;
    BOOL ret = TRUE;

    if(dc->funcs->pStrokePath)
        return dc->funcs->pStrokePath(dc);

    if(pPath->state != PATH_Closed)
        return FALSE;

    /* Save the mapping mode info */
    mapMode=GetMapMode(dc->hSelf);
    GetViewportExtEx(dc->hSelf, &szViewportExt);
    GetViewportOrgEx(dc->hSelf, &ptViewportOrg);
    GetWindowExtEx(dc->hSelf, &szWindowExt);
    GetWindowOrgEx(dc->hSelf, &ptWindowOrg);
    GetWorldTransform(dc->hSelf, &xform);

    /* Set MM_TEXT */
    SetMapMode(dc->hSelf, MM_TEXT);
    SetViewportOrgEx(dc->hSelf, 0, 0, NULL);
    SetWindowOrgEx(dc->hSelf, 0, 0, NULL);


    for(i = 0; i < pPath->numEntriesUsed; i++) {
        switch(pPath->pFlags[i]) {
	case PT_MOVETO:
	    TRACE("Got PT_MOVETO (%ld, %ld)\n",
		  pPath->pPoints[i].x, pPath->pPoints[i].y);
	    MoveToEx(dc->hSelf, pPath->pPoints[i].x, pPath->pPoints[i].y, NULL);
	    ptLastMove = pPath->pPoints[i];
	    break;
	case PT_LINETO:
	case (PT_LINETO | PT_CLOSEFIGURE):
	    TRACE("Got PT_LINETO (%ld, %ld)\n",
		  pPath->pPoints[i].x, pPath->pPoints[i].y);
	    LineTo(dc->hSelf, pPath->pPoints[i].x, pPath->pPoints[i].y);
	    break;
	case PT_BEZIERTO:
	    TRACE("Got PT_BEZIERTO\n");
	    if(pPath->pFlags[i+1] != PT_BEZIERTO ||
	       (pPath->pFlags[i+2] & ~PT_CLOSEFIGURE) != PT_BEZIERTO) {
	        ERR("Path didn't contain 3 successive PT_BEZIERTOs\n");
		ret = FALSE;
		goto end;
	    }
	    PolyBezierTo(dc->hSelf, &pPath->pPoints[i], 3);
	    i += 2;
	    break;
	default:
	    ERR("Got path flag %d\n", (INT)pPath->pFlags[i]);
	    ret = FALSE;
	    goto end;
	}
	if(pPath->pFlags[i] & PT_CLOSEFIGURE)
	    LineTo(dc->hSelf, ptLastMove.x, ptLastMove.y);
    }

 end:

    /* Restore the old mapping mode */
    SetMapMode(dc->hSelf, mapMode);
    SetViewportExtEx(dc->hSelf, szViewportExt.cx, szViewportExt.cy, NULL);
    SetViewportOrgEx(dc->hSelf, ptViewportOrg.x, ptViewportOrg.y, NULL);
    SetWindowExtEx(dc->hSelf, szWindowExt.cx, szWindowExt.cy, NULL);
    SetWindowOrgEx(dc->hSelf, ptWindowOrg.x, ptWindowOrg.y, NULL);

    /* Go to GM_ADVANCED temporarily to restore the world transform */
    graphicsMode=GetGraphicsMode(dc->hSelf);
    SetGraphicsMode(dc->hSelf, GM_ADVANCED);
    SetWorldTransform(dc->hSelf, &xform);
    SetGraphicsMode(dc->hSelf, graphicsMode);
    return ret;
}


/*******************************************************************
 *      StrokeAndFillPath [GDI.520]
 *
 *
 */
BOOL16 WINAPI StrokeAndFillPath16(HDC16 hdc)
{
  return (BOOL16) StrokeAndFillPath((HDC) hdc);
}

/*******************************************************************
 *      StrokeAndFillPath [GDI32.@]
 *
 *
 */
BOOL WINAPI StrokeAndFillPath(HDC hdc)
{
   DC *dc = DC_GetDCPtr( hdc );
   BOOL bRet = FALSE;

   if(!dc) return FALSE;

   if(dc->funcs->pStrokeAndFillPath)
       bRet = dc->funcs->pStrokeAndFillPath(dc);
   else
   {
       bRet = PATH_FillPath(dc, &dc->path);
       if(bRet) bRet = PATH_StrokePath(dc, &dc->path);
       if(bRet) PATH_EmptyPath(&dc->path);
   }
   GDI_ReleaseObj( hdc );
   return bRet;
}

/*******************************************************************
 *      StrokePath [GDI.521]
 *
 *
 */
BOOL16 WINAPI StrokePath16(HDC16 hdc)
{
  return (BOOL16) StrokePath((HDC) hdc);
}

/*******************************************************************
 *      StrokePath [GDI32.@]
 *
 *
 */
BOOL WINAPI StrokePath(HDC hdc)
{
    DC *dc = DC_GetDCPtr( hdc );
    GdiPath *pPath;
    BOOL bRet = FALSE;

    TRACE("(%08x)\n", hdc);
    if(!dc) return FALSE;

    if(dc->funcs->pStrokePath)
        bRet = dc->funcs->pStrokePath(dc);
    else
    {
        pPath = &dc->path;
        bRet = PATH_StrokePath(dc, pPath);
        PATH_EmptyPath(pPath);
    }
    GDI_ReleaseObj( hdc );
    return bRet;
}

/*******************************************************************
 *      WidenPath [GDI.522]
 *
 *
 */
BOOL16 WINAPI WidenPath16(HDC16 hdc)
{
  return (BOOL16) WidenPath((HDC) hdc);
}

/*******************************************************************
 *      WidenPath [GDI32.@]
 *
 *
 */
BOOL WINAPI WidenPath(HDC hdc)
{
   DC *dc = DC_GetDCPtr( hdc );
   BOOL ret = FALSE;

   if(!dc) return FALSE;

   if(dc->funcs->pWidenPath)
     ret = dc->funcs->pWidenPath(dc);

   FIXME("stub\n");
   GDI_ReleaseObj( hdc );
   return ret;
}
