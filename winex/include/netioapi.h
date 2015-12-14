#ifndef _NETIOAPI_H_
#define _NETIOAPI_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#endif

#ifndef ANY_SIZE
#define ANY_SIZE 1
#endif

#ifdef __IPHLPAPI_H__

#define NETIO_STATUS DWORD
#define NETIO_SUCCESS(x) ((x) == NO_ERROR)
#define NETIOAPI_API_ WINAPI

#else

#include <ws2def.h>
#include <ws2ipdef.h>
#include <ifdef.h>
#include <nldef.h>

#define NETIO_STATUS NTSTATUS
#define NETIO_SUCCESS(x) NT_SUCCESS(x)
#define NETIOAPI_API_ NTAPI

#endif

#define NETIOAPI_API NETIO_STATUS NETIOAPI_API_

typedef enum _MIB_NOTIFICATION_TYPE {
    MibParameterNotification,
    MibAddInstance,
    MibDeleteInstance,
    MibInitialNotification,
} MIB_NOTIFICATION_TYPE, *PMIB_NOTIFICATION_TYPE;


#ifdef _WS2IPDEF_ 
#include <ntddndis.h>

typedef struct _MIB_IF_ROW2 {
    NET_LUID InterfaceLuid;
    NET_IFINDEX InterfaceIndex; 

    GUID InterfaceGuid;
    WCHAR Alias[IF_MAX_STRING_SIZE + 1]; 
    WCHAR Description[IF_MAX_STRING_SIZE + 1];
    ULONG PhysicalAddressLength;
    UCHAR PhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];
    UCHAR PermanentPhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];    

    ULONG Mtu;
    IFTYPE Type;                
    TUNNEL_TYPE TunnelType;     
    NDIS_MEDIUM MediaType; 
    NDIS_PHYSICAL_MEDIUM PhysicalMediumType; 
    NET_IF_ACCESS_TYPE AccessType;
    NET_IF_DIRECTION_TYPE DirectionType;
    struct {
        BOOLEAN HardwareInterface : 1;
        BOOLEAN FilterInterface : 1;
        BOOLEAN ConnectorPresent : 1;
        BOOLEAN NotAuthenticated : 1;
        BOOLEAN NotMediaConnected : 1;
        BOOLEAN Paused : 1;
        BOOLEAN LowPower : 1;
        BOOLEAN EndPointInterface : 1;
    } InterfaceAndOperStatusFlags;

    IF_OPER_STATUS OperStatus;  
    NET_IF_ADMIN_STATUS AdminStatus;
    NET_IF_MEDIA_CONNECT_STATE MediaConnectState;
    NET_IF_NETWORK_GUID NetworkGuid;
    NET_IF_CONNECTION_TYPE ConnectionType; 

    ULONG64 TransmitLinkSpeed;
    ULONG64 ReceiveLinkSpeed;

    ULONG64 InOctets;
    ULONG64 InUcastPkts;
    ULONG64 InNUcastPkts;
    ULONG64 InDiscards;
    ULONG64 InErrors;
    ULONG64 InUnknownProtos;
    ULONG64 InUcastOctets;      
    ULONG64 InMulticastOctets;  
    ULONG64 InBroadcastOctets; 
    ULONG64 OutOctets;
    ULONG64 OutUcastPkts;
    ULONG64 OutNUcastPkts;
    ULONG64 OutDiscards;
    ULONG64 OutErrors;
    ULONG64 OutUcastOctets;     
    ULONG64 OutMulticastOctets; 
    ULONG64 OutBroadcastOctets;   
    ULONG64 OutQLen; 
} MIB_IF_ROW2, *PMIB_IF_ROW2;

typedef struct _MIB_IF_TABLE2 {
    ULONG NumEntries;
    MIB_IF_ROW2 Table[ANY_SIZE];
} MIB_IF_TABLE2, *PMIB_IF_TABLE2;

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIfEntry2(
    __inout PMIB_IF_ROW2 Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIfTable2(
    __deref_out PMIB_IF_TABLE2 *Table
    );

typedef enum _MIB_IF_TABLE_LEVEL {
    MibIfTableNormal,
    MibIfTableRaw
} MIB_IF_TABLE_LEVEL, *PMIB_IF_TABLE_LEVEL;

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIfTable2Ex(
    __in MIB_IF_TABLE_LEVEL Level,
    __deref_out PMIB_IF_TABLE2 *Table
    );


typedef struct _MIB_IPINTERFACE_ROW {
    ADDRESS_FAMILY Family;
    NET_LUID InterfaceLuid;
    NET_IFINDEX InterfaceIndex;

    ULONG MaxReassemblySize;
    ULONG64 InterfaceIdentifier;
    ULONG MinRouterAdvertisementInterval;
    ULONG MaxRouterAdvertisementInterval;

    BOOLEAN AdvertisingEnabled;
    BOOLEAN ForwardingEnabled;
    BOOLEAN WeakHostSend;
    BOOLEAN WeakHostReceive;
    BOOLEAN UseAutomaticMetric;
    BOOLEAN UseNeighborUnreachabilityDetection;   
    BOOLEAN ManagedAddressConfigurationSupported;
    BOOLEAN OtherStatefulConfigurationSupported;
    BOOLEAN AdvertiseDefaultRoute;
    NL_ROUTER_DISCOVERY_BEHAVIOR RouterDiscoveryBehavior;
    ULONG DadTransmits;         
    ULONG BaseReachableTime;
    ULONG RetransmitTime;
    ULONG PathMtuDiscoveryTimeout; 
    NL_LINK_LOCAL_ADDRESS_BEHAVIOR LinkLocalAddressBehavior;
    ULONG LinkLocalAddressTimeout; 
    ULONG ZoneIndices[ScopeLevelCount]; 
    ULONG SitePrefixLength;
    ULONG Metric;
    ULONG NlMtu;    

    BOOLEAN Connected;
    BOOLEAN SupportsWakeUpPatterns;   
    BOOLEAN SupportsNeighborDiscovery;
    BOOLEAN SupportsRouterDiscovery;
    ULONG ReachableTime;

    NL_INTERFACE_OFFLOAD_ROD TransmitOffload;
    NL_INTERFACE_OFFLOAD_ROD ReceiveOffload; 

    BOOLEAN DisableDefaultRoutes;
} MIB_IPINTERFACE_ROW, *PMIB_IPINTERFACE_ROW;

typedef struct _MIB_IPINTERFACE_TABLE {
    ULONG NumEntries;
    MIB_IPINTERFACE_ROW Table[ANY_SIZE];
} MIB_IPINTERFACE_TABLE, *PMIB_IPINTERFACE_TABLE;

typedef struct _MIB_IFSTACK_ROW {
    NET_IFINDEX HigherLayerInterfaceIndex;
    NET_IFINDEX LowerLayerInterfaceIndex;
} MIB_IFSTACK_ROW, *PMIB_IFSTACK_ROW;

typedef struct _MIB_INVERTEDIFSTACK_ROW {
    NET_IFINDEX LowerLayerInterfaceIndex;
    NET_IFINDEX HigherLayerInterfaceIndex;
} MIB_INVERTEDIFSTACK_ROW, *PMIB_INVERTEDIFSTACK_ROW;

typedef struct _MIB_IFSTACK_TABLE {
    ULONG NumEntries;
    MIB_IFSTACK_ROW Table[ANY_SIZE];
} MIB_IFSTACK_TABLE, *PMIB_IFSTACK_TABLE;

typedef struct _MIB_INVERTEDIFSTACK_TABLE {
    ULONG NumEntries;
    MIB_INVERTEDIFSTACK_ROW Table[ANY_SIZE];
} MIB_INVERTEDIFSTACK_TABLE, *PMIB_INVERTEDIFSTACK_TABLE;

typedef
VOID
(NETIOAPI_API_ *PIPINTERFACE_CHANGE_CALLBACK) (
    __in PVOID CallerContext,
    __in PMIB_IPINTERFACE_ROW Row OPTIONAL,
    __in MIB_NOTIFICATION_TYPE NotificationType
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIfStackTable(
    __deref_out PMIB_IFSTACK_TABLE *Table
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetInvertedIfStackTable(
    __deref_out PMIB_INVERTEDIFSTACK_TABLE *Table
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpInterfaceEntry(
    __inout PMIB_IPINTERFACE_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpInterfaceTable(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_IPINTERFACE_TABLE *Table
    );

VOID
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
InitializeIpInterfaceEntry(
    __inout PMIB_IPINTERFACE_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
NotifyIpInterfaceChange(
    __in ADDRESS_FAMILY Family,
    __in PIPINTERFACE_CHANGE_CALLBACK Callback,
    __in PVOID CallerContext,    
    __in BOOLEAN InitialNotification,
    __inout OUT HANDLE *NotificationHandle
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
SetIpInterfaceEntry(
    __inout PMIB_IPINTERFACE_ROW Row
    );


typedef struct _MIB_UNICASTIPADDRESS_ROW {
    SOCKADDR_INET Address;
    NET_LUID InterfaceLuid;
    NET_IFINDEX InterfaceIndex;

    NL_PREFIX_ORIGIN PrefixOrigin;
    NL_SUFFIX_ORIGIN SuffixOrigin;    
    ULONG ValidLifetime;
    ULONG PreferredLifetime;
    UINT8 OnLinkPrefixLength;
    BOOLEAN SkipAsSource;

    NL_DAD_STATE DadState;
    SCOPE_ID ScopeId;
    LARGE_INTEGER CreationTimeStamp;    
} MIB_UNICASTIPADDRESS_ROW, *PMIB_UNICASTIPADDRESS_ROW;

typedef struct _MIB_UNICASTIPADDRESS_TABLE {
    ULONG NumEntries;
    MIB_UNICASTIPADDRESS_ROW Table[ANY_SIZE];
} MIB_UNICASTIPADDRESS_TABLE, *PMIB_UNICASTIPADDRESS_TABLE;

typedef
VOID
(NETIOAPI_API_ *PUNICAST_IPADDRESS_CHANGE_CALLBACK) (
    __in PVOID CallerContext,
    __in_opt PMIB_UNICASTIPADDRESS_ROW Row,
    __in MIB_NOTIFICATION_TYPE NotificationType
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)    
NETIOAPI_API
CreateUnicastIpAddressEntry(
    __in CONST MIB_UNICASTIPADDRESS_ROW *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
DeleteUnicastIpAddressEntry(
    __in CONST MIB_UNICASTIPADDRESS_ROW *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetUnicastIpAddressEntry(
    __inout PMIB_UNICASTIPADDRESS_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetUnicastIpAddressTable(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_UNICASTIPADDRESS_TABLE *Table
    );

VOID
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
InitializeUnicastIpAddressEntry(
    __out PMIB_UNICASTIPADDRESS_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
NotifyUnicastIpAddressChange(
    __in ADDRESS_FAMILY Family,
    __in PUNICAST_IPADDRESS_CHANGE_CALLBACK Callback,
    __in PVOID CallerContext,    
    __in BOOLEAN InitialNotification,
    __inout HANDLE *NotificationHandle
    );

typedef
VOID
(NETIOAPI_API_ *PSTABLE_UNICAST_IPADDRESS_TABLE_CALLBACK) (
    __in PVOID CallerContext,
    __in PMIB_UNICASTIPADDRESS_TABLE AddressTable
    );
    
__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
NotifyStableUnicastIpAddressTable(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_UNICASTIPADDRESS_TABLE* Table,
    __in PSTABLE_UNICAST_IPADDRESS_TABLE_CALLBACK CallerCallback,
    __in PVOID CallerContext,
    __inout HANDLE *NotificationHandle
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
SetUnicastIpAddressEntry(
    __in CONST MIB_UNICASTIPADDRESS_ROW *Row
    );

typedef struct _MIB_ANYCASTIPADDRESS_ROW {
    SOCKADDR_INET Address;
    NET_LUID InterfaceLuid;
    NET_IFINDEX InterfaceIndex;

    SCOPE_ID ScopeId;
} MIB_ANYCASTIPADDRESS_ROW, *PMIB_ANYCASTIPADDRESS_ROW; 

typedef struct _MIB_ANYCASTIPADDRESS_TABLE {
    ULONG NumEntries;
    MIB_ANYCASTIPADDRESS_ROW Table[ANY_SIZE];
} MIB_ANYCASTIPADDRESS_TABLE, *PMIB_ANYCASTIPADDRESS_TABLE;
    
__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
CreateAnycastIpAddressEntry(
    __in CONST MIB_ANYCASTIPADDRESS_ROW *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
DeleteAnycastIpAddressEntry(
    __in CONST MIB_ANYCASTIPADDRESS_ROW *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetAnycastIpAddressEntry(
    __inout PMIB_ANYCASTIPADDRESS_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetAnycastIpAddressTable(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_ANYCASTIPADDRESS_TABLE *Table
    );


typedef struct _MIB_MULTICASTIPADDRESS_ROW {
    SOCKADDR_INET Address;
    NET_IFINDEX InterfaceIndex;
    NET_LUID InterfaceLuid;

    SCOPE_ID ScopeId;
} MIB_MULTICASTIPADDRESS_ROW, *PMIB_MULTICASTIPADDRESS_ROW;

typedef struct _MIB_MULTICASTIPADDRESS_TABLE {
    ULONG NumEntries;
    MIB_MULTICASTIPADDRESS_ROW Table[ANY_SIZE];
} MIB_MULTICASTIPADDRESS_TABLE, *PMIB_MULTICASTIPADDRESS_TABLE;    

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetMulticastIpAddressEntry(
    __inout PMIB_MULTICASTIPADDRESS_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetMulticastIpAddressTable(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_MULTICASTIPADDRESS_TABLE *Table
    );


typedef struct _IP_ADDRESS_PREFIX {
    SOCKADDR_INET Prefix;
    UINT8 PrefixLength;
} IP_ADDRESS_PREFIX, *PIP_ADDRESS_PREFIX;    

typedef struct _MIB_IPFORWARD_ROW2 {
    NET_LUID InterfaceLuid;
    NET_IFINDEX InterfaceIndex;
    IP_ADDRESS_PREFIX DestinationPrefix;
    SOCKADDR_INET NextHop;

    UCHAR SitePrefixLength;
    ULONG ValidLifetime;
    ULONG PreferredLifetime;
    ULONG Metric;
    NL_ROUTE_PROTOCOL Protocol;

    BOOLEAN Loopback;
    BOOLEAN AutoconfigureAddress;
    BOOLEAN Publish;
    BOOLEAN Immortal;

    ULONG Age;
    NL_ROUTE_ORIGIN Origin;
} MIB_IPFORWARD_ROW2, *PMIB_IPFORWARD_ROW2;  

typedef struct _MIB_IPFORWARD_TABLE2 {
    ULONG NumEntries;
    MIB_IPFORWARD_ROW2 Table[ANY_SIZE];
} MIB_IPFORWARD_TABLE2, *PMIB_IPFORWARD_TABLE2;

typedef
VOID
(NETIOAPI_API_ *PIPFORWARD_CHANGE_CALLBACK) (
    __in PVOID CallerContext,
    __in_opt PMIB_IPFORWARD_ROW2 Row,
    __in MIB_NOTIFICATION_TYPE NotificationType
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
CreateIpForwardEntry2(
    __in CONST MIB_IPFORWARD_ROW2 *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
DeleteIpForwardEntry2(
    __in CONST MIB_IPFORWARD_ROW2 *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetBestRoute2(
    __in_opt NET_LUID *InterfaceLuid,
    __in NET_IFINDEX InterfaceIndex,
    __in_opt CONST SOCKADDR_INET *SourceAddress,
    __in CONST SOCKADDR_INET *DestinationAddress,
    __in ULONG AddressSortOptions,
    __out PMIB_IPFORWARD_ROW2 BestRoute,
    __out SOCKADDR_INET *BestSourceAddress
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpForwardEntry2(
    __inout PMIB_IPFORWARD_ROW2 Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpForwardTable2(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_IPFORWARD_TABLE2 *Table
    );

VOID
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
InitializeIpForwardEntry(
    __out PMIB_IPFORWARD_ROW2 Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
NotifyRouteChange2(
    __in ADDRESS_FAMILY AddressFamily,
    __in PIPFORWARD_CHANGE_CALLBACK Callback,
    __in PVOID CallerContext,    
    __in BOOLEAN InitialNotification,
    __inout HANDLE *NotificationHandle
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
SetIpForwardEntry2(
    __in CONST MIB_IPFORWARD_ROW2 *Route
    );


typedef struct _MIB_IPPATH_ROW {
    SOCKADDR_INET Source;    
    SOCKADDR_INET Destination;
    NET_LUID InterfaceLuid;
    NET_IFINDEX InterfaceIndex;  

    SOCKADDR_INET CurrentNextHop;

    ULONG PathMtu;

    ULONG RttMean;

    ULONG RttDeviation;
    union {
        ULONG LastReachable;    
        ULONG LastUnreachable;  
    };
    BOOLEAN IsReachable;

    ULONG64 LinkTransmitSpeed;
    ULONG64 LinkReceiveSpeed;
} MIB_IPPATH_ROW, *PMIB_IPPATH_ROW;

typedef struct _MIB_IPPATH_TABLE {
    ULONG NumEntries;
    MIB_IPPATH_ROW Table[ANY_SIZE];
} MIB_IPPATH_TABLE, *PMIB_IPPATH_TABLE;


__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
FlushIpPathTable(
    __in ADDRESS_FAMILY Family
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpPathEntry(
    __inout PMIB_IPPATH_ROW Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpPathTable(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_IPPATH_TABLE *Table
    );


typedef struct _MIB_IPNET_ROW2 {
    SOCKADDR_INET Address;
    NET_IFINDEX InterfaceIndex;
    NET_LUID InterfaceLuid;

    UCHAR PhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];

    ULONG PhysicalAddressLength;
    NL_NEIGHBOR_STATE State;

    union {
        struct {
            BOOLEAN IsRouter : 1;
            BOOLEAN IsUnreachable : 1;
        };
        UCHAR Flags;
    };

    union {
        ULONG LastReachable;
        ULONG LastUnreachable;
    } ReachabilityTime;
} MIB_IPNET_ROW2, *PMIB_IPNET_ROW2;

typedef struct _MIB_IPNET_TABLE2 {
    ULONG NumEntries;
    MIB_IPNET_ROW2 Table[ANY_SIZE];
} MIB_IPNET_TABLE2, *PMIB_IPNET_TABLE2;

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
CreateIpNetEntry2(
    __in CONST MIB_IPNET_ROW2 *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
DeleteIpNetEntry2(
    __in CONST MIB_IPNET_ROW2 *Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
FlushIpNetTable2(
    __in ADDRESS_FAMILY Family,
    __in NET_IFINDEX InterfaceIndex
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpNetEntry2(
    __inout PMIB_IPNET_ROW2 Row
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetIpNetTable2(
    __in ADDRESS_FAMILY Family,
    __deref_out PMIB_IPNET_TABLE2 *Table
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ResolveIpNetEntry2(
    __inout PMIB_IPNET_ROW2 Row,
    __in_opt CONST SOCKADDR_INET *SourceAddress
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
SetIpNetEntry2(
    __in PMIB_IPNET_ROW2 Row
    );

#define MIB_INVALID_TEREDO_PORT_NUMBER 0

typedef
VOID
(NETIOAPI_API_ *PTEREDO_PORT_CHANGE_CALLBACK) (
    __in PVOID CallerContext,
    __in USHORT Port,
    __inout MIB_NOTIFICATION_TYPE NotificationType
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
NotifyTeredoPortChange(
    __in PTEREDO_PORT_CHANGE_CALLBACK Callback,
    __in PVOID CallerContext,    
    __in BOOLEAN InitialNotification,
    __inout HANDLE *NotificationHandle
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
GetTeredoPort(
    __out USHORT *Port
    );

#ifndef TEREDO_API_NO_DEPRECATE
#ifdef _MSC_VER
#pragma deprecated(NotifyTeredoPortChange)
#pragma deprecated(GetTeredoPort)
#endif
#endif 



__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API
CancelMibChangeNotify2(
    __in HANDLE NotificationHandle
    );

VOID
NETIOAPI_API_
FreeMibTable(
    __in PVOID Memory
    ); 

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
CreateSortedAddressPairs(
    __in_opt const PSOCKADDR_IN6 SourceAddressList,
    __in ULONG SourceAddressCount,
    __in const PSOCKADDR_IN6 DestinationAddressList,
    __in ULONG DestinationAddressCount,
    __in ULONG AddressSortOptions,
    __in PSOCKADDR_IN6_PAIR *SortedAddressPairList,
    __out ULONG *SortedAddressPairCount
    );

#endif 

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceNameToLuidA(
    __in CONST CHAR *InterfaceName,
    __out NET_LUID *InterfaceLuid
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceNameToLuidW(
    __in CONST WCHAR *InterfaceName,
    __out NET_LUID *InterfaceLuid
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceLuidToNameA(
    __in CONST NET_LUID *InterfaceLuid,
    __out_ecount(Length) PSTR InterfaceName,
    __in SIZE_T Length
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceLuidToNameW(
    __in CONST NET_LUID *InterfaceLuid,
    __out_ecount(Length) PWSTR InterfaceName,
    __in SIZE_T Length
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceLuidToIndex(
    __in CONST NET_LUID *InterfaceLuid,
    __out PNET_IFINDEX InterfaceIndex
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceIndexToLuid(
    __in NET_IFINDEX InterfaceIndex,
    __out PNET_LUID InterfaceLuid
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceLuidToAlias(
    __in CONST NET_LUID *InterfaceLuid,
    __out_ecount(Length) PWSTR InterfaceAlias,
    __in SIZE_T Length
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceAliasToLuid(
    IN CONST WCHAR *InterfaceAlias,
    OUT PNET_LUID InterfaceLuid
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceLuidToGuid(
    __in CONST NET_LUID *InterfaceLuid,
    __out GUID *InterfaceGuid
    );

__drv_maxIRQL(PASSIVE_LEVEL)
__success(return==STATUS_SUCCESS)
NETIOAPI_API
ConvertInterfaceGuidToLuid(
    __in CONST GUID *InterfaceGuid,
    __out PNET_LUID InterfaceLuid
    );

#define IF_NAMESIZE NDIS_IF_MAX_STRING_SIZE

NET_IFINDEX
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
if_nametoindex(
    __in PCSTR InterfaceName
    );
    
PCHAR
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
if_indextoname(
    __in NET_IFINDEX InterfaceIndex,
    __out_ecount(IF_NAMESIZE) PCHAR InterfaceName
    );

NET_IF_COMPARTMENT_ID
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
GetCurrentThreadCompartmentId(
    VOID
    );

__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API
SetCurrentThreadCompartmentId(
    __in NET_IF_COMPARTMENT_ID CompartmentId
    );

NET_IF_COMPARTMENT_ID
__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API_
GetSessionCompartmentId(
    __in ULONG SessionId
    );

__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API
SetSessionCompartmentId(
    __in ULONG SessionId,
    __in NET_IF_COMPARTMENT_ID CompartmentId
    );

__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API
GetNetworkInformation(
    __in CONST NET_IF_NETWORK_GUID *NetworkGuid,
    __out PNET_IF_COMPARTMENT_ID CompartmentId,
    __out PULONG SiteId,
    __out_ecount(Length) PWCHAR NetworkName,
    __in ULONG Length
    );

__drv_maxIRQL(PASSIVE_LEVEL)
NETIOAPI_API
SetNetworkInformation(
    __in CONST NET_IF_NETWORK_GUID *NetworkGuid,
    __in NET_IF_COMPARTMENT_ID CompartmentId,
    __in CONST WCHAR *NetworkName
    );

#ifdef _MSC_VER
#pragma warning(pop) 
#endif

NETIOAPI_API
ConvertLengthToIpv4Mask(
    __in ULONG MaskLength,
    __out PULONG Mask
    );

NETIOAPI_API
ConvertIpv4MaskToLength(
    __in ULONG Mask,
    __out PUINT8 MaskLength
    );


#ifdef __cplusplus
}
#endif

#endif 

