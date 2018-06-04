/*
 * Resources
 *
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "wine/exception.h"
#include "wine/heapstr.h"
#include "cursoricon.h"
#include "wine/module.h"
#include "wine/file.h"
#include "wine/debug.h"
#include "winerror.h"
#include "winnls.h"
#include "msvcrt/excpt.h"

WINE_DEFAULT_DEBUG_CHANNEL(resource);

#define HRSRC_MAP_BLOCKSIZE 16

typedef struct _HRSRC_ELEM
{
    HANDLE hRsrc;
    WORD     type;
} HRSRC_ELEM;

typedef struct _HRSRC_MAP
{
    int nAlloc;
    int nUsed;
    HRSRC_ELEM *elem;
} HRSRC_MAP;

/**********************************************************************
 *          MapHRsrc32To16
 */
static HRSRC16 MapHRsrc32To16( NE_MODULE *pModule, HANDLE hRsrc32, WORD type )
{
    HRSRC_MAP *map = (HRSRC_MAP *)pModule->hRsrcMap;
    HRSRC_ELEM *newElem;
    int i;

    /* On first call, initialize HRSRC map */
    if ( !map )
    {
        if ( !(map = (HRSRC_MAP *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                             sizeof(HRSRC_MAP) ) ) )
        {
            ERR("Cannot allocate HRSRC map\n" );
            return 0;
        }
        pModule->hRsrcMap = (LPVOID)map;
    }

    /* Check whether HRSRC32 already in map */
    for ( i = 0; i < map->nUsed; i++ )
        if ( map->elem[i].hRsrc == hRsrc32 )
            return (HRSRC16)(i + 1);

    /* If no space left, grow table */
    if ( map->nUsed == map->nAlloc )
    {
        if ( !(newElem = (HRSRC_ELEM *)HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                                    map->elem,
                                                    (map->nAlloc + HRSRC_MAP_BLOCKSIZE)
                                                    * sizeof(HRSRC_ELEM) ) ))
        {
            ERR("Cannot grow HRSRC map\n" );
            return 0;
        }
        map->elem = newElem;
        map->nAlloc += HRSRC_MAP_BLOCKSIZE;
    }

    /* Add HRSRC32 to table */
    map->elem[map->nUsed].hRsrc = hRsrc32;
    map->elem[map->nUsed].type  = type;
    map->nUsed++;

    return (HRSRC16)map->nUsed;
}

/**********************************************************************
 *          MapHRsrc16To32
 */
static HANDLE MapHRsrc16To32( NE_MODULE *pModule, HRSRC16 hRsrc16 )
{
    HRSRC_MAP *map = (HRSRC_MAP *)pModule->hRsrcMap;
    if ( !map || !hRsrc16 || (int)hRsrc16 > map->nUsed ) return 0;

    return map->elem[(int)hRsrc16-1].hRsrc;
}

/**********************************************************************
 *          MapHRsrc16ToType
 */
static WORD MapHRsrc16ToType( NE_MODULE *pModule, HRSRC16 hRsrc16 )
{
    HRSRC_MAP *map = (HRSRC_MAP *)pModule->hRsrcMap;
    if ( !map || !hRsrc16 || (int)hRsrc16 > map->nUsed ) return 0;

    return map->elem[(int)hRsrc16-1].type;
}


/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}


/* Create Unicode upper-cased copy */
static WCHAR *RES_GetUnicodeName (LPCSTR pName, BOOL bUnicode)
{
   LPWSTR pNameW;
   unsigned int i, Len;

   if (!bUnicode)
   {
      pNameW = HEAP_strdupAtoW (GetProcessHeap (), 0, pName);
      if (!pNameW)
      {
         ERR ("Out of memory!\n");
         return NULL;
      }

      Len = strlenW (pNameW);
   }
   else
   {

      Len = strlenW ((LPWSTR)pName);
      pNameW = HeapAlloc (GetProcessHeap (), 0,
                          (Len + 1) * sizeof (WCHAR));
      if (!pNameW)
      {
         ERR ("Out of memory!\n");
         return NULL;
      }

      memcpy (pNameW, pName, (Len + 1) * sizeof (WCHAR));
   }

   for (i = 0; i < Len; i++)
      pNameW[i] = toupperW (pNameW[i]);
   pNameW[Len] = 0;

   return pNameW;
}


static HRSRC RES_FindResource2( HMODULE hModule, LPCSTR type,
				LPCSTR name, WORD lang,
				BOOL bUnicode, BOOL bRet16 )
{
    HRSRC hRsrc = 0;

    TRACE("(%08x, %08x%s, %08x%s, %04x, %s, %s)\n",
	  hModule,
	  (UINT)type, HIWORD(type)? (bUnicode? debugstr_w((LPWSTR)type) : debugstr_a(type)) : "",
	  (UINT)name, HIWORD(name)? (bUnicode? debugstr_w((LPWSTR)name) : debugstr_a(name)) : "",
	  lang,
	  bUnicode? "W"  : "A",
	  bRet16?   "NE" : "PE" );

    if (!HIWORD(hModule))
    {
        HMODULE16 hMod16   = MapHModuleLS( hModule );
        NE_MODULE *pModule = NE_GetPtr( hMod16 );
        if (!pModule) return 0;
        if (!pModule->module32)
        {
	    /* 16-bit NE module */
	    LPSTR typeStr, nameStr;

	    if ( HIWORD( type ) && bUnicode )
		typeStr = HEAP_strdupWtoA( GetProcessHeap(), 0, (LPCWSTR)type );
	    else
		typeStr = (LPSTR)type;
	    if ( HIWORD( name ) && bUnicode )
		nameStr = HEAP_strdupWtoA( GetProcessHeap(), 0, (LPCWSTR)name );
	    else
		nameStr = (LPSTR)name;

	    hRsrc = NE_FindResource( pModule, nameStr, typeStr );

	    if ( HIWORD( type ) && bUnicode )
		HeapFree( GetProcessHeap(), 0, typeStr );
	    if ( HIWORD( name ) && bUnicode )
		HeapFree( GetProcessHeap(), 0, nameStr );

	    /* If we need to return 32-bit HRSRC, no conversion is necessary,
	       we simply use the 16-bit HRSRC as 32-bit HRSRC */
        }
        else
        {
            /* 32-bit PE module */
            hRsrc = RES_FindResource2( pModule->module32, type, name, lang, bUnicode, FALSE );
            /* If we need to return 16-bit HRSRC, perform conversion */
            if ( bRet16 )
                hRsrc = MapHRsrc32To16( pModule, hRsrc,
                                        HIWORD( type )? 0 : LOWORD( type ) );
        }
    }
    else
    {
        /* 32-bit PE module */
        LPWSTR typeStr, nameStr;

        if (HIWORD (type))
        {
           typeStr = RES_GetUnicodeName (type, bUnicode);
           if (!typeStr)
              return 0;
        }
        else
            typeStr = (LPWSTR)type;
        if (HIWORD (name))
        {
           nameStr = RES_GetUnicodeName (name, bUnicode);
           if (!nameStr)
           {
              if (HIWORD (type))
                 HeapFree (GetProcessHeap (), 0, typeStr);
              return 0;
           }
        }
        else
            nameStr = (LPWSTR)name;

	/* Here is the real difference between FindResouce and FindResourceEx */
	if(lang == MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) ||
		lang == MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) ||
		lang == MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT) ||
		lang == MAKELANGID(LANG_NEUTRAL, 3)) /* FIXME: real name? */
	    hRsrc = PE_FindResourceW( hModule, nameStr, typeStr );
	else
	    hRsrc = PE_FindResourceExW( hModule, nameStr, typeStr, lang );

        if (HIWORD (type))
           HeapFree (GetProcessHeap (), 0, typeStr);
        if (HIWORD (name))
           HeapFree (GetProcessHeap (), 0, nameStr);
    }
    return hRsrc;
}

/**********************************************************************
 *          RES_FindResource
 */

static HRSRC RES_FindResource( HMODULE hModule, LPCSTR type,
                               LPCSTR name, WORD lang,
                               BOOL bUnicode, BOOL bRet16 )
{
    HRSRC hRsrc;
    __TRY
    {
	hRsrc = RES_FindResource2(hModule, type, name, lang, bUnicode, bRet16);
    }
    __EXCEPT(page_fault)
    {
	WARN("page fault\n");
	SetLastError(ERROR_INVALID_PARAMETER);
	return 0;
    }
    __ENDTRY
    return hRsrc;
}

/**********************************************************************
 *          RES_SizeofResource
 */
static DWORD RES_SizeofResource( HMODULE hModule, HRSRC hRsrc, BOOL bRet16 )
{
    if (!hRsrc) return 0;

    TRACE("(%08x, %08x, %s)\n", hModule, hRsrc, bRet16? "NE" : "PE" );

    if (!HIWORD(hModule))
    {
        HMODULE16 hMod16   = MapHModuleLS( hModule );
        NE_MODULE *pModule = NE_GetPtr( hMod16 );
        if (!pModule) return 0;

        if (!pModule->module32)  /* 16-bit NE module */
        {
            /* If we got a 32-bit hRsrc, we don't need to convert it */
            return NE_SizeofResource( pModule, hRsrc );
        }

        /* If we got a 16-bit hRsrc, convert it */
        if (!HIWORD(hRsrc)) hRsrc = MapHRsrc16To32( pModule, hRsrc );
    }

    /* 32-bit PE module */
    return PE_SizeofResource( hRsrc );
}

/**********************************************************************
 *          RES_LoadResource
 */
static HGLOBAL RES_LoadResource( HMODULE hModule, HRSRC hRsrc, BOOL bRet16 )
{
    HGLOBAL hMem = 0;

    TRACE("(%08x, %08x, %s)\n", hModule, hRsrc, bRet16? "NE" : "PE" );

    if (!hRsrc) return 0;

    if (!HIWORD(hModule))
    {
        HMODULE16 hMod16   = MapHModuleLS( hModule );
        NE_MODULE *pModule = NE_GetPtr( hMod16 );
        if (!pModule) return 0;
        if (!pModule->module32)
        {
            /* 16-bit NE module */

            /* If we got a 32-bit hRsrc, we don't need to convert it */
            hMem = NE_LoadResource( pModule, hRsrc );

            /* If we are to return a 32-bit resource, we should probably
               convert it but we don't for now.  FIXME !!! */
            return hMem;
        }
        else
        {
            /* If we got a 16-bit hRsrc, convert it */
            HRSRC hRsrc32 = HIWORD(hRsrc)? hRsrc : MapHRsrc16To32( pModule, hRsrc );

            hMem = PE_LoadResource( pModule->module32, hRsrc32 );

            /* If we need to return a 16-bit resource, convert it */
            if ( bRet16 )
            {
                WORD type   = MapHRsrc16ToType( pModule, hRsrc );
                DWORD size  = SizeofResource( hModule, hRsrc );
                LPVOID bits = LockResource( hMem );

                hMem = NE_LoadPEResource( pModule, type, bits, size );
            }
        }
    }
    else
    {
        /* 32-bit PE module */
        hMem = PE_LoadResource( hModule, hRsrc );
    }

    return hMem;
}

/**********************************************************************
 *          FindResource     (KERNEL.60)
 *          FindResource16   (KERNEL32.@)
 */
HRSRC16 WINAPI FindResource16( HMODULE16 hModule, LPCSTR name, LPCSTR type )
{
    return RES_FindResource( hModule, type, name,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), FALSE, TRUE );
}

/**********************************************************************
 *	    FindResourceA    (KERNEL32.@)
 */
HRSRC WINAPI FindResourceA( HMODULE hModule, LPCSTR name, LPCSTR type )
{
    return RES_FindResource( hModule, type, name,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), FALSE, FALSE );
}

/**********************************************************************
 *	    FindResourceExA  (KERNEL32.@)
 */
HRSRC WINAPI FindResourceExA( HMODULE hModule,
                               LPCSTR type, LPCSTR name, WORD lang )
{
    return RES_FindResource( hModule, type, name,
                             lang, FALSE, FALSE );
}

/**********************************************************************
 *	    FindResourceExW  (KERNEL32.@)
 */
HRSRC WINAPI FindResourceExW( HMODULE hModule,
                              LPCWSTR type, LPCWSTR name, WORD lang )
{
    return RES_FindResource( hModule, (LPCSTR)type, (LPCSTR)name,
                             lang, TRUE, FALSE );
}

/**********************************************************************
 *	    FindResourceW    (KERNEL32.@)
 */
HRSRC WINAPI FindResourceW(HINSTANCE hModule, LPCWSTR name, LPCWSTR type)
{
    return RES_FindResource( hModule, (LPCSTR)type, (LPCSTR)name,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), TRUE, FALSE );
}

/**********************************************************************
 *          LoadResource     (KERNEL.61)
 *          LoadResource16   (KERNEL32.@)
 */
HGLOBAL16 WINAPI LoadResource16( HMODULE16 hModule, HRSRC16 hRsrc )
{
    return RES_LoadResource( hModule, hRsrc, TRUE );
}

/**********************************************************************
 *	    LoadResource     (KERNEL32.@)
 */
HGLOBAL WINAPI LoadResource( HINSTANCE hModule, HRSRC hRsrc )
{
    return RES_LoadResource( hModule, hRsrc, FALSE );
}

/**********************************************************************
 *          LockResource   (KERNEL.62)
 */
SEGPTR WINAPI WIN16_LockResource16( HGLOBAL16 handle )
{
    TRACE("(%04x)\n", handle );
    /* May need to reload the resource if discarded */
    return K32WOWGlobalLock16( handle );
}

/**********************************************************************
 *          LockResource16 (KERNEL32.@)
 */
LPVOID WINAPI LockResource16( HGLOBAL16 handle )
{
    return MapSL( WIN16_LockResource16(handle) );
}

/**********************************************************************
 *	    LockResource     (KERNEL32.@)
 */
LPVOID WINAPI LockResource( HGLOBAL handle )
{
    TRACE("(%08x)\n", handle );

    if (HIWORD( handle ))  /* 32-bit memory handle */
        return (LPVOID)handle;

    /* 16-bit memory handle */
    return LockResource16( handle );
}

typedef WORD (WINAPI *pDestroyIcon32Proc)( HGLOBAL16 handle, UINT16 flags );


/**********************************************************************
 *          FreeResource     (KERNEL.63)
 *          FreeResource16   (KERNEL32.@)
 */
BOOL16 WINAPI FreeResource16( HGLOBAL16 handle )
{
    HGLOBAL retv = handle;
    NE_MODULE *pModule = NE_GetPtr( FarGetOwner16( handle ) );

    TRACE("(%04x)\n", handle );

    /* Try NE resource first */
    retv = NE_FreeResource( pModule, handle );

    /* If this failed, call USER.DestroyIcon32; this will check
       whether it is a shared cursor/icon; if not it will call
       GlobalFree16() */
    if ( retv )
    {
        pDestroyIcon32Proc proc;
        HMODULE user = GetModuleHandleA( "user32.dll" );

        if (user && (proc = (pDestroyIcon32Proc)GetProcAddress( user, "DestroyIcon32" )))
            retv = proc( handle, CID_RESOURCE );
        else
            retv = GlobalFree16( handle );
    }
    return (BOOL)retv;
}

/**********************************************************************
 *	    FreeResource     (KERNEL32.@)
 */
BOOL WINAPI FreeResource( HGLOBAL handle )
{
    if (HIWORD(handle)) return 0; /* 32-bit memory handle: nothing to do */

    return FreeResource16( handle );
}

/**********************************************************************
 *          SizeofResource   (KERNEL.65)
 *          SizeofResource16 (KERNEL32.@)
 */
DWORD WINAPI SizeofResource16( HMODULE16 hModule, HRSRC16 hRsrc )
{
    return RES_SizeofResource( hModule, hRsrc, TRUE );
}

/**********************************************************************
 *	    SizeofResource   (KERNEL32.@)
 */
DWORD WINAPI SizeofResource( HINSTANCE hModule, HRSRC hRsrc )
{
    return RES_SizeofResource( hModule, hRsrc, FALSE );
}
