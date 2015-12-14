#ifndef _WS2DEF_
#define _WS2DEF_

#ifdef USE_WS_PREFIX
#define WS(x)    WS_##x
#else
#define WS(x)    x
#endif

#ifndef __CSADDR_DEFINED__
#define __CSADDR_DEFINED__

typedef USHORT ADDRESS_FAMILY;

#ifdef USE_WS_PREFIX
#define WS_AF_UNSPEC       0
#define WS_AF_UNIX         1
#define WS_AF_INET         2
#define WS_AF_IMPLINK      3
#define WS_AF_PUP          4
#define WS_AF_CHAOS        5
#define WS_AF_NS           6
#define WS_AF_IPX          WS_AF_NS
#define WS_AF_ISO          7
#define WS_AF_OSI          AF_ISO
#define WS_AF_ECMA         8
#define WS_AF_DATAKIT      9
#define WS_AF_CCITT        10
#define WS_AF_SNA          11
#define WS_AF_DECnet       12
#define WS_AF_DLI          13
#define WS_AF_LAT          14
#define WS_AF_HYLINK       15
#define WS_AF_APPLETALK    16
#define WS_AF_NETBIOS      17
#define WS_AF_VOICEVIEW    18
#define WS_AF_FIREFOX      19
#define WS_AF_UNKNOWN1     20
#define WS_AF_BAN          21
#define WS_AF_ATM          22
#define WS_AF_INET6        23
#define WS_AF_CLUSTER      24
#define WS_AF_12844        25
#define WS_AF_IRDA         26
#define WS_AF_NETDES       28
#define WS_AF_TCNPROCESS   29
#define WS_AF_TCNMESSAGE   30
#define WS_AF_ICLFXBM      31
#define WS_AF_BTH          32
#define WS_AF_LINK         33
#define WS_AF_MAX          34
#else
#define AF_UNSPEC       0
#define AF_UNIX         1
#define AF_INET         2
#define AF_IMPLINK      3
#define AF_PUP          4
#define AF_CHAOS        5
#define AF_NS           6
#define AF_IPX          AF_NS
#define AF_ISO          7
#define AF_OSI          AF_ISO
#define AF_ECMA         8
#define AF_DATAKIT      9
#define AF_CCITT        10
#define AF_SNA          11
#define AF_DECnet       12
#define AF_DLI          13
#define AF_LAT          14
#define AF_HYLINK       15
#define AF_APPLETALK    16
#define AF_NETBIOS      17
#define AF_VOICEVIEW    18
#define AF_FIREFOX      19
#define AF_UNKNOWN1     20
#define AF_BAN          21
#define AF_ATM          22
#define AF_INET6        23
#define AF_CLUSTER      24
#define AF_12844        25
#define AF_IRDA         26
#define AF_NETDES       28
#define AF_TCNPROCESS   29
#define AF_TCNMESSAGE   30
#define AF_ICLFXBM      31
#define AF_BTH          32
#define AF_LINK         33
#define AF_MAX          34
#endif

typedef enum {
    ScopeLevelInterface    = 1,
    ScopeLevelLink         = 2,
    ScopeLevelSubnet       = 3,
    ScopeLevelAdmin        = 4,
    ScopeLevelSite         = 5,
    ScopeLevelOrganization = 8,
    ScopeLevelGlobal       = 14,
    ScopeLevelCount        = 16
} SCOPE_LEVEL;

typedef struct {
    union {
        struct {
            ULONG Zone : 28;
            ULONG Level : 4;
        };
        ULONG Value;
    };
} SCOPE_ID, *PSCOPE_ID;

#define SCOPEID_UNSPECIFIED_INIT    { 0 }

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
