/* shellstring.c
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "winerror.h"
#include "winreg.h"

#include "shlobj.h"
#include "shellapi.h"
#include "shell32_main.h"
#include "undocshell.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

/************************* STRRET functions ****************************/

/*
 * ***** NOTE *****
 *  These routines are identical to StrRetToBuf[AW] in dlls/shlwapi/string.c.
 *  They were duplicated here because not every version of Shlwapi.dll exports
 *  StrRetToBuf[AW]. If you change one routine, change them both. YOU HAVE BEEN
 *  WARNED.
 * ***** NOTE *****
 */

HRESULT WINAPI StrRetToStrNA (LPVOID dest, DWORD len, LPSTRRET src, const ITEMIDLIST *pidl)
{
	TRACE("dest=0x%p len=0x%lx strret=0x%p pidl=%p stub\n",dest,len,src,pidl);

	switch (src->uType)
	{
	  case STRRET_WSTR:
	    WideCharToMultiByte(CP_ACP, 0, src->u.pOleStr, -1, (LPSTR)dest, len, NULL, NULL);
/*	    SHFree(src->u.pOleStr);  FIXME: is this right? */
	    break;

	  case STRRET_CSTRA:
	    lstrcpynA((LPSTR)dest, src->u.cStr, len);
	    break;

	  case STRRET_OFFSETA:
	    lstrcpynA((LPSTR)dest, ((LPCSTR)&pidl->mkid)+src->u.uOffset, len);
	    break;

	  default:
	    FIXME("unknown type!\n");
	    if (len)
	    {
	      *(LPSTR)dest = '\0';
	    }
	    return(FALSE);
	}
	return S_OK;
}

/************************************************************************/

HRESULT WINAPI StrRetToStrNW (LPVOID dest1, DWORD len, LPSTRRET src, const ITEMIDLIST *pidl)
{
    LPWSTR dest = (LPWSTR) dest1;
	TRACE("dest=0x%p len=0x%lx strret=0x%p pidl=%p stub\n",dest,len,src,pidl);

	switch (src->uType)
	{
	  case STRRET_WSTR:
	    lstrcpynW((LPWSTR)dest, src->u.pOleStr, len);
/*	    SHFree(src->u.pOleStr);  FIXME: is this right? */
	    break;

	  case STRRET_CSTRA:
              if (!MultiByteToWideChar( CP_ACP, 0, src->u.cStr, -1, dest, len ) && len)
                  dest[len-1] = 0;
	    break;

	  case STRRET_OFFSETA:
	    if (pidl)
	    {
              if (!MultiByteToWideChar( CP_ACP, 0, ((LPCSTR)&pidl->mkid)+src->u.uOffset, -1,
                                        dest, len ) && len)
                  dest[len-1] = 0;
	    }
	    break;

	  default:
	    FIXME("unknown type!\n");
	    if (len)
	    { *(LPSTR)dest = '\0';
	    }
	    return(FALSE);
	}
	return S_OK;
}


/*************************************************************************
 * StrRetToStrN				[SHELL32.96]
 *
 * converts a STRRET to a normal string
 *
 * NOTES
 *  the pidl is for STRRET OFFSET
 */
HRESULT WINAPI StrRetToStrNAW (LPVOID dest, DWORD len, LPSTRRET src, const ITEMIDLIST *pidl)
{
	if(SHELL_OsIsUnicode())
	  return StrRetToStrNW (dest, len, src, pidl);
	return StrRetToStrNA (dest, len, src, pidl);
}

/************************* OLESTR functions ****************************/

/************************************************************************
 *	StrToOleStr			[SHELL32.163]
 *
 */
int WINAPI StrToOleStrA (LPWSTR lpWideCharStr, LPCSTR lpMultiByteString)
{
	TRACE("(%p, %p %s)\n",
	lpWideCharStr, lpMultiByteString, debugstr_a(lpMultiByteString));

	return MultiByteToWideChar(0, 0, lpMultiByteString, -1, lpWideCharStr, MAX_PATH);

}
int WINAPI StrToOleStrW (LPWSTR lpWideCharStr, LPCWSTR lpWString)
{
	TRACE("(%p, %p %s)\n",
	lpWideCharStr, lpWString, debugstr_w(lpWString));

	strcpyW (lpWideCharStr, lpWString );
	return strlenW(lpWideCharStr);
}

BOOL WINAPI StrToOleStrAW (LPWSTR lpWideCharStr, LPCVOID lpString)
{
	if (SHELL_OsIsUnicode())
	  return StrToOleStrW (lpWideCharStr, lpString);
	return StrToOleStrA (lpWideCharStr, lpString);
}

/*************************************************************************
 * StrToOleStrN					[SHELL32.79]
 *  lpMulti, nMulti, nWide [IN]
 *  lpWide [OUT]
 */
BOOL WINAPI StrToOleStrNA (LPWSTR lpWide, INT nWide, LPCSTR lpStrA, INT nStr)
{
	TRACE("(%p, %x, %s, %x)\n", lpWide, nWide, debugstr_an(lpStrA,nStr), nStr);
	return MultiByteToWideChar (0, 0, lpStrA, nStr, lpWide, nWide);
}
BOOL WINAPI StrToOleStrNW (LPWSTR lpWide, INT nWide, LPCWSTR lpStrW, INT nStr)
{
	TRACE("(%p, %x, %s, %x)\n", lpWide, nWide, debugstr_wn(lpStrW, nStr), nStr);

	if (lstrcpynW (lpWide, lpStrW, nWide))
	{ return lstrlenW (lpWide);
	}
	return 0;
}

BOOL WINAPI StrToOleStrNAW (LPWSTR lpWide, INT nWide, LPCVOID lpStr, INT nStr)
{
	if (SHELL_OsIsUnicode())
	  return StrToOleStrNW (lpWide, nWide, lpStr, nStr);
	return StrToOleStrNA (lpWide, nWide, lpStr, nStr);
}

/*************************************************************************
 * OleStrToStrN					[SHELL32.78]
 */
BOOL WINAPI OleStrToStrNA (LPSTR lpStr, INT nStr, LPCWSTR lpOle, INT nOle)
{
	TRACE("(%p, %x, %s, %x)\n", lpStr, nStr, debugstr_wn(lpOle,nOle), nOle);
	return WideCharToMultiByte (0, 0, lpOle, nOle, lpStr, nStr, NULL, NULL);
}

BOOL WINAPI OleStrToStrNW (LPWSTR lpwStr, INT nwStr, LPCWSTR lpOle, INT nOle)
{
	TRACE("(%p, %x, %s, %x)\n", lpwStr, nwStr, debugstr_wn(lpOle,nOle), nOle);

	if (lstrcpynW ( lpwStr, lpOle, nwStr))
	{ return lstrlenW (lpwStr);
	}
	return 0;
}

BOOL WINAPI OleStrToStrNAW (LPVOID lpOut, INT nOut, LPCVOID lpIn, INT nIn)
{
	if (SHELL_OsIsUnicode())
	  return OleStrToStrNW (lpOut, nOut, lpIn, nIn);
	return OleStrToStrNA (lpOut, nOut, lpIn, nIn);
}


/*************************************************************************
 * CheckEscapes [SHELL32]
 */
DWORD WINAPI CheckEscapesA(
    LPSTR    string,         /* [in]    string to check ??*/
           DWORD    b,              /* [???]   is 0 */
           DWORD    c,              /* [???]   is 0 */
           LPDWORD  d,              /* [???]   is address */
           LPDWORD  e,              /* [???]   is address */
           DWORD    handle )        /* [in]    looks like handle but not */
{
    FIXME("(%p<%s> %ld %ld %p<%ld> %p<%ld> 0x%08lx) stub\n",
   string, debugstr_a(string),
   b,
   c,
   d, (d) ? *d : 0xabbacddc,
   e, (e) ? *e : 0xabbacddd,
   handle);
    return 0;
}

DWORD WINAPI CheckEscapesW(
    LPWSTR   string,         /* [in]    string to check ??*/
           DWORD    b,              /* [???]   is 0 */
           DWORD    c,              /* [???]   is 0 */
           LPDWORD  d,              /* [???]   is address */
           LPDWORD  e,              /* [???]   is address */
           DWORD    handle )        /* [in]    looks like handle but not */
{
    FIXME("(%p<%s> %ld %ld %p<%ld> %p<%ld> 0x%08lx) stub\n",
   string, debugstr_w(string),
   b,
   c,
   d, (d) ? *d : 0xabbacddc,
   e, (e) ? *e : 0xabbacddd,
   handle);
    return 0;
}
