/*
 * RPC server API
 *
 * Copyright 2001 Ove Kåven, TransGaming Technologies
 */

#ifndef __WINE_RPC_SERVER_H
#define __WINE_RPC_SERVER_H

#include "rpc_binding.h"

typedef struct _RpcServerProtseq
{
  struct _RpcServerProtseq* Next;
  LPSTR Protseq;
  LPSTR Endpoint;
  UINT MaxCalls;
  RpcConnection* conn;
} RpcServerProtseq;

typedef struct _RpcServerInterface
{
  struct _RpcServerInterface* Next;
  RPC_SERVER_INTERFACE* If;
  UUID MgrTypeUuid;
  RPC_MGR_EPV* MgrEpv;
  UINT Flags;
  UINT MaxCalls;
  UINT MaxRpcSize;
  RPC_IF_CALLBACK_FN* IfCallbackFn;
} RpcServerInterface;

#endif  /* __WINE_RPC_SERVER_H */
