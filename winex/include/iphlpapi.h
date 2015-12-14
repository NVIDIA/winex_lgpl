/* WINE iphlpapi.h
 * Copyright (C) 2003 Juan Lang
 */
#ifndef WINE_IPHLPAPI_H__
#define WINE_IPHLPAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <iprtrmib.h>
#include <ipexport.h>
#include <iptypes.h>

DWORD WINAPI GetNumberOfInterfaces(PDWORD pdwNumIf);

DWORD WINAPI GetIfEntry(PMIB_IFROW pIfRow);

DWORD WINAPI GetIfTable(PMIB_IFTABLE pIfTable, PULONG pdwSize, BOOL bOrder);

DWORD WINAPI GetIpAddrTable(PMIB_IPADDRTABLE pIpAddrTable, PULONG pdwSize,
 BOOL bOrder);

DWORD WINAPI GetIpNetTable(PMIB_IPNETTABLE pIpNetTable, PULONG pdwSize,
 BOOL bOrder);

DWORD WINAPI GetIpForwardTable(PMIB_IPFORWARDTABLE pIpForwardTable,
 PULONG pdwSize, BOOL bOrder);

DWORD WINAPI GetTcpTable(PMIB_TCPTABLE pTcpTable, PDWORD pdwSize, BOOL bOrder);

DWORD WINAPI GetExtendedTcpTable(PVOID pTcpTable, PDWORD pdwSize, BOOL bOrder, ULONG ulAf, TCP_TABLE_CLASS TableClass, ULONG Reserved);

DWORD WINAPI GetUdpTable(PMIB_UDPTABLE pUdpTable, PDWORD pdwSize, BOOL bOrder);

DWORD WINAPI GetExtendedUdpTable(PVOID pUdpTable, PDWORD pdwSize, BOOL bOrder, ULONG ulAf, UDP_TABLE_CLASS TableClass, ULONG Reserved);

DWORD WINAPI GetIpStatistics(PMIB_IPSTATS pStats);

DWORD WINAPI GetIpStatisticsEx(PMIB_IPSTATS pStats, DWORD dwFamily);

DWORD WINAPI GetIcmpStatistics(PMIB_ICMP pStats);

DWORD WINAPI GetTcpStatistics(PMIB_TCPSTATS pStats);

DWORD WINAPI GetTcpStatisticsEx(PMIB_TCPSTATS pStats, DWORD dwFamily);

DWORD WINAPI GetUdpStatistics(PMIB_UDPSTATS pStats);

DWORD WINAPI GetUdpStatisticsEx(PMIB_UDPSTATS pStats, DWORD dwFamily);

DWORD WINAPI SetIfEntry(PMIB_IFROW pIfRow);

DWORD WINAPI CreateIpForwardEntry(PMIB_IPFORWARDROW pRoute);

DWORD WINAPI SetIpForwardEntry(PMIB_IPFORWARDROW pRoute);

DWORD WINAPI DeleteIpForwardEntry(PMIB_IPFORWARDROW pRoute);

DWORD WINAPI SetIpStatistics(PMIB_IPSTATS pIpStats);

DWORD WINAPI SetIpTTL(UINT nTTL);

DWORD WINAPI CreateIpNetEntry(PMIB_IPNETROW pArpEntry);

DWORD WINAPI SetIpNetEntry(PMIB_IPNETROW pArpEntry);

DWORD WINAPI DeleteIpNetEntry(PMIB_IPNETROW pArpEntry);

DWORD WINAPI FlushIpNetTable(DWORD dwIfIndex);

DWORD WINAPI CreateProxyArpEntry(DWORD dwAddress, DWORD dwMask,
 DWORD dwIfIndex);

DWORD WINAPI DeleteProxyArpEntry(DWORD dwAddress, DWORD dwMask,
 DWORD dwIfIndex);

DWORD WINAPI SetTcpEntry(PMIB_TCPROW pTcpRow);

DWORD WINAPI GetInterfaceInfo(PIP_INTERFACE_INFO pIfTable, PULONG dwOutBufLen);

DWORD WINAPI GetUniDirectionalAdapterInfo(
 PIP_UNIDIRECTIONAL_ADAPTER_ADDRESS pIPIfInfo, PULONG dwOutBufLen);

DWORD WINAPI GetBestInterface(IPAddr dwDestAddr, PDWORD pdwBestIfIndex);

DWORD WINAPI GetBestInterfaceEx(
#ifdef USE_WS_PREFIX
    struct WS_sockaddr *pDestAddr,
#else
    struct sockaddr *   pDestAddr,
#endif
    PDWORD              pdwBestIfIndex);

DWORD WINAPI GetBestRoute(DWORD dwDestAddr, DWORD dwSourceAddr,
 PMIB_IPFORWARDROW   pBestRoute);

DWORD WINAPI NotifyAddrChange(PHANDLE Handle, LPOVERLAPPED overlapped);

DWORD WINAPI NotifyRouteChange(PHANDLE Handle, LPOVERLAPPED overlapped);

DWORD WINAPI GetAdapterIndex(IN LPWSTR AdapterName, OUT PULONG IfIndex);

DWORD WINAPI AddIPAddress(IPAddr Address, IPMask IpMask, DWORD IfIndex,
 PULONG NTEContext, PULONG NTEInstance);

DWORD WINAPI DeleteIPAddress(ULONG NTEContext);

DWORD WINAPI GetNetworkParams(PFIXED_INFO pFixedInfo, PULONG pOutBufLen);

DWORD WINAPI GetAdaptersInfo(PIP_ADAPTER_INFO pAdapterInfo, PULONG pOutBufLen);

#ifdef _WINSOCK2API_

/*
 The following functions require Winsock2.
*/

ULONG WINAPI GetAdaptersAddresses(ULONG family, ULONG flags, PVOID reserved,
                                  PIP_ADAPTER_ADDRESSES aa, PULONG buflen);

#endif

DWORD WINAPI GetPerAdapterInfo(ULONG IfIndex,
 PIP_PER_ADAPTER_INFO pPerAdapterInfo, PULONG pOutBufLen);

DWORD WINAPI IpReleaseAddress(PIP_ADAPTER_INDEX_MAP AdapterInfo);

DWORD WINAPI IpRenewAddress(PIP_ADAPTER_INDEX_MAP AdapterInfo);

DWORD WINAPI SendARP(IPAddr DestIP, IPAddr SrcIP, PULONG pMacAddr,
 PULONG  PhyAddrLen);

BOOL WINAPI GetRTTAndHopCount(IPAddr DestIpAddress, PULONG HopCount,
 ULONG  MaxHops, PULONG RTT);

DWORD WINAPI GetFriendlyIfIndex(DWORD IfIndex);

DWORD WINAPI EnableRouter(HANDLE* pHandle, OVERLAPPED* pOverlapped);

DWORD WINAPI UnenableRouter(OVERLAPPED* pOverlapped, LPDWORD lpdwEnableCount);

#ifdef __cplusplus
}
#endif

#endif /* WINE_IPHLPAPI_H__ */
