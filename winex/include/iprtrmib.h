/* WINE iprtrmib.h
 * Copyright (C) 2003 Juan Lang
 */
#ifndef WINE_IPRTRMIB_H__
#define WINE_IPRTRMIB_H__

#define MAX_INTERFACE_NAME_LEN 256

#include <ipifcons.h>

#define MAXLEN_IFDESCR 256
#define MAXLEN_PHYSADDR 8

typedef struct _MIB_IFROW
{
    WCHAR wszName[MAX_INTERFACE_NAME_LEN];
    DWORD dwIndex;
    DWORD dwType;
    DWORD dwMtu;
    DWORD dwSpeed;
    DWORD dwPhysAddrLen;
    BYTE  bPhysAddr[MAXLEN_PHYSADDR];
    DWORD dwAdminStatus;
    DWORD dwOperStatus;
    DWORD dwLastChange;
    DWORD dwInOctets;
    DWORD dwInUcastPkts;
    DWORD dwInNUcastPkts;
    DWORD dwInDiscards;
    DWORD dwInErrors;
    DWORD dwInUnknownProtos;
    DWORD dwOutOctets;
    DWORD dwOutUcastPkts;
    DWORD dwOutNUcastPkts;
    DWORD dwOutDiscards;
    DWORD dwOutErrors;
    DWORD dwOutQLen;
    DWORD dwDescrLen;
    BYTE  bDescr[MAXLEN_IFDESCR];
} MIB_IFROW,*PMIB_IFROW;

typedef struct _MIB_IFTABLE
{
    DWORD     dwNumEntries;
    MIB_IFROW table[1];
} MIB_IFTABLE, *PMIB_IFTABLE;

typedef struct _MIBICMPSTATS
{
    DWORD dwMsgs;
    DWORD dwErrors;
    DWORD dwDestUnreachs;
    DWORD dwTimeExcds;
    DWORD dwParmProbs;
    DWORD dwSrcQuenchs;
    DWORD dwRedirects;
    DWORD dwEchos;
    DWORD dwEchoReps;
    DWORD dwTimestamps;
    DWORD dwTimestampReps;
    DWORD dwAddrMasks;
    DWORD dwAddrMaskReps;
} MIBICMPSTATS;

typedef    struct _MIBICMPINFO
{
    MIBICMPSTATS icmpInStats;
    MIBICMPSTATS icmpOutStats;
} MIBICMPINFO;

typedef struct _MIB_ICMP
{
    MIBICMPINFO stats;
} MIB_ICMP,*PMIB_ICMP;

typedef struct _MIB_UDPSTATS
{
    DWORD dwInDatagrams;
    DWORD dwNoPorts;
    DWORD dwInErrors;
    DWORD dwOutDatagrams;
    DWORD dwNumAddrs;
} MIB_UDPSTATS,*PMIB_UDPSTATS;

typedef struct _MIB_UDPROW
{
    DWORD dwLocalAddr;
    DWORD dwLocalPort;
} MIB_UDPROW, *PMIB_UDPROW;

typedef struct _MIB_UDPTABLE
{
    DWORD      dwNumEntries;
    MIB_UDPROW table[1];
} MIB_UDPTABLE, *PMIB_UDPTABLE;

typedef struct _MIB_TCPSTATS
{
    DWORD dwRtoAlgorithm;
    DWORD dwRtoMin;
    DWORD dwRtoMax;
    DWORD dwMaxConn;
    DWORD dwActiveOpens;
    DWORD dwPassiveOpens;
    DWORD dwAttemptFails;
    DWORD dwEstabResets;
    DWORD dwCurrEstab;
    DWORD dwInSegs;
    DWORD dwOutSegs;
    DWORD dwRetransSegs;
    DWORD dwInErrs;
    DWORD dwOutRsts;
    DWORD dwNumConns;
} MIB_TCPSTATS, *PMIB_TCPSTATS;

typedef struct _MIB_TCPROW
{
    DWORD dwState;
    DWORD dwLocalAddr;
    DWORD dwLocalPort;
    DWORD dwRemoteAddr;
    DWORD dwRemotePort;
} MIB_TCPROW, *PMIB_TCPROW;

#define MIB_TCP_STATE_CLOSED            1
#define MIB_TCP_STATE_LISTEN            2
#define MIB_TCP_STATE_SYN_SENT          3
#define MIB_TCP_STATE_SYN_RCVD          4
#define MIB_TCP_STATE_ESTAB             5
#define MIB_TCP_STATE_FIN_WAIT1         6
#define MIB_TCP_STATE_FIN_WAIT2         7
#define MIB_TCP_STATE_CLOSE_WAIT        8
#define MIB_TCP_STATE_CLOSING           9
#define MIB_TCP_STATE_LAST_ACK         10
#define MIB_TCP_STATE_TIME_WAIT        11
#define MIB_TCP_STATE_DELETE_TCB       12

typedef struct _MIB_TCPTABLE
{
    DWORD      dwNumEntries;
    MIB_TCPROW table[1];
} MIB_TCPTABLE, *PMIB_TCPTABLE;

typedef struct _MIB_IPSTATS
{
    DWORD dwForwarding;
    DWORD dwDefaultTTL;
    DWORD dwInReceives;
    DWORD dwInHdrErrors;
    DWORD dwInAddrErrors;
    DWORD dwForwDatagrams;
    DWORD dwInUnknownProtos;
    DWORD dwInDiscards;
    DWORD dwInDelivers;
    DWORD dwOutRequests;
    DWORD dwRoutingDiscards;
    DWORD dwOutDiscards;
    DWORD dwOutNoRoutes;
    DWORD dwReasmTimeout;
    DWORD dwReasmReqds;
    DWORD dwReasmOks;
    DWORD dwReasmFails;
    DWORD dwFragOks;
    DWORD dwFragFails;
    DWORD dwFragCreates;
    DWORD dwNumIf;
    DWORD dwNumAddr;
    DWORD dwNumRoutes;
} MIB_IPSTATS, *PMIB_IPSTATS;

typedef struct _MIB_IPADDRROW
{
    DWORD        dwAddr;
    DWORD        dwIndex;
    DWORD        dwMask;
    DWORD        dwBCastAddr;
    DWORD        dwReasmSize;
    unsigned short    unused1;
    unsigned short    wType;
} MIB_IPADDRROW, *PMIB_IPADDRROW;

typedef struct _MIB_IPADDRTABLE
{
    DWORD         dwNumEntries;
    MIB_IPADDRROW table[1];
} MIB_IPADDRTABLE, *PMIB_IPADDRTABLE;


typedef struct _MIB_IPFORWARDNUMBER
{
    DWORD      dwValue;
}MIB_IPFORWARDNUMBER,*PMIB_IPFORWARDNUMBER;

typedef struct _MIB_IPFORWARDROW
{
    DWORD dwForwardDest;
    DWORD dwForwardMask;
    DWORD dwForwardPolicy;
    DWORD dwForwardNextHop;
    DWORD dwForwardIfIndex;
    DWORD dwForwardType;
    DWORD dwForwardProto;
    DWORD dwForwardAge;
    DWORD dwForwardNextHopAS;
    DWORD dwForwardMetric1;
    DWORD dwForwardMetric2;
    DWORD dwForwardMetric3;
    DWORD dwForwardMetric4;
    DWORD dwForwardMetric5;
}MIB_IPFORWARDROW, *PMIB_IPFORWARDROW;

#define MIB_IPROUTE_TYPE_OTHER      1
#define MIB_IPROUTE_TYPE_INVALID    2
#define MIB_IPROUTE_TYPE_DIRECT     3
#define MIB_IPROUTE_TYPE_INDIRECT   4

#define MIB_IPPROTO_OTHER             1
#define MIB_IPPROTO_LOCAL             2
#define MIB_IPPROTO_NETMGMT           3
#define MIB_IPPROTO_ICMP              4
#define MIB_IPPROTO_EGP               5
#define MIB_IPPROTO_GGP               6
#define MIB_IPPROTO_HELLO             7
#define MIB_IPPROTO_RIP               8
#define MIB_IPPROTO_IS_IS             9
#define MIB_IPPROTO_ES_IS             10
#define MIB_IPPROTO_CISCO             11
#define MIB_IPPROTO_BBN               12
#define MIB_IPPROTO_OSPF              13
#define MIB_IPPROTO_BGP               14

#define MIB_IPPROTO_NT_AUTOSTATIC     10002
#define MIB_IPPROTO_NT_STATIC         10006
#define MIB_IPPROTO_NT_STATIC_NON_DOD 10007

typedef struct _MIB_IPFORWARDTABLE
{
    DWORD            dwNumEntries;
    MIB_IPFORWARDROW table[1];
} MIB_IPFORWARDTABLE, *PMIB_IPFORWARDTABLE;

typedef struct _MIB_IPNETROW
{
    DWORD dwIndex;
    DWORD dwPhysAddrLen;
    BYTE  bPhysAddr[MAXLEN_PHYSADDR];
    DWORD dwAddr;
    DWORD dwType;
} MIB_IPNETROW, *PMIB_IPNETROW;

#define    MIB_IPNET_TYPE_OTHER        1
#define    MIB_IPNET_TYPE_INVALID        2
#define    MIB_IPNET_TYPE_DYNAMIC        3
#define    MIB_IPNET_TYPE_STATIC        4

typedef struct _MIB_IPNETTABLE
{
    DWORD        dwNumEntries;
    MIB_IPNETROW table[1];
} MIB_IPNETTABLE, *PMIB_IPNETTABLE;

#endif /* WINE_IPRTRMIB_H__ */
