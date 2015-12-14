/*
 * Thread safe wrappers around shape calls.
 * Always include this file instead of <X11/shape.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_SHAPE_H
#define __WINE_TS_SHAPE_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif

#ifdef HAVE_LIBXSHAPE
#include <X11/IntrinsicP.h>

#include <X11/extensions/shape.h>

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

extern void  TSXShapeCombineRectangles(Display*, Window, int, int, int, XRectangle*, int, int, int);
extern void  TSXShapeCombineMask(Display*, Window, int, int, int, Pixmap, int);

#endif /* defined(HAVE_LIBXSHAPE) */

#endif /* __WINE_TS_SHAPE_H */
