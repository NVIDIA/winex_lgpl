/*
 * Copyright 2001 Mike McCormack
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "winbase.h"
#include "winreg.h"
#include "wingdi.h"
#include "winuser.h"
#include "lm.h"
#include "wine/debug.h"
#include "winerror.h"
#include "nb30.h"

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif
#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#define ifreq_len(ifr) \
 max(sizeof(struct ifreq), sizeof((ifr)->ifr_name)+(ifr)->ifr_addr.sa_len)
#else
#define ifreq_len(ifr) sizeof(struct ifreq)
#endif

WINE_DEFAULT_DEBUG_CHANNEL(netbios);

HMODULE NETAPI32_hModule = 0;
static BOOL EnumDone = FALSE;

struct NetBiosAdapter
{
    int valid;
    unsigned char address[6];
};

static struct NetBiosAdapter NETBIOS_Adapter[MAX_LANA];

# ifdef SIOCGIFHWADDR
int get_hw_address(int sd, struct ifreq *ifr, unsigned char *address)
{
    if (ioctl(sd, SIOCGIFHWADDR, ifr) < 0)
        return -1;
    memcpy(address, (unsigned char *)&ifr->ifr_hwaddr.sa_data, 6);
    return 0;
}
# else
#  ifdef SIOCGENADDR
int get_hw_address(int sd, struct ifreq *ifr, unsigned char *address)
{
    if (ioctl(sd, SIOCGENADDR, ifr) < 0)
        return -1;
    memcpy(address, (unsigned char *) ifr->ifr_enaddr, 6);
    return 0;
}
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if_dl.h>

int get_hw_address (int sd, struct ifreq *ifr, unsigned char *addr)
{
  DWORD ret;
  struct if_msghdr *ifm;
  struct sockaddr_dl *sdl;
  u_char *p, *buf;
  size_t mibLen;
  int mib[] = { CTL_NET, AF_ROUTE, 0, AF_LINK, NET_RT_IFLIST, 0 };
  int addrLen;
  BOOL found = FALSE;

  if (sysctl(mib, 6, NULL, &mibLen, NULL, 0) < 0)
    return -1;

  buf = HeapAlloc(GetProcessHeap(), 0, mibLen);
  if (!buf)
    return -1;

  if (sysctl(mib, 6, buf, &mibLen, NULL, 0) < 0) {
    HeapFree(GetProcessHeap(), 0, buf);
    return -1;
  }

  ret = ERROR_INVALID_DATA;
  for (p = buf; !found && p < buf + mibLen; p += ifm->ifm_msglen) {
    ifm = (struct if_msghdr *)p;
    sdl = (struct sockaddr_dl *)(ifm + 1);

    if (ifm->ifm_type != RTM_IFINFO || (ifm->ifm_addrs & RTA_IFP) == 0)
      continue;

    if (sdl->sdl_family != AF_LINK || sdl->sdl_nlen == 0 ||
     memcmp(sdl->sdl_data, ifr->ifr_name,
            max(sdl->sdl_nlen, strlen(ifr->ifr_name))) != 0)
      continue;

    found = TRUE;
    addrLen = min(8, sdl->sdl_alen);
    if (addrLen > 0)
        memcpy(addr, LLADDR(sdl), addrLen);
      /* zero out remaining bytes for broken implementations */
  }
  HeapFree(GetProcessHeap(), 0, buf);
  return 0;
}
#else
int get_hw_address(int sd, struct ifreq *ifr, unsigned char *address)
{
    return -1;
}
#  endif /* SIOCGENADDR */
# endif /* SIOCGIFHWADDR */

static UCHAR NETBIOS_Enum(PNCB ncb)
{
#ifdef HAVE_NET_IF_H
    int             sd;
    struct ifconf   ifc;
    int             i;
    int             lastlen, numAddresses = 2;
    int             ioctlRet = 0;
    caddr_t         ifPtr;
#endif
    LANA_ENUM *lanas = NULL;

    if (ncb)
    {
       lanas = (PLANA_ENUM) ncb->ncb_buffer;
       lanas->length = 0;
    }

    TRACE("NCBENUM\n");

#ifdef HAVE_NET_IF_H
    /* BSD 4.4 defines the size of an ifreq to be
     * max(sizeof(ifreq), sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len
     * However, under earlier systems, sa_len isn't present, so
     *  the size is just sizeof(struct ifreq)
     */

    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sd < 0)
        return NRC_OPENERROR;

    ifc.ifc_len = 0;
    ifc.ifc_buf = NULL;

    do {
       lastlen = ifc.ifc_len;
       HeapFree (GetProcessHeap (), 0, ifc.ifc_buf);
       numAddresses *= 2;

       ifc.ifc_len = sizeof (struct ifreq) * numAddresses;
       ifc.ifc_buf = HeapAlloc (GetProcessHeap (), 0, ifc.ifc_len);
       if (!ifc.ifc_buf)
       {
          close (sd);
          return NRC_OPENERROR;
       }

       ioctlRet = ioctl (sd, SIOCGIFCONF, (char *)&ifc);
    } while ((ioctlRet == 0) && (ifc.ifc_len != lastlen));

    if (ioctlRet)
    {
       HeapFree (GetProcessHeap (), 0, ifc.ifc_buf);
       close (sd);
       return NRC_OPENERROR;
    }

    /* loop through the interfaces, looking for a valid one */
    /* n = ifc.ifc_len; */
    ifPtr = ifc.ifc_buf;
    i = 0;
    while (ifPtr && (ifPtr < (ifc.ifc_buf + ifc.ifc_len)))
    {
        unsigned char *a = NETBIOS_Adapter[i].address;
        struct ifreq   ifr, *ifrp;

        ifrp = (struct ifreq *)ifPtr;

        ifPtr += ifreq_len ((struct ifreq *)ifPtr);
        i++;

        if (ifrp->ifr_addr.sa_family != AF_INET)
           continue;

        strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);

        /* try to get the address for this interface */
        if(get_hw_address(sd, &ifr, a)==0)
        {
            /* make sure it's not blank */
            /* if (!a[0] && !a[1] && !a[2] && !a[3] && !a[4] && !a[5])
	        continue; */

            TRACE("Found valid adapter %d at %02x:%02x:%02x:%02x:%02x:%02x\n", i,
                        a[0],a[1],a[2],a[3],a[4],a[5]);

            if (a[0] || a[1] || a[2] || a[3] || a[4] || a[5])
               NETBIOS_Adapter[i - 1].valid = TRUE;

            if (lanas)
            {
               lanas->lana[lanas->length] = i - 1;
               lanas->length++;
            }
        }
    }
    close(sd);
    HeapFree (GetProcessHeap (), 0, ifc.ifc_buf);

#endif /* HAVE_NET_IF_H */
    EnumDone = TRUE;
    return NRC_GOODRET;
}


static UCHAR NETBIOS_Astat(PNCB ncb)
{
    struct NetBiosAdapter *nad = &NETBIOS_Adapter[ncb->ncb_lana_num];
    PADAPTER_STATUS astat = (PADAPTER_STATUS) ncb->ncb_buffer;

    TRACE("NCBASTAT (Adapter %d)\n", ncb->ncb_lana_num);

    if(!nad->valid)
        return NRC_INVADDRESS;

    memset(astat, 0, sizeof astat);
    memcpy(astat->adapter_address, nad->address, sizeof astat->adapter_address);

    return NRC_GOODRET;
}

BOOL WINAPI
NETAPI32_LibMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("%x,%lx,%p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
            NETAPI32_hModule = hinstDLL;
	    break;
	case DLL_PROCESS_DETACH:
	    break;
    }

    return TRUE;
}

BOOL WINAPI Netbios(PNCB pncb)
{
    UCHAR ret = NRC_ILLCMD;

    TRACE("ncb = %p\n",pncb);

    if(!pncb)
        return NRC_INVADDRESS;

    switch(pncb->ncb_command&0x7f)
    {
    case NCBRESET:
       if (!EnumDone)
          NETBIOS_Enum (NULL);

        TRACE("NCBRESET adapter %d\n",pncb->ncb_lana_num);
        if( (pncb->ncb_lana_num < MAX_LANA ) &&
             NETBIOS_Adapter[pncb->ncb_lana_num].valid)
            ret = NRC_GOODRET;
        else
            ret = NRC_ILLCMD; /* NetBIOS emulator not found */
        break;

    case NCBADDNAME:
        FIXME("NCBADDNAME\n");
        break;

    case NCBADDGRNAME:
        FIXME("NCBADDGRNAME\n");
        break;

    case NCBDELNAME:
        FIXME("NCBDELNAME\n");
        break;

    case NCBSEND:
        FIXME("NCBSEND\n");
        break;

    case NCBRECV:
        FIXME("NCBRECV\n");
        break;

    case NCBHANGUP:
        FIXME("NCBHANGUP\n");
        break;

    case NCBCANCEL:
        FIXME("NCBCANCEL\n");
        break;

    case NCBLISTEN:
        FIXME("NCBLISTEN\n");
        break;

    case NCBASTAT:
        ret = NETBIOS_Astat(pncb);
        break;

    case NCBENUM:
        ret = NETBIOS_Enum(pncb);
        break;

    default:
        FIXME("(%p): command code %02x\n", pncb, pncb->ncb_command);

        ret = NRC_ILLCMD; /* NetBIOS emulator not found */
    }
    pncb->ncb_retcode = ret;
    return ret;
}


static WCHAR ServiceWorkstation[] = {'L', 'a', 'n', 'm', 'a', 'n', 'W', 'o',
      'r', 'k', 's', 't', 'a', 't', 'i', 'o', 'n', 0};
static WCHAR ServiceServer[] = {'L', 'a', 'n', 'm', 'a', 'n', 'S', 'e',
      'r', 'v', 'e', 'r', 0};


NET_API_STATUS NET_API_FUNCTION
NetStatisticsGet (LMSTR server, LMSTR service, DWORD level, DWORD options,
                  LPBYTE *bufptr)
{
   FIXME ("(%s, %s, %lu, %lu, %p): stub!\n", debugstr_w (server),
          debugstr_w (service), level, options, bufptr);

   /* Ignore server, since we're returning gibberish for now anyways */

   if (level)
      return ERROR_INVALID_LEVEL;

   if (options)
      return ERROR_INVALID_PARAMETER;

   /* Windows crashes on NULL service or bufptr, so we won't do otherwise */
   if (!lstrcmpW (service, ServiceWorkstation))
      *bufptr = HeapAlloc (GetProcessHeap (), 0, sizeof (STAT_WORKSTATION_0));
   else if (!lstrcmpW (service, ServiceServer))
      *bufptr = HeapAlloc (GetProcessHeap (), 0, sizeof (STAT_SERVER_0));
   else
      return ERROR_NOT_SUPPORTED;

   if (!*bufptr)
   {
      ERR ("Out of memory allocating statistics structure!\n");
      return ERROR_OUTOFMEMORY;
   }

   /* Yes, we're returning uninitialized memory, which will make for
      "interesting" network statistics. The main use for this function that
      we've encountered to date is by OpenSSL to gather random data for its
      entropy. Thus, we're obliging. */

   return NERR_Success;
}


NET_API_STATUS NET_API_FUNCTION
NetApiBufferFree (LPVOID Buffer)
{
   HeapFree (GetProcessHeap (), 0, Buffer);
   return NERR_Success;
}
