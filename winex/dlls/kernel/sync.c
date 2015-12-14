/*
 * Kernel synchronization objects
 *
 * Copyright 1998 Alexandre Julliard
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#include <stdarg.h>
#include <stdio.h>

#include "winbase.h"
#include "winerror.h"
#include "winnls.h"

#include "wine/server.h"
#include "wine/unicode.h"

#include "wine/debug.h"

#include "wine/file.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32);

/*
 * Events
 */


/***********************************************************************
 *           CreateEventA    (KERNEL32.@)
 */
HANDLE WINAPI CreateEventA( SECURITY_ATTRIBUTES *sa, BOOL manual_reset,
                            BOOL initial_state, LPCSTR name )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return CreateEventW( sa, manual_reset, initial_state, NULL );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return CreateEventW( sa, manual_reset, initial_state, buffer );
}


/***********************************************************************
 *           CreateEventW    (KERNEL32.@)
 */
HANDLE WINAPI CreateEventW( SECURITY_ATTRIBUTES *sa, BOOL manual_reset,
                            BOOL initial_state, LPCWSTR name )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    /* one buggy program needs this
     * ("Van Dale Groot woordenboek der Nederlandse taal")
     */
    if (sa && IsBadReadPtr(sa,sizeof(SECURITY_ATTRIBUTES)))
    {
        ERR("Bad security attributes pointer %p\n",sa);
        SetLastError( ERROR_INVALID_PARAMETER);
        return 0;
    }
    SERVER_START_REQ( create_event )
    {
        req->manual_reset = manual_reset;
        req->initial_state = initial_state;
        req->inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           CreateW32Event    (KERNEL.457)
 */
HANDLE WINAPI WIN16_CreateEvent( BOOL manual_reset, BOOL initial_state )
{
    return CreateEventA( NULL, manual_reset, initial_state, NULL );
}


/***********************************************************************
 *           OpenEventA    (KERNEL32.@)
 */
HANDLE WINAPI OpenEventA( DWORD access, BOOL inherit, LPCSTR name )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return OpenEventW( access, inherit, NULL );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return OpenEventW( access, inherit, buffer );
}


/***********************************************************************
 *           OpenEventW    (KERNEL32.@)
 */
HANDLE WINAPI OpenEventW( DWORD access, BOOL inherit, LPCWSTR name )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( open_event )
    {
        req->access  = access;
        req->inherit = inherit;
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           EVENT_Operation
 *
 * Execute an event operation (set,reset,pulse).
 */
static BOOL EVENT_Operation( HANDLE handle, enum event_op op )
{
    BOOL ret;
    SERVER_START_REQ( event_op )
    {
        req->handle = handle;
        req->op     = op;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           PulseEvent    (KERNEL32.@)
 */
BOOL WINAPI PulseEvent( HANDLE handle )
{
    return EVENT_Operation( handle, PULSE_EVENT );
}


/***********************************************************************
 *           SetW32Event (KERNEL.458)
 *           SetEvent    (KERNEL32.@)
 */
BOOL WINAPI SetEvent( HANDLE handle )
{
    return EVENT_Operation( handle, SET_EVENT );
}


/***********************************************************************
 *           ResetW32Event (KERNEL.459)
 *           ResetEvent    (KERNEL32.@)
 */
BOOL WINAPI ResetEvent( HANDLE handle )
{
    return EVENT_Operation( handle, RESET_EVENT );
}


/***********************************************************************
 * NOTE: The Win95 VWin32_Event routines given below are really low-level
 *       routines implemented directly by VWin32. The user-mode libraries
 *       implement Win32 synchronisation routines on top of these low-level
 *       primitives. We do it the other way around here :-)
 */

/***********************************************************************
 *       VWin32_EventCreate	(KERNEL.442)
 */
HANDLE WINAPI VWin32_EventCreate(VOID)
{
    HANDLE hEvent = CreateEventA( NULL, FALSE, 0, NULL );
    return ConvertToGlobalHandle( hEvent );
}

/***********************************************************************
 *       VWin32_EventDestroy	(KERNEL.443)
 */
VOID WINAPI VWin32_EventDestroy(HANDLE event)
{
    CloseHandle( event );
}

/***********************************************************************
 *       VWin32_EventWait	(KERNEL.450)
 */
VOID WINAPI VWin32_EventWait(HANDLE event)
{
    DWORD mutex_count;

    ReleaseThunkLock( &mutex_count );
    WaitForSingleObject( event, INFINITE );
    RestoreThunkLock( mutex_count );
}

/***********************************************************************
 *       VWin32_EventSet	(KERNEL.451)
 *       KERNEL_479             (KERNEL.479)
 */
VOID WINAPI VWin32_EventSet(HANDLE event)
{
    SetEvent( event );
}



/***********************************************************************
 *           CreateMutexA   (KERNEL32.@)
 */
HANDLE WINAPI CreateMutexA( SECURITY_ATTRIBUTES *sa, BOOL owner, LPCSTR name )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return CreateMutexW( sa, owner, NULL );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return CreateMutexW( sa, owner, buffer );
}


/***********************************************************************
 *           CreateMutexW   (KERNEL32.@)
 */
HANDLE WINAPI CreateMutexW( SECURITY_ATTRIBUTES *sa, BOOL owner, LPCWSTR name )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( create_mutex )
    {
        req->owned   = owner;
        req->inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           OpenMutexA   (KERNEL32.@)
 */
HANDLE WINAPI OpenMutexA( DWORD access, BOOL inherit, LPCSTR name )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return OpenMutexW( access, inherit, NULL );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return OpenMutexW( access, inherit, buffer );
}


/***********************************************************************
 *           OpenMutexW   (KERNEL32.@)
 */
HANDLE WINAPI OpenMutexW( DWORD access, BOOL inherit, LPCWSTR name )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( open_mutex )
    {
        req->access  = access;
        req->inherit = inherit;
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           ReleaseMutex   (KERNEL32.@)
 */
BOOL WINAPI ReleaseMutex( HANDLE handle )
{
    BOOL ret;
    SERVER_START_REQ( release_mutex )
    {
        req->handle = handle;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/*
 * Semaphores
 */


/***********************************************************************
 *           CreateSemaphoreA   (KERNEL32.@)
 */
HANDLE WINAPI CreateSemaphoreA( SECURITY_ATTRIBUTES *sa, LONG initial, LONG max, LPCSTR name )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return CreateSemaphoreW( sa, initial, max, NULL );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return CreateSemaphoreW( sa, initial, max, buffer );
}


/***********************************************************************
 *           CreateSemaphoreW   (KERNEL32.@)
 */
HANDLE WINAPI CreateSemaphoreW( SECURITY_ATTRIBUTES *sa, LONG initial,
                                    LONG max, LPCWSTR name )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;

    /* Check parameters */

    if ((max <= 0) || (initial < 0) || (initial > max))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }

    SERVER_START_REQ( create_semaphore )
    {
        req->initial = (unsigned int)initial;
        req->max     = (unsigned int)max;
        req->inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           OpenSemaphoreA   (KERNEL32.@)
 */
HANDLE WINAPI OpenSemaphoreA( DWORD access, BOOL inherit, LPCSTR name )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return OpenSemaphoreW( access, inherit, NULL );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return OpenSemaphoreW( access, inherit, buffer );
}


/***********************************************************************
 *           OpenSemaphoreW   (KERNEL32.@)
 */
HANDLE WINAPI OpenSemaphoreW( DWORD access, BOOL inherit, LPCWSTR name )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( open_semaphore )
    {
        req->access  = access;
        req->inherit = inherit;
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           ReleaseSemaphore   (KERNEL32.@)
 */
BOOL WINAPI ReleaseSemaphore( HANDLE handle, LONG count, LONG *previous )
{
    NTSTATUS status = NtReleaseSemaphore( handle, count, previous );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/*
 * Pipes
 */


/***********************************************************************
 *           CreateNamedPipeA   (KERNEL32.@)
 */
HANDLE WINAPI CreateNamedPipeA( LPCSTR name, DWORD dwOpenMode,
                                DWORD dwPipeMode, DWORD nMaxInstances,
                                DWORD nOutBufferSize, DWORD nInBufferSize,
                                DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES attr )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return CreateNamedPipeW( NULL, dwOpenMode, dwPipeMode, nMaxInstances,
                                        nOutBufferSize, nInBufferSize, nDefaultTimeOut, attr );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return CreateNamedPipeW( buffer, dwOpenMode, dwPipeMode, nMaxInstances,
                             nOutBufferSize, nInBufferSize, nDefaultTimeOut, attr );
}


/***********************************************************************
 *           CreateNamedPipeW   (KERNEL32.@)
 */
HANDLE WINAPI CreateNamedPipeW( LPCWSTR name, DWORD dwOpenMode,
                                DWORD dwPipeMode, DWORD nMaxInstances,
                                DWORD nOutBufferSize, DWORD nInBufferSize,
                                DWORD nDefaultTimeOut,
                                LPSECURITY_ATTRIBUTES sa )
{
    HANDLE ret;
    DWORD len = name ? strlenW(name) : 0;

    TRACE("(%s, %#08lx, %#08lx, %ld, %ld, %ld, %ld, %p)\n",
          debugstr_w(name), dwOpenMode, dwPipeMode, nMaxInstances,
          nOutBufferSize, nInBufferSize, nDefaultTimeOut, sa );

    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( create_named_pipe )
    {
        req->openmode = dwOpenMode;
        req->pipemode = dwPipeMode;
        req->inherit = (sa &&
                        (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        req->maxinstances = nMaxInstances;
        req->outsize = nOutBufferSize;
        req->insize = nInBufferSize;
        req->timeout = nDefaultTimeOut;
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           PeekNamedPipe   (KERNEL32.@)
 */
BOOL WINAPI PeekNamedPipe( HANDLE hPipe, LPVOID lpvBuffer, DWORD cbBuffer,
                           LPDWORD lpcbRead, LPDWORD lpcbAvail, LPDWORD lpcbMessage )
{
#ifdef HAVE_SYS_SOCKET_H
    int f, waiting = 0;

    f = FILE_GetUnixHandle(hPipe, GENERIC_READ);
    if (f == -1) return FALSE;

    if (ioctl(f, FIONREAD, &waiting) == -1)
    {
        ERR("ioctl on named pipe failed.\n");
        close(f);
        return FALSE;
    }
    if (lpcbAvail) *lpcbAvail = waiting;
    if (lpcbRead) *lpcbRead = 0;
    if (lpcbMessage) *lpcbMessage = 0; /* FIXME */
    if (waiting && lpvBuffer && cbBuffer)
    {
        int readbytes = (waiting < cbBuffer) ? waiting : cbBuffer;
        int ret;
        ret = recv(f, lpvBuffer, readbytes, MSG_PEEK);
        if (ret == -1)
        {
            ERR("failed to peek into socket.\n");
            ERR("reason: %s\n", strerror(errno));
            close(f);
            return FALSE;
        }
        if (lpcbRead) *lpcbRead = ret;
    }
    close(f);
    return TRUE;
#else
    FIXME("unimplemented. Need sys/socket.h during build.\n");
    return FALSE;
#endif
}

/***********************************************************************
 *           SYNC_CompletePipeOverlapped   (Internal)
 */
static void SYNC_CompletePipeOverlapped (LPOVERLAPPED overlapped, DWORD result)
{
    TRACE("for %p result %08lx\n",overlapped,result);
    if(!overlapped)
        return;
    overlapped->Internal = result;
    SetEvent(overlapped->hEvent);
}


/***********************************************************************
 *           WaitNamedPipeA   (KERNEL32.@)
 */
BOOL WINAPI WaitNamedPipeA (LPCSTR name, DWORD nTimeOut)
{
    WCHAR buffer[MAX_PATH];

    if (!name) return WaitNamedPipeW( NULL, nTimeOut );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return WaitNamedPipeW( buffer, nTimeOut );
}


/***********************************************************************
 *           WaitNamedPipeW   (KERNEL32.@)
 */
BOOL WINAPI WaitNamedPipeW (LPCWSTR name, DWORD nTimeOut)
{
    DWORD len = name ? strlenW(name) : 0;
    BOOL ret;
    OVERLAPPED ov;

    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return FALSE;
    }

    TRACE("%s 0x%08lx\n",debugstr_w(name),nTimeOut);

    memset(&ov,0,sizeof ov);
    ov.hEvent = CreateEventA( NULL, 0, 0, NULL );
    if (!ov.hEvent)
        return FALSE;

    SERVER_START_REQ( wait_named_pipe )
    {
        req->timeout = nTimeOut;
        req->overlapped = &ov;
        req->func = SYNC_CompletePipeOverlapped;
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;

    if(ret)
    {
        if (WAIT_OBJECT_0==WaitForSingleObject(ov.hEvent,INFINITE))
        {
            SetLastError(ov.Internal);
            ret = (ov.Internal==STATUS_SUCCESS);
        }
    }
    CloseHandle(ov.hEvent);
    return ret;
}


/***********************************************************************
 *           SYNC_ConnectNamedPipe   (Internal)
 */
static BOOL SYNC_ConnectNamedPipe(HANDLE hPipe, LPOVERLAPPED overlapped)
{
    BOOL ret;

    if(!overlapped)
        return FALSE;

    overlapped->Internal = STATUS_PENDING;

    SERVER_START_REQ( connect_named_pipe )
    {
        req->handle = hPipe;
        req->overlapped = overlapped;
        req->func = SYNC_CompletePipeOverlapped;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;

    return ret;
}

/***********************************************************************
 *           ConnectNamedPipe   (KERNEL32.@)
 */
BOOL WINAPI ConnectNamedPipe(HANDLE hPipe, LPOVERLAPPED overlapped)
{
    OVERLAPPED ov;
    BOOL ret;

    TRACE("(%d,%p)\n",hPipe, overlapped);

    if(overlapped)
        return SYNC_ConnectNamedPipe(hPipe,overlapped);

    memset(&ov,0,sizeof ov);
    ov.hEvent = CreateEventA(NULL,0,0,NULL);
    if (!ov.hEvent)
        return FALSE;

    ret=SYNC_ConnectNamedPipe(hPipe, &ov);
    if(ret)
    {
        if (WAIT_OBJECT_0==WaitForSingleObject(ov.hEvent,INFINITE))
        {
            SetLastError(ov.Internal);
            ret = (ov.Internal==STATUS_SUCCESS);
        }
    }

    CloseHandle(ov.hEvent);

    return ret;
}

/***********************************************************************
 *           DisconnectNamedPipe   (KERNEL32.@)
 */
BOOL WINAPI DisconnectNamedPipe(HANDLE hPipe)
{
    BOOL ret;

    TRACE("(%d)\n",hPipe);

    SERVER_START_REQ( disconnect_named_pipe )
    {
        req->handle = hPipe;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;

    return ret;
}

/***********************************************************************
 *           TransactNamedPipe   (KERNEL32.@)
 *
 * Combine a read and write into a single call.
 *
 * FIXME: Probably not correct for the overlapped case.
 */
BOOL WINAPI TransactNamedPipe(
    HANDLE hPipe, LPVOID lpInput, DWORD dwInputSize, LPVOID lpOutput,
    DWORD dwOutputSize, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped)
{
    DWORD dwActual;

    TRACE( "%d %p %ld %p %ld %p %p\n",
           hPipe, lpInput, dwInputSize, lpOutput,
           dwOutputSize, lpBytesRead, lpOverlapped);
 
    if( lpOverlapped ) FIXME( "correct?\n" );

    if(lpBytesRead)
        *lpBytesRead=0;

    if( !WriteFile( hPipe, lpInput, dwInputSize, &dwActual, lpOverlapped ) )
    {
      DWORD ret;

      /* Fail at this point on genuine error */
      if( !lpOverlapped || (GetLastError() != ERROR_IO_PENDING) ) return FALSE;
      
      /* Wait for IO to complete */
      ret = WaitForSingleObject( lpOverlapped->hEvent, INFINITE );
      if( ret != WAIT_OBJECT_0 ) return FALSE;
    } 

    /* Hmmm. Should we be making sure that everything was read? */
    return ReadFile( hPipe, lpOutput, dwOutputSize, lpBytesRead, lpOverlapped );
}

/***********************************************************************
 *           GetNamedPipeInfo   (KERNEL32.@)
 */
BOOL WINAPI GetNamedPipeInfo(
    HANDLE hNamedPipe, LPDWORD lpFlags, LPDWORD lpOutputBufferSize,
    LPDWORD lpInputBufferSize, LPDWORD lpMaxInstances)
{
    BOOL ret;

    TRACE("%d %p %p %p %p\n", hNamedPipe, lpFlags,
          lpOutputBufferSize, lpInputBufferSize, lpMaxInstances);

    SERVER_START_REQ( get_named_pipe_info )
    {
        req->handle = hNamedPipe;
        ret = !wine_server_call_err( req );
        if(lpFlags) *lpFlags = reply->flags;
        if(lpOutputBufferSize) *lpOutputBufferSize = reply->outsize;
        if(lpInputBufferSize) *lpInputBufferSize = reply->outsize;
        if(lpMaxInstances) *lpMaxInstances = reply->maxinstances;
    }
    SERVER_END_REQ;

    return ret;
}

/***********************************************************************
 *           GetNamedPipeHandleStateA  (KERNEL32.@)
 */
BOOL WINAPI GetNamedPipeHandleStateA(
    HANDLE hNamedPipe, LPDWORD lpState, LPDWORD lpCurInstances,
    LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout,
    LPSTR lpUsername, DWORD nUsernameMaxSize)
{
    FIXME("%d %p %p %p %p %p %ld\n",
          hNamedPipe, lpState, lpCurInstances,
          lpMaxCollectionCount, lpCollectDataTimeout,
          lpUsername, nUsernameMaxSize);

    return FALSE;
}

/***********************************************************************
 *           GetNamedPipeHandleStateW  (KERNEL32.@)
 */
BOOL WINAPI GetNamedPipeHandleStateW(
    HANDLE hNamedPipe, LPDWORD lpState, LPDWORD lpCurInstances,
    LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout,
    LPWSTR lpUsername, DWORD nUsernameMaxSize)
{
    FIXME("%d %p %p %p %p %p %ld\n",
          hNamedPipe, lpState, lpCurInstances,
          lpMaxCollectionCount, lpCollectDataTimeout,
          lpUsername, nUsernameMaxSize);

    return FALSE;
}

/***********************************************************************
 *           SetNamedPipeHandleState  (KERNEL32.@)
 */
BOOL WINAPI SetNamedPipeHandleState(
    HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount,
    LPDWORD lpCollectDataTimeout)
{
    FIXME("%d %p %p %p\n",
          hNamedPipe, lpMode, lpMaxCollectionCount, lpCollectDataTimeout);
    return FALSE;
}

/***********************************************************************
 *           CallNamedPipeA  (KERNEL32.@)
 */
BOOL WINAPI CallNamedPipeA(
    LPCSTR lpNamedPipeName, LPVOID lpInput, DWORD lpInputSize,
    LPVOID lpOutput, DWORD lpOutputSize,
    LPDWORD lpBytesRead, DWORD nTimeout)
{
    WCHAR buffer[MAX_PATH];

    TRACE( "%s\n", debugstr_a(lpNamedPipeName) );

    if (!MultiByteToWideChar( CP_ACP, 0, lpNamedPipeName, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return FALSE;
    }

    return CallNamedPipeW( buffer, lpInput, lpInputSize, lpOutput, lpOutputSize,
                           lpBytesRead, nTimeout);
}

/***********************************************************************
 *           CallNamedPipeW  (KERNEL32.@)
 */
BOOL WINAPI CallNamedPipeW(
    LPCWSTR lpNamedPipeName, LPVOID lpInput, DWORD lpInputSize,
    LPVOID lpOutput, DWORD lpOutputSize,
    LPDWORD lpBytesRead, DWORD nTimeout)
{
    HANDLE hPipe;
    BOOL bSuccess;

    TRACE( "%s %p %ld %p %ld %p %ld\n",
           debugstr_w(lpNamedPipeName), lpInput, lpInputSize,
           lpOutput, lpOutputSize, lpBytesRead, nTimeout);

    if( !WaitNamedPipeW( lpNamedPipeName, nTimeout ) ) return FALSE;

    hPipe = CreateFileW( lpNamedPipeName,
                         GENERIC_READ | GENERIC_WRITE,
                         0, NULL,
                         OPEN_EXISTING,
                         0, 0 );

    if( hPipe == INVALID_HANDLE_VALUE ) return FALSE;

    bSuccess = TransactNamedPipe( hPipe, lpInput, lpInputSize,
                                  lpOutput, lpOutputSize,
                                  lpBytesRead, NULL );

    CloseHandle( hPipe );

    return bSuccess;
}

/******************************************************************
 *		CreatePipe (KERNEL32.@)
 *
 */
BOOL WINAPI CreatePipe( PHANDLE hReadPipe, PHANDLE hWritePipe,
                        LPSECURITY_ATTRIBUTES sa, DWORD size )
{
    static unsigned  index = 0;
    char        name[64];
    HANDLE      hr, hw;
    unsigned    in_index = index;

    *hReadPipe = *hWritePipe = INVALID_HANDLE_VALUE;
    /* generate a unique pipe name (system wide) */
    do
    {
        sprintf(name, "\\\\.\\pipe\\Win32.Pipes.%08lu.%08u", GetCurrentProcessId(), ++index);
        hr = CreateNamedPipeA(name, PIPE_ACCESS_INBOUND, 
                              PIPE_TYPE_BYTE | PIPE_WAIT, 1, size, size, 
                              NMPWAIT_USE_DEFAULT_WAIT, sa);
    } while (hr == INVALID_HANDLE_VALUE && index != in_index);
    /* from completion sakeness, I think system resources might be exhausted before this happens !! */
    if (hr == INVALID_HANDLE_VALUE) return FALSE;

    hw = CreateFileA(name, GENERIC_WRITE, 0, sa, OPEN_EXISTING, 0, 0);
    if (hw == INVALID_HANDLE_VALUE) 
    {
        CloseHandle(hr);
        return FALSE;
    }

    *hReadPipe = hr;
    *hWritePipe = hw;
    return TRUE;
}
