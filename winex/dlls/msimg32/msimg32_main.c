/*
 * GDI drawing functions.
 *
 * Copyright 1993, 1994 Alexandre Julliard
 * Copyright 1997 Bertho A. Stultiens
 *           1999 Huw D M Davies
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <math.h>
#include "winbase.h"
#include "wingdi.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msimg32);


/* GradientFill and TransparentBlt are imports from WineHQ's 
   GDI32 DLL GdiGradientFill and GdiTransparentBlt */   
/******************************************************************************
 *           GdiGradientFill   (GDI32.@)
 *
 *  FIXME: we don't support the Alpha channel properly
 */
BOOL WINAPI GradientFill( HDC hdc, TRIVERTEX *vert_array, ULONG nvert,
                          void * grad_array, ULONG ngrad, ULONG mode )
{
  unsigned int i;

  TRACE("vert_array:%p nvert:%d grad_array:%p ngrad:%d\n",
        vert_array, nvert, grad_array, ngrad);

  switch(mode) 
    {
    case GRADIENT_FILL_RECT_H:
      for(i = 0; i < ngrad; i++) 
        {
          GRADIENT_RECT *rect = ((GRADIENT_RECT *)grad_array) + i;
          TRIVERTEX *v1 = vert_array + rect->UpperLeft;
          TRIVERTEX *v2 = vert_array + rect->LowerRight;
          int y1 = v1->y < v2->y ? v1->y : v2->y;
          int y2 = v2->y > v1->y ? v2->y : v1->y;
          int x, dx;
          if (v1->x > v2->x)
            {
              TRIVERTEX *t = v2;
              v2 = v1;
              v1 = t;
            }
          dx = v2->x - v1->x;
          for (x = 0; x < dx; x++)
            {
              POINT pts[2];
              HPEN hPen, hOldPen;
              
              hPen = CreatePen( PS_SOLID, 1, RGB(
                  (v1->Red   * (dx - x) + v2->Red   * x) / dx >> 8,
                  (v1->Green * (dx - x) + v2->Green * x) / dx >> 8,
                  (v1->Blue  * (dx - x) + v2->Blue  * x) / dx >> 8));
              hOldPen = SelectObject( hdc, hPen );
              pts[0].x = v1->x + x;
              pts[0].y = y1;
              pts[1].x = v1->x + x;
              pts[1].y = y2;
              Polyline( hdc, &pts[0], 2 );
              DeleteObject( SelectObject(hdc, hOldPen ) );
            }
        }
      break;
    case GRADIENT_FILL_RECT_V:
      for(i = 0; i < ngrad; i++) 
        {
          GRADIENT_RECT *rect = ((GRADIENT_RECT *)grad_array) + i;
          TRIVERTEX *v1 = vert_array + rect->UpperLeft;
          TRIVERTEX *v2 = vert_array + rect->LowerRight;
          int x1 = v1->x < v2->x ? v1->x : v2->x;
          int x2 = v2->x > v1->x ? v2->x : v1->x;
          int y, dy;
          if (v1->y > v2->y)
            {
              TRIVERTEX *t = v2;
              v2 = v1;
              v1 = t;
            }
          dy = v2->y - v1->y;
          for (y = 0; y < dy; y++)
            {
              POINT pts[2];
              HPEN hPen, hOldPen;
              
              hPen = CreatePen( PS_SOLID, 1, RGB(
                  (v1->Red   * (dy - y) + v2->Red   * y) / dy >> 8,
                  (v1->Green * (dy - y) + v2->Green * y) / dy >> 8,
                  (v1->Blue  * (dy - y) + v2->Blue  * y) / dy >> 8));
              hOldPen = SelectObject( hdc, hPen );
              pts[0].x = x1;
              pts[0].y = v1->y + y;
              pts[1].x = x2;
              pts[1].y = v1->y + y;
              Polyline( hdc, &pts[0], 2 );
              DeleteObject( SelectObject(hdc, hOldPen ) );
            }
        }
      break;
    case GRADIENT_FILL_TRIANGLE:
      for (i = 0; i < ngrad; i++)  
        {
          GRADIENT_TRIANGLE *tri = ((GRADIENT_TRIANGLE *)grad_array) + i;
          TRIVERTEX *v1 = vert_array + tri->Vertex1;
          TRIVERTEX *v2 = vert_array + tri->Vertex2;
          TRIVERTEX *v3 = vert_array + tri->Vertex3;
          int y, dy;
          
          if (v1->y > v2->y)
            { TRIVERTEX *t = v1; v1 = v2; v2 = t; }
          if (v2->y > v3->y)
            {
              TRIVERTEX *t = v2; v2 = v3; v3 = t;
              if (v1->y > v2->y)
                { t = v1; v1 = v2; v2 = t; }
            }
          /* v1->y <= v2->y <= v3->y */

          dy = v3->y - v1->y;
          for (y = 0; y < dy; y++)
            {
              /* v1->y <= y < v3->y */
              TRIVERTEX *v = y < (v2->y - v1->y) ? v1 : v3;
              /* (v->y <= y < v2->y) || (v2->y <= y < v->y) */
              int dy2 = v2->y - v->y;
              int y2 = y + v1->y - v->y;

              int x1 = (v3->x     * y  + v1->x     * (dy  - y )) / dy;
              int x2 = (v2->x     * y2 + v-> x     * (dy2 - y2)) / dy2;
              int r1 = (v3->Red   * y  + v1->Red   * (dy  - y )) / dy;
              int r2 = (v2->Red   * y2 + v-> Red   * (dy2 - y2)) / dy2;
              int g1 = (v3->Green * y  + v1->Green * (dy  - y )) / dy;
              int g2 = (v2->Green * y2 + v-> Green * (dy2 - y2)) / dy2;
              int b1 = (v3->Blue  * y  + v1->Blue  * (dy  - y )) / dy;
              int b2 = (v2->Blue  * y2 + v-> Blue  * (dy2 - y2)) / dy2;
               
              int x;
	      if (x1 < x2)
                {
                  int dx = x2 - x1;
                  for (x = 0; x < dx; x++)
                    SetPixel (hdc, x + x1, y + v1->y, RGB(
                      (r1 * (dx - x) + r2 * x) / dx >> 8,
                      (g1 * (dx - x) + g2 * x) / dx >> 8,
                      (b1 * (dx - x) + b2 * x) / dx >> 8));
                }
              else
                {
                  int dx = x1 - x2;
                  for (x = 0; x < dx; x++)
                    SetPixel (hdc, x + x2, y + v1->y, RGB(
                      (r2 * (dx - x) + r1 * x) / dx >> 8,
                      (g2 * (dx - x) + g1 * x) / dx >> 8,
                      (b2 * (dx - x) + b1 * x) / dx >> 8));
                }
            }
        }
      break;
    default:
      return FALSE;
  }

  return TRUE;
}

/******************************************************************************
 *           GdiTransparentBlt [GDI32.@]
 */
BOOL WINAPI TransparentBlt( HDC hdcDest, int xDest, int yDest, int widthDest, int heightDest,
                            HDC hdcSrc, int xSrc, int ySrc, int widthSrc, int heightSrc,
                            UINT crTransparent )
{
    BOOL ret = FALSE;
    HDC hdcWork;
    HBITMAP bmpWork;
    HGDIOBJ oldWork;
    HDC hdcMask = NULL;
    HBITMAP bmpMask = NULL;
    HBITMAP oldMask = NULL;
    COLORREF oldBackground;
    COLORREF oldForeground;
    int oldStretchMode;

    if(widthDest < 0 || heightDest < 0 || widthSrc < 0 || heightSrc < 0) {
        TRACE("Cannot mirror\n");
        return FALSE;
    }

    oldBackground = SetBkColor(hdcDest, RGB(255,255,255));
    oldForeground = SetTextColor(hdcDest, RGB(0,0,0));

    /* Stretch bitmap */
    oldStretchMode = GetStretchBltMode(hdcSrc);
    if(oldStretchMode == BLACKONWHITE || oldStretchMode == WHITEONBLACK)
        SetStretchBltMode(hdcSrc, COLORONCOLOR);
    hdcWork = CreateCompatibleDC(hdcDest);
    bmpWork = CreateCompatibleBitmap(hdcDest, widthDest, heightDest);
    oldWork = SelectObject(hdcWork, bmpWork);
    if(!StretchBlt(hdcWork, 0, 0, widthDest, heightDest, hdcSrc, xSrc, ySrc, widthSrc, heightSrc, SRCCOPY)) {
        TRACE("Failed to stretch\n");
        goto error;
    }
    SetBkColor(hdcWork, crTransparent);

    /* Create mask */
    hdcMask = CreateCompatibleDC(hdcDest);
    bmpMask = CreateCompatibleBitmap(hdcMask, widthDest, heightDest);
    oldMask = SelectObject(hdcMask, bmpMask);
    if(!BitBlt(hdcMask, 0, 0, widthDest, heightDest, hdcWork, 0, 0, SRCCOPY)) {
        TRACE("Failed to create mask\n");
        goto error;
    }

    /* Replace transparent color with black */
    SetBkColor(hdcWork, RGB(0,0,0));
    SetTextColor(hdcWork, RGB(255,255,255));
    if(!BitBlt(hdcWork, 0, 0, widthDest, heightDest, hdcMask, 0, 0, SRCAND)) {
        TRACE("Failed to mask out background\n");
        goto error;
    }

    /* Replace non-transparent area on destination with black */
    if(!BitBlt(hdcDest, xDest, yDest, widthDest, heightDest, hdcMask, 0, 0, SRCAND)) {
        TRACE("Failed to clear destination area\n");
        goto error;
    }

    /* Draw the image */
    if(!BitBlt(hdcDest, xDest, yDest, widthDest, heightDest, hdcWork, 0, 0, SRCPAINT)) {
        TRACE("Failed to paint image\n");
        goto error;
    }

    ret = TRUE;
error:
    SetStretchBltMode(hdcSrc, oldStretchMode);
    SetBkColor(hdcDest, oldBackground);
    SetTextColor(hdcDest, oldForeground);
    if(hdcWork) {
        SelectObject(hdcWork, oldWork);
        DeleteDC(hdcWork);
    }
    if(bmpWork) DeleteObject(bmpWork);
    if(hdcMask) {
        SelectObject(hdcMask, oldMask);
        DeleteDC(hdcMask);
    }
    if(bmpMask) DeleteObject(bmpMask);
    return ret;
}

BOOL WINAPI AlphaBlend(HDC hdcDest,
                       int nXOriginDest, int nYOriginDest,
                       int nWidthDest, int nHeightDest,
		       HDC hdcSrc,
		       int nXOriginSrc, int nYOriginSrc,
		       int nWidthSrc, int nHeightSrc,
		       BLENDFUNCTION blendFunction)
{
  HDC hdcScratch;
  BITMAPINFO bmiScratch;
  HBITMAP hbmSrc, hbmDest;
  BYTE *src, *dest;
  int x,y,c;
  double scaleX, scaleY;
  
  TRACE("(%p, (%d,%d),(%d,%d), %p, (%d,%d),(%d,%d), (%d,%d,%d,%d)) stub!\n",
        hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest,
	hdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, 
	blendFunction.BlendOp, blendFunction.BlendFlags, 
	blendFunction.SourceConstantAlpha, blendFunction.AlphaFormat);
  
  if ((blendFunction.BlendOp != AC_SRC_OVER) || blendFunction.BlendFlags) {
    FIXME("Invalid parameters\n");
    return FALSE;
  }
  
  /* Check for opaque case that doesn't need alpha blending */
  if ((blendFunction.SourceConstantAlpha == 255) && (blendFunction.AlphaFormat == 0))
  {
    StretchBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest,
               hdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, SRCCOPY);
    return TRUE;
  }
  
  /* create scratch dc and prep the dibsection header */
  /* FIXME: change bmiScratch to a BITMAPV5HEADER to ensure DIB sections have alpha */
  memset(&bmiScratch.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
  bmiScratch.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmiScratch.bmiHeader.biWidth = nWidthSrc;
  bmiScratch.bmiHeader.biHeight = nHeightSrc;
  bmiScratch.bmiHeader.biPlanes = 1;
  bmiScratch.bmiHeader.biBitCount = 32;
  bmiScratch.bmiHeader.biCompression = BI_RGB;
  
  hdcScratch = CreateCompatibleDC(hdcDest);
  
  /* read in src */
  hbmSrc = CreateDIBSection(hdcScratch, &bmiScratch, DIB_RGB_COLORS, (void**)(&src), 0, 0);
  SelectObject(hdcScratch, hbmSrc);

  /* Using BitBlt will preserve the alpha in the original DIB (if there is one), since we do a dib->dib copy */
  BitBlt(hdcScratch, 0, 0, nWidthSrc, nHeightSrc, hdcSrc, nXOriginSrc, nYOriginSrc, SRCCOPY);
  
  /* read in dest */
  bmiScratch.bmiHeader.biWidth = nWidthDest;
  bmiScratch.bmiHeader.biHeight = nHeightDest;
  hbmDest = CreateDIBSection(hdcScratch, &bmiScratch, DIB_RGB_COLORS, (void**)(&dest), 0, 0);
  SelectObject(hdcScratch, hbmDest);
  BitBlt(hdcScratch, 0, 0, nWidthDest, nHeightDest, hdcDest, nXOriginDest, nYOriginDest, SRCCOPY);

  /* Calculate the scaling factor for sampling - note that we're doing a trivial point-sample on 
   * the source data */
  scaleX = (double)nWidthSrc / (double)nWidthDest;
  scaleY =(double)nHeightSrc / (double)nHeightDest;

  /* blend them together in dest */
  if (blendFunction.AlphaFormat == AC_SRC_ALPHA)
  {
    DWORD sca = blendFunction.SourceConstantAlpha;
    if (nWidthSrc == nWidthDest && nHeightSrc == nHeightDest)
    {
      for (y=0; y<nHeightDest; y++) {
        for (x=0; x<nWidthDest; x++) {
          DWORD sa = src[3];
          for (c=0; c<4; c++, src++, dest++)
          {
            DWORD t = ((*src * sca) + ((255 - sa) * *dest))/255;
            *dest = min(t, 255);
          }
        }
      }
    } else {
      for (y=0; y<nHeightDest; y++) {
        for (x=0; x<nWidthDest; x++) {
          DWORD sa = src[((int)floor(y * scaleY) * 4 * nWidthSrc) + ((int)floor(x * scaleX) * 4) + 3];
          for (c=0; c<4; c++)
          {
            DWORD s = src[((int)floor(y * scaleY) * 4 * nWidthSrc) + ((int)floor(x * scaleX) * 4) + c];
            DWORD d = dest[(y * 4 * nWidthDest) + (x * 4) + c];
            DWORD t = ((s * sca) + ((255 - sa) * d))/255;
            dest[(y * 4 * nWidthDest) + (x * 4) + c] = min(t, 255);
          }
        }
      }
    }
  } else {
    DWORD sca = blendFunction.SourceConstantAlpha;
    if (nWidthSrc == nWidthDest && nHeightSrc == nHeightDest)
    {
      for (y=0; y<nHeightDest; y++) {
        for (x=0; x<nWidthDest; x++) {
          for (c=0; c<4; c++, src++, dest++)
            *dest = ((*src * sca) + ((255 - sca) * *dest))/255;
        }
      }
    } else {
      for (y=0; y<nHeightDest; y++) {
        for (x=0; x<nWidthDest; x++) {
          for (c=0; c<4; c++)
          {
            DWORD s = src[((int)floor(y * scaleY) * 4 * nWidthSrc) + ((int)floor(x * scaleX) * 4) + c];
            DWORD d = dest[(y * 4 * nWidthDest) + (x * 4) + c];
            DWORD t = ((s * sca) + ((255 - sca) * d))/255;
            dest[(y * 4 * nWidthDest) + (x * 4) + c] = t;
          }
        }
      }
    }
  }
  
  /* copy to destination dc */
  BitBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, hdcScratch, 0, 0, SRCCOPY);
  
  /* delete objects */
  DeleteObject(hbmDest);
  DeleteObject(hbmSrc);
  DeleteObject(hdcScratch);
  
  return TRUE;
}
