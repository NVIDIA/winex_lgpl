/*
 * 				Shell basics
 *
 *  1998 Marcus Meissner
 *  1998 Juergen Schmied (jsch)  *  <juergen.schmied@metronet.de>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "winbase.h"
#include "windef.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"
#include "dlgs.h"
#include "shellapi.h"
#include "shlobj.h"
#include "shlguid.h"
#include "shlwapi.h"

#include "undocshell.h"
#include "wine/winuser16.h"
#include "authors.h"
#include "heap.h"
#include "wine/file.h"
#include "pidl.h"
#include "shell32_main.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

extern void create_notify_cs(void);
extern void create_iconcache_cs(void);
extern void destroy_notify_cs(void);
extern void destroy_iconcache_cs(void);

#define MORE_DEBUG 1
/*************************************************************************
 * CommandLineToArgvW			[SHELL32.@]
 *
 * We must interpret the quotes in the command line to rebuild the argv
 * array correctly:
 * - arguments are separated by spaces or tabs
 * - quotes serve as optional argument delimiters
 *   '"a b"'   -> 'a b'
 * - escaped quotes must be converted back to '"'
 *   '\"'      -> '"'
 * - an odd number of '\'s followed by '"' correspond to half that number
 *   of '\' followed by a '"' (extension of the above)
 *   '\\\"'    -> '\"'
 *   '\\\\\"'  -> '\\"'
 * - an even number of '\'s followed by a '"' correspond to half that number
 *   of '\', plus a regular quote serving as an argument delimiter (which
 *   means it does not appear in the result)
 *   'a\\"b c"'   -> 'a\b c'
 *   'a\\\\"b c"' -> 'a\\b c'
 * - '\' that are not followed by a '"' are copied literally
 *   'a\b'     -> 'a\b'
 *   'a\\b'    -> 'a\\b'
 *
 * Note:
 * '\t' == 0x0009
 * ' '  == 0x0020
 * '"'  == 0x0022
 * '\\' == 0x005c
 */
#define CLTAW_STRCAT(x, y)  x##y
#define CLTAW_PASTE(x, y)   CLTAW_STRCAT(x, y)
#define CLTAW_ALLOCSPACE    Global
//#define CLTAW_ALLOCSPACE    Local
#define CLTAW_ALLOC         CLTAW_PASTE(CLTAW_ALLOCSPACE, Alloc)
#define CLTAW_REALLOC       CLTAW_PASTE(CLTAW_ALLOCSPACE, ReAlloc)
#define CLTAW_LOCK          CLTAW_PASTE(CLTAW_ALLOCSPACE, Lock)
#define CLTAW_FREE          CLTAW_PASTE(CLTAW_ALLOCSPACE, Free)


LPWSTR* WINAPI CommandLineToArgvW(LPCWSTR lpCmdline, int* numargs)
{
    DWORD argc;
    HGLOBAL hargv;
    LPWSTR  *argv;
    LPCWSTR cs;
    LPWSTR arg,s,d;
    LPWSTR cmdline;
    int in_quotes,bcount;

    if (*lpCmdline==0) {
        /* Return the path to the executable */
        DWORD size;
        DWORD result;
        DWORD maxChars;


        size = 32;
        /* NOTE: native LocalReAlloc() does not accept NULL as the handle to realloc.
                 This leads to an infinite loop on windows.
                 Also, under windows, the GMEM_MOVEABLE flag seems to be required for
                 the GlobalAlloc() function to succeed in this case.  Passing 0 for
                 the <flags> parameter results in NULL being returned as the handle. 
        */
        hargv = CLTAW_ALLOC(0, size + (2 * sizeof(WCHAR)));

        if (hargv == 0)
            return NULL;

        argv = CLTAW_LOCK(hargv);

        if (argv == NULL)
            return NULL;


        maxChars = (size - sizeof(LPWSTR)) / sizeof(WCHAR);

        /* try to grab the module filename into the new buffer.  If it's too small, it'll return
           <maxChars>, or 0 on error. */
        FIXME("our implementation of GetModuleFileName() is incorrect - it does not have a return value to indicate a too-small buffer.  Using a hack here to get around it... {size = %ld, maxChars = %ld, hargv = 0x%x, argv = %p}\n", size, maxChars, hargv, argv);
        result = GetModuleFileNameW((HMODULE)0, (LPWSTR)(argv + 1), maxChars);


        /* resize the buffer and try again */
        while (result == 0 || result == maxChars){
            size *= 2;
            hargv = CLTAW_REALLOC(hargv, size + (2 * sizeof(WCHAR)), 0);
            argv =  CLTAW_LOCK(hargv);
            maxChars = (size - sizeof(LPWSTR)) / sizeof(WCHAR);
            FIXME("our implementation of GetModuleFileName() is incorrect - it does not have a return value to indicate a too-small buffer.  Using a hack here to get around it... {size = %ld, maxChars = %ld, hargv = 0x%x, argv = %p}\n", size, maxChars, hargv, argv);

            if (argv == NULL)
                return NULL;

            result = GetModuleFileNameW((HMODULE)0, (LPWSTR)(argv + 1), maxChars);
        }

        /* an error occurred while retrieving the module name (ie: could not retrieve name or bad module handle)
               => fail */
        if (result == 0)
            return NULL;

        /* NOTE: native does NOT null terminate this string array itself, but it does null terminate 
                 the memory after the final string entry (ie: two null wide characters after the null
                 terminator of the last string).  It also returns 1 as the argument count in this case.
                 The first string begins immediately after argv[0]. */
        argv[0] = (LPWSTR)(argv + 1);
        argv[0][result + 1] = 0;
        argv[0][result + 2] = 0;

        if (numargs)
            *numargs = 1;

        return argv;
    }

    /* to get a writeable copy */
    argc=0;
    bcount=0;
    in_quotes=0;
    cs=lpCmdline;
    while (1) {
        if (*cs==0 || ((*cs==0x0009 || *cs==0x0020) && !in_quotes)) {
            /* space */
            argc++;
            /* skip the remaining spaces */
            while (*cs==0x0009 || *cs==0x0020) {
                cs++;
            }
            if (*cs==0)
                break;
            bcount=0;
            continue;
        } else if (*cs==0x005c) {
            /* '\', count them */
            bcount++;
        } else if ((*cs==0x0022) && ((bcount & 1)==0)) {
            /* unescaped '"' */
            in_quotes=!in_quotes;
            bcount=0;
        } else {
            /* a regular character */
            bcount=0;
        }
        cs++;
    }


    /* Allocate in a single lump, the string array, and the strings that go with it.
     * This way the caller can make a single Global/LocalFree call to free both, as per MSDN.
     * the extra '2' characters are for the two null terminators at the end of the string list.
     * NOTE: some time between VC7 and VC8 (possibly with winXP), this function changed from 
     *       using GlobalAlloc() to using LocalAlloc().  They aren't *supposed* to be compatible
     *       with each other, but they don't seem to complain and both function properly
     *       and interchangeably under windows.
     */
    hargv = CLTAW_ALLOC(0, argc * sizeof(LPWSTR) + (strlenW(lpCmdline) + 1 + 2) * sizeof(WCHAR));
    argv =  CLTAW_LOCK(hargv);

    if (argv == NULL)
        return NULL;


    cmdline = (LPWSTR)(argv + argc);
    strcpyW(cmdline, lpCmdline);

    argc=0;
    bcount=0;
    in_quotes=0;
    arg=d=s=cmdline;
    while (*s) {
        if ((*s==0x0009 || *s==0x0020) && !in_quotes) {
            /* Close the argument and copy it */
            *d=0;
            argv[argc++]=arg;

            /* skip the remaining spaces */
            do {
                s++;
            } while (*s==0x0009 || *s==0x0020);

            /* Start with a new argument */
            arg=d=s;
            bcount=0;
        } else if (*s==0x005c) {
            /* '\\' */
            *d++=*s++;
            bcount++;
        } else if (*s==0x0022) {
            /* '"' */
            if ((bcount & 1)==0) {
                /* Preceeded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase.
                 */
                d-=bcount/2;
                in_quotes=!in_quotes;
                s++;
            } else {
                /* Preceeded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d=d-bcount/2-1;
                *d++='"';
                s++;
            }
            bcount=0;
        } else {
            /* a regular character */
            *d++=*s++;
            bcount=0;
        }
    }

    /* double terminate the final string */
    if (*arg) {
        d[0] = '\0';
        d[1] = '\0';
        d[2] = '\0';
        argv[argc++] = arg;
    }
    if (numargs)
        *numargs = argc;

    return argv;
}

#undef CLTAW_STRCAT
#undef CLTAW_PASTE
#undef CLTAW_ALLOCSPACE
#undef CLTAW_ALLOC
#undef CLTAW_REALLOC
#undef CLTAW_LOCK
#undef CLTAW_FREE


/*************************************************************************
 * SHGetFileInfoA			[SHELL32.@]
 *
 */

DWORD WINAPI SHGetFileInfoA(LPCSTR path,DWORD dwFileAttributes,
                              SHFILEINFOA *psfi, UINT sizeofpsfi,
                              UINT flags )
{
	char szLoaction[MAX_PATH];
	int iIndex;
	DWORD ret = TRUE, dwAttributes = 0;
	IShellFolder * psfParent = NULL;
	IExtractIconA * pei = NULL;
	LPITEMIDLIST	pidlLast = NULL, pidl = NULL;
	HRESULT hr = S_OK;
    BOOL IconNotYetLoaded=TRUE;

	TRACE("(%s fattr=0x%lx sfi=%p(attr=0x%08lx) size=0x%x flags=0x%x)\n",
	  (flags & SHGFI_PIDL)? "pidl" : path, dwFileAttributes, psfi, psfi->dwAttributes, sizeofpsfi, flags);

	if ((flags & SHGFI_USEFILEATTRIBUTES) && (flags & (SHGFI_ATTRIBUTES|SHGFI_EXETYPE|SHGFI_PIDL)))
	  return FALSE;

	/* windows initializes this values regardless of the flags */
	psfi->szDisplayName[0] = '\0';
	psfi->szTypeName[0] = '\0';
	psfi->iIcon = 0;

	if (flags & SHGFI_EXETYPE) {
	  BOOL status = FALSE;
	  HANDLE hfile;
	  DWORD BinaryType;
	  IMAGE_DOS_HEADER mz_header;
	  IMAGE_NT_HEADERS nt;
	  DWORD len;
	  char magic[4];

	  if (flags != SHGFI_EXETYPE) return 0;

	  status = GetBinaryTypeA (path, &BinaryType);
	  if (!status) return 0;
	  if ((BinaryType == SCS_DOS_BINARY)
		|| (BinaryType == SCS_PIF_BINARY)) return 0x4d5a;

	  hfile = CreateFileA( path, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, 0 );
	  if ( hfile == INVALID_HANDLE_VALUE ) return 0;

	/* The next section is adapted from MODULE_GetBinaryType, as we need
	 * to examine the image header to get OS and version information. We
	 * know from calling GetBinaryTypeA that the image is valid and either
	 * an NE or PE, so much error handling can be omitted.
	 * Seek to the start of the file and read the header information.
	 */

	  SetFilePointer( hfile, 0, NULL, SEEK_SET );
	  ReadFile( hfile, &mz_header, sizeof(mz_header), &len, NULL );

         SetFilePointer( hfile, mz_header.e_lfanew, NULL, SEEK_SET );
         ReadFile( hfile, magic, sizeof(magic), &len, NULL );
         if ( *(DWORD*)magic      == IMAGE_NT_SIGNATURE )
         {
             SetFilePointer( hfile, mz_header.e_lfanew, NULL, SEEK_SET );
             ReadFile( hfile, &nt, sizeof(nt), &len, NULL );
	      CloseHandle( hfile );
	      if (nt.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI) {
                 return IMAGE_NT_SIGNATURE
			| (nt.OptionalHeader.MajorSubsystemVersion << 24)
			| (nt.OptionalHeader.MinorSubsystemVersion << 16);
	      }
	      return IMAGE_NT_SIGNATURE;
	  }
         else if ( *(WORD*)magic == IMAGE_OS2_SIGNATURE )
         {
             IMAGE_OS2_HEADER ne;
             SetFilePointer( hfile, mz_header.e_lfanew, NULL, SEEK_SET );
             ReadFile( hfile, &ne, sizeof(ne), &len, NULL );
	      CloseHandle( hfile );
             if (ne.ne_exetyp == 2) return IMAGE_OS2_SIGNATURE
			| (ne.ne_expver << 16);
	      return 0;
	  }
	  CloseHandle( hfile );
	  return 0;
      }


	/* translate the path into a pidl only when SHGFI_USEFILEATTRIBUTES in not specified
	   the pidl functions fail on not existing file names */
	if (flags & SHGFI_PIDL)
	{
	  pidl = (LPCITEMIDLIST) path;
	  if (!pidl )
	  {
	    ERR("pidl is null!\n");
	    return FALSE;
	  }
	}
	else if (!(flags & SHGFI_USEFILEATTRIBUTES))
	{
	  hr = SHILCreateFromPathA ( path, &pidl, &dwAttributes);
	  /* note: the attributes in ISF::ParseDisplayName are not implemented */
	}

	/* get the parent shellfolder */
	if (pidl)
	{
	  hr = SHBindToParent( pidl, &IID_IShellFolder, (LPVOID*)&psfParent, &pidlLast);
	}

	/* get the attributes of the child */
	if (SUCCEEDED(hr) && (flags & SHGFI_ATTRIBUTES))
	{
	  if (!(flags & SHGFI_ATTR_SPECIFIED))
	  {
	    psfi->dwAttributes = 0xffffffff;
	  }
	  IShellFolder_GetAttributesOf(psfParent, 1 , &pidlLast, &(psfi->dwAttributes));
	}

	/* get the displayname */
	if (SUCCEEDED(hr) && (flags & SHGFI_DISPLAYNAME))
	{
	  if (flags & SHGFI_USEFILEATTRIBUTES)
	  {
	    strcpy (psfi->szDisplayName, PathFindFileNameA(path));
	  }
	  else
	  {
	    STRRET str;
	    hr = IShellFolder_GetDisplayNameOf(psfParent, pidlLast, SHGDN_INFOLDER, &str);
	    StrRetToStrNA (psfi->szDisplayName, MAX_PATH, &str, pidlLast);
	  }
	}

	/* get the type name */
	if (SUCCEEDED(hr) && (flags & SHGFI_TYPENAME))
        {
            if (!(flags & SHGFI_USEFILEATTRIBUTES))
                _ILGetFileType(pidlLast, psfi->szTypeName, 80);
            else
            {
                char sTemp[64];
                strcpy(sTemp,PathFindExtensionA(path));
                if (!( HCR_MapTypeToValue(sTemp, sTemp, 64, TRUE)
                       && HCR_MapTypeToValue(sTemp, psfi->szTypeName, 80, FALSE )))
                {
                    lstrcpynA (psfi->szTypeName, sTemp, 80 - 6);
                    strcat (psfi->szTypeName, "-file");
                }
            }
        }

	/* ### icons ###*/
	if (flags & SHGFI_LINKOVERLAY)
	  FIXME("set icon to link, stub\n");

	if (flags & SHGFI_SELECTED)
	  FIXME("set icon to selected, stub\n");

	if (flags & SHGFI_SHELLICONSIZE)
	  FIXME("set icon to shell size, stub\n");

	/* get the iconlocation */
	if (SUCCEEDED(hr) && (flags & SHGFI_ICONLOCATION ))
	{
	  UINT uDummy,uFlags;
	  hr = IShellFolder_GetUIObjectOf(psfParent, 0, 1, &pidlLast, &IID_IExtractIconA, &uDummy, (LPVOID*)&pei);

	  if (SUCCEEDED(hr))
	  {
	    hr = IExtractIconA_GetIconLocation(pei, (flags & SHGFI_OPENICON)? GIL_OPENICON : 0,szLoaction, MAX_PATH, &iIndex, &uFlags);
	    /* FIXME what to do with the index? */

	    if(uFlags != GIL_NOTFILENAME)
	      strcpy (psfi->szDisplayName, szLoaction);
	    else
	      ret = FALSE;

	    IExtractIconA_Release(pei);
	  }
	}

	/* get icon index (or load icon)*/
	if (SUCCEEDED(hr) && (flags & (SHGFI_ICON | SHGFI_SYSICONINDEX)))
	{

	  if (flags & SHGFI_USEFILEATTRIBUTES)
	  {
	    char sTemp [MAX_PATH];
	    char * szExt;
	    DWORD dwNr=0;

	    lstrcpynA(sTemp, path, MAX_PATH);
	    szExt = (LPSTR) PathFindExtensionA(sTemp);
	    if( szExt && HCR_MapTypeToValue(szExt, sTemp, MAX_PATH, TRUE)
              && HCR_GetDefaultIcon(sTemp, sTemp, MAX_PATH, &dwNr))
            {
              if (!strcmp("%1",sTemp))            /* icon is in the file */
              {
                strcpy(sTemp, path);
              }
              IconNotYetLoaded=FALSE;
              psfi->iIcon = 0;
              if (SHGFI_LARGEICON)
                PrivateExtractIconsA(sTemp,dwNr,GetSystemMetrics(SM_CXICON),
                                     GetSystemMetrics(SM_CYICON),
                                     &psfi->hIcon,0,1,0);
              else
                PrivateExtractIconsA(sTemp,dwNr,GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON),
                                     &psfi->hIcon,0,1,0);
            }
            else                                  /* default icon */
            {
              psfi->iIcon = 0;
            }
	  }
	  else
	  {
	    if (!(PidlToSicIndex(psfParent, pidlLast, (flags & SHGFI_LARGEICON),
	      (flags & SHGFI_OPENICON)? GIL_OPENICON : 0, &(psfi->iIcon))))
	    {
	      ret = FALSE;
	    }
	  }
	  if (ret)
	  {
	    ret = (DWORD) ((flags & SHGFI_LARGEICON) ? ShellBigIconList : ShellSmallIconList);
	  }
	}

	/* icon handle */
	if (SUCCEEDED(hr) && (flags & SHGFI_ICON) && IconNotYetLoaded)
	  psfi->hIcon = ImageList_GetIcon((flags & SHGFI_LARGEICON) ? ShellBigIconList:ShellSmallIconList, psfi->iIcon, ILD_NORMAL);

	if (flags & (SHGFI_UNKNOWN1 | SHGFI_UNKNOWN2 | SHGFI_UNKNOWN3))
	  FIXME("unknown attribute!\n");

	if (psfParent)
	  IShellFolder_Release(psfParent);

	if (hr != S_OK)
	  ret = FALSE;

	if(pidlLast) SHFree(pidlLast);
#ifdef MORE_DEBUG
	TRACE ("icon=0x%08x index=0x%08x attr=0x%08lx name=%s type=%s ret=0x%08lx\n",
		psfi->hIcon, psfi->iIcon, psfi->dwAttributes, psfi->szDisplayName, psfi->szTypeName, ret);
#endif
	return ret;
}

/*************************************************************************
 * SHGetFileInfoW			[SHELL32.@]
 */

DWORD WINAPI SHGetFileInfoW(LPCWSTR path,DWORD dwFileAttributes,
                              SHFILEINFOW *psfi, UINT sizeofpsfi,
                              UINT flags )
{
	INT len;
	LPSTR temppath;
	DWORD ret;
	SHFILEINFOA temppsfi;

	len = WideCharToMultiByte(CP_ACP, 0, path, -1, NULL, 0, NULL, NULL);
	temppath = HeapAlloc(GetProcessHeap(), 0, len);
	WideCharToMultiByte(CP_ACP, 0, path, -1, temppath, len, NULL, NULL);

        WideCharToMultiByte(CP_ACP, 0, psfi->szDisplayName, -1, temppsfi.szDisplayName,
                            sizeof(temppsfi.szDisplayName), NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, psfi->szTypeName, -1, temppsfi.szTypeName,
                            sizeof(temppsfi.szTypeName), NULL, NULL);

	ret = SHGetFileInfoA(temppath, dwFileAttributes, &temppsfi, sizeof(temppsfi), flags);

	HeapFree(GetProcessHeap(), 0, temppath);

	return ret;
}

/*************************************************************************
 * SHGetFileInfo			[SHELL32.@]
 */
DWORD WINAPI SHGetFileInfoAW(
	LPCVOID path,
	DWORD dwFileAttributes,
	LPVOID psfi,
	UINT sizeofpsfi,
	UINT flags)
{
	if(SHELL_OsIsUnicode())
	  return SHGetFileInfoW(path, dwFileAttributes, psfi, sizeofpsfi, flags );
	return SHGetFileInfoA(path, dwFileAttributes, psfi, sizeofpsfi, flags );
}

/*************************************************************************
 * DuplicateIcon			[SHELL32.@]
 */
HICON WINAPI DuplicateIcon( HINSTANCE hInstance, HICON hIcon)
{
    ICONINFO IconInfo;
    HICON hDupIcon = 0;

    TRACE("(%04x, %04x)\n", hInstance, hIcon);

    if(GetIconInfo(hIcon, &IconInfo))
    {
        hDupIcon = CreateIconIndirect(&IconInfo);

        /* clean up hbmMask and hbmColor */
        DeleteObject(IconInfo.hbmMask);
        DeleteObject(IconInfo.hbmColor);
    }

    return hDupIcon;
}


/*************************************************************************
 * ExtractIconA				[SHELL32.@]
 *
 * FIXME
 *  if the filename is not a file return 1
 */
HICON WINAPI ExtractIconA( HINSTANCE hInstance, LPCSTR lpszExeFileName,
	UINT nIconIndex )
{   HGLOBAL16 handle = InternalExtractIcon16(hInstance,lpszExeFileName,nIconIndex, 1);
    TRACE("\n");
    if( handle )
    {
	HICON16* ptr = (HICON16*)GlobalLock16(handle);
	HICON16  hIcon = *ptr;

	GlobalFree16(handle);
	return hIcon;
    }
    return 0;
}

/*************************************************************************
 * ExtractIconW				[SHELL32.@]
 *
 * FIXME: if the filename is not a file return 1
 */
HICON WINAPI ExtractIconW( HINSTANCE hInstance, LPCWSTR lpszExeFileName,
	UINT nIconIndex )
{ LPSTR  exefn;
  HICON  ret;
  TRACE("\n");

  exefn = FILE_strdupWtoA(GetProcessHeap(),0,lpszExeFileName);
  ret = ExtractIconA(hInstance,exefn,nIconIndex);

	HeapFree(GetProcessHeap(),0,exefn);
	return ret;
}

typedef struct
{ LPCSTR  szApp;
    LPCSTR  szOtherStuff;
    HICON hIcon;
} ABOUT_INFO;

#define		IDC_STATIC_TEXT		100
#define		IDC_LISTBOX		99
#define		IDC_WINE_TEXT		98

#define		DROP_FIELD_TOP		(-15)
#define		DROP_FIELD_HEIGHT	15

static HICON hIconTitleFont;

static BOOL __get_dropline( HWND hWnd, LPRECT lprect )
{ HWND hWndCtl = GetDlgItem(hWnd, IDC_WINE_TEXT);
    if( hWndCtl )
  { GetWindowRect( hWndCtl, lprect );
	MapWindowPoints( 0, hWnd, (LPPOINT)lprect, 2 );
	lprect->bottom = (lprect->top += DROP_FIELD_TOP);
	return TRUE;
    }
    return FALSE;
}

/*************************************************************************
 * SHAppBarMessage			[SHELL32.@]
 */
UINT WINAPI SHAppBarMessage(DWORD msg, PAPPBARDATA data)
{
        int width=data->rc.right - data->rc.left;
        int height=data->rc.bottom - data->rc.top;
        RECT rec=data->rc;
        switch (msg)
        { case ABM_GETSTATE:
               return ABS_ALWAYSONTOP | ABS_AUTOHIDE;
          case ABM_GETTASKBARPOS:
               GetWindowRect(data->hWnd, &rec);
               data->rc=rec;
               return TRUE;
          case ABM_ACTIVATE:
               SetActiveWindow(data->hWnd);
               return TRUE;
          case ABM_GETAUTOHIDEBAR:
               data->hWnd=GetActiveWindow();
               return TRUE;
          case ABM_NEW:
               SetWindowPos(data->hWnd,HWND_TOP,rec.left,rec.top,
                                        width,height,SWP_SHOWWINDOW);
               return TRUE;
          case ABM_QUERYPOS:
               GetWindowRect(data->hWnd, &(data->rc));
               return TRUE;
          case ABM_REMOVE:
               FIXME("ABM_REMOVE broken\n");
               /* FIXME: this is wrong; should it be DestroyWindow instead? */
               /*CloseHandle(data->hWnd);*/
               return TRUE;
          case ABM_SETAUTOHIDEBAR:
               SetWindowPos(data->hWnd,HWND_TOP,rec.left+1000,rec.top,
                                       width,height,SWP_SHOWWINDOW);
               return TRUE;
          case ABM_SETPOS:
               data->uEdge=(ABE_RIGHT | ABE_LEFT);
               SetWindowPos(data->hWnd,HWND_TOP,data->rc.left,data->rc.top,
                                  width,height,SWP_SHOWWINDOW);
               return TRUE;
          case ABM_WINDOWPOSCHANGED:
               SetWindowPos(data->hWnd,HWND_TOP,rec.left,rec.top,
                                        width,height,SWP_SHOWWINDOW);
               return TRUE;
          }
      return FALSE;
}

/*************************************************************************
 * SHHelpShortcuts_RunDLL		[SHELL32.@]
 *
 */
DWORD WINAPI SHHelpShortcuts_RunDLL (DWORD dwArg1, DWORD dwArg2, DWORD dwArg3, DWORD dwArg4)
{ FIXME("(%lx, %lx, %lx, %lx) empty stub!\n",
	dwArg1, dwArg2, dwArg3, dwArg4);

  return 0;
}

/*************************************************************************
 * SHLoadInProc				[SHELL32.@]
 * Create an instance of specified object class from within
 * the shell process and release it immediately
 */

DWORD WINAPI SHLoadInProc (REFCLSID rclsid)
{
	IUnknown * pUnk = NULL;
	TRACE("%s\n", debugstr_guid(rclsid));

	CoCreateInstance(rclsid, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown,(LPVOID*)pUnk);
	if(pUnk)
	{
	  IUnknown_Release(pUnk);
          return NOERROR;
	}
	return DISP_E_MEMBERNOTFOUND;
}

/*************************************************************************
 * AboutDlgProc			(internal)
 */
BOOL WINAPI AboutDlgProc( HWND hWnd, UINT msg, WPARAM wParam,
                              LPARAM lParam )
{   HWND hWndCtl;
    char Template[512], AppTitle[512];

    TRACE("\n");

    switch(msg)
    { case WM_INITDIALOG:
      { ABOUT_INFO *info = (ABOUT_INFO *)lParam;
            if (info)
        { const char* const *pstr = SHELL_People;
                SendDlgItemMessageA(hWnd, stc1, STM_SETICON,info->hIcon, 0);
                GetWindowTextA( hWnd, Template, sizeof(Template) );
                sprintf( AppTitle, Template, info->szApp );
                SetWindowTextA( hWnd, AppTitle );
                SetWindowTextA( GetDlgItem(hWnd, IDC_STATIC_TEXT),
                                  info->szOtherStuff );
                hWndCtl = GetDlgItem(hWnd, IDC_LISTBOX);
                SendMessageA( hWndCtl, WM_SETREDRAW, 0, 0 );
                if (!hIconTitleFont)
                {
                    LOGFONTA logFont;
                    SystemParametersInfoA( SPI_GETICONTITLELOGFONT, 0, &logFont, 0 );
                    hIconTitleFont = CreateFontIndirectA( &logFont );
                }
                SendMessageA( hWndCtl, WM_SETFONT, hIconTitleFont, 0 );
                while (*pstr)
          { SendMessageA( hWndCtl, LB_ADDSTRING, (WPARAM)-1, (LPARAM)*pstr );
                    pstr++;
                }
                SendMessageA( hWndCtl, WM_SETREDRAW, 1, 0 );
            }
        }
        return 1;

    case WM_PAINT:
      { RECT rect;
	    PAINTSTRUCT ps;
	    HDC hDC = BeginPaint( hWnd, &ps );

	    if( __get_dropline( hWnd, &rect ) ) {
	        SelectObject( hDC, GetStockObject( BLACK_PEN ) );
	        MoveToEx( hDC, rect.left, rect.top, NULL );
		LineTo( hDC, rect.right, rect.bottom );
	    }
	    EndPaint( hWnd, &ps );
	}
	break;

#if 0  /* FIXME: should use DoDragDrop */
    case WM_LBTRACKPOINT:
	hWndCtl = GetDlgItem(hWnd, IDC_LISTBOX);
	if( (INT16)GetKeyState( VK_CONTROL ) < 0 )
      { if( DragDetect( hWndCtl, *((LPPOINT)&lParam) ) )
        { INT idx = SendMessageA( hWndCtl, LB_GETCURSEL, 0, 0 );
		if( idx != -1 )
          { INT length = SendMessageA( hWndCtl, LB_GETTEXTLEN, (WPARAM)idx, 0 );
		    HGLOBAL16 hMemObj = GlobalAlloc16( GMEM_MOVEABLE, length + 1 );
		    char* pstr = (char*)GlobalLock16( hMemObj );

		    if( pstr )
            { HCURSOR hCursor = LoadCursorA( 0, MAKEINTRESOURCEA(OCR_DRAGOBJECT) );
			SendMessageA( hWndCtl, LB_GETTEXT, (WPARAM)idx, (LPARAM)pstr );
			SendMessageA( hWndCtl, LB_DELETESTRING, (WPARAM)idx, 0 );
			UpdateWindow( hWndCtl );
			if( !DragObject16((HWND16)hWnd, (HWND16)hWnd, DRAGOBJ_DATA, 0, (WORD)hMemObj, hCursor) )
			    SendMessageA( hWndCtl, LB_ADDSTRING, (WPARAM)-1, (LPARAM)pstr );
		    }
            if( hMemObj )
              GlobalFree16( hMemObj );
		}
	    }
	}
	break;
#endif

    case WM_QUERYDROPOBJECT:
	if( wParam == 0 )
      { LPDRAGINFO16 lpDragInfo = MapSL((SEGPTR)lParam);
	    if( lpDragInfo && lpDragInfo->wFlags == DRAGOBJ_DATA )
        { RECT rect;
		if( __get_dropline( hWnd, &rect ) )
          { POINT pt;
	    pt.x=lpDragInfo->pt.x;
	    pt.x=lpDragInfo->pt.y;
		    rect.bottom += DROP_FIELD_HEIGHT;
		    if( PtInRect( &rect, pt ) )
            { SetWindowLongA( hWnd, DWL_MSGRESULT, 1 );
			return TRUE;
		    }
		}
	    }
	}
	break;

    case WM_DROPOBJECT:
	if( wParam == hWnd )
      { LPDRAGINFO16 lpDragInfo = MapSL((SEGPTR)lParam);
	    if( lpDragInfo && lpDragInfo->wFlags == DRAGOBJ_DATA && lpDragInfo->hList )
        { char* pstr = (char*)GlobalLock16( (HGLOBAL16)(lpDragInfo->hList) );
		if( pstr )
          { static char __appendix_str[] = " with";

		    hWndCtl = GetDlgItem( hWnd, IDC_WINE_TEXT );
		    SendMessageA( hWndCtl, WM_GETTEXT, 512, (LPARAM)Template );
		    if( !strncmp( Template, "WINE", 4 ) )
			SetWindowTextA( GetDlgItem(hWnd, IDC_STATIC_TEXT), Template );
		    else
          { char* pch = Template + strlen(Template) - strlen(__appendix_str);
			*pch = '\0';
			SendMessageA( GetDlgItem(hWnd, IDC_LISTBOX), LB_ADDSTRING,
					(WPARAM)-1, (LPARAM)Template );
		    }

		    strcpy( Template, pstr );
		    strcat( Template, __appendix_str );
		    SetWindowTextA( hWndCtl, Template );
		    SetWindowLongA( hWnd, DWL_MSGRESULT, 1 );
		    return TRUE;
		}
	    }
	}
	break;

    case WM_COMMAND:
        if (wParam == IDOK)
    {  EndDialog(hWnd, TRUE);
            return TRUE;
        }
        break;
    case WM_CLOSE:
      EndDialog(hWnd, TRUE);
      break;
    }

    return 0;
}


/*************************************************************************
 * ShellAboutA				[SHELL32.288]
 */
BOOL WINAPI ShellAboutA( HWND hWnd, LPCSTR szApp, LPCSTR szOtherStuff,
                             HICON hIcon )
{   ABOUT_INFO info;
    HRSRC hRes;
    LPVOID template;
    TRACE("\n");

    if(!(hRes = FindResourceA(shell32_hInstance, "SHELL_ABOUT_MSGBOX", RT_DIALOGA)))
        return FALSE;
    if(!(template = (LPVOID)LoadResource(shell32_hInstance, hRes)))
        return FALSE;

    info.szApp        = szApp;
    info.szOtherStuff = szOtherStuff;
    info.hIcon        = hIcon;
    if (!hIcon) info.hIcon = LoadIconA( 0, IDI_WINLOGOA );
    return DialogBoxIndirectParamA( GetWindowLongA( hWnd, GWL_HINSTANCE ),
                                      template, hWnd, AboutDlgProc, (LPARAM)&info );
}


/*************************************************************************
 * ShellAboutW				[SHELL32.289]
 */
BOOL WINAPI ShellAboutW( HWND hWnd, LPCWSTR szApp, LPCWSTR szOtherStuff,
                             HICON hIcon )
{   BOOL ret;
    ABOUT_INFO info;
    HRSRC hRes;
    LPVOID template;

    TRACE("\n");

    if(!(hRes = FindResourceA(shell32_hInstance, "SHELL_ABOUT_MSGBOX", RT_DIALOGA)))
        return FALSE;
    if(!(template = (LPVOID)LoadResource(shell32_hInstance, hRes)))
        return FALSE;

    info.szApp        = HEAP_strdupWtoA( GetProcessHeap(), 0, szApp );
    info.szOtherStuff = HEAP_strdupWtoA( GetProcessHeap(), 0, szOtherStuff );
    info.hIcon        = hIcon;
    if (!hIcon) info.hIcon = LoadIconA( 0, IDI_WINLOGOA );
    ret = DialogBoxIndirectParamA( GetWindowLongA( hWnd, GWL_HINSTANCE ),
                                   template, hWnd, AboutDlgProc, (LPARAM)&info );
    HeapFree( GetProcessHeap(), 0, (LPSTR)info.szApp );
    HeapFree( GetProcessHeap(), 0, (LPSTR)info.szOtherStuff );
    return ret;
}

/*************************************************************************
 * FreeIconList (SHELL32.@)
 */
void WINAPI FreeIconList( DWORD dw )
{ FIXME("(%lx): stub\n",dw);
}

/***********************************************************************
 * DllGetVersion [SHELL32.@]
 *
 * Retrieves version information of the 'SHELL32.DLL'
 *
 * PARAMS
 *     pdvi [O] pointer to version information structure.
 *
 * RETURNS
 *     Success: S_OK
 *     Failure: E_INVALIDARG
 *
 * NOTES
 *     Returns version of a shell32.dll from IE4.01 SP1.
 */

HRESULT WINAPI SHELL32_DllGetVersion (DLLVERSIONINFO *pdvi)
{
	if (pdvi->cbSize != sizeof(DLLVERSIONINFO))
	{
	  WARN("wrong DLLVERSIONINFO size from app\n");
	  return E_INVALIDARG;
	}

	pdvi->dwMajorVersion = 4;
	pdvi->dwMinorVersion = 72;
	pdvi->dwBuildNumber = 3110;
	pdvi->dwPlatformID = 1;

	TRACE("%lu.%lu.%lu.%lu\n",
	   pdvi->dwMajorVersion, pdvi->dwMinorVersion,
	   pdvi->dwBuildNumber, pdvi->dwPlatformID);

	return S_OK;
}
/*************************************************************************
 * global variables of the shell32.dll
 * all are once per process
 *
 */
void	(WINAPI *pDLLInitComctl)(LPVOID);

LPVOID	(WINAPI *pCOMCTL32_Alloc) (INT);
BOOL	(WINAPI *pCOMCTL32_Free) (LPVOID);

HDPA	(WINAPI *pDPA_Create) (INT);
INT	(WINAPI *pDPA_InsertPtr) (const HDPA, INT, LPVOID);
BOOL	(WINAPI *pDPA_Sort) (const HDPA, PFNDPACOMPARE, LPARAM);
LPVOID	(WINAPI *pDPA_GetPtr) (const HDPA, INT);
BOOL	(WINAPI *pDPA_Destroy) (const HDPA);
INT	(WINAPI *pDPA_Search) (const HDPA, LPVOID, INT, PFNDPACOMPARE, LPARAM, UINT);
LPVOID	(WINAPI *pDPA_DeletePtr) (const HDPA hdpa, INT i);
HANDLE  (WINAPI *pCreateMRUListA) (LPVOID lpcml);
DWORD   (WINAPI *pFreeMRUListA) (HANDLE hMRUList);
INT     (WINAPI *pAddMRUData) (HANDLE hList, LPCVOID lpData, DWORD cbData);
INT     (WINAPI *pFindMRUData) (HANDLE hList, LPCVOID lpData, DWORD cbData, LPINT lpRegNum);
INT     (WINAPI *pEnumMRUListA) (HANDLE hList, INT nItemPos, LPVOID lpBuffer, DWORD nBufferSize);

static HINSTANCE	hComctl32;

LONG		shell32_ObjCount = 0;
HINSTANCE	shell32_hInstance = 0;
HIMAGELIST	ShellSmallIconList = 0;
HIMAGELIST	ShellBigIconList = 0;


/*************************************************************************
 * SHELL32 LibMain
 *
 * NOTES
 *  calling oleinitialize here breaks sone apps.
 */

BOOL WINAPI Shell32LibMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID fImpLoad)
{
	TRACE("0x%x 0x%lx %p\n", hinstDLL, fdwReason, fImpLoad);

	switch (fdwReason)
	{
	  case DLL_PROCESS_ATTACH:
	    create_notify_cs();
	    create_iconcache_cs();

	    shell32_hInstance = hinstDLL;
	    hComctl32 = GetModuleHandleA("COMCTL32.DLL");
	    DisableThreadLibraryCalls(shell32_hInstance);

	    if (!hComctl32)
	    {
	      ERR("P A N I C SHELL32 loading failed\n");
	      return FALSE;
	    }

	    /* comctl32 */
	    pDLLInitComctl=(void*)GetProcAddress(hComctl32,"InitCommonControlsEx");
	    pCOMCTL32_Alloc=(void*)GetProcAddress(hComctl32, (LPCSTR)71L);
	    pCOMCTL32_Free=(void*)GetProcAddress(hComctl32, (LPCSTR)73L);
	    pDPA_Create=(void*)GetProcAddress(hComctl32, (LPCSTR)328L);
	    pDPA_Destroy=(void*)GetProcAddress(hComctl32, (LPCSTR)329L);
	    pDPA_GetPtr=(void*)GetProcAddress(hComctl32, (LPCSTR)332L);
	    pDPA_InsertPtr=(void*)GetProcAddress(hComctl32, (LPCSTR)334L);
	    pDPA_DeletePtr=(void*)GetProcAddress(hComctl32, (LPCSTR)336L);
	    pDPA_Sort=(void*)GetProcAddress(hComctl32, (LPCSTR)338L);
	    pDPA_Search=(void*)GetProcAddress(hComctl32, (LPCSTR)339L);
	    pCreateMRUListA=(void*)GetProcAddress(hComctl32, (LPCSTR)151L /*"CreateMRUListA"*/);
	    pFreeMRUListA=(void*)GetProcAddress(hComctl32, (LPCSTR)152L /*"FreeMRUList"*/);
	    pAddMRUData=(void*)GetProcAddress(hComctl32, (LPCSTR)167L /*"AddMRUData"*/);
	    pFindMRUData=(void*)GetProcAddress(hComctl32, (LPCSTR)169L /*"FindMRUData"*/);
	    pEnumMRUListA=(void*)GetProcAddress(hComctl32, (LPCSTR)154L /*"EnumMRUListA"*/);

	    /* initialize the common controls */
	    if (pDLLInitComctl)
	    {
	      pDLLInitComctl(NULL);
	    }

	    SIC_Initialize();
	    SYSTRAY_Init();
	    InitChangeNotifications();
	    SHInitRestricted(NULL, NULL);
	    Shellpath_Init();
	    break;

	  case DLL_THREAD_ATTACH:
	    break;

	  case DLL_THREAD_DETACH:
	    break;

	  case DLL_PROCESS_DETACH:
	      shell32_hInstance = 0;

	      if (pdesktopfolder)
	      {
	        IShellFolder_Release(pdesktopfolder);
	        pdesktopfolder = NULL;
	      }

	      SIC_Destroy();
	      FreeChangeNotifications();

	      /* this one is here to check if AddRef/Release is balanced */
	      if (shell32_ObjCount)
	      {
	        WARN("leaving with %lu objects left (memory leak)\n", shell32_ObjCount);
	      }
	      
	      destroy_notify_cs();
	      destroy_iconcache_cs();

	      break;
	}
	return TRUE;
}

/*************************************************************************
 * DllInstall         [SHELL32.@]
 *
 * PARAMETERS
 *
 *    BOOL bInstall - TRUE for install, FALSE for uninstall
 *    LPCWSTR pszCmdLine - command line (unused by shell32?)
 */

HRESULT WINAPI SHELL32_DllInstall(BOOL bInstall, LPCWSTR cmdline)
{
   FIXME("(%s, %s): stub!\n", bInstall ? "TRUE":"FALSE", debugstr_w(cmdline));

   return S_OK;		/* indicate success */
}

/***********************************************************************
 *              DllCanUnloadNow (SHELL32.@)
 */
HRESULT WINAPI SHELL32_DllCanUnloadNow(void)
{
    FIXME("(void): stub\n");

    return S_FALSE;
}
