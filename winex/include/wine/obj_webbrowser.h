/*
 * Defines the COM interfaces and APIs related to the IE Web browser control
 *
 * 2001 John R. Sheets (for CodeWeavers)
 */

#ifndef __WINE_WINE_OBJ_WEBBROWSER_H
#define __WINE_WINE_OBJ_WEBBROWSER_H

#include "docobj.h"

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */


/*****************************************************************************
 * Predeclare the interfaces and class IDs
 */
DEFINE_GUID(IID_IWebBrowser, 0xeab22ac1, 0x30c1, 0x11cf, 0xa7, 0xeb, 0x00, 0x00, 0xc0, 0x5b, 0xae, 0x0b);
typedef struct IWebBrowser IWebBrowser, *LPWEBBROWSER;
DEFINE_GUID(IID_IWebBrowser2, 0xD30C1661, 0xCDAF, 0x11d0, 0x8A, 0x3E, 0x00, 0xC0, 0x4F, 0xC9, 0xE2, 0x6E);
typedef struct IWebBrowser2 IWebBrowser2, *LPWEBBROWSER2;

DEFINE_GUID(CLSID_WebBrowser, 0x8856f961, 0x340a, 0x11d0, 0xa9, 0x6b, 0x00, 0xc0, 0x4f, 0xd7, 0x05, 0xa2);

/*****************************************************************************
 * IWebBrowser interface
 */
#define ICOM_INTERFACE IWebBrowser
#define IWebBrowser_METHODS \
	ICOM_METHOD(HRESULT,GoBack) \
	ICOM_METHOD(HRESULT,GoForward) \
	ICOM_METHOD(HRESULT,GoHome) \
	ICOM_METHOD(HRESULT,GoSearch) \
	ICOM_METHOD5(HRESULT,Navigate, BSTR,URL, VARIANT*,Flags, VARIANT*,TargetFrameName, \
                                       VARIANT*,PostData, VARIANT*,Headers) \
	ICOM_METHOD(HRESULT,Refresh) \
	ICOM_METHOD1(HRESULT,Refresh2, VARIANT*,Level) \
	ICOM_METHOD(HRESULT,Stop) \
	ICOM_METHOD1(HRESULT,get_Application, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_Parent, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_Container, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_Document, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_TopLevelContainer, VARIANT*,pBool) \
	ICOM_METHOD1(HRESULT,get_Type, BSTR*,Type) \
	ICOM_METHOD1(HRESULT,get_Left, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Left, long,Left) \
	ICOM_METHOD1(HRESULT,get_Top, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Top, long,Top) \
	ICOM_METHOD1(HRESULT,get_Width, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Width, long,Width) \
	ICOM_METHOD1(HRESULT,get_Height, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Height, long,Height) \
	ICOM_METHOD1(HRESULT,get_LocationName, BSTR*,LocationName) \
	ICOM_METHOD1(HRESULT,get_LocationURL, BSTR*,LocationURL) \
	ICOM_METHOD1(HRESULT,get_Busy, VARIANT*,pBool)
#define IWebBrowser_IMETHODS \
	IDispatch_IMETHODS \
	IWebBrowser_METHODS
ICOM_DEFINE(IWebBrowser,IDispatch)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IWebBrowser_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define IWebBrowser_AddRef(p)                  ICOM_CALL (AddRef,p)
#define IWebBrowser_Release(p)                 ICOM_CALL (Release,p)
/*** IDispatch methods ***/
#define IWebBrowser_GetTypeInfoCount(p,a)      ICOM_CALL1 (GetTypeInfoCount,p,a)
#define IWebBrowser_GetTypeInfo(p,a,b,c)       ICOM_CALL3 (GetTypeInfo,p,a,b,c)
#define IWebBrowser_GetIDsOfNames(p,a,b,c,d,e) ICOM_CALL5 (GetIDsOfNames,p,a,b,c,d,e)
#define IWebBrowser_Invoke(p,a,b,c,d,e,f,g,h)  ICOM_CALL8 (Invoke,p,a,b,c,d,e,f,g,h)
/*** IWebBrowserContainer methods ***/
#define IWebBrowser_GoBack(p)      ICOM_CALL(GoBack,p)
#define IWebBrowser_GoForward(p)      ICOM_CALL(GoForward,p)
#define IWebBrowser_GoHome(p)      ICOM_CALL(GoHome,p)
#define IWebBrowser_GoSearch(p)      ICOM_CALL(GoSearch,p)
#define IWebBrowser_Navigate(p,a,b,c,d,e)      ICOM_CALL5(Navigate,p,a,b,c,d,e)
#define IWebBrowser_Refresh(p)      ICOM_CALL(Refresh,p)
#define IWebBrowser_Refresh2(p,a)      ICOM_CALL1(Refresh2,p,a)
#define IWebBrowser_Stop(p)      ICOM_CALL(Stop,p)
#define IWebBrowser_get_Application(p,a)      ICOM_CALL1(get_Application,p,a)
#define IWebBrowser_get_Parent(p,a)      ICOM_CALL1(get_Parent,p,a)
#define IWebBrowser_get_Container(p,a)      ICOM_CALL1(get_Container,p,a)
#define IWebBrowser_get_Document(p,a)      ICOM_CALL1(get_Document,p,a)
#define IWebBrowser_get_TopLevelContainer(p,a)      ICOM_CALL1(get_TopLevelContainer,p,a)
#define IWebBrowser_get_Type(p,a)      ICOM_CALL1(get_Type,p,a)
#define IWebBrowser_get_Left(p,a)      ICOM_CALL1(get_Left,p,a)
#define IWebBrowser_put_Left(p,a)      ICOM_CALL1(put_Left,p,a)
#define IWebBrowser_get_Top(p,a)      ICOM_CALL1(get_Top,p,a)
#define IWebBrowser_put_Top(p,a)      ICOM_CALL1(put_Top,p,a)
#define IWebBrowser_get_Width(p,a)      ICOM_CALL1(get_Width,p,a)
#define IWebBrowser_put_Width(p,a)      ICOM_CALL1(put_Width,p,a)
#define IWebBrowser_get_Height(p,a)      ICOM_CALL1(get_Height,p,a)
#define IWebBrowser_put_Height(p,a)      ICOM_CALL1(put_Height,p,a)
#define IWebBrowser_get_LocationName(p,a)      ICOM_CALL1(get_LocationName,p,a)
#define IWebBrowser_get_LocationURL(p,a)      ICOM_CALL1(get_LocationURL,p,a)
#define IWebBrowser_get_Busy(p,a)      ICOM_CALL1(get_Busy,p,a)


/*****************************************************************************
 * IWebBrowser2 interface
 */
#define ICOM_INTERFACE IWebBrowser2
#define IWebBrowser2_METHODS \
	ICOM_METHOD(HRESULT,GoBack) \
	ICOM_METHOD(HRESULT,GoForward) \
	ICOM_METHOD(HRESULT,GoHome) \
	ICOM_METHOD(HRESULT,GoSearch) \
	ICOM_METHOD5(HRESULT,Navigate, BSTR,URL, VARIANT*,Flags, VARIANT*,TargetFrameName, \
                                       VARIANT*,PostData, VARIANT*,Headers) \
	ICOM_METHOD(HRESULT,Refresh) \
	ICOM_METHOD1(HRESULT,Refresh2, VARIANT*,Level) \
	ICOM_METHOD(HRESULT,Stop) \
	ICOM_METHOD1(HRESULT,get_Application, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_Parent, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_Container, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_Document, void**,ppDisp) \
	ICOM_METHOD1(HRESULT,get_TopLevelContainer, VARIANT*,pBool) \
	ICOM_METHOD1(HRESULT,get_Type, BSTR*,Type) \
	ICOM_METHOD1(HRESULT,get_Left, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Left, long,Left) \
	ICOM_METHOD1(HRESULT,get_Top, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Top, long,Top) \
	ICOM_METHOD1(HRESULT,get_Width, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Width, long,Width) \
	ICOM_METHOD1(HRESULT,get_Height, long*,pl) \
	ICOM_METHOD1(HRESULT,put_Height, long,Height) \
	ICOM_METHOD1(HRESULT,get_LocationName, BSTR*,LocationName) \
	ICOM_METHOD1(HRESULT,get_LocationURL, BSTR*,LocationURL) \
	ICOM_METHOD1(HRESULT,get_Busy, VARIANT*,pBool) \
        ICOM_METHOD(HRESULT, Quit) \
        ICOM_METHOD2(HRESULT,ClientToWindow, int*, pcx, int*, pcy) \
        ICOM_METHOD2(HRESULT,PutProperty, BSTR,Property, VARIANT,vtValue) \
        ICOM_METHOD2(HRESULT,GetProperty, BSTR,Property, VARIANT*,pvtValue)\
        ICOM_METHOD1(HRESULT,get_Name, BSTR*,Name) \
        ICOM_METHOD1(HRESULT,get_HWND, long*,pHWND) \
        ICOM_METHOD1(HRESULT,get_FullName, BSTR*,FullName)  \
        ICOM_METHOD1(HRESULT,get_Path, BSTR*, Path)  \
        ICOM_METHOD1(HRESULT,get_Visible, VARIANT_BOOL*,pBool) \
        ICOM_METHOD1(HRESULT,put_Visible,VARIANT_BOOL,Value) \
        ICOM_METHOD1(HRESULT,get_StatusBar, VARIANT_BOOL*,pBool) \
        ICOM_METHOD1(HRESULT,put_StatusBar, VARIANT_BOOL,Value)  \
        ICOM_METHOD1(HRESULT,get_StatusText, BSTR*,StatusText)  \
        ICOM_METHOD1(HRESULT,put_StatusText, BSTR,StatusText)  \
        ICOM_METHOD1(HRESULT,get_ToolBar, int*,Value)  \
        ICOM_METHOD1(HRESULT,put_ToolBar, int,Value)  \
        ICOM_METHOD1(HRESULT,get_MenuBar, VARIANT_BOOL*,Value)  \
        ICOM_METHOD1(HRESULT,put_MenuBar, VARIANT_BOOL,Value)  \
        ICOM_METHOD1(HRESULT,get_FullScreen, VARIANT_BOOL* ,pbFullScreen)  \
        ICOM_METHOD1(HRESULT,put_FullScreen, VARIANT_BOOL,bFullScreen)   \
        ICOM_METHOD5(HRESULT,Navigate2, VARIANT*,URL, VARIANT*,Flags, VARIANT*,TargetFrameName, VARIANT*,PostData, VARIANT*,Headers)  \
        ICOM_METHOD2(HRESULT,QueryStatusWB, OLECMDID,cmdID, OLECMDF*,pcmdf)  \
        ICOM_METHOD4(HRESULT,ExecWB, OLECMDID,cmdID, OLECMDEXECOPT,cmdexecopt, VARIANT*,pvaIn, VARIANT*,pvaOut)  \
        ICOM_METHOD3(HRESULT,ShowBrowserBar, VARIANT*,pvaClsid, VARIANT*,pvarShow, VARIANT*,pvarSize)  \
        ICOM_METHOD1(HRESULT,get_ReadyState, READYSTATE*,plReadyState)  \
        ICOM_METHOD1(HRESULT,get_Offline, VARIANT_BOOL*,pbOffline)  \
        ICOM_METHOD1(HRESULT,put_Offline, VARIANT_BOOL,bOffline)  \
        ICOM_METHOD1(HRESULT,get_Silent, VARIANT_BOOL*,pbSilent)  \
        ICOM_METHOD1(HRESULT,put_Silent, VARIANT_BOOL,bSilent)   \
        ICOM_METHOD1(HRESULT,get_RegisterAsBrowser, VARIANT_BOOL*,pbRegister)  \
        ICOM_METHOD1(HRESULT,put_RegisterAsBrowser, VARIANT_BOOL,bRegister)  \
        ICOM_METHOD1(HRESULT,get_RegisterAsDropTarget, VARIANT_BOOL*,pbRegister)  \
        ICOM_METHOD1(HRESULT,put_RegisterAsDropTarget, VARIANT_BOOL,bRegister)   \
        ICOM_METHOD1(HRESULT,get_TheaterMode, VARIANT_BOOL*,pbRegister)  \
        ICOM_METHOD1(HRESULT,put_TheaterMode, VARIANT_BOOL,bRegister)   \
        ICOM_METHOD1(HRESULT,get_AddressBar, VARIANT_BOOL*,Value)   \
        ICOM_METHOD1(HRESULT,put_AddressBar, VARIANT_BOOL,Value)  \
        ICOM_METHOD1(HRESULT,get_Resizable, VARIANT_BOOL*,Value)  \
        ICOM_METHOD1(HRESULT,put_Resizable, VARIANT_BOOL,Value) 
#define IWebBrowser2_IMETHODS \
	IDispatch_IMETHODS \
	IWebBrowser2_METHODS
ICOM_DEFINE(IWebBrowser2,IDispatch)
#undef ICOM_INTERFACE

/*** IUnknown methods ***/
#define IWebBrowser2_QueryInterface(p,a,b)      ICOM_CALL2(QueryInterface,p,a,b)
#define IWebBrowser2_AddRef(p)                  ICOM_CALL (AddRef,p)
#define IWebBrowser2_Release(p)                 ICOM_CALL (Release,p)
/*** IDispatch methods ***/
#define IWebBrowser2_GetTypeInfoCount(p,a)      ICOM_CALL1 (GetTypeInfoCount,p,a)
#define IWebBrowser2_GetTypeInfo(p,a,b,c)       ICOM_CALL3 (GetTypeInfo,p,a,b,c)
#define IWebBrowser2_GetIDsOfNames(p,a,b,c,d,e) ICOM_CALL5 (GetIDsOfNames,p,a,b,c,d,e)
#define IWebBrowser2_Invoke(p,a,b,c,d,e,f,g,h)  ICOM_CALL8 (Invoke,p,a,b,c,d,e,f,g,h)
/*** IWebBrowserContainer methods ***/
#define IWebBrowser2_GoBack(p)      ICOM_CALL(GoBack,p)
#define IWebBrowser2_GoForward(p)      ICOM_CALL(GoForward,p)
#define IWebBrowser2_GoHome(p)      ICOM_CALL(GoHome,p)
#define IWebBrowser2_GoSearch(p)      ICOM_CALL(GoSearch,p)
#define IWebBrowser2_Navigate(p,a,b,c,d,e)      ICOM_CALL5(Navigate,p,a,b,c,d,e)
#define IWebBrowser2_Refresh(p)      ICOM_CALL(Refresh,p)
#define IWebBrowser2_Refresh2(p,a)      ICOM_CALL1(Refresh2,p,a)
#define IWebBrowser2_Stop(p)      ICOM_CALL(Stop,p)
#define IWebBrowser2_get_Application(p,a)      ICOM_CALL1(get_Application,p,a)
#define IWebBrowser2_get_Parent(p,a)      ICOM_CALL1(get_Parent,p,a)
#define IWebBrowser2_get_Container(p,a)      ICOM_CALL1(get_Container,p,a)
#define IWebBrowser2_get_Document(p,a)      ICOM_CALL1(get_Document,p,a)
#define IWebBrowser2_get_TopLevelContainer(p,a)      ICOM_CALL1(get_TopLevelContainer,p,a)
#define IWebBrowser2_get_Type(p,a)      ICOM_CALL1(get_Type,p,a)
#define IWebBrowser2_get_Left(p,a)      ICOM_CALL1(get_Left,p,a)
#define IWebBrowser2_put_Left(p,a)      ICOM_CALL1(put_Left,p,a)
#define IWebBrowser2_get_Top(p,a)      ICOM_CALL1(get_Top,p,a)
#define IWebBrowser2_put_Top(p,a)      ICOM_CALL1(put_Top,p,a)
#define IWebBrowser2_get_Width(p,a)      ICOM_CALL1(get_Width,p,a)
#define IWebBrowser2_put_Width(p,a)      ICOM_CALL1(put_Width,p,a)
#define IWebBrowser2_get_Height(p,a)      ICOM_CALL1(get_Height,p,a)
#define IWebBrowser2_put_Height(p,a)      ICOM_CALL1(put_Height,p,a)
#define IWebBrowser2_get_LocationName(p,a)      ICOM_CALL1(get_LocationName,p,a)
#define IWebBrowser2_get_LocationURL(p,a)      ICOM_CALL1(get_LocationURL,p,a)
#define IWebBrowser2_get_Busy(p,a)      ICOM_CALL1(get_Busy,p,a)
/*** IWebBrowserApp ***/
#define IWebBrowser2_Quit(p)                         ICOM_CALL(Quit,p)
#define IWebBrowser2_ClientToWindow(p,a,b)           ICOM_CALL2(ClientToWindow,p,a,b)
#define IWebBrowser2_PutProperty(p,a,b)              ICOM_CALL2(PutProperty,p,a,b)         
#define IWebBrowser2_GetProperty(p,a,b)              ICOM_CALL2(GetProperty,p,a,b)
#define IWebBrowser2_get_Name(p,a)                   ICOM_CALL1(get_Namer,p,a)
#define IWebBrowser2_get_HWND(p,a)                   ICOM_CALL1(get_HWND,p,a)
#define IWebBrowser2_get_FullName(p,a)               ICOM_CALL1(get_FullName,p,a)
#define IWebBrowser2_get_Path(p,a)	             ICOM_CALL1(get_Path,p,a)
#define IWebBrowser2_get_Visible(p,a)                ICOM_CALL1(get_Visible,p,a)
#define IWebBrowser2_put_Visible(p,a)	             ICOM_CALL1(put_Visible,p,a)
#define IWebBrowser2_get_StatusBar(p,a)              ICOM_CALL1(get_StatusBar,p,a)
#define IWebBrowser2_put_StatusBar(p,a)	             ICOM_CALL1(put_StatusBar,p,a)
#define IWebBrowser2_get_StatusText(p,a)             ICOM_CALL1(get_StatusText,p,a)
#define IWebBrowser2_put_StatusText(p,a)             ICOM_CALL1(put_StatusText,p,a)
#define IWebBrowser2_get_ToolBar(p,a)                ICOM_CALL1(get_ToolBar,p,a)
#define IWebBrowser2_put_ToolBar(p,a)                ICOM_CALL1(put_ToolBar,p,a)
#define IWebBrowser2_get_MenuBar(p,a)                ICOM_CALL1(get_MenuBar,p,a)
#define IWebBrowser2_put_MenuBar(p,a)	             ICOM_CALL1(put_MenuBar,p,a)
#define IWebBrowser2_get_FullScreen(p,a)             ICOM_CALL1(get_FullScreen,p,a)
#define IWebBrowser2_put_FullScreen(p,a)             ICOM_CALL1(put_FullScreen,p,a)
/*** IWebBrowser2 ***/
#define IWebBrowser2_Navigate2(p,a,b,c,d,e)	     ICOM_CALL5(Navigate2,p,a,b,c,d,e)
#define IWebBrowser2_QueryStatusWB(p,a,b)	     ICOM_CALL2(QueryStatusWB,p,a,b)
#define IWebBrowser2_ExecWB(p,a,b,c,d)	             ICOM_CALL4(ExecWB,p,a,b,c,d)
#define IWebBrowser2_ShowBrowserBar(p,a,b,c)         ICOM_CALL3(ShowBrowserBar,p,a,b,c)
#define IWebBrowser2_get_ReadyState(p,a)             ICOM_CALL1(get_ReadyState,p,a)
#define IWebBrowser2_get_Offline(p,a)	             ICOM_CALL1(get_Offline,p,a)
#define IWebBrowser2_put_Offline(p,a)	             ICOM_CALL1(put_Offline,p,a)
#define IWebBrowser2_get_Silent(p,a)                 ICOM_CALL1(get_Silent,p,a)
#define IWebBrowser2_put_Silent(p,a)                 ICOM_CALL1(put_Silent,p,a)
#define IWebBrowser2_get_RegisterAsBrowser(p,a)      ICOM_CALL1(get_RegisterAsBrowser,p,a)
#define IWebBrowser2_put_RegisterAsBrowser(p,a)      ICOM_CALL1(put_RegisterAsBrowser,p,a)
#define IWebBrowser2_get_RegisterAsDropTarget(p,a)   ICOM_CALL1(get_RegisterAsDropTarget,p,a)
#define IWebBrowser2_put_RegisterAsDropTarget(p,a)   ICOM_CALL1(put_RegisterAsDropTarget,p,a)
#define IWebBrowser2_get_TheaterMode(p,a)            ICOM_CALL1(get_TheaterMode,p,a)
#define IWebBrowser2_put_TheaterMode(p,a)            ICOM_CALL1(put_TheaterMode,p,a)
#define IWebBrowser2_get_AddressBar(p,a)             ICOM_CALL1(get_AddressBar,p,a)
#define IWebBrowser2_put_AddressBar(p,a)             ICOM_CALL1(put_AddressBar,p,a)
#define IWebBrowser2_get_Resizable(p,a)              ICOM_CALL1(get_Resizable,p,a)
#define IWebBrowser2_put_Resizable(p,a)              ICOM_CALL1(put_Resizable,p,a)



#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* __WINE_WINE_OBJ_WEBBROWSER_H */
