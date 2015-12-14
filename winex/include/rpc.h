/**************************************
 *    RPC interface
 *
 */
#if !defined(RPC_NO_WINDOWS_H) && !defined(__WINE__)
#include "windows.h"
#endif

#ifndef __WINE_RPC_H
#define __WINE_RPC_H

#if defined(__PPC__) || defined(_MAC) /* ? */
 #define __RPC_MAC__
#elif defined(_WIN64)
 #define __RPC_WIN64__
#else
 #define __RPC_WIN32__
#endif

#define __RPC_FAR
#define __RPC_API  WINAPI
#define __RPC_USER WINAPI
#define __RPC_STUB WINAPI
#define RPC_ENTRY  WINAPI
#define RPCRTAPI
typedef long RPC_STATUS;

typedef void* I_RPC_HANDLE;

#include "rpcdce.h"
/* #include "rpcnsi.h" */
#include "rpcnterr.h"
/* #include "excpt.h" */
#include "winerror.h"

/* ignore exception handling for now */
#define RpcTryExcept if (1) {
#define RpcExcept(expr) } else {
#define RpcEndExcept }
#define RpcTryFinally
#define RpcFinally
#define RpcEndFinally
#define RpcExceptionCode() 0
/* #define RpcAbnormalTermination() abort() */

#endif /*__WINE_RPC_H */
