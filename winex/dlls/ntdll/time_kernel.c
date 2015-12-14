/*
 * Win32 kernel functions
 *
 * Copyright 1995 Martin von Loewis and Cameron Heide
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
#include "config.h"

#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <unistd.h>
#include "wine/file.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32);

/***********************************************************************
 *              GetLocalTime            (KERNEL32.@)
 */
VOID WINAPI GetLocalTime(LPSYSTEMTIME systime)
{
    time_t local_time;
    struct tm *local_tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    local_time = tv.tv_sec;
    local_tm = localtime(&local_time);

    systime->wYear = local_tm->tm_year + 1900;
    systime->wMonth = local_tm->tm_mon + 1;
    systime->wDayOfWeek = local_tm->tm_wday;
    systime->wDay = local_tm->tm_mday;
    systime->wHour = local_tm->tm_hour;
    systime->wMinute = local_tm->tm_min;
    systime->wSecond = local_tm->tm_sec;
    systime->wMilliseconds = (tv.tv_usec / 1000) % 1000;
}

/***********************************************************************
 *              GetSystemTime            (KERNEL32.@)
 */
VOID WINAPI GetSystemTime(LPSYSTEMTIME systime)
{
    time_t system_time;
    struct tm *system_tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    system_time = tv.tv_sec;
    system_tm = gmtime(&system_time);

    systime->wYear = system_tm->tm_year + 1900;
    systime->wMonth = system_tm->tm_mon + 1;
    systime->wDayOfWeek = system_tm->tm_wday;
    systime->wDay = system_tm->tm_mday;
    systime->wHour = system_tm->tm_hour;
    systime->wMinute = system_tm->tm_min;
    systime->wSecond = system_tm->tm_sec;
    systime->wMilliseconds = (tv.tv_usec / 1000) % 1000;
}
