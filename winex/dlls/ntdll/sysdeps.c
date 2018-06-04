/*
 * System-dependent scheduler support
 *
 * Copyright 1998 Alexandre Julliard
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"
#include "wine/port.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/resource.h>
#include <errno.h>
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_LWP_H
# include <sys/lwp.h>
#endif
#ifdef USE_PTHREADS
#include <pthread.h>
#include <limits.h>
#endif
#ifdef HAVE_UCONTEXT_H
# include <ucontext.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "wine/thread.h"
#include "wine/server.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "wine/exception.h"
#include "wine/hardware.h"
#include "wine/debug.h"
#include "selectors.h"
#include "snoop.h"

#if defined( USE_PTHREADS )
WINE_DEFAULT_DEBUG_CHANNEL(thread); 
#endif

#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/thread_info.h>
#ifdef __i386__
#if TG_MAC_OS_X_SDK_VERSION >= TG_MAC_OS_X_VERSION_MAVERICKS
#include <mach/thread_act.h>
#include <mach/thread_status.h>
#else
#include <mach/i386/thread_act.h>
#include <mach/i386/thread_status.h>
#endif
#else
#error "Need header for thread_info!"
#endif
#endif

#if defined(linux) || defined(HAVE_CLONE)
# ifdef HAVE_SCHED_H
#  include <sched.h>
# endif
# ifndef CLONE_VM
#  define CLONE_VM      0x00000100
#  define CLONE_FS      0x00000200
#  define CLONE_FILES   0x00000400
#  define CLONE_SIGHAND 0x00000800
#  define CLONE_PID     0x00001000
# endif  /* CLONE_VM */
#endif  /* linux || HAVE_CLONE */

#ifdef USE_PTHREADS
#define CHECK_ERR( op ) do { int err = (op); if( err ) { fprintf( stderr, "\"" # op "\" failed at %s:%d: %s\n", __FILE__, __LINE__, strerror( err ) ); goto pthread_error; } } while(0)
#endif

/* temporary stacks used on thread exit */
#define TEMP_STACK_SIZE 1024
#define NB_TEMP_STACKS  8
static char temp_stacks[NB_TEMP_STACKS][TEMP_STACK_SIZE];
static LONG next_temp_stack;  /* next temp stack to use */


#ifdef USE_PTHREADS
#define CHECK_ERR( op ) do { int err = (op); if( err ) { fprintf( stderr, "\"" # op "\" failed at %s:%d: %s\n", __FILE__, __LINE__, strerror( err ) ); goto pthread_error; } } while(0)
#endif

/* temporary stacks used on thread exit */
#define TEMP_STACK_SIZE 1024
#define NB_TEMP_STACKS  8
static char temp_stacks[NB_TEMP_STACKS][TEMP_STACK_SIZE];
static LONG next_temp_stack;  /* next temp stack to use */


struct thread_cleanup_info
{
    void *stack_base;
    int   stack_size;
    int   status;
    TEB  *teb;
};

#if defined(USE_PTHREADS)
pthread_cond_t new_dying_thread_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t dying_thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
CRITICAL_SECTION dying_thread_list_cs;
TEB* dying_thread_list_head; /* = NULL */

#if defined( USE_PTHREADS ) && !defined( __i386__ )

static pthread_key_t teb_key;
static void init_teb_key(void)
{
    CHECK_ERR( pthread_key_create(&teb_key, NULL) );
pthread_error: { /* Do nothing special */ }
    return;
}

#endif

#if defined( USE_PTHREADS )

static long int thread_count = 1;

static inline void SYSDEPS_PTHREADS_NewThread(void)
{
  InterlockedIncrement( &thread_count );
}

static void SYSDEPS_PTHREADS_ExitThread( int status ) WINE_NORETURN;
static void SYSDEPS_PTHREADS_ExitThread( int status )
{
  long int tc = InterlockedDecrement( &thread_count );
  
  if( tc == 0 )
  {
    exit( status ); /* End the process */
  }

  pthread_exit(NULL);
}

#endif


/***********************************************************************
 * SYSDEPS_SanitizeEFlags ()
 *
 * Check for bits left enabled in EFlags by thread that will cause
 * problems (currently the NT bit is left enabled by the system tests
 * in one game)
 */
#ifdef __i386__
void SYSDEPS_SanitizeEFlags ()
{
   DWORD dwFlags;

   asm ("pushf;pop %0" : "=r" (dwFlags));

   if (dwFlags & 0x4000)
   {
      dwFlags &= ~0x4000;
      asm ("push %0;popf" : : "r" (dwFlags));
   }
}
#else
void SYSDEPS_SanitizeEFlags () {}
#endif


/***********************************************************************
 *           SYSDEPS_SetCurThread
 *
 * Make 'thread' the current thread.
 */
void SYSDEPS_SetCurThread( TEB *teb )
{
#ifdef USE_PTHREADS
    teb->unix_tid_or_pid = wine_gettid_or_pid();
#endif

    teb->unix_inprocess_tid = wine_get_inprocess_tid();

#if defined(__i386__)
    /* On the i386, the current thread is in the %fs register */
    SELECTOR_SetupFs( teb->teb_sel, teb );
#elif defined(HAVE__LWP_CREATE)
    /* On non-i386 Solaris, we use the LWP private pointer */
    _lwp_setprivate( teb );
#elif defined(USE_PTHREADS)
    {
        static pthread_once_t once_control = PTHREAD_ONCE_INIT;

        pthread_once(&once_control, init_teb_key);
        CHECK_ERR( pthread_setspecific(teb_key, teb) );
pthread_error: { /* do nothing */ }
	
    }

#endif
}


/***********************************************************************
 *           call_on_thread_stack
 *
 * Call a function once we switched to the thread stack.
 */
static void call_on_thread_stack( void *func )
{
    __TRY
    {
        void (*funcptr)(void) = func;
        funcptr();
    }
    __EXCEPT(UnhandledExceptionFilter)
    {
        TerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    SYSDEPS_ExitThread(0);  /* should never get here */
}

/***********************************************************************
 *           get_temp_stack
 *
 * Get a temporary stack address to run the thread exit code on.
 */
static inline char *get_temp_stack(void)
{
    unsigned int next = InterlockedExchangeAdd( &next_temp_stack, 1 );
    return temp_stacks[next % NB_TEMP_STACKS];
}

extern void VIRTUAL_FreeMemory( LPVOID addr );
extern BOOL use_memory_manager;

#if !defined( USE_PTHREADS )

/***********************************************************************
 *           cleanup_thread
 *
 * Cleanup the remains of a thread. Runs on a temporary stack.
 */
static void cleanup_thread( void *ptr ) WINE_NORETURN;
static void cleanup_thread( void *ptr ) 
{
    struct thread_cleanup_info info = *(struct thread_cleanup_info *)ptr;

    /* Clear out fs. Do not use anything windows related as NtCurrentTEB is
     * now invalid */
    SELECTOR_FreeFs( __get_fs() );
    __set_fs( 0 );

    if (!use_memory_manager)
        munmap( info.stack_base, info.stack_size );

#if defined(HAVE__LWP_CREATE)
    _lwp_exit();
#elif defined(USE_PTHREADS)
    fprintf( stderr, "Shouldn't be called at this point\n" );
    SYSDEPS_PTHREADS_ExitThread( info.status );
#else
    _exit( info.status );
#endif

}
#endif

/***********************************************************************
 *           SYSDEPS_StartThread
 *
 * Startup routine for a new thread.
 */
static
void SYSDEPS_StartThread( TEB *teb )
{
    SYSDEPS_SetCurThread( teb );
    CLIENT_InitThread (FALSE);
    SIGNAL_Init();

    __TRY
    {
        teb->startup();
    }
    __EXCEPT(UnhandledExceptionFilter)
    {
        TerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    SYSDEPS_ExitThread(0);  /* should never get here */
}

#ifdef USE_PTHREADS
/***********************************************************************
 *           SYSDEPS_PTHREADS_StartThread
 *
 * Startup routine for a new pthread thread.
 */
static void* SYSDEPS_PTHREADS_StartThread( void* arg )
{
    TEB* teb = arg;
    
    SYSDEPS_PTHREADS_NewThread();

    SYSDEPS_StartThread( teb );
    
    return NULL; /* Avoid potential compiler warning */
}
#endif

/***********************************************************************
 *           SYSDEPS_SpawnThread
 *
 * Start running a new thread.
 * Return -1 on error, 0 if OK.
 */
int SYSDEPS_SpawnThread( TEB *teb )
{

#if defined( USE_PTHREADS )
        pthread_t thread;
        pthread_attr_t attr;

        SIGNAL_SuspendSignalStack();

        CHECK_ERR( pthread_attr_init(&attr) );

        CHECK_ERR( pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) );

# if  defined( HAVE_PTHREAD_ATTR_SETSTACK )
        CHECK_ERR( pthread_attr_setstack(&attr, teb->stack_low,
                                         (ULONG_PTR)teb->stack_top
                                         - (ULONG_PTR)teb->stack_low) );
# else
        CHECK_ERR( pthread_attr_setstackaddr(&attr, teb->stack_top) );
        CHECK_ERR( pthread_attr_setstacksize(&attr, (ULONG_PTR)teb->stack_top
			      - (ULONG_PTR)teb->stack_low) );
# endif

        CHECK_ERR( pthread_create(&thread, &attr, SYSDEPS_PTHREADS_StartThread, teb) );
        CHECK_ERR( pthread_attr_destroy(&attr) );

        SIGNAL_RestoreSignalStack();
    
        return 0;

pthread_error:    
        return -1;

#elif (defined(linux) || defined(HAVE_CLONE))
    {
        const int flags = CLONE_VM | CLONE_FS | CLONE_FILES | SIGCHLD;
        if (clone( (int (*)(void *))SYSDEPS_StartThread, teb->stack_top, flags, teb ) < 0)
            return -1;
        if (!(flags & CLONE_FILES)) close( teb->request_fd );  /* close the child socket in the parent */
        return 0;
    }

#elif defined(HAVE_RFORK) && defined( __i386__ )
    {
        const int flags = RFPROC | RFMEM; /*|RFFDG*/
        void **sp = (void **)teb->stack_top;
        *--sp = teb;
        *--sp = 0;
        *--sp = SYSDEPS_StartThread;
        __asm__ __volatile__(
        "pushl %2;\n\t"		/* flags */
        "pushl $0;\n\t"		/* 0 ? */
        "movl %1,%%eax;\n\t"	/* SYS_rfork */
        ".byte 0x9a; .long 0; .word 7;\n\t"	/* lcall 7:0... FreeBSD syscall */
        "cmpl $0, %%edx;\n\t"
        "je 1f;\n\t"
        "movl %0,%%esp;\n\t"	/* child -> new thread */
        "ret;\n"
        "1:\n\t"		/* parent -> caller thread */
        "addl $8,%%esp" :
        : "r" (sp), "g" (SYS_rfork), "g" (flags)
        : "eax", "edx");
        if (flags & RFFDG) close( teb->request_fd );  /* close the child socket in the parent */
       return 0;
    }
#elif defined(HAVE__LWP_CREATE)
    {
        ucontext_t context;
        _lwp_makecontext( &context, (void(*)(void *))SYSDEPS_StartThread, teb,
                          NULL, teb->stack_base, (char *)teb->stack_top - (char *)teb->stack_base );
        if ( _lwp_create( &context, 0, NULL ) )
            return -1;
        return 0;
    }
#else
    FIXME("CreateThread: stub\n" );
    return 0;
#endif

}


/***********************************************************************
 *           SYSDEPS_CallOnStack
 */
void SYSDEPS_CallOnStack( void (*func)(LPVOID), LPVOID arg )
    WINE_NORETURN;
#ifdef __i386__
#ifdef __GNUC__
__ASM_GLOBAL_FUNC( SYSDEPS_CallOnStack,
                   "movl 4(%esp),%ecx\n\t"  /* func */
                   "movl 8(%esp),%edx\n\t"  /* arg */
                   ".byte 0x64\n\tmovl 0x04,%esp\n\t"  /* teb->stack_top */
                   "pushl %edx\n\t"
                   "xorl %ebp,%ebp\n\t"
                   "call *%ecx\n\t"
                   "int $3" /* we never return here */ );
#elif defined(_MSC_VER)
__declspec(naked) void SYSDEPS_CallOnStack( void (*func)(LPVOID), LPVOID arg )
{
  __asm mov ecx, 4[esp];
  __asm mov edx, 8[esp];
  __asm mov fs:[0x04], esp;
  __asm push edx;
  __asm xor ebp, ebp;
  __asm call [ecx];
  __asm int 3;
}
#endif /* defined(__GNUC__) || defined(_MSC_VER) */
#else /* defined(__i386__) */
void SYSDEPS_CallOnStack( void (*func)(LPVOID), LPVOID arg )
{
    func( arg );
    while(1); /* avoid warning */
}
#endif /* defined(__i386__) */




/***********************************************************************
 *           SYSDEPS_SwitchToThreadStack
 */
void SYSDEPS_SwitchToThreadStack( void (*func)(void) )
{
    SYSDEPS_CallOnStack( call_on_thread_stack, func );
}

/***********************************************************************
 *           SYSDEPS_ExitThread
 *
 * Exit a running thread; must not return.
 */
void SYSDEPS_ExitThread( int status )
{
    TEB *teb = NtCurrentTeb();
    struct thread_cleanup_info info;

    SNOOP_ThreadExit ();

    FreeSelector16( teb->stack_sel );
    info.stack_base = teb->stack_base;
    info.stack_size = teb->total_stack_size;
    info.status     = status;
    info.teb        = teb;

    if (teb->TlsExpansionSlots)
        HeapFree(GetProcessHeap(), 0, teb->TlsExpansionSlots);

    SIGNAL_Reset();

    if (!use_memory_manager) {
        /* Start the 2-stage stack destruction. It isn't safe to use VirtualFree
         * to free the stack (even once we've changed stacks) because that
         * includes the TEB which VirtualFree might try to access.
         * However, once we're on a new stack, it's safe to call munmap. */
        VirtualFree( teb->stack_base, 0, MEM_RELEASE | MEM_SYSTEM );
    }

    /* We cannot free the TEB from inside thread destruction if the memory
     * manager is enabled, because the virtual memory critical section
     * needs the TEB. Have to insert it into the dying thread list instead. */

    if (use_memory_manager)
    {
        EnterCriticalSection( &dying_thread_list_cs );

        teb->next_dying_teb = dying_thread_list_head;
        dying_thread_list_head = teb;

        LeaveCriticalSection( &dying_thread_list_cs );
    }

    close( teb->wait_fd[0] );
    close( teb->wait_fd[1] );
    close( teb->reply_fd );
    close( teb->request_fd );

#if defined( USE_PTHREADS )
                                                                                                      
      /* Clear out fs. Do not use anything windows related as NtCurrentTEB is
       * now invalid */
      SELECTOR_FreeFs( __get_fs() );
      __set_fs( 0 );

      if (!use_memory_manager)
      {
        pthread_mutex_lock(&dying_thread_list_mutex);

        teb->next_dying_teb = dying_thread_list_head;
        dying_thread_list_head = teb;

        pthread_cond_signal(&new_dying_thread_cond);
        pthread_mutex_unlock(&dying_thread_list_mutex);
      }

      SYSDEPS_PTHREADS_ExitThread( status );

#else

      /* Allocate a new stack for us to finish our cleanup */
      teb->stack_low = get_temp_stack();
      teb->stack_top = teb->stack_low + TEMP_STACK_SIZE;

      SYSDEPS_CallOnStack( cleanup_thread, &info );

#endif
}


/***********************************************************************
 *           SYSDEPS_AbortThread
 *
 * Same as SYSDEPS_ExitThread, but must not do anything that requires a server call.
 */
void SYSDEPS_AbortThread( int status )
{
    /* Ensure that we cleanly exit if we get stopped in a non-wine thread */
    if (__get_fs() == 0)
        _exit( status);

    SIGNAL_Reset();

    close( NtCurrentTeb()->wait_fd[0] );
    close( NtCurrentTeb()->wait_fd[1] );
    close( NtCurrentTeb()->reply_fd );
    close( NtCurrentTeb()->request_fd );

#if defined(USE_PTHREADS)
      SYSDEPS_PTHREADS_ExitThread( status );
#elif defined(HAVE__LWP_CREATE)
    _lwp_exit();
#else

    for (;;)  /* avoid warning */
        _exit( status );
#endif
}


/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 *
 * This will crash and burn if called before threading is initialized
 */
#if defined(__i386__) && defined(__GNUC__)
#ifndef __APPLE__
__ASM_GLOBAL_FUNC( NtCurrentTeb, ".byte 0x64\n\tmovl 0x18,%eax\n\tret" );
#endif 
#elif defined(__i386__) && defined(_MSC_VER)
/* Nothing needs to be done. MS C "magically" exports the inline version from winnt.h */
#elif defined(HAVE__LWP_CREATE)
/***********************************************************************
 *		NtCurrentTeb (NTDLL.@)
 */
struct _TEB * WINAPI NtCurrentTeb(void)
{
    extern void *_lwp_getprivate(void);
    return (struct _TEB *)_lwp_getprivate();
}
#elif defined(USE_PTHREADS)
struct _TEB * WINAPI NtCurrentTeb(void)
{
    return pthread_getspecific(teb_key);
}
#else
# error "NtCurrentTeb not defined for this architecture"
#endif  /* __i386__ */


static void SYSDEPS_DoCleanThreads(void)
{
    TEB** tebp;

    for (tebp = &dying_thread_list_head; *tebp != NULL; )
    {
        TEB* teb = *tebp;
        TEB* next = teb->next_dying_teb;

#ifdef __APPLE__
        thread_basic_info_data_t Info;
        mach_msg_type_number_t Size = sizeof (Info);

        if (thread_info (teb->unix_inprocess_tid, THREAD_BASIC_INFO,
                         (thread_info_t)&Info, &Size))
#else
        /* kill(pid, 0) is used to see if the pid exists.
         * (Yes, this strange usage is sanctioned.) */
        if (wine_tkill(teb->unix_inprocess_tid, 0) == -1 && errno == ESRCH)
#endif
        {
            if (use_memory_manager)
                VIRTUAL_FreeMemory( teb->stack_base );
#if defined( USE_PTHREADS )
            else
                munmap( teb->stack_base, teb->total_stack_size );
#endif

            *tebp = next;
        }
        else
        {
            tebp = &teb->next_dying_teb;
        }
    }
}

void SYSDEPS_CleanThreads(void)
{
#if defined( USE_PTHREADS )
    if (!use_memory_manager) return;

    if (!TryEnterCriticalSection(&dying_thread_list_cs))
        return;

    SYSDEPS_DoCleanThreads();

    LeaveCriticalSection(&dying_thread_list_cs);
#endif
}

void SYSDEPS_PTHREADS_RunMainThread(void)
{

#if defined( USE_PTHREADS )
    while (TRUE)
    {
        if (use_memory_manager) {
            pause();
            continue;
        }

        pthread_mutex_lock(&dying_thread_list_mutex);

        if (dying_thread_list_head != NULL)
        {
            /* If a thread was still running, then its stack/teb could not be
             * destroyed, so it's still on the list.
             * We'll wait at most 2 seconds before trying again.
             */

            struct timeval now;
            struct timespec timeout;

            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + 2;
            timeout.tv_nsec = now.tv_usec * 1000;

            /* This may return in less than 2 seconds,
             * but the check below will handle things fine */
            pthread_cond_timedwait(&new_dying_thread_cond,
                                   &dying_thread_list_mutex,
                                   &timeout);
        }
        else
        {
            while (dying_thread_list_head == NULL)
            {
                pthread_cond_wait(&new_dying_thread_cond,
                                  &dying_thread_list_mutex);
            }
        }

        /* Ok, now somebody is on the list. */
        SYSDEPS_DoCleanThreads();

        pthread_mutex_unlock(&dying_thread_list_mutex);
    }
#else

    while (TRUE) pause();

#endif
}

void SYSDEPS_Init(void)
{
    InitializeCriticalSection( &dying_thread_list_cs );
    CRITICAL_SECTION_NAME( &dying_thread_list_cs, "dying_thread_list_cs" );
}

int SYSDEPS_sigprocmask( int how, const sigset_t *set, sigset_t *oldset )
{
#ifdef USE_PTHREADS
    return pthread_sigmask( how, set, oldset );
#else
    return sigprocmask( how, set, oldset );
#endif
}

#if defined( USE_PTHREADS )

/* Assume that the max allowed stack size is a multiple of page_size.
 * Max out at 256MB for systems with really high limits.
 */
DWORD SYSDEPS_PTHREADS_GetMaxStackSize(void)
{
  static DWORD pthread_max_stack = 0;
  const DWORD max_stack_to_search_for = 256 * 1024 * 1024;

  if( pthread_max_stack == 0 )
  {
     int err = 0;
     size_t try_stack = PTHREAD_STACK_MIN;
     const DWORD page_size = getpagesize();

     for( ; !err ; try_stack += page_size )
     {
       pthread_attr_t attr;
       pthread_attr_init(&attr);
       err = pthread_attr_setstacksize( &attr, try_stack );
       pthread_attr_destroy( &attr );
       if( try_stack > max_stack_to_search_for )
       {
         TRACE( "System stack limits high, capping at %lu bytes\n", max_stack_to_search_for );
         break;
       }
     }

     pthread_max_stack = (DWORD)try_stack - page_size;
     TRACE( "Max stack size is %lu\n", pthread_max_stack );
  }

  return pthread_max_stack;
}

#endif /* USE_PTHREADS */
