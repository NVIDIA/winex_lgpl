/*
 * X11 driver definitions
 */

#ifndef __WINE_X11DRV_H
#define __WINE_X11DRV_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef HAVE_LIBXXSHM
# include <X11/extensions/XShm.h>
#endif /* defined(HAVE_LIBXXSHM) */

/* Xlib defines Status as an int, and we need to use it as a variable name elsewhere. */
#undef Status
typedef int Status;

#include "windef.h"
#include "winbase.h"
#include "gdi.h"
#include "user.h"
#include "bitmap.h"
#include "win.h"
#include "wine/thread.h"

struct tagBITMAPOBJ;
struct tagCURSORICONINFO;
struct tagDC;
struct tagDeviceCaps;
struct tagPALETTEOBJ;
struct tagWINDOWPOS;

  /* X physical pen */
typedef struct
{
    int          style;
    int          endcap;
    int          linejoin;
    int          pixel;
    int          width;
    char *       dashes;
    int          dash_len;
    int          type;          /* GEOMETRIC || COSMETIC */
} X_PHYSPEN;

  /* X physical brush */
typedef struct
{
    int          style;
    int          fillStyle;
    int          pixel;
    Pixmap       pixmap;
} X_PHYSBRUSH;

  /* X physical font */
typedef UINT	 X_PHYSFONT;

typedef struct tagXRENDERINFO *XRENDERINFO;

  /* X physical device */
typedef struct
{
    GC            gc;          /* X Window GC */
    Drawable      drawable;
    X_PHYSFONT    font;
    X_PHYSPEN     pen;
    X_PHYSBRUSH   brush;
    int           backgroundPixel;
    int           textPixel;
    int           exposures;   /* count of graphics exposures operations */
    int           current_pf;
    XRENDERINFO   xrender;
} X11DRV_PDEVICE;


  /* GCs used for B&W and color bitmap operations */
extern GC BITMAP_monoGC, BITMAP_colorGC;

#define BITMAP_GC(bmp) \
  (((bmp)->bitmap.bmBitsPixel == 1) ? BITMAP_monoGC : BITMAP_colorGC)

#define DEPTH_FROM_BPP(bpp) (((bpp)==(32))?(24):(bpp))

extern unsigned int X11DRV_server_startticks;
extern Time X11DRV_event_startticks;
extern BOOL X11DRV_event_startticks_initialized;

static inline unsigned int X11DRV_event_time(Time evtime)
{
  DWORD ticks = GetTickCount();
  DWORD ret = evtime - X11DRV_event_startticks;

  if (evtime == CurrentTime)
     return ticks;

  /* this will "calibrate" X11DRV_event_startticks to approach
   * the actual X server time as events are received, without
   * ever returning a Win32 event time in the future. */
  if ((!X11DRV_event_startticks_initialized) || ((INT)(ret - ticks)) > 0) {
    X11DRV_event_startticks = evtime - ticks;
    /* FIXME */
    X11DRV_event_startticks_initialized = TRUE;
    ret = ticks;
  }
  return ret;
}

/* Wine driver X11 functions */

extern BOOL X11DRV_BitBlt( struct tagDC *dcDst, INT xDst, INT yDst,
                             INT width, INT height, struct tagDC *dcSrc,
                             INT xSrc, INT ySrc, DWORD rop );
extern BOOL X11DRV_GetDCOrgEx( struct tagDC *dc, LPPOINT lpp );
extern BOOL X11DRV_PatBlt( struct tagDC *dc, INT left, INT top,
                             INT width, INT height, DWORD rop );
extern VOID   X11DRV_SetDeviceClipping(struct tagDC *dc);
extern BOOL X11DRV_StretchBlt( struct tagDC *dcDst, INT xDst, INT yDst,
                                 INT widthDst, INT heightDst,
                                 struct tagDC *dcSrc, INT xSrc, INT ySrc,
                                 INT widthSrc, INT heightSrc, DWORD rop );
extern BOOL X11DRV_LineTo( struct tagDC *dc, INT x, INT y);
extern BOOL X11DRV_Arc( struct tagDC *dc, INT left, INT top, INT right,
			  INT bottom, INT xstart, INT ystart, INT xend,
			  INT yend );
extern BOOL X11DRV_Pie( struct tagDC *dc, INT left, INT top, INT right,
			  INT bottom, INT xstart, INT ystart, INT xend,
			  INT yend );
extern BOOL X11DRV_Chord( struct tagDC *dc, INT left, INT top,
			    INT right, INT bottom, INT xstart,
			    INT ystart, INT xend, INT yend );
extern BOOL X11DRV_Ellipse( struct tagDC *dc, INT left, INT top,
			      INT right, INT bottom );
extern BOOL X11DRV_Rectangle(struct tagDC *dc, INT left, INT top,
			      INT right, INT bottom);
extern BOOL X11DRV_RoundRect( struct tagDC *dc, INT left, INT top,
				INT right, INT bottom, INT ell_width,
				INT ell_height );
extern COLORREF X11DRV_SetPixel( struct tagDC *dc, INT x, INT y,
				 COLORREF color );
extern COLORREF X11DRV_GetPixel( struct tagDC *dc, INT x, INT y);
extern BOOL X11DRV_PaintRgn( struct tagDC *dc, HRGN hrgn );
extern BOOL X11DRV_Polyline( struct tagDC *dc,const POINT* pt,INT count);
extern BOOL X11DRV_Polygon( struct tagDC *dc, const POINT* pt, INT count );
extern BOOL X11DRV_PolyPolygon( struct tagDC *dc, const POINT* pt,
				  const INT* counts, UINT polygons);
extern BOOL X11DRV_PolyPolyline( struct tagDC *dc, const POINT* pt,
				  const DWORD* counts, DWORD polylines);

extern HGDIOBJ X11DRV_SelectObject( struct tagDC *dc, HGDIOBJ handle );

extern COLORREF X11DRV_SetBkColor( struct tagDC *dc, COLORREF color );
extern COLORREF X11DRV_SetTextColor( struct tagDC *dc, COLORREF color );
extern BOOL X11DRV_ExtFloodFill( struct tagDC *dc, INT x, INT y,
				   COLORREF color, UINT fillType );
extern BOOL X11DRV_CreateBitmap( HBITMAP hbitmap );
extern BOOL X11DRV_DeleteObject( HGDIOBJ handle );
extern LONG X11DRV_BitmapBits( HBITMAP hbitmap, void *bits, LONG count,
			       WORD flags );
extern INT X11DRV_SetDIBitsToDevice( struct tagDC *dc, INT xDest,
				       INT yDest, DWORD cx, DWORD cy,
				       INT xSrc, INT ySrc,
				       UINT startscan, UINT lines,
				       LPCVOID bits, const BITMAPINFO *info,
				       UINT coloruse );
extern INT X11DRV_DeviceBitmapBits( struct tagDC *dc, HBITMAP hbitmap,
				      WORD fGet, UINT startscan,
				      UINT lines, LPSTR bits,
				      LPBITMAPINFO info, UINT coloruse );
extern BOOL X11DRV_GetDeviceGammaRamp( struct tagDC *dc, LPVOID ramp );
extern BOOL X11DRV_SetDeviceGammaRamp( struct tagDC *dc, LPVOID ramp );

/* OpenGL / X11 driver functions */
extern int X11DRV_ChoosePixelFormat(DC *dc, const PIXELFORMATDESCRIPTOR *pppfd) ;
extern int X11DRV_DescribePixelFormat(DC *dc, int iPixelFormat, UINT nBytes, PIXELFORMATDESCRIPTOR *ppfd) ;
extern int X11DRV_GetPixelFormat(DC *dc) ;
extern BOOL X11DRV_SetPixelFormat(DC *dc, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd) ;
extern BOOL X11DRV_SwapBuffers(DC *dc) ;

/* X11 driver internal functions */

extern BOOL X11DRV_BITMAP_Init(void);
extern HBRUSH X11DRV_BRUSH_SelectObject( DC * dc, HBRUSH hbrush );
extern HFONT X11DRV_FONTInt_SelectObject( DC *dc, HFONT hfont );
extern HPEN X11DRV_PEN_SelectObject( DC * dc, HPEN hpen );
extern HBITMAP X11DRV_BITMAP_SelectObject( DC * dc, HBITMAP hbitmap );
extern BOOL X11DRV_BITMAP_DeleteObject( HBITMAP hbitmap );

struct tagBITMAPOBJ;
extern XImage *X11DRV_BITMAP_GetXImage( const struct tagBITMAPOBJ *bmp );
extern XImage *X11DRV_DIB_CreateXImage( int width, int height, int depth );
extern HBITMAP X11DRV_BITMAP_CreateBitmapHeaderFromPixmap(Pixmap pixmap);
extern HGLOBAL X11DRV_DIB_CreateDIBFromPixmap(Pixmap pixmap, HDC hdc, BOOL bDeletePixmap);
extern HBITMAP X11DRV_BITMAP_CreateBitmapFromPixmap(Pixmap pixmap, BOOL bDeletePixmap);
extern Pixmap X11DRV_DIB_CreatePixmapFromDIB( HGLOBAL hPackedDIB, HDC hdc );
extern Pixmap X11DRV_BITMAP_CreatePixmapFromBitmap( HBITMAP hBmp, HDC hdc );

extern void X11DRV_SetDrawable( HDC hdc, Drawable drawable, int mode, int org_x, int org_y );
extern void X11DRV_StartGraphicsExposures( HDC hdc );
extern void X11DRV_EndGraphicsExposures( HDC hdc, HRGN hrgn );

extern BOOL X11DRV_SetupGCForPatBlt( struct tagDC *dc, GC gc, BOOL fMapColors );
extern BOOL X11DRV_SetupGCForBrush( struct tagDC *dc );
extern BOOL X11DRV_SetupGCForPen( struct tagDC *dc );
extern BOOL X11DRV_SetupGCForText( struct tagDC *dc );

extern const int X11DRV_XROPfunction[];

extern void _XInitImageFuncPtrs(XImage *);

/* Font - XFont */

extern int X11DRV_FONT_Init( int *log_pixels_x, int *log_pixels_y );
extern BOOL X11DRV_EnumDeviceFonts( HDC hdc, LPLOGFONTW plf,
                                         DEVICEFONTENUMPROC proc, LPARAM lp );
extern BOOL X11DRV_ExtTextOut( DC *dc, INT x, INT y, UINT flags,
                                    const RECT *lprect, LPCWSTR wstr,
                                    UINT count, const INT *lpDx,
                                    BOOL antialias );
extern BOOL X11DRV_GetCharWidth( DC *dc, UINT firstChar, UINT lastChar,
                                      LPINT buffer );
extern BOOL X11DRV_GetTextExtentPoint( DC *dc, LPCWSTR str, INT count,
                                            LPSIZE size );
extern BOOL X11DRV_GetTextMetrics(DC *dc, TEXTMETRICW *metrics);
extern HFONT X11DRV_FONT_SelectObject( DC * dc, HFONT hfont );

/* XRender */

extern BOOL X11DRV_XRender_Installed;
extern BOOL X11DRV_XRender_Cursor;
extern BOOL X11DRV_XRender_Init(void);
extern void X11DRV_XRender_Finalize(void);
extern BOOL X11DRV_XRender_SelectFont(struct tagDC*, HFONT);
extern void X11DRV_XRender_DeleteDC(struct tagDC*);
extern BOOL X11DRV_XRender_ExtTextOut(DC *dc, INT x, INT y, UINT flags,
				      const RECT *lprect, LPCWSTR wstr,
				      UINT count, const INT *lpDx,
				      BOOL antialias);
extern void X11DRV_XRender_UpdateDrawable(DC *dc);
extern Cursor X11DRV_XRender_CreateCursor( Display *display, struct tagCURSORICONINFO *ptr );

/* exported dib functions for now */

#ifdef HAVE_LIBXXSHM
/* structure for use with non-depth compatiable DIB section,
 * shared pixmap with _extra_ shared memory segment */
typedef struct
{
    XShmSegmentInfo shminfo;
    Pixmap sharedPixmap;
    int size;
} X11DRV_DIBSECTION_NDPIXMAP;
#endif

/* Additional info for DIB section objects */
typedef struct
{
    /* Windows DIB section */
    DIBSECTIONOBJ  dibSection;

    /* backpointer to the owning bitmap */
    HBITMAP bmp;

    /* Mapping status */
    int         status, p_status;

    /* Color map info */
    int         nColorMap;
    int        *colorMap;

    /* Cached XImage */
    XImage     *image;

#ifdef HAVE_LIBXXSHM
    /* Possibly a Pixmap as well */
    Pixmap     sharedPixmap;

    /* Shared memory segment info */
    XShmSegmentInfo shminfo;

    /* use with dibsection when depth does not match that of
     * the screen */
    X11DRV_DIBSECTION_NDPIXMAP *shmNdPixmap;
#endif

    /* OpenGL pixel format */
    int current_pf;

    /* Aux buffer access function */
    void (*copy_aux)(struct tagBITMAPOBJ *bmp, void*ctx, int req);
    BOOL (*write_aux)(struct tagBITMAPOBJ *bmp, void*ctx, struct tagDC *dc, INT x, INT y, UINT flags, const RECT *lprect, LPCWSTR wstr, UINT count, const INT *lpDx, BOOL antialias);
    void *aux_ctx;

    /* GDI access lock */
    CRITICAL_SECTION lock;
    
    /* dib type specific functions */
    void (*coerce_dib)( struct tagBITMAPOBJ *bmp, INT req, BOOL lossy );
    void (*unlock_dib)( struct tagBITMAPOBJ *bmp, BOOL commit );

} X11DRV_DIBSECTION;

extern int *X11DRV_DIB_BuildColorMap( struct tagDC *dc, WORD coloruse,
				      WORD depth, const BITMAPINFO *info,
				      int *nColors );
extern INT X11DRV_CoerceDIBSection(struct tagDC *dc,INT,BOOL);
extern INT X11DRV_LockDIBSection(struct tagDC *dc,INT req,BOOL lossy, BOOL readonly);
extern void X11DRV_UnlockDIBSection(struct tagDC *dc,BOOL);
extern INT X11DRV_CoerceDIBSection2(HBITMAP bmp,INT,BOOL);
extern INT X11DRV_LockDIBSection2(HBITMAP bmp,INT,BOOL);
extern void X11DRV_UnlockDIBSection2(HBITMAP bmp,BOOL);

extern HBITMAP X11DRV_DIB_CreateDIBSection(struct tagDC *dc, BITMAPINFO *bmi, UINT usage,
					   LPVOID *bits, HANDLE section, DWORD offset, DWORD ovr_pitch);

extern struct tagBITMAP_DRIVER X11DRV_BITMAP_Driver;

extern INT X11DRV_DIB_SetDIBits(struct tagBITMAPOBJ *bmp, struct tagDC *dc, UINT startscan,
				UINT lines, LPCVOID bits, const BITMAPINFO *info,
				UINT coloruse, HBITMAP hbitmap);
extern INT X11DRV_DIB_GetDIBits(struct tagBITMAPOBJ *bmp, struct tagDC *dc, UINT startscan,
				UINT lines, LPVOID bits, BITMAPINFO *info,
				UINT coloruse, HBITMAP hbitmap);
extern void X11DRV_DIB_DeleteDIBSection(struct tagBITMAPOBJ *bmp);
extern UINT X11DRV_DIB_SetDIBColorTable(struct tagBITMAPOBJ *,struct tagDC*,UINT,UINT,const RGBQUAD *);
extern UINT X11DRV_DIB_GetDIBColorTable(struct tagBITMAPOBJ *,struct tagDC*,UINT,UINT,RGBQUAD *);
extern INT X11DRV_DIB_Coerce(struct tagBITMAPOBJ *,INT,BOOL);
extern INT X11DRV_DIB_Lock(struct tagBITMAPOBJ *,INT,BOOL);
extern void X11DRV_DIB_Unlock(struct tagBITMAPOBJ *,BOOL);
void X11DRV_DIB_CopyDIBSection(DC *dcSrc, DC *dcDst,
			       DWORD xSrc, DWORD ySrc,
			       DWORD xDest, DWORD yDest,
			       DWORD width, DWORD height);
void X11DRV_DIB_DrawDIBSection(struct tagBITMAPOBJ *bmp, Drawable dest,
			       DWORD xSrc, DWORD ySrc,
			       DWORD xDest, DWORD yDest,
			       DWORD width, DWORD height);
struct _DCICMD;
extern INT X11DRV_DCICommand(INT cbInput, const struct _DCICMD *lpCmd, LPVOID lpOutData);

/**************************************************************************
 * X11 GDI driver
 */

#define X11DRV_ESCAPE 6789 /* compatibility with the LGPL-ed x11drv */
enum x11drv_escape_codes
{
  X11DRV_GET_DISPLAY,
  X11DRV_GET_DRAWABLE,
  X11DRV_GET_FONT,
  X11DRV_LOCK_BITMAP,
  X11DRV_UNLOCK_BITMAP
};

BOOL X11DRV_GDI_Initialize( Display *display );
void X11DRV_GDI_Finalize(void);

extern Display *gdi_display;  /* display to use for all GDI functions */
extern XVisualInfo *visual_list; /* visuals available for OpenGL */
extern int visual_count;

/* X11 GDI palette driver */

#define X11DRV_PALETTE_FIXED    0x0001 /* read-only colormap - have to use XAllocColor (if not virtual) */
#define X11DRV_PALETTE_VIRTUAL  0x0002 /* no mapping needed - pixel == pixel color */

#define X11DRV_PALETTE_PRIVATE  0x1000 /* private colormap, identity mapping */
#define X11DRV_PALETTE_WHITESET 0x2000

extern Colormap X11DRV_PALETTE_PaletteXColormap;
extern UINT16 X11DRV_PALETTE_PaletteFlags;

extern int *X11DRV_PALETTE_PaletteToXPixel;
extern int *X11DRV_PALETTE_XPixelToPalette;

extern int X11DRV_PALETTE_mapEGAPixel[16];

extern int X11DRV_PALETTE_Init(void);
extern void X11DRV_PALETTE_Cleanup(void);

extern COLORREF X11DRV_PALETTE_ToLogical(int pixel);
extern int X11DRV_PALETTE_ToPhysical(struct tagDC *dc, COLORREF color);

extern struct tagPALETTE_DRIVER X11DRV_PALETTE_Driver;

extern int X11DRV_PALETTE_SetMapping(struct tagPALETTEOBJ *palPtr, UINT uStart, UINT uNum, BOOL mapOnly);
extern int X11DRV_PALETTE_UpdateMapping(struct tagPALETTEOBJ *palPtr);
extern BOOL X11DRV_PALETTE_IsDark(int pixel);

/**************************************************************************
 * X11 USER driver
 */

struct x11drv_thread_data
{
    Display *display;
    HANDLE   display_fd;
    int      process_event_count;  /* recursion count for event processing */
};

extern struct x11drv_thread_data *x11drv_init_thread_data(void);

inline static struct x11drv_thread_data *x11drv_thread_data(void)
{
    struct x11drv_thread_data *data = NtCurrentTeb()->driver_data;
    if (!data) data = x11drv_init_thread_data();
    return data;
}

inline static Display *thread_display(void) { return x11drv_thread_data()->display; }

extern Visual *visual;
extern Window root_window;
extern unsigned int default_screen_width;
extern unsigned int default_screen_height;
extern unsigned int default_screen_bpp;
extern unsigned int screen_width;
extern unsigned int screen_height;
extern unsigned int screen_depth;
extern unsigned int screen_bpp;

extern Atom wmProtocols;
extern Atom wmDeleteWindow;
extern Atom wmTakeFocus;
extern Atom dndProtocol;
extern Atom dndSelection;
extern Atom wmChangeState;
extern Atom kwmDockWindow;
extern Atom _kde_net_wm_system_tray_window_for;

/* X11 clipboard driver */

extern void X11DRV_CLIPBOARD_FreeResources( Atom property );
extern BOOL X11DRV_CLIPBOARD_RegisterPixmapResource( Atom property, Pixmap pixmap );
extern BOOL X11DRV_CLIPBOARD_IsNativeProperty(Atom prop);
extern UINT X11DRV_CLIPBOARD_MapPropertyToFormat(char *itemFmtName);
extern Atom X11DRV_CLIPBOARD_MapFormatToProperty(UINT id);
extern void X11DRV_CLIPBOARD_ReleaseSelection(Atom selType, Window w, HWND hwnd);
extern BOOL X11DRV_IsSelectionOwner(void);
extern BOOL X11DRV_GetClipboardData(UINT wFormat);

/* X11 event driver */

typedef enum {
  X11DRV_INPUT_RELATIVE,
  X11DRV_INPUT_ABSOLUTE
} INPUT_TYPE;
extern INPUT_TYPE X11DRV_EVENT_SetInputMethod(INPUT_TYPE type);

#ifdef HAVE_LIBXXF86DGA2
void X11DRV_EVENT_SetDGAStatus(HWND hwnd, int event_base) ;
#endif

/* x11drv private window data */
struct x11drv_win_data
{
    Window  whole_window;   /* X window for the complete window */
    Window  client_window;  /* X window for the client area */
    Window  icon_window;    /* X window for the icon */
    RECT    whole_rect;     /* X window rectangle for the whole window relative to parent */
    RECT    client_rect;    /* client area relative to whole window */
    HBITMAP hWMIconBitmap;
    HBITMAP hWMIconMask;
    int exclusive_fullscreen; /* non-zero indicates the window is fullscreen exclusive */
};

typedef struct x11drv_win_data X11DRV_WND_DATA;

extern Window X11DRV_get_client_window( HWND hwnd );
extern Window X11DRV_get_whole_window( HWND hwnd );

inline static Window get_client_window( WND *wnd )
{
    struct x11drv_win_data *data = wnd->pDriverData;
    return data->client_window;
}

inline static Window get_whole_window( WND *wnd )
{
    struct x11drv_win_data *data = wnd->pDriverData;
    return data->whole_window;
}

extern void X11DRV_SetFocus( HWND hwnd );
extern Cursor X11DRV_GetCursor( Display *display, struct tagCURSORICONINFO *ptr );

typedef int (*X11DRV_error_handler)( Display *display, XErrorEvent *event, void *arg );
extern int X11DRV_expect_any( Display *display, XErrorEvent *event, void *arg );
extern void X11DRV_expect_error( Display *display, X11DRV_error_handler handler, void *arg );
extern int X11DRV_check_error(void);
extern void X11DRV_register_window( Display *display, HWND hwnd, struct x11drv_win_data *data );
extern void X11DRV_set_iconic_state( WND *win, BOOL client );
extern void X11DRV_window_to_X_rect( WND *win, RECT *rect );
extern void X11DRV_X_to_window_rect( WND *win, RECT *rect );
extern void X11DRV_create_desktop_thread(void);
extern Window X11DRV_create_desktop( XVisualInfo *desktop_vi, const char *geometry );
extern int X11DRV_sync_whole_window_position( Display *display, WND *win, int zorder );
extern int X11DRV_sync_client_window_position( Display *display, WND *win );
extern BOOL X11DRV_ChangeDisplayMode(LPDEVMODEA devmode);

#endif  /* __WINE_X11DRV_H */
