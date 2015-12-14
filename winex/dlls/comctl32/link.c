/*
 * SysLink class
 *
 * Copyright 2003 TransGaming Technologies
 *
 * David Hammerton
 *
 */

/*
 * TODO:
 *   Word wrapping
 *   Keyboard selection (tab / enter)
 *   Right click
 *   Some of the messages
 */

#include <string.h>
#include "winbase.h"
#include "commctrl.h"
#include "shellapi.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(link);

typedef struct tagLINK_ITEM LINK_ITEM; /* internal - different from LITEM */
struct tagLINK_ITEM
{
    UINT linkOffset;
    UINT linkLen;
    UINT state;
    BOOL hasID;
    BOOL hasUrl;
    WCHAR szID[MAX_LINKID_TEXT];
    WCHAR szUrl[L_MAX_URL_LENGTH];

    INT cachedIDInList;

    INT x_offset, x_size;
    INT y_size;

    LINK_ITEM *next;
};

typedef HINSTANCE (WINAPI *LPF_SHELLEXECUTEW)(HWND,LPCWSTR,LPCWSTR,
                                              LPCWSTR,LPCWSTR,INT);

typedef struct
{
    WCHAR *lpText;
    WCHAR *lpPlainText;

    HFONT hStandardFont;
    HFONT hLinkFont;

    HCURSOR hHandCursor;
    HCURSOR hArrowCursor;

    BOOL hoverCursor;

    HMODULE hShell32;
    LPF_SHELLEXECUTEW pShellExecuteW;

    LINK_ITEM *linkItemHead;
} LINK_INFO;

#define LINK_GetInfoPtr(hwnd) ((LINK_INFO *)GetWindowLongW(hwnd, 0))

static LOGFONTW lfNormal =
{
    12, /* height */
    0, /* width */
    0, 0, FW_NORMAL,
    FALSE,
    FALSE, /* underline */
    FALSE,
    DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE,
    {'S', 'y', 's', 't', 'e', 'm', 0}
};

static LOGFONTW lfLink =
{
    12, /* height */
    0, /* width */
    0, 0, FW_NORMAL,
    FALSE,
    TRUE, /* underline */
    FALSE,
    DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE,
    {'S', 'y', 's', 't', 'e', 'm', 0}
};

static void LINK_DrawText(LINK_INFO *infoPtr, HDC hdc, BOOL metricsOnly)
{
    int offset = 0, offsetSize = 0;
    LINK_ITEM *curItem = infoPtr->linkItemHead;
    WCHAR *p = infoPtr->lpPlainText;
    HFONT prevFont;
    COLORREF prevColour;
    int prevMapping;
    HFONT dirtyProperties = FALSE;

    if (curItem && curItem->linkOffset == 0)
    {
        prevFont = SelectObject(hdc, infoPtr->hLinkFont);
        prevColour = SetTextColor(hdc, RGB(0, 0, 255));
    }
    else
    {
        prevFont = SelectObject(hdc, infoPtr->hStandardFont);
        prevColour = SetTextColor(hdc, RGB(0, 0, 0));
    }
    prevMapping = SetMapMode(hdc, MM_TEXT);

    while (*p)
    {
        int strLen;
        BOOL link = FALSE;
        LINK_ITEM *prevItem = FALSE;

        if (curItem && curItem->linkOffset == offset)
        {
            if (dirtyProperties)
            {
                SetTextColor(hdc, RGB(0, 0, 255));
                SelectObject(hdc, infoPtr->hLinkFont);
            }

            if (curItem->next &&
                    (curItem->linkOffset + curItem->linkLen) == curItem->next->linkOffset)
                dirtyProperties = FALSE;
            else
                dirtyProperties = TRUE;

            strLen = curItem->linkLen;

            prevItem = curItem;
            curItem = curItem->next;

            link = TRUE;
        }
        else
        {
            if (dirtyProperties)
            {
                SetTextColor(hdc, RGB(0, 0, 0));
                SelectObject(hdc, infoPtr->hStandardFont);
            }
            dirtyProperties = TRUE;

            if (curItem) strLen = curItem->linkOffset - offset;
            else strLen = strlenW(p);
        }

        if (!metricsOnly)
        {
            if (link)
                offsetSize = prevItem->x_offset;

            TRACE("(%s) calling TextOut: %s, %i  at % 4i\n", (link ? "LINK  " : "NORMAL"),
                  debugstr_wn(p, strLen), strLen, offsetSize);
            TextOutW(hdc, offsetSize, 0, p, strLen);

            if (link)
                offsetSize += prevItem->x_size;
        }
        else
        {
            SIZE size;
            size.cx = size.cy = 0;
            GetTextExtentPoint32W(hdc, p, strLen, &size);
            TRACE("string %s has size: %li\n", debugstr_wn(p, strLen), size.cx);

            if (link)
            {
                prevItem->x_size = size.cx;
                prevItem->x_offset = offsetSize;
                prevItem->y_size = size.cy;
            }

            offsetSize += size.cx;
        }

        p += strLen;
        offset += strLen;
    }
    if (curItem)
    {
        ERR("didn't print all links! (offset: %i)\n", curItem->linkOffset);
    }

    SetTextColor(hdc, prevColour);
    SelectObject(hdc, prevFont);
    SetMapMode(hdc, prevMapping);
}

static LRESULT LINK_Paint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    LINK_INFO *infoPtr = LINK_GetInfoPtr(hwnd);
    PAINTSTRUCT ps;
    HDC hdc = (HDC)wParam;
    TRACE("(%08x, %08x, %08lx)\n", hwnd, wParam, lParam);

    if (!wParam)
        hdc = BeginPaint(hwnd, &ps);

    LINK_DrawText(infoPtr, hdc, FALSE);

    if (!wParam)
        EndPaint(hwnd, &ps);

    return 0;
}

static const WCHAR lpszHREF[] = {'H', 'R', 'E', 'F', 0};
static const WCHAR lpszID[] = {'I', 'D', 0};

#define JUMP_SPACES(p) do { \
   while (isspaceW(*p)) \
   { \
       p++; \
       if (!(*p)) goto error; \
   } } while (0)

/* this is given something like:
 *    ' HREF="foo" ID="bar">etc etc etc'
 *  and puts foo into szUrl, bar into ID.
 *  returns NULL if an error occured (like reached the end of the string
 *  without hitting the >
 */
static LINK_ITEM *LINK_CreateItem(UINT offset, WCHAR *remText, WCHAR **newRemText)
{
    LINK_ITEM *newItem = COMCTL32_Alloc(sizeof(LINK_ITEM));
    WCHAR *p = remText;
    int i;

    newItem->linkOffset = offset;
    newItem->state = LIS_ENABLED;

    while (*p)
    {
        if (strncmpW(p, lpszHREF, 4) == 0)
        {
            p += 4;
            JUMP_SPACES(p);
            if ((*p) != (WCHAR)'=') goto error;
            p++;
            JUMP_SPACES(p);
            if ((*p) != (WCHAR)'"') goto error;
            p++;
            i = 0;
            while (*p != '"')
            {
                newItem->szUrl[i++] = *p;
                if (i >= L_MAX_URL_LENGTH) goto error;
                p++;
                if (!(*p)) goto error;
            }
            newItem->szUrl[i++] = (WCHAR)0;
            newItem->hasUrl = TRUE;
        }
        else if (strncmpW(p, lpszID, 2) == 0)
        {
            p += 2;
            JUMP_SPACES(p);
            if ((*p) != (WCHAR)'=') goto error;
            p++;
            JUMP_SPACES(p);
            if ((*p) != (WCHAR)'"') goto error;
            p++;
            i = 0;
            while (*p != '"')
            {
                newItem->szID[i++] = *p;
                if (i >= MAX_LINKID_TEXT) goto error;
                p++;
                if (!(*p)) goto error;
            }
            newItem->szID[i++] = (WCHAR)0;
            newItem->hasID = TRUE;
        }
        else if (*p == (WCHAR)'>')
        {
            *newRemText = p;
            return newItem;
        }
        else if (!isspaceW(*p))
        {
            WARN("invalid char in <A tag!: %s\n", debugstr_wn(p, 1));
        }
        p++;
    }
error:
    COMCTL32_Free(newItem);
    return NULL;
}

static void LINK_DeleteAllItems(LINK_INFO *infoPtr)
{
    while (infoPtr->linkItemHead)
    {
        LINK_ITEM *liCur = infoPtr->linkItemHead;
        infoPtr->linkItemHead = liCur->next;

        COMCTL32_Free(liCur);
    }
}

static BOOL LINK_UpdateText(HWND hwnd, const WCHAR *lpText)
{
    LINK_INFO *infoPtr = LINK_GetInfoPtr(hwnd);
    WCHAR *p;
    LINK_ITEM *curItem = NULL;
    LINK_ITEM *prevItem = NULL;
    int i;

    TRACE("(%08x, %s)\n", hwnd, debugstr_wn(lpText, strlenW(lpText)));

    if (infoPtr->lpText) COMCTL32_Free(infoPtr->lpText);
    infoPtr->lpText = COMCTL32_Alloc((strlenW(lpText) + 1) * sizeof(WCHAR));

    if (infoPtr->lpPlainText) COMCTL32_Free(infoPtr->lpPlainText);
    infoPtr->lpPlainText = COMCTL32_Alloc((strlenW(lpText) + 1) * sizeof(WCHAR));

    strcpyW(infoPtr->lpText, lpText);

    LINK_DeleteAllItems(infoPtr);

    p = infoPtr->lpText;
    i = 0;
    /* this while loop copies the contents of lpText info lpPlainText,
     * without the <A> tags.
     * It also creates a list of LINK_INFO structures from the tags.
     * This list is placed into the infoPtr structure.
     */
    while (*p)
    {
        BOOL recordChar = TRUE;
        if (!curItem)
        { /* looking for a <A */
            if (*p == (WCHAR)'<' &&
                    toupperW(*(p+1)) == (WCHAR)'A')
            {
                LINK_ITEM *newItem = LINK_CreateItem(i, p+2, &p);
                if (!newItem)
                    goto error;
                curItem = newItem;
                recordChar = FALSE;
            }
            else if (*p == (WCHAR)'<')
                WARN("possible out-of-order or unknown tag in link string");
        }
        else
        {
            if (*p == (WCHAR)'<' &&
                    *(p+1) == (WCHAR)'/' &&
                    toupperW(*(p+2)) == (WCHAR)'A' &&
                    *(p+3) == '>')
            {
                if (prevItem)
                {
                    curItem->cachedIDInList = prevItem->cachedIDInList + 1;
                    prevItem->next = curItem;
                }
                else
                {
                    curItem->cachedIDInList = 0;
                    infoPtr->linkItemHead = curItem;
                }
                prevItem = curItem;
                curItem = NULL;
                recordChar = FALSE;
                p+=3;
            }
            else
            {
                if (*p == (WCHAR)'<')
                    WARN("possible embedded tag in link string");
                curItem->linkLen++;
            }
        }
        if (recordChar)
        {
            infoPtr->lpPlainText[i++] = *p;
        }
        p++;
    }
    infoPtr->lpPlainText[i] = (WCHAR)0;
    if (curItem)
    {
        WARN("last link never closed\n");
        COMCTL32_Free(curItem);
    }
    LINK_DrawText(infoPtr, GetDC(hwnd), TRUE);
    return TRUE;

error:
    ERR("an error occured while processing the link: '%s'\n", debugstr_wn(lpText, strlenW(lpText)));

    COMCTL32_Free(infoPtr->lpText);
    infoPtr->lpText = NULL;
    COMCTL32_Free(infoPtr->lpPlainText);
    infoPtr->lpPlainText = NULL;
    LINK_DeleteAllItems(infoPtr);

    return FALSE;
}

static LINK_ITEM *LINK_HitTest_Int(LINK_INFO *infoPtr, POINT *pt)
{
    LINK_ITEM *curItem = infoPtr->linkItemHead;

    while (curItem)
    {
        if (pt->x < curItem->x_offset)
            return NULL; /* not a hit */
        if (pt->x <= (curItem->x_offset + curItem->x_size) &&
                pt->y <= curItem->y_size /* starts at 0 */)
        {
            return curItem;
        }
        curItem = curItem->next;
    }
    return NULL;
}

static void LINK_CreateLItemFromLinkItem(const LINK_ITEM *liItem, LITEM *litem)
{
    litem->mask = 0; /* FIXME: test on windows */
    litem->iLink = liItem->cachedIDInList;
    litem->state = liItem->state;
    litem->stateMask = 0; /* as above (mask) */
    lstrcpynW(litem->szID, liItem->szID, MAX_LINKID_TEXT);
    lstrcpynW(litem->szUrl, liItem->szUrl, L_MAX_URL_LENGTH);
}

static LRESULT LINK_LButtonUp(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    LINK_INFO *infoPtr = LINK_GetInfoPtr(hwnd);

    POINT pt = { SLOWORD(lParam), SHIWORD(lParam) };

    LINK_ITEM *hitItem = LINK_HitTest_Int(infoPtr, &pt);

    if (hitItem)
    {
        HWND hwndParent = GetParent(hwnd);
        if (hitItem->hasUrl)
        {
            infoPtr->pShellExecuteW(0, NULL, hitItem->szUrl, NULL, NULL, SW_SHOWNORMAL);
        }

        /* not sure if this is right */
        if (hwndParent)
        {
            NMLINK nmlink;
            nmlink.hdr.hwndFrom = hwnd;
            nmlink.hdr.hwndFrom = hwnd; /* id from - eh? FIXME */
            nmlink.hdr.code = NM_CLICK;
            LINK_CreateLItemFromLinkItem(hitItem, &nmlink.item);
            SendMessageW(hwndParent, WM_NOTIFY, (WPARAM)hwnd, (LPARAM)&nmlink);
        }
    }

    return TRUE;
}

static LRESULT LINK_MouseMove(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
#if 0
    LINK_INFO *infoPtr = LINK_GetInfoPtr(hwnd);

    POINT pt = { SLOWORD(lParam), SHIWORD(lParam) };

    LINK_ITEM *hitItem = LINK_HitTest_Int(infoPtr, &pt);

    /* FIXME: why isn't this working? */
    if (hitItem && !infoPtr->hoverCursor)
    {
        SetClassLongW(hwnd, GCL_HCURSOR, (LONG) infoPtr->hHandCursor);
        infoPtr->hoverCursor = TRUE;
    }
    else if (!hitItem && infoPtr->hoverCursor)
    {
        SetClassLongW(hwnd, GCL_HCURSOR, (LONG) infoPtr->hArrowCursor );
        infoPtr->hoverCursor = FALSE;
    }
#endif
    return TRUE;
}

static LRESULT LINK_Create(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    LINK_INFO *infoPtr;
    INT len;
    WCHAR *windowText;

    TRACE("(%08x, %08x, %08lx)\n", hwnd, wParam, lParam);

    infoPtr = (LINK_INFO *) COMCTL32_Alloc(sizeof(LINK_INFO));
    if (!infoPtr) return -1;
    SetWindowLongW(hwnd, 0, (DWORD)infoPtr);

    infoPtr->hStandardFont = CreateFontIndirectW(&lfNormal);
    infoPtr->hLinkFont = CreateFontIndirectW(&lfLink);

    infoPtr->hHandCursor = LoadCursorW(0, IDC_HANDW);
    infoPtr->hHandCursor = LoadCursorW(0, IDC_ARROWW);

    len = GetWindowTextLengthW(hwnd);
    windowText = COMCTL32_Alloc(sizeof(WCHAR) * (len + 1));
    GetWindowTextW(hwnd, windowText, len + 1);
    LINK_UpdateText(hwnd, windowText);
    COMCTL32_Free(windowText);

    infoPtr->hShell32 = LoadLibraryA("shell32.dll");
    infoPtr->pShellExecuteW = (LPF_SHELLEXECUTEW)GetProcAddress(infoPtr->hShell32,
                                                                "ShellExecuteW");

    return 0;
}

static LRESULT LINK_Destroy(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    LINK_INFO *infoPtr = LINK_GetInfoPtr(hwnd);

    TRACE("(%08x, %08x, %08lx)\n", hwnd, wParam, lParam);

    DeleteObject(infoPtr->hStandardFont);
    DeleteObject(infoPtr->hLinkFont);
    DeleteObject(infoPtr->hHandCursor);
    DeleteObject(infoPtr->hArrowCursor);

    FreeLibrary(infoPtr->hShell32);

    COMCTL32_Free(infoPtr->lpText);
    COMCTL32_Free(infoPtr);
    SetWindowLongW(hwnd, 0, 0);

    return 0;
}

static LRESULT LINK_SetItem(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /*LITEM *litem = (LITEM*)lParam;*/
    FIXME("stub\n");

    return TRUE;
}

static LRESULT LINK_GetItem(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /*LITEM *litem = (LITEM*)lParam;*/
    FIXME("stub\n");

    return TRUE;
}

static LRESULT LINK_GetIdealHeight(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    FIXME("stub\n");
    return (LRESULT)16;
}

static LRESULT LINK_HitTest(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    /*LHITTESTINFO *lhittestinfo = (LITEM*)lParam;*/

    return FALSE;
}

static LRESULT WINAPI
LINK_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!LINK_GetInfoPtr(hwnd) && (uMsg != WM_CREATE))
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
        case WM_CREATE:
            return LINK_Create(hwnd, wParam, lParam);

        case WM_DESTROY:
            return LINK_Destroy(hwnd, wParam, lParam);

        case WM_PAINT:
            return LINK_Paint(hwnd, wParam, lParam);

        case WM_LBUTTONUP:
            return LINK_LButtonUp(hwnd, wParam, lParam);

        case WM_MOUSEMOVE:
            return LINK_MouseMove(hwnd, wParam, lParam);

        case LM_SETITEM:
            return LINK_SetItem(hwnd, wParam, lParam);

        case LM_GETITEM:
            return LINK_GetItem(hwnd, wParam, lParam);

        case LM_GETIDEALHEIGHT:
            return LINK_GetIdealHeight(hwnd, wParam, lParam);

        case LM_HITTEST:
            return LINK_HitTest(hwnd, wParam, lParam);

        default:
            if (uMsg >= WM_USER)
                ERR("unknown message: msg %04x, wp: %08x, lp: %08lx\n",
                      uMsg, wParam, lParam);
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void LINK_Register(void)
{
    WNDCLASSW wndClass;

    ZeroMemory (&wndClass, sizeof(WNDCLASSW));
    wndClass.style         = CS_GLOBALCLASS;
    wndClass.lpfnWndProc   = (WNDPROC)LINK_WindowProc;
    wndClass.cbClsExtra    = 0;
    wndClass.cbWndExtra    = sizeof(LINK_INFO *);
    wndClass.hCursor       = LoadCursorW(0, IDC_ARROWW);
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszClassName = WC_LINKW;

    RegisterClassW (&wndClass);
}

void LINK_Unregister(void)
{
    UnregisterClassW(WC_LINKW, (HINSTANCE)NULL);
}

