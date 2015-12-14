/*
 * SHLWAPI ordinal functions
 *
 * Copyright 1997 Marcus Meissner
 *           1998 J�rgen Schmied
 *           2001 Jon Griffiths
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2004-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winnls.h"
#include "winbase.h"
#include "winuser.h"
#include "ddeml.h"
#include "shlobj.h"
#include "shellapi.h"
#include "commdlg.h"
#include "wine/unicode.h"
#include "wine/obj_serviceprovider.h"
#include "wingdi.h"
#include "winreg.h"
#include "winuser.h"
#include "wine/debug.h"
#include "ordinal.h"
#include "shlwapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

extern HINSTANCE shlwapi_hInstance;
extern HMODULE SHLWAPI_hshell32;
extern HMODULE SHLWAPI_hwinmm;
extern HMODULE SHLWAPI_hcomdlg32;
extern HMODULE SHLWAPI_hmpr;
extern HMODULE SHLWAPI_hmlang;
extern HMODULE SHLWAPI_hversion;

extern DWORD SHLWAPI_ThreadRef_index;

typedef HANDLE HSHARED; /* Shared memory */

/* following is GUID for IObjectWithSite::SetSite  -- see _174           */
static DWORD id1[4] = {0xfc4801a3, 0x11cf2ba9, 0xaa0029a2, 0x52733d00};
/* following is GUID for IPersistMoniker::GetClassID  -- see _174        */
static DWORD id2[4] = {0x79eac9ee, 0x11cebaf9, 0xaa00828c, 0x0ba94b00};

/* The following schemes were identified in the native version of
 * SHLWAPI.DLL version 5.50
 */
typedef enum {
    URL_SCHEME_INVALID     = -1,
    URL_SCHEME_UNKNOWN     =  0,
    URL_SCHEME_FTP,
    URL_SCHEME_HTTP,
    URL_SCHEME_GOPHER,
    URL_SCHEME_MAILTO,
    URL_SCHEME_NEWS,
    URL_SCHEME_NNTP,
    URL_SCHEME_TELNET,
    URL_SCHEME_WAIS,
    URL_SCHEME_FILE,
    URL_SCHEME_MK,
    URL_SCHEME_HTTPS,
    URL_SCHEME_SHELL,
    URL_SCHEME_SNEWS,
    URL_SCHEME_LOCAL,
    URL_SCHEME_JAVASCRIPT,
    URL_SCHEME_VBSCRIPT,
    URL_SCHEME_ABOUT,
    URL_SCHEME_RES,
    URL_SCHEME_MAXVALUE
} URL_SCHEME;

typedef struct {
    URL_SCHEME  scheme_number;
    LPCSTR scheme_name;
} SHL_2_inet_scheme;

static const SHL_2_inet_scheme shlwapi_schemes[] = {
  {URL_SCHEME_FTP,        "ftp"},
  {URL_SCHEME_HTTP,       "http"},
  {URL_SCHEME_GOPHER,     "gopher"},
  {URL_SCHEME_MAILTO,     "mailto"},
  {URL_SCHEME_NEWS,       "news"},
  {URL_SCHEME_NNTP,       "nntp"},
  {URL_SCHEME_TELNET,     "telnet"},
  {URL_SCHEME_WAIS,       "wais"},
  {URL_SCHEME_FILE,       "file"},
  {URL_SCHEME_MK,         "mk"},
  {URL_SCHEME_HTTPS,      "https"},
  {URL_SCHEME_SHELL,      "shell"},
  {URL_SCHEME_SNEWS,      "snews"},
  {URL_SCHEME_LOCAL,      "local"},
  {URL_SCHEME_JAVASCRIPT, "javascript"},
  {URL_SCHEME_VBSCRIPT,   "vbscript"},
  {URL_SCHEME_ABOUT,      "about"},
  {URL_SCHEME_RES,        "res"},
  {0, 0}
};

/*
 NOTES: Most functions exported by ordinal seem to be superflous.
 The reason for these functions to be there is to provide a wraper
 for unicode functions to provide these functions on systems without
 unicode functions eg. win95/win98. Since we have such functions we just
 call these. If running Wine with native DLL's, some late bound calls may
 fail. However, its better to implement the functions in the forward DLL
 and recommend the builtin rather than reimplementing the calls here!
*/

/*************************************************************************
 *      @	[SHLWAPI.1]
 *
 * Identifies the Internet "scheme" in the passed string. ASCII based.
 * Also determines start and length of item after the ':'
 */
DWORD WINAPI SHLWAPI_1 (LPCSTR x, UNKNOWN_SHLWAPI_1 *y)
{
    DWORD cnt;
    const SHL_2_inet_scheme *inet_pro;

    y->fcncde = URL_SCHEME_INVALID;
    if (y->size != 0x18) return E_INVALIDARG;
    /* FIXME: leading white space generates error of 0x80041001 which
     *        is undefined
     */
    if (*x <= ' ') return 0x80041001;
    cnt = 0;
    y->sizep1 = 0;
    y->ap1 = x;
    while (*x) {
	if (*x == ':') {
	    y->sizep1 = cnt;
	    cnt = -1;
	    y->ap2 = x+1;
	    break;
	}
	x++;
	cnt++;
    }

    /* check for no scheme in string start */
    /* (apparently schemes *must* be larger than a single character)  */
    if ((*x == '\0') || (y->sizep1 <= 1)) {
	y->ap1 = 0;
	return 0x80041001;
    }

    /* found scheme, set length of remainder */
    y->sizep2 = lstrlenA(y->ap2);

    /* see if known scheme and return indicator number */
    y->fcncde = URL_SCHEME_UNKNOWN;
    inet_pro = shlwapi_schemes;
    while (inet_pro->scheme_name) {
	if (!strncasecmp(inet_pro->scheme_name, y->ap1,
		    min(y->sizep1, lstrlenA(inet_pro->scheme_name)))) {
	    y->fcncde = inet_pro->scheme_number;
	    break;
	}
	inet_pro++;
    }
    return S_OK;
}

/*************************************************************************
 *      @	[SHLWAPI.2]
 *
 * Identifies the Internet "scheme" in the passed string. UNICODE based.
 * Also determines start and length of item after the ':'
 */
DWORD WINAPI SHLWAPI_2 (LPCWSTR x, UNKNOWN_SHLWAPI_2 *y)
{
    DWORD cnt;
    const SHL_2_inet_scheme *inet_pro;
    LPSTR cmpstr;
    INT len;

    y->fcncde = URL_SCHEME_INVALID;
    if (y->size != 0x18) return E_INVALIDARG;
    /* FIXME: leading white space generates error of 0x80041001 which
     *        is undefined
     */
    if (*x <= L' ') return 0x80041001;
    cnt = 0;
    y->sizep1 = 0;
    y->ap1 = x;
    while (*x) {
	if (*x == L':') {
	    y->sizep1 = cnt;
	    cnt = -1;
	    y->ap2 = x+1;
	    break;
	}
	x++;
	cnt++;
    }

    /* check for no scheme in string start */
    /* (apparently schemes *must* be larger than a single character)  */
    if ((*x == L'\0') || (y->sizep1 <= 1)) {
	y->ap1 = 0;
	return 0x80041001;
    }

    /* found scheme, set length of remainder */
    y->sizep2 = lstrlenW(y->ap2);

    /* see if known scheme and return indicator number */
    len = WideCharToMultiByte(0, 0, y->ap1, y->sizep1, 0, 0, 0, 0);
    cmpstr = (LPSTR)HeapAlloc(GetProcessHeap(), 0, len+1);
    WideCharToMultiByte(0, 0, y->ap1, y->sizep1, cmpstr, len+1, 0, 0);
    y->fcncde = URL_SCHEME_UNKNOWN;
    inet_pro = shlwapi_schemes;
    while (inet_pro->scheme_name) {
	if (!strncasecmp(inet_pro->scheme_name, cmpstr,
		    min(len, lstrlenA(inet_pro->scheme_name)))) {
	    y->fcncde = inet_pro->scheme_number;
	    break;
	}
	inet_pro++;
    }
    HeapFree(GetProcessHeap(), 0, cmpstr);
    return S_OK;
}

/*************************************************************************
 * @	[SHLWAPI.3]
 *
 * Determine if a file exists locally and is of an executable type.
 *
 * PARAMS
 *  lpszFile       [O] File to search for
 *  dwWhich        [I] Type of executable to search for
 *
 * RETURNS
 *  TRUE  If the file was found. lpszFile contains the file name.
 *  FALSE Otherwise.
 *
 * NOTES
 *  lpszPath is modified in place and must be at least MAX_PATH in length.
 *  If the function returns FALSE, the path is modified to its orginal state.
 *  If the given path contains an extension or dwWhich is 0, executable
 *  extensions are not checked.
 *
 *  Ordinals 3-6 are a classic case of MS exposing limited functionality to
 *  users (here through PathFindOnPath) and keeping advanced functionality for
 *  their own developers exclusive use. Monopoly, anyone?
 */
BOOL WINAPI SHLWAPI_3(LPSTR lpszFile,DWORD dwWhich)
{
  return SHLWAPI_PathFindLocalExeA(lpszFile,dwWhich);
}

/*************************************************************************
 * @	[SHLWAPI.4]
 *
 * Unicode version of SHLWAPI_3.
 */
BOOL WINAPI SHLWAPI_4(LPWSTR lpszFile,DWORD dwWhich)
{
  return SHLWAPI_PathFindLocalExeW(lpszFile,dwWhich);
}

/*************************************************************************
 * @	[SHLWAPI.5]
 *
 * Search a range of paths for a specific type of executable.
 *
 * PARAMS
 *  lpszFile       [O] File to search for
 *  lppszOtherDirs [I] Other directories to look in
 *  dwWhich        [I] Type of executable to search for
 *
 * RETURNS
 *  Success: TRUE. The path to the executable is stored in sFile.
 *  Failure: FALSE. The path to the executable is unchanged.
 */
BOOL WINAPI SHLWAPI_5(LPSTR lpszFile,LPCSTR *lppszOtherDirs,DWORD dwWhich)
{
  return SHLWAPI_PathFindOnPathExA(lpszFile,lppszOtherDirs,dwWhich);
}

/*************************************************************************
 * @	[SHLWAPI.6]
 *
 * Unicode version of SHLWAPI_5.
 */
BOOL WINAPI SHLWAPI_6(LPWSTR lpszFile,LPCWSTR *lppszOtherDirs,DWORD dwWhich)
{
  return SHLWAPI_PathFindOnPathExW(lpszFile,lppszOtherDirs,dwWhich);
}

/*************************************************************************
 * SHLWAPI_DupSharedHandle
 *
 * Internal implemetation of SHLWAPI_11.
 */
static
HSHARED WINAPI SHLWAPI_DupSharedHandle(HSHARED hShared, DWORD dwDstProcId,
                                       DWORD dwSrcProcId, DWORD dwAccess,
                                       DWORD dwOptions)
{
  HANDLE hDst, hSrc;
  DWORD dwMyProcId = GetCurrentProcessId();
  HSHARED hRet = (HSHARED)NULL;

  TRACE("(%p,%ld,%ld,%08lx,%08lx)\n", (PVOID)hShared, dwDstProcId, dwSrcProcId,
        dwAccess, dwOptions);

  /* Get dest process handle */
  if (dwDstProcId == dwMyProcId)
    hDst = GetCurrentProcess();
  else
    hDst = OpenProcess(PROCESS_DUP_HANDLE, 0, dwDstProcId);

  if (hDst)
  {
    /* Get src process handle */
    if (dwSrcProcId == dwMyProcId)
      hSrc = GetCurrentProcess();
    else
      hSrc = OpenProcess(PROCESS_DUP_HANDLE, 0, dwSrcProcId);

    if (hSrc)
    {
      /* Make handle available to dest process */
      if (!DuplicateHandle(hDst, (HANDLE)hShared, hSrc, &hRet,
                           dwAccess, 0, dwOptions | DUPLICATE_SAME_ACCESS))
        hRet = (HSHARED)NULL;

      if (dwSrcProcId != dwMyProcId)
        CloseHandle(hSrc);
    }

    if (dwDstProcId != dwMyProcId)
      CloseHandle(hDst);
  }

  TRACE("Returning handle %p\n", (PVOID)hRet);
  return hRet;
}

/*************************************************************************
 * @  [SHLWAPI.7]
 *
 * Create a block of sharable memory and initialise it with data.
 *
 * PARAMS
 * dwProcId [I] ID of process owning data
 * lpvData  [I] Pointer to data to write
 * dwSize   [I] Size of data
 *
 * RETURNS
 * Success: A shared memory handle
 * Failure: NULL
 *
 * NOTES
 * Ordinals 7-11 provide a set of calls to create shared memory between a
 * group of processes. The shared memory is treated opaquely in that its size
 * is not exposed to clients who map it. This is accomplished by storing
 * the size of the map as the first DWORD of mapped data, and then offsetting
 * the view pointer returned by this size.
 *
 * SHLWAPI_7/SHLWAPI_10 - Create/Destroy the shared memory handle
 * SHLWAPI_8/SHLWAPI_9  - Get/Release a pointer to the shared data
 * SHLWAPI_11           - Helper function; Duplicate cross-process handles
   */
HSHARED WINAPI SHLWAPI_7 (DWORD dwProcId, DWORD dwSize, LPCVOID lpvData)
{
  HANDLE hMap;
  LPVOID pMapped;
  HSHARED hRet = (HSHARED)NULL;

  TRACE("(%ld,%p,%ld)\n", dwProcId, lpvData, dwSize);

  /* Create file mapping of the correct length */
  hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, FILE_MAP_READ, 0,
                            dwSize + sizeof(dwSize), NULL);
  if (!hMap)
    return hRet;

  /* Get a view in our process address space */
  pMapped = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);

  if (pMapped)
  {
    /* Write size of data, followed by the data, to the view */
    *((DWORD*)pMapped) = dwSize;
    if (dwSize)
      memcpy((char *) pMapped + sizeof(dwSize), lpvData, dwSize);

    /* Release view. All further views mapped will be opaque */
    UnmapViewOfFile(pMapped);
    hRet = SHLWAPI_DupSharedHandle((HSHARED)hMap, dwProcId,
                                   GetCurrentProcessId(), FILE_MAP_ALL_ACCESS,
                                   DUPLICATE_SAME_ACCESS);
  }

  CloseHandle(hMap);
  return hRet;
}

/*************************************************************************
 * @ [SHLWAPI.8]
 *
 * Get a pointer to a block of shared memory from a shared memory handle.
 *
 * PARAMS
 * hShared  [I] Shared memory handle
 * dwProcId [I] ID of process owning hShared
 *
 * RETURNS
 * Success: A pointer to the shared memory
 * Failure: NULL
 *
 * NOTES
 * See SHLWAPI_7.
   */
PVOID WINAPI SHLWAPI_8 (HSHARED hShared, DWORD dwProcId)
  {
  HSHARED hDup;
  LPVOID pMapped;

  TRACE("(%p %ld)\n", (PVOID)hShared, dwProcId);

  /* Get handle to shared memory for current process */
  hDup = SHLWAPI_DupSharedHandle(hShared, dwProcId, GetCurrentProcessId(),
                                 FILE_MAP_ALL_ACCESS, 0);
  /* Get View */
  pMapped = MapViewOfFile((HANDLE)hDup, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
  CloseHandle(hDup);

  if (pMapped)
    return (char *) pMapped + sizeof(DWORD); /* Hide size */
  return NULL;
}

/*************************************************************************
 * @ [SHLWAPI.9]
 *
 * Release a pointer to a block of shared memory.
 *
 * PARAMS
 * lpView [I] Shared memory pointer
 *
 * RETURNS
 * Success: TRUE
 * Failure: FALSE
 *
 * NOTES
 * See SHLWAPI_7.
 */
BOOL WINAPI SHLWAPI_9 (LPVOID lpView)
{
  TRACE("(%p)\n", lpView);
  return UnmapViewOfFile((char *) lpView - sizeof(DWORD)); /* Include size */
}

/*************************************************************************
 * @ [SHLWAPI.10]
 *
 * Destroy a block of sharable memory.
 *
 * PARAMS
 * hShared  [I] Shared memory handle
 * dwProcId [I] ID of process owning hShared
 *
 * RETURNS
 * Success: TRUE
 * Failure: FALSE
 *
 * NOTES
 * See SHLWAPI_7.
 */
BOOL WINAPI SHLWAPI_10 (HSHARED hShared, DWORD dwProcId)
{
  HSHARED hClose;

  TRACE("(%p %ld)\n", (PVOID)hShared, dwProcId);

  /* Get a copy of the handle for our process, closing the source handle */
  hClose = SHLWAPI_DupSharedHandle(hShared, dwProcId, GetCurrentProcessId(),
                                   FILE_MAP_ALL_ACCESS,DUPLICATE_CLOSE_SOURCE);
  /* Close local copy */
  return CloseHandle((HANDLE)hClose);
}

/*************************************************************************
 * @   [SHLWAPI.11]
 *
 * Copy a sharable memory handle from one process to another.
 *
 * PARAMS
 * hShared     [I] Shared memory handle to duplicate
 * dwDstProcId [I] ID of the process wanting the duplicated handle
 * dwSrcProcId [I] ID of the process owning hShared
 * dwAccess    [I] Desired DuplicateHandle access
 * dwOptions   [I] Desired DuplicateHandle options
 *
 * RETURNS
 * Success: A handle suitable for use by the dwDstProcId process.
 * Failure: A NULL handle.
 *
 * NOTES
 * See SHLWAPI_7.
 */
HSHARED WINAPI SHLWAPI_11(HSHARED hShared, DWORD dwDstProcId, DWORD dwSrcProcId,
                          DWORD dwAccess, DWORD dwOptions)
{
  HSHARED hRet;

  hRet = SHLWAPI_DupSharedHandle(hShared, dwDstProcId, dwSrcProcId,
                                 dwAccess, dwOptions);
  return hRet;
}

/*************************************************************************
 *      @	[SHLWAPI.13]
 * (Used by IE4 during startup)
 */
HRESULT WINAPI SHLWAPI_13 (
	LPVOID w,
	LPVOID x)
{
	FIXME("(%p %p)stub\n",w,x);
	return 1;
#if 0
	/* pseudo code extracted from relay trace */
	RegOpenKeyA(HKLM, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\Aceepted Documents", &newkey);
	ret = 0;
	i = 0;
	size = 0;
	while(!ret) {
	    ret = RegEnumValueA(newkey, i, a1, a2, 0, a3, 0, 0);
	    size += ???;
	    i++;
	}
	b1 = LocalAlloc(0x40, size);
	ret = 0;
	i = 0;
	while(!ret) {
	    ret = RegEnumValueA(newkey, i, a1, a2, 0, a3, a4, a5);
	    RegisterClipBoardFormatA(a4);
	    i++;
	}
	hwnd1 = GetModuleHandleA("URLMON.DLL");
	proc = GetProcAddress(hwnd1, "CreateFormatEnumerator");
	HeapAlloc(??, 0, 0x14);
	HeapAlloc(??, 0, 0x50);
	LocalAlloc(0x40, 0x78);
	/* FIXME: bad string below */
	lstrlenW(L"{D0FCA420-D3F5-11CF-B211-00AA004AE837}");
	StrCpyW(a6,  L"{D0FCA420-D3F5-11CF-B211-00AA004AE837}");

	GetTickCount();
	IsBadReadPtr(c1 = 0x403fd210,4);
	InterlockedIncrement(c1+4);
	LocalFree(b1);
	RegCloseKey(newkey);
	IsBadReadPtr(c1 = 0x403fd210,4);
	InterlockedIncrement(c1+4);

	HeapAlloc(40350000,00000000,00000014) retval=403fd0a8;
	HeapAlloc(40350000,00000000,00000050) retval=403feb44;
	hwnd1 = GetModuleHandleA("URLMON.DLL");
	proc = GetProcAddress(hwnd1, "RegisterFormatEnumerator");
	/* 0x1a40c88c is in URLMON.DLL just before above proc
	 * content is L"_EnumFORMATETC_"
	 * label is d1
	 */
        IsBadReadPtr(d1 = 0x1a40c88c,00000002);
	lstrlenW(d1);
	lstrlenW(d1);
	HeapAlloc(40350000,00000000,0000001e) retval=403fed44;
	IsBadReadPtr(d2 = 0x403fd0a8,00000004);
	InterlockedIncrement(d2+4);
	IsBadReadPtr(d2 = 0x403fd0a8,00000004);
	InterlockedDecrement(d2+4);
	IsBadReadPtr(c1,00000004);
	InterlockedDecrement(c1+4);
	IsBadReadPtr(c1,00000004);
	InterlockedDecrement(c1+4);

#endif
}

/*************************************************************************
 *      @	[SHLWAPI.14]
 *
 * Function:
 *    Retrieves IE "AcceptLanguage" value from registry. ASCII mode.
 *
 */
HRESULT WINAPI SHLWAPI_14 (
	LPSTR langbuf,
	LPDWORD buflen)
{
	CHAR *mystr;
	DWORD mystrlen, mytype;
	HKEY mykey;
	LCID mylcid;

	mystrlen = (*buflen > 6) ? *buflen : 6;
	mystr = (CHAR*)HeapAlloc(GetProcessHeap(),
				 HEAP_ZERO_MEMORY, mystrlen);
	RegOpenKeyA(HKEY_CURRENT_USER,
		    "Software\\Microsoft\\Internet Explorer\\International",
		    &mykey);
	if (RegQueryValueExA(mykey, "AcceptLanguage",
			      0, &mytype, mystr, &mystrlen)) {
	    /* Did not find value */
	    mylcid = GetUserDefaultLCID();
	    /* somehow the mylcid translates into "en-us"
	     *  this is similar to "LOCALE_SABBREVLANGNAME"
	     *  which could be gotten via GetLocaleInfo.
	     *  The only problem is LOCALE_SABBREVLANGUAGE" is
	     *  a 3 char string (first 2 are country code and third is
	     *  letter for "sublanguage", which does not come close to
	     *  "en-us"
	     */
	    lstrcpyA(mystr, "en-us");
	    mystrlen = lstrlenA(mystr);
	}
	else {
	    /* handle returned string */
	    FIXME("missing code\n");
	}
	if (mystrlen > *buflen)
	    lstrcpynA(langbuf, mystr, *buflen);
	else {
	    lstrcpyA(langbuf, mystr);
	    *buflen = lstrlenA(langbuf);
	}
	RegCloseKey(mykey);
	HeapFree(GetProcessHeap(), 0, mystr);
	TRACE("language is %s\n", debugstr_a(langbuf));
	return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.15]
 *
 * Function:
 *    Retrieves IE "AcceptLanguage" value from registry. UNICODE mode.
 *
 */
HRESULT WINAPI SHLWAPI_15 (
	LPWSTR langbuf,
	LPDWORD buflen)
{
	CHAR *mystr;
	DWORD mystrlen, mytype;
	HKEY mykey;
	LCID mylcid;

	mystrlen = (*buflen > 6) ? *buflen : 6;
	mystr = (CHAR*)HeapAlloc(GetProcessHeap(),
				 HEAP_ZERO_MEMORY, mystrlen);
	RegOpenKeyA(HKEY_CURRENT_USER,
		    "Software\\Microsoft\\Internet Explorer\\International",
		    &mykey);
	if (RegQueryValueExA(mykey, "AcceptLanguage",
			      0, &mytype, mystr, &mystrlen)) {
	    /* Did not find value */
	    mylcid = GetUserDefaultLCID();
	    /* somehow the mylcid translates into "en-us"
	     *  this is similar to "LOCALE_SABBREVLANGNAME"
	     *  which could be gotten via GetLocaleInfo.
	     *  The only problem is LOCALE_SABBREVLANGUAGE" is
	     *  a 3 char string (first 2 are country code and third is
	     *  letter for "sublanguage", which does not come close to
	     *  "en-us"
	     */
	    lstrcpyA(mystr, "en-us");
	    mystrlen = lstrlenA(mystr);
	}
	else {
	    /* handle returned string */
	    FIXME("missing code\n");
	}
	RegCloseKey(mykey);
	*buflen = MultiByteToWideChar(0, 0, mystr, -1, langbuf, (*buflen)-1);
	HeapFree(GetProcessHeap(), 0, mystr);
	TRACE("language is %s\n", debugstr_w(langbuf));
	return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.16]
 */
HRESULT WINAPI SHLWAPI_16 (
	LPVOID w,
	LPVOID x,
	LPVOID y,
	LPWSTR z)
{
	FIXME("(%p %p %p %p)stub\n",w,x,y,z);
	return 0xabba1252;
}

/*************************************************************************
 *      @	[SHLWAPI.18]
 *
 *  w is pointer to address of callback routine
 *  x is pointer to LPVOID to receive address of locally allocated
 *         space size 0x14
 *  return is 0 (unless out of memory???)
 *
 * related to _19, _21 and _22 below
 *  only seen invoked by SHDOCVW
 */
LONG WINAPI SHLWAPI_18 (
	LPVOID *w,
	LPVOID x)
{
	FIXME("(%p %p)stub\n",w,x);
	*((LPDWORD)x) = 0;
	return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.19]
 *
 *  w is address of allocated memory from _21
 *  return is 0 (unless out of memory???)
 *
 * related to _18, _21 and _22 below
 *  only seen invoked by SHDOCVW
 */
LONG WINAPI SHLWAPI_19 (
	LPVOID w)
{
	FIXME("(%p) stub\n",w);
	return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.21]
 *
 *  w points to space allocated via .18 above
 *      LocalSize is done on it (retrieves 18)
 *      LocalReAlloc is done on it to size 8 with LMEM_MOVEABLE & LMEM_ZEROINIT
 *  x values seen 0xa0000005
 *  returns 1
 *
 *  relates to _18, _19 and _22 above and below
 *   only seen invoked by SHDOCVW
 */
LONG WINAPI SHLWAPI_21 (
	LPVOID w,
	DWORD  x)
{
	FIXME("(%p %lx)stub\n",w,x);
	return 1;
}

/*************************************************************************
 *      @	[SHLWAPI.22]
 *
 *  return is 'w' value seen in x is 0xa0000005
 *
 *  relates to _18, _19 and _21 above
 *   only seen invoked by SHDOCVW
 */
LPVOID WINAPI SHLWAPI_22 (
	LPVOID w,
	DWORD  x)
{
	FIXME("(%p %lx)stub\n",w,x);
	return w;
}

/*************************************************************************
 *      @	[SHLWAPI.23]
 *
 * NOTES
 *	converts a guid to a string
 *	returns strlen(str)
 */
DWORD WINAPI SHLWAPI_23 (
	REFGUID guid,	/* [in]  clsid */
	LPSTR str,	/* [out] buffer */
	INT cmax)	/* [in]  size of buffer */
{
	char xguid[40];

        sprintf( xguid, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 guid->Data1, guid->Data2, guid->Data3,
                 guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                 guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
	TRACE("(%s %p 0x%08x)stub\n", xguid, str, cmax);
	if (strlen(xguid)>=cmax) return 0;
	strcpy(str,xguid);
	return strlen(xguid) + 1;
}

/*************************************************************************
 *      @	[SHLWAPI.24]
 *
 * NOTES
 *	converts a guid to a string
 *	returns strlen(str)
 */
DWORD WINAPI SHLWAPI_24 (
	REFGUID guid,	/* [in]  clsid */
	LPWSTR str,	/* [out] buffer */
	INT cmax)	/* [in]  size of buffer */
{
    char xguid[40];

    sprintf( xguid, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
             guid->Data1, guid->Data2, guid->Data3,
             guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
             guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
    return MultiByteToWideChar( CP_ACP, 0, xguid, -1, str, cmax );
}

/*************************************************************************
 *      @	[SHLWAPI.25]
 *
 * Seems to be iswalpha
 */
BOOL WINAPI SHLWAPI_25(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_ALPHA) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.26]
 *
 * Seems to be iswupper
 */
BOOL WINAPI SHLWAPI_26(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_UPPER) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.27]
 *
 * Seems to be iswlower
 */
BOOL WINAPI SHLWAPI_27(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_LOWER) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.28]
 *
 * Seems to be iswalnum
 */
BOOL WINAPI SHLWAPI_28(WCHAR wc)
{
    return (get_char_typeW(wc) & (C1_ALPHA|C1_DIGIT)) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.29]
 *
 * Seems to be iswspace
 */
BOOL WINAPI SHLWAPI_29(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_SPACE) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.30]
 *
 * Seems to be iswblank
 */
BOOL WINAPI SHLWAPI_30(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_BLANK) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.31]
 *
 * Seems to be iswpunct
 */
BOOL WINAPI SHLWAPI_31(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_PUNCT) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.32]
 *
 * Seems to be iswcntrl
 */
BOOL WINAPI SHLWAPI_32(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_CNTRL) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.33]
 *
 * Seems to be iswdigit
 */
BOOL WINAPI SHLWAPI_33(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_DIGIT) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.34]
 *
 * Seems to be iswxdigit
 */
BOOL WINAPI SHLWAPI_34(WCHAR wc)
{
    return (get_char_typeW(wc) & C1_XDIGIT) != 0;
}

/*************************************************************************
 *      @	[SHLWAPI.35]
 *
 */
BOOL WINAPI SHLWAPI_35(LPVOID p1, DWORD dw2, LPVOID p3)
{
    FIXME("(%p, 0x%08lx, %p): stub\n", p1, dw2, p3);
    return TRUE;
}

/*************************************************************************
 *      @	[SHLWAPI.36]
 *
 */
BOOL WINAPI SHLWAPI_36(HMENU h1, UINT ui2, UINT h3, LPCWSTR p4)
{
    TRACE("(0x%08x, 0x%08x, 0x%08x, %s): stub\n",
	  h1, ui2, h3, debugstr_w(p4));
    return AppendMenuW(h1, ui2, h3, p4);
}

/*************************************************************************
 *      @	[SHLWAPI.74]
 *
 * Get the text from a given dialog item.
 */
INT WINAPI SHLWAPI_74(HWND hWnd, INT nItem, LPWSTR lpsDest,INT nDestLen)
{
  HWND hItem = GetDlgItem(hWnd, nItem);

  if (hItem)
    return GetWindowTextW(hItem, lpsDest, nDestLen);
  if (nDestLen)
    *lpsDest = (WCHAR)'\0';
  return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.151]
 * Function:  Compare two ASCII strings for "len" bytes.
 * Returns:   *str1-*str2  (case sensitive)
 */
DWORD WINAPI SHLWAPI_151(LPSTR str1, LPSTR str2, INT len)
{
    return strncmp( str1, str2, len );
}

/*************************************************************************
 *      @	[SHLWAPI.152]
 *
 * Function:  Compare two WIDE strings for "len" bytes.
 * Returns:   *str1-*str2  (case sensitive)
 */
DWORD WINAPI SHLWAPI_152(LPWSTR str1, LPWSTR str2, INT len)
{
    return strncmpW( str1, str2, len );
}

/*************************************************************************
 *      @	[SHLWAPI.153]
 * Function:  Compare two ASCII strings for "len" bytes via caseless compare.
 * Returns:   *str1-*str2  (case insensitive)
 */
DWORD WINAPI SHLWAPI_153(LPSTR str1, LPSTR str2, DWORD len)
{
    return strncasecmp( str1, str2, len );
}

/*************************************************************************
 *      @	[SHLWAPI.154]
 *
 * Function:  Compare two WIDE strings for "len" bytes via caseless compare.
 * Returns:   *str1-*str2  (case insensitive)
 */
DWORD WINAPI SHLWAPI_154(LPWSTR str1, LPWSTR str2, DWORD len)
{
    return strncmpiW( str1, str2, len );
}

/*************************************************************************
 *      @	[SHLWAPI.155]
 *
 *	Case sensitive string compare (ASCII). Does not SetLastError().
 */
DWORD WINAPI SHLWAPI_155 ( LPSTR str1, LPSTR str2)
{
    return strcmp(str1, str2);
}

/*************************************************************************
 *      @	[SHLWAPI.156]
 *
 *	Case sensitive string compare. Does not SetLastError().
 */
DWORD WINAPI SHLWAPI_156 ( LPWSTR str1, LPWSTR str2)
{
    return strcmpW( str1, str2 );
}

/*************************************************************************
 *      @	[SHLWAPI.158]
 *
 *	Case insensitive string compare. Does not SetLastError(). ??
 */
DWORD WINAPI SHLWAPI_158 ( LPWSTR str1, LPWSTR str2)
{
    return strcmpiW( str1, str2 );
}

/*************************************************************************
 *      @	[SHLWAPI.162]
 *
 * Ensure a multibyte character string doesn't end in a hanging lead byte.
 */
DWORD WINAPI SHLWAPI_162(LPSTR lpStr, DWORD size)
{
  if (lpStr && size)
  {
    LPSTR lastByte = lpStr + size - 1;

    while(lpStr < lastByte)
      lpStr += IsDBCSLeadByte(*lpStr) ? 2 : 1;

    if(lpStr == lastByte && IsDBCSLeadByte(*lpStr))
    {
      *lpStr = '\0';
      size--;
    }
    return size;
  }
  return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.164]
 */
DWORD WINAPI SHLWAPI_164 (
	LPVOID u,
	LPVOID v,
	LPVOID w,
	LPVOID x,
	LPVOID y,
	LPVOID z)
{
        TRACE("(%p %p %p %p %p %p) stub\n",u,v,w,x,y,z);
	return 0x80004002;    /* E_NOINTERFACE */
}

/*************************************************************************
 *      @	[SHLWAPI.165]
 *
 * SetWindowLongA with mask.
 */
LONG WINAPI SHLWAPI_165(HWND hwnd, INT offset, UINT wFlags, UINT wMask)
{
  LONG ret = GetWindowLongA(hwnd, offset);
  UINT newFlags = (wFlags & wMask) | (ret & ~wFlags);

  if (newFlags != ret)
    ret = SetWindowLongA(hwnd, offset, newFlags);
  return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.169]
 *
 *  Do IUnknown::Release on passed object.
 */
DWORD WINAPI SHLWAPI_169 (IUnknown ** lpUnknown)
{
        IUnknown *temp;

	TRACE("(%p)\n",lpUnknown);
	if(!lpUnknown || !*((LPDWORD)lpUnknown)) return 0;
	temp = *lpUnknown;
	*lpUnknown = NULL;
	TRACE("doing Release\n");
	return IUnknown_Release(temp);
}

/*************************************************************************
 *      @	[SHLWAPI.170]
 *
 * Skip URL '//' sequence.
 */
LPCSTR WINAPI SHLWAPI_170(LPCSTR lpszSrc)
{
  if (lpszSrc && lpszSrc[0] == '/' && lpszSrc[1] == '/')
    lpszSrc += 2;
  return lpszSrc;
}

/*************************************************************************
 *      @	[SHLWAPI.172]
 * Get window handle of OLE object
 */
DWORD WINAPI SHLWAPI_172 (
	IUnknown *y,       /* [in]   OLE object interface */
	LPHWND z)          /* [out]  location to put window handle */
{
        DWORD ret;
	IUnknown *pv;

        TRACE("(%p %p)\n",y,z);
	if (!y) return E_FAIL;

	if ((ret = IUnknown_QueryInterface(y, &IID_IOleWindow,(LPVOID *)&pv)) < 0) {
	    /* error */
	    return ret;
	}
	ret = IOleWindow_GetWindow((IOleWindow *)pv, z);
	IUnknown_Release(pv);
	TRACE("result hwnd=%08x\n", *z);
	return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.174]
 *
 * Seems to do call either IObjectWithSite::SetSite or
 *   IPersistMoniker::GetClassID.  But since we do not implement either
 *   of those classes in our headers, we will fake it out.
 */
DWORD WINAPI SHLWAPI_174(
	IUnknown *p1,     /* [in]   OLE object                          */
        LPVOID *p2)       /* [out]  ptr to result of either GetClassID
                                    or SetSite call.                    */
{
    DWORD ret, aa;

    if (!p1) return E_FAIL;

    /* see if SetSite interface exists for IObjectWithSite object */
    ret = IUnknown_QueryInterface((IUnknown *)p1, (REFIID)id1, (LPVOID *)&p1);
    TRACE("first IU_QI ret=%08lx, p1=%p\n", ret, p1);
    if (ret) {

	/* see if GetClassId interface exists for IPersistMoniker object */
	ret = IUnknown_QueryInterface((IUnknown *)p1, (REFIID)id2, (LPVOID *)&aa);
	TRACE("second IU_QI ret=%08lx, aa=%08lx\n", ret, aa);
	if (ret) return ret;

	/* fake a GetClassId call */
	ret = IOleWindow_GetWindow((IOleWindow *)aa, (HWND*)p2);
	TRACE("second IU_QI doing 0x0c ret=%08lx, *p2=%08lx\n", ret,
	      *(LPDWORD)p2);
	IUnknown_Release((IUnknown *)aa);
    }
    else {
	/* fake a SetSite call */
	ret = IOleWindow_GetWindow((IOleWindow *)p1, (HWND*)p2);
	TRACE("first IU_QI doing 0x0c ret=%08lx, *p2=%08lx\n", ret,
	      *(LPDWORD)p2);
	IUnknown_Release((IUnknown *)p1);
    }
    return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.175]
 *
 *	NOTE:
 *	  Param1 can be an IShellFolder Object
 */
HRESULT WINAPI SHLWAPI_175 (LPVOID x, LPVOID y)
{
	FIXME("(%p %p) stub\n", x,y);
	return E_FAIL;
}
/*************************************************************************
 *      @	[SHLWAPI.176]
 *
 * Function appears to be interface to IServiceProvider::QueryService
 *
 * NOTE:
 *   returns E_NOINTERFACE
 *           E_FAIL  if w == 0
 *           S_OK    if _219 called successfully
 */
DWORD WINAPI SHLWAPI_176 (
	IUnknown* unk,    /* [in]    object to give Service Provider */
	REFGUID   sid,    /* [in]    Service ID                      */
        REFIID    riid,   /* [in]    Function requested              */
	LPVOID    *z)     /* [out]   place to save interface pointer */
{
    DWORD ret;
    LPVOID aa;
    *z = 0;
    if (!unk) return E_FAIL;
    ret = IUnknown_QueryInterface(unk, &IID_IServiceProvider, &aa);
    TRACE("did IU_QI retval=%08lx, aa=%p\n", ret, aa);
    if (ret) return ret;
    ret = IServiceProvider_QueryService((IServiceProvider *)aa, sid, riid,
					(void **)z);
    TRACE("did ISP_QS retval=%08lx, *z=%p\n", ret, (LPVOID)*z);
    IUnknown_Release((IUnknown*)aa);
    return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.181]
 *
 *	Enable or disable a menu item.
 */
UINT WINAPI SHLWAPI_181(HMENU hMenu, UINT wItemID, BOOL bEnable)
{
  return EnableMenuItem(hMenu, wItemID, bEnable ? MF_ENABLED : MF_GRAYED);
}

/*************************************************************************
 *      @	[SHLWAPI.183]
 *
 * Register a window class if it isn't already.
 */
DWORD WINAPI SHLWAPI_183(WNDCLASSA *wndclass)
{
  WNDCLASSA wca;
  if (GetClassInfoA(wndclass->hInstance, wndclass->lpszClassName, &wca))
    return TRUE;
  return (DWORD)RegisterClassA(wndclass);
}

/*************************************************************************
 *      @	[SHLWAPI.193]
 */
DWORD WINAPI SHLWAPI_193 ()
{
	HDC hdc;
	DWORD ret;

	TRACE("()\n");

	hdc = GetDC(0);
	ret = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
	ReleaseDC(0, hdc);
	return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.199]
 *
 * Copy interface pointer
 */
DWORD WINAPI SHLWAPI_199 (
	IUnknown **dest,   /* [out] pointer to copy of interface ptr */
	IUnknown *src)     /* [in]  interface pointer */
{
        TRACE("(%p %p)\n",dest,src);
	if (*dest != src) {
	    if (*dest)
		IUnknown_Release(*dest);
	    if (src) {
		IUnknown_AddRef(src);
		*dest = src;
	    }
	}
	return 4;
}

/*************************************************************************
 *      @	[SHLWAPI.208]
 *
 * Some sort of memory management process - associated with _210
 */
DWORD WINAPI SHLWAPI_208 (
	DWORD    a,
	DWORD    b,
	LPVOID   c,
	LPVOID   d,
	DWORD    e)
{
    FIXME("(0x%08lx 0x%08lx %p %p 0x%08lx) stub\n",
	  a, b, c, d, e);
    return 1;
}

/*************************************************************************
 *      @	[SHLWAPI.209]
 *
 * Some sort of memory management process - associated with _208
 */
DWORD WINAPI SHLWAPI_209 (
	LPVOID   a)
{
    FIXME("(%p) stub\n",
	  a);
    return 1;
}

/*************************************************************************
 *      @	[SHLWAPI.210]
 *
 * Some sort of memory management process - associated with _208
 */
DWORD WINAPI SHLWAPI_210 (
	LPVOID   a,
	DWORD    b,
	LPVOID   c)
{
    FIXME("(%p 0x%08lx %p) stub\n",
	  a, b, c);
    return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.211]
 */
DWORD WINAPI SHLWAPI_211 (
	LPVOID   a,
	DWORD    b)
{
    FIXME("(%p 0x%08lx) stub\n",
	  a, b);
    return 1;
}

/*************************************************************************
 *      @	[SHLWAPI.215]
 *
 * NOTES
 *  check me!
 */
DWORD WINAPI SHLWAPI_215 (
	LPCSTR lpStrSrc,
	LPWSTR lpwStrDest,
	int len)
{
	INT len_a, ret;

	len_a = lstrlenA(lpStrSrc);
	ret = MultiByteToWideChar(0, 0, lpStrSrc, len_a, lpwStrDest, len);
	TRACE("%s %s %d, ret=%d\n",
	      debugstr_a(lpStrSrc), debugstr_w(lpwStrDest), len, ret);
	return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.218]
 *
 * WideCharToMultiByte with multi language support.
 */
INT WINAPI SHLWAPI_218(UINT CodePage, LPCWSTR lpSrcStr, LPSTR lpDstStr,
                       LPINT lpnMultiCharCount)
{
  static HRESULT (WINAPI *pfnFunc)(LPDWORD,DWORD,LPCWSTR,LPINT,LPSTR,LPINT);
  WCHAR emptyW[] = { '\0' };
  int len , reqLen;
  LPSTR mem;

  if (!lpDstStr || !lpnMultiCharCount)
    return 0;

  if (!lpSrcStr)
    lpSrcStr = emptyW;

  *lpDstStr = '\0';

  len = strlenW(lpSrcStr) + 1;

  switch (CodePage)
  {
  case CP_WINUNICODE:
    CodePage = CP_UTF8; /* Fall through... */
  case 0x0000C350: /* FIXME: CP_ #define */
  case CP_UTF7:
  case CP_UTF8:
    {
      DWORD dwMode = 0;
      INT nWideCharCount = len - 1;

      GET_FUNC(mlang, "ConvertINetUnicodeToMultiByte", 0);
      if (!pfnFunc(&dwMode, CodePage, lpSrcStr, &nWideCharCount, lpDstStr,
                   lpnMultiCharCount))
        return 0;

      if (nWideCharCount < len - 1)
      {
        mem = (LPSTR)HeapAlloc(GetProcessHeap(), 0, *lpnMultiCharCount);
        if (!mem)
          return 0;

        *lpnMultiCharCount = 0;

        if (pfnFunc(&dwMode, CodePage, lpSrcStr, &len, mem, lpnMultiCharCount))
        {
          SHLWAPI_162 (mem, *lpnMultiCharCount);
          lstrcpynA(lpDstStr, mem, *lpnMultiCharCount + 1);
          return *lpnMultiCharCount + 1;
        }
        HeapFree(GetProcessHeap(), 0, mem);
        return *lpnMultiCharCount;
      }
      lpDstStr[*lpnMultiCharCount] = '\0';
      return *lpnMultiCharCount;
    }
    break;
  default:
    break;
  }

  reqLen = WideCharToMultiByte(CodePage, 0, lpSrcStr, len, lpDstStr,
                               *lpnMultiCharCount, NULL, NULL);

  if (!reqLen && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
  {
    reqLen = WideCharToMultiByte(CodePage, 0, lpSrcStr, len, NULL, 0, NULL, NULL);
    if (reqLen)
    {
      mem = (LPSTR)HeapAlloc(GetProcessHeap(), 0, reqLen);
      if (mem)
      {
        reqLen = WideCharToMultiByte(CodePage, 0, lpSrcStr, len, mem,
                                     reqLen, NULL, NULL);

        reqLen = SHLWAPI_162(mem, *lpnMultiCharCount);
        reqLen++;

        lstrcpynA(lpDstStr, mem, *lpnMultiCharCount);

        HeapFree(GetProcessHeap(), 0, mem);
      }
    }
  }
  return reqLen;
}

/*************************************************************************
 *      @	[SHLWAPI.217]
 *
 * Hmm, some program used lpnMultiCharCount == 0x3 (and lpSrcStr was "C")
 * --> Crash. Something wrong here.
 *
 * It seems from OE v5 that the third param is the count. (GA 11/2001)
 */
INT WINAPI SHLWAPI_217(LPCWSTR lpSrcStr, LPSTR lpDstStr, INT MultiCharCount)
{
    INT myint = MultiCharCount;

    return SHLWAPI_218(CP_ACP, lpSrcStr, lpDstStr, &myint);
}

/*************************************************************************
 *      @	[SHLWAPI.219]
 *
 * Seems to be "super" QueryInterface. Supplied with at table of interfaces
 * and an array of IIDs and offsets into the table.
 *
 * NOTES
 *  error codes: E_POINTER, E_NOINTERFACE
 */
typedef struct {
    REFIID   refid;
    DWORD    indx;
} IFACE_INDEX_TBL;

HRESULT WINAPI SHLWAPI_219 (
	LPVOID w,           /* [in]   table of interfaces                   */
	IFACE_INDEX_TBL *x, /* [in]   array of REFIIDs and indexes to above */
	REFIID riid,        /* [in]   REFIID to get interface for           */
	LPVOID *z)          /* [out]  location to get interface pointer     */
{
	HRESULT ret;
	IUnknown *a_vtbl;
	IFACE_INDEX_TBL *xmove;

	TRACE("(%p %p %s %p)\n",
	      w,x,debugstr_guid(riid),z);
	if (z) {
	    xmove = x;
	    while (xmove->refid) {
		TRACE("trying (indx %ld) %s\n", xmove->indx,
		      debugstr_guid(xmove->refid));
		if (IsEqualIID(riid, xmove->refid)) {
		    a_vtbl = (IUnknown*)(xmove->indx + (LPBYTE)w);
		    TRACE("matched, returning (%p)\n", a_vtbl);
		    *z = (LPVOID)a_vtbl;
		    IUnknown_AddRef(a_vtbl);
		    return S_OK;
		}
		xmove++;
	    }

	    if (IsEqualIID(riid, &IID_IUnknown)) {
		a_vtbl = (IUnknown*)(x->indx + (LPBYTE)w);
		TRACE("returning first for IUnknown (%p)\n", a_vtbl);
		*z = (LPVOID)a_vtbl;
		IUnknown_AddRef(a_vtbl);
		return S_OK;
	    }
	    *z = 0;
	    ret = E_NOINTERFACE;
	} else
	    ret = E_POINTER;
	return ret;
}

/*************************************************************************
 *      @	[SHLWAPI.222]
 *
 * NOTES
 *  securityattributes missing
 */
HANDLE WINAPI SHLWAPI_222 (LPCLSID guid)
{
	char lpstrName[80];

        sprintf( lpstrName, "shell.{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 guid->Data1, guid->Data2, guid->Data3,
                 guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                 guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
	FIXME("(%s) stub\n", lpstrName);
	return CreateSemaphoreA(NULL,0, 0x7fffffff, lpstrName);
}

/*************************************************************************
 *      @	[SHLWAPI.223]
 *
 * NOTES
 *  get the count of the semaphore
 */
DWORD WINAPI SHLWAPI_223 (HANDLE handle)
{
	DWORD oldCount;

	FIXME("(0x%08x) stub\n",handle);

	ReleaseSemaphore( handle, 1, &oldCount);	/* +1 */
	WaitForSingleObject( handle, 0 );		/* -1 */
	return oldCount;
}

/*************************************************************************
 *      @	[SHLWAPI.236]
 */
HMODULE WINAPI SHLWAPI_236 (REFIID lpUnknown)
{
    HKEY newkey;
    DWORD type, count;
    CHAR value[MAX_PATH], string[MAX_PATH];

    strcpy(string, "CLSID\\");
    strcat(string, debugstr_guid(lpUnknown));
    strcat(string, "\\InProcServer32");

    count = MAX_PATH;
    RegOpenKeyExA(HKEY_CLASSES_ROOT, string, 0, 1, &newkey);
    RegQueryValueExA(newkey, 0, 0, &type, value, &count);
    RegCloseKey(newkey);
    return LoadLibraryExA(value, 0, 0);
}

/*************************************************************************
 *      @	[SHLWAPI.237]
 *
 * Unicode version of SHLWAPI_183.
 */
DWORD WINAPI SHLWAPI_237 (WNDCLASSW * lpWndClass)
{
	WNDCLASSW WndClass;

	TRACE("(0x%08x %s)\n",lpWndClass->hInstance, debugstr_w(lpWndClass->lpszClassName));

	if (GetClassInfoW(lpWndClass->hInstance, lpWndClass->lpszClassName, &WndClass))
		return TRUE;
	return RegisterClassW(lpWndClass);
}

/*************************************************************************
 *      @	[SHLWAPI.239]
 */
DWORD WINAPI SHLWAPI_239(HINSTANCE hInstance, LPVOID p2, DWORD dw3)
{
    FIXME("(0x%08x %p 0x%08lx) stub\n",
	  hInstance, p2, dw3);
    return 0;
#if 0
    /* pseudo code from relay trace */
    WideCharToMultiByte(0, 0, L"Shell DocObject View", -1, &aa, 0x0207, 0, 0);
    GetClassInfoA(70fe0000,405868ec "Shell DocObject View",40586b14);
    /* above pair repeated for:
           TridentThicketUrlDlClass
	   Shell Embedding
	   CIESplashScreen
	   Inet Notify_Hidden
	   OCHost
    */
#endif
}

/*************************************************************************
 *      @	[SHLWAPI.240]
 *
 *	Calls ASCII or Unicode WindowProc for the given window.
 */
LRESULT CALLBACK SHLWAPI_240(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	if (IsWindowUnicode(hWnd))
		return DefWindowProcW(hWnd, uMessage, wParam, lParam);
	return DefWindowProcA(hWnd, uMessage, wParam, lParam);
}

/*************************************************************************
 *      @	[SHLWAPI.241]
 *
 */
DWORD WINAPI SHLWAPI_241 ()
{
	FIXME("()stub\n");
	return /* 0xabba1243 */ 0;
}

/*************************************************************************
 *      @	[SHLWAPI.266]
 *
 * native does at least approximately:
 *     strcpyW(newstr, x);
 *     strcatW(newstr, "\\Restrictions");
 *     if (RegOpenKeyExA(80000001, newstr, 00000000,00000001,40520b78))
 *        return 0;
 *    *unknown*
 */
DWORD WINAPI SHLWAPI_266 (
	LPVOID w,
	LPVOID x,   /* [in] partial registry key */
	LPVOID y,
	LPVOID z)
{
	FIXME("(%p %p %p %p)stub\n",w,x,y,z);
	return /* 0xabba1248 */ 0;
}

/*************************************************************************
 *      @	[SHLWAPI.267]
 */
HRESULT WINAPI SHLWAPI_267 (
	LPVOID w,
	LPVOID x,
	LPVOID y, /* [???] NOTE: same as 3rd parameter of SHLWAPI_219 */
	LPVOID z) /* [???] NOTE: same as 4th parameter of SHLWAPI_219 */
{
	FIXME("(%p %p %p %p)stub\n",w,x,y,z);

	/* native seems to do:
	 *  SHLWAPI_219 ((LPVOID)(((LPSTR)x)-4), ???, (REFIID) y, (LPVOID*) z);
	 */

	*((LPDWORD)z) = 0xabba1200;
	return /* 0xabba1254 */ 0;
}

/*************************************************************************
 *      @	[SHLWAPI.268]
 */
DWORD WINAPI SHLWAPI_268 (
	LPVOID w,
	LPVOID x)
{
	FIXME("(%p %p)\n",w,x);
	return 0xabba1251; /* 0 = failure */
}

/*************************************************************************
 *      @	[SHLWAPI.276]
 *
 * on first call process does following:  other calls just returns 2
 *  instance = LoadLibraryA("SHELL32.DLL");
 *  func = GetProcAddress(instance, "DllGetVersion");
 *  ret = RegOpenKeyExA(80000002, "Software\\Microsoft\\Internet Explorer",00000000,0002001f, newkey);
 *  ret = RegQueryValueExA(newkey, "IntegratedBrowser",00000000,00000000,4052588c,40525890);
 *  RegCloseKey(newkey);
 *  FreeLibrary(instance);
 *  return 2;
 */
DWORD WINAPI SHLWAPI_276 ()
{
	FIXME("()stub\n");
	return /* 0xabba1244 */ 2;
}

/*************************************************************************
 *      @	[SHLWAPI.278]
 *
 */
HWND WINAPI SHLWAPI_278 (
	LONG wndProc,
	HWND hWndParent,
	DWORD dwExStyle,
	DWORD dwStyle,
	HMENU hMenu,
	LONG z)
{
	WNDCLASSA wndclass;
	HWND hwnd;
	HCURSOR hCursor;
	char * clsname = "WorkerA";

	FIXME("(0x%08lx 0x%08x 0x%08lx 0x%08lx 0x%08x 0x%08lx) partial stub\n",
	  wndProc,hWndParent,dwExStyle,dwStyle,hMenu,z);

	hCursor = LoadCursorA(0x00000000,IDC_ARROWA);

	if(!GetClassInfoA(shlwapi_hInstance, clsname, &wndclass))
	{
	  RtlZeroMemory(&wndclass, sizeof(WNDCLASSA));
	  wndclass.lpfnWndProc = DefWindowProcW;
	  wndclass.cbWndExtra = 4;
	  wndclass.hInstance = shlwapi_hInstance;
	  wndclass.hCursor = hCursor;
	  wndclass.hbrBackground = COLOR_BTNSHADOW;
	  wndclass.lpszMenuName = NULL;
	  wndclass.lpszClassName = clsname;
	  RegisterClassA (&wndclass);
	}
	hwnd = CreateWindowExA(dwExStyle, clsname, 0,dwStyle,0,0,0,0,hWndParent,
		hMenu,shlwapi_hInstance,0);
	SetWindowLongA(hwnd, 0, z);
	SetWindowLongA(hwnd, GWL_WNDPROC, wndProc);
	return hwnd;
}

/*************************************************************************
 *      @	[SHLWAPI.289]
 *
 * Late bound call to winmm.PlaySoundW
 */
BOOL WINAPI SHLWAPI_289(LPCWSTR pszSound, HMODULE hmod, DWORD fdwSound)
{
  static BOOL (WINAPI *pfnFunc)(LPCWSTR, HMODULE, DWORD) = NULL;

  GET_FUNC(winmm, "PlaySoundW", FALSE);
  return pfnFunc(pszSound, hmod, fdwSound);
}

/*************************************************************************
 *      @	[SHLWAPI.294]
 */
BOOL WINAPI SHLWAPI_294(LPSTR str1, LPSTR str2, LPSTR pStr, DWORD some_len,  LPCSTR lpStr2)
{
    /*
     * str1:		"I"	"I"	pushl esp+0x20
     * str2:		"U"	"I"	pushl 0x77c93810
     * (is "I" and "U" "integer" and "unsigned" ??)
     *
     * pStr:		""	""	pushl eax
     * some_len:	0x824	0x104	pushl 0x824
     * lpStr2:		"%l"	"%l"	pushl esp+0xc
     *
     * shlwapi. StrCpyNW(lpStr2, irrelevant_var, 0x104);
     * LocalAlloc(0x00, some_len) -> irrelevant_var
     * LocalAlloc(0x40, irrelevant_len) -> pStr
     * shlwapi.294(str1, str2, pStr, some_len, lpStr2);
     * shlwapi.PathRemoveBlanksW(pStr);
     */
    ERR("('%s', '%s', '%s', %08lx, '%s'): stub!\n", str1, str2, pStr, some_len, lpStr2);
    return TRUE;
}

/*************************************************************************
 *      @	[SHLWAPI.313]
 *
 * Late bound call to shell32.SHGetFileInfoW
 */
DWORD WINAPI SHLWAPI_313(LPCWSTR path, DWORD dwFileAttributes,
                         SHFILEINFOW *psfi, UINT sizeofpsfi, UINT flags)
{
  static DWORD (WINAPI *pfnFunc)(LPCWSTR,DWORD,SHFILEINFOW*,UINT,UINT) = NULL;

  GET_FUNC(shell32, "SHGetFileInfoW", 0);
  return pfnFunc(path, dwFileAttributes, psfi, sizeofpsfi, flags);
}

/*************************************************************************
 *      @	[SHLWAPI.318]
 *
 * Late bound call to shell32.DragQueryFileW
 */
UINT WINAPI SHLWAPI_318(HDROP hDrop, UINT lFile, LPWSTR lpszFile, UINT lLength)
{
  static UINT (WINAPI *pfnFunc)(HDROP, UINT, LPWSTR, UINT) = NULL;

  GET_FUNC(shell32, "DragQueryFileW", 0);
  return pfnFunc(hDrop, lFile, lpszFile, lLength);
}

/*************************************************************************
 *      @	[SHLWAPI.333]
 *
 * Late bound call to shell32.SHBrowseForFolderW
 */
LPITEMIDLIST WINAPI SHLWAPI_333(LPBROWSEINFOW lpBi)
{
  static LPITEMIDLIST (WINAPI *pfnFunc)(LPBROWSEINFOW) = NULL;

  GET_FUNC(shell32, "SHBrowseForFolderW", NULL);
  return pfnFunc(lpBi);
}

/*************************************************************************
 *      @	[SHLWAPI.334]
 *
 * Late bound call to shell32.SHGetPathFromIDListW
 */
BOOL WINAPI SHLWAPI_334(LPCITEMIDLIST pidl,LPWSTR pszPath)
{
  static BOOL (WINAPI *pfnFunc)(LPCITEMIDLIST, LPWSTR) = NULL;

  GET_FUNC(shell32, "SHGetPathFromIDListW", 0);
  return pfnFunc(pidl, pszPath);
}

/*************************************************************************
 *      @	[SHLWAPI.335]
 *
 * Late bound call to shell32.ShellExecuteExW
 */
BOOL WINAPI SHLWAPI_335(LPSHELLEXECUTEINFOW lpExecInfo)
{
  static BOOL (WINAPI *pfnFunc)(LPSHELLEXECUTEINFOW) = NULL;

  GET_FUNC(shell32, "ShellExecuteExW", FALSE);
  return pfnFunc(lpExecInfo);
}

/*************************************************************************
 *      @	[SHLWAPI.336]
 *
 * Late bound call to shell32.SHFileOperationW.
 */
DWORD WINAPI SHLWAPI_336(LPSHFILEOPSTRUCTW lpFileOp)
{
  static HICON (WINAPI *pfnFunc)(LPSHFILEOPSTRUCTW) = NULL;

  GET_FUNC(shell32, "SHFileOperationW", 0);
  return pfnFunc(lpFileOp);
}

/*************************************************************************
 *      @	[SHLWAPI.337]
 *
 * Late bound call to shell32.ExtractIconExW.
 */
HICON WINAPI SHLWAPI_337(LPCWSTR lpszFile, INT nIconIndex, HICON *phiconLarge,
                         HICON *phiconSmall, UINT nIcons)
{
  static HICON (WINAPI *pfnFunc)(LPCWSTR, INT,HICON *,HICON *, UINT) = NULL;

  GET_FUNC(shell32, "ExtractIconExW", (HICON)0);
  return pfnFunc(lpszFile, nIconIndex, phiconLarge, phiconSmall, nIcons);
}

/*************************************************************************
 *      @	[SHLWAPI.342]
 *
 * Wraps InterlockedCompareExchangePointer.
 */
PVOID WINAPI SHLWAPI_342 ( PVOID *destination, PVOID exchange, PVOID comparand)
{
	return InterlockedCompareExchangePointer(destination, exchange, comparand);
}

/*************************************************************************
 *      @	[SHLWAPI.346]
 */
DWORD WINAPI SHLWAPI_346 (
	LPCWSTR src,
	LPWSTR dest,
	int len)
{
	FIXME("(%s %p 0x%08x)stub\n",debugstr_w(src),dest,len);
	lstrcpynW(dest, src, len);
	return lstrlenW(dest)+1;
}

/*************************************************************************
 *      @	[SHLWAPI.350]
 *
 * seems to be late bound call to GetFileVersionInfoSizeW
 */
DWORD WINAPI SHLWAPI_350 (
	LPWSTR x,
	LPVOID y)
{
static DWORD   WINAPI (*pfnFunc)(LPCWSTR,LPDWORD) = NULL;
        DWORD ret;

	GET_FUNC(version, "GetFileVersionInfoSizeW", 0);
	ret = pfnFunc(x, y);
	return 0x208 + ret;
}

/*************************************************************************
 *      @	[SHLWAPI.351]
 *
 * seems to be late bound call to GetFileVersionInfoW
 */
BOOL  WINAPI SHLWAPI_351 (
	LPWSTR w,   /* [in] path to dll */
	DWORD  x,   /* [in] parm 2 to GetFileVersionInfoA */
	DWORD  y,   /* [in] return value from .350 - assume length */
	LPVOID z)   /* [in/out] buffer (+0x208 sent to GetFileVersionInfoA) */
{
        static BOOL    WINAPI (*pfnFunc)(LPCWSTR,DWORD,DWORD,LPVOID) = NULL;

	GET_FUNC(version, "GetFileVersionInfoW", 0);
	return pfnFunc(w, x, y-0x208, z+0x208);
}

/*************************************************************************
 *      @	[SHLWAPI.352]
 *
 * seems to be late bound call to VerQueryValueW
 */
WORD WINAPI SHLWAPI_352 (
	LPVOID w,   /* [in] buffer from _351 */
	LPWSTR x,   /* [in]   value to retrieve -
                              converted and passed to VerQueryValueA as #2 */
	LPVOID y,   /* [out]  ver buffer - passed to VerQueryValueA as #3 */
	UINT*  z)   /* [in]   ver length - passed to VerQueryValueA as #4 */
{
        static WORD    WINAPI (*pfnFunc)(LPVOID,LPCWSTR,LPVOID*,UINT*) = NULL;

	GET_FUNC(version, "VerQueryValueW", 0);
	return pfnFunc(w+0x208, x, y, z);
}

/**************************************************************************
 *      @       [SHLWAPI.356]
 *
 *      mbc - this function is undocumented, The parameters are correct and
 *            the calls to InitializeSecurityDescriptor and
 *            SetSecurityDescriptorDacl are correct, but apparently some
 *            apps call this function with all zero parameters.
 */

DWORD WINAPI SHLWAPI_356(PACL pDacl, PSECURITY_DESCRIPTOR pSD, LPCSTR *str)
{
  if(str != 0){
    *str = 0;
  }

  if(!pDacl){
    return 0;
  }

  if (!InitializeSecurityDescriptor(pSD, 1)) return 0;
  return SetSecurityDescriptorDacl(pSD, 1, pDacl, 0);
}


/*************************************************************************
 *      @	[SHLWAPI.357]
 *
 * Late bound call to shell32.SHGetNewLinkInfoW
 */
BOOL WINAPI SHLWAPI_357(LPCWSTR pszLinkTo, LPCWSTR pszDir, LPWSTR pszName,
                        BOOL *pfMustCopy, UINT uFlags)
{
  static BOOL (WINAPI *pfnFunc)(LPCWSTR, LPCWSTR, LPCWSTR, BOOL*, UINT) = NULL;

  GET_FUNC(shell32, "SHGetNewLinkInfoW", FALSE);
  return pfnFunc(pszLinkTo, pszDir, pszName, pfMustCopy, uFlags);
}

/*************************************************************************
 *      @	[SHLWAPI.358]
 *
 * Late bound call to shell32.SHDefExtractIconW
 */
DWORD WINAPI SHLWAPI_358(LPVOID arg1, LPVOID arg2, LPVOID arg3, LPVOID arg4,
                         LPVOID arg5, LPVOID arg6)
{
  /* FIXME: Correct args */
  static DWORD (WINAPI *pfnFunc)(LPVOID, LPVOID, LPVOID, LPVOID, LPVOID, LPVOID) = NULL;

  GET_FUNC(shell32, "SHDefExtractIconW", 0);
  return pfnFunc(arg1, arg2, arg3, arg4, arg5, arg6);
}

/*************************************************************************
 *      @	[SHLWAPI.364]
 *
 * Wrapper for lstrcpynA with src and dst swapped.
 */
DWORD WINAPI SHLWAPI_364(LPCSTR src, LPSTR dst, INT n)
{
  lstrcpynA(dst, src, n);
  return TRUE;
}

/*************************************************************************
 *      @	[SHLWAPI.370]
 *
 * Late bound call to shell32.ExtractIconW
 */
HICON WINAPI SHLWAPI_370(HINSTANCE hInstance, LPCWSTR lpszExeFileName,
                         UINT nIconIndex)
{
  static HICON (WINAPI *pfnFunc)(HINSTANCE, LPCWSTR, UINT) = NULL;

  GET_FUNC(shell32, "ExtractIconW", (HICON)0);
  return pfnFunc(hInstance, lpszExeFileName, nIconIndex);
}

/*************************************************************************
 *      @	[SHLWAPI.376]
 */
LANGID WINAPI SHLWAPI_376 ()
{
    FIXME("() stub\n");
    /* FIXME: This should be a forward in the .spec file to the win2k function
     * kernel32.GetUserDefaultUILanguage, however that function isn't there yet.
     */
    return GetUserDefaultLangID();
}

/*************************************************************************
 *      @	[SHLWAPI.377]
 *
 * FIXME: Native appears to do DPA_Create and a DPA_InsertPtr for
 *        each call here.
 * FIXME: Native shows calls to:
 *  SHRegGetUSValue for "Software\Microsoft\Internet Explorer\International"
 *                      CheckVersion
 *  RegOpenKeyExA for "HKLM\Software\Microsoft\Internet Explorer"
 *  RegQueryValueExA for "LPKInstalled"
 *  RegCloseKey
 *  RegOpenKeyExA for "HKCU\Software\Microsoft\Internet Explorer\International"
 *  RegQueryValueExA for "ResourceLocale"
 *  RegCloseKey
 *  RegOpenKeyExA for "HKLM\Software\Microsoft\Active Setup\Installed Components\{guid}"
 *  RegQueryValueExA for "Locale"
 *  RegCloseKey
 *  and then tests the Locale ("en" for me).
 *     code below
 *  after the code then a DPA_Create (first time) and DPA_InsertPtr are done.
 */
DWORD WINAPI SHLWAPI_377 (LPCSTR new_mod, HMODULE inst_hwnd, LPVOID z)
{
    CHAR    mod_path[2*MAX_PATH];
    LPSTR   ptr;
    DWORD   error;


    error = GetModuleFileNameA(inst_hwnd, mod_path, 2*MAX_PATH);

    if (error == 2 * MAX_PATH)
        ERR("the buffer for GetModuleFileName() is too small!\n");

    else if (error == 0){
        ERR("could not retrieve the module filename for 0x%lx\n", inst_hwnd);

        return 0;
    }


    ptr = strrchr(mod_path, '\\');
    if (ptr) {
        strcpy(ptr+1, new_mod);
        TRACE("loading %s\n", debugstr_a(mod_path));
        return (DWORD)LoadLibraryA(mod_path);
    }
    return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.378]
 *
 *  This is Unicode version of .377
 */
DWORD WINAPI SHLWAPI_378 (
	LPCWSTR   new_mod,          /* [in] new module name        */
	HMODULE   inst_hwnd,        /* [in] calling module handle  */
	LPVOID z)                   /* [???] 4 */
{
    WCHAR   mod_path[2*MAX_PATH];
    LPWSTR  ptr;
    DWORD   error;

    error = GetModuleFileNameW(inst_hwnd, mod_path, 2*MAX_PATH);

    if (error == 2 * MAX_PATH)
        ERR("the buffer for GetModuleFileName() is too small!\n");

    else if (error == 0){
        ERR("could not retrieve the filename of module 0x%lx\n", inst_hwnd);

        return 0;
    }


    ptr = strrchrW(mod_path, '\\');
    if (ptr) {
        strcpyW(ptr+1, new_mod);
        TRACE("loading %s\n", debugstr_w(mod_path));
        return (DWORD)LoadLibraryW(mod_path);
    }
    return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.389]
 *
 * Late bound call to comdlg32.GetSaveFileNameW
 */
BOOL WINAPI SHLWAPI_389(LPOPENFILENAMEW ofn)
{
  static BOOL (WINAPI *pfnFunc)(LPOPENFILENAMEW) = NULL;

  GET_FUNC(comdlg32, "GetSaveFileNameW", FALSE);
  return pfnFunc(ofn);
}

/*************************************************************************
 *      @	[SHLWAPI.390]
 *
 * Late bound call to mpr.WNetRestoreConnectionW
 */
DWORD WINAPI SHLWAPI_390(LPVOID arg1, LPVOID arg2)
{
  /* FIXME: Correct args */
  static DWORD (WINAPI *pfnFunc)(LPVOID, LPVOID) = NULL;

  GET_FUNC(mpr, "WNetRestoreConnectionW", 0);
  return pfnFunc(arg1, arg2);
}

/*************************************************************************
 *      @	[SHLWAPI.391]
 *
 * Late bound call to mpr.WNetGetLastErrorW
 */
DWORD WINAPI SHLWAPI_391(LPVOID arg1, LPVOID arg2, LPVOID arg3, LPVOID arg4,
                         LPVOID arg5)
{
  /* FIXME: Correct args */
  static DWORD (WINAPI *pfnFunc)(LPVOID, LPVOID, LPVOID, LPVOID, LPVOID) = NULL;

  GET_FUNC(mpr, "WNetGetLastErrorW", 0);
  return pfnFunc(arg1, arg2, arg3, arg4, arg5);
}

/*************************************************************************
 *      @	[SHLWAPI.401]
 *
 * Late bound call to comdlg32.PageSetupDlgW
 */
BOOL WINAPI SHLWAPI_401(LPPAGESETUPDLGW pagedlg)
{
  static BOOL (WINAPI *pfnFunc)(LPPAGESETUPDLGW) = NULL;

  GET_FUNC(comdlg32, "PageSetupDlgW", FALSE);
  return pfnFunc(pagedlg);
}

/*************************************************************************
 *      @	[SHLWAPI.402]
 *
 * Late bound call to comdlg32.PrintDlgW
 */
BOOL WINAPI SHLWAPI_402(LPPRINTDLGW printdlg)
{
  static BOOL (WINAPI *pfnFunc)(LPPRINTDLGW) = NULL;

  GET_FUNC(comdlg32, "PrintDlgW", FALSE);
  return pfnFunc(printdlg);
}

/*************************************************************************
 *      @	[SHLWAPI.403]
 *
 * Late bound call to comdlg32.GetOpenFileNameW
 */
BOOL WINAPI SHLWAPI_403(LPOPENFILENAMEW ofn)
{
  static BOOL (WINAPI *pfnFunc)(LPOPENFILENAMEW) = NULL;

  GET_FUNC(comdlg32, "GetOpenFileNameW", FALSE);
  return pfnFunc(ofn);
}

/* INTERNAL: Map from HLS color space to RGB */
static WORD ConvertHue(int wHue, WORD wMid1, WORD wMid2)
{
  wHue = wHue > 240 ? wHue - 240 : wHue < 0 ? wHue + 240 : wHue;

  if (wHue > 160)
    return wMid1;
  else if (wHue > 120)
    wHue = 160 - wHue;
  else if (wHue > 40)
    return wMid2;

  return ((wHue * (wMid2 - wMid1) + 20) / 40) + wMid1;
}

/* Convert to RGB and scale into RGB range (0..255) */
#define GET_RGB(h) (ConvertHue(h, wMid1, wMid2) * 255 + 120) / 240

/*************************************************************************
 *      ColorHLSToRGB	[SHLWAPI.404]
 *
 * Convert from HLS color space into an RGB COLORREF.
 *
 * NOTES
 * Input HLS values are constrained to the range (0..240).
 */
COLORREF WINAPI ColorHLSToRGB(WORD wHue, WORD wLuminosity, WORD wSaturation)
{
  WORD wRed;

  if (wSaturation)
  {
    WORD wGreen, wBlue, wMid1, wMid2;

    if (wLuminosity > 120)
      wMid2 = wSaturation + wLuminosity - (wSaturation * wLuminosity + 120) / 240;
    else
      wMid2 = ((wSaturation + 240) * wLuminosity + 120) / 240;

    wMid1 = wLuminosity * 2 - wMid2;

    wRed   = GET_RGB(wHue + 80);
    wGreen = GET_RGB(wHue);
    wBlue  = GET_RGB(wHue - 80);

    return RGB(wRed, wGreen, wBlue);
  }

  wRed = wLuminosity * 255 / 240;
  return RGB(wRed, wRed, wRed);
}

/*************************************************************************
 *      @	[SHLWAPI.413]
 *
 * Function unknown seems to always to return 0
 */
DWORD WINAPI SHLWAPI_413 (DWORD x)
{
	FIXME("(0x%08lx)stub\n", x);
	return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.418]
 *
 * Function seems to do FreeLibrary plus other things.
 *
 * FIXME native shows the following calls:
 *   RtlEnterCriticalSection
 *   LocalFree
 *   GetProcAddress(Comctl32??, 150L)
 *   DPA_DeletePtr
 *   RtlLeaveCriticalSection
 *  followed by the FreeLibrary.
 *  The above code may be related to .377 above.
 */
BOOL  WINAPI SHLWAPI_418 (HMODULE x)
{
	FIXME("(0x%08lx) partial stub\n", (LONG)x);
	return FreeLibrary(x);
}

/*************************************************************************
 *      @	[SHLWAPI.431]
 */
DWORD WINAPI SHLWAPI_431 (DWORD x)
{
	FIXME("(0x%08lx)stub\n", x);
	return 0xabba1247;
}

/*************************************************************************
 *      @	[SHLWAPI.436]
 *
 *  This is really CLSIDFromString which is exported by ole32.dll,
 *  however the native shlwapi.dll does *not* import ole32. Nor does
 *  ole32.dll import this ordinal from shlwapi. Therefore we must conclude
 *  that MS duplicated the code for CLSIDFromString.
 *
 *  This is a duplicate (with changes for UNICODE) of CLSIDFromString16
 *  in dlls/ole32/compobj.c
 */
DWORD WINAPI SHLWAPI_436 (LPWSTR idstr, CLSID *id)
{
    LPWSTR s = idstr;
    BYTE *p;
    INT i;
    WCHAR table[256];

    if (!s) {
	memset(s, 0, sizeof(CLSID));
	return S_OK;
    }
    else {  /* validate the CLSID string */

	if (strlenW(s) != 38)
	    return CO_E_CLASSSTRING;

	if ((s[0]!=L'{') || (s[9]!=L'-') || (s[14]!=L'-') || (s[19]!=L'-') || (s[24]!=L'-') || (s[37]!=L'}'))
	    return CO_E_CLASSSTRING;

	for (i=1; i<37; i++)
	    {
		if ((i == 9)||(i == 14)||(i == 19)||(i == 24)) continue;
		if (!(((s[i] >= L'0') && (s[i] <= L'9'))  ||
		      ((s[i] >= L'a') && (s[i] <= L'f'))  ||
		      ((s[i] >= L'A') && (s[i] <= L'F')))
		    )
		    return CO_E_CLASSSTRING;
	    }
    }

    TRACE("%s -> %p\n", debugstr_w(s), id);

  /* quick lookup table */
    memset(table, 0, 256*sizeof(WCHAR));

    for (i = 0; i < 10; i++) {
	table['0' + i] = i;
    }
    for (i = 0; i < 6; i++) {
	table['A' + i] = i+10;
	table['a' + i] = i+10;
    }

    /* in form {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} */

    p = (BYTE *) id;

    s++;	/* skip leading brace  */
    for (i = 0; i < 4; i++) {
	p[3 - i] = table[*s]<<4 | table[*(s+1)];
	s += 2;
    }
    p += 4;
    s++;	/* skip - */

    for (i = 0; i < 2; i++) {
	p[1-i] = table[*s]<<4 | table[*(s+1)];
	s += 2;
    }
    p += 2;
    s++;	/* skip - */

    for (i = 0; i < 2; i++) {
	p[1-i] = table[*s]<<4 | table[*(s+1)];
	s += 2;
    }
    p += 2;
    s++;	/* skip - */

    /* these are just sequential bytes */
    for (i = 0; i < 2; i++) {
	*p++ = table[*s]<<4 | table[*(s+1)];
	s += 2;
    }
    s++;	/* skip - */

    for (i = 0; i < 6; i++) {
	*p++ = table[*s]<<4 | table[*(s+1)];
	s += 2;
    }

    return S_OK;
}

/*************************************************************************
 *      @	[SHLWAPI.437]
 *
 * NOTES
 *  In the real shlwapi, One time initialisation calls GetVersionEx and reads
 *  the registry to determine what O/S & Service Pack level is running, and
 *  therefore which functions are available. Currently we always run as NT,
 *  since this means that we don't need extra code to emulate Unicode calls,
 *  they are forwarded directly to the appropriate API call instead.
 *  Since the flags for whether to call or emulate Unicode are internal to
 *  the dll, this function does not need a full implementation.
 */
DWORD WINAPI SHLWAPI_437 (DWORD functionToCall)
{
	FIXME("(0x%08lx)stub\n", functionToCall);
	return /* 0xabba1247 */ 0;
}

/*************************************************************************
 *      ColorRGBToHLS	[SHLWAPI.445]
 *
 * Convert from RGB COLORREF into the HLS color space.
 *
 * NOTES
 * Input HLS values are constrained to the range (0..240).
 */
VOID WINAPI ColorRGBToHLS(COLORREF drRGB, LPWORD pwHue,
			  LPWORD wLuminance, LPWORD pwSaturation)
{
    FIXME("stub\n");
    return;
}

/*************************************************************************
 *      SHCreateShellPalette	[SHLWAPI.@]
 */
HPALETTE WINAPI SHCreateShellPalette(HDC hdc)
{
	FIXME("stub\n");
	return CreateHalftonePalette(hdc);
}

/*************************************************************************
 *	SHGetInverseCMAP (SHLWAPI.@)
 */
DWORD WINAPI SHGetInverseCMAP (LPDWORD* x, DWORD why)
{
    if (why == 4) {
	FIXME(" - returning bogus address for SHGetInverseCMAP\n");
	*x = (LPDWORD)0xabba1249;
	return 0;
    }
    FIXME("(%p, %#lx)stub\n", x, why);
    return 0;
}

/*************************************************************************
 *      SHIsLowMemoryMachine	[SHLWAPI.@]
 */
DWORD WINAPI SHIsLowMemoryMachine (DWORD x)
{
	FIXME("0x%08lx\n", x);
	return 0;
}

/*************************************************************************
 *      GetMenuPosFromID	[SHLWAPI.@]
 */
INT WINAPI GetMenuPosFromID(HMENU hMenu, UINT wID)
{
 MENUITEMINFOA mi;
 INT nCount = GetMenuItemCount(hMenu), nIter = 0;

 while (nIter < nCount)
 {
   mi.wID = 0;
   if (!GetMenuItemInfoA(hMenu, nIter, TRUE, &mi) && mi.wID == wID)
     return nIter;
   nIter++;
 }
 return -1;
}

/*************************************************************************
 *      _SHGetInstanceExplorer@4	[SHLWAPI.@]
 *
 * Late bound call to shell32.SHGetInstanceExplorer.
 */
HRESULT WINAPI _SHGetInstanceExplorer (LPUNKNOWN *lpUnknown)
{
  static HRESULT (WINAPI *pfnFunc)(LPUNKNOWN *) = NULL;

  GET_FUNC(shell32, "SHGetInstanceExplorer", E_FAIL);
  return pfnFunc(lpUnknown);
}

/*************************************************************************
 *      SHGetThreadRef	[SHLWAPI.@]
 *
 * Retrieves the per-thread object reference set by SHSetThreadRef
 * "punk" - Address of a pointer to the IUnknown interface. Returns S_OK if
 *          successful or E_NOINTERFACE otherwise.
 */
HRESULT WINAPI SHGetThreadRef (IUnknown ** ppunk)
{
    if (SHLWAPI_ThreadRef_index < 0) return E_NOINTERFACE;
    *ppunk = (IUnknown *)TlsGetValue(SHLWAPI_ThreadRef_index);
    return S_OK;
}

/*************************************************************************
 *      SHSetThreadRef	[SHLWAPI.@]
 *
 * Stores a per-thread reference to a COM object
 * "punk" - Pointer to the IUnknown interface of the object to
 *          which you want to store a reference. Returns S_OK if successful
 *          or an OLE error value.
 */
HRESULT WINAPI SHSetThreadRef (IUnknown * punk)
{
    if (SHLWAPI_ThreadRef_index < 0) return E_NOINTERFACE;
    TlsSetValue(SHLWAPI_ThreadRef_index, (LPVOID) punk);
    return S_OK;
}
