/*
 * Registry management
 *
 * Copyright (C) 1999 Alexandre Julliard
 *
 * Based on misc/registry.c code
 * Copyright (C) 1996 Marcus Meissner
 * Copyright (C) 1998 Matthew Becker
 * Copyright (C) 1999 Sylvain St-Germain
 * Copyright (C) 2007 TransGaming Technologies
 *
 * This file is concerned about handle management and interaction with the Wine server.
 * Registry file I/O is in misc/registry.c.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "winbase.h"
#include "winreg.h"
#include "winerror.h"
#include "wine/unicode.h"
#include "heap.h"
#include "wine/file.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);


/* check if value type needs string conversion (Ansi<->Unicode) */
static inline int is_string( DWORD type )
{
    return (type == REG_SZ) || (type == REG_EXPAND_SZ) || (type == REG_MULTI_SZ);
}


/******************************************************************************
 *           RegCreateKeyExW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey       [I] Handle of an open key
 *    name       [I] Address of subkey name
 *    reserved   [I] Reserved - must be 0
 *    class      [I] Address of class string
 *    options    [I] Special options flag
 *    access     [I] Desired security access
 *    sa         [I] Address of key security structure
 *    retkey     [O] Address of buffer for opened handle
 *    dispos     [O] Receives REG_CREATED_NEW_KEY or REG_OPENED_EXISTING_KEY
 *
 * NOTES
 *  in case of failing retkey remains untouched
 */
DWORD WINAPI RegCreateKeyExW( HKEY hkey, LPCWSTR name, DWORD reserved, LPWSTR class,
                              DWORD options, REGSAM access, SECURITY_ATTRIBUTES *sa,
                              LPHKEY retkey, LPDWORD dispos )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW, classW;

    if (reserved) return ERROR_INVALID_PARAMETER;
    if (!(access & KEY_ALL_ACCESS) || (access & ~KEY_ALL_ACCESS)) return ERROR_ACCESS_DENIED;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, name );
    RtlInitUnicodeString( &classW, class );

    return RtlNtStatusToDosError( NtCreateKey( retkey, access, &attr, 0,
                                               &classW, options, dispos ) );
}


/******************************************************************************
 *           RegCreateKeyExA   [ADVAPI32.@]
 */
DWORD WINAPI RegCreateKeyExA( HKEY hkey, LPCSTR name, DWORD reserved, LPSTR class,
                              DWORD options, REGSAM access, SECURITY_ATTRIBUTES *sa,
                              LPHKEY retkey, LPDWORD dispos )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING classW;
    ANSI_STRING nameA, classA;
    NTSTATUS status;

    if (reserved) return ERROR_INVALID_PARAMETER;
    if (!(access & KEY_ALL_ACCESS) || (access & ~KEY_ALL_ACCESS)) return ERROR_ACCESS_DENIED;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &NtCurrentTeb()->StaticUnicodeString;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitAnsiString( &nameA, name );
    RtlInitAnsiString( &classA, class );

    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
        if (!(status = RtlAnsiStringToUnicodeString( &classW, &classA, TRUE )))
        {
            status = NtCreateKey( retkey, access, &attr, 0, &classW, options, dispos );
            RtlFreeUnicodeString( &classW );
        }
    }
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegCreateKeyW   [ADVAPI32.@]
 */
DWORD WINAPI RegCreateKeyW( HKEY hkey, LPCWSTR name, LPHKEY retkey )
{
    /* FIXME: previous implementation converted ERROR_INVALID_HANDLE to ERROR_BADKEY, */
    /* but at least my version of NT (4.0 SP5) doesn't do this.  -- AJ */
    return RegCreateKeyExW( hkey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS, NULL, retkey, NULL );
}


/******************************************************************************
 *           RegCreateKeyA   [ADVAPI32.@]
 */
DWORD WINAPI RegCreateKeyA( HKEY hkey, LPCSTR name, LPHKEY retkey )
{
    return RegCreateKeyExA( hkey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS, NULL, retkey, NULL );
}



/******************************************************************************
 *           RegOpenKeyExW   [ADVAPI32.@]
 *
 * Opens the specified key
 *
 * Unlike RegCreateKeyEx, this does not create the key if it does not exist.
 *
 * PARAMS
 *    hkey       [I] Handle of open key
 *    name       [I] Name of subkey to open
 *    reserved   [I] Reserved - must be zero
 *    access     [I] Security access mask
 *    retkey     [O] Handle to open key
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 *
 * NOTES
 *  in case of failing is retkey = 0
 */
DWORD WINAPI RegOpenKeyExW( HKEY hkey, LPCWSTR name, DWORD reserved, REGSAM access, LPHKEY retkey )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, name );
    return RtlNtStatusToDosError( NtOpenKey( retkey, access, &attr ) );
}


/******************************************************************************
 *           RegOpenKeyExA   [ADVAPI32.@]
 */
DWORD WINAPI RegOpenKeyExA( HKEY hkey, LPCSTR name, DWORD reserved, REGSAM access, LPHKEY retkey )
{
    OBJECT_ATTRIBUTES attr;
    STRING nameA;
    NTSTATUS status;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &NtCurrentTeb()->StaticUnicodeString;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    RtlInitAnsiString( &nameA, name );
    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
      /* mbc - 1/31/02 -

	 from looking at the trace relay from the native windows advapi32.dll, it seems that
	 when RegOpenKeyEx is called, in windows95 it calls NtOpenKey with KEY_ALL_ACCESS regardless
	 of the access mask that was passed in.

	 some apps, like aol7, rely on the brokenness of windows95 to function properly.

       */

      if (GetVersion() & 0x80000000)  /* win95 */ {
	status = NtOpenKey( retkey, KEY_ALL_ACCESS, &attr );
	TRACE("version is win95, using KEY_ALL_ACCESS\n");
      }
      else {
	status = NtOpenKey( retkey, access, &attr );
	TRACE("version is not win95, using requested access, %u\n", access);
      }

    }
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegOpenKeyW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey    [I] Handle of open key
 *    name    [I] Address of name of subkey to open
 *    retkey  [O] Handle to open key
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 *
 * NOTES
 *  in case of failing is retkey = 0
 */
DWORD WINAPI RegOpenKeyW( HKEY hkey, LPCWSTR name, LPHKEY retkey )
{
    return RegOpenKeyExW( hkey, name, 0, KEY_ALL_ACCESS, retkey );
}


/******************************************************************************
 *           RegOpenKeyA   [ADVAPI32.@]
 */
DWORD WINAPI RegOpenKeyA( HKEY hkey, LPCSTR name, LPHKEY retkey )
{
    return RegOpenKeyExA( hkey, name, 0, KEY_ALL_ACCESS, retkey );
}


/******************************************************************************
 *           RegOpenCurrentUser   [ADVAPI32.@]
 * FIXME: This function is supposed to retrieve a handle to the
 * HKEY_CURRENT_USER for the user the current thread is impersonating.
 * Since Wine does not currently allow threads to impersonate other users,
 * this stub should work fine.
 */
DWORD WINAPI RegOpenCurrentUser( REGSAM access, PHKEY retkey )
{
    return RegOpenKeyExA( HKEY_CURRENT_USER, "", 0, access, retkey );
}



/******************************************************************************
 *           RegEnumKeyExW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey         [I] Handle to key to enumerate
 *    index        [I] Index of subkey to enumerate
 *    name         [O] Buffer for subkey name
 *    name_len     [O] Size of subkey buffer
 *    reserved     [I] Reserved
 *    class        [O] Buffer for class string
 *    class_len    [O] Size of class buffer
 *    ft           [O] Time key last written to
 */
DWORD WINAPI RegEnumKeyExW( HKEY hkey, DWORD index, LPWSTR name, LPDWORD name_len,
                            LPDWORD reserved, LPWSTR class, LPDWORD class_len, FILETIME *ft )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_NODE_INFORMATION *info = (KEY_NODE_INFORMATION *)buffer;
    DWORD total_size;

    TRACE( "(0x%x,%u,%p,%p(%d),%p,%p,%p,%p)\n", hkey, index, name, name_len,
           name_len ? *name_len : -1, reserved, class, class_len, ft );

    if (reserved) return ERROR_INVALID_PARAMETER;

    status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                             buffer, sizeof(buffer), &total_size );

    while (status == STATUS_BUFFER_OVERFLOW)
    {
        /* retry with a dynamically allocated buffer */
        if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
        if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
            return ERROR_NOT_ENOUGH_MEMORY;
        info = (KEY_NODE_INFORMATION *)buf_ptr;
        status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                                 buf_ptr, total_size, &total_size );
    }

    if (!status)
    {
        DWORD len = info->NameLength / sizeof(WCHAR);
        DWORD cls_len = info->ClassLength / sizeof(WCHAR);

        if (ft) *ft = *(FILETIME *)&info->LastWriteTime;

        if (len >= *name_len || (class && class_len && (cls_len >= *class_len)))
            status = STATUS_BUFFER_OVERFLOW;
        else
        {
            *name_len = len;
            memcpy( name, info->Name, info->NameLength );
            name[len] = 0;
            if (class_len)
            {
                *class_len = cls_len;
                if (class)
                {
                    memcpy( class, buf_ptr + info->ClassOffset, info->ClassLength );
                    class[cls_len] = 0;
                }
            }
        }
    }

    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegEnumKeyExA   [ADVAPI32.@]
 */
DWORD WINAPI RegEnumKeyExA( HKEY hkey, DWORD index, LPSTR name, LPDWORD name_len,
                            LPDWORD reserved, LPSTR class, LPDWORD class_len, FILETIME *ft )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_NODE_INFORMATION *info = (KEY_NODE_INFORMATION *)buffer;
    DWORD total_size;

    TRACE( "(0x%x,%u,%p,%p(%d),%p,%p,%p,%p)\n", hkey, index, name, name_len,
           name_len ? *name_len : -1, reserved, class, class_len, ft );

    if (reserved) return ERROR_INVALID_PARAMETER;

    status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                             buffer, sizeof(buffer), &total_size );

    while (status == STATUS_BUFFER_OVERFLOW)
    {
        /* retry with a dynamically allocated buffer */
        if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
        if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
            return ERROR_NOT_ENOUGH_MEMORY;
        info = (KEY_NODE_INFORMATION *)buf_ptr;
        status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                                 buf_ptr, total_size, &total_size );
    }

    if (!status)
    {
        DWORD len, cls_len;

        RtlUnicodeToMultiByteSize( &len, info->Name, info->NameLength );
        RtlUnicodeToMultiByteSize( &cls_len, (WCHAR *)(buf_ptr + info->ClassOffset),
                                   info->ClassLength );
        if (ft) *ft = *(FILETIME *)&info->LastWriteTime;

        if (len >= *name_len || (class && class_len && (cls_len >= *class_len)))
            status = STATUS_BUFFER_OVERFLOW;
        else
        {
            *name_len = len;
            RtlUnicodeToMultiByteN( name, len, NULL, info->Name, info->NameLength );
            name[len] = 0;
            if (class_len)
            {
                *class_len = cls_len;
                if (class)
                {
                    RtlUnicodeToMultiByteN( class, cls_len, NULL,
                                            (WCHAR *)(buf_ptr + info->ClassOffset),
                                            info->ClassLength );
                    class[cls_len] = 0;
                }
            }
        }
    }

    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegEnumKeyW   [ADVAPI32.@]
 */
DWORD WINAPI RegEnumKeyW( HKEY hkey, DWORD index, LPWSTR name, DWORD name_len )
{
    return RegEnumKeyExW( hkey, index, name, &name_len, NULL, NULL, NULL, NULL );
}


/******************************************************************************
 *           RegEnumKeyA   [ADVAPI32.@]
 */
DWORD WINAPI RegEnumKeyA( HKEY hkey, DWORD index, LPSTR name, DWORD name_len )
{
    return RegEnumKeyExA( hkey, index, name, &name_len, NULL, NULL, NULL, NULL );
}


/******************************************************************************
 *           RegQueryInfoKeyW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey       [I] Handle to key to query
 *    class      [O] Buffer for class string
 *    class_len  [O] Size of class string buffer
 *    reserved   [I] Reserved
 *    subkeys    [O] Buffer for number of subkeys
 *    max_subkey [O] Buffer for longest subkey name length
 *    max_class  [O] Buffer for longest class string length
 *    values     [O] Buffer for number of value entries
 *    max_value  [O] Buffer for longest value name length
 *    max_data   [O] Buffer for longest value data length
 *    security   [O] Buffer for security descriptor length
 *    modif      [O] Modification time
 *
 * - win95 allows class to be valid and class_len to be NULL
 * - winnt returns ERROR_INVALID_PARAMETER if class is valid and class_len is NULL
 * - both allow class to be NULL and class_len to be NULL
 * (it's hard to test validity, so test !NULL instead)
 */
DWORD WINAPI RegQueryInfoKeyW( HKEY hkey, LPWSTR class, LPDWORD class_len, LPDWORD reserved,
                               LPDWORD subkeys, LPDWORD max_subkey, LPDWORD max_class,
                               LPDWORD values, LPDWORD max_value, LPDWORD max_data,
                               LPDWORD security, FILETIME *modif )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_FULL_INFORMATION *info = (KEY_FULL_INFORMATION *)buffer;
    DWORD total_size;

    TRACE( "(0x%x,%p,%u,%p,%p,%p,%p,%p,%p,%p,%p)\n", hkey, class, class_len ? *class_len : 0,
           reserved, subkeys, max_subkey, values, max_value, max_data, security, modif );

    if (class && !class_len && !(GetVersion() & 0x80000000 /*NT*/))
        return ERROR_INVALID_PARAMETER;

    status = NtQueryKey( hkey, KeyFullInformation, buffer, sizeof(buffer), &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (class)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_FULL_INFORMATION *)buf_ptr;
            status = NtQueryKey( hkey, KeyFullInformation, buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (class_len && (info->ClassLength/sizeof(WCHAR) + 1 > *class_len))
        {
            status = STATUS_BUFFER_OVERFLOW;
        }
        else
        {
            memcpy( class, buf_ptr + info->ClassOffset, info->ClassLength );
            class[info->ClassLength/sizeof(WCHAR)] = 0;
        }
    }
    else status = STATUS_SUCCESS;

    if (class_len) *class_len = info->ClassLength / sizeof(WCHAR);
    if (subkeys) *subkeys = info->SubKeys;
    if (max_subkey) *max_subkey = info->MaxNameLen;
    if (max_class) *max_class = info->MaxClassLen;
    if (values) *values = info->Values;
    if (max_value) *max_value = info->MaxValueNameLen;
    if (max_data) *max_data = info->MaxValueDataLen;
    if (modif) *modif = *(FILETIME *)&info->LastWriteTime;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegQueryInfoKeyA   [ADVAPI32.@]
 */
DWORD WINAPI RegQueryInfoKeyA( HKEY hkey, LPSTR class, LPDWORD class_len, LPDWORD reserved,
                               LPDWORD subkeys, LPDWORD max_subkey, LPDWORD max_class,
                               LPDWORD values, LPDWORD max_value, LPDWORD max_data,
                               LPDWORD security, FILETIME *modif )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_FULL_INFORMATION *info = (KEY_FULL_INFORMATION *)buffer;
    DWORD total_size, len;

    TRACE( "(0x%x,%p,%u,%p,%p,%p,%p,%p,%p,%p,%p)\n", hkey, class, class_len ? *class_len : 0,
           reserved, subkeys, max_subkey, values, max_value, max_data, security, modif );

    if (class && !class_len && !(GetVersion() & 0x80000000 /*NT*/))
        return ERROR_INVALID_PARAMETER;

    status = NtQueryKey( hkey, KeyFullInformation, buffer, sizeof(buffer), &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (class || class_len)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_FULL_INFORMATION *)buf_ptr;
            status = NtQueryKey( hkey, KeyFullInformation, buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        RtlUnicodeToMultiByteSize( &len, (WCHAR *)(buf_ptr + info->ClassOffset), info->ClassLength);
        if (class_len)
        {
            if (len + 1 > *class_len) status = STATUS_BUFFER_OVERFLOW;
            *class_len = len;
        }
        if (class && !status)
        {
            RtlUnicodeToMultiByteN( class, len, NULL, (WCHAR *)(buf_ptr + info->ClassOffset),
                                    info->ClassLength );
            class[len] = 0;
        }
    }
    else status = STATUS_SUCCESS;

    if (subkeys) *subkeys = info->SubKeys;
    if (max_subkey) *max_subkey = info->MaxNameLen;
    if (max_class) *max_class = info->MaxClassLen;
    if (values) *values = info->Values;
    if (max_value) *max_value = info->MaxValueNameLen;
    if (max_data) *max_data = info->MaxValueDataLen;
    if (modif) *modif = *(FILETIME *)&info->LastWriteTime;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegCloseKey   [ADVAPI32.@]
 *
 * Releases the handle of the specified key
 *
 * PARAMS
 *    hkey [I] Handle of key to close
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 */
DWORD WINAPI RegCloseKey( HKEY hkey )
{
    if (!hkey || hkey >= 0x80000000) return ERROR_SUCCESS;
    return RtlNtStatusToDosError( NtClose( hkey ) );
}


/******************************************************************************
 *           RegDeleteKeyW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey   [I] Handle to open key
 *    name   [I] Name of subkey to delete
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 */
DWORD WINAPI RegDeleteKeyW( HKEY hkey, LPCWSTR name )
{
    DWORD ret;
    HKEY tmp;

    if (!name || !*name) return RtlNtStatusToDosError( NtDeleteKey( hkey ) );
    if (!(ret = RegOpenKeyExW( hkey, name, 0, 0, &tmp )))
    {
        ret = RtlNtStatusToDosError( NtDeleteKey( tmp ) );
        RegCloseKey( tmp );
    }
    return ret;
}


/******************************************************************************
 *           RegDeleteKeyA   [ADVAPI32.@]
 */
DWORD WINAPI RegDeleteKeyA( HKEY hkey, LPCSTR name )
{
    DWORD ret;
    HKEY tmp;

    if (!name || !*name) return RtlNtStatusToDosError( NtDeleteKey( hkey ) );
    if (!(ret = RegOpenKeyExA( hkey, name, 0, 0, &tmp )))
    {
        ret = RtlNtStatusToDosError( NtDeleteKey( tmp ) );
        RegCloseKey( tmp );
    }
    return ret;
}


/******************************************************************************
 *           RegDeleteTreeW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey   [I] Handle to open key
 *    name   [I] Name of subkey to delete
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 */
DWORD WINAPI RegDeleteTreeW( HKEY hkey, LPCWSTR name )
{
    WCHAR kname[256];
    DWORD ret, i;
    HKEY tmp;

    if ((ret = RegOpenKeyExW( hkey, name, 0, 0, &tmp )) != ERROR_SUCCESS)
        return ret;

    for (i = 0; ; i++) {
        DWORD len = sizeof(kname)/sizeof(WCHAR);
        ret = RegEnumKeyExW( tmp, i, kname, &len, NULL, NULL, NULL, NULL );
        if (ret == ERROR_MORE_DATA) {
            /* the max size of a key is 255 chars, so this shouldn't happen,
             * but maybe we'll want to handle it someday anyway. */
            FIXME("key name too long\n");
            break;
        }
        if (ret != ERROR_SUCCESS)
            break;

        if ((ret = RegDeleteTreeW( tmp, kname )) != ERROR_SUCCESS)
            break;
    }

    if (name) {
        if (ret == ERROR_SUCCESS)
            ret = RegDeleteKeyW( hkey, name );
    } else {
        DWORD maxlen = 0;
        WCHAR *pvname = NULL;
        ret = RegQueryInfoKeyW( tmp, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &maxlen, NULL, NULL, NULL );
        if (ret == ERROR_SUCCESS) {
            pvname = HeapAlloc( GetProcessHeap(), 0, (maxlen+1) * sizeof(WCHAR) );
            if (!pvname) ret = ERROR_NOT_ENOUGH_MEMORY;
        }
        if (ret == ERROR_SUCCESS) {
            for (i = 0; ; i++) {
                DWORD len = maxlen+1;
                ret = RegEnumValueW( tmp, i, pvname, &len, NULL, NULL, NULL, NULL );
                if (ret == ERROR_MORE_DATA) {
                    /* shouldn't happen (unless someone else is modifying the key at the same time,
                     * but that would be a bad idea anyway), not sure whether to handle */
                    FIXME("value name too long\n");
                    break;
                }
                if (ret != ERROR_SUCCESS)
                    break;
                if ((ret = RegDeleteValueW( tmp, pvname )) != ERROR_SUCCESS)
                    break;
            }
            HeapFree( GetProcessHeap(), 0, pvname );
        }
    }

    RegCloseKey( tmp );
    return ret;
}


/******************************************************************************
 *           RegDeleteTreeA   [ADVAPI32.@]
 */
DWORD WINAPI RegDeleteTreeA( HKEY hkey, LPCSTR name )
{
    ANSI_STRING nameA;
    UNICODE_STRING nameW;
    NTSTATUS status;
    DWORD ret = ERROR_SUCCESS;

    RtlInitAnsiString( &nameA, name );
    RtlInitUnicodeString( &nameW, NULL );

    if (name && (status = RtlAnsiStringToUnicodeString( &nameW, &nameA, TRUE )) != STATUS_SUCCESS)
        ret = RtlNtStatusToDosError( status );

    if (ret == ERROR_SUCCESS)
        ret = RegDeleteTreeW( hkey, nameW.Buffer );

    RtlFreeUnicodeString( &nameW );
    return ret;
}



/******************************************************************************
 *           RegSetValueExW   [ADVAPI32.@]
 *
 * Sets the data and type of a value under a register key
 *
 * PARAMS
 *    hkey       [I] Handle of key to set value for
 *    name       [I] Name of value to set
 *    reserved   [I] Reserved - must be zero
 *    type       [I] Flag for value type
 *    data       [I] Address of value data
 *    count      [I] Size of value data
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 *
 * NOTES
 *   win95 does not care about count for REG_SZ and finds out the len by itself (js)
 *   NT does definitely care (aj)
 */
DWORD WINAPI RegSetValueExW( HKEY hkey, LPCWSTR name, DWORD reserved,
                             DWORD type, CONST BYTE *data, DWORD count )
{
    UNICODE_STRING nameW;

    if (GetVersion() & 0x80000000)  /* win95 */
    {
        if (type == REG_SZ) count = (strlenW( (WCHAR *)data ) + 1) * sizeof(WCHAR);
    }
    else if (count && is_string(type))
    {
        LPCWSTR str = (LPCWSTR)data;
        /* if user forgot to count terminating null, add it (yes NT does this) */
        if (str[count / sizeof(WCHAR) - 1] && !str[count / sizeof(WCHAR)])
            count += sizeof(WCHAR);
    }

    RtlInitUnicodeString( &nameW, name );
    return RtlNtStatusToDosError( NtSetValueKey( hkey, &nameW, 0, type, data, count ) );
}


/******************************************************************************
 *           RegSetValueExA   [ADVAPI32.@]
 */
DWORD WINAPI RegSetValueExA( HKEY hkey, LPCSTR name, DWORD reserved, DWORD type,
                             CONST BYTE *data, DWORD count )
{
    ANSI_STRING nameA;
    WCHAR *dataW = NULL;
    NTSTATUS status;

    if (GetVersion() & 0x80000000)  /* win95 */
    {
        if (type == REG_SZ) count = strlen((LPCSTR)data) + 1;
    }
    else if (count && is_string(type))
    {
        /* if user forgot to count terminating null, add it (yes NT does this) */
        if (data[count-1] && !data[count]) count++;
    }

    if (is_string( type )) /* need to convert to Unicode */
    {
        DWORD lenW;
        RtlMultiByteToUnicodeSize( &lenW, (LPCSTR)data, count );
        if (!(dataW = HeapAlloc( GetProcessHeap(), 0, lenW ))) return ERROR_OUTOFMEMORY;
        RtlMultiByteToUnicodeN( dataW, lenW, NULL, (LPCSTR)data, count );
        count = lenW;
        data = (BYTE *)dataW;
    }

    RtlInitAnsiString( &nameA, name );
    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
        status = NtSetValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString, 0, type, data, count );
    }
    if (dataW) HeapFree( GetProcessHeap(), 0, dataW );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegSetValueW   [ADVAPI32.@]
 */
DWORD WINAPI RegSetValueW( HKEY hkey, LPCWSTR name, DWORD type, LPCWSTR data, DWORD count )
{
    HKEY subkey = hkey;
    DWORD ret;

    TRACE("(0x%x,%s,%u,%s,%u)\n", hkey, debugstr_w(name), type, debugstr_w(data), count );

    if (type != REG_SZ) return ERROR_INVALID_PARAMETER;

    if (name && name[0])  /* need to create the subkey */
    {
        if ((ret = RegCreateKeyW( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }

    ret = RegSetValueExW( subkey, NULL, 0, REG_SZ, (LPBYTE)data,
                          (strlenW( data ) + 1) * sizeof(WCHAR) );
    if (subkey != hkey) RegCloseKey( subkey );
    return ret;
}


/******************************************************************************
 *           RegSetValueA   [ADVAPI32.@]
 */
DWORD WINAPI RegSetValueA( HKEY hkey, LPCSTR name, DWORD type, LPCSTR data, DWORD count )
{
    HKEY subkey = hkey;
    DWORD ret;

    TRACE("(0x%x,%s,%u,%s,%u)\n", hkey, debugstr_a(name), type, debugstr_a(data), count );

    if (type != REG_SZ) return ERROR_INVALID_PARAMETER;

    if (name && name[0])  /* need to create the subkey */
    {
        if ((ret = RegCreateKeyA( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }
    ret = RegSetValueExA( subkey, NULL, 0, REG_SZ, (LPBYTE)data, strlen(data)+1 );
    if (subkey != hkey) RegCloseKey( subkey );
    return ret;
}



/******************************************************************************
 *           RegQueryValueExW   [ADVAPI32.@]
 *
 * Retrieves type and data for a specified name associated with an open key
 *
 * PARAMS
 *    hkey      [I]   Handle of key to query
 *    name      [I]   Name of value to query
 *    reserved  [I]   Reserved - must be NULL
 *    type      [O]   Address of buffer for value type.  If NULL, the type
 *                        is not required.
 *    data      [O]   Address of data buffer.  If NULL, the actual data is
 *                        not required.
 *    count     [I/O] Address of data buffer size
 *
 * RETURNS
 *    ERROR_SUCCESS:   Success
 *    ERROR_MORE_DATA: !!! if the specified buffer is not big enough to hold the data
 * 		       buffer is left untouched. The MS-documentation is wrong (js) !!!
 */
DWORD WINAPI RegQueryValueExW( HKEY hkey, LPCWSTR name, LPDWORD reserved, LPDWORD type,
                               LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    UNICODE_STRING name_str;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    static const int info_size = info->Data - (UCHAR *)info;

    TRACE("(0x%x,%s,%p,%p,%p,%p=%u)\n",
          hkey, debugstr_w(name), reserved, type, data, count, count ? *count : 0 );

    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;

    RtlInitUnicodeString( &name_str, name );

    if (data) total_size = min( sizeof(buffer), *count + info_size );
    else total_size = info_size;

    status = NtQueryValueKey( hkey, &name_str, KeyValuePartialInformation,
                              buffer, total_size, &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (data)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW && total_size - info_size <= *count)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_VALUE_PARTIAL_INFORMATION *)buf_ptr;
            status = NtQueryValueKey( hkey, &name_str, KeyValuePartialInformation,
                                      buf_ptr, total_size, &total_size );
        }

        if (!status)
        {
            memcpy( data, buf_ptr + info_size, total_size - info_size );
            /* if the type is REG_SZ and data is not 0-terminated
             * and there is enough space in the buffer NT appends a \0 */
            if (total_size - info_size <= *count-sizeof(WCHAR) && is_string(info->Type))
            {
                WCHAR *ptr = (WCHAR *)(data + total_size - info_size);
                if (ptr > (WCHAR *)data && ptr[-1]) *ptr = 0;
            }
        }
        else if (status != STATUS_BUFFER_OVERFLOW) goto done;
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = total_size - info_size;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}


/******************************************************************************
 *           RegQueryValueExA   [ADVAPI32.@]
 *
 * NOTES:
 * the documentation is wrong: if the buffer is too small it remains untouched
 */
DWORD WINAPI RegQueryValueExA( HKEY hkey, LPCSTR name, LPDWORD reserved, LPDWORD type,
                               LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    ANSI_STRING nameA;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    static const int info_size = info->Data - (UCHAR *)info;

    TRACE("(0x%x,%s,%p,%p,%p,%p=%u)\n",
          hkey, debugstr_a(name), reserved, type, data, count, count ? *count : 0 );

    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;

    RtlInitAnsiString( &nameA, name );
    if ((status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                &nameA, FALSE )))
        return RtlNtStatusToDosError(status);

    status = NtQueryValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString,
                              KeyValuePartialInformation, buffer, sizeof(buffer), &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    /* we need to fetch the contents for a string type even if not requested,
     * because we need to compute the length of the ASCII string. */
    if (data || is_string(info->Type))
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
            {
                status = STATUS_NO_MEMORY;
                goto done;
            }
            info = (KEY_VALUE_PARTIAL_INFORMATION *)buf_ptr;
            status = NtQueryValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString,
                                    KeyValuePartialInformation, buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (is_string(info->Type))
        {
            DWORD len;

            RtlUnicodeToMultiByteSize( &len, (WCHAR *)(buf_ptr + info_size),
                                       total_size - info_size );
            if (data && len)
            {
                if (len > *count) status = STATUS_BUFFER_OVERFLOW;
                else
                {
                    RtlUnicodeToMultiByteN( (LPSTR)data, len, NULL, (WCHAR *)(buf_ptr + info_size),
                                            total_size - info_size );
                    /* if the type is REG_SZ and data is not 0-terminated
                     * and there is enough space in the buffer NT appends a \0 */
                    if (len < *count && data[len-1]) data[len] = 0;
                }
            }
            total_size = len + info_size;
        }
        else if (data)
        {
            if (total_size - info_size > *count) status = STATUS_BUFFER_OVERFLOW;
            else memcpy( data, buf_ptr + info_size, total_size - info_size );
        }
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = total_size - info_size;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}


/******************************************************************************
 *           RegQueryValueW   [ADVAPI32.@]
 */
DWORD WINAPI RegQueryValueW( HKEY hkey, LPCWSTR name, LPWSTR data, LPLONG count )
{
    DWORD ret;
    HKEY subkey = hkey;

    TRACE("(%x,%s,%p,%d)\n", hkey, debugstr_w(name), data, count ? *count : 0 );

    if (name && name[0])
    {
        if ((ret = RegOpenKeyW( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }
    ret = RegQueryValueExW( subkey, NULL, NULL, NULL, (LPBYTE)data, (LPDWORD)count );
    if (subkey != hkey) RegCloseKey( subkey );
    if (ret == ERROR_FILE_NOT_FOUND)
    {
        /* return empty string if default value not found */
        if (data) *data = 0;
        if (count) *count = 1;
        ret = ERROR_SUCCESS;
    }
    return ret;
}


/******************************************************************************
 *           RegQueryValueA   [ADVAPI32.@]
 */
DWORD WINAPI RegQueryValueA( HKEY hkey, LPCSTR name, LPSTR data, LPLONG count )
{
    DWORD ret;
    HKEY subkey = hkey;

    TRACE("(%x,%s,%p,%d)\n", hkey, debugstr_a(name), data, count ? *count : 0 );

    if (name && name[0])
    {
        if ((ret = RegOpenKeyA( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }
    ret = RegQueryValueExA( subkey, NULL, NULL, NULL, (LPBYTE)data, (LPDWORD)count );
    if (subkey != hkey) RegCloseKey( subkey );
    if (ret == ERROR_FILE_NOT_FOUND)
    {
        /* return empty string if default value not found */
        if (data) *data = 0;
        if (count) *count = 1;
        ret = ERROR_SUCCESS;
    }
    return ret;
}


/******************************************************************************
 *           RegEnumValueW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey       [I] Handle to key to query
 *    index      [I] Index of value to query
 *    value      [O] Value string
 *    val_count  [I/O] Size of value buffer (in wchars)
 *    reserved   [I] Reserved
 *    type       [O] Type code
 *    data       [O] Value data
 *    count      [I/O] Size of data buffer (in bytes)
 */

DWORD WINAPI RegEnumValueW( HKEY hkey, DWORD index, LPWSTR value, LPDWORD val_count,
                            LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_FULL_INFORMATION *info = (KEY_VALUE_FULL_INFORMATION *)buffer;
    static const int info_size = (char *)info->Name - (char *)info;

    TRACE("(%x,%u,%p,%p,%p,%p,%p,%p)\n",
          hkey, index, value, val_count, reserved, type, data, count );

    /* NT only checks count, not val_count */
    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;

    total_size = info_size + (MAX_PATH + 1) * sizeof(WCHAR);
    if (data) total_size += *count;
    total_size = min( sizeof(buffer), total_size );

    status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                  buffer, total_size, &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (value || data)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_VALUE_FULL_INFORMATION *)buf_ptr;
            status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                          buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (value)
        {
            if (info->NameLength/sizeof(WCHAR) >= *val_count)
            {
                status = STATUS_BUFFER_OVERFLOW;
                goto done;
            }
            memcpy( value, info->Name, info->NameLength );
            *val_count = info->NameLength / sizeof(WCHAR);
            value[*val_count] = 0;
        }

        if (data)
        {
            if (total_size - info->DataOffset > *count)
            {
                status = STATUS_BUFFER_OVERFLOW;
                goto done;
            }
            memcpy( data, buf_ptr + info->DataOffset, total_size - info->DataOffset );
            if (total_size - info->DataOffset <= *count-sizeof(WCHAR) && is_string(info->Type))
            {
                /* if the type is REG_SZ and data is not 0-terminated
                 * and there is enough space in the buffer NT appends a \0 */
                WCHAR *ptr = (WCHAR *)(data + total_size - info->DataOffset);
                if (ptr > (WCHAR *)data && ptr[-1]) *ptr = 0;
            }
        }
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = info->DataLength;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}


/******************************************************************************
 *           RegEnumValueA   [ADVAPI32.@]
 */
DWORD WINAPI RegEnumValueA( HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
                            LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_FULL_INFORMATION *info = (KEY_VALUE_FULL_INFORMATION *)buffer;
    static const int info_size = (char *)info->Name - (char *)info;

    TRACE("(%x,%u,%p,%p,%p,%p,%p,%p)\n",
          hkey, index, value, val_count, reserved, type, data, count );

    /* NT only checks count, not val_count */
    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;

    total_size = info_size + (MAX_PATH + 1) * sizeof(WCHAR);
    if (data) total_size += *count;
    total_size = min( sizeof(buffer), total_size );

    status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                  buffer, total_size, &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    /* we need to fetch the contents for a string type even if not requested,
     * because we need to compute the length of the ASCII string. */
    if (value || data || is_string(info->Type))
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_VALUE_FULL_INFORMATION *)buf_ptr;
            status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                          buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (value)
        {
            DWORD len;

            RtlUnicodeToMultiByteSize( &len, info->Name, info->NameLength );
            if (len >= *val_count)
            {
                status = STATUS_BUFFER_OVERFLOW;
                goto done;
            }
            RtlUnicodeToMultiByteN( value, len, NULL, info->Name, info->NameLength );
            value[len] = 0;
            *val_count = len;
        }

        if (is_string(info->Type))
        {
            DWORD len;
            RtlUnicodeToMultiByteSize( &len, (WCHAR *)(buf_ptr + info->DataOffset),
                                       total_size - info->DataOffset );
            if (data && len)
            {
                if (len > *count)
                {
                    status = STATUS_BUFFER_OVERFLOW;
                    goto done;
                }
                RtlUnicodeToMultiByteN( (LPSTR)data, len, NULL, (WCHAR *)(buf_ptr + info->DataOffset),
                                        total_size - info->DataOffset );
                /* if the type is REG_SZ and data is not 0-terminated
                 * and there is enough space in the buffer NT appends a \0 */
                if (len < *count && data[len-1]) data[len] = 0;
            }
            info->DataLength = len;
        }
        else if (data)
        {
            if (total_size - info->DataOffset > *count) status = STATUS_BUFFER_OVERFLOW;
            else memcpy( data, buf_ptr + info->DataOffset, total_size - info->DataOffset );
        }
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = info->DataLength;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}



/******************************************************************************
 *           RegDeleteValueW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey   [I] handle to key
 *    name   [I] name of value to delete
 *
 * RETURNS
 *    error status
 */
DWORD WINAPI RegDeleteValueW( HKEY hkey, LPCWSTR name )
{
    UNICODE_STRING nameW;
    RtlInitUnicodeString( &nameW, name );
    return RtlNtStatusToDosError( NtDeleteValueKey( hkey, &nameW ) );
}


/******************************************************************************
 *           RegDeleteValueA   [ADVAPI32.@]
 */
DWORD WINAPI RegDeleteValueA( HKEY hkey, LPCSTR name )
{
    STRING nameA;
    NTSTATUS status;

    RtlInitAnsiString( &nameA, name );
    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
        status = NtDeleteValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString );
    }
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 *           RegLoadKeyW   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey      [I] Handle of open key
 *    subkey    [I] Address of name of subkey
 *    filename  [I] Address of filename for registry information
 */
LONG WINAPI RegLoadKeyW( HKEY hkey, LPCWSTR subkey, LPCWSTR filename )
{
    HANDLE file;
    DWORD ret, len, err = GetLastError();

    TRACE( "(%x,%s,%s)\n", hkey, debugstr_w(subkey), debugstr_w(filename) );

    if (!filename || !*filename) return ERROR_INVALID_PARAMETER;
    if (!subkey || !*subkey) return ERROR_INVALID_PARAMETER;

    len = strlenW( subkey ) * sizeof(WCHAR);
    if (len > MAX_PATH*sizeof(WCHAR)) return ERROR_INVALID_PARAMETER;

    if ((file = CreateFileW( filename, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, 0 )) == INVALID_HANDLE_VALUE)
    {
        ret = GetLastError();
        goto done;
    }

    SERVER_START_REQ( load_registry )
    {
        req->hkey  = hkey;
        req->file  = file;
        wine_server_add_data( req, subkey, len );
        ret = RtlNtStatusToDosError( wine_server_call(req) );
    }
    SERVER_END_REQ;
    CloseHandle( file );

 done:
    SetLastError( err );  /* restore the last error code */
    return ret;
}


/******************************************************************************
 *           RegLoadKeyA   [ADVAPI32.@]
 */
LONG WINAPI RegLoadKeyA( HKEY hkey, LPCSTR subkey, LPCSTR filename )
{
    WCHAR buffer[MAX_PATH];
    HANDLE file;
    DWORD ret, len, err = GetLastError();

    TRACE( "(%x,%s,%s)\n", hkey, debugstr_a(subkey), debugstr_a(filename) );

    if (!filename || !*filename) return ERROR_INVALID_PARAMETER;
    if (!subkey || !*subkey) return ERROR_INVALID_PARAMETER;

    if (!(len = MultiByteToWideChar( CP_ACP, 0, subkey, strlen(subkey), buffer, MAX_PATH )))
        return ERROR_INVALID_PARAMETER;

    if ((file = CreateFileA( filename, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, 0 )) == INVALID_HANDLE_VALUE)
    {
        ret = GetLastError();
        goto done;
    }

    SERVER_START_REQ( load_registry )
    {
        req->hkey  = hkey;
        req->file  = file;
        wine_server_add_data( req, buffer, len * sizeof(WCHAR) );
        ret = RtlNtStatusToDosError( wine_server_call(req) );
    }
    SERVER_END_REQ;
    CloseHandle( file );

 done:
    SetLastError( err );  /* restore the last error code */
    return ret;
}


/******************************************************************************
 *           RegSaveKeyA   [ADVAPI32.@]
 *
 * PARAMS
 *    hkey   [I] Handle of key where save begins
 *    lpFile [I] Address of filename to save to
 *    sa     [I] Address of security structure
 */
LONG WINAPI RegSaveKeyA( HKEY hkey, LPCSTR file, LPSECURITY_ATTRIBUTES sa )
{
    char buffer[1024];
    int count = 0;
    LPSTR name;
    DWORD ret, err;
    HFILE handle;

    TRACE( "(%x,%s,%p)\n", hkey, debugstr_a(file), sa );

    if (!file || !*file) return ERROR_INVALID_PARAMETER;

    err = GetLastError();
    GetFullPathNameA( file, sizeof(buffer), buffer, &name );
    for (;;)
    {
        sprintf( name, "reg%04x.tmp", count++ );
        handle = CreateFileA( buffer, GENERIC_WRITE, 0, NULL,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
        if (handle != INVALID_HANDLE_VALUE) break;
        if ((ret = GetLastError()) != ERROR_ALREADY_EXISTS) goto done;

        /* Something gone haywire ? Please report if this happens abnormally */
        if (count >= 100)
            MESSAGE("Wow, we are already fiddling with a temp file %s with an ordinal as high as %d !\nYou might want to delete all corresponding temp files in that directory.\n", buffer, count);
    }

    SERVER_START_REQ( save_registry )
    {
        req->hkey = hkey;
        req->file = handle;
        ret = RtlNtStatusToDosError( wine_server_call( req ) );
    }
    SERVER_END_REQ;

    CloseHandle( handle );
    if (!ret)
    {
        if (!MoveFileExA( buffer, file, MOVEFILE_REPLACE_EXISTING ))
        {
            ERR( "Failed to move %s to %s\n", buffer, file );
            ret = GetLastError();
        }
    }
    if (ret) DeleteFileA( buffer );

done:
    SetLastError( err );  /* restore last error code */
    return ret;
}


/******************************************************************************
 *           RegSaveKeyW   [ADVAPI32.@]
 */
LONG WINAPI RegSaveKeyW( HKEY hkey, LPCWSTR file, LPSECURITY_ATTRIBUTES sa )
{
    LPSTR fileA = FILE_strdupWtoA( GetProcessHeap(), 0, file );
    DWORD ret = RegSaveKeyA( hkey, fileA, sa );
    if (fileA) HeapFree( GetProcessHeap(), 0, fileA );
    return ret;
}


/******************************************************************************
 * RegRestoreKeyW [ADVAPI32.@]
 *
 * PARAMS
 *    hkey    [I] Handle of key where restore begins
 *    lpFile  [I] Address of filename containing saved tree
 *    dwFlags [I] Optional flags
 */
LONG WINAPI RegRestoreKeyW( HKEY hkey, LPCWSTR lpFile, DWORD dwFlags )
{
    TRACE("(%x,%s,%u)\n",hkey,debugstr_w(lpFile),dwFlags);

    /* It seems to do this check before the hkey check */
    if (!lpFile || !*lpFile)
        return ERROR_INVALID_PARAMETER;

    FIXME("(%x,%s,%u): stub\n",hkey,debugstr_w(lpFile),dwFlags);

    /* Check for file existence */

    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegRestoreKeyA [ADVAPI32.@]
 */
LONG WINAPI RegRestoreKeyA( HKEY hkey, LPCSTR lpFile, DWORD dwFlags )
{
    LPWSTR lpFileW = HEAP_strdupAtoW( GetProcessHeap(), 0, lpFile );
    LONG ret = RegRestoreKeyW( hkey, lpFileW, dwFlags );
    HeapFree( GetProcessHeap(), 0, lpFileW );
    return ret;
}


/******************************************************************************
 * RegUnLoadKeyW [ADVAPI32.@]
 *
 * PARAMS
 *    hkey     [I] Handle of open key
 *    lpSubKey [I] Address of name of subkey to unload
 */
LONG WINAPI RegUnLoadKeyW( HKEY hkey, LPCWSTR lpSubKey )
{
    FIXME("(%x,%s): stub\n",hkey, debugstr_w(lpSubKey));
    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegUnLoadKeyA [ADVAPI32.@]
 */
LONG WINAPI RegUnLoadKeyA( HKEY hkey, LPCSTR lpSubKey )
{
    LPWSTR lpSubKeyW = HEAP_strdupAtoW( GetProcessHeap(), 0, lpSubKey );
    LONG ret = RegUnLoadKeyW( hkey, lpSubKeyW );
    if(lpSubKeyW) HeapFree( GetProcessHeap(), 0, lpSubKeyW);
    return ret;
}


/******************************************************************************
 * RegReplaceKeyW [ADVAPI32.@]
 *
 * PARAMS
 *    hkey      [I] Handle of open key
 *    lpSubKey  [I] Address of name of subkey
 *    lpNewFile [I] Address of filename for file with new data
 *    lpOldFile [I] Address of filename for backup file
 */
LONG WINAPI RegReplaceKeyW( HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpNewFile,
                              LPCWSTR lpOldFile )
{
    FIXME("(%x,%s,%s,%s): stub\n", hkey, debugstr_w(lpSubKey),
          debugstr_w(lpNewFile),debugstr_w(lpOldFile));
    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegReplaceKeyA [ADVAPI32.@]
 */
LONG WINAPI RegReplaceKeyA( HKEY hkey, LPCSTR lpSubKey, LPCSTR lpNewFile,
                              LPCSTR lpOldFile )
{
    LPWSTR lpSubKeyW = HEAP_strdupAtoW( GetProcessHeap(), 0, lpSubKey );
    LPWSTR lpNewFileW = HEAP_strdupAtoW( GetProcessHeap(), 0, lpNewFile );
    LPWSTR lpOldFileW = HEAP_strdupAtoW( GetProcessHeap(), 0, lpOldFile );
    LONG ret = RegReplaceKeyW( hkey, lpSubKeyW, lpNewFileW, lpOldFileW );
    HeapFree( GetProcessHeap(), 0, lpOldFileW );
    HeapFree( GetProcessHeap(), 0, lpNewFileW );
    HeapFree( GetProcessHeap(), 0, lpSubKeyW );
    return ret;
}


/******************************************************************************
 * RegSetKeySecurity [ADVAPI32.@]
 *
 * PARAMS
 *    hkey          [I] Open handle of key to set
 *    SecurityInfo  [I] Descriptor contents
 *    pSecurityDesc [I] Address of descriptor for key
 */
LONG WINAPI RegSetKeySecurity( HKEY hkey, SECURITY_INFORMATION SecurityInfo,
                               PSECURITY_DESCRIPTOR pSecurityDesc )
{
    TRACE("(%x,%u,%p)\n",hkey,SecurityInfo,pSecurityDesc);

    /* It seems to perform this check before the hkey check */
    if ((SecurityInfo & OWNER_SECURITY_INFORMATION) ||
        (SecurityInfo & GROUP_SECURITY_INFORMATION) ||
        (SecurityInfo & DACL_SECURITY_INFORMATION) ||
        (SecurityInfo & SACL_SECURITY_INFORMATION)) {
        /* Param OK */
    } else
        return ERROR_INVALID_PARAMETER;

    if (!pSecurityDesc)
        return ERROR_INVALID_PARAMETER;

    FIXME(":(%x,%u,%p): stub\n",hkey,SecurityInfo,pSecurityDesc);

    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegGetKeySecurity [ADVAPI32.@]
 * Retrieves a copy of security descriptor protecting the registry key
 *
 * PARAMS
 *    hkey                   [I]   Open handle of key to set
 *    SecurityInformation    [I]   Descriptor contents
 *    pSecurityDescriptor    [O]   Address of descriptor for key
 *    lpcbSecurityDescriptor [I/O] Address of size of buffer and description
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 */
LONG WINAPI RegGetKeySecurity( HKEY hkey, SECURITY_INFORMATION SecurityInformation,
                               PSECURITY_DESCRIPTOR pSecurityDescriptor,
                               LPDWORD lpcbSecurityDescriptor )
{
    TRACE("(%x,%u,%p,%u)\n",hkey,SecurityInformation,pSecurityDescriptor,
          lpcbSecurityDescriptor?*lpcbSecurityDescriptor:0);

    /* FIXME: Check for valid SecurityInformation values */

    if (*lpcbSecurityDescriptor < sizeof(SECURITY_DESCRIPTOR))
        return ERROR_INSUFFICIENT_BUFFER;

    FIXME("(%x,%u,%p,%u): stub\n",hkey,SecurityInformation,
          pSecurityDescriptor,lpcbSecurityDescriptor?*lpcbSecurityDescriptor:0);

    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegFlushKey [ADVAPI32.@]
 * Immediately writes key to registry.
 * Only returns after data has been written to disk.
 *
 * FIXME: does it really wait until data is written ?
 *
 * PARAMS
 *    hkey [I] Handle of key to write
 *
 * RETURNS
 *    Success: ERROR_SUCCESS
 *    Failure: Error code
 */
DWORD WINAPI RegFlushKey( HKEY hkey )
{
   TRACE ("(0x%x\n", hkey);

   return RtlNtStatusToDosError (NtFlushKey (hkey));
}


/******************************************************************************
 * RegConnectRegistryW [ADVAPI32.@]
 *
 * PARAMS
 *    lpMachineName [I] Address of name of remote computer
 *    hHey          [I] Predefined registry handle
 *    phkResult     [I] Address of buffer for remote registry handle
 */
LONG WINAPI RegConnectRegistryW( LPCWSTR lpMachineName, HKEY hKey,
                                   LPHKEY phkResult )
{
    TRACE("(%s,%x,%p): stub\n",debugstr_w(lpMachineName),hKey,phkResult);

    if (!lpMachineName || !*lpMachineName) {
        /* Use the local machine name */
        return RegOpenKeyA( hKey, "", phkResult );
    }

    FIXME("Cannot connect to %s\n",debugstr_w(lpMachineName));
    return ERROR_BAD_NETPATH;
}


/******************************************************************************
 * RegConnectRegistryA [ADVAPI32.@]
 */
LONG WINAPI RegConnectRegistryA( LPCSTR machine, HKEY hkey, LPHKEY reskey )
{
    LPWSTR machineW = HEAP_strdupAtoW( GetProcessHeap(), 0, machine );
    DWORD ret = RegConnectRegistryW( machineW, hkey, reskey );
    HeapFree( GetProcessHeap(), 0, machineW );
    return ret;
}


/******************************************************************************
 * RegNotifyChangeKeyValue [ADVAPI32.@]
 *
 * PARAMS
 *    hkey            [I] Handle of key to watch
 *    fWatchSubTree   [I] Flag for subkey notification
 *    fdwNotifyFilter [I] Changes to be reported
 *    hEvent          [I] Handle of signaled event
 *    fAsync          [I] Flag for asynchronous reporting
 */
LONG WINAPI RegNotifyChangeKeyValue( HKEY hkey, BOOL fWatchSubTree,
                                     DWORD fdwNotifyFilter, HANDLE hEvent,
                                     BOOL fAsync )
{
    FIXME("(%x,%i,%u,%x,%i): stub\n",hkey,fWatchSubTree,fdwNotifyFilter,
          hEvent,fAsync);
    return ERROR_SUCCESS;
}
