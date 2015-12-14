/*
 *  ImageList implementation
 *
 *  Copyright 1998 Eric Kohl
 *            2000 Jason Mawdsley.
 *            2001 Michael Stefaniuc
 *            2001 Charles Loep for CodeWeavers
 *
 *  TODO:
 *    - Fix ImageList_DrawIndirect (xBitmap, yBitmap, rgbFg, rgbBk, dwRop).
 *    - Fix ImageList_GetIcon.
 *    - Fix ImageList_SetFilter (undocumented).
 *      BTW does anybody know anything about this function???
 *        - It removes 12 Bytes from the stack (3 Parameters).
 *        - First parameter SHOULD be a HIMAGELIST.
 *        - Second parameter COULD be an index?????
 *        - Third parameter.... ?????????????????????
 *
 *  Comments:
 *    - ImageList_Draw, ImageList_DrawEx and ImageList_GetIcon use
 *      ImageList_DrawIndirect. Since ImageList_DrawIndirect is still
 *      partially implemented, the functions mentioned above will be
 *      limited in functionality too.
 *
 *    - Hotspot handling still not correct. The Hotspot passed to BeginDrag
 *	is the offset of the image position relative to the actual mouse pointer
 *	position. However the Hotspot passed to SetDragCursorImage is the
 *	offset of the mouse messages sent to the application...
 */

#include <stdlib.h>
#include <string.h>
#include "winerror.h"
#include "winbase.h"
#include "objbase.h"
#include "commctrl.h"
#include "imagelist.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(imagelist);


#define MAX_OVERLAYIMAGE 15

/* internal image list data used for Drag & Drop operations */
typedef struct
{
    HWND	hwnd;
    HIMAGELIST	himl;
    /* position of the drag image relative to the window */
    INT		x;
    INT		y;
    /* offset of the hotspot relative to the origin of the image */
    INT		dxHotspot;
    INT		dyHotspot;
    /* is the drag image visible */
    BOOL	bShow;
    /* saved background */
    HBITMAP	hbmBg;
    BOOL	bHSPending;
} INTERNALDRAG;

static INTERNALDRAG InternalDrag = { 0, 0, 0, 0, 0, 0, FALSE, 0, FALSE };



/*************************************************************************
 * IMAGELIST_InternalExpandBitmaps [Internal]
 *
 * Expands the bitmaps of an image list by the given number of images.
 *
 * PARAMS
 *     himl        [I] handle to image list
 *     nImageCount [I] number of images to add
 *
 * RETURNS
 *     nothing
 *
 * NOTES
 *     This function can NOT be used to reduce the number of images.
 */
static VOID
IMAGELIST_InternalExpandBitmaps (HIMAGELIST himl, INT nImageCount, INT cx, INT cy)
{
    HDC     hdcImageList, hdcBitmap;
    HBITMAP hbmNewBitmap;
    INT     nNewWidth, nNewCount;

    if ((himl->cCurImage + nImageCount <= himl->cMaxImage)
        && (himl->cy >= cy))
	return;

    if (cy == 0) cy = himl->cy;
    nNewCount = himl->cCurImage + nImageCount + himl->cGrow;
    nNewWidth = nNewCount * himl->cx;

    TRACE("Create expanded bitmaps : himl=%p x=%d y=%d count=%d\n", himl, nNewWidth, cy, nNewCount);
    hdcImageList = CreateCompatibleDC (0);
    hdcBitmap = CreateCompatibleDC (0);

    hbmNewBitmap =
        CreateBitmap (nNewWidth, cy, 1, himl->uBitsPixel, NULL);
    if (hbmNewBitmap == 0)
        ERR("creating new image bitmap (x=%d y=%d)!\n", nNewWidth, cy);

    SelectObject (hdcImageList, himl->hbmImage);
    SelectObject (hdcBitmap, hbmNewBitmap);
    BitBlt (hdcBitmap, 0, 0, himl->cCurImage * himl->cx, cy,
              hdcImageList, 0, 0, SRCCOPY);

    DeleteObject (himl->hbmImage);
    himl->hbmImage = hbmNewBitmap;

    if (himl->hbmMask) {
        hbmNewBitmap =
            CreateBitmap (nNewWidth, cy, 1, 1, NULL);

        if (hbmNewBitmap == 0)
            ERR("creating new mask bitmap!\n");

        SelectObject (hdcImageList, himl->hbmMask);
        SelectObject (hdcBitmap, hbmNewBitmap);
        BitBlt (hdcBitmap, 0, 0, himl->cCurImage * himl->cx, cy,
                  hdcImageList, 0, 0, SRCCOPY);
        DeleteObject (himl->hbmMask);
        himl->hbmMask = hbmNewBitmap;
    }

    himl->cMaxImage = nNewCount;

    DeleteDC (hdcImageList);
    DeleteDC (hdcBitmap);
}


/*************************************************************************
 * IMAGELIST_InternalDraw [Internal]
 *
 * Draws the image in the ImageList (without the mask)
 *
 * PARAMS
 *     pimldp        [I] pointer to IMAGELISTDRAWPARAMS structure.
 *     cx            [I] the width of the image to display
 *     cy............[I] the height of the image to display
 *
 * RETURNS
 *     nothing
 *
 * NOTES
 *     This function is used by ImageList_DrawIndirect, when it is
 *     required to draw only the Image (without the mask) to the screen.
 *
 *     Blending and Overlays styles are accomplished by another function
 */
static VOID
IMAGELIST_InternalDraw(IMAGELISTDRAWPARAMS *pimldp, INT cx, INT cy)
{
    HDC hImageDC;
    HBITMAP hOldBitmap;

    hImageDC = CreateCompatibleDC(0);
    hOldBitmap = SelectObject(hImageDC, pimldp->himl->hbmImage);
    BitBlt(pimldp->hdcDst,
        pimldp->x, pimldp->y, cx, cy,
        hImageDC,
        pimldp->himl->cx * pimldp->i, 0,
        SRCCOPY);

    SelectObject(hImageDC, hOldBitmap);
    DeleteDC(hImageDC);
}


/*************************************************************************
 * IMAGELIST_InternalDrawMask [Internal]
 *
 * Draws the image in the ImageList with the mask
 *
 * PARAMS
 *     pimldp        [I] pointer to IMAGELISTDRAWPARAMS structure.
 *     cx            [I] the width of the image to display
 *     cy............[I] the height of the image to display
 *
 * RETURNS
 *     nothing
 *
 * NOTES
 *     This function is used by ImageList_DrawIndirect, when it is
 *     required to draw the Image with the mask to the screen.
 *
 *     Blending and Overlays styles are accomplished by another function.
 */
static VOID
IMAGELIST_InternalDrawMask(IMAGELISTDRAWPARAMS *pimldp, INT cx, INT cy)
{
    BOOL bUseCustomBackground, bBlendFlag;
    HBRUSH hBrush, hOldBrush;
    HDC     hMaskDC, hImageDC;
    HBITMAP hOldBitmapImage, hOldBitmapMask;
    HIMAGELIST himlLocal = pimldp->himl;
    COLORREF oldBkColor, oldFgColor;
    UINT fStyle = pimldp->fStyle & (~ILD_OVERLAYMASK);

    /*
     * We need a dc and bitmap to draw on that is
     * not on the screen.
     */
    HDC hOffScreenDC = 0;
    HBITMAP hOffScreenBmp = 0;

    bUseCustomBackground = (himlLocal->clrBk != CLR_NONE);
    bBlendFlag = (fStyle & ILD_BLEND50 ) || (fStyle & ILD_BLEND25);

    hImageDC = CreateCompatibleDC(0);
    hMaskDC = CreateCompatibleDC(0);

    /* Create a compatible DC. */
    hOffScreenDC = CreateCompatibleDC( pimldp->hdcDst );

    if ( hOffScreenDC )
    {
        hOffScreenBmp = CreateCompatibleBitmap( pimldp->hdcDst, cx, cy );

        if ( hOffScreenBmp )
            SelectObject( hOffScreenDC, hOffScreenBmp  );
        else
            goto cleanup;
    }
    else
        goto cleanup;

    hOldBitmapImage = SelectObject(hImageDC, himlLocal->hbmImage);
    hOldBitmapMask = SelectObject(hMaskDC, himlLocal->hbmMask);

    /*
     * Get a copy of the image for the masking operations.
     * We will use the copy, and this dc for all the various
     * blitting, and then do one final blit to the screen dc.
     * This should clean up most of the flickering.
     */
    BitBlt( hOffScreenDC, 0, 0, cx, cy, pimldp->hdcDst, pimldp->x,
            pimldp->y, SRCCOPY);

    /*
     * Draw the Background for the appropriate Styles
     */
    if( bUseCustomBackground && (fStyle == ILD_NORMAL || fStyle & ILD_IMAGE
         || bBlendFlag) )
    {

        hBrush = CreateSolidBrush (himlLocal->clrBk);
        hOldBrush = SelectObject (pimldp->hdcDst, hBrush);

        PatBlt( hOffScreenDC, pimldp->x, pimldp->y, cx, cy, PATCOPY );

        DeleteObject (SelectObject (pimldp->hdcDst, hOldBrush));
    }

    /*
     * Draw Image Transparently over the current background
     */
    if(fStyle == ILD_NORMAL || (fStyle & ILD_TRANSPARENT) ||
       ((fStyle & ILD_IMAGE) && bUseCustomBackground) || bBlendFlag)
    {   /*
         * To obtain a transparent look, background color should be set
         * to white and foreground color to black when blting the
         * monochrome mask.
         */

        oldBkColor = SetBkColor( hOffScreenDC, RGB( 0xff, 0xff, 0xff ) );
        oldFgColor = SetTextColor( hOffScreenDC, RGB( 0, 0, 0 ) );

        BitBlt( hOffScreenDC, 0, 0, cx, cy,hMaskDC, himlLocal->cx * pimldp->i,
                0, SRCAND );

        BitBlt( hOffScreenDC, 0, 0, cx, cy, hImageDC,himlLocal->cx * pimldp->i,
                0, SRCPAINT );

    }

    /*
     * Draw the image when no Background is specified
     */
    else if((fStyle & ILD_IMAGE) && !bUseCustomBackground)
    {
        BitBlt( hOffScreenDC, 0, 0, cx, cy, hImageDC,
                himlLocal->cx * pimldp->i, 0, SRCCOPY);
    }
    /*
     * Draw the mask with or without a background
     */
    else if(fStyle & ILD_MASK)
    {
        BitBlt( hOffScreenDC, 0, 0, cx, cy, hMaskDC, himlLocal->cx * pimldp->i,
                0, bUseCustomBackground ? SRCCOPY : SRCAND);
    }

    /*
     * Blit the bitmap to the screen now.
     */
    BitBlt( pimldp->hdcDst, pimldp->x, pimldp->y, cx, cy,
            hOffScreenDC, 0, 0, SRCCOPY);


    SelectObject(hImageDC, hOldBitmapImage);
    SelectObject(hMaskDC, hOldBitmapMask);

cleanup:

    DeleteDC(hImageDC);
    DeleteDC(hMaskDC);

    DeleteDC( hOffScreenDC );
    DeleteObject( hOffScreenBmp );

    return;
}

/*************************************************************************
 * IMAGELIST_InternalDrawBlend [Internal]
 *
 * Draws the Blend over the current image
 *
 * PARAMS
 *     pimldp        [I] pointer to IMAGELISTDRAWPARAMS structure.
 *     cx            [I] the width of the image to display
 *     cy............[I] the height of the image to display
 *
 * RETURNS
 *     nothing
 *
 * NOTES
 *     This functions is used by ImageList_DrawIndirect, when it is
 *     required to add the blend to the current image.
 *
 */
static VOID
IMAGELIST_InternalDrawBlend(IMAGELISTDRAWPARAMS *pimldp, INT cx, INT cy)
{

    HDC         hBlendMaskDC,hMaskDC;
    HBRUSH      hBlendColorBrush, hBlendBrush, hOldBrush;
    HBITMAP     hBlendMaskBitmap, hOldBitmap;
    COLORREF    clrBlend, OldTextColor, OldBkColor;
    HIMAGELIST  himlLocal = pimldp->himl;

    clrBlend = GetSysColor (COLOR_HIGHLIGHT);
    if (!(pimldp->rgbFg == CLR_DEFAULT))
    {
        clrBlend = pimldp->rgbFg;
    }
    /* Create the blend Mask
    */
    hBlendMaskDC = CreateCompatibleDC(0);
    hBlendBrush = pimldp->fStyle & ILD_BLEND50 ?
        himlLocal->hbrBlend50 : himlLocal->hbrBlend25;

    hBlendMaskBitmap = CreateBitmap(cx, cy, 1, 1, NULL);
    hOldBitmap = SelectObject(hBlendMaskDC, hBlendMaskBitmap);

    hOldBrush = (HBRUSH) SelectObject(hBlendMaskDC, hBlendBrush);
    PatBlt(hBlendMaskDC, 0, 0, cx, cy, PATCOPY);
    SelectObject(hBlendMaskDC, hOldBrush);

    /* Modify the blend mask if an Image Mask exist
    */
    if(pimldp->himl->hbmMask != 0)
    {
        HBITMAP hOldMaskBitmap;
        hMaskDC = CreateCompatibleDC(0);
        hOldMaskBitmap = (HBITMAP) SelectObject(hMaskDC, himlLocal->hbmMask);

        BitBlt(hBlendMaskDC,
            0,0, cx, cy,
            hMaskDC,
            himlLocal->cx * pimldp->i,0,
            0x220326); /* NOTSRCAND */

        BitBlt(hBlendMaskDC,
            0,0, cx, cy,
            hBlendMaskDC,
            0,0,
            NOTSRCCOPY);

        SelectObject(hMaskDC, hOldMaskBitmap);
        DeleteDC(hMaskDC);

    }
    /* Apply blend to the current image given the BlendMask
    */
    OldTextColor = SetTextColor(pimldp->hdcDst, 0);
    OldBkColor = SetBkColor(pimldp->hdcDst, RGB(255,255,255));
    hBlendColorBrush = CreateSolidBrush(clrBlend);
    hOldBrush = (HBRUSH) SelectObject (pimldp->hdcDst, hBlendColorBrush);

    BitBlt (pimldp->hdcDst,
        pimldp->x, pimldp->y, cx, cy,
        hBlendMaskDC,
        0, 0,
        0xB8074A); /* PSDPxax */

    SelectObject(pimldp->hdcDst, hOldBrush);
    SetTextColor(pimldp->hdcDst, OldTextColor);
    SetBkColor(pimldp->hdcDst, OldBkColor);
    SelectObject(hBlendMaskDC, hOldBitmap);
    DeleteDC(hBlendMaskDC);
    DeleteObject(hBlendMaskBitmap);
    DeleteObject(hBlendColorBrush);
}

/*************************************************************************
 * IMAGELIST_InternalDrawOverlay [Internal]
 *
 * Draws the overlay image
 *
 * PARAMS
 *     pimldp        [I] pointer to IMAGELISTDRAWPARAMS structure.
 *     cx            [I] the width of the image to display
 *     cy............[I] the height of the image to display
 *
 * RETURNS
 *     nothing
 *
 * NOTES
 *     This functions is used by ImageList_DrawIndirect, when it is
 *     required to draw the overlay
 *
 *
 */
static VOID
IMAGELIST_InternalDrawOverlay(IMAGELISTDRAWPARAMS *pimldp, INT cx, INT cy)
{
    INT     nOvlIdx;
    HDC     hImageDC;
    HBITMAP hOldBitmap;

    nOvlIdx = (pimldp->fStyle & ILD_OVERLAYMASK) >> 8;
    if ((nOvlIdx >= 1) && (nOvlIdx <= MAX_OVERLAYIMAGE))
    {
        nOvlIdx = pimldp->himl->nOvlIdx[nOvlIdx - 1];
        if ((nOvlIdx >= 0) && (nOvlIdx <= pimldp->himl->cCurImage))
        {
            hImageDC = CreateCompatibleDC(0);
            if (pimldp->himl->hbmMask)
            {
                hOldBitmap = (HBITMAP) SelectObject (hImageDC,
                    pimldp->himl->hbmMask);

                BitBlt (pimldp->hdcDst,
                    pimldp->x, pimldp->y, cx, cy,
                    hImageDC, pimldp->himl->cx * nOvlIdx, 0,
                    SRCAND);

                SelectObject(hImageDC, hOldBitmap);
            }
            hOldBitmap = (HBITMAP) SelectObject (hImageDC,
                pimldp->himl->hbmImage);

            BitBlt (pimldp->hdcDst,
                pimldp->x, pimldp->y, cx, cy,
                hImageDC,
                pimldp->himl->cx * nOvlIdx, 0,
                SRCPAINT);

            SelectObject(hImageDC, hOldBitmap);
            DeleteDC(hImageDC);
        }
    }
}


/*************************************************************************
 * ImageList_Add [COMCTL32.40]
 *
 * Add an image or images to an image list.
 *
 * PARAMS
 *     himl     [I] handle to image list
 *     hbmImage [I] handle to image bitmap
 *     hbmMask  [I] handle to mask bitmap
 *
 * RETURNS
 *     Success: Index of the first new image.
 *     Failure: -1
 */

INT WINAPI
ImageList_Add (HIMAGELIST himl,	HBITMAP hbmImage, HBITMAP hbmMask)
{
    HDC     hdcImage, hdcBitmap;
    INT     nFirstIndex, nImageCount;
    INT     nStartX;
    BITMAP  bmp;
    HBITMAP hOldBitmapImage, hOldBitmap;

    TRACE("himl=%p hbmimage=%x hbmmask=%x\n", himl, hbmImage, hbmMask);
    if (!himl || !hbmImage)
        return -1;

    GetObjectA (hbmImage, sizeof(BITMAP), (LPVOID)&bmp);
    nImageCount = bmp.bmWidth / himl->cx;

    IMAGELIST_InternalExpandBitmaps (himl, nImageCount, bmp.bmWidth, bmp.bmHeight);

    nStartX = himl->cCurImage * himl->cx;

    hdcImage  = CreateCompatibleDC(0);
    hdcBitmap = CreateCompatibleDC(0);

    hOldBitmapImage = SelectObject(hdcImage, himl->hbmImage);
    hOldBitmap = SelectObject(hdcBitmap, hbmImage);

    /* Copy result to the imagelist
    */
    BitBlt (hdcImage, nStartX, 0, bmp.bmWidth, bmp.bmHeight,
        hdcBitmap, 0, 0, SRCCOPY);

    if(himl->hbmMask)
    {
        HDC hdcMask, hdcTemp, hOldBitmapMask, hOldBitmapTemp;

        hdcMask   = CreateCompatibleDC (0);
        hdcTemp   = CreateCompatibleDC(0);
        hOldBitmapMask = (HBITMAP) SelectObject(hdcMask, himl->hbmMask);
        hOldBitmapTemp = (HBITMAP) SelectObject(hdcTemp, hbmMask);

        BitBlt (hdcMask,
            nStartX, 0, bmp.bmWidth, bmp.bmHeight,
            hdcTemp,
            0, 0,
            SRCCOPY);

        SelectObject(hdcTemp, hOldBitmapTemp);
        DeleteDC(hdcTemp);

        /* Remove the background from the image
        */
        BitBlt (hdcImage,
            nStartX, 0, bmp.bmWidth, bmp.bmHeight,
            hdcMask,
            nStartX, 0,
            0x220326); /* NOTSRCAND */

        SelectObject(hdcMask, hOldBitmapMask);
        DeleteDC(hdcMask);
    }

    SelectObject(hdcImage, hOldBitmapImage);
    SelectObject(hdcBitmap, hOldBitmap);
    DeleteDC(hdcImage);
    DeleteDC(hdcBitmap);

    nFirstIndex = himl->cCurImage;
    himl->cCurImage += nImageCount;

    return nFirstIndex;
}


/*************************************************************************
 * ImageList_AddIcon [COMCTL32.41]
 *
 * Adds an icon to an image list.
 *
 * PARAMS
 *     himl  [I] handle to image list
 *     hIcon [I] handle to icon
 *
 * RETURNS
 *     Success: index of the new image
 *     Failure: -1
 */

INT WINAPI
ImageList_AddIcon (HIMAGELIST himl, HICON hIcon)
{
    return ImageList_ReplaceIcon (himl, -1, hIcon);
}


/*************************************************************************
 * ImageList_AddMasked [COMCTL32.42]
 *
 * Adds an image or images to an image list and creates a mask from the
 * specified bitmap using the mask color.
 *
 * PARAMS
 *     himl    [I] handle to image list.
 *     hBitmap [I] handle to bitmap
 *     clrMask [I] mask color.
 *
 * RETURNS
 *     Success: Index of the first new image.
 *     Failure: -1
 */

INT WINAPI
ImageList_AddMasked (HIMAGELIST himl, HBITMAP hBitmap, COLORREF clrMask)
{
    HDC    hdcImage, hdcMask, hdcBitmap;
    INT    nIndex, nImageCount, nMaskXOffset=0;
    BITMAP bmp;
    HBITMAP hOldBitmap, hOldBitmapMask, hOldBitmapImage;
    HBITMAP hMaskBitmap=0;
    COLORREF bkColor;

    TRACE("himl=%p hbitmap=%x clrmask=%lx\n", himl, hBitmap, clrMask);
    if (himl == NULL)
        return -1;

    if (!GetObjectA (hBitmap, sizeof(BITMAP), &bmp))
        return -1;

    nImageCount = bmp.bmWidth / himl->cx;

    IMAGELIST_InternalExpandBitmaps (himl, nImageCount, bmp.bmWidth, bmp.bmHeight);

    nIndex = himl->cCurImage;
    himl->cCurImage += nImageCount;

    hdcMask   = CreateCompatibleDC (0);
    hdcImage  = CreateCompatibleDC(0);
    hdcBitmap = CreateCompatibleDC(0);


    hOldBitmapImage = SelectObject(hdcImage, himl->hbmImage);
    hOldBitmap = SelectObject(hdcBitmap, hBitmap);
    if(himl->hbmMask)
    {
        hOldBitmapMask = SelectObject(hdcMask, himl->hbmMask);
        nMaskXOffset = nIndex * himl->cx;
    }
    else
    {
        /*
            Create a temp Mask so we can remove the background of
            the Image (Windows does this even if there is no mask)
        */
        hMaskBitmap = CreateBitmap(bmp.bmWidth, bmp.bmHeight, 1, 1, NULL);
        hOldBitmapMask = SelectObject(hdcMask, hMaskBitmap);
        nMaskXOffset = 0;
    }
    /* create monochrome image to the mask bitmap */
    bkColor = (clrMask != CLR_DEFAULT) ? clrMask :
        GetPixel (hdcBitmap, 0, 0);
    SetBkColor (hdcBitmap, bkColor);
    BitBlt (hdcMask,
        nMaskXOffset, 0, bmp.bmWidth, bmp.bmHeight,
        hdcBitmap, 0, 0,
        SRCCOPY);

    SetBkColor(hdcBitmap, RGB(255,255,255));
    /*Remove the background from the image
    */
    /*
        WINDOWS BUG ALERT!!!!!!
        The statement below should not be done in common practice
        but this is how ImageList_AddMasked works in Windows.
        It overwrites the original bitmap passed, this was discovered
        by using the same bitmap to iterate the different styles
        on windows where it failed (BUT ImageList_Add is OK)
        This is here in case some apps rely on this bug
    */
    BitBlt(hdcBitmap,
        0, 0, bmp.bmWidth, bmp.bmHeight,
        hdcMask,
        nMaskXOffset, 0,
        0x220326); /* NOTSRCAND */
    /* Copy result to the imagelist
    */
    BitBlt (hdcImage,
        nIndex * himl->cx, 0, bmp.bmWidth, bmp.bmHeight,
        hdcBitmap,
        0, 0,
        SRCCOPY);
    /* Clean up
    */
    SelectObject(hdcMask,hOldBitmapMask);
    SelectObject(hdcImage, hOldBitmapImage);
    SelectObject(hdcBitmap, hOldBitmap);
    DeleteDC(hdcMask);
    DeleteDC(hdcImage);
    DeleteDC(hdcBitmap);
    if(!himl->hbmMask)
    {
        DeleteObject(hMaskBitmap);
    }

    return nIndex;
}


/*************************************************************************
 * ImageList_BeginDrag [COMCTL32.43]
 *
 * Creates a temporary image list that contains one image. It will be used
 * as a drag image.
 *
 * PARAMS
 *     himlTrack [I] handle to the source image list
 *     iTrack    [I] index of the drag image in the source image list
 *     dxHotspot [I] X position of the hot spot of the drag image
 *     dyHotspot [I] Y position of the hot spot of the drag image
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_BeginDrag (HIMAGELIST himlTrack, INT iTrack,
	             INT dxHotspot, INT dyHotspot)
{
    HDC hdcSrc, hdcDst;
    INT cx, cy;

    TRACE("(himlTrack=%p iTrack=%d dx=%d dy=%d)\n", himlTrack, iTrack,
	  dxHotspot, dyHotspot);

    if (himlTrack == NULL)
	return FALSE;

    if (InternalDrag.himl)
        ImageList_EndDrag ();

    cx = himlTrack->cx;
    cy = himlTrack->cy;

    InternalDrag.himl = ImageList_Create (cx, cy, himlTrack->flags, 1, 1);
    if (InternalDrag.himl == NULL) {
        ERR("Error creating drag image list!\n");
        return FALSE;
    }

    InternalDrag.dxHotspot = dxHotspot;
    InternalDrag.dyHotspot = dyHotspot;

    hdcSrc = CreateCompatibleDC (0);
    hdcDst = CreateCompatibleDC (0);

    /* copy image */
    SelectObject (hdcSrc, himlTrack->hbmImage);
    SelectObject (hdcDst, InternalDrag.himl->hbmImage);
    BitBlt (hdcDst, 0, 0, cx, cy, hdcSrc, iTrack * cx, 0, SRCCOPY);

    /* copy mask */
    SelectObject (hdcSrc, himlTrack->hbmMask);
    SelectObject (hdcDst, InternalDrag.himl->hbmMask);
    BitBlt (hdcDst, 0, 0, cx, cy, hdcSrc, iTrack * cx, 0, SRCCOPY);

    DeleteDC (hdcSrc);
    DeleteDC (hdcDst);

    InternalDrag.himl->cCurImage = 1;
    InternalDrag.bHSPending = TRUE;

    return TRUE;
}


/*************************************************************************
 * ImageList_Copy [COMCTL32.44]
 *
 *  Copies an image of the source image list to an image of the
 *  destination image list. Images can be copied or swapped.
 *
 * PARAMS
 *     himlDst [I] handle to the destination image list
 *     iDst    [I] destination image index.
 *     himlSrc [I] handle to the source image list
 *     iSrc    [I] source image index
 *     uFlags  [I] flags for the copy operation
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     Copying from one image list to another is possible. The original
 *     implementation just copies or swaps within one image list.
 *     Could this feature become a bug??? ;-)
 */

BOOL WINAPI
ImageList_Copy (HIMAGELIST himlDst, INT iDst,	HIMAGELIST himlSrc,
		INT iSrc, INT uFlags)
{
    HDC hdcSrc, hdcDst;

    TRACE("iDst=%d  iSrc=%d\n", iDst, iSrc);

    if ((himlSrc == NULL) || (himlDst == NULL))
	return FALSE;
    if ((iDst < 0) || (iDst >= himlDst->cCurImage))
	return FALSE;
    if ((iSrc < 0) || (iSrc >= himlSrc->cCurImage))
	return FALSE;

    hdcSrc = CreateCompatibleDC (0);
    if (himlDst == himlSrc)
        hdcDst = hdcSrc;
    else
        hdcDst = CreateCompatibleDC (0);

    if (uFlags & ILCF_SWAP) {
        /* swap */
        HBITMAP hbmTempImage, hbmTempMask;

        /* create temporary bitmaps */
        hbmTempImage = CreateBitmap (himlSrc->cx, himlSrc->cy, 1,
                                       himlSrc->uBitsPixel, NULL);
        hbmTempMask = CreateBitmap (himlSrc->cx, himlSrc->cy, 1,
				      1, NULL);

        /* copy (and stretch) destination to temporary bitmaps.(save) */
        /* image */
        SelectObject (hdcSrc, himlDst->hbmImage);
        SelectObject (hdcDst, hbmTempImage);
        StretchBlt (hdcDst, 0, 0, himlSrc->cx, himlSrc->cy,
                      hdcSrc, iDst * himlDst->cx, 0, himlDst->cx, himlDst->cy,
                      SRCCOPY);
        /* mask */
        SelectObject (hdcSrc, himlDst->hbmMask);
        SelectObject (hdcDst, hbmTempMask);
        StretchBlt (hdcDst, 0, 0, himlSrc->cx, himlSrc->cy,
                      hdcSrc, iDst * himlDst->cx, 0, himlDst->cx, himlDst->cy,
                      SRCCOPY);

        /* copy (and stretch) source to destination */
        /* image */
        SelectObject (hdcSrc, himlSrc->hbmImage);
        SelectObject (hdcDst, himlDst->hbmImage);
        StretchBlt (hdcDst, iDst * himlDst->cx, 0, himlDst->cx, himlDst->cy,
                      hdcSrc, iSrc * himlSrc->cx, 0, himlSrc->cx, himlSrc->cy,
                      SRCCOPY);
        /* mask */
        SelectObject (hdcSrc, himlSrc->hbmMask);
        SelectObject (hdcDst, himlDst->hbmMask);
        StretchBlt (hdcDst, iDst * himlDst->cx, 0, himlDst->cx, himlDst->cy,
                      hdcSrc, iSrc * himlSrc->cx, 0, himlSrc->cx, himlSrc->cy,
                      SRCCOPY);

        /* copy (without stretching) temporary bitmaps to source (restore) */
        /* image */
        SelectObject (hdcSrc, hbmTempImage);
        SelectObject (hdcDst, himlSrc->hbmImage);
        BitBlt (hdcDst, iSrc * himlSrc->cx, 0, himlSrc->cx, himlSrc->cy,
                  hdcSrc, 0, 0, SRCCOPY);
        /* mask */
        SelectObject (hdcSrc, hbmTempMask);
        SelectObject (hdcDst, himlSrc->hbmMask);
        BitBlt (hdcDst, iSrc * himlSrc->cx, 0, himlSrc->cx, himlSrc->cy,
                  hdcSrc, 0, 0, SRCCOPY);

        /* delete temporary bitmaps */
        DeleteObject (hbmTempMask);
        DeleteObject (hbmTempImage);
    }
    else {
        /* copy image */
        SelectObject (hdcSrc, himlSrc->hbmImage);
        if (himlSrc == himlDst)
            hdcDst = hdcSrc;
        else
            SelectObject (hdcDst, himlDst->hbmImage);
        StretchBlt (hdcDst, iDst * himlDst->cx, 0, himlDst->cx, himlDst->cy,
                      hdcSrc, iSrc * himlSrc->cx, 0, himlSrc->cx, himlSrc->cy,
                      SRCCOPY);

        /* copy mask */
        SelectObject (hdcSrc, himlSrc->hbmMask);
        if (himlSrc == himlDst)
            hdcDst = hdcSrc;
        else
            SelectObject (hdcDst, himlDst->hbmMask);
        StretchBlt (hdcDst, iDst * himlDst->cx, 0, himlDst->cx, himlDst->cy,
                      hdcSrc, iSrc * himlSrc->cx, 0, himlSrc->cx, himlSrc->cy,
                      SRCCOPY);
    }

    DeleteDC (hdcSrc);
    if (himlSrc != himlDst)
        DeleteDC (hdcDst);

    return TRUE;
}


/*************************************************************************
 * ImageList_Create [COMCTL32.45]  Creates a new image list.
 *
 * PARAMS
 *     cx       [I] image height
 *     cy       [I] image width
 *     flags    [I] creation flags
 *     cInitial [I] initial number of images in the image list
 *     cGrow    [I] number of images by which image list grows
 *
 * RETURNS
 *     Success: Handle to the created image list
 *     Failure: NULL
 */

HIMAGELIST WINAPI
ImageList_Create (INT cx, INT cy, UINT flags,
		  INT cInitial, INT cGrow)
{
    HIMAGELIST himl;
    HDC      hdc;
    INT      nCount;
    HBITMAP  hbmTemp;
    static WORD aBitBlend25[] =
        {0xAA, 0x00, 0x55, 0x00, 0xAA, 0x00, 0x55, 0x00};

    static WORD aBitBlend50[] =
        {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA};

    TRACE("(%d %d 0x%x %d %d)\n", cx, cy, flags, cInitial, cGrow);

    himl = (HIMAGELIST)COMCTL32_Alloc (sizeof(struct _IMAGELIST));
    if (!himl)
        return NULL;

    himl->cx        = cx;
    himl->cy        = cy;
    himl->flags     = flags;
    himl->cMaxImage = cInitial + cGrow;
    himl->cInitial  = cInitial;
    himl->cGrow     = cGrow;
    himl->cCurImage = 0;
    himl->clrFg     = CLR_DEFAULT;
    himl->clrBk     = CLR_NONE;

    /* initialize overlay mask indices */
    for (nCount = 0; nCount < MAX_OVERLAYIMAGE; nCount++)
        himl->nOvlIdx[nCount] = -1;

    hdc = CreateCompatibleDC (0);
    himl->uBitsPixel = (UINT)GetDeviceCaps (hdc, BITSPIXEL);
    DeleteDC (hdc);

    TRACE("Image: %d Bits per Pixel\n", himl->uBitsPixel);

    if (himl->cMaxImage > 0) {
        himl->hbmImage =
	  CreateBitmap (himl->cx * himl->cMaxImage, himl->cy,
                        1, himl->uBitsPixel, NULL);
	if (himl->hbmImage == 0) {
	    ERR("Error creating image bitmap!\n");
	    return NULL;
	}
    }
    else
        himl->hbmImage = 0;

    if ( (himl->cMaxImage > 0) && (himl->flags & ILC_MASK)) {
        himl->hbmMask = CreateBitmap (himl->cx * himl->cMaxImage, himl->cy,
					1, 1, NULL);
        if (himl->hbmMask == 0) {
            ERR("Error creating mask bitmap!\n");
            if (himl->hbmImage)
                DeleteObject (himl->hbmImage);
            return NULL;
        }
    }
    else
        himl->hbmMask = 0;

    /* create blending brushes */
    hbmTemp = CreateBitmap (8, 8, 1, 1, &aBitBlend25);
    himl->hbrBlend25 = CreatePatternBrush (hbmTemp);
    DeleteObject (hbmTemp);

    hbmTemp = CreateBitmap (8, 8, 1, 1, &aBitBlend50);
    himl->hbrBlend50 = CreatePatternBrush (hbmTemp);
    DeleteObject (hbmTemp);

    TRACE("created imagelist %p\n", himl);
    return himl;
}


/*************************************************************************
 * ImageList_Destroy [COMCTL32.46]
 *
 * Destroys an image list.
 *
 * PARAMS
 *     himl [I] handle to image list
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_Destroy (HIMAGELIST himl)
{
    if (!himl)
	return FALSE;

    /* delete image bitmaps */
    if (himl->hbmImage)
        DeleteObject (himl->hbmImage);
    if (himl->hbmMask)
        DeleteObject (himl->hbmMask);

    /* delete blending brushes */
    if (himl->hbrBlend25)
        DeleteObject (himl->hbrBlend25);
    if (himl->hbrBlend50)
        DeleteObject (himl->hbrBlend50);

    COMCTL32_Free (himl);

    return TRUE;
}


/*************************************************************************
 * ImageList_DragEnter [COMCTL32.47]
 *
 * Locks window update and displays the drag image at the given position.
 *
 * PARAMS
 *     hwndLock [I] handle of the window that owns the drag image.
 *     x        [I] X position of the drag image.
 *     y        [I] Y position of the drag image.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     The position of the drag image is relative to the window, not
 *     the client area.
 */

BOOL WINAPI
ImageList_DragEnter (HWND hwndLock, INT x, INT y)
{
    TRACE("(hwnd=%#x x=%d y=%d)\n", hwndLock, x, y);

    if (InternalDrag.himl == NULL)
	return FALSE;

    if (hwndLock)
	InternalDrag.hwnd = hwndLock;
    else
	InternalDrag.hwnd = GetDesktopWindow ();

    InternalDrag.x = x;
    InternalDrag.y = y;

    /* draw the drag image and save the background */
    if (!ImageList_DragShowNolock(TRUE)) {
	return FALSE;
    }

    return TRUE;
}


/*************************************************************************
 * ImageList_DragLeave [COMCTL32.48]
 *
 * Unlocks window update and hides the drag image.
 *
 * PARAMS
 *     hwndLock [I] handle of the window that owns the drag image.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_DragLeave (HWND hwndLock)
{
    /* As we don't save drag info in the window this can lead to problems if
       an app does not supply the same window as DragEnter */
    /* if (hwndLock)
	InternalDrag.hwnd = hwndLock;
    else
	InternalDrag.hwnd = GetDesktopWindow (); */
    if(!hwndLock)
	hwndLock = GetDesktopWindow();
    if(InternalDrag.hwnd != hwndLock)
	FIXME("DragLeave hWnd != DragEnter hWnd\n");

    ImageList_DragShowNolock (FALSE);

    return TRUE;
}


/*************************************************************************
 * ImageList_DragMove [COMCTL32.49]
 *
 * Moves the drag image.
 *
 * PARAMS
 *     x [I] X position of the drag image.
 *     y [I] Y position of the drag image.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     The position of the drag image is relative to the window, not
 *     the client area.
 *
 * BUGS
 *     The drag image should be drawn semitransparent.
 */

BOOL WINAPI
ImageList_DragMove (INT x, INT y)
{
    TRACE("(x=%d y=%d)\n", x, y);

    if (!InternalDrag.himl) {
	return FALSE;
    }

    /* draw/update the drag image */
    if (InternalDrag.bShow) {
	HDC hdcDrag;
	HDC hdcOffScreen;
	HDC hdcBg;
	HBITMAP hbmOffScreen;
	INT origNewX, origNewY;
	INT origOldX, origOldY;
	INT origRegX, origRegY;
	INT sizeRegX, sizeRegY;


	/* calculate the update region */
	origNewX = x - InternalDrag.dxHotspot;
	origNewY = y - InternalDrag.dyHotspot;
	origOldX = InternalDrag.x - InternalDrag.dxHotspot;
	origOldY = InternalDrag.y - InternalDrag.dyHotspot;
	origRegX = min(origNewX, origOldX);
	origRegY = min(origNewY, origOldY);
	sizeRegX = InternalDrag.himl->cx + abs(x - InternalDrag.x);
	sizeRegY = InternalDrag.himl->cy + abs(y - InternalDrag.y);

	hdcDrag = GetDCEx(InternalDrag.hwnd, 0,
   			  DCX_WINDOW | DCX_CACHE | DCX_LOCKWINDOWUPDATE);
    	hdcOffScreen = CreateCompatibleDC(hdcDrag);
    	hdcBg = CreateCompatibleDC(hdcDrag);

	hbmOffScreen = CreateCompatibleBitmap(hdcDrag, sizeRegX, sizeRegY);
	SelectObject(hdcOffScreen, hbmOffScreen);
	SelectObject(hdcBg, InternalDrag.hbmBg);

	/* get the actual background of the update region */
	BitBlt(hdcOffScreen, 0, 0, sizeRegX, sizeRegY, hdcDrag,
	       origRegX, origRegY, SRCCOPY);
	/* erase the old image */
	BitBlt(hdcOffScreen, origOldX - origRegX, origOldY - origRegY,
	       InternalDrag.himl->cx, InternalDrag.himl->cy, hdcBg, 0, 0,
	       SRCCOPY);
	/* save the background */
	BitBlt(hdcBg, 0, 0, InternalDrag.himl->cx, InternalDrag.himl->cy,
	       hdcOffScreen, origNewX - origRegX, origNewY - origRegY, SRCCOPY);
	/* draw the image */
	/* FIXME: image should be drawn semitransparent */
	ImageList_Draw(InternalDrag.himl, 0, hdcOffScreen, origNewX - origRegX,
		       origNewY - origRegY, ILD_NORMAL);
	/* draw the update region to the screen */
	BitBlt(hdcDrag, origRegX, origRegY, sizeRegX, sizeRegY,
	       hdcOffScreen, 0, 0, SRCCOPY);

	DeleteDC(hdcBg);
	DeleteDC(hdcOffScreen);
	DeleteObject(hbmOffScreen);
	ReleaseDC(InternalDrag.hwnd, hdcDrag);
    }

    /* update the image position */
    InternalDrag.x = x;
    InternalDrag.y = y;

    return TRUE;
}


/*************************************************************************
 * ImageList_DragShowNolock [COMCTL32.50]
 *
 * Shows or hides the drag image.
 *
 * PARAMS
 *     bShow [I] TRUE shows the drag image, FALSE hides it.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * BUGS
 *     The drag image should be drawn semitransparent.
 */

BOOL WINAPI
ImageList_DragShowNolock (BOOL bShow)
{
    HDC hdcDrag;
    HDC hdcBg;
    INT x, y;

    TRACE("bShow=0x%X!\n", bShow);

    /* DragImage is already visible/hidden */
    if ((InternalDrag.bShow && bShow) || (!InternalDrag.bShow && !bShow)) {
	return FALSE;
    }

    /* position of the origin of the DragImage */
    x = InternalDrag.x - InternalDrag.dxHotspot;
    y = InternalDrag.y - InternalDrag.dyHotspot;

    hdcDrag = GetDCEx (InternalDrag.hwnd, 0,
			 DCX_WINDOW | DCX_CACHE | DCX_LOCKWINDOWUPDATE);
    if (!hdcDrag) {
	return FALSE;
    }

    hdcBg = CreateCompatibleDC(hdcDrag);
    if (!InternalDrag.hbmBg) {
	InternalDrag.hbmBg = CreateCompatibleBitmap(hdcDrag,
		    InternalDrag.himl->cx, InternalDrag.himl->cy);
    }
    SelectObject(hdcBg, InternalDrag.hbmBg);

    if (bShow) {
	/* save the background */
	BitBlt(hdcBg, 0, 0, InternalDrag.himl->cx, InternalDrag.himl->cy,
	       hdcDrag, x, y, SRCCOPY);
	/* show the image */
	/* FIXME: this should be drawn semitransparent */
	ImageList_Draw(InternalDrag.himl, 0, hdcDrag, x, y, ILD_NORMAL);
    } else {
	/* hide the image */
	BitBlt(hdcDrag, x, y, InternalDrag.himl->cx, InternalDrag.himl->cy,
	       hdcBg, 0, 0, SRCCOPY);
    }

    InternalDrag.bShow = !InternalDrag.bShow;

    DeleteDC(hdcBg);
    ReleaseDC (InternalDrag.hwnd, hdcDrag);
    return TRUE;
}


/*************************************************************************
 * ImageList_Draw [COMCTL32.51] Draws an image.
 *
 * PARAMS
 *     himl   [I] handle to image list
 *     i      [I] image index
 *     hdc    [I] handle to device context
 *     x      [I] x position
 *     y      [I] y position
 *     fStyle [I] drawing flags
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     Calls ImageList_DrawIndirect.
 *
 * SEE
 *     ImageList_DrawIndirect.
 */

BOOL WINAPI
ImageList_Draw (HIMAGELIST himl, INT i, HDC hdc,
		INT x, INT y, UINT fStyle)
{
    IMAGELISTDRAWPARAMS imldp;

    imldp.cbSize  = sizeof(IMAGELISTDRAWPARAMS);
    imldp.himl    = himl;
    imldp.i       = i;
    imldp.hdcDst  = hdc,
    imldp.x       = x;
    imldp.y       = y;
    imldp.cx      = 0;
    imldp.cy      = 0;
    imldp.xBitmap = 0;
    imldp.yBitmap = 0;
    imldp.rgbBk   = CLR_DEFAULT;
    imldp.rgbFg   = CLR_DEFAULT;
    imldp.fStyle  = fStyle;
    imldp.dwRop   = 0;

    return ImageList_DrawIndirect (&imldp);
}


/*************************************************************************
 * ImageList_DrawEx [COMCTL32.52]
 *
 * Draws an image and allows to use extended drawing features.
 *
 * PARAMS
 *     himl   [I] handle to image list
 *     i      [I] image index
 *     hdc    [I] handle to device context
 *     x      [I] X position
 *     y      [I] Y position
 *     xOffs  [I] X offset
 *     yOffs  [I] Y offset
 *     rgbBk  [I] background color
 *     rgbFg  [I] foreground color
 *     fStyle [I] drawing flags
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     Calls ImageList_DrawIndirect.
 *
 * SEE
 *     ImageList_DrawIndirect.
 */

BOOL WINAPI
ImageList_DrawEx (HIMAGELIST himl, INT i, HDC hdc, INT x, INT y,
		  INT dx, INT dy, COLORREF rgbBk, COLORREF rgbFg,
		  UINT fStyle)
{
    IMAGELISTDRAWPARAMS imldp;

    imldp.cbSize  = sizeof(IMAGELISTDRAWPARAMS);
    imldp.himl    = himl;
    imldp.i       = i;
    imldp.hdcDst  = hdc,
    imldp.x       = x;
    imldp.y       = y;
    imldp.cx      = dx;
    imldp.cy      = dy;
    imldp.xBitmap = 0;
    imldp.yBitmap = 0;
    imldp.rgbBk   = rgbBk;
    imldp.rgbFg   = rgbFg;
    imldp.fStyle  = fStyle;
    imldp.dwRop   = 0;

    return ImageList_DrawIndirect (&imldp);
}


/*************************************************************************
 * ImageList_DrawIndirect [COMCTL32.53]
 *
 * Draws an image using ...
 *
 * PARAMS
 *     pimldp [I] pointer to IMAGELISTDRAWPARAMS structure.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_DrawIndirect (IMAGELISTDRAWPARAMS *pimldp)
{
    INT      cx, cy;
    /*
        Do some Error Checking
    */
    if (pimldp == NULL)
        return FALSE;
    if (pimldp->cbSize < sizeof(IMAGELISTDRAWPARAMS))
        return FALSE;
    if (pimldp->himl == NULL)
        return FALSE;
    if ((pimldp->i < 0) || (pimldp->i >= pimldp->himl->cCurImage)) {
	ERR("%d not within range (max %d)\n",pimldp->i,pimldp->himl->cCurImage-1);
        return FALSE;
    }
    /*
        Get the Height and Width to display
    */
    cx = (pimldp->cx == 0) ? pimldp->himl->cx : pimldp->cx;
    cy = (pimldp->cy == 0) ? pimldp->himl->cy : pimldp->cy;
    /*
        Draw the image
    */
    if(pimldp->himl->hbmMask != 0)
    {
        IMAGELIST_InternalDrawMask(pimldp, cx, cy);
    }
    else
    {
        IMAGELIST_InternalDraw(pimldp, cx, cy);
    }
    /*
        Apply the blend if needed to the Image
    */
    if((pimldp->fStyle & ILD_BLEND50)
        || (pimldp->fStyle & ILD_BLEND25))
    {
        IMAGELIST_InternalDrawBlend(pimldp, cx, cy);
    }
    /*
        Apply the Overlay if needed
    */
    if (pimldp->fStyle & ILD_OVERLAYMASK)
    {
        IMAGELIST_InternalDrawOverlay(pimldp, cx, cy);
    }

    return TRUE;
}


/*************************************************************************
 * ImageList_Duplicate [COMCTL32.54] Duplicates an image list.
 *
 * PARAMS
 *     himlSrc [I] source image list handle
 *
 * RETURNS
 *     Success: Handle of duplicated image list.
 *     Failure: NULL
 */

HIMAGELIST WINAPI
ImageList_Duplicate (HIMAGELIST himlSrc)
{
    HIMAGELIST himlDst;
    HDC hdcSrc, hdcDst;

    if (himlSrc == NULL) {
        ERR("Invalid image list handle!\n");
        return NULL;
    }

    himlDst = ImageList_Create (himlSrc->cx, himlSrc->cy, himlSrc->flags,
                                himlSrc->cInitial, himlSrc->cGrow);

    if (himlDst)
    {
        hdcSrc = CreateCompatibleDC (0);
        hdcDst = CreateCompatibleDC (0);
        SelectObject (hdcSrc, himlSrc->hbmImage);
        SelectObject (hdcDst, himlDst->hbmImage);
        BitBlt (hdcDst, 0, 0, himlSrc->cCurImage * himlSrc->cx, himlSrc->cy,
                  hdcSrc, 0, 0, SRCCOPY);

        if (himlDst->hbmMask)
        {
            SelectObject (hdcSrc, himlSrc->hbmMask);
            SelectObject (hdcDst, himlDst->hbmMask);
            BitBlt (hdcDst, 0, 0, himlSrc->cCurImage * himlSrc->cx,
                      himlSrc->cy, hdcSrc, 0, 0, SRCCOPY);
        }

        DeleteDC (hdcDst);
        DeleteDC (hdcSrc);

	himlDst->cCurImage = himlSrc->cCurImage;
	himlDst->cMaxImage = himlSrc->cMaxImage;
    }
    return himlDst;
}


/*************************************************************************
 * ImageList_EndDrag [COMCTL32.55] Finishes a drag operation.
 *
 * Finishes a drag operation.
 *
 * PARAMS
 *     no Parameters
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_EndDrag (void)
{
    /* cleanup the InternalDrag struct */
    InternalDrag.hwnd = 0;
    ImageList_Destroy (InternalDrag.himl);
    InternalDrag.himl = 0;
    InternalDrag.x= 0;
    InternalDrag.y= 0;
    InternalDrag.dxHotspot = 0;
    InternalDrag.dyHotspot = 0;
    InternalDrag.bShow = FALSE;
    DeleteObject(InternalDrag.hbmBg);
    InternalDrag.hbmBg = 0;
    InternalDrag.bHSPending = FALSE;

    return TRUE;
}


/*************************************************************************
 * ImageList_GetBkColor [COMCTL32.56]
 *
 * Returns the background color of an image list.
 *
 * PARAMS
 *     himl [I] Image list handle.
 *
 * RETURNS
 *     Success: background color
 *     Failure: CLR_NONE
 */

COLORREF WINAPI
ImageList_GetBkColor (HIMAGELIST himl)
{
    if (himl == NULL)
	return CLR_NONE;

    return himl->clrBk;
}


/*************************************************************************
 * ImageList_GetDragImage [COMCTL32.57]
 *
 * Returns the handle to the internal drag image list.
 *
 * PARAMS
 *     ppt        [O] Pointer to the drag position. Can be NULL.
 *     pptHotspot [O] Pointer to the position of the hot spot. Can be NULL.
 *
 * RETURNS
 *     Success: Handle of the drag image list.
 *     Failure: NULL.
 */

HIMAGELIST WINAPI
ImageList_GetDragImage (POINT *ppt, POINT *pptHotspot)
{
    if (InternalDrag.himl) {
	if (ppt) {
	    ppt->x = InternalDrag.x;
	    ppt->y = InternalDrag.y;
	}
	if (pptHotspot) {
	    pptHotspot->x = InternalDrag.dxHotspot;
	    pptHotspot->y = InternalDrag.dyHotspot;
	}
        return (InternalDrag.himl);
    }

    return NULL;
}


/*************************************************************************
 * ImageList_GetFlags [COMCTL32.58]
 *
 * BUGS
 *    Stub.
 */

DWORD WINAPI
ImageList_GetFlags(HIMAGELIST himl)
{
    FIXME("(%p):empty stub\n", himl);
    return 0;
}


/*************************************************************************
 * ImageList_GetIcon [COMCTL32.59]
 *
 * Creates an icon from a masked image of an image list.
 *
 * PARAMS
 *     himl  [I] handle to image list
 *     i     [I] image index
 *     flags [I] drawing style flags
 *
 * RETURNS
 *     Success: icon handle
 *     Failure: NULL
 */

HICON WINAPI
ImageList_GetIcon (HIMAGELIST himl, INT i, UINT fStyle)
{
    ICONINFO ii;
    HICON  hIcon;
    HBITMAP hOldSrcBitmap,hOldDstBitmap;
    HDC    hdcSrc, hdcDst;

    if ((himl == NULL) || (i < 0) || (i >= himl->cCurImage)) {
	FIXME("(%p,%d,%x), params out of range!\n",himl,i,fStyle);
	return 0;
   }

    hdcSrc = CreateCompatibleDC(0);
    hdcDst = CreateCompatibleDC(0);

    ii.fIcon = TRUE;
    ii.hbmMask  = CreateCompatibleBitmap (hdcDst, himl->cx, himl->cy);

    /* draw mask*/
    hOldDstBitmap = (HBITMAP)SelectObject (hdcDst, ii.hbmMask);
    if (himl->hbmMask) {
	SelectObject (hdcSrc, himl->hbmMask);
	BitBlt (hdcDst, 0, 0, himl->cx, himl->cy,
		  hdcSrc, i * himl->cx, 0, SRCCOPY);
    }
    else
	PatBlt (hdcDst, 0, 0, himl->cx, himl->cy, BLACKNESS);

    /* draw image*/
    hOldSrcBitmap = (HBITMAP)SelectObject (hdcSrc, himl->hbmImage);
    ii.hbmColor = CreateCompatibleBitmap (hdcSrc, himl->cx, himl->cy);
    SelectObject (hdcDst, ii.hbmColor);
    BitBlt (hdcDst, 0, 0, himl->cx, himl->cy,
	      hdcSrc, i * himl->cx, 0, SRCCOPY);

    /*
     * CreateIconIndirect requires us to deselect the bitmaps from
     * the DCs before calling
     */
    SelectObject(hdcSrc, hOldSrcBitmap);
    SelectObject(hdcDst, hOldDstBitmap);

    hIcon = CreateIconIndirect (&ii);

    DeleteDC (hdcSrc);
    DeleteDC (hdcDst);
    DeleteObject (ii.hbmMask);
    DeleteObject (ii.hbmColor);

    return hIcon;
}


/*************************************************************************
 * ImageList_GetIconSize [COMCTL32.60]
 *
 * Retrieves the size of an image in an image list.
 *
 * PARAMS
 *     himl [I] handle to image list
 *     cx   [O] pointer to the image width.
 *     cy   [O] pointer to the image height.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     All images in an image list have the same size.
 */

BOOL WINAPI
ImageList_GetIconSize (HIMAGELIST himl, INT *cx, INT *cy)
{
    if (himl == NULL)
	return FALSE;
    if ((himl->cx <= 0) || (himl->cy <= 0))
	return FALSE;

    if (cx)
	*cx = himl->cx;
    if (cy)
	*cy = himl->cy;

    return TRUE;
}


/*************************************************************************
 * ImageList_GetImageCount [COMCTL32.61]
 *
 * Returns the number of images in an image list.
 *
 * PARAMS
 *     himl [I] handle to image list
 *
 * RETURNS
 *     Success: Number of images.
 *     Failure: 0
 */

INT WINAPI
ImageList_GetImageCount (HIMAGELIST himl)
{
    if (himl == NULL)
	return 0;

    return himl->cCurImage;
}


/*************************************************************************
 * ImageList_GetImageInfo [COMCTL32.62]
 *
 * Returns information about an image in an image list.
 *
 * PARAMS
 *     himl       [I] handle to image list
 *     i          [I] image index
 *     pImageInfo [O] pointer to the image information
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_GetImageInfo (HIMAGELIST himl, INT i, IMAGEINFO *pImageInfo)
{
    if ((himl == NULL) || (pImageInfo == NULL))
	return FALSE;
    if ((i < 0) || (i >= himl->cCurImage))
	return FALSE;

    pImageInfo->hbmImage = himl->hbmImage;
    pImageInfo->hbmMask  = himl->hbmMask;

    pImageInfo->rcImage.top    = 0;
    pImageInfo->rcImage.bottom = himl->cy;
    pImageInfo->rcImage.left   = i * himl->cx;
    pImageInfo->rcImage.right  = (i+1) * himl->cx;

    return TRUE;
}


/*************************************************************************
 * ImageList_GetImageRect [COMCTL32.63]
 *
 * Retrieves the rectangle of the specified image in an image list.
 *
 * PARAMS
 *     himl   [I] handle to image list
 *     i      [I] image index
 *     lpRect [O] pointer to the image rectangle
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 *
 * NOTES
 *    This is an UNDOCUMENTED function!!!
 */

BOOL WINAPI
ImageList_GetImageRect (HIMAGELIST himl, INT i, LPRECT lpRect)
{
    if ((himl == NULL) || (lpRect == NULL))
	return FALSE;
    if ((i < 0) || (i >= himl->cCurImage))
	return FALSE;

    lpRect->left   = i * himl->cx;
    lpRect->top    = 0;
    lpRect->right  = lpRect->left + himl->cx;
    lpRect->bottom = himl->cy;

    return TRUE;
}


/*************************************************************************
 * ImageList_LoadImage  [COMCTL32.64]
 * ImageList_LoadImageA [COMCTL32.65]
 *
 * Creates an image list from a bitmap, icon or cursor.
 *
 * PARAMS
 *     hi      [I] instance handle
 *     lpbmp   [I] name or id of the image
 *     cx      [I] width of each image
 *     cGrow   [I] number of images to expand
 *     clrMask [I] mask color
 *     uType   [I] type of image to load
 *     uFlags  [I] loading flags
 *
 * RETURNS
 *     Success: handle to the loaded image list
 *     Failure: NULL
 *
 * SEE
 *     LoadImage ()
 */

HIMAGELIST WINAPI
ImageList_LoadImageA (HINSTANCE hi, LPCSTR lpbmp, INT cx, INT cGrow,
			COLORREF clrMask, UINT uType, UINT uFlags)
{
    HIMAGELIST himl = NULL;
    HANDLE   handle;
    INT      nImageCount;

    handle = LoadImageA (hi, lpbmp, uType, 0, 0, uFlags);
    if (!handle) {
        ERR("Error loading image!\n");
        return NULL;
    }

    if (uType == IMAGE_BITMAP) {
        BITMAP bmp;
        GetObjectA (handle, sizeof(BITMAP), &bmp);

        /* To match windows behavior, if cx is set to zero and
         the flag DI_DEFAULTSIZE is specified, cx becomes the
         system metric value for icons. If the flag is not specified
         the function sets the size to the height of the bitmap */
        if (cx == 0)
        {
            if (uFlags & DI_DEFAULTSIZE)
                cx = GetSystemMetrics (SM_CXICON);
            else
                cx = bmp.bmHeight;
        }

        nImageCount = bmp.bmWidth / cx;

        himl = ImageList_Create (cx, bmp.bmHeight, ILC_MASK | ILC_COLOR,
                                 nImageCount, cGrow);
        ImageList_AddMasked (himl, (HBITMAP)handle, clrMask);
    }
    else if ((uType == IMAGE_ICON) || (uType == IMAGE_CURSOR)) {
        ICONINFO ii;
        BITMAP bmp;

        GetIconInfo (handle, &ii);
        GetObjectA (ii.hbmColor, sizeof(BITMAP), (LPVOID)&bmp);
        himl = ImageList_Create (bmp.bmWidth, bmp.bmHeight,
                                 ILC_MASK | ILC_COLOR, 1, cGrow);
        ImageList_Add (himl, ii.hbmColor, ii.hbmMask);
        DeleteObject (ii.hbmColor);
        DeleteObject (ii.hbmMask);
    }

    DeleteObject (handle);

    return himl;
}


/*************************************************************************
 * ImageList_LoadImageW [COMCTL32.66]
 *
 * Creates an image list from a bitmap, icon or cursor.
 *
 * PARAMS
 *     hi      [I] instance handle
 *     lpbmp   [I] name or id of the image
 *     cx      [I] width of each image
 *     cGrow   [I] number of images to expand
 *     clrMask [I] mask color
 *     uType   [I] type of image to load
 *     uFlags  [I] loading flags
 *
 * RETURNS
 *     Success: handle to the loaded image list
 *     Failure: NULL
 *
 * SEE
 *     LoadImage ()
 */

HIMAGELIST WINAPI
ImageList_LoadImageW (HINSTANCE hi, LPCWSTR lpbmp, INT cx, INT cGrow,
			COLORREF clrMask, UINT uType,	UINT uFlags)
{
    HIMAGELIST himl = NULL;
    HANDLE   handle;
    INT      nImageCount;

    handle = LoadImageW (hi, lpbmp, uType, 0, 0, uFlags);
    if (!handle) {
        ERR("Error loading image!\n");
        return NULL;
    }

    if (uType == IMAGE_BITMAP) {
        BITMAP bmp;
        GetObjectA (handle, sizeof(BITMAP), &bmp);
        nImageCount = bmp.bmWidth / cx;

        himl = ImageList_Create (cx, bmp.bmHeight, ILC_MASK | ILC_COLOR,
                                 nImageCount, cGrow);
        ImageList_AddMasked (himl, (HBITMAP)handle, clrMask);
    }
    else if ((uType == IMAGE_ICON) || (uType == IMAGE_CURSOR)) {
        ICONINFO ii;
        BITMAP bmp;

        GetIconInfo (handle, &ii);
        GetObjectA (ii.hbmMask, sizeof(BITMAP), (LPVOID)&bmp);
        himl = ImageList_Create (bmp.bmWidth, bmp.bmHeight,
                                 ILC_MASK | ILC_COLOR, 1, cGrow);
        ImageList_Add (himl, ii.hbmColor, ii.hbmMask);
        DeleteObject (ii.hbmColor);
        DeleteObject (ii.hbmMask);
    }

    DeleteObject (handle);

    return himl;
}


/*************************************************************************
 * ImageList_Merge [COMCTL32.67]
 *
 * Creates a new image list that contains a merged image from the specified
 * images of both source image lists.
 *
 * PARAMS
 *     himl1 [I] handle to first image list
 *     i1    [I] first image index
 *     himl2 [I] handle to second image list
 *     i2    [I] second image index
 *     dx    [I] X offset of the second image relative to the first.
 *     dy    [I] Y offset of the second image relative to the first.
 *
 * RETURNS
 *     Success: handle of the merged image list.
 *     Failure: NULL
 */

HIMAGELIST WINAPI
ImageList_Merge (HIMAGELIST himl1, INT i1, HIMAGELIST himl2, INT i2,
		 INT dx, INT dy)
{
    HIMAGELIST himlDst = NULL;
    HDC      hdcSrcImage, hdcDstImage;
    INT      cxDst, cyDst;
    INT      xOff1, yOff1, xOff2, yOff2;
    INT      nX1, nX2;

    TRACE("(himl1=%p i1=%d himl2=%p i2=%d dx=%d dy=%d)\n", himl1, i1, himl2,
	   i2, dx, dy);

    if ((himl1 == NULL) || (himl2 == NULL))
	return NULL;

    /* check indices */
    if ((i1 < 0) || (i1 >= himl1->cCurImage)) {
        ERR("Index 1 out of range! %d\n", i1);
        return NULL;
    }

    if ((i2 < 0) || (i2 >= himl2->cCurImage)) {
        ERR("Index 2 out of range! %d\n", i2);
        return NULL;
    }

    if (dx > 0) {
        cxDst = max (himl1->cx, dx + himl2->cx);
        xOff1 = 0;
        xOff2 = dx;
    }
    else if (dx < 0) {
        cxDst = max (himl2->cx, himl1->cx - dx);
        xOff1 = -dx;
        xOff2 = 0;
    }
    else {
        cxDst = max (himl1->cx, himl2->cx);
        xOff1 = 0;
        xOff2 = 0;
    }

    if (dy > 0) {
        cyDst = max (himl1->cy, dy + himl2->cy);
        yOff1 = 0;
        yOff2 = dy;
    }
    else if (dy < 0) {
        cyDst = max (himl2->cy, himl1->cy - dy);
        yOff1 = -dy;
        yOff2 = 0;
    }
    else {
        cyDst = max (himl1->cy, himl2->cy);
        yOff1 = 0;
        yOff2 = 0;
    }

    himlDst = ImageList_Create (cxDst, cyDst, ILC_MASK | ILC_COLOR, 1, 1);

    if (himlDst) {
        hdcSrcImage = CreateCompatibleDC (0);
        hdcDstImage = CreateCompatibleDC (0);
        nX1 = i1 * himl1->cx;
        nX2 = i2 * himl2->cx;

        /* copy image */
        SelectObject (hdcSrcImage, himl1->hbmImage);
        SelectObject (hdcDstImage, himlDst->hbmImage);
        BitBlt (hdcDstImage, 0, 0, cxDst, cyDst,
                  hdcSrcImage, 0, 0, BLACKNESS);
        BitBlt (hdcDstImage, xOff1, yOff1, himl1->cx, himl1->cy,
                  hdcSrcImage, nX1, 0, SRCCOPY);

        SelectObject (hdcSrcImage, himl2->hbmMask);
        BitBlt (hdcDstImage, xOff2, yOff2, himl2->cx, himl2->cy,
                  hdcSrcImage, nX2, 0, SRCAND);

        SelectObject (hdcSrcImage, himl2->hbmImage);
        BitBlt (hdcDstImage, xOff2, yOff2, himl2->cx, himl2->cy,
                  hdcSrcImage, nX2, 0, SRCPAINT);

        /* copy mask */
        SelectObject (hdcSrcImage, himl1->hbmMask);
        SelectObject (hdcDstImage, himlDst->hbmMask);
        BitBlt (hdcDstImage, 0, 0, cxDst, cyDst,
                  hdcSrcImage, 0, 0, WHITENESS);
        BitBlt (hdcDstImage, xOff1, yOff1, himl1->cx, himl1->cy,
                  hdcSrcImage, nX1, 0, SRCCOPY);

        SelectObject (hdcSrcImage, himl2->hbmMask);
        BitBlt (hdcDstImage, xOff2, yOff2, himl2->cx, himl2->cy,
                  hdcSrcImage, nX2, 0, SRCAND);

        DeleteDC (hdcSrcImage);
        DeleteDC (hdcDstImage);
	himlDst->cCurImage = 1;
    }

    return himlDst;
}


/* helper for _read_bitmap currently unused */
#if 0
static int may_use_dibsection(HDC hdc) {
    int bitspixel = GetDeviceCaps(hdc,BITSPIXEL)*GetDeviceCaps(hdc,PLANES);
    if (bitspixel>8)
	return TRUE;
    if (bitspixel<=4)
	return FALSE;
    return GetDeviceCaps(hdc,CAPS1) & C1_DIBENGINE;
}
#endif

/* helper for ImageList_Read, see comments below */
static HBITMAP _read_bitmap(LPSTREAM pstm,int ilcFlag,int cx,int cy) {
    HDC			xdc = 0;
    BITMAPFILEHEADER	bmfh;
    BITMAPINFOHEADER	bmih;
    int			bitsperpixel,palspace,longsperline,width,height;
    LPBITMAPINFOHEADER	bmihc = NULL;
    int			result = 0;
    HBITMAP		hbitmap = 0;
    LPBYTE		bits = NULL,nbits = NULL;
    int			nbytesperline,bytesperline;

    if (!SUCCEEDED(IStream_Read ( pstm, &bmfh, sizeof(bmfh), NULL))	||
    	(bmfh.bfType != (('M'<<8)|'B'))					||
    	!SUCCEEDED(IStream_Read ( pstm, &bmih, sizeof(bmih), NULL))	||
    	(bmih.biSize != sizeof(bmih))
    )
	return 0;

    bitsperpixel = bmih.biPlanes * bmih.biBitCount;
    if (bitsperpixel<=8)
    	palspace = (1<<bitsperpixel)*sizeof(RGBQUAD);
    else
    	palspace = 0;
    width = bmih.biWidth;
    height = bmih.biHeight;
    bmihc = (LPBITMAPINFOHEADER)LocalAlloc(LMEM_ZEROINIT,sizeof(bmih)+palspace);
    memcpy(bmihc,&bmih,sizeof(bmih));
    longsperline	= ((width*bitsperpixel+31)&~0x1f)>>5;
    bmihc->biSizeImage	= (longsperline*height)<<2;

    /* read the palette right after the end of the bitmapinfoheader */
    if (palspace)
	if (!SUCCEEDED(IStream_Read ( pstm, bmihc+1, palspace, NULL)))
	    goto ret1;

    xdc = GetDC(0);
#if 0 /* Magic for NxM -> 1x(N*M) not implemented for DIB Sections */
    if ((bitsperpixel>1) &&
	((ilcFlag!=ILC_COLORDDB) && (!ilcFlag || may_use_dibsection(xdc)))
     ) {
	hbitmap = CreateDIBSection(xdc,(BITMAPINFO*)bmihc,0,(LPVOID*)&bits,0,0);
	if (!hbitmap)
	    goto ret1;
	if (!SUCCEEDED(IStream_Read( pstm, bits, bmihc->biSizeImage, NULL)))
	    goto ret1;
	result = 1;
    } else
#endif
    {
	int i,nwidth,nheight;

	nwidth	= width*(height/cy);
	nheight	= cy;

	if (bitsperpixel==1)
	    hbitmap = CreateBitmap(nwidth,nheight,1,1,NULL);
	else
	    hbitmap = CreateCompatibleBitmap(xdc,nwidth,nheight);

	/* Might be a bit excessive memory use here */
	bits = (LPBYTE)LocalAlloc(LMEM_ZEROINIT,bmihc->biSizeImage);
	nbits = (LPBYTE)LocalAlloc(LMEM_ZEROINIT,bmihc->biSizeImage);
	if (!SUCCEEDED(IStream_Read ( pstm, bits, bmihc->biSizeImage, NULL)))
		goto ret1;

	/* Copy the NxM bitmap into a 1x(N*M) bitmap we need, linewise */
	/* Do not forget that windows bitmaps are bottom->top */
	bytesperline	= longsperline*4;
	nbytesperline	= (height/cy)*bytesperline;
	for (i=0;i<height;i++) {
	    memcpy(
		nbits+((height-1-i)%cy)*nbytesperline+(i/cy)*bytesperline,
		bits+bytesperline*(height-1-i),
		bytesperline
	    );
	}
	bmihc->biWidth	= nwidth;
	bmihc->biHeight	= nheight;
	if (!SetDIBits(xdc,hbitmap,0,nheight,nbits,(BITMAPINFO*)bmihc,0))
		goto ret1;
	LocalFree((HLOCAL)nbits);
	LocalFree((HLOCAL)bits);
	result = 1;
    }
ret1:
    if (xdc)	ReleaseDC(0,xdc);
    if (bmihc)	LocalFree((HLOCAL)bmihc);
    if (!result) {
	if (hbitmap) {
	    DeleteObject(hbitmap);
	    hbitmap = 0;
	}
    }
    return hbitmap;
}

/*************************************************************************
 * ImageList_Read [COMCTL32.68]
 *
 * Reads an image list from a stream.
 *
 * PARAMS
 *     pstm [I] pointer to a stream
 *
 * RETURNS
 *     Success: handle to image list
 *     Failure: NULL
 *
 * The format is like this:
 * 	ILHEAD 			ilheadstruct;
 *
 * for the color image part:
 * 	BITMAPFILEHEADER	bmfh;
 * 	BITMAPINFOHEADER	bmih;
 * only if it has a palette:
 *	RGBQUAD		rgbs[nr_of_paletted_colors];
 *
 *	BYTE			colorbits[imagesize];
 *
 * the following only if the ILC_MASK bit is set in ILHEAD.ilFlags:
 *	BITMAPFILEHEADER	bmfh_mask;
 *	BITMAPINFOHEADER	bmih_mask;
 * only if it has a palette (it usually does not):
 *	RGBQUAD		rgbs[nr_of_paletted_colors];
 *
 *	BYTE			maskbits[imagesize];
 *
 * CAVEAT: Those images are within a NxM bitmap, not the 1xN we expect.
 *         _read_bitmap needs to convert them.
 */
HIMAGELIST WINAPI ImageList_Read (LPSTREAM pstm)
{
    ILHEAD	ilHead;
    HIMAGELIST	himl;
    HBITMAP	hbmColor=0,hbmMask=0;
    int		i;

    if (!SUCCEEDED(IStream_Read (pstm, &ilHead, sizeof(ILHEAD), NULL)))
    	return NULL;
    if (ilHead.usMagic != (('L' << 8) | 'I'))
	return NULL;
    if (ilHead.usVersion != 0x101) /* probably version? */
	return NULL;

#if 0
    FIXME("	ilHead.cCurImage = %d\n",ilHead.cCurImage);
    FIXME("	ilHead.cMaxImage = %d\n",ilHead.cMaxImage);
    FIXME("	ilHead.cGrow = %d\n",ilHead.cGrow);
    FIXME("	ilHead.cx = %d\n",ilHead.cx);
    FIXME("	ilHead.cy = %d\n",ilHead.cy);
    FIXME("	ilHead.flags = %x\n",ilHead.flags);
    FIXME("	ilHead.ovls[0] = %d\n",ilHead.ovls[0]);
    FIXME("	ilHead.ovls[1] = %d\n",ilHead.ovls[1]);
    FIXME("	ilHead.ovls[2] = %d\n",ilHead.ovls[2]);
    FIXME("	ilHead.ovls[3] = %d\n",ilHead.ovls[3]);
#endif

    hbmColor = _read_bitmap(pstm,ilHead.flags & ~ILC_MASK,ilHead.cx,ilHead.cy);
    if (!hbmColor)
	return NULL;
    if (ilHead.flags & ILC_MASK) {
	hbmMask = _read_bitmap(pstm,0,ilHead.cx,ilHead.cy);
	if (!hbmMask) {
	    DeleteObject(hbmColor);
	    return NULL;
	}
    }

    himl = ImageList_Create (
		    ilHead.cx,
		    ilHead.cy,
		    ilHead.flags,
		    1,		/* initial */
		    ilHead.cGrow
    );
    if (!himl) {
	DeleteObject(hbmColor);
	DeleteObject(hbmMask);
	return NULL;
    }
    himl->hbmImage = hbmColor;
    himl->hbmMask = hbmMask;
    himl->cCurImage = ilHead.cCurImage;
    himl->cMaxImage = ilHead.cMaxImage;

    ImageList_SetBkColor(himl,ilHead.bkcolor);
    for (i=0;i<4;i++)
    	ImageList_SetOverlayImage(himl,ilHead.ovls[i],i+1);
    return himl;
}


/*************************************************************************
 * ImageList_Remove [COMCTL32.69] Removes an image from an image list
 *
 * PARAMS
 *     himl [I] image list handle
 *     i    [I] image index
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_Remove (HIMAGELIST himl, INT i)
{
    HBITMAP hbmNewImage, hbmNewMask;
    HDC     hdcSrc, hdcDst;
    INT     cxNew, nCount;

    if (himl == NULL) {
        ERR("Invalid image list handle!\n");
        return FALSE;
    }

    if ((i < -1) || (i >= himl->cCurImage)) {
        ERR("index out of range! %d\n", i);
        return FALSE;
    }

    if (himl->cCurImage == 0) {
        ERR("image list is already empty!\n");
        return FALSE;
    }

    if (i == -1) {
        /* remove all */
        TRACE("remove all!\n");

        himl->cMaxImage = himl->cInitial + himl->cGrow;
        himl->cCurImage = 0;
        for (nCount = 0; nCount < MAX_OVERLAYIMAGE; nCount++)
             himl->nOvlIdx[nCount] = -1;

        DeleteObject (himl->hbmImage);
        himl->hbmImage =
            CreateBitmap (himl->cMaxImage * himl->cx, himl->cy,
                            1, himl->uBitsPixel, NULL);

        if (himl->hbmMask) {
            DeleteObject (himl->hbmMask);
            himl->hbmMask =
                CreateBitmap (himl->cMaxImage * himl->cx, himl->cy,
                                1, 1, NULL);
        }
    }
    else {
        /* delete one image */
        TRACE("Remove single image! %d\n", i);

        /* create new bitmap(s) */
        cxNew = (himl->cCurImage + himl->cGrow - 1) * himl->cx;

        TRACE(" - Number of images: %d / %d (Old/New)\n",
                 himl->cCurImage, himl->cCurImage - 1);
        TRACE(" - Max. number of images: %d / %d (Old/New)\n",
                 himl->cMaxImage, himl->cCurImage + himl->cGrow - 1);

        hbmNewImage =
            CreateBitmap (cxNew, himl->cy, 1, himl->uBitsPixel, NULL);

        if (himl->hbmMask)
            hbmNewMask = CreateBitmap (cxNew, himl->cy, 1, 1, NULL);
        else
            hbmNewMask = 0;  /* Just to keep compiler happy! */

        hdcSrc = CreateCompatibleDC (0);
        hdcDst = CreateCompatibleDC (0);

        /* copy all images and masks prior to the "removed" image */
        if (i > 0) {
            TRACE("Pre image copy: Copy %d images\n", i);

            SelectObject (hdcSrc, himl->hbmImage);
            SelectObject (hdcDst, hbmNewImage);
            BitBlt (hdcDst, 0, 0, i * himl->cx, himl->cy,
                      hdcSrc, 0, 0, SRCCOPY);

            if (himl->hbmMask) {
                SelectObject (hdcSrc, himl->hbmMask);
                SelectObject (hdcDst, hbmNewMask);
                BitBlt (hdcDst, 0, 0, i * himl->cx, himl->cy,
                          hdcSrc, 0, 0, SRCCOPY);
            }
        }

        /* copy all images and masks behind the removed image */
        if (i < himl->cCurImage - 1) {
            TRACE("Post image copy!\n");
            SelectObject (hdcSrc, himl->hbmImage);
            SelectObject (hdcDst, hbmNewImage);
            BitBlt (hdcDst, i * himl->cx, 0, (himl->cCurImage - i - 1) * himl->cx,
                      himl->cy, hdcSrc, (i + 1) * himl->cx, 0, SRCCOPY);

            if (himl->hbmMask) {
                SelectObject (hdcSrc, himl->hbmMask);
                SelectObject (hdcDst, hbmNewMask);
                BitBlt (hdcDst, i * himl->cx, 0,
                          (himl->cCurImage - i - 1) * himl->cx,
                          himl->cy, hdcSrc, (i + 1) * himl->cx, 0, SRCCOPY);
            }
        }

        DeleteDC (hdcSrc);
        DeleteDC (hdcDst);

        /* delete old images and insert new ones */
        DeleteObject (himl->hbmImage);
        himl->hbmImage = hbmNewImage;
        if (himl->hbmMask) {
            DeleteObject (himl->hbmMask);
            himl->hbmMask = hbmNewMask;
        }

        himl->cCurImage--;
        himl->cMaxImage = himl->cCurImage + himl->cGrow;
    }

    return TRUE;
}


/*************************************************************************
 * ImageList_Replace [COMCTL32.70]
 *
 * Replaces an image in an image list with a new image.
 *
 * PARAMS
 *     himl     [I] handle to image list
 *     i        [I] image index
 *     hbmImage [I] handle to image bitmap
 *     hbmMask  [I] handle to mask bitmap. Can be NULL.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_Replace (HIMAGELIST himl, INT i, HBITMAP hbmImage,
		   HBITMAP hbmMask)
{
    HDC hdcImageList, hdcImage;
    BITMAP bmp;

    if (himl == NULL) {
        ERR("Invalid image list handle!\n");
        return FALSE;
    }

    if ((i >= himl->cMaxImage) || (i < 0)) {
        ERR("Invalid image index!\n");
        return FALSE;
    }

    hdcImageList = CreateCompatibleDC (0);
    hdcImage = CreateCompatibleDC (0);
    GetObjectA (hbmImage, sizeof(BITMAP), (LPVOID)&bmp);

    /* Replace Image */
    SelectObject (hdcImageList, himl->hbmImage);
    SelectObject (hdcImage, hbmImage);

    StretchBlt (hdcImageList, i * himl->cx, 0, himl->cx, himl->cy,
                  hdcImage, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);

    if (himl->hbmMask)
    {
        /* Replace Mask */
        SelectObject (hdcImageList, himl->hbmMask);
        SelectObject (hdcImage, hbmMask);

        StretchBlt (hdcImageList, i * himl->cx, 0, himl->cx, himl->cy,
                      hdcImage, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);


        /* Remove the background from the image
        */
        SelectObject (hdcImageList, himl->hbmImage);
        StretchBlt (hdcImageList,
            i*himl->cx, 0, himl->cx, himl->cy,
            hdcImage,
            0, 0, bmp.bmWidth, bmp.bmHeight,
            0x220326); /* NOTSRCAND */
    }

    DeleteDC (hdcImage);
    DeleteDC (hdcImageList);

    return TRUE;
}


/*************************************************************************
 * ImageList_ReplaceIcon [COMCTL32.75]
 *
 * Replaces an image in an image list using an icon.
 *
 * PARAMS
 *     himl  [I] handle to image list
 *     i     [I] image index
 *     hIcon [I] handle to icon
 *
 * RETURNS
 *     Success: index of the replaced image
 *     Failure: -1
 */

INT WINAPI
ImageList_ReplaceIcon (HIMAGELIST himl, INT i, HICON hIcon)
{
    HDC     hdcImageList, hdcImage;
    INT     nIndex;
    HICON   hBestFitIcon;
    HBITMAP hbmOldSrc, hbmOldDst;
    ICONINFO  ii;
    BITMAP  bmp;

    TRACE("(0x%lx 0x%x 0x%x)\n", (DWORD)himl, i, hIcon);

    if (himl == NULL)
	return -1;
    if ((i >= himl->cMaxImage) || (i < -1))
	return -1;

    hBestFitIcon = CopyImage(
        hIcon, IMAGE_ICON,
        himl->cx, himl->cy,
        LR_COPYFROMRESOURCE);

    GetIconInfo (hBestFitIcon, &ii);
    if (ii.hbmMask == 0)
	ERR("no mask!\n");
    if (ii.hbmColor == 0)
	ERR("no color!\n");
    GetObjectA (ii.hbmMask, sizeof(BITMAP), (LPVOID)&bmp);

    if (i == -1) {
        if (himl->cCurImage + 1 > himl->cMaxImage)
            IMAGELIST_InternalExpandBitmaps (himl, 1, 0, 0);

        nIndex = himl->cCurImage;
        himl->cCurImage++;
    }
    else
        nIndex = i;

    hdcImageList = CreateCompatibleDC (0);
    TRACE("hdcImageList=0x%x!\n", hdcImageList);
    if (hdcImageList == 0)
	ERR("invalid hdcImageList!\n");

    hdcImage = CreateCompatibleDC (0);
    TRACE("hdcImage=0x%x!\n", hdcImage);
    if (hdcImage == 0)
	ERR("invalid hdcImage!\n");

    hbmOldDst = SelectObject (hdcImageList, himl->hbmImage);
    SetTextColor( hdcImageList, RGB(0,0,0));
    SetBkColor( hdcImageList, RGB(255,255,255));
    hbmOldSrc = SelectObject (hdcImage, ii.hbmColor);
    StretchBlt (hdcImageList, nIndex * himl->cx, 0, himl->cx, himl->cy,
                  hdcImage, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);

    if (himl->hbmMask) {
        SelectObject (hdcImageList, himl->hbmMask);
        SelectObject (hdcImage, ii.hbmMask);
        StretchBlt (hdcImageList, nIndex * himl->cx, 0, himl->cx, himl->cy,
                      hdcImage, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);
    }

    SelectObject (hdcImage, hbmOldSrc);
    SelectObject (hdcImageList, hbmOldDst);

    if(hBestFitIcon)
	DestroyIcon(hBestFitIcon);
    if (hdcImageList)
	DeleteDC (hdcImageList);
    if (hdcImage)
	DeleteDC (hdcImage);
    if (ii.hbmColor)
	DeleteObject (ii.hbmColor);
    if (ii.hbmMask)
	DeleteObject (ii.hbmMask);

    return nIndex;
}


/*************************************************************************
 * ImageList_SetBkColor [COMCTL32.76]
 *
 * Sets the background color of an image list.
 *
 * PARAMS
 *     himl  [I] handle to image list
 *     clrBk [I] background color
 *
 * RETURNS
 *     Success: previous background color
 *     Failure: CLR_NONE
 */

COLORREF WINAPI
ImageList_SetBkColor (HIMAGELIST himl, COLORREF clrBk)
{
    COLORREF clrOldBk;

    if (himl == NULL)
	return CLR_NONE;

    clrOldBk = himl->clrBk;
    himl->clrBk = clrBk;
    return clrOldBk;
}


/*************************************************************************
 * ImageList_SetDragCursorImage [COMCTL32.77]
 *
 * Combines the specified image with the current drag image
 *
 * PARAMS
 *     himlDrag  [I] handle to drag image list
 *     iDrag     [I] drag image index
 *     dxHotspot [I] X position of the hot spot
 *     dyHotspot [I] Y position of the hot spot
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * NOTES
 *     When this function is called and the drag image is visible, a
 *     short flickering occurs but this matches the Win9x behavior. It is
 *     possible to fix the flickering using code like in ImageList_DragMove.
 */

BOOL WINAPI
ImageList_SetDragCursorImage (HIMAGELIST himlDrag, INT iDrag,
			      INT dxHotspot, INT dyHotspot)
{
    HIMAGELIST himlTemp;
    INT dx, dy;
    BOOL visible;

    if (InternalDrag.himl == NULL)
	return FALSE;

    TRACE(" dxH=%d dyH=%d nX=%d nY=%d\n",
	   dxHotspot, dyHotspot, InternalDrag.dxHotspot, InternalDrag.dyHotspot);

    visible = InternalDrag.bShow;

    /* Calculate the offset between the origin of the old image and the
     * origin of the second image.
     * dxHotspot, dyHotspot is the offset of THE Hotspot (there is only one
     * hotspot) to the origin of the second image.
     * See M$DN for details */
    if(InternalDrag.bHSPending) {
	dx = 0;
	dy = 0;
	InternalDrag.bHSPending = FALSE;
    } else {
	dx = InternalDrag.dxHotspot - dxHotspot;
	dy = InternalDrag.dyHotspot - dyHotspot;
    }
    himlTemp = ImageList_Merge (InternalDrag.himl, 0, himlDrag, iDrag, dx, dy);

    if (visible) {
	/* hide the drag image */
	ImageList_DragShowNolock(FALSE);
    }
    if ((InternalDrag.himl->cx != himlTemp->cx) ||
	   (InternalDrag.himl->cy != himlTemp->cy)) {
	/* the size of the drag image changed, invalidate the buffer */
	DeleteObject(InternalDrag.hbmBg);
	InternalDrag.hbmBg = 0;
    }

    ImageList_Destroy (InternalDrag.himl);
    InternalDrag.himl = himlTemp;

    /* update the InternalDragOffset, if the origin of the
     * DragImage was changed by ImageList_Merge. */
    if (dx <= 0)
	InternalDrag.dxHotspot = dxHotspot;
    if (dy <= 0)
	InternalDrag.dyHotspot = dyHotspot;

    if (visible) {
	/* show the drag image */
	ImageList_DragShowNolock(TRUE);
    }

    return TRUE;
}


/*************************************************************************
 * ImageList_SetFilter [COMCTL32.78]
 *
 * Sets a filter (or does something completely different)!!???
 *
 * PARAMS
 *     himl     [I] handle to image list
 *     i        [I] ???
 *     dwFilter [I] ???
 *
 * RETURNS
 *     Success: TRUE ???
 *     Failure: FALSE ???
 *
 * BUGS
 *     This is an UNDOCUMENTED function!!!!
 *     empty stub.
 */

BOOL WINAPI
ImageList_SetFilter (HIMAGELIST himl, INT i, DWORD dwFilter)
{
    FIXME("(%p 0x%x 0x%lx):empty stub!\n",
	   himl, i, dwFilter);

    return FALSE;
}


/*************************************************************************
 * ImageList_SetFlags [COMCTL32.79]
 *
 * BUGS
 *    Stub.
 */

DWORD WINAPI
ImageList_SetFlags(HIMAGELIST himl, DWORD flags)
{
    FIXME("(%p %08lx):empty stub\n", himl, flags);
    return 0;
}


/*************************************************************************
 * ImageList_SetIconSize [COMCTL32.80]
 *
 * Sets the image size of the bitmap and deletes all images.
 *
 * PARAMS
 *     himl [I] handle to image list
 *     cx   [I] image width
 *     cy   [I] image height
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_SetIconSize (HIMAGELIST himl, INT cx, INT cy)
{
    INT nCount;

    if (!himl)
	return FALSE;

    /* remove all images */
    himl->cMaxImage = himl->cInitial + himl->cGrow;
    himl->cCurImage = 0;
    himl->cx        = cx;
    himl->cy        = cy;

    /* initialize overlay mask indices */
    for (nCount = 0; nCount < MAX_OVERLAYIMAGE; nCount++)
        himl->nOvlIdx[nCount] = -1;

    DeleteObject (himl->hbmImage);
    himl->hbmImage =
        CreateBitmap (himl->cMaxImage * himl->cx, himl->cy,
                        1, himl->uBitsPixel, NULL);

    if (himl->hbmMask) {
        DeleteObject (himl->hbmMask);
        himl->hbmMask =
            CreateBitmap (himl->cMaxImage * himl->cx, himl->cy,
                            1, 1, NULL);
    }

    return TRUE;
}


/*************************************************************************
 * ImageList_SetImageCount [COMCTL32.81]
 *
 * Resizes an image list to the specified number of images.
 *
 * PARAMS
 *     himl        [I] handle to image list
 *     iImageCount [I] number of images in the image list
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_SetImageCount (HIMAGELIST himl, INT iImageCount)
{
    HDC     hdcImageList, hdcBitmap;
    HBITMAP hbmNewBitmap;
    INT     nNewCount, nCopyCount;

    if (!himl)
	return FALSE;
    if (himl->cCurImage >= iImageCount)
	return FALSE;
    if (himl->cMaxImage > iImageCount)
	return TRUE;

    nNewCount = iImageCount + himl->cGrow;
    nCopyCount = min(himl->cCurImage, iImageCount);

    hdcImageList = CreateCompatibleDC (0);
    hdcBitmap = CreateCompatibleDC (0);

    hbmNewBitmap = CreateBitmap (nNewCount * himl->cx, himl->cy,
                                   1, himl->uBitsPixel, NULL);
    if (hbmNewBitmap != 0)
    {
        SelectObject (hdcImageList, himl->hbmImage);
        SelectObject (hdcBitmap, hbmNewBitmap);

	/* copy images */
        BitBlt (hdcBitmap, 0, 0, nCopyCount * himl->cx, himl->cy,
                  hdcImageList, 0, 0, SRCCOPY);
#if 0
	/* delete 'empty' image space */
	SetBkColor (hdcBitmap, RGB(255, 255, 255));
	SetTextColor (hdcBitmap, RGB(0, 0, 0));
	PatBlt (hdcBitmap,  nCopyCount * himl->cx, 0,
		  (nNewCount - nCopyCount) * himl->cx, himl->cy, BLACKNESS);
#endif
	DeleteObject (himl->hbmImage);
	himl->hbmImage = hbmNewBitmap;
    }
    else
	ERR("Could not create new image bitmap !\n");

    if (himl->hbmMask)
    {
        hbmNewBitmap = CreateBitmap (nNewCount * himl->cx, himl->cy,
                                       1, 1, NULL);
        if (hbmNewBitmap != 0)
        {
            SelectObject (hdcImageList, himl->hbmMask);
            SelectObject (hdcBitmap, hbmNewBitmap);

	    /* copy images */
            BitBlt (hdcBitmap, 0, 0, nCopyCount * himl->cx, himl->cy,
                      hdcImageList, 0, 0, SRCCOPY);
#if 0
	    /* delete 'empty' image space */
	    SetBkColor (hdcBitmap, RGB(255, 255, 255));
	    SetTextColor (hdcBitmap, RGB(0, 0, 0));
            PatBlt (hdcBitmap,  nCopyCount * himl->cx, 0,
		      (nNewCount - nCopyCount) * himl->cx, himl->cy, BLACKNESS);
#endif
            DeleteObject (himl->hbmMask);
            himl->hbmMask = hbmNewBitmap;
        }
        else
            ERR("Could not create new mask bitmap!\n");
    }

    DeleteDC (hdcImageList);
    DeleteDC (hdcBitmap);

    /* Update max image count and current image count */
    himl->cMaxImage = nNewCount;
    if (himl->cCurImage > nCopyCount)
        himl->cCurImage = nCopyCount;

    return TRUE;
}


/*************************************************************************
 * ImageList_SetOverlayImage [COMCTL32.82]
 *
 * Assigns an overlay mask index to an existing image in an image list.
 *
 * PARAMS
 *     himl     [I] handle to image list
 *     iImage   [I] image index
 *     iOverlay [I] overlay mask index
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
ImageList_SetOverlayImage (HIMAGELIST himl, INT iImage, INT iOverlay)
{
    if (!himl)
	return FALSE;
    if ((iOverlay < 1) || (iOverlay > MAX_OVERLAYIMAGE))
	return FALSE;
    if ((iImage!=-1) && ((iImage < 0) || (iImage > himl->cCurImage)))
	return FALSE;
    himl->nOvlIdx[iOverlay - 1] = iImage;
    return TRUE;
}



/* helper for ImageList_Write - write bitmap to pstm
 * currently everything is written as 24 bit RGB, except masks
 */
static BOOL
_write_bitmap(HBITMAP hBitmap, LPSTREAM pstm, int cx, int cy)
{
    LPBITMAPFILEHEADER bmfh;
    LPBITMAPINFOHEADER bmih;
    LPBYTE data, lpBits, lpBitsOrg;
    BITMAP bm;
    INT bitCount, sizeImage, offBits, totalSize;
    INT nwidth, nheight, nsizeImage, icount;
    HDC xdc;
    BOOL result = FALSE;


    xdc = GetDC(0);
    GetObjectA(hBitmap, sizeof(BITMAP), (LPVOID)&bm);

    /* XXX is this always correct? */
    icount = bm.bmWidth / cx;
    nwidth = cx << 2;
    nheight = cy * ((icount+3)>>2);

    bitCount = bm.bmBitsPixel == 1 ? 1 : 24;
    sizeImage = ((((bm.bmWidth * bitCount)+31) & ~31) >> 3) * bm.bmHeight;
    nsizeImage = ((((nwidth * bitCount)+31) & ~31) >> 3) * nheight;

    totalSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    if(bitCount != 24)
	totalSize += (1 << bitCount) * sizeof(RGBQUAD);
    offBits = totalSize;
    totalSize += nsizeImage;

    data = (LPBYTE)LocalAlloc(LMEM_ZEROINIT, totalSize);
    bmfh = (LPBITMAPFILEHEADER)data;
    bmih = (LPBITMAPINFOHEADER)(data + sizeof(BITMAPFILEHEADER));
    lpBits = data + offBits;

    /* setup BITMAPFILEHEADER */
    bmfh->bfType      = (('M' << 8) | 'B');
    bmfh->bfSize      = 0;
    bmfh->bfReserved1 = 0;
    bmfh->bfReserved2 = 0;
    bmfh->bfOffBits   = offBits;

    /* setup BITMAPINFOHEADER */
    bmih->biSize          = sizeof(BITMAPINFOHEADER);
    bmih->biWidth         = bm.bmWidth;
    bmih->biHeight        = bm.bmHeight;
    bmih->biPlanes        = 1;
    bmih->biBitCount      = bitCount;
    bmih->biCompression   = BI_RGB;
    bmih->biSizeImage     = nsizeImage;
    bmih->biXPelsPerMeter = 0;
    bmih->biYPelsPerMeter = 0;
    bmih->biClrUsed       = 0;
    bmih->biClrImportant  = 0;

    lpBitsOrg = (LPBYTE)LocalAlloc(LMEM_ZEROINIT, nsizeImage);
    if(!GetDIBits(xdc, hBitmap, 0, bm.bmHeight, lpBitsOrg,
		  (BITMAPINFO *)bmih, DIB_RGB_COLORS))
	goto failed;
    else {
	int i;
	int obpl = (((bm.bmWidth*bitCount+31) & ~31)>>3);
	int nbpl = (((nwidth*bitCount+31) & ~31)>>3);

	for(i = 0; i < nheight; i++) {
	    int ooff = ((nheight-1-i)%cy) * obpl + ((i/cy) * nbpl);
	    int noff = (nbpl * (nheight-1-i));
	    memcpy(lpBits + noff, lpBitsOrg + ooff, nbpl);
	}
    }

    bmih->biWidth  = nwidth;
    bmih->biHeight = nheight;

    if(bitCount == 1) {
        /* Hack. */
	LPBITMAPINFO inf = (LPBITMAPINFO)bmih;
	inf->bmiColors[0].rgbRed = inf->bmiColors[0].rgbGreen = inf->bmiColors[0].rgbBlue = 0;
	inf->bmiColors[1].rgbRed = inf->bmiColors[1].rgbGreen = inf->bmiColors[1].rgbBlue = 0xff;
    }

    if(!SUCCEEDED(IStream_Write(pstm, data, totalSize, NULL)))
	goto failed;

    result = TRUE;

    failed:
    ReleaseDC(0, xdc);
    LocalFree((HLOCAL)lpBitsOrg);

    return result;
}


/*************************************************************************
 * ImageList_Write [COMCTL32.83]
 *
 * Writes an image list to a stream.
 *
 * PARAMS
 *     himl [I] handle to image list
 *     pstm [O] Pointer to a stream.
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 *
 * BUGS
 *     probably.
 */

BOOL WINAPI
ImageList_Write (HIMAGELIST himl, LPSTREAM pstm)
{
    ILHEAD ilHead;
    int i;

    if (!himl)
	return FALSE;

    ilHead.usMagic   = (('L' << 8) | 'I');
    ilHead.usVersion = 0x101;
    ilHead.cCurImage = himl->cCurImage;
    ilHead.cMaxImage = himl->cMaxImage;
    ilHead.cGrow     = himl->cGrow;
    ilHead.cx        = himl->cx;
    ilHead.cy        = himl->cy;
    ilHead.bkcolor   = himl->clrBk;
    ilHead.flags     = himl->flags;
    for(i = 0; i < 4; i++) {
	ilHead.ovls[i] = himl->nOvlIdx[i];
    }

    if(!SUCCEEDED(IStream_Write(pstm, &ilHead, sizeof(ILHEAD), NULL)))
	return FALSE;

    /* write the bitmap */
    if(!_write_bitmap(himl->hbmImage, pstm, himl->cx, himl->cy))
	return FALSE;

    /* write the mask if we have one */
    if(himl->flags & ILC_MASK) {
	if(!_write_bitmap(himl->hbmMask, pstm, himl->cx, himl->cy))
	    return FALSE;
    }

    return TRUE;
}

