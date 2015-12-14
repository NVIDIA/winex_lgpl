/*
 * SList implementaiton
 *
 * Copyright 2007 Tim Beckmann
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
