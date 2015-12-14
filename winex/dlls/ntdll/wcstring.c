/*
 * NTDLL wide-char functions
 *
 * Copyright 2000 Alexandre Julliard
 * Copyright 2000 Jon Griffiths
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "winternl.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);


/*********************************************************************
 *           _wcsicmp    (NTDLL.@)
 */
INT __cdecl NTDLL__wcsicmp( LPCWSTR str1, LPCWSTR str2 )
{
    return strcmpiW( str1, str2 );
}


/*********************************************************************
 *           _wcslwr    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL__wcslwr( LPWSTR str )
{
    return strlwrW( str );
}


/*********************************************************************
 *           _wcsnicmp    (NTDLL.@)
 */
INT __cdecl NTDLL__wcsnicmp( LPCWSTR str1, LPCWSTR str2, INT n )
{
    return strncmpiW( str1, str2, n );
}


/*********************************************************************
 *           _wcsupr    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL__wcsupr( LPWSTR str )
{
    return struprW( str );
}


/*********************************************************************
 *           towlower    (NTDLL.@)
 */
WCHAR __cdecl NTDLL_towlower( WCHAR ch )
{
    return tolowerW(ch);
}


/*********************************************************************
 *           towupper    (NTDLL.@)
 */
WCHAR __cdecl NTDLL_towupper( WCHAR ch )
{
    return toupperW(ch);
}


/***********************************************************************
 *           wcscat    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcscat( LPWSTR dst, LPCWSTR src )
{
    return strcatW( dst, src );
}


/*********************************************************************
 *           wcschr    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcschr( LPCWSTR str, WCHAR ch )
{
    return strchrW( str, ch );
}


/*********************************************************************
 *           wcscmp    (NTDLL.@)
 */
INT __cdecl NTDLL_wcscmp( LPCWSTR str1, LPCWSTR str2 )
{
    return strcmpW( str1, str2 );
}


/***********************************************************************
 *           wcscpy    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcscpy( LPWSTR dst, LPCWSTR src )
{
    return strcpyW( dst, src );
}


/*********************************************************************
 *           wcscspn    (NTDLL.@)
 */
INT __cdecl NTDLL_wcscspn( LPCWSTR str, LPCWSTR reject )
{
   return strcspnW (str, reject);
}


/***********************************************************************
 *           wcslen    (NTDLL.@)
 */
INT __cdecl NTDLL_wcslen( LPCWSTR str )
{
    return strlenW( str );
}


/*********************************************************************
 *           wcsncat    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcsncat( LPWSTR s1, LPCWSTR s2, INT n )
{
    LPWSTR ret = s1;
    while (*s1) s1++;
    while (n-- > 0) if (!(*s1++ = *s2++)) return ret;
    *s1 = 0;
    return ret;
}


/*********************************************************************
 *           wcsncmp    (NTDLL.@)
 */
INT __cdecl NTDLL_wcsncmp( LPCWSTR str1, LPCWSTR str2, INT n )
{
    return strncmpW( str1, str2, n );
}


/*********************************************************************
 *           wcsncpy    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcsncpy( LPWSTR s1, LPCWSTR s2, INT n )
{
    WCHAR *ret = s1;
    while (n-- > 0) if (!(*s1++ = *s2++)) break;
    while (n-- > 0) *s1++ = 0;
    return ret;
}


/*********************************************************************
 *           wcspbrk    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcspbrk( LPCWSTR str, LPCWSTR accept )
{
   return strpbrkW (str, accept);
}


/*********************************************************************
 *           wcsrchr    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcsrchr( LPWSTR str, WCHAR ch )
{
   return strrchrW (str, ch);
}


/*********************************************************************
 *           wcsspn    (NTDLL.@)
 */
INT __cdecl NTDLL_wcsspn( LPCWSTR str, LPCWSTR accept )
{
   return strspnW (str, accept);
}


/*********************************************************************
 *           wcsstr    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcsstr( LPCWSTR str, LPCWSTR sub )
{
    return strstrW( str, sub );
}


/*********************************************************************
 *           wcstok    (NTDLL.@)
 */
LPWSTR __cdecl NTDLL_wcstok( LPWSTR str, LPCWSTR delim )
{
    static LPWSTR next = NULL;
    LPWSTR ret;

    if (!str)
        if (!(str = next)) return NULL;

    while (*str && NTDLL_wcschr( delim, *str )) str++;
    if (!*str) return NULL;
    ret = str++;
    while (*str && !NTDLL_wcschr( delim, *str )) str++;
    if (*str) *str++ = 0;
    next = str;
    return ret;
}


/*********************************************************************
 *           wcstombs    (NTDLL.@)
 */
INT __cdecl NTDLL_wcstombs( LPSTR dst, LPCWSTR src, INT n )
{
    DWORD len;

    if (!dst)
    {
        RtlUnicodeToMultiByteSize( &len, src, strlenW(src)*sizeof(WCHAR) );
        return len;
    }
    else
    {
        if (n <= 0) return 0;
        RtlUnicodeToMultiByteN( dst, n, &len, src, strlenW(src)*sizeof(WCHAR) );
        if (len < n) dst[len] = 0;
    }
    return len;
}


/*********************************************************************
 *           mbstowcs    (NTDLL.@)
 */
INT __cdecl NTDLL_mbstowcs( LPWSTR dst, LPCSTR src, INT n )
{
    DWORD len;

    if (!dst)
    {
        RtlMultiByteToUnicodeSize( &len, src, strlen(src) );
    }
    else
    {
        if (n <= 0) return 0;
        RtlMultiByteToUnicodeN( dst, n*sizeof(WCHAR), &len, src, strlen(src) );
        if (len / sizeof(WCHAR) < n) dst[len / sizeof(WCHAR)] = 0;
    }
    return len / sizeof(WCHAR);
}


/*********************************************************************
 *                  wcstol  (NTDLL.@)
 * Like strtol, but for wide character strings.
 */
INT __cdecl NTDLL_wcstol(LPCWSTR s,LPWSTR *end,INT base)
{
   return strtolW (s, end, base);
}


/*********************************************************************
 *                  wcstoul  (NTDLL.@)
 * Like strtoul, but for wide character strings.
 */
INT __cdecl NTDLL_wcstoul(LPCWSTR s,LPWSTR *end,INT base)
{
   return strtoulW (s, end, base);
}


/*********************************************************************
 *           iswctype    (NTDLL.@)
 */
INT __cdecl NTDLL_iswctype( WCHAR wc, WCHAR wct )
{
    return (get_char_typeW(wc) & 0xfff) & wct;
}


/*********************************************************************
 *           iswalpha    (NTDLL.@)
 */
INT __cdecl NTDLL_iswalpha( WCHAR wc )
{
   return isalphaW (wc);
}


/*********************************************************************
 *           iswdigit    (NTDLL.@)
 */
INT __cdecl NTDLL_iswdigit( WCHAR wc )
{
   return isdigitW (wc);
}


/*********************************************************************
 *           iswlower    (NTDLL.@)
 */
INT __cdecl NTDLL_iswlower( WCHAR wc )
{
   return islowerW (wc);
}


/*********************************************************************
 *           iswspace    (NTDLL.@)
 */
INT __cdecl NTDLL_iswspace( WCHAR wc )
{
   return isspaceW (wc);
}


/*********************************************************************
 *           iswxdigit    (NTDLL.@)
 */
INT __cdecl NTDLL_iswxdigit( WCHAR wc )
{
   return isxdigitW (wc);
}


/*********************************************************************
 *           _ultow    (NTDLL.@)
 * Like _ultoa, but for wide character strings.
 */
LPWSTR __cdecl _ultow(ULONG value, LPWSTR string, INT radix)
{
    WCHAR tmp[33];
    LPWSTR tp = tmp;
    LPWSTR sp;
    LONG i;
    ULONG v = value;

    if (radix > 36 || radix <= 1)
	return 0;

    while (v || tp == tmp)
    {
	i = v % radix;
	v = v / radix;
	if (i < 10)
	    *tp++ = i + '0';
	else
	    *tp++ = i + 'a' - 10;
    }

    sp = string;
    while (tp > tmp)
	*sp++ = *--tp;
    *sp = 0;
    return string;
}

/*********************************************************************
 *           _wtol    (NTDLL.@)
 * Like atol, but for wide character strings.
 */
LONG __cdecl _wtol(LPWSTR string)
{
    char buffer[30];
    NTDLL_wcstombs( buffer, string, sizeof(buffer) );
    return atol( buffer );
}

/*********************************************************************
 *           _wtoi    (NTDLL.@)
 */
INT __cdecl _wtoi(LPWSTR string)
{
    return _wtol(string);
}

/*********************************************************************
 *           _vsnwprintf    (NTDLL.@)
 *
 * Wide char snprintf
 */
INT __cdecl NTDLL_vsnwprintf(WCHAR *str, unsigned int len,
                                    const WCHAR *format, va_list valist)
{
  return vsnprintfW(str, len, format, valist);
}

/***********************************************************************
 *        _snwprintf (NTDLL.@)
 */
int __cdecl _snwprintf(WCHAR *str, unsigned int len, const WCHAR *format, ...)
{
  int retval;
  va_list valist;
  va_start(valist, format);
  retval = vsnprintfW(str, len, format, valist);
  va_end(valist);
  return retval;
}


/***********************************************************************
 *        swprintf (NTDLL.@)
 */
int __cdecl NTDLL_swprintf(WCHAR *str, const WCHAR *format, ...)
{
  int retval;
  va_list valist;
  va_start(valist, format);
  retval = vsnprintfW(str, INT_MAX, format, valist);
  va_end(valist);
  return retval;
}
