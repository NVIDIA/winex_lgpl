/*
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * Main reference:
 *   http://www.microsoft.com/technet/sysinternals/information/IoCompletionPorts.mspx
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "config.h"

#include <unistd.h>

#include "winternl.h"
#include "wine/server.h"
#include "async.h"
#include "wine/debug.h"


WINE_DEFAULT_DEBUG_CHANNEL(iocomp);


/******************************************************************************
 *		NtCreateIoCompletion (ntdll.@)
 */
NTSTATUS WINAPI NtCreateIoCompletion
(PHANDLE pIoCompletionHandle, ACCESS_MASK DesiredAccess,
 POBJECT_ATTRIBUTES pObjectAttributes, ULONG NumberOfConcurrentThreads)
{
   NTSTATUS Ret;

   TRACE ("(%p, 0x%lx, %p, %lu)\n",
          pIoCompletionHandle, DesiredAccess, pObjectAttributes,
          NumberOfConcurrentThreads);

   if (pObjectAttributes)
      FIXME ("pObjectAttributes are not supported\n");

   if (pIoCompletionHandle == NULL)
      return STATUS_INVALID_PARAMETER;

   if (NumberOfConcurrentThreads == 0)
   {
      FIXME ("Need to query number of processors\n");
      NumberOfConcurrentThreads = 2;
   }

   SERVER_START_REQ(create_io_completion)
   {
      req->num_threads = NumberOfConcurrentThreads;
      Ret = wine_server_call (req);
      *pIoCompletionHandle = reply->handle;
   }
   SERVER_END_REQ;

   TRACE ("Ret=0x%lx, *pIoCompletionHandle=0x%x\n", Ret,
          *pIoCompletionHandle);
   return Ret;
}


/******************************************************************************
 * Helper routine to handle an async I/O request. It is reponsible for
 * doing the actual work and then passing the result back */
static NTSTATUS HandleAsyncIO (LPOVERLAPPED *lplpOverlapped,
                               PIO_STATUS_BLOCK pIoStatusBlock,
                               async_private *pAsync)
{
   DWORD Status;

   WaitForSingleObject (pAsync->hStartupEvent, INFINITE);

   Status = pAsync->ops->get_status (pAsync);

   if (Status == STATUS_PENDING)
      pAsync->func (pAsync);

   /* Check if still PENDING; if so, pass back & restart */
   Status = pAsync->ops->get_status (pAsync);
   if (Status == STATUS_PENDING)
   {
      register_old_async (pAsync);
      return Status;
   }

   *lplpOverlapped = pAsync->lpOverlapped;
   pIoStatusBlock->Information = pAsync->lpOverlapped->InternalHigh;
   pIoStatusBlock->u.Status = Status;

   /* This will destroy the request */
   register_old_async (pAsync);

   return Status;
}


/******************************************************************************
 *		NtRemoveIoCompletion (ntdll.@)
 */
NTSTATUS WINAPI NtRemoveIoCompletion
(HANDLE IoCompletionHandle, PULONG_PTR ppCompletionKey,
 LPOVERLAPPED *lplpOverlapped, PIO_STATUS_BLOCK pIoStatusBlock,
 PLARGE_INTEGER pWaitTime)
{
   NTSTATUS Ret;
   int cookie;
   async_private *pAsync = NULL;

   TRACE ("(0x%x, %p, %p, %p, %p)\n", IoCompletionHandle,
          ppCompletionKey, lplpOverlapped, pIoStatusBlock, pWaitTime);

   if (!ppCompletionKey || !lplpOverlapped || !pIoStatusBlock)
      return STATUS_INVALID_PARAMETER;

 restart:

   /* Ask for a task to do. If one's given to us, run with it. Otherwise,
      wait for one to be given to us */
   SERVER_START_REQ(remove_io_completion)
   {
      req->handle = IoCompletionHandle;
      req->cookie = &cookie;

      if (pWaitTime)
      {
         req->sec = pWaitTime->QuadPart / 1000000;
         req->usec = pWaitTime->QuadPart % 1000000;
         req->select_flags = SELECT_TIMEOUT;
      }
      else
      {
         req->sec = 0;
         req->usec = 0;
         req->select_flags = 0;
      }

      Ret = wine_server_call (req);
      pIoStatusBlock->u.Status = Ret;
      if (Ret == STATUS_SUCCESS)
      {
         *ppCompletionKey = (ULONG_PTR)reply->completion_key;
         *lplpOverlapped = reply->overlapped;
         pIoStatusBlock->Information = reply->num_bytes;
         pAsync = (async_private *)reply->async_private;
      }
   }
   SERVER_END_REQ;

   /* This can timeout immediately if a timeout of 0 is specified and
      no tasks are available */
   if (Ret == STATUS_TIMEOUT)
   {
      TRACE ("Timed out\n");
      return Ret;
   }

   if (Ret == STATUS_SUCCESS)
   {
      if (pAsync)
      {
         Ret = HandleAsyncIO (lplpOverlapped, pIoStatusBlock, pAsync);
         if (Ret == STATUS_PENDING)
            goto restart;
      }

      TRACE ("Ret=0x%lx Key=0x%lx Overlapped=%p NumBytes=%lu\n", Ret,
             *ppCompletionKey, *lplpOverlapped, pIoStatusBlock->Information);
      return Ret;
   }

   if (Ret == STATUS_PENDING)
      Ret = wait_server_reply (&cookie);

   /* If we were told to abandon our waiting, that means we've been
      given a task to do */
   if (Ret == STATUS_ABANDONED_WAIT_0)
   {
      SERVER_START_REQ(retrieve_assigned_io_completion)
      {
         req->handle = IoCompletionHandle;
         Ret = wine_server_call (req);
         if (Ret == STATUS_SUCCESS)
         {
            *ppCompletionKey = (ULONG_PTR)reply->completion_key;
            *lplpOverlapped = reply->overlapped;
            pIoStatusBlock->Information = reply->num_bytes;
            pAsync = (async_private *)reply->async_private;
         }
      }
      SERVER_END_REQ;
   }

   pIoStatusBlock->u.Status = Ret;

   if ((Ret == STATUS_SUCCESS) && pAsync)
   {
      Ret = HandleAsyncIO (lplpOverlapped, pIoStatusBlock, pAsync);
      if (Ret == STATUS_PENDING)
         goto restart;
   }

   TRACE ("Ret=0x%lx Key=0x%lx Overlapped=%p NumBytes=%lu\n", Ret,
          *ppCompletionKey, *lplpOverlapped, pIoStatusBlock->Information);
   return Ret;
}


/******************************************************************************
 *		NtSetIoCompletion (ntdll.@)
 */
NTSTATUS WINAPI NtSetIoCompletion
(HANDLE IoCompletionHandle, ULONG_PTR pCompletionKey,
 LPOVERLAPPED lpOverlapped, ULONG NumberOfBytesTransferred,
 ULONG NumberOfBytesToTransfer)
{
   NTSTATUS Ret;

   TRACE ("(0x%x, 0x%lx, %p, %lu, %lu)\n",
          IoCompletionHandle, pCompletionKey, lpOverlapped,
          NumberOfBytesTransferred, NumberOfBytesToTransfer);

   if (NumberOfBytesTransferred)
      WARN ("NumberOfBytesTransferred != 0 not handled\n");


   SERVER_START_REQ(set_io_completion)
   {
      req->handle = IoCompletionHandle;
      req->completion_key = (void *)pCompletionKey;
      req->overlapped = lpOverlapped;
      req->num_bytes = NumberOfBytesToTransfer;
      Ret = wine_server_call (req);
   }
   SERVER_END_REQ;

   TRACE ("Ret=0x%lx\n", Ret);
   return Ret;
}
