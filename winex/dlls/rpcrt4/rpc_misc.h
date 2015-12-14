/*
 * RPC definitions
 *
 * Copyright 2003 Ove Kåven, TransGaming Technologies
 */

#ifndef __WINE_RPC_MISC_H
#define __WINE_RPC_MISC_H

/* flags for RPC_MESSAGE.RpcFlags */
#define WINE_RPCFLAG_EXCEPTION 0x0001

#define OVERLAPPED_WORKS

void RPCRT4_ReadyServer(void);

#endif  /* __WINE_RPC_MISC_H */
