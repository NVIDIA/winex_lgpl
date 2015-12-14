/*
 * Win32 heap function initialization
 *
 * Copyright 2006 TransGaming Technologies
 */

#ifndef _WINE_HEAPFUNCS_H
#define _WINE_HEAPFUNCS_H

extern HANDLE OldRtlCreateHeap (ULONG flags, PVOID addr, ULONG reserveSize,
                                ULONG commitSize, PVOID unknown,
                                PRTL_HEAP_DEFINITION definition);
extern HANDLE OldRtlDestroyHeap (HANDLE heap);
extern PVOID OldRtlAllocateHeap (HANDLE heap, ULONG flags, ULONG size);
extern BOOLEAN OldRtlFreeHeap (HANDLE heap, ULONG flags, PVOID ptr);
extern PVOID OldRtlReAllocateHeap (HANDLE heap, ULONG flags, PVOID ptr,
                                   ULONG size);
extern ULONG OldRtlCompactHeap (HANDLE heap, ULONG flags);
extern BOOLEAN OldRtlLockHeap (HANDLE heap);
extern BOOLEAN OldRtlUnlockHeap (HANDLE heap);
extern ULONG OldRtlSizeHeap (HANDLE heap, ULONG flags, PVOID ptr);
extern BOOLEAN OldRtlValidateHeap (HANDLE heap, ULONG flags, PCVOID block);
extern NTSTATUS OldRtlWalkHeap (HANDLE heap, PVOID entry_ptr);
extern ULONG OldRtlGetProcessHeaps (ULONG count, HANDLE *heaps);


extern HANDLE NewRtlCreateHeap (ULONG flags, PVOID addr, ULONG reserveSize,
                                ULONG commitSize, PVOID unknown,
                                PRTL_HEAP_DEFINITION definition);
extern HANDLE NewRtlDestroyHeap (HANDLE heap);
extern PVOID NewRtlAllocateHeap (HANDLE heap, ULONG flags, ULONG size);
extern BOOLEAN NewRtlFreeHeap (HANDLE heap, ULONG flags, PVOID ptr);
extern PVOID NewRtlReAllocateHeap (HANDLE heap, ULONG flags, PVOID ptr,
                                   ULONG size);
extern ULONG NewRtlCompactHeap (HANDLE heap, ULONG flags);
extern BOOLEAN NewRtlLockHeap (HANDLE heap);
extern BOOLEAN NewRtlUnlockHeap (HANDLE heap);
extern ULONG NewRtlSizeHeap (HANDLE heap, ULONG flags, PVOID ptr);
extern BOOLEAN NewRtlValidateHeap (HANDLE heap, ULONG flags, PCVOID block);
extern NTSTATUS NewRtlWalkHeap (HANDLE heap, PVOID entry_ptr);
extern ULONG NewRtlGetProcessHeaps (ULONG count, HANDLE *heaps);

#endif
