/*
 * Profile functions
 *
 * Copyright 1993 Miguel de Icaza
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <pwd.h>
#include <unistd.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "winternl.h"
#include "wine/winbase16.h"
#include "wine/unicode.h"
#include "winreg.h"
#include "wine/file.h"
#include "wine/heapstr.h"
#include "wine/debug.h"
#include "options.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(profile);


#define WFN(filename)           debugstr_w((filename))
#define STRCOPYW(dst, src)      (dst) = HEAP_strdupAtoW(GetProcessHeap(), 0, (src))
#define STRFREEW(src)           if (src){ HeapFree(GetProcessHeap(), 0, (src)); }


typedef enum{
    ENCODING_UNKNOWN,
    ENCODING_ASCII,
    ENCODING_UTF8,
    ENCODING_UCS16LE,
    ENCODING_UCS16BE,
    ENCODING_UCS32LE,
    ENCODING_UCS32BE,
} TextEncoding;


typedef struct tagPROFILEKEY
{
    WCHAR *                 value;
    struct tagPROFILEKEY *  next;
    WCHAR                   name[1];
} PROFILEKEY;

typedef struct tagPROFILESECTION
{
    struct tagPROFILEKEY       *key;
    struct tagPROFILESECTION   *next;
    WCHAR                       name[1];
} PROFILESECTION;


typedef struct
{
    BOOL            changed;
    PROFILESECTION *section;
    WCHAR *         filename;
    FILETIME        lastWriteTime;
    TextEncoding    originalEncoding;
} PROFILE;


/* byte order markers for UTF8 and 32-bit unicode text.  IsTextUnicode()
   will take care of detecting the other UCS16 formats. */
static BYTE bom_utf8[] =    {0xef, 0xbb, 0xbf};
static BYTE bom_ucs16be[] = {0xfe, 0xff};
static BYTE bom_ucs16le[] = {0xff, 0xfe};
static BYTE bom_ucs32be[] = {0x00, 0x00, 0xfe, 0xff};
static BYTE bom_ucs32le[] = {0xff, 0xfe, 0x00, 0x00};


#define N_CACHED_PROFILES 10

/* Cached profile files */
static PROFILE *MRUProfile[N_CACHED_PROFILES]={NULL};

#define CurProfile (MRUProfile[0])

/* wine.ini config file registry root */
static HKEY wine_profile_key;

#define PROFILE_MAX_LINE_LEN   1024

/* Check for comments in profile */
#define IS_ENTRY_COMMENT(str)  ((str)[0] == ';')

static const WCHAR wininiW[] = { 'w','i','n','.','i','n','i',0 };
static const CHAR  wininiA[] = { 'w','i','n','.','i','n','i',0 };

static CRITICAL_SECTION PROFILE_CritSect;

void create_profile_cs(void)
{
  InitializeCriticalSection( &PROFILE_CritSect );
  CRITICAL_SECTION_NAME( &PROFILE_CritSect, "PROFILE_CritSect" );
}

static const char hex[16] = "0123456789ABCDEF";


/***********************************************************************
 *           PROFILE_DetectEncoding
 *
 *  attempt to detect the text encoding of the buffer <buffer>.  Returns
 *  the encoding enum that was detected.  If all tests fail, ENCODING_ASCII
 *  is assumed.  Returns the size of the encoding signature (in bytes) in
 *  <signatureLen>.
 */
static TextEncoding PROFILE_DetectEncoding(CONST LPVOID buffer, size_t len, size_t *signatureLen){
    *signatureLen = 0;


    /* the buffer is too small to be unicode => assume ascii */
    if (len < 2)
        return ENCODING_ASCII;


    /* found the UTF8 byte order marker => return UTF8 encoding */
    if (len >= 3 && !memcmp(buffer, bom_utf8, sizeof(bom_utf8))){
        *signatureLen = sizeof(bom_utf8);

        return ENCODING_UTF8;
    }

    /* found the UCS32BE byte order marker => return UCS32BE */
    else if (len >= 4 && !memcmp(buffer, bom_ucs32be, sizeof(bom_ucs32be))){
        *signatureLen = sizeof(bom_ucs32be);

        return ENCODING_UCS32BE;
    }

    /* found the UCS32LE byte order marker => return UCS32LE */
    else if (len >= 4 && !memcmp(buffer, bom_ucs32le, sizeof(bom_ucs32le))){
        *signatureLen = sizeof(bom_ucs32le);

        return ENCODING_UCS32LE;
    }

    else{

        /* start off with the quickest tests for a unicode buffer */
        int flags = IS_TEXT_UNICODE_SIGNATURE |
                    IS_TEXT_UNICODE_REVERSE_SIGNATURE |
                    IS_TEXT_UNICODE_ODD_LENGTH;


        /* the text was detected as unicode => figure out which kind */
        if (RtlIsTextUnicode(buffer, len, &flags)){

            /* found the UCS16LE signature => return UCS16LE */
            if (flags & IS_TEXT_UNICODE_SIGNATURE){
                *signatureLen = 2;

                return ENCODING_UCS16LE;
            }

            /* found the UCS16BE signature => return UCS16BE */
            else if (flags & IS_TEXT_UNICODE_REVERSE_SIGNATURE){
                *signatureLen = 2;

                return ENCODING_UCS16BE;
            }

            /* this shouldn't be hit, but we'll add it for error reporting's sake */
            else
                ERR("could not detect the text encoding type???\n");
        }
    }

    return ENCODING_ASCII;
}


/********************************* UCS conversion functions ************************************/
static const char *PROFILE_getEncodingName(TextEncoding encoding){
#define GETNAME(enc)    case enc: return #enc
    switch (encoding){
        GETNAME(ENCODING_ASCII);
        GETNAME(ENCODING_UTF8);
        GETNAME(ENCODING_UCS16LE);
        GETNAME(ENCODING_UCS16BE);
        GETNAME(ENCODING_UCS32LE);
        GETNAME(ENCODING_UCS32BE);
        GETNAME(ENCODING_UNKNOWN);
        default:
            return "<unknown_encoding>";
    }
#undef GETNAME
}

static void PROFILE_swapUCS16BE(const void *buffer, size_t len, void *output, BOOL toUnicode){
    const BYTE *buf = (const BYTE *)buffer;
    BYTE *      out = (BYTE *)output;
    size_t      i;


    /* convert UCS16BE to UCS16LE (or vice versa) */
    for (i = 0; i < len; i += sizeof(WORD)){
        out[i + 0] = buf[i + 1];
        out[i + 1] = buf[i + 0];
    }
}

static void PROFILE_swapUCS32BE(const void *buffer, size_t len, void *output, BOOL toUnicode){
    const BYTE *buf = (const BYTE *)buffer;
    BYTE *      out = (BYTE *)output;
    size_t      i;


    /* convert UCS32BE to UCS16LE */
    if (toUnicode){
        for (i = 0; i < len; i += sizeof(DWORD)){
            out[i + 0] = buf[i + 3];
            out[i + 1] = buf[i + 2];
        }
    }

    /* convert UCS16LE to UCS32BE */
    else{
        for (i = 0; i < len; i += sizeof(WORD)){
            out[i + 0] = 0x00;
            out[i + 1] = 0x00;
            out[i + 2] = buf[i + 1];
            out[i + 3] = buf[i + 0];
        }
    }
}

static void PROFILE_swapUCS32LE(const void *buffer, size_t len, void *output, BOOL toUnicode){
    size_t      i;


    /* convert UCS32LE to UCS16LE */
    if (toUnicode){
        const DWORD *   buf = (const DWORD *)buffer;
        WORD *          out = (WORD *)output;


        for (i = 0; i < len / sizeof(DWORD); i++)
            out[i] = (WORD)buf[i];
    }

    /* convert UCS16LE to UCS32LE */
    else{
        const WORD *buf = (const WORD *)buffer;
        DWORD *     out = (DWORD *)output;


        for (i = 0; i < len / sizeof(WORD); i++)
            out[i] = buf[i];
    }
}


static const WCHAR *PROFILE_convertTextToUnicode(const void *buffer, size_t len, TextEncoding fromEncoding, DWORD *outLen){
    WCHAR * output = NULL;
    size_t  dstLen;


    TRACE("converting the buffer %p from %s to unicode {len = %d bytes}\n", buffer, PROFILE_getEncodingName(fromEncoding), len);
    *outLen = 0;


    switch (fromEncoding){
        case ENCODING_ASCII:
            dstLen = MultiByteToWideChar(CP_ACP, 0, buffer, len, NULL, 0);

            /* error or nothing to convert => fail */
            if (dstLen == 0)
                return NULL;


            output = HeapAlloc(GetProcessHeap(), 0, dstLen * sizeof(WCHAR));

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", dstLen * sizeof(WCHAR));

                return NULL;
            }


            MultiByteToWideChar(CP_ACP, 0, buffer, len, output, dstLen);
            *outLen = dstLen * sizeof(WCHAR);
            break;


        case ENCODING_UTF8:
            dstLen = MultiByteToWideChar(CP_UTF8, 0, buffer, len, NULL, 0);

            /* error or nothing to convert => fail */
            if (dstLen == 0)
                return NULL;


            output = HeapAlloc(GetProcessHeap(), 0, dstLen * sizeof(WCHAR));

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", dstLen * sizeof(WCHAR));

                return NULL;
            }


            MultiByteToWideChar(CP_UTF8, 0, buffer, len, output, dstLen);
            *outLen = dstLen * sizeof(WCHAR);
            break;

        /* already unicode => just return the buffer */
        case ENCODING_UCS16LE:
            *outLen = len;

            return (const WCHAR *)buffer;

        case ENCODING_UCS16BE:
            output = HeapAlloc(GetProcessHeap(), 0, len);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", len);

                return NULL;
            }

            PROFILE_swapUCS16BE(buffer, len, output, TRUE);
            *outLen = len;
            break;

        case ENCODING_UCS32LE:
            output = HeapAlloc(GetProcessHeap(), 0, len / 2);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", len / 2);

                return NULL;
            }

            PROFILE_swapUCS32LE(buffer, len, output, TRUE);
            *outLen = len / 2;
            break;

        case ENCODING_UCS32BE:
            output = HeapAlloc(GetProcessHeap(), 0, len / 2);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", len / 2);

                return NULL;
            }

            PROFILE_swapUCS32BE(buffer, len, output, TRUE);
            *outLen = len / 2;
            break;

        case ENCODING_UNKNOWN:
        default:
            ERR("unknown encoding {fromEncoding = %s}\n", PROFILE_getEncodingName(fromEncoding));
            break;
    }

    
    return output;
}

static const void *PROFILE_convertTextFromUnicode(const WCHAR *buffer, size_t len, TextEncoding toEncoding, DWORD *outLen){
    void *  output = NULL;
    size_t  dstLen;


    TRACE("converting the buffer %p from %s to unicode {len = %d bytes}\n", buffer, PROFILE_getEncodingName(toEncoding), len);
    *outLen = 0;


    switch (toEncoding){
        case ENCODING_ASCII:
            dstLen = WideCharToMultiByte(CP_ACP, 0, buffer, len / sizeof(WCHAR), NULL, 0, NULL, NULL);

            /* error or nothing to convert => fail */
            if (dstLen == 0)
                return NULL;


            output = HeapAlloc(GetProcessHeap(), 0, dstLen);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", dstLen);

                return NULL;
            }


            WideCharToMultiByte(CP_ACP, 0, buffer, len / sizeof(WCHAR), output, dstLen, NULL, NULL);
            *outLen = dstLen;
            break;


        case ENCODING_UTF8:
            dstLen = WideCharToMultiByte(CP_UTF8, 0, buffer, len / sizeof(WCHAR), NULL, 0, NULL, NULL);

            /* error or nothing to convert => fail */
            if (dstLen == 0)
                return NULL;


            output = HeapAlloc(GetProcessHeap(), 0, dstLen);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", dstLen);

                return NULL;
            }


            WideCharToMultiByte(CP_UTF8, 0, buffer, len / sizeof(WCHAR), output, dstLen, NULL, NULL);
            *outLen = dstLen;
            break;

        /* already unicode => just return the buffer */
        case ENCODING_UCS16LE:
            *outLen = len;
            return (const void *)buffer;

        case ENCODING_UCS16BE:
            output = HeapAlloc(GetProcessHeap(), 0, len);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", len);

                return NULL;
            }

            PROFILE_swapUCS16BE(buffer, len, output, FALSE);
            *outLen = len;
            break;

        case ENCODING_UCS32LE:
            output = HeapAlloc(GetProcessHeap(), 0, len / 2);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", len / 2);

                return NULL;
            }

            PROFILE_swapUCS32LE(buffer, len, output, FALSE);
            *outLen = len / 2;
            break;

        case ENCODING_UCS32BE:
            output = HeapAlloc(GetProcessHeap(), 0, len / 2);

            if (output == NULL){
                ERR("could not allocate %d bytes for the output buffer\n", len / 2);

                return NULL;
            }

            PROFILE_swapUCS32BE(buffer, len, output, FALSE);
            *outLen = len / 2;
            break;

        case ENCODING_UNKNOWN:
        default:
            ERR("unknown encoding {fromEncoding = %s}\n", PROFILE_getEncodingName(toEncoding));
            break;
    }


    return output;
}

static void PROFILE_destroyConvertedText(const void *buffer, TextEncoding encoding){
    switch (encoding){
        case ENCODING_ASCII:
        case ENCODING_UTF8:
        case ENCODING_UCS16BE:
        case ENCODING_UCS32LE:
        case ENCODING_UCS32BE:
            if (buffer)
                HeapFree(GetProcessHeap(), 0, (void *)buffer);

            break;

        case ENCODING_UCS16LE:
        case ENCODING_UNKNOWN:
        default:
            break;
    }
}

/***********************************************************************************/

static DWORD PROFILE_getConfigDir(WCHAR *buffer, size_t maxLen){
    LPCSTR   configDir = get_config_dir();


    if (configDir == NULL)
        return 0;


    return MultiByteToWideChar(CP_ACP, 0, configDir, -1, buffer, maxLen) - 1;
}

/***********************************************************************
 *           PROFILE_CopyEntry
 *
 * Copy the content of an entry into a buffer and possibly
 * translating environment variables.
 * According to testing on winXP, we should remove quotes
 * when called from GetPrivateProfileString, but not when called from
 * GetPrivateProfileSection.
 */
static void PROFILE_CopyEntry( WCHAR *buffer, const WCHAR *value, int len,
                               int handle_env, int strip_quotes )
{
    WCHAR quote = '\0';
    const WCHAR *p;

    if(!buffer) return;

    if (strip_quotes && ((*value == '\'') || (*value == '\"')))
    {
        if (value[1] && (value[strlenW(value)-1] == *value)) quote = *value++;
    }

    if (!handle_env)
    {
        lstrcpynW( buffer, value, len );
        if (quote && (len >= strlenW(value))) buffer[strlenW(buffer)-1] = '\0';
        return;
    }

    p = value;
    while (*p && (len > 1)) {
        if ((*p == '$') && (p[1] == '{'))
        {
            WCHAR           env_val[MAX_PATH];
            const WCHAR *   p2 = strchrW( p, '}' );
            int             buffer_len;


            /* ignore it */
            if (!p2)
                continue;

            /* need to copy with enough space to hold the terminating null too! */
            lstrcpynW(env_val, p + 2, min( sizeof(env_val) / sizeof(env_val[0]), (int)(p2 - p - 1) ));

            TRACE ("Looking up %s\n", debugstr_w (env_val));
            buffer_len = GetEnvironmentVariableW(env_val, buffer, len);

            if (!buffer_len)
               ERR ("Failed to find env var %s!\n", debugstr_w (env_val));
            else
               TRACE ("Value is %s\n", debugstr_w (buffer));

            /* advance through the output buffer to the end of the string we just replaced */
            buffer += buffer_len;
            len -= buffer_len;

            /* advance through the input string to character after the replaced string */
            p = p2 + 1;

            continue;
        }
        *buffer++ = *p++;
        len--;
    }
    if (quote && (len > 1)) buffer--;
    *buffer = '\0';
}

static void PROFILE_CopyEntryA( char *buffer, const char *value, int len,
                               int handle_env, int strip_quotes )
{
    LPWSTR bufferw;
    LPWSTR valuew;


    STRCOPYW(valuew, value);
    bufferw = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));

    if (bufferw == NULL){
        ERR("could not allocate %u bytes for a scratch buffer\n", len * sizeof(WCHAR));

        if (len)
            buffer[0] = 0;

        return;
    }


    PROFILE_CopyEntry(bufferw, valuew, len, handle_env, strip_quotes);

    WideCharToMultiByte(CP_ACP, 0, bufferw, len, buffer, len, NULL, NULL);

    STRFREEW(valuew);
    STRFREEW(bufferw);
}


static BOOL PROFILE_writeBOM(HANDLE file, TextEncoding encoding){
    BOOL    result = FALSE;
    DWORD   bytesWritten;

    switch (encoding){
        case ENCODING_UTF8:
            result = WriteFile(file, bom_utf8, sizeof(bom_utf8), &bytesWritten, NULL);
            break;

        case ENCODING_UCS16LE:
            result = WriteFile(file, bom_ucs16le, sizeof(bom_ucs16le), &bytesWritten, NULL);
            break;

        case ENCODING_UCS16BE:
            result = WriteFile(file, bom_ucs16be, sizeof(bom_ucs16be), &bytesWritten, NULL);
            break;

        case ENCODING_UCS32LE:
            result = WriteFile(file, bom_ucs32le, sizeof(bom_ucs32le), &bytesWritten, NULL);
            break;

        case ENCODING_UCS32BE:
            result = WriteFile(file, bom_ucs32be, sizeof(bom_ucs32be), &bytesWritten, NULL);
            break;
        
        case ENCODING_ASCII:
        case ENCODING_UNKNOWN:
        default:
            return FALSE;
    }
    
    return result;
}

static BOOL PROFILE_writeProfileString(HANDLE file, WCHAR *buffer, size_t len, TextEncoding encoding){
    DWORD       bytesWritten;
    DWORD       writeLen;
    BOOL        result;
    const void *data;


    /* convert the text to the destination encoding */
    data = PROFILE_convertTextFromUnicode(buffer, len * sizeof(WCHAR), encoding, &writeLen);

    result = WriteFile(file, data, writeLen, &bytesWritten, NULL);
    PROFILE_destroyConvertedText(data, encoding);

    return result;
}


/***********************************************************************
 *           PROFILE_Save
 *
 *  Save a profile tree to a file.  The text is converted back to the 
 *  original encoding format identified by <encoding> before it is
 *  written to the file.
 */
static void PROFILE_Save( HANDLE file, PROFILESECTION *section, TextEncoding encoding )
{
    PROFILEKEY *    key;
    size_t          len;
    WCHAR *         buffer;
    const WCHAR     sectionPrefix[] =   {'\r', '\n', '[',  0};
    const WCHAR     sectionSuffix[] =   {']',  '\r', '\n', 0};
    const WCHAR     keySuffix[] =       {'\r', '\n', 0};
    const size_t    sectionPrefixLen =  ((sizeof(sectionPrefix) / sizeof(sectionPrefix[0])) - 1);
    const size_t    sectionSuffixLen =  ((sizeof(sectionSuffix) / sizeof(sectionSuffix[0])) - 1);
    const size_t    sectionExtraLen =   sectionPrefixLen + sectionSuffixLen;
    const size_t    keySuffixLen =      ((sizeof(keySuffix) / sizeof(keySuffix[0])) - 1);


    /* write the byte order marker to the file before anything else */
    PROFILE_writeBOM(file, encoding);


    for ( ; section; section = section->next)
    {
        if (section->name == NULL || section->name[0] == 0)
            continue;

        /* count up the amount of space that we need for the string */
        len = strlenW(section->name) + sectionExtraLen;

        /* add in the size of the key-value pairs (with extra space for '=' and the newlines) */
        for (key = section->key; key; key = key->next)
        {
            len +=  strlenW(key->name) + 
                    (key->value ? strlenW(key->value) : 0) + 
                    keySuffixLen + 
                    1;
        }


        /* allocate the buffer and fill it up */
        buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (len + 1));

        /* out of memory?!?! => bail */
        if (buffer == NULL){
            ERR("out of memory! {allocationSize = %u}\n", len);

            break;
        }


        len = 0;
        
        strcpyW(&buffer[len], sectionPrefix);
        len += sectionPrefixLen;
        
        strcpyW(&buffer[len], section->name);
        len += strlenW(section->name);

        strcpyW(&buffer[len], sectionSuffix);
        len += sectionSuffixLen;


        for (key = section->key; key; key = key->next)
        {
            strcpyW(&buffer[len], key->name);
            len += strlenW(key->name);

            if (key->value){
                buffer[len++] = '=';
                strcpyW(&buffer[len], key->value);
                len += strlenW(key->value);
            }

            strcpyW(&buffer[len], keySuffix);
            len += keySuffixLen;
        }


        /* write the section out to file in the proper encoding */
        PROFILE_writeProfileString(file, buffer, len, encoding);
        HeapFree(GetProcessHeap(), 0, buffer);
    }
}


/***********************************************************************
 *           PROFILE_Free
 *
 * Free a profile tree.
 */
static void PROFILE_Free( PROFILESECTION *section )
{
    PROFILESECTION *next_section;
    PROFILEKEY *key, *next_key;

    for ( ; section; section = next_section)
    {
        for (key = section->key; key; key = next_key)
        {
            next_key = key->next;
            if (key->value) HeapFree( GetProcessHeap(), 0, key->value );
            HeapFree( GetProcessHeap(), 0, key );
        }
        next_section = section->next;
        HeapFree( GetProcessHeap(), 0, section );
    }
}

static inline int PROFILE_isspace(WCHAR c)
{
	if (isspaceW(c)) return 1;
	if (c == '\r' || c == 0x1a) return 1;
	/* CR and ^Z (DOS EOF) are spaces too  (found on CD-ROMs) */
	return 0;
}

/***********************************************************************
 *           PROFILE_copyLine
 *
 *  copies a line of text from the string <str> to the output buffer 
 *  <buffer>.  The length of the string copied to <buffer> will not
 *  exceed <maxLen> characters.  The copy stops when either the end
 *  of the string is reached or a newline character is encountered.
 *  The newline character is not copied into the buffer.  The buffer
 *  will always be null terminated.  Returns a pointer to the start
 *  of the next line (a CRLF or LFCR counts as a single line ending)
 *  or NULL if there are no more lines in the text.
 */
static inline const WCHAR *PROFILE_copyLine(const WCHAR *str, WCHAR *buffer, size_t maxLen){
    const WCHAR *   result;
    int             i;


    /* find the end of the line and copy to the output buffer up to that point */
    for (i = 0; i < maxLen - 1 && str[i] && (str[i] != '\n' && str[i] != '\r'); i++)
        buffer[i] = str[i];

    /* make sure the buffer is terminated */
    buffer[i] = 0;


    /* save a pointer to the start of the next line */
    result = &str[i] + 1;

    /* the line ended in a CRLF => count both characters as a single line ending */
    if (str[i] == '\n' && str[i + 1] == '\r')
        result++;

    /* the line ended in a LFCR => count both characters as a single line ending */
    else if (str[i] == '\r' && str[i + 1] == '\n')
        result++;

    
    /* signal the end of the input string */
    if (str[i] == 0)
        return NULL;

    return result;
}


/***********************************************************************
 *           PROFILE_Load
 *
 * Load a profile tree from a file.
 */
static PROFILESECTION *PROFILE_Load( HANDLE file, TextEncoding *encoding )
{
    WCHAR               buffer[PROFILE_MAX_LINE_LEN];
    WCHAR *             p;
    WCHAR *             p2;
    int                 line = 0;
    BYTE *              fileData;
    LARGE_INTEGER       fileSize;
    DWORD               size;
    DWORD               sizeRead;
    const WCHAR *       ptr;
    size_t              signatureLen;
    PROFILESECTION *    section;
    PROFILESECTION *    first_section;
    PROFILESECTION **   next_section;
    PROFILEKEY *        key;
    PROFILEKEY *        prev_key;
    PROFILEKEY **       next_key;


    /* couldn't retrieve file size => fail */
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart == 0){
        ERR("could not retrieve the size of the file\n");

        return NULL;
    }

    /* file is too large to handle in memory => fail */
    if (fileSize.QuadPart > 0xffffffffllu){
        ERR("this file is enormous... skipping\n");

        return NULL;
    }


    /* create a buffer to read the entire file into.  Allocate enough 
       extra space to hold a null terminator for a UCS32 string. */
    size = fileSize.u.LowPart;
    fileData = HeapAlloc(GetProcessHeap(), 0, size + sizeof(DWORD));

    if (fileData == NULL){
        ERR("out of memory trying to allocate space for the profile file {fileSize = %llu}\n", fileSize.QuadPart);

        return NULL;
    }


    /* read the entire file into memory */
    if (!ReadFile(file, fileData, size, &sizeRead, NULL)){
        ERR("could not read the profile file into memory\n");
        HeapFree(GetProcessHeap(), 0, fileData);

        return NULL;
    }

    if (size != sizeRead)
        WARN("could not read the entire file\n");


    /* terminate the buffer for easier management.  Make sure to add 
       enough 0 characters to terminate even a UCS32 string buffer. */
    memset(&fileData[size], 0, sizeof(DWORD));

    /* try to figure out what the original text encoding type is and skip over the
       encoding signature if it exists */
    CurProfile->originalEncoding = PROFILE_DetectEncoding(fileData, size, &signatureLen);
    ptr = PROFILE_convertTextToUnicode(&fileData[signatureLen], size + sizeof(DWORD), CurProfile->originalEncoding, &size);


    first_section = HeapAlloc( GetProcessHeap(), 0, sizeof(*section) );

    if (first_section == NULL){
        ERR("could not allocate %u bytes for the first section buffer\n", sizeof(*section));

        return NULL;
    }

    first_section->name[0] = 0;
    first_section->key  = NULL;
    first_section->next = NULL;
    next_section = &first_section->next;
    next_key     = &first_section->key;
    prev_key     = NULL;



    while (ptr)
    {
        ptr = PROFILE_copyLine(ptr, buffer, PROFILE_MAX_LINE_LEN);

        line++;
        p = buffer;
        while (*p && PROFILE_isspace(*p)) p++;

        if (*p == '[')  /* section start */
        {
            if (!(p2 = strrchrW( p, ']' )))
            {
                WARN("Invalid section header at line %d: %s\n",
		     line, WFN(p) );
            }
            else
            {
                *p2 = '\0';
                p++;

                if (!(section = HeapAlloc( GetProcessHeap(), 0, sizeof(*section) + sizeof(WCHAR) * strlenW(p) ))){
                    ERR("could not allocate %u bytes for the section %s\n", sizeof(*section) + sizeof(WCHAR) * strlenW(p), debugstr_w(p));

                    break;
                }

                strcpyW( section->name, p );
                section->key  = NULL;
                section->next = NULL;
                *next_section = section;
                next_section  = &section->next;
                next_key      = &section->key;
                prev_key      = NULL;

                TRACE("New section: %s\n", WFN(section->name));

                continue;
            }
        }

        p2=p+strlenW(p) - 1;
        while ((p2 > p) && ((*p2 == '\n') || PROFILE_isspace(*p2))) *p2--='\0';

        if ((p2 = strchrW( p, '=' )) != NULL)
        {
            WCHAR *p3 = p2 - 1;
            while ((p3 > p) && PROFILE_isspace(*p3)) *p3-- = '\0';
            *p2++ = '\0';
            while (*p2 && PROFILE_isspace(*p2)) p2++;
        }

        if(*p || !prev_key || *prev_key->name)
        {
            if (!(key = HeapAlloc( GetProcessHeap(), 0, sizeof(*key) + sizeof(WCHAR) * strlenW(p) ))){
                ERR("could not allocate %u bytes for the key name %s\n", sizeof(*key) + sizeof(WCHAR) * strlenW(p), debugstr_w(p));

                break;
            }

            strcpyW( key->name, p );
            if (p2)
            {
                key->value = HeapAlloc( GetProcessHeap(), 0, sizeof(WCHAR) * (strlenW(p2) + 1) );

                if (key->value == NULL)
                    ERR("could not allocate %u bytes for the key value %s\n", sizeof(WCHAR) * (strlenW(p2) + 1), debugstr_w(p));

                else
                    strcpyW( key->value, p2 );
            }
            else key->value = NULL;

           key->next  = NULL;
           *next_key  = key;
           next_key   = &key->next;
           prev_key   = key;

           TRACE("New key: name=%s, value=%s\n",WFN(key->name), WFN(key->value));
        }
    }

    PROFILE_destroyConvertedText(ptr, CurProfile->originalEncoding);
    HeapFree(GetProcessHeap(), 0, fileData);
    return first_section;
}


/***********************************************************************
 *           PROFILE_DeleteSection
 *
 * Delete a section from a profile tree.
 */
static BOOL PROFILE_DeleteSection( PROFILESECTION **section, LPCWSTR name )
{
    while (*section)
    {
        if ((*section)->name[0] && !strcmpiW( (*section)->name, name ))
        {
            PROFILESECTION *to_del = *section;
            *section = to_del->next;
            to_del->next = NULL;
            PROFILE_Free( to_del );
            return TRUE;
        }
        section = &(*section)->next;
    }
    return FALSE;
}


/***********************************************************************
 *           PROFILE_DeleteKey
 *
 * Delete a key from a profile tree.
 */
static BOOL PROFILE_DeleteKey( PROFILESECTION **section,
			       LPCWSTR section_name, LPCWSTR key_name )
{
    while (*section)
    {
        if ((*section)->name[0] && !strcmpiW( (*section)->name, section_name ))
        {
            PROFILEKEY **key = &(*section)->key;
            while (*key)
            {
                if (!strcmpiW( (*key)->name, key_name ))
                {
                    PROFILEKEY *to_del = *key;
                    *key = to_del->next;
                    if (to_del->value) HeapFree( GetProcessHeap(), 0, to_del->value);
                    HeapFree( GetProcessHeap(), 0, to_del );
                    return TRUE;
                }
                key = &(*key)->next;
            }
        }
        section = &(*section)->next;
    }
    return FALSE;
}


/***********************************************************************
 *           PROFILE_DeleteAllKeys
 *
 * Delete all keys from a profile tree.
 */
void PROFILE_DeleteAllKeys( LPCWSTR section_name)
{
    PROFILESECTION **section= &CurProfile->section;
    while (*section)
    {
        if ((*section)->name[0] && !strcmpiW( (*section)->name, section_name ))
        {
            PROFILEKEY **key = &(*section)->key;
            while (*key)
            {
                PROFILEKEY *to_del = *key;
		*key = to_del->next;
		if (to_del->value) HeapFree( GetProcessHeap(), 0, to_del->value);
		HeapFree( GetProcessHeap(), 0, to_del );
		CurProfile->changed =TRUE;
            }
        }
        section = &(*section)->next;
    }
}


/***********************************************************************
 *           PROFILE_Find
 *
 * Find a key in a profile tree, optionally creating it.
 */
static PROFILEKEY *PROFILE_Find( PROFILESECTION **section, const WCHAR *section_name,
                                 const WCHAR *key_name, BOOL create, BOOL create_always )
{
    const WCHAR *   p;
    int             seclen;
    int             keylen;


    while (PROFILE_isspace(*section_name)) section_name++;
    p = section_name + strlenW(section_name) - 1;
    while ((p > section_name) && PROFILE_isspace(*p)) p--;
    seclen = p - section_name + 1;

    while (PROFILE_isspace(*key_name)) key_name++;
    p = key_name + strlenW(key_name) - 1;
    while ((p > key_name) && PROFILE_isspace(*p)) p--;
    keylen = p - key_name + 1;

    while (*section)
    {
        if ( ((*section)->name[0])
             && (!(strncmpiW( (*section)->name, section_name, seclen )))
             && (((*section)->name)[seclen] == '\0') )
        {
            PROFILEKEY **key = &(*section)->key;

            while (*key)
            {
                /* If create_always is FALSE then we check if the keyname already exists.
                 * Otherwise we add it regardless of its existence, to allow
                 * keys to be added more then once in some cases.
                 */
                if(!create_always)
                {
                    if ( (!(strncmpiW( (*key)->name, key_name, keylen )))
                         && (((*key)->name)[keylen] == '\0') )
                        return *key;
                }
                key = &(*key)->next;
            }
            if (!create) return NULL;

            if (!(*key = HeapAlloc( GetProcessHeap(), 0, sizeof(PROFILEKEY) + sizeof(WCHAR) * strlenW(key_name) ))){
                ERR("could not allocate %u bytes for the key name %s\n", sizeof(PROFILEKEY) + sizeof(WCHAR) * strlenW(key_name), debugstr_w(key_name));

                return NULL;
            }

            strcpyW( (*key)->name, key_name );
            (*key)->value = NULL;
            (*key)->next  = NULL;
            return *key;
        }
        section = &(*section)->next;
    }
    if (!create) return NULL;
    *section = HeapAlloc( GetProcessHeap(), 0, sizeof(PROFILESECTION) + sizeof(WCHAR) * strlenW(section_name) );

    if (*section == NULL){
        ERR("could not allocate %u bytes for the section name %s\n", sizeof(PROFILESECTION) + sizeof(WCHAR) * strlenW(section_name), debugstr_w(section_name));

        return NULL;
    }


    strcpyW( (*section)->name, section_name );
    (*section)->next = NULL;
    
    if (!((*section)->key  = HeapAlloc( GetProcessHeap(), 0,
                                        sizeof(PROFILEKEY) + sizeof(WCHAR) * strlenW(key_name) )))
    {
        ERR("could not allocate %u bytes for the key name %s\n", sizeof(PROFILEKEY) + sizeof(WCHAR) * strlenW(key_name), debugstr_w(key_name));

        HeapFree(GetProcessHeap(), 0, *section);
        return NULL;
    }


    strcpyW( (*section)->key->name, key_name );
    (*section)->key->value = NULL;
    (*section)->key->next  = NULL;
    return (*section)->key;
}


/***********************************************************************
 *           PROFILE_FlushFile
 *
 * Flush the current profile to disk if changed.
 */
static BOOL PROFILE_FlushFile(void)
{
    WCHAR *                     p;
    WCHAR                       buffer[MAX_PATHNAME_LEN] = {0};
    HANDLE                      file;
    BY_HANDLE_FILE_INFORMATION  info;


    if (CurProfile == NULL)
    {
        WARN("No current profile!\n");
        return FALSE;
    }

    /* the profile hasn't changed or isn't loaded */
    if (!CurProfile->changed || CurProfile->filename == NULL)
        return TRUE;
  

    file = CreateFileW(CurProfile->filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (file == INVALID_HANDLE_VALUE)
    {
        size_t  len;


        GetWindowsDirectoryW( buffer, MAX_PATHNAME_LEN );

        len = strlenW(buffer);
        p = buffer + len;
        *p++ = '/';
        lstrcpynW( p, strrchrW( CurProfile->filename, '\\' ) + 1, MAX_PATHNAME_LEN - len );

        file = CreateFileW(buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    }


    if (file == INVALID_HANDLE_VALUE)
    {
        WARN("could not save profile file %s\n", WFN(CurProfile->filename));
        return FALSE;
    }

    TRACE("Saving %s into %s\n", WFN(CurProfile->filename), WFN(buffer));
    PROFILE_Save( file, CurProfile->section, CurProfile->originalEncoding );
    CurProfile->changed = FALSE;
    
    FlushFileBuffers(file);

    GetFileInformationByHandle(file, &info);
    memcpy(&CurProfile->lastWriteTime, &info.ftLastWriteTime, sizeof(FILETIME));

    CloseHandle(file);

    return TRUE;
}


/***********************************************************************
 *           PROFILE_ReleaseFile
 *
 * Flush the current profile to disk and remove it from the cache.
 */
static void PROFILE_ReleaseFile(void)
{
    PROFILE_FlushFile();
    PROFILE_Free( CurProfile->section );
    if (CurProfile->filename) HeapFree( GetProcessHeap(), 0, CurProfile->filename );
    CurProfile->changed   = FALSE;
    CurProfile->section   = NULL;
    CurProfile->filename  = NULL;
    memset(&CurProfile->lastWriteTime, 0, sizeof(FILETIME));
    CurProfile->originalEncoding = ENCODING_UNKNOWN;
}


/***********************************************************************
 *           PROFILE_Open
 *
 * Open a profile file, checking the cached file first.
 */
static BOOL PROFILE_Open( LPCWSTR filename )
{
    WCHAR                       buffer[MAX_PATHNAME_LEN];
    HANDLE                      file = INVALID_HANDLE_VALUE;
    int                         i, j;
    PROFILE *                   tempProfile;
    BY_HANDLE_FILE_INFORMATION  info;


    TRACE("opening the file %s\n", WFN(filename));

    /* First time around */

    if(!CurProfile)
       for(i=0;i<N_CACHED_PROFILES;i++)
         {
          MRUProfile[i]=HeapAlloc( GetProcessHeap(), 0, sizeof(PROFILE) );
          
          if (MRUProfile[i] == NULL){
              ERR("could not allocate %u bytes for a new profile\n", sizeof(PROFILE));

              break;
          }

          MRUProfile[i]->changed=FALSE;
          MRUProfile[i]->section=NULL;
          MRUProfile[i]->filename=NULL;
          memset(&MRUProfile[i]->lastWriteTime, 0, sizeof(FILETIME));
          MRUProfile[i]->originalEncoding = ENCODING_UNKNOWN;
         }


    /* no filename specified => pick 'win.ini' by default */
    if (filename == NULL)
        filename = wininiW;


    /* Check for a match */
    if (!(  strchrW( filename, '/' ) ||
            strchrW( filename, '\\' ) ||
            strchrW( filename, ':' )  ) )
    {
        UINT len;


        len = GetWindowsDirectoryW( buffer, MAX_PATHNAME_LEN );

        if (!(len == 0 || len >= MAX_PATHNAME_LEN)){
            buffer[len] = '\\';

            lstrcpynW(&buffer[len + 1], filename, MAX_PATHNAME_LEN - len);
        }

        else
            ERR("could not retrieve the windows directory for some reason\n");
    }

    /* grab the full path name for later comparisons' purpose */
    else{
        LPWSTR dummy;


        GetFullPathNameW(filename, sizeof(buffer) / sizeof(buffer[0]), buffer, &dummy);
    }


    /* attempt to find the profile in the cache */
    for(i=0;i<N_CACHED_PROFILES;i++)
      {
       if ( MRUProfile[i]->filename && 
            !strcmpW( buffer, MRUProfile[i]->filename ) )
         {
          if(i)
            {
             PROFILE_FlushFile();
             tempProfile=MRUProfile[i];
             for(j=i;j>0;j--)
                MRUProfile[j]=MRUProfile[j-1];
             CurProfile=tempProfile;
            }

          
          return TRUE;
         }
      }

    /* Flush the old current profile */
    PROFILE_FlushFile();

    /* Make the oldest profile the current one only in order to get rid of it */
    if(i==N_CACHED_PROFILES)
      {
       tempProfile=MRUProfile[N_CACHED_PROFILES-1];
       for(i=N_CACHED_PROFILES-1;i>0;i--)
          MRUProfile[i]=MRUProfile[i-1];
       CurProfile=tempProfile;
      }
    if(CurProfile->filename) PROFILE_ReleaseFile();

    /* OK, now that CurProfile is definitely free we assign it our new file */
    CurProfile->filename  = HeapAlloc( GetProcessHeap(), 0, sizeof(WCHAR) * (strlenW(buffer) + 1) );

    if (CurProfile->filename == NULL){
        ERR("could not allocate %u bytes for the filename %s\n", sizeof(WCHAR) * (strlenW(buffer) + 1), debugstr_w(buffer));

        return FALSE;
    }

    strcpyW( CurProfile->filename, buffer );

    
    TRACE("attempting to open the file %s\n", WFN(buffer));

    /* Try to open the profile file in the specified location */
    file = CreateFileW(buffer, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (file == INVALID_HANDLE_VALUE)
    {
        size_t      bufferLen;
        const WCHAR *name;
        const WCHAR *name2;


        /* FIXME: this needs a proper fallback search path instead of just checking the prefs directory */

        /* Try to open the profile file in $HOME/.wine */
        TRACE("failed to open %s\n", WFN(buffer));

        bufferLen = PROFILE_getConfigDir(buffer, MAX_PATHNAME_LEN);
        buffer[bufferLen++] = '/';


        name = strrchrW(filename, '\\');
        name2 = strrchrW(filename, '/');

        if (name == NULL && name2 == NULL)
            name = filename;

        else if (name == NULL)
            name = name2 + 1;

        else if (name2 > name)
            name = name2 + 1;

        else
            name++;


        lstrcpynW( &buffer[bufferLen], name, MAX_PATHNAME_LEN - bufferLen);

        TRACE("attempting to open the file %s {name = %s}\n", WFN(buffer), WFN(name));

        file = CreateFileW(buffer, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    }

    
    if (file != INVALID_HANDLE_VALUE)
    {
        TRACE("(%s): found it in %s\n", WFN(filename), WFN(buffer) );

        CurProfile->section = PROFILE_Load( file, &CurProfile->originalEncoding );

        /* grab the last modification time for the file */
        GetFileInformationByHandle(file, &info);

        memcpy(&CurProfile->lastWriteTime, &info.ftLastWriteTime, sizeof(FILETIME));

        CloseHandle(file);
    }
    else
    {
        /* Does not exist yet, we will create it in PROFILE_FlushFile */
        WARN("profile file %s not found\n", WFN(buffer) );
    }
    return TRUE;
}


/***********************************************************************
 *           PROFILE_GetSection
 *
 * Returns all keys of a section.
 * If return_values is TRUE, also include the corresponding values.
 * According to testing on WinXP SP2, this should not strip quotes from 
 * the values (and prior testing indicated that this was also the case for
 * the keys).
 */
static INT PROFILE_GetSection( PROFILESECTION *section, LPCWSTR section_name,
			       LPWSTR buffer, UINT len, BOOL handle_env,
			       BOOL return_values )
{
    PROFILEKEY *key;

    if(!buffer) return 0;

    while (section)
    {
        if (section->name[0] && !strcmpiW( section->name, section_name ))
        {
            UINT oldlen = len;
            for (key = section->key; key; key = key->next)
            {
                if (len <= 2) break;
                if (!*key->name) continue;  /* Skip empty lines */
                if (IS_ENTRY_COMMENT(key->name)) continue;  /* Skip comments */
                PROFILE_CopyEntry( buffer, key->name, len - 1, handle_env, FALSE );
                len -= strlenW(buffer) + 1;
                buffer += strlenW(buffer) + 1;
                if (len < 2)
                    break;
                if (return_values && key->value) {
                    buffer[-1] = '=';
                    PROFILE_CopyEntry ( buffer,
                        key->value, len - 1, handle_env, FALSE );
                    len -= strlenW(buffer) + 1;
                    buffer += strlenW(buffer) + 1;
                }
            }
            *buffer = '\0';
            if (len <= 1)
                /*If either lpszSection or lpszKey is NULL and the supplied
                  destination buffer is too small to hold all the strings,
                  the last string is truncated and followed by two null characters.
                  In this case, the return value is equal to cchReturnBuffer
                  minus two. */
            {
                buffer[-1] = '\0';
                return oldlen - 2;
            }
            return oldlen - len;
        }
        section = section->next;
    }
    buffer[0] = buffer[1] = '\0';
    return 0;
}

/* See GetPrivateProfileSectionNamesA for documentation */
static INT PROFILE_GetSectionNames( LPWSTR buffer, UINT len )
{
    LPWSTR          buf;
    UINT            f, l;
    PROFILESECTION *section;


    if (!buffer || !len)
        return 0;

    if (len == 1) {
        *buffer = '\0';
        return 0;
    }

    f = len - 1;
    buf = buffer;
    section = CurProfile->section;
    while ((section != NULL)) {
        if (section->name[0]) {
            l = strlenW(section->name) + 1;

            if (l > f) {
                if (f > 0) {
                    lstrcpynW(buf, section->name, f - 1);
                    buf += f - 1;
                    *buf++ = '\0';
                }

                *buf = '\0';
                return len - 2;
            }

            lstrcpynW(buf, section->name, f);
            buf += l;
            f -= l;
        }

        section = section->next;
    }

    *buf = '\0';
    return buf - buffer;
}


/***********************************************************************
 *           PROFILE_GetString
 *
 * Get a profile string.
 *
 * Tests with GetPrivateProfileString16, W95a,
 * with filled buffer ("****...") and section "set1" and key_name "1" valid:
 * section	key_name	def_val		res	buffer
 * "set1"	"1"		"x"		43	[data]
 * "set1"	"1   "		"x"		43	[data]		(!)
 * "set1"	"  1  "'	"x"		43	[data]		(!)
 * "set1"	""		"x"		1	"x"
 * "set1"	""		"x   "		1	"x"		(!)
 * "set1"	""		"  x   "	3	"  x"		(!)
 * "set1"	NULL		"x"		6	"1\02\03\0\0"
 * "set1"	""		"x"		1	"x"
 * NULL		"1"		"x"		0	""		(!)
 * ""		"1"		"x"		1	"x"
 * NULL		NULL		""		0	""
 *
 * According to MSDN, GetPrivateProfileString needs to strip single and double
 * quotation marks.
 *
 */
static INT PROFILE_GetString( LPCWSTR section, LPCWSTR key_name,
			      LPCWSTR def_val, LPWSTR buffer, UINT len )
{
    PROFILEKEY *key = NULL;
    WCHAR       nullString[] = {'\0'};


    if (!buffer)
        return 0;

    if (!def_val)
        def_val = nullString;

    /* key name defined => grab just the key's value */
    if (key_name)
    {
        if (!key_name[0])
            /* Win95 returns 0 on keyname "". Tested with Likse32 bon 000227 */
            return 0;

        key = PROFILE_Find( &CurProfile->section, section, key_name, FALSE, FALSE);
        PROFILE_CopyEntry( buffer, (key && key->value) ? key->value : def_val,
                           len, FALSE, TRUE);
        TRACE("(%s, %s, %s): returning %s\n",
                         WFN(section), WFN(key_name), WFN(def_val), WFN(buffer) );
        return strlenW( buffer );
    }

    /* no key name set, but a section name was given => copy all the key names into the buffer */
    else if (section && section[0])
    {
        INT ret;


        ret = PROFILE_GetSection(CurProfile->section, section, buffer, len,
                FALSE, FALSE);

        /* couldn't enumerate key names => copy the default string into the buffer */
        if (!buffer[0]){
            PROFILE_CopyEntry(buffer, def_val, len, FALSE, TRUE);

            return strlenW(buffer);
        }
        
        return ret;
    }

    buffer[0] = '\0';
    return 0;
}


/***********************************************************************
 *           PROFILE_SetString
 *
 * Set a profile string.
 */
static BOOL PROFILE_SetString( LPCWSTR section_name, LPCWSTR key_name,
                               LPCWSTR value, BOOL create_always )
{
    if (!key_name)  /* Delete a whole section */
    {
        TRACE("(%s)\n", WFN(section_name));
        CurProfile->changed |= PROFILE_DeleteSection( &CurProfile->section,
                                                      section_name );
        if ( CurProfile->changed == TRUE )
        {
            /* Our file has been altered, so let's write it to disk...*/
            PROFILE_FlushFile();
        }
        return TRUE;         /* Even if PROFILE_DeleteSection() has failed,
                                this is not an error on application's level.*/
    }
    else if (!value)  /* Delete a key */
    {
        TRACE("(%s, %s)\n",
                         WFN(section_name), WFN(key_name) );
        CurProfile->changed |= PROFILE_DeleteKey( &CurProfile->section,
                                                  section_name, key_name );
        if ( CurProfile->changed == TRUE )
        {
            /* Our file has been altered, so let's write it to disk...*/
            PROFILE_FlushFile();
        }
        return TRUE;          /* same error handling as above */
    }
    else  /* Set the key value */
    {
        PROFILEKEY *key = PROFILE_Find(&CurProfile->section, section_name,
                                        key_name, TRUE, create_always );
        TRACE("(%s, %s, %s): \n",
                         WFN(section_name), WFN(key_name), WFN(value) );
        if (!key) return FALSE;
        if (key->value)
        {
	    /* strip the leading spaces. We can safely strip \n\r and
	     * friends too, they should not happen here anyway. */
	    while (PROFILE_isspace(*value)) value++;

            if (!strcmpW( key->value, value ))
            {
                TRACE("  no change needed\n" );
                return TRUE;  /* No change needed */
            }
            TRACE("  replacing %s\n", WFN(key->value) );
            HeapFree( GetProcessHeap(), 0, key->value );
        }
        else TRACE("  creating key\n" );
        
        
        key->value = HeapAlloc( GetProcessHeap(), 0, sizeof(WCHAR) * (strlenW(value) + 1) );

        if (key->value == NULL)
            ERR("could not allocate %u bytes for the key value %s\n", sizeof(WCHAR) * (strlenW(value) + 1), debugstr_w(value));

        else
            strcpyW( key->value, value );

        CurProfile->changed = TRUE;

        /* Our file has been altered, so let's write it to disk...*/
        PROFILE_FlushFile();
    }
    return TRUE;
}


/***********************************************************************
 *           PROFILE_GetWineIniString
 *
 * Get a config string from the wine.ini file.
 */
int PROFILE_GetWineIniString( const char *section, const char *key_name,
                              const char *def, char *buffer, int len )
{
    char    tmp[PROFILE_MAX_LINE_LEN];
    HKEY    hkey;
    DWORD   err;

    if (!(err = RegOpenKeyA( wine_profile_key, section, &hkey )))
    {
        DWORD type;
        DWORD count = sizeof(tmp);
        err = RegQueryValueExA( hkey, key_name, 0, &type, (LPBYTE)tmp,
                                &count );
        RegCloseKey( hkey );
    }

    
    PROFILE_CopyEntryA( buffer, err ? def : tmp, len, TRUE, TRUE );
    TRACE( "(%s, %s, %s): returning %s\n", debugstr_a(section), debugstr_a(key_name), debugstr_a(def), debugstr_a(buffer) );
    return strlen(buffer);
}


/***********************************************************************
 *           PROFILE_EnumWineIniString
 *
 * Get a config string from the wine.ini file.
 */
BOOL PROFILE_EnumWineIniString( const char *section, int index,
                                char *name, int name_len, char *buffer, int len )
{
    char    tmp[PROFILE_MAX_LINE_LEN];
    HKEY    hkey;
    DWORD   err, type;
    DWORD   count = sizeof(tmp);

    if (RegOpenKeyA( wine_profile_key, section, &hkey )) return FALSE;
    err = RegEnumValueA( hkey, index, name, (DWORD*)&name_len, NULL, &type,
                         (LPBYTE)tmp, &count );
    RegCloseKey( hkey );
    if (!err)
    {
        PROFILE_CopyEntryA( buffer, tmp, len, TRUE, TRUE );
        TRACE( "(%s, %d): returning %s = %s\n", debugstr_a(section), index, debugstr_a(name), debugstr_a(buffer) );
    }
    return !err;
}


/***********************************************************************
 *           PROFILE_GetWineIniInt
 *
 * Get a config integer from the wine.ini file.
 */
int PROFILE_GetWineIniInt( const char *section, const char *key_name, int def )
{
    char    buffer[20];
    char *  p;
    long    result;


    PROFILE_GetWineIniString( section, key_name, "", buffer, sizeof(buffer) );
    if (!buffer[0]) return def;
    /* FIXME: strtol wrong ?? see GetPrivateProfileIntA */
    result = strtol( buffer, &p, 0 );
    return (p == buffer) ? 0  /* No digits at all */ : (int)result;
}


/******************************************************************************
 *
 *   int  PROFILE_GetWineIniBool(
 *      char const  *section,
 *      char const  *key_name,
 *      int  def )
 *
 *   Reads a boolean value from the wine.ini file.  This function attempts to
 *   be user-friendly by accepting 'n', 'N' (no), 'f', 'F' (false), or '0'
 *   (zero) for false, 'y', 'Y' (yes), 't', 'T' (true), or '1' (one) for
 *   true.  Anything else results in the return of the default value.
 *
 *   This function uses 1 to indicate true, and 0 for false.  You can check
 *   for existence by setting def to something other than 0 or 1 and
 *   examining the return value.
 */
int  PROFILE_GetWineIniBool(
    char const  *section,
    char const  *key_name,
    int  def )
{
    char    key_value[2];
    int     retval;

    PROFILE_GetWineIniString(section, key_name, "~", key_value, 2);

    switch(key_value[0]) {
    case 'n':
    case 'N':
    case 'f':
    case 'F':
    case '0':
	retval = 0;
	break;

    case 'y':
    case 'Y':
    case 't':
    case 'T':
    case '1':
	retval = 1;
	break;

    default:
	retval = def;
    }

    TRACE("(%s, %s, %s), [%c], ret %s.\n", debugstr_a(section), debugstr_a(key_name),
		    def ? "TRUE" : "FALSE", key_value[0],
		    retval ? "TRUE" : "FALSE");

    return retval;
}


/***********************************************************************
 *           PROFILE_LoadWineIni
 *
 * Load the old .winerc file.
 */
int PROFILE_LoadWineIni(void)
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    HKEY hKeySW;
    DWORD disp;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    /* make sure HKLM\\Software\\Wine\\Wine exists as non-volatile key */
    if (!RtlCreateUnicodeStringFromAsciiz( &nameW, "Machine\\Software\\Wine\\Wine" ) ||
        NtCreateKey( &hKeySW, KEY_ALL_ACCESS, &attr, 0, NULL, 0, &disp ))
    {
        ERR("Cannot create wine registry key\n" );
        ExitProcess( 1 );
    }
    RtlFreeUnicodeString( &nameW );
    NtClose( hKeySW );

    if (!RtlCreateUnicodeStringFromAsciiz( &nameW, "Machine\\Software\\Wine\\Wine\\Config" ) ||
        NtCreateKey( &wine_profile_key, KEY_ALL_ACCESS, &attr, 0,
                     NULL, REG_OPTION_VOLATILE, &disp ))
    {
        ERR("Cannot create config registry key\n" );
        ExitProcess( 1 );
    }
    RtlFreeUnicodeString( &nameW );

    if (!CLIENT_IsBootThread()) return 1;  /* already loaded */

    if (disp == REG_OPENED_EXISTING_KEY) return 1;  /* loaded by the server */

    MESSAGE( "Can't open configuration file %s/config\n",get_config_dir() );
    return 0;
}


/***********************************************************************
 *           PROFILE_UsageWineIni
 *
 * Explain the wine.ini file to those who don't read documentation.
 * Keep below one screenful in length so that error messages above are
 * noticed.
 */
void PROFILE_UsageWineIni(void)
{
    MESSAGE("Perhaps you have not properly edited or created "
	"your Wine configuration file.\n");
    MESSAGE("This is (supposed to be) '%s/config'\n", get_config_dir());
    /* RTFM, so to say */
}


/********************* API functions **********************************/

/***********************************************************************
 *           GetProfileInt   (KERNEL.57)
 */
UINT16 WINAPI GetProfileInt16( LPCSTR section, LPCSTR entry, INT16 def_val )
{
    return GetPrivateProfileInt16( section, entry, def_val, "win.ini" );
}


/***********************************************************************
 *           GetProfileIntA   (KERNEL32.@)
 */
UINT WINAPI GetProfileIntA( LPCSTR section, LPCSTR entry, INT def_val )
{
    return GetPrivateProfileIntA( section, entry, def_val, "win.ini" );
}

/***********************************************************************
 *           GetProfileIntW   (KERNEL32.@)
 */
UINT WINAPI GetProfileIntW( LPCWSTR section, LPCWSTR entry, INT def_val )
{
    return GetPrivateProfileIntW( section, entry, def_val, wininiW );
}

/*
 * if allow_section_name_copy is TRUE, allow the copying :
 *   - of Section names if 'section' is NULL
 *   - of Keys in a Section if 'entry' is NULL
 * (see MSDN doc for GetPrivateProfileString)
 */
static int PROFILE_GetPrivateProfileString( LPCWSTR section, LPCWSTR entry,
					    LPCWSTR def_val, LPWSTR buffer,
					    UINT16 len, LPCWSTR filename,
					    BOOL allow_section_name_copy )
{
    int         ret;
    LPWSTR      pDefVal = NULL;
    const WCHAR nullString[] = {'\0'};


    if (!filename)
        filename = wininiW;


    /* strip any trailing ' ' of def_val. */
    if (def_val)
    {
        LPWSTR  p = (LPWSTR)&def_val[strlenW(def_val) - 1];


        /* find the last non-space character in the string */
        while (p >= def_val && (*p) == ' ')
            p--;

        /* correct the pointer since we went under the string by one character */
        p++;


        if (*p == ' ') /* ouch, contained trailing ' ' */
        {
            if (p == def_val)
                def_val = nullString;

            else{
                size_t len2 = p - def_val;


                pDefVal = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (len2 + 1));

                if (pDefVal){
                    lstrcpynW(pDefVal, def_val, len2);
                    pDefVal[len2] = '\0';

                    def_val = pDefVal;
                }

                else
                    ERR("could not allocate %u bytes for the default value %s\n", sizeof(WCHAR) * (len2 + 1), debugstr_w(def_val));
            }
        }
    }

    if (def_val == NULL)
        def_val = (LPWSTR)nullString;


    EnterCriticalSection( &PROFILE_CritSect );

    if (PROFILE_Open( filename )) {
        if ((allow_section_name_copy) && (section == NULL))
            ret = PROFILE_GetSectionNames(buffer, len);
        else
            /* PROFILE_GetString already handles the 'entry == NULL' case */
            ret = PROFILE_GetString( section, entry, def_val, buffer, len );
    } 
    
    else {
       lstrcpynW( buffer, def_val, len );
       ret = strlenW( buffer );
    }

    LeaveCriticalSection( &PROFILE_CritSect );


    if (pDefVal && pDefVal != def_val) /* allocated */
        HeapFree(GetProcessHeap(), 0, pDefVal);

    return ret;
}

/***********************************************************************
 *           GetPrivateProfileString   (KERNEL.128)
 */
INT16 WINAPI GetPrivateProfileString16( LPCSTR section, LPCSTR entry,
                                        LPCSTR def_val, LPSTR buffer,
                                        UINT16 len, LPCSTR filename )
{
    INT16   result;
    LPWSTR  sectionw, def_valw;
    LPWSTR  entryw, filenamew;
    LPWSTR  bufferw;


    bufferw = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));

    if (bufferw == NULL){
        ERR("could not allocate %u bytes\n", len * sizeof(WCHAR));

        return 0;
    }


    STRCOPYW(sectionw,  section);
    STRCOPYW(entryw,    entry);
    STRCOPYW(def_valw,  def_val);
    STRCOPYW(filenamew, filename);


    result = PROFILE_GetPrivateProfileString( sectionw, entryw, def_valw,
					    bufferw, len, filenamew, FALSE );

    if (result > 0 && !WideCharToMultiByte(CP_ACP, 0, bufferw, len, buffer, len, NULL, NULL))
        buffer[len - 1] = 0;

    STRFREEW(bufferw);
    STRFREEW(filenamew);
    STRFREEW(def_valw);
    STRFREEW(entryw);
    STRFREEW(sectionw);

    return result;
}

/***********************************************************************
 *           GetPrivateProfileStringA   (KERNEL32.@)
 */
INT WINAPI GetPrivateProfileStringA( LPCSTR section, LPCSTR entry,
				     LPCSTR def_val, LPSTR buffer,
				     UINT len, LPCSTR filename )
{
    INT     result;
    LPWSTR  sectionw, def_valw;
    LPWSTR  entryw, filenamew;
    LPWSTR  bufferw;


    bufferw = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));

    if (bufferw == NULL){
        ERR("could not allocate %u bytes for a scratch buffer\n", len * sizeof(WCHAR));

        return 0;
    }


    STRCOPYW(sectionw,  section);
    STRCOPYW(entryw,    entry);
    STRCOPYW(def_valw,  def_val);
    STRCOPYW(filenamew, filename);

    result = PROFILE_GetPrivateProfileString( sectionw, entryw, def_valw,
					    bufferw, len, filenamew, TRUE );

    /* these two return values are special cases for when the buffer is too small.  For these
       values we must make sure the entire buffer is double terminated. */
    if (result == len - 1 || result == len - 2)
    {
        if (!WideCharToMultiByte(CP_ACP, 0, bufferw, len, buffer, len, NULL, NULL))
        {
            if (len)
                buffer[len - 1] = 0;
            
            if (len > 1)
                buffer[len - 2] = 0;
        }
    }
    
    /* the string fit in the result buffer.  Some installers seem to pass the buffer length
       incorrectly => we'll only convert the number of characters that were returned (plus
       the terminating null). */
    else if (result)
    {
        if (!WideCharToMultiByte(CP_ACP, 0, bufferw, result + 1, buffer, len, NULL, NULL) && len)
            buffer[len - 1] = 0;
    }

    /* the key or section was found but the value returned was a null string => 
         make sure to return an empty string. */
    else if (buffer && len)
        buffer[0] = 0;


    STRFREEW(bufferw);
    STRFREEW(filenamew);
    STRFREEW(def_valw);
    STRFREEW(entryw);
    STRFREEW(sectionw);

    TRACE ("Returning value %s\n", debugstr_a (buffer));
    return result;
}

/***********************************************************************
 *           GetPrivateProfileStringW   (KERNEL32.@)
 */
INT WINAPI GetPrivateProfileStringW( LPCWSTR section, LPCWSTR entry,
				     LPCWSTR def_val, LPWSTR buffer,
				     UINT len, LPCWSTR filename )
{
    return PROFILE_GetPrivateProfileString( section, entry, def_val,
					                        buffer, len, filename, TRUE );
}

/***********************************************************************
 *           GetProfileString   (KERNEL.58)
 */
INT16 WINAPI GetProfileString16( LPCSTR section, LPCSTR entry, LPCSTR def_val,
                                 LPSTR buffer, UINT16 len )
{
    return GetPrivateProfileString16( section, entry, def_val, buffer,
                                      len, wininiA );
}

/***********************************************************************
 *           GetProfileStringA   (KERNEL32.@)
 */
INT WINAPI GetProfileStringA( LPCSTR section, LPCSTR entry, LPCSTR def_val,
			      LPSTR buffer, UINT len )
{
    return GetPrivateProfileStringA( section, entry, def_val, buffer,
                                     len, wininiA );
}

/***********************************************************************
 *           GetProfileStringW   (KERNEL32.@)
 */
INT WINAPI GetProfileStringW( LPCWSTR section, LPCWSTR entry,
			      LPCWSTR def_val, LPWSTR buffer, UINT len )
{
    return GetPrivateProfileStringW( section, entry, def_val,
				     buffer, len, wininiW );
}

/***********************************************************************
 *           WriteProfileString   (KERNEL.59)
 */
BOOL16 WINAPI WriteProfileString16( LPCSTR section, LPCSTR entry,
                                    LPCSTR string )
{
    return WritePrivateProfileString16( section, entry, string, wininiA );
}

/***********************************************************************
 *           WriteProfileStringA   (KERNEL32.@)
 */
BOOL WINAPI WriteProfileStringA( LPCSTR section, LPCSTR entry,
				 LPCSTR string )
{
    return WritePrivateProfileStringA( section, entry, string, wininiA );
}

/***********************************************************************
 *           WriteProfileStringW   (KERNEL32.@)
 */
BOOL WINAPI WriteProfileStringW( LPCWSTR section, LPCWSTR entry,
                                     LPCWSTR string )
{
    return WritePrivateProfileStringW( section, entry, string, wininiW );
}


/***********************************************************************
 *           GetPrivateProfileInt   (KERNEL.127)
 */
UINT16 WINAPI GetPrivateProfileInt16( LPCSTR section, LPCSTR entry,
                                      INT16 def_val, LPCSTR filename )
{
    /* we used to have some elaborate return value limitation (<= -32768 etc.)
     * here, but Win98SE doesn't care about this at all, so I deleted it.
     * AFAIR versions prior to Win9x had these limits, though. */
    return (INT16)GetPrivateProfileIntA(section,entry,def_val,filename);
}

/***********************************************************************
 *           GetPrivateProfileIntA   (KERNEL32.@)
 */
UINT WINAPI GetPrivateProfileIntA( LPCSTR section, LPCSTR entry,
				   INT def_val, LPCSTR filename )
{
    long    result;
    LPWSTR  sectionw, entryw;
    LPWSTR  filenamew;


    STRCOPYW(sectionw, section);
    STRCOPYW(entryw, entry);
    STRCOPYW(filenamew, filename);

    result = GetPrivateProfileIntW(sectionw, entryw, def_val, filenamew);

    STRFREEW(sectionw);
    STRFREEW(entryw);
    STRFREEW(filenamew);

    return (UINT)result;
}

/***********************************************************************
 *           GetPrivateProfileIntW   (KERNEL32.@)
 */
UINT WINAPI GetPrivateProfileIntW( LPCWSTR section, LPCWSTR entry,
				   INT def_val, LPCWSTR filename )
{
    WCHAR   buffer[20];
    long    result;
    WCHAR   nullString[] = {'\0'};


    if (!PROFILE_GetPrivateProfileString( section, entry, nullString,
                                          buffer, 
                                          sizeof(buffer) / sizeof(buffer[0]),
                                          filename, FALSE ))
        result = def_val;

    /* FIXME: if entry can be found but it's empty, then Win16 is
     * supposed to return 0 instead of def_val ! Difficult/problematic
     * to implement (every other failure also returns zero buffer),
     * thus wait until testing framework avail for making sure nothing
     * else gets broken that way. */
    else if (!buffer[0])
        result = (UINT)def_val;

    /* FIXME: Don't use strtol() here !
     * (returns LONG_MAX/MIN on overflow instead of "proper" overflow)
     YES, scan for unsigned format ! (otherwise compatibility error) */
    else 
        result = strtoulW(buffer, NULL, 0);


    TRACE ("Result: %ld\n", result);
    return (UINT)result;
}

/***********************************************************************
 *           GetPrivateProfileSection   (KERNEL.418)
 */
INT16 WINAPI GetPrivateProfileSection16( LPCSTR section, LPSTR buffer,
				        UINT16 len, LPCSTR filename )
{
    return GetPrivateProfileSectionA( section, buffer, len, filename );
}

/***********************************************************************
 *           GetPrivateProfileSectionA   (KERNEL32.@)
 */
INT WINAPI GetPrivateProfileSectionA( LPCSTR section, LPSTR buffer,
				      DWORD len, LPCSTR filename )
{
    int		ret = 0;
    LPWSTR  sectionw, filenamew;
    LPWSTR  bufferw;


    bufferw = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));

    if (bufferw == NULL){
        ERR("could not allocate %ld bytes for a scratch buffer\n", len * sizeof(WCHAR));

        return 0;
    }


    STRCOPYW(sectionw, section);
    STRCOPYW(filenamew, filename);


    ret = GetPrivateProfileSectionW(sectionw, bufferw, len, filenamew);

    if (ret > 0 && !WideCharToMultiByte(CP_ACP, 0, bufferw, len, buffer, len, NULL, NULL))
        buffer[len - 1] = 0;

    STRFREEW(bufferw);
    STRFREEW(filenamew);
    STRFREEW(sectionw);

    return ret;
}

/***********************************************************************
 *           GetPrivateProfileSectionW   (KERNEL32.@)
 */

INT WINAPI GetPrivateProfileSectionW (LPCWSTR section, LPWSTR buffer,
				      DWORD len, LPCWSTR filename )

{
    int		ret = 0;
 

    EnterCriticalSection( &PROFILE_CritSect );

    if (PROFILE_Open( filename )){
        ret = PROFILE_GetSection(CurProfile->section, section, buffer, len,
				 FALSE, TRUE);
    }

    LeaveCriticalSection( &PROFILE_CritSect );


    return ret;
}

/***********************************************************************
 *           GetProfileSection   (KERNEL.419)
 */
INT16 WINAPI GetProfileSection16( LPCSTR section, LPSTR buffer, UINT16 len )
{
    return GetPrivateProfileSection16( section, buffer, len, wininiA );
}

/***********************************************************************
 *           GetProfileSectionA   (KERNEL32.@)
 */
INT WINAPI GetProfileSectionA( LPCSTR section, LPSTR buffer, DWORD len )
{
    return GetPrivateProfileSectionA( section, buffer, len, wininiA );
}

/***********************************************************************
 *           GetProfileSectionW   (KERNEL32.@)
 */
INT WINAPI GetProfileSectionW( LPCWSTR section, LPWSTR buffer, DWORD len )
{
    return GetPrivateProfileSectionW( section, buffer, len, wininiW );
}


/***********************************************************************
 *           WritePrivateProfileString   (KERNEL.129)
 */
BOOL16 WINAPI WritePrivateProfileString16( LPCSTR section, LPCSTR entry,
                                           LPCSTR string, LPCSTR filename )
{
    return WritePrivateProfileStringA(section,entry,string,filename);
}

/***********************************************************************
 *           WritePrivateProfileStringA   (KERNEL32.@)
 */
BOOL WINAPI WritePrivateProfileStringA( LPCSTR section, LPCSTR entry,
					LPCSTR string, LPCSTR filename )
{
    BOOL    ret = FALSE;
    LPWSTR  sectionw, entryw;
    LPWSTR  stringw, filenamew;


    EnterCriticalSection( &PROFILE_CritSect );

    STRCOPYW(sectionw, section);
    STRCOPYW(entryw, entry);
    STRCOPYW(stringw, string);
    STRCOPYW(filenamew, filename);


    ret = WritePrivateProfileStringW( sectionw, entryw,
                                      stringw, filenamew );

    STRFREEW(sectionw);
    STRFREEW(entryw);
    STRFREEW(stringw);
    STRFREEW(filenamew);

    LeaveCriticalSection( &PROFILE_CritSect );
    return ret;
}

/***********************************************************************
 *           WritePrivateProfileStringW   (KERNEL32.@)
 */
BOOL WINAPI WritePrivateProfileStringW( LPCWSTR section, LPCWSTR entry,
					LPCWSTR string, LPCWSTR filename )
{
    BOOL ret = FALSE;


    if (PROFILE_Open( filename ))
    {
        if (!section && !entry && !string) /* documented "file flush" case */
            PROFILE_ReleaseFile();  /* always return FALSE in this case */
        else {
            if (!section) {
                FIXME("(NULL?, %s, %s, %s)? \n", WFN(entry), WFN(string), WFN(filename));
            } else {
                ret = PROFILE_SetString( section, entry, string, FALSE);
            }
        }
    }


    return ret;
}

/***********************************************************************
 *           WritePrivateProfileSection   (KERNEL.416)
 */
BOOL16 WINAPI WritePrivateProfileSection16( LPCSTR section,
				 	    LPCSTR string, LPCSTR filename )
{
    return WritePrivateProfileSectionA( section, string, filename );
}

/***********************************************************************
 *           WritePrivateProfileSectionA   (KERNEL32.@)
 */
BOOL WINAPI WritePrivateProfileSectionA( LPCSTR section,
					 LPCSTR string, LPCSTR filename )
{
    BOOL    ret = FALSE;
    LPWSTR  sectionw, filenamew;
    LPWSTR  stringw;


    STRCOPYW(sectionw, section);
    STRCOPYW(stringw, string);
    STRCOPYW(filenamew, filename);

    ret = WritePrivateProfileSectionW(  sectionw,
                                        stringw, 
                                        filenamew);

    STRFREEW(filenamew);
    STRFREEW(stringw);
    STRFREEW(sectionw);

    return ret;
}

/***********************************************************************
 *           WritePrivateProfileSectionW   (KERNEL32.@)
 */
BOOL WINAPI WritePrivateProfileSectionW( LPCWSTR section,
					 LPCWSTR string, LPCWSTR filename)
{
    BOOL    ret = FALSE;
    LPWSTR  p;


    EnterCriticalSection( &PROFILE_CritSect );


    if (PROFILE_Open( filename )) {
        if (!section && !string)
            PROFILE_ReleaseFile();  /* always return FALSE in this case */

        else if (!string) /* delete the named section*/
            ret = PROFILE_SetString(section, NULL, NULL, FALSE);

        else {
            PROFILE_DeleteAllKeys(section);
            ret = TRUE;

            while(*string) {
                size_t  keyLen = strlenW(string);
                LPWSTR  buf = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * (keyLen + 1));


                if (buf == NULL){
                    ERR("could not allocate %u bytes for a scratch buffer\n", sizeof(WCHAR) * (keyLen + 1));

                    break;
                }

                strcpyW(buf, string);

                if ((p = strchrW( buf, '='))){
                    *p = '\0';
                    ret = PROFILE_SetString( section, buf, p + 1, TRUE);
                }

                HeapFree(GetProcessHeap(), 0, buf);
                string += strlenW(string) + 1;
            }
        }
    }


    LeaveCriticalSection( &PROFILE_CritSect );

    return ret;
}

/***********************************************************************
 *           WriteProfileSection   (KERNEL.417)
 */
BOOL16 WINAPI WriteProfileSection16( LPCSTR section, LPCSTR keys_n_values)
{
    return WritePrivateProfileSection16( section, keys_n_values, wininiA);
}

/***********************************************************************
 *           WriteProfileSectionA   (KERNEL32.@)
 */
BOOL WINAPI WriteProfileSectionA( LPCSTR section, LPCSTR keys_n_values)

{
    return WritePrivateProfileSectionA( section, keys_n_values, wininiA);
}

/***********************************************************************
 *           WriteProfileSectionW   (KERNEL32.@)
 */
BOOL WINAPI WriteProfileSectionW( LPCWSTR section, LPCWSTR keys_n_values)
{
   return (WritePrivateProfileSectionW (section,keys_n_values, wininiW));
}

/***********************************************************************
 *           GetPrivateProfileSectionNames   (KERNEL.143)
 */
WORD WINAPI GetPrivateProfileSectionNames16( LPSTR buffer, WORD size,
                                             LPCSTR filename )
{
    return GetPrivateProfileSectionNamesA(buffer,size,filename);
}


/***********************************************************************
 *           GetProfileSectionNames   (KERNEL.142)
 */
WORD WINAPI GetProfileSectionNames16(LPSTR buffer, WORD size)

{
    return GetPrivateProfileSectionNamesA(buffer,size,wininiA);
}


/***********************************************************************
 *           GetPrivateProfileSectionNamesA  (KERNEL32.@)
 *
 * Returns the section names contained in the specified file.
 * FIXME: Where do we find this file when the path is relative?
 * The section names are returned as a list of strings with an extra
 * '\0' to mark the end of the list. Except for that the behavior
 * depends on the Windows version.
 *
 * Win95:
 * - if the buffer is 0 or 1 character long then it is as if it was of
 *   infinite length.
 * - otherwise, if the buffer is to small only the section names that fit
 *   are returned.
 * - note that this means if the buffer was to small to return even just
 *   the first section name then a single '\0' will be returned.
 * - the return value is the number of characters written in the buffer,
 *   except if the buffer was too smal in which case len-2 is returned
 *
 * Win2000:
 * - if the buffer is 0, 1 or 2 characters long then it is filled with
 *   '\0' and the return value is 0
 * - otherwise if the buffer is too small then the first section name that
 *   does not fit is truncated so that the string list can be terminated
 *   correctly (double '\0')
 * - the return value is the number of characters written in the buffer
 *   except for the trailing '\0'. If the buffer is too small, then the
 *   return value is len-2
 * - Win2000 has a bug that triggers when the section names and the
 *   trailing '\0' fit exactly in the buffer. In that case the trailing
 *   '\0' is missing.
 *
 * Wine implements the observed Win2000 behavior (except for the bug).
 *
 * Note that when the buffer is big enough then the return value may be any
 * value between 1 and len-1 (or len in Win95), including len-2.
 */
DWORD WINAPI GetPrivateProfileSectionNamesA( LPSTR buffer, DWORD size,
					     LPCSTR filename)

{
    DWORD   ret = 0;
    LPWSTR  bufferw, filenamew;


    bufferw = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));

    if (bufferw == NULL){
        ERR("could not allocate %ld bytes for a scratch buffer\n", size * sizeof(WCHAR));

        return 0;
    }


    STRCOPYW(filenamew, filename);

    ret = GetPrivateProfileSectionNamesW(bufferw, size, filenamew);

    /* make sure to convert the entire buffer back to ANSI text */
    WideCharToMultiByte(CP_ACP, 0, bufferw, size, buffer, size, NULL, NULL);

    STRFREEW(filenamew);
    STRFREEW(bufferw);

    return ret;
}


/***********************************************************************
 *           GetPrivateProfileSectionNamesW  (KERNEL32.@)
 */
DWORD WINAPI GetPrivateProfileSectionNamesW( LPWSTR buffer, DWORD size,
					     LPCWSTR filename)
{
    DWORD   ret = 0;


    EnterCriticalSection( &PROFILE_CritSect );

    if (PROFILE_Open( filename ))
        ret = PROFILE_GetSectionNames(buffer, size);

    LeaveCriticalSection( &PROFILE_CritSect );

    return ret;
}

/***********************************************************************
 *           GetPrivateProfileStruct (KERNEL.407)
 */
BOOL16 WINAPI GetPrivateProfileStruct16(LPCSTR section, LPCSTR key,
 				        LPVOID buf, UINT16 len, LPCSTR filename)
{
    return GetPrivateProfileStructA( section, key, buf, len, filename );
}

/***********************************************************************
 *           GetPrivateProfileStructA (KERNEL32.@)
 *
 * Should match Win95's behaviour pretty much
 */
BOOL WINAPI GetPrivateProfileStructA (LPCSTR section, LPCSTR key,
				      LPVOID buf, UINT len, LPCSTR filename)
{
    BOOL    result;
    LPWSTR  sectionw;
    LPWSTR  keyw;
    LPWSTR  filenamew;

    
    STRCOPYW(sectionw, section);
    STRCOPYW(keyw, key);
    STRCOPYW(filenamew, filename);

    result = GetPrivateProfileStructW( sectionw, keyw, buf, len, filenamew );
    
    STRFREEW(filenamew);
    STRFREEW(keyw);
    STRFREEW(sectionw);

    return result;
}

/***********************************************************************
 *           GetPrivateProfileStructW (KERNEL32.@)
 *
 *  NOTE: i can't get either version of this function to work under
 *        windowsXP, so i have no idea what it is supposed to return.
 *        However, because of the rather odd restriction on the buffer
 *        size in here (dunno what it could possibly be for???), this
 *        function is very likely to always fail as well.
 */
BOOL WINAPI GetPrivateProfileStructW (LPCWSTR section, LPCWSTR key,
				      LPVOID buffer, UINT len, LPCWSTR filename)
{
    BOOL	ret = FALSE;

    EnterCriticalSection( &PROFILE_CritSect );

    if (PROFILE_Open( filename )) {
        PROFILEKEY *k = PROFILE_Find ( &CurProfile->section, section, key, FALSE, FALSE);
    
        if (k && k->value)
        {
            TRACE("value (at %p): %s\n", WFN(k->value), WFN(k->value));
            if (((strlenW(k->value) - 2) / 2) == len)
            {
                LPWSTR  end, p;
                BOOL    valid = TRUE;
                WCHAR   c;
                DWORD   chksum = 0;

                end = k->value + strlenW(k->value); /* -> '\0' */

                /* check for invalid chars in ASCII coded hex string */
                for (p=k->value; p < end; p++)
                {
                    if (!isxdigitW(*p))
                    {
                        WARN("invalid char '%c' in file %s->[%s]->%s !\n",
                                     *p, WFN(filename), WFN(section), WFN(key));
                        valid = FALSE;
                        break;
                    }
                }

                if (valid)
                {
                    BOOL highnibble = TRUE;
                    BYTE b = 0, val;
                    LPBYTE binbuf = (LPBYTE)buffer;

                    end -= 2; /* don't include checksum in output data */
                              /* translate ASCII hex format into binary data */
                    for (p=k->value; p < end; p++)
                    {
                        c = toupperW(*p);
                        val = (c > '9') ?
                              (c - 'A' + 10) : (c - '0');

                        if (highnibble)
                            b = val << 4;
                        else
                        {
                            b += val;
                            *binbuf++ = b; /* feed binary data into output */
                            chksum += b; /* calculate checksum */
                        }
                        highnibble ^= 1; /* toggle */
                    }

                    /* retrieve stored checksum value */
                    c = toupperW(*p++);
                    b = ( (c > '9') ? (c - 'A' + 10) : (c - '0') ) << 4;
                    c = toupperW(*p);
                    b +=  (c > '9') ? (c - 'A' + 10) : (c - '0');
                    if (b == (chksum & 0xff)) /* checksums match ? */
                        ret = TRUE;
                }
            }
        }
    }

    LeaveCriticalSection( &PROFILE_CritSect );

    return ret;
}



/***********************************************************************
 *           WritePrivateProfileStruct (KERNEL.406)
 */
BOOL16 WINAPI WritePrivateProfileStruct16 (LPCSTR section, LPCSTR key,
	LPVOID buf, UINT16 bufsize, LPCSTR filename)
{
    return WritePrivateProfileStructA( section, key, buf, bufsize, filename );
}

/***********************************************************************
 *           WritePrivateProfileStructA (KERNEL32.@)
 */
BOOL WINAPI WritePrivateProfileStructA (LPCSTR section, LPCSTR key,
                                        LPVOID buf, UINT bufsize, LPCSTR filename)
{
    BOOL    result;
    LPWSTR  sectionw;
    LPWSTR  keyw;
    LPWSTR  filenamew;


    STRCOPYW(sectionw, section);
    STRCOPYW(keyw, key);
    STRCOPYW(filenamew, filename);

    result = WritePrivateProfileStructW( sectionw, keyw, buf, bufsize, filenamew );

    STRFREEW(sectionw);
    STRFREEW(keyw);
    STRFREEW(filenamew);

    return result;
}

/***********************************************************************
 *           WritePrivateProfileStructW (KERNEL32.@)
 */
BOOL WINAPI WritePrivateProfileStructW (LPCWSTR section, LPCWSTR key,
					LPVOID buf, UINT bufsize, LPCWSTR filename)
{
    BOOL    ret = FALSE;
    LPBYTE  binbuf;
    LPWSTR  outstring, p;
    DWORD   sum = 0;


    if (!section && !key && !buf)  /* flush the cache */
        return WritePrivateProfileStringW( NULL, NULL, NULL, filename );

    /* allocate string buffer for hex chars + checksum hex char + '\0' */
    outstring = HeapAlloc( GetProcessHeap(), 0, sizeof(WCHAR) * (bufsize * 2 + 2 + 1));

    /* couldn't allocate memory for the checksum string (!?!?) */
    if (outstring == NULL){
        ERR("could not allocate %u bytes for the checksum buffer\n", sizeof(WCHAR) * (bufsize * 2 + 2 + 1));

        return FALSE;
    }


    p = outstring;
    for (binbuf = (LPBYTE)buf; binbuf < (LPBYTE)buf + bufsize; binbuf++) {
      *p++ = hex[*binbuf >> 4];
      *p++ = hex[*binbuf & 0xf];
      sum += *binbuf;
    }
    /* checksum is sum & 0xff */
    *p++ = hex[(sum & 0xf0) >> 4];
    *p++ = hex[sum & 0xf];
    *p++ = '\0';

    EnterCriticalSection( &PROFILE_CritSect );

    if (PROFILE_Open( filename ))
        ret = PROFILE_SetString( section, key, outstring, FALSE);

    LeaveCriticalSection( &PROFILE_CritSect );

    HeapFree( GetProcessHeap(), 0, outstring );

    return ret;
}


/***********************************************************************
 *           WriteOutProfiles   (KERNEL.315)
 */
void WINAPI WriteOutProfiles16(void)
{
    EnterCriticalSection( &PROFILE_CritSect );
    PROFILE_FlushFile();
    LeaveCriticalSection( &PROFILE_CritSect );
}

/***********************************************************************
 *           CloseProfileUserMapping   (KERNEL32.@)
 */
BOOL WINAPI CloseProfileUserMapping(void) {
    FIXME("(), stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

#undef STRCOPYW
#undef STRFREEW
