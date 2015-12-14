/*
 * Proxy/Stub implementation
 *
 * Copyright 2001 Ove Kåven, TransGaming Technologies
 *
 */

#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "ole2.h"
#include "winternl.h"
#include "rpcproxy.h"
#include "wine/unicode.h"
#include "wine/exception.h"
#include "msvcrt/excpt.h"

#include "objbase.h"
#include "compobj_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/* until I figure out how a table-strong marshal is truly represented,
 * I'll just take one of the reserved flags */
#define SORF_TABLESTRONG SORF_OXRES1

#define UNKNOWN_ADDREF(obj) do { DWORD rc = IUnknown_AddRef(obj); TRACE("AddRef(%p) -> %ld\n", obj, rc); } while (0)
#define UNKNOWN_RELEASE(obj) do { DWORD rc = IUnknown_Release(obj); TRACE("Release(%p) -> %ld\n", obj, rc); } while (0)

static void __RPC_STUB COM_RpcDispatch(PRPC_MESSAGE Message);
static LPRPCCHANNELBUFFER RpcChannel_Create(RPC_BINDING_HANDLE bind, DWORD model, DWORD tid);
void COM_FindXIf2(APARTMENT *apt, XOBJECT *obj, XIF **xif, REFIID riid);
void COM_FindXObj(APARTMENT *apt, OID oid, IPID ipid, XOBJECT **xobj, XIF **xif);
void COM_ExtAddRef(APARTMENT *apt, XOBJECT *xobj, XIF *xif, DWORD refs);
void COM_ExtRelease(APARTMENT *apt, XOBJECT *xobj, XIF *xif, DWORD refs);
void COM_CreateIObj(APARTMENT *apt, OXID oxid, OID oid, IPID ipid, IOBJECT **iobj, RPC_BINDING_HANDLE bind);
static HRESULT COM_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv);

/* the standard marshaller returns a special GUID */
const CLSID CLSID_StdMarshal = {
  0x1B, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}
};

/* CLSID for ole32's builtin marshalers
 * (referenced by winedefault.reg) */
static const CLSID CLSID_PSFactoryBuffer = {
  0x320, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}
};

const IID IID_IRemUnknown = {
  0x131, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}
};

static LONG initRPC;

static RPC_DISPATCH_FUNCTION rpc_dispatch_table[1] = { COM_RpcDispatch };
static RPC_DISPATCH_TABLE rpc_dispatch = { 1, rpc_dispatch_table };

typedef struct tagREGIF {
  struct tagREGIF *next;
  RPC_SERVER_INTERFACE If;
} REGIF;

static REGIF *RegIf;
static CRITICAL_SECTION csRegIf;

static LPRPCCHANNELBUFFER rpc_server_chan;
static RPC_IF_HANDLE rpc_IRemUnknown;

/* since I don't know how MS generates IPIDs, I'll just
 * combine the lower 32 bits of the OXID with the OID
 * and an interface counter */
typedef struct tagIIPID {
  OID oid;
  DWORD ifc;
  DWORD oxid;
} IIPID;

static RPC_IF_HANDLE COM_RpcRegIf(REFIID riid)
{
  REGIF *cif;
  BOOL reg = FALSE;
  TRACE("(%s)\n", debugstr_guid(riid));
  EnterCriticalSection(&csRegIf);
  for (cif = RegIf;
       cif && !IsEqualGUID(&cif->If.InterfaceId.SyntaxGUID, riid);
       cif = cif->next);
  if (!cif) {
    cif = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(REGIF));
    cif->If.Length = sizeof(RPC_SERVER_INTERFACE);
    cif->If.InterfaceId.SyntaxGUID = *riid;
    cif->If.DispatchTable = &rpc_dispatch;
    cif->next = RegIf;
    RegIf = cif;
    reg = TRUE;
  }
  LeaveCriticalSection(&csRegIf);
  if (reg)
    RpcServerRegisterIfEx((RPC_IF_HANDLE)&cif->If,
			  NULL, NULL,
			  RPC_IF_OLE | RPC_IF_AUTOLISTEN,
			  RPC_C_LISTEN_MAX_CALLS_DEFAULT,
			  NULL);
  return (RPC_IF_HANDLE)&cif->If;
}

void COM_RpcInit(void)
{
  if (!InterlockedExchange(&initRPC, 1)) {
    LPSTR eps;

    UuidToStringA((UUID*)&MTA.oxid, &eps);
    RpcServerUseProtseqEpA("ncalrpc",
                           RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                           eps, NULL);
    RpcStringFreeA(&eps);
    rpc_server_chan = RpcChannel_Create(0, COINIT_MULTITHREADED, 0);
    rpc_IRemUnknown = COM_RpcRegIf(&IID_IRemUnknown);
  }
}

void COM_RpcExportClass(REFCLSID rclsid, DWORD dwCtx)
{
  RPC_BINDING_VECTOR *bind;
  UUID_VECTOR objs;

  if (dwCtx == MSHCTX_INPROC) return;

  COM_RpcInit();
  /* I don't think using the RPC endpoint mapper is really the right place to
   * register class objects, but it'll do for now */
  objs.Count = 1;
  objs.Uuid[0] = (UUID*)rclsid;
  RpcServerInqBindings(&bind);
  /* FIXME: IRemUnknown is not the right interface for this, we should
   * rather implement IRemoteActivation, but I don't wanna right now */
  RpcEpRegisterA(rpc_IRemUnknown, bind, &objs, NULL);
  RpcBindingVectorFree(&bind);
}

void COM_RpcUnexportClass(REFCLSID rclsid, DWORD dwCtx)
{
  RPC_BINDING_VECTOR *bind;
  UUID_VECTOR objs;

  if (dwCtx == MSHCTX_INPROC) return;

  COM_RpcInit();
  objs.Count = 1;
  objs.Uuid[0] = (UUID*)rclsid;
  RpcServerInqBindings(&bind);
  RpcEpUnregister(rpc_IRemUnknown, bind, &objs);
  RpcBindingVectorFree(&bind);
}

HRESULT COM_RpcImportClass(LPVOID *ppv, REFCLSID rclsid, REFIID riid, LPSTR server)
{
  APARTMENT *apt = COM_CurrentApt();
  LPSTR uuid = NULL, sbind = NULL;
  RPC_BINDING_HANDLE bind = NULL;
  RPC_STATUS status;

  COM_RpcInit(); /* temporary */

  *ppv = NULL;
  UuidToStringA((UUID*)rclsid, &uuid);
  /* we only do local connections for now */
  RpcStringBindingComposeA(uuid, "ncalrpc", NULL, NULL, NULL, &sbind);
  status = RpcBindingFromStringBindingA(sbind, &bind);
  RpcStringFreeA(&sbind);
  if (status != RPC_S_OK) return status;
  /* use the endpoint mapper entry we made in COM_RpcExportClass */
  status = RpcEpResolveBinding(bind, rpc_IRemUnknown);
  if (status == RPC_S_OK) {
    /* we have a fully bound handle, get the class object */
    /* as a hack, we use IRemUnknown with the clsid as the ipid,
     * since we haven't implemented IRemoteActivation yet */
    IOBJECT *obj;

    COM_CreateIObj(apt, 0, 0, *rclsid, &obj, bind);
    if (obj) {
      status = IRemUnknown_QueryInterface((LPREMUNKNOWN)obj, riid, ppv);
      IRemUnknown_Release((LPREMUNKNOWN)obj);
    }
    else status = CO_E_OBJNOTCONNECTED;
  }
  else RpcBindingFree(&bind);
  return status;
}

static void __RPC_STUB COM_RpcDispatch(PRPC_MESSAGE pMsg)
{
  APARTMENT *apt = COM_CurrentApt();
  PRPC_SERVER_INTERFACE sif;
  IID iid;
  STDOBJREF oref;
  HRESULT hr;
  XOBJECT *xobj;
  XIF *xif;
  LPRPCSTUBBUFFER stub;

  TRACE("(%p)\n", pMsg);
  sif = pMsg->RpcInterfaceInformation;
  iid = sif->InterfaceId.SyntaxGUID;
  RpcBindingInqObject(pMsg->Handle, &oref.ipid);
  TRACE(" iid  %s\n", debugstr_guid(&iid));
  TRACE(" method %d\n", pMsg->ProcNum);

  if (!apt) apt = &MTA;

  /* to support the hack we use in COM_RpcImportClass,
   * first check whether the IPID is an exported clas
   */
  hr = COM_GetRegisteredClassObject(&oref.ipid, NULL,
				    &oref, NULL);
  /* if found:     hr = S_OK,    oref.ipid replaced
   * if not found: hr = S_FALSE, oref.ipid unchanged
   */
  if (FAILED(hr)) {
    /* we need to implement IRemoteActivation */
    FIXME("can't handle custom-marshaled class object\n");
    RpcRaiseException(RPC_S_CALL_FAILED_DNE);
    return;
  }

  /* get OID */
  oref.oid = ((IIPID*)&oref.ipid)->oid;
  /* recreate OXID (see compobj.c for OXID generation) */
  oref.oxid = ((IIPID*)&oref.ipid)->oxid | MTA.oxid;

  TRACE(" oxid %llx\n", oref.oxid);
  TRACE(" oid %llx\n", oref.oid);
  TRACE(" ipid %s\n", debugstr_guid(&oref.ipid));

  if (oref.oxid != apt->oxid) {
    HWND win;
    EXCEPTION_RECORD exc;

    if (oref.oxid == MTA.oxid) {
      FIXME("mysterious RPC STA found - aiee\n");
      RpcRaiseException(RPC_S_CALL_FAILED_DNE);
      return;
    }
    win = COM_GetApartmentWin(oref.oxid);
    if (win) {
      TRACE("dispatching to STA (hwnd=%08x)\n", win);
      memset(&exc, 0, sizeof(exc));
      if (SendMessageA(win, WM_USER, (WPARAM)&exc, (LPARAM)pMsg) != 0) {
        /* propagate exception */
        RtlRaiseException(&exc);
      }
    } else {
      ERR("failed to dispatch to STA\n");
      RpcRaiseException(RPC_S_CALL_FAILED_DNE);
    }
    return;
  }

  COM_FindXObj(apt, oref.oid, oref.ipid, &xobj, NULL);
  if (!xobj) {
    ERR("requested object not found\n");
    RpcRaiseException(RPC_S_OBJECT_NOT_FOUND);
    return;
  }

  TRACE(" object=%p\n", xobj->obj);

  /* handle IRemUnknown specially, since we haven't properly
   * implemented the apartment objects (and IOXIDResolver) yet */
  if (IsEqualGUID(&iid, &IID_IRemUnknown)) {
    stub = (LPRPCSTUBBUFFER)xobj;
  }
  else {
    /* our proxies use the same RPC binding for all the interfaces,
     * so we get the same IPID for them all, so we'll use the IID
     * instead of the IPID to find the actual interface stub */
    COM_FindXIf2(apt, xobj, &xif, &iid);
    if (!xif) {
      ERR("requested interface not found\n");
      return;
    }
    TRACE(" interface=%p\n", xif->iface);
    stub = xif->stub;
  }

  TRACE(" stub=%p\n", stub);
  if (!stub) {
    ERR("stub not found\n");
    RpcRaiseException(RPC_S_CALL_FAILED_DNE);
    return;
  }

  /* FIXME: handle ORPCTHIS and ORPCTHAT */

  IRpcStubBuffer_Invoke(stub, (RPCOLEMESSAGE*)pMsg, rpc_server_chan);
}

static WINE_EXCEPTION_FILTER(apt_filter)
{
  APARTMENT* apt = COM_CurrentInfo();
  memcpy(apt->except, GetExceptionInformation()->ExceptionRecord, sizeof(EXCEPTION_RECORD));
  return EXCEPTION_EXECUTE_HANDLER;
}

LRESULT CALLBACK COM_AptWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  APARTMENT* apt = COM_CurrentInfo();
  EXCEPTION_RECORD* old_except;
  LRESULT ret = 0;
  switch (msg) {
  case WM_USER:
    TRACE("received RPC message\n");
    old_except = apt->except;
    apt->except = (EXCEPTION_RECORD*)wParam;
    __TRY {
      COM_RpcDispatch((PRPC_MESSAGE)lParam);
    } __EXCEPT(apt_filter) {
      TRACE("returning caught exception\n");
      ret = 1;
    } __ENDTRY
    apt->except = old_except;
    break;
  default:
    ret = DefWindowProcA(hWnd, msg, wParam, lParam);
    break;
  }
  return ret;
}

/* COM_GetPSFactory [internal]:
 * Given an interface ID riid, this function returns an IPSFactoryBuffer
 * interface which can be used to create marshalling proxies or stubs for
 * the given interface. */
static HRESULT COM_GetPSFactory(REFIID riid, LPPSFACTORYBUFFER *pPS)
{
  HRESULT hr;
  CLSID clsid;
  hr = CoGetPSClsid(riid, &clsid);
  if (FAILED(hr)) return hr;
  hr = CoGetClassObject(&clsid, CLSCTX_INPROC_SERVER, NULL,
			&IID_IPSFactoryBuffer, (LPVOID *)pPS);
  return hr;
}

/******************** CHANNEL OBJECT ********************/

#define MAX_THREADS 128

typedef struct {
  ICOM_VTABLE(IRpcChannelBuffer) *lpVtbl;
  DWORD ref, tid;
  RPC_BINDING_HANDLE bind;
  HANDLE done;
} RpcChannelImpl;

typedef struct _RpcRequest {
  struct _RpcRequest *next;
  PRPC_MESSAGE msg;
  RPC_STATUS ret;
  HANDLE done;
} RpcRequest;

static HANDLE worker_sem;
static DWORD worker_count, worker_free;
static CRITICAL_SECTION creq_cs;
static RpcRequest *creq_head;
static RpcRequest *creq_tail;

static void RpcChannel_push_request(RpcRequest *req)
{
  req->next = NULL;
  req->ret = RPC_S_CALL_IN_PROGRESS; /* ? */
  EnterCriticalSection(&creq_cs);
  if (creq_tail) creq_tail->next = req;
  else {
    creq_head = req;
    creq_tail = req;
  }
  LeaveCriticalSection(&creq_cs);
}

static RpcRequest* RpcChannel_pop_request(void)
{
  RpcRequest* req;
  EnterCriticalSection(&creq_cs);
  req = creq_head;
  if (req) {
    creq_head = req->next;
    if (!creq_head) creq_tail = NULL;
  }
  LeaveCriticalSection(&creq_cs);
  if (req) req->next = NULL;
  return req;
}

static DWORD CALLBACK RpcChannel_worker_thread(LPVOID the_arg)
{
  HANDLE obj;
  RpcRequest* req;

  for (;;) {
    /* idle timeout after 5s */
    obj = WaitForSingleObject(worker_sem, 5000);
    if (obj == WAIT_TIMEOUT) {
      /* if another idle thread exist, self-destruct */
      if (worker_free > 1) break;
      continue;
    }
    req = RpcChannel_pop_request();
    if (!req) continue;
    InterlockedDecrement(&worker_free);
    for (;;) {
      req->ret = I_RpcSendReceive(req->msg);
      SetEvent(req->done);
      /* try to grab another request here without waiting
       * on the semaphore, in case it hits max */
      req = RpcChannel_pop_request();
      if (!req) break;
      /* decrement semaphore */
      WaitForSingleObject(worker_sem, 0);
    }
    InterlockedIncrement(&worker_free);
  }
  InterlockedDecrement(&worker_free);
  InterlockedDecrement(&worker_count);
  return 0;
}

static void RpcChannel_create_worker_if_needed(void)
{
  if (!worker_sem) {
    HANDLE sem;
    sem = CreateSemaphoreA(NULL, 0, MAX_THREADS, NULL);
    if (InterlockedCompareExchange((PLONG)&worker_sem, sem, 0))
      CloseHandle(sem); /* somebody beat us to it */
  }
  if (!worker_free && worker_count < MAX_THREADS) {
    HANDLE thread;
    InterlockedIncrement(&worker_count);
    InterlockedIncrement(&worker_free);
    thread = CreateThread(NULL, 0, RpcChannel_worker_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
    else {
      InterlockedDecrement(&worker_free);
      InterlockedDecrement(&worker_count);
    }
  }
}

static HRESULT WINAPI RpcChannel_QueryInterface(LPRPCCHANNELBUFFER iface,
						REFIID riid,
						LPVOID *obj)
{
  ICOM_THIS(RpcChannelImpl,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IRpcChannelBuffer,riid)) {
    *obj = This;
    This->ref++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI RpcChannel_AddRef(LPRPCCHANNELBUFFER iface)
{
  ICOM_THIS(RpcChannelImpl,iface);
  TRACE("(%p)->AddRef()\n",This);
  return ++(This->ref);
}

static ULONG WINAPI RpcChannel_Release(LPRPCCHANNELBUFFER iface)
{
  ICOM_THIS(RpcChannelImpl,iface);
  TRACE("(%p)->Release()\n",This);
  if (!--(This->ref)) {
    if (This->done) CloseHandle(This->done);
    RpcBindingFree(&This->bind);
    HeapFree(GetProcessHeap(),0,This);
    return 0;
  }
  return This->ref;
}

static inline HRESULT RPC2HR(RPC_STATUS status)
{
  switch (status) {
  case RPC_S_OK:
    return S_OK;
  case RPC_S_CALL_FAILED:
    return RPC_S_CALL_FAILED; /* ? */
  default:
    return E_FAIL;
  }
}

static HRESULT WINAPI RpcChannel_GetBuffer(LPRPCCHANNELBUFFER iface,
					   PRPCOLEMESSAGE pMessage,
					   REFIID riid)
{
  ICOM_THIS(RpcChannelImpl,iface);
  PRPC_MESSAGE pmsg = (PRPC_MESSAGE)pMessage;
  PRPC_CLIENT_INTERFACE cif;
  RPC_STATUS status;

  TRACE("(%p)->GetBuffer(%p,%p)\n",This,pMessage,riid);

  if (This->bind) {
    cif = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RPC_CLIENT_INTERFACE));
    cif->Length = sizeof(RPC_CLIENT_INTERFACE);
    cif->InterfaceId.SyntaxGUID = *riid;
    pmsg->RpcInterfaceInformation = cif;
    pmsg->Handle = This->bind;
  }

  status = I_RpcGetBuffer(pmsg);
  return RPC2HR(status);
}

static HRESULT WINAPI RpcChannel_SendReceive(LPRPCCHANNELBUFFER iface,
					     PRPCOLEMESSAGE pMessage,
					     ULONG *pStatus)
{
  ICOM_THIS(RpcChannelImpl,iface);
  PRPC_MESSAGE pmsg = (PRPC_MESSAGE)pMessage;
  RPC_STATUS status;
  DWORD got_quit = 0;
  int quit_code = 0;

  TRACE("(%p)->SendReceive(%p,%p)\n",This,pMessage,pStatus);
  if (This->done) {
    RpcRequest req;
    DWORD w;
    MSG msg;

    req.msg = pmsg;
    req.done = This->done;
    RpcChannel_create_worker_if_needed();
    RpcChannel_push_request(&req);
    ReleaseSemaphore(worker_sem, 1, NULL);
    do {
      DWORD timeout;
      while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          got_quit++;
          quit_code = msg.wParam;
          TRACE("postponing WM_QUIT\n");
          continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        TRACE("message %04x dispatched\n", msg.message);
      }
      /* in the event that a recursive SendReceive caught the event for the completion
       * of this RPC call, set timeout to 0 if the request is already complete */
      timeout = (req.ret != RPC_S_CALL_IN_PROGRESS) ? 0 : INFINITE;
      w = MsgWaitForMultipleObjectsEx(1, &This->done, timeout, QS_ALLINPUT, MWMO_ALERTABLE);
      TRACE("MsgWaitForMultipleObjects returns %ld\n", w);
      if (w == (WAIT_OBJECT_0+1)) continue;
      if (req.ret != RPC_S_CALL_IN_PROGRESS) break;
    } while (TRUE);
    /* in the unlikely event that we happened to catch the completion (through req.ret)
     * before the event was signaled, the signal won't be cleared, but this should
     * not cause anything worse than an extra round of PeekMessage next call */
    status = req.ret;
  } else {
    status = I_RpcSendReceive(pmsg);
  }
  while (got_quit--) PostQuitMessage(quit_code);
  return RPC2HR(status);
}

static HRESULT WINAPI RpcChannel_FreeBuffer(LPRPCCHANNELBUFFER iface,
					    PRPCOLEMESSAGE pMessage)
{
  ICOM_THIS(RpcChannelImpl,iface);
  PRPC_MESSAGE pmsg = (PRPC_MESSAGE)pMessage;
  RPC_STATUS status;

  TRACE("(%p)->FreeBuffer(%p)\n",This,pMessage);
  status = I_RpcFreeBuffer(pmsg);
  if (This->bind) {
    HeapFree(GetProcessHeap(), 0, pmsg->RpcInterfaceInformation);
    pmsg->RpcInterfaceInformation = NULL;
  }
  return RPC2HR(status);
}

static HRESULT WINAPI RpcChannel_GetDestCtx(LPRPCCHANNELBUFFER iface,
					    DWORD *pdwDestContext,
					    LPVOID *ppvDestContext)
{
  ICOM_THIS(RpcChannelImpl,iface);
  FIXME("(%p)->GetDestCtx(%p,%p)\n",This,pdwDestContext,ppvDestContext);
  return E_FAIL;
}

static HRESULT WINAPI RpcChannel_IsConnected(LPRPCCHANNELBUFFER iface)
{
  ICOM_THIS(RpcChannelImpl,iface);
  FIXME("(%p)->IsConnected()\n",This);
  return S_OK;
}

static ICOM_VTABLE(IRpcChannelBuffer) RpcChannel_VTable = {
  RpcChannel_QueryInterface,
  RpcChannel_AddRef,
  RpcChannel_Release,
  RpcChannel_GetBuffer,
  RpcChannel_SendReceive,
  RpcChannel_FreeBuffer,
  RpcChannel_GetDestCtx,
  RpcChannel_IsConnected
};

static LPRPCCHANNELBUFFER RpcChannel_Create(RPC_BINDING_HANDLE bind, DWORD model, DWORD tid)
{
  RpcChannelImpl *chan;

  chan = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(RpcChannelImpl));
  if (!chan) return NULL;
  chan->lpVtbl = &RpcChannel_VTable;
  chan->ref = 1;
  chan->tid = tid;
  chan->bind = bind;
  if (bind && (model & COINIT_APARTMENTTHREADED)) {
    chan->done = CreateEventA(NULL, FALSE, FALSE, NULL);
  }
  return (LPRPCCHANNELBUFFER)chan;
}


/* COM tracks interfaces with three kinds of IDs:
 *  OXID = Object Exporter ID, one for each apartment (unique within network)
 *  OID = Object ID, one for each object (unique within apartment)
 *  IPID = Interface Pointer ID, one for each interface (unique within apartment)
 *
 * Hence, each "stub manager" would correspond to an OID, and each
 * of the interface stubs would correspond to an IPID.
 */

/******************** STUB MANAGER ********************/
/* (FIXME: it is incorrect for the stub manager to implement
 * IRemUnknown, it's rather a job for the OXID object,
 * but this will do for now) */

void COM_FindXIf(APARTMENT *apt, IPID ipid, XOBJECT *obj, XIF **xif)
{
  XIF *iface;
  EnterCriticalSection(&apt->cs);
  if (xif) {
    for (iface = obj->ifaces; iface && !IsEqualGUID(&iface->ipid, &ipid); iface = iface->next);
    *xif = iface;
  }
  LeaveCriticalSection(&apt->cs);
}

void COM_FindXIf2(APARTMENT *apt, XOBJECT *obj, XIF **xif, REFIID riid)
{
  XIF *iface;
  EnterCriticalSection(&apt->cs);
  if (xif) {
    for (iface = obj->ifaces; iface && !IsEqualGUID(&iface->iid, riid); iface = iface->next);
    *xif = iface;
  }
  LeaveCriticalSection(&apt->cs);
}

void COM_CreateXIf(APARTMENT *apt, XOBJECT *obj, XIF **xif, REFIID riid, LPVOID pv)
{
  XIF *iface;

  EnterCriticalSection(&apt->cs);
  for (iface = obj->ifaces; iface && !IsEqualGUID(&iface->iid, riid); iface = iface->next);
  if (!iface) {
    LPPSFACTORYBUFFER pPSF;
    iface = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(XIF));
    iface->iface = pv;
    iface->iid = *riid;
    /* generate IPID */
    ((IIPID*)&iface->ipid)->oxid = apt->oxid;
    ((IIPID*)&iface->ipid)->oid = obj->oid;
    ((IIPID*)&iface->ipid)->ifc = obj->ifc++;
    if (!IsEqualGUID(riid, &IID_IUnknown)) {
      /* register RPC interface */
      COM_RpcRegIf(riid);
      iface->hres = COM_GetPSFactory(riid, &pPSF);
      /* attempt to create an interface stub */
      if (SUCCEEDED(iface->hres)) {
	iface->hres = IPSFactoryBuffer_CreateStub(pPSF, riid, pv,
						  &iface->stub);
	IPSFactoryBuffer_Release(pPSF);
	TRACE(" oid %llx ipid %s iface=%p stub=%p\n",
	      obj->oid, debugstr_guid(&iface->ipid), iface->iface,
	      iface->stub);
      }
    }
    iface->next = obj->ifaces;
    obj->ifaces = iface;
  }
#if 0 /* I don't think we AddRef-ed it? */
  else UNKNOWN_RELEASE((LPUNKNOWN)pv);
#endif
  if (xif) *xif = iface;
  LeaveCriticalSection(&apt->cs);
}

static HRESULT WINAPI StubMan_QueryInterface(LPRPCSTUBBUFFER iface,
					     REFIID riid,
					     LPVOID *obj)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IRpcStubBuffer,riid)) {
    *obj = This;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI StubMan_AddRef(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->AddRef()\n",This);
  return 2;
}

static ULONG WINAPI StubMan_Release(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->Release()\n",This);
  return 1;
}

static HRESULT WINAPI StubMan_Connect(LPRPCSTUBBUFFER iface,
				      LPUNKNOWN lpUnkServer)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->Connect(%p)\n",This,lpUnkServer);
  return S_OK;
}

static void WINAPI StubMan_Disconnect(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->Disconnect()\n",This);
}

static HRESULT WINAPI StubMan_Invoke(LPRPCSTUBBUFFER iface,
				     PRPCOLEMESSAGE pMsg,
				     LPRPCCHANNELBUFFER pChannel)
{
  ICOM_THIS(XOBJECT,iface);
  APARTMENT *apt = COM_CurrentApt();
  /* DWORD dwPhase = 0; */
  ULONG iMethod = pMsg->iMethod;
  XIF *xiface;
  LPVOID ipv;
  LPBYTE buf = pMsg->Buffer;
  DWORD count;
  ULONG cRefs;
  USHORT cIids;
  IID iid, *piid;

  TRACE("(%p)->Invoke(%p,%p) method %ld\n",This,pMsg,pChannel,iMethod);
  switch (iMethod) {
  case 3: /* RemQueryInterface */
    /* FIXME: use rpcrt4's NDR marshaller library
     * (NdrSimpleTypeUnmarshall etc) */
#if 0
    COM_FindXIf(apt, *(IPID*)buf, This, &xiface);
    if (!xiface) {
      ERR("RemQueryInterface on invalid interface %s\n", debugstr_guid((IPID*)buf));
      break;
    }
    ipv = xiface->iface;
#else
    xiface = NULL;
    ipv = This->obj;
#endif
    buf += sizeof(IPID);
    memcpy(&cRefs, buf, sizeof(ULONG));
    buf += sizeof(ULONG);
    memcpy(&cIids, buf, sizeof(USHORT));
    buf += sizeof(USHORT);
    TRACE("RemQueryInterface(%p,%ld,%d,...)\n", ipv, cRefs, cIids);
    if (cIids == 1) {
      piid = &iid;
    }
    else {
      FIXME("handle more than one IID\n");
      break;
    }
    memcpy(piid, buf, cIids*sizeof(IID));
    /* compose reply */
    pMsg->cbBuffer = cIids*sizeof(REMQIRESULT);
    I_RpcGetBuffer((PRPC_MESSAGE)pMsg);
    buf = pMsg->Buffer;
    for (count=0; count<cIids; count++) {
      REMQIRESULT *qi = (REMQIRESULT*)buf;
      XIF *xif;
      LPVOID pv = NULL;

      qi->hResult = E_FAIL;
      TRACE(" querying IID %s\n", debugstr_guid(&piid[count]));
      COM_FindXIf2(apt, This, &xif, &piid[count]);
      if (!xif) {
	qi->hResult = IUnknown_QueryInterface((LPUNKNOWN)ipv,
					      &piid[count],
					      &pv);
	if (qi->hResult == S_OK) {
	  COM_CreateXIf(apt, This, &xif, &piid[count], pv);
	  UNKNOWN_RELEASE((LPUNKNOWN)pv);
	}
      }
      if (xif) {
	qi->hResult = xif->hres;
	if (qi->hResult == S_OK) {
	  qi->std.flags = 0;
	  qi->std.cPublicRefs = cRefs;
	  qi->std.oxid = apt->oxid;
	  qi->std.oid = This->oid;
	  qi->std.ipid = xif->ipid;
	  COM_ExtAddRef(apt, This, xif, cRefs);
	  TRACE(" returning IPID %s for iface %p\n",
		debugstr_guid(&qi->std.ipid), xif->iface);
	}
      }
      buf += sizeof(REMQIRESULT);
    }
    break;
  case 4: /* RemAddRef */
    memcpy(&cIids, buf, sizeof(USHORT));
    buf += sizeof(USHORT);
    TRACE("RemAddRef(%d,...)\n", cIids);
    for (count=0; count<cIids; count++) {
      REMINTERFACEREF *ir = (REMINTERFACEREF*)buf;
      TRACE(" on IPID %s\n", debugstr_guid(&ir->ipid));
      COM_FindXIf(apt, ir->ipid, This, &xiface);
      if (!xiface)
        ERR("RemAddRef on invalid interface %s\n", debugstr_guid(&ir->ipid));
      else
        COM_ExtAddRef(apt, This, xiface, ir->cPublicRefs);
      buf += sizeof(REMINTERFACEREF);
    }
    pMsg->cbBuffer = cIids * sizeof(HRESULT);
    I_RpcGetBuffer((PRPC_MESSAGE)pMsg);
    buf = pMsg->Buffer;
    /* S_OK on all addrefs */
    memset(buf, 0, cIids * sizeof(HRESULT));
    break;
  case 5: /* RemRelease */
    memcpy(&cIids, buf, sizeof(USHORT));
    buf += sizeof(USHORT);
    TRACE("RemRelease(%d,...)\n", cIids);
    for (count=0; count<cIids; count++) {
      REMINTERFACEREF *ir = (REMINTERFACEREF*)buf;
      TRACE(" on IPID %s: refs %u\n", debugstr_guid(&ir->ipid), ir->cPublicRefs);
      COM_FindXIf(apt, ir->ipid, This, &xiface);
      if (!xiface)
        ERR("RemRelease on invalid interface %s\n", debugstr_guid(&ir->ipid));
      else
        COM_ExtRelease(apt, This, xiface, ir->cPublicRefs);
      buf += sizeof(REMINTERFACEREF);
    }
    pMsg->cbBuffer = 0;
    I_RpcGetBuffer((PRPC_MESSAGE)pMsg);
    buf = pMsg->Buffer;
    break;
  }
  return S_OK;
}

static LPRPCSTUBBUFFER WINAPI StubMan_IsIIDSupported(LPRPCSTUBBUFFER iface,
						     REFIID riid)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->IsIIDSupported(%s)\n",This,debugstr_guid(riid));
  return IsEqualGUID(&IID_IRemUnknown, riid) ? iface : NULL;
}

static ULONG WINAPI StubMan_CountRefs(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->CountRefs()\n",This);
  return This->refs;
}

static HRESULT WINAPI StubMan_DebugServerQueryInterface(LPRPCSTUBBUFFER iface,
							LPVOID *ppv)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->DebugServerQueryInterface(%p)\n",This,ppv);
  return S_OK;
}

static void WINAPI StubMan_DebugServerRelease(LPRPCSTUBBUFFER iface,
					      LPVOID pv)
{
  ICOM_THIS(XOBJECT,iface);
  TRACE("(%p)->DebugServerRelease(%p)\n",This,pv);
}

static ICOM_VTABLE(IRpcStubBuffer) StubMan_VTable =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  StubMan_QueryInterface,
  StubMan_AddRef,
  StubMan_Release,
  StubMan_Connect,
  StubMan_Disconnect,
  StubMan_Invoke,
  StubMan_IsIIDSupported,
  StubMan_CountRefs,
  StubMan_DebugServerQueryInterface,
  StubMan_DebugServerRelease
};

void COM_FindXObj(APARTMENT *apt, OID oid, IPID ipid, XOBJECT **xobj, XIF **xif)
{
  XOBJECT *obj;
  EnterCriticalSection(&apt->cs);
  for (obj = apt->objs; obj && obj->oid != oid; obj = obj->next);
  if (xobj) *xobj = obj;
  if (obj) {
    if (xif) COM_FindXIf(apt, ipid, obj, xif);
  }
  else if (xif) *xif = NULL;
  LeaveCriticalSection(&apt->cs);
}

void COM_CreateXObj(APARTMENT *apt, XOBJECT **xobj, XIF **xif, REFIID riid, LPVOID pv)
{
  LPUNKNOWN pUnk = NULL;
  XOBJECT *obj;

  IUnknown_QueryInterface((LPUNKNOWN)pv, &IID_IUnknown, (LPVOID*)&pUnk);
  EnterCriticalSection(&apt->cs);
  for (obj = apt->objs; obj && obj->obj != pUnk; obj = obj->next);
  if (!obj) {
    obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(XOBJECT));
    obj->lpVtbl = &StubMan_VTable;
    obj->parent = apt;
    UNKNOWN_ADDREF(pUnk);
    obj->obj = pUnk;
    /* generate OID */
    obj->oid = apt->oidc++;
    obj->next = apt->objs;
    apt->objs = obj;
    TRACE(" oid=%llx\n", obj->oid);
    /* add the IUnknown interface first */
#if 0
    UNKNOWN_ADDREF(pUnk);
#endif
    COM_CreateXIf(apt, obj, NULL, &IID_IUnknown, pUnk);
  }
  if (xobj) *xobj = obj;
  if (obj) COM_CreateXIf(apt, obj, xif, riid, pv);
  else {
#if 0 /* I don't think we AddRef-ed it? */
    UNKNOWN_RELEASE((LPUNKNOWN)pv);
#endif
    if (xif) *xif = NULL;
  }
  LeaveCriticalSection(&apt->cs);
  UNKNOWN_RELEASE(pUnk);
}

void COM_ExtAddRef(APARTMENT *apt, XOBJECT *xobj, XIF *xif, DWORD refs)
{
  EnterCriticalSection(&apt->cs);
  xif->refs += refs;
  xobj->refs += refs;
  LeaveCriticalSection(&apt->cs);
}

void COM_ExtRelease(APARTMENT *apt, XOBJECT *xobj, XIF *xif, DWORD refs)
{
  if (!refs) return;
  EnterCriticalSection(&apt->cs);
  xif->refs -= refs;
  xobj->refs -= refs;
  if (!xobj->refs) {
    XOBJECT *cobj, *pobj;
    XIF *cif, *pif;

    TRACE("destroying stub manager %p\n", xobj);
    TRACE(" oid=%llx\n", xobj->oid);

    /* unlink exported object */
    for (pobj = NULL, cobj = apt->objs;
	 cobj && cobj != xobj;
	 pobj = cobj, cobj = cobj->next);
    if (cobj) {
      if (pobj) pobj->next = cobj->next;
      else apt->objs = cobj->next;
      cobj->next = NULL;
    }
    LeaveCriticalSection(&apt->cs);

    /* destroy interface stubs */
    cif = xobj->ifaces;
    while (cif) {
      pif = cif;
      cif = cif->next;

      TRACE(" ipid %s iface=%p stub=%p\n",
	    debugstr_guid(&pif->ipid), pif->iface, pif->stub);

      /* maybe call something like RpcUnreg? */
      if (pif->stub)
        IRpcStubBuffer_Release(pif->stub);
#if 0 /* I don't think we AddRef-ed it */
      UNKNOWN_RELEASE((LPUNKNOWN)pif->iface);
#endif
      HeapFree(GetProcessHeap(), 0, pif);
    }
    /* destroy stub manager */
    UNKNOWN_RELEASE(xobj->obj);
    HeapFree(GetProcessHeap(), 0, xobj);
    return;
  }
  TRACE("remaining obj refs: %ld (interface refs: %ld)\n", xobj->refs, xif->refs);
  LeaveCriticalSection(&apt->cs);
}

/******************** PROXY MANAGER ********************/
/* (FIXME: it is incorrect for the proxy manager to implement
 * IRemUnknown, it's rather a job for the OXID object,
 * but this will do for now) */
/* FIXME: the proxy manager should implement IMarshal,
 * so that proxies to proxies can't occur */

void COM_CreateIIf(APARTMENT *apt, IPID ipid, IOBJECT *obj, IIF **iif, REFIID riid)
{
  IIF *iface;
  EnterCriticalSection(&apt->cs);
  for (iface = obj->ifaces; iface && !IsEqualGUID(&iface->ipid, &ipid); iface = iface->next);
  if (!iface) {
    LPPSFACTORYBUFFER pPSF;
    iface = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IIF));
    iface->iid = *riid;
    iface->ipid = ipid;
    if (!IsEqualGUID(riid, &IID_IUnknown)) {
      /* attempt to aggregate an interface proxy
       * into the imported object identity */
      iface->hres = COM_GetPSFactory(riid, &pPSF);
      if (SUCCEEDED(iface->hres)) {
	iface->hres = IPSFactoryBuffer_CreateProxy(pPSF, (LPUNKNOWN)obj, riid,
						   &iface->proxy, &iface->iface);
	IPSFactoryBuffer_Release(pPSF);
	/* if successful, connect the interface proxy */
	if (SUCCEEDED(iface->hres) && obj->chan)
	  IRpcProxyBuffer_Connect(iface->proxy, obj->chan);
      }
    }
    else iface->iface = obj;
    iface->next = obj->ifaces;
    obj->ifaces = iface;
  }
  if (iif) *iif = iface;
  LeaveCriticalSection(&apt->cs);
}

static HRESULT WINAPI ProxyMan_QueryInterface(LPREMUNKNOWN iface,
					      REFIID riid,
					      LPVOID *obj)
{
  ICOM_THIS(IOBJECT,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid)) {
    *obj = This;
    EnterCriticalSection(&This->parent->cs);
    This->refs++;
    LeaveCriticalSection(&This->parent->cs);
    return S_OK;
  }
  else {
    IIF *iif;
    REMQIRESULT qres;
    HRESULT hr;

    EnterCriticalSection(&This->parent->cs);
    iif = This->ifaces;
    while (iif) {
      if (IsEqualGUID(&iif->iid,riid)) {
	*obj = iif->iface;
	IUnknown_AddRef((LPUNKNOWN)*obj);
	LeaveCriticalSection(&This->parent->cs);
	return S_OK;
      }
      iif = iif->next;
    }
    LeaveCriticalSection(&This->parent->cs);

    /* not found locally, ask server for interface */
    hr = IRemUnknown_RemQueryInterface(iface, &This->ipid, 5,
				       1, (IID*)riid, &qres);
    if (SUCCEEDED(hr)) hr = qres.hResult;
    if (SUCCEEDED(hr)) {
      EnterCriticalSection(&This->parent->cs);
      COM_CreateIIf(This->parent, qres.std.ipid, This, &iif, riid);
      if (iif) {
	iif->refs += qres.std.cPublicRefs;
	hr = iif->hres;

	/* unmarshal complete */
	if (SUCCEEDED(hr)) {
	  *obj = iif->iface;
	  IUnknown_AddRef((LPUNKNOWN)*obj);
	}
      }
      LeaveCriticalSection(&This->parent->cs);
    }
    return hr;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI ProxyMan_AddRef(LPREMUNKNOWN iface)
{
  ICOM_THIS(IOBJECT,iface);
  ULONG refs;

  TRACE("(%p)->AddRef() ref=%ld\n",This,This->refs);
  EnterCriticalSection(&This->parent->cs);
  refs = ++(This->refs);
  LeaveCriticalSection(&This->parent->cs);
  return refs;
}

static ULONG WINAPI ProxyMan_Release(LPREMUNKNOWN iface)
{
  ICOM_THIS(IOBJECT,iface);
  ULONG refs;

  TRACE("(%p)->Release() ref=%ld\n",This,This->refs);
  EnterCriticalSection(&This->parent->cs);
  refs = --(This->refs);
  if (!refs) {
    IOBJECT *cobj, *pobj;
    IIF *cif, *pif;
    DWORD icnt, icur;
    REMINTERFACEREF *iref;

    /* unlink imported object */
    for (pobj = NULL, cobj = This->parent->proxies;
	 cobj && cobj != This;
	 pobj = cobj, cobj = cobj->next);
    if (cobj) {
      if (pobj) pobj->next = cobj->next;
      else This->parent->proxies = cobj->next;
      cobj->next = NULL;
    }
    LeaveCriticalSection(&This->parent->cs);

    TRACE("destroying proxy manager %p\n", This);
    TRACE(" oid=%llx\n", This->oid);

    /* count interface proxies */
    for (icnt = 0, cif = This->ifaces; cif; cif = cif->next, icnt++);
    if (icnt) {
      iref = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(REMINTERFACEREF)*icnt);

      /* destroy interface proxies */
      icur = 0;
      cif = This->ifaces;
      while (cif) {
	iref[icur].ipid = cif->ipid;
	iref[icur].cPublicRefs = cif->refs;
	iref[icur].cPrivateRefs = 0;

	pif = cif;
	cif = cif->next;
	icur++;

	TRACE(" ipid %s iface=%p proxy=%p\n",
	      debugstr_guid(&pif->ipid), pif->iface, pif->proxy);

	if (pif->proxy) IRpcProxyBuffer_Release(pif->proxy);
	HeapFree(GetProcessHeap(), 0, pif);
      }

      if (icur != icnt) {
	ERR("icur != icnt, bad stuff\n");
      }

      IRemUnknown_RemRelease(iface, icur, iref);
      HeapFree(GetProcessHeap(), 0, iref);
    }
    HeapFree(GetProcessHeap(), 0, This);
    return 0;
  }
  LeaveCriticalSection(&This->parent->cs);
  return refs;
}

static HRESULT WINAPI ProxyMan_RemQueryInterface(LPREMUNKNOWN iface,
						 REFIPID ripid,
						 ULONG cRefs,
						 USHORT cIids,
						 IID* iids,
						 REMQIRESULT* ppQIResults)
{
  ICOM_THIS(IOBJECT,iface);
  RPCOLEMESSAGE msg;
  HRESULT hr = S_OK;
  ULONG status;

  TRACE("(%p)->(%s,%ld,%d,%p,%p)\n",This,
	debugstr_guid(ripid),cRefs,cIids,iids,ppQIResults);

  memset(&msg, 0, sizeof(msg));
  msg.iMethod = 3;
  msg.cbBuffer = sizeof(IPID) + sizeof(ULONG) +
    sizeof(USHORT) + cIids*sizeof(IID);
  hr = IRpcChannelBuffer_GetBuffer(This->chan, &msg, &IID_IRemUnknown);
  if (SUCCEEDED(hr)) {
    LPBYTE buf = msg.Buffer;
    /* FIXME: use rpcrt4's NDR marshaller library
     * (NdrSimpleTypeMarshall etc) */
    memcpy(buf, ripid, sizeof(IPID));
    buf += sizeof(IPID);
    memcpy(buf, &cRefs, sizeof(ULONG));
    buf += sizeof(ULONG);
    memcpy(buf, &cIids, sizeof(USHORT));
    buf += sizeof(USHORT);
    memcpy(buf, iids, cIids*sizeof(IID));

    hr = IRpcChannelBuffer_SendReceive(This->chan, &msg, &status);

    if (SUCCEEDED(hr)) {
      buf = msg.Buffer;
      memcpy(ppQIResults, buf, cIids*sizeof(REMQIRESULT));
    }

    IRpcChannelBuffer_FreeBuffer(This->chan, &msg);
  }

  return hr;
}

static HRESULT WINAPI ProxyMan_RemAddRef(LPREMUNKNOWN iface,
					 USHORT cInterfaceRefs,
					 REMINTERFACEREF* InterfaceRefs,
					 HRESULT* pResults)
{
  ICOM_THIS(IOBJECT,iface);
  RPCOLEMESSAGE msg;
  HRESULT hr = S_OK;
  ULONG status;

  TRACE("(%p)->(%d,%p,%p)\n",This,
	cInterfaceRefs,InterfaceRefs,pResults);

  memset(&msg, 0, sizeof(msg));
  msg.iMethod = 4;
  msg.cbBuffer = sizeof(USHORT) + cInterfaceRefs*sizeof(REMINTERFACEREF);
  hr = IRpcChannelBuffer_GetBuffer(This->chan, &msg, &IID_IRemUnknown);
  if (SUCCEEDED(hr)) {
    LPBYTE buf = msg.Buffer;
    /* FIXME: use rpcrt4's NDR marshaller library
     * (NdrSimpleTypeMarshall etc) */
    memcpy(buf, &cInterfaceRefs, sizeof(USHORT));
    buf += sizeof(USHORT);
    memcpy(buf, InterfaceRefs, cInterfaceRefs*sizeof(REMINTERFACEREF));

    hr = IRpcChannelBuffer_SendReceive(This->chan, &msg, &status);

    if (SUCCEEDED(hr)) {
      buf = msg.Buffer;
      memcpy(pResults, buf, cInterfaceRefs*sizeof(HRESULT));
    }

    IRpcChannelBuffer_FreeBuffer(This->chan, &msg);
  }

  return hr;
}

static HRESULT WINAPI ProxyMan_RemRelease(LPREMUNKNOWN iface,
					  USHORT cInterfaceRefs,
					  REMINTERFACEREF* InterfaceRefs)
{
  ICOM_THIS(IOBJECT,iface);
  RPCOLEMESSAGE msg;
  HRESULT hr = S_OK;
  ULONG status;

  TRACE("(%p)->(%d,%p)\n",This,
	cInterfaceRefs,InterfaceRefs);

  memset(&msg, 0, sizeof(msg));
  msg.iMethod = 5;
  msg.cbBuffer = sizeof(USHORT) + cInterfaceRefs*sizeof(REMINTERFACEREF);
  hr = IRpcChannelBuffer_GetBuffer(This->chan, &msg, &IID_IRemUnknown);
  if (SUCCEEDED(hr)) {
    LPBYTE buf = msg.Buffer;
    /* FIXME: use rpcrt4's NDR marshaller library
     * (NdrSimpleTypeMarshall etc) */
    memcpy(buf, &cInterfaceRefs, sizeof(USHORT));
    buf += sizeof(USHORT);
    memcpy(buf, InterfaceRefs, cInterfaceRefs*sizeof(REMINTERFACEREF));

    hr = IRpcChannelBuffer_SendReceive(This->chan, &msg, &status);

    IRpcChannelBuffer_FreeBuffer(This->chan, &msg);
  }

  return hr;
}

static ICOM_VTABLE(IRemUnknown) ProxyMan_VTable =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  ProxyMan_QueryInterface,
  ProxyMan_AddRef,
  ProxyMan_Release,
  ProxyMan_RemQueryInterface,
  ProxyMan_RemAddRef,
  ProxyMan_RemRelease
};

void COM_FindIObj(APARTMENT *apt, OXID oxid, OID oid, IPID ipid, IOBJECT **iobj, IIF **iif)
{
  IOBJECT *obj;
  IIF *iface;
  EnterCriticalSection(&apt->cs);
  for (obj = apt->proxies; obj && (obj->oxid != oxid || obj->oid != oid); obj = obj->next);
  if (iobj) *iobj = obj;
  if (obj) {
    if (iif) {
      for (iface = obj->ifaces; iface && !IsEqualGUID(&iface->ipid, &ipid); iface = iface->next);
      *iif = iface;
    }
  }
  else if (iif) *iif = NULL;
  LeaveCriticalSection(&apt->cs);
}

void COM_CreateIObj(APARTMENT *apt, OXID oxid, OID oid, IPID ipid, IOBJECT **iobj, RPC_BINDING_HANDLE bind)
{
  IOBJECT *obj;
  RpcBindingSetObject(bind, &ipid);
  EnterCriticalSection(&apt->cs);
  for (obj = apt->proxies; obj && (obj->oxid != oxid || obj->oid != oid); obj = obj->next);
  if (!obj) {
    obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IOBJECT));
    obj->lpVtbl = &ProxyMan_VTable;
    obj->parent = apt;
    obj->oxid = oxid;
    obj->oid = oid;
    obj->ipid = ipid;
    if (bind)
      if ((obj->chan = RpcChannel_Create(bind, apt->model, apt->tid)) != NULL) bind = 0;
    obj->refs = 1;
    obj->next = apt->proxies;
    apt->proxies = obj;
  }
  if (iobj) *iobj = obj;
  LeaveCriticalSection(&apt->cs);
  if (bind) RpcBindingFree(&bind);
}

/******************** STANDARD MARSHALER ********************/

typedef struct StdMarshalImpl {
  ICOM_VTABLE(IMarshal) *lpVtbl;
  DWORD ref;
} StdMarshalImpl;

static HRESULT WINAPI StdMarshal_QueryInterface(LPMARSHAL iface,
						REFIID riid,
						LPVOID *obj)
{
  ICOM_THIS(StdMarshalImpl,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IMarshal,riid)) {
    *obj = This;
    This->ref++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI StdMarshal_AddRef(LPMARSHAL iface)
{
  ICOM_THIS(StdMarshalImpl,iface);
  TRACE("(%p)->AddRef()\n",This);
  return ++(This->ref);
}

static ULONG WINAPI StdMarshal_Release(LPMARSHAL iface)
{
  ICOM_THIS(StdMarshalImpl,iface);
  TRACE("(%p)->Release()\n",This);
  if (!--(This->ref)) {
    HeapFree(GetProcessHeap(),0,This);
    return 0;
  }
  return This->ref;
}

static DWORD GetDualStringArray(LPWSTR *binds, DWORD *sec_ofs)
{
  RPC_BINDING_VECTOR *bind = NULL;
  DWORD count, len = 0;
  STRINGBINDING **sbind = NULL;
  unsigned u;

  if (binds) *binds = NULL;
  if (sec_ofs) *sec_ofs = 0;
  RpcServerInqBindings(&bind);
  count = bind->Count;
  if (binds) sbind = HeapAlloc(GetProcessHeap(), 0, count*sizeof(STRINGBINDING*));
  for (u=0; u<count; u++) {
    LPWSTR sb, protseq, nsb;
    RPC_BINDING_HANDLE bindh = bind->BindingH[u];
    DWORD clen;

    /* get string binding */
    RpcBindingToStringBindingW(bindh, &sb);
    /* skip the object, if any */
    protseq = strchrW(sb, '@');
    if (protseq) protseq++; else protseq = sb;
    /* grab the protseq */
    nsb = strchrW(protseq, ':');
    if (nsb) { *nsb++ = 0; }
    TRACE("protseq: %s\n", debugstr_w(protseq));
    TRACE("net-addr: %s\n", debugstr_w(nsb));
    /* the rest becomes the marshaled string binding */
    /* (the null terminator is included in sizeof(STRINGBINDING)) */
    clen = sizeof(STRINGBINDING) + strlenW(nsb)*sizeof(WCHAR);
    len += clen;
    if (sbind) {
      STRINGBINDING *cbind = HeapAlloc(GetProcessHeap(), 0, clen);
      sbind[u] = cbind;
      /* FIXME: lookup the protseq tower id... */
      cbind->wTowerId = 0;
      strcpyW(cbind->aNetworkAddr, nsb);
      RpcStringFreeW(&sb);
    }
  }
  len += sizeof(WCHAR);
  if (sec_ofs) *sec_ofs = len;
  /* FIXME: security bindings */
  RpcBindingVectorFree(&bind);
  if (sbind) {
    LPBYTE ptr;
    *binds = HeapAlloc(GetProcessHeap(), 0, len);
    ptr = (LPBYTE)*binds;
    for (u=0; u<count; u++) {
      STRINGBINDING *cbind = sbind[u];
      DWORD clen = sizeof(STRINGBINDING) + strlenW(cbind->aNetworkAddr)*sizeof(WCHAR);
      memcpy(ptr, cbind, clen);
      ptr += clen;
      HeapFree(GetProcessHeap(), 0, cbind);
    }
    *(WCHAR*)ptr = 0;
    ptr += sizeof(WCHAR);
    TRACE("len=%ld (%d)\n", len, ptr-(LPBYTE)*binds);
    HeapFree(GetProcessHeap(), 0, sbind);
  }
  else {
    TRACE("len=%ld\n", len);
  }
  return len;
}

static RPC_BINDING_HANDLE ParseDualStringArray(LPWSTR binds, DWORD len, DWORD sec_ofs)
{
  RPC_BINDING_HANDLE bind = 0;
  RPC_STATUS status;
  LPBYTE ptr;
  LPWSTR sb;
  STRINGBINDING *cbind;
  DWORD pos, lim, clen, plen, slen;
  static const WCHAR def_protseq[] = {'n','c','a','l','r','p','c',0};

  TRACE("(%p,%ld,%ld)\n", binds, len, sec_ofs);

  if (!len) return 0;
  if (sec_ofs >= len) sec_ofs = 0;

  ptr = (LPBYTE)binds;
  pos = 0;
  lim = (sec_ofs ? sec_ofs : len) - 1;
  while ((!bind) && (pos < lim)) {
    cbind = (STRINGBINDING *)(ptr+pos);

    pos += sizeof(STRINGBINDING) + strlenW(cbind->aNetworkAddr)*sizeof(WCHAR);
    if (pos > lim) {
      ERR("malformed dual string array (%ld > %ld)\n", pos, lim);
      return 0;
    }
    TRACE("protseq: %s\n", debugstr_w(def_protseq));
    TRACE("net-addr: %s\n", debugstr_w(cbind->aNetworkAddr));

    /* FIXME: lookup the protseq tower id... */
    plen = strlenW(def_protseq);
    clen = strlenW(cbind->aNetworkAddr) + 1;
    slen = plen + 1 + clen;
    sb = HeapAlloc(GetProcessHeap(), 0, slen*sizeof(WCHAR));
    strcpyW(sb, def_protseq);
    sb[plen] = ':';
    strcpyW(sb + plen + 1, cbind->aNetworkAddr);

    /* try to get a RPC binding */
    status = RpcBindingFromStringBindingW(sb, &bind);

    HeapFree(GetProcessHeap(), 0, sb);
  }

  if (!bind) return 0;

  /* make sure string binding array is null-terminated */
  if (*(WCHAR*)(ptr + pos)) {
    ERR("malformed dual string array (not terminated)\n");
    RpcBindingFree(&bind);
    return 0;
  }
  ptr += sizeof(WCHAR);

  /* FIXME: security bindings */

  return bind;
}

static HRESULT WINAPI StdMarshal_GetUnmarshalClass(LPMARSHAL iface,
						   REFIID riid,
						   LPVOID pv,
						   DWORD dwDestContext,
						   LPVOID pvDestContext,
						   DWORD mshlFlags,
						   CLSID *pCid)
{
  ICOM_THIS(StdMarshalImpl,iface);
  TRACE("(%p)->GetUnmarshalClass(%s,%p,%lx,%p,%lx,%p)\n",This,
	debugstr_guid(riid),pv,dwDestContext,pvDestContext,mshlFlags,pCid);
  /* return CLSID of the standard marshaller */
  memcpy(pCid, &CLSID_StdMarshal,sizeof(*pCid));
  return S_OK;
}

static HRESULT WINAPI StdMarshal_GetMarshalSizeMax(LPMARSHAL iface,
						   REFIID riid,
						   LPVOID pv,
						   DWORD dwDestContext,
						   LPVOID pvDestContext,
						   DWORD mshlFlags,
						   DWORD *pSize)
{
  ICOM_THIS(StdMarshalImpl,iface);
  APARTMENT *apt = COM_CurrentApt();
  DWORD size;

  TRACE("(%p)->GetMarshalSizeMax(%s,%p,%lx,%p,%lx,%p)\n",This,
	debugstr_guid(riid),pv,dwDestContext,pvDestContext,mshlFlags,pSize);
  if (!apt) return CO_E_NOTINITIALIZED;

  COM_RpcInit();

  size = sizeof(struct OR_STANDARD);
  size += GetDualStringArray(NULL, NULL);
  *pSize = size;
  return S_OK;
}

static HRESULT WINAPI StdMarshal_MarshalInterface(LPMARSHAL iface,
						  LPSTREAM pStm,
						  REFIID riid,
						  LPVOID pv,
						  DWORD dwDestContext,
						  LPVOID pvDestContext,
						  DWORD mshlFlags)
{
  ICOM_THIS(StdMarshalImpl,iface);
  APARTMENT *apt = COM_CurrentApt();
  DWORD refs;
  struct OR_STANDARD obj;
  LPWSTR dsastr;
  DWORD dsalen, secofs;
  HRESULT hr;
  XOBJECT *xobj;
  XIF *xif;

  TRACE("(%p)->MarshalInterface(%p,%s,%p,%lx,%p,%lx)\n",This,pStm,
	debugstr_guid(riid),pv,dwDestContext,pvDestContext,mshlFlags);
  if (!apt) return CO_E_NOTINITIALIZED;

  COM_RpcInit();

  memset(&obj, 0, sizeof(obj));
  if (mshlFlags & MSHLFLAGS_TABLEWEAK) {
    /* marshaled data is 0 external refs */
    obj.std.cPublicRefs = 0;
    TRACE("table-weak marshal\n");
    refs = 0;
  }
  else if (mshlFlags & MSHLFLAGS_TABLESTRONG) {
    /* marshaled data is 1 external ref */
    obj.std.cPublicRefs = 0;
    obj.std.flags |= SORF_TABLESTRONG;
    TRACE("table-strong marshal\n");
    refs = 1;
  }
  else {
    /* 5 external refs transferred in marshaled data */
    refs = obj.std.cPublicRefs = 5;
  }
  if (mshlFlags & MSHLFLAGS_NOPING)
    obj.std.flags |= SORF_NOPING;

  EnterCriticalSection(&apt->cs);
  COM_CreateXObj(apt, &xobj, &xif, riid, pv);
  if (xif) {
    obj.std.oxid = apt->oxid;
    obj.std.oid  = xobj->oid;
    obj.std.ipid = xif->ipid;
    COM_ExtAddRef(apt, xobj, xif, refs);
    hr = xif->hres;
    if (FAILED(hr))
      COM_ExtRelease(apt, xobj, xif, refs);
  }
  else hr = E_OUTOFMEMORY;
  LeaveCriticalSection(&apt->cs);

  if (FAILED(hr)) return hr;

  dsalen = GetDualStringArray(&dsastr, &secofs);
  obj.saResAddr.wNumEntries = dsalen / sizeof(WCHAR);
  obj.saResAddr.wSecurityOffset = secofs / sizeof(WCHAR);

  hr = IStream_Write(pStm, &obj, sizeof(obj), NULL);
  if (SUCCEEDED(hr)) {
    hr = IStream_Write(pStm, dsastr, dsalen, NULL);
  }

  HeapFree(GetProcessHeap(), 0, dsastr);

  return hr;
}

static HRESULT WINAPI StdMarshal_UnmarshalInterface(LPMARSHAL iface,
						    LPSTREAM pStm,
						    REFIID riid,
						    LPVOID *ppv)
{
  ICOM_THIS(StdMarshalImpl,iface);
  APARTMENT *apt = COM_CurrentApt();
  struct OR_STANDARD obj;
  LPWSTR dsastr = NULL;
  DWORD dsalen = 0, secofs = 0;
  HRESULT hr;
  IOBJECT *iobj;
  IIF *iif;

  TRACE("(%p)->UnmarshalInterface(%p,%s,%p)\n",This,pStm,
	debugstr_guid(riid),ppv);
  *ppv = NULL;
  if (!apt) return CO_E_NOTINITIALIZED;

  hr = IStream_Read(pStm, &obj, sizeof(obj), NULL);
  if (FAILED(hr)) return hr;
  if (obj.saResAddr.wNumEntries) {
    dsalen = obj.saResAddr.wNumEntries*sizeof(WCHAR);
    secofs = obj.saResAddr.wSecurityOffset*sizeof(WCHAR);
    dsastr = HeapAlloc(GetProcessHeap(), 0, dsalen);
    hr = IStream_Read(pStm, dsastr, dsalen, NULL);
    if (FAILED(hr)) goto finito;
  }

  if (obj.std.oxid == apt->oxid) {
    XOBJECT *xobj;
    XIF *xif;
    /* dude, we're still in the same apartment...
     * we don't need no stinkin' proxy */
    EnterCriticalSection(&apt->cs);
    COM_FindXObj(apt, obj.std.oid, obj.std.ipid, &xobj, &xif);
    if (xif) {
      UNKNOWN_ADDREF((LPUNKNOWN)xif->iface);
      *ppv = xif->iface;
      COM_ExtRelease(apt, xobj, xif, obj.std.cPublicRefs);
      hr = S_OK;
    }
    else hr = CO_E_OBJNOTCONNECTED;
    LeaveCriticalSection(&apt->cs);
    goto finito;
  }

  hr = E_FAIL;

  /* see if object has already been imported */
  EnterCriticalSection(&apt->cs);
  COM_FindIObj(apt, obj.std.oxid, obj.std.oid, obj.std.ipid, &iobj, &iif);

  if (!iobj) {
    /* object not already imported, connect to it */
    RPC_BINDING_HANDLE bind = ParseDualStringArray(dsastr, dsalen, secofs);
    if (bind) /* register new connection */
      COM_CreateIObj(apt, obj.std.oxid, obj.std.oid, obj.std.ipid, &iobj, bind);
    else
      hr = CO_E_OBJNOTCONNECTED;
  }
  else iobj->refs++;
  if (iobj && !iif) {
    /* object imported, register new interface */
    COM_CreateIIf(apt, obj.std.ipid, iobj, &iif, riid);
  }

  if (iif) {
    iif->refs += obj.std.cPublicRefs;
    hr = iif->hres;
    if (SUCCEEDED(hr) && !iif->refs) {
      /* we got no references by unmarshaling (hmm, table-marshal?),
       * must explicitly request some references */
      REMINTERFACEREF ref;
      HRESULT rhr;
      ref.ipid = iobj->ipid;
      ref.cPublicRefs = 5;
      ref.cPrivateRefs = 0;
      hr = IRemUnknown_RemAddRef((LPREMUNKNOWN)iobj, 1, &ref, &rhr);
      if (SUCCEEDED(hr)) hr = rhr;
      if (SUCCEEDED(hr)) iif->refs += ref.cPublicRefs;
    }

    /* unmarshal complete */
    *ppv = iif->iface;
  }

  LeaveCriticalSection(&apt->cs);

finito:
  if (dsastr) HeapFree(GetProcessHeap(), 0, dsastr);
  return hr;
}

static HRESULT WINAPI StdMarshal_ReleaseMarshalData(LPMARSHAL iface,
						    LPSTREAM pStm)
{
  ICOM_THIS(StdMarshalImpl,iface);
  APARTMENT *apt = COM_CurrentApt();
  DWORD refs;
  struct OR_STANDARD obj;
  HRESULT hr;

  TRACE("(%p)->ReleaseMarshalData(%p)\n",This,pStm);
  if (!apt) return CO_E_NOTINITIALIZED;

  hr = IStream_Read(pStm, &obj, sizeof(obj), NULL);
  if (FAILED(hr)) return hr;
  /* FIXME: read string bindings */
  /* FIXME: read security bindings */

  if (obj.std.oxid == apt->oxid) {
    XOBJECT *xobj;
    XIF *xif;
    refs = obj.std.cPublicRefs;
    if (obj.std.flags & SORF_TABLESTRONG) refs += 1;
    EnterCriticalSection(&apt->cs);
    COM_FindXObj(apt, obj.std.oid, obj.std.ipid, &xobj, &xif);
    if (xif) {
      COM_ExtRelease(apt, xobj, xif, refs);
      hr = S_OK;
    }
    else hr = CO_E_OBJNOTCONNECTED;
    LeaveCriticalSection(&apt->cs);
    return hr;
  }

  FIXME("interapartment marshal release\n");
  return RPC_E_WRONG_THREAD;
}

static HRESULT WINAPI StdMarshal_DisconnectObject(LPMARSHAL iface,
						  DWORD dwReserved)
{
  ICOM_THIS(StdMarshalImpl,iface);
  TRACE("(%p)->DisconnectObject(%ld)\n",This,dwReserved);
  /* FIXME */
  return S_OK;
}

static ICOM_VTABLE(IMarshal) StdMarshalImpl_VTable =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  StdMarshal_QueryInterface,
  StdMarshal_AddRef,
  StdMarshal_Release,
  StdMarshal_GetUnmarshalClass,
  StdMarshal_GetMarshalSizeMax,
  StdMarshal_MarshalInterface,
  StdMarshal_UnmarshalInterface,
  StdMarshal_ReleaseMarshalData,
  StdMarshal_DisconnectObject
};

static HRESULT COM_GetMarshal(REFIID riid,
			      IUnknown *pUnk,
			      DWORD dwDestContext,
			      void *pvDestContext,
			      DWORD mshlFlags,
			      LPMARSHAL *pMarshal)
{
  HRESULT hr;
  hr = IUnknown_QueryInterface(pUnk, &IID_IMarshal, (LPVOID*)pMarshal);
  if (hr == E_NOINTERFACE) {
    TRACE("no custom marshaler found, using standard marshaler\n");
    hr = CoGetStandardMarshal(riid, pUnk, dwDestContext, pvDestContext,
			      mshlFlags, pMarshal);
  }
  return hr;
}

static HRESULT COM_GetUnmarshal(IStream *pStm,
				LPMARSHAL *pMarshal,
				LPIID piid)
{
  HRESULT hr;
  OBJREF obj;
  struct OR_CUSTOM cobj;
  ULONG count;

  /* get administrative data */
  hr = IStream_Read(pStm, &obj, sizeof(obj), &count);
  if (FAILED(hr)) return hr;
  if (count != sizeof(obj)) return E_FAIL;

  if (piid) memcpy(piid, &obj.iid, sizeof(obj.iid));

  /* get unmarshal class instance */
  if (obj.flags & OBJREF_CUSTOM) {
    hr = IStream_Read(pStm, &cobj, sizeof(cobj), &count);
    if (FAILED(hr)) return hr;
    if (count != sizeof(cobj)) return E_FAIL;

    hr = CoCreateInstance(&cobj.clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMarshal, (LPVOID*)pMarshal);
  }
  else {
    hr = CoGetStandardMarshal(NULL, NULL, 0, NULL, 0, pMarshal);
  }
  return hr;
}

HRESULT COM_GetStdObjRef(IStream *pStm,
			 STDOBJREF *pObjRef,
			 LPIID piid)
{
  HRESULT hr;
  OBJREF obj;
  LARGE_INTEGER move, opos;
  ULONG count;

  /* save initial position */
  move.s.LowPart = 0;
  move.s.HighPart = 0;
  hr = IStream_Seek(pStm, move, STREAM_SEEK_CUR, (ULARGE_INTEGER*)&opos);
  if (FAILED(hr)) return hr;

  /* get administrative data */
  hr = IStream_Read(pStm, &obj, sizeof(obj), &count);
  if (FAILED(hr)) return hr;
  if (count != sizeof(obj)) return E_FAIL;

  if (piid) memcpy(piid, &obj.iid, sizeof(obj.iid));

  /* read objref */
  if (obj.flags & OBJREF_CUSTOM) {
    hr = E_INVALIDARG;
  }
  else {
    hr = IStream_Read(pStm, pObjRef, sizeof(STDOBJREF), &count);
  }

  /* return to initial position */
  IStream_Seek(pStm, opos, STREAM_SEEK_SET, NULL);

  return hr;
}

/******************** API FUNCTIONS ********************/

HRESULT WINAPI CoGetStandardMarshal(REFIID riid,
				    IUnknown *pUnk,
				    DWORD dwDestContext,
				    LPVOID pvDestContext,
				    DWORD mshlFlags,
				    LPMARSHAL *ppMarshal)
{
  StdMarshalImpl *pMarshal;
  pMarshal = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(StdMarshalImpl));
  if (!pMarshal) return E_OUTOFMEMORY;
  pMarshal->lpVtbl = &StdMarshalImpl_VTable;
  pMarshal->ref = 1;
  *ppMarshal = (LPMARSHAL)pMarshal;
  return S_OK;
}

HRESULT WINAPI CoGetMarshalSizeMax(ULONG *pulSize,
				   REFIID riid,
				   IUnknown *pUnk,
				   DWORD dwDestContext,
				   void *pvDestContext,
				   DWORD mshlFlags)
{
  HRESULT hr = S_OK;
  CLSID clsid;
  ULONG size = 0;
  LPMARSHAL pMarshal;

  *pulSize = 0;

  /* get an IMarshal interface */
  hr = COM_GetMarshal(riid, pUnk, dwDestContext, pvDestContext, mshlFlags, &pMarshal);
  if (FAILED(hr)) return hr;

  /* get unmarshal class */
  hr = IMarshal_GetUnmarshalClass(pMarshal, riid, pUnk, dwDestContext, pvDestContext,
				  mshlFlags, &clsid);
  if (FAILED(hr)) goto fail;

  /* get marshal size */
  hr = IMarshal_GetMarshalSizeMax(pMarshal, riid, pUnk, dwDestContext, pvDestContext,
				  mshlFlags, &size);
  if (FAILED(hr)) goto fail;

  if (!IsEqualGUID(&clsid, &CLSID_StdMarshal)) size += sizeof(struct OR_CUSTOM);
  *pulSize = sizeof(OBJREF) + size;

fail:
  IMarshal_Release(pMarshal);

  return hr;
}

HRESULT WINAPI CoMarshalInterface(IStream *pStm,
				  REFIID riid,
				  IUnknown *pUnk,
				  DWORD dwDestContext,
				  void *pvDestContext,
				  DWORD mshlFlags)
{
  HRESULT hr;
  CLSID clsid;
  LPMARSHAL pMarshal;
  OBJREF obj;
  struct OR_CUSTOM cobj;
  ULONG count;

  /* get an IMarshal interface */
  hr = COM_GetMarshal(riid, pUnk, dwDestContext, pvDestContext, mshlFlags, &pMarshal);
  if (FAILED(hr)) return hr;

  /* get unmarshal class */
  hr = IMarshal_GetUnmarshalClass(pMarshal, riid, pUnk, dwDestContext, pvDestContext,
				  mshlFlags, &clsid);
  if (FAILED(hr)) goto fail;

  /* write marshal header */
  obj.signature = OBJREF_SIGNATURE;
  if (!IsEqualGUID(&clsid, &CLSID_StdMarshal))
    obj.flags = OBJREF_CUSTOM;
  else
    obj.flags = OBJREF_STANDARD;
  memcpy(&obj.iid, riid, sizeof(obj.iid));

  hr = IStream_Write(pStm, &obj, sizeof(obj), &count);
  if (FAILED(hr)) goto fail;
  if (count != sizeof(obj)) {
    hr = E_FAIL;
    goto fail;
  }

  if (obj.flags == OBJREF_CUSTOM) {
    memcpy(&cobj.clsid, &clsid, sizeof(cobj.clsid));
    cobj.cbExtension = 0;
    cobj.size = 0;

    hr = IStream_Write(pStm, &cobj, sizeof(cobj), &count);
    if (FAILED(hr)) goto fail;
    if (count != sizeof(cobj)) {
      hr = E_FAIL;
      goto fail;
    }
  }

  /* perform the marshaling */
  hr = IMarshal_MarshalInterface(pMarshal, pStm, riid, pUnk, dwDestContext, pvDestContext,
				 mshlFlags);


  if (obj.flags == OBJREF_CUSTOM) {
    FIXME("update custom-marshal size\n");
  }

fail:
  IMarshal_Release(pMarshal);

  return hr;
}

HRESULT WINAPI CoUnmarshalInterface(IStream *pStm,
				    REFIID riid,
				    void **ppv)
{
  HRESULT hr;
  LPMARSHAL pMarshal;
  IID iid;
  void *pv;

  hr = COM_GetUnmarshal(pStm, &pMarshal, &iid);
  if (FAILED(hr)) return hr;

  hr = IMarshal_UnmarshalInterface(pMarshal, pStm, &iid, &pv);
  IMarshal_Release(pMarshal);
  if (FAILED(hr)) return hr;

  if (IsEqualGUID(riid, &IID_NULL) ||
      IsEqualGUID(riid, &iid)) {
    *ppv = pv;
  } else {
    hr = IUnknown_QueryInterface((LPUNKNOWN)pv, riid, ppv);
    IUnknown_Release((LPUNKNOWN)pv);
  }
  return hr;
}

HRESULT WINAPI CoReleaseMarshalData(IStream *pStm)
{
  HRESULT hr;
  LPMARSHAL pMarshal;

  hr = COM_GetUnmarshal(pStm, &pMarshal, NULL);
  if (FAILED(hr)) return hr;

  hr = IMarshal_ReleaseMarshalData(pMarshal, pStm);

  IMarshal_Release(pMarshal);

  return hr;
}

/* #define FAKE_INTERTHREAD */

HRESULT WINAPI CoMarshalInterThreadInterfaceInStream(REFIID riid,
						     LPUNKNOWN pUnk,
						     LPSTREAM *ppStm)
{
  HRESULT hr = CreateStreamOnHGlobal(0, TRUE, ppStm);
  if (SUCCEEDED(hr)) {
    LARGE_INTEGER pos;
    DWORD not_null = (pUnk != NULL);
    hr = IStream_Write(*ppStm, &not_null, sizeof(not_null), NULL);
    if (FAILED(hr)) return hr;
    if (not_null) {
#ifdef FAKE_INTERTHREAD
      hr = IStream_Write(*ppStm, &pUnk, sizeof(pUnk), NULL);
      if (SUCCEEDED(hr)) UNKNOWN_ADDREF(pUnk);
      TRACE("<= %p\n", pUnk);
#else
      hr = CoMarshalInterface(*ppStm, riid, pUnk, MSHCTX_INPROC, 0, MSHLFLAGS_NORMAL);
#endif
    }
    pos.s.LowPart = 0;
    pos.s.HighPart = 0;
    IStream_Seek(*ppStm, pos, 0, NULL);
  }
  return hr;
}

HRESULT WINAPI CoGetInterfaceAndReleaseStream(LPSTREAM pStm,
					      REFIID riid,
					      LPVOID *ppv)
{
  DWORD not_null;
  HRESULT hr = IStream_Read(pStm, &not_null, sizeof(not_null), NULL);
  *ppv = NULL;
  if (SUCCEEDED(hr) && not_null) {
#ifdef FAKE_INTERTHREAD
    LPUNKNOWN pUnk;
    hr = IStream_Read(pStm, &pUnk, sizeof(pUnk), NULL);
    if (SUCCEEDED(hr)) {
      hr = IUnknown_QueryInterface(pUnk, riid, ppv);
      TRACE("=> %p\n", *ppv);
      IUnknown_Release(pUnk);
    }
#else
    hr = CoUnmarshalInterface(pStm, riid, ppv);
#endif
  }
  IStream_Release(pStm);
  return hr;
}

/******************** BUILT-IN P/S MARSHALERS ********************/
/* we need to create an IDL compiler for Wine that can generate
 * most of these automatically */

const MIDL_STUB_DESC Object_StubDesc = {
  0,
  NdrOleAllocate,
  NdrOleFree,
  {0}, 0, 0, 0, 0,
  0 /* __MIDL_TypeFormatString.Format */
};

#define INTERFACES 2

HRESULT WINAPI IClassFactory_CreateInstance_Proxy(LPCLASSFACTORY This,
						  LPUNKNOWN pUnkOuter,
						  REFIID riid,
						  LPVOID *ppv)
{
  HRESULT ret = E_FAIL;
  RPC_MESSAGE Msg;
  MIDL_STUB_MESSAGE StubMsg;
  LPUNKNOWN pvObj = NULL;
  BOOL not_null;

  TRACE("(%p,%p,%s,%p)\n", This, pUnkOuter, debugstr_guid(riid), ppv);
  *ppv = NULL;

  NdrProxyInitialize(This, &Msg, &StubMsg, &Object_StubDesc, 3);

  StubMsg.BufferLength = sizeof(IID);
  NdrProxyGetBuffer(This, &StubMsg);

  /* FIXME: use NdrSimpleStructMarshall instead of memcpy */
  memcpy(StubMsg.Buffer, riid, sizeof(IID));
  StubMsg.Buffer += sizeof(IID);

  NdrProxySendReceive(This, &StubMsg);

  /* FIXME: NdrPointerUnmarshall */
  memcpy(&not_null, StubMsg.Buffer, sizeof(BOOL));
  StubMsg.Buffer += sizeof(BOOL);
  if (not_null)
    NdrInterfacePointerUnmarshall(&StubMsg,
				  (unsigned char**)&pvObj,
				  NULL,
				  0);
  memcpy(&ret, StubMsg.Buffer, sizeof(HRESULT));
  StubMsg.Buffer += sizeof(HRESULT);

  NdrProxyFreeBuffer(This, &StubMsg);

  if (pvObj) {
    ret = IUnknown_QueryInterface(pvObj, riid, ppv);
    IUnknown_Release(pvObj);
  }

  return ret;
}

HRESULT WINAPI IClassFactory_LockServer_Proxy(LPCLASSFACTORY This,
					      BOOL fLock)
{
  FIXME("(%p,%d):stub\n", This, fLock);
  return S_OK;
}

const CINTERFACE_PROXY_VTABLE(5) IClassFactoryProxyVtbl = {
  {&IID_IClassFactory},
  {IUnknown_QueryInterface_Proxy,
   IUnknown_AddRef_Proxy,
   IUnknown_Release_Proxy,
   IClassFactory_CreateInstance_Proxy,
   IClassFactory_LockServer_Proxy}
};

/* FIXME: this marshalling is not done exactly like MS's,
 * so it needs to be fixed for real DCOM, but for real DCOM
 * we should probably implement a real IDL compiler anyway */

void __RPC_STUB IClassFactory_RemoteCreateInstance_Stub(LPRPCSTUBBUFFER This,
							LPRPCCHANNELBUFFER pChannel,
							PRPC_MESSAGE pMsg,
							DWORD *pdwPhase)
{
  LPCLASSFACTORY pServer = (LPCLASSFACTORY)((CStdStubBuffer*)This)->pvServerObject;
  HRESULT ret = E_FAIL;
  MIDL_STUB_MESSAGE StubMsg;
  IID iid;
  LPVOID pvObj = NULL;
  BOOL not_null;

  TRACE("(%p,%p,%p,%p)\n", This, pChannel, pMsg, pdwPhase);

  NdrStubInitialize(pMsg, &StubMsg, &Object_StubDesc, pChannel);
  /* FIXME: use NdrSimpleStructUnmarshall instead of memcpy */
  memcpy(&iid, StubMsg.Buffer, sizeof(IID));
  StubMsg.Buffer += sizeof(IID);

  *pdwPhase = STUB_CALL_SERVER;
  TRACE("->(%p,NULL,%s,%p)\n", pServer, debugstr_guid(&iid), &pvObj);
  TRACE("vtbl=%p\n", pServer->lpVtbl);
  ret = IClassFactory_CreateInstance(pServer, NULL, &iid, &pvObj);
  TRACE("<-(%p) %08lx\n", pvObj, ret);
  not_null = (pvObj != NULL);
  *pdwPhase = STUB_MARSHAL;

  StubMsg.BufferLength = sizeof(BOOL);
  if (not_null) {
    /* seems MaxCount is used to store the riid for interface marshaling */
    StubMsg.MaxCount = (ULONG_PTR)&iid;
    NdrInterfacePointerBufferSize(&StubMsg,
				  (unsigned char*)pvObj,
				  NULL);
  }
  StubMsg.BufferLength += sizeof(HRESULT);
  NdrStubGetBuffer(This, pChannel, &StubMsg);
  /* FIXME: NdrPointerMarshall */
  memcpy(StubMsg.Buffer, &not_null, sizeof(BOOL));
  StubMsg.Buffer += sizeof(BOOL);
  if (not_null) {
    StubMsg.MaxCount = (ULONG_PTR)&iid;
    NdrInterfacePointerMarshall(&StubMsg,
				(unsigned char*)pvObj,
				NULL);
    UNKNOWN_RELEASE((LPUNKNOWN)pvObj);
  }
  memcpy(StubMsg.Buffer, &ret, sizeof(HRESULT));
  StubMsg.Buffer += sizeof(HRESULT);
}

void __RPC_STUB IClassFactory_RemoteLockServer_Stub(LPRPCSTUBBUFFER This,
						    LPRPCCHANNELBUFFER pChannel,
						    PRPC_MESSAGE pMsg,
						    DWORD *pdwPhase)
{
  FIXME("(%p,%p,%p,%p):stub\n", This, pChannel, pMsg, pdwPhase);
}

const PRPC_STUB_FUNCTION IClassFactory_table[] = {
  IClassFactory_RemoteCreateInstance_Stub,
  IClassFactory_RemoteLockServer_Stub
};

const CInterfaceStubVtbl IClassFactoryStubVtbl = {
  {&IID_IClassFactory,
   0,
   5,
   &IClassFactory_table[-3]},
  {CStdStubBuffer_METHODS}
};

const CInterfaceProxyVtbl* _compobj_ProxyVtblList[INTERFACES+1] = {
  0,
  (CInterfaceProxyVtbl*)&IClassFactoryProxyVtbl,
  0
};

const CInterfaceStubVtbl* _compobj_StubVtblList[INTERFACES+1] = {
  0,
  (CInterfaceStubVtbl*)&IClassFactoryStubVtbl,
  0
};

const PCInterfaceName _compobj_InterfaceNamesList[INTERFACES+1] = {
  "IRemUnknown",
  "IClassFactory",
  0
};

int __stdcall _compobj_IID_Lookup(const IID *pIID, int *pIndex)
{
  unsigned u;
  /* the purpose of this routine is probably to "unroll"
   * this kind of for loop, but without an IDL compiler,
   * maintainability is more important */
  for (u=1; u<INTERFACES; u++) {
    if (!IID_GENERIC_CHECK_IID(_compobj, pIID, u)) {
      *pIndex = u;
      return 1;
    }
  }
  return 0;
}

const ExtendedProxyFileInfo compobj_ProxyFileInfo = {
  (PCInterfaceProxyVtblList*)&_compobj_ProxyVtblList,
  (PCInterfaceStubVtblList*)&_compobj_StubVtblList,
  (const PCInterfaceName*)&_compobj_InterfaceNamesList,
  0, /* pDelegatedIIDs */
  &_compobj_IID_Lookup,
  INTERFACES, /* TableSize */
  1  /* TableVersion */
};

const ProxyFileInfo* OLE32_ProxyFileList[] = {
  &compobj_ProxyFileInfo,
  NULL
};

static CStdPSFactoryBuffer PSFactoryBuffer;

CSTDSTUBBUFFERRELEASE(&PSFactoryBuffer)

static HRESULT COM_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
  return NdrDllGetClassObject(rclsid, riid, ppv, OLE32_ProxyFileList,
			      &CLSID_PSFactoryBuffer, &PSFactoryBuffer);
}


/***********************************************************************
 *           DllGetClassObject [OLE32.63]
 */
HRESULT WINAPI OLE32_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
  if (IsEqualGUID(rclsid, &CLSID_PSFactoryBuffer)) {
    return COM_DllGetClassObject(rclsid, riid, ppv);
  }
  FIXME("\n\tCLSID:\t%s,\n\tIID:\t%s\n",debugstr_guid(rclsid),debugstr_guid(riid));
  return CLASS_E_CLASSNOTAVAILABLE;
}


/****************/

void create_proxy_cs(void)
{
  InitializeCriticalSection( &creq_cs );
  CRITICAL_SECTION_NAME( &creq_cs, "csRpcRequest" );

  InitializeCriticalSection( &csRegIf );
  CRITICAL_SECTION_NAME( &csRegIf, "csRegIf" );
}

void destroy_proxy_cs(void)
{
  DeleteCriticalSection( &csRegIf );
  DeleteCriticalSection( &creq_cs );
}



