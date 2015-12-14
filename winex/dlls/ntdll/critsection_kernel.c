/*
 * Win32 critical sections
 *
 * Copyright 1998 Alexandre Julliard
 * Copyright (c) 2009-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "winerror.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/port.h"
#include "wine/debug.h"
#include "thread.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32);
WINE_DECLARE_DEBUG_CHANNEL(relay);

extern void free_debug_info( PRTL_CRITICAL_SECTION crit );

/***********************************************************************
 *           InitializeCriticalSection   (KERNEL32.@)
 */
void WINAPI InitializeCriticalSection( CRITICAL_SECTION *crit )
{
    NTSTATUS ret = RtlInitializeCriticalSection( crit );
    if (ret) RtlRaiseStatus( ret );
}

/***********************************************************************
 *           InitializeCriticalSectionAndSpinCount   (KERNEL32.@)
 */
BOOL WINAPI InitializeCriticalSectionAndSpinCount( CRITICAL_SECTION *crit, DWORD spincount )
{
    NTSTATUS ret = RtlInitializeCriticalSectionAndSpinCount( crit, spincount );
    if (ret) RtlRaiseStatus( ret );
    return !ret;
}

/***********************************************************************
 *           MakeCriticalSectionGlobal   (KERNEL32.@)
 */
void WINAPI MakeCriticalSectionGlobal( CRITICAL_SECTION *crit )
{
    /* let's assume that only one thread at a time will try to do this */
    HANDLE sem = crit->LockSemaphore;
    if (!sem) NtCreateSemaphore( &sem, SEMAPHORE_ALL_ACCESS, NULL, 0, 1 );
    crit->LockSemaphore = ConvertToGlobalHandle( sem );

    free_debug_info( crit );
}


/***********************************************************************
 *           ReinitializeCriticalSection   (KERNEL32.@)
 */
void WINAPI ReinitializeCriticalSection( CRITICAL_SECTION *crit )
{
    if ( !crit->LockSemaphore )
        RtlInitializeCriticalSection( crit );
}


/***********************************************************************
 *           UninitializeCriticalSection   (KERNEL32.@)
 */
void WINAPI UninitializeCriticalSection( CRITICAL_SECTION *crit )
{
    RtlDeleteCriticalSection( crit );
}

#ifdef __i386__

/***********************************************************************
 *		InterlockedCompareExchange (KERNEL32.@)
 */
/* LONG WINAPI InterlockedCompareExchange( PLONG dest, LONG xchg, LONG compare ); */
__ASM_GLOBAL_FUNC(InterlockedCompareExchange,
                  "movl 12(%esp),%eax\n\t"
                  "movl 8(%esp),%ecx\n\t"
                  "movl 4(%esp),%edx\n\t"
                  "lock; cmpxchgl %ecx,(%edx)\n\t"
                  "ret $12");

/***********************************************************************
 *		InterlockedExchange (KERNEL32.@)
 */
/* LONG WINAPI InterlockedExchange( PLONG dest, LONG val ); */
__ASM_GLOBAL_FUNC(InterlockedExchange,
                  "movl 8(%esp),%eax\n\t"
                  "movl 4(%esp),%edx\n\t"
                  "lock; xchgl %eax,(%edx)\n\t"
                  "ret $8");

/***********************************************************************
 *		InterlockedExchangeAdd (KERNEL32.@)
 */
/* LONG WINAPI InterlockedExchangeAdd( PLONG dest, LONG incr ); */
__ASM_GLOBAL_FUNC(InterlockedExchangeAdd,
                  "movl 8(%esp),%eax\n\t"
                  "movl 4(%esp),%edx\n\t"
                  "lock; xaddl %eax,(%edx)\n\t"
                  "ret $8");

/***********************************************************************
 *		InterlockedIncrement (KERNEL32.@)
 */
/* LONG WINAPI InterlockedIncrement( PLONG dest ); */
__ASM_GLOBAL_FUNC(InterlockedIncrement,
                  "movl 4(%esp),%edx\n\t"
                  "movl $1,%eax\n\t"
                  "lock; xaddl %eax,(%edx)\n\t"
                  "incl %eax\n\t"
                  "ret $4");

/***********************************************************************
 *		InterlockedDecrement (KERNEL32.@)
 */
__ASM_GLOBAL_FUNC(InterlockedDecrement,
                  "movl 4(%esp),%edx\n\t"
                  "movl $-1,%eax\n\t"
                  "lock; xaddl %eax,(%edx)\n\t"
                  "decl %eax\n\t"
                  "ret $4");

#elif defined(__sparc__) && defined(__sun__)

/*
 * As the earlier Sparc processors lack necessary atomic instructions,
 * I'm simply falling back to the library-provided _lwp_mutex routines
 * to ensure mutual exclusion in a way appropriate for the current
 * architecture.
 *
 * FIXME:  If we have the compare-and-swap instruction (Sparc v9 and above)
 *         we could use this to speed up the Interlocked operations ...
 */

#include <synch.h>
static lwp_mutex_t interlocked_mutex = DEFAULTMUTEX;

/***********************************************************************
 *		InterlockedCompareExchange (KERNEL32.@)
 */
LONG WINAPI InterlockedCompareExchange( PLONG dest, LONG xchg, LONG compare )
{
    _lwp_mutex_lock( &interlocked_mutex );

    if ( *dest == compare )
        *dest = xchg;
    else
        compare = *dest;

    _lwp_mutex_unlock( &interlocked_mutex );
    return compare;
}

/***********************************************************************
 *		InterlockedExchange (KERNEL32.@)
 */
LONG WINAPI InterlockedExchange( PLONG dest, LONG val )
{
    LONG retv;
    _lwp_mutex_lock( &interlocked_mutex );

    retv = *dest;
    *dest = val;

    _lwp_mutex_unlock( &interlocked_mutex );
    return retv;
}

/***********************************************************************
 *		InterlockedExchangeAdd (KERNEL32.@)
 */
LONG WINAPI InterlockedExchangeAdd( PLONG dest, LONG incr )
{
    LONG retv;
    _lwp_mutex_lock( &interlocked_mutex );

    retv = *dest;
    *dest += incr;

    _lwp_mutex_unlock( &interlocked_mutex );
    return retv;
}

/***********************************************************************
 *		InterlockedIncrement (KERNEL32.@)
 */
LONG WINAPI InterlockedIncrement( PLONG dest )
{
    LONG retv;
    _lwp_mutex_lock( &interlocked_mutex );

    retv = ++*dest;

    _lwp_mutex_unlock( &interlocked_mutex );
    return retv;
}

/***********************************************************************
 *		InterlockedDecrement (KERNEL32.@)
 */
LONG WINAPI InterlockedDecrement( PLONG dest )
{
    LONG retv;
    _lwp_mutex_lock( &interlocked_mutex );

    retv = --*dest;

    _lwp_mutex_unlock( &interlocked_mutex );
    return retv;
}

#elif defined(__ppc__) && defined (__APPLE__)
/*The following 2 declaration are for Assembly versions of these
 * functions in powerpc_atomics.s*/
extern long  PowerPC_AddAtomic(signed long amount, signed long *address);
extern void  PowerPC_CmpExchangeAtomic(long *dest, long xchg, long compare, long* returnval);

static PVOID interlocked_cmpxchg( PVOID *dest, PVOID xchg, PVOID compare )
{
	       long retval = 0;
	              PowerPC_CmpExchangeAtomic((long)dest, (long)xchg, (long)compare, &retval);
		             return (PVOID)(retval);
}

static LONG interlocked_inc( PLONG dest )
{
	    LONG retval = 0;
	        retval = PowerPC_AddAtomic(1, dest);

		    return (retval + 1);
}

static LONG interlocked_dec( PLONG dest )
{
	    LONG retval = 0;
	        retval = PowerPC_AddAtomic(-1, dest);
		    return (retval - 1);
}

#else
#error "You must implement the Interlocked* functions for your CPU"
#endif
