/*
 * RPC messages
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * TODO:
 *  - figure out whether we *really* got this right
 *  - check for errors and throw exceptions
 *  - decide if OVERLAPPED_WORKS
 */

#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"

#include "rpc.h"
#include "rpcdcep.h"

#include "wine/debug.h"

#include "rpc_binding.h"
#include "rpc_misc.h"
#include "rpc_defs.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/***********************************************************************
 *           I_RpcGetBuffer [RPCRT4.@]
 */
RPC_STATUS WINAPI I_RpcGetBuffer(PRPC_MESSAGE pMsg)
{
  RpcBinding* bind = (RpcBinding*)pMsg->Handle;
  void* buf;

  TRACE("(%p): BufferLength=%d\n", pMsg, pMsg->BufferLength);
  /* FIXME: pfnAllocate? */
  if (bind->server) {
    /* it turns out that the original buffer data must still be available
     * while the RPC server is marshalling a reply, so we should not deallocate
     * it, we'll leave deallocating the original buffer to the RPC server */
    buf = HeapAlloc(GetProcessHeap(), 0, pMsg->BufferLength);
  } else {
    buf = HeapReAlloc(GetProcessHeap(), 0, pMsg->Buffer, pMsg->BufferLength);
  }
  TRACE("Buffer=%p\n", buf);
  if (buf) pMsg->Buffer = buf;
  /* FIXME: which errors to return? */
  return buf ? S_OK : E_OUTOFMEMORY;
}

/***********************************************************************
 *           I_RpcFreeBuffer [RPCRT4.@]
 */
RPC_STATUS WINAPI I_RpcFreeBuffer(PRPC_MESSAGE pMsg)
{
  TRACE("(%p) Buffer=%p\n", pMsg, pMsg->Buffer);
  /* FIXME: pfnFree? */
  HeapFree(GetProcessHeap(), 0, pMsg->Buffer);
  pMsg->Buffer = NULL;
  return S_OK;
}

/***********************************************************************
 *           I_RpcSend [RPCRT4.@]
 */
RPC_STATUS WINAPI I_RpcSend(PRPC_MESSAGE pMsg)
{
  RpcBinding* bind = (RpcBinding*)pMsg->Handle;
  RpcConnection* conn;
  RPC_CLIENT_INTERFACE* cif = NULL;
  RPC_SERVER_INTERFACE* sif = NULL;
  UUID* obj;
  UUID* act;
  RPC_STATUS status;
  RpcPktHdr hdr;

  TRACE("(%p)\n", pMsg);
  if (!bind) return RPC_S_INVALID_BINDING;

  RPCRT4_ReadyServer();
  status = RPCRT4_OpenBinding(bind, &conn);
  if (status != RPC_S_OK) return status;

  obj = &bind->ObjectUuid;
  act = &bind->ActiveUuid;

  if (bind->server) {
    sif = pMsg->RpcInterfaceInformation;
    if (!sif) return RPC_S_INTERFACE_NOT_FOUND; /* ? */
  } else {
    cif = pMsg->RpcInterfaceInformation;
    if (!cif) return RPC_S_INTERFACE_NOT_FOUND; /* ? */
  }

  /* initialize packet header */
  memset(&hdr, 0, sizeof(hdr));
  hdr.rpc_ver = 4;
  hdr.ptype = bind->server
              ? ((pMsg->RpcFlags & WINE_RPCFLAG_EXCEPTION) ? PKT_FAULT : PKT_RESPONSE)
              : PKT_REQUEST;
  hdr.object = *obj; /* FIXME: IIRC iff no object, the header structure excludes this elt */
  hdr.if_id = (bind->server) ? sif->InterfaceId.SyntaxGUID : cif->InterfaceId.SyntaxGUID;
  hdr.if_vers =
    (bind->server) ?
    MAKELONG(sif->InterfaceId.SyntaxVersion.MinorVersion, sif->InterfaceId.SyntaxVersion.MajorVersion) :
    MAKELONG(cif->InterfaceId.SyntaxVersion.MinorVersion, cif->InterfaceId.SyntaxVersion.MajorVersion);
  hdr.act_id = *act;
  hdr.opnum = pMsg->ProcNum;
  /* only the low-order 3 octets of the DataRepresentation go in the header */
  hdr.drep[0] = LOBYTE(LOWORD(pMsg->DataRepresentation));
  hdr.drep[1] = HIBYTE(LOWORD(pMsg->DataRepresentation));
  hdr.drep[2] = LOBYTE(HIWORD(pMsg->DataRepresentation));
  hdr.len = pMsg->BufferLength;

  /* transmit packet */
#ifdef OVERLAPPED_WORKS
  if (!WriteFile(conn->conn, &hdr, sizeof(hdr), NULL, &conn->ovl_w)) {
    DWORD err = GetLastError();
    if (err != ERROR_IO_PENDING) {
      status = err;
      goto fail;
    }
    if (!GetOverlappedResult(conn->conn, &conn->ovl_w, NULL, TRUE)) {
      status = GetLastError();
      goto fail;
    }
  }
#else
  if (!WriteFile(conn->conn, &hdr, sizeof(hdr), NULL, NULL)) {
    status = GetLastError();
    goto fail;
  }
#endif
  if (pMsg->BufferLength) {
#ifdef OVERLAPPED_WORKS
    if (!WriteFile(conn->conn, pMsg->Buffer, pMsg->BufferLength, NULL, &conn->ovl_w)) {
      DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        status = err;
        goto fail;
      }
      if (!GetOverlappedResult(conn->conn, &conn->ovl_w, NULL, TRUE)) {
        status = GetLastError();
        goto fail;
      }
    }
#else
    if (!WriteFile(conn->conn, pMsg->Buffer, pMsg->BufferLength, NULL, NULL)) {
      status = GetLastError();
      goto fail;
    }
#endif
  }

  /* success */
  if (!bind->server) {
    /* save the connection, so the response can be read from it */
    pMsg->ReservedForRuntime = conn;
    return RPC_S_OK;
  }
  RPCRT4_CloseBinding(bind, conn);
  status = RPC_S_OK;
fail:

  return status;
}

/***********************************************************************
 *           I_RpcReceive [RPCRT4.@]
 */
RPC_STATUS WINAPI I_RpcReceive(PRPC_MESSAGE pMsg)
{
  RpcBinding* bind = (RpcBinding*)pMsg->Handle;
  RpcConnection* conn;
  UUID* act;
  RPC_STATUS status;
  RpcPktHdr hdr;
  DWORD dwRead;

  TRACE("(%p)\n", pMsg);
  if (!bind) return RPC_S_INVALID_BINDING;

  if (pMsg->ReservedForRuntime) {
    conn = pMsg->ReservedForRuntime;
    pMsg->ReservedForRuntime = NULL;
  } else {
    status = RPCRT4_OpenBinding(bind, &conn);
    if (status != RPC_S_OK) return status;
  }

  act = &bind->ActiveUuid;

  for (;;) {
    /* read packet header */
#ifdef OVERLAPPED_WORKS
    if (!ReadFile(conn->conn, &hdr, sizeof(hdr), &dwRead, &conn->ovl_r)) {
      DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        status = err;
        goto fail;
      }
      if (!GetOverlappedResult(conn->conn, &conn->ovl_r, &dwRead, TRUE)) {
        status = GetLastError();
        goto fail;
      }
    }
#else
    if (!ReadFile(conn->conn, &hdr, sizeof(hdr), &dwRead, NULL)) {
      status = GetLastError();
      goto fail;
    }
#endif
    if (dwRead != sizeof(hdr)) {
      status = RPC_S_PROTOCOL_ERROR;
      goto fail;
    }

    /* read packet body */
    pMsg->BufferLength = hdr.len;
    status = I_RpcGetBuffer(pMsg);
    if (status != RPC_S_OK) goto fail;
    if (!pMsg->BufferLength) dwRead = 0; else
#ifdef OVERLAPPED_WORKS
    if (!ReadFile(conn->conn, pMsg->Buffer, hdr.len, &dwRead, &conn->ovl_r)) {
      DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        status = err;
        goto fail;
      }
      if (!GetOverlappedResult(conn->conn, &conn->ovl_r, &dwRead, TRUE)) {
        status = GetLastError();
        goto fail;
      }
    }
#else
    if (!ReadFile(conn->conn, pMsg->Buffer, hdr.len, &dwRead, NULL)) {
      status = GetLastError();
      goto fail;
    }
#endif
    if (dwRead != hdr.len) {
      status = RPC_S_PROTOCOL_ERROR;
      goto fail;
    }

    status = RPC_S_PROTOCOL_ERROR;

    switch (hdr.ptype) {
    case PKT_RESPONSE:
      if (bind->server) goto fail;
      break;
    case PKT_REQUEST:
      if (!bind->server) goto fail;
      break;
    case PKT_FAULT:
      pMsg->RpcFlags |= WINE_RPCFLAG_EXCEPTION;
      status = RPC_S_CALL_FAILED; /* ? */
      goto fail;
    default:
      goto fail;
    }

    /* success */
    status = RPC_S_OK;

    /* FIXME: check destination, etc? */
    break;
  }
fail:
  RPCRT4_CloseBinding(bind, conn);
  return status;
}

/***********************************************************************
 *           I_RpcSendReceive [RPCRT4.@]
 */
RPC_STATUS WINAPI I_RpcSendReceive(PRPC_MESSAGE pMsg)
{
  RPC_STATUS status;

  TRACE("(%p)\n", pMsg);
  status = I_RpcSend(pMsg);
  if (status == RPC_S_OK)
    status = I_RpcReceive(pMsg);
  return status;
}
