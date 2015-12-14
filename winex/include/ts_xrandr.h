/*
 * Thread safe wrappers around Xrandr calls.
 * Always include this file instead of <X11/Xrandr.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_XRANDR_H
#define __WINE_TS_XRANDR_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif

#ifdef HAVE_LIBXRANDR

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

/* Multithread protection for X */
extern void (*wine_tsx11_lock)(void);
extern void (*wine_tsx11_unlock)(void);

/* Setting error handlers - must be set with caution */
#ifndef WINE_X11_ERROR_HANDLER_TYPE
#define WINE_X11_ERROR_HANDLER_TYPE
typedef int (*wine_x11_error_handler)(Display *, XErrorEvent *);
#endif /* #ifndef WINE_X11_ERROR_HANDLER_TYPE */

extern void X11DRV_SetXErrorHandler( wine_x11_error_handler new_handler );
extern void X11DRV_RestoreXErrorHandler(void);


#endif /* defined(HAVE_LIBXRANDR) */

#endif /* __WINE_TS_XRANDR_H */
