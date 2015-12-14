/*
 * File pe_module.c - handle PE module information
 *
 * Copyright (C) 1996,      Eric Youngdale.
 * Copyright (C) 1999-2000, Ulrich Weigand.
 * Copyright (C) 2004-2007, Eric Pouech.
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
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
 *
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dbghelp_private.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

/******************************************************************
 *		pe_load_stabs
 *
 * look for stabs information in PE header (it's how the mingw compiler provides 
 * its debugging information)
 */
static BOOL pe_load_stabs(const struct process* pcs, struct module* module, 
                          const void* mapping, IMAGE_NT_HEADERS* nth)
{
    IMAGE_SECTION_HEADER*       section;
    int                         i, stabsize = 0, stabstrsize = 0;
    unsigned int                stabs = 0, stabstr = 0;
    BOOL                        ret = FALSE;


    TRACE("looking for STABS debug info {pcs = %p, module = %p, mapping = %p, nth = %p}\n",
            pcs, module, mapping, nth);

    section = (IMAGE_SECTION_HEADER*)
        ((char*)&nth->OptionalHeader + nth->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nth->FileHeader.NumberOfSections; i++, section++)
    {
        if (!strcasecmp((const char*)section->Name, ".stab"))
        {
            stabs = section->VirtualAddress;
            stabsize = section->SizeOfRawData;

            TRACE("found a '.stab' section at 0x%08x {size = %d}\n", stabs, stabsize);
        }
        else if (!strncasecmp((const char*)section->Name, ".stabstr", 8))
        {
            stabstr = section->VirtualAddress;
            stabstrsize = section->SizeOfRawData;

            TRACE("found a '.stabstr' section at 0x%08x {size = %d}\n", stabstr, stabstrsize);
        }
    }

    if (stabstrsize && stabsize)
    {
        ret = stabs_parse(module, 
                          module->module.BaseOfImage - nth->OptionalHeader.ImageBase, 
                          RtlImageRvaToVa(nth, (HMODULE)mapping, stabs, NULL),
                          stabsize,
                          RtlImageRvaToVa(nth, (HMODULE)mapping, stabstr, NULL),
                          stabstrsize);
    }

    TRACE("%s the STABS debug info\n", ret ? "successfully loaded" : "failed to load");
    return ret;
}

/******************************************************************
 *		pe_load_dbg_file
 *
 * loads a .dbg file
 */
static BOOL pe_load_dbg_file(const struct process* pcs, struct module* module,
                             const char* dbg_name, DWORD timestamp)
{
    char                                tmp[MAX_PATH];
    HANDLE                              hFile = INVALID_HANDLE_VALUE, hMap = 0;
    const BYTE*                         dbg_mapping = NULL;
    const IMAGE_SEPARATE_DEBUG_HEADER*  hdr;
    const IMAGE_DEBUG_DIRECTORY*        dbg;
    BOOL                                ret = FALSE;

    WINE_TRACE("Processing DBG file %s\n", dbg_name);

    if (path_find_symbol_file(pcs, dbg_name, NULL, timestamp, 0, tmp, &module->module.DbgUnmatched) &&
        (hFile = CreateFileA(tmp, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE &&
        ((hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)) != 0) &&
        ((dbg_mapping = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) != NULL))
    {
        TRACE("opened .DBG file '%s'\n", tmp);
        hdr = (const IMAGE_SEPARATE_DEBUG_HEADER*)dbg_mapping;
        if (hdr->TimeDateStamp != timestamp)
        {
            WINE_ERR("Warning - %s has incorrect internal timestamp\n",
                     dbg_name);
            /*
             * Well, sometimes this happens to DBG files which ARE REALLY the
             * right .DBG files but nonetheless this check fails. Anyway,
             * WINDBG (debugger for Windows by Microsoft) loads debug symbols
             * which have incorrect timestamps.
             */
        }
        if (hdr->Signature == IMAGE_SEPARATE_DEBUG_SIGNATURE)
        {
            /* section headers come immediately after debug header */
            const IMAGE_SECTION_HEADER *sectp =
                (const IMAGE_SECTION_HEADER*)(hdr + 1);
            /* and after that and the exported names comes the debug directory */
            dbg = (const IMAGE_DEBUG_DIRECTORY*) 
                (dbg_mapping + sizeof(*hdr) + 
                 hdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER) +
                 hdr->ExportedNamesSize);


            ret = pe_load_debug_directory(pcs, module, dbg_mapping, sectp,
                                          hdr->NumberOfSections, dbg,
                                          hdr->DebugDirectorySize / sizeof(*dbg));
        }
        else
            ERR("Wrong signature in .DBG file %s\n", debugstr_a(tmp));
    }
    else
        WINE_ERR("-Unable to peruse .DBG file %s (%s)\n", dbg_name, debugstr_a(tmp));

    if (dbg_mapping) UnmapViewOfFile((void *)dbg_mapping);
    if (hMap) CloseHandle(hMap);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    return ret;
}

/******************************************************************
 *		pe_load_msc_debug_info
 *
 * Process MSC debug information in PE file.
 */
static BOOL pe_load_msc_debug_info(const struct process* pcs, 
                                   struct module* module,
                                   const void* mapping, IMAGE_NT_HEADERS* nth)
{
    BOOL                        ret = FALSE;
    const IMAGE_DATA_DIRECTORY* dir;
    const IMAGE_DEBUG_DIRECTORY*dbg = NULL;
    int                         nDbg;

    /* Read in debug directory */
    dir = nth->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_DEBUG;
    nDbg = dir->Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    if (!nDbg) return FALSE;

    dbg = RtlImageRvaToVa(nth, (HMODULE)mapping, dir->VirtualAddress, NULL);

    /* Parse debug directory */
    if (nth->FileHeader.Characteristics & IMAGE_FILE_DEBUG_STRIPPED)
    {
        /* Debug info is stripped to .DBG file */
        const IMAGE_DEBUG_MISC* misc = (const IMAGE_DEBUG_MISC*)
            ((const char*)mapping + dbg->PointerToRawData);


        TRACE("debug information has been stripped\n");

        if (nDbg != 1 || dbg->Type != IMAGE_DEBUG_TYPE_MISC ||
            misc->DataType != IMAGE_DEBUG_MISC_EXENAME)
        {
            WINE_ERR("-Debug info stripped, but no .DBG file in module %s\n",
                     debugstr_w(module->module.ModuleName));
        }
        else
        {
            TRACE("loading a .DBG file from '%s'\n", (const char*)misc->Data);
            ret = pe_load_dbg_file(pcs, module, (const char*)misc->Data, nth->FileHeader.TimeDateStamp);
        }
    }
    else
    {
        const IMAGE_SECTION_HEADER *sectp = (const IMAGE_SECTION_HEADER*)((const char*)&nth->OptionalHeader + nth->FileHeader.SizeOfOptionalHeader);


        TRACE("debug info is embedded in the module\n");
        /* Debug info is embedded into PE module */
        ret = pe_load_debug_directory(pcs, module, mapping, sectp,
            nth->FileHeader.NumberOfSections, dbg, nDbg);
    }

    return ret;
}

/***********************************************************************
 *			pe_load_export_debug_info
 */
static BOOL pe_load_export_debug_info(const struct process* pcs, 
                                      struct module* module, 
                                      const void* mapping, IMAGE_NT_HEADERS* nth)
{
    unsigned int 		        i;
    const IMAGE_EXPORT_DIRECTORY* 	exports;
    DWORD			        base = module->module.BaseOfImage;
    DWORD                               size;

    if (dbghelp_options & SYMOPT_NO_PUBLICS) return TRUE;

#if 0
    /* Add start of DLL (better use the (yet unimplemented) Exe SymTag for this) */
    /* FIXME: module.ModuleName isn't correctly set yet if it's passed in SymLoadModule */
    symt_new_public(module, NULL, module->module.ModuleName, base, 1,
                    TRUE /* FIXME */, TRUE /* FIXME */);
#endif
    
    /* Add entry point */
    symt_new_public(module, NULL, "EntryPoint", 
                    base + nth->OptionalHeader.AddressOfEntryPoint, 1,
                    TRUE, TRUE);
#if 0
    /* FIXME: we'd better store addresses linked to sections rather than 
       absolute values */
    IMAGE_SECTION_HEADER*       section;
    /* Add start of sections */
    section = (IMAGE_SECTION_HEADER*)
        ((char*)&nth->OptionalHeader + nth->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nth->FileHeader.NumberOfSections; i++, section++) 
    {
	symt_new_public(module, NULL, section->Name, 
                        RtlImageRvaToVa(nth, (void*)mapping, section->VirtualAddress, NULL), 
                        1, TRUE /* FIXME */, TRUE /* FIXME */);
    }
#endif

    /* Add exported functions */
    if ((exports = RtlImageDirectoryEntryToData((HMODULE)mapping, FALSE,
                                                IMAGE_DIRECTORY_ENTRY_EXPORT, &size)))
    {
        const WORD*             ordinals = NULL;
        const DWORD_PTR*	functions = NULL;
        const DWORD*		names = NULL;
        unsigned int		j;
        char			buffer[16];

        functions = RtlImageRvaToVa(nth, (HMODULE)mapping, exports->AddressOfFunctions, NULL);
        ordinals  = RtlImageRvaToVa(nth, (HMODULE)mapping, exports->AddressOfNameOrdinals, NULL);
        names     = RtlImageRvaToVa(nth, (HMODULE)mapping, exports->AddressOfNames, NULL);

        for (i = 0; i < exports->NumberOfNames; i++) 
        {
            if (!names[i]) continue;
            symt_new_public(module, NULL, 
                            RtlImageRvaToVa(nth, (HMODULE)mapping, names[i], NULL),
                            base + functions[ordinals[i]], 
                            1, TRUE /* FIXME */, TRUE /* FIXME */);
        }
	    
        for (i = 0; i < exports->NumberOfFunctions; i++) 
        {
            if (!functions[i]) continue;
            /* Check if we already added it with a name */
            for (j = 0; j < exports->NumberOfNames; j++)
                if ((ordinals[j] == i) && names[j]) break;
            if (j < exports->NumberOfNames) continue;
            snprintf(buffer, sizeof(buffer), "%u", i + exports->Base);
            symt_new_public(module, NULL, buffer, base + (DWORD)functions[i], 1,
                            TRUE /* FIXME */, TRUE /* FIXME */);
        }
    }
    /* no real debug info, only entry points */
    if (module->module.SymType == SymDeferred)
        module->module.SymType = SymExport;
    return TRUE;
}

/* pe_map_file(): helper function for mapping a native PE module into memory.  This lets
     ntdll's loader functions do the dirty work and we just get a mapping object back that
     we can map views from.  If the function is not available, we'll just fall back to
     doing the old mapping and attempting to process the headers manually. */
static HANDLE pe_map_file(HANDLE hFile)
{
    typedef HANDLE (WINAPI *LdrFindSectionHeader_Func)(HANDLE, BOOL);
    static LdrFindSectionHeader_Func LdrFindSectionHeader = NULL;


    if (LdrFindSectionHeader == NULL)
    {
        HMODULE ntdll;


        /* try to load the mapping helper function from ntdll.  Fallback to just using a
           standard file mapping if the function isn't available. */
        ntdll = GetModuleHandleA("ntdll.dll");

        if (ntdll == 0)
            goto Fail;

        LdrFindSectionHeader = (LdrFindSectionHeader_Func)GetProcAddress(ntdll, "LdrFindSectionHeader");

        if (LdrFindSectionHeader == NULL)
            goto Fail;
    }

    return LdrFindSectionHeader(hFile, FALSE);

Fail:
    return CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
}

/******************************************************************
 *		pe_load_debug_info
 *
 */
BOOL pe_load_debug_info(const struct process* pcs, struct module* module)
{
    BOOL                ret = FALSE;
    HANDLE              hFile;
    HANDLE              hMap;
    void*               mapping;
    IMAGE_NT_HEADERS*   nth;

    hFile = CreateFileW(module->module.LoadedImageName, GENERIC_READ, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE) return ret;
    if ((hMap = pe_map_file(hFile)) != 0)
    {
        if ((mapping = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) != NULL)
        {
            nth = RtlImageNtHeader((HMODULE)mapping);

            if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY))
            {
                TRACE("loading main debug info\n");

                /* recent versions of the MSC compiler can include both a .stab/.stabstr section
                   *and* a PDB reference in their modules.  We need to make sure to load both.
                   For the most part it seems that the .stab section includes references to PSDK
                   types and symbols, while the PDB contains information about the module itself. */
                ret = pe_load_stabs(pcs, module, mapping, nth);

                if (!ret)
                    WARN("failed to load the STABs info for the module '%s'\n", debugstr_a(module->module_name));

                ret = pe_load_msc_debug_info(pcs, module, mapping, nth) || ret;

                if (!ret)
                    ERR("loading debug information failed for module '%s'\n", debugstr_a(module->module_name));

                /* if we still have no debug info (we could only get SymExport at this
                 * point), then do the SymExport except if we have an ELF container, 
                 * in which case we'll rely on the export's on the ELF side
                 */
            }
/* FIXME shouldn't we check that? if (!module_get_debug(pcs, module))l */
            TRACE("loading public debug symbols\n");
            if (pe_load_export_debug_info(pcs, module, mapping, nth) && !ret)
                ret = TRUE;
            TRACE("%s the debug symbols\n", ret ? "successfully loaded" : "failed to load");
            UnmapViewOfFile(mapping);
        }
        CloseHandle(hMap);
    }
    CloseHandle(hFile);

    return ret;
}

/******************************************************************
 *		pe_load_native_module
 *
 */
struct module* pe_load_native_module(struct process* pcs, const WCHAR* name,
                                     HANDLE hFile, DWORD base, DWORD size)
{
    struct module*      module = NULL;
    BOOL                opened = FALSE;
    HANDLE              hMap;
    WCHAR               loaded_name[MAX_PATH];


    TRACE("(pcs = %p, name = '%s', hFile = %p, base = 0x%08x, size = %u)\n",
            pcs, debugstr_w(name), hFile, base, size);

    loaded_name[0] = '\0';
    if (!hFile)
    {

        assert(name);

        TRACE("attempting to find the executable image '%s'\n", debugstr_w(name));
        if ((hFile = FindExecutableImageExW(name, pcs->search_path, loaded_name, NULL, NULL)) == 0){
            WARN("could not open the executable image.  It could be another type of module...\n");
            return NULL;
        }

        TRACE("found the executable at '%s'\n", debugstr_w(loaded_name));
        opened = TRUE;
    }
    else if (name) strcpyW(loaded_name, name);
    else if (dbghelp_options & SYMOPT_DEFERRED_LOADS)
        FIXME("Trouble ahead (no module name passed in deferred mode)\n");

    if ((hMap = pe_map_file(hFile)) != 0)
    {
        void*   mapping;

        if ((mapping = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) != NULL)
        {
            IMAGE_NT_HEADERS*   nth = RtlImageNtHeader((HMODULE)mapping);

            if (nth)
            {
                if (!base) base = nth->OptionalHeader.ImageBase;
                if (!size) size = nth->OptionalHeader.SizeOfImage;

                TRACE("reading module information\n");
                module = module_new(pcs, loaded_name, DMT_PE, FALSE, base, size,
                                    nth->FileHeader.TimeDateStamp,
                                    nth->OptionalHeader.CheckSum);

                if (module)
                {
                    if (dbghelp_options & SYMOPT_DEFERRED_LOADS)
                        module->module.SymType = SymDeferred;
                    else{
                        TRACE("loading the debug information for module %p\n", module);
                        pe_load_debug_info(pcs, module);
                        TRACE("done loading debug information\n");
                    }
                }

                else
                    ERR("could not load the module '%s'\n", debugstr_w(loaded_name));
            }
            UnmapViewOfFile(mapping);
        }
        CloseHandle(hMap);
    }
    if (opened) CloseHandle(hFile);

    TRACE("done loading the module '%s'\n", debugstr_w(name));
    return module;
}

/******************************************************************
 *		pe_load_nt_header
 *
 */
BOOL pe_load_nt_header(HANDLE hProc, DWORD base, IMAGE_NT_HEADERS* nth)
{
    IMAGE_DOS_HEADER    dos;

    return ReadProcessMemory(hProc, (char*)base, &dos, sizeof(dos), NULL) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE &&
        ReadProcessMemory(hProc, (char*)(base + dos.e_lfanew), 
                          nth, sizeof(*nth), NULL) &&
        nth->Signature == IMAGE_NT_SIGNATURE;
}

/******************************************************************
 *      pe_load_nt_sect_header
 *
 *  attempts to read an NT section header from the module loaded
 *  at the base address <base>.
 *
 *  parameters:
 *    hProc:    handle to the process to read the header from.
 *    base:     base address of the module to be read from.
 *    i:        0-based index of the section to retrieve.
 *    nt:       [optional] pointer to the NT image header to use to
 *              lookup the section header.  If this value is non-NULL
 *              it is assumed to be a valid header.  If this value is
 *              NULL, the NT header will be read using a call to
 *              pe_load_nt_header().
 *    section:  pointer to the buffer that will receive the section
 *              header if read successfully.
 *
 *  return values:
 *    returns TRUE if the section was successfully read.
 *    returns FALSE if the section or NT header could not be read,
 *      or the index <i> was out of range of the number of sections
 *      in the module.
 */
BOOL pe_load_nt_sect_header(HANDLE hProc, DWORD base, int i, IMAGE_NT_HEADERS *nt, IMAGE_SECTION_HEADER *section){
    IMAGE_NT_HEADERS    _nt;


    /* NT header not passed in => read it locally */
    if (nt == NULL){
        if (!pe_load_nt_header(hProc, base, &_nt))
            return FALSE;

        nt = &_nt;
    }


    /* out of range section number */
    if (i < 0 || i >= nt->FileHeader.NumberOfSections)
        return FALSE;

    /* read the section header from the process memory */
    return ReadProcessMemory(   hProc, 
                                (char *)(base + 
                                         sizeof(IMAGE_DOS_HEADER) + 
                                         sizeof(IMAGE_NT_HEADERS) + 
                                         (i * sizeof(IMAGE_SECTION_HEADER))), 
                                section, 
                                sizeof(*section), 
                                NULL);
}

/******************************************************************
 *		pe_load_builtin_module
 *
 */
struct module* pe_load_builtin_module(struct process* pcs, const WCHAR* name,
                                      DWORD base, DWORD size)
{
    struct module*      module = NULL;

    if (base && pcs->dbg_hdr_addr)
    {
        IMAGE_NT_HEADERS    nth;

        if (pe_load_nt_header(pcs->handle, base, &nth))
        {
            if (!size) size = nth.OptionalHeader.SizeOfImage;
            module = module_new(pcs, name, DMT_PE, FALSE, base, size,
                                nth.FileHeader.TimeDateStamp,
                                nth.OptionalHeader.CheckSum);
        }
    }
    return module;
}
