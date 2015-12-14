/*
 * RPC server API
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * TODO:
 *  - a whole lot
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"

#include "rpc.h"
#include "msvcrt/excpt.h"

#include "wine/debug.h"
#include "wine/exception.h"
#include "msvcrt/excpt.h"

#include "rpc_server.h"
#include "rpc_misc.h"
#include "rpc_defs.h"

#define MAX_THREADS 20
#define MAX_IDLE_THREADS 4

WINE_DEFAULT_DEBUG_CHANNEL(ole);

typedef struct _RpcPacket
{
  struct _RpcPacket* next;
  struct _RpcConnection* conn;
  RpcPktHdr hdr;
  void* buf;
} RpcPacket;

static RpcServerProtseq* protseqs;
static RpcServerInterface* ifs;

static CRITICAL_SECTION server_cs;
static CRITICAL_SECTION listen_cs;
static BOOL std_listen;
static LONG listen_count = -1;
static HANDLE mgr_event, server_thread, server_event;

static CRITICAL_SECTION spacket_cs;
static RpcPacket* spacket_head;
static RpcPacket* spacket_tail;
static HANDLE server_sem;

static DWORD worker_count, worker_free, worker_tls;

void create_server_cs(void)
{
  CRITICAL_SECTION_DEFINE( &server_cs );
  CRITICAL_SECTION_DEFINE( &listen_cs );
  CRITICAL_SECTION_DEFINE( &spacket_cs );
}

void destroy_server_cs(void)
{

  DeleteCriticalSection( &spacket_cs );
  DeleteCriticalSection( &listen_cs );
  DeleteCriticalSection( &server_cs );
}


static RpcServerInterface* RPCRT4_find_interface(UUID* object, UUID* if_id)
{
  UUID* MgrType = NULL;
  RpcServerInterface* cif = NULL;
  RPC_STATUS status;

  /* FIXME: object -> MgrType */
  EnterCriticalSection(&server_cs);
  cif = ifs;
  while (cif) {
    if (UuidEqual(if_id, &cif->If->InterfaceId.SyntaxGUID, &status) &&
       UuidEqual(MgrType, &cif->MgrTypeUuid, &status) &&
       (std_listen || (cif->Flags & RPC_IF_AUTOLISTEN))) break;
    cif = cif->Next;
  }
  LeaveCriticalSection(&server_cs);
  return cif;
}

static void RPCRT4_push_packet(RpcPacket* packet)
{
  packet->next = NULL;
  EnterCriticalSection(&spacket_cs);
  if (spacket_tail) spacket_tail->next = packet;
  else {
    spacket_head = packet;
    spacket_tail = packet;
  }
  LeaveCriticalSection(&spacket_cs);
}

static RpcPacket* RPCRT4_pop_packet(void)
{
  RpcPacket* packet;
  EnterCriticalSection(&spacket_cs);
  packet = spacket_head;
  if (packet) {
    spacket_head = packet->next;
    if (!spacket_head) spacket_tail = NULL;
  }
  LeaveCriticalSection(&spacket_cs);
  if (packet) packet->next = NULL;
  return packet;
}

typedef struct {
  PRPC_MESSAGE msg;
  void* buf;
} packet_state;

static WINE_EXCEPTION_FILTER(rpc_filter)
{
  packet_state* state;
  PRPC_MESSAGE msg;
  state = TlsGetValue(worker_tls);
  msg = state->msg;
  if (msg->Buffer != state->buf) I_RpcFreeBuffer(msg);
  msg->RpcFlags |= WINE_RPCFLAG_EXCEPTION;
  msg->BufferLength = sizeof(DWORD);
  I_RpcGetBuffer(msg);
  *(DWORD*)msg->Buffer = GetExceptionCode();
  return EXCEPTION_EXECUTE_HANDLER;
}

static void RPCRT4_process_packet(RpcConnection* conn, RpcPktHdr* hdr, void* buf)
{
  RpcBinding* pbind;
  RPC_MESSAGE msg;
  RpcServerInterface* sif;
  RPC_DISPATCH_FUNCTION func;
  packet_state state;

  state.msg = &msg;
  state.buf = buf;
  TlsSetValue(worker_tls, &state);
  memset(&msg, 0, sizeof(msg));
  msg.BufferLength = hdr->len;
  msg.Buffer = buf;
  sif = RPCRT4_find_interface(&hdr->object, &hdr->if_id);
  if (sif) {
    TRACE("packet received for interface %s\n", debugstr_guid(&hdr->if_id));
    msg.RpcInterfaceInformation = sif->If;
    /* create temporary binding for dispatch */
    RPCRT4_MakeBinding(&pbind, conn);
    RPCRT4_SetBindingObject(pbind, &hdr->object);
    msg.Handle = (RPC_BINDING_HANDLE)pbind;
    /* process packet */
    switch (hdr->ptype) {
    case PKT_REQUEST:
      /* find dispatch function */
      msg.ProcNum = hdr->opnum;
      if (sif->Flags & RPC_IF_OLE) {
        /* native ole32 always gives us a dispatch table with a single entry
         * (I assume that's a wrapper for IRpcStubBuffer::Invoke) */
        func = *sif->If->DispatchTable->DispatchTable;
      } else {
        if (msg.ProcNum >= sif->If->DispatchTable->DispatchTableCount) {
          ERR("invalid procnum\n");
          func = NULL;
        }
        func = sif->If->DispatchTable->DispatchTable[msg.ProcNum];
      }

      /* put in the drep. FIXME: is this more universally applicable?
         perhaps we should move this outward... */
      msg.DataRepresentation = 
        MAKELONG( MAKEWORD(hdr->drep[0], hdr->drep[1]),
                  MAKEWORD(hdr->drep[2], 0));

      /* dispatch */
      __TRY {
        if (func) func(&msg);
      } __EXCEPT(rpc_filter) {
        /* failure packet was created in rpc_filter */
        TRACE("exception caught, returning failure packet\n");
      } __ENDTRY

      /* send response packet */
      I_RpcSend(&msg);
      break;
    default:
      ERR("unknown packet type\n");
      break;
    }

    RPCRT4_DestroyBinding(pbind);
    msg.Handle = 0;
    msg.RpcInterfaceInformation = NULL;
  }
  else {
    ERR("got RPC packet to unregistered interface %s\n", debugstr_guid(&hdr->if_id));
  }

  /* clean up */
  if (msg.Buffer == buf) msg.Buffer = NULL;
  TRACE("freeing Buffer=%p\n", buf);
  HeapFree(GetProcessHeap(), 0, buf);
  I_RpcFreeBuffer(&msg);
  msg.Buffer = NULL;
  TlsSetValue(worker_tls, NULL);
}

static DWORD CALLBACK RPCRT4_worker_thread(LPVOID the_arg)
{
  HANDLE obj;
  RpcPacket* pkt;

  for (;;) {
    /* idle timeout after 5s */
    obj = WaitForSingleObject(server_sem, 5000);
    if (obj == WAIT_TIMEOUT) {
      /* if another idle thread exist, self-destruct */
      if (worker_free > MAX_IDLE_THREADS) break;
      continue;
    }
    pkt = RPCRT4_pop_packet();
    if (!pkt) continue;
    InterlockedDecrement(&worker_free);
    for (;;) {
      RPCRT4_process_packet(pkt->conn, &pkt->hdr, pkt->buf);
      HeapFree(GetProcessHeap(), 0, pkt);
      /* try to grab another packet here without waiting
       * on the semaphore, in case it hits max */
      pkt = RPCRT4_pop_packet();
      if (!pkt) break;
      /* decrement semaphore */
      WaitForSingleObject(server_sem, 0);
    }
    InterlockedIncrement(&worker_free);
  }
  InterlockedDecrement(&worker_free);
  InterlockedDecrement(&worker_count);
  return 0;
}

static void RPCRT4_create_worker_if_needed(void)
{
  if (!worker_free && worker_count < MAX_THREADS) {
    HANDLE thread;
    InterlockedIncrement(&worker_count);
    InterlockedIncrement(&worker_free);
    thread = CreateThread(NULL, 0, RPCRT4_worker_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
    else {
      InterlockedDecrement(&worker_free);
      InterlockedDecrement(&worker_count);
    }
  }
}

static DWORD CALLBACK RPCRT4_io_thread(LPVOID the_arg)
{
  RpcConnection* conn = (RpcConnection*)the_arg;
  RpcPktHdr hdr;
  DWORD dwRead;
  void* buf = NULL;
  RpcPacket* packet;

  TRACE("(%p)\n", conn);

  for (;;) {
    /* read packet header */
#ifdef OVERLAPPED_WORKS
    if (!ReadFile(conn->conn, &hdr, sizeof(hdr), &dwRead, &conn->ovl_r)) {
      DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        TRACE("connection lost, error=%08lx\n", err);
        break;
      }
      if (!GetOverlappedResult(conn->conn, &conn->ovl_r, &dwRead, TRUE)) break;
    }
#else
    if (!ReadFile(conn->conn, &hdr, sizeof(hdr), &dwRead, NULL)) {
      TRACE("connection lost, error=%08lx\n", GetLastError());
      break;
    }
#endif
    if (dwRead != sizeof(hdr)) {
      if (dwRead) TRACE("protocol error: <hdrsz == %d, dwRead == %lu>\n", sizeof(hdr), dwRead);
      break;
    }

    /* read packet body */
    buf = HeapAlloc(GetProcessHeap(), 0, hdr.len);
    TRACE("receiving payload=%d\n", hdr.len);
    if (!hdr.len) dwRead = 0; else
#ifdef OVERLAPPED_WORKS
    if (!ReadFile(conn->conn, buf, hdr.len, &dwRead, &conn->ovl_r)) {
      DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        TRACE("connection lost, error=%08lx\n", err);
        break;
      }
      if (!GetOverlappedResult(conn->conn, &conn->ovl_r, &dwRead, TRUE)) break;
    }
#else
    if (!ReadFile(conn->conn, buf, hdr.len, &dwRead, NULL)) {
      TRACE("connection lost, error=%08lx\n", GetLastError());
      break;
    }
#endif
    if (dwRead != hdr.len) {
      TRACE("protocol error: <bodylen == %d, dwRead == %lu>\n", hdr.len, dwRead);
      break;
    }

#if 1
    RPCRT4_process_packet(conn, &hdr, buf);
#else
    packet = HeapAlloc(GetProcessHeap(), 0, sizeof(RpcPacket));
    packet->conn = conn;
    packet->hdr = hdr;
    packet->buf = buf;
    RPCRT4_create_worker_if_needed();
    RPCRT4_push_packet(packet);
    ReleaseSemaphore(server_sem, 1, NULL);
#endif
    buf = NULL;
  }
  if (buf) HeapFree(GetProcessHeap(), 0, buf);
  RPCRT4_DestroyConnection(conn);
  return 0;
}

static void RPCRT4_new_client(RpcConnection* conn)
{
  HANDLE thread = CreateThread(NULL, 0, RPCRT4_io_thread, conn, 0, NULL);
  if (!thread) {
    DWORD err = GetLastError();
    ERR("failed to create thread, error=%08lx\n", err);
    RPCRT4_DestroyConnection(conn);
  }
  /* we could set conn->thread, but then we'd have to make the io_thread wait
   * for that, otherwise the thread might finish, destroy the connection, and
   * free the memory we'd write to before we did, causing crashes and stuff -
   * so let's implement that later, when we really need conn->thread */

  CloseHandle( thread );
}

static DWORD CALLBACK RPCRT4_server_thread(LPVOID the_arg)
{
  HANDLE m_event = mgr_event, b_handle;
  HANDLE *objs = NULL;
  DWORD count, res;
  RpcServerProtseq* cps;
  RpcConnection* conn;
  RpcConnection* cconn;

  for (;;) {
    EnterCriticalSection(&server_cs);
    /* open and count connections */
    count = 1;
    cps = protseqs;
    while (cps) {
      conn = cps->conn;
      while (conn) {
        RPCRT4_OpenConnection(conn);
        if (conn->ovl_r.hEvent) count++;
        conn = conn->Next;
      }
      cps = cps->Next;
    }
    /* make array of connections */
    objs = HeapReAlloc(GetProcessHeap(), 0, objs, count*sizeof(HANDLE));
    objs[0] = m_event;
    count = 1;
    cps = protseqs;
    while (cps) {
      conn = cps->conn;
      while (conn) {
        if (conn->ovl_r.hEvent) objs[count++] = conn->ovl_r.hEvent;
        conn = conn->Next;
      }
      cps = cps->Next;
    }
    SetEvent(server_event);
    LeaveCriticalSection(&server_cs);

    /* start waiting */
    res = WaitForMultipleObjects(count, objs, FALSE, INFINITE);
    if (res == WAIT_OBJECT_0) {
      ResetEvent(m_event);
      if (!std_listen) break;
    }
    else if (res == WAIT_FAILED) {
      ERR("wait failed\n");
    }
    else {
      b_handle = objs[res - WAIT_OBJECT_0];
      /* find which connection got a RPC */
      EnterCriticalSection(&server_cs);
      conn = NULL;
      cps = protseqs;
      while (cps) {
        conn = cps->conn;
        while (conn) {
          if (conn->ovl_r.hEvent == b_handle) break;
          conn = conn->Next;
        }
        if (conn) break;
        cps = cps->Next;
      }
      cconn = NULL;
      if (conn) RPCRT4_SpawnConnection(&cconn, conn);
      LeaveCriticalSection(&server_cs);
      if (!conn) {
        ERR("failed to locate connection for handle %d\n", b_handle);
      }
      if (cconn) RPCRT4_new_client(cconn);
    }
  }
  HeapFree(GetProcessHeap(), 0, objs);
  EnterCriticalSection(&server_cs);
  ResetEvent(server_event);
  /* close connections */
  cps = protseqs;
  while (cps) {
    conn = cps->conn;
    while (conn) {
      RPCRT4_CloseConnection(conn);
      conn = conn->Next;
    }
    cps = cps->Next;
  }
  LeaveCriticalSection(&server_cs);
  return 0;
}

static void RPCRT4_start_listen(void)
{
  TRACE("\n");

  EnterCriticalSection(&listen_cs);
  if (! ++listen_count) {
    if (!mgr_event) mgr_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!server_sem) server_sem = CreateSemaphoreA(NULL, 0, MAX_THREADS, NULL);
    if (!server_event) server_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!worker_tls) worker_tls = TlsAlloc();
    std_listen = TRUE;
    server_thread = CreateThread(NULL, 0, RPCRT4_server_thread, NULL, 0, NULL);
    LeaveCriticalSection(&listen_cs);
  } else {
    LeaveCriticalSection(&listen_cs);
    SetEvent(mgr_event);
  }
}

static void RPCRT4_stop_listen(void)
{
  EnterCriticalSection(&listen_cs);
  if (listen_count == -1)
    LeaveCriticalSection(&listen_cs);
  else if (--listen_count == -1) {
    std_listen = FALSE;
    LeaveCriticalSection(&listen_cs);
    SetEvent(mgr_event);
  } else
    LeaveCriticalSection(&listen_cs);
  assert(listen_count > -2);
}

static RPC_STATUS RPCRT4_use_protseq(RpcServerProtseq* ps)
{
  RPCRT4_CreateConnection(&ps->conn, TRUE, ps->Protseq, NULL, ps->Endpoint, NULL, NULL);

  EnterCriticalSection(&server_cs);
  ps->Next = protseqs;
  protseqs = ps;
  LeaveCriticalSection(&server_cs);

  if (std_listen) SetEvent(mgr_event);

  return RPC_S_OK;
}

void RPCRT4_ReadyServer(void)
{
  EnterCriticalSection(&listen_cs);
  if (std_listen)
    WaitForSingleObject(server_event, INFINITE);
  LeaveCriticalSection(&listen_cs);
}

/***********************************************************************
 *             RpcServerInqBindings (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerInqBindings( RPC_BINDING_VECTOR** BindingVector )
{
  RPC_STATUS status;
  DWORD count;
  RpcServerProtseq* ps;
  RpcConnection* conn;

  if (BindingVector)
    TRACE("(*BindingVector == ^%p)\n", *BindingVector);
  else
    ERR("(BindingVector == ^null!!?)\n");

  EnterCriticalSection(&server_cs);
  /* count connections */
  count = 0;
  ps = protseqs;
  while (ps) {
    conn = ps->conn;
    while (conn) {
      count++;
      conn = conn->Next;
    }
    ps = ps->Next;
  }
  if (count) {
    /* export bindings */
    *BindingVector = HeapAlloc(GetProcessHeap(), 0,
                              sizeof(RPC_BINDING_VECTOR) +
                              sizeof(RPC_BINDING_HANDLE)*(count-1));
    (*BindingVector)->Count = count;
    count = 0;
    ps = protseqs;
    while (ps) {
      conn = ps->conn;
      while (conn) {
       RPCRT4_MakeBinding((RpcBinding**)&(*BindingVector)->BindingH[count],
                          conn);
       count++;
       conn = conn->Next;
      }
      ps = ps->Next;
    }
    status = RPC_S_OK;
  } else {
    *BindingVector = NULL;
    status = RPC_S_NO_BINDINGS;
  }
  LeaveCriticalSection(&server_cs);
  return status;
}

/***********************************************************************
 *             RpcServerUseProtseqEpA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUseProtseqEpA( LPSTR Protseq, UINT MaxCalls, LPSTR Endpoint, LPVOID SecurityDescriptor )
{
  RPC_POLICY policy;

  TRACE( "(%s,%u,%s,%p)\n", Protseq, MaxCalls, Endpoint, SecurityDescriptor );

  /* This should provide the default behaviour */
  policy.Length        = sizeof( policy );
  policy.EndpointFlags = 0;
  policy.NICFlags      = 0;

  return RpcServerUseProtseqEpExA( Protseq, MaxCalls, Endpoint, SecurityDescriptor, &policy );
}

/***********************************************************************
 *             RpcServerUseProtseqEpW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUseProtseqEpW( LPWSTR Protseq, UINT MaxCalls, LPWSTR Endpoint, LPVOID SecurityDescriptor )
{
  RPC_POLICY policy;

  TRACE( "(%s,%u,%s,%p)\n", debugstr_w( Protseq ), MaxCalls, debugstr_w( Endpoint ), SecurityDescriptor );

  /* This should provide the default behaviour */
  policy.Length        = sizeof( policy );
  policy.EndpointFlags = 0;
  policy.NICFlags      = 0;

  return RpcServerUseProtseqEpExW( Protseq, MaxCalls, Endpoint, SecurityDescriptor, &policy );
}

/***********************************************************************
 *             RpcServerUseProtseqEpExA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUseProtseqEpExA( LPSTR Protseq, UINT MaxCalls, LPSTR Endpoint, LPVOID SecurityDescriptor,
                                            PRPC_POLICY lpPolicy )
{
  RpcServerProtseq* ps;

  TRACE("(%s,%u,%s,%p,{%u,%lu,%lu})\n", debugstr_a( Protseq ), MaxCalls,
       debugstr_a( Endpoint ), SecurityDescriptor,
       lpPolicy->Length, lpPolicy->EndpointFlags, lpPolicy->NICFlags );

  ps = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RpcServerProtseq));
  ps->MaxCalls = MaxCalls;
  ps->Protseq = RPCRT4_strdupA(Protseq);
  ps->Endpoint = RPCRT4_strdupA(Endpoint);

  return RPCRT4_use_protseq(ps);
}

/***********************************************************************
 *             RpcServerUseProtseqEpExW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUseProtseqEpExW( LPWSTR Protseq, UINT MaxCalls, LPWSTR Endpoint, LPVOID SecurityDescriptor,
                                            PRPC_POLICY lpPolicy )
{
  RpcServerProtseq* ps;

  TRACE("(%s,%u,%s,%p,{%u,%lu,%lu})\n", debugstr_w( Protseq ), MaxCalls,
       debugstr_w( Endpoint ), SecurityDescriptor,
       lpPolicy->Length, lpPolicy->EndpointFlags, lpPolicy->NICFlags );

  ps = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RpcServerProtseq));
  ps->MaxCalls = MaxCalls;
  /* FIXME: Did Ove have these next two as RPCRT4_strdupW for a reason? */
  ps->Protseq = RPCRT4_strdupWtoA(Protseq);
  ps->Endpoint = RPCRT4_strdupWtoA(Endpoint);

  return RPCRT4_use_protseq(ps);
}

/***********************************************************************
 *             RpcServerUseProtseqA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUseProtseqA(LPSTR Protseq, unsigned int MaxCalls, void *SecurityDescriptor)
{
  TRACE("(Protseq == %s, MaxCalls == %d, SecurityDescriptor == ^%p)\n", debugstr_a(Protseq), MaxCalls, SecurityDescriptor);
  return RpcServerUseProtseqEpA(Protseq, MaxCalls, NULL, SecurityDescriptor);
}

/***********************************************************************
 *             RpcServerUseProtseqW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUseProtseqW(LPWSTR Protseq, unsigned int MaxCalls, void *SecurityDescriptor)
{
  TRACE("Protseq == %s, MaxCalls == %d, SecurityDescriptor == ^%p)\n", debugstr_w(Protseq), MaxCalls, SecurityDescriptor);
  return RpcServerUseProtseqEpW(Protseq, MaxCalls, NULL, SecurityDescriptor);
}

/***********************************************************************
 *             RpcServerRegisterIf (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerRegisterIf( RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv )
{
  TRACE("(%p,%s,%p)\n", IfSpec, debugstr_guid(MgrTypeUuid), MgrEpv);
  return RpcServerRegisterIf2( IfSpec, MgrTypeUuid, MgrEpv, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, (UINT)-1, NULL );
}

/***********************************************************************
 *             RpcServerRegisterIfEx (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerRegisterIfEx( RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
                       UINT Flags, UINT MaxCalls, RPC_IF_CALLBACK_FN* IfCallbackFn )
{
  TRACE("(%p,%s,%p,%u,%u,%p)\n", IfSpec, debugstr_guid(MgrTypeUuid), MgrEpv, Flags, MaxCalls, IfCallbackFn);
  return RpcServerRegisterIf2( IfSpec, MgrTypeUuid, MgrEpv, Flags, MaxCalls, (UINT)-1, IfCallbackFn );
}

/***********************************************************************
 *             RpcServerRegisterIf2 (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerRegisterIf2( RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, RPC_MGR_EPV* MgrEpv,
                      UINT Flags, UINT MaxCalls, UINT MaxRpcSize, RPC_IF_CALLBACK_FN* IfCallbackFn )
{
  PRPC_SERVER_INTERFACE If = (PRPC_SERVER_INTERFACE)IfSpec;
  RpcServerInterface* sif;
  int i;

  TRACE("(%p,%s,%p,%u,%u,%u,%p)\n", IfSpec, debugstr_guid(MgrTypeUuid), MgrEpv, Flags, MaxCalls,
         MaxRpcSize, IfCallbackFn);
  TRACE(" interface id: %s %d.%d\n", debugstr_guid(&If->InterfaceId.SyntaxGUID),
                                     If->InterfaceId.SyntaxVersion.MajorVersion,
                                     If->InterfaceId.SyntaxVersion.MinorVersion);
  TRACE(" transfer syntax: %s %d.%d\n", debugstr_guid(&If->TransferSyntax.SyntaxGUID),
                                        If->TransferSyntax.SyntaxVersion.MajorVersion,
                                        If->TransferSyntax.SyntaxVersion.MinorVersion);
  TRACE(" dispatch table: %p\n", If->DispatchTable);
  if (If->DispatchTable) {
    TRACE("  dispatch table count: %d\n", If->DispatchTable->DispatchTableCount);
    for (i=0; i<If->DispatchTable->DispatchTableCount; i++) {
      TRACE("   entry %d: %p\n", i, If->DispatchTable->DispatchTable[i]);
    }
    TRACE("  reserved: %d\n", If->DispatchTable->Reserved);
  }
  TRACE(" protseq endpoint count: %d\n", If->RpcProtseqEndpointCount);
  TRACE(" default manager epv: %p\n", If->DefaultManagerEpv);
  TRACE(" interpreter info: %p\n", If->InterpreterInfo);
  TRACE(" flags: %08x\n", If->Flags);

  sif = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RpcServerInterface));
  sif->If           = If;
  if (MgrTypeUuid)
    memcpy(&sif->MgrTypeUuid, MgrTypeUuid, sizeof(UUID));
  else
    memset(&sif->MgrTypeUuid, 0, sizeof(UUID));
  sif->MgrEpv       = MgrEpv;
  sif->Flags        = Flags;
  sif->MaxCalls     = MaxCalls;
  sif->MaxRpcSize   = MaxRpcSize;
  sif->IfCallbackFn = IfCallbackFn;

  EnterCriticalSection(&server_cs);
  sif->Next = ifs;
  ifs = sif;
  LeaveCriticalSection(&server_cs);

  if (sif->Flags & RPC_IF_AUTOLISTEN) {
    /* well, start listening, I think... */
    RPCRT4_start_listen();
  }

  return RPC_S_OK;
}

/***********************************************************************
 *             RpcServerUnregisterIf (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUnregisterIf( RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, UINT WaitForCallsToComplete )
{
  FIXME("(IfSpec == (RPC_IF_HANDLE)^%p, MgrTypeUuid == %s, WaitForCallsToComplete == %u): stub\n",
    IfSpec, debugstr_guid(MgrTypeUuid), WaitForCallsToComplete);

  return RPC_S_OK;
}

/***********************************************************************
 *             RpcServerUnregisterIfEx (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerUnregisterIfEx( RPC_IF_HANDLE IfSpec, UUID* MgrTypeUuid, int RundownContextHandles )
{
  FIXME("(IfSpec == (RPC_IF_HANDLE)^%p, MgrTypeUuid == %s, RundownContextHandles == %d): stub\n",
    IfSpec, debugstr_guid(MgrTypeUuid), RundownContextHandles);

  return RPC_S_OK;
}

/***********************************************************************
 *             RpcServerRegisterAuthInfoA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerRegisterAuthInfoA( LPSTR ServerPrincName, ULONG AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn,
                            LPVOID Arg )
{
  FIXME( "(%s,%lu,%p,%p): stub\n", ServerPrincName, AuthnSvc, GetKeyFn, Arg );

  return RPC_S_UNKNOWN_AUTHN_SERVICE; /* We don't know any authentication services */
}

/***********************************************************************
 *             RpcServerRegisterAuthInfoW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerRegisterAuthInfoW( LPWSTR ServerPrincName, ULONG AuthnSvc, RPC_AUTH_KEY_RETRIEVAL_FN GetKeyFn,
                            LPVOID Arg )
{
  FIXME( "(%s,%lu,%p,%p): stub\n", debugstr_w( ServerPrincName ), AuthnSvc, GetKeyFn, Arg );

  return RPC_S_UNKNOWN_AUTHN_SERVICE; /* We don't know any authentication services */
}

/***********************************************************************
 *             RpcServerListen (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcServerListen( UINT MinimumCallThreads, UINT MaxCalls, UINT DontWait )
{
  TRACE("(%u,%u,%u)\n", MinimumCallThreads, MaxCalls, DontWait);

  if (!protseqs)
    return RPC_S_NO_PROTSEQS_REGISTERED;

  EnterCriticalSection(&listen_cs);

#if 0 /* RpcServerRegisterIf2/RPC_IF_AUTOLISTEN incorrectly sets std_listen, nontrivial to fix */
  if (std_listen) {
    LeaveCriticalSection(&listen_cs);
    return RPC_S_ALREADY_LISTENING;
  }
#endif

  RPCRT4_start_listen();

  LeaveCriticalSection(&listen_cs);

  if (DontWait) return RPC_S_OK;

  return RpcMgmtWaitServerListen();
}

/***********************************************************************
 *             RpcMgmtServerWaitListen (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcMgmtWaitServerListen( void )
{
  RPC_STATUS rslt = RPC_S_OK;

  TRACE("\n");

  EnterCriticalSection(&listen_cs);

  if (!std_listen)
    if ( (rslt = RpcServerListen(1, 0, TRUE)) != RPC_S_OK ) {
      LeaveCriticalSection(&listen_cs);
      return rslt;
    }

  LeaveCriticalSection(&listen_cs);

  while (std_listen) {
    WaitForSingleObject(mgr_event, INFINITE);
    if (!std_listen) {
      Sleep(100); /* don't spin violently */
      TRACE("spinning.\n");
    }
  }

  return rslt;
}

/***********************************************************************
 *             RpcMgmtStopServerListening (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcMgmtStopServerListening ( RPC_BINDING_HANDLE Binding )
{
  TRACE("(Binding == (RPC_BINDING_HANDLE)^%p)\n", Binding);

  if (Binding) {
    FIXME("client-side invocation not implemented.\n");
    return RPC_S_WRONG_KIND_OF_BINDING;
  }

  /* hmm... */
  EnterCriticalSection(&listen_cs);
  while (std_listen)
    RPCRT4_stop_listen();
  LeaveCriticalSection(&listen_cs);

  return RPC_S_OK;
}

/***********************************************************************
 *             I_RpcServerStartListening (RPCRT4.@)
 */
RPC_STATUS WINAPI I_RpcServerStartListening( void* hWnd )
{
  FIXME( "(%p): stub\n", hWnd );

  return RPC_S_OK;
}

/***********************************************************************
 *             I_RpcServerStopListening (RPCRT4.@)
 */
RPC_STATUS WINAPI I_RpcServerStopListening( void )
{
  FIXME( "(): stub\n" );

  return RPC_S_OK;
}

/***********************************************************************
 *             I_RpcWindowProc (RPCRT4.@)
 */
UINT WINAPI I_RpcWindowProc( void *hWnd, UINT Message, UINT wParam, ULONG lParam )
{
  FIXME( "(%p,%08x,%08x,%08lx): stub\n", hWnd, Message, wParam, lParam );

  return 0;
}
