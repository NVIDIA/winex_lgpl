/*
 * File module.c - module handling for the wine debugger
 *
 * Copyright (C) 1993,      Eric Youngdale.
 * 		 2000-2007, Eric Pouech
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
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dbghelp_private.h"
#include "psapi.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

const WCHAR        S_ElfW[]         = {'<','e','l','f','>','\0'};
const WCHAR        S_WineLoaderW[]  = {'<','w','i','n','e','-','l','o','a','d','e','r','>','\0'};
static const WCHAR S_DotSoW[]       = {'.','s','o','\0'};
static const WCHAR S_DotPdbW[]      = {'.','p','d','b','\0'};
static const WCHAR S_DotDbgW[]      = {'.','d','b','g','\0'};
static const WCHAR S_DotDylibW[]    = {'.','d','y','l','i','b','\0'};
const WCHAR        S_WinePThreadW[] = {'w','i','n','e','-','p','t','h','r','e','a','d','\0'};
const WCHAR        S_WineKThreadW[] = {'w','i','n','e','-','k','t','h','r','e','a','d','\0'};
const WCHAR        S_SlashW[]       = {'/','\0'};

static const WCHAR S_AcmW[] = {'.','a','c','m','\0'};
static const WCHAR S_DllW[] = {'.','d','l','l','\0'};
static const WCHAR S_DrvW[] = {'.','d','r','v','\0'};
static const WCHAR S_ExeW[] = {'.','e','x','e','\0'};
static const WCHAR S_OcxW[] = {'.','o','c','x','\0'};
static const WCHAR S_VxdW[] = {'.','v','x','d','\0'};
static const WCHAR * const ext[] = {S_AcmW, S_DllW, S_DrvW, S_ExeW, S_OcxW, S_VxdW, NULL};

static int match_ext(const WCHAR* ptr, size_t len)
{
    const WCHAR* const *e;
    size_t      l;

    for (e = ext; *e; e++)
    {
        l = strlenW(*e);
        if (l >= len) return FALSE;
        if (strncmpiW(&ptr[len - l], *e, l)) continue;
        return l;
    }
    return 0;
}

static const WCHAR* get_filename(const WCHAR* name, const WCHAR* endptr)
{
    const WCHAR*        ptr;

    if (!endptr) endptr = name + strlenW(name);
    for (ptr = endptr - 1; ptr >= name; ptr--)
    {
        if (*ptr == '/' || *ptr == '\\') break;
    }
    return ++ptr;
}

static void module_fill_module(const WCHAR* in, WCHAR* out, size_t size)
{
    const WCHAR *ptr, *endptr;
    size_t      len, l;

    ptr = get_filename(in, endptr = in + strlenW(in));
    len = min(endptr - ptr, size - 1);
    memcpy(out, ptr, len * sizeof(WCHAR));
    out[len] = '\0';
    if (len > 4 && (l = match_ext(out, len)))
        out[len - l] = '\0';
    else if (len > 12 &&
             (!strcmpiW(out + len - 12, S_WinePThreadW) ||
              !strcmpiW(out + len - 12, S_WineKThreadW)))
        lstrcpynW(out, S_WineLoaderW, size);
    else
    {
        if (len > 3 && !strcmpiW(&out[len - 3], S_DotSoW) &&
            (l = match_ext(out, len - 3)))
            strcpyW(&out[len - l - 3], S_ElfW);
    }
    while ((*out = tolowerW(*out))) out++;
}

void module_set_module(struct module* module, const WCHAR* name)
{
    module_fill_module(name, module->module.ModuleName, sizeof(module->module.ModuleName));
    WideCharToMultiByte(CP_ACP, 0, module->module.ModuleName, -1,
                        module->module_name, sizeof(module->module_name),
                        NULL, NULL);
}

static const char*      get_module_type(enum module_type type, BOOL virtual)
{
    switch (type)
    {
    case DMT_ELF: return virtual ? "Virtual ELF" : "ELF";
    case DMT_PE: return virtual ? "Virtual PE" : "PE";
    case DMT_DYLIB: return virtual ? "Virtual dylib" : "dylib";
    default: return "---";
    }
}

/***********************************************************************
 * Creates and links a new module to a process
 */
struct module* module_new(struct process* pcs, const WCHAR* name,
                          enum module_type type, BOOL virtual,
                          unsigned long mod_addr, unsigned long size,
                          unsigned long stamp, unsigned long checksum)
{
    struct module*      module;

    assert(type == DMT_ELF || type == DMT_PE || type == DMT_DYLIB);
    if (!(module = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*module))))
	return NULL;

    module->next = pcs->lmodules;
    pcs->lmodules = module;

    TRACE("=> %s %08lx-%08lx %s\n",
          get_module_type(type, virtual), mod_addr, mod_addr + size,
          debugstr_w(name));

    pool_init(&module->pool, 65536);

    module->module.SizeOfStruct = sizeof(module->module);
    module->module.BaseOfImage = mod_addr;
    module->module.ImageSize = size;
    module_set_module(module, name);
    module->module.ImageName[0] = '\0';
    lstrcpynW(module->module.LoadedImageName, name, sizeof(module->module.LoadedImageName) / sizeof(WCHAR));
    module->module.SymType = SymNone;
    module->module.NumSyms = 0;
    module->module.TimeDateStamp = stamp;
    module->module.CheckSum = checksum;

    memset(module->module.LoadedPdbName, 0, sizeof(module->module.CVData));
    module->module.CVSig = 0;
    memset(module->module.CVData, 0, sizeof(module->module.CVData));
    module->module.PdbSig = 0;
    memset(&module->module.PdbSig70, 0, sizeof(module->module.PdbSig70));
    module->module.PdbAge = 0;
    module->module.PdbUnmatched = FALSE;
    module->module.DbgUnmatched = FALSE;
    module->module.LineNumbers = FALSE;
    module->module.GlobalSymbols = FALSE;
    module->module.TypeInfo = FALSE;
    module->module.SourceIndexed = FALSE;
    module->module.Publics = FALSE;

    module->type              = type;
    module->is_virtual        = virtual ? TRUE : FALSE;
    module->sortlist_valid    = FALSE;
    module->addr_sorttab      = NULL;
    /* FIXME: this seems a bit too high (on a per module basis)
     * need some statistics about this
     */
    hash_table_init(&module->pool, &module->ht_symbols, 4096);
    hash_table_init(&module->pool, &module->ht_types,   4096);
    vector_init(&module->vtypes, sizeof(struct symt*),  32);

    module->sources_used      = 0;
    module->sources_alloc     = 0;
    module->sources           = 0;

    return module;
}

/***********************************************************************
 *	module_find_by_name
 *
 */
static struct module* module_find_by_name(const struct process* pcs, const WCHAR* name)
{
    struct module*      module;

    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!strcmpiW(name, module->module.ModuleName)) return module;
    }
    SetLastError(ERROR_INVALID_NAME);
    return NULL;
}

struct module* module_find_by_nameA(const struct process* pcs, const char* name)
{
    WCHAR wname[MAX_PATH];

    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, sizeof(wname) / sizeof(WCHAR));
    return module_find_by_name(pcs, wname);
}

/***********************************************************************
 *	module_is_already_loaded
 *
 */
struct module* module_is_already_loaded(const struct process* pcs, const WCHAR* name)
{
    struct module*      module;
    const WCHAR*        filename;

    /* first compare the loaded image name... */
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!strcmpiW(name, module->module.LoadedImageName))
            return module;
    }
    /* then compare the standard filenames (without the path) ... */
    filename = get_filename(name, NULL);
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!strcmpiW(filename, get_filename(module->module.LoadedImageName, NULL)))
            return module;
    }
    SetLastError(ERROR_INVALID_NAME);
    return NULL;
}

/***********************************************************************
 *           module_get_container
 *
 */
static struct module* module_get_container(const struct process* pcs,
                                    const struct module* inner)
{
    struct module*      module;
     
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module != inner &&
            module->module.BaseOfImage <= inner->module.BaseOfImage &&
            module->module.BaseOfImage + module->module.ImageSize >=
            inner->module.BaseOfImage + inner->module.ImageSize)
            return module;
    }
    return NULL;
}

/***********************************************************************
 *           module_get_containee
 *
 */
struct module* module_get_containee(const struct process* pcs, 
                                    const struct module* outter)
{
    struct module*      module;
     
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module != outter &&
            outter->module.BaseOfImage <= module->module.BaseOfImage &&
            outter->module.BaseOfImage + outter->module.ImageSize >=
            module->module.BaseOfImage + module->module.ImageSize)
            return module;
    }
    return NULL;
}

/******************************************************************
 *		module_get_debug
 *
 * get the debug information from a module:
 * - if the module's type is deferred, then force loading of debug info (and return
 *   the module itself)
 * - if the module has no debug info and has an ELF container, then return the ELF
 *   container (and also force the ELF container's debug info loading if deferred)
 * - otherwise return the module itself if it has some debug info
 */
BOOL module_get_debug(struct module_pair* pair)
{
    IMAGEHLP_DEFERRED_SYMBOL_LOADW64    idslW64;

    if (!pair->requested) return FALSE;
    /* for a PE builtin, always get info from container */
    if (!(pair->effective = module_get_container(pair->pcs, pair->requested)))
        pair->effective = pair->requested;
    /* if deferred, force loading */
    if (pair->effective->module.SymType == SymDeferred)
    {
        BOOL ret;
        
        if (pair->effective->is_virtual) ret = FALSE;
        else switch (pair->effective->type)
        {
        case DMT_ELF:
            ret = elf_load_debug_info(pair->effective, NULL);
            break;
        case DMT_DYLIB:
            ret = machLoadDebugInfo(pair->pcs, pair->effective, NULL);
            break;
        case DMT_PE:
            idslW64.SizeOfStruct = sizeof(idslW64);
            idslW64.BaseOfImage = pair->effective->module.BaseOfImage;
            idslW64.CheckSum = pair->effective->module.CheckSum;
            idslW64.TimeDateStamp = pair->effective->module.TimeDateStamp;
            memcpy(idslW64.FileName, pair->effective->module.ImageName,
                   sizeof(pair->effective->module.ImageName));
            idslW64.Reparse = FALSE;
            idslW64.hFile = INVALID_HANDLE_VALUE;

            pcs_callback(pair->pcs, CBA_DEFERRED_SYMBOL_LOAD_START, &idslW64);
            ret = pe_load_debug_info(pair->pcs, pair->effective);
            pcs_callback(pair->pcs,
                         ret ? CBA_DEFERRED_SYMBOL_LOAD_COMPLETE : CBA_DEFERRED_SYMBOL_LOAD_FAILURE,
                         &idslW64);
            break;
        default:
            ret = FALSE;
            break;
        }
        if (!ret) pair->effective->module.SymType = SymNone;
        assert(pair->effective->module.SymType != SymDeferred);
        pair->effective->module.NumSyms = pair->effective->ht_symbols.num_elts;
    }
    return pair->effective->module.SymType != SymNone;
}

/***********************************************************************
 *	module_find_by_addr
 *
 * either the addr where module is loaded, or any address inside the 
 * module
 */
struct module* module_find_by_addr(const struct process* pcs, unsigned long addr, 
                                   enum module_type type)
{
    struct module*      module;
    
    if (type == DMT_UNKNOWN)
    {
        if ((module = module_find_by_addr(pcs, addr, DMT_PE)) ||
            (module = module_find_by_addr(pcs, addr, DMT_ELF)) ||
            (module = module_find_by_addr(pcs, addr, DMT_DYLIB)))
            return module;
    }
    else
    {
        for (module = pcs->lmodules; module; module = module->next)
        {
            if (type == module->type && addr >= module->module.BaseOfImage &&
                addr < module->module.BaseOfImage + module->module.ImageSize) 
                return module;
        }
    }
    SetLastError(ERROR_INVALID_ADDRESS);
    return module;
}

/******************************************************************
 *		module_is_elf_container_loaded
 *
 * checks whether the ELF container, for a (supposed) PE builtin is
 * already loaded
 */
static BOOL module_is_elf_container_loaded(const struct process* pcs,
                                           const WCHAR* ImageName, DWORD base)
{
    size_t              len;
    struct module*      module;
    PCWSTR              filename, modname;

    if (!base) return FALSE;
    filename = get_filename(ImageName, NULL);
    len = strlenW(filename);

    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module->type == DMT_ELF &&
            base >= module->module.BaseOfImage &&
            base < module->module.BaseOfImage + module->module.ImageSize)
        {
            modname = get_filename(module->module.LoadedImageName, NULL);
            if (!strncmpiW(modname, filename, len) &&
                !memcmp(modname + len, S_DotSoW, 3 * sizeof(WCHAR)))
            {
                return TRUE;
            }
        }
    }
    /* likely a native PE module */
    WARN("Couldn't find container for %s\n", debugstr_w(ImageName));
    return FALSE;
}

/******************************************************************
 *		module_get_type_by_name
 *
 * Guesses a filename type from its extension
 */
enum module_type module_get_type_by_name(const WCHAR* name)
{
    size_t          len = strlenW(name);
    const WCHAR *   ext;
    const size_t    extLen = (sizeof(S_WinePThreadW) / sizeof(S_WinePThreadW[0])) - 1;


    /* Skip all version extensions (.[digits]) regex: "(\.\d+)*$" */
    do
    {
        size_t i = len;

        while (i && isdigit(name[i - 1]))
            i--;

        if (i && name[i - 1] == '.')
            len = i - 1;
        else
            break;
    } while (len);


    /* the buffer is too short to possibly include any important extension => fail */
    if (len < sizeof(S_DotSoW) / sizeof(S_DotSoW[0]))
        return DMT_PE;

    /* find the start of the file extension */
    ext = &name[len - 1];

    while (ext > name && *ext != '.')
        ext--;


    /* for now we'll assume that a module name cannot be *just* an extension */
    if (ext > name){

        /* check for terminating .so or .so.[digit] */
        if (!strncmpiW(ext, S_DotSoW, (sizeof(S_DotSoW) / sizeof(S_DotSoW[0])) - 1))
            return DMT_ELF;

        if (!strncmpiW(ext, S_DotPdbW, (sizeof(S_DotPdbW) / sizeof(S_DotPdbW[0])) - 1))
            return DMT_PDB;

        if (!strncmpiW(ext, S_DotDylibW, (sizeof(S_DotDylibW) / sizeof(S_DotDylibW[0])) - 1))
            return DMT_DYLIB;

        if (!strncmpiW(ext, S_DotDbgW, (sizeof(S_DotDbgW) / sizeof(S_DotDbgW[0])) - 1))
            return DMT_DBG;
    }

    /* wine-[kp]thread is also an ELF module */
    if (((len > extLen && name[len - extLen - 1] == '/') || len == extLen) &&
        (!strncmpiW(name + len - extLen, S_WinePThreadW, extLen) ||
         !strncmpiW(name + len - extLen, S_WineKThreadW, extLen)))
    {
        return DMT_ELF;
    }

    return DMT_PE;
}

/***********************************************************************
 *			SymLoadModule (DBGHELP.@)
 */
DWORD WINAPI SymLoadModule(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
                           PCSTR ModuleName, DWORD BaseOfDll, DWORD SizeOfDll)
{
    TRACE("(hProcess = %p, hFile = %p, ImageName = '%s', ModuleName = '%s', BaseOfDll = 0x%08x, SizeOfDll = %u)\n",
            hProcess, hFile, ImageName, ModuleName, BaseOfDll, SizeOfDll);

    return SymLoadModuleEx(hProcess, hFile, ImageName, ModuleName, BaseOfDll,
                           SizeOfDll, NULL, 0);
}

/***********************************************************************
 *			SymLoadModuleEx (DBGHELP.@)
 */
DWORD64 WINAPI  SymLoadModuleEx(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
                                PCSTR ModuleName, DWORD64 BaseOfDll, DWORD DllSize,
                                PMODLOAD_DATA Data, DWORD Flags)
{
    PWSTR       wImageName, wModuleName;
    unsigned    len;
    DWORD64     ret;

    TRACE("(hProcess = %p, hFile = %p, ImageName = '%s', ModuleName = '%s', BaseOfDll = %s, DllSize = %u, Data = %p, Flags = 0x%08x)\n",
          hProcess, hFile, debugstr_a(ImageName), debugstr_a(ModuleName),
          wine_dbgstr_longlong(BaseOfDll), DllSize, Data, Flags);

    if (ImageName)
    {
        len = MultiByteToWideChar(CP_ACP, 0, ImageName, -1, NULL, 0);
        wImageName = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, ImageName, -1, wImageName, len);
    }
    else wImageName = NULL;
    if (ModuleName)
    {
        len = MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, NULL, 0);
        wModuleName = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, wModuleName, len);
    }
    else wModuleName = NULL;

    ret = SymLoadModuleExW(hProcess, hFile, wImageName, wModuleName,
                          BaseOfDll, DllSize, Data, Flags);
    HeapFree(GetProcessHeap(), 0, wImageName);
    HeapFree(GetProcessHeap(), 0, wModuleName);
    return ret;
}

/***********************************************************************
 *			SymLoadModuleExW (DBGHELP.@)
 */
DWORD64 WINAPI  SymLoadModuleExW(HANDLE hProcess, HANDLE hFile, PCWSTR wImageName,
                                 PCWSTR wModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll,
                                 PMODLOAD_DATA Data, DWORD Flags)
{
    struct process*     pcs;
    struct module*	module = NULL;

    
    TRACE("(hProcess = %p, hFile = %p, ImageName = '%s', ModuleName = '%s', BaseOfDll = %s, DllSize = %u, Data = %p, Flags = 0x%08x)\n",
          hProcess, hFile, debugstr_w(wImageName), debugstr_w(wModuleName),
          wine_dbgstr_longlong(BaseOfDll), SizeOfDll, Data, Flags);

    if (Data)
        FIXME("Unsupported load data parameter %p for %s\n",
              Data, debugstr_w(wImageName));
    if (!validate_addr64(BaseOfDll)) return 0;
    BaseOfDll = normalize_addr64(BaseOfDll);

    if (!(pcs = process_find_by_handle(hProcess))) return 0;

    if (Flags & SLMFLAG_VIRTUAL)
    {
        module = module_new(pcs, wImageName, module_get_type_by_name(wImageName),
                            TRUE, (DWORD)BaseOfDll, SizeOfDll, 0, 0);
        if (!module) return 0;
        if (wModuleName) module_set_module(module, wModuleName);
        /* this assumes that the <wImageName> parameter is non-NULL.  Luckily, we
           won't get to this point if it is NULL since module_new() will fail (or
           crash. */
        lstrcpynW(module->module.ImageName, wImageName,
                  sizeof(module->module.ImageName) / sizeof(WCHAR));
        module->module.SymType = SymVirtual;

        return BaseOfDll;
    }
    if (Flags & ~(SLMFLAG_VIRTUAL))
        FIXME("Unsupported Flags %08x for %s\n", Flags, debugstr_w(wImageName));

    /* force transparent ELF loading / unloading */
    elf_synchronize_module_list(pcs);

    /* this is a Wine extension to the API just to redo the synchronisation */
    if (!wImageName && !hFile) return 0;

    /* check if the module is already loaded, or if it's a builtin PE module with
     * an containing ELF module
     */
    if (wImageName)
    {
        TRACE("loading the module '%s'\n", debugstr_w(wImageName));
        module = module_is_already_loaded(pcs, wImageName);

        TRACE("checking if the module's elf container is already loaded\n");
        if (!module && module_is_elf_container_loaded(pcs, wImageName, BaseOfDll))
        {
            /* force the loading of DLL as builtin */
            TRACE("loading the module '%s' as builtin {base = 0x%08llx, size = %u}\n", debugstr_w(wImageName), BaseOfDll, SizeOfDll);
            module = pe_load_builtin_module(pcs, wImageName, BaseOfDll, SizeOfDll);
        }
    }
    if (!module)
    {
        TRACE("module '%s' not loaded yet.  Trying as a native PE module\n", debugstr_w(wImageName));
        /* otherwise, try a regular PE module */
        if (!(module = pe_load_native_module(pcs, wImageName, hFile, BaseOfDll, SizeOfDll)))
        {
            TRACE("PE load failed... Getting the dbg typename from filename '%s'\n", debugstr_w(wImageName));

            /* trying a mach-o format... */
            module = machLoadModule(pcs, wImageName, BaseOfDll, SizeOfDll);

            /* and finally an ELF module */
            if (!module && module_get_type_by_name(wImageName) == DMT_ELF){
                TRACE("found an ELF file.  Loading\n");
                module = elf_load_module(pcs, wImageName, BaseOfDll);
            }
        }
    }
    if (!module)
    {
        WARN("Couldn't locate %s\n", debugstr_w(wImageName));
        return 0;
    }
    module->module.NumSyms = module->ht_symbols.num_elts;
    /* by default module_new fills module.ModuleName from a derivation
     * of LoadedImageName. Overwrite it, if we have better information
     */
    TRACE("setting the module name\n");
    if (wModuleName)
        module_set_module(module, wModuleName);
    lstrcpynW(module->module.ImageName, wImageName,
              sizeof(module->module.ImageName) / sizeof(WCHAR));

    return module->module.BaseOfImage;
}

/***********************************************************************
 *                     SymLoadModule64 (DBGHELP.@)
 */
DWORD64 WINAPI SymLoadModule64(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
                               PCSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll)
{
    if (!validate_addr64(BaseOfDll)) return FALSE;
    BaseOfDll = normalize_addr64(BaseOfDll);
    return SymLoadModule(hProcess, hFile, ImageName, ModuleName, (DWORD)BaseOfDll, SizeOfDll);
}

/******************************************************************
 *		module_remove
 *
 */
BOOL module_remove(struct process* pcs, struct module* module)
{
    struct module**     p;

    TRACE("%s (%p)\n", debugstr_w(module->module.ModuleName), module);
    hash_table_destroy(&module->ht_symbols);
    hash_table_destroy(&module->ht_types);
    HeapFree(GetProcessHeap(), 0, module->sources);
    HeapFree(GetProcessHeap(), 0, module->addr_sorttab);
    HeapFree(GetProcessHeap(), 0, module->dwarf2_info);
    pool_destroy(&module->pool);
    /* native dbghelp doesn't invoke registered callback(,CBA_SYMBOLS_UNLOADED,) here
     * so do we
     */
    for (p = &pcs->lmodules; *p; p = &(*p)->next)
    {
        if (*p == module)
        {
            *p = module->next;
            HeapFree(GetProcessHeap(), 0, module);
            return TRUE;
        }
    }
    FIXME("This shouldn't happen\n");
    return FALSE;
}

/******************************************************************
 *		SymUnloadModule (DBGHELP.@)
 *
 */
BOOL WINAPI SymUnloadModule(HANDLE hProcess, DWORD BaseOfDll)
{
    struct process*     pcs;
    struct module*      module;

    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;
    module = module_find_by_addr(pcs, BaseOfDll, DMT_UNKNOWN);
    if (!module) return FALSE;
    return module_remove(pcs, module);
}

/******************************************************************
 *		SymUnloadModule64 (DBGHELP.@)
 *
 */
BOOL WINAPI SymUnloadModule64(HANDLE hProcess, DWORD64 BaseOfDll)
{
    struct process*     pcs;
    struct module*      module;

    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;
    if (!validate_addr64(BaseOfDll)) return FALSE;
    BaseOfDll = normalize_addr64(BaseOfDll);
    module = module_find_by_addr(pcs, (DWORD)BaseOfDll, DMT_UNKNOWN);
    if (!module) return FALSE;
    return module_remove(pcs, module);
}

/******************************************************************
 *		SymEnumerateModules (DBGHELP.@)
 *
 */
struct enum_modW64_32
{
    PSYM_ENUMMODULES_CALLBACK   cb;
    PVOID                       user;
    char                        module[MAX_PATH];
};

static BOOL CALLBACK enum_modW64_32(PCWSTR name, DWORD64 base, PVOID user)
{
    struct enum_modW64_32*      x = user;

    WideCharToMultiByte(CP_ACP, 0, name, -1, x->module, sizeof(x->module), NULL, NULL);
    return x->cb(x->module, (DWORD)base, x->user);
}

BOOL  WINAPI SymEnumerateModules(HANDLE hProcess,
                                 PSYM_ENUMMODULES_CALLBACK EnumModulesCallback,  
                                 PVOID UserContext)
{
    struct enum_modW64_32       x;

    x.cb = EnumModulesCallback;
    x.user = UserContext;

    return SymEnumerateModulesW64(hProcess, enum_modW64_32, &x);
}

/******************************************************************
 *		SymEnumerateModules64 (DBGHELP.@)
 *
 */
struct enum_modW64_64
{
    PSYM_ENUMMODULES_CALLBACK64 cb;
    PVOID                       user;
    char                        module[MAX_PATH];
};

static BOOL CALLBACK enum_modW64_64(PCWSTR name, DWORD64 base, PVOID user)
{
    struct enum_modW64_64*      x = user;

    WideCharToMultiByte(CP_ACP, 0, name, -1, x->module, sizeof(x->module), NULL, NULL);
    return x->cb(x->module, base, x->user);
}

BOOL  WINAPI SymEnumerateModules64(HANDLE hProcess,
                                   PSYM_ENUMMODULES_CALLBACK64 EnumModulesCallback,  
                                   PVOID UserContext)
{
    struct enum_modW64_64       x;

    x.cb = EnumModulesCallback;
    x.user = UserContext;

    return SymEnumerateModulesW64(hProcess, enum_modW64_64, &x);
}

/* safeStrLenW(): calculates the length of a wide string up to a maximum of
     <maxLen> characters.  This function should be used instead of strlenW()
     in cases where a potentially unterminated string could be in use. */
static DWORD safeStrLenW(const WCHAR *str, DWORD maxLen)
{
    DWORD len = 0;


    /* bad or empty string => fail */
    if (str == NULL || maxLen <= 0)
        return 0;

    /* count the characters up to the limit or we find a terminator */
    while (str[0] && len < maxLen)
    {
        len++;
        str++;
    }

    return len;
}

/* get_module_name(): extracts the module name from the [potentially] full path
     name <name>.  This extraction is done in a manner that will still safely
     work on an unterminated string.  The <maxLen> parameter is used to specify
     the maximum number of characters to process.  The extracted name is stored
     in a buffer that is internally allocated.  The pointer to this buffer is
     returned through the <buffer> parameter, and its length in characters is
     returned through the <len> parameter.  The first call to this function
     should have the value in <len> set to 0 or the value in <buffer> set to
     NULL.  Returns a pointer to the extracted name buffer on success.  Returns
     NULL if the buffer could not be allocated or resized.  On failure, the
     buffer pointer and buffer length will not be modified.  The buffer must be
     later cleaned up with a call to cleanup_module_name_buffer(). */
static WCHAR *get_module_name(const WCHAR *name, DWORD maxLen, WCHAR **buffer, DWORD *len)
{
    DWORD           nameLen = safeStrLenW(name, maxLen);
    DWORD           required;
    const WCHAR *   ext;
    const WCHAR *   path;
    int             i;


    /* assume the name doesn't have a path separator or absolute path element. */
    ext = &name[nameLen];
    path = name;

    /* make a pass backwards through the string to find the file extension and
       the last path separator. */
    for (i = nameLen - 1; i >= 0; i--)
    {
        if (name[i] == '.')
            ext = &name[i];

        else if (name[i] == '/' || name[i] == '\\')
        {
            path = &name[i] + 1;
            break;
        }
    }


    /* calculate the number of characters that we need to copy */
    required = (ext - path + 1);

    /* no buffer yet => allocate a new buffer for the name */
    if ((*buffer) == NULL || (*len) == 0)
    {
        *buffer = HeapAlloc(GetProcessHeap(), 0, required * sizeof(WCHAR));

        if ((*buffer) == NULL)
            return NULL;

        *len = required;
    }

    /* the existing buffer is too small => increase its size */
    else if ((*len) < required)
    {
        WCHAR *tmp;


        tmp = HeapReAlloc(GetProcessHeap(), 0, *buffer, required * sizeof(WCHAR));

        if (tmp == NULL)
            return NULL;

        *buffer = tmp;
        *len = required;
    }


    /* copy the name string to the buffer and make sure it is terminated. */
    memcpy(*buffer, path, (required - 1) * sizeof(WCHAR));
    (*buffer)[required - 1] = 0;

    return (*buffer);
}

/* cleanup_module_name_buffer(): cleans up a buffer allocated by the get_module_name()
     function.  This needs to be passed the same buffer and length values as were
     returned from the last successful call to get_module_name(). */
static void cleanup_module_name_buffer(WCHAR *buffer, DWORD len)
{
    if (buffer)
        HeapFree(GetProcessHeap(), 0, buffer);
}

/******************************************************************
 *		SymEnumerateModulesW64 (DBGHELP.@)
 *
 */
BOOL  WINAPI SymEnumerateModulesW64(HANDLE hProcess,
                                    PSYM_ENUMMODULES_CALLBACKW64 EnumModulesCallback,
                                    PVOID UserContext)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;
    DWORD               bufLen = 0;
    WCHAR *             nameBuffer = NULL;
    const WCHAR *       moduleName;


    if (!pcs) return FALSE;
    
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!(dbghelp_options & SYMOPT_WINE_WITH_ELF_MODULES) && module->type == DMT_ELF)
            continue;

        /* extract the module name from the image name.  Using the <ModuleName> member
           will be insufficient for long module names since it only has a size of 32
           characters and could result in a truncated or unterminated string being used.
           We need to guarantee that the module name is both untruncated and terminated,
           so we'll extract it manually from the image name.  The fallback in the case 
           of everything going wrong will be to use the <ModuleName> member (and spew 
           an error message). */
        moduleName = get_module_name(module->module.ImageName,
                                     sizeof(module->module.ImageName) / sizeof(module->module.ImageName[0]),
                                     &nameBuffer,
                                     &bufLen);

        /* could not extract the module name from the image name => report the error and
             fall back to using the module name stored in the buffer.  This may still lead
             to the use of a truncated or unterminated name. */
        if (moduleName == NULL)
        {
            ERR("could not allocate memory for the module name buffer\n");
            moduleName = module->module.ModuleName;
        }

        if (!EnumModulesCallback(moduleName,
                                 module->module.BaseOfImage,
                                 UserContext))
            break;
    }

    cleanup_module_name_buffer(nameBuffer, bufLen);

    return TRUE;
}

/******************************************************************
 *		EnumerateLoadedModules64 (DBGHELP.@)
 *
 */
struct enum_load_modW64_64
{
    PENUMLOADED_MODULES_CALLBACK64      cb;
    PVOID                               user;
    char                                module[MAX_PATH];
};

static BOOL CALLBACK enum_load_modW64_64(PCWSTR name, DWORD64 base, ULONG size,
                                         PVOID user)
{
    struct enum_load_modW64_64* x = user;

    WideCharToMultiByte(CP_ACP, 0, name, -1, x->module, sizeof(x->module), NULL, NULL);
    return x->cb(x->module, base, size, x->user);
}

BOOL  WINAPI EnumerateLoadedModules64(HANDLE hProcess,
                                      PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback,
                                      PVOID UserContext)
{
    struct enum_load_modW64_64  x;

    x.cb = EnumLoadedModulesCallback;
    x.user = UserContext;

    return EnumerateLoadedModulesW64(hProcess, enum_load_modW64_64, &x);
}

/******************************************************************
 *		EnumerateLoadedModules (DBGHELP.@)
 *
 */
struct enum_load_modW64_32
{
    PENUMLOADED_MODULES_CALLBACK        cb;
    PVOID                               user;
    char                                module[MAX_PATH];
};

static BOOL CALLBACK enum_load_modW64_32(PCWSTR name, DWORD64 base, ULONG size,
                                         PVOID user)
{
    struct enum_load_modW64_32* x = user;
    WideCharToMultiByte(CP_ACP, 0, name, -1, x->module, sizeof(x->module), NULL, NULL);
    return x->cb(x->module, (DWORD)base, size, x->user);
}

BOOL  WINAPI EnumerateLoadedModules(HANDLE hProcess,
                                    PENUMLOADED_MODULES_CALLBACK EnumLoadedModulesCallback,
                                    PVOID UserContext)
{
    struct enum_load_modW64_32  x;

    x.cb = EnumLoadedModulesCallback;
    x.user = UserContext;

    return EnumerateLoadedModulesW64(hProcess, enum_load_modW64_32, &x);
}

/******************************************************************
 *		EnumerateLoadedModulesW64 (DBGHELP.@)
 *
 */
BOOL  WINAPI EnumerateLoadedModulesW64(HANDLE hProcess,
                                       PENUMLOADED_MODULES_CALLBACKW64 EnumLoadedModulesCallback,
                                       PVOID UserContext)
{
    HMODULE*    hMods;
    WCHAR       baseW[256], modW[256];
    DWORD       i, sz;
    MODULEINFO  mi;

    hMods = HeapAlloc(GetProcessHeap(), 0, 256 * sizeof(hMods[0]));
    if (!hMods) return FALSE;


    TRACE("retrieving the module list for process %p\n", hProcess);
    if (!EnumProcessModules(hProcess, hMods, 256 * sizeof(hMods[0]), &sz))
    {
        /* hProcess should also be a valid process handle !! */
        FIXME("If this happens, bump the number in mod\n");
        HeapFree(GetProcessHeap(), 0, hMods);
        return FALSE;
    }


    sz /= sizeof(HMODULE);

    for (i = 0; i < sz; i++)
    {
        TRACE("getting info for the module %p\n", hMods[i]);
        if (!GetModuleInformation(hProcess, hMods[i], &mi, sizeof(mi)) ||
            !GetModuleBaseNameW(hProcess, hMods[i], baseW, sizeof(baseW) / sizeof(WCHAR))){
            ERR("couldn't get info for %p\n", hMods[i]);
            continue;
        }

        module_fill_module(baseW, modW, sizeof(modW) / sizeof(CHAR));
        EnumLoadedModulesCallback(modW, (DWORD_PTR)mi.lpBaseOfDll, mi.SizeOfImage,
                                  UserContext);
    }
    HeapFree(GetProcessHeap(), 0, hMods);

    return sz != 0 && i == sz;
}

/******************************************************************
 *		SymGetModuleInfo (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfo(HANDLE hProcess, DWORD dwAddr,
                              PIMAGEHLP_MODULE ModuleInfo)
{
    IMAGEHLP_MODULE     mi;
    IMAGEHLP_MODULEW64  miw64;

    if (sizeof(mi) < ModuleInfo->SizeOfStruct) FIXME("Wrong size\n");

    miw64.SizeOfStruct = sizeof(miw64);
    if (!SymGetModuleInfoW64(hProcess, dwAddr, &miw64)) return FALSE;

    mi.SizeOfStruct  = miw64.SizeOfStruct;
    mi.BaseOfImage   = miw64.BaseOfImage;
    mi.ImageSize     = miw64.ImageSize;
    mi.TimeDateStamp = miw64.TimeDateStamp;
    mi.CheckSum      = miw64.CheckSum;
    mi.NumSyms       = miw64.NumSyms;
    mi.SymType       = miw64.SymType;
    WideCharToMultiByte(CP_ACP, 0, miw64.ModuleName, -1,
                        mi.ModuleName, sizeof(mi.ModuleName), NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, miw64.ImageName, -1,
                        mi.ImageName, sizeof(mi.ImageName), NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, miw64.LoadedImageName, -1,
                        mi.LoadedImageName, sizeof(mi.LoadedImageName), NULL, NULL);

    memcpy(ModuleInfo, &mi, ModuleInfo->SizeOfStruct);

    return TRUE;
}

/******************************************************************
 *		SymGetModuleInfoW (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfoW(HANDLE hProcess, DWORD dwAddr,
                               PIMAGEHLP_MODULEW ModuleInfo)
{
    IMAGEHLP_MODULEW64  miw64;
    IMAGEHLP_MODULEW    miw;

    if (sizeof(miw) < ModuleInfo->SizeOfStruct) FIXME("Wrong size\n");

    miw64.SizeOfStruct = sizeof(miw64);
    if (!SymGetModuleInfoW64(hProcess, dwAddr, &miw64)) return FALSE;

    miw.SizeOfStruct  = miw64.SizeOfStruct;
    miw.BaseOfImage   = miw64.BaseOfImage;
    miw.ImageSize     = miw64.ImageSize;
    miw.TimeDateStamp = miw64.TimeDateStamp;
    miw.CheckSum      = miw64.CheckSum;
    miw.NumSyms       = miw64.NumSyms;
    miw.SymType       = miw64.SymType;
    strcpyW(miw.ModuleName, miw64.ModuleName);
    strcpyW(miw.ImageName, miw64.ImageName);
    strcpyW(miw.LoadedImageName, miw64.LoadedImageName);
    memcpy(ModuleInfo, &miw, ModuleInfo->SizeOfStruct);

    return TRUE;
}

/******************************************************************
 *		SymGetModuleInfo64 (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfo64(HANDLE hProcess, DWORD64 dwAddr,
                                PIMAGEHLP_MODULE64 ModuleInfo)
{
    IMAGEHLP_MODULE64   mi64;
    IMAGEHLP_MODULEW64  miw64;

    dwAddr = normalize_addr64(dwAddr);
    if (sizeof(mi64) < ModuleInfo->SizeOfStruct)
    {
        SetLastError(ERROR_MOD_NOT_FOUND); /* NOTE: native returns this error */
        WARN("Wrong size %u\n", ModuleInfo->SizeOfStruct);
        return FALSE;
    }

    miw64.SizeOfStruct = sizeof(miw64);
    if (!SymGetModuleInfoW64(hProcess, dwAddr, &miw64)) return FALSE;

    mi64.SizeOfStruct  = miw64.SizeOfStruct;
    mi64.BaseOfImage   = miw64.BaseOfImage;
    mi64.ImageSize     = miw64.ImageSize;
    mi64.TimeDateStamp = miw64.TimeDateStamp;
    mi64.CheckSum      = miw64.CheckSum;
    mi64.NumSyms       = miw64.NumSyms;
    mi64.SymType       = miw64.SymType;
    WideCharToMultiByte(CP_ACP, 0, miw64.ModuleName, -1,
                        mi64.ModuleName, sizeof(mi64.ModuleName), NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, miw64.ImageName, -1,
                        mi64.ImageName, sizeof(mi64.ImageName), NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, miw64.LoadedImageName, -1,
                        mi64.LoadedImageName, sizeof(mi64.LoadedImageName), NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, miw64.LoadedPdbName, -1,
                        mi64.LoadedPdbName, sizeof(mi64.LoadedPdbName), NULL, NULL);

    mi64.CVSig         = miw64.CVSig;
    WideCharToMultiByte(CP_ACP, 0, miw64.CVData, -1,
                        mi64.CVData, sizeof(mi64.CVData), NULL, NULL);
    mi64.PdbSig        = miw64.PdbSig;
    mi64.PdbSig70      = miw64.PdbSig70;
    mi64.PdbAge        = miw64.PdbAge;
    mi64.PdbUnmatched  = miw64.PdbUnmatched;
    mi64.DbgUnmatched  = miw64.DbgUnmatched;
    mi64.LineNumbers   = miw64.LineNumbers;
    mi64.GlobalSymbols = miw64.GlobalSymbols;
    mi64.TypeInfo      = miw64.TypeInfo;
    mi64.SourceIndexed = miw64.SourceIndexed;
    mi64.Publics       = miw64.Publics;

    memcpy(ModuleInfo, &mi64, ModuleInfo->SizeOfStruct);

    return TRUE;
}

/******************************************************************
 *		SymGetModuleInfoW64 (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfoW64(HANDLE hProcess, DWORD64 dwAddr,
                                 PIMAGEHLP_MODULEW64 ModuleInfo)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;
    IMAGEHLP_MODULEW64  miw64;

    TRACE("%p %s %p\n", hProcess, wine_dbgstr_longlong(dwAddr), ModuleInfo);

    dwAddr = normalize_addr64(dwAddr);
    if (!pcs) return FALSE;
    if (ModuleInfo->SizeOfStruct > sizeof(*ModuleInfo)) return FALSE;
    module = module_find_by_addr(pcs, dwAddr, DMT_UNKNOWN);
    if (!module) return FALSE;

    miw64 = module->module;

    /* update debug information from container if any */
    if (module->module.SymType == SymNone)
    {
        module = module_get_container(pcs, module);
        if (module && module->module.SymType != SymNone)
        {
            miw64.SymType = module->module.SymType;
            miw64.NumSyms = module->module.NumSyms;
        }
    }
    memcpy(ModuleInfo, &miw64, ModuleInfo->SizeOfStruct);
    return TRUE;
}

/***********************************************************************
 *		SymGetModuleBase (DBGHELP.@)
 */
DWORD WINAPI SymGetModuleBase(HANDLE hProcess, DWORD dwAddr)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;


    TRACE("(hProcess = %p, dwAddr = 0x%08x)\n", hProcess, dwAddr);

    if (!pcs){
        ERR("no process with the handle %p has been initialized\n", hProcess);
        return 0;
    }


    module = module_find_by_addr(pcs, dwAddr, DMT_UNKNOWN);
    
    if (!module){
        ERR("no module containing the address 0x%08x was found\n", dwAddr);
        return 0;
    }

    TRACE("returning base address of 0x%016llx\n", module->module.BaseOfImage);
    return module->module.BaseOfImage;
}

/***********************************************************************
 *		SymGetModuleBase64 (DBGHELP.@)
 */
DWORD64 WINAPI SymGetModuleBase64(HANDLE hProcess, DWORD64 dwAddr)
{
    TRACE("(hProcess = %p, dwAddr = 0x%016llx)\n",
            hProcess, dwAddr);

    if (!validate_addr64(dwAddr)){
        return 0;
    }

    dwAddr = normalize_addr64(dwAddr);
    return SymGetModuleBase(hProcess, (DWORD)dwAddr);
}

/******************************************************************
 *		module_reset_debug_info
 * Removes any debug information linked to a given module.
 */
void module_reset_debug_info(struct module* module)
{
    module->sortlist_valid = TRUE;
    module->addr_sorttab = NULL;
    hash_table_destroy(&module->ht_symbols);
    module->ht_symbols.num_buckets = 0;
    module->ht_symbols.buckets = NULL;
    hash_table_destroy(&module->ht_types);
    module->ht_types.num_buckets = 0;
    module->ht_types.buckets = NULL;
    module->vtypes.num_elts = 0;
    hash_table_destroy(&module->ht_symbols);
    module->sources_used = module->sources_alloc = 0;
    module->sources = NULL;
}
