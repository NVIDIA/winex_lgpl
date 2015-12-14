/*
 * 				Shell Library Functions
 *
 *  1998 Marcus Meissner
 *  2002 Eric Pouech
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "winbase.h"
#include "windef.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"
#include "dlgs.h"
#include "shellapi.h"
#include "shlobj.h"
#include "shlwapi.h"
#include "ddeml.h"

#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "shell32_main.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);
WINE_DECLARE_DEBUG_CHANNEL(exec);


typedef struct {     /* structure for dropped files */
 WORD     wSize;
 POINT16  ptMousePos;
 BOOL16   fInNonClientArea;
 /* memory block with filenames follows */
} DROPFILESTRUCT16, *LPDROPFILESTRUCT16;

static const char*	lpstrMsgWndCreated = "OTHERWINDOWCREATED";
static const char*	lpstrMsgWndDestroyed = "OTHERWINDOWDESTROYED";
static const char*	lpstrMsgShellActivate = "ACTIVATESHELLWINDOW";

static HWND16	SHELL_hWnd = 0;
static HHOOK	SHELL_hHook = 0;
static UINT16	uMsgWndCreated = 0;
static UINT16	uMsgWndDestroyed = 0;
static UINT16	uMsgShellActivate = 0;
HINSTANCE16	SHELL_hInstance = 0;
HINSTANCE SHELL_hInstance32;
static int SHELL_Attach = 0;

/***********************************************************************
 * DllEntryPoint [SHELL.101]
 *
 * Initialization code for shell.dll. Automatically loads the
 * 32-bit shell32.dll to allow thunking up to 32-bit code.
 *
 * RETURNS:
 */
BOOL WINAPI SHELL_DllEntryPoint(DWORD Reason, HINSTANCE16 hInst,
				WORD ds, WORD HeapSize, DWORD res1, WORD res2)
{
    TRACE("(%08lx, %04x, %04x, %04x, %08lx, %04x)\n",
          Reason, hInst, ds, HeapSize, res1, res2);

    switch(Reason)
    {
    case DLL_PROCESS_ATTACH:
	if (SHELL_Attach++) break;
	SHELL_hInstance = hInst;
	if(!SHELL_hInstance32)
	{
	    if(!(SHELL_hInstance32 = LoadLibraryA("shell32.dll")))
	    {
		ERR("Could not load sibling shell32.dll\n");
		return FALSE;
	    }
	}
	break;

    case DLL_PROCESS_DETACH:
	if(!--SHELL_Attach)
	{
	    SHELL_hInstance = 0;
	    if(SHELL_hInstance32)
		FreeLibrary(SHELL_hInstance32);
	}
	break;
    }
    return TRUE;
}

/*************************************************************************
 *				DragAcceptFiles		[SHELL.9]
 */
void WINAPI DragAcceptFiles16(HWND16 hWnd, BOOL16 b)
{
  DragAcceptFiles(hWnd, b);
}

/*************************************************************************
 *				DragQueryFile		[SHELL.11]
 */
UINT16 WINAPI DragQueryFile16(
	HDROP16 hDrop,
	WORD wFile,
	LPSTR lpszFile,
	WORD wLength)
{
 	LPSTR lpDrop;
	UINT i = 0;
	UINT iFile;
	LPDROPFILESTRUCT16 lpDropFileStruct = (LPDROPFILESTRUCT16) GlobalLock16(hDrop);

	TRACE("(%04x, %x, %p, %u)\n", hDrop,wFile,lpszFile,wLength);

	if(!lpDropFileStruct) goto end;

	lpDrop = (LPSTR) lpDropFileStruct + lpDropFileStruct->wSize;
	iFile = (wFile==0xffff) ? 0xffffffff : wFile;

	while (i++ < iFile)
	{
	  while (*lpDrop++); /* skip filename */
	  if (!*lpDrop)
	  {
	    i = (iFile == 0xFFFFFFFF) ? i : 0;
	    goto end;
	  }
	}

	i = strlen(lpDrop);
	i++;
	if (!lpszFile ) goto end;   /* needed buffer size */
	i = (wLength > i) ? i : wLength;
	lstrcpynA (lpszFile,  lpDrop,  i);
end:
	GlobalUnlock16(hDrop);
	return i;
}

/*************************************************************************
 *				DragFinish		[SHELL.12]
 */
void WINAPI DragFinish16(HDROP16 h)
{
    TRACE("\n");
    GlobalFree16((HGLOBAL16)h);
}


/*************************************************************************
 *				DragQueryPoint		[SHELL.13]
 */
BOOL16 WINAPI DragQueryPoint16(HDROP16 hDrop, POINT16 *p)
{
  LPDROPFILESTRUCT16 lpDropFileStruct;
  BOOL16           bRet;
  TRACE("\n");
  lpDropFileStruct = (LPDROPFILESTRUCT16) GlobalLock16(hDrop);

  memcpy(p,&lpDropFileStruct->ptMousePos,sizeof(POINT16));
  bRet = lpDropFileStruct->fInNonClientArea;

  GlobalUnlock16(hDrop);
  return bRet;
}

/*************************************************************************
 *             FindExecutable   (SHELL.21)
 */
HINSTANCE16 WINAPI FindExecutable16( LPCSTR lpFile, LPCSTR lpDirectory,
                                     LPSTR lpResult )
{ return (HINSTANCE16)FindExecutableA( lpFile, lpDirectory, lpResult );
}


/*************************************************************************
 *             AboutDlgProc   (SHELL.33)
 */
BOOL16 WINAPI AboutDlgProc16( HWND16 hWnd, UINT16 msg, WPARAM16 wParam,
                               LPARAM lParam )
{ return AboutDlgProc( hWnd, msg, wParam, lParam );
}


/*************************************************************************
 *             ShellAbout   (SHELL.22)
 */
BOOL16 WINAPI ShellAbout16( HWND16 hWnd, LPCSTR szApp, LPCSTR szOtherStuff,
                            HICON16 hIcon )
{ return ShellAboutA( hWnd, szApp, szOtherStuff, hIcon );
}

/*************************************************************************
 *			InternalExtractIcon		[SHELL.39]
 *
 * This abortion is called directly by Progman
 */
HGLOBAL16 WINAPI InternalExtractIcon16(HINSTANCE16 hInstance,
                                     LPCSTR lpszExeFileName, UINT16 nIconIndex, WORD n )
{
    HGLOBAL16 hRet = 0;
    HICON16 *RetPtr = NULL;
    OFSTRUCT ofs;
    HFILE hFile;

	TRACE("(%04x,file %s,start %d,extract %d\n",
		       hInstance, lpszExeFileName, nIconIndex, n);

	if( !n )
	  return 0;

	hFile = OpenFile( lpszExeFileName, &ofs, OF_READ|OF_EXIST );

	hRet = GlobalAlloc16( GMEM_FIXED | GMEM_ZEROINIT, sizeof(HICON16)*n);
	RetPtr = (HICON16*)GlobalLock16(hRet);

	if (hFile == HFILE_ERROR)
	{ /* not found - load from builtin module if available */
	  HINSTANCE hInst = (HINSTANCE)LoadLibrary16(lpszExeFileName);

	  if (hInst < 32) /* hmm, no Win16 module - try Win32 :-) */
	    hInst = LoadLibraryA(lpszExeFileName);
	  if (hInst)
	  {
	    int i;
	    for (i=nIconIndex; i < nIconIndex + n; i++)
	      RetPtr[i-nIconIndex] =
		      (HICON16)LoadIconA(hInst, (LPCSTR)(DWORD)i);
	    FreeLibrary(hInst);
	    return hRet;
	  }
          GlobalFree16( hRet );
	  return 0;
	}

        if (nIconIndex == (UINT16)-1)  /* get number of icons */
        {
            RetPtr[0] = PrivateExtractIconsA( ofs.szPathName, -1, 0, 0, NULL, 0, 0, 0 );
        }
        else
        {
            HRESULT res;
            HICON *icons;
            icons = HeapAlloc( GetProcessHeap(), 0, n * sizeof(*icons) );
            res = PrivateExtractIconsA( ofs.szPathName, nIconIndex,
                                        GetSystemMetrics(SM_CXICON),
                                        GetSystemMetrics(SM_CYICON),
                                        icons, 0, n, 0 );
            if (!res)
            {
                int i;
                for (i = 0; i < n; i++) RetPtr[i] = (HICON16)icons[i];
            }
            else
            {
                GlobalFree16( hRet );
                hRet = 0;
            }
            HeapFree( GetProcessHeap(), 0, icons );
        }
        return hRet;
}

/*************************************************************************
 *             ExtractIcon   (SHELL.34)
 */
HICON16 WINAPI ExtractIcon16( HINSTANCE16 hInstance, LPCSTR lpszExeFileName,
	UINT16 nIconIndex )
{   TRACE("\n");
    return ExtractIconA( hInstance, lpszExeFileName, nIconIndex );
}

/*************************************************************************
 *             ExtractIconEx   (SHELL.40)
 */
HICON16 WINAPI ExtractIconEx16(
	LPCSTR lpszFile, INT16 nIconIndex, HICON16 *phiconLarge,
	HICON16 *phiconSmall, UINT16 nIcons
) {
    HICON	*ilarge,*ismall;
    UINT16	ret;
    int		i;

    if (phiconLarge)
    	ilarge = (HICON*)HeapAlloc(GetProcessHeap(),0,nIcons*sizeof(HICON));
    else
    	ilarge = NULL;
    if (phiconSmall)
    	ismall = (HICON*)HeapAlloc(GetProcessHeap(),0,nIcons*sizeof(HICON));
    else
    	ismall = NULL;
    ret = ExtractIconExA(lpszFile,nIconIndex,ilarge,ismall,nIcons);
    if (ilarge) {
    	for (i=0;i<nIcons;i++)
	    phiconLarge[i]=ilarge[i];
	HeapFree(GetProcessHeap(),0,ilarge);
    }
    if (ismall) {
    	for (i=0;i<nIcons;i++)
	    phiconSmall[i]=ismall[i];
	HeapFree(GetProcessHeap(),0,ismall);
    }
    return ret;
}

/*************************************************************************
 *				ExtractAssociatedIcon	[SHELL.36]
 *
 * Return icon for given file (either from file itself or from associated
 * executable) and patch parameters if needed.
 */
HICON16 WINAPI ExtractAssociatedIcon16(HINSTANCE16 hInst, LPSTR lpIconPath, LPWORD lpiIcon)
{	HICON16 hIcon;
	WORD wDummyIcon = 0;

	TRACE("\n");

	if(lpiIcon == NULL)
	    lpiIcon = &wDummyIcon;

	hIcon = ExtractIcon16(hInst, lpIconPath, *lpiIcon);

	if( hIcon < 2 )
	{ if( hIcon == 1 ) /* no icons found in given file */
	  { char  tempPath[0x80];
	    UINT16  uRet = FindExecutable16(lpIconPath,NULL,tempPath);

	    if( uRet > 32 && tempPath[0] )
	    { strcpy(lpIconPath,tempPath);
	      hIcon = ExtractIcon16(hInst, lpIconPath, *lpiIcon);
	      if( hIcon > 2 )
	        return hIcon;
	    }
	    else hIcon = 0;
	  }

	  if( hIcon == 1 )
	    *lpiIcon = 2;   /* MSDOS icon - we found .exe but no icons in it */
	  else
	    *lpiIcon = 6;   /* generic icon - found nothing */

	  GetModuleFileName16(hInst, lpIconPath, 0x80);
	  hIcon = LoadIconA( hInst, MAKEINTRESOURCEA(*lpiIcon));
	}
	return hIcon;
}

/*************************************************************************
 *				FindEnvironmentString	[SHELL.38]
 *
 * Returns a pointer into the DOS environment... Ugh.
 */
LPSTR SHELL_FindString(LPSTR lpEnv, LPCSTR entry)
{ UINT16 l;

  TRACE("\n");

  l = strlen(entry);
  for( ; *lpEnv ; lpEnv+=strlen(lpEnv)+1 )
  { if( strncasecmp(lpEnv, entry, l) )
      continue;
	if( !*(lpEnv+l) )
	    return (lpEnv + l); 		/* empty entry */
	else if ( *(lpEnv+l)== '=' )
	    return (lpEnv + l + 1);
    }
    return NULL;
}

/**********************************************************************/

SEGPTR WINAPI FindEnvironmentString16(LPSTR str)
{ SEGPTR  spEnv;
  LPSTR lpEnv,lpString;
  TRACE("\n");

  spEnv = GetDOSEnvironment16();

  lpEnv = MapSL(spEnv);
  lpString = (spEnv)?SHELL_FindString(lpEnv, str):NULL;

    if( lpString )		/*  offset should be small enough */
	return spEnv + (lpString - lpEnv);
    return (SEGPTR)NULL;
}

/*************************************************************************
 *              		DoEnvironmentSubst      [SHELL.37]
 *
 * Replace %KEYWORD% in the str with the value of variable KEYWORD
 * from "DOS" environment.
 */
DWORD WINAPI DoEnvironmentSubst16(LPSTR str,WORD length)
{
  LPSTR   lpEnv = MapSL(GetDOSEnvironment16());
  LPSTR   lpBuffer = (LPSTR)HeapAlloc( GetProcessHeap(), 0, length);
  LPSTR   lpstr = str;
  LPSTR   lpbstr = lpBuffer;

  CharToOemA(str,str);

  TRACE("accept %s\n", str);

  while( *lpstr && lpbstr - lpBuffer < length )
   {
     LPSTR lpend = lpstr;

     if( *lpstr == '%' )
       {
	  do { lpend++; } while( *lpend && *lpend != '%' );
	  if( *lpend == '%' && lpend - lpstr > 1 )	/* found key */
	    {
	       LPSTR lpKey;
	      *lpend = '\0';
	       lpKey = SHELL_FindString(lpEnv, lpstr+1);
	       if( lpKey )				/* found key value */
		 {
		   int l = strlen(lpKey);

		   if( l > length - (lpbstr - lpBuffer) - 1 )
		     {
           WARN("-- Env subst aborted - string too short\n");
		      *lpend = '%';
		       break;
		     }
		   strcpy(lpbstr, lpKey);
		   lpbstr += l;
		 }
	       else break;
	      *lpend = '%';
	       lpstr = lpend + 1;
	    }
	  else break;					/* back off and whine */

	  continue;
       }

     *lpbstr++ = *lpstr++;
   }

 *lpbstr = '\0';
  if( lpstr - str == strlen(str) )
    {
      strncpy(str, lpBuffer, length);
      length = 1;
    }
  else
      length = 0;

  TRACE("-- return %s\n", str);

  OemToCharA(str,str);
  HeapFree( GetProcessHeap(), 0, lpBuffer);

  /*  Return str length in the LOWORD
   *  and 1 in HIWORD if subst was successful.
   */
 return (DWORD)MAKELONG(strlen(str), length);
}

/*************************************************************************
 *				ShellHookProc		[SHELL.103]
 * System-wide WH_SHELL hook.
 */
LRESULT WINAPI ShellHookProc16(INT16 code, WPARAM16 wParam, LPARAM lParam)
{
    TRACE("%i, %04x, %08x\n", code, wParam,
						      (unsigned)lParam );
    if( SHELL_hHook && SHELL_hWnd )
    {
	UINT16	uMsg = 0;
        switch( code )
        {
	    case HSHELL_WINDOWCREATED:		uMsg = uMsgWndCreated;   break;
	    case HSHELL_WINDOWDESTROYED:	uMsg = uMsgWndDestroyed; break;
	    case HSHELL_ACTIVATESHELLWINDOW: 	uMsg = uMsgShellActivate;
        }
	PostMessageA( SHELL_hWnd, uMsg, wParam, 0 );
    }
    return CallNextHookEx16( SHELL_hHook, code, wParam, lParam );
}

/*************************************************************************
 *				RegisterShellHook	[SHELL.102]
 */
BOOL WINAPI RegisterShellHook16(HWND16 hWnd, UINT16 uAction)
{
    TRACE("%04x [%u]\n", hWnd, uAction );

    switch( uAction )
    {
    case 2:  /* register hWnd as a shell window */
        if( !SHELL_hHook )
        {
            HMODULE16 hShell = GetModuleHandle16( "SHELL" );
            HOOKPROC16 hookProc = (HOOKPROC16)GetProcAddress16( hShell, (LPCSTR)103 );
            SHELL_hHook = SetWindowsHookEx16( WH_SHELL, hookProc, hShell, 0 );
            if ( SHELL_hHook )
            {
                uMsgWndCreated = RegisterWindowMessageA( lpstrMsgWndCreated );
                uMsgWndDestroyed = RegisterWindowMessageA( lpstrMsgWndDestroyed );
                uMsgShellActivate = RegisterWindowMessageA( lpstrMsgShellActivate );
            }
            else
                WARN("-- unable to install ShellHookProc()!\n");
        }

        if ( SHELL_hHook )
            return ((SHELL_hWnd = hWnd) != 0);
        break;

    default:
        WARN("-- unknown code %i\n", uAction );
        SHELL_hWnd = 0; /* just in case */
    }
    return FALSE;
}


/***********************************************************************
 *           DriveType   (SHELL.262)
 */
UINT16 WINAPI DriveType16( UINT16 drive )
{
    UINT ret;
    char path[] = "A:\\";
    path[0] += drive;
    ret = GetDriveTypeA(path);
    switch(ret)  /* some values are not supported in Win16 */
    {
    case DRIVE_CDROM:
        ret = DRIVE_REMOTE;
        break;
    case DRIVE_NO_ROOT_DIR:
        ret = DRIVE_UNKNOWN;
        break;
    }
    return ret;
}
