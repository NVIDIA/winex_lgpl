#ifndef __WINE_INTSHCUT_H
#define __WINE_INTSHCUT_H

#ifdef __cplusplus
extern "C" {
#endif

DEFINE_GUID(CLSID_InternetShortcut, 0xfbf23b40,0xe3f0,0x101b,0x84,0x88,0x00,0xaa,0x00,0x3e,0x56,0xf8);
DEFINE_GUID(IID_IUniformResourceLocatorA, 0xfbf23b80,0xe3f0,0x101b,0x84,0x88,0x00,0xaa,0x00,0x3e,0x56,0xf8);
DEFINE_GUID(IID_IUniformResourceLocatorW, 0xcabb0da0,0xda57,0x11cf,0x99,0x74,0x00,0x20,0xaf,0xd7,0x97,0x62);

typedef struct _URLINVOKECOMMANDINFOA {
  DWORD  dwcbSize;
  DWORD  dwFlags;
  HWND   hwndParent;
  LPCSTR pcszVerb;
} URLINVOKECOMMANDINFOA, *PURLINVOKECOMMANDINFOA;

typedef struct _URLINVOKECOMMANDINFOW {
  DWORD   dwcbSize;
  DWORD   dwFlags;
  HWND    hwndParent;
  LPCWSTR pcszVerb;
} URLINVOKECOMMANDINFOW, *PURLINVOKECOMMANDINFOW;

#define E_FLAGS                     MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1000)
#define IS_E_EXEC_FAILED            MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x2002)
#define URL_E_INVALID_SYNTAX        MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1001)
#define URL_E_UNREGISTERED_PROTOCOL MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1002)
#define URL_E_UNREGISTERED_PROTOCOL MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1002)

typedef struct IUniformResourceLocatorA IUniformResourceLocatorA, *LPUNIFORMRESOURCELOCATORA;
typedef struct IUniformResourceLocatorW IUniformResourceLocatorW, *LPUNIFORMRESOURCELOCATORW;

/*****************************************************************************
 * IUniformResourceLocatorA interface
 */
#define ICOM_INTERFACE IUniformResourceLocatorA
#define IUniformResourceLocatorA_METHODS \
  ICOM_METHOD2(HRESULT,SetURL,       LPCSTR,pcstrURL,DWORD,dwFlags) \
  ICOM_METHOD1(HRESULT,GetURL,       LPSTR*,pstrURL) \
  ICOM_METHOD1(HRESULT,InvokeCommand,PURLINVOKECOMMANDINFOA,pInv)
#define IUniformResourceLocatorA_IMETHODS \
  IUnknown_IMETHODS \
  IUniformResourceLocatorA_METHODS
ICOM_DEFINE(IUniformResourceLocatorA,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IUniformResourceLocatorA_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IUniformResourceLocatorA_AddRef(p)             ICOM_CALL (AddRef,p)
#define IUniformResourceLocatorA_Release(p)            ICOM_CALL (Release,p)
	/*** IUniformResourceLocatorW methods */
#define IUniformResourceLocatorA_SetURL(p,a,b)         ICOM_CALL2(SetURL,p,a,b)
#define IUniformResourceLocatorA_GetURL(p,a)           ICOM_CALL1(GetURL,p,a)
#define IUniformResourceLocatorA_InvokeCommand(p,a)    ICOM_CALL1(InvokeCommand,p,a)


/*****************************************************************************
 * IUniformResourceLocatorW interface
 */
#define ICOM_INTERFACE IUniformResourceLocatorW
#define IUniformResourceLocatorW_METHODS \
  ICOM_METHOD2(HRESULT,SetURL,       LPCWSTR,pcstrURL,DWORD,dwFlags) \
  ICOM_METHOD1(HRESULT,GetURL,       LPWSTR*,pstrURL) \
  ICOM_METHOD1(HRESULT,InvokeCommand,PURLINVOKECOMMANDINFOW,pInv)
#define IUniformResourceLocatorW_IMETHODS \
  IUnknown_IMETHODS \
  IUniformResourceLocatorW_METHODS
ICOM_DEFINE(IUniformResourceLocatorW,IUnknown)
#undef ICOM_INTERFACE

	/*** IUnknown methods ***/
#define IUniformResourceLocatorW_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IUniformResourceLocatorW_AddRef(p)             ICOM_CALL (AddRef,p)
#define IUniformResourceLocatorW_Release(p)            ICOM_CALL (Release,p)
	/*** IUniformResourceLocatorW methods */
#define IUniformResourceLocatorW_SetURL(p,a,b)         ICOM_CALL2(SetURL,p,a,b)
#define IUniformResourceLocatorW_GetURL(p,a)           ICOM_CALL1(GetURL,p,a)
#define IUniformResourceLocatorW_InvokeCommand(p,a)    ICOM_CALL1(InvokeCommand,p,a)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __WINE_INTSHCUT_H */
