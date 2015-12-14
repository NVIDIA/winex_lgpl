/*
 * Thread safe wrappers around Xutil calls.
 * Always include this file instead of <X11/Xutil.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_XUTIL_H
#define __WINE_TS_XUTIL_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif


#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

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

extern XClassHint * TSXAllocClassHint(void);
extern XSizeHints * TSXAllocSizeHints(void);
extern XWMHints * TSXAllocWMHints(void);
extern int  TSXClipBox(Region, XRectangle*);
extern Region  TSXCreateRegion(void);
extern int  TSXDestroyRegion(Region);
extern int  TSXEmptyRegion(Region);
extern int  TSXEqualRegion(Region, Region);
extern int  TSXFindContext(Display*, XID, XContext, XPointer*);
extern XVisualInfo * TSXGetVisualInfo(Display*, long, XVisualInfo*, int*);
extern XWMHints * TSXGetWMHints(Display*, Window);
extern int  TSXGetWMSizeHints(Display*, Window, XSizeHints*, long*, Atom);
extern int  TSXIntersectRegion(Region, Region, Region);
extern int  TSXLookupString(XKeyEvent*, char*, int, KeySym*, XComposeStatus*);
extern int  TSXOffsetRegion(Region, int, int);
extern int  TSXPointInRegion(Region, int, int);
extern Region  TSXPolygonRegion(XPoint*, int, int);
extern int  TSXRectInRegion(Region, int, int, unsigned int, unsigned int);
extern int  TSXSaveContext(Display*, XID, XContext, const char*);
extern int  TSXSetClassHint(Display*, Window, XClassHint*);
extern int  TSXSetWMHints(Display*, Window, XWMHints*);
extern void  TSXSetWMProperties(Display*, Window, XTextProperty*, XTextProperty*, char**, int, XSizeHints*, XWMHints*, XClassHint*);
extern void  TSXSetWMSizeHints(Display*, Window, XSizeHints*, Atom);
extern int  TSXSetRegion(Display*, GC, Region);
extern int  TSXShrinkRegion(Region, int, int);
extern int  TSXStringListToTextProperty(char**, int, XTextProperty*);
extern int  TSXSubtractRegion(Region, Region, Region);
extern int  TSXUnionRectWithRegion(XRectangle*, Region, Region);
extern int  TSXUnionRegion(Region, Region, Region);
extern int  TSXXorRegion(Region, Region, Region);
extern int TSXDestroyImage(struct _XImage *);
extern struct _XImage * TSXSubImage(struct _XImage *, int, int, unsigned int, unsigned int);
extern int TSXAddPixel(struct _XImage *, long);
extern XContext TSXUniqueContext(void);
extern int TSXDeleteContext(Display*,XID,XContext);


#endif /* __WINE_TS_XUTIL_H */
