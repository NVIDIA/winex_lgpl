/*
 * Win32 heap functions
 *
 *  \copyright Copyright (c) 2006-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 *
 *  Win32 heap functions.
*/
#include "config.h"

#include "winternl.h"
#include "thread.h"
#include "wine/debug.h"
#include "heapfuncs.h"

#define ONLY_MSPACES 1
#define MSPACES 1
#include "../../libs/ptmalloc3/malloc-2.8.3.h"


WINE_DEFAULT_DEBUG_CHANNEL(heap);


/* Sanity test to ensure we're given a valid heap */
#define HEAP_MAGIC       ((DWORD)('H' | ('E'<<8) | ('A'<<16) | ('P'<<24)))

#define HEAP_DEF_SIZE        0x110000   /* Default heap size = 1Mb + 64Kb */


/* Structure for holding per-heap information */
typedef struct {
    DWORD            Magic;
    mspace           MSpace;
    DWORD            Flags;
    DWORD            InitialCommit;
    CRITICAL_SECTION CS;
} HeapInfo_t;


static HeapInfo_t *gProcessHeap;


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

/* Get heap ptr from handle, doing necessary error checking */
inline static HeapInfo_t *get_heap_ptr (HANDLE hHeap)
{
    HeapInfo_t *pHeap = (HeapInfo_t *)hHeap;

    if (!pHeap || (pHeap->Magic != HEAP_MAGIC))
    {
        ERR ("Invalid heap %08x!\n", hHeap);
        return NULL;
    }

    return pHeap;
}


/***********************************************************************
 *           RtlCreateHeap   (NTDLL.@)
 */
HANDLE NewRtlCreateHeap (ULONG flags, PVOID addr, ULONG reserveSize,
                         ULONG commitSize, PVOID unknown,
                         PRTL_HEAP_DEFINITION definition)
{
   HeapInfo_t *NewHeap;

   TRACE ("(0x%lx, %p, 0x%lx, 0x%lx, %p, %p)\n", flags, addr, reserveSize,
          commitSize, unknown, definition);

    if (!reserveSize)
    {
        reserveSize = HEAP_DEF_SIZE;
        flags |= HEAP_GROWABLE;
    }

   /* HACK - commit full reserved area, as dmalloc doesn't have the concept
      of reserving a location separately from committing; this is generally
      ok as normally reserveSize == commitSize or 0. This could fall down
      in the case of a very large reserve size */
   if (reserveSize && (commitSize < reserveSize))
   {
      WARN ("reserveSize (0x%lx) > commitSize (0x%lx) - committing 0x%lx\n",
            reserveSize, commitSize, reserveSize);
      commitSize = reserveSize;
   }

   /* HEAP_CREATE_ENABLE_EXECUTE is ignored, as we currently always allocate
      heap memory as rwx for pre-XP compatibility */

   NewHeap = (HeapInfo_t *)malloc (sizeof (HeapInfo_t));
   if (!NewHeap)
   {
      SetLastError (ERROR_OUTOFMEMORY);
      return 0;
   }

   commitSize = (commitSize + 0xffff) & 0xffff0000;
   if (!commitSize)
      commitSize = 0x10000;

   memset (NewHeap, 0, sizeof (*NewHeap));
   NewHeap->Magic = HEAP_MAGIC;
   if (addr)
      NewHeap->MSpace = create_mspace_with_base (addr, commitSize, 0,
                                                 flags & HEAP_GROWABLE);
   else
      NewHeap->MSpace = create_mspace (commitSize, 0);
   NewHeap->Flags = flags;
   NewHeap->InitialCommit = commitSize;
   if (!(flags & HEAP_NO_SERIALIZE))
       RTL_CRITICAL_SECTION_DEFINE (&NewHeap->CS);

   /* Assume first call is to set up process heap */
   if (!gProcessHeap)
      gProcessHeap = NewHeap;

   TRACE ("=> 0x%x\n", (HANDLE)NewHeap);
   return (HANDLE)NewHeap;
}


/***********************************************************************
 *           RtlDestroyHeap   (NTDLL.@)
 */
HANDLE NewRtlDestroyHeap (HANDLE heap)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);

   TRACE ("(0x%x)\n", heap);

   /* Can't destroy process heap, and check for invalid heap */
   if (!pHeap || (pHeap == gProcessHeap))
   {
      TRACE ("=> 0x%x\n", heap);
      return heap;
   }

   destroy_mspace (pHeap->MSpace);
   pHeap->Magic = 0;
   if (!(pHeap->Flags & HEAP_NO_SERIALIZE))
       RtlDeleteCriticalSection (&pHeap->CS);
   free (pHeap);

   TRACE ("=> 0\n");
   return 0;
}


/***********************************************************************
 *           RtlAllocateHeap   (NTDLL.@)
 *
 * NOTE: does not set last error.
 */
PVOID NewRtlAllocateHeap (HANDLE heap, ULONG flags, ULONG size)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);
   PVOID pMem;
   ULONG UserSize = size;
   ULONG ChunkSize = 0;

   TRACE ("(0x%x, 0x%lx, 0x%lx)\n", heap, flags, size);

   /* HEAP_CREATE_ENABLE_EXECUTE is ignored, as we currently always allocate
      heap memory as rwx for pre-XP compatibility */

   if (!pHeap)
   {
       TRACE ("=> %p\n", NULL);
       return NULL;
   }

   flags |= pHeap->Flags;

   /* Make size a multiple of 4 */
   size = (size + 3) & ~3;

   /* Add space to store user requested size.
      dmalloc already stores a size with each chunk of memory; however, it is the chunk
      size, which is often bigger than the user-requested size. We need to know the
      user-requested size for both RtlSizeHeap() and RtlReAllocateHeap().
      Adding the dword here to track it was easier than changing the allocator's chunk
      structures.
      We put the size at the end of the block (so at ChunkSize - 4). */
   size += sizeof (ULONG);

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlEnterCriticalSection (&pHeap->CS);

   if (flags & HEAP_CREATE_ALIGN_16)
      pMem = mspace_memalign (pHeap->MSpace, 16, size);
   else
      pMem = mspace_malloc (pHeap->MSpace, size);

   if (pMem)
      ChunkSize = mspace_usable_size (pMem);


   if (!(flags & HEAP_NO_SERIALIZE))
       RtlLeaveCriticalSection (&pHeap->CS);

   if (!pMem)
   {
      ERR ("Allocation of size 0x%lx failed\n", size);
      if (flags & HEAP_GENERATE_EXCEPTIONS)
         RtlRaiseStatus (STATUS_NO_MEMORY);

      set_status (STATUS_NO_MEMORY);
      return NULL;
   }

   if (flags & HEAP_ZERO_MEMORY)
      memset (pMem, 0, size);

   /* Store user requested size, as what the allocator tracks is
      chunk size (which is often > user requested size)
      We calculate it as (chunksize - 4) so that we can locate it in
      realloc where we aren't given the previous size */
   memcpy ((char *)pMem + ChunkSize - sizeof (ULONG), &UserSize,
           sizeof (ULONG));

   TRACE ("=> %p\n", pMem);
   return pMem;
}


/***********************************************************************
 *           RtlFreeHeap   (NTDLL.@)
 */
BOOLEAN NewRtlFreeHeap (HANDLE heap, ULONG flags, PVOID ptr)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);
   BOOLEAN Ret = TRUE;

   TRACE ("(0x%x, 0x%lx, %p)\n", heap, flags, ptr);

   if (!ptr)
   {
      TRACE ("=> TRUE\n");
      return TRUE;
   }

   if (!pHeap)
   {
       set_status (STATUS_INVALID_HANDLE);
       TRACE ("=> FALSE\n");
       return FALSE;
   }

   flags |= pHeap->Flags;

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlEnterCriticalSection (&pHeap->CS);

   if (!mspace_validate (pHeap->MSpace, ptr))
   {
      ERR ("block %p isn't a valid part of heap 0x%x\n", ptr, heap);
      set_status (STATUS_INVALID_PARAMETER);
      Ret = FALSE;
   }
   else
      mspace_free (pHeap->MSpace, ptr);

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlLeaveCriticalSection (&pHeap->CS);

   TRACE ("=> %d\n", Ret);
   return Ret;
}


/***********************************************************************
 *           RtlReAllocateHeap   (NTDLL.@)
 */
PVOID NewRtlReAllocateHeap (HANDLE heap, ULONG flags, PVOID ptr,
                            ULONG size)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);
   ULONG OldUserSize = 0, NewUserSize, OldChunkSize = 0, NewChunkSize = 0;
   PVOID pMem = NULL;

   TRACE ("(0x%x, 0x%lx, %p, 0x%lx)\n", heap, flags, ptr, size);

   if (!pHeap)
   {
       set_status (STATUS_INVALID_HANDLE);
       TRACE ("=> %p\n", NULL);
       return NULL;
   }

   if (!ptr)
   {
      pMem = NewRtlAllocateHeap (heap, flags, size);
      TRACE ("=> %p\n", pMem);
      return pMem;
   }

   NewUserSize = size;

   /* Make size a multiple of 4 */
   size = (size + 3) & ~3;

   /* Add space to store user requested size */
   size += sizeof (ULONG);

   /* HEAP_CREATE_ENABLE_EXECUTE is ignored, as we currently always allocate
      heap memory as rwx for pre-XP compatibility */

   flags |= pHeap->Flags;

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlEnterCriticalSection (&pHeap->CS);

   if (!mspace_validate (pHeap->MSpace, ptr))
   {
      ERR ("block %p isn't a valid part of heap 0x%x\n", ptr, heap);
      set_status (STATUS_INVALID_PARAMETER);
   }
   else
   {
      OldChunkSize = mspace_usable_size (ptr);
      memcpy (&OldUserSize, (char *)ptr + OldChunkSize - sizeof (ULONG),
              sizeof (ULONG));
      pMem = mspace_realloc (pHeap->MSpace, ptr, size,
                             flags & HEAP_REALLOC_IN_PLACE_ONLY ? 1 : 0);
      if (pMem)
         NewChunkSize = mspace_usable_size (pMem);
      else
      {
         if (flags & HEAP_REALLOC_IN_PLACE_ONLY)
            WARN ("Resize-in-place of %p from 0x%lx to 0x%lx failed!\n",
                  ptr, OldUserSize, NewUserSize);
         else
            ERR ("Resize of %p from 0x%lx to 0x%lx failed!\n",
                 ptr, OldUserSize, NewUserSize);
         set_status (STATUS_NO_MEMORY);
      }
   }

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlLeaveCriticalSection (&pHeap->CS);

   if (!OldChunkSize)
      return NULL;

   if (!pMem)
   {
      if (flags & HEAP_GENERATE_EXCEPTIONS)
         RtlRaiseStatus (STATUS_NO_MEMORY);
      return NULL;
   }

   if (flags & HEAP_ZERO_MEMORY)
   {
      if (NewUserSize > OldUserSize)
         memset ((char *)pMem + OldUserSize, 0, NewUserSize - OldUserSize);
   }

   /* Update the user requested size */
   memcpy ((char *)pMem + NewChunkSize - sizeof (ULONG), &NewUserSize,
           sizeof (ULONG));

   TRACE ("=> %p\n", pMem);
   return pMem;
}


/***********************************************************************
 *           RtlCompactHeap   (NTDLL.@)
 */
ULONG NewRtlCompactHeap (HANDLE heap, ULONG flags)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);
   ULONG Ret;

   TRACE ("(%x)\n", heap);
   if (!pHeap)
   {
       TRACE ("=> FALSE\n");
       return FALSE;
   }

   flags |= pHeap->Flags;

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlEnterCriticalSection (&pHeap->CS);

   /* Size returned is a lie, but MSDN states the value can't be depended
      on anyways */
   Ret = mspace_trim (pHeap->MSpace, pHeap->InitialCommit);

   if (!(flags & HEAP_NO_SERIALIZE))
       RtlLeaveCriticalSection (&pHeap->CS);

   return Ret;
}


/***********************************************************************
 *           RtlLockHeap   (NTDLL.@)
 */
BOOLEAN NewRtlLockHeap (HANDLE heap)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);

   TRACE ("(%x)\n", heap);
   if (!pHeap)
   {
       TRACE ("=> FALSE\n");
       return FALSE;
   }

   RtlEnterCriticalSection (&pHeap->CS);

   TRACE ("=> TRUE\n");
   return TRUE;
}


/***********************************************************************
 *           RtlUnlockHeap   (NTDLL.@)
 */
BOOLEAN NewRtlUnlockHeap (HANDLE heap)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);

   TRACE ("(%x)\n", heap);
   if (!pHeap)
   {
       TRACE ("=> FALSE\n");
       return FALSE;
   }

   RtlLeaveCriticalSection (&pHeap->CS);

   TRACE ("=> TRUE\n");
   return TRUE;
}


/***********************************************************************
 *           RtlSizeHeap   (NTDLL.@)
 */
ULONG NewRtlSizeHeap (HANDLE heap, ULONG flags, PVOID ptr)
{
   HeapInfo_t *pHeap = get_heap_ptr (heap);
   ULONG Ret = 0, ChunkSize = 0;

   TRACE ("(0x%x, 0x%lx, %p)\n", heap, flags, ptr);

   if (!pHeap)
   {
       set_status (STATUS_INVALID_HANDLE);
       return (ULONG)-1;
   }

   flags |= pHeap->Flags;

   if (!(flags & HEAP_NO_SERIALIZE))
      RtlEnterCriticalSection (&pHeap->CS);

   if (mspace_validate (pHeap->MSpace, ptr))
   {
      ChunkSize = mspace_usable_size (ptr);
      if (ChunkSize)
         memcpy (&Ret, (char *)ptr + ChunkSize - sizeof (ULONG),
                 sizeof (ULONG));
   }

   if (!(flags & HEAP_NO_SERIALIZE))
      RtlLeaveCriticalSection (&pHeap->CS);

   if (ChunkSize == 0)
   {
      ERR ("ptr %p isn't in heap 0x%x\n", ptr, heap);
      Ret = (ULONG)-1;
      set_status (STATUS_INVALID_PARAMETER);
   }

   TRACE ("=> 0x%lx\n", Ret);
   return Ret;
}


/***********************************************************************
 *           RtlValidateHeap   (NTDLL.@)
 */
BOOLEAN NewRtlValidateHeap (HANDLE heap, ULONG flags, PCVOID block)
{
   HeapInfo_t *pHeap;
   BOOL Ret = TRUE;

   TRACE ("(0x%x, 0x%lx, %p)\n", heap, flags, block);

   if (!block)
   {
      FIXME ("stub with block == NULL\n");
      return TRUE;
   }

   pHeap = get_heap_ptr (heap);
   if (!pHeap)
   {
      WARN ("heap 0x%x is an invalid heap ptr\n", heap);
      return FALSE;
   }

   flags |= pHeap->Flags;

   if (!(flags & HEAP_NO_SERIALIZE))
      RtlEnterCriticalSection (&pHeap->CS);

   if (!mspace_validate (pHeap->MSpace, block))
   {
      WARN ("block %p isn't a valid part of heap 0x%x\n", block, heap);
      Ret = FALSE;
   }

   if (!(flags & HEAP_NO_SERIALIZE))
      RtlLeaveCriticalSection (&pHeap->CS);

   TRACE ("=> %d\n", Ret);
   return Ret;
}


/***********************************************************************
 *           RtlWalkHeap    (NTDLL.@)
 *
 * FIXME: the PROCESS_HEAP_ENTRY flag values seem different between this
 *        function and HeapWalk. To be checked.
 */
NTSTATUS NewRtlWalkHeap (HANDLE heap, PVOID entry_ptr)
{
   FIXME ("(): stub\n");
   return STATUS_INVALID_PARAMETER;
}


/***********************************************************************
 *           RtlGetProcessHeaps    (NTDLL.@)
 */
ULONG NewRtlGetProcessHeaps (ULONG count, HANDLE *heaps)
{
   FIXME ("(): stub\n");
   return 0;
}
