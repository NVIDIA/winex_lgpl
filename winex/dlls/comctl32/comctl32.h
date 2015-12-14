/******************************************************************************
 *
 * Common definitions (resource ids and global variables)
 *
 * Copyright 1999 Thuy Nguyen
 * Copyright 1999 Eric Kohl
 *
 */

#ifndef __WINE_COMCTL32_H
#define __WINE_COMCTL32_H

extern HMODULE COMCTL32_hModule;
extern HBRUSH  COMCTL32_hPattern55AABrush;

/* Property sheet / Wizard */
#define IDD_PROPSHEET 1006
#define IDD_WIZARD    1020

#define IDC_TABCONTROL   12320
#define IDC_APPLY_BUTTON 12321
#define IDC_BACK_BUTTON  12323
#define IDC_NEXT_BUTTON  12324
#define IDC_FINISH_BUTTON 12325
#define IDC_SUNKEN_LINE   12326

#define IDS_CLOSE	  4160

/* Toolbar customization dialog */
#define IDD_TBCUSTOMIZE     200

#define IDC_AVAILBTN_LBOX   201
#define IDC_RESET_BTN       202
#define IDC_TOOLBARBTN_LBOX 203
#define IDC_REMOVE_BTN      204
#define IDC_HELP_BTN        205
#define IDC_MOVEUP_BTN      206
#define IDC_MOVEDN_BTN      207

#define IDS_SEPARATOR      1024

/* Toolbar imagelist bitmaps */
#define IDB_STD_SMALL       120
#define IDB_STD_LARGE       121
#define IDB_VIEW_SMALL      124
#define IDB_VIEW_LARGE      125
#define IDB_HIST_SMALL      130
#define IDB_HIST_LARGE      131


/* Month calendar month menu popup */
#define IDD_MCMONTHMENU     300

#define IDM_JAN				301
#define IDM_FEB				302
#define IDM_MAR				303
#define IDM_APR				304
#define IDM_MAY				305
#define IDM_JUN				306
#define IDM_JUL				307
#define IDM_AUG				308
#define IDM_SEP				309
#define IDM_OCT				310
#define IDM_NOV				311
#define IDM_DEC				312

#define IDM_TODAY                      4163
#define IDM_GOTODAY                    4164

/* Treeview Checboxes */

#define IDT_CHECK        401


/* Header cursors */
#define IDC_DIVIDER                     106
#define IDC_DIVIDEROPEN                 107


/* DragList icon */
#define IDI_DRAGARROW                   150

typedef struct
{
    COLORREF clrBtnHighlight;       /* COLOR_BTNHIGHLIGHT                  */
    COLORREF clrBtnShadow;          /* COLOR_BTNSHADOW                     */
    COLORREF clrBtnText;            /* COLOR_BTNTEXT                       */
    COLORREF clrBtnFace;            /* COLOR_BTNFACE                       */
    COLORREF clrHighlight;          /* COLOR_HIGHLIGHT                     */
    COLORREF clrHighlightText;      /* COLOR_HIGHLIGHTTEXT                 */
    COLORREF clr3dHilight;          /* COLOR_3DHILIGHT                     */
    COLORREF clr3dShadow;           /* COLOR_3DSHADOW                      */
    COLORREF clr3dDkShadow;         /* COLOR_3DDKSHADOW                    */
    COLORREF clr3dFace;             /* COLOR_3DFACE                        */
    COLORREF clrWindow;             /* COLOR_WINDOW                        */
    COLORREF clrWindowText;         /* COLOR_WINDOWTEXT                    */
    COLORREF clrGrayText;           /* COLOR_GREYTEXT                      */
    COLORREF clrActiveCaption;      /* COLOR_ACTIVECAPTION                 */
    COLORREF clrInfoBk;             /* COLOR_INFOBK                        */
    COLORREF clrInfoText;           /* COLOR_INFOTEXT                      */
} COMCTL32_SysColor;

extern COMCTL32_SysColor  comctl32_color;

/* Internal function */
HWND COMCTL32_CreateToolTip (HWND);
VOID COMCTL32_RefreshSysColors(void);
INT  Str_GetPtrWtoA (LPCWSTR lpSrc, LPSTR lpDest, INT nMaxLen);
BOOL Str_SetPtrAtoW (LPWSTR *lppDest, LPCSTR lpSrc);

#define COMCTL32_VERSION_MINOR 0
#define WINE_FILEVERSION 5, COMCTL32_VERSION_MINOR, 0, 0
#define WINE_FILEVERSIONSTR "5.00"

#endif  /* __WINE_COMCTL32_H */
