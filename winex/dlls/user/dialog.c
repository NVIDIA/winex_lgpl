/*
 * Dialog functions
 *
 * Copyright 1993, 1994, 1996 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"
#include "wine/port.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winnls.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "windowsx.h"
#include "wine/winuser16.h"
#include "wine/winbase16.h"
#include "wine/unicode.h"
#include "controls.h"
#include "heap.h"
#include "win.h"
#include "user.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dialog);


  /* Dialog control information */
typedef struct
{
    DWORD      style;
    DWORD      exStyle;
    DWORD      helpId;
    INT16      x;
    INT16      y;
    INT16      cx;
    INT16      cy;
    UINT       id;
    LPCSTR     className;
    LPCSTR     windowName;
    LPCVOID    data;
} DLG_CONTROL_INFO;

  /* Dialog template */
typedef struct
{
    DWORD      style;
    DWORD      exStyle;
    DWORD      helpId;
    UINT16     nbItems;
    INT16      x;
    INT16      y;
    INT16      cx;
    INT16      cy;
    LPCSTR     menuName;
    LPCSTR     className;
    LPCSTR     caption;
    WORD       pointSize;
    WORD       weight;
    BOOL       italic;
    LPCSTR     faceName;
    BOOL       dialogEx;
} DLG_TEMPLATE;

  /* Radio button group */
typedef struct
{
    UINT firstID;
    UINT lastID;
    UINT checkID;
} RADIOGROUP;

  /* Dialog base units */
static WORD xBaseUnit = 0, yBaseUnit = 0;


/*********************************************************************
 * dialog class descriptor
 */
const struct builtin_class_descr DIALOG_builtin_class =
{
    DIALOG_CLASS_ATOM,  /* name */
    CS_GLOBALCLASS | CS_SAVEBITS, /* style  */
    DefDlgProcA,        /* procA */
    DefDlgProcW,        /* procW */
    DLGWINDOWEXTRA,     /* extra */
    IDC_ARROWA,         /* cursor */
    0                   /* brush */
};


/***********************************************************************
 *           DIALOG_EnableOwner
 *
 * Helper function for modal dialogs to enable again the
 * owner of the dialog box.
 */
void DIALOG_EnableOwner( HWND hOwner )
{
    /* Owner must be a top-level window */
    if (hOwner)
        hOwner = GetAncestor( hOwner, GA_ROOT );
    if (!hOwner) return;
    EnableWindow( hOwner, TRUE );
}


/***********************************************************************
 *           DIALOG_DisableOwner
 *
 * Helper function for modal dialogs to disable the
 * owner of the dialog box. Returns TRUE if owner was enabled.
 */
BOOL DIALOG_DisableOwner( HWND hOwner )
{
    /* Owner must be a top-level window */
    if (hOwner)
        hOwner = GetAncestor( hOwner, GA_ROOT );
    if (!hOwner) return FALSE;
    if (IsWindowEnabled( hOwner ))
    {
        EnableWindow( hOwner, FALSE );
        return TRUE;
    }
    else
        return FALSE;
}

/***********************************************************************
 *           DIALOG_GetCharSizeFromDC
 *
 * Calculates the *true* average size of English characters in the
 * specified font as oppposed to the one returned by GetTextMetrics.
 * Despite most of MSDN insisting that the horizontal base unit is
 * tmAveCharWidth it isn't (because it is too imprecise). So we calculate a
 * more precise vesrion of tmAveCharWidth.
 * See Knowledge base article Q145994 "HOWTO: Calculate Dialog Units When Not
 * Using the System Font". (This code is taken from the article method 1).
 */
static BOOL DIALOG_GetCharSizeFromDC( HDC hDC, HFONT hFont, SIZE * pSize )
{
    HFONT hFontPrev = 0;
    SIZE sz;
    TEXTMETRICA tm;
    BOOL rc = TRUE;

    pSize->cx = xBaseUnit;
    pSize->cy = yBaseUnit;

    if(!hDC) return FALSE;

    if(hFont)
    {
      hFontPrev = SelectFont(hDC, hFont);
      if( !hFontPrev ) return FALSE;
    }

    if( !GetTextMetricsA(hDC, &tm) ||
        !GetTextExtentPointA(hDC, 
                             "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                             52, /* Number of char in english alphabet */
                             &sz))
    {
      rc = FALSE;
    }
    else
    {

      pSize->cy = tm.tmHeight;
      pSize->cx = ((sz.cx / 26) + 1) / 2;  /* Average width rounded up */
      TRACE("dlg base units: %ld x %ld\n", pSize->cx, pSize->cy);
    }

    if (hFontPrev) SelectFont(hDC, hFontPrev);

    return rc;
}

/***********************************************************************
 *           DIALOG_GetCharSize
 *
 *  A convenient variant of DIALOG_GetCharSizeFromDC.
 */
static BOOL DIALOG_GetCharSize( HFONT hFont, SIZE * pSize )
{
    HDC  hDC = GetDC(0);
    BOOL Success = DIALOG_GetCharSizeFromDC( hDC, hFont, pSize );
    ReleaseDC(0, hDC);
    return Success;
}

/***********************************************************************
 *           DIALOG_Init
 *
 * Initialisation of the dialog manager.
 */
BOOL DIALOG_Init(void)
{
    HDC hdc;
    SIZE size;

      /* Calculate the dialog base units */

    if (!(hdc = CreateDCA( "DISPLAY", NULL, NULL, NULL )))
    {
	ERR("Could not create Display DC\n");
	return FALSE;
    }

    if (!DIALOG_GetCharSizeFromDC( hdc, 0, &size ))
    {
	DeleteDC( hdc );
	ERR("Could not initialize base dialog units\n");
	return FALSE;
    }

    DeleteDC( hdc );
    xBaseUnit = size.cx;
    yBaseUnit = size.cy;

    TRACE("base units = %d,%d\n", xBaseUnit, yBaseUnit );
    return TRUE;
}


/***********************************************************************
 *           DIALOG_GetControl16
 *
 * Return the class and text of the control pointed to by ptr,
 * fill the header structure and return a pointer to the next control.
 */
static LPCSTR DIALOG_GetControl16( LPCSTR p, DLG_CONTROL_INFO *info )
{
    static char buffer[10];
    int int_id;

    info->x       = GET_WORD(p);  p += sizeof(WORD);
    info->y       = GET_WORD(p);  p += sizeof(WORD);
    info->cx      = GET_WORD(p);  p += sizeof(WORD);
    info->cy      = GET_WORD(p);  p += sizeof(WORD);
    info->id      = GET_WORD(p);  p += sizeof(WORD);
    info->style   = GET_DWORD(p); p += sizeof(DWORD);
    info->exStyle = 0;

    if (*p & 0x80)
    {
        switch((BYTE)*p)
        {
            case 0x80: strcpy( buffer, "BUTTON" ); break;
            case 0x81: strcpy( buffer, "EDIT" ); break;
            case 0x82: strcpy( buffer, "STATIC" ); break;
            case 0x83: strcpy( buffer, "LISTBOX" ); break;
            case 0x84: strcpy( buffer, "SCROLLBAR" ); break;
            case 0x85: strcpy( buffer, "COMBOBOX" ); break;
            default:   buffer[0] = '\0'; break;
        }
        info->className = buffer;
        p++;
    }
    else
    {
	info->className = p;
	p += strlen(p) + 1;
    }

    int_id = ((BYTE)*p == 0xff);
    if (int_id)
    {
	  /* Integer id, not documented (?). Only works for SS_ICON controls */
	info->windowName = (LPCSTR)(UINT)GET_WORD(p+1);
	p += 3;
    }
    else
    {
	info->windowName = p;
	p += strlen(p) + 1;
    }

    if (*p) info->data = p + 1;
    else info->data = NULL;

    p += *p + 1;

    if(int_id)
      TRACE("   %s %04x %d, %d, %d, %d, %d, %08lx, %p\n",
		      info->className,  LOWORD(info->windowName),
		      info->id, info->x, info->y, info->cx, info->cy,
		      info->style, info->data );
    else
      TRACE("   %s '%s' %d, %d, %d, %d, %d, %08lx, %p\n",
		      info->className,  info->windowName,
		      info->id, info->x, info->y, info->cx, info->cy,
		      info->style, info->data );

    return p;
}


/***********************************************************************
 *           DIALOG_GetControl32
 *
 * Return the class and text of the control pointed to by ptr,
 * fill the header structure and return a pointer to the next control.
 */
static const WORD *DIALOG_GetControl32( const WORD *p, DLG_CONTROL_INFO *info,
                                        BOOL dialogEx )
{
    if (dialogEx)
    {
        info->helpId  = GET_DWORD(p); p += 2;
        info->exStyle = GET_DWORD(p); p += 2;
        info->style   = GET_DWORD(p); p += 2;
    }
    else
    {
        info->helpId  = 0;
        info->style   = GET_DWORD(p); p += 2;
        info->exStyle = GET_DWORD(p); p += 2;
    }
    info->x       = GET_WORD(p); p++;
    info->y       = GET_WORD(p); p++;
    info->cx      = GET_WORD(p); p++;
    info->cy      = GET_WORD(p); p++;

    if (dialogEx)
    {
        /* id is a DWORD for DIALOGEX */
        info->id = GET_DWORD(p);
        p += 2;
    }
    else
    {
        info->id = GET_WORD(p);
        p++;
    }

    if (GET_WORD(p) == 0xffff)
    {
        static const WCHAR class_names[6][10] =
        {
            { 'B','u','t','t','o','n', },             /* 0x80 */
            { 'E','d','i','t', },                     /* 0x81 */
            { 'S','t','a','t','i','c', },             /* 0x82 */
            { 'L','i','s','t','B','o','x', },         /* 0x83 */
            { 'S','c','r','o','l','l','B','a','r', }, /* 0x84 */
            { 'C','o','m','b','o','B','o','x', }      /* 0x85 */
        };
        WORD id = GET_WORD(p+1);
        if ((id >= 0x80) && (id <= 0x85))
            info->className = (LPCSTR)class_names[id - 0x80];
        else
        {
            info->className = NULL;
            ERR("Unknown built-in class id %04x\n", id );
        }
        p += 2;
    }
    else
    {
        info->className = (LPCSTR)p;
        p += strlenW( (LPCWSTR)p ) + 1;
    }

    if (GET_WORD(p) == 0xffff)  /* Is it an integer id? */
    {
	info->windowName = (LPCSTR)(UINT)GET_WORD(p + 1);
	p += 2;
    }
    else
    {
	info->windowName = (LPCSTR)p;
        p += strlenW( (LPCWSTR)p ) + 1;
    }

    TRACE("    %s %s %d, %d, %d, %d, %d, %08lx, %08lx, %08lx\n",
          debugstr_w( (LPCWSTR)info->className ),
          debugres_w( (LPCWSTR)info->windowName ),
          info->id, info->x, info->y, info->cx, info->cy,
          info->style, info->exStyle, info->helpId );

    if (GET_WORD(p))
    {
        if (TRACE_ON(dialog))
        {
            WORD i, count = GET_WORD(p) / sizeof(WORD);
            TRACE("  BEGIN\n");
            TRACE("    ");
            for (i = 0; i < count; i++) DPRINTF( "%04x,", GET_WORD(p+i+1) );
            DPRINTF("\n");
            TRACE("  END\n" );
        }
        info->data = p + 1;
        p += GET_WORD(p) / sizeof(WORD);
    }
    else info->data = NULL;
    p++;

    /* Next control is on dword boundary */
    return (const WORD *)((((int)p) + 3) & ~3);
}


/***********************************************************************
 *           DIALOG_CreateControls
 *
 * Create the control windows for a dialog.
 */
static BOOL DIALOG_CreateControls( HWND hwnd, LPCSTR template, const DLG_TEMPLATE *dlgTemplate,
                                   HINSTANCE hInst, BOOL win32 )
{
    DIALOGINFO *dlgInfo = DIALOG_get_info( hwnd );
    DLG_CONTROL_INFO info;
    HWND hwndCtrl, hwndDefButton = 0;
    INT items = dlgTemplate->nbItems;

    TRACE(" BEGIN\n" );
    while (items--)
    {
        if (!win32)
        {
            HINSTANCE16 instance;
            SEGPTR segptr;

            template = DIALOG_GetControl16( template, &info );
            if (HIWORD(info.className) && !strcmp( info.className, "EDIT") &&
                !(GetWindowLongW( hwnd, GWL_STYLE ) & DS_LOCALEDIT))
            {
                if (!dlgInfo->hDialogHeap)
                {
                    dlgInfo->hDialogHeap = GlobalAlloc16(GMEM_FIXED, 0x10000);
                    if (!dlgInfo->hDialogHeap)
                    {
                        ERR("Insufficient memory to create heap for edit control\n" );
                        continue;
                    }
                    LocalInit16(dlgInfo->hDialogHeap, 0, 0xffff);
                }
                instance = dlgInfo->hDialogHeap;
            }
            else instance = (HINSTANCE16)hInst;

            segptr = MapLS( info.data );
            hwndCtrl = WIN_Handle32( CreateWindowEx16( info.exStyle | WS_EX_NOPARENTNOTIFY,
                                                       info.className, info.windowName,
                                                       info.style | WS_CHILD,
                                                       MulDiv(info.x, dlgInfo->xBaseUnit, 4),
                                                       MulDiv(info.y, dlgInfo->yBaseUnit, 8),
                                                       MulDiv(info.cx, dlgInfo->xBaseUnit, 4),
                                                       MulDiv(info.cy, dlgInfo->yBaseUnit, 8),
                                                       WIN_Handle16(hwnd), (HMENU16)info.id,
                                                       instance, (LPVOID)segptr ));
            UnMapLS( segptr );
        }
        else
        {
            template = (LPCSTR)DIALOG_GetControl32( (WORD *)template, &info,
                                                    dlgTemplate->dialogEx );
            /* Is this it? */
            if (info.style & WS_BORDER)
            {
                info.style &= ~WS_BORDER;
                info.exStyle |= WS_EX_CLIENTEDGE;
            }
            hwndCtrl = CreateWindowExW( info.exStyle | WS_EX_NOPARENTNOTIFY,
                                          (LPCWSTR)info.className,
                                          (LPCWSTR)info.windowName,
                                          info.style | WS_CHILD,
                                          MulDiv(info.x, dlgInfo->xBaseUnit, 4),
                                          MulDiv(info.y, dlgInfo->yBaseUnit, 8),
                                          MulDiv(info.cx, dlgInfo->xBaseUnit, 4),
                                          MulDiv(info.cy, dlgInfo->yBaseUnit, 8),
                                          hwnd, (HMENU)info.id,
                                          hInst, (LPVOID)info.data );
        }
        if (!hwndCtrl)
        {
            if (dlgTemplate->style & DS_NOFAILCREATE) continue;
            return FALSE;
        }

            /* Send initialisation messages to the control */
        if (dlgInfo->hUserFont) SendMessageA( hwndCtrl, WM_SETFONT,
                                             (WPARAM)dlgInfo->hUserFont, 0 );
        if (SendMessageA(hwndCtrl, WM_GETDLGCODE, 0, 0) & DLGC_DEFPUSHBUTTON)
        {
              /* If there's already a default push-button, set it back */
              /* to normal and use this one instead. */
            if (hwndDefButton)
                SendMessageA( hwndDefButton, BM_SETSTYLE,
                                BS_PUSHBUTTON,FALSE );
            hwndDefButton = hwndCtrl;
            dlgInfo->idResult = GetWindowLongA( hwndCtrl, GWL_ID );
        }
    }
    TRACE(" END\n" );
    return TRUE;
}


/***********************************************************************
 *           DIALOG_ParseTemplate16
 *
 * Fill a DLG_TEMPLATE structure from the dialog template, and return
 * a pointer to the first control.
 */
static LPCSTR DIALOG_ParseTemplate16( LPCSTR p, DLG_TEMPLATE * result )
{
    result->style   = GET_DWORD(p); p += sizeof(DWORD);
    result->exStyle = 0;
    result->nbItems = (unsigned char) *p++;
    result->x       = GET_WORD(p);  p += sizeof(WORD);
    result->y       = GET_WORD(p);  p += sizeof(WORD);
    result->cx      = GET_WORD(p);  p += sizeof(WORD);
    result->cy      = GET_WORD(p);  p += sizeof(WORD);
    TRACE("DIALOG %d, %d, %d, %d\n",
                    result->x, result->y, result->cx, result->cy );
    TRACE(" STYLE %08lx\n", result->style );

    /* Get the menu name */

    switch( (BYTE)*p )
    {
    case 0:
        result->menuName = 0;
        p++;
        break;
    case 0xff:
        result->menuName = (LPCSTR)(UINT)GET_WORD( p + 1 );
        p += 3;
	TRACE(" MENU %04x\n", LOWORD(result->menuName) );
        break;
    default:
        result->menuName = p;
        TRACE(" MENU '%s'\n", p );
        p += strlen(p) + 1;
        break;
    }

    /* Get the class name */

    if (*p)
    {
        result->className = p;
        TRACE(" CLASS '%s'\n", result->className );
    }
    else result->className = DIALOG_CLASS_ATOM;
    p += strlen(p) + 1;

    /* Get the window caption */

    result->caption = p;
    p += strlen(p) + 1;
    TRACE(" CAPTION '%s'\n", result->caption );

    /* Get the font name */

    if (result->style & DS_SETFONT)
    {
	result->pointSize = GET_WORD(p);
        p += sizeof(WORD);
	result->faceName = p;
        p += strlen(p) + 1;
	TRACE(" FONT %d,'%s'\n",
                        result->pointSize, result->faceName );
    }
    return p;
}


/***********************************************************************
 *           DIALOG_ParseTemplate32
 *
 * Fill a DLG_TEMPLATE structure from the dialog template, and return
 * a pointer to the first control.
 */
static LPCSTR DIALOG_ParseTemplate32( LPCSTR template, DLG_TEMPLATE * result )
{
    const WORD *p = (const WORD *)template;

    result->style = GET_DWORD(p); p += 2;
    if (result->style == 0xffff0001)  /* DIALOGEX resource */
    {
        result->dialogEx = TRUE;
        result->helpId   = GET_DWORD(p); p += 2;
        result->exStyle  = GET_DWORD(p); p += 2;
        result->style    = GET_DWORD(p); p += 2;
    }
    else
    {
        result->dialogEx = FALSE;
        result->helpId   = 0;
        result->exStyle  = GET_DWORD(p); p += 2;
    }
    result->nbItems = GET_WORD(p); p++;
    result->x       = GET_WORD(p); p++;
    result->y       = GET_WORD(p); p++;
    result->cx      = GET_WORD(p); p++;
    result->cy      = GET_WORD(p); p++;
    TRACE("DIALOG%s %d, %d, %d, %d, %ld\n",
           result->dialogEx ? "EX" : "", result->x, result->y,
           result->cx, result->cy, result->helpId );
    TRACE(" STYLE 0x%08lx\n", result->style );
    TRACE(" EXSTYLE 0x%08lx\n", result->exStyle );

    /* Get the menu name */

    switch(GET_WORD(p))
    {
    case 0x0000:
        result->menuName = NULL;
        p++;
        break;
    case 0xffff:
        result->menuName = (LPCSTR)(UINT)GET_WORD( p + 1 );
        p += 2;
	TRACE(" MENU %04x\n", LOWORD(result->menuName) );
        break;
    default:
        result->menuName = (LPCSTR)p;
        TRACE(" MENU %s\n", debugstr_w( (LPCWSTR)p ));
        p += strlenW( (LPCWSTR)p ) + 1;
        break;
    }

    /* Get the class name */

    switch(GET_WORD(p))
    {
    case 0x0000:
        result->className = DIALOG_CLASS_ATOM;
        p++;
        break;
    case 0xffff:
        result->className = (LPCSTR)(UINT)GET_WORD( p + 1 );
        p += 2;
	TRACE(" CLASS %04x\n", LOWORD(result->className) );
        break;
    default:
        result->className = (LPCSTR)p;
        TRACE(" CLASS %s\n", debugstr_w( (LPCWSTR)p ));
        p += strlenW( (LPCWSTR)p ) + 1;
        break;
    }

    /* Get the window caption */

    result->caption = (LPCSTR)p;
    p += strlenW( (LPCWSTR)p ) + 1;
    TRACE(" CAPTION %s\n", debugstr_w( (LPCWSTR)result->caption ) );

    /* Get the font name */

    if (result->style & DS_SETFONT)
    {
	result->pointSize = GET_WORD(p);
        p++;
        if (result->dialogEx)
        {
            result->weight = GET_WORD(p); p++;
            result->italic = LOBYTE(GET_WORD(p)); p++;
        }
        else
        {
            result->weight = FW_DONTCARE;
            result->italic = FALSE;
        }
	result->faceName = (LPCSTR)p;
        p += strlenW( (LPCWSTR)p ) + 1;
	TRACE(" FONT %d, %s, %d, %s\n",
              result->pointSize, debugstr_w( (LPCWSTR)result->faceName ),
              result->weight, result->italic ? "TRUE" : "FALSE" );
    }

    /* First control is on dword boundary */
    return (LPCSTR)((((int)p) + 3) & ~3);
}


/***********************************************************************
 *           DIALOG_CreateIndirect
 *       Creates a dialog box window
 *
 *       modal = TRUE if we are called from a modal dialog box.
 *       (it's more compatible to do it here, as under Windows the owner
 *       is never disabled if the dialog fails because of an invalid template)
 */
static HWND DIALOG_CreateIndirect( HINSTANCE hInst, LPCSTR dlgTemplate,
                                   HWND owner, DLGPROC dlgProc, LPARAM param,
                                   WINDOWPROCTYPE procType, BOOL modal )
{
    HWND hwnd;
    RECT rect;
    WND * wndPtr;
    DLG_TEMPLATE template;
    DIALOGINFO * dlgInfo;
    BOOL ownerEnabled = TRUE;
    BOOL win32Template = (procType != WIN_PROC_16);

      /* Parse dialog template */

    if (!dlgTemplate) return 0;

    memset (&template, 0, sizeof (template));
    if (win32Template)
        dlgTemplate = DIALOG_ParseTemplate32( dlgTemplate, &template );
    else
        dlgTemplate = DIALOG_ParseTemplate16( dlgTemplate, &template );

      /* Initialise dialog extra data */

    if (!(dlgInfo = HeapAlloc( GetProcessHeap(), 0, sizeof(*dlgInfo) ))) return 0;
    dlgInfo->hwndFocus   = 0;
    dlgInfo->hUserFont   = 0;
    dlgInfo->hMenu       = 0;
    dlgInfo->xBaseUnit   = xBaseUnit;
    dlgInfo->yBaseUnit   = yBaseUnit;
    dlgInfo->idResult    = 0;
    dlgInfo->flags       = 0;
    dlgInfo->hDialogHeap = 0;

      /* Load menu */

    if (template.menuName)
    {
        if (!win32Template) dlgInfo->hMenu = LoadMenu16( hInst, template.menuName );
        else dlgInfo->hMenu = LoadMenuW( hInst, (LPCWSTR)template.menuName );
    }

      /* Create custom font if needed */

    if (template.style & DS_SETFONT)
    {
	  /* The font height must be negative as it is a point size */
	  /* and must be converted to pixels first */
          /* (see CreateFont() documentation in the Windows SDK).   */
        HDC dc;
        int pixels;
        if (((short)template.pointSize) < 0)
            pixels = -((short)template.pointSize);
        else
        {
            dc = GetDC(0);
            pixels = MulDiv( template.pointSize, GetDeviceCaps(dc , LOGPIXELSY), 72 );
            ReleaseDC(0, dc);
        }
	if (win32Template)
            dlgInfo->hUserFont = CreateFontW( -pixels, 0, 0, 0, template.weight,
                                              template.italic, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                                              PROOF_QUALITY, FF_DONTCARE,
                                              (LPCWSTR)template.faceName );
	else
            dlgInfo->hUserFont = CreateFontA( -pixels, 0, 0, 0, FW_DONTCARE,
                                              FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                                              PROOF_QUALITY, FF_DONTCARE, template.faceName );
        if (dlgInfo->hUserFont)
        {
            SIZE charSize;
            if (DIALOG_GetCharSize( dlgInfo->hUserFont, &charSize ))
            {
                dlgInfo->xBaseUnit = charSize.cx;
                dlgInfo->yBaseUnit = charSize.cy;
            }
        }
        TRACE("units = %d,%d\n", dlgInfo->xBaseUnit, dlgInfo->yBaseUnit );
    }

    /* Create dialog main window */
    rect.left = rect.top = 0;

    /* Convert from dialog template units to pixels */
    rect.right = MulDiv(template.cx, dlgInfo->xBaseUnit, 4);
    rect.bottom =  MulDiv(template.cy, dlgInfo->yBaseUnit, 8);

    if (template.style & DS_MODALFRAME)
        template.exStyle |= WS_EX_DLGMODALFRAME;
    AdjustWindowRectEx( &rect, template.style, (dlgInfo->hMenu != 0), template.exStyle );
    rect.right -= rect.left;
    rect.bottom -= rect.top;

    if ((INT16)template.x == CW_USEDEFAULT16)
    {
        rect.left = rect.top = win32Template? CW_USEDEFAULT : CW_USEDEFAULT16;
    }
    else
    {
        if (template.style & DS_CENTER)
        {
            rect.left = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
            rect.top = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom) / 2;
        }
        else
        {
            rect.left += MulDiv(template.x, dlgInfo->xBaseUnit, 4);
            rect.top += MulDiv(template.y, dlgInfo->yBaseUnit, 8);
        }
        if ( !(template.style & WS_CHILD) )
	{
            INT16 dX, dY;

            if( !(template.style & DS_ABSALIGN) )
                ClientToScreen( owner, (POINT *)&rect );

            /* try to fit it into the desktop */

            if( (dX = rect.left + rect.right + GetSystemMetrics(SM_CXDLGFRAME)
                 - GetSystemMetrics(SM_CXSCREEN)) > 0 ) rect.left -= dX;
            if( (dY = rect.top + rect.bottom + GetSystemMetrics(SM_CYDLGFRAME)
                 - GetSystemMetrics(SM_CYSCREEN)) > 0 ) rect.top -= dY;
            if( rect.left < 0 ) rect.left = 0;
            if( rect.top < 0 ) rect.top = 0;
        }
    }

    if (modal)
    {
        ownerEnabled = DIALOG_DisableOwner( owner );
        if (ownerEnabled) dlgInfo->flags |= DF_OWNERENABLED;
    }

    if (!win32Template)
        hwnd = WIN_Handle32( CreateWindowEx16(template.exStyle, template.className,
                                              template.caption, template.style & ~WS_VISIBLE,
                                              rect.left, rect.top, rect.right, rect.bottom,
                                              WIN_Handle16(owner), dlgInfo->hMenu, hInst, NULL ));
    else
        hwnd = CreateWindowExW(template.exStyle, (LPCWSTR)template.className,
                                 (LPCWSTR)template.caption,
                                 template.style & ~WS_VISIBLE,
                                 rect.left, rect.top, rect.right, rect.bottom,
                                 owner, dlgInfo->hMenu, hInst, NULL );

    if (!hwnd)
    {
        if (dlgInfo->hUserFont) DeleteObject( dlgInfo->hUserFont );
        if (dlgInfo->hMenu) DestroyMenu( dlgInfo->hMenu );
        if (modal && (dlgInfo->flags & DF_OWNERENABLED)) DIALOG_EnableOwner(owner);
        HeapFree( GetProcessHeap(), 0, dlgInfo );
	return 0;
    }
    wndPtr = WIN_GetPtr( hwnd );
    wndPtr->flags |= WIN_ISDIALOG;
    WIN_ReleasePtr( wndPtr );

    if (template.helpId) SetWindowContextHelpId( hwnd, template.helpId );
    SetWindowLongW( hwnd, DWL_WINE_DIALOGINFO, (LONG)dlgInfo );
    switch(procType)
    {
    case WIN_PROC_16: SetWindowLong16( WIN_Handle16(hwnd), DWL_DLGPROC, (LONG)dlgProc ); break;
    case WIN_PROC_32A: SetWindowLongA( hwnd, DWL_DLGPROC, (LONG)dlgProc ); break;
    case WIN_PROC_32W: SetWindowLongW( hwnd, DWL_DLGPROC, (LONG)dlgProc ); break;
    default: break;
    }

    if (dlgInfo->hUserFont)
        SendMessageA( hwnd, WM_SETFONT, (WPARAM)dlgInfo->hUserFont, 0 );

    /* Create controls */

    if (DIALOG_CreateControls( hwnd, dlgTemplate, &template,
                               hInst, win32Template ))
    {
        HWND hwndPreInitFocus;

        /* Send initialisation messages and set focus */

	dlgInfo->hwndFocus = GetNextDlgTabItem( hwnd, 0, FALSE );

	hwndPreInitFocus = GetFocus();
	if (SendMessageA( hwnd, WM_INITDIALOG, (WPARAM)dlgInfo->hwndFocus, param ))
        {
            /* check where the focus is again,
	     * some controls status might have changed in WM_INITDIALOG */
            dlgInfo->hwndFocus = GetNextDlgTabItem( hwnd, 0, FALSE);
            if( dlgInfo->hwndFocus )
                SetFocus( dlgInfo->hwndFocus );
        }
        else
        {
            /* If the dlgproc has returned FALSE (indicating handling of keyboard focus)
               but the focus has not changed, set the focus where we expect it. */
            if ((GetFocus() == hwndPreInitFocus) &&
                (GetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE))
            {
                dlgInfo->hwndFocus = GetNextDlgTabItem( hwnd, 0, FALSE);
                if( dlgInfo->hwndFocus )
                    SetFocus( dlgInfo->hwndFocus );
            }
        }

	if (template.style & WS_VISIBLE && !(GetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE))
	{
	   ShowWindow( hwnd, SW_SHOWNORMAL );	/* SW_SHOW doesn't always work */
	}
	return hwnd;
    }
    if( IsWindow(hwnd) ) DestroyWindow( hwnd );
    if (modal && ownerEnabled) DIALOG_EnableOwner(owner);
    return 0;
}


/***********************************************************************
 *		CreateDialog (USER.89)
 */
HWND16 WINAPI CreateDialog16( HINSTANCE16 hInst, LPCSTR dlgTemplate,
                              HWND16 owner, DLGPROC16 dlgProc )
{
    return CreateDialogParam16( hInst, dlgTemplate, owner, dlgProc, 0 );
}


/***********************************************************************
 *		CreateDialogParam (USER.241)
 */
HWND16 WINAPI CreateDialogParam16( HINSTANCE16 hInst, LPCSTR dlgTemplate,
                                   HWND16 owner, DLGPROC16 dlgProc,
                                   LPARAM param )
{
    HWND16 hwnd = 0;
    HRSRC16 hRsrc;
    HGLOBAL16 hmem;
    LPCVOID data;

    TRACE("%04x,%s,%04x,%08lx,%ld\n",
          hInst, debugres_a(dlgTemplate), owner, (DWORD)dlgProc, param );

    if (!(hRsrc = FindResource16( hInst, dlgTemplate, RT_DIALOGA ))) return 0;
    if (!(hmem = LoadResource16( hInst, hRsrc ))) return 0;
    if (!(data = LockResource16( hmem ))) hwnd = 0;
    else hwnd = CreateDialogIndirectParam16( hInst, data, owner,
                                             dlgProc, param );
    FreeResource16( hmem );
    return hwnd;
}

/***********************************************************************
 *		CreateDialogParamA (USER32.@)
 */
HWND WINAPI CreateDialogParamA( HINSTANCE hInst, LPCSTR name,
                                    HWND owner, DLGPROC dlgProc,
                                    LPARAM param )
{
    HANDLE hrsrc = FindResourceA( hInst, name, RT_DIALOGA );
    if (!hrsrc) return 0;
    return CreateDialogIndirectParamA( hInst,
                                         (LPVOID)LoadResource(hInst, hrsrc),
                                         owner, dlgProc, param );
}


/***********************************************************************
 *		CreateDialogParamW (USER32.@)
 */
HWND WINAPI CreateDialogParamW( HINSTANCE hInst, LPCWSTR name,
                                    HWND owner, DLGPROC dlgProc,
                                    LPARAM param )
{
    HANDLE hrsrc = FindResourceW( hInst, name, RT_DIALOGW );
    if (!hrsrc) return 0;
    return CreateDialogIndirectParamW( hInst,
                                         (LPVOID)LoadResource(hInst, hrsrc),
                                         owner, dlgProc, param );
}


/***********************************************************************
 *		CreateDialogIndirect (USER.219)
 */
HWND16 WINAPI CreateDialogIndirect16( HINSTANCE16 hInst, LPCVOID dlgTemplate,
                                      HWND16 owner, DLGPROC16 dlgProc )
{
    return CreateDialogIndirectParam16( hInst, dlgTemplate, owner, dlgProc, 0);
}


/***********************************************************************
 *		CreateDialogIndirectParam (USER.242)
 *		CreateDialogIndirectParam16 (USER32.@)
 */
HWND16 WINAPI CreateDialogIndirectParam16( HINSTANCE16 hInst,
                                           LPCVOID dlgTemplate,
                                           HWND16 owner, DLGPROC16 dlgProc,
                                           LPARAM param )
{
    return WIN_Handle16( DIALOG_CreateIndirect( hInst, dlgTemplate, WIN_Handle32(owner),
                                                (DLGPROC)dlgProc, param, WIN_PROC_16, FALSE ));
}


/***********************************************************************
 *		CreateDialogIndirectParamA (USER32.@)
 */
HWND WINAPI CreateDialogIndirectParamA( HINSTANCE hInst,
                                            LPCVOID dlgTemplate,
                                            HWND owner, DLGPROC dlgProc,
                                            LPARAM param )
{
    return DIALOG_CreateIndirect( hInst, dlgTemplate, owner, dlgProc, param, WIN_PROC_32A, FALSE );
}

/***********************************************************************
 *		CreateDialogIndirectParamAorW (USER32.@)
 */
HWND WINAPI CreateDialogIndirectParamAorW( HINSTANCE hInst,
                                            LPCVOID dlgTemplate,
                                            HWND owner, DLGPROC dlgProc,
                                            LPARAM param )
{   FIXME("assume WIN_PROC_32W\n");
    return DIALOG_CreateIndirect( hInst, dlgTemplate, owner, dlgProc, param, WIN_PROC_32W, FALSE );
}

/***********************************************************************
 *		CreateDialogIndirectParamW (USER32.@)
 */
HWND WINAPI CreateDialogIndirectParamW( HINSTANCE hInst,
                                            LPCVOID dlgTemplate,
                                            HWND owner, DLGPROC dlgProc,
                                            LPARAM param )
{
    return DIALOG_CreateIndirect( hInst, dlgTemplate, owner, dlgProc, param, WIN_PROC_32W, FALSE );
}


/***********************************************************************
 *           DIALOG_DoDialogBox
 */
static INT DIALOG_DoDialogBox( HWND hwnd, HWND owner )
{
    DIALOGINFO * dlgInfo;
    MSG msg;
    INT retval;
    HWND ownerMsg = GetAncestor( owner, GA_ROOT );

    if (!(dlgInfo = DIALOG_get_info( hwnd ))) return -1;

    if (!(dlgInfo->flags & DF_END)) /* was EndDialog called in WM_INITDIALOG ? */
    {
        ShowWindow( hwnd, SW_SHOW );
        for (;;)
        {
            if (!(GetWindowLongW( hwnd, GWL_STYLE ) & DS_NOIDLEMSG))
            {
                if (!PeekMessageW( &msg, 0, 0, 0, PM_REMOVE ))
                {
                    /* No message present -> send ENTERIDLE and wait */
                    SendMessageW( ownerMsg, WM_ENTERIDLE, MSGF_DIALOGBOX, (LPARAM)hwnd );
                    if (!GetMessageW( &msg, 0, 0, 0 )) break;
                }
            }
            else if (!GetMessageW( &msg, 0, 0, 0 )) break;

            if (CallMsgFilterW( &msg, MSGF_DIALOGBOX )) continue;

            if (!IsWindow( hwnd )) return -1;
            if (!(dlgInfo->flags & DF_END) && !IsDialogMessageW( hwnd, &msg))
            {
                TranslateMessage( &msg );
                DispatchMessageW( &msg );
            }
            if (dlgInfo->flags & DF_END) break;
        }
    }
    if (dlgInfo->flags & DF_OWNERENABLED) DIALOG_EnableOwner( owner );
    retval = dlgInfo->idResult;
    DestroyWindow( hwnd );
    return retval;
}


/***********************************************************************
 *		DialogBox (USER.87)
 */
INT16 WINAPI DialogBox16( HINSTANCE16 hInst, LPCSTR dlgTemplate,
                          HWND16 owner, DLGPROC16 dlgProc )
{
    return DialogBoxParam16( hInst, dlgTemplate, owner, dlgProc, 0 );
}


/***********************************************************************
 *		DialogBoxParam (USER.239)
 */
INT16 WINAPI DialogBoxParam16( HINSTANCE16 hInst, LPCSTR template,
                               HWND16 owner16, DLGPROC16 dlgProc, LPARAM param )
{
    HWND hwnd = 0;
    HRSRC16 hRsrc;
    HGLOBAL16 hmem;
    LPCVOID data;
    int ret = -1;

    if (!(hRsrc = FindResource16( hInst, template, RT_DIALOGA ))) return 0;
    if (!(hmem = LoadResource16( hInst, hRsrc ))) return 0;
    if ((data = LockResource16( hmem )))
    {
        HWND owner = WIN_Handle32(owner16);
        hwnd = DIALOG_CreateIndirect( hInst, data, owner,
                                      (DLGPROC)dlgProc, param, WIN_PROC_16, TRUE );
        if (hwnd) ret = DIALOG_DoDialogBox( hwnd, owner );
        GlobalUnlock16( hmem );
    }
    FreeResource16( hmem );
    return ret;
}


/***********************************************************************
 *		DialogBoxParamA (USER32.@)
 */
INT WINAPI DialogBoxParamA( HINSTANCE hInst, LPCSTR name,
                                HWND owner, DLGPROC dlgProc, LPARAM param )
{
    HWND hwnd;
    HANDLE hrsrc = FindResourceA( hInst, name, RT_DIALOGA );
    if (!hrsrc) return 0;
    hwnd = DIALOG_CreateIndirect( hInst, (LPVOID)LoadResource(hInst, hrsrc),
                                  owner, dlgProc, param, WIN_PROC_32A, TRUE );
    if (hwnd) return DIALOG_DoDialogBox( hwnd, owner );
    return -1;
}


/***********************************************************************
 *		DialogBoxParamW (USER32.@)
 */
INT WINAPI DialogBoxParamW( HINSTANCE hInst, LPCWSTR name,
                                HWND owner, DLGPROC dlgProc, LPARAM param )
{
    HWND hwnd;
    HANDLE hrsrc = FindResourceW( hInst, name, RT_DIALOGW );
    if (!hrsrc) return 0;
    hwnd = DIALOG_CreateIndirect( hInst, (LPVOID)LoadResource(hInst, hrsrc),
                                  owner, dlgProc, param, WIN_PROC_32W, TRUE );
    if (hwnd) return DIALOG_DoDialogBox( hwnd, owner );
    return -1;
}


/***********************************************************************
 *		DialogBoxIndirect (USER.218)
 */
INT16 WINAPI DialogBoxIndirect16( HINSTANCE16 hInst, HANDLE16 dlgTemplate,
                                  HWND16 owner, DLGPROC16 dlgProc )
{
    return DialogBoxIndirectParam16( hInst, dlgTemplate, owner, dlgProc, 0 );
}


/***********************************************************************
 *		DialogBoxIndirectParam (USER.240)
 *		DialogBoxIndirectParam16 (USER32.@)
 */
INT16 WINAPI DialogBoxIndirectParam16( HINSTANCE16 hInst, HANDLE16 dlgTemplate,
                                       HWND16 owner16, DLGPROC16 dlgProc,
                                       LPARAM param )
{
    HWND hwnd, owner = WIN_Handle32( owner16 );
    LPCVOID ptr;

    if (!(ptr = GlobalLock16( dlgTemplate ))) return -1;
    hwnd = DIALOG_CreateIndirect( hInst, ptr, owner, (DLGPROC)dlgProc,
                                  param, WIN_PROC_16, TRUE );
    GlobalUnlock16( dlgTemplate );
    if (hwnd) return DIALOG_DoDialogBox( hwnd, owner );
    return -1;
}


/***********************************************************************
 *		DialogBoxIndirectParamA (USER32.@)
 */
INT WINAPI DialogBoxIndirectParamA(HINSTANCE hInstance, LPCVOID template,
                                       HWND owner, DLGPROC dlgProc,
                                       LPARAM param )
{
    HWND hwnd = DIALOG_CreateIndirect( hInstance, template, owner,
                                       dlgProc, param, WIN_PROC_32A, TRUE );
    if (hwnd) return DIALOG_DoDialogBox( hwnd, owner );
    return -1;
}


/***********************************************************************
 *		DialogBoxIndirectParamW (USER32.@)
 */
INT WINAPI DialogBoxIndirectParamW(HINSTANCE hInstance, LPCVOID template,
                                       HWND owner, DLGPROC dlgProc,
                                       LPARAM param )
{
    HWND hwnd = DIALOG_CreateIndirect( hInstance, template, owner,
                                       dlgProc, param, WIN_PROC_32W, TRUE );
    if (hwnd) return DIALOG_DoDialogBox( hwnd, owner );
    return -1;
}

/***********************************************************************
 *		DialogBoxIndirectParamAorW (USER32.@)
 */
INT WINAPI DialogBoxIndirectParamAorW(HINSTANCE hInstance, LPCVOID template,
                                       HWND owner, DLGPROC dlgProc,
                                       LPARAM param, DWORD x )
{
    HWND hwnd;
    FIXME("0x%08x %p 0x%08x %p 0x%08lx 0x%08lx\n",
      hInstance, template, owner, dlgProc, param, x);
    hwnd = DIALOG_CreateIndirect( hInstance, template, owner, dlgProc, param, WIN_PROC_32W, TRUE );
    if (hwnd) return DIALOG_DoDialogBox( hwnd, owner );
    return -1;
}

/***********************************************************************
 *		EndDialog (USER32.@)
 */
BOOL WINAPI EndDialog( HWND hwnd, INT retval )
{
    BOOL wasEnabled = TRUE;
    DIALOGINFO * dlgInfo;
    HWND owner;

    TRACE("%04x %d\n", hwnd, retval );

    if (!(dlgInfo = DIALOG_get_info( hwnd )))
    {
        ERR("got invalid window handle (%04x); buggy app !?\n", hwnd);
        return FALSE;
    }
    dlgInfo->idResult = retval;
    dlgInfo->flags |= DF_END;
    wasEnabled = (dlgInfo->flags & DF_OWNERENABLED);

    if (wasEnabled && (owner = GetWindow( hwnd, GW_OWNER )))
        DIALOG_EnableOwner( owner );

    /* Windows sets the focus to the dialog itself in EndDialog */

    if (IsChild(hwnd, GetFocus()))
       SetFocus( hwnd );

    /* Don't have to send a ShowWindow(SW_HIDE), just do
       SetWindowPos with SWP_HIDEWINDOW as done in Windows */

    SetWindowPos(hwnd, (HWND)0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE
                 | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);

    /* unblock dialog loop */
    PostMessageA(hwnd, WM_NULL, 0, 0);
    return TRUE;
}


/***********************************************************************
 *           DIALOG_IsAccelerator
 */
static BOOL DIALOG_IsAccelerator( HWND hwnd, HWND hwndDlg, WPARAM vKey )
{
    HWND hwndControl = hwnd;
    HWND hwndNext;
    INT dlgCode;
    WCHAR buffer[128];

    do
    {
        DWORD style = GetWindowLongW( hwndControl, GWL_STYLE );
        if ((style & (WS_VISIBLE | WS_DISABLED)) == WS_VISIBLE)
        {
            dlgCode = SendMessageA( hwndControl, WM_GETDLGCODE, 0, 0 );
            if ( (dlgCode & (DLGC_BUTTON | DLGC_STATIC)) &&
                 GetWindowTextW( hwndControl, buffer, sizeof(buffer)/sizeof(WCHAR) ))
            {
                /* find the accelerator key */
                LPWSTR p = buffer - 2;
                char a_char = vKey;
                WCHAR w_char = 0;

                do
                {
                    p = strchrW( p + 2, '&' );
                }
                while (p != NULL && p[1] == '&');

                /* and check if it's the one we're looking for */
                MultiByteToWideChar(CP_ACP, 0, &a_char, 1, &w_char, 1);
                if (p != NULL && toupperW( p[1] ) == toupperW( w_char ) )
                {
                    if ((dlgCode & DLGC_STATIC) || (style & 0x0f) == BS_GROUPBOX )
                    {
                        /* set focus to the control */
                        SendMessageA( hwndDlg, WM_NEXTDLGCTL, (WPARAM)hwndControl, 1);
                        /* and bump it on to next */
                        SendMessageA( hwndDlg, WM_NEXTDLGCTL, 0, 0);
                    }
                    else if (dlgCode & DLGC_BUTTON)
                    {
                        /* send BM_CLICK message to the control */
                        SendMessageA( hwndControl, BM_CLICK, 0, 0 );
                    }
                    return TRUE;
                }
            }
            hwndNext = GetWindow( hwndControl, GW_CHILD );
        }
        else hwndNext = 0;

        if (!hwndNext) hwndNext = GetWindow( hwndControl, GW_HWNDNEXT );

        while (!hwndNext && hwndControl)
        {
            hwndControl = GetParent( hwndControl );
            if (hwndControl == hwndDlg)
            {
                if(hwnd==hwndDlg)   /* prevent endless loop */
                {
                    hwndNext=hwnd;
                    break;
                }
                hwndNext = GetWindow( hwndDlg, GW_CHILD );
            }
            else
                hwndNext = GetWindow( hwndControl, GW_HWNDNEXT );
        }
        hwndControl = hwndNext;
    }
    while (hwndControl && (hwndControl != hwnd));

    return FALSE;
}

/***********************************************************************
 *           DIALOG_FindMsgDestination
 *
 * The messages that IsDialogMessage sends may not go to the dialog
 * calling IsDialogMessage if that dialog is a child, and it has the
 * DS_CONTROL style set.
 * We propagate up until we hit one that does not have DS_CONTROL, or
 * whose parent is not a dialog.
 *
 * This is undocumented behaviour.
 */
static HWND DIALOG_FindMsgDestination( HWND hwndDlg )
{
    while (GetWindowLongA(hwndDlg, GWL_STYLE) & DS_CONTROL)
    {
	WND *pParent;
	HWND hParent = GetParent(hwndDlg);
	if (!hParent) break;

	pParent = WIN_FindWndPtr(hParent);
	if (!pParent) break;

	if (!(pParent->flags & WIN_ISDIALOG))
	{
	    WIN_ReleaseWndPtr(pParent);
	    break;
	}
	WIN_ReleaseWndPtr(pParent);

	hwndDlg = hParent;
    }

    return hwndDlg;
}

/***********************************************************************
 *           DIALOG_IsDialogMessage
 */
static BOOL DIALOG_IsDialogMessage( HWND hwnd, HWND hwndDlg,
                                      UINT message, WPARAM wParam,
                                      LPARAM lParam, BOOL *translate,
                                      BOOL *dispatch, INT dlgCode )
{
    *translate = *dispatch = FALSE;

    if (message == WM_PAINT)
    {
        /* Apparently, we have to handle this one as well */
        *dispatch = TRUE;
        return TRUE;
    }

      /* Only the key messages get special processing */
    if ((message != WM_KEYDOWN) &&
        (message != WM_SYSKEYDOWN) &&
        (message != WM_SYSCHAR) &&
	(message != WM_CHAR))
        return FALSE;

    if (dlgCode & DLGC_WANTMESSAGE)
    {
        *translate = *dispatch = TRUE;
        return TRUE;
    }

    hwndDlg = DIALOG_FindMsgDestination(hwndDlg);

    switch(message)
    {
    case WM_KEYDOWN:
        switch(wParam)
        {
        case VK_TAB:
            if (!(dlgCode & DLGC_WANTTAB))
            {
                SendMessageA( hwndDlg, WM_NEXTDLGCTL,
                                (GetKeyState(VK_SHIFT) & 0x8000), 0 );
                return TRUE;
            }
            break;

        case VK_RIGHT:
        case VK_DOWN:
        case VK_LEFT:
        case VK_UP:
            if (!(dlgCode & DLGC_WANTARROWS))
            {
                BOOL fPrevious = (wParam == VK_LEFT || wParam == VK_UP);
                HWND hwndNext =
                    GetNextDlgGroupItem (hwndDlg, GetFocus(), fPrevious );
                SendMessageA( hwndDlg, WM_NEXTDLGCTL, (WPARAM)hwndNext, 1 );
                return TRUE;
            }
            break;

        case VK_ESCAPE:
            SendMessageA( hwndDlg, WM_COMMAND, IDCANCEL,
                            (LPARAM)GetDlgItem( hwndDlg, IDCANCEL ) );
            return TRUE;

        case VK_RETURN:
            {
                DWORD dw = SendMessageW( hwndDlg, DM_GETDEFID, 0, 0 );
                if (HIWORD(dw) == DC_HASDEFID)
                {
                    SendMessageA( hwndDlg, WM_COMMAND,
                                    MAKEWPARAM( LOWORD(dw), BN_CLICKED ),
                                    (LPARAM)GetDlgItem(hwndDlg, LOWORD(dw)));
                }
                else
                {
                    SendMessageA( hwndDlg, WM_COMMAND, IDOK,
                                    (LPARAM)GetDlgItem( hwndDlg, IDOK ) );

                }
            }
            return TRUE;
        }
        *translate = TRUE;
        break; /* case WM_KEYDOWN */

    case WM_CHAR:
        if (dlgCode & DLGC_WANTCHARS) break;
        /* drop through */

    case WM_SYSCHAR:
        if (DIALOG_IsAccelerator( WIN_GetFullHandle(hwnd), hwndDlg, wParam ))
        {
            /* don't translate or dispatch */
            return TRUE;
        }
        break;

    case WM_SYSKEYDOWN:
        *translate = TRUE;
        break;
    }

    /* If we get here, the message has not been treated specially */
    /* and can be sent to its destination window. */
    *dispatch = TRUE;
    return TRUE;
}


/***********************************************************************
 *		IsDialogMessage (USER.90)
 */
BOOL16 WINAPI IsDialogMessage16( HWND16 hwndDlg, SEGPTR msg16 )
{
    LPMSG16 msg = MapSL(msg16);
    BOOL ret, translate, dispatch;
    INT dlgCode = 0;

    if ((hwndDlg != msg->hwnd) && !IsChild16( hwndDlg, msg->hwnd ))
        return FALSE;

    if ((msg->message == WM_KEYDOWN) ||
        (msg->message == WM_CHAR))
    {
       dlgCode = SendMessage16( msg->hwnd, WM_GETDLGCODE, 0, (LPARAM)msg16);
    }
    ret = DIALOG_IsDialogMessage( WIN_Handle32(msg->hwnd), WIN_Handle32(hwndDlg),
                                  msg->message, msg->wParam, msg->lParam,
                                  &translate, &dispatch, dlgCode );
    if (translate) TranslateMessage16( msg );
    if (dispatch) DispatchMessage16( msg );
    return ret;
}


/***********************************************************************
 *		IsDialogMessage  (USER32.@)
 *		IsDialogMessageA (USER32.@)
 */
BOOL WINAPI IsDialogMessageA( HWND hwndDlg, LPMSG msg )
{
    BOOL ret, translate, dispatch;
    INT dlgCode = 0;

    hwndDlg = WIN_GetFullHandle( hwndDlg );
    if ((hwndDlg != msg->hwnd) && !IsChild( hwndDlg, msg->hwnd ))
        return FALSE;

    if ((msg->message == WM_KEYDOWN) ||
        (msg->message == WM_CHAR))
    {
        dlgCode = SendMessageA( msg->hwnd, WM_GETDLGCODE, 0, (LPARAM)msg);
    }
    ret = DIALOG_IsDialogMessage( msg->hwnd, hwndDlg, msg->message,
                                  msg->wParam, msg->lParam,
                                  &translate, &dispatch, dlgCode );
    if (translate) TranslateMessage( msg );
    if (dispatch) DispatchMessageA( msg );
    return ret;
}


/***********************************************************************
 *		IsDialogMessageW (USER32.@)
 */
BOOL WINAPI IsDialogMessageW( HWND hwndDlg, LPMSG msg )
{
    BOOL ret, translate, dispatch;
    INT dlgCode = 0;

    hwndDlg = WIN_GetFullHandle( hwndDlg );
    if ((hwndDlg != msg->hwnd) && !IsChild( hwndDlg, msg->hwnd ))
        return FALSE;

    if ((msg->message == WM_KEYDOWN) ||
        (msg->message == WM_CHAR))
    {
        dlgCode = SendMessageW( msg->hwnd, WM_GETDLGCODE, 0, (LPARAM)msg);
    }
    ret = DIALOG_IsDialogMessage( msg->hwnd, hwndDlg, msg->message,
                                  msg->wParam, msg->lParam,
                                  &translate, &dispatch, dlgCode );
    if (translate) TranslateMessage( msg );
    if (dispatch) DispatchMessageW( msg );
    return ret;
}


/***********************************************************************
 *		GetDlgCtrlID (USER32.@)
 */
INT WINAPI GetDlgCtrlID( HWND hwnd )
{
    return GetWindowLongW( hwnd, GWL_ID );
}


/***********************************************************************
 *		GetDlgItem (USER32.@)
 */
HWND WINAPI GetDlgItem( HWND hwndDlg, INT id )
{
    int i;
    HWND *list = WIN_ListChildren( hwndDlg );
    HWND ret = 0;

    if (!list) return 0;

    for (i = 0; list[i]; i++) if (GetWindowLongW( list[i], GWL_ID ) == id) break;
    ret = list[i];
    HeapFree( GetProcessHeap(), 0, list );
    return ret;
}


/*******************************************************************
 *		SendDlgItemMessageA (USER32.@)
 */
LRESULT WINAPI SendDlgItemMessageA( HWND hwnd, INT id, UINT msg,
                                      WPARAM wParam, LPARAM lParam )
{
    HWND hwndCtrl = GetDlgItem( hwnd, id );
    if (hwndCtrl) return SendMessageA( hwndCtrl, msg, wParam, lParam );
    else return 0;
}


/*******************************************************************
 *		SendDlgItemMessageW (USER32.@)
 */
LRESULT WINAPI SendDlgItemMessageW( HWND hwnd, INT id, UINT msg,
                                      WPARAM wParam, LPARAM lParam )
{
    HWND hwndCtrl = GetDlgItem( hwnd, id );
    if (hwndCtrl) return SendMessageW( hwndCtrl, msg, wParam, lParam );
    else return 0;
}


/*******************************************************************
 *		SetDlgItemTextA (USER32.@)
 */
BOOL WINAPI SetDlgItemTextA( HWND hwnd, INT id, LPCSTR lpString )
{
    return SendDlgItemMessageA( hwnd, id, WM_SETTEXT, 0, (LPARAM)lpString );
}


/*******************************************************************
 *		SetDlgItemTextW (USER32.@)
 */
BOOL WINAPI SetDlgItemTextW( HWND hwnd, INT id, LPCWSTR lpString )
{
    return SendDlgItemMessageW( hwnd, id, WM_SETTEXT, 0, (LPARAM)lpString );
}


/***********************************************************************
 *		GetDlgItemTextA (USER32.@)
 */
INT WINAPI GetDlgItemTextA( HWND hwnd, INT id, LPSTR str, UINT len )
{
    return (INT)SendDlgItemMessageA( hwnd, id, WM_GETTEXT,
                                         len, (LPARAM)str );
}


/***********************************************************************
 *		GetDlgItemTextW (USER32.@)
 */
INT WINAPI GetDlgItemTextW( HWND hwnd, INT id, LPWSTR str, UINT len )
{
    return (INT)SendDlgItemMessageW( hwnd, id, WM_GETTEXT,
                                         len, (LPARAM)str );
}


/*******************************************************************
 *		SetDlgItemInt (USER32.@)
 */
BOOL WINAPI SetDlgItemInt( HWND hwnd, INT id, UINT value,
                             BOOL fSigned )
{
    char str[20];

    if (fSigned) sprintf( str, "%d", (INT)value );
    else sprintf( str, "%u", value );
    SendDlgItemMessageA( hwnd, id, WM_SETTEXT, 0, (LPARAM)str );
    return TRUE;
}


/***********************************************************************
 *		GetDlgItemInt (USER32.@)
 */
UINT WINAPI GetDlgItemInt( HWND hwnd, INT id, BOOL *translated,
                               BOOL fSigned )
{
    char str[30];
    char * endptr;
    long result = 0;

    if (translated) *translated = FALSE;
    if (!SendDlgItemMessageA(hwnd, id, WM_GETTEXT, sizeof(str), (LPARAM)str))
        return 0;
    if (fSigned)
    {
        result = strtol( str, &endptr, 10 );
        if (!endptr || (endptr == str))  /* Conversion was unsuccessful */
            return 0;
        if (((result == LONG_MIN) || (result == LONG_MAX)) && (errno==ERANGE))
            return 0;
    }
    else
    {
        result = strtoul( str, &endptr, 10 );
        if (!endptr || (endptr == str))  /* Conversion was unsuccessful */
            return 0;
        if ((result == ULONG_MAX) && (errno == ERANGE)) return 0;
    }
    if (translated) *translated = TRUE;
    return (UINT)result;
}


/***********************************************************************
 *		CheckDlgButton (USER32.@)
 */
BOOL WINAPI CheckDlgButton( HWND hwnd, INT id, UINT check )
{
    SendDlgItemMessageA( hwnd, id, BM_SETCHECK, check, 0 );
    return TRUE;
}


/***********************************************************************
 *		IsDlgButtonChecked (USER32.@)
 */
UINT WINAPI IsDlgButtonChecked( HWND hwnd, UINT id )
{
    return (UINT)SendDlgItemMessageA( hwnd, id, BM_GETCHECK, 0, 0 );
}


/***********************************************************************
 *           CheckRB
 *
 * Callback function used to check/uncheck radio buttons that fall
 * within a specific range of IDs.
 */
static BOOL CALLBACK CheckRB(HWND hwndChild, LPARAM lParam)
{
    LONG lChildID = GetWindowLongA(hwndChild, GWL_ID);
    RADIOGROUP *lpRadioGroup = (RADIOGROUP *) lParam;

    if ((lChildID >= lpRadioGroup->firstID) &&
        (lChildID <= lpRadioGroup->lastID))
    {
        if (lChildID == lpRadioGroup->checkID)
        {
            SendMessageA(hwndChild, BM_SETCHECK, BST_CHECKED, 0);
        }
        else
        {
            SendMessageA(hwndChild, BM_SETCHECK, BST_UNCHECKED, 0);
        }
    }

    return TRUE;
}


/***********************************************************************
 *		CheckRadioButton (USER32.@)
 */
BOOL WINAPI CheckRadioButton( HWND hwndDlg, UINT firstID,
                              UINT lastID, UINT checkID )
{
    RADIOGROUP radioGroup;

    /* perform bounds checking for a radio button group */
    radioGroup.firstID = min(min(firstID, lastID), checkID);
    radioGroup.lastID = max(max(firstID, lastID), checkID);
    radioGroup.checkID = checkID;

    return EnumChildWindows(hwndDlg, (WNDENUMPROC)CheckRB,
                            (LPARAM)&radioGroup);
}


/***********************************************************************
 *		GetDialogBaseUnits (USER.243)
 *		GetDialogBaseUnits (USER32.@)
 */
DWORD WINAPI GetDialogBaseUnits(void)
{
    return MAKELONG( xBaseUnit, yBaseUnit );
}


/***********************************************************************
 *		MapDialogRect (USER32.@)
 */
BOOL WINAPI MapDialogRect( HWND hwnd, LPRECT rect )
{
    DIALOGINFO * dlgInfo;
    if (!(dlgInfo = DIALOG_get_info( hwnd ))) return FALSE;
    rect->left   = MulDiv(rect->left, dlgInfo->xBaseUnit, 4);
    rect->right  = MulDiv(rect->right, dlgInfo->xBaseUnit, 4);
    rect->top    = MulDiv(rect->top, dlgInfo->yBaseUnit, 8);
    rect->bottom = MulDiv(rect->bottom, dlgInfo->yBaseUnit, 8);
    return TRUE;
}


/***********************************************************************
 *		GetNextDlgGroupItem (USER32.@)
 */
HWND WINAPI GetNextDlgGroupItem( HWND hwndDlg, HWND hwndCtrl, BOOL fPrevious )
{
    HWND hwnd, retvalue;

    hwndDlg = WIN_GetFullHandle( hwndDlg );
    hwndCtrl = WIN_GetFullHandle( hwndCtrl );

    if(hwndCtrl)
    {
        /* if the hwndCtrl is the child of the control in the hwndDlg,
	 * then the hwndDlg has to be the parent of the hwndCtrl */
        if(GetParent(hwndCtrl) != hwndDlg && GetParent(GetParent(hwndCtrl)) == hwndDlg)
            hwndDlg = GetParent(hwndCtrl);
    }

    if (hwndCtrl)
    {
        /* Make sure hwndCtrl is a top-level child */
        HWND parent = GetParent( hwndCtrl );
        while (parent && parent != hwndDlg) parent = GetParent(parent);
        if (parent != hwndDlg) return 0;
    }
    else
    {
        /* No ctrl specified -> start from the beginning */
        if (!(hwndCtrl = GetWindow( hwndDlg, GW_CHILD ))) return 0;
        if (fPrevious) hwndCtrl = GetWindow( hwndCtrl, GW_HWNDLAST );
    }

    retvalue = hwndCtrl;
    hwnd = GetWindow( hwndCtrl, GW_HWNDNEXT );
    while (1)
    {
        if (!hwnd || (GetWindowLongW( hwnd, GWL_STYLE ) & WS_GROUP))
        {
            /* Wrap-around to the beginning of the group */
            HWND tmp;

            hwnd = GetWindow( hwndDlg, GW_CHILD );
            for (tmp = hwnd; tmp; tmp = GetWindow( tmp, GW_HWNDNEXT ) )
            {
                if (GetWindowLongW( tmp, GWL_STYLE ) & WS_GROUP) hwnd = tmp;
                if (tmp == hwndCtrl) break;
            }
        }
        if (hwnd == hwndCtrl) break;
        if ((GetWindowLongW( hwnd, GWL_STYLE ) & (WS_VISIBLE|WS_DISABLED)) == WS_VISIBLE)
        {
            retvalue = hwnd;
	    if (!fPrevious) break;
	}
        hwnd = GetWindow( hwnd, GW_HWNDNEXT );
    }
    return retvalue;
}


/***********************************************************************
 *           DIALOG_GetNextTabItem
 *
 * Helper for GetNextDlgTabItem
 */
static HWND DIALOG_GetNextTabItem( HWND hwndMain, HWND hwndDlg, HWND hwndCtrl, BOOL fPrevious )
{
    LONG dsStyle;
    LONG exStyle;
    UINT wndSearch = fPrevious ? GW_HWNDPREV : GW_HWNDNEXT;
    HWND retWnd = 0;
    HWND hChildFirst = 0;

    if(!hwndCtrl)
    {
        hChildFirst = GetWindow(hwndDlg,GW_CHILD);
        if(fPrevious) hChildFirst = GetWindow(hChildFirst,GW_HWNDLAST);
    }
    else if (IsChild( hwndMain, hwndCtrl ))
    {
        hChildFirst = GetWindow(hwndCtrl,wndSearch);
        if(!hChildFirst)
        {
            if(GetParent(hwndCtrl) != hwndMain)
                hChildFirst = GetWindow(GetParent(hwndCtrl),wndSearch);
            else
            {
                if(fPrevious)
                    hChildFirst = GetWindow(hwndCtrl,GW_HWNDLAST);
                else
                    hChildFirst = GetWindow(hwndCtrl,GW_HWNDFIRST);
            }
        }
    }

    while(hChildFirst)
    {
        BOOL bCtrl = FALSE;
        while(hChildFirst)
        {
            dsStyle = GetWindowLongA(hChildFirst,GWL_STYLE);
            exStyle = GetWindowLongA(hChildFirst,GWL_EXSTYLE);
            if( (dsStyle & DS_CONTROL || exStyle & WS_EX_CONTROLPARENT) && (dsStyle & WS_VISIBLE) && !(dsStyle & WS_DISABLED))
            {
                bCtrl=TRUE;
                break;
            }
            else if( (dsStyle & WS_TABSTOP) && (dsStyle & WS_VISIBLE) && !(dsStyle & WS_DISABLED))
                break;
            hChildFirst = GetWindow(hChildFirst,wndSearch);
        }
        if(hChildFirst)
        {
            if(bCtrl)
                retWnd = DIALOG_GetNextTabItem(hwndMain,hChildFirst,(HWND)NULL,fPrevious );
            else
                retWnd = hChildFirst;
        }
        if(retWnd) break;
        hChildFirst = GetWindow(hChildFirst,wndSearch);
    }
    if(!retWnd && hwndCtrl)
    {
        HWND hParent = GetParent(hwndCtrl);
        while(hParent)
	{
            if(hParent == hwndMain) break;
            retWnd = DIALOG_GetNextTabItem(hwndMain,GetParent(hParent),hParent,fPrevious );
            if(retWnd) break;
            hParent = GetParent(hParent);
	}
        if(!retWnd)
            retWnd = DIALOG_GetNextTabItem(hwndMain,hwndMain,(HWND)NULL,fPrevious );
    }
    return retWnd;
}

/***********************************************************************
 *		GetNextDlgTabItem (USER32.@)
 */
HWND WINAPI GetNextDlgTabItem( HWND hwndDlg, HWND hwndCtrl,
                                   BOOL fPrevious )
{
    hwndDlg = WIN_GetFullHandle( hwndDlg );
    hwndCtrl = WIN_GetFullHandle( hwndCtrl );
    return DIALOG_GetNextTabItem(hwndDlg,hwndDlg,hwndCtrl,fPrevious);
}

/**********************************************************************
 *           DIALOG_DlgDirSelect
 *
 * Helper function for DlgDirSelect*
 */
static BOOL DIALOG_DlgDirSelect( HWND hwnd, LPSTR str, INT len,
                                 INT id, BOOL unicode, BOOL combo )
{
    char *buffer, *ptr;
    INT item, size;
    BOOL ret;
    HWND listbox = GetDlgItem( hwnd, id );

    TRACE("%04x '%s' %d\n", hwnd, str, id );
    if (!listbox) return FALSE;

    item = SendMessageA(listbox, combo ? CB_GETCURSEL : LB_GETCURSEL, 0, 0 );
    if (item == LB_ERR) return FALSE;
    size = SendMessageA(listbox, combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN, 0, 0 );
    if (size == LB_ERR) return FALSE;

    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, size+1 ))) return FALSE;

    SendMessageA( listbox, combo ? CB_GETLBTEXT : LB_GETTEXT, item, (LPARAM)buffer );

    if ((ret = (buffer[0] == '[')))  /* drive or directory */
    {
        if (buffer[1] == '-')  /* drive */
        {
            buffer[3] = ':';
            buffer[4] = 0;
            ptr = buffer + 2;
        }
        else
        {
            buffer[strlen(buffer)-1] = '\\';
            ptr = buffer + 1;
        }
    }
    else ptr = buffer;

    if (unicode)
    {
        if (len > 0 && !MultiByteToWideChar( CP_ACP, 0, ptr, -1, (LPWSTR)str, len ))
            ((LPWSTR)str)[len-1] = 0;
    }
    else lstrcpynA( str, ptr, len );
    HeapFree( GetProcessHeap(), 0, buffer );
    TRACE("Returning %d '%s'\n", ret, str );
    return ret;
}


/**********************************************************************
 *	    DIALOG_DlgDirList
 *
 * Helper function for DlgDirList*
 */
static INT DIALOG_DlgDirList( HWND hDlg, LPSTR spec, INT idLBox,
                                INT idStatic, UINT attrib, BOOL combo )
{
    HWND hwnd;
    LPSTR orig_spec = spec;

#define SENDMSG(msg,wparam,lparam) \
    ((attrib & DDL_POSTMSGS) ? PostMessageA( hwnd, msg, wparam, lparam ) \
                             : SendMessageA( hwnd, msg, wparam, lparam ))

    TRACE("%04x '%s' %d %d %04x\n",
                    hDlg, spec ? spec : "NULL", idLBox, idStatic, attrib );

    /* If the path exists and is a directory, chdir to it */
    if (!spec || !spec[0] || SetCurrentDirectoryA( spec )) spec = "*.*";
    else
    {
        char *p, *p2;
        p = spec;
        if ((p2 = strrchr( p, '\\' ))) p = p2;
        if ((p2 = strrchr( p, '/' ))) p = p2;
        if (p != spec)
        {
            char sep = *p;
            *p = 0;
            if (!SetCurrentDirectoryA( spec ))
            {
                *p = sep;  /* Restore the original spec */
                return FALSE;
            }
            spec = p + 1;
        }
    }

    TRACE( "mask=%s\n", spec );

    if (idLBox && ((hwnd = GetDlgItem( hDlg, idLBox )) != 0))
    {
        SENDMSG( combo ? CB_RESETCONTENT : LB_RESETCONTENT, 0, 0 );
        if (attrib & DDL_DIRECTORY)
        {
            if (!(attrib & DDL_EXCLUSIVE))
            {
                if (SENDMSG( combo ? CB_DIR : LB_DIR,
                             attrib & ~(DDL_DIRECTORY | DDL_DRIVES),
                             (LPARAM)spec ) == LB_ERR)
                    return FALSE;
            }
            if (SENDMSG( combo ? CB_DIR : LB_DIR,
                       (attrib & (DDL_DIRECTORY | DDL_DRIVES)) | DDL_EXCLUSIVE,
                         (LPARAM)"*.*" ) == LB_ERR)
                return FALSE;
        }
        else
        {
            if (SENDMSG( combo ? CB_DIR : LB_DIR, attrib,
                         (LPARAM)spec ) == LB_ERR)
                return FALSE;
        }
    }

    if (idStatic && ((hwnd = GetDlgItem( hDlg, idStatic )) != 0))
    {
        char temp[MAX_PATH];
        GetCurrentDirectoryA( sizeof(temp), temp );
        CharLowerA( temp );
        /* Can't use PostMessage() here, because the string is on the stack */
        SetDlgItemTextA( hDlg, idStatic, temp );
    }

    if (orig_spec && (spec != orig_spec))
    {
        /* Update the original file spec */
        char *p = spec;
        while ((*orig_spec++ = *p++));
    }

    return TRUE;
#undef SENDMSG
}


/**********************************************************************
 *	    DIALOG_DlgDirListW
 *
 * Helper function for DlgDirList*W
 */
static INT DIALOG_DlgDirListW( HWND hDlg, LPWSTR spec, INT idLBox,
                                 INT idStatic, UINT attrib, BOOL combo )
{
    if (spec)
    {
        LPSTR specA = HEAP_strdupWtoA( GetProcessHeap(), 0, spec );
        INT ret = DIALOG_DlgDirList( hDlg, specA, idLBox, idStatic,
                                       attrib, combo );
        MultiByteToWideChar( CP_ACP, 0, specA, -1, spec, 0x7fffffff );
        HeapFree( GetProcessHeap(), 0, specA );
        return ret;
    }
    return DIALOG_DlgDirList( hDlg, NULL, idLBox, idStatic, attrib, combo );
}


/**********************************************************************
 *		DlgDirSelectExA (USER32.@)
 */
BOOL WINAPI DlgDirSelectExA( HWND hwnd, LPSTR str, INT len, INT id )
{
    return DIALOG_DlgDirSelect( hwnd, str, len, id, FALSE, FALSE );
}


/**********************************************************************
 *		DlgDirSelectExW (USER32.@)
 */
BOOL WINAPI DlgDirSelectExW( HWND hwnd, LPWSTR str, INT len, INT id )
{
    return DIALOG_DlgDirSelect( hwnd, (LPSTR)str, len, id, TRUE, FALSE );
}


/**********************************************************************
 *		DlgDirSelectComboBoxExA (USER32.@)
 */
BOOL WINAPI DlgDirSelectComboBoxExA( HWND hwnd, LPSTR str, INT len,
                                         INT id )
{
    return DIALOG_DlgDirSelect( hwnd, str, len, id, FALSE, TRUE );
}


/**********************************************************************
 *		DlgDirSelectComboBoxExW (USER32.@)
 */
BOOL WINAPI DlgDirSelectComboBoxExW( HWND hwnd, LPWSTR str, INT len,
                                         INT id)
{
    return DIALOG_DlgDirSelect( hwnd, (LPSTR)str, len, id, TRUE, TRUE );
}


/**********************************************************************
 *		DlgDirListA (USER32.@)
 */
INT WINAPI DlgDirListA( HWND hDlg, LPSTR spec, INT idLBox,
                            INT idStatic, UINT attrib )
{
    return DIALOG_DlgDirList( hDlg, spec, idLBox, idStatic, attrib, FALSE );
}


/**********************************************************************
 *		DlgDirListW (USER32.@)
 */
INT WINAPI DlgDirListW( HWND hDlg, LPWSTR spec, INT idLBox,
                            INT idStatic, UINT attrib )
{
    return DIALOG_DlgDirListW( hDlg, spec, idLBox, idStatic, attrib, FALSE );
}


/**********************************************************************
 *		DlgDirListComboBoxA (USER32.@)
 */
INT WINAPI DlgDirListComboBoxA( HWND hDlg, LPSTR spec, INT idCBox,
                                    INT idStatic, UINT attrib )
{
    return DIALOG_DlgDirList( hDlg, spec, idCBox, idStatic, attrib, TRUE );
}


/**********************************************************************
 *		DlgDirListComboBoxW (USER32.@)
 */
INT WINAPI DlgDirListComboBoxW( HWND hDlg, LPWSTR spec, INT idCBox,
                                    INT idStatic, UINT attrib )
{
    return DIALOG_DlgDirListW( hDlg, spec, idCBox, idStatic, attrib, TRUE );
}
