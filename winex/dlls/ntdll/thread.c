/*
 * Win32 threads
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <fcntl.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <sched.h>

#include "wine/winbase16.h"
#include "wine/thread.h"
#include "task.h"
#include "wine/module.h"
#include "winerror.h"
#include "selectors.h"
#include "winnt.h"
#include "wine/server.h"
#include "wine/stackframe.h"
#include "wine/debug.h"
#include "winnls.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);
WINE_DECLARE_DEBUG_CHANNEL(relay);

extern void ERRNO_init(void);

/* TEB of the initial thread */
static TEB initial_teb;

extern struct _PDB current_PDB;
extern struct _PEB current_PEB;

/***********************************************************************
 *           THREAD_IdToTEB
 *
 * Convert a thread id to a TEB, making sure it is valid.
 */
TEB *THREAD_IdToTEB( DWORD id )
{
    TEB *ret = NULL;

    if (!id || id == GetCurrentThreadId()) return NtCurrentTeb();

    SERVER_START_REQ( get_thread_info )
    {
        req->handle = 0;
        req->tid_in = (thread_id_t)id;
        if (!wine_server_call( req )) ret = reply->teb;
    }
    SERVER_END_REQ;

    if (!ret)
    {
        /* Allow task handles to be used; convert to main thread */
        if ( IsTask16( id ) )
        {
            TDB *pTask = TASK_GetPtr( id );
            if (pTask) return pTask->teb;
        }
        SetLastError( ERROR_INVALID_PARAMETER );
    }
    return ret;
}


/***********************************************************************
 *           THREAD_InitTEB
 *
 * Initialization of a newly created TEB.
 */
static BOOL THREAD_InitTEB( TEB *teb )
{
    /* The NVIDIA 23.13 drivers want sequential pthread_self return values. */
    static LONG next_pthread_id; /* = 0 */

    teb->except    = (void *)~0UL;
    teb->self      = teb;
    teb->tibflags  = TEBF_WIN32;
    teb->tls_ptr   = teb->tls_array;
    teb->TlsExpansionSlots = NULL;
    teb->exit_code = STILL_ACTIVE;
    teb->request_fd = -1;
    teb->reply_fd   = -1;
    teb->wait_fd[0] = -1;
    teb->wait_fd[1] = -1;
    teb->stack_top  = (void *)~0UL;
    teb->StaticUnicodeString.MaximumLength = sizeof(teb->StaticUnicodeBuffer);
    teb->StaticUnicodeString.Buffer = (PWSTR)teb->StaticUnicodeBuffer;
    teb->pthread_id = InterlockedIncrement(&next_pthread_id);
    teb->uRelayLevel = 0;
    teb->driver_data = 0;

    teb->tid = 0;
    teb->pid = 0;

    teb->teb_sel = SELECTOR_AllocFs( teb );

    return (teb->teb_sel != 0);
}


/***********************************************************************
 *           THREAD_FreeTEB
 *
 * Free data structures associated with a thread.
 * Must be called from the context of another thread.
 */
static void THREAD_FreeTEB( TEB *teb )
{
    TRACE("(%p) called\n", teb );

    /* Free the associated memory */
    FreeSelector16( teb->stack_sel );
    SELECTOR_FreeFs( teb->teb_sel );    

    VirtualFree( teb->stack_base, 0, MEM_RELEASE );
}

#ifdef USE_PTHREADS
extern DWORD SYSDEPS_PTHREADS_GetMaxStackSize(void);
#endif

/***********************************************************************
 *           THREAD_InitStack
 *
 * Allocate the stack of a thread.
 */
TEB *THREAD_InitStack( TEB *teb, DWORD stack_size )
{
    DWORD old_prot, total_size, sig_stack_size;
    DWORD page_size = getpagesize();
    void *base;
    const DWORD default_sig_stack_size = 1024* 1024;
#ifdef USE_PTHREADS
    DWORD max_stack_size = SYSDEPS_PTHREADS_GetMaxStackSize();
    DWORD desired_stack_size = stack_size;
#endif

    /* Allocate the stack */

    if (stack_size >= 16*1024*1024)
    	WARN("Thread stack size is %ld MB.\n",stack_size/1024/1024);

    /* if size is smaller than default, get stack size from parent */
    if (stack_size < 1024 * 1024)
    {
        if (teb)
            stack_size = 1024 * 1024;  /* no parent */
        else
            stack_size = ((char *)NtCurrentTeb()->stack_top - (char *)NtCurrentTeb()->stack_base
                          - SIGNAL_STACK_SIZE - 3 * page_size);
    }

    /* FIXME: some Wine functions use a lot of stack, so we add 64Kb here */
    stack_size += 64 * 1024;
    
    /* Memory layout in allocated block:
     *
     *   size                 contents
     * teb->stack_base
     * 1 page              NOACCESS guard page
     * SIGNAL_STACK_SIZE   signal stack
     * 1 page              NOACCESS guard page
     * 1 page              PAGE_GUARD guard page
     * stack_size          normal stack
     * teb->stack_top
     * 64Kb                16-bit stack (optional)
     * 1 page              TEB (except for initial thread)
     * 1 page              debug info (except for initial thread)
     *
     */

    stack_size     = (stack_size + (page_size - 1)) & ~(page_size - 1);
    sig_stack_size = default_sig_stack_size;

    total_size = stack_size + sig_stack_size + 3 * page_size;
    total_size += 0x10000; /* 16-bit stack */
    if (!teb) total_size += 2 * page_size;


#ifdef USE_PTHREADS
    /* Most versions of pthreads have some very lame stack restrictions */
    if( total_size > max_stack_size )
    {
      DWORD avail = max_stack_size;

      /* Take away constants (guard pages and 16 bit stack) */
      if( !teb ) avail -= 2 * page_size; /* TEB & debug info if not initial thread */
      avail -= 3 * page_size; /* Guard pages */
      avail -= 0x10000; /* 16 bit stack */

      /*
       * Now what remains is what we have available to carve between our stack and signal stack.
       * Give >=60% to the main stack but don't give more than default_sig_stack_size to the sig
       * stack. This division is just a heuristic and no real basis.
       */
      sig_stack_size = 0.4 * (float)avail;
      if( sig_stack_size > default_sig_stack_size ) sig_stack_size = default_sig_stack_size;
      sig_stack_size = sig_stack_size & ~(page_size - 1); /* Round down to nearest page size */

      stack_size = avail - sig_stack_size;

      WARN( "Desired stack (%lu->%lu) > %lu. Giving (%lu+%lu) \n",
              desired_stack_size, total_size, max_stack_size,
              stack_size, sig_stack_size );
	   
      assert( sig_stack_size );
      assert( stack_size );
    }
#endif

    if (!(base = VirtualAlloc( NULL, total_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE )))
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return NULL;
    }

    if (!teb)
    {
        teb = (TEB *)((char *)base + total_size - 2 * page_size);
        if (!THREAD_InitTEB( teb )) goto error;
        teb->debug_info = (char *)teb + page_size;
    }

    teb->stack_low      = base + (3 * page_size) + sig_stack_size;
    teb->stack_base     = base;
    teb->signal_stack   = (char *)base + page_size;
    teb->sig_stack_size = sig_stack_size;
    teb->stack_top      = (char *)base + 3 * page_size + sig_stack_size + stack_size;
    teb->total_stack_size = total_size;

#ifdef __APPLE__
    /* MacOS X 10.4.4 on x86 wants stack size to be a multiple of 4k, or
       at least pthread_attr_setstack() does */
    teb->stack_top -= ((ULONG)teb->stack_top - (ULONG)teb->stack_low) % 4096;
#endif

    TRACE ("base: %p, stack_low: %p, stack_top: %p, total_size: %ld\n",
           base, teb->stack_low, teb->stack_top, total_size);

    /* Setup guard pages */

    VirtualProtect( base, 1, PAGE_NOACCESS, &old_prot );
    VirtualProtect( (char *)teb->signal_stack + sig_stack_size, 1, PAGE_NOACCESS, &old_prot );
    VirtualProtect( (char *)teb->signal_stack + sig_stack_size + page_size, 1,
                    PAGE_EXECUTE_READWRITE | PAGE_GUARD, &old_prot );

    /* Allocate the 16-bit stack selector */

    teb->stack_sel = SELECTOR_AllocBlock( teb->stack_top, 0x10000, WINE_LDT_FLAGS_DATA );
    if (!teb->stack_sel) goto error;
    teb->WOW32Reserved = (void *)(MAKESEGPTR( teb->stack_sel, 0x10000 - sizeof(STACK16FRAME) ));

    return teb;

error:
    SetLastError( ERROR_NOT_ENOUGH_MEMORY );
    SELECTOR_FreeFs( teb->teb_sel );
    VirtualFree( base, 0, MEM_RELEASE );
    return NULL;
}


/***********************************************************************
 *           thread_errno_location
 *
 * Get the per-thread errno location.
 */
static int *thread_errno_location(void)
{
    /* FIXME  Brief window on thread shutdown where this is pointing to
     *        unallocated memory. Another brief period where this will
     *        crash as fs is set to 0. Same problem for h_errno.
     */
    return &NtCurrentTeb()->thread_errno;
}

/***********************************************************************
 *           thread_h_errno_location
 *
 * Get the per-thread h_errno location.
 */
static int *thread_h_errno_location(void)
{
    /* FIXME: Same problem as thread_errno_location */
    return &NtCurrentTeb()->thread_h_errno;
}

/***********************************************************************
 *           THREAD_Init
 *
 * Setup the initial thread.
 * Should be able to use debugmsg after this function is run.
 *
 * NOTES: The first allocated TEB on NT is at 0x7ffde000.
 */
void THREAD_Init(void)
{
    if (!initial_teb.self)  /* do it only once */
    {
        THREAD_InitTEB( &initial_teb );
        assert( initial_teb.teb_sel );
        initial_teb.process.PDB = &current_PDB;
        initial_teb.PEB = &current_PEB;
        SYSDEPS_SetCurThread( &initial_teb );
        wine_errno_location = thread_errno_location;
        wine_h_errno_location = thread_h_errno_location;
        ERRNO_init();
    }
}

/* using a wrapper function ensures that %ebx is reinitialized,
 * so that we can properly find ExitThread in the GOT, even
 * if the thread function clobbered %ebx before returning
 * (note, the wrapper must be static, so that it doesn't get
 * called through the GOT itself) */
static void THREAD_Exit(DWORD ret)
{
    ExitThread(ret);
}

/***********************************************************************
 *           THREAD_Start
 *
 * Start execution of a newly created thread. Does not return.
 */
static void THREAD_Start(void)
{
    LPTHREAD_START_ROUTINE func = (LPTHREAD_START_ROUTINE)NtCurrentTeb()->entry_point;

    if (TRACE_ON(relay))
        DPRINTF("%04lx:Starting thread (entryproc=%p)\n", GetCurrentThreadId(), func );

    PROCESS_CallUserSignalProc( USIG_THREAD_INIT, 0 );
    MODULE_DllThreadAttach( NULL );
    
    /* We seem to need this yield here due to MSI installers, which try 
    * to use the handle returned from a CreateThread call in the thread
    * itself.  Thus, CreateThread *must* return before the new thread
    * begins executing.  
    *
    * If the kernel schedules the new thread (executing in this function)
    * immediately after the CreateThread call creates and initializes this
    * thread, the yield below should allow the thread that called
    * CreateThread to run again before the new thread starts running,
    * which is what we suspect is actually happening on Windows, allowing
    * these installers to work.  Three calls to sched_yield() appears to 
    * be enough to allow the MSI code in question to work properly 
    * on several machines.  It's possible that other kernels / systems 
    * will need a few more yields before the offending code can run */
    sched_yield();
    sched_yield();
    sched_yield();

    THREAD_Exit( func( NtCurrentTeb()->entry_arg ) );
}


/***********************************************************************
 *           CreateThread   (KERNEL32.@)
 */
HANDLE WINAPI CreateThread( SECURITY_ATTRIBUTES *sa, DWORD stack,
                            LPTHREAD_START_ROUTINE start, LPVOID param,
                            DWORD flags, LPDWORD id )
{
    HANDLE handle = 0;
    TEB *teb;
    thread_id_t tid = 0;
    int request_pipe[2];
    int reply_pipe[2];
    int ret;
   
    TRACE("sec_attr: %p, stack: %ld, start_proc: %p, param: %p, flags: %ld, id: %p\n", 
	sa, stack, start, param, flags, id);

 restart:
    if (pipe( request_pipe ) == -1)
    {
        SetLastError( ERROR_TOO_MANY_OPEN_FILES );
        return 0;
    }
    if (pipe (reply_pipe) == -1)
    {
       SetLastError (ERROR_TOO_MANY_OPEN_FILES);
       close (request_pipe[0]);
       close (request_pipe[1]);
       return 0;
    }

    fcntl( request_pipe[1], F_SETFD, 1 ); /* set close on exec flag */
    wine_server_send_fd( request_pipe[0] );

    fcntl (reply_pipe[1], F_SETFD, 1);
    wine_server_send_fd (reply_pipe[1]);

    SERVER_START_REQ( new_thread )
    {
        req->suspend    = ((flags & CREATE_SUSPENDED) != 0);
        req->inherit    = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        req->request_fd = request_pipe[0];
        req->reply_fd   = reply_pipe[1];
        if (!(ret = wine_server_call_err( req )))
        {
            handle = reply->handle;
            tid = reply->tid;
        }
        close (request_pipe[0]);
        close (reply_pipe[1]);
    }
    SERVER_END_REQ;

    if (ret == STATUS_INVALID_HANDLE)
    {
       close (request_pipe[1]);
       close (reply_pipe[0]);
       goto restart;
    }

    if( !handle )
       goto error;

    if ( !(teb = THREAD_InitStack( NULL, stack )))
       goto error;

    teb->process.PDB = NtCurrentTeb ()->process.PDB;
    teb->has_peb     = NtCurrentTeb ()->has_peb;
    teb->PEB         = NtCurrentTeb ()->PEB;
    teb->tid         = tid;
    teb->request_fd  = request_pipe[1];
    teb->reply_fd    = reply_pipe[0];
    teb->entry_point = start;
    teb->entry_arg   = param;
    teb->startup     = THREAD_Start;
    teb->htask16     = GetCurrentTask();
    teb->TlsExpansionSlots = NULL;

    if (id) *id = (DWORD)tid;
    if (SYSDEPS_SpawnThread( teb ) == -1)
    {
        CloseHandle( handle );
        close( request_pipe[1] );
        THREAD_FreeTEB( teb );
        return 0;
    }
    return handle;

error:
    close (request_pipe[1]);
    close (reply_pipe[0]);
    return 0;
}

/***********************************************************************
 *           CreateRemoteThread   (KERNEL32.@)
 */

struct ex_mkthread {
    HANDLE from;
    HANDLE event;
    HANDLE *result;
    DWORD *err;
    BOOL inherit;
    DWORD stack;
    LPTHREAD_START_ROUTINE start;
    LPVOID param;
    DWORD flags;
    LPDWORD id;
};

HANDLE WINAPI CreateRemoteThread( HANDLE hProcess,
                                  SECURITY_ATTRIBUTES *sa, DWORD stack,
                                  LPTHREAD_START_ROUTINE start, LPVOID param,
                                  DWORD flags, LPDWORD id )
{
    DWORD pid = GetProcessId( hProcess );
    HANDLE ret = 0;
    DWORD err = 0;
    HANDLE event = 0;
    struct ex_mkthread ex;

    if (pid == GetCurrentProcessId())
        return CreateThread( sa, stack, start, param, flags, id );

    TRACE("sending interprocess CreateThread request\n");

    NtCreateEvent(&event, EVENT_ALL_ACCESS, NULL, FALSE, FALSE);
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(),
                    hProcess, &ex.from, PROCESS_ALL_ACCESS, FALSE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(GetCurrentProcess(), event,
                    hProcess, &ex.event, EVENT_ALL_ACCESS, FALSE, DUPLICATE_SAME_ACCESS);
    ex.result = &ret;
    ex.err = &err;
    ex.inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
    ex.stack = stack;
    ex.start = start;
    ex.param = param;
    ex.flags = flags;
    ex.id = id;

    SERVER_START_REQ( send_unix_signal )
    {
        req->thread = 0;
        req->process = pid;
        req->code = USC_MKTHREAD;
        wine_server_add_data( req, &ex, sizeof(ex) );
        wine_server_call( req );
    }
    SERVER_END_REQ;

    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);

    TRACE("received reply: 0x%08x 0x%08lx\n", ret, err);

    if (!ret) SetLastError(err);

    return ret;
}

void __wine_mkthread(void *data)
{
    struct ex_mkthread *ex = data;
    DWORD id = 0, err = 0;
    HANDLE thread, result;

    TRACE("received interprocess CreateThread request\n");
    thread = CreateThread(NULL, ex->stack, ex->start, ex->param, ex->flags, &id);

    if (thread)
        DuplicateHandle(GetCurrentProcess(), thread, ex->from, &result, THREAD_ALL_ACCESS, ex->inherit, DUPLICATE_SAME_ACCESS);
    else
        err = GetLastError();
    CloseHandle(thread);

    TRACE("sending reply\n");
    WriteProcessMemory(ex->from, ex->result, &thread, sizeof(thread), NULL);
    WriteProcessMemory(ex->from, ex->err, &err, sizeof(err), NULL);
    if (ex->id) WriteProcessMemory(ex->from, ex->id, &id, sizeof(id), NULL);
    NtSetEvent(ex->event, NULL);
    CloseHandle(ex->event);
    CloseHandle(ex->from);
}

/***********************************************************************
 *           CreateThread16   (KERNEL.441)
 */
static DWORD CALLBACK THREAD_StartThread16( LPVOID threadArgs )
{
    FARPROC16 start = ((FARPROC16 *)threadArgs)[0];
    DWORD     param = ((DWORD *)threadArgs)[1];
    HeapFree( GetProcessHeap(), 0, threadArgs );

    ((LPDWORD)CURRENT_STACK16)[-1] = param;
    return wine_call_to_16_long( start, sizeof(DWORD) );
}
HANDLE WINAPI CreateThread16( SECURITY_ATTRIBUTES *sa, DWORD stack,
                              FARPROC16 start, SEGPTR param,
                              DWORD flags, LPDWORD id )
{
    DWORD *threadArgs = HeapAlloc( GetProcessHeap(), 0, 2*sizeof(DWORD) );
    if (!threadArgs) return INVALID_HANDLE_VALUE;
    threadArgs[0] = (DWORD)start;
    threadArgs[1] = (DWORD)param;

    return CreateThread( sa, stack, THREAD_StartThread16, threadArgs, flags, id );
}


/***********************************************************************
 * ExitThread [KERNEL32.@]  Ends a thread
 *
 * RETURNS
 *    None
 */
void WINAPI ExitThread( DWORD code ) /* [in] Exit code for this thread */
{
    BOOL last;

    SYSDEPS_SanitizeEFlags ();

    SERVER_START_REQ( terminate_thread )
    {
        /* send the exit code to the server */
        req->handle    = GetCurrentThread();
        req->exit_code = code;
        wine_server_call( req );
        last = reply->last;
    }
    SERVER_END_REQ;

    if (last)
    {
        MODULE_DllProcessDetach( TRUE, (LPVOID)1 );
        exit( code );
    }
    else
    {
        MODULE_DllThreadDetach( NULL );
        if (!(NtCurrentTeb()->tibflags & TEBF_WIN32)) TASK_ExitTask();
        SYSDEPS_ExitThread( code );
    }
}

/***********************************************************************
 * OpenThread Retrieves a handle to a thread from its thread id
 *
 * RETURNS
 *    None
 */
HANDLE WINAPI OpenThread( DWORD dwDesiredAccess, BOOL bInheritHandle,
		DWORD dwThreadId )
{
    HANDLE ret = 0;
    SERVER_START_REQ( open_thread )
    {
        req->tid     = (thread_id_t)dwThreadId;
        req->access  = dwDesiredAccess;
        req->inherit = bInheritHandle;
        if (!wine_server_call_err( req )) ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 * SetThreadContext [KERNEL32.@]  Sets context of thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI SetThreadContext( HANDLE handle,           /* [in]  Handle to thread with context */
                              const CONTEXT *context ) /* [in] Address of context structure */
{
    BOOL ret;
    SERVER_START_REQ( set_thread_context )
    {
        req->handle = handle;
        req->flags = context->ContextFlags;
        wine_server_add_data( req, context, sizeof(*context) );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 * GetThreadContext [KERNEL32.@]  Retrieves context of thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetThreadContext( HANDLE handle,     /* [in]  Handle to thread with context */
                              CONTEXT *context ) /* [out] Address of context structure */
{
    BOOL ret;
    SERVER_START_REQ( get_thread_context )
    {
        req->handle = handle;
        req->flags = context->ContextFlags;
        wine_server_add_data( req, context, sizeof(*context) );
        wine_server_set_reply( req, context, sizeof(*context) );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * GetThreadPriority [KERNEL32.@]  Returns priority for thread.
 *
 * RETURNS
 *    Success: Thread's priority level.
 *    Failure: THREAD_PRIORITY_ERROR_RETURN
 */
INT WINAPI GetThreadPriority(
    HANDLE hthread) /* [in] Handle to thread */
{
    INT ret = THREAD_PRIORITY_ERROR_RETURN;
    SERVER_START_REQ( get_thread_info )
    {
        req->handle = hthread;
        req->tid_in = 0;
        if (!wine_server_call_err( req )) ret = reply->priority;
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * SetThreadPriority [KERNEL32.@]  Sets priority for thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI SetThreadPriority(
    HANDLE hthread, /* [in] Handle to thread */
    INT priority)   /* [in] Thread priority level */
{
    BOOL ret;
    SERVER_START_REQ( set_thread_info )
    {
        req->handle   = hthread;
        req->priority = priority;
        req->mask     = SET_THREAD_INFO_PRIORITY;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * GetThreadPriorityBoost [KERNEL32.@]  Returns priority boost for thread.
 *
 * Always reports that priority boost is disabled.
 *
 * RETURNS
 *    Success: TRUE.
 *    Failure: FALSE
 */
BOOL WINAPI GetThreadPriorityBoost(
    HANDLE hthread, /* [in] Handle to thread */
    PBOOL pstate)   /* [out] pointer to var that receives the boost state */
{
    if (pstate) *pstate = FALSE;
    return NO_ERROR;
}


/**********************************************************************
 * SetThreadPriorityBoost [KERNEL32.@]  Sets priority boost for thread.
 *
 * Priority boost is not implemented. Thsi function always returns
 * FALSE and sets last error to ERROR_CALL_NOT_IMPLEMENTED
 *
 * RETURNS
 *    Always returns FALSE to indicate a failure
 */
BOOL WINAPI SetThreadPriorityBoost(
    HANDLE hthread, /* [in] Handle to thread */
    BOOL disable)   /* [in] TRUE to disable priority boost */
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/**********************************************************************
 * SetThreadIdealProcessor [KERNEL32.@]  Per-thread processor affinity
 */
DWORD WINAPI SetThreadIdealProcessor(
    HANDLE hthread,
    DWORD processorId)
{
   FIXME ("(0x%x, %ld): stub\n", hthread, processorId);
   return 0;
}


/**********************************************************************
 *           SetThreadAffinityMask   (KERNEL32.@)
 */
DWORD WINAPI SetThreadAffinityMask( HANDLE hThread, DWORD dwThreadAffinityMask )
{
    DWORD ret;
    SERVER_START_REQ( set_thread_info )
    {
        req->handle   = hThread;
        req->affinity = dwThreadAffinityMask;
        req->mask     = SET_THREAD_INFO_AFFINITY;
        ret = !wine_server_call_err( req );
        /* FIXME: should return previous value */
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * TerminateThread [KERNEL32.@]  Terminates a thread
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI TerminateThread( HANDLE handle,    /* [in] Handle to thread */
                             DWORD exit_code)  /* [in] Exit code for thread */
{
    NTSTATUS status = NtTerminateThread( handle, exit_code );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/**********************************************************************
 *		GetExitCodeThread (KERNEL32.@)
 *
 * Gets termination status of thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetExitCodeThread(
    HANDLE hthread, /* [in]  Handle to thread */
    LPDWORD exitcode) /* [out] Address to receive termination status */
{
    BOOL ret;
    SERVER_START_REQ( get_thread_info )
    {
        req->handle = hthread;
        req->tid_in = 0;
        ret = !wine_server_call_err( req );
        if (ret && exitcode) *exitcode = reply->exit_code;
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * ResumeThread [KERNEL32.@]  Resumes a thread.
 *
 * Decrements a thread's suspend count.  When count is zero, the
 * execution of the thread is resumed.
 *
 * RETURNS
 *    Success: Previous suspend count
 *    Failure: 0xFFFFFFFF
 *    Already running: 0
 */
DWORD WINAPI ResumeThread(
    HANDLE hthread) /* [in] Identifies thread to restart */
{
    DWORD ret = 0xffffffff;
    SERVER_START_REQ( resume_thread )
    {
        req->handle = hthread;
        if (!wine_server_call_err( req )) ret = reply->count;
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * SuspendThread [KERNEL32.@]  Suspends a thread.
 *
 * RETURNS
 *    Success: Previous suspend count
 *    Failure: 0xFFFFFFFF
 */
DWORD WINAPI SuspendThread(
    HANDLE hthread) /* [in] Handle to the thread */
{
    DWORD ret = 0xffffffff;
    SERVER_START_REQ( suspend_thread )
    {
        req->handle = hthread;
        if (!wine_server_call_err( req )) ret = reply->count;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *              QueueUserAPC  (KERNEL32.@)
 */
DWORD WINAPI QueueUserAPC( PAPCFUNC func, HANDLE hthread, ULONG_PTR data )
{
    DWORD ret;
    SERVER_START_REQ( queue_apc )
    {
        req->handle = hthread;
        req->user   = 1;
        req->func   = func;
        req->param  = (void *)data;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * GetThreadTimes [KERNEL32.@]  Obtains timing information.
 *
 * NOTES
 *    What are the fields where these values are stored?
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetThreadTimes(
    HANDLE thread,         /* [in]  Specifies the thread of interest */
    LPFILETIME creationtime, /* [out] When the thread was created */
    LPFILETIME exittime,     /* [out] When the thread was destroyed */
    LPFILETIME kerneltime,   /* [out] Time thread spent in kernel mode */
    LPFILETIME usertime)     /* [out] Time thread spent in user mode */
{
    FIXME("(0x%08x): stub\n",thread);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/**********************************************************************
 * VWin32_BoostThreadGroup [KERNEL.535]
 */
VOID WINAPI VWin32_BoostThreadGroup( DWORD threadId, INT boost )
{
    FIXME("(0x%08lx,%d): stub\n", threadId, boost);
}

/**********************************************************************
 * VWin32_BoostThreadStatic [KERNEL.536]
 */
VOID WINAPI VWin32_BoostThreadStatic( DWORD threadId, INT boost )
{
    FIXME("(0x%08lx,%d): stub\n", threadId, boost);
}


/***********************************************************************
 *           GetThreadLocale    (KERNEL32.@)
 */
LCID WINAPI GetThreadLocale(void)
{
    LCID ret = NtCurrentTeb()->CurrentLocale;
    if (!ret) NtCurrentTeb()->CurrentLocale = ret = GetUserDefaultLCID();
    return ret;
}


/**********************************************************************
 * SetThreadLocale [KERNEL32.@]  Sets the calling threads current locale.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 *
 * FIXME
 *  check if lcid is a valid cp
 */
BOOL WINAPI SetThreadLocale(
    LCID lcid)     /* [in] Locale identifier */
{
    switch (lcid)
    {
      case LOCALE_SYSTEM_DEFAULT:
        lcid = GetSystemDefaultLCID();
	break;
      case LOCALE_USER_DEFAULT:
      case LOCALE_NEUTRAL:
        lcid = GetUserDefaultLCID();
	break;
    }
    NtCurrentTeb()->CurrentLocale = lcid;
    return TRUE;
}


/***********************************************************************
 * GetCurrentThread [KERNEL32.@]  Gets pseudohandle for current thread
 *
 * RETURNS
 *    Pseudohandle for the current thread
 */
#undef GetCurrentThread
HANDLE WINAPI GetCurrentThread(void)
{
    return 0xfffffffe;
}


/***********************************************************************
 * ProcessIdToSessionId   (KERNEL32.@)
 * This function is available on Terminal Server 4SP4 and Windows 2000
 */
BOOL WINAPI ProcessIdToSessionId( DWORD procid, DWORD *sessionid_ptr )
{
	/* According to MSDN, if the calling process is not in a terminal
	 * services environment, then the sessionid returned is zero.
	 */
	*sessionid_ptr = 0;
	return TRUE;
}

/***********************************************************************
 * SetThreadExecutionState (KERNEL32.@)
 *
 * Informs the system that activity is taking place for
 * power management purposes.
 */
EXECUTION_STATE WINAPI SetThreadExecutionState(EXECUTION_STATE flags)
{
    static EXECUTION_STATE current =
	    ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED|ES_USER_PRESENT;
    EXECUTION_STATE old = current;

    if (!(current & ES_CONTINUOUS) || (flags & ES_CONTINUOUS))
        current = flags;
    FIXME("(0x%lx): stub, harmless (power management).\n", flags);
    return old;
}


#ifdef __i386__

/***********************************************************************
 *		SetLastError (KERNEL.147)
 *		SetLastError (KERNEL32.@)
 */
/* void WINAPI SetLastError( DWORD error ); */
__ASM_GLOBAL_FUNC( SetLastError,
                   "movl 4(%esp),%eax\n\t"
                   ".byte 0x64\n\t"
                   "movl %eax,0x60\n\t"
                   "ret $4" );

/***********************************************************************
 *		GetLastError (KERNEL.148)
 *		GetLastError (KERNEL32.@)
 */
/* DWORD WINAPI GetLastError(void); */
__ASM_GLOBAL_FUNC( GetLastError, ".byte 0x64\n\tmovl 0x60,%eax\n\tret" );

/***********************************************************************
 *		GetCurrentProcessId (KERNEL.471)
 *		GetCurrentProcessId (KERNEL32.@)
 */
/* DWORD WINAPI GetCurrentProcessId(void) */
__ASM_GLOBAL_FUNC( GetCurrentProcessId, ".byte 0x64\n\tmovl 0x20,%eax\n\tret" );

/***********************************************************************
 *		GetCurrentThreadId (KERNEL.462)
 *		GetCurrentThreadId (KERNEL32.@)
 */
/* DWORD WINAPI GetCurrentThreadId(void) */
__ASM_GLOBAL_FUNC( GetCurrentThreadId, ".byte 0x64\n\tmovl 0x24,%eax\n\tret" );

#else  /* __i386__ */

/**********************************************************************
 *		SetLastError (KERNEL.147)
 *		SetLastError (KERNEL32.@)
 *
 * Sets the last-error code.
 */
void WINAPI SetLastError( DWORD error ) /* [in] Per-thread error code */
{
    NtCurrentTeb()->last_error = error;
}

/**********************************************************************
 *		GetLastError (KERNEL.148)
 *              GetLastError (KERNEL32.@)
 *
 * Returns last-error code.
 */
DWORD WINAPI GetLastError(void)
{
    return NtCurrentTeb()->last_error;
}

/***********************************************************************
 *		GetCurrentProcessId (KERNEL.471)
 *		GetCurrentProcessId (KERNEL32.@)
 *
 * Returns process identifier.
 */
DWORD WINAPI GetCurrentProcessId(void)
{
    return (DWORD)NtCurrentTeb()->pid;
}

/***********************************************************************
 *		GetCurrentThreadId (KERNEL.462)
 *		GetCurrentThreadId (KERNEL32.@)
 *
 * Returns thread identifier.
 */
DWORD WINAPI GetCurrentThreadId(void)
{
    return (DWORD)NtCurrentTeb()->tid;
}

#endif  /* __i386__ */
