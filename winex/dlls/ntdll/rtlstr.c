/*
 * Rtl string functions
 *
 * Copyright (C) 1996-1998 Marcus Meissner
 * Copyright (C) 2000      Alexandre Julliard
 */

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "winternl.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);


UINT NlsAnsiCodePage = 1252;
BYTE NlsMbCodePageTag = 0;
BYTE NlsMbOemCodePageTag = 0;

static const union cptable *ansi_table;
static const union cptable *oem_table;

inline static const union cptable *get_ansi_table(void)
{
    if (!ansi_table) ansi_table = wine_cp_get_table( 1252 );
    return ansi_table;
}

inline static const union cptable *get_oem_table(void)
{
    if (!oem_table) oem_table = wine_cp_get_table( 437 );
    return oem_table;
}


/**************************************************************************
 *	__wine_init_codepages   (NTDLL.@)
 *
 * Set the code page once kernel32 is loaded. Should be done differently.
 */
void __wine_init_codepages( const union cptable *ansi, const union cptable *oem )
{
    ansi_table = ansi;
    oem_table = oem;
    NlsAnsiCodePage = ansi->info.codepage;
}


/**************************************************************************
 *	RtlInitAnsiString   (NTDLL.@)
 */
void WINAPI RtlInitAnsiString( PSTRING target, LPCSTR source)
{
    if ((target->Buffer = (LPSTR)source))
    {
        target->Length = strlen(source);
        target->MaximumLength = target->Length + 1;
    }
    else target->Length = target->MaximumLength = 0;
}


/**************************************************************************
 *	RtlInitString   (NTDLL.@)
 */
void WINAPI RtlInitString( PSTRING target, LPCSTR source )
{
    RtlInitAnsiString( target, source );
}


/**************************************************************************
 *	RtlFreeAnsiString   (NTDLL.@)
 */
void WINAPI RtlFreeAnsiString( PSTRING str )
{
    if (str->Buffer) RtlFreeHeap( GetProcessHeap(), 0, str->Buffer );
}


/**************************************************************************
 *	RtlFreeOemString   (NTDLL.@)
 */
void WINAPI RtlFreeOemString( PSTRING str )
{
    RtlFreeAnsiString( str );
}


/**************************************************************************
 *	RtlCopyString   (NTDLL.@)
 */
void WINAPI RtlCopyString( STRING *dst, const STRING *src )
{
    if (src)
    {
        unsigned int len = min( src->Length, dst->MaximumLength );
        memcpy( dst->Buffer, src->Buffer, len );
        dst->Length = len;
    }
    else dst->Length = 0;
}


/**************************************************************************
 *	RtlInitUnicodeString   (NTDLL.@)
 */
void WINAPI RtlInitUnicodeString( PUNICODE_STRING target, LPCWSTR source )
{
    if ((target->Buffer = (LPWSTR)source))
    {
        target->Length = strlenW(source) * sizeof(WCHAR);
        target->MaximumLength = target->Length + sizeof(WCHAR);
    }
    else target->Length = target->MaximumLength = 0;
}


/**************************************************************************
 *	RtlCreateUnicodeString   (NTDLL.@)
 */
BOOLEAN WINAPI RtlCreateUnicodeString( PUNICODE_STRING target, LPCWSTR src )
{
    int len = (strlenW(src) + 1) * sizeof(WCHAR);
    if (!(target->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, len ))) return FALSE;
    memcpy( target->Buffer, src, len );
    target->MaximumLength = len;
    target->Length = len - 2;
    return TRUE;
}


/**************************************************************************
 *	RtlCreateUnicodeStringFromAsciiz   (NTDLL.@)
 */
BOOLEAN WINAPI RtlCreateUnicodeStringFromAsciiz( PUNICODE_STRING target, LPCSTR src )
{
    STRING ansi;
    RtlInitAnsiString( &ansi, src );
    return !RtlAnsiStringToUnicodeString( target, &ansi, TRUE );
}


/**************************************************************************
 *	RtlFreeUnicodeString   (NTDLL.@)
 */
void WINAPI RtlFreeUnicodeString( PUNICODE_STRING str )
{
    if (str->Buffer) RtlFreeHeap( GetProcessHeap(), 0, str->Buffer );
}


/**************************************************************************
 *	RtlCopyUnicodeString   (NTDLL.@)
 */
void WINAPI RtlCopyUnicodeString( UNICODE_STRING *dst, const UNICODE_STRING *src )
{
    if (src)
    {
        unsigned int len = min( src->Length, dst->MaximumLength );
        memcpy( dst->Buffer, src->Buffer, len );
        dst->Length = len;
        /* append terminating NULL if enough space */
        if (len < dst->MaximumLength) dst->Buffer[len / sizeof(WCHAR)] = 0;
    }
    else dst->Length = 0;
}


/**************************************************************************
 *	RtlEraseUnicodeString   (NTDLL.@)
 */
void WINAPI RtlEraseUnicodeString( UNICODE_STRING *str )
{
    if (str->Buffer)
    {
        memset( str->Buffer, 0, str->MaximumLength );
        str->Length = 0;
    }
}

/*
    COMPARISON FUNCTIONS
*/

/******************************************************************************
 *	RtlCompareString   (NTDLL.@)
 */
LONG WINAPI RtlCompareString( const STRING *s1, const STRING *s2, BOOLEAN CaseInsensitive )
{
    unsigned int len;
    LONG ret = 0;
    LPCSTR p1, p2;

    len = min(s1->Length, s2->Length);
    p1 = s1->Buffer;
    p2 = s2->Buffer;

    if (CaseInsensitive)
    {
        while (!ret && len--) ret = toupper(*p1++) - toupper(*p2++);
    }
    else
    {
        while (!ret && len--) ret = *p1++ - *p2++;
    }
    if (!ret) ret = s1->Length - s2->Length;
    return ret;
}


/******************************************************************************
 *	RtlCompareUnicodeString   (NTDLL.@)
 */
LONG WINAPI RtlCompareUnicodeString( const UNICODE_STRING *s1, const UNICODE_STRING *s2,
                                     BOOLEAN CaseInsensitive )
{
    unsigned int len;
    LONG ret = 0;
    LPCWSTR p1, p2;

    len = min(s1->Length, s2->Length) / sizeof(WCHAR);
    p1 = s1->Buffer;
    p2 = s2->Buffer;

    if (CaseInsensitive)
    {
        while (!ret && len--) ret = toupperW(*p1++) - toupperW(*p2++);
    }
    else
    {
        while (!ret && len--) ret = *p1++ - *p2++;
    }
    if (!ret) ret = s1->Length - s2->Length;
    return ret;
}


/**************************************************************************
 *	RtlEqualString   (NTDLL.@)
 */
BOOLEAN WINAPI RtlEqualString( const STRING *s1, const STRING *s2, BOOLEAN CaseInsensitive )
{
    if (s1->Length != s2->Length) return FALSE;
    return !RtlCompareString( s1, s2, CaseInsensitive );
}


/**************************************************************************
 *	RtlEqualUnicodeString   (NTDLL.@)
 */
BOOLEAN WINAPI RtlEqualUnicodeString( const UNICODE_STRING *s1, const UNICODE_STRING *s2,
                                      BOOLEAN CaseInsensitive )
{
    if (s1->Length != s2->Length) return FALSE;
    return !RtlCompareUnicodeString( s1, s2, CaseInsensitive );
}


/**************************************************************************
 *	RtlPrefixString   (NTDLL.@)
 *
 * Test if s1 is a prefix in s2
 */
BOOLEAN WINAPI RtlPrefixString( const STRING *s1, const STRING *s2, BOOLEAN ignore_case )
{
    unsigned int i;

    if (s1->Length > s2->Length) return FALSE;
    if (ignore_case)
    {
        for (i = 0; i < s1->Length; i++)
            if (toupper(s1->Buffer[i]) != toupper(s2->Buffer[i])) return FALSE;
    }
    else
    {
        for (i = 0; i < s1->Length; i++)
            if (s1->Buffer[i] != s2->Buffer[i]) return FALSE;
    }
    return TRUE;
}


/**************************************************************************
 *	RtlPrefixUnicodeString   (NTDLL.@)
 *
 * Test if s1 is a prefix in s2
 */
BOOLEAN WINAPI RtlPrefixUnicodeString( const UNICODE_STRING *s1,
                                       const UNICODE_STRING *s2,
                                       BOOLEAN ignore_case )
{
    unsigned int i;

    if (s1->Length > s2->Length) return FALSE;
    if (ignore_case)
    {
        for (i = 0; i < s1->Length / sizeof(WCHAR); i++)
            if (toupper(s1->Buffer[i]) != toupper(s2->Buffer[i])) return FALSE;
    }
    else
    {
        for (i = 0; i < s1->Length / sizeof(WCHAR); i++)
            if (s1->Buffer[i] != s2->Buffer[i]) return FALSE;
    }
    return TRUE;
}


/*
	COPY BETWEEN ANSI_STRING or UNICODE_STRING
	there is no parameter checking, it just crashes
*/


/**************************************************************************
 *	RtlAnsiStringToUnicodeString   (NTDLL.@)
 *
 * NOTES:
 *  writes terminating 0
 */
NTSTATUS WINAPI RtlAnsiStringToUnicodeString( PUNICODE_STRING uni,
                                              PCANSI_STRING ansi,
                                              BOOLEAN doalloc )
{
    DWORD total = RtlAnsiStringToUnicodeSize( ansi );

    if (total > 0xffff) return STATUS_INVALID_PARAMETER_2;
    uni->Length = total - sizeof(WCHAR);
    if (doalloc)
    {
        uni->MaximumLength = total;
        if (!(uni->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, total ))) return STATUS_NO_MEMORY;
    }
    else if (total > uni->MaximumLength) return STATUS_BUFFER_OVERFLOW;

    RtlMultiByteToUnicodeN( uni->Buffer, uni->Length, NULL, ansi->Buffer, ansi->Length );
    uni->Buffer[uni->Length / sizeof(WCHAR)] = 0;
    return STATUS_SUCCESS;
}


/**************************************************************************
 *	RtlOemStringToUnicodeString   (NTDLL.@)
 *
 * NOTES
 *  writes terminating 0
 *  if resulting length > 0xffff it returns STATUS_INVALID_PARAMETER_2
 */
NTSTATUS WINAPI RtlOemStringToUnicodeString( UNICODE_STRING *uni,
                                             const STRING *oem,
                                             BOOLEAN doalloc )
{
    DWORD total = RtlOemStringToUnicodeSize( oem );

    if (total > 0xffff) return STATUS_INVALID_PARAMETER_2;
    uni->Length = total - sizeof(WCHAR);
    if (doalloc)
    {
        uni->MaximumLength = total;
        if (!(uni->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, total ))) return STATUS_NO_MEMORY;
    }
    else if (total > uni->MaximumLength) return STATUS_BUFFER_OVERFLOW;

    RtlOemToUnicodeN( uni->Buffer, uni->Length, NULL, oem->Buffer, oem->Length );
    uni->Buffer[uni->Length / sizeof(WCHAR)] = 0;
    return STATUS_SUCCESS;
}


/**************************************************************************
 *	RtlUnicodeStringToAnsiString   (NTDLL.@)
 *
 * NOTES
 *  writes terminating 0
 *  copies a part if the buffer is too small
 */
NTSTATUS WINAPI RtlUnicodeStringToAnsiString( STRING *ansi,
                                              const UNICODE_STRING *uni,
                                              BOOLEAN doalloc )
{
    NTSTATUS ret = STATUS_SUCCESS;
    DWORD len = RtlUnicodeStringToAnsiSize( uni );

    ansi->Length = len;
    if (doalloc)
    {
        ansi->MaximumLength = len + 1;
        if (!(ansi->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, len + 1 ))) return STATUS_NO_MEMORY;
    }
    else if (ansi->MaximumLength <= len)
    {
        if (!ansi->MaximumLength) return STATUS_BUFFER_OVERFLOW;
        ansi->Length = ansi->MaximumLength - 1;
        ret = STATUS_BUFFER_OVERFLOW;
    }

    RtlUnicodeToMultiByteN( ansi->Buffer, ansi->Length, NULL, uni->Buffer, uni->Length );
    ansi->Buffer[ansi->Length] = 0;
    return ret;
}


/**************************************************************************
 *	RtlUnicodeStringToOemString   (NTDLL.@)
 *
 * NOTES
 *   allocates uni->Length+1
 *   writes terminating 0
 */
NTSTATUS WINAPI RtlUnicodeStringToOemString( STRING *oem,
                                             const UNICODE_STRING *uni,
                                             BOOLEAN doalloc )
{
    NTSTATUS ret = STATUS_SUCCESS;
    DWORD len = RtlUnicodeStringToOemSize( uni );

    oem->Length = len;
    if (doalloc)
    {
        oem->MaximumLength = len + 1;
        if (!(oem->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, len + 1 ))) return STATUS_NO_MEMORY;
    }
    else if (oem->MaximumLength <= len)
    {
        if (!oem->MaximumLength) return STATUS_BUFFER_OVERFLOW;
        oem->Length = oem->MaximumLength - 1;
        ret = STATUS_BUFFER_OVERFLOW;
    }

    RtlUnicodeToOemN( oem->Buffer, oem->Length, NULL, uni->Buffer, uni->Length );
    oem->Buffer[oem->Length] = 0;
    return ret;
}


/**************************************************************************
 *	RtlMultiByteToUnicodeN   (NTDLL.@)
 *
 * NOTES
 *  if unistr is too small a part is copied
 */
NTSTATUS WINAPI RtlMultiByteToUnicodeN( LPWSTR dst, DWORD dstlen, LPDWORD reslen,
                                        LPCSTR src, DWORD srclen )
{

    int ret = wine_cp_mbstowcs( get_ansi_table(), 0, src, srclen, dst, dstlen/sizeof(WCHAR) );
    if (reslen)
        *reslen = (ret >= 0) ? ret*sizeof(WCHAR) : dstlen; /* overflow -> we filled up to dstlen */
    return STATUS_SUCCESS;
}


/**************************************************************************
 *	RtlOemToUnicodeN   (NTDLL.@)
 */
NTSTATUS WINAPI RtlOemToUnicodeN( LPWSTR dst, DWORD dstlen, LPDWORD reslen,
                                  LPCSTR src, DWORD srclen )
{
    int ret = wine_cp_mbstowcs( get_oem_table(), 0, src, srclen, dst, dstlen/sizeof(WCHAR) );
    if (reslen)
        *reslen = (ret >= 0) ? ret*sizeof(WCHAR) : dstlen; /* overflow -> we filled up to dstlen */
    return STATUS_SUCCESS;
}


/**************************************************************************
 *	RtlUnicodeToMultiByteN   (NTDLL.@)
 */
NTSTATUS WINAPI RtlUnicodeToMultiByteN( LPSTR dst, DWORD dstlen, LPDWORD reslen,
                                        LPCWSTR src, DWORD srclen )
{
    int ret = wine_cp_wcstombs( get_ansi_table(), 0, src, srclen / sizeof(WCHAR),
                           dst, dstlen, NULL, NULL );
    if (reslen)
        *reslen = (ret >= 0) ? ret : dstlen; /* overflow -> we filled up to dstlen */
    return STATUS_SUCCESS;
}


/**************************************************************************
 *	RtlUnicodeToOemN   (NTDLL.@)
 */
NTSTATUS WINAPI RtlUnicodeToOemN( LPSTR dst, DWORD dstlen, LPDWORD reslen,
                                  LPCWSTR src, DWORD srclen )
{
    int ret = wine_cp_wcstombs( get_oem_table(), 0, src, srclen / sizeof(WCHAR),
                           dst, dstlen, NULL, NULL );
    if (reslen)
        *reslen = (ret >= 0) ? ret : dstlen; /* overflow -> we filled up to dstlen */
    return STATUS_SUCCESS;
}


/*
     CASE CONVERSIONS
*/

/**************************************************************************
 *	RtlUpperString   (NTDLL.@)
 */
void WINAPI RtlUpperString( STRING *dst, const STRING *src )
{
    unsigned int i, len = min(src->Length, dst->MaximumLength);

    for (i = 0; i < len; i++) dst->Buffer[i] = toupper(src->Buffer[i]);
    dst->Length = len;
}


/**************************************************************************
 *	RtlUpcaseUnicodeString   (NTDLL.@)
 *
 * NOTES:
 *  destination string is never 0-terminated because dest can be equal to src
 *  and src might be not 0-terminated
 *  dest.Length only set when success
 */
NTSTATUS WINAPI RtlUpcaseUnicodeString( UNICODE_STRING *dest,
                                        const UNICODE_STRING *src,
                                        BOOLEAN doalloc )
{
    DWORD i, len = src->Length;

    if (doalloc)
    {
        dest->MaximumLength = len;
        if (!(dest->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, len ))) return STATUS_NO_MEMORY;
    }
    else if (len > dest->MaximumLength) return STATUS_BUFFER_OVERFLOW;

    for (i = 0; i < len/sizeof(WCHAR); i++) dest->Buffer[i] = toupperW(src->Buffer[i]);
    dest->Length = len;
    return STATUS_SUCCESS;
}


/**************************************************************************
 *	RtlUpcaseUnicodeStringToAnsiString   (NTDLL.@)
 *
 * NOTES
 *  writes terminating 0
 */
NTSTATUS WINAPI RtlUpcaseUnicodeStringToAnsiString( STRING *dst,
                                                    const UNICODE_STRING *src,
                                                    BOOLEAN doalloc )
{
    NTSTATUS ret;
    UNICODE_STRING upcase;

    if (!(ret = RtlUpcaseUnicodeString( &upcase, src, TRUE )))
    {
        ret = RtlUnicodeStringToAnsiString( dst, &upcase, doalloc );
        RtlFreeUnicodeString( &upcase );
    }
    return ret;
}


/**************************************************************************
 *	RtlUpcaseUnicodeStringToOemString   (NTDLL.@)
 *
 * NOTES
 *  writes terminating 0
 */
NTSTATUS WINAPI RtlUpcaseUnicodeStringToOemString( STRING *dst,
                                                   const UNICODE_STRING *src,
                                                   BOOLEAN doalloc )
{
    NTSTATUS ret;
    UNICODE_STRING upcase;

    if (!(ret = RtlUpcaseUnicodeString( &upcase, src, TRUE )))
    {
        ret = RtlUnicodeStringToOemString( dst, &upcase, doalloc );
        RtlFreeUnicodeString( &upcase );
    }
    return ret;
}


/**************************************************************************
 *	RtlUpcaseUnicodeToMultiByteN   (NTDLL.@)
 */
NTSTATUS WINAPI RtlUpcaseUnicodeToMultiByteN( LPSTR dst, DWORD dstlen, LPDWORD reslen,
                                              LPCWSTR src, DWORD srclen )
{
    NTSTATUS ret;
    LPWSTR upcase;
    DWORD i;

    if (!(upcase = RtlAllocateHeap( GetProcessHeap(), 0, srclen ))) return STATUS_NO_MEMORY;
    for (i = 0; i < srclen/sizeof(WCHAR); i++) upcase[i] = toupperW(src[i]);
    ret = RtlUnicodeToMultiByteN( dst, dstlen, reslen, upcase, srclen );
    RtlFreeHeap( GetProcessHeap(), 0, upcase );
    return ret;
}


/**************************************************************************
 *	RtlUpcaseUnicodeToOemN   (NTDLL.@)
 */
NTSTATUS WINAPI RtlUpcaseUnicodeToOemN( LPSTR dst, DWORD dstlen, LPDWORD reslen,
                                        LPCWSTR src, DWORD srclen )
{
    NTSTATUS ret;
    LPWSTR upcase;
    DWORD i;

    if (!(upcase = RtlAllocateHeap( GetProcessHeap(), 0, srclen ))) return STATUS_NO_MEMORY;
    for (i = 0; i < srclen/sizeof(WCHAR); i++) upcase[i] = toupperW(src[i]);
    ret = RtlUnicodeToOemN( dst, dstlen, reslen, upcase, srclen );
    RtlFreeHeap( GetProcessHeap(), 0, upcase );
    return ret;
}


/*
	STRING SIZE
*/

/**************************************************************************
 *      RtlOemStringToUnicodeSize   (NTDLL.@)
 *      RtlxOemStringToUnicodeSize  (NTDLL.@)
 *
 * Return the size in bytes necessary for the Unicode conversion of 'str',
 * including the terminating NULL.
 */
UINT WINAPI RtlOemStringToUnicodeSize( const STRING *str )
{
    int ret = wine_cp_mbstowcs( get_oem_table(), 0, str->Buffer, str->Length, NULL, 0 );
    return (ret + 1) * sizeof(WCHAR);
}


/**************************************************************************
 *      RtlAnsiStringToUnicodeSize   (NTDLL.@)
 *      RtlxAnsiStringToUnicodeSize  (NTDLL.@)
 *
 * Return the size in bytes necessary for the Unicode conversion of 'str',
 * including the terminating NULL.
 */
DWORD WINAPI RtlAnsiStringToUnicodeSize( const STRING *str )
{
    DWORD ret;
    RtlMultiByteToUnicodeSize( &ret, str->Buffer, str->Length );
    return ret + sizeof(WCHAR);
}


/**************************************************************************
 *      RtlMultiByteToUnicodeSize   (NTDLL.@)
 *
 * Compute the size in bytes necessary for the Unicode conversion of 'str',
 * without the terminating NULL.
 */
NTSTATUS WINAPI RtlMultiByteToUnicodeSize( DWORD *size, LPCSTR str, UINT len )
{
    *size = wine_cp_mbstowcs( get_ansi_table(), 0, str, len, NULL, 0 ) * sizeof(WCHAR);
    return STATUS_SUCCESS;
}


/**************************************************************************
 *      RtlUnicodeToMultiByteSize   (NTDLL.@)
 *
 * Compute the size necessary for the multibyte conversion of 'str',
 * without the terminating NULL.
 */
NTSTATUS WINAPI RtlUnicodeToMultiByteSize( PULONG size, LPCWSTR str, ULONG len )
{
    *size = wine_cp_wcstombs( get_ansi_table(), 0, str, len / sizeof(WCHAR), NULL, 0, NULL, NULL );
    return STATUS_SUCCESS;
}


/**************************************************************************
 *      RtlUnicodeStringToAnsiSize   (NTDLL.@)
 *      RtlxUnicodeStringToAnsiSize  (NTDLL.@)
 *
 * Return the size in bytes necessary for the Ansi conversion of 'str',
 * including the terminating NULL.
 */
DWORD WINAPI RtlUnicodeStringToAnsiSize( const UNICODE_STRING *str )
{
    DWORD ret;
    RtlUnicodeToMultiByteSize( &ret, str->Buffer, str->Length );
    return ret + 1;
}


/**************************************************************************
 *      RtlUnicodeStringToOemSize   (NTDLL.@)
 *      RtlxUnicodeStringToOemSize  (NTDLL.@)
 *
 * Return the size in bytes necessary for the OEM conversion of 'str',
 * including the terminating NULL.
 */
DWORD WINAPI RtlUnicodeStringToOemSize( const UNICODE_STRING *str )
{
    return wine_cp_wcstombs( get_oem_table(), 0, str->Buffer, str->Length / sizeof(WCHAR),
                        NULL, 0, NULL, NULL ) + 1;
}


/**************************************************************************
 *      RtlAppendStringToString   (NTDLL.@)
 */
NTSTATUS WINAPI RtlAppendStringToString( STRING *dst, const STRING *src )
{
    unsigned int len = src->Length + dst->Length;
    if (len > dst->MaximumLength) return STATUS_BUFFER_TOO_SMALL;
    memcpy( dst->Buffer + dst->Length, src->Buffer, src->Length );
    dst->Length = len;
    return STATUS_SUCCESS;
}


/**************************************************************************
 *      RtlAppendAsciizToString   (NTDLL.@)
 */
NTSTATUS WINAPI RtlAppendAsciizToString( STRING *dst, LPCSTR src )
{
    if (src)
    {
        unsigned int srclen = strlen(src);
        unsigned int total  = srclen + dst->Length;
        if (total > dst->MaximumLength) return STATUS_BUFFER_TOO_SMALL;
        memcpy( dst->Buffer + dst->Length, src, srclen );
        dst->Length = total;
    }
    return STATUS_SUCCESS;
}


/**************************************************************************
 *      RtlAppendUnicodeToString   (NTDLL.@)
 */
NTSTATUS WINAPI RtlAppendUnicodeToString( UNICODE_STRING *dst, LPCWSTR src )
{
    if (src)
    {
        unsigned int srclen = strlenW(src) * sizeof(WCHAR);
        unsigned int total  = srclen + dst->Length;
        if (total > dst->MaximumLength) return STATUS_BUFFER_TOO_SMALL;
        memcpy( dst->Buffer + dst->Length/sizeof(WCHAR), src, srclen );
        dst->Length = total;
        /* append terminating NULL if enough space */
        if (total < dst->MaximumLength) dst->Buffer[total / sizeof(WCHAR)] = 0;
    }
    return STATUS_SUCCESS;
}


/**************************************************************************
 *      RtlAppendUnicodeStringToString   (NTDLL.@)
 */
NTSTATUS WINAPI RtlAppendUnicodeStringToString( UNICODE_STRING *dst, const UNICODE_STRING *src )
{
    unsigned int len = src->Length + dst->Length;
    if (len > dst->MaximumLength) return STATUS_BUFFER_TOO_SMALL;
    memcpy( dst->Buffer + dst->Length/sizeof(WCHAR), src->Buffer, src->Length );
    dst->Length = len;
    /* append terminating NULL if enough space */
    if (len < dst->MaximumLength) dst->Buffer[len / sizeof(WCHAR)] = 0;
    return STATUS_SUCCESS;
}


/*
	MISC
*/

/* this value is documented in MSDN but is not defined in any PSDK headers */
#define IS_TEXT_UNICODE_BUFFER_TOO_SMALL    0x0005

/* this value identifies the IS_TEXT_UNICODE_* bits that are currently unused.
   These bits will always be cleared from the resulting flags. */
#define IS_TEXT_UNICODE_UNUSED_MASK         0xec00

/**************************************************************************
 *	RtlIsTextUnicode (NTDLL.@)
 *
 *	Apply various feeble heuristics to guess whether
 *	the text buffer contains Unicode.
 *	FIXME: the statistical tests should be implemented
 */
DWORD WINAPI RtlIsTextUnicode(  CONST VOID *buffer,
	                            int         len,
	                            LPINT       lpi)
{
    /* assume all tests to start */
    int             flags = IS_TEXT_UNICODE_UNICODE_MASK | 
                            IS_TEXT_UNICODE_REVERSE_MASK |
                            IS_TEXT_UNICODE_NOT_UNICODE_MASK |
                            IS_TEXT_UNICODE_NOT_ASCII_MASK;
    const CHAR *    str = (const CHAR *)buffer;
    const WCHAR *   wstr = (const WCHAR *)buffer;
    int             i;
    int             n;


    /* buffer is too small to provide meaningful results */
    if (len < 2){

        /* this value should completely replace the input flags */
        if (lpi)
            *lpi = IS_TEXT_UNICODE_BUFFER_TOO_SMALL;

        return FALSE;
    }

    /* native crashes in this case.  Let's prevent that */
    if (buffer == NULL){
        ERR("native would have crashed here.  Something has gone horribly wrong\n");

        return FALSE;
    }


    /* perform only the requested tests if specified */
    if (lpi)
        flags = *lpi;


    /* removed the unused bits from the tests flag */
    flags &= ~IS_TEXT_UNICODE_UNUSED_MASK;

    /****************** quick checks *********************/
    if (flags & IS_TEXT_UNICODE_SIGNATURE){
        if (wstr[0] != 0xfeff)
            flags &= ~IS_TEXT_UNICODE_SIGNATURE;

        /* no sense in performing the odd length test if this category has already
           passed.  Native does the same thing. */
        else
            flags &= ~IS_TEXT_UNICODE_ODD_LENGTH;
    }

    if (flags & IS_TEXT_UNICODE_REVERSE_SIGNATURE){
        if (wstr[0] != 0xfffe)
            flags &= ~IS_TEXT_UNICODE_REVERSE_SIGNATURE;
    }

    if (flags & IS_TEXT_UNICODE_ODD_LENGTH){
        if (!(len & 1))
            flags &= ~IS_TEXT_UNICODE_ODD_LENGTH;
    }


    /**************** single pass checks *****************/

    /* calculate the number of unicode characters in the buffer */
    n = len / sizeof(WCHAR);

    
    /* check if there is a specific variation between low and high bytes in the buffer */
    if (flags & IS_TEXT_UNICODE_STATISTICS){

        /* FIXME: the statistical analysis method is unknown at this time.  We'll leave 
                  the implementation of this test until it is actually needed. */
        FIXME("the IS_TEXT_UNICODE_STATISTICS test is not currently supported!\n");
        flags &= ~IS_TEXT_UNICODE_STATISTICS;
    }

    /* check if there is a specific variation between low and high bytes in the buffer 
       (assuming byte reversed data). */
    if (flags & IS_TEXT_UNICODE_REVERSE_STATISTICS){

        /* FIXME: the statistical analysis method is unknown at this time.  We'll leave 
                  the implementation of this test until it is actually needed. */
        FIXME("the IS_TEXT_UNICODE_REVERSE_STATISTICS test is not currently supported!\n");
        flags &= ~IS_TEXT_UNICODE_REVERSE_STATISTICS;
    }


    /* check for wchar versions of the newline, CR, tab, space, of CJK_SPACE characters */
    if (flags & IS_TEXT_UNICODE_CONTROLS){
        int pass = 0;

        
        for (i = 0; i < n; i++){

            /* FIXME: this is also supposed to check for the CJK_SPACE character
                      but i don't know what it's value is */
            if (wstr[i] == '\n' || 
                wstr[i] == '\r' ||
                wstr[i] == '\t' ||
                wstr[i] == ' ')
            {
                pass = 1;

                break;
            }
        }


        if (!pass)
            flags &= ~IS_TEXT_UNICODE_CONTROLS;
    }

    /* check for byte reversed wchar versions of the newline, CR, tab, space, of CJK_SPACE characters */
    if (flags & IS_TEXT_UNICODE_REVERSE_CONTROLS){
        int pass = 0;

        
        for (i = 0; i < len; i += 2){

            /* this wchar can't be a byte reversed newline, space, tab, or CR => skip it */
            if (str[i])
                continue;

            /* FIXME: this is also supposed to check for the CJK_SPACE character
                      but i don't know what it's value is */
            if (str[i + 1] == '\n' || 
                str[i + 1] == '\r' ||
                str[i + 1] == '\t' ||
                str[i + 1] == ' ')
            {
                pass = 1;

                break;
            }
        }


        if (!pass)
            flags &= ~IS_TEXT_UNICODE_REVERSE_CONTROLS;
    }


    /* check for the illegal unicode characters reverse BOM, UNICODE_NUL, CRLF (packed into one WORD), or 0xFFFF. */
    if (flags & IS_TEXT_UNICODE_ILLEGAL_CHARS){
        int fail = 0;
        int start = 0;


        /* prevent a false negative from the terminating null at the end of the string */
        if (wstr[n - 1] == 0)
            n--;

        /* prevent a false negative by skipping the leading BOM if it exists */
        if (wstr[0] == 0xfeff)
            start++;


        for (i = start; i < n; i++){
            if (wstr[i] == 0xfffe || /* reverse BOM */
                wstr[i] == 0x0000 || /* UNICODE_NUL */
                wstr[i] == 0x0a0d || /* CRLF */
                wstr[i] == 0x0d0a || /* LFCR */
                wstr[i] == 0xffff)   /* illegal unicode character */
            {
                fail = 1;
                
                break;
            }
        }


        if (!fail)
            flags &= ~IS_TEXT_UNICODE_ILLEGAL_CHARS;
    }


    /* check if the buffer contains any null bytes => likely unicode */
    if (flags & IS_TEXT_UNICODE_NULL_BYTES){
        int pass = 0;
        int count = len;


        /* prevent a false negative for the IS_TEXT_UNICODE_NULL_BYTES test by 
           removing the terminating null character */
        if (str[len - 1] == 0)
            count = len - 1;


        for (i = 0; i < count; i++){
            if (str[i] == 0){
                pass = 1;

                break;
            }
        }

        /* didn't find any null characters in the buffer => could be ASCII text */
        if (!pass)
            flags &= ~IS_TEXT_UNICODE_NULL_BYTES;
    }


    /* check if the buffer consists of only zero-extended ascii characters.  No sense in
       performing this test if we already know the string is unicode (this can only prove
       the same). */
    if (!(flags & (IS_TEXT_UNICODE_REVERSE_MASK | IS_TEXT_UNICODE_NOT_UNICODE_MASK)) &&
        flags & IS_TEXT_UNICODE_ASCII16)
    {
        int fail = 0;
        int start = 0;


        if (wstr[0] == 0xfeff)
            start++;


        for (i = start; i < n; i++){
            if (wstr[i] & 0xff00){
                fail = 1;

                break;
            }
        }

        if (fail)
            flags &= ~IS_TEXT_UNICODE_ASCII16;
    }

    /* clear this test flag if either not performing the test or the buffer has already
       been determined to be unicode */
    else 
        flags &= ~IS_TEXT_UNICODE_ASCII16;

    /* check if the buffer consists of only byte reversed zero-extended ascii characters.
       No sense in performing this test if we already know the string is not unicode (this 
       can only prove the same). */
    if (!(flags & (IS_TEXT_UNICODE_NOT_ASCII_MASK | IS_TEXT_UNICODE_UNICODE_MASK)) && 
        flags & IS_TEXT_UNICODE_REVERSE_ASCII16){
        int fail = 0;
        int start = 0;


        if (wstr[0] == 0xfffe)
            start++;


        for (i = start; i < n; i++){
            if (wstr[i] & 0x00ff){
                fail = 1;

                break;
            }
        }

        if (fail)
            flags &= ~IS_TEXT_UNICODE_REVERSE_ASCII16;
    }

    /* clear this test flag if either not performing the test or the buffer has already
       been determined to be not unicode */
    else 
        flags &= ~IS_TEXT_UNICODE_REVERSE_ASCII16;



    if (lpi)
        *lpi = flags;


    /* check if some of the 'not unicode' tests passed */
    if (flags & (IS_TEXT_UNICODE_REVERSE_MASK | IS_TEXT_UNICODE_NOT_UNICODE_MASK))
        return FALSE;

    /* check if some of the 'not ascii' tests passed */
    if (flags & IS_TEXT_UNICODE_NOT_ASCII_MASK)
        return TRUE;

    /* check if some of the 'is unicode' tests passed */
    if (flags & IS_TEXT_UNICODE_UNICODE_MASK)
        return TRUE;

    return FALSE;
}
