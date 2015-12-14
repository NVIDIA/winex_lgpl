#ifndef __WS2TCPIP__
#define __WS2TCPIP__

#ifdef USE_WS_PREFIX
#define WS(x)    WS_##x
#else
#define WS(x)    x
#endif

/* FIXME: This gets defined by some Unix (Linux) header and messes things */
#undef s6_addr

typedef struct WS(in_addr6)
{
   u_char s6_addr[16];   /* IPv6 address */
} IN6_ADDR, *PIN6_ADDR, *LPIN6_ADDR;

typedef struct WS(sockaddr_in6)
{
   short   sin6_family;            /* AF_INET6 */
   u_short sin6_port;              /* Transport level port number */
   u_long  sin6_flowinfo;          /* IPv6 flow information */
   struct  WS(in_addr6) sin6_addr; /* IPv6 address */
} SOCKADDR_IN6,*PSOCKADDR_IN6, *LPSOCKADDR_IN6;

typedef union sockaddr_gen
{
   struct WS(sockaddr) Address;
   struct WS(sockaddr_in)  AddressIn;
   struct WS(sockaddr_in6) AddressIn6;
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
#define EAI_AGAIN             WSATRY_AGAIN
#define EAI_BADFLAGS          WSAEINVAL
#define EAI_FAIL              WSANO_RECOVERY
#define EAI_FAMILY            WSAEAFNOSUPPORT
#define EAI_MEMORY            WSA_NOT_ENOUGH_MEMORY
#define EAI_NONAME            WSAHOST_NOT_FOUND
#define EAI_SERVICE           WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE          WSAESOCKTNOSUPPORT
#define EAI_NODATA            EAI_NONAME
#else
#define WS_EAI_AGAIN          WSATRY_AGAIN
#define WS_EAI_BADFLAGS       WSAEINVAL
#define WS_EAI_FAIL           WSANO_RECOVERY
#define WS_EAI_FAMILY         WSAEAFNOSUPPORT
#define WS_EAI_MEMORY         WSA_NOT_ENOUGH_MEMORY
#define WS_EAI_NONAME         WSAHOST_NOT_FOUND
#define WS_EAI_SERVICE        WSATYPE_NOT_FOUND
#define WS_EAI_SOCKTYPE       WSAESOCKTNOSUPPORT
#define WS_EAI_NODATA         WS_EAI_NONAME
#endif

#ifndef USE_WS_PREFIX
#define AI_PASSIVE            0x1
#define AI_CANONNAME          0x2
#define AI_NUMERICHOST        0x4
#else
#define WS_AI_PASSIVE         0x1
#define WS_AI_CANONNAME       0x2
#define WS_AI_NUMERICHOST     0x4
#endif

typedef struct WS(addrinfo)
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char *ai_canonname;
    struct WS(sockaddr) *ai_addr;
    struct WS(addrinfo) *ai_next;
} ADDRINFO,*LPADDRINFO;

#ifndef USE_WS_PREFIX
#define NI_MAXHOST            1025
#define NI_MAXSERV            32
#else
#define WS_NI_MAXHOST         1025
#define WS_NI_MAXSERV         32
#endif

#ifndef USE_WS_PREFIX
#define NI_NOFQDN             0x01
#define NI_NUMERICHOST        0x02
#define NI_NAMEREQD           0x04
#define NI_NUMERICSERV        0x08
#define NI_DGRAM              0x10
#else
#define WS_NI_NOFQDN          0x01
#define WS_NI_NUMERICHOST     0x02
#define WS_NI_NAMEREQD        0x04
#define WS_NI_NUMERICSERV     0x08
#define WS_NI_DGRAM           0x10
#endif

typedef int WS(socklen_t);

int WINAPI WS(getaddrinfo)(const char*,const char*,const struct WS(addrinfo)*,struct WS(addrinfo)**);
void WINAPI WS(freeaddrinfo)(struct WS(addrinfo)*);
int WINAPI WS(getnameinfo)(const struct WS(sockaddr)*,socklen_t,char*,DWORD,char*,DWORD,int);

#if WS_API_TYPEDEFS
typedef int (WINAPI *LPFN_GETADDRINFO)(const char*,const char*,const struct WS(addrinfo)*,struct WS(addrinfo)**);
typedef void (WINAPI *LPFN_FREEADDRINFO)(struct WS(addrinfo)*);
typedef int (WINAPI *LPFN_GETNAMEINFO)(const struct WS(sockaddr)*,socklen_t,char*,DWORD,char*,DWORD,int);
#endif

#endif /* __WS2TCPIP__ */
