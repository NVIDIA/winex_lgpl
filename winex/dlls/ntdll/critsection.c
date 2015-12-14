/*
 * Win32 critical sections
 *
 * Copyright 1998 Alexandre Julliard
 * Copyright 2004 TransGaming Technologies
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "winerror.h"
#include "winternl.h"
#include "wine/port.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);
WINE_DECLARE_DEBUG_CHANNEL(relay);

/* See http://msdn.microsoft.com/msdnmag/issues/03/12/CriticalSections/ for NT crit section
 * information */

/* Disabled as they aren't useful as implemented (would be more useful if done
   on locking a CS, although expensive), and they cause crashes on Apple */
/* #define CS_BACKTRACES */

/* PH: FIXME: Should autodetect */
const BOOL multiprocessor = FALSE;

extern WORD CreateAndStoreBacktrace(void);
extern const char* RetrieveBacktrace( WORD dwIndex );
extern void ReleaseBacktrace( WORD dwIndex );

#define NUMBER_OF_DEBUG_SECTIONS 32

static DWORD dwAllocatedDebugSections = 0;
static RTL_CRITICAL_SECTION_DEBUG preallocated_debug_area[ NUMBER_OF_DEBUG_SECTIONS ];
LIST_ENTRY csList;
static CRITICAL_SECTION cs_cs;

/* This function must be called before any critical sections are used */
void INIT_CritSects(void)
{
    csList.Flink = &csList;
    csList.Blink = &csList;

    RtlInitializeCriticalSectionAndSpinCount( &cs_cs, 400 );
    CRITICAL_SECTION_NAME( &cs_cs, "The CriticalSection CriticalSection" );
}

static void dump_critsec( const PRTL_CRITICAL_SECTION crit, char* buf, int len )
{
  snprintf( buf, len, "crit (%p): lc %ld rc %ld ot %d sem %d spin %ld",
                         crit, crit->LockCount, crit->RecursionCount,
                         crit->OwningThread, crit->LockSemaphore, (LONG)crit->SpinCount ); 
  buf[ len-1 ] = '\0';
}

/* Debug routine */
void walk_critical_sections(void)
{
  LIST_ENTRY* lpList = &csList;

  fprintf( stderr, "csList(%p) = {Flink=%p, Blink=%p}\n", &csList, csList.Flink, csList.Blink );

#ifdef CS_BACKTRACES
  for( lpList = csList.Flink; lpList != &csList;  lpList = lpList->Flink )
  {
    
    PRTL_CRITICAL_SECTION_DEBUG debug = 
     (PRTL_CRITICAL_SECTION_DEBUG)( (BYTE*)lpList - offsetof(RTL_CRITICAL_SECTION_DEBUG,ProcessLocksList));
    const char* backtrace = RetrieveBacktrace( debug->CreatorBackTraceIndex );
    const char* name;
    char buf[256];

    if( debug->CriticalSection->DebugInfo != debug )
    {
       fprintf( stderr, "Corrupt DebugInfo @ %p\n", debug );
       break;
    }

    if( debug->Spare[1] && !IsBadStringPtrA( (const char*)debug->Spare[1], 80 ) )
       name = (const char*)debug->Spare[1];
    else
       name = "?";

    dump_critsec( debug->CriticalSection, buf, sizeof( buf ) );

    fprintf( stderr, "\tFlink(%p) -> {Flink=%p, Blink=%p}\n", lpList, lpList->Flink, lpList->Blink );
    fprintf( stderr, "\t%p: crit=%p %s %s\n", debug, debug->CriticalSection, name, backtrace );
    fprintf( stderr, "\t%s\n", buf );
  }
#endif /* CS_BACKTRACES */

  fprintf( stderr, "\tDone\n" );
}



/* Define the atomic exchange/inc/dec functions.
 * These are available in kernel32.dll already,
 * but we don't want to import kernel32 from ntdll.
 */

#ifdef __i386__

# ifdef __GNUC__

inline static PVOID interlocked_cmpxchg( PVOID *dest, PVOID xchg, PVOID compare )
{
    PVOID ret;
    __asm__ __volatile__( "lock; cmpxchgl %2,(%1)"
                          : "=a" (ret) : "r" (dest), "r" (xchg), "0" (compare) : "memory" );
    return ret;
}
inline static LONG interlocked_inc( PLONG dest )
{
    LONG ret;
    __asm__ __volatile__( "lock; xaddl %0,(%1)"
                          : "=r" (ret) : "r" (dest), "0" (1) : "memory" );
    return ret + 1;
}
inline static LONG interlocked_dec( PLONG dest )
{
    LONG ret;
    __asm__ __volatile__( "lock; xaddl %0,(%1)"
                          : "=r" (ret) : "r" (dest), "0" (-1) : "memory" );
    return ret - 1;
}

# else  /* __GNUC__ */

PVOID WINAPI interlocked_cmpxchg( PVOID *dest, PVOID xchg, PVOID compare );
__ASM_GLOBAL_FUNC(interlocked_cmpxchg,
                  "movl 12(%esp),%eax\n\t"
                  "movl 8(%esp),%ecx\n\t"
                  "movl 4(%esp),%edx\n\t"
                  "lock; cmpxchgl %ecx,(%edx)\n\t"
                  "ret $12");
LONG WINAPI interlocked_inc( PLONG dest );
__ASM_GLOBAL_FUNC(interlocked_inc,
                  "movl 4(%esp),%edx\n\t"
                  "movl $1,%eax\n\t"
                  "lock; xaddl %eax,(%edx)\n\t"
                  "incl %eax\n\t"
                  "ret $4");
LONG WINAPI interlocked_dec( PLONG dest );
__ASM_GLOBAL_FUNC(interlocked_dec,
                  "movl 4(%esp),%edx\n\t"
                  "movl $-1,%eax\n\t"
                  "lock; xaddl %eax,(%edx)\n\t"
                  "decl %eax\n\t"
                  "ret $4");
# endif  /* __GNUC__ */

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

static PVOID interlocked_cmpxchg( PVOID *dest, PVOID xchg, PVOID compare )
{
    _lwp_mutex_lock( &interlocked_mutex );
    if ( *dest == compare )
        *dest = xchg;
    else
        compare = *dest;
    _lwp_mutex_unlock( &interlocked_mutex );
    return compare;
}

static LONG interlocked_inc( PLONG dest )
{
    LONG retv;
    _lwp_mutex_lock( &interlocked_mutex );
    retv = ++*dest;
    _lwp_mutex_unlock( &interlocked_mutex );
    return retv;
}

static LONG interlocked_dec( PLONG dest )
{
    LONG retv;
    _lwp_mutex_lock( &interlocked_mutex );
    retv = --*dest;
    _lwp_mutex_unlock( &interlocked_mutex );
    return retv;
}
#else
# error You must implement the interlocked* functions for your CPU
#endif



/***********************************************************************
 *           get_semaphore
 */
static inline HANDLE get_semaphore( RTL_CRITICAL_SECTION *crit )
{
    HANDLE ret = crit->LockSemaphore;
    if (!ret)
    {
        HANDLE sem;
        if (NtCreateSemaphore( &sem, SEMAPHORE_ALL_ACCESS, NULL, 0, 1 )) return 0;
        if (!(ret = (HANDLE)interlocked_cmpxchg( (PVOID *)&crit->LockSemaphore,
                                                 (PVOID)sem, 0 )))
            ret = sem;
        else
            NtClose(sem);  /* somebody beat us to it */
    }
    return ret;
}

/***********************************************************************
 *           alloc_debug_info
 *
 * NOTE: We have a little bit of a problem here when running with pthread emulation.
 *       The reason is that malloc uses pthread_mutex. We simulate the mutex with
 *       critical sections (you see where this is going). Thus if we call malloc we
 *       end up in a nice nice nice infinite loop. Unfortunately, calling HeapAlloc
 *       will create a view which uses malloc hence we basically can't allocate any
 *       memory!
 */
static inline void alloc_debug_info( PRTL_CRITICAL_SECTION crit )
{
    PRTL_CRITICAL_SECTION_DEBUG pDebug;

    if( crit != &cs_cs ) RtlEnterCriticalSection( &cs_cs );

    /* The first NUMBER_OF_DEBUG_SECTIONS crit sections have pre
     * allocated memory. This allows fundamental functionality to
     * rely on critical sections at the start of WineX.
     */
    if( dwAllocatedDebugSections >= NUMBER_OF_DEBUG_SECTIONS )
    {
        crit->DebugInfo = HeapAlloc( GetProcessHeap(), 0, sizeof( *pDebug ) );
    }
    else
    {
        crit->DebugInfo = &preallocated_debug_area[ dwAllocatedDebugSections++ ];
    }
    
    if( !crit->DebugInfo ) goto done;

    crit->DebugInfo->Type            = RTL_CRITSECT_TYPE;
    crit->DebugInfo->CriticalSection = crit;
    crit->DebugInfo->EntryCount      = 0;
    crit->DebugInfo->ContentionCount = 0;

#ifdef CS_BACKTRACES
    crit->DebugInfo->CreatorBackTraceIndex = CreateAndStoreBacktrace();
#endif

    crit->DebugInfo->ProcessLocksList.Flink = &csList;
    crit->DebugInfo->ProcessLocksList.Blink = csList.Blink;
    csList.Blink->Flink = &crit->DebugInfo->ProcessLocksList;
    csList.Blink        = &crit->DebugInfo->ProcessLocksList;

    /* Spare is not supposed to be initialized. Fun tricks can ensue.
     * We, however, to speed up checks against NULL will initialize
     */
    crit->DebugInfo->Spare[0] = crit->DebugInfo->Spare[1] = 0;

done:
    if( crit != &cs_cs ) RtlLeaveCriticalSection( &cs_cs );

    TRACE( "Allocated DebugSection %p for cs %p\n", crit->DebugInfo, crit );
}

/***********************************************************************
 *           free_debug_info
 */
void free_debug_info( PRTL_CRITICAL_SECTION crit )
{
    PRTL_CRITICAL_SECTION_DEBUG info;
#if 0
    LIST_ENTRY* lpList;
#endif

#if !defined( USE_PTHREADS )
    /* DebugInfo is not valid when we're using pthread emulation so just ignore it */
    crit->DebugInfo = NULL;
    return;
#endif

    /* There may not be any DebugInfo if the crit section was made global */
    if( !crit->DebugInfo ) return;


    if( crit != &cs_cs ) RtlEnterCriticalSection( &cs_cs );

    info = crit->DebugInfo;

#if 0
    for( lpList = csList.Flink; lpList != &csList;  lpList = lpList->Flink )
    {
      PRTL_CRITICAL_SECTION_DEBUG debug = 
       (PRTL_CRITICAL_SECTION_DEBUG)( (BYTE*)lpList - offsetof(RTL_CRITICAL_SECTION_DEBUG,ProcessLocksList));
      if (debug == info) break;
    }
    if (lpList == &csList) {
      ERR_(ntdll)("critical section %p attempted freed without being allocated\n", crit);
      crit->DebugInfo = NULL;
      /* crit->DebugInfo->Type = 0; */ /* force a crash */
      return;
    }
#endif
 
    info->ProcessLocksList.Flink->Blink = info->ProcessLocksList.Blink;
    info->ProcessLocksList.Blink->Flink = info->ProcessLocksList.Flink;

    info->Spare[0] = info->Spare[1] = 0;

#ifdef CS_BACKTRACES
    ReleaseBacktrace( info->CreatorBackTraceIndex );
#endif
    info->CreatorBackTraceIndex = 0;

    crit->DebugInfo = NULL;
    
    if( crit != &cs_cs ) RtlLeaveCriticalSection( &cs_cs );
    
    /* We only need to free if not statically allocated memory */
    if( (info < &preallocated_debug_area[0]) ||
        (info >= &preallocated_debug_area[NUMBER_OF_DEBUG_SECTIONS]) )
    {
        HeapFree( GetProcessHeap(), 0, info );
    }    
}

/***********************************************************************
 *           RtlInitializeCriticalSection   (NTDLL.@)
 */
NTSTATUS WINAPI RtlInitializeCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    return RtlInitializeCriticalSectionAndSpinCount( crit, 0 );
}

/***********************************************************************
 *           RtlSetCriticalSectionSpinCount   (NTDLL.@)
 */
ULONG_PTR WINAPI RtlSetCriticalSectionSpinCount( RTL_CRITICAL_SECTION *crit, DWORD spincount )
{
    ULONG_PTR oldspincount = crit->SpinCount;

    crit->SpinCount = multiprocessor ? spincount : 0;

    return oldspincount;
}

/***********************************************************************
 *           RtlInitializeCriticalSectionAndSpinCount   (NTDLL.@)
 * The InitializeCriticalSectionAndSpinCount (KERNEL32) function is
 * available on NT4SP3 or later, and Win98 or later.
 */
NTSTATUS WINAPI RtlInitializeCriticalSectionAndSpinCount( RTL_CRITICAL_SECTION *crit, DWORD spincount )
{
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    crit->LockSemaphore  = 0;

    RtlSetCriticalSectionSpinCount( crit, spincount );

#if defined( USE_PTHREADS )
    alloc_debug_info( crit );
    if( crit->DebugInfo == NULL )
    {
        WARN( "Not enough memory for crit=%p\n", crit );
        return STATUS_NO_MEMORY;
    }
#else
    /* See notes in alloc_debug_info */
    crit->DebugInfo = NULL;
#endif

#if 0
    TRACE( "Allocated DebugSection %p for cs %p\n", crit->DebugInfo, crit );
#endif

    return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlDeleteCriticalSection   (NTDLL.@)
 */
NTSTATUS WINAPI RtlDeleteCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    if (crit->LockSemaphore) NtClose( crit->LockSemaphore );
    crit->LockSemaphore  = 0;

    free_debug_info( crit );

    return STATUS_SUCCESS;
}

/***********************************************************************
 *           display_wait_error
 */
static void display_wait_error( PRTL_CRITICAL_SECTION crit, LPCSTR str )
{
    const char* name = "?";
    const char* backtrace = "none";

    if( ERR_ON(ntdll) )
    {
        char buf[256];
        DWORD EntryCount = 0;
        DWORD ContentionCount = 0;

        dump_critsec( crit, buf, sizeof( buf ) );

        if( crit != &cs_cs ) RtlEnterCriticalSection( &cs_cs );    

        if( crit->DebugInfo )
        {
#if defined( USE_PTHREADS )
            /* Spare[1] is not intitialized. Is it actually pointing at something
             * which might be alright?
             */
            if( crit->DebugInfo->Spare[1] && !IsBadStringPtrA( (const char*)crit->DebugInfo->Spare[1], 80 ))
              name = (const char *)crit->DebugInfo->Spare[1];
            backtrace = RetrieveBacktrace( crit->DebugInfo->CreatorBackTraceIndex );

            EntryCount      = crit->DebugInfo->EntryCount;
            ContentionCount = crit->DebugInfo->ContentionCount;
#else
            /* At best DebugInfo is the name of the crit section. There is no backtrace info. */
            if( crit->DebugInfo && !IsBadStringPtrA( (const char*)crit->DebugInfo, 80 ))
              name = (const char*)crit->DebugInfo;
#endif
        }

        /* Note that content of crit are not guaranteed to be correct. But provide a possible cause. */
        ERR( "%s: crit=%p name=%s ec=%lu cc=%lu\n", str, crit, debugstr_a(name), EntryCount, ContentionCount );
        ERR( "backtrace=%s\n",  debugstr_a(backtrace) );
        ERR( "  %s\n", buf );

        if( crit != &cs_cs ) RtlLeaveCriticalSection( &cs_cs );
    }
}

/***********************************************************************
 *           RtlpWaitForCriticalSection   (NTDLL.@)
 */
NTSTATUS WINAPI RtlpWaitForCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    for (;;)
    {
        EXCEPTION_RECORD rec;
        HANDLE sem = get_semaphore( crit );
        DWORD res;
	
#if defined( USE_PTHREADS )
        if( crit->DebugInfo )
        {
            /* MSDN December 2003 article seems to say that these both have
               same purpose - tracking the number of times threads have entered
               a wait state trying to acquire this critical section; the
               values are thus never decremented. Since this function is only
               called once there's been contention, we don't need to do
               any additional checks prior to incrementing */
            interlocked_inc( (PLONG)&crit->DebugInfo->EntryCount );
            interlocked_inc( (PLONG)&crit->DebugInfo->ContentionCount );
        }
#endif

        res = WaitForSingleObject( sem, 5000L );
        if ( res == WAIT_TIMEOUT )
        {
            display_wait_error( crit, "Timeout. Retry with 60 secs" );
            res = WaitForSingleObject( sem, 60000L );

#if defined( USE_PTHREADS )
            /* If critical section is unnamed, assume it has been created by the app
             * and give it more leeway as far as timeouts are concerned.  */
            if ( res == WAIT_TIMEOUT && crit->DebugInfo && !crit->DebugInfo->Spare[1] ) {
                display_wait_error( crit, "Timeout. Assuming unnamed critsection belongs to app. Retrying" );
                res = WaitForSingleObject( sem, 2592000L );
            }
#endif

            /* Since the code can execute slowly with relay on... give it another longer chance */
            if ( res == WAIT_TIMEOUT && TRACE_ON(relay) )
            {
                display_wait_error( crit, "Timeout. Retry with 5 mins" );
                res = WaitForSingleObject( sem, 300000L );
            }

            if (res == STATUS_WAIT_0)
                display_wait_error( crit, "Section acquired" );
        }
        if (res == STATUS_WAIT_0) return STATUS_SUCCESS;

/* PH: FIXME: Should the lock be decremented here since we're no longer waiting for it? */

        rec.ExceptionCode    = EXCEPTION_CRITICAL_SECTION_WAIT;
        rec.ExceptionFlags   = 0;
        rec.ExceptionRecord  = NULL;
        rec.ExceptionAddress = RtlRaiseException;  /* sic */
        rec.NumberParameters = 1;
        rec.ExceptionInformation[0] = (DWORD)crit;
        RtlRaiseException( &rec );
    }
}


/***********************************************************************
 *           RtlpUnWaitCriticalSection   (NTDLL.@)
 */
NTSTATUS WINAPI RtlpUnWaitCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    HANDLE sem = get_semaphore( crit );
    NTSTATUS res = NtReleaseSemaphore( sem, 1, NULL );
    if (res) RtlRaiseStatus( res );
    return res;
}


/***********************************************************************
 * function called by EnterCriticalSection if the section is contested
 * Note - must be WINAPI (and non-static?) to ensure stack alignment
 * fixups are done on MacOS X, since we don't do them in the
 * hand-coded EnterCriticalSection()
*/
NTSTATUS WINAPI EnterContestedCriticalSection (RTL_CRITICAL_SECTION *crit,
                                               DWORD dwThreadId)
{
   DWORD dwCount;

   if (crit->OwningThread == dwThreadId)
   {
      crit->RecursionCount++;
      return STATUS_SUCCESS;
   }

   /* Spin if desired */
   for (dwCount = crit->SpinCount; dwCount; dwCount--)
   {
      if (RtlTryEnterCriticalSection (crit))
         return STATUS_SUCCESS;
   }

   /* Blocking wait for it */
   RtlpWaitForCriticalSection (crit);
    
   crit->OwningThread   = dwThreadId;
   crit->RecursionCount = 1;

   return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlEnterCriticalSection   (NTDLL.@)
 *
 */
#ifdef __i386__
__ASM_GLOBAL_FUNC(RtlEnterCriticalSection,
                  "mov    %fs:0x24,%ecx\n\t"
                  "mov    0x4(%esp),%edx\n\t"
                  "lock; incl 0x4(%edx)\n\t"
                  "jne    1f\n\t"
                  "mov    %ecx,%eax\n\t"
                  "movl   $0x1,0x8(%edx)\n\t"
                  "mov    %eax,0xc(%edx)\n\t"
                  "xor    %eax,%eax\n\t"
                  "ret    $0x4\n\t"
                  "1: push   %ecx\n\t"
                  "push   %edx\n\t"
                  "call   " __ASM_NAME("EnterContestedCriticalSection") "\n\t"
                  "ret    $0x4");
#else
NTSTATUS WINAPI RtlEnterCriticalSection( RTL_CRITICAL_SECTION *crit )
{
   const DWORD dwThreadId = GetCurrentThreadId ();

   if (!interlocked_inc (&crit->LockCount))
   {
      crit->OwningThread   = dwThreadId;
      crit->RecursionCount = 1;

      return STATUS_SUCCESS;
   }

   return EnterContestedCriticalSection (crit, dwThreadId);
}
#endif


/***********************************************************************
 *           RtlTryEnterCriticalSection   (NTDLL.@)
 */
BOOL WINAPI RtlTryEnterCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    BOOL ret = FALSE;
    if (interlocked_cmpxchg( (PVOID *)&crit->LockCount, (PVOID)0L, (PVOID)-1L ) == (PVOID)-1L)
    {
        crit->OwningThread   = GetCurrentThreadId();
        crit->RecursionCount = 1;
        ret = TRUE;
    }
    else if (crit->OwningThread == GetCurrentThreadId())
    {
        interlocked_inc( &crit->LockCount );
        crit->RecursionCount++;
        ret = TRUE;
    }
    return ret;
}


/***********************************************************************
 *           RtlLeaveCriticalSection   (NTDLL.@)
 */
#ifdef __i386__
__ASM_GLOBAL_FUNC(RtlLeaveCriticalSection,
                  "mov    0x4(%esp),%edx\n\t"
                  "xor    %eax,%eax\n\t"
                  "decl   0x8(%edx)\n\t"
                  "jne    1f\n\t"
                  "mov    %eax,0xc(%edx)\n\t"
                  "lock; decl 0x4(%edx)\n\t"
                  "jge    2f\n\t"
                  "ret    $0x4\n\t"
                  "1: lock; decl 0x4(%edx)\n\t"
                  "ret    $0x4\n\t"
                  "2: push %edx\n\t"
                  "call " __ASM_NAME("RtlpUnWaitCriticalSection") "\n\t"
                  "xor    %eax,%eax\n\t"
                  "ret    $0x4");
#else
NTSTATUS WINAPI RtlLeaveCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (--crit->RecursionCount) interlocked_dec( &crit->LockCount );
    else
    {
        crit->OwningThread = 0;
        if (interlocked_dec( &crit->LockCount ) >= 0)
        {
            /* someone is waiting */
            RtlpUnWaitCriticalSection( crit );
        }
    }
    return STATUS_SUCCESS;
}
#endif
