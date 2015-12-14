/*
 * RPC endpoint mapper
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * TODO:
 *  - actually do things right
 */

#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"

#include "wine/server.h"
#include "rpc.h"

#include "wine/debug.h"

#include "rpc_binding.h"
#include "rpc_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/* The "real" RPC portmapper endpoints that I know of are:
 *
 *  ncadg_ip_udp: 135
 *  ncacn_ip_tcp: 135
 *  ncacn_np: \\pipe\epmapper (?)
 *  ncalrpc: epmapper
 *
 * If the user's machine ran a DCE RPC daemon, it would
 * probably be possible to connect to it, but there are many
 * reasons not to, like:
 *  - the user probably does *not* run one, and probably
 *    shouldn't be forced to run one just for local COM
 *  - very few Unix systems use DCE RPC... if they run a RPC
 *    daemon at all, it's usually Sun RPC
 *  - DCE RPC registrations are persistent and saved on disk,
 *    while MS-RPC registrations are documented as non-persistent
 *    and stored only in RAM, and auto-destroyed when the process
 *    dies (something DCE RPC can't do)
 *
 * Of course, if the user *did* want to run a DCE RPC daemon anyway,
 * there would be interoperability advantages, like the possibility
 * of running a fully functional DCOM server using Wine...
 *
 * But for now, I'll just use the wineserver...
 */

/***********************************************************************
 *		RpcEpRegisterA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcEpRegisterA( RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector,
                                  UUID_VECTOR* UuidVector, LPSTR Annotation )
{
  NTSTATUS ret;
  PRPC_SERVER_INTERFACE If = (PRPC_SERVER_INTERFACE)IfSpec;
  unsigned long c;

  TRACE("(%p,%p,%p,%s)\n", IfSpec, BindingVector, UuidVector, debugstr_a(Annotation));
  TRACE(" ifid=%s\n", debugstr_guid(&If->InterfaceId.SyntaxGUID));
  for (c=0; c<BindingVector->Count; c++) {
    RpcBinding* bind = (RpcBinding*)(BindingVector->BindingH[c]);
    TRACE(" protseq[%ld]=%s\n", c, bind->Protseq);
    TRACE(" endpoint[%ld]=%s\n", c, bind->Endpoint);
  }
  if (UuidVector) {
    for (c=0; c<UuidVector->Count; c++)
      TRACE(" obj[%ld]=%s\n", c, debugstr_guid(UuidVector->Uuid[c]));
  }

  RPCRT4_ReadyServer();

  SERVER_START_REQ( register_rpc_endpoints )
  {
    wine_server_add_data( req, &If->InterfaceId, sizeof(RPC_SYNTAX_IDENTIFIER) );
    if (UuidVector) {
      req->objects = UuidVector->Count;
      for (c=0; c<req->objects; c++)
        wine_server_add_data( req, UuidVector->Uuid[c], sizeof(UUID) );
    }
    else req->objects = 0;
    req->bindings = BindingVector->Count;
    for (c=0; c<req->bindings; c++) {
      RpcBinding* bind = (RpcBinding*)(BindingVector->BindingH[c]);
      wine_server_add_data( req, bind->Protseq, strlen(bind->Protseq)+1 );
      wine_server_add_data( req, bind->Endpoint, strlen(bind->Endpoint)+1 );
    }
    req->no_replace = 0;
    /* FIXME: annotation */
    ret = wine_server_call( req );
  }
  SERVER_END_REQ;

  return RtlNtStatusToDosError(ret);
}

/***********************************************************************
 *		RpcEpUnregister (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcEpUnregister( RPC_IF_HANDLE IfSpec, RPC_BINDING_VECTOR* BindingVector,
                                   UUID_VECTOR* UuidVector )
{
  NTSTATUS ret;
  PRPC_SERVER_INTERFACE If = (PRPC_SERVER_INTERFACE)IfSpec;
  unsigned long c;

  TRACE("(%p,%p,%p)\n", IfSpec, BindingVector, UuidVector);
  TRACE(" ifid=%s\n", debugstr_guid(&If->InterfaceId.SyntaxGUID));
  for (c=0; c<BindingVector->Count; c++) {
    RpcBinding* bind = (RpcBinding*)(BindingVector->BindingH[c]);
    TRACE(" protseq[%ld]=%s\n", c, bind->Protseq);
    TRACE(" endpoint[%ld]=%s\n", c, bind->Endpoint);
  }
  if (UuidVector) {
    for (c=0; c<UuidVector->Count; c++)
      TRACE(" obj[%ld]=%s\n", c, debugstr_guid(UuidVector->Uuid[c]));
  }

  SERVER_START_REQ( unregister_rpc_endpoints )
  {
    wine_server_add_data( req, &If->InterfaceId, sizeof(RPC_SYNTAX_IDENTIFIER) );
    if (UuidVector) {
      req->objects = UuidVector->Count;
      for (c=0; c<req->objects; c++)
        wine_server_add_data( req, UuidVector->Uuid[c], sizeof(UUID) );
    }
    else req->objects = 0;
    req->bindings = BindingVector->Count;
    for (c=0; c<req->bindings; c++) {
      RpcBinding* bind = (RpcBinding*)(BindingVector->BindingH[c]);
      wine_server_add_data( req, bind->Protseq, strlen(bind->Protseq)+1 );
      wine_server_add_data( req, bind->Endpoint, strlen(bind->Endpoint)+1 );
    }
    ret = wine_server_call( req );
  }
  SERVER_END_REQ;

  return RtlNtStatusToDosError(ret);
}

/***********************************************************************
 *		RpcEpResolveBinding (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcEpResolveBinding( RPC_BINDING_HANDLE Binding, RPC_IF_HANDLE IfSpec )
{
  NTSTATUS ret;
  PRPC_CLIENT_INTERFACE If = (PRPC_CLIENT_INTERFACE)IfSpec;
  RpcBinding* bind = (RpcBinding*)Binding;
  char Endpoint[64];

  TRACE("(%p,%p)\n", Binding, IfSpec);
  TRACE(" protseq=%s\n", bind->Protseq);
  TRACE(" obj=%s\n", debugstr_guid(&bind->ObjectUuid));
  TRACE(" ifid=%s\n", debugstr_guid(&If->InterfaceId.SyntaxGUID));

  SERVER_START_REQ( resolve_rpc_endpoint )
  {
    wine_server_add_data( req, &If->InterfaceId, sizeof(RPC_SYNTAX_IDENTIFIER) );
    wine_server_add_data( req, &bind->ObjectUuid, sizeof(UUID) );
    wine_server_add_data( req, bind->Protseq, strlen(bind->Protseq)+1 );
    wine_server_set_reply( req, Endpoint, sizeof(Endpoint) );
    ret = wine_server_call( req );
  }
  SERVER_END_REQ;

  if (ret) return RtlNtStatusToDosError(ret);

  return RPCRT4_ResolveBinding(Binding, Endpoint);
}
