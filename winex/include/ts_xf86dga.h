/*
 * Thread safe wrappers around xf86dga calls.
 * Always include this file instead of <X11/xf86dga.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_XF86DGA_H
#define __WINE_TS_XF86DGA_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif

#ifdef HAVE_LIBXXF86DGA

#include <X11/Xlib.h>
#include <X11/extensions/xf86dga.h>

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

extern Bool TSXF86DGAQueryVersion(Display*,int*,int*);
extern Bool TSXF86DGAQueryExtension(Display*,int*,int*);
extern Status TSXF86DGAGetVideo(Display*,int,char**,int*,int*,int*);
extern Status TSXF86DGADirectVideo(Display*,int,int);
extern Status TSXF86DGAGetViewPortSize(Display*,int,int*,int*);
extern Status TSXF86DGASetViewPort(Display*,int,int,int);
extern Status TSXF86DGAInstallColormap(Display*,int,Colormap);
extern Status TSXF86DGAQueryDirectVideo(Display*,int,int*);
extern Status TSXF86DGAViewPortChanged(Display*,int,int);

#endif /* defined(HAVE_LIBXXF86DGA) */

#endif /* __WINE_TS_XF86DGA_H */
