#include "d3dx9.h"

#ifndef __WINEX_D3DX9MESH_H
#define __WINEX_D3DX9MESH_H

#include "dxfile.h"


/*****************************************************************************
 * Predeclare Interfaces
 */
DEFINE_GUID(IID_ID3DXBaseMesh,  0x2a835771,0xbf4d,0x43f4,0x8e,0x14,0x82,0xa8,0x9, 0xf1,0x7d,0x8a);
DEFINE_GUID(IID_ID3DXMesh,      0xccae5c3b,0x4dd1,0x4d0f,0x99,0x7e,0x46,0x84,0xca,0x64,0x55,0x7f);
DEFINE_GUID(IID_ID3DXPMesh,     0x19fbe386,0xc282,0x4659,0x97,0xbd,0xcb,0x86,0x9b,0x8, 0x4a,0x6c);
DEFINE_GUID(IID_ID3DXSPMesh,    0x4e3ca05c,0xd4ff,0x4d11,0x8a,0x2, 0x16,0x45,0x9e,0x8, 0xf6,0xf4);
DEFINE_GUID(IID_ID3DXSkinInfo,  0xe7dbbf3, 0x421a,0x4dd8,0xb7,0x38,0xa5,0xda,0xc3,0xa4,0x87,0x67);
DEFINE_GUID(IID_ID3DXPatchMesh, 0xad3e8bc, 0x290d,0x4dc7,0x91,0xab,0x73,0xa8,0x27,0x55,0xb1,0x3e);

typedef struct ID3DXBaseMesh    ID3DXBaseMesh, *LPD3DXBASEMESH;
typedef struct ID3DXMesh        ID3DXMesh, *LPD3DXMESH;
typedef struct ID3DXPMesh       ID3DXPMesh, *LPD3DXPMESH;
typedef struct ID3DXSPMesh      ID3DXSPMesh, *LPD3DXSPMESH;
typedef struct ID3DXSkinInfo    ID3DXSkinInfo, *LPD3DXSKININFO;
typedef struct ID3DXPatchMesh   ID3DXPatchMesh, *LPD3DXPATCHMESH;


/*****************************************************************************
 * Types, enums, defines, structures
 */
typedef enum _D3DXPATCHMESHTYPE {
    D3DXPATCHMESH_RECT   = 0x001,
    D3DXPATCHMESH_TRI    = 0x002,
    D3DXPATCHMESH_NPATCH = 0x003,

    D3DXPATCHMESH_FORCE_DWORD    = 0x7fffffff,
} D3DXPATCHMESHTYPE;

enum _D3DXMESH {
    D3DXMESH_32BIT                  = 0x001,
    D3DXMESH_DONOTCLIP              = 0x002,
    D3DXMESH_POINTS                 = 0x004,
    D3DXMESH_RTPATCHES              = 0x008,
    D3DXMESH_NPATCHES               = 0x4000,
    D3DXMESH_VB_SYSTEMMEM           = 0x010,
    D3DXMESH_VB_MANAGED             = 0x020,
    D3DXMESH_VB_WRITEONLY           = 0x040,
    D3DXMESH_VB_DYNAMIC             = 0x080,
    D3DXMESH_VB_SOFTWAREPROCESSING = 0x8000,
    D3DXMESH_IB_SYSTEMMEM           = 0x100,
    D3DXMESH_IB_MANAGED             = 0x200,
    D3DXMESH_IB_WRITEONLY           = 0x400,
    D3DXMESH_IB_DYNAMIC             = 0x800,
    D3DXMESH_IB_SOFTWAREPROCESSING= 0x10000,

    D3DXMESH_VB_SHARE               = 0x1000,

    D3DXMESH_USEHWONLY              = 0x2000,

    D3DXMESH_SYSTEMMEM              = 0x110,
    D3DXMESH_MANAGED                = 0x220,
    D3DXMESH_WRITEONLY              = 0x440,
    D3DXMESH_DYNAMIC                = 0x880,
    D3DXMESH_SOFTWAREPROCESSING   = 0x18000,
};

enum _D3DXPATCHMESH {
    D3DXPATCHMESH_DEFAULT = 000,
};

enum _D3DXMESHSIMP
{
    D3DXMESHSIMP_VERTEX   = 0x1,
    D3DXMESHSIMP_FACE     = 0x2,
};

typedef enum _D3DXCLEANTYPE {
    D3DXCLEAN_BACKFACING    = 0x00000001,
    D3DXCLEAN_BOWTIES       = 0x00000002,

    D3DXCLEAN_SKINNING      = D3DXCLEAN_BACKFACING,
    D3DXCLEAN_OPTIMIZATION  = D3DXCLEAN_BACKFACING,
    D3DXCLEAN_SIMPLIFICATION= D3DXCLEAN_BACKFACING | D3DXCLEAN_BOWTIES,
} D3DXCLEANTYPE;

enum _MAX_FVF_DECL_SIZE
{
    MAX_FVF_DECL_SIZE = MAXD3DDECLLENGTH + 1
};

typedef enum _D3DXTANGENT
{
    D3DXTANGENT_WRAP_U =                    0x01,
    D3DXTANGENT_WRAP_V =                    0x02,
    D3DXTANGENT_WRAP_UV =                   0x03,
    D3DXTANGENT_DONT_NORMALIZE_PARTIALS =   0x04,
    D3DXTANGENT_DONT_ORTHOGONALIZE =        0x08,
    D3DXTANGENT_ORTHOGONALIZE_FROM_V =      0x010,
    D3DXTANGENT_ORTHOGONALIZE_FROM_U =      0x020,
    D3DXTANGENT_WEIGHT_BY_AREA =            0x040,
    D3DXTANGENT_WEIGHT_EQUAL =              0x080,
    D3DXTANGENT_WIND_CW =                   0x0100,
    D3DXTANGENT_CALCULATE_NORMALS =         0x0200,
    D3DXTANGENT_GENERATE_IN_PLACE =         0x0400,
} D3DXTANGENT;

typedef enum _D3DXIMT
{
    D3DXIMT_WRAP_U =                    0x01,
    D3DXIMT_WRAP_V =                    0x02,
    D3DXIMT_WRAP_UV =                   0x03,
} D3DXIMT;


typedef struct _D3DXATTRIBUTERANGE
{
    DWORD AttribId;
    DWORD FaceStart;
    DWORD FaceCount;
    DWORD VertexStart;
    DWORD VertexCount;
} D3DXATTRIBUTERANGE;

typedef D3DXATTRIBUTERANGE* LPD3DXATTRIBUTERANGE;

typedef struct _D3DXMATERIAL
{
    D3DMATERIAL9  MatD3D;
    LPSTR         pTextureFilename;
} D3DXMATERIAL;
typedef D3DXMATERIAL *LPD3DXMATERIAL;

typedef enum _D3DXEFFECTDEFAULTTYPE
{
    D3DXEDT_STRING = 0x1,
    D3DXEDT_FLOATS = 0x2,
    D3DXEDT_DWORD  = 0x3,

    D3DXEDT_FORCEDWORD = 0x7fffffff
} D3DXEFFECTDEFAULTTYPE;

typedef struct _D3DXEFFECTDEFAULT
{
    LPSTR                 pParamName;
    D3DXEFFECTDEFAULTTYPE Type;
    DWORD                 NumBytes;
    LPVOID                pValue;
} D3DXEFFECTDEFAULT, *LPD3DXEFFECTDEFAULT;

typedef struct _D3DXEFFECTINSTANCE
{
    LPSTR               pEffectFilename;
    DWORD               NumDefaults;
    LPD3DXEFFECTDEFAULT pDefaults;
} D3DXEFFECTINSTANCE, *LPD3DXEFFECTINSTANCE;

typedef struct _D3DXATTRIBUTEWEIGHTS
{
    FLOAT Position;
    FLOAT Boundary;
    FLOAT Normal;
    FLOAT Diffuse;
    FLOAT Specular;
    FLOAT Texcoord[8];
    FLOAT Tangent;
    FLOAT Binormal;
} D3DXATTRIBUTEWEIGHTS, *LPD3DXATTRIBUTEWEIGHTS;

enum _D3DXWELDEPSILONSFLAGS
{
    D3DXWELDEPSILONS_WELDALL             = 0x1,
    D3DXWELDEPSILONS_WELDPARTIALMATCHES  = 0x2,
    D3DXWELDEPSILONS_DONOTREMOVEVERTICES = 0x4,
    D3DXWELDEPSILONS_DONOTSPLIT          = 0x8,
};

typedef struct _D3DXWELDEPSILONS
{
    FLOAT Position;
    FLOAT BlendWeights;
    FLOAT Normal;
    FLOAT PSize;
    FLOAT Specular;
    FLOAT Diffuse;
    FLOAT Texcoord[8];
    FLOAT Tangent;
    FLOAT Binormal;
    FLOAT TessFactor;
} D3DXWELDEPSILONS;

typedef D3DXWELDEPSILONS* LPD3DXWELDEPSILONS;

enum _D3DXMESHOPT {
    D3DXMESHOPT_COMPACT       = 0x01000000,
    D3DXMESHOPT_ATTRSORT      = 0x02000000,
    D3DXMESHOPT_VERTEXCACHE   = 0x04000000,
    D3DXMESHOPT_STRIPREORDER  = 0x08000000,
    D3DXMESHOPT_IGNOREVERTS   = 0x10000000,
    D3DXMESHOPT_DONOTSPLIT    = 0x20000000,
    D3DXMESHOPT_DEVICEINDEPENDENT = 0x00400000,
};

typedef struct _D3DXBONECOMBINATION
{
    DWORD AttribId;
    DWORD FaceStart;
    DWORD FaceCount;
    DWORD VertexStart;
    DWORD VertexCount;
    DWORD* BoneId;
} D3DXBONECOMBINATION, *LPD3DXBONECOMBINATION;

typedef struct _D3DXPATCHINFO
{
    D3DXPATCHMESHTYPE PatchType;
    D3DDEGREETYPE Degree;
    D3DBASISTYPE Basis;
} D3DXPATCHINFO, *LPD3DXPATCHINFO;

typedef struct _D3DXINTERSECTINFO
{
    DWORD FaceIndex;
    FLOAT U;
    FLOAT V;
    FLOAT Dist;
} D3DXINTERSECTINFO, *LPD3DXINTERSECTINFO;

#ifdef __cplusplus
extern "C" {
#endif


/*****************************************************************************
 * Interfaces
 */


/*****************************************************************************
 * ID3DXBaseMesh interface
 */
#define ICOM_INTERFACE ID3DXBaseMesh
#define ID3DXBaseMesh_METHODS \
    ICOM_METHOD1 (HRESULT,DrawSubset,DWORD,AttribId) \
    ICOM_METHOD  (DWORD,GetNumFaces) \
    ICOM_METHOD  (DWORD,GetNumVertices) \
    ICOM_METHOD  (DWORD,GetFVF) \
    ICOM_METHOD1 (HRESULT,GetDeclaration,D3DVERTEXELEMENT9*,Declaration) \
    ICOM_METHOD  (DWORD,GetNumBytesPerVertex) \
    ICOM_METHOD  (DWORD,GetOptions) \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD4 (HRESULT,CloneMeshFVF,DWORD,Options,DWORD,FVF,LPDIRECT3DDEVICE9,pD3DDevice,LPD3DXMESH*,ppCloneMesh) \
    ICOM_METHOD4 (HRESULT,CloneMesh,DWORD,Options,D3DVERTEXELEMENT9*,pDeclaration,LPDIRECT3DDEVICE9,pD3DDevice,LPD3DXMESH*,ppCloneMesh) \
    ICOM_METHOD1 (HRESULT,GetVertexBuffer,LPDIRECT3DVERTEXBUFFER9*,ppVB) \
    ICOM_METHOD1 (HRESULT,GetIndexBuffer,LPDIRECT3DINDEXBUFFER9*,ppIB) \
    ICOM_METHOD2 (HRESULT,LockVertexBuffer,DWORD,Flags,void**,ppData) \
    ICOM_METHOD  (HRESULT,UnlockVertexBuffer) \
    ICOM_METHOD2 (HRESULT,LockIndexBuffer,DWORD,Flags,void**,ppData) \
    ICOM_METHOD  (HRESULT,UnlockIndexBuffer) \
    ICOM_METHOD2 (HRESULT,GetAttributeTable,D3DXATTRIBUTERANGE*,pAttribTable,DWORD*,pAttribTableSize) \
    ICOM_METHOD2 (HRESULT,ConvertPointRepsToAdjacency,DWORD*,pPRep,DWORD*,pAdjacency) \
    ICOM_METHOD2 (HRESULT,ConvertAdjacencyToPointReps,DWORD*,pAdjacency,DWORD*,pPRep) \
    ICOM_METHOD2 (HRESULT,GenerateAdjacency,FLOAT,Epsilon,DWORD*,pAdjacency) \
    ICOM_METHOD1 (HRESULT,UpdateSemantics,D3DVERTEXELEMENT9*,Declaration)
#define ID3DXBaseMesh_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXBaseMesh_METHODS
ICOM_DEFINE(ID3DXBaseMesh,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXBaseMesh_QueryInterface(p,a,b)                ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXBaseMesh_AddRef(p)                            ICOM_CALL(AddRef,p)
#define ID3DXBaseMesh_Release(p)                           ICOM_CALL(Release,p)
/*** ID3DXBaseMesh methods ***/
#define ID3DXBaseMesh_DrawSubset(p,a)                      ICOM_CALL1(DrawSubset,p,a)
#define ID3DXBaseMesh_GetNumFaces(p)                       ICOM_CALL(GetNumFaces,p)
#define ID3DXBaseMesh_GetNumVertices(p)                    ICOM_CALL(GetNumVertices,p)
#define ID3DXBaseMesh_GetFVF(p)                            ICOM_CALL(GetFVF,p)
#define ID3DXBaseMesh_GetDeclaration(p,a)                  ICOM_CALL1(GetDeclaration,p,a)
#define ID3DXBaseMesh_GetNumBytesPerVertex(p)              ICOM_CALL(GetNumBytesPerVertex,p)
#define ID3DXBaseMesh_GetOptions(p)                        ICOM_CALL(GetOptions,p)
#define ID3DXBaseMesh_GetDevice(p,a)                       ICOM_CALL1(GetDevice,p,a)
#define ID3DXBaseMesh_CloneMeshFVF(p,a,b,c,d)              ICOM_CALL4(CloneMeshFVF,p,a,b,c,d)
#define ID3DXBaseMesh_CloneMesh(p,a,b,c,d)                 ICOM_CALL4(CloneMesh,p,a,b,c,d)
#define ID3DXBaseMesh_GetVertexBuffer(p,a)                 ICOM_CALL1(GetVertexBuffer,p,a)
#define ID3DXBaseMesh_GetIndexBuffer(p,a)                  ICOM_CALL1(GetIndexBuffer,p,a)
#define ID3DXBaseMesh_LockVertexBuffer(p,a,b)              ICOM_CALL2(LockVertexBuffer,p,a,b)
#define ID3DXBaseMesh_UnlockVertexBuffer(p)                ICOM_CALL(UnlockVertexBuffer,p)
#define ID3DXBaseMesh_LockIndexBuffer(p,a,b)               ICOM_CALL2(LockIndexBuffer,p,a,b)
#define ID3DXBaseMesh_UnlockIndexBuffer(p)                 ICOM_CALL(UnlockIndexBuffer,p)
#define ID3DXBaseMesh_GetAttributeTable(p,a,b)             ICOM_CALL2(GetAttributeTable,p,a,b)
#define ID3DXBaseMesh_ConvertPointRepsToAdjacency(p,a,b)   ICOM_CALL2(ConvertPointRepsToAdjacency,p,a,b)
#define ID3DXBaseMesh_ConvertAdjacencyToPointReps(p,a,b)   ICOM_CALL2(ConvertAdjacencyToPointReps,p,a,b)
#define ID3DXBaseMesh_GenerateAdjacency(p,a,b)             ICOM_CALL2(GenerateAdjacency,p,a,b)
#define ID3DXBaseMesh_UpdateSemantics(p,a)                 ICOM_CALL1(UpdateSemantics,p,a)


/*****************************************************************************
 * ID3DXMesh interface
 */
#define ICOM_INTERFACE ID3DXMesh
#define ID3DXMesh_METHODS \
    ICOM_METHOD2 (HRESULT,LockAttributeBuffer,DWORD,Flags,DWORD**,ppData) \
    ICOM_METHOD  (HRESULT,UnlockAttributeBuffer) \
    ICOM_METHOD6 (HRESULT,Optimize,DWORD,Flags,CONST DWORD*,pAdjacencyIn,DWORD*,pAdjacencyOut,DWORD*,pFaceRemap,LPD3DXBUFFER*,ppVertexRemap,LPD3DXMESH*,ppOptMesh) \
    ICOM_METHOD5 (HRESULT,OptimizeInPlace,DWORD,Flags,CONST DWORD*,pAdjacencyIn,DWORD*,pAdjacencyOut,DWORD*,pFaceRemap,LPD3DXBUFFER*,ppVertexRemap) \
    ICOM_METHOD2 (HRESULT,SetAttributeTable,CONST D3DXATTRIBUTERANGE*,pAttribTable,DWORD,cAttribTableSize)
#define ID3DXMesh_IMETHODS \
    ID3DXBaseMesh_IMETHODS \
    ID3DXMesh_METHODS
ICOM_DEFINE(ID3DXMesh,ID3DXBaseMesh)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXMesh_QueryInterface(p,a,b)                ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXMesh_AddRef(p)                            ICOM_CALL(AddRef,p)
#define ID3DXMesh_Release(p)                           ICOM_CALL(Release,p)
/*** ID3DXBaseMesh methods ***/
#define ID3DXMesh_DrawSubset(p,a)                      ICOM_CALL1(DrawSubset,p,a)
#define ID3DXMesh_GetNumFaces(p)                       ICOM_CALL(GetNumFaces,p)
#define ID3DXMesh_GetNumVertices(p)                    ICOM_CALL(GetNumVertices,p)
#define ID3DXMesh_GetFVF(p)                            ICOM_CALL(GetFVF,p)
#define ID3DXMesh_GetDeclaration(p,a)                  ICOM_CALL1(GetDeclaration,p,a)
#define ID3DXMesh_GetNumBytesPerVertex(p)              ICOM_CALL(GetNumBytesPerVertex,p)
#define ID3DXMesh_GetOptions(p)                        ICOM_CALL(GetOptions,p)
#define ID3DXMesh_GetDevice(p,a)                       ICOM_CALL1(GetDevice,p,a)
#define ID3DXMesh_CloneMeshFVF(p,a,b,c,d)              ICOM_CALL4(CloneMeshFVF,p,a,b,c,d)
#define ID3DXMesh_CloneMesh(p,a,b,c,d)                 ICOM_CALL4(CloneMesh,p,a,b,c,d)
#define ID3DXMesh_GetVertexBuffer(p,a)                 ICOM_CALL1(GetVertexBuffer,p,a)
#define ID3DXMesh_GetIndexBuffer(p,a)                  ICOM_CALL1(GetIndexBuffer,p,a)
#define ID3DXMesh_LockVertexBuffer(p,a,b)              ICOM_CALL2(LockVertexBuffer,p,a,b)
#define ID3DXMesh_UnlockVertexBuffer(p)                ICOM_CALL(UnlockVertexBuffer,p)
#define ID3DXMesh_LockIndexBuffer(p,a,b)               ICOM_CALL2(LockIndexBuffer,p,a,b)
#define ID3DXMesh_UnlockIndexBuffer(p)                 ICOM_CALL(UnlockIndexBuffer,p)
#define ID3DXMesh_GetAttributeTable(p,a,b)             ICOM_CALL2(GetAttributeTable,p,a,b)
#define ID3DXMesh_ConvertPointRepsToAdjacency(p,a,b)   ICOM_CALL2(ConvertPointRepsToAdjacency,p,a,b)
#define ID3DXMesh_ConvertAdjacencyToPointReps(p,a,b)   ICOM_CALL2(ConvertAdjacencyToPointReps,p,a,b)
#define ID3DXMesh_GenerateAdjacency(p,a,b)             ICOM_CALL2(GenerateAdjacency,p,a,b)
#define ID3DXMesh_UpdateSemantics(p,a)                 ICOM_CALL1(UpdateSemantics,p,a)
/*** ID3DXMesh methods ***/
#define ID3DXMesh_LockAttributeBuffer(p,a,b)            ICOM_CALL2(LockAttributeBuffer,p,a,b)
#define ID3DXMesh_UnlockAttributeBuffer(p)              ICOM_CALL(UnlockAttributeBuffer,p)
#define ID3DXMesh_Optimize(p,a,b,c,d,e,f)               ICOM_CALL6(Optimize,p,a,b,c,d,e,f)
#define ID3DXMesh_OptimizeInPlace(p,a,b,c,d,e)          ICOM_CALL5(OptimizeInPlace,p,a,b,c,d,e)
#define ID3DXMesh_SetAttributeTable(p,a,b)              ICOM_CALL2(SetAttributeTable,p,a,b)


/*****************************************************************************
 * ID3DXPMesh interface
 */
#define ICOM_INTERFACE ID3DXPMesh
#define ID3DXPMesh_METHODS \
    ICOM_METHOD4 (HRESULT,ClonePMeshFVF,DWORD,Options,DWORD,FVF,LPDIRECT3DDEVICE9,pD3D,LPD3DXPMESH*,ppCloneMesh) \
    ICOM_METHOD4 (HRESULT,ClonePMesh,DWORD,Options,D3DVERTEXELEMENT9*,PDeclaration,LPDIRECT3DDEVICE9,pD3D,LPD3DXPMESH*,ppCloneMesh) \
    ICOM_METHOD1 (HRESULT,SetNumFaces,DWORD,Faces) \
    ICOM_METHOD1 (HRESULT,SetNumVertices,DWORD,Vertices) \
    ICOM_METHOD  (DWORD,GetMaxFaces) \
    ICOM_METHOD  (DWORD,GetMinFaces) \
    ICOM_METHOD  (DWORD,GetMaxVertices) \
    ICOM_METHOD  (DWORD,GetMinVertices) \
    ICOM_METHOD4 (HRESULT,Save,IStream*,pStream,CONST D3DXMATERIAL*,pMaterials,CONST D3DXEFFECTINSTANCE*,pEffectInstances,DWORD,NumMaterials) \
    ICOM_METHOD5 (HRESULT,Optimize,DWORD,Flags,DWORD*,pAdjacencyOut,DWORD*,pFaceRamp,LPD3DXBUFFER*,ppVertexRemap,LPD3DXMESH*,ppOptMesh) \
    ICOM_METHOD2 (HRESULT,OptimizeBaseLOD,DWORD,Flags,DWORD*,pFaceRamp) \
    ICOM_METHOD4 (HRESULT,TrimByFaces,DWORD,NewFacesMin,DWORD,NewFacesMax,DWORD*,rgiFaceRemap,DWORD*,rgiVertRemap) \
    ICOM_METHOD4 (HRESULT,TrimByVertices,DWORD,NewVerticesMin,DWORD,NewVerticesMax,DWORD*,rgiFaceRemap,DWORD*,rgiVertRemap) \
    ICOM_METHOD1 (HRESULT,GetAdjacency,DWORD*,pAdjacency) \
    ICOM_METHOD1 (HRESULT,GenerateVertexHistory,DWORD*,pVertexHistory)
#define ID3DXPMesh_IMETHODS \
    ID3DXBaseMesh_IMETHODS \
    ID3DXPMesh_METHODS
ICOM_DEFINE(ID3DXPMesh,ID3DXBaseMesh)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXPMesh_QueryInterface(p,a,b)                ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXPMesh_AddRef(p)                            ICOM_CALL(AddRef,p)
#define ID3DXPMesh_Release(p)                           ICOM_CALL(Release,p)
/*** ID3DXBaseMesh methods ***/
#define ID3DXPMesh_DrawSubset(p,a)                      ICOM_CALL1(DrawSubset,p,a)
#define ID3DXPMesh_GetNumFaces(p)                       ICOM_CALL(GetNumFaces,p)
#define ID3DXPMesh_GetNumVertices(p)                    ICOM_CALL(GetNumVertices,p)
#define ID3DXPMesh_GetFVF(p)                            ICOM_CALL(GetFVF,p)
#define ID3DXPMesh_GetDeclaration(p,a)                  ICOM_CALL1(GetDeclaration,p,a)
#define ID3DXPMesh_GetNumBytesPerVertex(p)              ICOM_CALL(GetNumBytesPerVertex,p)
#define ID3DXPMesh_GetOptions(p)                        ICOM_CALL(GetOptions,p)
#define ID3DXPMesh_GetDevice(p,a)                       ICOM_CALL1(GetDevice,p,a)
#define ID3DXPMesh_CloneMeshFVF(p,a,b,c,d)              ICOM_CALL4(CloneMeshFVF,p,a,b,c,d)
#define ID3DXPMesh_CloneMesh(p,a,b,c,d)                 ICOM_CALL4(CloneMesh,p,a,b,c,d)
#define ID3DXPMesh_GetVertexBuffer(p,a)                 ICOM_CALL1(GetVertexBuffer,p,a)
#define ID3DXPMesh_GetIndexBuffer(p,a)                  ICOM_CALL1(GetIndexBuffer,p,a)
#define ID3DXPMesh_LockVertexBuffer(p,a,b)              ICOM_CALL2(LockVertexBuffer,p,a,b)
#define ID3DXPMesh_UnlockVertexBuffer(p)                ICOM_CALL(UnlockVertexBuffer,p)
#define ID3DXPMesh_LockIndexBuffer(p,a,b)               ICOM_CALL2(LockIndexBuffer,p,a,b)
#define ID3DXPMesh_UnlockIndexBuffer(p)                 ICOM_CALL(UnlockIndexBuffer,p)
#define ID3DXPMesh_GetAttributeTable(p,a,b)             ICOM_CALL2(GetAttributeTable,p,a,b)
#define ID3DXPMesh_ConvertPointRepsToAdjacency(p,a,b)   ICOM_CALL2(ConvertPointRepsToAdjacency,p,a,b)
#define ID3DXPMesh_ConvertAdjacencyToPointReps(p,a,b)   ICOM_CALL2(ConvertAdjacencyToPointReps,p,a,b)
#define ID3DXPMesh_GenerateAdjacency(p,a,b)             ICOM_CALL2(GenerateAdjacency,p,a,b)
#define ID3DXPMesh_UpdateSemantics(p,a)                 ICOM_CALL1(UpdateSemantics,p,a)
/*** ID3DXPMesh methods ***/
#define ID3DXPMesh_ClonePMeshFVF(p,a,b,c,d)     ICOM_CALL4(ClonePMeshFVF,p,a,b,c,d)
#define ID3DXPMesh_ClonePMesh(p,a,b,c,d)        ICOM_CALL4(ClonePMesh,p,a,b,c,d)
#define ID3DXPMesh_SetNumFaces(p,a)             ICOM_CALL1(SetNumFaces,p,a)
#define ID3DXPMesh_SetNumVertices(p,a)          ICOM_CALL1(SetNumVertices,p,a)
#define ID3DXPMesh_GetMaxFaces(p)               ICOM_CALL(GetMaxFaces,p)
#define ID3DXPMesh_GetMinFaces(p)               ICOM_CALL(GetMinFaces,p)
#define ID3DXPMesh_GetMaxVertices(p)            ICOM_CALL(GetMaxVertices,p)
#define ID3DXPMesh_GetMinVertices(p)            ICOM_CALL(GetMinVertices,p)
#define ID3DXPMesh_Save(p,a,b,c,d)              ICOM_CALL4(Save,p,a,b,c,d)
#define ID3DXPMesh_Optimize(p,a,b,c,d,e)        ICOM_CALL5(Optimize,p,a,b,c,d,e)
#define ID3DXPMesh_OptimizeBaseLOD(p,a,b)       ICOM_CALL2(OptimizeBaseLOD,p,a,b)
#define ID3DXPMesh_TrimByFaces(p,a,b,c,d)       ICOM_CALL4(TrimByFaces,p,a,b,c,d)
#define ID3DXPMesh_TrimByVertices(p,a,b,c,d)    ICOM_CALL4(TrimByVertices,p,a,b,c,d)
#define ID3DXPMesh_GetAdjacency(p,a)            ICOM_CALL1(GetAdjacency,p,a)
#define ID3DXPMesh_GenerateVertexHistory(p,a)   ICOM_CALL1(GenerateVertexHistory,p,a)


/*****************************************************************************
 * ID3DXSPMesh interface
 */
#define ICOM_INTERFACE ID3DXSPMesh
#define ID3DXSPMesh_METHODS \
    ICOM_METHOD  (DWORD,GetNumFaces) \
    ICOM_METHOD  (DWORD,GetNumVertices) \
    ICOM_METHOD  (DWORD,GetFVF) \
    ICOM_METHOD1 (HRESULT,GetDeclaration,D3DVERTEXELEMENT9*,Declaration) \
    ICOM_METHOD  (DWORD,GetOptions) \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD6 (HRESULT,CloneMeshFVF,DWORD,Options,DWORD,FVF,LPDIRECT3DDEVICE9,pD3D,DWORD*,pAdjacencyOut,DWORD*,pVertexRemapOut,LPD3DXMESH*,ppCloneMesh) \
    ICOM_METHOD6 (HRESULT,CloneMesh,DWORD,Options,CONST D3DVERTEXELEMENT9*,pDeclaration,LPDIRECT3DDEVICE9,pD3D,DWORD*,pAdjacencyOut,DWORD*,pVertexRemapOut,LPD3DXMESH*,ppCloneMesh) \
    ICOM_METHOD6 (HRESULT,ClonePMeshFVF,DWORD,Options,DWORD,FVF,LPDIRECT3DDEVICE9,pD3D,DWORD*,pVertexRemapOut,FLOAT*,pErrorsByFace,LPD3DXPMESH*,ppCloneMesh) \
    ICOM_METHOD6 (HRESULT,ClonePMesh,DWORD,Options,CONST D3DVERTEXELEMENT9*,pDeclaration,LPDIRECT3DDEVICE9,pD3D,DWORD*,pVertexRemapOut,FLOAT*,pErrorsByFace,LPD3DXPMESH*,ppCloneMesh) \
    ICOM_METHOD1 (HRESULT,ReduceFaces,DWORD,Faces) \
    ICOM_METHOD1 (HRESULT,ReduceVertices,DWORD,Vertices) \
    ICOM_METHOD  (DWORD,GetMaxFaces) \
    ICOM_METHOD  (DWORD,GetMaxVertices) \
    ICOM_METHOD1 (HRESULT,GetVertexAttributeWeights,LPD3DXATTRIBUTEWEIGHTS,pVertexAttributeWeights) \
    ICOM_METHOD1 (HRESULT,GetVertexWeights,FLOAT*,pVertexWeights)
#define ID3DXSPMesh_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXSPMesh_METHODS
ICOM_DEFINE(ID3DXSPMesh,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXSPMesh_QueryInterface(p,a,b)               ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXSPMesh_AddRef(p)                           ICOM_CALL(AddRef,p)
#define ID3DXSPMesh_Release(p)                          ICOM_CALL(Release,p)
/*** ID3DXSPMesh methods ***/
#define ID3DXSPMesh_GetNumFaces(p)                      ICOM_CALL(GetNumFaces,p)
#define ID3DXSPMesh_GetNumVertices(p)                   ICOM_CALL(GetNumVertices,p)
#define ID3DXSPMesh_GetFVF(p)                           ICOM_CALL(GetFVF,p)
#define ID3DXSPMesh_GetDeclaration(p,a)                 ICOM_CALL1(GetDeclaration,p,a)
#define ID3DXSPMesh_GetOptions(p)                       ICOM_CALL(GetOptions,p)
#define ID3DXSPMesh_GetDevice(p,a)                      ICOM_CALL1(GetDevice,p,a)
#define ID3DXSPMesh_CloneMeshFVF(p,a,b,c,d,e,f)         ICOM_CALL6(CloneMeshFVF,p,a,b,c,d,e,f)
#define ID3DXSPMesh_CloneMesh(p,a,b,c,d,e,f)            ICOM_CALL6(CloneMesh,p,a,b,c,d,e,f)
#define ID3DXSPMesh_ClonePMeshFVF(p,a,b,c,d,e,f)        ICOM_CALL6(ClonePMeshFVF,p,a,b,c,d,e,f)
#define ID3DXSPMesh_ClonePMesh(p,a,b,c,d,e,f)           ICOM_CALL6(ClonePMesh,p,a,b,c,d,e,f)
#define ID3DXSPMesh_ReduceFaces(p,a)                    ICOM_CALL1(ReduceFaces,p,a)
#define ID3DXSPMesh_ReduceVertices(p,a)                 ICOM_CALL1(ReduceVertices,p,a)
#define ID3DXSPMesh_GetMaxFaces(p)                      ICOM_CALL(GetMaxFaces,p)
#define ID3DXSPMesh_GetMaxVertices(p)                   ICOM_CALL(GetMaxVertices,p)
#define ID3DXSPMesh_GetVertexAttributeWeights(p,a)      ICOM_CALL1(GetVertexAttributeWeights,p,a)
#define ID3DXSPMesh_GetVertexWeights(p,a)               ICOM_CALL1(GetVertexWeights,p,a)


/*****************************************************************************
 * ID3DXPatchMesh interface
 */
#define ICOM_INTERFACE ID3DXPatchMesh
#define ID3DXPatchMesh_METHODS \
    ICOM_METHOD  (DWORD,GetNumPatches) \
    ICOM_METHOD  (DWORD,GetNumVertices) \
    ICOM_METHOD1 (HRESULT,GetDeclaration,LPD3DVERTEXELEMENT9,pDeclaration) \
    ICOM_METHOD  (DWORD,GetControlVerticesPerPatch) \
    ICOM_METHOD  (DWORD,GetOptions) \
    ICOM_METHOD1 (HRESULT,GetDevice,LPDIRECT3DDEVICE9*,ppDevice) \
    ICOM_METHOD1 (HRESULT,GetPatchInfo,LPD3DXPATCHINFO,PatchInfo) \
    ICOM_METHOD1 (HRESULT,GetVertexBuffer,LPDIRECT3DVERTEXBUFFER9*,ppVB) \
    ICOM_METHOD1 (HRESULT,GetIndexBuffer,LPDIRECT3DINDEXBUFFER9*,ppIB) \
    ICOM_METHOD2 (HRESULT,LockVertexBuffer,DWORD,flags,void**,ppData) \
    ICOM_METHOD  (HRESULT,UnlockVertexBuffer) \
    ICOM_METHOD2 (HRESULT,LockIndexBuffer,DWORD,flags,void**,ppData) \
    ICOM_METHOD  (HRESULT,UnlockIndexBuffer) \
    ICOM_METHOD2 (HRESULT,LockAttributeBuffer,DWORD,flags,DWORD**,ppData) \
    ICOM_METHOD  (HRESULT,UnlockAttributeBuffer) \
    ICOM_METHOD4 (HRESULT,GetTessSize,FLOAT,fTessLevel,DWORD,Adaptive,DWORD*,NumTriangles,DWORD*,NumVertices) \
    ICOM_METHOD1 (HRESULT,GenerateAdjacency,FLOAT,Tolerance) \
    ICOM_METHOD3 (HRESULT,CloneMesh,DWORD,Options,CONST D3DVERTEXELEMENT9*,pDecl,LPD3DXPATCHMESH*,pMesh) \
    ICOM_METHOD1 (HRESULT,Optimize,DWORD,flags) \
    ICOM_METHOD6 (HRESULT,SetDisplaceParam,LPDIRECT3DBASETEXTURE9,Texture,D3DTEXTUREFILTERTYPE,MinFilter,D3DTEXTUREFILTERTYPE,MagFilter,D3DTEXTUREFILTERTYPE,MipFilter,D3DTEXTUREADDRESS,Wrap,DWORD,dwLODBias) \
    ICOM_METHOD6 (HRESULT,GetDisplaceParam,LPDIRECT3DBASETEXTURE9*,Texture,D3DTEXTUREFILTERTYPE*,MinFilter,D3DTEXTUREFILTERTYPE*,MagFilter,D3DTEXTUREFILTERTYPE*,MipFilter,D3DTEXTUREADDRESS*,Wrap,DWORD*,dwLODBias) \
    ICOM_METHOD2 (HRESULT,Tessellate,FLOAT,fTessLevel,LPD3DXMESH,pMesh) \
    ICOM_METHOD4 (HRESULT,TessellateAdaptive,D3DXVECTOR4*,pTrans,DWORD,dwMaxTessLevel,DWORD,dwMinTessLevel,LPD3DXMESH,pMesh)
#define ID3DXPatchMesh_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXPatchMesh_METHODS
ICOM_DEFINE(ID3DXPatchMesh,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXPatchMesh_QueryInterface(p,a,b)            ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXPatchMesh_AddRef(p)                        ICOM_CALL(AddRef,p)
#define ID3DXPatchMesh_Release(p)                       ICOM_CALL(Release,p)
/*** ID3DXPatchMesh methods ***/
#define ID3DXPatchMesh_GetNumPatches(p)                 ICOM_CALL(GetNumPatches,p)
#define ID3DXPatchMesh_GetNumVertices(p)                ICOM_CALL(GetNumVertices,p)
#define ID3DXPatchMesh_GetDeclaration(p,a)              ICOM_CALL1(GetDeclaration,p,a)
#define ID3DXPatchMesh_GetControlVerticesPerPatch(p)    ICOM_CALL(GetControlVerticesPerPatch,p)
#define ID3DXPatchMesh_GetOptions(p)                    ICOM_CALL(GetOptions,p)
#define ID3DXPatchMesh_GetDevice(p,a)                   ICOM_CALL1(GetDevice,p,a)
#define ID3DXPatchMesh_GetPatchInfo(p,a)                ICOM_CALL1(GetPatchInfo,p,a)
#define ID3DXPatchMesh_GetVertexBuffer(p,a)             ICOM_CALL1(GetVertexBuffer,p,a)
#define ID3DXPatchMesh_GetIndexBuffer(p,a)              ICOM_CALL1(GetIndexBuffer,p,a)
#define ID3DXPatchMesh_LockVertexBuffer(p,a,b)          ICOM_CALL2(LockVertexBuffer,p,a,b)
#define ID3DXPatchMesh_UnlockVertexBuffer(p)            ICOM_CALL(UnlockVertexBuffer,p)
#define ID3DXPatchMesh_LockIndexBuffer(p,a,b)           ICOM_CALL2(LockIndexBuffer,p,a,b)
#define ID3DXPatchMesh_UnlockIndexBuffer(p)             ICOM_CALL(UnlockIndexBuffer,p)
#define ID3DXPatchMesh_LockAttributeBuffer(p,a,b)       ICOM_CALL2(LockAttributeBuffer,p,a,b)
#define ID3DXPatchMesh_UnlockAttributeBuffer(p)         ICOM_CALL(UnlockAttributeBuffer,p)
#define ID3DXPatchMesh_GetTessSize(p,a,b,c,d)           ICOM_CALL4(GetTessSize,p,a,b,c,d)
#define ID3DXPatchMesh_GenerateAdjacency(p,a)           ICOM_CALL1(GenerateAdjacency,p,a)
#define ID3DXPatchMesh_CloneMesh(p,a,b,c)               ICOM_CALL3(CloneMesh,p,a,b,c)
#define ID3DXPatchMesh_Optimize(p,a)                    ICOM_CALL1(Optimize,p,a)
#define ID3DXPatchMesh_SetDisplaceParam(p,a,b,c,d,e,f)  ICOM_CALL6(SetDisplaceParam,p,a,b,c,d,e,f)
#define ID3DXPatchMesh_GetDisplaceParam(p,a,b,c,d,e,f)  ICOM_CALL6(GetDisplaceParam,p,a,b,c,d,e,f)
#define ID3DXPatchMesh_Tessellate(p,a,b)                ICOM_CALL2(Tessellate,p,a,b)
#define ID3DXPatchMesh_TessellateAdaptive(p,a,b,c,d)    ICOM_CALL4(TessellateAdaptive,p,a,b,c,d)


/*****************************************************************************
 * ID3DXSkinInfo interface
 */
#define ICOM_INTERFACE ID3DXSkinInfo
#define ID3DXSkinInfo_METHODS \
    ICOM_METHOD4 (HRESULT,SetBoneInfluence,DWORD,bone,DWORD,numInfluences,CONST DWORD*,vertices,CONST FLOAT*,weights) \
	ICOM_METHOD3 (HRESULT,SetBoneVertexInfluence,DWORD,boneNum,DWORD,influenceNum,FLOAT,weight) \
    ICOM_METHOD1 (DWORD,GetNumBoneInfluences,DWORD,bone) \
    ICOM_METHOD3 (HRESULT,GetBoneInfluence,DWORD,bone,DWORD*,vertices,FLOAT*,weights) \
    ICOM_METHOD4 (HRESULT,GetBoneVertexInfluence,DWORD,boneNum,DWORD,influenceNum,FLOAT*,pWeight,DWORD*,pVertexNum) \
    ICOM_METHOD1 (HRESULT,GetMaxVertexInfluence,DWORD*,maxVertexInfluences) \
    ICOM_METHOD  (DWORD,GetNumBones) \
	ICOM_METHOD3 (HRESULT,FindBoneVertexInfluenceIndex,DWORD,boneNum,DWORD,vertexNum,DWORD*,pInfluenceIndex) \
    ICOM_METHOD3 (HRESULT,GetMaxFaceInfluences,LPDIRECT3DINDEXBUFFER9,pIB,DWORD,NumFaces,DWORD*,maxFaceInfluences) \
    ICOM_METHOD1 (HRESULT,SetMinBoneInfluence,FLOAT,MinInfl) \
    ICOM_METHOD  (FLOAT,GetMinBoneInfluence) \
    ICOM_METHOD2 (HRESULT,SetBoneName,DWORD,Bone,LPCSTR,pName) \
    ICOM_METHOD1 (LPCSTR,GetBoneName,DWORD,Bone) \
    ICOM_METHOD2 (HRESULT,SetBoneOffsetMatrix,DWORD,Bone,LPD3DXMATRIX,pBoneTransform) \
    ICOM_METHOD1 (LPD3DXMATRIX,GetBoneOffsetMatrix,DWORD,Bone) \
    ICOM_METHOD1 (HRESULT,Clone,LPD3DXSKININFO*,ppSkinInfo) \
    ICOM_METHOD2 (HRESULT,Remap,DWORD,NumVertices,DWORD*,pVertexRemap) \
    ICOM_METHOD1 (HRESULT,SetFVF,DWORD,FVF) \
    ICOM_METHOD1 (HRESULT,SetDeclaration,CONST D3DVERTEXELEMENT9*,Declaration) \
    ICOM_METHOD  (DWORD,GetFVF) \
    ICOM_METHOD1 (HRESULT,GetDeclaration,D3DVERTEXELEMENT9*,Declaration) \
    ICOM_METHOD4 (HRESULT,UpdateSkinnedMesh,CONST D3DXMATRIX*,pBoneTransforms,CONST D3DXMATRIX*,pBoneInvTransposeTransforms,PVOID,pVerticesSrc,PVOID,pVerticesDst) \
    ICOM_METHOD10 (HRESULT,ConvertToBlendedMesh,LPD3DXMESH,pMesh,DWORD,Options,CONST DWORD*,pAdjacencyIn,LPDWORD,pAdjacencyOut,DWORD*,pFaceRemap,LPD3DXBUFFER*,ppVertexRemap,DWORD*,pMaxFaceInfl,DWORD*,pNumBoneCombinations,LPD3DXBUFFER*,ppBoneCombinationTable,LPD3DXMESH*,ppMesh) \
    ICOM_METHOD11 (HRESULT,ConvertToIndexedBlendedMesh,LPD3DXMESH,pMesh,DWORD,Options,DWORD,paletteSize,CONST DWORD*,pAdjacencyIn,LPDWORD,pAdjacencyOut,DWORD*,pFaceRemap,LPD3DXBUFFER*,ppVertexRemap,DWORD*,pMaxVertexInfl,DWORD*,pNumBoneCombinations,LPD3DXBUFFER*,ppBoneCombinationTable,LPD3DXMESH*,ppMesh)
#define ID3DXSkinInfo_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXSkinInfo_METHODS
ICOM_DEFINE(ID3DXSkinInfo,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXSkinInfo_QueryInterface(p,a,b)                                 ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXSkinInfo_AddRef(p)                                             ICOM_CALL(AddRef,p)
#define ID3DXSkinInfo_Release(p)                                            ICOM_CALL(Release,p)
/*** ID3DXSkinInfo methods ***/
#define ID3DXSkinInfo_SetBoneInfluence(p,a,b,c,d)                           ICOM_CALL4(SetBoneInfluence,p,a,b,c,d)
#define ID3DXSkinInfo_GetNumBoneInfluences(p,a)                             ICOM_CALL1(GetNumBoneInfluences,p,a)
#define ID3DXSkinInfo_GetBoneInfluence(p,a,b,c)                             ICOM_CALL3(GetBoneInfluence,p,a,b,c)
#define ID3DXSkinInfo_GetMaxVertexInfluence(p,a)                            ICOM_CALL1(GetMaxVertexInfluence,p,a)
#define ID3DXSkinInfo_GetNumBones(p)                                        ICOM_CALL(GetNumBones,p)
#define ID3DXSkinInfo_GetMaxFaceInfluences(p,a,b,c)                         ICOM_CALL3(GetMaxFaceInfluences,p,a,b,c)
#define ID3DXSkinInfo_SetMinBoneInfluence(p,a)                              ICOM_CALL1(SetMinBoneInfluence,p,a)
#define ID3DXSkinInfo_GetMinBoneInfluence(p)                                ICOM_CALL(GetMinBoneInfluence,p)
#define ID3DXSkinInfo_SetBoneName(p,a,b)                                    ICOM_CALL2(SetBoneName,p,a,b)
#define ID3DXSkinInfo_GetBoneName(p,a)                                      ICOM_CALL1(GetBoneName,p,a)
#define ID3DXSkinInfo_SetBoneOffsetMatrix(p,a,b)                            ICOM_CALL2(SetBoneOffsetMatrix,p,a,b)
#define ID3DXSkinInfo_GetBoneOffsetMatrix(p,a)                              ICOM_CALL1(GetBoneOffsetMatrix,p,a)
#define ID3DXSkinInfo_Clone(p,a)                                            ICOM_CALL1(Clone,p,a)
#define ID3DXSkinInfo_Remap(p,a,b)                                          ICOM_CALL2(Remap,p,a,b)
#define ID3DXSkinInfo_SetFVF(p,a)                                           ICOM_CALL1(SetFVF,p,a)
#define ID3DXSkinInfo_SetDeclaration(p,a)                                   ICOM_CALL1(SetDeclaration,p,a)
#define ID3DXSkinInfo_GetFVF(p)                                             ICOM_CALL(GetFVF,p)
#define ID3DXSkinInfo_GetDeclaration(p,a)                                   ICOM_CALL1(GetDeclaration,p,a)
#define ID3DXSkinInfo_UpdateSkinnedMesh(p,a,b,c,d)                          ICOM_CALL4(UpdateSkinnedMesh,p,a,b,c,d)
#define ID3DXSkinInfo_ConvertToBlendedMesh(p,a,b,c,d,e,f,g,h,i,j)           ICOM_CALL10(ConvertToBlendedMesh,p,a,b,c,d,e,f,g,h,i,j)
#define ID3DXSkinInfo_ConvertToIndexedBlendedMesh(p,a,b,c,d,e,f,g,h,i,j,k)  ICOM_CALL11(ConvertToIndexedBlendedMesh,p,a,b,c,d,e,f,g,h,i,j,k)
#define ID3DXSkinInfo_SetBoneVertexInfluence(p,a,b,c)						ICOM_CALL3(SetBoneVertexInfluence,p,a,b,c)
#define ID3DXSkinInfo_GetBoneVertexInfluence(p,a,b,c,d)						ICOM_CALL4(GetBoneVertexInfluence,p,a,b,c,d)

/****************************************************************************
 * D3DX9 Mesh functions
 */
HRESULT WINAPI 
    D3DXCreateMesh(
        DWORD NumFaces, 
        DWORD NumVertices, 
        DWORD Options, 
        CONST D3DVERTEXELEMENT9 *pDeclaration, 
        LPDIRECT3DDEVICE9 pD3D, 
        LPD3DXMESH* ppMesh);

HRESULT WINAPI 
    D3DXCreateMeshFVF(
        DWORD NumFaces, 
        DWORD NumVertices, 
        DWORD Options, 
        DWORD FVF, 
        LPDIRECT3DDEVICE9 pD3D, 
        LPD3DXMESH* ppMesh);

HRESULT WINAPI 
    D3DXCreateSPMesh(
        LPD3DXMESH pMesh, 
        CONST DWORD* pAdjacency, 
        CONST D3DXATTRIBUTEWEIGHTS *pVertexAttributeWeights,
        CONST FLOAT *pVertexWeights,
        LPD3DXSPMESH* ppSMesh);

HRESULT WINAPI
    D3DXCleanMesh(
    D3DXCLEANTYPE CleanType,   /* This parameter added in later SDKs */
    LPD3DXMESH pMeshIn,
    CONST DWORD* pAdjacencyIn,
    LPD3DXMESH* ppMeshOut,
    DWORD* pAdjacencyOut,
    LPD3DXBUFFER* ppErrorsAndWarnings);

HRESULT WINAPI
    D3DXValidMesh(
    LPD3DXMESH pMeshIn,
    CONST DWORD* pAdjacency,
    LPD3DXBUFFER* ppErrorsAndWarnings);

HRESULT WINAPI 
    D3DXGeneratePMesh(
        LPD3DXMESH pMesh, 
        CONST DWORD* pAdjacency, 
        CONST D3DXATTRIBUTEWEIGHTS *pVertexAttributeWeights,
        CONST FLOAT *pVertexWeights,
        DWORD MinValue, 
        DWORD Options, 
        LPD3DXPMESH* ppPMesh);

HRESULT WINAPI 
    D3DXSimplifyMesh(
        LPD3DXMESH pMesh, 
        CONST DWORD* pAdjacency, 
        CONST D3DXATTRIBUTEWEIGHTS *pVertexAttributeWeights,
        CONST FLOAT *pVertexWeights,
        DWORD MinValue, 
        DWORD Options, 
        LPD3DXMESH* ppMesh);

HRESULT WINAPI 
    D3DXComputeBoundingSphere(
        CONST D3DXVECTOR3 *pFirstPosition,
        DWORD NumVertices, 
        DWORD dwStride,
        D3DXVECTOR3 *pCenter, 
        FLOAT *pRadius);

HRESULT WINAPI 
    D3DXComputeBoundingBox(
        CONST D3DXVECTOR3 *pFirstPosition,
        DWORD NumVertices, 
        DWORD dwStride,
        D3DXVECTOR3 *pMin, 
        D3DXVECTOR3 *pMax);

HRESULT WINAPI 
    D3DXComputeNormals(
        LPD3DXBASEMESH pMesh,
        CONST DWORD *pAdjacency);

HRESULT WINAPI 
    D3DXCreateBuffer(
        DWORD NumBytes, 
        LPD3DXBUFFER *ppBuffer);

HRESULT WINAPI
    D3DXLoadMeshFromXA(
        LPCSTR pFilename, 
        DWORD Options, 
        LPDIRECT3DDEVICE9 pD3D, 
        LPD3DXBUFFER *ppAdjacency,
        LPD3DXBUFFER *ppMaterials, 
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD *pNumMaterials,
        LPD3DXMESH *ppMesh);

HRESULT WINAPI
    D3DXLoadMeshFromXW(
        LPCWSTR pFilename, 
        DWORD Options, 
        LPDIRECT3DDEVICE9 pD3D, 
        LPD3DXBUFFER *ppAdjacency,
        LPD3DXBUFFER *ppMaterials, 
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD *pNumMaterials,
        LPD3DXMESH *ppMesh);

#ifdef UNICODE
#define D3DXLoadMeshFromX D3DXLoadMeshFromXW
#else
#define D3DXLoadMeshFromX D3DXLoadMeshFromXA
#endif

HRESULT WINAPI 
    D3DXLoadMeshFromXInMemory(
        LPCVOID Memory,
        DWORD SizeOfMemory,
        DWORD Options, 
        LPDIRECT3DDEVICE9 pD3D, 
        LPD3DXBUFFER *ppAdjacency,
        LPD3DXBUFFER *ppMaterials, 
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD *pNumMaterials,
        LPD3DXMESH *ppMesh);

HRESULT WINAPI 
    D3DXLoadMeshFromXResource(
        HMODULE Module,
        LPCSTR Name,
        LPCSTR Type,
        DWORD Options, 
        LPDIRECT3DDEVICE9 pD3D, 
        LPD3DXBUFFER *ppAdjacency,
        LPD3DXBUFFER *ppMaterials, 
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD *pNumMaterials,
        LPD3DXMESH *ppMesh);

HRESULT WINAPI 
    D3DXSaveMeshToXA(
        LPCSTR pFilename,
        LPD3DXMESH pMesh,
        CONST DWORD* pAdjacency,
        CONST D3DXMATERIAL* pMaterials,
        CONST D3DXEFFECTINSTANCE* pEffectInstances, 
        DWORD NumMaterials,
        DWORD Format
        );

HRESULT WINAPI 
    D3DXSaveMeshToXW(
        LPCWSTR pFilename,
        LPD3DXMESH pMesh,
        CONST DWORD* pAdjacency,
        CONST D3DXMATERIAL* pMaterials,
        CONST D3DXEFFECTINSTANCE* pEffectInstances, 
        DWORD NumMaterials,
        DWORD Format
        );
        
#ifdef UNICODE
#define D3DXSaveMeshToX D3DXSaveMeshToXW
#else
#define D3DXSaveMeshToX D3DXSaveMeshToXA
#endif
        
HRESULT WINAPI 
    D3DXCreatePMeshFromStream(
        IStream *pStream, 
        DWORD Options,
        LPDIRECT3DDEVICE9 pD3DDevice, 
        LPD3DXBUFFER *ppMaterials,
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD* pNumMaterials,
        LPD3DXPMESH *ppPMesh);

HRESULT WINAPI
    D3DXCreateSkinInfo(
        DWORD NumVertices,
        CONST D3DVERTEXELEMENT9 *pDeclaration, 
        DWORD NumBones,
        LPD3DXSKININFO* ppSkinInfo);
        
HRESULT WINAPI
    D3DXCreateSkinInfoFVF(
        DWORD NumVertices,
        DWORD FVF,
        DWORD NumBones,
        LPD3DXSKININFO* ppSkinInfo);
        
#ifdef __cplusplus
}

extern "C" {
#endif

HRESULT WINAPI 
    D3DXLoadMeshFromXof(
        LPD3DXFILEDATA pXofObjMesh, 
        DWORD Options, 
        LPDIRECT3DDEVICE9 pD3DDevice, 
        LPD3DXBUFFER *ppAdjacency,
        LPD3DXBUFFER *ppMaterials, 
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD *pNumMaterials,
        LPD3DXMESH *ppMesh);

HRESULT WINAPI
    D3DXLoadSkinMeshFromXof(
        LPDIRECTXFILEDATA pxofobjMesh, 
        DWORD Options,
        LPDIRECT3DDEVICE9 pD3D,
        LPD3DXBUFFER* ppAdjacency,
        LPD3DXBUFFER* ppMaterials,
        LPD3DXBUFFER *ppEffectInstances, 
        DWORD *pMatOut,
        LPD3DXSKININFO* ppSkinInfo,
        LPD3DXMESH* ppMesh);

HRESULT WINAPI
    D3DXCreateSkinInfoFromBlendedMesh(
        LPD3DXBASEMESH pMesh,
        DWORD NumBoneCombinations,
        CONST D3DXBONECOMBINATION *pBoneCombinationTable,
        LPD3DXSKININFO* ppSkinInfo);
        
HRESULT WINAPI
    D3DXTessellateNPatches(
        LPD3DXMESH pMeshIn,             
        CONST DWORD* pAdjacencyIn,             
        FLOAT NumSegs,                    
        BOOL  QuadraticInterpNormals,
        LPD3DXMESH *ppMeshOut,
        LPD3DXBUFFER *ppAdjacencyOut);

HRESULT WINAPI
    D3DXGenerateOutputDecl(
        D3DVERTEXELEMENT9 *pOutput,
        CONST D3DVERTEXELEMENT9 *pInput);

HRESULT WINAPI
    D3DXLoadPatchMeshFromXof(
        LPD3DXFILEDATA pXofObjMesh,
        DWORD Options,
        LPDIRECT3DDEVICE9 pDevice,
        LPD3DXBUFFER *ppMaterials,
        LPD3DXBUFFER *ppEffectInstances, 
        PDWORD pNumMaterials,
        LPD3DXPATCHMESH *ppMesh);

HRESULT WINAPI
    D3DXRectPatchSize(
        CONST FLOAT *pfNumSegs,
        DWORD *pdwTriangles,
        DWORD *pdwVertices);

HRESULT WINAPI
    D3DXTriPatchSize(
        CONST FLOAT *pfNumSegs,
        DWORD *pdwTriangles,
        DWORD *pdwVertices);

HRESULT WINAPI
    D3DXTessellateRectPatch(
        LPDIRECT3DVERTEXBUFFER9 pVB,
        CONST FLOAT *pNumSegs,
        CONST D3DVERTEXELEMENT9 *pdwInDecl,
        CONST D3DRECTPATCH_INFO *pRectPatchInfo,
        LPD3DXMESH pMesh);

HRESULT WINAPI
    D3DXTessellateTriPatch(
      LPDIRECT3DVERTEXBUFFER9 pVB,
      CONST FLOAT *pNumSegs,
      CONST D3DVERTEXELEMENT9 *pInDecl,
      CONST D3DTRIPATCH_INFO *pTriPatchInfo,
      LPD3DXMESH pMesh);

HRESULT WINAPI
    D3DXCreateNPatchMesh(
        LPD3DXMESH pMeshSysMem,
        LPD3DXPATCHMESH *pPatchMesh);

HRESULT WINAPI
    D3DXCreatePatchMesh(
        CONST D3DXPATCHINFO *pInfo,
        DWORD dwNumPatches,
        DWORD dwNumVertices,
        DWORD dwOptions,
        CONST D3DVERTEXELEMENT9 *pDecl,
        LPDIRECT3DDEVICE9 pDevice,
        LPD3DXPATCHMESH *pPatchMesh);

HRESULT WINAPI
    D3DXValidPatchMesh(LPD3DXPATCHMESH pMesh,
                        DWORD *dwcDegenerateVertices,
                        DWORD *dwcDegeneratePatches,
                        LPD3DXBUFFER *ppErrorsAndWarnings);

UINT WINAPI
    D3DXGetFVFVertexSize(DWORD FVF);

UINT WINAPI 
    D3DXGetDeclVertexSize(CONST D3DVERTEXELEMENT9 *pDecl,DWORD Stream);

UINT WINAPI 
    D3DXGetDeclLength(CONST D3DVERTEXELEMENT9 *pDecl);

HRESULT WINAPI
    D3DXDeclaratorFromFVF(
        DWORD FVF,
        D3DVERTEXELEMENT9 pDeclarator[MAX_FVF_DECL_SIZE]);

HRESULT WINAPI
    D3DXFVFFromDeclarator(
        CONST D3DVERTEXELEMENT9 *pDeclarator,
        DWORD *pFVF);

HRESULT WINAPI 
    D3DXWeldVertices(
        CONST LPD3DXMESH pMesh,         
        DWORD Flags,
        CONST D3DXWELDEPSILONS *pEpsilons,                 
        CONST DWORD *pAdjacencyIn, 
        DWORD *pAdjacencyOut,
        DWORD *pFaceRemap, 
        LPD3DXBUFFER *ppVertexRemap);


HRESULT WINAPI
    D3DXIntersect(
        LPD3DXBASEMESH pMesh,
        CONST D3DXVECTOR3 *pRayPos,
        CONST D3DXVECTOR3 *pRayDir, 
        BOOL    *pHit,
        DWORD   *pFaceIndex,
        FLOAT   *pU,
        FLOAT   *pV,
        FLOAT   *pDist,
        LPD3DXBUFFER *ppAllHits,
        DWORD   *pCountOfHits);

HRESULT WINAPI
    D3DXIntersectSubset(
        LPD3DXBASEMESH pMesh,
        DWORD AttribId,
        CONST D3DXVECTOR3 *pRayPos,
        CONST D3DXVECTOR3 *pRayDir, 
        BOOL    *pHit,
        DWORD   *pFaceIndex,
        FLOAT   *pU,
        FLOAT   *pV,
        FLOAT   *pDist,
        LPD3DXBUFFER *ppAllHits,
        DWORD   *pCountOfHits);


HRESULT WINAPI D3DXSplitMesh
    (
    LPD3DXMESH pMeshIn,         
    CONST DWORD *pAdjacencyIn, 
    CONST DWORD MaxSize,
    CONST DWORD Options,
    DWORD *pMeshesOut,
    LPD3DXBUFFER *ppMeshArrayOut,
    LPD3DXBUFFER *ppAdjacencyArrayOut,
    LPD3DXBUFFER *ppFaceRemapArrayOut,
    LPD3DXBUFFER *ppVertRemapArrayOut
    );

BOOL WINAPI D3DXIntersectTri 
(
    CONST D3DXVECTOR3 *p0,
    CONST D3DXVECTOR3 *p1,
    CONST D3DXVECTOR3 *p2,
    CONST D3DXVECTOR3 *pRayPos,
    CONST D3DXVECTOR3 *pRayDir,
    FLOAT *pU,
    FLOAT *pV,
    FLOAT *pDist);

BOOL WINAPI
    D3DXSphereBoundProbe(
        CONST D3DXVECTOR3 *pCenter,
        FLOAT Radius,
        CONST D3DXVECTOR3 *pRayPosition,
        CONST D3DXVECTOR3 *pRayDirection);

BOOL WINAPI 
    D3DXBoxBoundProbe(
        CONST D3DXVECTOR3 *pMin, 
        CONST D3DXVECTOR3 *pMax,
        CONST D3DXVECTOR3 *pRayPosition,
        CONST D3DXVECTOR3 *pRayDirection);

HRESULT WINAPI D3DXComputeTangentFrame(ID3DXMesh *pMesh,
                                       DWORD dwOptions);

HRESULT WINAPI D3DXComputeTangentFrameEx(ID3DXMesh *pMesh,
                                         DWORD dwTextureInSemantic,
                                         DWORD dwTextureInIndex,
                                         DWORD dwUPartialOutSemantic,
                                         DWORD dwUPartialOutIndex,
                                         DWORD dwVPartialOutSemantic,
                                         DWORD dwVPartialOutIndex,
                                         DWORD dwNormalOutSemantic,
                                         DWORD dwNormalOutIndex,
                                         DWORD dwOptions,
                                         CONST DWORD *pdwAdjacency,
                                         FLOAT fPartialEdgeThreshold,
                                         FLOAT fSingularPointThreshold,
                                         FLOAT fNormalEdgeThreshold,
                                         ID3DXMesh **ppMeshOut,
                                         ID3DXBuffer **ppVertexMapping);


HRESULT WINAPI D3DXComputeTangent(LPD3DXMESH Mesh,
                                 DWORD TexStage,
                                 DWORD TangentIndex,
                                 DWORD BinormIndex,
                                 DWORD Wrap,
                                 CONST DWORD *Adjacency);

HRESULT WINAPI
D3DXConvertMeshSubsetToSingleStrip
(
    LPD3DXBASEMESH MeshIn,
    DWORD AttribId,
    DWORD IBOptions,
    LPDIRECT3DINDEXBUFFER9 *ppIndexBuffer,
    DWORD *pNumIndices
);

HRESULT WINAPI
D3DXConvertMeshSubsetToStrips
(
    LPD3DXBASEMESH MeshIn,
    DWORD AttribId,
    DWORD IBOptions,
    LPDIRECT3DINDEXBUFFER9 *ppIndexBuffer,
    DWORD *pNumIndices,
    LPD3DXBUFFER *ppStripLengths,
    DWORD *pNumStrips
);


#ifdef __cplusplus
}
#endif


/*************************************************************************
 * DXFileObject GUIDs
 */
DEFINE_GUID(DXFILEOBJ_XSkinMeshHeader,         0x3cf169ce,0xff7c,0x44ab,0x93,0xc0,0xf7,0x8f,0x62,0xd1,0x72,0xe2);
DEFINE_GUID(DXFILEOBJ_VertexDuplicationIndices,0xb8d65549,0xd7c9,0x4995,0x89,0xcf,0x53,0xa9,0xa8,0xb0,0x31,0xe3);
DEFINE_GUID(DXFILEOBJ_FaceAdjacency,           0xa64c844a,0xe282,0x4756,0x8b,0x80,0x25,0xc ,0xde,0x4 ,0x39,0x8c);
DEFINE_GUID(DXFILEOBJ_SkinWeights,             0x6f0d123b,0xbad2,0x4167,0xa0,0xd0,0x80,0x22,0x4f,0x25,0xfa,0xbb);
DEFINE_GUID(DXFILEOBJ_Patch,                   0xa3eb5d44,0xfc22,0x429d,0x9a,0xfb,0x32,0x21,0xcb,0x97,0x19,0xa6);
DEFINE_GUID(DXFILEOBJ_PatchMesh,               0xd02c95cc,0xedba,0x4305,0x9b,0x5d,0x18,0x20,0xd7,0x70,0x4b,0xbf);
DEFINE_GUID(DXFILEOBJ_PatchMesh9,              0xb9ec94e1,0xb9a6,0x4251,0xba,0x18,0x94,0x89,0x3f,0x2 ,0xc0,0xea);
DEFINE_GUID(DXFILEOBJ_PMInfo,                  0xb6c3e656,0xec8b,0x4b92,0x9b,0x62,0x68,0x16,0x59,0x52,0x29,0x47);
DEFINE_GUID(DXFILEOBJ_PMAttributeRange,        0x917e0427,0xc61e,0x4a14,0x9c,0x64,0xaf,0xe6,0x5f,0x9e,0x98,0x44);
DEFINE_GUID(DXFILEOBJ_PMVSplitRecord,          0x574ccc14,0xf0b3,0x4333,0x82,0x2d,0x93,0xe8,0xa8,0xa0,0x8e,0x4c);
DEFINE_GUID(DXFILEOBJ_FVFData,                 0xb6e70a0e,0x8ef9,0x4e83,0x94,0xad,0xec,0xc8,0xb0,0xc0,0x48,0x97);
DEFINE_GUID(DXFILEOBJ_VertexElement,           0xf752461c,0x1e23,0x48f6,0xb9,0xf8,0x83,0x50,0x85,0xf ,0x33,0x6f);
DEFINE_GUID(DXFILEOBJ_DeclData,                0xbf22e553,0x292c,0x4781,0x9f,0xea,0x62,0xbd,0x55,0x4b,0xdd,0x93);
DEFINE_GUID(DXFILEOBJ_EffectFloats,            0xf1cfe2b3,0xde3 ,0x4e28,0xaf,0xa1,0x15,0x5a,0x75,0xa ,0x28,0x2d);
DEFINE_GUID(DXFILEOBJ_EffectString,            0xd55b097e,0xbdb6,0x4c52,0xb0,0x3d,0x60,0x51,0xc8,0x9d,0xe ,0x42);
DEFINE_GUID(DXFILEOBJ_EffectDWord,             0x622c0ed0,0x956e,0x4da9,0x90,0x8a,0x2a,0xf9,0x4f,0x3c,0xe7,0x16);
DEFINE_GUID(DXFILEOBJ_EffectParamFloats,       0x3014b9a0,0x62f5,0x478c,0x9b,0x86,0xe4,0xac,0x9f,0x4e,0x41,0x8b);
DEFINE_GUID(DXFILEOBJ_EffectParamString,       0x1dbc4c88,0x94c1,0x46ee,0x90,0x76,0x2c,0x28,0x81,0x8c,0x94,0x81);
DEFINE_GUID(DXFILEOBJ_EffectParamDWord,        0xe13963bc,0xae51,0x4c5d,0xb0,0xf ,0xcf,0xa3,0xa9,0xd9,0x7c,0xe5);
DEFINE_GUID(DXFILEOBJ_EffectInstance,          0xe331f7e4,0x559 ,0x4cc2,0x8e,0x99,0x1c,0xec,0x16,0x57,0x92,0x8f);
DEFINE_GUID(DXFILEOBJ_AnimTicksPerSecond,      0x9e415a43,0x7ba6,0x4a73,0x87,0x43,0xb7,0x3d,0x47,0xe8,0x84,0x76);

#define XSKINEXP_TEMPLATES \
        "xof 0303txt 0032\
        template XSkinMeshHeader \
        { \
            <3CF169CE-FF7C-44ab-93C0-F78F62D172E2> \
            WORD nMaxSkinWeightsPerVertex; \
            WORD nMaxSkinWeightsPerFace; \
            WORD nBones; \
        } \
        template VertexDuplicationIndices \
        { \
            <B8D65549-D7C9-4995-89CF-53A9A8B031E3> \
            DWORD nIndices; \
            DWORD nOriginalVertices; \
            array DWORD indices[nIndices]; \
        } \
        template FaceAdjacency \
        { \
            <A64C844A-E282-4756-8B80-250CDE04398C> \
            DWORD nIndices; \
            array DWORD indices[nIndices]; \
        } \
        template SkinWeights \
        { \
            <6F0D123B-BAD2-4167-A0D0-80224F25FABB> \
            STRING transformNodeName; \
            DWORD nWeights; \
            array DWORD vertexIndices[nWeights]; \
            array float weights[nWeights]; \
            Matrix4x4 matrixOffset; \
        } \
        template Patch \
        { \
            <A3EB5D44-FC22-429D-9AFB-3221CB9719A6> \
            DWORD nControlIndices; \
            array DWORD controlIndices[nControlIndices]; \
        } \
        template PatchMesh \
        { \
            <D02C95CC-EDBA-4305-9B5D-1820D7704BBF> \
            DWORD nVertices; \
            array Vector vertices[nVertices]; \
            DWORD nPatches; \
            array Patch patches[nPatches]; \
            [ ... ] \
        } \
        template PatchMesh9 \
        { \
            <B9EC94E1-B9A6-4251-BA18-94893F02C0EA> \
            DWORD Type; \
            DWORD Degree; \
            DWORD Basis; \
            DWORD nVertices; \
            array Vector vertices[nVertices]; \
            DWORD nPatches; \
            array Patch patches[nPatches]; \
            [ ... ] \
        } " \
        "template EffectFloats \
        { \
            <F1CFE2B3-0DE3-4e28-AFA1-155A750A282D> \
            DWORD nFloats; \
            array float Floats[nFloats]; \
        } \
        template EffectString \
        { \
            <D55B097E-BDB6-4c52-B03D-6051C89D0E42> \
            STRING Value; \
        } \
        template EffectDWord \
        { \
            <622C0ED0-956E-4da9-908A-2AF94F3CE716> \
            DWORD Value; \
        } " \
        "template EffectParamFloats \
        { \
            <3014B9A0-62F5-478c-9B86-E4AC9F4E418B> \
            STRING ParamName; \
            DWORD nFloats; \
            array float Floats[nFloats]; \
        } " \
        "template EffectParamString \
        { \
            <1DBC4C88-94C1-46ee-9076-2C28818C9481> \
            STRING ParamName; \
            STRING Value; \
        } \
        template EffectParamDWord \
        { \
            <E13963BC-AE51-4c5d-B00F-CFA3A9D97CE5> \
            STRING ParamName; \
            DWORD Value; \
        } \
        template EffectInstance \
        { \
            <E331F7E4-0559-4cc2-8E99-1CEC1657928F> \
            STRING EffectFilename; \
            [ ... ] \
        } " \
        "template AnimTicksPerSecond \
        { \
            <9E415A43-7BA6-4a73-8743-B73D47E88476> \
            DWORD AnimTicksPerSecond; \
        } "

#define XEXTENSIONS_TEMPLATES \
        "xof 0303txt 0032\
        template FVFData \
        { \
            <B6E70A0E-8EF9-4e83-94AD-ECC8B0C04897> \
            DWORD dwFVF; \
            DWORD nDWords; \
            array DWORD data[nDWords]; \
        } \
        template VertexElement \
        { \
            <F752461C-1E23-48f6-B9F8-8350850F336F> \
            DWORD Type; \
            DWORD Method; \
            DWORD Usage; \
            DWORD UsageIndex; \
        } \
        template DeclData \
        { \
            <BF22E553-292C-4781-9FEA-62BD554BDD93> \
            DWORD nElements; \
            array VertexElement Elements[nElements]; \
            DWORD nDWords; \
            array DWORD data[nDWords]; \
        } \
        template PMAttributeRange \
        { \
            <917E0427-C61E-4a14-9C64-AFE65F9E9844> \
            DWORD iFaceOffset; \
            DWORD nFacesMin; \
            DWORD nFacesMax; \
            DWORD iVertexOffset; \
            DWORD nVerticesMin; \
            DWORD nVerticesMax; \
        } \
        template PMVSplitRecord \
        { \
            <574CCC14-F0B3-4333-822D-93E8A8A08E4C> \
            DWORD iFaceCLW; \
            DWORD iVlrOffset; \
            DWORD iCode; \
        } \
        template PMInfo \
        { \
            <B6C3E656-EC8B-4b92-9B62-681659522947> \
            DWORD nAttributes; \
            array PMAttributeRange attributeRanges[nAttributes]; \
            DWORD nMaxValence; \
            DWORD nMinLogicalVertices; \
            DWORD nMaxLogicalVertices; \
            DWORD nVSplits; \
            array PMVSplitRecord splitRecords[nVSplits]; \
            DWORD nAttributeMispredicts; \
            array DWORD attributeMispredicts[nAttributeMispredicts]; \
        } "
        
#endif

