/*
 * wsprintf functions
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "stackframe.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(string);


#define WPRINTF_LEFTALIGN   0x0001  /* Align output on the left ('-' prefix) */
#define WPRINTF_PREFIX_HEX  0x0002  /* Prefix hex with 0x ('#' prefix) */
#define WPRINTF_ZEROPAD     0x0004  /* Pad with zeros ('0' prefix) */
#define WPRINTF_LONG        0x0008  /* Long arg ('l' prefix) */
#define WPRINTF_SHORT       0x0010  /* Short arg ('h' prefix) */
#define WPRINTF_UPPER_HEX   0x0020  /* Upper-case hex ('X' specifier) */
#define WPRINTF_WIDE        0x0040  /* Wide arg ('w' prefix) */

typedef enum
{
    WPR_UNKNOWN,
    WPR_CHAR,
    WPR_WCHAR,
    WPR_STRING,
    WPR_WSTRING,
    WPR_SIGNED,
    WPR_UNSIGNED,
    WPR_HEXA
} WPRINTF_TYPE;

typedef struct
{
    UINT         flags;
    UINT         width;
    UINT         precision;
    WPRINTF_TYPE   type;
} WPRINTF_FORMAT;

typedef union {
    WCHAR   wchar_view;
    CHAR    char_view;
    LPCSTR  lpcstr_view;
    LPCWSTR lpcwstr_view;
    INT     int_view;
} WPRINTF_DATA;

static const CHAR null_stringA[] = "(null)";
static const WCHAR null_stringW[] = { '(', 'n', 'u', 'l', 'l', ')', 0 };

/***********************************************************************
 *           WPRINTF_ParseFormatA
 *
 * Parse a format specification. A format specification has the form:
 *
 * [-][#][0][width][.precision]type
 *
 * Return value is the length of the format specification in characters.
 */
static INT WPRINTF_ParseFormatA( LPCSTR format, WPRINTF_FORMAT *res )
{
    LPCSTR p = format;

    res->flags = 0;
    res->width = 0;
    res->precision = 0;
    if (*p == '-') { res->flags |= WPRINTF_LEFTALIGN; p++; }
    if (*p == '#') { res->flags |= WPRINTF_PREFIX_HEX; p++; }
    if (*p == '0') { res->flags |= WPRINTF_ZEROPAD; p++; }
    while ((*p >= '0') && (*p <= '9'))  /* width field */
    {
        res->width = res->width * 10 + *p - '0';
        p++;
    }
    if (*p == '.')  /* precision field */
    {
        p++;
        while ((*p >= '0') && (*p <= '9'))
        {
            res->precision = res->precision * 10 + *p - '0';
            p++;
        }
    }
    if (*p == 'l') { res->flags |= WPRINTF_LONG; p++; }
    else if (*p == 'h') { res->flags |= WPRINTF_SHORT; p++; }
    else if (*p == 'w') { res->flags |= WPRINTF_WIDE; p++; }
    switch(*p)
    {
    case 'c':
        res->type = (res->flags & WPRINTF_LONG) ? WPR_WCHAR : WPR_CHAR;
        break;
    case 'C':
        res->type = (res->flags & WPRINTF_SHORT) ? WPR_CHAR : WPR_WCHAR;
        break;
    case 'd':
    case 'i':
        res->type = WPR_SIGNED;
        break;
    case 's':
        res->type = (res->flags & (WPRINTF_LONG |WPRINTF_WIDE)) ? WPR_WSTRING : WPR_STRING;
        break;
    case 'S':
        res->type = (res->flags & (WPRINTF_SHORT|WPRINTF_WIDE)) ? WPR_STRING : WPR_WSTRING;
        break;
    case 'u':
        res->type = WPR_UNSIGNED;
        break;
    case 'X':
        res->flags |= WPRINTF_UPPER_HEX;
        /* fall through */
    case 'x':
        res->type = WPR_HEXA;
        break;
    default: /* unknown format char */
        res->type = WPR_UNKNOWN;
        p--;  /* print format as normal char */
        break;
    }
    return (INT)(p - format) + 1;
}


/***********************************************************************
 *           WPRINTF_ParseFormatW
 *
 * Parse a format specification. A format specification has the form:
 *
 * [-][#][0][width][.precision]type
 *
 * Return value is the length of the format specification in characters.
 */
static INT WPRINTF_ParseFormatW( LPCWSTR format, WPRINTF_FORMAT *res )
{
    LPCWSTR p = format;

    res->flags = 0;
    res->width = 0;
    res->precision = 0;
    if (*p == '-') { res->flags |= WPRINTF_LEFTALIGN; p++; }
    if (*p == '#') { res->flags |= WPRINTF_PREFIX_HEX; p++; }
    if (*p == '0') { res->flags |= WPRINTF_ZEROPAD; p++; }
    while ((*p >= '0') && (*p <= '9'))  /* width field */
    {
        res->width = res->width * 10 + *p - '0';
        p++;
    }
    if (*p == '.')  /* precision field */
    {
        p++;
        while ((*p >= '0') && (*p <= '9'))
        {
            res->precision = res->precision * 10 + *p - '0';
            p++;
        }
    }
    if (*p == 'l') { res->flags |= WPRINTF_LONG; p++; }
    else if (*p == 'h') { res->flags |= WPRINTF_SHORT; p++; }
    else if (*p == 'w') { res->flags |= WPRINTF_WIDE; p++; }
    switch((CHAR)*p)
    {
    case 'c':
        res->type = (res->flags & WPRINTF_SHORT) ? WPR_CHAR : WPR_WCHAR;
        break;
    case 'C':
        res->type = (res->flags & WPRINTF_LONG) ? WPR_WCHAR : WPR_CHAR;
        break;
    case 'd':
    case 'i':
        res->type = WPR_SIGNED;
        break;
    case 's':
        res->type = ((res->flags & WPRINTF_SHORT) && !(res->flags & WPRINTF_WIDE)) ? WPR_STRING : WPR_WSTRING;
        break;
    case 'S':
        res->type = (res->flags & (WPRINTF_LONG|WPRINTF_WIDE)) ? WPR_WSTRING : WPR_STRING;
        break;
    case 'u':
        res->type = WPR_UNSIGNED;
        break;
    case 'X':
        res->flags |= WPRINTF_UPPER_HEX;
        /* fall through */
    case 'x':
        res->type = WPR_HEXA;
        break;
    default:
        res->type = WPR_UNKNOWN;
        p--;  /* print format as normal char */
        break;
    }
    return (INT)(p - format) + 1;
}


/***********************************************************************
 *           WPRINTF_GetLen
 */
static UINT WPRINTF_GetLen( WPRINTF_FORMAT *format, WPRINTF_DATA *arg,
                              LPSTR number, UINT maxlen )
{
    UINT len;

    if (format->flags & WPRINTF_LEFTALIGN) format->flags &= ~WPRINTF_ZEROPAD;
    if (format->width > maxlen) format->width = maxlen;
    switch(format->type)
    {
    case WPR_CHAR:
    case WPR_WCHAR:
        return (format->precision = 1);
    case WPR_STRING:
        if (!arg->lpcstr_view) arg->lpcstr_view = null_stringA;
        for (len = 0; !format->precision || (len < format->precision); len++)
            if (!*(arg->lpcstr_view + len)) break;
        if (len > maxlen) len = maxlen;
        return (format->precision = len);
    case WPR_WSTRING:
        if (!arg->lpcwstr_view) arg->lpcwstr_view = null_stringW;
        for (len = 0; !format->precision || (len < format->precision); len++)
            if (!*(arg->lpcwstr_view + len)) break;
        if (len > maxlen) len = maxlen;
        return (format->precision = len);
    case WPR_SIGNED:
        len = sprintf( number, "%d", arg->int_view );
        break;
    case WPR_UNSIGNED:
        len = sprintf( number, "%u", (UINT)arg->int_view );
        break;
    case WPR_HEXA:
        len = sprintf( number,
                       (format->flags & WPRINTF_UPPER_HEX) ? "%X" : "%x",
                       (UINT)arg->int_view);
        break;
    default:
        return 0;
    }
    if (len > maxlen) len = maxlen;
    if (format->precision < len) format->precision = len;
    if (format->precision > maxlen) format->precision = maxlen;
    if ((format->flags & WPRINTF_ZEROPAD) && (format->width > format->precision))
        format->precision = format->width;
    if (format->flags & WPRINTF_PREFIX_HEX) len += 2;
    return len;
}


/***********************************************************************
 *           wvsnprintf16   (Not a Windows API)
 */
static INT16 wvsnprintf16( LPSTR buffer, UINT16 maxlen, LPCSTR spec,
                           LPCVOID args )
{
    WPRINTF_FORMAT format;
    LPSTR p = buffer;
    UINT i, len, sign;
    CHAR number[20];
    WPRINTF_DATA cur_arg;
    SEGPTR seg_str;

    while (*spec && (maxlen > 1))
    {
        if (*spec != '%') { *p++ = *spec++; maxlen--; continue; }
        spec++;
        if (*spec == '%') { *p++ = *spec++; maxlen--; continue; }
        spec += WPRINTF_ParseFormatA( spec, &format );
        switch(format.type)
        {
        case WPR_WCHAR:  /* No Unicode in Win16 */
        case WPR_CHAR:
            cur_arg.char_view = VA_ARG16( args, CHAR );
            break;
        case WPR_WSTRING:  /* No Unicode in Win16 */
        case WPR_STRING:
            seg_str = VA_ARG16( args, SEGPTR );
            if (IsBadReadPtr16(seg_str, 1 )) cur_arg.lpcstr_view = "";
            else cur_arg.lpcstr_view = MapSL( seg_str );
            break;
        case WPR_SIGNED:
            if (!(format.flags & WPRINTF_LONG))
            {
                cur_arg.int_view = VA_ARG16( args, INT16 );
                break;
            }
            /* fall through */
        case WPR_HEXA:
        case WPR_UNSIGNED:
            if (format.flags & WPRINTF_LONG)
                cur_arg.int_view = VA_ARG16( args, UINT );
            else
                cur_arg.int_view = VA_ARG16( args, UINT16 );
            break;
        case WPR_UNKNOWN:
            continue;
        }
        len = WPRINTF_GetLen( &format, &cur_arg, number, maxlen - 1 );
        sign = 0;
        if (!(format.flags & WPRINTF_LEFTALIGN))
            for (i = format.precision; i < format.width; i++, maxlen--)
                *p++ = ' ';
        switch(format.type)
        {
        case WPR_WCHAR:  /* No Unicode in Win16 */
        case WPR_CHAR:
            *p= cur_arg.char_view;
            /* wsprintf16 (unlike wsprintf) ignores null characters */
            if (*p != '\0') p++;
            else if (format.width > 1) *p++ = ' ';
            else len = 0;
            break;
        case WPR_WSTRING:  /* No Unicode in Win16 */
        case WPR_STRING:
            if (len) memcpy( p, cur_arg.lpcstr_view, len );
            p += len;
            break;
        case WPR_HEXA:
            if ((format.flags & WPRINTF_PREFIX_HEX) && (maxlen > 3))
            {
                *p++ = '0';
                *p++ = (format.flags & WPRINTF_UPPER_HEX) ? 'X' : 'x';
                maxlen -= 2;
                len -= 2;
            }
            /* fall through */
        case WPR_SIGNED:
            /* Transfer the sign now, just in case it will be zero-padded*/
            if (number[0] == '-')
            {
                *p++ = '-';
                sign = 1;
            }
            /* fall through */
        case WPR_UNSIGNED:
            for (i = len; i < format.precision; i++, maxlen--) *p++ = '0';
            if (len > sign) memcpy( p, number + sign, len - sign );
            p += len;
            break;
        case WPR_UNKNOWN:
            continue;
        }
        if (format.flags & WPRINTF_LEFTALIGN)
            for (i = format.precision; i < format.width; i++, maxlen--)
                *p++ = ' ';
        maxlen -= len;
    }
    *p = 0;
    return (maxlen > 1) ? (INT)(p - buffer) : -1;
}


/***********************************************************************
 *           wvsnprintfA   (USER32.@) (Not a Windows API, but we export it from USER32 anyway)
 */
INT WINAPI wvsnprintfA( LPSTR buffer, UINT maxlen, LPCSTR spec, va_list args )
{
    WPRINTF_FORMAT format;
    LPSTR p = buffer;
    UINT i, len, sign;
    CHAR number[20];
    WPRINTF_DATA argData;

    TRACE("%p %u %s\n", buffer, maxlen, debugstr_a(spec));

    while (*spec && (maxlen > 1))
    {
        if (*spec != '%') { *p++ = *spec++; maxlen--; continue; }
        spec++;
        if (*spec == '%') { *p++ = *spec++; maxlen--; continue; }
        spec += WPRINTF_ParseFormatA( spec, &format );

        switch(format.type)
        {
        case WPR_WCHAR:
            argData.wchar_view = (WCHAR)va_arg( args, int );
            break;
        case WPR_CHAR:
            argData.char_view = (CHAR)va_arg( args, int );
            break;
        case WPR_STRING:
            argData.lpcstr_view = va_arg( args, LPCSTR );
            break;
        case WPR_WSTRING:
            argData.lpcwstr_view = va_arg( args, LPCWSTR );
            break;
        case WPR_HEXA:
        case WPR_SIGNED:
        case WPR_UNSIGNED:
            argData.int_view = va_arg( args, INT );
            break;
        default:
            argData.wchar_view = 0;
            break;
        }

        len = WPRINTF_GetLen( &format, &argData, number, maxlen - 1 );
        sign = 0;
        if (!(format.flags & WPRINTF_LEFTALIGN))
            for (i = format.precision; i < format.width; i++, maxlen--)
                *p++ = ' ';
        switch(format.type)
        {
        case WPR_WCHAR:
            *p++ = argData.wchar_view;
            break;
        case WPR_CHAR:
            *p++ = argData.char_view;
            break;
        case WPR_STRING:
            memcpy( p, argData.lpcstr_view, len );
            p += len;
            break;
        case WPR_WSTRING:
            {
                LPCWSTR ptr = argData.lpcwstr_view;
                for (i = 0; i < len; i++) *p++ = (CHAR)*ptr++;
            }
            break;
        case WPR_HEXA:
            if ((format.flags & WPRINTF_PREFIX_HEX) && (maxlen > 3))
            {
                *p++ = '0';
                *p++ = (format.flags & WPRINTF_UPPER_HEX) ? 'X' : 'x';
                maxlen -= 2;
                len -= 2;
            }
            /* fall through */
        case WPR_SIGNED:
            /* Transfer the sign now, just in case it will be zero-padded*/
            if (number[0] == '-')
            {
                *p++ = '-';
                sign = 1;
            }
            /* fall through */
        case WPR_UNSIGNED:
            for (i = len; i < format.precision; i++, maxlen--) *p++ = '0';
            memcpy( p, number + sign, len - sign  );
            p += len - sign;
            break;
        case WPR_UNKNOWN:
            continue;
        }
        if (format.flags & WPRINTF_LEFTALIGN)
            for (i = format.precision; i < format.width; i++, maxlen--)
                *p++ = ' ';
        maxlen -= len;
    }
    *p = 0;
    TRACE("%s\n",debugstr_a(buffer));
    return (maxlen > 1) ? (INT)(p - buffer) : -1;
}


/***********************************************************************
 *           wvsnprintfW   (USER32.@) (Not a Windows API, but we export it from USER32 anyway)
 */
INT WINAPI wvsnprintfW( LPWSTR buffer, UINT maxlen, LPCWSTR spec, va_list args )
{
    WPRINTF_FORMAT format;
    LPWSTR p = buffer;
    UINT i, len, sign;
    CHAR number[20];
    WPRINTF_DATA argData;

    TRACE("%p %u %s\n", buffer, maxlen, debugstr_w(spec));

    while (*spec && (maxlen > 1))
    {
        if (*spec != '%') { *p++ = *spec++; maxlen--; continue; }
        spec++;
        if (*spec == '%') { *p++ = *spec++; maxlen--; continue; }
        spec += WPRINTF_ParseFormatW( spec, &format );

        switch(format.type)
        {
        case WPR_WCHAR:
            argData.wchar_view = (WCHAR)va_arg( args, int );
            break;
        case WPR_CHAR:
            argData.char_view = (CHAR)va_arg( args, int );
            break;
        case WPR_STRING:
            argData.lpcstr_view = va_arg( args, LPCSTR );
            break;
        case WPR_WSTRING:
            argData.lpcwstr_view = va_arg( args, LPCWSTR );
            break;
        case WPR_HEXA:
        case WPR_SIGNED:
        case WPR_UNSIGNED:
            argData.int_view = va_arg( args, INT );
            break;
        default:
            argData.wchar_view = 0;
            break;
        }

        len = WPRINTF_GetLen( &format, &argData, number, maxlen - 1 );
        sign = 0;
        if (!(format.flags & WPRINTF_LEFTALIGN))
            for (i = format.precision; i < format.width; i++, maxlen--)
                *p++ = ' ';
        switch(format.type)
        {
        case WPR_WCHAR:
            *p++ = argData.wchar_view;
            break;
        case WPR_CHAR:
            *p++ = argData.char_view;
            break;
        case WPR_STRING:
            {
                LPCSTR ptr = argData.lpcstr_view;
                for (i = 0; i < len; i++) *p++ = (WCHAR)*ptr++;
            }
            break;
        case WPR_WSTRING:
            if (len) memcpy( p, argData.lpcwstr_view, len * sizeof(WCHAR) );
            p += len;
            break;
        case WPR_HEXA:
            if ((format.flags & WPRINTF_PREFIX_HEX) && (maxlen > 3))
            {
                *p++ = '0';
                *p++ = (format.flags & WPRINTF_UPPER_HEX) ? 'X' : 'x';
                maxlen -= 2;
                len -= 2;
            }
            /* fall through */
        case WPR_SIGNED:
            /* Transfer the sign now, just in case it will be zero-padded*/
            if (number[0] == '-')
            {
                *p++ = '-';
                sign = 1;
            }
            /* fall through */
        case WPR_UNSIGNED:
            for (i = len; i < format.precision; i++, maxlen--) *p++ = '0';
            for (i = sign; i < len; i++) *p++ = (WCHAR)number[i];
            break;
        case WPR_UNKNOWN:
            continue;
        }
        if (format.flags & WPRINTF_LEFTALIGN)
            for (i = format.precision; i < format.width; i++, maxlen--)
                *p++ = ' ';
        maxlen -= len;
    }
    *p = 0;
    TRACE("%s\n",debugstr_w(buffer));
    return (maxlen > 1) ? (INT)(p - buffer) : -1;
}


/***********************************************************************
 *           wvsprintf   (USER.421)
 */
INT16 WINAPI wvsprintf16( LPSTR buffer, LPCSTR spec, LPCVOID args )
{
    INT16 res;

    TRACE("for %p got:\n",buffer);
    res = wvsnprintf16( buffer, 1024, spec, args );
    return ( res == -1 ) ? 1024 : res;
}


/***********************************************************************
 *           wvsprintfA   (USER32.@)
 */
INT WINAPI wvsprintfA( LPSTR buffer, LPCSTR spec, va_list args )
{
    INT res = wvsnprintfA( buffer, 1024, spec, args );
    return ( res == -1 ) ? 1024 : res;
}


/***********************************************************************
 *           wvsprintfW   (USER32.@)
 */
INT WINAPI wvsprintfW( LPWSTR buffer, LPCWSTR spec, va_list args )
{
    INT res = wvsnprintfW( buffer, 1024, spec, args );
    return ( res == -1 ) ? 1024 : res;
}


/***********************************************************************
 *           _wsprintf   (USER.420)
 */
INT16 WINAPIV wsprintf16(void)
{
    VA_LIST16 valist;
    INT16 res;
    SEGPTR buffer, spec;

    VA_START16( valist );
    buffer = VA_ARG16( valist, SEGPTR );
    spec   = VA_ARG16( valist, SEGPTR );
    res = wvsnprintf16( MapSL(buffer), 1024, MapSL(spec), valist );
    VA_END16( valist );
    return ( res == -1 ) ? 1024 : res;
}


/***********************************************************************
 *           wsprintfA   (USER32.@)
 */
INT WINAPIV wsprintfA( LPSTR buffer, LPCSTR spec, ... )
{
    va_list valist;
    INT res;

    va_start( valist, spec );
    res = wvsnprintfA( buffer, 1024, spec, valist );
    va_end( valist );
    return ( res == -1 ) ? 1024 : res;
}


/***********************************************************************
 *           wsprintfW   (USER32.@)
 */
INT WINAPIV wsprintfW( LPWSTR buffer, LPCWSTR spec, ... )
{
    va_list valist;
    INT res;

    va_start( valist, spec );
    res = wvsnprintfW( buffer, 1024, spec, valist );
    va_end( valist );
    return ( res == -1 ) ? 1024 : res;
}
