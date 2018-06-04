/*
 * Win32 heap functions
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright 1998 Ulrich Weigand
 * Copyright (c) 2003-2011 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)

 */

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "winternl.h"
#include "wine/winbase16.h"
#include "winbase.h"
#include "winerror.h"
#include "winnt.h"
#include "wine/heapstr.h"
#include "wine/thread.h"
#include "heapfuncs.h"
#include "wine/debug.h"
#include "wine/profile.h"

WINE_DEFAULT_DEBUG_CHANNEL(heap);

/* uncomment the following to enable profiling of the heap
 * NOTE: this should *NOT* be left on, as it will not work on some non-pthread 
 * systems (wine_get_timer_ref needs to allocate memory, causes the pthread emulation
 * to get initialized, which allocates memory using RtlAllocateHeap -> boom!) */
/* #define HEAP_PROFILE_ON */

/* Note: the heap data structures are based on what Pietrek describes in his
 * book 'Windows 95 System Programming Secrets'. The layout is not exactly
 * the same, but could be easily adapted if it turns out some programs
 * require it.
 */

typedef struct tagARENA_INUSE
{
    DWORD  size;                    /* Block size; must be the first field */
    DWORD  magic;                   /* Magic number */
} ARENA_INUSE;

typedef struct tagARENA_FREE
{
    DWORD                 size;     /* Block size; must be the first field */
    DWORD                 magic;    /* Magic number */
    struct tagARENA_FREE *next;     /* Next free arena */
    struct tagARENA_FREE *prev;     /* Prev free arena */
} ARENA_FREE;

#define ARENA_FLAG_FREE        0x00000001  /* flags OR'ed with arena size */
#define ARENA_FLAG_PREV_FREE   0x00000002
#define ARENA_SIZE_MASK        (~3)
#define ARENA_INUSE_MAGIC      0x44455355      /* Value for arena 'magic' field */
#define ARENA_FREE_MAGIC       0x45455246      /* Value for arena 'magic' field */

#define ARENA_INUSE_FILLER     0x55
#define ARENA_FREE_FILLER      0xaa

#define QUIET                  1           /* Suppress messages  */
#define NOISY                  0           /* Report all errors  */

#define FREE_BLOCK_INSERTION_THRESHOLD	4096

#define HEAP_NB_FREE_LISTS   4   /* Number of free lists */

/* Max size of the blocks on the free lists */
static const DWORD HEAP_freeListSizes[HEAP_NB_FREE_LISTS] =
{
    0x20, 0x80, 0x200, ~0UL
};

typedef struct
{
    DWORD       size;
    ARENA_FREE  arena;
} FREE_LIST_ENTRY;

struct tagHEAP;

typedef struct tagSUBHEAP
{
    DWORD               size;       /* Size of the whole sub-heap */
    DWORD               commitSize; /* Committed size of the sub-heap */
    DWORD               headerSize; /* Size of the heap header */
    struct tagSUBHEAP  *next;       /* Next sub-heap */
    struct tagHEAP     *heap;       /* Main heap structure */
    DWORD               magic;      /* Magic number */
} SUBHEAP;

#define SUBHEAP_MAGIC    ((DWORD)('S' | ('U'<<8) | ('B'<<16) | ('H'<<24)))

typedef struct tagHEAP
{
    SUBHEAP          subheap;       /* First sub-heap */
    struct tagHEAP  *next;          /* Next heap for this process */
    CRITICAL_SECTION critSection;   /* Critical section for serialization */
    FREE_LIST_ENTRY  freeList[HEAP_NB_FREE_LISTS];  /* Free lists */
    DWORD            flags;         /* Heap flags */
    DWORD            magic;         /* Magic number */
} HEAP;

#define HEAP_MAGIC       ((DWORD)('H' | ('E'<<8) | ('A'<<16) | ('P'<<24)))

#define HEAP_DEF_SIZE        0x110000   /* Default heap size = 1Mb + 64Kb */
#define HEAP_MIN_BLOCK_SIZE  (8+sizeof(ARENA_FREE))  /* Min. heap block size */
#define COMMIT_MASK          0xffff  /* bitmask for commit/decommit granularity */

static HANDLE processHeap;  /* main process heap */

static HEAP *firstHeap;     /* head of secondary heaps list */

static BOOL HEAP_IsRealArena( HEAP *heapPtr, DWORD flags, LPCVOID block, BOOL quiet );

/* SetLastError for ntdll */
inline static void set_status( NTSTATUS status )
{
#if defined(__i386__) && defined(__GNUC__)
    /* in this case SetLastError is an inline function so we can use it */
    SetLastError( RtlNtStatusToDosError( status ) );
#else
    /* cannot use SetLastError, do it manually */
    NtCurrentTeb()->last_error = RtlNtStatusToDosError( status );
#endif
}

/***********************************************************************
 *           HEAP_Dump
 */
static void HEAP_Dump( HEAP *heap )
{
    int i;
    SUBHEAP *subheap;
    char *ptr;

    DPRINTF( "Heap: %08lx\n", (DWORD)heap );
    DPRINTF( "Next: %08lx  Sub-heaps: %08lx",
	  (DWORD)heap->next, (DWORD)&heap->subheap );
    subheap = &heap->subheap;
    while (subheap->next)
    {
        DPRINTF( " -> %08lx", (DWORD)subheap->next );
        subheap = subheap->next;
    }

    DPRINTF( "\nFree lists:\n Block   Stat   Size    Id\n" );
    for (i = 0; i < HEAP_NB_FREE_LISTS; i++)
        DPRINTF( "%08lx free %08lx prev=%08lx next=%08lx\n",
	      (DWORD)&heap->freeList[i].arena, heap->freeList[i].arena.size,
	      (DWORD)heap->freeList[i].arena.prev,
	      (DWORD)heap->freeList[i].arena.next );

    subheap = &heap->subheap;
    while (subheap)
    {
        DWORD freeSize = 0, usedSize = 0, arenaSize = subheap->headerSize;
        DPRINTF( "\n\nSub-heap %08lx: size=%08lx committed=%08lx\n",
	      (DWORD)subheap, subheap->size, subheap->commitSize );

        DPRINTF( "\n Block   Stat   Size    Id\n" );
        ptr = (char*)subheap + subheap->headerSize;
        while (ptr < (char *)subheap + subheap->size)
        {
            if (*(DWORD *)ptr & ARENA_FLAG_FREE)
            {
                ARENA_FREE *pArena = (ARENA_FREE *)ptr;
                DPRINTF( "%08lx free %08lx prev=%08lx next=%08lx\n",
		      (DWORD)pArena, pArena->size & ARENA_SIZE_MASK,
		      (DWORD)pArena->prev, (DWORD)pArena->next);
                ptr += sizeof(*pArena) + (pArena->size & ARENA_SIZE_MASK);
                arenaSize += sizeof(ARENA_FREE);
                freeSize += pArena->size & ARENA_SIZE_MASK;
            }
            else if (*(DWORD *)ptr & ARENA_FLAG_PREV_FREE)
            {
                ARENA_INUSE *pArena = (ARENA_INUSE *)ptr;
                DPRINTF( "%08lx Used %08lx back=%08lx\n",
                         (DWORD)pArena, pArena->size & ARENA_SIZE_MASK, *((DWORD *)pArena - 1) );
                ptr += sizeof(*pArena) + (pArena->size & ARENA_SIZE_MASK);
                arenaSize += sizeof(ARENA_INUSE);
                usedSize += pArena->size & ARENA_SIZE_MASK;
            }
            else
            {
                ARENA_INUSE *pArena = (ARENA_INUSE *)ptr;
                DPRINTF( "%08lx used %08lx\n",
		      (DWORD)pArena, pArena->size & ARENA_SIZE_MASK );
                ptr += sizeof(*pArena) + (pArena->size & ARENA_SIZE_MASK);
                arenaSize += sizeof(ARENA_INUSE);
                usedSize += pArena->size & ARENA_SIZE_MASK;
            }
        }
        DPRINTF( "\nTotal: Size=%08lx Committed=%08lx Free=%08lx Used=%08lx Arenas=%08lx (%ld%%)\n\n",
	      subheap->size, subheap->commitSize, freeSize, usedSize,
	      arenaSize, (arenaSize * 100) / subheap->size );
        subheap = subheap->next;
    }
}


/***********************************************************************
 *           HEAP_GetPtr
 * RETURNS
 *	Pointer to the heap
 *	NULL: Failure
 */
static HEAP *HEAP_GetPtr(
             HANDLE heap /* [in] Handle to the heap */
) {
    HEAP *heapPtr = (HEAP *)heap;
    if (!heapPtr || (heapPtr->magic != HEAP_MAGIC))
    {
        ERR("Invalid heap %08x!\n", heap );
        return NULL;
    }
    if (TRACE_ON(heap) && !HEAP_IsRealArena( heapPtr, 0, NULL, NOISY ))
    {
        HEAP_Dump( heapPtr );
        assert( FALSE );
        return NULL;
    }
    return heapPtr;
}


/***********************************************************************
 *           HEAP_InsertFreeBlockAtFront
 *
 * Insert a free block into the front of the free list.
 */
static void HEAP_InsertFreeBlockAtFront( HEAP *heap, ARENA_FREE *pArena )
{
    FREE_LIST_ENTRY *pEntry = heap->freeList;
    while (pEntry->size < pArena->size) pEntry++;
    pArena->size      |= ARENA_FLAG_FREE;
    pArena->next       = pEntry->arena.next;
    pArena->next->prev = pArena;
    pArena->prev       = &pEntry->arena;
    pEntry->arena.next = pArena;
}

/***********************************************************************
 *           HEAP_InsertFreeBlockAtEnd
 *
 * Insert a free block into the end of the free list.
 */
static void HEAP_InsertFreeBlockAtEnd( HEAP *heap, ARENA_FREE *pArena )
{
    FREE_LIST_ENTRY *pEntry = heap->freeList;

    /* Determine which free list contains blocks of the appropriate size. */
    while (pEntry->size < pArena->size) pEntry++;

    /* But we want to put this block on the end of said appropriate free list.
     * So instead, find the list for the next size up, knowing that they are
     * all connected. */
    if (++pEntry == heap->freeList+HEAP_NB_FREE_LISTS)
        pEntry = heap->freeList;

    pArena->size      |= ARENA_FLAG_FREE;
    pArena->next       = &pEntry->arena;
    pArena->prev       = pEntry->arena.prev;

    pArena->next->prev = pArena;
    pArena->prev->next = pArena;
}

/***********************************************************************
 *           HEAP_FindSubHeap
 * Find the sub-heap containing a given address.
 *
 * RETURNS
 *	Pointer: Success
 *	NULL: Failure
 */
static SUBHEAP *HEAP_FindSubHeap(
                HEAP *heap, /* [in] Heap pointer */
                LPCVOID ptr /* [in] Address */
) {
    SUBHEAP *sub = &heap->subheap;
    while (sub)
    {
        if (((char *)ptr >= (char *)sub) &&
            ((char *)ptr < (char *)sub + sub->size)) return sub;
        sub = sub->next;
    }
    return NULL;
}


/***********************************************************************
 *           HEAP_Commit
 *
 * Make sure the heap storage is committed up to (not including) ptr.
 */
static inline BOOL HEAP_Commit( SUBHEAP *subheap, void *ptr )
{
    DWORD size = (DWORD)((char *)ptr - (char *)subheap);
    size = (size + COMMIT_MASK) & ~COMMIT_MASK;
    if (size > subheap->size) size = subheap->size;
    if (size <= subheap->commitSize) return TRUE;
    if (!VirtualAlloc( (char *)subheap + subheap->commitSize,
                       size - subheap->commitSize, MEM_COMMIT,
                       PAGE_EXECUTE_READWRITE))
    {
        WARN("Could not commit %08lx bytes at %08lx for heap %08lx\n",
                 size - subheap->commitSize,
                 (DWORD)((char *)subheap + subheap->commitSize),
                 (DWORD)subheap->heap );
        return FALSE;
    }
    subheap->commitSize = size;
    return TRUE;
}


/***********************************************************************
 *           HEAP_Decommit
 *
 * If possible, decommit the heap storage from (including) 'ptr'.
 */
static inline BOOL HEAP_Decommit( SUBHEAP *subheap, void *ptr )
{
    DWORD size = (DWORD)((char *)ptr - (char *)subheap);
    /* round to next block and add one full block */
    size = ((size + COMMIT_MASK) & ~COMMIT_MASK) + COMMIT_MASK + 1;
    if (size >= subheap->commitSize) return TRUE;
    if (!VirtualFree( (char *)subheap + size,
                      subheap->commitSize - size, MEM_DECOMMIT ))
    {
        WARN("Could not decommit %08lx bytes at %08lx for heap %08lx\n",
                 subheap->commitSize - size,
                 (DWORD)((char *)subheap + size),
                 (DWORD)subheap->heap );
        return FALSE;
    }
    subheap->commitSize = size;
    return TRUE;
}


/***********************************************************************
 *           HEAP_CreateFreeBlock
 *
 * Create a free block at a specified address. 'size' is the size of the
 * whole block, including the new arena.
 */
static void HEAP_CreateFreeBlock( SUBHEAP *subheap, void *ptr, DWORD size )
{
    ARENA_FREE *pFree;

    /* Create a free arena */

    pFree = (ARENA_FREE *)ptr;
    pFree->magic = ARENA_FREE_MAGIC;

    /* If debugging, erase the freed block content */

    if (TRACE_ON(heap))
    {
        char *pEnd = (char *)ptr + size;
        if (pEnd > (char *)subheap + subheap->commitSize)
            pEnd = (char *)subheap + subheap->commitSize;
        if (pEnd > (char *)(pFree + 1))
            memset( pFree + 1, ARENA_FREE_FILLER, pEnd - (char *)(pFree + 1) );
    }

    /* Check if next block is free also */

    if (((char *)ptr + size < (char *)subheap + subheap->size) &&
        (*(DWORD *)((char *)ptr + size) & ARENA_FLAG_FREE))
    {
        /* Remove the next arena from the free list */
        ARENA_FREE *pNext = (ARENA_FREE *)((char *)ptr + size);
        pNext->next->prev = pNext->prev;
        pNext->prev->next = pNext->next;
        size += (pNext->size & ARENA_SIZE_MASK) + sizeof(*pNext);
        if (TRACE_ON(heap))
            memset( pNext, ARENA_FREE_FILLER, sizeof(ARENA_FREE) );
    }

    /* Set the next block PREV_FREE flag and pointer */

    if ((char *)ptr + size < (char *)subheap + subheap->size)
    {
        DWORD *pNext = (DWORD *)((char *)ptr + size);
        *pNext |= ARENA_FLAG_PREV_FREE;
        *(ARENA_FREE **)(pNext - 1) = pFree;
    }

    /* Last, insert the new block into the free list */

    pFree->size = size - sizeof(*pFree);

    if ((pFree->size & ARENA_SIZE_MASK) > FREE_BLOCK_INSERTION_THRESHOLD)
        HEAP_InsertFreeBlockAtEnd( subheap->heap, pFree );
    else
        HEAP_InsertFreeBlockAtFront( subheap->heap, pFree );
}


/***********************************************************************
 *           HEAP_MakeInUseBlockFree
 *
 * Turn an in-use block into a free block. Can also decommit the end of
 * the heap, and possibly even free the sub-heap altogether.
 */
static void HEAP_MakeInUseBlockFree( SUBHEAP *subheap, ARENA_INUSE *pArena )
{
    ARENA_FREE *pFree;
    DWORD size = (pArena->size & ARENA_SIZE_MASK) + sizeof(*pArena);

    /* Check if we can merge with previous block */

    if (pArena->size & ARENA_FLAG_PREV_FREE)
    {
        pFree = *((ARENA_FREE **)pArena - 1);
        size += (pFree->size & ARENA_SIZE_MASK) + sizeof(ARENA_FREE);
        /* Remove it from the free list */
        pFree->next->prev = pFree->prev;
        pFree->prev->next = pFree->next;
    }
    else pFree = (ARENA_FREE *)pArena;

    /* Create a free block */

    HEAP_CreateFreeBlock( subheap, pFree, size );
    size = (pFree->size & ARENA_SIZE_MASK) + sizeof(ARENA_FREE);
    if ((char *)pFree + size < (char *)subheap + subheap->size)
        return;  /* Not the last block, so nothing more to do */

    /* Free the whole sub-heap if it's empty and not the original one */

    if (((char *)pFree == (char *)subheap + subheap->headerSize) &&
        (subheap != &subheap->heap->subheap))
    {
        SUBHEAP *pPrev = &subheap->heap->subheap;
        /* Remove the free block from the list */
        pFree->next->prev = pFree->prev;
        pFree->prev->next = pFree->next;
        /* Remove the subheap from the list */
        while (pPrev && (pPrev->next != subheap)) pPrev = pPrev->next;
        if (pPrev) pPrev->next = subheap->next;
        /* Free the memory */
        subheap->magic = 0;
        VirtualFree( subheap, 0, MEM_RELEASE );
        return;
    }

    /* Decommit the end of the heap */

    if (!(subheap->heap->flags & HEAP_SHARED)) HEAP_Decommit( subheap, pFree + 1 );
}


/***********************************************************************
 *           HEAP_ShrinkBlock
 *
 * Shrink an in-use block.
 */
static void HEAP_ShrinkBlock(SUBHEAP *subheap, ARENA_INUSE *pArena, DWORD size)
{
    if ((pArena->size & ARENA_SIZE_MASK) >= size + HEAP_MIN_BLOCK_SIZE)
    {
        HEAP_CreateFreeBlock( subheap, (char *)(pArena + 1) + size,
                              (pArena->size & ARENA_SIZE_MASK) - size );
	/* assign size plus previous arena flags */
        pArena->size = size | (pArena->size & ~ARENA_SIZE_MASK);
    }
    else
    {
        /* Turn off PREV_FREE flag in next block */
        char *pNext = (char *)(pArena + 1) + (pArena->size & ARENA_SIZE_MASK);
        if (pNext < (char *)subheap + subheap->size)
            *(DWORD *)pNext &= ~ARENA_FLAG_PREV_FREE;
    }
}

/***********************************************************************
 *           HEAP_InitSubHeap
 */
static BOOL HEAP_InitSubHeap( HEAP *heap, LPVOID address, DWORD flags,
                                DWORD commitSize, DWORD totalSize )
{
    SUBHEAP *subheap = (SUBHEAP *)address;
    FREE_LIST_ENTRY *pEntry;
    int i;

    /* Commit memory */

    if (flags & HEAP_SHARED)
        commitSize = totalSize;  /* always commit everything in a shared heap */
    if (!VirtualAlloc(address, commitSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE))
    {
        WARN("Could not commit %08lx bytes for sub-heap %08lx\n",
                   commitSize, (DWORD)address );
        return FALSE;
    }

    /* Fill the sub-heap structure */

    subheap->heap       = heap;
    subheap->size       = totalSize;
    subheap->commitSize = commitSize;
    subheap->magic      = SUBHEAP_MAGIC;

    if ( subheap != (SUBHEAP *)heap )
    {
        /* If this is a secondary subheap, insert it into list */

        subheap->headerSize = sizeof(SUBHEAP);
        subheap->next       = heap->subheap.next;
        heap->subheap.next  = subheap;
    }
    else
    {
        /* If this is a primary subheap, initialize main heap */

        subheap->headerSize = sizeof(HEAP);
        subheap->next       = NULL;
        heap->next          = NULL;
        heap->flags         = flags;
        heap->magic         = HEAP_MAGIC;

        /* Build the free lists */

        for (i = 0, pEntry = heap->freeList; i < HEAP_NB_FREE_LISTS; i++, pEntry++)
        {
            pEntry->size       = HEAP_freeListSizes[i];
            pEntry->arena.size = 0 | ARENA_FLAG_FREE;
            pEntry->arena.next = i < HEAP_NB_FREE_LISTS-1 ?
                                 &heap->freeList[i+1].arena : &heap->freeList[0].arena;
            pEntry->arena.prev = i ? &heap->freeList[i-1].arena :
                                  &heap->freeList[HEAP_NB_FREE_LISTS-1].arena;
            pEntry->arena.magic = ARENA_FREE_MAGIC;
        }

        /* Initialize critical section */

        RtlInitializeCriticalSection( &heap->critSection );
    }

    /* Create the first free block */

    HEAP_CreateFreeBlock( subheap, (LPBYTE)subheap + subheap->headerSize,
                          subheap->size - subheap->headerSize );

    return TRUE;
}

/***********************************************************************
 *           HEAP_CreateSubHeap
 *
 * Create a sub-heap of the given size.
 * If heap == NULL, creates a main heap.
 */
static SUBHEAP *HEAP_CreateSubHeap( HEAP *heap, void *base, DWORD flags,
                                    DWORD commitSize, DWORD totalSize )
{
    LPVOID address = base;

    if (!address)
    {
        /* round-up sizes on a 64K boundary */
        totalSize  = (totalSize + 0xffff) & 0xffff0000;
        commitSize = (commitSize + 0xffff) & 0xffff0000;
        if (!commitSize) commitSize = 0x10000;
        if (totalSize < commitSize) totalSize = commitSize;

        /* allocate the memory block */
        if (!(address = VirtualAlloc( NULL, totalSize, MEM_RESERVE, PAGE_EXECUTE_READWRITE )))
        {
            WARN("Could not VirtualAlloc %08lx bytes\n",
                 totalSize );
            return NULL;
        }
    }

    /* Initialize subheap */

    if (!HEAP_InitSubHeap( heap ? heap : (HEAP *)address,
                           address, flags, commitSize, totalSize ))
    {
        if (!base) VirtualFree( address, 0, MEM_RELEASE );
        return NULL;
    }

    return (SUBHEAP *)address;
}


/***********************************************************************
 *           HEAP_FindFreeBlock
 *
 * Find a free block at least as large as the requested size, and make sure
 * the requested size is committed.
 */
static ARENA_FREE *HEAP_FindFreeBlock( HEAP *heap, DWORD size,
                                       SUBHEAP **ppSubHeap )
{
    SUBHEAP *subheap;
    ARENA_FREE *pArena;
    FREE_LIST_ENTRY *pEntry = heap->freeList;

    /* Find a suitable free list, and in it find a block large enough */

    while (pEntry->size < size) pEntry++;
    pArena = pEntry->arena.next;
    while (pArena != &heap->freeList[0].arena)
    {
        DWORD arena_size = (pArena->size & ARENA_SIZE_MASK) +
                            sizeof(ARENA_FREE) - sizeof(ARENA_INUSE);
        if (arena_size >= size)
        {
            subheap = HEAP_FindSubHeap( heap, pArena );
            if (!HEAP_Commit( subheap, (char *)pArena + sizeof(ARENA_INUSE)
                                               + size + HEAP_MIN_BLOCK_SIZE))
                return NULL;
            *ppSubHeap = subheap;
            return pArena;
        }
        pArena = pArena->next;
    }

    /* If no block was found, attempt to grow the heap */

    if (!(heap->flags & HEAP_GROWABLE))
    {
        WARN("Not enough space in heap %08lx for %08lx bytes\n",
                 (DWORD)heap, size );
        return NULL;
    }
    /* make sure that we have a big enough size *committed* to fit another
     * last free arena in !
     * So just one heap struct, one first free arena which will eventually
     * get inuse, and HEAP_MIN_BLOCK_SIZE for the second free arena that
     * might get assigned all remaining free space in HEAP_ShrinkBlock() */
    size += sizeof(SUBHEAP) + sizeof(ARENA_INUSE) + HEAP_MIN_BLOCK_SIZE;
    if (!(subheap = HEAP_CreateSubHeap( heap, NULL, heap->flags, size,
                                        max( HEAP_DEF_SIZE, size ) )))
        return NULL;

    TRACE("created new sub-heap %08lx of %08lx bytes for heap %08lx\n",
                (DWORD)subheap, size, (DWORD)heap );

    *ppSubHeap = subheap;
    return (ARENA_FREE *)(subheap + 1);
}


/***********************************************************************
 *           HEAP_IsValidArenaPtr
 *
 * Check that the pointer is inside the range possible for arenas.
 */
static BOOL HEAP_IsValidArenaPtr( HEAP *heap, void *ptr )
{
    int i;
    SUBHEAP *subheap = HEAP_FindSubHeap( heap, ptr );
    if (!subheap) return FALSE;
    if ((char *)ptr >= (char *)subheap + subheap->headerSize) return TRUE;
    if (subheap != &heap->subheap) return FALSE;
    for (i = 0; i < HEAP_NB_FREE_LISTS; i++)
        if (ptr == (void *)&heap->freeList[i].arena) return TRUE;
    return FALSE;
}


/***********************************************************************
 *           HEAP_ValidateFreeArena
 */
static BOOL HEAP_ValidateFreeArena( SUBHEAP *subheap, ARENA_FREE *pArena )
{
    char *heapEnd = (char *)subheap + subheap->size;

#if !defined(ALLOW_UNALIGNED_ACCESS)
    /* Check for unaligned pointers */
    if ( (long)pArena % sizeof(void *) != 0 )
    {
        ERR( "Heap %08lx: unaligned arena pointer %08lx\n",
             (DWORD)subheap->heap, (DWORD)pArena );
        return FALSE;
    }
#endif

    /* Check magic number */
    if (pArena->magic != ARENA_FREE_MAGIC)
    {
        ERR("Heap %08lx: invalid free arena magic for %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena );
        return FALSE;
    }
    /* Check size flags */
    if (!(pArena->size & ARENA_FLAG_FREE) ||
        (pArena->size & ARENA_FLAG_PREV_FREE))
    {
        ERR("Heap %08lx: bad flags %lx for free arena %08lx\n",
                 (DWORD)subheap->heap, pArena->size & ~ARENA_SIZE_MASK, (DWORD)pArena );
    }
    /* Check arena size */
    if ((char *)(pArena + 1) + (pArena->size & ARENA_SIZE_MASK) > heapEnd)
    {
        ERR("Heap %08lx: bad size %08lx for free arena %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena->size & ARENA_SIZE_MASK, (DWORD)pArena );
        return FALSE;
    }
    /* Check that next pointer is valid */
    if (!HEAP_IsValidArenaPtr( subheap->heap, pArena->next ))
    {
        ERR("Heap %08lx: bad next ptr %08lx for arena %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena->next, (DWORD)pArena );
        return FALSE;
    }
    /* Check that next arena is free */
    if (!(pArena->next->size & ARENA_FLAG_FREE) ||
        (pArena->next->magic != ARENA_FREE_MAGIC))
    {
        ERR("Heap %08lx: next arena %08lx invalid for %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena->next, (DWORD)pArena );
        return FALSE;
    }
    /* Check that prev pointer is valid */
    if (!HEAP_IsValidArenaPtr( subheap->heap, pArena->prev ))
    {
        ERR("Heap %08lx: bad prev ptr %08lx for arena %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena->prev, (DWORD)pArena );
        return FALSE;
    }
    /* Check that prev arena is free */
    if (!(pArena->prev->size & ARENA_FLAG_FREE) ||
        (pArena->prev->magic != ARENA_FREE_MAGIC))
    {
	/* this often means that the prev arena got overwritten
	 * by a memory write before that prev arena */
        ERR("Heap %08lx: prev arena %08lx invalid for %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena->prev, (DWORD)pArena );
        return FALSE;
    }
    /* Check that next block has PREV_FREE flag */
    if ((char *)(pArena + 1) + (pArena->size & ARENA_SIZE_MASK) < heapEnd)
    {
        if (!(*(DWORD *)((char *)(pArena + 1) +
            (pArena->size & ARENA_SIZE_MASK)) & ARENA_FLAG_PREV_FREE))
        {
            ERR("Heap %08lx: free arena %08lx next block has no PREV_FREE flag\n",
                     (DWORD)subheap->heap, (DWORD)pArena );
            return FALSE;
        }
        /* Check next block back pointer */
        if (*((ARENA_FREE **)((char *)(pArena + 1) +
            (pArena->size & ARENA_SIZE_MASK)) - 1) != pArena)
        {
            ERR("Heap %08lx: arena %08lx has wrong back ptr %08lx\n",
                     (DWORD)subheap->heap, (DWORD)pArena,
                     *((DWORD *)((char *)(pArena+1)+ (pArena->size & ARENA_SIZE_MASK)) - 1));
            return FALSE;
        }
    }
    return TRUE;
}


/***********************************************************************
 *           HEAP_ValidateInUseArena
 */
static BOOL HEAP_ValidateInUseArena( SUBHEAP *subheap, ARENA_INUSE *pArena, BOOL quiet )
{
    char *heapEnd = (char *)subheap + subheap->size;

#if !defined(ALLOW_UNALIGNED_ACCESS)
    /* Check for unaligned pointers */
    if ( (long)pArena % sizeof(void *) != 0 )
    {
        if ( quiet == NOISY )
        {
            ERR( "Heap %08lx: unaligned arena pointer %08lx\n",
                  (DWORD)subheap->heap, (DWORD)pArena );
            if ( TRACE_ON(heap) )
                HEAP_Dump( subheap->heap );
        }
        else if ( WARN_ON(heap) )
        {
            WARN( "Heap %08lx: unaligned arena pointer %08lx\n",
                  (DWORD)subheap->heap, (DWORD)pArena );
            if ( TRACE_ON(heap) )
                HEAP_Dump( subheap->heap );
        }
        return FALSE;
    }
#endif

    /* Check magic number */
    if (pArena->magic != ARENA_INUSE_MAGIC)
    {
        if (quiet == NOISY) {
        ERR("Heap %08lx: invalid in-use arena magic for %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena );
            if (TRACE_ON(heap))
               HEAP_Dump( subheap->heap );
        }  else if (WARN_ON(heap)) {
            WARN("Heap %08lx: invalid in-use arena magic for %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena );
            if (TRACE_ON(heap))
               HEAP_Dump( subheap->heap );
        }
        return FALSE;
    }
    /* Check size flags */
    if (pArena->size & ARENA_FLAG_FREE)
    {
        ERR("Heap %08lx: bad flags %lx for in-use arena %08lx\n",
                 (DWORD)subheap->heap, pArena->size & ~ARENA_SIZE_MASK, (DWORD)pArena );
	return FALSE;
    }
    /* Check arena size */
    if ((char *)(pArena + 1) + (pArena->size & ARENA_SIZE_MASK) > heapEnd)
    {
        ERR("Heap %08lx: bad size %08lx for in-use arena %08lx\n",
                 (DWORD)subheap->heap, (DWORD)pArena->size & ARENA_SIZE_MASK, (DWORD)pArena );
        return FALSE;
    }
    /* Check next arena PREV_FREE flag */
    if (((char *)(pArena + 1) + (pArena->size & ARENA_SIZE_MASK) < heapEnd) &&
        (*(DWORD *)((char *)(pArena + 1) + (pArena->size & ARENA_SIZE_MASK)) & ARENA_FLAG_PREV_FREE))
    {
        ERR("Heap %08lx: in-use arena %08lx next block has PREV_FREE flag\n",
                 (DWORD)subheap->heap, (DWORD)pArena );
        return FALSE;
    }
    /* Check prev free arena */
    if (pArena->size & ARENA_FLAG_PREV_FREE)
    {
        ARENA_FREE *pPrev = *((ARENA_FREE **)pArena - 1);
        /* Check prev pointer */
        if (!HEAP_IsValidArenaPtr( subheap->heap, pPrev ))
        {
            ERR("Heap %08lx: bad back ptr %08lx for arena %08lx\n",
                    (DWORD)subheap->heap, (DWORD)pPrev, (DWORD)pArena );
            return FALSE;
        }
        /* Check that prev arena is free */
        if (!(pPrev->size & ARENA_FLAG_FREE) ||
            (pPrev->magic != ARENA_FREE_MAGIC))
        {
            ERR("Heap %08lx: prev arena %08lx invalid for in-use %08lx\n",
                     (DWORD)subheap->heap, (DWORD)pPrev, (DWORD)pArena );
            return FALSE;
        }
        /* Check that prev arena is really the previous block */
        if ((char *)(pPrev + 1) + (pPrev->size & ARENA_SIZE_MASK) != (char *)pArena)
        {
            ERR("Heap %08lx: prev arena %08lx is not prev for in-use %08lx\n",
                     (DWORD)subheap->heap, (DWORD)pPrev, (DWORD)pArena );
            return FALSE;
        }
    }
    return TRUE;
}


/***********************************************************************
 *           HEAP_IsRealArena  [Internal]
 * Validates a block is a valid arena.
 *
 * RETURNS
 *	TRUE: Success
 *	FALSE: Failure
 */
static BOOL HEAP_IsRealArena( HEAP *heapPtr,   /* [in] ptr to the heap */
              DWORD flags,   /* [in] Bit flags that control access during operation */
              LPCVOID block, /* [in] Optional pointer to memory block to validate */
              BOOL quiet )   /* [in] Flag - if true, HEAP_ValidateInUseArena
                              *             does not complain    */
{
    SUBHEAP *subheap;
    BOOL ret = TRUE;

    if (!heapPtr || (heapPtr->magic != HEAP_MAGIC))
    {
        ERR("Invalid heap %p!\n", heapPtr );
        return FALSE;
    }

    flags &= HEAP_NO_SERIALIZE;
    flags |= heapPtr->flags;
    /* calling HeapLock may result in infinite recursion, so do the critsect directly */
    if (!(flags & HEAP_NO_SERIALIZE))
        RtlEnterCriticalSection( &heapPtr->critSection );

    if (block)
    {
        /* Only check this single memory block */

        if (!(subheap = HEAP_FindSubHeap( heapPtr, block )) ||
            ((char *)block < (char *)subheap + subheap->headerSize
                              + sizeof(ARENA_INUSE)))
        {
            if (quiet == NOISY)
                ERR("Heap %p: block %p is not inside heap\n", heapPtr, block );
            else if (WARN_ON(heap))
                WARN("Heap %p: block %p is not inside heap\n", heapPtr, block );
            ret = FALSE;
        } else
            ret = HEAP_ValidateInUseArena( subheap, (ARENA_INUSE *)block - 1, quiet );

        if (!(flags & HEAP_NO_SERIALIZE))
            RtlLeaveCriticalSection( &heapPtr->critSection );
        return ret;
    }

    subheap = &heapPtr->subheap;
    while (subheap && ret)
    {
        char *ptr = (char *)subheap + subheap->headerSize;
        while (ptr < (char *)subheap + subheap->size)
        {
            if (*(DWORD *)ptr & ARENA_FLAG_FREE)
            {
                if (!HEAP_ValidateFreeArena( subheap, (ARENA_FREE *)ptr )) {
                    ret = FALSE;
                    break;
                }
                ptr += sizeof(ARENA_FREE) + (*(DWORD *)ptr & ARENA_SIZE_MASK);
            }
            else
            {
                if (!HEAP_ValidateInUseArena( subheap, (ARENA_INUSE *)ptr, NOISY )) {
                    ret = FALSE;
                    break;
                }
                ptr += sizeof(ARENA_INUSE) + (*(DWORD *)ptr & ARENA_SIZE_MASK);
            }
        }
        subheap = subheap->next;
    }

    if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
    return ret;
}


/***********************************************************************
 *           RtlCreateHeap   (NTDLL.@)
 */
HANDLE OldRtlCreateHeap( ULONG flags, PVOID addr, ULONG totalSize, ULONG commitSize,
                             PVOID unknown, PRTL_HEAP_DEFINITION definition )
{
    SUBHEAP *subheap;

    /* Allocate the heap block */

    if (!totalSize)
    {
        totalSize = HEAP_DEF_SIZE;
        flags |= HEAP_GROWABLE;
    }
    /* round up sizes */
    totalSize  = (totalSize + 0xffff) & 0xffff0000;
    commitSize = (commitSize + 0xffff) & 0xffff0000;
    if (!commitSize) commitSize = 0x10000;
    if (totalSize < commitSize) totalSize = commitSize;

    if (!(subheap = HEAP_CreateSubHeap( NULL, addr, flags, commitSize, totalSize ))) return 0;

    /* link it into the per-process heap list */
    if (processHeap)
    {
        HEAP *heapPtr = subheap->heap;
        RtlLockHeap( processHeap );
        heapPtr->next = firstHeap;
        firstHeap = heapPtr;
        RtlUnlockHeap( processHeap );
    }
    else  /* assume the first heap we create is the process main heap */
       processHeap = (HANDLE)subheap->heap;
    return (HANDLE)subheap;
}


/***********************************************************************
 *           RtlDestroyHeap   (NTDLL.@)
 */
HANDLE OldRtlDestroyHeap( HANDLE heap )
{
    HEAP *heapPtr = HEAP_GetPtr( heap );
    SUBHEAP *subheap;

    TRACE("%08x\n", heap );
    if (!heapPtr) return heap;

    if (heap == processHeap) return heap; /* cannot delete the main process heap */
    else /* remove it from the per-process list */
    {
        HEAP **pptr;
        RtlLockHeap( processHeap );
        pptr = &firstHeap;
        while (*pptr && *pptr != heapPtr) pptr = &(*pptr)->next;
        if (*pptr) *pptr = (*pptr)->next;
        RtlUnlockHeap( processHeap );
    }

    RtlDeleteCriticalSection( &heapPtr->critSection );
    subheap = &heapPtr->subheap;
    while (subheap)
    {
        SUBHEAP *next = subheap->next;
        VirtualFree( subheap, 0, MEM_RELEASE );
        subheap = next;
    }
    return 0;
}


/***********************************************************************
 *           RtlAllocateHeap   (NTDLL.@)
 *
 * NOTE: does not set last error.
 */
PVOID OldRtlAllocateHeap( HANDLE heap, ULONG flags, ULONG size )
{
    ARENA_FREE *pArena;
    ARENA_INUSE *pInUse;
    SUBHEAP *subheap;
    HEAP *heapPtr = HEAP_GetPtr( heap );

    /* Validate the parameters */

    if (!heapPtr) return NULL;
    flags &= HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY;
    flags |= heapPtr->flags;
    size = (size + 3) & ~3;
    if (size < HEAP_MIN_BLOCK_SIZE) size = HEAP_MIN_BLOCK_SIZE;

    if (!(flags & HEAP_NO_SERIALIZE)) RtlEnterCriticalSection( &heapPtr->critSection );
    /* Locate a suitable free block */

    if (!(pArena = HEAP_FindFreeBlock( heapPtr, size, &subheap )))
    {
        TRACE("(%08x,%08lx,%08lx): returning NULL\n",
                  heap, flags, size  );
        if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
        if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus( STATUS_NO_MEMORY );
        return NULL;
    }

    /* Remove the arena from the free list */

    pArena->next->prev = pArena->prev;
    pArena->prev->next = pArena->next;

    /* Build the in-use arena */

    pInUse = (ARENA_INUSE *)pArena;

    /* in-use arena is smaller than free arena,
     * so we have to add the difference to the size */
    pInUse->size  = (pInUse->size & ~ARENA_FLAG_FREE) + sizeof(ARENA_FREE) - sizeof(ARENA_INUSE);
    pInUse->magic = ARENA_INUSE_MAGIC;

    /* Shrink the block */

    HEAP_ShrinkBlock( subheap, pInUse, size );
    
#ifdef HEAP_PROFILE_ON
    WINE_INCREMENT_COUNTER("Heap:memory", pInUse->size);
    WINE_TIMER_FLAGS_UNSET("Heap:memory", RESET_FLAG | HISTORY_FLAG);
#endif

    if (flags & HEAP_ZERO_MEMORY)
        memset( pInUse + 1, 0, pInUse->size & ARENA_SIZE_MASK );
    else if (TRACE_ON(heap))
        memset( pInUse + 1, ARENA_INUSE_FILLER, pInUse->size & ARENA_SIZE_MASK );

    if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );

    TRACE("(%08x,%08lx,%08lx): returning %08lx\n",
                  heap, flags, size, (DWORD)(pInUse + 1) );
    return (LPVOID)(pInUse + 1);
}


/***********************************************************************
 *           RtlFreeHeap   (NTDLL.@)
 */
BOOLEAN OldRtlFreeHeap( HANDLE heap, ULONG flags, PVOID ptr )
{
    ARENA_INUSE *pInUse;
    SUBHEAP *subheap;
    HEAP *heapPtr = HEAP_GetPtr( heap );

    /* Validate the parameters */

    if (!ptr) return TRUE;  /* freeing a NULL ptr isn't an error in Win2k */
    if (!heapPtr)
    {
        set_status( STATUS_INVALID_HANDLE );
        return FALSE;
    }

    flags &= HEAP_NO_SERIALIZE;
    flags |= heapPtr->flags;
    if (!(flags & HEAP_NO_SERIALIZE)) RtlEnterCriticalSection( &heapPtr->critSection );
    if (!HEAP_IsRealArena( heapPtr, HEAP_NO_SERIALIZE, ptr, QUIET ))
    {
        if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
        set_status( STATUS_INVALID_PARAMETER );
        TRACE("(%08x,%08lx,%08lx): returning FALSE\n",
                      heap, flags, (DWORD)ptr );
        return FALSE;
    }

    /* Turn the block into a free block */

    pInUse  = (ARENA_INUSE *)ptr - 1;

#ifdef HEAP_PROFILE_ON
    WINE_INCREMENT_COUNTER("Heap:memory", -pInUse->size);
#endif
    
    subheap = HEAP_FindSubHeap( heapPtr, pInUse );
    HEAP_MakeInUseBlockFree( subheap, pInUse );

    if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );

    TRACE("(%08x,%08lx,%08lx): returning TRUE\n",
                  heap, flags, (DWORD)ptr );
    return TRUE;
}


/***********************************************************************
 *           RtlReAllocateHeap   (NTDLL.@)
 */
PVOID OldRtlReAllocateHeap( HANDLE heap, ULONG flags, PVOID ptr, ULONG size )
{
    ARENA_INUSE *pArena;
    DWORD oldSize;
    HEAP *heapPtr;
    SUBHEAP *subheap;

    if (!ptr) return RtlAllocateHeap( heap, flags, size );  /* FIXME: correct? */
    if (!(heapPtr = HEAP_GetPtr( heap )))
    {
        set_status( STATUS_INVALID_HANDLE );
        return FALSE;
    }

    /* Validate the parameters */

    flags &= HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY |
             HEAP_REALLOC_IN_PLACE_ONLY;
    flags |= heapPtr->flags;
    size = (size + 3) & ~3;
    if (size < HEAP_MIN_BLOCK_SIZE) size = HEAP_MIN_BLOCK_SIZE;

    if (!(flags & HEAP_NO_SERIALIZE)) RtlEnterCriticalSection( &heapPtr->critSection );
    if (!HEAP_IsRealArena( heapPtr, HEAP_NO_SERIALIZE, ptr, QUIET ))
    {
        if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
        set_status( STATUS_INVALID_PARAMETER );
        TRACE("(%08x,%08lx,%08lx,%08lx): returning NULL\n",
                      heap, flags, (DWORD)ptr, size );
        return NULL;
    }

    /* Check if we need to grow the block */

    pArena = (ARENA_INUSE *)ptr - 1;
    subheap = HEAP_FindSubHeap( heapPtr, pArena );
    oldSize = (pArena->size & ARENA_SIZE_MASK);

#ifdef HEAP_PROFILE_ON
    WINE_INCREMENT_COUNTER("Heap:memory", -oldSize);
    WINE_INCREMENT_COUNTER("Heap:memory", size);
#endif

    if (size > oldSize)
    {
        char *pNext = (char *)(pArena + 1) + oldSize;
        if ((pNext < (char *)subheap + subheap->size) &&
            (*(DWORD *)pNext & ARENA_FLAG_FREE) &&
            (oldSize + (*(DWORD *)pNext & ARENA_SIZE_MASK) + sizeof(ARENA_FREE) >= size))
        {
            /* The next block is free and large enough */
            ARENA_FREE *pFree = (ARENA_FREE *)pNext;
            pFree->next->prev = pFree->prev;
            pFree->prev->next = pFree->next;
            pArena->size += (pFree->size & ARENA_SIZE_MASK) + sizeof(*pFree);
            if (!HEAP_Commit( subheap, (char *)pArena + sizeof(ARENA_INUSE)
                                               + size + HEAP_MIN_BLOCK_SIZE))
            {
                if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
                if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus( STATUS_NO_MEMORY );
                set_status( STATUS_NO_MEMORY );
                return NULL;
            }
            HEAP_ShrinkBlock( subheap, pArena, size );
        }
        else  /* Do it the hard way */
        {
            ARENA_FREE *pNew;
            ARENA_INUSE *pInUse;
            SUBHEAP *newsubheap;

            if ((flags & HEAP_REALLOC_IN_PLACE_ONLY) ||
                !(pNew = HEAP_FindFreeBlock( heapPtr, size, &newsubheap )))
            {
                if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
                if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus( STATUS_NO_MEMORY );
                set_status( STATUS_NO_MEMORY );
                return NULL;
            }

            /* Build the in-use arena */

            pNew->next->prev = pNew->prev;
            pNew->prev->next = pNew->next;
            pInUse = (ARENA_INUSE *)pNew;
            pInUse->size = (pInUse->size & ~ARENA_FLAG_FREE)
                           + sizeof(ARENA_FREE) - sizeof(ARENA_INUSE);
            pInUse->magic = ARENA_INUSE_MAGIC;
            HEAP_ShrinkBlock( newsubheap, pInUse, size );
            memcpy( pInUse + 1, pArena + 1, oldSize );

            /* Free the previous block */

            HEAP_MakeInUseBlockFree( subheap, pArena );
            subheap = newsubheap;
            pArena  = pInUse;
        }
    }
    else HEAP_ShrinkBlock( subheap, pArena, size );  /* Shrink the block */

    /* Clear the extra bytes if needed */

    if (size > oldSize)
    {
        if (flags & HEAP_ZERO_MEMORY)
            memset( (char *)(pArena + 1) + oldSize, 0,
                    (pArena->size & ARENA_SIZE_MASK) - oldSize );
        else if (TRACE_ON(heap))
            memset( (char *)(pArena + 1) + oldSize, ARENA_INUSE_FILLER,
                    (pArena->size & ARENA_SIZE_MASK) - oldSize );
    }

    /* Return the new arena */

    if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );

    TRACE("(%08x,%08lx,%08lx,%08lx): returning %08lx\n",
                  heap, flags, (DWORD)ptr, size, (DWORD)(pArena + 1) );
    return (LPVOID)(pArena + 1);
}


/***********************************************************************
 *           RtlCompactHeap   (NTDLL.@)
 */
ULONG OldRtlCompactHeap( HANDLE heap, ULONG flags )
{
    FIXME( "stub\n" );
    return 0;
}


/***********************************************************************
 *           RtlLockHeap   (NTDLL.@)
 */
BOOLEAN OldRtlLockHeap( HANDLE heap )
{
    HEAP *heapPtr = HEAP_GetPtr( heap );
    if (!heapPtr) return FALSE;
    RtlEnterCriticalSection( &heapPtr->critSection );
    return TRUE;
}


/***********************************************************************
 *           RtlUnlockHeap   (NTDLL.@)
 */
BOOLEAN OldRtlUnlockHeap( HANDLE heap )
{
    HEAP *heapPtr = HEAP_GetPtr( heap );
    if (!heapPtr) return FALSE;
    RtlLeaveCriticalSection( &heapPtr->critSection );
    return TRUE;
}


/***********************************************************************
 *           RtlSizeHeap   (NTDLL.@)
 */
ULONG OldRtlSizeHeap( HANDLE heap, ULONG flags, PVOID ptr )
{
    DWORD ret;
    HEAP *heapPtr = HEAP_GetPtr( heap );

    if (!heapPtr)
    {
        set_status( STATUS_INVALID_HANDLE );
        return (ULONG)-1;
    }
    flags &= HEAP_NO_SERIALIZE;
    flags |= heapPtr->flags;
    if (!(flags & HEAP_NO_SERIALIZE)) RtlEnterCriticalSection( &heapPtr->critSection );
    if (!HEAP_IsRealArena( heapPtr, HEAP_NO_SERIALIZE, ptr, QUIET ))
    {
        set_status( STATUS_INVALID_PARAMETER );
        ret = (ULONG)-1;
    }
    else
    {
        ARENA_INUSE *pArena = (ARENA_INUSE *)ptr - 1;
        ret = pArena->size & ARENA_SIZE_MASK;
    }
    if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );

    TRACE("(%08x,%08lx,%08lx): returning %08lx\n",
                  heap, flags, (DWORD)ptr, ret );
    return ret;
}


/***********************************************************************
 *           RtlValidateHeap   (NTDLL.@)
 */
BOOLEAN OldRtlValidateHeap( HANDLE heap, ULONG flags, PCVOID block )
{
    HEAP *heapPtr = HEAP_GetPtr( heap );
    if (!heapPtr) return FALSE;
    return HEAP_IsRealArena( heapPtr, flags, block, QUIET );
}


/***********************************************************************
 *           RtlWalkHeap    (NTDLL.@)
 *
 * FIXME: the PROCESS_HEAP_ENTRY flag values seem different between this
 *        function and HeapWalk. To be checked.
 */
NTSTATUS OldRtlWalkHeap( HANDLE heap, PVOID entry_ptr )
{
    LPPROCESS_HEAP_ENTRY entry = entry_ptr; /* FIXME */
    HEAP *heapPtr = HEAP_GetPtr(heap);
    SUBHEAP *sub, *currentheap = NULL;
    NTSTATUS ret;
    char *ptr;
    int region_index = 0;

    FIXME( "not fully compatible\n" );

    if (!heapPtr || !entry) return STATUS_INVALID_PARAMETER;

    if (!(heapPtr->flags & HEAP_NO_SERIALIZE)) EnterCriticalSection( &heapPtr->critSection );

    /* set ptr to the next arena to be examined */

    if (!entry->lpData) /* first call (init) ? */
    {
        TRACE("begin walking of heap 0x%08x.\n", heap);
        currentheap = &heapPtr->subheap;
        ptr = (char*)currentheap + currentheap->headerSize;
    }
    else
    {
        ptr = entry->lpData;
        sub = &heapPtr->subheap;
        while (sub)
        {
            if (((char *)ptr >= (char *)sub) &&
                ((char *)ptr < (char *)sub + sub->size))
            {
                currentheap = sub;
                break;
            }
            sub = sub->next;
            region_index++;
        }
        if (currentheap == NULL)
        {
            ERR("no matching subheap found, shouldn't happen !\n");
            ret = STATUS_NO_MORE_ENTRIES;
            goto HW_end;
        }

        ptr += entry->cbData; /* point to next arena */
        if (ptr > (char *)currentheap + currentheap->size - 1)
        {   /* proceed with next subheap */
            if (!(currentheap = currentheap->next))
            {  /* successfully finished */
                TRACE("end reached.\n");
                ret = STATUS_NO_MORE_ENTRIES;
                goto HW_end;
            }
            ptr = (char*)currentheap + currentheap->headerSize;
        }
    }

    entry->wFlags = 0;
    if (*(DWORD *)ptr & ARENA_FLAG_FREE)
    {
        ARENA_FREE *pArena = (ARENA_FREE *)ptr;

        /*TRACE("free, magic: %04x\n", pArena->magic);*/

        entry->lpData = pArena + 1;
        entry->cbData = pArena->size & ARENA_SIZE_MASK;
        entry->cbOverhead = sizeof(ARENA_FREE);
        entry->wFlags = PROCESS_HEAP_UNCOMMITTED_RANGE;
    }
    else
    {
        ARENA_INUSE *pArena = (ARENA_INUSE *)ptr;

        /*TRACE("busy, magic: %04x\n", pArena->magic);*/

        entry->lpData = pArena + 1;
        entry->cbData = pArena->size & ARENA_SIZE_MASK;
        entry->cbOverhead = sizeof(ARENA_INUSE);
        entry->wFlags = PROCESS_HEAP_ENTRY_BUSY;
        /* FIXME: can't handle PROCESS_HEAP_ENTRY_MOVEABLE
        and PROCESS_HEAP_ENTRY_DDESHARE yet */
    }

    entry->iRegionIndex = region_index;

    /* first element of heap ? */
    if (ptr == (char *)(currentheap + currentheap->headerSize))
    {
        entry->wFlags |= PROCESS_HEAP_REGION;
        entry->u.Region.dwCommittedSize = currentheap->commitSize;
        entry->u.Region.dwUnCommittedSize =
                currentheap->size - currentheap->commitSize;
        entry->u.Region.lpFirstBlock = /* first valid block */
                currentheap + currentheap->headerSize;
        entry->u.Region.lpLastBlock  = /* first invalid block */
                currentheap + currentheap->size;
    }
    ret = STATUS_SUCCESS;

HW_end:
    if (!(heapPtr->flags & HEAP_NO_SERIALIZE)) LeaveCriticalSection( &heapPtr->critSection );
    return ret;
}


/***********************************************************************
 *           RtlGetProcessHeaps    (NTDLL.@)
 */
ULONG OldRtlGetProcessHeaps( ULONG count, HANDLE *heaps )
{
    DWORD total;
    HEAP *ptr;

    if (!processHeap) return 0;  /* should never happen */
    total = 1;  /* main heap */
    RtlLockHeap( processHeap );
    for (ptr = firstHeap; ptr; ptr = ptr->next) total++;
    if (total <= count)
    {
        *heaps++ = (HANDLE)processHeap;
        for (ptr = firstHeap; ptr; ptr = ptr->next) *heaps++ = (HANDLE)ptr;
    }
    RtlUnlockHeap( processHeap );
    return total;
}
