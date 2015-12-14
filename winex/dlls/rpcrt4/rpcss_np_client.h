/*
 * Copyright (C) 2002 Greg Turner
 */

#ifndef __WINE_RPCSS_NP_CLIENT_H
#define __WINE_RPCSS_NP_CLIENT_H

/* rpcss_np_client.c */
HANDLE RPC_RpcssNPConnect(void);
BOOL RPCRT4_SendReceiveNPMsg(HANDLE, PRPCSS_NP_MESSAGE, char *,  PRPCSS_NP_REPLY);

#endif /* __RPCSS_NP_CLINET_H */
