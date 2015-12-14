/*
 * COM proxy/stub factory (CStdPSFactory) implementation
 *
 * Copyright 2001 Ove K�ven, TransGaming Technologies
 */

#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"

#include "objbase.h"

#include "rpcproxy.h"

#include "wine/debug.h"

#include "cpsf.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

static BOOL FindProxyInfo(const ProxyFileInfo **pProxyFileList, REFIID riid, const ProxyFileInfo **pProxyInfo, int *pIndex)
{
  while (*pProxyFileList) {
    if ((*pProxyFileList)->pIIDLookupRtn(riid, pIndex)) {
      *pProxyInfo = *pProxyFileList;
      TRACE("found: ProxyInfo %p Index %d\n", *pProxyInfo, *pIndex);
      return TRUE;
    }
    pProxyFileList++;
  }
  TRACE("not found\n");
  return FALSE;
}

static HRESULT WINAPI CStdPSFactory_QueryInterface(LPPSFACTORYBUFFER iface,
                                                  REFIID riid,
                                                  LPVOID *obj)
{
  ICOM_THIS(CStdPSFactoryBuffer,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",iface,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IPSFactoryBuffer,riid)) {
    *obj = This;
    This->RefCount++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI CStdPSFactory_AddRef(LPPSFACTORYBUFFER iface)
{
  ICOM_THIS(CStdPSFactoryBuffer,iface);
  TRACE("(%p)->AddRef()\n",iface);
  return ++(This->RefCount);
}

static ULONG WINAPI CStdPSFactory_Release(LPPSFACTORYBUFFER iface)
{
  ICOM_THIS(CStdPSFactoryBuffer,iface);
  TRACE("(%p)->Release()\n",iface);
  return --(This->RefCount);
}

static HRESULT WINAPI CStdPSFactory_CreateProxy(LPPSFACTORYBUFFER iface,
                                               LPUNKNOWN pUnkOuter,
                                               REFIID riid,
                                               LPRPCPROXYBUFFER *ppProxy,
                                               LPVOID *ppv)
{
  ICOM_THIS(CStdPSFactoryBuffer,iface);
  const ProxyFileInfo *ProxyInfo;
  int Index;
  TRACE("(%p)->CreateProxy(%p,%s,%p,%p)\n",iface,pUnkOuter,
       debugstr_guid(riid),ppProxy,ppv);
  if (!FindProxyInfo(This->pProxyFileList,riid,&ProxyInfo,&Index))
    return E_NOINTERFACE;
  return StdProxy_Construct(riid, pUnkOuter, ProxyInfo->pNamesArray[Index],
                            ProxyInfo->pProxyVtblList[Index],
                            ProxyInfo->pStubVtblList[Index], iface, ppProxy, ppv);
}

static HRESULT WINAPI CStdPSFactory_CreateStub(LPPSFACTORYBUFFER iface,
                                              REFIID riid,
                                              LPUNKNOWN pUnkServer,
                                              LPRPCSTUBBUFFER *ppStub)
{
  ICOM_THIS(CStdPSFactoryBuffer,iface);
  const ProxyFileInfo *ProxyInfo;
  int Index;
  TRACE("(%p)->CreateStub(%s,%p,%p)\n",iface,debugstr_guid(riid),
       pUnkServer,ppStub);
  if (!FindProxyInfo(This->pProxyFileList,riid,&ProxyInfo,&Index))
    return E_NOINTERFACE;
  return CStdStubBuffer_Construct(riid, pUnkServer, ProxyInfo->pNamesArray[Index],
                                  ProxyInfo->pStubVtblList[Index], iface, ppStub);
}

static ICOM_VTABLE(IPSFactoryBuffer) CStdPSFactory_Vtbl =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  CStdPSFactory_QueryInterface,
  CStdPSFactory_AddRef,
  CStdPSFactory_Release,
  CStdPSFactory_CreateProxy,
  CStdPSFactory_CreateStub
};

/***********************************************************************
 *           NdrDllGetClassObject [RPCRT4.@]
 */
HRESULT WINAPI NdrDllGetClassObject(REFCLSID rclsid, REFIID iid, LPVOID *ppv,
                                   const ProxyFileInfo **pProxyFileList,
                                   const CLSID *pclsid,
                                   CStdPSFactoryBuffer *pPSFactoryBuffer)
{
  *ppv = NULL;
  if (!pPSFactoryBuffer->lpVtbl) {
    pPSFactoryBuffer->lpVtbl = &CStdPSFactory_Vtbl;
    pPSFactoryBuffer->RefCount = 0;
    pPSFactoryBuffer->pProxyFileList = pProxyFileList;
  }
  if (IsEqualGUID(rclsid, pclsid))
    return IPSFactoryBuffer_QueryInterface((LPPSFACTORYBUFFER)pPSFactoryBuffer, iid, ppv);
  return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *           NdrDllCanUnloadNow [RPCRT4.@]
 */
HRESULT WINAPI NdrDllCanUnloadNow(CStdPSFactoryBuffer *pPSFactoryBuffer)
{
  return !(pPSFactoryBuffer->RefCount);
}

/***********************************************************************
 *           NdrDllRegisterProxy [RPCRT4.@]
 */
HRESULT WINAPI NdrDllRegisterProxy(HMODULE hDll,
                                  const ProxyFileInfo **pProxyFileList,
                                  const CLSID *pclsid)
{
  LPSTR clsid;
  char keyname[120], module[120];
  HKEY key, subkey;
  DWORD error;

  TRACE("(%x,%p,%s)\n", hDll, pProxyFileList, debugstr_guid(pclsid));
  UuidToStringA((UUID*)pclsid, &clsid);

  /* register interfaces to point to clsid */
  while (*pProxyFileList) {
    unsigned u;
    for (u=0; u<(*pProxyFileList)->TableSize; u++) {
      CInterfaceStubVtbl *proxy = (*pProxyFileList)->pStubVtblList[u];
      PCInterfaceName name = (*pProxyFileList)->pNamesArray[u];
      LPSTR iid;

      TRACE("registering %s %s => %s\n", name, debugstr_guid(proxy->header.piid), clsid);

      UuidToStringA((UUID*)proxy->header.piid, &iid);
      snprintf(keyname, sizeof(keyname), "Interface\\{%s}", iid);
      RpcStringFreeA(&iid);
      if (RegCreateKeyExA(HKEY_CLASSES_ROOT, keyname, 0, NULL, 0,
                          KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
        if (name)
          RegSetValueExA(key, NULL, 0, REG_SZ, name, strlen(name));
        if (RegCreateKeyExA(key, "ProxyStubClsid32", 0, NULL, 0,
                            KEY_WRITE, NULL, &subkey, NULL) == ERROR_SUCCESS) {
          snprintf(module, sizeof(module), "{%s}", clsid);
          RegSetValueExA(subkey, NULL, 0, REG_SZ, module, strlen(module));
          RegCloseKey(subkey);
        }
        RegCloseKey(key);
      }
    }
    pProxyFileList++;
  }

  /* register clsid to point to module */
  snprintf(keyname, sizeof(keyname), "CLSID\\{%s}", clsid);
  error = GetModuleFileNameA(hDll, module, sizeof(module));

  if (error == sizeof(module))
      ERR("the buffer for GetModuleFileNameA() is too small!\n");

  else if (error == 0){
      ERR("could not retrieve the module filename for 0x%lx\n", hDll);

      return E_FAIL;
  }


  TRACE("registering CLSID %s => %s\n", clsid, module);
  if (RegCreateKeyExA(HKEY_CLASSES_ROOT, keyname, 0, NULL, 0,
                      KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
     if (RegCreateKeyExA(key, "InProcServer32", 0, NULL, 0,
                         KEY_WRITE, NULL, &subkey, NULL) == ERROR_SUCCESS) {
       RegSetValueExA(subkey, NULL, 0, REG_SZ, module, strlen(module));
       RegCloseKey(subkey);
     }
     RegCloseKey(key);
  }

  /* done */
  RpcStringFreeA(&clsid);
  return S_OK;
}

/***********************************************************************
 *           NdrDllUnregisterProxy [RPCRT4.@]
 */
HRESULT WINAPI NdrDllUnregisterProxy(HMODULE hDll,
                                    const ProxyFileInfo **pProxyFileList,
                                    const CLSID *pclsid)
{
  LPSTR clsid;
  char keyname[120], module[120];
  DWORD error;

  TRACE("(%x,%p,%s)\n", hDll, pProxyFileList, debugstr_guid(pclsid));
  UuidToStringA((UUID*)pclsid, &clsid);

  /* unregister interfaces */
  while (*pProxyFileList) {
    unsigned u;
    for (u=0; u<(*pProxyFileList)->TableSize; u++) {
      CInterfaceStubVtbl *proxy = (*pProxyFileList)->pStubVtblList[u];
      PCInterfaceName name = (*pProxyFileList)->pNamesArray[u];
      LPSTR iid;

      TRACE("unregistering %s %s <= %s\n", name, debugstr_guid(proxy->header.piid), clsid);

      UuidToStringA((UUID*)proxy->header.piid, &iid);
      snprintf(keyname, sizeof(keyname), "Interface\\{%s}", iid);
      RpcStringFreeA(&iid);
      RegDeleteKeyA(HKEY_CLASSES_ROOT, keyname);
    }
    pProxyFileList++;
  }

  /* unregister clsid */
  snprintf(keyname, sizeof(keyname), "CLSID\\{%s}", clsid);
  error = GetModuleFileNameA(hDll, module, sizeof(module));

  if (error == sizeof(module))
      ERR("the buffer for GetModuleFileNameA() is too small!\n");
    
  else if (error == 0){
      ERR("could not retrieve the module filename for 0x%lx\n", hDll);

      return E_FAIL;
  }


  TRACE("unregistering CLSID %s <= %s\n", clsid, module);
  RegDeleteKeyA(HKEY_CLASSES_ROOT, keyname);

  /* done */
  RpcStringFreeA(&clsid);
  return S_OK;
}
