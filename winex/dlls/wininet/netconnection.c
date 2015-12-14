/*
 * Wininet - networking layer. Uses unix sockets or OpenSSL.
 *
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * David Hammerton
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

#include <sys/types.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#include <time.h>
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include "wine/openssl.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "wine/library.h"
#include "windef.h"
#include "winbase.h"
#include "wininet.h"
#include "winerror.h"
#include "wincrypt.h"

#include "wine/debug.h"
#include "internet.h"

/* To avoid conflicts with the Unix socket headers. we only need it for
 * the error codes anyway. */
#define USE_WS_PREFIX
#include "winsock2.h"

#define RESPONSE_TIMEOUT        30            /* FROM internet.c */


WINE_DEFAULT_DEBUG_CHANNEL(wininet);

/* FIXME!!!!!!
 *    This should use winsock - To use winsock the functions will have to change a bit
 *        as they are designed for unix sockets.
 *    SSL stuff should use crypt32.dll
 */

#ifdef SSL_AVAILABLE
static SSL_METHOD *meth;
static SSL_CTX *ctx;
#endif

BOOL NETCON_init(WININET_NETCONNECTION *connection, BOOL useSSL)
{
    int ret;

    connection->useSSL = FALSE;
    connection->socketFD = -1;
    if (useSSL)
    {
#ifdef SSL_AVAILABLE
        ret = OPENSSL_loadOpenSSL();

        if (ret != 0)
        {
            switch (ret)
            {
                /* failed to load the SSL library */
                case OPENSSL_ERR_SSL_LOAD_FAILED:
                    MESSAGE("libssl could not be loaded.\n"
                            "\tThis is not necessarily a fatal error.  However, some\n"
                            "\tfeatures may not work properly without it.\n");
                    break;

                /* failed to load the crypto library */
                case OPENSSL_ERR_CRYPTO_LOAD_FAILED:
                    MESSAGE("libcrypto could not be loaded.\n"
                            "\tThis is not necessarily a fatal error.  However, some\n"
                            "\tfeatures may not work properly without it.\n");
                    break;

                /* failed to load at least one function */
                default:
                    MESSAGE("failed to load at least one function from libssl or libcrypto (missing %d function%s)\n",
                            ret, ret == 1 ? "" : "s");
                    break;
            }

            INTERNET_SetLastError(ERROR_INTERNET_SECURITY_CHANNEL_ERROR);
            return FALSE;
        }

        BIO_new_fp(stderr, BIO_NOCLOSE); /* FIXME: should use winedebug stuff */
        meth = SSLv23_method();
        connection->peek_msg = NULL;
        connection->peek_msg_mem = NULL;
#else
        FIXME("can't use SSL, not compiled in.\n");
        INTERNET_SetLastError(ERROR_INTERNET_SECURITY_CHANNEL_ERROR);
        return FALSE;
#endif
    }
    return TRUE;
}

BOOL NETCON_connected(WININET_NETCONNECTION *connection)
{
    if (connection->socketFD == -1)
        return FALSE;
    else
        return TRUE;
}

/* translate a unix error code into a winsock one */
static int sock_get_error( int err )
{
#if !defined(__MINGW32__) && !defined (_MSC_VER)
    switch (err)
    {
        case EINTR:             return WSAEINTR;
        case EBADF:             return WSAEBADF;
        case EPERM:
        case EACCES:            return WSAEACCES;
        case EFAULT:            return WSAEFAULT;
        case EINVAL:            return WSAEINVAL;
        case EMFILE:            return WSAEMFILE;
        case EWOULDBLOCK:       return WSAEWOULDBLOCK;
        case EINPROGRESS:       return WSAEINPROGRESS;
        case EALREADY:          return WSAEALREADY;
        case ENOTSOCK:          return WSAENOTSOCK;
        case EDESTADDRREQ:      return WSAEDESTADDRREQ;
        case EMSGSIZE:          return WSAEMSGSIZE;
        case EPROTOTYPE:        return WSAEPROTOTYPE;
        case ENOPROTOOPT:       return WSAENOPROTOOPT;
        case EPROTONOSUPPORT:   return WSAEPROTONOSUPPORT;
        case ESOCKTNOSUPPORT:   return WSAESOCKTNOSUPPORT;
        case EOPNOTSUPP:        return WSAEOPNOTSUPP;
        case EPFNOSUPPORT:      return WSAEPFNOSUPPORT;
        case EAFNOSUPPORT:      return WSAEAFNOSUPPORT;
        case EADDRINUSE:        return WSAEADDRINUSE;
        case EADDRNOTAVAIL:     return WSAEADDRNOTAVAIL;
        case ENETDOWN:          return WSAENETDOWN;
        case ENETUNREACH:       return WSAENETUNREACH;
        case ENETRESET:         return WSAENETRESET;
        case ECONNABORTED:      return WSAECONNABORTED;
        case EPIPE:
        case ECONNRESET:        return WSAECONNRESET;
        case ENOBUFS:           return WSAENOBUFS;
        case EISCONN:           return WSAEISCONN;
        case ENOTCONN:          return WSAENOTCONN;
        case ESHUTDOWN:         return WSAESHUTDOWN;
        case ETOOMANYREFS:      return WSAETOOMANYREFS;
        case ETIMEDOUT:         return WSAETIMEDOUT;
        case ECONNREFUSED:      return WSAECONNREFUSED;
        case ELOOP:             return WSAELOOP;
        case ENAMETOOLONG:      return WSAENAMETOOLONG;
        case EHOSTDOWN:         return WSAEHOSTDOWN;
        case EHOSTUNREACH:      return WSAEHOSTUNREACH;
        case ENOTEMPTY:         return WSAENOTEMPTY;
#ifdef EPROCLIM
        case EPROCLIM:          return WSAEPROCLIM;
#endif
#ifdef EUSERS
        case EUSERS:            return WSAEUSERS;
#endif
#ifdef EDQUOT
        case EDQUOT:            return WSAEDQUOT;
#endif
#ifdef ESTALE
        case ESTALE:            return WSAESTALE;
#endif
#ifdef EREMOTE
        case EREMOTE:           return WSAEREMOTE;
#endif
    default: errno=err; perror("sock_set_error"); return WSAEFAULT;
    }
#endif
    return err;
}

/******************************************************************************
 * NETCON_create
 * Basically calls 'socket()'
 */
BOOL NETCON_create(WININET_NETCONNECTION *connection, int domain,
	      int type, int protocol)
{
#ifdef SSL_AVAILABLE
    if (connection->useSSL)
        return FALSE;
#endif

    connection->socketFD = socket(domain, type, protocol);
    if (connection->socketFD == -1)
    {
        INTERNET_SetLastError(sock_get_error(errno));
        return FALSE;
    }
    return TRUE;
}

/******************************************************************************
 * NETCON_close
 * Basically calls 'close()' unless we should use SSL
 */
BOOL NETCON_close(WININET_NETCONNECTION *connection)
{
    int result;

    if (!NETCON_connected(connection)) return FALSE;

#ifdef SSL_AVAILABLE
    if (connection->useSSL)
    {
        HeapFree(GetProcessHeap(),0,connection->peek_msg_mem);
        connection->peek_msg = NULL;
        connection->peek_msg_mem = NULL;
        connection->peek_len = 0;

        SSL_shutdown(connection->ssl_s);
        SSL_free(connection->ssl_s);
        connection->ssl_s = NULL;

        connection->useSSL = FALSE;
    }
#endif

    result = closesocket(connection->socketFD);
    connection->socketFD = -1;

    if (result == -1)
    {
        INTERNET_SetLastError(sock_get_error(errno));
        return FALSE;
    }
    return TRUE;
}
#ifdef SSL_AVAILABLE
static BOOL check_hostname(X509 *cert, char *hostname)
{
    /* FIXME: implement */
    return TRUE;
}
#endif
/******************************************************************************
 * NETCON_secure_connect
 * Initiates a secure connection over an existing plaintext connection.
 */
BOOL NETCON_secure_connect(WININET_NETCONNECTION *connection, LPCWSTR hostname)
{
#ifdef SSL_AVAILABLE
    long verify_res;
    X509 *cert;
    int len;
    char *hostname_unix;

    /* can't connect if we are already connected */
    if (connection->useSSL)
    {
        ERR("already connected\n");
        return FALSE;
    }

    ctx = SSL_CTX_new(meth);
    if (!SSL_CTX_set_default_verify_paths(ctx))
    {
        ERR("SSL_CTX_set_default_verify_paths failed: %s\n",
            ERR_error_string(ERR_get_error(), 0));
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    connection->ssl_s = SSL_new(ctx);
    if (!connection->ssl_s)
    {
        ERR("SSL_new failed: %s\n",
            ERR_error_string(ERR_get_error(), 0));
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        goto fail;
    }

    if (!SSL_set_fd(connection->ssl_s, connection->socketFD))
    {
        ERR("SSL_set_fd failed: %s\n",
            ERR_error_string(ERR_get_error(), 0));
        INTERNET_SetLastError(ERROR_INTERNET_SECURITY_CHANNEL_ERROR);
        goto fail;
    }

    if (SSL_connect(connection->ssl_s) <= 0)
    {
        ERR("SSL_connect failed: %s\n",
            ERR_error_string(ERR_get_error(), 0));
        INTERNET_SetLastError(ERROR_INTERNET_SECURITY_CHANNEL_ERROR);
        goto fail;
    }
    cert = SSL_get_peer_certificate(connection->ssl_s);
    if (!cert)
    {
        ERR("no certificate for server %s\n", debugstr_w(hostname));
        /* FIXME: is this the best error? */
        INTERNET_SetLastError(ERROR_INTERNET_INVALID_CA);
        goto fail;
    }
    verify_res = SSL_get_verify_result(connection->ssl_s);
    if (verify_res != X509_V_OK)
    {
        ERR("couldn't verify the security of the connection, %ld\n", verify_res);
        /* FIXME: we should set an error and return, but we only warn at
         * the moment */
    }

    len = WideCharToMultiByte(CP_UNIXCP, 0, hostname, -1, NULL, 0, NULL, NULL);
    hostname_unix = HeapAlloc(GetProcessHeap(), 0, len);
    if (!hostname_unix)
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        goto fail;
    }
    WideCharToMultiByte(CP_UNIXCP, 0, hostname, -1, hostname_unix, len, NULL, NULL);

    if (!check_hostname(cert, hostname_unix))
    {
        HeapFree(GetProcessHeap(), 0, hostname_unix);
        INTERNET_SetLastError(ERROR_INTERNET_SEC_CERT_CN_INVALID);
        goto fail;
    }

    HeapFree(GetProcessHeap(), 0, hostname_unix);
    connection->useSSL = TRUE;
    return TRUE;

fail:
    if (connection->ssl_s)
    {
        SSL_shutdown(connection->ssl_s);
        SSL_free(connection->ssl_s);
        connection->ssl_s = NULL;
    }
#endif
    return FALSE;
}

/******************************************************************************
 * NETCON_connect
 * Connects to the specified address.
 */
BOOL NETCON_connect(WININET_NETCONNECTION *connection, const struct sockaddr *serv_addr,
		    unsigned int addrlen)
{
    int result;

    if (!NETCON_connected(connection)) return FALSE;

    result = connect(connection->socketFD, serv_addr, addrlen);
    if (result == -1)
    {
        WARN("Unable to connect to host (%s)\n", strerror(errno));
        INTERNET_SetLastError(sock_get_error(errno));

        closesocket(connection->socketFD);
        connection->socketFD = -1;
        return FALSE;
    }

    return TRUE;
}

/******************************************************************************
 * NETCON_send
 * Basically calls 'send()' unless we should use SSL
 * number of chars send is put in *sent
 */
BOOL NETCON_send(WININET_NETCONNECTION *connection, const void *msg, size_t len, int flags,
		int *sent /* out */)
{
    if (!NETCON_connected(connection)) return FALSE;
    if (!connection->useSSL)
    {
	*sent = send(connection->socketFD, msg, len, flags);
	if (*sent == -1)
	{
	    INTERNET_SetLastError(sock_get_error(errno));
	    return FALSE;
	}
        return TRUE;
    }
    else
    {
#ifdef SSL_AVAILABLE
	if (flags)
            FIXME("SSL_write doesn't support any flags (%08x)\n", flags);
	*sent = SSL_write(connection->ssl_s, msg, len);
	if (*sent < 1 && len)
	    return FALSE;
        return TRUE;
#else
	return FALSE;
#endif
    }
}

/******************************************************************************
 * NETCON_recv
 * Basically calls 'recv()' unless we should use SSL
 * number of chars received is put in *recvd
 */
BOOL NETCON_recv(WININET_NETCONNECTION *connection, void *buf, size_t len, int flags,
		int *recvd /* out */)
{
    *recvd = 0;
    if (!NETCON_connected(connection)) return FALSE;
    if (!len)
        return TRUE;
    if (!connection->useSSL)
    {
	*recvd = recv(connection->socketFD, buf, len, flags);
	if (*recvd == -1)
	{
	    INTERNET_SetLastError(sock_get_error(errno));
	    return FALSE;
	}
        return TRUE;
    }
    else
    {
#ifdef SSL_AVAILABLE
        size_t peek_read = 0, read;

	if (flags & ~(MSG_PEEK|MSG_WAITALL))
	    FIXME("SSL_read does not support the following flag: %08x\n", flags);

        /* this ugly hack is all for MSG_PEEK. eww gross */
        if(connection->peek_msg) {
            if(connection->peek_len >= len) {
                memcpy(buf, connection->peek_msg, len);
                if(!(flags & MSG_PEEK)) {
                    if(connection->peek_len == len) {
                        HeapFree(GetProcessHeap(), 0, connection->peek_msg);
                        connection->peek_msg = NULL;
                        connection->peek_len = 0;
                    }else {
                        memmove(connection->peek_msg, connection->peek_msg+len, connection->peek_len-len);
                        connection->peek_len -= len;
                        connection->peek_msg = HeapReAlloc(GetProcessHeap(), 0, connection->peek_msg, connection->peek_len);
                    }
                }

                *recvd = len;
                return TRUE;
            }

            memcpy(buf, connection->peek_msg, connection->peek_len);
            peek_read = connection->peek_len;

            if(!(flags & MSG_PEEK)) {
                HeapFree(GetProcessHeap(), 0, connection->peek_msg);
                connection->peek_msg = NULL;
                connection->peek_len = 0;
            }
        }

	read = SSL_read(connection->ssl_s, (BYTE*)buf+peek_read, len-peek_read);

        if(flags & MSG_PEEK) {
            if(connection->peek_msg)
                connection->peek_msg = HeapReAlloc(GetProcessHeap(), 0, connection->peek_msg,
                                                   connection->peek_len+read);
            else
                connection->peek_msg = HeapAlloc(GetProcessHeap(), 0, read);
            memcpy(connection->peek_msg+connection->peek_len, (BYTE*)buf+peek_read, read);
            connection->peek_len += read;
        }

        *recvd = read + peek_read;
	return *recvd > 0 || !len;
#else
	return FALSE;
#endif
    }
}

/******************************************************************************
 * NETCON_query_data_available
 * Returns the number of bytes of peeked data plus the number of bytes of
 * queued, but unread data.
 */
BOOL NETCON_query_data_available(WININET_NETCONNECTION *connection, DWORD *available)
{
    *available = 0;
    if (!NETCON_connected(connection))
        return FALSE;

#ifdef SSL_AVAILABLE
    if (connection->peek_msg) *available = connection->peek_len + SSL_pending(connection->ssl_s);
#endif

#ifdef FIONREAD
    if (!connection->useSSL)
    {
        int unread;
        int retval = ioctlsocket(connection->socketFD, FIONREAD, &unread);
        if (!retval)
        {
            TRACE("%d bytes of queued, but unread data\n", unread);
            *available += unread;
        }
    }
#endif
    return TRUE;
}

/******************************************************************************
 * NETCON_getNextLine
 */
BOOL NETCON_getNextLine(WININET_NETCONNECTION *connection, LPSTR lpszBuffer, LPDWORD dwBuffer)
{

    TRACE("\n");

    if (!NETCON_connected(connection)) return FALSE;

    if (!connection->useSSL)
    {
        struct pollfd pfd;
	DWORD nRecv = 0;
        int ret;

        pfd.fd = connection->socketFD;
        pfd.events = POLLIN;

	while (nRecv < *dwBuffer)
	{
	    if (poll(&pfd,1, RESPONSE_TIMEOUT * 1000) > 0)
	    {
		if ((ret = recv(connection->socketFD, &lpszBuffer[nRecv], 1, 0)) <= 0)
		{
		    if (ret == -1) INTERNET_SetLastError(sock_get_error(errno));
                    break;
		}

		if (lpszBuffer[nRecv] == '\n')
		{
                    lpszBuffer[nRecv++] = '\0';
                    *dwBuffer = nRecv;
                    TRACE(":%u %s\n", nRecv, debugstr_a(lpszBuffer));
                    return TRUE;
		}
		if (lpszBuffer[nRecv] != '\r')
		    nRecv++;
	    }
	    else
	    {
		INTERNET_SetLastError(ERROR_INTERNET_TIMEOUT);
                break;
	    }
	}
    }
    else
    {
#ifdef SSL_AVAILABLE
	long prev_timeout;
	DWORD nRecv = 0;
        BOOL success = TRUE;

        prev_timeout = SSL_CTX_get_timeout(ctx);
	SSL_CTX_set_timeout(ctx, RESPONSE_TIMEOUT);

	while (nRecv < *dwBuffer)
	{
	    int recv = 1;
	    if (!NETCON_recv(connection, &lpszBuffer[nRecv], 1, 0, &recv))
	    {
                INTERNET_SetLastError(ERROR_CONNECTION_ABORTED);
		success = FALSE;
	    }

	    if (lpszBuffer[nRecv] == '\n')
	    {
		success = TRUE;
                break;
	    }
	    if (lpszBuffer[nRecv] != '\r')
		nRecv++;
	}

        SSL_CTX_set_timeout(ctx, prev_timeout);
	if (success)
	{
	    lpszBuffer[nRecv++] = '\0';
	    *dwBuffer = nRecv;
	    TRACE("_SSL:%u %s\n", nRecv, debugstr_a(lpszBuffer));
            return TRUE;
	}
#endif
    }
    return FALSE;
}


LPCVOID NETCON_GetCert(WININET_NETCONNECTION *connection)
{
#ifdef SSL_AVAILABLE
    X509* cert;
    unsigned char* buffer,*p;
    INT len;
    BOOL malloced = FALSE;
    LPCVOID r = NULL;

    if (!connection->useSSL)
        return NULL;

    cert = SSL_get_peer_certificate(connection->ssl_s);
    p = NULL;
    len = i2d_X509(cert,&p);
    /*
     * SSL 0.9.7 and above malloc the buffer if it is null. 
     * however earlier version do not and so we would need to alloc the buffer.
     *
     * see the i2d_X509 man page for more details.
     */
    if (!p)
    {
        buffer = HeapAlloc(GetProcessHeap(),0,len);
        p = buffer;
        len = i2d_X509(cert,&p);
    }
    else
    {
        buffer = p;
        malloced = TRUE;
    }

    r = CertCreateCertificateContext(X509_ASN_ENCODING,buffer,len);

    if (malloced)
        free(buffer);
    else
        HeapFree(GetProcessHeap(),0,buffer);

    return r;
#else
    return NULL;
#endif
}

DWORD NETCON_set_timeout(WININET_NETCONNECTION *connection, BOOL send, int value)
{
    int result;
    struct timeval tv;

    /* FIXME: we should probably store the timeout in the connection to set
     * when we do connect */
    if (!NETCON_connected(connection))
        return ERROR_SUCCESS;

    /* value is in milliseconds, convert to struct timeval */
    tv.tv_sec = value / 1000;
    tv.tv_usec = (value % 1000) * 1000;

    result = setsockopt(connection->socketFD, SOL_SOCKET,
                        send ? SO_SNDTIMEO : SO_RCVTIMEO, (void*)&tv,
                        sizeof(tv));

    if (result == -1)
    {
        WARN("setsockopt failed (%s)\n", strerror(errno));
        return sock_get_error(errno);
    }

    return ERROR_SUCCESS;
}
