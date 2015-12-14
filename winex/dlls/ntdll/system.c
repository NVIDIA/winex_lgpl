/*
 * SYSTEM DLL routines
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "windef.h"
#include "wingdi.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "services.h"
#include "stackframe.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(system);

typedef struct
{
    SYSTEMTIMERPROC callback;  /* NULL if not in use */
    FARPROC16       callback16;
    INT             rate;
    INT             ticks;
} SYSTEM_TIMER;

#define NB_SYS_TIMERS   8
#define SYS_TIMER_RATE  54925

static SYSTEM_TIMER SYS_Timers[NB_SYS_TIMERS];
static int SYS_NbTimers = 0;
static HANDLE SYS_Service = INVALID_HANDLE_VALUE;


/***********************************************************************
 *           SYSTEM_TimerTick
 */
static void CALLBACK SYSTEM_TimerTick( ULONG_PTR arg )
{
    int i;

    for (i = 0; i < NB_SYS_TIMERS; i++)
    {
        if (!SYS_Timers[i].callback) continue;
        if ((SYS_Timers[i].ticks -= SYS_TIMER_RATE) <= 0)
        {
            SYS_Timers[i].ticks += SYS_Timers[i].rate;
            SYS_Timers[i].callback( i+1 );
        }
    }
}

/**********************************************************************
 *           SYSTEM_StartTicks
 *
 * Start the system tick timer.
 */
static void SYSTEM_StartTicks(void)
{
    if ( SYS_Service == INVALID_HANDLE_VALUE )
        SYS_Service = SERVICE_AddTimer( (SYS_TIMER_RATE+500)/1000, SYSTEM_TimerTick, 0L );
}


/**********************************************************************
 *           SYSTEM_StopTicks
 *
 * Stop the system tick timer.
 */
static void SYSTEM_StopTicks(void)
{
    if ( SYS_Service != INVALID_HANDLE_VALUE )
    {
        SERVICE_Delete( SYS_Service );
        SYS_Service = INVALID_HANDLE_VALUE;
    }
}


/***********************************************************************
 *           InquireSystem   (SYSTEM.1)
 *
 * Note: the function always takes 2 WORD arguments, contrary to what
 *       "Undocumented Windows" says.
  */
DWORD WINAPI InquireSystem16( WORD code, WORD arg )
{
    WORD drivetype;

    switch(code)
    {
    case 0:  /* Get timer resolution */
        return SYS_TIMER_RATE;

    case 1:  /* Get drive type */
        drivetype = GetDriveType16( arg );
        return MAKELONG( drivetype, drivetype );

    case 2:  /* Enable one-drive logic */
        FIXME("Case %d: set single-drive %d not supported\n", code, arg );
        return 0;
    }
    WARN("Unknown code %d\n", code );
    return 0;
}


/***********************************************************************
 *           CreateSystemTimer   (SYSTEM.2)
 */
WORD WINAPI CreateSystemTimer( WORD rate, SYSTEMTIMERPROC callback )
{
    int i;
    for (i = 0; i < NB_SYS_TIMERS; i++)
        if (!SYS_Timers[i].callback)  /* Found one */
        {
            SYS_Timers[i].rate = (UINT)rate * 1000;
            if (SYS_Timers[i].rate < SYS_TIMER_RATE)
                SYS_Timers[i].rate = SYS_TIMER_RATE;
            SYS_Timers[i].ticks = SYS_Timers[i].rate;
            SYS_Timers[i].callback = callback;
            if (++SYS_NbTimers == 1) SYSTEM_StartTicks();
            return i + 1;  /* 0 means error */
        }
    return 0;
}

/**********************************************************************/

static void call_timer_proc16( WORD timer )
{
    CONTEXT86 context;
    FARPROC16 proc = SYS_Timers[timer-1].callback16;

    memset( &context, '\0', sizeof(context) );

    context.SegCs = SELECTOROF( proc );
    context.Eip   = OFFSETOF( proc );
    context.Ebp   = OFFSETOF( NtCurrentTeb()->WOW32Reserved )
                          + (WORD)&((STACK16FRAME*)0)->bp;

    AX_reg( &context ) = timer;

    wine_call_to_16_regs_short( &context, 0 );
}

/**********************************************************************/

WORD WINAPI WIN16_CreateSystemTimer( WORD rate, FARPROC16 proc )
{
    WORD ret = CreateSystemTimer( rate, call_timer_proc16 );
    if (ret) SYS_Timers[ret - 1].callback16 = proc;
    return ret;
}


/***********************************************************************
 *           KillSystemTimer   (SYSTEM.3)
 *
 * Note: do not confuse this function with USER.182
 */
WORD WINAPI SYSTEM_KillSystemTimer( WORD timer )
{
    if ( !timer || timer > NB_SYS_TIMERS || !SYS_Timers[timer-1].callback )
        return timer;  /* Error */
    SYS_Timers[timer-1].callback = NULL;
    if (!--SYS_NbTimers) SYSTEM_StopTicks();
    return 0;
}


/***********************************************************************
 *           EnableSystemTimers   (SYSTEM.4)
 */
void WINAPI EnableSystemTimers16(void)
{
    if ( SYS_Service != INVALID_HANDLE_VALUE )
        SERVICE_Enable( SYS_Service );
}


/***********************************************************************
 *           DisableSystemTimers   (SYSTEM.5)
 */
void WINAPI DisableSystemTimers16(void)
{
    if ( SYS_Service != INVALID_HANDLE_VALUE )
        SERVICE_Disable( SYS_Service );
}


/***********************************************************************
 *           Get80x87SaveSize   (SYSTEM.7)
 */
WORD WINAPI Get80x87SaveSize16(void)
{
    return 94;
}


/***********************************************************************
 *           Save80x87State   (SYSTEM.8)
 */
void WINAPI Save80x87State16( char *ptr )
{
#ifdef __i386__
    __asm__(".byte 0x66; fsave %0; fwait" : "=m" (ptr) );
#endif
}


/***********************************************************************
 *           Restore80x87State   (SYSTEM.9)
 */
void WINAPI Restore80x87State16( const char *ptr )
{
#ifdef __i386__
    __asm__(".byte 0x66; frstor %0" : : "m" (ptr) );
#endif
}

