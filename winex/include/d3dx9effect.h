#include "d3dx9.h"

#ifndef __WINEX_D3DX9EFFECT_H
#define __WINEX_D3DX9EFFECT_H


/*************************************************************************
 * Defines
 */

#define D3DXFX_DONOTSAVESTATE         (1 << 0)
#define D3DXFX_DONOTSAVESHADERSTATE   (1 << 1)
#define D3DXFX_DONOTSAVESAMPLERSTATE  (1 << 2)
#define D3DXFX_NOT_CLONEABLE          (1 << 11)

#define D3DX_PARAMETER_SHARED       (1 << 0)
#define D3DX_PARAMETER_LITERAL      (1 << 1)
#define D3DX_PARAMETER_ANNOTATION   (1 << 2)


/*****************************************************************************
 * Predeclare Interfaces
 */

DEFINE_GUID( IID_ID3DXEffectPool,        0x53ca7768, 0xc0d0, 0x4664, 0x8e, 0x79, 0xd1, 0x56, 0xe4, 0xf5, 0xb7, 0xe0);
DEFINE_GUID( IID_ID3DXBaseEffect,        0x804ef574, 0xccc1, 0x4bf6, 0xb0, 0x6a, 0xb1, 0x40, 0x4a, 0xbd, 0xea, 0xde);
DEFINE_GUID( IID_ID3DXEffect,            0xb589b04a, 0x293d, 0x4516, 0xaf, 0xb,  0x3d, 0x7d, 0xbc, 0xf5, 0xac, 0x54);
DEFINE_GUID( IID_ID3DXEffectCompiler,    0xf8ee90d3, 0xfcc6, 0x4f14, 0x8a, 0xe8, 0x63, 0x74, 0xae, 0x96, 0x8e, 0x33);
DEFINE_GUID( IID_ID3DXEffectStateManager,0x79aab587, 0x6dbc, 0x4fa7, 0x82, 0xde, 0x37, 0xfa, 0x17, 0x81, 0xc5, 0xce);


typedef struct ID3DXEffectPool          ID3DXEffectPool, *LPD3DXEFFECTPOOL;
typedef struct ID3DXBaseEffect          ID3DXBaseEffect, *LPD3DXBASEEFFECT;
typedef struct ID3DXEffect              ID3DXEffect, *LPD3DXEFFECT;
typedef struct ID3DXEffectCompiler      ID3DXEffectCompiler, *LPD3DXEFFECTCOMPILER;
typedef struct ID3DXEffectStateManager  ID3DXEffectStateManager, *LPD3DXEFFECTSTATEMANAGER;


/************************************************************************
 * Types, structures, and enums
 */

typedef struct _D3DXEFFECT_DESC
{
    LPCSTR Creator;
    UINT Parameters;
    UINT Techniques;
    UINT Functions;
} D3DXEFFECT_DESC;

typedef struct _D3DXPARAMETER_DESC
{
    LPCSTR Name;
    LPCSTR Semantic;
    D3DXPARAMETER_CLASS Class;
    D3DXPARAMETER_TYPE Type;
    UINT Rows;
    UINT Columns;
    UINT Elements;
    UINT Annotations;
    UINT StructMembers;
    DWORD Flags;
    UINT Bytes;
} D3DXPARAMETER_DESC;

typedef struct _D3DXTECHNIQUE_DESC
{
    LPCSTR Name;
    UINT Passes;
    UINT Annotations;
} D3DXTECHNIQUE_DESC;

typedef struct _D3DXPASS_DESC
{
    LPCSTR Name;
    UINT Annotations;

#if 0
    /* This stuff is only available in very early SDK versions */
    DWORD VSVersion;
    DWORD PSVersion;

    UINT VSSemanticsUsed;
    D3DXSEMANTIC VSSemantics[MAXD3DDECLLENGTH];

    UINT PSSemanticsUsed;
    D3DXSEMANTIC PSSemantics[MAXD3DDECLLENGTH];

    UINT PSSamplersUsed;
    LPCSTR PSSamplers[16];
#else
    /* Available in later SDK versions */
    CONST DWORD* pVertexShaderFunction;
    CONST DWORD* pPixelShaderFunction;
#endif

} D3DXPASS_DESC;

typedef struct _D3DXFUNCTION_DESC
{
    LPCSTR Name;
    UINT Annotations;
} D3DXFUNCTION_DESC;


/*****************************************************************************
 * Interfaces
 */

/*****************************************************************************
 * ID3DXEffectPool interface (no public methods besides IUnknown methods)
 */
#define ICOM_INTERFACE ID3DXEffectPool
#define ID3DXEffectPool_METHODS  /* Empty */
#define ID3DXEffectPool_IMETHODS \
    IUnknown_IMETHODS
ICOM_DEFINE(ID3DXEffectPool,IUnknown)
#undef ICOM_INTERFACE
/*** IUnknown methods ***/
#define ID3DXEffectPool_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXEffectPool_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXEffectPool_Release(p) ICOM_CALL(Release,p)


/*****************************************************************************
 * ID3DXBaseEffect interface
 */
#define ICOM_INTERFACE ID3DXBaseEffect
#define ID3DXBaseEffect_METHODS \
    ICOM_METHOD1 (HRESULT,GetDesc,D3DXEFFECT_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,GetParameterDesc,D3DXHANDLE,hParameter,D3DXPARAMETER_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,GetTechniqueDesc,D3DXHANDLE,hTechnique,D3DXTECHNIQUE_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,GetPassDesc,D3DXHANDLE,hShader,D3DXPASS_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,GetFunctionDesc,D3DXHANDLE,hShader,D3DXFUNCTION_DESC*,pDesc) \
    ICOM_METHOD2 (D3DXHANDLE,GetParameter,D3DXHANDLE,hParameter,UINT,Index) \
    ICOM_METHOD2 (D3DXHANDLE,GetParameterByName,D3DXHANDLE,hParameter,LPCSTR,pName) \
    ICOM_METHOD2 (D3DXHANDLE,GetParameterBySemantic,D3DXHANDLE,hParameter,LPCSTR,pSemantic) \
    ICOM_METHOD2 (D3DXHANDLE,GetParameterElement,D3DXHANDLE,hParameter,UINT,Index) \
    ICOM_METHOD1 (D3DXHANDLE,GetTechnique,UINT,Index) \
    ICOM_METHOD1 (D3DXHANDLE,GetTechniqueByName,LPCSTR,pName) \
    ICOM_METHOD2 (D3DXHANDLE,GetPass,D3DXHANDLE,hTechnique,UINT,Index) \
    ICOM_METHOD2 (D3DXHANDLE,GetPassByName,D3DXHANDLE,hTechnique,LPCSTR,pName) \
    ICOM_METHOD1 (D3DXHANDLE,GetFunction,UINT,Index) \
    ICOM_METHOD1 (D3DXHANDLE,GetFunctionByName,LPCSTR,pName) \
    ICOM_METHOD2 (D3DXHANDLE,GetAnnotation,D3DXHANDLE,hObject,UINT,Index) \
    ICOM_METHOD2 (D3DXHANDLE,GetAnnotationByName,D3DXHANDLE,hObject,LPCSTR,pName) \
    ICOM_METHOD3 (HRESULT,SetValue,D3DXHANDLE,hParm,LPCVOID,pData,UINT,Bytes) \
    ICOM_METHOD3 (HRESULT,GetValue,D3DXHANDLE,hParm,LPVOID,pData,UINT,Bytes) \
    ICOM_METHOD2 (HRESULT,SetBool,D3DXHANDLE,hParm,BOOL,b) \
    ICOM_METHOD2 (HRESULT,GetBool,D3DXHANDLE,hParm,BOOL*,pb) \
    ICOM_METHOD3 (HRESULT,SetBoolArray,D3DXHANDLE,hParm,CONST BOOL*,pb,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetBoolArray,D3DXHANDLE,hParm,BOOL*,pb,UINT,Count) \
    ICOM_METHOD2 (HRESULT,SetInt,D3DXHANDLE,hParm,INT,n) \
    ICOM_METHOD2 (HRESULT,GetInt,D3DXHANDLE,hParm,INT*,pn) \
    ICOM_METHOD3 (HRESULT,SetIntArray,D3DXHANDLE,hParm,CONST INT*,pn,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetIntArray,D3DXHANDLE,hParm,INT*,pn,UINT,Count) \
    ICOM_METHOD2 (HRESULT,SetFloat,D3DXHANDLE,hParm,FLOAT,f) \
    ICOM_METHOD2 (HRESULT,GetFloat,D3DXHANDLE,hParm,FLOAT*,pf) \
    ICOM_METHOD3 (HRESULT,SetFloatArray,D3DXHANDLE,hParm,CONST FLOAT*,pf,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetFloatArray,D3DXHANDLE,hParm,FLOAT*,pf,UINT,Count) \
    ICOM_METHOD2 (HRESULT,SetVector,D3DXHANDLE,hParm,CONST D3DXVECTOR4*,pVector) \
    ICOM_METHOD2 (HRESULT,GetVector,D3DXHANDLE,hParm,D3DXVECTOR4*,pVector) \
    ICOM_METHOD3 (HRESULT,SetVectorArray,D3DXHANDLE,hParm,CONST D3DXVECTOR4*,pVector,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetVectorArray,D3DXHANDLE,hParm,D3DXVECTOR4*,pVector,UINT,Count) \
    ICOM_METHOD2 (HRESULT,SetMatrix,D3DXHANDLE,hParm,CONST D3DXMATRIX*,pMatrix) \
    ICOM_METHOD2 (HRESULT,GetMatrix,D3DXHANDLE,hParm,D3DXMATRIX*,pMatrix) \
    ICOM_METHOD3 (HRESULT,SetMatrixArray,D3DXHANDLE,hParm,CONST D3DXMATRIX*,pMatrix,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetMatrixArray,D3DXHANDLE,hParm,D3DXMATRIX*,pMatrix,UINT,Count) \
    ICOM_METHOD3 (HRESULT,SetMatrixPointerArray,D3DXHANDLE,hParm,CONST D3DXMATRIX**,ppMatrix,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetMatrixPointerArray,D3DXHANDLE,hParm,D3DXMATRIX**,ppMatrix,UINT,Count) \
    ICOM_METHOD2 (HRESULT,SetMatrixTranspose,D3DXHANDLE,hParm,CONST D3DXMATRIX*,pMatrix) \
    ICOM_METHOD2 (HRESULT,GetMatrixTranspose,D3DXHANDLE,hParm,D3DXMATRIX*,pMatrix) \
    ICOM_METHOD3 (HRESULT,SetMatrixTransposeArray,D3DXHANDLE,hParm,CONST D3DXMATRIX*,pMatrix,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetMatrixTransposeArray,D3DXHANDLE,hParm,D3DXMATRIX*,pMatrix,UINT,Count) \
    ICOM_METHOD3 (HRESULT,SetMatrixTransposePointerArray,D3DXHANDLE,hParm,CONST D3DXMATRIX**,ppMatrix,UINT,Count) \
    ICOM_METHOD3 (HRESULT,GetMatrixTransposePointerArray,D3DXHANDLE,hParm,D3DXMATRIX**,ppMatrix,UINT,Count) \
    ICOM_METHOD2 (HRESULT,SetString,D3DXHANDLE,hParm,LPCSTR,pString) \
    ICOM_METHOD2 (HRESULT,GetString,D3DXHANDLE,hParm,LPCSTR*,ppString) \
    ICOM_METHOD2 (HRESULT,SetTexture,D3DXHANDLE,hParm,LPDIRECT3DBASETEXTURE9,pTexture) \
    ICOM_METHOD2 (HRESULT,GetTexture,D3DXHANDLE,hParm,LPDIRECT3DBASETEXTURE9*,ppTexture) \
/*    ICOM_METHOD2 (HRESULT,SetPixelShader,D3DXHANDLE,hParm,LPDIRECT3DPIXELSHADER9,pPShader) */ \
    ICOM_METHOD2 (HRESULT,GetPixelShader,D3DXHANDLE,hParm,LPDIRECT3DPIXELSHADER9*,ppPShader) \
/*    ICOM_METHOD2 (HRESULT,SetVertexShader,D3DXHANDLE,hParm,LPDIRECT3DVERTEXSHADER9,pVShader) */ \
    ICOM_METHOD2 (HRESULT,GetVertexShader,D3DXHANDLE,hParm,LPDIRECT3DVERTEXSHADER9*,ppVShader) \
     ICOM_METHOD3 (HRESULT,SetArrayRange,D3DXHANDLE,hParm,UINT,uStart,UINT,uEnd)
#define ID3DXBaseEffect_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXBaseEffect_METHODS
ICOM_DEFINE(ID3DXBaseEffect,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXBaseEffect_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXBaseEffect_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXBaseEffect_Release(p) ICOM_CALL(Release,p)
/*** ID3DXBaseEffect methods ***/
#define ID3DXBaseEffect_GetDesc(p,a) ICOM_CALL1(GetDesc,p,a)
#define ID3DXBaseEffect_GetParameterDesc(p,a,b) ICOM_CALL2(GetParameterDesc,p,a,b)
#define ID3DXBaseEffect_GetTechniqueDesc(p,a,b) ICOM_CALL2(GetTechniqueDesc,p,a,b)
#define ID3DXBaseEffect_GetPassDesc(p,a,b) ICOM_CALL2(GetPassDesc,p,a,b)
#define ID3DXBaseEffect_GetFunctionDesc(p,a,b) ICOM_CALL2(GetFunctionDesc,p,a,b)
#define ID3DXBaseEffect_GetParameter(p,a,b) ICOM_CALL2(GetParameter,p,a,b)
#define ID3DXBaseEffect_GetParameterByName(p,a,b) ICOM_CALL2(GetParameterByName,p,a,b)
#define ID3DXBaseEffect_GetParameterBySemantic(p,a,b) ICOM_CALL2(GetParameterBySemantic,p,a,b)
#define ID3DXBaseEffect_GetParameterElement(p,a,b) ICOM_CALL2(GetParameterElement,p,a,b)
#define ID3DXBaseEffect_GetTechnique(p,a) ICOM_CALL1(GetTechnique,p,a)
#define ID3DXBaseEffect_GetTechniqueByName(p,a) ICOM_CALL1(GetTechniqueByName,p,a)
#define ID3DXBaseEffect_GetPass(p,a,b) ICOM_CALL2(GetPass,p,a,b)
#define ID3DXBaseEffect_GetPassByName(p,a,b) ICOM_CALL2(GetPassByName,p,a,b)
#define ID3DXBaseEffect_GetFunction(p,a) ICOM_CALL1(GetFunction,p,a)
#define ID3DXBaseEffect_GetFunctionByName(p,a) ICOM_CALL1(GetFunctionByName,p,a)
#define ID3DXBaseEffect_GetAnnotation(p,a,b) ICOM_CALL2(GetAnnotation,p,a,b)
#define ID3DXBaseEffect_GetAnnotationByName(p,a,b) ICOM_CALL2(GetAnnotationByName,p,a,b)
#define ID3DXBaseEffect_SetValue(p,a,b,c) ICOM_CALL3(SetValue,p,a,b,c)
#define ID3DXBaseEffect_GetValue(p,a,b,c) ICOM_CALL3(GetValue,p,a,b,c)
#define ID3DXBaseEffect_SetBool(p,a,b) ICOM_CALL2(SetBool,p,a,b)
#define ID3DXBaseEffect_GetBool(p,a,b) ICOM_CALL2(GetBool,p,a,b)
#define ID3DXBaseEffect_SetBoolArray(p,a,b,c) ICOM_CALL3(SetBoolArray,p,a,b,c)
#define ID3DXBaseEffect_GetBoolArray(p,a,b,c) ICOM_CALL3(GetBoolArray,p,a,b,c)
#define ID3DXBaseEffect_SetInt(p,a,b) ICOM_CALL2(SetInt,p,a,b)
#define ID3DXBaseEffect_GetInt(p,a,b) ICOM_CALL2(GetInt,p,a,b)
#define ID3DXBaseEffect_SetIntArray(p,a,b,c) ICOM_CALL3(SetIntArray,p,a,b,c)
#define ID3DXBaseEffect_GetIntArray(p,a,b,c) ICOM_CALL3(GetIntArray,p,a,b,c)
#define ID3DXBaseEffect_SetFloat(p,a,b) ICOM_CALL2(SetFloat,p,a,b)
#define ID3DXBaseEffect_GetFloat(p,a,b) ICOM_CALL2(GetFloat,p,a,b)
#define ID3DXBaseEffect_SetFloatArray(p,a,b,c) ICOM_CALL3(SetFloatArray,p,a,b,c)
#define ID3DXBaseEffect_GetFloatArray(p,a,b,c) ICOM_CALL3(GetFloatArray,p,a,b,c)
#define ID3DXBaseEffect_SetVector(p,a,b) ICOM_CALL2(SetVector,p,a,b)
#define ID3DXBaseEffect_GetVector(p,a,b) ICOM_CALL2(GetVector,p,a,b)
#define ID3DXBaseEffect_SetVectorArray(p,a,b,c) ICOM_CALL3(SetVectorArray,p,a,b,c)
#define ID3DXBaseEffect_GetVectorArray(p,a,b,c) ICOM_CALL3(GetVectorArray,p,a,b,c)
#define ID3DXBaseEffect_SetMatrix(p,a,b) ICOM_CALL2(SetMatrix,p,a,b)
#define ID3DXBaseEffect_GetMatrix(p,a,b) ICOM_CALL2(GetMatrix,p,a,b)
#define ID3DXBaseEffect_SetMatrixArray(p,a,b,c) ICOM_CALL3(SetMatrixArray,p,a,b,c)
#define ID3DXBaseEffect_GetMatrixArray(p,a,b,c) ICOM_CALL3(GetMatrixArray,p,a,b,c)
#define ID3DXBaseEffect_SetMatrixPointerArray(p,a,b,c) ICOM_CALL3(SetMatrixPointerArray,p,a,b,c)
#define ID3DXBaseEffect_GetMatrixPointerArray(p,a,b,c) ICOM_CALL3(GetMatrixPointerArray,p,a,b,c)
#define ID3DXBaseEffect_SetMatrixTranspose(p,a,b) ICOM_CALL2(SetMatrixTranspose,p,a,b)
#define ID3DXBaseEffect_GetMatrixTranspose(p,a,b) ICOM_CALL2(GetMatrixTranspose,p,a,b)
#define ID3DXBaseEffect_SetMatrixTransposeArray(p,a,b,c) ICOM_CALL3(SetMatrixTransposeArray,p,a,b,c)
#define ID3DXBaseEffect_GetMatrixTransposeArray(p,a,b,c) ICOM_CALL3(GetMatrixTransposeArray,p,a,b,c)
#define ID3DXBaseEffect_SetMatrixTransposePointerArray(p,a,b,c) ICOM_CALL3(SetMatrixTransposePointerArray,p,a,b,c)
#define ID3DXBaseEffect_GetMatrixTransposePointerArray(p,a,b,c) ICOM_CALL3(GetMatrixTransposePointerArray,p,a,b,c)
#define ID3DXBaseEffect_SetString(p,a,b) ICOM_CALL2(SetString,p,a,b)
#define ID3DXBaseEffect_GetString(p,a,b) ICOM_CALL2(GetString,p,a,b)
#define ID3DXBaseEffect_SetTexture(p,a,b) ICOM_CALL2(SetTexture,p,a,b)
#define ID3DXBaseEffect_GetTexture(p,a,b) ICOM_CALL2(GetTexture,p,a,b)
/* #define ID3DXBaseEffect_SetPixelShader(p,a,b) ICOM_CALL2(SetPixelShader,p,a,b) */
#define ID3DXBaseEffect_GetPixelShader(p,a,b) ICOM_CALL2(GetPixelShader,p,a,b)
/* #define ID3DXBaseEffect_SetVertexShader(p,a,b) ICOM_CALL2(SetVertexShader,p,a,b) */
#define ID3DXBaseEffect_GetVertexShader(p,a,b) ICOM_CALL2(GetVertexShader,p,a,b)
#define ID3DXBaseEffect_SetArrayRange(p,a,b,c) ICOM_CALL3(SetArrayRange,p,a,b,c)

/*****************************************************************************
 * ID3DXEffect interface
 */
#define ICOM_INTERFACE ID3DXEffect
#define ID3DXEffect_METHODS \
    ICOM_METHOD1 (HRESULT,GetPool,LPD3DXEFFECTPOOL*,ppPool) \
    ICOM_METHOD1 (HRESULT,SetTechnique,D3DXHANDLE,hTechnique) \
    ICOM_METHOD  (D3DXHANDLE,GetCurrentTechnique) \
    ICOM_METHOD1 (HRESULT,ValidateTechnique,D3DXHANDLE,hTechnique) \
    ICOM_METHOD2 (HRESULT,FindNextValidTechnique,D3DXHANDLE,hTechnique,D3DXHANDLE*,pTechnique) \
    ICOM_METHOD2 (BOOL,IsParameterUsed,D3DXHANDLE,hParameter,D3DXHANDLE,hTechnique) \
    ICOM_METHOD2 (HRESULT,Begin,UINT*,pPasses,DWORD,Flags) \
    ICOM_METHOD1 (HRESULT,BeginPass,UINT,Pass) \
    ICOM_METHOD  (HRESULT,CommitChanges) \
    ICOM_METHOD  (HRESULT,EndPass) \
    ICOM_METHOD  (HRESULT,End) \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD  (HRESULT,OnLostDevice) \
    ICOM_METHOD  (HRESULT,OnResetDevice) \
    ICOM_METHOD1 (HRESULT,SetStateManager,LPD3DXEFFECTSTATEMANAGER,pManager) \
    ICOM_METHOD1 (HRESULT,GetStateManager,LPD3DXEFFECTSTATEMANAGER*,ppManager) \
    ICOM_METHOD  (HRESULT,BeginParameterBlock) \
    ICOM_METHOD  (D3DXHANDLE,EndParameterBlock) \
    ICOM_METHOD1 (HRESULT,ApplyParameterBlock,D3DXHANDLE,hParameterBlock) \
    ICOM_METHOD1 (HRESULT,DeleteParameterBlock,D3DXHANDLE,hParameterBlock) \
    ICOM_METHOD2 (HRESULT,CloneEffect,LPDIRECT3DDEVICE9,pDevice,LPD3DXEFFECT*,ppEffect) \
    ICOM_METHOD4 (HRESULT,SetRawValue,D3DXHANDLE,hParameter,LPCVOID,pData,UINT,ByteOffset,UINT,Bytes)
#define ID3DXEffect_IMETHODS \
    ID3DXBaseEffect_IMETHODS \
    ID3DXEffect_METHODS
ICOM_DEFINE(ID3DXEffect,ID3DXBaseEffect)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXEffect_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXEffect_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXEffect_Release(p) ICOM_CALL(Release,p)
/*** ID3DXBaseEffect methods ***/
#define ID3DXEffect_GetDesc(p,a) ICOM_CALL1(GetDesc,p,a)
#define ID3DXEffect_GetParameterDesc(p,a,b) ICOM_CALL2(GetParameterDesc,p,a,b)
#define ID3DXEffect_GetTechniqueDesc(p,a,b) ICOM_CALL2(GetTechniqueDesc,p,a,b)
#define ID3DXEffect_GetPassDesc(p,a,b) ICOM_CALL2(GetPassDesc,p,a,b)
#define ID3DXEffect_GetFunctionDesc(p,a,b) ICOM_CALL2(GetFunctionDesc,p,a,b)
#define ID3DXEffect_GetParameter(p,a,b) ICOM_CALL2(GetParameter,p,a,b)
#define ID3DXEffect_GetParameterByName(p,a,b) ICOM_CALL2(GetParameterByName,p,a,b)
#define ID3DXEffect_GetParameterBySemantic(p,a,b) ICOM_CALL2(GetParameterBySemantic,p,a,b)
#define ID3DXEffect_GetParameterElement(p,a,b) ICOM_CALL2(GetParameterElement,p,a,b)
#define ID3DXEffect_GetTechnique(p,a) ICOM_CALL1(GetTechnique,p,a)
#define ID3DXEffect_GetTechniqueByName(p,a) ICOM_CALL1(GetTechniqueByName,p,a)
#define ID3DXEffect_GetPass(p,a,b) ICOM_CALL2(GetPass,p,a,b)
#define ID3DXEffect_GetPassByName(p,a,b) ICOM_CALL2(GetPassByName,p,a,b)
#define ID3DXEffect_GetFunction(p,a) ICOM_CALL1(GetFunction,p,a)
#define ID3DXEffect_GetFunctionByName(p,a) ICOM_CALL1(GetFunctionByName,p,a)
#define ID3DXEffect_GetAnnotation(p,a,b) ICOM_CALL2(GetAnnotation,p,a,b)
#define ID3DXEffect_GetAnnotationByName(p,a,b) ICOM_CALL2(GetAnnotationByName,p,a,b)
#define ID3DXEffect_SetValue(p,a,b,c) ICOM_CALL3(SetValue,p,a,b,c)
#define ID3DXEffect_GetValue(p,a,b,c) ICOM_CALL3(GetValue,p,a,b,c)
#define ID3DXEffect_SetBool(p,a,b) ICOM_CALL2(SetBool,p,a,b)
#define ID3DXEffect_GetBool(p,a,b) ICOM_CALL2(GetBool,p,a,b)
#define ID3DXEffect_SetBoolArray(p,a,b,c) ICOM_CALL3(SetBoolArray,p,a,b,c)
#define ID3DXEffect_GetBoolArray(p,a,b,c) ICOM_CALL3(GetBoolArray,p,a,b,c)
#define ID3DXEffect_SetInt(p,a,b) ICOM_CALL2(SetInt,p,a,b)
#define ID3DXEffect_GetInt(p,a,b) ICOM_CALL2(GetInt,p,a,b)
#define ID3DXEffect_SetIntArray(p,a,b,c) ICOM_CALL3(SetIntArray,p,a,b,c)
#define ID3DXEffect_GetIntArray(p,a,b,c) ICOM_CALL3(GetIntArray,p,a,b,c)
#define ID3DXEffect_SetFloat(p,a,b) ICOM_CALL2(SetFloat,p,a,b)
#define ID3DXEffect_GetFloat(p,a,b) ICOM_CALL2(GetFloat,p,a,b)
#define ID3DXEffect_SetFloatArray(p,a,b,c) ICOM_CALL3(SetFloatArray,p,a,b,c)
#define ID3DXEffect_GetFloatArray(p,a,b,c) ICOM_CALL3(GetFloatArray,p,a,b,c)
#define ID3DXEffect_SetVector(p,a,b) ICOM_CALL2(SetVector,p,a,b)
#define ID3DXEffect_GetVector(p,a,b) ICOM_CALL2(GetVector,p,a,b)
#define ID3DXEffect_SetVectorArray(p,a,b,c) ICOM_CALL3(SetVectorArray,p,a,b,c)
#define ID3DXEffect_GetVectorArray(p,a,b,c) ICOM_CALL3(GetVectorArray,p,a,b,c)
#define ID3DXEffect_SetMatrix(p,a,b) ICOM_CALL2(SetMatrix,p,a,b)
#define ID3DXEffect_GetMatrix(p,a,b) ICOM_CALL2(GetMatrix,p,a,b)
#define ID3DXEffect_SetMatrixArray(p,a,b,c) ICOM_CALL3(SetMatrixArray,p,a,b,c)
#define ID3DXEffect_GetMatrixArray(p,a,b,c) ICOM_CALL3(GetMatrixArray,p,a,b,c)
#define ID3DXEffect_SetMatrixPointerArray(p,a,b,c) ICOM_CALL3(SetMatrixPointerArray,p,a,b,c)
#define ID3DXEffect_GetMatrixPointerArray(p,a,b,c) ICOM_CALL3(GetMatrixPointerArray,p,a,b,c)
#define ID3DXEffect_SetMatrixTranspose(p,a,b) ICOM_CALL2(SetMatrixTranspose,p,a,b)
#define ID3DXEffect_GetMatrixTranspose(p,a,b) ICOM_CALL2(GetMatrixTranspose,p,a,b)
#define ID3DXEffect_SetMatrixTransposeArray(p,a,b,c) ICOM_CALL3(SetMatrixTransposeArray,p,a,b,c)
#define ID3DXEffect_GetMatrixTransposeArray(p,a,b,c) ICOM_CALL3(GetMatrixTransposeArray,p,a,b,c)
#define ID3DXEffect_SetMatrixTransposePointerArray(p,a,b,c) ICOM_CALL3(SetMatrixTransposePointerArray,p,a,b,c)
#define ID3DXEffect_GetMatrixTransposePointerArray(p,a,b,c) ICOM_CALL3(GetMatrixTransposePointerArray,p,a,b,c)
#define ID3DXEffect_SetString(p,a,b) ICOM_CALL2(SetString,p,a,b)
#define ID3DXEffect_GetString(p,a,b) ICOM_CALL2(GetString,p,a,b)
#define ID3DXEffect_SetTexture(p,a,b) ICOM_CALL2(SetTexture,p,a,b)
#define ID3DXEffect_GetTexture(p,a,b) ICOM_CALL2(GetTexture,p,a,b)
/* #define ID3DXEffect_SetPixelShader(p,a,b) ICOM_CALL2(SetPixelShader,p,a,b) */
#define ID3DXEffect_GetPixelShader(p,a,b) ICOM_CALL2(GetPixelShader,p,a,b)
/* #define ID3DXEffect_SetVertexShader(p,a,b) ICOM_CALL2(SetVertexShader,p,a,b) */
#define ID3DXEffect_GetVertexShader(p,a,b) ICOM_CALL2(GetVertexShader,p,a,b)
#define ID3DXEffect_SetArrayRange(p,a,b,c) ICOM_CALL3(SetArrayRange,p,a,b,c)
/*** ID3DXEffect methods ***/
#define ID3DXEffect_GetPool(p,a) ICOM_CALL1(GetPool,p,a)
#define ID3DXEffect_SetTechnique(p,a) ICOM_CALL1(SetTechnique,p,a)
#define ID3DXEffect_GetCurrentTechnique(p) ICOM_CALL(GetCurrentTechnique,p)
#define ID3DXEffect_ValidateTechnique(p,a) ICOM_CALL1(ValidateTechnique,p,a)
#define ID3DXEffect_FindNextValidTechnique(p,a,b) ICOM_CALL2(FindNextValidTechnique,p,a,b)
#define ID3DXEffect_IsParameterUsed(p,a,b) ICOM_CALL2(IsParameterUsed,p,a,b)
#define ID3DXEffect_Begin(p,a,b) ICOM_CALL2(Begin,p,a,b)
#define ID3DXEffect_BeginPass(p,a) ICOM_CALL1(BeginPass,p,a)
#define ID3DXEffect_CommitChanges(p) ICOM_CALL(CommitChanges,p)
#define ID3DXEffect_EndPass(p) ICOM_CALL(EndPass,p)
#define ID3DXEffect_End(p) ICOM_CALL(End,p)
#define ID3DXEffect_GetDevice(p,a) ICOM_CALL1(GetDevice,p,a)
#define ID3DXEffect_OnLostDevice(p) ICOM_CALL(OnLostDevice,p)
#define ID3DXEffect_OnResetDevice(p) ICOM_CALL(OnResetDevice,p)
#define ID3DXEffect_SetStateManager(p,a) ICOM_CALL1(SetStateManager,p,a)
#define ID3DXEffect_GetStateManager(p,a) ICOM_CALL1(GetStateManager,p,a)
#define ID3DXEffect_BeginParameterBlock(p) ICOM_CALL(BeginParameterBlock,p)
#define ID3DXEffect_EndParameterBlock(p) ICOM_CALL(EndParameterBlock,p)
#define ID3DXEffect_ApplyParameterBlock(p,a) ICOM_CALL1(ApplyParameterBlock,p,a)
#define ID3DXEffect_DeleteParameterBlock(p,a) ICOM_CALL1(DeleteParameterBlock,p,a)
#define ID3DXEffect_CloneEffect(p,a,b) ICOM_CALL2(CloneEffect,p,a,b)
#define ID3DXEffect_SetRawValue(p,a,b,c,d) ICOM_CALL4(SetRawValue,p,a,b,c,d)

/*****************************************************************************
 * ID3DXEffectCompiler interface
 */
#define ICOM_INTERFACE ID3DXEffectCompiler
#define ID3DXEffectCompiler_METHODS \
    ICOM_METHOD2 (HRESULT,SetLiteral,D3DXHANDLE,hParameter,BOOL,Literal) \
    ICOM_METHOD2 (HRESULT,GetLiteral,D3DXHANDLE,hParameter,BOOL*,pLiteral) \
    ICOM_METHOD3 (HRESULT,CompileEffect,DWORD,Flags,LPD3DXBUFFER*,ppEffect,LPD3DXBUFFER*,ppErrorMsgs) \
    ICOM_METHOD6 (HRESULT,CompileShader,D3DXHANDLE,hFunction,LPCSTR,pTarget,DWORD,Flags,LPD3DXBUFFER*,ppShader,LPD3DXBUFFER*,ppErrorMsgs,LPD3DXCONSTANTTABLE*,ppConstantTable)
#define ID3DXEffectCompiler_IMETHODS \
    ID3DXBaseEffect_IMETHODS \
    ID3DXEffectCompiler_METHODS
ICOM_DEFINE(ID3DXEffectCompiler,ID3DXBaseEffect)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXEffectCompiler_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXEffectCompiler_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXEffectCompiler_Release(p) ICOM_CALL(Release,p)
/*** ID3DXBaseEffect methods ***/
#define ID3DXEffectCompiler_GetDesc(p,a) ICOM_CALL1(GetDesc,p,a)
#define ID3DXEffectCompiler_GetParameterDesc(p,a,b) ICOM_CALL2(GetParameterDesc,p,a,b)
#define ID3DXEffectCompiler_GetTechniqueDesc(p,a,b) ICOM_CALL2(GetTechniqueDesc,p,a,b)
#define ID3DXEffectCompiler_GetPassDesc(p,a,b) ICOM_CALL2(GetPassDesc,p,a,b)
#define ID3DXEffectCompiler_GetFunctionDesc(p,a,b) ICOM_CALL2(GetFunctionDesc,p,a,b)
#define ID3DXEffectCompiler_GetParameter(p,a,b) ICOM_CALL2(GetParameter,p,a,b)
#define ID3DXEffectCompiler_GetParameterByName(p,a,b) ICOM_CALL2(GetParameterByName,p,a,b)
#define ID3DXEffectCompiler_GetParameterBySemantic(p,a,b) ICOM_CALL2(GetParameterBySemantic,p,a,b)
#define ID3DXEffectCompiler_GetParameterElement(p,a,b) ICOM_CALL2(GetParameterElement,p,a,b)
#define ID3DXEffectCompiler_GetTechnique(p,a) ICOM_CALL1(GetTechnique,p,a)
#define ID3DXEffectCompiler_GetTechniqueByName(p,a) ICOM_CALL1(GetTechniqueByName,p,a)
#define ID3DXEffectCompiler_GetPass(p,a,b) ICOM_CALL2(GetPass,p,a,b)
#define ID3DXEffectCompiler_GetPassByName(p,a,b) ICOM_CALL2(GetPassByName,p,a,b)
#define ID3DXEffectCompiler_GetFunction(p,a) ICOM_CALL1(GetFunction,p,a)
#define ID3DXEffectCompiler_GetFunctionByName(p,a) ICOM_CALL1(GetFunctionByName,p,a)
#define ID3DXEffectCompiler_GetAnnotation(p,a,b) ICOM_CALL2(GetAnnotation,p,a,b)
#define ID3DXEffectCompiler_GetAnnotationByName(p,a,b) ICOM_CALL2(GetAnnotationByName,p,a,b)
#define ID3DXEffectCompiler_SetValue(p,a,b,c) ICOM_CALL3(SetValue,p,a,b,c)
#define ID3DXEffectCompiler_GetValue(p,a,b,c) ICOM_CALL3(GetValue,p,a,b,c)
#define ID3DXEffectCompiler_SetBool(p,a,b) ICOM_CALL2(SetBool,p,a,b)
#define ID3DXEffectCompiler_GetBool(p,a,b) ICOM_CALL2(GetBool,p,a,b)
#define ID3DXEffectCompiler_SetBoolArray(p,a,b,c) ICOM_CALL3(SetBoolArray,p,a,b,c)
#define ID3DXEffectCompiler_GetBoolArray(p,a,b,c) ICOM_CALL3(GetBoolArray,p,a,b,c)
#define ID3DXEffectCompiler_SetInt(p,a,b) ICOM_CALL2(SetInt,p,a,b)
#define ID3DXEffectCompiler_GetInt(p,a,b) ICOM_CALL2(GetInt,p,a,b)
#define ID3DXEffectCompiler_SetIntArray(p,a,b,c) ICOM_CALL3(SetIntArray,p,a,b,c)
#define ID3DXEffectCompiler_GetIntArray(p,a,b,c) ICOM_CALL3(GetIntArray,p,a,b,c)
#define ID3DXEffectCompiler_SetFloat(p,a,b) ICOM_CALL2(SetFloat,p,a,b)
#define ID3DXEffectCompiler_GetFloat(p,a,b) ICOM_CALL2(GetFloat,p,a,b)
#define ID3DXEffectCompiler_SetFloatArray(p,a,b,c) ICOM_CALL3(SetFloatArray,p,a,b,c)
#define ID3DXEffectCompiler_GetFloatArray(p,a,b,c) ICOM_CALL3(GetFloatArray,p,a,b,c)
#define ID3DXEffectCompiler_SetVector(p,a,b) ICOM_CALL2(SetVector,p,a,b)
#define ID3DXEffectCompiler_GetVector(p,a,b) ICOM_CALL2(GetVector,p,a,b)
#define ID3DXEffectCompiler_SetVectorArray(p,a,b,c) ICOM_CALL3(SetVectorArray,p,a,b,c)
#define ID3DXEffectCompiler_GetVectorArray(p,a,b,c) ICOM_CALL3(GetVectorArray,p,a,b,c)
#define ID3DXEffectCompiler_SetMatrix(p,a,b) ICOM_CALL2(SetMatrix,p,a,b)
#define ID3DXEffectCompiler_GetMatrix(p,a,b) ICOM_CALL2(GetMatrix,p,a,b)
#define ID3DXEffectCompiler_SetMatrixArray(p,a,b,c) ICOM_CALL3(SetMatrixArray,p,a,b,c)
#define ID3DXEffectCompiler_GetMatrixArray(p,a,b,c) ICOM_CALL3(GetMatrixArray,p,a,b,c)
#define ID3DXEffectCompiler_SetMatrixPointerArray(p,a,b,c) ICOM_CALL3(SetMatrixPointerArray,p,a,b,c)
#define ID3DXEffectCompiler_GetMatrixPointerArray(p,a,b,c) ICOM_CALL3(GetMatrixPointerArray,p,a,b,c)
#define ID3DXEffectCompiler_SetMatrixTranspose(p,a,b) ICOM_CALL2(SetMatrixTranspose,p,a,b)
#define ID3DXEffectCompiler_GetMatrixTranspose(p,a,b) ICOM_CALL2(GetMatrixTranspose,p,a,b)
#define ID3DXEffectCompiler_SetMatrixTransposeArray(p,a,b,c) ICOM_CALL3(SetMatrixTransposeArray,p,a,b,c)
#define ID3DXEffectCompiler_GetMatrixTransposeArray(p,a,b,c) ICOM_CALL3(GetMatrixTransposeArray,p,a,b,c)
#define ID3DXEffectCompiler_SetMatrixTransposePointerArray(p,a,b,c) ICOM_CALL3(SetMatrixTransposePointerArray,p,a,b,c)
#define ID3DXEffectCompiler_GetMatrixTransposePointerArray(p,a,b,c) ICOM_CALL3(GetMatrixTransposePointerArray,p,a,b,c)
#define ID3DXEffectCompiler_SetString(p,a,b) ICOM_CALL2(SetString,p,a,b)
#define ID3DXEffectCompiler_GetString(p,a,b) ICOM_CALL2(GetString,p,a,b)
#define ID3DXEffectCompiler_SetTexture(p,a,b) ICOM_CALL2(SetTexture,p,a,b)
#define ID3DXEffectCompiler_GetTexture(p,a,b) ICOM_CALL2(GetTexture,p,a,b)
/* #define ID3DXEffectCompiler_SetPixelShader(p,a,b) ICOM_CALL2(SetPixelShader,p,a,b) */
#define ID3DXEffectCompiler_GetPixelShader(p,a,b) ICOM_CALL2(GetPixelShader,p,a,b)
/* #define ID3DXEffectCompiler_SetVertexShader(p,a,b) ICOM_CALL2(SetVertexShader,p,a,b) */
#define ID3DXEffectCompiler_GetVertexShader(p,a,b) ICOM_CALL2(GetVertexShader,p,a,b)
#define ID3DXEffectCompiler_SetArrayRange(p,a,b,c) ICOM_CALL3(SetArrayRange,p,a,b,c)
/*** ID3DXEffectCompiler methods ***/
#define ID3DXEffectCompiler_SetLiteral(p,a,b) ICOM_CALL2(SetLiteral,p,a,b)
#define ID3DXEffectCompiler_GetLiteral(p,a,b) ICOM_CALL2(GetLiteral,p,a,b)
#define ID3DXEffectCompiler_CompileEffect(p,a,b,c) ICOM_CALL3(CompileEffect,p,a,b,c)
#define ID3DXEffectCompiler_CompileShader(p,a,b,c,d,e,f) ICOM_CALL6(CompileShader,p,a,b,c,d,e,f)


/*****************************************************************************
 * ID3DXEffectStateManager interface
 */
#define ICOM_INTERFACE ID3DXEffectStateManager
#define ID3DXEffectStateManager_METHODS \
    ICOM_METHOD2 (HRESULT,SetTransform,D3DTRANSFORMSTATETYPE,State,CONST D3DMATRIX*,pMatrix) \
    ICOM_METHOD1 (HRESULT,SetMaterial,CONST D3DMATERIAL9*,pMaterial) \
    ICOM_METHOD2 (HRESULT,SetLight,DWORD,Index,CONST D3DLIGHT9*,pLight) \
    ICOM_METHOD2 (HRESULT,LightEnable,DWORD,Index,BOOL,Enable) \
    ICOM_METHOD2 (HRESULT,SetRenderState,D3DRENDERSTATETYPE,State,DWORD,Value) \
    ICOM_METHOD2 (HRESULT,SetTexture,DWORD,Stage,LPDIRECT3DBASETEXTURE9,pTexture) \
    ICOM_METHOD3 (HRESULT,SetTextureStageState,DWORD,Stage,D3DTEXTURESTAGESTATETYPE,Type,DWORD,Value) \
    ICOM_METHOD3 (HRESULT,SetSamplerState,DWORD,Sampler,D3DSAMPLERSTATETYPE,Type,DWORD,Value) \
    ICOM_METHOD1 (HRESULT,SetNPatchMode,FLOAT,NumSegments) \
    ICOM_METHOD1 (HRESULT,SetFVF,DWORD,FVF) \
    ICOM_METHOD1 (HRESULT,SetVertexShader,LPDIRECT3DVERTEXSHADER9,pShader) \
    ICOM_METHOD3 (HRESULT,SetVertexShaderConstantF,UINT,RegisterIndex,CONST FLOAT*,pConstantData,UINT,RegisterCount) \
    ICOM_METHOD3 (HRESULT,SetVertexShaderConstantI,UINT,RegisterIndex,CONST INT*,pConstantData,UINT,RegisterCount) \
    ICOM_METHOD3 (HRESULT,SetVertexShaderConstantB,UINT,RegisterIndex,CONST BOOL*,pConstantData,UINT,RegisterCount) \
    ICOM_METHOD1 (HRESULT,SetPixelShader,LPDIRECT3DPIXELSHADER9,pShader) \
    ICOM_METHOD3 (HRESULT,SetPixelShaderConstantF,UINT,RegisterIndex,CONST FLOAT*,pConstantData,UINT,RegisterCount) \
    ICOM_METHOD3 (HRESULT,SetPixelShaderConstantI,UINT,RegisterIndex,CONST INT*,pConstantData,UINT,RegisterCount) \
    ICOM_METHOD3 (HRESULT,SetPixelShaderConstantB,UINT,RegisterIndex,CONST BOOL*,pConstantData,UINT,RegisterCount)
#define ID3DXEffectStateManager_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXEffectStateManager_METHODS
ICOM_DEFINE(ID3DXEffectStateManager,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXEffectStateManager_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXEffectStateManager_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXEffectStateManager_Release(p) ICOM_CALL(Release,p)
/*** ID3DXEffectStateManager methods ***/
#define ID3DXEffectStateManager_SetTransform(p,a,b) ICOM_CALL2(SetTransform,p,a,b)
#define ID3DXEffectStateManager_SetMaterial(p,a) ICOM_CALL1(SetMaterial,p,a)
#define ID3DXEffectStateManager_SetLight(p,a,b) ICOM_CALL2(SetLight,p,a,b)
#define ID3DXEffectStateManager_LightEnable(p,a,b) ICOM_CALL2(LightEnable,p,a,b)
#define ID3DXEffectStateManager_SetRenderState(p,a,b) ICOM_CALL2(SetRenderState,p,a,b)
#define ID3DXEffectStateManager_SetTexture(p,a,b) ICOM_CALL2(SetTexture,p,a,b)
#define ID3DXEffectStateManager_SetTextureStageState(p,a,b,c) ICOM_CALL3(SetTextureStageState,p,a,b,c)
#define ID3DXEffectStateManager_SetSamplerState(p,a,b,c) ICOM_CALL3(SetSamplerState,p,a,b,c)
#define ID3DXEffectStateManager_SetNPatchMode(p,a) ICOM_CALL1(SetNPatchMode(p,a)
#define ID3DXEffectStateManager_SetFVF(p,a) ICOM_CALL1(SetFVF,p,a)
#define ID3DXEffectStateManager_SetVertexShader(p,a) ICOM_CALL1(SetVertexShader,p,a)
#define ID3DXEffectStateManager_SetVertexShaderConstantF(p,a,b,c) ICOM_CALL3(SetVertexShaderConstantF,p,a,b,c)
#define ID3DXEffectStateManager_SetVertexShaderConstantI(p,a,b,c) ICOM_CALL3(SetVertexShaderConstantI,p,a,b,c)
#define ID3DXEffectStateManager_SetVertexShaderConstantB(p,a,b,c) ICOM_CALL3(SetVertexShaderConstantB,p,a,b,c)
#define ID3DXEffectStateManager_SetPixelShader(p,a) ICOM_CALL1(SetPixelShader,p,a)
#define ID3DXEffectStateManager_SetPixelShaderConstantF(p,a,b,c) ICOM_CALL3(SetVertexShaderConstantF,p,a,b,c)
#define ID3DXEffectStateManager_SetPixelShaderConstantI(p,a,b,c) ICOM_CALL3(SetVertexShaderConstantI,p,a,b,c)
#define ID3DXEffectStateManager_SetPixelShaderConstantB(p,a,b,c) ICOM_CALL3(SetVertexShaderConstantB,p,a,b,c)


/***********************************************************************
 * D3DX Effects functions
 */

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXCreateEffectPool(
        LPD3DXEFFECTPOOL*               ppPool);

HRESULT WINAPI
    D3DXCreateEffectFromFileA(
        LPDIRECT3DDEVICE9               pDevice,
        LPCSTR                          pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTPOOL                pPool,
        LPD3DXEFFECT*                   ppEffect,
        LPD3DXBUFFER*                   ppCompilationErrors);

HRESULT WINAPI
    D3DXCreateEffectFromFileW(
        LPDIRECT3DDEVICE9               pDevice,
        LPCWSTR                         pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTPOOL                pPool,
        LPD3DXEFFECT*                   ppEffect,
        LPD3DXBUFFER*                   ppCompilationErrors);

#ifdef UNICODE
#define D3DXCreateEffectFromFile D3DXCreateEffectFromFileW
#else
#define D3DXCreateEffectFromFile D3DXCreateEffectFromFileA
#endif

HRESULT WINAPI
    D3DXCreateEffectFromResourceA(
        LPDIRECT3DDEVICE9               pDevice,
        HMODULE                         hSrcModule,
        LPCSTR                          pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTPOOL                pPool,
        LPD3DXEFFECT*                   ppEffect,
        LPD3DXBUFFER*                   ppCompilationErrors);

HRESULT WINAPI
    D3DXCreateEffectFromResourceW(
        LPDIRECT3DDEVICE9               pDevice,
        HMODULE                         hSrcModule,
        LPCWSTR                         pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTPOOL                pPool,
        LPD3DXEFFECT*                   ppEffect,
        LPD3DXBUFFER*                   ppCompilationErrors);

#ifdef UNICODE
#define D3DXCreateEffectFromResource D3DXCreateEffectFromResourceW
#else
#define D3DXCreateEffectFromResource D3DXCreateEffectFromResourceA
#endif

HRESULT WINAPI
    D3DXCreateEffect(
        LPDIRECT3DDEVICE9               pDevice,
        LPCVOID                         pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTPOOL                pPool,
        LPD3DXEFFECT*                   ppEffect,
        LPD3DXBUFFER*                   ppCompilationErrors);

HRESULT WINAPI
    D3DXCreateEffectCompilerFromFileA(
        LPCSTR                          pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTCOMPILER*           ppCompiler,
        LPD3DXBUFFER*                   ppParseErrors);

HRESULT WINAPI
    D3DXCreateEffectCompilerFromFileW(
        LPCWSTR                         pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTCOMPILER*           ppCompiler,
        LPD3DXBUFFER*                   ppParseErrors);

#ifdef UNICODE
#define D3DXCreateEffectCompilerFromFile D3DXCreateEffectCompilerFromFileW
#else
#define D3DXCreateEffectCompilerFromFile D3DXCreateEffectCompilerFromFileA
#endif

HRESULT WINAPI
    D3DXCreateEffectCompilerFromResourceA(
        HMODULE                         hSrcModule,
        LPCSTR                          pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTCOMPILER*           ppCompiler,
        LPD3DXBUFFER*                   ppParseErrors);

HRESULT WINAPI
    D3DXCreateEffectCompilerFromResourceW(
        HMODULE                         hSrcModule,
        LPCWSTR                         pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTCOMPILER*           ppCompiler,
        LPD3DXBUFFER*                   ppParseErrors);

#ifdef UNICODE
#define D3DXCreateEffectCompilerFromResource D3DXCreateEffectCompilerFromResourceW
#else
#define D3DXCreateEffectCompilerFromResource D3DXCreateEffectCompilerFromResourceA
#endif

HRESULT WINAPI
    D3DXCreateEffectCompiler(
        LPCSTR                          pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXEFFECTCOMPILER*           ppCompiler,
        LPD3DXBUFFER*                   ppParseErrors);

HRESULT WINAPI
    D3DXDisassembleEffect(
	LPD3DXEFFECT pEffect,
	BOOL EnableColorCode,
	LPD3DXBUFFER * ppDisassembly);

#ifdef __cplusplus
}
#endif

#endif /* WINEX_D3DXEFFECT_H */



