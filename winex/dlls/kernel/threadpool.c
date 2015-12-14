/*
 * Copyright 2007, TransGaming Technologies Inc.
 *
 */

#include "config.h"

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/threadpool.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(threadpool);


/* Why this value? the only items that post to the completion port are either
   tasks that we add, or ones bound with BindIoCompletionCallback. There, we'll
   pass in the function to be called as the completion key. Since no user
   function will be our worker thread main, this gives a safe identifier */
#define MAGIC_COMP_KEY ((ULONG_PTR)&WorkerThreadMain)


typedef enum {WI_SHUTDOWN, WI_CALLBACK} WorkItemEnum_t;

typedef struct {
   WorkItemEnum_t Type;
   union {
      struct {
         LPTHREAD_START_ROUTINE Function;
         PVOID Context;
      } Callback;
      struct {
         HANDLE hEvent;
      } Shutdown;
   };
} WorkItem_t;

static HANDLE hCompPort = INVALID_HANDLE_VALUE;
static DWORD  NumWorkerThreads = 0;


static DWORD WINAPI WorkerThreadMain (PVOID pData)
{
   while (TRUE)
   {
      DWORD NumBytes = 0;
      ULONG_PTR CompletionKey = 0;
      LPOVERLAPPED pOverlapped = NULL;
      BOOL Ret;
      WorkItem_t *pWorkItem;

      Ret = GetQueuedCompletionStatus (hCompPort, &NumBytes, &CompletionKey,
                                       &pOverlapped, INFINITE);

      /* User callback from BindIoCompletionCallback */
      if (CompletionKey != MAGIC_COMP_KEY)
      {
         LPOVERLAPPED_COMPLETION_ROUTINE pFunc =
            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletionKey;

         TRACE ("Doing I/O completion callback with (%lu, %lu, %p)\n",
                (Ret ? 0 : GetLastError ()), NumBytes, pOverlapped);

         /* FIXME - check that GetLastError () is the correct value to
            be passing here */
         if (Ret)
            pFunc (0, NumBytes, pOverlapped);
         else
            pFunc (GetLastError (), NumBytes, pOverlapped);

         TRACE ("Done callback\n");
         continue;
      }

      pWorkItem = (WorkItem_t *)pOverlapped;
      switch (pWorkItem->Type)
      {
         case WI_SHUTDOWN:
            SetEvent (pWorkItem->Shutdown.hEvent);
            return 0;
         case WI_CALLBACK:
            TRACE ("Handling queued work item callback with (%p)\n",
                   pWorkItem->Callback.Context);

            /* FIXME - should we be doing anything with return value? */
            pWorkItem->Callback.Function (pWorkItem->Callback.Context);
            TRACE ("Done callback\n");
            break;
      }

      HeapFree (GetProcessHeap (), 0, pWorkItem);
   }

   return 0;
}


/******************************************************************************
 * Helper function to initialize the thread pool if it isn't already
 * running
 */
static BOOL InitializeThreadPool ()
{
   SYSTEM_INFO si;
   HANDLE hThread;
   DWORD i;

   if (hCompPort != INVALID_HANDLE_VALUE)
      return TRUE;

   /* FIXME - needs to be dynamic based on amount of requests */
   GetSystemInfo (&si);
   NumWorkerThreads = si.dwNumberOfProcessors;
   hCompPort = CreateIoCompletionPort (INVALID_HANDLE_VALUE, (HANDLE)0,
                                       MAGIC_COMP_KEY,
                                       NumWorkerThreads);
   if (hCompPort == (HANDLE)0)
   {
      ERR ("Failed to create completion port!\n");
      return FALSE;
   }

   /* FIXME - need threads to handle ASYNC I/O requests with callbacks */
   for (i = 0; i < NumWorkerThreads; i++)
   {
      hThread = CreateThread (NULL, 0, WorkerThreadMain, NULL, 0, NULL);
      if (hThread == (HANDLE)NULL)
      {
         ERR ("Failed to create worker thread!\n");
         return FALSE;
      }
      CloseHandle (hThread);
   }

   return TRUE;
}


/******************************************************************************
 * Helper function to terminate any running threads in the thread pool
 * FIXME - call from somewhere
 */
void CleanupThreadPool ()
{
   DWORD i;
   HANDLE hEvent;

   if (hCompPort == INVALID_HANDLE_VALUE)
      return;

   hEvent = CreateEventA (NULL, TRUE, FALSE, NULL);
   if (!hEvent)
      return;

   for (i = 0; i < NumWorkerThreads; i++)
   {
      WorkItem_t *pWorkItem =
         (WorkItem_t *)HeapAlloc (GetProcessHeap (), 0, sizeof (WorkItem_t));
      if (pWorkItem)
      {
         pWorkItem->Type = WI_SHUTDOWN;
         pWorkItem->Shutdown.hEvent = hEvent;
         ResetEvent (hEvent);
         PostQueuedCompletionStatus (hCompPort, 0, MAGIC_COMP_KEY,
                                     (LPOVERLAPPED)pWorkItem);

         if (WaitForSingleObject (hEvent, 1000) != WAIT_OBJECT_0)
            ERR ("Worker thread not shutting down properly!\n");
      }
   }

   CloseHandle (hEvent);
   CloseHandle (hCompPort);
}


/******************************************************************************
 *		QueueUserWorkItem (KERNEL32.@)
 *
 * FIXME - should really thunk to RtlQueueWorkItem; however, due to our
 * lack of dll separation, we don't have needed routines in ntdll yet
 * such as RtlCreateUserThread
 */
BOOL WINAPI QueueUserWorkItem (LPTHREAD_START_ROUTINE Function,
                               PVOID Context, ULONG Flags)
{
   WorkItem_t *pWorkItem;

   TRACE ("(%p, %p, 0x%lx)\n", Function, Context, Flags);

   if (Flags & ~WT_EXECUTELONGFUNCTION)
      FIXME ("Unsupported flags: 0x%lx\n", Flags);

   /* This function is remarkably ignorant of errors. Windows XP merrily
      accepts NULL Function or invalid flags, and then crashes down the line.
      Since we aim for crash-for-crash compatibility... */

   if (!InitializeThreadPool ())
      return FALSE;

   pWorkItem = HeapAlloc (GetProcessHeap (), 0, sizeof (WorkItem_t));
   if (!pWorkItem)
   {
      ERR ("Out of memory!\n");
      return FALSE;
   }

   pWorkItem->Type = WI_CALLBACK;
   pWorkItem->Callback.Function = Function;
   pWorkItem->Callback.Context = Context;

   if (!PostQueuedCompletionStatus (hCompPort, 0, MAGIC_COMP_KEY,
                                    (LPOVERLAPPED)pWorkItem))
   {
      ERR ("Failed to post work item!\n");
      HeapFree (GetProcessHeap (), 0, pWorkItem);
      return FALSE;
   }

   return TRUE;
}


/******************************************************************************
 *		BindIoCompletionCallback (KERNEL32.@)
 */
BOOL WINAPI BindIoCompletionCallback (HANDLE FileHandle,
                                      LPOVERLAPPED_COMPLETION_ROUTINE Function,
                                      ULONG Flags)
{
   TRACE ("(0x%lx, %p, %lu)\n", (ULONG)FileHandle, Function, Flags);

   if (FileHandle == INVALID_HANDLE_VALUE)
   {
      SetLastError (ERROR_INVALID_HANDLE);
      return FALSE;
   }

   /* This function seems to just succeed with non-zero Flags or a NULL
      function, and then crashes when used. Since we aim for
      crash-for-crash compatibility, we do no further checking here... */

   if (!InitializeThreadPool ())
      return FALSE;

   /* We're going to use the Function as the completion key */
   if (CreateIoCompletionPort (FileHandle, hCompPort, (ULONG_PTR)Function,
                               0) == (HANDLE)NULL)
      return FALSE;

   return TRUE;
}
