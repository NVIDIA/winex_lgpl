#ifndef __WINE_D3D8_H
#define __WINE_D3D8_H

#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0800
#endif

/*****************************************************************************
 * Predeclare the interfaces
 */
DEFINE_GUID(IID_IDirect3D8,             0x1dd9e8da,0x1c77,0x4d40,0xb0,0xcf,0x98,0xfe,0xfd,0xff,0x95,0x12);
DEFINE_GUID(IID_IDirect3DDevice8,       0x7385e5df,0x8fe8,0x41d5,0x86,0xb6,0xd7,0xb4,0x85,0x47,0xb6,0xcf);
DEFINE_GUID(IID_IDirect3DResource8,     0x1b36bb7b,0x09b7,0x410a,0xb4,0x45,0x7d,0x14,0x30,0xd7,0xb3,0x3f);
DEFINE_GUID(IID_IDirect3DBaseTexture8,  0xb4211cfa,0x51b9,0x4a9f,0xab,0x78,0xdb,0x99,0xb2,0xbb,0x67,0x8e);
DEFINE_GUID(IID_IDirect3DTexture8,      0xe4cdd575,0x2866,0x4f01,0xb1,0x2e,0x7e,0xec,0xe1,0xec,0x93,0x58);
DEFINE_GUID(IID_IDirect3DCubeTexture8,  0x3ee5b968,0x2aca,0x4c34,0x8b,0xb5,0x7e,0x0c,0x3d,0x19,0xb7,0x50);
DEFINE_GUID(IID_IDirect3DVolumeTexture8,0x4b8aaafa,0x140f,0x42ba,0x91,0x31,0x59,0x7e,0xaf,0xaa,0x2e,0xad);
DEFINE_GUID(IID_IDirect3DVertexBuffer8, 0x8aeeeac7,0x05f9,0x44d4,0xb5,0x91,0x00,0x0b,0x0d,0xf1,0xcb,0x95);
DEFINE_GUID(IID_IDirect3DIndexBuffer8,  0x0e689c9a,0x053d,0x44a0,0x9d,0x92,0xdb,0x0e,0x3d,0x75,0x0f,0x86);
DEFINE_GUID(IID_IDirect3DSurface8,      0xb96eebca,0xb326,0x4ea5,0x88,0x2f,0x2f,0xf5,0xba,0xe0,0x21,0xdd);
DEFINE_GUID(IID_IDirect3DVolume8,       0xbd7349f5,0x14f1,0x42e4,0x9c,0x79,0x97,0x23,0x80,0xdb,0x40,0xc0);
DEFINE_GUID(IID_IDirect3DSwapChain8,    0x928c088b,0x76b9,0x4c6b,0xa5,0x36,0xa5,0x90,0x85,0x38,0x76,0xcd);

typedef struct IDirect3D8              IDirect3D8, *LPDIRECT3D8;
typedef struct IDirect3DDevice8        IDirect3DDevice8, *LPDIRECT3DDEVICE8;
typedef struct IDirect3DResource8      IDirect3DResource8, *LPDIRECT3DRESOURCE8;
typedef struct IDirect3DBaseTexture8   IDirect3DBaseTexture8, *LPDIRECT3DBASETEXTURE8;
typedef struct IDirect3DTexture8       IDirect3DTexture8, *LPDIRECT3DTEXTURE8;
typedef struct IDirect3DVolumeTexture8 IDirect3DVolumeTexture8, *LPDIRECT3DVOLUMETEXTURE8;
typedef struct IDirect3DCubeTexture8   IDirect3DCubeTexture8, *LPDIRECT3DCUBETEXTURE8;
typedef struct IDirect3DVertexBuffer8  IDirect3DVertexBuffer8, *LPDIRECT3DVERTEXBUFFER8;
typedef struct IDirect3DIndexBuffer8   IDirect3DIndexBuffer8, *LPDIRECT3DINDEXBUFFER8;
typedef struct IDirect3DSurface8       IDirect3DSurface8, *LPDIRECT3DSURFACE8;
typedef struct IDirect3DVolume8        IDirect3DVolume8, *LPDIRECT3DVOLUME8;
typedef struct IDirect3DSwapChain8     IDirect3DSwapChain8, *LPDIRECT3DSWAPCHAIN8;

#if DIRECT3D_VERSION < 0x0900

#include "windef.h"
#include "wingdi.h"
#include "objbase.h"
#include "d3d8types.h" /* must precede d3d8caps.h */
#include "d3d8caps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define D3D_SDK_VERSION 220

/* ********************************************************************
   Error Codes
   ******************************************************************** */
#define _FACD3D 0x876
#define MAKE_D3DHRESULT(code) MAKE_HRESULT(1, _FACD3D, code)

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

#define D3DADAPTER_DEFAULT            0

#define D3DENUM_NO_WHQL_LEVEL         0x00000002

#define D3DPRESENT_BACK_BUFFERS_MAX   3

#define D3DSGR_NO_CALIBRATION         0x00000000
#define D3DSGR_CALIBRATE              0x00000001

#define D3DCURSOR_IMMEDIATE_UPDATE    0x00000001

/* ********************************************************************
   Types and structures
   ******************************************************************** */


/*****************************************************************************
 * IDirect3D8 interface
 */
#define ICOM_INTERFACE IDirect3D8
#define IDirect3D8_METHODS \
    ICOM_METHOD1(HRESULT,RegisterSoftwareDevice,    void*,pInitializeFunction) \
    ICOM_METHOD (UINT,   GetAdapterCount) \
    ICOM_METHOD3(HRESULT,GetAdapterIdentifier,      UINT,Adapter,DWORD,Flags,D3DADAPTER_IDENTIFIER8*,pIdentifier) \
    ICOM_METHOD1(UINT,   GetAdapterModeCount,       UINT,Adapter) \
    ICOM_METHOD3(HRESULT,EnumAdapterModes,          UINT,Adapter,UINT,Mode,D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD2(HRESULT,GetAdapterDisplayMode,     UINT,Adapter,D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD5(HRESULT,CheckDeviceType,           UINT,Adapter,D3DDEVTYPE,CheckType,D3DFORMAT,DisplayFormat,D3DFORMAT,BackBufferFormat,BOOL,Windowed) \
    ICOM_METHOD6(HRESULT,CheckDeviceFormat,         UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,AdapterFormat,DWORD,Usage,D3DRESOURCETYPE,RType,D3DFORMAT,CheckFormat) \
    ICOM_METHOD5(HRESULT,CheckDeviceMultiSampleType,UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,SurfaceFormat,BOOL,Windowed,D3DMULTISAMPLE_TYPE,MultiSampleType) \
    ICOM_METHOD5(HRESULT,CheckDepthStencilMatch,    UINT,Adapter,D3DDEVTYPE,DeviceType,D3DFORMAT,AdapterFormat,D3DFORMAT,RenderTargetFormat,D3DFORMAT,DepthStencilFormat) \
    ICOM_METHOD3(HRESULT,GetDeviceCaps,             UINT,Adapter,D3DDEVTYPE,DeviceType,D3DCAPS8*,pCaps) \
    ICOM_METHOD1(HMONITOR,GetAdapterMonitor,        UINT,Adapter) \
    ICOM_METHOD6(HRESULT,CreateDevice,              UINT,Adapter,D3DDEVTYPE,DeviceType,HWND,hFocusWindow,DWORD,BehaviorFlags,D3DPRESENT_PARAMETERS*,pPresentationParameters,IDirect3DDevice8**,ppReturnedDeviceInterface)
#define IDirect3D8_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3D8_METHODS
ICOM_DEFINE(IDirect3D8,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3D8_QueryInterface(p,a,b)                   ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3D8_AddRef(p)                               ICOM_CALL (AddRef,p)
#define IDirect3D8_Release(p)                              ICOM_CALL (Release,p)
	/*** IDirect3D8 methods ***/
#define IDirect3D8_RegisterSoftwareDevice(p,a)             ICOM_CALL1(RegisterSoftwareDevice,p,a)
#define IDirect3D8_GetAdapterCount(p)                      ICOM_CALL (GetAdapterCount,p)
#define IDirect3D8_GetAdapterIdentifier(p,a,b,c)           ICOM_CALL3(GetAdapterIdentifier,p,a,b,c)
#define IDirect3D8_GetAdapterModeCount(p,a)                ICOM_CALL1(GetAdapterModeCount,p,a)
#define IDirect3D8_EnumAdapterModes(p,a,b,c)               ICOM_CALL3(EnumAdapterModes,p,a,b,c)
#define IDirect3D8_GetAdapterDisplayMode(p,a,b)            ICOM_CALL2(GetAdapterDisplayMode,p,a,b)
#define IDirect3D8_CheckDeviceType(p,a,b,c,d,e)            ICOM_CALL5(CheckDeviceType,p,a,b,c,d,e)
#define IDirect3D8_CheckDeviceFormat(p,a,b,c,d,e,f)        ICOM_CALL6(CheckDeviceFormat,p,a,b,c,d,e,f)
#define IDirect3D8_CheckDeviceMultiSampleType(p,a,b,c,d,e) ICOM_CALL5(CheckDeviceMultiSampleType,p,a,b,c,d,e)
#define IDirect3D8_CheckDepthStencilMatch(p,a,b,c,d,e)     ICOM_CALL5(CheckDepthStencilMatch,p,a,b,c,d,e)
#define IDirect3D8_GetDeviceCaps(p,a,b,c)                  ICOM_CALL3(CheckDepthStencilMatch,p,a,b,c)
#define IDirect3D8_GetAdapterMonitor(p,a)                  ICOM_CALL1(GetAdapterMonitor,p,a)
#define IDirect3D8_CreateDevice(p,a,b,c,d,e,f)             ICOM_CALL5(CreateDevice,p,a,b,c,d,e,f)


/*****************************************************************************
 * IDirect3DDevice8 interface
 */
#define ICOM_INTERFACE IDirect3DDevice8
#define IDirect3DDevice8_METHODS \
    ICOM_METHOD (HRESULT,TestCooperativeLevel) \
    ICOM_METHOD (UINT,   GetAvailableTextureMem) \
    ICOM_METHOD1(HRESULT,ResourceManagerDiscardBytes,DWORD,Bytes) \
    ICOM_METHOD1(HRESULT,GetDirect3D,                IDirect3D8**,ppD3D8) \
    ICOM_METHOD1(HRESULT,GetDeviceCaps,              D3DCAPS8*,pCaps) \
    ICOM_METHOD1(HRESULT,GetDisplayMode,             D3DDISPLAYMODE*,pMode) \
    ICOM_METHOD1(HRESULT,GetCreationParameters,      D3DDEVICE_CREATION_PARAMETERS*,pParameters) \
    ICOM_METHOD3(HRESULT,SetCursorProperties,        UINT,XHotSpot,UINT,YHotSpot,IDirect3DSurface8*,pCursorBitmap) \
    ICOM_VMETHOD3(       SetCursorPosition,          UINT,XScreenSpace,UINT,YScreenSpace,DWORD,Flags) \
    ICOM_METHOD1(BOOL,   ShowCursor,                 BOOL,bShow) \
    ICOM_METHOD2(HRESULT,CreateAdditionalSwapChain,  D3DPRESENT_PARAMETERS*,pPresentationParameters,IDirect3DSwapChain8**,pSwapChain) \
    ICOM_METHOD1(HRESULT,Reset,                      D3DPRESENT_PARAMETERS*,pPresentationParameters) \
    ICOM_METHOD4(HRESULT,Present,                    CONST RECT*,pSourceRect,CONST RECT*,pDestRect,HWND,hDestWindowOverride,CONST RGNDATA*,pDirtyRegion) \
    ICOM_METHOD3(HRESULT,GetBackBuffer,              UINT,BackBuffer,D3DBACKBUFFER_TYPE,Type,IDirect3DSurface8**,ppBackBuffer) \
    ICOM_METHOD1(HRESULT,GetRasterStatus,            D3DRASTER_STATUS*,pRasterStatus) \
    ICOM_VMETHOD2(       SetGammaRamp,               DWORD,Flags,CONST D3DGAMMARAMP*,pRamp) \
    ICOM_VMETHOD1(       GetGammaRamp,               D3DGAMMARAMP*,pRamp) \
    ICOM_METHOD7(HRESULT,CreateTexture,              UINT,Width,UINT,Height,UINT,Levels,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DTexture8**,ppTexture) \
    ICOM_METHOD8(HRESULT,CreateVolumeTexture,        UINT,Width,UINT,Height,UINT,Depth,UINT,Levels,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DVolumeTexture8**,ppVolumeTexture) \
    ICOM_METHOD6(HRESULT,CreateCubeTexture,          UINT,EdgeLength,UINT,Levels,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DCubeTexture8**,ppCubeTexture) \
    ICOM_METHOD5(HRESULT,CreateVertexBuffer,         UINT,Length,DWORD,Usage,DWORD,FVF,D3DPOOL,Pool,IDirect3DVertexBuffer8**,ppVertexBuffer) \
    ICOM_METHOD5(HRESULT,CreateIndexBuffer,          UINT,Length,DWORD,Usage,D3DFORMAT,Format,D3DPOOL,Pool,IDirect3DIndexBuffer8**,ppIndexBuffer) \
    ICOM_METHOD6(HRESULT,CreateRenderTarget,         UINT,Width,UINT,Height,D3DFORMAT,Format,D3DMULTISAMPLE_TYPE,MultiSample,BOOL,Lockable,IDirect3DSurface8**,ppSurface) \
    ICOM_METHOD5(HRESULT,CreateDepthStencilSurface,  UINT,Width,UINT,Height,D3DFORMAT,Format,D3DMULTISAMPLE_TYPE,MultiSample,IDirect3DSurface8**,ppSurface) \
    ICOM_METHOD4(HRESULT,CreateImageSurface,         UINT,Width,UINT,Height,D3DFORMAT,Format,IDirect3DSurface8**,ppSurface) \
    ICOM_METHOD5(HRESULT,CopyRects,                  IDirect3DSurface8*,pSourceSurface,CONST RECT*,pSourceRectsArray,UINT,cRects,IDirect3DSurface8*,pDestSurface,CONST POINT*,pDestPointsArray) \
    ICOM_METHOD2(HRESULT,UpdateTexture,              IDirect3DBaseTexture8*,pSourceTexture,IDirect3DBaseTexture8*,pDestTexture) \
    ICOM_METHOD1(HRESULT,GetFrontBuffer,             IDirect3DSurface8*,pDestSurface) \
    ICOM_METHOD2(HRESULT,SetRenderTarget,            IDirect3DSurface8*,pRenderTarget,IDirect3DSurface8*,pNewZStencil) \
    ICOM_METHOD1(HRESULT,GetRenderTarget,            IDirect3DSurface8**,ppRenderTarget) \
    ICOM_METHOD1(HRESULT,GetDepthStencilSurface,     IDirect3DSurface8**,ppZStencilSurface) \
    ICOM_METHOD (HRESULT,BeginScene) \
    ICOM_METHOD (HRESULT,EndScene) \
    ICOM_METHOD6(HRESULT,Clear,                      DWORD,Count,CONST D3DRECT*,pRects,DWORD,Flags,D3DCOLOR,Color,float,Z,DWORD,Stencil) \
    ICOM_METHOD2(HRESULT,SetTransform,               D3DTRANSFORMSTATETYPE,State,CONST D3DMATRIX*,pMatrix) \
    ICOM_METHOD2(HRESULT,GetTransform,               D3DTRANSFORMSTATETYPE,State,D3DMATRIX*,pMatrix) \
    ICOM_METHOD2(HRESULT,MultiplyTransform,          D3DTRANSFORMSTATETYPE,State,CONST D3DMATRIX*,pMatrix) \
    ICOM_METHOD1(HRESULT,SetViewport,                CONST D3DVIEWPORT8*,pViewport) \
    ICOM_METHOD1(HRESULT,GetViewport,                D3DVIEWPORT8*,pViewport) \
    ICOM_METHOD1(HRESULT,SetMaterial,                CONST D3DMATERIAL8*,pMaterial) \
    ICOM_METHOD1(HRESULT,GetMaterial,                D3DMATERIAL8*,pMaterial) \
    ICOM_METHOD2(HRESULT,SetLight,                   DWORD,Index,CONST D3DLIGHT8*,pLight) \
    ICOM_METHOD2(HRESULT,GetLight,                   DWORD,Index,D3DLIGHT8*,pLight) \
    ICOM_METHOD2(HRESULT,LightEnable,                DWORD,Index,BOOL,Enable) \
    ICOM_METHOD2(HRESULT,GetLightEnable,             DWORD,Index,BOOL*,pEnable) \
    ICOM_METHOD2(HRESULT,SetClipPlane,               DWORD,Index,CONST float*,pPlane) \
    ICOM_METHOD2(HRESULT,GetClipPlane,               DWORD,Index,float*,pPlane) \
    ICOM_METHOD2(HRESULT,SetRenderState,             D3DRENDERSTATETYPE,State,DWORD,Value) \
    ICOM_METHOD2(HRESULT,GetRenderState,             D3DRENDERSTATETYPE,State,DWORD*,pValue) \
    ICOM_METHOD (HRESULT,BeginStateBlock) \
    ICOM_METHOD1(HRESULT,EndStateBlock,              DWORD*,pToken) \
    ICOM_METHOD1(HRESULT,ApplyStateBlock,            DWORD,Token) \
    ICOM_METHOD1(HRESULT,CaptureStateBlock,          DWORD,Token) \
    ICOM_METHOD1(HRESULT,DeleteStateBlock,           DWORD,Token) \
    ICOM_METHOD2(HRESULT,CreateStateBlock,           D3DSTATEBLOCKTYPE,Type,DWORD*,pToken) \
    ICOM_METHOD1(HRESULT,SetClipStatus,              CONST D3DCLIPSTATUS8*,pClipStatus) \
    ICOM_METHOD1(HRESULT,GetClipStatus,              D3DCLIPSTATUS8*,pClipStatus) \
    ICOM_METHOD2(HRESULT,GetTexture,                 DWORD,Stage,IDirect3DBaseTexture8**,ppTexture) \
    ICOM_METHOD2(HRESULT,SetTexture,                 DWORD,Stage,IDirect3DBaseTexture8*,pTexture) \
    ICOM_METHOD3(HRESULT,GetTextureStageState,       DWORD,Stage,D3DTEXTURESTAGESTATETYPE,Type,DWORD*,pValue) \
    ICOM_METHOD3(HRESULT,SetTextureStageState,       DWORD,Stage,D3DTEXTURESTAGESTATETYPE,Type,DWORD,Value) \
    ICOM_METHOD1(HRESULT,ValidateDevice,             DWORD*,pNumPasses) \
    ICOM_METHOD3(HRESULT,GetInfo,                    DWORD,DevInfoID,void*,pDevInfoStruct,DWORD,DevInfoStructSize) \
    ICOM_METHOD2(HRESULT,SetPaletteEntries,          UINT,PaletteNumber,CONST PALETTEENTRY*,pEntries) \
    ICOM_METHOD2(HRESULT,GetPaletteEntries,          UINT,PaletteNumber,PALETTEENTRY*,pEntries) \
    ICOM_METHOD1(HRESULT,SetCurrentTexturePalette,   UINT,PaletteNumber) \
    ICOM_METHOD1(HRESULT,GetCurrentTexturePalette,   UINT*,pPaletteNumber) \
    ICOM_METHOD3(HRESULT,DrawPrimitive,              D3DPRIMITIVETYPE,PrimitiveType,UINT,StartVertex,UINT,PrimitiveCount) \
    ICOM_METHOD5(HRESULT,DrawIndexedPrimitive,       D3DPRIMITIVETYPE,PrimitiveType,UINT,MinIndex,UINT,NumVertices,UINT,StartIndex,UINT,PrimitiveCount) \
    ICOM_METHOD4(HRESULT,DrawPrimitiveUP,            D3DPRIMITIVETYPE,PrimitiveType,UINT,PrimitiveCount,CONST void*,pVertexStreamZeroData,UINT,VertexStreamZeroStride) \
    ICOM_METHOD8(HRESULT,DrawIndexedPrimitiveUP,     D3DPRIMITIVETYPE,PrimitiveType,UINT,MinIndex,UINT,NumVertices,UINT,PrimitiveCount,CONST void*,pIndexData,D3DFORMAT,IndexFormat,CONST void*,pVertexStreamZeroData,UINT,VertexStreamZeroStride) \
    ICOM_METHOD5(HRESULT,ProcessVertices,            UINT,SrcStartIndex,UINT,DestIndex,UINT,VertexCount,IDirect3DVertexBuffer8*,pDestBuffer,DWORD,Flags) \
    ICOM_METHOD4(HRESULT,CreateVertexShader,         CONST DWORD*,pDeclaration,CONST DWORD*,pFunction,DWORD*,pHandle,DWORD,Usage) \
    ICOM_METHOD1(HRESULT,SetVertexShader,            DWORD,Handle) \
    ICOM_METHOD1(HRESULT,GetVertexShader,            DWORD*,pHandle) \
    ICOM_METHOD1(HRESULT,DeleteVertexShader,         DWORD,Handle) \
    ICOM_METHOD3(HRESULT,SetVertexShaderConstant,    DWORD,Register,CONST void*,pConstantData,DWORD,ConstantCount) \
    ICOM_METHOD3(HRESULT,GetVertexShaderConstant,    DWORD,Register,void*,pConstantData,DWORD,ConstantCount) \
    ICOM_METHOD3(HRESULT,GetVertexShaderDeclaration, DWORD,Handle,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD3(HRESULT,GetVertexShaderFunction,    DWORD,Handle,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD3(HRESULT,SetStreamSource,            UINT,StreamNumber,IDirect3DVertexBuffer8*,pStreamData,UINT,Stride) \
    ICOM_METHOD3(HRESULT,GetStreamSource,            UINT,StreamNumber,IDirect3DVertexBuffer8**,ppStreamData,UINT*,pStride) \
    ICOM_METHOD2(HRESULT,SetIndices,                 IDirect3DIndexBuffer8*,pIndexData,UINT,BaseIndex) \
    ICOM_METHOD2(HRESULT,GetIndices,                 IDirect3DIndexBuffer8**,ppIndexData,UINT*,pBaseIndex) \
    ICOM_METHOD2(HRESULT,CreatePixelShader,          CONST DWORD*,pFunction,DWORD*,pHandle) \
    ICOM_METHOD1(HRESULT,SetPixelShader,             DWORD,Handle) \
    ICOM_METHOD1(HRESULT,GetPixelShader,             DWORD*,pHandle) \
    ICOM_METHOD1(HRESULT,DeletePixelShader,          DWORD,Handle) \
    ICOM_METHOD3(HRESULT,SetPixelShaderConstant,     DWORD,Register,CONST void*,pConstantData,DWORD,ConstantCount) \
    ICOM_METHOD3(HRESULT,GetPixelShaderConstant,     DWORD,Register,void*,pConstantData,DWORD,ConstantCount) \
    ICOM_METHOD3(HRESULT,GetPixelShaderFunction,     DWORD,Handle,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD3(HRESULT,DrawRectPatch,              UINT,Handle,CONST float*,pNumSegs,CONST D3DRECTPATCH_INFO*,pRectPatchInfo) \
    ICOM_METHOD3(HRESULT,DrawTriPatch,               UINT,Handle,CONST float*,pNumSegs,CONST D3DTRIPATCH_INFO*,pTriPatchInfo) \
    ICOM_METHOD1(HRESULT,DeletePatch,                UINT,Handle)

#define IDirect3DDevice8_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DDevice8_METHODS
ICOM_DEFINE(IDirect3DDevice8,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DDevice8_QueryInterface(p,a,b)                     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DDevice8_AddRef(p)                                 ICOM_CALL (AddRef,p)
#define IDirect3DDevice8_Release(p)                                ICOM_CALL (Release,p)
	/*** IDirect3DDevice8 methods ***/
#define IDirect3DDevice8_TestCooperativeLevel(p)                   ICOM_CALL (TestCooperativeLevel,p)
#define IDirect3DDevice8_GetAvailableTextureMem(p)                 ICOM_CALL (GetAvailableTextureMem,p)
#define IDirect3DDevice8_ResourceManagerDiscardBytes(p,a)          ICOM_CALL1(ResourceManagerDiscardBytes,p,a)
#define IDirect3DDevice8_GetDirect3D(p,a)                          ICOM_CALL1(GetDirect3D,p,a)
#define IDirect3DDevice8_GetDeviceCaps(p,a)                        ICOM_CALL1(GetDeviceCaps,p,a)
#define IDirect3DDevice8_GetDisplayMode(p,a)                       ICOM_CALL1(GetDisplayMode,p,a)
#define IDirect3DDevice8_GetCreationParameters(p,a)                ICOM_CALL1(GetCreationParameters,p,a)
#define IDirect3DDevice8_SetCursorProperties(p,a,b,c)              ICOM_CALL3(SetCursorProperties,p,a,b,c)
#define IDirect3DDevice8_SetCursorPosition(p,a,b,c)                ICOM_CALL3(SetCursorPosition,p,a,b,c)
#define IDirect3DDevice8_ShowCursor(p,a)                           ICOM_CALL1(ShowCursor,p,a)
#define IDirect3DDevice8_CreateAdditionalSwapChain(p,a,b)          ICOM_CALL2(CreateAdditionalSwapChain,p,a)
#define IDirect3DDevice8_Reset(p,a)                                ICOM_CALL1(Reset,p,a)
#define IDirect3DDevice8_Present(p,a,b,c,d)                        ICOM_CALL4(Reset,p,a,b,c,d)
#define IDirect3DDevice8_GetBackBuffer(p,a,b,c)                    ICOM_CALL3(GetBackBuffer,p,a,b,c)
#define IDirect3DDevice8_GetRasterStatus(p,a)                      ICOM_CALL1(GetRasterStatus,p,a)
#define IDirect3DDevice8_SetGammaRamp(p,a,b)                       ICOM_CALL2(SetGammaRamp,p,a,b)
#define IDirect3DDevice8_GetGammaRamp(p,a)                         ICOM_CALL1(GetGammaRamp,p,a)
#define IDirect3DDevice8_CreateTexture(p,a,b,c,d,e,f,g)            ICOM_CALL7(CreateTexture,p,a,b,c,d,e,f,g)
#define IDirect3DDevice8_CreateVolumeTexture(p,a,b,c,d,e,f,g,h)    ICOM_CALL8(CreateVolumeTexture,p,a,b,c,d,e,f,g,h)
#define IDirect3DDevice8_CreateCubeTexture(p,a,b,c,d,e,f)          ICOM_CALL6(CreateCubeTexture,p,a,b,c,d,e,f)
#define IDirect3DDevice8_CreateVertexBuffer(p,a,b,c,d,e)           ICOM_CALL5(CreateVertexBuffer,p,a,b,c,d,e)
#define IDirect3DDevice8_CreateIndexBuffer(p,a,b,c,d,e)            ICOM_CALL5(CreateIndexBuffer,p,a,b,c,d,e)
#define IDirect3DDevice8_CreateRenderTarget(p,a,b,c,d,e,f)         ICOM_CALL6(CreateRenderTarget,p,a,b,c,d,e,f)
#define IDirect3DDevice8_CreateDepthStencilSurface(p,a,b,c,d,e)    ICOM_CALL5(CreateDepthStencilSurface,p,a,b,c,d,e)
#define IDirect3DDevice8_CreateImageSurface(p,a,b,c,d)             ICOM_CALL4(CreateImageSurface,p,a,b,c,d)
#define IDirect3DDevice8_CopyRects(p,a,b,c,d,e)                    ICOM_CALL5(CopyRects,p,a,b,c,d,e)
#define IDirect3DDevice8_UpdateTexture(p,a,b)                      ICOM_CALL2(UpdateTexture,p,a,b)
#define IDirect3DDevice8_GetFrontBuffer(p,a)                       ICOM_CALL1(GetFrontBuffer,p,a)
#define IDirect3DDevice8_SetRenderTarget(p,a,b)                    ICOM_CALL2(SetRenderTarget,p,a,b)
#define IDirect3DDevice8_GetRenderTarget(p,a)                      ICOM_CALL1(GetRenderTarget,p,a)
#define IDirect3DDevice8_GetDepthStencilSurface(p,a)               ICOM_CALL1(GetDepthStencilSurface,p,a)
#define IDirect3DDevice8_BeginScene(p)                             ICOM_CALL (BeginScene,p)
#define IDirect3DDevice8_EndScene(p)                               ICOM_CALL (EndScene,p)
#define IDirect3DDevice8_Clear(p,a,b,c,d,e,f)                      ICOM_CALL6(Clear,p,a,b,c,d,e,f)
#define IDirect3DDevice8_SetTransform(p,a,b)                       ICOM_CALL2(SetTransform,p,a,b)
#define IDirect3DDevice8_GetTransform(p,a,b)                       ICOM_CALL2(GetTransform,p,a,b)
#define IDirect3DDevice8_MultiplyTransform(p,a,b)                  ICOM_CALL2(MultiplyTransform,p,a,b)
#define IDirect3DDevice8_SetViewport(p,a)                          ICOM_CALL1(SetViewport,p,a)
#define IDirect3DDevice8_GetViewport(p,a)                          ICOM_CALL1(GetViewport,p,a)
#define IDirect3DDevice8_SetMaterial(p,a)                          ICOM_CALL1(SetMaterial,p,a)
#define IDirect3DDevice8_GetMaterial(p,a)                          ICOM_CALL1(GetMaterial,p,a)
#define IDirect3DDevice8_SetLight(p,a,b)                           ICOM_CALL2(SetLight,p,a,b)
#define IDirect3DDevice8_GetLight(p,a,b)                           ICOM_CALL2(GetLight,p,a,b)
#define IDirect3DDevice8_LightEnable(p,a,b)                        ICOM_CALL2(LightEnable,p,a,b)
#define IDirect3DDevice8_GetLightEnable(p,a,b)                     ICOM_CALL2(GetLightEnable,p,a,b)
#define IDirect3DDevice8_SetClipPlane(p,a,b)                       ICOM_CALL2(SetClipPlane,p,a,b)
#define IDirect3DDevice8_GetClipPlane(p,a,b)                       ICOM_CALL2(GetClipPlane,p,a,b)
#define IDirect3DDevice8_SetRenderState(p,a,b)                     ICOM_CALL2(SetRenderState,p,a,b)
#define IDirect3DDevice8_GetRenderState(p,a,b)                     ICOM_CALL2(GetRenderState,p,a,b)
#define IDirect3DDevice8_BeginStateBlock(p)                        ICOM_CALL (BeginStateBlock,p)
#define IDirect3DDevice8_EndStateBlock(p,a)                        ICOM_CALL1(EndStateBlock,p,a)
#define IDirect3DDevice8_ApplyStateBlock(p,a)                      ICOM_CALL1(ApplyStateBlock,p,a)
#define IDirect3DDevice8_CaptureStateBlock(p,a)                    ICOM_CALL1(CaptureStateBlock,p,a)
#define IDirect3DDevice8_DeleteStateBlock(p,a)                     ICOM_CALL1(DeleteStateBlock,p,a)
#define IDirect3DDevice8_CreateStateBlock(p,a,b)                   ICOM_CALL2(CreateStateBlock,p,a,b)
#define IDirect3DDevice8_SetClipStatus(p,a)                        ICOM_CALL1(SetClipStatus,p,a)
#define IDirect3DDevice8_GetClipStatus(p,a)                        ICOM_CALL1(GetClipStatus,p,a)
#define IDirect3DDevice8_GetTexture(p,a,b)                         ICOM_CALL2(GetTexture,p,a,b)
#define IDirect3DDevice8_SetTexture(p,a,b)                         ICOM_CALL2(SetTexture,p,a,b)
#define IDirect3DDevice8_GetTextureStageState(p,a,b,c)             ICOM_CALL3(GetTextureStageState,p,a,b,c)
#define IDirect3DDevice8_SetTextureStageState(p,a,b,c)             ICOM_CALL3(SetTextureStageState,p,a,b,c)
#define IDirect3DDevice8_ValidateDevice(p,a)                       ICOM_CALL1(ValidateDevice,p,a)
#define IDirect3DDevice8_GetInfo(p,a,b,c)                          ICOM_CALL3(GetInfo,p,a,b,c)
#define IDirect3DDevice8_SetPaletteEntries(p,a,b)                  ICOM_CALL2(SetPaletteEntries,p,a,b)
#define IDirect3DDevice8_GetPaletteEntries(p,a,b)                  ICOM_CALL2(GetPaletteEntries,p,a,b)
#define IDirect3DDevice8_SetCurrentTexturePalette(p,a)             ICOM_CALL1(SetCurrentTexturePalette,p,a)
#define IDirect3DDevice8_GetCurrentTexturePalette(p,a)             ICOM_CALL1(GetCurrentTexturePalette,p,a)
#define IDirect3DDevice8_DrawPrimitive(p,a,b,c)                    ICOM_CALL3(DrawPrimitive,p,a,b,c)
#define IDirect3DDevice8_DrawIndexedPrimitive(p,a,b,c,d,e)         ICOM_CALL5(DrawIndexedPrimitive,p,a,b,c,d,e)
#define IDirect3DDevice8_DrawPrimitiveUP(p,a,b,c,d)                ICOM_CALL4(DrawPrimitiveUP,p,a,b,c,d)
#define IDirect3DDevice8_DrawIndexedPrimitiveUP(p,a,b,c,d,e,f,g,h) ICOM_CALL8(DrawIndexedPrimitiveUP,p,a,b,c,d,e,f,g,h)
#define IDirect3DDevice8_ProcessVertices(p,a,b,c,d,e)              ICOM_CALL5(ProcessVertices,p,a,b,c,d,e)
#define IDirect3DDevice8_CreateVertexShader(p,a,b,c,d)             ICOM_CALL4(CreateVertexShader,p,a,b,c,d)
#define IDirect3DDevice8_SetVertexShader(p,a)                      ICOM_CALL1(SetVertexShader,p,a)
#define IDirect3DDevice8_GetVertexShader(p,a)                      ICOM_CALL1(GetVertexShader,p,a)
#define IDirect3DDevice8_DeleteVertexShader(p,a)                   ICOM_CALL1(DeleteVertexShader,p,a)
#define IDirect3DDevice8_SetVertexShaderConstant(p,a,b,c)          ICOM_CALL3(SetVertexShaderConstant,p,a,b,c)
#define IDirect3DDevice8_GetVertexShaderConstant(p,a,b,c)          ICOM_CALL3(GetVertexShaderConstant,p,a,b,c)
#define IDirect3DDevice8_GetVertexShaderDeclaration(p,a,b,c)       ICOM_CALL3(GetVertexShaderDeclaration,p,a,b,c)
#define IDirect3DDevice8_GetVertexShaderFunction(p,a,b,c)          ICOM_CALL3(GetVertexShaderFunction,p,a,b,c)
#define IDirect3DDevice8_SetStreamSource(p,a,b,c)                  ICOM_CALL3(SetStreamSource,p,a,b,c)
#define IDirect3DDevice8_GetStreamSource(p,a,b,c)                  ICOM_CALL3(GetStreamSource,p,a,b,c)
#define IDirect3DDevice8_SetIndices(p,a,b)                         ICOM_CALL2(SetIndices,p,a,b)
#define IDirect3DDevice8_GetIndices(p,a,b)                         ICOM_CALL2(GetIndices,p,a,b)
#define IDirect3DDevice8_CreatePixelShader(p,a,b)                  ICOM_CALL2(CreatePixelShader,p,a,b)
#define IDirect3DDevice8_SetPixelShader(p,a)                       ICOM_CALL1(SetPixelShader,p,a)
#define IDirect3DDevice8_GetPixelShader(p,a)                       ICOM_CALL1(GetPixelShader,p,a)
#define IDirect3DDevice8_DeletePixelShader(p,a)                    ICOM_CALL1(DeletePixelShader,p,a)
#define IDirect3DDevice8_SetPixelShaderConstant(p,a,b,c)           ICOM_CALL3(SetPixelShaderConstant,p,a,b,c)
#define IDirect3DDevice8_GetPixelShaderConstant(p,a,b,c)           ICOM_CALL3(GetPixelShaderConstant,p,a,b,c)
#define IDirect3DDevice8_GetPixelShaderFunction(p,a,b,c)           ICOM_CALL3(GetPixelShaderFunction,p,a,b,c)
#define IDirect3DDevice8_DrawRectPatch(p,a,b,c)                    ICOM_CALL3(DrawRectPatch,p,a,b,c)
#define IDirect3DDevice8_DrawTriPatch(p,a,b,c)                     ICOM_CALL3(DrawTriPatch,p,a,b,c)
#define IDirect3DDevice8_DeletePatch(p,a)                          ICOM_CALL1(DeletePatch,p,a)


/*****************************************************************************
 * IDirect3DSwapChain8 interface
 */
#define ICOM_INTERFACE IDirect3DSwapChain8
#define IDirect3DSwapChain8_METHODS \
    ICOM_METHOD4(HRESULT,Present,      CONST RECT*,pSourceRect,CONST RECT*,pDestRect,HWND,hDestWindowOverride,CONST RGNDATA*,pDirtyRegion) \
    ICOM_METHOD3(HRESULT,GetBackBuffer,UINT,BackBuffer,D3DBACKBUFFER_TYPE,Type,IDirect3DSurface8**,ppBackBuffer)
#define IDirect3DSwapChain8_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DSwapChain8_METHODS
ICOM_DEFINE(IDirect3DSwapChain8,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DSwapChain8_QueryInterface(p,a,b)  ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DSwapChain8_AddRef(p)              ICOM_CALL (AddRef,p)
#define IDirect3DSwapChain8_Release(p)             ICOM_CALL (Release,p)
	/*** IDirect3DSwapChain8 methods ***/
#define IDirect3DSwapChain8_Present(p,a,b,c,d)     ICOM_CALL4(Present,p,a,b,c,d)
#define IDirect3DSwapChain8_GetBackBuffer(p,a,b,c) ICOM_CALL3(GetBackBuffer,p,a,b,c)


/*****************************************************************************
 * IDirect3DResource8 interface
 */
#define ICOM_INTERFACE IDirect3DResource8
#define IDirect3DResource8_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,      IDirect3DDevice8**,ppDevice) \
    ICOM_METHOD4(HRESULT,SetPrivateData, REFGUID,refguid,CONST void*,pData,DWORD,SizeOfData,DWORD,Flags) \
    ICOM_METHOD3(HRESULT,GetPrivateData, REFGUID,refguid,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD1(HRESULT,FreePrivateData,REFGUID,refguid) \
    ICOM_METHOD1(DWORD,  SetPriority,    DWORD,PriorityNew) \
    ICOM_METHOD (DWORD,  GetPriority) \
    ICOM_VMETHOD(        PreLoad) \
    ICOM_METHOD (D3DRESOURCETYPE,GetType)
#define IDirect3DResource8_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DResource8_METHODS
ICOM_DEFINE(IDirect3DResource8,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DResource8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DResource8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DResource8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DResource8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DResource8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DResource8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DResource8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DResource8_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DResource8_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DResource8_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DResource8_GetType(p)                ICOM_CALL (GetType,p)


/*****************************************************************************
 * IDirect3DBaseTexture8 interface
 */
#define ICOM_INTERFACE IDirect3DBaseTexture8
#define IDirect3DBaseTexture8_METHODS \
    ICOM_METHOD1(DWORD,SetLOD,DWORD,LODNew) \
    ICOM_METHOD (DWORD,GetLOD) \
    ICOM_METHOD (DWORD,GetLevelCount)
#define IDirect3DBaseTexture8_IMETHODS \
    IDirect3DResource8_IMETHODS \
    IDirect3DBaseTexture8_METHODS
ICOM_DEFINE(IDirect3DBaseTexture8,IDirect3DResource8)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DBaseTexture8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DBaseTexture8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DBaseTexture8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DBaseTexture8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DBaseTexture8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DBaseTexture8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DBaseTexture8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DBaseTexture8_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DBaseTexture8_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DBaseTexture8_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DBaseTexture8_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture8 methods ***/
#define IDirect3DBaseTexture8_SetLOD(p,a)               ICOM_CALL1(SetLOD,p,a)
#define IDirect3DBaseTexture8_GetLOD(p)                 ICOM_CALL (GetLOD,p)
#define IDirect3DBaseTexture8_GetLevelCount(p)          ICOM_CALL (GetLevelCount,p)


/*****************************************************************************
 * IDirect3DTexture8 interface
 */
#define ICOM_INTERFACE IDirect3DTexture8
#define IDirect3DTexture8_METHODS \
    ICOM_METHOD2(HRESULT,GetLevelDesc,   UINT,Level,D3DSURFACE_DESC*,pDesc) \
    ICOM_METHOD2(HRESULT,GetSurfaceLevel,UINT,Level,IDirect3DSurface8**,ppSurfaceLevel) \
    ICOM_METHOD4(HRESULT,LockRect,       UINT,Level,D3DLOCKED_RECT*,pLockedRect,CONST RECT*,pRect,DWORD,Flags) \
    ICOM_METHOD1(HRESULT,UnlockRect,     UINT,Level) \
    ICOM_METHOD1(HRESULT,AddDirtyRect,   CONST RECT*,pDirtyRect)
#define IDirect3DTexture8_IMETHODS \
    IDirect3DBaseTexture8_IMETHODS \
    IDirect3DTexture8_METHODS
ICOM_DEFINE(IDirect3DTexture8,IDirect3DBaseTexture8)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DTexture8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DTexture8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DTexture8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DTexture8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DTexture8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DTexture8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DTexture8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DTexture8_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DTexture8_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DTexture8_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DTexture8_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture8 methods ***/
#define IDirect3DTexture8_SetLOD(p,a)               ICOM_CALL1(SetLOD,p,a)
#define IDirect3DTexture8_GetLOD(p)                 ICOM_CALL (GetLOD,p)
#define IDirect3DTexture8_GetLevelCount(p)          ICOM_CALL (GetLevelCount,p)
	/*** IDirect3DTexture8 methods ***/
#define IDirect3DTexture8_GetLevelDesc(p,a,b)       ICOM_CALL2(GetLevelDesc,p,a,b)
#define IDirect3DTexture8_GetSurfaceLevel(p,a,b)    ICOM_CALL2(GetSurfaceLevel,p,a,b)
#define IDirect3DTexture8_LockRect(p,a,b,c,d)       ICOM_CALL4(LockRect,p,a,b,c,d)
#define IDirect3DTexture8_UnlockRect(p,a)           ICOM_CALL1(UnlockRect,p,a)
#define IDirect3DTexture8_AddDirtyRect(p,a)         ICOM_CALL1(AddDirtyRect,p,a)


/*****************************************************************************
 * IDirect3DVolumeTexture8 interface
 */
#define ICOM_INTERFACE IDirect3DVolumeTexture8
#define IDirect3DVolumeTexture8_METHODS \
    ICOM_METHOD2(HRESULT,GetLevelDesc,  UINT,Level,D3DVOLUME_DESC*,pDesc) \
    ICOM_METHOD2(HRESULT,GetVolumeLevel,UINT,Level,IDirect3DVolume8**,ppVolumeLevel) \
    ICOM_METHOD4(HRESULT,LockBox,       UINT,Level,D3DLOCKED_BOX*,pLockedVolume,CONST D3DBOX*,pBox,DWORD,Flags) \
    ICOM_METHOD1(HRESULT,UnlockBox,     UINT,Level) \
    ICOM_METHOD1(HRESULT,AddDirtyBox,   CONST D3DBOX*,pDirtyBox)
#define IDirect3DVolumeTexture8_IMETHODS \
    IDirect3DBaseTexture8_IMETHODS \
    IDirect3DVolumeTexture8_METHODS
ICOM_DEFINE(IDirect3DVolumeTexture8,IDirect3DBaseTexture8)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVolumeTexture8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVolumeTexture8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DVolumeTexture8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DVolumeTexture8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVolumeTexture8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DVolumeTexture8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DVolumeTexture8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DVolumeTexture8_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DVolumeTexture8_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DVolumeTexture8_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DVolumeTexture8_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture8 methods ***/
#define IDirect3DVolumeTexture8_SetLOD(p,a)               ICOM_CALL1(SetLOD,p,a)
#define IDirect3DVolumeTexture8_GetLOD(p)                 ICOM_CALL (GetLOD,p)
#define IDirect3DVolumeTexture8_GetLevelCount(p)          ICOM_CALL (GetLevelCount,p)
	/*** IDirect3DVolumeTexture8 methods ***/
#define IDirect3DVolumeTexture8_GetLevelDesc(p,a,b)       ICOM_CALL2(GetLevelDesc,p,a,b)
#define IDirect3DVolumeTexture8_GetVolumeLevel(p,a,b)     ICOM_CALL2(GetVolumeLevel,p,a,b)
#define IDirect3DVolumeTexture8_LockBox(p,a,b,c,d)        ICOM_CALL4(LockBox,p,a,b,c,d)
#define IDirect3DVolumeTexture8_UnlockBox(p,a)            ICOM_CALL1(UnlockBox,p,a)
#define IDirect3DVolumeTexture8_AddDirtyBox(p,a)          ICOM_CALL1(AddDirtyBox,p,a)


/*****************************************************************************
 * IDirect3DCubeTexture8 interface
 */
#define ICOM_INTERFACE IDirect3DCubeTexture8
#define IDirect3DCubeTexture8_METHODS \
    ICOM_METHOD2(HRESULT,GetLevelDesc,     UINT,Level,D3DSURFACE_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,GetCubeMapSurface,D3DCUBEMAP_FACES,FaceType,UINT,Level,IDirect3DSurface8**,ppCubeMapSurface) \
    ICOM_METHOD5(HRESULT,LockRect,         D3DCUBEMAP_FACES,FaceType,UINT,Level,D3DLOCKED_RECT*,pLockedRect,CONST RECT*,pRect,DWORD,Flags) \
    ICOM_METHOD2(HRESULT,UnlockRect,       D3DCUBEMAP_FACES,FaceType,UINT,Level) \
    ICOM_METHOD2(HRESULT,AddDirtyRect,     D3DCUBEMAP_FACES,FaceType,CONST RECT*,pDirtyRect)
#define IDirect3DCubeTexture8_IMETHODS \
    IDirect3DBaseTexture8_IMETHODS \
    IDirect3DCubeTexture8_METHODS
ICOM_DEFINE(IDirect3DCubeTexture8,IDirect3DBaseTexture8)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DCubeTexture8_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DCubeTexture8_AddRef(p)                  ICOM_CALL (AddRef,p)
#define IDirect3DCubeTexture8_Release(p)                 ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DCubeTexture8_GetDevice(p,a)             ICOM_CALL1(GetDevice,p,a)
#define IDirect3DCubeTexture8_SetPrivateData(p,a,b,c,d)  ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DCubeTexture8_GetPrivateData(p,a,b,c)    ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DCubeTexture8_FreePrivateData(p,a)       ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DCubeTexture8_SetPriority(p,a)           ICOM_CALL1(SetPriority,p,a)
#define IDirect3DCubeTexture8_GetPriority(p)             ICOM_CALL (GetPriority,p)
#define IDirect3DCubeTexture8_PreLoad(p)                 ICOM_CALL (PreLoad,p)
#define IDirect3DCubeTexture8_GetType(p)                 ICOM_CALL (GetType,p)
	/*** IDirect3DBaseTexture8 methods ***/
#define IDirect3DCubeTexture8_SetLOD(p,a)                ICOM_CALL1(SetLOD,p,a)
#define IDirect3DCubeTexture8_GetLOD(p)                  ICOM_CALL (GetLOD,p)
#define IDirect3DCubeTexture8_GetLevelCount(p)           ICOM_CALL (GetLevelCount,p)
	/*** IDirect3DCubeTexture8 methods ***/
#define IDirect3DCubeTexture8_GetLevelDesc(p,a,b)        ICOM_CALL2(GetLevelDesc,p,a,b)
#define IDirect3DCubeTexture8_GetCubeMapSurface(p,a,b,c) ICOM_CALL3(GetCubeMapSurface,p,a,b,c)
#define IDirect3DCubeTexture8_LockRect(p,a,b,c,d,e)      ICOM_CALL4(LockRect,p,a,b,c,d,e)
#define IDirect3DCubeTexture8_UnlockRect(p,a,b)          ICOM_CALL1(UnlockRect,p,a,b)
#define IDirect3DCubeTexture8_AddDirtyRect(p,a,b)        ICOM_CALL1(AddDirtyRect,p,a,b)


/*****************************************************************************
 * IDirect3DVertexBuffer8 interface
 */
#define ICOM_INTERFACE IDirect3DVertexBuffer8
#define IDirect3DVertexBuffer8_METHODS \
    ICOM_METHOD4(HRESULT,Lock,   UINT,OffsetToLock,UINT,SizeToLock,BYTE**,ppbData,DWORD,Flags) \
    ICOM_METHOD (HRESULT,Unlock) \
    ICOM_METHOD1(HRESULT,GetDesc,D3DVERTEXBUFFER_DESC*,pDesc)
#define IDirect3DVertexBuffer8_IMETHODS \
    IDirect3DResource8_IMETHODS \
    IDirect3DVertexBuffer8_METHODS
ICOM_DEFINE(IDirect3DVertexBuffer8,IDirect3DResource8)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVertexBuffer8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVertexBuffer8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DVertexBuffer8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DVertexBuffer8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVertexBuffer8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DVertexBuffer8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DVertexBuffer8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DVertexBuffer8_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DVertexBuffer8_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DVertexBuffer8_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DVertexBuffer8_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DVertexBuffer8 methods ***/
#define IDirect3DVertexBuffer8_Lock(p,a,b,c,d)           ICOM_CALL4(Lock,p,a,b,c,d)
#define IDirect3DVertexBuffer8_Unlock(p)                 ICOM_CALL (Unlock,p)
#define IDirect3DVertexBuffer8_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)


/*****************************************************************************
 * IDirect3DIndexBuffer8 interface
 */
#define ICOM_INTERFACE IDirect3DIndexBuffer8
#define IDirect3DIndexBuffer8_METHODS \
    ICOM_METHOD4(HRESULT,Lock,   UINT,OffsetToLock,UINT,SizeToLock,BYTE**,ppbData,DWORD,Flags) \
    ICOM_METHOD (HRESULT,Unlock) \
    ICOM_METHOD1(HRESULT,GetDesc,D3DINDEXBUFFER_DESC*,pDesc)
#define IDirect3DIndexBuffer8_IMETHODS \
    IDirect3DResource8_IMETHODS \
    IDirect3DIndexBuffer8_METHODS
ICOM_DEFINE(IDirect3DIndexBuffer8,IDirect3DResource8)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DIndexBuffer8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DIndexBuffer8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DIndexBuffer8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DResource8 methods ***/
#define IDirect3DIndexBuffer8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DIndexBuffer8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DIndexBuffer8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DIndexBuffer8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DIndexBuffer8_SetPriority(p,a)          ICOM_CALL1(SetPriority,p,a)
#define IDirect3DIndexBuffer8_GetPriority(p)            ICOM_CALL (GetPriority,p)
#define IDirect3DIndexBuffer8_PreLoad(p)                ICOM_CALL (PreLoad,p)
#define IDirect3DIndexBuffer8_GetType(p)                ICOM_CALL (GetType,p)
	/*** IDirect3DIndexBuffer8 methods ***/
#define IDirect3DIndexBuffer8_Lock(p,a,b,c,d)           ICOM_CALL4(Lock,p,a,b,c,d)
#define IDirect3DIndexBuffer8_Unlock(p)                 ICOM_CALL (Unlock,p)
#define IDirect3DIndexBuffer8_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)


/*****************************************************************************
 * IDirect3DSurface8 interface
 */
#define ICOM_INTERFACE IDirect3DSurface8
#define IDirect3DSurface8_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,      IDirect3DDevice8**,ppDevice) \
    ICOM_METHOD4(HRESULT,SetPrivateData, REFGUID,refguid,CONST void*,pData,DWORD,SizeOfData,DWORD,Flags) \
    ICOM_METHOD3(HRESULT,GetPrivateData, REFGUID,refguid,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD1(HRESULT,FreePrivateData,REFGUID,refguid) \
    ICOM_METHOD2(HRESULT,GetContainer,   REFIID,riid,void**,ppContainer) \
    ICOM_METHOD1(HRESULT,GetDesc,        D3DSURFACE_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,LockRect,       D3DLOCKED_RECT*,pLockedRect,CONST RECT*,pRect,DWORD,Flags) \
    ICOM_METHOD (HRESULT,UnlockRect)
#define IDirect3DSurface8_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DSurface8_METHODS
ICOM_DEFINE(IDirect3DSurface8,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DSurface8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DSurface8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DSurface8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DSurface8 methods ***/
#define IDirect3DSurface8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DSurface8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DSurface8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DSurface8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DSurface8_GetContainer(p,a,b)       ICOM_CALL2(GetContainer,p,a,b)
#define IDirect3DSurface8_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)
#define IDirect3DSurface8_LockRect(p,a,b,c)         ICOM_CALL3(LockRect,p,a,b,c)
#define IDirect3DSurface8_UnlockRect(p)             ICOM_CALL (UnlockRect,p)


/*****************************************************************************
 * IDirect3DVolume8 interface
 */
#define ICOM_INTERFACE IDirect3DVolume8
#define IDirect3DVolume8_METHODS \
    ICOM_METHOD1(HRESULT,GetDevice,      IDirect3DDevice8**,ppDevice) \
    ICOM_METHOD4(HRESULT,SetPrivateData, REFGUID,refguid,CONST void*,pData,DWORD,SizeOfData,DWORD,Flags) \
    ICOM_METHOD3(HRESULT,GetPrivateData, REFGUID,refguid,void*,pData,DWORD*,pSizeOfData) \
    ICOM_METHOD1(HRESULT,FreePrivateData,REFGUID,refguid) \
    ICOM_METHOD2(HRESULT,GetContainer,   REFIID,riid,void**,ppContainer) \
    ICOM_METHOD1(HRESULT,GetDesc,        D3DVOLUME_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,LockBox,        D3DLOCKED_BOX*,pLockedVolume,CONST D3DBOX*,pBox,DWORD,Flags) \
    ICOM_METHOD (HRESULT,UnlockBox)
#define IDirect3DVolume8_IMETHODS \
    IUnknown_IMETHODS \
    IDirect3DVolume8_METHODS
ICOM_DEFINE(IDirect3DVolume8,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IDirect3DVolume8_QueryInterface(p,a,b)     ICOM_CALL2(QueryInterface,p,a,b)
#define IDirect3DVolume8_AddRef(p)                 ICOM_CALL (AddRef,p)
#define IDirect3DVolume8_Release(p)                ICOM_CALL (Release,p)
	/*** IDirect3DVolume8 methods ***/
#define IDirect3DVolume8_GetDevice(p,a)            ICOM_CALL1(GetDevice,p,a)
#define IDirect3DVolume8_SetPrivateData(p,a,b,c,d) ICOM_CALL4(SetPrivateData,p,a,b,c,d)
#define IDirect3DVolume8_GetPrivateData(p,a,b,c)   ICOM_CALL3(GetPrivateData,p,a,b,c)
#define IDirect3DVolume8_FreePrivateData(p,a)      ICOM_CALL1(GetPrivateData,p,a)
#define IDirect3DVolume8_GetContainer(p,a,b)       ICOM_CALL2(GetContainer,p,a,b)
#define IDirect3DVolume8_GetDesc(p,a)              ICOM_CALL1(GetDesc,p,a)
#define IDirect3DVolume8_LockBox(p,a,b,c)          ICOM_CALL3(LockBox,p,a,b,c)
#define IDirect3DVolume8_UnlockBox(p)              ICOM_CALL (UnlockBox,p)


IDirect3D8* WINAPI Direct3DCreate8(UINT SDKVersion);

#ifdef __cplusplus
}
#endif

#endif /* DIRECT3D_VERSION < 0x0900 */
#endif /* __WINE_D3D8_H */
