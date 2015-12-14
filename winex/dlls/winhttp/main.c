/*
 * Copyright 2007 Jacek Caban for CodeWeavers
 * Copyright 2007 Hans Leidekker
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "winhttp_private.h"

#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(winhttp);

static inline WCHAR *winhttp_strdup( const WCHAR *src )
{
    WCHAR *dst;

    if (!src) return NULL;
    if ((dst = HeapAlloc( GetProcessHeap(), 0, (strlenW( src ) + 1) * sizeof(WCHAR) )))
        strcpyW( dst, src );
    return dst;
}

#define QUERY_MODIFIER_FLAGS_MASK ( WINHTTP_QUERY_FLAG_REQUEST_HEADERS \
                                  | WINHTTP_QUERY_FLAG_SYSTEMTIME      \
                                  | WINHTTP_QUERY_FLAG_NUMBER          \
                                  )
#define QUERY_HEADER_MASK (~QUERY_MODIFIER_FLAGS_MASK)

static DWORD map_info_level(DWORD level)
{
    DWORD index, ret;

    static const DWORD info_level[] =
    {
        HTTP_QUERY_MIME_VERSION,                HTTP_QUERY_CONTENT_TYPE,
        HTTP_QUERY_CONTENT_TRANSFER_ENCODING,   HTTP_QUERY_CONTENT_ID,
        HTTP_QUERY_CONTENT_DESCRIPTION,         HTTP_QUERY_CONTENT_LENGTH,
        HTTP_QUERY_CONTENT_LANGUAGE,            HTTP_QUERY_ALLOW,
        HTTP_QUERY_PUBLIC,                      HTTP_QUERY_DATE,
        HTTP_QUERY_EXPIRES,                     HTTP_QUERY_LAST_MODIFIED,
        HTTP_QUERY_MESSAGE_ID,                  HTTP_QUERY_URI,
        HTTP_QUERY_DERIVED_FROM,                HTTP_QUERY_COST,
        HTTP_QUERY_LINK,                        HTTP_QUERY_PRAGMA,
        HTTP_QUERY_VERSION,                     HTTP_QUERY_STATUS_CODE,
        HTTP_QUERY_STATUS_TEXT,                 HTTP_QUERY_RAW_HEADERS,
        HTTP_QUERY_RAW_HEADERS_CRLF,            HTTP_QUERY_CONNECTION,
        HTTP_QUERY_ACCEPT,                      HTTP_QUERY_ACCEPT_CHARSET,
        HTTP_QUERY_ACCEPT_ENCODING,             HTTP_QUERY_ACCEPT_LANGUAGE,
        HTTP_QUERY_AUTHORIZATION,               HTTP_QUERY_CONTENT_ENCODING,
        HTTP_QUERY_FORWARDED,                   HTTP_QUERY_FROM,
        HTTP_QUERY_IF_MODIFIED_SINCE,           HTTP_QUERY_LOCATION,
        HTTP_QUERY_ORIG_URI,                    HTTP_QUERY_REFERER,
        HTTP_QUERY_RETRY_AFTER,                 HTTP_QUERY_SERVER,
        HTTP_QUERY_TITLE,                       HTTP_QUERY_USER_AGENT,
        HTTP_QUERY_WWW_AUTHENTICATE,            HTTP_QUERY_PROXY_AUTHENTICATE,
        HTTP_QUERY_ACCEPT_RANGES,               HTTP_QUERY_SET_COOKIE,
        HTTP_QUERY_COOKIE,                      HTTP_QUERY_REQUEST_METHOD,
        HTTP_QUERY_REFRESH,                     HTTP_QUERY_CONTENT_DISPOSITION,
        HTTP_QUERY_AGE,                         HTTP_QUERY_CACHE_CONTROL,
        HTTP_QUERY_CONTENT_BASE,                HTTP_QUERY_CONTENT_LOCATION,
        HTTP_QUERY_CONTENT_MD5,                 HTTP_QUERY_CONTENT_RANGE,
        HTTP_QUERY_ETAG,                        HTTP_QUERY_HOST,
        HTTP_QUERY_IF_MATCH,                    HTTP_QUERY_IF_NONE_MATCH,
        HTTP_QUERY_IF_RANGE,                    HTTP_QUERY_IF_UNMODIFIED_SINCE,
        HTTP_QUERY_MAX_FORWARDS,                HTTP_QUERY_PROXY_AUTHORIZATION,
        HTTP_QUERY_RANGE,                       HTTP_QUERY_TRANSFER_ENCODING,
        HTTP_QUERY_UPGRADE,                     HTTP_QUERY_VARY,
        HTTP_QUERY_VIA,                         HTTP_QUERY_WARNING,
        HTTP_QUERY_EXPECT,                      HTTP_QUERY_PROXY_CONNECTION,
        HTTP_QUERY_UNLESS_MODIFIED_SINCE,       HTTP_QUERY_PROXY_SUPPORT,
        HTTP_QUERY_AUTHENTICATION_INFO,         HTTP_QUERY_PASSPORT_URLS,
        HTTP_QUERY_PASSPORT_CONFIG
    };

    index = level & QUERY_HEADER_MASK;
    if (index < sizeof(info_level) / sizeof(info_level[0])) ret = info_level[index];
    else if (level == WINHTTP_QUERY_CUSTOM) ret = HTTP_QUERY_CUSTOM;
    else
    {
        WARN("unknown info level: %u\n", level);
        return level;
    }
    if (level & WINHTTP_QUERY_FLAG_REQUEST_HEADERS) ret |= HTTP_QUERY_FLAG_REQUEST_HEADERS;
    if (level & WINHTTP_QUERY_FLAG_SYSTEMTIME)      ret |= HTTP_QUERY_FLAG_SYSTEMTIME;
    if (level & WINHTTP_QUERY_FLAG_NUMBER)          ret |= HTTP_QUERY_FLAG_NUMBER;
    return ret;
}

static DWORD map_option(DWORD option, BOOL *retval)
{
    DWORD opt;

    *retval = TRUE;
    switch (option)
    {
    case WINHTTP_OPTION_CALLBACK:               opt = INTERNET_OPTION_CALLBACK; break;
    case WINHTTP_OPTION_CONNECT_TIMEOUT:        opt = INTERNET_OPTION_CONNECT_TIMEOUT; break;
    case WINHTTP_OPTION_CONNECT_RETRIES:        opt = INTERNET_OPTION_CONNECT_RETRIES; break;
    case WINHTTP_OPTION_SEND_TIMEOUT:           opt = INTERNET_OPTION_SEND_TIMEOUT; break;
    case WINHTTP_OPTION_RECEIVE_TIMEOUT:        opt = INTERNET_OPTION_RECEIVE_TIMEOUT; break;
    case WINHTTP_OPTION_HANDLE_TYPE:            opt = INTERNET_OPTION_HANDLE_TYPE; break;
    case WINHTTP_OPTION_READ_BUFFER_SIZE:       opt = INTERNET_OPTION_READ_BUFFER_SIZE; break;
    case WINHTTP_OPTION_WRITE_BUFFER_SIZE:      opt = INTERNET_OPTION_WRITE_BUFFER_SIZE; break;
    case WINHTTP_OPTION_PARENT_HANDLE:          opt = INTERNET_OPTION_PARENT_HANDLE; break;
    case WINHTTP_OPTION_EXTENDED_ERROR:         opt = INTERNET_OPTION_EXTENDED_ERROR; break;
    case WINHTTP_OPTION_SECURITY_FLAGS:         opt = INTERNET_OPTION_SECURITY_FLAGS; break;
    case WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT:    opt = INTERNET_OPTION_SECURITY_CERTIFICATE_STRUCT; break;
    case WINHTTP_OPTION_URL:                    opt = INTERNET_OPTION_URL; break;
    case WINHTTP_OPTION_SECURITY_KEY_BITNESS:   opt = INTERNET_OPTION_SECURITY_KEY_BITNESS; break;
    case WINHTTP_OPTION_PROXY:                  opt = INTERNET_OPTION_PROXY; break;
    case WINHTTP_OPTION_USER_AGENT:             opt = INTERNET_OPTION_USER_AGENT; break;
    case WINHTTP_OPTION_CONTEXT_VALUE:          opt = INTERNET_OPTION_CONTEXT_VALUE; break;
    case WINHTTP_OPTION_CLIENT_CERT_CONTEXT:    opt = INTERNET_OPTION_SECURITY_SELECT_CLIENT_CERT; break;
    case WINHTTP_OPTION_REQUEST_PRIORITY:       opt = INTERNET_OPTION_REQUEST_PRIORITY; break;
    case WINHTTP_OPTION_HTTP_VERSION:           opt = INTERNET_OPTION_HTTP_VERSION; break;
    case WINHTTP_OPTION_CODEPAGE:               opt = INTERNET_OPTION_CODEPAGE; break;
    case WINHTTP_OPTION_MAX_CONNS_PER_SERVER:   opt = INTERNET_OPTION_MAX_CONNS_PER_SERVER; break;
    case WINHTTP_OPTION_MAX_CONNS_PER_1_0_SERVER:   opt = INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER; break;
    case WINHTTP_OPTION_USERNAME:               opt = INTERNET_OPTION_USERNAME; break;
    case WINHTTP_OPTION_PASSWORD:               opt = INTERNET_OPTION_PASSWORD; break;
    case WINHTTP_OPTION_PROXY_USERNAME:         opt = INTERNET_OPTION_PROXY_USERNAME; break;
    case WINHTTP_OPTION_PROXY_PASSWORD:         opt = INTERNET_OPTION_PROXY_PASSWORD; break;


    case WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT:
    case WINHTTP_OPTION_RESOLVE_TIMEOUT:
    case WINHTTP_OPTION_DISABLE_FEATURE:
    case WINHTTP_OPTION_AUTOLOGON_POLICY:
    case WINHTTP_OPTION_SERVER_CERT_CONTEXT:
    case WINHTTP_OPTION_ENABLE_FEATURE:
    case WINHTTP_OPTION_WORKER_THREAD_COUNT:
    case WINHTTP_OPTION_PASSPORT_COBRANDING_TEXT:
    case WINHTTP_OPTION_PASSPORT_COBRANDING_URL:
    case WINHTTP_OPTION_CONFIGURE_PASSPORT_AUTH:
    case WINHTTP_OPTION_SECURE_PROTOCOLS:
    case WINHTTP_OPTION_ENABLETRACING:
    case WINHTTP_OPTION_PASSPORT_SIGN_OUT:
    case WINHTTP_OPTION_PASSPORT_RETURN_URL:
    case WINHTTP_OPTION_REDIRECT_POLICY:
    case WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS:
    case WINHTTP_OPTION_MAX_HTTP_STATUS_CONTINUE:
    case WINHTTP_OPTION_MAX_RESPONSE_HEADER_SIZE:
    case WINHTTP_OPTION_MAX_RESPONSE_DRAIN_SIZE:
        FIXME("option not supported: %x\n", option);
        return 0;
    default:
        WARN("unknown option: %x\n", option);
        *retval = FALSE;
        return 0;
    }
    return opt;
}

static DWORD map_open_flags(DWORD flags)
{
    DWORD ret = 0;

    if (flags & WINHTTP_FLAG_ASYNC) ret |= INTERNET_FLAG_ASYNC;
    return ret;
}

static DWORD map_req_flags(DWORD flags)
{
    DWORD ret = 0;

    if (flags & WINHTTP_FLAG_SECURE)               ret |= INTERNET_FLAG_SECURE;
    if (flags & WINHTTP_FLAG_BYPASS_PROXY_CACHE)   ret |= INTERNET_FLAG_PRAGMA_NOCACHE;

    if (flags & WINHTTP_FLAG_ESCAPE_PERCENT)       FIXME("WINHTTP_FLAG_ESCAPE_PERCENT not implemented\n");
    if (flags & WINHTTP_FLAG_NULL_CODEPAGE)        FIXME("WINHTTP_FLAG_NULL_CODEPAGE not implemented\n");
    if (flags & WINHTTP_FLAG_ESCAPE_DISABLE)       FIXME("WINHTTP_FLAG_ESCAPE_DISABLE not implemented\n");
    if (flags & WINHTTP_FLAG_ESCAPE_DISABLE_QUERY) FIXME("WINHTTP_FLAG_ESCAPE_DISABLE_QUERY not implemented\n");
    return ret;
}

static DWORD map_req_modifiers(DWORD modifiers)
{
    DWORD ret = 0;

    if (modifiers & WINHTTP_ADDREQ_FLAG_ADD)                     ret |= HTTP_ADDREQ_FLAG_ADD;
    if (modifiers & WINHTTP_ADDREQ_FLAG_ADD_IF_NEW)              ret |= HTTP_ADDREQ_FLAG_ADD_IF_NEW;
    if (modifiers & WINHTTP_ADDREQ_FLAG_COALESCE)                ret |= HTTP_ADDREQ_FLAG_COALESCE;
    if (modifiers & WINHTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA)     ret |= HTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA;
    if (modifiers & WINHTTP_ADDREQ_FLAG_COALESCE_WITH_SEMICOLON) ret |= HTTP_ADDREQ_FLAG_COALESCE_WITH_SEMICOLON;
    if (modifiers & WINHTTP_ADDREQ_FLAG_REPLACE)                 ret |= HTTP_ADDREQ_FLAG_REPLACE;
    return ret;
}

/***********************************************************************
 *  HTTP_EncodeBase64 (taken from dlls/wininet/http.c)
 */
static UINT HTTP_EncodeBase64( LPCSTR bin, unsigned int len, LPWSTR base64 )
{
    UINT n = 0, x;
    static const CHAR HTTP_Base64Enc[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    while( len > 0 )
    {
        /* first 6 bits, all from bin[0] */
        base64[n++] = HTTP_Base64Enc[(bin[0] & 0xfc) >> 2];
        x = (bin[0] & 3) << 4;

        /* next 6 bits, 2 from bin[0] and 4 from bin[1] */
        if( len == 1 )
        {
            base64[n++] = HTTP_Base64Enc[x];
            base64[n++] = '=';
            base64[n++] = '=';
            break;
        }
        base64[n++] = HTTP_Base64Enc[ x | ( (bin[1]&0xf0) >> 4 ) ];
        x = ( bin[1] & 0x0f ) << 2;

        /* next 6 bits 4 from bin[1] and 2 from bin[2] */
        if( len == 2 )
        {
            base64[n++] = HTTP_Base64Enc[x];
            base64[n++] = '=';
            break;
        }
        base64[n++] = HTTP_Base64Enc[ x | ( (bin[2]&0xc0 ) >> 6 ) ];

        /* last 6 bits, all from bin [2] */
        base64[n++] = HTTP_Base64Enc[ bin[2] & 0x3f ];
        bin += 3;
        len -= 3;
    }
    base64[n] = 0;
    return n;
}

/******************************************************************
 *              DllMain (winhttp.@)
 */
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch(fdwReason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDLL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

/******************************************************************
 *		DllGetClassObject (winhttp.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    FIXME("(%s %s %p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

/******************************************************************
 *              DllCanUnloadNow (winhttp.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    FIXME("()\n");
    return S_FALSE;
}

/***********************************************************************
 *          DllRegisterServer (winhttp.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    FIXME("()\n");
    return S_OK;
}

/***********************************************************************
 *          DllUnregisterServer (winhttp.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    FIXME("()\n");
    return S_OK;
}

/***********************************************************************
 *          WinHttpAddRequestHeaders (winhttp.@)
 */
BOOL WINAPI WinHttpAddRequestHeaders(HINTERNET request, LPCWSTR headers, DWORD len, DWORD modifiers)
{
    TRACE("(%p %s %u %x)\n", request, debugstr_w(headers), len, modifiers);
    return HttpAddRequestHeadersW(request, headers, len, map_req_modifiers(modifiers));
}

/***********************************************************************
 *          WinHttpCheckPlatform (winhttp.@)
 */
BOOL WINAPI WinHttpCheckPlatform(void)
{
    TRACE("()\n");
    return TRUE;
}

/***********************************************************************
 *          WinHttpCloseHandle (winhttp.@)
 */
BOOL WINAPI WinHttpCloseHandle(HINTERNET handle)
{
    TRACE("(%p)\n", handle);
    return InternetCloseHandle(handle);
}

/***********************************************************************
 *          WinHttpConnect (winhttp.@)
 */
HINTERNET WINAPI WinHttpConnect(HINTERNET session, LPCWSTR server, INTERNET_PORT port, DWORD reserved)
{
    TRACE("(%p %s %d %x)\n", session, debugstr_w(server), port, reserved);
    return InternetConnectW(session, server, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
}

/***********************************************************************
 *          WinHttpCrackUrl (winhttp.@)
 */
BOOL WINAPI WinHttpCrackUrl(LPCWSTR url, DWORD len, DWORD flags, LPURL_COMPONENTSW _components)
{
    BOOL                result;
    static const WCHAR  nullString[] = {'\0'};
    URL_COMPONENTSW     components;


    TRACE("(%s %u %x %p)\n", debugstr_w(url), len, flags, _components);
    
    if (_components == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    if (_components->dwStructSize != sizeof(URL_COMPONENTSW))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    
    memcpy(&components, _components, sizeof(URL_COMPONENTSW));

    result = InternetCrackUrlW(url, len, flags, &components);


    /* don't clobber the output buffer on failure */
    if (result)
    {
        switch (components.nScheme)
        {
            case INTERNET_SCHEME_HTTP:
                components.nScheme = WINHTTP_INTERNET_SCHEME_HTTP;
                break;

            case INTERNET_SCHEME_HTTPS:
                components.nScheme = WINHTTP_INTERNET_SCHEME_HTTPS;
                break;

            case INTERNET_SCHEME_FTP:
            case INTERNET_SCHEME_GOPHER:
            case INTERNET_SCHEME_FILE:
            case INTERNET_SCHEME_NEWS:
            case INTERNET_SCHEME_MAILTO:
            case INTERNET_SCHEME_SOCKS:
            case INTERNET_SCHEME_JAVASCRIPT:
            case INTERNET_SCHEME_VBSCRIPT:
            case INTERNET_SCHEME_RES:
                TRACE("unsupported internet scheme\n");
                SetLastError(ERROR_WINHTTP_UNRECOGNIZED_SCHEME);
                return FALSE;

            default:
                FIXME("should never get to this case\n");
                break;
        }

        memcpy(_components, &components, sizeof(URL_COMPONENTSW));

        /* WinHTTP returns this value as a null string instead of NULL */
        if (_components->lpszUrlPath == NULL)
            _components->lpszUrlPath = (LPWSTR)nullString;

        /* WinHTTP returns this value as a null string instead of NULL */
        if (_components->lpszExtraInfo == NULL)
            _components->lpszExtraInfo = (LPWSTR)nullString;
    }


    return result;
}

/***********************************************************************
 *          WinHttpCreateUrl (winhttp.@)
 */
BOOL WINAPI WinHttpCreateUrl(LPURL_COMPONENTSW components, DWORD flags, LPWSTR url, LPDWORD len)
{
    TRACE("(%p %x %p %p)\n", components, flags, url, len);
    return InternetCreateUrlW(components, flags, url, len);
}

/***********************************************************************
 *          WinHttpDetectAutoProxyConfigUrl (winhttp.@)
 */
BOOL WINAPI WinHttpDetectAutoProxyConfigUrl(DWORD flags, LPWSTR *url)
{
    FIXME("(%x %p)\n", flags, url);

    SetLastError(ERROR_WINHTTP_AUTODETECTION_FAILED);
    return FALSE;
}

/***********************************************************************
 *         WinHttpGetDefaultProxyConfiguration (winhttp.@)
 */
BOOL WINAPI WinHttpGetDefaultProxyConfiguration(WINHTTP_PROXY_INFO *info)
{
    INTERNET_PROXY_INFOW proxy;
    DWORD size;

    TRACE("(%p)\n", info);

    size = sizeof(proxy);
    if (!InternetQueryOptionW(NULL, INTERNET_OPTION_PROXY, &proxy, &size))
        return FALSE;

    switch (proxy.dwAccessType)
    {
    case INTERNET_OPEN_TYPE_PRECONFIG: info->dwAccessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY; break;
    case INTERNET_OPEN_TYPE_DIRECT:    info->dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY; break;
    case INTERNET_OPEN_TYPE_PROXY:     info->dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY; break;
    default:
        ERR("unknown access type: %x\n", proxy.dwAccessType);
        return FALSE;
    }
    info->lpszProxy       = winhttp_strdup(proxy.lpszProxy);
    info->lpszProxyBypass = winhttp_strdup(proxy.lpszProxyBypass);

    return TRUE;
}

/***********************************************************************
 *          WinHttpGetIEProxyConfigForCurrentUser (winhttp.@)
 */
BOOL WINAPI WinHttpGetIEProxyConfigForCurrentUser(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config)
{
    if(!config)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* TODO: read from HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings */
    FIXME("returning no proxy used\n");
    config->fAutoDetect = FALSE;
    config->lpszAutoConfigUrl = NULL;
    config->lpszProxy = NULL;
    config->lpszProxyBypass = NULL;

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/***********************************************************************
 *          WinHttpGetProxyForUrl (winhttp.@)
 */
BOOL WINAPI WinHttpGetProxyForUrl(HINTERNET session, LPCWSTR url, WINHTTP_AUTOPROXY_OPTIONS *options,
                                  WINHTTP_PROXY_INFO *info)
{
    FIXME("(%p %s %p %p)\n", session, debugstr_w(url), options, info);
    return FALSE;
}

/***********************************************************************
 *          WinHttpOpen (winhttp.@)
 */
HINTERNET WINAPI WinHttpOpen(LPCWSTR agent, DWORD access, LPCWSTR proxy, LPCWSTR bypass, DWORD flags)
{
    TRACE("(%s %x %s %s %x)\n", debugstr_w(agent), access, debugstr_w(proxy), debugstr_w(bypass), flags);
    return InternetOpenW(agent, access, proxy, bypass, map_open_flags(flags));
}

/***********************************************************************
 *          WinHttpOpenRequest (winhttp.@)
 */
HINTERNET WINAPI WinHttpOpenRequest(HINTERNET handle, LPCWSTR verb, LPCWSTR object, LPCWSTR version,
                                    LPCWSTR referrer, LPCWSTR *accept, DWORD flags)
{
    DWORD flags_new;

    TRACE("(%p %s %s %s %s %p %x)\n", handle, debugstr_w(verb), debugstr_w(object), debugstr_w(version),
          debugstr_w(referrer), accept, flags);

    flags_new = map_req_flags(flags) | INTERNET_FLAG_KEEP_CONNECTION;
    return HttpOpenRequestW(handle, verb, object, version, referrer, accept, flags_new, 0);
}

#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))

static DWORD auth_scheme_from_header(WCHAR *header)
{
    static const WCHAR basic[]     = { 'B','a','s','i','c' };
    static const WCHAR ntlm[]      = { 'N','T','L','M' };
    static const WCHAR passport[]  = { 'P','a','s','s','p','o','r','t' };
    static const WCHAR digest[]    = { 'D','i','g','e','s','t' };
    static const WCHAR negotiate[] = { 'N','e','g','o','t','i','a','t','e' };

    if (!strncmpiW(header, basic, ARRAYSIZE(basic)) &&
        (header[ARRAYSIZE(basic)] == ' ' || !header[ARRAYSIZE(basic)])) return WINHTTP_AUTH_SCHEME_BASIC;

    if (!strncmpiW(header, ntlm, ARRAYSIZE(ntlm)) &&
        (header[ARRAYSIZE(ntlm)] == ' ' || !header[ARRAYSIZE(ntlm)])) return WINHTTP_AUTH_SCHEME_NTLM;

    if (!strncmpiW(header, passport, ARRAYSIZE(passport)) &&
        (header[ARRAYSIZE(passport)] == ' ' || !header[ARRAYSIZE(passport)])) return WINHTTP_AUTH_SCHEME_PASSPORT;

    if (!strncmpiW(header, digest, ARRAYSIZE(digest)) &&
        (header[ARRAYSIZE(digest)] == ' ' || !header[ARRAYSIZE(digest)])) return WINHTTP_AUTH_SCHEME_DIGEST;

    if (!strncmpiW(header, negotiate, ARRAYSIZE(negotiate)) &&
        (header[ARRAYSIZE(negotiate)] == ' ' || !header[ARRAYSIZE(negotiate)])) return WINHTTP_AUTH_SCHEME_NEGOTIATE;

    return 0;
}

static BOOL query_auth_schemes(HINTERNET request, DWORD level, LPDWORD supported, LPDWORD first, LPDWORD target)
{
    DWORD index = 0;
    BOOL ret = FALSE;

    for (;;)
    {
        DWORD len, scheme;
        WCHAR *buffer;

        len = 0;
        HttpQueryInfoW(request, level, NULL, &len, &index);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;

        index--;
        if (!(buffer = HeapAlloc(GetProcessHeap(), 0, len + sizeof(WCHAR)))) return FALSE;
        if (!HttpQueryInfoW(request, level, buffer, &len, &index))
        {
            HeapFree(GetProcessHeap(), 0, buffer);
            return FALSE;
        }
        scheme = auth_scheme_from_header(buffer);
        if (index == 1) *first = scheme;
        *supported |= scheme;

        HeapFree(GetProcessHeap(), 0, buffer);
        ret = TRUE;
        index++;
    }
    return ret;
}

/***********************************************************************
 *          WinHttpQueryAuthSchemes (winhttp.@)
 */
BOOL WINAPI WinHttpQueryAuthSchemes(HINTERNET request, LPDWORD supported, LPDWORD first, LPDWORD target)
{
    BOOL ret = FALSE;

    TRACE("(%p %p %p %p)\n", request, supported, first, target);

    *supported = *first = 0;
	
    if (query_auth_schemes(request, HTTP_QUERY_WWW_AUTHENTICATE, supported, first, target))
    {
        *target = WINHTTP_AUTH_TARGET_SERVER;
        ret = TRUE;
    }
    if (query_auth_schemes(request, HTTP_QUERY_PROXY_AUTHENTICATE, supported, first, target))
    {
        *target |= WINHTTP_AUTH_TARGET_PROXY;
        ret = TRUE;
    }

    return ret;
}

/***********************************************************************
 *          WinHttpQueryDataAvailable (winhttp.@)
 */
BOOL WINAPI WinHttpQueryDataAvailable(HINTERNET request, LPDWORD available)
{
    TRACE("(%p %p)\n", request, available);
    return InternetQueryDataAvailable(request, available, 0, 0);
}

/***********************************************************************
 *          WinHttpQueryHeaders (winhttp.@)
 */
BOOL WINAPI WinHttpQueryHeaders(HINTERNET request, DWORD level, LPCWSTR name, LPVOID buffer,
                                LPDWORD len, LPDWORD index)
{
    TRACE("(%p %u %s %p %p %p)\n", request, level, debugstr_w(name), buffer, len, index);
    return HttpQueryInfoW(request, map_info_level(level), buffer, len, index);
}

/***********************************************************************
 *          WinHttpQueryOption (winhttp.@)
 */
BOOL WINAPI WinHttpQueryOption(HINTERNET handle, DWORD option, LPVOID buffer, LPDWORD len)
{
    BOOL ret;
    DWORD opt = 0;

    TRACE("(%p %x %p %p)\n", handle, option, buffer, len);

    if (!option || ((opt = map_option(option, &ret)) && ret))
        ret = InternetQueryOptionW(handle, opt, buffer, len);

    return ret;
}

/***********************************************************************
 *          WinHttpReadData (winhttp.@)
 */
BOOL WINAPI WinHttpReadData(HINTERNET request, LPVOID buffer, DWORD to_read, LPDWORD read)
{
    TRACE("(%p %p %u %p)\n", request, buffer, to_read, read);
    return InternetReadFile(request, buffer, to_read, read);
}

/***********************************************************************
 *          WinHttpReceiveResponse (winhttp.@)
 */
BOOL WINAPI WinHttpReceiveResponse(HINTERNET request, LPVOID reserved)
{
    TRACE("(%p %p)\n", request, reserved);
    return HttpEndRequestW(request, 0, 0, 0);
}

/***********************************************************************
 *          WinHttpSendRequest (winhttp.@)
 */
BOOL WINAPI WinHttpSendRequest(HINTERNET request, LPCWSTR headers, DWORD headers_len,
                               LPVOID optional, DWORD optional_len, DWORD total_len, DWORD_PTR context)
{
    INTERNET_BUFFERSW buffers;

    TRACE("(%p %s %u %p %u %u %lx)\n", request, debugstr_w(headers), headers_len, optional,
          optional_len, total_len, context);

    memset(&buffers, 0, sizeof(buffers));
    buffers.dwStructSize = sizeof(buffers);
    if (headers)
    {
        buffers.lpcszHeader = headers;
        if (headers_len == ~0UL) buffers.dwHeadersLength = strlenW(buffers.lpcszHeader) + 1;
        else buffers.dwHeadersLength = headers_len;
    }
    buffers.lpvBuffer = optional;
    buffers.dwBufferLength = optional_len;
    buffers.dwBufferTotal = total_len;

    return HttpSendRequestExW(request, &buffers, NULL, 0, context);
}

/***********************************************************************
 *          WinHttpSetCredentials (winhttp.@)
 */
BOOL WINAPI WinHttpSetCredentials(HINTERNET request, DWORD target, DWORD scheme, LPCWSTR username,
                                  LPCWSTR password, LPVOID params)
{
    static const WCHAR basic[] =
        { 'B','a','s','i','c',' ',0 };
    static const WCHAR auth_server[] =
        { 'A','u','t','h','o','r','i','z','a','t','i','o','n',':',' ',0 };
    static const WCHAR auth_proxy[] =
        { 'P','r','o','x','y','-','A','u','t','h','o','r','i','z','a','t','i','o','n',':',' ',0 };

    const WCHAR *auth_scheme, *auth_target;
    WCHAR *auth_header;
    DWORD len, auth_data_len;
    char *auth_data;
    BOOL ret;

    TRACE("(%p %x %x %s %s %p)\n", request, target, scheme, debugstr_w(username),
          debugstr_w(password), params);

    switch (target)
    {
    case WINHTTP_AUTH_TARGET_SERVER: auth_target = auth_server; break;
    case WINHTTP_AUTH_TARGET_PROXY:  auth_target = auth_proxy; break;
    default:
        ERR("unknown target %x\n", target);
        return FALSE;
    }
    switch (scheme)
    {
    case WINHTTP_AUTH_SCHEME_BASIC:
    {
        int userlen = WideCharToMultiByte(CP_UTF8, 0, username, strlenW(username), NULL, 0, NULL, NULL);
        int passlen = WideCharToMultiByte(CP_UTF8, 0, password, strlenW(password), NULL, 0, NULL, NULL);

        TRACE("basic authentication\n");

        auth_scheme = basic;
        auth_data_len = userlen + 1 + passlen;
        if (!(auth_data = HeapAlloc(GetProcessHeap(), 0, auth_data_len))) return FALSE;

        WideCharToMultiByte(CP_UTF8, 0, username, -1, auth_data, userlen, NULL, NULL);
        auth_data[userlen] = ':';
        WideCharToMultiByte(CP_UTF8, 0, password, -1, &auth_data[userlen + 1], passlen, NULL, NULL);
        break;
    }
    case WINHTTP_AUTH_SCHEME_NTLM:
    case WINHTTP_AUTH_SCHEME_PASSPORT:
    case WINHTTP_AUTH_SCHEME_DIGEST:
    case WINHTTP_AUTH_SCHEME_NEGOTIATE:
        FIXME("unimplemented authentication scheme %x\n", scheme);
        return FALSE;
    default:
        ERR("unknown authentication scheme %x\n", scheme);
        return FALSE;
    }

    len = strlenW(auth_target) + strlenW(auth_scheme) + ((auth_data_len + 2) * 4) / 3;
    if (!(auth_header = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR))))
    {
        HeapFree(GetProcessHeap(), 0, auth_data);
        return FALSE;
    }
    strcpyW(auth_header, auth_target);
    strcatW(auth_header, auth_scheme);
    HTTP_EncodeBase64(auth_data, auth_data_len, auth_header + strlenW(auth_header));

    ret = WinHttpSendRequest(request, auth_header, ~0UL, NULL, 0, 0, 0);

    HeapFree(GetProcessHeap(), 0, auth_data);
    HeapFree(GetProcessHeap(), 0, auth_header);
    return ret;
}

/***********************************************************************
 *         WinHttpSetDefaultProxyConfiguration (winhttp.@)
 */
BOOL WINAPI WinHttpSetDefaultProxyConfiguration(WINHTTP_PROXY_INFO *info)
{
    INTERNET_PROXY_INFOW proxy;

    TRACE("(%p)\n", info);

    switch (info->dwAccessType)
    {
    case WINHTTP_ACCESS_TYPE_DEFAULT_PROXY: proxy.dwAccessType = INTERNET_OPEN_TYPE_PRECONFIG;
    case WINHTTP_ACCESS_TYPE_NO_PROXY:      proxy.dwAccessType = INTERNET_OPEN_TYPE_DIRECT;
    case WINHTTP_ACCESS_TYPE_NAMED_PROXY:   proxy.dwAccessType = INTERNET_OPEN_TYPE_PROXY;
    default:
        WARN("unknown access type: %x\n", info->dwAccessType);
        return FALSE;
    }
    proxy.lpszProxy       = info->lpszProxy;
    proxy.lpszProxyBypass = info->lpszProxyBypass;

    return InternetSetOptionW(NULL, INTERNET_OPTION_PROXY, &proxy, sizeof(proxy));
}

/***********************************************************************
 *          WinHttpSetOption (winhttp.@)
 */
BOOL WINAPI WinHttpSetOption(HINTERNET handle, DWORD option, LPVOID buffer, DWORD len)
{
    BOOL ret;
    DWORD opt = 0;

    TRACE("(%p %x %p %u)\n", handle, option, buffer, len);

    if (!option || ((opt = map_option(option, &ret)) && ret))
        ret = InternetSetOptionW(handle, opt, buffer, len);

    return ret;
}

#define INTERNET_OPTION_CALLBACK_FILTERED     10000   /* Wine extension */

/***********************************************************************
 *          WinHttpSetStatusCallback (winhttp.@)
 */
WINHTTP_STATUS_CALLBACK WINAPI WinHttpSetStatusCallback(HINTERNET handle, WINHTTP_STATUS_CALLBACK callback,
                                                        DWORD flags, DWORD_PTR reserved)
{
    struct
    {
        DWORD filter;
        INTERNET_STATUS_CALLBACK new;
        INTERNET_STATUS_CALLBACK old;
    } cbf;

    TRACE("(%p %p %x %lx)\n", handle, callback, flags, reserved);

    cbf.filter = flags;
    cbf.new    = callback;
    cbf.old    = NULL;
    if (InternetSetOptionW(handle, INTERNET_OPTION_CALLBACK_FILTERED, &cbf, sizeof(cbf)))
        return cbf.old;

    return WINHTTP_INVALID_STATUS_CALLBACK;
}

/***********************************************************************
 *          WinHttpSetTimeouts (winhttp.@)
 */
BOOL WINAPI WinHttpSetTimeouts(HINTERNET handle, int resolve, int connect, int send, int receive)
{
    ULONG timeout;

    TRACE("(%p %d %d %d %d)\n", handle, resolve, connect, send, receive);

    if (resolve < -1 || connect < -1 || send < -1 || receive < -1)
        return ERROR_INVALID_PARAMETER;

    if (resolve > 0) FIXME("resolve timeout not supported\n");

    timeout = (connect > 0) ? connect : ~0UL;
    InternetSetOptionW(handle, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));

    timeout = (send > 0) ? send : ~0UL;
    InternetSetOptionW(handle, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    timeout = (receive > 0) ? receive : ~0UL;
    InternetSetOptionW(handle, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    return TRUE;
}

/***********************************************************************
 *          WinHttpTimeFromSystemTime (winhttp.@)
 */
BOOL WINAPI WinHttpTimeFromSystemTime(const SYSTEMTIME *st, LPWSTR time)
{
    TRACE("(%p %p)\n", st, time);
    return InternetTimeFromSystemTimeW(st, INTERNET_RFC1123_FORMAT, time, WINHTTP_TIME_FORMAT_BUFSIZE);
}

/***********************************************************************
 *          WinHttpTimeToSystemTime (winhttp.@)
 */
BOOL WINAPI WinHttpTimeToSystemTime(LPCWSTR time, SYSTEMTIME *st)
{
    TRACE("(%s %p)\n", debugstr_w(time), st);
    return InternetTimeToSystemTimeW(time, st, 0);
}

/***********************************************************************
 *          WinHttpWriteData (winhttp.@)
 */
BOOL WINAPI WinHttpWriteData(HINTERNET request, LPCVOID buffer, DWORD to_write, LPDWORD written)
{
    TRACE("(%p %p %u %p)\n", request, buffer, to_write, written);
    return InternetWriteFile(request, buffer, to_write, written);
}
