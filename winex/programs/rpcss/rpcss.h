/*
 * RPCSS definitions
 *
 * Copyright (C) 2002 Greg Turner
 */

#ifndef __WINE_RPCSS_H
#define __WINE_RPCSS_H

#include "wine/rpcss_shared.h"
#include "windows.h"

/* in seconds */
#define RPCSS_DEFAULT_MAX_LAZY_TIMEOUT 30

/* rpcss_main.c */
HANDLE RPCSS_GetMasterMutex(void);
BOOL RPCSS_ReadyToDie(void);
void RPCSS_SetLazyTimeRemaining(long);
long RPCSS_GetLazyTimeRemaining(void);
void RPCSS_SetMaxLazyTimeout(long);
long RPCSS_GetMaxLazyTimeout(void);

/* epmap_server.c */
BOOL RPCSS_EpmapEmpty(void);
BOOL RPCSS_NPDoWork(void);
void RPCSS_RegisterRpcEndpoints(RPC_SYNTAX_IDENTIFIER iface, int object_count,
  int binding_count, int no_replace, char *vardata, long vardata_size);
void RPCSS_UnregisterRpcEndpoints(RPC_SYNTAX_IDENTIFIER iface, int object_count,
  int binding_count, char *vardata, long vardata_size);
void RPCSS_ResolveRpcEndpoints(RPC_SYNTAX_IDENTIFIER iface, UUID object,
  char *protseq, char *rslt_ep);

/* named_pipe_kludge.c */
BOOL RPCSS_BecomePipeServer(void);
BOOL RPCSS_UnBecomePipeServer(void);
LONG RPCSS_SrvThreadCount(void);

#endif /* __WINE_RPCSS_H */
