/*
 * Unicode definitions
 *
 * Copyright 2000 Francois Gouget.
 */
#ifndef __WINE_WCTYPE_H
#define __WINE_WCTYPE_H

#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif


/* FIXME: winnt.h includes 'ctype.h' which includes 'wctype.h'. So we get
 * there but WCHAR is not defined.
 */
/* Some systems might have wchar_t, but we really need 16 bit characters */
#ifndef WINE_WCHAR_DEFINED
#ifdef WINE_UNICODE_NATIVE
typedef wchar_t         WCHAR,      *PWCHAR;
#else
typedef unsigned short  WCHAR,      *PWCHAR;
#endif
#define WINE_WCHAR_DEFINED
#endif

#ifdef USE_MSVCRT_PREFIX
#define MSVCRT(x)    MSVCRT_##x
#else
#define MSVCRT(x)    x
#endif


/* ASCII char classification table - binary compatible */
#define _UPPER        C1_UPPER
#define _LOWER        C1_LOWER
#define _DIGIT        C1_DIGIT
#define _SPACE        C1_SPACE
#define _PUNCT        C1_PUNCT
#define _CONTROL      C1_CNTRL
#define _BLANK        C1_BLANK
#define _HEX          C1_XDIGIT
#define _LEADBYTE     0x8000
#define _ALPHA       (C1_ALPHA|_UPPER|_LOWER)

#ifndef USE_MSVCRT_PREFIX
# ifndef WEOF
#  define WEOF        (WCHAR)(0xFFFF)
# endif
#else
# ifndef MSVCRT_WEOF
#  define MSVCRT_WEOF (WCHAR)(0xFFFF)
# endif
#endif /* USE_MSVCRT_PREFIX */

typedef WCHAR MSVCRT(wctype_t);
typedef WCHAR MSVCRT(wint_t);

/* FIXME: there's something to do with __p__pctype and __p__pwctype */


#ifdef __cplusplus
extern "C" {
#endif

int MSVCRT(is_wctype)(MSVCRT(wint_t),MSVCRT(wctype_t));
int MSVCRT(isleadbyte)(int);
int MSVCRT(iswalnum)(MSVCRT(wint_t));
int MSVCRT(iswalpha)(MSVCRT(wint_t));
int MSVCRT(iswascii)(MSVCRT(wint_t));
int MSVCRT(iswcntrl)(MSVCRT(wint_t));
int MSVCRT(iswctype)(MSVCRT(wint_t),MSVCRT(wctype_t));
int MSVCRT(iswdigit)(MSVCRT(wint_t));
int MSVCRT(iswgraph)(MSVCRT(wint_t));
int MSVCRT(iswlower)(MSVCRT(wint_t));
int MSVCRT(iswprint)(MSVCRT(wint_t));
int MSVCRT(iswpunct)(MSVCRT(wint_t));
int MSVCRT(iswspace)(MSVCRT(wint_t));
int MSVCRT(iswupper)(MSVCRT(wint_t));
int MSVCRT(iswxdigit)(MSVCRT(wint_t));
WCHAR MSVCRT(towlower)(WCHAR);
WCHAR MSVCRT(towupper)(WCHAR);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_WCTYPE_H */
