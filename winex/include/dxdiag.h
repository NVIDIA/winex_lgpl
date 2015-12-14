#ifndef _WINE_DXDIAG_H_
#define _WINE_DXDIAG_H_

#include "ole2.h"
/* i'm not sure about this oaidl, but one of the interfaces needs 'VARIANT'.. which is
 * defined in oaidl.h
 */
#include "oaidl.h"

#define DXDIAG_DX9_SDK_VERSION 111

#ifdef __cplusplus
extern "C" {
#endif


/* errors */

#define DXDIAG_E_INSUFFICIENT_BUFFER       ((HRESULT)0x8007007AL)


/* CLSIDs for classes */

/* {A65B8071-3BFE-4213-9A5B-491DA4461CA7} */
DEFINE_GUID(CLSID_DxDiagProvider,
0xA65B8071, 0x3BFE, 0x4213, 0x9A, 0x5B, 0x49, 0x1D, 0xA4, 0x46, 0x1C, 0xA7);


/* IIDs for interfaces */

/* {9C6B4CB0-23F8-49CC-A3ED-45A55000A6D2} */
DEFINE_GUID(IID_IDxDiagProvider,
0x9C6B4CB0, 0x23F8, 0x49CC, 0xA3, 0xED, 0x45, 0xA5, 0x50, 0x00, 0xA6, 0xD2);

/* {0x7D0F462F-0x4064-0x4862-BC7F-933E5058C10F} */
DEFINE_GUID(IID_IDxDiagContainer,
0x7D0F462F, 0x4064, 0x4862, 0xBC, 0x7F, 0x93, 0x3E, 0x50, 0x58, 0xC1, 0x0F);

/* interface pointers */
typedef struct IDxDiagProvider     IDxDiagProvider, *LPDXDIAGPROVIDER, *PDXDIAGPROVIDER;
typedef struct IDxDiagContainer    IDxDiagContainer, *LPDXDIAGCONTAINER, *PDXDIAGCONTAINER;


/* structures */
typedef struct _DXDIAG_INIT_PARAMS
{
    DWORD   dwSize;
    DWORD   dwDxDiagHeaderVersion;
    BOOL    bAllowWHQLChecks;
    VOID*   pReserved;
} DXDIAG_INIT_PARAMS;


/********* Interfaces ***********/


#define ICOM_INTERFACE IDxDiagProvider
#define IDxDiagProvider_METHODS \
    ICOM_METHOD1(HRESULT,Initialize,                   DXDIAG_INIT_PARAMS*, pParams) \
    ICOM_METHOD1(HRESULT,GetRootContainer,             IDxDiagContainer **, ppInstance)
#define IDxDiagProvider_IMETHODS \
    IUnknown_METHODS \
    IDxDiagProvider_METHODS
ICOM_DEFINE(IDxDiagProvider,IUnknown)
#undef ICOM_INTERFACE

/* HACK: when this is built in ole/uuid.c winuser.h is included already
 * which defined GetProp (since its an A/W function)..
 * this fixes that here
 */
#ifdef __WINE__
#undef GetProp
#endif

#define ICOM_INTERFACE IDxDiagContainer
#define IDxDiagContainer_METHODS \
    ICOM_METHOD1(HRESULT, GetNumberOfChildContainers,           DWORD*, pdwCount) \
    ICOM_METHOD3(HRESULT, EnumChildContainerNames,              DWORD, dwIndex, LPWSTR, pwszContainer, DWORD, cchContainer) \
    ICOM_METHOD2(HRESULT, GetChildContainer,                    LPCWSTR, pwszContainer, IDxDiagContainer**, ppInstance) \
    ICOM_METHOD1(HRESULT, GetNumberOfProps,                     DWORD*, pdwCount) \
    ICOM_METHOD3(HRESULT, EnumPropNames,                        DWORD, dwIndex, LPWSTR, pwszPropName, DWORD, cchPropName) \
    ICOM_METHOD2(HRESULT, GetProp,                              LPCWSTR, pwszPropName, VARIANT*, pvarProp)
#define IDxDiagContainer_IMETHODS \
    IUnknown_METHODS \
    IDxDiagContainer_METHODS
ICOM_DEFINE(IDxDiagContainer,IUnknown)
#undef ICOM_INTERFACE

#define IDxDiagProvider_QueryInterface(p,a,b)                   ICOM_CALL2(QueryInterface,p,a,b)
#define IDxDiagProvider_AddRef(p)                               ICOM_CALL (AddRef,p)
#define IDxDiagProvider_Release(p)                              ICOM_CALL (Release,p)
#define IDxDiagProvider_Initialize(p,a,b)                       ICOM_CALL2(Initialize,p,a,b)
#define IDxDiagProvider_GetRootContainer(p,a)                   ICOM_CALL1(GetRootContainer,p,a)

#define IDxDiagContainer_QueryInterface(p,a,b)                  ICOM_CALL2(QueryInterface,p,a,b)
#define IDxDiagContainer_AddRef(p)                              ICOM_CALL (AddRef,p)
#define IDxDiagContainer_Release(p)                             ICOM_CALL (Release,p)
#define IDxDiagContainer_GetNumberOfChildContainers(p,a)        ICOM_CALL1(GetNumberOfChildContainers,p,a)
#define IDxDiagContainer_EnumChildContainerNames(p,a,b,c)       ICOM_CALL3(EnumChildContainerNames,p,a,b,c)
#define IDxDiagContainer_GetChildContainer(p,a,b)               ICOM_CALL2(GetChildContainer,p,a,b)
#define IDxDiagContainer_GetNumberOfProps(p,a)                  ICOM_CALL1(GetNumberOfProps,p,a)
#define IDxDiagContainer_EnumProps(p,a,b)                       ICOM_CALL3(EnumProps,p,a,b,c)
#define IDxDiagContainer_GetProp(p,a,b)                         ICOM_CALL2(GetProp,p,a,b)

#ifdef __cplusplus
}
#endif

#endif /* _WINE_DXDIAG_H_ */


