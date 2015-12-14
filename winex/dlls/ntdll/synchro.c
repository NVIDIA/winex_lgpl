/*
 * Win32 process and thread synchronisation
 *
 * Copyright 1997 Alexandre Julliard
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>

#include "wine/file.h"  /* for DOSFS_UnixTimeToFileTime */
#include "thread.h"
#include "winerror.h"
#include "wine/server.h"
#include "async.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(sync);


/***********************************************************************
 *              get_timeout
 */
inline static void get_timeout( struct timeval *when, int timeout )
{
    gettimeofday( when, 0 );
    if (timeout)
    {
        long sec = timeout / 1000;
        if ((when->tv_usec += (timeout - 1000*sec) * 1000) >= 1000000)
        {
            when->tv_usec -= 1000000;
            when->tv_sec++;
        }
        when->tv_sec += sec;
    }
}

/* return 1 if t1 is before t2 */
static inline int time_before( const struct timeval *t1, const struct timeval *t2 )
{
    return ((t1->tv_sec < t2->tv_sec) ||
            ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec < t2->tv_usec)));
}

/* How long, in milli seconds, from now until then */
static int get_poll_timeout( const struct timeval* then )
{
    struct timeval now;
    int timeout = 0;

    /* Is this an infinite wait? */
    if( (then->tv_sec == 0) && (then->tv_usec == 0) ) return -1;

    gettimeofday( &now, 0 );

    if( then->tv_sec > now.tv_sec )
    {
       timeout += (then->tv_sec - now.tv_sec) * 1000;
       timeout += (then->tv_usec - now.tv_usec + 1000)/1000;

    }
    else if( (then->tv_sec == now.tv_sec) && (then->tv_usec > now.tv_usec) )
    {
       timeout += (then->tv_usec - now.tv_usec + 1000)/1000;
    }

    /* Unfortunately timeout < 0 means infinite timeout */
#if !defined( MAX_INT )
#define MAX_INT    0x7ffffff
#endif
    if( timeout < 0 ) timeout = MAX_INT;

    return timeout;    
}

/***********************************************************************
 *           check_async_list
 *
 * Process a status event from the server.
 */
static void WINAPI check_async_list(async_private *asp, DWORD status)
{
    async_private *ovp;
    DWORD ovp_status;

    for( ovp = NtCurrentTeb()->pending_list; ovp && ovp != asp; ovp = ovp->next );

    if(!ovp)
            return;

    if( status != STATUS_ALERTED )
    {
        ovp_status = status;
        ovp->ops->set_status (ovp, status);
    }
    else ovp_status = ovp->ops->get_status (ovp);

    if( ovp_status == STATUS_PENDING ) ovp->func( ovp );

    /* This will destroy all but PENDING requests */
    register_old_async( ovp );
}


/***********************************************************************
 *              wait_server_reply
 *
 * Wait for a reply on the waiting pipe of the current thread.
 */
int wait_server_reply( const void *cookie )
{
    int signaled;
    struct wake_up_reply reply;

    TRACE( "Checking for result\n" );

    for (;;)
    {
        int ret;
        ret = read( NtCurrentTeb()->wait_fd[0], &reply, sizeof(reply) );
        if (ret == sizeof(reply))
        {
            if (!reply.cookie) break;  /* thread got killed */
            if (reply.cookie == cookie) return reply.signaled;
            TRACE( "Stole cookie reply.cookie=%p\n", reply.cookie );
            /* we stole another reply, wait for the real one */
            signaled = wait_server_reply( cookie );
            /* and now put the wrong one back in the pipe */
            for (;;)
            {
                ret = write( NtCurrentTeb()->wait_fd[1], &reply, sizeof(reply) );
                if (ret == sizeof(reply)) break;
                if (ret >= 0) server_protocol_error( "partial wakeup write %d\n", ret );
                if (errno == EINTR) continue;
                server_protocol_perror("wakeup write");
            }
            return signaled;
        }
        if (ret >= 0) server_protocol_error( "partial wakeup read %d\n", ret );
        if (errno == EINTR) continue;
        server_protocol_perror("wakeup read");
    }
    /* the server closed the connection; time to die... */
    SYSDEPS_AbortThread(0);
}


/***********************************************************************
 *              wait_shm_reply
 *
 * Wait for a reply on the waiting pipe of the current thread.
 */
static int wait_shm_reply( const void *cookie, const struct timeval* tv,
                           struct pollfd* pollfd_array, int poll_size,
                           int* hits, int* reg_reply )
{
    int signaled;
    struct wake_up_reply reply;
    for (;;)
    {
        int ret;
        int timeout;

        timeout = get_poll_timeout( tv );

        for(;;)
        {
            TRACE( "Poll waiting for %d\n", timeout );
            ret = poll( pollfd_array, poll_size, timeout );
            TRACE( "Poll complete for %d (%s)\n", ret, (ret != -1) ? "" : strerror( errno ) );

            /* Was the poll interrupted? Restart it */
            if( ret == -1 && errno == EINTR ) {
                timeout = get_poll_timeout( tv );
                continue;
            }

            /* poll can't handle the full timeout range that WaitForMultipleObjects can.
             * Make sure we actually wait for the full expected length of time. */
            if( ret == 0 && ((timeout = get_poll_timeout( tv )) != 0) ) continue;

            break;
        }

        /* Timeout? */
        if( ret == 0 )
        {
            return STATUS_TIMEOUT;
        }
        else if ( ret == -1 )
        {
            server_protocol_perror( "poll" );
        }

        *hits = ret;

        if( pollfd_array[0].revents )
        {
            if( pollfd_array[0].revents & (POLLHUP|POLLERR) ) break;

            *hits = *hits - 1;
            TRACE( "Regular wake up\n" );
            for(;;)
            {
                ret = read( NtCurrentTeb()->wait_fd[0], &reply, sizeof(reply) );

                if (ret == sizeof(reply))
                {
                    if (!reply.cookie) SYSDEPS_AbortThread(0);  /* thread got killed */
                    if (reply.cookie == cookie) {
                        *reg_reply = 1;
                        return reply.signaled;
                    }

                    /* we stole another reply, wait for the real one */
                    TRACE( "Stole cookie\n" );
                    signaled = wait_shm_reply( cookie, tv, pollfd_array, poll_size, hits, reg_reply );
                    /* and now put the wrong one back in the pipe */
                    for (;;)
                    {
                        ret = write( NtCurrentTeb()->wait_fd[1], &reply, sizeof(reply) );
                        if (ret == sizeof(reply)) break;
                        if (ret >= 0) server_protocol_error( "partial wakeup write %d\n", ret );
                        if (errno == EINTR) continue;
                        server_protocol_perror("wakeup write");
                    }
                    return signaled;
                }
                if (ret >= 0) server_protocol_error( "partial wakeup read %d\n", ret );
                if (errno == EINTR ) continue;
                ERR( "Something went wrong with read/write %x/%d\n", pollfd_array[0].revents, errno ); 
                server_protocol_perror("wakeup read");
            }
        }

        if( *hits ) return STATUS_PENDING;

        server_protocol_error( "poll on wait_fd" );
    }
    /* the server closed the connection; time to die... */
    SYSDEPS_AbortThread(0);
}

/* PH: FIXME: Too many defns */
struct shm_pollfd
{
    struct pollfd fd_array[MAXIMUM_WAIT_OBJECTS+1];
    void*         objects[MAXIMUM_WAIT_OBJECTS+1];
};

int wine_server_complete_shm_poll( const void* cookie,
                                   struct shm_pollfd* shm_pollfd,
                                   unsigned int size,
                                   int hits,
                                   int signaled );

static int wait_shm( const void *cookie, const struct timeval* tv,
                     struct shm_pollfd* pfd, unsigned int fds )
{
    int ret;
    int reg_reply = 0;
    int hits = 0;

    /* Add the wait_fd into the poll array */
    pfd->fd_array[0].fd      = NtCurrentTeb()->wait_fd[0];
    pfd->fd_array[0].events  = POLLIN;
    pfd->fd_array[0].revents = 0;

    ret = wait_shm_reply( cookie, tv, pfd->fd_array, fds+1, &hits, &reg_reply );

    /* Right now we either timeout or we're woken up through our wait on an fd
     * because a condition was satisfied or an fd got input.
     *
     * If we've timed out (STATUS_TIMEOUT), we need to consider the possibility that in fact 1 of 2
     * things may have happened:
     * 1) This client thread may have timed out but the condition WAS satisfied (race).
     * 2) This client thread may have timed out and the condition WAS NOT satisfied.
     * If we have 2) then we must remove the timeout and wait structures for this thread.
     * If we have 1) we don't need to remove the timeout structure (as it would have been removed
     * by the signalling thread/wineserver) but we do need to read from our wait_fd so that the
     * next wait isn't instantly signalled.
     *
     * If we've had an fd event 1 of 2 things have happened.
     * 1) If it was on the wait_fd, then we don't have anything else to do as everything is
     *    cleaned up in the wineserver. In this cases ret != STATUS_PENDING
     * 2) We had an event on one or more fds. In this case we need to call back into the
     *    wineserver so that we can deal with it.
     */
    /* if( ret == STATUS_TIMEOUT || hits ) */
    if( !reg_reply )
    {
        if( wine_server_complete_shm_poll( cookie, pfd, fds, hits, ret ) && 
            (ret == STATUS_TIMEOUT || ret == STATUS_PENDING) )
        {
            TRACE( "Hmmm. Actually have wait race\n" );
            ret = wait_server_reply( cookie );
        }
    }

    TRACE( "Have ret of %x\n", ret );

    return ret;
}

/***********************************************************************
 *              call_apcs
 *
 * Call outstanding APCs.
 */
static void call_apcs( BOOL alertable )
{
    FARPROC proc = NULL;
    FILETIME ft;
    void *args[4];

    for (;;)
    {
        int type = APC_NONE;
        SERVER_START_REQ( get_apc )
        {
            req->alertable = alertable;
            wine_server_set_reply( req, args, sizeof(args) );
            if (!wine_server_call( req ))
            {
                type = reply->type;
                proc = reply->func;
            }
        }
        SERVER_END_REQ;

        switch(type)
        {
        case APC_NONE:
            return;  /* no more APCs */
        case APC_ASYNC:
            proc( args[0], args[1]);
            break;
        case APC_USER:
            proc( args[0] );
            break;
        case APC_TIMER:
            /* convert sec/usec to NT time */
            DOSFS_UnixTimeToFileTime( (time_t)args[0], &ft, (DWORD)args[1] * 10 );
            proc( args[2], ft.dwLowDateTime, ft.dwHighDateTime );
            break;
        case APC_ASYNC_IO:
            check_async_list ( args[0], (DWORD) args[1]);
            break;

        default:
            server_protocol_error( "get_apc_request: bad type %d\n", type );
            break;
        }
    }
}

int __wine_recv_unix_signal(int *code, size_t *size, void *data)
{
    int ret, cookie;

    SERVER_START_REQ( recv_unix_signal )
    {
        req->cookie = &cookie;
        if (size) wine_server_set_reply( req, data, *size );
        ret = wine_server_call( req );
        *code = reply->code;
        if (size) *size = wine_server_reply_size(reply);
    }
    SERVER_END_REQ;

    if (ret == STATUS_PENDING) ret = wait_server_reply( &cookie );
    if (ret != STATUS_SUCCESS) *code = -1;
    return ret;
}


/***********************************************************************
 *              Sleep  (KERNEL32.@)
 */
VOID WINAPI Sleep( DWORD timeout )
{
   SleepEx (timeout, FALSE);
}

/******************************************************************************
 *              SleepEx   (KERNEL32.@)
 */
DWORD WINAPI SleepEx( DWORD timeout, BOOL alertable )
{
    DWORD ret;

    ret = WaitForMultipleObjectsEx( 0, NULL, FALSE, timeout, alertable );
    if (ret != WAIT_IO_COMPLETION) ret = 0;
    return ret;
}


/***********************************************************************
 *           WaitForSingleObject   (KERNEL32.@)
 */
DWORD WINAPI WaitForSingleObject( HANDLE handle, DWORD timeout )
{
    return WaitForMultipleObjectsEx( 1, &handle, FALSE, timeout, FALSE );
}


/***********************************************************************
 *           WaitForSingleObjectEx   (KERNEL32.@)
 */
DWORD WINAPI WaitForSingleObjectEx( HANDLE handle, DWORD timeout,
                                    BOOL alertable )
{
    return WaitForMultipleObjectsEx( 1, &handle, FALSE, timeout, alertable );
}


/***********************************************************************
 *           WaitForMultipleObjects   (KERNEL32.@)
 */
DWORD WINAPI WaitForMultipleObjects( DWORD count, const HANDLE *handles,
                                     BOOL wait_all, DWORD timeout )
{
    return WaitForMultipleObjectsEx( count, handles, wait_all, timeout, FALSE );
}


/***********************************************************************
 *           do_multiple_wait
 */
static DWORD do_multiple_wait (DWORD count, const HANDLE *handles,
                               BOOL wait_all, DWORD timeout,
                               BOOL alertable, HANDLE hSignalObject)
{
    int ret, cookie;
    struct timeval tv;

    if (timeout == INFINITE) tv.tv_sec = tv.tv_usec = 0;
    else get_timeout( &tv, timeout );

    if( call_supported_through_shared_memory( REQ_shm_select ) )
    {
      for(;;)
      {
        struct shm_pollfd shm_pfd;
        int   pollfds;

        SERVER_START_REQ( shm_select )
        {
            req->flags   = SELECT_INTERRUPTIBLE;
            req->cookie  = &cookie;
            req->sec     = tv.tv_sec;
            req->usec    = tv.tv_usec;
            req->hSignalObject = hSignalObject;
            req->pollfd  = &shm_pfd;
            wine_server_add_data( req, handles, count * sizeof(HANDLE) );

            if (wait_all) req->flags |= SELECT_ALL;
            if (alertable) req->flags |= SELECT_ALERTABLE;
            if (timeout != INFINITE) req->flags |= SELECT_TIMEOUT;

            ret = wine_server_call( req );

            pollfds = reply->pollfds;
        }
        SERVER_END_REQ;

        if (ret == STATUS_PENDING)
        {
            ret = wait_shm( &cookie, &tv, &shm_pfd, pollfds );
            if( ret == STATUS_PENDING ) continue;
        }

        if (ret != STATUS_USER_APC) break;
        call_apcs( alertable );
        if (alertable) break;
      }
    }
    else
    {
      for (;;)
      {
        SERVER_START_REQ( select )
        {
            req->flags   = SELECT_INTERRUPTIBLE;
            req->cookie  = &cookie;
            req->sec     = tv.tv_sec;
            req->usec    = tv.tv_usec;
            req->hSignalObject = hSignalObject;
            wine_server_add_data( req, handles, count * sizeof(HANDLE) );

            if (wait_all) req->flags |= SELECT_ALL;
            if (alertable) req->flags |= SELECT_ALERTABLE;
            if (timeout != INFINITE) req->flags |= SELECT_TIMEOUT;

            ret = wine_server_call( req );
        }
        SERVER_END_REQ;

        if (ret == STATUS_PENDING) ret = wait_server_reply( &cookie );
        if (ret != STATUS_USER_APC) break;
        call_apcs( alertable );
        if (alertable) break;
      }
    }

    if (HIWORD(ret))  /* is it an error code? */
    {
        SetLastError( RtlNtStatusToDosError(ret) );
        ret = WAIT_FAILED;
    }
    return ret;
}


/***********************************************************************
 *           WaitForMultipleObjectsEx   (KERNEL32.@)
 */
DWORD WINAPI WaitForMultipleObjectsEx( DWORD count, const HANDLE *handles,
                                       BOOL wait_all, DWORD timeout,
                                       BOOL alertable )
{
    if (count > MAXIMUM_WAIT_OBJECTS)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return WAIT_FAILED;
    }

    return do_multiple_wait (count, handles, wait_all, timeout, alertable, 0);
}


/***********************************************************************
 *           SignalObjectAndWait   (KERNEL32.@)
 */
DWORD WINAPI SignalObjectAndWait (HANDLE hObjectToSignal,
                                  HANDLE hObjectToWaitOn,
                                  DWORD dwMilliseconds,
                                  BOOL bAlertable)
{
   return do_multiple_wait (1, &hObjectToWaitOn, FALSE, dwMilliseconds,
                            bAlertable, hObjectToSignal);
}


/***********************************************************************
 *           WaitForSingleObject   (KERNEL.460)
 */
DWORD WINAPI WaitForSingleObject16( HANDLE handle, DWORD timeout )
{
    DWORD retval, mutex_count;

    ReleaseThunkLock( &mutex_count );
    retval = WaitForSingleObject( handle, timeout );
    RestoreThunkLock( mutex_count );
    return retval;
}

/***********************************************************************
 *           WaitForMultipleObjects   (KERNEL.461)
 */
DWORD WINAPI WaitForMultipleObjects16( DWORD count, const HANDLE *handles,
                                       BOOL wait_all, DWORD timeout )
{
    DWORD retval, mutex_count;

    ReleaseThunkLock( &mutex_count );
    retval = WaitForMultipleObjectsEx( count, handles, wait_all, timeout, FALSE );
    RestoreThunkLock( mutex_count );
    return retval;
}

/***********************************************************************
 *           WaitForMultipleObjectsEx   (KERNEL.495)
 */
DWORD WINAPI WaitForMultipleObjectsEx16( DWORD count, const HANDLE *handles,
                                         BOOL wait_all, DWORD timeout, BOOL alertable )
{
    DWORD retval, mutex_count;

    ReleaseThunkLock( &mutex_count );
    retval = WaitForMultipleObjectsEx( count, handles, wait_all, timeout, alertable );
    RestoreThunkLock( mutex_count );
    return retval;
}
