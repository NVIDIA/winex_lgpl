/*
 * Message queues related functions
 *
 * Copyright 1993, 1994 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <string.h>
#include <signal.h>
#include <assert.h>
#include "windef.h"
#include "wingdi.h"
#include "winerror.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "queue.h"
#include "win.h"
#include "user.h"
#include "hook.h"
#include "wine/thread.h"
#include "wine/debug.h"
#include "wine/server.h"
#include "spy.h"

WINE_DEFAULT_DEBUG_CHANNEL(msg);


static PERQUEUEDATA *pQDataWin16 = NULL;  /* Global perQData for Win16 tasks */

static CRITICAL_SECTION queueCS;

HQUEUE16 hActiveQueue = 0;

void create_queue_cs(void)
{
  CRITICAL_SECTION_DEFINE( &queueCS );
}

void destroy_queue_cs(void)
{
  DeleteCriticalSection( &queueCS );
}


/***********************************************************************
 *           PERQDATA_Addref
 *
 * Increment reference count for the PERQUEUEDATA instance
 */
static ULONG PERQDATA_Addref( PERQUEUEDATA *pQData )
{
    ULONG ulRef;

    assert(pQData != 0 );
    
    ulRef = InterlockedIncrement( &pQData->ulRefCount );
    
    TRACE("(%p): refcount incremented to %lu ...\n",
                  pQData, ulRef );

    return ulRef;
}


/***********************************************************************
 *           PERQDATA_Release
 *
 * Release a reference to a PERQUEUEDATA instance.
 * Destroy the instance if no more references exist.
 * Return the new ref count.
 */
static ULONG PERQDATA_Release( PERQUEUEDATA *pQData )
{
    ULONG ulRef;
    assert(pQData != 0 );

    ulRef = InterlockedDecrement( &pQData->ulRefCount );

    TRACE("(%p): refcount decremented to %lu ...\n",
                  pQData, ulRef );

    if (!ulRef)
    {
        DeleteCriticalSection( &pQData->cSection );

        TRACE("(%p): deleting PERQUEUEDATA instance (%p) ...\n",
	             pQData, pQDataWin16 );

        /* Deleting our global 16 bit perQData? */
        if ( pQData == pQDataWin16 ) pQDataWin16 = 0;

        /* Free the PERQUEUEDATA instance */
        HeapFree( GetProcessHeap(), 0, pQData );
    }
    
    return ulRef;
}


/***********************************************************************
 *           PERQDATA_CreateInstance
 *
 * Creates an instance of a reference counted PERQUEUEDATA element
 * for the message queue. perQData is stored globally for 16 bit tasks.
 *
 * Note: We don't implement perQdata exactly the same way Windows does.
 * Each perQData element is reference counted since it may be potentially
 * shared by multiple message Queues (via AttachThreadInput).
 * We only store the current values for Active, Capture and focus windows
 * currently.
 */
static PERQUEUEDATA * PERQDATA_CreateInstance(void)
{
    PERQUEUEDATA *pQData;

    BOOL16 bIsWin16 = 0;

    TRACE("()\n");

    /* Share a single instance of perQData for all 16 bit tasks */
    if ( ( bIsWin16 = !(NtCurrentTeb()->tibflags & TEBF_WIN32) ) )
    {
        /* If previously allocated, just bump up ref count */
        if ( pQDataWin16 )
        {
            PERQDATA_Addref( pQDataWin16 );
            return pQDataWin16;
        }
    }

    /* Allocate PERQUEUEDATA from the system heap */
    if (!( pQData = (PERQUEUEDATA *) HeapAlloc( GetProcessHeap(), 0,
                                                    sizeof(PERQUEUEDATA) ) ))
        return 0;
	
    TRACE( "created PERQDATA=%p\n", pQData );

    /* Initialize */
    pQData->hWndCapture = pQData->hWndFocus = pQData->hWndActive = 0;
    pQData->ulRefCount = 1;
    pQData->nCaptureHT = HTCLIENT;

    /* Note: We have an independent critical section for the per queue data
     * since this may be shared by different threads. see AttachThreadInput()
     */
    CRITICAL_SECTION_DEFINE( &pQData->cSection );
    /* FIXME: not all per queue data critical sections should be global */
    MakeCriticalSectionGlobal( &pQData->cSection );

    /* Save perQData globally for 16 bit tasks */
    if ( bIsWin16 )
        pQDataWin16 = pQData;

    return pQData;
}


/***********************************************************************
 *           PERQDATA_GetFocusWnd
 *
 * Get the focus hwnd member in a threadsafe manner
 */
HWND PERQDATA_GetFocusWnd( PERQUEUEDATA *pQData )
{
    HWND hWndFocus;
    assert(pQData != 0 );

    EnterCriticalSection( &pQData->cSection );
    hWndFocus = pQData->hWndFocus;
    LeaveCriticalSection( &pQData->cSection );

    return hWndFocus;
}


/***********************************************************************
 *           PERQDATA_SetFocusWnd
 *
 * Set the focus hwnd member in a threadsafe manner
 */
HWND PERQDATA_SetFocusWnd( PERQUEUEDATA *pQData, HWND hWndFocus )
{
    HWND hWndFocusPrv;
    assert(pQData != 0 );

    EnterCriticalSection( &pQData->cSection );
    hWndFocusPrv = pQData->hWndFocus;
    pQData->hWndFocus = hWndFocus;
    LeaveCriticalSection( &pQData->cSection );

    return hWndFocusPrv;
}


/***********************************************************************
 *           PERQDATA_GetActiveWnd
 *
 * Get the active hwnd member in a threadsafe manner
 */
HWND PERQDATA_GetActiveWnd( PERQUEUEDATA *pQData )
{
    HWND hWndActive;
    assert(pQData != 0 );

    EnterCriticalSection( &pQData->cSection );
    hWndActive = pQData->hWndActive;
    LeaveCriticalSection( &pQData->cSection );

    return hWndActive;
}


/***********************************************************************
 *           PERQDATA_SetActiveWnd
 *
 * Set the active focus hwnd member in a threadsafe manner
 */
HWND PERQDATA_SetActiveWnd( PERQUEUEDATA *pQData, HWND hWndActive )
{
    HWND hWndActivePrv;
    assert(pQData != 0 );

    EnterCriticalSection( &pQData->cSection );
    hWndActivePrv = pQData->hWndActive;
    pQData->hWndActive = hWndActive;
    LeaveCriticalSection( &pQData->cSection );

    return hWndActivePrv;
}


/***********************************************************************
 *           PERQDATA_GetCaptureWnd
 *
 * Get the capture hwnd member in a threadsafe manner
 */
HWND PERQDATA_GetCaptureWnd( INT *hittest )
{
    MESSAGEQUEUE *queue;
    PERQUEUEDATA *pQData;
    HWND hWndCapture;

    if (!(queue = QUEUE_Current())) return 0;
    pQData = queue->pQData;

    EnterCriticalSection( &pQData->cSection );
    hWndCapture = pQData->hWndCapture;
    *hittest = pQData->nCaptureHT;
    LeaveCriticalSection( &pQData->cSection );
    return hWndCapture;
}


/***********************************************************************
 *           PERQDATA_SetCaptureWnd
 *
 * Set the capture hwnd member in a threadsafe manner
 */
HWND PERQDATA_SetCaptureWnd( HWND hWndCapture, INT hittest )
{
    MESSAGEQUEUE *queue;
    PERQUEUEDATA *pQData;
    HWND hWndCapturePrv;

    if (!(queue = QUEUE_Current())) return 0;
    pQData = queue->pQData;

    EnterCriticalSection( &pQData->cSection );
    hWndCapturePrv = pQData->hWndCapture;
    pQData->hWndCapture = hWndCapture;
    pQData->nCaptureHT = hittest;
    LeaveCriticalSection( &pQData->cSection );
    return hWndCapturePrv;
}



/***********************************************************************
 *	     QUEUE_Lock
 *
 * Function for getting a 32 bit pointer on queue structure. For thread
 * safeness programmers should use this function instead of GlobalLock to
 * retrieve a pointer on the structure. QUEUE_Unlock should also be called
 * when access to the queue structure is not required anymore.
 */
MESSAGEQUEUE *QUEUE_Lock( HQUEUE16 hQueue )
{
    MESSAGEQUEUE *queue;

    EnterCriticalSection( &queueCS );
    queue = GlobalLock16( hQueue );
    if ( !queue || (queue->magic != QUEUE_MAGIC) )
    {
        LeaveCriticalSection( &queueCS );
        return NULL;
    }

    queue->lockCount++;
    LeaveCriticalSection( &queueCS );
    return queue;
}


/***********************************************************************
 *	     QUEUE_Current
 *
 * Get the current thread queue, creating it if required.
 * QUEUE_Unlock is not needed since the queue can only be deleted by
 * the current thread anyway.
 */
MESSAGEQUEUE *QUEUE_Current(void)
{
    MESSAGEQUEUE *queue;
    HQUEUE16 hQueue;

    if (!(hQueue = GetThreadQueue16(0)))
    {
        if (!(hQueue = InitThreadInput16( 0, 0 ))) return NULL;
    }

    if ((queue = GlobalLock16( hQueue )))
    {
        if (queue->magic != QUEUE_MAGIC) queue = NULL;
    }
    return queue;
}


/***********************************************************************
 *	     QUEUE_Unlock
 *
 * Use with QUEUE_Lock to get a thread safe access to message queue
 * structure
 */
void QUEUE_Unlock( MESSAGEQUEUE *queue )
{
    if (queue)
    {
        EnterCriticalSection( &queueCS );

        if ( --queue->lockCount == 0 )
        {
	    HQUEUE16 the_queue = queue->self;
	    
            queue->self = 0;
            if (queue->pQData)
            {
                PERQDATA_Release(queue->pQData);
                queue->pQData = 0;
            }
            if (queue->server_queue)
                CloseHandle( queue->server_queue );
            GlobalFree16( the_queue );
        }

        LeaveCriticalSection( &queueCS );
    }
}


/***********************************************************************
 *           QUEUE_CreateMsgQueue
 *
 * Creates a message queue. Doesn't link it into queue list!
 */
static HQUEUE16 QUEUE_CreateMsgQueue( BOOL16 bCreatePerQData )
{
    HQUEUE16 hQueue;
    HANDLE handle;
    MESSAGEQUEUE * msgQueue;

    TRACE("(): Creating message queue...\n");

    if (!(hQueue = GlobalAlloc16( GMEM_FIXED | GMEM_ZEROINIT,
                                  sizeof(MESSAGEQUEUE) )))
        return 0;

    msgQueue = (MESSAGEQUEUE *) GlobalLock16( hQueue );
    if ( !msgQueue )
        return 0;

    if (bCreatePerQData)
    {
        SERVER_START_REQ( get_msg_queue )
        {
            wine_server_call_err( req );
            handle = reply->handle;
        }
        SERVER_END_REQ;
        if (!handle)
        {
            ERR_(msg)("Cannot get thread queue");
            GlobalFree16( hQueue );
            return 0;
        }
        msgQueue->server_queue = handle;
    }

    msgQueue->self = hQueue;
    msgQueue->lockCount = 1;
    msgQueue->magic = QUEUE_MAGIC;

    /* Create and initialize our per queue data */
    msgQueue->pQData = bCreatePerQData ? PERQDATA_CreateInstance() : NULL;

    return hQueue;
}


/***********************************************************************
 *	     QUEUE_DeleteMsgQueue
 *
 * Unlinks and deletes a message queue.
 *
 * Note: We need to mask asynchronous events to make sure PostMessage works
 * even in the signal handler.
 */
void QUEUE_DeleteMsgQueue(void)
{
    HQUEUE16 hQueue = GetThreadQueue16(0);
    MESSAGEQUEUE * msgQueue;

    if (!hQueue) return;  /* thread doesn't have a queue */

    TRACE("(): Deleting message queue %04x\n", hQueue);

    if (!(msgQueue = QUEUE_Lock(hQueue)))
    {
        ERR("invalid thread queue\n");
        return;
    }

    EnterCriticalSection( &queueCS );

    /* This thread is no longer referencing this object reduce
     * the stay alive count. We cannot, however, destroy the
     * object since another thread may have reference to this
     * queue. When all outstanding references are removed it
     * will be deleted
     */
    msgQueue->lockCount--;
    
    /* This should stop some new requests for the queue object */
    SetThreadQueue16( 0, 0 );
    msgQueue->magic = 0;
    if (hActiveQueue == hQueue) hActiveQueue = 0;
    
    LeaveCriticalSection( &queueCS );

    QUEUE_Unlock( msgQueue );
}


/***********************************************************************
 *		GetWindowTask (USER.224)
 */
HTASK16 WINAPI GetWindowTask16( HWND16 hwnd )
{
    HTASK16 retvalue;
    MESSAGEQUEUE *queue;

    WND *wndPtr = WIN_FindWndPtr16( hwnd );
    if (!wndPtr) return 0;

    queue = QUEUE_Lock( wndPtr->hmemTaskQ );
    WIN_ReleaseWndPtr(wndPtr);

    if (!queue) return 0;

    retvalue = queue->teb->htask16;
    QUEUE_Unlock( queue );

    return retvalue;
}

/***********************************************************************
 *		InitThreadInput (USER.409)
 */
HQUEUE16 WINAPI InitThreadInput16( WORD unknown, WORD flags )
{
    MESSAGEQUEUE *queuePtr;
    HQUEUE16 hQueue = NtCurrentTeb()->queue;

    if ( !hQueue )
    {
        /* Create thread message queue */
        if( !(hQueue = QUEUE_CreateMsgQueue( TRUE )))
        {
            ERR_(msg)("failed!\n");
            return FALSE;
	}

        /* Link new queue into list */
        queuePtr = QUEUE_Lock( hQueue );
        queuePtr->teb = NtCurrentTeb();

        EnterCriticalSection( &queueCS );
        SetThreadQueue16( 0, hQueue );
        NtCurrentTeb()->queue = hQueue;
        LeaveCriticalSection( &queueCS );

        QUEUE_Unlock( queuePtr );
    }

    return hQueue;
}

/***********************************************************************
 *		GetQueueStatus (USER32.@)
 */
DWORD WINAPI GetQueueStatus( UINT flags )
{
    DWORD ret = 0;

    SERVER_START_REQ( get_queue_status )
    {
        req->clear = 1;
        wine_server_call( req );
        ret = MAKELONG( reply->changed_bits & flags, reply->wake_bits & flags );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *		GetInputState   (USER32.@)
 */
BOOL WINAPI GetInputState(void)
{
    DWORD ret = 0;

    SERVER_START_REQ( get_queue_status )
    {
        req->clear = 0;
        wine_server_call( req );
        ret = reply->wake_bits & (QS_KEY | QS_MOUSEBUTTON);
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *		GetMessagePos (USER.119)
 *		GetMessagePos (USER32.@)
 *
 * The GetMessagePos() function returns a long value representing a
 * cursor position, in screen coordinates, when the last message
 * retrieved by the GetMessage() function occurs. The x-coordinate is
 * in the low-order word of the return value, the y-coordinate is in
 * the high-order word. The application can use the MAKEPOINT()
 * macro to obtain a POINT structure from the return value.
 *
 * For the current cursor position, use GetCursorPos().
 *
 * RETURNS
 *
 * Cursor position of last message on success, zero on failure.
 *
 * CONFORMANCE
 *
 * ECMA-234, Win32
 *
 */
DWORD WINAPI GetMessagePos(void)
{
    MESSAGEQUEUE *queue;

    if (!(queue = QUEUE_Current())) return 0;
    return queue->GetMessagePosVal;
}


/***********************************************************************
 *		GetMessageTime (USER.120)
 *		GetMessageTime (USER32.@)
 *
 * GetMessageTime() returns the message time for the last message
 * retrieved by the function. The time is measured in milliseconds with
 * the same offset as GetTickCount().
 *
 * Since the tick count wraps, this is only useful for moderately short
 * relative time comparisons.
 *
 * RETURNS
 *
 * Time of last message on success, zero on failure.
 *
 * CONFORMANCE
 *
 * ECMA-234, Win32
 *
 */
LONG WINAPI GetMessageTime(void)
{
    MESSAGEQUEUE *queue;

    if (!(queue = QUEUE_Current())) return 0;
    if (queue->GetMessageTimeVal == 0)
    {
	TRACE (" GetMessageTime is 0. Using GetCurrentTime instead.\n");
	return GetCurrentTime();
    }
    return queue->GetMessageTimeVal;
}


/***********************************************************************
 *		GetMessageExtraInfo (USER.288)
 *		GetMessageExtraInfo (USER32.@)
 */
LONG WINAPI GetMessageExtraInfo(void)
{
    MESSAGEQUEUE *queue;

    if (!(queue = QUEUE_Current())) return 0;
    return queue->GetMessageExtraInfoVal;
}


/**********************************************************************
 *		AttachThreadInput (USER32.@) Attaches input of 1 thread to other
 *
 * Attaches the input processing mechanism of one thread to that of
 * another thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 *
 * TODO:
 *    1. Reset the Key State (currenly per thread key state is not maintained)
 */
BOOL WINAPI AttachThreadInput(
    DWORD idAttach,   /* [in] Thread to attach */
    DWORD idAttachTo, /* [in] Thread to attach to */
    BOOL fAttach)   /* [in] Attach or detach */
{
    MESSAGEQUEUE *pSrcMsgQ = 0, *pTgtMsgQ = 0;
    BOOL16 bRet = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    /* A thread cannot attach to itself */
    if ( idAttach == idAttachTo )
        goto CLEANUP;

    /* According to the docs this method should fail if a
     * "Journal record" hook is installed. (attaches all input queues together)
     */
    if ( HOOK_IsHooked( WH_JOURNALRECORD ) )
        goto CLEANUP;

    /* Retrieve message queues corresponding to the thread id's */
    pTgtMsgQ = QUEUE_Lock( GetThreadQueue16( idAttach ) );
    pSrcMsgQ = QUEUE_Lock( GetThreadQueue16( idAttachTo ) );

    /* Ensure we have message queues and that Src and Tgt threads
     * are not system threads.
     */
    if ( !pSrcMsgQ || !pTgtMsgQ || !pSrcMsgQ->pQData || !pTgtMsgQ->pQData )
        goto CLEANUP;

    if (fAttach)   /* Attach threads */
    {
        /* Only attach if currently detached  */
        if ( pTgtMsgQ->pQData != pSrcMsgQ->pQData )
        {
            /* First release the target threads perQData */
            PERQDATA_Release( pTgtMsgQ->pQData );

            /* Share a reference to the source threads perQDATA */
            PERQDATA_Addref( pSrcMsgQ->pQData );
            pTgtMsgQ->pQData = pSrcMsgQ->pQData;
        }
    }
    else    /* Detach threads */
    {
        /* Only detach if currently attached */
        if ( pTgtMsgQ->pQData == pSrcMsgQ->pQData )
        {
            /* First release the target threads perQData */
            PERQDATA_Release( pTgtMsgQ->pQData );

            /* Give the target thread its own private perQDATA once more */
            pTgtMsgQ->pQData = PERQDATA_CreateInstance();
        }
    }

    /* TODO: Reset the Key State */

    bRet = 1;      /* Success */

CLEANUP:

    /* Unlock the queues before returning */
    if ( pSrcMsgQ )
        QUEUE_Unlock( pSrcMsgQ );
    if ( pTgtMsgQ )
        QUEUE_Unlock( pTgtMsgQ );

    return bRet;
}
