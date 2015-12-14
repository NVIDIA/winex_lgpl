/*
 * NT exception handling routines
 *
 * Copyright 1999 Turchanov Sergey
 * Copyright 1999 Alexandre Julliard
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#include <assert.h>
#include <signal.h>

#include "wine/hardware.h"

#include "winnt.h"
#include "winternl.h"
#include "global.h"
#include "wine/exception.h"
#include "stackframe.h"
#include "miscemu.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "wine/unhandledException.h"
#include "msvcrt/excpt.h"

#include "msvcrt/excpt.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

/* Exception record for handling exceptions happening inside exception handlers */
typedef struct
{
    EXCEPTION_FRAME frame;
    EXCEPTION_FRAME *prevFrame;
} EXC_NESTED_FRAME;

#ifdef __i386__
# define GET_IP(context) ((LPVOID)(context)->Eip)
#endif
#ifdef __sparc__
# define GET_IP(context) ((LPVOID)(context)->pc)
#endif
#ifndef GET_IP
# error You must define GET_IP for this CPU
#endif

extern void SIGNAL_Unblock(void);

void WINAPI EXC_RtlRaiseException( PEXCEPTION_RECORD, PCONTEXT );
void WINAPI EXC_RtlUnwind( PEXCEPTION_FRAME, LPVOID,
                           PEXCEPTION_RECORD, DWORD, PCONTEXT );
void WINAPI EXC_NtRaiseException( PEXCEPTION_RECORD, PCONTEXT,
                                  BOOL, PCONTEXT );

/*******************************************************************
 *         EXC_RaiseHandler
 *
 * Handler for exceptions happening inside a handler.
 */
static DWORD EXC_RaiseHandler( EXCEPTION_RECORD *rec, EXCEPTION_FRAME *frame,
                               CONTEXT *context, EXCEPTION_FRAME **dispatcher )
{
    if (rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND))
        return ExceptionContinueSearch;
    /* We shouldn't get here so we store faulty frame in dispatcher */
    *dispatcher = ((EXC_NESTED_FRAME*)frame)->prevFrame;
    return ExceptionNestedException;
}


/*******************************************************************
 *         EXC_UnwindHandler
 *
 * Handler for exceptions happening inside an unwind handler.
 */
static DWORD EXC_UnwindHandler( EXCEPTION_RECORD *rec, EXCEPTION_FRAME *frame,
                                CONTEXT *context, EXCEPTION_FRAME **dispatcher )
{
    if (!(rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND)))
        return ExceptionContinueSearch;
    /* We shouldn't get here so we store faulty frame in dispatcher */
    *dispatcher = ((EXC_NESTED_FRAME*)frame)->prevFrame;
    return ExceptionCollidedUnwind;
}


/*******************************************************************
 *         EXC_CallHandler
 *
 * Call an exception handler, setting up an exception frame to catch exceptions
 * happening during the handler execution.
 * Please do not change the first 4 parameters order in any way - some exceptions handlers
 * rely on Base Pointer (EBP) to have a fixed position related to the exception frame
 */
static DWORD EXC_CallHandler( EXCEPTION_RECORD *record, EXCEPTION_FRAME *frame,
                              CONTEXT *context, EXCEPTION_FRAME **dispatcher,
                              PEXCEPTION_HANDLER handler, PEXCEPTION_HANDLER nested_handler)
{
    EXC_NESTED_FRAME newframe;
    DWORD ret;

    newframe.frame.Handler = nested_handler;
    newframe.prevFrame     = frame;
    __wine_push_frame( &newframe.frame );
    TRACE( "calling handler at %p code=%lx flags=%lx\n",
           handler, record->ExceptionCode, record->ExceptionFlags );
    ret = handler( record, frame, context, dispatcher );
    TRACE( "handler returned %lx\n", ret );
    __wine_pop_frame( &newframe.frame );
    return ret;
}


/**********************************************************************
 *           send_debug_event
 *
 * Send an EXCEPTION_DEBUG_EVENT event to the debugger.
 */
static int send_debug_event( EXCEPTION_RECORD *rec, int first_chance, CONTEXT *context )
{
    int ret;
    HANDLE handle = 0;

    SERVER_START_REQ( queue_exception_event )
    {
        req->first   = first_chance;
        wine_server_add_data( req, context, sizeof(*context) );
        wine_server_add_data( req, rec, sizeof(*rec) );
        if (!wine_server_call( req )) handle = reply->handle;
    }
    SERVER_END_REQ;
    if (!handle) return 0;  /* no debugger present or other error */

    /* No need to wait on the handle since the process gets suspended
     * once the event is passed to the debugger, so when we get back
     * here the event has been continued already.
     */
    SERVER_START_REQ( get_exception_status )
    {
        req->handle = handle;
        wine_server_set_reply( req, context, sizeof(*context) );
        wine_server_call( req );
        ret = reply->status;
    }
    SERVER_END_REQ;
    NtClose( handle );
    return ret;
}


/*******************************************************************
 *         EXC_DefaultHandling
 *
 * Default handling for exceptions. Called when we didn't find a suitable handler.
 */
static void EXC_DefaultHandling( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    if (send_debug_event( rec, FALSE, context ) == DBG_CONTINUE) return;  /* continue execution */

    if (rec->ExceptionFlags & EH_STACK_INVALID)
        ERR("Exception frame is not in stack limits => unable to dispatch exception.\n");
    else if (rec->ExceptionCode == EXCEPTION_NONCONTINUABLE_EXCEPTION)
        ERR("Process attempted to continue execution after noncontinuable exception.\n");
    else
        ERR("Unhandled exception code %lx flags %lx addr %p\n",
            rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress );
    NtTerminateProcess( NtCurrentProcess(), 1 );
}


/***********************************************************************
 *		RtlRaiseException (NTDLL.@)
 */
DEFINE_REGS_ENTRYPOINT_1( RtlRaiseException, EXC_RtlRaiseException, EXCEPTION_RECORD * );
void WINAPI EXC_RtlRaiseException( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    PEXCEPTION_FRAME    frame, dispatch, nested_frame;
    EXCEPTION_RECORD    newrec;
    EXCEPTION_POINTERS  epointers;
    DWORD res, c;

    if (((unsigned long)(rec->ExceptionAddress) & 0xffff0000) == 0xdead0000)
        ERR("possibly COM stub exception at %p\n", rec->ExceptionAddress);
    TRACE( "code=%lx flags=%lx addr=%p\n", rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress );
    for (c=0; c<rec->NumberParameters; c++) TRACE(" info[%ld]=%08lx\n", c, rec->ExceptionInformation[c]);
    if (rec->ExceptionCode == EXCEPTION_WINE_STUB) TRACE(" stub=%s\n", (char*)rec->ExceptionInformation[1]);
    /* TRACE( " backtrace %s\n", debugstack(10, context) ); */

    if (send_debug_event( rec, TRUE, context ) == DBG_CONTINUE) return;  /* continue execution */

    SIGNAL_Unblock(); /* we may be in a signal handler, and exception handlers may jump out */

    frame = NtCurrentTeb()->except;
    nested_frame = NULL;
    while (frame != (PEXCEPTION_FRAME)0xFFFFFFFF)
    {
        /* Check frame address */
        if (((void*)frame < NtCurrentTeb()->stack_low) ||
            ((void*)(frame+1) > NtCurrentTeb()->stack_top) ||
            (int)frame & 3)
        {
            WARN ("Marking as invalid. Frame: %p, stack_low: %p, Frame+1: %p, stack_top: %p, frame&3: %d\n",
                  (void *)frame, NtCurrentTeb ()->stack_low, (void *)(frame+1),
                  NtCurrentTeb ()->stack_top, (int)frame & 3);
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        /* Call handler */
        res = EXC_CallHandler( rec, frame, context, &dispatch, frame->Handler, EXC_RaiseHandler );
        if (frame == nested_frame)
        {
            /* no longer nested */
            nested_frame = NULL;
            rec->ExceptionFlags &= ~EH_NESTED_CALL;
        }

        switch(res)
        {
        case ExceptionContinueExecution:
            if (!(rec->ExceptionFlags & EH_NONCONTINUABLE)) return;
            newrec.ExceptionCode    = STATUS_NONCONTINUABLE_EXCEPTION;
            newrec.ExceptionFlags   = EH_NONCONTINUABLE;
            newrec.ExceptionRecord  = rec;
            newrec.NumberParameters = 0;
            RtlRaiseException( &newrec );  /* never returns */
            break;
        case ExceptionContinueSearch:
            break;
        case ExceptionNestedException:
            if (nested_frame < dispatch) nested_frame = dispatch;
            rec->ExceptionFlags |= EH_NESTED_CALL;
            break;
        case ExceptionCollidedUnwind:
            /* Check for having gotten into an infinite exception loop due to
             a busted exception frame somewhere */
            if (rec->ExceptionCode == STATUS_INVALID_DISPOSITION)
                EXC_DefaultHandling (rec, context);
       default:
            newrec.ExceptionCode    = STATUS_INVALID_DISPOSITION;
            newrec.ExceptionFlags   = EH_NONCONTINUABLE;
            newrec.ExceptionRecord  = rec;
            newrec.NumberParameters = 0;


            /* dump the crash report here since the next RtlRaiseException()
               call will never return and the exception info from this point on will
               be clobbered.  */
            epointers.ContextRecord = context;
            epointers.ExceptionRecord = rec;

            if (!dumpExceptionInfo(&epointers, NTDLL_CRASHREPORTFILENAME, NTDLL_MINIDUMPFILENAME))
                ERR("ERROR-> could not dump the crash report\n");

            RtlRaiseException( &newrec );  /* never returns */
            break;
        }
        frame = frame->Prev;
    }
    EXC_DefaultHandling( rec, context );
}


/*******************************************************************
 *		RtlUnwind (NTDLL.@)
 */
DEFINE_REGS_ENTRYPOINT_4( RtlUnwind, EXC_RtlUnwind,
                          PVOID, PVOID, PEXCEPTION_RECORD, PVOID );
void WINAPI EXC_RtlUnwind( PEXCEPTION_FRAME pEndFrame, LPVOID unusedEip,
                           PEXCEPTION_RECORD pRecord, DWORD returnEax,
                           CONTEXT *context )
{
    EXCEPTION_RECORD record, newrec;
    PEXCEPTION_FRAME frame, dispatch;

#ifdef __i386__
    context->Eax = returnEax;
#endif

    /* build an exception record, if we do not have one */
    if (!pRecord)
    {
        record.ExceptionCode    = STATUS_UNWIND;
        record.ExceptionFlags   = 0;
        record.ExceptionRecord  = NULL;
        record.ExceptionAddress = GET_IP(context);
        record.NumberParameters = 0;
        pRecord = &record;
    }

    pRecord->ExceptionFlags |= EH_UNWINDING | (pEndFrame ? 0 : EH_EXIT_UNWIND);

    TRACE( "code=%lx flags=%lx\n", pRecord->ExceptionCode, pRecord->ExceptionFlags );

    /* get chain of exception frames */
    frame = NtCurrentTeb()->except;
    while ((frame != (PEXCEPTION_FRAME)0xffffffff) && (frame != pEndFrame))
    {
        /* Check frame address */
        if (pEndFrame && (frame > pEndFrame))
        {
            newrec.ExceptionCode    = STATUS_INVALID_UNWIND_TARGET;
            newrec.ExceptionFlags   = EH_NONCONTINUABLE;
            newrec.ExceptionRecord  = pRecord;
            newrec.NumberParameters = 0;
            RtlRaiseException( &newrec );  /* never returns */
        }
        if (((void*)frame < NtCurrentTeb()->stack_low) ||
            ((void*)(frame+1) > NtCurrentTeb()->stack_top) ||
            (int)frame & 3)
        {
            newrec.ExceptionCode    = STATUS_BAD_STACK;
            newrec.ExceptionFlags   = EH_NONCONTINUABLE;
            newrec.ExceptionRecord  = pRecord;
            newrec.NumberParameters = 0;
            RtlRaiseException( &newrec );  /* never returns */
        }

        /* Call handler */
        switch(EXC_CallHandler( pRecord, frame, context, &dispatch,
                                frame->Handler, EXC_UnwindHandler ))
        {
        case ExceptionContinueSearch:
            break;
        case ExceptionCollidedUnwind:
            frame = dispatch;
            break;
        default:
            newrec.ExceptionCode    = STATUS_INVALID_DISPOSITION;
            newrec.ExceptionFlags   = EH_NONCONTINUABLE;
            newrec.ExceptionRecord  = pRecord;
            newrec.NumberParameters = 0;
            RtlRaiseException( &newrec );  /* never returns */
            break;
        }
        frame = __wine_pop_frame( frame );
    }
}


/*******************************************************************
 *		NtRaiseException (NTDLL.@)
 */
DEFINE_REGS_ENTRYPOINT_3( NtRaiseException, EXC_NtRaiseException,
                          EXCEPTION_RECORD *, CONTEXT *, BOOL );
void WINAPI EXC_NtRaiseException( EXCEPTION_RECORD *rec, CONTEXT *ctx,
                                  BOOL first, CONTEXT *context )
{
    EXC_RtlRaiseException( rec, ctx );
    *context = *ctx;
}


/***********************************************************************
 *            RtlRaiseStatus  (NTDLL.@)
 *
 * Raise an exception with ExceptionCode = status
 */
void WINAPI RtlRaiseStatus( NTSTATUS status )
{
    EXCEPTION_RECORD ExceptionRec;

    ExceptionRec.ExceptionCode    = status;
    ExceptionRec.ExceptionFlags   = EH_NONCONTINUABLE;
    ExceptionRec.ExceptionRecord  = NULL;
    ExceptionRec.NumberParameters = 0;
    RtlRaiseException( &ExceptionRec );
}


/*************************************************************
 *            __wine_exception_handler (NTDLL.@)
 *
 * Exception handler for exception blocks declared in Wine code.
 */
DWORD __wine_exception_handler( EXCEPTION_RECORD *record, EXCEPTION_FRAME *frame,
                                CONTEXT *context, LPVOID pdispatcher )
{
    __WINE_FRAME *wine_frame = (__WINE_FRAME *)frame;

    if (record->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND | EH_NESTED_CALL))
        return ExceptionContinueSearch;
    if (wine_frame->u.filter)
    {
        EXCEPTION_POINTERS ptrs;
        ptrs.ExceptionRecord = record;
        ptrs.ContextRecord = context;
        switch(wine_frame->u.filter( &ptrs ))
        {
        case EXCEPTION_CONTINUE_SEARCH:
            return ExceptionContinueSearch;
        case EXCEPTION_CONTINUE_EXECUTION:
            return ExceptionContinueExecution;
        case EXCEPTION_EXECUTE_HANDLER:
            break;
        default:
            MESSAGE( "Invalid return value from exception filter\n" );
            assert( FALSE );
        }
    }
    /* hack to make GetExceptionCode() work in handler */
    wine_frame->ExceptionCode   = record->ExceptionCode;
    wine_frame->ExceptionRecord = wine_frame;

    RtlUnwind( frame, 0, record, 0 );
    __wine_pop_frame( frame );
    longjmp( wine_frame->jmp, 1 );
}


/*************************************************************
 *            __wine_finally_handler (NTDLL.@)
 *
 * Exception handler for try/finally blocks declared in Wine code.
 */
DWORD __wine_finally_handler( EXCEPTION_RECORD *record, EXCEPTION_FRAME *frame,
                              CONTEXT *context, LPVOID pdispatcher )
{
    __WINE_FRAME *wine_frame = (__WINE_FRAME *)frame;

    if (!(record->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND)))
        return ExceptionContinueSearch;
    wine_frame->u.finally_func( FALSE );
    return ExceptionContinueSearch;
}
