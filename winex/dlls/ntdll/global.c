/*
 * Global heap functions
 *
 * Copyright 1995 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
/* 0xffff sometimes seems to mean: CURRENT_DS */

#include "config.h"
#include "wine/port.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/host_info.h>
#include <mach/vm_types.h>
#endif

#include "wine/winbase16.h"
#include "wine/exception.h"
#include "global.h"
#include "toolhelp.h"
#include "selectors.h"
#include "miscemu.h"
#include "stackframe.h"
#include "wine/module.h"
#include "wine/debug.h"
#include "winerror.h"
#include "msvcrt/excpt.h"

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(global);

#define LOW_LONG(ll) ((ULONG)((ll) & 0xffffffffL))
#define HI_LONG(ll)  ((ULONG)((ll) >> 32))

 /* Global arena block */
typedef struct
{
    DWORD     base;          /* Base address (0 if discarded) */
    DWORD     size;          /* Size in bytes (0 indicates a free block) */
    HGLOBAL16 handle;        /* Handle for this block */
    HGLOBAL16 hOwner;        /* Owner of this block */
    BYTE      lockCount;     /* Count of GlobalFix() calls */
    BYTE      pageLockCount; /* Count of GlobalPageLock() calls */
    BYTE      flags;         /* Allocation flags */
    BYTE      selCount;      /* Number of selectors allocated for this block */
} GLOBALARENA;

  /* Flags definitions */
#define GA_MOVEABLE     0x02  /* same as GMEM_MOVEABLE */
#define GA_DGROUP       0x04
#define GA_DISCARDABLE  0x08
#define GA_IPCSHARE     0x10  /* same as GMEM_DDESHARE */
#define GA_DOSMEM       0x20

  /* Arena array */
static GLOBALARENA *pGlobalArena = NULL;
static int globalArenaSize = 0;

#define GLOBAL_MAX_ALLOC_SIZE 0x00ff0000  /* Largest allocation is 16M - 64K */

#define VALID_HANDLE(handle) (((handle)>>__AHSHIFT)<globalArenaSize)
#define GET_ARENA_PTR(handle)  (pGlobalArena + ((handle) >> __AHSHIFT))

/* filter for page-fault exceptions */
/* It is possible for a bogus global pointer to cause a */
/* page zero reference, so I include EXCEPTION_PRIV_INSTRUCTION too. */

static WINE_EXCEPTION_FILTER(page_fault)
{
    switch (GetExceptionCode()) {
        case (EXCEPTION_ACCESS_VIOLATION):
        case (EXCEPTION_PRIV_INSTRUCTION):
           return EXCEPTION_EXECUTE_HANDLER;
        default:
           return EXCEPTION_CONTINUE_SEARCH;
    }
}

/***********************************************************************
 *           GLOBAL_GetArena
 *
 * Return the arena for a given selector, growing the arena array if needed.
 */
static GLOBALARENA *GLOBAL_GetArena( WORD sel, WORD selcount )
{
    if (((sel >> __AHSHIFT) + selcount) > globalArenaSize)
    {
        int newsize = ((sel >> __AHSHIFT) + selcount + 0xff) & ~0xff;
        GLOBALARENA *pNewArena = realloc( pGlobalArena,
                                          newsize * sizeof(GLOBALARENA) );
        if (!pNewArena) return 0;
        pGlobalArena = pNewArena;
        memset( pGlobalArena + globalArenaSize, 0,
                (newsize - globalArenaSize) * sizeof(GLOBALARENA) );
        globalArenaSize = newsize;
    }
    return pGlobalArena + (sel >> __AHSHIFT);
}

void debug_handles(void)
{
    int printed=0;
    int i;
    for (i = globalArenaSize-1 ; i>=0 ; i--) {
	if (pGlobalArena[i].size!=0 && (pGlobalArena[i].handle & 0x8000)){
	    printed=1;
	    DPRINTF("0x%08x, ",pGlobalArena[i].handle);
	}
    }
    if (printed)
	DPRINTF("\n");
}


/***********************************************************************
 *           GLOBAL_CreateBlock
 *
 * Create a global heap block for a fixed range of linear memory.
 */
HGLOBAL16 GLOBAL_CreateBlock( WORD flags, const void *ptr, DWORD size,
                              HGLOBAL16 hOwner, unsigned char selflags )
{
    WORD sel, selcount;
    GLOBALARENA *pArena;

      /* Allocate the selector(s) */

    sel = SELECTOR_AllocBlock( ptr, size, selflags );
    if (!sel) return 0;
    selcount = (size + 0xffff) / 0x10000;

    if (!(pArena = GLOBAL_GetArena( sel, selcount )))
    {
        SELECTOR_FreeBlock( sel );
        return 0;
    }

      /* Fill the arena block */

    pArena->base = (DWORD)ptr;
    pArena->size = GetSelectorLimit16(sel) + 1;
    pArena->handle = (flags & GMEM_MOVEABLE) ? sel - 1 : sel;
    pArena->hOwner = hOwner;
    pArena->lockCount = 0;
    pArena->pageLockCount = 0;
    pArena->flags = flags & GA_MOVEABLE;
    if (flags & GMEM_DISCARDABLE) pArena->flags |= GA_DISCARDABLE;
    if (flags & GMEM_DDESHARE) pArena->flags |= GA_IPCSHARE;
    if (!(selflags & (WINE_LDT_FLAGS_CODE^WINE_LDT_FLAGS_DATA))) pArena->flags |= GA_DGROUP;
    pArena->selCount = selcount;
    if (selcount > 1)  /* clear the next arena blocks */
        memset( pArena + 1, 0, (selcount - 1) * sizeof(GLOBALARENA) );

    return pArena->handle;
}


/***********************************************************************
 *           GLOBAL_FreeBlock
 *
 * Free a block allocated by GLOBAL_CreateBlock, without touching
 * the associated linear memory range.
 */
BOOL16 GLOBAL_FreeBlock( HGLOBAL16 handle )
{
    WORD sel;
    GLOBALARENA *pArena;

    if (!handle) return TRUE;
    sel = GlobalHandleToSel16( handle );
    if (!VALID_HANDLE(sel))
    	return FALSE;
    pArena = GET_ARENA_PTR(sel);
    SELECTOR_FreeBlock( sel );
    memset( pArena, 0, sizeof(GLOBALARENA) );
    return TRUE;
}

/***********************************************************************
 *           GLOBAL_MoveBlock
 */
BOOL16 GLOBAL_MoveBlock( HGLOBAL16 handle, const void *ptr, DWORD size )
{
    WORD sel;
    GLOBALARENA *pArena;

    if (!handle) return TRUE;
    sel = GlobalHandleToSel16( handle );
    if (!VALID_HANDLE(sel))
    	return FALSE;
    pArena = GET_ARENA_PTR(sel);
    if (pArena->selCount != 1)
        return FALSE;

    pArena->base = (DWORD)ptr;
    pArena->size = size;
    SELECTOR_ReallocBlock( sel, ptr, size );
    return TRUE;
}

/***********************************************************************
 *           GLOBAL_Alloc
 *
 * Implementation of GlobalAlloc16()
 */
HGLOBAL16 GLOBAL_Alloc( UINT16 flags, DWORD size, HGLOBAL16 hOwner, unsigned char selflags )
{
    void *ptr;
    HGLOBAL16 handle;

    TRACE("%ld flags=%04x\n", size, flags );

    /* If size is 0, create a discarded block */

    if (size == 0) return GLOBAL_CreateBlock( flags, NULL, 1, hOwner, selflags );

    /* Fixup the size */

    if (size >= GLOBAL_MAX_ALLOC_SIZE - 0x1f) return 0;
    size = (size + 0x1f) & ~0x1f;

    /* Allocate the linear memory */
    ptr = HeapAlloc( GetProcessHeap(), 0, size );
      /* FIXME: free discardable blocks and try again? */
    if (!ptr) return 0;

      /* Allocate the selector(s) */

    handle = GLOBAL_CreateBlock( flags, ptr, size, hOwner, selflags );
    if (!handle)
    {
        HeapFree( GetProcessHeap(), 0, ptr );
        return 0;
    }

    if (flags & GMEM_ZEROINIT) memset( ptr, 0, size );
    return handle;
}

/***********************************************************************
 *           GlobalAlloc     (KERNEL.15)
 *           GlobalAlloc16   (KERNEL32.24)
 * RETURNS
 *	Handle: Success
 *	NULL: Failure
 */
HGLOBAL16 WINAPI GlobalAlloc16(
                 UINT16 flags, /* [in] Object allocation attributes */
                 DWORD size    /* [in] Number of bytes to allocate */
) {
    HANDLE16 owner = GetCurrentPDB16();

    if (flags & GMEM_DDESHARE)
        owner = GetExePtr(owner);  /* Make it a module handle */
    return GLOBAL_Alloc( flags, size, owner, WINE_LDT_FLAGS_DATA );
}


/***********************************************************************
 *           GlobalReAlloc     (KERNEL.16)
 *           GlobalReAlloc16   (KERNEL32.@)
 * RETURNS
 *	Handle: Success
 *	NULL: Failure
 */
HGLOBAL16 WINAPI GlobalReAlloc16(
                 HGLOBAL16 handle, /* [in] Handle of global memory object */
                 DWORD size,       /* [in] New size of block */
                 UINT16 flags      /* [in] How to reallocate object */
) {
    WORD selcount;
    DWORD oldsize;
    void *ptr, *newptr;
    GLOBALARENA *pArena, *pNewArena;
    WORD sel = GlobalHandleToSel16( handle );

    TRACE("%04x %ld flags=%04x\n",
                    handle, size, flags );
    if (!handle) return 0;

    if (!VALID_HANDLE(handle)) {
    	WARN("Invalid handle 0x%04x!\n", handle);
    	return 0;
    }
    pArena = GET_ARENA_PTR( handle );

      /* Discard the block if requested */

    if ((size == 0) && (flags & GMEM_MOVEABLE) && !(flags & GMEM_MODIFY))
    {
        if (!(pArena->flags & GA_MOVEABLE) ||
            !(pArena->flags & GA_DISCARDABLE) ||
            (pArena->lockCount > 0) || (pArena->pageLockCount > 0)) return 0;
        HeapFree( GetProcessHeap(), 0, (void *)pArena->base );
        pArena->base = 0;

        /* Note: we rely on the fact that SELECTOR_ReallocBlock won't
         * change the selector if we are shrinking the block.
	 * FIXME: shouldn't we keep selectors until the block is deleted?
	 */
        SELECTOR_ReallocBlock( sel, 0, 1 );
        return handle;
    }

      /* Fixup the size */

    if (size > GLOBAL_MAX_ALLOC_SIZE - 0x20) return 0;
    if (size == 0) size = 0x20;
    else size = (size + 0x1f) & ~0x1f;

      /* Change the flags */

    if (flags & GMEM_MODIFY)
    {
          /* Change the flags, leaving GA_DGROUP alone */
        pArena->flags = (pArena->flags & GA_DGROUP) | (flags & GA_MOVEABLE);
        if (flags & GMEM_DISCARDABLE) pArena->flags |= GA_DISCARDABLE;
        return handle;
    }

      /* Reallocate the linear memory */

    ptr = (void *)pArena->base;
    oldsize = pArena->size;
    TRACE("oldbase %p oldsize %08lx newsize %08lx\n", ptr,oldsize,size);
    if (ptr && (size == oldsize)) return handle;  /* Nothing to do */

    if (pArena->flags & GA_DOSMEM)
        newptr = DOSMEM_ResizeBlock(ptr, size, NULL);
    else
        /* if more then one reader (e.g. some pointer has been given out by GetVDMPointer32W16),
	   only try to realloc in place */
        newptr = HeapReAlloc( GetProcessHeap(),
                              (pArena->pageLockCount > 0)?HEAP_REALLOC_IN_PLACE_ONLY:0, ptr, size );
    if (!newptr)
    {
        FIXME("Realloc failed lock %d\n",pArena->pageLockCount);
        if (pArena->pageLockCount <1)
        {
            HeapFree( GetProcessHeap(), 0, ptr );
            SELECTOR_FreeBlock( sel );
            memset( pArena, 0, sizeof(GLOBALARENA) );
        }
        return 0;
    }
    ptr = newptr;

      /* Reallocate the selector(s) */

    sel = SELECTOR_ReallocBlock( sel, ptr, size );
    if (!sel)
    {
        HeapFree( GetProcessHeap(), 0, ptr );
        memset( pArena, 0, sizeof(GLOBALARENA) );
        return 0;
    }
    selcount = (size + 0xffff) / 0x10000;

    if (!(pNewArena = GLOBAL_GetArena( sel, selcount )))
    {
        HeapFree( GetProcessHeap(), 0, ptr );
        SELECTOR_FreeBlock( sel );
        return 0;
    }

      /* Fill the new arena block
         As we may have used HEAP_REALLOC_IN_PLACE_ONLY, areas may overlap*/

    if (pNewArena != pArena) memmove( pNewArena, pArena, sizeof(GLOBALARENA) );
    pNewArena->base = (DWORD)ptr;
    pNewArena->size = GetSelectorLimit16(sel) + 1;
    pNewArena->selCount = selcount;
    pNewArena->handle = (pNewArena->flags & GA_MOVEABLE) ? sel - 1 : sel;

    if (selcount > 1)  /* clear the next arena blocks */
        memset( pNewArena + 1, 0, (selcount - 1) * sizeof(GLOBALARENA) );

    if ((oldsize < size) && (flags & GMEM_ZEROINIT))
        memset( (char *)ptr + oldsize, 0, size - oldsize );
    return pNewArena->handle;
}


/***********************************************************************
 *           GlobalFree     (KERNEL.17)
 *           GlobalFree16   (KERNEL32.31)
 * RETURNS
 *	NULL: Success
 *	Handle: Failure
 */
HGLOBAL16 WINAPI GlobalFree16(
                 HGLOBAL16 handle /* [in] Handle of global memory object */
) {
    void *ptr;

    if (!VALID_HANDLE(handle)) {
    	WARN("Invalid handle 0x%04x passed to GlobalFree16!\n",handle);
    	return 0;
    }
    ptr = (void *)GET_ARENA_PTR(handle)->base;

    TRACE("%04x\n", handle );
    if (!GLOBAL_FreeBlock( handle )) return handle;  /* failed */
    if (ptr) HeapFree( GetProcessHeap(), 0, ptr );
    return 0;
}


/***********************************************************************
 *           GlobalLock   (KERNEL.18)
 *
 * This is the GlobalLock16() function used by 16-bit code.
 */
SEGPTR WINAPI WIN16_GlobalLock16( HGLOBAL16 handle )
{
    WORD sel = GlobalHandleToSel16( handle );
    TRACE("(%04x) -> %08lx\n", handle, MAKELONG( 0, sel ) );

    if (handle)
    {
	if (handle == (HGLOBAL16)-1) handle = CURRENT_DS;

	if (!VALID_HANDLE(handle)) {
	    WARN("Invalid handle 0x%04x passed to WIN16_GlobalLock16!\n",handle);
	    sel = 0;
	}
	else if (!GET_ARENA_PTR(handle)->base)
            sel = 0;
        else
            GET_ARENA_PTR(handle)->lockCount++;
    }

    CURRENT_STACK16->ecx = sel;  /* selector must be returned in CX as well */
    return MAKESEGPTR( sel, 0 );
}


/**********************************************************************
 *           K32WOWGlobalLock16         (KERNEL32.60)
 */
SEGPTR WINAPI K32WOWGlobalLock16( HGLOBAL16 hMem )
{
    return WIN16_GlobalLock16( hMem );
}


/***********************************************************************
 *           GlobalLock16   (KERNEL32.25)
 *
 * This is the GlobalLock16() function used by 32-bit code.
 *
 * RETURNS
 *	Pointer to first byte of memory block
 *	NULL: Failure
 */
LPVOID WINAPI GlobalLock16(
              HGLOBAL16 handle /* [in] Handle of global memory object */
) {
    if (!handle) return 0;
    if (!VALID_HANDLE(handle))
	return (LPVOID)0;
    GET_ARENA_PTR(handle)->lockCount++;
    return (LPVOID)GET_ARENA_PTR(handle)->base;
}


/***********************************************************************
 *           GlobalUnlock     (KERNEL.19)
 *           GlobalUnlock16   (KERNEL32.26)
 * NOTES
 *	Should the return values be cast to booleans?
 *
 * RETURNS
 *	TRUE: Object is still locked
 *	FALSE: Object is unlocked
 */
BOOL16 WINAPI GlobalUnlock16(
              HGLOBAL16 handle /* [in] Handle of global memory object */
) {
    GLOBALARENA *pArena = GET_ARENA_PTR(handle);
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalUnlock16!\n",handle);
	return 0;
    }
    TRACE("%04x\n", handle );
    if (pArena->lockCount) pArena->lockCount--;
    return pArena->lockCount;
}

/***********************************************************************
 *     GlobalChangeLockCount               (KERNEL.365)
 *
 * This is declared as a register function as it has to preserve
 * *all* registers, even AX/DX !
 *
 */
void WINAPI GlobalChangeLockCount16( HGLOBAL16 handle, INT16 delta,
                                     CONTEXT86 *context )
{
    if ( delta == 1 )
        GlobalLock16( handle );
    else if ( delta == -1 )
        GlobalUnlock16( handle );
    else
        ERR("(%04X, %d): strange delta value\n", handle, delta );
}

/***********************************************************************
 *           GlobalSize     (KERNEL.20)
 *           GlobalSize16   (KERNEL32.32)
 * RETURNS
 *	Size in bytes of object
 *	0: Failure
 */
DWORD WINAPI GlobalSize16(
             HGLOBAL16 handle /* [in] Handle of global memory object */
) {
    TRACE("%04x\n", handle );
    if (!handle) return 0;
    if (!VALID_HANDLE(handle))
	return 0;
    return GET_ARENA_PTR(handle)->size;
}


/***********************************************************************
 *           GlobalHandle   (KERNEL.21)
 * NOTES
 *	Why is GlobalHandleToSel used here with the sel as input?
 *
 * RETURNS
 *	Handle: Success
 *	NULL: Failure
 */
DWORD WINAPI GlobalHandle16(
             WORD sel /* [in] Address of global memory block */
) {
    TRACE("%04x\n", sel );
    if (!VALID_HANDLE(sel)) {
	WARN("Invalid handle 0x%04x passed to GlobalHandle16!\n",sel);
	return 0;
    }
    return MAKELONG( GET_ARENA_PTR(sel)->handle, GlobalHandleToSel16(sel) );
}

/***********************************************************************
 *           GlobalHandleNoRIP   (KERNEL.159)
 */
DWORD WINAPI GlobalHandleNoRIP16( WORD sel )
{
    int i;
    for (i = globalArenaSize-1 ; i>=0 ; i--) {
        if (pGlobalArena[i].size!=0 && pGlobalArena[i].handle == sel)
		return MAKELONG( GET_ARENA_PTR(sel)->handle, GlobalHandleToSel16(sel) );
    }
    return 0;
}


/***********************************************************************
 *           GlobalFlags     (KERNEL.22)
 *           GlobalFlags16   (KERNEL32.@)
 * NOTES
 *	Should this return GMEM_INVALID_HANDLE instead of 0 on invalid
 *	handle?
 *
 * RETURNS
 *	Value specifying flags and lock count
 *	GMEM_INVALID_HANDLE: Invalid handle
 */
UINT16 WINAPI GlobalFlags16(
              HGLOBAL16 handle /* [in] Handle of global memory object */
) {
    GLOBALARENA *pArena;

    TRACE("%04x\n", handle );
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalFlags16!\n",handle);
	return 0;
    }
    pArena = GET_ARENA_PTR(handle);
    return pArena->lockCount |
           ((pArena->flags & GA_DISCARDABLE) ? GMEM_DISCARDABLE : 0) |
           ((pArena->base == 0) ? GMEM_DISCARDED : 0);
}


/***********************************************************************
 *           LockSegment   (KERNEL.23)
 */
HGLOBAL16 WINAPI LockSegment16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (handle == (HGLOBAL16)-1) handle = CURRENT_DS;
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to LockSegment16!\n",handle);
	return 0;
    }
    GET_ARENA_PTR(handle)->lockCount++;
    return handle;
}


/***********************************************************************
 *           UnlockSegment   (KERNEL.24)
 */
void WINAPI UnlockSegment16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (handle == (HGLOBAL16)-1) handle = CURRENT_DS;
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to UnlockSegment16!\n",handle);
	return;
    }
    GET_ARENA_PTR(handle)->lockCount--;
    /* FIXME: this ought to return the lock count in CX (go figure...) */
}


/***********************************************************************
 *           GlobalCompact   (KERNEL.25)
 */
DWORD WINAPI GlobalCompact16( DWORD desired )
{
    return GLOBAL_MAX_ALLOC_SIZE;
}


/***********************************************************************
 *           GlobalFreeAll   (KERNEL.26)
 */
void WINAPI GlobalFreeAll16( HGLOBAL16 owner )
{
    DWORD i;
    GLOBALARENA *pArena;

    pArena = pGlobalArena;
    for (i = 0; i < globalArenaSize; i++, pArena++)
    {
        if ((pArena->size != 0) && (pArena->hOwner == owner))
            GlobalFree16( pArena->handle );
    }
}


/***********************************************************************
 *           GlobalWire     (KERNEL.111)
 *           GlobalWire16   (KERNEL32.29)
 */
SEGPTR WINAPI GlobalWire16( HGLOBAL16 handle )
{
    return WIN16_GlobalLock16( handle );
}


/***********************************************************************
 *           GlobalUnWire     (KERNEL.112)
 *           GlobalUnWire16   (KERNEL32.30)
 */
BOOL16 WINAPI GlobalUnWire16( HGLOBAL16 handle )
{
    return !GlobalUnlock16( handle );
}


/***********************************************************************
 *           SetSwapAreaSize   (KERNEL.106)
 */
LONG WINAPI SetSwapAreaSize16( WORD size )
{
    FIXME("(%d) - stub!\n", size );
    return MAKELONG( size, 0xffff );
}


/***********************************************************************
 *           GlobalLRUOldest   (KERNEL.163)
 */
HGLOBAL16 WINAPI GlobalLRUOldest16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (handle == (HGLOBAL16)-1) handle = CURRENT_DS;
    return handle;
}


/***********************************************************************
 *           GlobalLRUNewest   (KERNEL.164)
 */
HGLOBAL16 WINAPI GlobalLRUNewest16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (handle == (HGLOBAL16)-1) handle = CURRENT_DS;
    return handle;
}


/***********************************************************************
 *           GetFreeSpace   (KERNEL.169)
 */
DWORD WINAPI GetFreeSpace16( UINT16 wFlags )
{
    MEMORYSTATUS ms;
    GlobalMemoryStatus( &ms );
    return ms.dwAvailVirtual;
}

/***********************************************************************
 *           GlobalDOSAlloc   (KERNEL.184)
 * RETURNS
 *	Address (HW=Paragraph segment; LW=Selector)
 */
DWORD WINAPI GlobalDOSAlloc16(
             DWORD size /* [in] Number of bytes to be allocated */
) {
   UINT16    uParagraph;
   LPVOID    lpBlock = DOSMEM_GetBlock( size, &uParagraph );

   if( lpBlock )
   {
       HMODULE16 hModule = GetModuleHandle16("KERNEL");
       WORD	 wSelector;
       GLOBALARENA *pArena;

       wSelector = GLOBAL_CreateBlock(GMEM_FIXED, lpBlock, size, hModule, WINE_LDT_FLAGS_DATA );
       pArena = GET_ARENA_PTR(wSelector);
       pArena->flags |= GA_DOSMEM;
       return MAKELONG(wSelector,uParagraph);
   }
   return 0;
}


/***********************************************************************
 *           GlobalDOSFree      (KERNEL.185)
 * RETURNS
 *	NULL: Success
 *	sel: Failure
 */
WORD WINAPI GlobalDOSFree16(
            WORD sel /* [in] Selector */
) {
   DWORD   block = GetSelectorBase(sel);

   if( block && block < 0x100000 )
   {
       LPVOID lpBlock = DOSMEM_MapDosToLinear( block );
       if( DOSMEM_FreeBlock( lpBlock ) )
	   GLOBAL_FreeBlock( sel );
       sel = 0;
   }
   return sel;
}


/***********************************************************************
 *           GlobalPageLock   (KERNEL.191)
 *           GlobalSmartPageLock(KERNEL.230)
 */
WORD WINAPI GlobalPageLock16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalPageLock!\n",handle);
	return 0;
    }
    return ++(GET_ARENA_PTR(handle)->pageLockCount);
}


/***********************************************************************
 *           GlobalPageUnlock   (KERNEL.192)
 *           GlobalSmartPageUnlock(KERNEL.231)
 */
WORD WINAPI GlobalPageUnlock16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalPageUnlock!\n",handle);
	return 0;
    }
    return --(GET_ARENA_PTR(handle)->pageLockCount);
}


/***********************************************************************
 *           GlobalFix     (KERNEL.197)
 *           GlobalFix16   (KERNEL32.27)
 */
WORD WINAPI GlobalFix16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalFix16!\n",handle);
	return 0;
    }
    GET_ARENA_PTR(handle)->lockCount++;

    return GlobalHandleToSel16(handle);
}


/***********************************************************************
 *           GlobalUnfix     (KERNEL.198)
 *           GlobalUnfix16   (KERNEL32.28)
 */
void WINAPI GlobalUnfix16( HGLOBAL16 handle )
{
    TRACE("%04x\n", handle );
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalUnfix16!\n",handle);
	return;
    }
    GET_ARENA_PTR(handle)->lockCount--;
}


/***********************************************************************
 *           FarSetOwner   (KERNEL.403)
 */
void WINAPI FarSetOwner16( HGLOBAL16 handle, HANDLE16 hOwner )
{
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to FarSetOwner!\n",handle);
	return;
    }
    GET_ARENA_PTR(handle)->hOwner = hOwner;
}


/***********************************************************************
 *           FarGetOwner   (KERNEL.404)
 */
HANDLE16 WINAPI FarGetOwner16( HGLOBAL16 handle )
{
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to FarGetOwner!\n",handle);
	return 0;
    }
    return GET_ARENA_PTR(handle)->hOwner;
}


/***********************************************************************
 *           GlobalHandleToSel   (TOOLHELP.50)
 */
WORD WINAPI GlobalHandleToSel16( HGLOBAL16 handle )
{
    if (!handle) return 0;
    if (!VALID_HANDLE(handle)) {
	WARN("Invalid handle 0x%04x passed to GlobalHandleToSel!\n",handle);
	return 0;
    }
    if (!(handle & 7))
    {
        WARN("Program attempted invalid selector conversion\n" );
        return handle - 1;
    }
    return handle | 7;
}


/***********************************************************************
 *           GlobalFirst   (TOOLHELP.51)
 */
BOOL16 WINAPI GlobalFirst16( GLOBALENTRY *pGlobal, WORD wFlags )
{
    if (wFlags == GLOBAL_LRU) return FALSE;
    pGlobal->dwNext = 0;
    return GlobalNext16( pGlobal, wFlags );
}


/***********************************************************************
 *           GlobalNext   (TOOLHELP.52)
 */
BOOL16 WINAPI GlobalNext16( GLOBALENTRY *pGlobal, WORD wFlags)
{
    GLOBALARENA *pArena;

    if (pGlobal->dwNext >= globalArenaSize) return FALSE;
    pArena = pGlobalArena + pGlobal->dwNext;
    if (wFlags == GLOBAL_FREE)  /* only free blocks */
    {
        int i;
        for (i = pGlobal->dwNext; i < globalArenaSize; i++, pArena++)
            if (pArena->size == 0) break;  /* block is free */
        if (i >= globalArenaSize) return FALSE;
        pGlobal->dwNext = i;
    }

    pGlobal->dwAddress    = pArena->base;
    pGlobal->dwBlockSize  = pArena->size;
    pGlobal->hBlock       = pArena->handle;
    pGlobal->wcLock       = pArena->lockCount;
    pGlobal->wcPageLock   = pArena->pageLockCount;
    pGlobal->wFlags       = (GetCurrentPDB16() == pArena->hOwner);
    pGlobal->wHeapPresent = FALSE;
    pGlobal->hOwner       = pArena->hOwner;
    pGlobal->wType        = GT_UNKNOWN;
    pGlobal->wData        = 0;
    pGlobal->dwNext++;
    return TRUE;
}


/***********************************************************************
 *           GlobalInfo   (TOOLHELP.53)
 */
BOOL16 WINAPI GlobalInfo16( GLOBALINFO *pInfo )
{
    int i;
    GLOBALARENA *pArena;

    pInfo->wcItems = globalArenaSize;
    pInfo->wcItemsFree = 0;
    pInfo->wcItemsLRU = 0;
    for (i = 0, pArena = pGlobalArena; i < globalArenaSize; i++, pArena++)
        if (pArena->size == 0) pInfo->wcItemsFree++;
    return TRUE;
}


/***********************************************************************
 *           GlobalEntryHandle   (TOOLHELP.54)
 */
BOOL16 WINAPI GlobalEntryHandle16( GLOBALENTRY *pGlobal, HGLOBAL16 hItem )
{
    GLOBALARENA *pArena = GET_ARENA_PTR(hItem);

    pGlobal->dwAddress    = pArena->base;
    pGlobal->dwBlockSize  = pArena->size;
    pGlobal->hBlock       = pArena->handle;
    pGlobal->wcLock       = pArena->lockCount;
    pGlobal->wcPageLock   = pArena->pageLockCount;
    pGlobal->wFlags       = (GetCurrentPDB16() == pArena->hOwner);
    pGlobal->wHeapPresent = FALSE;
    pGlobal->hOwner       = pArena->hOwner;
    pGlobal->wType        = GT_UNKNOWN;
    pGlobal->wData        = 0;
    pGlobal->dwNext++;
    return TRUE;
}


/***********************************************************************
 *           GlobalEntryModule   (TOOLHELP.55)
 */
BOOL16 WINAPI GlobalEntryModule16( GLOBALENTRY *pGlobal, HMODULE16 hModule,
                                 WORD wSeg )
{
    FIXME("(%p, 0x%04x, 0x%04x), stub.\n", pGlobal, hModule, wSeg);
    return FALSE;
}


/***********************************************************************
 *           MemManInfo   (TOOLHELP.72)
 */
BOOL16 WINAPI MemManInfo16( MEMMANINFO *info )
{
    MEMORYSTATUS status;

    /*
     * Not unsurprisingly although the documention says you
     * _must_ provide the size in the dwSize field, this function
     * (under Windows) always fills the structure and returns true.
     */
    GlobalMemoryStatus( &status );
    info->wPageSize            = getpagesize();
    info->dwLargestFreeBlock   = status.dwAvailVirtual;
    info->dwMaxPagesAvailable  = info->dwLargestFreeBlock / info->wPageSize;
    info->dwMaxPagesLockable   = info->dwMaxPagesAvailable;
    info->dwTotalLinearSpace   = status.dwTotalVirtual / info->wPageSize;
    info->dwTotalUnlockedPages = info->dwTotalLinearSpace;
    info->dwFreePages          = info->dwMaxPagesAvailable;
    info->dwTotalPages         = info->dwTotalLinearSpace;
    info->dwFreeLinearSpace    = info->dwMaxPagesAvailable;
    info->dwSwapFilePages      = status.dwTotalPageFile / info->wPageSize;
    return TRUE;
}

/***********************************************************************
 *           GetFreeMemInfo   (KERNEL.316)
 */
DWORD WINAPI GetFreeMemInfo16(void)
{
    MEMMANINFO info;
    MemManInfo16( &info );
    return MAKELONG( info.dwTotalLinearSpace, info.dwMaxPagesAvailable );
}

/*
 * Win32 Global heap functions (GlobalXXX).
 * These functions included in Win32 for compatibility with 16 bit Windows
 * Especially the moveable blocks and handles are oldish.
 * But the ability to directly allocate memory with GPTR and LPTR is widely
 * used.
 *
 * The handle stuff looks horrible, but it's implemented almost like Win95
 * does it.
 *
 */

#define MAGIC_GLOBAL_USED 0x5342
#define GLOBAL_LOCK_MAX   0xFF
#define HANDLE_TO_INTERN(h)  ((PGLOBAL32_INTERN)(((char *)(h))-2))
#define INTERN_TO_HANDLE(i)  ((HGLOBAL) &((i)->Pointer))
#define POINTER_TO_HANDLE(p) (*(((HGLOBAL *)(p))-1))
#define ISHANDLE(h)          (((DWORD)(h)&2)!=0)
#define ISPOINTER(h)         (((DWORD)(h)&2)==0)

typedef struct __GLOBAL32_INTERN
{
   WORD         Magic;
   LPVOID       Pointer WINE_PACKED;
   BYTE         Flags;
   BYTE         LockCount;
} GLOBAL32_INTERN, *PGLOBAL32_INTERN;


/***********************************************************************
 *           GlobalAlloc   (KERNEL32.@)
 * RETURNS
 *	Handle: Success
 *	NULL: Failure
 */
HGLOBAL WINAPI GlobalAlloc(
                 UINT flags, /* [in] Object allocation attributes */
                 DWORD size    /* [in] Number of bytes to allocate */
) {
   PGLOBAL32_INTERN     pintern;
   DWORD		hpflags;
   LPVOID               palloc;

   if(flags&GMEM_ZEROINIT)
      hpflags=HEAP_ZERO_MEMORY;
   else
      hpflags=0;

   TRACE("() flags=%04x\n",  flags );

   if((flags & GMEM_MOVEABLE)==0) /* POINTER */
   {
      palloc=HeapAlloc(GetProcessHeap(), hpflags, size);
      return (HGLOBAL) palloc;
   }
   else  /* HANDLE */
   {
      /* HeapLock(heap); */

      pintern=HeapAlloc(GetProcessHeap(), 0,  sizeof(GLOBAL32_INTERN));
      if (!pintern) return 0;
      if(size)
      {
	 if (!(palloc=HeapAlloc(GetProcessHeap(), hpflags, size+sizeof(HGLOBAL)))) {
	    HeapFree(GetProcessHeap(), 0, pintern);
	    return 0;
	 }
	 *(HGLOBAL *)palloc=INTERN_TO_HANDLE(pintern);
	 pintern->Pointer=(char *) palloc+sizeof(HGLOBAL);
      }
      else
	 pintern->Pointer=NULL;
      pintern->Magic=MAGIC_GLOBAL_USED;
      pintern->Flags=flags>>8;
      pintern->LockCount=0;

      /* HeapUnlock(heap); */

      return INTERN_TO_HANDLE(pintern);
   }
}


/***********************************************************************
 *           GlobalLock   (KERNEL32.@)
 * RETURNS
 *	Pointer to first byte of block
 *	NULL: Failure
 */
LPVOID WINAPI GlobalLock(
              HGLOBAL hmem /* [in] Handle of global memory object */
) {
   PGLOBAL32_INTERN pintern;
   LPVOID           palloc;

   if(ISPOINTER(hmem))
      return (LPVOID) hmem;

   /* HeapLock(GetProcessHeap()); */

   pintern=HANDLE_TO_INTERN(hmem);
   if(pintern->Magic==MAGIC_GLOBAL_USED)
   {
      if(pintern->LockCount<GLOBAL_LOCK_MAX)
	 pintern->LockCount++;
      palloc=pintern->Pointer;
   }
   else
   {
      WARN("invalid handle\n");
      palloc=(LPVOID) NULL;
      SetLastError(ERROR_INVALID_HANDLE);
   }
   /* HeapUnlock(GetProcessHeap()); */;
   return palloc;
}


/***********************************************************************
 *           GlobalUnlock   (KERNEL32.@)
 * RETURNS
 *	TRUE: Object is still locked
 *	FALSE: Object is unlocked
 */
BOOL WINAPI GlobalUnlock(
              HGLOBAL hmem /* [in] Handle of global memory object */
) {
    PGLOBAL32_INTERN pintern;
    BOOL locked;

    if (ISPOINTER(hmem)) return FALSE;

    __TRY
    {
        /* HeapLock(GetProcessHeap()); */
        pintern=HANDLE_TO_INTERN(hmem);
        if(pintern->Magic==MAGIC_GLOBAL_USED)
        {
            if((pintern->LockCount<GLOBAL_LOCK_MAX)&&(pintern->LockCount>0))
                pintern->LockCount--;

            locked = (pintern->LockCount != 0);
            if (!locked) SetLastError(NO_ERROR);
        }
        else
        {
            WARN("invalid handle\n");
            SetLastError(ERROR_INVALID_HANDLE);
            locked=FALSE;
        }
        /* HeapUnlock(GetProcessHeap()); */
    }
    __EXCEPT(page_fault)
    {
        ERR("page fault occurred ! Caused by bug ?\n");
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    __ENDTRY
    return locked;
}


/***********************************************************************
 *           GlobalHandle   (KERNEL32.@)
 * Returns the handle associated with the specified pointer.
 *
 * RETURNS
 *	Handle: Success
 *	NULL: Failure
 */
HGLOBAL WINAPI GlobalHandle(
                 LPCVOID pmem /* [in] Pointer to global memory block */
) {
    HGLOBAL handle;
    PGLOBAL32_INTERN  maybe_intern;
    LPCVOID test;

    if (!pmem)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    __TRY
    {
        handle = 0;

        /* note that if pmem is a pointer to a a block allocated by        */
        /* GlobalAlloc with GMEM_MOVEABLE then magic test in HeapValidate  */
        /* will fail.                                                      */
        if (ISPOINTER(pmem)) {
            if (HeapValidate( GetProcessHeap(), 0, pmem )) {
                handle = (HGLOBAL)pmem;  /* valid fixed block */
                break;
            }
            handle = POINTER_TO_HANDLE(pmem);
        } else
            handle = (HGLOBAL)pmem;

        /* Now test handle either passed in or retrieved from pointer */
        maybe_intern = HANDLE_TO_INTERN( handle );
        if (maybe_intern->Magic == MAGIC_GLOBAL_USED) {
            test = maybe_intern->Pointer;
            if (HeapValidate( GetProcessHeap(), 0, ((HGLOBAL *)test)-1 ) && /* obj(-handle) valid arena? */
                HeapValidate( GetProcessHeap(), 0, maybe_intern ))  /* intern valid arena? */
                break;  /* valid moveable block */
        }
        handle = 0;
        SetLastError( ERROR_INVALID_HANDLE );
    }
    __EXCEPT(page_fault)
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return 0;
    }
    __ENDTRY

    return handle;
}


/***********************************************************************
 *           GlobalReAlloc   (KERNEL32.@)
 * RETURNS
 *	Handle: Success
 *	NULL: Failure
 */
HGLOBAL WINAPI GlobalReAlloc(
                 HGLOBAL hmem, /* [in] Handle of global memory object */
                 DWORD size,     /* [in] New size of block */
                 UINT flags    /* [in] How to reallocate object */
) {
   LPVOID               palloc;
   HGLOBAL            hnew;
   PGLOBAL32_INTERN     pintern;
   DWORD heap_flags = (flags & GMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0;

   hnew = 0;
   /* HeapLock(heap); */
   if(flags & GMEM_MODIFY) /* modify flags */
   {
      if( ISPOINTER(hmem) && (flags & GMEM_MOVEABLE))
      {
	 /* make a fixed block moveable
	  * actually only NT is able to do this. But it's soo simple
	  */
         if (hmem == 0)
         {
	     ERR("GlobalReAlloc32 with null handle!\n");
             SetLastError( ERROR_NOACCESS );
    	     return 0;
         }
	 size=HeapSize(GetProcessHeap(), 0, (LPVOID) hmem);
	 hnew=GlobalAlloc( flags, size);
	 palloc=GlobalLock(hnew);
	 memcpy(palloc, (LPVOID) hmem, size);
	 GlobalUnlock(hnew);
	 GlobalFree(hmem);
      }
      else if( ISPOINTER(hmem) &&(flags & GMEM_DISCARDABLE))
      {
	 /* change the flags to make our block "discardable" */
	 pintern=HANDLE_TO_INTERN(hmem);
	 pintern->Flags = pintern->Flags | (GMEM_DISCARDABLE >> 8);
	 hnew=hmem;
      }
      else
      {
	 SetLastError(ERROR_INVALID_PARAMETER);
	 hnew = 0;
      }
   }
   else
   {
      if(ISPOINTER(hmem))
      {
	 /* reallocate fixed memory */
	 hnew=(HGLOBAL)HeapReAlloc(GetProcessHeap(), heap_flags, (LPVOID) hmem, size);
      }
      else
      {
	 /* reallocate a moveable block */
	 pintern=HANDLE_TO_INTERN(hmem);

#if 0
/* Apparently Windows doesn't care whether the handle is locked at this point */
/* See also the same comment in GlobalFree() */
	 if(pintern->LockCount>1) {
	    ERR("handle 0x%08lx is still locked, cannot realloc!\n",(DWORD)hmem);
	    SetLastError(ERROR_INVALID_HANDLE);
	 } else
#endif
         if(size!=0)
	 {
	    hnew=hmem;
	    if(pintern->Pointer)
	    {
	       if((palloc = HeapReAlloc(GetProcessHeap(), heap_flags,
				   (char *) pintern->Pointer-sizeof(HGLOBAL),
				   size+sizeof(HGLOBAL))) == NULL)
		   return 0; /* Block still valid */
	       pintern->Pointer=(char *) palloc+sizeof(HGLOBAL);
	    }
	    else
	    {
	        if((palloc=HeapAlloc(GetProcessHeap(), heap_flags, size+sizeof(HGLOBAL)))
		   == NULL)
		    return 0;
	       *(HGLOBAL *)palloc=hmem;
	       pintern->Pointer=(char *) palloc+sizeof(HGLOBAL);
	    }
	 }
	 else
	 {
	    if(pintern->Pointer)
	    {
	       HeapFree(GetProcessHeap(), 0, (char *) pintern->Pointer-sizeof(HGLOBAL));
	       pintern->Pointer=NULL;
	    }
	 }
      }
   }
   /* HeapUnlock(heap); */
   return hnew;
}


/***********************************************************************
 *           GlobalFree   (KERNEL32.@)
 * RETURNS
 *	NULL: Success
 *	Handle: Failure
 */
HGLOBAL WINAPI GlobalFree(
                 HGLOBAL hmem /* [in] Handle of global memory object */
) {
    PGLOBAL32_INTERN pintern;
    HGLOBAL hreturned;

    __TRY
    {
        hreturned = 0;
        if(ISPOINTER(hmem)) /* POINTER */
        {
            if(!HeapFree(GetProcessHeap(), 0, (LPVOID) hmem)) hmem = 0;
        }
        else  /* HANDLE */
        {
            /* HeapLock(heap); */
            pintern=HANDLE_TO_INTERN(hmem);

            if(pintern->Magic==MAGIC_GLOBAL_USED)
            {

                /* WIN98 does not make this test. That is you can free a */
                /* block you have not unlocked. Go figure!!              */
                /* if(pintern->LockCount!=0)  */
                /*    SetLastError(ERROR_INVALID_HANDLE);  */

                if(pintern->Pointer) {
                    if(!HeapFree(GetProcessHeap(), 0, (char *)(pintern->Pointer)-sizeof(HGLOBAL)))
                        hreturned=hmem;
                    pintern->Pointer = NULL;
                }
                if(!HeapFree(GetProcessHeap(), 0, pintern))
                    hreturned=hmem;
            }
            /* HeapUnlock(heap); */
        }
    }
    __EXCEPT(page_fault)
    {
        ERR("page fault occurred ! Caused by bug ?\n");
        SetLastError( ERROR_INVALID_PARAMETER );
        return hmem;
    }
    __ENDTRY
    return hreturned;
}


/***********************************************************************
 *           GlobalSize   (KERNEL32.@)
 * RETURNS
 *	Size in bytes of the global memory object
 *	0: Failure
 */
DWORD WINAPI GlobalSize(
             HGLOBAL hmem /* [in] Handle of global memory object */
) {
   DWORD                retval;
   PGLOBAL32_INTERN     pintern;

   if(ISPOINTER(hmem))
   {
      retval=HeapSize(GetProcessHeap(), 0,  (LPVOID) hmem);
   }
   else
   {
      /* HeapLock(heap); */
      pintern=HANDLE_TO_INTERN(hmem);

      if(pintern->Magic==MAGIC_GLOBAL_USED)
      {
        if (!pintern->Pointer) /* handle case of GlobalAlloc( ??,0) */
            return 0;
	 retval=HeapSize(GetProcessHeap(), 0,
	                 (char *)(pintern->Pointer)-sizeof(HGLOBAL))-4;
	 if (retval == 0xffffffff-4) retval = 0;
      }
      else
      {
	 WARN("invalid handle\n");
	 retval=0;
      }
      /* HeapUnlock(heap); */
   }
   /* HeapSize returns 0xffffffff on failure */
   if (retval == 0xffffffff) retval = 0;
   return retval;
}


/***********************************************************************
 *           GlobalWire   (KERNEL32.@)
 */
LPVOID WINAPI GlobalWire(HGLOBAL hmem)
{
   return GlobalLock( hmem );
}


/***********************************************************************
 *           GlobalUnWire   (KERNEL32.@)
 */
BOOL WINAPI GlobalUnWire(HGLOBAL hmem)
{
   return GlobalUnlock( hmem);
}


/***********************************************************************
 *           GlobalFix   (KERNEL32.@)
 */
VOID WINAPI GlobalFix(HGLOBAL hmem)
{
    GlobalLock( hmem );
}


/***********************************************************************
 *           GlobalUnfix   (KERNEL32.@)
 */
VOID WINAPI GlobalUnfix(HGLOBAL hmem)
{
   GlobalUnlock( hmem);
}


/***********************************************************************
 *           GlobalFlags   (KERNEL32.@)
 * Returns information about the specified global memory object
 *
 * NOTES
 *	Should this return GMEM_INVALID_HANDLE on invalid handle?
 *
 * RETURNS
 *	Value specifying allocation flags and lock count
 *	GMEM_INVALID_HANDLE: Failure
 */
UINT WINAPI GlobalFlags(
              HGLOBAL hmem /* [in] Handle to global memory object */
) {
   DWORD                retval;
   PGLOBAL32_INTERN     pintern;

   if(ISPOINTER(hmem))
   {
      retval=0;
   }
   else
   {
      /* HeapLock(GetProcessHeap()); */
      pintern=HANDLE_TO_INTERN(hmem);
      if(pintern->Magic==MAGIC_GLOBAL_USED)
      {
	 retval=pintern->LockCount + (pintern->Flags<<8);
	 if(pintern->Pointer==0)
	    retval|= GMEM_DISCARDED;
      }
      else
      {
	 WARN("Invalid handle: %04x\n", hmem);
	 retval=0;
      }
      /* HeapUnlock(GetProcessHeap()); */
   }
   return retval;
}


/***********************************************************************
 *           GlobalCompact   (KERNEL32.@)
 */
DWORD WINAPI GlobalCompact( DWORD minfree )
{
    return 0;  /* GlobalCompact does nothing in Win32 */
}


/***********************************************************************
 *           GlobalMemoryStatus   (KERNEL32.@)
 * RETURNS
 *	None
 */
BOOL WINAPI GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpmemex)
{
    static MEMORYSTATUSEX cached_memstatus;
    static int cache_lastchecked = 0;
    SYSTEM_INFO si;
#ifdef linux
    FILE *f;
#endif
#if defined(__FreeBSD__)
    unsigned int tmp;
    size_t size_sys;
#endif
#ifdef __APPLE__
    mach_msg_type_number_t count;
    kern_return_t error;
    vm_statistics_data_t vm_stat;
#endif

    if (time(NULL)==cache_lastchecked) {
	memcpy(lpmemex,&cached_memstatus,sizeof(cached_memstatus));
	return TRUE;
    }
    cache_lastchecked = time(NULL);

#ifdef linux
    f = fopen( "/proc/meminfo", "r" );
    if (f)
    {
        char buffer[256];
        unsigned long long total, used, free, shared, buffers, cached;

        lpmemex->dwLength = sizeof(*lpmemex);
        lpmemex->ullTotalPhys = lpmemex->ullAvailPhys = 0;
        lpmemex->ullTotalPageFile = lpmemex->ullAvailPageFile = 0;

        while (fgets( buffer, sizeof(buffer), f ))
        {
	    /* old style /proc/meminfo ... */
            if (sscanf( buffer, "Mem: %llu %llu %llu %llu %llu %llu", &total, &used, &free, &shared, &buffers, &cached ))
            {
                lpmemex->ullTotalPhys += total;
                lpmemex->ullAvailPhys += free + buffers + cached;
            }
            if (sscanf( buffer, "Swap: %llu %llu %llu", &total, &used, &free ))
            {
                lpmemex->ullTotalPageFile += total;
                lpmemex->ullAvailPageFile += free;
            }

	    /* new style /proc/meminfo ... */
	    if (sscanf(buffer, "MemTotal: %llu", &total))
	    	lpmemex->ullTotalPhys = total*1024;
	    if (sscanf(buffer, "MemFree: %llu", &free))
	    	lpmemex->ullAvailPhys = free*1024;
	    if (sscanf(buffer, "SwapTotal: %llu", &total))
	        lpmemex->ullTotalPageFile = total*1024;
	    if (sscanf(buffer, "SwapFree: %llu", &free))
	        lpmemex->ullAvailPageFile = free*1024;
	    if (sscanf(buffer, "Buffers: %llu", &buffers))
	        lpmemex->ullAvailPhys += buffers*1024;
	    if (sscanf(buffer, "Cached: %llu", &cached))
	        lpmemex->ullAvailPhys += cached*1024;
        }
        fclose( f );

        lpmemex->ullTotalPageFile += lpmemex->ullTotalPhys;
        lpmemex->ullAvailPageFile += lpmemex->ullAvailPhys;

        if (lpmemex->ullTotalPhys)
           lpmemex->dwMemoryLoad =
              (lpmemex->ullTotalPhys - lpmemex->ullAvailPhys) /
              (lpmemex->ullTotalPhys / 100);
    } else
#elif defined(__FreeBSD__)
    size_sys = sizeof (tmp);
    if (sysctlbyname ("hw.physmem", &tmp, &size_sys, NULL, 0) == 0)
    {
       lpmemex->ullTotalPhys = tmp;
       if (sysctlbyname ("hw.usermem", &tmp, &size_sys, NULL, 0) == 0)
       {
          lpmemex->ullAvailPhys = tmp;
          lpmemex->ullTotalPageFile = tmp;
          lpmemex->ullAvailPageFile = tmp;
          lpmemex->dwMemoryLoad =
             (lpmemex->ullTotalPhys - lpmemex->ullAvailPhys) /
             (lpmemex->ullTotalPhys / 100);
       }
       else
       {
          lpmemex->ullAvailPhys = lpmemex->ullTotalPhys;
          lpmemex->ullTotalPageFile = lpmemex->ullTotalPhys;
          lpmemex->ullAvailPageFile = lpmemex->ullTotalPhys;
          lpmemex->dwMemoryLoad = 0;
       }
    } else
#elif defined(__APPLE__)
    count = sizeof (vm_stat) / sizeof (natural_t);
    error = host_statistics (mach_host_self (), HOST_VM_INFO,
                             (host_info_t)&vm_stat, &count);
    if (error == KERN_SUCCESS) {
       vm_size_t page_size;
       struct xsw_usage swap_usage;
       int mib[2];
       size_t len;

       host_page_size (mach_host_self (), &page_size);

       lpmemex->ullAvailPhys = ((DWORDLONG)vm_stat.free_count +
                                vm_stat.inactive_count) * page_size;

       lpmemex->ullTotalPhys = lpmemex->ullAvailPhys +
          ((DWORDLONG)vm_stat.active_count + vm_stat.wire_count) *
          page_size;

       lpmemex->dwMemoryLoad =
          (lpmemex->ullTotalPhys - lpmemex->ullAvailPhys) /
          (lpmemex->ullTotalPhys / 100);

       mib[0] = CTL_VM;
       mib[1] = VM_SWAPUSAGE;
       len = sizeof (swap_usage);
       if (sysctl (mib, 2, &swap_usage, &len, NULL, 0) == 0)
       {
          lpmemex->ullTotalPageFile = swap_usage.xsu_total +
             lpmemex->ullTotalPhys;
          lpmemex->ullAvailPageFile = swap_usage.xsu_avail +
             lpmemex->ullAvailPhys;
       }
       else
       {
          ERR ("Failed to get swap space statistics!\n");
          lpmemex->ullTotalPageFile = lpmemex->ullTotalPhys;
          lpmemex->ullAvailPageFile = lpmemex->ullTotalPhys;
       }
    } else
#endif
    {
        ERR ("Unable to get system memory information!\n");
	/* FIXME: should do something for other systems */
	lpmemex->dwMemoryLoad    = 0;
	lpmemex->ullTotalPhys     = 16*1024*1024;
	lpmemex->ullAvailPhys     = 16*1024*1024;
	lpmemex->ullTotalPageFile = 16*1024*1024;
	lpmemex->ullAvailPageFile = 16*1024*1024;
    }

    GetSystemInfo(&si);
    lpmemex->ullTotalVirtual  = si.lpMaximumApplicationAddress-si.lpMinimumApplicationAddress;
    /* FIXME: we should track down all the already allocated VM pages and substract them, for now arbitrarily remove 64KB so that it matches NT */
    lpmemex->ullAvailVirtual  = lpmemex->ullTotalVirtual-64*1024;
    lpmemex->ullAvailExtendedVirtual = 0;

    memcpy(&cached_memstatus,lpmemex,sizeof(cached_memstatus));

    /* it appears some memory display programs want to divide by these values */
    if(lpmemex->ullTotalPageFile==0)
        lpmemex->ullTotalPageFile++;

    if(lpmemex->ullAvailPageFile==0)
        lpmemex->ullAvailPageFile++;

    TRACE("<-- LPMEMORYSTATUSEX: dwLength %lu, dwMemoryLoad %lu, ullTotalPhys %lx%08lx, ullAvailPhys %lx%08lx, ullTotalPageFile %lx%08lx, ullAvailPageFile %lx%08lx, ullTotalVirtual %lx%08lx, ullAvailVirtual %lx%08lx ullAvailExtendedVirtual %lx%08lx\n", 
           lpmemex->dwLength, lpmemex->dwMemoryLoad, HI_LONG(lpmemex->ullTotalPhys), LOW_LONG(lpmemex->ullTotalPhys), HI_LONG(lpmemex->ullAvailPhys), LOW_LONG(lpmemex->ullAvailPhys), HI_LONG(lpmemex->ullTotalPageFile), LOW_LONG(lpmemex->ullTotalPageFile), HI_LONG(lpmemex->ullAvailPageFile), LOW_LONG(lpmemex->ullAvailPageFile), HI_LONG(lpmemex->ullTotalVirtual), LOW_LONG(lpmemex->ullTotalVirtual), HI_LONG(lpmemex->ullAvailVirtual), LOW_LONG(lpmemex->ullAvailVirtual), HI_LONG(lpmemex->ullAvailExtendedVirtual), LOW_LONG(lpmemex->ullAvailExtendedVirtual));

    return TRUE;
}

/***********************************************************************
 *           GlobalMemoryStatus   (KERNEL32.@)
 * RETURNS
 *      TRUE
 */
VOID WINAPI GlobalMemoryStatus( LPMEMORYSTATUS lpmem)
{
    MEMORYSTATUSEX ms;
    OSVERSIONINFOA osver;
    GlobalMemoryStatusEx( &ms );

    lpmem->dwLength = sizeof(MEMORYSTATUS);

    lpmem->dwMemoryLoad = ms.dwMemoryLoad;

    /* NT reports the incorrect value ie value modulo 4GB so the
     * truncation should be correct. Windows 2000 and newer, however,
     * report the overflow with -1 in these fields.
     */

    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA( &osver );

    if( osver.dwMajorVersion >= 5 )
    {
        lpmem->dwTotalPhys = HI_LONG(ms.ullTotalPhys) ? -1 : LOW_LONG(ms.ullTotalPhys);
        lpmem->dwAvailPhys = HI_LONG(ms.ullAvailPhys) ? -1 : LOW_LONG(ms.ullAvailPhys);
        lpmem->dwTotalPageFile = HI_LONG(ms.ullTotalPageFile) ? -1 : LOW_LONG(ms.ullTotalPageFile);
        lpmem->dwAvailPageFile = HI_LONG(ms.ullAvailPageFile) ? -1 : LOW_LONG(ms.ullAvailPageFile);
        lpmem->dwTotalVirtual = HI_LONG(ms.ullTotalVirtual) ? -1 : LOW_LONG(ms.ullTotalVirtual);
        lpmem->dwAvailVirtual = HI_LONG(ms.ullAvailVirtual) ? -1 : LOW_LONG(ms.ullAvailVirtual);
    }
    else
    {
        lpmem->dwTotalPhys = (DWORD)ms.ullTotalPhys;
        lpmem->dwAvailPhys = (DWORD)ms.ullAvailPhys;
        lpmem->dwTotalPageFile = (DWORD)ms.ullTotalPageFile;
        lpmem->dwAvailPageFile = (DWORD)ms.ullAvailPageFile;
        lpmem->dwTotalVirtual =(DWORD) ms.ullTotalVirtual;
        lpmem->dwAvailVirtual =(DWORD) ms.ullAvailVirtual;
    }

    /* According to MSDN, need to handle values between 2G and 4G by
       pretending there's only 2G */
    if ((lpmem->dwTotalPhys > MAXLONG) && (lpmem->dwTotalPhys < MAXDWORD))
       lpmem->dwTotalPhys = MAXLONG;
    if ((lpmem->dwAvailPhys > MAXLONG) && (lpmem->dwAvailPhys < MAXDWORD))
       lpmem->dwAvailPhys = MAXLONG;

    TRACE ("Total mem: %lu Avail mem: %lu\n",
           lpmem->dwTotalPhys, lpmem->dwAvailPhys);
}

/***********************************************************************
 *           A20Proc   (KERNEL.165)
 *           A20_Proc  (SYSTEM.20)
 */
void WINAPI A20Proc16( WORD unused )
{
    /* this is also a NOP in Windows */
}

/***********************************************************************
 *           LimitEMSPages   (KERNEL.156)
 */
DWORD WINAPI LimitEMSPages16( DWORD unused )
{
    return 0;
}
