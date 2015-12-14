/*
 * COM proxy implementation
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * TODO: Handle non-i386 architectures
 *       Get rid of #if 0'ed code.
 */

#include "windef.h"
#include "winbase.h"
#include "winerror.h"

#include "objbase.h"

#include "rpcproxy.h"

#include "cpsf.h"
#include "ndr_misc.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

struct StublessThunk;

/* I don't know what MS's std proxy structure looks like,
   so this probably doesn't match, but that shouldn't matter */
typedef struct {
  ICOM_VTABLE(IRpcProxyBuffer) *lpVtbl;
  LPVOID *PVtbl;
  DWORD RefCount;
  const MIDL_STUBLESS_PROXY_INFO *stubless;
  const IID* piid;
  LPUNKNOWN pUnkOuter;
  PCInterfaceName name;
  LPPSFACTORYBUFFER pPSFactory;
  LPRPCCHANNELBUFFER pChannel;
  struct StublessThunk *thunks;
} StdProxyImpl;

static ICOM_VTABLE(IRpcProxyBuffer) StdProxy_Vtbl;

/* How the Windows stubless proxy thunks work is explained at
 * http://msdn.microsoft.com/library/en-us/dnmsj99/html/com0199.asp,
 * but I'll use a slightly different method, to make life easier */

#if defined(__i386__)

struct StublessThunk {
  BYTE push WINE_PACKED;
  DWORD index WINE_PACKED;
  BYTE call WINE_PACKED;
  LONG handler WINE_PACKED;
  BYTE ret WINE_PACKED;
  WORD bytes WINE_PACKED;
  BYTE pad[3];
};

/* adjust the stack size since we don't use Windows's method */
#define STACK_ADJUST sizeof(DWORD)

#define FILL_STUBLESS(x,idx,stk) \
 x->push = 0x68; /* pushl [immediate] */ \
 x->index = (idx); \
 x->call = 0xe8; /* call [near] */ \
 x->handler = (char*)ObjectStubless - (char*)&x->ret; \
 x->ret = 0xc2; /* ret [immediate] */ \
 x->bytes = stk; \
 x->pad[0] = 0x8d; /* leal (%esi),%esi */ \
 x->pad[1] = 0x76; \
 x->pad[2] = 0x00;

#else
/* can't do that on this arch */
struct ObjectStubless {};
#define FILL_STUBLESS(x,idx) \
 ERR("stubless proxies are not supported on this architecture\n");
#endif

static HRESULT WINAPI ObjectStubless(DWORD index)
{
  char *args = (char*)(&index + 2);
  LPVOID iface = *(LPVOID*)args;

  ICOM_THIS_MULTI(StdProxyImpl,PVtbl,iface);

  PFORMAT_STRING fs = This->stubless->ProcFormatString + This->stubless->FormatStringOffset[index];
  unsigned bytes = *(WORD*)(fs+8) - STACK_ADJUST;
  TRACE("(%p)->(%ld)([%d bytes]) ret=%08lx\n", iface, index, bytes, *(DWORD*)(args+bytes));

  return RPCRT4_NdrClientCall2(This->stubless->pStubDesc, fs, args);
}

HRESULT WINAPI StdProxy_Construct(REFIID riid,
                                 LPUNKNOWN pUnkOuter,
                                 PCInterfaceName name,
                                 CInterfaceProxyVtbl *vtbl,
                                 CInterfaceStubVtbl *svtbl,
                                 LPPSFACTORYBUFFER pPSFactory,
                                 LPRPCPROXYBUFFER *ppProxy,
                                 LPVOID *ppvObj)
{
  StdProxyImpl *This;
  const MIDL_STUBLESS_PROXY_INFO *stubless = NULL;

  TRACE("(%p,%p,%p,%p,%p) %s\n", pUnkOuter, vtbl, pPSFactory, ppProxy, ppvObj, name);

  /* I can't find any other way to detect stubless proxies than this hack */
  if (!IsEqualGUID(vtbl->header.piid, riid)) {
    stubless = *((const void **)vtbl);
    vtbl = (CInterfaceProxyVtbl *)((const void **)vtbl + 1);
    TRACE("stubless=%p\n", stubless);
  }

  TRACE("iid=%s\n", debugstr_guid(vtbl->header.piid));
  TRACE("vtbl=%p\n", vtbl->Vtbl);

  if (!IsEqualGUID(vtbl->header.piid, riid)) {
    ERR("IID mismatch during proxy creation\n");
    return RPC_E_UNEXPECTED;
  }

  This = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(StdProxyImpl));
  if (!This) return E_OUTOFMEMORY;

  if (stubless) {
    unsigned i, count = svtbl->header.DispatchTableCount;
    /* Maybe the original vtbl is just modified directly to point at
     * ObjectStublessClientXXX thunks in real Windows, but I don't like it
     */
    TRACE("stubless thunks: count=%d\n", count);
    This->thunks = HeapAlloc(GetProcessHeap(),0,sizeof(struct StublessThunk)*count);
    This->PVtbl = HeapAlloc(GetProcessHeap(),0,sizeof(LPVOID)*count);
    for (i=0; i<count; i++) {
      struct StublessThunk *thunk = &This->thunks[i];
      if (vtbl->Vtbl[i] == (LPVOID)-1) {
        PFORMAT_STRING fs = stubless->ProcFormatString + stubless->FormatStringOffset[i];
        unsigned bytes = *(WORD*)(fs+8) - STACK_ADJUST;
        TRACE("method %d: stacksize=%d\n", i, bytes);
        FILL_STUBLESS(thunk, i, bytes)
        This->PVtbl[i] = thunk;
      }
      else {
        memset(thunk, 0, sizeof(struct StublessThunk));
        This->PVtbl[i] = vtbl->Vtbl[i];
      }
    }
  }
  else
    This->PVtbl = vtbl->Vtbl;

  This->lpVtbl = &StdProxy_Vtbl;
  This->RefCount = 1;
  This->stubless = stubless;
  This->piid = vtbl->header.piid;
  This->pUnkOuter = pUnkOuter;
  This->name = name;
  This->pPSFactory = pPSFactory;
  This->pChannel = NULL;
  *ppProxy = (LPRPCPROXYBUFFER)&This->lpVtbl;
  *ppvObj = &This->PVtbl;
  IPSFactoryBuffer_AddRef(pPSFactory);

  return S_OK;
}

static void WINAPI StdProxy_Destruct(LPRPCPROXYBUFFER iface)
{
  ICOM_THIS_MULTI(StdProxyImpl,lpVtbl,iface);

  IPSFactoryBuffer_Release(This->pPSFactory);
  if (This->thunks) {
    HeapFree(GetProcessHeap(),0,This->PVtbl);
    HeapFree(GetProcessHeap(),0,This->thunks);
  }
  HeapFree(GetProcessHeap(),0,This);
}

static HRESULT WINAPI StdProxy_QueryInterface(LPRPCPROXYBUFFER iface,
                                             REFIID riid,
                                             LPVOID *obj)
{
  ICOM_THIS_MULTI(StdProxyImpl,lpVtbl,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);

  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(This->piid,riid)) {
    *obj = &This->PVtbl;
    This->RefCount++;
    return S_OK;
  }

  if (IsEqualGUID(&IID_IRpcProxyBuffer,riid)) {
    *obj = &This->lpVtbl;
    This->RefCount++;
    return S_OK;
  }

  return E_NOINTERFACE;
}

static ULONG WINAPI StdProxy_AddRef(LPRPCPROXYBUFFER iface)
{
  ICOM_THIS_MULTI(StdProxyImpl,lpVtbl,iface);
  TRACE("(%p)->AddRef()\n",This);

  return ++(This->RefCount);
}

static ULONG WINAPI StdProxy_Release(LPRPCPROXYBUFFER iface)
{
  ICOM_THIS_MULTI(StdProxyImpl,lpVtbl,iface);
  TRACE("(%p)->Release()\n",This);

  if (!--(This->RefCount)) {
    StdProxy_Destruct((LPRPCPROXYBUFFER)&This->lpVtbl);
    return 0;
  }
  return This->RefCount;
}

static HRESULT WINAPI StdProxy_Connect(LPRPCPROXYBUFFER iface,
                                      LPRPCCHANNELBUFFER pChannel)
{
  ICOM_THIS_MULTI(StdProxyImpl,lpVtbl,iface);
  TRACE("(%p)->Connect(%p)\n",This,pChannel);

  This->pChannel = pChannel;
  return S_OK;
}

static VOID WINAPI StdProxy_Disconnect(LPRPCPROXYBUFFER iface)
{
  ICOM_THIS_MULTI(StdProxyImpl,lpVtbl,iface);
  TRACE("(%p)->Disconnect()\n",This);

  This->pChannel = NULL;
}

static ICOM_VTABLE(IRpcProxyBuffer) StdProxy_Vtbl =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  StdProxy_QueryInterface,
  StdProxy_AddRef,
  StdProxy_Release,
  StdProxy_Connect,
  StdProxy_Disconnect
};

HRESULT WINAPI StdProxy_GetChannel(LPVOID iface,
                                  LPRPCCHANNELBUFFER *ppChannel)
{
  ICOM_THIS_MULTI(StdProxyImpl,PVtbl,iface);
  TRACE("(%p)->GetChannel(%p) %s\n",This,ppChannel,This->name);

  *ppChannel = This->pChannel;
  return S_OK;
}

HRESULT WINAPI StdProxy_GetIID(LPVOID iface,
                              const IID **ppiid)
{
  ICOM_THIS_MULTI(StdProxyImpl,PVtbl,iface);
  TRACE("(%p)->GetIID(%p) %s\n",This,ppiid,This->name);

  *ppiid = This->piid;
  return S_OK;
}

HRESULT WINAPI IUnknown_QueryInterface_Proxy(LPUNKNOWN iface,
                                            REFIID riid,
                                            LPVOID *ppvObj)
{
  ICOM_THIS_MULTI(StdProxyImpl,PVtbl,iface);
  TRACE("(%p)->QueryInterface(%s,%p) %s\n",This,debugstr_guid(riid),ppvObj,This->name);
  return IUnknown_QueryInterface(This->pUnkOuter,riid,ppvObj);
}

ULONG WINAPI IUnknown_AddRef_Proxy(LPUNKNOWN iface)
{
  ICOM_THIS_MULTI(StdProxyImpl,PVtbl,iface);
  TRACE("(%p)->AddRef() %s\n",This,This->name);
#if 0 /* interface refcounting */
  return ++(This->RefCount);
#else /* object refcounting */
  return IUnknown_AddRef(This->pUnkOuter);
#endif
}

ULONG WINAPI IUnknown_Release_Proxy(LPUNKNOWN iface)
{
  ICOM_THIS_MULTI(StdProxyImpl,PVtbl,iface);
  TRACE("(%p)->Release() %s\n",This,This->name);
#if 0 /* interface refcounting */
  if (!--(This->RefCount)) {
    StdProxy_Destruct((LPRPCPROXYBUFFER)&This->lpVtbl);
    return 0;
  }
  return This->RefCount;
#else /* object refcounting */
  return IUnknown_Release(This->pUnkOuter);
#endif
}

