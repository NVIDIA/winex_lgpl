/*
 * Basic types definitions
 *
 * Copyright 1996 Alexandre Julliard
 */

#ifndef __WINE_WINDEF_H
#define __WINE_WINDEF_H

#ifndef NO_STRICT
# ifndef STRICT
#  define STRICT 1
# endif
#endif /* NO_STRICT */

#ifdef __WINE__
# undef UNICODE
#endif  /* __WINE__ */

#ifndef WINVER
# define WINVER 0x0500
#endif

#include "winnt.h"


#ifdef __cplusplus
extern "C" {
#endif


/* Macros to map Winelib names to the correct implementation name */
/* depending on __WINE__ and UNICODE macros.                      */
/* Note that Winelib is purely Win32.                             */

#ifdef __WINE__
#define WINE_NO_UNICODE_MACROS
#endif

#ifdef WINE_NO_UNICODE_MACROS
# define WINELIB_NAME_AW(func) \
    func##_must_be_suffixed_with_W_or_A_in_this_context \
    func##_must_be_suffixed_with_W_or_A_in_this_context
#else  /* __WINE__ */
# ifdef UNICODE
#  define WINELIB_NAME_AW(func) func##W
# else
#  define WINELIB_NAME_AW(func) func##A
# endif  /* UNICODE */
#endif  /* WINE_NO_UNICODE_MACROS */

#ifdef WINE_NO_UNICODE_MACROS
# define DECL_WINELIB_TYPE_AW(type)  /* nothing */
#else   /* __WINE__ */
# define DECL_WINELIB_TYPE_AW(type)  typedef WINELIB_NAME_AW(type) type;
#endif  /* WINE_NO_UNICODE_MACROS */

#ifndef __ms_va_list
# if defined(__x86_64__) && defined (__GNUC__)
#  define __ms_va_list __builtin_ms_va_list
#  define __ms_va_start(list,arg) __builtin_ms_va_start(list,arg)
#  define __ms_va_end(list) __builtin_ms_va_end(list)
# else
#  define __ms_va_list va_list
#  define __ms_va_start(list,arg) va_start(list,arg)
#  define __ms_va_end(list) va_end(list)
# endif
#endif


/* Integer types */
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;
typedef WORD            ATOM;
typedef WORD            CATCHBUF[9];
typedef WORD           *LPCATCHBUF;
typedef DWORD           COLORREF, *LPCOLORREF;


/* Handle types that exist both in Win16 and Win32. */

typedef int HFILE;
DECLARE_HANDLE(HACCEL);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HCOLORSPACE);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HDESK);
DECLARE_HANDLE(HENHMETAFILE);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HHOOK);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HKL);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HMETAFILE);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HPEN);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HRSRC);
DECLARE_HANDLE(HTASK);
DECLARE_HANDLE(HWINSTA);
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HWINEVENTHOOK);

/* Handle types that must remain interchangeable even with strict on */

typedef HINSTANCE HMODULE;
typedef HANDLE HGDIOBJ;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;
typedef HANDLE GLOBALHANDLE;
typedef HANDLE LOCALHANDLE;
typedef HICON HCURSOR;

/* Callback function pointers types */

#ifdef __WINE__
/* We force these to have empty parameter lists as some compilers can be confused if we do
   multiple calls in a single function with different parameter lists. We thus want to use full
   prototypes everywhere we can */
typedef INT     (CALLBACK *FARPROC)(void);
typedef INT     (CALLBACK *PROC)(void);
#else
typedef INT     (CALLBACK *FARPROC)();
typedef INT     (CALLBACK *PROC)();
#endif

/* inline functions */
#define WINAPI_INLINE          WINAPI

/* Macros to split words and longs. */

#define LOBYTE(w)              ((BYTE)(WORD)(w))
#define HIBYTE(w)              ((BYTE)((WORD)(w) >> 8))

#define LOWORD(l)              ((WORD)(DWORD)(l))
#define HIWORD(l)              ((WORD)((DWORD)(l) >> 16))

#define SLOWORD(l)             ((SHORT)(LONG)(l))
#define SHIWORD(l)             ((SHORT)((LONG)(l) >> 16))

#define MAKEWORD(low,high)     ((WORD)(((BYTE)(low)) | ((WORD)((BYTE)(high))) << 8))
#define MAKELONG(low,high)     ((LONG)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
#define MAKELPARAM(low,high)   ((LPARAM)MAKELONG(low,high))
#define MAKEWPARAM(low,high)   ((WPARAM)MAKELONG(low,high))
#define MAKELRESULT(low,high)  ((LRESULT)MAKELONG(low,high))

#define SELECTOROF(ptr)     (HIWORD(ptr))
#define OFFSETOF(ptr)       (LOWORD(ptr))

#ifdef __WINE__
/* macros to set parts of a DWORD (not in the Windows API) */
#define SET_LOWORD(dw,val)  ((dw) = ((dw) & 0xffff0000) | LOWORD(val))
#define SET_LOBYTE(dw,val)  ((dw) = ((dw) & 0xffffff00) | LOBYTE(val))
#define SET_HIBYTE(dw,val)  ((dw) = ((dw) & 0xffff00ff) | (LOBYTE(val) << 8))
#define ADD_LOWORD(dw,val)  ((dw) = ((dw) & 0xffff0000) | LOWORD((DWORD)(dw)+(val)))
#endif

/* min and max macros */
#ifndef NOMINMAX
#ifndef max
#define max(a,b)   (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#endif
#endif  /* NOMINMAX */

#ifndef _MAX_PATH
/* FIXME: These are supposed to be in stdlib.h only */
#define _MAX_DRIVE          3
#define _MAX_FNAME          256
#define _MAX_DIR            _MAX_FNAME
#define _MAX_EXT            _MAX_FNAME
#define _MAX_PATH           260
#endif
#ifndef MAX_PATH
#define MAX_PATH            260
#endif


#define HFILE_ERROR     ((HFILE)-1)

/* The SIZE structure */
typedef struct tagSIZE
{
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef SIZE SIZEL, *PSIZEL, *LPSIZEL;

/* The POINT structure */
typedef struct tagPOINT
{
    LONG  x;
    LONG  y;
} POINT, *PPOINT, *LPPOINT;

typedef struct _POINTL
{
    LONG x;
    LONG y;
} POINTL;

/* The POINTS structure */

typedef struct tagPOINTS
{
    SHORT x;
    SHORT y;
} POINTS, *PPOINTS, *LPPOINTS;

/* The RECT structure */
typedef struct tagRECT
{
    INT  left;
    INT  top;
    INT  right;
    INT  bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT *LPCRECT;


typedef struct tagRECTL
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECTL, *PRECTL, *LPRECTL;

typedef const RECTL *LPCRECTL;

#ifdef __cplusplus
}
#endif

#endif /* __WINE_WINDEF_H */
