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

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(iocomp);


/******************************************************************************
 *		CreateIoCompletionPort (KERNEL32.@)
 */
HANDLE WINAPI CreateIoCompletionPort
(HANDLE hFileHandle, HANDLE hExistingCompletionPort, ULONG_PTR dwCompletionKey,
 DWORD dwNumberOfConcurrentThreads)
{
   HANDLE hNewPort = INVALID_HANDLE_VALUE;
   NTSTATUS Ret;

   TRACE ("(0x%x, 0x%x, %lu, %lu)\n", hFileHandle,
          hExistingCompletionPort, dwCompletionKey,
          dwNumberOfConcurrentThreads);

   /* If given a completion port, must also be given a file handle with
      which we are to associate it */
   if ((hExistingCompletionPort != (HANDLE)NULL) &&
       (hFileHandle == INVALID_HANDLE_VALUE))
   {
      SetLastError (ERROR_INVALID_PARAMETER);
      return (HANDLE)NULL;
   }

   /* FIXME - this should probably be done in NtCreateIoCompletion();
      however, we don't currently have easy access to the needed info
      in ntdll */
   if (!dwNumberOfConcurrentThreads)
   {
      SYSTEM_INFO si;

      GetSystemInfo (&si);
      dwNumberOfConcurrentThreads = si.dwNumberOfProcessors;
   }

   /* If not given a completion port, we must create one */
   if (hExistingCompletionPort == (HANDLE)NULL)
   {
      Ret = NtCreateIoCompletion (&hNewPort, IO_COMPLETION_ALL_ACCESS,
                                  NULL, dwNumberOfConcurrentThreads);
      if (Ret)
      {
         SetLastError (RtlNtStatusToDosError (Ret));
         return (HANDLE)NULL;
      }

      hExistingCompletionPort = hNewPort;
   }

   /* if hFileHandle, call NtSetFileInformation */
   if (hFileHandle != INVALID_HANDLE_VALUE)
   {
      IO_STATUS_BLOCK IoBlock;
      FILE_COMPLETION_INFORMATION FileInfo;

      FileInfo.CompletionPort = hExistingCompletionPort;
      FileInfo.CompletionKey = dwCompletionKey;
      Ret = NtSetInformationFile (hFileHandle, &IoBlock, &FileInfo, sizeof (FileInfo),
                                  FileCompletionInformation);
      if (Ret != STATUS_SUCCESS)
      {
         if (hNewPort != INVALID_HANDLE_VALUE)
            NtClose (hNewPort);
         SetLastError (RtlNtStatusToDosError (Ret));
         return (HANDLE)NULL;
      }
   }


   return hExistingCompletionPort;
}


/******************************************************************************
 *		GetQueuedCompletionStatus (KERNEL32.@)
 */
BOOL WINAPI GetQueuedCompletionStatus
(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
 PULONG_PTR lpCompletionKey, LPOVERLAPPED *lplpOverlapped,
 DWORD dwMilliseconds)
{
   IO_STATUS_BLOCK StatusBlock;
   NTSTATUS Ret;

   TRACE ("(%x, %p, %p, %p, %ld)\n", CompletionPort,
          lpNumberOfBytesTransferred, lpCompletionKey, lplpOverlapped,
          dwMilliseconds);

   if (!lpNumberOfBytesTransferred)
   {
      SetLastError (ERROR_INVALID_PARAMETER);
      return FALSE;
   }

   if (dwMilliseconds == INFINITE)
      Ret = NtRemoveIoCompletion (CompletionPort, lpCompletionKey,
                                  lplpOverlapped, &StatusBlock, NULL);
   else
   {
      LARGE_INTEGER WaitTime;

      WaitTime.QuadPart = (LONGLONG)dwMilliseconds * 1000;
      Ret = NtRemoveIoCompletion (CompletionPort, lpCompletionKey,
                                  lplpOverlapped, &StatusBlock, &WaitTime);
   }

   if (Ret)
   {
      SetLastError (RtlNtStatusToDosError (Ret));
      *lpNumberOfBytesTransferred = 0;
      return FALSE;
   }

   *lpNumberOfBytesTransferred = StatusBlock.Information;
   return TRUE;
}


/******************************************************************************
 *		PostQueuedCompletionStatus (KERNEL32.@)
 */
BOOL WINAPI PostQueuedCompletionStatus
(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
 ULONG_PTR pCompletionKey, LPOVERLAPPED lpOverlapped)
{
   NTSTATUS Ret;

   TRACE ("(%x, %lu, %lu, %p)\n", CompletionPort,
          dwNumberOfBytesTransferred, pCompletionKey, lpOverlapped);

   Ret = NtSetIoCompletion (CompletionPort, pCompletionKey, lpOverlapped,
                            0, dwNumberOfBytesTransferred);
   if (Ret)
   {
      SetLastError (RtlNtStatusToDosError (Ret));
      return FALSE;
   }

   return TRUE;
}
