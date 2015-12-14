/*
 * Copyright (C) 2007 Google (Evan Stade)
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

#include <stdarg.h>
#include <math.h>
#include <limits.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"
#include "wine/unicode.h"

#define COBJMACROS
#include "objbase.h"
#include "ocidl.h"
#include "olectl.h"
#include "ole2.h"

#include "winreg.h"
#include "shlwapi.h"

#include "gdiplus.h"
#include "gdiplus_private.h"
#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdiplus);

/* looks-right constants */
#define ANCHOR_WIDTH (2.0)
#define MAX_ITERS (50)

/* Converts angle (in degrees) to x/y coordinates */
static void deg2xy(REAL angle, REAL x_0, REAL y_0, REAL *x, REAL *y)
{
    REAL radAngle, hypotenuse;

    radAngle = deg2rad(angle);
    hypotenuse = 50.0; /* arbitrary */

    *x = x_0 + cos(radAngle) * hypotenuse;
    *y = y_0 + sin(radAngle) * hypotenuse;
}

/* Converts from gdiplus path point type to gdi path point type. */
static BYTE convert_path_point_type(BYTE type)
{
    BYTE ret;

    switch(type & PathPointTypePathTypeMask){
        case PathPointTypeBezier:
            ret = PT_BEZIERTO;
            break;
        case PathPointTypeLine:
            ret = PT_LINETO;
            break;
        case PathPointTypeStart:
            ret = PT_MOVETO;
            break;
        default:
            ERR("Bad point type\n");
            return 0;
    }

    if(type & PathPointTypeCloseSubpath)
        ret |= PT_CLOSEFIGURE;

    return ret;
}

static INT prepare_dc(GpGraphics *graphics, GpPen *pen)
{
    HPEN gdipen;
    REAL width;
    INT save_state = SaveDC(graphics->hdc), i, numdashes;
    GpPointF pt[2];
    DWORD dash_array[MAX_DASHLEN];

    EndPath(graphics->hdc);

    if(pen->unit == UnitPixel){
        width = pen->width;
    }
    else{
        /* Get an estimate for the amount the pen width is affected by the world
         * transform. (This is similar to what some of the wine drivers do.) */
        pt[0].X = 0.0;
        pt[0].Y = 0.0;
        pt[1].X = 1.0;
        pt[1].Y = 1.0;
        GdipTransformMatrixPoints(graphics->worldtrans, pt, 2);
        width = sqrt((pt[1].X - pt[0].X) * (pt[1].X - pt[0].X) +
                     (pt[1].Y - pt[0].Y) * (pt[1].Y - pt[0].Y)) / sqrt(2.0);

        width *= pen->width * convert_unit(graphics->hdc,
                              pen->unit == UnitWorld ? graphics->unit : pen->unit);
    }

    if(pen->dash == DashStyleCustom){
        numdashes = min(pen->numdashes, MAX_DASHLEN);

        TRACE("dashes are: ");
        for(i = 0; i < numdashes; i++){
            dash_array[i] = roundr(width * pen->dashes[i]);
            TRACE("%d, ", dash_array[i]);
        }
        TRACE("\n and the pen style is %x\n", pen->style);

        gdipen = ExtCreatePen(pen->style, roundr(width), &pen->brush->lb,
                              numdashes, dash_array);
    }
    else
        gdipen = ExtCreatePen(pen->style, roundr(width), &pen->brush->lb, 0, NULL);

    SelectObject(graphics->hdc, gdipen);

    return save_state;
}

static void restore_dc(GpGraphics *graphics, INT state)
{
    DeleteObject(SelectObject(graphics->hdc, GetStockObject(NULL_PEN)));
    RestoreDC(graphics->hdc, state);
}

/* This helper applies all the changes that the points listed in ptf need in
 * order to be drawn on the device context.  In the end, this should include at
 * least:
 *  -scaling by page unit
 *  -applying world transformation
 *  -converting from float to int
 * Native gdiplus uses gdi32 to do all this (via SetMapMode, SetViewportExtEx,
 * SetWindowExtEx, SetWorldTransform, etc.) but we cannot because we are using
 * gdi to draw, and these functions would irreparably mess with line widths.
 */
static void transform_and_round_points(GpGraphics *graphics, POINT *pti,
    GpPointF *ptf, INT count)
{
    REAL unitscale;
    GpMatrix *matrix;
    int i;

    unitscale = convert_unit(graphics->hdc, graphics->unit);

    /* apply page scale */
    if(graphics->unit != UnitDisplay)
        unitscale *= graphics->scale;

    GdipCloneMatrix(graphics->worldtrans, &matrix);
    GdipScaleMatrix(matrix, unitscale, unitscale, MatrixOrderAppend);
    GdipTransformMatrixPoints(matrix, ptf, count);
    GdipDeleteMatrix(matrix);

    for(i = 0; i < count; i++){
        pti[i].x = roundr(ptf[i].X);
        pti[i].y = roundr(ptf[i].Y);
    }
}

/* Draw non-premultiplied ARGB data to the given graphics object */
static GpStatus alpha_blend_pixels(GpGraphics *graphics, INT dst_x, INT dst_y,
    const BYTE *src, INT src_width, INT src_height, INT src_stride)
{
    if (graphics->image && graphics->image->type == ImageTypeBitmap)
    {
        GpBitmap *dst_bitmap = (GpBitmap*)graphics->image;
        INT x, y;

        for (x=0; x<src_width; x++)
        {
            for (y=0; y<src_height; y++)
            {
                ARGB dst_color, src_color;
                GdipBitmapGetPixel(dst_bitmap, x+dst_x, y+dst_y, &dst_color);
                src_color = ((ARGB*)(src + src_stride * y))[x];
                GdipBitmapSetPixel(dst_bitmap, x+dst_x, y+dst_y, color_over(dst_color, src_color));
            }
        }

        return Ok;
    }
    else
    {
        HDC hdc;
        HBITMAP hbitmap, old_hbm=NULL;
        BITMAPINFOHEADER bih;
        BYTE *temp_bits;
        BLENDFUNCTION bf;

        hdc = CreateCompatibleDC(0);

        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = src_width;
        bih.biHeight = -src_height;
        bih.biPlanes = 1;
        bih.biBitCount = 32;
        bih.biCompression = BI_RGB;
        bih.biSizeImage = 0;
        bih.biXPelsPerMeter = 0;
        bih.biYPelsPerMeter = 0;
        bih.biClrUsed = 0;
        bih.biClrImportant = 0;

        hbitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bih, DIB_RGB_COLORS,
            (void**)&temp_bits, NULL, 0);

        convert_32bppARGB_to_32bppPARGB(src_width, src_height, temp_bits,
            4 * src_width, src, src_stride);

        old_hbm = SelectObject(hdc, hbitmap);

        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;

        GdiAlphaBlend(graphics->hdc, dst_x, dst_y, src_width, src_height,
            hdc, 0, 0, src_width, src_height, bf);

        SelectObject(hdc, old_hbm);
        DeleteDC(hdc);
        DeleteObject(hbitmap);

        return Ok;
    }
}

static ARGB blend_colors(ARGB start, ARGB end, REAL position)
{
    ARGB result=0;
    ARGB i;
    for (i=0xff; i<=0xff0000; i = i << 8)
        result |= (int)((start&i)*(1.0f - position)+(end&i)*(position))&i;
    return result;
}

static ARGB blend_line_gradient(GpLineGradient* brush, REAL position)
{
    REAL blendfac;

    /* clamp to between 0.0 and 1.0, using the wrap mode */
    if (brush->wrap == WrapModeTile)
    {
        position = fmodf(position, 1.0f);
        if (position < 0.0f) position += 1.0f;
    }
    else /* WrapModeFlip* */
    {
        position = fmodf(position, 2.0f);
        if (position < 0.0f) position += 2.0f;
        if (position > 1.0f) position = 2.0f - position;
    }

    if (brush->blendcount == 1)
        blendfac = position;
    else
    {
        int i=1;
        REAL left_blendpos, left_blendfac, right_blendpos, right_blendfac;
        REAL range;

        /* locate the blend positions surrounding this position */
        while (position > brush->blendpos[i])
            i++;

        /* interpolate between the blend positions */
        left_blendpos = brush->blendpos[i-1];
        left_blendfac = brush->blendfac[i-1];
        right_blendpos = brush->blendpos[i];
        right_blendfac = brush->blendfac[i];
        range = right_blendpos - left_blendpos;
        blendfac = (left_blendfac * (right_blendpos - position) +
                    right_blendfac * (position - left_blendpos)) / range;
    }

    if (brush->pblendcount == 0)
        return blend_colors(brush->startcolor, brush->endcolor, blendfac);
    else
    {
        int i=1;
        ARGB left_blendcolor, right_blendcolor;
        REAL left_blendpos, right_blendpos;

        /* locate the blend colors surrounding this position */
        while (blendfac > brush->pblendpos[i])
            i++;

        /* interpolate between the blend colors */
        left_blendpos = brush->pblendpos[i-1];
        left_blendcolor = brush->pblendcolor[i-1];
        right_blendpos = brush->pblendpos[i];
        right_blendcolor = brush->pblendcolor[i];
        blendfac = (blendfac - left_blendpos) / (right_blendpos - left_blendpos);
        return blend_colors(left_blendcolor, right_blendcolor, blendfac);
    }
}

static void brush_fill_path(GpGraphics *graphics, GpBrush* brush)
{
    switch (brush->bt)
    {
    case BrushTypeLinearGradient:
    {
        GpLineGradient *line = (GpLineGradient*)brush;
        RECT rc;

        SelectClipPath(graphics->hdc, RGN_AND);
        if (GetClipBox(graphics->hdc, &rc) != NULLREGION)
        {
            GpPointF endpointsf[2];
            POINT endpointsi[2];
            POINT poly[4];

            SelectObject(graphics->hdc, GetStockObject(NULL_PEN));

            endpointsf[0] = line->startpoint;
            endpointsf[1] = line->endpoint;
            transform_and_round_points(graphics, endpointsi, endpointsf, 2);

            if (abs(endpointsi[0].x-endpointsi[1].x) > abs(endpointsi[0].y-endpointsi[1].y))
            {
                /* vertical-ish gradient */
                int startx, endx; /* x co-ordinates of endpoints shifted to intersect the top of the visible rectangle */
                int startbottomx; /* x co-ordinate of start point shifted to intersect the bottom of the visible rectangle */
                int width;
                COLORREF col;
                HBRUSH hbrush, hprevbrush;
                int leftx, rightx; /* x co-ordinates where the leftmost and rightmost gradient lines hit the top of the visible rectangle */
                int x;
                int tilt; /* horizontal distance covered by a gradient line */

                startx = roundr((rc.top - endpointsf[0].Y) * (endpointsf[1].Y - endpointsf[0].Y) / (endpointsf[0].X - endpointsf[1].X) + endpointsf[0].X);
                endx = roundr((rc.top - endpointsf[1].Y) * (endpointsf[1].Y - endpointsf[0].Y) / (endpointsf[0].X - endpointsf[1].X) + endpointsf[1].X);
                width = endx - startx;
                startbottomx = roundr((rc.bottom - endpointsf[0].Y) * (endpointsf[1].Y - endpointsf[0].Y) / (endpointsf[0].X - endpointsf[1].X) + endpointsf[0].X);
                tilt = startx - startbottomx;

                if (startx >= startbottomx)
                {
                    leftx = rc.left;
                    rightx = rc.right + tilt;
                }
                else
                {
                    leftx = rc.left + tilt;
                    rightx = rc.right;
                }

                poly[0].y = rc.bottom;
                poly[1].y = rc.top;
                poly[2].y = rc.top;
                poly[3].y = rc.bottom;

                for (x=leftx; x<=rightx; x++)
                {
                    ARGB argb = blend_line_gradient(line, (x-startx)/(REAL)width);
                    col = ARGB2COLORREF(argb);
                    hbrush = CreateSolidBrush(col);
                    hprevbrush = SelectObject(graphics->hdc, hbrush);
                    poly[0].x = x - tilt - 1;
                    poly[1].x = x - 1;
                    poly[2].x = x;
                    poly[3].x = x - tilt;
                    Polygon(graphics->hdc, poly, 4);
                    SelectObject(graphics->hdc, hprevbrush);
                    DeleteObject(hbrush);
                }
            }
            else if (endpointsi[0].y != endpointsi[1].y)
            {
                /* horizontal-ish gradient */
                int starty, endy; /* y co-ordinates of endpoints shifted to intersect the left of the visible rectangle */
                int startrighty; /* y co-ordinate of start point shifted to intersect the right of the visible rectangle */
                int height;
                COLORREF col;
                HBRUSH hbrush, hprevbrush;
                int topy, bottomy; /* y co-ordinates where the topmost and bottommost gradient lines hit the left of the visible rectangle */
                int y;
                int tilt; /* vertical distance covered by a gradient line */

                starty = roundr((rc.left - endpointsf[0].X) * (endpointsf[0].X - endpointsf[1].X) / (endpointsf[1].Y - endpointsf[0].Y) + endpointsf[0].Y);
                endy = roundr((rc.left - endpointsf[1].X) * (endpointsf[0].X - endpointsf[1].X) / (endpointsf[1].Y - endpointsf[0].Y) + endpointsf[1].Y);
                height = endy - starty;
                startrighty = roundr((rc.right - endpointsf[0].X) * (endpointsf[0].X - endpointsf[1].X) / (endpointsf[1].Y - endpointsf[0].Y) + endpointsf[0].Y);
                tilt = starty - startrighty;

                if (starty >= startrighty)
                {
                    topy = rc.top;
                    bottomy = rc.bottom + tilt;
                }
                else
                {
                    topy = rc.top + tilt;
                    bottomy = rc.bottom;
                }

                poly[0].x = rc.right;
                poly[1].x = rc.left;
                poly[2].x = rc.left;
                poly[3].x = rc.right;

                for (y=topy; y<=bottomy; y++)
                {
                    ARGB argb = blend_line_gradient(line, (y-starty)/(REAL)height);
                    col = ARGB2COLORREF(argb);
                    hbrush = CreateSolidBrush(col);
                    hprevbrush = SelectObject(graphics->hdc, hbrush);
                    poly[0].y = y - tilt - 1;
                    poly[1].y = y - 1;
                    poly[2].y = y;
                    poly[3].y = y - tilt;
                    Polygon(graphics->hdc, poly, 4);
                    SelectObject(graphics->hdc, hprevbrush);
                    DeleteObject(hbrush);
                }
            }
            /* else startpoint == endpoint */
        }
        break;
    }
    case BrushTypeSolidColor:
    {
        GpSolidFill *fill = (GpSolidFill*)brush;
        if (fill->bmp)
        {
            RECT rc;
            /* partially transparent fill */

            SelectClipPath(graphics->hdc, RGN_AND);
            if (GetClipBox(graphics->hdc, &rc) != NULLREGION)
            {
                HDC hdc = CreateCompatibleDC(NULL);
                HBITMAP oldbmp;
                BLENDFUNCTION bf;

                if (!hdc) break;

                oldbmp = SelectObject(hdc, fill->bmp);

                bf.BlendOp = AC_SRC_OVER;
                bf.BlendFlags = 0;
                bf.SourceConstantAlpha = 255;
                bf.AlphaFormat = AC_SRC_ALPHA;

                GdiAlphaBlend(graphics->hdc, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, hdc, 0, 0, 1, 1, bf);

                SelectObject(hdc, oldbmp);
                DeleteDC(hdc);
            }

            break;
        }
        /* else fall through */
    }
    default:
        SelectObject(graphics->hdc, brush->gdibrush);
        FillPath(graphics->hdc);
        break;
    }
}

/* GdipDrawPie/GdipFillPie helper function */
static void draw_pie(GpGraphics *graphics, REAL x, REAL y, REAL width,
    REAL height, REAL startAngle, REAL sweepAngle)
{
    GpPointF ptf[4];
    POINT pti[4];

    ptf[0].X = x;
    ptf[0].Y = y;
    ptf[1].X = x + width;
    ptf[1].Y = y + height;

    deg2xy(startAngle+sweepAngle, x + width / 2.0, y + width / 2.0, &ptf[2].X, &ptf[2].Y);
    deg2xy(startAngle, x + width / 2.0, y + width / 2.0, &ptf[3].X, &ptf[3].Y);

    transform_and_round_points(graphics, pti, ptf, 4);

    Pie(graphics->hdc, pti[0].x, pti[0].y, pti[1].x, pti[1].y, pti[2].x,
        pti[2].y, pti[3].x, pti[3].y);
}

/* Draws the linecap the specified color and size on the hdc.  The linecap is in
 * direction of the line from x1, y1 to x2, y2 and is anchored on x2, y2. Probably
 * should not be called on an hdc that has a path you care about. */
static void draw_cap(GpGraphics *graphics, COLORREF color, GpLineCap cap, REAL size,
    const GpCustomLineCap *custom, REAL x1, REAL y1, REAL x2, REAL y2)
{
    HGDIOBJ oldbrush = NULL, oldpen = NULL;
    GpMatrix *matrix = NULL;
    HBRUSH brush = NULL;
    HPEN pen = NULL;
    PointF ptf[4], *custptf = NULL;
    POINT pt[4], *custpt = NULL;
    BYTE *tp = NULL;
    REAL theta, dsmall, dbig, dx, dy = 0.0;
    INT i, count;
    LOGBRUSH lb;
    BOOL customstroke;

    if((x1 == x2) && (y1 == y2))
        return;

    theta = gdiplus_atan2(y2 - y1, x2 - x1);

    customstroke = (cap == LineCapCustom) && custom && (!custom->fill);
    if(!customstroke){
        brush = CreateSolidBrush(color);
        lb.lbStyle = BS_SOLID;
        lb.lbColor = color;
        lb.lbHatch = 0;
        pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT |
                           PS_JOIN_MITER, 1, &lb, 0,
                           NULL);
        oldbrush = SelectObject(graphics->hdc, brush);
        oldpen = SelectObject(graphics->hdc, pen);
    }

    switch(cap){
        case LineCapFlat:
            break;
        case LineCapSquare:
        case LineCapSquareAnchor:
        case LineCapDiamondAnchor:
            size = size * (cap & LineCapNoAnchor ? ANCHOR_WIDTH : 1.0) / 2.0;
            if(cap == LineCapDiamondAnchor){
                dsmall = cos(theta + M_PI_2) * size;
                dbig = sin(theta + M_PI_2) * size;
            }
            else{
                dsmall = cos(theta + M_PI_4) * size;
                dbig = sin(theta + M_PI_4) * size;
            }

            ptf[0].X = x2 - dsmall;
            ptf[1].X = x2 + dbig;

            ptf[0].Y = y2 - dbig;
            ptf[3].Y = y2 + dsmall;

            ptf[1].Y = y2 - dsmall;
            ptf[2].Y = y2 + dbig;

            ptf[3].X = x2 - dbig;
            ptf[2].X = x2 + dsmall;

            transform_and_round_points(graphics, pt, ptf, 4);
            Polygon(graphics->hdc, pt, 4);

            break;
        case LineCapArrowAnchor:
            size = size * 4.0 / sqrt(3.0);

            dx = cos(M_PI / 6.0 + theta) * size;
            dy = sin(M_PI / 6.0 + theta) * size;

            ptf[0].X = x2 - dx;
            ptf[0].Y = y2 - dy;

            dx = cos(- M_PI / 6.0 + theta) * size;
            dy = sin(- M_PI / 6.0 + theta) * size;

            ptf[1].X = x2 - dx;
            ptf[1].Y = y2 - dy;

            ptf[2].X = x2;
            ptf[2].Y = y2;

            transform_and_round_points(graphics, pt, ptf, 3);
            Polygon(graphics->hdc, pt, 3);

            break;
        case LineCapRoundAnchor:
            dx = dy = ANCHOR_WIDTH * size / 2.0;

            ptf[0].X = x2 - dx;
            ptf[0].Y = y2 - dy;
            ptf[1].X = x2 + dx;
            ptf[1].Y = y2 + dy;

            transform_and_round_points(graphics, pt, ptf, 2);
            Ellipse(graphics->hdc, pt[0].x, pt[0].y, pt[1].x, pt[1].y);

            break;
        case LineCapTriangle:
            size = size / 2.0;
            dx = cos(M_PI_2 + theta) * size;
            dy = sin(M_PI_2 + theta) * size;

            ptf[0].X = x2 - dx;
            ptf[0].Y = y2 - dy;
            ptf[1].X = x2 + dx;
            ptf[1].Y = y2 + dy;

            dx = cos(theta) * size;
            dy = sin(theta) * size;

            ptf[2].X = x2 + dx;
            ptf[2].Y = y2 + dy;

            transform_and_round_points(graphics, pt, ptf, 3);
            Polygon(graphics->hdc, pt, 3);

            break;
        case LineCapRound:
            dx = dy = size / 2.0;

            ptf[0].X = x2 - dx;
            ptf[0].Y = y2 - dy;
            ptf[1].X = x2 + dx;
            ptf[1].Y = y2 + dy;

            dx = -cos(M_PI_2 + theta) * size;
            dy = -sin(M_PI_2 + theta) * size;

            ptf[2].X = x2 - dx;
            ptf[2].Y = y2 - dy;
            ptf[3].X = x2 + dx;
            ptf[3].Y = y2 + dy;

            transform_and_round_points(graphics, pt, ptf, 4);
            Pie(graphics->hdc, pt[0].x, pt[0].y, pt[1].x, pt[1].y, pt[2].x,
                pt[2].y, pt[3].x, pt[3].y);

            break;
        case LineCapCustom:
            if(!custom)
                break;

            count = custom->pathdata.Count;
            custptf = GdipAlloc(count * sizeof(PointF));
            custpt = GdipAlloc(count * sizeof(POINT));
            tp = GdipAlloc(count);

            if(!custptf || !custpt || !tp || (GdipCreateMatrix(&matrix) != Ok))
                goto custend;

            memcpy(custptf, custom->pathdata.Points, count * sizeof(PointF));

            GdipScaleMatrix(matrix, size, size, MatrixOrderAppend);
            GdipRotateMatrix(matrix, (180.0 / M_PI) * (theta - M_PI_2),
                             MatrixOrderAppend);
            GdipTranslateMatrix(matrix, x2, y2, MatrixOrderAppend);
            GdipTransformMatrixPoints(matrix, custptf, count);

            transform_and_round_points(graphics, custpt, custptf, count);

            for(i = 0; i < count; i++)
                tp[i] = convert_path_point_type(custom->pathdata.Types[i]);

            if(custom->fill){
                BeginPath(graphics->hdc);
                PolyDraw(graphics->hdc, custpt, tp, count);
                EndPath(graphics->hdc);
                StrokeAndFillPath(graphics->hdc);
            }
            else
                PolyDraw(graphics->hdc, custpt, tp, count);

custend:
            GdipFree(custptf);
            GdipFree(custpt);
            GdipFree(tp);
            GdipDeleteMatrix(matrix);
            break;
        default:
            break;
    }

    if(!customstroke){
        SelectObject(graphics->hdc, oldbrush);
        SelectObject(graphics->hdc, oldpen);
        DeleteObject(brush);
        DeleteObject(pen);
    }
}

/* Shortens the line by the given percent by changing x2, y2.
 * If percent is > 1.0 then the line will change direction.
 * If percent is negative it can lengthen the line. */
static void shorten_line_percent(REAL x1, REAL  y1, REAL *x2, REAL *y2, REAL percent)
{
    REAL dist, theta, dx, dy;

    if((y1 == *y2) && (x1 == *x2))
        return;

    dist = sqrt((*x2 - x1) * (*x2 - x1) + (*y2 - y1) * (*y2 - y1)) * -percent;
    theta = gdiplus_atan2((*y2 - y1), (*x2 - x1));
    dx = cos(theta) * dist;
    dy = sin(theta) * dist;

    *x2 = *x2 + dx;
    *y2 = *y2 + dy;
}

/* Shortens the line by the given amount by changing x2, y2.
 * If the amount is greater than the distance, the line will become length 0.
 * If the amount is negative, it can lengthen the line. */
static void shorten_line_amt(REAL x1, REAL y1, REAL *x2, REAL *y2, REAL amt)
{
    REAL dx, dy, percent;

    dx = *x2 - x1;
    dy = *y2 - y1;
    if(dx == 0 && dy == 0)
        return;

    percent = amt / sqrt(dx * dx + dy * dy);
    if(percent >= 1.0){
        *x2 = x1;
        *y2 = y1;
        return;
    }

    shorten_line_percent(x1, y1, x2, y2, percent);
}

/* Draws lines between the given points, and if caps is true then draws an endcap
 * at the end of the last line. */
static GpStatus draw_polyline(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF * pt, INT count, BOOL caps)
{
    POINT *pti = NULL;
    GpPointF *ptcopy = NULL;
    GpStatus status = GenericError;

    if(!count)
        return Ok;

    pti = GdipAlloc(count * sizeof(POINT));
    ptcopy = GdipAlloc(count * sizeof(GpPointF));

    if(!pti || !ptcopy){
        status = OutOfMemory;
        goto end;
    }

    memcpy(ptcopy, pt, count * sizeof(GpPointF));

    if(caps){
        if(pen->endcap == LineCapArrowAnchor)
            shorten_line_amt(ptcopy[count-2].X, ptcopy[count-2].Y,
                             &ptcopy[count-1].X, &ptcopy[count-1].Y, pen->width);
        else if((pen->endcap == LineCapCustom) && pen->customend)
            shorten_line_amt(ptcopy[count-2].X, ptcopy[count-2].Y,
                             &ptcopy[count-1].X, &ptcopy[count-1].Y,
                             pen->customend->inset * pen->width);

        if(pen->startcap == LineCapArrowAnchor)
            shorten_line_amt(ptcopy[1].X, ptcopy[1].Y,
                             &ptcopy[0].X, &ptcopy[0].Y, pen->width);
        else if((pen->startcap == LineCapCustom) && pen->customstart)
            shorten_line_amt(ptcopy[1].X, ptcopy[1].Y,
                             &ptcopy[0].X, &ptcopy[0].Y,
                             pen->customstart->inset * pen->width);

        draw_cap(graphics, pen->brush->lb.lbColor, pen->endcap, pen->width, pen->customend,
                 pt[count - 2].X, pt[count - 2].Y, pt[count - 1].X, pt[count - 1].Y);
        draw_cap(graphics, pen->brush->lb.lbColor, pen->startcap, pen->width, pen->customstart,
                         pt[1].X, pt[1].Y, pt[0].X, pt[0].Y);
    }

    transform_and_round_points(graphics, pti, ptcopy, count);

    if(Polyline(graphics->hdc, pti, count))
        status = Ok;

end:
    GdipFree(pti);
    GdipFree(ptcopy);

    return status;
}

/* Conducts a linear search to find the bezier points that will back off
 * the endpoint of the curve by a distance of amt. Linear search works
 * better than binary in this case because there are multiple solutions,
 * and binary searches often find a bad one. I don't think this is what
 * Windows does but short of rendering the bezier without GDI's help it's
 * the best we can do. If rev then work from the start of the passed points
 * instead of the end. */
static void shorten_bezier_amt(GpPointF * pt, REAL amt, BOOL rev)
{
    GpPointF origpt[4];
    REAL percent = 0.00, dx, dy, origx, origy, diff = -1.0;
    INT i, first = 0, second = 1, third = 2, fourth = 3;

    if(rev){
        first = 3;
        second = 2;
        third = 1;
        fourth = 0;
    }

    origx = pt[fourth].X;
    origy = pt[fourth].Y;
    memcpy(origpt, pt, sizeof(GpPointF) * 4);

    for(i = 0; (i < MAX_ITERS) && (diff < amt); i++){
        /* reset bezier points to original values */
        memcpy(pt, origpt, sizeof(GpPointF) * 4);
        /* Perform magic on bezier points. Order is important here.*/
        shorten_line_percent(pt[third].X, pt[third].Y, &pt[fourth].X, &pt[fourth].Y, percent);
        shorten_line_percent(pt[second].X, pt[second].Y, &pt[third].X, &pt[third].Y, percent);
        shorten_line_percent(pt[third].X, pt[third].Y, &pt[fourth].X, &pt[fourth].Y, percent);
        shorten_line_percent(pt[first].X, pt[first].Y, &pt[second].X, &pt[second].Y, percent);
        shorten_line_percent(pt[second].X, pt[second].Y, &pt[third].X, &pt[third].Y, percent);
        shorten_line_percent(pt[third].X, pt[third].Y, &pt[fourth].X, &pt[fourth].Y, percent);

        dx = pt[fourth].X - origx;
        dy = pt[fourth].Y - origy;

        diff = sqrt(dx * dx + dy * dy);
        percent += 0.0005 * amt;
    }
}

/* Draws bezier curves between given points, and if caps is true then draws an
 * endcap at the end of the last line. */
static GpStatus draw_polybezier(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF * pt, INT count, BOOL caps)
{
    POINT *pti;
    GpPointF *ptcopy;
    GpStatus status = GenericError;

    if(!count)
        return Ok;

    pti = GdipAlloc(count * sizeof(POINT));
    ptcopy = GdipAlloc(count * sizeof(GpPointF));

    if(!pti || !ptcopy){
        status = OutOfMemory;
        goto end;
    }

    memcpy(ptcopy, pt, count * sizeof(GpPointF));

    if(caps){
        if(pen->endcap == LineCapArrowAnchor)
            shorten_bezier_amt(&ptcopy[count-4], pen->width, FALSE);
        else if((pen->endcap == LineCapCustom) && pen->customend)
            shorten_bezier_amt(&ptcopy[count-4], pen->width * pen->customend->inset,
                               FALSE);

        if(pen->startcap == LineCapArrowAnchor)
            shorten_bezier_amt(ptcopy, pen->width, TRUE);
        else if((pen->startcap == LineCapCustom) && pen->customstart)
            shorten_bezier_amt(ptcopy, pen->width * pen->customstart->inset, TRUE);

        /* the direction of the line cap is parallel to the direction at the
         * end of the bezier (which, if it has been shortened, is not the same
         * as the direction from pt[count-2] to pt[count-1]) */
        draw_cap(graphics, pen->brush->lb.lbColor, pen->endcap, pen->width, pen->customend,
            pt[count - 1].X - (ptcopy[count - 1].X - ptcopy[count - 2].X),
            pt[count - 1].Y - (ptcopy[count - 1].Y - ptcopy[count - 2].Y),
            pt[count - 1].X, pt[count - 1].Y);

        draw_cap(graphics, pen->brush->lb.lbColor, pen->startcap, pen->width, pen->customstart,
            pt[0].X - (ptcopy[0].X - ptcopy[1].X),
            pt[0].Y - (ptcopy[0].Y - ptcopy[1].Y), pt[0].X, pt[0].Y);
    }

    transform_and_round_points(graphics, pti, ptcopy, count);

    PolyBezier(graphics->hdc, pti, count);

    status = Ok;

end:
    GdipFree(pti);
    GdipFree(ptcopy);

    return status;
}

/* Draws a combination of bezier curves and lines between points. */
static GpStatus draw_poly(GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF * pt,
    GDIPCONST BYTE * types, INT count, BOOL caps)
{
    POINT *pti = GdipAlloc(count * sizeof(POINT));
    BYTE *tp = GdipAlloc(count);
    GpPointF *ptcopy = GdipAlloc(count * sizeof(GpPointF));
    INT i, j;
    GpStatus status = GenericError;

    if(!count){
        status = Ok;
        goto end;
    }
    if(!pti || !tp || !ptcopy){
        status = OutOfMemory;
        goto end;
    }

    for(i = 1; i < count; i++){
        if((types[i] & PathPointTypePathTypeMask) == PathPointTypeBezier){
            if((i + 2 >= count) || !(types[i + 1] & PathPointTypeBezier)
                || !(types[i + 1] & PathPointTypeBezier)){
                ERR("Bad bezier points\n");
                goto end;
            }
            i += 2;
        }
    }

    memcpy(ptcopy, pt, count * sizeof(GpPointF));

    /* If we are drawing caps, go through the points and adjust them accordingly,
     * and draw the caps. */
    if(caps){
        switch(types[count - 1] & PathPointTypePathTypeMask){
            case PathPointTypeBezier:
                if(pen->endcap == LineCapArrowAnchor)
                    shorten_bezier_amt(&ptcopy[count - 4], pen->width, FALSE);
                else if((pen->endcap == LineCapCustom) && pen->customend)
                    shorten_bezier_amt(&ptcopy[count - 4],
                                       pen->width * pen->customend->inset, FALSE);

                draw_cap(graphics, pen->brush->lb.lbColor, pen->endcap, pen->width, pen->customend,
                    pt[count - 1].X - (ptcopy[count - 1].X - ptcopy[count - 2].X),
                    pt[count - 1].Y - (ptcopy[count - 1].Y - ptcopy[count - 2].Y),
                    pt[count - 1].X, pt[count - 1].Y);

                break;
            case PathPointTypeLine:
                if(pen->endcap == LineCapArrowAnchor)
                    shorten_line_amt(ptcopy[count - 2].X, ptcopy[count - 2].Y,
                                     &ptcopy[count - 1].X, &ptcopy[count - 1].Y,
                                     pen->width);
                else if((pen->endcap == LineCapCustom) && pen->customend)
                    shorten_line_amt(ptcopy[count - 2].X, ptcopy[count - 2].Y,
                                     &ptcopy[count - 1].X, &ptcopy[count - 1].Y,
                                     pen->customend->inset * pen->width);

                draw_cap(graphics, pen->brush->lb.lbColor, pen->endcap, pen->width, pen->customend,
                         pt[count - 2].X, pt[count - 2].Y, pt[count - 1].X,
                         pt[count - 1].Y);

                break;
            default:
                ERR("Bad path last point\n");
                goto end;
        }

        /* Find start of points */
        for(j = 1; j < count && ((types[j] & PathPointTypePathTypeMask)
            == PathPointTypeStart); j++);

        switch(types[j] & PathPointTypePathTypeMask){
            case PathPointTypeBezier:
                if(pen->startcap == LineCapArrowAnchor)
                    shorten_bezier_amt(&ptcopy[j - 1], pen->width, TRUE);
                else if((pen->startcap == LineCapCustom) && pen->customstart)
                    shorten_bezier_amt(&ptcopy[j - 1],
                                       pen->width * pen->customstart->inset, TRUE);

                draw_cap(graphics, pen->brush->lb.lbColor, pen->startcap, pen->width, pen->customstart,
                    pt[j - 1].X - (ptcopy[j - 1].X - ptcopy[j].X),
                    pt[j - 1].Y - (ptcopy[j - 1].Y - ptcopy[j].Y),
                    pt[j - 1].X, pt[j - 1].Y);

                break;
            case PathPointTypeLine:
                if(pen->startcap == LineCapArrowAnchor)
                    shorten_line_amt(ptcopy[j].X, ptcopy[j].Y,
                                     &ptcopy[j - 1].X, &ptcopy[j - 1].Y,
                                     pen->width);
                else if((pen->startcap == LineCapCustom) && pen->customstart)
                    shorten_line_amt(ptcopy[j].X, ptcopy[j].Y,
                                     &ptcopy[j - 1].X, &ptcopy[j - 1].Y,
                                     pen->customstart->inset * pen->width);

                draw_cap(graphics, pen->brush->lb.lbColor, pen->startcap, pen->width, pen->customstart,
                         pt[j].X, pt[j].Y, pt[j - 1].X,
                         pt[j - 1].Y);

                break;
            default:
                ERR("Bad path points\n");
                goto end;
        }
    }

    transform_and_round_points(graphics, pti, ptcopy, count);

    for(i = 0; i < count; i++){
        tp[i] = convert_path_point_type(types[i]);
    }

    PolyDraw(graphics->hdc, pti, tp, count);

    status = Ok;

end:
    GdipFree(pti);
    GdipFree(ptcopy);
    GdipFree(tp);

    return status;
}

GpStatus trace_path(GpGraphics *graphics, GpPath *path)
{
    GpStatus result;

    BeginPath(graphics->hdc);
    result = draw_poly(graphics, NULL, path->pathdata.Points,
                       path->pathdata.Types, path->pathdata.Count, FALSE);
    EndPath(graphics->hdc);
    return result;
}

typedef struct _GraphicsContainerItem {
    struct list entry;
    GraphicsContainer contid;

    SmoothingMode smoothing;
    CompositingQuality compqual;
    InterpolationMode interpolation;
    CompositingMode compmode;
    TextRenderingHint texthint;
    REAL scale;
    GpUnit unit;
    PixelOffsetMode pixeloffset;
    UINT textcontrast;
    GpMatrix* worldtrans;
    GpRegion* clip;
} GraphicsContainerItem;

static GpStatus init_container(GraphicsContainerItem** container,
        GDIPCONST GpGraphics* graphics){
    GpStatus sts;

    *container = GdipAlloc(sizeof(GraphicsContainerItem));
    if(!(*container))
        return OutOfMemory;

    (*container)->contid = graphics->contid + 1;

    (*container)->smoothing = graphics->smoothing;
    (*container)->compqual = graphics->compqual;
    (*container)->interpolation = graphics->interpolation;
    (*container)->compmode = graphics->compmode;
    (*container)->texthint = graphics->texthint;
    (*container)->scale = graphics->scale;
    (*container)->unit = graphics->unit;
    (*container)->textcontrast = graphics->textcontrast;
    (*container)->pixeloffset = graphics->pixeloffset;

    sts = GdipCloneMatrix(graphics->worldtrans, &(*container)->worldtrans);
    if(sts != Ok){
        GdipFree(*container);
        *container = NULL;
        return sts;
    }

    sts = GdipCloneRegion(graphics->clip, &(*container)->clip);
    if(sts != Ok){
        GdipDeleteMatrix((*container)->worldtrans);
        GdipFree(*container);
        *container = NULL;
        return sts;
    }

    return Ok;
}

static void delete_container(GraphicsContainerItem* container){
    GdipDeleteMatrix(container->worldtrans);
    GdipDeleteRegion(container->clip);
    GdipFree(container);
}

static GpStatus restore_container(GpGraphics* graphics,
        GDIPCONST GraphicsContainerItem* container){
    GpStatus sts;
    GpMatrix *newTrans;
    GpRegion *newClip;

    sts = GdipCloneMatrix(container->worldtrans, &newTrans);
    if(sts != Ok)
        return sts;

    sts = GdipCloneRegion(container->clip, &newClip);
    if(sts != Ok){
        GdipDeleteMatrix(newTrans);
        return sts;
    }

    GdipDeleteMatrix(graphics->worldtrans);
    graphics->worldtrans = newTrans;

    GdipDeleteRegion(graphics->clip);
    graphics->clip = newClip;

    graphics->contid = container->contid - 1;

    graphics->smoothing = container->smoothing;
    graphics->compqual = container->compqual;
    graphics->interpolation = container->interpolation;
    graphics->compmode = container->compmode;
    graphics->texthint = container->texthint;
    graphics->scale = container->scale;
    graphics->unit = container->unit;
    graphics->textcontrast = container->textcontrast;
    graphics->pixeloffset = container->pixeloffset;

    return Ok;
}

static GpStatus get_graphics_bounds(GpGraphics* graphics, GpRectF* rect)
{
    RECT wnd_rect;

    if(graphics->hwnd) {
        if(!GetClientRect(graphics->hwnd, &wnd_rect))
            return GenericError;

        rect->X = wnd_rect.left;
        rect->Y = wnd_rect.top;
        rect->Width = wnd_rect.right - wnd_rect.left;
        rect->Height = wnd_rect.bottom - wnd_rect.top;
    }else{
        rect->X = 0;
        rect->Y = 0;
        rect->Width = GetDeviceCaps(graphics->hdc, HORZRES);
        rect->Height = GetDeviceCaps(graphics->hdc, VERTRES);
    }

    return Ok;
}

/* on success, rgn will contain the region of the graphics object which
 * is visible after clipping has been applied */
static GpStatus get_visible_clip_region(GpGraphics *graphics, GpRegion *rgn)
{
    GpStatus stat;
    GpRectF rectf;
    GpRegion* tmp;

    if((stat = get_graphics_bounds(graphics, &rectf)) != Ok)
        return stat;

    if((stat = GdipCreateRegion(&tmp)) != Ok)
        return stat;

    if((stat = GdipCombineRegionRect(tmp, &rectf, CombineModeReplace)) != Ok)
        goto end;

    if((stat = GdipCombineRegionRegion(tmp, graphics->clip, CombineModeIntersect)) != Ok)
        goto end;

    stat = GdipCombineRegionRegion(rgn, tmp, CombineModeReplace);

end:
    GdipDeleteRegion(tmp);
    return stat;
}

GpStatus WINGDIPAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics)
{
    TRACE("(%p, %p)\n", hdc, graphics);

    return GdipCreateFromHDC2(hdc, NULL, graphics);
}

GpStatus WINGDIPAPI GdipCreateFromHDC2(HDC hdc, HANDLE hDevice, GpGraphics **graphics)
{
    GpStatus retval;

    TRACE("(%p, %p, %p)\n", hdc, hDevice, graphics);

    if(hDevice != NULL) {
        FIXME("Don't know how to handle parameter hDevice\n");
        return NotImplemented;
    }

    if(hdc == NULL)
        return OutOfMemory;

    if(graphics == NULL)
        return InvalidParameter;

    *graphics = GdipAlloc(sizeof(GpGraphics));
    if(!*graphics)  return OutOfMemory;

    if((retval = GdipCreateMatrix(&(*graphics)->worldtrans)) != Ok){
        GdipFree(*graphics);
        return retval;
    }

    if((retval = GdipCreateRegion(&(*graphics)->clip)) != Ok){
        GdipFree((*graphics)->worldtrans);
        GdipFree(*graphics);
        return retval;
    }

    (*graphics)->hdc = hdc;
    (*graphics)->hwnd = WindowFromDC(hdc);
    (*graphics)->owndc = FALSE;
    (*graphics)->smoothing = SmoothingModeDefault;
    (*graphics)->compqual = CompositingQualityDefault;
    (*graphics)->interpolation = InterpolationModeDefault;
    (*graphics)->pixeloffset = PixelOffsetModeDefault;
    (*graphics)->compmode = CompositingModeSourceOver;
    (*graphics)->unit = UnitDisplay;
    (*graphics)->scale = 1.0;
    (*graphics)->busy = FALSE;
    (*graphics)->textcontrast = 4;
    list_init(&(*graphics)->containers);
    (*graphics)->contid = 0;

    TRACE("<-- %p\n", *graphics);

    return Ok;
}

GpStatus WINGDIPAPI GdipCreateFromHWND(HWND hwnd, GpGraphics **graphics)
{
    GpStatus ret;
    HDC hdc;

    TRACE("(%p, %p)\n", hwnd, graphics);

    hdc = GetDC(hwnd);

    if((ret = GdipCreateFromHDC(hdc, graphics)) != Ok)
    {
        ReleaseDC(hwnd, hdc);
        return ret;
    }

    (*graphics)->hwnd = hwnd;
    (*graphics)->owndc = TRUE;

    return Ok;
}

/* FIXME: no icm handling */
GpStatus WINGDIPAPI GdipCreateFromHWNDICM(HWND hwnd, GpGraphics **graphics)
{
    TRACE("(%p, %p)\n", hwnd, graphics);

    return GdipCreateFromHWND(hwnd, graphics);
}

GpStatus WINGDIPAPI GdipCreateMetafileFromEmf(HENHMETAFILE hemf, BOOL delete,
    GpMetafile **metafile)
{
    static int calls;

    TRACE("(%p,%i,%p)\n", hemf, delete, metafile);

    if(!hemf || !metafile)
        return InvalidParameter;

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipCreateMetafileFromWmf(HMETAFILE hwmf, BOOL delete,
    GDIPCONST WmfPlaceableFileHeader * placeable, GpMetafile **metafile)
{
    IStream *stream = NULL;
    UINT read;
    BYTE* copy;
    HENHMETAFILE hemf;
    GpStatus retval = Ok;

    TRACE("(%p, %d, %p, %p)\n", hwmf, delete, placeable, metafile);

    if(!hwmf || !metafile || !placeable)
        return InvalidParameter;

    *metafile = NULL;
    read = GetMetaFileBitsEx(hwmf, 0, NULL);
    if(!read)
        return GenericError;
    copy = GdipAlloc(read);
    GetMetaFileBitsEx(hwmf, read, copy);

    hemf = SetWinMetaFileBits(read, copy, NULL, NULL);
    GdipFree(copy);

    read = GetEnhMetaFileBits(hemf, 0, NULL);
    copy = GdipAlloc(read);
    GetEnhMetaFileBits(hemf, read, copy);
    DeleteEnhMetaFile(hemf);

    if(CreateStreamOnHGlobal(copy, TRUE, &stream) != S_OK){
        ERR("could not make stream\n");
        GdipFree(copy);
        retval = GenericError;
        goto err;
    }

    *metafile = GdipAlloc(sizeof(GpMetafile));
    if(!*metafile){
        retval = OutOfMemory;
        goto err;
    }

    if(OleLoadPicture(stream, 0, FALSE, &IID_IPicture,
        (LPVOID*) &((*metafile)->image.picture)) != S_OK)
    {
        retval = GenericError;
        goto err;
    }


    (*metafile)->image.type = ImageTypeMetafile;
    memcpy(&(*metafile)->image.format, &ImageFormatWMF, sizeof(GUID));
    (*metafile)->image.palette_flags = 0;
    (*metafile)->image.palette_count = 0;
    (*metafile)->image.palette_size = 0;
    (*metafile)->image.palette_entries = NULL;
    (*metafile)->image.xres = (REAL)placeable->Inch;
    (*metafile)->image.yres = (REAL)placeable->Inch;
    (*metafile)->bounds.X = ((REAL) placeable->BoundingBox.Left) / ((REAL) placeable->Inch);
    (*metafile)->bounds.Y = ((REAL) placeable->BoundingBox.Top) / ((REAL) placeable->Inch);
    (*metafile)->bounds.Width = ((REAL) (placeable->BoundingBox.Right
                    - placeable->BoundingBox.Left));
    (*metafile)->bounds.Height = ((REAL) (placeable->BoundingBox.Bottom
                   - placeable->BoundingBox.Top));
    (*metafile)->unit = UnitPixel;

    if(delete)
        DeleteMetaFile(hwmf);

    TRACE("<-- %p\n", *metafile);

err:
    if (retval != Ok)
        GdipFree(*metafile);
    IStream_Release(stream);
    return retval;
}

GpStatus WINGDIPAPI GdipCreateMetafileFromWmfFile(GDIPCONST WCHAR *file,
    GDIPCONST WmfPlaceableFileHeader * placeable, GpMetafile **metafile)
{
    HMETAFILE hmf = GetMetaFileW(file);

    TRACE("(%s, %p, %p)\n", debugstr_w(file), placeable, metafile);

    if(!hmf) return InvalidParameter;

    return GdipCreateMetafileFromWmf(hmf, TRUE, placeable, metafile);
}

GpStatus WINGDIPAPI GdipCreateMetafileFromFile(GDIPCONST WCHAR *file,
    GpMetafile **metafile)
{
    FIXME("(%p, %p): stub\n", file, metafile);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipCreateMetafileFromStream(IStream *stream,
    GpMetafile **metafile)
{
    FIXME("(%p, %p): stub\n", stream, metafile);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipCreateStreamOnFile(GDIPCONST WCHAR * filename,
    UINT access, IStream **stream)
{
    DWORD dwMode;
    HRESULT ret;

    TRACE("(%s, %u, %p)\n", debugstr_w(filename), access, stream);

    if(!stream || !filename)
        return InvalidParameter;

    if(access & GENERIC_WRITE)
        dwMode = STGM_SHARE_DENY_WRITE | STGM_WRITE | STGM_CREATE;
    else if(access & GENERIC_READ)
        dwMode = STGM_SHARE_DENY_WRITE | STGM_READ | STGM_FAILIFTHERE;
    else
        return InvalidParameter;

    ret = SHCreateStreamOnFileW(filename, dwMode, stream);

    return hresult_to_status(ret);
}

GpStatus WINGDIPAPI GdipDeleteGraphics(GpGraphics *graphics)
{
    GraphicsContainerItem *cont, *next;
    TRACE("(%p)\n", graphics);

    if(!graphics) return InvalidParameter;
    if(graphics->busy) return ObjectBusy;

    if(graphics->owndc)
        ReleaseDC(graphics->hwnd, graphics->hdc);

    LIST_FOR_EACH_ENTRY_SAFE(cont, next, &graphics->containers, GraphicsContainerItem, entry){
        list_remove(&cont->entry);
        delete_container(cont);
    }

    GdipDeleteRegion(graphics->clip);
    GdipDeleteMatrix(graphics->worldtrans);
    GdipFree(graphics);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawArc(GpGraphics *graphics, GpPen *pen, REAL x,
    REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    INT save_state, num_pts;
    GpPointF points[MAX_ARC_PTS];
    GpStatus retval;

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f)\n", graphics, pen, x, y,
          width, height, startAngle, sweepAngle);

    if(!graphics || !pen || width <= 0 || height <= 0)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    num_pts = arc2polybezier(points, x, y, width, height, startAngle, sweepAngle);

    save_state = prepare_dc(graphics, pen);

    retval = draw_polybezier(graphics, pen, points, num_pts, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawArcI(GpGraphics *graphics, GpPen *pen, INT x,
    INT y, INT width, INT height, REAL startAngle, REAL sweepAngle)
{
    TRACE("(%p, %p, %d, %d, %d, %d, %.2f, %.2f)\n", graphics, pen, x, y,
          width, height, startAngle, sweepAngle);

    return GdipDrawArc(graphics,pen,(REAL)x,(REAL)y,(REAL)width,(REAL)height,startAngle,sweepAngle);
}

GpStatus WINGDIPAPI GdipDrawBezier(GpGraphics *graphics, GpPen *pen, REAL x1,
    REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4)
{
    INT save_state;
    GpPointF pt[4];
    GpStatus retval;

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f)\n", graphics, pen, x1, y1,
          x2, y2, x3, y3, x4, y4);

    if(!graphics || !pen)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pt[0].X = x1;
    pt[0].Y = y1;
    pt[1].X = x2;
    pt[1].Y = y2;
    pt[2].X = x3;
    pt[2].Y = y3;
    pt[3].X = x4;
    pt[3].Y = y4;

    save_state = prepare_dc(graphics, pen);

    retval = draw_polybezier(graphics, pen, pt, 4, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawBezierI(GpGraphics *graphics, GpPen *pen, INT x1,
    INT y1, INT x2, INT y2, INT x3, INT y3, INT x4, INT y4)
{
    INT save_state;
    GpPointF pt[4];
    GpStatus retval;

    TRACE("(%p, %p, %d, %d, %d, %d, %d, %d, %d, %d)\n", graphics, pen, x1, y1,
          x2, y2, x3, y3, x4, y4);

    if(!graphics || !pen)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pt[0].X = x1;
    pt[0].Y = y1;
    pt[1].X = x2;
    pt[1].Y = y2;
    pt[2].X = x3;
    pt[2].Y = y3;
    pt[3].X = x4;
    pt[3].Y = y4;

    save_state = prepare_dc(graphics, pen);

    retval = draw_polybezier(graphics, pen, pt, 4, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawBeziers(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF *points, INT count)
{
    INT i;
    GpStatus ret;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    if(!graphics || !pen || !points || (count <= 0))
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    for(i = 0; i < floor(count / 4); i++){
        ret = GdipDrawBezier(graphics, pen,
                             points[4*i].X, points[4*i].Y,
                             points[4*i + 1].X, points[4*i + 1].Y,
                             points[4*i + 2].X, points[4*i + 2].Y,
                             points[4*i + 3].X, points[4*i + 3].Y);
        if(ret != Ok)
            return ret;
    }

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawBeziersI(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPoint *points, INT count)
{
    GpPointF *pts;
    GpStatus ret;
    INT i;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    if(!graphics || !pen || !points || (count <= 0))
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pts = GdipAlloc(sizeof(GpPointF) * count);
    if(!pts)
        return OutOfMemory;

    for(i = 0; i < count; i++){
        pts[i].X = (REAL)points[i].X;
        pts[i].Y = (REAL)points[i].Y;
    }

    ret = GdipDrawBeziers(graphics,pen,pts,count);

    GdipFree(pts);

    return ret;
}

GpStatus WINGDIPAPI GdipDrawClosedCurve(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF *points, INT count)
{
    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    return GdipDrawClosedCurve2(graphics, pen, points, count, 1.0);
}

GpStatus WINGDIPAPI GdipDrawClosedCurveI(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPoint *points, INT count)
{
    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    return GdipDrawClosedCurve2I(graphics, pen, points, count, 1.0);
}

GpStatus WINGDIPAPI GdipDrawClosedCurve2(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF *points, INT count, REAL tension)
{
    GpPath *path;
    GpStatus stat;

    TRACE("(%p, %p, %p, %d, %.2f)\n", graphics, pen, points, count, tension);

    if(!graphics || !pen || !points || count <= 0)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    if((stat = GdipCreatePath(FillModeAlternate, &path)) != Ok)
        return stat;

    stat = GdipAddPathClosedCurve2(path, points, count, tension);
    if(stat != Ok){
        GdipDeletePath(path);
        return stat;
    }

    stat = GdipDrawPath(graphics, pen, path);

    GdipDeletePath(path);

    return stat;
}

GpStatus WINGDIPAPI GdipDrawClosedCurve2I(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPoint *points, INT count, REAL tension)
{
    GpPointF *ptf;
    GpStatus stat;
    INT i;

    TRACE("(%p, %p, %p, %d, %.2f)\n", graphics, pen, points, count, tension);

    if(!points || count <= 0)
        return InvalidParameter;

    ptf = GdipAlloc(sizeof(GpPointF)*count);
    if(!ptf)
        return OutOfMemory;

    for(i = 0; i < count; i++){
        ptf[i].X = (REAL)points[i].X;
        ptf[i].Y = (REAL)points[i].Y;
    }

    stat = GdipDrawClosedCurve2(graphics, pen, ptf, count, tension);

    GdipFree(ptf);

    return stat;
}

GpStatus WINGDIPAPI GdipDrawCurve(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF *points, INT count)
{
    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    return GdipDrawCurve2(graphics,pen,points,count,1.0);
}

GpStatus WINGDIPAPI GdipDrawCurveI(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPoint *points, INT count)
{
    GpPointF *pointsF;
    GpStatus ret;
    INT i;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    if(!points)
        return InvalidParameter;

    pointsF = GdipAlloc(sizeof(GpPointF)*count);
    if(!pointsF)
        return OutOfMemory;

    for(i = 0; i < count; i++){
        pointsF[i].X = (REAL)points[i].X;
        pointsF[i].Y = (REAL)points[i].Y;
    }

    ret = GdipDrawCurve(graphics,pen,pointsF,count);
    GdipFree(pointsF);

    return ret;
}

/* Approximates cardinal spline with Bezier curves. */
GpStatus WINGDIPAPI GdipDrawCurve2(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF *points, INT count, REAL tension)
{
    /* PolyBezier expects count*3-2 points. */
    INT i, len_pt = count*3-2, save_state;
    GpPointF *pt;
    REAL x1, x2, y1, y2;
    GpStatus retval;

    TRACE("(%p, %p, %p, %d, %.2f)\n", graphics, pen, points, count, tension);

    if(!graphics || !pen)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    if(count < 2)
        return InvalidParameter;

    pt = GdipAlloc(len_pt * sizeof(GpPointF));
    if(!pt)
        return OutOfMemory;

    tension = tension * TENSION_CONST;

    calc_curve_bezier_endp(points[0].X, points[0].Y, points[1].X, points[1].Y,
        tension, &x1, &y1);

    pt[0].X = points[0].X;
    pt[0].Y = points[0].Y;
    pt[1].X = x1;
    pt[1].Y = y1;

    for(i = 0; i < count-2; i++){
        calc_curve_bezier(&(points[i]), tension, &x1, &y1, &x2, &y2);

        pt[3*i+2].X = x1;
        pt[3*i+2].Y = y1;
        pt[3*i+3].X = points[i+1].X;
        pt[3*i+3].Y = points[i+1].Y;
        pt[3*i+4].X = x2;
        pt[3*i+4].Y = y2;
    }

    calc_curve_bezier_endp(points[count-1].X, points[count-1].Y,
        points[count-2].X, points[count-2].Y, tension, &x1, &y1);

    pt[len_pt-2].X = x1;
    pt[len_pt-2].Y = y1;
    pt[len_pt-1].X = points[count-1].X;
    pt[len_pt-1].Y = points[count-1].Y;

    save_state = prepare_dc(graphics, pen);

    retval = draw_polybezier(graphics, pen, pt, len_pt, TRUE);

    GdipFree(pt);
    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawCurve2I(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPoint *points, INT count, REAL tension)
{
    GpPointF *pointsF;
    GpStatus ret;
    INT i;

    TRACE("(%p, %p, %p, %d, %.2f)\n", graphics, pen, points, count, tension);

    if(!points)
        return InvalidParameter;

    pointsF = GdipAlloc(sizeof(GpPointF)*count);
    if(!pointsF)
        return OutOfMemory;

    for(i = 0; i < count; i++){
        pointsF[i].X = (REAL)points[i].X;
        pointsF[i].Y = (REAL)points[i].Y;
    }

    ret = GdipDrawCurve2(graphics,pen,pointsF,count,tension);
    GdipFree(pointsF);

    return ret;
}

GpStatus WINGDIPAPI GdipDrawCurve3(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPointF *points, INT count, INT offset, INT numberOfSegments,
    REAL tension)
{
    TRACE("(%p, %p, %p, %d, %d, %d, %.2f)\n", graphics, pen, points, count, offset, numberOfSegments, tension);

    if(offset >= count || numberOfSegments > count - offset - 1 || numberOfSegments <= 0){
        return InvalidParameter;
    }

    return GdipDrawCurve2(graphics, pen, points + offset, numberOfSegments + 1, tension);
}

GpStatus WINGDIPAPI GdipDrawCurve3I(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpPoint *points, INT count, INT offset, INT numberOfSegments,
    REAL tension)
{
    TRACE("(%p, %p, %p, %d, %d, %d, %.2f)\n", graphics, pen, points, count, offset, numberOfSegments, tension);

    if(count < 0){
        return OutOfMemory;
    }

    if(offset >= count || numberOfSegments > count - offset - 1 || numberOfSegments <= 0){
        return InvalidParameter;
    }

    return GdipDrawCurve2I(graphics, pen, points + offset, numberOfSegments + 1, tension);
}

GpStatus WINGDIPAPI GdipDrawEllipse(GpGraphics *graphics, GpPen *pen, REAL x,
    REAL y, REAL width, REAL height)
{
    INT save_state;
    GpPointF ptf[2];
    POINT pti[2];

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f)\n", graphics, pen, x, y, width, height);

    if(!graphics || !pen)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf[0].X = x;
    ptf[0].Y = y;
    ptf[1].X = x + width;
    ptf[1].Y = y + height;

    save_state = prepare_dc(graphics, pen);
    SelectObject(graphics->hdc, GetStockObject(NULL_BRUSH));

    transform_and_round_points(graphics, pti, ptf, 2);

    Ellipse(graphics->hdc, pti[0].x, pti[0].y, pti[1].x, pti[1].y);

    restore_dc(graphics, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawEllipseI(GpGraphics *graphics, GpPen *pen, INT x,
    INT y, INT width, INT height)
{
    TRACE("(%p, %p, %d, %d, %d, %d)\n", graphics, pen, x, y, width, height);

    return GdipDrawEllipse(graphics,pen,(REAL)x,(REAL)y,(REAL)width,(REAL)height);
}


GpStatus WINGDIPAPI GdipDrawImage(GpGraphics *graphics, GpImage *image, REAL x, REAL y)
{
    UINT width, height;
    GpPointF points[3];

    TRACE("(%p, %p, %.2f, %.2f)\n", graphics, image, x, y);

    if(!graphics || !image)
        return InvalidParameter;

    GdipGetImageWidth(image, &width);
    GdipGetImageHeight(image, &height);

    /* FIXME: we should use the graphics and image dpi, somehow */

    points[0].X = points[2].X = x;
    points[0].Y = points[1].Y = y;
    points[1].X = x + width;
    points[2].Y = y + height;

    return GdipDrawImagePointsRect(graphics, image, points, 3, 0, 0, width, height,
        UnitPixel, NULL, NULL, NULL);
}

GpStatus WINGDIPAPI GdipDrawImageI(GpGraphics *graphics, GpImage *image, INT x,
    INT y)
{
    TRACE("(%p, %p, %d, %d)\n", graphics, image, x, y);

    return GdipDrawImage(graphics, image, (REAL)x, (REAL)y);
}

GpStatus WINGDIPAPI GdipDrawImagePointRect(GpGraphics *graphics, GpImage *image,
    REAL x, REAL y, REAL srcx, REAL srcy, REAL srcwidth, REAL srcheight,
    GpUnit srcUnit)
{
    GpPointF points[3];
    TRACE("(%p, %p, %f, %f, %f, %f, %f, %f, %d)\n", graphics, image, x, y, srcx, srcy, srcwidth, srcheight, srcUnit);

    points[0].X = points[2].X = x;
    points[0].Y = points[1].Y = y;

    /* FIXME: convert image coordinates to Graphics coordinates? */
    points[1].X = x + srcwidth;
    points[2].Y = y + srcheight;

    return GdipDrawImagePointsRect(graphics, image, points, 3, srcx, srcy,
        srcwidth, srcheight, srcUnit, NULL, NULL, NULL);
}

GpStatus WINGDIPAPI GdipDrawImagePointRectI(GpGraphics *graphics, GpImage *image,
    INT x, INT y, INT srcx, INT srcy, INT srcwidth, INT srcheight,
    GpUnit srcUnit)
{
    return GdipDrawImagePointRect(graphics, image, x, y, srcx, srcy, srcwidth, srcheight, srcUnit);
}

GpStatus WINGDIPAPI GdipDrawImagePoints(GpGraphics *graphics, GpImage *image,
    GDIPCONST GpPointF *dstpoints, INT count)
{
    FIXME("(%p, %p, %p, %d): stub\n", graphics, image, dstpoints, count);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipDrawImagePointsI(GpGraphics *graphics, GpImage *image,
    GDIPCONST GpPoint *dstpoints, INT count)
{
    FIXME("(%p, %p, %p, %d): stub\n", graphics, image, dstpoints, count);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipDrawImagePointsRect(GpGraphics *graphics, GpImage *image,
     GDIPCONST GpPointF *points, INT count, REAL srcx, REAL srcy, REAL srcwidth,
     REAL srcheight, GpUnit srcUnit, GDIPCONST GpImageAttributes* imageAttributes,
     DrawImageAbort callback, VOID * callbackData)
{
    GpPointF ptf[4];
    POINT pti[4];
    REAL dx, dy;
    GpStatus stat;

    TRACE("(%p, %p, %p, %d, %f, %f, %f, %f, %d, %p, %p, %p)\n", graphics, image, points,
          count, srcx, srcy, srcwidth, srcheight, srcUnit, imageAttributes, callback,
          callbackData);

    if(!graphics || !image || !points || count != 3)
         return InvalidParameter;

    TRACE("%s %s %s\n", debugstr_pointf(&points[0]), debugstr_pointf(&points[1]),
        debugstr_pointf(&points[2]));

    memcpy(ptf, points, 3 * sizeof(GpPointF));
    ptf[3].X = ptf[2].X + ptf[1].X - ptf[0].X;
    ptf[3].Y = ptf[2].Y + ptf[1].Y - ptf[0].Y;
    transform_and_round_points(graphics, pti, ptf, 4);

    if (image->picture)
    {
        /* FIXME: partially implemented (only works for rectangular parallelograms) */
        if(srcUnit == UnitInch)
            dx = dy = (REAL) INCH_HIMETRIC;
        else if(srcUnit == UnitPixel){
            dx = ((REAL) INCH_HIMETRIC) /
                 ((REAL) GetDeviceCaps(graphics->hdc, LOGPIXELSX));
            dy = ((REAL) INCH_HIMETRIC) /
                 ((REAL) GetDeviceCaps(graphics->hdc, LOGPIXELSY));
        }
        else
            return NotImplemented;

        if(IPicture_Render(image->picture, graphics->hdc,
            pti[0].x, pti[0].y, pti[1].x - pti[0].x, pti[2].y - pti[0].y,
            srcx * dx, srcy * dy,
            srcwidth * dx, srcheight * dy,
            NULL) != S_OK){
            if(callback)
                callback(callbackData);
            return GenericError;
        }
    }
    else if (image->type == ImageTypeBitmap && ((GpBitmap*)image)->hbitmap)
    {
        GpBitmap* bitmap = (GpBitmap*)image;
        int use_software=0;

        if (srcUnit == UnitInch)
            dx = dy = 96.0; /* FIXME: use the image resolution */
        else if (srcUnit == UnitPixel)
            dx = dy = 1.0;
        else
            return NotImplemented;

        if (imageAttributes ||
            (graphics->image && graphics->image->type == ImageTypeBitmap) ||
            ptf[1].Y != ptf[0].Y || ptf[2].X != ptf[0].X)
            use_software = 1;

        if (use_software)
        {
            RECT src_area, dst_area;
            int i, x, y, stride;
            GpMatrix *dst_to_src;
            REAL m11, m12, m21, m22, mdx, mdy;
            LPBYTE data;

            src_area.left = srcx*dx;
            src_area.top = srcy*dy;
            src_area.right = (srcx+srcwidth)*dx;
            src_area.bottom = (srcy+srcheight)*dy;

            dst_area.left = dst_area.right = pti[0].x;
            dst_area.top = dst_area.bottom = pti[0].y;
            for (i=1; i<4; i++)
            {
                if (dst_area.left > pti[i].x) dst_area.left = pti[i].x;
                if (dst_area.right < pti[i].x) dst_area.right = pti[i].x;
                if (dst_area.top > pti[i].y) dst_area.top = pti[i].y;
                if (dst_area.bottom < pti[i].y) dst_area.bottom = pti[i].y;
            }

            m11 = (ptf[1].X - ptf[0].X) / srcwidth;
            m21 = (ptf[2].X - ptf[0].X) / srcheight;
            mdx = ptf[0].X - m11 * srcx - m21 * srcy;
            m12 = (ptf[1].Y - ptf[0].Y) / srcwidth;
            m22 = (ptf[2].Y - ptf[0].Y) / srcheight;
            mdy = ptf[0].Y - m12 * srcx - m22 * srcy;

            stat = GdipCreateMatrix2(m11, m12, m21, m22, mdx, mdy, &dst_to_src);
            if (stat != Ok) return stat;

            stat = GdipInvertMatrix(dst_to_src);
            if (stat != Ok)
            {
                GdipDeleteMatrix(dst_to_src);
                return stat;
            }

            data = GdipAlloc(sizeof(ARGB) * (dst_area.right - dst_area.left) * (dst_area.bottom - dst_area.top));
            if (!data)
            {
                GdipDeleteMatrix(dst_to_src);
                return OutOfMemory;
            }

            stride = sizeof(ARGB) * (dst_area.right - dst_area.left);

            for (x=dst_area.left; x<dst_area.right; x++)
            {
                for (y=dst_area.top; y<dst_area.bottom; y++)
                {
                    GpPointF src_pointf;
                    int src_x, src_y;
                    ARGB *src_color;

                    src_pointf.X = x;
                    src_pointf.Y = y;

                    GdipTransformMatrixPoints(dst_to_src, &src_pointf, 1);

                    src_x = roundr(src_pointf.X);
                    src_y = roundr(src_pointf.Y);

                    src_color = (ARGB*)(data + stride * (y - dst_area.top) + sizeof(ARGB) * (x - dst_area.left));

                    if (src_x < src_area.left || src_x >= src_area.right ||
                        src_y < src_area.top || src_y >= src_area.bottom)
                        /* FIXME: Use wrapmode */
                        *src_color = 0;
                    else
                        GdipBitmapGetPixel(bitmap, src_x, src_y, src_color);
                }
            }

            GdipDeleteMatrix(dst_to_src);

            if (imageAttributes)
            {
                if (imageAttributes->colorkeys[ColorAdjustTypeBitmap].enabled ||
                    imageAttributes->colorkeys[ColorAdjustTypeDefault].enabled)
                {
                    static int fixme;
                    if (!fixme++)
                        FIXME("Color keying not implemented\n");
                }

                if (imageAttributes->colorremaptables[ColorAdjustTypeBitmap].enabled ||
                    imageAttributes->colorremaptables[ColorAdjustTypeDefault].enabled)
                {
                    const struct color_remap_table *table;

                    if (imageAttributes->colorremaptables[ColorAdjustTypeBitmap].enabled)
                        table = &imageAttributes->colorremaptables[ColorAdjustTypeBitmap];
                    else
                        table = &imageAttributes->colorremaptables[ColorAdjustTypeDefault];

                    for (x=dst_area.left; x<dst_area.right; x++)
                        for (y=dst_area.top; y<dst_area.bottom; y++)
                        {
                            ARGB *src_color;
                            src_color = (ARGB*)(data + stride * (y - dst_area.top) + sizeof(ARGB) * (x - dst_area.left));
                            for (i=0; i<table->mapsize; i++)
                            {
                                if (*src_color == table->colormap[i].oldColor.Argb)
                                {
                                    *src_color = table->colormap[i].newColor.Argb;
                                    break;
                                }
                            }
                        }
                }

                if (imageAttributes->colormatrices[ColorAdjustTypeBitmap].enabled ||
                    imageAttributes->colormatrices[ColorAdjustTypeDefault].enabled)
                {
                    static int fixme;
                    if (!fixme++)
                        FIXME("Color transforms not implemented\n");
                }

                if (imageAttributes->gamma_enabled[ColorAdjustTypeBitmap] ||
                    imageAttributes->gamma_enabled[ColorAdjustTypeDefault])
                {
                    static int fixme;
                    if (!fixme++)
                        FIXME("Gamma adjustment not implemented\n");
                }
            }

            stat = alpha_blend_pixels(graphics, dst_area.left, dst_area.top,
                data, dst_area.right - dst_area.left, dst_area.bottom - dst_area.top, stride);

            GdipFree(data);

            return stat;
        }
        else
        {
            HDC hdc;
            int temp_hdc=0, temp_bitmap=0;
            HBITMAP hbitmap, old_hbm=NULL;

            if (!(bitmap->format == PixelFormat16bppRGB555 ||
                  bitmap->format == PixelFormat24bppRGB ||
                  bitmap->format == PixelFormat32bppRGB ||
                  bitmap->format == PixelFormat32bppPARGB))
            {
                BITMAPINFOHEADER bih;
                BYTE *temp_bits;
                PixelFormat dst_format;

                /* we can't draw a bitmap of this format directly */
                hdc = CreateCompatibleDC(0);
                temp_hdc = 1;
                temp_bitmap = 1;

                bih.biSize = sizeof(BITMAPINFOHEADER);
                bih.biWidth = bitmap->width;
                bih.biHeight = -bitmap->height;
                bih.biPlanes = 1;
                bih.biBitCount = 32;
                bih.biCompression = BI_RGB;
                bih.biSizeImage = 0;
                bih.biXPelsPerMeter = 0;
                bih.biYPelsPerMeter = 0;
                bih.biClrUsed = 0;
                bih.biClrImportant = 0;

                hbitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bih, DIB_RGB_COLORS,
                    (void**)&temp_bits, NULL, 0);

                if (bitmap->format & (PixelFormatAlpha|PixelFormatPAlpha))
                    dst_format = PixelFormat32bppPARGB;
                else
                    dst_format = PixelFormat32bppRGB;

                convert_pixels(bitmap->width, bitmap->height,
                    bitmap->width*4, temp_bits, dst_format,
                    bitmap->stride, bitmap->bits, bitmap->format, bitmap->image.palette_entries);
            }
            else
            {
                hbitmap = bitmap->hbitmap;
                hdc = bitmap->hdc;
                temp_hdc = (hdc == 0);
            }

            if (temp_hdc)
            {
                if (!hdc) hdc = CreateCompatibleDC(0);
                old_hbm = SelectObject(hdc, hbitmap);
            }

            if (bitmap->format & (PixelFormatAlpha|PixelFormatPAlpha))
            {
                BLENDFUNCTION bf;

                bf.BlendOp = AC_SRC_OVER;
                bf.BlendFlags = 0;
                bf.SourceConstantAlpha = 255;
                bf.AlphaFormat = AC_SRC_ALPHA;

                GdiAlphaBlend(graphics->hdc, pti[0].x, pti[0].y, pti[1].x-pti[0].x, pti[2].y-pti[0].y,
                    hdc, srcx*dx, srcy*dy, srcwidth*dx, srcheight*dy, bf);
            }
            else
            {
                StretchBlt(graphics->hdc, pti[0].x, pti[0].y, pti[1].x-pti[0].x, pti[2].y-pti[0].y,
                    hdc, srcx*dx, srcy*dy, srcwidth*dx, srcheight*dy, SRCCOPY);
            }

            if (temp_hdc)
            {
                SelectObject(hdc, old_hbm);
                DeleteDC(hdc);
            }

            if (temp_bitmap)
                DeleteObject(hbitmap);
        }
    }
    else
    {
        ERR("GpImage with no IPicture or HBITMAP?!\n");
        return NotImplemented;
    }

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawImagePointsRectI(GpGraphics *graphics, GpImage *image,
     GDIPCONST GpPoint *points, INT count, INT srcx, INT srcy, INT srcwidth,
     INT srcheight, GpUnit srcUnit, GDIPCONST GpImageAttributes* imageAttributes,
     DrawImageAbort callback, VOID * callbackData)
{
    GpPointF pointsF[3];
    INT i;

    TRACE("(%p, %p, %p, %d, %d, %d, %d, %d, %d, %p, %p, %p)\n", graphics, image, points, count,
          srcx, srcy, srcwidth, srcheight, srcUnit, imageAttributes, callback,
          callbackData);

    if(!points || count!=3)
        return InvalidParameter;

    for(i = 0; i < count; i++){
        pointsF[i].X = (REAL)points[i].X;
        pointsF[i].Y = (REAL)points[i].Y;
    }

    return GdipDrawImagePointsRect(graphics, image, pointsF, count, (REAL)srcx, (REAL)srcy,
                                   (REAL)srcwidth, (REAL)srcheight, srcUnit, imageAttributes,
                                   callback, callbackData);
}

GpStatus WINGDIPAPI GdipDrawImageRectRect(GpGraphics *graphics, GpImage *image,
    REAL dstx, REAL dsty, REAL dstwidth, REAL dstheight, REAL srcx, REAL srcy,
    REAL srcwidth, REAL srcheight, GpUnit srcUnit,
    GDIPCONST GpImageAttributes* imageattr, DrawImageAbort callback,
    VOID * callbackData)
{
    GpPointF points[3];

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %d, %p, %p, %p)\n",
          graphics, image, dstx, dsty, dstwidth, dstheight, srcx, srcy,
          srcwidth, srcheight, srcUnit, imageattr, callback, callbackData);

    points[0].X = dstx;
    points[0].Y = dsty;
    points[1].X = dstx + dstwidth;
    points[1].Y = dsty;
    points[2].X = dstx;
    points[2].Y = dsty + dstheight;

    return GdipDrawImagePointsRect(graphics, image, points, 3, srcx, srcy,
               srcwidth, srcheight, srcUnit, imageattr, callback, callbackData);
}

GpStatus WINGDIPAPI GdipDrawImageRectRectI(GpGraphics *graphics, GpImage *image,
	INT dstx, INT dsty, INT dstwidth, INT dstheight, INT srcx, INT srcy,
	INT srcwidth, INT srcheight, GpUnit srcUnit,
	GDIPCONST GpImageAttributes* imageAttributes, DrawImageAbort callback,
	VOID * callbackData)
{
    GpPointF points[3];

    TRACE("(%p, %p, %d, %d, %d, %d, %d, %d, %d, %d, %d, %p, %p, %p)\n",
          graphics, image, dstx, dsty, dstwidth, dstheight, srcx, srcy,
          srcwidth, srcheight, srcUnit, imageAttributes, callback, callbackData);

    points[0].X = dstx;
    points[0].Y = dsty;
    points[1].X = dstx + dstwidth;
    points[1].Y = dsty;
    points[2].X = dstx;
    points[2].Y = dsty + dstheight;

    return GdipDrawImagePointsRect(graphics, image, points, 3, srcx, srcy,
               srcwidth, srcheight, srcUnit, imageAttributes, callback, callbackData);
}

GpStatus WINGDIPAPI GdipDrawImageRect(GpGraphics *graphics, GpImage *image,
    REAL x, REAL y, REAL width, REAL height)
{
    RectF bounds;
    GpUnit unit;
    GpStatus ret;

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f)\n", graphics, image, x, y, width, height);

    if(!graphics || !image)
        return InvalidParameter;

    ret = GdipGetImageBounds(image, &bounds, &unit);
    if(ret != Ok)
        return ret;

    return GdipDrawImageRectRect(graphics, image, x, y, width, height,
                                 bounds.X, bounds.Y, bounds.Width, bounds.Height,
                                 unit, NULL, NULL, NULL);
}

GpStatus WINGDIPAPI GdipDrawImageRectI(GpGraphics *graphics, GpImage *image,
    INT x, INT y, INT width, INT height)
{
    TRACE("(%p, %p, %d, %d, %d, %d)\n", graphics, image, x, y, width, height);

    return GdipDrawImageRect(graphics, image, (REAL)x, (REAL)y, (REAL)width, (REAL)height);
}

GpStatus WINGDIPAPI GdipDrawLine(GpGraphics *graphics, GpPen *pen, REAL x1,
    REAL y1, REAL x2, REAL y2)
{
    INT save_state;
    GpPointF pt[2];
    GpStatus retval;

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f)\n", graphics, pen, x1, y1, x2, y2);

    if(!pen || !graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pt[0].X = x1;
    pt[0].Y = y1;
    pt[1].X = x2;
    pt[1].Y = y2;

    save_state = prepare_dc(graphics, pen);

    retval = draw_polyline(graphics, pen, pt, 2, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawLineI(GpGraphics *graphics, GpPen *pen, INT x1,
    INT y1, INT x2, INT y2)
{
    INT save_state;
    GpPointF pt[2];
    GpStatus retval;

    TRACE("(%p, %p, %d, %d, %d, %d)\n", graphics, pen, x1, y1, x2, y2);

    if(!pen || !graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pt[0].X = (REAL)x1;
    pt[0].Y = (REAL)y1;
    pt[1].X = (REAL)x2;
    pt[1].Y = (REAL)y2;

    save_state = prepare_dc(graphics, pen);

    retval = draw_polyline(graphics, pen, pt, 2, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawLines(GpGraphics *graphics, GpPen *pen, GDIPCONST
    GpPointF *points, INT count)
{
    INT save_state;
    GpStatus retval;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    if(!pen || !graphics || (count < 2))
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    save_state = prepare_dc(graphics, pen);

    retval = draw_polyline(graphics, pen, points, count, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawLinesI(GpGraphics *graphics, GpPen *pen, GDIPCONST
    GpPoint *points, INT count)
{
    INT save_state;
    GpStatus retval;
    GpPointF *ptf = NULL;
    int i;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    if(!pen || !graphics || (count < 2))
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf = GdipAlloc(count * sizeof(GpPointF));
    if(!ptf) return OutOfMemory;

    for(i = 0; i < count; i ++){
        ptf[i].X = (REAL) points[i].X;
        ptf[i].Y = (REAL) points[i].Y;
    }

    save_state = prepare_dc(graphics, pen);

    retval = draw_polyline(graphics, pen, ptf, count, TRUE);

    restore_dc(graphics, save_state);

    GdipFree(ptf);
    return retval;
}

GpStatus WINGDIPAPI GdipDrawPath(GpGraphics *graphics, GpPen *pen, GpPath *path)
{
    INT save_state;
    GpStatus retval;

    TRACE("(%p, %p, %p)\n", graphics, pen, path);

    if(!pen || !graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    save_state = prepare_dc(graphics, pen);

    retval = draw_poly(graphics, pen, path->pathdata.Points,
                       path->pathdata.Types, path->pathdata.Count, TRUE);

    restore_dc(graphics, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipDrawPie(GpGraphics *graphics, GpPen *pen, REAL x,
    REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    INT save_state;

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f)\n", graphics, pen, x, y,
            width, height, startAngle, sweepAngle);

    if(!graphics || !pen)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    save_state = prepare_dc(graphics, pen);
    SelectObject(graphics->hdc, GetStockObject(NULL_BRUSH));

    draw_pie(graphics, x, y, width, height, startAngle, sweepAngle);

    restore_dc(graphics, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawPieI(GpGraphics *graphics, GpPen *pen, INT x,
    INT y, INT width, INT height, REAL startAngle, REAL sweepAngle)
{
    TRACE("(%p, %p, %d, %d, %d, %d, %.2f, %.2f)\n", graphics, pen, x, y,
            width, height, startAngle, sweepAngle);

    return GdipDrawPie(graphics,pen,(REAL)x,(REAL)y,(REAL)width,(REAL)height,startAngle,sweepAngle);
}

GpStatus WINGDIPAPI GdipDrawRectangle(GpGraphics *graphics, GpPen *pen, REAL x,
    REAL y, REAL width, REAL height)
{
    INT save_state;
    GpPointF ptf[4];
    POINT pti[4];

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f)\n", graphics, pen, x, y, width, height);

    if(!pen || !graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf[0].X = x;
    ptf[0].Y = y;
    ptf[1].X = x + width;
    ptf[1].Y = y;
    ptf[2].X = x + width;
    ptf[2].Y = y + height;
    ptf[3].X = x;
    ptf[3].Y = y + height;

    save_state = prepare_dc(graphics, pen);
    SelectObject(graphics->hdc, GetStockObject(NULL_BRUSH));

    transform_and_round_points(graphics, pti, ptf, 4);
    Polygon(graphics->hdc, pti, 4);

    restore_dc(graphics, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawRectangleI(GpGraphics *graphics, GpPen *pen, INT x,
    INT y, INT width, INT height)
{
    TRACE("(%p, %p, %d, %d, %d, %d)\n", graphics, pen, x, y, width, height);

    return GdipDrawRectangle(graphics,pen,(REAL)x,(REAL)y,(REAL)width,(REAL)height);
}

GpStatus WINGDIPAPI GdipDrawRectangles(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpRectF* rects, INT count)
{
    GpPointF *ptf;
    POINT *pti;
    INT save_state, i;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, rects, count);

    if(!graphics || !pen || !rects || count < 1)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf = GdipAlloc(4 * count * sizeof(GpPointF));
    pti = GdipAlloc(4 * count * sizeof(POINT));

    if(!ptf || !pti){
        GdipFree(ptf);
        GdipFree(pti);
        return OutOfMemory;
    }

    for(i = 0; i < count; i++){
        ptf[4 * i + 3].X = ptf[4 * i].X = rects[i].X;
        ptf[4 * i + 1].Y = ptf[4 * i].Y = rects[i].Y;
        ptf[4 * i + 2].X = ptf[4 * i + 1].X = rects[i].X + rects[i].Width;
        ptf[4 * i + 3].Y = ptf[4 * i + 2].Y = rects[i].Y + rects[i].Height;
    }

    save_state = prepare_dc(graphics, pen);
    SelectObject(graphics->hdc, GetStockObject(NULL_BRUSH));

    transform_and_round_points(graphics, pti, ptf, 4 * count);

    for(i = 0; i < count; i++)
        Polygon(graphics->hdc, &pti[4 * i], 4);

    restore_dc(graphics, save_state);

    GdipFree(ptf);
    GdipFree(pti);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawRectanglesI(GpGraphics *graphics, GpPen *pen,
    GDIPCONST GpRect* rects, INT count)
{
    GpRectF *rectsF;
    GpStatus ret;
    INT i;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, rects, count);

    if(!rects || count<=0)
        return InvalidParameter;

    rectsF = GdipAlloc(sizeof(GpRectF) * count);
    if(!rectsF)
        return OutOfMemory;

    for(i = 0;i < count;i++){
        rectsF[i].X      = (REAL)rects[i].X;
        rectsF[i].Y      = (REAL)rects[i].Y;
        rectsF[i].Width  = (REAL)rects[i].Width;
        rectsF[i].Height = (REAL)rects[i].Height;
    }

    ret = GdipDrawRectangles(graphics, pen, rectsF, count);
    GdipFree(rectsF);

    return ret;
}

GpStatus WINGDIPAPI GdipFillClosedCurve2(GpGraphics *graphics, GpBrush *brush,
    GDIPCONST GpPointF *points, INT count, REAL tension, GpFillMode fill)
{
    GpPath *path;
    GpStatus stat;

    TRACE("(%p, %p, %p, %d, %.2f, %d)\n", graphics, brush, points,
            count, tension, fill);

    if(!graphics || !brush || !points)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    stat = GdipCreatePath(fill, &path);
    if(stat != Ok)
        return stat;

    stat = GdipAddPathClosedCurve2(path, points, count, tension);
    if(stat != Ok){
        GdipDeletePath(path);
        return stat;
    }

    stat = GdipFillPath(graphics, brush, path);
    if(stat != Ok){
        GdipDeletePath(path);
        return stat;
    }

    GdipDeletePath(path);

    return Ok;
}

GpStatus WINGDIPAPI GdipFillClosedCurve2I(GpGraphics *graphics, GpBrush *brush,
    GDIPCONST GpPoint *points, INT count, REAL tension, GpFillMode fill)
{
    GpPointF *ptf;
    GpStatus stat;
    INT i;

    TRACE("(%p, %p, %p, %d, %.2f, %d)\n", graphics, brush, points,
            count, tension, fill);

    if(!points || count <= 0)
        return InvalidParameter;

    ptf = GdipAlloc(sizeof(GpPointF)*count);
    if(!ptf)
        return OutOfMemory;

    for(i = 0;i < count;i++){
        ptf[i].X = (REAL)points[i].X;
        ptf[i].Y = (REAL)points[i].Y;
    }

    stat = GdipFillClosedCurve2(graphics, brush, ptf, count, tension, fill);

    GdipFree(ptf);

    return stat;
}

GpStatus WINGDIPAPI GdipFillEllipse(GpGraphics *graphics, GpBrush *brush, REAL x,
    REAL y, REAL width, REAL height)
{
    INT save_state;
    GpPointF ptf[2];
    POINT pti[2];

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f)\n", graphics, brush, x, y, width, height);

    if(!graphics || !brush)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf[0].X = x;
    ptf[0].Y = y;
    ptf[1].X = x + width;
    ptf[1].Y = y + height;

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);

    transform_and_round_points(graphics, pti, ptf, 2);

    BeginPath(graphics->hdc);
    Ellipse(graphics->hdc, pti[0].x, pti[0].y, pti[1].x, pti[1].y);
    EndPath(graphics->hdc);

    brush_fill_path(graphics, brush);

    RestoreDC(graphics->hdc, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipFillEllipseI(GpGraphics *graphics, GpBrush *brush, INT x,
    INT y, INT width, INT height)
{
    TRACE("(%p, %p, %d, %d, %d, %d)\n", graphics, brush, x, y, width, height);

    return GdipFillEllipse(graphics,brush,(REAL)x,(REAL)y,(REAL)width,(REAL)height);
}

GpStatus WINGDIPAPI GdipFillPath(GpGraphics *graphics, GpBrush *brush, GpPath *path)
{
    INT save_state;
    GpStatus retval;

    TRACE("(%p, %p, %p)\n", graphics, brush, path);

    if(!brush || !graphics || !path)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);
    SetPolyFillMode(graphics->hdc, (path->fill == FillModeAlternate ? ALTERNATE
                                                                    : WINDING));

    BeginPath(graphics->hdc);
    retval = draw_poly(graphics, NULL, path->pathdata.Points,
                       path->pathdata.Types, path->pathdata.Count, FALSE);

    if(retval != Ok)
        goto end;

    EndPath(graphics->hdc);
    brush_fill_path(graphics, brush);

    retval = Ok;

end:
    RestoreDC(graphics->hdc, save_state);

    return retval;
}

GpStatus WINGDIPAPI GdipFillPie(GpGraphics *graphics, GpBrush *brush, REAL x,
    REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    INT save_state;

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f)\n",
            graphics, brush, x, y, width, height, startAngle, sweepAngle);

    if(!graphics || !brush)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);

    BeginPath(graphics->hdc);
    draw_pie(graphics, x, y, width, height, startAngle, sweepAngle);
    EndPath(graphics->hdc);

    brush_fill_path(graphics, brush);

    RestoreDC(graphics->hdc, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipFillPieI(GpGraphics *graphics, GpBrush *brush, INT x,
    INT y, INT width, INT height, REAL startAngle, REAL sweepAngle)
{
    TRACE("(%p, %p, %d, %d, %d, %d, %.2f, %.2f)\n",
            graphics, brush, x, y, width, height, startAngle, sweepAngle);

    return GdipFillPie(graphics,brush,(REAL)x,(REAL)y,(REAL)width,(REAL)height,startAngle,sweepAngle);
}

GpStatus WINGDIPAPI GdipFillPolygon(GpGraphics *graphics, GpBrush *brush,
    GDIPCONST GpPointF *points, INT count, GpFillMode fillMode)
{
    INT save_state;
    GpPointF *ptf = NULL;
    POINT *pti = NULL;
    GpStatus retval = Ok;

    TRACE("(%p, %p, %p, %d, %d)\n", graphics, brush, points, count, fillMode);

    if(!graphics || !brush || !points || !count)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf = GdipAlloc(count * sizeof(GpPointF));
    pti = GdipAlloc(count * sizeof(POINT));
    if(!ptf || !pti){
        retval = OutOfMemory;
        goto end;
    }

    memcpy(ptf, points, count * sizeof(GpPointF));

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);
    SetPolyFillMode(graphics->hdc, (fillMode == FillModeAlternate ? ALTERNATE
                                                                  : WINDING));

    transform_and_round_points(graphics, pti, ptf, count);

    BeginPath(graphics->hdc);
    Polygon(graphics->hdc, pti, count);
    EndPath(graphics->hdc);

    brush_fill_path(graphics, brush);

    RestoreDC(graphics->hdc, save_state);

end:
    GdipFree(ptf);
    GdipFree(pti);

    return retval;
}

GpStatus WINGDIPAPI GdipFillPolygonI(GpGraphics *graphics, GpBrush *brush,
    GDIPCONST GpPoint *points, INT count, GpFillMode fillMode)
{
    INT save_state, i;
    GpPointF *ptf = NULL;
    POINT *pti = NULL;
    GpStatus retval = Ok;

    TRACE("(%p, %p, %p, %d, %d)\n", graphics, brush, points, count, fillMode);

    if(!graphics || !brush || !points || !count)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf = GdipAlloc(count * sizeof(GpPointF));
    pti = GdipAlloc(count * sizeof(POINT));
    if(!ptf || !pti){
        retval = OutOfMemory;
        goto end;
    }

    for(i = 0; i < count; i ++){
        ptf[i].X = (REAL) points[i].X;
        ptf[i].Y = (REAL) points[i].Y;
    }

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);
    SetPolyFillMode(graphics->hdc, (fillMode == FillModeAlternate ? ALTERNATE
                                                                  : WINDING));

    transform_and_round_points(graphics, pti, ptf, count);

    BeginPath(graphics->hdc);
    Polygon(graphics->hdc, pti, count);
    EndPath(graphics->hdc);

    brush_fill_path(graphics, brush);

    RestoreDC(graphics->hdc, save_state);

end:
    GdipFree(ptf);
    GdipFree(pti);

    return retval;
}

GpStatus WINGDIPAPI GdipFillPolygon2(GpGraphics *graphics, GpBrush *brush,
    GDIPCONST GpPointF *points, INT count)
{
    TRACE("(%p, %p, %p, %d)\n", graphics, brush, points, count);

    return GdipFillPolygon(graphics, brush, points, count, FillModeAlternate);
}

GpStatus WINGDIPAPI GdipFillPolygon2I(GpGraphics *graphics, GpBrush *brush,
    GDIPCONST GpPoint *points, INT count)
{
    TRACE("(%p, %p, %p, %d)\n", graphics, brush, points, count);

    return GdipFillPolygonI(graphics, brush, points, count, FillModeAlternate);
}

GpStatus WINGDIPAPI GdipFillRectangle(GpGraphics *graphics, GpBrush *brush,
    REAL x, REAL y, REAL width, REAL height)
{
    INT save_state;
    GpPointF ptf[4];
    POINT pti[4];

    TRACE("(%p, %p, %.2f, %.2f, %.2f, %.2f)\n", graphics, brush, x, y, width, height);

    if(!graphics || !brush)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf[0].X = x;
    ptf[0].Y = y;
    ptf[1].X = x + width;
    ptf[1].Y = y;
    ptf[2].X = x + width;
    ptf[2].Y = y + height;
    ptf[3].X = x;
    ptf[3].Y = y + height;

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);

    transform_and_round_points(graphics, pti, ptf, 4);

    BeginPath(graphics->hdc);
    Polygon(graphics->hdc, pti, 4);
    EndPath(graphics->hdc);

    brush_fill_path(graphics, brush);

    RestoreDC(graphics->hdc, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipFillRectangleI(GpGraphics *graphics, GpBrush *brush,
    INT x, INT y, INT width, INT height)
{
    INT save_state;
    GpPointF ptf[4];
    POINT pti[4];

    TRACE("(%p, %p, %d, %d, %d, %d)\n", graphics, brush, x, y, width, height);

    if(!graphics || !brush)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    ptf[0].X = x;
    ptf[0].Y = y;
    ptf[1].X = x + width;
    ptf[1].Y = y;
    ptf[2].X = x + width;
    ptf[2].Y = y + height;
    ptf[3].X = x;
    ptf[3].Y = y + height;

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);

    transform_and_round_points(graphics, pti, ptf, 4);

    BeginPath(graphics->hdc);
    Polygon(graphics->hdc, pti, 4);
    EndPath(graphics->hdc);

    brush_fill_path(graphics, brush);

    RestoreDC(graphics->hdc, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipFillRectangles(GpGraphics *graphics, GpBrush *brush, GDIPCONST GpRectF *rects,
    INT count)
{
    GpStatus ret;
    INT i;

    TRACE("(%p, %p, %p, %d)\n", graphics, brush, rects, count);

    if(!rects)
        return InvalidParameter;

    for(i = 0; i < count; i++){
        ret = GdipFillRectangle(graphics, brush, rects[i].X, rects[i].Y, rects[i].Width, rects[i].Height);
        if(ret != Ok)   return ret;
    }

    return Ok;
}

GpStatus WINGDIPAPI GdipFillRectanglesI(GpGraphics *graphics, GpBrush *brush, GDIPCONST GpRect *rects,
    INT count)
{
    GpRectF *rectsF;
    GpStatus ret;
    INT i;

    TRACE("(%p, %p, %p, %d)\n", graphics, brush, rects, count);

    if(!rects || count <= 0)
        return InvalidParameter;

    rectsF = GdipAlloc(sizeof(GpRectF)*count);
    if(!rectsF)
        return OutOfMemory;

    for(i = 0; i < count; i++){
        rectsF[i].X      = (REAL)rects[i].X;
        rectsF[i].Y      = (REAL)rects[i].Y;
        rectsF[i].X      = (REAL)rects[i].Width;
        rectsF[i].Height = (REAL)rects[i].Height;
    }

    ret = GdipFillRectangles(graphics,brush,rectsF,count);
    GdipFree(rectsF);

    return ret;
}

/*****************************************************************************
 * GdipFillRegion [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipFillRegion(GpGraphics* graphics, GpBrush* brush,
        GpRegion* region)
{
    INT save_state;
    GpStatus status;
    HRGN hrgn;
    RECT rc;

    TRACE("(%p, %p, %p)\n", graphics, brush, region);

    if (!(graphics && brush && region))
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    status = GdipGetRegionHRgn(region, graphics, &hrgn);
    if(status != Ok)
        return status;

    save_state = SaveDC(graphics->hdc);
    EndPath(graphics->hdc);

    ExtSelectClipRgn(graphics->hdc, hrgn, RGN_AND);

    if (GetClipBox(graphics->hdc, &rc) != NULLREGION)
    {
        BeginPath(graphics->hdc);
        Rectangle(graphics->hdc, rc.left, rc.top, rc.right, rc.bottom);
        EndPath(graphics->hdc);

        brush_fill_path(graphics, brush);
    }

    RestoreDC(graphics->hdc, save_state);

    DeleteObject(hrgn);

    return Ok;
}

GpStatus WINGDIPAPI GdipFlush(GpGraphics *graphics, GpFlushIntention intention)
{
    TRACE("(%p,%u)\n", graphics, intention);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    /* We have no internal operation queue, so there's no need to clear it. */

    if (graphics->hdc)
        GdiFlush();

    return Ok;
}

/*****************************************************************************
 * GdipGetClipBounds [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipGetClipBounds(GpGraphics *graphics, GpRectF *rect)
{
    TRACE("(%p, %p)\n", graphics, rect);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipGetRegionBounds(graphics->clip, graphics, rect);
}

/*****************************************************************************
 * GdipGetClipBoundsI [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipGetClipBoundsI(GpGraphics *graphics, GpRect *rect)
{
    TRACE("(%p, %p)\n", graphics, rect);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipGetRegionBoundsI(graphics->clip, graphics, rect);
}

/* FIXME: Compositing mode is not used anywhere except the getter/setter. */
GpStatus WINGDIPAPI GdipGetCompositingMode(GpGraphics *graphics,
    CompositingMode *mode)
{
    TRACE("(%p, %p)\n", graphics, mode);

    if(!graphics || !mode)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *mode = graphics->compmode;

    return Ok;
}

/* FIXME: Compositing quality is not used anywhere except the getter/setter. */
GpStatus WINGDIPAPI GdipGetCompositingQuality(GpGraphics *graphics,
    CompositingQuality *quality)
{
    TRACE("(%p, %p)\n", graphics, quality);

    if(!graphics || !quality)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *quality = graphics->compqual;

    return Ok;
}

/* FIXME: Interpolation mode is not used anywhere except the getter/setter. */
GpStatus WINGDIPAPI GdipGetInterpolationMode(GpGraphics *graphics,
    InterpolationMode *mode)
{
    TRACE("(%p, %p)\n", graphics, mode);

    if(!graphics || !mode)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *mode = graphics->interpolation;

    return Ok;
}

/* FIXME: Need to handle color depths less than 24bpp */
GpStatus WINGDIPAPI GdipGetNearestColor(GpGraphics *graphics, ARGB* argb)
{
    FIXME("(%p, %p): Passing color unmodified\n", graphics, argb);

    if(!graphics || !argb)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetPageScale(GpGraphics *graphics, REAL *scale)
{
    TRACE("(%p, %p)\n", graphics, scale);

    if(!graphics || !scale)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *scale = graphics->scale;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetPageUnit(GpGraphics *graphics, GpUnit *unit)
{
    TRACE("(%p, %p)\n", graphics, unit);

    if(!graphics || !unit)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *unit = graphics->unit;

    return Ok;
}

/* FIXME: Pixel offset mode is not used anywhere except the getter/setter. */
GpStatus WINGDIPAPI GdipGetPixelOffsetMode(GpGraphics *graphics, PixelOffsetMode
    *mode)
{
    TRACE("(%p, %p)\n", graphics, mode);

    if(!graphics || !mode)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *mode = graphics->pixeloffset;

    return Ok;
}

/* FIXME: Smoothing mode is not used anywhere except the getter/setter. */
GpStatus WINGDIPAPI GdipGetSmoothingMode(GpGraphics *graphics, SmoothingMode *mode)
{
    TRACE("(%p, %p)\n", graphics, mode);

    if(!graphics || !mode)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *mode = graphics->smoothing;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetTextContrast(GpGraphics *graphics, UINT *contrast)
{
    TRACE("(%p, %p)\n", graphics, contrast);

    if(!graphics || !contrast)
        return InvalidParameter;

    *contrast = graphics->textcontrast;

    return Ok;
}

/* FIXME: Text rendering hint is not used anywhere except the getter/setter. */
GpStatus WINGDIPAPI GdipGetTextRenderingHint(GpGraphics *graphics,
    TextRenderingHint *hint)
{
    TRACE("(%p, %p)\n", graphics, hint);

    if(!graphics || !hint)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *hint = graphics->texthint;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetVisibleClipBounds(GpGraphics *graphics, GpRectF *rect)
{
    GpRegion *clip_rgn;
    GpStatus stat;

    TRACE("(%p, %p)\n", graphics, rect);

    if(!graphics || !rect)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    /* intersect window and graphics clipping regions */
    if((stat = GdipCreateRegion(&clip_rgn)) != Ok)
        return stat;

    if((stat = get_visible_clip_region(graphics, clip_rgn)) != Ok)
        goto cleanup;

    /* get bounds of the region */
    stat = GdipGetRegionBounds(clip_rgn, graphics, rect);

cleanup:
    GdipDeleteRegion(clip_rgn);

    return stat;
}

GpStatus WINGDIPAPI GdipGetVisibleClipBoundsI(GpGraphics *graphics, GpRect *rect)
{
    GpRectF rectf;
    GpStatus stat;

    TRACE("(%p, %p)\n", graphics, rect);

    if(!graphics || !rect)
        return InvalidParameter;

    if((stat = GdipGetVisibleClipBounds(graphics, &rectf)) == Ok)
    {
        rect->X = roundr(rectf.X);
        rect->Y = roundr(rectf.Y);
        rect->Width  = roundr(rectf.Width);
        rect->Height = roundr(rectf.Height);
    }

    return stat;
}

GpStatus WINGDIPAPI GdipGetWorldTransform(GpGraphics *graphics, GpMatrix *matrix)
{
    TRACE("(%p, %p)\n", graphics, matrix);

    if(!graphics || !matrix)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *matrix = *graphics->worldtrans;
    return Ok;
}

GpStatus WINGDIPAPI GdipGraphicsClear(GpGraphics *graphics, ARGB color)
{
    GpSolidFill *brush;
    GpStatus stat;
    GpRectF wnd_rect;

    TRACE("(%p, %x)\n", graphics, color);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    if((stat = GdipCreateSolidFill(color, &brush)) != Ok)
        return stat;

    if((stat = get_graphics_bounds(graphics, &wnd_rect)) != Ok){
        GdipDeleteBrush((GpBrush*)brush);
        return stat;
    }

    GdipFillRectangle(graphics, (GpBrush*)brush, wnd_rect.X, wnd_rect.Y,
                                                 wnd_rect.Width, wnd_rect.Height);

    GdipDeleteBrush((GpBrush*)brush);

    return Ok;
}

GpStatus WINGDIPAPI GdipIsClipEmpty(GpGraphics *graphics, BOOL *res)
{
    TRACE("(%p, %p)\n", graphics, res);

    if(!graphics || !res)
        return InvalidParameter;

    return GdipIsEmptyRegion(graphics->clip, graphics, res);
}

GpStatus WINGDIPAPI GdipIsVisiblePoint(GpGraphics *graphics, REAL x, REAL y, BOOL *result)
{
    GpStatus stat;
    GpRegion* rgn;
    GpPointF pt;

    TRACE("(%p, %.2f, %.2f, %p)\n", graphics, x, y, result);

    if(!graphics || !result)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pt.X = x;
    pt.Y = y;
    if((stat = GdipTransformPoints(graphics, CoordinateSpaceDevice,
                   CoordinateSpaceWorld, &pt, 1)) != Ok)
        return stat;

    if((stat = GdipCreateRegion(&rgn)) != Ok)
        return stat;

    if((stat = get_visible_clip_region(graphics, rgn)) != Ok)
        goto cleanup;

    stat = GdipIsVisibleRegionPoint(rgn, pt.X, pt.Y, graphics, result);

cleanup:
    GdipDeleteRegion(rgn);
    return stat;
}

GpStatus WINGDIPAPI GdipIsVisiblePointI(GpGraphics *graphics, INT x, INT y, BOOL *result)
{
    return GdipIsVisiblePoint(graphics, (REAL)x, (REAL)y, result);
}

GpStatus WINGDIPAPI GdipIsVisibleRect(GpGraphics *graphics, REAL x, REAL y, REAL width, REAL height, BOOL *result)
{
    GpStatus stat;
    GpRegion* rgn;
    GpPointF pts[2];

    TRACE("(%p %.2f %.2f %.2f %.2f %p)\n", graphics, x, y, width, height, result);

    if(!graphics || !result)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pts[0].X = x;
    pts[0].Y = y;
    pts[1].X = x + width;
    pts[1].Y = y + height;

    if((stat = GdipTransformPoints(graphics, CoordinateSpaceDevice,
                    CoordinateSpaceWorld, pts, 2)) != Ok)
        return stat;

    pts[1].X -= pts[0].X;
    pts[1].Y -= pts[0].Y;

    if((stat = GdipCreateRegion(&rgn)) != Ok)
        return stat;

    if((stat = get_visible_clip_region(graphics, rgn)) != Ok)
        goto cleanup;

    stat = GdipIsVisibleRegionRect(rgn, pts[0].X, pts[0].Y, pts[1].X, pts[1].Y, graphics, result);

cleanup:
    GdipDeleteRegion(rgn);
    return stat;
}

GpStatus WINGDIPAPI GdipIsVisibleRectI(GpGraphics *graphics, INT x, INT y, INT width, INT height, BOOL *result)
{
    return GdipIsVisibleRect(graphics, (REAL)x, (REAL)y, (REAL)width, (REAL)height, result);
}

typedef GpStatus (*gdip_format_string_callback)(GpGraphics *graphics,
    GDIPCONST WCHAR *string, INT index, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    INT lineno, const RectF *bounds, void *user_data);

static GpStatus gdip_format_string(GpGraphics *graphics,
    GDIPCONST WCHAR *string, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    gdip_format_string_callback callback, void *user_data)
{
    WCHAR* stringdup;
    INT sum = 0, height = 0, fit, fitcpy, i, j, lret, nwidth,
        nheight, lineend, lineno = 0;
    RectF bounds;
    StringAlignment halign;
    GpStatus stat = Ok;
    SIZE size;

    if(length == -1) length = lstrlenW(string);

    stringdup = GdipAlloc((length + 1) * sizeof(WCHAR));
    if(!stringdup) return OutOfMemory;

    nwidth = roundr(rect->Width);
    nheight = roundr(rect->Height);

    if (nwidth == 0) nwidth = INT_MAX;
    if (nheight == 0) nheight = INT_MAX;

    for(i = 0, j = 0; i < length; i++){
        /* FIXME: This makes the indexes passed to callback inaccurate. */
        if(!isprintW(string[i]) && (string[i] != '\n'))
            continue;

        stringdup[j] = string[i];
        j++;
    }

    length = j;

    if (format) halign = format->align;
    else halign = StringAlignmentNear;

    while(sum < length){
        GetTextExtentExPointW(graphics->hdc, stringdup + sum, length - sum,
                              nwidth, &fit, NULL, &size);
        fitcpy = fit;

        if(fit == 0)
            break;

        for(lret = 0; lret < fit; lret++)
            if(*(stringdup + sum + lret) == '\n')
                break;

        /* Line break code (may look strange, but it imitates windows). */
        if(lret < fit)
            lineend = fit = lret;    /* this is not an off-by-one error */
        else if(fit < (length - sum)){
            if(*(stringdup + sum + fit) == ' ')
                while(*(stringdup + sum + fit) == ' ')
                    fit++;
            else
                while(*(stringdup + sum + fit - 1) != ' '){
                    fit--;

                    if(*(stringdup + sum + fit) == '\t')
                        break;

                    if(fit == 0){
                        fit = fitcpy;
                        break;
                    }
                }
            lineend = fit;
            while(*(stringdup + sum + lineend - 1) == ' ' ||
                  *(stringdup + sum + lineend - 1) == '\t')
                lineend--;
        }
        else
            lineend = fit;

        GetTextExtentExPointW(graphics->hdc, stringdup + sum, lineend,
                              nwidth, &j, NULL, &size);

        bounds.Width = size.cx;

        if(height + size.cy > nheight)
            bounds.Height = nheight - (height + size.cy);
        else
            bounds.Height = size.cy;

        bounds.Y = rect->Y + height;

        switch (halign)
        {
        case StringAlignmentNear:
        default:
            bounds.X = rect->X;
            break;
        case StringAlignmentCenter:
            bounds.X = rect->X + (rect->Width/2) - (bounds.Width/2);
            break;
        case StringAlignmentFar:
            bounds.X = rect->X + rect->Width - bounds.Width;
            break;
        }

        stat = callback(graphics, stringdup, sum, lineend,
            font, rect, format, lineno, &bounds, user_data);

        if (stat != Ok)
            break;

        sum += fit + (lret < fitcpy ? 1 : 0);
        height += size.cy;
        lineno++;

        if(height > nheight)
            break;

        /* Stop if this was a linewrap (but not if it was a linebreak). */
        if((lret == fitcpy) && format && (format->attr & StringFormatFlagsNoWrap))
            break;
    }

    GdipFree(stringdup);

    return stat;
}

struct measure_ranges_args {
    GpRegion **regions;
};

GpStatus measure_ranges_callback(GpGraphics *graphics,
    GDIPCONST WCHAR *string, INT index, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    INT lineno, const RectF *bounds, void *user_data)
{
    int i;
    GpStatus stat = Ok;
    struct measure_ranges_args *args = user_data;

    for (i=0; i<format->range_count; i++)
    {
        INT range_start = max(index, format->character_ranges[i].First);
        INT range_end = min(index+length, format->character_ranges[i].First+format->character_ranges[i].Length);
        if (range_start < range_end)
        {
            GpRectF range_rect;
            SIZE range_size;

            range_rect.Y = bounds->Y;
            range_rect.Height = bounds->Height;

            GetTextExtentExPointW(graphics->hdc, string + index, range_start - index,
                                  INT_MAX, NULL, NULL, &range_size);
            range_rect.X = bounds->X + range_size.cx;

            GetTextExtentExPointW(graphics->hdc, string + index, range_end - index,
                                  INT_MAX, NULL, NULL, &range_size);
            range_rect.Width = (bounds->X + range_size.cx) - range_rect.X;

            stat = GdipCombineRegionRect(args->regions[i], &range_rect, CombineModeUnion);
            if (stat != Ok)
                break;
        }
    }

    return stat;
}

GpStatus WINGDIPAPI GdipMeasureCharacterRanges(GpGraphics* graphics,
        GDIPCONST WCHAR* string, INT length, GDIPCONST GpFont* font,
        GDIPCONST RectF* layoutRect, GDIPCONST GpStringFormat *stringFormat,
        INT regionCount, GpRegion** regions)
{
    GpStatus stat;
    int i;
    HFONT oldfont;
    struct measure_ranges_args args;

    TRACE("(%p %s %d %p %s %p %d %p)\n", graphics, debugstr_w(string),
            length, font, debugstr_rectf(layoutRect), stringFormat, regionCount, regions);

    if (!(graphics && string && font && layoutRect && stringFormat && regions))
        return InvalidParameter;

    if (regionCount < stringFormat->range_count)
        return InvalidParameter;

    if (stringFormat->attr)
        TRACE("may be ignoring some format flags: attr %x\n", stringFormat->attr);

    oldfont = SelectObject(graphics->hdc, CreateFontIndirectW(&font->lfw));

    for (i=0; i<stringFormat->range_count; i++)
    {
        stat = GdipSetEmpty(regions[i]);
        if (stat != Ok)
            return stat;
    }

    args.regions = regions;

    stat = gdip_format_string(graphics, string, length, font, layoutRect, stringFormat,
        measure_ranges_callback, &args);

    DeleteObject(SelectObject(graphics->hdc, oldfont));

    return stat;
}

struct measure_string_args {
    RectF *bounds;
    INT *codepointsfitted;
    INT *linesfilled;
};

static GpStatus measure_string_callback(GpGraphics *graphics,
    GDIPCONST WCHAR *string, INT index, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    INT lineno, const RectF *bounds, void *user_data)
{
    struct measure_string_args *args = user_data;

    if (bounds->Width > args->bounds->Width)
        args->bounds->Width = bounds->Width;

    if (bounds->Height + bounds->Y > args->bounds->Height + args->bounds->Y)
        args->bounds->Height = bounds->Height + bounds->Y - args->bounds->Y;

    if (args->codepointsfitted)
        *args->codepointsfitted = index + length;

    if (args->linesfilled)
        (*args->linesfilled)++;

    return Ok;
}

/* Find the smallest rectangle that bounds the text when it is printed in rect
 * according to the format options listed in format. If rect has 0 width and
 * height, then just find the smallest rectangle that bounds the text when it's
 * printed at location (rect->X, rect-Y). */
GpStatus WINGDIPAPI GdipMeasureString(GpGraphics *graphics,
    GDIPCONST WCHAR *string, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format, RectF *bounds,
    INT *codepointsfitted, INT *linesfilled)
{
    HFONT oldfont;
    struct measure_string_args args;

    TRACE("(%p, %s, %i, %p, %s, %p, %p, %p, %p)\n", graphics,
        debugstr_wn(string, length), length, font, debugstr_rectf(rect), format,
        bounds, codepointsfitted, linesfilled);

    if(!graphics || !string || !font || !rect || !bounds)
        return InvalidParameter;

    if(linesfilled) *linesfilled = 0;
    if(codepointsfitted) *codepointsfitted = 0;

    if(format)
        TRACE("may be ignoring some format flags: attr %x\n", format->attr);

    oldfont = SelectObject(graphics->hdc, CreateFontIndirectW(&font->lfw));

    bounds->X = rect->X;
    bounds->Y = rect->Y;
    bounds->Width = 0.0;
    bounds->Height = 0.0;

    args.bounds = bounds;
    args.codepointsfitted = codepointsfitted;
    args.linesfilled = linesfilled;

    gdip_format_string(graphics, string, length, font, rect, format,
        measure_string_callback, &args);

    DeleteObject(SelectObject(graphics->hdc, oldfont));

    return Ok;
}

struct draw_string_args {
    POINT drawbase;
    UINT drawflags;
    REAL ang_cos, ang_sin;
};

static GpStatus draw_string_callback(GpGraphics *graphics,
    GDIPCONST WCHAR *string, INT index, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    INT lineno, const RectF *bounds, void *user_data)
{
    struct draw_string_args *args = user_data;
    RECT drawcoord;

    drawcoord.left = drawcoord.right = args->drawbase.x + roundr(args->ang_sin * bounds->Y);
    drawcoord.top = drawcoord.bottom = args->drawbase.y + roundr(args->ang_cos * bounds->Y);

    DrawTextW(graphics->hdc, string + index, length, &drawcoord, args->drawflags);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawString(GpGraphics *graphics, GDIPCONST WCHAR *string,
    INT length, GDIPCONST GpFont *font, GDIPCONST RectF *rect,
    GDIPCONST GpStringFormat *format, GDIPCONST GpBrush *brush)
{
    HRGN rgn = NULL;
    HFONT gdifont;
    LOGFONTW lfw;
    TEXTMETRICW textmet;
    GpPointF pt[3], rectcpy[4];
    POINT corners[4];
    REAL angle, rel_width, rel_height;
    INT offsety = 0, save_state;
    struct draw_string_args args;
    RectF scaled_rect;

    TRACE("(%p, %s, %i, %p, %s, %p, %p)\n", graphics, debugstr_wn(string, length),
        length, font, debugstr_rectf(rect), format, brush);

    if(!graphics || !string || !font || !brush || !rect)
        return InvalidParameter;

    if((brush->bt != BrushTypeSolidColor)){
        FIXME("not implemented for given parameters\n");
        return NotImplemented;
    }

    if(format){
        TRACE("may be ignoring some format flags: attr %x\n", format->attr);

        /* Should be no need to explicitly test for StringAlignmentNear as
         * that is default behavior if no alignment is passed. */
        if(format->vertalign != StringAlignmentNear){
            RectF bounds;
            GdipMeasureString(graphics, string, length, font, rect, format, &bounds, 0, 0);

            if(format->vertalign == StringAlignmentCenter)
                offsety = (rect->Height - bounds.Height) / 2;
            else if(format->vertalign == StringAlignmentFar)
                offsety = (rect->Height - bounds.Height);
        }
    }

    save_state = SaveDC(graphics->hdc);
    SetBkMode(graphics->hdc, TRANSPARENT);
    SetTextColor(graphics->hdc, brush->lb.lbColor);

    pt[0].X = 0.0;
    pt[0].Y = 0.0;
    pt[1].X = 1.0;
    pt[1].Y = 0.0;
    pt[2].X = 0.0;
    pt[2].Y = 1.0;
    GdipTransformPoints(graphics, CoordinateSpaceDevice, CoordinateSpaceWorld, pt, 3);
    angle = -gdiplus_atan2((pt[1].Y - pt[0].Y), (pt[1].X - pt[0].X));
    args.ang_cos = cos(angle);
    args.ang_sin = sin(angle);
    rel_width = sqrt((pt[1].Y-pt[0].Y)*(pt[1].Y-pt[0].Y)+
                     (pt[1].X-pt[0].X)*(pt[1].X-pt[0].X));
    rel_height = sqrt((pt[2].Y-pt[0].Y)*(pt[2].Y-pt[0].Y)+
                      (pt[2].X-pt[0].X)*(pt[2].X-pt[0].X));

    rectcpy[3].X = rectcpy[0].X = rect->X;
    rectcpy[1].Y = rectcpy[0].Y = rect->Y + offsety;
    rectcpy[2].X = rectcpy[1].X = rect->X + rect->Width;
    rectcpy[3].Y = rectcpy[2].Y = rect->Y + offsety + rect->Height;
    transform_and_round_points(graphics, corners, rectcpy, 4);

    scaled_rect.X = 0.0;
    scaled_rect.Y = 0.0;
    scaled_rect.Width = rel_width * rect->Width;
    scaled_rect.Height = rel_height * rect->Height;

    if (roundr(scaled_rect.Width) != 0 && roundr(scaled_rect.Height) != 0)
    {
        /* FIXME: If only the width or only the height is 0, we should probably still clip */
        rgn = CreatePolygonRgn(corners, 4, ALTERNATE);
        SelectClipRgn(graphics->hdc, rgn);
    }

    /* Use gdi to find the font, then perform transformations on it (height,
     * width, angle). */
    SelectObject(graphics->hdc, CreateFontIndirectW(&font->lfw));
    GetTextMetricsW(graphics->hdc, &textmet);
    lfw = font->lfw;

    lfw.lfHeight = roundr(((REAL)lfw.lfHeight) * rel_height);
    lfw.lfWidth = roundr(textmet.tmAveCharWidth * rel_width);

    lfw.lfEscapement = lfw.lfOrientation = roundr((angle / M_PI) * 1800.0);

    gdifont = CreateFontIndirectW(&lfw);
    DeleteObject(SelectObject(graphics->hdc, CreateFontIndirectW(&lfw)));

    if (!format || format->align == StringAlignmentNear)
    {
        args.drawbase.x = corners[0].x;
        args.drawbase.y = corners[0].y;
        args.drawflags = DT_NOCLIP | DT_EXPANDTABS;
    }
    else if (format->align == StringAlignmentCenter)
    {
        args.drawbase.x = (corners[0].x + corners[1].x)/2;
        args.drawbase.y = (corners[0].y + corners[1].y)/2;
        args.drawflags = DT_NOCLIP | DT_EXPANDTABS | DT_CENTER;
    }
    else /* (format->align == StringAlignmentFar) */
    {
        args.drawbase.x = corners[1].x;
        args.drawbase.y = corners[1].y;
        args.drawflags = DT_NOCLIP | DT_EXPANDTABS | DT_RIGHT;
    }

    gdip_format_string(graphics, string, length, font, &scaled_rect, format,
        draw_string_callback, &args);

    DeleteObject(rgn);
    DeleteObject(gdifont);

    RestoreDC(graphics->hdc, save_state);

    return Ok;
}

GpStatus WINGDIPAPI GdipResetClip(GpGraphics *graphics)
{
    TRACE("(%p)\n", graphics);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipSetInfinite(graphics->clip);
}

GpStatus WINGDIPAPI GdipResetWorldTransform(GpGraphics *graphics)
{
    TRACE("(%p)\n", graphics);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->worldtrans->matrix[0] = 1.0;
    graphics->worldtrans->matrix[1] = 0.0;
    graphics->worldtrans->matrix[2] = 0.0;
    graphics->worldtrans->matrix[3] = 1.0;
    graphics->worldtrans->matrix[4] = 0.0;
    graphics->worldtrans->matrix[5] = 0.0;

    return Ok;
}

GpStatus WINGDIPAPI GdipRestoreGraphics(GpGraphics *graphics, GraphicsState state)
{
    return GdipEndContainer(graphics, state);
}

GpStatus WINGDIPAPI GdipRotateWorldTransform(GpGraphics *graphics, REAL angle,
    GpMatrixOrder order)
{
    TRACE("(%p, %.2f, %d)\n", graphics, angle, order);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipRotateMatrix(graphics->worldtrans, angle, order);
}

GpStatus WINGDIPAPI GdipSaveGraphics(GpGraphics *graphics, GraphicsState *state)
{
    return GdipBeginContainer2(graphics, state);
}

GpStatus WINGDIPAPI GdipBeginContainer2(GpGraphics *graphics,
        GraphicsContainer *state)
{
    GraphicsContainerItem *container;
    GpStatus sts;

    TRACE("(%p, %p)\n", graphics, state);

    if(!graphics || !state)
        return InvalidParameter;

    sts = init_container(&container, graphics);
    if(sts != Ok)
        return sts;

    list_add_head(&graphics->containers, &container->entry);
    *state = graphics->contid = container->contid;

    return Ok;
}

GpStatus WINGDIPAPI GdipBeginContainer(GpGraphics *graphics, GDIPCONST GpRectF *dstrect, GDIPCONST GpRectF *srcrect, GpUnit unit, GraphicsContainer *state)
{
    FIXME("(%p, %p, %p, %d, %p): stub\n", graphics, dstrect, srcrect, unit, state);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipBeginContainerI(GpGraphics *graphics, GDIPCONST GpRect *dstrect, GDIPCONST GpRect *srcrect, GpUnit unit, GraphicsContainer *state)
{
    FIXME("(%p, %p, %p, %d, %p): stub\n", graphics, dstrect, srcrect, unit, state);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipComment(GpGraphics *graphics, UINT sizeData, GDIPCONST BYTE *data)
{
    FIXME("(%p, %d, %p): stub\n", graphics, sizeData, data);
    return NotImplemented;
}

GpStatus WINGDIPAPI GdipEndContainer(GpGraphics *graphics, GraphicsContainer state)
{
    GpStatus sts;
    GraphicsContainerItem *container, *container2;

    TRACE("(%p, %x)\n", graphics, state);

    if(!graphics)
        return InvalidParameter;

    LIST_FOR_EACH_ENTRY(container, &graphics->containers, GraphicsContainerItem, entry){
        if(container->contid == state)
            break;
    }

    /* did not find a matching container */
    if(&container->entry == &graphics->containers)
        return Ok;

    sts = restore_container(graphics, container);
    if(sts != Ok)
        return sts;

    /* remove all of the containers on top of the found container */
    LIST_FOR_EACH_ENTRY_SAFE(container, container2, &graphics->containers, GraphicsContainerItem, entry){
        if(container->contid == state)
            break;
        list_remove(&container->entry);
        delete_container(container);
    }

    list_remove(&container->entry);
    delete_container(container);

    return Ok;
}

GpStatus WINGDIPAPI GdipScaleWorldTransform(GpGraphics *graphics, REAL sx,
    REAL sy, GpMatrixOrder order)
{
    TRACE("(%p, %.2f, %.2f, %d)\n", graphics, sx, sy, order);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipScaleMatrix(graphics->worldtrans, sx, sy, order);
}

GpStatus WINGDIPAPI GdipSetClipGraphics(GpGraphics *graphics, GpGraphics *srcgraphics,
    CombineMode mode)
{
    TRACE("(%p, %p, %d)\n", graphics, srcgraphics, mode);

    if(!graphics || !srcgraphics)
        return InvalidParameter;

    return GdipCombineRegionRegion(graphics->clip, srcgraphics->clip, mode);
}

GpStatus WINGDIPAPI GdipSetCompositingMode(GpGraphics *graphics,
    CompositingMode mode)
{
    TRACE("(%p, %d)\n", graphics, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->compmode = mode;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetCompositingQuality(GpGraphics *graphics,
    CompositingQuality quality)
{
    TRACE("(%p, %d)\n", graphics, quality);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->compqual = quality;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetInterpolationMode(GpGraphics *graphics,
    InterpolationMode mode)
{
    TRACE("(%p, %d)\n", graphics, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->interpolation = mode;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetPageScale(GpGraphics *graphics, REAL scale)
{
    TRACE("(%p, %.2f)\n", graphics, scale);

    if(!graphics || (scale <= 0.0))
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->scale = scale;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetPageUnit(GpGraphics *graphics, GpUnit unit)
{
    TRACE("(%p, %d)\n", graphics, unit);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    if(unit == UnitWorld)
        return InvalidParameter;

    graphics->unit = unit;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetPixelOffsetMode(GpGraphics *graphics, PixelOffsetMode
    mode)
{
    TRACE("(%p, %d)\n", graphics, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->pixeloffset = mode;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetRenderingOrigin(GpGraphics *graphics, INT x, INT y)
{
    static int calls;

    TRACE("(%p,%i,%i)\n", graphics, x, y);

    if (!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipSetSmoothingMode(GpGraphics *graphics, SmoothingMode mode)
{
    TRACE("(%p, %d)\n", graphics, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->smoothing = mode;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetTextContrast(GpGraphics *graphics, UINT contrast)
{
    TRACE("(%p, %d)\n", graphics, contrast);

    if(!graphics)
        return InvalidParameter;

    graphics->textcontrast = contrast;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetTextRenderingHint(GpGraphics *graphics,
    TextRenderingHint hint)
{
    TRACE("(%p, %d)\n", graphics, hint);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    graphics->texthint = hint;

    return Ok;
}

GpStatus WINGDIPAPI GdipSetWorldTransform(GpGraphics *graphics, GpMatrix *matrix)
{
    TRACE("(%p, %p)\n", graphics, matrix);

    if(!graphics || !matrix)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    GdipDeleteMatrix(graphics->worldtrans);
    return GdipCloneMatrix(matrix, &graphics->worldtrans);
}

GpStatus WINGDIPAPI GdipTranslateWorldTransform(GpGraphics *graphics, REAL dx,
    REAL dy, GpMatrixOrder order)
{
    TRACE("(%p, %.2f, %.2f, %d)\n", graphics, dx, dy, order);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipTranslateMatrix(graphics->worldtrans, dx, dy, order);
}

/*****************************************************************************
 * GdipSetClipHrgn [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipSetClipHrgn(GpGraphics *graphics, HRGN hrgn, CombineMode mode)
{
    GpRegion *region;
    GpStatus status;

    TRACE("(%p, %p, %d)\n", graphics, hrgn, mode);

    if(!graphics)
        return InvalidParameter;

    status = GdipCreateRegionHrgn(hrgn, &region);
    if(status != Ok)
        return status;

    status = GdipSetClipRegion(graphics, region, mode);

    GdipDeleteRegion(region);
    return status;
}

GpStatus WINGDIPAPI GdipSetClipPath(GpGraphics *graphics, GpPath *path, CombineMode mode)
{
    TRACE("(%p, %p, %d)\n", graphics, path, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipCombineRegionPath(graphics->clip, path, mode);
}

GpStatus WINGDIPAPI GdipSetClipRect(GpGraphics *graphics, REAL x, REAL y,
                                    REAL width, REAL height,
                                    CombineMode mode)
{
    GpRectF rect;

    TRACE("(%p, %.2f, %.2f, %.2f, %.2f, %d)\n", graphics, x, y, width, height, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    rect.X = x;
    rect.Y = y;
    rect.Width  = width;
    rect.Height = height;

    return GdipCombineRegionRect(graphics->clip, &rect, mode);
}

GpStatus WINGDIPAPI GdipSetClipRectI(GpGraphics *graphics, INT x, INT y,
                                     INT width, INT height,
                                     CombineMode mode)
{
    TRACE("(%p, %d, %d, %d, %d, %d)\n", graphics, x, y, width, height, mode);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipSetClipRect(graphics, (REAL)x, (REAL)y, (REAL)width, (REAL)height, mode);
}

GpStatus WINGDIPAPI GdipSetClipRegion(GpGraphics *graphics, GpRegion *region,
                                      CombineMode mode)
{
    TRACE("(%p, %p, %d)\n", graphics, region, mode);

    if(!graphics || !region)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipCombineRegionRegion(graphics->clip, region, mode);
}

GpStatus WINGDIPAPI GdipSetMetafileDownLevelRasterizationLimit(GpMetafile *metafile,
    UINT limitDpi)
{
    static int calls;

    TRACE("(%p,%u)\n", metafile, limitDpi);

    if(!(calls++))
        FIXME("not implemented\n");

    return NotImplemented;
}

GpStatus WINGDIPAPI GdipDrawPolygon(GpGraphics *graphics,GpPen *pen,GDIPCONST GpPointF *points,
    INT count)
{
    INT save_state;
    POINT *pti;

    TRACE("(%p, %p, %d)\n", graphics, points, count);

    if(!graphics || !pen || count<=0)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    pti = GdipAlloc(sizeof(POINT) * count);

    save_state = prepare_dc(graphics, pen);
    SelectObject(graphics->hdc, GetStockObject(NULL_BRUSH));

    transform_and_round_points(graphics, pti, (GpPointF*)points, count);
    Polygon(graphics->hdc, pti, count);

    restore_dc(graphics, save_state);
    GdipFree(pti);

    return Ok;
}

GpStatus WINGDIPAPI GdipDrawPolygonI(GpGraphics *graphics,GpPen *pen,GDIPCONST GpPoint *points,
    INT count)
{
    GpStatus ret;
    GpPointF *ptf;
    INT i;

    TRACE("(%p, %p, %p, %d)\n", graphics, pen, points, count);

    if(count<=0)    return InvalidParameter;
    ptf = GdipAlloc(sizeof(GpPointF) * count);

    for(i = 0;i < count; i++){
        ptf[i].X = (REAL)points[i].X;
        ptf[i].Y = (REAL)points[i].Y;
    }

    ret = GdipDrawPolygon(graphics,pen,ptf,count);
    GdipFree(ptf);

    return ret;
}

GpStatus WINGDIPAPI GdipGetDpiX(GpGraphics *graphics, REAL* dpi)
{
    TRACE("(%p, %p)\n", graphics, dpi);

    if(!graphics || !dpi)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *dpi = (REAL)GetDeviceCaps(graphics->hdc, LOGPIXELSX);

    return Ok;
}

GpStatus WINGDIPAPI GdipGetDpiY(GpGraphics *graphics, REAL* dpi)
{
    TRACE("(%p, %p)\n", graphics, dpi);

    if(!graphics || !dpi)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *dpi = (REAL)GetDeviceCaps(graphics->hdc, LOGPIXELSY);

    return Ok;
}

GpStatus WINGDIPAPI GdipMultiplyWorldTransform(GpGraphics *graphics, GDIPCONST GpMatrix *matrix,
    GpMatrixOrder order)
{
    GpMatrix m;
    GpStatus ret;

    TRACE("(%p, %p, %d)\n", graphics, matrix, order);

    if(!graphics || !matrix)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    m = *(graphics->worldtrans);

    ret = GdipMultiplyMatrix(&m, matrix, order);
    if(ret == Ok)
        *(graphics->worldtrans) = m;

    return ret;
}

GpStatus WINGDIPAPI GdipGetDC(GpGraphics *graphics, HDC *hdc)
{
    TRACE("(%p, %p)\n", graphics, hdc);

    if(!graphics || !hdc)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    *hdc = graphics->hdc;
    graphics->busy = TRUE;

    return Ok;
}

GpStatus WINGDIPAPI GdipReleaseDC(GpGraphics *graphics, HDC hdc)
{
    TRACE("(%p, %p)\n", graphics, hdc);

    if(!graphics)
        return InvalidParameter;

    if(graphics->hdc != hdc || !(graphics->busy))
        return InvalidParameter;

    graphics->busy = FALSE;

    return Ok;
}

GpStatus WINGDIPAPI GdipGetClip(GpGraphics *graphics, GpRegion *region)
{
    GpRegion *clip;
    GpStatus status;

    TRACE("(%p, %p)\n", graphics, region);

    if(!graphics || !region)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    if((status = GdipCloneRegion(graphics->clip, &clip)) != Ok)
        return status;

    /* free everything except root node and header */
    delete_element(&region->node);
    memcpy(region, clip, sizeof(GpRegion));
    GdipFree(clip);

    return Ok;
}

GpStatus WINGDIPAPI GdipTransformPoints(GpGraphics *graphics, GpCoordinateSpace dst_space,
                                        GpCoordinateSpace src_space, GpPointF *points, INT count)
{
    GpMatrix *matrix;
    GpStatus stat;
    REAL unitscale;

    if(!graphics || !points || count <= 0)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    TRACE("(%p, %d, %d, %p, %d)\n", graphics, dst_space, src_space, points, count);

    if (src_space == dst_space) return Ok;

    stat = GdipCreateMatrix(&matrix);
    if (stat == Ok)
    {
        unitscale = convert_unit(graphics->hdc, graphics->unit);

        if(graphics->unit != UnitDisplay)
            unitscale *= graphics->scale;

        /* transform from src_space to CoordinateSpacePage */
        switch (src_space)
        {
        case CoordinateSpaceWorld:
            GdipMultiplyMatrix(matrix, graphics->worldtrans, MatrixOrderAppend);
            break;
        case CoordinateSpacePage:
            break;
        case CoordinateSpaceDevice:
            GdipScaleMatrix(matrix, 1.0/unitscale, 1.0/unitscale, MatrixOrderAppend);
            break;
        }

        /* transform from CoordinateSpacePage to dst_space */
        switch (dst_space)
        {
        case CoordinateSpaceWorld:
            {
                GpMatrix *inverted_transform;
                stat = GdipCloneMatrix(graphics->worldtrans, &inverted_transform);
                if (stat == Ok)
                {
                    stat = GdipInvertMatrix(inverted_transform);
                    if (stat == Ok)
                        GdipMultiplyMatrix(matrix, inverted_transform, MatrixOrderAppend);
                    GdipDeleteMatrix(inverted_transform);
                }
                break;
            }
        case CoordinateSpacePage:
            break;
        case CoordinateSpaceDevice:
            GdipScaleMatrix(matrix, unitscale, unitscale, MatrixOrderAppend);
            break;
        }

        if (stat == Ok)
            stat = GdipTransformMatrixPoints(matrix, points, count);

        GdipDeleteMatrix(matrix);
    }

    return stat;
}

GpStatus WINGDIPAPI GdipTransformPointsI(GpGraphics *graphics, GpCoordinateSpace dst_space,
                                         GpCoordinateSpace src_space, GpPoint *points, INT count)
{
    GpPointF *pointsF;
    GpStatus ret;
    INT i;

    TRACE("(%p, %d, %d, %p, %d)\n", graphics, dst_space, src_space, points, count);

    if(count <= 0)
        return InvalidParameter;

    pointsF = GdipAlloc(sizeof(GpPointF) * count);
    if(!pointsF)
        return OutOfMemory;

    for(i = 0; i < count; i++){
        pointsF[i].X = (REAL)points[i].X;
        pointsF[i].Y = (REAL)points[i].Y;
    }

    ret = GdipTransformPoints(graphics, dst_space, src_space, pointsF, count);

    if(ret == Ok)
        for(i = 0; i < count; i++){
            points[i].X = roundr(pointsF[i].X);
            points[i].Y = roundr(pointsF[i].Y);
        }
    GdipFree(pointsF);

    return ret;
}

HPALETTE WINGDIPAPI GdipCreateHalftonePalette(void)
{
    static int calls;

    TRACE("\n");

    if (!calls++)
      FIXME("stub\n");

    return NULL;
}

/*****************************************************************************
 * GdipTranslateClip [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipTranslateClip(GpGraphics *graphics, REAL dx, REAL dy)
{
    TRACE("(%p, %.2f, %.2f)\n", graphics, dx, dy);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipTranslateRegion(graphics->clip, dx, dy);
}

/*****************************************************************************
 * GdipTranslateClipI [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipTranslateClipI(GpGraphics *graphics, INT dx, INT dy)
{
    TRACE("(%p, %d, %d)\n", graphics, dx, dy);

    if(!graphics)
        return InvalidParameter;

    if(graphics->busy)
        return ObjectBusy;

    return GdipTranslateRegion(graphics->clip, (REAL)dx, (REAL)dy);
}


/*****************************************************************************
 * GdipMeasureDriverString [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipMeasureDriverString(GpGraphics *graphics, GDIPCONST UINT16 *text, INT length,
                                            GDIPCONST GpFont *font, GDIPCONST PointF *positions,
                                            INT flags, GDIPCONST GpMatrix *matrix, RectF *boundingBox)
{
    FIXME("(%p %p %d %p %p %d %p %p): stub\n", graphics, text, length, font, positions, flags, matrix, boundingBox);
    return NotImplemented;
}

/*****************************************************************************
 * GdipDrawDriverString [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipDrawDriverString(GpGraphics *graphics, GDIPCONST UINT16 *text, INT length,
                                         GDIPCONST GpFont *font, GDIPCONST GpBrush *brush,
                                         GDIPCONST PointF *positions, INT flags,
                                         GDIPCONST GpMatrix *matrix )
{
    FIXME("(%p %p %d %p %p %p %d %p): stub\n", graphics, text, length, font, brush, positions, flags, matrix);
    return NotImplemented;
}

/*****************************************************************************
 * GdipRecordMetafileI [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipRecordMetafileI(HDC hdc, EmfType type, GDIPCONST GpRect *frameRect,
                                        MetafileFrameUnit frameUnit, GDIPCONST WCHAR *desc, GpMetafile **metafile)
{
    FIXME("(%p %d %p %d %p %p): stub\n", hdc, type, frameRect, frameUnit, desc, metafile);
    return NotImplemented;
}

/*****************************************************************************
 * GdipIsVisibleClipEmpty [GDIPLUS.@]
 */
GpStatus WINGDIPAPI GdipIsVisibleClipEmpty(GpGraphics *graphics, BOOL *res)
{
    GpStatus stat;
    GpRegion* rgn;

    TRACE("(%p, %p)\n", graphics, res);

    if((stat = GdipCreateRegion(&rgn)) != Ok)
        return stat;

    if((stat = get_visible_clip_region(graphics, rgn)) != Ok)
        goto cleanup;

    stat = GdipIsEmptyRegion(rgn, graphics, res);

cleanup:
    GdipDeleteRegion(rgn);
    return stat;
}
