/*
 * nt_reg.c - simplified registry access functions for use in ntdll
 *
 * Copyright 2008 TransGaming Inc.
 *
 */
#include "nt_reg.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "wine/nt_config.h"


WINE_DEFAULT_DEBUG_CHANNEL (reg);


#define ARRAYSIZE(x)    (sizeof(x) / sizeof((x)[0]))

/* Nt_regCreateKeyExW(): a slightly modified version of the real implementation of RegCreateKeyExA().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
NTSTATUS Nt_regCreateKeyExW(HKEY hkey, LPCWSTR name, DWORD options, REGSAM access, LPHKEY retkey){
    OBJECT_ATTRIBUTES   attr;
    UNICODE_STRING      nameW;
    UNICODE_STRING      classW;
    NTSTATUS            status;


    if (!(access & KEY_ALL_ACCESS) || (access & ~KEY_ALL_ACCESS))
        return ERROR_ACCESS_DENIED;


    RtlInitUnicodeString(&nameW, name);
    RtlInitUnicodeString(&classW, NULL);

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;


    /* FIXME: should use Unicode buffer in TEB */
    status = NtCreateKey( retkey, access, &attr, 0, &classW, options, NULL);

    return status;
}

/* Nt_regOpenKeyExW(): a slightly modified version of the real implementation of RegOpenKey().
     This version has been simplified to suit the needs of accessing the config areas of the
     registry. */
NTSTATUS Nt_regOpenKeyExW(HKEY hkey, LPCWSTR name, REGSAM access, LPHKEY retkey )
{
    OBJECT_ATTRIBUTES   attr;
    UNICODE_STRING      nameW;


    RtlInitUnicodeString(&nameW, name);

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;


    return NtOpenKey(retkey, access, &attr);
}

/* Nt_regCloseKey(): a slightly modified version of the real implementation of RegCloseKey().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
NTSTATUS Nt_regCloseKey(HKEY hkey){
    if (!hkey || hkey >= 0x80000000)
        return ERROR_SUCCESS;

    return NtClose( hkey );
}

/* Nt_regQueryValueExW(): a slightly modified version of the real implementation of RegQueryValueExW().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
NTSTATUS Nt_regQueryValueExW(HKEY hkey, LPCWSTR name, LPBYTE data, DWORD *count){
    NTSTATUS                        status;
    UNICODE_STRING                  nameW;
    DWORD                           total_size;
    char                            buffer[MAX_PATH + 128];
    char *                          buf_ptr = buffer;
    KEY_VALUE_PARTIAL_INFORMATION * info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    static const int                info_size = info->Data - (UCHAR *)info;


    RtlInitUnicodeString( &nameW, name );


    status = NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation,
                              buffer, sizeof(buffer), &total_size );

    if (status && status != STATUS_BUFFER_OVERFLOW)
        return status;


    /* we need to fetch the contents for a string type even if not requested,
     * because we need to compute the length of the ASCII string. */
    if (data)
    {
        /* give up and let the caller handle the failure */
        if (status == STATUS_BUFFER_OVERFLOW)
            return status;

        if (data){
            if (total_size - info_size > *count)
                status = STATUS_BUFFER_OVERFLOW;

            else
                memcpy(data, buf_ptr + info_size, total_size - info_size);
        }
    }


    if (count)
        *count = total_size - info_size;

    return status;
}


/* getConfigKey(): get a config key from either the app-specific or the default config */
BOOL Nt_getConfigKey(HKEY defkey, LPCWSTR name, WCHAR *buffer, DWORD size){
    return Nt_regQueryValueExW( defkey, name, (LPBYTE)buffer, &size ) == STATUS_SUCCESS;
}


/* Nt_getSingleConfigValue(): get the value of a single config option.   Handles all registry
     operations internally.  Returns TRUE if the value was successfully retrieved, and FALSE
     otherwise.
     If testAppDefault is true, we'll check for AppDefaults\\<exe name>\\<section> for <name> as
     well and use that to override the global setting, if it exists */
BOOL Nt_getSingleConfigValueW(LPCWSTR    section, 
                              LPCWSTR    name, 
                              BOOL       testAppDefault,
                              WCHAR *    buffer, 
                              DWORD      size)
{
    HCONFIG config;
    BOOL    result;


    TRACE("opening section %s to retrieve the value for %s {testAppDefault = %s, buffer = %p, size = %d}\n", 
            debugstr_w(section), 
            debugstr_w(name), 
            testAppDefault ? "TRUE" : "FALSE", 
            buffer, 
            size);


    config = Nt_openConfigW(section, testAppDefault ? 0 : CONFIG_FLAG_NOAPPDEFAULTS);

    if (config == NULL){
        ERR("failed to open the config\n");

        return FALSE;
    }


    memset(buffer, 0, size);
    result = Nt_getConfigKeyW(config, name, buffer, size);
    
    if (result)
        TRACE("retrieved the value %s\n", debugstr_w(buffer));

    else
        TRACE("could not retrieve the key value\n");

    Nt_closeConfig(config);

    return result;
}

/* Nt_getSingleConfigValue(): get the value of a single config option.   Handles all registry
     operations internally.  Returns TRUE if the value was successfully retrieved, and FALSE
     otherwise.
     If testAppDefault is true, we'll check for AppDefaults\\<exe name>\\<section> for <name> as
     well and use that to override the global setting, if it exists */
BOOL Nt_getSingleConfigValueA(LPCSTR     section, 
                              LPCSTR     name, 
                              BOOL       testAppDefault,
                              CHAR *     buffer, 
                              DWORD      size)
{
    HCONFIG config;
    BOOL    result;


    TRACE("opening section '%s' to retrieve the value for '%s' {testAppDefault = %s, buffer = %p, size = %d}\n", 
            section, 
            name, 
            testAppDefault ? "TRUE" : "FALSE", 
            buffer, 
            size);


    config = Nt_openConfigA(section, testAppDefault ? 0 : CONFIG_FLAG_NOAPPDEFAULTS);

    if (config == NULL){
        ERR("failed to open the config\n");

        return FALSE;
    }


    result = Nt_getConfigKeyA(config, name, buffer, size);

    if (result)
        TRACE("retrieved the value '%s'\n", buffer);

    else
        TRACE("could not retrieve the key value\n");

    Nt_closeConfig(config);

    return result;
}



/************************************** external config API functions *************************************/

/* ConfigData: internal struct for the config context handle. */
typedef struct{
    HKEY    hkey;
    HKEY    appkey;
    DWORD   lastError;
} ConfigData;


/* strdupAtoW(): converts an ANSI string to a wide character string in a new
     buffer.  A pointer to the new buffer is returned on success.  NULL is
     returned on failure (ie: out of memory).  The returned buffer must later
     be destroyed using a call to RtlFreeHeap(GetProcessHeap(), 0, <buf>). */
static LPWSTR strdupAtoW(LPCSTR str){
    LPWSTR  ret;
    INT     len;


    if (str == NULL)
        return NULL;

    len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    ret = RtlAllocateHeap(GetProcessHeap(), 0, len * sizeof(WCHAR));

    if (ret)
        MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);

    return ret;
}


/* Nt_openConfig(): opens the config file to the section <section> and prepares
     it for being read from.  Returns a handle to the config context if successful.
     Returns NULL on failure. */
HCONFIG Nt_openConfigA(LPCSTR section, DWORD flags){
    WCHAR * sectionW = strdupAtoW(section);
    HCONFIG result;


    if (sectionW == NULL){
        ERR("out of memory!\n");

        return NULL;
    }


    result = Nt_openConfigW(sectionW, flags);

    RtlFreeHeap(GetProcessHeap(), 0, sectionW);

    return result;
}

/* Nt_openConfig(): opens the config file to the section <section> and prepares
     it for being read from.  Returns a handle to the config context if successful.
     Returns NULL on failure. */
HCONFIG Nt_openConfigW(LPCWSTR section, DWORD flags){
    ConfigData *        data = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(ConfigData));
    char                buffer[MAX_PATH];
    WCHAR               wbuffer[MAX_PATH + 64];
    static const WCHAR  baseKeyName[] = {'S', 'o', 'f', 't', 'w', 'a', 'r', 'e', '\\',
                                         'W', 'i', 'n', 'e', '\\',
                                         'W', 'i', 'n', 'e', '\\',
                                         'C', 'o', 'n', 'f', 'i', 'g', 0};
    static const WCHAR  baseAppName[] = {'\\', 'A', 'p', 'p', 'D', 'e', 'f', 'a', 'u', 'l', 't', 's', 0};
    static const WCHAR  keyFormat[] =   {'%', 's', '\\', '%', 's', 0};
    static const WCHAR  appFormat[] =   {'%', 's', '%', 's', '\\', '%', 's', '\\', '%', 's', 0};


    if (data == NULL){
        ERR("out of memory!\n");

        return NULL;
    }


    data->hkey = 0;
    data->appkey = 0;
    data->lastError = STATUS_SUCCESS;


    /* create the section name and try to open the key */
    snprintfW(wbuffer, ARRAYSIZE(wbuffer), keyFormat, baseKeyName, section);

    data->lastError = Nt_regCreateKeyExW(   HKEY_LOCAL_MACHINE,
                                            wbuffer,
                                            REG_OPTION_VOLATILE,
                                            KEY_ALL_ACCESS,
                                            &data->hkey);

    if (data->lastError != STATUS_SUCCESS)
        ERR("Cannot create config registry key\n" );


    /* open the app-specific key */
    if (!(flags & CONFIG_FLAG_NOAPPDEFAULTS)){
        if (GetModuleFileName16(GetCurrentTask(), buffer, MAX_PATH) ||
            ((data->lastError = GetModuleFileNameA(0, buffer, MAX_PATH)) != 0 &&
             data->lastError != MAX_PATH))
        {
            WCHAR * appNameW;
            char *  p;
            char *  appname = buffer;


            if ((p = strrchr(appname, '/')))
                appname = p + 1;

            if ((p = strrchr(appname, '\\')))
                appname = p + 1;


            appNameW = strdupAtoW(appname);

            if (appNameW == NULL)
                ERR("could not convert the app name '%s'\n", appname);

            snprintfW(  wbuffer, 
                        ARRAYSIZE(wbuffer),
                        appFormat,
                        baseKeyName,
                        baseAppName,
                        appNameW,
                        section);

            RtlFreeHeap(GetProcessHeap(), 0, appNameW);

            data->lastError = Nt_regOpenKeyExW(HKEY_LOCAL_MACHINE, wbuffer, KEY_ALL_ACCESS, &data->appkey);

            if (data->lastError != STATUS_SUCCESS)
                data->appkey = 0;
        }
     
        else
            ERR("could not retrieve the module file name (reason: '%s')\n", data->lastError == 0 ? "bad module" : "buffer too small");
    }



    /* couldn't open either key => fail */
    if (data->hkey == 0 && data->appkey == 0){
        ERR("could not open the config {error = 0x%08x}\n", data->lastError);
        RtlFreeHeap(GetProcessHeap(), 0, data);

        data = NULL;
    }

    return data;
}

/* Nt_closeConfig(): closes a config context that was previously opened by a call
     to Nt_openConfig().  The handle is no longer valid after this point. */
void Nt_closeConfig(HCONFIG config){
    ConfigData *data = (ConfigData *)config;


    if (data == NULL){
        ERR("passed a null config handle\n");

        return;
    }


    if (data->hkey)
        Nt_regCloseKey(data->hkey);

    if (data->appkey)
        Nt_regCloseKey(data->appkey);

    RtlFreeHeap(GetProcessHeap(), 0, data);
}

/* Nt_getConfigKeyA(): retrieves a single key value from the config section identified
     by <config>.  The value for the key <name> will be copied into the buffer <buffer>.
     No more than <size> bytes will be copied into the buffer.  Returns TRUE if the
     value was successfully retrieved.  Returns FALSE otherwise. */
BOOL Nt_getConfigKeyA(HCONFIG config, LPCSTR name, CHAR *buffer, DWORD size){
    WCHAR * nameW = strdupAtoW(name);
    WCHAR * result;
    BOOL    ret;


    if (config == NULL){
        ERR("passed a null config handle\n");

        return FALSE;
    }


    if (nameW == NULL){
        ERR("could not convert the key name '%s'\n", name);

        return FALSE;
    }


    /* <size> is the size in bytes of the buffer <buffer>.  Since it is an ANSI string,
       that is also the same number of characters.  Because of this we'll need to allocate
       <size> wide characters in the temp buffer to accommodate as much of the string as
       the caller expects. */
    result = RtlAllocateHeap(GetProcessHeap(), 0, size * sizeof(WCHAR));

    if (result == NULL){
        ERR("out of memory {size = %d}\n", size);
        RtlFreeHeap(GetProcessHeap(), 0, nameW);

        return FALSE;
    }


    ret = Nt_getConfigKeyW(config, nameW, result, size * sizeof(WCHAR));

    if (ret)
        WideCharToMultiByte(CP_ACP, 0, result, -1, buffer, size, NULL, NULL);


    RtlFreeHeap(GetProcessHeap(), 0, nameW);
    RtlFreeHeap(GetProcessHeap(), 0, result);

    return ret;
}

/* Nt_getConfigKeyW(): retrieves a single key value from the config section identified
     by <config>.  The value for the key <name> will be copied into the buffer <buffer>.
     No more than <size> bytes will be copied into the buffer.  Returns TRUE if the
     value was successfully retrieved.  Returns FALSE otherwise. */
BOOL Nt_getConfigKeyW(HCONFIG config, LPCWSTR name, WCHAR *buffer, DWORD size){
    ConfigData *data = (ConfigData *)config;


    if (data == NULL){
        ERR("passed a null config handle\n");

        return FALSE;
    }


    if (data->appkey){
        data->lastError = Nt_regQueryValueExW(data->appkey, name, (LPBYTE)buffer, &size);

        if (data->lastError == STATUS_SUCCESS)
            return TRUE;
    }


    data->lastError = Nt_regQueryValueExW(data->hkey, name, (LPBYTE)buffer, &size);

    return data->lastError == STATUS_SUCCESS;
}
