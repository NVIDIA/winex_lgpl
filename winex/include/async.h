/*
 * Structures and static functions for handling asynchronous I/O.
 *
 * Copyright (C) 2002 Mike McCormack, Martin Wilck
 */

/*
 * This file declares static functions.
 * It should only be included by those source files that implement async I/O requests.
 */

#ifndef __WINE_ASYNC_H
#define __WINE_ASYNC_H

#include "winbase.h"

struct async_private;

typedef void (*async_handler)(struct async_private *ovp);
typedef void (CALLBACK *async_call_completion_func)(ULONG_PTR data);
typedef DWORD (*async_get_status)(const struct async_private *ovp);
typedef DWORD (*async_get_count)(const struct async_private *ovp);
typedef void (*async_set_status)(struct async_private *ovp, const DWORD status);
typedef void (*async_cleanup)(struct async_private *ovp);

typedef struct async_ops
{
    async_get_status            get_status;
    async_set_status            set_status;
    async_get_count             get_count;
    async_call_completion_func  call_completion;
    async_cleanup               cleanup;
} async_ops;

typedef struct async_private
{
    struct async_ops     *ops;
    HANDLE        handle;
    int           fd;
    async_handler func;
    int           type;
    LPOVERLAPPED  lpOverlapped;
    BOOL          forceOverlapped;
    HANDLE        hStartupEvent;
    struct async_private *next;
    struct async_private *prev;
} async_private;

/* All functions declared static for Dll separation purposes */

inline static void remove_async_from_TEB (async_private *ovp)
{
   if (ovp->prev)
      ovp->prev->next = ovp->next;
   else
      NtCurrentTeb()->pending_list = ovp->next;

   if (ovp->next)
      ovp->next->prev = ovp->prev;

   ovp->next = ovp->prev = NULL;
}


inline static void finish_async( async_private *ovp )
{
    remove_async_from_TEB (ovp);

    close( ovp->fd );

    if( ovp->lpOverlapped->hEvent != INVALID_HANDLE_VALUE )
        NtSetEvent( ovp->lpOverlapped->hEvent, NULL );

    NtClose (ovp->hStartupEvent);

    /* Windows doesn't call the completion routine if there was an error
       in the inital async operation setup */
    if ( ovp->ops->call_completion &&
         (ovp->ops->get_status (ovp) != STATUS_INVALID_PARAMETER))
        QueueUserAPC( ovp->ops->call_completion, GetCurrentThread(), (ULONG_PTR)ovp );
    else
        ovp->ops->cleanup ( ovp );
}

inline static BOOL __register_async( async_private *ovp, const DWORD status )
{
    BOOL ret, IsIoComp = FALSE;

    SERVER_START_REQ( register_async )
    {
        req->handle = ovp->handle;
        req->overlapped = ovp;
        req->type = ovp->type;
        req->count = ovp->ops->get_count( ovp );
        req->status = status;
        req->has_callback = (ovp->ops->call_completion != NULL);
        req->force_overlapped = ovp->forceOverlapped;
        ret = wine_server_call( req );
        if (ret == STATUS_SUCCESS)
           IsIoComp = reply->is_io_comp;
    }
    SERVER_END_REQ;

    if (IsIoComp)
       remove_async_from_TEB (ovp);

    if ( ret ) {
        SetLastError( RtlNtStatusToDosError(ret) );
        ovp->ops->set_status ( ovp, ret );
    }

    if ( ovp->ops->get_status (ovp) != STATUS_PENDING )
        finish_async (ovp);
    else
       NtSetEvent (ovp->hStartupEvent, NULL);

    return ret;
}

#define register_old_async(ovp) \
    __register_async (ovp, ovp->ops->get_status( ovp ));

inline static BOOL register_new_async( async_private *ovp )
{
    ovp->ops->set_status ( ovp, STATUS_PENDING );

    ovp->next = NtCurrentTeb()->pending_list;
    ovp->prev = NULL;
    if ( ovp->next ) ovp->next->prev = ovp;
    NtCurrentTeb()->pending_list = ovp;

    return __register_async( ovp, STATUS_PENDING );
}

inline static BOOL cancel_async ( async_private *ovp )
{
     /* avoid multiple cancellations */
     if ( ovp->ops->get_status( ovp ) != STATUS_PENDING )
          return 0;
     ovp->ops->set_status ( ovp, STATUS_CANCELLED );
     return __register_async ( ovp, STATUS_CANCELLED );
}

#endif /* __WINE_ASYNC_H */
