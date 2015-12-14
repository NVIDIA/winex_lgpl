/*
 * MIDL proxy/stub stuff
 *
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * TODO:
 *  - figure out whether we *really* got this right
 *  - check for errors and throw exceptions
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"

#include "objbase.h"

#include "rpcproxy.h"

#include "wine/debug.h"

#include "cpsf.h"
#include "ndr_misc.h"
#include "rpcndr.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/***********************************************************************
 *           NdrProxyInitialize [RPCRT4.@]
 */
void WINAPI NdrProxyInitialize(void *This,
                              PRPC_MESSAGE pRpcMsg,
                              PMIDL_STUB_MESSAGE pStubMsg,
                              PMIDL_STUB_DESC pStubDescriptor,
                              unsigned int ProcNum)
{
  HRESULT hr;

  TRACE("(%p,%p,%p,%p,%d)\n", This, pRpcMsg, pStubMsg, pStubDescriptor, ProcNum);
  NdrClientInitializeNew(pRpcMsg, pStubMsg, pStubDescriptor, ProcNum);
  if (This) StdProxy_GetChannel(This, &pStubMsg->pRpcChannelBuffer);
  if (pStubMsg->pRpcChannelBuffer) {
    hr = IRpcChannelBuffer_GetDestCtx(pStubMsg->pRpcChannelBuffer,
                                     &pStubMsg->dwDestContext,
                                     &pStubMsg->pvDestContext);
  }
  TRACE("channel=%p\n", pStubMsg->pRpcChannelBuffer);
}

/***********************************************************************
 *           NdrProxyGetBuffer [RPCRT4.@]
 */
void WINAPI NdrProxyGetBuffer(void *This,
                             PMIDL_STUB_MESSAGE pStubMsg)
{
  HRESULT hr;
  const IID *riid = NULL;

  TRACE("(%p,%p)\n", This, pStubMsg);
  pStubMsg->RpcMsg->BufferLength = pStubMsg->BufferLength;
  pStubMsg->dwStubPhase = PROXY_GETBUFFER;
  hr = StdProxy_GetIID(This, &riid);
  hr = IRpcChannelBuffer_GetBuffer(pStubMsg->pRpcChannelBuffer,
                                  (RPCOLEMESSAGE*)pStubMsg->RpcMsg,
                                  riid);
  pStubMsg->BufferStart = pStubMsg->RpcMsg->Buffer;
  pStubMsg->BufferEnd = pStubMsg->BufferStart + pStubMsg->BufferLength;
  pStubMsg->Buffer = pStubMsg->BufferStart;
  pStubMsg->dwStubPhase = PROXY_MARSHAL;
}

/***********************************************************************
 *           NdrProxySendReceive [RPCRT4.@]
 */
void WINAPI NdrProxySendReceive(void *This,
                               PMIDL_STUB_MESSAGE pStubMsg)
{
  ULONG Status = 0;
  HRESULT hr;

  TRACE("(%p,%p)\n", This, pStubMsg);
  pStubMsg->dwStubPhase = PROXY_SENDRECEIVE;
  hr = IRpcChannelBuffer_SendReceive(pStubMsg->pRpcChannelBuffer,
                                    (RPCOLEMESSAGE*)pStubMsg->RpcMsg,
                                    &Status);
  pStubMsg->dwStubPhase = PROXY_UNMARSHAL;
  pStubMsg->BufferLength = pStubMsg->RpcMsg->BufferLength;
  pStubMsg->BufferStart = pStubMsg->RpcMsg->Buffer;
  pStubMsg->BufferEnd = pStubMsg->BufferStart + pStubMsg->BufferLength;
  pStubMsg->Buffer = pStubMsg->BufferStart;

  /* raise exception if call failed */
  if (hr == RPC_S_CALL_FAILED) RpcRaiseException(*(DWORD*)pStubMsg->Buffer);
  else if (FAILED(hr)) RpcRaiseException(hr);
}

/***********************************************************************
 *           NdrProxyFreeBuffer [RPCRT4.@]
 */
void WINAPI NdrProxyFreeBuffer(void *This,
                              PMIDL_STUB_MESSAGE pStubMsg)
{
  HRESULT hr;

  TRACE("(%p,%p)\n", This, pStubMsg);
  hr = IRpcChannelBuffer_FreeBuffer(pStubMsg->pRpcChannelBuffer,
                                   (RPCOLEMESSAGE*)pStubMsg->RpcMsg);
}

/***********************************************************************
 *           NdrProxyErrorHandler [RPCRT4.@]
 */
HRESULT WINAPI NdrProxyErrorHandler(DWORD dwExceptionCode)
{
  FIXME("(0x%08lx): semi-stub\n", dwExceptionCode);
  return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, RPC_S_CALL_FAILED);
}

/***********************************************************************
 *           NdrStubInitialize [RPCRT4.@]
 */
void WINAPI NdrStubInitialize(PRPC_MESSAGE pRpcMsg,
                             PMIDL_STUB_MESSAGE pStubMsg,
                             PMIDL_STUB_DESC pStubDescriptor,
                             LPRPCCHANNELBUFFER pRpcChannelBuffer)
{
  TRACE("(%p,%p,%p,%p)\n", pRpcMsg, pStubMsg, pStubDescriptor, pRpcChannelBuffer);
  NdrServerInitializeNew(pRpcMsg, pStubMsg, pStubDescriptor);
  pStubMsg->pRpcChannelBuffer = pRpcChannelBuffer;
}

/***********************************************************************
 *           NdrStubGetBuffer [RPCRT4.@]
 */
void WINAPI NdrStubGetBuffer(LPRPCSTUBBUFFER This,
                            LPRPCCHANNELBUFFER pRpcChannelBuffer,
                            PMIDL_STUB_MESSAGE pStubMsg)
{
  TRACE("(%p,%p)\n", This, pStubMsg);
  pStubMsg->pRpcChannelBuffer = pRpcChannelBuffer;
  pStubMsg->RpcMsg->BufferLength = pStubMsg->BufferLength;
  I_RpcGetBuffer(pStubMsg->RpcMsg); /* ? */
  pStubMsg->BufferStart = pStubMsg->RpcMsg->Buffer;
  pStubMsg->BufferEnd = pStubMsg->BufferStart + pStubMsg->BufferLength;
  pStubMsg->Buffer = pStubMsg->BufferStart;
}

/************************************************************************
 *             NdrClientInitializeNew [RPCRT4.@]
 */
void WINAPI NdrClientInitializeNew( PRPC_MESSAGE pRpcMessage, PMIDL_STUB_MESSAGE pStubMsg,
                                    PMIDL_STUB_DESC pStubDesc, unsigned int ProcNum )
{
  TRACE("(pRpcMessage == ^%p, pStubMsg == ^%p, pStubDesc == ^%p, ProcNum == %d)\n",
    pRpcMessage, pStubMsg, pStubDesc, ProcNum);

  assert( pRpcMessage && pStubMsg && pStubDesc );

  memset(pRpcMessage, 0, sizeof(RPC_MESSAGE));

  /* not everyone allocates stack space for w2kReserved */
  memset(pStubMsg, 0, sizeof(*pStubMsg) - sizeof(pStubMsg->w2kReserved));

  pStubMsg->ReuseBuffer = FALSE;
  pStubMsg->IsClient = TRUE;
  pStubMsg->StubDesc = pStubDesc;
  pStubMsg->pfnAllocate = pStubDesc->pfnAllocate;
  pStubMsg->pfnFree = pStubDesc->pfnFree;
  pStubMsg->RpcMsg = pRpcMessage;

  pRpcMessage->ProcNum = ProcNum;
  pRpcMessage->RpcInterfaceInformation = pStubDesc->RpcInterfaceInformation;
}

/***********************************************************************
 *             NdrServerInitializeNew [RPCRT4.@]
 */
unsigned char* WINAPI NdrServerInitializeNew( PRPC_MESSAGE pRpcMsg, PMIDL_STUB_MESSAGE pStubMsg,
                                              PMIDL_STUB_DESC pStubDesc )
{
  TRACE("(pRpcMsg == ^%p, pStubMsg == ^%p, pStubDesc == ^%p)\n", pRpcMsg, pStubMsg, pStubDesc);

  assert( pRpcMsg && pStubMsg && pStubDesc );

  /* not everyone allocates stack space for w2kReserved */
  memset(pStubMsg, 0, sizeof(*pStubMsg) - sizeof(pStubMsg->w2kReserved));

  pStubMsg->ReuseBuffer = TRUE;
  pStubMsg->IsClient = FALSE;
  pStubMsg->StubDesc = pStubDesc;
  pStubMsg->pfnAllocate = pStubDesc->pfnAllocate;
  pStubMsg->pfnFree = pStubDesc->pfnFree;
  pStubMsg->RpcMsg = pRpcMsg;
  pStubMsg->Buffer = pStubMsg->BufferStart = pRpcMsg->Buffer;
  pStubMsg->BufferLength = pRpcMsg->BufferLength;
  pStubMsg->BufferEnd = pStubMsg->Buffer + pStubMsg->BufferLength;

  /* FIXME: determine the proper return value */
  return NULL;
}

/***********************************************************************
 *           NdrGetBuffer [RPCRT4.@]
 */
unsigned char *WINAPI NdrGetBuffer(MIDL_STUB_MESSAGE *stubmsg, unsigned long buflen, RPC_BINDING_HANDLE handle)
{
  TRACE("(stubmsg == ^%p, buflen == %lu, handle == %p): wild guess.\n", stubmsg, buflen, handle);

  assert( stubmsg && stubmsg->RpcMsg );

  /* I guess this is our chance to put the binding handle into the RPC_MESSAGE */
  stubmsg->RpcMsg->Handle = handle;

  stubmsg->RpcMsg->BufferLength = buflen;
  if (I_RpcGetBuffer(stubmsg->RpcMsg) != S_OK)
    return NULL;

  stubmsg->Buffer = stubmsg->BufferStart = stubmsg->RpcMsg->Buffer;
  stubmsg->BufferLength = stubmsg->RpcMsg->BufferLength;
  stubmsg->BufferEnd = stubmsg->Buffer + stubmsg->BufferLength;
  return (stubmsg->Buffer = (unsigned char *)stubmsg->RpcMsg->Buffer);
}
/***********************************************************************
 *           NdrFreeBuffer [RPCRT4.@]
 */
void WINAPI NdrFreeBuffer(MIDL_STUB_MESSAGE *pStubMsg)
{
  TRACE("(pStubMsg == ^%p): wild guess.\n", pStubMsg);
  I_RpcFreeBuffer(pStubMsg->RpcMsg);
  pStubMsg->BufferLength = 0;
  pStubMsg->Buffer = pStubMsg->BufferEnd = (unsigned char *)(pStubMsg->RpcMsg->Buffer = NULL);
}

/************************************************************************
 *           NdrSendReceive [RPCRT4.@]
 */
unsigned char *WINAPI NdrSendReceive( MIDL_STUB_MESSAGE *stubmsg, unsigned char *buffer  )
{
  TRACE("(stubmsg == ^%p, buffer == ^%p)\n", stubmsg, buffer);

  /* FIXME: how to handle errors? (raise exception?) */
  if (!stubmsg) {
    ERR("NULL stub message.  No action taken.\n");
    return NULL;
  }
  if (!stubmsg->RpcMsg) {
    ERR("RPC Message not present in stub message.  No action taken.\n");
    return NULL;
  }

  stubmsg->Buffer = buffer;
  stubmsg->RpcMsg->BufferLength = stubmsg->Buffer - stubmsg->BufferStart;

  /* not sure where MS does this; for now I'll stick it here */
  stubmsg->RpcMsg->DataRepresentation = NDR_LOCAL_DATA_REPRESENTATION;

  if (I_RpcSendReceive(stubmsg->RpcMsg) != RPC_S_OK) {
    WARN("I_RpcSendReceive did not return success.\n");
  }

  stubmsg->BufferLength = stubmsg->RpcMsg->BufferLength;
  stubmsg->BufferStart = stubmsg->RpcMsg->Buffer;
  stubmsg->BufferEnd = stubmsg->BufferStart + stubmsg->BufferLength;
  stubmsg->Buffer = stubmsg->BufferStart;

  /* FIXME: is this the right return value? */
  return NULL;
}
