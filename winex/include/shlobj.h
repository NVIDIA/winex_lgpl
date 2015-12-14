#ifndef __WINE_SHLOBJ_H
#define __WINE_SHLOBJ_H

#include "windef.h"
#include "winbase.h"	/* WIN32_FIND_* */
#include "ole2.h"
#include "oleauto.h"
#include "commctrl.h"
#include "prsht.h"

#ifndef INITGUID
#include "shlguid.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

/****************************************************************************
* SHITEMID, ITEMIDLIST, PIDL API
*/
#include "pshpack1.h"
typedef struct
{
    WORD cb;      /* nr of bytes in this item */
    BYTE abID[1]; /* first byte in this item */
} SHITEMID, *LPSHITEMID;
typedef LPSHITEMID const LPCSHITEMID;

typedef struct _ITEMIDLIST
{
    SHITEMID mkid; /* first itemid in list */
} ITEMIDLIST,*LPITEMIDLIST,*LPCITEMIDLIST;
#include "poppack.h"

BOOL WINAPI SHGetPathFromIDListA (LPCITEMIDLIST pidl,LPSTR pszPath);
BOOL WINAPI SHGetPathFromIDListW (LPCITEMIDLIST pidl,LPWSTR pszPath);
#define     SHGetPathFromIDList WINELIB_NAME_AW(SHGetPathFromIDList)


#include "wine/obj_enumidlist.h"
#include "wine/obj_shellfolder.h"
#include "wine/obj_shellview.h"
#include "wine/obj_shelllink.h"
#include "wine/obj_shellbrowser.h"
#include "wine/obj_contextmenu.h"
#include "wine/obj_shellextinit.h"
#include "wine/obj_extracticon.h"
#include "wine/obj_commdlgbrowser.h"
#include "wine/obj_dockingwindowframe.h"

/*****************************************************************************
 * Predeclare interfaces
 */
typedef struct IShellIcon IShellIcon, *LPSHELLICON;


/*****************************************************************************
 * IContextMenu interface
 */


/* DATAOBJECT_InitShellIDList*/
#define CFSTR_SHELLIDLIST       "Shell IDList Array"      /* CF_IDLIST */

extern UINT cfShellIDList;

typedef struct
{	UINT cidl;
	UINT aoffset[1];
} CIDA, *LPCIDA,*LPIDA;

#define CFSTR_SHELLIDLISTOFFSET "Shell Object Offsets"    /* CF_OBJECTPOSITIONS */
#define CFSTR_NETRESOURCES      "Net Resource"            /* CF_NETRESOURCE */

/* DATAOBJECT_InitFileGroupDesc */
#define CFSTR_FILEDESCRIPTORA   "FileGroupDescriptor"     /* CF_FILEGROUPDESCRIPTORA */
extern UINT cfFileGroupDesc;

#define CFSTR_FILEDESCRIPTORW   "FileGroupDescriptorW"    /* CF_FILEGROUPDESCRIPTORW */

/* DATAOBJECT_InitFileContents*/
#define CFSTR_FILECONTENTS      "FileContents"            /* CF_FILECONTENTS */
extern UINT cfFileContents;

#define CFSTR_FILENAMEA         "FileName"                /* CF_FILENAMEA */
#define CFSTR_FILENAMEW         "FileNameW"               /* CF_FILENAMEW */
#define CFSTR_PRINTERGROUP      "PrinterFriendlyName"     /* CF_PRINTERS */
#define CFSTR_FILENAMEMAPA      "FileNameMap"             /* CF_FILENAMEMAPA */
#define CFSTR_FILENAMEMAPW      "FileNameMapW"            /* CF_FILENAMEMAPW */
#define CFSTR_SHELLURL          "UniformResourceLocator"
#define CFSTR_PREFERREDDROPEFFECT "Preferred DropEffect"
#define CFSTR_PERFORMEDDROPEFFECT "Performed DropEffect"
#define CFSTR_PASTESUCCEEDED    "Paste Succeeded"
#define CFSTR_INDRAGLOOP        "InShellDragLoop"


/************************************************************************
* IShellView interface
*/

typedef GUID SHELLVIEWID;
#define SV_CLASS_NAME   ("SHELLDLL_DefView")

/****************************************************************************
 * IShellIcon interface
 */

#define ICOM_INTERFACE IShellIcon
#define IShellIcon_METHODS \
    ICOM_METHOD3(HRESULT, GetIconOf,     LPCITEMIDLIST,pidl, UINT,flags, LPINT,lpIconIndex)
#define IShellIcon_IMETHODS \
    IUnknown_IMETHODS \
    IShellIcon_METHODS
ICOM_DEFINE(IShellIcon, IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IShellIcon_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define IShellIcon_AddRef(p)                  ICOM_CALL (AddRef,p)
#define IShellIcon_Release(p)                 ICOM_CALL (Release,p)
/*** IShellIcon methods ***/
#define IShellIcon_GetIconOf(p,a,b,c)         ICOM_CALL3(GetIconOf,p,a,b,c)

/****************************************************************************
* SHAddToRecentDocs API
*/
#define SHARD_PIDL      0x00000001L
#define SHARD_PATHA     0x00000002L
#define SHARD_PATHW     0x00000003L
#define SHARD_PATH WINELIB_NAME_AW(SHARD_PATH)

DWORD WINAPI SHAddToRecentDocs(UINT uFlags, LPCVOID pv);

/****************************************************************************
 * Shell Execute API
 */
#define SE_ERR_FNF              2       /* file not found */
#define SE_ERR_PNF              3       /* path not found */
#define SE_ERR_ACCESSDENIED     5       /* access denied */
#define SE_ERR_OOM              8       /* out of memory */
#define SE_ERR_DLLNOTFOUND      32
#define SE_ERR_SHARE                    26
#define SE_ERR_ASSOCINCOMPLETE          27
#define SE_ERR_DDETIMEOUT               28
#define SE_ERR_DDEFAIL                  29
#define SE_ERR_DDEBUSY                  30
#define SE_ERR_NOASSOC                  31

#define SEE_MASK_CLASSNAME        0x00000001
#define SEE_MASK_CLASSKEY         0x00000003
#define SEE_MASK_IDLIST           0x00000004
#define SEE_MASK_INVOKEIDLIST     0x0000000c
#define SEE_MASK_ICON             0x00000010
#define SEE_MASK_HOTKEY           0x00000020
#define SEE_MASK_NOCLOSEPROCESS   0x00000040
#define SEE_MASK_CONNECTNETDRV    0x00000080
#define SEE_MASK_FLAG_DDEWAIT     0x00000100
#define SEE_MASK_DOENVSUBST       0x00000200
#define SEE_MASK_FLAG_NO_UI       0x00000400
#define SEE_MASK_UNICODE          0x00004000
#define SEE_MASK_NO_CONSOLE       0x00008000
#define SEE_MASK_ASYNCOK          0x00100000
#define SEE_MASK_HMONITOR         0x00200000

typedef struct _SHELLEXECUTEINFOA
{       DWORD cbSize;
        ULONG fMask;
        HWND hwnd;
        LPCSTR   lpVerb;
        LPCSTR   lpFile;
        LPCSTR   lpParameters;
        LPCSTR   lpDirectory;
       INT nShow;
        HINSTANCE hInstApp;
        /* Optional fields */
        LPVOID lpIDList;
        LPCSTR   lpClass;
        HKEY hkeyClass;
        DWORD dwHotKey;
        union
        { HANDLE hIcon;
          HANDLE hMonitor;
        } DUMMYUNIONNAME;
        HANDLE hProcess;
} SHELLEXECUTEINFOA, *LPSHELLEXECUTEINFOA;

typedef struct _SHELLEXECUTEINFOW
{       DWORD cbSize;
        ULONG fMask;
        HWND hwnd;
        LPCWSTR  lpVerb;
        LPCWSTR  lpFile;
        LPCWSTR  lpParameters;
        LPCWSTR  lpDirectory;
       INT nShow;
        HINSTANCE hInstApp;
        /* Optional fields*/
        LPVOID lpIDList;
        LPCWSTR  lpClass;
        HKEY hkeyClass;
        DWORD dwHotKey;
        union
        { HANDLE hIcon;
          HANDLE hMonitor;
        } DUMMYUNIONNAME;
        HANDLE hProcess;
} SHELLEXECUTEINFOW, *LPSHELLEXECUTEINFOW;

#define SHELLEXECUTEINFO   WINELIB_NAME_AW(SHELLEXECUTEINFO)
#define LPSHELLEXECUTEINFO WINELIB_NAME_AW(LPSHELLEXECUTEINFO)

BOOL WINAPI ShellExecuteExA(LPSHELLEXECUTEINFOA lpExecInfo);
BOOL WINAPI ShellExecuteExW(LPSHELLEXECUTEINFOW lpExecInfo);
#define ShellExecuteEx  WINELIB_NAME_AW(ShellExecuteEx)

void WINAPI WinExecErrorA(HWND hwnd,INT error, LPCSTR lpstrFileName, LPCSTR lpstrTitle);
void WINAPI WinExecErrorW(HWND hwnd,INT error, LPCWSTR lpstrFileName, LPCWSTR lpstrTitle);
#define WinExecError  WINELIB_NAME_AW(WinExecError)



/****************************************************************************
 * SHBrowseForFolder API
 */
typedef INT (CALLBACK *BFFCALLBACK)(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

typedef struct tagBROWSEINFOA {
    HWND        hwndOwner;
    LPCITEMIDLIST pidlRoot;
    LPSTR         pszDisplayName;
    LPCSTR        lpszTitle;
    UINT        ulFlags;
    BFFCALLBACK   lpfn;
    LPARAM        lParam;
    INT         iImage;
} BROWSEINFOA, *PBROWSEINFOA, *LPBROWSEINFOA;

typedef struct tagBROWSEINFOW {
    HWND        hwndOwner;
    LPCITEMIDLIST pidlRoot;
    LPWSTR        pszDisplayName;
    LPCWSTR       lpszTitle;
    UINT        ulFlags;
    BFFCALLBACK   lpfn;
    LPARAM        lParam;
    INT         iImage;
} BROWSEINFOW, *PBROWSEINFOW, *LPBROWSEINFOW;

#define BROWSEINFO   WINELIB_NAME_AW(BROWSEINFO)
#define PBROWSEINFO  WINELIB_NAME_AW(PBROWSEINFO)
#define LPBROWSEINFO WINELIB_NAME_AW(LPBROWSEINFO)

/* Browsing for directory. */
#define BIF_RETURNONLYFSDIRS   0x0001
#define BIF_DONTGOBELOWDOMAIN  0x0002
#define BIF_STATUSTEXT         0x0004
#define BIF_RETURNFSANCESTORS  0x0008
#define BIF_EDITBOX            0x0010
#define BIF_VALIDATE           0x0020

#define BIF_BROWSEFORCOMPUTER  0x1000
#define BIF_BROWSEFORPRINTER   0x2000
#define BIF_BROWSEINCLUDEFILES 0x4000

/* message from browser */
#define BFFM_INITIALIZED        1
#define BFFM_SELCHANGED         2
#define BFFM_VALIDATEFAILEDA    3   /* lParam:szPath ret:1(cont),0(EndDialog) */
#define BFFM_VALIDATEFAILEDW    4   /* lParam:wzPath ret:1(cont),0(EndDialog) */

/* messages to browser */
#define BFFM_SETSTATUSTEXTA     (WM_USER+100)
#define BFFM_ENABLEOK           (WM_USER+101)
#define BFFM_SETSELECTIONA      (WM_USER+102)
#define BFFM_SETSELECTIONW      (WM_USER+103)
#define BFFM_SETSTATUSTEXTW     (WM_USER+104)

LPITEMIDLIST WINAPI SHBrowseForFolderA(LPBROWSEINFOA lpbi);
LPITEMIDLIST WINAPI SHBrowseForFolderW(LPBROWSEINFOW lpbi);
#define SHBrowseForFolder	 WINELIB_NAME_AW(SHBrowseForFolder)
#define BFFM_SETSTATUSTEXT  WINELIB_NAME_AW(BFFM_SETSTATUSTEXT)
#define BFFM_SETSELECTION   WINELIB_NAME_AW(BFFM_SETSELECTION)
#define BFFM_VALIDATEFAILED WINELIB_NAME_AW(BFFM_VALIDATEFAILED)

/****************************************************************************
*	SHGetDataFromIDList API
*/
#define SHGDFIL_FINDDATA        1
#define SHGDFIL_NETRESOURCE     2
#define SHGDFIL_DESCRIPTIONID   3

#define SHDID_ROOT_REGITEM          1
#define SHDID_FS_FILE               2
#define SHDID_FS_DIRECTORY          3
#define SHDID_FS_OTHER              4
#define SHDID_COMPUTER_DRIVE35      5
#define SHDID_COMPUTER_DRIVE525     6
#define SHDID_COMPUTER_REMOVABLE    7
#define SHDID_COMPUTER_FIXED        8
#define SHDID_COMPUTER_NETDRIVE     9
#define SHDID_COMPUTER_CDROM        10
#define SHDID_COMPUTER_RAMDISK      11
#define SHDID_COMPUTER_OTHER        12
#define SHDID_NET_DOMAIN            13
#define SHDID_NET_SERVER            14
#define SHDID_NET_SHARE             15
#define SHDID_NET_RESTOFNET         16
#define SHDID_NET_OTHER             17

typedef struct _SHDESCRIPTIONID
{   DWORD   dwDescriptionId;
    CLSID   clsid;
} SHDESCRIPTIONID, *LPSHDESCRIPTIONID;

HRESULT WINAPI SHGetDataFromIDListA(LPSHELLFOLDER psf, LPCITEMIDLIST pidl, int nFormat, LPVOID pv, int cb);
HRESULT WINAPI SHGetDataFromIDListW(LPSHELLFOLDER psf, LPCITEMIDLIST pidl, int nFormat, LPVOID pv, int cb);
#define  SHGetDataFromIDList WINELIB_NAME_AW(SHGetDataFromIDList)

BOOL WINAPI SHGetSpecialFolderPathA (HWND hwndOwner,LPSTR szPath,DWORD csidl,BOOL bCreate);
BOOL WINAPI SHGetSpecialFolderPathW (HWND hwndOwner,LPWSTR szPath,DWORD csidl,BOOL bCreate);
#define  SHGetSpecialFolderPath WINELIB_NAME_AW(SHGetSpecialFolderPath)

HRESULT WINAPI SHGetSpecialFolderLocation(HWND hwndOwner, INT nFolder, LPITEMIDLIST * ppidl);

/****************************************************************************
* shlview structures
*/

/*
* IShellFolderViewCallback Callback
*  This "callback" is called by the shells default IShellView implementation (that
*  we got using SHCreateShellViewEx()), to notify us of the various things that
*  are happening to the shellview (and ask for things too).
*
*  You don't have to support anything here - anything you don't want to
*  handle, the shell will do itself if you just return E_NOTIMPL. This parameters
*  that the shell passes to this function are entirely undocumented.
*
*  HOWEVER, as the cabview sample as originally written used this callback, the
*  writers implemented the callback mechanism on top of their own IShellView.
*  Look there for some clues on what to do here.
*/

typedef HRESULT (CALLBACK *SHELLVIEWPROC)(DWORD dwUserParam,LPSHELLFOLDER psf,
                         HWND hwnd,UINT uMsg,UINT wParam,LPARAM lParam);

/* NF valid values for the "viewmode" item of the SHELLTEMPLATE*/
#define NF_INHERITVIEW    0x0000
#define NF_LOCALVIEW        0x0001

typedef struct _SHELLVIEWDATA   /* idl */
{ DWORD           dwSize;
  LPSHELLFOLDER   pShellFolder;
  DWORD           dwUserParam;
  LPCITEMIDLIST   pidl;
  DWORD           v3;        /* always 0 */
  SHELLVIEWPROC   pCallBack;
  DWORD           viewmode;  /* NF_* enum */
} SHELLVIEWDATA, * LPSHELLVIEWDATA;

DWORD WINAPI SHGetMalloc(LPMALLOC *lpmal) ;

/**********************************************************************
 * SHGetSettings ()
 */
typedef struct
{	BOOL fShowAllObjects : 1;
	BOOL fShowExtensions : 1;
	BOOL fNoConfirmRecycle : 1;
	BOOL fShowSysFiles : 1;

	BOOL fShowCompColor : 1;
	BOOL fDoubleClickInWebView : 1;
	BOOL fDesktopHTML : 1;
	BOOL fWin95Classic : 1;

	BOOL fDontPrettyPath : 1;
	BOOL fShowAttribCol : 1;
	BOOL fMapNetDrvBtn : 1;
	BOOL fShowInfoTip : 1;

	BOOL fHideIcons : 1;
	UINT fRestFlags : 3;
} SHELLFLAGSTATE, * LPSHELLFLAGSTATE;

VOID WINAPI SHGetSettings(LPSHELLFLAGSTATE lpsfs, DWORD dwMask);

#define SSF_SHOWALLOBJECTS		0x0001
#define SSF_SHOWEXTENSIONS		0x0002
#define SSF_SHOWCOMPCOLOR		0x0008
#define SSF_SHOWSYSFILES		0x0020
#define SSF_DOUBLECLICKINWEBVIEW	0x0080
#define SSF_SHOWATTRIBCOL		0x0100
#define SSF_DESKTOPHTML			0x0200
#define SSF_WIN95CLASSIC		0x0400
#define SSF_DONTPRETTYPATH		0x0800
#define SSF_SHOWINFOTIP			0x2000
#define SSF_MAPNETDRVBUTTON		0x1000
#define SSF_NOCONFIRMRECYCLE		0x8000
#define SSF_HIDEICONS			0x4000

/**********************************************************************
 * SHChangeNotify
 */
#define SHCNE_RENAMEITEM	0x00000001
#define SHCNE_CREATE		0x00000002
#define SHCNE_DELETE		0x00000004
#define SHCNE_MKDIR		0x00000008
#define SHCNE_RMDIR		0x00000010
#define SHCNE_MEDIAINSERTED	0x00000020
#define SHCNE_MEDIAREMOVED	0x00000040
#define SHCNE_DRIVEREMOVED	0x00000080
#define SHCNE_DRIVEADD		0x00000100
#define SHCNE_NETSHARE		0x00000200
#define SHCNE_NETUNSHARE	0x00000400
#define SHCNE_ATTRIBUTES	0x00000800
#define SHCNE_UPDATEDIR		0x00001000
#define SHCNE_UPDATEITEM	0x00002000
#define SHCNE_SERVERDISCONNECT	0x00004000
#define SHCNE_UPDATEIMAGE	0x00008000
#define SHCNE_DRIVEADDGUI	0x00010000
#define SHCNE_RENAMEFOLDER	0x00020000
#define SHCNE_FREESPACE		0x00040000

#define SHCNE_EXTENDED_EVENT	0x04000000
#define SHCNE_ASSOCCHANGED	0x08000000
#define SHCNE_DISKEVENTS	0x0002381F
#define SHCNE_GLOBALEVENTS	0x0C0581E0
#define SHCNE_ALLEVENTS		0x7FFFFFFF
#define SHCNE_INTERRUPT		0x80000000

#define SHCNEE_ORDERCHANGED	0x00000002

#define SHCNF_IDLIST		0x0000
#define SHCNF_PATHA		0x0001
#define SHCNF_PRINTERA		0x0002
#define SHCNF_DWORD		0x0003
#define SHCNF_PATHW		0x0005
#define SHCNF_PRINTERW		0x0006
#define SHCNF_TYPE		0x00FF
#define SHCNF_FLUSH		0x1000
#define SHCNF_FLUSHNOWAIT	0x2000

void WINAPI SHChangeNotifyA(LONG wEventId, UINT uFlags, LPCVOID dwItem1, LPCVOID dwItem2);
void WINAPI SHChangeNotifyW(LONG wEventId, UINT uFlags, LPCVOID dwItem1, LPCVOID dwItem2);
#define  SHChangeNotify WINELIB_NAME_AW(SHChangeNotify)

typedef struct tagDATABLOCKHEADER
{
    DWORD cbSize;
    DWORD dwSignature;
} DATABLOCK_HEADER, *LPDATABLOCK_HEADER, *LPDBLIST;

typedef struct {
    DATABLOCK_HEADER dbh;
    CHAR szDarwinID[MAX_PATH];
    WCHAR szwDarwinID[MAX_PATH];
} EXP_DARWIN_LINK, *LPEXP_DARWIN_LINK;

#define EXP_SZ_LINK_SIG         0xa0000001
#define NT_CONSOLE_PROPS_SIG    0xa0000002
#define NT_FE_CONSOLE_PROPS_SIG 0xa0000004
#define EXP_SPECIAL_FOLDER_SIG  0xa0000005
#define EXP_DARWIN_ID_SIG       0xa0000006
#define EXP_LOGO3_ID_SIG        0xa0000007
#define EXP_SZ_ICON_SIG         0xa0000007


/****************************************************************************
 * SHCreateDirectory API
 */
int WINAPI SHCreateDirectory(HWND hwnd, LPCVOID path);
int WINAPI SHCreateDirectoryExA(HWND hwnd, LPCSTR pszPath, SECURITY_ATTRIBUTES *psa);
int WINAPI SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, SECURITY_ATTRIBUTES *psa);
#define SHCreateDirectoryEx WINELIB_NAME_AW(SHCreateDirectoryEx)

/****************************************************************************
* SHGetSpecialFolderLocation API
*/
HRESULT WINAPI SHGetSpecialFolderLocation(HWND hwndOwner, INT nFolder, LPITEMIDLIST *ppidl);
HRESULT WINAPI SHGetFolderLocation(HWND hwndOwner, INT nFolder, HANDLE hToken, DWORD dwReserved, LPITEMIDLIST *ppidl);

/****************************************************************************
* SHGetFolderPath API
*/
HRESULT WINAPI SHGetFolderPathA(HWND hwnd, INT nFolder, HANDLE hToken, DWORD dwFlags, LPSTR pszPath);
HRESULT WINAPI SHGetFolderPathW(HWND hwnd, INT nFolder, HANDLE hToken, DWORD dwFlags, LPWSTR pszPath);
#define        SHGetFolderPath WINELIB_NAME_AW(SHGetFolderPath)

#define CSIDL_DESKTOP		0x0000
#define CSIDL_INTERNET		0x0001
#define CSIDL_PROGRAMS		0x0002
#define CSIDL_CONTROLS		0x0003
#define CSIDL_PRINTERS		0x0004
#define CSIDL_PERSONAL		0x0005
#define CSIDL_FAVORITES		0x0006
#define CSIDL_STARTUP		0x0007
#define CSIDL_RECENT		0x0008
#define CSIDL_SENDTO		0x0009
#define CSIDL_BITBUCKET		0x000a
#define CSIDL_STARTMENU		0x000b
#define CSIDL_MYDOCUMENTS	0x000c
#define CSIDL_MYMUSIC		0x000d
#define CSIDL_MYVIDEO		0x000e
#define CSIDL_DESKTOPDIRECTORY	0x0010
#define CSIDL_DRIVES		0x0011
#define CSIDL_NETWORK		0x0012
#define CSIDL_NETHOOD		0x0013
#define CSIDL_FONTS		0x0014
#define CSIDL_TEMPLATES		0x0015
#define CSIDL_COMMON_STARTMENU	0x0016
#define CSIDL_COMMON_PROGRAMS	0X0017
#define CSIDL_COMMON_STARTUP	0x0018
#define CSIDL_COMMON_DESKTOPDIRECTORY	0x0019
#define CSIDL_APPDATA		0x001a
#define CSIDL_PRINTHOOD		0x001b
#define CSIDL_LOCAL_APPDATA	0x001c
#define CSIDL_ALTSTARTUP	0x001d
#define CSIDL_COMMON_ALTSTARTUP	0x001e
#define CSIDL_COMMON_FAVORITES  0x001f
#define CSIDL_INTERNET_CACHE	0x0020
#define CSIDL_COOKIES		0x0021
#define CSIDL_HISTORY		0x0022
#define CSIDL_COMMON_APPDATA	0x0023
#define CSIDL_WINDOWS		0x0024
#define CSIDL_SYSTEM		0x0025
#define CSIDL_PROGRAM_FILES	0x0026
#define CSIDL_MYPICTURES	0x0027
#define CSIDL_PROFILE		0x0028
#define CSIDL_SYSTEMX86		0x0029
#define CSIDL_PROGRAM_FILESX86	0x002a
#define CSIDL_PROGRAM_FILES_COMMON	0x002b
#define CSIDL_PROGRAM_FILES_COMMONX86	0x002c
#define CSIDL_COMMON_TEMPLATES	0x002d
#define CSIDL_COMMON_DOCUMENTS	0x002e
#define CSIDL_COMMON_ADMINTOOLS	0x002f
#define CSIDL_ADMINTOOLS	0x0030
#define CSIDL_CONNECTIONS	0x0031
#define CSIDL_COMMON_MUSIC	0x0035
#define CSIDL_COMMON_PICTURES	0x0036
#define CSIDL_COMMON_VIDEO	0x0037
#define CSIDL_RESOURCES		0x0038
#define CSIDL_RESOURCES_LOCALIZED 0x0039
#define CSIDL_COMMON_OEM_LINKS	0x003a
#define CSIDL_CDBURN_AREA	0x003b
#define CSIDL_COMPUTERSNEARME	0x003d
#define CSIDL_FOLDER_MASK	0x00ff
#define CSIDL_FLAG_PER_USER_INIT 0x0800
#define CSIDL_FLAG_NO_ALIAS	0x1000
#define CSIDL_FLAG_DONT_VERIFY	0x4000
#define CSIDL_FLAG_CREATE	0x8000

#define CSIDL_FLAG_MASK		0xff00

/*
 * DROPFILES for CF_HDROP and CF_PRINTERS
 */
typedef struct _DROPFILES
{
  DWORD pFiles;
  POINT pt;
  BOOL  fNC;
  BOOL  fWide;
} DROPFILES;

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* __WINE_SHLOBJ_H */
