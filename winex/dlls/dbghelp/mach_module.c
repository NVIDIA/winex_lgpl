/*
 *          Mach-O module container ('mach_module.c')
 *
 *  Copyright (c) 2009-2015 NVIDIA CORPORATION. All rights reserved.
 *  Author: Eric van Beurden
 *  Date: Jan 29, 2009
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
/*
 *  handles loading the debug symbols from a Mach-O formatted module.
*/
#include "config.h"

#include "windef.h"
#include "dbghelp_private.h"
#include "wine/debug.h"
#include "wine/module.h"
#include "heap.h"
#include "crc32.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "wine/unicode.h"
#include "wine/library.h"

DEFAULT_DEBUG_CHANNEL(dbghelp);


#ifdef __APPLE__
# include <mach-o/dyld.h>
# include <mach-o/loader.h>
# include <nlist.h>


/* SAFEALLOCBUF(): allocate a buffer to hold <size> bytes and check if the allocation
     was successful.  The trivial code in <err> is executed if the allocation fails. */
#define SAFEALLOCBUF(buf, size, err) { \
        if ((size) == 0) \
            (buf) = NULL; \
        else{ \
            (buf) = HeapAlloc(GetProcessHeap(), 0, (size)); \
            if ((buf) == NULL){ \
                ERR("could not allocate %ld bytes for %s\n", (size_t)(size), #buf); \
                err; \
            } \
        } \
    }

/* SAFEFREEBUF(): frees the buffer <x> if it is non-NULL. */
#define SAFEFREEBUF(x) { \
        if (x) \
            HeapFree(GetProcessHeap(), 0, x); \
    }

/* READBUF(): reads a symbol table from the file <map::fd>.  An attempt is made to
     read the full symbol table represented by <table>, located at offset <offset>
     in the file.  The <i>th load command entry in <map> is used to store the table
     data.  On failure (ie: not all data could be read), an error message is written
     out and the trivial code in <err> is executed. */
#define READBUF(map, offset, i, table, err) { \
        if ((map)->loadCmds[i].data.table##Size){ \
            size_t  bytes; \
            lseek((map)->fd, offset, SEEK_SET); \
            bytes = read((map)->fd, (map)->loadCmds[i].data.table, (map)->loadCmds[i].data.table##Size); \
             \
            if (bytes != (map)->loadCmds[i].data.table##Size){ \
                ERR("could not read all the data for %s\n", #table); \
                err; \
            } \
        } \
    }



/* struct dylib_relocation: seemingly undefined structure for the external and local
     symbol relocation tables in the dynamic symbol table of a mach-o formatted module. */
struct dylib_relocation{
    uint32_t offset;    /* unknown offset */
    uint32_t flags;     /* unknown flags */
};

/* struct dylib_indirect_symbol: undefined structure for indirect symbol reference
     table entries.  This is just a 32-bit index into the symbol table. */
struct dylib_indirect_symbol{
    uint32_t index;     /* index into the symbol table */
};

/* struct MachLoadCmd: contains data for a single mach-o formatted load command.
     The load command header is stored in the <header> member.  For the LC_SYMTAB
     and LC_DYSYMTAB entries, all the symbol table data is also stored. */
typedef struct{
    struct load_command *   header;             /* header data read straight from file */
    union{
        struct{
            BYTE *          stab;               /* symbol table data */
            BYTE *          strings;            /* string table data */
            size_t          stabSize;           /* symbol table size (in bytes) */
            size_t          stringsSize;        /* string table size (in bytes) */
        } stabs;
        struct{
            BYTE *          toc;                /* table of contents data */
            BYTE *          modtab;             /* module table data */
            BYTE *          extref;             /* external references data */
            BYTE *          indirectsym;        /* indirect symbols table */
            BYTE *          extrel;             /* external relocations table */
            BYTE *          locrel;             /* local relocations table */
            size_t          tocSize;            /* table of contents size (in bytes) */
            size_t          modtabSize;         /* module table size (in bytes) */
            size_t          extrefSize;         /* external references size (in bytes) */
            size_t          indirectsymSize;    /* indirect symbols size (in bytes) */
            size_t          extrelSize;         /* external relocations size (in bytes) */
            size_t          locrelSize;         /* local relocations size (in bytes) */
        } dyn;
    } data;
} MachLoadCmd;

/* struct MachMap: contains information about a single opened mach-o file.  Some of
     the file's debug information may be copied into the struct as it is mapped.
     Also stores some information found in the header of the file.  An optional
     linked structure is used to handle external debug information files. */
typedef struct MachMap{
    ULONG               modSize;        /* size of the module */
    ULONG               modStart;       /* base address of the module */
    ULONG               timestamp;      /* timestamp of the module (unavailable) */
    crc32               checksum;       /* CRC32 of the file data */
    int                 numLoadCmds;    /* number of load commands */
    MachLoadCmd *       loadCmds;       /* selected stored load commands */
    int                 fd;             /* descriptor for the open file */
    struct MachMap *    external;       /* data for a linked external debug file */
} MachMap;


/**************************************************************************************/
/* machLoadCmdAddCmd(): copy a load command header into the mapped file struct.  
     The table of stored load commands will be expanded to accomodate the new
     entry.  The new entry will be cleaned up by a call to machUnmapFile(). 
     Returns the index into the load command table where the new entry was
     stored. */
static int machLoadCmdAddCmd(MachMap *map, const struct load_command *lc){
    int index;


    /* create or expand the load command array */
    if (map->numLoadCmds == 0 || map->loadCmds == NULL)
        map->loadCmds = HeapAlloc(GetProcessHeap(), 0, sizeof(MachLoadCmd));

    else
        map->loadCmds = HeapReAlloc(GetProcessHeap(), 0, map->loadCmds, (map->numLoadCmds + 1) * sizeof(MachLoadCmd));

    if (map->loadCmds == NULL){
        ERR("could not allocate memory to store the load command headers\n");

        return -1;
    }


    /* save the current index for return */
    index = map->numLoadCmds;
    memset(&map->loadCmds[index], 0, sizeof(MachLoadCmd));
    map->loadCmds[index].header = HeapAlloc(GetProcessHeap(), 0, lc->cmdsize);

    if (map->loadCmds[index].header == NULL){
        ERR("could not allocate memory to store the load command headers\n");

        return -1;
    }


    /* copy the header into the new entry */
    memcpy(map->loadCmds[index].header, lc, lc->cmdsize);


    map->numLoadCmds++;

    return index;
}

/* machLoadCmdAddSegment(): log the finding of a segment load command and count up its size.
     Returns TRUE. */
static BOOL machLoadCmdAddSegment(MachMap *map, const struct segment_command *seg){
    TRACE("LC_SEGMENT:\n");
    TRACE("    segname =    '%s'\n",    seg->segname);
    TRACE("    vmaddr =     0x%08x\n",  seg->vmaddr);
    TRACE("    vmsize =     %d\n",      seg->vmsize);
    TRACE("    fileoff =    0x%08x\n",  seg->fileoff);
    TRACE("    filesize =   %d\n",      seg->filesize);
    TRACE("    maxprot =    0x%08x\n",  seg->maxprot);
    TRACE("    initprot =   0x%08x\n",  seg->initprot);
    TRACE("    nsects =     %d\n",      seg->nsects);
    TRACE("    flags =      0x%08x\n",  seg->flags);


    map->modSize += seg->vmsize;

    /* this is the text segment => grab its VM address as the module base. */
    /* NOTE: this value may be 0 for several dylibs.  In this case we'll have to attempt
             to grab the NT header from the module (if it's loaded) to grab the virtual
             address as the load offset. */
    if (!strcmp(seg->segname, SEG_TEXT))
        map->modStart = seg->vmaddr;

    return TRUE;
}

/* machLoadCmdAddSymTab(): log the finding of a LC_SYMTAB load command and store a copy of
     all its related data.  Returns TRUE if all the data was successfully read into the 
     buffers.  Returns FALSE if memory could not be allocated or the data could not be
     copied. */
static BOOL machLoadCmdAddSymTab(MachMap *map, const struct symtab_command *symtab){
    int     i;


    TRACE("LC_SYMTAB:\n");
    TRACE("    symoff =     %d\n", symtab->symoff);
    TRACE("    nsyms =      %d\n", symtab->nsyms);
    TRACE("    stroff =     %d\n", symtab->stroff);
    TRACE("    strsize =    %d\n", symtab->strsize);

    /* store the header for this load command */
    i = machLoadCmdAddCmd(map, (const struct load_command *)symtab);

    if (i == -1)
        return FALSE;


    /* we want to read in the entire symbol and string tables. */
    map->loadCmds[i].data.stabs.stringsSize = symtab->strsize;
    map->loadCmds[i].data.stabs.stabSize = sizeof(struct nlist) * symtab->nsyms;

    /* allocate memory for the table buffers */
    SAFEALLOCBUF(map->loadCmds[i].data.stabs.stab,    map->loadCmds[i].data.stabs.stabSize,    return FALSE);
    SAFEALLOCBUF(map->loadCmds[i].data.stabs.strings, map->loadCmds[i].data.stabs.stringsSize, return FALSE);

    /* read in the entire symbol and string tables */
    READBUF(map, symtab->symoff, i, stabs.stab, return FALSE);
    READBUF(map, symtab->stroff, i, stabs.strings, return FALSE);

    return TRUE;
}

/* machLoadCmdAddDySymTab(): log the finding of a LC_DYSYMTAB load command and store a copy
     of all its related data.  Returns TRUE if all the data was successfully read into the 
     buffers.  Returns FALSE if memory could not be allocated or the data could not be read. */
static BOOL machLoadCmdAddDySymTab(MachMap *map, const struct dysymtab_command *symtab){
    int     i;


    TRACE("LC_DYSYMTAB:\n");
    TRACE("    ilocalsym =      %d\n", symtab->ilocalsym);
    TRACE("    nlocalsym =      %d\n", symtab->nlocalsym);
    TRACE("    iextdefsym =     %d\n", symtab->iextdefsym);
    TRACE("    nextdefsym =     %d\n", symtab->nextdefsym);
    TRACE("    iundefsym =      %d\n", symtab->iundefsym);
    TRACE("    nundefsym =      %d\n", symtab->nundefsym);
    TRACE("    tocoff =         %d\n", symtab->tocoff);
    TRACE("    ntoc =           %d\n", symtab->ntoc);
    TRACE("    modtaboff =      %d\n", symtab->modtaboff);
    TRACE("    nmodtab =        %d\n", symtab->nmodtab);
    TRACE("    extrefsymoff =   %d\n", symtab->extrefsymoff);
    TRACE("    nextrefsyms =    %d\n", symtab->nextrefsyms);
    TRACE("    indirectsymoff = %d\n", symtab->indirectsymoff);
    TRACE("    nindirectsyms =  %d\n", symtab->nindirectsyms);
    TRACE("    extreloff =      %d\n", symtab->extreloff);
    TRACE("    nextrel =        %d\n", symtab->nextrel);
    TRACE("    locreloff =      %d\n", symtab->locreloff);
    TRACE("    nlocrel =        %d\n", symtab->nlocrel);

    /* store the header for this load command */
    i = machLoadCmdAddCmd(map, (const struct load_command *)symtab);

    if (i == -1)
        return FALSE;


    /* calculate all the buffer sizes */
    map->loadCmds[i].data.dyn.tocSize =         sizeof(struct dylib_table_of_contents) * symtab->ntoc;
    map->loadCmds[i].data.dyn.modtabSize =      sizeof(struct dylib_module) * symtab->nmodtab;
    map->loadCmds[i].data.dyn.extrefSize =      sizeof(struct dylib_reference) * symtab->nextrefsyms;
    map->loadCmds[i].data.dyn.indirectsymSize = sizeof(struct dylib_indirect_symbol) * symtab->nindirectsyms;
    map->loadCmds[i].data.dyn.extrelSize =      sizeof(struct dylib_relocation) * symtab->nextrel;
    map->loadCmds[i].data.dyn.locrelSize =      sizeof(struct dylib_relocation) * symtab->nlocrel;

    /* allocate buffers for the symbol table data */
    SAFEALLOCBUF(map->loadCmds[i].data.dyn.toc,         map->loadCmds[i].data.dyn.tocSize,         return FALSE);
    SAFEALLOCBUF(map->loadCmds[i].data.dyn.modtab,      map->loadCmds[i].data.dyn.modtabSize,      return FALSE);
    SAFEALLOCBUF(map->loadCmds[i].data.dyn.extref,      map->loadCmds[i].data.dyn.extrefSize,      return FALSE);
    SAFEALLOCBUF(map->loadCmds[i].data.dyn.indirectsym, map->loadCmds[i].data.dyn.indirectsymSize, return FALSE);
    SAFEALLOCBUF(map->loadCmds[i].data.dyn.extrel,      map->loadCmds[i].data.dyn.extrelSize,      return FALSE);
    SAFEALLOCBUF(map->loadCmds[i].data.dyn.locrel,      map->loadCmds[i].data.dyn.locrelSize,      return FALSE);

    /* read the symbol table data */
    READBUF(map, symtab->tocoff,         i, dyn.toc,         return FALSE);
    READBUF(map, symtab->modtaboff,      i, dyn.modtab,      return FALSE);
    READBUF(map, symtab->extrefsymoff,   i, dyn.extref,      return FALSE);
    READBUF(map, symtab->indirectsymoff, i, dyn.indirectsym, return FALSE);
    READBUF(map, symtab->extreloff,      i, dyn.extrel,      return FALSE);
    READBUF(map, symtab->locreloff,      i, dyn.locrel,      return FALSE);

    return TRUE;
}


/* machReadLoadCommands(): parses a mach-o load command table searching for segment and debug
     section commands.  The symbol information is copied into the <map> parameter on successful
     parse.  The operation will fail immediately if the mach header <hdr> is invalid.  Returns
     TRUE on success, and FALSE on failure. */
static BOOL machReadLoadCommands(MachMap *map, struct mach_header *hdr, BOOL checkHeaderOnly){
    BYTE *                      cmds = NULL;
    uint32_t                    i;
    unsigned long               len;
    BOOL                        ret = FALSE;
    const struct load_command * loadCmd;


    /* not a mach-o file => fail */
    if (hdr->magic != MH_MAGIC){
        switch (hdr->magic){
            case MH_CIGAM:      FIXME("support byte swapped mach-o files\n");           break;
            case MH_MAGIC_64:   FIXME("support 64-bit mach-o files\n");                 break;
            case MH_CIGAM_64:   FIXME("support 64-bit byte swapped mach-o files\n");    break;
            default:            TRACE("not a mach-o file\n");                           break;
        }

        return FALSE;
    }

    TRACE("read the mach header:\n");
    TRACE("    magic =       0x%08x\n", hdr->magic);
    TRACE("    cputype =     %d:%d\n",  hdr->cputype, hdr->cpusubtype);
    TRACE("    filetype =    0x%x\n",   hdr->filetype);
    TRACE("    ncmds =       %d\n",     hdr->ncmds);
    TRACE("    sizeofcmds =  %d\n",     hdr->sizeofcmds);
    TRACE("    flags =       0x%08x\n", hdr->flags);

    /* we were only asked to verify the header => succeed */
    if (checkHeaderOnly)
        return TRUE;


    /* allocate a buffer for the load commands and read them in */
    cmds = HeapAlloc(GetProcessHeap(), 0, hdr->sizeofcmds);

    if (cmds == NULL){
        ERR("could not allocate %d bytes for the load command buffer\n", hdr->sizeofcmds);

        goto done;
    }

    lseek(map->fd, sizeof(struct mach_header), SEEK_SET);
    len = read(map->fd, cmds, hdr->sizeofcmds);

    if (len != hdr->sizeofcmds){
        ERR("could not read the entire load command list\n");

        goto done;
    }


    /* parse the load commands list to find the debug sections, image size, image base, etc */
    loadCmd = (const struct load_command *)cmds;

    for (i = 0; i < hdr->ncmds; i++){
        if (loadCmd->cmd == LC_SEGMENT)
            machLoadCmdAddSegment(map, (const struct segment_command *)loadCmd);

        else if (loadCmd->cmd == LC_SYMTAB){
            if (!machLoadCmdAddSymTab(map, (const struct symtab_command *)loadCmd)){
                ERR("could not read all the symbol table information\n");
                ret = FALSE;
            }
        }

        else if (loadCmd->cmd == LC_DYSYMTAB){
            if (!machLoadCmdAddDySymTab(map, (const struct dysymtab_command *)loadCmd)){
                ERR("could not read all the dynamic symbol table information\n");
                ret = FALSE;
            }
        }


        /* move on to the next load command in the buffer */
        loadCmd = (const struct load_command *)(((BYTE *)loadCmd) + loadCmd->cmdsize);

        /* sanity check (should never hit this) */
        if (((BYTE *)loadCmd) > cmds + hdr->sizeofcmds){
            ERR("overran the load command buffer.  Possibly a corrupt mach-o module?\n");

            break;
        }
    }

    ret = TRUE;

done:
    SAFEFREEBUF(cmds);

    return ret;
}

/* machInitMap(): initializes a MachMap struct to be invalid/empty. */
static void machInitMap(MachMap *map){
    memset(map, 0, sizeof(MachMap));

    map->fd = -1;
}

/* machUnmapFile(): frees all data associated with the mapped mach file <map>.  This will
     gracefully handle partially created MachMap structures as long as they are in a valid 
     state. */
static BOOL machUnmapFile(MachMap *map){
    int i;


    TRACE("cleaning up the mapped file %p\n", map);

    /* free all data associated with the stored load commands */
    for (i = 0; i < map->numLoadCmds; i++){
        switch (map->loadCmds[i].header->cmd){
            case LC_SYMTAB:
                SAFEFREEBUF(map->loadCmds[i].data.stabs.stab);
                SAFEFREEBUF(map->loadCmds[i].data.stabs.strings);
                break;

            case LC_DYSYMTAB:
                SAFEFREEBUF(map->loadCmds[i].data.dyn.toc);
                SAFEFREEBUF(map->loadCmds[i].data.dyn.modtab);
                SAFEFREEBUF(map->loadCmds[i].data.dyn.extref);
                SAFEFREEBUF(map->loadCmds[i].data.dyn.indirectsym);
                SAFEFREEBUF(map->loadCmds[i].data.dyn.extrel);
                SAFEFREEBUF(map->loadCmds[i].data.dyn.locrel);
                break;

            default:
                break;
        }

        SAFEFREEBUF(map->loadCmds[i].header);
    }


    HeapFree(GetProcessHeap(), 0, map->loadCmds);

    /* recursively destroy and linked external debug files */
    if (map->external){
        machUnmapFile(map->external);

        HeapFree(GetProcessHeap(), 0, map->external);
    }


    /* close the file */
    if (map->fd != -1)
        close(map->fd);

    /* clear the struct to be empty and reuseable */
    machInitMap(map);

    return TRUE;
}

/* machMapFile(): opens, parses, and reads a mach-o formatted file.  The header is checked
     and parsed to ensure it is the correct file type.  The file identified by <imageName>
     will be opened and read into the struct in the <map> parameter.  This name should
     represent the actual dylib name, not the windows name.  A full path is optional.  If
     the <checkHeaderOnly> parameter is TRUE, the function will return immediately after
     verifying the mach header.  Returns TRUE if the file is a mach-o file and all the
     debug information could be read in.  Returns FALSE if the file is not a mach-o file,
     not enough memory could be allocated, or not all the data could be read. */
static BOOL machMapFile(const WCHAR *imageName, MachMap *map, BOOL checkHeaderOnly){
    struct stat         statbuf;
    struct mach_header  hdr;
    char *              filename;
    unsigned long       len;
    BOOL                ret = FALSE;


    machInitMap(map);

    len = WideCharToMultiByte(CP_UNIXCP, 0, imageName, -1, NULL, 0, NULL, NULL);

    if (!(filename = HeapAlloc(GetProcessHeap(), 0, len))){
        ERR("could not allocate %lu bytes for a filename\n", len);

        goto done;
    }

    WideCharToMultiByte(CP_UNIXCP, 0, imageName, -1, filename, len, NULL, NULL);


    /* check that the file exists */
    if (stat(filename, &statbuf) == -1 || S_ISDIR(statbuf.st_mode)){
        TRACE("the file '%s' could not be found or is a directory\n", filename);

        goto done;
    }


    /* Now open the file, so that we can mmap() it. */
    map->fd = open(filename, O_RDONLY);

    if (map->fd == -1){
        TRACE("could not open the file '%s'\n", filename);

        goto done;
    }



    /* read the mach header from the file */
    if (read(map->fd, &hdr, sizeof(hdr)) != sizeof(hdr)){
        ERR("could not read the entire mach header\n");

        goto done;
    }


    /* read the load commands and gather the image size data */
    ret = machReadLoadCommands(map, &hdr, checkHeaderOnly);

    if (!ret)
        ERR("could not read load commands\n");

    /* calculate the CRC32 for the file */
    map->checksum = calc_crc32_fd(map->fd);

done:
    SAFEFREEBUF(filename);

    if (!ret || checkHeaderOnly)
        machUnmapFile(map);

    return ret;
}


/* machFindActualFilename(): attempt to map a windows DLL name to the matching dylib name.  The
     name will be generated in the same manner as is done when calling LoadLibrary(), but will
     only search in the stored dylib paths (ie: not inside the install directories).  On success,
     TRUE will be returned and a pointer to an allocated buffer storing the actual filename will
     be returned through <actualName>.  This buffer must be freed using HeapFree() when no longer
     needed.  The file is guaranteed to exist on disk at the time of testing, but not at any time
     after.  On failure, FALSE is returned and <actualName> is set to NULL. */
static BOOL machFindActualFilename(const WCHAR *filename, WCHAR **actualName){
    DWORD           len;
    char *          libName = NULL;
    char *          path = NULL;
    char *          name = NULL;
    const WCHAR *   tmp;
    size_t          libLen;
    struct stat     st;
    int             n;
    BOOL            ret = FALSE;


    TRACE("attempting to find the actual filename for the library %s\n", debugstr_w(filename));

    *actualName = NULL;

    /* we only need the actual filename, not the full path of the library (which is likely 'c:/windows/system32') */
    tmp = strrchrW(filename, '/');
    if (tmp)
        filename = tmp + 1;

    tmp = strrchrW(filename, '\\');
    if (tmp)
        filename = tmp + 1;


    /* convert the original filename to ANSI */
    len = WideCharToMultiByte(CP_UNIXCP, 0, filename, -1, NULL, 0, NULL, NULL);
    SAFEALLOCBUF(name, len, goto done);
    WideCharToMultiByte(CP_UNIXCP, 0, filename, -1, name, len, NULL, NULL);


    /* generate the native library name */
    libLen = build_native_dll_name(name, NULL, 0);
    SAFEALLOCBUF(libName, libLen, goto done);
    build_native_dll_name(name, libName, libLen);
    TRACE("generated the native name '%s' for the library\n", libName);


    /* allocate a buffer for the path names */
    libLen = build_dll_pathname(libName, 0, TRUE, NULL, 0);
    SAFEALLOCBUF(path, libLen, goto done);


    /* loop through all the built-in DLL load paths and check to see if any exist */
    for (n = 0; libLen; n++){
        libLen = build_dll_pathname(libName, n, TRUE, path, libLen);

        /* no more paths available => stop searching */
        if (libLen == 0)
            break;

        TRACE("checking if '%s' exists {n = %d, libLen = %ld}\n", path, n, libLen);

        /* check to see if the file exists */
        if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode)){
            ret = TRUE;

            break;
        }
    }


    /* found a native library file that exists => convert the name back and return */
    if (ret){
        TRACE("the file '%s' exists.  Saving its name and returning\n", path);

        len = MultiByteToWideChar(CP_UNIXCP, 0, path, -1, NULL, 0);
        SAFEALLOCBUF(*actualName, len * sizeof(WCHAR), goto done);
        MultiByteToWideChar(CP_UNIXCP, 0, path, -1, *actualName, len);
    }

done:
    SAFEFREEBUF(name);
    SAFEFREEBUF(path);
    SAFEFREEBUF(libName);

    return ret;
}

/* machIsMachFile(): tests if the filename <imageName> can be mapped to a valid dylib file.
     If not NULL, a pointer to the actual filename for the dylib will be returned in the
     <_actualName> parameter.  This buffer must be freed using HeapFree() when no longer
     needed.  If not NULL, the file header and debug information will be read into the
     <map> object.  If <map> is NULL, only the header of the file will be verified.
     Returns TRUE if the file <imageName> can be mapped to a valid dylib, and FALSE
     otherwise. */
static BOOL machIsMachFile(const WCHAR *imageName, WCHAR **_actualName, MachMap *map){
    MachMap _map;
    BOOL    checkHeaderOnly = (map == NULL);
    WCHAR * actualName;
    BOOL    result;


    if (_actualName)
        *_actualName = NULL;

    if (map == NULL)
        map = &_map;


    TRACE("trying to find the module name for %s\n", debugstr_w(imageName));

    /* try to find the dylib name and file for the windows DLL name */
    if (!machFindActualFilename(imageName, &actualName)){
        ERR("could not find a dylib file for %s\n", debugstr_w(imageName));

        return FALSE;
    }


    TRACE("found the actual name of %s to be %s\n", debugstr_w(imageName), debugstr_w(actualName));

    /* try to read the file and verify that it is actually a mach-o file */
    result = machMapFile(actualName, map, checkHeaderOnly);


    /* the actual name was requested => store it */
    if (_actualName)
        *_actualName = actualName;

    /* the actual name was not requested => free it */
    else
        SAFEFREEBUF(actualName);

    return result;
}


/* machLoadDebugInfo(): attempts to load the debug information for the module identified by 
     <module>.  If <map> is NULL, the dylib name is generated from the image name stored 
     in <module> and the file's header and debug information is read into <map> if found.
     If <map> is non-NULL, it is assumed that the debug information has already been read
     into it, and parses from that instead.  Returns TRUE if the file was found, successfully 
     opened, and had its debug information parsed.  Returns FALSE on failure. */
BOOL machLoadDebugInfo(struct process *pcs, struct module *module, MachMap *map){
    MachMap _map;
    BOOL    opened = FALSE;
    BOOL    ret = FALSE;
    int     i;
    DWORD64 loadOffset = 0;


    TRACE("loading the debug information for %s {module = %p, map = %p}\n", debugstr_w(module->module.LoadedImageName), module, map);

    /* the file has not been mapped yet => map it now */
    if (map == NULL){
        /* this isn't a mach file or it couldn't be loaded => fail */
        if (!machIsMachFile(module->module.LoadedImageName, NULL, &_map)){
            ERR("could not map the image file %s into memory\n", debugstr_w(module->module.LoadedImageName));

            return FALSE;
        }

        map = &_map;
        opened = TRUE;
    }


    /* try to retrieve the load offset from the NT header.  The module base address that is passed
       into machLoadModule() and that is stored in 'module->module.BaseOfImage' is not entirely the
       correct address.  The symbol addresses stored in the debug information are relative to the
       start of the mach module itself, and that always seems to load at an address 0x2000 bytes
       below the module's base address.  Unfortunately, all the addresses that get passed to the
       SymFromAddr() function are relative to the address stored in 'module->module.BaseOfImage.
       Happily, the offset between these two addresses is stored in the module's header for the
       text section (IMAGE_SECTION_HEADER).  We'll attempt to grab the offset from there instead
       of hardcoding it.  The load offset will be applied to all the symbol addresses as they are
       parsed. */
    if (module->module.BaseOfImage){
        IMAGE_NT_HEADERS nt;


        /* read the NT header => attempt to grab a section header and extract the load offset */
        if (pe_load_nt_header(pcs->handle, module->module.BaseOfImage, &nt)){
            IMAGE_SECTION_HEADER    sec;
            int                     i;


            for (i = 0; i < nt.FileHeader.NumberOfSections; i++){
                if (pe_load_nt_sect_header(pcs->handle, module->module.BaseOfImage, i, &nt, &sec)){
                    if (!strcmp((char *)sec.Name, ".text")){
                        loadOffset = (DWORD64)(signed)sec.VirtualAddress;

                        break;
                    }
                }
            }

            if (loadOffset == 0)
                WARN("could not find a valid text section header.  Assuming a load offset of 0.  Symbol lookups will fail in this module\n");
        }

        else{
            /* HACK!!!  This value seems to be the correct offset for our modules, but we
                        couldn't calculate it because this module isn't currently loaded.
                        Setting this offset here allows symbols from non-resident modules
                        to still be looked up by address (assuming the base address was
                        correctly specified as well). */
            loadOffset = -0x2000ll;
            FIXME("module could not be read from.  Assuming a load offset of 0x%08llx\n", loadOffset);
        }
    }

    else
        ERR("no module base specified or found.  Symbol lookups in this module will fail!\n");



    for (i = 0; i < map->numLoadCmds; i++){
        switch (map->loadCmds[i].header->cmd){
            case LC_SYMTAB:
                TRACE("parsing the STABS debug information {BaseOfImage = 0x%08llx, loadOffset = 0x%08llx}\n", 
                        module->module.BaseOfImage,
                        loadOffset);

                ret = stabs_parse(  module,
                                    module->module.BaseOfImage + loadOffset,
                                    map->loadCmds[i].data.stabs.stab,
                                    map->loadCmds[i].data.stabs.stabSize,
                                    (const char *)map->loadCmds[i].data.stabs.strings,
                                    map->loadCmds[i].data.stabs.stringsSize);

                TRACE("%s the STABS debug information\n", ret ? "SUCCESSFULLY loaded" : "FAILED to load");
                break;

            case LC_DYSYMTAB:
                /* FIXME!! figure out if we need to load or fixup anything from this table */
                FIXME("figure out if we need to load or fixup anything from the dynamic symbol table\n");
                break;

            default:
                continue;
        }
    }


    TRACE("done loading debug information for %s\n", debugstr_w(module->module.LoadedImageName));

    if (opened)
        machUnmapFile(map);

    return ret;
}


/* machLoadModule(): attempts to find and load the debug information for the module <imageName>.
     The filename in <imageName> is assumed to be a windows DLL name (and optional path).  The
     <moduleBase> and <moduleSize> parameters are stored in the module information that is 
     returned.  On success, a module object is created, added to the module list in the process
     desription <pcs>, and is returned.  Failing to load the debug information from a module
     is not considered failure, and the module object will still be returned.  Returns NULL if
     the module does not represent a dylib, the file could not be opened or read, or the memory
     could not be allocated. */
struct module *machLoadModule(struct process *pcs, const WCHAR *imageName, DWORD64 moduleBase, ULONG moduleSize){
    struct module * module;
    WCHAR *         actualName;
    MachMap         map;


    TRACE("adding mach-o module {pcs = %p, imageName = %s, moduleBase = 0x%08llx, moduleSize = %d bytes}\n",
            pcs, debugstr_w(imageName), moduleBase, moduleSize);

    /* make sure it's not a 64-bit address */
    if (!validate_addr64(moduleBase)){
        FIXME("module base is actually 64-bit!\n");

        return NULL;
    }

    moduleBase = normalize_addr64(moduleBase);

    /* verify that this image represents a dylib file */
    if (!machIsMachFile(imageName, &actualName, NULL) || actualName == NULL){
        TRACE("the module %s was not loaded from a dylib file\n", debugstr_w(imageName));
        SAFEFREEBUF(actualName);

        return NULL;
    }


    /* need to load the module file to get the header information */
    if (machMapFile(actualName, &map, FALSE)){

        /* correct the module base and size if they were passed in as 0 */
        if (moduleBase == 0){
            if (map.modStart == 0)
                FIXME("no module base specified and couldn't guess at it\n");

            moduleBase = map.modStart;
        }

        if (moduleSize == 0)
            moduleSize = map.modSize;
    }

    else{
        ERR("could not open the dylib %s to gather module header information\n", debugstr_w(actualName));
        SAFEFREEBUF(actualName);

        return NULL;
    }


    /* create a container for the module information */
    module = module_new(pcs, 
                        imageName,
                        DMT_DYLIB,
                        FALSE,          /* <-- not a virtual module */
                        moduleBase,
                        moduleSize,
                        map.timestamp,
                        map.checksum);

    if (module == NULL){
        ERR("could not create a new module container\n");
        SAFEFREEBUF(actualName);
        machUnmapFile(&map);

        return NULL;
    }


    /* deferred module debug loads is set in the options => mark it and allow the module 
         to be loaded when first needed */
    if (dbghelp_options & SYMOPT_DEFERRED_LOADS)
        module->module.SymType = SymDeferred;

    /* attempt to load the module's debug information */
    else{
        BOOL ret;


        TRACE("loading the debug information for module %p\n", module);

        ret = machLoadDebugInfo(pcs, module, &map);

        TRACE("loading debug information %s\n", ret ? "SUCCEEDED" : "FAILED");
    }


    machUnmapFile(&map);
    SAFEFREEBUF(actualName);

    return module;
}


#else   /* __APPLE__ */

BOOL machLoadDebugInfo(struct process *pcs, struct module *module, struct MachMap *map){
    TRACE("stub! {pcs = %p, module = %p, map = %p}\n", pcs, module, map);

    return FALSE;
}

struct module *machLoadModule(struct process *pcs, const WCHAR *imageName, DWORD64 moduleBase, ULONG moduleSize){
    TRACE("stub! {pcs = %p, imageName = %s, moduleBase = 0x%08llx, moduleSize = 0x%08x}\n",
            pcs, debugstr_w(imageName), moduleBase, moduleSize);

    return NULL;
}

#endif
