/*
 *  System tray handling code (client side)
 *
 *  Copyright 1999 Kai Morich     <kai.morich@bigfoot.de>
 *  Copyright 2003 Mike Hearn     <mike@theoretic.com>
 *
 * This code creates a window with the WS_EX_TRAYWINDOW style. The actual
 * environment integration code is handled inside the X11 driver.
 *
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "shlobj.h"
#include "shellapi.h"
#include "shell32_main.h"
#include "commctrl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);
WINE_DECLARE_DEBUG_CHANNEL(systray);

typedef struct SystrayItem {
  HWND                  hWnd;
  HWND                  hWndToolTip;
  NOTIFYICONDATAA       notifyIcon;
  CRITICAL_SECTION      lock;
  struct SystrayItem    *nextTrayItem;
} SystrayItem;

static SystrayItem *systray=NULL;
static int firstSystray=TRUE; /* defer creation of window class until first systray item is created */

static BOOL SYSTRAY_Delete(PNOTIFYICONDATAA pnid);


#define ICON_SIZE GetSystemMetrics(SM_CXSMICON)
/* space around icon (forces icon to center of KDE systray area) */
#define ICON_BORDER  4



static BOOL SYSTRAY_ItemIsEqual(PNOTIFYICONDATAA pnid1, PNOTIFYICONDATAA pnid2)
{
  if (pnid1->hWnd != pnid2->hWnd) return FALSE;
  if (pnid1->uID  != pnid2->uID)  return FALSE;
  return TRUE;
}

static LRESULT CALLBACK SYSTRAY_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  HDC hdc;
  PAINTSTRUCT ps;

  TRACE_(systray)("hwnd=%08x, msg=0x%x\n", hWnd, message);
  switch (message) {
  case WM_PAINT:
  {
    RECT rc;
    SystrayItem  *ptrayItem = systray;

    int top;
    EnterCriticalSection(&ptrayItem->lock);

    while (ptrayItem) {
      if (ptrayItem->hWnd==hWnd) {
	if (ptrayItem->notifyIcon.hIcon) {
	  hdc = BeginPaint(hWnd, &ps);
	  GetClientRect(hWnd, &rc);
	  /* calculate top so we can deal with arbitrary sized trays */
	  top = ((rc.bottom-rc.top)/2) - ((ICON_SIZE)/2);
	  if (!DrawIconEx(hdc, (ICON_BORDER/2), top, ptrayItem->notifyIcon.hIcon,
			  ICON_SIZE, ICON_SIZE, 0, 0, DI_DEFAULTSIZE|DI_NORMAL)) {
	    ERR("Paint(SystrayWindow 0x%08x) failed -> removing SystrayItem %p\n", hWnd, ptrayItem);
	    LeaveCriticalSection(&ptrayItem->lock);
	    SYSTRAY_Delete(&ptrayItem->notifyIcon);
	  }
	}
	break;
      }
      ptrayItem = ptrayItem->nextTrayItem;
    }
    EndPaint(hWnd, &ps);
    LeaveCriticalSection(&ptrayItem->lock);
  }
  break;

  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
  {
    MSG msg;
    SystrayItem *ptrayItem = systray;

    /* relay the event to the tooltip */
    while ( ptrayItem ) {
      if (ptrayItem->hWnd == hWnd) {
        msg.hwnd=hWnd;
        msg.message=message;
        msg.wParam=wParam;
        msg.lParam=lParam;
        msg.time = GetMessageTime ();
        msg.pt.x = LOWORD(GetMessagePos ());
        msg.pt.y = HIWORD(GetMessagePos ());

        SendMessageA(ptrayItem->hWndToolTip, TTM_RELAYEVENT, 0, (LPARAM)&msg);
      }
      ptrayItem = ptrayItem->nextTrayItem;
    }
  }
  /* fall through, so the message is sent to the callback as well */

  case WM_LBUTTONDBLCLK:
  case WM_RBUTTONDBLCLK:
  case WM_MBUTTONDBLCLK:
  {
    SystrayItem *ptrayItem = systray;

    /* iterate over the currently active tray items */
    while (ptrayItem) {
      if (ptrayItem->hWnd == hWnd) {
	if (ptrayItem->notifyIcon.hWnd && ptrayItem->notifyIcon.uCallbackMessage) {
          if (!PostMessageA(ptrayItem->notifyIcon.hWnd, ptrayItem->notifyIcon.uCallbackMessage,
                            (WPARAM)ptrayItem->notifyIcon.uID, (LPARAM)message)) {
	      ERR("PostMessage(SystrayWindow 0x%08x) failed -> removing SystrayItem %p\n", hWnd, ptrayItem);
	      SYSTRAY_Delete(&ptrayItem->notifyIcon);
	    }
        }
	break;
      }
      ptrayItem = ptrayItem->nextTrayItem;
    }
  }
  break;

  case WM_NOTIFYFORMAT:
  {
      TRACE_(systray)("Received WM_NOTIFYFORMAT, showing the tray window\n");
      ShowWindow(hWnd, SW_SHOW);
      return (DefWindowProcA(hWnd, message, wParam, lParam));
  }

  default:
    return (DefWindowProcA(hWnd, message, wParam, lParam));
  }
  return (0);

}


BOOL SYSTRAY_RegisterClass(void)
{
  WNDCLASSA  wc;

  wc.style         = CS_SAVEBITS;
  wc.lpfnWndProc   = (WNDPROC)SYSTRAY_WndProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = 0;
  wc.hIcon         = 0;
  wc.hCursor       = LoadCursorA(0, IDC_ARROWA);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = "WineSystray";

  if (!RegisterClassA(&wc)) {
    ERR("RegisterClass(WineSystray) failed\n");
    return FALSE;
  }
  return TRUE;
}


DWORD WINAPI SYSTRAY_ThreadProc(LPVOID p1)
{
  SystrayItem *ptrayItem = (SystrayItem *)p1;
  MSG msg;
  RECT rect;

  /* Initialize the window size. */
  rect.left   = 0;
  rect.top    = 0;
  rect.right  = ICON_SIZE+2*ICON_BORDER;
  rect.bottom = ICON_SIZE+2*ICON_BORDER;

  TRACE_(systray)("welcome to the thread proc\n");

  /* Create tray window for icon. */
  ptrayItem->hWnd = CreateWindowExA( WS_EX_TRAYWINDOW,
                                "WineSystray", "Wine-Systray",
                                0,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right-rect.left, rect.bottom-rect.top,
                                0, 0, 0, 0 );
  if ( !ptrayItem->hWnd ) {
    ERR( "CreateWindow(WineSystray) failed\n" );
    goto done;
  }

  TRACE_(systray)("created a window\n");

  /* Create tooltip for icon. */
  ptrayItem->hWndToolTip = CreateWindowA( TOOLTIPS_CLASSA,NULL,TTS_ALWAYSTIP,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     ptrayItem->hWnd, 0, 0, 0 );
  if ( !ptrayItem->hWndToolTip ) {
    ERR( "CreateWindow(TOOLTIP) failed\n" );
    goto done;
  }
  TRACE_(systray)("created a (TOOLTIP) window\n");

  while (GetMessageA (&msg, 0, 0, 0) > 0) {
    TranslateMessage (&msg);
    DispatchMessageA (&msg);
  }

done:
  TRACE_(systray)("Shutting down system tray thread\n");

  if(ptrayItem->notifyIcon.hIcon)
     DestroyIcon(ptrayItem->notifyIcon.hIcon);
  if(ptrayItem->hWndToolTip)
     DestroyWindow(ptrayItem->hWndToolTip);
  if(ptrayItem->hWnd)
     DestroyWindow(ptrayItem->hWnd);

  return 0;
}

BOOL SYSTRAY_ItemInit(SystrayItem *ptrayItem)
{
  DWORD threadID;
  HANDLE hThread;

  /* Register the class if this is our first tray item. */
  if ( firstSystray ) {
    firstSystray = FALSE;
    if ( !SYSTRAY_RegisterClass() ) {
      ERR( "RegisterClass(WineSystray) failed\n" );
      return FALSE;
    }
  }

  ZeroMemory( ptrayItem, sizeof(SystrayItem) );

  InitializeCriticalSection(&ptrayItem->lock);

  /* We need to run the system tray window in a separate thread, as otherwise if the originating thread
     stops processing messages, the tray window will hang. If another part of the application then does
     for instance a FindWindow call, this can deadlock the application. */
  hThread = CreateThread(NULL, 0, SYSTRAY_ThreadProc, (LPVOID) ptrayItem, 0, &threadID);
  if (!hThread) {
    ERR("Could not create system tray item thread\n");
    return FALSE;
  }

  CloseHandle(hThread);

  return TRUE;
}

static void SYSTRAY_ItemTerm(SystrayItem *ptrayItem)
{
  /* MSDN says we shouldn't do this, but I can't see another way to make GetMessage() return zero */
  PostMessageA(ptrayItem->hWnd, WM_QUIT, 0, 0);
  DeleteCriticalSection(&ptrayItem->lock);
  return;
}


void SYSTRAY_ItemSetMessage(SystrayItem *ptrayItem, UINT uCallbackMessage)
{
  EnterCriticalSection(&ptrayItem->lock);
    ptrayItem->notifyIcon.uCallbackMessage = uCallbackMessage;
  LeaveCriticalSection(&ptrayItem->lock);
}


void SYSTRAY_ItemSetIcon(SystrayItem *ptrayItem, HICON hIcon)
{
  EnterCriticalSection(&ptrayItem->lock);
    ptrayItem->notifyIcon.hIcon = CopyIcon(hIcon);
  LeaveCriticalSection(&ptrayItem->lock);

  InvalidateRect(ptrayItem->hWnd, NULL, TRUE);
}


void SYSTRAY_ItemSetTip(SystrayItem *ptrayItem, CHAR* szTip, int modify)
{
  TTTOOLINFOA ti;

  EnterCriticalSection(&ptrayItem->lock);
    strncpy(ptrayItem->notifyIcon.szTip, szTip, sizeof(ptrayItem->notifyIcon.szTip));
    ptrayItem->notifyIcon.szTip[sizeof(ptrayItem->notifyIcon.szTip)-1] = 0;
  LeaveCriticalSection(&ptrayItem->lock);

  ti.cbSize = sizeof(TTTOOLINFOA);
  ti.uFlags = 0;
  ti.hwnd = ptrayItem->hWnd;
  ti.hinst = 0;
  ti.uId = 0;
  ti.lpszText = ptrayItem->notifyIcon.szTip;
  ti.rect.left   = 0;
  ti.rect.top    = 0;
  ti.rect.right  = ICON_SIZE+2*ICON_BORDER;
  ti.rect.bottom = ICON_SIZE+2*ICON_BORDER;

  if(modify)
    SendMessageA(ptrayItem->hWndToolTip, TTM_UPDATETIPTEXTA, 0, (LPARAM)&ti);
  else
    SendMessageA(ptrayItem->hWndToolTip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}


static BOOL SYSTRAY_Add(PNOTIFYICONDATAA pnid)
{
  SystrayItem **ptrayItem = &systray;

  /* Find last element. */
  while( *ptrayItem ) {
    if ( SYSTRAY_ItemIsEqual(pnid, &(*ptrayItem)->notifyIcon) )
      return FALSE;
    ptrayItem = &((*ptrayItem)->nextTrayItem);
  }
  /* Allocate SystrayItem for element and add to end of list. */
  (*ptrayItem) = ( SystrayItem *)HeapAlloc( GetProcessHeap(), 0, sizeof(SystrayItem) );

  /* Initialize and set data for the tray element. */
  SYSTRAY_ItemInit( (*ptrayItem) ); /* FIXME: Should handle failure */

  (*ptrayItem)->notifyIcon.uID = pnid->uID; /* only needed for callback message */
  (*ptrayItem)->notifyIcon.hWnd = pnid->hWnd; /* only needed for callback message */
  SYSTRAY_ItemSetIcon   (*ptrayItem, (pnid->uFlags&NIF_ICON)   ?pnid->hIcon           :0);
  SYSTRAY_ItemSetMessage(*ptrayItem, (pnid->uFlags&NIF_MESSAGE)?pnid->uCallbackMessage:0);
  SYSTRAY_ItemSetTip    (*ptrayItem, (pnid->uFlags&NIF_TIP)    ?pnid->szTip           :"", FALSE);

  TRACE_(systray)("%p: 0x%08x %s\n",  (*ptrayItem), (*ptrayItem)->notifyIcon.hWnd,
                                                    (*ptrayItem)->notifyIcon.szTip);
  return TRUE;
}


static BOOL SYSTRAY_Modify(PNOTIFYICONDATAA pnid)
{
  SystrayItem *ptrayItem = systray;

  while ( ptrayItem ) {
    if ( SYSTRAY_ItemIsEqual(pnid, &ptrayItem->notifyIcon) ) {
      if (pnid->uFlags & NIF_ICON)
        SYSTRAY_ItemSetIcon(ptrayItem, pnid->hIcon);
      if (pnid->uFlags & NIF_MESSAGE)
        SYSTRAY_ItemSetMessage(ptrayItem, pnid->uCallbackMessage);
      if (pnid->uFlags & NIF_TIP)
        SYSTRAY_ItemSetTip(ptrayItem, pnid->szTip, TRUE);

      TRACE_(systray)("%p: 0x%08x %s\n", ptrayItem, ptrayItem->notifyIcon.hWnd, ptrayItem->notifyIcon.szTip);
      return TRUE;
    }
    ptrayItem = ptrayItem->nextTrayItem;
  }
  return FALSE; /* not found */
}


static BOOL SYSTRAY_Delete(PNOTIFYICONDATAA pnid)
{
  SystrayItem **ptrayItem = &systray;

  while (*ptrayItem) {
    if (SYSTRAY_ItemIsEqual(pnid, &(*ptrayItem)->notifyIcon)) {
      SystrayItem *next = (*ptrayItem)->nextTrayItem;
      TRACE_(systray)("%p: 0x%08x %s\n", *ptrayItem, (*ptrayItem)->notifyIcon.hWnd, (*ptrayItem)->notifyIcon.szTip);
      SYSTRAY_ItemTerm(*ptrayItem);

      HeapFree(GetProcessHeap(), 0, *ptrayItem);
      *ptrayItem = next;

      return TRUE;
    }
    ptrayItem = &((*ptrayItem)->nextTrayItem);
  }

  return FALSE; /* not found */
}

/*************************************************************************
 *
 */
BOOL SYSTRAY_Init(void)
{
  return TRUE;
}

/*************************************************************************
 * Shell_NotifyIcon			[SHELL32.296]
 * Shell_NotifyIconA			[SHELL32.297]
 */
BOOL WINAPI Shell_NotifyIconA(DWORD dwMessage, PNOTIFYICONDATAA pnid )
{
  BOOL flag=FALSE;
  TRACE_(systray)("enter %d %d %ld\n", pnid->hWnd, pnid->uID, dwMessage);
  switch(dwMessage) {
  case NIM_ADD:
    flag = SYSTRAY_Add(pnid);
    break;
  case NIM_MODIFY:
    flag = SYSTRAY_Modify(pnid);
    break;
  case NIM_DELETE:
    flag = SYSTRAY_Delete(pnid);
    break;
  }
  TRACE_(systray)("leave %d %d %ld=%d\n", pnid->hWnd, pnid->uID, dwMessage, flag);
  return flag;
}

/*************************************************************************
 * Shell_NotifyIconW			[SHELL32.298]
 */
BOOL WINAPI Shell_NotifyIconW (DWORD dwMessage, PNOTIFYICONDATAW pnid )
{
	BOOL ret;

	PNOTIFYICONDATAA p = HeapAlloc(GetProcessHeap(),0,sizeof(NOTIFYICONDATAA));
	memcpy(p, pnid, sizeof(NOTIFYICONDATAA));
        WideCharToMultiByte( CP_ACP, 0, pnid->szTip, -1, p->szTip, sizeof(p->szTip), NULL, NULL );
        p->szTip[sizeof(p->szTip)-1] = 0;

	ret = Shell_NotifyIconA(dwMessage, p );

	HeapFree(GetProcessHeap(),0,p);
	return ret;
}
