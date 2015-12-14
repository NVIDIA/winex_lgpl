/*
 * Cursor and icon support
 *
 * Copyright 1995 Alexandre Julliard
 *           1996 Martin Von Loewis
 *           1997 Alex Korobka
 *           1998 Turchanov Sergey
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

/*
 * Theory:
 *
 * http://www.microsoft.com/win32dev/ui/icons.htm
 *
 * Cursors and icons are stored in a global heap block, with the
 * following layout:
 *
 * CURSORICONINFO info;
 * BYTE[]         ANDbits;
 * BYTE[]         XORbits;
 *
 * The bits structures are in the format of a device-dependent bitmap.
 *
 * This layout is very sub-optimal, as the bitmap bits are stored in
 * the X client instead of in the server like other bitmaps; however,
 * some programs (notably Paint Brush) expect to be able to manipulate
 * the bits directly :-(
 *
 * FIXME: what are we going to do with animation and color (bpp > 1) cursors ?!
 */

#include <string.h>
#include <stdlib.h>

#include "windef.h"
#include "wingdi.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "wine/exception.h"
#include "palette.h"
#include "bitmap.h"
#include "cursoricon.h"
#include "wine/module.h"
#include "wine/debug.h"
#include "user.h"
#include "input.h"
#include "message.h"
#include "winerror.h"
#include "msvcrt/excpt.h"

WINE_DECLARE_DEBUG_CHANNEL(cursor);
WINE_DECLARE_DEBUG_CHANNEL(icon);
WINE_DECLARE_DEBUG_CHANNEL(resource);

static HCURSOR hActiveCursor = 0;  /* Active cursor */
static INT CURSOR_ShowCount = 0;   /* Cursor display count */
static RECT CURSOR_ClipRect;       /* Cursor clipping rect */

static HDC screen_dc;

/**********************************************************************
 * ICONCACHE for cursors/icons loaded with LR_SHARED.
 *
 * FIXME: This should not be allocated on the system heap, but on a
 *        subsystem-global heap (i.e. one for all Win16 processes,
 *        and one for each Win32 process).
 */
typedef struct tagICONCACHE
{
    struct tagICONCACHE *next;

    HMODULE              hModule;
    HRSRC                hRsrc;
    HRSRC                hGroupRsrc;
    HANDLE               handle;

    INT                  count;

} ICONCACHE;

static ICONCACHE *IconAnchor = NULL;
static CRITICAL_SECTION IconCrst;
static WORD ICON_HOTSPOT = 0x4242;

void create_icon_cs(void)
{
  CRITICAL_SECTION_DEFINE( &IconCrst );
}

void destroy_icon_cs(void)
{
  DeleteCriticalSection( &IconCrst );
}

/***********************************************************************
 *             map_fileW
 *
 * Helper function to map a file to memory:
 *  name			-	file name
 *  [RETURN] ptr		-	pointer to mapped file
 */
static void *map_fileW( LPCWSTR name )
{
    HANDLE hFile, hMapping;
    LPVOID ptr = NULL;

    hFile = CreateFileW( name, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0 );
    if (hFile != INVALID_HANDLE_VALUE)
    {
        hMapping = CreateFileMappingA( hFile, NULL, PAGE_READONLY, 0, 0, NULL );
        CloseHandle( hFile );
        if (hMapping)
        {
            ptr = MapViewOfFile( hMapping, FILE_MAP_READ, 0, 0, 0 );
            CloseHandle( hMapping );
        }
    }
    return ptr;
}


/**********************************************************************
 *	    CURSORICON_FindSharedIcon
 */
static HANDLE CURSORICON_FindSharedIcon( HMODULE hModule, HRSRC hRsrc )
{
    HANDLE handle = 0;
    ICONCACHE *ptr;

    EnterCriticalSection( &IconCrst );

    for ( ptr = IconAnchor; ptr; ptr = ptr->next )
        if ( ptr->hModule == hModule && ptr->hRsrc == hRsrc )
        {
            ptr->count++;
            handle = ptr->handle;
            break;
        }

    LeaveCriticalSection( &IconCrst );

    return handle;
}

/*************************************************************************
 * CURSORICON_FindCache
 *
 * Given a handle, find the corresponding cache element
 *
 * PARAMS
 *      Handle     [I] handle to an Image
 *
 * RETURNS
 *     Success: The cache entry
 *     Failure: NULL
 *
 */
static ICONCACHE* CURSORICON_FindCache(HANDLE handle)
{
    ICONCACHE *ptr;
    ICONCACHE *pRet=NULL;
    BOOL IsFound = FALSE;
    int count;

    EnterCriticalSection( &IconCrst );

    for (count = 0, ptr = IconAnchor; ptr != NULL && !IsFound; ptr = ptr->next, count++ )
    {
        if ( handle == ptr->handle )
        {
            IsFound = TRUE;
            pRet = ptr;
        }
    }

    LeaveCriticalSection( &IconCrst );

    return pRet;
}

/**********************************************************************
 *	    CURSORICON_AddSharedIcon
 */
static void CURSORICON_AddSharedIcon( HMODULE hModule, HRSRC hRsrc, HRSRC hGroupRsrc, HANDLE handle )
{
    ICONCACHE *ptr = HeapAlloc( GetProcessHeap(), 0, sizeof(ICONCACHE) );
    if ( !ptr ) return;

    ptr->hModule = hModule;
    ptr->hRsrc   = hRsrc;
    ptr->handle  = handle;
    ptr->hGroupRsrc = hGroupRsrc;
    ptr->count   = 1;

    EnterCriticalSection( &IconCrst );
    ptr->next    = IconAnchor;
    IconAnchor   = ptr;
    LeaveCriticalSection( &IconCrst );
}

/**********************************************************************
 *	    CURSORICON_DelSharedIcon
 */
static INT CURSORICON_DelSharedIcon( HANDLE handle )
{
    INT count = -1;
    ICONCACHE *ptr;

    EnterCriticalSection( &IconCrst );

    for ( ptr = IconAnchor; ptr; ptr = ptr->next )
        if ( ptr->handle == handle )
        {
            if ( ptr->count > 0 ) ptr->count--;
            count = ptr->count;
            break;
        }

    LeaveCriticalSection( &IconCrst );

    return count;
}

/**********************************************************************
 *	    CURSORICON_FreeModuleIcons
 */
void CURSORICON_FreeModuleIcons( HMODULE hModule )
{
    ICONCACHE **ptr = &IconAnchor;

    if ( HIWORD( hModule ) )
        hModule = MapHModuleLS( hModule );
    else
        hModule = GetExePtr( hModule );

    EnterCriticalSection( &IconCrst );

    while ( *ptr )
    {
        if ( (*ptr)->hModule == hModule )
        {
            ICONCACHE *freePtr = *ptr;
            *ptr = freePtr->next;

            GlobalFree16( freePtr->handle );
            HeapFree( GetProcessHeap(), 0, freePtr );
            continue;
        }
        ptr = &(*ptr)->next;
    }

    LeaveCriticalSection( &IconCrst );
}

/**********************************************************************
 *	    CURSORICON_FindBestIcon
 *
 * Find the icon closest to the requested size and number of colors.
 */
static CURSORICONDIRENTRY *CURSORICON_FindBestIcon( CURSORICONDIR *dir, int width,
                                              int height, int colors )
{
    int i;
    CURSORICONDIRENTRY *entry, *bestEntry = NULL;
    UINT iTotalDiff, iXDiff=0, iYDiff=0, iColorDiff;
    UINT iTempXDiff, iTempYDiff, iTempColorDiff;

    if (dir->idCount < 1)
    {
        WARN_(icon)("Empty directory!\n" );
        return NULL;
    }
    if (dir->idCount == 1) return &dir->idEntries[0];  /* No choice... */

    /* Find Best Fit */
    iTotalDiff = 0xFFFFFFFF;
    iColorDiff = 0xFFFFFFFF;
    for (i = 0, entry = &dir->idEntries[0]; i < dir->idCount; i++,entry++)
        {
	iTempXDiff = abs(width - entry->ResInfo.icon.bWidth);
	iTempYDiff = abs(height - entry->ResInfo.icon.bHeight);

        if(iTotalDiff > (iTempXDiff + iTempYDiff))
        {
            iXDiff = iTempXDiff;
            iYDiff = iTempYDiff;
	    iTotalDiff = iXDiff + iYDiff;
        }
        }

    /* Find Best Colors for Best Fit */
    for (i = 0, entry = &dir->idEntries[0]; i < dir->idCount; i++,entry++)
        {
        if(abs(width - entry->ResInfo.icon.bWidth) == iXDiff &&
            abs(height - entry->ResInfo.icon.bHeight) == iYDiff)
        {
            iTempColorDiff = abs(colors - entry->ResInfo.icon.bColorCount);
            if(iColorDiff > iTempColorDiff)
        {
            bestEntry = entry;
                iColorDiff = iTempColorDiff;
        }
        }
    }

    return bestEntry;
}


/**********************************************************************
 *	    CURSORICON_FindBestCursor
 *
 * Find the cursor closest to the requested size.
 * FIXME: parameter 'color' ignored and entries with more than 1 bpp
 *        ignored too
 */
static CURSORICONDIRENTRY *CURSORICON_FindBestCursor( CURSORICONDIR *dir,
                                                  int width, int height, int color)
{
    int i, maxwidth, maxheight;
    CURSORICONDIRENTRY *entry, *bestEntry = NULL;

    if (dir->idCount < 1)
    {
        WARN_(cursor)("Empty directory!\n" );
        return NULL;
    }
    if (dir->idCount == 1) return &dir->idEntries[0]; /* No choice... */

    /* Double height to account for AND and XOR masks */

    height *= 2;

    /* First find the largest one smaller than or equal to the requested size*/

    maxwidth = maxheight = 0;
    for(i = 0,entry = &dir->idEntries[0]; i < dir->idCount; i++,entry++)
        if ((entry->ResInfo.cursor.wWidth <= width) && (entry->ResInfo.cursor.wHeight <= height) &&
            (entry->ResInfo.cursor.wWidth > maxwidth) && (entry->ResInfo.cursor.wHeight > maxheight) &&
	    (entry->wBitCount == 1))
        {
            bestEntry = entry;
            maxwidth  = entry->ResInfo.cursor.wWidth;
            maxheight = entry->ResInfo.cursor.wHeight;
        }
    if (bestEntry) return bestEntry;

    /* Now find the smallest one larger than the requested size */

    maxwidth = maxheight = 255;
    for(i = 0,entry = &dir->idEntries[0]; i < dir->idCount; i++,entry++)
        if ((entry->ResInfo.cursor.wWidth < maxwidth) && (entry->ResInfo.cursor.wHeight < maxheight) &&
	    (entry->wBitCount == 1))
        {
            bestEntry = entry;
            maxwidth  = entry->ResInfo.cursor.wWidth;
            maxheight = entry->ResInfo.cursor.wHeight;
        }

    return bestEntry;
}

/*********************************************************************
 * The main purpose of this function is to create fake resource directory
 * and fake resource entries. There are several reasons for this:
 *	-	CURSORICONDIR and CURSORICONFILEDIR differ in sizes and their
 *              fields
 *	There are some "bad" cursor files which do not have
 *		bColorCount initialized but instead one must read this info
 *		directly from corresponding DIB sections
 * Note: wResId is index to array of pointer returned in ptrs (origin is 1)
 */
BOOL CURSORICON_SimulateLoadingFromResourceW( LPWSTR filename, BOOL fCursor,
                                                CURSORICONDIR **res, LPBYTE **ptr)
{
    LPBYTE   _free;
    CURSORICONFILEDIR *bits;
    CURSORICONFILEDIR *originalBits;
    int	     entries, size, i;

    *res = NULL;
    *ptr = NULL;

    originalBits = bits = map_fileW( filename );
    if (bits == NULL) return FALSE;

    /* FIXME: test for inimated icons
     * hack to load the first icon from the *.ani file
     */
    if ( *(LPDWORD)bits==0x46464952 ){ /* "RIFF" */
        LPBYTE pos = (LPBYTE) bits;
        FIXME_(cursor)("Animated icons not correctly implemented! %p \n", bits);

        for (;;){ 
            if (*(LPDWORD)pos==0x6e6f6369){		/* "icon" */
                FIXME_(cursor)("icon entry found! %p\n", bits);
                pos+=4;
                if ( !*(LPWORD) pos==0x2fe){		/* iconsize */
                    goto fail;
                }
                bits=(CURSORICONFILEDIR*)(pos+4);
                FIXME_(cursor)("icon size ok. offset=%p \n", bits);
                break;
            }
            pos+=2;
            if (pos>=(LPBYTE)bits+766) goto fail;
        }
    }

    if (!(entries = bits->idCount)) goto fail;
    size = sizeof(CURSORICONDIR) + sizeof(CURSORICONDIRENTRY) * (entries - 1);
    _free = (LPBYTE) size;

    for (i=0; i < entries; i++)
      size += bits->idEntries[i].dwDIBSize + (fCursor ? sizeof(POINT16): 0);

    if (!(*ptr = HeapAlloc( GetProcessHeap(), 0,
                            entries * sizeof (CURSORICONDIRENTRY*)))) goto fail;
    if (!(*res = HeapAlloc( GetProcessHeap(), 0, size))) goto fail;

    _free = (LPBYTE)(*res) + (int)_free;
    memcpy((*res), bits, 6);
    for (i=0; i<entries; i++)
    {
      ((LPBYTE*)(*ptr))[i] = _free;
      if (fCursor) {
        (*res)->idEntries[i].ResInfo.cursor.wWidth=bits->idEntries[i].bWidth;
        (*res)->idEntries[i].ResInfo.cursor.wHeight=bits->idEntries[i].bHeight;
	((LPPOINT16)_free)->x=bits->idEntries[i].xHotspot;
	((LPPOINT16)_free)->y=bits->idEntries[i].yHotspot;
	_free+=sizeof(POINT16);
      } else {
        (*res)->idEntries[i].ResInfo.icon.bWidth=bits->idEntries[i].bWidth;
        (*res)->idEntries[i].ResInfo.icon.bHeight=bits->idEntries[i].bHeight;
        (*res)->idEntries[i].ResInfo.icon.bColorCount = bits->idEntries[i].bColorCount;
      }
      (*res)->idEntries[i].wPlanes=1;
      (*res)->idEntries[i].wBitCount = ((LPBITMAPINFOHEADER)((LPBYTE)bits +
                                                   bits->idEntries[i].dwDIBOffset))->biBitCount;
      (*res)->idEntries[i].dwBytesInRes = bits->idEntries[i].dwDIBSize;
      (*res)->idEntries[i].wResId=i+1;

      memcpy(_free,(LPBYTE)bits +bits->idEntries[i].dwDIBOffset,
             (*res)->idEntries[i].dwBytesInRes);
      _free += (*res)->idEntries[i].dwBytesInRes;
    }
    UnmapViewOfFile( originalBits );
    return TRUE;
fail:
    if (*res) HeapFree( GetProcessHeap(), 0, *res );
    if (*ptr) HeapFree( GetProcessHeap(), 0, *ptr );
    UnmapViewOfFile( originalBits );
    return FALSE;
}


static void CURSORICON_FillAlphaChannel( CURSORICONINFO *info )
{
    int mask_size;
    char *mask;
    char *image;
    char *msrc;
    int x,y;
    DWORD color;
    DWORD *dst;
    BYTE mbit;

    if (info->bBitsPerPixel != 32) return;

    mask_size = (info->nWidth/8 * info->nHeight);
    mask = (char *)(info+1);
    image = mask + mask_size;

    msrc = mask;
    dst = (DWORD *)image;
    mbit = 0x80;
    for (y=0; y<info->nHeight; y++) {
      for (x=0; x<info->nWidth; x++) {
        color = (*msrc & mbit) ? 0x00000000 : 0xff000000;
        color |= *dst & 0x00ffffff;
        *dst++ = color;
        mbit >>= 1;
        if (!mbit) {
          msrc++;
          mbit = 0x80;
        }
      }
      /* if each line of the mask should be aligned, do it here.
       * However, it doesn't seem anywhere else takes care of that
       * anyway, so I haven't bothered to do it here either. */
    }
}

static void CURSORICON_FillAlphaMask( CURSORICONINFO *info )
{
    int mask_size;
    char *mask;
    char *image;
    char *mdst;
    int x,y;
    DWORD color;
    DWORD *src;
    BYTE mbit;

    if (info->bBitsPerPixel != 32) return;

    mask_size = (info->nWidth/8 * info->nHeight);
    mask = (char *)(info+1);
    image = mask + mask_size;

    mdst = mask;
    src = (DWORD *)image;
    mbit = 0x80;
    for (y=0; y<info->nHeight; y++) {
      for (x=0; x<info->nWidth; x++) {
        color = *src++;
        if ((color >> 24) >= 0x80)
          *mdst &= ~mbit;
        else
          *mdst |= mbit;
        mbit >>= 1;
        if (!mbit) {
          mdst++;
          mbit = 0x80;
        }
      }
      /* if each line of the mask should be aligned, do it here.
       * However, it doesn't seem anywhere else takes care of that
       * anyway, so I haven't bothered to do it here either. */
    }
}

static int get_dib_width_bytes( int width, int depth )
{
    int words;

    switch(depth)
    {
	case 1:  words = (width + 31) / 32; break;
	case 4:  words = (width + 7) / 8; break;
	case 8:  words = (width + 3) / 4; break;
	case 15:
	case 16: words = (width + 1) / 2; break;
	case 24: words = (width * 3 + 3)/4; break;

	default:
            WARN_(cursor)("(%d): Unsupported depth\n", depth );
	/* fall through */
	case 32:
	        words = width;
    }
    return 4 * words;
}

/**********************************************************************
 *	    CURSORICON_CreateFromResource
 *
 * Create a cursor or icon from in-memory resource template.
 *
 * FIXME: Convert to mono when cFlag is LR_MONOCHROME. Do something
 *        with cbSize parameter as well.
 */
static HGLOBAL16 CURSORICON_CreateFromResource( HINSTANCE16 hInstance, HGLOBAL16 hObj, LPBYTE bits,
	 					UINT cbSize, BOOL bIcon, DWORD dwVersion,
						INT width, INT height, UINT loadflags )
{
    static HDC hdcMem;
    int sizeAnd, sizeXor, sizePal;
    HBITMAP hAndBits = 0, hXorBits = 0; /* error condition for later */
    BITMAP bmpXor, bmpAnd;
    POINT16 hotspot;
    BITMAPINFO *bmi;
    BOOL DoStretch;
    INT size;

    hotspot.x = ICON_HOTSPOT;
    hotspot.y = ICON_HOTSPOT;

    TRACE_(cursor)("%08x (%u bytes), ver %08x, %ix%i %s %s\n",
                        (unsigned)bits, cbSize, (unsigned)dwVersion, width, height,
                                  bIcon ? "icon" : "cursor", (loadflags & LR_MONOCHROME) ? "mono" : "" );
    if (dwVersion == 0x00020000)
    {
	FIXME_(cursor)("\t2.xx resources are not supported\n");
	return 0;
    }

    if (bIcon)
	bmi = (BITMAPINFO *)bits;
    else /* get the hotspot */
    {
        POINT16 *pt = (POINT16 *)bits;
        hotspot = *pt;
        bmi = (BITMAPINFO *)(pt + 1);
    }
    size = DIB_BitmapInfoSize( bmi, DIB_RGB_COLORS );

    if (!width) width = bmi->bmiHeader.biWidth;
    if (!height) height = bmi->bmiHeader.biHeight/2;
    DoStretch = (bmi->bmiHeader.biHeight/2 != height) ||
      (bmi->bmiHeader.biWidth != width);

    /* Check bitmap header */

    if ( (bmi->bmiHeader.biSize != sizeof(BITMAPCOREHEADER)) &&
	 (bmi->bmiHeader.biSize != sizeof(BITMAPINFOHEADER)  ||
	  bmi->bmiHeader.biCompression != BI_RGB) )
    {
          WARN_(cursor)("\tinvalid resource bitmap header.\n");
          return 0;
    }

    if (!screen_dc || GetObjectType(screen_dc) != OBJ_DC) screen_dc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
    if (screen_dc)
    {
	BITMAPINFO* pInfo;

        /* Make sure we have room for the monochrome bitmap later on.
	 * Note that BITMAPINFOINFO and BITMAPCOREHEADER are the same
	 * up to and including the biBitCount. In-memory icon resource
	 * format is as follows:
	 *
	 *   BITMAPINFOHEADER   icHeader  // DIB header
	 *   RGBQUAD         icColors[]   // Color table
  	 *   BYTE            icXOR[]      // DIB bits for XOR mask
	 *   BYTE            icAND[]      // DIB bits for AND mask
	 */

    	if ((pInfo = (BITMAPINFO *)HeapAlloc( GetProcessHeap(), 0,
	  max(size, sizeof(BITMAPINFOHEADER) + 2*sizeof(RGBQUAD)))))
	{
	    memcpy( pInfo, bmi, size );
	    pInfo->bmiHeader.biHeight /= 2;

	    /* Create the XOR bitmap */

	    if (DoStretch) {
                if((pInfo->bmiHeader.biBitCount * pInfo->bmiHeader.biPlanes) > 1)
                {
                    hXorBits = CreateCompatibleBitmap(screen_dc, width, height);
                }
                else
                {
                    hXorBits = CreateBitmap(width, height, 1, 1, NULL);
                }
                if(hXorBits)
                {
		HBITMAP hOld;
                BOOL res = FALSE;

                if (!hdcMem) hdcMem = CreateCompatibleDC(screen_dc);
                if (hdcMem) {
                    hOld = SelectObject(hdcMem, hXorBits);
                    res = StretchDIBits(hdcMem, 0, 0, width, height, 0, 0,
                                        bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight/2,
                                        (char*)bmi + size, pInfo, DIB_RGB_COLORS, SRCCOPY);
                    SelectObject(hdcMem, hOld);
                }
		if (!res) { DeleteObject(hXorBits); hXorBits = 0; }
	      }
	    } else hXorBits = CreateDIBitmap( screen_dc, &pInfo->bmiHeader,
		CBM_INIT, (char*)bmi + size, pInfo, DIB_RGB_COLORS );
	    if( hXorBits )
	    {
		char* xbits = (char *)bmi + size +
			(get_dib_width_bytes(bmi->bmiHeader.biWidth,
					     bmi->bmiHeader.biBitCount) *
					     abs(bmi->bmiHeader.biHeight)) / 2;

		pInfo->bmiHeader.biBitCount = 1;
	        if (pInfo->bmiHeader.biSize == sizeof(BITMAPINFOHEADER))
	        {
	            RGBQUAD *rgb = pInfo->bmiColors;

	            pInfo->bmiHeader.biClrUsed = pInfo->bmiHeader.biClrImportant = 2;
	            rgb[0].rgbBlue = rgb[0].rgbGreen = rgb[0].rgbRed = 0x00;
	            rgb[1].rgbBlue = rgb[1].rgbGreen = rgb[1].rgbRed = 0xff;
	            rgb[0].rgbReserved = rgb[1].rgbReserved = 0;
	        }
	        else
	        {
	            RGBTRIPLE *rgb = (RGBTRIPLE *)(((BITMAPCOREHEADER *)pInfo) + 1);

	            rgb[0].rgbtBlue = rgb[0].rgbtGreen = rgb[0].rgbtRed = 0x00;
	            rgb[1].rgbtBlue = rgb[1].rgbtGreen = rgb[1].rgbtRed = 0xff;
	        }

	        /* Create the AND bitmap */

	    if (DoStretch) {
	      if ((hAndBits = CreateBitmap(width, height, 1, 1, NULL))) {
		HBITMAP hOld;
                BOOL res = FALSE;

                if (!hdcMem) hdcMem = CreateCompatibleDC(screen_dc);
                if (hdcMem) {
                    hOld = SelectObject(hdcMem, hAndBits);
                    res = StretchDIBits(hdcMem, 0, 0, width, height, 0, 0,
                                        pInfo->bmiHeader.biWidth, pInfo->bmiHeader.biHeight,
                                        xbits, pInfo, DIB_RGB_COLORS, SRCCOPY);
                    SelectObject(hdcMem, hOld);
                }
		if (!res) { DeleteObject(hAndBits); hAndBits = 0; }
	      }
	    } else hAndBits = CreateDIBitmap( screen_dc, &pInfo->bmiHeader,
	      CBM_INIT, xbits, pInfo, DIB_RGB_COLORS );

		if( !hAndBits ) DeleteObject( hXorBits );
	    }
	    HeapFree( GetProcessHeap(), 0, pInfo );
	}
    }

    if( !hXorBits || !hAndBits )
    {
	WARN_(cursor)("\tunable to create an icon bitmap.\n");
	return 0;
    }

    /* Now create the CURSORICONINFO structure */
    GetObjectA( hXorBits, sizeof(bmpXor), &bmpXor );
    GetObjectA( hAndBits, sizeof(bmpAnd), &bmpAnd );
    sizeXor = bmpXor.bmHeight * bmpXor.bmWidthBytes;
    sizeAnd = bmpAnd.bmHeight * bmpAnd.bmWidthBytes;
    sizePal = (bmpXor.bmPlanes == 1 && bmpXor.bmBitsPixel == 8) ?
	      (sizeof(RGBQUAD) << bmpXor.bmBitsPixel) : 0;

    if (hObj) hObj = GlobalReAlloc16( hObj,
		     sizeof(CURSORICONINFO) + sizeXor + sizeAnd + sizePal, GMEM_MOVEABLE );
    if (!hObj) hObj = GlobalAlloc16( GMEM_MOVEABLE,
		     sizeof(CURSORICONINFO) + sizeXor + sizeAnd + sizePal );
    if (hObj)
    {
	CURSORICONINFO *info;

	/* Make it owned by the module */
        if (hInstance) hInstance = GetExePtr(hInstance);
        FarSetOwner16( hObj, hInstance );

	info = (CURSORICONINFO *)GlobalLock16( hObj );
	info->ptHotSpot.x   = hotspot.x;
	info->ptHotSpot.y   = hotspot.y;
	info->nWidth        = bmpXor.bmWidth;
	info->nHeight       = bmpXor.bmHeight;
	info->nWidthBytes   = bmpXor.bmWidthBytes;
	info->bPlanes       = bmpXor.bmPlanes;
	info->bBitsPerPixel = bmpXor.bmBitsPixel;

	/* Transfer the bitmap bits to the CURSORICONINFO structure */

	GetBitmapBits( hAndBits, sizeAnd, (char *)(info + 1) );
	GetBitmapBits( hXorBits, sizeXor, (char *)(info + 1) + sizeAnd );
	if ((bmi->bmiHeader.biSize == sizeof(BITMAPV4HEADER) ||
	     bmi->bmiHeader.biSize == sizeof(BITMAPV5HEADER)) &&
	    ((BITMAPV4HEADER*)bmi)->bV4AlphaMask) {
	    /* we have an alpha channel...
	     * fill mask in case we don't have alpha-capable hardware */
	    CURSORICON_FillAlphaMask( info );
	} else {
	    /* fill alpha channel from mask */
	    CURSORICON_FillAlphaChannel( info );
	}
	if (sizePal) {
	    HBITMAP hOld;
	    if (!hdcMem) hdcMem = CreateCompatibleDC(screen_dc);
	    hOld = SelectObject(hdcMem, hXorBits);
	    GetDIBColorTable(hdcMem, 0, 1 << bmpXor.bmBitsPixel,
			     (RGBQUAD*)((char *)(info + 1) + sizeAnd + sizeXor));
	    SelectObject(hdcMem, hOld);
	}
	GlobalUnlock16( hObj );
    }

    DeleteObject( hAndBits );
    DeleteObject( hXorBits );
    return hObj;
}


/**********************************************************************
 *		CreateIconFromResourceEx (USER.450)
 *
 * FIXME: not sure about exact parameter types
 */
HICON16 WINAPI CreateIconFromResourceEx16( LPBYTE bits, UINT16 cbSize, BOOL16 bIcon,
                                    DWORD dwVersion, INT16 width, INT16 height, UINT16 cFlag )
{
    return CreateIconFromResourceEx(bits, cbSize, bIcon, dwVersion,
      width, height, cFlag);
}


/**********************************************************************
 *		CreateIconFromResource (USER32.@)
 */
HICON WINAPI CreateIconFromResource( LPBYTE bits, UINT cbSize,
                                           BOOL bIcon, DWORD dwVersion)
{
    return CreateIconFromResourceEx( bits, cbSize, bIcon, dwVersion, 0,0,0);
}


/**********************************************************************
 *		CreateIconFromResourceEx (USER32.@)
 */
HICON WINAPI CreateIconFromResourceEx( LPBYTE bits, UINT cbSize,
                                           BOOL bIcon, DWORD dwVersion,
                                           INT width, INT height,
                                           UINT cFlag )
{
    return CURSORICON_CreateFromResource( 0, 0, bits, cbSize, bIcon, dwVersion,
                                          width, height, cFlag );
}

/**********************************************************************
 *          CURSORICON_Load
 *
 * Load a cursor or icon from resource or file.
 */
HGLOBAL CURSORICON_Load( HINSTANCE hInstance, LPCWSTR name,
                         INT width, INT height, INT colors,
                         BOOL fCursor, UINT loadflags )
{
    HANDLE handle = 0, h = 0;
    HANDLE hRsrc;
    CURSORICONDIR *dir;
    CURSORICONDIRENTRY *dirEntry;
    LPBYTE bits;

    if ( loadflags & LR_LOADFROMFILE )    /* Load from file */
    {
        LPBYTE *ptr;
        if (!CURSORICON_SimulateLoadingFromResourceW((LPWSTR)name, fCursor, &dir, &ptr))
            return 0;
        if (fCursor)
            dirEntry = (CURSORICONDIRENTRY *)CURSORICON_FindBestCursor(dir, width, height, 1);
        else
            dirEntry = (CURSORICONDIRENTRY *)CURSORICON_FindBestIcon(dir, width, height, colors);

        if (!dirEntry) return 0;		
        bits = ptr[dirEntry->wResId-1];
        h = CURSORICON_CreateFromResource( 0, 0, bits, dirEntry->dwBytesInRes,
                                           !fCursor, 0x00030000, width, height, loadflags);
        HeapFree( GetProcessHeap(), 0, dir );
        HeapFree( GetProcessHeap(), 0, ptr );
    }
    else  /* Load from resource */
    {
        HANDLE hGroupRsrc;
        WORD wResId;
        DWORD dwBytesInRes;

        if (!hInstance)  /* Load OEM cursor/icon */
        {
            if (!(hInstance = GetModuleHandleA( "user32.dll" ))) return 0;
        }

        /* Normalize hInstance (must be uniquely represented for icon cache) */

        if ( HIWORD( hInstance ) )
            hInstance = MapHModuleLS( hInstance );
        else
            hInstance = GetExePtr( hInstance );

        /* Get directory resource ID */

        if (!(hRsrc = FindResourceW( hInstance, name,
                          fCursor ? RT_GROUP_CURSORW : RT_GROUP_ICONW )))
            return 0;
	hGroupRsrc = hRsrc;

        /* Find the best entry in the directory */

        if (!(handle = LoadResource( hInstance, hRsrc ))) return 0;
        if (!(dir = (CURSORICONDIR*)LockResource( handle ))) return 0;
        if (fCursor)
            dirEntry = (CURSORICONDIRENTRY *)CURSORICON_FindBestCursor( dir,
                                                              width, height, 1);
        else
            dirEntry = (CURSORICONDIRENTRY *)CURSORICON_FindBestIcon( dir,
                                                       width, height, colors );
        if (!dirEntry) return 0;
        wResId = dirEntry->wResId;
        dwBytesInRes = dirEntry->dwBytesInRes;
        FreeResource( handle );

        /* Load the resource */

        if (!(hRsrc = FindResourceW(hInstance,MAKEINTRESOURCEW(wResId),
                                      fCursor ? RT_CURSORW : RT_ICONW ))) return 0;

        /* If shared icon, check whether it was already loaded */
        if (    (loadflags & LR_SHARED)
             && (h = CURSORICON_FindSharedIcon( hInstance, hRsrc ) ) != 0 )
            return h;

        if (!(handle = LoadResource( hInstance, hRsrc ))) return 0;
        bits = (LPBYTE)LockResource( handle );
        h = CURSORICON_CreateFromResource( 0, 0, bits, dwBytesInRes,
                                           !fCursor, 0x00030000, width, height, loadflags);
        FreeResource( handle );

        /* If shared icon, add to icon cache */

        if ( h && (loadflags & LR_SHARED) )
            CURSORICON_AddSharedIcon( hInstance, hRsrc, hGroupRsrc, h );
    }

    return h;
}

/***********************************************************************
 *           CURSORICON_Copy
 *
 * Make a copy of a cursor or icon.
 */
static HGLOBAL16 CURSORICON_Copy( HINSTANCE16 hInstance, HGLOBAL16 handle )
{
    char *ptrOld, *ptrNew;
    int size;
    HGLOBAL16 hNew;

    if (!(ptrOld = (char *)GlobalLock16( handle ))) return 0;
    if (hInstance && !(hInstance = GetExePtr( hInstance ))) return 0;
    size = GlobalSize16( handle );
    hNew = GlobalAlloc16( GMEM_MOVEABLE, size );
    FarSetOwner16( hNew, hInstance );
    ptrNew = (char *)GlobalLock16( hNew );
    memcpy( ptrNew, ptrOld, size );
    GlobalUnlock16( handle );
    GlobalUnlock16( hNew );
    return hNew;
}

/*************************************************************************
 * CURSORICON_ExtCopy
 *
 * Copies an Image from the Cache if LR_COPYFROMRESOURCE is specified
 *
 * PARAMS
 *      Handle     [I] handle to an Image
 *      nType      [I] Type of Handle (IMAGE_CURSOR | IMAGE_ICON)
 *      iDesiredCX [I] The Desired width of the Image
 *      iDesiredCY [I] The desired height of the Image
 *      nFlags     [I] The flags from CopyImage
 *
 * RETURNS
 *     Success: The new handle of the Image
 *
 * NOTES
 *     LR_COPYDELETEORG and LR_MONOCHROME are currently not implemented.
 *     LR_MONOCHROME should be implemented by CURSORICON_CreateFromResource.
 *     LR_COPYFROMRESOURCE will only work if the Image is in the Cache.
 *
 *
 */

HGLOBAL CURSORICON_ExtCopy(HGLOBAL Handle, UINT nType,
			   INT iDesiredCX, INT iDesiredCY,
			   UINT nFlags)
{
    HGLOBAL16 hNew=0;

    TRACE_(icon)("Handle %u, uType %u, iDesiredCX %i, iDesiredCY %i, nFlags %u\n",
        Handle, nType, iDesiredCX, iDesiredCY, nFlags);

    if(Handle == 0)
    {
	return 0;
    }

    /* Best Fit or Monochrome */
    if( (nFlags & LR_COPYFROMRESOURCE
        && (iDesiredCX > 0 || iDesiredCY > 0))
        || nFlags & LR_MONOCHROME)
    {
        ICONCACHE* pIconCache = CURSORICON_FindCache(Handle);

        /* Not Found in Cache, then do a straight copy
        */
        if(pIconCache == NULL)
        {
            hNew = CURSORICON_Copy(0, Handle);
            if(nFlags & LR_COPYFROMRESOURCE)
            {
                TRACE_(icon)("LR_COPYFROMRESOURCE: Failed to load from cache\n");
            }
        }
        else
        {
            int iTargetCY = iDesiredCY, iTargetCX = iDesiredCX;
            LPBYTE pBits;
            HANDLE hMem;
            HRSRC hRsrc;
            DWORD dwBytesInRes;
            WORD wResId;
            CURSORICONDIR *pDir;
            CURSORICONDIRENTRY *pDirEntry;
            BOOL bIsIcon = (nType == IMAGE_ICON);

            /* Completing iDesiredCX CY for Monochrome Bitmaps if needed
            */
            if(((nFlags & LR_MONOCHROME) && !(nFlags & LR_COPYFROMRESOURCE))
                || (iDesiredCX == 0 && iDesiredCY == 0))
            {
                iDesiredCY = GetSystemMetrics(bIsIcon ?
                    SM_CYICON : SM_CYCURSOR);
                iDesiredCX = GetSystemMetrics(bIsIcon ?
                    SM_CXICON : SM_CXCURSOR);
            }

            /* Retrieve the CURSORICONDIRENTRY
            */
            if (!(hMem = LoadResource( pIconCache->hModule ,
                            pIconCache->hGroupRsrc)))
            {
                return 0;
            }
            if (!(pDir = (CURSORICONDIR*)LockResource( hMem )))
            {
                return 0;
            }

            /* Find Best Fit
            */
            if(bIsIcon)
            {
                pDirEntry = (CURSORICONDIRENTRY *)CURSORICON_FindBestIcon(
                                pDir, iDesiredCX, iDesiredCY, 256);
            }
            else
            {
                pDirEntry = (CURSORICONDIRENTRY *)CURSORICON_FindBestCursor(
                                pDir, iDesiredCX, iDesiredCY, 1);
            }

            if (!pDirEntry) return 0;		
            wResId = pDirEntry->wResId;
            dwBytesInRes = pDirEntry->dwBytesInRes;
            FreeResource(hMem);

            TRACE_(icon)("ResID %u, BytesInRes %lu, Width %d, Height %d DX %d, DY %d\n",
                wResId, dwBytesInRes,  pDirEntry->ResInfo.icon.bWidth,
                pDirEntry->ResInfo.icon.bHeight, iDesiredCX, iDesiredCY);

            /* Get the Best Fit
            */
            if (!(hRsrc = FindResourceW(pIconCache->hModule ,
                MAKEINTRESOURCEW(wResId), bIsIcon ? RT_ICONW : RT_CURSORW)))
            {
                return 0;
            }
            if (!(hMem = LoadResource( pIconCache->hModule , hRsrc )))
            {
                return 0;
            }

            pBits = (LPBYTE)LockResource( hMem );

	    if(nFlags & LR_DEFAULTSIZE)
	    {
	        iTargetCY = GetSystemMetrics(SM_CYICON);
                iTargetCX = GetSystemMetrics(SM_CXICON);
	    }

            /* Create a New Icon with the proper dimension
            */
            hNew = CURSORICON_CreateFromResource( 0, 0, pBits, dwBytesInRes,
                       bIsIcon, 0x00030000, iTargetCX, iTargetCY, nFlags);
            FreeResource(hMem);
        }
    }
    else hNew = CURSORICON_Copy(0, Handle);
    return hNew;
}


/***********************************************************************
 *		LoadCursor (USER.173)
 */
HCURSOR16 WINAPI LoadCursor16( HINSTANCE16 hInstance, LPCSTR name )
{
    return LoadCursorA( hInstance, name );
}


/***********************************************************************
 *		LoadIcon (USER.174)
 */
HICON16 WINAPI LoadIcon16( HINSTANCE16 hInstance, LPCSTR name )
{
    return LoadIconA( hInstance, name );
}


/***********************************************************************
 *		CreateCursor (USER.406)
 */
HCURSOR16 WINAPI CreateCursor16( HINSTANCE16 hInstance,
                                 INT16 xHotSpot, INT16 yHotSpot,
                                 INT16 nWidth, INT16 nHeight,
                                 LPCVOID lpANDbits, LPCVOID lpXORbits )
{
    CURSORICONINFO info;

    TRACE_(cursor)("%dx%d spot=%d,%d xor=%p and=%p\n",
                    nWidth, nHeight, xHotSpot, yHotSpot, lpXORbits, lpANDbits);

    info.ptHotSpot.x = xHotSpot;
    info.ptHotSpot.y = yHotSpot;
    info.nWidth = nWidth;
    info.nHeight = nHeight;
    info.nWidthBytes = 0;
    info.bPlanes = 1;
    info.bBitsPerPixel = 1;

    return CreateCursorIconIndirect16( hInstance, &info, lpANDbits, lpXORbits );
}


/***********************************************************************
 *		CreateCursor (USER32.@)
 */
HCURSOR WINAPI CreateCursor( HINSTANCE hInstance,
                                 INT xHotSpot, INT yHotSpot,
                                 INT nWidth, INT nHeight,
                                 LPCVOID lpANDbits, LPCVOID lpXORbits )
{
    CURSORICONINFO info;

    TRACE_(cursor)("%dx%d spot=%d,%d xor=%p and=%p\n",
                    nWidth, nHeight, xHotSpot, yHotSpot, lpXORbits, lpANDbits);

    info.ptHotSpot.x = xHotSpot;
    info.ptHotSpot.y = yHotSpot;
    info.nWidth = nWidth;
    info.nHeight = nHeight;
    info.nWidthBytes = 0;
    info.bPlanes = 1;
    info.bBitsPerPixel = 1;

    return CreateCursorIconIndirect16( 0, &info, lpANDbits, lpXORbits );
}


/***********************************************************************
 *		CreateIcon (USER.407)
 */
HICON16 WINAPI CreateIcon16( HINSTANCE16 hInstance, INT16 nWidth,
                             INT16 nHeight, BYTE bPlanes, BYTE bBitsPixel,
                             LPCVOID lpANDbits, LPCVOID lpXORbits )
{
    CURSORICONINFO info;

    TRACE_(icon)("%dx%dx%d, xor=%p, and=%p\n",
                  nWidth, nHeight, bPlanes * bBitsPixel, lpXORbits, lpANDbits);

    info.ptHotSpot.x = ICON_HOTSPOT;
    info.ptHotSpot.y = ICON_HOTSPOT;
    info.nWidth = nWidth;
    info.nHeight = nHeight;
    info.nWidthBytes = 0;
    info.bPlanes = bPlanes;
    info.bBitsPerPixel = bBitsPixel;

    return CreateCursorIconIndirect16( hInstance, &info, lpANDbits, lpXORbits );
}


/***********************************************************************
 *		CreateIcon (USER32.@)
 *
 *  Creates an icon based on the specified bitmaps. The bitmaps must be
 *  provided in a device dependent format and will be resized to
 *  (SM_CXICON,SM_CYICON) and depth converted to match the screen's color
 *  depth. The provided bitmaps must be top-down bitmaps.
 *  Although Windows does not support 15bpp(*) this API must support it
 *  for Winelib applications.
 *
 *  (*) Windows does not support 15bpp but it supports the 555 RGB 16bpp
 *      format!
 *
 * BUGS
 *
 *  - The provided bitmaps are not resized!
 *  - The documentation says the lpXORbits bitmap must be in a device
 *    dependent format. But we must still resize it and perform depth
 *    conversions if necessary.
 *  - I'm a bit unsure about the how the 'device dependent format' thing works.
 *    I did some tests on windows and found that if you provide a 16bpp bitmap
 *    in lpXORbits, then its format but be 565 RGB if the screen's bit depth
 *    is 16bpp but it must be 555 RGB if the screen's bit depth is anything
 *    else. I don't know if this is part of the GDI specs or if this is a
 *    quirk of the graphics card driver.
 *  - You may think that we check whether the bit depths match or not
 *    as an optimization. But the truth is that the conversion using
 *    CreateDIBitmap does not work for some bit depth (e.g. 8bpp) and I have
 *    no idea why.
 *  - I'm pretty sure that all the things we do in CreateIcon should
 *    also be done in CreateIconIndirect...
 */
HICON WINAPI CreateIcon(
    HINSTANCE hInstance,  /* [in] the application's hInstance, currently unused */
    INT       nWidth,     /* [in] the width of the provided bitmaps */
    INT       nHeight,    /* [in] the height of the provided bitmaps */
    BYTE      bPlanes,    /* [in] the number of planes in the provided bitmaps */
    BYTE      bBitsPixel, /* [in] the number of bits per pixel of the lpXORbits bitmap */
    LPCVOID   lpANDbits,  /* [in] a monochrome bitmap representing the icon's mask */
    LPCVOID   lpXORbits)  /* [in] the icon's 'color' bitmap */
{
    HICON hIcon;
    HDC hdc;

    TRACE_(icon)("%dx%dx%d, xor=%p, and=%p\n",
                 nWidth, nHeight, bPlanes * bBitsPixel, lpXORbits, lpANDbits);

    hdc=GetDC(0);
    if (!hdc)
        return 0;

    if (GetDeviceCaps(hdc,BITSPIXEL)==bBitsPixel) {
        CURSORICONINFO info;

        info.ptHotSpot.x = ICON_HOTSPOT;
        info.ptHotSpot.y = ICON_HOTSPOT;
        info.nWidth = nWidth;
        info.nHeight = nHeight;
        info.nWidthBytes = 0;
        info.bPlanes = bPlanes;
        info.bBitsPerPixel = bBitsPixel;

        hIcon=CreateCursorIconIndirect16( 0, &info, lpANDbits, lpXORbits );
    } else {
        ICONINFO iinfo;
        BITMAPINFO bmi;

        iinfo.fIcon=TRUE;
        iinfo.xHotspot=ICON_HOTSPOT;
        iinfo.yHotspot=ICON_HOTSPOT;
        iinfo.hbmMask=CreateBitmap(nWidth,nHeight,1,1,lpANDbits);

        bmi.bmiHeader.biSize=sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth=nWidth;
        bmi.bmiHeader.biHeight=-nHeight;
        bmi.bmiHeader.biPlanes=bPlanes;
        bmi.bmiHeader.biBitCount=bBitsPixel;
        bmi.bmiHeader.biCompression=BI_RGB;
        bmi.bmiHeader.biSizeImage=0;
        bmi.bmiHeader.biXPelsPerMeter=0;
        bmi.bmiHeader.biYPelsPerMeter=0;
        bmi.bmiHeader.biClrUsed=0;
        bmi.bmiHeader.biClrImportant=0;

        iinfo.hbmColor = CreateDIBitmap( hdc, &bmi.bmiHeader,
                                         CBM_INIT, lpXORbits,
                                         &bmi, DIB_RGB_COLORS );

        hIcon=CreateIconIndirect(&iinfo);
        DeleteObject(iinfo.hbmMask);
        DeleteObject(iinfo.hbmColor);
    }
    ReleaseDC(0,hdc);
    return hIcon;
}


/***********************************************************************
 *		CreateCursorIconIndirect (USER.408)
 */
HGLOBAL16 WINAPI CreateCursorIconIndirect16( HINSTANCE16 hInstance,
                                           CURSORICONINFO *info,
                                           LPCVOID lpANDbits,
                                           LPCVOID lpXORbits )
{
    HGLOBAL16 handle;
    char *ptr;
    int sizeAnd, sizeXor;

    hInstance = GetExePtr( hInstance );  /* Make it a module handle */
    if (!lpXORbits || !lpANDbits || info->bPlanes != 1) return 0;
    info->nWidthBytes = BITMAP_GetWidthBytes(info->nWidth,info->bBitsPerPixel);
    sizeXor = info->nHeight * info->nWidthBytes;
    sizeAnd = info->nHeight * BITMAP_GetWidthBytes( info->nWidth, 1 );
    if (!(handle = GlobalAlloc16( GMEM_MOVEABLE,
                                  sizeof(CURSORICONINFO) + sizeXor + sizeAnd)))
        return 0;
    FarSetOwner16( handle, hInstance );
    ptr = (char *)GlobalLock16( handle );
    memcpy( ptr, info, sizeof(*info) );
    memcpy( ptr + sizeof(CURSORICONINFO), lpANDbits, sizeAnd );
    memcpy( ptr + sizeof(CURSORICONINFO) + sizeAnd, lpXORbits, sizeXor );
    GlobalUnlock16( handle );
    return handle;
}


/***********************************************************************
 *		CopyIcon (USER.368)
 */
HICON16 WINAPI CopyIcon16( HINSTANCE16 hInstance, HICON16 hIcon )
{
    TRACE_(icon)("%04x %04x\n", hInstance, hIcon );
    return CURSORICON_Copy( hInstance, hIcon );
}


/***********************************************************************
 *		CopyIcon (USER32.@)
 */
HICON WINAPI CopyIcon( HICON hIcon )
{
    TRACE_(icon)("%04x\n", hIcon );
    return CURSORICON_Copy( 0, hIcon );
}


/***********************************************************************
 *		CopyCursor (USER.369)
 */
HCURSOR16 WINAPI CopyCursor16( HINSTANCE16 hInstance, HCURSOR16 hCursor )
{
    TRACE_(cursor)("%04x %04x\n", hInstance, hCursor );
    return CURSORICON_Copy( hInstance, hCursor );
}

/**********************************************************************
 *		DestroyIcon32 (USER.610)
 *		DestroyIcon32 (USER32.@)
 *
 * This routine is actually exported from Win95 USER under the name
 * DestroyIcon32 ...  The behaviour implemented here should mimic
 * the Win95 one exactly, especially the return values, which
 * depend on the setting of various flags.
 */
WORD WINAPI DestroyIcon32( HGLOBAL16 handle, UINT16 flags )
{
    WORD retv;

    TRACE_(icon)("(%04x, %04x)\n", handle, flags );

    /* Check whether destroying active cursor */

    if ( hActiveCursor == handle )
    {
        WARN_(cursor)("Destroying active cursor!\n" );
        SetCursor( 0 );
    }

    /* Try shared cursor/icon first */

    if ( !(flags & CID_NONSHARED) )
    {
        INT count = CURSORICON_DelSharedIcon( handle );

        if ( count != -1 )
            return (flags & CID_WIN32)? TRUE : (count == 0);

        /* FIXME: OEM cursors/icons should be recognized */
    }

    /* Now assume non-shared cursor/icon */

    retv = GlobalFree16( handle );
    return (flags & CID_RESOURCE)? retv : TRUE;
}

/***********************************************************************
 *		DestroyIcon (USER.457)
 */
BOOL16 WINAPI DestroyIcon16( HICON16 hIcon )
{
    return DestroyIcon32( hIcon, 0 );
}

/***********************************************************************
 *		DestroyIcon (USER32.@)
 */
BOOL WINAPI DestroyIcon( HICON hIcon )
{
    return DestroyIcon32( hIcon, CID_WIN32 );
}

/***********************************************************************
 *		DestroyCursor (USER.458)
 */
BOOL16 WINAPI DestroyCursor16( HCURSOR16 hCursor )
{
    return DestroyIcon32( hCursor, 0 );
}

/***********************************************************************
 *		DestroyCursor (USER32.@)
 */
BOOL WINAPI DestroyCursor( HCURSOR hCursor )
{
    return DestroyIcon32( hCursor, CID_WIN32 );
}


/***********************************************************************
 *		DrawIcon (USER.84)
 */
BOOL16 WINAPI DrawIcon16( HDC16 hdc, INT16 x, INT16 y, HICON16 hIcon )
{
    return DrawIcon( hdc, x, y, hIcon );
}


/***********************************************************************
 *		DrawIcon (USER32.@)
 */
BOOL WINAPI DrawIcon( HDC hdc, INT x, INT y, HICON hIcon )
{
    CURSORICONINFO *ptr;
    HDC hMemDC;
    HBITMAP hXorBits, hAndBits;
    COLORREF oldFg, oldBg;

    if (!(ptr = (CURSORICONINFO *)GlobalLock16( hIcon ))) return FALSE;
    if (!(hMemDC = CreateCompatibleDC( hdc ))) return FALSE;
    hAndBits = CreateBitmap( ptr->nWidth, ptr->nHeight, 1, 1,
                               (char *)(ptr+1) );
    hXorBits = CreateBitmap( ptr->nWidth, ptr->nHeight, ptr->bPlanes,
                               ptr->bBitsPerPixel, (char *)(ptr + 1)
                        + ptr->nHeight * BITMAP_GetWidthBytes(ptr->nWidth,1) );
    oldFg = SetTextColor( hdc, RGB(0,0,0) );
    oldBg = SetBkColor( hdc, RGB(255,255,255) );

    if (hXorBits && hAndBits)
    {
        HBITMAP hBitTemp = SelectObject( hMemDC, hAndBits );
        BitBlt( hdc, x, y, ptr->nWidth, ptr->nHeight, hMemDC, 0, 0, SRCAND );
        SelectObject( hMemDC, hXorBits );
        BitBlt(hdc, x, y, ptr->nWidth, ptr->nHeight, hMemDC, 0, 0,SRCINVERT);
        SelectObject( hMemDC, hBitTemp );
    }
    DeleteDC( hMemDC );
    if (hXorBits) DeleteObject( hXorBits );
    if (hAndBits) DeleteObject( hAndBits );
    GlobalUnlock16( hIcon );
    SetTextColor( hdc, oldFg );
    SetBkColor( hdc, oldBg );
    return TRUE;
}


/***********************************************************************
 *		IconSize (USER.86)
 *
 * See "Undocumented Windows". Used by W2.0 paint.exe.
 */
DWORD WINAPI IconSize16( void )
{
    return MAKELONG(GetSystemMetrics(SM_CYICON), GetSystemMetrics(SM_CXICON));
}


/***********************************************************************
 *		DumpIcon (USER.459)
 */
DWORD WINAPI DumpIcon16( SEGPTR pInfo, WORD *lpLen,
                       SEGPTR *lpXorBits, SEGPTR *lpAndBits )
{
    CURSORICONINFO *info = MapSL( pInfo );
    int sizeAnd, sizeXor;

    if (!info) return 0;
    sizeXor = info->nHeight * info->nWidthBytes;
    sizeAnd = info->nHeight * BITMAP_GetWidthBytes( info->nWidth, 1 );
    if (lpAndBits) *lpAndBits = pInfo + sizeof(CURSORICONINFO);
    if (lpXorBits) *lpXorBits = pInfo + sizeof(CURSORICONINFO) + sizeAnd;
    if (lpLen) *lpLen = sizeof(CURSORICONINFO) + sizeAnd + sizeXor;
    return MAKELONG( sizeXor, sizeXor );
}


/***********************************************************************
 *		SetCursor (USER.69)
 */
HCURSOR16 WINAPI SetCursor16( HCURSOR16 hCursor )
{
    return (HCURSOR16)SetCursor( hCursor );
}


/***********************************************************************
 *		SetCursor (USER32.@)
 * RETURNS:
 *	A handle to the previous cursor shape.
 */
HCURSOR WINAPI SetCursor(
	         HCURSOR hCursor /* [in] Handle of cursor to show */
) {
    HCURSOR hOldCursor;

    /* before we shortcut out, test for NULL in case game wants to
       hide system cursor before actually assigning any cursor */
    if (hCursor == NULL && (CURSOR_ShowCount >= 0))
    {
        USER_Driver.pSetCursor( NULL );
        hOldCursor = hActiveCursor;
        hActiveCursor = NULL;
        return hOldCursor;
    }
            
    if (hCursor == hActiveCursor) return hActiveCursor;  /* No change */
    TRACE_(cursor)("%04x\n", hCursor );
    hOldCursor = hActiveCursor;
    hActiveCursor = hCursor;
    /* Change the cursor shape only if it is visible */
    if (CURSOR_ShowCount >= 0)
    {
        USER_Driver.pSetCursor( (CURSORICONINFO*)GlobalLock16( hActiveCursor ) );
        GlobalUnlock16( hActiveCursor );
    }
    return hOldCursor;
}


/***********************************************************************
 *		ShowCursor (USER.71)
 */
INT16 WINAPI ShowCursor16( BOOL16 bShow )
{
    return ShowCursor( bShow );
}


/***********************************************************************
 *		ShowCursor (USER32.@)
 */
INT WINAPI ShowCursor( BOOL bShow )
{
    TRACE_(cursor)("%d, count=%d\n",
                    bShow, CURSOR_ShowCount );

    if (bShow)
    {
        if (++CURSOR_ShowCount == 0)  /* Show it */
        {
            USER_Driver.pSetCursor( (CURSORICONINFO*)GlobalLock16( hActiveCursor ) );
            GlobalUnlock16( hActiveCursor );
        }
    }
    else
    {
        if (--CURSOR_ShowCount == -1)  /* Hide it */
            USER_Driver.pSetCursor( NULL );
    }
    return CURSOR_ShowCount;
}


/***********************************************************************
 *		GetCursor (USER.247)
 */
HCURSOR16 WINAPI GetCursor16(void)
{
    return hActiveCursor;
}


/***********************************************************************
 *		GetCursor (USER32.@)
 */
HCURSOR WINAPI GetCursor(void)
{
    return hActiveCursor;
}


/***********************************************************************
 *             GetCursorInfo (USER32.@)
 */
BOOL WINAPI GetCursorInfo(PCURSORINFO pci)
{
    pci->flags = (CURSOR_ShowCount >= 0 ? CURSOR_SHOWING : 0);
    pci->hCursor = GetCursor();
    GetCursorPos(&pci->ptScreenPos);
    return TRUE;
}		


/***********************************************************************
 *		ClipCursor (USER.16)
 */
BOOL16 WINAPI ClipCursor16( const RECT16 *rect )
{
    if (!rect) SetRectEmpty( &CURSOR_ClipRect );
    else CONV_RECT16TO32( rect, &CURSOR_ClipRect );
    return TRUE;
}


/***********************************************************************
 *		ClipCursor (USER32.@)
 */
BOOL WINAPI ClipCursor( const RECT *rect )
{
    if (!rect) SetRectEmpty( &CURSOR_ClipRect );
    else CopyRect( &CURSOR_ClipRect, rect );
    return TRUE;
}


/***********************************************************************
 *		GetClipCursor (USER.309)
 */
void WINAPI GetClipCursor16( RECT16 *rect )
{
    if (rect) CONV_RECT32TO16( &CURSOR_ClipRect, rect );
}


/***********************************************************************
 *		GetClipCursor (USER32.@)
 */
BOOL WINAPI GetClipCursor( RECT *rect )
{
    if (rect)
    {
       if (IsRectEmpty( &CURSOR_ClipRect ))
           SetRect( rect, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) );
       else
           CopyRect( rect, &CURSOR_ClipRect );
       return TRUE;
    }
    return FALSE;
}

/**********************************************************************
 *		LookupIconIdFromDirectoryEx (USER.364)
 *
 * FIXME: exact parameter sizes
 */
INT16 WINAPI LookupIconIdFromDirectoryEx16( LPBYTE xdir, BOOL16 bIcon,
	     INT16 width, INT16 height, UINT16 cFlag )
{
    CURSORICONDIR	*dir = (CURSORICONDIR*)xdir;
    UINT16 retVal = 0;
    if( dir && !dir->idReserved && (dir->idType & 3) )
    {
	CURSORICONDIRENTRY* entry;
	HDC hdc;
	UINT palEnts;
	int colors;
	hdc = GetDC(0);
	palEnts = GetSystemPaletteEntries(hdc, 0, 0, NULL);
	if (palEnts == 0)
	    palEnts = 256;
	colors = (cFlag & LR_MONOCHROME) ? 2 : palEnts;

	ReleaseDC(0, hdc);

	if( bIcon )
	    entry = CURSORICON_FindBestIcon( dir, width, height, colors );
	else
	    entry = CURSORICON_FindBestCursor( dir, width, height, 1);

	if( entry ) retVal = entry->wResId;
    }
    else WARN_(cursor)("invalid resource directory\n");
    return retVal;
}

/**********************************************************************
 *		LookupIconIdFromDirectoryEx (USER32.@)
 */
INT WINAPI LookupIconIdFromDirectoryEx( LPBYTE dir, BOOL bIcon,
             INT width, INT height, UINT cFlag )
{
    return LookupIconIdFromDirectoryEx16( dir, bIcon, width, height, cFlag );
}

/**********************************************************************
 *		LookupIconIdFromDirectory (USER.?)
 */
INT16 WINAPI LookupIconIdFromDirectory16( LPBYTE dir, BOOL16 bIcon )
{
    return LookupIconIdFromDirectoryEx16( dir, bIcon,
	   bIcon ? GetSystemMetrics(SM_CXICON) : GetSystemMetrics(SM_CXCURSOR),
	   bIcon ? GetSystemMetrics(SM_CYICON) : GetSystemMetrics(SM_CYCURSOR), bIcon ? 0 : LR_MONOCHROME );
}

/**********************************************************************
 *		LookupIconIdFromDirectory (USER32.@)
 */
INT WINAPI LookupIconIdFromDirectory( LPBYTE dir, BOOL bIcon )
{
    return LookupIconIdFromDirectoryEx( dir, bIcon,
	   bIcon ? GetSystemMetrics(SM_CXICON) : GetSystemMetrics(SM_CXCURSOR),
	   bIcon ? GetSystemMetrics(SM_CYICON) : GetSystemMetrics(SM_CYCURSOR), bIcon ? 0 : LR_MONOCHROME );
}

/**********************************************************************
 *		GetIconID (USER.455)
 */
WORD WINAPI GetIconID16( HGLOBAL16 hResource, DWORD resType )
{
    LPBYTE lpDir = (LPBYTE)GlobalLock16(hResource);

    TRACE_(cursor)("hRes=%04x, entries=%i\n",
                    hResource, lpDir ? ((CURSORICONDIR*)lpDir)->idCount : 0);

    switch(resType)
    {
	case RT_CURSOR16:
	     return (WORD)LookupIconIdFromDirectoryEx16( lpDir, FALSE,
			  GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR), LR_MONOCHROME );
	case RT_ICON16:
	     return (WORD)LookupIconIdFromDirectoryEx16( lpDir, TRUE,
			  GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0 );
	default:
	     WARN_(cursor)("invalid res type %ld\n", resType );
    }
    return 0;
}

/**********************************************************************
 *		LoadCursorIconHandler (USER.336)
 *
 * Supposed to load resources of Windows 2.x applications.
 */
HGLOBAL16 WINAPI LoadCursorIconHandler16( HGLOBAL16 hResource, HMODULE16 hModule, HRSRC16 hRsrc )
{
    FIXME_(cursor)("(%04x,%04x,%04x): old 2.x resources are not supported!\n",
	  hResource, hModule, hRsrc);
    return (HGLOBAL16)0;
}

/**********************************************************************
 *		LoadDIBIconHandler (USER.357)
 *
 * RT_ICON resource loader, installed by USER_SignalProc when module
 * is initialized.
 */
HGLOBAL16 WINAPI LoadDIBIconHandler16( HGLOBAL16 hMemObj, HMODULE16 hModule, HRSRC16 hRsrc )
{
    /* If hResource is zero we must allocate a new memory block, if it's
     * non-zero but GlobalLock() returns NULL then it was discarded and
     * we have to recommit some memory, otherwise we just need to check
     * the block size. See LoadProc() in 16-bit SDK for more.
     */

     hMemObj = NE_DefResourceHandler( hMemObj, hModule, hRsrc );
     if( hMemObj )
     {
	 LPBYTE bits = (LPBYTE)GlobalLock16( hMemObj );
	 hMemObj = CURSORICON_CreateFromResource( hModule, hMemObj, bits,
		   SizeofResource16(hModule, hRsrc), TRUE, 0x00030000,
		   GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR );
     }
     return hMemObj;
}

/**********************************************************************
 *		LoadDIBCursorHandler (USER.356)
 *
 * RT_CURSOR resource loader. Same as above.
 */
HGLOBAL16 WINAPI LoadDIBCursorHandler16( HGLOBAL16 hMemObj, HMODULE16 hModule, HRSRC16 hRsrc )
{
    hMemObj = NE_DefResourceHandler( hMemObj, hModule, hRsrc );
    if( hMemObj )
    {
	LPBYTE bits = (LPBYTE)GlobalLock16( hMemObj );
	hMemObj = CURSORICON_CreateFromResource( hModule, hMemObj, bits,
		  SizeofResource16(hModule, hRsrc), FALSE, 0x00030000,
		  GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR), LR_MONOCHROME );
    }
    return hMemObj;
}

/**********************************************************************
 *		LoadIconHandler (USER.456)
 */
HICON16 WINAPI LoadIconHandler16( HGLOBAL16 hResource, BOOL16 bNew )
{
    LPBYTE bits = (LPBYTE)LockResource16( hResource );

    TRACE_(cursor)("hRes=%04x\n",hResource);

    return CURSORICON_CreateFromResource( 0, 0, bits, 0, TRUE,
		      bNew ? 0x00030000 : 0x00020000, 0, 0, LR_DEFAULTCOLOR );
}

/***********************************************************************
 *		LoadCursorW (USER32.@)
 */
HCURSOR WINAPI LoadCursorW(HINSTANCE hInstance, LPCWSTR name)
{
    return LoadImageW( hInstance, name, IMAGE_CURSOR, 0, 0,
                       LR_SHARED | LR_DEFAULTSIZE );
}

/***********************************************************************
 *		LoadCursorA (USER32.@)
 */
HCURSOR WINAPI LoadCursorA(HINSTANCE hInstance, LPCSTR name)
{
    return LoadImageA( hInstance, name, IMAGE_CURSOR, 0, 0,
                       LR_SHARED | LR_DEFAULTSIZE );
}

/***********************************************************************
 *		LoadCursorFromFileW (USER32.@)
 */
HCURSOR WINAPI LoadCursorFromFileW (LPCWSTR name)
{
    return LoadImageW( 0, name, IMAGE_CURSOR, 0, 0,
                       LR_LOADFROMFILE | LR_DEFAULTSIZE );
}

/***********************************************************************
 *		LoadCursorFromFileA (USER32.@)
 */
HCURSOR WINAPI LoadCursorFromFileA (LPCSTR name)
{
    return LoadImageA( 0, name, IMAGE_CURSOR, 0, 0,
                       LR_LOADFROMFILE | LR_DEFAULTSIZE );
}

/***********************************************************************
 *		LoadIconW (USER32.@)
 */
HICON WINAPI LoadIconW(HINSTANCE hInstance, LPCWSTR name)
{
    return LoadImageW( hInstance, name, IMAGE_ICON, 0, 0,
                       LR_SHARED | LR_DEFAULTSIZE );
}

/***********************************************************************
 *		LoadIconA (USER32.@)
 */
HICON WINAPI LoadIconA(HINSTANCE hInstance, LPCSTR name)
{
    return LoadImageA( hInstance, name, IMAGE_ICON, 0, 0,
                       LR_SHARED | LR_DEFAULTSIZE );
}

/**********************************************************************
 *		GetIconInfo (USER.395)
 */
BOOL16 WINAPI GetIconInfo16(HICON16 hIcon,LPICONINFO16 iconinfo)
{
    ICONINFO	ii32;
    BOOL16	ret = GetIconInfo((HICON)hIcon, &ii32);

    iconinfo->fIcon = ii32.fIcon;
    iconinfo->xHotspot = ii32.xHotspot;
    iconinfo->yHotspot = ii32.yHotspot;
    iconinfo->hbmMask = ii32.hbmMask;
    iconinfo->hbmColor = ii32.hbmColor;
    return ret;
}

/**********************************************************************
 *		GetIconInfo (USER32.@)
 */
BOOL WINAPI GetIconInfo(HICON hIcon,PICONINFO iconinfo) {
    CURSORICONINFO	*ciconinfo;

    ciconinfo = GlobalLock16(hIcon);
    if (!ciconinfo)
	return FALSE;

    if ( (ciconinfo->ptHotSpot.x == ICON_HOTSPOT) &&
	 (ciconinfo->ptHotSpot.y == ICON_HOTSPOT) )
    {
      iconinfo->fIcon    = TRUE;
      iconinfo->xHotspot = ciconinfo->nWidth / 2;
      iconinfo->yHotspot = ciconinfo->nHeight / 2;
    }
    else
    {
      iconinfo->fIcon    = FALSE;
      iconinfo->xHotspot = ciconinfo->ptHotSpot.x;
      iconinfo->yHotspot = ciconinfo->ptHotSpot.y;
    }

    iconinfo->hbmColor = CreateBitmap ( ciconinfo->nWidth, ciconinfo->nHeight,
                                ciconinfo->bPlanes, ciconinfo->bBitsPerPixel,
                                (char *)(ciconinfo + 1)
                                + ciconinfo->nHeight *
                                BITMAP_GetWidthBytes (ciconinfo->nWidth,1) );
    iconinfo->hbmMask = CreateBitmap ( ciconinfo->nWidth, ciconinfo->nHeight,
                                1, 1, (char *)(ciconinfo + 1));

    GlobalUnlock16(hIcon);

    return TRUE;
}

/**********************************************************************
 *		CreateIconIndirect (USER32.@)
 */
HICON WINAPI CreateIconIndirect(PICONINFO iconinfo)
{
    DIBSECTIONOBJ bmpXor,bmpAnd;
    HICON hObj;
    int	hdrXor,hdrAnd,sizeXor,sizeAnd,sizePal;

    hdrXor = GetObjectA( iconinfo->hbmColor, sizeof(bmpXor), &bmpXor );
    hdrAnd = GetObjectA( iconinfo->hbmMask, sizeof(bmpAnd), &bmpAnd );

    TRACE_(icon)("hbmColor=%08x, hbmMask=%08x\n", iconinfo->hbmColor, iconinfo->hbmMask);

    sizeXor = bmpXor.dsBm.bmHeight * bmpXor.dsBm.bmWidthBytes;
    sizeAnd = bmpAnd.dsBm.bmHeight * bmpAnd.dsBm.bmWidthBytes;
    sizePal = (bmpXor.dsBm.bmPlanes == 1 && bmpXor.dsBm.bmBitsPixel == 8) ?
	      (sizeof(RGBQUAD) << bmpXor.dsBm.bmBitsPixel) : 0;

    TRACE_(icon)("width=%d, height=%d, width_bytes=%d, bpp=%d\n",
		 bmpXor.dsBm.bmWidth, bmpXor.dsBm.bmHeight,
		 bmpXor.dsBm.bmWidthBytes, bmpXor.dsBm.bmBitsPixel);

    hObj = GlobalAlloc16( GMEM_MOVEABLE,
		     sizeof(CURSORICONINFO) + sizeXor + sizeAnd + sizePal );
    if (hObj)
    {
	CURSORICONINFO *info;

	info = (CURSORICONINFO *)GlobalLock16( hObj );

	/* If we are creating an icon, the hotspot is unused */
	if (iconinfo->fIcon)
	{
	  info->ptHotSpot.x   = ICON_HOTSPOT;
	  info->ptHotSpot.y   = ICON_HOTSPOT;
	}
	else
	{
	  info->ptHotSpot.x   = iconinfo->xHotspot;
	  info->ptHotSpot.y   = iconinfo->yHotspot;
	}

	info->nWidth        = bmpXor.dsBm.bmWidth;
	info->nHeight       = bmpXor.dsBm.bmHeight;
	info->nWidthBytes   = bmpXor.dsBm.bmWidthBytes;
	info->bPlanes       = bmpXor.dsBm.bmPlanes;
	info->bBitsPerPixel = bmpXor.dsBm.bmBitsPixel;

	/* Transfer the bitmap bits to the CURSORICONINFO structure */

	GetBitmapBits( iconinfo->hbmMask ,sizeAnd,(char*)(info + 1) );
	GetBitmapBits( iconinfo->hbmColor,sizeXor,(char*)(info + 1) +sizeAnd);
	if (hdrXor == sizeof(DIBSECTIONOBJ) && bmpXor.dsAlphaMask) {
	    TRACE_(icon)("user-defined alpha channel detected\n");
	    /* fill alpha mask in case of non-alpha-capable hardware */
	    CURSORICON_FillAlphaMask( info );
	} else {
	    /* fill alpha channel from mask */
	    CURSORICON_FillAlphaChannel( info );
	}
	if (sizePal) {
	    HDC hdcMem;
	    HBITMAP hOld;
	    if (!screen_dc || GetObjectType(screen_dc) != OBJ_DC) screen_dc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
	    hdcMem = CreateCompatibleDC(screen_dc);
	    hOld = SelectObject(hdcMem, iconinfo->hbmColor);
	    GetDIBColorTable(hdcMem, 0, 1 << bmpXor.dsBm.bmBitsPixel,
			     (RGBQUAD*)((char *)(info + 1) + sizeAnd + sizeXor));
	    SelectObject(hdcMem, hOld);
	    DeleteDC(hdcMem);
	}
	GlobalUnlock16( hObj );
    }
    TRACE_(icon)("returning %08x\n", hObj);
    return hObj;
}


/**********************************************************************
 *		DrawIconEx (USER.394)
 */
BOOL16 WINAPI DrawIconEx16 (HDC16 hdc, INT16 xLeft, INT16 yTop, HICON16 hIcon,
			    INT16 cxWidth, INT16 cyWidth, UINT16 istep,
			    HBRUSH16 hbr, UINT16 flags)
{
    return DrawIconEx(hdc, xLeft, yTop, hIcon, cxWidth, cyWidth,
                        istep, hbr, flags);
}


/******************************************************************************
 *		DrawIconEx (USER32.@) Draws an icon or cursor on device context
 *
 * NOTES
 *    Why is this using SM_CXICON instead of SM_CXCURSOR?
 *
 * PARAMS
 *    hdc     [I] Handle to device context
 *    x0      [I] X coordinate of upper left corner
 *    y0      [I] Y coordinate of upper left corner
 *    hIcon   [I] Handle to icon to draw
 *    cxWidth [I] Width of icon
 *    cyWidth [I] Height of icon
 *    istep   [I] Index of frame in animated cursor
 *    hbr     [I] Handle to background brush
 *    flags   [I] Icon-drawing flags
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI DrawIconEx( HDC hdc, INT x0, INT y0, HICON hIcon,
                            INT cxWidth, INT cyWidth, UINT istep,
                            HBRUSH hbr, UINT flags )
{
    CURSORICONINFO *ptr = (CURSORICONINFO *)GlobalLock16 (hIcon);
    HDC hDC_off = 0, hMemDC = CreateCompatibleDC (hdc);
    BOOL result = FALSE, DoOffscreen;
    HBITMAP hB_off = 0, hOld = 0;

    if (!ptr) return FALSE;
    TRACE_(icon)("(hdc=%x,pos=%d.%d,hicon=%x,extend=%d.%d,istep=%d,br=%x,flags=0x%08x)\n",
	    hdc,x0,y0,hIcon,cxWidth,cyWidth,istep,hbr,flags
    );

    if (istep)
        FIXME_(icon)("Ignoring istep=%d\n", istep);
    if (flags & DI_COMPAT)
        FIXME_(icon)("Ignoring flag DI_COMPAT\n");

    if (!flags) {
	FIXME_(icon)("no flags set? setting to DI_NORMAL\n");
	flags = DI_NORMAL;
    }

    /* Calculate the size of the destination image.  */
    if (cxWidth == 0)
    {
      if (flags & DI_DEFAULTSIZE)
	cxWidth = GetSystemMetrics (SM_CXICON);
      else
	cxWidth = ptr->nWidth;
    }
    if (cyWidth == 0)
    {
      if (flags & DI_DEFAULTSIZE)
        cyWidth = GetSystemMetrics (SM_CYICON);
      else
	cyWidth = ptr->nHeight;
    }

    DoOffscreen = (GetObjectType( hbr ) == OBJ_BRUSH);

    if (DoOffscreen) {
      RECT r;

      r.left = 0;
      r.top = 0;
      r.right = cxWidth;
      r.bottom = cxWidth;

      hDC_off = CreateCompatibleDC(hdc);
      hB_off = CreateCompatibleBitmap(hdc, cxWidth, cyWidth);
      if (hDC_off && hB_off) {
	hOld = SelectObject(hDC_off, hB_off);
	FillRect(hDC_off, &r, hbr);
      }
    }

    if (hMemDC && (!DoOffscreen || (hDC_off && hB_off)))
    {
	HBITMAP hXorBits, hAndBits;
	COLORREF  oldFg, oldBg;
	INT     nStretchMode;

	nStretchMode = SetStretchBltMode (hdc, STRETCH_DELETESCANS);

	hXorBits = CreateBitmap ( ptr->nWidth, ptr->nHeight,
				    ptr->bPlanes, ptr->bBitsPerPixel,
				    (char *)(ptr + 1)
				    + ptr->nHeight *
				    BITMAP_GetWidthBytes(ptr->nWidth,1) );
	hAndBits = CreateBitmap ( ptr->nWidth, ptr->nHeight,
				    1, 1, (char *)(ptr+1) );
	oldFg = SetTextColor( hdc, RGB(0,0,0) );
	oldBg = SetBkColor( hdc, RGB(255,255,255) );

	if (hXorBits && hAndBits)
	{
	    HBITMAP hBitTemp = SelectObject( hMemDC, hAndBits );
	    if (flags & DI_MASK)
            {
	      if (DoOffscreen)
		StretchBlt (hDC_off, 0, 0, cxWidth, cyWidth,
			      hMemDC, 0, 0, ptr->nWidth, ptr->nHeight, SRCAND);
	      else
	        StretchBlt (hdc, x0, y0, cxWidth, cyWidth,
			      hMemDC, 0, 0, ptr->nWidth, ptr->nHeight, SRCAND);
            }
	    SelectObject( hMemDC, hXorBits );
	    if (flags & DI_IMAGE)
            {
	      if (DoOffscreen)
		StretchBlt (hDC_off, 0, 0, cxWidth, cyWidth,
			  hMemDC, 0, 0, ptr->nWidth, ptr->nHeight, SRCPAINT);
	      else
		StretchBlt (hdc, x0, y0, cxWidth, cyWidth,
			      hMemDC, 0, 0, ptr->nWidth, ptr->nHeight, SRCPAINT);
            }
	    SelectObject( hMemDC, hBitTemp );
	    result = TRUE;
	}

	SetTextColor( hdc, oldFg );
	SetBkColor( hdc, oldBg );
	if (hXorBits) DeleteObject( hXorBits );
	if (hAndBits) DeleteObject( hAndBits );
	SetStretchBltMode (hdc, nStretchMode);
	if (DoOffscreen) {
	  BitBlt(hdc, x0, y0, cxWidth, cyWidth, hDC_off, 0, 0, SRCCOPY);
	  SelectObject(hDC_off, hOld);
	}
    }
    if (hMemDC) DeleteDC( hMemDC );
    if (hDC_off) DeleteDC(hDC_off);
    if (hB_off) DeleteObject(hB_off);
    GlobalUnlock16( hIcon );
    return result;
}

/***********************************************************************
 *           DIB_FixColorsToLoadflags
 *
 * Change color table entries when LR_LOADTRANSPARENT or LR_LOADMAP3DCOLORS
 * are in loadflags
 */
static void DIB_FixColorsToLoadflags(BITMAPINFO * bmi, UINT loadflags, BYTE pix)
{
  int colors;
  COLORREF c_W, c_S, c_F, c_L, c_C;
  int incr,i;
  RGBQUAD *ptr;

  if (bmi->bmiHeader.biBitCount > 8) return;
  if (bmi->bmiHeader.biSize == sizeof(BITMAPINFOHEADER)) incr = 4;
  else if (bmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER)) incr = 3;
  else {
    WARN_(resource)("Wrong bitmap header size!\n");
    return;
  }
  colors = bmi->bmiHeader.biClrUsed;
  if (!colors && (bmi->bmiHeader.biBitCount <= 8))
    colors = 1 << bmi->bmiHeader.biBitCount;
  c_W = GetSysColor(COLOR_WINDOW);
  c_S = GetSysColor(COLOR_3DSHADOW);
  c_F = GetSysColor(COLOR_3DFACE);
  c_L = GetSysColor(COLOR_3DLIGHT);
  if (loadflags & LR_LOADTRANSPARENT) {
    switch (bmi->bmiHeader.biBitCount) {
      case 1: pix = pix >> 7; break;
      case 4: pix = pix >> 4; break;
      case 8: break;
      default:
        WARN_(resource)("(%d): Unsupported depth\n", bmi->bmiHeader.biBitCount);
	return;
    }
    if (pix >= colors) {
      WARN_(resource)("pixel has color index greater than biClrUsed!\n");
      return;
    }
    if (loadflags & LR_LOADMAP3DCOLORS) c_W = c_F;
    ptr = (RGBQUAD*)((char*)bmi->bmiColors+pix*incr);
    ptr->rgbBlue = GetBValue(c_W);
    ptr->rgbGreen = GetGValue(c_W);
    ptr->rgbRed = GetRValue(c_W);
  }
  if (loadflags & LR_LOADMAP3DCOLORS)
    for (i=0; i<colors; i++) {
      ptr = (RGBQUAD*)((char*)bmi->bmiColors+i*incr);
      c_C = RGB(ptr->rgbRed, ptr->rgbGreen, ptr->rgbBlue);
      if (c_C == RGB(128, 128, 128)) {
	ptr->rgbRed = GetRValue(c_S);
	ptr->rgbGreen = GetGValue(c_S);
	ptr->rgbBlue = GetBValue(c_S);
      } else if (c_C == RGB(192, 192, 192)) {
	ptr->rgbRed = GetRValue(c_F);
	ptr->rgbGreen = GetGValue(c_F);
	ptr->rgbBlue = GetBValue(c_F);
      } else if (c_C == RGB(223, 223, 223)) {
	ptr->rgbRed = GetRValue(c_L);
	ptr->rgbGreen = GetGValue(c_L);
	ptr->rgbBlue = GetBValue(c_L);
      }
    }
}


/**********************************************************************
 *       BITMAP_Load
 */
static HBITMAP BITMAP_Load( HINSTANCE instance,LPCWSTR name, UINT loadflags )
{
    HBITMAP hbitmap = 0;
    HRSRC hRsrc;
    HGLOBAL handle;
    char *ptr = NULL;
    BITMAPINFO *info, *fix_info=NULL;
    HGLOBAL hFix;
    int size;

    if (!(loadflags & LR_LOADFROMFILE))
    {
      if (!instance)
      {
          /* OEM bitmap: try to load the resource from user32.dll */
          if (HIWORD(name)) return 0;
          if (!(instance = GetModuleHandleA("user32.dll"))) return 0;
      }
      if (!(hRsrc = FindResourceW( instance, name, RT_BITMAPW ))) return 0;
      if (!(handle = LoadResource( instance, hRsrc ))) return 0;

      if ((info = (BITMAPINFO *)LockResource( handle )) == NULL) return 0;
    }
    else
    {
        if (!(ptr = map_fileW( name ))) return 0;
        info = (BITMAPINFO *)(ptr + sizeof(BITMAPFILEHEADER));
    }
    size = DIB_BitmapInfoSize(info, DIB_RGB_COLORS);
    if ((hFix = GlobalAlloc(0, size))) fix_info=GlobalLock(hFix);
    if (fix_info) {
      BYTE pix;

      memcpy(fix_info, info, size);
      pix = *((LPBYTE)info+DIB_BitmapInfoSize(info, DIB_RGB_COLORS));
      DIB_FixColorsToLoadflags(fix_info, loadflags, pix);
      if (!screen_dc || GetObjectType(screen_dc) != OBJ_DC) screen_dc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
      if (screen_dc)
      {
        char *bits = (char *)info + size;
	if (loadflags & LR_CREATEDIBSECTION) {
          DIBSECTION dib;
	  hbitmap = CreateDIBSection(screen_dc, fix_info, DIB_RGB_COLORS, NULL, 0, 0);
          GetObjectA(hbitmap, sizeof(DIBSECTION), &dib);
          SetDIBits(screen_dc, hbitmap, 0, dib.dsBm.bmHeight, bits, info,
                    DIB_RGB_COLORS);
        }
        else {
          hbitmap = CreateDIBitmap( screen_dc, &fix_info->bmiHeader, CBM_INIT,
                                      bits, fix_info, DIB_RGB_COLORS );
	}
      }
      GlobalUnlock(hFix);
      GlobalFree(hFix);
    }
    if (loadflags & LR_LOADFROMFILE) UnmapViewOfFile( ptr );
    return hbitmap;
}


/***********************************************************************
 *		LoadImage (USER.389)
 *
 */
HANDLE16 WINAPI LoadImage16( HINSTANCE16 hinst, LPCSTR name, UINT16 type,
                             INT16 desiredx, INT16 desiredy, UINT16 loadflags)
{
    return LoadImageA( hinst, name, type, desiredx, desiredy, loadflags );
}

/**********************************************************************
 *		LoadImageA (USER32.@)
 *
 * FIXME: implementation lacks some features, see LR_ defines in winuser.h
 */

/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
	return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

/*********************************************************************/

HANDLE WINAPI LoadImageA( HINSTANCE hinst, LPCSTR name, UINT type,
                              INT desiredx, INT desiredy, UINT loadflags)
{
    HANDLE res;
    LPWSTR u_name;

    if (!HIWORD(name))
        return LoadImageW(hinst, (LPWSTR)name, type, desiredx, desiredy, loadflags);

    __TRY {
        DWORD len = MultiByteToWideChar( CP_ACP, 0, name, -1, NULL, 0 );
        u_name = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, name, -1, u_name, len );
    }
    __EXCEPT(page_fault) {
	SetLastError( ERROR_INVALID_PARAMETER );
	return 0;
    }
    __ENDTRY
    res = LoadImageW(hinst, u_name, type, desiredx, desiredy, loadflags);
    HeapFree(GetProcessHeap(), 0, u_name);
    return res;
}


/******************************************************************************
 *		LoadImageW (USER32.@) Loads an icon, cursor, or bitmap
 *
 * PARAMS
 *    hinst     [I] Handle of instance that contains image
 *    name      [I] Name of image
 *    type      [I] Type of image
 *    desiredx  [I] Desired width
 *    desiredy  [I] Desired height
 *    loadflags [I] Load flags
 *
 * RETURNS
 *    Success: Handle to newly loaded image
 *    Failure: NULL
 *
 * FIXME: Implementation lacks some features, see LR_ defines in winuser.h
 */
HANDLE WINAPI LoadImageW( HINSTANCE hinst, LPCWSTR name, UINT type,
                INT desiredx, INT desiredy, UINT loadflags )
{
    if (HIWORD(name)) {
        TRACE_(resource)("(0x%04x,%p,%d,%d,%d,0x%08x)\n",
	      hinst,name,type,desiredx,desiredy,loadflags);
    } else {
        TRACE_(resource)("(0x%04x,%p,%d,%d,%d,0x%08x)\n",
	      hinst,name,type,desiredx,desiredy,loadflags);
    }
    if (loadflags & LR_DEFAULTSIZE) {
        if (type == IMAGE_ICON) {
	    if (!desiredx) desiredx = GetSystemMetrics(SM_CXICON);
	    if (!desiredy) desiredy = GetSystemMetrics(SM_CYICON);
	} else if (type == IMAGE_CURSOR) {
            if (!desiredx) desiredx = GetSystemMetrics(SM_CXCURSOR);
	    if (!desiredy) desiredy = GetSystemMetrics(SM_CYCURSOR);
	}
    }
    if (loadflags & LR_LOADFROMFILE) loadflags &= ~LR_SHARED;
    switch (type) {
    case IMAGE_BITMAP:
        return BITMAP_Load( hinst, name, loadflags );

    case IMAGE_ICON:
        if (!screen_dc || GetObjectType(screen_dc) != OBJ_DC) screen_dc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
        if (screen_dc)
        {
            UINT palEnts = GetSystemPaletteEntries(screen_dc, 0, 0, NULL);
            if (palEnts == 0) palEnts = 256;
            return CURSORICON_Load(hinst, name, desiredx, desiredy,
                                   palEnts, FALSE, loadflags);
        }
        break;

    case IMAGE_CURSOR:
        return CURSORICON_Load(hinst, name, desiredx, desiredy,
				 1, TRUE, loadflags);
    }
    return 0;
}


/******************************************************************************
 *		CopyImage (USER.390) Creates new image and copies attributes to it
 *
 */
HICON16 WINAPI CopyImage16( HANDLE16 hnd, UINT16 type, INT16 desiredx,
                             INT16 desiredy, UINT16 flags )
{
    return (HICON16)CopyImage((HANDLE)hnd, (UINT)type, (INT)desiredx,
                                (INT)desiredy, (UINT)flags);
}

/******************************************************************************
 *		CopyImage (USER32.@) Creates new image and copies attributes to it
 *
 * PARAMS
 *    hnd      [I] Handle to image to copy
 *    type     [I] Type of image to copy
 *    desiredx [I] Desired width of new image
 *    desiredy [I] Desired height of new image
 *    flags    [I] Copy flags
 *
 * RETURNS
 *    Success: Handle to newly created image
 *    Failure: NULL
 *
 * FIXME: implementation still lacks nearly all features, see LR_*
 * defines in winuser.h
 */
HICON WINAPI CopyImage( HANDLE hnd, UINT type, INT desiredx,
                             INT desiredy, UINT flags )
{
    switch (type)
    {
	case IMAGE_BITMAP:
		return BITMAP_CopyBitmap(hnd);
	case IMAGE_ICON:
		return CURSORICON_ExtCopy(hnd,type, desiredx, desiredy, flags);
	case IMAGE_CURSOR:
		/* Should call CURSORICON_ExtCopy but more testing
		 * needs to be done before we change this
		 */
		return CopyCursor(hnd);
    }
    return 0;
}


/******************************************************************************
 *		LoadBitmapW (USER32.@) Loads bitmap from the executable file
 *
 * RETURNS
 *    Success: Handle to specified bitmap
 *    Failure: NULL
 */
HBITMAP WINAPI LoadBitmapW(
    HINSTANCE instance, /* [in] Handle to application instance */
    LPCWSTR name)         /* [in] Address of bitmap resource name */
{
    return LoadImageW( instance, name, IMAGE_BITMAP, 0, 0, 0 );
}

/**********************************************************************
 *		LoadBitmapA (USER32.@)
 */
HBITMAP WINAPI LoadBitmapA( HINSTANCE instance, LPCSTR name )
{
    return LoadImageA( instance, name, IMAGE_BITMAP, 0, 0, 0 );
}

/**********************************************************************
 *		LoadBitmap (USER.175)
 */
HBITMAP16 WINAPI LoadBitmap16( HINSTANCE16 instance, LPCSTR name )
{
    return LoadBitmapA( instance, name );
}
