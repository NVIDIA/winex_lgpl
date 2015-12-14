/*
 * Unicode definitions
 *
 * Derived from the mingw header written by Colin Peters.
 * Modified for Wine use by Jon Griffiths and Francois Gouget.
 * This file is in the public domain.
 */
#ifndef __WINE_WCHAR_H
#define __WINE_WCHAR_H

#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif

#include "msvcrt/io.h"
#include "msvcrt/locale.h"
#include "msvcrt/process.h"
#include "msvcrt/stdio.h"
#include "msvcrt/stdlib.h"
#include "msvcrt/string.h"
#include "msvcrt/sys/stat.h"
#include "msvcrt/sys/types.h"
#include "msvcrt/time.h"
#include "msvcrt/wctype.h"


#define WCHAR_MIN 0
#define WCHAR_MAX ((WCHAR)-1)

typedef int MSVCRT(mbstate_t);

#ifndef MSVCRT_SIZE_T_DEFINED
typedef unsigned int MSVCRT(size_t);
#define MSVCRT_SIZE_T_DEFINED
#endif


#ifdef __cplusplus
extern "C" {
#endif

WCHAR       btowc(int);
MSVCRT(size_t) mbrlen(const char *,MSVCRT(size_t),MSVCRT(mbstate_t)*);
MSVCRT(size_t) mbrtowc(WCHAR*,const char*,MSVCRT(size_t),MSVCRT(mbstate_t)*);
MSVCRT(size_t) mbsrtowcs(WCHAR*,const char**,MSVCRT(size_t),MSVCRT(mbstate_t)*);
MSVCRT(size_t) wcrtomb(char*,WCHAR,MSVCRT(mbstate_t)*);
MSVCRT(size_t) wcsrtombs(char*,const WCHAR**,MSVCRT(size_t),MSVCRT(mbstate_t)*);
int         wctob(MSVCRT(wint_t));

#ifdef __cplusplus

/* These have C style linkage */
static inline int wmemcmp(const WCHAR* _S1, const WCHAR* _S2, size_t _N)
{ 
  for (; 0 < _N; ++_S1, ++_S2, --_N)
    if (*_S1 != *_S2)
      return (*_S1 < *_S2 ? -1 : +1);
  return 0;
}

static inline WCHAR* wmemcpy(WCHAR* _S1, const WCHAR* _S2, size_t _N)
{
  WCHAR* _Su1 = _S1;
  for (; 0 < _N; ++_Su1, ++_S2, --_N)
    *_Su1 = *_S2;
  return _S1;
}

static inline const WCHAR* wmemchr(const WCHAR* _S, WCHAR _C, size_t _N)
{
  for (; 0 < _N; ++_S, --_N)
    if (*_S == _C)
      return _S;
  return 0;
}

static inline WCHAR* wmemmove(WCHAR* _S1, const WCHAR* _S2, size_t _N)
{
  WCHAR* _Su1 = _S1;
  if (_S2 < _Su1 && _Su1 < _S2 + _N)
    for (_Su1 += _N, _S2 += _N; 0 < _N; --_N)
      *--_Su1 = *--_S2;
  else
    for (; 0 < _N; --_N)
      *_Su1++ = *_S2++;

  return _S1;
}

static inline WCHAR* wmemset(WCHAR* _S, WCHAR _C, size_t _N)
{
  WCHAR* _Su = _S;
  for (; 0 < _N; ++_Su, --_N)
    *_Su = _C;
  return _S;
}

#ifdef __cplusplus
}
#endif

/* These have C++ linkage */

static inline WCHAR* wmemchr(WCHAR* _S, WCHAR _C, size_t _N)
{
  return ((WCHAR*)wmemchr((const WCHAR*)_S, _C, _N));
}

#endif /* __cplusplus */


#endif /* __WINE_WCHAR_H */
