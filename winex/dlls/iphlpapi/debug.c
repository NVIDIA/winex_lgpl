/*
 * Copyright (c) 2014-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
 
#include "config.h"
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include "specstrings.h"
#include "winbase.h"
#define USE_WS_PREFIX
#include "winsock2.h"
#include "winternl.h"
#include "iphlpapi.h"
#include "netioapi.h"
#include "wine/debug.h"

DEFAULT_DEBUG_CHANNEL(iphlpapi);


static const char *IPHLPAPI_getAddressFamilyName(ULONG af)
{
#define GETNAME(x)  case x: return #x

    switch (af)
    {
        GETNAME(WS(AF_UNSPEC));
        GETNAME(WS(AF_UNIX));
        GETNAME(WS(AF_INET));
        GETNAME(WS(AF_IMPLINK));
        GETNAME(WS(AF_PUP));
        GETNAME(WS(AF_CHAOS));
        GETNAME(WS(AF_IPX));
        /*GETNAME(WS(AF_NS));*/
        GETNAME(WS(AF_ISO));
        /*GETNAME(WS(AF_OSI));*/
        GETNAME(WS(AF_ECMA));
        GETNAME(WS(AF_DATAKIT));
        GETNAME(WS(AF_CCITT));
        GETNAME(WS(AF_SNA));
        GETNAME(WS(AF_DECnet));
        GETNAME(WS(AF_DLI));
        GETNAME(WS(AF_LAT));
        GETNAME(WS(AF_HYLINK));
        GETNAME(WS(AF_APPLETALK));
        GETNAME(WS(AF_NETBIOS));
        GETNAME(WS(AF_VOICEVIEW));
        GETNAME(WS(AF_FIREFOX));
        GETNAME(WS(AF_UNKNOWN1));
        GETNAME(WS(AF_BAN));
        GETNAME(WS(AF_ATM));
        GETNAME(WS(AF_INET6));
        GETNAME(WS(AF_CLUSTER));
        GETNAME(WS(AF_12844));
        GETNAME(WS(AF_IRDA));
        GETNAME(WS(AF_NETDES));
        default:
            return "<unknown_family>";
    }

#undef GETNAME
}

static void IPHLPAPI_dumpSocketAddress___(SOCKET_ADDRESS *addr)
{
    if (addr == NULL)
        return;

    TRACE("            iSockaddrLength =    %d\n", addr->iSockaddrLength);
    TRACE("            lpSockaddr =         %p\n", addr->lpSockaddr);

    if (addr->lpSockaddr == NULL)
        return;

    switch (addr->lpSockaddr->sa_family)
    {
        case AF_INET:
        {
            struct WS(sockaddr_in) *inet = (struct WS(sockaddr_in) *)addr->lpSockaddr;


            TRACE("                sin_family =     %s (%u)\n", IPHLPAPI_getAddressFamilyName(inet->sin_family), inet->sin_family);
            TRACE("                sin_port =       %u\n", inet->sin_port);
            TRACE("                sin_addr =       %u.%u.%u.%u\n", (unsigned char)inet->sin_addr.WS(s_net), (unsigned char)inet->sin_addr.WS(s_host), (unsigned char)inet->sin_addr.WS(s_lh), (unsigned char)inet->sin_addr.WS(s_impno));
            break;
        }

        default:
        {
            struct WS(sockaddr) *sa = addr->lpSockaddr;


            TRACE("                sa_family =      %s (%u)\n", IPHLPAPI_getAddressFamilyName(sa->sa_family), sa->sa_family);
            TRACE("                sa_data =        <%02x %02x %02x %02x>\n", (unsigned char)sa->sa_data[0], (unsigned char)sa->sa_data[1], (unsigned char)sa->sa_data[2], (unsigned char)sa->sa_data[3]);
            TRACE("                                 <%02x %02x %02x %02x>\n", (unsigned char)sa->sa_data[4], (unsigned char)sa->sa_data[5], (unsigned char)sa->sa_data[6], (unsigned char)sa->sa_data[7]);
            TRACE("                                 <%02x %02x %02x %02x>\n", (unsigned char)sa->sa_data[8], (unsigned char)sa->sa_data[9], (unsigned char)sa->sa_data[10], (unsigned char)sa->sa_data[11]);
            TRACE("                                 <%02x %02x>\n",           (unsigned char)sa->sa_data[12], (unsigned char)sa->sa_data[13]);
            break;
        }
    }
}

static void IPHLPAPI_dumpUnicastAddress__(IP_ADAPTER_UNICAST_ADDRESS *addr)
{
    if (addr == NULL)
        return;

    TRACE("        Length =             %u\n", addr->u.s.Length);
    TRACE("        Flags =              0x%08x\n", addr->u.s.Flags);
    TRACE("        Next =               %p\n", addr->Next);
    TRACE("        Address:\n");
    IPHLPAPI_dumpSocketAddress___(&addr->Address);
    TRACE("        PrefixOrigin =       %d\n", addr->PrefixOrigin);
    TRACE("        SuffixOrigin =       %d\n", addr->SuffixOrigin);
    TRACE("        DadState =           %d\n", addr->DadState);
    TRACE("        ValidLifetime =      %u\n", addr->ValidLifetime);
    TRACE("        PreferredLifetime =  %u\n", addr->PreferredLifetime);
    TRACE("        LeaseLifetime =      %u\n", addr->LeaseLifetime);

    IPHLPAPI_dumpUnicastAddress__(addr->Next);
}

static void IPHLPAPI_dumpAnycastAddress__(IP_ADAPTER_ANYCAST_ADDRESS *addr)
{
    if (addr == NULL)
        return;

    TRACE("        Length =     %u\n", addr->u.s.Length);
    TRACE("        Flags =      0x%08x\n", addr->u.s.Flags);
    TRACE("        Next =       %p\n", addr->Next);
    TRACE("        Address:\n");
    IPHLPAPI_dumpSocketAddress___(&addr->Address);

    IPHLPAPI_dumpAnycastAddress__(addr->Next);
}

static void IPHLPAPI_dumpMulticastAddress(IP_ADAPTER_MULTICAST_ADDRESS *addr)
{
    if (addr == NULL)
        return;

    TRACE("        Length =     %u\n", addr->u.s.Length);
    TRACE("        Flags =      0x%08x\n", addr->u.s.Flags);
    TRACE("        Next =       %p\n", addr->Next);
    TRACE("        Address:\n");
    IPHLPAPI_dumpSocketAddress___(&addr->Address);

    IPHLPAPI_dumpMulticastAddress(addr->Next);
}

static void IPHLPAPI_dumpDnsServerAddress(IP_ADAPTER_DNS_SERVER_ADDRESS *addr)
{
    if (addr == NULL)
        return;

    TRACE("        Length =     %u\n", addr->u.s.Length);
    TRACE("        Flags =      0x%08x\n", addr->u.s.Reserved);
    TRACE("        Next =       %p\n", addr->Next);
    TRACE("        Address:\n");
    IPHLPAPI_dumpSocketAddress___(&addr->Address);

    IPHLPAPI_dumpDnsServerAddress(addr->Next);
}

static void IPHLPAPI_dumpPrefixAddress___(IP_ADAPTER_PREFIX *addr)
{
    if (addr == NULL)
        return;

    TRACE("        Length =       %u\n", addr->u.s.Length);
    TRACE("        Flags =        0x%08x\n", addr->u.s.Flags);
    TRACE("        Next =         %p\n", addr->Next);
    TRACE("        Address:\n");
    IPHLPAPI_dumpSocketAddress___(&addr->Address);
    TRACE("        PrefixLength = %u\n", addr->PrefixLength);

    IPHLPAPI_dumpPrefixAddress___(addr->Next);
}

static const char *IPHLPAPI_getOperStatusName(IF_OPER_STATUS status)
{
#define GETNAME(x)  case x: return #x

    switch (status)
    {
        GETNAME(IfOperStatusUp);
        GETNAME(IfOperStatusDown);
        GETNAME(IfOperStatusTesting);
        GETNAME(IfOperStatusUnknown);
        GETNAME(IfOperStatusDormant);
        GETNAME(IfOperStatusNotPresent);
        GETNAME(IfOperStatusLowerLayerDown);
        default:
            return "<unknown_status>";
    }

#undef GETNAME
}

static const char *IPHLPAPI_getIfTypeName(ULONG ifType)
{
#define GETNAME(x)  case x: return #x

    switch (ifType)
    {
        GETNAME(IF_TYPE_OTHER);
        GETNAME(IF_TYPE_ETHERNET_CSMACD);
        GETNAME(IF_TYPE_ISO88025_TOKENRING);
        GETNAME(IF_TYPE_PPP);
        GETNAME(IF_TYPE_SOFTWARE_LOOPBACK);
        GETNAME(IF_TYPE_ATM);
        GETNAME(IF_TYPE_IEEE80211);
        GETNAME(IF_TYPE_TUNNEL);
        GETNAME(IF_TYPE_IEEE1394);
        default:
            return "<unknown_type>";
    }

#undef GETNAME
}

void IPHLPAPI_dumpAdapterAddresses(IP_ADAPTER_ADDRESSES *addr)
{
    if (addr == NULL)
        return;

    TRACE("dumping adapter address %p {Length = %u, IfIndex = %u}\n", addr, addr->u.s.Length, addr->u.s.IfIndex);
    TRACE("    Next =                   %p\n", addr->Next);
    TRACE("    AdapterName =            '%s'\n", addr->AdapterName);
    TRACE("    FirstUnicastAddress =    %p\n", addr->FirstUnicastAddress);
    IPHLPAPI_dumpUnicastAddress__(addr->FirstUnicastAddress);
    TRACE("    FirstAnycastAddress =    %p\n", addr->FirstAnycastAddress);
    IPHLPAPI_dumpAnycastAddress__(addr->FirstAnycastAddress);
    TRACE("    FirstMulticastAddress =  %p\n", addr->FirstMulticastAddress);
    IPHLPAPI_dumpMulticastAddress(addr->FirstMulticastAddress);
    TRACE("    FirstDnsServerAddress =  %p\n", addr->FirstDnsServerAddress);
    IPHLPAPI_dumpDnsServerAddress(addr->FirstDnsServerAddress);
    TRACE("    DnsSuffix =              %s\n", debugstr_w(addr->DnsSuffix));
    TRACE("    Description =            %s\n", debugstr_w(addr->Description));
    TRACE("    FriendlyName =           %s\n", debugstr_w(addr->FriendlyName));
    TRACE("    PhysicalAddress =        <%02x %02x %02x %02x %02x %02x %02x %02x>\n",
            addr->PhysicalAddress[0], addr->PhysicalAddress[1], addr->PhysicalAddress[2], addr->PhysicalAddress[3],
            addr->PhysicalAddress[4], addr->PhysicalAddress[5], addr->PhysicalAddress[6], addr->PhysicalAddress[7]);
    TRACE("    PhysicalAddressLength =  %u\n", addr->PhysicalAddressLength);
    TRACE("    Flags =                  0x%08x\n", addr->Flags);
    TRACE("    Mtu =                    %u\n", addr->Mtu);
    TRACE("    IfType =                 %s (%u)\n", IPHLPAPI_getIfTypeName(addr->IfType), addr->IfType);
    TRACE("    OperStatus =             %s (%d)\n", IPHLPAPI_getOperStatusName(addr->OperStatus), addr->OperStatus);
    TRACE("    Ipv6IfIndex =            %u\n", addr->Ipv6IfIndex);
    TRACE("    ZoneIndices =            <%08x %08x %08x %08x>\n", addr->ZoneIndices[0], addr->ZoneIndices[1], addr->ZoneIndices[2], addr->ZoneIndices[3]);
    TRACE("                             <%08x %08x %08x %08x>\n", addr->ZoneIndices[4], addr->ZoneIndices[5], addr->ZoneIndices[6], addr->ZoneIndices[7]);
    TRACE("                             <%08x %08x %08x %08x>\n", addr->ZoneIndices[8], addr->ZoneIndices[9], addr->ZoneIndices[10], addr->ZoneIndices[11]);
    TRACE("                             <%08x %08x %08x %08x>\n", addr->ZoneIndices[12], addr->ZoneIndices[13], addr->ZoneIndices[14], addr->ZoneIndices[15]);
    TRACE("    FirstPrefix =            %p\n", addr->FirstPrefix);
    IPHLPAPI_dumpPrefixAddress___(addr->FirstPrefix);

    IPHLPAPI_dumpAdapterAddresses(addr->Next);
}
