/* file.c
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
#include <stdlib.h>
#include <string.h>
#include "wine/server.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

#include "winternl.h"
#include "winioctl.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

/**************************************************************************
 *                 NtOpenFile				[NTDLL.@]
 *                 ZwOpenFile				[NTDLL.@]
 * FUNCTION: Opens a file
 * ARGUMENTS:
 *  FileHandle		Variable that receives the file handle on return
 *  DesiredAccess	Access desired by the caller to the file
 *  ObjectAttributes	Structue describing the file to be opened
 *  IoStatusBlock	Receives details about the result of the operation
 *  ShareAccess		Type of shared access the caller requires
 *  OpenOptions		Options for the file open
 */
NTSTATUS WINAPI NtOpenFile(
	OUT PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	ULONG ShareAccess,
	ULONG OpenOptions)
{
	FIXME("(%p,0x%08lx,%p,%p,0x%08lx,0x%08lx) stub\n",
	FileHandle, DesiredAccess, ObjectAttributes,
	IoStatusBlock, ShareAccess, OpenOptions);
	dump_ObjectAttributes (ObjectAttributes);
	return 0;
}

/**************************************************************************
 *		NtCreateFile				[NTDLL.@]
 *		ZwCreateFile				[NTDLL.@]
 * FUNCTION: Either causes a new file or directory to be created, or it opens
 *  an existing file, device, directory or volume, giving the caller a handle
 *  for the file object. This handle can be used by subsequent calls to
 *  manipulate data within the file or the file object's state of attributes.
 * ARGUMENTS:
 *	FileHandle		Points to a variable which receives the file handle on return
 *	DesiredAccess		Desired access to the file
 *	ObjectAttributes	Structure describing the file
 *	IoStatusBlock		Receives information about the operation on return
 *	AllocationSize		Initial size of the file in bytes
 *	FileAttributes		Attributes to create the file with
 *	ShareAccess		Type of shared access the caller would like to the file
 *	CreateDisposition	Specifies what to do, depending on whether the file already exists
 *	CreateOptions		Options for creating a new file
 *	EaBuffer		Undocumented
 *	EaLength		Undocumented
 */
NTSTATUS WINAPI NtCreateFile(
	OUT PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocateSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG CreateDisposition,
	ULONG CreateOptions,
	PVOID EaBuffer,
	ULONG EaLength)
{
	FIXME("(%p,0x%08lx,%p,%p,%p,0x%08lx,0x%08lx,0x%08lx,0x%08lx,%p,0x%08lx) stub\n",
	FileHandle,DesiredAccess,ObjectAttributes,
	IoStatusBlock,AllocateSize,FileAttributes,
	ShareAccess,CreateDisposition,CreateOptions,EaBuffer,EaLength);
	dump_ObjectAttributes (ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtReadFile					[NTDLL.@]
 *  ZwReadFile					[NTDLL.@]
 *
 * Parameters
 *   HANDLE32 		FileHandle
 *   HANDLE32 		Event 		OPTIONAL
 *   PIO_APC_ROUTINE 	ApcRoutine 	OPTIONAL
 *   PVOID 		ApcContext 	OPTIONAL
 *   PIO_STATUS_BLOCK 	IoStatusBlock
 *   PVOID 		Buffer
 *   ULONG 		Length
 *   PLARGE_INTEGER 	ByteOffset 	OPTIONAL
 *   PULONG 		Key 		OPTIONAL
 */
NTSTATUS WINAPI NtReadFile (
	HANDLE FileHandle,
	HANDLE EventHandle,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset,
	PULONG Key)
{
	FIXME("(0x%08x,0x%08x,%p,%p,%p,%p,0x%08lx,%p,%p),stub!\n",
	FileHandle,EventHandle,ApcRoutine,ApcContext,IoStatusBlock,Buffer,Length,ByteOffset,Key);
	return 0;
}

/******************************************************************************
 *  NtWriteFile					[NTDLL.@]
 *  ZwWriteFile					[NTDLL.@]
 *
 * Parameters
 *   HANDLE32 		FileHandle
 *   HANDLE32 		Event 		OPTIONAL
 *   PIO_APC_ROUTINE 	ApcRoutine 	OPTIONAL
 *   PVOID 		ApcContext 	OPTIONAL
 *   PIO_STATUS_BLOCK 	IoStatusBlock
 *   PVOID 		Buffer
 *   ULONG 		Length
 *   PLARGE_INTEGER 	ByteOffset 	OPTIONAL
 *   PULONG 		Key 		OPTIONAL
 */
NTSTATUS WINAPI NtWriteFile(
	HANDLE FileHandle,
	HANDLE EventHandle,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset,
	PULONG Key)
{
	FIXME("(0x%08x,0x%08x,%p,%p,%p,%p,0x%08lx,%p,%p),stub!\n",
	FileHandle,EventHandle,ApcRoutine,ApcContext,IoStatusBlock,Buffer,Length,ByteOffset,Key);
	return 0;
}

/**************************************************************************
 *		NtDeviceIoControlFile			[NTDLL.@]
 *		ZwDeviceIoControlFile			[NTDLL.@]
 */
NTSTATUS WINAPI NtDeviceIoControlFile(
	IN HANDLE DeviceHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL,
	IN PVOID UserApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	OUT PVOID OutputBuffer,
	IN ULONG OutputBufferSize)
{
	FIXME("(0x%08x,0x%08x,%p,%p,%p,0x%08lx,%p,0x%08lx,%p,0x%08lx): empty stub\n",
	DeviceHandle, Event, UserApcRoutine, UserApcContext,
	IoStatusBlock, IoControlCode, InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize);
	return 0;
}

/******************************************************************************
 * NtFsControlFile [NTDLL.@]
 * ZwFsControlFile [NTDLL.@]
 */
NTSTATUS WINAPI NtFsControlFile(
	IN HANDLE DeviceHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	OUT PVOID OutputBuffer,
	IN ULONG OutputBufferSize)
{
	FIXME("(0x%08x,0x%08x,%p,%p,%p,0x%08lx,%p,0x%08lx,%p,0x%08lx): stub\n",
	DeviceHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,IoControlCode,
	InputBuffer,InputBufferSize,OutputBuffer,OutputBufferSize);
	return 0;
}

/******************************************************************************
 *  NtSetVolumeInformationFile		[NTDLL.@]
 *  ZwSetVolumeInformationFile		[NTDLL.@]
 */
NTSTATUS WINAPI NtSetVolumeInformationFile(
	IN HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FsInformation,
        ULONG Length,
	FS_INFORMATION_CLASS FsInformationClass)
{
	FIXME("(0x%08x,%p,%p,0x%08lx,0x%08x) stub\n",
	FileHandle,IoStatusBlock,FsInformation,Length,FsInformationClass);
	return 0;
}

/******************************************************************************
 *  NtQueryInformationFile		[NTDLL.@]
 *  ZwQueryInformationFile		[NTDLL.@]
 */
NTSTATUS WINAPI NtQueryInformationFile(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation,
	ULONG Length,
	FILE_INFORMATION_CLASS FileInformationClass)
{
	FIXME("(0x%08x,%p,%p,0x%08lx,0x%08x),stub!\n",
	FileHandle,IoStatusBlock,FileInformation,Length,FileInformationClass);
	return 0;
}

/******************************************************************************
 *  NtSetInformationFile		[NTDLL.@]
 *  ZwSetInformationFile		[NTDLL.@]
 */
NTSTATUS WINAPI
NtSetInformationFile (HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
                      PVOID FileInformation, ULONG Length,
                      FILE_INFORMATION_CLASS FileInformationClass)
{
   NTSTATUS Ret = STATUS_SUCCESS;

   TRACE ("(0x%lx, %p, %p, %lu, %d)\n", FileHandle, IoStatusBlock,
          FileInformation, Length, FileInformationClass);

   if (!IoStatusBlock || !FileInformation)
      return STATUS_ACCESS_VIOLATION;

   if (FileHandle == INVALID_HANDLE_VALUE)
      return STATUS_OBJECT_TYPE_MISMATCH;

   IoStatusBlock->u.Status = STATUS_SUCCESS;
   IoStatusBlock->Information = 0;

   switch (FileInformationClass)
   {
      case FileCompletionInformation: {
         if (Length < sizeof (FILE_COMPLETION_INFORMATION))
            Ret = STATUS_INFO_LENGTH_MISMATCH;
         else
         {
            PFILE_COMPLETION_INFORMATION pInfo =
               (PFILE_COMPLETION_INFORMATION)FileInformation;

            SERVER_START_REQ (set_handle_completion_port)
            {
               req->handle = FileHandle;
               req->completion_port = pInfo->CompletionPort;
               req->completion_key = pInfo->CompletionKey;
               Ret = wine_server_call (req);
            }
            SERVER_END_REQ;

            /* Output error if wrong handle type, as may be our problem */
            if (Ret == STATUS_INVALID_PARAMETER)
               ERR ("Failed to set I/O completion port - might be unsupported handle type?\n");
         }

         break;
      }

      default:
         FIXME ("Unimplemented class %d\n", FileInformationClass);
   }

   return Ret;
}

/******************************************************************************
 *  NtQueryDirectoryFile	[NTDLL.@]
 *  ZwQueryDirectoryFile	[NTDLL.@]
 *  ZwQueryDirectoryFile
 */
NTSTATUS WINAPI NtQueryDirectoryFile(
	IN HANDLE FileHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	OUT PVOID FileInformation,
	IN ULONG Length,
	IN FILE_INFORMATION_CLASS FileInformationClass,
	IN BOOLEAN ReturnSingleEntry,
	IN PUNICODE_STRING FileName OPTIONAL,
	IN BOOLEAN RestartScan)
{
	FIXME("(0x%08x 0x%08x %p %p %p %p 0x%08lx 0x%08x 0x%08x %p 0x%08x\n",
	FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FileInformation,
	Length, FileInformationClass, ReturnSingleEntry,
	debugstr_us(FileName),RestartScan);
	return 0;
}

/******************************************************************************
 *  NtQueryVolumeInformationFile		[NTDLL.@]
 *  ZwQueryVolumeInformationFile		[NTDLL.@]
 */
NTSTATUS WINAPI NtQueryVolumeInformationFile (
	IN HANDLE FileHandle,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	OUT PVOID FSInformation,
	IN ULONG Length,
	IN FS_INFORMATION_CLASS FSInformationClass)
{
	ULONG len = 0;

	FIXME("(0x%08x %p %p 0x%08lx 0x%08x) stub!\n",
	FileHandle, IoStatusBlock, FSInformation, Length, FSInformationClass);

	switch ( FSInformationClass )
	{
	  case FileFsVolumeInformation:
	    len = sizeof( FILE_FS_VOLUME_INFORMATION );
	    break;
	  case FileFsLabelInformation:
	    len = 0;
	    break;

	  case FileFsSizeInformation:
	    len = sizeof( FILE_FS_SIZE_INFORMATION );
	    break;

	  case FileFsDeviceInformation:
	    len = sizeof( FILE_FS_DEVICE_INFORMATION );
	    break;
	  case FileFsAttributeInformation:
	    len = sizeof( FILE_FS_ATTRIBUTE_INFORMATION );
	    break;

	  case FileFsControlInformation:
	    len = 0;
	    break;

	  case FileFsFullSizeInformation:
	    len = 0;
	    break;

	  case FileFsObjectIdInformation:
	    len = 0;
	    break;

	  case FileFsMaximumInformation:
	    len = 0;
	    break;
	}

	if (Length < len)
	  return STATUS_BUFFER_TOO_SMALL;

	switch ( FSInformationClass )
	{
	  case FileFsVolumeInformation:
	    break;
	  case FileFsLabelInformation:
	    break;

	  case FileFsSizeInformation:
	    break;

	  case FileFsDeviceInformation:
	    if (FSInformation)
	    {
	      FILE_FS_DEVICE_INFORMATION * DeviceInfo = FSInformation;
	      DeviceInfo->DeviceType = FILE_DEVICE_DISK;
	      DeviceInfo->Characteristics = 0;
	      break;
	    };
	  case FileFsAttributeInformation:
	    break;

	  case FileFsControlInformation:
	    break;

	  case FileFsFullSizeInformation:
	    break;

	  case FileFsObjectIdInformation:
	    break;

	  case FileFsMaximumInformation:
	    break;
	}
	IoStatusBlock->DUMMYUNIONNAME.Status = STATUS_SUCCESS;
	IoStatusBlock->Information = len;
	return STATUS_SUCCESS;
}



