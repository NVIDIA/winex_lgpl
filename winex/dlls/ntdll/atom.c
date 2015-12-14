/*
 * Atom table functions
 *
 * Copyright 1993, 1994, 1995 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

/*
 * Warning: The code assumes that LocalAlloc() returns a block aligned
 * on a 4-bytes boundary (because of the shifting done in
 * HANDLETOATOM).  If this is not the case, the allocation code will
 * have to be changed.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"

#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/winbase16.h"
#include "global.h"
#include "instance.h"
#include "stackframe.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(atom);

#define DEFAULT_ATOMTABLE_SIZE    37
#define MAX_ATOM_LEN              255

#define ATOMTOHANDLE(atom)        ((HANDLE16)(atom) << 2)
#define HANDLETOATOM(handle)      ((ATOM)(0xc000 | ((handle) >> 2)))

typedef struct
{
    HANDLE16    next;
    WORD        refCount;
    BYTE        length;
    char        str[1];
} ATOMENTRY;

typedef struct
{
    WORD        size;
    HANDLE16    entries[1];
} ATOMTABLE;

static WORD ATOM_UserDS = 0;  /* USER data segment */

/***********************************************************************
 *           ATOM_Init
 *
 * Global table initialisation.
 */
BOOL ATOM_Init( WORD globalTableSel )
{
    ATOM_UserDS = globalTableSel;
    return TRUE;
}


/***********************************************************************
 *           ATOM_GetTable
 *
 * Return a pointer to the atom table of a given segment, creating
 * it if necessary.
 *
 * RETURNS
 *	Pointer to table: Success
 *	NULL: Failure
 */
static ATOMTABLE *ATOM_GetTable( BOOL create  /* [in] Create */ )
{
    INSTANCEDATA *ptr = MapSL( MAKESEGPTR( CURRENT_DS, 0 ) );
    if (ptr->atomtable)
    {
        ATOMTABLE *table = (ATOMTABLE *)((char *)ptr + ptr->atomtable);
        if (table->size) return table;
    }
    if (!create) return NULL;
    if (!InitAtomTable16( 0 )) return NULL;
    /* Reload ptr in case it moved in linear memory */
    ptr = MapSL( MAKESEGPTR( CURRENT_DS, 0 ) );
    return (ATOMTABLE *)((char *)ptr + ptr->atomtable);
}


/***********************************************************************
 *           ATOM_Hash
 * RETURNS
 *	The hash value for the input string
 */
static WORD ATOM_Hash(
            WORD entries, /* [in] Total number of entries */
            LPCSTR str,   /* [in] Pointer to string to hash */
            WORD len      /* [in] Length of string */
) {
    WORD i, hash = 0;

    TRACE("%x, %s, %x\n", entries, str, len);

    for (i = 0; i < len; i++) hash ^= toupper(str[i]) + i;
    return hash % entries;
}


/***********************************************************************
 *           ATOM_IsIntAtomA
 */
static BOOL ATOM_IsIntAtomA(LPCSTR atomstr,WORD *atomid)
{
    UINT atom = 0;
    if (!HIWORD(atomstr)) atom = LOWORD(atomstr);
    else
    {
        if (*atomstr++ != '#') return FALSE;
        while (*atomstr >= '0' && *atomstr <= '9')
        {
            atom = atom * 10 + *atomstr - '0';
            atomstr++;
        }
        if (*atomstr) return FALSE;
    }
    if (!atom || (atom >= MAXINTATOM))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        atom = 0;
    }
    *atomid = atom;
    return TRUE;
}


/***********************************************************************
 *           ATOM_IsIntAtomW
 */
static BOOL ATOM_IsIntAtomW(LPCWSTR atomstr,WORD *atomid)
{
    UINT atom = 0;
    if (!HIWORD(atomstr)) atom = LOWORD(atomstr);
    else
    {
        if (*atomstr++ != '#') return FALSE;
        while (*atomstr >= '0' && *atomstr <= '9')
        {
            atom = atom * 10 + *atomstr - '0';
            atomstr++;
        }
        if (*atomstr) return FALSE;
    }
    if (!atom || (atom >= MAXINTATOM))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        atom = 0;
    }
    *atomid = atom;
    return TRUE;
}


/***********************************************************************
 *           ATOM_MakePtr
 *
 * Make an ATOMENTRY pointer from a handle (obtained from GetAtomHandle()).
 */
static inline ATOMENTRY *ATOM_MakePtr( HANDLE16 handle /* [in] Handle */ )
{
    return MapSL( MAKESEGPTR( CURRENT_DS, handle ) );
}


/***********************************************************************
 *           InitAtomTable   (KERNEL.68)
 */
WORD WINAPI InitAtomTable16( WORD entries )
{
    int i;
    HANDLE16 handle;
    ATOMTABLE *table;

      /* We consider the first table to be initialized as the global table.
       * This works, as USER (both built-in and native) is the first one to
       * register ...
       */

    if (!ATOM_UserDS)
    {
        ATOM_UserDS = CURRENT_DS;
        /* return dummy local handle */
        return LocalAlloc16( LMEM_FIXED, 1 );
    }

      /* Allocate the table */

    if (!entries) entries = DEFAULT_ATOMTABLE_SIZE;  /* sanity check */
    handle = LocalAlloc16( LMEM_FIXED, sizeof(ATOMTABLE) + (entries-1) * sizeof(HANDLE16) );
    if (!handle) return 0;
    table = MapSL( MAKESEGPTR( CURRENT_DS, handle ) );
    table->size = entries;
    for (i = 0; i < entries; i++) table->entries[i] = 0;

      /* Store a pointer to the table in the instance data */

    ((INSTANCEDATA *)MapSL( MAKESEGPTR( CURRENT_DS, 0 )))->atomtable = handle;
    return handle;
}

/***********************************************************************
 *           GetAtomHandle   (KERNEL.73)
 */
HANDLE16 WINAPI GetAtomHandle16( ATOM atom )
{
    if (atom < MAXINTATOM) return 0;
    return ATOMTOHANDLE( atom );
}


/***********************************************************************
 *           AddAtom   (KERNEL.70)
 *
 * Windows DWORD aligns the atom entry size.
 * The remaining unused string space created by the alignment
 * gets padded with '\0's in a certain way to ensure
 * that at least one trailing '\0' remains.
 *
 * RETURNS
 *	Atom: Success
 *	0: Failure
 */
ATOM WINAPI AddAtom16( LPCSTR str )
{
    char buffer[MAX_ATOM_LEN+1];
    WORD hash;
    HANDLE16 entry;
    ATOMENTRY * entryPtr;
    ATOMTABLE * table;
    int len, ae_len;
    WORD	iatom;

    if (ATOM_IsIntAtomA( str, &iatom )) return iatom;

    TRACE("%s\n",debugstr_a(buffer));

    /* Make a copy of the string to be sure it doesn't move in linear memory. */
    lstrcpynA( buffer, str, sizeof(buffer) );

    len = strlen( buffer );
    if (!(table = ATOM_GetTable( TRUE ))) return 0;
    if (CURRENT_DS == ATOM_UserDS) return GlobalAddAtomA( str );

    hash = ATOM_Hash( table->size, buffer, len );
    entry = table->entries[hash];
    while (entry)
    {
	entryPtr = ATOM_MakePtr( entry );
	if ((entryPtr->length == len) &&
	    (!strncasecmp( entryPtr->str, buffer, len )))
	{
	    entryPtr->refCount++;
            TRACE("-- existing 0x%x\n", entry);
	    return HANDLETOATOM( entry );
	}
	entry = entryPtr->next;
    }

    ae_len = (sizeof(ATOMENTRY)+len+3) & ~3;
    entry = LocalAlloc16( LMEM_FIXED, ae_len );
    if (!entry) return 0;
    /* Reload the table ptr in case it moved in linear memory */
    table = ATOM_GetTable( FALSE );
    entryPtr = ATOM_MakePtr( entry );
    entryPtr->next = table->entries[hash];
    entryPtr->refCount = 1;
    entryPtr->length = len;
    /* Some applications _need_ the '\0' padding provided by this strncpy */
    strncpy( entryPtr->str, buffer, ae_len - sizeof(ATOMENTRY) + 1 );
    entryPtr->str[ae_len - sizeof(ATOMENTRY)] = '\0';
    table->entries[hash] = entry;
    TRACE("-- new 0x%x\n", entry);
    return HANDLETOATOM( entry );
}


/***********************************************************************
 *           DeleteAtom   (KERNEL.71)
 */
ATOM WINAPI DeleteAtom16( ATOM atom )
{
    ATOMENTRY * entryPtr;
    ATOMTABLE * table;
    HANDLE16 entry, *prevEntry;
    WORD hash;

    if (atom < MAXINTATOM) return 0;  /* Integer atom */
    if (CURRENT_DS == ATOM_UserDS) return GlobalDeleteAtom( atom );

    TRACE("0x%x\n",atom);

    if (!(table = ATOM_GetTable( FALSE ))) return 0;
    entry = ATOMTOHANDLE( atom );
    entryPtr = ATOM_MakePtr( entry );

    /* Find previous atom */
    hash = ATOM_Hash( table->size, entryPtr->str, entryPtr->length );
    prevEntry = &table->entries[hash];
    while (*prevEntry && *prevEntry != entry)
    {
	ATOMENTRY * prevEntryPtr = ATOM_MakePtr( *prevEntry );
	prevEntry = &prevEntryPtr->next;
    }
    if (!*prevEntry) return atom;

    /* Delete atom */
    if (--entryPtr->refCount == 0)
    {
	*prevEntry = entryPtr->next;
        LocalFree16( entry );
    }
    return 0;
}


/***********************************************************************
 *           FindAtom   (KERNEL.69)
 */
ATOM WINAPI FindAtom16( LPCSTR str )
{
    ATOMTABLE * table;
    WORD hash,iatom;
    HANDLE16 entry;
    int len;

    if (CURRENT_DS == ATOM_UserDS) return GlobalFindAtomA( str );

    TRACE("%s\n",debugres_a(str));

    if (ATOM_IsIntAtomA( str, &iatom )) return iatom;
    if ((len = strlen( str )) > 255) len = 255;
    if (!(table = ATOM_GetTable( FALSE ))) return 0;
    hash = ATOM_Hash( table->size, str, len );
    entry = table->entries[hash];
    while (entry)
    {
	ATOMENTRY * entryPtr = ATOM_MakePtr( entry );
	if ((entryPtr->length == len) &&
	    (!strncasecmp( entryPtr->str, str, len )))
        {
            TRACE("-- found %x\n", entry);
	    return HANDLETOATOM( entry );
        }
	entry = entryPtr->next;
    }
    TRACE("-- not found\n");
    return 0;
}


/***********************************************************************
 *           GetAtomName   (KERNEL.72)
 */
UINT16 WINAPI GetAtomName16( ATOM atom, LPSTR buffer, INT16 count )
{
    ATOMTABLE * table;
    ATOMENTRY * entryPtr;
    HANDLE16 entry;
    char * strPtr;
    UINT len;
    char text[8];

    if (CURRENT_DS == ATOM_UserDS) return GlobalGetAtomNameA( atom, buffer, count );

    TRACE("%x\n",atom);

    if (!count) return 0;
    if (atom < MAXINTATOM)
    {
	sprintf( text, "#%d", atom );
	len = strlen(text);
	strPtr = text;
    }
    else
    {
       if (!(table = ATOM_GetTable( FALSE ))) return 0;
	entry = ATOMTOHANDLE( atom );
	entryPtr = ATOM_MakePtr( entry );
	len = entryPtr->length;
	strPtr = entryPtr->str;
    }
    if (len >= count) len = count-1;
    memcpy( buffer, strPtr, len );
    buffer[len] = '\0';
    return len;
}

/***********************************************************************
 *           InitAtomTable   (KERNEL32.@)
 */
BOOL WINAPI ATOM_InitAtomTable( DWORD entries )
{
    BOOL ret;
    SERVER_START_REQ( init_atom_table )
    {
        req->entries = entries;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


static ATOM ATOM_AddAtomA( LPCSTR str, BOOL local )
{
    ATOM atom = 0;
    if (!ATOM_IsIntAtomA( str, &atom ))
    {
        WCHAR buffer[MAX_ATOM_LEN];

        DWORD len = MultiByteToWideChar( CP_ACP, 0, str, strlen(str), buffer, MAX_ATOM_LEN );
        if (!len)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        SERVER_START_REQ( add_atom )
        {
            req->local = local;
            wine_server_add_data( req, buffer, len * sizeof(WCHAR) );
            if (!wine_server_call_err(req)) atom = reply->atom;
        }
        SERVER_END_REQ;
    }
    TRACE( "(%s) %s -> %x\n", local ? "local" : "global", debugres_a(str), atom );
    return atom;
}


/***********************************************************************
 *           GlobalAddAtomA   (KERNEL32.@)
 *
 * Adds a character string to the global atom table and returns a unique
 * value identifying the string.
 *
 * RETURNS
 *	Atom: Success
 *	0: Failure
 */
ATOM WINAPI GlobalAddAtomA( LPCSTR str /* [in] Pointer to string to add */ )
{
    return ATOM_AddAtomA( str, FALSE );
}


/***********************************************************************
 *           AddAtomA   (KERNEL32.@)
 * Adds a string to the atom table and returns the atom identifying the
 * string.
 *
 * RETURNS
 *	Atom: Success
 *	0: Failure
 */
ATOM WINAPI AddAtomA( LPCSTR str /* [in] Pointer to string to add */ )
{
    return ATOM_AddAtomA( str, TRUE );
}


static ATOM ATOM_AddAtomW( LPCWSTR str, BOOL local )
{
    ATOM atom = 0;
    if (!ATOM_IsIntAtomW( str, &atom ))
    {
        DWORD len = strlenW(str);
        if (len > MAX_ATOM_LEN)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        SERVER_START_REQ( add_atom )
        {
            req->local = local;
            wine_server_add_data( req, str, len * sizeof(WCHAR) );
            if (!wine_server_call_err(req)) atom = reply->atom;
        }
        SERVER_END_REQ;
    }
    TRACE( "(%s) %s -> %x\n", local ? "local" : "global", debugres_w(str), atom );
    return atom;
}


/***********************************************************************
 *           GlobalAddAtomW   (KERNEL32.@)
 */
ATOM WINAPI GlobalAddAtomW( LPCWSTR str )
{
    return ATOM_AddAtomW( str, FALSE );
}


/***********************************************************************
 *           AddAtomW   (KERNEL32.@)
 */
ATOM WINAPI AddAtomW( LPCWSTR str )
{
    return ATOM_AddAtomW( str, TRUE );
}


static ATOM ATOM_DeleteAtom( ATOM atom,  BOOL local)
{
    TRACE( "(%s) %x\n", local ? "local" : "global", atom );
    if (atom < MAXINTATOM) atom = 0;
    else
    {
        SERVER_START_REQ( delete_atom )
        {
            req->atom = atom;
            req->local = local;
            if (!wine_server_call_err( req )) atom = 0;
        }
        SERVER_END_REQ;
    }
    return atom;
}


/***********************************************************************
 *           GlobalDeleteAtom   (KERNEL32.@)
 * Decrements the reference count of a string atom.  If the count is
 * zero, the string associated with the atom is removed from the table.
 *
 * RETURNS
 *	0: Success
 *	Atom: Failure
 */
ATOM WINAPI GlobalDeleteAtom( ATOM atom /* [in] Atom to delete */ )
{
    return ATOM_DeleteAtom( atom, FALSE);
}


/***********************************************************************
 *           DeleteAtom   (KERNEL32.@)
 * Decrements the reference count of a string atom.  If count becomes
 * zero, the string associated with the atom is removed from the table.
 *
 * RETURNS
 *	0: Success
 *	Atom: Failure
 */
ATOM WINAPI DeleteAtom( ATOM atom /* [in] Atom to delete */ )
{
    return ATOM_DeleteAtom( atom, TRUE );
}


static ATOM ATOM_FindAtomA( LPCSTR str, BOOL local )
{
    ATOM atom = 0;
    if (!ATOM_IsIntAtomA( str, &atom ))
    {
        WCHAR buffer[MAX_ATOM_LEN];

        DWORD len = MultiByteToWideChar( CP_ACP, 0, str, strlen(str), buffer, MAX_ATOM_LEN );
        if (!len)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        SERVER_START_REQ( find_atom )
        {
            req->local = local;
            wine_server_add_data( req, buffer, len * sizeof(WCHAR) );
            if (!wine_server_call_err(req)) atom = reply->atom;
        }
        SERVER_END_REQ;
    }
    TRACE( "(%s) %s -> %x\n", local ? "local" : "global", debugres_a(str), atom );
    return atom;
}


/***********************************************************************
 *           GlobalFindAtomA   (KERNEL32.@)
 *
 * Searches the atom table for the string and returns the atom
 * associated with it.
 *
 * RETURNS
 *	Atom: Success
 *	0: Failure
 */
ATOM WINAPI GlobalFindAtomA( LPCSTR str /* [in] Pointer to string to search for */ )
{
    return ATOM_FindAtomA( str, FALSE );
}

/***********************************************************************
 *           FindAtomA   (KERNEL32.@)
 * Searches the local atom table for the string and returns the atom
 * associated with that string.
 *
 * RETURNS
 *	Atom: Success
 *	0: Failure
 */
ATOM WINAPI FindAtomA( LPCSTR str /* [in] Pointer to string to find */ )
{
    return ATOM_FindAtomA( str, TRUE );
}


static ATOM ATOM_FindAtomW( LPCWSTR str, BOOL local )
{
    ATOM atom = 0;
    if (!ATOM_IsIntAtomW( str, &atom ))
    {
        DWORD len = strlenW(str);
        if (len > MAX_ATOM_LEN)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        SERVER_START_REQ( find_atom )
        {
            req->local = local;
            wine_server_add_data( req, str, len * sizeof(WCHAR) );
            if (!wine_server_call_err( req )) atom = reply->atom;
        }
        SERVER_END_REQ;
    }
    TRACE( "(%s) %s -> %x\n", local ? "local" : "global", debugres_w(str), atom );
    return atom;
}


/***********************************************************************
 *           GlobalFindAtomW   (KERNEL32.@)
 */
ATOM WINAPI GlobalFindAtomW( LPCWSTR str )
{
    return ATOM_FindAtomW( str, FALSE );
}


/***********************************************************************
 *           FindAtomW   (KERNEL32.@)
 */
ATOM WINAPI FindAtomW( LPCWSTR str )
{
    return ATOM_FindAtomW( str, TRUE );
}


static UINT ATOM_GetAtomNameA( ATOM atom, LPSTR buffer, INT count, BOOL local )
{
    INT len;

    if (count <= 0)
    {
        SetLastError( ERROR_MORE_DATA );
        return 0;
    }
    if (atom < MAXINTATOM)
    {
        char name[8];
        if (!atom)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        len = sprintf( name, "#%d", atom );
        lstrcpynA( buffer, name, count );
    }
    else
    {
        WCHAR full_name[MAX_ATOM_LEN];

        len = 0;
        SERVER_START_REQ( get_atom_name )
        {
            req->atom = atom;
            req->local = local;
            wine_server_set_reply( req, full_name, sizeof(full_name) );
            if (!wine_server_call_err( req ))
            {
                len = WideCharToMultiByte( CP_ACP, 0, full_name,
                                           wine_server_reply_size(reply) / sizeof(WCHAR),
                                           buffer, count - 1, NULL, NULL );
                if (!len) len = count; /* overflow */
                else buffer[len] = 0;
            }
        }
        SERVER_END_REQ;
    }

    if (len && count <= len)
    {
        SetLastError( ERROR_MORE_DATA );
        buffer[count-1] = 0;
        return 0;
    }
    TRACE( "(%s) %x -> %s\n", local ? "local" : "global", atom, debugstr_a(buffer) );
    return len;
}


/***********************************************************************
 *           GlobalGetAtomNameA   (KERNEL32.@)
 *
 * Retrieves a copy of the string associated with an atom.
 *
 * RETURNS
 *	Length of string in characters: Success
 *	0: Failure
 */
UINT WINAPI GlobalGetAtomNameA(
              ATOM atom,    /* [in]  Atom identifier */
              LPSTR buffer, /* [out] Pointer to buffer for atom string */
              INT count )   /* [in]  Size of buffer */
{
    return ATOM_GetAtomNameA( atom, buffer, count, FALSE );
}


/***********************************************************************
 *           GetAtomNameA   (KERNEL32.@)
 * Retrieves a copy of the string associated with the atom.
 *
 * RETURNS
 *	Length of string: Success
 *	0: Failure
 */
UINT WINAPI GetAtomNameA(
              ATOM atom,    /* [in]  Atom */
              LPSTR buffer, /* [out] Pointer to string for atom string */
              INT count)    /* [in]  Size of buffer */
{
    return ATOM_GetAtomNameA( atom, buffer, count, TRUE );
}


static UINT ATOM_GetAtomNameW( ATOM atom, LPWSTR buffer, INT count, BOOL local )
{
    INT len;

    if (count <= 0)
    {
        SetLastError( ERROR_MORE_DATA );
        return 0;
    }
    if (atom < MAXINTATOM)
    {
        char name[8];
        if (!atom)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        sprintf( name, "#%d", atom );
        len = MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, count );
        if (!len) buffer[count-1] = 0;  /* overflow */
    }
    else
    {
        WCHAR full_name[MAX_ATOM_LEN];

        len = 0;
        SERVER_START_REQ( get_atom_name )
        {
            req->atom = atom;
            req->local = local;
            wine_server_set_reply( req, full_name, sizeof(full_name) );
            if (!wine_server_call_err( req ))
            {
                len = wine_server_reply_size(reply) / sizeof(WCHAR);
                if (count > len) count = len + 1;
                memcpy( buffer, full_name, (count-1) * sizeof(WCHAR) );
                buffer[count-1] = 0;
            }
        }
        SERVER_END_REQ;
        if (!len) return 0;
    }
    if (count <= len)
    {
        SetLastError( ERROR_MORE_DATA );
        return 0;
    }
    TRACE( "(%s) %x -> %s\n", local ? "local" : "global", atom, debugstr_w(buffer) );
    return len;
}


/***********************************************************************
 *           GlobalGetAtomNameW   (KERNEL32.@)
 */
UINT WINAPI GlobalGetAtomNameW( ATOM atom, LPWSTR buffer, INT count )
{
    return ATOM_GetAtomNameW( atom, buffer, count, FALSE);
}


/***********************************************************************
 *           GetAtomNameW   (KERNEL32.@)
 */
UINT WINAPI GetAtomNameW( ATOM atom, LPWSTR buffer, INT count )
{
    return ATOM_GetAtomNameW( atom, buffer, count, TRUE );
}
