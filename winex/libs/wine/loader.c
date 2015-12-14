/*
 * Win32 builtin dlls support
 *
 * Copyright 2000 Alexandre Julliard
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <unistd.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_MACH_O_DYLD_H
#include <mach-o/dyld.h>
#endif
#ifdef HAVE_LINK_H
#include <link.h>
#endif

#include "winnt.h"
#include "wine/library.h"
#include "global.h"

#define MAX_DLLS 100

static struct
{
    const IMAGE_NT_HEADERS *nt;           /* NT header */
    const char             *filename;     /* DLL file name */
} builtin_dlls[MAX_DLLS];

static int nb_dlls;

static const IMAGE_NT_HEADERS *main_exe;

static load_dll_callback_t load_dll_callback;

static char **dll_paths = NULL;
static int nb_dll_paths;
static int dll_path_maxlen;
static int init_done;

#ifdef __APPLE__
/* This is a hacky stubbly bit that we use to transition the main 
   thread from macos_main_SDL.m to the sdldrv if it's running.  It's
   a function pointer that both the driver and the main app know about - 
   when the driver is ready to start the event loop, it fills in the 
   function pointer */
   void (*SDLDRV_hack_eventloop)(void) = NULL;
   
/* We'll also use this opportunity to expose the SDL lock to the outside
   world in the same way that the tsx11 lib does for X11. */
   static void nop(void)
   {
   }

   void (*wine_sdldrv_lock)(void) = nop;
   void (*wine_sdldrv_unlock)(void) = nop;

   /* This hack variable is used to detect after launch activations */
   BOOL SDLDRV_hack_didBecomeActive = FALSE;
  
/* Finally, we'll use this to specify whether it's the initial exe being 
   run, or whether it's a sub-launched exe being run.  SDLDRV puts up 
   a splash screen for the main exe until the first primary surface is
   created */
   BOOL SDLDRV_hack_ismainapp = FALSE;
#endif


/* build the dll load path from the WINEDLLPATH variable */
void build_dll_path(void)
{
    int count = 0;
    char *p, *path = getenv( "WINEDLLPATH" );

    init_done = 1;
    if (!path) return;
    path = strdup(path);
    p = path;
    while (*p)
    {
        while (*p == ':') p++;
        if (!*p) break;
        count++;
        while (*p && *p != ':') p++;
    }

    dll_paths = malloc( count * sizeof(*dll_paths) );
    p = path;
    nb_dll_paths = 0;
    while (*p)
    {
        while (*p == ':') *p++ = 0;
        if (!*p) break;
        dll_paths[nb_dll_paths] = p;
        while (*p && *p != ':') p++;
        if (p - dll_paths[nb_dll_paths] > dll_path_maxlen)
            dll_path_maxlen = p - dll_paths[nb_dll_paths];
        nb_dll_paths++;
    }
}


/* build_native_dll_name(): builds a system specific library name from a windows DLL name
     (ie: 'kernel32.dll').  On success, returns the number of characters written to the 
     buffer <buf> not including the null terminator.  On failure, returns the required
     number of characters in the buffer including the null terminator.  To determine the
     required size of the buffer, call the function with the <buf> parameter set to NULL
     or <len> set to 0. */
size_t build_native_dll_name(const char *name, char *buf, size_t len){
    int         i;
    size_t      namelen = strlen(name);
    char *      ext;
    const char *prefix = "lib";
#if !defined(__APPLE__)
    const char *extension = ".so";
#else
    const char *extension = ".dylib";
#endif
    size_t      extLen = strlen(extension);
    size_t      required = namelen + extLen + strlen(prefix);


    if (buf == NULL)
        return required + 1;

    if (len <= required)
        return required + 1;


    /* store the name at the end of the buffer, prefixed by /lib and followed by .so */
    i = snprintf(buf, len, "%s%s", prefix, name);

    ext = strrchr(buf, '.');
    
    if (!(ext && (!strcmp(ext, ".dll") || !strcmp(ext, ".exe"))))
        ext = &buf[i];


    /* copy on the system dependent extension */
    memcpy(ext, extension, extLen + 1);


    return ext - buf + extLen;
}

/* build_dll_pathname(): builds a path and file name for the windows DLL named <name>
     (ie: 'kernel32.dll').  The name is appended to the <n>th DLL path in the search
     order list and returned in <buf>.  If <isNativeName> is non-zero, the string passed
     in <name> will be used directly assuming it is already a native library name.  If
     <isNativeName> is 0, the name will be converted to a native library name using the
     build_native_dll_name() function.  The length of the string will not exceed <len>
     characters.  If the buffer <buf> is not large enough, required length of the buffer,
     including the null terminator character, in characters will be returned.  To
     retrieve the maximum length of a DLL path segment for a given name, pass NULL the
     <buf> parameter and 0 for the <len> parameter.  In this case, the return value will
     be the maximum length, in characters, including the null terminator, of any path name
     that will be returned for the given name.  Returns 0 if <n> is larger than or equal
     to the number of DLL path segments.  Returns a the number of characters written to 
     <buf> not including the null terminator.  No guarantees are made that the file for 
     the created pathname actually exists. */
size_t build_dll_pathname(const char *name, int n, int isNativeName, char *buf, size_t len){
    size_t copyLen;
    size_t nameLen;


    if (!init_done)
        build_dll_path();


    /* return the max path segment length in <len> */
    if (len == 0 && buf == NULL){
        return (isNativeName ?  strlen(name) :          /* length of native name */
                                build_native_dll_name(name, NULL, 0)) +   /* length of the converted name */
               dll_path_maxlen +                        /* maximum length of a path segment */
               1 +                                      /* plus separator */
               1;                                       /* plus terminator */
    }


    if (n >= nb_dll_paths || n < 0)
        return 0;


    copyLen = strlen(dll_paths[n]);

    /* we'll assume this function was called to query the max buffer size before actually
       trying to build a path name. */
    if (!isNativeName)
        nameLen = build_native_dll_name(name, &buf[copyLen + 1], len - copyLen - 1);

    else{
        nameLen = strlen(name);

        memcpy(&buf[copyLen + 1], name, nameLen + 1);
    }


    buf[copyLen] = '/';
    memcpy(buf, dll_paths[n], copyLen);


    return copyLen + nameLen + 1;
}


/* open a library for a given dll, searching in the dll path
 * 'name' must be the Windows dll name (e.g. "kernel32.dll") */
static void *dlopen_dll( const char *name, char *error, int errorsize )
{
    size_t  nameLen;
    size_t  pathLen;
    char *  buffer;
    char *  libName;
    int     i;
    void *  ret = NULL;


    if (!init_done) 
        build_dll_path();

    /* generate the library name */
    nameLen = build_native_dll_name(name, NULL, 0);
    libName = (char *)malloc(nameLen * sizeof(char));
    if (libName == NULL)
        return NULL;

    build_native_dll_name(name, libName, nameLen);


    /* calculate the maximum buffer size and allocate */
    pathLen = build_dll_pathname(libName, 0, 1, NULL, 0);
    buffer = (char *)malloc(pathLen * sizeof(char));
    if (buffer == NULL){
        free(libName);
        return NULL;
    }


    for (i = 0; i < nb_dll_paths; i++){
        build_dll_pathname(libName, i, 1, buffer, pathLen);

        if ((ret = wine_dlopen(buffer, RTLD_NOW, error, errorsize)))
            break;
    }


    if (ret == NULL)
        ret = wine_dlopen(libName, RTLD_NOW, error, errorsize);

    free(buffer);
    free(libName);
    return ret;
}



/* adjust an array of pointers to make them into RVAs */
static inline void fixup_rva_ptrs( void *array, void *base, int count )
{
    void **ptr = (void **)array;
    while (count--)
    {
        if (*ptr) *ptr = (void *)((char *)*ptr - (char *)base);
        ptr++;
    }
}


/* fixup RVAs in the resource directory */
static void fixup_resources( IMAGE_RESOURCE_DIRECTORY *dir, char *root, void *base )
{
    IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    int i;

    entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    for (i = 0; i < dir->NumberOfNamedEntries + dir->NumberOfIdEntries; i++, entry++)
    {
        void *ptr = root + entry->u2.s3.OffsetToDirectory;
        if (entry->u2.s3.DataIsDirectory) fixup_resources( ptr, root, base );
        else
        {
            IMAGE_RESOURCE_DATA_ENTRY *data = ptr;
            fixup_rva_ptrs( &data->OffsetToData, base, 1 );
        }
    }
}


#ifdef HAVE_MACH_O_DYLD_H
BOOL get_segment_locations (LPCVOID pAddr, BYTE **pCodeStart, size_t *pCodeSize,
                            BYTE **pDataStart, size_t *pDataSize)
{
    const struct mach_header *pMachHeader;
    const struct load_command *pLoadCmd;
    uint32_t CmdNum;
    BOOL UsingHeaderOffsets = FALSE;

    pMachHeader = _dyld_get_image_header_containing_address (pAddr);
    if (!pMachHeader)
       return FALSE;

    /*fprintf (stderr, "pMachHeader: %p, Type: 0x%x, Flags: 0x%x\n", pMachHeader, pMachHeader->filetype, pMachHeader->flags);*/

    pLoadCmd = (const struct load_command *)((char *)pMachHeader +
                                             sizeof (*pMachHeader));
    for (CmdNum = 0; CmdNum < pMachHeader->ncmds; CmdNum++)
    {
       /* fprintf (stderr, "LoadCmd: 0x%x\n", pLoadCmd->cmd); */
       if (pLoadCmd->cmd == LC_SEGMENT)
       {
          const struct segment_command *pSeg;

          pSeg = (const struct segment_command *)pLoadCmd;

          /*fprintf (stderr, "Seg name: %s, Addr: 0x%x, Size: 0x%x, initprot: 0x%x, flags: 0x%x\n",
             pSeg->segname, pSeg->vmaddr, pSeg->vmsize, pSeg->initprot,
             pSeg->flags);*/

          if (!pSeg->vmaddr)
             UsingHeaderOffsets = TRUE;

          if (!strcmp (pSeg->segname, SEG_TEXT))
          {
             if (UsingHeaderOffsets)
                *pCodeStart = (LPBYTE)pMachHeader + pSeg->vmaddr;
             else
                *pCodeStart = (LPBYTE)pSeg->vmaddr;
             *pCodeSize = pSeg->vmsize;
          }
          else if (!strcmp (pSeg->segname, SEG_DATA))
          {
             if (UsingHeaderOffsets)
                *pDataStart = (LPBYTE)pMachHeader + pSeg->vmaddr;
             else
                *pDataStart = (LPBYTE)pSeg->vmaddr;
             *pDataSize = pSeg->vmsize;
          }
       }

       pLoadCmd = (const struct load_command *)((char *)pLoadCmd +
                                                pLoadCmd->cmdsize);
    }

    return TRUE;
}
#elif defined(HAVE_LINK_H)
#define ALL_PF_PERMS (PF_X | PF_W | PF_R)

typedef struct {
   void    *pBase;
   BYTE   **pCodeStart;
   size_t  *pCodeSize;
   BYTE   **pDataStart;
   size_t  *pDataSize;
   size_t   PageSize;
} SegInfo_t;


static int segment_iter_cb (struct dl_phdr_info *pInfo, size_t Size,
                            void *pData)
{
   SegInfo_t *pSegInfo = (SegInfo_t *)pData;
   int i;

   if ((void *)pInfo->dlpi_addr != pSegInfo->pBase)
   {
       /* For some reason on some systems, dlpi_addr is 0; the segments will
          be correct, so check the first segment */
       if (pInfo->dlpi_addr != 0)
           return 0;

       if ((pInfo->dlpi_phnum < 1) ||
           (pInfo->dlpi_phdr[0].p_vaddr != (Elf32_Addr)pSegInfo->pBase))
           return 0;
   }

   /*printf ("'%s' at 0x%x\n", pInfo->dlpi_name, pInfo->dlpi_addr);*/
   for (i = 0; i < pInfo->dlpi_phnum; i++)
   {
      const ElfW(Phdr) *pCur = &pInfo->dlpi_phdr[i];

      /*printf ("   header %2d: address=%p, type=0x%x, flags=0x%x\n", i,
              (void *)(pInfo->dlpi_addr + pCur->p_vaddr),
              pCur->p_type, pCur->p_flags);
      printf ("      memsize: 0x%x, filesize: 0x%x\n", pCur->p_memsz,
      pCur->p_filesz);*/

      if (pCur->p_type != PT_LOAD)
         continue;

      if ((pCur->p_flags & ALL_PF_PERMS) == (PF_R | PF_X))
      {
         *pSegInfo->pCodeStart = (void *)(pInfo->dlpi_addr + pCur->p_vaddr);
         *pSegInfo->pCodeSize = pCur->p_memsz;
      }
      else if ((pCur->p_flags & ALL_PF_PERMS) == (PF_R | PF_W))
      {
         *pSegInfo->pDataStart = (void *)(pInfo->dlpi_addr + pCur->p_vaddr);
         *pSegInfo->pDataSize = pCur->p_memsz;
      }
   }

   return 1;
}


BOOL get_segment_locations (LPCVOID pAddr, BYTE **pCodeStart, size_t *pCodeSize,
                            BYTE **pDataStart, size_t *pDataSize)
{
   Dl_info Info;
   SegInfo_t SegInfo;

   /* All this is to find the elf phdr structures for this particular shared
      library; the only way I've found to date is to walk the loaded
      libraries to find one with the same base address as the one we're
      interested in. Ugh. */
   if (!dladdr (pAddr, &Info))
   {
      fprintf (stderr, "Unable to get DL_info for %p!\n", pAddr);
      return FALSE;
   }

   SegInfo.pBase      = Info.dli_fbase;
   SegInfo.pCodeStart = pCodeStart;
   SegInfo.pCodeSize  = pCodeSize;
   SegInfo.pDataStart = pDataStart;
   SegInfo.pDataSize  = pDataSize;
   SegInfo.PageSize   = getpagesize ();
   dl_iterate_phdr (segment_iter_cb, &SegInfo);

   return TRUE;
}
#else
BOOL get_segment_locations (LPCVOID pAddr, BYTE **pCodeStart, size_t *pCodeSize,
                            BYTE **pDataStart, size_t *pDataSize)
{
   return FALSE;
}
#endif


/* map a builtin dll in memory and fixup RVAs */
static void *map_dll( const IMAGE_NT_HEADERS *nt_descr )
{
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    BYTE *addr, *code_start, *data_start;
    size_t page_size = getpagesize();
    int nb_sections = 2;  /* code + data */
    size_t code_size, data_size;

    size_t size = (sizeof(IMAGE_DOS_HEADER)
                   + sizeof(IMAGE_NT_HEADERS)
                   + nb_sections * sizeof(IMAGE_SECTION_HEADER));

    assert( size <= page_size );

    if (nt_descr->OptionalHeader.ImageBase)
    {
        addr = wine_anon_mmap( (void *)nt_descr->OptionalHeader.ImageBase,
                               page_size, PROT_READ|PROT_WRITE, MAP_FIXED );
        if (addr != (BYTE *)nt_descr->OptionalHeader.ImageBase) return NULL;
    }
    else
    {
        /* this will leak memory; but it should never happen */
        addr = wine_anon_mmap( NULL, page_size, PROT_READ|PROT_WRITE, 0 );
        if (addr == (BYTE *)-1) return NULL;
    }

    dos    = (IMAGE_DOS_HEADER *)addr;
    nt     = (IMAGE_NT_HEADERS *)(dos + 1);
    sec    = (IMAGE_SECTION_HEADER *)(nt + 1);

    /* Defaults */
    code_start = addr + page_size;

    /* HACK! */
    data_start = code_start + page_size;
    code_size = page_size;
    data_size = 0;

    get_segment_locations (nt_descr, &code_start, &code_size,
                           &data_start, &data_size);

    /* Sections may not be aligned on 4k boundaries on some platforms (such
       as Linux). Thus we need to align these sections for the VMM. Note that
       this may cause problems, since the result is that the last part of the
       code section and the first part of the data section may end up
       overlapping. */
    code_start = (LPVOID)(((unsigned long)code_start) & ~(page_size - 1));
    data_start = (LPVOID)(((unsigned long)data_start) & ~(page_size - 1));
    code_size = (code_size + (page_size - 1)) & ~(page_size - 1);
    data_size = (data_size + (page_size - 1)) & ~(page_size - 1);

    /* Build the DOS and NT headers */

    dos->e_magic  = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = sizeof(*dos);

    *nt = *nt_descr;

    nt->FileHeader.NumberOfSections                = nb_sections;
    nt->OptionalHeader.SizeOfCode                  = data_start - code_start;
    nt->OptionalHeader.SizeOfInitializedData       = 0;
    nt->OptionalHeader.SizeOfUninitializedData     = 0;
    nt->OptionalHeader.ImageBase                   = (DWORD)addr;

    fixup_rva_ptrs( &nt->OptionalHeader.AddressOfEntryPoint, addr, 1 );

    /* Build the code section */

    strcpy( (char *)sec->Name, ".text" );
    sec->SizeOfRawData = code_size;
    sec->Misc.VirtualSize = sec->SizeOfRawData;
    sec->VirtualAddress   = code_start - addr;
    sec->PointerToRawData = code_start - addr;
    sec->Characteristics  = (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
    sec++;
    VIRTUAL_AddLoadedView (code_start, code_size, PAGE_EXECUTE_READ);

    /* Build the data section */

    strcpy( (char *)sec->Name, ".data" );
    sec->SizeOfRawData = data_size;
    sec->Misc.VirtualSize = sec->SizeOfRawData;
    sec->VirtualAddress   = data_start - addr;
    sec->PointerToRawData = data_start - addr;
    sec->Characteristics  = (IMAGE_SCN_CNT_INITIALIZED_DATA |
                             IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ);
    sec++;
    if (data_size)
       VIRTUAL_AddLoadedView (data_start, data_size, PAGE_READWRITE);

    /* Build the import directory */

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (dir->Size)
    {
        IMAGE_IMPORT_DESCRIPTOR *imports = (void *)dir->VirtualAddress;
        fixup_rva_ptrs( &dir->VirtualAddress, addr, 1 );
        /* we can fixup everything at once since we only have pointers and 0 values */
        fixup_rva_ptrs( imports, addr, dir->Size / sizeof(void*) );
    }

    /* Build the resource directory */

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    if (dir->Size)
    {
        void *ptr = (void *)dir->VirtualAddress;
        fixup_rva_ptrs( &dir->VirtualAddress, addr, 1 );
        fixup_resources( ptr, ptr, addr );
    }

    /* Build the export directory */

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir->Size)
    {
        IMAGE_EXPORT_DIRECTORY *exports = (void *)dir->VirtualAddress;
        fixup_rva_ptrs( &dir->VirtualAddress, addr, 1 );
        fixup_rva_ptrs( (void *)exports->AddressOfFunctions, addr, exports->NumberOfFunctions );
        fixup_rva_ptrs( (void *)exports->AddressOfNames, addr, exports->NumberOfNames );
        fixup_rva_ptrs( &exports->Name, addr, 1 );
        fixup_rva_ptrs( &exports->AddressOfFunctions, addr, 1 );
        fixup_rva_ptrs( &exports->AddressOfNames, addr, 1 );
        fixup_rva_ptrs( &exports->AddressOfNameOrdinals, addr, 1 );
    }
    return addr;
}


/***********************************************************************
 *           __wine_dll_register
 *
 * Register a built-in DLL descriptor.
 */
void __wine_dll_register( const IMAGE_NT_HEADERS *header, const char *filename )
{
    if (load_dll_callback) load_dll_callback( map_dll(header), filename );
    else
    {
        if (!(header->FileHeader.Characteristics & IMAGE_FILE_DLL))
            main_exe = header;
        else
        {
            assert( nb_dlls < MAX_DLLS );
            builtin_dlls[nb_dlls].nt = header;
            builtin_dlls[nb_dlls].filename = filename;
            nb_dlls++;
        }
    }
}


/***********************************************************************
 *           wine_dll_set_callback
 *
 * Set the callback function for dll loading, and call it
 * for all dlls that were implicitly loaded already.
 */
void wine_dll_set_callback( load_dll_callback_t load )
{
    int i;
    load_dll_callback = load;
    for (i = 0; i < nb_dlls; i++)
    {
        const IMAGE_NT_HEADERS *nt = builtin_dlls[i].nt;
        if (!nt) continue;
        builtin_dlls[i].nt = NULL;
        load_dll_callback( map_dll(nt), builtin_dlls[i].filename );
    }
    nb_dlls = 0;
    if (main_exe) load_dll_callback( map_dll(main_exe), "" );
}


/***********************************************************************
 *           wine_dll_load
 *
 * Load a builtin dll.
 */
void *wine_dll_load( const char *filename, char *error, int errorsize )
{
    int i;

    /* callback must have been set already */
    assert( load_dll_callback );

    /* check if we have it in the list */
    /* this can happen when initializing pre-loaded dlls in wine_dll_set_callback */
    for (i = 0; i < nb_dlls; i++)
    {
        if (!builtin_dlls[i].nt) continue;
        if (!strcmp( builtin_dlls[i].filename, filename ))
        {
            const IMAGE_NT_HEADERS *nt = builtin_dlls[i].nt;
            builtin_dlls[i].nt = NULL;
            load_dll_callback( map_dll(nt), builtin_dlls[i].filename );
            return (void *)1;
        }
    }
    return dlopen_dll( filename, error, errorsize );
}


/***********************************************************************
 *           wine_dll_enum_load_path
 *
 * Provide an API for walking the DLL paths
 */

char *wine_dll_enum_load_path( int index )
{
    if (index < nb_dll_paths)
        return dll_paths[index];
    else
        return NULL;
}


/***********************************************************************
 *           wine_dll_unload
 *
 * Unload a builtin dll.
 */
void wine_dll_unload( void *handle )
{
    if (handle != (void *)1)
	wine_dlclose( handle, NULL, 0 );
}


/***********************************************************************
 *           wine_dll_load_main_exe
 *
 * Try to load the .so for the main exe, optionally searching for it in PATH.
 */
void *wine_dll_load_main_exe( const char *name, int search_path, char *error, int errorsize )
{
    void *ret = NULL;
    const char *path = NULL;
    if (search_path) path = getenv( "PATH" );

    if (!path)
    {
        /* no path, try only the specified name */
        ret = wine_dlopen( name, RTLD_NOW, error, errorsize );
    }
    else
    {
        char buffer[128], *tmp = buffer;
        size_t namelen = strlen(name);
        size_t pathlen = strlen(path);

        if (namelen + pathlen + 2 > sizeof(buffer)) tmp = malloc( namelen + pathlen + 2 );
        if (tmp)
        {
            char *basename = tmp + pathlen;
            *basename = '/';
            strcpy( basename + 1, name );
            for (;;)
            {
                int len;
                const char *p = strchr( path, ':' );
                if (!p) p = path + strlen(path);
                if ((len = p - path) > 0)
                {
                    memcpy( basename - len, path, len );
                    if ((ret = wine_dlopen( basename - len, RTLD_NOW, error, errorsize ))) break;
                }
                if (!*p) break;
                path = p + 1;
            }
            if (tmp != buffer) free( tmp );
        }
    }
    return ret;
}
