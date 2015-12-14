/*
 * Hotkey control
 *
 * Copyright 1998, 1999 Eric Kohl
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2011-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NOTES
 *   Development in progress. An author is needed! Any volunteers?
 *   I will only improve this control once in a while.
 *     Eric <ekohl@abo.rhein-zeitung.de>
 *
 * TODO:
 *   - Some messages.
 *   - Display code.
 */

#include <string.h>
#include "winbase.h"
#include "commctrl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(hotkey);

typedef struct tagHOTKEY_INFO
{
    HFONT hFont;
    BOOL  bFocus;
    INT   nHeight;
} HOTKEY_INFO;

#define HOTKEY_GetInfoPtr(hwnd) ((HOTKEY_INFO *)GetWindowLongA (hwnd, 0))


/* << HOTHEY_GetHotKey >> */
/* << HOTHEY_SetHotKey >> */
/* << HOTHEY_SetRules >> */



/* << HOTKEY_Char >> */


static LRESULT
HOTKEY_Create (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HOTKEY_INFO *infoPtr;
    TEXTMETRICA tm;
    HDC hdc;

    /* allocate memory for info structure */
    infoPtr = (HOTKEY_INFO *)COMCTL32_Alloc (sizeof(HOTKEY_INFO));
    SetWindowLongA (hwnd, 0, (DWORD)infoPtr);

    /* initialize info structure */


    /* get default font height */
    hdc = GetDC (hwnd);
    GetTextMetricsA (hdc, &tm);
    infoPtr->nHeight = tm.tmHeight;
    ReleaseDC (hwnd, hdc);

    return 0;
}


static LRESULT
HOTKEY_Destroy (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd);



    /* free hotkey info data */
    COMCTL32_Free (infoPtr);
    SetWindowLongA (hwnd, 0, 0);
    return 0;
}


static LRESULT
HOTKEY_EraseBackground (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /* HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd); */
    HBRUSH hBrush;
    RECT   rc;

    hBrush =
	(HBRUSH)SendMessageA (GetParent (hwnd), WM_CTLCOLOREDIT,
				wParam, (LPARAM)hwnd);
    if (hBrush)
	hBrush = (HBRUSH)GetStockObject (WHITE_BRUSH);
    GetClientRect (hwnd, &rc);

    FillRect ((HDC)wParam, &rc, hBrush);

    return -1;
}


inline static LRESULT
HOTKEY_GetFont (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd);

    return infoPtr->hFont;
}


static LRESULT
HOTKEY_KeyDown (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /* HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd); */

    switch (wParam) {
	case VK_RETURN:
	case VK_TAB:
	case VK_SPACE:
	case VK_DELETE:
	case VK_ESCAPE:
	case VK_BACK:
	    return DefWindowProcA (hwnd, WM_KEYDOWN, wParam, lParam);

	case VK_SHIFT:
	case VK_CONTROL:
	case VK_MENU:
	    FIXME("modifier key pressed!\n");
	    break;

	default:
	    FIXME(" %d\n", wParam);
	    break;
    }

    return TRUE;
}


static LRESULT
HOTKEY_KeyUp (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /* HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd); */

    FIXME(" %d\n", wParam);

    return 0;
}


static LRESULT
HOTKEY_KillFocus (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd);

    infoPtr->bFocus = FALSE;
    DestroyCaret ();

    return 0;
}


static LRESULT
HOTKEY_LButtonDown (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
/*    HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd); */

    SetFocus (hwnd);

    return 0;
}


inline static LRESULT
HOTKEY_NCCreate (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    DWORD dwExStyle = GetWindowLongA (hwnd, GWL_EXSTYLE);
    SetWindowLongA (hwnd, GWL_EXSTYLE, dwExStyle | WS_EX_CLIENTEDGE);
    return DefWindowProcA (hwnd, WM_NCCREATE, wParam, lParam);
}





static LRESULT
HOTKEY_SetFocus (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd);

    infoPtr->bFocus = TRUE;


    CreateCaret (hwnd, (HBITMAP)0, 1, infoPtr->nHeight);

    SetCaretPos (1, 1);

    ShowCaret (hwnd);


    return 0;
}


inline static LRESULT
HOTKEY_SetFont (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd);
    TEXTMETRICA tm;
    HDC hdc;
    HFONT hOldFont = 0;

    infoPtr->hFont = (HFONT)wParam;

    hdc = GetDC (hwnd);
    if (infoPtr->hFont)
	hOldFont = SelectObject (hdc, infoPtr->hFont);

    GetTextMetricsA (hdc, &tm);
    infoPtr->nHeight = tm.tmHeight;

    if (infoPtr->hFont)
	SelectObject (hdc, hOldFont);
    ReleaseDC (hwnd, hdc);

    if (LOWORD(lParam)) {

	FIXME("force redraw!\n");

    }

    return 0;
}


static LRESULT WINE_UNUSED
HOTKEY_SysKeyDown (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /* HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd); */

    switch (wParam) {
	case VK_RETURN:
	case VK_TAB:
	case VK_SPACE:
	case VK_DELETE:
	case VK_ESCAPE:
	case VK_BACK:
	    return DefWindowProcA (hwnd, WM_SYSKEYDOWN, wParam, lParam);

	case VK_SHIFT:
	case VK_CONTROL:
	case VK_MENU:
	    FIXME("modifier key pressed!\n");
	    break;

	default:
	    FIXME(" %d\n", wParam);
	    break;
    }

    return TRUE;
}


static LRESULT WINE_UNUSED
HOTKEY_SysKeyUp (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /* HOTKEY_INFO *infoPtr = HOTKEY_GetInfoPtr (hwnd); */

    FIXME(" %d\n", wParam);

    return 0;
}



static LRESULT WINAPI
HOTKEY_WindowProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TRACE("hwnd=%x msg=%x wparam=%x lparam=%lx\n", hwnd, uMsg, wParam, lParam);
    if (!HOTKEY_GetInfoPtr (hwnd) && (uMsg != WM_CREATE))
        return DefWindowProcA (hwnd, uMsg, wParam, lParam);
    switch (uMsg)
    {
/*	case HKM_GETHOTKEY: */
/*	case HKM_SETHOTKEY: */
/*	case HKM_SETRULES: */

/*	case WM_CHAR: */

	case WM_CREATE:
	    return HOTKEY_Create (hwnd, wParam, lParam);

	case WM_DESTROY:
	    return HOTKEY_Destroy (hwnd, wParam, lParam);

	case WM_ERASEBKGND:
	    return HOTKEY_EraseBackground (hwnd, wParam, lParam);

	case WM_GETDLGCODE:
	    return DLGC_WANTCHARS | DLGC_WANTARROWS;

	case WM_GETFONT:
	    return HOTKEY_GetFont (hwnd, wParam, lParam);

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	    return HOTKEY_KeyDown (hwnd, wParam, lParam);

	case WM_KEYUP:
	case WM_SYSKEYUP:
	    return HOTKEY_KeyUp (hwnd, wParam, lParam);

	case WM_KILLFOCUS:
	    return HOTKEY_KillFocus (hwnd, wParam, lParam);

	case WM_LBUTTONDOWN:
	    return HOTKEY_LButtonDown (hwnd, wParam, lParam);

	case WM_NCCREATE:
	    return HOTKEY_NCCreate (hwnd, wParam, lParam);

/*	case WM_PAINT: */

	case WM_SETFOCUS:
	    return HOTKEY_SetFocus (hwnd, wParam, lParam);

	case WM_SETFONT:
	    return HOTKEY_SetFont (hwnd, wParam, lParam);

/*	case WM_SYSCHAR: */

	default:
	    if (uMsg >= WM_USER)
		ERR("unknown msg %04x wp=%08x lp=%08lx\n",
		     uMsg, wParam, lParam);
	    return DefWindowProcA (hwnd, uMsg, wParam, lParam);
    }
    return 0;
}


VOID
HOTKEY_Register (void)
{
    WNDCLASSA wndClass;

    ZeroMemory (&wndClass, sizeof(WNDCLASSA));
    wndClass.style         = CS_GLOBALCLASS;
    wndClass.lpfnWndProc   = (WNDPROC)HOTKEY_WindowProc;
    wndClass.cbClsExtra    = 0;
    wndClass.cbWndExtra    = sizeof(HOTKEY_INFO *);
    wndClass.hCursor       = 0;
    wndClass.hbrBackground = 0;
    wndClass.lpszClassName = HOTKEY_CLASSA;

    RegisterClassA (&wndClass);
}


VOID
HOTKEY_Unregister (void)
{
    UnregisterClassA (HOTKEY_CLASSA, (HINSTANCE)NULL);
}

