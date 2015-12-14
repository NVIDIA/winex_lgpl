#ifndef _UXTHEME_H_
#define _UXTHEME_H_

#include <commctrl.h>
#include <specstrings.h>

#ifndef THEMEAPI
#if !defined(_UXTHEME_)
#define THEMEAPI          EXTERN_C DECLSPEC_IMPORT HRESULT STDAPICALLTYPE
#define THEMEAPI_(type)   EXTERN_C DECLSPEC_IMPORT type STDAPICALLTYPE
#else
#define THEMEAPI          STDAPI
#define THEMEAPI_(type)   STDAPI_(type)
#endif
#endif

typedef HANDLE HTHEME;

#if (_WIN32_WINNT >= 0x0600)
#define MAX_THEMECOLOR  64
#define MAX_THEMESIZE   64
#endif

#if (NTDDI_VERSION>= NTDDI_WIN7)

BOOL WINAPI
BeginPanningFeedback(
__in HWND hwnd);

BOOL WINAPI
UpdatePanningFeedback(
	__in HWND hwnd,
	__in LONG lTotalOverpanOffsetX,
	__in LONG lTotalOverpanOffsetY,
	__in BOOL fInInertia);

BOOL WINAPI
EndPanningFeedback(
	__in HWND hwnd,
	__in BOOL fAnimateBack);
#endif

THEMEAPI_(HTHEME)
OpenThemeData(
    HWND hwnd,
    LPCWSTR pszClassList
    );

#define OTD_FORCE_RECT_SIZING   0x00000001
#define OTD_NONCLIENT           0x00000002
#define OTD_VALIDBITS           (OTD_FORCE_RECT_SIZING | \
                                 OTD_NONCLIENT)

THEMEAPI_(HTHEME)
OpenThemeDataEx(
    HWND hwnd,
    LPCWSTR pszClassList,
    DWORD dwFlags
    );

THEMEAPI
CloseThemeData(
    HTHEME hTheme
    );

THEMEAPI
DrawThemeBackground(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    __in_opt LPCRECT pClipRect
    );

#define DTBG_CLIPRECT           0x00000001
#define DTBG_DRAWSOLID          0x00000002
#define DTBG_OMITBORDER         0x00000004
#define DTBG_OMITCONTENT        0x00000008
#define DTBG_COMPUTINGREGION    0x00000010
#define DTBG_MIRRORDC           0x00000020

#define DTBG_NOMIRROR           0x00000040
#define DTBG_VALIDBITS          (DTBG_CLIPRECT | \
                                 DTBG_DRAWSOLID | \
                                 DTBG_OMITBORDER | \
                                 DTBG_OMITCONTENT | \
                                 DTBG_COMPUTINGREGION | \
                                 DTBG_MIRRORDC | \
                                 DTBG_NOMIRROR)

typedef struct _DTBGOPTS
{
    DWORD dwSize;
    DWORD dwFlags;
    RECT rcClip;
} DTBGOPTS, *PDTBGOPTS;

THEMEAPI
DrawThemeBackgroundEx(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    __in_opt const DTBGOPTS *pOptions
    );

#define DTT_GRAYED              0x00000001
#define DTT_FLAGS2VALIDBITS     (DTT_GRAYED)

THEMEAPI
DrawThemeText(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __in_ecount(cchText) LPCWSTR pszText,
    int cchText,
    DWORD dwTextFlags,
    DWORD dwTextFlags2,
    LPCRECT pRect
    );

THEMEAPI
GetThemeBackgroundContentRect(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pBoundingRect,
    __out LPRECT pContentRect
    );

THEMEAPI
GetThemeBackgroundExtent(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pContentRect,
    __out LPRECT pExtentRect
    );

THEMEAPI
GetThemeBackgroundRegion(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    __out HRGN *pRegion
    );

enum THEMESIZE
{
    TS_MIN,
    TS_TRUE,
    TS_DRAW
};

THEMEAPI
GetThemePartSize(
    HTHEME hTheme,
    __in_opt HDC hdc,
    int iPartId,
    int iStateId,
    __in_opt LPCRECT prc,
    enum THEMESIZE eSize,
    __out SIZE *psz
    );

THEMEAPI
GetThemeTextExtent(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __in_ecount(cchCharCount) LPCWSTR pszText,
    int cchCharCount,
    DWORD dwTextFlags,
    __in_opt LPCRECT pBoundingRect,
    __out LPRECT pExtentRect
    );

THEMEAPI
GetThemeTextMetrics(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __out TEXTMETRICW *ptm
    );

#define HTTB_BACKGROUNDSEG          0x00000000

#define HTTB_FIXEDBORDER            0x00000002

#define HTTB_CAPTION                0x00000004

#define HTTB_RESIZINGBORDER_LEFT    0x00000010
#define HTTB_RESIZINGBORDER_TOP     0x00000020
#define HTTB_RESIZINGBORDER_RIGHT   0x00000040
#define HTTB_RESIZINGBORDER_BOTTOM  0x00000080
#define HTTB_RESIZINGBORDER         (HTTB_RESIZINGBORDER_LEFT | \
                                     HTTB_RESIZINGBORDER_TOP | \
                                     HTTB_RESIZINGBORDER_RIGHT | \
                                     HTTB_RESIZINGBORDER_BOTTOM)

#define HTTB_SIZINGTEMPLATE         0x00000100

#define HTTB_SYSTEMSIZINGMARGINS    0x00000200

THEMEAPI
HitTestThemeBackground(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    DWORD dwOptions,
    LPCRECT pRect,
    HRGN hrgn,
    POINT ptTest,
    __out WORD *pwHitTestCode
    );

THEMEAPI
DrawThemeEdge(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pDestRect,
    UINT uEdge,
    UINT uFlags,
    __out_opt LPRECT pContentRect
    );

THEMEAPI
DrawThemeIcon(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    HIMAGELIST himl,
    int iImageIndex
    );

THEMEAPI_(BOOL)
IsThemePartDefined(
    HTHEME hTheme,
    int iPartId,
    int iStateId
    );

THEMEAPI_(BOOL)
IsThemeBackgroundPartiallyTransparent(
    HTHEME hTheme,
    int iPartId,
    int iStateId
    );

THEMEAPI
GetThemeColor(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out COLORREF *pColor
    );

THEMEAPI
GetThemeMetric(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    int iPropId,
    __out int *piVal
    );

THEMEAPI
GetThemeString(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out_ecount(cchMaxBuffChars) LPWSTR pszBuff,
    int cchMaxBuffChars
    );

THEMEAPI
GetThemeBool(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out BOOL *pfVal
    );

THEMEAPI
GetThemeInt(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out int *piVal
    );

THEMEAPI
GetThemeEnumValue(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out int *piVal
    );

THEMEAPI
GetThemePosition(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out POINT *pPoint
    );

THEMEAPI
GetThemeFont(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    int iPropId,
    __out LOGFONTW *pFont
    );

THEMEAPI
GetThemeRect(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out LPRECT pRect
    );

typedef struct _MARGINS
{
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
} MARGINS, *PMARGINS;

THEMEAPI
GetThemeMargins(
    HTHEME hTheme,
    __in_opt HDC hdc,
    int iPartId,
    int iStateId,
    int iPropId,
    __in_opt LPCRECT prc,
    __out MARGINS *pMargins
    );

#if (_WIN32_WINNT >= 0x0600)
#define MAX_INTLIST_COUNT 402
#else
#define MAX_INTLIST_COUNT 10
#endif

typedef struct _INTLIST
{
    int iValueCount;
    int iValues[MAX_INTLIST_COUNT];
} INTLIST, *PINTLIST;

THEMEAPI
GetThemeIntList(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out INTLIST *pIntList
    );

enum PROPERTYORIGIN
{
    PO_STATE,
    PO_PART,
    PO_CLASS,
    PO_GLOBAL,
    PO_NOTFOUND
};

THEMEAPI
GetThemePropertyOrigin(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out enum PROPERTYORIGIN *pOrigin
    );

THEMEAPI
SetWindowTheme(
    HWND hwnd,
    LPCWSTR pszSubAppName,
    LPCWSTR pszSubIdList
    );

THEMEAPI
GetThemeFilename(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out_ecount(cchMaxBuffChars) LPWSTR pszThemeFileName,
    int cchMaxBuffChars
    );

THEMEAPI_(COLORREF)
GetThemeSysColor(
    HTHEME hTheme,
    int iColorId
    );

THEMEAPI_(HBRUSH)
GetThemeSysColorBrush(
    HTHEME hTheme,
    int iColorId
    );

THEMEAPI_(BOOL)
GetThemeSysBool(
    HTHEME hTheme,
    int iBoolId
    );

THEMEAPI_(int)
GetThemeSysSize(
    HTHEME hTheme,
    int iSizeId
    );

THEMEAPI
GetThemeSysFont(
    HTHEME hTheme,
    int iFontId,
    __out LOGFONTW *plf
    );

THEMEAPI
GetThemeSysString(
    HTHEME hTheme,
    int iStringId,
    __out_ecount(cchMaxStringChars) LPWSTR pszStringBuff,
    int cchMaxStringChars
    );

THEMEAPI
GetThemeSysInt(
    HTHEME hTheme,
    int iIntId,
    __out int *piValue
    );

THEMEAPI_(BOOL)
IsThemeActive(
    VOID
    );

THEMEAPI_(BOOL)
IsAppThemed(
    VOID
    );

THEMEAPI_(HTHEME)
GetWindowTheme(
    HWND hwnd
    );

#define ETDT_DISABLE                    0x00000001
#define ETDT_ENABLE                     0x00000002
#define ETDT_USETABTEXTURE              0x00000004

#define ETDT_ENABLETAB              (ETDT_ENABLE | \
                                     ETDT_USETABTEXTURE)

#if (_WIN32_WINNT >= 0x0600)
#define ETDT_USEAEROWIZARDTABTEXTURE    0x00000008

#define ETDT_ENABLEAEROWIZARDTAB    (ETDT_ENABLE | \
                                     ETDT_USEAEROWIZARDTABTEXTURE)

#define ETDT_VALIDBITS              (ETDT_DISABLE | \
                                     ETDT_ENABLE | \
                                     ETDT_USETABTEXTURE | \
                                     ETDT_USEAEROWIZARDTABTEXTURE)
#endif

THEMEAPI
EnableThemeDialogTexture(
    __in HWND hwnd,
    __in DWORD dwFlags
    );

THEMEAPI_(BOOL)
IsThemeDialogTextureEnabled(
    __in HWND hwnd
    );

#define STAP_ALLOW_NONCLIENT    (1UL << 0)
#define STAP_ALLOW_CONTROLS     (1UL << 1)
#define STAP_ALLOW_WEBCONTENT   (1UL << 2)
#define STAP_VALIDBITS          (STAP_ALLOW_NONCLIENT | \
                                 STAP_ALLOW_CONTROLS | \
                                 STAP_ALLOW_WEBCONTENT)

THEMEAPI_(DWORD)
GetThemeAppProperties(
    VOID
    );

THEMEAPI_(void)
SetThemeAppProperties(
    DWORD dwFlags
    );

THEMEAPI GetCurrentThemeName(
    __out_ecount(cchMaxNameChars) LPWSTR pszThemeFileName,
    int cchMaxNameChars,
    __out_ecount_opt(cchMaxColorChars) LPWSTR pszColorBuff,
    int cchMaxColorChars,
    __out_ecount_opt(cchMaxSizeChars) LPWSTR pszSizeBuff,
    int cchMaxSizeChars
    );

#define SZ_THDOCPROP_DISPLAYNAME    L"DisplayName"
#define SZ_THDOCPROP_CANONICALNAME  L"ThemeName"
#define SZ_THDOCPROP_TOOLTIP        L"ToolTip"
#define SZ_THDOCPROP_AUTHOR         L"author"

THEMEAPI
GetThemeDocumentationProperty(
    LPCWSTR pszThemeName,
    LPCWSTR pszPropertyName,
    __out_ecount(cchMaxValChars) LPWSTR pszValueBuff,
    int cchMaxValChars
    );

THEMEAPI
DrawThemeParentBackground(
    HWND hwnd,
    HDC hdc,
    __in_opt const RECT* prc
    );

THEMEAPI
EnableTheming(
    BOOL fEnable
    );

#define GBF_DIRECT      0x00000001
#define GBF_COPY        0x00000002
#define GBF_VALIDBITS   (GBF_DIRECT | \
                         GBF_COPY)

#if (_WIN32_WINNT >= 0x0600)

#define DTPB_WINDOWDC           0x00000001
#define DTPB_USECTLCOLORSTATIC  0x00000002
#define DTPB_USEERASEBKGND      0x00000004

THEMEAPI
DrawThemeParentBackgroundEx(
    HWND hwnd,
    HDC hdc,
    DWORD dwFlags,
    __in_opt const RECT* prc
    );

enum WINDOWTHEMEATTRIBUTETYPE
{
    WTA_NONCLIENT = 1
};

typedef struct _WTA_OPTIONS
{
    DWORD dwFlags;
    DWORD dwMask;

} WTA_OPTIONS, *PWTA_OPTIONS;

#define WTNCA_NODRAWCAPTION       0x00000001
#define WTNCA_NODRAWICON          0x00000002
#define WTNCA_NOSYSMENU           0x00000004
#define WTNCA_NOMIRRORHELP        0x00000008
#define WTNCA_VALIDBITS           (WTNCA_NODRAWCAPTION | \
                                   WTNCA_NODRAWICON | \
                                   WTNCA_NOSYSMENU | \
                                   WTNCA_NOMIRRORHELP)

THEMEAPI
SetWindowThemeAttribute(
    HWND hwnd,
    enum WINDOWTHEMEATTRIBUTETYPE eAttribute,
    __in_bcount(cbAttribute) PVOID pvAttribute,
    DWORD cbAttribute
    );

__inline HRESULT SetWindowThemeNonClientAttributes(HWND hwnd, DWORD dwMask, DWORD dwAttributes)
{
    WTA_OPTIONS wta;
    wta.dwFlags = dwAttributes;
    wta.dwMask = dwMask;
    return SetWindowThemeAttribute(hwnd, WTA_NONCLIENT, (void*)&(wta), sizeof(wta));
}

#endif

typedef
int
(WINAPI *DTT_CALLBACK_PROC)
(
    __in HDC hdc,
    __inout_ecount(cchText) LPWSTR pszText,
    __in int cchText,
    __inout LPRECT prc,
    __in UINT dwFlags,
    __in LPARAM lParam);

#define DTT_TEXTCOLOR       (1UL << 0)
#define DTT_BORDERCOLOR     (1UL << 1)
#define DTT_SHADOWCOLOR     (1UL << 2)
#define DTT_SHADOWTYPE      (1UL << 3)
#define DTT_SHADOWOFFSET    (1UL << 4)
#define DTT_BORDERSIZE      (1UL << 5)
#define DTT_FONTPROP        (1UL << 6)
#define DTT_COLORPROP       (1UL << 7)
#define DTT_STATEID         (1UL << 8)
#define DTT_CALCRECT        (1UL << 9)
#define DTT_APPLYOVERLAY    (1UL << 10)
#define DTT_GLOWSIZE        (1UL << 11)
#define DTT_CALLBACK        (1UL << 12)
#define DTT_COMPOSITED      (1UL << 13)
#define DTT_VALIDBITS       (DTT_TEXTCOLOR | \
                             DTT_BORDERCOLOR | \
                             DTT_SHADOWCOLOR | \
                             DTT_SHADOWTYPE | \
                             DTT_SHADOWOFFSET | \
                             DTT_BORDERSIZE | \
                             DTT_FONTPROP | \
                             DTT_COLORPROP | \
                             DTT_STATEID | \
                             DTT_CALCRECT | \
                             DTT_APPLYOVERLAY | \
                             DTT_GLOWSIZE | \
                             DTT_COMPOSITED)

typedef struct _DTTOPTS
{
    DWORD             dwSize;
    DWORD             dwFlags;
    COLORREF          crText;
    COLORREF          crBorder;
    COLORREF          crShadow;
    int               iTextShadowType;
    POINT             ptShadowOffset;
    int               iBorderSize;
    int               iFontPropId;
    int               iColorPropId;
    int               iStateId;
    BOOL              fApplyOverlay;
    int               iGlowSize;
    DTT_CALLBACK_PROC pfnDrawTextCallback;
    LPARAM            lParam;
} DTTOPTS, *PDTTOPTS;


THEMEAPI
DrawThemeTextEx(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __in_ecount(cchText) LPCWSTR pszText,
    int cchText,
    DWORD dwTextFlags,
    __inout LPRECT pRect,
    __in_opt const DTTOPTS *pOptions
    );

THEMEAPI
GetThemeBitmap(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    ULONG dwFlags,
    __out HBITMAP* phBitmap
    );

THEMEAPI
GetThemeStream(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out VOID **ppvStream,
    __out_opt DWORD *pcbStream,
    __in_opt HINSTANCE hInst
    );

THEMEAPI
BufferedPaintInit(
    VOID
    );

THEMEAPI
BufferedPaintUnInit(
    VOID
    );

typedef HANDLE HPAINTBUFFER;

typedef enum _BP_BUFFERFORMAT
{
    BPBF_COMPATIBLEBITMAP,
    BPBF_DIB,
    BPBF_TOPDOWNDIB,
    BPBF_TOPDOWNMONODIB
} BP_BUFFERFORMAT;

#define BPBF_COMPOSITED BPBF_TOPDOWNDIB

typedef enum _BP_ANIMATIONSTYLE
{
    BPAS_NONE,
    BPAS_LINEAR,
    BPAS_CUBIC,
    BPAS_SINE
} BP_ANIMATIONSTYLE;

typedef struct _BP_ANIMATIONPARAMS
{
    DWORD               cbSize;
    DWORD               dwFlags;
    BP_ANIMATIONSTYLE   style;
    DWORD               dwDuration;
} BP_ANIMATIONPARAMS, *PBP_ANIMATIONPARAMS;

#define BPPF_ERASE               0x0001
#define BPPF_NOCLIP              0x0002
#define BPPF_NONCLIENT           0x0004

typedef struct _BP_PAINTPARAMS
{
    DWORD                       cbSize;
    DWORD                       dwFlags;
    const RECT *                prcExclude;
    const BLENDFUNCTION *       pBlendFunction;
} BP_PAINTPARAMS, *PBP_PAINTPARAMS;

THEMEAPI_(__success(return != NULL) HPAINTBUFFER)
BeginBufferedPaint(
    HDC hdcTarget,
    const RECT* prcTarget,
    BP_BUFFERFORMAT dwFormat,
    __in_opt BP_PAINTPARAMS *pPaintParams,
    __out HDC *phdc
    );

THEMEAPI
EndBufferedPaint(
    HPAINTBUFFER hBufferedPaint,
    BOOL fUpdateTarget
    );

THEMEAPI
GetBufferedPaintTargetRect(
    HPAINTBUFFER hBufferedPaint,
    __out RECT *prc
    );

THEMEAPI_(HDC)
GetBufferedPaintTargetDC(
    HPAINTBUFFER hBufferedPaint
    );

THEMEAPI_(HDC)
GetBufferedPaintDC(
    HPAINTBUFFER hBufferedPaint
    );

THEMEAPI
GetBufferedPaintBits(
    HPAINTBUFFER hBufferedPaint,
    __out RGBQUAD **ppbBuffer,
    __out int *pcxRow
    );

THEMEAPI
BufferedPaintClear(
    HPAINTBUFFER hBufferedPaint,
    __in_opt const RECT *prc
    );

THEMEAPI
BufferedPaintSetAlpha(
    HPAINTBUFFER hBufferedPaint,
    __in_opt const RECT *prc,
    BYTE alpha
    );

#define BufferedPaintMakeOpaque(hBufferedPaint, prc) BufferedPaintSetAlpha(hBufferedPaint, prc, 255)

THEMEAPI
BufferedPaintStopAllAnimations(
    HWND hwnd
    );

typedef HANDLE HANIMATIONBUFFER;

THEMEAPI_(HANIMATIONBUFFER)
BeginBufferedAnimation(
    HWND hwnd,
    HDC hdcTarget,
    const RECT* prcTarget,
    BP_BUFFERFORMAT dwFormat,
    __in_opt BP_PAINTPARAMS *pPaintParams,
    __in BP_ANIMATIONPARAMS *pAnimationParams,
    __out HDC *phdcFrom,
    __out HDC *phdcTo
    );

THEMEAPI
EndBufferedAnimation(
    HANIMATIONBUFFER hbpAnimation,
    BOOL fUpdateTarget
    );

THEMEAPI_(BOOL)
BufferedPaintRenderAnimation(
    HWND hwnd,
    HDC hdcTarget
    );

THEMEAPI_(BOOL) IsCompositionActive();

THEMEAPI
GetThemeTransitionDuration(
    HTHEME hTheme,
    int iPartId,
    int iStateIdFrom,
    int iStateIdTo,
    int iPropId,
    __out DWORD *pdwDuration
    );

#endif
