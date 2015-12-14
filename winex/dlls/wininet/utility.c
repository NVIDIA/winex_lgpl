/*
 * Wininet - Utility functions
 *
 * Copyright 1999 Corel Corporation
 * Copyright 2002 CodeWeavers Inc.
 *
 * Ulrich Czekalla
 * Aric Stewart
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
#include "wine/port.h"

#if defined(__MINGW32__) || defined (_MSC_VER)
#include <ws2tcpip.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "windef.h"
#include "winbase.h"
#include "wininet.h"
#include "winnls.h"

#include "wine/debug.h"
#include "internet.h"

WINE_DEFAULT_DEBUG_CHANNEL(wininet);

#ifndef HAVE_GETADDRINFO

/* critical section to protect non-reentrant gethostbyname() */
static CRITICAL_SECTION cs_gethostbyname;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &cs_gethostbyname,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": cs_gethostbyname") }
};
static CRITICAL_SECTION cs_gethostbyname = { &critsect_debug, -1, 0, 0, 0, 0 };

#endif

#define TIME_STRING_LEN  30

time_t ConvertTimeString(LPCWSTR asctime)
{
    WCHAR tmpChar[TIME_STRING_LEN];
    WCHAR *tmpChar2;
    struct tm t;
    int timelen = strlenW(asctime);

    if(!timelen)
        return 0;

    /* FIXME: the atoiWs below rely on that tmpChar is \0 padded */
    memset( tmpChar, 0, sizeof(tmpChar) );
    lstrcpynW(tmpChar, asctime, TIME_STRING_LEN);

    /* Assert that the string is the expected length */
    if (strlenW(asctime) >= TIME_STRING_LEN) FIXME("\n");

    /* Convert a time such as 'Mon, 15 Nov 1999 16:09:35 GMT' into a SYSTEMTIME structure
     * We assume the time is in this format
     * and divide it into easy to swallow chunks
     */
    tmpChar[3]='\0';
    tmpChar[7]='\0';
    tmpChar[11]='\0';
    tmpChar[16]='\0';
    tmpChar[19]='\0';
    tmpChar[22]='\0';
    tmpChar[25]='\0';

    memset( &t, 0, sizeof(t) );
    t.tm_year = atoiW(tmpChar+12) - 1900;
    t.tm_mday = atoiW(tmpChar+5);
    t.tm_hour = atoiW(tmpChar+17);
    t.tm_min = atoiW(tmpChar+20);
    t.tm_sec = atoiW(tmpChar+23);

    /* and month */
    tmpChar2 = tmpChar + 8;
    switch(tmpChar2[2])
    {
        case 'n':
            if(tmpChar2[1]=='a')
                t.tm_mon = 0;
            else
                t.tm_mon = 5;
            break;
        case 'b':
            t.tm_mon = 1;
            break;
        case 'r':
            if(tmpChar2[1]=='a')
                t.tm_mon = 2;
            else
                t.tm_mon = 3;
            break;
        case 'y':
            t.tm_mon = 4;
            break;
        case 'l':
            t.tm_mon = 6;
            break;
        case 'g':
            t.tm_mon = 7;
            break;
        case 'p':
            t.tm_mon = 8;
            break;
        case 't':
            t.tm_mon = 9;
            break;
        case 'v':
            t.tm_mon = 10;
            break;
        case 'c':
            t.tm_mon = 11;
            break;
        default:
            FIXME("\n");
    }

    return mktime(&t);
}


BOOL GetAddress(LPCWSTR lpszServerName, INTERNET_PORT nServerPort,
	struct sockaddr *psa, socklen_t *sa_len)
{
    WCHAR *found;
    char *name;
    int len, sz;
#ifdef HAVE_GETADDRINFO
    struct addrinfo *res, hints;
    int ret;
#else
    struct hostent *phe;
    struct sockaddr_in *sin = (struct sockaddr_in *)psa;
#endif

    TRACE("%s\n", debugstr_w(lpszServerName));

    /* Validate server name first
     * Check if there is something like
     * pinger.macromedia.com:80
     * if yes, eliminate the :80....
     */
    found = strchrW(lpszServerName, ':');
    if (found)
        len = found - lpszServerName;
    else
        len = strlenW(lpszServerName);

    sz = WideCharToMultiByte( CP_UNIXCP, 0, lpszServerName, len, NULL, 0, NULL, NULL );
    if (!(name = heap_alloc(sz + 1))) return FALSE;
    WideCharToMultiByte( CP_UNIXCP, 0, lpszServerName, len, name, sz, NULL, NULL );
    name[sz] = 0;

#ifdef HAVE_GETADDRINFO
    memset( &hints, 0, sizeof(struct addrinfo) );
    /* Prefer IPv4 to IPv6 addresses, since some servers do not listen on
     * their IPv6 addresses even though they have IPv6 addresses in the DNS.
     */
    hints.ai_family = AF_INET;

    ret = getaddrinfo( name, NULL, &hints, &res );
    if (ret != 0)
    {
        TRACE("failed to get IPv4 address of %s (%s), retrying with IPv6\n", debugstr_w(lpszServerName), gai_strerror(ret));
        hints.ai_family = AF_INET6;
        ret = getaddrinfo( name, NULL, &hints, &res );
    }
    heap_free( name );
    if (ret != 0)
    {
        TRACE("failed to get address of %s (%s)\n", debugstr_w(lpszServerName), gai_strerror(ret));
        return FALSE;
    }
    if (*sa_len < res->ai_addrlen)
    {
        WARN("address too small\n");
        freeaddrinfo( res );
        return FALSE;
    }
    *sa_len = res->ai_addrlen;
    memcpy( psa, res->ai_addr, res->ai_addrlen );
    /* Copy port */
    switch (res->ai_family)
    {
    case AF_INET:
        ((struct sockaddr_in *)psa)->sin_port = htons(nServerPort);
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *)psa)->sin6_port = htons(nServerPort);
        break;
    }

    freeaddrinfo( res );
#else
    EnterCriticalSection( &cs_gethostbyname );
    phe = gethostbyname(name);
    heap_free( name );

    if (NULL == phe)
    {
        TRACE("failed to get address of %s (%d)\n", debugstr_w(lpszServerName), h_errno);
        LeaveCriticalSection( &cs_gethostbyname );
        return FALSE;
    }
    if (*sa_len < sizeof(struct sockaddr_in))
    {
        WARN("address too small\n");
        LeaveCriticalSection( &cs_gethostbyname );
        return FALSE;
    }
    *sa_len = sizeof(struct sockaddr_in);
    memset(sin,0,sizeof(struct sockaddr_in));
    memcpy((char *)&sin->sin_addr, phe->h_addr, phe->h_length);
    sin->sin_family = phe->h_addrtype;
    sin->sin_port = htons(nServerPort);

    LeaveCriticalSection( &cs_gethostbyname );
#endif
    return TRUE;
}

/*
 * Helper function for sending async Callbacks
 */

static const char *get_callback_name(DWORD dwInternetStatus) {
    static const wininet_flag_info internet_status[] = {
#define FE(x) { x, #x }
	FE(INTERNET_STATUS_RESOLVING_NAME),
	FE(INTERNET_STATUS_NAME_RESOLVED),
	FE(INTERNET_STATUS_CONNECTING_TO_SERVER),
	FE(INTERNET_STATUS_CONNECTED_TO_SERVER),
	FE(INTERNET_STATUS_SENDING_REQUEST),
	FE(INTERNET_STATUS_REQUEST_SENT),
	FE(INTERNET_STATUS_RECEIVING_RESPONSE),
	FE(INTERNET_STATUS_RESPONSE_RECEIVED),
	FE(INTERNET_STATUS_CTL_RESPONSE_RECEIVED),
	FE(INTERNET_STATUS_PREFETCH),
	FE(INTERNET_STATUS_CLOSING_CONNECTION),
	FE(INTERNET_STATUS_CONNECTION_CLOSED),
	FE(INTERNET_STATUS_HANDLE_CREATED),
	FE(INTERNET_STATUS_HANDLE_CLOSING),
	FE(INTERNET_STATUS_REQUEST_COMPLETE),
	FE(INTERNET_STATUS_REDIRECT),
	FE(INTERNET_STATUS_INTERMEDIATE_RESPONSE),
	FE(INTERNET_STATUS_USER_INPUT_REQUIRED),
	FE(INTERNET_STATUS_STATE_CHANGE),
	FE(INTERNET_STATUS_COOKIE_SENT),
	FE(INTERNET_STATUS_COOKIE_RECEIVED),
	FE(INTERNET_STATUS_PRIVACY_IMPACTED),
	FE(INTERNET_STATUS_P3P_HEADER),
	FE(INTERNET_STATUS_P3P_POLICYREF),
	FE(INTERNET_STATUS_COOKIE_HISTORY)
#undef FE
    };
    DWORD i;

    for (i = 0; i < (sizeof(internet_status) / sizeof(internet_status[0])); i++) {
	if (internet_status[i].val == dwInternetStatus) return internet_status[i].name;
    }
    return "Unknown";
}

static const char *debugstr_status_info(DWORD status, void *info)
{
    switch(status) {
    case INTERNET_STATUS_REQUEST_COMPLETE: {
        INTERNET_ASYNC_RESULT *iar = info;
        return wine_dbg_sprintf("{%s, %d}", wine_dbgstr_longlong(iar->dwResult), iar->dwError);
    }
    default:
        return wine_dbg_sprintf("%p", info);
    }
}

void INTERNET_SendCallback(object_header_t *hdr, DWORD_PTR context, DWORD status, void *info, DWORD info_len)
{
    void *new_info = info;

    if( !hdr->lpfnStatusCB )
        return;

    /* the IE5 version of wininet does not
       send callbacks if dwContext is zero */
    if(!context)
        return;

    switch(status) {
    case INTERNET_STATUS_NAME_RESOLVED:
    case INTERNET_STATUS_CONNECTING_TO_SERVER:
    case INTERNET_STATUS_CONNECTED_TO_SERVER:
        new_info = heap_alloc(info_len);
        if(new_info)
            memcpy(new_info, info, info_len);
        break;
    case INTERNET_STATUS_RESOLVING_NAME:
    case INTERNET_STATUS_REDIRECT:
        if(hdr->dwInternalFlags & INET_CALLBACKW) {
            new_info = heap_strdupW(info);
            break;
        }else {
            new_info = heap_strdupWtoA(info);
            info_len = strlen(new_info)+1;
            break;
        }
    }
    
    TRACE(" callback(%p) (%p (%p), %08tx, %d (%s), %s, %d)\n",
	  hdr->lpfnStatusCB, hdr->hInternet, hdr, context, status, get_callback_name(status),
	  debugstr_status_info(status, new_info), info_len);
    
    hdr->lpfnStatusCB(hdr->hInternet, context, status, new_info, info_len);

    TRACE(" end callback().\n");

    if(new_info != info)
        heap_free(new_info);
}

typedef struct {
    task_header_t hdr;
    DWORD_PTR context;
    DWORD     status;
    LPVOID    status_info;
    DWORD     status_info_len;
} send_callback_task_t;

static void SendAsyncCallbackProc(task_header_t *hdr)
{
    send_callback_task_t *task = (send_callback_task_t*)hdr;

    TRACE("%p\n", task->hdr.hdr);

    INTERNET_SendCallback(task->hdr.hdr, task->context, task->status, task->status_info, task->status_info_len);

    /* And frees the copy of the status info */
    heap_free(task->status_info);
}

void SendAsyncCallback(object_header_t *hdr, DWORD_PTR dwContext,
                       DWORD dwInternetStatus, LPVOID lpvStatusInfo,
                       DWORD dwStatusInfoLength)
{
    TRACE("(%p, %08tx, %d (%s), %p, %d): %sasync call with callback %p\n",
	  hdr, dwContext, dwInternetStatus, get_callback_name(dwInternetStatus),
	  lpvStatusInfo, dwStatusInfoLength,
	  hdr->dwFlags & INTERNET_FLAG_ASYNC ? "" : "non ",
	  hdr->lpfnStatusCB);
    
    if (!(hdr->lpfnStatusCB))
	return;
    
    if (hdr->dwFlags & INTERNET_FLAG_ASYNC)
    {
        send_callback_task_t *task;
        void *lpvStatusInfo_copy = lpvStatusInfo;

        if (lpvStatusInfo)
        {
            lpvStatusInfo_copy = heap_alloc(dwStatusInfoLength);
            memcpy(lpvStatusInfo_copy, lpvStatusInfo, dwStatusInfoLength);
        }

        task = alloc_async_task(hdr, SendAsyncCallbackProc, sizeof(*task));
        task->context = dwContext;
        task->status = dwInternetStatus;
        task->status_info = lpvStatusInfo_copy;
        task->status_info_len = dwStatusInfoLength;
	
        INTERNET_AsyncCall(&task->hdr);
    }
    else
	INTERNET_SendCallback(hdr, dwContext, dwInternetStatus,
			      lpvStatusInfo, dwStatusInfoLength);
}
