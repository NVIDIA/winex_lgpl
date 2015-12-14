#ifndef __WINEX_D3DX9ANIM_H
#define __WINEX_D3DX9ANIM_H


/****************************************************************************
 * Predeclare interfaces
 */
DEFINE_GUID(IID_ID3DXInterpolator,          0xade2c06d, 0x3747, 0x4b9f, 0xa5, 0x14, 0x34, 0x40, 0xb8, 0x28, 0x49, 0x80);
DEFINE_GUID(IID_ID3DXKeyFrameInterpolator,  0x6caa71f8, 0x972,  0x4cdb, 0xa5, 0x5b, 0x43, 0xb9, 0x68, 0x99, 0x75, 0x15);
DEFINE_GUID(IID_ID3DXAnimationSet,          0x54b569ac, 0xaef,  0x473e, 0x97, 0x4,  0x3f, 0xef, 0x31, 0x7f, 0x64, 0xab);
DEFINE_GUID(IID_ID3DXAnimationController,   0x3a714d34, 0xff61, 0x421e, 0x90, 0x9f, 0x63, 0x9f, 0x38, 0x35, 0x67, 0x8);

typedef struct ID3DXInterpolator ID3DXInterpolator, *LPD3DXINTERPOLATOR;
typedef struct ID3DXKeyFrameInterpolator ID3DXKeyFrameInterpolator, *LPD3DXKEYFRAMEINTERPOLATOR;
typedef struct ID3DXAnimationSet ID3DXAnimationSet, *LPD3DXANIMATIONSET;
typedef struct ID3DXAnimationController ID3DXAnimationController, *LPD3DXANIMATIONCONTROLLER;

typedef struct ID3DXAllocateHierarchy ID3DXAllocateHierarchy, *LPD3DXALLOCATEHIERARCHY;
typedef struct ID3DXLoadUserData ID3DXLoadUserData, *LPD3DXLOADUSERDATA;
typedef struct ID3DXSaveUserData ID3DXSaveUserData, *LPD3DXSAVEUSERDATA;


/************************************************************************
 * Types, structures, and enums
 */
typedef enum _D3DXMESHDATATYPE {
    D3DXMESHTYPE_MESH      = 0x001,
    D3DXMESHTYPE_PMESH     = 0x002,
    D3DXMESHTYPE_PATCHMESH = 0x003,

    D3DXMESHTYPE_FORCE_DWORD    = 0x7fffffff,
} D3DXMESHDATATYPE;

typedef struct _D3DXMESHDATA
{
    D3DXMESHDATATYPE Type;

    union
    {
        LPD3DXMESH              pMesh;
        LPD3DXPMESH             pPMesh;
        LPD3DXPATCHMESH         pPatchMesh;
    };
} D3DXMESHDATA, *LPD3DXMESHDATA;

typedef struct _D3DXMESHCONTAINER
{
    LPSTR                   Name;

    D3DXMESHDATA            MeshData;

    LPD3DXMATERIAL          pMaterials;
    LPD3DXEFFECTINSTANCE    pEffects;
    DWORD                   NumMaterials;
    DWORD                  *pAdjacency;

    LPD3DXSKININFO          pSkinInfo;

    struct _D3DXMESHCONTAINER *pNextMeshContainer;
} D3DXMESHCONTAINER, *LPD3DXMESHCONTAINER;

typedef struct _D3DXFRAME
{
    LPSTR                   Name;
    D3DXMATRIX              TransformationMatrix;

    LPD3DXMESHCONTAINER     pMeshContainer;

    struct _D3DXFRAME       *pFrameSibling;
    struct _D3DXFRAME       *pFrameFirstChild;
} D3DXFRAME, *LPD3DXFRAME;

typedef struct _D3DXKEY_VECTOR3
{
    FLOAT Time;
    D3DXVECTOR3 Value;
} D3DXKEY_VECTOR3, *LPD3DXKEY_VECTOR3;

typedef struct _D3DXKEY_QUATERNION
{
    FLOAT Time;
    D3DXQUATERNION Value;
} D3DXKEY_QUATERNION, *LPD3DXKEY_QUATERNION;

typedef struct _D3DXTRACK_DESC
{
    DWORD Flags;
    FLOAT Weight;
    FLOAT Speed;
    BOOL  Enable;
    DOUBLE AnimTime;
} D3DXTRACK_DESC, *LPD3DXTRACK_DESC;

typedef enum _D3DXTRACKFLAG {
    D3DXTF_LOWPRIORITY            = 0x000,
    D3DXTF_HIGHPRIORITY           = 0x001,

    D3DXTF_FORCE_DWORD    = 0x7fffffff,
} D3DXTRACKFLAG;

typedef enum _D3DXTRANSITIONTYPE {
    D3DXTRANSITION_LINEAR            = 0x000,
    D3DXTRANSITION_EASEINEASEOUT     = 0x001,

    D3DXTRANSITION_FORCE_DWORD    = 0x7fffffff,
} D3DXTRANSITIONTYPE;


/***********************************************************************
 * Interfaces
 */

/*****************************************************************************
 * ID3DXAllocateHierarchy interface
 */
#define ICOM_INTERFACE ID3DXAllocateHierarchy
#define ID3DXAllocateHierarchy_METHODS \
    ICOM_METHOD2 (HRESULT,CreateFrame,LPCSTR,Name,LPD3DXFRAME*,ppNewFrame) \
    ICOM_METHOD8 (HRESULT,CreateMeshContainer,LPCSTR,Name,LPD3DXMESHDATA,pMeshData,LPD3DXMATERIAL,pMaterials,LPD3DXEFFECTINSTANCE,pEffectInstances,DWORD,NumMaterials,DWORD*,pAdjacency,LPD3DXSKININFO,pSkinInfo,LPD3DXMESHCONTAINER*,ppNewMeshContainer) \
    ICOM_METHOD1 (HRESULT,DestroyFrame,LPD3DXFRAME,pFrameToFree) \
    ICOM_METHOD1 (HRESULT,DestroyMeshContainter,LPD3DXMESHCONTAINER,pMeshContainerToFree)
#define ID3DXAllocateHierarchy_IMETHODS \
    ID3DXAllocateHierarchy_METHODS
ICOM_DEFINE1(ID3DXAllocateHierarchy)
#undef ICOM_INTERFACE

/*** ID3DXAllocateHierarchy methods ***/
#define ID3DXAllocateHierarchy_CreateFrame(p,a,b) ICOM_CALL2(CreateFrame,p,a,b)
#define ID3DXAllocateHierarchy_CreateMeshContainer(p,a,b,c,d,e,f,g,h) ICOM_CALL8(CreateMeshContainer,p,a,b,c,d,e,f,g,h)
#define ID3DXAllocateHierarchy_DestroyFrame(p,a) ICOM_CALL1(DestroyFrame,p,a)
#define ID3DXAllocateHierarchy_DestroyMeshContainter(p,a) ICOM_CALL1(DestroyMeshContainter,p,a)


/*****************************************************************************
 * ID3DXLoadUserData interface
 */
#define ICOM_INTERFACE ID3DXLoadUserData
#define ID3DXLoadUserData_METHODS \
    ICOM_METHOD1 (HRESULT,LoadTopLevelData,LPDIRECTXFILEDATA,pXofChildData) \
    ICOM_METHOD2 (HRESULT,LoadFrameChildData,LPD3DXFRAME,pFrame,LPDIRECTXFILEDATA,pXofChildData) \
    ICOM_METHOD2 (HRESULT,LoadMeshChildData,LPD3DXMESHCONTAINER,pMeshContainer,LPDIRECTXFILEDATA,pXofChildData)
#define ID3DXLoadUserData_IMETHODS \
    ID3DXLoadUserData_METHODS
ICOM_DEFINE1(ID3DXLoadUserData)
#undef ICOM_INTERFACE

/*** ID3DXLoadUserData methods ***/
#define ID3DXLoadUserData_LoadTopLevelData(p,a) ICOM_CALL1(LoadTopLevelData,p,a)
#define ID3DXLoadUserData_LoadFrameChildData(p,a,b) ICOM_CALL2(LoadFrameChildData,p,a,b)
#define ID3DXLoadUserData_LoadMeshChildData(p,a,b) ICOM_CALL2(LoadMeshChildData,p,a,b)


/*****************************************************************************
 * ID3DXSaveUserData interface
 */
#define ICOM_INTERFACE ID3DXSaveUserData
#define ID3DXSaveUserData_METHODS \
    ICOM_METHOD3 (HRESULT,AddFrameChildData,LPD3DXFRAME,pFrame,LPDIRECTXFILESAVEOBJECT,pXofSave,LPDIRECTXFILEDATA,pXofFrameData) \
    ICOM_METHOD3 (HRESULT,AddMeshChildData,LPD3DXMESHCONTAINER,pMeshContainer,LPDIRECTXFILESAVEOBJECT,pXofSave,LPDIRECTXFILEDATA,pXofMeshData) \
    ICOM_METHOD1 (HRESULT,AddTopLevelDataObjectsPre,LPDIRECTXFILESAVEOBJECT,pXofSave) \
    ICOM_METHOD1 (HRESULT,AddTopLevelDataObjectsPost,LPDIRECTXFILESAVEOBJECT,pXofSave) \
    ICOM_METHOD1 (HRESULT,RegisterTemplates,LPDIRECTXFILE,pXFileApi) \
    ICOM_METHOD1 (HRESULT,SaveTemplates,LPDIRECTXFILESAVEOBJECT,pXofSave)
#define ID3DXSaveUserData_IMETHODS \
    ID3DXSaveUserData_METHODS
ICOM_DEFINE1(ID3DXSaveUserData)
#undef ICOM_INTERFACE

/*** ID3DXSaveUserData methods ***/
#define ID3DXSaveUserData_AddFrameChildData(p,a,b,c) ICOM_CALL3(AddFrameChildData,p,a,b,c)
#define ID3DXSaveUserData_AddMeshChildData(p,a,b,c) ICOM_CALL3(AddMeshChildData,p,a,b,c)
#define ID3DXSaveUserData_AddTopLevelDataObjectsPre(p,a) ICOM_CALL1(AddTopLevelDataObjectsPre,p,a)
#define ID3DXSaveUserData_AddTopLevelDataObjectsPost(p,a) ICOM_CALL1(AddTopLevelDataObjectsPost,p,a)
#define ID3DXSaveUserData_RegisterTemplates(p,a) ICOM_CALL1(RegisterTemplates,p,a)
#define ID3DXSaveUserData_SaveTemplates(p,a) ICOM_CALL1(SaveTemplates,p,a)


/*****************************************************************************
 * ID3DXKeyFrameInterpolator interface
 */
#define ICOM_INTERFACE ID3DXKeyFrameInterpolator
#define ID3DXKeyFrameInterpolator_METHODS \
    ICOM_METHOD  (LPCSTR,GetName) \
    ICOM_METHOD  (DOUBLE,GetPeriod) \
    ICOM_METHOD4 (HRESULT,GetSRT,DOUBLE,Time,D3DXVECTOR3*,pScale,D3DXQUATERNION*,pRotate,D3DXVECTOR3*,pTranslate) \
    ICOM_METHOD3 (HRESULT,GetLastSRT,D3DXVECTOR3*,pScale,D3DXQUATERNION*,pRotate,D3DXVECTOR3*,pTranslate) \
    ICOM_METHOD  (UINT,GetNumScaleKeys) \
    ICOM_METHOD1 (HRESULT,GetScaleKeys,LPD3DXKEY_VECTOR3,pKeys) \
    ICOM_METHOD  (UINT,GetNumRotationKeys) \
    ICOM_METHOD1 (HRESULT,GetRotationKeys,LPD3DXKEY_QUATERNION,pKeys) \
    ICOM_METHOD  (UINT,GetNumTranslationKeys) \
    ICOM_METHOD1 (HRESULT,GetTranslationKeys,LPD3DXKEY_VECTOR3,pKeys) \
    ICOM_METHOD  (DOUBLE,GetSourceTicksPerSecond)
#define ID3DXKeyFrameInterpolator_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXKeyFrameInterpolator_METHODS
ICOM_DEFINE(ID3DXKeyFrameInterpolator,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXKeyFrameInterpolator_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXKeyFrameInterpolator_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXKeyFrameInterpolator_Release(p) ICOM_CALL(Release,p)
/*** ID3DXKeyFrameInterpolator methods ***/
#define ID3DXKeyFrameInterpolator_GetName(p) ICOM_CALL(GetName,p)
#define ID3DXKeyFrameInterpolator_GetPeriod(p) ICOM_CALL(GetPeriod,p)
#define ID3DXKeyFrameInterpolator_GetSRT(p,a,b,c,d) ICOM_CALL4(GetSRT,p,a,b,c,d)
#define ID3DXKeyFrameInterpolator_GetLastSRT(p,a,b,c) ICOM_CALL3(GetLastSRT,p,a,b,c)
#define ID3DXKeyFrameInterpolator_GetNumScaleKeys(p) ICOM_CALL(GetNumScaleKeys,p)
#define ID3DXKeyFrameInterpolator_GetScaleKeys(p,a) ICOM_CALL1(GetScaleKeys,p,a)
#define ID3DXKeyFrameInterpolator_GetNumRotationKeys(p) ICOM_CALL(GetNumRotationKeys,p)
#define ID3DXKeyFrameInterpolator_GetRotationKeys(p,a) ICOM_CALL1(GetRotationKeys,p,a)
#define ID3DXKeyFrameInterpolator_GetNumTranslationKeys(p) ICOM_CALL(GetNumTranslationKeys,p)
#define ID3DXKeyFrameInterpolator_GetTranslationKeys(p,a) ICOM_CALL1(GetTranslationKeys,p,a)
#define ID3DXKeyFrameInterpolator_GetSourceTicksPerSecond(p) ICOM_CALL(GetSourceTicksPerSecond,p)


/*****************************************************************************
 * ID3DXAnimationSet interface
 */
#define ICOM_INTERFACE ID3DXAnimationSet
#define ID3DXAnimationSet_METHODS \
    ICOM_METHOD  (LPCSTR,GetName) \
    ICOM_METHOD  (DOUBLE,GetPeriod) \
    ICOM_METHOD  (UINT,GetNumInterpolators) \
    ICOM_METHOD2 (HRESULT,GetInterpolatorByIndex,UINT,Index,LPD3DXINTERPOLATOR*,ppInterpolator) \
    ICOM_METHOD2 (HRESULT,GetInterpolatorByName,LPCSTR,pName,LPD3DXINTERPOLATOR*,ppInterpolator)
#define ID3DXAnimationSet_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXAnimationSet_METHODS
ICOM_DEFINE(ID3DXAnimationSet,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXAnimationSet_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXAnimationSet_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXAnimationSet_Release(p) ICOM_CALL(Release,p)
/*** ID3DXAnimationSet methods ***/
#define ID3DXAnimationSet_GetName(p) ICOM_CALL(GetName,p)
#define ID3DXAnimationSet_GetPeriod(p) ICOM_CALL(GetPeriod,p)
#define ID3DXAnimationSet_GetNumInterpolators(p) ICOM_CALL(GetNumInterpolators,p)
#define ID3DXAnimationSet_GetInterpolatorByIndex(p,a,b) ICOM_CALL2(GetInterpolatorByIndex,p,a,b)
#define ID3DXAnimationSet_GetInterpolatorByName(p,a,b) ICOM_CALL2(GetInterpolatorByName,p,a,b)


/*****************************************************************************
 * ID3DXAnimationController interface
 */
#define ICOM_INTERFACE ID3DXAnimationController
#define ID3DXAnimationController_METHODS \
    ICOM_METHOD2 (HRESULT,RegisterMatrix,LPCSTR,Name,D3DXMATRIX*,pMatrix) \
    ICOM_METHOD  (UINT,GetNumAnimationSets) \
    ICOM_METHOD2 (HRESULT,GetAnimationSet,DWORD,iAnimationSet,LPD3DXANIMATIONSET*,ppAnimSet) \
    ICOM_METHOD1 (HRESULT,RegisterAnimationSet,LPD3DXANIMATIONSET,pAnimSet) \
    ICOM_METHOD1 (HRESULT,UnregisterAnimationSet,LPD3DXANIMATIONSET,pAnimSet) \
    ICOM_METHOD  (UINT,GetMaxNumTracks) \
    ICOM_METHOD2 (HRESULT,GetTrackDesc,DWORD,Track,D3DXTRACK_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,SetTrackDesc,DWORD,Track,D3DXTRACK_DESC*,pDesc) \
    ICOM_METHOD2 (HRESULT,GetTrackAnimationSet,DWORD,Track,LPD3DXANIMATIONSET*,ppAnimSet) \
    ICOM_METHOD2 (HRESULT,SetTrackAnimationSet,DWORD,Track,LPD3DXANIMATIONSET,pAnimSet) \
    ICOM_METHOD2 (HRESULT,SetTrackSpeed,DWORD,Track,FLOAT,Speed) \
    ICOM_METHOD2 (HRESULT,SetTrackWeight,DWORD,Track,FLOAT,Weight) \
    ICOM_METHOD2 (HRESULT,SetTrackAnimTime,DWORD,Track,DOUBLE,AnimTime) \
    ICOM_METHOD2 (HRESULT,SetTrackEnable,DWORD,Track,BOOL,Enable) \
    ICOM_METHOD  (DOUBLE,GetTime) \
    ICOM_METHOD1 (HRESULT,SetTime,DOUBLE,Time) \
    ICOM_METHOD5 (HRESULT,CloneAnimationController,UINT,MaxNumMatrices,UINT,MaxNumAnimationSets,UINT,MaxNumTracks,UINT,MaxNumEvents,LPD3DXANIMATIONCONTROLLER*,ppAnimController) \
    ICOM_METHOD  (UINT,GetMaxNumMatrices) \
    ICOM_METHOD  (UINT,GetMaxNumEvents) \
    ICOM_METHOD  (UINT,GetMaxNumAnimationSets) \
    ICOM_METHOD5 (HRESULT,KeyTrackSpeed,DWORD,Track,FLOAT,NewSpeed,DOUBLE,StartTime,DOUBLE,Duration,DWORD,Method) \
    ICOM_METHOD5 (HRESULT,KeyTrackWeight,DWORD,Track,FLOAT,NewWeight,DOUBLE,StartTime,DOUBLE,Duration,DWORD,Method) \
    ICOM_METHOD3 (HRESULT,KeyTrackAnimTime,DWORD,Track,DOUBLE,NewAnimTime,DOUBLE,StartTime) \
    ICOM_METHOD3 (HRESULT,KeyTrackEnable,DWORD,Track,BOOL,NewEnable,DOUBLE,StartTime) \
    ICOM_METHOD  (FLOAT,GetPriorityBlend) \
    ICOM_METHOD1 (HRESULT,SetPriorityBlend,FLOAT,BlendWeight) \
    ICOM_METHOD4 (HRESULT,KeyPriorityBlend,FLOAT,NewBlendWeight,DOUBLE,StartTime,DOUBLE,Duration,DWORD,Method)
#define ID3DXAnimationController_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXAnimationController_METHODS
ICOM_DEFINE(ID3DXAnimationController,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define ID3DXAnimationController_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define ID3DXAnimationController_AddRef(p) ICOM_CALL(AddRef,p)
#define ID3DXAnimationController_Release(p) ICOM_CALL(Release,p)
/*** ID3DXAnimationController methods ***/
#define ID3DXAnimationController_RegisterMatrix(p,a,b) ICOM_CALL2(RegisterMatrix,p,a,b)
#define ID3DXAnimationController_GetNumAnimationSets(p) ICOM_CALL(GetNumAnimationSets,p)
#define ID3DXAnimationController_GetAnimationSet(p,a,b) ICOM_CALL2(GetAnimationSet,p,a,b)
#define ID3DXAnimationController_RegisterAnimationSet(p,a) ICOM_CALL1(RegisterAnimationSet,p,a)
#define ID3DXAnimationController_UnregisterAnimationSet(p,a) ICOM_CALL1(UnregisterAnimationSet,p,a)
#define ID3DXAnimationController_GetMaxNumTracks(p) ICOM_CALL(GetMaxNumTracks,p)
#define ID3DXAnimationController_GetTrackDesc(p,a,b) ICOM_CALL2(GetTrackDesc,p,a,b)
#define ID3DXAnimationController_SetTrackDesc(p,a,b) ICOM_CALL2(SetTrackDesc,p,a,b)
#define ID3DXAnimationController_GetTrackAnimationSet(p,a,b) ICOM_CALL2(GetTrackAnimationSet,p,a,b)
#define ID3DXAnimationController_SetTrackAnimationSet(p,a,b) ICOM_CALL2(SetTrackAnimationSet,p,a,b)
#define ID3DXAnimationController_SetTrackSpeed(p,a,b) ICOM_CALL2(SetTrackSpeed,p,a,b)
#define ID3DXAnimationController_SetTrackWeight(p,a,b) ICOM_CALL2(SetTrackWeight,p,a,b)
#define ID3DXAnimationController_SetTrackAnimTime(p,a,b) ICOM_CALL2(SetTrackAnimTime,p,a,b)
#define ID3DXAnimationController_SetTrackEnable(p,a,b) ICOM_CALL2(SetTrackEnable,p,a,b)
#define ID3DXAnimationController_GetTime(p) ICOM_CALL(GetTime,p)
#define ID3DXAnimationController_SetTime(p,a) ICOM_CALL1(SetTime,p,a)
#define ID3DXAnimationController_CloneAnimationController(p,a,b,c,d,e) ICOM_CALL5(CloneAnimationController,p,a,b,c,d,e)
#define ID3DXAnimationController_GetMaxNumMatrices(p) ICOM_CALL(GetMaxNumMatrices,p)
#define ID3DXAnimationController_GetMaxNumEvents(p) ICOM_CALL(GetMaxNumEvents,p)
#define ID3DXAnimationController_GetMaxNumAnimationSets(p) ICOM_CALL(GetMaxNumAnimationSets,p)
#define ID3DXAnimationController_KeyTrackSpeed(p,a,b,c,d,e) ICOM_CALL5(KeyTrackSpeed,p,a,b,c,d,e)
#define ID3DXAnimationController_KeyTrackWeight(p,a,b,c,d,e) ICOM_CALL5(KeyTrackWeight,p,a,b,c,d,e)
#define ID3DXAnimationController_KeyTrackAnimTime(p,a,b,c) ICOM_CALL3(KeyTrackAnimTime,p,a,b,c)
#define ID3DXAnimationController_KeyTrackEnable(p,a,b,c) ICOM_CALL3(KeyTrackEnable,p,a,b,c)
#define ID3DXAnimationController_GetPriorityBlend(p) ICOM_CALL(GetPriorityBlend,p)
#define ID3DXAnimationController_SetPriorityBlend(p,a) ICOM_CALL1(SetPriorityBlend,p,a)
#define ID3DXAnimationController_KeyPriorityBlend(p,a,b,c,d) ICOM_CALL4(KeyPriorityBlend,p,a,b,c,d)


/***********************************************************************
 * D3DX Animation functions
 */

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI 
    D3DXLoadMeshHierarchyFromXA(
        LPCSTR Filename,
        DWORD MeshOptions,
        LPDIRECT3DDEVICE9 pD3DDevice,
        LPD3DXALLOCATEHIERARCHY pAlloc,
        LPD3DXLOADUSERDATA pUserDataLoader, 
        LPD3DXFRAME *ppFrameHierarchy,
        LPD3DXANIMATIONCONTROLLER *ppAnimController
);

HRESULT WINAPI 
    D3DXLoadMeshHierarchyFromXW(
        LPCWSTR Filename,
        DWORD MeshOptions,
        LPDIRECT3DDEVICE9 pD3DDevice,
        LPD3DXALLOCATEHIERARCHY pAlloc,
        LPD3DXLOADUSERDATA pUserDataLoader, 
        LPD3DXFRAME *ppFrameHierarchy,
        LPD3DXANIMATIONCONTROLLER *ppAnimController
);

#ifdef UNICODE
#define D3DXLoadMeshHierarchyFromX D3DXLoadMeshHierarchyFromXW
#else
#define D3DXLoadMeshHierarchyFromX D3DXLoadMeshHierarchyFromXA
#endif

HRESULT WINAPI 
    D3DXLoadMeshHierarchyFromXInMemory(
        LPCVOID Memory,
        DWORD SizeOfMemory,
        DWORD MeshOptions,
        LPDIRECT3DDEVICE9 pD3DDevice,
        LPD3DXALLOCATEHIERARCHY pAlloc,
        LPD3DXLOADUSERDATA pUserDataLoader, 
        LPD3DXFRAME *ppFrameHierarchy,
        LPD3DXANIMATIONCONTROLLER *ppAnimController
);

HRESULT WINAPI 
    D3DXSaveMeshHierarchyToFileA(
        LPCSTR Filename,
        DWORD XFormat,
        CONST D3DXFRAME *pFrameRoot, 
        LPD3DXANIMATIONCONTROLLER pAnimMixer,
        LPD3DXSAVEUSERDATA pUserDataSaver
);

HRESULT WINAPI 
    D3DXSaveMeshHierarchyToFileW(
        LPCWSTR Filename,
        DWORD XFormat,
        CONST D3DXFRAME *pFrameRoot, 
        LPD3DXANIMATIONCONTROLLER pAnimMixer,
        LPD3DXSAVEUSERDATA pUserDataSaver
);

#ifdef UNICODE
#define D3DXSaveMeshHierarchyToFile D3DXSaveMeshHierarchyToFileW
#else
#define D3DXSaveMeshHierarchyToFile D3DXSaveMeshHierarchyToFileA
#endif

HRESULT WINAPI
    D3DXFrameDestroy(
        LPD3DXFRAME pFrameRoot,
        LPD3DXALLOCATEHIERARCHY pAlloc
);

HRESULT WINAPI 
    D3DXFrameAppendChild(
        LPD3DXFRAME pFrameParent,
        CONST D3DXFRAME *pFrameChild
);

LPD3DXFRAME WINAPI 
    D3DXFrameFind(
        CONST D3DXFRAME *pFrameRoot,
        LPCSTR Name
);

HRESULT WINAPI
    D3DXFrameRegisterNamedMatrices(
        LPD3DXFRAME pFrameRoot,
        LPD3DXANIMATIONCONTROLLER pAnimMixer
);

UINT WINAPI
    D3DXFrameNumNamedMatrices(
        CONST D3DXFRAME *pFrameRoot
);

HRESULT WINAPI
    D3DXFrameCalculateBoundingSphere(
        CONST D3DXFRAME *pFrameRoot, 
        LPD3DXVECTOR3 pObjectCenter, 
        FLOAT *pObjectRadius
);

HRESULT WINAPI
D3DXCreateKeyFrameInterpolator(LPCSTR Name, 
                            LPD3DXKEY_VECTOR3    ScaleKeys,     UINT NumScaleKeys,
                            LPD3DXKEY_QUATERNION RotationKeys,  UINT NumRotationKeys,
                            LPD3DXKEY_VECTOR3    TranslateKeys, UINT NumTranslateKeys,
                            DOUBLE ScaleInputTimeBy, LPD3DXKEYFRAMEINTERPOLATOR *ppNewInterpolator);

HRESULT WINAPI
D3DXCreateAnimationSet(LPCSTR Name, 
                            LPD3DXINTERPOLATOR *ppInterpolators, UINT NumInterpolators,
                            LPD3DXANIMATIONSET *ppAnimSet);

HRESULT WINAPI
D3DXCreateAnimationController(UINT MaxNumMatrices, UINT MaxNumAnimationSets, UINT MaxNumTracks, UINT MaxNumEvents,
                                 LPD3DXANIMATIONCONTROLLER *ppAnimController);

#ifdef __cplusplus
}
#endif

#endif /* __WINEX_D3DX9ANIM_H */

