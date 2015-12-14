#ifndef _WS2DEF_
#define _WS2DEF_

#ifdef USE_WS_PREFIX
#define WS(x)    WS_##x
#else
#define WS(x)    x
#endif

#ifndef __CSADDR_DEFINED__
#define __CSADDR_DEFINED__

typedef struct _SOCKET_ADDRESS {
        LPSOCKADDR      lpSockaddr;
        INT             iSockaddrLength;
} SOCKET_ADDRESS, *PSOCKET_ADDRESS, *LPSOCKET_ADDRESS;

typedef struct _CSADDR_INFO {
        SOCKET_ADDRESS  LocalAddr;
        SOCKET_ADDRESS  RemoteAddr;
        INT             iSocketType;
        INT             iProtocol;
} CSADDR_INFO, *PCSADDR_INFO, *LPCSADDR_INFO;
#endif

#ifdef USE_WS_PREFIX
#define WS__SS_MAXSIZE 128
#define WS__SS_ALIGNSIZE (sizeof(__int64))
#define WS__SS_PAD1SIZE (WS__SS_ALIGNSIZE - sizeof(short))
#define WS__SS_PAD2SIZE (WS__SS_MAXSIZE - 2 * WS__SS_ALIGNSIZE)
#else
#define _SS_MAXSIZE 128
#define _SS_ALIGNSIZE (sizeof(__int64))
#define _SS_PAD1SIZE (_SS_ALIGNSIZE - sizeof(short))
#define _SS_PAD2SIZE (_SS_MAXSIZE - 2 * _SS_ALIGNSIZE)
#endif

typedef struct WS(sockaddr_storage) {
        short ss_family;
        char __ss_pad1[WS(_SS_PAD1SIZE)];
        __int64 __ss_align;
        char __ss_pad2[WS(_SS_PAD2SIZE)];
} SOCKADDR_STORAGE, *PSOCKADDR_STORAGE, *LPSOCKADDR_STORAGE;

typedef struct _SOCKET_ADDRESS_LIST {
        INT             iAddressCount;
        SOCKET_ADDRESS  Address[1];
} SOCKET_ADDRESS_LIST, *LPSOCKET_ADDRESS_LIST;

#endif
