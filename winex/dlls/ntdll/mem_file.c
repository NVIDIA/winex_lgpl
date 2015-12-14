/*
 * Mem_file.c - ram-based read-only files
 *
 * Copyright 2007 TransGaming Technologies
 *
 *
 *  This component adds the following config options to the [wine] section:
 *
 *      'UseMemFiles' = {Y/N, 1/0, T/F}
 *          This option enables or disables the memory file system.  If the
 *          value is n/N/0/F/f, no files will be cached when open for read.
 *
 *      'MemFileCacheLimit' = <n> [{KB, MB, GB}]
 *          This option sets the maximum amount of memory to be used for 
 *          caching files.  This memory is only tracked for the files buffers
 *          themselves, and not for individual file handles.  This can be set
 *          in bytes, kilobytes, megabytes, or gigabytes.  This value will be
 *          clamped to 10% of total physical system memory at most, and to the
 *          max cached file size (below) limit at least.
 *
 *      'MemFileMaxSize' = <n> [{KB, MB, GB}]
 *          This option sets the maximum file size that can be cached.  Attempts
 *          to cache files larger than this size will fail and just result in 
 *          the normal file access methods being used.  This can be set in bytes,
 *          kilobytes, megabytes, or gigabytes.  This value will be clamped to 
 *          10% of total physical system memory at most.
 *          
 *      'MemFileDeniedExtensions' = <list>
 *          This option provides a list of file extensions that should not be
 *          cached.  Each extension in the list is separated by a comma, semicolon, 
 *          dot, space, or tab.  Files with these extensions will immediately
 *          return failure from MEMFILE_CreateFile(), and normal file access 
 *          methods will be used on them.
 *      
 */

#include "config.h"
#include "wine/port.h"
#include "windef.h"
#include "winbase.h"
#include "wine/mem_file.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "wine/winbase16.h"
#include "winreg.h"
#include "wine/unicode.h"


WINE_DEFAULT_DEBUG_CHANNEL (memfile);

/* for now, take 10% of total system RAM as the max size for the cache limit.
   This should really go in as a config option expressed in percent */
#define CACHE_SIZE_CAP(ram)     ((ram) / 10)
#define SIZEOF(x)               (sizeof(x) / sizeof((x)[0]))
#define DEFAULT_DENIED_EXT      "exe;dylib;com;dll;so;reg;sys"


typedef struct MemFileData_t MemFileData_t;
typedef struct FileBuffer_t  FileBuffer_t;

struct MemFileData_t {
    MemFileData_t * Next;
    MemFileData_t * Prev;
    HANDLE          Handle;
    LARGE_INTEGER   CurPos;
    
    FileBuffer_t *  Buffer;
};


struct FileBuffer_t{
    int             refCount;
    char *          filename;
    LARGE_INTEGER   fileSize;
    BYTE *          buffer;
};


/******************** Memory File Cache State **********************/
typedef struct{
    CRITICAL_SECTION    cs;
    BOOL                initialized;
    MemFileData_t *     list;
    MemFileData_t *     tail;
    int                 count;
    LARGE_INTEGER       totalCacheSize;
    LARGE_INTEGER       cacheLimit;
    LARGE_INTEGER       maxSize;
    BOOL                disabled;
    int                 extensionCount;
    char **             deniedList;
} MemFileState;

static MemFileState g_memFile = {
        {NULL},         /* critical section */
        FALSE,          /* initialized? */
        NULL,           /* list head */
        NULL,           /* list tail */
        0,              /* open file count */
        {{0}},          /* total current size of the cache */
        {{104857600}},  /* cache size limit (100MB) */
        {{10485760}},   /* max size of each cached file (10MB) */
        FALSE,          /* disabled? */
        0,              /* number of denied extensions */
        NULL            /* denied extensions list */
};

/*******************************************************************/


static VOID MEMFILE_DeleteMemFileByData(MemFileData_t *pData);
static void MEMFILE_DoPointerSync(MemFileData_t *pData);


/*************** Config Options & Registry Functions ***************/
#define IS_OPTION_TRUE(ch) \
    ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')



static DWORD getSizeScale(LPCWSTR endp){
    
    /* nothing left in the string => succeed */
    if (endp == NULL || endp[0] == 0)
        return 1;


    /* skip any whitespace in the middle of the string */
    while (endp[0] && (endp[0] == ' ' || endp[0] == '\t'))
        endp++;

    /* check for a KB, MB, or GB scale string and return the appropriate scale value */
    if (endp[1] == 'B' || endp[1] == 'b'){
        switch (endp[0]){
            case 'K':
            case 'k':
                return 1024;

            case 'M':
            case 'm':
                return 1048576;

            case 'G':
            case 'g':
                return 1048576 * 1024;
        }
    }

    return 1;
}


static char *addDefaultDeniedList(const char *denied, const char *separators){
    char        buf[sizeof(DEFAULT_DENIED_EXT) + 1] = DEFAULT_DENIED_EXT;
    char        addedList[sizeof(DEFAULT_DENIED_EXT) + 1] = {0};
    char *      tok;
    char *      endp;
    const char *ext;
    size_t      len = strlen(denied);
    size_t      origLen;
    size_t      tmpLen;
    size_t      pos = 0;
    const char *original;



    tok = strtok_r(buf, separators, &endp);

    while (tok){
        original = denied;
        origLen = len;

        while (origLen > 0 && original){
            ext = strstr(original, tok);
            tmpLen = strlen(tok);

            /* extension was found in the original list => make sure it isn't just a substring */
            if (ext){
                int sep = 0;


                if (ext == denied || strchr(separators, ext[-1]))
                    sep++;

                if (ext[tmpLen] == 0 || strchr(separators, ext[tmpLen]))
                    sep++;

                
                /* the extension is in the list as a properly separated substring => stop searching */
                if (sep == 2)
                    original = NULL;

                /* the extension is in the list, but only as a substring of a larger name => keep searching */
                else{
                    original += tmpLen;
                    origLen -= tmpLen;
                }
            }

            /* the extension was not in the list => add it */
            else{
                memcpy(&addedList[pos], tok, tmpLen);
                addedList[pos + tmpLen] = ';';
                pos += tmpLen + 1;

                original = NULL;
            }
        }


        tok = strtok_r(NULL, separators, &endp);
    }


    if (pos){
        addedList[pos] = 0;

        tok = (char *)RtlAllocateHeap(GetProcessHeap(), 0, len + pos + 1 + 1);

        if (tok == NULL){
            ERR("could not allocate %ld bytes to add some default extensions to the denied list\n", len + pos + 1 + 1);

            return (char *)denied;
        }


        memcpy(tok, addedList, pos);
        memcpy(&tok[pos], denied, len);
        tok[len + pos] = 0;

        return tok;
    }

    else
        return (char *)denied;
}

static void destroyDefaultDeniedList(char *denied, const char *original){
    if (denied != original)
        RtlFreeHeap(GetProcessHeap(), 0, denied);
}

static void processDeniedList(const char *_denied){
    const char *separators = " \t,;|.";
    char *      denied;
    char *      buf;
    char *      tmp;
    char *      tok;
    char *      endp;
    size_t      len;
    size_t      totalLen = 0;
    int         i;


    denied = addDefaultDeniedList(_denied, separators);

    len = strlen(denied);
    buf = (char *)RtlAllocateHeap(GetProcessHeap(), 0, len + 1);

    /* could not allocate memory to duplicate the denied extensions string => fail */
    if (buf == NULL){
        ERR("could not allocate %ld bytes for the denied extensions list\n", len + 1);
        destroyDefaultDeniedList(denied, _denied);

        g_memFile.extensionCount = 0;
        g_memFile.deniedList = NULL;

        return;
    }


    memcpy(buf, denied, len + 1);


    /* count up the number of extensions that were specified */
    g_memFile.extensionCount = 0;

    tok = strtok_r(buf, separators, &endp);

    while (tok){
        g_memFile.extensionCount++;

        totalLen += strlen(tok) + 1;

        tok = strtok_r(NULL, separators, &endp);
    }


    g_memFile.deniedList = (char **)RtlAllocateHeap(GetProcessHeap(), 0, g_memFile.extensionCount * sizeof(char *) + totalLen * sizeof(char));

    /* could not allocate enough memory to store the denied extensions list => fail */
    if (g_memFile.deniedList == NULL){
        ERR("not enough memory to store the denied extensions list {len = %lu}\n", g_memFile.extensionCount * sizeof(char *) + totalLen * sizeof(char));
        g_memFile.extensionCount = 0;

        RtlFreeHeap(GetProcessHeap(), 0, buf);
        destroyDefaultDeniedList(denied, _denied);

        return;
    }


    /* copy the list over again and parse it this time */
    memcpy(buf, denied, len + 1);
    tmp = ((char *)g_memFile.deniedList) + (g_memFile.extensionCount * sizeof(char *));
    i = 0;


    tok = strtok_r(buf, separators, &endp);

    while (tok){
        len = strlen(tok);
        memcpy(tmp, tok, len + 1);

        g_memFile.deniedList[i] = tmp;
        tmp += len + 1;
        i++;
     
        tok = strtok_r(NULL, separators, &endp);
    }


    RtlFreeHeap(GetProcessHeap(), 0, buf);
    destroyDefaultDeniedList(denied, _denied);
}


/* Nt_regCreateKeyExA(): a slightly modified version of the real implementation of RegCreateKeyExA().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
static NTSTATUS Nt_regCreateKeyExA(HKEY hkey, LPCWSTR name, DWORD options, REGSAM access, LPHKEY retkey){
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

    return status ;
}

/* Nt_regCloseKey(): a slightly modified version of the real implementation of RegCloseKey().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
static NTSTATUS Nt_regCloseKey(HKEY hkey){
    if (!hkey || hkey >= 0x80000000)
        return ERROR_SUCCESS;

    return NtClose( hkey );
}

/* Nt_regQueryValueExA(): a slightly modified version of the real implementation of RegQueryValueExA().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
static NTSTATUS Nt_regQueryValueExA(HKEY hkey, LPCWSTR name, LPBYTE data, DWORD *count){
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
static BOOL getConfigKey(HKEY defkey, LPCWSTR name, WCHAR *buffer, DWORD size){
    return Nt_regQueryValueExA( defkey, name, (LPBYTE)buffer, &size ) == STATUS_SUCCESS;
}


/* setupOptions(): setup memory file cache specific options */
static void setupOptions(void){
	WCHAR           buffer[MAX_PATH + 16];
    HKEY            hkey;
    MEMORYSTATUSEX  memoryStatus;
    WCHAR           configKey[] = { 'S', 'o', 'f', 't', 'w', 'a', 'r', 'e', '\\', 
                                    'W', 'i', 'n', 'e', '\\', 
                                    'W', 'i', 'n', 'e', '\\', 
                                    'C', 'o', 'n', 'f', 'i', 'g', '\\', 
                                    'w', 'i', 'n', 'e', 0};
    WCHAR           useMemFilesKey[] = {'U', 's', 'e', 'M', 'e', 'm', 'F', 'i', 'l', 'e', 's', 0};
    WCHAR           memFileCacheLimitKey[] = {'M', 'e', 'm', 'F', 'i', 'l', 'e', 'C', 'a', 'c', 'h', 'e', 'L', 'i', 'm', 'i', 't', 0};
    WCHAR           memFileMaxSizeKey[] = {'M', 'e', 'm', 'F', 'i', 'l', 'e', 'M', 'a', 'x', 'S', 'i', 'z', 'e', 0};
    WCHAR           memFileDeniedExtensionsKey[] = {'M', 'e', 'm', 'F', 'i', 'l', 'e', 'D', 'e', 'n', 'i', 'e', 'd', 'E', 'x', 't', 'e', 'n', 's', 'i', 'o', 'n', 's', 0};


    if (Nt_regCreateKeyExA(HKEY_LOCAL_MACHINE, configKey, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, &hkey) != STATUS_SUCCESS)
    {
        ERR("Cannot create config registry key\n");

        return;
    }


    /* check if we should disable the memory files */
    if (getConfigKey(hkey, useMemFilesKey, buffer, sizeof(buffer)))
        g_memFile.disabled = !IS_OPTION_TRUE(buffer[0]);


    /* get the maximum total size for the cache.  This isn't a hard limit, more of a suggestion */
    if (getConfigKey(hkey, memFileCacheLimitKey, buffer, sizeof(buffer))){
        LARGE_INTEGER   limit = {{0}};
        WCHAR *         endp = NULL;


        /* make sure the buffer is terminated */
        buffer[SIZEOF(buffer) - 1] = 0;

        limit.QuadPart = (LONGLONG)strtolW(buffer, &endp, 10);

        /* make sure the number converted properly */
        if (limit.QuadPart >= 0){

            /* check for any scale strings such as 'KB', 'MB', or 'GB' */
            limit.QuadPart *= getSizeScale(endp);

            g_memFile.cacheLimit.QuadPart = limit.QuadPart;
        }
    }


    /* get the maximum size of a file to cache */
    if (getConfigKey(hkey, memFileMaxSizeKey, buffer, sizeof(buffer))){
        LARGE_INTEGER   limit = {{0}};
        WCHAR *         endp = NULL;


        /* make sure the buffer is terminated */
        buffer[SIZEOF(buffer) - 1] = 0;

        limit.QuadPart = (LONGLONG)strtolW(buffer, &endp, 10);

        /* make sure the number converted properly */
        if (limit.QuadPart >= 0){

            /* check for any scale strings such as 'KB', 'MB', or 'GB' */
            limit.QuadPart *= getSizeScale(endp);

            g_memFile.maxSize.QuadPart = limit.QuadPart;
        }
    }


    /* get the list of file extensions that should not be cached */
    if (getConfigKey(hkey, memFileDeniedExtensionsKey, buffer, sizeof(buffer))){
        char    tmp[MAX_PATH + 16];


        /* make sure the source buffer is terminated and just convert the string */
        buffer[SIZEOF(buffer) - 1] = 0;

        /* we always get the filenames as ANSI strings so we'll keep the denied extensions
           list as ANSI strings.  Convert the string once here and process the list */
        if (WideCharToMultiByte( CP_ACP, 0, buffer, -1, tmp, sizeof(tmp), NULL, NULL ))
            processDeniedList(tmp);
    }

    else
        processDeniedList(DEFAULT_DENIED_EXT);


    /* retrieve the amount of physical RAM in the system to clamp the cache sizes to a reasonable amount */
    if (GlobalMemoryStatusEx(&memoryStatus)){

        /* make sure the max cached file size is a reasonable size given the amount of RAM on the system */
        if (g_memFile.maxSize.QuadPart > CACHE_SIZE_CAP(memoryStatus.ullTotalPhys)){
            WARN("the max cached file size is quite large compared to the amount of available RAM.  Resizing to %lld {maxSize = %lld, availableRAM = %llu}\n",
                    CACHE_SIZE_CAP(memoryStatus.ullTotalPhys),
                    g_memFile.maxSize.QuadPart,
                    memoryStatus.ullTotalPhys);

            g_memFile.maxSize.QuadPart = CACHE_SIZE_CAP(memoryStatus.ullTotalPhys);
        }

        /* make sure the cache limit is a reasonable size given the amount of RAM on the system */
        if (g_memFile.cacheLimit.QuadPart > CACHE_SIZE_CAP(memoryStatus.ullTotalPhys)){
            WARN("the total cache limit is quite large compared to the amount of available RAM.  Resizing to %lld {cacheLimit = %lld, availableRAM = %llu}\n",
                    CACHE_SIZE_CAP(memoryStatus.ullTotalPhys),
                    g_memFile.cacheLimit.QuadPart,
                    memoryStatus.ullTotalPhys);

            g_memFile.cacheLimit.QuadPart = CACHE_SIZE_CAP(memoryStatus.ullTotalPhys);
        }
    }


    /* make sure the cache limit is at least as large as the max cached file size */
    if (g_memFile.maxSize.QuadPart > g_memFile.cacheLimit.QuadPart){
        WARN("the maximum cached file size is larger than the total cache limit {cacheLimit = %lld, maxSize = %lld}\n",
                g_memFile.cacheLimit.QuadPart,
                g_memFile.maxSize.QuadPart);

        g_memFile.cacheLimit.QuadPart = g_memFile.maxSize.QuadPart;
    }



    /* trace out the current values of all the option variables */
    TRACE("MemFile options:\n");
    TRACE("    MemFileDisabled =        %s\n", g_memFile.disabled ? "TRUE" : "FALSE");
    TRACE("    MemFileCacheLimit =      %lld\n", g_memFile.cacheLimit.QuadPart);
    TRACE("    MemFileMaxSize =         %lld\n", g_memFile.maxSize.QuadPart);

    /* trace out the denied extension list if used */
    if (g_memFile.extensionCount){
        int i;


        TRACE("    MemFileExtensionCount =  %d\n", g_memFile.extensionCount);
        TRACE("    MemFileDeniedList:\n");

        for (i = 0; i < g_memFile.extensionCount; i++)
            TRACE("        %2d) %s\n", i, g_memFile.deniedList[i]);
    }
    
    
    Nt_regCloseKey( hkey );
}
/***********************************************************/


void MEMFILE_Initialize()
{
    if (g_memFile.initialized)
       return;

    InitializeCriticalSection (&g_memFile.cs);
    g_memFile.initialized = TRUE;
    g_memFile.totalCacheSize.QuadPart = 0;
    setupOptions();
}


/***************** List Management Functions **********************/

/* AddMemFile(): adds a single memory file object to the start of the file list.  
     Returns the head of the list. */
static MemFileData_t *AddMemFile(MemFileData_t *pData){
    pData->Prev = NULL;
    pData->Next = g_memFile.list;
    
    if (g_memFile.list)
        g_memFile.list->Prev = pData;

    g_memFile.list = pData;


    if (g_memFile.tail == NULL)
        g_memFile.tail = pData;

    g_memFile.count++;


    return g_memFile.list;
}


/* RemoveMemFile(): removes the memory file <pData> from the file list.  Returns
     the new head of the list. */
static MemFileData_t *RemoveMemFile(MemFileData_t *pData){
    if (pData == NULL)
        return NULL;


    if (pData->Prev)
        pData->Prev->Next = pData->Next;

    else
        g_memFile.list = pData->Next;


    if (pData->Next)
        pData->Next->Prev = pData->Prev;

    else
        g_memFile.tail = pData->Prev;

    g_memFile.count--;


    return g_memFile.list;
}


/* -----------------------------------------------------------------
 * Retrieve the memory file for the given handle if it exists
 * Must be called while <g_memFile.cs> is held!
 */
static MemFileData_t *GetMemFile (HANDLE hFile, BOOL RemoveFromList){
    MemFileData_t *pData = g_memFile.list;


    while (pData){
        if (pData->Handle == hFile){
            if (RemoveFromList)
                RemoveMemFile(pData);

            return pData;
        }

        pData = pData->Next;
    }


    return NULL;
}

/* -----------------------------------------------------------------
 * Retrieve the first memory file for the given filename if it exists
 * Must be called while <g_memFile.cs> is held!
 */
static MemFileData_t *GetMemFileByName(const char *filename, BOOL RemoveFromList){
    MemFileData_t *pData = g_memFile.list;


    while (pData){
        if (!strcasecmp(filename, pData->Buffer->filename)){
            if (RemoveFromList)
                RemoveMemFile(pData);

            return pData;
        }

        pData = pData->Next;
    }


    return NULL;
}


/* ClipTail(): removes the last memory file object from the list.  The object
     is not destroyed.  Returns the memory file object that was removed. */
static MemFileData_t *ClipTail(){
    MemFileData_t *pData = g_memFile.tail;


    /* unlink the last item in the list */
    if (pData->Prev)
        pData->Prev->Next = NULL;

    /* only one item in the list => clear the list */
    else
        g_memFile.list = NULL;

    g_memFile.tail = pData->Prev;
    g_memFile.count--;


    return pData;
}


/* MoveToFront(): moves the memory file <file> to the front of the file list.
     Returns the new head of the list. */
static MemFileData_t *MoveToFront(MemFileData_t *file){

    /* list is empty => just add the new item */
    if (g_memFile.list == NULL)
        return AddMemFile(file);


    /* item is already the first in the list or is the only one in the list => succeed */
    if (g_memFile.list == g_memFile.tail || file == g_memFile.list)
        return g_memFile.list;


    /* remove the item from the list and relink the list around it */
    if (file->Prev)
        file->Prev->Next = file->Next;

    if (file->Next)
        file->Next->Prev = file->Prev;

    else
        g_memFile.tail = file->Prev;


    /* add the item to the front of the list */
    file->Prev = NULL;
    file->Next = g_memFile.list;

    if (g_memFile.list)
        g_memFile.list->Prev = file;

    g_memFile.list = file;


    return g_memFile.list;
}

/******************************************************************************/

/************************ Write Notification Functions ************************/

/* MEMFILE_ClearCache(): performs the actual work of clearing the cache for a named file. */
static BOOL MEMFILE_ClearCache(const char *filename){
    MemFileData_t * pData = GetMemFileByName(filename, TRUE);
    DWORD           error;
    BOOL            result;


    if (pData == NULL){
        TRACE("i don't have the file '%s' open for read right now\n", filename);

        return FALSE;
    }


    /* save and clear the current error value (this operation shouldn't
       affect the current error code) */
    error = GetLastError();
    SetLastError(0);


    EnterCriticalSection(&g_memFile.cs);

    /* Multiple threads could have the same file open for read simultaneously => find 
         and close all files with the same name */
    do{
        TRACE("destroying the file cache for '%s' {handle = %u}\n", filename, pData->Handle);

        /* resync the real file pointer to the current position of the memory file */
        MEMFILE_DoPointerSync(pData);

        /* clean up the buffers for the memory file */
        MEMFILE_DeleteMemFileByData(pData);

        /* keep searching in case another thread also has the same file open for read */
        pData = GetMemFileByName(filename, TRUE);
    } while (pData);

    LeaveCriticalSection(&g_memFile.cs);


    /* check the results and restore the previous error code */
    result = GetLastError() == 0;
    SetLastError(error);

    return result;
}


/* __wine_clearFileCache(): write notification dispatch function.  This function is called
     from ntdll when the user signal for the 'clear file cache' event is received.  This
     dispatches the event to the main handler then signals the calling process that the
     job is done. */
void __wine_clearFileCache(void *data)
{
    const char *filename = (const char *)data;


    TRACE("received interprocess memfile clear cache request for '%s'\n", filename);
    MEMFILE_ClearCache(filename);
}


/* MEMFILE_isExtensionDenied(): check the filename <filename> to see if its extension
     is on the denied extensions list.  Returns TRUE if the extension is on the list.
     Returns FALSE if the file has no extension, the extension is not on the list, or
     there are no denied extensions. */
static BOOL MEMFILE_isExtensionDenied(const char *filename){
    const char *ext;


    /* no denied extensions => succeed */
    if (g_memFile.extensionCount == 0)
        return FALSE;


    ext = strrchr(filename, '.');

    if (ext){
        int i;


        /* don't include the '.' in the extension name */
        ext++;

        for (i = 0; i < g_memFile.extensionCount; i++){
            if (!strcasecmp(ext, g_memFile.deniedList[i]))
                return TRUE;
        }
    }

    /* no extension on the filename or extension allowed => succeed */
    return FALSE;
}

static BOOL MEMFILE_isFileIgnored(const char *filename){
    if (!strncasecmp(filename, "/dev/", 5))
        return TRUE;

    return FALSE;
}



/************************* API Functions ****************************/

/* MEMFILE_allocBuffer(): creates a new file buffer to hold the contents of a file.
     The total allocated size of the data buffer will be <fileSize>.  The buffer's
     reference count will start at 1.  Returns NULL if the buffer could not be
     allocated.  Returns a pointer to the buffer on success. */
static FileBuffer_t *MEMFILE_allocBuffer(const char *filename, LARGE_INTEGER fileSize){
    FileBuffer_t *  buf;
    size_t          len = strlen(filename);


    buf = (FileBuffer_t *)RtlAllocateHeap(GetProcessHeap(), 0, sizeof(FileBuffer_t) + fileSize.s.LowPart + (len + 1));

    /* couldn't allocate enough memory for the entire file buffer => fail */
    if (buf == NULL){
        ERR("could not allocate %ld bytes to store the file data for '%s'\n", 
                sizeof(FileBuffer_t) + fileSize.s.LowPart + (len + 1),
                filename);

        return NULL;
    }



    buf->filename = (char *)(((BYTE *)buf) + sizeof(FileBuffer_t));
    buf->buffer = (BYTE *)(buf->filename + (len + 1));

    buf->refCount = 1;
    buf->fileSize.QuadPart = fileSize.QuadPart;
    memcpy(buf->filename, filename, len + 1);


    /* update the total cache size */
    EnterCriticalSection(&g_memFile.cs);
    g_memFile.totalCacheSize.QuadPart += fileSize.QuadPart;
    LeaveCriticalSection(&g_memFile.cs);

    return buf;
}


/* MEMFILE_releaseBuffer(): releases a reference to the file buffer <buf>.  The buffer
     will be destroyed if nothing else has a reference to it.  Returns FALSE if the 
     file buffer <buf> is NULL or another cached file object still holds a reference
     to it.  Returns TRUE if the file buffer was destroyed.
     
     This must be called while <g_memFile.cs> is held! */
static BOOL MEMFILE_releaseBuffer(FileBuffer_t *buf){
    if (buf == NULL)
        return FALSE;


    buf->refCount--;
    TRACE("decremented ref count to %d for '%s'\n", buf->refCount, buf->filename);

    /* no other files hold a reference to the file buffer => destroy it */
    if (buf->refCount <= 0){
        TRACE("file buffer is unreferenced.  Destroying... {fileSize = %lld}\n", buf->fileSize.QuadPart);
        g_memFile.totalCacheSize.QuadPart -= buf->fileSize.QuadPart;
     
        RtlFreeHeap(GetProcessHeap(), 0, buf);

        return TRUE;
    }

    return FALSE;
}


/* MEMFILE_createBuffer(): searches the cached file list to check if the file with the
     name <filename> has already been cached.  If it is already cached, a pointer to its
     buffer object is returned and the buffer's reference count is incremented.  If not
     found, a new file buffer is allocated with enough space to store <fileSize> bytes.
     Returns NULL if the buffer was not found and not enough memory could be allocated. */
static FileBuffer_t *MEMFILE_createBuffer(const char *filename, LARGE_INTEGER fileSize, BOOL *created){
    MemFileData_t *file = GetMemFileByName(filename, FALSE);


    if (file == NULL){
        if (created)
            *created = TRUE;

        return MEMFILE_allocBuffer(filename, fileSize);
    }


    file->Buffer->refCount++;
    TRACE("incremented the ref count to %d for '%s'\n", file->Buffer->refCount, file->Buffer->filename);

    /* issue a warning if the file size has changed (this shouldn't happen) */
    if (file->Buffer->fileSize.QuadPart != fileSize.QuadPart){
        WARN("someone change the file '%s' on me!!! {currentSize = %lld, oldSize = %lld}\n",
                filename,
                fileSize.QuadPart,
                file->Buffer->fileSize.QuadPart);
    }


    if (created)
        *created = FALSE;
    
    return file->Buffer;
}


/* --------------------------------------------------------------
 * Create a new memory file associated with the given handle,
 * and read in its contents
 */
BOOL MEMFILE_CreateMemFile(const char *filename, HANDLE hFile)
{
    MemFileData_t * pData;
    DWORD           BytesRead = 0;
    LARGE_INTEGER   fileSize;
    BOOL            newBufferCreated;


    MEMFILE_Initialize ();

    /* the memory files stuff is disabled => ignore this operation */
    if (g_memFile.disabled)
        return FALSE;


    /* the file should be ignored by the memory files => fail */
    if (MEMFILE_isFileIgnored(filename)){
        TRACE("the file '%s' will be ignored\n", filename);

        return FALSE;
    }

    /* the file's extension is on the denied list => fail */
    if (MEMFILE_isExtensionDenied(filename)){
        TRACE("the file '%s' has an extension that is on the denied list\n", filename);

        return FALSE;
    }


    /* grab the file's size so we can allocate a buffer later */
    if (!GetFileSizeEx(hFile, &fileSize)){
        ERR("Unable to get file size for memory file! {filename = '%s', handle = %d}\n", filename, hFile);

        return FALSE;
    }

    /* don't cache empty files, it would be a waste => fail */
    if (fileSize.QuadPart == 0){
        TRACE("this file is empty.  Skipping memory file creation {filename = '%s', handle = %d}\n", filename, hFile);

        return FALSE;
    }


    pData = RtlAllocateHeap (GetProcessHeap (), HEAP_ZERO_MEMORY, sizeof (*pData));

    if (!pData){
        ERR("could not allocate %ld bytes to store the memory file data for '%s'\n", sizeof (*pData), filename);

        return FALSE;
    }


    pData->Handle = hFile;


    /* the file is larger than the recommended maximum size for caching => fail */
    if (fileSize.QuadPart > g_memFile.maxSize.QuadPart){
        WARN("the file '%s' is too large to cache {hFile = %u, fileSize = %lld bytes}\n",
              filename,
              hFile,
              fileSize.QuadPart);

        RtlFreeHeap(GetProcessHeap (), 0, pData);
        return FALSE;
    }


    EnterCriticalSection (&g_memFile.cs);

    pData->Buffer = MEMFILE_createBuffer(filename, fileSize, &newBufferCreated);

    if (pData->Buffer == NULL){
        ERR("Unable to allocate memory for memory file buffer! {size = %lld bytes}\n", fileSize.QuadPart);

        RtlFreeHeap(GetProcessHeap (), 0, pData);
        return FALSE;
    }


    if (newBufferCreated){
        TRACE("a new buffer was created to hold '%s' {fileSize = %lld}\n", filename, fileSize.QuadPart);

        if (!ReadFile(hFile, pData->Buffer->buffer, pData->Buffer->fileSize.u.LowPart,
                       &BytesRead, NULL) ||
            (BytesRead != pData->Buffer->fileSize.u.LowPart))
        {
            ERR("Error reading memory file into buffer!\n");


            MEMFILE_releaseBuffer(pData->Buffer);
            RtlFreeHeap(GetProcessHeap (), 0, pData);

            return FALSE;
        }
    }

    else{
        TRACE("the file '%s' is already open for read.  Sharing its file buffer {fileSize = %lld}\n",
                filename,
                fileSize.QuadPart);
    }

    /* We need to reset actual file pointer to start of file so that the
       load_registry wineserver call can read the file directly */
    SetFilePointer (hFile, 0, NULL, FILE_BEGIN);
    
    /* add the new cached file to the file list */
    AddMemFile(pData);


    /* the cache is getting too large => kill the cached data for the least recently accessed file */
    if (g_memFile.tail && 
        g_memFile.list != g_memFile.tail && 
        g_memFile.totalCacheSize.QuadPart > g_memFile.cacheLimit.QuadPart)
    {
        MemFileData_t *file = ClipTail();


        TRACE("cache is filling up!!  Destroying the cache for:\n");

        /* keep clearing the cache for the last file in the list until the total cache size
           drops below the limit.  This should leave at least one memory file in the cache
           (the file that was just opened).  With appropriately set cache and file size
           limit values, this should always work correctly. */
        do{
            TRACE("    %lld bytes, (%04u) '%s'\n", file->Buffer->fileSize.QuadPart, file->Handle, file->Buffer->filename);

            MEMFILE_DoPointerSync(file);
            MEMFILE_DeleteMemFileByData(file);

            file = ClipTail();
        } while (file && g_memFile.totalCacheSize.QuadPart > g_memFile.cacheLimit.QuadPart);
    }

    LeaveCriticalSection (&g_memFile.cs);

    
    TRACE("Created memory file of size %lld for handle %u {totalCacheSize = %lld, MemFileCount = %d, filename = '%s'}\n",
           pData->Buffer->fileSize.QuadPart, hFile, g_memFile.totalCacheSize.QuadPart, g_memFile.count, filename);

    return TRUE;
}


/* --------------------------------------------------------------
 *      MEMFILE_doSeek()
 *
 *  Do the actual work of moving the memory file read pointer.
 *  NOTE: <g_memFile.cs> must be held before calling this function!
 */
static void MEMFILE_doSeek(MemFileData_t *pData, LARGE_INTEGER liDistance, PLARGE_INTEGER pNewPos, DWORD dwMethod){
    LARGE_INTEGER NewPos;


    if (dwMethod == FILE_BEGIN)
        NewPos.QuadPart = liDistance.QuadPart;

    else if (dwMethod == FILE_CURRENT)
        NewPos.QuadPart = pData->CurPos.QuadPart + liDistance.QuadPart;

    else
        NewPos.QuadPart = pData->Buffer->fileSize.QuadPart + liDistance.QuadPart;


    if (NewPos.QuadPart < 0)
        SetLastError (ERROR_NEGATIVE_SEEK);

    else
    {
        pData->CurPos.QuadPart = NewPos.QuadPart;
        if (pNewPos)
            pNewPos->QuadPart = NewPos.QuadPart;
    }


    TRACE ("Updated file pointer to %lld for handle %u\n", pData->CurPos.QuadPart, pData->Handle);
}

/* --------------------------------------------------------------
 * Move mem file pointer to a new position, and return updated
 * position */
BOOL MEMFILE_SetFilePointerEx (HANDLE hFile, LARGE_INTEGER liDistance,
                               PLARGE_INTEGER pNewPos, DWORD dwMethod)
{
    MemFileData_t * pData;


    /* Optimization for common case - if no memory files, then no need
       to grab the cs */
    if (g_memFile.list == NULL || g_memFile.disabled)
        return FALSE;

    
    EnterCriticalSection (&g_memFile.cs);
    pData = GetMemFile (hFile, FALSE);
    
    if (!pData)
    {
        LeaveCriticalSection (&g_memFile.cs);
        return FALSE;
    }


    /* perform the actual seek operation */
    MEMFILE_doSeek(pData, liDistance, pNewPos, dwMethod);

    LeaveCriticalSection (&g_memFile.cs);
    return TRUE;
}


/* --------------------------------------------------------------
 * Sync real data pointer with the cached one if necessary
 * Note that the <g_memFile.cs> must be held while calling this!
 */
static void MEMFILE_DoPointerSync(MemFileData_t *pData)
{
    SERVER_START_REQ( set_file_pointer )
    {
        req->handle = pData->Handle;
        req->low = pData->CurPos.u.LowPart;
        req->high = pData->CurPos.u.HighPart;
        req->whence = FILE_BEGIN;
        
        if (wine_server_call_err (req))
            ERR ("Failure to sync read pointer of handle %u to %lld!\n",
                 pData->Handle, pData->CurPos.QuadPart);

        else
        {
            if ((pData->CurPos.u.LowPart != reply->new_low) ||
                (pData->CurPos.u.HighPart != reply->new_high))
            {
                ERR ("New read pointer for handle %u mismatch! expected: (%ld,%lu) actual: (%d,%u)\n",
                     pData->Handle, pData->CurPos.u.HighPart,
                     pData->CurPos.u.LowPart, reply->new_high, reply->new_low);
            }

            pData->CurPos.u.LowPart = reply->new_low;
            pData->CurPos.u.HighPart = reply->new_high;
        }
    }
    SERVER_END_REQ;
}


/* --------------------------------------------------------------
 *              MEMFILE_doRead()
 *  Performs the actual work of reading from the memory file.
 *  NOTE: <g_memFile.cs> must be held before calling this function!
 */
static void MEMFILE_doRead(MemFileData_t *pData, LPVOID Buffer, DWORD BytesToRead, LPDWORD BytesRead)
{
    DWORD maxSize;


    /* Determine how much data we can copy */
    maxSize = pData->Buffer->fileSize.QuadPart - pData->CurPos.QuadPart;
    if (maxSize < BytesToRead)
        BytesToRead = maxSize;


    /* Copy data */
    memcpy (Buffer, pData->Buffer->buffer + pData->CurPos.QuadPart, BytesToRead);
    pData->CurPos.QuadPart += BytesToRead;

    if (BytesRead)
        *BytesRead = BytesToRead;
    
    
    /* move this file to the front of the list so the next access will 
       be faster and we'll have an ordering for an MRU list */
    MoveToFront(pData);
}

/* --------------------------------------------------------------
 * Read data from memory file
 */
BOOL MEMFILE_ReadFile (HANDLE hFile, LPVOID Buffer, DWORD BytesToRead,
                       LPDWORD BytesRead)
{
    MemFileData_t * pData;


    /* Optimization for common case - if no memory files, then no need
       to grab the cs */
    if (g_memFile.list == NULL || g_memFile.disabled)
        return FALSE;

    EnterCriticalSection (&g_memFile.cs);
    pData = GetMemFile (hFile, FALSE);

    if (!pData)
    {
        LeaveCriticalSection (&g_memFile.cs);
        return FALSE;
    }


    MEMFILE_doRead(pData, Buffer, BytesToRead, BytesRead);

    LeaveCriticalSection (&g_memFile.cs);
    return TRUE;
}

/* --------------------------------------------------------------
 *          MEMFILE_ReadFileFromOffset()
 *
 *  Read data from memory file starting from a specified offset.
 *  The offset value <offset> is assumed to be a valid seek 
 *  location in the file.
 */
BOOL MEMFILE_ReadFileFromOffset (HANDLE hFile, LPVOID Buffer, DWORD BytesToRead,
                                        LPDWORD BytesRead, LARGE_INTEGER offset)
{
    MemFileData_t * pData;


    /* Optimization for common case - if no memory files, then no need
       to grab the cs */
    if (g_memFile.list == NULL || g_memFile.disabled)
        return FALSE;

    EnterCriticalSection (&g_memFile.cs);
    pData = GetMemFile (hFile, FALSE);

    if (!pData)
    {
        LeaveCriticalSection (&g_memFile.cs);
        return FALSE;
    }


    /* seek to the requested location in the file */
    MEMFILE_doSeek(pData, offset, NULL, FILE_BEGIN);
    
    /* perform the read starting at the requested location */
    MEMFILE_doRead(pData, Buffer, BytesToRead, BytesRead);
    
    LeaveCriticalSection (&g_memFile.cs);
    return TRUE;
}


/* --------------------------------------------------------------
 * Walk through mem file list; if one matches the given handle, then
 * delete it.  Must be called while <g_memFile.cs> is held!
 */
static VOID MEMFILE_DeleteMemFileByData(MemFileData_t *pData){
    if (pData){

        MEMFILE_releaseBuffer(pData->Buffer);
        
        RtlFreeHeap (GetProcessHeap (), 0, pData);
    }
}


VOID MEMFILE_DeleteMemFile (HANDLE hFile)
{
    MemFileData_t *pData;


    /* Optimization for common case - if no memory files, then no need
       to grab the cs */
    if (g_memFile.list == NULL || g_memFile.disabled)
        return;


    MEMFILE_Initialize ();


    EnterCriticalSection (&g_memFile.cs);
    
    pData = GetMemFile (hFile, TRUE);

    if (pData){
        TRACE("destroyed memory file for handle %u {fileSize = %lld bytes, MemFileCount = %d}\n", pData->Handle, pData->Buffer->fileSize.QuadPart, g_memFile.count);

        MEMFILE_DeleteMemFileByData(pData);
    }

    LeaveCriticalSection (&g_memFile.cs);
}
