/*
 * Path Functions
 *
 * Many of this functions are in SHLWAPI.DLL also
 *
 */
#include <string.h>
#include <ctype.h>
#include "wine/debug.h"
#include "winbase.h"
#include "winuser.h"
#include "windef.h"
#include "winnls.h"
#include "winreg.h"

#include "shlobj.h"
#include "shell32_main.h"
#include "undocshell.h"
#include "wine/unicode.h"
#include "shlwapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

/*
	########## Combining and Constructing paths ##########
*/

/*************************************************************************
 * PathAppend		[SHELL32.36]
 */
BOOL WINAPI PathAppendAW(
	LPVOID lpszPath1,
	LPCVOID lpszPath2)
{
	if (SHELL_OsIsUnicode())
	  return PathAppendW(lpszPath1, lpszPath2);
	return PathAppendA(lpszPath1, lpszPath2);
}

/*************************************************************************
 * PathCombine	 [SHELL32.37]
 */
LPVOID WINAPI PathCombineAW(
	LPVOID szDest,
	LPCVOID lpszDir,
	LPCVOID lpszFile)
{
	if (SHELL_OsIsUnicode())
	  return PathCombineW( szDest, lpszDir, lpszFile );
	return PathCombineA( szDest, lpszDir, lpszFile );
}

/*************************************************************************
 * PathAddBackslash		[SHELL32.32]
 */
LPVOID WINAPI PathAddBackslashAW(LPVOID lpszPath)
{
	if(SHELL_OsIsUnicode())
	  return PathAddBackslashW(lpszPath);
	return PathAddBackslashA(lpszPath);
}

/*************************************************************************
 * PathBuildRoot		[SHELL32.30]
 */
LPVOID WINAPI PathBuildRootAW(LPVOID lpszPath, int drive)
{
	if(SHELL_OsIsUnicode())
	  return PathBuildRootW(lpszPath, drive);
	return PathBuildRootA(lpszPath, drive);
}

/*
	Extracting Component Parts
*/

/*************************************************************************
 * PathFindFileName	[SHELL32.34]
 */
LPVOID WINAPI PathFindFileNameAW(LPCVOID lpszPath)
{
	if(SHELL_OsIsUnicode())
	  return PathFindFileNameW(lpszPath);
	return PathFindFileNameA(lpszPath);
}

/*************************************************************************
 * PathFindExtension		[SHELL32.31]
 */
LPVOID WINAPI PathFindExtensionAW(LPCVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathFindExtensionW(lpszPath);
	return PathFindExtensionA(lpszPath);

}

/*************************************************************************
 * PathGetExtensionA		[internal]
 *
 * NOTES
 *  exported by ordinal
 *  return value points to the first char after the dot
 */
static LPSTR PathGetExtensionA(LPCSTR lpszPath)
{
	TRACE("(%s)\n",lpszPath);

	lpszPath = PathFindExtensionA(lpszPath);
	return (LPSTR)(*lpszPath?(lpszPath+1):lpszPath);
}

/*************************************************************************
 * PathGetExtensionW		[internal]
 */
static LPWSTR PathGetExtensionW(LPCWSTR lpszPath)
{
	TRACE("(%s)\n",debugstr_w(lpszPath));

	lpszPath = PathFindExtensionW(lpszPath);
	return (LPWSTR)(*lpszPath?(lpszPath+1):lpszPath);
}

/*************************************************************************
 * PathGetExtension		[SHELL32.158]
 */
LPVOID WINAPI PathGetExtensionAW(LPCVOID lpszPath,DWORD void1, DWORD void2)
{
	if (SHELL_OsIsUnicode())
	  return PathGetExtensionW(lpszPath);
	return PathGetExtensionA(lpszPath);
}

/*************************************************************************
 * PathGetArgs	[SHELL32.52]
 */
LPVOID WINAPI PathGetArgsAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathGetArgsW(lpszPath);
	return PathGetArgsA(lpszPath);
}

/*************************************************************************
 * PathGetDriveNumber	[SHELL32.57]
 */
int WINAPI PathGetDriveNumberAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathGetDriveNumberW(lpszPath);
	return PathGetDriveNumberA(lpszPath);
}

/*************************************************************************
 * PathRemoveFileSpec [SHELL32.35]
 */
BOOL WINAPI PathRemoveFileSpecAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathRemoveFileSpecW(lpszPath);
	return PathRemoveFileSpecA(lpszPath);
}

/*************************************************************************
 * PathStripPath	[SHELL32.38]
 */
void WINAPI PathStripPathAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
            PathStripPathW(lpszPath);
        else
            PathStripPathA(lpszPath);
}

/*************************************************************************
 * PathStripToRoot	[SHELL32.50]
 */
BOOL WINAPI PathStripToRootAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathStripToRootW(lpszPath);
	return PathStripToRootA(lpszPath);
}

/*************************************************************************
 * PathRemoveArgs	[SHELL32.251]
 */
void WINAPI PathRemoveArgsAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
            PathRemoveArgsW(lpszPath);
        else
            PathRemoveArgsA(lpszPath);
}

/*************************************************************************
 * PathRemoveExtension	[SHELL32.250]
 */
void WINAPI PathRemoveExtensionAW(LPVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
            PathRemoveExtensionW(lpszPath);
        else
            PathRemoveExtensionA(lpszPath);
}


/*
	Path Manipulations
*/

/*************************************************************************
 * PathGetShortPathA [internal]
 */
LPSTR WINAPI PathGetShortPathA(LPSTR lpszPath)
{
	FIXME("%s stub\n", lpszPath);
	return NULL;
}

/*************************************************************************
 * PathGetShortPathW [internal]
 */
LPWSTR WINAPI PathGetShortPathW(LPWSTR lpszPath)
{
	FIXME("%s stub\n", debugstr_w(lpszPath));
	return NULL;
}

/*************************************************************************
 * PathGetShortPath [SHELL32.92]
 */
LPVOID WINAPI PathGetShortPathAW(LPVOID lpszPath)
{
	if(SHELL_OsIsUnicode())
	  return PathGetShortPathW(lpszPath);
	return PathGetShortPathA(lpszPath);
}

/*************************************************************************
 * PathRemoveBlanks [SHELL32.33]
 */
void WINAPI PathRemoveBlanksAW(LPVOID str)
{
	if(SHELL_OsIsUnicode())
            PathRemoveBlanksW(str);
        else
            PathRemoveBlanksA(str);
}

/*************************************************************************
 * PathQuoteSpaces [SHELL32.55]
 */
VOID WINAPI PathQuoteSpacesAW (LPVOID lpszPath)
{
	if(SHELL_OsIsUnicode())
            PathQuoteSpacesW(lpszPath);
        else
            PathQuoteSpacesA(lpszPath);
}

/*************************************************************************
 * PathUnquoteSpaces [SHELL32.56]
 */
VOID WINAPI PathUnquoteSpacesAW(LPVOID str)
{
	if(SHELL_OsIsUnicode())
	  PathUnquoteSpacesW(str);
	else
	  PathUnquoteSpacesA(str);
}

/*************************************************************************
 * PathParseIconLocation	[SHELL32.249]
 */
int WINAPI PathParseIconLocationAW (LPVOID lpszPath)
{
	if(SHELL_OsIsUnicode())
	  return PathParseIconLocationW(lpszPath);
	return PathParseIconLocationA(lpszPath);
}

/*
	########## Path Testing ##########
*/
/*************************************************************************
 * PathIsUNC		[SHELL32.39]
 */
BOOL WINAPI PathIsUNCAW (LPCVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathIsUNCW( lpszPath );
	return PathIsUNCA( lpszPath );
}

/*************************************************************************
 *  PathIsRelative	[SHELL32.40]
 */
BOOL WINAPI PathIsRelativeAW (LPCVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathIsRelativeW( lpszPath );
	return PathIsRelativeA( lpszPath );
}

/*************************************************************************
 * PathIsRoot		[SHELL32.29]
 */
BOOL WINAPI PathIsRootAW(LPCVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathIsRootW(lpszPath);
	return PathIsRootA(lpszPath);
}

/*************************************************************************
 *  PathIsExeA		[internal]
 */
static BOOL PathIsExeA (LPCSTR lpszPath)
{
	LPCSTR lpszExtension = PathGetExtensionA(lpszPath);
	int i = 0;
	static char * lpszExtensions[6] = {"exe", "com", "pid", "cmd", "bat", NULL };

	TRACE("path=%s\n",lpszPath);

	for(i=0; lpszExtensions[i]; i++)
	  if (!strcasecmp(lpszExtension,lpszExtensions[i])) return TRUE;

	return FALSE;
}

/*************************************************************************
 *  PathIsExeW		[internal]
 */
static BOOL PathIsExeW (LPCWSTR lpszPath)
{
	LPCWSTR lpszExtension = PathGetExtensionW(lpszPath);
	int i = 0;
	static WCHAR lpszExtensions[6][4] =
	  {{'e','x','e','\0'}, {'c','o','m','\0'}, {'p','i','d','\0'},
	   {'c','m','d','\0'}, {'b','a','t','\0'}, {'\0'} };

	TRACE("path=%s\n",debugstr_w(lpszPath));

	for(i=0; lpszExtensions[i][0]; i++)
	  if (!strcmpiW(lpszExtension,lpszExtensions[i])) return TRUE;

	return FALSE;
}

/*************************************************************************
 *  PathIsExe		[SHELL32.43]
 */
BOOL WINAPI PathIsExeAW (LPCVOID path)
{
	if (SHELL_OsIsUnicode())
	  return PathIsExeW (path);
	return PathIsExeA(path);
}

/*************************************************************************
 * PathIsDirectory	[SHELL32.159]
 */
BOOL WINAPI PathIsDirectoryAW (LPCVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathIsDirectoryW (lpszPath);
	return PathIsDirectoryA (lpszPath);
}

/*************************************************************************
 * PathFileExists	[SHELL32.45]
 */
BOOL WINAPI PathFileExistsAW (LPCVOID lpszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathFileExistsW (lpszPath);
	return PathFileExistsA (lpszPath);
}

/*************************************************************************
 * PathMatchSpec	[SHELL32.46]
 */
BOOL WINAPI PathMatchSpecAW(LPVOID name, LPVOID mask)
{
	if (SHELL_OsIsUnicode())
	  return PathMatchSpecW( name, mask );
	return PathMatchSpecA( name, mask );
}

/*************************************************************************
 * PathIsSameRoot	[SHELL32.650]
 */
BOOL WINAPI PathIsSameRootAW(LPCVOID lpszPath1, LPCVOID lpszPath2)
{
	if (SHELL_OsIsUnicode())
	  return PathIsSameRootW(lpszPath1, lpszPath2);
	return PathIsSameRootA(lpszPath1, lpszPath2);
}

/*************************************************************************
 * IsLFNDrive		[SHELL32.119]
 *
 * NOTES
 *     exported by ordinal Name
 */
BOOL WINAPI IsLFNDriveA(LPCSTR lpszPath)
{
    DWORD	fnlen;

    if (!GetVolumeInformationA(lpszPath,NULL,0,NULL,&fnlen,NULL,NULL,0))
	return FALSE;
    return fnlen>12;
}

/*
	########## Creating Something Unique ##########
*/
/*************************************************************************
 * PathMakeUniqueNameA	[internal]
 */
BOOL WINAPI PathMakeUniqueNameA(
	LPSTR lpszBuffer,
	DWORD dwBuffSize,
	LPCSTR lpszShortName,
	LPCSTR lpszLongName,
	LPCSTR lpszPathName)
{
	FIXME("%p %lu %s %s %s stub\n",
	 lpszBuffer, dwBuffSize, debugstr_a(lpszShortName),
	 debugstr_a(lpszLongName), debugstr_a(lpszPathName));
	return TRUE;
}

/*************************************************************************
 * PathMakeUniqueNameW	[internal]
 */
BOOL WINAPI PathMakeUniqueNameW(
	LPWSTR lpszBuffer,
	DWORD dwBuffSize,
	LPCWSTR lpszShortName,
	LPCWSTR lpszLongName,
	LPCWSTR lpszPathName)
{
	FIXME("%p %lu %s %s %s stub\n",
	 lpszBuffer, dwBuffSize, debugstr_w(lpszShortName),
	 debugstr_w(lpszLongName), debugstr_w(lpszPathName));
	return TRUE;
}

/*************************************************************************
 * PathMakeUniqueName	[SHELL32.47]
 */
BOOL WINAPI PathMakeUniqueNameAW(
	LPVOID lpszBuffer,
	DWORD dwBuffSize,
	LPCVOID lpszShortName,
	LPCVOID lpszLongName,
	LPCVOID lpszPathName)
{
	if (SHELL_OsIsUnicode())
	  return PathMakeUniqueNameW(lpszBuffer,dwBuffSize, lpszShortName,lpszLongName,lpszPathName);
	return PathMakeUniqueNameA(lpszBuffer,dwBuffSize, lpszShortName,lpszLongName,lpszPathName);
}

/*************************************************************************
 * PathYetAnotherMakeUniqueName [SHELL32.75]
 *
 * NOTES
 *     exported by ordinal
 */
BOOL WINAPI PathYetAnotherMakeUniqueNameA(
	LPSTR lpszBuffer,
	LPCSTR lpszPathName,
	LPCSTR lpszShortName,
	LPCSTR lpszLongName)
{
    FIXME("(%p,%p, %p ,%p):stub.\n",
     lpszBuffer, lpszPathName, lpszShortName, lpszLongName);
    return TRUE;
}


/*
	########## cleaning and resolving paths ##########
 */

/*************************************************************************
 * PathFindOnPath	[SHELL32.145]
 */
BOOL WINAPI PathFindOnPathAW(LPVOID sFile, LPCVOID sOtherDirs)
{
	if (SHELL_OsIsUnicode())
	  return PathFindOnPathW(sFile, (LPCWSTR *)sOtherDirs);
	return PathFindOnPathA(sFile, (LPCSTR *)sOtherDirs);
}

/*************************************************************************
 * PathCleanupSpec	[SHELL32.171]
 */
DWORD WINAPI PathCleanupSpecAW (LPCVOID x, LPVOID y)
{
    FIXME("(%p, %p) stub\n",x,y);
    return TRUE;
}

/*************************************************************************
 * PathQualifyA		[SHELL32]
 */
BOOL WINAPI PathQualifyA(LPCSTR pszPath)
{
	FIXME("%s\n",pszPath);
	return 0;
}

/*************************************************************************
 * PathQualifyW		[SHELL32]
 */
BOOL WINAPI PathQualifyW(LPCWSTR pszPath)
{
	FIXME("%s\n",debugstr_w(pszPath));
	return 0;
}

/*************************************************************************
 * PathQualify	[SHELL32.49]
 */
BOOL WINAPI PathQualifyAW(LPCVOID pszPath)
{
	if (SHELL_OsIsUnicode())
	  return PathQualifyW(pszPath);
	return PathQualifyA(pszPath);
}

/*************************************************************************
 * PathResolveA [SHELL32.51]
 */
BOOL WINAPI PathResolveA(
	LPSTR lpszPath,
	LPCSTR *alpszPaths,
	DWORD dwFlags)
{
	FIXME("(%s,%p,0x%08lx),stub!\n",
	  lpszPath, *alpszPaths, dwFlags);
	return 0;
}

/*************************************************************************
 * PathResolveW [SHELL32]
 */
BOOL WINAPI PathResolveW(
	LPWSTR lpszPath,
	LPCWSTR *alpszPaths,
	DWORD dwFlags)
{
	FIXME("(%s,%p,0x%08lx),stub!\n",
	  debugstr_w(lpszPath), debugstr_w(*alpszPaths), dwFlags);
	return 0;
}

/*************************************************************************
 * PathResolve [SHELL32.51]
 */
BOOL WINAPI PathResolveAW(
	LPVOID lpszPath,
	LPCVOID *alpszPaths,
	DWORD dwFlags)
{
	if (SHELL_OsIsUnicode())
	  return PathResolveW(lpszPath, (LPCWSTR*)alpszPaths, dwFlags);
	return PathResolveA(lpszPath, (LPCSTR*)alpszPaths, dwFlags);
}

/*************************************************************************
*	PathProcessCommandA	[SHELL32.653]
*/
HRESULT WINAPI PathProcessCommandA (
	LPCSTR lpszPath,
	LPSTR lpszBuff,
	DWORD dwBuffSize,
	DWORD dwFlags)
{
	FIXME("%s %p 0x%04lx 0x%04lx stub\n",
	lpszPath, lpszBuff, dwBuffSize, dwFlags);
	strcpy(lpszBuff, lpszPath);
	return 0;
}

/*************************************************************************
*	PathProcessCommandW
*/
HRESULT WINAPI PathProcessCommandW (
	LPCWSTR lpszPath,
	LPWSTR lpszBuff,
	DWORD dwBuffSize,
	DWORD dwFlags)
{
	FIXME("(%s, %p, 0x%04lx, 0x%04lx) stub\n",
	debugstr_w(lpszPath), lpszBuff, dwBuffSize, dwFlags);
	strcpyW(lpszBuff, lpszPath);
	return 0;
}

/*************************************************************************
*	PathProcessCommand (SHELL32.653)
*/
HRESULT WINAPI PathProcessCommandAW (
	LPCVOID lpszPath,
	LPVOID lpszBuff,
	DWORD dwBuffSize,
	DWORD dwFlags)
{
	if (SHELL_OsIsUnicode())
	  return PathProcessCommandW(lpszPath, lpszBuff, dwBuffSize, dwFlags);
	return PathProcessCommandA(lpszPath, lpszBuff, dwBuffSize, dwFlags);
}

/*
	########## special ##########
*/

/*************************************************************************
 * PathSetDlgItemPath (SHELL32.48)
 */
VOID WINAPI PathSetDlgItemPathAW(HWND hDlg, int id, LPCVOID pszPath)
{
	if (SHELL_OsIsUnicode())
            PathSetDlgItemPathW(hDlg, id, pszPath);
        else
            PathSetDlgItemPathA(hDlg, id, pszPath);
}


/*************************************************************************
 * SHGetSpecialFolderPathA [SHELL32.@]
 *
 * converts csidl to path
 */

static const char * const szSHFolders = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
static const char * const szSHUserFolders = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders";
static const char * const szSetup = "Software\\Microsoft\\Windows\\CurrentVersion\\Setup";
static const char * const szCurrentVersion = "Software\\Microsoft\\Windows\\CurrentVersion";
#if 0
static const char * const szEnvUserProfile = "%USERPROFILE%";
static const char * const szEnvSystemRoot = "%SYSTEMROOT%";
#endif

typedef struct
{
    DWORD dwFlags;
    HKEY hRootKey;
    LPCSTR szValueName;
    LPCSTR szDefaultPath; /* fallback string; sub dir of windows directory */
} CSIDL_DATA;


#define CSIDL_MYFLAG_SHFOLDER   0x01
#define CSIDL_MYFLAG_SETUP      0x02
#define CSIDL_MYFLAG_CURRVER    0x04
#define CSIDL_MYFLAG_SHUSERTOO  0x08

/* these next flags are mutually exclusive! */
#define CSIDL_MYFLAG_RELATIVE   0x10
#define CSIDL_MYFLAG_USERPREFS  0x20
#define CSIDL_MYFLAG_ALLPREFS   0x40


#define HKLM HKEY_LOCAL_MACHINE
#define HKCU HKEY_CURRENT_USER
static const CSIDL_DATA CSIDL_Data[] =
{
    { /* CSIDL_DESKTOP */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Desktop",
        "Desktop"
    },
    { /* CSIDL_INTERNET */
      /* WindowsXP doesn't report any folder for this ID.
         Supposed to be a virtual folder for Internet Explorer. */
        0, 1,
        NULL,
        NULL,
    },
    { /* CSIDL_PROGRAMS */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Programs",
        "Start Menu\\Programs"
    },
    { /* CSIDL_CONTROLS (.CPL files) */
      /* WindowsXP doesn't report anything but since it's used in our MSI 
         implementation we haven't changed it. Supposed to be the virtual 
         folder that contains icons for the Control Panel applications.*/
        CSIDL_MYFLAG_RELATIVE | CSIDL_MYFLAG_SETUP, HKLM, 
        "SysDir", 
        "SYSTEM"
    },
    { /* CSIDL_PRINTERS */
      /* WindowsXP doesn't report anything but since it's used in our MSI 
         implementation we haven't changed it. The virtual folder that contains
         installed printers.*/
        CSIDL_MYFLAG_RELATIVE | CSIDL_MYFLAG_SETUP, HKLM,
        "SysDir",
        "SYSTEM"
    },
    { /* CSIDL_PERSONAL */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "Personal",
        "My Documents"
    },
    { /* CSIDL_FAVORITES */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Favorites",
        "Favorites"
    },
    { /* CSIDL_STARTUP */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "StartUp",
        "Start Menu\\Programs\\Startup"
    },
    { /* CSIDL_RECENT */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "Recent",
        "Recent"
    },
    { /* CSIDL_SENDTO */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "SendTo",
        "SendTo"
    },
    { /* CSIDL_BITBUCKET */
      /* WindowsXP doesn't report any folder for this ID.
        The virtual folder that contains the objects in the user's Recycle Bin. */
        0, 1,
        NULL,
        "recycled"
    },
    { /* CSIDL_STARTMENU */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Start Menu",
        "Start Menu"
    },
    { /* CSIDL_MYDOCUMENTS */
      /* WindowsXP did not report any folder for this ID, but MSDN says Ver 6.0. 
         The virtual folder that represents the My Documents desktop item. This 
         value is equivalent to CSIDL_PERSONAL. */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "My Documents",
        "My Documents"
    },
    { /* CSIDL_MYMUSIC */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "My Music",
        "My Documents\\My Music",
    },
    { /* CSIDL_MYVIDEO */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "My Video",
        "My Documents\\My Video"
    },
    { /* 0x000F ID is unassigned */
        0, 0,
        NULL,
        NULL,
    },
    { /* CSIDL_DESKTOPDIRECTORY */
      /* MSDN says C:\Documents and Settings\username\Desktop would be a typical path. */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Desktop",
        "Desktop"
    },
    { /* CSIDL_DRIVES */
      /* WindowsXP did not report any folder for this ID, but since this is supposed to be
         the virtual folder that represents My Computer this setup is OK */
        0, 1,
        NULL,
        "My Computer"
    },
    { /* CSIDL_NETWORK */
      /* WindowsXP did not report any folder for this ID, but since this is supposed to be
         the virtual folder that represents Network Neighborhood, this setup is OK */
        0, 1,
        NULL,
        "Network Neighborhood"
    },
    { /* CSIDL_NETHOOD */
      /* MSDN says a typical path is C:\Documents and Settings\username\NetHood. */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "NetHood",
        "NetHood"
    },
    { /* CSIDL_FONTS */
        CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_RELATIVE, HKCU,
        "Fonts",
        "Fonts"
    },
    { /* CSIDL_TEMPLATES */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Templates",
        "Templates"
    },
    { /* CSIDL_COMMON_STARTMENU */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common Start Menu",
        "Start Menu"
    },
    { /* CSIDL_COMMON_PROGRAMS */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common Programs",
        "Start Menu\\Programs"
    },
    { /* CSIDL_COMMON_STARTUP */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common StartUp",
        "Start Menu\\Programs\\Startup"
    },
    { /* CSIDL_COMMON_DESKTOPDIRECTORY */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common Desktop",
        "Desktop"
    },
    { /* CSIDL_APPDATA */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "AppData",
        "Application Data"
    },
    { /* CSIDL_PRINTHOOD */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "PrintHood",
        "PrintHood"
    },
    { /* CSIDL_LOCAL_APPDATA */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Local AppData",
        "Local Settings\\Application Data",
    },
    { /* CSIDL_ALTSTARTUP */
      /* The file system directory that corresponds to the user's nonlocalized Startup program group. */
        0, 1,
        NULL,
        NULL
    },
    { /* CSIDL_COMMON_ALTSTARTUP */
      /* The file system directory that corresponds to the nonlocalized Startup program group for all users.*/
        0, 1,
        NULL,
        NULL
    },
    { /* CSIDL_COMMON_FAVORITES */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER, HKLM,
        "Favorites",
        "Favorites"
    },
    { /* CSIDL_INTERNET_CACHE */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Cache",
        "Local Settings\\Temporary Internet Files"
    },
    { /* CSIDL_COOKIES */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "Cookies",
        "Cookies"
    },
    { /* CSIDL_HISTORY */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKCU,
        "History",
        "Local Settings\\History"
    },
    { /* CSIDL_COMMON_APPDATA */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common AppData",
        "Application Data"
    },
    { /* CSIDL_WINDOWS */
        CSIDL_MYFLAG_SETUP, HKLM,
        "WinDir",
        "WINDOWS"
    },
    { /* CSIDL_SYSTEM */
        CSIDL_MYFLAG_RELATIVE | CSIDL_MYFLAG_SETUP, HKLM,
        "SysDir",
        "system32"
    },
    { /* CSIDL_PROGRAM_FILES */
        CSIDL_MYFLAG_CURRVER, HKLM,
        "ProgramFilesDir",
        "Program Files"
    },
    { /* CSIDL_MYPICTURES */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "My Pictures",
        "My Documents\\My Pictures"
    },
    { /* CSIDL_PROFILE */
      /* It uses the "User" unsername hardcoded for now, that can be changed later. */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "WinDir",
        ""    /* FIXME - sync with %USERPROFILE% */
    },
    { /* CSIDL_SYSTEMX86 */
      /* Similar with CSIDL_SYSTEM */
        CSIDL_MYFLAG_RELATIVE | CSIDL_MYFLAG_SETUP, HKLM,
        "SysDir",
        "system32"
    },
    { /* CSIDL_PROGRAM_FILESX86 */
      /* WindowsXP doesn't report any folder for this ID, but we can make it identical with CSIDL_PROGRAM_FILES */
        CSIDL_MYFLAG_CURRVER, HKLM,
        "ProgramFilesDir",
        "Program Files"
    },
    { /* CSIDL_PROGRAM_FILES_COMMON */
      /* MSDN says a typical path is C:\Program Files\Common */
        CSIDL_MYFLAG_CURRVER, HKLM,
        "CommonFilesDir",
        "Program Files\\Common Files"
    },
    { /* CSIDL_PROGRAM_FILES_COMMONX86 */
      /* Identical with CSIDL_PROGRAM_FILES_COMMON */
        CSIDL_MYFLAG_CURRVER, HKLM,
        "CommonFilesDir",
        "Program Files\\Common Files"
    },
    { /* CSIDL_COMMON_TEMPLATES */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common Templates",
        "Templates"
    },
    { /* CSIDL_COMMON_DOCUMENTS */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER | CSIDL_MYFLAG_SHUSERTOO, HKLM,
        "Common Documents",
        "Documents"
    },
    { /* CSIDL_COMMON_ADMINTOOLS */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER, HKLM,
        "AdminTools",
        "Start Menu\\Programs\\Administrative Tools"
    },
    { /* CSIDL_ADMINTOOLS */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "Administrative Tools",
        "Start Menu\\Programs\\Administrative Tools"
    },
    { /* CSIDL_CONNECTIONS */
      /* WindowsXP did not report any folder for this ID. MSDN says is the virtual folder that 
         represents Network Connections, that contains network and dial-up connections. */
        0, 1,
        NULL,
        NULL,
    },
    { /* 0x0032 ID is unassigned */
        0, 0,
        NULL,
        NULL,
    },
    { /* 0x0033 ID is unassigned */
        0, 0,
        NULL,
        NULL,
    },
    { /* 0x0034 ID is unassigned */
        0, 0,
        NULL,
        NULL,
    },
    { /* CSIDL_COMMON_MUSIC */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER, HKLM,
        "Music",
        "Documents\\My Music"
    },
    { /* CSIDL_COMMON_PICTURES */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER, HKLM,
        "Pictures",
        "Documents\\My Pictures"
    },
    { /* CSIDL_COMMON_VIDEO */
        CSIDL_MYFLAG_ALLPREFS | CSIDL_MYFLAG_SHFOLDER, HKLM,
        "Videos",
        "Documents\\My Videos"
    },
    { /* CSIDL_RESOURCES */
        CSIDL_MYFLAG_RELATIVE, HKLM,
        "",
        "resources"
    },
    { /* CSIDL_RESOURCES_LOCALIZED */
      /* WindowsXP did not report any folder for this ID. */
        0, 0,
        NULL,
        NULL,
    },
    { /* CSIDL_COMMON_OEM_LINKS */
      /* WindowsXP did not report any folder for this ID. */
        0, 0,
        NULL,
        NULL,
    },
    { /* CSIDL_CDBURN_AREA */
        CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_SHFOLDER, HKCU,
        "CD Burning",
        "Local Settings\\Application Data\\Microsoft\\CD Burning"
    },
    { /* 0x003C ID is unassigned */
        0, 0,
        NULL,
        NULL,
    },
    { /* CSIDL_COMPUTERSNEARME */
      /* The folder that represents other computers in your workgroup. */
        0, 0,
        NULL,
        NULL,
    }
};
#undef HKCU
#undef HKLM

/**********************************************************************
 * Common routine to build a folder path and set it in the registry.  The
 * path built it also returned in the szPath parameter.
 *
 * Returns: TRUE on success, FALSE on an error
 */

static BOOL BuildAndSetFolderPath(
    HKEY hKey,
    DWORD dwFlags,
    DWORD csidl,
    LPSTR szPath)
{
    if (dwFlags & CSIDL_MYFLAG_RELATIVE)
    {
        if (!GetWindowsDirectoryA(szPath, MAX_PATH))
            return FALSE;
        PathAddBackslashA(szPath);
    }
#ifdef __APPLE__
    /* On the Apple, user prefs are stored on P:\ which gets
       mapped into the user's preferences folder */
    else if ((dwFlags & (CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_ALLPREFS)) &&
             PathIsDirectoryA ("P:\\"))
    {
        if (dwFlags & CSIDL_MYFLAG_USERPREFS)
            strcpy (szPath, "P:\\User\\");
        else if (dwFlags & CSIDL_MYFLAG_ALLPREFS)
            strcpy (szPath, "P:\\All Users\\");
    }
#endif
    else
    {
        strcpy(szPath, "C:\\");
        if (dwFlags & CSIDL_MYFLAG_USERPREFS)
            strcat (szPath, "Documents and Settings\\User\\");
        else if (dwFlags & CSIDL_MYFLAG_ALLPREFS)
            strcat (szPath, "Documents and Settings\\All Users\\");
    }
    if (CSIDL_Data[csidl].szDefaultPath[0])
        strcat(szPath, CSIDL_Data[csidl].szDefaultPath);
    else
    {
       size_t Len;

       /* Remove trailing backslash, as paths from this set of shell functions
          shouldn't have them */
       Len = strlen (szPath);
       if (szPath[Len - 1] == '\\')
          szPath[Len - 1] = 0;
    }

    if (hKey && RegSetValueExA(hKey,CSIDL_Data[csidl].szValueName,0,REG_SZ,
                               (LPBYTE)szPath,strlen(szPath)+1))
    {
        return FALSE;
    }

    SHCreateDirectoryExA (NULL, szPath, NULL);
    return TRUE;
}

/**********************************************************************
 * Shellpath_Init
 *
 * Initializes the shell folder registry keys for the current user using the
 * default values if they don't already exist in the registry.  This is done
 * since some older programs, such as Blizzard installers, check for the
 * registry keys without ever calling SHGetSpecialFolder.
 */
void Shellpath_Init(void)
{
    CHAR  szPath[MAX_PATH];
    DWORD folder;
    HKEY  hKeyUser,     hKeyLM;         /* 'Shell Folder' key */
    HKEY  hKeyUserUser, hKeyLMUser;     /* 'User Shell Folder' key */
    DWORD dwFlags;
    DWORD dwType, dwDisp;


    /* create/open the Shell Folders registry key for the current user */
    if (RegCreateKeyExA(HKEY_CURRENT_USER,szSHFolders,0,NULL,0,KEY_ALL_ACCESS,
                        NULL,&hKeyUser,&dwDisp))
    {
        ERR("failed to create HKEY_CURRENT_USER Shell Folder registry key\n");
        return;
    }
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,szSHFolders,0,NULL,0,KEY_ALL_ACCESS,
                        NULL,&hKeyLM,&dwDisp))
    {
        ERR("failed to create HKEY_LOCAL_MACHINE Shell Folder registry key\n");
        return;
    }

    /* create/open the Shell Folders registry key for the current user */
    if (RegCreateKeyExA(HKEY_CURRENT_USER,szSHUserFolders,0,NULL,0,KEY_ALL_ACCESS,
                        NULL,&hKeyUserUser,&dwDisp))
    {
        ERR("failed to create HKEY_CURRENT_USER User Shell Folder registry key\n");
        return;
    }
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,szSHUserFolders,0,NULL,0,KEY_ALL_ACCESS,
                        NULL,&hKeyLMUser,&dwDisp))
    {
        ERR("failed to create HKEY_LOCAL_MACHINE User Shell Folder registry key\n");
        return;
    }



    /* loop over the CSIDL_Data entries looking for entries to consider for
       making default entries into the registry */
    for (folder = 0; folder <= CSIDL_COMPUTERSNEARME; folder++)
    {
        dwFlags = CSIDL_Data[folder].dwFlags;

        /* entries for user preferences are the ones that may need defaults
           entered into the registry */
        if ((dwFlags & CSIDL_MYFLAG_SHFOLDER)
            && CSIDL_Data[folder].szValueName 
            && CSIDL_Data[folder].szDefaultPath)
        {
            HKEY     pKey[2];
            int      keyCount = 0;
            int      i;
            DWORD    size;


            if (CSIDL_Data[folder].hRootKey == HKEY_CURRENT_USER){
               pKey[keyCount++] = hKeyUser;

               if (dwFlags & CSIDL_MYFLAG_SHUSERTOO)
                   pKey[keyCount++] = hKeyUserUser;
            }

            else if (CSIDL_Data[folder].hRootKey == HKEY_LOCAL_MACHINE){
               pKey[keyCount++] = hKeyLM;

               if (dwFlags & CSIDL_MYFLAG_SHUSERTOO)
                   pKey[keyCount++] = hKeyLMUser;
            }

            else
               continue;


            for (i = 0; i < keyCount; i++){
                size = sizeof(szPath);

                /* either the registry key is missing or the corresponding folder doesn't exist => create both */
                if (RegQueryValueExA(pKey[i], CSIDL_Data[folder].szValueName, NULL,
                                     &dwType, (BYTE *)szPath, &size) != ERROR_SUCCESS ||
                    !PathFileExistsA(szPath))
                {
                    /* value does not exist, so add it */
                    if (!BuildAndSetFolderPath(pKey[i], dwFlags, folder, szPath))
                    {
                        /* setting the value failed, so issue a warning message
                           but continue since it likely isn't critical if this
                           fails */
                        ERR("failed to initialze registry setting for CSIDL %ld\n", folder);
                        break;
                    }
                }
            }
        }
    }

    RegCloseKey(hKeyUser);
    RegCloseKey(hKeyLM);
    RegCloseKey(hKeyUserUser);
    RegCloseKey(hKeyLMUser);
}

/**********************************************************************/

BOOL WINAPI SHGetSpecialFolderPathA (
	HWND hwndOwner,
	LPSTR szPath,
	DWORD csidl,
	BOOL bCreate)
{
	CHAR	szValueName[MAX_PATH], szDefaultPath[MAX_PATH], szBuildPath[MAX_PATH];
	HKEY	hRootKey, hKey;
	DWORD	dwFlags;
	DWORD	dwType, dwDisp, dwPathLen = MAX_PATH;
	DWORD	folder = csidl & CSIDL_FOLDER_MASK;
	CHAR	*p;

	TRACE("0x%04x,%p,csidl=%lu,0x%04x\n", hwndOwner,szPath,csidl,bCreate);

	if ((folder > CSIDL_COMPUTERSNEARME) || (CSIDL_Data[folder].hRootKey == 0))
	{
	    ERR("folder unknown or not allowed\n");
	    return FALSE;
	}
	if (CSIDL_Data[folder].hRootKey == 1)
	{
	    FIXME("folder unknown, please add.\n");
	    return FALSE;
	}

	dwFlags = CSIDL_Data[folder].dwFlags;
	hRootKey = CSIDL_Data[folder].hRootKey;
	strcpy(szValueName, CSIDL_Data[folder].szValueName);
	strcpy(szDefaultPath, CSIDL_Data[folder].szDefaultPath);

	if (dwFlags & CSIDL_MYFLAG_SHFOLDER)
	{
	  /*   user shell folders */
	  if   (RegCreateKeyExA(hRootKey,szSHUserFolders,0,NULL,0,KEY_ALL_ACCESS,NULL,&hKey,&dwDisp)) return FALSE;

	  if   (RegQueryValueExA(hKey,szValueName,NULL,&dwType,(LPBYTE)szPath,&dwPathLen))
	  {
	    RegCloseKey(hKey);

	    /* shell folders */
	    if (RegCreateKeyExA(hRootKey,szSHFolders,0,NULL,0,KEY_ALL_ACCESS,NULL,&hKey,&dwDisp)) return FALSE;

	    if (RegQueryValueExA(hKey,szValueName,NULL,&dwType,(LPBYTE)szPath,&dwPathLen))
	    {
	      /* value not existing */
	      if (!BuildAndSetFolderPath(hKey, dwFlags, folder, szPath))
	      {
	        ERR("Failed to build folder path for CSIDL %ld\n", folder);
	        RegCloseKey(hKey);
	        return FALSE;
	      }
	    }
	  }
	  RegCloseKey(hKey);
        }
	else
	{
          LPCSTR pRegPath = 0;
          BOOL NeedToCreate = FALSE;

          if (dwFlags & CSIDL_MYFLAG_SETUP)
             pRegPath = szSetup;
          else if (dwFlags & CSIDL_MYFLAG_CURRVER)
             pRegPath = szCurrentVersion;

          if (pRegPath)
          {
             if (RegCreateKeyExA(hRootKey,pRegPath,0,NULL,0,KEY_ALL_ACCESS,NULL,&hKey,&dwDisp)) return FALSE;

             if (RegQueryValueExA(hKey,szValueName,NULL,&dwType,(LPBYTE)szPath,&dwPathLen))
                NeedToCreate = TRUE;
          }
          else
             hKey = 0;

          if (!pRegPath || NeedToCreate)
          {
             if (!BuildAndSetFolderPath(hKey, dwFlags, folder, szPath))
             {
                ERR("Failed to build folder path for CSIDL %ld\n", folder);
                RegCloseKey(hKey);
                return FALSE;
             }
          }

          if (pRegPath)
             RegCloseKey(hKey);
        }

	/* expand paths like %USERPROFILE% */
	if (dwType == REG_EXPAND_SZ)
	{
	  ExpandEnvironmentStringsA(szPath, szDefaultPath, MAX_PATH);
	  strcpy(szPath, szDefaultPath);
	}

	/* if we don't care about existing directories we are ready */
	if(csidl & CSIDL_FLAG_DONT_VERIFY) return TRUE;

	if (PathFileExistsA(szPath)) return TRUE;

	/* To minimize the number of empty directories we have to
	   manually have created in our packages.
	   handled here instead of above so that it still works if the 
	   registry entries exist but the folder got removed */
	if (dwFlags & (CSIDL_MYFLAG_USERPREFS | CSIDL_MYFLAG_ALLPREFS))
	   bCreate = TRUE;

	/* not existing but we are not allowed to create it */
	if (!bCreate) return FALSE;

	/* create directory/directories */
	strcpy(szBuildPath, szPath);
	p = strchr(szBuildPath, '\\');
	while (p)
	{
	    *p = 0;
	    if (!PathFileExistsA(szBuildPath))
	    {
		if (!CreateDirectoryA(szBuildPath,NULL))
		{
		    ERR("Failed to create directory '%s'.\n", szPath);
		    return FALSE;
		}
	    }
	    *p = '\\';
	    p = strchr(p+1, '\\');
	}
	/* last component must be created too. */
	if (!PathFileExistsA(szBuildPath))
	{
	    if (!CreateDirectoryA(szBuildPath,NULL))
	    {
		ERR("Failed to create directory '%s'.\n", szPath);
		return FALSE;
	    }
	}

	MESSAGE("Created not existing system directory '%s'\n", szPath);
	return TRUE;
}

/*************************************************************************
 * SHGetSpecialFolderPathW
 */
BOOL WINAPI SHGetSpecialFolderPathW (
	HWND hwndOwner,
	LPWSTR szPath,
	DWORD csidl,
	BOOL bCreate)
{
	char szTemp[MAX_PATH];
	BOOL Ret;

	if ((Ret = SHGetSpecialFolderPathA(hwndOwner, szTemp, csidl, bCreate)))
	{
            if (!MultiByteToWideChar( CP_ACP, 0, szTemp, -1, szPath, MAX_PATH ))
                szPath[MAX_PATH-1] = 0;
        }

	TRACE("0x%04x,%p,csidl=%lu,0x%04x\n", hwndOwner,szPath,csidl,bCreate);

	return Ret;
}

/*************************************************************************
 * SHGetSpecialFolderPath (SHELL32.175)
 */
BOOL WINAPI SHGetSpecialFolderPathAW (
	HWND hwndOwner,
	LPVOID szPath,
	DWORD csidl,
	BOOL bCreate)

{
	if (SHELL_OsIsUnicode())
	  return SHGetSpecialFolderPathW (hwndOwner, szPath, csidl, bCreate);
	return SHGetSpecialFolderPathA (hwndOwner, szPath, csidl, bCreate);
}

/*************************************************************************
 * SHGetFolderPathA			[SHELL32.@]
 */
HRESULT WINAPI SHGetFolderPathA(
	HWND hwndOwner,
	int nFolder,
	HANDLE hToken,	/* [in] FIXME: get paths for specific user */
	DWORD dwFlags,	/* [in] FIXME: SHGFP_TYPE_CURRENT|SHGFP_TYPE_DEFAULT */
	LPSTR pszPath)
{
	return (SHGetSpecialFolderPathA(
		hwndOwner,
		pszPath,
		CSIDL_FOLDER_MASK & nFolder,
		CSIDL_FLAG_CREATE & nFolder )) ? S_OK : E_FAIL;
}

/*************************************************************************
 * SHGetFolderPathW			[SHELL32.@]
 */
HRESULT WINAPI SHGetFolderPathW(
	HWND hwndOwner,
	int nFolder,
	HANDLE hToken,
	DWORD dwFlags,
	LPWSTR pszPath)
{
	return (SHGetSpecialFolderPathW(
		hwndOwner,
		pszPath,
		CSIDL_FOLDER_MASK & nFolder,
		CSIDL_FLAG_CREATE & nFolder )) ? S_OK : E_FAIL;
}
