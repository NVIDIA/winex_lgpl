/*
 *  RPCRT4
 *
 * WINE RPC TODO's (and a few TODONT's)
 *
 * - widl is like MIDL for wine.  For wine to be a useful RPC platform, quite
 *   a bit of work needs to be done here.  widl currently doesn't generate stubs
 *   for RPC invocation -- it will need to; this is tricky because the MIDL compiler
 *   does some really wierd stuff.  Then again, we don't neccesarily have to
 *   make widl work like MIDL, so it could be worse.  Lately Ove has been working on
 *   some widl enhancements.
 *
 * - RPC has a quite featureful error handling mechanism; basically none of this is
 *   implemented right now.
 *
 * - There are several different memory allocation schemes for MSRPC.
 *   I don't even understand what they all are yet, much less have them
 *   properly implemented.  Surely we are supposed to be doing something with
 *   the user-provided allocation/deallocation functions, but so far,
 *   I don't think we are doing this...
 *
 * - MSRPC provides impersonation capabilities which currently are not possible
 *   to implement in wine.  At the very least we should implement the authorization
 *   API's & gracefully ignore the irrelevant stuff (to a small extent we already do).
 *
 * - Some transports are not yet implemented.  The existing transport implementations
 *   are incomplete and many seem to be buggy
 *
 * - The various transports that we do support ought to be supported in a more
 *   object-oriented manner, like in DCE's RPC implementation, instead of cluttering
 *   up the code with conditionals like we do now.
 *
 * - Data marshalling: So far, only the very beginnings of an implementation
 *   exist in wine.  NDR protocol itself is documented, but the MS API's to
 *   convert data-types in memory into NDR are not.  This is a bit of a challenge,
 *   but it is at the top of Greg's queue and should be improving soon.
 *
 * - ORPC is RPC for OLE; once we have a working RPC framework, we can
 *   use it to implement out-of-process OLE client/server communications.
 *   ATM there is a 100% disconnect between the marshalling in the OLE DLL's
 *   and the marshalling going on here.  This is a good thing, since marshalling
 *   doesn't work yet.  But once it does, obviously there will be the opportunity
 *   to implement out-of-process OLE using wine's rpcrt4 or some derivative.
 *   This may require some collaboration between the RPC workers and the OLE
 *   workers, of course.
 *
 * - In-source API Documentation, at least for those functions which we have
 *   implemented, but preferably for everything we can document, would be nice.
 *   Some stuff is undocumented by Microsoft and we are guessing how to implement
 *   (in these cases we should document the behavior we implemented, or, if there
 *   is no implementation, at least hazard some kind of guess, and put a few
 *   question marks after it ;) ).
 *
 * - Stubs.  Lots of stuff is defined in Microsoft's headers, including undocumented
 *   stuff.  So let's make a stub-farm and populate it with as many rpcrt4 api's as
 *   we can stand, so people don't get unimplemented function exceptions.
 *
 * - Name services: this part hasn't even been started.
 *
 * - Concurrency: right now I have not tested more than one request at a time;
 *   we are supposed to be able to do this, and to queue requests which exceed the
 *   concurrency limit.
 *
 * - Protocol Towers: Totally unimplemented.... I think.
 *
 * - Context Handle Rundown: whatever that is.
 *
 * - Nested RPC's: Totally unimplemented.
 *
 * - Statistics: we are supposed to be keeping various counters.  we aren't.
 *
 * - Connectionless RPC: unimplemented (DNE in win9x so not a top priority)
 *
 * - XML RPC: Dunno if microsoft does it... but we'd might as well just for kicks.
 *
 * - ...?  More stuff I haven't thought of.  If you think of more RPC todo's drop me
 *   an e-mail <gmturner007@ameritech.net> or send a patch to wine-patches.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <unistd.h>

#include "windef.h"
#include "winerror.h"
#include "winbase.h"
#include "winuser.h"
#include "wine/unicode.h"
#include "rpc.h"

#include "ole2.h"
#include "rpcndr.h"
#include "rpcproxy.h"

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

#include "rpc_binding.h"
#include "rpcss_np_client.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

static UUID uuid_nil;
static HANDLE master_mutex;

extern void create_server_cs(void);
extern void create_binding_cs(void);
extern void destroy_server_cs(void);
extern void destroy_binding_cs(void);

HANDLE RPCRT4_GetMasterMutex(void)
{
    return master_mutex;
}

/***********************************************************************
 * RPCRT4_LibMain
 *
 * PARAMS
 *     hinstDLL    [I] handle to the DLL's instance
 *     fdwReason   [I]
 *     lpvReserved [I] reserved, must be NULL
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI
RPCRT4_LibMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        master_mutex = CreateMutexA( NULL, FALSE, RPCSS_MASTER_MUTEX_NAME);
        if (!master_mutex)
          ERR("Failed to create master mutex\n");
        create_binding_cs();
        create_server_cs();
        break;

    case DLL_PROCESS_DETACH:
        destroy_server_cs();
        destroy_binding_cs();
        CloseHandle(master_mutex);
        master_mutex = (HANDLE) NULL;
        break;
    }

    return TRUE;
}

/*************************************************************************
 *           RpcStringFreeA   [RPCRT4.@]
 *
 * Frees a character string allocated by the RPC run-time library.
 *
 * RETURNS
 *
 *  S_OK if successful.
 */
RPC_STATUS WINAPI RpcStringFreeA(LPSTR* String)
{
  HeapFree( GetProcessHeap(), 0, *String);

  return RPC_S_OK;
}

/*************************************************************************
 *           RpcStringFreeW   [RPCRT4.@]
 *
 * Frees a character string allocated by the RPC run-time library.
 *
 * RETURNS
 *
 *  S_OK if successful.
 */
RPC_STATUS WINAPI RpcStringFreeW(LPWSTR* String)
{
  HeapFree( GetProcessHeap(), 0, *String);

  return RPC_S_OK;
}

/*************************************************************************
 *           RpcRaiseException   [RPCRT4.@]
 *
 * Raises an exception.
 */
void WINAPI RpcRaiseException(RPC_STATUS exception)
{
  /* FIXME: translate exception? */
  RaiseException(exception, 0, 0, NULL);
}

/*************************************************************************
 * UuidCompare [RPCRT4.@]
 *
 * (an educated-guess implementation)
 *
 * PARAMS
 *     UUID *Uuid1        [I] Uuid to compare
 *     UUID *Uuid2        [I] Uuid to compare
 *     RPC_STATUS *Status [O] returns RPC_S_OK
 *
 * RETURNS
 *     -1 if Uuid1 is less than Uuid2
 *     0  if Uuid1 and Uuid2 are equal
 *     1  if Uuid1 is greater than Uuid2
 */
int WINAPI UuidCompare(UUID *Uuid1, UUID *Uuid2, RPC_STATUS *Status)
{
  TRACE("(%s,%s)\n", debugstr_guid(Uuid1), debugstr_guid(Uuid2));
  *Status = RPC_S_OK;
  if (!Uuid1) Uuid1 = &uuid_nil;
  if (!Uuid2) Uuid2 = &uuid_nil;
  if (Uuid1 == Uuid2) return 0;
  return memcmp(Uuid1, Uuid2, sizeof(UUID));
}

/*************************************************************************
 * UuidEqual [RPCRT4.@]
 *
 * PARAMS
 *     UUID *Uuid1        [I] Uuid to compare
 *     UUID *Uuid2        [I] Uuid to compare
 *     RPC_STATUS *Status [O] returns RPC_S_OK
 *
 * RETURNS
 *     TRUE/FALSE
 */
int WINAPI UuidEqual(UUID *Uuid1, UUID *Uuid2, RPC_STATUS *Status)
{
  TRACE("(%s,%s)\n", debugstr_guid(Uuid1), debugstr_guid(Uuid2));
  return !UuidCompare(Uuid1, Uuid2, Status);
}

/*************************************************************************
 * UuidIsNil [RPCRT4.@]
 *
 * PARAMS
 *     UUID *Uuid         [I] Uuid to compare
 *     RPC_STATUS *Status [O] retuns RPC_S_OK
 *
 * RETURNS
 *     TRUE/FALSE
 */
int WINAPI UuidIsNil(UUID *uuid, RPC_STATUS *Status)
{
  TRACE("(%s)\n", debugstr_guid(uuid));
  *Status = RPC_S_OK;
  if (!uuid) return TRUE;
  return !memcmp(uuid, &uuid_nil, sizeof(UUID));
}

 /*************************************************************************
 * UuidCreateNil [RPCRT4.@]
 *
 * PARAMS
 *     UUID *Uuid [O] returns a nil UUID
 *
 * RETURNS
 *     RPC_S_OK
 */
RPC_STATUS WINAPI UuidCreateNil(UUID *Uuid)
{
  *Uuid = uuid_nil;
  return RPC_S_OK;
}

/*************************************************************************
 *           UuidCreate   [RPCRT4.@]
 *
 * Creates a 128bit UUID.
 * Implemented according the DCE specification for UUID generation.
 * Code is based upon uuid library in e2fsprogs by Theodore Ts'o.
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * RETURNS
 *
 *  S_OK if successful.
 */
RPC_STATUS WINAPI UuidCreate(UUID *Uuid)
{
   static char has_init = 0;
   static unsigned char a[6];
   static int                      adjustment = 0;
   static struct timeval           last = {0, 0};
   static WORD                     clock_seq;
   struct timeval                  tv;
   unsigned long long              clock_reg;
   DWORD clock_high, clock_low;
   WORD temp_clock_seq, temp_clock_mid, temp_clock_hi_and_version;
#ifdef HAVE_NET_IF_H
   int             sd;
   struct ifreq    ifr, *ifrp;
   struct ifconf   ifc;
   char buf[1024];
   int             n, i;
#endif

   /* Have we already tried to get the MAC address? */
   if (!has_init) {
#ifdef HAVE_NET_IF_H
      /* BSD 4.4 defines the size of an ifreq to be
       * max(sizeof(ifreq), sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len
       * However, under earlier systems, sa_len isn't present, so
       *  the size is just sizeof(struct ifreq)
       */
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#  ifndef max
#   define max(a,b) ((a) > (b) ? (a) : (b))
#  endif
#  define ifreq_size(i) max(sizeof(struct ifreq),\
sizeof((i).ifr_name)+(i).ifr_addr.sa_len)
# else
#  define ifreq_size(i) sizeof(struct ifreq)
# endif /* defined(HAVE_STRUCT_SOCKADDR_SA_LEN) */

      sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
      if (sd < 0) {
	 /* if we can't open a socket, just use random numbers */
	 /* set the multicast bit to prevent conflicts with real cards */
	 a[0] = (rand() & 0xff) | 0x80;
	 a[1] = rand() & 0xff;
	 a[2] = rand() & 0xff;
	 a[3] = rand() & 0xff;
	 a[4] = rand() & 0xff;
	 a[5] = rand() & 0xff;
      } else {
	 memset(buf, 0, sizeof(buf));
	 ifc.ifc_len = sizeof(buf);
	 ifc.ifc_buf = buf;
	 /* get the ifconf interface */
	 if (ioctl (sd, SIOCGIFCONF, (char *)&ifc) < 0) {
	    close(sd);
	    /* no ifconf, so just use random numbers */
	    /* set the multicast bit to prevent conflicts with real cards */
	    a[0] = (rand() & 0xff) | 0x80;
	    a[1] = rand() & 0xff;
	    a[2] = rand() & 0xff;
	    a[3] = rand() & 0xff;
	    a[4] = rand() & 0xff;
	    a[5] = rand() & 0xff;
	 } else {
	    /* loop through the interfaces, looking for a valid one */
	    n = ifc.ifc_len;
	    for (i = 0; i < n; i+= ifreq_size(ifr) ) {
	       ifrp = (struct ifreq *)((char *) ifc.ifc_buf+i);
	       strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);
	       /* try to get the address for this interface */
# ifdef SIOCGIFHWADDR
	       if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0)
		   continue;
	       memcpy(a, (unsigned char *)&ifr.ifr_hwaddr.sa_data, 6);
# else
#  ifdef SIOCGENADDR
	       if (ioctl(sd, SIOCGENADDR, &ifr) < 0)
		   continue;
	       memcpy(a, (unsigned char *) ifr.ifr_enaddr, 6);
#  else
	       /* XXX we don't have a way of getting the hardware address */
	       close(sd);
	       a[0] = 0;
	       break;
#  endif /* SIOCGENADDR */
# endif /* SIOCGIFHWADDR */
	       /* make sure it's not blank */
	       if (!a[0] && !a[1] && !a[2] && !a[3] && !a[4] && !a[5])
		   continue;

	       goto valid_address;
	    }
	    /* if we didn't find a valid address, make a random one */
	    /* once again, set multicast bit to avoid conflicts */
	    a[0] = (rand() & 0xff) | 0x80;
	    a[1] = rand() & 0xff;
	    a[2] = rand() & 0xff;
	    a[3] = rand() & 0xff;
	    a[4] = rand() & 0xff;
	    a[5] = rand() & 0xff;

	    valid_address:
	    close(sd);
	 }
      }
#else
      /* no networking info, so generate a random address */
      a[0] = (rand() & 0xff) | 0x80;
      a[1] = rand() & 0xff;
      a[2] = rand() & 0xff;
      a[3] = rand() & 0xff;
      a[4] = rand() & 0xff;
      a[5] = rand() & 0xff;
#endif /* HAVE_NET_IF_H */
      has_init = 1;
   }

   /* generate time element of GUID */

   /* Assume that the gettimeofday() has microsecond granularity */
#define MAX_ADJUSTMENT 10

   try_again:
   gettimeofday(&tv, 0);
   if ((last.tv_sec == 0) && (last.tv_usec == 0)) {
      clock_seq = ((rand() & 0xff) << 8) + (rand() & 0xff);
      clock_seq &= 0x1FFF;
      last = tv;
      last.tv_sec--;
   }
   if ((tv.tv_sec < last.tv_sec) ||
       ((tv.tv_sec == last.tv_sec) &&
	(tv.tv_usec < last.tv_usec))) {
      clock_seq = (clock_seq+1) & 0x1FFF;
      adjustment = 0;
   } else if ((tv.tv_sec == last.tv_sec) &&
	      (tv.tv_usec == last.tv_usec)) {
      if (adjustment >= MAX_ADJUSTMENT)
	  goto try_again;
      adjustment++;
   } else
       adjustment = 0;

   clock_reg = tv.tv_usec*10 + adjustment;
   clock_reg += ((unsigned long long) tv.tv_sec)*10000000;
   clock_reg += (((unsigned long long) 0x01B21DD2) << 32) + 0x13814000;

   clock_high = clock_reg >> 32;
   clock_low = clock_reg;
   temp_clock_seq = clock_seq | 0x8000;
   temp_clock_mid = (WORD)clock_high;
   temp_clock_hi_and_version = (clock_high >> 16) | 0x1000;

   /* pack the information into the GUID structure */

   ((unsigned char*)&Uuid->Data1)[3] = (unsigned char)clock_low;
   clock_low >>= 8;
   ((unsigned char*)&Uuid->Data1)[2] = (unsigned char)clock_low;
   clock_low >>= 8;
   ((unsigned char*)&Uuid->Data1)[1] = (unsigned char)clock_low;
   clock_low >>= 8;
   ((unsigned char*)&Uuid->Data1)[0] = (unsigned char)clock_low;

   ((unsigned char*)&Uuid->Data2)[1] = (unsigned char)temp_clock_mid;
   temp_clock_mid >>= 8;
   ((unsigned char*)&Uuid->Data2)[0] = (unsigned char)temp_clock_mid;

   ((unsigned char*)&Uuid->Data3)[1] = (unsigned char)temp_clock_hi_and_version;
   temp_clock_hi_and_version >>= 8;
   ((unsigned char*)&Uuid->Data3)[0] = (unsigned char)temp_clock_hi_and_version;

   ((unsigned char*)Uuid->Data4)[1] = (unsigned char)temp_clock_seq;
   temp_clock_seq >>= 8;
   ((unsigned char*)Uuid->Data4)[0] = (unsigned char)temp_clock_seq;

   ((unsigned char*)Uuid->Data4)[2] = a[0];
   ((unsigned char*)Uuid->Data4)[3] = a[1];
   ((unsigned char*)Uuid->Data4)[4] = a[2];
   ((unsigned char*)Uuid->Data4)[5] = a[3];
   ((unsigned char*)Uuid->Data4)[6] = a[4];
   ((unsigned char*)Uuid->Data4)[7] = a[5];

   TRACE("%s\n", debugstr_guid(Uuid));

   return RPC_S_OK;
}


/*************************************************************************
 *           UuidCreateSequential   [RPCRT4.@]
 *
 * Creates a 128bit UUID by calling UuidCreate.
 * New API in Win 2000
 */
RPC_STATUS WINAPI UuidCreateSequential(UUID *Uuid)
{
   return UuidCreate (Uuid);
}


/*************************************************************************
 *           UuidHash   [RPCRT4.@]
 *
 * Generates a hash value for a given UUID
 *
 * Code based on FreeDCE implementation
 *
 */
unsigned short WINAPI UuidHash(UUID *uuid, RPC_STATUS *Status)
{
  BYTE *data = (BYTE*)uuid;
  short c0 = 0, c1 = 0, x, y;
  int i;

  if (!uuid) data = (BYTE*)(uuid = &uuid_nil);

  TRACE("(%s)\n", debugstr_guid(uuid));

  for (i=0; i<sizeof(UUID); i++) {
    c0 += data[i];
    c1 += c0;
  }

  x = -c1 % 255;
  if (x < 0) x += 255;

  y = (c1 - c0) % 255;
  if (y < 0) y += 255;

  *Status = RPC_S_OK;
  return y*256 + x;
}

/*************************************************************************
 *           UuidToStringA   [RPCRT4.@]
 *
 * Converts a UUID to a string.
 *
 * UUID format is 8 hex digits, followed by a hyphen then three groups of
 * 4 hex digits each followed by a hyphen and then 12 hex digits
 *
 * RETURNS
 *
 *  S_OK if successful.
 *  S_OUT_OF_MEMORY if unsucessful.
 */
RPC_STATUS WINAPI UuidToStringA(UUID *Uuid, LPSTR* StringUuid)
{
  *StringUuid = HeapAlloc( GetProcessHeap(), 0, sizeof(char) * 37);

  if(!(*StringUuid))
    return RPC_S_OUT_OF_MEMORY;

  if (!Uuid) Uuid = &uuid_nil;

  sprintf(*StringUuid, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 Uuid->Data1, Uuid->Data2, Uuid->Data3,
                 Uuid->Data4[0], Uuid->Data4[1], Uuid->Data4[2],
                 Uuid->Data4[3], Uuid->Data4[4], Uuid->Data4[5],
                 Uuid->Data4[6], Uuid->Data4[7] );

  return RPC_S_OK;
}

/*************************************************************************
 *           UuidToStringW   [RPCRT4.@]
 *
 * Converts a UUID to a string.
 *
 *  S_OK if successful.
 *  S_OUT_OF_MEMORY if unsucessful.
 */
RPC_STATUS WINAPI UuidToStringW(UUID *Uuid, LPWSTR* StringUuid)
{
  char buf[37];

  if (!Uuid) Uuid = &uuid_nil;

  sprintf(buf, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               Uuid->Data1, Uuid->Data2, Uuid->Data3,
               Uuid->Data4[0], Uuid->Data4[1], Uuid->Data4[2],
               Uuid->Data4[3], Uuid->Data4[4], Uuid->Data4[5],
               Uuid->Data4[6], Uuid->Data4[7] );

  *StringUuid = RPCRT4_strdupAtoW(buf);

  if(!(*StringUuid))
    return RPC_S_OUT_OF_MEMORY;

  return RPC_S_OK;
}

/***********************************************************************
 * Inlines for hex "parsing"
 */
static inline int hex_d(WCHAR v)
{
  switch (v) {
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    return v - '0';
  case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    return v - 'a' + 10;
  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    return v - 'A' + 10;
  default:
    return -1;
  }
}

static inline unsigned char hex1A(LPSTR s)
{
  unsigned char v1 = hex_d(s[0]), v2 = hex_d(s[1]);
  return (v1 << 4) | v2;
}

static inline unsigned char hex1W(LPWSTR s)
{
  unsigned char v1 = hex_d(s[0]), v2 = hex_d(s[1]);
  return (v1 << 4) | v2;
}

static inline unsigned short hex2A(LPSTR s)
{
  unsigned char v1 = hex1A(s+0), v2 = hex1A(s+2);
  return (unsigned short)(v1 << 8) | v2;
}

static inline unsigned short hex2W(LPWSTR s)
{
  unsigned char v1 = hex1W(s+0), v2 = hex1W(s+2);
  return (unsigned short)(v1 << 8) | v2;
}

static inline unsigned long hex4A(LPSTR s)
{
  unsigned short v1 = hex2A(s+0), v2 = hex2A(s+4);
  return (unsigned long)(v1 << 16) | v2;
}

static inline unsigned long hex4W(LPWSTR s)
{
  unsigned short v1 = hex2W(s+0), v2 = hex2W(s+4);
  return (unsigned long)(v1 << 16) | v2;
}

/***********************************************************************
 *		UuidFromStringA (RPCRT4.@)
 *
 * Coverts a string to a UUID
 *
 * RETURNS
 *
 *   S_OK if successful.
 *   RPC_S_INVALID_STRING_UUID if unsuccessful.
 */
RPC_STATUS WINAPI UuidFromStringA(LPSTR StringUuid, UUID *Uuid)
{
  unsigned i;

  TRACE("%s\n", StringUuid);

  if(!StringUuid)
    return UuidCreateNil(Uuid);

  if (strlen(StringUuid) != 36)
    return RPC_S_INVALID_STRING_UUID;

  if (StringUuid[8]!='-' || StringUuid[13]!='-' || StringUuid[18]!='-' || StringUuid[23]!='-')
    return RPC_S_INVALID_STRING_UUID;

  for (i=0; i<36; i++) {
    if (i==8 || i == 13 || i == 18 || i == 23) continue;
    if (hex_d(StringUuid[i]) == -1)
      return RPC_S_INVALID_STRING_UUID;
  }

  Uuid->Data1 = hex4A(StringUuid);
  Uuid->Data2 = hex2A(StringUuid+9);
  Uuid->Data3 = hex2A(StringUuid+14);
  Uuid->Data4[0] = hex1A(StringUuid+19);
  Uuid->Data4[1] = hex1A(StringUuid+21);
  Uuid->Data4[2] = hex1A(StringUuid+24);
  Uuid->Data4[3] = hex1A(StringUuid+26);
  Uuid->Data4[4] = hex1A(StringUuid+28);
  Uuid->Data4[5] = hex1A(StringUuid+30);
  Uuid->Data4[6] = hex1A(StringUuid+32);
  Uuid->Data4[7] = hex1A(StringUuid+34);
  return RPC_S_OK;
}

/***********************************************************************
 *		UuidFromStringW (RPCRT4.@)
 *
 * Coverts a wide string to a UUID
 *
 * RETURNS
 *
 *   S_OK if successful.
 *   RPC_S_INVALID_STRING_UUID if unsuccessful.
 */
RPC_STATUS WINAPI UuidFromStringW(LPWSTR StringUuid, UUID *Uuid)
{
  unsigned i;

  TRACE("%s\n", debugstr_w(StringUuid));

  if(!StringUuid)
    return UuidCreateNil(Uuid);

  if (strlenW(StringUuid) != 36)
    return RPC_S_INVALID_STRING_UUID;

  if (StringUuid[8]!='-' || StringUuid[13]!='-' || StringUuid[18]!='-' || StringUuid[23]!='-')
    return RPC_S_INVALID_STRING_UUID;

  for (i=0; i<36; i++) {
    if (i==8 || i == 13 || i == 18 || i == 23) continue;
    if (hex_d(StringUuid[i]) == -1)
      return RPC_S_INVALID_STRING_UUID;
  }

  Uuid->Data1 = hex4W(StringUuid);
  Uuid->Data2 = hex2W(StringUuid+9);
  Uuid->Data3 = hex2W(StringUuid+14);
  Uuid->Data4[0] = hex1W(StringUuid+19);
  Uuid->Data4[1] = hex1W(StringUuid+21);
  Uuid->Data4[2] = hex1W(StringUuid+24);
  Uuid->Data4[3] = hex1W(StringUuid+26);
  Uuid->Data4[4] = hex1W(StringUuid+28);
  Uuid->Data4[5] = hex1W(StringUuid+30);
  Uuid->Data4[6] = hex1W(StringUuid+32);
  Uuid->Data4[7] = hex1W(StringUuid+34);

  return RPC_S_OK;
}

/***********************************************************************
 *              DllRegisterServer (RPCRT4.@)
 */

HRESULT WINAPI RPCRT4_DllRegisterServer( void )
{
    FIXME( "(): stub\n" );
    return S_OK;
}

BOOL RPCRT4_StartRPCSS(void)
{
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    static char cmd[6];
    BOOL rslt;

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);

    /* apparently it's not OK to use a constant string below */
    CopyMemory(cmd, "rpcss", 6);

    /* FIXME: will this do the right thing when run as a test? */
    rslt = CreateProcessA(
        NULL,           /* executable */
        cmd,            /* command line */
        NULL,           /* process security attributes */
        NULL,           /* primary thread security attributes */
        FALSE,          /* inherit handles */
        0,              /* creation flags */
        NULL,           /* use parent's environment */
        NULL,           /* use parent's current directory */
        &si,            /* STARTUPINFO pointer */
        &pi             /* PROCESS_INFORMATION */
    );

    if (rslt) {
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }

    return rslt;
}

/***********************************************************************
 *           RPCRT4_RPCSSOnDemandCall (internal)
 *
 * Attempts to send a message to the RPCSS process
 * on the local machine, invoking it if necessary.
 * For remote RPCSS calls, use.... your imagination.
 *
 * PARAMS
 *     msg             [I] pointer to the RPCSS message
 *     vardata_payload [I] pointer vardata portion of the RPCSS message
 *     reply           [O] pointer to reply structure
 *
 * RETURNS
 *     TRUE if successful
 *     FALSE otherwise
 */
BOOL RPCRT4_RPCSSOnDemandCall(PRPCSS_NP_MESSAGE msg, char *vardata_payload, PRPCSS_NP_REPLY reply)
{
    HANDLE client_handle;
    int i, j = 0;

    TRACE("(msg == %p, vardata_payload == %p, reply == %p)\n", msg, vardata_payload, reply);

    client_handle = RPCRT4_RpcssNPConnect();

    while (!client_handle) {
        /* start the RPCSS process */
	if (!RPCRT4_StartRPCSS()) {
	    ERR("Unable to start RPCSS process.\n");
	    return FALSE;
	}
	/* wait for a connection (w/ periodic polling) */
        for (i = 0; i < 60; i++) {
            Sleep(200);
            client_handle = RPCRT4_RpcssNPConnect();
            if (client_handle) break;
        }
        /* we are only willing to try twice */
	if (j++ >= 1) break;
    }

    if (!client_handle) {
        /* no dice! */
        ERR("Unable to connect to RPCSS process!\n");
	SetLastError(RPC_E_SERVER_DIED_DNE);
	return FALSE;
    }

    /* great, we're connected.  now send the message */
    if (!RPCRT4_SendReceiveNPMsg(client_handle, msg, vardata_payload, reply)) {
        ERR("Something is amiss: RPC_SendReceive failed.\n");
	return FALSE;
    }

    return TRUE;
}
