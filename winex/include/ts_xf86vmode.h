/*
 * Thread safe wrappers around xf86vmode calls.
 * Always include this file instead of <X11/xf86vmode.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_XF86VMODE_H
#define __WINE_TS_XF86VMODE_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif

#include "windef.h"
#ifdef HAVE_LIBXXF86VM
#define XMD_H
#include "basetsd.h"

#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

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

extern Bool TSXF86VidModeQueryVersion(Display*,int*,int*);
extern Bool TSXF86VidModeQueryExtension(Display*,int*,int*);
extern Bool TSXF86VidModeGetModeLine(Display*,int,int*,XF86VidModeModeLine*);
extern Bool TSXF86VidModeGetAllModeLines(Display*,int,int*,XF86VidModeModeInfo***);
extern Bool TSXF86VidModeAddModeLine(Display*,int,XF86VidModeModeInfo*,XF86VidModeModeInfo*);
extern Bool TSXF86VidModeDeleteModeLine(Display*,int,XF86VidModeModeInfo*);
extern Bool TSXF86VidModeModModeLine(Display*,int,XF86VidModeModeLine*);
extern Status TSXF86VidModeValidateModeLine(Display*,int,XF86VidModeModeInfo*);
extern Bool TSXF86VidModeSwitchMode(Display*,int,int);
extern Bool TSXF86VidModeSwitchToMode(Display*,int,XF86VidModeModeInfo*);
extern Bool TSXF86VidModeLockModeSwitch(Display*,int,int);
extern Bool TSXF86VidModeGetMonitor(Display*,int,XF86VidModeMonitor*);
extern Bool TSXF86VidModeGetViewPort(Display*,int,int*,int*);
extern Bool TSXF86VidModeSetViewPort(Display*,int,int,int);

#if defined( X_XF86VidModeGetGammaRampSize )
extern Bool TSXF86VidModeGetGammaRampSize(Display*,int,int*);
#endif /* defined( X_XF86VidModeGetGammaRampSize ) */


#endif /* defined(HAVE_LIBXXF86VM) */

#endif /* __WINE_TS_XF86VMODE_H */
