/*
 * PE (Portable Execute) File Resources
 *
 * Copyright 1995 Thomas Sandford
 * Copyright 1996 Martin von Loewis
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 *
 * Based on the Win16 resource handling code in loader/resource.c
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Alexandre Julliard
 * Copyright 1997 Marcus Meissner
 */

#include "config.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "wine/unicode.h"
#include "windef.h"
#include "winnls.h"
#include "winerror.h"
#include "wine/module.h"
#include "wine/stackframe.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(resource);


/**********************************************************************
 *  get_module_base
 *
 * Get the base address of a module
 */
static const void *get_module_base( HMODULE hmod )
{
    if (!hmod) hmod = GetModuleHandleA( NULL );
    else if (!HIWORD(hmod))
    {
        FIXME("Enumeration of 16-bit resources is not supported\n");
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    /* clear low order bit in case of LOAD_LIBRARY_AS_DATAFILE module */
    return (void *)((ULONG_PTR)hmod & ~1);
}


/**********************************************************************
 *  is_data_file_module
 *
 * Check if a module handle is for a LOAD_LIBRARY_AS_DATAFILE module.
 */
inline static int is_data_file_module( HMODULE hmod )
{
    return (ULONG_PTR)hmod & 1;
}


/**********************************************************************
 *  get_data_file_ptr
 *
 * Get a pointer to a given offset in a file mapped as data file.
 */
static const void *get_data_file_ptr( const void *base, DWORD offset )
{
    const IMAGE_NT_HEADERS *nt = PE_HEADER(base);
    const IMAGE_SECTION_HEADER *sec = (IMAGE_SECTION_HEADER *)((char *)&nt->OptionalHeader +
                                                               nt->FileHeader.SizeOfOptionalHeader);
    int i;

    /* find the section containing the virtual address */
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
    {
        if ((sec->VirtualAddress <= offset) && (sec->VirtualAddress + sec->SizeOfRawData > offset))
            return (char *)base + sec->PointerToRawData + (offset - sec->VirtualAddress);
    }
    return NULL;
}


/**********************************************************************
 *  get_resdir
 *
 * Get the resource directory of a PE module
 */
static const IMAGE_RESOURCE_DIRECTORY* get_resdir( HMODULE hmod )
{
    const IMAGE_DATA_DIRECTORY *dir;
    const IMAGE_RESOURCE_DIRECTORY *ret = NULL;
    const void *base = get_module_base( hmod );

    if (base)
    {
        dir = &PE_HEADER(base)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
        if (dir->Size && dir->VirtualAddress)
        {
            if (is_data_file_module(hmod)) ret = get_data_file_ptr( base, dir->VirtualAddress );
            else ret = (IMAGE_RESOURCE_DIRECTORY *)((char *)base + dir->VirtualAddress);
        }
    }
    return ret;
}


/**********************************************************************
 *  find_entry_by_id
 *
 * Find an entry by id in a resource directory
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_by_id( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                         WORD id, const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    int min, max, pos;

    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    min = dir->NumberOfNamedEntries;
    max = min + dir->NumberOfIdEntries - 1;
    while (min <= max)
    {
        pos = (min + max) / 2;
        if (entry[pos].u1.s2.Id == id)
            return (IMAGE_RESOURCE_DIRECTORY *)((char *)root + entry[pos].u2.s3.OffsetToDirectory);
        if (entry[pos].u1.s2.Id > id) max = pos - 1;
        else min = pos + 1;
    }
    return NULL;
}


/**********************************************************************
 *  find_entry_by_nameW
 *
 * Find an entry by name in a resource directory
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_by_nameW( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                            LPCWSTR name, const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    const IMAGE_RESOURCE_DIR_STRING_U *str;
    int min, max, res, pos, namelen;

    if (!HIWORD(name)) return find_entry_by_id( dir, LOWORD(name), root );
    if (name[0] == '#')
    {
        char buf[16];
        if (!WideCharToMultiByte( CP_ACP, 0, name+1, -1, buf, sizeof(buf), NULL, NULL ))
            return NULL;
        return find_entry_by_id( dir, atoi(buf), root );
    }

    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    namelen = strlenW(name);
    min = 0;
    max = dir->NumberOfNamedEntries - 1;
    while (min <= max)
    {
        pos = (min + max) / 2;
        str = (IMAGE_RESOURCE_DIR_STRING_U *)((char *)root + entry[pos].u1.s1.NameOffset);
        res = strncmpW( name, str->NameString, str->Length );
        if (!res && namelen == str->Length)
            return (IMAGE_RESOURCE_DIRECTORY *)((char *)root + entry[pos].u2.s3.OffsetToDirectory);
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }
    return NULL;
}


/**********************************************************************
 *  find_entry_by_nameA
 *
 * Find an entry by name in a resource directory
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_by_nameA( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                            LPCSTR name, const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY *ret = NULL;
    LPWSTR nameW;
    INT len;

    if (!HIWORD(name)) return find_entry_by_id( dir, LOWORD(name), root );
    if (name[0] == '#')
    {
        return find_entry_by_id( dir, atoi(name+1), root );
    }

    len = MultiByteToWideChar( CP_ACP, 0, name, -1, NULL, 0 );
    if ((nameW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) )))
    {
        MultiByteToWideChar( CP_ACP, 0, name, -1, nameW, len );
        ret = find_entry_by_nameW( dir, nameW, root );
        HeapFree( GetProcessHeap(), 0, nameW );
    }
    return ret;
}


/**********************************************************************
 *  find_entry_default
 *
 * Find a default entry in a resource directory
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_default( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                           const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;

    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    return (IMAGE_RESOURCE_DIRECTORY *)((char *)root + entry->u2.s3.OffsetToDirectory);
}


/**********************************************************************
 *	    PE_FindResourceExW
 *
 * FindResourceExA/W does search in the following order:
 * 1. Exact specified language
 * 2. Language with neutral sublanguage
 * 3. Neutral language with neutral sublanguage
 * 4. Neutral language with default sublanguage
 */
HRSRC PE_FindResourceExW( HMODULE hmod, LPCWSTR name, LPCWSTR type, WORD lang )
{
    const IMAGE_RESOURCE_DIRECTORY *resdirptr = get_resdir(hmod);
    const void *root;
    HRSRC result;

    if (!resdirptr) return 0;

    root = resdirptr;
    if (!(resdirptr = find_entry_by_nameW(resdirptr, type, root))) return 0;
    if (!(resdirptr = find_entry_by_nameW(resdirptr, name, root))) return 0;

    /* 1. Exact specified language */
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 2. Language with neutral sublanguage */
    lang = MAKELANGID(PRIMARYLANGID(lang), SUBLANG_NEUTRAL);
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 3. Neutral language with neutral sublanguage */
    lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 4. Neutral language with default sublanguage */
    lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    result = (HRSRC)find_entry_by_id( resdirptr, lang, root );

 found:
    return result;
}

/**********************************************************************
 *	    PE_FindResourceW
 *
 * Load[String]/[Icon]/[Menu]/[etc.] does use FindResourceA/W.
 * FindResourceA/W does search in the following order:
 * 1. Neutral language with neutral sublanguage
 * 2. Neutral language with default sublanguage
 * 3. Current locale lang id
 * 4. Current locale lang id with neutral sublanguage
 * 5. (!) LANG_ENGLISH, SUBLANG_DEFAULT
 * 6. Return first in the list
 */
HRSRC PE_FindResourceW( HMODULE hmod, LPCWSTR name, LPCWSTR type )
{
    const IMAGE_RESOURCE_DIRECTORY *resdirptr = get_resdir(hmod);
    const void *root;
    HRSRC result;
    WORD lang;

    if (!resdirptr) return 0;

    root = resdirptr;
    if (!(resdirptr = find_entry_by_nameW(resdirptr, type, root))) return 0;
    if (!(resdirptr = find_entry_by_nameW(resdirptr, name, root))) return 0;

    /* 1. Neutral language with neutral sublanguage */
    lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 2. Neutral language with default sublanguage */
    lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 3. Current locale lang id */
    lang = LANGIDFROMLCID(GetUserDefaultLCID());
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 4. Current locale lang id with neutral sublanguage */
    lang = MAKELANGID(PRIMARYLANGID(lang), SUBLANG_NEUTRAL);
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 5. (!) LANG_ENGLISH, SUBLANG_DEFAULT */
    lang = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT);
    if ((result = (HRSRC)find_entry_by_id( resdirptr, lang, root ))) goto found;

    /* 6. Return first in the list */
    result = (HRSRC)find_entry_default( resdirptr, root );

 found:
    return result;
}


/**********************************************************************
 *	    PE_LoadResource
 */
HGLOBAL PE_LoadResource( HMODULE hmod, HRSRC hRsrc )
{
    DWORD offset;
    const void *base = get_module_base( hmod );

    if (!hRsrc || !base) return 0;

    offset = ((PIMAGE_RESOURCE_DATA_ENTRY)hRsrc)->OffsetToData;

    if (is_data_file_module(hmod))
        return (HANDLE)get_data_file_ptr( base, offset );
    else
        return (HANDLE)((char *)base + offset);
}


/**********************************************************************
 *	    PE_SizeofResource
 */
DWORD PE_SizeofResource( HRSRC hRsrc )
{
    if (!hRsrc) return 0;
    return ((PIMAGE_RESOURCE_DATA_ENTRY)hRsrc)->Size;
}


/**********************************************************************
 *	EnumResourceTypesA	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceTypesA( HMODULE hmod, ENUMRESTYPEPROCA lpfun, LONG lparam)
{
    int		i;
    const IMAGE_RESOURCE_DIRECTORY *resdir = get_resdir(hmod);
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;
    BOOL	ret;

    if (!resdir) return FALSE;

    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resdir + 1);
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
        LPSTR type;

        if (et[i].u1.s1.NameIsString)
        {
            PIMAGE_RESOURCE_DIR_STRING_U pResString = (PIMAGE_RESOURCE_DIR_STRING_U) ((LPBYTE) resdir + et[i].u1.s1.NameOffset);
            DWORD len = WideCharToMultiByte( CP_ACP, 0, pResString->NameString, pResString->Length,
                                             NULL, 0, NULL, NULL);
            if (!(type = HeapAlloc(GetProcessHeap(), 0, len + 1)))
                return FALSE;
            WideCharToMultiByte( CP_ACP, 0, pResString->NameString, pResString->Length,
                                 type, len, NULL, NULL);
            type[len] = '\0';
            ret = lpfun(hmod,type,lparam);
            HeapFree(GetProcessHeap(), 0, type);
        }
        else
        {
            type = (LPSTR)(int)et[i].u1.s2.Id;
            ret = lpfun(hmod,type,lparam);
        }
	if (!ret)
		break;
    }
    return ret;
}


/**********************************************************************
 *	EnumResourceTypesW	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceTypesW( HMODULE hmod, ENUMRESTYPEPROCW lpfun, LONG lparam)
{
    int		i;
    const IMAGE_RESOURCE_DIRECTORY *resdir = get_resdir(hmod);
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;
    BOOL	ret;

    if (!resdir) return FALSE;

    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resdir + 1);
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
	LPWSTR	type;

        if (et[i].u1.s1.NameIsString)
        {
            PIMAGE_RESOURCE_DIR_STRING_U pResString = (PIMAGE_RESOURCE_DIR_STRING_U) ((LPBYTE) resdir + et[i].u1.s1.NameOffset);
            if (!(type = HeapAlloc(GetProcessHeap(), 0, (pResString->Length+1) * sizeof (WCHAR))))
                return FALSE;
            memcpy(type, pResString->NameString, pResString->Length * sizeof (WCHAR));
            type[pResString->Length] = '\0';
            ret = lpfun(hmod,type,lparam);
            HeapFree(GetProcessHeap(), 0, type);
        }
        else
        {
            type = (LPWSTR)(int)et[i].u1.s2.Id;
            ret = lpfun(hmod,type,lparam);
        }
	if (!ret)
		break;
    }
    return ret;
}


/**********************************************************************
 *	EnumResourceNamesA	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceNamesA( HMODULE hmod, LPCSTR type, ENUMRESNAMEPROCA lpfun, LONG lparam )
{
    int		i;
    const IMAGE_RESOURCE_DIRECTORY *basedir = get_resdir(hmod);
    const IMAGE_RESOURCE_DIRECTORY *resdir;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;
    BOOL	ret;

    if (!basedir) return FALSE;

    if (!(resdir = find_entry_by_nameA( basedir, type, basedir ))) return FALSE;

    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resdir + 1);
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
        LPSTR name;

        if (et[i].u1.s1.NameIsString)
        {
            PIMAGE_RESOURCE_DIR_STRING_U pResString = (PIMAGE_RESOURCE_DIR_STRING_U) ((LPBYTE) basedir + et[i].u1.s1.NameOffset);
            DWORD len = WideCharToMultiByte(CP_ACP, 0, pResString->NameString, pResString->Length,
                                            NULL, 0, NULL, NULL);
            if (!(name = HeapAlloc(GetProcessHeap(), 0, len + 1 )))
                return FALSE;
            WideCharToMultiByte( CP_ACP, 0, pResString->NameString, pResString->Length,
                                 name, len, NULL, NULL );
            name[len] = '\0';
            ret = lpfun(hmod,type,name,lparam);
            HeapFree( GetProcessHeap(), 0, name );
        }
        else
        {
            name = (LPSTR)(int)et[i].u1.s2.Id;
            ret = lpfun(hmod,type,name,lparam);
        }
	if (!ret)
		break;
    }
    return ret;
}


/**********************************************************************
 *	EnumResourceNamesW	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceNamesW( HMODULE hmod, LPCWSTR type, ENUMRESNAMEPROCW lpfun, LONG lparam )
{
    int		i;
    const IMAGE_RESOURCE_DIRECTORY *basedir = get_resdir(hmod);
    const IMAGE_RESOURCE_DIRECTORY *resdir;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;
    BOOL	ret;

    if (!basedir) return FALSE;

    if (!(resdir = find_entry_by_nameW( basedir, type, basedir ))) return FALSE;

    et = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resdir + 1);
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
        LPWSTR name;

        if (et[i].u1.s1.NameIsString)
        {
            PIMAGE_RESOURCE_DIR_STRING_U pResString = (PIMAGE_RESOURCE_DIR_STRING_U) ((LPBYTE) basedir + et[i].u1.s1.NameOffset);
            if (!(name = HeapAlloc(GetProcessHeap(), 0, (pResString->Length + 1) * sizeof (WCHAR))))
                return FALSE;
            memcpy(name, pResString->NameString, pResString->Length * sizeof (WCHAR));
            name[pResString->Length] = '\0';
            ret = lpfun(hmod,type,name,lparam);
            HeapFree(GetProcessHeap(), 0, name);
        }
        else
        {
            name = (LPWSTR)(int)et[i].u1.s2.Id;
            ret = lpfun(hmod,type,name,lparam);
        }
	if (!ret)
		break;
    }
    return ret;
}


/**********************************************************************
 *	EnumResourceLanguagesA	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceLanguagesA( HMODULE hmod, LPCSTR type, LPCSTR name,
                                    ENUMRESLANGPROCA lpfun, LONG lparam )
{
    int		i;
    const IMAGE_RESOURCE_DIRECTORY *basedir = get_resdir(hmod);
    const IMAGE_RESOURCE_DIRECTORY *resdir;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;
    BOOL	ret;

    if (!basedir) return FALSE;
    if (!(resdir = find_entry_by_nameA( basedir, type, basedir ))) return FALSE;
    if (!(resdir = find_entry_by_nameA( resdir, name, basedir ))) return FALSE;

    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resdir + 1);
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
        /* languages are just ids... I hope */
	ret = lpfun(hmod,type,name,et[i].u1.s2.Id,lparam);
	if (!ret)
		break;
    }
    return ret;
}


/**********************************************************************
 *	EnumResourceLanguagesW	(KERNEL32.@)
 */
BOOL WINAPI EnumResourceLanguagesW( HMODULE hmod, LPCWSTR type, LPCWSTR name,
                                    ENUMRESLANGPROCW lpfun, LONG lparam )
{
    int		i;
    const IMAGE_RESOURCE_DIRECTORY *basedir = get_resdir(hmod);
    const IMAGE_RESOURCE_DIRECTORY *resdir;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *et;
    BOOL	ret;

    if (!basedir) return FALSE;

    if (!(resdir = find_entry_by_nameW( basedir, type, basedir ))) return FALSE;
    if (!(resdir = find_entry_by_nameW( resdir, name, basedir ))) return FALSE;

    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resdir + 1);
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
	ret = lpfun(hmod,type,name,et[i].u1.s2.Id,lparam);
	if (!ret)
		break;
    }
    return ret;
}
