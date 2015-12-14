#ifndef __WINE_D3D9_H
#define __WINE_D3D9_H

#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#endif

#include "windef.h"
#include "wingdi.h"
#include "objbase.h"
#include "d3d9types.h" /* must precede d3d9caps.h */
#include "d3d9caps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define D3D_SDK_VERSION 31

/*****************************************************************************
 * Predeclare the interfaces
 */
DEFINE_GUID(IID_IDirect3D9,                 0x81bdcbca,0x64d4,0x426d,0xae,0x8d,0xad,0x01,0x47,0xf4,0x27,0x5c);
DEFINE_GUID(IID_IDirect3DDevice9,           0xd0223b96,0xbf7a,0x43fd,0x92,0xbd,0xa4,0x3b,0x0d,0x82,0xb9,0xeb);
DEFINE_GUID(IID_IDirect3DResource9,         0x05eec05d,0x8f7d,0x4362,0xb9,0x99,0xd1,0xba,0xf3,0x57,0xc7,0x04);
DEFINE_GUID(IID_IDirect3DBaseTexture9,      0x580ca87e,0x1d3c,0x4d54,0x99,0x1d,0xb7,0xd3,0xe3,0xc2,0x98,0xce);
DEFINE_GUID(IID_IDirect3DTexture9,          0x85c31227,0x3de5,0x4f00,0x9b,0x3a,0xf1,0x1a,0xc3,0x8c,0x18,0xb5);
DEFINE_GUID(IID_IDirect3DCubeTexture9,      0xfff32f81,0xd953,0x473a,0x92,0x23,0x93,0xd6,0x52,0xab,0xa9,0x3f);
DEFINE_GUID(IID_IDirect3DVolumeTexture9,    0x2518526c,0xe789,0x4111,0xa7,0xb9,0x47,0xef,0x32,0x8d,0x13,0xe6);
DEFINE_GUID(IID_IDirect3DVertexBuffer9,     0xb64bb1b5,0xfd70,0x4df6,0xbf,0x91,0x19,0xd0,0xa1,0x24,0x55,0xe3);
DEFINE_GUID(IID_IDirect3DIndexBuffer9,      0x7c9dd65e,0xd3f7,0x4529,0xac,0xee,0x78,0x58,0x30,0xac,0xde,0x35);
DEFINE_GUID(IID_IDirect3DSurface9,          0x0cfbaf3a,0x9ff6,0x429a,0x99,0xb3,0xa2,0x79,0x6a,0xf8,0xb8,0x9b);
DEFINE_GUID(IID_IDirect3DVolume9,           0x24f416e6,0x1f67,0x4aa7,0xb8,0x8e,0xd3,0x3f,0x6f,0x31,0x28,0xa1);
DEFINE_GUID(IID_IDirect3DSwapChain9,        0x794950f2,0xadfc,0x458a,0x90,0x5e,0x10,0xa1,0x0b,0x0b,0x50,0x3b);
DEFINE_GUID(IID_IDirect3DVertexDeclaration9,0xdd13c59c,0x36fa,0x4098,0xa8,0xfb,0xc7,0xed,0x39,0xdc,0x85,0x46);
DEFINE_GUID(IID_IDirect3DVertexShader9,     0xefc5557e,0x6265,0x4613,0x8a,0x94,0x43,0x85,0x78,0x89,0xeb,0x36);
DEFINE_GUID(IID_IDirect3DPixelShader9,      0x6d3bdbdc,0x5b02,0x4415,0xb8,0x52,0xce,0x5e,0x8b,0xcc,0xb2,0x89);
DEFINE_GUID(IID_IDirect3DStateBlock9,       0xb07c4fe5,0x310d,0x4ba8,0xa2,0x3c,0x4f,0x0f,0x20,0x6f,0x21,0x8b);
DEFINE_GUID(IID_IDirect3DQuery9,            0xd9771460,0xa695,0x4f26,0xbb,0xd3,0x27,0xb8,0x40,0xb5,0x41,0xcc);

typedef struct IDirect3D9                  IDirect3D9, *LPDIRECT3D9;
typedef struct IDirect3DDevice9            IDirect3DDevice9, *LPDIRECT3DDEVICE9;
typedef struct IDirect3DStateBlock9        IDirect3DStateBlock9, *LPDIRECT3DSTATEBLOCK9;
typedef struct IDirect3DResource9          IDirect3DResource9, *LPDIRECT3DRESOURCE9;
typedef struct IDirect3DVertexDeclaration9 IDirect3DVertexDeclaration9, *LPDIRECT3DVERTEXDECLARATION9;
typedef struct IDirect3DVertexShader9      IDirect3DVertexShader9, *LPDIRECT3DVERTEXSHADER9;
typedef struct IDirect3DPixelShader9       IDirect3DPixelShader9, *LPDIRECT3DPIXELSHADER9;
typedef struct IDirect3DBaseTexture9       IDirect3DBaseTexture9, *LPDIRECT3DBASETEXTURE9;
typedef struct IDirect3DTexture9           IDirect3DTexture9, *LPDIRECT3DTEXTURE9;
typedef struct IDirect3DVolumeTexture9     IDirect3DVolumeTexture9, *LPDIRECT3DVOLUMETEXTURE9;
typedef struct IDirect3DCubeTexture9       IDirect3DCubeTexture9, *LPDIRECT3DCUBETEXTURE9;
typedef struct IDirect3DVertexBuffer9      IDirect3DVertexBuffer9, *LPDIRECT3DVERTEXBUFFER9;
typedef struct IDirect3DIndexBuffer9       IDirect3DIndexBuffer9, *LPDIRECT3DINDEXBUFFER9;
typedef struct IDirect3DSurface9           IDirect3DSurface9, *LPDIRECT3DSURFACE9;
typedef struct IDirect3DVolume9            IDirect3DVolume9, *LPDIRECT3DVOLUME9;
typedef struct IDirect3DSwapChain9         IDirect3DSwapChain9, *LPDIRECT3DSWAPCHAIN9;
typedef struct IDirect3DQuery9             IDirect3DQuery9, *LPDIRECT3DQUERY9;

/* ********************************************************************
   Error Codes
   ******************************************************************** */
#define _FACD3D 0x876
#define MAKE_D3DHRESULT(code) MAKE_HRESULT(1, _FACD3D, code)
#define MAKE_D3DSTATUS(code)  MAKE_HRESULT(0, _FACD3D, code)

#define D3D_OK                          S_OK

#define D3DERR_WRONGTEXTUREFORMAT		MAKE_D3DHRESULT(2072)
#define D3DERR_UNSUPPORTEDCOLOROPERATION	MAKE_D3DHRESULT(2073)
#define D3DERR_UNSUPPORTEDCOLORARG		MAKE_D3DHRESULT(2074)
#define D3DERR_UNSUPPORTEDALPHAOPERATION	MAKE_D3DHRESULT(2075)
#define D3DERR_UNSUPPORTEDALPHAARG		MAKE_D3DHRESULT(2076)
#define D3DERR_TOOMANYOPERATIONS		MAKE_D3DHRESULT(2077)
#define D3DERR_CONFLICTINGTEXTUREFILTER		MAKE_D3DHRESULT(2078)
#define D3DERR_UNSUPPORTEDFACTORVALUE		MAKE_D3DHRESULT(2079)
#define D3DERR_CONFLICTINGRENDERSTATE		MAKE_D3DHRESULT(2081)
#define D3DERR_UNSUPPORTEDTEXTUREFILTER		MAKE_D3DHRESULT(2082)
#define D3DERR_CONFLICTINGTEXTUREPALETTE	MAKE_D3DHRESULT(2086)
#define D3DERR_DRIVERINTERNALERROR		MAKE_D3DHRESULT(2087)

#define D3DERR_NOTFOUND				MAKE_D3DHRESULT(2150)
#define D3DERR_MOREDATA				MAKE_D3DHRESULT(2151)
#define D3DERR_DEVICELOST			MAKE_D3DHRESULT(2152)
#define D3DERR_DEVICENOTRESET			MAKE_D3DHRESULT(2153)
#define D3DERR_NOTAVAILABLE			MAKE_D3DHRESULT(2154)
#define D3DERR_OUTOFVIDEOMEMORY			MAKE_D3DHRESULT(380)
#define D3DERR_INVALIDDEVICE			MAKE_D3DHRESULT(2155)
#define D3DERR_INVALIDCALL			MAKE_D3DHRESULT(2156)
#define D3DERR_DRIVERINVALIDCALL		MAKE_D3DHRESULT(2157)
#define D3DERR_WASSTILLDRAWING			MAKE_D3DHRESULT(540)
#define D3DOK_NOAUTOGEN				MAKE_D3DSTATUS(2159)

/* ********************************************************************
   Enums
   ******************************************************************** */

#define D3DSPD_IUNKNOWN               0x00000001

#define D3DCURRENT_DISPLAY_MODE       0x00EFFFFF

#define D3DCREATE_FPU_PRESERVE              0x00000002
#define D3DCREATE_MULTITHREADED             0x00000004
#define D3DCREATE_PUREDEVICE                0x00000010
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040
#define D3DCREATE_MIXED_VERTEXPROCESSING    0x00000080
#define D3DCREATE_DISABLE_DRIVER_MANAGEMENT 0x00000100
#define D3DCREATE_ADAPTERGROUP_DEVICE       0x00000200

#define D3DADAPTER_DEFAULT            0

#define D3DENUM_WHQL_LEVEL            0x00000002

#define D3DPRESENT_BACK_BUFFERS_MAX   3

#define D3DSGR_NO_CALIBRATION         0x00000000
#define D3DSGR_CALIBRATE              0x00000001

#define D3DCURSOR_IMMEDIATE_UPDATE    0x00000001

#define D3DPRESENT_DONOTWAIT          0x00000001
#define D3DPRESENT_LINEAR_CONTENT     0x00000002

/* ********************************************************************
   Types and structures
   ******************************************************************** */


/*****************************************************************************
 * IDirect3D9 interface
 */
#define ICOM_INTERFACE IDirect3D9
#define IDirect3D9_METHODS \
    ICOM_METHOD1(HRESULT,RegisterSoftwareDevice,     void*,pInitializeFunction) \
    ICOM_METHOD (UINT,   GetAdapterCount) \
    ICOM_METHOD3(HRESULT,GetAdapterIdentifier,       UINT,Adapter,DWORD,Flags,D3DADAPTER_IDENTIFIER9*,pIdentifier) \
    ICOM_METHOD2(UINT,   GetAdapterModeCount,        UINT,Adapter,D3DFORMAT,Format) \
    ICOM_METHOD4(HRESULT,EnumAdapterModes,           UINT,Adapter,D3DFORMAT,Format,UINT,Mode,D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD2(HRESULT,GetAdapterDisplayMode,      UINT,Adapter,D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD5(HRESULT,CheckDeviceType,            UINT,Adapter,D3DDEVTYPE,CheckType,D3DFORMAT,DisplayFormat,D3DFORMAT,BackBufferFormat,BOOL,Windowed) \
    ICOM_METHOD6(HRESULT,CheckDeviceFormat,          UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,AdapterFormat,DWORD,Usage,D3DRESOURCETYPE,RType,D3DFORMAT,CheckFormat) \
    ICOM_METHOD6(HRESULT,CheckDeviceMultiSampleType, UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,SurfaceFormat,BOOL,Windowed,D3DMULTISAMPLE_TYPE,MultiSampleType,DWORD*,pQualityLevels) \
    ICOM_METHOD5(HRESULT,CheckDepthStencilMatch,     UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,AdapterFormat,D3DFORMAT,RenderTargetFormat,D3DFORMAT,DepthStencilFormat) \
    ICOM_METHOD4(HRESULT,CheckDeviceFormatConversion,UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,SourceFormat,D3DFORMAT,TargetFormat) \
    ICOM_METHOD3(HRESULT,GetDeviceCaps,              UINT,Adapter,D3DDEVTYPE,DeviceType,D3DCAPS9*,pCaps) \
    ICOM_METHOD1(HMONITOR,GetAdapterMonitor,         UINT,Adapter) \
    ICOM_METHOD6(HRESULT,CreateDevice,               UINT,Adapter,D3DDEVTYPE,DeviceType,HWND,hFocusWindow,DWORD,BehaviorFlags,D3DPRESENT_PARAMETERS*,pPresentationParameters,IDirect3DDevice9**,ppReturnedDeviceInterface)
#define IDirect3D9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3D9_METHODS
ICOM_DEFINE(IDirect3D9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3D9_QueryInterface(p,a,b)                   ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3D9_AddRef(p)                               ICOM_CALL (AddRef,p)
#define IDirect3D9_Release(p)                              ICOM_CALL (Release,p)
	/*** IDirect3D9 methods ***/
#define IDirect3D9_RegisterSoftwareDevice(p,a)             ICOM_CALL1(RegisterSoftwareDevice,p,a)
#define IDirect3D9_GetAdapterCount(p)                      ICOM_CALL (GetAdapterCount,p)
#define IDirect3D9_GetAdapterIdentifier(p,a,b,c)           ICOM_CALL3(GetAdapterIdentifier,p,a,b,c)
#define IDirect3D9_GetAdapterModeCount(p,a,b)              ICOM_CALL2(GetAdapterModeCount,p,a,b)
#define IDirect3D9_EnumAdapterModes(p,a,b,c,d)             ICOM_CALL4(EnumAdapterModes,p,a,b,c,d)
#define IDirect3D9_GetAdapterDisplayMode(p,a,b)            ICOM_CALL2(GetAdapterDisplayMode,p,a,b)
#define IDirect3D9_CheckDeviceType(p,a,b,c,d,e)            ICOM_CALL5(CheckDeviceType,p,a,b,c,d,e)
#define IDirect3D9_CheckDeviceFormat(p,a,b,c,d,e,f)        ICOM_CALL6(CheckDeviceFormat,p,a,b,c,d,e,f)
#define IDirect3D9_CheckDeviceMultiSampleType(p,a,b,c,d,e) ICOM_CALL5(CheckDeviceMultiSampleType,p,a,b,c,d,e)
#define IDirect3D9_CheckDepthStencilMatch(p,a,b,c,d,e)     ICOM_CALL5(CheckDepthStencilMatch,p,a,b,c,d,e)
#define IDirect3D9_CheckDeviceFormatConversion(p,a,b,c,d)  ICOM_CALL4(CheckDeviceFormatConversion,p,a,b,c,d)
#define IDirect3D9_GetDeviceCaps(p,a,b,c)                  ICOM_CALL3(CheckDepthStencilMatch,p,a,b,c)
#define IDirect3D9_GetAdapterMonitor(p,a)                  ICOM_CALL1(GetAdapterMonitor,p,a)
#define IDirect3D9_CreateDevice(p,a,b,c,d,e,f)             ICOM_CALL6(CreateDevice,p,a,b,c,d,e,f)


/*****************************************************************************
 * IDirect3DDevice9 interface
 */
#define ICOM_INTERFACE IDirect3DDevice9
#define IDirect3DDevice9_METHODS \
    ICOM_METHOD (HRESULT,TestCooperativeLevel) \
    ICOM_METHOD (UINT,   GetAvailableTextureMem) \
    ICOM_METHOD (HRESULT,EvictManagedResources) \
    ICOM_METHOD1(HRESULT,GetDirect3D,                IDirect3D9**,ppD3D9) \
    ICOM_METHOD1(HRESULT,GetDeviceCaps,              D3DCAPS9*,pCaps) \
    ICOM_METHOD2(HRESULT,GetDisplayMode,             UINT,iSwapChain,D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD1(HRESULT,GetCreationParameters,      D3DDEVICE_CREATION_PARAMETERS*,pParameters) \
    ICOM_METHOD3(HRESULT,SetCursorProperties,        UINT,XHotSpot,UINT,YHotSpot,IDirect3DSurface9*,pCursorBitmap) \
    ICOM_VMETHOD3(       SetCursorPosition,          UINT,XScreenSpace,UINT,YScreenSpace,DWORD,Flags) \
    ICOM_METHOD1(BOOL,   ShowCursor,                 BOOL,bShow) \
    ICOM_METHOD2(HRESULT,CreateAdditionalSwapChain,  D3DPRESENT_PARAMETERS*,pPresentationParameters,IDirect3DSwapChain9**,pSwapChain) \
    ICOM_METHOD2(HRESULT,GetSwapChain,               UINT,iSwapChain,IDirect3DSwapChain9**,pSwapChain) \
    ICOM_METHOD (UINT,   GetNumberOfSwapChains) \
    ICOM_METHOD1(HRESULT,Reset,                      D3DPRESENT_PARAMETERS*,pPresentationParameters) \
    ICOM_METHOD4(HRESULT,Present,                    CONST RECT*,pSourceRect,CONST RECT*,pDestRect,HWND,hDestWindowOverride,CONST RGNDATA*,pDirtyRegion) \
    ICOM_METHOD4(HRESULT,GetBackBuffer,              UINT,iSwapChain,UINT,BackBuffer,D3DBACKBUFFER_TYPE,Type,IDirect3DSurface9**,ppBackBuffer) \
    ICOM_METHOD2(HRESULT,GetRasterStatus,            UINT,iSwapChain,D3DRASTER_STATUS*,pRasterStatus) \
    ICOM_METHOD1(HRESULT,SetDialogBoxMode,           BOOL,bEnableDialogs) \
    ICOM_VMETHOD3(       SetGammaRamp,               UINT,iSwapChain,DWORD,Flags,CONST D3DGAMMARAMP*,pRamp) \
    ICOM_VMETHOD2(       GetGammaRamp,               UINT,iSwapChain,D3DGAMMARAMP*,pRamp) \
    ICOM_METHOD8(HRESULT,CreateTexture,              UINT,Width,UINT,Height,UINT,Levels,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DTexture9**,ppTexture,HANDLE*,pSharedHandle) \
    ICOM_METHOD9(HRESULT,CreateVolumeTexture,        UINT,Width,UINT,Height,UINT,Depth,UINT,Levels,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DVolumeTexture9**,ppVolumeTexture,HANDLE*,pSharedHandle) \
    ICOM_METHOD7(HRESULT,CreateCubeTexture,          UINT,EdgeLength,UINT,Levels,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DCubeTexture9**,ppCubeTexture,HANDLE*,pSharedHandle) \
    ICOM_METHOD6(HRESULT,CreateVertexBuffer,         UINT,Length,DWORD,Usage,DWORD,FVF,D3DPOOL,Pool,IDirect3DVertexBuffer9**,ppVertexBuffer,HANDLE*,pSharedHandle) \
    ICOM_METHOD6(HRESULT,CreateIndexBuffer,          UINT,Length,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DIndexBuffer9**,ppIndexBuffer,HANDLE*,pSharedHandle) \
    ICOM_METHOD8(HRESULT,CreateRenderTarget,         UINT,Width,UINT,Height,D3DFORMAT,Format,D3DMULTISAMPLE_TYPE,MultiSample,DWORD,MultiSampleQuality,BOOL,Lockable,IDirect3DSurface9**,ppSurface,HANDLE*,pSharedHandle) \
    ICOM_METHOD8(HRESULT,CreateDepthStencilSurface,  UINT,Width,UINT,Height,D3DFORMAT,Format,D3DMULTISAMPLE_TYPE,MultiSample,DWORD,MultiSampleQuality,BOOL,Discard,IDirect3DSurface9**,ppSurface,HANDLE*,pSharedHandle) \
    ICOM_METHOD4(HRESULT,UpdateSurface,              IDirect3DSurface9*,pSourceSurface,CONST RECT*,pSourceRect,IDirect3DSurface9*,pDestSurface,CONST POINT*,pDestPoint) \
    ICOM_METHOD2(HRESULT,UpdateTexture,              IDirect3DBaseTexture9*,pSourceTexture,IDirect3DBaseTexture9*,pDestTexture) \
    ICOM_METHOD2(HRESULT,GetRenderTargetData,        IDirect3DSurface9*,pRenderTarget,IDirect3DSurface9*,pDestSurface) \
    ICOM_METHOD2(HRESULT,GetFrontBufferData,         UINT,iSwapChain,IDirect3DSurface9*,pDestSurface) \
    ICOM_METHOD5(HRESULT,StretchRect,                IDirect3DSurface9*,pSourceSurface,CONST RECT*,pSourceRect,IDirect3DSurface9*,pDestSurface,CONST RECT*,pDestRect,D3DTEXTUREFILTERTYPE,Filter) \
    ICOM_METHOD3(HRESULT,ColorFill,                  IDirect3DSurface9*,pSurface,CONST RECT*,pRect,D3DCOLOR,color) \
    ICOM_METHOD6(HRESULT,CreateOffscreenPlainSurface,UINT,Width,UINT,Height,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DSurface9**,ppSurface,HANDLE*,pSharedHandle) \
    ICOM_METHOD2(HRESULT,SetRenderTarget,            DWORD,RenderTargetIndex,IDirect3DSurface9*,pRenderTarget) \
    ICOM_METHOD2(HRESULT,GetRenderTarget,            DWORD,RenderTargetIndex,IDirect3DSurface9**,ppRenderTarget) \
    ICOM_METHOD1(HRESULT,SetDepthStencilSurface,     IDirect3DSurface9*,pZStencilSurface) \
    ICOM_METHOD1(HRESULT,GetDepthStencilSurface,     IDirect3DSurface9**,ppZStencilSurface) \
    ICOM_METHOD (HRESULT,BeginScene) \
    ICOM_METHOD (HRESULT,EndScene) \
    ICOM_METHOD6(HRESULT,Clear,                      DWORD,Count,CONST D3DRECT*,pRects,DWORD,Flags,D3DCOLOR,Color,float,Z,DWORD,Stencil) \
    ICOM_METHOD2(HRESULT,SetTransform,               D3DTRANSFORMSTATETYPE,State,CONST D3DMATRIX*,pMatrix) \
    ICOM_METHOD2(HRESULT,GetTransform,               D3DTRANSFORMSTATETYPE,State,D3DMATRIX*,pMatrix) \
    ICOM_METHOD2(HRESULT,MultiplyTransform,          D3DTRANSFORMSTATETYPE,State,CONST D3DMATRIX*,pMatrix) \
    ICOM_METHOD1(HRESULT,SetViewport,                CONST D3DVIEWPORT9*,pViewport) \
    ICOM_METHOD1(HRESULT,GetViewport,                D3DVIEWPORT9*,pViewport) \
    ICOM_METHOD1(HRESULT,SetMaterial,                CONST D3DMATERIAL9*,pMaterial) \
    ICOM_METHOD1(HRESULT,GetMaterial,                D3DMATERIAL9*,pMaterial) \
    ICOM_METHOD2(HRESULT,SetLight,                   DWORD,Index,CONST D3DLIGHT9*,pLight) \
    ICOM_METHOD2(HRESULT,GetLight,                   DWORD,Index,D3DLIGHT9*,pLight) \
    ICOM_METHOD2(HRESULT,LightEnable,                DWORD,Index,BOOL,Enable) \
    ICOM_METHOD2(HRESULT,GetLightEnable,             DWORD,Index,BOOL*,pEnable) \
    ICOM_METHOD2(HRESULT,SetClipPlane,               DWORD,Index,CONST float*,pPlane) \
    ICOM_METHOD2(HRESULT,GetClipPlane,               DWORD,Index,float*,pPlane) \
    ICOM_METHOD2(HRESULT,SetRenderState,             D3DRENDERSTATETYPE,State,DWORD,Value) \
    ICOM_METHOD2(HRESULT,GetRenderState,             D3DRENDERSTATETYPE,State,DWORD*,pValue) \
    ICOM_METHOD2(HRESULT,CreateStateBlock,           D3DSTATEBLOCKTYPE,Type,IDirect3DStateBlock9**,ppSB) \
    ICOM_METHOD (HRESULT,BeginStateBlock) \
    ICOM_METHOD1(HRESULT,EndStateBlock,              IDirect3DStateBlock9**,ppSB) \
    ICOM_METHOD1(HRESULT,SetClipStatus,              CONST D3DCLIPSTATUS9*,pClipStatus) \
    ICOM_METHOD1(HRESULT,GetClipStatus,              D3DCLIPSTATUS9*,pClipStatus) \
    ICOM_METHOD2(HRESULT,GetTexture,                 DWORD,Stage,IDirect3DBaseTexture9**,ppTexture) \
    ICOM_METHOD2(HRESULT,SetTexture,                 DWORD,Stage,IDirect3DBaseTexture9*,pTexture) \
    ICOM_METHOD3(HRESULT,GetTextureStageState,       DWORD,Stage,D3DTEXTURESTAGESTATETYPE,Type,DWORD*,pValue) \
    ICOM_METHOD3(HRESULT,SetTextureStageState,       DWORD,Stage,D3DTEXTURESTAGESTATETYPE,Type,DWORD,Value) \
    ICOM_METHOD3(HRESULT,GetSamplerState,            DWORD,Sampler,D3DSAMPLERSTATETYPE,Type,DWORD*,pValue) \
    ICOM_METHOD3(HRESULT,SetSamplerState,            DWORD,Sampler,D3DSAMPLERSTATETYPE,Type,DWORD,Value) \
    ICOM_METHOD1(HRESULT,ValidateDevice,             DWORD*,pNumPasses) \
    ICOM_METHOD2(HRESULT,SetPaletteEntries,          UINT,PaletteNumber,CONST PALETTEENTRY*,pEntries) \
    ICOM_METHOD2(HRESULT,GetPaletteEntries,          UINT,PaletteNumber,PALETTEENTRY*,pEntries) \
    ICOM_METHOD1(HRESULT,SetCurrentTexturePalette,   UINT,PaletteNumber) \
    ICOM_METHOD1(HRESULT,GetCurrentTexturePalette,   UINT*,pPaletteNumber) \
    ICOM_METHOD1(HRESULT,SetScissorRect,             CONST RECT*,pRect) \
    ICOM_METHOD1(HRESULT,GetScissorRect,             RECT*,pRect) \
    ICOM_METHOD1(HRESULT,SetSoftwareVertexProcessing,BOOL,bSoftware) \
    ICOM_METHOD (BOOL,   GetSoftwareVertexProcessing) \
    ICOM_METHOD1(HRESULT,SetNPatchMode,              float,nSegments) \
    ICOM_METHOD (float,  GetNPatchMode) \
    ICOM_METHOD3(HRESULT,DrawPrimitive,              D3DPRIMITIVETYPE,PrimitiveType,UINT,StartVertex,UINT,PrimitiveCount) \
    ICOM_METHOD6(HRESULT,DrawIndexedPrimitive,       D3DPRIMITIVETYPE,PrimitiveType,INT,BaseVertexIndex,UINT,MinIndex,UINT,NumVertices,UINT,StartIndex,UINT,PrimitiveCount) \
    ICOM_METHOD4(HRESULT,DrawPrimitiveUP,            D3DPRIMITIVETYPE,PrimitiveType,UINT,PrimitiveCount,CONST void*,pVertexStreamZeroData,UINT,VertexStreamZeroStride) \
    ICOM_METHOD8(HRESULT,DrawIndexedPrimitiveUP,     D3DPRIMITIVETYPE,PrimitiveType,UINT,MinIndex,UINT,NumVertices,UINT,PrimitiveCount,CONST void*,pIndexData,D3DFORMAT,IndexFormat,CONST void*,pVertexStreamZeroData,UINT,VertexStreamZeroStride) \
    ICOM_METHOD6(HRESULT,ProcessVertices,            UINT,SrcStartIndex,UINT,DestIndex,UINT,VertexCount,IDirect3DVertexBuffer9*,pDestBuffer,IDirect3DVertexDeclaration9*,pVertexDecl,DWORD,Flags) \
    ICOM_METHOD2(HRESULT,CreateVertexDeclaration,    CONST D3DVERTEXELEMENT9*,pVertexElements,IDirect3DVertexDeclaration9**,ppDecl) \
    ICOM_METHOD1(HRESULT,SetVertexDeclaration,       IDirect3DVertexDeclaration9*,pDecl) \
    ICOM_METHOD1(HRESULT,GetVertexDeclaration,       IDirect3DVertexDeclaration9**,ppDecl) \
    ICOM_METHOD1(HRESULT,SetFVF,                     DWORD,FVF) \
    ICOM_METHOD1(HRESULT,GetFVF,                     DWORD*,pFVF) \
    ICOM_METHOD2(HRESULT,CreateVertexShader,         CONST DWORD*,pFunction,IDirect3DVertexShader9**,ppShader) \
    ICOM_METHOD1(HRESULT,SetVertexShader,            IDirect3DVertexShader9*,pShader) \
    ICOM_METHOD1(HRESULT,GetVertexShader,            IDirect3DVertexShader9**,ppShader) \
    ICOM_METHOD3(HRESULT,SetVertexShaderConstantF,   UINT,StartRegister,CONST float*,pConstantData,UINT,Vector4fCount) \
    ICOM_METHOD3(HRESULT,GetVertexShaderConstantF,   UINT,StartRegister,float*,pConstantData,UINT,Vector4fCount) \
    ICOM_METHOD3(HRESULT,SetVertexShaderConstantI,   UINT,StartRegister,CONST int*,pConstantData,UINT,Vector4iCount) \
    ICOM_METHOD3(HRESULT,GetVertexShaderConstantI,   UINT,StartRegister,int*,pConstantData,UINT,Vector4iCount) \
    ICOM_METHOD3(HRESULT,SetVertexShaderConstantB,   UINT,StartRegister,CONST BOOL*,pConstantData,UINT,BoolCount) \
    ICOM_METHOD3(HRESULT,GetVertexShaderConstantB,   UINT,StartRegister,BOOL*,pConstantData,UINT,BoolCount) \
    ICOM_METHOD4(HRESULT,SetStreamSource,            UINT,StreamNumber,IDirect3DVertexBuffer9*,pStreamData,UINT,OffsetInBytes,UINT,Stride) \
    ICOM_METHOD4(HRESULT,GetStreamSource,            UINT,StreamNumber,IDirect3DVertexBuffer9**,ppStreamData,UINT*,pOffsetInBytes,UINT*,pStride) \
    ICOM_METHOD2(HRESULT,SetStreamSourceFreq,        UINT,StreamNumber,UINT,Divider) \
    ICOM_METHOD2(HRESULT,GetStreamSourceFreq,        UINT,StreamNumber,UINT*,pDivider) \
    ICOM_METHOD1(HRESULT,SetIndices,                 IDirect3DIndexBuffer9*,pIndexData) \
    ICOM_METHOD1(HRESULT,GetIndices,                 IDirect3DIndexBuffer9**,ppIndexData) \
    ICOM_METHOD2(HRESULT,CreatePixelShader,          CONST DWORD*,pFunction,IDirect3DPixelShader9**,ppShader) \
    ICOM_METHOD1(HRESULT,SetPixelShader,             IDirect3DPixelShader9*,pShader) \
    ICOM_METHOD1(HRESULT,GetPixelShader,             IDirect3DPixelShader9**,ppShader) \
    ICOM_METHOD3(HRESULT,SetPixelShaderConstantF,    UINT,StartRegister,CONST float*,pConstantData,UINT,Vector4fCount) \
    ICOM_METHOD3(HRESULT,GetPixelShaderConstantF,    UINT,StartRegister,float*,pConstantData,UINT,Vector4fCount) \
    ICOM_METHOD3(HRESULT,SetPixelShaderConstantI,    UINT,StartRegister,CONST int*,pConstantData,UINT,Vector4iCount) \
    ICOM_METHOD3(HRESULT,GetPixelShaderConstantI,    UINT,StartRegister,int*,pConstantData,UINT,Vector4iCount) \
    ICOM_METHOD3(HRESULT,SetPixelShaderConstantB,    UINT,StartRegister,CONST BOOL*,pConstantData,UINT,BoolCount) \
    ICOM_METHOD3(HRESULT,GetPixelShaderConstantB,    UINT,StartRegister,BOOL*,pConstantData,UINT,BoolCount) \
    ICOM_METHOD3(HRESULT,DrawRectPatch,              UINT,Handle,CONST float*,pNumSegs,CONST D3DRECTPATCH_INFO*,pRectPatchInfo) \
    ICOM_METHOD3(HRESULT,DrawTriPatch,               UINT,Handle,CONST float*,pNumSegs,CONST D3DTRIPATCH_INFO*,pTriPatchInfo) \
    ICOM_METHOD1(HRESULT,DeletePatch,                UINT,Handle) \
    ICOM_METHOD2(HRESULT,CreateQuery,                D3DQUERYTYPE,Type,IDirect3DQuery9**,ppQuery)

#define IDirect3DDevice9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DDevice9_METHODS
ICOM_DEFINE(IDirect3DDevice9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DDevice9_QueryInterface(p,a,b)                        ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DDevice9_AddRef(p)                                    ICOM_CALL (AddRef,p)
#define IDirect3DDevice9_Release(p)                                   ICOM_CALL (Release,p)
	/*** IDirect3DDevice9 methods ***/
#define IDirect3DDevice9_TestCooperativeLevel(p)                      ICOM_CALL (TestCooperativeLevel,p)
#define IDirect3DDevice9_GetAvailableTextureMem(p)                    ICOM_CALL (GetAvailableTextureMem,p)
#define IDirect3DDevice9_EvictManagedResources(p)                     ICOM_CALL (EvictManagedResources,p)
#define IDirect3DDevice9_GetDirect3D(p,a)                             ICOM_CALL1(GetDirect3D,p,a)
#define IDirect3DDevice9_GetDeviceCaps(p,a)                           ICOM_CALL1(GetDeviceCaps,p,a)
#define IDirect3DDevice9_GetDisplayMode(p,a,b)                        ICOM_CALL2(GetDisplayMode,p,a,b)
#define IDirect3DDevice9_GetCreationParameters(p,a)                   ICOM_CALL1(GetCreationParameters,p,a)
#define IDirect3DDevice9_SetCursorProperties(p,a,b,c)                 ICOM_CALL3(SetCursorProperties,p,a,b,c)
#define IDirect3DDevice9_SetCursorPosition(p,a,b,c)                   ICOM_CALL3(SetCursorPosition,p,a,b,c)
#define IDirect3DDevice9_ShowCursor(p,a)                              ICOM_CALL1(ShowCursor,p,a)
#define IDirect3DDevice9_CreateAdditionalSwapChain(p,a,b)             ICOM_CALL2(CreateAdditionalSwapChain,p,a,b)
#define IDirect3DDevice9_GetSwapChain(p,a,b)                          ICOM_CALL2(GetSwapChain,p,a,b)
#define IDirect3DDevice9_GetNumberOfSwapChains(p)                     ICOM_CALL (GetNumberOfSwapChains,p,a)
#define IDirect3DDevice9_Reset(p,a)                                   ICOM_CALL1(Reset,p,a)
#define IDirect3DDevice9_Present(p,a,b,c,d)                           ICOM_CALL4(Present,p,a,b,c,d)
#define IDirect3DDevice9_GetBackBuffer(p,a,b,c,d)                     ICOM_CALL4(GetBackBuffer,p,a,b,c,d)
#define IDirect3DDevice9_GetRasterStatus(p,a,b)                       ICOM_CALL2(GetRasterStatus,p,a,b)
#define IDirect3DDevice9_SetDialogBoxMode(p,a)                        ICOM_CALL1(SetDialogBoxMode,p,a)
#define IDirect3DDevice9_SetGammaRamp(p,a,b,c)                        ICOM_CALL3(SetGammaRamp,p,a,b,c)
#define IDirect3DDevice9_GetGammaRamp(p,a,b)                          ICOM_CALL2(GetGammaRamp,p,a,b)
#define IDirect3DDevice9_CreateTexture(p,a,b,c,d,e,f,g,h)             ICOM_CALL8(CreateTexture,p,a,b,c,d,e,f,g,h)
#define IDirect3DDevice9_CreateVolumeTexture(p,a,b,c,d,e,f,g,h,i)     ICOM_CALL9(CreateVolumeTexture,p,a,b,c,d,e,f,g,h,i)
#define IDirect3DDevice9_CreateCubeTexture(p,a,b,c,d,e,f,g)           ICOM_CALL7(CreateCubeTexture,p,a,b,c,d,e,f,g)
#define IDirect3DDevice9_CreateVertexBuffer(p,a,b,c,d,e,f)            ICOM_CALL6(CreateVertexBuffer,p,a,b,c,d,e,f)
#define IDirect3DDevice9_CreateIndexBuffer(p,a,b,c,d,e,f)             ICOM_CALL6(CreateIndexBuffer,p,a,b,c,d,e,f)
#define IDirect3DDevice9_CreateRenderTarget(p,a,b,c,d,e,f,g,h)        ICOM_CALL8(CreateRenderTarget,p,a,b,c,d,e,f,g,h)
#define IDirect3DDevice9_CreateDepthStencilSurface(p,a,b,c,d,e,f,g,h) ICOM_CALL8(CreateDepthStencilSurface,p,a,b,c,d,e,f,g,h)
#define IDirect3DDevice9_UpdateSurface(p,a,b,c,d)                     ICOM_CALL4(UpdateSurface,p,a,b,c,d)
#define IDirect3DDevice9_UpdateTexture(p,a,b)                         ICOM_CALL2(UpdateTexture,p,a,b)
#define IDirect3DDevice9_GetRenderTargetData(p,a,b)                   ICOM_CALL2(GetRenderTargetData,p,a,b)
#define IDirect3DDevice9_GetFrontBufferData(p,a,b)                    ICOM_CALL2(GetFrontBufferData,p,a,b)
#define IDirect3DDevice9_StretchRect(p,a,b,c,d,e)                     ICOM_CALL5(StretchRect,p,a,b,c,d,e)
#define IDirect3DDevice9_ColorFill(p,a,b,c)                           ICOM_CALL3(ColorFill,p,a,b,c)
#define IDirect3DDevice9_CreateOffscreenPlainSurface(p,a,b,c,d,e,f)   ICOM_CALL6(CreateOffscreenPlainSurface,p,a,b,c,d,e,f)
#define IDirect3DDevice9_SetRenderTarget(p,a,b)                       ICOM_CALL2(SetRenderTarget,p,a,b)
#define IDirect3DDevice9_GetRenderTarget(p,a,b)                       ICOM_CALL2(GetRenderTarget,p,a,b)
#define IDirect3DDevice9_SetDepthStencilSurface(p,a)                  ICOM_CALL1(SetDepthStencilSurface,p,a)
#define IDirect3DDevice9_GetDepthStencilSurface(p,a)                  ICOM_CALL1(GetDepthStencilSurface,p,a)
#define IDirect3DDevice9_BeginScene(p)                                ICOM_CALL (BeginScene,p)
#define IDirect3DDevice9_EndScene(p)                                  ICOM_CALL (EndScene,p)
#define IDirect3DDevice9_Clear(p,a,b,c,d,e,f)                         ICOM_CALL6(Clear,p,a,b,c,d,e,f)
#define IDirect3DDevice9_SetTransform(p,a,b)                          ICOM_CALL2(SetTransform,p,a,b)
#define IDirect3DDevice9_GetTransform(p,a,b)                          ICOM_CALL2(GetTransform,p,a,b)
#define IDirect3DDevice9_MultiplyTransform(p,a,b)                     ICOM_CALL2(MultiplyTransform,p,a,b)
#define IDirect3DDevice9_SetViewport(p,a)                             ICOM_CALL1(SetViewport,p,a)
#define IDirect3DDevice9_GetViewport(p,a)                             ICOM_CALL1(GetViewport,p,a)
#define IDirect3DDevice9_SetMaterial(p,a)                             ICOM_CALL1(SetMaterial,p,a)
#define IDirect3DDevice9_GetMaterial(p,a)                             ICOM_CALL1(GetMaterial,p,a)
#define IDirect3DDevice9_SetLight(p,a,b)                              ICOM_CALL2(SetLight,p,a,b)
#define IDirect3DDevice9_GetLight(p,a,b)                              ICOM_CALL2(GetLight,p,a,b)
#define IDirect3DDevice9_LightEnable(p,a,b)                           ICOM_CALL2(LightEnable,p,a,b)
#define IDirect3DDevice9_GetLightEnable(p,a,b)                        ICOM_CALL2(GetLightEnable,p,a,b)
#define IDirect3DDevice9_SetClipPlane(p,a,b)                          ICOM_CALL2(SetClipPlane,p,a,b)
#define IDirect3DDevice9_GetClipPlane(p,a,b)                          ICOM_CALL2(GetClipPlane,p,a,b)
#define IDirect3DDevice9_SetRenderState(p,a,b)                        ICOM_CALL2(SetRenderState,p,a,b)
#define IDirect3DDevice9_GetRenderState(p,a,b)                        ICOM_CALL2(GetRenderState,p,a,b)
#define IDirect3DDevice9_CreateStateBlock(p,a,b)                      ICOM_CALL2(CreateStateBlock,p,a,b)
#define IDirect3DDevice9_BeginStateBlock(p)                           ICOM_CALL (BeginStateBlock,p)
#define IDirect3DDevice9_EndStateBlock(p,a)                           ICOM_CALL1(EndStateBlock,p,a)
#define IDirect3DDevice9_SetClipStatus(p,a)                           ICOM_CALL1(SetClipStatus,p,a)
#define IDirect3DDevice9_GetClipStatus(p,a)                           ICOM_CALL1(GetClipStatus,p,a)
#define IDirect3DDevice9_GetTexture(p,a,b)                            ICOM_CALL2(GetTexture,p,a,b)
#define IDirect3DDevice9_SetTexture(p,a,b)                            ICOM_CALL2(SetTexture,p,a,b)
#define IDirect3DDevice9_GetTextureStageState(p,a,b,c)                ICOM_CALL3(GetTextureStageState,p,a,b,c)
#define IDirect3DDevice9_SetTextureStageState(p,a,b,c)                ICOM_CALL3(SetTextureStageState,p,a,b,c)
#define IDirect3DDevice9_GetSamplerState(p,a,b,c)                     ICOM_CALL3(GetSamplerState,p,a,b,c)
#define IDirect3DDevice9_SetSamplerState(p,a,b,c)                     ICOM_CALL3(SetSamplerState,p,a,b,c)
#define IDirect3DDevice9_ValidateDevice(p,a)                          ICOM_CALL1(ValidateDevice,p,a)
#define IDirect3DDevice9_SetPaletteEntries(p,a,b)                     ICOM_CALL2(SetPaletteEntries,p,a,b)
#define IDirect3DDevice9_GetPaletteEntries(p,a,b)                     ICOM_CALL2(GetPaletteEntries,p,a,b)
#define IDirect3DDevice9_SetCurrentTexturePalette(p,a)                ICOM_CALL1(SetCurrentTexturePalette,p,a)
#define IDirect3DDevice9_GetCurrentTexturePalette(p,a)                ICOM_CALL1(GetCurrentTexturePalette,p,a)
#define IDirect3DDevice9_SetScissorRect(p,a)                          ICOM_CALL1(SetScissorRect,p,a)
#define IDirect3DDevice9_GetScissorRect(p,a)                          ICOM_CALL1(GetScissorRect,p,a)
#define IDirect3DDevice9_SetSoftwareVertexProcessing(p,a)             ICOM_CALL1(SetSoftwareVertexProcessing,p,a)
#define IDirect3DDevice9_GetSoftwareVertexProcessing(p)               ICOM_CALL (GetSoftwareVertexProcessing,p)
#define IDirect3DDevice9_SetNPatchMode(p,a)                           ICOM_CALL1(SetNPatchMode,p,a)
#define IDirect3DDevice9_GetNPatchMode(p)                             ICOM_CALL (GetNPatchMode,p)
#define IDirect3DDevice9_DrawPrimitive(p,a,b,c)                       ICOM_CALL3(DrawPrimitive,p,a,b,c)
#define IDirect3DDevice9_DrawIndexedPrimitive(p,a,b,c,d,e,f)          ICOM_CALL6(DrawIndexedPrimitive,p,a,b,c,d,e,f)
#define IDirect3DDevice9_DrawPrimitiveUP(p,a,b,c,d)                   ICOM_CALL4(DrawPrimitiveUP,p,a,b,c,d)
#define IDirect3DDevice9_DrawIndexedPrimitiveUP(p,a,b,c,d,e,f,g,h)    ICOM_CALL8(DrawIndexedPrimitiveUP,p,a,b,c,d,e,f,g,h)
#define IDirect3DDevice9_ProcessVertices(p,a,b,c,d,e,f)               ICOM_CALL6(ProcessVertices,p,a,b,c,d,e,f)
#define IDirect3DDevice9_CreateVertexDeclaration(p,a,b)               ICOM_CALL2(CreateVertexDeclaration,p,a,b)
#define IDirect3DDevice9_SetVertexDeclaration(p,a)                    ICOM_CALL1(SetVertexDeclaration,p,a)
#define IDirect3DDevice9_GetVertexDeclaration(p,a)                    ICOM_CALL1(GetVertexDeclaration,p,a)
#define IDirect3DDevice9_SetFVF(p,a)                                  ICOM_CALL1(SetFVF,p,a)
#define IDirect3DDevice9_GetFVF(p,a)                                  ICOM_CALL1(GetFVF,p,a)
#define IDirect3DDevice9_CreateVertexShader(p,a,b)                    ICOM_CALL2(CreateVertexShader,p,a,b)
#define IDirect3DDevice9_SetVertexShader(p,a)                         ICOM_CALL1(SetVertexShader,p,a)
#define IDirect3DDevice9_GetVertexShader(p,a)                         ICOM_CALL1(GetVertexShader,p,a)
#define IDirect3DDevice9_SetVertexShaderConstantF(p,a,b,c)            ICOM_CALL3(SetVertexShaderConstantF,p,a,b,c)
#define IDirect3DDevice9_GetVertexShaderConstantF(p,a,b,c)            ICOM_CALL3(GetVertexShaderConstantF,p,a,b,c)
#define IDirect3DDevice9_SetVertexShaderConstantI(p,a,b,c)            ICOM_CALL3(SetVertexShaderConstantI,p,a,b,c)
#define IDirect3DDevice9_GetVertexShaderConstantI(p,a,b,c)            ICOM_CALL3(GetVertexShaderConstantI,p,a,b,c)
#define IDirect3DDevice9_SetVertexShaderConstantB(p,a,b,c)            ICOM_CALL3(SetVertexShaderConstantB,p,a,b,c)
#define IDirect3DDevice9_GetVertexShaderConstantB(p,a,b,c)            ICOM_CALL3(GetVertexShaderConstantB,p,a,b,c)
#define IDirect3DDevice9_SetStreamSource(p,a,b,c,d)                   ICOM_CALL4(SetStreamSource,p,a,b,c,d)
#define IDirect3DDevice9_GetStreamSource(p,a,b,c,d)                   ICOM_CALL4(GetStreamSource,p,a,b,c,d)
#define IDirect3DDevice9_SetStreamSourceFreq(p,a,b)                   ICOM_CALL2(SetStreamSourceFreq,p,a,b)
#define IDirect3DDevice9_GetStreamSourceFreq(p,a,b)                   ICOM_CALL2(GetStreamSourceFreq,p,a,b)
#define IDirect3DDevice9_SetIndices(p,a)                              ICOM_CALL1(SetIndices,p,a)
#define IDirect3DDevice9_GetIndices(p,a)                              ICOM_CALL1(GetIndices,p,a)
#define IDirect3DDevice9_CreatePixelShader(p,a,b)                     ICOM_CALL2(CreatePixelShader,p,a,b)
#define IDirect3DDevice9_SetPixelShader(p,a)                          ICOM_CALL1(SetPixelShader,p,a)
#define IDirect3DDevice9_GetPixelShader(p,a)                          ICOM_CALL1(GetPixelShader,p,a)
#define IDirect3DDevice9_SetPixelShaderConstantF(p,a,b,c)             ICOM_CALL3(SetPixelShaderConstantF,p,a,b,c)
#define IDirect3DDevice9_GetPixelShaderConstantF(p,a,b,c)             ICOM_CALL3(GetPixelShaderConstantF,p,a,b,c)
#define IDirect3DDevice9_SetPixelShaderConstantI(p,a,b,c)             ICOM_CALL3(SetPixelShaderConstantI,p,a,b,c)
#define IDirect3DDevice9_GetPixelShaderConstantI(p,a,b,c)             ICOM_CALL3(GetPixelShaderConstantI,p,a,b,c)
#define IDirect3DDevice9_SetPixelShaderConstantB(p,a,b,c)             ICOM_CALL3(SetPixelShaderConstantB,p,a,b,c)
#define IDirect3DDevice9_GetPixelShaderConstantB(p,a,b,c)             ICOM_CALL3(GetPixelShaderConstantB,p,a,b,c)
#define IDirect3DDevice9_DrawRectPatch(p,a,b,c)                       ICOM_CALL3(DrawRectPatch,p,a,b,c)
#define IDirect3DDevice9_DrawTriPatch(p,a,b,c)                        ICOM_CALL3(DrawTriPatch,p,a,b,c)
#define IDirect3DDevice9_DeletePatch(p,a)                             ICOM_CALL1(DeletePatch,p,a)
#define IDirect3DDevice9_CreateQuery(p,a,b)                           ICOM_CALL2(CreateQuery,p,a,b)


/*****************************************************************************
 * IDirect3DStateBlock9 interface
 */
#define ICOM_INTERFACE IDirect3DStateBlock9
#define IDirect3DStateBlock9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD (HRESULT,Capture) \
    ICOM_METHOD (HRESULT,Apply)
#define IDirect3DStateBlock9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DStateBlock9_METHODS
ICOM_DEFINE(IDirect3DStateBlock9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DStateBlock9_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DStateBlock9_AddRef(p)             ICOM_CALL (AddRef,p)
#define IDirect3DStateBlock9_Release(p)            ICOM_CALL (Release,p)
	/*** IDirect3DStateBlock9 methods ***/
#define IDirect3DStateBlock9_GetDevice(p,a)        ICOM_CALL1(GetDevice,p,a)
#define IDirect3DStateBlock9_Capture(p)            ICOM_CALL (Capture,p)
#define IDirect3DStateBlock9_Apply(p)              ICOM_CALL (Apply,p)


/*****************************************************************************
 * IDirect3DSwapChain9 interface
 */
#define ICOM_INTERFACE IDirect3DSwapChain9
#define IDirect3DSwapChain9_METHODS \
    ICOM_METHOD5(HRESULT,Present,             CONST RECT*,pSourceRect,CONST RECT*,pDestRect,HWND,hDestWindowOverride,CONST RGNDATA*,pDirtyRegion,DWORD,dwFlags) \
    ICOM_METHOD1(HRESULT,GetFrontBufferData,  IDirect3DSurface9*,pDestSurface) \
    ICOM_METHOD3(HRESULT,GetBackBuffer,       UINT,BackBuffer,D3DBACKBUFFER_TYPE,Type,IDirect3DSurface9**,ppBackBuffer) \
    ICOM_METHOD1(HRESULT,GetRasterStatus,     D3DRASTER_STATUS*,pRasterStatus) \
    ICOM_METHOD1(HRESULT,GetDisplayMode,      D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD1(HRESULT,GetDevice,           IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD1(HRESULT,GetPresentParameters,D3DPRESENT_PARAMETERS*,pPresentationParameters)
#define IDirect3DSwapChain9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DSwapChain9_METHODS
ICOM_DEFINE(IDirect3DSwapChain9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DSwapChain9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DSwapChain9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DSwapChain9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DSwapChain9 methods ***/
#define IDirect3DSwapChain9_Present(p,a,b,c,d,e)      ICOM_CALL5(Present,p,a,b,c,d,e)
#define IDirect3DSwapChain9_GetFrontBufferData(p,a)   ICOM_CALL1(GetFrontBufferData,p,a)
#define IDirect3DSwapChain9_GetBackBuffer(p,a,b,c)    ICOM_CALL3(GetBackBuffer,p,a,b,c)
#define IDirect3DSwapChain9_GetRasterStatus(p,a)      ICOM_CALL1(GetRasterStatus,p,a)
#define IDirect3DSwapChain9_GetDisplayMode(p,a)       ICOM_CALL1(GetDisplayMode,p,a)
#define IDirect3DSwapChain9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DSwapChain9_GetPresentParameters(p,a) ICOM_CALL1(GetPresentParameters,p,a)


/*****************************************************************************
 * IDirect3DResource9 interface
 */
#define ICOM_INTERFACE IDirect3DResource9
#define IDirect3DResource9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,      IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD4(HRESULT,SetPrivateData, REFGUID,refguid,CONST void*,pData,DWORD,SizeOfData,DWORD,Flags) \
    ICOM_METHOD3(HRESULT,GetPrivateData, REFGUID,refguid,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD1(HRESULT,FreePrivateData,REFGUID,refguid) \
    ICOM_METHOD1(DWORD,  SetPriority,    DWORD,PriorityNew) \
    ICOM_METHOD (DWORD,  GetPriority) \
    ICOM_VMETHOD(        PreLoad) \
    ICOM_METHOD (D3DRESOURCETYPE,GetType)
#define IDirect3DResource9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DResource9_METHODS
ICOM_DEFINE(IDirect3DResource9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DResource9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DResource9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DResource9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DResource9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DResource9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DResource9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DResource9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DResource9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DResource9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DResource9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DResource9_GetType(p)                ICOM_CALL (GetType,p)


/*****************************************************************************
 * IDirect3DVertexDeclaration9 interface
 */
#define ICOM_INTERFACE IDirect3DVertexDeclaration9
#define IDirect3DVertexDeclaration9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,     IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD2(HRESULT,GetDeclaration,D3DVERTEXELEMENT9*,,UINT*,pNumElements)
#define IDirect3DVertexDeclaration9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DVertexDeclaration9_METHODS
ICOM_DEFINE(IDirect3DVertexDeclaration9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVertexDeclaration9_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVertexDeclaration9_AddRef(p)             ICOM_CALL (AddRef,p)
#define IDirect3DVertexDeclaration9_Release(p)            ICOM_CALL (Release,p)
	/*** IDirect3DVertexDeclaration9 methods ***/
#define IDirect3DVertexDeclaration9_GetDevice(p,a)        ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVertexDeclaration9_GetDeclaration(p,a,b) ICOM_CALL2(GetDeclaration,p,a,b)


/*****************************************************************************
 * IDirect3DVertexShader9 interface
 */
#define ICOM_INTERFACE IDirect3DVertexShader9
#define IDirect3DVertexShader9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,  IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD2(HRESULT,GetFunction,void*,,UINT*,pSizeOfData)
#define IDirect3DVertexShader9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DVertexShader9_METHODS
ICOM_DEFINE(IDirect3DVertexShader9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVertexShader9_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVertexShader9_AddRef(p)             ICOM_CALL (AddRef,p)
#define IDirect3DVertexShader9_Release(p)            ICOM_CALL (Release,p)
	/*** IDirect3DVertexShader9 methods ***/
#define IDirect3DVertexShader9_GetDevice(p,a)        ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVertexShader9_GetFunction(p,a,b)    ICOM_CALL2(GetFunction,p,a,b)


/*****************************************************************************
 * IDirect3DPixelShader9 interface
 */
#define ICOM_INTERFACE IDirect3DPixelShader9
#define IDirect3DPixelShader9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,  IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD2(HRESULT,GetFunction,void*,,UINT*,pSizeOfData)
#define IDirect3DPixelShader9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DPixelShader9_METHODS
ICOM_DEFINE(IDirect3DPixelShader9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DPixelShader9_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DPixelShader9_AddRef(p)             ICOM_CALL (AddRef,p)
#define IDirect3DPixelShader9_Release(p)            ICOM_CALL (Release,p)
	/*** IDirect3DPixelShader9 methods ***/
#define IDirect3DPixelShader9_GetDevice(p,a)        ICOM_CALL1(GetDevice,p,a)
#define IDirect3DPixelShader9_GetFunction(p,a,b)    ICOM_CALL2(GetFunction,p,a,b)


/*****************************************************************************
 * IDirect3DBaseTexture9 interface
 */
#define ICOM_INTERFACE IDirect3DBaseTexture9
#define IDirect3DBaseTexture9_METHODS \
    ICOM_METHOD1(DWORD,  SetLOD,DWORD,LODNew) \
    ICOM_METHOD (DWORD,  GetLOD) \
    ICOM_METHOD (DWORD,  GetLevelCount) \
    ICOM_METHOD1(HRESULT,SetAutoGenFilterType,D3DTEXTUREFILTERTYPE,FilterType) \
    ICOM_METHOD (D3DTEXTUREFILTERTYPE,GetAutoGenFilterType) \
    ICOM_VMETHOD(        GenerateMipSubLevels)
#define IDirect3DBaseTexture9_IMETHODS \
    IDirect3DResource9_IMETHODS \
    IDirect3DBaseTexture9_METHODS
ICOM_DEFINE(IDirect3DBaseTexture9,IDirect3DResource9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DBaseTexture9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DBaseTexture9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DBaseTexture9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DBaseTexture9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DBaseTexture9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DBaseTexture9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DBaseTexture9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DBaseTexture9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DBaseTexture9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DBaseTexture9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DBaseTexture9_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture9 methods ***/
#define IDirect3DBaseTexture9_SetLOD(p,a)               ICOM_CALL1(SetLOD,p,a)
#define IDirect3DBaseTexture9_GetLOD(p)                 ICOM_CALL (GetLOD,p)
#define IDirect3DBaseTexture9_GetLevelCount(p)          ICOM_CALL (GetLevelCount,p)
#define IDirect3DBaseTexture9_SetAutoGenFilterType(p,a) ICOM_CALL1(SetAutoGenFilterType,p,a)
#define IDirect3DBaseTexture9_GetAutoGenFilterType(p)   ICOM_CALL (GetAutoGenFilterType,p)
#define IDirect3DBaseTexture9_GenerateMipSubLevels(p)   ICOM_CALL (GenerateMipSubLevels,p)


/*****************************************************************************
 * IDirect3DTexture9 interface
 */
#define ICOM_INTERFACE IDirect3DTexture9
#define IDirect3DTexture9_METHODS \
    ICOM_METHOD2(HRESULT,GetLevelDesc,   UINT,Level,D3DSURFACE_DESC*,pDesc) \
    ICOM_METHOD2(HRESULT,GetSurfaceLevel,UINT,Level,IDirect3DSurface9**,ppSurfaceLevel) \
    ICOM_METHOD4(HRESULT,LockRect,       UINT,Level,D3DLOCKED_RECT*,pLockedRect,CONST RECT*,pRect,DWORD,Flags) \
    ICOM_METHOD1(HRESULT,UnlockRect,     UINT,Level) \
    ICOM_METHOD1(HRESULT,AddDirtyRect,   CONST RECT*,pDirtyRect)
#define IDirect3DTexture9_IMETHODS \
    IDirect3DBaseTexture9_IMETHODS \
    IDirect3DTexture9_METHODS
ICOM_DEFINE(IDirect3DTexture9,IDirect3DBaseTexture9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DTexture9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DTexture9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DTexture9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DTexture9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DTexture9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DTexture9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DTexture9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DTexture9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DTexture9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DTexture9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DTexture9_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture9 methods ***/
#define IDirect3DTexture9_SetLOD(p,a)               ICOM_CALL1(SetLOD,p,a)
#define IDirect3DTexture9_GetLOD(p)                 ICOM_CALL (GetLOD,p)
#define IDirect3DTexture9_GetLevelCount(p)          ICOM_CALL (GetLevelCount,p)
#define IDirect3DTexture9_SetAutoGenFilterType(p,a) ICOM_CALL1(SetAutoGenFilterType,p,a)
#define IDirect3DTexture9_GetAutoGenFilterType(p)   ICOM_CALL (GetAutoGenFilterType,p)
#define IDirect3DTexture9_GenerateMipSubLevels(p)   ICOM_CALL (GenerateMipSubLevels,p)
	/*** IDirect3DTexture9 methods ***/
#define IDirect3DTexture9_GetLevelDesc(p,a,b)       ICOM_CALL2(GetLevelDesc,p,a,b)
#define IDirect3DTexture9_GetSurfaceLevel(p,a,b)    ICOM_CALL2(GetSurfaceLevel,p,a,b)
#define IDirect3DTexture9_LockRect(p,a,b,c,d)       ICOM_CALL4(LockRect,p,a,b,c,d)
#define IDirect3DTexture9_UnlockRect(p,a)           ICOM_CALL1(UnlockRect,p,a)
#define IDirect3DTexture9_AddDirtyRect(p,a)         ICOM_CALL1(AddDirtyRect,p,a)


/*****************************************************************************
 * IDirect3DVolumeTexture9 interface
 */
#define ICOM_INTERFACE IDirect3DVolumeTexture9
#define IDirect3DVolumeTexture9_METHODS \
    ICOM_METHOD2(HRESULT,GetLevelDesc,  UINT,Level,D3DVOLUME_DESC*,pDesc) \
    ICOM_METHOD2(HRESULT,GetVolumeLevel,UINT,Level,IDirect3DVolume9**,ppVolumeLevel) \
    ICOM_METHOD4(HRESULT,LockBox,       UINT,Level,D3DLOCKED_BOX*,pLockedVolume,CONST D3DBOX*,pBox,DWORD,Flags) \
    ICOM_METHOD1(HRESULT,UnlockBox,     UINT,Level) \
    ICOM_METHOD1(HRESULT,AddDirtyBox,   CONST D3DBOX*,pDirtyBox)
#define IDirect3DVolumeTexture9_IMETHODS \
    IDirect3DBaseTexture9_IMETHODS \
    IDirect3DVolumeTexture9_METHODS
ICOM_DEFINE(IDirect3DVolumeTexture9,IDirect3DBaseTexture9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVolumeTexture9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVolumeTexture9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DVolumeTexture9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DVolumeTexture9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVolumeTexture9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DVolumeTexture9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DVolumeTexture9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DVolumeTexture9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DVolumeTexture9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DVolumeTexture9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DVolumeTexture9_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture9 methods ***/
#define IDirect3DVolumeTexture9_SetLOD(p,a)               ICOM_CALL1(SetLOD,p,a)
#define IDirect3DVolumeTexture9_GetLOD(p)                 ICOM_CALL (GetLOD,p)
#define IDirect3DVolumeTexture9_GetLevelCount(p)          ICOM_CALL (GetLevelCount,p)
#define IDirect3DVolumeTexture9_SetAutoGenFilterType(p,a) ICOM_CALL1(SetAutoGenFilterType,p,a)
#define IDirect3DVolumeTexture9_GetAutoGenFilterType(p)   ICOM_CALL (GetAutoGenFilterType,p)
#define IDirect3DVolumeTexture9_GenerateMipSubLevels(p)   ICOM_CALL (GenerateMipSubLevels,p)
	/*** IDirect3DVolumeTexture9 methods ***/
#define IDirect3DVolumeTexture9_GetLevelDesc(p,a,b)       ICOM_CALL2(GetLevelDesc,p,a,b)
#define IDirect3DVolumeTexture9_GetVolumeLevel(p,a,b)     ICOM_CALL2(GetVolumeLevel,p,a,b)
#define IDirect3DVolumeTexture9_LockBox(p,a,b,c,d)        ICOM_CALL4(LockBox,p,a,b,c,d)
#define IDirect3DVolumeTexture9_UnlockBox(p,a)            ICOM_CALL1(UnlockBox,p,a)
#define IDirect3DVolumeTexture9_AddDirtyBox(p,a)          ICOM_CALL1(AddDirtyBox,p,a)


/*****************************************************************************
 * IDirect3DCubeTexture9 interface
 */
#define ICOM_INTERFACE IDirect3DCubeTexture9
#define IDirect3DCubeTexture9_METHODS \
    ICOM_METHOD2(HRESULT,GetLevelDesc,     UINT,Level,D3DSURFACE_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,GetCubeMapSurface,D3DCUBEMAP_FACES,FaceType,UINT,Level,IDirect3DSurface9**,ppCubeMapSurface) \
    ICOM_METHOD5(HRESULT,LockRect,         D3DCUBEMAP_FACES,FaceType,UINT,Level,D3DLOCKED_RECT*,pLockedRect,CONST RECT*,pRect,DWORD,Flags) \
    ICOM_METHOD2(HRESULT,UnlockRect,       D3DCUBEMAP_FACES,FaceType,UINT,Level) \
    ICOM_METHOD2(HRESULT,AddDirtyRect,     D3DCUBEMAP_FACES,FaceType,CONST RECT*,pDirtyRect)
#define IDirect3DCubeTexture9_IMETHODS \
    IDirect3DBaseTexture9_IMETHODS \
    IDirect3DCubeTexture9_METHODS
ICOM_DEFINE(IDirect3DCubeTexture9,IDirect3DBaseTexture9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DCubeTexture9_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DCubeTexture9_AddRef(p)                  ICOM_CALL (AddRef,p)
#define IDirect3DCubeTexture9_Release(p)                 ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DCubeTexture9_GetDevice(p,a)             ICOM_CALL1(GetDevice,p,a)
#define IDirect3DCubeTexture9_SetPrivateData(p,a,b,c,d)  ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DCubeTexture9_GetPrivateData(p,a,b,c)    ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DCubeTexture9_FreePrivateData(p,a)       ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DCubeTexture9_SetPriority(p,a)           ICOM_CALL1(SetPriority,p,a)
#define IDirect3DCubeTexture9_GetPriority(p)             ICOM_CALL (GetPriority,p)
#define IDirect3DCubeTexture9_PreLoad(p)                 ICOM_CALL (PreLoad,p)
#define IDirect3DCubeTexture9_GetType(p)                 ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture9 methods ***/
#define IDirect3DCubeTexture9_SetLOD(p,a)                ICOM_CALL1(SetLOD,p,a)
#define IDirect3DCubeTexture9_GetLOD(p)                  ICOM_CALL (GetLOD,p)
#define IDirect3DCubeTexture9_GetLevelCount(p)           ICOM_CALL (GetLevelCount,p)
#define IDirect3DCubeTexture9_SetAutoGenFilterType(p,a)  ICOM_CALL1(SetAutoGenFilterType,p,a)
#define IDirect3DCubeTexture9_GetAutoGenFilterType(p)    ICOM_CALL (GetAutoGenFilterType,p)
#define IDirect3DCubeTexture9_GenerateMipSubLevels(p)    ICOM_CALL (GenerateMipSubLevels,p)
	/*** IDirect3DCubeTexture9 methods ***/
#define IDirect3DCubeTexture9_GetLevelDesc(p,a,b)        ICOM_CALL2(GetLevelDesc,p,a,b)
#define IDirect3DCubeTexture9_GetCubeMapSurface(p,a,b,c) ICOM_CALL3(GetCubeMapSurface,p,a,b,c)
#define IDirect3DCubeTexture9_LockRect(p,a,b,c,d,e)      ICOM_CALL4(LockRect,p,a,b,c,d,e)
#define IDirect3DCubeTexture9_UnlockRect(p,a,b)          ICOM_CALL1(UnlockRect,p,a,b)
#define IDirect3DCubeTexture9_AddDirtyRect(p,a,b)        ICOM_CALL1(AddDirtyRect,p,a,b)


/*****************************************************************************
 * IDirect3DVertexBuffer9 interface
 */
#define ICOM_INTERFACE IDirect3DVertexBuffer9
#define IDirect3DVertexBuffer9_METHODS \
    ICOM_METHOD4(HRESULT,Lock,   UINT,OffsetToLock,UINT,SizeToLock,void**,ppbData,DWORD,Flags) \
    ICOM_METHOD (HRESULT,Unlock) \
    ICOM_METHOD1(HRESULT,GetDesc,D3DVERTEXBUFFER_DESC*,pDesc)
#define IDirect3DVertexBuffer9_IMETHODS \
    IDirect3DResource9_IMETHODS \
    IDirect3DVertexBuffer9_METHODS
ICOM_DEFINE(IDirect3DVertexBuffer9,IDirect3DResource9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVertexBuffer9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVertexBuffer9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DVertexBuffer9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DVertexBuffer9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVertexBuffer9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DVertexBuffer9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DVertexBuffer9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DVertexBuffer9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DVertexBuffer9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DVertexBuffer9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DVertexBuffer9_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DVertexBuffer9 methods ***/
#define IDirect3DVertexBuffer9_Lock(p,a,b,c,d)           ICOM_CALL4(Lock,p,a,b,c,d)
#define IDirect3DVertexBuffer9_Unlock(p)                 ICOM_CALL (Unlock,p)
#define IDirect3DVertexBuffer9_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)


/*****************************************************************************
 * IDirect3DIndexBuffer9 interface
 */
#define ICOM_INTERFACE IDirect3DIndexBuffer9
#define IDirect3DIndexBuffer9_METHODS \
    ICOM_METHOD4(HRESULT,Lock,   UINT,OffsetToLock,UINT,SizeToLock,void**,ppbData,DWORD,Flags) \
    ICOM_METHOD (HRESULT,Unlock) \
    ICOM_METHOD1(HRESULT,GetDesc,D3DINDEXBUFFER_DESC*,pDesc)
#define IDirect3DIndexBuffer9_IMETHODS \
    IDirect3DResource9_IMETHODS \
    IDirect3DIndexBuffer9_METHODS
ICOM_DEFINE(IDirect3DIndexBuffer9,IDirect3DResource9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DIndexBuffer9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DIndexBuffer9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DIndexBuffer9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DIndexBuffer9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DIndexBuffer9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DIndexBuffer9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DIndexBuffer9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DIndexBuffer9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DIndexBuffer9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DIndexBuffer9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DIndexBuffer9_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DIndexBuffer9 methods ***/
#define IDirect3DIndexBuffer9_Lock(p,a,b,c,d)           ICOM_CALL4(Lock,p,a,b,c,d)
#define IDirect3DIndexBuffer9_Unlock(p)                 ICOM_CALL (Unlock,p)
#define IDirect3DIndexBuffer9_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)


/*****************************************************************************
 * IDirect3DSurface9 interface
 */
#define ICOM_INTERFACE IDirect3DSurface9
#define IDirect3DSurface9_METHODS \
    ICOM_METHOD2(HRESULT,GetContainer,   REFIID,riid,void**,ppContainer) \
    ICOM_METHOD1(HRESULT,GetDesc,        D3DSURFACE_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,LockRect,       D3DLOCKED_RECT*,pLockedRect,CONST RECT*,pRect,DWORD,Flags) \
    ICOM_METHOD (HRESULT,UnlockRect) \
    ICOM_METHOD1(HRESULT,GetDC,          HDC*,phdc) \
    ICOM_METHOD1(HRESULT,ReleaseDC,      HDC,hdc)
#define IDirect3DSurface9_IMETHODS \
    IDirect3DResource9_IMETHODS \
    IDirect3DSurface9_METHODS
ICOM_DEFINE(IDirect3DSurface9,IDirect3DResource9)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DSurface9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DSurface9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DSurface9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource9 methods ***/
#define IDirect3DSurface9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DSurface9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DSurface9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DSurface9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DSurface9_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DSurface9_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DSurface9_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DSurface9_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DSurface9 methods ***/
#define IDirect3DSurface9_GetContainer(p,a,b)       ICOM_CALL2(GetContainer,p,a,b)
#define IDirect3DSurface9_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)
#define IDirect3DSurface9_LockRect(p,a,b,c)         ICOM_CALL3(LockRect,p,a,b,c)
#define IDirect3DSurface9_UnlockRect(p)             ICOM_CALL (UnlockRect,p)
#define IDirect3DSurface9_GetDC(p,a)                ICOM_CALL1(GetDC,p,a)
#define IDirect3DSurface9_ReleaseDC(p,a)            ICOM_CALL1(ReleaseDC,p,a)


/*****************************************************************************
 * IDirect3DVolume9 interface
 */
#define ICOM_INTERFACE IDirect3DVolume9
#define IDirect3DVolume9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,      IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD4(HRESULT,SetPrivateData, REFGUID,refguid,CONST void*,pData,DWORD,SizeOfData,DWORD,Flags) \
    ICOM_METHOD3(HRESULT,GetPrivateData, REFGUID,refguid,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD1(HRESULT,FreePrivateData,REFGUID,refguid) \
    ICOM_METHOD2(HRESULT,GetContainer,   REFIID,riid,void**,ppContainer) \
    ICOM_METHOD1(HRESULT,GetDesc,        D3DVOLUME_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,LockBox,        D3DLOCKED_BOX*,pLockedVolume,CONST D3DBOX*,pBox,DWORD,Flags) \
    ICOM_METHOD (HRESULT,UnlockBox)
#define IDirect3DVolume9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DVolume9_METHODS
ICOM_DEFINE(IDirect3DVolume9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVolume9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVolume9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DVolume9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DVolume9 methods ***/
#define IDirect3DVolume9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVolume9_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DVolume9_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DVolume9_FreePrivateData(p,a)      ICOM_CALL1(FreePrivateData,p,a)
#define IDirect3DVolume9_GetContainer(p,a,b)       ICOM_CALL2(GetContainer,p,a,b)
#define IDirect3DVolume9_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)
#define IDirect3DVolume9_LockBox(p,a,b,c)          ICOM_CALL3(LockBox,p,a,b,c)
#define IDirect3DVolume9_UnlockBox(p)              ICOM_CALL (UnlockBox,p)


/*****************************************************************************
 * IDirect3DQuery9 interface
 */
#define ICOM_INTERFACE IDirect3DQuery9
#define IDirect3DQuery9_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,      IDirect3DDevice9**,ppDevice) \
    ICOM_METHOD (D3DQUERYTYPE,GetType) \
    ICOM_METHOD (DWORD,  GetDataSize) \
    ICOM_METHOD1(HRESULT,Issue,          DWORD,dwIssueFlags) \
    ICOM_METHOD3(HRESULT,GetData,        void*,pData,DWORD,dwSize,DWORD,dwGetDataFlags)
#define IDirect3DQuery9_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DQuery9_METHODS
ICOM_DEFINE(IDirect3DQuery9,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DQuery9_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DQuery9_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DQuery9_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DQuery9 methods ***/
#define IDirect3DQuery9_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DQuery9_GetType(p)                ICOM_CALL (GetType,p)
#define IDirect3DQuery9_GetDataSize(p)            ICOM_CALL (GetDataSize,p)
#define IDirect3DQuery9_Issue(p,a)                ICOM_CALL1(Issue,p,a)
#define IDirect3DQuery9_GetData(p,a,b,c)          ICOM_CALL3(GetData,p,a,b,c)


IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_D3D9_H */
