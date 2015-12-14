/*
 * Thread safe wrappers around Xrender calls.
 * Always include this file instead of <X11/Xrender.h>.
 * This file was generated automatically by tools/make_X11wrappers
 * DO NOT EDIT!
 */

#ifndef __WINE_TS_XRENDER_H
#define __WINE_TS_XRENDER_H

#ifndef __WINE_CONFIG_H
# error "You must include config.h to use this header"
#endif

#ifdef HAVE_LIBXRENDER

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

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

extern void TSXRenderAddGlyphs(Display*,GlyphSet,Glyph*,XGlyphInfo*,int,char*,int);
extern void TSXRenderCompositeString8(Display*,int,Picture,Picture,const XRenderPictFormat*,GlyphSet,int,int,int,int,const char*,int);
extern void TSXRenderCompositeString16(Display*,int,Picture,Picture,const XRenderPictFormat*,GlyphSet,int,int,int,int,const unsigned short*,int);
extern void TSXRenderCompositeString32(Display*,int,Picture,Picture,const XRenderPictFormat*,GlyphSet,int,int,int,int,const unsigned int*,int);
extern GlyphSet TSXRenderCreateGlyphSet(Display*,XRenderPictFormat*);
extern Picture TSXRenderCreatePicture(Display*,Drawable,XRenderPictFormat*,unsigned long,XRenderPictureAttributes*);
extern void TSXRenderFillRectangle(Display*,int,Picture,XRenderColor*,int,int,unsigned int, unsigned int);
extern XRenderPictFormat* TSXRenderFindFormat(Display*,unsigned long,XRenderPictFormat*,int);
extern XRenderPictFormat* TSXRenderFindVisualFormat(Display*,Visual*);
extern void TSXRenderFreeGlyphSet(Display*,GlyphSet);
extern void TSXRenderFreePicture(Display*,Picture);
extern void TSXRenderSetPictureClipRectangles(Display*,Picture,int,int,XRectangle*,int);
extern Bool TSXRenderQueryExtension(Display*,int*,int*);

#endif /* defined(HAVE_LIBXRENDER) */

#endif /* __WINE_TS_XRENDER_H */
