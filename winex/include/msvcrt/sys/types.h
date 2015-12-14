/*
 * _stat() definitions
 *
 * Copyright 2000 Francois Gouget.
 */
#ifndef __WINE_SYS_TYPES_H
#define __WINE_SYS_TYPES_H

#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif


#ifdef USE_MSVCRT_PREFIX
#define MSVCRT(x)    MSVCRT_##x
#else
#define MSVCRT(x)    x
#endif


typedef unsigned int   _dev_t;
typedef unsigned short _ino_t;
typedef int            _off_t;
typedef long           MSVCRT(time_t);


#ifndef USE_MSVCRT_PREFIX
#define dev_t _dev_t
#define ino_t _ino_t
#define off_t _off_t
#endif /* USE_MSVCRT_PREFIX */

#endif /* __WINE_SYS_TYPES_H */
