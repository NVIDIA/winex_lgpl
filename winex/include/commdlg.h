/*
 * COMMDLG - Common Wine Dialog ... :-)
 */

#ifndef __WINE_COMMDLG_H
#define __WINE_COMMDLG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "prsht.h"
#include "pshpack1.h"

#define OFN_READONLY                 0x00000001
#define OFN_OVERWRITEPROMPT          0x00000002
#define OFN_HIDEREADONLY             0x00000004
#define OFN_NOCHANGEDIR              0x00000008
#define OFN_SHOWHELP                 0x00000010
#define OFN_ENABLEHOOK               0x00000020
#define OFN_ENABLETEMPLATE           0x00000040
#define OFN_ENABLETEMPLATEHANDLE     0x00000080
#define OFN_NOVALIDATE               0x00000100
#define OFN_ALLOWMULTISELECT         0x00000200
#define OFN_EXTENSIONDIFFERENT       0x00000400
#define OFN_PATHMUSTEXIST            0x00000800
#define OFN_FILEMUSTEXIST            0x00001000
#define OFN_CREATEPROMPT             0x00002000
#define OFN_SHAREAWARE               0x00004000
#define OFN_NOREADONLYRETURN         0x00008000
#define OFN_NOTESTFILECREATE         0x00010000
#define OFN_NONETWORKBUTTON          0x00020000
#define OFN_NOLONGNAMES              0x00040000
#define OFN_EXPLORER                 0x00080000
#define OFN_NODEREFERENCELINKS       0x00100000
#define OFN_LONGNAMES                0x00200000
#define OFN_ENABLEINCLUDENOTIFY      0x00400000
#define OFN_ENABLESIZING             0x00800000
#define OFN_DONTADDTORECENT          0x02000000
#define OFN_FORCESHOWHIDDEN          0x10000000

/* WINE internal flags */
#define OFN_UNICODE		     0x40000000	/*to differ between 32W/A hook*/
#define OFN_WINE		     0x80000000	/* comdlg32 */

#define OFN_SHAREFALLTHROUGH     2
#define OFN_SHARENOWARN          1
#define OFN_SHAREWARN            0

#define SAVE_DIALOG  1
#define OPEN_DIALOG  2

typedef UINT (CALLBACK *LPOFNHOOKPROC)(HWND,UINT,WPARAM,LPARAM);

typedef struct {
	DWORD		lStructSize;
	HWND		hwndOwner;
	HINSTANCE	hInstance;
	LPCSTR		lpstrFilter;
	LPSTR		lpstrCustomFilter;
	DWORD		nMaxCustFilter;
	DWORD		nFilterIndex;
	LPSTR		lpstrFile;
	DWORD		nMaxFile;
	LPSTR		lpstrFileTitle;
	DWORD		nMaxFileTitle;
	LPCSTR		lpstrInitialDir;
	LPCSTR		lpstrTitle;
	DWORD		Flags;
	WORD		nFileOffset;
	WORD		nFileExtension;
	LPCSTR		lpstrDefExt;
	LPARAM		lCustData;
	LPOFNHOOKPROC	lpfnHook;
	LPCSTR		lpTemplateName;
} OPENFILENAMEA,*LPOPENFILENAMEA;

typedef struct {
	DWORD		lStructSize;
	HWND		hwndOwner;
	HINSTANCE	hInstance;
	LPCWSTR		lpstrFilter;
	LPWSTR		lpstrCustomFilter;
	DWORD		nMaxCustFilter;
	DWORD		nFilterIndex;
	LPWSTR		lpstrFile;
	DWORD		nMaxFile;
	LPWSTR		lpstrFileTitle;
	DWORD		nMaxFileTitle;
	LPCWSTR		lpstrInitialDir;
	LPCWSTR		lpstrTitle;
	DWORD		Flags;
	WORD		nFileOffset;
	WORD		nFileExtension;
	LPCWSTR		lpstrDefExt;
	LPARAM		lCustData;
	LPOFNHOOKPROC	lpfnHook;
	LPCWSTR		lpTemplateName;
} OPENFILENAMEW,*LPOPENFILENAMEW;

DECL_WINELIB_TYPE_AW(OPENFILENAME)
DECL_WINELIB_TYPE_AW(LPOPENFILENAME)

typedef struct
{
	NMHDR           hdr;
	LPOPENFILENAMEA lpOFN;
	LPSTR           pszFile;
} OFNOTIFYA, *LPOFNOTIFYA;

typedef struct
{
	NMHDR           hdr;
	LPOPENFILENAMEW lpOFN;
	LPWSTR          pszFile;
} OFNOTIFYW, *LPOFNOTIFYW;

DECL_WINELIB_TYPE_AW(OFNOTIFY)
DECL_WINELIB_TYPE_AW(LPOFNOTIFY)

typedef UINT (CALLBACK *LPCCHOOKPROC) (HWND, UINT, WPARAM, LPARAM);

typedef struct {
	DWORD		lStructSize;
	HWND		hwndOwner;
	HWND		hInstance; /* Should be an HINSTANCE but MS made a typo */
	DWORD	        rgbResult;
	LPDWORD         lpCustColors;
	DWORD 		Flags;
	DWORD		lCustData;
        LPCCHOOKPROC    lpfnHook;
	LPCSTR 		lpTemplateName;
} CHOOSECOLORA;
typedef CHOOSECOLORA *LPCHOOSECOLORA;

typedef struct {
	DWORD		lStructSize;
	HWND		hwndOwner;
	HWND		hInstance; /* Should be an HINSTANCE but MS made a typo */
	DWORD	        rgbResult;
	LPDWORD         lpCustColors;
	DWORD 		Flags;
	DWORD		lCustData;
        LPCCHOOKPROC    lpfnHook;
	LPCWSTR 	lpTemplateName;
} CHOOSECOLORW;
typedef CHOOSECOLORW *LPCHOOSECOLORW;

DECL_WINELIB_TYPE_AW(CHOOSECOLOR)
DECL_WINELIB_TYPE_AW(LPCHOOSECOLOR)


#define CC_RGBINIT               0x00000001
#define CC_FULLOPEN              0x00000002
#define CC_PREVENTFULLOPEN       0x00000004
#define CC_SHOWHELP              0x00000008
#define CC_ENABLEHOOK            0x00000010
#define CC_ENABLETEMPLATE        0x00000020
#define CC_ENABLETEMPLATEHANDLE  0x00000040
#define CC_SOLIDCOLOR            0x00000080
#define CC_ANYCOLOR              0x00000100

typedef UINT (CALLBACK *LPFRHOOKPROC)(HWND,UINT,WPARAM,LPARAM);

typedef struct {
	DWORD		lStructSize;
	HWND		hwndOwner;
	HINSTANCE	hInstance;
	DWORD		Flags;
	LPSTR		lpstrFindWhat;
	LPSTR		lpstrReplaceWith;
	WORD		wFindWhatLen;
	WORD 		wReplaceWithLen;
	LPARAM 		lCustData;
        LPFRHOOKPROC    lpfnHook;
	LPCSTR 		lpTemplateName;
} FINDREPLACEA, *LPFINDREPLACEA;

typedef struct {
	DWORD		lStructSize;
	HWND		hwndOwner;
	HINSTANCE	hInstance;
	DWORD		Flags;
	LPWSTR		lpstrFindWhat;
	LPWSTR		lpstrReplaceWith;
	WORD		wFindWhatLen;
	WORD 		wReplaceWithLen;
	LPARAM 		lCustData;
        LPFRHOOKPROC    lpfnHook;
	LPCWSTR		lpTemplateName;
} FINDREPLACEW, *LPFINDREPLACEW;

DECL_WINELIB_TYPE_AW(FINDREPLACE)
DECL_WINELIB_TYPE_AW(LPFINDREPLACE)

#define FR_DOWN                         0x00000001
#define FR_WHOLEWORD                    0x00000002
#define FR_MATCHCASE                    0x00000004
#define FR_FINDNEXT                     0x00000008
#define FR_REPLACE                      0x00000010
#define FR_REPLACEALL                   0x00000020
#define FR_DIALOGTERM                   0x00000040
#define FR_SHOWHELP                     0x00000080
#define FR_ENABLEHOOK                   0x00000100
#define FR_ENABLETEMPLATE               0x00000200
#define FR_NOUPDOWN                     0x00000400
#define FR_NOMATCHCASE                  0x00000800
#define FR_NOWHOLEWORD                  0x00001000
#define FR_ENABLETEMPLATEHANDLE         0x00002000
#define FR_HIDEUPDOWN                   0x00004000
#define FR_HIDEMATCHCASE                0x00008000
#define FR_HIDEWHOLEWORD                0x00010000

typedef UINT (CALLBACK *LPCFHOOKPROC)(HWND,UINT,WPARAM,LPARAM);

typedef struct
{
	UINT  	lStructSize;
	HWND 		hwndOwner;
	HDC  		hDC;
	LPLOGFONTA    lpLogFont;
	INT		iPointSize;
	DWORD		Flags;
	COLORREF	rgbColors;
	LPARAM		lCustData;
	LPCFHOOKPROC 	lpfnHook;
	LPCSTR		lpTemplateName;
	HINSTANCE	hInstance;
	LPSTR		lpszStyle;
	WORD		nFontType;
	WORD	___MISSING_ALIGNMENT__;
	INT   	nSizeMin;
	INT		nSizeMax;
} CHOOSEFONTA, *LPCHOOSEFONTA;

typedef struct
{
	UINT  	lStructSize;
	HWND 		hwndOwner;
	HDC  		hDC;
	LPLOGFONTW    lpLogFont;
	INT		iPointSize;
	DWORD		Flags;
	COLORREF	rgbColors;
	LPARAM		lCustData;
	LPCFHOOKPROC 	lpfnHook;
	LPCWSTR		lpTemplateName;
	HINSTANCE	hInstance;
	LPWSTR		lpszStyle;
	WORD		nFontType;
	WORD	___MISSING_ALIGNMENT__;
	INT   	nSizeMin;
	INT		nSizeMax;
} CHOOSEFONTW, *LPCHOOSEFONTW;

DECL_WINELIB_TYPE_AW(CHOOSEFONT)
DECL_WINELIB_TYPE_AW(LPCHOOSEFONT)

#define CF_SCREENFONTS               0x00000001
#define CF_PRINTERFONTS              0x00000002
#define CF_BOTH                      (CF_SCREENFONTS | CF_PRINTERFONTS)
#define CF_SHOWHELP                  0x00000004L
#define CF_ENABLEHOOK                0x00000008L
#define CF_ENABLETEMPLATE            0x00000010L
#define CF_ENABLETEMPLATEHANDLE      0x00000020L
#define CF_INITTOLOGFONTSTRUCT       0x00000040L
#define CF_USESTYLE                  0x00000080L
#define CF_EFFECTS                   0x00000100L
#define CF_APPLY                     0x00000200L
#define CF_ANSIONLY                  0x00000400L
#define CF_SCRIPTSONLY               CF_ANSIONLY
#define CF_NOVECTORFONTS             0x00000800L
#define CF_NOOEMFONTS                CF_NOVECTORFONTS
#define CF_NOSIMULATIONS             0x00001000L
#define CF_LIMITSIZE                 0x00002000L
#define CF_FIXEDPITCHONLY            0x00004000L
#define CF_WYSIWYG                   0x00008000L /* use with CF_SCREENFONTS & CF_PRINTERFONTS */
#define CF_FORCEFONTEXIST            0x00010000L
#define CF_SCALABLEONLY              0x00020000L
#define CF_TTONLY                    0x00040000L
#define CF_NOFACESEL                 0x00080000L
#define CF_NOSTYLESEL                0x00100000L
#define CF_NOSIZESEL                 0x00200000L
#define CF_SELECTSCRIPT              0x00400000L
#define CF_NOSCRIPTSEL               0x00800000L
#define CF_NOVERTFONTS               0x01000000L

#define SIMULATED_FONTTYPE      0x8000
#define PRINTER_FONTTYPE        0x4000
#define SCREEN_FONTTYPE         0x2000
#define BOLD_FONTTYPE           0x0100
#define ITALIC_FONTTYPE         0x0200
#define REGULAR_FONTTYPE        0x0400

#define WM_CHOOSEFONT_GETLOGFONT        (WM_USER + 1)
#define WM_CHOOSEFONT_SETLOGFONT	(WM_USER + 101)
#define WM_CHOOSEFONT_SETFLAGS		(WM_USER + 102)

#define LBSELCHSTRINGA  "commdlg_LBSelChangedNotify"
static const WCHAR LBSELCHSTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'L','B','S','e','l','C','h','a','n','g','e','d','N','o','t','i','f','y',0 };
#define LBSELCHSTRING   WINELIB_NAME_AW(LBSELCHSTRING)

#define SHAREVISTRINGA  "commdlg_ShareViolation"
static const WCHAR SHAREVISTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'S','h','a','r','e','V','i','o','l','a','t','i','o','n',0 };
#define SHAREVISTRING   WINELIB_NAME_AW(SHAREVISTRING)

#define FILEOKSTRINGA   "commdlg_FileNameOK"
static const WCHAR FILEOKSTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'F','i','l','e','N','a','m','e','O','K',0 };
#define FILEOKSTRING    WINELIB_NAME_AW(FILEOKSTRING)

#define COLOROKSTRINGA  "commdlg_ColorOK"
static const WCHAR COLOROKSTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'C','o','l','o','r','O','K',0 };
#define COLOROKSTRING   WINELIB_NAME_AW(COLOROKSTRING)

#define SETRGBSTRINGA   "commdlg_SetRGBColor"
static const WCHAR SETRGBSTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'S','e','t','R','G','B','C','o','l','o','r',0 };
#define SETRGBSTRING    WINELIB_NAME_AW(SETRGBSTRING)

#define FINDMSGSTRINGA  "commdlg_FindReplace"
static const WCHAR FINDMSGSTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'F','i','n','d','R','e','p','l','a','c','e',0 };
#define FINDMSGSTRING   WINELIB_NAME_AW(FINDMSGSTRING)

#define HELPMSGSTRINGA  "commdlg_help"
static const WCHAR HELPMSGSTRINGW[] = { 'c','o','m','m','d','l','g','_',
  'H','e','l','p',0 };
#define HELPMSGSTRING   WINELIB_NAME_AW(HELPMSGSTRING)


#define CD_LBSELNOITEMS -1
#define CD_LBSELCHANGE   0
#define CD_LBSELSUB      1
#define CD_LBSELADD      2

#define CDN_FIRST   (0U-601U)
#define CDN_LAST    (0U-699U)

#define CDN_INITDONE            (CDN_FIRST - 0x0000)
#define CDN_SELCHANGE           (CDN_FIRST - 0x0001)
#define CDN_FOLDERCHANGE        (CDN_FIRST - 0x0002)
#define CDN_SHAREVIOLATION      (CDN_FIRST - 0x0003)
#define CDN_HELP                (CDN_FIRST - 0x0004)
#define CDN_FILEOK              (CDN_FIRST - 0x0005)
#define CDN_TYPECHANGE          (CDN_FIRST - 0x0006)

#define CDM_FIRST               (WM_USER + 100)
#define CDM_LAST                (WM_USER + 200)

#define CDM_GETSPEC             (CDM_FIRST + 0x0000)
#define CDM_GETFILEPATH         (CDM_FIRST + 0x0001)
#define CDM_GETFOLDERPATH       (CDM_FIRST + 0x0002)
#define CDM_GETFOLDERLIST       (CDM_FIRST + 0x0003)
#define CDM_SETCONTROLTEXT      (CDM_FIRST + 0x0004)
#define CDM_HIDECONTROL         (CDM_FIRST + 0x0005)
#define CDM_SETDEFEXT           (CDM_FIRST + 0x0006)


/* Messages to query information from the open or save dialogs */

#define CommDlg_OpenSave_GetSpecA(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETSPEC, (WPARAM)_cbmax, (LPARAM)(LPSTR)_psz)
#define CommDlg_OpenSave_GetSpecW(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETSPEC, (WPARAM)_cbmax, (LPARAM)(LPWSTR)_psz)
#define CommDlg_OpenSave_GetSpec  WINELIB_NAME_AW(CommDlg_OpenSave_GetSpec)

#define CommDlg_OpenSave_GetFilePathA(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFILEPATH, (WPARAM)_cbmax, (LPARAM)(LPSTR)_psz)
#define CommDlg_OpenSave_GetFilePathW(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFILEPATH, (WPARAM)_cbmax, (LPARAM)(LPWSTR)_psz)
#define CommDlg_OpenSave_GetFilePath  WINELIB_NAME_AW(CommDlg_OpenSave_GetFilePath)

#define CommDlg_OpenSave_GetFolderPathA(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFOLDERPATH, (WPARAM)_cbmax, (LPARAM)(LPSTR)_psz)
#define CommDlg_OpenSave_GetFolderPathW(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFOLDERPATH, (WPARAM)_cbmax, (LPARAM)(LPWSTR)_psz)
#define CommDlg_OpenSave_GetFolderPath  WINELIB_NAME_AW(CommDlg_OpenSave_GetFolderPath)

#define CommDlg_OpenSave_GetFolderIDList(_hdlg, _pidl, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFOLDERIDLIST, (WPARAM)_cbmax, (LPARAM)(LPVOID)_pidl)

#define CommDlg_OpenSave_SetControlText(_hdlg, _id, _text) \
        (void)SNDMSG(_hdlg, CDM_SETCONTROLTEXT, (WPARAM)_id, (LPARAM)(LPSTR)_text)

#define CommDlg_OpenSave_HideControl(_hdlg, _id) \
        (void)SNDMSG(_hdlg, CDM_HIDECONTROL, (WPARAM)_id, 0)

#define CommDlg_OpenSave_SetDefExt(_hdlg, _pszext) \
        (void)SNDMSG(_hdlg, CDM_SETDEFEXT, 0, (LPARAM)(LPSTR)_pszext)

#define CommDlg_OpenSave_GetSpecA(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETSPEC, (WPARAM)_cbmax, (LPARAM)(LPSTR)_psz)
#define CommDlg_OpenSave_GetSpecW(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETSPEC, (WPARAM)_cbmax, (LPARAM)(LPWSTR)_psz)
#define CommDlg_OpenSave_GetSpec WINELIB_NAME_AW(CommDlg_OpenSave_GetSpec)

#define CommDlg_OpenSave_GetFilePathA(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFILEPATH, (WPARAM)_cbmax, (LPARAM)(LPSTR)_psz)
#define CommDlg_OpenSave_GetFilePathW(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFILEPATH, (WPARAM)_cbmax, (LPARAM)(LPWSTR)_psz)
#define CommDlg_OpenSave_GetFilePath WINELIB_NAME_AW(CommDlg_OpenSave_GetFilePath)

#define CommDlg_OpenSave_GetFolderPathA(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFOLDERPATH, (WPARAM)_cbmax, (LPARAM)(LPSTR)_psz)
#define CommDlg_OpenSave_GetFolderPathW(_hdlg, _psz, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFOLDERPATH, (WPARAM)_cbmax, (LPARAM)(LPWSTR)_psz)
#define CommDlg_OpenSave_GetFolderPath WINELIB_NAME_AW(CommDlg_OpenSave_GetFolderPath)

#define CommDlg_OpenSave_GetFolderIDList(_hdlg, _pidl, _cbmax) \
        (int)SNDMSG(_hdlg, CDM_GETFOLDERIDLIST, (WPARAM)_cbmax, (LPARAM)(LPVOID)_pidl)

#define CommDlg_OpenSave_SetControlText(_hdlg, _id, _text) \
        (void)SNDMSG(_hdlg, CDM_SETCONTROLTEXT, (WPARAM)_id, (LPARAM)(LPSTR)_text)

#define CommDlg_OpenSave_HideControl(_hdlg, _id) \
        (void)SNDMSG(_hdlg, CDM_HIDECONTROL, (WPARAM)_id, 0)

#define CommDlg_OpenSave_SetDefExt(_hdlg, _pszext) \
        (void)SNDMSG(_hdlg, CDM_SETDEFEXT, 0, (LPARAM)(LPSTR)_pszext)


typedef UINT (CALLBACK *LPPRINTHOOKPROC) (HWND, UINT, WPARAM, LPARAM);
typedef UINT (CALLBACK *LPSETUPHOOKPROC) (HWND, UINT, WPARAM, LPARAM);

typedef struct tagPDA
{
    DWORD            lStructSize;
    HWND           hwndOwner;
    HGLOBAL        hDevMode;
    HGLOBAL        hDevNames;
    HDC            hDC;
    DWORD            Flags;
    WORD             nFromPage;
    WORD             nToPage;
    WORD             nMinPage;
    WORD             nMaxPage;
    WORD             nCopies;
    HINSTANCE      hInstance;
    LPARAM           lCustData;
    LPPRINTHOOKPROC  lpfnPrintHook;
    LPSETUPHOOKPROC  lpfnSetupHook;
    LPCSTR           lpPrintTemplateName;
    LPCSTR           lpSetupTemplateName;
    HGLOBAL        hPrintTemplate;
    HGLOBAL        hSetupTemplate;
} PRINTDLGA, *LPPRINTDLGA;

typedef struct tagPDW
{
    DWORD            lStructSize;
    HWND           hwndOwner;
    HGLOBAL        hDevMode;
    HGLOBAL        hDevNames;
    HDC            hDC;
    DWORD            Flags;
    WORD             nFromPage;
    WORD             nToPage;
    WORD             nMinPage;
    WORD             nMaxPage;
    WORD             nCopies;
    HINSTANCE      hInstance;
    LPARAM           lCustData;
    LPPRINTHOOKPROC  lpfnPrintHook;
    LPSETUPHOOKPROC  lpfnSetupHook;
    LPCWSTR          lpPrintTemplateName;
    LPCWSTR          lpSetupTemplateName;
    HGLOBAL        hPrintTemplate;
    HGLOBAL        hSetupTemplate;
} PRINTDLGW, *LPPRINTDLGW;

DECL_WINELIB_TYPE_AW(PRINTDLG)
DECL_WINELIB_TYPE_AW(LPPRINTDLG)

#define PD_ALLPAGES                  0x00000000
#define PD_SELECTION                 0x00000001
#define PD_PAGENUMS                  0x00000002
#define PD_NOSELECTION               0x00000004
#define PD_NOPAGENUMS                0x00000008
#define PD_COLLATE                   0x00000010
#define PD_PRINTTOFILE               0x00000020
#define PD_PRINTSETUP                0x00000040
#define PD_NOWARNING                 0x00000080
#define PD_RETURNDC                  0x00000100
#define PD_RETURNIC                  0x00000200
#define PD_RETURNDEFAULT             0x00000400
#define PD_SHOWHELP                  0x00000800
#define PD_ENABLEPRINTHOOK           0x00001000
#define PD_ENABLESETUPHOOK           0x00002000
#define PD_ENABLEPRINTTEMPLATE       0x00004000
#define PD_ENABLESETUPTEMPLATE       0x00008000
#define PD_ENABLEPRINTTEMPLATEHANDLE 0x00010000
#define PD_ENABLESETUPTEMPLATEHANDLE 0x00020000
#define PD_USEDEVMODECOPIES          0x00040000
#define PD_USEDEVMODECOPIESANDCOLLATE 0x00040000
#define PD_DISABLEPRINTTOFILE        0x00080000
#define PD_HIDEPRINTTOFILE           0x00100000
#define PD_NONETWORKBUTTON           0x00200000

typedef struct
{
    WORD  wDriverOffset;
    WORD  wDeviceOffset;
    WORD  wOutputOffset;
    WORD  wDefault;
} DEVNAMES;
typedef DEVNAMES * LPDEVNAMES;

#define DN_DEFAULTPRN      0x0001

/* PageSetupDlg stuff ... */
#define WM_PSD_PAGESETUPDLG	(WM_USER  )
#define WM_PSD_FULLPAGERECT	(WM_USER+1)
#define WM_PSD_MINMARGINRECT	(WM_USER+2)
#define WM_PSD_MARGINRECT	(WM_USER+3)
#define WM_PSD_GREEKTEXTRECT	(WM_USER+4)
#define WM_PSD_ENVSTAMPRECT	(WM_USER+5)
#define WM_PSD_YAFULLPAGERECT	(WM_USER+6)

typedef UINT (CALLBACK *LPPAGEPAINTHOOK)( HWND, UINT, WPARAM, LPARAM );
typedef UINT (CALLBACK *LPPAGESETUPHOOK)( HWND, UINT, WPARAM, LPARAM );

typedef struct tagPSDA
{
	DWORD		lStructSize;
	HWND		hwndOwner;
	HGLOBAL	hDevMode;
	HGLOBAL	hDevNames;
	DWORD		Flags;
	POINT		ptPaperSize;
	RECT		rtMinMargin;
	RECT		rtMargin;
	HINSTANCE	hInstance;
	LPARAM		lCustData;
	LPPAGESETUPHOOK	lpfnPageSetupHook;
	LPPAGEPAINTHOOK	lpfnPagePaintHook;
	LPCSTR		lpPageSetupTemplateName;
	HGLOBAL	hPageSetupTemplate;
} PAGESETUPDLGA,*LPPAGESETUPDLGA;

typedef struct tagPSDW
{
	DWORD		lStructSize;
	HWND		hwndOwner;
	HGLOBAL	hDevMode;
	HGLOBAL	hDevNames;
	DWORD		Flags;
	POINT		ptPaperSize;
	RECT		rtMinMargin;
	RECT		rtMargin;
	HINSTANCE	hInstance;
	LPARAM		lCustData;
	LPPAGESETUPHOOK	lpfnPageSetupHook;
	LPPAGEPAINTHOOK	lpfnPagePaintHook;
	LPCWSTR		lpPageSetupTemplateName;
	HGLOBAL	hPageSetupTemplate;
} PAGESETUPDLGW,*LPPAGESETUPDLGW;
DECL_WINELIB_TYPE_AW(PAGESETUPDLG)
DECL_WINELIB_TYPE_AW(LPPAGESETUPDLG)

#define PSD_DEFAULTMINMARGINS             0x00000000
#define PSD_INWININIINTLMEASURE           0x00000000

#define PSD_MINMARGINS                    0x00000001
#define PSD_MARGINS                       0x00000002
#define PSD_INTHOUSANDTHSOFINCHES         0x00000004
#define PSD_INHUNDREDTHSOFMILLIMETERS     0x00000008
#define PSD_DISABLEMARGINS                0x00000010
#define PSD_DISABLEPRINTER                0x00000020
#define PSD_NOWARNING                     0x00000080
#define PSD_DISABLEORIENTATION            0x00000100
#define PSD_RETURNDEFAULT                 0x00000400
#define PSD_DISABLEPAPER                  0x00000200
#define PSD_SHOWHELP                      0x00000800
#define PSD_ENABLEPAGESETUPHOOK           0x00002000
#define PSD_ENABLEPAGESETUPTEMPLATE       0x00008000
#define PSD_ENABLEPAGESETUPTEMPLATEHANDLE 0x00020000
#define PSD_ENABLEPAGEPAINTHOOK           0x00040000
#define PSD_DISABLEPAGEPAINTING           0x00080000


BOOL  WINAPI ChooseColorA(LPCHOOSECOLORA lpChCol);
BOOL  WINAPI ChooseColorW(LPCHOOSECOLORW lpChCol);
#define ChooseColor WINELIB_NAME_AW(ChooseColor)
DWORD   WINAPI CommDlgExtendedError(void);
HWND  WINAPI FindTextA(LPFINDREPLACEA lpFind);
HWND  WINAPI FindTextW(LPFINDREPLACEW lpFind);
#define FindText WINELIB_NAME_AW(FindText)
short   WINAPI GetFileTitleA(LPCSTR lpFile, LPSTR lpTitle, UINT cbBuf);
short   WINAPI GetFileTitleW(LPCWSTR lpFile, LPWSTR lpTitle, UINT cbBuf);
#define GetFileTitle WINELIB_NAME_AW(GetFileTitle)
BOOL  WINAPI GetOpenFileNameA(LPOPENFILENAMEA ofn);
BOOL  WINAPI GetOpenFileNameW(LPOPENFILENAMEW ofn);
#define GetOpenFileName WINELIB_NAME_AW(GetOpenFileName)
BOOL  WINAPI GetSaveFileNameA(LPOPENFILENAMEA ofn);
BOOL  WINAPI GetSaveFileNameW(LPOPENFILENAMEW ofn);
#define GetSaveFileName WINELIB_NAME_AW(GetSaveFileName)
BOOL WINAPI PageSetupDlgA( LPPAGESETUPDLGA );
BOOL WINAPI PageSetupDlgW( LPPAGESETUPDLGW );
#define PageSetupDlg WINELIB_NAME_AW(PageSetupDlg)
BOOL  WINAPI PrintDlgA( LPPRINTDLGA printdlg);
BOOL  WINAPI PrintDlgW( LPPRINTDLGW printdlg);
#define PrintDlg WINELIB_NAME_AW(PrintDlg)
HWND  WINAPI ReplaceTextA( LPFINDREPLACEA lpFind);
HWND  WINAPI ReplaceTextW( LPFINDREPLACEW lpFind);
#define ReplaceText WINELIB_NAME_AW(ReplaceText)
BOOL  WINAPI ChooseFontA(LPCHOOSEFONTA);
BOOL  WINAPI ChooseFontW(LPCHOOSEFONTW);
#define ChooseFont WINELIB_NAME_AW(ChooseFont)

void COMDLG32_SetCommDlgExtendedError(DWORD err);


#include "poppack.h"

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_COMMDLG_H */
