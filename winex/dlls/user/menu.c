/*
 * Menu functions
 *
 * Copyright 1993 Martin Ayotte
 * Copyright 1994 Alexandre Julliard
 * Copyright 1997 Morten Welinder
 */

/*
 * Note: the style MF_MOUSESELECT is used to mark popup items that
 * have been selected, i.e. their popup menu is currently displayed.
 * This is probably not the meaning this style has in MS-Windows.
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winnls.h"
#include "wingdi.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "wine/unicode.h"
#include "win.h"
#include "controls.h"
#include "nonclient.h"
#include "user.h"
#include "message.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(menu);
WINE_DECLARE_DEBUG_CHANNEL(accel);

#define HACCEL_16(h32)		(LOWORD(h32))

/* internal popup menu window messages */

#define MM_SETMENUHANDLE	(WM_USER + 0)
#define MM_GETMENUHANDLE	(WM_USER + 1)

/* Menu item structure */
typedef struct {
    /* ----------- MENUITEMINFO Stuff ----------- */
    UINT fType;			/* Item type. */
    UINT fState;		/* Item state.  */
    UINT wID;			/* Item id.  */
    HMENU hSubMenu;		/* Pop-up menu.  */
    HBITMAP hCheckBit;		/* Bitmap when checked.  */
    HBITMAP hUnCheckBit;	/* Bitmap when unchecked.  */
    LPWSTR text;			/* Item text or bitmap handle.  */
    DWORD dwItemData;		/* Application defined.  */
    DWORD dwTypeData;		/* depends on fMask */
    HBITMAP hbmpItem;		/* bitmap in win98 style menus */
    /* ----------- Wine stuff ----------- */
    RECT      rect;		/* Item area (relative to menu window) */
    UINT      xTab;		/* X position of text after Tab */
} MENUITEM;

/* Popup menu structure */
typedef struct {
    WORD        wFlags;       /* Menu flags (MF_POPUP, MF_SYSMENU) */
    WORD        wMagic;       /* Magic number */
    WORD	Width;        /* Width of the whole menu */
    WORD	Height;       /* Height of the whole menu */
    UINT        nItems;       /* Number of items in the menu */
    HWND        hWnd;         /* Window containing the menu */
    MENUITEM    *items;       /* Array of menu items */
    UINT        FocusedItem;  /* Currently focused item */
    HWND	hwndOwner;    /* window receiving the messages for ownerdraw */
    BOOL        bTimeToHide;  /* Request hiding when receiving a second click in the top-level menu item */
    /* ------------ MENUINFO members ------ */
    DWORD	dwStyle;	/* Extended mennu style */
    UINT	cyMax;		/* max hight of the whole menu, 0 is screen hight */
    HBRUSH	hbrBack;	/* brush for menu background */
    DWORD	dwContextHelpID;
    DWORD	dwMenuData;	/* application defined value */
    HMENU       hSysMenuOwner;  /* Handle to the dummy sys menu holder */
} POPUPMENU, *LPPOPUPMENU;

/* internal flags for menu tracking */

#define TF_ENDMENU              0x0001
#define TF_SUSPENDPOPUP         0x0002
#define TF_SKIPREMOVE		0x0004

typedef struct
{
    UINT	trackFlags;
    HMENU	hCurrentMenu; /* current submenu (can be equal to hTopMenu)*/
    HMENU	hTopMenu;     /* initial menu */
    HWND	hOwnerWnd;    /* where notifications are sent */
    POINT	pt;
} MTRACKER;

#define MENU_MAGIC   0x554d  /* 'MU' */

#define ITEM_PREV		-1
#define ITEM_NEXT		 1

  /* Internal MENU_TrackMenu() flags */
#define TPM_INTERNAL		0xF0000000
#define TPM_ENTERIDLEEX	 	0x80000000		/* set owner window for WM_ENTERIDLE */
#define TPM_BUTTONDOWN		0x40000000		/* menu was clicked before tracking */
#define TPM_POPUPMENU           0x20000000              /* menu is a popup menu */

  /* popup menu shade thickness */
#define POPUP_XSHADE		4
#define POPUP_YSHADE		4

  /* Space between 2 menu bar items */
#define MENU_BAR_ITEMS_SPACE 12

  /* Minimum width of a tab character */
#define MENU_TAB_SPACE 8

  /* Height of a separator item */
#define SEPARATOR_HEIGHT 5

  /* (other menu->FocusedItem values give the position of the focused item) */
#define NO_SELECTED_ITEM  0xffff

#define MENU_ITEM_TYPE(flags) \
  ((flags) & (MF_STRING | MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR))

#define IS_STRING_ITEM(flags) (MENU_ITEM_TYPE ((flags)) == MF_STRING)
#define IS_BITMAP_ITEM(flags) (MENU_ITEM_TYPE ((flags)) == MF_BITMAP)
#define IS_MAGIC_ITEM(text)   (LOWORD((int)text)<12)

#define IS_SYSTEM_MENU(menu)  \
	(!((menu)->wFlags & MF_POPUP) && (menu)->wFlags & MF_SYSMENU)

#define IS_SYSTEM_POPUP(menu) \
	((menu)->wFlags & MF_POPUP && (menu)->wFlags & MF_SYSMENU)

#define TYPE_MASK (MFT_STRING | MFT_BITMAP | MFT_OWNERDRAW | MFT_SEPARATOR | \
		   MFT_MENUBARBREAK | MFT_MENUBREAK | MFT_RADIOCHECK | \
		   MFT_RIGHTORDER | MFT_RIGHTJUSTIFY | \
		   MF_POPUP | MF_SYSMENU | MF_HELP)
#define STATE_MASK (~TYPE_MASK)

  /* Dimension of the menu bitmaps */
static WORD arrow_bitmap_width = 0, arrow_bitmap_height = 0;

static HBITMAP hStdMnArrow = 0;
static HBITMAP hBmpSysMenu = 0;

static HBRUSH	hShadeBrush = 0;
static HFONT	hMenuFont = 0;
static HFONT	hMenuFontBold = 0;

static HMENU MENU_DefSysPopup = 0;  /* Default system menu popup */

/* Use global popup window because there's no way 2 menus can
 * be tracked at the same time.  */
static HWND top_popup;

  /* Flag set by EndMenu() to force an exit from menu tracking */
static BOOL fEndMenu = FALSE;

static LRESULT WINAPI PopupMenuWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );


/*********************************************************************
 * menu class descriptor
 */
const struct builtin_class_descr MENU_builtin_class =
{
    POPUPMENU_CLASS_ATOM,          /* name */
    CS_GLOBALCLASS | CS_SAVEBITS,  /* style */
    NULL,                          /* procA (winproc is Unicode only) */
    PopupMenuWndProc,              /* procW */
    sizeof(HMENU),                 /* extra */
    IDC_ARROWA,                    /* cursor */
    COLOR_MENU+1                   /* brush */
};


/***********************************************************************
 *           debug_print_menuitem
 *
 * Print a menuitem in readable form.
 */

#define debug_print_menuitem(pre, mp, post) \
  if(!TRACE_ON(menu)) ; else do_debug_print_menuitem(pre, mp, post)

#define MENUOUT(text) \
  DPRINTF("%s%s", (count++ ? "," : ""), (text))

#define MENUFLAG(bit,text) \
  do { \
    if (flags & (bit)) { flags &= ~(bit); MENUOUT ((text)); } \
  } while (0)

static void do_debug_print_menuitem(const char *prefix, MENUITEM * mp,
				    const char *postfix)
{
    TRACE("%s ", prefix);
    if (mp) {
	UINT flags = mp->fType;
	int typ = MENU_ITEM_TYPE(flags);
	DPRINTF( "{ ID=0x%x", mp->wID);
	if (flags & MF_POPUP)
	    DPRINTF( ", Sub=0x%x", mp->hSubMenu);
	if (flags) {
	    int count = 0;
	    DPRINTF( ", Typ=");
	    if (typ == MFT_STRING)
		/* Nothing */ ;
	    else if (typ == MFT_SEPARATOR)
		MENUOUT("sep");
	    else if (typ == MFT_OWNERDRAW)
		MENUOUT("own");
	    else if (typ == MFT_BITMAP)
		MENUOUT("bit");
	    else
		MENUOUT("???");
	    flags -= typ;

	    MENUFLAG(MF_POPUP, "pop");
	    MENUFLAG(MFT_MENUBARBREAK, "barbrk");
	    MENUFLAG(MFT_MENUBREAK, "brk");
	    MENUFLAG(MFT_RADIOCHECK, "radio");
	    MENUFLAG(MFT_RIGHTORDER, "rorder");
	    MENUFLAG(MF_SYSMENU, "sys");
	    MENUFLAG(MFT_RIGHTJUSTIFY, "right");	/* same as MF_HELP */

	    if (flags)
		DPRINTF( "+0x%x", flags);
	}
	flags = mp->fState;
	if (flags) {
	    int count = 0;
	    DPRINTF( ", State=");
	    MENUFLAG(MFS_GRAYED, "grey");
	    MENUFLAG(MFS_DEFAULT, "default");
	    MENUFLAG(MFS_DISABLED, "dis");
	    MENUFLAG(MFS_CHECKED, "check");
	    MENUFLAG(MFS_HILITE, "hi");
	    MENUFLAG(MF_USECHECKBITMAPS, "usebit");
	    MENUFLAG(MF_MOUSESELECT, "mouse");
	    if (flags)
		DPRINTF( "+0x%x", flags);
	}
	if (mp->hCheckBit)
	    DPRINTF( ", Chk=0x%x", mp->hCheckBit);
	if (mp->hUnCheckBit)
	    DPRINTF( ", Unc=0x%x", mp->hUnCheckBit);

	if (typ == MFT_STRING) {
	    if (mp->text)
		DPRINTF( ", Text=%s", debugstr_w(mp->text));
	    else
		DPRINTF( ", Text=Null");
	} else if (mp->text == NULL)
	    /* Nothing */ ;
	else
	    DPRINTF( ", Text=%p", mp->text);
	if (mp->dwItemData)
	    DPRINTF( ", ItemData=0x%08lx", mp->dwItemData);
	DPRINTF( " }");
    } else {
	DPRINTF( "NULL");
    }

    DPRINTF(" %s\n", postfix);
}

#undef MENUOUT
#undef MENUFLAG


/***********************************************************************
 *           MENU_GetMenu
 *
 * Validate the given menu handle and returns the menu structure pointer.
 */
static POPUPMENU *MENU_GetMenu(HMENU hMenu)
{
    POPUPMENU *menu = USER_HEAP_LIN_ADDR(hMenu);
    if (!menu || menu->wMagic != MENU_MAGIC)
    {
        WARN("invalid menu handle=%x, ptr=%p, magic=%x\n", hMenu, menu, menu? menu->wMagic:0);
        menu = NULL;
    }
    return menu;
}

/***********************************************************************
 *           get_win_sys_menu
 *
 * Get the system menu of a window
 */
static HMENU get_win_sys_menu( HWND hwnd )
{
    HMENU ret = 0;
    WND *win = WIN_FindWndPtr( hwnd );
    if (win)
    {
        ret = win->hSysMenu;
        WIN_ReleaseWndPtr( win );
    }
    return ret;
}

/***********************************************************************
 *           MENU_CopySysPopup
 *
 * Return the default system menu.
 */
static HMENU MENU_CopySysPopup(void)
{
    HMENU hMenu = LoadMenuA(GetModuleHandleA("USER32"), "SYSMENU");

    if( hMenu ) {
        POPUPMENU* menu = MENU_GetMenu(hMenu);
        menu->wFlags |= MF_SYSMENU | MF_POPUP;
	SetMenuDefaultItem(hMenu, SC_CLOSE, FALSE);
    }
    else
	ERR("Unable to load default system menu\n" );

    TRACE("returning %x.\n", hMenu );

    return hMenu;
}


/**********************************************************************
 *           MENU_GetSysMenu
 *
 * Create a copy of the system menu. System menu in Windows is
 * a special menu bar with the single entry - system menu popup.
 * This popup is presented to the outside world as a "system menu".
 * However, the real system menu handle is sometimes seen in the
 * WM_MENUSELECT parameters (and Word 6 likes it this way).
 */
HMENU MENU_GetSysMenu( HWND hWnd, HMENU hPopupMenu )
{
    HMENU hMenu;

    if ((hMenu = CreateMenu()))
    {
	POPUPMENU *menu = MENU_GetMenu(hMenu);
	menu->wFlags = MF_SYSMENU;
	menu->hWnd = WIN_GetFullHandle( hWnd );

	if (hPopupMenu == (HMENU)(-1))
	    hPopupMenu = MENU_CopySysPopup();
	else if( !hPopupMenu ) hPopupMenu = MENU_DefSysPopup;

	if (hPopupMenu)
	{
	    InsertMenuA( hMenu, -1, MF_SYSMENU | MF_POPUP | MF_BYPOSITION, hPopupMenu, NULL );

            menu->items[0].fType = MF_SYSMENU | MF_POPUP;
            menu->items[0].fState = 0;
            if ((menu = MENU_GetMenu(hPopupMenu))) menu->wFlags |= MF_SYSMENU;

	    TRACE("GetSysMenu hMenu=%04x (%04x)\n", hMenu, hPopupMenu );
	    return hMenu;
	}
	DestroyMenu( hMenu );
    }
    ERR("failed to load system menu!\n");
    return 0;
}


/***********************************************************************
 *           MENU_Init
 *
 * Menus initialisation.
 */
BOOL MENU_Init()
{
    HBITMAP hBitmap;
    NONCLIENTMETRICSA ncm;

    static unsigned char shade_bits[16] = { 0x55, 0, 0xAA, 0,
					    0x55, 0, 0xAA, 0,
					    0x55, 0, 0xAA, 0,
					    0x55, 0, 0xAA, 0 };

    /* Load menu bitmaps */
    hStdMnArrow = LoadBitmapA(0, MAKEINTRESOURCEA(OBM_MNARROW));
    /* Load system buttons bitmaps */
    hBmpSysMenu = LoadBitmapA(0, MAKEINTRESOURCEA(OBM_CLOSE));

    if (hStdMnArrow)
    {
	BITMAP bm;
	GetObjectA( hStdMnArrow, sizeof(bm), &bm );
	arrow_bitmap_width = bm.bmWidth;
	arrow_bitmap_height = bm.bmHeight;
    } else
	return FALSE;

    if (! (hBitmap = CreateBitmap( 8, 8, 1, 1, shade_bits)))
	return FALSE;

    if(!(hShadeBrush = CreatePatternBrush( hBitmap )))
	return FALSE;

    DeleteObject( hBitmap );
    if (!(MENU_DefSysPopup = MENU_CopySysPopup()))
	return FALSE;

    ncm.cbSize = sizeof (NONCLIENTMETRICSA);
    if (!(SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSA), &ncm, 0)))
	return FALSE;

    if (!(hMenuFont = CreateFontIndirectA( &ncm.lfMenuFont )))
	return FALSE;

    ncm.lfMenuFont.lfWeight += 300;
    if ( ncm.lfMenuFont.lfWeight > 1000)
	ncm.lfMenuFont.lfWeight = 1000;

    if (!(hMenuFontBold = CreateFontIndirectA( &ncm.lfMenuFont )))
	return FALSE;

    return TRUE;
}

/***********************************************************************
 *           MENU_InitSysMenuPopup
 *
 * Grey the appropriate items in System menu.
 */
static void MENU_InitSysMenuPopup( HMENU hmenu, DWORD style, DWORD clsStyle )
{
    BOOL gray;

    gray = !(style & WS_THICKFRAME) || (style & (WS_MAXIMIZE | WS_MINIMIZE));
    EnableMenuItem( hmenu, SC_SIZE, (gray ? MF_GRAYED : MF_ENABLED) );
    gray = ((style & WS_MAXIMIZE) != 0);
    EnableMenuItem( hmenu, SC_MOVE, (gray ? MF_GRAYED : MF_ENABLED) );
    gray = !(style & WS_MINIMIZEBOX) || (style & WS_MINIMIZE);
    EnableMenuItem( hmenu, SC_MINIMIZE, (gray ? MF_GRAYED : MF_ENABLED) );
    gray = !(style & WS_MAXIMIZEBOX) || (style & WS_MAXIMIZE);
    EnableMenuItem( hmenu, SC_MAXIMIZE, (gray ? MF_GRAYED : MF_ENABLED) );
    gray = !(style & (WS_MAXIMIZE | WS_MINIMIZE));
    EnableMenuItem( hmenu, SC_RESTORE, (gray ? MF_GRAYED : MF_ENABLED) );
    gray = (clsStyle & CS_NOCLOSE) != 0;

    /* The menu item must keep its state if it's disabled */
    if(gray)
	EnableMenuItem( hmenu, SC_CLOSE, MF_GRAYED);
}


/******************************************************************************
 *
 *   UINT  MENU_GetStartOfNextColumn(
 *     HMENU  hMenu )
 *
 *****************************************************************************/

static UINT  MENU_GetStartOfNextColumn(
    HMENU  hMenu )
{
    POPUPMENU *menu = MENU_GetMenu(hMenu);
    UINT i;

    if(!menu)
	return NO_SELECTED_ITEM;

    i = menu->FocusedItem + 1;
    if( i == NO_SELECTED_ITEM )
	return i;

    for( ; i < menu->nItems; ++i ) {
	if (menu->items[i].fType & MF_MENUBARBREAK)
	    return i;
    }

    return NO_SELECTED_ITEM;
}


/******************************************************************************
 *
 *   UINT  MENU_GetStartOfPrevColumn(
 *     HMENU  hMenu )
 *
 *****************************************************************************/

static UINT  MENU_GetStartOfPrevColumn(
    HMENU  hMenu )
{
    POPUPMENU *menu = MENU_GetMenu(hMenu);
    UINT  i;

    if( !menu )
	return NO_SELECTED_ITEM;

    if( menu->FocusedItem == 0 || menu->FocusedItem == NO_SELECTED_ITEM )
	return NO_SELECTED_ITEM;

    /* Find the start of the column */

    for(i = menu->FocusedItem; i != 0 &&
	 !(menu->items[i].fType & MF_MENUBARBREAK);
	--i); /* empty */

    if(i == 0)
	return NO_SELECTED_ITEM;

    for(--i; i != 0; --i) {
	if (menu->items[i].fType & MF_MENUBARBREAK)
	    break;
    }

    TRACE("ret %d.\n", i );

    return i;
}



/***********************************************************************
 *           MENU_FindItem
 *
 * Find a menu item. Return a pointer on the item, and modifies *hmenu
 * in case the item was in a sub-menu.
 */
static MENUITEM *MENU_FindItem( HMENU *hmenu, UINT *nPos, UINT wFlags )
{
    POPUPMENU *menu;
    UINT i;

    if (((*hmenu)==0xffff) || (!(menu = MENU_GetMenu(*hmenu)))) return NULL;
    if (wFlags & MF_BYPOSITION)
    {
	if (*nPos >= menu->nItems) return NULL;
	return &menu->items[*nPos];
    }
    else
    {
        MENUITEM *item = menu->items;
	for (i = 0; i < menu->nItems; i++, item++)
	{
	    if (item->wID == *nPos)
	    {
		*nPos = i;
		return item;
	    }
	    else if (item->fType & MF_POPUP)
	    {
		HMENU hsubmenu = item->hSubMenu;
		MENUITEM *subitem = MENU_FindItem( &hsubmenu, nPos, wFlags );
		if (subitem)
		{
		    *hmenu = hsubmenu;
		    return subitem;
		}
	    }
	}
    }
    return NULL;
}

/***********************************************************************
 *           MENU_FindSubMenu
 *
 * Find a Sub menu. Return the position of the submenu, and modifies
 * *hmenu in case it is found in another sub-menu.
 * If the submenu cannot be found, NO_SELECTED_ITEM is returned.
 */
UINT MENU_FindSubMenu( HMENU *hmenu, HMENU hSubTarget )
{
    POPUPMENU *menu;
    UINT i;
    MENUITEM *item;
    if (((*hmenu)==0xffff) ||
            (!(menu = MENU_GetMenu(*hmenu))))
        return NO_SELECTED_ITEM;
    item = menu->items;
    for (i = 0; i < menu->nItems; i++, item++) {
        if(!(item->fType & MF_POPUP)) continue;
        if (item->hSubMenu == hSubTarget) {
            return i;
        }
        else  {
            HMENU hsubmenu = item->hSubMenu;
            UINT pos = MENU_FindSubMenu( &hsubmenu, hSubTarget );
            if (pos != NO_SELECTED_ITEM) {
                *hmenu = hsubmenu;
                return pos;
            }
        }
    }
    return NO_SELECTED_ITEM;
}

/***********************************************************************
 *           MENU_FreeItemData
 */
static void MENU_FreeItemData( MENUITEM* item )
{
    /* delete text */
    if (IS_STRING_ITEM(item->fType) && item->text)
        HeapFree( GetProcessHeap(), 0, item->text );
}

/***********************************************************************
 *           MENU_FindItemByCoords
 *
 * Find the item at the specified coordinates (screen coords). Does
 * not work for child windows and therefore should not be called for
 * an arbitrary system menu.
 */
static MENUITEM *MENU_FindItemByCoords( POPUPMENU *menu,
					POINT pt, UINT *pos )
{
    MENUITEM *item;
    UINT i;
    RECT wrect;

    if (!GetWindowRect(menu->hWnd,&wrect)) return NULL;
    pt.x -= wrect.left;pt.y -= wrect.top;
    item = menu->items;
    for (i = 0; i < menu->nItems; i++, item++)
    {
	if ((pt.x >= item->rect.left) && (pt.x < item->rect.right) &&
	    (pt.y >= item->rect.top) && (pt.y < item->rect.bottom))
	{
	    if (pos) *pos = i;
	    return item;
	}
    }
    return NULL;
}


/***********************************************************************
 *           MENU_FindItemByKey
 *
 * Find the menu item selected by a key press.
 * Return item id, -1 if none, -2 if we should close the menu.
 */
static UINT MENU_FindItemByKey( HWND hwndOwner, HMENU hmenu,
				  UINT key, BOOL forceMenuChar )
{
    TRACE("\tlooking for '%c' in [%04x]\n", (char)key, (UINT16)hmenu );

    if (!IsMenu( hmenu )) hmenu = GetSubMenu( get_win_sys_menu(hwndOwner), 0);

    if (hmenu)
    {
	POPUPMENU *menu = MENU_GetMenu( hmenu );
	MENUITEM *item = menu->items;
	LONG menuchar;

	if( !forceMenuChar )
	{
	     UINT i;

	     key = toupper(key);
	     for (i = 0; i < menu->nItems; i++, item++)
	     {
		if (item->text && (IS_STRING_ITEM(item->fType)))
		{
		    WCHAR *p = item->text - 2;
		    do
		    {
		    	p = strchrW (p + 2, '&');
		    }
		    while (p != NULL && p [1] == '&');
		    if (p && (toupper(p[1]) == key)) return i;
		}
	     }
	}
	menuchar = SendMessageA( hwndOwner, WM_MENUCHAR,
                                   MAKEWPARAM( key, menu->wFlags ), hmenu );
	if (HIWORD(menuchar) == 2) return LOWORD(menuchar);
	if (HIWORD(menuchar) == 1) return (UINT)(-2);
    }
    return (UINT)(-1);
}


/***********************************************************************
 *           MENU_GetBitmapItemSize
 *
 * Get the size of a bitmap item.
 */
static void MENU_GetBitmapItemSize( UINT id, DWORD data, SIZE *size )
{
    BITMAP bm;
    HBITMAP bmp = (HBITMAP)id;

    size->cx = size->cy = 0;

    /* check if there is a magic menu item associated with this item */
    if (id && IS_MAGIC_ITEM( id ))
    {
        switch(LOWORD(id))
        {
        case HBMMENU_SYSTEM:
            if (data)
            {
                bmp = (HBITMAP)data;
                break;
            }
            /* fall through */
        case HBMMENU_MBAR_RESTORE:
        case HBMMENU_MBAR_MINIMIZE:
        case HBMMENU_MBAR_MINIMIZE_D:
        case HBMMENU_MBAR_CLOSE:
        case HBMMENU_MBAR_CLOSE_D:
            size->cx = GetSystemMetrics( SM_CXSIZE );
            size->cy = GetSystemMetrics( SM_CYSIZE );
            return;
        case (WORD)HBMMENU_CALLBACK:
        case HBMMENU_POPUP_CLOSE:
        case HBMMENU_POPUP_RESTORE:
        case HBMMENU_POPUP_MAXIMIZE:
        case HBMMENU_POPUP_MINIMIZE:
        default:
            FIXME("Magic 0x%08x not implemented\n", id);
            return;
        }
    }
    if (GetObjectA(bmp, sizeof(bm), &bm ))
    {
        size->cx = bm.bmWidth;
        size->cy = bm.bmHeight;
    }
}

/***********************************************************************
 *           MENU_DrawBitmapItem
 *
 * Draw a bitmap item.
 */
static void MENU_DrawBitmapItem( HDC hdc, MENUITEM *lpitem, const RECT *rect, BOOL menuBar )
{
    BITMAP bm;
    DWORD rop;
    HDC hdcMem;
    HBITMAP bmp = (HBITMAP)lpitem->text;
    int w = rect->right - rect->left;
    int h = rect->bottom - rect->top;
    int bmp_xoffset = 0;
    int left, top;

    /* Check if there is a magic menu item associated with this item */
    if (lpitem->text && IS_MAGIC_ITEM(lpitem->text))
    {
        UINT flags = 0;
        RECT r;

        switch(LOWORD(lpitem->text))
        {
        case HBMMENU_SYSTEM:
            if (lpitem->dwItemData)
            {
                bmp = (HBITMAP)lpitem->dwItemData;
                if (!GetObjectA( bmp, sizeof(bm), &bm )) return;
            }
            else
            {
                bmp = hBmpSysMenu;
                if (!GetObjectA( bmp, sizeof(bm), &bm )) return;
                /* only use right half of the bitmap */
                bmp_xoffset = bm.bmWidth / 2;
                bm.bmWidth -= bmp_xoffset;
            }
            goto got_bitmap;
        case HBMMENU_MBAR_RESTORE:
            flags = DFCS_CAPTIONRESTORE;
            break;
        case HBMMENU_MBAR_MINIMIZE:
            flags = DFCS_CAPTIONMIN;
            break;
        case HBMMENU_MBAR_MINIMIZE_D:
            flags = DFCS_CAPTIONMIN | DFCS_INACTIVE;
            break;
        case HBMMENU_MBAR_CLOSE:
            flags = DFCS_CAPTIONCLOSE;
            break;
        case HBMMENU_MBAR_CLOSE_D:
            flags = DFCS_CAPTIONCLOSE | DFCS_INACTIVE;
            break;
        case (WORD)HBMMENU_CALLBACK:
        case HBMMENU_POPUP_CLOSE:
        case HBMMENU_POPUP_RESTORE:
        case HBMMENU_POPUP_MAXIMIZE:
        case HBMMENU_POPUP_MINIMIZE:
        default:
            FIXME("Magic 0x%08x not implemented\n", LOWORD(lpitem->text));
            return;
        }
        r = *rect;
        InflateRect( &r, -1, -1 );
        if (lpitem->fState & MF_HILITE) flags |= DFCS_PUSHED;
        DrawFrameControl( hdc, &r, DFC_CAPTION, flags );
        return;
    }

    if (!bmp || !GetObjectA( bmp, sizeof(bm), &bm )) return;

 got_bitmap:
    hdcMem = CreateCompatibleDC( hdc );
    SelectObject( hdcMem, bmp );

    /* handle fontsize > bitmap_height */
    top = (h>bm.bmHeight) ? rect->top+(h-bm.bmHeight)/2 : rect->top;
    left=rect->left;
    if (TWEAK_WineLook == WIN95_LOOK) {
        rop=((lpitem->fState & MF_HILITE) && !IS_MAGIC_ITEM(lpitem->text)) ? NOTSRCCOPY : SRCCOPY;
        if ((lpitem->fState & MF_HILITE) && IS_BITMAP_ITEM(lpitem->fType))
            SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
    } else {
        left++;
        w-=2;
        rop=((lpitem->fState & MF_HILITE) && !IS_MAGIC_ITEM(lpitem->text) && (!menuBar)) ? MERGEPAINT : SRCCOPY;
    }
    BitBlt( hdc, left, top, w, h, hdcMem, bmp_xoffset, 0, rop );
    DeleteDC( hdcMem );
}


/***********************************************************************
 *           MENU_CalcItemSize
 *
 * Calculate the size of the menu item and store it in lpitem->rect.
 */
static void MENU_CalcItemSize( HDC hdc, MENUITEM *lpitem, HWND hwndOwner,
			       INT orgX, INT orgY, BOOL menuBar )
{
    WCHAR *p;
    UINT check_bitmap_width = GetSystemMetrics( SM_CXMENUCHECK );

    TRACE("dc=0x%04x owner=0x%04x (%d,%d)\n", hdc, hwndOwner, orgX, orgY);
    debug_print_menuitem("MENU_CalcItemSize: menuitem:", lpitem,
			 (menuBar ? " (MenuBar)" : ""));

    SetRect( &lpitem->rect, orgX, orgY, orgX, orgY );

    if (lpitem->fType & MF_OWNERDRAW)
    {
        /*
        ** Experimentation under Windows reveals that an owner-drawn
        ** menu is expected to return the size of the content part of
        ** the menu item, not including the checkmark nor the submenu
        ** arrow.  Windows adds those values itself and returns the
        ** enlarged rectangle on subsequent WM_DRAWITEM messages.
        */
        MEASUREITEMSTRUCT mis;
        mis.CtlType    = ODT_MENU;
        mis.CtlID      = 0;
        mis.itemID     = lpitem->wID;
        mis.itemData   = (DWORD)lpitem->dwItemData;
        mis.itemHeight = 0;
        mis.itemWidth  = 0;
        SendMessageA( hwndOwner, WM_MEASUREITEM, 0, (LPARAM)&mis );
        lpitem->rect.right  += mis.itemWidth;

 	if (menuBar)
	{
	     lpitem->rect.right += MENU_BAR_ITEMS_SPACE;


             /* under at least win95 you seem to be given a standard
                height for the menu and the height value is ignored */

	     if (TWEAK_WineLook == WIN31_LOOK)
	    	lpitem->rect.bottom += GetSystemMetrics(SM_CYMENU);
	     else
	    	lpitem->rect.bottom += GetSystemMetrics(SM_CYMENU)-1;
        }
        else
            lpitem->rect.bottom += mis.itemHeight;

	TRACE("id=%04x size=%dx%d\n",
                     lpitem->wID, mis.itemWidth, mis.itemHeight);
        /* Fall through to get check/arrow width calculation. */
    }

    if (lpitem->fType & MF_SEPARATOR)
    {
	lpitem->rect.bottom += SEPARATOR_HEIGHT;
	return;
    }

    if (!menuBar)
    {
	lpitem->rect.right += 2 * check_bitmap_width;
	if (lpitem->fType & MF_POPUP)
	    lpitem->rect.right += arrow_bitmap_width;
    }

    if (lpitem->fType & MF_OWNERDRAW)
        return;

    if (IS_BITMAP_ITEM(lpitem->fType))
    {
        SIZE size;

        MENU_GetBitmapItemSize( (int)lpitem->text, lpitem->dwItemData, &size );
        lpitem->rect.right  += size.cx;
        lpitem->rect.bottom += size.cy;
        if (TWEAK_WineLook == WIN98_LOOK)
        {
            /* Leave space for the sunken border */
            lpitem->rect.right  += 2;
            lpitem->rect.bottom += 2;
        }
    }


    /* it must be a text item - unless it's the system menu */
    if (!(lpitem->fType & MF_SYSMENU) && IS_STRING_ITEM( lpitem->fType ))
    {   SIZE size;

	GetTextExtentPoint32W(hdc, lpitem->text,  strlenW(lpitem->text), &size);

	lpitem->rect.right  += size.cx;
	if (TWEAK_WineLook == WIN31_LOOK)
	    lpitem->rect.bottom += max( size.cy, GetSystemMetrics(SM_CYMENU) );
	else
	    lpitem->rect.bottom += max(size.cy, GetSystemMetrics(SM_CYMENU)-1);
	lpitem->xTab = 0;

	if (menuBar)
	{
	     lpitem->rect.right += MENU_BAR_ITEMS_SPACE;
	}
	else if ((p = strchrW( lpitem->text, '\t' )) != NULL)
	{
	    /* Item contains a tab (only meaningful in popup menus) */
	    GetTextExtentPoint32W(hdc, lpitem->text, (int)(p - lpitem->text) , &size);
	    lpitem->xTab = check_bitmap_width + MENU_TAB_SPACE + size.cx;
	    lpitem->rect.right += MENU_TAB_SPACE;
	}
	else
	{
	    if (strchrW( lpitem->text, '\b' ))
	        lpitem->rect.right += MENU_TAB_SPACE;
	    lpitem->xTab = lpitem->rect.right - check_bitmap_width
	                   - arrow_bitmap_width;
	}
    }
    TRACE("(%d,%d)-(%d,%d)\n", lpitem->rect.left, lpitem->rect.top, lpitem->rect.right, lpitem->rect.bottom);
}


/***********************************************************************
 *           MENU_PopupMenuCalcSize
 *
 * Calculate the size of a popup menu.
 */
static void MENU_PopupMenuCalcSize( LPPOPUPMENU lppop, HWND hwndOwner )
{
    MENUITEM *lpitem;
    HDC hdc;
    int start, i;
    int orgX, orgY, maxX, maxTab, maxTabWidth;

    lppop->Width = lppop->Height = 0;
    if (lppop->nItems == 0) return;
    hdc = GetDC( 0 );

    SelectObject( hdc, hMenuFont);

    start = 0;
    maxX = (TWEAK_WineLook == WIN31_LOOK) ? GetSystemMetrics(SM_CXBORDER) : 2+1 ;

    while (start < lppop->nItems)
    {
	lpitem = &lppop->items[start];
	orgX = maxX;
	orgY = (TWEAK_WineLook == WIN31_LOOK) ? GetSystemMetrics(SM_CYBORDER) : 2;

	maxTab = maxTabWidth = 0;

	  /* Parse items until column break or end of menu */
	for (i = start; i < lppop->nItems; i++, lpitem++)
	{
	    if ((i != start) &&
		(lpitem->fType & (MF_MENUBREAK | MF_MENUBARBREAK))) break;

	    MENU_CalcItemSize( hdc, lpitem, hwndOwner, orgX, orgY, FALSE );

	    if (lpitem->fType & MF_MENUBARBREAK) orgX++;
	    maxX = max( maxX, lpitem->rect.right );
	    orgY = lpitem->rect.bottom;
	    if (IS_STRING_ITEM(lpitem->fType) && lpitem->xTab)
	    {
		maxTab = max( maxTab, lpitem->xTab );
		maxTabWidth = max(maxTabWidth,lpitem->rect.right-lpitem->xTab);
	    }
	}

	  /* Finish the column (set all items to the largest width found) */
	maxX = max( maxX, maxTab + maxTabWidth );
	for (lpitem = &lppop->items[start]; start < i; start++, lpitem++)
	{
	    lpitem->rect.right = maxX;
	    if (IS_STRING_ITEM(lpitem->fType) && lpitem->xTab)
		lpitem->xTab = maxTab;

	}
	lppop->Height = max( lppop->Height, orgY );
    }

    lppop->Width  = maxX;

    /* space for 3d border */
    if(TWEAK_WineLook > WIN31_LOOK)
    {
	lppop->Height += 2;
	lppop->Width += 2;
    }

    ReleaseDC( 0, hdc );
}


/***********************************************************************
 *           MENU_MenuBarCalcSize
 *
 * FIXME: Word 6 implements its own MDI and its own 'close window' bitmap
 * height is off by 1 pixel which causes lengthy window relocations when
 * active document window is maximized/restored.
 *
 * Calculate the size of the menu bar.
 */
static void MENU_MenuBarCalcSize( HDC hdc, LPRECT lprect,
                                  LPPOPUPMENU lppop, HWND hwndOwner )
{
    MENUITEM *lpitem;
    int start, i, orgX, orgY, maxY, helpPos;

    if ((lprect == NULL) || (lppop == NULL)) return;
    if (lppop->nItems == 0) return;
    TRACE("left=%d top=%d right=%d bottom=%d\n",
                 lprect->left, lprect->top, lprect->right, lprect->bottom);
    lppop->Width  = lprect->right - lprect->left;
    lppop->Height = 0;
    maxY = lprect->top+1;
    start = 0;
    helpPos = -1;
    while (start < lppop->nItems)
    {
	lpitem = &lppop->items[start];
	orgX = lprect->left;
	orgY = maxY;

	  /* Parse items until line break or end of menu */
	for (i = start; i < lppop->nItems; i++, lpitem++)
	{
	    if ((helpPos == -1) && (lpitem->fType & MF_RIGHTJUSTIFY)) helpPos = i;
	    if ((i != start) &&
		(lpitem->fType & (MF_MENUBREAK | MF_MENUBARBREAK))) break;

	    TRACE("calling MENU_CalcItemSize org=(%d, %d)\n",
			 orgX, orgY );
	    debug_print_menuitem ("  item: ", lpitem, "");
	    MENU_CalcItemSize( hdc, lpitem, hwndOwner, orgX, orgY, TRUE );

	    if (lpitem->rect.right > lprect->right)
	    {
		if (i != start) break;
		else lpitem->rect.right = lprect->right;
	    }
	    maxY = max( maxY, lpitem->rect.bottom );
	    orgX = lpitem->rect.right;
	}

	  /* Finish the line (set all items to the largest height found) */
	while (start < i) lppop->items[start++].rect.bottom = maxY;
    }

    lprect->bottom = maxY;
    lppop->Height = lprect->bottom - lprect->top;

    /* Flush right all items between the MF_RIGHTJUSTIFY and */
    /* the last item (if several lines, only move the last line) */
    lpitem = &lppop->items[lppop->nItems-1];
    orgY = lpitem->rect.top;
    orgX = lprect->right;
    for (i = lppop->nItems - 1; i >= helpPos; i--, lpitem--) {
        if ( (helpPos==-1) || (helpPos>i) )
            break;				/* done */
        if (lpitem->rect.top != orgY) break;	/* Other line */
        if (lpitem->rect.right >= orgX) break;	/* Too far right already */
        lpitem->rect.left += orgX - lpitem->rect.right;
        lpitem->rect.right = orgX;
        orgX = lpitem->rect.left;
    }
}

/***********************************************************************
 *           MENU_DrawMenuItem
 *
 * Draw a single menu item.
 */
static void MENU_DrawMenuItem( HWND hwnd, HMENU hmenu, HWND hwndOwner, HDC hdc, MENUITEM *lpitem,
			       UINT height, BOOL menuBar, UINT odaction )
{
    RECT rect;

    debug_print_menuitem("MENU_DrawMenuItem: ", lpitem, "");

    if (lpitem->fType & MF_SYSMENU)
    {
	if( !IsIconic(hwnd) ) {
	    if (TWEAK_WineLook > WIN31_LOOK)
		NC_DrawSysButton95( hwnd, hdc,
				    lpitem->fState &
				    (MF_HILITE | MF_MOUSESELECT) );
	    else
		NC_DrawSysButton( hwnd, hdc,
				  lpitem->fState &
				  (MF_HILITE | MF_MOUSESELECT) );
	}

	return;
    }

    if (lpitem->fType & MF_OWNERDRAW)
    {
        /*
        ** Experimentation under Windows reveals that an owner-drawn
        ** menu is given the rectangle which includes the space it requested
        ** in its response to WM_MEASUREITEM _plus_ width for a checkmark
        ** and a popup-menu arrow.  This is the value of lpitem->rect.
        ** Windows will leave all drawing to the application except for
        ** the popup-menu arrow.  Windows always draws that itself, after
        ** the menu owner has finished drawing.
        */
        DRAWITEMSTRUCT dis;

        dis.CtlType   = ODT_MENU;
	dis.CtlID     = 0;
        dis.itemID    = lpitem->wID;
        dis.itemData  = (DWORD)lpitem->dwItemData;
        dis.itemState = 0;
        if (lpitem->fState & MF_CHECKED) dis.itemState |= ODS_CHECKED;
        if (lpitem->fState & MF_GRAYED)  dis.itemState |= ODS_GRAYED;
        if (lpitem->fState & MF_HILITE)  dis.itemState |= ODS_SELECTED;
        dis.itemAction = odaction; /* ODA_DRAWENTIRE | ODA_SELECT | ODA_FOCUS; */
        dis.hwndItem   = (HWND)hmenu;
        dis.hDC        = hdc;
        dis.rcItem     = lpitem->rect;
        TRACE("Ownerdraw: owner=%04x itemID=%d, itemState=%d, itemAction=%d, "
	      "hwndItem=%04x, hdc=%04x, rcItem={%d,%d,%d,%d}\n", hwndOwner,
	      dis.itemID, dis.itemState, dis.itemAction, dis.hwndItem,
	      dis.hDC, dis.rcItem.left, dis.rcItem.top, dis.rcItem.right,
	      dis.rcItem.bottom);
        SendMessageA( hwndOwner, WM_DRAWITEM, 0, (LPARAM)&dis );
        /* Fall through to draw popup-menu arrow */
    }

    TRACE("rect={%d,%d,%d,%d}\n", lpitem->rect.left, lpitem->rect.top,
					lpitem->rect.right,lpitem->rect.bottom);

    if (menuBar && (lpitem->fType & MF_SEPARATOR)) return;

    rect = lpitem->rect;

    if (!(lpitem->fType & MF_OWNERDRAW))
    {
	if (lpitem->fState & MF_HILITE)
	{
	    if(TWEAK_WineLook == WIN98_LOOK)
	    {
		if(menuBar)
		    DrawEdge(hdc, &rect, BDR_SUNKENOUTER, BF_RECT);
		else
		    FillRect(hdc, &rect, GetSysColorBrush(COLOR_HIGHLIGHT));
	    }
	    else /* Not Win98 Look */
	    {
		if(!IS_BITMAP_ITEM(lpitem->fType))
		    FillRect(hdc, &rect, GetSysColorBrush(COLOR_HIGHLIGHT));
	    }
	}
        else
	    FillRect( hdc, &rect, GetSysColorBrush(COLOR_MENU) );
    }

    SetBkMode( hdc, TRANSPARENT );

    if (!(lpitem->fType & MF_OWNERDRAW))
    {
        /* vertical separator */
        if (!menuBar && (lpitem->fType & MF_MENUBARBREAK))
        {
	    if (TWEAK_WineLook > WIN31_LOOK)
	    {
	        RECT rc = rect;
	        rc.top = 3;
	        rc.bottom = height - 3;
	        DrawEdge (hdc, &rc, EDGE_ETCHED, BF_LEFT);
	    }
	    else
	    {
	        SelectObject( hdc, GetSysColorPen(COLOR_WINDOWFRAME) );
	        MoveToEx( hdc, rect.left, 0, NULL );
	        LineTo( hdc, rect.left, height );
	    }
        }

        /* horizontal separator */
        if (lpitem->fType & MF_SEPARATOR)
        {
	    if (TWEAK_WineLook > WIN31_LOOK)
	    {
	        RECT rc = rect;
	        rc.left++;
	        rc.right--;
	        rc.top += SEPARATOR_HEIGHT / 2;
	        DrawEdge (hdc, &rc, EDGE_ETCHED, BF_TOP);
	    }
	    else
	    {
	        SelectObject( hdc, GetSysColorPen(COLOR_WINDOWFRAME) );
	        MoveToEx( hdc, rect.left, rect.top + SEPARATOR_HEIGHT/2, NULL );
	        LineTo( hdc, rect.right, rect.top + SEPARATOR_HEIGHT/2 );
	    }
	    return;
        }
    }

      /* Setup colors */

    if (lpitem->fState & MF_HILITE)
    {
	if(TWEAK_WineLook == WIN98_LOOK)
	{
            if(menuBar) {
		SetTextColor(hdc, GetSysColor(COLOR_MENUTEXT));
                SetBkColor(hdc, GetSysColor(COLOR_MENU));
	    } else {
		if(lpitem->fState & MF_GRAYED)
		    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
		else
		    SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
	    }
	}
	else /* Not Win98 Look */
	{
	    SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
	    if(!IS_BITMAP_ITEM(lpitem->fType))
		SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
	}
    }
    else
    {
	if (lpitem->fState & MF_GRAYED)
	    SetTextColor( hdc, GetSysColor( COLOR_GRAYTEXT ) );
	else
	    SetTextColor( hdc, GetSysColor( COLOR_MENUTEXT ) );
	SetBkColor( hdc, GetSysColor( COLOR_MENU ) );
    }

	/* helper lines for debugging */
/*	FrameRect(hdc, &rect, GetStockObject(BLACK_BRUSH));
	SelectObject( hdc, GetSysColorPen(COLOR_WINDOWFRAME) );
	MoveToEx( hdc, rect.left, (rect.top + rect.bottom)/2, NULL );
	LineTo( hdc, rect.right, (rect.top + rect.bottom)/2 );
*/

    if (!menuBar)
    {
	INT	y = rect.top + rect.bottom;
        UINT check_bitmap_width = GetSystemMetrics( SM_CXMENUCHECK );
        UINT check_bitmap_height = GetSystemMetrics( SM_CYMENUCHECK );

        if (!(lpitem->fType & MF_OWNERDRAW))
        {
	      /* Draw the check mark
	       *
	       * FIXME:
	       * Custom checkmark bitmaps are monochrome but not always 1bpp.
	       */
            HBITMAP bm = (lpitem->fState & MF_CHECKED) ? lpitem->hCheckBit : lpitem->hUnCheckBit;
            if (bm)  /* we have a custom bitmap */
            {
                HDC hdcMem = CreateCompatibleDC( hdc );
                SelectObject( hdcMem, bm );
                BitBlt( hdc, rect.left, (y - check_bitmap_height) / 2,
                        check_bitmap_width, check_bitmap_height,
                        hdcMem, 0, 0, SRCCOPY );
                DeleteDC( hdcMem );
            }
            else if (lpitem->fState & MF_CHECKED)  /* standard bitmaps */
            {
                RECT r;
                HBITMAP bm = CreateBitmap( check_bitmap_width, check_bitmap_height, 1, 1, NULL );
                HDC hdcMem = CreateCompatibleDC( hdc );
                SelectObject( hdcMem, bm );
                SetRect( &r, 0, 0, check_bitmap_width, check_bitmap_height );
                DrawFrameControl( hdcMem, &r, DFC_MENU,
                                  (lpitem->fType & MFT_RADIOCHECK) ?
                                  DFCS_MENUBULLET : DFCS_MENUCHECK );
                BitBlt( hdc, rect.left, (y - r.bottom) / 2, r.right, r.bottom,
                        hdcMem, 0, 0, SRCCOPY );
                DeleteDC( hdcMem );
                DeleteObject( bm );
            }
        }

	  /* Draw the popup-menu arrow */
	if (lpitem->fType & MF_POPUP)
	{
	    HDC hdcMem = CreateCompatibleDC( hdc );
	    HBITMAP hOrigBitmap;

	    hOrigBitmap = SelectObject( hdcMem, hStdMnArrow );
	    BitBlt( hdc, rect.right - arrow_bitmap_width - 1,
		      (y - arrow_bitmap_height) / 2,
		      arrow_bitmap_width, arrow_bitmap_height,
		      hdcMem, 0, 0, SRCCOPY );
            SelectObject( hdcMem, hOrigBitmap );
	    DeleteDC( hdcMem );
	}

	rect.left += check_bitmap_width;
	rect.right -= arrow_bitmap_width;
    }

    /* Done for owner-drawn */
    if (lpitem->fType & MF_OWNERDRAW)
        return;

    /* Draw the item text or bitmap */
    if (IS_BITMAP_ITEM(lpitem->fType))
    {
        MENU_DrawBitmapItem( hdc, lpitem, &rect, menuBar );
	return;

    }
    /* No bitmap - process text if present */
    else if (IS_STRING_ITEM(lpitem->fType))
    {
	register int i;
	HFONT hfontOld = 0;

	UINT uFormat = (menuBar) ?
			DT_CENTER | DT_VCENTER | DT_SINGLELINE :
			DT_LEFT | DT_VCENTER | DT_SINGLELINE;

	if ( lpitem->fState & MFS_DEFAULT )
	{
	     hfontOld = SelectObject( hdc, hMenuFontBold);
	}

	if (menuBar)
	{
	    rect.left += MENU_BAR_ITEMS_SPACE / 2;
	    rect.right -= MENU_BAR_ITEMS_SPACE / 2;
	}

	for (i = 0; lpitem->text[i]; i++)
	    if ((lpitem->text[i] == '\t') || (lpitem->text[i] == '\b'))
	        break;

	if( (TWEAK_WineLook != WIN31_LOOK) && (lpitem->fState & MF_GRAYED))
	{
	    if (!(lpitem->fState & MF_HILITE) )
	    {
		++rect.left; ++rect.top; ++rect.right; ++rect.bottom;
		SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
		DrawTextW( hdc, lpitem->text, i, &rect, uFormat );
		--rect.left; --rect.top; --rect.right; --rect.bottom;
	    }
	    SetTextColor(hdc, RGB(0x80, 0x80, 0x80));
	}

	DrawTextW( hdc, lpitem->text, i, &rect, uFormat);

	/* paint the shortcut text */
	if (!menuBar && lpitem->text[i])  /* There's a tab or flush-right char */
	{
	    if (lpitem->text[i] == '\t')
	    {
		rect.left = lpitem->xTab;
		uFormat = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
	    }
	    else
	    {
		uFormat = DT_RIGHT | DT_VCENTER | DT_SINGLELINE;
	    }

	    if( !(TWEAK_WineLook == WIN31_LOOK) && (lpitem->fState & MF_GRAYED))
	    {
		if (!(lpitem->fState & MF_HILITE) )
		{
		    ++rect.left; ++rect.top; ++rect.right; ++rect.bottom;
		    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
		    DrawTextW( hdc, lpitem->text + i + 1, -1, &rect, uFormat );
		    --rect.left; --rect.top; --rect.right; --rect.bottom;
		}
		SetTextColor(hdc, RGB(0x80, 0x80, 0x80));
	    }
	    DrawTextW( hdc, lpitem->text + i + 1, -1, &rect, uFormat );
	}

	if (hfontOld)
	    SelectObject (hdc, hfontOld);
    }
}


/***********************************************************************
 *           MENU_DrawPopupMenu
 *
 * Paint a popup menu.
 */
static void MENU_DrawPopupMenu( HWND hwnd, HDC hdc, HMENU hmenu )
{
    HBRUSH hPrevBrush = 0;
    RECT rect;

    TRACE("wnd=0x%04x dc=0x%04x menu=0x%04x\n", hwnd, hdc, hmenu);

    GetClientRect( hwnd, &rect );

    if(TWEAK_WineLook == WIN31_LOOK)
    {
	rect.bottom -= POPUP_YSHADE * GetSystemMetrics(SM_CYBORDER);
	rect.right -= POPUP_XSHADE * GetSystemMetrics(SM_CXBORDER);
    }

    if((hPrevBrush = SelectObject( hdc, GetSysColorBrush(COLOR_MENU) ))
        && (SelectObject( hdc, hMenuFont)))
    {
	HPEN hPrevPen;

	Rectangle( hdc, rect.left, rect.top, rect.right, rect.bottom );

	hPrevPen = SelectObject( hdc, GetStockObject( NULL_PEN ) );
	if( hPrevPen )
	{
	    INT ropPrev, i;
	    POPUPMENU *menu;

	    /* draw 3-d shade */
	    if(TWEAK_WineLook == WIN31_LOOK) {
		SelectObject( hdc, hShadeBrush );
		SetBkMode( hdc, TRANSPARENT );
		ropPrev = SetROP2( hdc, R2_MASKPEN );

		i = rect.right;		/* why SetBrushOrg() doesn't? */
		PatBlt( hdc, i & 0xfffffffe,
			  rect.top + POPUP_YSHADE*GetSystemMetrics(SM_CYBORDER),
			  i%2 + POPUP_XSHADE*GetSystemMetrics(SM_CXBORDER),
			  rect.bottom - rect.top, 0x00a000c9 );
		i = rect.bottom;
		PatBlt( hdc, rect.left + POPUP_XSHADE*GetSystemMetrics(SM_CXBORDER),
			  i & 0xfffffffe,rect.right - rect.left,
			  i%2 + POPUP_YSHADE*GetSystemMetrics(SM_CYBORDER), 0x00a000c9 );
		SelectObject( hdc, hPrevPen );
		SelectObject( hdc, hPrevBrush );
		SetROP2( hdc, ropPrev );
	    }
	    else
		DrawEdge (hdc, &rect, EDGE_RAISED, BF_RECT);

	    /* draw menu items */

	    menu = MENU_GetMenu( hmenu );
	    if (menu && menu->nItems)
	    {
		MENUITEM *item;
		UINT u;

		for (u = menu->nItems, item = menu->items; u > 0; u--, item++)
		    MENU_DrawMenuItem( hwnd, hmenu, menu->hwndOwner, hdc, item,
				       menu->Height, FALSE, ODA_DRAWENTIRE );

	    }
	} else
	{
	    SelectObject( hdc, hPrevBrush );
	}
    }
}

/***********************************************************************
 *           MENU_DrawMenuBar
 *
 * Paint a menu bar. Returns the height of the menu bar.
 * called from [windows/nonclient.c]
 */
UINT MENU_DrawMenuBar( HDC hDC, LPRECT lprect, HWND hwnd,
                         BOOL suppress_draw)
{
    LPPOPUPMENU lppop;
    UINT i,retvalue;
    HFONT hfontOld = 0;
    HMENU hMenu = GetMenu(hwnd);

    lppop = MENU_GetMenu( hMenu );
    if (lppop == NULL || lprect == NULL)
    {
        retvalue = GetSystemMetrics(SM_CYMENU);
        goto END;
    }

    TRACE("(%04x, %p, %p)\n", hDC, lprect, lppop);

    hfontOld = SelectObject( hDC, hMenuFont);

    if (lppop->Height == 0)
        MENU_MenuBarCalcSize(hDC, lprect, lppop, hwnd);

    lprect->bottom = lprect->top + lppop->Height;

    if (suppress_draw)
    {
        retvalue = lppop->Height;
        goto END;
    }

    FillRect(hDC, lprect, GetSysColorBrush(COLOR_MENU) );

    if (TWEAK_WineLook == WIN31_LOOK)
    {
	SelectObject( hDC, GetSysColorPen(COLOR_WINDOWFRAME) );
	MoveToEx( hDC, lprect->left, lprect->bottom, NULL );
	LineTo( hDC, lprect->right, lprect->bottom );
    }
    else
    {
	SelectObject( hDC, GetSysColorPen(COLOR_3DFACE));
	MoveToEx( hDC, lprect->left, lprect->bottom, NULL );
	LineTo( hDC, lprect->right, lprect->bottom );
    }

    if (lppop->nItems == 0)
    {
        retvalue = GetSystemMetrics(SM_CYMENU);
        goto END;
    }

    for (i = 0; i < lppop->nItems; i++)
    {
        MENU_DrawMenuItem( hwnd, hMenu, hwnd,
                           hDC, &lppop->items[i], lppop->Height, TRUE, ODA_DRAWENTIRE );
    }
    retvalue = lppop->Height;

END:
    if (hfontOld) SelectObject (hDC, hfontOld);
    return retvalue;
}


/***********************************************************************
 *           MENU_ShowPopup
 *
 * Display a popup menu.
 */
static BOOL MENU_ShowPopup( HWND hwndOwner, HMENU hmenu, UINT id,
                              INT x, INT y, INT xanchor, INT yanchor )
{
    POPUPMENU *menu;
    UINT width, height;

    TRACE("owner=0x%04x hmenu=0x%04x id=0x%04x x=0x%04x y=0x%04x xa=0x%04x ya=0x%04x\n",
    hwndOwner, hmenu, id, x, y, xanchor, yanchor);

    if (!(menu = MENU_GetMenu( hmenu ))) return FALSE;
    if (menu->FocusedItem != NO_SELECTED_ITEM)
    {
	menu->items[menu->FocusedItem].fState &= ~(MF_HILITE|MF_MOUSESELECT);
	menu->FocusedItem = NO_SELECTED_ITEM;
    }

    /* store the owner for DrawItem */
    menu->hwndOwner = hwndOwner;


    MENU_PopupMenuCalcSize( menu, hwndOwner );

    /* adjust popup menu pos so that it fits within the desktop */

    width = menu->Width + GetSystemMetrics(SM_CXBORDER);
    height = menu->Height + GetSystemMetrics(SM_CYBORDER);

    if( x + width > GetSystemMetrics(SM_CXSCREEN ))
    {
        if( xanchor )
            x -= width - xanchor;
        if( x + width > GetSystemMetrics(SM_CXSCREEN))
            x = GetSystemMetrics(SM_CXSCREEN) - width;
    }
    if( x < 0 ) x = 0;

    if( y + height > GetSystemMetrics(SM_CYSCREEN ))
    {
        if( yanchor )
            y -= height + yanchor;
        if( y + height > GetSystemMetrics(SM_CYSCREEN ))
            y = GetSystemMetrics(SM_CYSCREEN) - height;
    }
    if( y < 0 ) y = 0;

    if( TWEAK_WineLook == WIN31_LOOK )
    {
        width += POPUP_XSHADE * GetSystemMetrics(SM_CXBORDER);  /* add space for shading */
        height += POPUP_YSHADE * GetSystemMetrics(SM_CYBORDER);
    }

    /* NOTE: In Windows, top menu popup is not owned. */
    menu->hWnd = CreateWindowA( POPUPMENU_CLASS_ATOM, NULL,
                                WS_POPUP, x, y, width, height,
                                hwndOwner, 0, GetWindowLongA(hwndOwner,GWL_HINSTANCE),
                                (LPVOID)hmenu );
    if( !menu->hWnd ) return FALSE;
    if (!top_popup) top_popup = menu->hWnd;

    /* Display the window */

    SetWindowPos( menu->hWnd, HWND_TOP, 0, 0, 0, 0,
                  SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE );
    UpdateWindow( menu->hWnd );
    return TRUE;
}


/***********************************************************************
 *           MENU_SelectItem
 */
static void MENU_SelectItem( HWND hwndOwner, HMENU hmenu, UINT wIndex,
                             BOOL sendMenuSelect, HMENU topmenu )
{
    LPPOPUPMENU lppop;
    HDC hdc;

    TRACE("owner=0x%04x menu=0x%04x index=0x%04x select=0x%04x\n", hwndOwner, hmenu, wIndex, sendMenuSelect);

    lppop = MENU_GetMenu( hmenu );
    if ((!lppop) || (!lppop->nItems) || (!lppop->hWnd)) return;

    if (lppop->FocusedItem == wIndex) return;
    if (lppop->wFlags & MF_POPUP) hdc = GetDC( lppop->hWnd );
    else hdc = GetDCEx( lppop->hWnd, 0, DCX_CACHE | DCX_WINDOW);
    if (!top_popup) top_popup = lppop->hWnd;

    SelectObject( hdc, hMenuFont);

      /* Clear previous highlighted item */
    if (lppop->FocusedItem != NO_SELECTED_ITEM)
    {
	lppop->items[lppop->FocusedItem].fState &= ~(MF_HILITE|MF_MOUSESELECT);
	MENU_DrawMenuItem(lppop->hWnd, hmenu, hwndOwner, hdc,&lppop->items[lppop->FocusedItem],
                          lppop->Height, !(lppop->wFlags & MF_POPUP),
			  ODA_SELECT );
    }

      /* Highlight new item (if any) */
    lppop->FocusedItem = wIndex;
    if (lppop->FocusedItem != NO_SELECTED_ITEM)
    {
        if(!(lppop->items[wIndex].fType & MF_SEPARATOR)) {
            lppop->items[wIndex].fState |= MF_HILITE;
            MENU_DrawMenuItem( lppop->hWnd, hmenu, hwndOwner, hdc,
                    &lppop->items[wIndex], lppop->Height,
                    !(lppop->wFlags & MF_POPUP), ODA_SELECT );
        }
        if (sendMenuSelect)
        {
            MENUITEM *ip = &lppop->items[lppop->FocusedItem];
	    SendMessageA( hwndOwner, WM_MENUSELECT,
                     MAKELONG(ip->fType & MF_POPUP ? wIndex: ip->wID,
                     ip->fType | ip->fState | MF_MOUSESELECT |
                     (lppop->wFlags & MF_SYSMENU)), hmenu);
        }
    }
    else if (sendMenuSelect) {
        if(topmenu){
            int pos;
            if((pos=MENU_FindSubMenu(&topmenu, hmenu))!=NO_SELECTED_ITEM){
                POPUPMENU *ptm = MENU_GetMenu( topmenu );
                MENUITEM *ip = &ptm->items[pos];
                SendMessageA( hwndOwner, WM_MENUSELECT, MAKELONG(pos,
                         ip->fType | ip->fState | MF_MOUSESELECT |
                         (ptm->wFlags & MF_SYSMENU)), topmenu);
            }
        }
    }
    ReleaseDC( lppop->hWnd, hdc );
}


/***********************************************************************
 *           MENU_MoveSelection
 *
 * Moves currently selected item according to the offset parameter.
 * If there is no selection then it should select the last item if
 * offset is ITEM_PREV or the first item if offset is ITEM_NEXT.
 */
static void MENU_MoveSelection( HWND hwndOwner, HMENU hmenu, INT offset )
{
    INT i;
    POPUPMENU *menu;

    TRACE("hwnd=0x%04x hmenu=0x%04x off=0x%04x\n", hwndOwner, hmenu, offset);

    menu = MENU_GetMenu( hmenu );
    if ((!menu) || (!menu->items)) return;

    if ( menu->FocusedItem != NO_SELECTED_ITEM )
    {
	if( menu->nItems == 1 ) return; else
	for (i = menu->FocusedItem + offset ; i >= 0 && i < menu->nItems
					    ; i += offset)
	    if (!(menu->items[i].fType & MF_SEPARATOR))
	    {
		MENU_SelectItem( hwndOwner, hmenu, i, TRUE, 0 );
		return;
	    }
    }

    for ( i = (offset > 0) ? 0 : menu->nItems - 1;
		  i >= 0 && i < menu->nItems ; i += offset)
	if (!(menu->items[i].fType & MF_SEPARATOR))
	{
	    MENU_SelectItem( hwndOwner, hmenu, i, TRUE, 0 );
	    return;
	}
}


/**********************************************************************
 *         MENU_SetItemData
 *
 * Set an item flags, id and text ptr. Called by InsertMenu() and
 * ModifyMenu().
 */
static BOOL MENU_SetItemData( MENUITEM *item, UINT flags, UINT id,
                                LPCWSTR str )
{
    LPWSTR prevText = IS_STRING_ITEM(item->fType) ? item->text : NULL;

    debug_print_menuitem("MENU_SetItemData from: ", item, "");
    TRACE("flags=%x str=%p\n", flags, str);

    if (IS_STRING_ITEM(flags))
    {
        if (!str)
        {
            flags |= MF_SEPARATOR;
            item->text = NULL;
        }
        else
        {
            LPWSTR text;
            /* Item beginning with a backspace is a help item */
            if (*str == '\b')
            {
                flags |= MF_HELP;
                str++;
            }
            if (!(text = HeapAlloc( GetProcessHeap(), 0, (strlenW(str)+1) * sizeof(WCHAR) )))
                return FALSE;
            strcpyW( text, str );
            item->text = text;
        }
    }
    else if (IS_BITMAP_ITEM(flags))
        item->text = (LPWSTR)(HBITMAP)LOWORD(str);
    else item->text = NULL;

    if (flags & MF_OWNERDRAW)
        item->dwItemData = (DWORD)str;
    else
        item->dwItemData = 0;

    if ((item->fType & MF_POPUP) && (flags & MF_POPUP) && (item->hSubMenu != id) )
	DestroyMenu( item->hSubMenu );   /* ModifyMenu() spec */

    if (flags & MF_POPUP)
    {
	POPUPMENU *menu = MENU_GetMenu((UINT16)id);
        if (menu) menu->wFlags |= MF_POPUP;
	else
        {
            item->wID = 0;
            item->hSubMenu = 0;
            item->fType = 0;
            item->fState = 0;
	    return FALSE;
        }
    }

    item->wID = id;
    if (flags & MF_POPUP)
      item->hSubMenu = id;

    if ((item->fType & MF_POPUP) && !(flags & MF_POPUP) )
      flags |= MF_POPUP; /* keep popup */

    item->fType = flags & TYPE_MASK;
    item->fState = (flags & STATE_MASK) &
        ~(MF_HILITE | MF_MOUSESELECT | MF_BYPOSITION);


    /* Don't call SetRectEmpty here! */


    if (prevText) HeapFree( GetProcessHeap(), 0, prevText );

    debug_print_menuitem("MENU_SetItemData to  : ", item, "");
    return TRUE;
}


/**********************************************************************
 *         MENU_InsertItem
 *
 * Insert a new item into a menu.
 */
static MENUITEM *MENU_InsertItem( HMENU hMenu, UINT pos, UINT flags )
{
    MENUITEM *newItems;
    POPUPMENU *menu;

    if (!(menu = MENU_GetMenu(hMenu)))
        return NULL;

    /* Find where to insert new item */

    if (flags & MF_BYPOSITION) {
        if (pos > menu->nItems)
            pos = menu->nItems;
    } else {
        if (!MENU_FindItem( &hMenu, &pos, flags ))
            pos = menu->nItems;
        else {
            if (!(menu = MENU_GetMenu( hMenu )))
                return NULL;
        }
    }

    /* Create new items array */

    newItems = HeapAlloc( GetProcessHeap(), 0, sizeof(MENUITEM) * (menu->nItems+1) );
    if (!newItems)
    {
        WARN("allocation failed\n" );
        return NULL;
    }
    if (menu->nItems > 0)
    {
	  /* Copy the old array into the new one */
	if (pos > 0) memcpy( newItems, menu->items, pos * sizeof(MENUITEM) );
	if (pos < menu->nItems) memcpy( &newItems[pos+1], &menu->items[pos],
					(menu->nItems-pos)*sizeof(MENUITEM) );
        HeapFree( GetProcessHeap(), 0, menu->items );
    }
    menu->items = newItems;
    menu->nItems++;
    memset( &newItems[pos], 0, sizeof(*newItems) );
    menu->Height = 0; /* force size recalculate */
    return &newItems[pos];
}


/**********************************************************************
 *         MENU_ParseResource
 *
 * Parse a standard menu resource and add items to the menu.
 * Return a pointer to the end of the resource.
 */
static LPCSTR MENU_ParseResource( LPCSTR res, HMENU hMenu, BOOL unicode )
{
    WORD flags, id = 0;
    LPCSTR str;

    do
    {
        flags = GET_WORD(res);
        res += sizeof(WORD);
        if (!(flags & MF_POPUP))
        {
            id = GET_WORD(res);
            res += sizeof(WORD);
        }
        if (!IS_STRING_ITEM(flags))
            ERR("not a string item %04x\n", flags );
        str = res;
        if (!unicode) res += strlen(str) + 1;
        else res += (strlenW((LPCWSTR)str) + 1) * sizeof(WCHAR);
        if (flags & MF_POPUP)
        {
            HMENU hSubMenu = CreatePopupMenu();
            if (!hSubMenu) return NULL;
            if (!(res = MENU_ParseResource( res, hSubMenu, unicode )))
                return NULL;
            if (!unicode) AppendMenuA( hMenu, flags, (UINT)hSubMenu, str );
            else AppendMenuW( hMenu, flags, (UINT)hSubMenu, (LPCWSTR)str );
        }
        else  /* Not a popup */
        {
            if (!unicode) AppendMenuA( hMenu, flags, id, *str ? str : NULL );
            else AppendMenuW( hMenu, flags, id,
                                *(LPCWSTR)str ? (LPCWSTR)str : NULL );
        }
    } while (!(flags & MF_END));
    return res;
}


/**********************************************************************
 *         MENUEX_ParseResource
 *
 * Parse an extended menu resource and add items to the menu.
 * Return a pointer to the end of the resource.
 */
static LPCSTR MENUEX_ParseResource( LPCSTR res, HMENU hMenu)
{
    WORD resinfo;
    do {
	MENUITEMINFOW mii;

	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
	mii.fType = GET_DWORD(res);
        res += sizeof(DWORD);
	mii.fState = GET_DWORD(res);
        res += sizeof(DWORD);
	mii.wID = GET_DWORD(res);
        res += sizeof(DWORD);
	resinfo = GET_WORD(res); /* FIXME: for 16-bit apps this is a byte.  */
        res += sizeof(WORD);
	/* Align the text on a word boundary.  */
	res += (~((int)res - 1)) & 1;
	mii.dwTypeData = (LPWSTR) res;
	res += (1 + strlenW(mii.dwTypeData)) * sizeof(WCHAR);
	/* Align the following fields on a dword boundary.  */
	res += (~((int)res - 1)) & 3;

        TRACE("Menu item: [%08x,%08x,%04x,%04x,%s]\n",
              mii.fType, mii.fState, mii.wID, resinfo, debugstr_w(mii.dwTypeData));

	if (resinfo & 1) {	/* Pop-up? */
	    /* DWORD helpid = GET_DWORD(res); FIXME: use this.  */
	    res += sizeof(DWORD);
	    mii.hSubMenu = CreatePopupMenu();
	    if (!mii.hSubMenu)
		return NULL;
	    if (!(res = MENUEX_ParseResource(res, mii.hSubMenu))) {
		DestroyMenu(mii.hSubMenu);
                return NULL;
        }
	    mii.fMask |= MIIM_SUBMENU;
	    mii.fType |= MF_POPUP;
        }
	InsertMenuItemW(hMenu, -1, MF_BYPOSITION, &mii);
    } while (!(resinfo & MF_END));
    return res;
}


/***********************************************************************
 *           MENU_GetSubPopup
 *
 * Return the handle of the selected sub-popup menu (if any).
 */
static HMENU MENU_GetSubPopup( HMENU hmenu )
{
    POPUPMENU *menu;
    MENUITEM *item;

    menu = MENU_GetMenu( hmenu );

    if ((!menu) || (menu->FocusedItem == NO_SELECTED_ITEM)) return 0;

    item = &menu->items[menu->FocusedItem];
    if ((item->fType & MF_POPUP) && (item->fState & MF_MOUSESELECT))
        return item->hSubMenu;
    return 0;
}


/***********************************************************************
 *           MENU_HideSubPopups
 *
 * Hide the sub-popup menus of this menu.
 */
static void MENU_HideSubPopups( HWND hwndOwner, HMENU hmenu,
                                BOOL sendMenuSelect )
{
    POPUPMENU *menu = MENU_GetMenu( hmenu );

    TRACE("owner=0x%04x hmenu=0x%04x 0x%04x\n", hwndOwner, hmenu, sendMenuSelect);

    if (menu && top_popup)
    {
	HMENU hsubmenu;
	POPUPMENU *submenu;
	MENUITEM *item;

	if (menu->FocusedItem != NO_SELECTED_ITEM)
	{
	    item = &menu->items[menu->FocusedItem];
	    if (!(item->fType & MF_POPUP) ||
		!(item->fState & MF_MOUSESELECT)) return;
	    item->fState &= ~MF_MOUSESELECT;
	    hsubmenu = item->hSubMenu;
	} else return;

	submenu = MENU_GetMenu( hsubmenu );
	MENU_HideSubPopups( hwndOwner, hsubmenu, FALSE );
	MENU_SelectItem( hwndOwner, hsubmenu, NO_SELECTED_ITEM, sendMenuSelect, 0 );
        DestroyWindow( submenu->hWnd );
        submenu->hWnd = 0;
    }
}


/***********************************************************************
 *           MENU_ShowSubPopup
 *
 * Display the sub-menu of the selected item of this menu.
 * Return the handle of the submenu, or hmenu if no submenu to display.
 */
static HMENU MENU_ShowSubPopup( HWND hwndOwner, HMENU hmenu,
                                  BOOL selectFirst, UINT wFlags )
{
    RECT rect;
    POPUPMENU *menu;
    MENUITEM *item;
    HDC hdc;

    TRACE("owner=0x%04x hmenu=0x%04x 0x%04x\n", hwndOwner, hmenu, selectFirst);

    if (!(menu = MENU_GetMenu( hmenu ))) return hmenu;

    if (menu->FocusedItem == NO_SELECTED_ITEM) return hmenu;

    item = &menu->items[menu->FocusedItem];
    if (!(item->fType & MF_POPUP) || (item->fState & (MF_GRAYED | MF_DISABLED)))
        return hmenu;

    /* message must be sent before using item,
       because nearly everything may be changed by the application ! */

    /* Send WM_INITMENUPOPUP message only if TPM_NONOTIFY flag is not specified */
    if (!(wFlags & TPM_NONOTIFY))
       SendMessageA( hwndOwner, WM_INITMENUPOPUP, item->hSubMenu,
		   MAKELONG( menu->FocusedItem, IS_SYSTEM_MENU(menu) ));

    item = &menu->items[menu->FocusedItem];
    rect = item->rect;

    /* correct item if modified as a reaction to WM_INITMENUPOPUP message */
    if (!(item->fState & MF_HILITE))
    {
        if (menu->wFlags & MF_POPUP) hdc = GetDC( menu->hWnd );
        else hdc = GetDCEx( menu->hWnd, 0, DCX_CACHE | DCX_WINDOW);

        SelectObject( hdc, hMenuFont);

        item->fState |= MF_HILITE;
        MENU_DrawMenuItem( menu->hWnd, hmenu, hwndOwner, hdc, item, menu->Height, !(menu->wFlags & MF_POPUP), ODA_DRAWENTIRE );
	ReleaseDC( menu->hWnd, hdc );
    }
    if (!item->rect.top && !item->rect.left && !item->rect.bottom && !item->rect.right)
      item->rect = rect;

    item->fState |= MF_MOUSESELECT;

    if (IS_SYSTEM_MENU(menu))
    {
	MENU_InitSysMenuPopup(item->hSubMenu,
                              GetWindowLongA( menu->hWnd, GWL_STYLE ),
                              GetClassLongA( menu->hWnd, GCL_STYLE));

	NC_GetSysPopupPos( menu->hWnd, &rect );
	rect.top = rect.bottom;
	rect.right = GetSystemMetrics(SM_CXSIZE);
        rect.bottom = GetSystemMetrics(SM_CYSIZE);
    }
    else
    {
        GetWindowRect( menu->hWnd, &rect );
	if (menu->wFlags & MF_POPUP)
	{
	    rect.left += item->rect.right - GetSystemMetrics(SM_CXBORDER);
	    rect.top += item->rect.top;
	    rect.right = item->rect.left - item->rect.right + GetSystemMetrics(SM_CXBORDER);
	    rect.bottom = item->rect.top - item->rect.bottom;
	}
	else
	{
	    rect.left += item->rect.left;
	    rect.top += item->rect.bottom;
	    rect.right = item->rect.right - item->rect.left;
	    rect.bottom = item->rect.bottom - item->rect.top;
	}
    }

    MENU_ShowPopup( hwndOwner, item->hSubMenu, menu->FocusedItem,
		    rect.left, rect.top, rect.right, rect.bottom );
    if (selectFirst)
        MENU_MoveSelection( hwndOwner, item->hSubMenu, ITEM_NEXT );
    return item->hSubMenu;
}



/**********************************************************************
 *         MENU_IsMenuActive
 */
BOOL MENU_IsMenuActive(void)
{
    return (top_popup != 0);
}

/***********************************************************************
 *           MENU_PtMenu
 *
 * Walks menu chain trying to find a menu pt maps to.
 */
static HMENU MENU_PtMenu( HMENU hMenu, POINT pt )
{
   POPUPMENU *menu = MENU_GetMenu( hMenu );
   UINT item = menu->FocusedItem;
   HMENU ret;

   /* try subpopup first (if any) */
   ret = (item != NO_SELECTED_ITEM &&
          (menu->items[item].fType & MF_POPUP) &&
          (menu->items[item].fState & MF_MOUSESELECT))
        ? MENU_PtMenu(menu->items[item].hSubMenu, pt) : 0;

   if (!ret)  /* check the current window (avoiding WM_HITTEST) */
   {
       INT ht = NC_HandleNCHitTest( menu->hWnd, pt );
       if( menu->wFlags & MF_POPUP )
       {
           if (ht != HTNOWHERE && ht != HTERROR) ret = hMenu;
       }
       else if (ht == HTSYSMENU)
           ret = get_win_sys_menu( menu->hWnd );
       else if (ht == HTMENU)
           ret = GetMenu( menu->hWnd );
   }
   return ret;
}

/***********************************************************************
 *           MENU_ExecFocusedItem
 *
 * Execute a menu item (for instance when user pressed Enter).
 * Return the wID of the executed item. Otherwise, -1 indicating
 * that no menu item was executed;
 * Have to receive the flags for the TrackPopupMenu options to avoid
 * sending unwanted message.
 *
 */
static INT MENU_ExecFocusedItem( MTRACKER* pmt, HMENU hMenu, UINT wFlags )
{
    MENUITEM *item;
    POPUPMENU *menu = MENU_GetMenu( hMenu );

    TRACE("%p hmenu=0x%04x\n", pmt, hMenu);

    if (!menu || !menu->nItems ||
	(menu->FocusedItem == NO_SELECTED_ITEM)) return -1;

    item = &menu->items[menu->FocusedItem];

    TRACE("%08x %08x %08x\n",
                 hMenu, item->wID, item->hSubMenu);

    if (!(item->fType & MF_POPUP))
    {
	if (!(item->fState & (MF_GRAYED | MF_DISABLED)) && !(item->fType & MF_SEPARATOR))
	{
	    /* If TPM_RETURNCMD is set you return the id, but
	       do not send a message to the owner */
	    if(!(wFlags & TPM_RETURNCMD))
	    {
		if( menu->wFlags & MF_SYSMENU )
		    PostMessageA( pmt->hOwnerWnd, WM_SYSCOMMAND, item->wID,
				  MAKELPARAM((INT16)pmt->pt.x, (INT16)pmt->pt.y) );
		else
		    PostMessageA( pmt->hOwnerWnd, WM_COMMAND, item->wID, 0 );
	    }
	    return item->wID;
	}
    }
    else
	pmt->hCurrentMenu = MENU_ShowSubPopup(pmt->hOwnerWnd, hMenu, TRUE, wFlags);

    return -1;
}

/***********************************************************************
 *           MENU_SwitchTracking
 *
 * Helper function for menu navigation routines.
 */
static void MENU_SwitchTracking( MTRACKER* pmt, HMENU hPtMenu, UINT id )
{
    POPUPMENU *ptmenu = MENU_GetMenu( hPtMenu );
    POPUPMENU *topmenu = MENU_GetMenu( pmt->hTopMenu );

    TRACE("%p hmenu=0x%04x 0x%04x\n", pmt, hPtMenu, id);

    if( pmt->hTopMenu != hPtMenu &&
	!((ptmenu->wFlags | topmenu->wFlags) & MF_POPUP) )
    {
	/* both are top level menus (system and menu-bar) */
	MENU_HideSubPopups( pmt->hOwnerWnd, pmt->hTopMenu, FALSE );
	MENU_SelectItem( pmt->hOwnerWnd, pmt->hTopMenu, NO_SELECTED_ITEM, FALSE, 0 );
        pmt->hTopMenu = hPtMenu;
    }
    else MENU_HideSubPopups( pmt->hOwnerWnd, hPtMenu, FALSE );
    MENU_SelectItem( pmt->hOwnerWnd, hPtMenu, id, TRUE, 0 );
}


/***********************************************************************
 *           MENU_ButtonDown
 *
 * Return TRUE if we can go on with menu tracking.
 */
static BOOL MENU_ButtonDown( MTRACKER* pmt, HMENU hPtMenu, UINT wFlags )
{
    TRACE("%p hmenu=0x%04x\n", pmt, hPtMenu);

    if (hPtMenu)
    {
	UINT id = 0;
	POPUPMENU *ptmenu = MENU_GetMenu( hPtMenu );
	MENUITEM *item;

	if( IS_SYSTEM_MENU(ptmenu) )
	    item = ptmenu->items;
	else
	    item = MENU_FindItemByCoords( ptmenu, pmt->pt, &id );

	if( item )
	{
	    if( ptmenu->FocusedItem != id )
		MENU_SwitchTracking( pmt, hPtMenu, id );

	    /* If the popup menu is not already "popped" */
	    if(!(item->fState & MF_MOUSESELECT ))
	    {
		pmt->hCurrentMenu = MENU_ShowSubPopup( pmt->hOwnerWnd, hPtMenu, FALSE, wFlags );

		/* In win31, a newly popped menu always remains opened for the next buttonup */
		if(TWEAK_WineLook == WIN31_LOOK)
		    ptmenu->bTimeToHide = FALSE;
	    }

	    return TRUE;
	}
	/* Else the click was on the menu bar, finish the tracking */
    }
    return FALSE;
}

/***********************************************************************
 *           MENU_ButtonUp
 *
 * Return the value of MENU_ExecFocusedItem if
 * the selected item was not a popup. Else open the popup.
 * A -1 return value indicates that we go on with menu tracking.
 *
 */
static INT MENU_ButtonUp( MTRACKER* pmt, HMENU hPtMenu, UINT wFlags)
{
    TRACE("%p hmenu=0x%04x\n", pmt, hPtMenu);

    if (hPtMenu)
    {
	UINT id = 0;
	POPUPMENU *ptmenu = MENU_GetMenu( hPtMenu );
	MENUITEM *item;

        if( IS_SYSTEM_MENU(ptmenu) )
            item = ptmenu->items;
        else
            item = MENU_FindItemByCoords( ptmenu, pmt->pt, &id );

	if( item && (ptmenu->FocusedItem == id ))
	{
	    if( !(item->fType & MF_POPUP) )
		return MENU_ExecFocusedItem( pmt, hPtMenu, wFlags);

	    /* If we are dealing with the top-level menu            */
	    /* and this is a click on an already "popped" item:     */
	    /* Stop the menu tracking and close the opened submenus */
	    if((pmt->hTopMenu == hPtMenu) && (ptmenu->bTimeToHide == TRUE))
		return 0;
	}
	ptmenu->bTimeToHide = TRUE;
    }
    return -1;
}


/***********************************************************************
 *           MENU_MouseMove
 *
 * Return TRUE if we can go on with menu tracking.
 */
static BOOL MENU_MouseMove( MTRACKER* pmt, HMENU hPtMenu, UINT wFlags )
{
    UINT id = NO_SELECTED_ITEM;
    POPUPMENU *ptmenu = NULL;

    if( hPtMenu )
    {
	ptmenu = MENU_GetMenu( hPtMenu );
        if( IS_SYSTEM_MENU(ptmenu) )
	    id = 0;
        else
            MENU_FindItemByCoords( ptmenu, pmt->pt, &id );
    }

    if( id == NO_SELECTED_ITEM )
    {
	MENU_SelectItem( pmt->hOwnerWnd, pmt->hCurrentMenu,
			 NO_SELECTED_ITEM, TRUE, pmt->hTopMenu);

    }
    else if( ptmenu->FocusedItem != id )
    {
	    MENU_SwitchTracking( pmt, hPtMenu, id );
	    pmt->hCurrentMenu = MENU_ShowSubPopup(pmt->hOwnerWnd, hPtMenu, FALSE, wFlags);
    }
    return TRUE;
}


/***********************************************************************
 *           MENU_DoNextMenu
 *
 * NOTE: WM_NEXTMENU documented in Win32 is a bit different.
 */
static LRESULT MENU_DoNextMenu( MTRACKER* pmt, UINT vk )
{
    POPUPMENU *menu = MENU_GetMenu( pmt->hTopMenu );

    if( (vk == VK_LEFT &&  menu->FocusedItem == 0 ) ||
        (vk == VK_RIGHT && menu->FocusedItem == menu->nItems - 1))
    {
        MDINEXTMENU next_menu;
	HMENU hNewMenu;
	HWND  hNewWnd;
	UINT  id = 0;

        next_menu.hmenuIn = (IS_SYSTEM_MENU(menu)) ? GetSubMenu(pmt->hTopMenu,0) : pmt->hTopMenu;
        next_menu.hmenuNext = 0;
        next_menu.hwndNext = 0;
        SendMessageW( pmt->hOwnerWnd, WM_NEXTMENU, vk, (LPARAM)&next_menu );

	TRACE("%04x [%04x] -> %04x [%04x]\n",
              pmt->hCurrentMenu, pmt->hOwnerWnd, next_menu.hmenuNext, next_menu.hwndNext );

	if (!next_menu.hmenuNext || !next_menu.hwndNext)
	{
            DWORD style = GetWindowLongA( pmt->hOwnerWnd, GWL_STYLE );
	    hNewWnd = pmt->hOwnerWnd;
	    if( IS_SYSTEM_MENU(menu) )
	    {
		/* switch to the menu bar */

                if(style & WS_CHILD || !(hNewMenu = GetMenu(hNewWnd))) return FALSE;

	        if( vk == VK_LEFT )
	        {
		    menu = MENU_GetMenu( hNewMenu );
		    id = menu->nItems - 1;
	        }
	    }
	    else if (style & WS_SYSMENU )
	    {
		/* switch to the system menu */
	        hNewMenu = get_win_sys_menu( hNewWnd );
	    }
            else return FALSE;
	}
	else    /* application returned a new menu to switch to */
	{
            hNewMenu = next_menu.hmenuNext;
            hNewWnd = WIN_GetFullHandle( next_menu.hwndNext );

	    if( IsMenu(hNewMenu) && IsWindow(hNewWnd) )
	    {
                DWORD style = GetWindowLongA( hNewWnd, GWL_STYLE );

		if (style & WS_SYSMENU &&
		    GetSubMenu(get_win_sys_menu(hNewWnd), 0) == hNewMenu )
		{
	            /* get the real system menu */
		    hNewMenu =  get_win_sys_menu(hNewWnd);
		}
	        else if (style & WS_CHILD || GetMenu(hNewWnd) != hNewMenu )
		{
		    /* FIXME: Not sure what to do here;
		     * perhaps try to track hNewMenu as a popup? */

		    TRACE(" -- got confused.\n");
		    return FALSE;
		}
	    }
	    else return FALSE;
	}

	if( hNewMenu != pmt->hTopMenu )
	{
	    MENU_SelectItem( pmt->hOwnerWnd, pmt->hTopMenu, NO_SELECTED_ITEM,
                    FALSE, 0 );
	    if( pmt->hCurrentMenu != pmt->hTopMenu )
		MENU_HideSubPopups( pmt->hOwnerWnd, pmt->hTopMenu, FALSE );
	}

	if( hNewWnd != pmt->hOwnerWnd )
	{
	    ReleaseCapture();
	    pmt->hOwnerWnd = hNewWnd;
	    EVENT_Capture( pmt->hOwnerWnd, HTMENU );
	}

	pmt->hTopMenu = pmt->hCurrentMenu = hNewMenu; /* all subpopups are hidden */
	MENU_SelectItem( pmt->hOwnerWnd, pmt->hTopMenu, id, TRUE, 0 );

	return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           MENU_SuspendPopup
 *
 * The idea is not to show the popup if the next input message is
 * going to hide it anyway.
 */
static BOOL MENU_SuspendPopup( MTRACKER* pmt, UINT16 uMsg )
{
    MSG msg;

    msg.hwnd = pmt->hOwnerWnd;

    PeekMessageA( &msg, 0, 0, 0, PM_NOYIELD | PM_REMOVE);
    pmt->trackFlags |= TF_SKIPREMOVE;

    switch( uMsg )
    {
	case WM_KEYDOWN:
	     PeekMessageA( &msg, 0, 0, 0, PM_NOYIELD | PM_NOREMOVE);
	     if( msg.message == WM_KEYUP || msg.message == WM_PAINT )
	     {
		 PeekMessageA( &msg, 0, 0, 0, PM_NOYIELD | PM_REMOVE);
	         PeekMessageA( &msg, 0, 0, 0, PM_NOYIELD | PM_NOREMOVE);
	         if( msg.message == WM_KEYDOWN &&
		    (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT))
	         {
		     pmt->trackFlags |= TF_SUSPENDPOPUP;
		     return TRUE;
	         }
	     }
	     break;
    }

    /* failures go through this */
    pmt->trackFlags &= ~TF_SUSPENDPOPUP;
    return FALSE;
}

/***********************************************************************
 *           MENU_KeyEscape
 *
 * Handle a VK_ESCAPE key event in a menu.
 */
static BOOL MENU_KeyEscape(MTRACKER* pmt, UINT wFlags)
{
    BOOL bEndMenu = TRUE;

    if (pmt->hCurrentMenu != pmt->hTopMenu)
    {
        POPUPMENU *menu = MENU_GetMenu(pmt->hCurrentMenu);

        if (menu->wFlags & MF_POPUP)
        {
            HMENU hmenutmp, hmenuprev;

            hmenuprev = hmenutmp = pmt->hTopMenu;

            /* close topmost popup */
            while (hmenutmp != pmt->hCurrentMenu)
            {
                hmenuprev = hmenutmp;
                hmenutmp = MENU_GetSubPopup( hmenuprev );
            }

            MENU_HideSubPopups( pmt->hOwnerWnd, hmenuprev, TRUE );
            pmt->hCurrentMenu = hmenuprev;
            bEndMenu = FALSE;
        }
    }

    return bEndMenu;
}

/***********************************************************************
 *           MENU_KeyLeft
 *
 * Handle a VK_LEFT key event in a menu.
 */
static void MENU_KeyLeft( MTRACKER* pmt, UINT wFlags )
{
    POPUPMENU *menu;
    HMENU hmenutmp, hmenuprev;
    UINT  prevcol;

    hmenuprev = hmenutmp = pmt->hTopMenu;
    menu = MENU_GetMenu( hmenutmp );

    /* Try to move 1 column left (if possible) */
    if( (prevcol = MENU_GetStartOfPrevColumn( pmt->hCurrentMenu )) !=
	NO_SELECTED_ITEM ) {

	MENU_SelectItem( pmt->hOwnerWnd, pmt->hCurrentMenu,
			 prevcol, TRUE, 0 );
	return;
    }

    /* close topmost popup */
    while (hmenutmp != pmt->hCurrentMenu)
    {
	hmenuprev = hmenutmp;
	hmenutmp = MENU_GetSubPopup( hmenuprev );
    }

    MENU_HideSubPopups( pmt->hOwnerWnd, hmenuprev, TRUE );
    pmt->hCurrentMenu = hmenuprev;

    if ( (hmenuprev == pmt->hTopMenu) && !(menu->wFlags & MF_POPUP) )
    {
	/* move menu bar selection if no more popups are left */

	if( !MENU_DoNextMenu( pmt, VK_LEFT) )
	     MENU_MoveSelection( pmt->hOwnerWnd, pmt->hTopMenu, ITEM_PREV );

	if ( hmenuprev != hmenutmp || pmt->trackFlags & TF_SUSPENDPOPUP )
	{
	   /* A sublevel menu was displayed - display the next one
	    * unless there is another displacement coming up */

	    if( !MENU_SuspendPopup( pmt, WM_KEYDOWN ) )
		pmt->hCurrentMenu = MENU_ShowSubPopup(pmt->hOwnerWnd,
						pmt->hTopMenu, TRUE, wFlags);
	}
    }
}


/***********************************************************************
 *           MENU_KeyRight
 *
 * Handle a VK_RIGHT key event in a menu.
 */
static void MENU_KeyRight( MTRACKER* pmt, UINT wFlags )
{
    HMENU hmenutmp;
    POPUPMENU *menu = MENU_GetMenu( pmt->hTopMenu );
    UINT  nextcol;

    TRACE("MENU_KeyRight called, cur %x (%s), top %x (%s).\n",
		  pmt->hCurrentMenu,
		  debugstr_w((MENU_GetMenu(pmt->hCurrentMenu))->
		     items[0].text),
		  pmt->hTopMenu, debugstr_w(menu->items[0].text) );

    if ( (menu->wFlags & MF_POPUP) || (pmt->hCurrentMenu != pmt->hTopMenu))
    {
	/* If already displaying a popup, try to display sub-popup */

	hmenutmp = pmt->hCurrentMenu;
	pmt->hCurrentMenu = MENU_ShowSubPopup(pmt->hOwnerWnd, hmenutmp, TRUE, wFlags);

	/* if subpopup was displayed then we are done */
	if (hmenutmp != pmt->hCurrentMenu) return;
    }

    /* Check to see if there's another column */
    if( (nextcol = MENU_GetStartOfNextColumn( pmt->hCurrentMenu )) !=
	NO_SELECTED_ITEM ) {
	TRACE("Going to %d.\n", nextcol );
	MENU_SelectItem( pmt->hOwnerWnd, pmt->hCurrentMenu,
			 nextcol, TRUE, 0 );
	return;
    }

    if (!(menu->wFlags & MF_POPUP))	/* menu bar tracking */
    {
	if( pmt->hCurrentMenu != pmt->hTopMenu )
	{
	    MENU_HideSubPopups( pmt->hOwnerWnd, pmt->hTopMenu, FALSE );
	    hmenutmp = pmt->hCurrentMenu = pmt->hTopMenu;
	} else hmenutmp = 0;

	/* try to move to the next item */
	if( !MENU_DoNextMenu( pmt, VK_RIGHT) )
	     MENU_MoveSelection( pmt->hOwnerWnd, pmt->hTopMenu, ITEM_NEXT );

	if( hmenutmp || pmt->trackFlags & TF_SUSPENDPOPUP )
	    if( !MENU_SuspendPopup(pmt, WM_KEYDOWN) )
		pmt->hCurrentMenu = MENU_ShowSubPopup(pmt->hOwnerWnd,
						       pmt->hTopMenu, TRUE, wFlags);
    }
}

/***********************************************************************
 *           MENU_TrackMenu
 *
 * Menu tracking code.
 */
static INT MENU_TrackMenu( HMENU hmenu, UINT wFlags, INT x, INT y,
                              HWND hwnd, const RECT *lprect )
{
    MSG msg;
    POPUPMENU *menu;
    BOOL fRemove;
    INT executedMenuId = -1;
    MTRACKER mt;
    BOOL enterIdleSent = FALSE;

    mt.trackFlags = 0;
    mt.hCurrentMenu = hmenu;
    mt.hTopMenu = hmenu;
    mt.hOwnerWnd = WIN_GetFullHandle( hwnd );
    mt.pt.x = x;
    mt.pt.y = y;

    TRACE("hmenu=0x%04x flags=0x%08x (%d,%d) hwnd=0x%04x (%d,%d)-(%d,%d)\n",
	    hmenu, wFlags, x, y, hwnd, (lprect) ? lprect->left : 0, (lprect) ? lprect->top : 0,
	    (lprect) ? lprect->right : 0,  (lprect) ? lprect->bottom : 0);

    fEndMenu = FALSE;
    if (!(menu = MENU_GetMenu( hmenu ))) return FALSE;

    if (wFlags & TPM_BUTTONDOWN)
    {
	/* Get the result in order to start the tracking or not */
	fRemove = MENU_ButtonDown( &mt, hmenu, wFlags );
	fEndMenu = !fRemove;
    }

    EVENT_Capture( mt.hOwnerWnd, HTMENU );

    while (!fEndMenu)
    {
	menu = MENU_GetMenu( mt.hCurrentMenu );
	if (!menu) /* sometimes happens if I do a window manager close */
	    break;

	/* we have to keep the message in the queue until it's
	 * clear that menu loop is not over yet. */

        for (;;)
        {
            if (PeekMessageA( &msg, 0, 0, 0, PM_NOREMOVE ))
            {
                if (!CallMsgFilterA( &msg, MSGF_MENU )) break;
                /* remove the message from the queue */
                PeekMessageA( &msg, 0, msg.message, msg.message, PM_REMOVE );
            }
            else
            {
                if (!enterIdleSent)
                {
                    HWND win = (wFlags & TPM_ENTERIDLEEX && menu->wFlags & MF_POPUP) ? menu->hWnd : 0;
                    enterIdleSent = TRUE;
                    SendMessageW( mt.hOwnerWnd, WM_ENTERIDLE, MSGF_MENU, (LPARAM)win );
                }
                WaitMessage();
            }
        }

	/* check if EndMenu() tried to cancel us, by posting this message */
        if(msg.message == WM_CANCELMODE)
	{
	    /* we are now out of the loop */
    	    fEndMenu = TRUE;

	    /* remove the message from the queue */
	    PeekMessageA( &msg, 0, msg.message, msg.message, PM_REMOVE );

	    /* break out of internal loop, ala ESCAPE */
	    break;
	}

        TranslateMessage( &msg );
        mt.pt = msg.pt;

	if ( (msg.hwnd==menu->hWnd) || (msg.message!=WM_TIMER) )
	  enterIdleSent=FALSE;

        fRemove = FALSE;
	if ((msg.message >= WM_MOUSEFIRST) && (msg.message <= WM_MOUSELAST))
	{
            /*
             * use the mouse coordinates in lParam instead of those in the MSG
             * struct to properly handle synthetic messages. lParam coords are
             * relative to client area, so they must be converted; since they can
             * be negative, we must use SLOWORD/SHIWORD instead of LOWORD/HIWORD.
             */
            mt.pt.x = SLOWORD(msg.lParam);
            mt.pt.y = SHIWORD(msg.lParam);
            ClientToScreen(msg.hwnd,&mt.pt);

	    /* Find a menu for this mouse event */
	    hmenu = MENU_PtMenu( mt.hTopMenu, mt.pt );

	    switch(msg.message)
	    {
		/* no WM_NC... messages in captured state */

		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		    if (!(wFlags & TPM_RIGHTBUTTON)) break;
		    /* fall through */
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		    /* If the message belongs to the menu, removes it from the queue */
		    /* Else, end menu tracking */
		    fRemove = MENU_ButtonDown( &mt, hmenu, wFlags );
		    fEndMenu = !fRemove;
		    break;

		case WM_RBUTTONUP:
		    if (!(wFlags & TPM_RIGHTBUTTON)) break;
		    /* fall through */
		case WM_LBUTTONUP:
		    /* Check if a menu was selected by the mouse */
		    if (hmenu)
		    {
                        executedMenuId = MENU_ButtonUp( &mt, hmenu, wFlags);

			/* End the loop if executedMenuId is an item ID */
			/* or if the job was done (executedMenuId = 0). */
                        fEndMenu = fRemove = (executedMenuId != -1);
		    }
                    /* No menu was selected by the mouse */
                    /* if the function was called by TrackPopupMenu, continue
                       with the menu tracking. If not, stop it */
                    else
                        fEndMenu = ((wFlags & TPM_POPUPMENU) ? FALSE : TRUE);

		    break;

		case WM_MOUSEMOVE:
                    /* In win95 winelook, the selected menu item must be changed every time the
                       mouse moves. In Win31 winelook, the mouse button has to be held down */

                    if ( hmenu && ((TWEAK_WineLook > WIN31_LOOK) ||
                         ( (msg.wParam & MK_LBUTTON) ||
                           ((wFlags & TPM_RIGHTBUTTON) && (msg.wParam & MK_RBUTTON)))) )

			fEndMenu |= !MENU_MouseMove( &mt, hmenu, wFlags );

	    } /* switch(msg.message) - mouse */
	}
	else if ((msg.message >= WM_KEYFIRST) && (msg.message <= WM_KEYLAST))
	{
            fRemove = TRUE;  /* Keyboard messages are always removed */
	    switch(msg.message)
	    {
	    case WM_KEYDOWN:
		switch(msg.wParam)
		{
		case VK_HOME:
		case VK_END:
		    MENU_SelectItem( mt.hOwnerWnd, mt.hCurrentMenu,
				     NO_SELECTED_ITEM, FALSE, 0 );
		/* fall through */
		case VK_UP:
		    MENU_MoveSelection( mt.hOwnerWnd, mt.hCurrentMenu,
				       (msg.wParam == VK_HOME)? ITEM_NEXT : ITEM_PREV );
		    break;

		case VK_DOWN: /* If on menu bar, pull-down the menu */

		    menu = MENU_GetMenu( mt.hCurrentMenu );
		    if (!(menu->wFlags & MF_POPUP))
			mt.hCurrentMenu = MENU_ShowSubPopup(mt.hOwnerWnd, mt.hTopMenu, TRUE, wFlags);
		    else      /* otherwise try to move selection */
			MENU_MoveSelection( mt.hOwnerWnd, mt.hCurrentMenu, ITEM_NEXT );
		    break;

		case VK_LEFT:
		    MENU_KeyLeft( &mt, wFlags );
		    break;

		case VK_RIGHT:
		    MENU_KeyRight( &mt, wFlags );
		    break;

		case VK_ESCAPE:
                    fEndMenu = MENU_KeyEscape(&mt, wFlags);
		    break;

		case VK_F1:
		    {
			HELPINFO hi;
			hi.cbSize = sizeof(HELPINFO);
			hi.iContextType = HELPINFO_MENUITEM;
			if (menu->FocusedItem == NO_SELECTED_ITEM)
			    hi.iCtrlId = 0;
		        else
			    hi.iCtrlId = menu->items[menu->FocusedItem].wID;
			hi.hItemHandle = hmenu;
			hi.dwContextId = menu->dwContextHelpID;
			hi.MousePos = msg.pt;
			SendMessageA(hwnd, WM_HELP, 0, (LPARAM)&hi);
			break;
		    }

		default:
		    break;
		}
		break;  /* WM_KEYDOWN */

	    case WM_SYSKEYDOWN:
		switch(msg.wParam)
		{
		case VK_MENU:
		    fEndMenu = TRUE;
		    break;

		}
		break;  /* WM_SYSKEYDOWN */

	    case WM_CHAR:
		{
		    UINT	pos;

		    if (msg.wParam == '\r' || msg.wParam == ' ')
		    {
                        executedMenuId = MENU_ExecFocusedItem(&mt,mt.hCurrentMenu, wFlags);
                        fEndMenu = (executedMenuId != -1);

			break;
		    }

		      /* Hack to avoid control chars. */
		      /* We will find a better way real soon... */
		    if ((msg.wParam <= 32) || (msg.wParam >= 127)) break;

		    pos = MENU_FindItemByKey( mt.hOwnerWnd, mt.hCurrentMenu,
                                              LOWORD(msg.wParam), FALSE );
		    if (pos == (UINT)-2) fEndMenu = TRUE;
		    else if (pos == (UINT)-1) MessageBeep(0);
		    else
		    {
			MENU_SelectItem( mt.hOwnerWnd, mt.hCurrentMenu, pos,
                                TRUE, 0 );
                        executedMenuId = MENU_ExecFocusedItem(&mt,mt.hCurrentMenu, wFlags);
                        fEndMenu = (executedMenuId != -1);
		    }
		}
		break;
	    }  /* switch(msg.message) - kbd */
	}
	else
	{
	    DispatchMessageA( &msg );
	}

	if (!fEndMenu) fRemove = TRUE;

	/* finally remove message from the queue */

        if (fRemove && !(mt.trackFlags & TF_SKIPREMOVE) )
	    PeekMessageA( &msg, 0, msg.message, msg.message, PM_REMOVE );
	else mt.trackFlags &= ~TF_SKIPREMOVE;
    }

    ReleaseCapture();

    /* If dropdown is still painted and the close box is clicked on
       then the menu will be destroyed as part of the DispatchMessage above.
       This will then invalidate the menu handle in mt.hTopMenu. We should
       check for this first.  */
    if( IsMenu( mt.hTopMenu ) )
    {
	menu = MENU_GetMenu( mt.hTopMenu );

        if( IsWindow( mt.hOwnerWnd ) )
        {
	    MENU_HideSubPopups( mt.hOwnerWnd, mt.hTopMenu, FALSE );

	    if (menu && menu->wFlags & MF_POPUP)
	    {
                DestroyWindow( menu->hWnd );
                menu->hWnd = 0;
	    }
	    MENU_SelectItem( mt.hOwnerWnd, mt.hTopMenu, NO_SELECTED_ITEM, FALSE, 0 );
	    SendMessageA( mt.hOwnerWnd, WM_MENUSELECT, MAKELONG(0,0xffff), 0 );
        }

        /* Reset the variable for hiding menu */
        if( menu ) menu->bTimeToHide = FALSE;
    }

    /* The return value is only used by TrackPopupMenu */
    return ((executedMenuId != -1) ? executedMenuId : 0);
}

/***********************************************************************
 *           MENU_InitTracking
 */
static BOOL MENU_InitTracking(HWND hWnd, HMENU hMenu, BOOL bPopup, UINT wFlags)
{
    TRACE("hwnd=0x%04x hmenu=0x%04x\n", hWnd, hMenu);

    HideCaret(0);

    /* Send WM_ENTERMENULOOP and WM_INITMENU message only if TPM_NONOTIFY flag is not specified */
    if (!(wFlags & TPM_NONOTIFY))
       SendMessageA( hWnd, WM_ENTERMENULOOP, bPopup, 0 );

    SendMessageA( hWnd, WM_SETCURSOR, (WPARAM)hWnd, HTCAPTION );

    if (!(wFlags & TPM_NONOTIFY))
    {
       POPUPMENU *menu;
       SendMessageA( hWnd, WM_INITMENU, hMenu, 0 );
       if ((menu = MENU_GetMenu( hMenu )) && (!menu->Height))
       { /* app changed/recreated menu bar entries in WM_INITMENU
            Recalculate menu sizes else clicks will not work */
          SetWindowPos( hWnd, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                        SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED );

       }
    }
    return TRUE;
}
/***********************************************************************
 *           MENU_ExitTracking
 */
static BOOL MENU_ExitTracking(HWND hWnd)
{
    TRACE("hwnd=0x%04x\n", hWnd);

    SendMessageA( hWnd, WM_EXITMENULOOP, 0, 0 );
    ShowCaret(0);
    return TRUE;
}

/***********************************************************************
 *           MENU_TrackMouseMenuBar
 *
 * Menu-bar tracking upon a mouse event. Called from NC_HandleSysCommand().
 */
void MENU_TrackMouseMenuBar( HWND hWnd, INT ht, POINT pt )
{
    HMENU hMenu = (ht == HTSYSMENU) ? get_win_sys_menu( hWnd ) : GetMenu( hWnd );
    UINT wFlags = TPM_ENTERIDLEEX | TPM_BUTTONDOWN | TPM_LEFTALIGN | TPM_LEFTBUTTON;

    TRACE("wnd=%x ht=0x%04x (%ld,%ld)\n", hWnd, ht, pt.x, pt.y);

    if (IsMenu(hMenu))
    {
        /* map point to parent client coordinates */
        HWND parent = GetAncestor( hWnd, GA_PARENT );
        if (parent != GetDesktopWindow()) ScreenToClient( parent, &pt );

	MENU_InitTracking( hWnd, hMenu, FALSE, wFlags );
	MENU_TrackMenu( hMenu, wFlags, pt.x, pt.y, hWnd, NULL );
	MENU_ExitTracking(hWnd);
    }
}


/***********************************************************************
 *           MENU_TrackKbdMenuBar
 *
 * Menu-bar tracking upon a keyboard event. Called from NC_HandleSysCommand().
 */
void MENU_TrackKbdMenuBar( HWND hwnd, UINT wParam, INT vkey)
{
    UINT uItem = NO_SELECTED_ITEM;
    HMENU hTrackMenu;
    UINT wFlags = TPM_ENTERIDLEEX | TPM_LEFTALIGN | TPM_LEFTBUTTON;

    /* find window that has a menu */

    while (GetWindowLongA( hwnd, GWL_STYLE ) & WS_CHILD)
        if (!(hwnd = GetParent( hwnd ))) return;

    /* check if we have to track a system menu */

    hTrackMenu = GetMenu( hwnd );
    if (!hTrackMenu || IsIconic(hwnd) || vkey == VK_SPACE )
    {
        if (!(GetWindowLongA( hwnd, GWL_STYLE ) & WS_SYSMENU)) return;
        hTrackMenu = get_win_sys_menu( hwnd );
        uItem = 0;
        wParam |= HTSYSMENU; /* prevent item lookup */
    }

    if (!IsMenu( hTrackMenu )) return;

    MENU_InitTracking( hwnd, hTrackMenu, FALSE, wFlags );

    if( vkey && vkey != VK_SPACE )
    {
        uItem = MENU_FindItemByKey( hwnd, hTrackMenu, vkey, (wParam & HTSYSMENU) );
        if( uItem >= (UINT)(-2) )
        {
            if( uItem == (UINT)(-1) ) MessageBeep(0);
            hTrackMenu = 0;
        }
    }

    if( hTrackMenu )
    {
        MENU_SelectItem( hwnd, hTrackMenu, uItem, TRUE, 0 );

        if( uItem == NO_SELECTED_ITEM )
            MENU_MoveSelection( hwnd, hTrackMenu, ITEM_NEXT );
        else if( vkey )
            PostMessageA( hwnd, WM_KEYDOWN, VK_DOWN, 0L );

        MENU_TrackMenu( hTrackMenu, wFlags, 0, 0, hwnd, NULL );
    }
    MENU_ExitTracking( hwnd );
}


/**********************************************************************
 *           TrackPopupMenu   (USER32.@)
 *
 * Like the win32 API, the function return the command ID only if the
 * flag TPM_RETURNCMD is on.
 *
 */
BOOL WINAPI TrackPopupMenu( HMENU hMenu, UINT wFlags, INT x, INT y,
                           INT nReserved, HWND hWnd, const RECT *lpRect )
{
    BOOL ret = FALSE;

    MENU_InitTracking(hWnd, hMenu, TRUE, wFlags);

    /* Send WM_INITMENUPOPUP message only if TPM_NONOTIFY flag is not specified */
    if (!(wFlags & TPM_NONOTIFY))
        SendMessageA( hWnd, WM_INITMENUPOPUP, hMenu, 0);

    if (MENU_ShowPopup( hWnd, hMenu, 0, x, y, 0, 0 ))
	ret = MENU_TrackMenu( hMenu, wFlags | TPM_POPUPMENU, 0, 0, hWnd, lpRect );
    MENU_ExitTracking(hWnd);

    if( (!(wFlags & TPM_RETURNCMD)) && (ret != FALSE) )
	ret = 1;

    return ret;
}

/**********************************************************************
 *           TrackPopupMenuEx   (USER32.@)
 */
BOOL WINAPI TrackPopupMenuEx( HMENU hMenu, UINT wFlags, INT x, INT y,
                                HWND hWnd, LPTPMPARAMS lpTpm )
{
    FIXME("not fully implemented\n" );
    return TrackPopupMenu( hMenu, wFlags, x, y, 0, hWnd,
                             lpTpm ? &lpTpm->rcExclude : NULL );
}

/***********************************************************************
 *           PopupMenuWndProc
 *
 * NOTE: Windows has totally different (and undocumented) popup wndproc.
 */
static LRESULT WINAPI PopupMenuWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    TRACE("hwnd=0x%04x msg=0x%04x wp=0x%04x lp=0x%08lx\n",
    hwnd, message, wParam, lParam);

    switch(message)
    {
    case WM_CREATE:
	{
	    CREATESTRUCTW *cs = (CREATESTRUCTW*)lParam;
	    SetWindowLongW( hwnd, 0, (LONG)cs->lpCreateParams );
            return 0;
	}

    case WM_MOUSEACTIVATE:  /* We don't want to be activated */
        return MA_NOACTIVATE;

    case WM_PAINT:
	{
	    PAINTSTRUCT ps;
	    BeginPaint( hwnd, &ps );
	    MENU_DrawPopupMenu( hwnd, ps.hdc,
                                (HMENU)GetWindowLongA( hwnd, 0 ) );
	    EndPaint( hwnd, &ps );
            return 0;
	}
    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        /* zero out global pointer in case resident popup window was destroyed. */
        if (hwnd == top_popup) top_popup = 0;
        break;

    case WM_SHOWWINDOW:

	if( wParam )
	{
            if (!GetWindowLongW( hwnd, 0 )) ERR("no menu to display\n");
	}
	else
            SetWindowLongW( hwnd, 0, 0 );
	break;

    case MM_SETMENUHANDLE:
        SetWindowLongW( hwnd, 0, wParam );
        break;

    case MM_GETMENUHANDLE:
        return GetWindowLongW( hwnd, 0 );

    default:
        return DefWindowProcW( hwnd, message, wParam, lParam );
    }
    return 0;
}


/***********************************************************************
 *           MENU_GetMenuBarHeight
 *
 * Compute the size of the menu bar height. Used by NC_HandleNCCalcSize().
 */
UINT MENU_GetMenuBarHeight( HWND hwnd, UINT menubarWidth,
                              INT orgX, INT orgY )
{
    HDC hdc;
    RECT rectBar;
    LPPOPUPMENU lppop;

    TRACE("HWND 0x%x, width %d, at (%d, %d).\n",
          hwnd, menubarWidth, orgX, orgY );

    if (!(lppop = MENU_GetMenu( GetMenu(hwnd) ))) return 0;

    hdc = GetDCEx( hwnd, 0, DCX_CACHE | DCX_WINDOW );
    SelectObject( hdc, hMenuFont);
    SetRect(&rectBar, orgX, orgY, orgX+menubarWidth, orgY+GetSystemMetrics(SM_CYMENU));
    MENU_MenuBarCalcSize( hdc, &rectBar, lppop, hwnd );
    ReleaseDC( hwnd, hdc );
    return lppop->Height;
}


/**********************************************************************
 *		GetMenuBarInfo    (USER32.@)
 *
 */
BOOL WINAPI GetMenuBarInfo(HWND hwnd, LONG idObject, LONG idItem, PMENUBARINFO pmbi)
{
    TRACE("hwnd: %x, idObject: %lx, idItem: %lx, pmbi: %p\n",
	    hwnd, idObject, idItem, pmbi);

    FIXME("stub\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return FALSE;
}


/*******************************************************************
 *         ChangeMenu    (USER.153)
 */
BOOL16 WINAPI ChangeMenu16( HMENU16 hMenu, UINT16 pos, SEGPTR data,
                            UINT16 id, UINT16 flags )
{
    TRACE("menu=%04x pos=%d data=%08lx id=%04x flags=%04x\n",
                  hMenu, pos, (DWORD)data, id, flags );
    if (flags & MF_APPEND) return AppendMenu16( hMenu, flags & ~MF_APPEND,
                                                id, data );

    /* FIXME: Word passes the item id in 'pos' and 0 or 0xffff as id */
    /* for MF_DELETE. We should check the parameters for all others */
    /* MF_* actions also (anybody got a doc on ChangeMenu?). */

    if (flags & MF_DELETE) return DeleteMenu16(hMenu, pos, flags & ~MF_DELETE);
    if (flags & MF_CHANGE) return ModifyMenu16(hMenu, pos, flags & ~MF_CHANGE,
                                               id, data );
    if (flags & MF_REMOVE) return RemoveMenu16(hMenu,
                                              flags & MF_BYPOSITION ? pos : id,
                                              flags & ~MF_REMOVE );
    /* Default: MF_INSERT */
    return InsertMenu16( hMenu, pos, flags, id, data );
}


/*******************************************************************
 *         ChangeMenuA    (USER32.@)
 */
BOOL WINAPI ChangeMenuA( HMENU hMenu, UINT pos, LPCSTR data,
                             UINT id, UINT flags )
{
    TRACE("menu=%08x pos=%d data=%08lx id=%08x flags=%08x\n",
                  hMenu, pos, (DWORD)data, id, flags );
    if (flags & MF_APPEND) return AppendMenuA( hMenu, flags & ~MF_APPEND,
                                                 id, data );
    if (flags & MF_DELETE) return DeleteMenu(hMenu, pos, flags & ~MF_DELETE);
    if (flags & MF_CHANGE) return ModifyMenuA(hMenu, pos, flags & ~MF_CHANGE,
                                                id, data );
    if (flags & MF_REMOVE) return RemoveMenu( hMenu,
                                              flags & MF_BYPOSITION ? pos : id,
                                              flags & ~MF_REMOVE );
    /* Default: MF_INSERT */
    return InsertMenuA( hMenu, pos, flags, id, data );
}


/*******************************************************************
 *         ChangeMenuW    (USER32.@)
 */
BOOL WINAPI ChangeMenuW( HMENU hMenu, UINT pos, LPCWSTR data,
                             UINT id, UINT flags )
{
    TRACE("menu=%08x pos=%d data=%08lx id=%08x flags=%08x\n",
                  hMenu, pos, (DWORD)data, id, flags );
    if (flags & MF_APPEND) return AppendMenuW( hMenu, flags & ~MF_APPEND,
                                                 id, data );
    if (flags & MF_DELETE) return DeleteMenu(hMenu, pos, flags & ~MF_DELETE);
    if (flags & MF_CHANGE) return ModifyMenuW(hMenu, pos, flags & ~MF_CHANGE,
                                                id, data );
    if (flags & MF_REMOVE) return RemoveMenu( hMenu,
                                              flags & MF_BYPOSITION ? pos : id,
                                              flags & ~MF_REMOVE );
    /* Default: MF_INSERT */
    return InsertMenuW( hMenu, pos, flags, id, data );
}


/*******************************************************************
 *         CheckMenuItem    (USER.154)
 */
BOOL16 WINAPI CheckMenuItem16( HMENU16 hMenu, UINT16 id, UINT16 flags )
{
    return (BOOL16)CheckMenuItem( hMenu, id, flags );
}


/*******************************************************************
 *         CheckMenuItem    (USER32.@)
 */
DWORD WINAPI CheckMenuItem( HMENU hMenu, UINT id, UINT flags )
{
    MENUITEM *item;
    DWORD ret;

    TRACE("menu=%04x id=%04x flags=%04x\n", hMenu, id, flags );
    if (!(item = MENU_FindItem( &hMenu, &id, flags ))) return -1;
    ret = item->fState & MF_CHECKED;
    if (flags & MF_CHECKED) item->fState |= MF_CHECKED;
    else item->fState &= ~MF_CHECKED;
    return ret;
}


/**********************************************************************
 *         EnableMenuItem    (USER.155)
 */
UINT16 WINAPI EnableMenuItem16( HMENU16 hMenu, UINT16 wItemID, UINT16 wFlags )
{
    return EnableMenuItem( hMenu, wItemID, wFlags );
}


/**********************************************************************
 *         EnableMenuItem    (USER32.@)
 */
UINT WINAPI EnableMenuItem( HMENU hMenu, UINT wItemID, UINT wFlags )
{
    UINT    oldflags;
    MENUITEM *item;
    POPUPMENU *menu;

    TRACE("(%04x, %04X, %04X) !\n",
		 hMenu, wItemID, wFlags);

    /* Get the Popupmenu to access the owner menu */
    if (!(menu = MENU_GetMenu(hMenu)))
	return (UINT)-1;

    if (!(item = MENU_FindItem( &hMenu, &wItemID, wFlags )))
	return (UINT)-1;

    oldflags = item->fState & (MF_GRAYED | MF_DISABLED);
    item->fState ^= (oldflags ^ wFlags) & (MF_GRAYED | MF_DISABLED);

    /* In win95 if the close item in the system menu change update the close button */
    if (TWEAK_WineLook == WIN95_LOOK)
	if((item->wID == SC_CLOSE) && (oldflags != wFlags))
	{
	    if (menu->hSysMenuOwner != 0)
	    {
		POPUPMENU* parentMenu;

		/* Get the parent menu to access*/
		if (!(parentMenu = MENU_GetMenu(menu->hSysMenuOwner)))
		    return (UINT)-1;

		/* Refresh the frame to reflect the change*/
		SetWindowPos(parentMenu->hWnd, 0, 0, 0, 0, 0,
			     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
	    }
	}

    return oldflags;
}


/*******************************************************************
 *         GetMenuString    (USER.161)
 */
INT16 WINAPI GetMenuString16( HMENU16 hMenu, UINT16 wItemID,
                              LPSTR str, INT16 nMaxSiz, UINT16 wFlags )
{
    return GetMenuStringA( hMenu, wItemID, str, nMaxSiz, wFlags );
}


/*******************************************************************
 *         GetMenuStringA    (USER32.@)
 */
INT WINAPI GetMenuStringA(
	HMENU hMenu,	/* [in] menuhandle */
	UINT wItemID,	/* [in] menu item (dep. on wFlags) */
	LPSTR str,	/* [out] outbuffer. If NULL, func returns entry length*/
	INT nMaxSiz,	/* [in] length of buffer. if 0, func returns entry len*/
	UINT wFlags	/* [in] MF_ flags */
) {
    MENUITEM *item;

    TRACE("menu=%04x item=%04x ptr=%p len=%d flags=%04x\n",
                 hMenu, wItemID, str, nMaxSiz, wFlags );
    if (!(item = MENU_FindItem( &hMenu, &wItemID, wFlags ))) return 0;
    if (!IS_STRING_ITEM(item->fType)) return 0;
    if (!str || !nMaxSiz) return strlenW(item->text);
    str[0] = '\0';
    if (!WideCharToMultiByte( CP_ACP, 0, item->text, -1, str, nMaxSiz, NULL, NULL ))
        str[nMaxSiz-1] = 0;
    TRACE("returning '%s'\n", str );
    return strlen(str);
}


/*******************************************************************
 *         GetMenuStringW    (USER32.@)
 */
INT WINAPI GetMenuStringW( HMENU hMenu, UINT wItemID,
                               LPWSTR str, INT nMaxSiz, UINT wFlags )
{
    MENUITEM *item;

    TRACE("menu=%04x item=%04x ptr=%p len=%d flags=%04x\n",
                 hMenu, wItemID, str, nMaxSiz, wFlags );
    if (!(item = MENU_FindItem( &hMenu, &wItemID, wFlags ))) return 0;
    if (!IS_STRING_ITEM(item->fType)) return 0;
    if (!str || !nMaxSiz) return strlenW(item->text);
    str[0] = '\0';
    lstrcpynW( str, item->text, nMaxSiz );
    return strlenW(str);
}


/**********************************************************************
 *         HiliteMenuItem    (USER32.@)
 */
BOOL WINAPI HiliteMenuItem( HWND hWnd, HMENU hMenu, UINT wItemID,
                                UINT wHilite )
{
    LPPOPUPMENU menu;
    TRACE("(%04x, %04x, %04x, %04x);\n",
                 hWnd, hMenu, wItemID, wHilite);
    if (!MENU_FindItem( &hMenu, &wItemID, wHilite )) return FALSE;
    if (!(menu = MENU_GetMenu(hMenu))) return FALSE;
    if (menu->FocusedItem == wItemID) return TRUE;
    MENU_HideSubPopups( hWnd, hMenu, FALSE );
    MENU_SelectItem( hWnd, hMenu, wItemID, TRUE, 0 );
    return TRUE;
}


/**********************************************************************
 *         GetMenuState    (USER.250)
 */
UINT16 WINAPI GetMenuState16( HMENU16 hMenu, UINT16 wItemID, UINT16 wFlags )
{
    return GetMenuState( hMenu, wItemID, wFlags );
}


/**********************************************************************
 *         GetMenuState    (USER32.@)
 */
UINT WINAPI GetMenuState( HMENU hMenu, UINT wItemID, UINT wFlags )
{
    MENUITEM *item;
    TRACE("(menu=%04x, id=%04x, flags=%04x);\n",
		 hMenu, wItemID, wFlags);
    if (!(item = MENU_FindItem( &hMenu, &wItemID, wFlags ))) return -1;
    debug_print_menuitem ("  item: ", item, "");
    if (item->fType & MF_POPUP)
    {
	POPUPMENU *menu = MENU_GetMenu( item->hSubMenu );
	if (!menu) return -1;
	else return (menu->nItems << 8) | ((item->fState|item->fType) & 0xff);
    }
    else
    {
	/* We used to (from way back then) mask the result to 0xff.  */
	/* I don't know why and it seems wrong as the documented */
	/* return flag MF_SEPARATOR is outside that mask.  */
	return (item->fType | item->fState);
    }
}


/**********************************************************************
 *         GetMenuItemCount    (USER.263)
 */
INT16 WINAPI GetMenuItemCount16( HMENU16 hMenu )
{
    LPPOPUPMENU	menu = MENU_GetMenu(hMenu);
    if (!menu) return -1;
    TRACE("(%04x) returning %d\n",
                  hMenu, menu->nItems );
    return menu->nItems;
}


/**********************************************************************
 *         GetMenuItemCount    (USER32.@)
 */
INT WINAPI GetMenuItemCount( HMENU hMenu )
{
    LPPOPUPMENU	menu = MENU_GetMenu(hMenu);
    if (!menu) return -1;
    TRACE("(%04x) returning %d\n",
                  hMenu, menu->nItems );
    return menu->nItems;
}

/**********************************************************************
 *         GetMenuItemID    (USER.264)
 */
UINT16 WINAPI GetMenuItemID16( HMENU16 hMenu, INT16 nPos )
{
    return (UINT16) GetMenuItemID (hMenu, nPos);
}

/**********************************************************************
 *         GetMenuItemID    (USER32.@)
 */
UINT WINAPI GetMenuItemID( HMENU hMenu, INT nPos )
{
    MENUITEM * lpmi;

    if (!(lpmi = MENU_FindItem(&hMenu,&nPos,MF_BYPOSITION))) return -1;
    if (lpmi->fType & MF_POPUP) return -1;
    return lpmi->wID;

}

/*******************************************************************
 *         InsertMenu    (USER.410)
 */
BOOL16 WINAPI InsertMenu16( HMENU16 hMenu, UINT16 pos, UINT16 flags,
                            UINT16 id, SEGPTR data )
{
    UINT pos32 = (UINT)pos;
    if ((pos == (UINT16)-1) && (flags & MF_BYPOSITION)) pos32 = (UINT)-1;
    if (IS_STRING_ITEM(flags) && data)
        return InsertMenuA( hMenu, pos32, flags, id, MapSL(data) );
    return InsertMenuA( hMenu, pos32, flags, id, (LPSTR)data );
}


/*******************************************************************
 *         InsertMenuW    (USER32.@)
 */
BOOL WINAPI InsertMenuW( HMENU hMenu, UINT pos, UINT flags,
                             UINT id, LPCWSTR str )
{
    MENUITEM *item;

    if (IS_STRING_ITEM(flags) && str)
        TRACE("hMenu %04x, pos %d, flags %08x, "
		      "id %04x, str %s\n",
                      hMenu, pos, flags, id, debugstr_w(str) );
    else TRACE("hMenu %04x, pos %d, flags %08x, "
		       "id %04x, str %08lx (not a string)\n",
                       hMenu, pos, flags, id, (DWORD)str );

    if (!(item = MENU_InsertItem( hMenu, pos, flags ))) return FALSE;

    if (!(MENU_SetItemData( item, flags, id, str )))
    {
        RemoveMenu( hMenu, pos, flags );
        return FALSE;
    }

    if (flags & MF_POPUP)  /* Set the MF_POPUP flag on the popup-menu */
	(MENU_GetMenu((HMENU16)id))->wFlags |= MF_POPUP;

    item->hCheckBit = item->hUnCheckBit = 0;
    return TRUE;
}


/*******************************************************************
 *         InsertMenuA    (USER32.@)
 */
BOOL WINAPI InsertMenuA( HMENU hMenu, UINT pos, UINT flags,
                             UINT id, LPCSTR str )
{
    BOOL ret = FALSE;

    if (IS_STRING_ITEM(flags) && str)
    {
        INT len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
        LPWSTR newstr = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        if (newstr)
        {
            MultiByteToWideChar( CP_ACP, 0, str, -1, newstr, len );
            ret = InsertMenuW( hMenu, pos, flags, id, newstr );
            HeapFree( GetProcessHeap(), 0, newstr );
        }
        return ret;
    }
    else return InsertMenuW( hMenu, pos, flags, id, (LPCWSTR)str );
}


/*******************************************************************
 *         AppendMenu    (USER.411)
 */
BOOL16 WINAPI AppendMenu16(HMENU16 hMenu, UINT16 flags, UINT16 id, SEGPTR data)
{
    return InsertMenu16( hMenu, -1, flags | MF_BYPOSITION, id, data );
}


/*******************************************************************
 *         AppendMenuA    (USER32.@)
 */
BOOL WINAPI AppendMenuA( HMENU hMenu, UINT flags,
                             UINT id, LPCSTR data )
{
    return InsertMenuA( hMenu, -1, flags | MF_BYPOSITION, id, data );
}


/*******************************************************************
 *         AppendMenuW    (USER32.@)
 */
BOOL WINAPI AppendMenuW( HMENU hMenu, UINT flags,
                             UINT id, LPCWSTR data )
{
    return InsertMenuW( hMenu, -1, flags | MF_BYPOSITION, id, data );
}


/**********************************************************************
 *         RemoveMenu   (USER.412)
 */
BOOL16 WINAPI RemoveMenu16( HMENU16 hMenu, UINT16 nPos, UINT16 wFlags )
{
    return RemoveMenu( hMenu, nPos, wFlags );
}


/**********************************************************************
 *         RemoveMenu    (USER32.@)
 */
BOOL WINAPI RemoveMenu( HMENU hMenu, UINT nPos, UINT wFlags )
{
    LPPOPUPMENU	menu;
    MENUITEM *item;

    TRACE("(menu=%04x pos=%04x flags=%04x)\n",hMenu, nPos, wFlags);
    if (!(item = MENU_FindItem( &hMenu, &nPos, wFlags ))) return FALSE;
    if (!(menu = MENU_GetMenu(hMenu))) return FALSE;

      /* Remove item */

    MENU_FreeItemData( item );

    if (--menu->nItems == 0)
    {
        HeapFree( GetProcessHeap(), 0, menu->items );
        menu->items = NULL;
    }
    else
    {
	while(nPos < menu->nItems)
	{
	    *item = *(item+1);
	    item++;
	    nPos++;
	}
        menu->items = HeapReAlloc( GetProcessHeap(), 0, menu->items,
                                   menu->nItems * sizeof(MENUITEM) );
    }
    return TRUE;
}


/**********************************************************************
 *         DeleteMenu    (USER.413)
 */
BOOL16 WINAPI DeleteMenu16( HMENU16 hMenu, UINT16 nPos, UINT16 wFlags )
{
    return DeleteMenu( hMenu, nPos, wFlags );
}


/**********************************************************************
 *         DeleteMenu    (USER32.@)
 */
BOOL WINAPI DeleteMenu( HMENU hMenu, UINT nPos, UINT wFlags )
{
    MENUITEM *item = MENU_FindItem( &hMenu, &nPos, wFlags );
    if (!item) return FALSE;
    if (item->fType & MF_POPUP) DestroyMenu( item->hSubMenu );
      /* nPos is now the position of the item */
    RemoveMenu( hMenu, nPos, wFlags | MF_BYPOSITION );
    return TRUE;
}


/*******************************************************************
 *         ModifyMenu    (USER.414)
 */
BOOL16 WINAPI ModifyMenu16( HMENU16 hMenu, UINT16 pos, UINT16 flags,
                            UINT16 id, SEGPTR data )
{
    if (IS_STRING_ITEM(flags))
        return ModifyMenuA( hMenu, pos, flags, id, MapSL(data) );
    return ModifyMenuA( hMenu, pos, flags, id, (LPSTR)data );
}


/*******************************************************************
 *         ModifyMenuW    (USER32.@)
 */
BOOL WINAPI ModifyMenuW( HMENU hMenu, UINT pos, UINT flags,
                             UINT id, LPCWSTR str )
{
    MENUITEM *item;

    if (IS_STRING_ITEM(flags))
    {
	TRACE("%04x %d %04x %04x %s\n",
                      hMenu, pos, flags, id, debugstr_w(str) );
        if (!str) return FALSE;
    }
    else
    {
	TRACE("%04x %d %04x %04x %08lx\n",
                      hMenu, pos, flags, id, (DWORD)str );
    }

    if (!(item = MENU_FindItem( &hMenu, &pos, flags ))) return FALSE;
    return MENU_SetItemData( item, flags, id, str );
}


/*******************************************************************
 *         ModifyMenuA    (USER32.@)
 */
BOOL WINAPI ModifyMenuA( HMENU hMenu, UINT pos, UINT flags,
                             UINT id, LPCSTR str )
{
    BOOL ret = FALSE;

    if (IS_STRING_ITEM(flags) && str)
    {
        INT len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
        LPWSTR newstr = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        if (newstr)
        {
            MultiByteToWideChar( CP_ACP, 0, str, -1, newstr, len );
            ret = ModifyMenuW( hMenu, pos, flags, id, newstr );
            HeapFree( GetProcessHeap(), 0, newstr );
        }
        return ret;
    }
    else return ModifyMenuW( hMenu, pos, flags, id, (LPCWSTR)str );
}


/**********************************************************************
 *         CreatePopupMenu    (USER.415)
 */
HMENU16 WINAPI CreatePopupMenu16(void)
{
    return CreatePopupMenu();
}


/**********************************************************************
 *         CreatePopupMenu    (USER32.@)
 */
HMENU WINAPI CreatePopupMenu(void)
{
    HMENU hmenu;
    POPUPMENU *menu;

    if (!(hmenu = CreateMenu())) return 0;
    menu = MENU_GetMenu( hmenu );
    menu->wFlags |= MF_POPUP;
    menu->bTimeToHide = FALSE;
    return hmenu;
}


/**********************************************************************
 *         GetMenuCheckMarkDimensions    (USER.417)
 *         GetMenuCheckMarkDimensions    (USER32.@)
 */
DWORD WINAPI GetMenuCheckMarkDimensions(void)
{
    return MAKELONG( GetSystemMetrics(SM_CXMENUCHECK), GetSystemMetrics(SM_CYMENUCHECK) );
}


/**********************************************************************
 *         SetMenuItemBitmaps    (USER.418)
 */
BOOL16 WINAPI SetMenuItemBitmaps16( HMENU16 hMenu, UINT16 nPos, UINT16 wFlags,
                                    HBITMAP16 hNewUnCheck, HBITMAP16 hNewCheck)
{
    return SetMenuItemBitmaps( hMenu, nPos, wFlags, hNewUnCheck, hNewCheck );
}


/**********************************************************************
 *         SetMenuItemBitmaps    (USER32.@)
 */
BOOL WINAPI SetMenuItemBitmaps( HMENU hMenu, UINT nPos, UINT wFlags,
                                    HBITMAP hNewUnCheck, HBITMAP hNewCheck)
{
    MENUITEM *item;
    TRACE("(%04x, %04x, %04x, %04x, %04x)\n",
                 hMenu, nPos, wFlags, hNewCheck, hNewUnCheck);
    if (!(item = MENU_FindItem( &hMenu, &nPos, wFlags ))) return FALSE;

    if (!hNewCheck && !hNewUnCheck)
    {
	item->fState &= ~MF_USECHECKBITMAPS;
    }
    else  /* Install new bitmaps */
    {
	item->hCheckBit = hNewCheck;
	item->hUnCheckBit = hNewUnCheck;
	item->fState |= MF_USECHECKBITMAPS;
    }
    return TRUE;
}


/**********************************************************************
 *         CreateMenu    (USER.151)
 */
HMENU16 WINAPI CreateMenu16(void)
{
    return CreateMenu();
}


/**********************************************************************
 *         CreateMenu    (USER32.@)
 */
HMENU WINAPI CreateMenu(void)
{
    HMENU hMenu;
    LPPOPUPMENU menu;
    if (!(hMenu = USER_HEAP_ALLOC( sizeof(POPUPMENU) ))) return 0;
    menu = (LPPOPUPMENU) USER_HEAP_LIN_ADDR(hMenu);

    ZeroMemory(menu, sizeof(POPUPMENU));
    menu->wMagic = MENU_MAGIC;
    menu->FocusedItem = NO_SELECTED_ITEM;
    menu->bTimeToHide = FALSE;

    TRACE("return %04x\n", hMenu );

    return hMenu;
}


/**********************************************************************
 *         DestroyMenu    (USER.152)
 */
BOOL16 WINAPI DestroyMenu16( HMENU16 hMenu )
{
    return DestroyMenu( hMenu );
}


/**********************************************************************
 *         DestroyMenu    (USER32.@)
 */
BOOL WINAPI DestroyMenu( HMENU hMenu )
{
    TRACE("(%04x)\n", hMenu);

    /* Silently ignore attempts to destroy default system popup */

    if (hMenu && hMenu != MENU_DefSysPopup)
    {
        LPPOPUPMENU lppop = MENU_GetMenu(hMenu);

        if (!lppop) return FALSE;

        lppop->wMagic = 0;  /* Mark it as destroyed */

        if ((lppop->wFlags & MF_POPUP) && lppop->hWnd)
        {
            DestroyWindow( lppop->hWnd );
            lppop->hWnd = 0;
        }

        if (lppop->items) /* recursively destroy submenus */
        {
            int i;
            MENUITEM *item = lppop->items;
            for (i = lppop->nItems; i > 0; i--, item++)
            {
                if (item->fType & MF_POPUP) DestroyMenu(item->hSubMenu);
                MENU_FreeItemData( item );
            }
            HeapFree( GetProcessHeap(), 0, lppop->items );
        }
        USER_HEAP_FREE( hMenu );
    }
    return (hMenu != MENU_DefSysPopup);
}


/**********************************************************************
 *         GetSystemMenu    (USER32.@)
 */
HMENU WINAPI GetSystemMenu( HWND hWnd, BOOL bRevert )
{
    WND *wndPtr = WIN_FindWndPtr( hWnd );
    HMENU retvalue = 0;

    if (wndPtr)
    {
	if( wndPtr->hSysMenu )
	{
	    if( bRevert )
	    {
		DestroyMenu(wndPtr->hSysMenu);
		wndPtr->hSysMenu = 0;
	    }
	    else
	    {
		POPUPMENU *menu = MENU_GetMenu( wndPtr->hSysMenu );
		if( menu )
                {
		   if( menu->nItems > 0 && menu->items[0].hSubMenu == MENU_DefSysPopup )
		      menu->items[0].hSubMenu = MENU_CopySysPopup();
		}
		else
		{
		   WARN("Current sys-menu (%04x) of wnd %04x is broken\n",
			wndPtr->hSysMenu, hWnd);
		   wndPtr->hSysMenu = 0;
		}
	    }
	}

	if(!wndPtr->hSysMenu && (wndPtr->dwStyle & WS_SYSMENU) )
	    wndPtr->hSysMenu = MENU_GetSysMenu( hWnd, (HMENU)(-1) );

	if( wndPtr->hSysMenu )
        {
	    POPUPMENU *menu;
	    retvalue = GetSubMenu(wndPtr->hSysMenu, 0);

	    /* Store the dummy sysmenu handle to facilitate the refresh */
	    /* of the close button if the SC_CLOSE item change */
	    menu = MENU_GetMenu(retvalue);
	    if ( menu )
	       menu->hSysMenuOwner = wndPtr->hSysMenu;
        }
        WIN_ReleaseWndPtr(wndPtr);
    }
    return bRevert ? 0 : retvalue;
}


/*******************************************************************
 *         SetSystemMenu    (USER32.@)
 */
BOOL WINAPI SetSystemMenu( HWND hwnd, HMENU hMenu )
{
    WND *wndPtr = WIN_FindWndPtr(hwnd);

    if (wndPtr)
    {
	if (wndPtr->hSysMenu) DestroyMenu( wndPtr->hSysMenu );
	wndPtr->hSysMenu = MENU_GetSysMenu( hwnd, hMenu );
        WIN_ReleaseWndPtr(wndPtr);
	return TRUE;
    }
    return FALSE;
}


/**********************************************************************
 *         GetMenu    (USER32.@)
 */
HMENU WINAPI GetMenu( HWND hWnd )
{
    HMENU retvalue = (HMENU)GetWindowLongA( hWnd, GWL_ID );
    TRACE("for %04x returning %04x\n", hWnd, retvalue);
    return retvalue;
}


/**********************************************************************
 *         SetMenu    (USER32.@)
 */
BOOL WINAPI SetMenu( HWND hWnd, HMENU hMenu )
{
    TRACE("(%04x, %04x);\n", hWnd, hMenu);

    if (hMenu && !IsMenu(hMenu))
    {
        WARN("hMenu %x is not a menu handle\n", hMenu);
        return FALSE;
    }
    if (GetWindowLongA( hWnd, GWL_STYLE ) & WS_CHILD) return FALSE;

    hWnd = WIN_GetFullHandle( hWnd );
    if (GetCapture() == hWnd) ReleaseCapture();

    if (hMenu != 0)
    {
        LPPOPUPMENU lpmenu;

        if (!(lpmenu = MENU_GetMenu(hMenu))) return FALSE;

        lpmenu->hWnd = hWnd;
        lpmenu->Height = 0;  /* Make sure we recalculate the size */
    }
    SetWindowLongA( hWnd, GWL_ID, hMenu );

    if (IsWindowVisible(hWnd))
        SetWindowPos( hWnd, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                      SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED );
    return TRUE;
}



/**********************************************************************
 *         GetSubMenu    (USER.159)
 */
HMENU16 WINAPI GetSubMenu16( HMENU16 hMenu, INT16 nPos )
{
    return GetSubMenu( hMenu, nPos );
}


/**********************************************************************
 *         GetSubMenu    (USER32.@)
 */
HMENU WINAPI GetSubMenu( HMENU hMenu, INT nPos )
{
    MENUITEM * lpmi;

    if (!(lpmi = MENU_FindItem(&hMenu,&nPos,MF_BYPOSITION))) return 0;
    if (!(lpmi->fType & MF_POPUP)) return 0;
    return lpmi->hSubMenu;
}


/**********************************************************************
 *         DrawMenuBar    (USER32.@)
 */
BOOL WINAPI DrawMenuBar( HWND hWnd )
{
    LPPOPUPMENU lppop;
    HMENU hMenu = GetMenu(hWnd);

    if (GetWindowLongA( hWnd, GWL_STYLE ) & WS_CHILD) return FALSE;
    if (!hMenu || !(lppop = MENU_GetMenu( hMenu ))) return FALSE;

    lppop->Height = 0; /* Make sure we call MENU_MenuBarCalcSize */
    lppop->hwndOwner = hWnd;
    SetWindowPos( hWnd, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                  SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED );
    return TRUE;
}

/***********************************************************************
 *           DrawMenuBarTemp   (USER32.@)
 *
 * UNDOCUMENTED !!
 *
 * called by W98SE desk.cpl Control Panel Applet
 *
 * Not 100% sure about the param names, but close.
 */
DWORD WINAPI DrawMenuBarTemp(HWND someHWND, HDC someHDC, LPRECT someRECT, HMENU someHMENU, HFONT someFONT)
{
    FIXME("(0x%08x, 0x%08x, %p, 0x%08x, 0x%08x): stub\n", someHWND, someHDC, someRECT, someHMENU, someFONT);
    return 0;
}

/***********************************************************************
 *           EndMenu   (USER.187)
 *           EndMenu   (USER32.@)
 */
void WINAPI EndMenu(void)
{
    /* if we are in the menu code, and it is active */
    if (!fEndMenu && top_popup)
    {
	/* terminate the menu handling code */
        fEndMenu = TRUE;

	/* needs to be posted to wakeup the internal menu handler */
	/* which will now terminate the menu, in the event that */
	/* the main window was minimized, or lost focus, so we */
	/* don't end up with an orphaned menu */
        PostMessageA( top_popup, WM_CANCELMODE, 0, 0);
    }
}


/***********************************************************************
 *           LookupMenuHandle   (USER.217)
 */
HMENU16 WINAPI LookupMenuHandle16( HMENU16 hmenu, INT16 id )
{
    HMENU hmenu32 = hmenu;
    UINT id32 = id;
    if (!MENU_FindItem( &hmenu32, &id32, MF_BYCOMMAND )) return 0;
    else return hmenu32;
}


/**********************************************************************
 *	    LoadMenu    (USER.150)
 */
HMENU16 WINAPI LoadMenu16( HINSTANCE16 instance, LPCSTR name )
{
    HRSRC16 hRsrc;
    HGLOBAL16 handle;
    HMENU16 hMenu;

    TRACE("(%04x,%s)\n", instance, debugres_a(name) );

    if (HIWORD(name))
    {
        if (name[0] == '#') name = (LPCSTR)atoi( name + 1 );
    }

    if (!name) return 0;

    /* check for Win32 module */
    if (HIWORD(instance)) return LoadMenuA( instance, name );
    instance = GetExePtr( instance );

    if (!(hRsrc = FindResource16( instance, name, RT_MENUA ))) return 0;
    if (!(handle = LoadResource16( instance, hRsrc ))) return 0;
    hMenu = LoadMenuIndirect16(LockResource16(handle));
    FreeResource16( handle );
    return hMenu;
}


/*****************************************************************
 *        LoadMenuA   (USER32.@)
 */
HMENU WINAPI LoadMenuA( HINSTANCE instance, LPCSTR name )
{
    HRSRC hrsrc = FindResourceA( instance, name, RT_MENUA );
    if (!hrsrc) return 0;
    return LoadMenuIndirectA( (LPCVOID)LoadResource( instance, hrsrc ));
}


/*****************************************************************
 *        LoadMenuW   (USER32.@)
 */
HMENU WINAPI LoadMenuW( HINSTANCE instance, LPCWSTR name )
{
    HRSRC hrsrc = FindResourceW( instance, name, RT_MENUW );
    if (!hrsrc) return 0;
    return LoadMenuIndirectW( (LPCVOID)LoadResource( instance, hrsrc ));
}


/**********************************************************************
 *	    LoadMenuIndirect    (USER.220)
 */
HMENU16 WINAPI LoadMenuIndirect16( LPCVOID template )
{
    HMENU16 hMenu;
    WORD version, offset;
    LPCSTR p = (LPCSTR)template;

    TRACE("(%p)\n", template );
    version = GET_WORD(p);
    p += sizeof(WORD);
    if (version)
    {
        WARN("version must be 0 for Win16\n" );
        return 0;
    }
    offset = GET_WORD(p);
    p += sizeof(WORD) + offset;
    if (!(hMenu = CreateMenu())) return 0;
    if (!MENU_ParseResource( p, hMenu, FALSE ))
    {
        DestroyMenu( hMenu );
        return 0;
    }
    return hMenu;
}


/**********************************************************************
 *	    LoadMenuIndirectA    (USER32.@)
 */
HMENU WINAPI LoadMenuIndirectA( LPCVOID template )
{
    HMENU16 hMenu;
    WORD version, offset;
    LPCSTR p = (LPCSTR)template;

    TRACE("%p\n", template );
    version = GET_WORD(p);
    p += sizeof(WORD);
    switch (version)
      {
      case 0:
	offset = GET_WORD(p);
	p += sizeof(WORD) + offset;
	if (!(hMenu = CreateMenu())) return 0;
	if (!MENU_ParseResource( p, hMenu, TRUE ))
	  {
	    DestroyMenu( hMenu );
	    return 0;
	  }
	return hMenu;
      case 1:
	offset = GET_WORD(p);
	p += sizeof(WORD) + offset;
	if (!(hMenu = CreateMenu())) return 0;
	if (!MENUEX_ParseResource( p, hMenu))
	  {
	    DestroyMenu( hMenu );
	    return 0;
	  }
	return hMenu;
      default:
        ERR("version %d not supported.\n", version);
        return 0;
      }
}


/**********************************************************************
 *	    LoadMenuIndirectW    (USER32.@)
 */
HMENU WINAPI LoadMenuIndirectW( LPCVOID template )
{
    /* FIXME: is there anything different between A and W? */
    return LoadMenuIndirectA( template );
}


/**********************************************************************
 *		IsMenu    (USER.358)
 */
BOOL16 WINAPI IsMenu16( HMENU16 hmenu )
{
    return IsMenu( hmenu );
}


/**********************************************************************
 *		IsMenu    (USER32.@)
 */
BOOL WINAPI IsMenu(HMENU hmenu)
{
    LPPOPUPMENU menu = MENU_GetMenu(hmenu);
    return menu != NULL;
}

/**********************************************************************
 *		GetMenuItemInfo_common
 */

static BOOL GetMenuItemInfo_common ( HMENU hmenu, UINT item, BOOL bypos,
					LPMENUITEMINFOW lpmii, BOOL unicode)
{
    MENUITEM *menu = MENU_FindItem (&hmenu, &item, bypos? MF_BYPOSITION : 0);

    debug_print_menuitem("GetMenuItemInfo_common: ", menu, "");

    if (!menu)
	return FALSE;

    if (lpmii->fMask & MIIM_TYPE) {
	lpmii->fType = menu->fType;
	switch (MENU_ITEM_TYPE(menu->fType)) {
	case MF_STRING:
            break;  /* will be done below */
	case MF_OWNERDRAW:
	case MF_BITMAP:
	    lpmii->dwTypeData = menu->text;
	    /* fall through */
	default:
	    lpmii->cch = 0;
	}
    }

    /* copy the text string */
    if ((lpmii->fMask & (MIIM_TYPE|MIIM_STRING)) &&
         (MENU_ITEM_TYPE(menu->fType) == MF_STRING) && menu->text)
    {
        int len;
        if (unicode)
        {
            len = strlenW(menu->text);
            if(lpmii->dwTypeData && lpmii->cch)
                lstrcpynW(lpmii->dwTypeData, menu->text, lpmii->cch);
        }
        else
        {
            len = WideCharToMultiByte( CP_ACP, 0, menu->text, -1, NULL, 0, NULL, NULL );
            if(lpmii->dwTypeData && lpmii->cch)
                if (!WideCharToMultiByte( CP_ACP, 0, menu->text, -1,
                                          (LPSTR)lpmii->dwTypeData, lpmii->cch, NULL, NULL ))
                    ((LPSTR)lpmii->dwTypeData)[lpmii->cch-1] = 0;
        }
        /* if we've copied a substring we return its length */
        if(lpmii->dwTypeData && lpmii->cch)
        {
            if (lpmii->cch <= len) lpmii->cch--;
        }
        else /* return length of string */
            lpmii->cch = len;
    }

    if (lpmii->fMask & MIIM_FTYPE)
	lpmii->fType = menu->fType;

    if (lpmii->fMask & MIIM_BITMAP)
	lpmii->hbmpItem = menu->hbmpItem;

    if (lpmii->fMask & MIIM_STATE)
	lpmii->fState = menu->fState;

    if (lpmii->fMask & MIIM_ID)
	lpmii->wID = menu->wID;

    if (lpmii->fMask & MIIM_SUBMENU)
	lpmii->hSubMenu = menu->hSubMenu;

    if (lpmii->fMask & MIIM_CHECKMARKS) {
	lpmii->hbmpChecked = menu->hCheckBit;
	lpmii->hbmpUnchecked = menu->hUnCheckBit;
    }
    if (lpmii->fMask & MIIM_DATA)
	lpmii->dwItemData = menu->dwItemData;

  return TRUE;
}

/**********************************************************************
 *		GetMenuItemInfoA    (USER32.@)
 */
BOOL WINAPI GetMenuItemInfoA( HMENU hmenu, UINT item, BOOL bypos,
                                  LPMENUITEMINFOA lpmii)
{
    return GetMenuItemInfo_common (hmenu, item, bypos,
                                    (LPMENUITEMINFOW)lpmii, FALSE);
}

/**********************************************************************
 *		GetMenuItemInfoW    (USER32.@)
 */
BOOL WINAPI GetMenuItemInfoW( HMENU hmenu, UINT item, BOOL bypos,
                                  LPMENUITEMINFOW lpmii)
{
    return GetMenuItemInfo_common (hmenu, item, bypos,
                                     lpmii, TRUE);
}


/* set a menu item text from a ASCII or Unicode string */
inline static void set_menu_item_text( MENUITEM *menu, LPCWSTR text, BOOL unicode )
{
    if (!text)
    {
        menu->text = NULL;
        menu->fType |= MF_SEPARATOR;
    }
    else if (unicode)
    {
        if ((menu->text = HeapAlloc( GetProcessHeap(), 0, (strlenW(text)+1) * sizeof(WCHAR) )))
            strcpyW( menu->text, text );
    }
    else
    {
        LPCSTR str = (LPCSTR)text;
        int len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
        if ((menu->text = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) )))
            MultiByteToWideChar( CP_ACP, 0, str, -1, menu->text, len );
    }
}


/**********************************************************************
 *		SetMenuItemInfo_common
 */

static BOOL SetMenuItemInfo_common(MENUITEM * menu,
				       const MENUITEMINFOW *lpmii,
				       BOOL unicode)
{
    if (!menu) return FALSE;

    debug_print_menuitem("MENU_SetItemInfo_common from: ", menu, "");

    if (lpmii->fMask & MIIM_TYPE ) {
	/* Get rid of old string.  */
	if ( IS_STRING_ITEM(menu->fType) && menu->text) {
	    HeapFree(GetProcessHeap(), 0, menu->text);
	    menu->text = NULL;
	}

	/* make only MENU_ITEM_TYPE bits in menu->fType equal lpmii->fType */
	menu->fType &= ~MENU_ITEM_TYPE(menu->fType);
	menu->fType |= MENU_ITEM_TYPE(lpmii->fType);

	menu->text = lpmii->dwTypeData;

       if (IS_STRING_ITEM(menu->fType))
           set_menu_item_text( menu, lpmii->dwTypeData, unicode );
    }

    if (lpmii->fMask & MIIM_FTYPE ) {
	/* free the string when the type is changing */
	if ( (!IS_STRING_ITEM(lpmii->fType)) && IS_STRING_ITEM(menu->fType) && menu->text) {
	    HeapFree(GetProcessHeap(), 0, menu->text);
	    menu->text = NULL;
	}
	menu->fType &= ~MENU_ITEM_TYPE(menu->fType);
	menu->fType |= MENU_ITEM_TYPE(lpmii->fType);
        if ( IS_STRING_ITEM(menu->fType) && !menu->text )
            menu->fType |= MF_SEPARATOR;
    }

    if (lpmii->fMask & MIIM_STRING ) {
	/* free the string when used */
	if ( IS_STRING_ITEM(menu->fType) && menu->text) {
	    HeapFree(GetProcessHeap(), 0, menu->text);
            set_menu_item_text( menu, lpmii->dwTypeData, unicode );
	}
    }

    if (lpmii->fMask & MIIM_STATE)
    {
	/* FIXME: MFS_DEFAULT do we have to reset the other menu items? */
	menu->fState = lpmii->fState;
    }

    if (lpmii->fMask & MIIM_ID)
	menu->wID = lpmii->wID;

    if (lpmii->fMask & MIIM_SUBMENU) {
	menu->hSubMenu = lpmii->hSubMenu;
	if (menu->hSubMenu) {
	    POPUPMENU *subMenu = MENU_GetMenu((UINT16)menu->hSubMenu);
	    if (subMenu) {
		subMenu->wFlags |= MF_POPUP;
		menu->fType |= MF_POPUP;
	    }
	    else
		/* FIXME: Return an error ? */
		menu->fType &= ~MF_POPUP;
	}
	else
	    menu->fType &= ~MF_POPUP;
    }

    if (lpmii->fMask & MIIM_CHECKMARKS)
    {
        if (lpmii->fType & MFT_RADIOCHECK)
            menu->fType |= MFT_RADIOCHECK;

	menu->hCheckBit = lpmii->hbmpChecked;
	menu->hUnCheckBit = lpmii->hbmpUnchecked;
    }
    if (lpmii->fMask & MIIM_DATA)
	menu->dwItemData = lpmii->dwItemData;

    debug_print_menuitem("SetMenuItemInfo_common to : ", menu, "");
    return TRUE;
}

/**********************************************************************
 *		SetMenuItemInfoA    (USER32.@)
 */
BOOL WINAPI SetMenuItemInfoA(HMENU hmenu, UINT item, BOOL bypos,
                                 const MENUITEMINFOA *lpmii)
{
    return SetMenuItemInfo_common(MENU_FindItem(&hmenu, &item, bypos? MF_BYPOSITION : 0),
				    (const MENUITEMINFOW *)lpmii, FALSE);
}

/**********************************************************************
 *		SetMenuItemInfoW    (USER32.@)
 */
BOOL WINAPI SetMenuItemInfoW(HMENU hmenu, UINT item, BOOL bypos,
                                 const MENUITEMINFOW *lpmii)
{
    return SetMenuItemInfo_common(MENU_FindItem(&hmenu, &item, bypos? MF_BYPOSITION : 0),
				    lpmii, TRUE);
}

/**********************************************************************
 *		SetMenuDefaultItem    (USER32.@)
 *
 */
BOOL WINAPI SetMenuDefaultItem(HMENU hmenu, UINT uItem, UINT bypos)
{
	UINT i;
	POPUPMENU *menu;
	MENUITEM *item;

	TRACE("(0x%x,%d,%d)\n", hmenu, uItem, bypos);

	if (!(menu = MENU_GetMenu(hmenu))) return FALSE;

	/* reset all default-item flags */
	item = menu->items;
	for (i = 0; i < menu->nItems; i++, item++)
	{
	    item->fState &= ~MFS_DEFAULT;
	}

	/* no default item */
	if ( -1 == uItem)
	{
	    return TRUE;
	}

	item = menu->items;
	if ( bypos )
	{
	    if ( uItem >= menu->nItems ) return FALSE;
	    item[uItem].fState |= MFS_DEFAULT;
	    return TRUE;
	}
	else
	{
	    for (i = 0; i < menu->nItems; i++, item++)
	    {
		if (item->wID == uItem)
		{
		     item->fState |= MFS_DEFAULT;
		     return TRUE;
		}
	    }

	}
	return FALSE;
}

/**********************************************************************
 *		GetMenuDefaultItem    (USER32.@)
 */
UINT WINAPI GetMenuDefaultItem(HMENU hmenu, UINT bypos, UINT flags)
{
	POPUPMENU *menu;
	MENUITEM * item;
	UINT i = 0;

	TRACE("(0x%x,%d,%d)\n", hmenu, bypos, flags);

	if (!(menu = MENU_GetMenu(hmenu))) return -1;

	/* find default item */
	item = menu->items;

	/* empty menu */
	if (! item) return -1;

	while ( !( item->fState & MFS_DEFAULT ) )
	{
	    i++; item++;
	    if  (i >= menu->nItems ) return -1;
	}

	/* default: don't return disabled items */
	if ( (!(GMDI_USEDISABLED & flags)) && (item->fState & MFS_DISABLED )) return -1;

	/* search rekursiv when needed */
	if ( (item->fType & MF_POPUP) &&  (flags & GMDI_GOINTOPOPUPS) )
	{
	    UINT ret;
	    ret = GetMenuDefaultItem( item->hSubMenu, bypos, flags );
	    if ( -1 != ret ) return ret;

	    /* when item not found in submenu, return the popup item */
	}
	return ( bypos ) ? i : item->wID;

}

/*******************************************************************
 *              InsertMenuItem   (USER.441)
 *
 * FIXME: untested
 */
BOOL16 WINAPI InsertMenuItem16( HMENU16 hmenu, UINT16 pos, BOOL16 byposition,
                                const MENUITEMINFO16 *mii )
{
    MENUITEMINFOA miia;

    miia.cbSize        = sizeof(miia);
    miia.fMask         = mii->fMask;
    miia.dwTypeData    = (LPSTR)mii->dwTypeData;
    miia.fType         = mii->fType;
    miia.fState        = mii->fState;
    miia.wID           = mii->wID;
    miia.hSubMenu      = mii->hSubMenu;
    miia.hbmpChecked   = mii->hbmpChecked;
    miia.hbmpUnchecked = mii->hbmpUnchecked;
    miia.dwItemData    = mii->dwItemData;
    miia.cch           = mii->cch;
    if (IS_STRING_ITEM(miia.fType))
        miia.dwTypeData = MapSL(mii->dwTypeData);
    return InsertMenuItemA( hmenu, pos, byposition, &miia );
}


/**********************************************************************
 *		InsertMenuItemA    (USER32.@)
 */
BOOL WINAPI InsertMenuItemA(HMENU hMenu, UINT uItem, BOOL bypos,
                                const MENUITEMINFOA *lpmii)
{
    MENUITEM *item = MENU_InsertItem(hMenu, uItem, bypos ? MF_BYPOSITION : 0 );
    return SetMenuItemInfo_common(item, (const MENUITEMINFOW *)lpmii, FALSE);
}


/**********************************************************************
 *		InsertMenuItemW    (USER32.@)
 */
BOOL WINAPI InsertMenuItemW(HMENU hMenu, UINT uItem, BOOL bypos,
                                const MENUITEMINFOW *lpmii)
{
    MENUITEM *item = MENU_InsertItem(hMenu, uItem, bypos ? MF_BYPOSITION : 0 );
    return SetMenuItemInfo_common(item, lpmii, TRUE);
}

/**********************************************************************
 *		CheckMenuRadioItem    (USER32.@)
 */

BOOL WINAPI CheckMenuRadioItem(HMENU hMenu,
				   UINT first, UINT last, UINT check,
				   UINT bypos)
{
     MENUITEM *mifirst, *milast, *micheck;
     HMENU mfirst = hMenu, mlast = hMenu, mcheck = hMenu;

     TRACE("ox%x: %d-%d, check %d, bypos=%d\n",
		  hMenu, first, last, check, bypos);

     mifirst = MENU_FindItem (&mfirst, &first, bypos);
     milast = MENU_FindItem (&mlast, &last, bypos);
     micheck = MENU_FindItem (&mcheck, &check, bypos);

     if (mifirst == NULL || milast == NULL || micheck == NULL ||
	 mifirst > milast || mfirst != mlast || mfirst != mcheck ||
	 micheck > milast || micheck < mifirst)
	  return FALSE;

     while (mifirst <= milast)
     {
	  if (mifirst == micheck)
	  {
	       mifirst->fType |= MFT_RADIOCHECK;
	       mifirst->fState |= MFS_CHECKED;
	  } else {
	       mifirst->fType &= ~MFT_RADIOCHECK;
	       mifirst->fState &= ~MFS_CHECKED;
	  }
	  mifirst++;
     }

     return TRUE;
}

/**********************************************************************
 *		CheckMenuRadioItem (USER.666)
 */
BOOL16 WINAPI CheckMenuRadioItem16(HMENU16 hMenu,
				   UINT16 first, UINT16 last, UINT16 check,
				   BOOL16 bypos)
{
     return CheckMenuRadioItem (hMenu, first, last, check, bypos);
}

/**********************************************************************
 *		GetMenuItemRect    (USER32.@)
 *
 *      ATTENTION: Here, the returned values in rect are the screen
 *                 coordinates of the item just like if the menu was
 *                 always on the upper left side of the application.
 *
 */
BOOL WINAPI GetMenuItemRect (HWND hwnd, HMENU hMenu, UINT uItem,
				 LPRECT rect)
{
     POPUPMENU *itemMenu;
     MENUITEM *item;
     HWND referenceHwnd;

     TRACE("(0x%x,0x%x,%d,%p)\n", hwnd, hMenu, uItem, rect);

     item = MENU_FindItem (&hMenu, &uItem, MF_BYPOSITION);
     referenceHwnd = hwnd;

     if(!hwnd)
     {
	 itemMenu = MENU_GetMenu(hMenu);
	 if (itemMenu == NULL)
	     return FALSE;

	 if(itemMenu->hWnd == 0)
	     return FALSE;
	 referenceHwnd = itemMenu->hWnd;
     }

     if ((rect == NULL) || (item == NULL))
	 return FALSE;

     *rect = item->rect;

     MapWindowPoints(referenceHwnd, 0, (LPPOINT)rect, 2);

     return TRUE;
}


/**********************************************************************
 *		SetMenuInfo    (USER32.@)
 *
 * FIXME
 *	MIM_APPLYTOSUBMENUS
 *	actually use the items to draw the menu
 */
BOOL WINAPI SetMenuInfo (HMENU hMenu, LPCMENUINFO lpmi)
{
    POPUPMENU *menu;

    TRACE("(0x%04x %p)\n", hMenu, lpmi);

    if (lpmi && (lpmi->cbSize==sizeof(MENUINFO)) && (menu = MENU_GetMenu(hMenu)))
    {

	if (lpmi->fMask & MIM_BACKGROUND)
	    menu->hbrBack = lpmi->hbrBack;

	if (lpmi->fMask & MIM_HELPID)
	    menu->dwContextHelpID = lpmi->dwContextHelpID;

	if (lpmi->fMask & MIM_MAXHEIGHT)
	    menu->cyMax = lpmi->cyMax;

	if (lpmi->fMask & MIM_MENUDATA)
	    menu->dwMenuData = lpmi->dwMenuData;

	if (lpmi->fMask & MIM_STYLE)
	    menu->dwStyle = lpmi->dwStyle;

	return TRUE;
    }
    return FALSE;
}

/**********************************************************************
 *		GetMenuInfo    (USER32.@)
 *
 *  NOTES
 *	win98/NT5.0
 *
 */
BOOL WINAPI GetMenuInfo (HMENU hMenu, LPMENUINFO lpmi)
{   POPUPMENU *menu;

    TRACE("(0x%04x %p)\n", hMenu, lpmi);

    if (lpmi && (menu = MENU_GetMenu(hMenu)))
    {

	if (lpmi->fMask & MIM_BACKGROUND)
	    lpmi->hbrBack = menu->hbrBack;

	if (lpmi->fMask & MIM_HELPID)
	    lpmi->dwContextHelpID = menu->dwContextHelpID;

	if (lpmi->fMask & MIM_MAXHEIGHT)
	    lpmi->cyMax = menu->cyMax;

	if (lpmi->fMask & MIM_MENUDATA)
	    lpmi->dwMenuData = menu->dwMenuData;

	if (lpmi->fMask & MIM_STYLE)
	    lpmi->dwStyle = menu->dwStyle;

	return TRUE;
    }
    return FALSE;
}

/**********************************************************************
 *         SetMenuContextHelpId    (USER.384)
 */
BOOL16 WINAPI SetMenuContextHelpId16( HMENU16 hMenu, DWORD dwContextHelpID)
{
	return SetMenuContextHelpId( hMenu, dwContextHelpID );
}


/**********************************************************************
 *         SetMenuContextHelpId    (USER32.@)
 */
BOOL WINAPI SetMenuContextHelpId( HMENU hMenu, DWORD dwContextHelpID)
{
    LPPOPUPMENU menu;

    TRACE("(0x%04x 0x%08lx)\n", hMenu, dwContextHelpID);

    if ((menu = MENU_GetMenu(hMenu)))
    {
	menu->dwContextHelpID = dwContextHelpID;
	return TRUE;
    }
    return FALSE;
}

/**********************************************************************
 *         GetMenuContextHelpId    (USER.385)
 */
DWORD WINAPI GetMenuContextHelpId16( HMENU16 hMenu )
{
	return GetMenuContextHelpId( hMenu );
}

/**********************************************************************
 *         GetMenuContextHelpId    (USER32.@)
 */
DWORD WINAPI GetMenuContextHelpId( HMENU hMenu )
{
    LPPOPUPMENU menu;

    TRACE("(0x%04x)\n", hMenu);

    if ((menu = MENU_GetMenu(hMenu)))
    {
	return menu->dwContextHelpID;
    }
    return 0;
}

/**********************************************************************
 *         MenuItemFromPoint    (USER32.@)
 */
UINT WINAPI MenuItemFromPoint(HWND hWnd, HMENU hMenu, POINT ptScreen)
{
    POPUPMENU *menu = MENU_GetMenu(hMenu);
    UINT pos;
    MENUITEM *item;

    /*FIXME: Do we have to handle hWnd here? */
    item = MENU_FindItemByCoords(menu, ptScreen, &pos);

    return pos;
}


/**********************************************************************
 *           translate_accelerator
 */
static BOOL translate_accelerator( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                   BYTE fVirt, WORD key, WORD cmd )
{
    UINT mesg = 0;

    if (wParam != key) return FALSE;

    if (message == WM_CHAR)
    {
        if ( !(fVirt & FALT) && !(fVirt & FVIRTKEY) )
        {
            TRACE_(accel)("found accel for WM_CHAR: ('%c')\n", wParam & 0xff);
            goto found;
        }
    }
    else
    {
        if(fVirt & FVIRTKEY)
        {
            INT mask = 0;
            TRACE_(accel)("found accel for virt_key %04x (scan %04x)\n",
                          wParam, 0xff & HIWORD(lParam));
            if(GetKeyState(VK_SHIFT) & 0x8000) mask |= FSHIFT;
            if(GetKeyState(VK_CONTROL) & 0x8000) mask |= FCONTROL;
            if(GetKeyState(VK_MENU) & 0x8000) mask |= FALT;
            if(mask == (fVirt & (FSHIFT | FCONTROL | FALT))) goto found;
            TRACE_(accel)(", but incorrect SHIFT/CTRL/ALT-state\n");
        }
        else
        {
            if (!(lParam & 0x01000000))  /* no special_key */
            {
                if ((fVirt & FALT) && (lParam & 0x20000000))
                {                              /* ^^ ALT pressed */
                    TRACE_(accel)("found accel for Alt-%c\n", wParam & 0xff);
                    goto found;
                }
            }
        }
    }
    return FALSE;

 found:
    if (message == WM_KEYUP || message == WM_SYSKEYUP)
        mesg = 1;
    else if (GetCapture())
        mesg = 2;
    else if (!IsWindowEnabled(hWnd))
        mesg = 3;
    else
    {
        HMENU hMenu, hSubMenu, hSysMenu;
        UINT uSysStat = (UINT)-1, uStat = (UINT)-1, nPos;

        hMenu = (GetWindowLongA( hWnd, GWL_STYLE ) & WS_CHILD) ? 0 : GetMenu(hWnd);
        hSysMenu = get_win_sys_menu( hWnd );

        /* find menu item and ask application to initialize it */
        /* 1. in the system menu */
        hSubMenu = hSysMenu;
        nPos = cmd;
        if(MENU_FindItem(&hSubMenu, &nPos, MF_BYCOMMAND))
        {
            SendMessageA(hWnd, WM_INITMENU, (WPARAM)hSysMenu, 0L);
            if(hSubMenu != hSysMenu)
            {
                nPos = MENU_FindSubMenu(&hSysMenu, hSubMenu);
                TRACE_(accel)("hSysMenu = %04x, hSubMenu = %04x, nPos = %d\n", hSysMenu, hSubMenu, nPos);
                SendMessageA(hWnd, WM_INITMENUPOPUP, (WPARAM)hSubMenu, MAKELPARAM(nPos, TRUE));
            }
            uSysStat = GetMenuState(GetSubMenu(hSysMenu, 0), cmd, MF_BYCOMMAND);
        }
        else /* 2. in the window's menu */
        {
            hSubMenu = hMenu;
            nPos = cmd;
            if(MENU_FindItem(&hSubMenu, &nPos, MF_BYCOMMAND))
            {
                SendMessageA(hWnd, WM_INITMENU, (WPARAM)hMenu, 0L);
                if(hSubMenu != hMenu)
                {
                    nPos = MENU_FindSubMenu(&hMenu, hSubMenu);
                    TRACE_(accel)("hMenu = %04x, hSubMenu = %04x, nPos = %d\n", hMenu, hSubMenu, nPos);
                    SendMessageA(hWnd, WM_INITMENUPOPUP, (WPARAM)hSubMenu, MAKELPARAM(nPos, FALSE));
                }
                uStat = GetMenuState(hMenu, cmd, MF_BYCOMMAND);
            }
        }

        if (uSysStat != (UINT)-1)
        {
            if (uSysStat & (MF_DISABLED|MF_GRAYED))
                mesg=4;
            else
                mesg=WM_SYSCOMMAND;
        }
        else
        {
            if (uStat != (UINT)-1)
            {
                if (IsIconic(hWnd))
                    mesg=5;
                else
                {
                    if (uStat & (MF_DISABLED|MF_GRAYED))
                        mesg=6;
                    else
                        mesg=WM_COMMAND;
                }
            }
            else
                mesg=WM_COMMAND;
        }
    }

    if( mesg==WM_COMMAND )
    {
        TRACE_(accel)(", sending WM_COMMAND, wParam=%0x\n", 0x10000 | cmd);
        SendMessageA(hWnd, mesg, 0x10000 | cmd, 0L);
    }
    else if( mesg==WM_SYSCOMMAND )
    {
        TRACE_(accel)(", sending WM_SYSCOMMAND, wParam=%0x\n", cmd);
        SendMessageA(hWnd, mesg, cmd, 0x00010000L);
    }
    else
    {
        /*  some reasons for NOT sending the WM_{SYS}COMMAND message:
         *   #0: unknown (please report!)
         *   #1: for WM_KEYUP,WM_SYSKEYUP
         *   #2: mouse is captured
         *   #3: window is disabled
         *   #4: it's a disabled system menu option
         *   #5: it's a menu option, but window is iconic
         *   #6: it's a menu option, but disabled
         */
        TRACE_(accel)(", but won't send WM_{SYS}COMMAND, reason is #%d\n",mesg);
        if(mesg==0)
            ERR_(accel)(" unknown reason - please report!");
    }
    return TRUE;
}

/**********************************************************************
 *      TranslateAccelerator      (USER32.@)
 *      TranslateAcceleratorA     (USER32.@)
 *      TranslateAcceleratorW     (USER32.@)
 */
INT WINAPI TranslateAccelerator( HWND hWnd, HACCEL hAccel, LPMSG msg )
{
    /* YES, Accel16! */
    LPACCEL16 lpAccelTbl;
    int i;

    if (msg == NULL)
    {
        WARN_(accel)("msg null; should hang here to be win compatible\n");
        return 0;
    }
    if (!hAccel || !(lpAccelTbl = (LPACCEL16) LockResource16(HACCEL_16(hAccel))))
    {
        WARN_(accel)("invalid accel handle=%x\n", hAccel);
        return 0;
    }
    if ((msg->message != WM_KEYDOWN &&
         msg->message != WM_KEYUP &&
         msg->message != WM_SYSKEYDOWN &&
         msg->message != WM_SYSKEYUP &&
         msg->message != WM_CHAR)) return 0;

    TRACE_(accel)("TranslateAccelerators hAccel=%04x, hWnd=%04x,"
                  "msg->hwnd=%04x, msg->message=%04x, wParam=%08x, lParam=%lx\n",
                  hAccel,hWnd,msg->hwnd,msg->message,msg->wParam,msg->lParam);

    i = 0;
    do
    {
        if (translate_accelerator( hWnd, msg->message, msg->wParam, msg->lParam,
                                   lpAccelTbl[i].fVirt, lpAccelTbl[i].key, lpAccelTbl[i].cmd))
            return 1;
    } while ((lpAccelTbl[i++].fVirt & 0x80) == 0);
    WARN_(accel)("couldn't translate accelerator key\n");
    return 0;
}
