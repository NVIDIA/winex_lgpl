/*
 * Tab control
 *
 * Copyright 1998 Anders Carlsson
 * Copyright 1999 Alex Priem <alexp@sci.kun.nl>
 * Copyright 1999 Francis Beaudet
 *
 * TODO:
 *  Image list support
 *  Unicode support (under construction)
 *
 * FIXME:
 *  UpDown control not displayed until after a tab is clicked on
 */

#include <string.h>

#include "winbase.h"
#include "commctrl.h"
#include "comctl32.h"
#include "wine/debug.h"
#include <math.h>

WINE_DEFAULT_DEBUG_CHANNEL(tab);

typedef struct
{
  UINT   mask;
  DWORD  dwState;
  LPWSTR pszText;
  INT    iImage;
  LPARAM lParam;
  RECT   rect;    /* bounding rectangle of the item relative to the
		   * leftmost item (the leftmost item, 0, would have a
		   * "left" member of 0 in this rectangle)
                   *
                   * additionally the top member hold the row number
                   * and bottom is unused and should be 0 */
} TAB_ITEM;

typedef struct
{
  UINT       uNumItem;        /* number of tab items */
  UINT       uNumRows;	      /* number of tab rows */
  INT        tabHeight;       /* height of the tab row */
  INT        tabWidth;        /* width of tabs */
  HFONT      hFont;           /* handle to the current font */
  HCURSOR    hcurArrow;       /* handle to the current cursor */
  HIMAGELIST himl;            /* handle to a image list (may be 0) */
  HWND       hwndToolTip;     /* handle to tab's tooltip */
  INT        leftmostVisible; /* Used for scrolling, this member contains
			       * the index of the first visible item */
  INT        iSelected;       /* the currently selected item */
  INT        iHotTracked;     /* the highlighted item under the mouse */
  INT        uFocus;          /* item which has the focus */
  TAB_ITEM*  items;           /* pointer to an array of TAB_ITEM's */
  BOOL       DoRedraw;        /* flag for redrawing when tab contents is changed*/
  BOOL       needsScrolling;  /* TRUE if the size of the tabs is greater than
			       * the size of the control */
  BOOL	     fSizeSet;	      /* was the size of the tabs explicitly set? */
  BOOL       bUnicode;        /* Unicode control? */
  HWND       hwndUpDown;      /* Updown control used for scrolling */
} TAB_INFO;

/******************************************************************************
 * Positioning constants
 */
#define SELECTED_TAB_OFFSET     2
#define HORIZONTAL_ITEM_PADDING 6
#define VERTICAL_ITEM_PADDING   3
#define ROUND_CORNER_SIZE       2
#define DISPLAY_AREA_PADDINGX   2
#define DISPLAY_AREA_PADDINGY   2
#define CONTROL_BORDER_SIZEX    2
#define CONTROL_BORDER_SIZEY    2
#define BUTTON_SPACINGX         4
#define BUTTON_SPACINGY         4
#define FLAT_BTN_SPACINGX       8
#define DEFAULT_TAB_WIDTH       96

#define TAB_GetInfoPtr(hwnd) ((TAB_INFO *)GetWindowLongA(hwnd,0))

/******************************************************************************
 * Hot-tracking timer constants
 */
#define TAB_HOTTRACK_TIMER            1
#define TAB_HOTTRACK_TIMER_INTERVAL   100   /* milliseconds */

/******************************************************************************
 * Prototypes
 */
static void TAB_Refresh (HWND hwnd, HDC hdc);
static void TAB_InvalidateTabArea(HWND hwnd, TAB_INFO* infoPtr);
static void TAB_EnsureSelectionVisible(HWND hwnd, TAB_INFO* infoPtr);
static void TAB_DrawItem(HWND hwnd, HDC hdc, INT iItem);
static void TAB_DrawItemInterior(HWND hwnd, HDC hdc, INT iItem, RECT* drawRect);

static BOOL
TAB_SendSimpleNotify (HWND hwnd, UINT code)
{
    NMHDR nmhdr;

    nmhdr.hwndFrom = hwnd;
    nmhdr.idFrom = GetWindowLongA(hwnd, GWL_ID);
    nmhdr.code = code;

    return (BOOL) SendMessageA (GetParent (hwnd), WM_NOTIFY,
            (WPARAM) nmhdr.idFrom, (LPARAM) &nmhdr);
}

static VOID
TAB_RelayEvent (HWND hwndTip, HWND hwndMsg, UINT uMsg,
            WPARAM wParam, LPARAM lParam)
{
    MSG msg;

    msg.hwnd = hwndMsg;
    msg.message = uMsg;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.time = GetMessageTime ();
    msg.pt.x = LOWORD(GetMessagePos ());
    msg.pt.y = HIWORD(GetMessagePos ());

    SendMessageA (hwndTip, TTM_RELAYEVENT, 0, (LPARAM)&msg);
}

static void
TAB_DumpItemExternalA(TCITEMA *pti, UINT iItem)
{
    if (TRACE_ON(tab)) {
	TRACE("external tab %d, mask=0x%08x, dwState=0x%08x, dwStateMask=0x%08x, cchTextMax=0x%08x\n",
	      iItem, pti->mask, pti->dwState, pti->dwStateMask, pti->cchTextMax);
	TRACE("external tab %d,   iImage=%d, lParam=0x%08lx, pszTextA=%s\n",
	      iItem, pti->iImage, pti->lParam, debugstr_a(pti->pszText));
    }
}


static void
TAB_DumpItemExternalW(TCITEMW *pti, UINT iItem)
{
    if (TRACE_ON(tab)) {
	TRACE("external tab %d, mask=0x%08x, dwState=0x%08lx, dwStateMask=0x%08lx, cchTextMax=0x%08x\n",
	      iItem, pti->mask, pti->dwState, pti->dwStateMask, pti->cchTextMax);
	TRACE("external tab %d,   iImage=%d, lParam=0x%08lx, pszTextW=%s\n",
	      iItem, pti->iImage, pti->lParam, debugstr_w(pti->pszText));
    }
}

static void
TAB_DumpItemInternal(TAB_INFO *infoPtr, UINT iItem)
{
    if (TRACE_ON(tab)) {
	TAB_ITEM *ti;

	ti = &infoPtr->items[iItem];
	TRACE("tab %d, mask=0x%08x, dwState=0x%08lx, pszText=%s, iImage=%d\n",
	      iItem, ti->mask, ti->dwState, debugstr_w(ti->pszText),
	      ti->iImage);
	TRACE("tab %d, lParam=0x%08lx, rect.left=%d, rect.top(row)=%d\n",
	      iItem, ti->lParam, ti->rect.left, ti->rect.top);
    }
}

static LRESULT
TAB_GetCurSel (HWND hwnd)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

    return infoPtr->iSelected;
}

static LRESULT
TAB_GetCurFocus (HWND hwnd)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

    return infoPtr->uFocus;
}

static LRESULT
TAB_GetToolTips (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

    if (infoPtr == NULL) return 0;
    return infoPtr->hwndToolTip;
}

static LRESULT
TAB_SetCurSel (HWND hwnd,WPARAM wParam)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  INT iItem = (INT)wParam;
  INT prevItem;

  prevItem = -1;
  if ((iItem >= 0) && (iItem < infoPtr->uNumItem)) {
    prevItem=infoPtr->iSelected;
      infoPtr->iSelected=iItem;
      TAB_EnsureSelectionVisible(hwnd, infoPtr);
      TAB_InvalidateTabArea(hwnd, infoPtr);
  }
  return prevItem;
}

static LRESULT
TAB_SetCurFocus (HWND hwnd,WPARAM wParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  INT iItem=(INT) wParam;

  if ((iItem < 0) || (iItem >= infoPtr->uNumItem)) return 0;

  if (GetWindowLongA(hwnd, GWL_STYLE) & TCS_BUTTONS) {
    FIXME("Should set input focus\n");
  } else {
    int oldFocus = infoPtr->uFocus;
    if (infoPtr->iSelected != iItem || infoPtr->uFocus == -1 ) {
      infoPtr->uFocus = iItem;
      if (oldFocus != -1) {
        if (TAB_SendSimpleNotify(hwnd, TCN_SELCHANGING)!=TRUE)  {
          infoPtr->iSelected = iItem;
          TAB_SendSimpleNotify(hwnd, TCN_SELCHANGE);
        }
        else
          infoPtr->iSelected = iItem;
        TAB_EnsureSelectionVisible(hwnd, infoPtr);
        TAB_InvalidateTabArea(hwnd, infoPtr);
      }
    }
  }
  return 0;
}

static LRESULT
TAB_SetToolTips (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

    if (infoPtr == NULL) return 0;
    infoPtr->hwndToolTip = (HWND)wParam;
    return 0;
}

/******************************************************************************
 * TAB_InternalGetItemRect
 *
 * This method will calculate the rectangle representing a given tab item in
 * client coordinates. This method takes scrolling into account.
 *
 * This method returns TRUE if the item is visible in the window and FALSE
 * if it is completely outside the client area.
 */
static BOOL TAB_InternalGetItemRect(
  HWND        hwnd,
  TAB_INFO*   infoPtr,
  INT         itemIndex,
  RECT*       itemRect,
  RECT*       selectedRect)
{
  RECT tmpItemRect,clientRect;
  LONG        lStyle  = GetWindowLongA(hwnd, GWL_STYLE);

  /* Perform a sanity check and a trivial visibility check. */
  if ( (infoPtr->uNumItem <= 0) ||
       (itemIndex >= infoPtr->uNumItem) ||
       (!((lStyle & TCS_MULTILINE) || (lStyle & TCS_VERTICAL)) && (itemIndex < infoPtr->leftmostVisible)) )
    return FALSE;

  /*
   * Avoid special cases in this procedure by assigning the "out"
   * parameters if the caller didn't supply them
   */
  if (itemRect == NULL)
    itemRect = &tmpItemRect;

  /* Retrieve the unmodified item rect. */
  *itemRect = infoPtr->items[itemIndex].rect;

  /* calculate the times bottom and top based on the row */
  GetClientRect(hwnd, &clientRect);

  if ((lStyle & TCS_BOTTOM) && !(lStyle & TCS_VERTICAL))
  {
    itemRect->bottom = clientRect.bottom -
                   SELECTED_TAB_OFFSET -
                   itemRect->top * (infoPtr->tabHeight - 2) -
                   ((lStyle & TCS_BUTTONS) ? itemRect->top * BUTTON_SPACINGY : 0);

    itemRect->top = clientRect.bottom -
                   infoPtr->tabHeight -
                   itemRect->top * (infoPtr->tabHeight - 2) -
                   ((lStyle & TCS_BUTTONS) ? itemRect->top * BUTTON_SPACINGY : 0);
  }
  else if((lStyle & TCS_BOTTOM) && (lStyle & TCS_VERTICAL))
  {
    itemRect->right = clientRect.right - SELECTED_TAB_OFFSET - itemRect->left * (infoPtr->tabHeight - 2) -
                      ((lStyle & TCS_BUTTONS) ? itemRect->left * BUTTON_SPACINGY : 0);
    itemRect->left = clientRect.right - infoPtr->tabHeight - itemRect->left * (infoPtr->tabHeight - 2) -
                      ((lStyle & TCS_BUTTONS) ? itemRect->left * BUTTON_SPACINGY : 0);
  }
  else if((lStyle & TCS_VERTICAL) && !(lStyle & TCS_BOTTOM))
  {
    itemRect->right = clientRect.left + infoPtr->tabHeight + itemRect->left * (infoPtr->tabHeight - 2) +
                      ((lStyle & TCS_BUTTONS) ? itemRect->left * BUTTON_SPACINGY : 0);
    itemRect->left = clientRect.left + SELECTED_TAB_OFFSET + itemRect->left * (infoPtr->tabHeight - 2) +
                      ((lStyle & TCS_BUTTONS) ? itemRect->left * BUTTON_SPACINGY : 0);
  }
  else if(!(lStyle & TCS_VERTICAL) && !(lStyle & TCS_BOTTOM)) /* not TCS_BOTTOM and not TCS_VERTICAL */
  {
    itemRect->bottom = clientRect.top +
                      infoPtr->tabHeight +
                      itemRect->top * (infoPtr->tabHeight - 2) +
                      ((lStyle & TCS_BUTTONS) ? itemRect->top * BUTTON_SPACINGY : 0);
    itemRect->top = clientRect.top +
                   SELECTED_TAB_OFFSET +
                   itemRect->top * (infoPtr->tabHeight - 2) +
                   ((lStyle & TCS_BUTTONS) ? itemRect->top * BUTTON_SPACINGY : 0);
 }

  /*
   * "scroll" it to make sure the item at the very left of the
   * tab control is the leftmost visible tab.
   */
  if(lStyle & TCS_VERTICAL)
  {
    OffsetRect(itemRect,
	     0,
	     -(clientRect.bottom - infoPtr->items[infoPtr->leftmostVisible].rect.bottom));

    /*
     * Move the rectangle so the first item is slightly offset from
     * the bottom of the tab control.
     */
    OffsetRect(itemRect,
	     0,
	     -SELECTED_TAB_OFFSET);

  } else
  {
    OffsetRect(itemRect,
	     -infoPtr->items[infoPtr->leftmostVisible].rect.left,
	     0);

    /*
     * Move the rectangle so the first item is slightly offset from
     * the left of the tab control.
     */
    OffsetRect(itemRect,
	     SELECTED_TAB_OFFSET,
	     0);
  }

  /* Now, calculate the position of the item as if it were selected. */
  if (selectedRect!=NULL)
  {
    CopyRect(selectedRect, itemRect);

    /* The rectangle of a selected item is a bit wider. */
    if(lStyle & TCS_VERTICAL)
      InflateRect(selectedRect, 0, SELECTED_TAB_OFFSET);
    else
      InflateRect(selectedRect, SELECTED_TAB_OFFSET, 0);

    /* If it also a bit higher. */
    if ((lStyle & TCS_BOTTOM) && !(lStyle & TCS_VERTICAL))
    {
      selectedRect->top -= 2; /* the border is thicker on the bottom */
      selectedRect->bottom += SELECTED_TAB_OFFSET;
    }
    else if((lStyle & TCS_BOTTOM) && (lStyle & TCS_VERTICAL))
    {
      selectedRect->left -= 2; /* the border is thicker on the right */
      selectedRect->right += SELECTED_TAB_OFFSET;
    }
    else if(lStyle & TCS_VERTICAL)
    {
      selectedRect->left -= SELECTED_TAB_OFFSET;
      selectedRect->right += 1;
    }
    else
    {
      selectedRect->top -= SELECTED_TAB_OFFSET;
      selectedRect->bottom += 1;
    }
  }

  return TRUE;
}

static BOOL TAB_GetItemRect(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  return TAB_InternalGetItemRect(hwnd, TAB_GetInfoPtr(hwnd), (INT)wParam,
                                 (LPRECT)lParam, (LPRECT)NULL);
}

/******************************************************************************
 * TAB_KeyUp
 *
 * This method is called to handle keyboard input
 */
static LRESULT TAB_KeyUp(
  HWND   hwnd,
  WPARAM keyCode)
{
  TAB_INFO* infoPtr = TAB_GetInfoPtr(hwnd);
  int       newItem = -1;

  switch (keyCode)
  {
    case VK_LEFT:
      newItem = infoPtr->uFocus - 1;
      break;
    case VK_RIGHT:
      newItem = infoPtr->uFocus + 1;
      break;
  }

  /*
   * If we changed to a valid item, change the selection
   */
  if ((newItem >= 0) &&
       (newItem < infoPtr->uNumItem) &&
       (infoPtr->uFocus != newItem))
  {
    if (!TAB_SendSimpleNotify(hwnd, TCN_SELCHANGING))
    {
      infoPtr->iSelected = newItem;
      infoPtr->uFocus    = newItem;
      TAB_SendSimpleNotify(hwnd, TCN_SELCHANGE);

      TAB_EnsureSelectionVisible(hwnd, infoPtr);
      TAB_InvalidateTabArea(hwnd, infoPtr);
    }
  }

  return 0;
}

/******************************************************************************
 * TAB_FocusChanging
 *
 * This method is called whenever the focus goes in or out of this control
 * it is used to update the visual state of the control.
 */
static LRESULT TAB_FocusChanging(
  HWND   hwnd,
  UINT   uMsg,
  WPARAM wParam,
  LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  RECT      selectedRect;
  BOOL      isVisible;

  /*
   * Get the rectangle for the item.
   */
  isVisible = TAB_InternalGetItemRect(hwnd,
				      infoPtr,
				      infoPtr->uFocus,
				      NULL,
				      &selectedRect);

  /*
   * If the rectangle is not completely invisible, invalidate that
   * portion of the window.
   */
  if (isVisible)
  {
    InvalidateRect(hwnd, &selectedRect, TRUE);
  }

  /*
   * Don't otherwise disturb normal behavior.
   */
  return DefWindowProcA (hwnd, uMsg, wParam, lParam);
}

static HWND TAB_InternalHitTest (
  HWND      hwnd,
  TAB_INFO* infoPtr,
  POINT     pt,
  UINT*     flags)

{
  RECT rect;
  int iCount;

  for (iCount = 0; iCount < infoPtr->uNumItem; iCount++)
  {
    TAB_InternalGetItemRect(hwnd, infoPtr, iCount, &rect, NULL);

    if (PtInRect(&rect, pt))
    {
      *flags = TCHT_ONITEM;
      return iCount;
    }
  }

  *flags = TCHT_NOWHERE;
  return -1;
}

static LRESULT
TAB_HitTest (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  LPTCHITTESTINFO lptest = (LPTCHITTESTINFO) lParam;

  return TAB_InternalHitTest (hwnd, infoPtr, lptest->pt, &lptest->flags);
}

/******************************************************************************
 * TAB_NCHitTest
 *
 * Napster v2b5 has a tab control for its main navigation which has a client
 * area that covers the whole area of the dialog pages.
 * That's why it receives all msgs for that area and the underlying dialog ctrls
 * are dead.
 * So I decided that we should handle WM_NCHITTEST here and return
 * HTTRANSPARENT if we don't hit the tab control buttons.
 * FIXME: WM_NCHITTEST handling correct ? Fix it if you know that Windows
 * doesn't do it that way. Maybe depends on tab control styles ?
 */
static LRESULT
TAB_NCHitTest (HWND hwnd, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  POINT pt;
  UINT dummyflag;

  pt.x = LOWORD(lParam);
  pt.y = HIWORD(lParam);
  ScreenToClient(hwnd, &pt);

  if (TAB_InternalHitTest(hwnd, infoPtr, pt, &dummyflag) == -1)
    return HTTRANSPARENT;
  else
    return HTCLIENT;
}

static LRESULT
TAB_LButtonDown (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  POINT pt;
  INT newItem, dummy;

  if (infoPtr->hwndToolTip)
    TAB_RelayEvent (infoPtr->hwndToolTip, hwnd,
		    WM_LBUTTONDOWN, wParam, lParam);

  if (GetWindowLongA(hwnd, GWL_STYLE) & TCS_FOCUSONBUTTONDOWN ) {
    SetFocus (hwnd);
  }

  if (infoPtr->hwndToolTip)
    TAB_RelayEvent (infoPtr->hwndToolTip, hwnd,
		    WM_LBUTTONDOWN, wParam, lParam);

  pt.x = (INT)LOWORD(lParam);
  pt.y = (INT)HIWORD(lParam);

  newItem = TAB_InternalHitTest (hwnd, infoPtr, pt, &dummy);

  TRACE("On Tab, item %d\n", newItem);

  if ((newItem != -1) && (infoPtr->iSelected != newItem))
  {
    if (TAB_SendSimpleNotify(hwnd, TCN_SELCHANGING) != TRUE)
    {
      infoPtr->iSelected = newItem;
      infoPtr->uFocus    = newItem;
      TAB_SendSimpleNotify(hwnd, TCN_SELCHANGE);

      TAB_EnsureSelectionVisible(hwnd, infoPtr);

      TAB_InvalidateTabArea(hwnd, infoPtr);
    }
  }
  return 0;
}

static LRESULT
TAB_LButtonUp (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_SendSimpleNotify(hwnd, NM_CLICK);

  return 0;
}

static LRESULT
TAB_RButtonDown (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_SendSimpleNotify(hwnd, NM_RCLICK);
  return 0;
}

/******************************************************************************
 * TAB_DrawLoneItemInterior
 *
 * This calls TAB_DrawItemInterior.  However, TAB_DrawItemInterior is normally
 * called by TAB_DrawItem which is normally called by TAB_Refresh which sets
 * up the device context and font.  This routine does the same setup but
 * only calls TAB_DrawItemInterior for the single specified item.
 */
static void
TAB_DrawLoneItemInterior(HWND hwnd, TAB_INFO* infoPtr, int iItem)
{
  HDC hdc = GetDC(hwnd);
  HFONT hOldFont = SelectObject(hdc, infoPtr->hFont);
  TAB_DrawItemInterior(hwnd, hdc, iItem, NULL);
  SelectObject(hdc, hOldFont);
  ReleaseDC(hwnd, hdc);
}

/******************************************************************************
 * TAB_HotTrackTimerProc
 *
 * When a mouse-move event causes a tab to be highlighted (hot-tracking), a
 * timer is setup so we can check if the mouse is moved out of our window.
 * (We don't get an event when the mouse leaves, the mouse-move events just
 * stop being delivered to our window and just start being delivered to
 * another window.)  This function is called when the timer triggers so
 * we can check if the mouse has left our window.  If so, we un-highlight
 * the hot-tracked tab.
 */
static VOID CALLBACK
TAB_HotTrackTimerProc
  (
  HWND hwnd,    /* handle of window for timer messages */
  UINT uMsg,    /* WM_TIMER message */
  UINT idEvent, /* timer identifier */
  DWORD dwTime  /* current system time */
  )
{
  TAB_INFO* infoPtr = TAB_GetInfoPtr(hwnd);

  if (infoPtr != NULL && infoPtr->iHotTracked >= 0)
  {
    POINT pt;

    /*
    ** If we can't get the cursor position, or if the cursor is outside our
    ** window, we un-highlight the hot-tracked tab.  Note that the cursor is
    ** "outside" even if it is within our bounding rect if another window
    ** overlaps.  Note also that the case where the cursor stayed within our
    ** window but has moved off the hot-tracked tab will be handled by the
    ** WM_MOUSEMOVE event.
    */
    if (!GetCursorPos(&pt) || WindowFromPoint(pt) != hwnd)
    {
      /* Redraw iHotTracked to look normal */
      INT iRedraw = infoPtr->iHotTracked;
      infoPtr->iHotTracked = -1;
      TAB_DrawLoneItemInterior(hwnd, infoPtr, iRedraw);

      /* Kill this timer */
      KillTimer(hwnd, TAB_HOTTRACK_TIMER);
    }
  }
}

/******************************************************************************
 * TAB_RecalcHotTrack
 *
 * If a tab control has the TCS_HOTTRACK style, then the tab under the mouse
 * should be highlighted.  This function determines which tab in a tab control,
 * if any, is under the mouse and records that information.  The caller may
 * supply output parameters to receive the item number of the tab item which
 * was highlighted but isn't any longer and of the tab item which is now
 * highlighted but wasn't previously.  The caller can use this information to
 * selectively redraw those tab items.
 *
 * If the caller has a mouse position, it can supply it through the pos
 * parameter.  For example, TAB_MouseMove does this.  Otherwise, the caller
 * supplies NULL and this function determines the current mouse position
 * itself.
 */
static void
TAB_RecalcHotTrack
  (
  HWND            hwnd,
  const LPARAM*   pos,
  int*            out_redrawLeave,
  int*            out_redrawEnter
  )
{
  TAB_INFO* infoPtr = TAB_GetInfoPtr(hwnd);

  int item = -1;


  if (out_redrawLeave != NULL)
    *out_redrawLeave = -1;
  if (out_redrawEnter != NULL)
    *out_redrawEnter = -1;

  if (GetWindowLongA(hwnd, GWL_STYLE) & TCS_HOTTRACK)
  {
    POINT pt;
    UINT  flags;

    if (pos == NULL)
    {
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
    }
    else
    {
      pt.x = LOWORD(*pos);
      pt.y = HIWORD(*pos);
    }

    item = TAB_InternalHitTest(hwnd, infoPtr, pt, &flags);
  }

  if (item != infoPtr->iHotTracked)
  {
    if (infoPtr->iHotTracked >= 0)
    {
      /* Mark currently hot-tracked to be redrawn to look normal */
      if (out_redrawLeave != NULL)
        *out_redrawLeave = infoPtr->iHotTracked;

      if (item < 0)
      {
        /* Kill timer which forces recheck of mouse pos */
        KillTimer(hwnd, TAB_HOTTRACK_TIMER);
      }
    }
    else
    {
      /* Start timer so we recheck mouse pos */
      UINT timerID = SetTimer
        (
        hwnd,
        TAB_HOTTRACK_TIMER,
        TAB_HOTTRACK_TIMER_INTERVAL,
        TAB_HotTrackTimerProc
        );

      if (timerID == 0)
        return; /* Hot tracking not available */
    }

    infoPtr->iHotTracked = item;

    if (item >= 0)
    {
	/* Mark new hot-tracked to be redrawn to look highlighted */
      if (out_redrawEnter != NULL)
        *out_redrawEnter = item;
    }
  }
}

/******************************************************************************
 * TAB_MouseMove
 *
 * Handles the mouse-move event.  Updates tooltips.  Updates hot-tracking.
 */
static LRESULT
TAB_MouseMove (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  int redrawLeave;
  int redrawEnter;

  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  if (infoPtr->hwndToolTip)
    TAB_RelayEvent (infoPtr->hwndToolTip, hwnd,
		    WM_LBUTTONDOWN, wParam, lParam);

  /* Determine which tab to highlight.  Redraw tabs which change highlight
  ** status. */
  TAB_RecalcHotTrack(hwnd, &lParam, &redrawLeave, &redrawEnter);

  if (redrawLeave != -1)
    TAB_DrawLoneItemInterior(hwnd, infoPtr, redrawLeave);
  if (redrawEnter != -1)
    TAB_DrawLoneItemInterior(hwnd, infoPtr, redrawEnter);

  return 0;
}

/******************************************************************************
 * TAB_AdjustRect
 *
 * Calculates the tab control's display area given the window rectangle or
 * the window rectangle given the requested display rectangle.
 */
static LRESULT TAB_AdjustRect(
  HWND   hwnd,
  WPARAM fLarger,
  LPRECT prc)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  DWORD lStyle = GetWindowLongA(hwnd, GWL_STYLE);

  if(lStyle & TCS_VERTICAL)
  {
    if (fLarger) /* Go from display rectangle */
    {
      /* Add the height of the tabs. */
      if (lStyle & TCS_BOTTOM)
        prc->right += (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
      else
        prc->left -= (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;

      /* FIXME: not sure if these InflateRect's need to have different values for TCS_VERTICAL */
      /* Inflate the rectangle for the padding */
      InflateRect(prc, DISPLAY_AREA_PADDINGX, DISPLAY_AREA_PADDINGY);

      /* Inflate for the border */
      InflateRect(prc, CONTROL_BORDER_SIZEX, CONTROL_BORDER_SIZEX);
    }
    else /* Go from window rectangle. */
    {
      /* FIXME: not sure if these InflateRect's need to have different values for TCS_VERTICAL */
      /* Deflate the rectangle for the border */
      InflateRect(prc, -CONTROL_BORDER_SIZEX, -CONTROL_BORDER_SIZEX);

      /* Deflate the rectangle for the padding */
      InflateRect(prc, -DISPLAY_AREA_PADDINGX, -DISPLAY_AREA_PADDINGY);

      /* Remove the height of the tabs. */
      if (lStyle & TCS_BOTTOM)
        prc->right -= (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
      else
        prc->left += (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
    }
  }
  else {
    if (fLarger) /* Go from display rectangle */
    {
      /* Add the height of the tabs. */
      if (lStyle & TCS_BOTTOM)
        prc->bottom += (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
      else
        prc->top -= (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;

      /* Inflate the rectangle for the padding */
      InflateRect(prc, DISPLAY_AREA_PADDINGX, DISPLAY_AREA_PADDINGY);

      /* Inflate for the border */
      InflateRect(prc, CONTROL_BORDER_SIZEX, CONTROL_BORDER_SIZEX);
    }
    else /* Go from window rectangle. */
    {
      /* Deflate the rectangle for the border */
      InflateRect(prc, -CONTROL_BORDER_SIZEX, -CONTROL_BORDER_SIZEX);

      /* Deflate the rectangle for the padding */
      InflateRect(prc, -DISPLAY_AREA_PADDINGX, -DISPLAY_AREA_PADDINGY);

      /* Remove the height of the tabs. */
      if (lStyle & TCS_BOTTOM)
        prc->bottom -= (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
      else
        prc->top += (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
    }
  }

  return 0;
}

/******************************************************************************
 * TAB_OnHScroll
 *
 * This method will handle the notification from the scroll control and
 * perform the scrolling operation on the tab control.
 */
static LRESULT TAB_OnHScroll(
  HWND    hwnd,
  int     nScrollCode,
  int     nPos,
  HWND    hwndScroll)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  if(nScrollCode == SB_THUMBPOSITION && nPos != infoPtr->leftmostVisible)
  {
     if(nPos < infoPtr->leftmostVisible)
        infoPtr->leftmostVisible--;
     else
        infoPtr->leftmostVisible++;

     TAB_RecalcHotTrack(hwnd, NULL, NULL, NULL);
     TAB_InvalidateTabArea(hwnd, infoPtr);
     SendMessageA(infoPtr->hwndUpDown, UDM_SETPOS, 0,
                   MAKELONG(infoPtr->leftmostVisible, 0));
   }

   return 0;
}

/******************************************************************************
 * TAB_SetupScrolling
 *
 * This method will check the current scrolling state and make sure the
 * scrolling control is displayed (or not).
 */
static void TAB_SetupScrolling(
  HWND        hwnd,
  TAB_INFO*   infoPtr,
  const RECT* clientRect)
{
  INT maxRange = 0;
  DWORD lStyle = GetWindowLongA(hwnd, GWL_STYLE);

  if (infoPtr->needsScrolling)
  {
    RECT controlPos;
    INT vsize, tabwidth;

    /*
     * Calculate the position of the scroll control.
     */
    if(lStyle & TCS_VERTICAL)
    {
      controlPos.right = clientRect->right;
      controlPos.left  = controlPos.right - 2 * GetSystemMetrics(SM_CXHSCROLL);

      if (lStyle & TCS_BOTTOM)
      {
        controlPos.top    = clientRect->bottom - infoPtr->tabHeight;
        controlPos.bottom = controlPos.top + GetSystemMetrics(SM_CYHSCROLL);
      }
      else
      {
        controlPos.bottom = clientRect->top + infoPtr->tabHeight;
        controlPos.top    = controlPos.bottom - GetSystemMetrics(SM_CYHSCROLL);
      }
    }
    else
    {
      controlPos.right = clientRect->right;
      controlPos.left  = controlPos.right - 2 * GetSystemMetrics(SM_CXHSCROLL);

      if (lStyle & TCS_BOTTOM)
      {
        controlPos.top    = clientRect->bottom - infoPtr->tabHeight;
        controlPos.bottom = controlPos.top + GetSystemMetrics(SM_CYHSCROLL);
      }
      else
      {
        controlPos.bottom = clientRect->top + infoPtr->tabHeight;
        controlPos.top    = controlPos.bottom - GetSystemMetrics(SM_CYHSCROLL);
      }
    }

    /*
     * If we don't have a scroll control yet, we want to create one.
     * If we have one, we want to make sure it's positioned properly.
     */
    if (infoPtr->hwndUpDown==0)
    {
      infoPtr->hwndUpDown = CreateWindowA("msctls_updown32",
					  "",
					  WS_VISIBLE | WS_CHILD | UDS_HORZ,
					  controlPos.left, controlPos.top,
					  controlPos.right - controlPos.left,
					  controlPos.bottom - controlPos.top,
					  hwnd,
					  (HMENU)NULL,
					  (HINSTANCE)NULL,
					  NULL);
    }
    else
    {
      SetWindowPos(infoPtr->hwndUpDown,
		   (HWND)NULL,
		   controlPos.left, controlPos.top,
		   controlPos.right - controlPos.left,
		   controlPos.bottom - controlPos.top,
		   SWP_SHOWWINDOW | SWP_NOZORDER);
    }

    /* Now calculate upper limit of the updown control range.
     * We do this by calculating how many tabs will be offscreen when the
     * last tab is visible.
     */
    if(infoPtr->uNumItem)
    {
       vsize = clientRect->right - (controlPos.right - controlPos.left + 1);
       maxRange = infoPtr->uNumItem;
       tabwidth = infoPtr->items[maxRange - 1].rect.right;

       for(; maxRange > 0; maxRange--)
       {
          if(tabwidth - infoPtr->items[maxRange - 1].rect.left > vsize)
             break;
       }

       if(maxRange == infoPtr->uNumItem)
          maxRange--;
    }
  }
  else
  {
    /* If we once had a scroll control... hide it */
    if (infoPtr->hwndUpDown!=0)
      ShowWindow(infoPtr->hwndUpDown, SW_HIDE);
  }
  if (infoPtr->hwndUpDown)
     SendMessageA(infoPtr->hwndUpDown, UDM_SETRANGE32, 0, maxRange);
}

/******************************************************************************
 * TAB_SetItemBounds
 *
 * This method will calculate the position rectangles of all the items in the
 * control. The rectangle calculated starts at 0 for the first item in the
 * list and ignores scrolling and selection.
 * It also uses the current font to determine the height of the tab row and
 * it checks if all the tabs fit in the client area of the window. If they
 * dont, a scrolling control is added.
 */
static void TAB_SetItemBounds (HWND hwnd)
{
  TAB_INFO*   infoPtr = TAB_GetInfoPtr(hwnd);
  LONG        lStyle  = GetWindowLongA(hwnd, GWL_STYLE);
  TEXTMETRICA fontMetrics;
  INT         curItem;
  INT         curItemLeftPos;
  INT         curItemRowCount;
  HFONT       hFont, hOldFont;
  HDC         hdc;
  RECT        clientRect;
  SIZE        size;
  INT         iTemp;
  RECT*       rcItem;
  INT         iIndex;

  /*
   * We need to get text information so we need a DC and we need to select
   * a font.
   */
  hdc = GetDC(hwnd);

  hFont = infoPtr->hFont ? infoPtr->hFont : GetStockObject (SYSTEM_FONT);
  hOldFont = SelectObject (hdc, hFont);

  /*
   * We will base the rectangle calculations on the client rectangle
   * of the control.
   */
  GetClientRect(hwnd, &clientRect);

  /* if TCS_VERTICAL then swap the height and width so this code places the
     tabs along the top of the rectangle and we can just rotate them after
     rather than duplicate all of the below code */
  if(lStyle & TCS_VERTICAL)
  {
     iTemp = clientRect.bottom;
     clientRect.bottom = clientRect.right;
     clientRect.right = iTemp;
  }

  /* The leftmost item will be "0" aligned */
  curItemLeftPos = 0;
  curItemRowCount = infoPtr->uNumItem ? 1 : 0;

  if (!(lStyle & TCS_FIXEDWIDTH) && !((lStyle & TCS_OWNERDRAWFIXED) && infoPtr->fSizeSet) )
  {
    int item_height;
    int icon_height = 0;

    /* Use the current font to determine the height of a tab. */
    GetTextMetricsA(hdc, &fontMetrics);

    /* Get the icon height */
    if (infoPtr->himl)
      ImageList_GetIconSize(infoPtr->himl, 0, &icon_height);

    /* Take the highest between font or icon */
    if (fontMetrics.tmHeight > icon_height)
      item_height = fontMetrics.tmHeight;
    else
      item_height = icon_height;

    /*
     * Make sure there is enough space for the letters + icon + growing the
     * selected item + extra space for the selected item.
     */
    infoPtr->tabHeight = item_height + 2 * VERTICAL_ITEM_PADDING +
        SELECTED_TAB_OFFSET;

  }

  TRACE("client right=%d\n", clientRect.right);

  for (curItem = 0; curItem < infoPtr->uNumItem; curItem++)
  {
    /* Set the leftmost position of the tab. */
    infoPtr->items[curItem].rect.left = curItemLeftPos;

    if ( (lStyle & TCS_FIXEDWIDTH) || ((lStyle & TCS_OWNERDRAWFIXED) && infoPtr->fSizeSet))
    {
      infoPtr->items[curItem].rect.right = infoPtr->items[curItem].rect.left +
                                           infoPtr->tabWidth +
                                           2 * HORIZONTAL_ITEM_PADDING;
    }
    else
    {
      int icon_width  = 0;
      int num = 2;

      /* Calculate how wide the tab is depending on the text it contains */
      GetTextExtentPoint32W(hdc, infoPtr->items[curItem].pszText,
                            lstrlenW(infoPtr->items[curItem].pszText), &size);

      /* under Windows, there seems to be a minimum width of 2x the height
       * for button style tabs */
      if (lStyle & TCS_BUTTONS)
	      size.cx = max(size.cx, 2 * (infoPtr->tabHeight - 2));

      /* Add the icon width */
      if (infoPtr->himl)
      {
        ImageList_GetIconSize(infoPtr->himl, &icon_width, 0);
        num++;
      }

      infoPtr->items[curItem].rect.right = infoPtr->items[curItem].rect.left +
                                           size.cx + icon_width +
                                           num * HORIZONTAL_ITEM_PADDING;
      TRACE("for <%s>, l,r=%d,%d, num=%d\n",
	  debugstr_w(infoPtr->items[curItem].pszText),
	  infoPtr->items[curItem].rect.left,
	  infoPtr->items[curItem].rect.right,
	  num);
    }

    /*
     * Check if this is a multiline tab control and if so
     * check to see if we should wrap the tabs
     *
     * Because we are going to arange all these tabs evenly
     * really we are basically just counting rows at this point
     *
     */

    if (((lStyle & TCS_MULTILINE) || (lStyle & TCS_VERTICAL)) &&
        (infoPtr->items[curItem].rect.right > clientRect.right))
    {
        infoPtr->items[curItem].rect.right -=
                                      infoPtr->items[curItem].rect.left;

	infoPtr->items[curItem].rect.left = 0;
        curItemRowCount++;
	TRACE("wrapping <%s>, l,r=%d,%d\n",
	    debugstr_w(infoPtr->items[curItem].pszText),
	    infoPtr->items[curItem].rect.left,
	    infoPtr->items[curItem].rect.right);
    }

    infoPtr->items[curItem].rect.bottom = 0;
    infoPtr->items[curItem].rect.top = curItemRowCount - 1;

    TRACE("TextSize: %li\n", size.cx);
    TRACE("Rect: T %i, L %i, B %i, R %i\n",
	  infoPtr->items[curItem].rect.top,
	  infoPtr->items[curItem].rect.left,
	  infoPtr->items[curItem].rect.bottom,
	  infoPtr->items[curItem].rect.right);

    /*
     * The leftmost position of the next item is the rightmost position
     * of this one.
     */
    if (lStyle & TCS_BUTTONS)
    {
      curItemLeftPos = infoPtr->items[curItem].rect.right + 1;
      if (lStyle & TCS_FLATBUTTONS)
        curItemLeftPos += FLAT_BTN_SPACINGX;
    }
    else
      curItemLeftPos = infoPtr->items[curItem].rect.right;
  }

  if (!((lStyle & TCS_MULTILINE) || (lStyle & TCS_VERTICAL)))
  {
    /*
     * Check if we need a scrolling control.
     */
    infoPtr->needsScrolling = (curItemLeftPos + (2 * SELECTED_TAB_OFFSET) >
                               clientRect.right);

    /* Don't need scrolling, then update infoPtr->leftmostVisible */
    if(!infoPtr->needsScrolling)
      infoPtr->leftmostVisible = 0;

    TAB_SetupScrolling(hwnd, infoPtr, &clientRect);
  }

  /* Set the number of rows */
  infoPtr->uNumRows = curItemRowCount;

   if (((lStyle & TCS_MULTILINE) || (lStyle & TCS_VERTICAL)) && (infoPtr->uNumItem > 0))
   {
      INT widthDiff, remainder;
      INT tabPerRow,remTab;
      INT iRow,iItm;
      INT iIndexStart=0,iIndexEnd=0, iCount=0;

      /*
       * Ok windows tries to even out the rows. place the same
       * number of tabs in each row. So lets give that a shot
       */

      tabPerRow = infoPtr->uNumItem / (infoPtr->uNumRows);
      remTab = infoPtr->uNumItem % (infoPtr->uNumRows);

      for (iItm=0,iRow=0,iCount=0,curItemLeftPos=0;
           iItm<infoPtr->uNumItem;
           iItm++,iCount++)
      {
          /* normalize the current rect */

          /* shift the item to the left side of the clientRect */
          infoPtr->items[iItm].rect.right -=
            infoPtr->items[iItm].rect.left;
          infoPtr->items[iItm].rect.left = 0;

	  TRACE("r=%d, cl=%d, cl.r=%d, iCount=%d, iRow=%d, uNumRows=%d, remTab=%d, tabPerRow=%d\n",
	      infoPtr->items[iItm].rect.right,
	      curItemLeftPos, clientRect.right,
	      iCount, iRow, infoPtr->uNumRows, remTab, tabPerRow);

          /* if we have reached the maximum number of tabs on this row */
          /* move to the next row, reset our current item left position and */
          /* the count of items on this row */

	  /* ************  FIXME FIXME FIXME  *************** */
	  /*                                                  */
	  /* FIXME:                                           */
	  /* if vertical,                                     */ 
	  /*   if item n and n+1 are in the same row,         */
	  /*      then the display has n+1 lower (toward the  */
	  /*      bottom) than n. We do it just the           */
	  /*      opposite!!!                                 */
	  /*                                                  */
	  /* ************  FIXME FIXME FIXME  *************** */

	  if (lStyle & TCS_VERTICAL) {
	      /* Vert: Add the remaining tabs in the *last* remainder rows */
	      if (iCount >= ((iRow>=(INT)infoPtr->uNumRows - remTab)?tabPerRow + 1:tabPerRow)) {
		  iRow++;
		  curItemLeftPos = 0;
		  iCount = 0;
	      }
	  } else {
	      /* Horz: Add the remaining tabs in the *first* remainder rows */
	      if (iCount >= ((iRow<remTab)?tabPerRow + 1:tabPerRow)) {
		  iRow++;
		  curItemLeftPos = 0;
		  iCount = 0;
	      }
	  } 

          /* shift the item to the right to place it as the next item in this row */
          infoPtr->items[iItm].rect.left += curItemLeftPos;
          infoPtr->items[iItm].rect.right += curItemLeftPos;
          infoPtr->items[iItm].rect.top = iRow;
          if (lStyle & TCS_BUTTONS)
	  {
            curItemLeftPos = infoPtr->items[iItm].rect.right + 1;
            if (lStyle & TCS_FLATBUTTONS)
	      curItemLeftPos += FLAT_BTN_SPACINGX;
	  }
          else
            curItemLeftPos = infoPtr->items[iItm].rect.right;

	  TRACE("arranging <%s>, l,r=%d,%d, row=%d\n",
	      debugstr_w(infoPtr->items[iItm].pszText),
	      infoPtr->items[iItm].rect.left,
	      infoPtr->items[iItm].rect.right,
	      infoPtr->items[iItm].rect.top);
      }

      /*
       * Justify the rows
       */
      {
         while(iIndexStart < infoPtr->uNumItem)
        {
        /*
         * find the indexs of the row
         */
        /* find the first item on the next row */
        for (iIndexEnd=iIndexStart;
             (iIndexEnd < infoPtr->uNumItem) &&
 	       (infoPtr->items[iIndexEnd].rect.top ==
                infoPtr->items[iIndexStart].rect.top) ;
            iIndexEnd++)
        /* intentionally blank */;

        /*
         * we need to justify these tabs so they fill the whole given
         * client area
         *
         */
        /* find the amount of space remaining on this row */
        widthDiff = clientRect.right - (2 * SELECTED_TAB_OFFSET) -
                            infoPtr->items[iIndexEnd - 1].rect.right;

        /* iCount is the number of tab items on this row */
        iCount = iIndexEnd - iIndexStart;


        if (iCount > 1)
        {
           remainder = widthDiff % iCount;
           widthDiff = widthDiff / iCount;
           /* add widthDiff/iCount, or extra space/items on row, to each item on this row */
           for (iIndex=iIndexStart,iCount=0; iIndex < iIndexEnd;
                iIndex++,iCount++)
           {
              infoPtr->items[iIndex].rect.left += iCount * widthDiff;
              infoPtr->items[iIndex].rect.right += (iCount + 1) * widthDiff;

	      TRACE("adjusting 1 <%s>, l,r=%d,%d\n",
		  debugstr_w(infoPtr->items[iIndex].pszText),
		  infoPtr->items[iIndex].rect.left,
		  infoPtr->items[iIndex].rect.right);

           }
           infoPtr->items[iIndex - 1].rect.right += remainder;
        }
        else /* we have only one item on this row, make it take up the entire row */
        {
          infoPtr->items[iIndexStart].rect.left = clientRect.left;
          infoPtr->items[iIndexStart].rect.right = clientRect.right - 4;

	  TRACE("adjusting 2 <%s>, l,r=%d,%d\n",
	      debugstr_w(infoPtr->items[iIndexStart].pszText),
	      infoPtr->items[iIndexStart].rect.left,
	      infoPtr->items[iIndexStart].rect.right);

        }


        iIndexStart = iIndexEnd;
        }
      }
  }

  /* if TCS_VERTICAL rotate the tabs so they are along the side of the clientRect */
  if(lStyle & TCS_VERTICAL)
  {
    RECT rcOriginal;
    for(iIndex = 0; iIndex < infoPtr->uNumItem; iIndex++)
    {
      rcItem = &(infoPtr->items[iIndex].rect);

      rcOriginal = *rcItem;

      /* this is rotating the items by 90 degrees around the center of the control */
      rcItem->top = (clientRect.right - (rcOriginal.left - clientRect.left)) - (rcOriginal.right - rcOriginal.left);
      rcItem->bottom = rcItem->top + (rcOriginal.right - rcOriginal.left);
      rcItem->left = rcOriginal.top;
      rcItem->right = rcOriginal.bottom;
    }
  }

  TAB_EnsureSelectionVisible(hwnd,infoPtr);
  TAB_RecalcHotTrack(hwnd, NULL, NULL, NULL);

  /* Cleanup */
  SelectObject (hdc, hOldFont);
  ReleaseDC (hwnd, hdc);
}

/******************************************************************************
 * TAB_DrawItemInterior
 *
 * This method is used to draw the interior (text and icon) of a single tab
 * into the tab control.
 */
static void
TAB_DrawItemInterior
  (
  HWND        hwnd,
  HDC         hdc,
  INT         iItem,
  RECT*       drawRect
  )
{
  TAB_INFO* infoPtr = TAB_GetInfoPtr(hwnd);
  LONG      lStyle  = GetWindowLongA(hwnd, GWL_STYLE);

  RECT localRect;

  HPEN   htextPen   = GetSysColorPen (COLOR_BTNTEXT);
  HPEN   holdPen;
  INT    oldBkMode;

  if (drawRect == NULL)
  {
    BOOL isVisible;
    RECT itemRect;
    RECT selectedRect;

    /*
     * Get the rectangle for the item.
     */
    isVisible = TAB_InternalGetItemRect(hwnd, infoPtr, iItem, &itemRect, &selectedRect);
    if (!isVisible)
      return;

    /*
     * Make sure drawRect points to something valid; simplifies code.
     */
    drawRect = &localRect;

    /*
     * This logic copied from the part of TAB_DrawItem which draws
     * the tab background.  It's important to keep it in sync.  I
     * would have liked to avoid code duplication, but couldn't figure
     * out how without making spaghetti of TAB_DrawItem.
     */
    if (lStyle & TCS_BUTTONS)
    {
      *drawRect = itemRect;
      if (iItem == infoPtr->iSelected)
      {
        drawRect->right--;
        drawRect->bottom--;
      }
    }
    else
    {
      if (iItem == infoPtr->iSelected)
        *drawRect = selectedRect;
      else
        *drawRect = itemRect;
      drawRect->right--;
      drawRect->bottom--;
    }
  }

  /*
   * Text pen
   */
  holdPen = SelectObject(hdc, htextPen);

  oldBkMode = SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, GetSysColor((iItem == infoPtr->iHotTracked) ? COLOR_HIGHLIGHT : COLOR_BTNTEXT));

  /*
   * Deflate the rectangle to acount for the padding
   */
  if(lStyle & TCS_VERTICAL)
    InflateRect(drawRect, -VERTICAL_ITEM_PADDING, -HORIZONTAL_ITEM_PADDING);
  else
    InflateRect(drawRect, -HORIZONTAL_ITEM_PADDING, -VERTICAL_ITEM_PADDING);


  /*
   * if owner draw, tell the owner to draw
   */
  if ((lStyle & TCS_OWNERDRAWFIXED) && GetParent(hwnd))
  {
    DRAWITEMSTRUCT dis;
    UINT id;

    /*
     * get the control id
     */
    id = GetWindowLongA( hwnd, GWL_ID );

    /*
     * put together the DRAWITEMSTRUCT
     */
    dis.CtlType    = ODT_TAB;
    dis.CtlID      = id;
    dis.itemID     = iItem;
    dis.itemAction = ODA_DRAWENTIRE;
    if ( iItem == infoPtr->iSelected )
      dis.itemState = ODS_SELECTED;
    else
      dis.itemState = 0;
    dis.hwndItem = hwnd;		/* */
    dis.hDC      = hdc;
    dis.rcItem   = *drawRect;		/* */
    dis.itemData = infoPtr->items[iItem].lParam;

    /*
     * send the draw message
     */
    SendMessageA( GetParent(hwnd), WM_DRAWITEM, (WPARAM)id, (LPARAM)&dis );
  }
  else
  {
    INT cx;
    INT cy;
    UINT uHorizAlign;
    RECT rcTemp;
    RECT rcImage;
    LOGFONTA logfont;
    HFONT hFont = 0;
    HFONT hOldFont = 0; /* stop uninitialized warning */

    INT nEscapement = 0; /* stop uninitialized warning */
    INT nOrientation = 0; /* stop uninitialized warning */
    INT iPointSize;

    /* used to center the icon and text in the tab */
    RECT rcText;
    INT center_offset;

    /* set rcImage to drawRect, we will use top & left in our ImageList_Draw call */
    rcImage = *drawRect;

    rcTemp = *drawRect;

    rcText.left = rcText.top = rcText.right = rcText.bottom = 0;

    /*
     * Setup for text output
     */
    oldBkMode = SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor((iItem == infoPtr->iHotTracked) ? COLOR_HIGHLIGHT : COLOR_BTNTEXT));

    /* get the rectangle that the text fits in */
    DrawTextW(hdc, infoPtr->items[iItem].pszText, -1,
              &rcText, DT_CALCRECT);
    rcText.right += 4;
    /*
     * If not owner draw, then do the drawing ourselves.
     *
     * Draw the icon.
     */
    if (infoPtr->himl && (infoPtr->items[iItem].mask & TCIF_IMAGE))
    {
      ImageList_GetIconSize(infoPtr->himl, &cx, &cy);

      if(lStyle & TCS_VERTICAL)
        center_offset = ((drawRect->bottom - drawRect->top) - (cy + HORIZONTAL_ITEM_PADDING + (rcText.right - rcText.left))) / 2;
      else
        center_offset = ((drawRect->right - drawRect->left) - (cx + HORIZONTAL_ITEM_PADDING + (rcText.right - rcText.left))) / 2;

      TRACE("for <%s>, c_o=%d, draw=(%d,%d)-(%d,%d), textlen=%d\n",
	  debugstr_w(infoPtr->items[iItem].pszText), center_offset,
	  drawRect->left, drawRect->top, drawRect->right, drawRect->bottom,
	  (rcText.right-rcText.left));

      if((lStyle & TCS_VERTICAL) && (lStyle & TCS_BOTTOM))
      {
        rcImage.top = drawRect->top + center_offset;
        rcImage.left = drawRect->right - cx; /* if tab is TCS_VERTICAL and TCS_BOTTOM, the text is drawn from the */
                                             /* right side of the tab, but the image still uses the left as its x position */
                                             /* this keeps the image always drawn off of the same side of the tab */
        drawRect->top = rcImage.top + (cx + HORIZONTAL_ITEM_PADDING);
      }
      else if(lStyle & TCS_VERTICAL)
      {
        rcImage.top = drawRect->bottom - cy - center_offset;
	rcImage.left--; 
        drawRect->bottom = rcImage.top - HORIZONTAL_ITEM_PADDING;
      }
      else /* normal style, whether TCS_BOTTOM or not */
      {
        rcImage.left = drawRect->left + center_offset + 3;
        drawRect->left = rcImage.left + cx + HORIZONTAL_ITEM_PADDING;
	rcImage.top -= (lStyle & TCS_BOTTOM) ? 2 : 1; 
      }

      TRACE("drawing image=%d, left=%d, top=%d\n",
	    infoPtr->items[iItem].iImage, rcImage.left, rcImage.top-1);
      ImageList_Draw
        (
        infoPtr->himl,
        infoPtr->items[iItem].iImage,
        hdc,
        rcImage.left,
        rcImage.top,
        ILD_NORMAL
        );
    } else /* no image, so just shift the drawRect borders around */
    {
      if(lStyle & TCS_VERTICAL)
      {
        center_offset = 0;
        /*
        currently the rcText rect is flawed because the rotated font does not
        often match the horizontal font. So leave this as 0
        ((drawRect->bottom - drawRect->top) - (rcText.right - rcText.left)) / 2;
        */
        if(lStyle & TCS_BOTTOM)
          drawRect->top+=center_offset;
        else
          drawRect->bottom-=center_offset;
      }
      else
      {
        center_offset = ((drawRect->right - drawRect->left) - (rcText.right - rcText.left)) / 2;
        drawRect->left+=center_offset;
      }
    }

    /* Draw the text */
    if (lStyle & TCS_RIGHTJUSTIFY)
      uHorizAlign = DT_CENTER;
    else
      uHorizAlign = DT_LEFT;

    if(lStyle & TCS_VERTICAL) /* if we are vertical rotate the text and each character */
    {
      if(lStyle & TCS_BOTTOM)
      {
        nEscapement = -900;
        nOrientation = -900;
      }
      else
      {
        nEscapement = 900;
        nOrientation = 900;
      }
    }

    /* to get a font with the escapement and orientation we are looking for, we need to */
    /* call CreateFontIndirectA, which requires us to set the values of the logfont we pass in */
    if(lStyle & TCS_VERTICAL)
    {
      if (!GetObjectA((infoPtr->hFont) ?
                infoPtr->hFont : GetStockObject(SYSTEM_FONT),
                sizeof(LOGFONTA),&logfont))
      {
        iPointSize = 9;

        lstrcpyA(logfont.lfFaceName, "Arial");
        logfont.lfHeight = -MulDiv(iPointSize, GetDeviceCaps(hdc, LOGPIXELSY),
                                    72);
        logfont.lfWeight = FW_NORMAL;
        logfont.lfItalic = 0;
        logfont.lfUnderline = 0;
        logfont.lfStrikeOut = 0;
      }

      logfont.lfEscapement = nEscapement;
      logfont.lfOrientation = nOrientation;
      hFont = CreateFontIndirectA(&logfont);
      hOldFont = SelectObject(hdc, hFont);
    }

    if (lStyle & TCS_VERTICAL)
    {
      ExtTextOutW(hdc,
      (lStyle & TCS_BOTTOM) ? drawRect->right : drawRect->left,
      (!(lStyle & TCS_BOTTOM)) ? drawRect->bottom : drawRect->top,
      ETO_CLIPPED,
      drawRect,
      infoPtr->items[iItem].pszText,
      lstrlenW(infoPtr->items[iItem].pszText),
      0);
    }
    else
    {
      DrawTextW
      (
        hdc,
        infoPtr->items[iItem].pszText,
        lstrlenW(infoPtr->items[iItem].pszText),
        drawRect,
        uHorizAlign | DT_SINGLELINE
        );
    }

    /* clean things up */
    *drawRect = rcTemp; /* restore drawRect */

    if(lStyle & TCS_VERTICAL)
    {
      SelectObject(hdc, hOldFont); /* restore the original font */
      if (hFont)
        DeleteObject(hFont);
    }
  }

  /*
  * Cleanup
  */
  SetBkMode(hdc, oldBkMode);
  SelectObject(hdc, holdPen);
}

/******************************************************************************
 * TAB_DrawItem
 *
 * This method is used to draw a single tab into the tab control.
 */
static void TAB_DrawItem(
  HWND hwnd,
  HDC  hdc,
  INT  iItem)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  LONG      lStyle  = GetWindowLongA(hwnd, GWL_STYLE);
  RECT      itemRect;
  RECT      selectedRect;
  BOOL      isVisible;
  RECT      r;

  /*
   * Get the rectangle for the item.
   */
  isVisible = TAB_InternalGetItemRect(hwnd,
				      infoPtr,
				      iItem,
				      &itemRect,
				      &selectedRect);

  if (isVisible)
  {
    HBRUSH hbr       = CreateSolidBrush (GetSysColor(COLOR_BTNFACE));
    HPEN   hwPen     = GetSysColorPen (COLOR_3DHILIGHT);
    HPEN hbPen  = GetSysColorPen (COLOR_3DDKSHADOW);
    HPEN hShade = GetSysColorPen (COLOR_BTNSHADOW);

    HPEN   holdPen;
    BOOL   deleteBrush = TRUE;

    if (lStyle & TCS_BUTTONS)
    {
      /* Get item rectangle */
      r = itemRect;

      holdPen = SelectObject (hdc, hwPen);

      /* Separators between flat buttons */
      /* FIXME: test and correct this if necessary for TCS_FLATBUTTONS style */
      if (lStyle & TCS_FLATBUTTONS)
      {
        int x = r.right + FLAT_BTN_SPACINGX - 2;

        /* highlight */
        MoveToEx (hdc, x, r.bottom - 1, NULL);
        LineTo   (hdc, x, r.top - 1);
        x--;

        /* shadow */
        SelectObject(hdc, hbPen);
        MoveToEx (hdc, x, r.bottom - 1, NULL);
        LineTo   (hdc, x, r.top - 1);

        /* shade */
        SelectObject (hdc, hShade );
        MoveToEx (hdc, x - 1, r.bottom - 1, NULL);
        LineTo   (hdc, x - 1, r.top - 1);
      }

      if (iItem == infoPtr->iSelected)
      {
        /* Background color */
        if (!((lStyle & TCS_OWNERDRAWFIXED) && infoPtr->fSizeSet))
	{
              COLORREF bk = GetSysColor(COLOR_3DHILIGHT);
              DeleteObject(hbr);
              hbr = GetSysColorBrush(COLOR_SCROLLBAR);

              SetTextColor(hdc, GetSysColor(COLOR_3DFACE));
              SetBkColor(hdc, bk);

              /* if COLOR_WINDOW happens to be the same as COLOR_3DHILIGHT
               * we better use 0x55aa bitmap brush to make scrollbar's background
               * look different from the window background.
               */
               if (bk == GetSysColor(COLOR_WINDOW))
                  hbr = COMCTL32_hPattern55AABrush;

              deleteBrush = FALSE;
	}

        /* Erase the background */
        FillRect(hdc, &r, hbr);

        /*
         * Draw the tab now.
         * The rectangles calculated exclude the right and bottom
         * borders of the rectangle. To simplify the following code, those
         * borders are shaved-off beforehand.
         */
        r.right--;
        r.bottom--;

        /* highlight */
        SelectObject(hdc, hwPen);
        MoveToEx (hdc, r.left, r.bottom, NULL);
        LineTo   (hdc, r.right, r.bottom);
        LineTo   (hdc, r.right, r.top + 1);

        /* shadow */
        SelectObject(hdc, hbPen);
        LineTo  (hdc, r.left + 1, r.top + 1);
        LineTo  (hdc, r.left + 1, r.bottom);

        /* shade */
        SelectObject (hdc, hShade );
        MoveToEx (hdc, r.right, r.top, NULL);
        LineTo   (hdc, r.left, r.top);
        LineTo   (hdc, r.left, r.bottom);
      }
      else
      {
        /* Erase the background */
        FillRect(hdc, &r, hbr);

	if (!(lStyle & TCS_FLATBUTTONS))
	{
          /* highlight */
          MoveToEx (hdc, r.left, r.bottom, NULL);
          LineTo   (hdc, r.left, r.top);
          LineTo   (hdc, r.right, r.top);

          /* shadow */
          SelectObject(hdc, hbPen);
          LineTo  (hdc, r.right, r.bottom);
          LineTo  (hdc, r.left, r.bottom);

          /* shade */
          SelectObject (hdc, hShade );
          MoveToEx (hdc, r.right - 1, r.top, NULL);
          LineTo   (hdc, r.right - 1, r.bottom - 1);
          LineTo   (hdc, r.left + 1, r.bottom - 1);
	}
      }
    }
    else /* !TCS_BUTTONS */
    {
      /* Background color */
      DeleteObject(hbr);
      hbr = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

      /* We draw a rectangle of different sizes depending on the selection
       * state. */
      if (iItem == infoPtr->iSelected)
        r = selectedRect;
      else
        r = itemRect;

      /*
       * Erase the background.
       * This is necessary when drawing the selected item since it is larger
       * than the others, it might overlap with stuff already drawn by the
       * other tabs
       */
      FillRect(hdc, &r, hbr);

      /*
       * Draw the tab now.
       * The rectangles calculated exclude the right and bottom
       * borders of the rectangle. To simplify the following code, those
       * borders are shaved-off beforehand.
       */
      r.right--;
      r.bottom--;

      holdPen = SelectObject (hdc, hwPen);
      if(lStyle & TCS_VERTICAL)
      {
        if (lStyle & TCS_BOTTOM)
        {
          /* highlight */
          MoveToEx (hdc, r.left, r.top, NULL);
          LineTo   (hdc, r.right - ROUND_CORNER_SIZE, r.top);
          LineTo   (hdc, r.right, r.top + ROUND_CORNER_SIZE);

          /* shadow */
          SelectObject(hdc, hbPen);
          LineTo  (hdc, r.right, r.bottom - ROUND_CORNER_SIZE);
          LineTo  (hdc, r.right - ROUND_CORNER_SIZE, r.bottom);
          LineTo  (hdc, r.left - 1, r.bottom);

          /* shade */
          SelectObject (hdc, hShade );
          MoveToEx (hdc, r.right - 1, r.top, NULL);
          LineTo   (hdc, r.right - 1, r.bottom - 1);
          LineTo   (hdc, r.left - 1,    r.bottom - 1);
        }
        else
        {
          /* highlight */
          MoveToEx (hdc, r.right, r.top, NULL);
          LineTo   (hdc, r.left + ROUND_CORNER_SIZE, r.top);
          LineTo   (hdc, r.left, r.top + ROUND_CORNER_SIZE);
          LineTo   (hdc, r.left, r.bottom - ROUND_CORNER_SIZE);

          /* shadow */
          SelectObject(hdc, hbPen);
          LineTo (hdc, r.left + ROUND_CORNER_SIZE,  r.bottom);
          LineTo (hdc, r.right + 1, r.bottom);

          /* shade */
          SelectObject (hdc, hShade );
          MoveToEx (hdc, r.left + ROUND_CORNER_SIZE - 1, r.bottom - 1, NULL);
          LineTo   (hdc, r.right + 1, r.bottom - 1);
        }
      }
      else
      {
        if (lStyle & TCS_BOTTOM)
        {
          /* highlight */
          MoveToEx (hdc, r.left, r.top, NULL);
          LineTo   (hdc, r.left, r.bottom - ROUND_CORNER_SIZE);
          LineTo   (hdc, r.left + ROUND_CORNER_SIZE, r.bottom);

          /* shadow */
          SelectObject(hdc, hbPen);
          LineTo  (hdc, r.right - ROUND_CORNER_SIZE, r.bottom);
          LineTo  (hdc, r.right, r.bottom - ROUND_CORNER_SIZE);
          LineTo  (hdc, r.right, r.top - 1);

          /* shade */
          SelectObject (hdc, hShade );
          MoveToEx   (hdc, r.left, r.bottom - 1, NULL);
          LineTo   (hdc, r.right - ROUND_CORNER_SIZE - 1, r.bottom - 1);
          LineTo   (hdc, r.right - 1, r.bottom - ROUND_CORNER_SIZE - 1);
          LineTo  (hdc, r.right - 1, r.top - 1);
        }
        else
        {
          /* highlight */
          if(infoPtr->items[iItem].rect.left == 0) /* if leftmost draw the line longer */
            MoveToEx (hdc, r.left, r.bottom, NULL);
          else
            MoveToEx (hdc, r.left, r.bottom - 1, NULL);

          LineTo   (hdc, r.left, r.top + ROUND_CORNER_SIZE);
          LineTo   (hdc, r.left + ROUND_CORNER_SIZE, r.top);
          LineTo   (hdc, r.right - ROUND_CORNER_SIZE, r.top);

          /* shadow */
          SelectObject(hdc, hbPen);
          LineTo (hdc, r.right,  r.top + ROUND_CORNER_SIZE);
          LineTo (hdc, r.right,  r.bottom + 1);


          /* shade */
          SelectObject (hdc, hShade );
          MoveToEx (hdc, r.right - 1, r.top + ROUND_CORNER_SIZE, NULL);
          LineTo   (hdc, r.right - 1, r.bottom + 1);
        }
      }
    }

    TAB_DumpItemInternal(infoPtr, iItem);

    /* This modifies r to be the text rectangle. */
    {
      HFONT hOldFont = SelectObject(hdc, infoPtr->hFont);
      TAB_DrawItemInterior(hwnd, hdc, iItem, &r);
      SelectObject(hdc,hOldFont);
    }

    /* Draw the focus rectangle */
    if (((lStyle & TCS_FOCUSNEVER) == 0) &&
	 (GetFocus() == hwnd) &&
	 (iItem == infoPtr->uFocus) )
    {
      r = itemRect;
      InflateRect(&r, -1, -1);

      DrawFocusRect(hdc, &r);
    }

    /* Cleanup */
    SelectObject(hdc, holdPen);
    if (deleteBrush) DeleteObject(hbr);
  }
}

/******************************************************************************
 * TAB_DrawBorder
 *
 * This method is used to draw the raised border around the tab control
 * "content" area.
 */
static void TAB_DrawBorder (HWND hwnd, HDC hdc)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  HPEN htmPen;
  HPEN hwPen  = GetSysColorPen (COLOR_3DHILIGHT);
  HPEN hbPen  = GetSysColorPen (COLOR_3DDKSHADOW);
  HPEN hShade = GetSysColorPen (COLOR_BTNSHADOW);
  RECT rect;
  DWORD lStyle = GetWindowLongA(hwnd, GWL_STYLE);

  GetClientRect (hwnd, &rect);

  /*
   * Adjust for the style
   */

  if (infoPtr->uNumItem)
  {
    if ((lStyle & TCS_BOTTOM) && !(lStyle & TCS_VERTICAL))
    {
      rect.bottom -= (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
    }
    else if((lStyle & TCS_BOTTOM) && (lStyle & TCS_VERTICAL))
    {
      rect.right -= (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
    }
    else if(lStyle & TCS_VERTICAL)
    {
      rect.left += (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 2;
    }
    else /* not TCS_VERTICAL and not TCS_BOTTOM */
    {
      rect.top += (infoPtr->tabHeight - 2) * infoPtr->uNumRows + 1;
    }
  }

  /*
   * Shave-off the right and bottom margins (exluded in the
   * rect)
   */
  rect.right--;
  rect.bottom--;

  /* highlight */
  htmPen = SelectObject (hdc, hwPen);

  MoveToEx (hdc, rect.left, rect.bottom, NULL);
  LineTo (hdc, rect.left, rect.top);
  LineTo (hdc, rect.right, rect.top);

  /* Dark Shadow */
  SelectObject (hdc, hbPen);
  LineTo (hdc, rect.right, rect.bottom );
  LineTo (hdc, rect.left, rect.bottom);

  /* shade */
  SelectObject (hdc, hShade );
  MoveToEx (hdc, rect.right - 1, rect.top, NULL);
  LineTo   (hdc, rect.right - 1, rect.bottom - 1);
  LineTo   (hdc, rect.left,    rect.bottom - 1);

  SelectObject(hdc, htmPen);
}

/******************************************************************************
 * TAB_Refresh
 *
 * This method repaints the tab control..
 */
static void TAB_Refresh (HWND hwnd, HDC hdc)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  HFONT hOldFont;
  INT i;

  if (!infoPtr->DoRedraw)
    return;

  hOldFont = SelectObject (hdc, infoPtr->hFont);

  if (GetWindowLongA(hwnd, GWL_STYLE) & TCS_BUTTONS)
  {
    for (i = 0; i < infoPtr->uNumItem; i++)
      TAB_DrawItem (hwnd, hdc, i);
  }
  else
  {
    /* Draw all the non selected item first */
    for (i = 0; i < infoPtr->uNumItem; i++)
    {
      if (i != infoPtr->iSelected)
	TAB_DrawItem (hwnd, hdc, i);
    }

    /* Now, draw the border, draw it before the selected item
     * since the selected item overwrites part of the border. */
    TAB_DrawBorder (hwnd, hdc);

    /* Then, draw the selected item */
    TAB_DrawItem (hwnd, hdc, infoPtr->iSelected);

    /* If we haven't set the current focus yet, set it now.
     * Only happens when we first paint the tab controls */
    if (infoPtr->uFocus == -1)
      TAB_SetCurFocus(hwnd, infoPtr->iSelected);
  }

  SelectObject (hdc, hOldFont);
}

static DWORD
TAB_GetRowCount (HWND hwnd )
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  return infoPtr->uNumRows;
}

static LRESULT
TAB_SetRedraw (HWND hwnd, WPARAM wParam)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  infoPtr->DoRedraw=(BOOL) wParam;
  return 0;
}

static LRESULT TAB_EraseBackground(
  HWND hwnd,
  HDC  givenDC)
{
  HDC  hdc;
  RECT clientRect;

  HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

  hdc = givenDC ? givenDC : GetDC(hwnd);

  GetClientRect(hwnd, &clientRect);

  FillRect(hdc, &clientRect, brush);

  if (givenDC==0)
    ReleaseDC(hwnd, hdc);

  DeleteObject(brush);

  return 0;
}

/******************************************************************************
 * TAB_EnsureSelectionVisible
 *
 * This method will make sure that the current selection is completely
 * visible by scrolling until it is.
 */
static void TAB_EnsureSelectionVisible(
  HWND      hwnd,
  TAB_INFO* infoPtr)
{
  INT iSelected = infoPtr->iSelected;
  LONG lStyle = GetWindowLongA(hwnd, GWL_STYLE);
  INT iOrigLeftmostVisible = infoPtr->leftmostVisible;

  /* set the items row to the bottommost row or topmost row depending on
   * style */
  if ((infoPtr->uNumRows > 1) && !(lStyle & TCS_BUTTONS))
  {
      INT newselected;
      INT iTargetRow;

      if(lStyle & TCS_VERTICAL)
        newselected = infoPtr->items[iSelected].rect.left;
      else
        newselected = infoPtr->items[iSelected].rect.top;

      /* the target row is always (number of rows - 1)
         as row 0 is furthest from the clientRect */
      iTargetRow = infoPtr->uNumRows - 1;

      if (newselected != iTargetRow)
      {
         INT i;
         if(lStyle & TCS_VERTICAL)
         {
           for (i=0; i < infoPtr->uNumItem; i++)
           {
             /* move everything in the row of the selected item to the iTargetRow */
             if (infoPtr->items[i].rect.left == newselected )
                 infoPtr->items[i].rect.left = iTargetRow;
             else
             {
               if (infoPtr->items[i].rect.left > newselected)
                 infoPtr->items[i].rect.left-=1;
             }
           }
         }
         else
         {
           for (i=0; i < infoPtr->uNumItem; i++)
           {
             if (infoPtr->items[i].rect.top == newselected )
                 infoPtr->items[i].rect.top = iTargetRow;
             else
             {
               if (infoPtr->items[i].rect.top > newselected)
                 infoPtr->items[i].rect.top-=1;
             }
          }
        }
        TAB_RecalcHotTrack(hwnd, NULL, NULL, NULL);
      }
  }

  /*
   * Do the trivial cases first.
   */
  if ( (!infoPtr->needsScrolling) ||
       (infoPtr->hwndUpDown==0) || (lStyle & TCS_VERTICAL))
    return;

  if (infoPtr->leftmostVisible >= iSelected)
  {
    infoPtr->leftmostVisible = iSelected;
  }
  else
  {
     RECT r;
     INT  width, i;

     /* Calculate the part of the client area that is visible */
     GetClientRect(hwnd, &r);
     width = r.right;

     GetClientRect(infoPtr->hwndUpDown, &r);
     width -= r.right;

     if ((infoPtr->items[iSelected].rect.right -
          infoPtr->items[iSelected].rect.left) >= width )
     {
        /* Special case: width of selected item is greater than visible
         * part of control.
         */
        infoPtr->leftmostVisible = iSelected;
     }
     else
     {
        for (i = infoPtr->leftmostVisible; i < infoPtr->uNumItem; i++)
        {
           if ((infoPtr->items[iSelected].rect.right -
                infoPtr->items[i].rect.left) < width)
              break;
        }
        infoPtr->leftmostVisible = i;
     }
  }

  if (infoPtr->leftmostVisible != iOrigLeftmostVisible)
    TAB_RecalcHotTrack(hwnd, NULL, NULL, NULL);

  SendMessageA(infoPtr->hwndUpDown, UDM_SETPOS, 0,
               MAKELONG(infoPtr->leftmostVisible, 0));
}

/******************************************************************************
 * TAB_InvalidateTabArea
 *
 * This method will invalidate the portion of the control that contains the
 * tabs. It is called when the state of the control changes and needs
 * to be redisplayed
 */
static void TAB_InvalidateTabArea(
  HWND      hwnd,
  TAB_INFO* infoPtr)
{
  RECT clientRect;
  DWORD lStyle = GetWindowLongA(hwnd, GWL_STYLE);
  INT lastRow = infoPtr->uNumRows - 1;

  if (lastRow < 0) return;

  GetClientRect(hwnd, &clientRect);

  if ((lStyle & TCS_BOTTOM) && !(lStyle & TCS_VERTICAL))
  {
    clientRect.top = clientRect.bottom -
                   infoPtr->tabHeight -
                   lastRow * (infoPtr->tabHeight - 2) -
                   ((lStyle & TCS_BUTTONS) ? lastRow * BUTTON_SPACINGY : 0) - 2;
  }
  else if((lStyle & TCS_BOTTOM) && (lStyle & TCS_VERTICAL))
  {
    clientRect.left = clientRect.right - infoPtr->tabHeight -
                      lastRow * (infoPtr->tabHeight - 2) -
                      ((lStyle & TCS_BUTTONS) ? lastRow * BUTTON_SPACINGY : 0) - 2;
  }
  else if(lStyle & TCS_VERTICAL)
  {
    clientRect.right = clientRect.left + infoPtr->tabHeight +
                       lastRow * (infoPtr->tabHeight - 2) -
                      ((lStyle & TCS_BUTTONS) ? lastRow * BUTTON_SPACINGY : 0) + 1;

  }
  else
  {
    clientRect.bottom = clientRect.top + infoPtr->tabHeight +
                      lastRow * (infoPtr->tabHeight - 2) +
                      ((lStyle & TCS_BUTTONS) ? lastRow * BUTTON_SPACINGY : 0) + 1;
  }

  InvalidateRect(hwnd, &clientRect, TRUE);
}

static LRESULT
TAB_Paint (HWND hwnd, WPARAM wParam)
{
  HDC hdc;
  PAINTSTRUCT ps;

  hdc = wParam== 0 ? BeginPaint (hwnd, &ps) : (HDC)wParam;
  TAB_Refresh (hwnd, hdc);

  if(!wParam)
    EndPaint (hwnd, &ps);

  return 0;
}

static LRESULT
TAB_InsertItemA (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  TCITEMA *pti;
  INT iItem;
  RECT rect;

  GetClientRect (hwnd, &rect);
  TRACE("Rect: %x T %i, L %i, B %i, R %i\n", hwnd,
        rect.top, rect.left, rect.bottom, rect.right);

  pti = (TCITEMA *)lParam;
  iItem = (INT)wParam;

  if (iItem < 0) return -1;
  if (iItem > infoPtr->uNumItem)
    iItem = infoPtr->uNumItem;

  TAB_DumpItemExternalA(pti, iItem);


  if (infoPtr->uNumItem == 0) {
    infoPtr->items = COMCTL32_Alloc (sizeof (TAB_ITEM));
    infoPtr->uNumItem++;
    infoPtr->iSelected = 0;
  }
  else {
    TAB_ITEM *oldItems = infoPtr->items;

    infoPtr->uNumItem++;
    infoPtr->items = COMCTL32_Alloc (sizeof (TAB_ITEM) * infoPtr->uNumItem);

    /* pre insert copy */
    if (iItem > 0) {
      memcpy (&infoPtr->items[0], &oldItems[0],
	      iItem * sizeof(TAB_ITEM));
    }

    /* post insert copy */
    if (iItem < infoPtr->uNumItem - 1) {
      memcpy (&infoPtr->items[iItem+1], &oldItems[iItem],
	      (infoPtr->uNumItem - iItem - 1) * sizeof(TAB_ITEM));

    }

    if (iItem <= infoPtr->iSelected)
      infoPtr->iSelected++;

    COMCTL32_Free (oldItems);
  }

  infoPtr->items[iItem].mask = pti->mask;
  if (pti->mask & TCIF_TEXT)
    Str_SetPtrAtoW (&infoPtr->items[iItem].pszText, pti->pszText);

  if (pti->mask & TCIF_IMAGE)
    infoPtr->items[iItem].iImage = pti->iImage;

  if (pti->mask & TCIF_PARAM)
    infoPtr->items[iItem].lParam = pti->lParam;

  TAB_SetItemBounds(hwnd);
  TAB_InvalidateTabArea(hwnd, infoPtr);

  TRACE("[%04x]: added item %d %s\n",
	hwnd, iItem, debugstr_w(infoPtr->items[iItem].pszText));

  return iItem;
}


static LRESULT
TAB_InsertItemW (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  TCITEMW *pti;
  INT iItem;
  RECT rect;

  GetClientRect (hwnd, &rect);
  TRACE("Rect: %x T %i, L %i, B %i, R %i\n", hwnd,
        rect.top, rect.left, rect.bottom, rect.right);

  pti = (TCITEMW *)lParam;
  iItem = (INT)wParam;

  if (iItem < 0) return -1;
  if (iItem > infoPtr->uNumItem)
    iItem = infoPtr->uNumItem;

  TAB_DumpItemExternalW(pti, iItem);

  if (infoPtr->uNumItem == 0) {
    infoPtr->items = COMCTL32_Alloc (sizeof (TAB_ITEM));
    infoPtr->uNumItem++;
    infoPtr->iSelected = 0;
  }
  else {
    TAB_ITEM *oldItems = infoPtr->items;

    infoPtr->uNumItem++;
    infoPtr->items = COMCTL32_Alloc (sizeof (TAB_ITEM) * infoPtr->uNumItem);

    /* pre insert copy */
    if (iItem > 0) {
      memcpy (&infoPtr->items[0], &oldItems[0],
	      iItem * sizeof(TAB_ITEM));
    }

    /* post insert copy */
    if (iItem < infoPtr->uNumItem - 1) {
      memcpy (&infoPtr->items[iItem+1], &oldItems[iItem],
	      (infoPtr->uNumItem - iItem - 1) * sizeof(TAB_ITEM));

  }

    if (iItem <= infoPtr->iSelected)
      infoPtr->iSelected++;

    COMCTL32_Free (oldItems);
  }

  infoPtr->items[iItem].mask = pti->mask;
  if (pti->mask & TCIF_TEXT)
    Str_SetPtrW (&infoPtr->items[iItem].pszText, pti->pszText);

  if (pti->mask & TCIF_IMAGE)
    infoPtr->items[iItem].iImage = pti->iImage;

  if (pti->mask & TCIF_PARAM)
    infoPtr->items[iItem].lParam = pti->lParam;

  TAB_SetItemBounds(hwnd);
  TAB_InvalidateTabArea(hwnd, infoPtr);

  TRACE("[%04x]: added item %d %s\n",
	hwnd, iItem, debugstr_w(infoPtr->items[iItem].pszText));

  return iItem;
}


static LRESULT
TAB_SetItemSize (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  LONG lStyle = GetWindowLongA(hwnd, GWL_STYLE);
  LONG lResult = 0;

  if ((lStyle & TCS_FIXEDWIDTH) || (lStyle & TCS_OWNERDRAWFIXED))
  {
    lResult = MAKELONG(infoPtr->tabWidth, infoPtr->tabHeight);
    infoPtr->tabWidth = (INT)LOWORD(lParam);
    infoPtr->tabHeight = (INT)HIWORD(lParam);
  }
  infoPtr->fSizeSet = TRUE;

  return lResult;
}

static LRESULT
TAB_SetItemA (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  TCITEMA *tabItem;
  TAB_ITEM *wineItem;
  INT    iItem;

  iItem = (INT)wParam;
  tabItem = (LPTCITEMA)lParam;

  TRACE("%d %p\n", iItem, tabItem);
  if ((iItem<0) || (iItem>=infoPtr->uNumItem)) return FALSE;

  TAB_DumpItemExternalA(tabItem, iItem);

  wineItem = &infoPtr->items[iItem];

  if (tabItem->mask & TCIF_IMAGE)
    wineItem->iImage = tabItem->iImage;

  if (tabItem->mask & TCIF_PARAM)
    wineItem->lParam = tabItem->lParam;

  if (tabItem->mask & TCIF_RTLREADING)
    FIXME("TCIF_RTLREADING\n");

  if (tabItem->mask & TCIF_STATE)
    wineItem->dwState = tabItem->dwState;

  if (tabItem->mask & TCIF_TEXT)
   Str_SetPtrAtoW(&wineItem->pszText, tabItem->pszText);

  /* Update and repaint tabs */
  TAB_SetItemBounds(hwnd);
  TAB_InvalidateTabArea(hwnd,infoPtr);

  return TRUE;
  }


static LRESULT
TAB_SetItemW (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  TCITEMW *tabItem;
  TAB_ITEM *wineItem;
  INT    iItem;

  iItem = (INT)wParam;
  tabItem = (LPTCITEMW)lParam;

  TRACE("%d %p\n", iItem, tabItem);
  if ((iItem<0) || (iItem>=infoPtr->uNumItem)) return FALSE;

  TAB_DumpItemExternalW(tabItem, iItem);

  wineItem = &infoPtr->items[iItem];

  if (tabItem->mask & TCIF_IMAGE)
    wineItem->iImage = tabItem->iImage;

  if (tabItem->mask & TCIF_PARAM)
    wineItem->lParam = tabItem->lParam;

  if (tabItem->mask & TCIF_RTLREADING)
    FIXME("TCIF_RTLREADING\n");

  if (tabItem->mask & TCIF_STATE)
    wineItem->dwState = tabItem->dwState;

  if (tabItem->mask & TCIF_TEXT)
   Str_SetPtrW(&wineItem->pszText, tabItem->pszText);

  /* Update and repaint tabs */
  TAB_SetItemBounds(hwnd);
  TAB_InvalidateTabArea(hwnd,infoPtr);

  return TRUE;
}


static LRESULT
TAB_GetItemCount (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
   TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

   return infoPtr->uNumItem;
}


static LRESULT
TAB_GetItemA (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
   TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
   TCITEMA *tabItem;
   TAB_ITEM *wineItem;
   INT    iItem;

  iItem = (INT)wParam;
  tabItem = (LPTCITEMA)lParam;
  TRACE("\n");
  if ((iItem<0) || (iItem>=infoPtr->uNumItem))
    return FALSE;

  wineItem = &infoPtr->items[iItem];

  if (tabItem->mask & TCIF_IMAGE)
    tabItem->iImage = wineItem->iImage;

  if (tabItem->mask & TCIF_PARAM)
    tabItem->lParam = wineItem->lParam;

  if (tabItem->mask & TCIF_RTLREADING)
    FIXME("TCIF_RTLREADING\n");

  if (tabItem->mask & TCIF_STATE)
    tabItem->dwState = wineItem->dwState;

  if (tabItem->mask & TCIF_TEXT)
   Str_GetPtrWtoA (wineItem->pszText, tabItem->pszText, tabItem->cchTextMax);

  TAB_DumpItemExternalA(tabItem, iItem);

  return TRUE;
}


static LRESULT
TAB_GetItemW (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  TCITEMW *tabItem;
  TAB_ITEM *wineItem;
  INT    iItem;

  iItem = (INT)wParam;
  tabItem = (LPTCITEMW)lParam;
  TRACE("\n");
  if ((iItem<0) || (iItem>=infoPtr->uNumItem))
    return FALSE;

  wineItem=& infoPtr->items[iItem];

  if (tabItem->mask & TCIF_IMAGE)
    tabItem->iImage = wineItem->iImage;

  if (tabItem->mask & TCIF_PARAM)
    tabItem->lParam = wineItem->lParam;

  if (tabItem->mask & TCIF_RTLREADING)
    FIXME("TCIF_RTLREADING\n");

  if (tabItem->mask & TCIF_STATE)
    tabItem->dwState = wineItem->dwState;

  if (tabItem->mask & TCIF_TEXT)
   Str_GetPtrW (wineItem->pszText, tabItem->pszText, tabItem->cchTextMax);

  TAB_DumpItemExternalW(tabItem, iItem);

  return TRUE;
}


static LRESULT
TAB_DeleteItem (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  INT iItem = (INT) wParam;
  BOOL bResult = FALSE;

  if ((iItem >= 0) && (iItem < infoPtr->uNumItem))
  {
    TAB_ITEM *oldItems = infoPtr->items;

    infoPtr->uNumItem--;
    infoPtr->items = COMCTL32_Alloc(sizeof (TAB_ITEM) * infoPtr->uNumItem);

    if (iItem > 0)
      memcpy(&infoPtr->items[0], &oldItems[0], iItem * sizeof(TAB_ITEM));

    if (iItem < infoPtr->uNumItem)
      memcpy(&infoPtr->items[iItem], &oldItems[iItem + 1],
              (infoPtr->uNumItem - iItem) * sizeof(TAB_ITEM));

    COMCTL32_Free(oldItems);

    /* Readjust the selected index */
    if ((iItem == infoPtr->iSelected) && (iItem > 0))
      infoPtr->iSelected--;

    if (iItem < infoPtr->iSelected)
      infoPtr->iSelected--;

    if (infoPtr->uNumItem == 0)
      infoPtr->iSelected = -1;

    /* Reposition and repaint tabs */
    TAB_SetItemBounds(hwnd);
    TAB_InvalidateTabArea(hwnd,infoPtr);

    bResult = TRUE;
  }

  return bResult;
}

static LRESULT
TAB_DeleteAllItems (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
   TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  COMCTL32_Free (infoPtr->items);
  infoPtr->uNumItem = 0;
  infoPtr->iSelected = -1;
  if (infoPtr->iHotTracked >= 0)
    KillTimer(hwnd, TAB_HOTTRACK_TIMER);
  infoPtr->iHotTracked = -1;

  TAB_SetItemBounds(hwnd);
  TAB_InvalidateTabArea(hwnd,infoPtr);
  return TRUE;
}


static LRESULT
TAB_GetFont (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  TRACE("\n");
  return (LRESULT)infoPtr->hFont;
}

static LRESULT
TAB_SetFont (HWND hwnd, WPARAM wParam, LPARAM lParam)

{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  TRACE("%x %lx\n",wParam, lParam);

  infoPtr->hFont = (HFONT)wParam;

  TAB_SetItemBounds(hwnd);

  TAB_InvalidateTabArea(hwnd, infoPtr);

  return 0;
}


static LRESULT
TAB_GetImageList (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);

  TRACE("\n");
  return (LRESULT)infoPtr->himl;
}

static LRESULT
TAB_SetImageList (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
    HIMAGELIST himlPrev;

    TRACE("\n");
    himlPrev = infoPtr->himl;
    infoPtr->himl= (HIMAGELIST)lParam;
    return (LRESULT)himlPrev;
}

static LRESULT
TAB_GetUnicodeFormat (HWND hwnd)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr (hwnd);
    return infoPtr->bUnicode;
}

static LRESULT
TAB_SetUnicodeFormat (HWND hwnd, WPARAM wParam)
{
    TAB_INFO *infoPtr = TAB_GetInfoPtr (hwnd);
    BOOL bTemp = infoPtr->bUnicode;

    infoPtr->bUnicode = (BOOL)wParam;

    return bTemp;
}

static LRESULT
TAB_Size (HWND hwnd, WPARAM wParam, LPARAM lParam)

{
/* I'm not really sure what the following code was meant to do.
   This is what it is doing:
   When WM_SIZE is sent with SIZE_RESTORED, the control
   gets positioned in the top left corner.

  RECT parent_rect;
  HWND parent;
  UINT uPosFlags,cx,cy;

  uPosFlags=0;
  if (!wParam) {
    parent = GetParent (hwnd);
    GetClientRect(parent, &parent_rect);
    cx=LOWORD (lParam);
    cy=HIWORD (lParam);
    if (GetWindowLongA(hwnd, GWL_STYLE) & CCS_NORESIZE)
        uPosFlags |= (SWP_NOSIZE | SWP_NOMOVE);

    SetWindowPos (hwnd, 0, parent_rect.left, parent_rect.top,
            cx, cy, uPosFlags | SWP_NOZORDER);
  } else {
    FIXME("WM_SIZE flag %x %lx not handled\n", wParam, lParam);
  } */

  /* Recompute the size/position of the tabs. */
  TAB_SetItemBounds (hwnd);

  /* Force a repaint of the control. */
  InvalidateRect(hwnd, NULL, TRUE);

  return 0;
}


static LRESULT
TAB_Create (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr;
  TEXTMETRICA fontMetrics;
  HDC hdc;
  HFONT hOldFont;
  DWORD dwStyle;

  infoPtr = (TAB_INFO *)COMCTL32_Alloc (sizeof(TAB_INFO));

  SetWindowLongA(hwnd, 0, (DWORD)infoPtr);

  infoPtr->uNumItem        = 0;
  infoPtr->uNumRows        = 0;
  infoPtr->hFont           = 0;
  infoPtr->items           = 0;
  infoPtr->hcurArrow       = LoadCursorA (0, IDC_ARROWA);
  infoPtr->iSelected       = -1;
  infoPtr->iHotTracked     = -1;
  infoPtr->uFocus          = -1;
  infoPtr->hwndToolTip     = 0;
  infoPtr->DoRedraw        = TRUE;
  infoPtr->needsScrolling  = FALSE;
  infoPtr->hwndUpDown      = 0;
  infoPtr->leftmostVisible = 0;
  infoPtr->fSizeSet	   = FALSE;
  infoPtr->bUnicode	   = IsWindowUnicode (hwnd);

  TRACE("Created tab control, hwnd [%04x]\n", hwnd);

  /* The tab control always has the WS_CLIPSIBLINGS style. Even
     if you don't specify it in CreateWindow. This is necessary in
     order for paint to work correctly. This follows windows behaviour. */
  dwStyle = GetWindowLongA(hwnd, GWL_STYLE);
  SetWindowLongA(hwnd, GWL_STYLE, dwStyle|WS_CLIPSIBLINGS);

  if (dwStyle & TCS_TOOLTIPS) {
    /* Create tooltip control */
    infoPtr->hwndToolTip =
      CreateWindowExA (0, TOOLTIPS_CLASSA, NULL, 0,
		       CW_USEDEFAULT, CW_USEDEFAULT,
		       CW_USEDEFAULT, CW_USEDEFAULT,
		       hwnd, 0, 0, 0);

    /* Send NM_TOOLTIPSCREATED notification */
    if (infoPtr->hwndToolTip) {
      NMTOOLTIPSCREATED nmttc;

      nmttc.hdr.hwndFrom = hwnd;
      nmttc.hdr.idFrom = GetWindowLongA(hwnd, GWL_ID);
      nmttc.hdr.code = NM_TOOLTIPSCREATED;
      nmttc.hwndToolTips = infoPtr->hwndToolTip;

      SendMessageA (GetParent (hwnd), WM_NOTIFY,
		    (WPARAM)GetWindowLongA(hwnd, GWL_ID), (LPARAM)&nmttc);
    }
  }

  /*
   * We need to get text information so we need a DC and we need to select
   * a font.
   */
  hdc = GetDC(hwnd);
  hOldFont = SelectObject (hdc, GetStockObject (SYSTEM_FONT));

  /* Use the system font to determine the initial height of a tab. */
  GetTextMetricsA(hdc, &fontMetrics);

  /*
   * Make sure there is enough space for the letters + growing the
   * selected item + extra space for the selected item.
   */
  infoPtr->tabHeight = fontMetrics.tmHeight + 2 * VERTICAL_ITEM_PADDING +
                       SELECTED_TAB_OFFSET;

  /* Initialize the width of a tab. */
  infoPtr->tabWidth = DEFAULT_TAB_WIDTH;

  SelectObject (hdc, hOldFont);
  ReleaseDC(hwnd, hdc);

  return 0;
}

static LRESULT
TAB_Destroy (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  TAB_INFO *infoPtr = TAB_GetInfoPtr(hwnd);
  INT iItem;

  if (!infoPtr)
      return 0;

  if (infoPtr->items) {
    for (iItem = 0; iItem < infoPtr->uNumItem; iItem++) {
      if (infoPtr->items[iItem].pszText)
	COMCTL32_Free (infoPtr->items[iItem].pszText);
    }
    COMCTL32_Free (infoPtr->items);
  }

  if (infoPtr->hwndToolTip)
    DestroyWindow (infoPtr->hwndToolTip);

  if (infoPtr->hwndUpDown)
    DestroyWindow(infoPtr->hwndUpDown);

  if (infoPtr->iHotTracked >= 0)
    KillTimer(hwnd, TAB_HOTTRACK_TIMER);

  COMCTL32_Free (infoPtr);
  SetWindowLongA(hwnd, 0, 0);
  return 0;
}

static LRESULT WINAPI
TAB_WindowProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    TRACE("hwnd=%x msg=%x wParam=%x lParam=%lx\n", hwnd, uMsg, wParam, lParam);
    if (!TAB_GetInfoPtr(hwnd) && (uMsg != WM_CREATE))
      return DefWindowProcA (hwnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case TCM_GETIMAGELIST:
      return TAB_GetImageList (hwnd, wParam, lParam);

    case TCM_SETIMAGELIST:
      return TAB_SetImageList (hwnd, wParam, lParam);

    case TCM_GETITEMCOUNT:
      return TAB_GetItemCount (hwnd, wParam, lParam);

    case TCM_GETITEMA:
      return TAB_GetItemA (hwnd, wParam, lParam);

    case TCM_GETITEMW:
      return TAB_GetItemW (hwnd, wParam, lParam);

    case TCM_SETITEMA:
      return TAB_SetItemA (hwnd, wParam, lParam);

    case TCM_SETITEMW:
      return TAB_SetItemW (hwnd, wParam, lParam);

    case TCM_DELETEITEM:
      return TAB_DeleteItem (hwnd, wParam, lParam);

    case TCM_DELETEALLITEMS:
     return TAB_DeleteAllItems (hwnd, wParam, lParam);

    case TCM_GETITEMRECT:
     return TAB_GetItemRect (hwnd, wParam, lParam);

    case TCM_GETCURSEL:
      return TAB_GetCurSel (hwnd);

    case TCM_HITTEST:
      return TAB_HitTest (hwnd, wParam, lParam);

    case TCM_SETCURSEL:
      return TAB_SetCurSel (hwnd, wParam);

    case TCM_INSERTITEMA:
      return TAB_InsertItemA (hwnd, wParam, lParam);

    case TCM_INSERTITEMW:
      return TAB_InsertItemW (hwnd, wParam, lParam);

    case TCM_SETITEMEXTRA:
      FIXME("Unimplemented msg TCM_SETITEMEXTRA\n");
      return 0;

    case TCM_ADJUSTRECT:
      return TAB_AdjustRect (hwnd, (BOOL)wParam, (LPRECT)lParam);

    case TCM_SETITEMSIZE:
      return TAB_SetItemSize (hwnd, wParam, lParam);

    case TCM_REMOVEIMAGE:
      FIXME("Unimplemented msg TCM_REMOVEIMAGE\n");
      return 0;

    case TCM_SETPADDING:
      FIXME("Unimplemented msg TCM_SETPADDING\n");
      return 0;

    case TCM_GETROWCOUNT:
      return TAB_GetRowCount(hwnd);

    case TCM_GETUNICODEFORMAT:
      return TAB_GetUnicodeFormat (hwnd);

    case TCM_SETUNICODEFORMAT:
      return TAB_SetUnicodeFormat (hwnd, wParam);

    case TCM_HIGHLIGHTITEM:
      FIXME("Unimplemented msg TCM_HIGHLIGHTITEM\n");
      return 0;

    case TCM_GETTOOLTIPS:
      return TAB_GetToolTips (hwnd, wParam, lParam);

    case TCM_SETTOOLTIPS:
      return TAB_SetToolTips (hwnd, wParam, lParam);

    case TCM_GETCURFOCUS:
      return TAB_GetCurFocus (hwnd);

    case TCM_SETCURFOCUS:
      return TAB_SetCurFocus (hwnd, wParam);

    case TCM_SETMINTABWIDTH:
      FIXME("Unimplemented msg TCM_SETMINTABWIDTH\n");
      return 0;

    case TCM_DESELECTALL:
      FIXME("Unimplemented msg TCM_DESELECTALL\n");
      return 0;

    case TCM_GETEXTENDEDSTYLE:
      FIXME("Unimplemented msg TCM_GETEXTENDEDSTYLE\n");
      return 0;

    case TCM_SETEXTENDEDSTYLE:
      FIXME("Unimplemented msg TCM_SETEXTENDEDSTYLE\n");
      return 0;

    case WM_GETFONT:
      return TAB_GetFont (hwnd, wParam, lParam);

    case WM_SETFONT:
      return TAB_SetFont (hwnd, wParam, lParam);

    case WM_CREATE:
      return TAB_Create (hwnd, wParam, lParam);

    case WM_NCDESTROY:
      return TAB_Destroy (hwnd, wParam, lParam);

    case WM_GETDLGCODE:
      return DLGC_WANTARROWS | DLGC_WANTCHARS;

    case WM_LBUTTONDOWN:
      return TAB_LButtonDown (hwnd, wParam, lParam);

    case WM_LBUTTONUP:
      return TAB_LButtonUp (hwnd, wParam, lParam);

    case WM_NOTIFY:
      return SendMessageA(GetParent(hwnd), WM_NOTIFY, wParam, lParam);

    case WM_RBUTTONDOWN:
      return TAB_RButtonDown (hwnd, wParam, lParam);

    case WM_MOUSEMOVE:
      return TAB_MouseMove (hwnd, wParam, lParam);

    case WM_ERASEBKGND:
      return TAB_EraseBackground (hwnd, (HDC)wParam);

    case WM_PAINT:
      return TAB_Paint (hwnd, wParam);

    case WM_SIZE:
      return TAB_Size (hwnd, wParam, lParam);

    case WM_SETREDRAW:
      return TAB_SetRedraw (hwnd, wParam);

    case WM_HSCROLL:
      return TAB_OnHScroll(hwnd, (int)LOWORD(wParam), (int)HIWORD(wParam), (HWND)lParam);

    case WM_STYLECHANGED:
      TAB_SetItemBounds (hwnd);
      InvalidateRect(hwnd, NULL, TRUE);
      return 0;

    case WM_KILLFOCUS:
    case WM_SETFOCUS:
      return TAB_FocusChanging(hwnd, uMsg, wParam, lParam);

    case WM_KEYUP:
      return TAB_KeyUp(hwnd, wParam);
    case WM_NCHITTEST:
      return TAB_NCHitTest(hwnd, lParam);

    default:
      if (uMsg >= WM_USER)
	WARN("unknown msg %04x wp=%08x lp=%08lx\n",
	     uMsg, wParam, lParam);
      return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}


VOID
TAB_Register (void)
{
  WNDCLASSA wndClass;

  ZeroMemory (&wndClass, sizeof(WNDCLASSA));
  wndClass.style         = CS_GLOBALCLASS | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
  wndClass.lpfnWndProc   = (WNDPROC)TAB_WindowProc;
  wndClass.cbClsExtra    = 0;
  wndClass.cbWndExtra    = sizeof(TAB_INFO *);
  wndClass.hCursor       = LoadCursorA (0, IDC_ARROWA);
  wndClass.hbrBackground = (HBRUSH)NULL;
  wndClass.lpszClassName = WC_TABCONTROLA;

  RegisterClassA (&wndClass);
}


VOID
TAB_Unregister (void)
{
    UnregisterClassA (WC_TABCONTROLA, (HINSTANCE)NULL);
}
