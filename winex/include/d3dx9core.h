#include "d3dx9.h"

#ifndef __WINEX_D3DX9CORE_H
#define __WINEX_D3DX9CORE_H

#define D3DX_VERSION 0x0900
#define D3DX_SDK_VERSION 9

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI D3DXCheckVersion(UINT D3DSdkVersion, UINT D3DXSdkVersion);
UINT WINAPI D3DXGetDriverLevel(LPDIRECT3DDEVICE9 pDevice);

#ifdef __cplusplus
}
#endif

/***********************************************************************
 * Declare Interfaces
 */

DEFINE_GUID(IID_ID3DXBuffer,            0x932e6a7e,0xc68e,0x45dd,0xa7,0xbf,0x53,0xd1,0x9c,0x86,0xdb,0x1f);
DEFINE_GUID(IID_ID3DXSprite,            0xba0b762d,0x7d28,0x43ec,0xb9,0xdc,0x2f,0x84,0x44,0x3b,0x06,0x14);
DEFINE_GUID(IID_ID3DXFont,              0x4aae6b4d,0xd15f,0x4909,0xb0,0x9f,0x8d,0x6a,0xa3,0x4a,0xc0,0x6b);
DEFINE_GUID(IID_ID3DXRenderToSurface,   0xd014791 ,0x8863,0x4c2c,0xa1,0xc0,0x2 ,0xf3,0xe0,0xc0,0xb6,0x53);
DEFINE_GUID(IID_ID3DXRenderToEnvMap,    0x1561135e,0xbc78,0x495b,0x85,0x86,0x94,0xea,0x53,0x7b,0xd5,0x57);
/*
#if D3DX_SDK_VERSION < 25
DEFINE_GUID(IID_ID3DXLine,              0x72ce4d70,0xcc40,0x4143,0xa8,0x96,0x32,0xe5,0xa ,0xd2,0xef,0x35);
#else
*/
DEFINE_GUID(IID_ID3DXLine,              0xd379ba7f,0x9042,0x4ac4,0x9f,0x5e,0x58,0x19,0x2a,0x4c,0x6b,0xd8);
/* #endif */

typedef struct ID3DXBuffer              ID3DXBuffer, *LPD3DXBUFFER;
typedef struct ID3DXFont                ID3DXFont, *LPD3DXFONT;
typedef struct ID3DXSprite              ID3DXSprite, *LPD3DXSPRITE;
typedef struct ID3DXRenderToSurface     ID3DXRenderToSurface, *LPD3DXRENDERTOSURFACE;
typedef struct ID3DXRenderToEnvMap      ID3DXRenderToEnvMap, *LPD3DXRenderToEnvMap;
typedef struct ID3DXLine                ID3DXLine, *LPD3DXLINE;


/******************************************************************
 * Types and structures
 */
typedef struct _D3DXRTS_DESC
{
    UINT                Width;
    UINT                Height;
    D3DFORMAT           Format;
    BOOL                DepthStencil;
    D3DFORMAT           DepthStencilFormat;

} D3DXRTS_DESC;

typedef struct _D3DXRTE_DESC
{
    UINT        Size;
    UINT        MipLevels;
    D3DFORMAT   Format;
    BOOL        DepthStencil;
    D3DFORMAT   DepthStencilFormat;
} D3DXRTE_DESC;

typedef struct _D3DXFONT_DESCA
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    CHAR FaceName[LF_FACESIZE];

} D3DXFONT_DESCA, *LPD3DXFONT_DESCA;

typedef struct _D3DXFONT_DESCW
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    WCHAR FaceName[LF_FACESIZE];

} D3DXFONT_DESCW, *LPD3DXFONT_DESCW;

#ifdef UNICODE
typedef D3DXFONT_DESCW D3DXFONT_DESC;
typedef LPD3DXFONT_DESCW LPD3DXFONT_DESC;
#else
typedef D3DXFONT_DESCA D3DXFONT_DESC;
typedef LPD3DXFONT_DESCA LPD3DXFONT_DESC;
#endif


/******************************************************************
 * ID3DXBuffer interface
 */
#define ICOM_INTERFACE ID3DXBuffer
#define ID3DXBuffer_METHODS \
    ICOM_METHOD (LPVOID,GetBufferPointer) \
    ICOM_METHOD (DWORD, GetBufferSize)
#define ID3DXBuffer_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXBuffer_METHODS
ICOM_DEFINE(ID3DXBuffer,IUnknown);
#undef ICOM_INTERFACE

/* IUnknown Methods */
#define ID3DXBuffer_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXBuffer_AddRef(p)                 ICOM_CALL (AddRef,p)
#define ID3DXBuffer_Release(p)                ICOM_CALL (Release,p)
/* ID3DXBuffer Methods */
#define ID3DXBuffer_GetBufferPointer(p)       ICOM_CALL (GetBufferPointer,p)
#define ID3DXBuffer_GetBufferSize(p)          ICOM_CALL (GetBufferSize,p)


/******************************************************************
 * ID3DXFont interface
 */
#ifdef UNICODE
#define LOGFONT LOGFONTW
#else
#define LOGFONT LOGFONTA
#endif

#if 0
/* FIXME: This is from the original DX9 SDK */
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD1 (HRESULT,GetLogFont,LOGFONT*,pLogFont) \
    ICOM_METHOD  (HRESULT,Begin) \
    ICOM_METHOD5 (INT,DrawTextA,LPCSTR,pString,INT,Count,LPRECT,pRect,DWORD,Format,D3DCOLOR,Color) \
    ICOM_METHOD5 (INT,DrawTextW,LPCWSTR,pString,INT,Count,LPRECT,pRect,DWORD,Format,D3DCOLOR,Color) \
    ICOM_METHOD  (HRESULT,End) \
    ICOM_METHOD  (HRESULT,OnLostDevice) \
    ICOM_METHOD  (HRESULT,OnResetDevice)
#endif

/* This is from the April 2005 SDK (d3dx9_25.dll) */
#define ICOM_INTERFACE ID3DXFont
#define ID3DXFont_METHODS \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD1 (HRESULT,GetDescA,D3DXFONT_DESCA*,pDesc) \
    ICOM_METHOD1 (HRESULT,GetDescW,D3DXFONT_DESCW*,pDesc) \
    ICOM_METHOD1 (BOOL,GetTexMetricsA,TEXTMETRICA*,pTextMetrics) \
    ICOM_METHOD1 (BOOL,GetTexMetricsW,TEXTMETRICW*,pTextMetrics) \
    ICOM_METHOD  (HDC,GetDC) \
    ICOM_METHOD4 (HRESULT,GetGlyphData,UINT,Glyph,LPDIRECT3DTEXTURE9*,ppTexture,RECT*,pBlackBox,POINT*,pCellInc) \
    ICOM_METHOD2 (HRESULT,PreloadCharacters,UINT,First,UINT,Last) \
    ICOM_METHOD2 (HRESULT,PreloadGlyphs,UINT,First,UINT,Last) \
    ICOM_METHOD2 (HRESULT,PreloadTextA,LPCSTR,pString,INT,Count) \
    ICOM_METHOD2 (HRESULT,PreloadTextW,LPCWSTR,pString,INT,Count) \
    ICOM_METHOD6 (INT,DrawTextA,LPD3DXSPRITE,pSprite,LPCSTR,pString,INT,Count,LPRECT,pRect,DWORD,Format,D3DCOLOR,Color) \
    ICOM_METHOD6 (INT,DrawTextW,LPD3DXSPRITE,pSprite,LPCWSTR,pString,INT,Count,LPRECT,pRect,DWORD,Format,D3DCOLOR,Color) \
    ICOM_METHOD  (HRESULT,OnLostDevice) \
    ICOM_METHOD  (HRESULT,OnResetDevice)
#define ID3DXFont_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXFont_METHODS
ICOM_DEFINE(ID3DXFont,IUnknown)
#undef ICOM_INTERFACE

/* IUnknown Methods */
#define ID3DXFont_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXFont_AddRef(p)                 ICOM_CALL (AddRef,p)
#define ID3DXFont_Release(p)                ICOM_CALL (Release,p)
/* ID3DXFont Methods */
#define ID3DXFont_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#if 0
/* FIXME: This is from the original DX9 SDK */
#define ID3DXFont_GetLogFont(p,a)           ICOM_CALL1(GetLogFont,p,a)
#define ID3DXFont_Begin(p)                  ICOM_CALL(Begin,p)
#define ID3DXFont_DrawTextA(p,a,b,c,d,e)    ICOM_CALL5(DrawTextA,p,a,b,c,d,e)
#define ID3DXFont_DrawTextW(p,a,b,c,d,e)    ICOM_CALL5(DrawTextW,p,a,b,c,d,e)
#define ID3DXFont_End(p)                    ICOM_CALL(End,p)
#endif
/* This is from the April 2005 SDK (d3dx9_25.dll) */
#define ID3DXFont_GetDescA(p,a)             ICOM_CALL1(GetTextDescA,p,a)
#define ID3DXFont_GetDescW(p,a)             ICOM_CALL1(GetTextDescW,p,a)
#define ID3DXFont_GetTextMetricsA(p,a)      ICOM_CALL1(GetTextMetricsA,p,a)
#define ID3DXFont_GetTextMetricsW(p,a)      ICOM_CALL1(GetTextMetricsW,p,a)
#define ID3DXFont_GetDC(p)                  ICOM_CALL(GetDC,p)
#define ID3DXFont_GetGlyphData(p,a,b,c,d)   ICOM_CALL4(GetGlyphData,p,a,b,c,d)
#define ID3DXFont_PreloadCharacters(p,a,b)  ICOM_CALL2(PreloadCharacters,p,a,b)
#define ID3DXFont_PreloadGlyphs(p,a,b)      ICOM_CALL2(PreloadGlyphs,p,a,b)
#define ID3DXFont_PreloadTextA(p,a,b)       ICOM_CALL2(PreloadTextA,p,a,b)
#define ID3DXFont_PreloadTextW(p,a,b)       ICOM_CALL2(PreloadTextW,p,a,b)
#define ID3DXFont_DrawTextA(p,a,b,c,d,e,f)  ICOM_CALL6(DrawTextW,p,a,b,c,d,e,f)
#define ID3DXFont_DrawTextW(p,a,b,c,d,e,f)  ICOM_CALL6(DrawTextW,p,a,b,c,d,e,f)
#define ID3DXFont_OnLostDevice(p)           ICOM_CALL(OnLostDevice,p)
#define ID3DXFont_OnResetDevice(p)          ICOM_CALL(OnResetDevice,p)


/******************************************************************
 * ID3DXSprite interface
 */
#define ICOM_INTERFACE ID3DXSprite
#define ID3DXSprite_METHODS \
    ICOM_METHOD1 (HRESULT, GetDevice, LPDIRECT3DDEVICE9 *, ppDevice) \
    ICOM_METHOD1 (HRESULT, GetTransform, D3DXMATRIX *, pTransform) \
    ICOM_METHOD1 (HRESULT, SetTransform, CONST D3DXMATRIX *, pTransform) \
    ICOM_METHOD2 (HRESULT, SetWorldViewRH, CONST D3DXMATRIX *, pWorld, CONST D3DXMATRIX *, pView) \
    ICOM_METHOD2 (HRESULT, SetWorldViewLH, CONST D3DXMATRIX *, pWorld, CONST D3DXMATRIX *, pView) \
    ICOM_METHOD1 (HRESULT, Begin, DWORD, flags) \
    ICOM_METHOD5 (HRESULT, Draw, LPDIRECT3DTEXTURE9, pTexture, CONST RECT *, pSrcRect, CONST D3DXVECTOR3 *, pCenter, CONST D3DXVECTOR3 *, pPosition, D3DCOLOR, Color) \
    ICOM_METHOD  (HRESULT, Flush) \
    ICOM_METHOD  (HRESULT, End) \
    ICOM_METHOD  (HRESULT, OnLostDevice) \
    ICOM_METHOD  (HRESULT, OnResetDevice)
#define ID3DXSprite_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXSprite_METHODS
ICOM_DEFINE(ID3DXSprite,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXSprite_QueryInterface(p,a,b)           ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXSprite_AddRef(p)                       ICOM_CALL(AddRef,p)
#define ID3DXSprite_Release(p)                      ICOM_CALL(Release,p)
/*** ID3DXSprite methods ***/
#define ID3DXSprite_GetDevice(p,a)                  ICOM_CALL1(GetDevice, p, a)
#define ID3DXSprite_GetTransform(p, a)              ICOM_CALL1(GetTransform, p, a)
#define ID3DXSprite_SetTransform(p, a)              ICOM_CALL1(SetTransform, p, a)
#define ID3DXSprite_SetWorldViewRH(p, a, b)         ICOM_CALL2(SetWorldViewRH, p, a, b)
#define ID3DXSprite_SetWorldViewLH(p, a, b)         ICOM_CALL2(SetWorldViewLH, p, a, b)
#define ID3DXSprite_Begin(p, a)                     ICOM_CALL1(Begin, p, a)
#define ID3DXSprite_Draw(p, a, b, c, d, e)          ICOM_CALL5(Draw, p, a, b, c, d, e)
#define ID3DXSprite_Flush(p)                        ICOM_CALL(Flush, p)
#define ID3DXSprite_End(p)                          ICOM_CALL(End, p)
#define ID3DXSprite_OnLostDevice(p)                 ICOM_CALL(OnLostDevice, p)
#define ID3DXSprite_OnResetDevice(p)                ICOM_CALL(OnResetDevice, p)

#define D3DXSPRITE_DONOTSAVESTATE               (1 << 0)
#define D3DXSPRITE_DONOTMODIFY_RENDERSTATE      (1 << 1)
#define D3DXSPRITE_OBJECTSPACE                  (1 << 2)
#define D3DXSPRITE_BILLBOARD                    (1 << 3)
#define D3DXSPRITE_ALPHABLEND                   (1 << 4)
#define D3DXSPRITE_SORT_TEXTURE                 (1 << 5)
#define D3DXSPRITE_SORT_DEPTH_FRONTTOBACK       (1 << 6)
#define D3DXSPRITE_SORT_DEPTH_BACKTOFRONT       (1 << 7)
#define D3DXSPRITE_DO_NOT_ADDREF_TEXTURE        (1 << 8)


/******************************************************************
 * ID3DXRenderToSurface interface
 */
#define ICOM_INTERFACE ID3DXRenderToSurface
#define ID3DXRenderToSurface_METHODS \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD1 (HRESULT,GetDesc,D3DXRTS_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,BeginScene,LPDIRECT3DSURFACE9,pSurface,CONST D3DVIEWPORT9*,pViewport) \
    ICOM_METHOD1 (HRESULT,EndScene,DWORD,MipFilter) \
    ICOM_METHOD  (HRESULT,OnLostDevice) \
    ICOM_METHOD  (HRESULT,OnResetDevice)
#define ID3DXRenderToSurface_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXRenderToSurface_METHODS
ICOM_DEFINE(ID3DXRenderToSurface,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXRenderToSurface_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXRenderToSurface_AddRef(p)                  ICOM_CALL(AddRef,p)
#define ID3DXRenderToSurface_Release(p)                 ICOM_CALL(Release,p)
/*** ID3DXRenderToSurface methods ***/
#define ID3DXRenderToSurface_GetDevice(p,a)             ICOM_CALL1(GetDevice,p,a)
#define ID3DXRenderToSurface_GetDesc(p,a)               ICOM_CALL1(GetDesc,p,a)
#define ID3DXRenderToSurface_BeginScene(p,a,b)          ICOM_CALL2(BeginScene,p,a,b)
#define ID3DXRenderToSurface_EndScene(p,a)              ICOM_CALL1(EndScene,p,a)
#define ID3DXRenderToSurface_OnLostDevice(p)            ICOM_CALL(OnLostDevice,p)
#define ID3DXRenderToSurface_OnResetDevice(p)           ICOM_CALL(OnResetDevice,p)


/******************************************************************
 * ID3DXRenderToEnvMap interface
 */
#define ICOM_INTERFACE ID3DXRenderToEnvMap
#define ID3DXRenderToEnvMap_METHODS \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD1 (HRESULT,GetDesc,D3DXRTE_DESC*,pDesc) \
    ICOM_METHOD1 (HRESULT,BeginCube,LPDIRECT3DCUBETEXTURE9,pCubeTex) \
    ICOM_METHOD1 (HRESULT,BeginSphere,LPDIRECT3DTEXTURE9,pTex) \
    ICOM_METHOD2 (HRESULT,BeginHemisphere,LPDIRECT3DTEXTURE9,pTexZPos,LPDIRECT3DTEXTURE9,pTexZNeg) \
    ICOM_METHOD2 (HRESULT,BeginParabolic,LPDIRECT3DTEXTURE9,pTexZPos,LPDIRECT3DTEXTURE9,pTexZNeg) \
    ICOM_METHOD2 (HRESULT,Face,D3DCUBEMAP_FACES,Face,DWORD,MipFilter) \
    ICOM_METHOD1 (HRESULT,End,DWORD,MipFilter) \
    ICOM_METHOD  (HRESULT,OnLostDevice) \
    ICOM_METHOD  (HRESULT,OnResetDevice)
#define ID3DXRenderToEnvMap_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXRenderToEnvMap_METHODS
ICOM_DEFINE(ID3DXRenderToEnvMap,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXRenderToEnvMap_QueryInterface(p,a,b)       ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXRenderToEnvMap_AddRef(p)                   ICOM_CALL(AddRef,p)
#define ID3DXRenderToEnvMap_Release(p)                  ICOM_CALL(Release,p)
/*** ID3DXRenderToEnvMap methods ***/
#define ID3DXRenderToEnvMap_GetDevice(p,a)              ICOM_CALL1(GetDevice,p,a)
#define ID3DXRenderToEnvMap_GetDesc(p,a)                ICOM_CALL1(GetDesc,p,a)
#define ID3DXRenderToEnvMap_BeginCube(p,a)              ICOM_CALL1(BeginCube,p,a)
#define ID3DXRenderToEnvMap_BeginSphere(p,a)            ICOM_CALL1(BeginSphere,p,a)
#define ID3DXRenderToEnvMap_BeginHemisphere(p,a,b)      ICOM_CALL2(BeginHemisphere,p,a,b)
#define ID3DXRenderToEnvMap_BeginParabolic(p,a,b)       ICOM_CALL2(BeginParabolic,p,a,b)
#define ID3DXRenderToEnvMap_Face(p,a,b)                 ICOM_CALL2(Face,p,a,b)
#define ID3DXRenderToEnvMap_End(p,a)                    ICOM_CALL1(End,p,a)
#define ID3DXRenderToEnvMap_OnLostDevice(p)             ICOM_CALL(OnLostDevice,p)
#define ID3DXRenderToEnvMap_OnResetDevice(p)            ICOM_CALL(OnResetDevice,p)


/******************************************************************
 * ID3DXLine interface
 */
#define ICOM_INTERFACE ID3DXLine
#define ID3DXLine_METHODS \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD  (HRESULT,Begin) \
    ICOM_METHOD3 (HRESULT,Draw,CONST D3DXVECTOR2*,pVertexList,DWORD,dwVertexListCount,D3DCOLOR,Color) \
    ICOM_METHOD4 (HRESULT,DrawTransform,CONST D3DXVECTOR3*,pVertexList,DWORD,dwVertexListCount,CONST D3DXMATRIX*,pTransform,D3DCOLOR,Color) \
    ICOM_METHOD1 (HRESULT,SetPattern,DWORD,dwPatter) \
    ICOM_METHOD  (DWORD,GetPattern) \
    ICOM_METHOD1 (HRESULT,SetPatternScale,FLOAT,fPatternScale) \
    ICOM_METHOD  (FLOAT,GetPatternScale) \
    ICOM_METHOD1 (HRESULT,SetWidth,FLOAT,fWidth) \
    ICOM_METHOD  (FLOAT,GetWidth) \
    ICOM_METHOD1 (HRESULT,SetAntialias,BOOL,bAntialias) \
    ICOM_METHOD  (BOOL,GetAntialias) \
    ICOM_METHOD1 (HRESULT,SetGLLines,BOOL,bGLLines) \
    ICOM_METHOD  (BOOL,GetGLLines) \
    ICOM_METHOD  (HRESULT,End) \
    ICOM_METHOD  (HRESULT,OnLostDevice) \
    ICOM_METHOD  (HRESULT,OnResetDevice)
#define ID3DXLine_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXLine_METHODS
ICOM_DEFINE(ID3DXLine,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXLine_QueryInterface(p,a,b)         ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXLine_AddRef(p)                     ICOM_CALL(AddRef,p)
#define ID3DXLine_Release(p)                    ICOM_CALL(Release,p)
/*** ID3DXLine methods ***/
#define ID3DXLine_GetDevice(p,a)                ICOM_CALL1(GetDevice,p,a)
#define ID3DXLine_Begin(p)                      ICOM_CALL(Begin,p)
#define ID3DXLine_Draw(p,a,b,c)                 ICOM_CALL3(Draw,p,a,b,c)
#define ID3DXLine_DrawTransform(p,a,b,c,d)      ICOM_CALL4(DrawTransform,p,a,b,c,d)
#define ID3DXLine_SetPattern(p,a)               ICOM_CALL1(SetPattern,p,a)
#define ID3DXLine_GetPattern(p)                 ICOM_CALL(GetPattern,p)
#define ID3DXLine_SetPatternScale(p,a)          ICOM_CALL1(SetPatternScale,p,a)
#define ID3DXLine_GetPatternScale(p)            ICOM_CALL(GetPatternScale,p)
#define ID3DXLine_SetWidth(p,a)                 ICOM_CALL1(SetWidth,p,a)
#define ID3DXLine_GetWidth(p)                   ICOM_CALL(GetWidth,p)
#define ID3DXLine_SetAntialias(p,a)             ICOM_CALL1(SetAntialias,p,a)
#define ID3DXLine_GetAntialias(p)               ICOM_CALL(GetAntialias,p)
#define ID3DXLine_SetGLLines(p,a)               ICOM_CALL1(SetGLLines,p,a)
#define ID3DXLine_GetGLLines(p)                 ICOM_CALL(GetGLLines,p)
#define ID3DXLine_End(p)                        ICOM_CALL(End,p)
#define ID3DXLine_OnLostDevice(p)               ICOM_CALL(OnLostDevice,p)
#define ID3DXLine_OnResetDevice(p)              ICOM_CALL(OnResetDevice,p)


/******************************************************************
 * D3DX Core Functions
 */

#ifndef DrawText
#ifdef UNICODE
#define DrawText DrawTextW
#else
#define DrawText DrawTextA
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXCreateFontA(
        LPDIRECT3DDEVICE9       pDevice,
        INT                     Height,
        UINT                    Width,
        UINT                    Weight,
        UINT                    MipLevels,
        BOOL                    Italic,
        DWORD                   CharSet,
        DWORD                   OutputPrecision,
        DWORD                   Quality,
        DWORD                   PitchAndFamily,
        LPCSTR                  pFaceName,
        LPD3DXFONT*             ppFont);

HRESULT WINAPI
    D3DXCreateFontW(
        LPDIRECT3DDEVICE9       pDevice,
        INT                     Height,
        UINT                    Width,
        UINT                    Weight,
        UINT                    MipLevels,
        BOOL                    Italic,
        DWORD                   CharSet,
        DWORD                   OutputPrecision,
        DWORD                   Quality,
        DWORD                   PitchAndFamily,
        LPCWSTR                 pFaceName,
        LPD3DXFONT*             ppFont);

#ifdef UNICODE
#define D3DXCreateFont D3DXCreateFontW
#else
#define D3DXCreateFont D3DXCreateFontA
#endif

HRESULT WINAPI
    D3DXCreateFontIndirectA(
        LPDIRECT3DDEVICE9       pDevice,
        CONST D3DXFONT_DESCA*   pDesc,
        LPD3DXFONT*             ppFont);

HRESULT WINAPI
    D3DXCreateFontIndirectW(
        LPDIRECT3DDEVICE9       pDevice,
        CONST D3DXFONT_DESCW*   pDesc,
        LPD3DXFONT*             ppFont);

#ifdef UNICODE
#define D3DXCreateFontIndirect D3DXCreateFontIndirectW
#else
#define D3DXCreateFontIndirect D3DXCreateFontIndirectA
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXCreateSprite(
        LPDIRECT3DDEVICE9   pDevice,
        LPD3DXSPRITE*       ppSprite);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXCreateRenderToSurface(
        LPDIRECT3DDEVICE9       pDevice,
        UINT                    Width,
        UINT                    Height,
        D3DFORMAT               Format,
        BOOL                    DepthStencil,
        D3DFORMAT               DepthStencilFormat,
        LPD3DXRENDERTOSURFACE*  ppRenderToSurface);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXCreateRenderToEnvMap(
        LPDIRECT3DDEVICE9       pDevice,
        UINT                    Size,
        UINT                    MipLevels,
        D3DFORMAT               Format,
        BOOL                    DepthStencil,
        D3DFORMAT               DepthStencilFormat,
        LPD3DXRenderToEnvMap*   ppRenderToEnvMap);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXCreateLine(
        LPDIRECT3DDEVICE9   pDevice,
        LPD3DXLINE*         ppLine);

#ifdef __cplusplus
}
#endif

#endif /*__WINEX_D3DX9CORE_H */

