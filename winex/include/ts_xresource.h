/*
 * Thread safe wrappers around Xresource calls.
 * Always include this file instead of <X11/Xresource.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_XRESOURCE_H
#define __WINE_TS_XRESOURCE_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif


#include <X11/Xlib.h>
#include <X11/Xresource.h>

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

extern XrmQuark  TSXrmUniqueQuark(void);
extern int  TSXrmGetResource(XrmDatabase, const char*, const char*, char**, XrmValue*);
extern XrmDatabase  TSXrmGetFileDatabase(const char*);
extern XrmDatabase  TSXrmGetStringDatabase(const char*);
extern void  TSXrmMergeDatabases(XrmDatabase, XrmDatabase*);
extern void  TSXrmParseCommand(XrmDatabase*, XrmOptionDescList, int, const char*, int*, char**);


#endif /* __WINE_TS_XRESOURCE_H */
