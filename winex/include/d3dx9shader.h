#include "d3dx9.h"

#ifndef __WINEX_D3DX9SHADER_H
#define __WINEX_D3DX9SHADER_H


/*****************************************************************************
 * Predeclare Interfaces
 */

DEFINE_GUID(IID_ID3DXConstantTable,         0x9dca3190,0x38b9,0x4fc3,0x92,0xe3,0x39,0xc6,0xdd,0xfb,0x35,0x8b);
DEFINE_GUID(IID_ID3DXFragmentLinker,        0xd59d3777,0xc973,0x4a3c,0xb4,0xb0,0x2a,0x62,0xcd,0x3d,0x8b,0x40);

typedef struct ID3DXInclude             ID3DXInclude, *LPD3DXINCLUDE;
typedef struct ID3DXFragmentLinker      ID3DXFragmentLinker, *LPD3DXFRAGMENTLINKER;
typedef struct ID3DXConstantTable       ID3DXConstantTable, *LPD3DXCONSTANTTABLE;


/*************************************************************************
 * Defines
 */

#define D3DXTX_VERSION(_Major,_Minor) (('T' << 24) | ('X' << 16) | ((_Major) << 8) | (_Minor))

#define D3DXSHADER_DEBUG                    (1 << 0)
#define D3DXSHADER_SKIPVALIDATION           (1 << 1)
#define D3DXSHADER_SKIPOPTIMIZATION         (1 << 2)
#define D3DXSHADER_PACKMATRIX_ROWMAJOR      (1 << 3)
#define D3DXSHADER_PACKMATRIX_COLUMNMAJOR   (1 << 4)
#define D3DXSHADER_PARTIALPRECISION         (1 << 5)
#define D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT  (1 << 6)
#define D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT  (1 << 7)
#define D3DXSHADER_NO_PRESHADER             (1 << 8)
#define D3DXSHADER_AVOID_FLOW_CONTROL       (1 << 9)
#define D3DXSHADER_PREFER_FLOW_CONTROL      (1 << 10)


/************************************************************************
 * Types, structures, and enums
 */

typedef LPCSTR D3DXHANDLE;
typedef D3DXHANDLE *LPD3DXHANDLE;

typedef struct _D3DXMACRO
{
    LPCSTR Name;
    LPCSTR Definition;

} D3DXMACRO, *LPD3DXMACRO;

typedef struct _D3DXSEMANTIC
{
    UINT Usage;
    UINT UsageIndex;

} D3DXSEMANTIC, *LPD3DXSEMANTIC;

typedef struct _D3DXFRAGMENT_DESC
{
    LPCSTR Name;
    DWORD Target;

} D3DXFRAGMENT_DESC, *LPD3DXFRAGMENT_DESC;

typedef enum _D3DXREGISTER_SET
{
    D3DXRS_BOOL,
    D3DXRS_INT4,
    D3DXRS_FLOAT4,
    D3DXRS_SAMPLER,

    D3DXRS_FORCE_DWORD = 0x7fffffff

} D3DXREGISTER_SET, *LPD3DXREGISTER_SET;

typedef enum _D3DXPARAMETER_CLASS
{
    D3DXPC_SCALAR,
    D3DXPC_VECTOR,
    D3DXPC_MATRIX_ROWS,
    D3DXPC_MATRIX_COLUMNS,
    D3DXPC_OBJECT,
    D3DXPC_STRUCT,

    D3DXPC_FORCE_DWORD = 0x7fffffff

} D3DXPARAMETER_CLASS, *LPD3DXPARAMETER_CLASS;

typedef enum _D3DXPARAMETER_TYPE
{
    D3DXPT_VOID,
    D3DXPT_BOOL,
    D3DXPT_INT,
    D3DXPT_FLOAT,
    D3DXPT_STRING,
    D3DXPT_TEXTURE,
    D3DXPT_TEXTURE1D,
    D3DXPT_TEXTURE2D,
    D3DXPT_TEXTURE3D,
    D3DXPT_TEXTURECUBE,
    D3DXPT_SAMPLER,
    D3DXPT_SAMPLER1D,
    D3DXPT_SAMPLER2D,
    D3DXPT_SAMPLER3D,
    D3DXPT_SAMPLERCUBE,
    D3DXPT_PIXELSHADER,
    D3DXPT_VERTEXSHADER,
    D3DXPT_PIXELFRAGMENT,
    D3DXPT_VERTEXFRAGMENT,

    D3DXPT_FORCE_DWORD = 0x7fffffff

} D3DXPARAMETER_TYPE, *LPD3DXPARAMETER_TYPE;

typedef struct _D3DXCONSTANTTABLE_DESC
{
    LPCSTR Creator;
    DWORD Version;
    UINT Constants;

} D3DXCONSTANTTABLE_DESC, *LPD3DXCONSTANTTABLE_DESC;

typedef struct _D3DXCONSTANT_DESC
{
    LPCSTR Name;

    D3DXREGISTER_SET RegisterSet;
    UINT RegisterIndex;
    UINT RegisterCount;

    D3DXPARAMETER_CLASS Class;
    D3DXPARAMETER_TYPE Type;

    UINT Rows;
    UINT Columns;
    UINT Elements;
    UINT StructMembers;

    UINT Bytes;
    LPCVOID DefaultValue;

} D3DXCONSTANT_DESC, *LPD3DXCONSTANT_DESC;

typedef enum _D3DXINCLUDE_TYPE
{
    D3DXINC_LOCAL,
    D3DXINC_SYSTEM,

    D3DXINC_FORCE_DWORD = 0x7fffffff

} D3DXINCLUDE_TYPE, *LPD3DXINCLUDE_TYPE;

typedef struct _D3DXSHADER_CONSTANTTABLE
{
    DWORD Size;
    DWORD Creator;
    DWORD Version;
    DWORD Constants;
    DWORD ConstantInfo;

} D3DXSHADER_CONSTANTTABLE, *LPD3DXSHADER_CONSTANTTABLE;

typedef struct _D3DXSHADER_CONSTANTINFO
{
    DWORD Name;
    WORD  RegisterSet;
    WORD  RegisterIndex;
    WORD  RegisterCount;
    WORD  Reserved;
    DWORD TypeInfo;
    DWORD DefaultValue;

} D3DXSHADER_CONSTANTINFO, *LPD3DXSHADER_CONSTANTINFO;

typedef struct _D3DXSHADER_TYPEINFO
{
    WORD  Class;
    WORD  Type;
    WORD  Rows;
    WORD  Columns;
    WORD  Elements;
    WORD  StructMembers;
    DWORD StructMemberInfo;

} D3DXSHADER_TYPEINFO, *LPD3DXSHADER_TYPEINFO;

typedef struct _D3DXSHADER_STRUCTMEMBERINFO
{
    DWORD Name;
    DWORD TypeInfo;

} D3DXSHADER_STRUCTMEMBERINFO, *LPD3DXSHADER_STRUCTMEMBERINFO;


/***********************************************************************
 * Interfaces
 */


/***********************************************************************
 * ID3DXConstantTable interface
 */
#define ICOM_INTERFACE ID3DXConstantTable
#define ID3DXConstantTable_METHODS \
    ICOM_METHOD (LPVOID,GetBufferPointer) \
    ICOM_METHOD (DWORD,GetBufferSize) \
    ICOM_METHOD1(HRESULT,GetDesc,                        D3DXCONSTANTTABLE_DESC*,pDesc) \
    ICOM_METHOD3(HRESULT,GetConstantDesc,                D3DXHANDLE,hConstant,D3DXCONSTANT_DESC*,pDesc,UINT*,pCount) \
    ICOM_METHOD2(D3DXHANDLE,GetConstant,                 D3DXHANDLE,hConstant,UINT,Index) \
    ICOM_METHOD2(D3DXHANDLE,GetConstantByName,           D3DXHANDLE,hConstant,LPCSTR,pName) \
    ICOM_METHOD2(D3DXHANDLE,GetConstantElement,          D3DXHANDLE,hConstant,UINT,Index) \
    ICOM_METHOD1(HRESULT,SetDefaults,                    LPDIRECT3DDEVICE9,pDevice) \
    ICOM_METHOD4(HRESULT,SetValue,                       LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,LPCVOID,pData,UINT,Bytes) \
    ICOM_METHOD3(HRESULT,SetBool,                        LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,BOOL,b) \
    ICOM_METHOD4(HRESULT,SetBoolArray,                   LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST BOOL*,pB,UINT,Count) \
    ICOM_METHOD3(HRESULT,SetInt,                         LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,INT,n) \
    ICOM_METHOD4(HRESULT,SetIntArray,                    LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST INT*,pN,UINT,Count) \
    ICOM_METHOD3(HRESULT,SetFloat,                       LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,FLOAT,f) \
    ICOM_METHOD4(HRESULT,SetFloatArray,                  LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST FLOAT*,pF,UINT,Count) \
    ICOM_METHOD3(HRESULT,SetVector,                      LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXVECTOR4*,pVector) \
    ICOM_METHOD4(HRESULT,SetVectorArray,                 LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXVECTOR4*,pVector,UINT,Count) \
    ICOM_METHOD3(HRESULT,SetMatrix,                      LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXMATRIX*,pMatrix) \
    ICOM_METHOD4(HRESULT,SetMatrixArray,                 LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXMATRIX*,pMatrix,UINT,Count) \
    ICOM_METHOD4(HRESULT,SetMatrixPointerArray,          LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXMATRIX**,ppMatrix,UINT,Count) \
    ICOM_METHOD3(HRESULT,SetMatrixTranspose,             LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXMATRIX*,pMatrix) \
    ICOM_METHOD4(HRESULT,SetMatrixTransposeArray,        LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXMATRIX*,pMatrix,UINT,Count) \
    ICOM_METHOD4(HRESULT,SetMatrixTransposePointerArray, LPDIRECT3DDEVICE9,pDevice,D3DXHANDLE,hConstant,CONST D3DXMATRIX**,ppMatrix,UINT,Count) \

#define ID3DXConstantTable_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXConstantTable_METHODS
ICOM_DEFINE(ID3DXConstantTable,IUnknown)
#undef ICOM_INTERFACE

    /* IUnknown Methods */
#define ID3DXConstantTable_QueryInterface(p,a,b)                        ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXConstantTable_AddRef(p)                                    ICOM_CALL (AddRef,p)
#define ID3DXConstantTable_Release(p)                                   ICOM_CALL (Release,p)
    /* ID3DXBuffer */
#define ID3DXConstantTable_GetBufferPointer(p)                          ICOM_CALL (GetBufferPointer,p)
#define ID3DXConstantTable_GetBufferSize(p)                             ICOM_CALL (GetBufferSize,p)
    /* Descriptions */
#define ID3DXConstantTable_GetDesc(p,a)                                 ICOM_CALL1(GetDesc,p,a)
#define ID3DXConstantTable_GetConstantDesc(p,a,b,c)                     ICOM_CALL3(GetConstantDesc,p,a,b,c)
    /* Handle ops */
#define ID3DXConstantTable_GetConstant(p,a,b)                           ICOM_CALL2(GetConstant,p,a,b)
#define ID3DXConstantTable_GetConstantByName(p,a,b)                     ICOM_CALL2(GetConstantByName,p,a,b)
#define ID3DXConstantTable_GetConstantElement(p,a,b)                    ICOM_CALL2(GetConstantElement,p,a,b)
    /* Set constants */
#define ID3DXConstantTable_SetDefaults(p,a)                             ICOM_CALL1(SetDefaults,p,a)
#define ID3DXConstantTable_SetValue(p,a,b,c,d)                          ICOM_CALL4(SetValue,p,a,b,c,d)
#define ID3DXConstantTable_SetBool(p,a,b,c)                             ICOM_CALL3(SetBool,p,a,b,c)
#define ID3DXConstantTable_SetBoolArray(p,a,b,c,d)                      ICOM_CALL4(SetBoolArray,p,a,b,c,d)
#define ID3DXConstantTable_SetInt(p,a,b,c)                              ICOM_CALL3(SetInt,p,a,b,c)
#define ID3DXConstantTable_SetIntArray(p,a,b,c,d)                       ICOM_CALL4(SetIntArray,p,a,b,c,d)
#define ID3DXConstantTable_SetFloat(p,a,b,c)                            ICOM_CALL3(SetFloat,p,a,b,c)
#define ID3DXConstantTable_SetFloatArray(p,a,b,c,d)                     ICOM_CALL4(SetFloatArray,p,a,b,c,d)
#define ID3DXConstantTable_SetVector(p,a,b,c)                           ICOM_CALL3(SetVector,p,a,b,c)
#define ID3DXConstantTable_SetVectorArray(p,a,b,c,d)                    ICOM_CALL4(SetVectorArray,p,a,b,c,d)
#define ID3DXConstantTable_SetMatrix(p,a,b,c)                           ICOM_CALL3(SetMatrix,p,a,b,c)
#define ID3DXConstantTable_SetMatrixArray(p,a,b,c,d)                    ICOM_CALL4(SetMatrixArray,p,a,b,c,d)
#define ID3DXConstantTable_SetMatrixPointerArray(p,a,b,c,d)             ICOM_CALL4(SetMatrixPointerArray,p,a,b,c,d)
#define ID3DXConstantTable_SetMatrixTranspose(p,a,b,c)                  ICOM_CALL3(SetMatrixTranspose,p,a,b,c)
#define ID3DXConstantTable_SetMatrixTransposeArray(p,a,b,c,d)           ICOM_CALL4(SetMatrixTransposeArray,p,a,b,c,d)
#define ID3DXConstantTable_SetMatrixTransposePointerArray(p,a,b,c,d)    ICOM_CALL4(SetMatrixTransposePointerArray,p,a,b,c,d)


/*****************************************************************************
 * ID3DXFragmentLinker interface
 */
#define ICOM_INTERFACE ID3DXFragmentLinker
#define ID3DXFragmentLinker_METHODS \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD  (UINT,GetNumberOfFragments) \
    ICOM_METHOD1 (D3DXHANDLE,GetFragmentHandleByIndex,UINT,Index) \
    ICOM_METHOD1 (D3DXHANDLE,GetFragmentHandleByName,LPCSTR,Name) \
    ICOM_METHOD2 (HRESULT,GetFragmentDesc,D3DXHANDLE,Name,LPD3DXFRAGMENT_DESC,FragDesc) \
    ICOM_METHOD1 (HRESULT,AddFragments,CONST DWORD*,Fragments) \
    ICOM_METHOD1 (HRESULT,GetAllFragments,LPD3DXBUFFER*,ppBuffer) \
    ICOM_METHOD2 (HRESULT,GetFragment,D3DXHANDLE,Name,LPD3DXBUFFER*,ppBuffer) \
    ICOM_METHOD6 (HRESULT,LinkShader,LPCSTR,pTarget,DWORD,Flags,LPD3DXHANDLE,rgFragmentHandles,UINT,cFragments,LPD3DXBUFFER*,ppBuffer,LPD3DXBUFFER*,ppErrorMsgs) \
    ICOM_METHOD6 (HRESULT,LinkVertexShader,LPCSTR,pTarget,DWORD,Flags,LPD3DXHANDLE,rgFragmentHandles,UINT,cFragments,LPDIRECT3DVERTEXSHADER9*,pVShader,LPD3DXBUFFER*,ppErrorMsgs) \
    ICOM_METHOD  (HRESULT,ClearCache)
#define ID3DXFragmentLinker_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXFragmentLinker_METHODS
ICOM_DEFINE(ID3DXFragmentLinker,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXFragmentLinker_QueryInterface(p,a,b)                   ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXFragmentLinker_AddRef(p)                               ICOM_CALL(AddRef,p)
#define ID3DXFragmentLinker_Release(p)                              ICOM_CALL(Release,p)
/*** ID3DXFragmentLinker methods ***/
#define ID3DXFragmentLinker_GetDevice(p,a)                          ICOM_CALL1(GetDevice,p,a)
#define ID3DXFragmentLinker_GetNumberOfFragments(p)                 ICOM_CALL(GetNumberOfFragments,p)
#define ID3DXFragmentLinker_GetFragmentHandleByIndex(p,a)           ICOM_CALL1(GetFragmentHandleByIndex,p,a)
#define ID3DXFragmentLinker_GetFragmentHandleByName(p,a)            ICOM_CALL1(GetFragmentHandleByName,p,a)
#define ID3DXFragmentLinker_GetFragmentDesc(p,a,b)                  ICOM_CALL2(GetFragmentDesc,p,a,b)
#define ID3DXFragmentLinker_AddFragments(p,a)                       ICOM_CALL1(AddFragments,p,a)
#define ID3DXFragmentLinker_GetAllFragments(p,a)                    ICOM_CALL1(GetAllFragments,p,a)
#define ID3DXFragmentLinker_GetFragment(p,a,b)                      ICOM_CALL2(GetFragment,p,a,b)
#define ID3DXFragmentLinker_LinkShader(p,a,b,c,d,e,f)               ICOM_CALL6(LinkShader,p,a,b,c,d,e,f)
#define ID3DXFragmentLinker_LinkVertexShader(p,a,b,c,d,e,f)         ICOM_CALL6(LinkVertexShader,p,a,b,c,d,e,f)
#define ID3DXFragmentLinker_ClearCache(p)                           ICOM_CALL(ClearCache,p)


/*****************************************************************************
 * ID3DXInclude interface
 */
#define ICOM_INTERFACE ID3DXInclude
#define ID3DXInclude_METHODS \
    ICOM_METHOD5 (HRESULT,Open,D3DXINCLUDE_TYPE,IncludeType,LPCSTR,pFileName,LPCVOID,pParentData,LPCVOID*,ppData,UINT*,pBytes) \
    ICOM_METHOD1 (HRESULT,Close,LPCVOID,pData)
#define ID3DXInclude_IMETHODS \
    ID3DXInclude_METHODS
ICOM_DEFINE1(ID3DXInclude)
#undef ICOM_INTERFACE

/*** ID3DXInclude methods ***/
#define ID3DXInclude_Open(p,a,b,c,d,e)      ICOM_CALL5(Open,p,a,b,c,d,e)
#define ID3DXInclude_Close(p,a)             ICOM_CALL1(Close,p,a)


/***********************************************************************
 * D3DX Shader functions
 */

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI
    D3DXAssembleShaderFromFileA(
        LPCSTR                          pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

HRESULT WINAPI
    D3DXAssembleShaderFromFileW(
        LPCWSTR                         pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

#ifdef UNICODE
#define D3DXAssembleShaderFromFile D3DXAssembleShaderFromFileW
#else
#define D3DXAssembleShaderFromFile D3DXAssembleShaderFromFileA
#endif

HRESULT WINAPI
    D3DXAssembleShaderFromResourceA(
        HMODULE                         hSrcModule,
        LPCSTR                          pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

HRESULT WINAPI
    D3DXAssembleShaderFromResourceW(
        HMODULE                         hSrcModule,
        LPCWSTR                         pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

#ifdef UNICODE
#define D3DXAssembleShaderFromResource D3DXAssembleShaderFromResourceW
#else
#define D3DXAssembleShaderFromResource D3DXAssembleShaderFromResourceA
#endif

HRESULT WINAPI
    D3DXAssembleShader(
        LPCSTR                          pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

HRESULT WINAPI
    D3DXCompileShaderFromFileA(
        LPCSTR                          pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pTarget,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable);

HRESULT WINAPI
    D3DXCompileShaderFromFileW(
        LPCWSTR                         pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pTarget,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable);

#ifdef UNICODE
#define D3DXCompileShaderFromFile D3DXCompileShaderFromFileW
#else
#define D3DXCompileShaderFromFile D3DXCompileShaderFromFileA
#endif

HRESULT WINAPI
    D3DXCompileShaderFromResourceA(
        HMODULE                         hSrcModule,
        LPCSTR                          pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pTarget,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable);

HRESULT WINAPI
    D3DXCompileShaderFromResourceW(
        HMODULE                         hSrcModule,
        LPCWSTR                         pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pTarget,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable);

#ifdef UNICODE
#define D3DXCompileShaderFromResource D3DXCompileShaderFromResourceW
#else
#define D3DXCompileShaderFromResource D3DXCompileShaderFromResourceA
#endif


HRESULT WINAPI
    D3DXCompileShader(
        LPCSTR                          pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pTarget,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable);

HRESULT WINAPI
    D3DXFindShaderComment(
        CONST DWORD*                    pFunction,
        DWORD                           FourCC,
        LPCVOID*                        ppData,
        UINT*                           pSizeInBytes);

LPCSTR WINAPI
    D3DXGetPixelShaderProfile(
        LPDIRECT3DDEVICE9               pDevice);

LPCSTR WINAPI
    D3DXGetVertexShaderProfile(
        LPDIRECT3DDEVICE9               pDevice);

HRESULT WINAPI
    D3DXGetShaderInputSemantics(
        CONST DWORD*                    pFunction,
        D3DXSEMANTIC*                   pSemantics,
        UINT*                           pCount);

HRESULT WINAPI
    D3DXGetShaderOutputSemantics(
        CONST DWORD*                    pFunction,
        D3DXSEMANTIC*                   pSemantics,
        UINT*                           pCount);

HRESULT WINAPI
    D3DXGetShaderSamplers(
        CONST DWORD*                    pFunction,
        LPCSTR*                         pSamplers,
        UINT*                           pCount);

HRESULT WINAPI
    D3DXGetShaderConstantTable(
        CONST DWORD*                    pFunction,
        LPD3DXCONSTANTTABLE*            ppConstantTable);

HRESULT WINAPI
    D3DXGetShaderDebugInfo(
        CONST DWORD*                    pFunction,
        LPD3DXBUFFER*                   ppDebugInfo);

HRESULT WINAPI
D3DXGatherFragmentsFromFileA(
        LPCSTR                          pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

HRESULT WINAPI
D3DXGatherFragmentsFromFileW(
        LPCWSTR                         pSrcFile,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

#ifdef UNICODE
#define D3DXGatherFragmentsFromFile D3DXGatherFragmentsFromFileW
#else
#define D3DXGatherFragmentsFromFile D3DXGatherFragmentsFromFileA
#endif

HRESULT WINAPI
    D3DXGatherFragmentsFromResourceA(
        HMODULE                         hSrcModule,
        LPCSTR                          pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

HRESULT WINAPI
    D3DXGatherFragmentsFromResourceW(
        HMODULE                         hSrcModule,
        LPCWSTR                         pSrcResource,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

#ifdef UNICODE
#define D3DXGatherFragmentsFromResource D3DXGatherFragmentsFromResourceW
#else
#define D3DXGatherFragmentsFromResource D3DXGatherFragmentsFromResourceA
#endif

HRESULT WINAPI
    D3DXGatherFragments(
        LPCSTR                          pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs);

HRESULT WINAPI
    D3DXCreateFragmentLinker(
        LPDIRECT3DDEVICE9               pDevice,
        UINT                            ShaderCacheSize,
        LPD3DXFRAGMENTLINKER*           ppFragmentLinker);

#ifdef __cplusplus
}
#endif

#endif /* __WINEX_D3DX9SHADER_H */

