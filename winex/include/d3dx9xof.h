#include "d3dx9.h"

#ifndef __WINEX_D3DX9XOF_H
#define __WINEX_D3DX9XOF_H

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------*/
/* interface pointers */
/*--------------------------------------------------*/
typedef struct ID3DXFile				ID3DXFile, *LPD3DXFILE;
typedef struct ID3DXFileData			ID3DXFileData, *LPD3DXFILEDATA;
typedef struct ID3DXFileEnumObject		ID3DXFileEnumObject;
typedef struct ID3DXFileSaveObject		ID3DXFileSaveObject;

/*--------------------------------------------------*/
/* params */
/*--------------------------------------------------*/
typedef DWORD D3DXF_FILELOADOPTIONS;

#define D3DXF_FILELOAD_FROMFILE     0x00L
#define D3DXF_FILELOAD_FROMWFILE    0x01L
#define D3DXF_FILELOAD_FROMRESOURCE 0x02L
#define D3DXF_FILELOAD_FROMMEMORY   0x03L

typedef DWORD D3DXF_FILESAVEOPTIONS;

#define D3DXF_FILESAVE_TOFILE     0x00L
#define D3DXF_FILESAVE_TOWFILE    0x01L

typedef DWORD D3DXF_FILEFORMAT;

#define D3DXF_FILEFORMAT_BINARY          0
#define D3DXF_FILEFORMAT_TEXT            1
#define D3DXF_FILEFORMAT_COMPRESSED      2

/*--------------------------------------------------*/
/* xof token defs */
/*--------------------------------------------------*/

#define TOKEN_NAME         1
#define TOKEN_STRING       2
#define TOKEN_INTEGER      3
#define TOKEN_GUID         5
#define TOKEN_INTEGER_LIST 6
#define TOKEN_FLOAT_LIST   7

#define TOKEN_OBRACE      10
#define TOKEN_CBRACE      11
#define TOKEN_OPAREN      12
#define TOKEN_CPAREN      13
#define TOKEN_OBRACKET    14
#define TOKEN_CBRACKET    15
#define TOKEN_OANGLE      16
#define TOKEN_CANGLE      17
#define TOKEN_DOT         18
#define TOKEN_COMMA       19
#define TOKEN_SEMICOLON   20
#define TOKEN_TEMPLATE    31
#define TOKEN_WORD        40
#define TOKEN_DWORD       41
#define TOKEN_FLOAT       42
#define TOKEN_DOUBLE      43
#define TOKEN_CHAR        44
#define TOKEN_UCHAR       45
#define TOKEN_SWORD       46
#define TOKEN_SDWORD      47
#define TOKEN_VOID        48
#define TOKEN_LPSTR       49
#define TOKEN_UNICODE     50
#define TOKEN_CSTRING     51
#define TOKEN_ARRAY       52

/*--------------------------------------------------*/
/* DirectX File errors */
/*--------------------------------------------------*/

#define _FACD3DXF 0x876

#define D3DXFERR_BADOBJECT              MAKE_HRESULT( 1, _FACD3DXF, 900 )
#define D3DXFERR_BADVALUE               MAKE_HRESULT( 1, _FACD3DXF, 901 )
#define D3DXFERR_BADTYPE                MAKE_HRESULT( 1, _FACD3DXF, 902 )
#define D3DXFERR_NOTFOUND               MAKE_HRESULT( 1, _FACD3DXF, 903 )
#define D3DXFERR_NOTDONEYET             MAKE_HRESULT( 1, _FACD3DXF, 904 )
#define D3DXFERR_FILENOTFOUND           MAKE_HRESULT( 1, _FACD3DXF, 905 )
#define D3DXFERR_RESOURCENOTFOUND       MAKE_HRESULT( 1, _FACD3DXF, 906 )
#define D3DXFERR_BADRESOURCE            MAKE_HRESULT( 1, _FACD3DXF, 907 )
#define D3DXFERR_BADFILETYPE            MAKE_HRESULT( 1, _FACD3DXF, 908 )
#define D3DXFERR_BADFILEVERSION         MAKE_HRESULT( 1, _FACD3DXF, 909 )
#define D3DXFERR_BADFILEFLOATSIZE       MAKE_HRESULT( 1, _FACD3DXF, 910 )
#define D3DXFERR_BADFILE                MAKE_HRESULT( 1, _FACD3DXF, 911 )
#define D3DXFERR_PARSEERROR             MAKE_HRESULT( 1, _FACD3DXF, 912 )
#define D3DXFERR_BADARRAYSIZE           MAKE_HRESULT( 1, _FACD3DXF, 913 )
#define D3DXFERR_BADDATAREFERENCE       MAKE_HRESULT( 1, _FACD3DXF, 914 )
#define D3DXFERR_NOMOREOBJECTS          MAKE_HRESULT( 1, _FACD3DXF, 915 )
#define D3DXFERR_NOMOREDATA             MAKE_HRESULT( 1, _FACD3DXF, 916 )
#define D3DXFERR_BADCACHEFILE           MAKE_HRESULT( 1, _FACD3DXF, 917 )

/*--------------------------------------------------*/
/* GUID's */
/*--------------------------------------------------*/

DEFINE_GUID( IID_ID3DXFile,				0xcef08cf9, 0x7b4f, 0x4429, 0x96, 0x24, 0x2a, 0x69, 0x0a, 0x93, 0x32, 0x01 );
DEFINE_GUID( IID_ID3DXFileSaveObject,	0xcef08cfa, 0x7b4f, 0x4429, 0x96, 0x24, 0x2a, 0x69, 0x0a, 0x93, 0x32, 0x01 );
DEFINE_GUID( IID_ID3DXFileSaveData,		0xcef08cfb, 0x7b4f, 0x4429, 0x96, 0x24, 0x2a, 0x69, 0x0a, 0x93, 0x32, 0x01 );
DEFINE_GUID( IID_ID3DXFileEnumObject,	0xcef08cfc, 0x7b4f, 0x4429, 0x96, 0x24, 0x2a, 0x69, 0x0a, 0x93, 0x32, 0x01 );
DEFINE_GUID( IID_ID3DXFileData,			0xcef08cfd, 0x7b4f, 0x4429, 0x96, 0x24, 0x2a, 0x69, 0x0a, 0x93, 0x32, 0x01 );

/*--------------------------------------------------*/
/******** Interfaces *******/
/*--------------------------------------------------*/

typedef struct
{
    LPCVOID		lpMemory;
    size_t		dSize;
} D3DXF_FILELOADMEMORY;

/*--------------------------------------------------*/
/* ID3DXFileData */
/*--------------------------------------------------*/
#define ICOM_INTERFACE ID3DXFileData
#define ID3DXFileData_METHODS \
    ICOM_METHOD1(HRESULT,GetEnum,          ID3DXFileEnumObject**,ppEnumObj) \
	ICOM_METHOD2(HRESULT,GetName,          LPSTR,name,size_t*,size ) \
	ICOM_METHOD1(HRESULT,GetId,            LPGUID,id ) \
	ICOM_METHOD2(HRESULT,Lock,             size_t*,size, LPCVOID*, ptr) \
	ICOM_METHOD (HRESULT,Unlock) \
	ICOM_METHOD1(HRESULT,GetType,          GUID*,type ) \
	ICOM_METHOD (BOOL, IsReference) \
	ICOM_METHOD1(HRESULT,GetChildren,      size_t*,child ) \
	ICOM_METHOD2(HRESULT,GetChild,         size_t,id, ID3DXFileData**,data )
#define ID3DXFileData_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXFileData_METHODS
ICOM_DEFINE(ID3DXFileData, IUnknown)
#undef ICOM_INTERFACE

#define ID3DXFileData_Release(p)			ICOM_CALL(Release,p)
#define ID3DXFileData_GetEnum(p,a)			ICOM_CALL1(GetEnum,p,a)
#define ID3DXFileData_GetName(p,a,b)		ICOM_CALL2(GetName,p,a,b)
#define ID3DXFileData_GetId(p,a)			ICOM_CALL1(GetId,p,a) 
#define ID3DXFileData_Lock(p,a,b)			ICOM_CALL2(Lock,p,a,b)
#define ID3DXFileData_Unlock(p)				ICOM_CALL (Unlock,p) 
#define ID3DXFileData_GetType(p,a)			ICOM_CALL1(GetType,p,a)
#define ID3DXFileData_IsReference(p)		ICOM_CALL (IsReference,p)
#define ID3DXFileData_GetChildren(p,a)		ICOM_CALL1(GetChildren,p,a)
#define ID3DXFileData_GetChild(p,a,b)		ICOM_CALL2(GetChild,p,a,b) 

/*--------------------------------------------------*/
/* ID3DXFileEnumObject */
/*--------------------------------------------------*/
#define ICOM_INTERFACE ID3DXFileEnumObject
#define ID3DXFileEnumObject_METHODS \
    ICOM_METHOD1(HRESULT,GetFile,                   ID3DXFile**,ppFile) \
    ICOM_METHOD1(HRESULT,GetChildren,               size_t*, size ) \
    ICOM_METHOD2(HRESULT,GetChild,                  size_t, size, ID3DXFileData**, data ) \
    ICOM_METHOD2(HRESULT,GetDataObjectById,         REFGUID, id, ID3DXFileData**, data ) \
	ICOM_METHOD2(HRESULT,GetDataObjectByName,        LPCSTR, name, ID3DXFileData**, data )
#define ID3DXFileEnumObject_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXFileEnumObject_METHODS
ICOM_DEFINE(ID3DXFileEnumObject, IUnknown)
#undef ICOM_INTERFACE

#define ID3DXFileEnumObject_GetFile(p,a,b)				ICOM_CALL2(GetFile,p,a,b)
#define ID3DXFileEnumObject_GetChildren(p,a)			ICOM_CALL1(GetChildren,p,a)
#define ID3DXFileEnumObject_GetChild(p,a,b)				ICOM_CALL2(GetChild,p,a,b)
#define ID3DXFileEnumObject_GetDataObjectById(p,a,b)    ICOM_CALL2(GetDataObjectById,p,a,b)
#define ID3DXFileEnumObject_GetDataObjectByName(p,a,b)	ICOM_CALL2(GetDataObjectByName,p,a,b)

/*--------------------------------------------------*/
/* ID3DXFile */
/*--------------------------------------------------*/
#define ICOM_INTERFACE ID3DXFile
#define ID3DXFile_METHODS \
    ICOM_METHOD3(HRESULT,CreateEnumObject,          LPCVOID,pvSource,D3DXF_FILELOADOPTIONS,loadflags,ID3DXFileEnumObject**,ppEnumObj) \
    ICOM_METHOD4(HRESULT,CreateSaveObject,          LPCVOID,pData,D3DXF_FILESAVEOPTIONS,flags,D3DXF_FILEFORMAT,dwFileFormat,ID3DXFileSaveObject**,ppSaveObj) \
    ICOM_METHOD2(HRESULT,RegisterTemplates,         LPCVOID,pvData,size_t,cbSize) \
    ICOM_METHOD1(HRESULT,RegisterEnumTemplates,     ID3DXFileEnumObject*,pEnum)
#define ID3DXFile_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXFile_METHODS
ICOM_DEFINE(ID3DXFile, IUnknown)
#undef ICOM_INTERFACE

#define ID3DXFile_CreateEnumObject(p,a,b,c)				ICOM_CALL3(CreateEnumObject,p,a,b,c)
#define ID3DXFile_RegisterTemplates(p,a,b)				ICOM_CALL2(RegisterTemplates,p,a,b)

/*--------------------------------------------------*/
/* ID3DXFileSaveObject */
/*--------------------------------------------------*/
#define ICOM_INTERFACE ID3DXFileSaveObject
#define ID3DXFileSaveObject_METHODS \
    ICOM_METHOD1(HRESULT,GetFile,					ID3DXFile**,ppFile) \
    ICOM_METHOD4(HRESULT,AddDataObject,             LPCVOID,pData,D3DXF_FILESAVEOPTIONS,flags,D3DXF_FILEFORMAT,dwFileFormat,ID3DXFileSaveObject**,ppSaveObj) \
    ICOM_METHOD(HRESULT,Save)
#define ID3DXFileSaveObject_IMETHODS \
    IUnknown_IMETHODS \
    ID3DXFileSaveObject_METHODS
ICOM_DEFINE(ID3DXFileSaveObject, IUnknown)
#undef ICOM_INTERFACE


#ifdef __cplusplus
}
#endif
#endif /* __WINEX_D3DX9XOF_H */

/*--------------------------------------------------*/
