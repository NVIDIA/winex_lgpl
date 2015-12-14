/*
 * RPC binding API
 *
 * Copyright 2001 Ove Kåven, TransGaming Technologies
 */

#ifndef __WINE_RPC_BINDING_H
#define __WINE_RPC_BINDING_H

#include "wine/rpcss_shared.h"

typedef struct _RpcConnection
{
  struct _RpcConnection* Next;
  struct _RpcBinding* Used;
  BOOL server;
  LPSTR Protseq;
  LPSTR NetworkAddr;
  LPSTR Endpoint;
  HANDLE conn, thread;
  OVERLAPPED ovl_r, ovl_w;
} RpcConnection;

/* don't know what MS's structure looks like */
typedef struct _RpcBinding
{
  DWORD refs;
  CRITICAL_SECTION cs;
  struct _RpcBinding* Next;
  BOOL server;
  UUID ObjectUuid;
  UUID ActiveUuid;
  LPSTR Protseq;
  LPSTR NetworkAddr;
  LPSTR Endpoint;
  RPC_BLOCKING_FN BlockingFn;
  ULONG ServerTid;
  RpcConnection* FromConn;
} RpcBinding;

LPSTR RPCRT4_strndupA(LPSTR src, INT len);
LPWSTR RPCRT4_strndupW(LPWSTR src, INT len);
LPSTR RPCRT4_strdupWtoA(LPWSTR src);
LPWSTR RPCRT4_strdupAtoW(LPSTR src);
void RPCRT4_strfree(LPSTR src);

#define RPCRT4_strdupA(x) RPCRT4_strndupA((x),-1)
#define RPCRT4_strdupW(x) RPCRT4_strndupW((x),-1)

RPC_STATUS RPCRT4_CreateConnection(RpcConnection** Connection, BOOL server, LPSTR Protseq, LPSTR NetworkAddr, LPSTR Endpoint, LPSTR NetworkOptions, RpcBinding* Binding);
RPC_STATUS RPCRT4_DestroyConnection(RpcConnection* Connection);
RPC_STATUS RPCRT4_OpenConnection(RpcConnection* Connection);
RPC_STATUS RPCRT4_CloseConnection(RpcConnection* Connection);
RPC_STATUS RPCRT4_SpawnConnection(RpcConnection** Connection, RpcConnection* OldConnection);

RPC_STATUS RPCRT4_CreateBindingA(RpcBinding** Binding, BOOL server, LPSTR Protseq);
RPC_STATUS RPCRT4_CreateBindingW(RpcBinding** Binding, BOOL server, LPWSTR Protseq);
RPC_STATUS RPCRT4_CompleteBindingA(RpcBinding* Binding, LPSTR NetworkAddr,  LPSTR Endpoint,  LPSTR NetworkOptions);
RPC_STATUS RPCRT4_CompleteBindingW(RpcBinding* Binding, LPWSTR NetworkAddr, LPWSTR Endpoint, LPWSTR NetworkOptions);
RPC_STATUS RPCRT4_ResolveBinding(RpcBinding* Binding, LPSTR Endpoint);
RPC_STATUS RPCRT4_SetBindingObject(RpcBinding* Binding, UUID* ObjectUuid);
RPC_STATUS RPCRT4_MakeBinding(RpcBinding** Binding, RpcConnection* Connection);
RPC_STATUS RPCRT4_ExportBinding(RpcBinding** Binding, RpcBinding* OldBinding);
RPC_STATUS RPCRT4_DestroyBinding(RpcBinding* Binding);
RPC_STATUS RPCRT4_OpenBinding(RpcBinding* Binding, RpcConnection** Connection);
RPC_STATUS RPCRT4_CloseBinding(RpcBinding* Binding, RpcConnection* Connection);
BOOL RPCRT4_RPCSSOnDemandCall(PRPCSS_NP_MESSAGE msg, char *vardata_payload, PRPCSS_NP_REPLY reply);
HANDLE RPCRT4_GetMasterMutex(void);
HANDLE RPCRT4_RpcssNPConnect(void);

#endif
