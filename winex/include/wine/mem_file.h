/*
 * Mem_file.h - ram-based read-only files
 *
 * Copyright 2008 TransGaming Technologies
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
