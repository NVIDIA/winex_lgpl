#ifndef __RPCPROXY_H_VERSION__
/* FIXME: Find an appropriate version number.  I guess something is better than nothing */
#define __RPCPROXY_H_VERSION__ ( 399 )
#endif

#ifndef __WINE_RPCPROXY_H
#define __WINE_RPCPROXY_H

#include "basetsd.h"
#include "guiddef.h"
#include "winnt.h"
#include "rpc.h"
#include "rpcndr.h"

typedef struct tagCInterfaceStubVtbl *PCInterfaceStubVtblList;
typedef struct tagCInterfaceProxyVtbl *PCInterfaceProxyVtblList;
typedef const char *PCInterfaceName;
typedef int __stdcall IIDLookupRtn( const IID *pIID, int *pIndex );
typedef IIDLookupRtn *PIIDLookup;

typedef struct tagProxyFileInfo
{
  const PCInterfaceProxyVtblList *pProxyVtblList;
  const PCInterfaceStubVtblList *pStubVtblList;
  const PCInterfaceName *pNamesArray;
  const IID **pDelegatedIIDs;
  const PIIDLookup pIIDLookupRtn;
  unsigned short TableSize;
  unsigned short TableVersion;
  const IID **pAsyncIIDLookup;
  LONG_PTR Filler2;
  LONG_PTR Filler3;
  LONG_PTR Filler4;
} ProxyFileInfo;

typedef ProxyFileInfo ExtendedProxyFileInfo;

typedef struct tagCInterfaceProxyHeader
{
#ifdef USE_STUBLESS_PROXY
  const void *pStublessProxyInfo;
#endif
  const IID *piid;
} CInterfaceProxyHeader;

#define CINTERFACE_PROXY_VTABLE(n) \
  struct \
  { \
    CInterfaceProxyHeader header; \
    void *Vtbl[n]; \
  }

typedef struct tagCInterfaceProxyVtbl
{
  CInterfaceProxyHeader header;
#if defined(__GNUC__)
  void *Vtbl[0];
#else
  void *Vtbl[1];
#endif
} CInterfaceProxyVtbl;

typedef void (__RPC_STUB *PRPC_STUB_FUNCTION)(
  IRpcStubBuffer *This,
  IRpcChannelBuffer *_pRpcChannelBuffer,
  PRPC_MESSAGE _pRpcMessage,
  DWORD *pdwStubPhase);

typedef struct tagCInterfaceStubHeader
{
  const IID *piid;
  const MIDL_SERVER_INFO *pServerInfo;
  unsigned long DispatchTableCount;
  const PRPC_STUB_FUNCTION *pDispatchTable;
} CInterfaceStubHeader;

typedef struct tagCInterfaceStubVtbl
{
  CInterfaceStubHeader header;
  ICOM_VTABLE(IRpcStubBuffer) Vtbl;
} CInterfaceStubVtbl;

typedef struct tagCStdStubBuffer
{
  const ICOM_VTABLE(IRpcStubBuffer) *lpVtbl;
  long RefCount;
  struct IUnknown *pvServerObject;
  const struct ICallFactoryVtbl *pCallFactoryVtbl;
  const IID *pAsyncIID;
  struct IPSFactoryBuffer *pPSFactory;
} CStdStubBuffer;

typedef struct tagCStdPSFactoryBuffer
{
  const IPSFactoryBufferVtbl *lpVtbl;
  long RefCount;
  const ProxyFileInfo **pProxyFileList;
  long Filler1;
} CStdPSFactoryBuffer;

HRESULT WINAPI
  CStdStubBuffer_QueryInterface( IRpcStubBuffer *This, REFIID riid, void **ppvObject );
ULONG WINAPI
  CStdStubBuffer_AddRef( IRpcStubBuffer *This );
ULONG WINAPI
  CStdStubBuffer_Release( IRpcStubBuffer *This );
ULONG WINAPI
  NdrCStdStubBuffer_Release( IRpcStubBuffer *This, IPSFactoryBuffer *pPSF );
HRESULT WINAPI
  CStdStubBuffer_Connect( IRpcStubBuffer *This, IUnknown *pUnkServer );
void WINAPI
  CStdStubBuffer_Disconnect( IRpcStubBuffer *This );
HRESULT WINAPI
  CStdStubBuffer_Invoke( IRpcStubBuffer *This, RPCOLEMESSAGE *pRpcMsg, IRpcChannelBuffer *pRpcChannelBuffer );
IRpcStubBuffer * WINAPI
  CStdStubBuffer_IsIIDSupported( IRpcStubBuffer *This, REFIID riid );
ULONG WINAPI
  CStdStubBuffer_CountRefs( IRpcStubBuffer *This );
HRESULT WINAPI
  CStdStubBuffer_DebugServerQueryInterface( IRpcStubBuffer *This, void **ppv );
void WINAPI
  CStdStubBuffer_DebugServerRelease( IRpcStubBuffer *This, void *pv );

#define CStdStubBuffer_METHODS \
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE \
  CStdStubBuffer_QueryInterface, \
  CStdStubBuffer_AddRef, \
  CStdStubBuffer_Release, \
  CStdStubBuffer_Connect, \
  CStdStubBuffer_Disconnect, \
  CStdStubBuffer_Invoke, \
  CStdStubBuffer_IsIIDSupported, \
  CStdStubBuffer_CountRefs, \
  CStdStubBuffer_DebugServerQueryInterface, \
  CStdStubBuffer_DebugServerRelease

RPCRTAPI void RPC_ENTRY
  NdrProxyInitialize( void *This, PRPC_MESSAGE pRpcMsg, PMIDL_STUB_MESSAGE pStubMsg,
                      PMIDL_STUB_DESC pStubDescriptor, unsigned int ProcNum );
RPCRTAPI void RPC_ENTRY
  NdrProxyGetBuffer( void *This, PMIDL_STUB_MESSAGE pStubMsg );
RPCRTAPI void RPC_ENTRY
  NdrProxySendReceive( void *This, PMIDL_STUB_MESSAGE pStubMsg );
RPCRTAPI void RPC_ENTRY
  NdrProxyFreeBuffer( void *This, PMIDL_STUB_MESSAGE pStubMsg );
RPCRTAPI HRESULT RPC_ENTRY
  NdrProxyErrorHandler( DWORD dwExceptionCode );

RPCRTAPI void RPC_ENTRY
  NdrStubInitialize( PRPC_MESSAGE pRpcMsg, PMIDL_STUB_MESSAGE pStubMsg,
                     PMIDL_STUB_DESC pStubDescriptor, IRpcChannelBuffer *pRpcChannelBuffer );
RPCRTAPI void RPC_ENTRY
  NdrStubInitializePartial( PRPC_MESSAGE pRpcMsg, PMIDL_STUB_MESSAGE pStubMsg,
                            PMIDL_STUB_DESC pStubDescriptor, IRpcChannelBuffer *pRpcChannelBuffer,
                            unsigned long RequestedBufferSize );
void __RPC_STUB NdrStubForwardingFunction( IRpcStubBuffer *This, IRpcChannelBuffer *pChannel,
                                           PRPC_MESSAGE pMsg, DWORD *pdwStubPhase );
RPCRTAPI void RPC_ENTRY
  NdrStubGetBuffer( IRpcStubBuffer *This, IRpcChannelBuffer *pRpcChannelBuffer, PMIDL_STUB_MESSAGE pStubMsg );
RPCRTAPI HRESULT RPC_ENTRY
  NdrStubErrorHandler( DWORD dwExceptionCode );

RPCRTAPI HRESULT RPC_ENTRY
  NdrDllGetClassObject( REFCLSID rclsid, REFIID riid, void **ppv, const ProxyFileInfo **pProxyFileList,
                        const CLSID *pclsid, CStdPSFactoryBuffer *pPSFactoryBuffer );
RPCRTAPI HRESULT RPC_ENTRY
  NdrDllCanUnloadNow( CStdPSFactoryBuffer *pPSFactoryBuffer );

RPCRTAPI HRESULT RPC_ENTRY
  NdrDllRegisterProxy( HMODULE hDll, const ProxyFileInfo **pProxyFileList, const CLSID *pclsid );
RPCRTAPI HRESULT RPC_ENTRY
  NdrDllUnregisterProxy( HMODULE hDll, const ProxyFileInfo **pProxyFileList, const CLSID *pclsid );

#define CSTDSTUBBUFFERRELEASE(pFactory) \
ULONG WINAPI CStdStubBuffer_Release(IRpcStubBuffer *This) \
  { return NdrCStdStubBuffer_Release(This, (IPSFactoryBuffer *)pFactory); }

#define IID_GENERIC_CHECK_IID(name,pIID,index) memcmp(pIID, name##_ProxyVtblList[index]->header.piid, sizeof(IID))

/*
 * In these macros, BS stands for Binary Search. MIDL uses these to
 * "unroll" a binary search into the module's IID_Lookup function.
 * However, I haven't bothered to reimplement that stuff yet;
 * I've just implemented a linear search for now.
 */
#define IID_BS_LOOKUP_SETUP \
  int c;
#define IID_BS_LOOKUP_INITIAL_TEST(name, sz, split)
#define IID_BS_LOOKUP_NEXT_TEST(name, split)
#define IID_BS_LOOKUP_RETURN_RESULT(name, sz, index) \
  for (c=0; c<sz; c++) if (!name##_CHECK_IID(c)) { (index)=c; return 1; } \
  return 0;

#if defined(__WINE__) && defined(__WINE_WINE_OBJ_OLEAUT_H)
/* see http://msdn.microsoft.com/library/en-us/dnmsj99/html/com0199.asp?frame=true */

RPCRTAPI HRESULT RPC_ENTRY
  CreateProxyFromTypeInfo( LPTYPEINFO pTypeInfo, LPUNKNOWN pUnkOuter, REFIID riid,
                           LPRPCPROXYBUFFER *ppProxy, LPVOID *ppv );
RPCRTAPI HRESULT RPC_ENTRY
  CreateStubFromTypeInfo( LPTYPEINFO pTypeInfo, REFIID riid, LPUNKNOWN pUnkServer,
                          LPRPCSTUBBUFFER *ppStub );

#endif

#endif /*__WINE_RPCDCE_H */
