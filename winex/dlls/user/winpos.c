/*
 * Window position related functions.
 *
 * Copyright 1993, 1994, 1995 Alexandre Julliard
 *                       1995, 1996, 1999 Alex Korobka
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <string.h>
#include "winerror.h"
#include "windef.h"
#include "wingdi.h"
#include "winerror.h"
#include "wine/winuser16.h"
#include "wine/server.h"
#include "controls.h"
#include "user.h"
#include "region.h"
#include "win.h"
#include "hook.h"
#include "message.h"
#include "queue.h"
#include "winpos.h"
#include "dce.h"
#include "nonclient.h"
#include "wine/debug.h"
#include "input.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

#define HAS_DLGFRAME(style,exStyle) \
    (((exStyle) & WS_EX_DLGMODALFRAME) || \
     (((style) & WS_DLGFRAME) && !((style) & WS_BORDER)))

#define HAS_THICKFRAME(style) \
    (((style) & WS_THICKFRAME) && \
     !(((style) & (WS_DLGFRAME|WS_BORDER)) == WS_DLGFRAME))

#define EMPTYPOINT(pt)          ((*(LONG*)&(pt)) == -1)

#define PLACE_MIN		0x0001
#define PLACE_MAX		0x0002
#define PLACE_RECT		0x0004


#define DWP_MAGIC  ((INT)('W' | ('P' << 8) | ('O' << 16) | ('S' << 24)))

typedef struct
{
    INT       actualCount;
    INT       suggestedCount;
    BOOL      valid;
    INT       wMagic;
    HWND      hwndParent;
    WINDOWPOS winPos[1];
} DWP;

/* ----- internal variables ----- */

static HWND hwndPrevActive  = 0;  /* Previously active window */
static HWND hGlobalShellWindow=0; /*the shell*/
static HWND hGlobalTaskmanWindow=0;
static HWND hGlobalProgmanWindow=0;

static LPCSTR atomInternalPos;

extern HQUEUE16 hActiveQueue;

/***********************************************************************
 *           WINPOS_CreateInternalPosAtom
 */
BOOL WINPOS_CreateInternalPosAtom()
{
    LPSTR str = "SysIP";
    atomInternalPos = (LPCSTR)(DWORD)GlobalAddAtomA(str);
    return (atomInternalPos) ? TRUE : FALSE;
}

/***********************************************************************
 *           WINPOS_CheckInternalPos
 *
 * Called when a window is destroyed.
 */
void WINPOS_CheckInternalPos( HWND hwnd )
{
    LPINTERNALPOS lpPos;
    MESSAGEQUEUE *pMsgQ = 0;
    WND *wndPtr = WIN_GetPtr( hwnd );

    if (!wndPtr || wndPtr == WND_OTHER_PROCESS) return;

    lpPos = (LPINTERNALPOS) GetPropA( hwnd, atomInternalPos );

    /* Retrieve the message queue associated with this window */
    pMsgQ = (MESSAGEQUEUE *)QUEUE_Lock( wndPtr->hmemTaskQ );
    if ( !pMsgQ )
    {
        WARN("\tMessage queue not found. Exiting!\n" );
        WIN_ReleasePtr( wndPtr );
        return;
    }

    if( hwnd == hwndPrevActive ) hwndPrevActive = 0;

    if( hwnd == PERQDATA_GetActiveWnd( pMsgQ->pQData ) )
    {
        PERQDATA_SetActiveWnd( pMsgQ->pQData, 0 );
        if (hActiveQueue == wndPtr->hmemTaskQ)
            hActiveQueue = 0;
	WARN("\tattempt to activate destroyed window!\n");
    }

    if( lpPos )
    {
	if( IsWindow(lpPos->hwndIconTitle) )
	    DestroyWindow( lpPos->hwndIconTitle );
	HeapFree( GetProcessHeap(), 0, lpPos );
    }

    QUEUE_Unlock( pMsgQ );
    WIN_ReleasePtr( wndPtr );
}

/***********************************************************************
 *		ArrangeIconicWindows (USER32.@)
 */
UINT WINAPI ArrangeIconicWindows( HWND parent )
{
    RECT rectParent;
    HWND hwndChild;
    INT x, y, xspacing, yspacing;

    GetClientRect( parent, &rectParent );
    x = rectParent.left;
    y = rectParent.bottom;
    xspacing = GetSystemMetrics(SM_CXICONSPACING);
    yspacing = GetSystemMetrics(SM_CYICONSPACING);

    hwndChild = GetWindow( parent, GW_CHILD );
    while (hwndChild)
    {
        if( IsIconic( hwndChild ) )
        {
            WINPOS_ShowIconTitle( hwndChild, FALSE );

            SetWindowPos( hwndChild, 0, x + (xspacing - GetSystemMetrics(SM_CXICON)) / 2,
                            y - yspacing - GetSystemMetrics(SM_CYICON)/2, 0, 0,
                            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	    if( IsWindow(hwndChild) )
                WINPOS_ShowIconTitle(hwndChild , TRUE );

            if (x <= rectParent.right - xspacing) x += xspacing;
            else
            {
                x = rectParent.left;
                y -= yspacing;
            }
        }
        hwndChild = GetWindow( hwndChild, GW_HWNDNEXT );
    }
    return yspacing;
}


/***********************************************************************
 *		SwitchToThisWindow (USER32.@)
 */
void WINAPI SwitchToThisWindow( HWND hwnd, BOOL restore )
{
    ShowWindow( hwnd, restore ? SW_RESTORE : SW_SHOWMINIMIZED );
}


/***********************************************************************
 *		GetWindowRect (USER32.@)
 */
BOOL WINAPI GetWindowRect( HWND hwnd, LPRECT rect )
{
    BOOL ret = WIN_GetRectangles( hwnd, rect, NULL );
    if (ret)
    {
        MapWindowPoints( GetAncestor( hwnd, GA_PARENT ), 0, (POINT *)rect, 2 );
        TRACE( "hwnd %04x (%d,%d)-(%d,%d)\n",
               hwnd, rect->left, rect->top, rect->right, rect->bottom);
    }
    return ret;
}


/***********************************************************************
 *		GetWindowRgn (USER32.@)
 */
int WINAPI GetWindowRgn ( HWND hwnd, HRGN hrgn )
{
    int nRet = ERROR;
    WND *wndPtr = WIN_GetPtr( hwnd );

    if (wndPtr == WND_OTHER_PROCESS)
    {
        if (IsWindow( hwnd ))
            FIXME( "not supported on other process window %x\n", hwnd );
        wndPtr = NULL;
    }
    if (!wndPtr)
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return ERROR;
    }
    if (wndPtr->hrgnWnd) nRet = CombineRgn( hrgn, wndPtr->hrgnWnd, 0, RGN_COPY );
    WIN_ReleasePtr( wndPtr );
    return nRet;
}


/***********************************************************************
 *		SetWindowRgn (USER32.@)
 */
int WINAPI SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL bRedraw )
{
    RECT rect;
    WND *wndPtr;

    if (hrgn) /* verify that region really exists */
    {
        if (GetRgnBox( hrgn, &rect ) == ERROR) return FALSE;
    }

    if (USER_Driver.pSetWindowRgn)
        return USER_Driver.pSetWindowRgn( hwnd, hrgn, bRedraw );

    if ((wndPtr = WIN_GetPtr( hwnd )) == WND_OTHER_PROCESS)
    {
        if (IsWindow( hwnd ))
            FIXME( "not supported on other process window %x\n", hwnd );
        wndPtr = NULL;
    }
    if (!wndPtr)
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return FALSE;
    }

    if (wndPtr->hrgnWnd == hrgn)
    {
        WIN_ReleasePtr( wndPtr );
        return TRUE;
    }

    if (wndPtr->hrgnWnd)
    {
        /* delete previous region */
        DeleteObject(wndPtr->hrgnWnd);
        wndPtr->hrgnWnd = 0;
    }
    wndPtr->hrgnWnd = hrgn;
    WIN_ReleasePtr( wndPtr );

    /* Size the window to the rectangle of the new region (if it isn't NULL) */
    if (hrgn) SetWindowPos( hwnd, 0, rect.left, rect.top,
                            rect.right  - rect.left, rect.bottom - rect.top,
                            SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOACTIVATE |
                            SWP_NOZORDER | (bRedraw ? 0 : SWP_NOREDRAW) );
    return TRUE;
}


/***********************************************************************
 *		GetClientRect (USER32.@)
 */
BOOL WINAPI GetClientRect( HWND hwnd, LPRECT rect )
{
    BOOL ret;

    rect->right = rect->bottom = 0;
    if ((ret = WIN_GetRectangles( hwnd, NULL, rect )))
    {
        rect->right -= rect->left;
        rect->bottom -= rect->top;
    }
    rect->left = rect->top = 0;
    return ret;
}


/*******************************************************************
 *		ClientToScreen (USER32.@)
 */
BOOL WINAPI ClientToScreen( HWND hwnd, LPPOINT lppnt )
{
    MapWindowPoints( hwnd, 0, lppnt, 1 );
    return TRUE;
}


/*******************************************************************
 *		ScreenToClient (USER32.@)
 */
BOOL WINAPI ScreenToClient( HWND hwnd, LPPOINT lppnt )
{
    MapWindowPoints( 0, hwnd, lppnt, 1 );
    return TRUE;
}


/***********************************************************************
 *           find_child_from_point
 *
 * Find the child that contains pt. Helper for WindowFromPoint.
 * pt is in parent client coordinates.
 * lparam is the param to pass in the WM_NCHITTEST message.
 */
static HWND find_child_from_point( HWND parent, POINT pt, INT *hittest, LPARAM lparam )
{
    int i, res;
    LONG style, exstyle;
    RECT rectWindow, rectClient;
    WND *wndPtr;
    HWND *list = WIN_ListChildren( parent );
    HWND retvalue = 0;

    if (!list) return 0;
    for (i = 0; list[i]; i++)
    {
        /* If point is in window, and window is visible, and it  */
        /* is enabled (or it's a top-level window), then explore */
        /* its children. Otherwise, go to the next window.       */

        style = GetWindowLongW( list[i], GWL_STYLE );
        if (!(style & WS_VISIBLE)) continue;  /* not visible -> skip */
        if ((style & (WS_POPUP | WS_CHILD | WS_DISABLED)) == (WS_CHILD | WS_DISABLED))
            continue;  /* disabled child -> skip */
        exstyle = GetWindowLongW( list[i], GWL_EXSTYLE );
        if ((exstyle & (WS_EX_LAYERED | WS_EX_TRANSPARENT)) == (WS_EX_LAYERED | WS_EX_TRANSPARENT))
            continue;  /* transparent -> skip */

        if (!WIN_GetRectangles( list[i], &rectWindow, &rectClient )) continue;
        if (!PtInRect( &rectWindow, pt )) continue;  /* not in window -> skip */

        /* FIXME: check window region for other processes too */
        if ((wndPtr = WIN_GetPtr( list[i] )) && wndPtr != WND_OTHER_PROCESS)
        {
            if (wndPtr->hrgnWnd && !PtInRegion( wndPtr->hrgnWnd,
                                                pt.x - rectWindow.left, pt.y - rectWindow.top ))
            {
                WIN_ReleasePtr( wndPtr );
                continue;  /* point outside window region -> skip */
            }
            WIN_ReleasePtr( wndPtr );
        }

        /* If window is minimized or disabled, return at once */
        if (style & WS_MINIMIZE)
        {
            *hittest = HTCAPTION;
            retvalue = list[i];
            break;
        }
        if (style & WS_DISABLED)
        {
            *hittest = HTERROR;
            retvalue = list[i];
            break;
        }

        /* If point is in client area, explore children */
        if (PtInRect( &rectClient, pt ))
        {
            POINT new_pt;

            new_pt.x = pt.x - rectClient.left;
            new_pt.y = pt.y - rectClient.top;
            if ((retvalue = find_child_from_point( list[i], new_pt, hittest, lparam ))) break;
        }

        /* Now it's inside window, send WM_NCCHITTEST (if same thread) */
        if (!WIN_IsCurrentThread( list[i] ))
        {
            *hittest = HTCLIENT;
            retvalue = list[i];
            break;
        }
        if ((res = SendMessageA( list[i], WM_NCHITTEST, 0, lparam )) != HTTRANSPARENT)
        {
            *hittest = res;  /* Found the window */
            retvalue = list[i];
            break;
        }
        /* continue search with next sibling */
    }
    HeapFree( GetProcessHeap(), 0, list );
    return retvalue;
}


/***********************************************************************
 *           WINPOS_WindowFromPoint
 *
 * Find the window and hittest for a given point.
 */
HWND WINPOS_WindowFromPoint( HWND hwndScope, POINT pt, INT *hittest )
{
    POINT xy = pt;
    int res;
    LONG style;

    TRACE("scope %04x %ld,%ld\n", hwndScope, pt.x, pt.y);

    if (!hwndScope) hwndScope = GetDesktopWindow();
    style = GetWindowLongW( hwndScope, GWL_STYLE );

    *hittest = HTERROR;
    if (style & WS_DISABLED) return 0;

    MapWindowPoints( GetDesktopWindow(), GetAncestor( hwndScope, GA_PARENT ), &xy, 1 );

    if (!(style & WS_MINIMIZE))
    {
        RECT rectClient;
        if (WIN_GetRectangles( hwndScope, NULL, &rectClient ) && PtInRect( &rectClient, xy ))
        {
            HWND ret;

            xy.x -= rectClient.left;
            xy.y -= rectClient.top;
            if ((ret = find_child_from_point( hwndScope, xy, hittest, MAKELONG( pt.x, pt.y ) )))
            {
                TRACE( "found child %x\n", ret );
                return ret;
            }
        }
    }

    /* If nothing found, try the scope window */
    if (!WIN_IsCurrentThread( hwndScope ))
    {
        *hittest = HTCLIENT;
        TRACE( "returning %x\n", hwndScope );
        return hwndScope;
    }
    res = SendMessageA( hwndScope, WM_NCHITTEST, 0, MAKELONG( pt.x, pt.y ) );
    if (res != HTTRANSPARENT)
    {
        *hittest = res;  /* Found the window */
        TRACE( "returning %x\n", hwndScope );
        return hwndScope;
    }
    *hittest = HTNOWHERE;
    TRACE( "nothing found\n" );
    return 0;
}


/*******************************************************************
 *		WindowFromPoint (USER32.@)
 */
HWND WINAPI WindowFromPoint( POINT pt )
{
    INT hittest;
    return WINPOS_WindowFromPoint( 0, pt, &hittest );
}


/*******************************************************************
 *		ChildWindowFromPoint (USER32.@)
 */
HWND WINAPI ChildWindowFromPoint( HWND hwndParent, POINT pt )
{
    return ChildWindowFromPointEx( hwndParent, pt, CWP_ALL );
}

/*******************************************************************
 *		ChildWindowFromPointEx (USER32.@)
 */
HWND WINAPI ChildWindowFromPointEx( HWND hwndParent, POINT pt, UINT uFlags)
{
    /* pt is in the client coordinates */
    HWND *list;
    int i;
    RECT rect;
    HWND retvalue;

    GetClientRect( hwndParent, &rect );
    if (!PtInRect( &rect, pt )) return 0;
    if (!(list = WIN_ListChildren( hwndParent ))) return 0;

    for (i = 0; list[i]; i++)
    {
        if (!WIN_GetRectangles( list[i], &rect, NULL )) continue;
        if (!PtInRect( &rect, pt )) continue;
        if (uFlags & (CWP_SKIPINVISIBLE|CWP_SKIPDISABLED))
        {
            LONG style = GetWindowLongW( list[i], GWL_STYLE );
            if ((uFlags & CWP_SKIPINVISIBLE) && !(style & WS_VISIBLE)) continue;
            if ((uFlags & CWP_SKIPDISABLED) && (style & WS_DISABLED)) continue;
        }
        if (uFlags & CWP_SKIPTRANSPARENT)
        {
            if (GetWindowLongW( list[i], GWL_EXSTYLE ) & WS_EX_TRANSPARENT) continue;
        }
        break;
    }
    retvalue = list[i];
    HeapFree( GetProcessHeap(), 0, list );
    if (!retvalue) retvalue = hwndParent;
    return retvalue;
}


/*******************************************************************
 *         WINPOS_GetWinOffset
 *
 * Calculate the offset between the origin of the two windows. Used
 * to implement MapWindowPoints.
 */
static void WINPOS_GetWinOffset( HWND hwndFrom, HWND hwndTo, POINT *offset )
{
    WND * wndPtr;

    offset->x = offset->y = 0;

    /* Translate source window origin to screen coords */
    if (hwndFrom)
    {
        HWND hwnd = hwndFrom;

        while (hwnd)
        {
            if (hwnd == hwndTo) return;
            if (!(wndPtr = WIN_GetPtr( hwnd )))
            {
                ERR( "bad hwndFrom = %04x\n", hwnd );
                return;
            }
            if (wndPtr == WND_OTHER_PROCESS) goto other_process;
            offset->x += wndPtr->rectClient.left;
            offset->y += wndPtr->rectClient.top;
            hwnd = wndPtr->parent;
            WIN_ReleasePtr( wndPtr );
        }
    }

    /* Translate origin to destination window coords */
    if (hwndTo)
    {
        HWND hwnd = hwndTo;

        while (hwnd)
        {
            if (!(wndPtr = WIN_GetPtr( hwnd )))
            {
                ERR( "bad hwndTo = %04x\n", hwnd );
                return;
            }
            if (wndPtr == WND_OTHER_PROCESS) goto other_process;
            offset->x -= wndPtr->rectClient.left;
            offset->y -= wndPtr->rectClient.top;
            hwnd = wndPtr->parent;
            WIN_ReleasePtr( wndPtr );
        }
    }
    return;

 other_process:  /* one of the parents may belong to another process, do it the hard way */
    offset->x = offset->y = 0;
    SERVER_START_REQ( get_windows_offset )
    {
        req->from = hwndFrom;
        req->to   = hwndTo;
        if (!wine_server_call( req ))
        {
            offset->x = reply->x;
            offset->y = reply->y;
        }
    }
    SERVER_END_REQ;
}


/*******************************************************************
 *		MapWindowPoints (USER.258)
 */
void WINAPI MapWindowPoints16( HWND16 hwndFrom, HWND16 hwndTo,
                               LPPOINT16 lppt, UINT16 count )
{
    POINT offset;

    WINPOS_GetWinOffset( WIN_Handle32(hwndFrom), WIN_Handle32(hwndTo), &offset );
    while (count--)
    {
	lppt->x += offset.x;
	lppt->y += offset.y;
        lppt++;
    }
}


/*******************************************************************
 *		MapWindowPoints (USER32.@)
 */
INT WINAPI MapWindowPoints( HWND hwndFrom, HWND hwndTo, LPPOINT lppt, UINT count )
{
    POINT offset;

    WINPOS_GetWinOffset( hwndFrom, hwndTo, &offset );
    while (count--)
    {
	lppt->x += offset.x;
	lppt->y += offset.y;
        lppt++;
    }
    return MAKELONG( LOWORD(offset.x), LOWORD(offset.y) );
}


/***********************************************************************
 *		IsIconic (USER32.@)
 */
BOOL WINAPI IsIconic(HWND hWnd)
{
    return (GetWindowLongW( hWnd, GWL_STYLE ) & WS_MINIMIZE) != 0;
}


/***********************************************************************
 *		IsZoomed (USER32.@)
 */
BOOL WINAPI IsZoomed(HWND hWnd)
{
    return (GetWindowLongW( hWnd, GWL_STYLE ) & WS_MAXIMIZE) != 0;
}


/*******************************************************************
 *		GetActiveWindow (USER32.@)
 */
HWND WINAPI GetActiveWindow(void)
{
    MESSAGEQUEUE *pCurMsgQ = 0;

    /* Get the messageQ for the current thread */
    if (!(pCurMsgQ = QUEUE_Current()))
{
        WARN("\tCurrent message queue not found. Exiting!\n" );
        return 0;
    }

    /* Return the current active window from the perQ data of the current message Q */
    return PERQDATA_GetActiveWnd( pCurMsgQ->pQData );
}


/*******************************************************************
 *         WINPOS_CanActivate
 */
static BOOL WINPOS_CanActivate(HWND hwnd)
{
    if (!hwnd) return FALSE;
    return ((GetWindowLongW( hwnd, GWL_STYLE ) & (WS_DISABLED|WS_CHILD)) == 0);
}

/*******************************************************************
 *         WINPOS_IsVisible
 */
static BOOL WINPOS_IsVisible(HWND hwnd)
{
    if (!hwnd) return FALSE;
    return ((GetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE) == WS_VISIBLE);
}


/*******************************************************************
 *		SetActiveWindow (USER32.@)
 */
HWND WINAPI SetActiveWindow( HWND hwnd )
{
    HWND prev = 0;
    WND *wndPtr = WIN_FindWndPtr( hwnd );
    MESSAGEQUEUE *pMsgQ = 0, *pCurMsgQ = 0;

    TRACE("(%08x)\n", hwnd);
    if (!hwnd)
    {
        prev = GetActiveWindow();
        WINPOS_SetActiveWindow( 0, FALSE, TRUE, FALSE );
        return prev;
    }
    if (!wndPtr) return 0;

    if (wndPtr->dwStyle & (WS_DISABLED | WS_CHILD)) goto error;

    /* Get the messageQ for the current thread */
    if (!(pCurMsgQ = QUEUE_Current()))
    {
        WARN("\tCurrent message queue not found. Exiting!\n" );
        goto error;
    }

    /* Retrieve the message queue associated with this window */
    pMsgQ = (MESSAGEQUEUE *)QUEUE_Lock( wndPtr->hmemTaskQ );
    if ( !pMsgQ )
    {
        WARN("\tWindow message queue not found. Exiting!\n" );
        goto error;
    }

    /* Make sure that the window is associated with the calling threads
     * message queue. It must share the same perQ data.
     */
    if ( pCurMsgQ->pQData != pMsgQ->pQData )
    {
        QUEUE_Unlock( pMsgQ );
        goto error;
    }

    /* Save current active window */
    prev = PERQDATA_GetActiveWnd( pMsgQ->pQData );
    QUEUE_Unlock( pMsgQ );
    WIN_ReleaseWndPtr(wndPtr);

    WINPOS_SetActiveWindow( hwnd, FALSE, TRUE, FALSE );
    return prev;

 error:
    WIN_ReleaseWndPtr(wndPtr);
    return 0;
}


/*******************************************************************
 *		GetForegroundWindow (USER32.@)
 */
HWND WINAPI GetForegroundWindow(void)
{
    HWND hwndActive = 0;

    /* Get the foreground window (active window of hActiveQueue) */
    if ( hActiveQueue )
    {
        MESSAGEQUEUE *pActiveQueue = QUEUE_Lock( hActiveQueue );
        if ( pActiveQueue )
            hwndActive = PERQDATA_GetActiveWnd( pActiveQueue->pQData );

        QUEUE_Unlock( pActiveQueue );
    }

    return hwndActive;
}

/*******************************************************************
 *		SetForegroundWindow (USER32.@)
 */
BOOL WINAPI SetForegroundWindow( HWND hwnd )
{
   HWND PrevWindow;
   BOOL Ret = TRUE;
   DWORD PrevProcessId, PrevThreadId = 0;
   DWORD NewProcessId, NewThreadId = 0;

   TRACE ("(0x%08x)\n", hwnd);
   if (!hwnd)
      return WINPOS_SetActiveWindow (0, FALSE, TRUE, TRUE);

   /* child windows get WM_CHILDACTIVATE message */
   if ((GetWindowLongW (hwnd, GWL_STYLE) & (WS_CHILD | WS_POPUP)) == WS_CHILD)
      return SendMessageA (hwnd, WM_CHILDACTIVATE, 0, 0);

   hwnd = WIN_GetFullHandle (hwnd);
   PrevWindow = GetForegroundWindow ();
   if (hwnd == PrevWindow)
      return FALSE;

   if (PrevWindow)
      PrevThreadId = GetWindowThreadProcessId (PrevWindow, &PrevProcessId);
   if (hwnd)
      NewThreadId = GetWindowThreadProcessId (hwnd, &NewProcessId);

   /* Optimization, and also "fixes" some issues seen with the event
      splitting as done below. That really does need to be investigated
      closely to see what's causing breakages */
   if (PrevWindow && hwnd && (PrevProcessId == NewProcessId) &&
       (PrevThreadId == NewThreadId))
   {
      if ((NewProcessId == GetCurrentProcessId ()) &&
          (NewThreadId == GetCurrentThreadId ()))
         return WINPOS_SetActiveWindow (hwnd, FALSE, TRUE, TRUE);
      else
      {
         SendNotifyMessageA (hwnd, WM_WINE_SETFOREGROUNDWINDOW,
                             (WPARAM)hwnd, 0);
         return TRUE;
      }
   }


   /* If old window owned by different thread, send message; otherwise
      clear directly */
   if (PrevWindow)
   {
      if ((PrevProcessId == GetCurrentProcessId ()) &&
          (PrevThreadId == GetCurrentThreadId ()))
         Ret = WINPOS_SetActiveWindow (0, FALSE, TRUE, TRUE);
      else
         SendNotifyMessageA (PrevWindow, WM_WINE_SETFOREGROUNDWINDOW, 0, 0);
   }

   /* If new window owned by different thread, send message; otherwise
      set directly */
   if (hwnd)
   {
      if ((NewProcessId == GetCurrentProcessId ()) &&
          (NewThreadId == GetCurrentThreadId ()))
         Ret = WINPOS_SetActiveWindow (hwnd, FALSE, TRUE, TRUE);
      else
         SendNotifyMessageA (hwnd, WM_WINE_SETFOREGROUNDWINDOW,
                             (WPARAM)hwnd, 0);
   }

   return Ret;
}


/*******************************************************************
 *		AllowSetForegroundWindow (USER32.@)
 */
BOOL WINAPI AllowSetForegroundWindow( DWORD procid )
{
    /* FIXME: If Win98/2000 style SetForegroundWindow behavior is
     * implemented, then fix this function. */
    return TRUE;
}


/*******************************************************************
 *		LockSetForegroundWindow (USER32.@)
 */
BOOL WINAPI LockSetForegroundWindow( UINT lockcode )
{
    /* FIXME: If Win98/2000 style SetForegroundWindow behavior is
     * implemented, then fix this function. */
    return TRUE;
}


/*******************************************************************
 *		SetShellWindow (USER32.@)
 */
HWND WINAPI SetShellWindow(HWND hwndshell)
{   WARN("(hWnd=%08x) semi stub\n",hwndshell );

    hGlobalShellWindow = WIN_GetFullHandle( hwndshell );
    return hGlobalShellWindow;
}


/*******************************************************************
 *		GetShellWindow (USER32.@)
 */
HWND WINAPI GetShellWindow(void)
{   WARN("(hWnd=%x) semi stub\n",hGlobalShellWindow );

    return hGlobalShellWindow;
}


/***********************************************************************
 *		BringWindowToTop (USER32.@)
 */
BOOL WINAPI BringWindowToTop( HWND hwnd )
{
    return SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
}


/***********************************************************************
 *		MoveWindow (USER32.@)
 */
BOOL WINAPI MoveWindow( HWND hwnd, INT x, INT y, INT cx, INT cy,
                            BOOL repaint )
{
    int flags = SWP_NOZORDER | SWP_NOACTIVATE;
    if (!repaint) flags |= SWP_NOREDRAW;
    TRACE("%04x %d,%d %dx%d %d\n",
	    hwnd, x, y, cx, cy, repaint );
    return SetWindowPos( hwnd, 0, x, y, cx, cy, flags );
}

/***********************************************************************
 *           WINPOS_InitInternalPos
 */
static LPINTERNALPOS WINPOS_InitInternalPos( WND* wnd, POINT pt, const RECT *restoreRect )
{
    LPINTERNALPOS lpPos = (LPINTERNALPOS) GetPropA( wnd->hwndSelf,
                                                      atomInternalPos );
    if( !lpPos )
    {
	/* this happens when the window is minimized/maximized
	 * for the first time (rectWindow is not adjusted yet) */

	lpPos = HeapAlloc( GetProcessHeap(), 0, sizeof(INTERNALPOS) );
	if( !lpPos ) return NULL;

	SetPropA( wnd->hwndSelf, atomInternalPos, (HANDLE)lpPos );
	lpPos->hwndIconTitle = 0; /* defer until needs to be shown */
        CONV_RECT32TO16( &wnd->rectWindow, &lpPos->rectNormal );
	*(UINT*)&lpPos->ptIconPos = *(UINT*)&lpPos->ptMaxPos = 0xFFFFFFFF;
    }

    if( wnd->dwStyle & WS_MINIMIZE )
	CONV_POINT32TO16( &pt, &lpPos->ptIconPos );
    else if( wnd->dwStyle & WS_MAXIMIZE )
	CONV_POINT32TO16( &pt, &lpPos->ptMaxPos );
    else if( restoreRect )
	CONV_RECT32TO16( restoreRect, &lpPos->rectNormal );

    return lpPos;
}

/***********************************************************************
 *           WINPOS_RedrawIconTitle
 */
BOOL WINPOS_RedrawIconTitle( HWND hWnd )
{
    LPINTERNALPOS lpPos = (LPINTERNALPOS)GetPropA( hWnd, atomInternalPos );
    if( lpPos )
    {
	if( lpPos->hwndIconTitle )
	{
	    SendMessageA( lpPos->hwndIconTitle, WM_SHOWWINDOW, TRUE, 0);
	    InvalidateRect( lpPos->hwndIconTitle, NULL, TRUE );
	    return TRUE;
	}
    }
    return FALSE;
}

/***********************************************************************
 *           WINPOS_ShowIconTitle
 */
BOOL WINPOS_ShowIconTitle( HWND hwnd, BOOL bShow )
{
    LPINTERNALPOS lpPos = (LPINTERNALPOS)GetPropA( hwnd, atomInternalPos );

    if( lpPos && !(GetWindowLongA( hwnd, GWL_EXSTYLE) & WS_EX_MANAGED))
    {
        HWND title = lpPos->hwndIconTitle;

	TRACE("0x%04x %i\n", hwnd, (bShow != 0) );

	if( !title )
	    lpPos->hwndIconTitle = title = ICONTITLE_Create( hwnd );
	if( bShow )
        {
            if (!IsWindowVisible(title))
            {
                SendMessageA( title, WM_SHOWWINDOW, TRUE, 0 );
                SetWindowPos( title, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                              SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
            }
	}
	else ShowWindow( title, SW_HIDE );
    }
    return FALSE;
}

/*******************************************************************
 *           WINPOS_GetMinMaxInfo
 *
 * Get the minimized and maximized information for a window.
 */
void WINPOS_GetMinMaxInfo( HWND hwnd, POINT *maxSize, POINT *maxPos,
			   POINT *minTrack, POINT *maxTrack )
{
    LPINTERNALPOS lpPos;
    MINMAXINFO MinMax;
    INT xinc, yinc;
    LONG style = GetWindowLongA( hwnd, GWL_STYLE );
    LONG exstyle = GetWindowLongA( hwnd, GWL_EXSTYLE );

    /* Compute default values */

    MinMax.ptMaxSize.x = GetSystemMetrics(SM_CXSCREEN);
    MinMax.ptMaxSize.y = GetSystemMetrics(SM_CYSCREEN);
    /* uncertain change - 19/09/03 - david@transgaing.com
     * My tests on windows show that the MinTrackSize doesn't
     * come from the system metrics if the window is a WS_CHILD window.
     * Instead, they are fixed at 6 - this is not a value in the system
     * metrics.
     * Test was done by intercepting the WM_GETMINMAXINFO message
     * and checking out the values.
     */
    if (style & WS_CHILD)
    {
        MinMax.ptMinTrackSize.x = 6;
        MinMax.ptMinTrackSize.y = 6;
    }
    else
    {
        MinMax.ptMinTrackSize.x = GetSystemMetrics(SM_CXMINTRACK);
        MinMax.ptMinTrackSize.y = GetSystemMetrics(SM_CYMINTRACK);
    }
    MinMax.ptMaxTrackSize.x = GetSystemMetrics(SM_CXSCREEN);
    MinMax.ptMaxTrackSize.y = GetSystemMetrics(SM_CYSCREEN);

    if (HAS_DLGFRAME( style, exstyle ))
    {
        xinc = GetSystemMetrics(SM_CXDLGFRAME);
        yinc = GetSystemMetrics(SM_CYDLGFRAME);
    }
    else
    {
        xinc = yinc = 0;
        if (HAS_THICKFRAME(style))
        {
            xinc += GetSystemMetrics(SM_CXFRAME);
            yinc += GetSystemMetrics(SM_CYFRAME);
        }
        if (style & WS_BORDER)
        {
            xinc += GetSystemMetrics(SM_CXBORDER);
            yinc += GetSystemMetrics(SM_CYBORDER);
        }
    }
    MinMax.ptMaxSize.x += 2 * xinc;
    MinMax.ptMaxSize.y += 2 * yinc;

    lpPos = (LPINTERNALPOS)GetPropA( hwnd, atomInternalPos );
    if( lpPos && !EMPTYPOINT(lpPos->ptMaxPos) )
	CONV_POINT16TO32( &lpPos->ptMaxPos, &MinMax.ptMaxPosition );
    else
    {
        MinMax.ptMaxPosition.x = -xinc;
        MinMax.ptMaxPosition.y = -yinc;
    }

    SendMessageA( hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&MinMax );

      /* Some sanity checks */

    TRACE("%ld %ld / %ld %ld / %ld %ld / %ld %ld\n",
                      MinMax.ptMaxSize.x, MinMax.ptMaxSize.y,
                      MinMax.ptMaxPosition.x, MinMax.ptMaxPosition.y,
                      MinMax.ptMaxTrackSize.x, MinMax.ptMaxTrackSize.y,
                      MinMax.ptMinTrackSize.x, MinMax.ptMinTrackSize.y);
    MinMax.ptMaxTrackSize.x = max( MinMax.ptMaxTrackSize.x,
                                   MinMax.ptMinTrackSize.x );
    MinMax.ptMaxTrackSize.y = max( MinMax.ptMaxTrackSize.y,
                                   MinMax.ptMinTrackSize.y );

    if (maxSize) *maxSize = MinMax.ptMaxSize;
    if (maxPos) *maxPos = MinMax.ptMaxPosition;
    if (minTrack) *minTrack = MinMax.ptMinTrackSize;
    if (maxTrack) *maxTrack = MinMax.ptMaxTrackSize;
}

/***********************************************************************
 *		ShowWindowAsync (USER32.@)
 *
 * doesn't wait; returns immediately.
 * used by threads to toggle windows in other (possibly hanging) threads
 */
BOOL WINAPI ShowWindowAsync( HWND hwnd, INT cmd )
{
    /* FIXME: does ShowWindow() return immediately ? */
    return ShowWindow(hwnd, cmd);
}


/***********************************************************************
 *		ShowWindow (USER32.@)
 */
BOOL WINAPI ShowWindow( HWND hwnd, INT cmd )
{
    HWND full_handle;

    if ((full_handle = WIN_IsCurrentThread( hwnd )))
        return USER_Driver.pShowWindow( full_handle, cmd );
    return SendMessageW( hwnd, WM_WINE_SHOWWINDOW, cmd, 0 );
}


/***********************************************************************
 *		GetInternalWindowPos (USER.460)
 */
UINT16 WINAPI GetInternalWindowPos16( HWND16 hwnd, LPRECT16 rectWnd,
                                      LPPOINT16 ptIcon )
{
    WINDOWPLACEMENT16 wndpl;
    if (GetWindowPlacement16( hwnd, &wndpl ))
    {
	if (rectWnd) *rectWnd = wndpl.rcNormalPosition;
	if (ptIcon)  *ptIcon = wndpl.ptMinPosition;
	return wndpl.showCmd;
    }
    return 0;
}


/***********************************************************************
 *		GetInternalWindowPos (USER32.@)
 */
UINT WINAPI GetInternalWindowPos( HWND hwnd, LPRECT rectWnd,
                                      LPPOINT ptIcon )
{
    WINDOWPLACEMENT wndpl;
    if (GetWindowPlacement( hwnd, &wndpl ))
    {
	if (rectWnd) *rectWnd = wndpl.rcNormalPosition;
	if (ptIcon)  *ptIcon = wndpl.ptMinPosition;
	return wndpl.showCmd;
    }
    return 0;
}


/***********************************************************************
 *		GetWindowPlacement (USER32.@)
 *
 * Win95:
 * Fails if wndpl->length of Win95 (!) apps is invalid.
 */
BOOL WINAPI GetWindowPlacement( HWND hwnd, WINDOWPLACEMENT *wndpl )
{
    WND *pWnd = WIN_FindWndPtr( hwnd );
    LPINTERNALPOS lpPos;

    if(!pWnd ) return FALSE;

    lpPos = WINPOS_InitInternalPos( pWnd, *(LPPOINT)&pWnd->rectWindow.left, &pWnd->rectWindow );
    wndpl->length  = sizeof(*wndpl);
    if( pWnd->dwStyle & WS_MINIMIZE )
        wndpl->showCmd = SW_SHOWMINIMIZED;
    else
        wndpl->showCmd = ( pWnd->dwStyle & WS_MAXIMIZE ) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL ;
    if( pWnd->flags & WIN_RESTORE_MAX )
        wndpl->flags = WPF_RESTORETOMAXIMIZED;
    else
        wndpl->flags = 0;
    CONV_POINT16TO32( &lpPos->ptIconPos, &wndpl->ptMinPosition );
    CONV_POINT16TO32( &lpPos->ptMaxPos, &wndpl->ptMaxPosition );
    CONV_RECT16TO32( &lpPos->rectNormal, &wndpl->rcNormalPosition );
    WIN_ReleaseWndPtr(pWnd);
    return TRUE;
}


/***********************************************************************
 *           WINPOS_SetPlacement
 */
static BOOL WINPOS_SetPlacement( HWND hwnd, const WINDOWPLACEMENT *wndpl, UINT flags )
{
    WND *pWnd = WIN_FindWndPtr( hwnd );
    if( pWnd )
    {
	LPINTERNALPOS lpPos = (LPINTERNALPOS)WINPOS_InitInternalPos( pWnd,
			     *(LPPOINT)&pWnd->rectWindow.left, &pWnd->rectWindow );

	if( flags & PLACE_MIN ) CONV_POINT32TO16( &wndpl->ptMinPosition, &lpPos->ptIconPos );
	if( flags & PLACE_MAX ) CONV_POINT32TO16( &wndpl->ptMaxPosition, &lpPos->ptMaxPos );
	if( flags & PLACE_RECT) CONV_RECT32TO16( &wndpl->rcNormalPosition, &lpPos->rectNormal );

	if( pWnd->dwStyle & WS_MINIMIZE )
	{
	    WINPOS_ShowIconTitle( pWnd->hwndSelf, FALSE );
	    if( wndpl->flags & WPF_SETMINPOSITION && !EMPTYPOINT(lpPos->ptIconPos))
		SetWindowPos( hwnd, 0, lpPos->ptIconPos.x, lpPos->ptIconPos.y,
				0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	}
	else if( pWnd->dwStyle & WS_MAXIMIZE )
	{
	    if( !EMPTYPOINT(lpPos->ptMaxPos) )
		SetWindowPos( hwnd, 0, lpPos->ptMaxPos.x, lpPos->ptMaxPos.y,
				0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	}
	else if( flags & PLACE_RECT )
		SetWindowPos( hwnd, 0, lpPos->rectNormal.left, lpPos->rectNormal.top,
				lpPos->rectNormal.right - lpPos->rectNormal.left,
				lpPos->rectNormal.bottom - lpPos->rectNormal.top,
				SWP_NOZORDER | SWP_NOACTIVATE );

	ShowWindow( hwnd, wndpl->showCmd );
	if( IsWindow(hwnd) && pWnd->dwStyle & WS_MINIMIZE )
	{
	    if( pWnd->dwStyle & WS_VISIBLE ) WINPOS_ShowIconTitle( pWnd->hwndSelf, TRUE );

	    /* SDK: ...valid only the next time... */
	    if( wndpl->flags & WPF_RESTORETOMAXIMIZED ) pWnd->flags |= WIN_RESTORE_MAX;
	}
        WIN_ReleaseWndPtr(pWnd);
	return TRUE;
    }
    return FALSE;
}


/***********************************************************************
 *		SetWindowPlacement (USER32.@)
 *
 * Win95:
 * Fails if wndpl->length of Win95 (!) apps is invalid.
 */
BOOL WINAPI SetWindowPlacement( HWND hwnd, const WINDOWPLACEMENT *wpl )
{
    if (!wpl) return FALSE;
    return WINPOS_SetPlacement( hwnd, wpl, PLACE_MIN | PLACE_MAX | PLACE_RECT );
}


/***********************************************************************
 *		AnimateWindow (USER32.@)
 *		Shows/Hides a window with an animation
 *		NO ANIMATION YET
 */
BOOL WINAPI AnimateWindow(HWND hwnd, DWORD dwTime, DWORD dwFlags)
{
	FIXME("partial stub\n");

	/* If trying to show/hide and it's already   *
	 * shown/hidden or invalid window, fail with *
	 * invalid parameter                         */
	if(!IsWindow(hwnd) ||
	   (IsWindowVisible(hwnd) && !(dwFlags & AW_HIDE)) ||
	   (!IsWindowVisible(hwnd) && (dwFlags & AW_HIDE)))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ShowWindow(hwnd, (dwFlags & AW_HIDE) ? SW_HIDE : ((dwFlags & AW_ACTIVATE) ? SW_SHOW : SW_SHOWNA));

	return TRUE;
}

/***********************************************************************
 *		SetInternalWindowPos (USER32.@)
 */
void WINAPI SetInternalWindowPos( HWND hwnd, UINT showCmd,
                                    LPRECT rect, LPPOINT pt )
{
    if( IsWindow(hwnd) )
    {
        WINDOWPLACEMENT wndpl;
	UINT flags;

	wndpl.length  = sizeof(wndpl);
	wndpl.showCmd = showCmd;
	wndpl.flags = flags = 0;

	if( pt )
	{
            flags |= PLACE_MIN;
            wndpl.flags |= WPF_SETMINPOSITION;
            wndpl.ptMinPosition = *pt;
	}
	if( rect )
	{
            flags |= PLACE_RECT;
            wndpl.rcNormalPosition = *rect;
	}
        WINPOS_SetPlacement( hwnd, &wndpl, flags );
    }
}

/*******************************************************************
 *	   WINPOS_SetActiveWindow
 *
 * SetActiveWindow() back-end. This is the only function that
 * can assign active status to a window. It must be called only
 * for the top level windows.
 */
BOOL WINPOS_SetActiveWindow( HWND hWnd, BOOL fMouse, BOOL fChangeFocus, BOOL fForegroundify)
{
    WND*     wndPtr=0, *wndTemp;
    HQUEUE16 hOldActiveQueue = 0, hNewActiveQueue = 0;
    MESSAGEQUEUE *pOldActiveQueue = 0, *pNewActiveQueue = 0;
    WORD     wIconized = 0;
    HWND     hwndActive = 0;
    HWND     hwndLastActive = 0;
    HWND     hwndForeground = 0;
    BOOL     bRet = 0;

    TRACE("(%04x, %d, %d, %d)\n", hWnd, fMouse, fChangeFocus, fForegroundify );

    /* Get current active window from the active queue */
    if ( hActiveQueue && fForegroundify )
    {
        pOldActiveQueue = QUEUE_Lock( hActiveQueue );
        if ( pOldActiveQueue )
            hwndForeground = PERQDATA_GetActiveWnd( pOldActiveQueue->pQData );
        if (hwndForeground)
            hOldActiveQueue = hActiveQueue;
        /* else: leave hOldActiveQueue at zero so that WM_ACTIVATEAPP is sent */
    }

    if ((wndPtr = WIN_FindWndPtr(hWnd)))
        hWnd = wndPtr->hwndSelf;  /* make it a full handle */

    if (wndPtr) {
        hNewActiveQueue = wndPtr->hmemTaskQ;
        pNewActiveQueue = QUEUE_Lock( hNewActiveQueue );
    }
    else {
        pNewActiveQueue = QUEUE_Lock( GetFastQueue16() );
        /* leave hNewActiveQueue at zero so that WM_ACTIVATEAPP is sent */
    }
    if ( pNewActiveQueue )
        hwndActive = PERQDATA_GetActiveWnd( pNewActiveQueue->pQData );

    if ( !fForegroundify ) {
        hwndForeground = hwndActive;
        if (hwndActive)
            /* the hwndActive's hmemTaskQ is supposed to be equal to
             * GetFastQueue16() if we get here, so I'll just use that */
            hOldActiveQueue = GetFastQueue16();
    }

    /* paranoid checks */
    if( hWnd == GetDesktopWindow() || (bRet = (hWnd == hwndForeground)) )
	goto CLEANUP_END;

/*  if (wndPtr && (GetFastQueue16() != wndPtr->hmemTaskQ))
 *	return 0;
 */

    if( (wndTemp = WIN_FindWndPtr(hwndActive)) )
    {
        wIconized = HIWORD(wndTemp->dwStyle & WS_MINIMIZE);
        WIN_ReleaseWndPtr(wndTemp);
    }
    else
        TRACE("no current active window.\n");

    /* call CBT hook chain */
    if (HOOK_IsHooked( WH_CBT ))
    {
        CBTACTIVATESTRUCT cbt;
        cbt.fMouse     = fMouse;
        cbt.hWndActive = hwndActive;
        if (HOOK_CallHooksA( WH_CBT, HCBT_ACTIVATE, (WPARAM)hWnd, (LPARAM)&cbt )) goto CLEANUP_END;
    }

    /* set prev active wnd to current active wnd */
    if (fForegroundify) hwndPrevActive = hwndForeground;
    /* send notification to current thread's old active */
    if ((hwndLastActive = hwndActive) && hwndLastActive != hWnd && IsWindow(hwndLastActive))
    {
#if 0
        MESSAGEQUEUE *pTempActiveQueue = 0;
#endif

        if (!SendMessageA( hwndActive, WM_NCACTIVATE, FALSE, 0 ))
        {
            if (GetSysModalWindow16() != WIN_Handle16(hWnd)) goto CLEANUP_END;
	          /* disregard refusal if hWnd is sysmodal */
        }

        SendMessageA( hwndActive, WM_ACTIVATE,
                        MAKEWPARAM( WA_INACTIVE, wIconized ),
                        (LPARAM)hWnd );

        /* check if something happened during message processing
         */
#if 0 /* should not be checked here, maybe after the local active window is changed (if foregroundifying) */
        pTempActiveQueue = QUEUE_Lock( hActiveQueue );
        if(!pTempActiveQueue)
          goto CLEANUP_END;

        hwndForeground = PERQDATA_GetActiveWnd( pTempActiveQueue->pQData );
        QUEUE_Unlock( pTempActiveQueue );
        if( hwndPrevActive != hwndForeground )
            goto CLEANUP_END;
#endif

        /* this check may not be worth it, but just in case */
        if ( pNewActiveQueue )
            hwndActive = PERQDATA_GetActiveWnd( pNewActiveQueue->pQData );
        if ( hwndActive != hwndLastActive )
            goto CLEANUP_END;
    }

    /* send notification to old thread's active */
    if ( pOldActiveQueue && pOldActiveQueue->pQData != pNewActiveQueue->pQData ) {
        SendNotifyMessageA( hwndForeground, WM_NCACTIVATE, FALSE, 0 );
        SendNotifyMessageA( hwndForeground, WM_ACTIVATE,
                        MAKEWPARAM( WA_INACTIVE, IsIconic(hwndForeground) ),
                        (LPARAM)hWnd );
    }

    /* Set new active window in the message queue */
    hwndActive = hWnd;
    if ( pNewActiveQueue )
    {
        PERQDATA_SetActiveWnd( pNewActiveQueue->pQData, hwndActive );
        if ( pOldActiveQueue && pOldActiveQueue->pQData != pNewActiveQueue->pQData )
            PERQDATA_SetActiveWnd( pOldActiveQueue->pQData, 0 );
    }
    else if ( pOldActiveQueue )
        PERQDATA_SetActiveWnd( pOldActiveQueue->pQData, 0 );

    /* send palette messages */
    if( !IsIconic( hWnd ) )
    {
      if (hWnd && SendMessageW( hWnd, WM_QUERYNEWPALETTE, 0, 0L))
        SendNotifyMessageW( HWND_BROADCAST, WM_PALETTEISCHANGING, (WPARAM)hWnd, 0 );
    }

    /* if prev wnd is minimized redraw icon title */
    if( IsIconic( hwndPrevActive ) ) WINPOS_RedrawIconTitle(hwndPrevActive);

    /* managed windows will get ConfigureNotify event */
    if (wndPtr && !(wndPtr->dwStyle & WS_CHILD) && !(wndPtr->dwExStyle & WS_EX_MANAGED))
    {
	/* check Z-order and bring hWnd to the top */
        HWND tmp = GetTopWindow(0);
        while (tmp && !(GetWindowLongA( tmp, GWL_STYLE ) & WS_VISIBLE))
            tmp = GetWindow( tmp, GW_HWNDNEXT );

        if( tmp != hWnd )
            SetWindowPos(hWnd, HWND_TOP, 0,0,0,0,
                         SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE );
        if (!IsWindow(hWnd))
            goto CLEANUP;
    }

    /* send WM_ACTIVATEAPP if necessary */
    if (hOldActiveQueue != hNewActiveQueue)
    {
        HWND *list, *phwnd;
        DWORD old_thread = GetWindowThreadProcessId( hwndLastActive, NULL );
        DWORD new_thread = GetWindowThreadProcessId( hwndActive, NULL );

        if ((list = WIN_ListChildren( GetDesktopWindow() )))
        {
            for (phwnd = list; *phwnd; phwnd++)
            {
                if (!IsWindow( *phwnd )) continue;
                if (GetWindowThreadProcessId( *phwnd, NULL ) == old_thread)
                    SendMessageW( *phwnd, WM_ACTIVATEAPP, 0, new_thread );
            }
            HeapFree( GetProcessHeap(), 0, list );
        }

        if (fForegroundify) hActiveQueue = hNewActiveQueue;

        if ((list = WIN_ListChildren( GetDesktopWindow() )))
        {
            for (phwnd = list; *phwnd; phwnd++)
            {
                if (!IsWindow( *phwnd )) continue;
                if (GetWindowThreadProcessId( *phwnd, NULL ) == new_thread)
                    SendMessageW( *phwnd, WM_ACTIVATEAPP, 1, old_thread );
            }
            HeapFree( GetProcessHeap(), 0, list );
        }

        if (hWnd && !IsWindow(hWnd)) goto CLEANUP;
    }

    if (hWnd)
    {
        /* walk up to the first unowned window */
        HWND tmp = GetAncestor( hWnd, GA_ROOTOWNER );
        if ((wndTemp = WIN_FindWndPtr( tmp )))
        {
            /* and set last active owned popup */
            wndTemp->hwndLastActive = hWnd;

            wIconized = HIWORD(wndTemp->dwStyle & WS_MINIMIZE);
            WIN_ReleaseWndPtr(wndTemp);
        }
        SendMessageA( hWnd, WM_NCACTIVATE, TRUE, 0 );
        SendMessageA( hWnd, WM_ACTIVATE,
         MAKEWPARAM( (fMouse) ? WA_CLICKACTIVE : WA_ACTIVE, wIconized),
         (LPARAM)hwndPrevActive );
        if( !IsWindow(hWnd) ) goto CLEANUP;
    }

    /* change focus if possible */
    if ( fChangeFocus )
    {
        if ( pNewActiveQueue )
        {
            HWND hOldFocus = PERQDATA_GetFocusWnd( pNewActiveQueue->pQData );

            if ( !hOldFocus || GetAncestor( hOldFocus, GA_ROOT ) != hwndActive )
                FOCUS_SwitchFocus( pNewActiveQueue, hOldFocus,
                                   (wndPtr && (wndPtr->dwStyle & WS_MINIMIZE))?
                                   0 : hwndActive );
        }

        if ( pOldActiveQueue &&
             ( !pNewActiveQueue ||
                pNewActiveQueue->pQData != pOldActiveQueue->pQData ) )
        {
            HWND hOldFocus = PERQDATA_GetFocusWnd( pOldActiveQueue->pQData );
            if ( hOldFocus )
                FOCUS_SwitchFocus( pOldActiveQueue, hOldFocus, 0 );
        }
    }

    if( !hwndPrevActive && hActiveQueue == hNewActiveQueue && wndPtr )
    {
        if (USER_Driver.pForceWindowRaise) USER_Driver.pForceWindowRaise( wndPtr->hwndSelf );
    }

    /* if active wnd is minimized redraw icon title */
    if( IsIconic(hwndActive) ) WINPOS_RedrawIconTitle(hwndActive);

    bRet = (hWnd == hwndActive);  /* Success? */

CLEANUP: /* Unlock the message queues before returning */

    if ( pNewActiveQueue )
        QUEUE_Unlock( pNewActiveQueue );

CLEANUP_END:

    if ( pOldActiveQueue )
        QUEUE_Unlock( pOldActiveQueue );

    WIN_ReleaseWndPtr(wndPtr);
    return bRet;
}

/*******************************************************************
 *         WINPOS_ActivateOtherWindow
 *
 *  Activates window other than pWnd.
 */
void WINPOS_ActivateOtherWindow(HWND hwnd)
{
    HWND hwndActive = 0;
    HWND hwndTo = 0;
    HWND hwndDefaultTo = 0;
    HWND owner;

    /* Get current active window from the active queue */
    if ( hActiveQueue )
    {
        MESSAGEQUEUE *pActiveQueue = QUEUE_Lock( hActiveQueue );
        if ( pActiveQueue )
        {
            hwndActive = PERQDATA_GetActiveWnd( pActiveQueue->pQData );
            QUEUE_Unlock( pActiveQueue );
        }
    }

    if (!(hwnd = WIN_IsCurrentThread( hwnd ))) return;

    if( hwnd == hwndPrevActive )
        hwndPrevActive = 0;

    if( hwndActive != hwnd && (hwndActive || USER_IsExitingThread( GetCurrentThreadId() )))
        return;

    if (!(GetWindowLongW( hwnd, GWL_STYLE ) & WS_POPUP) ||
        !(owner = GetWindow( hwnd, GW_OWNER )) ||
        !WINPOS_CanActivate((hwndTo = GetAncestor( owner, GA_ROOT ))) ||
        !WINPOS_IsVisible(hwndTo))
    {
        HWND tmp = GetAncestor( hwnd, GA_ROOT );
        hwndTo = hwndPrevActive;

        while( !WINPOS_CanActivate(hwndTo) || !WINPOS_IsVisible(hwndTo))
        {
            /* by now owned windows should've been taken care of */
            if(!hwndDefaultTo && WINPOS_CanActivate(hwndTo))
            	hwndDefaultTo = hwndTo;
            tmp = hwndTo = GetWindow( tmp, GW_HWNDNEXT );
            if( !hwndTo )
            {
            	hwndTo = hwndDefaultTo;
            	break;
            }
        }
    }

    SetActiveWindow( hwndTo );
    hwndPrevActive = 0;
}


/***********************************************************************
 *           WINPOS_HandleWindowPosChanging16
 *
 * Default handling for a WM_WINDOWPOSCHANGING. Called from DefWindowProc().
 */
LONG WINPOS_HandleWindowPosChanging16( HWND hwnd, WINDOWPOS16 *winpos )
{
    POINT maxSize, minTrack;
    LONG style = GetWindowLongA( hwnd, GWL_STYLE );

    if (winpos->flags & SWP_NOSIZE) return 0;
    if ((style & WS_THICKFRAME) || ((style & (WS_POPUP | WS_CHILD)) == 0))
    {
	WINPOS_GetMinMaxInfo( hwnd, &maxSize, NULL, &minTrack, NULL );
	if (maxSize.x < winpos->cx) winpos->cx = maxSize.x;
	if (maxSize.y < winpos->cy) winpos->cy = maxSize.y;
	if (!(style & WS_MINIMIZE))
	{
	    if (winpos->cx < minTrack.x ) winpos->cx = minTrack.x;
	    if (winpos->cy < minTrack.y ) winpos->cy = minTrack.y;
	}
    }
    return 0;
}


/***********************************************************************
 *           WINPOS_HandleWindowPosChanging
 *
 * Default handling for a WM_WINDOWPOSCHANGING. Called from DefWindowProc().
 */
LONG WINPOS_HandleWindowPosChanging( HWND hwnd, WINDOWPOS *winpos )
{
    POINT maxSize, minTrack;
    LONG style = GetWindowLongA( hwnd, GWL_STYLE );

    if (winpos->flags & SWP_NOSIZE) return 0;
    if ((style & WS_THICKFRAME) || ((style & (WS_POPUP | WS_CHILD)) == 0))
    {
	WINPOS_GetMinMaxInfo( hwnd, &maxSize, NULL, &minTrack, NULL );
	winpos->cx = min( winpos->cx, maxSize.x );
	winpos->cy = min( winpos->cy, maxSize.y );
	if (!(style & WS_MINIMIZE))
	{
	    if (winpos->cx < minTrack.x ) winpos->cx = minTrack.x;
	    if (winpos->cy < minTrack.y ) winpos->cy = minTrack.y;
	}
    }
    return 0;
}


/***********************************************************************
 *		SetWindowPos (USER32.@)
 */
BOOL WINAPI SetWindowPos( HWND hwnd, HWND hwndInsertAfter,
                          INT x, INT y, INT cx, INT cy, UINT flags )
{
    WINDOWPOS winpos;

    winpos.hwnd = hwnd;
    winpos.hwndInsertAfter = hwndInsertAfter;
    winpos.x = x;
    winpos.y = y;
    winpos.cx = cx;
    winpos.cy = cy;
    winpos.flags = flags;

    /* for windows that are exclusive fullscreen, do not let the window
       position be set to anything besides 0,0, otherwise the window contents
       may be shown in the wrong location or the mouse may be offset from where
       the application thinks it is */
    if (USER_Driver.pIsExclusiveFullscreenWindow
        && USER_Driver.pIsExclusiveFullscreenWindow( hwnd ))
    {
        TRACE("Forced window position to 0,0 for exclusive fullscreen window\n");
        winpos.x = 0;
        winpos.y = 0;
    }
    if (WIN_IsCurrentThread( hwnd )) return USER_Driver.pSetWindowPos( &winpos );
    return SendMessageW( winpos.hwnd, WM_WINE_SETWINDOWPOS, 0, (LPARAM)&winpos );
}


/***********************************************************************
 *		BeginDeferWindowPos (USER32.@)
 */
HDWP WINAPI BeginDeferWindowPos( INT count )
{
    HDWP handle;
    DWP *pDWP;

    if (count < 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    /* Windows allows zero count, in which case it allocates context for 8 moves */
    if (count == 0) count = 8;

    handle = USER_HEAP_ALLOC( sizeof(DWP) + (count-1)*sizeof(WINDOWPOS) );
    if (!handle) return 0;
    pDWP = (DWP *) USER_HEAP_LIN_ADDR( handle );
    pDWP->actualCount    = 0;
    pDWP->suggestedCount = count;
    pDWP->valid          = TRUE;
    pDWP->wMagic         = DWP_MAGIC;
    pDWP->hwndParent     = 0;
    return handle;
}


/***********************************************************************
 *		DeferWindowPos (USER32.@)
 */
HDWP WINAPI DeferWindowPos( HDWP hdwp, HWND hwnd, HWND hwndAfter,
                                INT x, INT y, INT cx, INT cy,
                                UINT flags )
{
    DWP *pDWP;
    int i;
    HDWP newhdwp = hdwp,retvalue;

    hwnd = WIN_GetFullHandle( hwnd );
    if (hwnd == GetDesktopWindow()) return 0;

    if (!(pDWP = USER_HEAP_LIN_ADDR( hdwp ))) return 0;

    USER_Lock();

    for (i = 0; i < pDWP->actualCount; i++)
    {
        if (pDWP->winPos[i].hwnd == hwnd)
        {
              /* Merge with the other changes */
            if (!(flags & SWP_NOZORDER))
            {
                pDWP->winPos[i].hwndInsertAfter = hwndAfter;
            }
            if (!(flags & SWP_NOMOVE))
            {
                pDWP->winPos[i].x = x;
                pDWP->winPos[i].y = y;
            }
            if (!(flags & SWP_NOSIZE))
            {
                pDWP->winPos[i].cx = cx;
                pDWP->winPos[i].cy = cy;
            }
            pDWP->winPos[i].flags &= flags | ~(SWP_NOSIZE | SWP_NOMOVE |
                                               SWP_NOZORDER | SWP_NOREDRAW |
                                               SWP_NOACTIVATE | SWP_NOCOPYBITS|
                                               SWP_NOOWNERZORDER);
            pDWP->winPos[i].flags |= flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW |
                                              SWP_FRAMECHANGED);
            retvalue = hdwp;
            goto END;
        }
    }
    if (pDWP->actualCount >= pDWP->suggestedCount)
    {
        newhdwp = USER_HEAP_REALLOC( hdwp,
                      sizeof(DWP) + pDWP->suggestedCount*sizeof(WINDOWPOS) );
        if (!newhdwp)
        {
            retvalue = 0;
            goto END;
        }
        pDWP = (DWP *) USER_HEAP_LIN_ADDR( newhdwp );
        pDWP->suggestedCount++;
    }
    pDWP->winPos[pDWP->actualCount].hwnd = hwnd;
    pDWP->winPos[pDWP->actualCount].hwndInsertAfter = hwndAfter;
    pDWP->winPos[pDWP->actualCount].x = x;
    pDWP->winPos[pDWP->actualCount].y = y;
    pDWP->winPos[pDWP->actualCount].cx = cx;
    pDWP->winPos[pDWP->actualCount].cy = cy;
    pDWP->winPos[pDWP->actualCount].flags = flags;
    pDWP->actualCount++;
    retvalue = newhdwp;
END:
    USER_Unlock();
    return retvalue;
}


/***********************************************************************
 *		EndDeferWindowPos (USER32.@)
 */
BOOL WINAPI EndDeferWindowPos( HDWP hdwp )
{
    DWP *pDWP;
    WINDOWPOS *winpos;
    BOOL res = TRUE;
    int i;

    pDWP = (DWP *) USER_HEAP_LIN_ADDR( hdwp );
    if (!pDWP) return FALSE;
    for (i = 0, winpos = pDWP->winPos; i < pDWP->actualCount; i++, winpos++)
    {
        if (!(res = USER_Driver.pSetWindowPos( winpos ))) break;
    }
    USER_HEAP_FREE( hdwp );
    return res;
}


/***********************************************************************
 *		TileChildWindows (USER.199)
 */
void WINAPI TileChildWindows16( HWND16 parent, WORD action )
{
    FIXME("(%04x, %d): stub\n", parent, action);
}

/***********************************************************************
 *		CascadeChildWindows (USER.198)
 */
void WINAPI CascadeChildWindows16( HWND16 parent, WORD action )
{
    FIXME("(%04x, %d): stub\n", parent, action);
}

/***********************************************************************
 *		SetProgmanWindow (USER32.@)
 */
HWND WINAPI SetProgmanWindow ( HWND hwnd )
{
	hGlobalProgmanWindow = hwnd;
	return hGlobalProgmanWindow;
}

/***********************************************************************
 *		GetProgmanWindow (USER32.@)
 */
HWND WINAPI GetProgmanWindow(void)
{
	return hGlobalProgmanWindow;
}

/***********************************************************************
 *		SetShellWindowEx (USER32.@)
 * hwndProgman =  Progman[Program Manager]
 *                |-> SHELLDLL_DefView
 * hwndListView = |   |-> SysListView32
 *                |   |   |-> tooltips_class32
 *                |   |
 *                |   |-> SysHeader32
 *                |
 *                |-> ProxyTarget
 */
HWND WINAPI SetShellWindowEx ( HWND hwndProgman, HWND hwndListView )
{
	FIXME("0x%08x 0x%08x stub\n",hwndProgman ,hwndListView );
	hGlobalShellWindow = hwndProgman;
	return hGlobalShellWindow;

}

/***********************************************************************
 *		SetTaskmanWindow (USER32.@)
 * NOTES
 *   hwnd = MSTaskSwWClass
 *          |-> SysTabControl32
 */
HWND WINAPI SetTaskmanWindow ( HWND hwnd )
{
	hGlobalTaskmanWindow = hwnd;
	return hGlobalTaskmanWindow;
}

/***********************************************************************
 *		GetTaskmanWindow (USER32.@)
 */
HWND WINAPI GetTaskmanWindow(void)
{
	return hGlobalTaskmanWindow;
}
