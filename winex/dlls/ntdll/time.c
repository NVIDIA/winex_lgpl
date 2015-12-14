/*
 * Conversion between Time and TimeFields
 *
 * RtlTimeToTimeFields, RtlTimeFieldsToTime and defines are taken from ReactOS and
 * adapted to wine with special permissions of the author
 * Rex Jolliff (rex@lvcablemodem.com)
 *
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
#include "config.h"

#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <unistd.h>

#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

#define TICKSPERSEC        10000000
#define TICKSPERMSEC       10000
#define SECSPERDAY         86400
#define SECSPERHOUR        3600
#define SECSPERMIN         60
#define MINSPERHOUR        60
#define HOURSPERDAY        24
#define EPOCHWEEKDAY       1
#define DAYSPERWEEK        7
#define EPOCHYEAR          1601
#define DAYSPERNORMALYEAR  365
#define DAYSPERLEAPYEAR    366
#define MONSPERYEAR        12

/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970  ((369 * 365 + 89) * 86400ULL)
/* 1601 to 1980 is 379 years plus 91 leap days */
#define SECS_1601_to_1980  ((379 * 365 + 91) * 86400ULL)

/* Constants for QueryPerformanceCounter() to avoid 64-bit division */

/* - base frequency */
#define QP_FREQ (1193182)

/* - this is 1.193182 << 21, which is smallest power of two that'll
     retain its full precision */
#define QP_SCALE (2502284)

/* - value to shift the result to do the division */
#define QP_DIVSHIFT (21)


extern struct timeval server_starttime;
static RTL_CRITICAL_SECTION TIME_sync_cs;

static const int YearLengths[2] = {DAYSPERNORMALYEAR, DAYSPERLEAPYEAR};
static const int MonthLengths[2][MONSPERYEAR] =
{
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};


void TIME_Init(void)
{
    RtlInitializeCriticalSection( &TIME_sync_cs );
    CRITICAL_SECTION_NAME( &TIME_sync_cs, "TIME_sync_cs" );
}

static inline int IsLeapYear(int Year)
{
	return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0) ? 1 : 0;
}

static inline void NormalizeTimeFields(CSHORT *FieldToNormalize, CSHORT *CarryField,int Modulus)
{
	*FieldToNormalize = (CSHORT) (*FieldToNormalize - Modulus);
	*CarryField = (CSHORT) (*CarryField + 1);
}

/******************************************************************************
 *  RtlTimeToTimeFields		[NTDLL.@]
 *
 */

VOID WINAPI RtlTimeToTimeFields(
	PLARGE_INTEGER liTime,
	PTIME_FIELDS TimeFields)
{
	const int *Months;
	int LeapSecondCorrections, SecondsInDay, CurYear;
	int LeapYear, CurMonth, GMTOffset;
	long int Days;
	LONGLONG Time = liTime->QuadPart;

	/* Extract millisecond from time and convert time into seconds */
	TimeFields->Milliseconds = (CSHORT) ((Time % TICKSPERSEC) / TICKSPERMSEC);
	Time = Time / TICKSPERSEC;

	/* FIXME: Compute the number of leap second corrections here */
	LeapSecondCorrections = 0;

	/* FIXME: get the GMT offset here */
	GMTOffset = 0;

	/* Split the time into days and seconds within the day */
	Days = Time / SECSPERDAY;
	SecondsInDay = Time % SECSPERDAY;

	/* Adjust the values for GMT and leap seconds */
	SecondsInDay += (GMTOffset - LeapSecondCorrections);
	while (SecondsInDay < 0)
	{ SecondsInDay += SECSPERDAY;
	  Days--;
	}
	while (SecondsInDay >= SECSPERDAY)
	{ SecondsInDay -= SECSPERDAY;
	  Days++;
	}

	/* compute time of day */
	TimeFields->Hour = (CSHORT) (SecondsInDay / SECSPERHOUR);
	SecondsInDay = SecondsInDay % SECSPERHOUR;
	TimeFields->Minute = (CSHORT) (SecondsInDay / SECSPERMIN);
	TimeFields->Second = (CSHORT) (SecondsInDay % SECSPERMIN);

	/* FIXME: handle the possibility that we are on a leap second (i.e. Second = 60) */

	/* compute day of week */
	TimeFields->Weekday = (CSHORT) ((EPOCHWEEKDAY + Days) % DAYSPERWEEK);

	/* compute year */
	CurYear = EPOCHYEAR;
	/* FIXME: handle calendar modifications */
	while (1)
	{ LeapYear = IsLeapYear(CurYear);
	  if (Days < (long) YearLengths[LeapYear])
	  { break;
	  }
	  CurYear++;
	  Days = Days - (long) YearLengths[LeapYear];
	}
	TimeFields->Year = (CSHORT) CurYear;

	/* Compute month of year */
	Months = MonthLengths[LeapYear];
	for (CurMonth = 0; Days >= (long) Months[CurMonth]; CurMonth++)
	  Days = Days - (long) Months[CurMonth];
	TimeFields->Month = (CSHORT) (CurMonth + 1);
	TimeFields->Day = (CSHORT) (Days + 1);
}
/******************************************************************************
 *  RtlTimeFieldsToTime		[NTDLL.@]
 *
 */
BOOLEAN WINAPI RtlTimeFieldsToTime(
	PTIME_FIELDS tfTimeFields,
	PLARGE_INTEGER Time)
{
	int CurYear, CurMonth;
	long long int rcTime;
	TIME_FIELDS TimeFields = *tfTimeFields;

	rcTime = 0;

	/* FIXME: normalize the TIME_FIELDS structure here */
	while (TimeFields.Second >= SECSPERMIN)
	{ NormalizeTimeFields(&TimeFields.Second, &TimeFields.Minute, SECSPERMIN);
	}
	while (TimeFields.Minute >= MINSPERHOUR)
	{ NormalizeTimeFields(&TimeFields.Minute, &TimeFields.Hour, MINSPERHOUR);
	}
	while (TimeFields.Hour >= HOURSPERDAY)
	{ NormalizeTimeFields(&TimeFields.Hour, &TimeFields.Day, HOURSPERDAY);
	}
	while (TimeFields.Day > MonthLengths[IsLeapYear(TimeFields.Year)][TimeFields.Month - 1])
	{ NormalizeTimeFields(&TimeFields.Day, &TimeFields.Month, SECSPERMIN);
	}
	while (TimeFields.Month > MONSPERYEAR)
	{ NormalizeTimeFields(&TimeFields.Month, &TimeFields.Year, MONSPERYEAR);
	}

	/* FIXME: handle calendar corrections here */
	for (CurYear = EPOCHYEAR; CurYear < TimeFields.Year; CurYear++)
	{ rcTime += YearLengths[IsLeapYear(CurYear)];
	}
	for (CurMonth = 1; CurMonth < TimeFields.Month; CurMonth++)
	{ rcTime += MonthLengths[IsLeapYear(CurYear)][CurMonth - 1];
	}
	rcTime += TimeFields.Day - 1;
	rcTime *= SECSPERDAY;
	rcTime += TimeFields.Hour * SECSPERHOUR + TimeFields.Minute * SECSPERMIN + TimeFields.Second;
	rcTime *= TICKSPERSEC;
	rcTime += TimeFields.Milliseconds * TICKSPERMSEC;
	*Time = *(LARGE_INTEGER *)&rcTime;

	return TRUE;
}
/************* end of code by Rex Jolliff (rex@lvcablemodem.com) *******************/

/******************************************************************************
 *  RtlSystemTimeToLocalTime 	[NTDLL.@]
 */
VOID WINAPI RtlSystemTimeToLocalTime(
	IN  PLARGE_INTEGER SystemTime,
	OUT PLARGE_INTEGER LocalTime)
{
	FIXME("(%p, %p),stub!\n",SystemTime,LocalTime);

	memcpy (LocalTime, SystemTime, sizeof (PLARGE_INTEGER));
}

/******************************************************************************
 *  RtlTimeToSecondsSince1970		[NTDLL.@]
 */
BOOLEAN WINAPI RtlTimeToSecondsSince1970( const LARGE_INTEGER *time, PULONG res )
{
    ULONGLONG tmp = RtlLargeIntegerDivide( time->QuadPart, 10000000LL, NULL );
    tmp -= SECS_1601_TO_1970;
    if (tmp > 0xffffffff) return FALSE;
    *res = (DWORD)tmp;
    return TRUE;
}

/******************************************************************************
 *  RtlTimeToSecondsSince1980		[NTDLL.@]
 */
BOOLEAN WINAPI RtlTimeToSecondsSince1980( const LARGE_INTEGER *time, LPDWORD res )
{
    ULONGLONG tmp = RtlLargeIntegerDivide( time->QuadPart, 10000000LL, NULL );
    tmp -= SECS_1601_to_1980;
    if (tmp > 0xffffffff) return FALSE;
    *res = (DWORD)tmp;
    return TRUE;
}

/******************************************************************************
 *  RtlSecondsSince1970ToTime		[NTDLL.@]
 */
void WINAPI RtlSecondsSince1970ToTime( DWORD time, LARGE_INTEGER *res )
{
    LONGLONG secs = time + SECS_1601_TO_1970;
    res->QuadPart = RtlExtendedIntegerMultiply( secs, 10000000 );
}

/******************************************************************************
 *  RtlSecondsSince1980ToTime		[NTDLL.@]
 */
void WINAPI RtlSecondsSince1980ToTime( DWORD time, LARGE_INTEGER *res )
{
    LONGLONG secs = time + SECS_1601_to_1980;
    res->QuadPart = RtlExtendedIntegerMultiply( secs, 10000000 );
}

/******************************************************************************
 * RtlTimeToElapsedTimeFields [NTDLL.@]
 * FIXME: prototype guessed
 */
VOID WINAPI RtlTimeToElapsedTimeFields(
	PLARGE_INTEGER liTime,
	PTIME_FIELDS TimeFields)
{
	FIXME("(%p,%p): stub\n",liTime,TimeFields);
}

/***********************************************************************
 *      NtQuerySystemTime   (NTDLL.@)
 *      ZwQuerySystemTime   (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySystemTime( PLARGE_INTEGER time )
{
    LONGLONG secs;
    struct timeval now;

    gettimeofday( &now, 0 );
    secs = now.tv_sec + SECS_1601_TO_1970;
    time->QuadPart = RtlExtendedIntegerMultiply( secs, 10000000 ) + now.tv_usec * 10;
    return STATUS_SUCCESS;
}


static inline void NT_getCurrentTimeAndAdjustEpoch(struct timeval *t, struct timeval *prevActualTime){

    /* retrieve the current system time.  Unfortunately this function is the
       best resolution standard Unix time function that we could find.  It IS
       affected by system clock changes so we will manually have to check for
       epoch changes and adjust accordingly. 

       NOTE: the timer will stay stable if the current system time is adjusted
             backward.  However, since it cannot be reliably detected, it does
             not account for forward time adjustments.  At worst, the client
             app will see an expectedly large time gap that includes the change
             in time.  At least it won't have to deal with a potential 
             wrap-around situation.
    */
    gettimeofday (t, NULL);


    RtlEnterCriticalSection(&TIME_sync_cs);
    
    /* we've moved past the epoch => change the epoch ;) */
    if (t->tv_sec < server_starttime.tv_sec ||
        (t->tv_sec == server_starttime.tv_sec && t->tv_usec < server_starttime.tv_usec)){

        /* adjust the epoch to account for the current tick value and the modified start time */
        server_starttime.tv_sec = t->tv_sec - (prevActualTime->tv_sec - server_starttime.tv_sec);
        server_starttime.tv_usec = t->tv_usec - (prevActualTime->tv_usec - server_starttime.tv_usec);

        /* the microsecond counter wrapped around => correct it */
        if (server_starttime.tv_usec < 0){
            server_starttime.tv_sec--;
            server_starttime.tv_usec = 1000000 + server_starttime.tv_usec;
        }
    }

    /* the clock was adjusted => change the epoch to reflect the change.  This tick count will
       return with the same value as the previous call. */
    else if (t->tv_sec < prevActualTime->tv_sec ||
             (t->tv_sec == prevActualTime->tv_sec && t->tv_usec < prevActualTime->tv_usec)){

        /* adjust the epoch by the time delta */
        server_starttime.tv_sec -= (prevActualTime->tv_sec - t->tv_sec);
        server_starttime.tv_usec -= (prevActualTime->tv_usec - t->tv_usec);

        /* the microsecond counter wrapped around => correct it */
        if (server_starttime.tv_usec < 0){
            server_starttime.tv_sec--;
            server_starttime.tv_usec = 1000000 + server_starttime.tv_usec;
        }
    }
    
    /* update the previous time values */
    prevActualTime->tv_sec = t->tv_sec;
    prevActualTime->tv_usec = t->tv_usec;    

    RtlLeaveCriticalSection(&TIME_sync_cs);    
}

/******************************************************************************
 *      NtQueryPerformanceCounter  (NTDLL.@)
 *      ZwQueryPerformanceCounter  (NTDLL.@)
 *
 *  Windows seems to use a timer clocked at a multiple of 1193182 Hz. To keep
 *  it simple, our counter will just use that frequency.
 *
 *  References:
 *    http://support.microsoft.com/kb/q172338/
 *    http://www.gamedev.net/community/forums/topic.asp?topic_id=55545&whichpage=1&#270640
 */
NTSTATUS WINAPI NtQueryPerformanceCounter (PLARGE_INTEGER Count,
                                           PLARGE_INTEGER Freq)
{
   static struct timeval    prevActualTime = {0};
   struct timeval           CurTime;
   long unsigned int        Diff;
   

   if (!Count)
      return STATUS_ACCESS_VIOLATION;

   if (Freq)
      Freq->QuadPart = QP_FREQ;

   /* get the current time and adjust the epoch if necessary */
   NT_getCurrentTimeAndAdjustEpoch(&CurTime, &prevActualTime);

   /* Convert counter of 1 MHz (1 usec) to our frequency */

   /* Check if difference is < QP_FREQ. If so, do multiplication early so
      that we don't lose precision in the shift; otherwise, wait until
      after the scaling so that we don't overflow */
   Diff = CurTime.tv_sec - server_starttime.tv_sec;
   if (Diff < QP_FREQ)
      Count->QuadPart =
         ((long long unsigned int)Diff * QP_SCALE * 1000000) >> QP_DIVSHIFT;
   else
      Count->QuadPart =
         (((long long unsigned int)Diff * QP_SCALE) >> QP_DIVSHIFT) * 1000000;

   /* Need to handle negative numbers here */
   if (CurTime.tv_usec < server_starttime.tv_usec)
      Count->QuadPart -=
         (((long long unsigned int)server_starttime.tv_usec -
           CurTime.tv_usec) * QP_SCALE) >> QP_DIVSHIFT;
   else
      Count->QuadPart +=
         (((long long unsigned int)CurTime.tv_usec -
           server_starttime.tv_usec) * QP_SCALE) >> QP_DIVSHIFT;

   return STATUS_SUCCESS;
}


/******************************************************************************
 *      NtGetTickCount   (NTDLL.@)
 *      ZwGetTickCount   (NTDLL.@)
 *
 *  Return a non-decreasing milliseconds-since-boot counter.  The timer should
 *  stay stable despite system clock changes (by user or otherwise).
 */
ULONG WINAPI NtGetTickCount ()
{
    static struct timeval   prevActualTime = {0};
    struct timeval          t;


    /* get the current time and adjust the epoch if necessary */
    NT_getCurrentTimeAndAdjustEpoch(&t, &prevActualTime);

    /* calculate the current time based on time of day and the current epoch value */
    return (t.tv_sec - server_starttime.tv_sec) * 1000 +
           (t.tv_usec - server_starttime.tv_usec) / 1000;
}

ULONGLONG WINAPI NtGetTickCount64()
{
    static struct timeval   prevActualTime = {0};
    struct timeval          t;


    /* get the current time and adjust the epoch if necessary */
    NT_getCurrentTimeAndAdjustEpoch(&t, &prevActualTime);

    /* calculate the current time based on time of day and the current epoch value */
    return (t.tv_sec - server_starttime.tv_sec) * 1000ull +
           (t.tv_usec - server_starttime.tv_usec) / 1000ull;
}
