/*
 * RPC definitions
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 */

#ifndef __WINE_RPC_MISC_H
#define __WINE_RPC_MISC_H

/* flags for RPC_MESSAGE.RpcFlags */
#define WINE_RPCFLAG_EXCEPTION 0x0001

#define OVERLAPPED_WORKS

void RPCRT4_ReadyServer(void);

#endif  /* __WINE_RPC_MISC_H */
