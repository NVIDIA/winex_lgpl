/*
 * Win32 exception functions
 *
 * Copyright (c) 1996 Onno Hovers, (onno@stack.urc.tue.nl)
 * Copyright (c) 1999 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 *
 * Notes:
 *  What really happens behind the scenes of those new
 *  __try{...}__except(..){....}  and
 *  __try{...}__finally{...}
 *  statements is simply not documented by Microsoft. There could be different
 *  reasons for this:
 *  One reason could be that they try to hide the fact that exception
 *  handling in Win32 looks almost the same as in OS/2 2.x.
 *  Another reason could be that Microsoft does not want others to write
 *  binary compatible implementations of the Win32 API (like us).
 *
 *  Whatever the reason, THIS SUCKS!! Ensuring portability or future
 *  compatibility may be valid reasons to keep some things undocumented.
 *  But exception handling is so basic to Win32 that it should be
 *  documented!
 *
 */

#include <stdio.h>
#include "windef.h"
#include "winerror.h"
#include "winternl.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/exception.h"
#include "wine/thread.h"
#include "wine/stackframe.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "msvcrt/excpt.h"
#include "config.h"
#include "wine/hardware.h"
#include "wine/unhandledException.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

static PTOP_LEVEL_EXCEPTION_FILTER top_filter;

typedef INT (WINAPI *MessageBoxA_funcptr)(HWND,LPCSTR,LPCSTR,UINT);
typedef INT (WINAPI *MessageBoxW_funcptr)(HWND,LPCWSTR,LPCWSTR,UINT);

/*******************************************************************
 *         RaiseException  (KERNEL32.@)
 */
void WINAPI RaiseException( DWORD code, DWORD flags, DWORD nbargs, const LPDWORD args )
{
    EXCEPTION_RECORD record;

    /* Ignore "API" for setting the name of a thread */
    if (code == 0x406d1388)
    {
       FIXME ("Ignoring thread name setting\n");
       return;
    }

    /* Compose an exception record */

    record.ExceptionCode    = code;
    record.ExceptionFlags   = flags & EH_NONCONTINUABLE;
    record.ExceptionRecord  = NULL;
    record.ExceptionAddress = RaiseException;
    if (nbargs && args)
    {
        if (nbargs > EXCEPTION_MAXIMUM_PARAMETERS) nbargs = EXCEPTION_MAXIMUM_PARAMETERS;
        record.NumberParameters = nbargs;
        memcpy( record.ExceptionInformation, args, nbargs * sizeof(*args) );
    }
    else record.NumberParameters = 0;

    RtlRaiseException( &record );
}


/*******************************************************************
 *         format_exception_msg
 */
static int format_exception_msg( const EXCEPTION_POINTERS *ptr, char *buffer, int size )
{
    const EXCEPTION_RECORD *rec = ptr->ExceptionRecord;
    int len,len2;

    switch(rec->ExceptionCode)
    {
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        len = snprintf( buffer, size, "Unhandled division by zero" );
        break;
    case EXCEPTION_INT_OVERFLOW:
        len = snprintf( buffer, size, "Unhandled overflow" );
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        len = snprintf( buffer, size, "Unhandled array bounds" );
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        len = snprintf( buffer, size, "Unhandled illegal instruction" );
        break;
    case EXCEPTION_STACK_OVERFLOW:
        len = snprintf( buffer, size, "Unhandled stack overflow" );
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        len = snprintf( buffer, size, "Unhandled privileged instruction" );
        break;
    case EXCEPTION_ACCESS_VIOLATION:
        if (rec->NumberParameters == 2)
            len = snprintf( buffer, size, "Unhandled page fault on %s access to 0x%08lx",
                     rec->ExceptionInformation[0] ? "write" : "read",
                     rec->ExceptionInformation[1]);
        else
            len = snprintf( buffer, size, "Unhandled page fault");
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        len = snprintf( buffer, size, "Unhandled alignment" );
        break;
    case CONTROL_C_EXIT:
        len = snprintf( buffer, size, "Unhandled ^C");
        break;
    case EXCEPTION_CRITICAL_SECTION_WAIT:
        len = snprintf( buffer, size, "Critical section %08lx wait failed",
                 rec->ExceptionInformation[0]);
        break;
    case EXCEPTION_WINE_STUB:
        len = snprintf( buffer, size, "Unimplemented function %s.%s called",
                 (char *)rec->ExceptionInformation[0], (char *)rec->ExceptionInformation[1] );
        break;
    case EXCEPTION_WINE_ASSERTION:
        len = snprintf( buffer, size, "Assertion failed" );
        break;
    case EXCEPTION_VM86_INTx:
        len = snprintf( buffer, size, "Unhandled interrupt %02lx in vm86 mode",
                 rec->ExceptionInformation[0]);
        break;
    case EXCEPTION_VM86_STI:
        len = snprintf( buffer, size, "Unhandled sti in vm86 mode");
        break;
    case EXCEPTION_VM86_PICRETURN:
        len = snprintf( buffer, size, "Unhandled PIC return in vm86 mode");
        break;
    default:
        len = snprintf( buffer, size, "Unhandled exception 0x%08lx", rec->ExceptionCode);
        break;
    }
    if ((len<0) || (len>=size))
        return -1;
#ifdef __i386__
    if (ptr->ContextRecord->SegCs != __get_cs())
        len2 = snprintf(buffer+len, size-len,
                        " at address 0x%04lx:0x%08lx.\nDo you wish to debug it ?",
                        ptr->ContextRecord->SegCs,
                        (DWORD)ptr->ExceptionRecord->ExceptionAddress);
    else
#endif
        len2 = snprintf(buffer+len, size-len,
                        " at address 0x%08lx.\nDo you wish to debug it ?",
                        (DWORD)ptr->ExceptionRecord->ExceptionAddress);
    if ((len2<0) || (len>=size-len))
        return -1;
    return len+len2;
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
        if (!wine_server_call(req)) handle = reply->handle;
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

/******************************************************************
 *		start_debugger
 *
 * Does the effective debugger startup according to 'format'
 */
static BOOL	start_debugger(PEXCEPTION_POINTERS epointers, HANDLE hEvent)
{
    HKEY		hDbgConf;
    DWORD		bAuto = FALSE;
    PROCESS_INFORMATION	info;
    STARTUPINFOA	startup;
    char*		cmdline = NULL;
    char*		format = NULL;
    DWORD		format_size;
    BOOL		ret = FALSE;
    UINT		code;

    MESSAGE("wine: Unhandled exception, starting debugger...\n");

    if (!RegOpenKeyA(HKEY_LOCAL_MACHINE,
		     "Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug", &hDbgConf)) {
       DWORD 	type;
       DWORD 	count;

       format_size = 0;
       if (!RegQueryValueExA(hDbgConf, "Debugger", 0, &type, NULL, &format_size)) {
           format = HeapAlloc(GetProcessHeap(), 0, format_size);
           RegQueryValueExA(hDbgConf, "Debugger", 0, &type, format, &format_size);
           if (type==REG_EXPAND_SZ) {
               char* tmp;

               /* Expand environment variable references */
               format_size=ExpandEnvironmentStringsA(format,NULL,0);
               tmp=HeapAlloc(GetProcessHeap(), 0, format_size);
               ExpandEnvironmentStringsA(format,tmp,format_size);
               HeapFree(GetProcessHeap(), 0, format);
               format=tmp;
           }
       }

       count = sizeof(bAuto);
       if (RegQueryValueExA(hDbgConf, "Auto", 0, &type, (char*)&bAuto, &count))
           bAuto = TRUE;
       else if (type == REG_SZ)
       {
           char autostr[10];
           count = sizeof(autostr);
           if (!RegQueryValueExA(hDbgConf, "Auto", 0, &type, autostr, &count))
               bAuto = atoi(autostr);
       }
       RegCloseKey(hDbgConf);
    } else {
       /* try a default setup... */
       const char default_cmdline[] = "winedbg --debugmsg -all -- --auto %ld %ld";
       format = HeapAlloc( GetProcessHeap(), 0, strlen( default_cmdline ) + 1 );
       strcpy( format, default_cmdline );
    }

    if (!bAuto)
    {
	HMODULE			mod = GetModuleHandleA( "user32.dll" );
	MessageBoxA_funcptr	pMessageBoxA = NULL;

	if (mod) pMessageBoxA = (MessageBoxA_funcptr)GetProcAddress( mod, "MessageBoxA" );
	if (pMessageBoxA)
	{
	    char buffer[256];
	    format_exception_msg( epointers, buffer, sizeof(buffer) );
	    if (pMessageBoxA( 0, buffer, "Exception raised", MB_YESNO | MB_ICONHAND ) == IDNO)
	    {
		TRACE("Killing process\n");
		goto EXIT;
	    }
	}
    }

    if (format) {
        TRACE("Starting debugger (fmt=%s)\n", format);
        cmdline=HeapAlloc(GetProcessHeap(), 0, format_size+2*20);
        sprintf(cmdline, format, GetCurrentProcessId(), hEvent);
        memset(&startup, 0, sizeof(startup));
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_SHOWNORMAL;
        if (CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info)) {
            /* wait for debugger to come up... */
            WaitForSingleObject(hEvent, INFINITE);
            ret = TRUE;
            goto EXIT;
        }
    } else {
        cmdline = NULL;
    }


    if (!dumpExceptionInfo(epointers, WIN32_CRASHREPORTFILENAME, WIN32_MINIDUMPFILENAME))
        ERR("ERROR-> could not dump the crash report\n");

    else{
        ERR("Couldn't start debugger (%s) (%ld)\n"
	        "Read the Wine Developers Guide on how to set up winedbg or another debugger\n",
	        debugstr_a(cmdline), GetLastError());
    }

    TerminateProcess (GetCurrentProcess (), &code);

EXIT:
    if (cmdline)
        HeapFree(GetProcessHeap(), 0, cmdline);
    if (format)
        HeapFree(GetProcessHeap(), 0, format);
    return ret;
}

/******************************************************************
 *		start_debugger_atomic
 *
 * starts the debugger in an atomic way:
 *	- either the debugger is not started and it is started
 *	- or the debugger has already been started by another thread
 *	- or the debugger couldn't be started
 *
 * returns TRUE for the two first conditions, FALSE for the last
 */
static	int	start_debugger_atomic(PEXCEPTION_POINTERS epointers)
{
    static HANDLE	hRunOnce /* = 0 */;

    if (hRunOnce == 0)
    {
	OBJECT_ATTRIBUTES	attr;
	HANDLE			hEvent;

	attr.Length                   = sizeof(attr);
	attr.RootDirectory            = 0;
	attr.Attributes               = OBJ_INHERIT;
	attr.ObjectName               = NULL;
	attr.SecurityDescriptor       = NULL;
	attr.SecurityQualityOfService = NULL;

	/* ask for manual reset, so that once the debugger is started,
	 * every thread will know it */
	NtCreateEvent( &hEvent, EVENT_ALL_ACCESS, &attr, TRUE, FALSE );
	if (InterlockedCompareExchange( (LPLONG)&hRunOnce, hEvent, 0 ) == 0)
	{
	    /* ok, our event has been set... we're the winning thread */
	    BOOL	ret = start_debugger( epointers, hRunOnce );
	    DWORD	tmp;

	    if (!ret)
	    {
		/* so that the other threads won't be stuck */
		NtSetEvent( hRunOnce, &tmp );
	    }
	    return ret;
	}

	/* someone beat us here... */
	CloseHandle( hEvent );
    }

    /* and wait for the winner to have actually created the debugger */
    WaitForSingleObject( hRunOnce, INFINITE );
    /* in fact, here, we only know that someone has tried to start the debugger,
     * we'll know by reposting the exception if it has actually attached
     * to the current process */
    return TRUE;
}

#include <signal.h>
#include <unistd.h>
/*******************************************************************
 *         UnhandledExceptionFilter   (KERNEL32.@)
 */
DWORD WINAPI UnhandledExceptionFilter(PEXCEPTION_POINTERS epointers)
{
    int 		status;
    int			loop = 0;

    for (loop = 0; loop <= 1; loop++)
    {
	/* send a last chance event to the debugger */
	status = send_debug_event( epointers->ExceptionRecord, FALSE, epointers->ContextRecord );
	switch (status)
	{
	case DBG_CONTINUE:
	    return EXCEPTION_CONTINUE_EXECUTION;
	case DBG_EXCEPTION_NOT_HANDLED:
	    TerminateProcess( GetCurrentProcess(), epointers->ExceptionRecord->ExceptionCode );
	    break; /* not reached */
	case 0: /* no debugger is present */
	    if (epointers->ExceptionRecord->ExceptionCode == CONTROL_C_EXIT)
	    {
		/* do not launch the debugger on ^C, simply terminate the process */
		TerminateProcess( GetCurrentProcess(), 1 );
	    }
	    /* second try, the debugger isn't present... */
	    if (loop == 1) return EXCEPTION_EXECUTE_HANDLER;
	    break;
	default:
	    FIXME("Unsupported yet debug continue value %d (please report)\n", status);
	    return EXCEPTION_EXECUTE_HANDLER;
	}

	/* should only be there when loop == 0 */

	if (top_filter)
	{
	    DWORD ret = top_filter( epointers );

        
        /* the unhandled exception filter told us to execute the handler => this essentially
             means it couldn't (or didn't want to) handle the exception and that we should 
             kill the entire process. */
        if (ret == EXCEPTION_EXECUTE_HANDLER){
            TerminateProcess(GetCurrentProcess(), epointers->ExceptionRecord->ExceptionCode);

            return ret; /* not reached */
        }

	    if (ret != EXCEPTION_CONTINUE_SEARCH) return ret;
	}

	/* FIXME: Should check the current error mode */

	if (!start_debugger_atomic( epointers ))
	    return EXCEPTION_EXECUTE_HANDLER;
	/* now that we should have a debugger attached, try to resend event */
    }

    return EXCEPTION_EXECUTE_HANDLER;
}


/***********************************************************************
 *            SetUnhandledExceptionFilter   (KERNEL32.@)
 */
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetUnhandledExceptionFilter(
                                          LPTOP_LEVEL_EXCEPTION_FILTER filter )
{
    LPTOP_LEVEL_EXCEPTION_FILTER old = top_filter;
    top_filter = filter;
    return old;
}


/**************************************************************************
 *           FatalAppExitA   (KERNEL32.@)
 */
void WINAPI FatalAppExitA( UINT action, LPCSTR str )
{
    HMODULE mod = GetModuleHandleA( "user32.dll" );
    MessageBoxA_funcptr pMessageBoxA = NULL;

    WARN("AppExit\n");

    if (mod) pMessageBoxA = (MessageBoxA_funcptr)GetProcAddress( mod, "MessageBoxA" );
    if (pMessageBoxA) pMessageBoxA( 0, str, NULL, MB_SYSTEMMODAL | MB_OK );
    else ERR( "%s\n", debugstr_a(str) );
    ExitProcess(0);
}


/**************************************************************************
 *           FatalAppExitW   (KERNEL32.@)
 */
void WINAPI FatalAppExitW( UINT action, LPCWSTR str )
{
    HMODULE mod = GetModuleHandleA( "user32.dll" );
    MessageBoxW_funcptr pMessageBoxW = NULL;

    WARN("AppExit\n");

    if (mod) pMessageBoxW = (MessageBoxW_funcptr)GetProcAddress( mod, "MessageBoxW" );
    if (pMessageBoxW) pMessageBoxW( 0, str, NULL, MB_SYSTEMMODAL | MB_OK );
    else ERR( "%s\n", debugstr_w(str) );
    ExitProcess(0);
}
