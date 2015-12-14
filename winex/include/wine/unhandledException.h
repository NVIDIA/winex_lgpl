/* unhandledException.h
 *
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#ifndef  __UNHANDLEDEXCEPTION_H__
# define __UNHANDLEDEXCEPTION_H__


#ifdef __APPLE__
# define WIN32_CRASHREPORTFILENAME      "p:\\win32_crashReport.txt"
# define WIN32_MINIDUMPFILENAME         "p:\\win32_minidump.dmp"
# define NTDLL_CRASHREPORTFILENAME      "p:\\ntdll_crashReport.txt"
# define NTDLL_MINIDUMPFILENAME         "p:\\ntdll_minidump.dmp"
#else
# define WIN32_CRASHREPORTFILENAME      "c:\\win32_crashReport.txt"
# define WIN32_MINIDUMPFILENAME         "c:\\win32_minidump.dmp"
# define NTDLL_CRASHREPORTFILENAME      "c:\\ntdll_crashReport.txt"
# define NTDLL_MINIDUMPFILENAME         "c:\\ntdll_minidump.dmp"
#endif

LONG WINAPI filterFunction(struct _EXCEPTION_POINTERS *exceptionInfo);
BOOL dumpExceptionInfo(struct _EXCEPTION_POINTERS *exceptionInfo, const char *stackTraceFile, const char *minidumpFile);
BOOL setupHandler();

#endif
