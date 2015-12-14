/*
 * Win32 kernel functions
 *
 * Copyright 1995 Martin von Loewis and Cameron Heide
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/times.h>
#include <limits.h>
#include "wine/file.h"
#include "winternl.h"
#include "winerror.h"
#include "winnls.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32);

/* maximum time adjustment in seconds for SetLocalTime and SetSystemTime */
#define SETTIME_MAX_ADJUST 120


/* This structure is used to store strings that represent all of the time zones
   in the world. (This is used to help GetTimeZoneInformation)
*/
struct tagTZ_INFO
{
    const char *psTZFromUnix;
    WCHAR psTZWindows[32];
    int bias;
    int dst;
};

static const struct tagTZ_INFO TZ_INFO[] =
{
   {"MHT",
    {'D','a','t','e','l','i','n','e',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -720, 0},
   {"SST",
    {'S','a','m','o','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    660, 0},
   {"HST",
    {'H','a','w','a','i','i','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    600, 0},
   {"AKDT",
    {'A','l','a','s','k','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    480, 1},
   {"PDT",
    {'P','a','c','i','f','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    420, 1},
   {"MST",
    {'U','S',' ','M','o','u','n','t','a','i','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    420, 0},
   {"MDT",
    {'M','o','u','n','t','a','i','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    360, 1},
   {"CST",
    {'C','e','n','t','r','a','l',' ','A','m','e','r','i','c','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    360, 0},
   {"CDT",
    {'C','e','n','t','r','a','l',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    300, 1},
   {"COT",
    {'S','A',' ','P','a','c','i','f','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    300, 0},
   {"EDT",
    {'E','a','s','t','e','r','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
     240, 1},
   {"EST",
    {'U','S',' ','E','a','s','t','e','r','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    300, 0},
   {"ADT",
    {'A','t','l','a','n','t','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    180, 1},
   {"VET",
    {'S','A',' ','W','e','s','t','e','r','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
     240, 0},
   {"CLT",
    {'P','a','c','i','f','i','c',' ','S','A',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
     240, 0},
   {"NDT",
    {'N','e','w','f','o','u','n','d','l','a','n','d',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    150, 1},
   {"BRT",
    {'E','.',' ','S','o','u','t','h',' ','A','m','e','r','i','c','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    180, 0},
   {"ART",
    {'S','A',' ','E','a','s','t','e','r','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    180, 0},
   {"WGST",
    {'G','r','e','e','n','l','a','n','d',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    120, 1},
   {"GST",
    {'M','i','d','-','A','t','l','a','n','t','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    120, 0},
   {"AZOST",
    {'A','z','o','r','e','s',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    0, 1},
   {"CVT",
    {'C','a','p','e',' ','V','e','r','d','e',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    60, 0},
   {"WET",
    {'G','r','e','e','n','w','i','c','h',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    0, 0},
   {"BST",
    {'G','M','T',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -60, 1},
   {"GMT",
    {'G','M','T',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    0, 0},
   {"CEST",
    {'C','e','n','t','r','a','l',' ','E','u','r','o','p','e',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -120, 1},
   {"WAT",
    {'W','.',' ','C','e','n','t','r','a','l',' ','A','f','r','i','c','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -60, 0},
   {"EEST",
    {'E','.',' ','E','u','r','o','p','e',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -180, 1},
   {"EET",
    {'E','g','y','p','t',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -120, 0},
   {"CAT",
    {'S','o','u','t','h',' ','A','f','r','i','c','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -120, 0},
   {"IST",
    {'I','s','r','a','e','l',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -120, 0},
   {"ADT",
    {'A','r','a','b','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -240, 1},
   {"AST",
    {'A','r','a','b',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -180, 0},
   {"MSD",
    {'R','u','s','s','i','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -240, 1},
   {"EAT",
    {'E','.',' ','A','f','r','i','c','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -180, 0},
   {"IRST",
    {'I','r','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -270, 1},
   {"GST",
    {'A','r','a','b','i','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -240, 0},
   {"AZST",
    {'C','a','u','c','a','s','u','s',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -300, 1},
   {"AFT",
    {'A','f','g','h','a','n','i','s','t','a','n',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -270, 0},
   {"YEKST",
    {'E','k','a','t','e','r','i','n','b','u','r','g',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -360, 1},
   {"PKT",
    {'W','e','s','t',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -300, 0},
   {"IST",
    {'I','n','d','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -330, 0},
   {"NPT",
    {'N','e','p','a','l',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -345, 0},
   {"ALMST",
    {'N','.',' ','C','e','n','t','r','a','l',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -420, 1},
   {"BDT",
    {'C','e','n','t','r','a','l',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -360, 0},
   {"LKT",
    {'S','r','i',' ','L','a','n','k','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -360, 0},
   {"MMT",
    {'M','y','a','n','m','a','r',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -390, 0},
   {"ICT",
    {'S','E',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -420, 0},
   {"KRAST",
    {'N','o','r','t','h',' ','A','s','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -480, 1},
   {"CST",
    {'C','h','i','n','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -480, 0},
   {"IRKST",
    {'N','o','r','t','h',' ','A','s','i','a',' ','E','a','s','t',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -540, 1},
   {"SGT",
    {'M','a','l','a','y',' ','P','e','n','i','n','s','u','l','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -480, 0},
   {"WST",
    {'W','.',' ','A','u','s','t','r','a','l','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -480, 0},
   {"JST",
    {'T','o','k','y','o',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -540, 0},
   {"KST",
    {'K','o','r','e','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -540, 0},
   {"YAKST",
    {'Y','a','k','u','t','s','k',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -600, 1},
   {"CST",
    {'C','e','n','.',' ','A','u','s','t','r','a','l','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -570, 0},
   {"EST",
    {'E','.',' ','A','u','s','t','r','a','l','i','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -600, 0},
   {"GST",
    {'W','e','s','t',' ','P','a','c','i','f','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -600, 0},
   {"VLAST",
    {'V','l','a','d','i','v','o','s','t','o','k',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -660, 1},
   {"MAGST",
    {'C','e','n','t','r','a','l',' ','P','a','c','i','f','i','c',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -720, 1},
   {"NZST",
    {'N','e','w',' ','Z','e','a','l','a','n','d',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -720, 0},
   {"FJT",
    {'F','i','j','i',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -720, 0},
   {"TOT",
    {'T','o','n','g','a',' ','S','t','a','n','d','a','r','d',' ','T','i','m','e','\0'},
    -780, 0}
};

/* TIME_GetTZAsStr: helper function that returns the given timezone as a string.
   This could be done with a hash table instead of merely iterating through
   a table, however with the small amount of entries (60 or so) I didn't think
   it was worth it. */
static const WCHAR* TIME_GetTZAsStr (time_t utc, int bias, int dst)
{
   char psTZName[7];
   struct tm *ptm = localtime(&utc);
   int i;

   if (!strftime (psTZName, 7, "%Z", ptm))
      return (NULL);

   for (i=0; i<(sizeof(TZ_INFO) / sizeof(struct tagTZ_INFO)); i++)
   {
      if ( strcmp(TZ_INFO[i].psTZFromUnix, psTZName) == 0 &&
           TZ_INFO[i].bias == bias &&
           TZ_INFO[i].dst == dst
         )
            return TZ_INFO[i].psTZWindows;
   }

   return (NULL);
}


/* TIME_GetBias: helper function calculates delta local time from UTC */
static int TIME_GetBias( time_t utc, int *pdaylight)
{
    struct tm *ptm = localtime(&utc);
    *pdaylight = ptm->tm_isdst; /* daylight for local timezone */
    ptm = gmtime(&utc);
    ptm->tm_isdst = *pdaylight; /* use local daylight, not that of Greenwich */
    return (int)(utc-mktime(ptm));
}


/***********************************************************************
 *              SetLocalTime            (KERNEL32.@)
 *
 *  Sets the local time using current time zone and daylight
 *  savings settings.
 *
 * RETURNS
 *
 *  True if the time was set, false if the time was invalid or the
 *  necessary permissions were not held.
 */
BOOL WINAPI SetLocalTime(
    const SYSTEMTIME *systime) /* [in] The desired local time. */
{
    struct timeval tv;
    struct tm t;
    time_t sec;
    time_t oldsec=time(NULL);
    int err;

    /* get the number of seconds */
    t.tm_sec = systime->wSecond;
    t.tm_min = systime->wMinute;
    t.tm_hour = systime->wHour;
    t.tm_mday = systime->wDay;
    t.tm_mon = systime->wMonth - 1;
    t.tm_year = systime->wYear - 1900;
    t.tm_isdst = -1;
    sec = mktime (&t);

    /* set the new time */
    tv.tv_sec = sec;
    tv.tv_usec = systime->wMilliseconds * 1000;

    /* error and sanity check*/
    if( sec == (time_t)-1 || abs((int)(sec-oldsec)) > SETTIME_MAX_ADJUST ){
        err = 1;
        SetLastError(ERROR_INVALID_PARAMETER);
    } else {
#ifdef HAVE_SETTIMEOFDAY
        err=settimeofday(&tv, NULL); /* 0 is OK, -1 is error */
        if(err == 0)
            return TRUE;
        SetLastError(ERROR_PRIVILEGE_NOT_HELD);
#else
	err = 1;
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
#endif
    }
    ERR("Cannot set time to %d/%d/%d %d:%d:%d Time adjustment %ld %s\n",
            systime->wYear, systime->wMonth, systime->wDay, systime->wHour,
            systime->wMinute, systime->wSecond,
            sec-oldsec, err == -1 ? "No Permission" :
                sec==(time_t)-1 ? "" : "is too large." );
    return FALSE;
}


/***********************************************************************
 *           GetSystemTimeAdjustment     (KERNEL32.@)
 *
 *  Indicates the period between clock interrupt and the amount the clock
 *  is adjusted each interrupt so as to keep it insync with an external source.
 *
 * RETURNS
 *
 *  Always returns true.
 *
 * BUGS
 *
 *  Only the special case of disabled time adjustments is supported.
 */
BOOL WINAPI GetSystemTimeAdjustment(
    PDWORD lpTimeAdjustment,         /* [out] The clock adjustment per interupt in 100's of nanoseconds. */
    PDWORD lpTimeIncrement,          /* [out] The time between clock interupts in 100's of nanoseconds. */
    PBOOL  lpTimeAdjustmentDisabled) /* [out] The clock synchonisation has been disabled. */
{
    *lpTimeAdjustment = 0;
    *lpTimeIncrement = 0;
    *lpTimeAdjustmentDisabled = TRUE;
    return TRUE;
}


/***********************************************************************
 *              SetSystemTime            (KERNEL32.@)
 *
 *  Sets the system time (utc).
 *
 * RETURNS
 *
 *  True if the time was set, false if the time was invalid or the
 *  necessary permissions were not held.
 */
BOOL WINAPI SetSystemTime(
    const SYSTEMTIME *systime) /* [in] The desired system time. */
{
    struct timeval tv;
    struct timezone tz;
    struct tm t;
    time_t sec, oldsec;
    int dst, bias;
    int err;

    /* call gettimeofday to get the current timezone */
    gettimeofday(&tv, &tz);
    oldsec=tv.tv_sec;
    /* get delta local time from utc */
    bias=TIME_GetBias(oldsec,&dst);

    /* get the number of seconds */
    t.tm_sec = systime->wSecond;
    t.tm_min = systime->wMinute;
    t.tm_hour = systime->wHour;
    t.tm_mday = systime->wDay;
    t.tm_mon = systime->wMonth - 1;
    t.tm_year = systime->wYear - 1900;
    t.tm_isdst = dst;
    sec = mktime (&t);
    /* correct for timezone and daylight */
    sec += bias;

    /* set the new time */
    tv.tv_sec = sec;
    tv.tv_usec = systime->wMilliseconds * 1000;

    /* error and sanity check*/
    if( sec == (time_t)-1 || abs((int)(sec-oldsec)) > SETTIME_MAX_ADJUST ){
        err = 1;
        SetLastError(ERROR_INVALID_PARAMETER);
    } else {
#ifdef HAVE_SETTIMEOFDAY
        err=settimeofday(&tv, NULL); /* 0 is OK, -1 is error */
        if(err == 0)
            return TRUE;
        SetLastError(ERROR_PRIVILEGE_NOT_HELD);
#else
	err = 1;
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
#endif
    }
    ERR("Cannot set time to %d/%d/%d %d:%d:%d Time adjustment %ld %s\n",
            systime->wYear, systime->wMonth, systime->wDay, systime->wHour,
            systime->wMinute, systime->wSecond,
            sec-oldsec, err == -1 ? "No Permission" :
                sec==(time_t)-1 ? "" : "is too large." );
    return FALSE;
}


/***********************************************************************
 *              GetTimeZoneInformation  (KERNEL32.@)
 *
 *  Fills in the a time zone information structure with values based on
 *  the current local time.
 *
 * RETURNS
 *
 *  The daylight savings time standard or TIME_ZONE_ID_INVALID if the call failed.
 */
DWORD WINAPI GetTimeZoneInformation(
    LPTIME_ZONE_INFORMATION tzinfo) /* [out] The time zone structure to be filled in. */
{
    time_t gmt;
    int bias, daylight;
    const WCHAR *psTZ;


    memset(tzinfo, 0, sizeof(TIME_ZONE_INFORMATION));

    gmt = time(NULL);
    bias=TIME_GetBias(gmt,&daylight);

    tzinfo->Bias = -bias / 60;
    tzinfo->StandardBias = 0;
    tzinfo->DaylightBias = -60;
    psTZ = TIME_GetTZAsStr (gmt, (-bias/60), daylight);
    if (psTZ) strcpyW( tzinfo->StandardName, psTZ );
    return TIME_ZONE_ID_STANDARD;
}


/***********************************************************************
 *              SetTimeZoneInformation  (KERNEL32.@)
 *
 *  Set the local time zone with values based on the time zone structure.
 *
 * RETURNS
 *
 *  True on successful setting of the time zone.
 *
 * BUGS
 *
 *  Use the obsolete unix timezone structure and tz_dsttime member.
 */
BOOL WINAPI SetTimeZoneInformation(
    const LPTIME_ZONE_INFORMATION tzinfo) /* [in] The new time zone. */
{
    struct timezone tz;

    tz.tz_minuteswest = tzinfo->Bias;
#ifdef DST_NONE
    tz.tz_dsttime = DST_NONE;
#else
    tz.tz_dsttime = 0;
#endif
    return !settimeofday(NULL, &tz);
}


/***********************************************************************
 *              SystemTimeToTzSpecificLocalTime  (KERNEL32.@)
 *
 *  Converts the system time (utc) to the local time in the specified time zone.
 *
 * RETURNS
 *
 *  Returns true when the local time was calculated.
 *
 * BUGS
 *
 *  Does not handle daylight savings time adjustments correctly.
 */
BOOL WINAPI SystemTimeToTzSpecificLocalTime(
    LPTIME_ZONE_INFORMATION lpTimeZoneInformation, /* [in] The desired time zone. */
    LPSYSTEMTIME            lpUniversalTime,       /* [in] The utc time to base local time on. */
    LPSYSTEMTIME            lpLocalTime)           /* [out] The local time in the time zone. */
{
  FIXME(":stub\n");
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
       return FALSE;
}


/***********************************************************************
 *              GetSystemTimeAsFileTime  (KERNEL32.@)
 *
 *  Fills in a file time structure with the current time in UTC format.
 */
VOID WINAPI GetSystemTimeAsFileTime(
    LPFILETIME time) /* [out] The file time struct to be filled with the system time. */
{
    NtQuerySystemTime( (LARGE_INTEGER *)time );
}


/*********************************************************************
 *      TIME_ClockTimeToFileTime    (olorin@fandra.org, 20-Sep-1998)
 *
 *  Used by GetProcessTimes to convert clock_t into FILETIME.
 *
 *      Differences to UnixTimeToFileTime:
 *          1) Divided by CLK_TCK
 *          2) Time is relative. There is no 'starting date', so there is
 *             no need in offset correction, like in UnixTimeToFileTime
 */
static void TIME_ClockTimeToFileTime(clock_t unix_time, LPFILETIME filetime)
{
    LONGLONG secs = RtlEnlargedUnsignedMultiply( unix_time, 10000000 );
    ((LARGE_INTEGER *)filetime)->QuadPart =
       RtlExtendedLargeIntegerDivide( secs, sysconf( _SC_CLK_TCK ), NULL );
}

/*********************************************************************
 *	GetProcessTimes				(KERNEL32.@)
 *
 *  Returns the user and kernel execution times of a process,
 *  along with the creation and exit times if known.
 *
 *  olorin@fandra.org:
 *  Would be nice to subtract the cpu time, used by Wine at startup.
 *  Also, there is a need to separate times used by different applications.
 *
 * RETURNS
 *
 *  Always returns true.
 *
 * BUGS
 *
 *  lpCreationTime, lpExitTime are NOT INITIALIZED.
 */
BOOL WINAPI GetProcessTimes(
    HANDLE     hprocess,       /* [in] The process to be queried (obtained from PROCESS_QUERY_INFORMATION). */
    LPFILETIME lpCreationTime, /* [out] The creation time of the process. */
    LPFILETIME lpExitTime,     /* [out] The exit time of the process if exited. */
    LPFILETIME lpKernelTime,   /* [out] The time spent in kernal routines in 100's of nanoseconds. */
    LPFILETIME lpUserTime)     /* [out] The time spent in user routines in 100's of nanoseconds. */
{
    struct tms tms;

    times(&tms);
    TIME_ClockTimeToFileTime(tms.tms_utime,lpUserTime);
    TIME_ClockTimeToFileTime(tms.tms_stime,lpKernelTime);
    return TRUE;
}

/*********************************************************************
 *	GetCalendarInfoA				(KERNEL32.@)
 *
 */
int WINAPI GetCalendarInfoA(LCID Locale, CALID Calendar, CALTYPE CalType,
			    LPSTR lpCalData, int cchData, LPDWORD lpValue)
{
    int ret;
    LPWSTR lpCalDataW = NULL;

    FIXME("(%08lx,%08lx,%08lx,%p,%d,%p): quarter-stub\n",
	  Locale, Calendar, CalType, lpCalData, cchData, lpValue);
    /* FIXME: Should verify if Locale is allowable in ANSI, as per MSDN */

    if(cchData)
      if(!(lpCalDataW = HeapAlloc(GetProcessHeap(), 0, cchData*sizeof(WCHAR)))) return 0;

    ret = GetCalendarInfoW(Locale, Calendar, CalType, lpCalDataW, cchData, lpValue);
    if(ret && lpCalDataW && lpCalData)
      WideCharToMultiByte(CP_ACP, 0, lpCalDataW, cchData, lpCalData, cchData, NULL, NULL);
    if(lpCalDataW)
      HeapFree(GetProcessHeap(), 0, lpCalDataW);

    return ret;
}

/*********************************************************************
 *	GetCalendarInfoW				(KERNEL32.@)
 *
 */
int WINAPI GetCalendarInfoW(LCID Locale, CALID Calendar, CALTYPE CalType,
			    LPWSTR lpCalData, int cchData, LPDWORD lpValue)
{
    FIXME("(%08lx,%08lx,%08lx,%p,%d,%p): quarter-stub\n",
	  Locale, Calendar, CalType, lpCalData, cchData, lpValue);

    if (CalType & CAL_NOUSEROVERRIDE)
	FIXME("flag CAL_NOUSEROVERRIDE used, not fully implemented\n");
    if (CalType & CAL_USE_CP_ACP)
	FIXME("flag CAL_USE_CP_ACP used, not fully implemented\n");

    if (CalType & CAL_RETURN_NUMBER) {
	if (lpCalData != NULL)
	    WARN("lpCalData not NULL (%p) when it should!\n", lpCalData);
	if (cchData != 0)
	    WARN("cchData not 0 (%d) when it should!\n", cchData);
    } else {
	if (lpValue != NULL)
	    WARN("lpValue not NULL (%p) when it should!\n", lpValue);
    }

    /* FIXME: No verification is made yet wrt Locale
     * for the CALTYPES not requiring GetLocaleInfoA */
    switch (CalType & ~(CAL_NOUSEROVERRIDE|CAL_RETURN_NUMBER|CAL_USE_CP_ACP)) {
	case CAL_ICALINTVALUE:
	    FIXME("Unimplemented caltype %ld\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_SCALNAME:
	    FIXME("Unimplemented caltype %ld\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_IYEAROFFSETRANGE:
	    FIXME("Unimplemented caltype %ld\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_SERASTRING:
	    FIXME("Unimplemented caltype %ld\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_SSHORTDATE:
	    return GetLocaleInfoW(Locale, LOCALE_SSHORTDATE, lpCalData, cchData);
	case CAL_SLONGDATE:
	    return GetLocaleInfoW(Locale, LOCALE_SLONGDATE, lpCalData, cchData);
	case CAL_SDAYNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME1, lpCalData, cchData);
	case CAL_SDAYNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME2, lpCalData, cchData);
	case CAL_SDAYNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME3, lpCalData, cchData);
	case CAL_SDAYNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME4, lpCalData, cchData);
	case CAL_SDAYNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME5, lpCalData, cchData);
	case CAL_SDAYNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME6, lpCalData, cchData);
	case CAL_SDAYNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME7, lpCalData, cchData);
	case CAL_SABBREVDAYNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME1, lpCalData, cchData);
	case CAL_SABBREVDAYNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME2, lpCalData, cchData);
	case CAL_SABBREVDAYNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME3, lpCalData, cchData);
	case CAL_SABBREVDAYNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME4, lpCalData, cchData);
	case CAL_SABBREVDAYNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME5, lpCalData, cchData);
	case CAL_SABBREVDAYNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME6, lpCalData, cchData);
	case CAL_SABBREVDAYNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME7, lpCalData, cchData);
	case CAL_SMONTHNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME1, lpCalData, cchData);
	case CAL_SMONTHNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME2, lpCalData, cchData);
	case CAL_SMONTHNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME3, lpCalData, cchData);
	case CAL_SMONTHNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME4, lpCalData, cchData);
	case CAL_SMONTHNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME5, lpCalData, cchData);
	case CAL_SMONTHNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME6, lpCalData, cchData);
	case CAL_SMONTHNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME7, lpCalData, cchData);
	case CAL_SMONTHNAME8:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME8, lpCalData, cchData);
	case CAL_SMONTHNAME9:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME9, lpCalData, cchData);
	case CAL_SMONTHNAME10:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME10, lpCalData, cchData);
	case CAL_SMONTHNAME11:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME11, lpCalData, cchData);
	case CAL_SMONTHNAME12:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME12, lpCalData, cchData);
	case CAL_SMONTHNAME13:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME13, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME1, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME2, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME3, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME4, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME5, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME6, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME7, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME8:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME8, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME9:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME9, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME10:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME10, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME11:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME11, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME12:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME12, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME13:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME13, lpCalData, cchData);
	case CAL_SYEARMONTH:
	    return GetLocaleInfoW(Locale, LOCALE_SYEARMONTH, lpCalData, cchData);
	case CAL_ITWODIGITYEARMAX:
	    FIXME("Unimplemented caltype %ld\n", CalType & 0xffff);
	    return E_FAIL;
	default: MESSAGE("Unknown caltype %ld\n",CalType & 0xffff);
		 return E_FAIL;
    }
    return 0;
}

/*********************************************************************
 *	SetCalendarInfoA				(KERNEL32.@)
 *
 */
int WINAPI	SetCalendarInfoA(LCID Locale, CALID Calendar, CALTYPE CalType, LPCSTR lpCalData)
{
    FIXME("(%08lx,%08lx,%08lx,%s): stub\n",
	  Locale, Calendar, CalType, debugstr_a(lpCalData));
    return 0;
}

/*********************************************************************
 *	SetCalendarInfoW				(KERNEL32.@)
 *
 */
int WINAPI	SetCalendarInfoW(LCID Locale, CALID Calendar, CALTYPE CalType, LPCWSTR lpCalData)
{
    FIXME("(%08lx,%08lx,%08lx,%s): stub\n",
	  Locale, Calendar, CalType, debugstr_w(lpCalData));
    return 0;
}


/****************************************************************************
 *		QueryPerformanceCounter (KERNEL32.@)
 */
BOOL WINAPI QueryPerformanceCounter (PLARGE_INTEGER counter)
{
   NTSTATUS status = NtQueryPerformanceCounter (counter, NULL);
   if (status)
      SetLastError (RtlNtStatusToDosError (status));
   return !status;
}


/****************************************************************************
 *		QueryPerformanceFrequency (KERNEL32.@)
 */
BOOL WINAPI QueryPerformanceFrequency (PLARGE_INTEGER frequency)
{
   LARGE_INTEGER counter;
   NTSTATUS status = NtQueryPerformanceCounter (&counter, frequency);
   if (status)
      SetLastError (RtlNtStatusToDosError (status));
   return !status;
}


/***********************************************************************
 *           GetTickCount       (KERNEL32.@)
 *
 * Returns the number of milliseconds, modulo 2^32, since the start
 * of the wineserver.
 */
DWORD WINAPI GetTickCount ()
{
   return (DWORD)NtGetTickCount ();
}

ULONGLONG WINAPI GetTickCount64 ()
{
   return NtGetTickCount64 ();
}
