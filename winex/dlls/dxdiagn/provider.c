/* 
 * IDxDiagProvider Implementation
 * 
 * Copyright 2004-2005 Raphael Junqueira
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#include "config.h"

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "dxdiag_private.h"
#include "wine/unicode.h"
#include "winver.h"
#include "objidl.h"
/*#include "dshow.h"*/
#include "strmif.h"
#include "vfw.h"
#include "mmddk.h"
#include "ddraw.h"
#include "d3d9.h"
#include "strmif.h"
#include "initguid.h"
/*#include "fil_data.h"*/

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxdiag);

static HRESULT DXDiag_InitRootDXDiagContainer(IDxDiagContainer* pRootCont);

/* IDxDiagProvider IUnknown parts follow: */
static HRESULT WINAPI IDxDiagProviderImpl_QueryInterface(PDXDIAGPROVIDER iface, REFIID riid, LPVOID *ppobj)
{
    IDxDiagProviderImpl *This = (IDxDiagProviderImpl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDxDiagProvider)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }

    WARN("(%p)->(%s,%p),not found\n",This,debugstr_guid(riid),ppobj);
    return E_NOINTERFACE;
}

static ULONG WINAPI IDxDiagProviderImpl_AddRef(PDXDIAGPROVIDER iface) {
    IDxDiagProviderImpl *This = (IDxDiagProviderImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(ref before=%u)\n", This, refCount - 1);

    DXDIAGN_LockModule();

    return refCount;
}

static ULONG WINAPI IDxDiagProviderImpl_Release(PDXDIAGPROVIDER iface) {
    IDxDiagProviderImpl *This = (IDxDiagProviderImpl *)iface;
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(ref before=%u)\n", This, refCount + 1);

    if (!refCount) {
        HeapFree(GetProcessHeap(), 0, This);
    }

    DXDIAGN_UnlockModule();
    
    return refCount;
}

/* IDxDiagProvider Interface follow: */
static HRESULT WINAPI IDxDiagProviderImpl_Initialize(PDXDIAGPROVIDER iface, DXDIAG_INIT_PARAMS* pParams) {
    IDxDiagProviderImpl *This = (IDxDiagProviderImpl *)iface;
    TRACE("(%p,%p)\n", iface, pParams);

    if (NULL == pParams) {
      return E_POINTER;
    }
    if (pParams->dwSize != sizeof(DXDIAG_INIT_PARAMS)) {
      return E_INVALIDARG;
    }

    This->init = TRUE;
    memcpy(&This->params, pParams, pParams->dwSize);
    return S_OK;
}

static HRESULT WINAPI IDxDiagProviderImpl_GetRootContainer(PDXDIAGPROVIDER iface, IDxDiagContainer** ppInstance) {
  HRESULT hr = S_OK;
  IDxDiagProviderImpl *This = (IDxDiagProviderImpl *)iface;
  TRACE("(%p,%p)\n", iface, ppInstance);

  if (NULL == ppInstance) {
    return E_INVALIDARG;
  }
  if (FALSE == This->init) {
    return E_INVALIDARG; /* should be E_CO_UNINITIALIZED */
  }
  if (NULL == This->pRootContainer) {
    hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &This->pRootContainer);
    if (FAILED(hr)) {
      return hr;
    }
    hr = DXDiag_InitRootDXDiagContainer(This->pRootContainer);
  }
  return IDxDiagContainerImpl_QueryInterface((PDXDIAGCONTAINER)This->pRootContainer, &IID_IDxDiagContainer, (void**) ppInstance);
}

static const IDxDiagProviderVtbl DxDiagProvider_Vtbl =
{
    IDxDiagProviderImpl_QueryInterface,
    IDxDiagProviderImpl_AddRef,
    IDxDiagProviderImpl_Release,
    IDxDiagProviderImpl_Initialize,
    IDxDiagProviderImpl_GetRootContainer
};

HRESULT DXDiag_CreateDXDiagProvider(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj) {
  IDxDiagProviderImpl* provider;

  TRACE("(%p, %s, %p)\n", punkOuter, debugstr_guid(riid), ppobj);
  
  provider = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDxDiagProviderImpl));
  if (NULL == provider) {
    *ppobj = NULL;
    return E_OUTOFMEMORY;
  }
  provider->lpVtbl = &DxDiagProvider_Vtbl;
  provider->ref = 0; /* will be inited with QueryInterface */
  return IDxDiagProviderImpl_QueryInterface ((PDXDIAGPROVIDER)provider, riid, ppobj);
} 

static inline HRESULT add_prop_str( IDxDiagContainer* cont, LPCWSTR prop, LPCWSTR str )
{
    HRESULT hr;
    VARIANT var;

    V_VT( &var ) = VT_BSTR;
    V_BSTR( &var ) = SysAllocString( str );
    hr = IDxDiagContainerImpl_AddProp( cont, prop, &var );
    VariantClear( &var );

    return hr;
}

static inline HRESULT add_prop_ui4( IDxDiagContainer* cont, LPCWSTR prop, DWORD data )
{
    VARIANT var;

    V_VT( &var ) = VT_UI4;
    V_UI4( &var ) = data;
    return IDxDiagContainerImpl_AddProp( cont, prop, &var );
}

static inline HRESULT add_prop_bool( IDxDiagContainer* cont, LPCWSTR prop, BOOL data )
{
    VARIANT var;

    V_VT( &var ) = VT_BOOL;
    V_BOOL( &var ) = data;
    return IDxDiagContainerImpl_AddProp( cont, prop, &var );
}

static inline HRESULT add_prop_ull_as_str( IDxDiagContainer* cont, LPCWSTR prop, ULONGLONG data )
{
    HRESULT hr;
    VARIANT var;

    V_VT( &var ) = VT_UI8;
    V_UI8( &var ) = data;
    VariantChangeType( &var, &var, 0, VT_BSTR );
    hr = IDxDiagContainerImpl_AddProp( cont, prop, &var );
    VariantClear( &var );

    return hr;
}

/**
 * @param szFilePath: usually GetSystemDirectoryW
 * @param szFileName: name of the dll without path
 */
static HRESULT DXDiag_AddFileDescContainer(IDxDiagContainer* pSubCont, const WCHAR* szFilePath, const WCHAR* szFileName) {
  HRESULT hr = S_OK;
  /**/
  static const WCHAR szSlashSep[] = {'\\',0};
  static const WCHAR szPath[] = {'s','z','P','a','t','h',0};
  static const WCHAR szName[] = {'s','z','N','a','m','e',0};
  static const WCHAR szVersion[] = {'s','z','V','e','r','s','i','o','n',0};
  static const WCHAR szAttributes[] = {'s','z','A','t','t','r','i','b','u','t','e','s',0};
  static const WCHAR szLanguageEnglish[] = {'s','z','L','a','n','g','u','a','g','e','E','n','g','l','i','s','h',0};
  static const WCHAR dwFileTimeHigh[] = {'d','w','F','i','l','e','T','i','m','e','H','i','g','h',0};
  static const WCHAR dwFileTimeLow[] = {'d','w','F','i','l','e','T','i','m','e','L','o','w',0};
  static const WCHAR bBeta[] = {'b','B','e','t','a',0};
  static const WCHAR bDebug[] = {'b','D','e','b','u','g',0};
  static const WCHAR bExists[] = {'b','E','x','i','s','t','s',0};
  /** values */
  static const WCHAR szFinal_Retail_v[] = {'F','i','n','a','l',' ','R','e','t','a','i','l',0};
  static const WCHAR szEnglish_v[] = {'E','n','g','l','i','s','h',0};
  static const WCHAR szVersionFormat[] = {'%','u','.','%','0','2','u','.','%','0','4','u','.','%','0','4','u',0};

  WCHAR szFile[512];
  WCHAR szVersion_v[1024];
  DWORD retval, hdl;
  LPVOID pVersionInfo;
  BOOL boolret;
  UINT uiLength;
  VS_FIXEDFILEINFO* pFileInfo;

  TRACE("(%p,%s)\n", pSubCont, debugstr_w(szFileName));

  lstrcpyW(szFile, szFilePath);
  lstrcatW(szFile, szSlashSep);
  lstrcatW(szFile, szFileName);

  retval = GetFileVersionInfoSizeW(szFile, &hdl);
  pVersionInfo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, retval);
  boolret = GetFileVersionInfoW(szFile, 0, retval, pVersionInfo);
  boolret = VerQueryValueW(pVersionInfo, szSlashSep, (LPVOID) &pFileInfo, &uiLength);

  add_prop_str(pSubCont, szPath, szFile);
  add_prop_str(pSubCont, szName, szFileName);
  add_prop_bool(pSubCont, bExists, boolret);

  if (boolret) {
    snprintfW(szVersion_v, sizeof(szVersion_v)/sizeof(szVersion_v[0]),
	      szVersionFormat,
	      HIWORD(pFileInfo->dwFileVersionMS), 
	      LOWORD(pFileInfo->dwFileVersionMS),
	      HIWORD(pFileInfo->dwFileVersionLS),
	      LOWORD(pFileInfo->dwFileVersionLS));

    TRACE("Found version as (%s)\n", debugstr_w(szVersion_v));

    add_prop_str(pSubCont, szVersion,         szVersion_v);
    add_prop_str(pSubCont, szAttributes,      szFinal_Retail_v);
    add_prop_str(pSubCont, szLanguageEnglish, szEnglish_v);
    add_prop_ui4(pSubCont, dwFileTimeHigh,    pFileInfo->dwFileDateMS);
    add_prop_ui4(pSubCont, dwFileTimeLow,     pFileInfo->dwFileDateLS);
    add_prop_bool(pSubCont, bBeta,  0 != ((pFileInfo->dwFileFlags & pFileInfo->dwFileFlagsMask) & VS_FF_PRERELEASE));
    add_prop_bool(pSubCont, bDebug, 0 != ((pFileInfo->dwFileFlags & pFileInfo->dwFileFlagsMask) & VS_FF_DEBUG));
  }

  HeapFree(GetProcessHeap(), 0, pVersionInfo);

  return hr;
}

static HRESULT DXDiag_InitDXDiagSystemInfoContainer(IDxDiagContainer* pSubCont) {
  static const WCHAR dwDirectXVersionMajor[] = {'d','w','D','i','r','e','c','t','X','V','e','r','s','i','o','n','M','a','j','o','r',0};
  static const WCHAR dwDirectXVersionMinor[] = {'d','w','D','i','r','e','c','t','X','V','e','r','s','i','o','n','M','i','n','o','r',0};
  static const WCHAR szDirectXVersionLetter[] = {'s','z','D','i','r','e','c','t','X','V','e','r','s','i','o','n','L','e','t','t','e','r',0};
  static const WCHAR szDirectXVersionLetter_v[] = {'c',0};
  static const WCHAR bDebug[] = {'b','D','e','b','u','g',0};
  static const WCHAR bNECPC98[] = {'b','N','E','C','P','C','9','8',0};
  static const WCHAR szDirectXVersionEnglish[] = {'s','z','D','i','r','e','c','t','X','V','e','r','s','i','o','n','E','n','g','l','i','s','h',0};
  static const WCHAR szDirectXVersionEnglish_v[] = {'4','.','0','9','.','0','0','0','0','.','0','9','0','4',0};
  static const WCHAR szDirectXVersionLongEnglish[] = {'s','z','D','i','r','e','c','t','X','V','e','r','s','i','o','n','L','o','n','g','E','n','g','l','i','s','h',0};
  static const WCHAR szDirectXVersionLongEnglish_v[] = {'=',' ','"','D','i','r','e','c','t','X',' ','9','.','0','c',' ','(','4','.','0','9','.','0','0','0','0','.','0','9','0','4',')',0};
  static const WCHAR ullPhysicalMemory[] = {'u','l','l','P','h','y','s','i','c','a','l','M','e','m','o','r','y',0};
  static const WCHAR ullUsedPageFile[]   = {'u','l','l','U','s','e','d','P','a','g','e','F','i','l','e',0};
  static const WCHAR ullAvailPageFile[]  = {'u','l','l','A','v','a','i','l','P','a','g','e','F','i','l','e',0};
  /*static const WCHAR szDxDiagVersion[] = {'s','z','D','x','D','i','a','g','V','e','r','s','i','o','n',0};*/
  static const WCHAR szWindowsDir[] = {'s','z','W','i','n','d','o','w','s','D','i','r',0};
  static const WCHAR dwOSMajorVersion[] = {'d','w','O','S','M','a','j','o','r','V','e','r','s','i','o','n',0};
  static const WCHAR dwOSMinorVersion[] = {'d','w','O','S','M','i','n','o','r','V','e','r','s','i','o','n',0};
  static const WCHAR dwOSBuildNumber[] = {'d','w','O','S','B','u','i','l','d','N','u','m','b','e','r',0};
  static const WCHAR dwOSPlatformID[] = {'d','w','O','S','P','l','a','t','f','o','r','m','I','D',0};
  static const WCHAR szCSDVersion[] = {'s','z','C','S','D','V','e','r','s','i','o','n',0};
  MEMORYSTATUSEX msex;
  OSVERSIONINFOW info;
  WCHAR buffer[MAX_PATH];

  add_prop_ui4(pSubCont, dwDirectXVersionMajor, 9);
  add_prop_ui4(pSubCont, dwDirectXVersionMinor, 0);
  add_prop_str(pSubCont, szDirectXVersionLetter, szDirectXVersionLetter_v);
  add_prop_str(pSubCont, szDirectXVersionEnglish, szDirectXVersionEnglish_v);
  add_prop_str(pSubCont, szDirectXVersionLongEnglish, szDirectXVersionLongEnglish_v);
  add_prop_bool(pSubCont, bDebug, FALSE);
  add_prop_bool(pSubCont, bNECPC98, FALSE);

  msex.dwLength = sizeof(msex);
  GlobalMemoryStatusEx( &msex );
  add_prop_ull_as_str(pSubCont, ullPhysicalMemory, msex.ullTotalPhys);
  add_prop_ull_as_str(pSubCont, ullUsedPageFile, msex.ullTotalPageFile - msex.ullAvailPageFile);
  add_prop_ull_as_str(pSubCont, ullAvailPageFile, msex.ullAvailPageFile);

  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionExW( &info );
  add_prop_ui4(pSubCont, dwOSMajorVersion, info.dwMajorVersion);
  add_prop_ui4(pSubCont, dwOSMinorVersion, info.dwMinorVersion);
  add_prop_ui4(pSubCont, dwOSBuildNumber,  info.dwBuildNumber);
  add_prop_ui4(pSubCont, dwOSPlatformID,   info.dwPlatformId);
  add_prop_str(pSubCont, szCSDVersion,     info.szCSDVersion);

  GetWindowsDirectoryW(buffer, MAX_PATH);
  add_prop_str(pSubCont, szWindowsDir, buffer);

  return S_OK;
}

static HRESULT DXDiag_InitDXDiagSystemDevicesContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
  /*
  static const WCHAR szDescription[] = {'s','z','D','e','s','c','r','i','p','t','i','o','n',0};
  static const WCHAR szDeviceID[] = {'s','z','D','e','v','i','c','e','I','D',0};

  static const WCHAR szDrivers[] = {'s','z','D','r','i','v','e','r','s',0};

  VARIANT v;
  IDxDiagContainer* pDeviceSubCont = NULL;
  IDxDiagContainer* pDriversCont = NULL;

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, &pDeviceSubCont);
  if (FAILED(hr)) { return hr; }
  V_VT(pvarProp) = VT_BSTR; V_BSTR(pvarProp) = SysAllocString(property->psz);
  hr = IDxDiagContainerImpl_AddProp(pDeviceSubCont, szDescription, &v);
  VariantClear(&v);
  V_VT(pvarProp) = VT_BSTR; V_BSTR(pvarProp) = SysAllocString(property->psz);
  hr = IDxDiagContainerImpl_AddProp(pDeviceSubCont, szDeviceID, &v);
  VariantClear(&v);

  hr = IDxDiagContainerImpl_AddChildContainer(pSubCont, "", pDeviceSubCont);
  */

  /*
   * Drivers Cont contains Files Desc Containers
   */
  /*
  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, &pDriversCont);
  if (FAILED(hr)) { return hr; }
  hr = IDxDiagContainerImpl_AddChildContainer(pDeviceSubCont, szDrivers, pDriversCont);

  */
  return hr;
}

static HRESULT DXDiag_InitDXDiagLogicalDisksContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
  /*
  static const WCHAR szDriveLetter[] = {'s','z','D','r','i','v','e','L','e','t','t','e','r',0};
  static const WCHAR szFreeSpace[] = {'s','z','F','r','e','e','S','p','a','c','e',0};
  static const WCHAR szMaxSpace[] = {'s','z','M','a','x','S','p','a','c','e',0};
  static const WCHAR szFileSystem[] = {'s','z','F','i','l','e','S','y','s','t','e','m',0};
  static const WCHAR szModel[] = {'s','z','M','o','d','e','l',0};
  static const WCHAR szPNPDeviceID[] = {'s','z','P','N','P','D','e','v','i','c','e','I','D',0};
  static const WCHAR dwHardDriveIndex[] = {'d','w','H','a','r','d','D','r','i','v','e','I','n','d','e','x',0};

  static const WCHAR szDrivers[] = {'s','z','D','r','i','v','e','r','s',0};
 
  VARIANT v;
  IDxDiagContainer* pDiskSubCont = NULL;
  IDxDiagContainer* pDriversCont = NULL;

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, &pDiskSubCont);
  if (FAILED(hr)) { return hr; }
  hr = IDxDiagContainerImpl_AddChildContainer(pSubCont, "" , pDiskSubCont);
  */
  
  /*
   * Drivers Cont contains Files Desc Containers
   */
  /*
  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, &pDriversCont);
  if (FAILED(hr)) { return hr; }
  hr = IDxDiagContainerImpl_AddChildContainer(pDeviceSubCont, szDrivers, pDriversCont);
  */
  return hr;
}

static HRESULT DXDiag_InitDXDiagDirectXFilesContainer(IDxDiagContainer* pSubCont)
{
    HRESULT hr = S_OK;
    static const WCHAR dlls[][15] =
    {
        {'d','3','d','8','.','d','l','l',0},
        {'d','3','d','9','.','d','l','l',0},
        {'d','d','r','a','w','.','d','l','l',0},
        {'d','e','v','e','n','u','m','.','d','l','l',0},
        {'d','i','n','p','u','t','8','.','d','l','l',0},
        {'d','i','n','p','u','t','.','d','l','l',0},
        /* Commented out dlls haven't been implemented in our tree */
        /*{'d','m','b','a','n','d','.','d','l','l',0},*/
        /*{'d','m','c','o','m','p','o','s','.','d','l','l',0},*/
        {'d','m','i','m','e','.','d','l','l',0},
        {'d','m','l','o','a','d','e','r','.','d','l','l',0},
        /*{'d','m','s','c','r','i','p','t','.','d','l','l',0},*/
        /*{'d','m','s','t','y','l','e','.','d','l','l',0},*/
        {'d','m','s','y','n','t','h','.','d','l','l',0},
        {'d','m','u','s','i','c','.','d','l','l',0},
        {'d','p','l','a','y','x','.','d','l','l',0},
        {'d','p','n','e','t','.','d','l','l',0},
        {'d','s','o','u','n','d','.','d','l','l',0},
        {'d','s','w','a','v','e','.','d','l','l',0},
        {'d','x','d','i','a','g','n','.','d','l','l',0},
        {'q','u','a','r','t','z','.','d','l','l',0}
    };
    WCHAR szFilePath[MAX_PATH];
    INT i;

    GetSystemDirectoryW(szFilePath, MAX_PATH);

    for (i = 0; i < sizeof(dlls) / sizeof(dlls[0]); i++)
    {
        static const WCHAR szFormat[] = {'%','d',0};
        WCHAR szFileID[5];
        IDxDiagContainer *pDXFileSubCont;

        snprintfW(szFileID, sizeof(szFileID)/sizeof(szFileID[0]), szFormat, i);

        hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pDXFileSubCont);
        if (FAILED(hr)) continue;

        if (FAILED(DXDiag_AddFileDescContainer(pDXFileSubCont, szFilePath, dlls[i])) ||
            FAILED(IDxDiagContainerImpl_AddChildContainer(pSubCont, szFileID, pDXFileSubCont)))
        {
            IUnknown_Release(pDXFileSubCont);
            continue;
        }
    }
    return hr;
}

/* The logic from pixelformat_for_depth() in dlls/wined3d/utils.c is reversed. */
static DWORD depth_for_pixelformat(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_P8: return 8;
    case D3DFMT_X1R5G5B5: return 15;
    case D3DFMT_R5G6B5: return 16;
    /* This case will fail to distinguish an original bpp of 24. */
    case D3DFMT_X8R8G8B8: return 32;
    default:
        FIXME("Unknown D3DFORMAT %d, returning 32 bpp\n", format);
        return 32;
    }
}

static BOOL get_texture_memory(GUID *adapter, DWORD *available_mem)
{
    IDirectDraw7 *pDirectDraw;
    HRESULT hr;
    DDSCAPS2 dd_caps;

    hr = DirectDrawCreateEx(adapter, (void **)&pDirectDraw, &IID_IDirectDraw7, NULL);
    if (SUCCEEDED(hr))
    {
        dd_caps.dwCaps = DDSCAPS_LOCALVIDMEM | DDSCAPS_VIDEOMEMORY;
        dd_caps.dwCaps2 = dd_caps.dwCaps3 = dd_caps.dwCaps4 = 0;
        hr = IDirectDraw7_GetAvailableVidMem(pDirectDraw, &dd_caps, available_mem, NULL);
        IDirectDraw7_Release(pDirectDraw);
        if (SUCCEEDED(hr))
            return TRUE;
    }

    return FALSE;
}

static const WCHAR *vendor_id_to_manufacturer_string(DWORD vendor_id, const char *pDesc)
{
    static const WCHAR atiW[] = {'A','T','I',' ','T','e','c','h','n','o','l','o','g','i','e','s',' ','I','n','c','.',0};
    static const WCHAR amdW[] = {'A','d','v','a','n','c','e','d',' ','M','i','c','r','o',' ','D','e','v','i','c','e','s',',',' ','I','n','c','.',0};
    static const WCHAR nvidiaW[] = {'N','V','I','D','I','A',0};
    static const WCHAR intelW[] = {'I','n','t','e','l',' ','C','o','r','p','o','r','a','t','i','o','n',0};
    static const WCHAR unknownW[] = {'U','n','k','n','o','w','n',0};

    /* Enumeration copied from dlls/wined3d/wined3d_private.h and slightly modified. */
    enum pci_vendor
    {
        HW_VENDOR_AMD = 0x1002,
        HW_VENDOR_NVIDIA = 0x10de,
        HW_VENDOR_INTEL = 0x8086,
    };

    switch (vendor_id)
    {
    case HW_VENDOR_AMD:
        if (strstr(pDesc, "ATI") != NULL)
            return atiW;
        return amdW;
    case HW_VENDOR_NVIDIA:
        return nvidiaW;
    case HW_VENDOR_INTEL:
        return intelW;
    default:
        FIXME("Unknown PCI vendor ID 0x%04x\n", vendor_id);
        return unknownW;
    }
}

#define add_bstr_property add_prop_str
#define add_ui4_property add_prop_ui4

static const WCHAR szEmpty[] = {0};
static const WCHAR szDescription[] = {'s','z','D','e','s','c','r','i','p','t','i','o','n',0};
static const WCHAR szDeviceName[] = {'s','z','D','e','v','i','c','e','N','a','m','e',0};
static const WCHAR szKeyDeviceID[] = {'s','z','K','e','y','D','e','v','i','c','e','I','D',0};
static const WCHAR szKeyDeviceKey[] = {'s','z','K','e','y','D','e','v','i','c','e','K','e','y',0};
static const WCHAR szVendorId[] = {'s','z','V','e','n','d','o','r','I','d',0};
static const WCHAR szDeviceId[] = {'s','z','D','e','v','i','c','e','I','d',0};
static const WCHAR szDeviceIdentifier[] = {'s','z','D','e','v','i','c','e','I','d','e','n','t','i','f','i','e','r',0};
static const WCHAR dwWidth[] = {'d','w','W','i','d','t','h',0};
static const WCHAR dwHeight[] = {'d','w','H','e','i','g','h','t',0};
static const WCHAR dwBpp[] = {'d','w','B','p','p',0};
static const WCHAR szDisplayMemoryLocalized[] = {'s','z','D','i','s','p','l','a','y','M','e','m','o','r','y','L','o','c','a','l','i','z','e','d',0};
static const WCHAR szDisplayMemoryEnglish[] = {'s','z','D','i','s','p','l','a','y','M','e','m','o','r','y','E','n','g','l','i','s','h',0};
static const WCHAR szDriverName[] = {'s','z','D','r','i','v','e','r','N','a','m','e',0};
static const WCHAR szDriverVersion[] = {'s','z','D','r','i','v','e','r','V','e','r','s','i','o','n',0};
static const WCHAR szSubSysId[] = {'s','z','S','u','b','S','y','s','I','d',0};
static const WCHAR szRevisionId[] = {'s','z','R','e','v','i','s','i','o','n','I','d',0};
static const WCHAR dwRefreshRate[] = {'d','w','R','e','f','r','e','s','h','R','a','t','e',0};
static const WCHAR szManufacturer[] = {'s','z','M','a','n','u','f','a','c','t','u','r','e','r',0};
static const WCHAR iAdapter[] = {'i','A','d','a','p','t','e','r',0};

static HRESULT fill_display_information_d3d(IDxDiagContainer *pSubCont)
{
    IDxDiagContainer *display_adapter;
    HRESULT hr;
    IDirect3D9 *pDirect3D9;
    WCHAR buffer[256];
    UINT index, count;

    pDirect3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pDirect3D9)
        return E_FAIL;

    count = IDirect3D9_GetAdapterCount(pDirect3D9);
    for (index = 0; index < count; index++)
    {
        static const WCHAR adapterid_fmtW[] = {'%','u',0};
        static const WCHAR driverversion_fmtW[] = {'%','u','.','%','u','.','%','0','4','u','.','%','0','4','u',0};
        static const WCHAR id_fmtW[] = {'0','x','%','0','4','x',0};
        static const WCHAR subsysid_fmtW[] = {'0','x','%','0','8','x',0};
        static const WCHAR mem_fmt[] = {'%','.','1','f',' ','M','B',0};

        D3DADAPTER_IDENTIFIER9 adapter_info;
        D3DDISPLAYMODE adapter_mode;
        DWORD available_mem = 0;

        snprintfW(buffer, sizeof(buffer)/sizeof(WCHAR), adapterid_fmtW, index);

        hr = DXDiag_CreateDXDiagContainer( &IID_IDxDiagContainer, (void**) &display_adapter );
        if (FAILED( hr )) return hr;
        hr = IDxDiagContainerImpl_AddChildContainer( pSubCont, buffer, display_adapter );
        if (FAILED( hr )) return hr;

        hr = IDirect3D9_GetAdapterIdentifier(pDirect3D9, index, 0, &adapter_info);
        if (SUCCEEDED(hr))
        {
            WCHAR driverW[sizeof(adapter_info.Driver)];
            WCHAR descriptionW[sizeof(adapter_info.Description)];
            WCHAR devicenameW[sizeof(adapter_info.DeviceName)];

            MultiByteToWideChar(CP_ACP, 0, adapter_info.Driver, -1, driverW, sizeof(driverW)/sizeof(WCHAR));
            MultiByteToWideChar(CP_ACP, 0, adapter_info.Description, -1, descriptionW, sizeof(descriptionW)/sizeof(WCHAR));
            MultiByteToWideChar(CP_ACP, 0, adapter_info.DeviceName, -1, devicenameW, sizeof(devicenameW)/sizeof(WCHAR));

            hr = add_bstr_property(display_adapter, szDriverName, driverW);
            if (FAILED(hr))
                goto cleanup;

            hr = add_bstr_property(display_adapter, szDescription, descriptionW);
            if (FAILED(hr))
                goto cleanup;

            hr = add_bstr_property(display_adapter, szDeviceName, devicenameW);
            if (FAILED(hr))
                goto cleanup;

            snprintfW(buffer, sizeof(buffer)/sizeof(WCHAR), driverversion_fmtW,
                      HIWORD(adapter_info.DriverVersion.u.HighPart), LOWORD(adapter_info.DriverVersion.u.HighPart),
                      HIWORD(adapter_info.DriverVersion.u.LowPart), LOWORD(adapter_info.DriverVersion.u.LowPart));

            hr = add_bstr_property(display_adapter, szDriverVersion, buffer);
            if (FAILED(hr))
                goto cleanup;

            snprintfW(buffer, sizeof(buffer)/sizeof(WCHAR), id_fmtW, adapter_info.VendorId);
            hr = add_bstr_property(display_adapter, szVendorId, buffer);
            if (FAILED(hr))
                goto cleanup;

            snprintfW(buffer, sizeof(buffer)/sizeof(WCHAR), id_fmtW, adapter_info.DeviceId);
            hr = add_bstr_property(display_adapter, szDeviceId, buffer);
            if (FAILED(hr))
                goto cleanup;

            snprintfW(buffer, sizeof(buffer)/sizeof(WCHAR), subsysid_fmtW, adapter_info.SubSysId);
            hr = add_bstr_property(display_adapter, szSubSysId, buffer);
            if (FAILED(hr))
                goto cleanup;

            snprintfW(buffer, sizeof(buffer)/sizeof(WCHAR), id_fmtW, adapter_info.Revision);
            hr = add_bstr_property(display_adapter, szRevisionId, buffer);
            if (FAILED(hr))
                goto cleanup;

            StringFromGUID2(&adapter_info.DeviceIdentifier, buffer, 39);
            hr = add_bstr_property(display_adapter, szDeviceIdentifier, buffer);
            if (FAILED(hr))
                goto cleanup;

            hr = add_bstr_property(display_adapter, szManufacturer,
                                   vendor_id_to_manufacturer_string(adapter_info.VendorId, adapter_info.Description));
            if (FAILED(hr))
                goto cleanup;
        }

        hr = IDirect3D9_GetAdapterDisplayMode(pDirect3D9, index, &adapter_mode);
        if (SUCCEEDED(hr))
        {
            hr = add_ui4_property(display_adapter, dwWidth, adapter_mode.Width);
            if (FAILED(hr))
                goto cleanup;

            hr = add_ui4_property(display_adapter, dwHeight, adapter_mode.Height);
            if (FAILED(hr))
                goto cleanup;

            hr = add_ui4_property(display_adapter, dwRefreshRate, adapter_mode.RefreshRate);
            if (FAILED(hr))
                goto cleanup;

            hr = add_ui4_property(display_adapter, dwBpp, depth_for_pixelformat(adapter_mode.Format));
            if (FAILED(hr))
                goto cleanup;
        }

        hr = add_bstr_property(display_adapter, szKeyDeviceKey, szEmpty);
        if (FAILED(hr))
            goto cleanup;

        hr = add_bstr_property(display_adapter, szKeyDeviceID, szEmpty);
        if (FAILED(hr))
            goto cleanup;

        if (!get_texture_memory(&adapter_info.DeviceIdentifier, &available_mem))
            WARN("get_texture_memory helper failed\n");

        snprintfW(buffer, sizeof(buffer)/sizeof(buffer[0]), mem_fmt, available_mem / 1000000.0f);

        hr = add_bstr_property(display_adapter, szDisplayMemoryLocalized, buffer);
        if (FAILED(hr))
            goto cleanup;

        hr = add_bstr_property(display_adapter, szDisplayMemoryEnglish, buffer);
        if (FAILED(hr))
            goto cleanup;

        add_prop_ui4(display_adapter, iAdapter, index);
    }

    hr = S_OK;
cleanup:
    IDirect3D9_Release(pDirect3D9);
    return hr;
}

static HRESULT DXDiag_InitDXDiagDisplayContainer(IDxDiagContainer* pSubCont)
{
    return fill_display_information_d3d(pSubCont);
}

static HRESULT DXDiag_InitDXDiagDirectSoundContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
  static const WCHAR DxDiag_SoundDevices[] = {'D','x','D','i','a','g','_','S','o','u','n','d','D','e','v','i','c','e','s',0};
  static const WCHAR DxDiag_SoundCaptureDevices[] = {'D','x','D','i','a','g','_','S','o','u','n','d','C','a','p','t','u','r','e','D','e','v','i','c','e','s',0};
  IDxDiagContainer* pSubSubCont = NULL;

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubSubCont);
  if (FAILED(hr)) { return hr; }
  hr = IDxDiagContainerImpl_AddChildContainer(pSubCont, DxDiag_SoundDevices, pSubSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubSubCont);
  if (FAILED(hr)) { return hr; }
  hr = IDxDiagContainerImpl_AddChildContainer(pSubCont, DxDiag_SoundCaptureDevices, pSubSubCont);

  return hr;
}

static HRESULT DXDiag_InitDXDiagDirectMusicContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
  return hr;
}

static HRESULT DXDiag_InitDXDiagDirectInputContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
  return hr;
}

static HRESULT DXDiag_InitDXDiagDirectPlayContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
  return hr;
}

static HRESULT DXDiag_InitDXDiagDirectShowFiltersContainer(IDxDiagContainer* pSubCont) {
  HRESULT hr = S_OK;
#if 0
  static const WCHAR szName[] = {'s','z','N','a','m','e',0};
  static const WCHAR szVersionW[] = {'s','z','V','e','r','s','i','o','n',0};
  static const WCHAR szCatName[] = {'s','z','C','a','t','N','a','m','e',0};
  static const WCHAR ClsidCatW[] = {'C','l','s','i','d','C','a','t',0};
  static const WCHAR ClsidFilterW[] = {'C','l','s','i','d','F','i','l','t','e','r',0};
  static const WCHAR dwInputs[] = {'d','w','I','n','p','u','t','s',0};
  static const WCHAR dwOutputs[] = {'d','w','O','u','t','p','u','t','s',0};
  static const WCHAR dwMeritW[] = {'d','w','M','e','r','i','t',0};
  /*
  static const WCHAR szFileName[] = {'s','z','F','i','l','e','N','a','m','e',0};
  static const WCHAR szFileVersion[] = {'s','z','F','i','l','e','V','e','r','s','i','o','n',0};
  */
  VARIANT v;

  static const WCHAR wszClsidName[] = {'C','L','S','I','D',0};
  static const WCHAR wszFriendlyName[] = {'F','r','i','e','n','d','l','y','N','a','m','e',0};  
  static const WCHAR wszFilterDataName[] = {'F','i','l','t','e','r','D','a','t','a',0};

  static const WCHAR szVersionFormat[] = {'v','%','d',0};
  static const WCHAR szIdFormat[] = {'%','d',0};
  int i = 0;


  ICreateDevEnum* pCreateDevEnum = NULL;
  IEnumMoniker* pEmCat = NULL;
  IMoniker* pMCat = NULL;
  /** */
  hr = CoCreateInstance(&CLSID_SystemDeviceEnum, 
			NULL, 
			CLSCTX_INPROC_SERVER,
			&IID_ICreateDevEnum, 
			(void**) &pCreateDevEnum);
  if (FAILED(hr)) return hr; 
  
  hr = ICreateDevEnum_CreateClassEnumerator(pCreateDevEnum, &CLSID_ActiveMovieCategories, &pEmCat, 0);
  if (FAILED(hr)) goto out_show_filters; 

  VariantInit(&v);

  while (S_OK == IEnumMoniker_Next(pEmCat, 1, &pMCat, NULL)) {
    IPropertyBag* pPropBag = NULL;
    CLSID clsidCat; 
    hr = IMoniker_BindToStorage(pMCat, NULL, NULL, &IID_IPropertyBag, (void**) &pPropBag);
    if (SUCCEEDED(hr)) {
      WCHAR* wszCatName = NULL;
      WCHAR* wszCatClsid = NULL;

      hr = IPropertyBag_Read(pPropBag, wszFriendlyName, &v, 0);
      wszCatName = SysAllocString(V_BSTR(&v));
      VariantClear(&v);

      hr = IPropertyBag_Read(pPropBag, wszClsidName, &v, 0);
      wszCatClsid = SysAllocString(V_BSTR(&v));
      hr = CLSIDFromString(V_UNION(&v, bstrVal), &clsidCat);
      VariantClear(&v);

      /*
      hr = IPropertyBag_Read(pPropBag, wszMeritName, &v, 0);
      hr = IDxDiagContainerImpl_AddProp(pSubCont, dwMerit, &v);
      VariantClear(&v);
      */

      if (SUCCEEDED(hr)) {
	IEnumMoniker* pEnum = NULL;
	IMoniker* pMoniker = NULL;
        hr = ICreateDevEnum_CreateClassEnumerator(pCreateDevEnum, &clsidCat, &pEnum, 0);        
        TRACE("\tClassEnumerator for clsid(%s) pEnum(%p)\n", debugstr_guid(&clsidCat), pEnum);
        if (FAILED(hr) || pEnum == NULL) {
          goto class_enum_failed;
        }
        while (NULL != pEnum && S_OK == IEnumMoniker_Next(pEnum, 1, &pMoniker, NULL)) {          
	  IPropertyBag* pPropFilterBag = NULL;
          TRACE("\tIEnumMoniker_Next(%p, 1, %p)\n", pEnum, pMoniker);
	  hr = IMoniker_BindToStorage(pMoniker, NULL, NULL, &IID_IPropertyBag, (void**) &pPropFilterBag);
	  if (SUCCEEDED(hr)) {
	    LPBYTE pData = NULL;
	    DWORD dwNOutputs = 0;
	    DWORD dwNInputs = 0;
	    DWORD dwMerit = 0;
            WCHAR bufferW[10];
            IDxDiagContainer *pDShowSubCont = NULL;
            IFilterMapper2 *pFileMapper = NULL;
            IAMFilterData *pFilterData = NULL;

            snprintfW(bufferW, sizeof(bufferW)/sizeof(bufferW[0]), szIdFormat, i);
            if (FAILED(DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pDShowSubCont)) ||
                FAILED(IDxDiagContainerImpl_AddChildContainer(pSubCont, bufferW, pDShowSubCont)))
            {
              IPropertyBag_Release(pPropFilterBag);
              if (pDShowSubCont) IUnknown_Release(pDShowSubCont);
              continue;
            }

            bufferW[0] = 0;
	    hr = IPropertyBag_Read(pPropFilterBag, wszFriendlyName, &v, 0);
	    hr = IDxDiagContainerImpl_AddProp(pDShowSubCont, szName, &v);
            TRACE("\tName:%s\n", debugstr_w(V_BSTR(&v)));
	    VariantClear(&v);

	    hr = IPropertyBag_Read(pPropFilterBag, wszClsidName, &v, 0);
            TRACE("\tClsid:%s\n", debugstr_w(V_BSTR(&v)));
	    hr = IDxDiagContainerImpl_AddProp(pDShowSubCont, ClsidFilterW, &v);
	    VariantClear(&v);

            hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC, &IID_IFilterMapper2,
                                  (LPVOID*)&pFileMapper);
            if (SUCCEEDED(hr) &&
                SUCCEEDED(IFilterMapper2_QueryInterface(pFileMapper, &IID_IAMFilterData, (void **)&pFilterData)))
            {
              DWORD array_size;
              BYTE *tmp;
              REGFILTER2 *pRF = NULL;

              if (SUCCEEDED(IPropertyBag_Read(pPropFilterBag, wszFilterDataName, &v, NULL)) &&
                  SUCCEEDED(SafeArrayAccessData(V_UNION(&v, parray), (LPVOID*) &pData)))
              {
                ULONG j;
                array_size = V_UNION(&v, parray)->rgsabound->cElements;

                if (SUCCEEDED(IAMFilterData_ParseFilterData(pFilterData, pData, array_size, &tmp)))
                {
                  pRF = ((REGFILTER2 **)tmp)[0];

                  snprintfW(bufferW, sizeof(bufferW)/sizeof(bufferW[0]), szVersionFormat, pRF->dwVersion);
                  if (pRF->dwVersion == 1)
                  {
                    for (j = 0; j < pRF->u.s.cPins; j++)
                      if (pRF->u.s.rgPins[j].bOutput)
                        dwNOutputs++;
                      else
                        dwNInputs++;
                  }
                  else if (pRF->dwVersion == 2)
                  {
                    for (j = 0; j < pRF->u.s1.cPins2; j++)
                      if (pRF->u.s1.rgPins2[j].dwFlags & REG_PINFLAG_B_OUTPUT)
                        dwNOutputs++;
                      else
                        dwNInputs++;
                  }

                  dwMerit = pRF->dwMerit;
                  CoTaskMemFree(tmp);
                }

                SafeArrayUnaccessData(V_UNION(&v, parray));
                VariantClear(&v);
              }
              IFilterMapper2_Release(pFilterData);
            }
            if (pFileMapper) IFilterMapper2_Release(pFileMapper);

            add_prop_str(pDShowSubCont, szVersionW, bufferW);
            add_prop_str(pDShowSubCont, szCatName, wszCatName);
            add_prop_str(pDShowSubCont, ClsidCatW, wszCatClsid);
            add_prop_ui4(pDShowSubCont, dwInputs,  dwNInputs);
            add_prop_ui4(pDShowSubCont, dwOutputs, dwNOutputs);
            add_prop_ui4(pDShowSubCont, dwMeritW, dwMerit);

            i++;
	  }
	  IPropertyBag_Release(pPropFilterBag); pPropFilterBag = NULL;
	}
	IEnumMoniker_Release(pEnum); pEnum = NULL;
      }
class_enum_failed:      
      SysFreeString(wszCatName);
      SysFreeString(wszCatClsid);
      IPropertyBag_Release(pPropBag); pPropBag = NULL;
    }
    IEnumMoniker_Release(pMCat); pMCat = NULL;
  }

out_show_filters:
  if (NULL != pEmCat) { IEnumMoniker_Release(pEmCat); pEmCat = NULL; }
  if (NULL != pCreateDevEnum) { ICreateDevEnum_Release(pCreateDevEnum); pCreateDevEnum = NULL; }
#endif
  return hr;
}

static HRESULT DXDiag_InitRootDXDiagContainer(IDxDiagContainer* pRootCont) {
  HRESULT hr = S_OK;
  static const WCHAR DxDiag_SystemInfo[] = {'D','x','D','i','a','g','_','S','y','s','t','e','m','I','n','f','o',0};
  static const WCHAR DxDiag_SystemDevices[] = {'D','x','D','i','a','g','_','S','y','s','t','e','m','D','e','v','i','c','e','s',0};
  static const WCHAR DxDiag_LogicalDisks[] = {'D','x','D','i','a','g','_','L','o','g','i','c','a','l','D','i','s','k','s',0};
  static const WCHAR DxDiag_DirectXFiles[] = {'D','x','D','i','a','g','_','D','i','r','e','c','t','X','F','i','l','e','s',0};
  static const WCHAR DxDiag_DisplayDevices[] = {'D','x','D','i','a','g','_','D','i','s','p','l','a','y','D','e','v','i','c','e','s',0};
  static const WCHAR DxDiag_DirectSound[] = {'D','x','D','i','a','g','_','D','i','r','e','c','t','S','o','u','n','d',0};
  static const WCHAR DxDiag_DirectMusic[] = {'D','x','D','i','a','g','_','D','i','r','e','c','t','M','u','s','i','c',0};
  static const WCHAR DxDiag_DirectInput[] = {'D','x','D','i','a','g','_','D','i','r','e','c','t','I','n','p','u','t',0};
  static const WCHAR DxDiag_DirectPlay[] = {'D','x','D','i','a','g','_','D','i','r','e','c','t','P','l','a','y',0};
  static const WCHAR DxDiag_DirectShowFilters[] = {'D','x','D','i','a','g','_','D','i','r','e','c','t','S','h','o','w','F','i','l','t','e','r','s',0};
  IDxDiagContainer* pSubCont = NULL;
  
  TRACE("(%p)\n", pRootCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagSystemInfoContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_SystemInfo, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagSystemDevicesContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_SystemDevices, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagLogicalDisksContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_LogicalDisks, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDirectXFilesContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DirectXFiles, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDisplayContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DisplayDevices, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDirectSoundContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DirectSound, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDirectMusicContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DirectMusic, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDirectInputContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DirectInput, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDirectPlayContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DirectPlay, pSubCont);

  hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, (void**) &pSubCont);
  if (FAILED(hr)) { return hr; }
  hr = DXDiag_InitDXDiagDirectShowFiltersContainer(pSubCont);
  hr = IDxDiagContainerImpl_AddChildContainer(pRootCont, DxDiag_DirectShowFilters, pSubCont);

  return hr;
}
