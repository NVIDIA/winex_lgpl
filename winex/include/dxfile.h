#ifndef __WINEX_DXFILE_H
#define __WINEX_DXFILE_H

#include "windef.h"
#include "wingdi.h"
#include "objbase.h"

#ifdef __cplusplus
extern "C" {
#endif


/*****************************************************************************
 * Predeclare Interfaces
 */
DEFINE_GUID(CLSID_CDirectXFile,             0x4516ec43,0x8f20,0x11d0,0x9b,0x6d,0x00,0x00,0xc0,0x78,0x1b,0xc3);
DEFINE_GUID(IID_IDirectXFile,               0x3d82ab40,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(IID_IDirectXFileEnumObject,     0x3d82ab41,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(IID_IDirectXFileSaveObject,     0x3d82ab42,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(IID_IDirectXFileObject,         0x3d82ab43,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(IID_IDirectXFileData,           0x3d82ab44,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(IID_IDirectXFileDataReference,  0x3d82ab45,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(IID_IDirectXFileBinary,         0x3d82ab46,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);
DEFINE_GUID(TID_DXFILEHeader,               0x3d82ab43,0x62da,0x11cf,0xab,0x39,0x0 ,0x20,0xaf,0x71,0xe4,0x33);


/*************************************************************************
 * Defines, types, structures
 */

typedef DWORD DXFILEFORMAT;

#define DXFILEFORMAT_BINARY     0
#define DXFILEFORMAT_TEXT       1
#define DXFILEFORMAT_COMPRESSED 2

typedef DWORD DXFILELOADOPTIONS;

#define DXFILELOAD_FROMFILE         0x00L
#define DXFILELOAD_FROMRESOURCE     0x01L
#define DXFILELOAD_FROMMEMORY       0x02L
#define DXFILELOAD_FROMSTREAM       0x04L
#define DXFILELOAD_FROMURL          0x08L

#ifndef LPCTSTR
 #ifdef UNICODE
  #define LPCTSTR LPCSTR
 #else
  #define LPCTSTR LPCWSTR
 #endif
#endif

typedef struct _DXFILELOADRESOURCE {
    HMODULE hModule;
    LPCTSTR lpName;
    LPCTSTR lpType;
} DXFILELOADRESOURCE, *LPDXFILELOADRESOURCE;

typedef struct _DXFILELOADMEMORY {
    LPVOID lpMemory;
    DWORD dSize;
} DXFILELOADMEMORY, *LPDXFILELOADMEMORY;

#define _FACDD  0x876
#define MAKE_DDHRESULT( code )  MAKE_HRESULT( 1, _FACDD, code )

#define DXFILE_OK   0

#define DXFILEERR_BADOBJECT                 MAKE_DDHRESULT(850)
#define DXFILEERR_BADVALUE                  MAKE_DDHRESULT(851)
#define DXFILEERR_BADTYPE                   MAKE_DDHRESULT(852)
#define DXFILEERR_BADSTREAMHANDLE           MAKE_DDHRESULT(853)
#define DXFILEERR_BADALLOC                  MAKE_DDHRESULT(854)
#define DXFILEERR_NOTFOUND                  MAKE_DDHRESULT(855)
#define DXFILEERR_NOTDONEYET                MAKE_DDHRESULT(856)
#define DXFILEERR_FILENOTFOUND              MAKE_DDHRESULT(857)
#define DXFILEERR_RESOURCENOTFOUND          MAKE_DDHRESULT(858)
#define DXFILEERR_URLNOTFOUND               MAKE_DDHRESULT(859)
#define DXFILEERR_BADRESOURCE               MAKE_DDHRESULT(860)
#define DXFILEERR_BADFILETYPE               MAKE_DDHRESULT(861)
#define DXFILEERR_BADFILEVERSION            MAKE_DDHRESULT(862)
#define DXFILEERR_BADFILEFLOATSIZE          MAKE_DDHRESULT(863)
#define DXFILEERR_BADFILECOMPRESSIONTYPE    MAKE_DDHRESULT(864)
#define DXFILEERR_BADFILE                   MAKE_DDHRESULT(865)
#define DXFILEERR_PARSEERROR                MAKE_DDHRESULT(866)
#define DXFILEERR_NOTEMPLATE                MAKE_DDHRESULT(867)
#define DXFILEERR_BADARRAYSIZE              MAKE_DDHRESULT(868)
#define DXFILEERR_BADDATAREFERENCE          MAKE_DDHRESULT(869)
#define DXFILEERR_INTERNALERROR             MAKE_DDHRESULT(870)
#define DXFILEERR_NOMOREOBJECTS             MAKE_DDHRESULT(871)
#define DXFILEERR_BADINTRINSICS             MAKE_DDHRESULT(872)
#define DXFILEERR_NOMORESTREAMHANDLES       MAKE_DDHRESULT(873)
#define DXFILEERR_NOMOREDATA                MAKE_DDHRESULT(874)
#define DXFILEERR_BADCACHEFILE              MAKE_DDHRESULT(875)
#define DXFILEERR_NOINTERNET                MAKE_DDHRESULT(876)

#undef _FACDD
#undef MAKE_DDHRESULT

/*****************************************************************
 * Interfaces
 */

typedef struct IDirectXFile                  IDirectXFile, *LPDIRECTXFILE, **LPLPDIRECTXFILE;
typedef struct IDirectXFileEnumObject        IDirectXFileEnumObject, *LPDIRECTXFILEENUMOBJECT, **LPLPDIRECTXFILEENUMOBJECT;
typedef struct IDirectXFileSaveObject        IDirectXFileSaveObject, *LPDIRECTXFILESAVEOBJECT, **LPLPDIRECTXFILESAVEOBJECT;
typedef struct IDirectXFileObject            IDirectXFileObject, *LPDIRECTXFILEOBJECT, **LPLPDIRECTXFILEOBJECT;
typedef struct IDirectXFileData              IDirectXFileData, *LPDIRECTXFILEDATA, **LPLPDIRECTXFILEDATA;
typedef struct IDirectXFileDataReference     IDirectXFileDataReference, *LPDIRECTXFILEDATAREFERENCE, **LPLPDIRECTXFILEDATAREFERENCE;
typedef struct IDirectXFileBinary            IDirectXFileBinary, *LPDIRECTXFILEBINARY, **LPLPDIRECTXFILEBINARY;


/*****************************************************
 * IDirectXFile interface
 */
#define ICOM_INTERFACE IDirectXFile
#define IDirectXFile_METHODS \
    ICOM_METHOD3 (HRESULT,CreateEnumObject,LPVOID,pvSource,DXFILELOADOPTIONS,dwLoadOptions,LPDIRECTXFILEENUMOBJECT*,ppEnumObj) \
    ICOM_METHOD3 (HRESULT,CreateSaveObject,LPCSTR,szFileName,DXFILEFORMAT,dwFileFormat,LPDIRECTXFILESAVEOBJECT*,ppSaveObj) \
    ICOM_METHOD2 (HRESULT,RegisterTemplates,LPVOID,pvData,DWORD,cbSize)
#define IDirectXFile_IMETHODS \
    IUnknown_IMETHODS \
    IDirectXFile_METHODS
ICOM_DEFINE(IDirectXFile,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IDirectXFile_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectXFile_AddRef(p) ICOM_CALL(AddRef,p)
#define IDirectXFile_Release(p) ICOM_CALL(Release,p)
/*** IDirectXFile methods ***/
#define IDirectXFile_CreateEnumObject(p,a,b,c) ICOM_CALL3(CreateEnumObject,p,a,b,c)
#define IDirectXFile_CreateSaveObject(p,a,b,c) ICOM_CALL3(CreateSaveObject,p,a,b,c)
#define IDirectXFile_RegisterTemplates(p,a,b) ICOM_CALL2(RegisterTemplates,p,a,b)


/*****************************************************************************
 * IDirectXFileSaveObject interface
 */
#define ICOM_INTERFACE IDirectXFileSaveObject
#define IDirectXFileSaveObject_METHODS \
    ICOM_METHOD2 (HRESULT,SaveTemplates,DWORD,cTemplates,const GUID**,ppguidTemplates) \
    ICOM_METHOD6 (HRESULT,CreateDataObject,REFGUID,rguidTemplate,LPCSTR,szName,const GUID*,pguid,DWORD,cbSize,LPVOID,pvData,LPDIRECTXFILEDATA*,ppDataObj) \
    ICOM_METHOD1 (HRESULT,SaveData,LPDIRECTXFILEDATA,pDataObj)
#define IDirectXFileSaveObject_IMETHODS \
    IUnknown_IMETHODS \
    IDirectXFileSaveObject_METHODS
ICOM_DEFINE(IDirectXFileSaveObject,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IDirectXFileSaveObject_QueryInterface(p,a,b)            ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectXFileSaveObject_AddRef(p)                        ICOM_CALL(AddRef,p)
#define IDirectXFileSaveObject_Release(p)                       ICOM_CALL(Release,p)
/*** IDirectXFileSaveObject methods ***/
#define IDirectXFileSaveObject_SaveTemplates(p,a,b)             ICOM_CALL2(SaveTemplates,p,a,b)
#define IDirectXFileSaveObject_CreateDataObject(p,a,b,c,d,e,f)  ICOM_CALL6(CreateDataObject,p,a,b,c,d,e,f)
#define IDirectXFileSaveObject_SaveData(p,a)                    ICOM_CALL1(SaveData,p,a)


/*****************************************************************************
 * IDirectXFileObject interface
 */
#define ICOM_INTERFACE IDirectXFileObject
#define IDirectXFileObject_METHODS \
    ICOM_METHOD2 (HRESULT,GetName,LPSTR,pstrNameBuf,LPDWORD,pdwBufLen) \
    ICOM_METHOD1 (HRESULT,GetId,LPGUID,pGuid)
#define IDirectXFileObject_IMETHODS \
    IUnknown_IMETHODS \
    IDirectXFileObject_METHODS
ICOM_DEFINE(IDirectXFileObject,IUnknown)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IDirectXFileObject_QueryInterface(p,a,b)            ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectXFileObject_AddRef(p)                        ICOM_CALL(AddRef,p)
#define IDirectXFileObject_Release(p)                       ICOM_CALL(Release,p)
/*** IDirectXFileObject methods ***/
#define IDirectXFileObject_GetName(p,a,b)                   ICOM_CALL2(GetName,p,a,b)
#define IDirectXFileObject_GetId(p,a)                       ICOM_CALL1(GetId,p,a)


/*****************************************************************************
 * IDirectXFileData interface
 */
#define ICOM_INTERFACE IDirectXFileData
#define IDirectXFileData_METHODS \
    ICOM_METHOD3 (HRESULT,GetData,LPCSTR,szMember,DWORD*,pcbSize,void**,ppvData) \
    ICOM_METHOD1 (HRESULT,GetType,const GUID**,ppguid) \
    ICOM_METHOD1 (HRESULT,GetNextObject,LPDIRECTXFILEOBJECT*,ppChildObj) \
    ICOM_METHOD1 (HRESULT,AddDataObject,LPDIRECTXFILEDATA,pDataObj) \
    ICOM_METHOD2 (HRESULT,AddDataReference,LPCSTR,szRef,const GUID*,pguidRef) \
    ICOM_METHOD5 (HRESULT,AddBinaryObject,LPCSTR,szName,const GUID*,pguid,LPCSTR,szMimeType,LPVOID,pvData,DWORD,cbSize)
#define IDirectXFileData_IMETHODS \
    IDirectXFileObject_IMETHODS \
    IDirectXFileData_METHODS
ICOM_DEFINE(IDirectXFileData,IDirectXFileObject)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IDirectXFileData_QueryInterface(p,a,b)          ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectXFileData_AddRef(p)                      ICOM_CALL(AddRef,p)
#define IDirectXFileData_Release(p)                     ICOM_CALL(Release,p)
/*** IDirectXFileObject methods ***/
#define IDirectXFileData_GetName(p,a,b)                 ICOM_CALL2(GetName,p,a,b)
#define IDirectXFileData_GetId(p,a)                     ICOM_CALL1(GetId,p,a)
/*** IDirectXFileData methods ***/
#define IDirectXFileData_GetData(p,a,b,c)               ICOM_CALL3(GetData,p,a,b,c)
#define IDirectXFileData_GetType(p,a)                   ICOM_CALL1(GetType,p,a)
#define IDirectXFileData_GetNextObject(p,a)             ICOM_CALL1(GetNextObject,p,a)
#define IDirectXFileData_AddDataObject(p,a)             ICOM_CALL1(AddDataObject,p,a)
#define IDirectXFileData_AddDataReference(p,a,b)        ICOM_CALL2(AddDataReference,p,a,b)
#define IDirectXFileData_AddBinaryObject(p,a,b,c,d,e)   ICOM_CALL5(AddBinaryObject,p,a,b,c,d,e)


/*****************************************************************************
 * IDirectXFileDataReference interface
 */
#define ICOM_INTERFACE IDirectXFileDataReference
#define IDirectXFileDataReference_METHODS \
    ICOM_METHOD1 (HRESULT,Resolve,LPDIRECTXFILEDATA*,ppDataObj)
#define IDirectXFileDataReference_IMETHODS \
    IDirectXFileObject_IMETHODS \
    IDirectXFileDataReference_METHODS
ICOM_DEFINE(IDirectXFileDataReference,IDirectXFileObject)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IDirectXFileDataReference_QueryInterface(p,a,b)         ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectXFileDataReference_AddRef(p)                     ICOM_CALL(AddRef,p)
#define IDirectXFileDataReference_Release(p)                    ICOM_CALL(Release,p)
/*** IDirectXFileObject methods ***/
#define IDirectXFileDataReference_GetName(p,a,b)                ICOM_CALL2(GetName,p,a,b)
#define IDirectXFileDataReference_GetId(p,a)                    ICOM_CALL1(GetId,p,a)
/*** IDirectXFileDataReference methods ***/
#define IDirectXFileDataReference_Resolve(p,a)                  ICOM_CALL1(Resolve,p,a)


/*****************************************************************************
 * IDirectXFileEnumObject interface
 */
#define ICOM_INTERFACE IDirectXFileEnumObject
#define IDirectXFileEnumObject_METHODS \
    ICOM_METHOD1 (HRESULT,GetNextDataObject, LPDIRECTXFILEOBJECT*,ppFileObject) \
    ICOM_METHOD2 (HRESULT,GetDataObjectById, REFGUID,gudi,LPDIRECTXFILEDATA*,ppFileData) \
    ICOM_METHOD2 (HRESULT,GetDataObjectByName, LPCSTR,Name,LPDIRECTXFILEDATA*,ppFileData)
#define IDirectXFileEnumObject_IMETHODS \
    IUnknown_IMETHODS \
    IDirectXFileEnumObject_METHODS
ICOM_DEFINE(IDirectXFileEnumObject,IUnknown)
#undef ICOM_INTERFACE


/*****************************************************************
 * DXFile functions
 */

STDAPI DirectXFileCreate(LPDIRECTXFILE *lplpDirectXFile);


#ifdef __cplusplus
};
#endif

#endif

