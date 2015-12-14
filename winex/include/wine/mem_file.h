/*
 * Mem_file.h - ram-based read-only files
 *
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef _MEM_FILE_H_
#define _MEM_FILE_H_


extern BOOL MEMFILE_CreateMemFile (const char *filename, HANDLE hFile);

extern BOOL MEMFILE_SetFilePointerEx (HANDLE hFile, LARGE_INTEGER liDistance,
                                      PLARGE_INTEGER pNewPos, DWORD dwMethod);

extern BOOL MEMFILE_ReadFile (HANDLE hFile, LPVOID Buffer, DWORD BytesToRead,
                              LPDWORD BytesRead);

extern BOOL MEMFILE_ReadFileFromOffset (HANDLE hFile, LPVOID Buffer, DWORD BytesToRead,
                                        LPDWORD BytesRead, LARGE_INTEGER offset);

extern VOID MEMFILE_DeleteMemFile (HANDLE hFile);

#endif
