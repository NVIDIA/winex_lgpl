/*
 * Win32 heap function initialization
 *
 * Copyright 2006 TransGaming Technologies
 */

#include "config.h"
#include "winternl.h"
#include "heapfuncs.h"


static BOOL gUseNewAllocator = 1;


void HEAP_Init (BOOL UseNewAllocator)
{
   gUseNewAllocator = UseNewAllocator;
}


HANDLE WINAPI RtlCreateHeap (ULONG flags, PVOID addr, ULONG reserveSize,
                             ULONG commitSize, PVOID unknown,
                             PRTL_HEAP_DEFINITION definition)
{
   if (gUseNewAllocator)
      return NewRtlCreateHeap (flags, addr, reserveSize, commitSize, unknown,
                               definition);
   else
      return OldRtlCreateHeap (flags, addr, reserveSize, commitSize, unknown,
                               definition);
}


HANDLE WINAPI RtlDestroyHeap (HANDLE heap)
{
   if (gUseNewAllocator)
      return NewRtlDestroyHeap (heap);
   else
      return OldRtlDestroyHeap (heap);
}


PVOID WINAPI RtlAllocateHeap (HANDLE heap, ULONG flags, ULONG size)
{
   if (gUseNewAllocator)
      return NewRtlAllocateHeap (heap, flags, size);
   else
      return OldRtlAllocateHeap (heap, flags, size);
}


BOOLEAN WINAPI RtlFreeHeap (HANDLE heap, ULONG flags, PVOID ptr)
{
   if (gUseNewAllocator)
      return NewRtlFreeHeap (heap, flags, ptr);
   else
      return OldRtlFreeHeap (heap, flags, ptr);
}


PVOID WINAPI RtlReAllocateHeap (HANDLE heap, ULONG flags, PVOID ptr,
                                ULONG size)
{
   if (gUseNewAllocator)
      return NewRtlReAllocateHeap (heap, flags, ptr, size);
   else
      return OldRtlReAllocateHeap (heap, flags, ptr, size);
}


ULONG WINAPI RtlCompactHeap (HANDLE heap, ULONG flags)
{
   if (gUseNewAllocator)
      return NewRtlCompactHeap (heap, flags);
   else
      return OldRtlCompactHeap (heap, flags);
}


BOOLEAN WINAPI RtlLockHeap (HANDLE heap)
{
   if (gUseNewAllocator)
      return NewRtlLockHeap (heap);
   else
      return OldRtlLockHeap (heap);
}


BOOLEAN WINAPI RtlUnlockHeap (HANDLE heap)
{
   if (gUseNewAllocator)
      return NewRtlUnlockHeap (heap);
   else
      return OldRtlUnlockHeap (heap);
}


ULONG WINAPI RtlSizeHeap (HANDLE heap, ULONG flags, PVOID ptr)
{
   if (gUseNewAllocator)
      return NewRtlSizeHeap (heap, flags, ptr);
   else
      return OldRtlSizeHeap (heap, flags, ptr);
}


BOOLEAN WINAPI RtlValidateHeap (HANDLE heap, ULONG flags, PCVOID block)
{
   if (gUseNewAllocator)
      return NewRtlValidateHeap (heap, flags, block);
   else
      return OldRtlValidateHeap (heap, flags, block);
}


NTSTATUS WINAPI RtlWalkHeap (HANDLE heap, PVOID entry_ptr)
{
   if (gUseNewAllocator)
      return NewRtlWalkHeap (heap, entry_ptr);
   else
      return OldRtlWalkHeap (heap, entry_ptr);
}


ULONG WINAPI RtlGetProcessHeaps (ULONG count, HANDLE *heaps)
{
   if (gUseNewAllocator)
      return NewRtlGetProcessHeaps (count, heaps);
   else
      return OldRtlGetProcessHeaps (count, heaps);
}
