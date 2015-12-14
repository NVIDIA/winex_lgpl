/*
 * SList implementaiton
 *
 *  \copyright Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 *  \author Tim Beckmann
 *  \author Eric van Beurden
 *
 *  Threadsafe singly linked list implementation.  These functions provide an implementation for
 *  the Interlocked*SList() functions.  These are a family of functions that allow threadsafe
 *  access to a linked list.  The list behaves as a stack object.  The only operations allowed
 *  on the list are push-to-front and pop-from-front.  The length of the list may be queried at
 *  any time, but it is not threadsafe and is limited to 65535.
 *
 *  Note: there are other vista+ interlocked functions that allow one list to be added to the
 *        start of another list.  These functions still need to be implemented.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "config.h"

#include <string.h>
#include <stdio.h>

#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);


/***********************************************************************
 *           RtlInitializeSListHead    (NTDLL.@)
 */
VOID WINAPI RtlInitializeSListHead( PSLIST_HEADER ListHead )
{
    TRACE("for %p\n", ListHead);

    ListHead->s.Next.Next = NULL;
    ListHead->s.Depth = 0;
    ListHead->s.Sequence = 0;
}


/***********************************************************************
 *           RtlQueryDepthSList    (NTDLL.@)
 */
WORD WINAPI RtlQueryDepthSList( PSLIST_HEADER ListHead )
{
    TRACE("for %p\n", ListHead);

    return ListHead->s.Depth;
}


/***********************************************************************
 *           RtlInterlockedPushEntrySList    (NTDLL.@)
 */
PSLIST_ENTRY WINAPI RtlInterlockedPushEntrySList( PSLIST_HEADER ListHead,
                    PSLIST_ENTRY ListEntry )
{
    PSLIST_ENTRY oldHead;

    TRACE("push %p onto %p\n", ListEntry, ListHead);

    /* put the new entry at the head of the list */
    do {
        oldHead = ListHead->s.Next.Next;
        ListEntry->Next = oldHead;
    } while (InterlockedCompareExchangePointer((PVOID*)&ListHead->s.Next.Next,
             ListEntry, ListEntry->Next) != oldHead);

    /* increment the depth and sequence */
    InterlockedExchangeAdd((PLONG)&ListHead->s.Depth, (1<<16) + 1);

    return oldHead;
}


/***********************************************************************
 *           RtlInterlockedPopEntrySList    (NTDLL.@)
 */
PSLIST_ENTRY WINAPI RtlInterlockedPopEntrySList( PSLIST_HEADER ListHead )
{
    PSLIST_ENTRY item;

    TRACE("for %p\n", ListHead);

    /* pop the first item from the list */
    do {
        item = ListHead->s.Next.Next;
        if (item == NULL)
            return NULL;
    } while (InterlockedCompareExchangePointer((PVOID*)&ListHead->s.Next.Next,
             item->Next, item) != item);

    /* decrement the depth (but not the sequence since testing shows that only
       gets incremented for a push) */
    InterlockedExchangeAdd((PLONG)&ListHead->s.Depth, -1);

    return item;
}


/***********************************************************************
 *           RtlInterlockedFlushSList    (NTDLL.@)
 */
PSLIST_ENTRY WINAPI RtlInterlockedFlushSList( PSLIST_HEADER ListHead )
{
    PSLIST_ENTRY head;
    PSLIST_ENTRY item;
    WORD depth = 0;

    TRACE("for %p\n", ListHead);

    /* get the list head and set it to NULL */
    do {
      head = ListHead->s.Next.Next;
      if (head == NULL)
        return NULL;
    } while (InterlockedCompareExchangePointer((PVOID*)&ListHead->s.Next.Next,
             NULL, head) != head);

    /* count the entries that were flushed from the list */
    item = head;
    while(item)
    {
        depth++;
        item = item->Next;
    }

    /* decrement the depth to zero */
    InterlockedExchangeAdd((PLONG)&ListHead->s.Depth, -depth);

    return head;
}
