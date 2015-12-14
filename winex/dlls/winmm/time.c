/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 * MMSYSTEM time functions
 *
 * Copyright 1993 Martin Ayotte
 */

#include "config.h"
#include "wine/port.h"

#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <unistd.h>

#include "mmsystem.h"
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/mmsystem16.h"
#include "winemm.h"

#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmtime);

/*
 * FIXME
 * We're using "1" as the mininum resolution to the timer,
 * as Windows 95 does, according to the docs. Maybe it should
 * depend on the computers resources!
 */
#define MMSYSTIME_MININTERVAL (1)
#define MMSYSTIME_MAXINTERVAL (65535)

/* ### start build ### */
extern WORD CALLBACK TIME_CallTo16_word_wwlll(FARPROC16,WORD,WORD,LONG,LONG,LONG);
/* ### stop build ### */

static	void	TIME_TriggerCallBack(LPWINE_TIMERENTRY lpTimer)
{
    TRACE("before CallBack => lpFunc=%p wTimerID=%04X dwUser=%08lX !\n",
	  lpTimer->lpFunc, lpTimer->wTimerID, lpTimer->dwUser);

    /* - TimeProc callback that is called here is something strange, under Windows 3.1x it is called
     * 		during interrupt time,  is allowed to execute very limited number of API calls (like
     *	    	PostMessage), and must reside in DLL (therefore uses stack of active application). So I
     *       guess current implementation via SetTimer has to be improved upon.
     */
    switch (lpTimer->wFlags & 0x30) {
    case TIME_CALLBACK_FUNCTION:
	if (lpTimer->wFlags & WINE_TIMER_IS32)
	    ((LPTIMECALLBACK)lpTimer->lpFunc)(lpTimer->wTimerID, 0, lpTimer->dwUser, 0, 0);
	else
	    TIME_CallTo16_word_wwlll(lpTimer->lpFunc, lpTimer->wTimerID, 0,
				     lpTimer->dwUser, 0, 0);
	break;
    case TIME_CALLBACK_EVENT_SET:
	SetEvent((HANDLE)lpTimer->lpFunc);
	break;
    case TIME_CALLBACK_EVENT_PULSE:
	PulseEvent((HANDLE)lpTimer->lpFunc);
	break;
    default:
	FIXME("Unknown callback type 0x%04x for mmtime callback (%p), ignored.\n",
	      lpTimer->wFlags, lpTimer->lpFunc);
	break;
    }
    TRACE("after CallBack !\n");
}

/**************************************************************************
 *           TIME_MMSysTimeCallback
 */
static DWORD CALLBACK TIME_MMSysTimeCallback(LPWINE_MM_IDATA iData)
{
    LPWINE_TIMERENTRY 	lpTimer, lpNextTimer;
    DWORD               StartCount, Delta;
    int			idx;
    DWORD               TimeToSleep = INFINITE;
    LONG                TimeUsed;

    if (!iData->lpTimerList)
        return TimeToSleep;

    /* since timeSetEvent() and timeKillEvent() can be called
     * from 16 bit code, there are cases where win16 lock is
     * locked upon entering timeSetEvent(), and then the mm timer
     * critical section is locked. This function cannot call the
     * timer callback with the crit sect locked (because callback
     * may need to acquire Win16 lock, thus providing a deadlock
     * situation).
     * To cope with that, we just copy the WINE_TIMERENTRY struct
     * that need to trigger the callback, and call it without the
     * mm timer crit sect locked. The bad side of this
     * implementation is that, in some cases, the callback may be
     * invoked *after* a timer has been destroyed...
     * EPP 99/07/13
     */
    idx = 0;
    StartCount = GetTickCount ();

    EnterCriticalSection (&iData->cs);
    for (lpTimer = iData->lpTimerList; lpTimer != NULL; ) {
        lpNextTimer = lpTimer->lpNext;

        if (StartCount >= lpTimer->uCurTime) {
            lpTimer->uCurTime += lpTimer->wDelay;

            if (lpTimer->lpFunc) {
                if (idx == iData->nSizeLpTimers) {
                    iData->lpTimers = (LPWINE_TIMERENTRY)
                        HeapReAlloc (GetProcessHeap (), 0,
                                     iData->lpTimers,
                                     ++iData->nSizeLpTimers *
                                     sizeof (WINE_TIMERENTRY));
                }
                iData->lpTimers[idx++] = *lpTimer;
            }

            /* TIME_ONESHOT is defined as 0 */
            if (!(lpTimer->wFlags & TIME_PERIODIC))
            {
                timeKillEvent (lpTimer->wTimerID);
                Delta = INFINITE;
            }
            else
            {
                if (lpTimer->uCurTime <= StartCount)
                    Delta = 0;
                else
                    Delta = lpTimer->uCurTime - StartCount;
            }
        }
        else
            Delta = lpTimer->uCurTime - StartCount;

        if (Delta < TimeToSleep)
            TimeToSleep = Delta;

        lpTimer = lpNextTimer;
    }
    LeaveCriticalSection (&iData->cs);

    while (idx > 0) {
        TIME_TriggerCallBack (&iData->lpTimers[--idx]);
    }

    /* Account for time already spent to avoid too much drift */
    TimeUsed = GetTickCount () - StartCount;
    if (TimeUsed >= TimeToSleep)
        TimeToSleep = 0;
    else if (TimeToSleep != INFINITE)
        TimeToSleep -= TimeUsed;

    return TimeToSleep;
}

/**************************************************************************
 *           TIME_MMSysTimeThread
 */
static DWORD CALLBACK TIME_MMSysTimeThread(LPVOID arg)
{
    LPWINE_MM_IDATA iData = (LPWINE_MM_IDATA)arg;
    volatile HANDLE *pActive = (volatile HANDLE *)&iData->hMMTimer;

    TRACE ("pid=%d tid=%d\n", getpid(), wine_get_inprocess_tid());

    while (*pActive) {
        DWORD TimeToSleep = TIME_MMSysTimeCallback (iData);

        TRACE ("Sleeping for %lu ms\n", TimeToSleep);

        /* If we're behind, let's get caught up now */
        if (TimeToSleep == 0)
            continue;

        WaitForSingleObject (iData->hMMTimerWakeEv, TimeToSleep);
    }
    return 0;
}

/**************************************************************************
 * 				TIME_MMTimeStart
 */
LPWINE_MM_IDATA	TIME_MMTimeStart(void)
{
    LPWINE_MM_IDATA	iData = MULTIMEDIA_GetIData();

    if (IsBadWritePtr(iData, sizeof(WINE_MM_IDATA))) {
	ERR("iData is not correctly set, please report. Expect failure.\n");
	return 0;
    }
    /* one could think it's possible to stop the service thread activity when no more
     * mm timers are active, but this would require to keep mmSysTimeMS up-to-date
     * without being incremented within the service thread callback.
     */
    if (!iData->hMMTimer) {
	iData->lpTimerList = NULL;
	iData->hMMTimerWakeEv = CreateEventA (NULL, FALSE, FALSE, NULL);
	iData->hMMTimer = CreateThread(NULL, 0, TIME_MMSysTimeThread, iData, CREATE_SUSPENDED, NULL);
	SERVER_START_REQ( set_scheduling_mode )
	{
	    req->handle = iData->hMMTimer;
	    req->mode = SCDM_INTERNAL;
	    wine_server_call( req );
	}
	SERVER_END_REQ;
	SetThreadPriority(iData->hMMTimer, THREAD_PRIORITY_TIME_CRITICAL);
	ResumeThread(iData->hMMTimer);
    }

    return iData;
}

/**************************************************************************
 * 				TIME_MMTimeStop
 */
void	TIME_MMTimeStop(void)
{
    LPWINE_MM_IDATA	iData = MULTIMEDIA_GetIData();

    if (IsBadWritePtr(iData, sizeof(WINE_MM_IDATA))) {
	ERR("iData is not correctly set, please report. Expect failure.\n");
	return;
    }
    if (iData->hMMTimer) {
	HANDLE hMMTimer = iData->hMMTimer;
	iData->hMMTimer = 0;
	SetEvent(iData->hMMTimerWakeEv);
	WaitForSingleObject(hMMTimer, INFINITE);
	CloseHandle(iData->hMMTimerWakeEv);
	CloseHandle(hMMTimer);
    }
}

/**************************************************************************
 * 				timeGetSystemTime	[WINMM.@]
 */
MMRESULT WINAPI timeGetSystemTime(LPMMTIME lpTime, UINT wSize)
{
    TRACE("(%p, %u);\n", lpTime, wSize);

    if (wSize >= sizeof(*lpTime)) {
	lpTime->wType = TIME_MS;
	lpTime->u.ms = GetTickCount ();

	TRACE("=> %lu\n", lpTime->u.ms);
    }

    return 0;
}

/**************************************************************************
 * 				timeGetSystemTime	[MMSYSTEM.601]
 */
MMRESULT16 WINAPI timeGetSystemTime16(LPMMTIME16 lpTime, UINT16 wSize)
{
    TRACE("(%p, %u);\n", lpTime, wSize);

    if (wSize >= sizeof(*lpTime)) {
	lpTime->wType = TIME_MS;

        /* Implementing timers properly is going to take a
         * fair bit of work.  For now, we'll pretend that we're
         * Windows 95/98/ME, and pretend that we have millisecond
         * timing.  This also means that we don't have to properly
         * implement timeBeginPeriod/timeEndPeriod.  Note however
         * that latency on timer callbacks is significantly lower
         * than this due to process switching overhead time */
	
	/* When this is all implemented with an appropriate low-latency
	   kernel timer, we should eventually return the time as:
		lpTime->u.ms = TIME_MMTimeStart()->mmSysTimeMS; */
	lpTime->u.ms = GetTickCount();


	TRACE("=> %lu\n", lpTime->u.ms);
    }

    return 0;
}

/**************************************************************************
 * 				timeSetEventInternal	[internal]
 */
static	WORD	timeSetEventInternal(UINT wDelay, UINT wResol,
				     FARPROC16 lpFunc, DWORD dwUser, UINT wFlags)
{
    WORD 		wNewID = 0;
    LPWINE_TIMERENTRY	lpNewTimer;
    LPWINE_TIMERENTRY	lpTimer;
    LPWINE_MM_IDATA	iData;

    TRACE("(%u, %u, %p, %08lX, %04X);\n", wDelay, wResol, lpFunc, dwUser, wFlags);

    lpNewTimer = (LPWINE_TIMERENTRY)HeapAlloc(GetProcessHeap(), 0, sizeof(WINE_TIMERENTRY));
    if (lpNewTimer == NULL)
	return 0;

    if (wDelay < MMSYSTIME_MININTERVAL || wDelay > MMSYSTIME_MAXINTERVAL)
	return 0;

    iData = TIME_MMTimeStart();

    lpNewTimer->uCurTime = GetTickCount () + wDelay;
    lpNewTimer->wDelay = wDelay;
    lpNewTimer->wResol = wResol;
    lpNewTimer->lpFunc = lpFunc;
    lpNewTimer->dwUser = dwUser;
    lpNewTimer->wFlags = wFlags;

    EnterCriticalSection(&iData->cs);

    for (lpTimer = iData->lpTimerList; lpTimer != NULL; lpTimer = lpTimer->lpNext) {
	wNewID = max(wNewID, lpTimer->wTimerID);
    }

    lpNewTimer->lpNext = iData->lpTimerList;
    iData->lpTimerList = lpNewTimer;
    lpNewTimer->wTimerID = wNewID + 1;

    LeaveCriticalSection(&iData->cs);

    SetEvent (iData->hMMTimerWakeEv);
    TRACE("=> %u\n", wNewID + 1);

    return wNewID + 1;
}

/**************************************************************************
 * 				timeSetEvent		[WINMM.@]
 */
MMRESULT WINAPI timeSetEvent(UINT wDelay, UINT wResol, LPTIMECALLBACK lpFunc,
			     DWORD dwUser, UINT wFlags)
{
    if (wFlags & WINE_TIMER_IS32)
	WARN("Unknown windows flag... wine internally used.. ooch\n");

    return timeSetEventInternal(wDelay, wResol, (FARPROC16)lpFunc,
				dwUser, wFlags|WINE_TIMER_IS32);
}

/**************************************************************************
 * 				timeSetEvent		[MMSYSTEM.602]
 */
MMRESULT16 WINAPI timeSetEvent16(UINT16 wDelay, UINT16 wResol, LPTIMECALLBACK16 lpFunc,
				 DWORD dwUser, UINT16 wFlags)
{
    if (wFlags & WINE_TIMER_IS32)
	WARN("Unknown windows flag... wine internally used.. ooch\n");

    return timeSetEventInternal(wDelay, wResol, (FARPROC16)lpFunc,
				dwUser, wFlags & ~WINE_TIMER_IS32);
}

/**************************************************************************
 * 				timeKillEvent		[WINMM.@]
 */
MMRESULT WINAPI timeKillEvent(UINT wID)
{
    LPWINE_TIMERENTRY*	lpTimer;
    LPWINE_MM_IDATA	iData = MULTIMEDIA_GetIData();
    MMRESULT		ret = MMSYSERR_INVALPARAM;

    TRACE("(%u)\n", wID);
    EnterCriticalSection(&iData->cs);
    /* remove WINE_TIMERENTRY from list */
    for (lpTimer = &iData->lpTimerList; *lpTimer; lpTimer = &(*lpTimer)->lpNext) {
	if (wID == (*lpTimer)->wTimerID) {
	    break;
	}
    }
    LeaveCriticalSection(&iData->cs);

    if (*lpTimer) {
	LPWINE_TIMERENTRY	lpTemp = *lpTimer;

	/* unlink timer of id 'wID' */
	*lpTimer = (*lpTimer)->lpNext;
	HeapFree(GetProcessHeap(), 0, lpTemp);
	ret = TIMERR_NOERROR;
    } else {
	WARN("wID=%u is not a valid timer ID\n", wID);
    }

    return ret;
}

/**************************************************************************
 * 				timeKillEvent		[MMSYSTEM.603]
 */
MMRESULT16 WINAPI timeKillEvent16(UINT16 wID)
{
    return timeKillEvent(wID);
}

/**************************************************************************
 * 				timeGetDevCaps		[WINMM.@]
 */
MMRESULT WINAPI timeGetDevCaps(LPTIMECAPS lpCaps, UINT wSize)
{
    TRACE("(%p, %u) !\n", lpCaps, wSize);

    lpCaps->wPeriodMin = MMSYSTIME_MININTERVAL;
    lpCaps->wPeriodMax = MMSYSTIME_MAXINTERVAL;
    return 0;
}

/**************************************************************************
 * 				timeGetDevCaps		[MMSYSTEM.604]
 */
MMRESULT16 WINAPI timeGetDevCaps16(LPTIMECAPS16 lpCaps, UINT16 wSize)
{
    TRACE("(%p, %u) !\n", lpCaps, wSize);

    lpCaps->wPeriodMin = MMSYSTIME_MININTERVAL;
    lpCaps->wPeriodMax = MMSYSTIME_MAXINTERVAL;
    return 0;
}

/**************************************************************************
 * 				timeBeginPeriod		[WINMM.@]
 */
MMRESULT WINAPI timeBeginPeriod(UINT wPeriod)
{
    TRACE("(%u) !\n", wPeriod);

    if (wPeriod < MMSYSTIME_MININTERVAL || wPeriod > MMSYSTIME_MAXINTERVAL)
	return TIMERR_NOCANDO;
    return 0;
}

/**************************************************************************
 * 				timeBeginPeriod	[MMSYSTEM.605]
 */
MMRESULT16 WINAPI timeBeginPeriod16(UINT16 wPeriod)
{
    TRACE("(%u) !\n", wPeriod);

    /* No point in checking for max, as it's the max for UINT16 */
    if (wPeriod < MMSYSTIME_MININTERVAL)
	return TIMERR_NOCANDO;
    return 0;
}

/**************************************************************************
 * 				timeEndPeriod		[WINMM.@]
 */
MMRESULT WINAPI timeEndPeriod(UINT wPeriod)
{
    TRACE("(%u) !\n", wPeriod);

    if (wPeriod < MMSYSTIME_MININTERVAL || wPeriod > MMSYSTIME_MAXINTERVAL)
	return TIMERR_NOCANDO;
    return 0;
}

/**************************************************************************
 * 				timeEndPeriod		[MMSYSTEM.606]
 */
MMRESULT16 WINAPI timeEndPeriod16(UINT16 wPeriod)
{
    TRACE("(%u) !\n", wPeriod);

    /* No point in checking for max, as it's the max for UINT16 */
    if (wPeriod < MMSYSTIME_MININTERVAL)
	return TIMERR_NOCANDO;
    return 0;
}

/**************************************************************************
 * 				timeGetTime    [MMSYSTEM.607]
 * 				timeGetTime    [WINMM.@]
 */
DWORD WINAPI timeGetTime(void)
{
	/* Implementing timers properly is going to take a 
	 * fair bit of work.  For now, we'll pretend that we're
	 * Windows 95/98/ME, and pretend that we have millisecond
	 * timing.  This also means that we don't have to properly
	 * implement timeBeginPeriod/timeEndPeriod.  Note however
	 * that latency on timer callbacks is significantly lower
	 * than this due to process switching overhead time */
	return GetTickCount();

	/* When timers are better implemented, this function
	   should probably look more like this:
   		return TIME_MMTimeStart()->mmSysTimeMS;
	   Note that previous revs of timeGetTime released
	   and restored the win16 lock as a hack.  Is this
	   really necessay?  It's not clear... */
}
