/*
 * Static control
 *
 * Copyright  David W. Metcalfe, 1993
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "windef.h"
#include "wingdi.h"
#include "wine/winuser16.h"
#include "cursoricon.h"
#include "controls.h"
#include "user.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(static);

static void STATIC_PaintOwnerDrawfn( HWND hwnd, HDC hdc, DWORD style );
static void STATIC_PaintTextfn( HWND hwnd, HDC hdc, DWORD style );
static void STATIC_PaintRectfn( HWND hwnd, HDC hdc, DWORD style );
static void STATIC_PaintIconfn( HWND hwnd, HDC hdc, DWORD style );
static void STATIC_PaintBitmapfn( HWND hwnd, HDC hdc, DWORD style );
static void STATIC_PaintEtchedfn( HWND hwnd, HDC hdc, DWORD style );
static LRESULT WINAPI StaticWndProcA( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static LRESULT WINAPI StaticWndProcW( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

static COLORREF color_windowframe, color_background, color_window;

/* offsets for GetWindowLong for static private information */
#define HFONT_GWL_OFFSET    0
#define HICON_GWL_OFFSET    (sizeof(HFONT))
#define STATIC_EXTRA_BYTES  (HICON_GWL_OFFSET + sizeof(HICON))

typedef void (*pfPaint)( HWND hwnd, HDC hdc, DWORD style );

static pfPaint staticPaintFunc[SS_TYPEMASK+1] =
{
    STATIC_PaintTextfn,      /* SS_LEFT */
    STATIC_PaintTextfn,      /* SS_CENTER */
    STATIC_PaintTextfn,      /* SS_RIGHT */
    STATIC_PaintIconfn,      /* SS_ICON */
    STATIC_PaintRectfn,      /* SS_BLACKRECT */
    STATIC_PaintRectfn,      /* SS_GRAYRECT */
    STATIC_PaintRectfn,      /* SS_WHITERECT */
    STATIC_PaintRectfn,      /* SS_BLACKFRAME */
    STATIC_PaintRectfn,      /* SS_GRAYFRAME */
    STATIC_PaintRectfn,      /* SS_WHITEFRAME */
    NULL,                    /* Not defined */
    STATIC_PaintTextfn,      /* SS_SIMPLE */
    STATIC_PaintTextfn,      /* SS_LEFTNOWORDWRAP */
    STATIC_PaintOwnerDrawfn, /* SS_OWNERDRAW */
    STATIC_PaintBitmapfn,    /* SS_BITMAP */
    NULL,                    /* SS_ENHMETAFILE */
    STATIC_PaintEtchedfn,    /* SS_ETCHEDHORIZ */
    STATIC_PaintEtchedfn,    /* SS_ETCHEDVERT */
    STATIC_PaintEtchedfn,    /* SS_ETCHEDFRAME */
};


/*********************************************************************
 * static class descriptor
 */
const struct builtin_class_descr STATIC_builtin_class =
{
    "Static",            /* name */
    CS_GLOBALCLASS | CS_DBLCLKS | CS_PARENTDC, /* style  */
    StaticWndProcA,      /* procA */
    StaticWndProcW,      /* procW */
    STATIC_EXTRA_BYTES,  /* extra */
    IDC_ARROWA,          /* cursor */
    0                    /* brush */
};


/***********************************************************************
 *           STATIC_SetIcon
 *
 * Set the icon for an SS_ICON control.
 */
static HICON STATIC_SetIcon( HWND hwnd, HICON hicon, DWORD style )
{
    HICON prevIcon;
    CURSORICONINFO *info = hicon?(CURSORICONINFO *) GlobalLock16( hicon ):NULL;

    if ((style & SS_TYPEMASK) != SS_ICON) return 0;
    if (hicon && !info) {
	ERR("huh? hicon!=0, but info=0???\n");
    	return 0;
    }
    prevIcon = SetWindowLongA( hwnd, HICON_GWL_OFFSET, hicon );
    if (hicon)
    {
        SetWindowPos( hwnd, 0, 0, 0, info->nWidth, info->nHeight,
                        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER );
        GlobalUnlock16( hicon );
    }
    return prevIcon;
}

/***********************************************************************
 *           STATIC_SetBitmap
 *
 * Set the bitmap for an SS_BITMAP control.
 */
static HBITMAP STATIC_SetBitmap( HWND hwnd, HBITMAP hBitmap, DWORD style )
{
    HBITMAP hOldBitmap;

    if ((style & SS_TYPEMASK) != SS_BITMAP) return 0;
    if (hBitmap && GetObjectType(hBitmap) != OBJ_BITMAP) {
	ERR("huh? hBitmap!=0, but not bitmap\n");
    	return 0;
    }
    hOldBitmap = SetWindowLongA( hwnd, HICON_GWL_OFFSET, hBitmap );
    if (hBitmap)
    {
        BITMAP bm;
        GetObjectW(hBitmap, sizeof(bm), &bm);
        SetWindowPos( hwnd, 0, 0, 0, bm.bmWidth, bm.bmHeight,
		      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER );
    }
    return hOldBitmap;
}

/***********************************************************************
 *           STATIC_LoadIconA
 *
 * Load the icon for an SS_ICON control.
 */
static HICON STATIC_LoadIconA( HWND hwnd, LPCSTR name )
{
    HINSTANCE hInstance = GetWindowLongA( hwnd, GWL_HINSTANCE );
    HICON hicon = LoadIconA( hInstance, name );
    if (!hicon) hicon = LoadIconA( 0, name );
    return hicon;
}

/***********************************************************************
 *           STATIC_LoadIconW
 *
 * Load the icon for an SS_ICON control.
 */
static HICON STATIC_LoadIconW( HWND hwnd, LPCWSTR name )
{
    HINSTANCE hInstance = GetWindowLongA( hwnd, GWL_HINSTANCE );
    HICON hicon = LoadIconW( hInstance, name );
    if (!hicon) hicon = LoadIconW( 0, name );
    return hicon;
}

/***********************************************************************
 *           STATIC_LoadBitmapA
 *
 * Load the bitmap for an SS_BITMAP control.
 */
static HBITMAP STATIC_LoadBitmapA( HWND hwnd, LPCSTR name )
{
    HINSTANCE hInstance = GetWindowLongA( hwnd, GWL_HINSTANCE );
    HBITMAP hbitmap = LoadBitmapA( hInstance, name );
    if (!hbitmap)  /* Try OEM icon (FIXME: is this right?) */
        hbitmap = LoadBitmapA( 0, name );
    return hbitmap;
}

/***********************************************************************
 *           STATIC_LoadBitmapW
 *
 * Load the bitmap for an SS_BITMAP control.
 */
static HBITMAP STATIC_LoadBitmapW( HWND hwnd, LPCWSTR name )
{
    HINSTANCE hInstance = GetWindowLongA( hwnd, GWL_HINSTANCE );
    HBITMAP hbitmap = LoadBitmapW( hInstance, name );
    if (!hbitmap)  /* Try OEM icon (FIXME: is this right?) */
        hbitmap = LoadBitmapW( 0, name );
    return hbitmap;
}

/***********************************************************************
 *           STATIC_TryPaintFcn
 *
 * Try to immediately paint the control.
 */
static VOID STATIC_TryPaintFcn(HWND hwnd, LONG full_style)
{
    LONG style = full_style & SS_TYPEMASK;
    RECT rc;

    GetClientRect( hwnd, &rc );
    if (!IsRectEmpty(&rc) && IsWindowVisible(hwnd) && staticPaintFunc[style])
    {
	HDC hdc;
	hdc = GetDC( hwnd );
	(staticPaintFunc[style])( hwnd, hdc, full_style );
	ReleaseDC( hwnd, hdc );
    }
}

/***********************************************************************
 *           StaticWndProc_common
 */
static LRESULT StaticWndProc_common( HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam, BOOL unicode )
{
    LRESULT lResult = 0;
    LONG full_style = GetWindowLongA( hwnd, GWL_STYLE );
    LONG style = full_style & SS_TYPEMASK;

    switch (uMsg)
    {
    case WM_CREATE:
        if (style < 0L || style > SS_TYPEMASK)
        {
            ERR("Unknown style 0x%02lx\n", style );
            return -1;
        }
        /* initialise colours */
        color_windowframe  = GetSysColor(COLOR_WINDOWFRAME);
        color_background   = GetSysColor(COLOR_BACKGROUND);
        color_window       = GetSysColor(COLOR_WINDOW);
        break;

    case WM_NCDESTROY:
        if (style == SS_ICON) {
/*
 * FIXME
 *           DestroyIcon32( STATIC_SetIcon( wndPtr, 0 ) );
 *
 * We don't want to do this yet because DestroyIcon32 is broken. If the icon
 * had already been loaded by the application the last thing we want to do is
 * GlobalFree16 the handle.
 */
            break;
        }
        else return unicode ? DefWindowProcW(hwnd, uMsg, wParam, lParam) :
                              DefWindowProcA(hwnd, uMsg, wParam, lParam);

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (staticPaintFunc[style])
                (staticPaintFunc[style])( hwnd, ps.hdc, full_style );
            EndPaint(hwnd, &ps);
        }
        break;

    case WM_ENABLE:
        InvalidateRect(hwnd, NULL, FALSE);
        break;

    case WM_SYSCOLORCHANGE:
        color_windowframe  = GetSysColor(COLOR_WINDOWFRAME);
        color_background   = GetSysColor(COLOR_BACKGROUND);
        color_window       = GetSysColor(COLOR_WINDOW);
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_NCCREATE:
	if ((TWEAK_WineLook > WIN31_LOOK) && (full_style & SS_SUNKEN))
            SetWindowLongA( hwnd, GWL_EXSTYLE,
                            GetWindowLongA( hwnd, GWL_EXSTYLE ) | WS_EX_STATICEDGE );

	if(unicode)
	    lParam = (LPARAM)(((LPCREATESTRUCTW)lParam)->lpszName);
	else
	    lParam = (LPARAM)(((LPCREATESTRUCTA)lParam)->lpszName);
	/* fall through */
    case WM_SETTEXT:
	switch (style) {
	case SS_ICON:
	{
	    HICON hIcon;
	    if(unicode)
		hIcon = STATIC_LoadIconW(hwnd, (LPCWSTR)lParam);
	    else
		hIcon = STATIC_LoadIconA(hwnd, (LPCSTR)lParam);
            /* FIXME : should we also return the previous hIcon here ??? */
            STATIC_SetIcon(hwnd, hIcon, style);
	    break;
	}
        case SS_BITMAP:
	{
	    HBITMAP hBitmap;
	    if(unicode)
		hBitmap = STATIC_LoadBitmapW(hwnd, (LPCWSTR)lParam);
	    else
		hBitmap = STATIC_LoadBitmapA(hwnd, (LPCSTR)lParam);
            STATIC_SetBitmap(hwnd, hBitmap, style);
	    break;
	}
	case SS_LEFT:
	case SS_CENTER:
	case SS_RIGHT:
	case SS_SIMPLE:
	case SS_LEFTNOWORDWRAP:
        {
	    if (HIWORD(lParam))
	    {
		if(unicode)
		    lResult = DefWindowProcW( hwnd, WM_SETTEXT, wParam, lParam );
		else
		    lResult = DefWindowProcA( hwnd, WM_SETTEXT, wParam, lParam );
	    }
	    if (uMsg == WM_SETTEXT)
		STATIC_TryPaintFcn( hwnd, full_style );
	    break;
	}
	default:
	    if (HIWORD(lParam))
	    {
		if(unicode)
		    lResult = DefWindowProcW( hwnd, WM_SETTEXT, wParam, lParam );
		else
		    lResult = DefWindowProcA( hwnd, WM_SETTEXT, wParam, lParam );
	    }
	    if(uMsg == WM_SETTEXT)
		InvalidateRect(hwnd, NULL, FALSE);
	}
        return 1; /* success. FIXME: check text length */

    case WM_SETFONT:
        if ((style == SS_ICON) || (style == SS_BITMAP)) return 0;
        SetWindowLongA( hwnd, HFONT_GWL_OFFSET, wParam );
        if (!LOWORD(lParam)) break;  /* don't refresh */
	switch (style) {
	case SS_LEFT:
	case SS_CENTER:
	case SS_RIGHT:
	case SS_SIMPLE:
	case SS_LEFTNOWORDWRAP:
            STATIC_TryPaintFcn( hwnd, full_style );
	    break;
	default:
            InvalidateRect( hwnd, NULL, FALSE );
            break;
	}
        break;

    case WM_GETFONT:
        return GetWindowLongA( hwnd, HFONT_GWL_OFFSET );

    case WM_NCHITTEST:
        if (full_style & SS_NOTIFY)
           return HTCLIENT;
        else
           return HTTRANSPARENT;

    case WM_GETDLGCODE:
        return DLGC_STATIC;

    case STM_GETIMAGE:
    case STM_GETICON16:
    case STM_GETICON:
        return GetWindowLongA( hwnd, HICON_GWL_OFFSET );

    case STM_SETIMAGE:
        switch(wParam) {
	case IMAGE_BITMAP:
	    lResult = STATIC_SetBitmap( hwnd, (HBITMAP)lParam, style );
	    break;
	case IMAGE_ICON:
	    lResult = STATIC_SetIcon( hwnd, (HICON)lParam, style );
	    break;
	default:
	    FIXME("STM_SETIMAGE: Unhandled type %x\n", wParam);
	    break;
	}
        InvalidateRect( hwnd, NULL, FALSE );
	break;

    case STM_SETICON16:
    case STM_SETICON:
        lResult = STATIC_SetIcon( hwnd, (HICON)wParam, style );
        InvalidateRect( hwnd, NULL, FALSE );
        break;

    default:
        return unicode ? DefWindowProcW(hwnd, uMsg, wParam, lParam) :
                         DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return lResult;
}

/***********************************************************************
 *           StaticWndProcA
 */
static LRESULT WINAPI StaticWndProcA( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    if (!IsWindow( hWnd )) return 0;
    return StaticWndProc_common(hWnd, uMsg, wParam, lParam, FALSE);
}

/***********************************************************************
 *           StaticWndProcW
 */
static LRESULT WINAPI StaticWndProcW( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    if (!IsWindow( hWnd )) return 0;
    return StaticWndProc_common(hWnd, uMsg, wParam, lParam, TRUE);
}

static void STATIC_PaintOwnerDrawfn( HWND hwnd, HDC hdc, DWORD style )
{
  DRAWITEMSTRUCT dis;
  LONG id = GetWindowLongA( hwnd, GWL_ID );

  dis.CtlType    = ODT_STATIC;
  dis.CtlID      = id;
  dis.itemID     = 0;
  dis.itemAction = ODA_DRAWENTIRE;
  dis.itemState  = 0;
  dis.hwndItem   = hwnd;
  dis.hDC        = hdc;
  dis.itemData   = 0;
  GetClientRect( hwnd, &dis.rcItem );

  SendMessageW( GetParent(hwnd), WM_CTLCOLORSTATIC, hdc, hwnd );
  SendMessageW( GetParent(hwnd), WM_DRAWITEM, id, (LPARAM)&dis );
}

static void STATIC_PaintTextfn( HWND hwnd, HDC hdc, DWORD style )
{
    RECT rc;
    HBRUSH hBrush;
    HFONT hFont;
    WORD wFormat;
    INT len;
    WCHAR *text;

    GetClientRect( hwnd, &rc);

    switch (style & SS_TYPEMASK)
    {
    case SS_LEFT:
	wFormat = DT_LEFT | DT_EXPANDTABS | DT_WORDBREAK | DT_NOCLIP;
	break;

    case SS_CENTER:
	wFormat = DT_CENTER | DT_EXPANDTABS | DT_WORDBREAK | DT_NOCLIP;
	break;

    case SS_RIGHT:
	wFormat = DT_RIGHT | DT_EXPANDTABS | DT_WORDBREAK | DT_NOCLIP;
	break;

    case SS_SIMPLE:
	wFormat = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOCLIP;
	break;

    case SS_LEFTNOWORDWRAP:
	wFormat = DT_LEFT | DT_EXPANDTABS | DT_VCENTER;
	break;

    default:
        return;
    }

    if (style & SS_NOPREFIX)
	wFormat |= DT_NOPREFIX;

    if ((hFont = GetWindowLongA( hwnd, HFONT_GWL_OFFSET ))) SelectObject( hdc, hFont );

    if ((style & SS_NOPREFIX) || ((style & SS_TYPEMASK) != SS_SIMPLE))
    {
        hBrush = SendMessageW( GetParent(hwnd), WM_CTLCOLORSTATIC, hdc, hwnd );
        if (!hBrush) /* did the app forget to call defwindowproc ? */
            hBrush = DefWindowProcW(GetParent(hwnd), WM_CTLCOLORSTATIC, hdc, hwnd);
        FillRect( hdc, &rc, hBrush );
    }
    if (!IsWindowEnabled(hwnd)) SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));

    if (!(len = SendMessageW( hwnd, WM_GETTEXTLENGTH, 0, 0 ))) return;
    if (!(text = HeapAlloc( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) ))) return;
    SendMessageW( hwnd, WM_GETTEXT, len + 1, (LPARAM)text );
    DrawTextW( hdc, text, -1, &rc, wFormat );
    HeapFree( GetProcessHeap(), 0, text );
}

static void STATIC_PaintRectfn( HWND hwnd, HDC hdc, DWORD style )
{
    RECT rc;
    HBRUSH hBrush;

    GetClientRect( hwnd, &rc);

    switch (style & SS_TYPEMASK)
    {
    case SS_BLACKRECT:
	hBrush = CreateSolidBrush(color_windowframe);
        FillRect( hdc, &rc, hBrush );
	break;
    case SS_GRAYRECT:
	hBrush = CreateSolidBrush(color_background);
        FillRect( hdc, &rc, hBrush );
	break;
    case SS_WHITERECT:
	hBrush = CreateSolidBrush(color_window);
        FillRect( hdc, &rc, hBrush );
	break;
    case SS_BLACKFRAME:
	hBrush = CreateSolidBrush(color_windowframe);
        FrameRect( hdc, &rc, hBrush );
	break;
    case SS_GRAYFRAME:
	hBrush = CreateSolidBrush(color_background);
        FrameRect( hdc, &rc, hBrush );
	break;
    case SS_WHITEFRAME:
	hBrush = CreateSolidBrush(color_window);
        FrameRect( hdc, &rc, hBrush );
	break;
    default:
        return;
    }
    DeleteObject( hBrush );
}


static void STATIC_PaintIconfn( HWND hwnd, HDC hdc, DWORD style )
{
    RECT rc;
    HBRUSH hbrush;
    HICON hIcon;

    GetClientRect( hwnd, &rc );
    hbrush = SendMessageW( GetParent(hwnd), WM_CTLCOLORSTATIC, hdc, hwnd );
    FillRect( hdc, &rc, hbrush );
    if ((hIcon = GetWindowLongA( hwnd, HICON_GWL_OFFSET )))
        DrawIcon( hdc, rc.left, rc.top, hIcon );
}

static void STATIC_PaintBitmapfn(HWND hwnd, HDC hdc, DWORD style )
{
    RECT rc;
    HBRUSH hbrush;
    HICON hIcon;
    HDC hMemDC;
    HBITMAP oldbitmap;

    GetClientRect( hwnd, &rc );
    hbrush = SendMessageW( GetParent(hwnd), WM_CTLCOLORSTATIC, hdc, hwnd );
    FillRect( hdc, &rc, hbrush );

    if ((hIcon = GetWindowLongA( hwnd, HICON_GWL_OFFSET )))
    {
        BITMAP bm;
	SIZE sz;

        if(GetObjectType(hIcon) != OBJ_BITMAP) return;
        if (!(hMemDC = CreateCompatibleDC( hdc ))) return;
	GetObjectW(hIcon, sizeof(bm), &bm);
	GetBitmapDimensionEx(hIcon, &sz);
	oldbitmap = SelectObject(hMemDC, hIcon);
	BitBlt(hdc, sz.cx, sz.cy, bm.bmWidth, bm.bmHeight, hMemDC, 0, 0,
	       SRCCOPY);
	SelectObject(hMemDC, oldbitmap);
	DeleteDC(hMemDC);
    }
}


static void STATIC_PaintEtchedfn( HWND hwnd, HDC hdc, DWORD style )
{
    RECT rc;

    if (TWEAK_WineLook == WIN31_LOOK)
	return;

    GetClientRect( hwnd, &rc );
    switch (style & SS_TYPEMASK)
    {
	case SS_ETCHEDHORZ:
	    DrawEdge(hdc,&rc,EDGE_ETCHED,BF_TOP|BF_BOTTOM);
	    break;
	case SS_ETCHEDVERT:
	    DrawEdge(hdc,&rc,EDGE_ETCHED,BF_LEFT|BF_RIGHT);
	    break;
	case SS_ETCHEDFRAME:
	    DrawEdge (hdc, &rc, EDGE_ETCHED, BF_RECT);
	    break;
    }
}
