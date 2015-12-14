#ifndef _WS2IPDEF_
#define _WS2IPDEF_

#include <in6addr.h>

#ifdef USE_WS_PREFIX
#define WS(x)    WS_##x
#else
#define WS(x)    x
#endif

/* FIXME: This gets defined by some Unix (Linux) header and messes things */
#undef s6_addr



struct WS(sockaddr_in6_old)
{
    SHORT       sin6_family;
    USHORT      sin6_port;
    ULONG       sin6_flowinfo;
    IN6_ADDR    sin6_addr;
};

typedef struct WS(sockaddr_in6)
{
   short    sin6_family;            /* AF_INET6 */
   u_short  sin6_port;              /* Transport level port number */
   u_long   sin6_flowinfo;          /* IPv6 flow information */
   IN6_ADDR sin6_addr; /* IPv6 address */
   union {
       ULONG sin6_scope_id;     // Set of interfaces for a scope.
       SCOPE_ID sin6_scope_struct; 
    };
} SOCKADDR_IN6,*PSOCKADDR_IN6, *LPSOCKADDR_IN6,
  SOCKADDR_IN6_LH, *PSOCKADDR_IN6_LH, FAR *LPSOCKADDR_IN6_LH;

typedef union _SOCKADDR_INET {
    SOCKADDR_IN Ipv4;
    SOCKADDR_IN6 Ipv6;
    ADDRESS_FAMILY si_family;    
} SOCKADDR_INET, *PSOCKADDR_INET;

typedef struct _sockaddr_in6_pair
{
    PSOCKADDR_IN6 SourceAddress;
    PSOCKADDR_IN6 DestinationAddress;
} SOCKADDR_IN6_PAIR, *PSOCKADDR_IN6_PAIR;

typedef struct sockaddr_in6_w2ksp1
{
    short       sin6_family;
    USHORT      sin6_port;
    ULONG       sin6_flowinfo;
    IN6_ADDR    sin6_addr;
    ULONG       sin6_scope_id;
} SOCKADDR_IN6_W2KSP1, *PSOCKADDR_IN6_W2KSP1, FAR *LPSOCKADDR_IN6_W2KSP1;

typedef union sockaddr_gen
{
   struct WS(sockaddr) Address;
   struct WS(sockaddr_in)  AddressIn;
   struct WS(sockaddr_in6_old) AddressIn6;
} WS(sockaddr_gen);

/* Structure to keep interface specific information */
typedef struct _INTERFACE_INFO
{
    u_long            iiFlags;             /* Interface flags */
    WS(sockaddr_gen)  iiAddress;           /* Interface address */
    WS(sockaddr_gen)  iiBroadcastAddress;  /* Broadcast address */
    WS(sockaddr_gen)  iiNetmask;           /* Network mask */
} INTERFACE_INFO, * LPINTERFACE_INFO;

/* Possible flags for the  iiFlags - bitmask  */
#ifndef USE_WS_PREFIX
#define IFF_UP                0x00000001 /* Interface is up */
#define IFF_BROADCAST         0x00000002 /* Broadcast is  supported */
#define IFF_LOOPBACK          0x00000004 /* this is loopback interface */
#define IFF_POINTTOPOINT      0x00000008 /* this is point-to-point interface */
#define IFF_MULTICAST         0x00000010 /* multicast is supported */
#else
#define WS_IFF_UP             0x00000001 /* Interface is up */
#define WS_IFF_BROADCAST      0x00000002 /* Broadcast is  supported */
#define WS_IFF_LOOPBACK       0x00000004 /* this is loopback interface */
#define WS_IFF_POINTTOPOINT   0x00000008 /* this is point-to-point interface */
#define WS_IFF_MULTICAST      0x00000010 /* multicast is supported */
#endif /* USE_WS_PREFIX */

#ifndef USE_WS_PREFIX
#define IP_OPTIONS                 1
#define IP_HDRINCL                 2
#define IP_TOS                     3
#define IP_TTL                     4
#define IP_MULTICAST_IF            9
#define IP_MULTICAST_TTL          10
#define IP_MULTICAST_LOOP         11
#define IP_ADD_MEMBERSHIP         12
#define IP_DROP_MEMBERSHIP        13
#define IP_DONTFRAGMENT           14
#define IP_ADD_SOURCE_MEMBERSHIP  15
#define IP_DROP_SOURCE_MEMBERSHIP 16
#define IP_BLOCK_SOURCE           17
#define IP_UNBLOCK_SOURCE         18
#define IP_PKTINFO                19
#define IP_HOPLIMIT               21
#define IP_RECEIVE_BROADCAST      22
#define IP_RECVIF                 24
#define IP_RECVDSTADDR            25
#define IP_IFLIST                 28
#define IP_ADD_IFLIST             29
#define IP_DEL_IFLIST             30
#define IP_UNICAST_IF             31
#define IP_RTHDR                  32
#define IP_RECVRTHDR              38
#define IP_TCLASS                 39
#define IP_RECVTCLASS             40
#define IP_ORIGINAL_ARRIVAL_IF    47
#else /* !USE_WS_PREFIX */
#define WS_IP_OPTIONS                 1
#define WS_IP_HDRINCL                 2
#define WS_IP_TOS                     3
#define WS_IP_TTL                     4
#define WS_IP_MULTICAST_IF            9
#define WS_IP_MULTICAST_TTL          10
#define WS_IP_MULTICAST_LOOP         11
#define WS_IP_ADD_MEMBERSHIP         12
#define WS_IP_DROP_MEMBERSHIP        13
#define WS_IP_DONTFRAGMENT           14
#define WS_IP_ADD_SOURCE_MEMBERSHIP  15
#define WS_IP_DROP_SOURCE_MEMBERSHIP 16
#define WS_IP_BLOCK_SOURCE           17
#define WS_IP_UNBLOCK_SOURCE         18
#define WS_IP_PKTINFO                19
#define WS_IP_HOPLIMIT               21
#define WS_IP_RECEIVE_BROADCAST      22
#define WS_IP_RECVIF                 24
#define WS_IP_RECVDSTADDR            25
#define WS_IP_IFLIST                 28
#define WS_IP_ADD_IFLIST             29
#define WS_IP_DEL_IFLIST             30
#define WS_IP_UNICAST_IF             31
#define WS_IP_RTHDR                  32
#define WS_IP_RECVRTHDR              38
#define WS_IP_TCLASS                 39
#define WS_IP_RECVTCLASS             40
#define WS_IP_ORIGINAL_ARRIVAL_IF    47
#endif /* !USE_WS_PREFIX */

typedef struct WS(ip_mreq) {
    IN_ADDR imr_multiaddr;
    IN_ADDR imr_interface;
} IP_MREQ, *PIP_MREQ;

#endif /* _WS2IPDEF_ */
