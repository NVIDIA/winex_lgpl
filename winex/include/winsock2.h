/*
 * Winsock 2 definitions - used for ws2_32.dll
 *
 * FIXME: Still missing required Winsock 2 definitions.
 */

#ifndef _WINSOCK2API_
#define _WINSOCK2API_

/*
 * Setup phase
 */

/* Everything common between winsock.h and winsock2.h */
#ifndef INCL_WINSOCK_API_PROTOTYPES
#define INCL_WINSOCK_API_PROTOTYPES 1
#define WS_API_PROTOTYPES          1
#else
#define WS_API_PROTOTYPES          INCL_WINSOCK_API_PROTOTYPES
#endif

#ifndef INCL_WINSOCK_API_TYPEDEFS
#define INCL_WINSOCK_API_TYPEDEFS  0
#define WS_API_TYPEDEFS            0
#else
#define WS_API_TYPEDEFS            INCL_WINSOCK_API_TYPEDEFS
#endif

#define __WINE_WINSOCK2__
#include "winsock.h"
#undef  __WINE_WINSOCK2__

#include <ws2def.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

/* proper 4-byte packing */
#include "pshpack4.h"


#ifndef USE_WS_PREFIX
#define SO_GROUP_ID                0x2001
#define SO_GROUP_PRIORITY          0x2002
#define SO_MAX_MSG_SIZE            0x2003
#define SO_PROTOCOL_INFOA          0x2004
#define SO_PROTOCOL_INFOW          0x2005
#define SO_PROTOCOL_INFO           WINELIB_NAME_AW(WS_SO_PROTOCOL_INFO)
#define PVD_CONFIG                 0x3001
#define SO_CONDITIONAL_ACCEPT      0x3002
#else
#define WS_SO_GROUP_ID             0x2001
#define WS_SO_GROUP_PRIORITY       0x2002
#define WS_SO_MAX_MSG_SIZE         0x2003
#define WS_SO_PROTOCOL_INFOA       0x2004
#define WS_SO_PROTOCOL_INFOW       0x2005
#define WS_SO_PROTOCOL_INFO        WINELIB_NAME_AW(WS_SO_PROTOCOL_INFO)
#define WS_PVD_CONFIG              0x3001
#define WS_SO_CONDITIONAL_ACCEPT   0x3002
#endif


/* option flags per socket */

#define FD_MAX_EVENTS              10
#define FD_READ_BIT                0
#define FD_WRITE_BIT               1
#define FD_OOB_BIT                 2
#define FD_ACCEPT_BIT              3
#define FD_CONNECT_BIT             4
#define FD_CLOSE_BIT               5

/* Constants for LPCONDITIONPROC */
#define CF_ACCEPT                  0x0000
#define CF_REJECT                  0x0001
#define CF_DEFER                   0x0002

/* Constants for shutdown() */
#define SD_RECEIVE                 0x00
#define SD_SEND                    0x01
#define SD_BOTH                    0x02

/* Constants for WSAIoctl() */
#undef IOC_VOID
#undef IOC_IN
#undef IOC_OUT
#undef IOC_INOUT
#define IOC_UNIX                   0x00000000
#define IOC_WS2                    0x08000000
#define IOC_PROTOCOL               0x10000000
#define IOC_VENDOR                 0x18000000
#define IOC_VOID                   0x20000000
#define IOC_OUT                    0x40000000
#define IOC_IN                     0x80000000
#define IOC_INOUT                  (IOC_IN|IOC_OUT)
#define _WSAIO(x,y)                (IOC_VOID|(x)|(y))
#define _WSAIOR(x,y)               (IOC_OUT|(x)|(y))
#define _WSAIOW(x,y)               (IOC_IN|(x)|(y))
#define _WSAIORW(x,y)              (IOC_INOUT|(x)|(y))
#define SIO_ASSOCIATE_HANDLE       _WSAIOW(IOC_WS2,1)
#define SIO_ENABLE_CIRCULAR_QUEUEING _WSAIO(IOC_WS2,2)
#define SIO_FIND_ROUTE             _WSAIOR(IOC_WS2,3)
#define SIO_FLUSH                  _WSAIO(IOC_WS2,4)
#define SIO_GET_BROADCAST_ADDRESS  _WSAIOR(IOC_WS2,5)
#define SIO_GET_EXTENSION_FUNCTION_POINTER  _WSAIORW(IOC_WS2,6)
#define SIO_GET_QOS                _WSAIORW(IOC_WS2,7)
#define SIO_GET_GROUP_QOS          _WSAIORW(IOC_WS2,8)
#define SIO_MULTIPOINT_LOOPBACK    _WSAIOW(IOC_WS2,9)
#define SIO_MULTICAST_SCOPE        _WSAIOW(IOC_WS2,10)
#define SIO_SET_QOS                _WSAIOW(IOC_WS2,11)
#define SIO_SET_GROUP_QOS          _WSAIOW(IOC_WS2,12)
#define SIO_TRANSLATE_HANDLE       _WSAIORW(IOC_WS2,13)
#define SIO_ROUTING_INTERFACE_QUERY _WSAIORW(IOC_WS2,20)
#define SIO_ROUTING_INTERFACE_CHANGE _WSAIOW(IOC_WS2,21)
#define SIO_ADDRESS_LIST_QUERY     _WSAIOR(IOC_WS2,22)
#define SIO_ADDRESS_LIST_CHANGE    _WSAIO(IOC_WS2,23)
#define SIO_QUERY_TARGET_PNP_HANDLE _WSAIOR(IOC_WS2,24)
#ifndef USE_WS_PREFIX
#define SIO_GET_INTERFACE_LIST     _IOR ('t', 127, u_long)
#else
#define SIO_GET_INTERFACE_LIST     WS__IOR ('t', 127, WS_u_long)
#endif

/* Constants for WSAIoctl() */
#define WSA_FLAG_OVERLAPPED        0x01
#define WSA_FLAG_MULTIPOINT_C_ROOT 0x02
#define WSA_FLAG_MULTIPOINT_C_LEAF 0x04
#define WSA_FLAG_MULTIPOINT_D_ROOT 0x08
#define WSA_FLAG_MULTIPOINT_D_LEAF 0x10

/* flags for WSALookupService*(). */
#define LUP_DEEP                0x0001
#define LUP_CONTAINERS          0x0002
#define LUP_NOCONTAINERS        0x0004
#define LUP_NEAREST             0x0008
#define LUP_RETURN_NAME         0x0010
#define LUP_RETURN_TYPE         0x0020
#define LUP_RETURN_VERSION      0x0040
#define LUP_RETURN_COMMENT      0x0080
#define LUP_RETURN_ADDR         0x0100
#define LUP_RETURN_BLOB         0x0200
#define LUP_RETURN_ALIASES      0x0400
#define LUP_RETURN_QUERY_STRING 0x0800
#define LUP_RETURN_ALL          0x0FF0
#define LUP_RES_SERVICE         0x8000

#define LUP_FLUSHCACHE          0x1000
#define LUP_FLUSHPREVIOUS       0x2000

#define LUP_NON_AUTHORITATIVE   0x4000
#define LUP_SECURE              0x8000
#define LUP_RETURN_PREFERRED_NAMES  0x10000

#define LUP_ADDRCONFIG          0x00100000
#define LUP_DUAL_ADDR           0x00200000
#define LUP_FILESERVER          0x00400000

#define  RESULT_IS_ALIAS      0x0001
#define  RESULT_IS_ADDED      0x0010
#define  RESULT_IS_CHANGED    0x0020
#define  RESULT_IS_DELETED    0x0040

#ifndef GUID_DEFINED
#include "guiddef.h"
#endif

#define MAX_PROTOCOL_CHAIN         7
#define BASE_PROTOCOL              1
#define LAYERED_PROTOCOL           0

typedef struct _WSAPROTOCOLCHAIN
{
    int ChainLen;                  /* the length of the chain,     */
                                   /* length = 0 means layered protocol, */
                                   /* length = 1 means base protocol, */
                                   /* length > 1 means protocol chain */
    DWORD ChainEntries[MAX_PROTOCOL_CHAIN]; /* a list of dwCatalogEntryIds */
} WSAPROTOCOLCHAIN, * LPWSAPROTOCOLCHAIN;

#define WSAPROTOCOL_LEN  255
typedef struct _WSAPROTOCOL_INFOA
{
    DWORD dwServiceFlags1;
    DWORD dwServiceFlags2;
    DWORD dwServiceFlags3;
    DWORD dwServiceFlags4;
    DWORD dwProviderFlags;
    GUID ProviderId;
    DWORD dwCatalogEntryId;
    WSAPROTOCOLCHAIN ProtocolChain;
    int iVersion;
    int iAddressFamily;
    int iMaxSockAddr;
    int iMinSockAddr;
    int iSocketType;
    int iProtocol;
    int iProtocolMaxOffset;
    int iNetworkByteOrder;
    int iSecurityScheme;
    DWORD dwMessageSize;
    DWORD dwProviderReserved;
    CHAR szProtocol[WSAPROTOCOL_LEN+1];
} WSAPROTOCOL_INFOA, * LPWSAPROTOCOL_INFOA;

typedef struct _WSAPROTOCOL_INFOW
{
    DWORD dwServiceFlags1;
    DWORD dwServiceFlags2;
    DWORD dwServiceFlags3;
    DWORD dwServiceFlags4;
    DWORD dwProviderFlags;
    GUID ProviderId;
    DWORD dwCatalogEntryId;
    WSAPROTOCOLCHAIN ProtocolChain;
    int iVersion;
    int iAddressFamily;
    int iMaxSockAddr;
    int iMinSockAddr;
    int iSocketType;
    int iProtocol;
    int iProtocolMaxOffset;
    int iNetworkByteOrder;
    int iSecurityScheme;
    DWORD dwMessageSize;
    DWORD dwProviderReserved;
    WCHAR szProtocol[WSAPROTOCOL_LEN+1];
} WSAPROTOCOL_INFOW, *LPWSAPROTOCOL_INFOW;

DECL_WINELIB_TYPE_AW(WSAPROTOCOL_INFO)
DECL_WINELIB_TYPE_AW(LPWSAPROTOCOL_INFO)

typedef struct _WSANETWORKEVENTS
{
    LONG lNetworkEvents;
    int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS, *LPWSANETWORKEVENTS;

typedef struct _WSABUF
{
    ULONG len;
    CHAR* buf;
} WSABUF, *LPWSABUF;

#define WSAEVENT      HANDLE
#define LPWSAEVENT    LPHANDLE
#define WSAOVERLAPPED OVERLAPPED
typedef struct _OVERLAPPED* LPWSAOVERLAPPED;

#define WSA_IO_PENDING             (ERROR_IO_PENDING)
#define WSA_IO_INCOMPLETE          (ERROR_IO_INCOMPLETE)
#define WSA_INVALID_HANDLE         (ERROR_INVALID_HANDLE)
#define WSA_INVALID_PARAMETER      (ERROR_INVALID_PARAMETER)
#define WSA_NOT_ENOUGH_MEMORY      (ERROR_NOT_ENOUGH_MEMORY)
#define WSA_OPERATION_ABORTED      (ERROR_OPERATION_ABORTED)

#define WSA_INVALID_EVENT          ((WSAEVENT)NULL)
#define WSA_MAXIMUM_WAIT_EVENTS    (MAXIMUM_WAIT_OBJECTS)
#define WSA_WAIT_FAILED            ((DWORD)-1L)
#define WSA_WAIT_EVENT_0           (WAIT_OBJECT_0)
#define WSA_WAIT_IO_COMPLETION     (WAIT_IO_COMPLETION)
#define WSA_WAIT_TIMEOUT           (WAIT_TIMEOUT)
#define WSA_INFINITE               (INFINITE)

typedef unsigned int   GROUP;
#define SG_UNCONSTRAINED_GROUP   0x01
#define SG_CONSTRAINED_GROUP     0x02

/*
 * FLOWSPEC and SERVICETYPE should eventually move to qos.h
 */

typedef ULONG   SERVICETYPE;

typedef struct _FLOWSPEC {
       unsigned int      TokenRate;
       unsigned int      TokenBucketSize;
       unsigned int      PeakBandwidth;
       unsigned int      Latency;
        unsigned int      DelayVariation;
       SERVICETYPE       ServiceType;
       unsigned int      MaxSduSize;
       unsigned int      MinimumPolicedSize;
   } FLOWSPEC, *PFLOWSPEC, *LPFLOWSPEC;

typedef struct _QUALITYOFSERVICE {
        FLOWSPEC           SendingFlowspec;
        FLOWSPEC           ReceivingFlowspec;
        WSABUF             ProviderSpecific;
   } QOS, *LPQOS;

typedef int (CALLBACK *LPCONDITIONPROC)
(
    LPWSABUF lpCallerId,
    LPWSABUF lpCallerData,
    LPQOS lpSQOS,
    LPQOS lpGQOS,
    LPWSABUF lpCalleeId,
    LPWSABUF lpCalleeData,
    GROUP *g,
    DWORD dwCallbackData
);

typedef void (CALLBACK *LPWSAOVERLAPPED_COMPLETION_ROUTINE)
(
    DWORD dwError,
    DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped,
    DWORD dwFlags
);


typedef struct _WSANAMESPACE_INFOA {
    GUID                NSProviderId;
    DWORD               dwNameSpace;
    BOOL                fActive;
    DWORD               dwVersion;
    LPSTR               lpszIdentifier;
} WSANAMESPACE_INFOA, *PWSANAMESPACE_INFOA, *LPWSANAMESPACE_INFOA;
typedef struct _WSANAMESPACE_INFOW {
    GUID                NSProviderId;
    DWORD               dwNameSpace;
    BOOL                fActive;
    DWORD               dwVersion;
    LPWSTR              lpszIdentifier;
} WSANAMESPACE_INFOW, *PWSANAMESPACE_INFOW, *LPWSANAMESPACE_INFOW;
#ifdef UNICODE
typedef WSANAMESPACE_INFOW WSANAMESPACE_INFO;
typedef PWSANAMESPACE_INFOW PWSANAMESPACE_INFO;
typedef LPWSANAMESPACE_INFOW LPWSANAMESPACE_INFO;
#else
typedef WSANAMESPACE_INFOA WSANAMESPACE_INFO;
typedef PWSANAMESPACE_INFOA PWSANAMESPACE_INFO;
typedef LPWSANAMESPACE_INFOA LPWSANAMESPACE_INFO;
#endif /* UNICODE */

#ifndef _tagBLOB_DEFINED
#define _tagBLOB_DEFINED
#define _BLOB_DEFINED
#define _LPBLOB_DEFINED
typedef struct _BLOB
{
    ULONG cbSize;
    BYTE *pBlobData;
} BLOB, *LPBLOB;
#endif

typedef struct _AFPROTOCOLS
{
    INT iAddressFamily;
    INT iProtocol;
} AFPROTOCOLS, *PAFPROTOCOLS, *LPAFPROTOCOLS;

typedef enum _WSAEcomparator
{
    COMP_EQUAL = 0,
    COMP_NOTLESS
} WSAECOMPARATOR, *PWSAECOMPARATOR, *LPWSAECOMPARATOR;

typedef struct _WSAVersion
{
    DWORD           dwVersion;
    WSAECOMPARATOR  ecHow;
} WSAVERSION, *PWSAVERSION, *LPWSAVERSION;

typedef struct _WSAQuerySetA
{
    DWORD           dwSize;
    LPSTR           lpszServiceInstanceName;
    LPGUID          lpServiceClassId;
    LPWSAVERSION    lpVersion;
    LPSTR           lpszComment;
    DWORD           dwNameSpace;
    LPGUID          lpNSProviderId;
    LPSTR           lpszContext;
    DWORD           dwNumberOfProtocols;
    LPAFPROTOCOLS   lpafpProtocols;
    LPSTR           lpszQueryString;
    DWORD           dwNumberOfCsAddrs;
    LPCSADDR_INFO   lpcsaBuffer;
    DWORD           dwOutputFlags;
    LPBLOB          lpBlob;
} WSAQUERYSETA, *PWSAQUERYSETA, *LPWSAQUERYSETA;

typedef struct _WSAQuerySetW
{
    DWORD           dwSize;
    LPWSTR          lpszServiceInstanceName;
    LPGUID          lpServiceClassId;
    LPWSAVERSION    lpVersion;
    LPWSTR          lpszComment;
    DWORD           dwNameSpace;
    LPGUID          lpNSProviderId;
    LPWSTR          lpszContext;
    DWORD           dwNumberOfProtocols;
    LPAFPROTOCOLS   lpafpProtocols;
    LPWSTR          lpszQueryString;
    DWORD           dwNumberOfCsAddrs;
    LPCSADDR_INFO   lpcsaBuffer;
    DWORD           dwOutputFlags;
    LPBLOB          lpBlob;
} WSAQUERYSETW, *PWSAQUERYSETW, *LPWSAQUERYSETW;

typedef struct _WSAQuerySet2A
{
    DWORD           dwSize;
    LPSTR           lpszServiceInstanceName;
    LPWSAVERSION    lpVersion;
    LPSTR           lpszComment;
    DWORD           dwNameSpace;
    LPGUID          lpNSProviderId;
    LPSTR           lpszContext;
    DWORD           dwNumberOfProtocols;
    LPAFPROTOCOLS   lpafpProtocols;
    LPSTR           lpszQueryString;
    DWORD           dwNumberOfCsAddrs;
    LPCSADDR_INFO   lpcsaBuffer;
    DWORD           dwOutputFlags;
    LPBLOB          lpBlob;   
} WSAQUERYSET2A, *PWSAQUERYSET2A, *LPWSAQUERYSET2A;

typedef struct _WSAQuerySet2W
{
    DWORD           dwSize;
    LPWSTR          lpszServiceInstanceName;
    LPWSAVERSION    lpVersion;
    LPWSTR          lpszComment;
    DWORD           dwNameSpace;
    LPGUID          lpNSProviderId;
    LPWSTR          lpszContext;
    DWORD           dwNumberOfProtocols;
    LPAFPROTOCOLS   lpafpProtocols;
    LPWSTR          lpszQueryString;
    DWORD           dwNumberOfCsAddrs;
    LPCSADDR_INFO   lpcsaBuffer;
    DWORD           dwOutputFlags;
    LPBLOB          lpBlob;   
} WSAQUERYSET2W, *PWSAQUERYSET2W, *LPWSAQUERYSET2W;

#ifdef UNICODE
typedef WSAQUERYSETW WSAQUERYSET;
typedef PWSAQUERYSETW PWSAQUERYSET;
typedef LPWSAQUERYSETW LPWSAQUERYSET;
typedef WSAQUERYSET2W WSAQUERYSET2;
typedef PWSAQUERYSET2W PWSAQUERYSET2;
typedef LPWSAQUERYSET2W LPWSAQUERYSET2;
#else
typedef WSAQUERYSETA WSAQUERYSET;
typedef PWSAQUERYSETA PWSAQUERYSET;
typedef LPWSAQUERYSETA LPWSAQUERYSET;
typedef WSAQUERYSET2A WSAQUERYSET2;
typedef PWSAQUERYSET2A PWSAQUERYSET2;
typedef LPWSAQUERYSET2A LPWSAQUERYSET2;
#endif


#ifndef WSABASEERR
#define WSABASEERR              10000
/*
 * New Winsock error codes
 */
#define WSAENOMORE              (WSABASEERR+102)
#define WSAECANCELLED           (WSABASEERR+103)
#define WSAEINVALIDPROCTABLE    (WSABASEERR+104)
#define WSAEINVALIDPROVIDER     (WSABASEERR+105)
#define WSAEPROVIDERFAILEDINIT  (WSABASEERR+106)
#define WSASYSCALLFAILURE       (WSABASEERR+107)
#define WSASERVICE_NOT_FOUND    (WSABASEERR+108)
#define WSATYPE_NOT_FOUND       (WSABASEERR+109)
#define WSA_E_NO_MORE           (WSABASEERR+110)
#define WSA_E_CANCELLED         (WSABASEERR+111)
#define WSAEREFUSED             (WSABASEERR+112)

#endif

/*
 * Winsock Function Typedefs
 *
 * Remember to keep this section in sync with the
 * "Prototypes" section in winsock.h.
 */
#if WS_API_TYPEDEFS
typedef HANDLE (WINAPI *LPFN_WSAASYNCGETHOSTBYADDR)(HWND,WS(u_int),const char*,int,int,char*,int);
typedef HANDLE (WINAPI *LPFN_WSAASYNCGETHOSTBYNAME)(HWND,WS(u_int),const char*,char*,int);
typedef HANDLE (WINAPI *LPFN_WSAASYNCGETPROTOBYNAME)(HWND,WS(u_int),const char*,char*,int);
typedef HANDLE (WINAPI *LPFN_WSAASYNCGETPROTOBYNUMBER)(HWND,WS(u_int),int,char*,int);
typedef HANDLE (WINAPI *LPFN_WSAASYNCGETSERVBYNAME)(HWND,WS(u_int),const char*,const char*,char*,int);
typedef HANDLE (WINAPI *LPFN_WSAASYNCGETSERVBYPORT)(HWND,WS(u_int),int,const char*,char*,int);
typedef int (WINAPI *LPFN_WSAASYNCSELECT)(SOCKET,HWND,WS(u_int),LONG);
typedef int (WINAPI *LPFN_WSACANCELASYNCREQUEST)(HANDLE);
typedef int (WINAPI *LPFN_WSACANCELBLOCKINGCALL)(void);
typedef int (WINAPI *LPFN_WSACLEANUP)(void);
typedef int (WINAPI *LPFN_WSAGETLASTERROR)(void);
typedef BOOL (WINAPI *LPFN_WSAISBLOCKING)(void);
typedef FARPROC (WINAPI *LPFN_WSASETBLOCKINGHOOK)(FARPROC);
typedef void (WINAPI *LPFN_WSASETLASTERROR)(int);
typedef int (WINAPI *LPFN_WSASTARTUP)(WORD,LPWSADATA);
typedef int (WINAPI *LPFN_WSAUNHOOKBLOCKINGHOOK)(void);

typedef SOCKET (WINAPI *LPFN_ACCEPT)(SOCKET,struct WS(sockaddr)*,int*);
typedef int (WINAPI *LPFN_BIND)(SOCKET,const struct WS(sockaddr)*,int);
typedef int (WINAPI *LPFN_CLOSESOCKET)(SOCKET);
typedef int (WINAPI *LPFN_CONNECT)(SOCKET,const struct WS(sockaddr)*,int);
typedef struct WS(hostent)* (WINAPI *LPFN_GETHOSTBYADDR)(const char*,int,int);
typedef struct WS(hostent)* (WINAPI *LPFN_GETHOSTBYNAME)(const char*);
typedef int (WINAPI *LPFN_GETHOSTNAME)(char*,int);
typedef int (WINAPI *LPFN_GETPEERNAME)(SOCKET,struct WS(sockaddr)*,int*);
typedef struct WS(protoent)* (WINAPI *LPFN_GETPROTOBYNAME)(const char*);
typedef struct WS(protoent)* (WINAPI *LPFN_GETPROTOBYNUMBER)(int);
#ifdef WS_DEFINE_SELECT
typedef int (WINAPI* LPFN_SELECT)(int,WS(fd_set)*,WS(fd_set)*,WS(fd_set)*,const struct WS(timeval)*);
#endif
typedef struct WS(servent)* (WINAPI *LPFN_GETSERVBYNAME)(const char*,const char*);
typedef struct WS(servent)* (WINAPI *LPFN_GETSERVBYPORT)(int,const char*);
typedef int (WINAPI *LPFN_GETSOCKNAME)(SOCKET,struct WS(sockaddr)*,int*);
typedef int (WINAPI *LPFN_GETSOCKOPT)(SOCKET,int,int,char*,int*);
typedef WS(u_long) (WINAPI *LPFN_HTONL)(WS(u_long));
typedef WS(u_short) (WINAPI *LPFN_HTONS)(WS(u_short));
typedef ULONG (WINAPI *LPFN_INET_ADDR)(const char*);
typedef char* (WINAPI *LPFN_INET_NTOA)(struct WS(in_addr);
typedef int (WINAPI *LPFN_IOCTLSOCKET)(SOCKET,LONG,WS(u_long)*);
typedef int (WINAPI *LPFN_LISTEN)(SOCKET,int);
typedef WS(u_long) (WINAPI *LPFN_NTOHL)(WS(u_long));
typedef WS(u_short) (WINAPI *LPFN_NTOHS)(WS(u_short));
typedef int (WINAPI *LPFN_RECV)(SOCKET,char*,int,int);
typedef int (WINAPI *LPFN_RECVFROM)(SOCKET,char*,int,int,struct WS(sockaddr)*,int*);
typedef int (WINAPI *LPFN_SEND)(SOCKET,const char*,int,int);
typedef int (WINAPI *LPFN_SENDTO)(SOCKET,const char*,int,int,const struct WS(sockaddr)*,int);
typedef int (WINAPI *LPFN_SETSOCKOPT)(SOCKET,int,int,const char*,int);
typedef int (WINAPI *LPFN_SHUTDOWN)(SOCKET,int);
typedef SOCKET (WINAPI *LPFN_SOCKET)(int,int,int);
#endif /* WS_API_TYPEDEFS */



/*
 * Winsock2 Prototypes
 *
 * Remember to keep this section in sync with the
 * "Winsock2 Function Typedefs" section below.
 */
#if WS_API_PROTOTYPES
SOCKET WINAPI WSAAccept(SOCKET,struct WS(sockaddr)*,LPINT,LPCONDITIONPROC,DWORD);
INT WINAPI WSAAddressToStringA(LPSOCKADDR,DWORD,LPWSAPROTOCOL_INFOA,LPSTR,LPDWORD);
INT WINAPI WSAAddressToStringW(LPSOCKADDR,DWORD,LPWSAPROTOCOL_INFOW,LPWSTR,LPDWORD);
#define WSAAddressToString         WINELIB_NAME_AW(WSAAddressToString)
BOOL WINAPI WSACloseEvent(WSAEVENT);
int WINAPI WSAConnect(SOCKET,const struct WS(sockaddr)*,int,LPWSABUF,LPWSABUF,LPQOS,LPQOS);
WSAEVENT WINAPI WSACreateEvent(void);
/* WSADuplicateSocketA */
/* WSADuplicateSocketW */
INT WINAPI WSAEnumNameSpaceProvidersA(LPDWORD,LPWSANAMESPACE_INFOA);
INT WINAPI WSAEnumNameSpaceProvidersW(LPDWORD,LPWSANAMESPACE_INFOW);
#define WSAEnumNameSpaceProviders  WINELIB_NAME_AW(WSAEnumNameSpaceProviders)
int WINAPI WSAEnumNetworkEvents(SOCKET,WSAEVENT,LPWSANETWORKEVENTS);
int WINAPI WSAEnumProtocolsA(LPINT,LPWSAPROTOCOL_INFOA,LPDWORD);
int WINAPI WSAEnumProtocolsW(LPINT,LPWSAPROTOCOL_INFOW,LPDWORD);
#define WSAEnumProtocols           WINELIB_NAME_AW(WSAEnumProtocols)
int WINAPI WSAEventSelect(SOCKET,WSAEVENT,LONG);
BOOL WINAPI WSAGetOverlappedResult(SOCKET,LPWSAOVERLAPPED,LPDWORD,BOOL,LPDWORD);
/* WSAGetQOSByName */
/* WSAGetServiceClassInfoA */
/* WSAGetServiceClassInfoW */
/* WSAGetServiceClassNameByClassIdA */
/* WSAGetServiceClassNameByClassIdW */
int WINAPI WSAHtonl(SOCKET,WS(u_long),WS(u_long)*);
int WINAPI WSAHtons(SOCKET,WS(u_short),WS(u_short)*);
/* WSAInstallServiceClassA */
/* WSAInstallServiceClassW */
int WINAPI WSAIoctl(SOCKET,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
/* WSAJoinLeaf */
/* WSALookupServiceBeginA */
/* WSALookupServiceBeginW */
/* WSALookupServiceEnd */
/* WSALookupServiceNextA */
/* WSALookupServiceNextW */
int WINAPI WSANtohl(SOCKET,WS(u_long),WS(u_long)*);
int WINAPI WSANtohs(SOCKET,WS(u_short),WS(u_short)*);
/* WSAProviderConfigChange */
int WINAPI WSARecv(SOCKET,LPWSABUF,DWORD,LPDWORD,LPDWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
int WINAPI WSARecvDisconnect(SOCKET,LPWSABUF);
int WINAPI WSARecvFrom(SOCKET,LPWSABUF,DWORD,LPDWORD,LPDWORD,struct WS(sockaddr)*,LPINT,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
/* WSARemoveServiceClass */
BOOL WINAPI WSAResetEvent(WSAEVENT);
int WINAPI WSASend(SOCKET,LPWSABUF,DWORD,LPDWORD,DWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
int WINAPI WSASendDisconnect(SOCKET,LPWSABUF);
int WINAPI WSASendTo(SOCKET,LPWSABUF,DWORD,LPDWORD,DWORD,const struct WS(sockaddr)*,int,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
BOOL WINAPI WSASetEvent(WSAEVENT);
/* WSASetServiceA */
/* WSASetServiceW */
SOCKET WINAPI WSASocketA(int,int,int,LPWSAPROTOCOL_INFOA,GROUP,DWORD);
SOCKET WINAPI WSASocketW(int,int,int,LPWSAPROTOCOL_INFOW,GROUP,DWORD);
INT WINAPI WSAStringToAddressA(LPSTR,INT,LPWSAPROTOCOL_INFOA,LPSOCKADDR,LPINT);
INT WINAPI WSAStringToAddressW(LPWSTR,INT,LPWSAPROTOCOL_INFOW,LPSOCKADDR,LPINT);
#define WSASocket                  WINELIB_NAME_AW(WSASocket)
#define WSAStringToAddress         WINELIB_NAME_AW(WSAStringToAddress)
DWORD WINAPI WSAWaitForMultipleEvents(DWORD,const WSAEVENT*,BOOL,DWORD,BOOL);
#endif /* WS_API_PROTOTYPES */



/*
 * Winsock2 Function Typedefs
 *
 * Remember to keep this section in sync with the
 * "Winsock2 Prototypes" section above.
 */
#if WS_API_TYPEDEFS
typedef SOCKET (WINAPI *LPFN_WSAACCEPT)(SOCKET,WS(sockaddr)*,LPINT,LPCONDITIONPROC,DWORD);
typedef INT (WINAPI *LPFN_WSAADRESSTOSTRINGA)(LPSOCKADDR,DWORD,LPWSAPROTOCOL_INFOA,LPSTR,LPDWORD);
typedef INT (WINAPI *LPFN_WSAADRESSTOSTRINGW)(LPSOCKADDR,DWORD,LPWSAPROTOCOL_INFOW,LPWSTR,LPDWORD);
#define LPFN_WSAADDRESSTOSTRING    WINELIB_NAME_AW(LPFN_WSAADDRESSTOSTRING)
typedef BOOL (WINAPI *LPFN_WSACLOSEEVENT)(WSAEVENT);
typedef int (WINAPI *LPFN_WSACONNECT)(SOCKET,const struct WS(sockaddr)*,int,LPWSABUF,LPWSABUF,LPQOS,LPQOS);
typedef WSAEVENT (WINAPI *LPFN_WSACREATEEVENT)(void);
/* WSADuplicateSocketA */
/* WSADuplicateSocketW */
typedef INT (WINAPI *LPFN_WSAENUMNAMESPACEPROVIDERSA)(LPDWORD,LPWSANAMESPACE_INFOA);
typedef INT (WINAPI *LPFN_WSAENUMNAMESPACEPROVIDERSW)(LPDWORD,LPWSANAMESPACE_INFOW);
#define LPFN_WSAENUMNAMESPACEPROVIDERS  WINELIB_NAME_AW(LPFN_WSAENUMNAMESPACEPROVIDERS)
typedef int (WINAPI *LPFN_WSAENUMNETWORKEVENT)(SOCKET,WSAEVENT,LPWSANETWORKEVENTS);
typedef int (WINAPI *LPFN_WSAENUMPROTOCOLSA)(LPINT,LPWSAPROTOCOL_INFOA,LPDWORD);
typedef int (WINAPI *LPFN_WSAENUMPROTOCOLSW)(LPINT,LPWSAPROTOCOL_INFOW,LPDWORD);
#define LPFN_WSAENUMPROTOCOLS      WINELIB_NAME_AW(LPFN_WSAENUMPROTOCOLS)
typedef int (WINAPI *LPFN_WSAEVENTSELECT)(SOCKET,WSAEVENT,LONG);
typedef BOOL (WINAPI *LPFN_WSAGETOVERLAPPEDRESULT)(SOCKET,LPWSAOVERLAPPED,LPDWORD,BOOL,LPDWORD);
/* WSAGetQOSByName */
/* WSAGetServiceClassInfoA */
/* WSAGetServiceClassInfoW */
/* WSAGetServiceClassNameByClassIdA */
/* WSAGetServiceClassNameByClassIdW */
typedef int (WINAPI *LPFN_WSAHTONL)(SOCKET,WS(u_long),WS(u_long)*);
typedef int (WINAPI *LPFN_WSAHTONS)(SOCKET,WS(u_short),WS(u_short)*);
/* WSAInstallServiceClassA */
/* WSAInstallServiceClassW */
typedef int (WINAPI *LPFN_WSAIOCTL)(SOCKET,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
/* WSAJoinLeaf */
/* WSALookupServiceBeginA */
/* WSALookupServiceBeginW */
/* WSALookupServiceEnd */
/* WSALookupServiceNextA */
/* WSALookupServiceNextW */
typedef int (WINAPI *LPFN_WSANTOHL)(SOCKET,WS(u_long),WS(u_long)*);
typedef int (WINAPI *LPFN_WSANTOHS)(SOCKET,WS(u_short),WS(u_short)*);
/* WSAProviderConfigChange */
typedef int (WINAPI *LPFN_WSARECV)(SOCKET,LPWSABUF,DWORD,LPDWORD,LPDWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI *LPFN_WSARECVDISCONNECT)(SOCKET,LPWSABUF);
typedef int (WINAPI *LPFN_WSARECVFROM)(SOCKET,LPWSABUF,DWORD,LPDWORD,LPDWORD,struct WS(sockaddr)*,LPINT,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
/* WSARemoveServiceClass */
typedef BOOL (WINAPI *LPFN_WSARESETEVENT)(WSAEVENT);
typedef int (WINAPI *LPFN_WSASEND)(SOCKET,LPWSABUF,DWORD,LPDWORD,DWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI *LPFN_WSASENDDISCONNECT)(SOCKET,LPWSABUF);
typedef int (WINAPI *LPFN_WSASENDTO)(SOCKET,LPWSABUF,DWORD,LPDWORD,DWORD,const struct WS(sockaddr)*,int,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef BOOL (WINAPI *LPFN_WSASETEVENT)(WSAEVENT);
/* WSASetServiceA */
/* WSASetServiceW */
typedef SOCKET (WINAPI *LPFN_WSASOCKETA)(int,int,int,LPWSAPROTOCOL_INFOA,GROUP,DWORD);
typedef SOCKET (WINAPI *LPFN_WSASOCKETW)(int,int,int,LPWSAPROTOCOL_INFOW,GROUP,DWORD);
typedef INT (WINAPI *LPFN_WSASTRINGTOADDRESSA)(LPSTR,INT,LPWSAPROTOCOL_INFOA,LPSOCKADDR,LPINT);
typedef INT (WINAPI *LPFN_WSASTRINGTOADDRESSW)(LPSTR,INT,LPWSAPROTOCOL_INFOA,LPSOCKADDR,LPINT);
#define LPFN_WSASOCKET             WINELIB_NAME_AW(LPFN_WSASOCKET)
#define LPFN_WSASTRINGTOADDRESS    WINELIB_NAME_AW(LPFN_WSASTRINGTOADDRESS)
typedef DWORD (WINAPI *LPFN_WSAWAITFORMULTIPLEEVENTS)(DWORD,const WSAEVENT*,BOOL,DWORD,BOOL);
#endif /* WS_API_TYPEDEFS */


/* Condition function return values */
#define CF_ACCEPT       0x0000
#define CF_REJECT       0x0001
#define CF_DEFER        0x0002

#include "poppack.h"

#ifdef __cplusplus
}
#endif

#undef WS
#undef WS_API_PROTOTYPES
#undef WS_API_TYPEDEFS

#endif /* __WINSOCK2API__ */
