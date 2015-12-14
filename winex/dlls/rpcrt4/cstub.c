/*
 * COM stub (CStdStubBuffer) implementation
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "windef.h"
#include "winbase.h"
#include "winerror.h"

#include "objbase.h"

#include "rpcproxy.h"

#include "wine/debug.h"

#include "cpsf.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

#define STUB_HEADER(This) (((CInterfaceStubHeader*)((This)->lpVtbl))[-1])

HRESULT WINAPI CStdStubBuffer_Construct(REFIID riid,
                                       LPUNKNOWN pUnkServer,
                                       PCInterfaceName name,
                                       CInterfaceStubVtbl *vtbl,
                                       LPPSFACTORYBUFFER pPSFactory,
                                       LPRPCSTUBBUFFER *ppStub)
{
  CStdStubBuffer *This;

  TRACE("(%p,%p,%p,%p) %s\n", pUnkServer, vtbl, pPSFactory, ppStub, name);
  TRACE("iid=%s\n", debugstr_guid(vtbl->header.piid));
  TRACE("vtbl=%p\n", &vtbl->Vtbl);

  if (!IsEqualGUID(vtbl->header.piid, riid)) {
    ERR("IID mismatch during stub creation\n");
    return RPC_E_UNEXPECTED;
  }

  This = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(CStdStubBuffer));
  if (!This) return E_OUTOFMEMORY;

  This->lpVtbl = &vtbl->Vtbl;
  This->RefCount = 1;
  This->pvServerObject = pUnkServer;
  This->pPSFactory = pPSFactory;
  *ppStub = (LPRPCSTUBBUFFER)This;

  IUnknown_AddRef(This->pvServerObject);
  IPSFactoryBuffer_AddRef(pPSFactory);
  return S_OK;
}

HRESULT WINAPI CStdStubBuffer_QueryInterface(LPRPCSTUBBUFFER iface,
                                            REFIID riid,
                                            LPVOID *obj)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);

  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IRpcStubBuffer,riid)) {
    *obj = This;
    This->RefCount++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG WINAPI CStdStubBuffer_AddRef(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->AddRef()\n",This);
  return ++(This->RefCount);
}

ULONG WINAPI NdrCStdStubBuffer_Release(LPRPCSTUBBUFFER iface,
                                      LPPSFACTORYBUFFER pPSF)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->Release()\n",This);

  if (!--(This->RefCount)) {
    IUnknown_Release(This->pvServerObject);
    IPSFactoryBuffer_Release(This->pPSFactory);
    HeapFree(GetProcessHeap(),0,This);
    return 0;
  }
  return This->RefCount;
}

HRESULT WINAPI CStdStubBuffer_Connect(LPRPCSTUBBUFFER iface,
                                     LPUNKNOWN lpUnkServer)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->Connect(%p)\n",This,lpUnkServer);
  This->pvServerObject = lpUnkServer;
  return S_OK;
}

void WINAPI CStdStubBuffer_Disconnect(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->Disconnect()\n",This);
  This->pvServerObject = NULL;
}

HRESULT WINAPI CStdStubBuffer_Invoke(LPRPCSTUBBUFFER iface,
                                    PRPCOLEMESSAGE pMsg,
                                    LPRPCCHANNELBUFFER pChannel)
{
  ICOM_THIS(CStdStubBuffer,iface);
  DWORD dwPhase = STUB_UNMARSHAL;
  TRACE("(%p)->Invoke(%p,%p)\n",This,pMsg,pChannel);

  STUB_HEADER(This).pDispatchTable[pMsg->iMethod](iface, pChannel, (PRPC_MESSAGE)pMsg, &dwPhase);
  return S_OK;
}

LPRPCSTUBBUFFER WINAPI CStdStubBuffer_IsIIDSupported(LPRPCSTUBBUFFER iface,
                                                    REFIID riid)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->IsIIDSupported(%s)\n",This,debugstr_guid(riid));
  return IsEqualGUID(STUB_HEADER(This).piid, riid) ? iface : NULL;
}

ULONG WINAPI CStdStubBuffer_CountRefs(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->CountRefs()\n",This);
  return This->RefCount;
}

HRESULT WINAPI CStdStubBuffer_DebugServerQueryInterface(LPRPCSTUBBUFFER iface,
                                                       LPVOID *ppv)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->DebugServerQueryInterface(%p)\n",This,ppv);
  return S_OK;
}

void WINAPI CStdStubBuffer_DebugServerRelease(LPRPCSTUBBUFFER iface,
                                             LPVOID pv)
{
  ICOM_THIS(CStdStubBuffer,iface);
  TRACE("(%p)->DebugServerRelease(%p)\n",This,pv);
}
