/*  			DirectSound
 *
 * Copyright (c) 2000-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "config.h"

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/windef16.h"
#include "wine/debug.h"
#include "dsound.h"
#include "dsdriver.h"
#include "dsound_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dsound);


/*******************************************************************************
 *		IDirectSoundNotify
 */
static HRESULT WINAPI IDirectSoundNotifyImpl_QueryInterface(
    LPDIRECTSOUNDNOTIFY iface,REFIID riid,LPVOID *ppobj
) {
    ICOM_THIS(IDirectSoundNotifyImpl,iface);

    TRACE("(%p,%s,%p)\n",This,debugstr_guid(riid),ppobj);


    /* don't allow a new notification object to be created by the parent object */
    if ( IsEqualGUID( &IID_IUnknown, riid ) ||
         IsEqualGUID( &IID_IDirectSoundNotify, riid ))
    {
        IDirectSoundNotify_AddRef(iface);

        *ppobj = (LPVOID)iface;

        return S_OK;
    }


    /* request the object from the parent buffer object */
    if (This->dsb)
        return IDirectSoundBuffer8_QueryInterface((LPDIRECTSOUNDBUFFER8)This->dsb, riid, ppobj);

    else if (This->dscb)
        return IDirectSoundCaptureBuffer8_QueryInterface((LPDIRECTSOUNDCAPTUREBUFFER8)This->dscb, riid, ppobj);


    return E_NOINTERFACE;
}

static ULONG WINAPI IDirectSoundNotifyImpl_AddRef(LPDIRECTSOUNDNOTIFY iface) {
    ICOM_THIS(IDirectSoundNotifyImpl,iface);
    ULONG ulReturn;

    TRACE("(%p) ref was %ld\n", This, This->ref);
    
    if (This->dsb)
        IDirectSoundBufferImpl_AddRefAggregate((LPDIRECTSOUNDBUFFER8)This->dsb);

    else if (This->dscb)
        IDirectSoundCaptureBufferImpl_AddRef((LPDIRECTSOUNDCAPTUREBUFFER8)This->dscb);


    ulReturn = InterlockedIncrement((LONG *)&(This->ref));
    
    return ulReturn;
}

static ULONG WINAPI IDirectSoundNotifyImpl_Release(LPDIRECTSOUNDNOTIFY iface) {
    ICOM_THIS(IDirectSoundNotifyImpl,iface);
    ULONG ulReturn=0;

    TRACE("(%p) ref was %ld\n", This, This->ref);

    /* NOTE: due to the aggregation of SoundBuffers and Sound3DBuffers, it is possible
       that there are aggregate refs, but no actual refs on this buffer, so to 
       prevent negative refcounts... */
    if( This->ref > 0 )
        ulReturn = InterlockedDecrement((LONG *)&(This->ref));


    if (This->dsb)
        IDirectSoundBufferImpl_ReleaseAggregate((LPDIRECTSOUNDBUFFER8)This->dsb);

    else if (This->dscb)
        IDirectSoundCaptureBufferImpl_Release((LPDIRECTSOUNDCAPTUREBUFFER8)This->dscb);


    if (This->ref <= 0){
        TRACE("this notification object is being destroyed {This = %p}\n", This);


        HeapFree(dsound_heap, 0, This);
    }
    
    
    return ulReturn;
}

static HRESULT WINAPI IDirectSoundNotifyImpl_SetNotificationPositions(
    LPDIRECTSOUNDNOTIFY     iface,
    DWORD                   howmuch,
    LPCDSBPOSITIONNOTIFY    notify)
{
    ICOM_THIS(IDirectSoundNotifyImpl,iface);
    IDirectSoundNotifyPositions *   positions = NULL;
    int	                            i;


    if (TRACE_ON(dsound)) {
        TRACE("(%p, 0x%08lx, %p)\n", This, howmuch, notify);

        for (i = 0; i < howmuch; i++){
            TRACE("notify at %ld to 0x%08lx\n",
                     notify[i].dwOffset, (DWORD)notify[i].hEventNotify);
        }
    }

    

    /* grab the appropriate positions list from my owning buffer object */
    if (This->dsb)
    {

        /* buffer objects should always have the notification positions list set up on creation
           if the DSBCAPS_CTRLPOSITIONNOTIFY flag was included in the creation flags.  If it was
           not included, a notification object cannot be retrieved from the buffer object at all. */
        if (This->dsb->positions == NULL)
        {
            ERR("no positions list has been set for the owning buffer object... something went horribly wrong\n");

            return E_FAIL;
        }

        positions = This->dsb->positions;
    }

    else if (This->dscb)
    {

        /* a capture object does not have a positions list set until the first notification object
           is requested from it.  There is no way to specify at creation time that the capture buffer
           should have a notification object related to it.  Attempting to grab a notification object
           from a capture buffer should always succeed. */
        if (This->dscb->positions == NULL)
        {
            /* create the positions list object */
            This->dscb->positions = (IDirectSoundNotifyPositions *)HeapAlloc(dsound_heap, 0, sizeof(IDirectSoundNotifyPositions));
            This->dscb->positions->refCount = 1;
            This->dscb->positions->notifies = NULL;
            This->dscb->positions->nrofnotifies = 0;
        }

        positions = This->dscb->positions;
    }

    else
        ERR("no owning buffer object has been set for this notification object!\n");


    if (positions == NULL)
    {
        ERR("no positions array has been set?!?!\n");

        return E_FAIL;
    }


    /* copy the notification positions list into the buffer object's positions list */
    positions->notifies = HeapReAlloc(dsound_heap, 0, positions->notifies, howmuch * sizeof(DSBPOSITIONNOTIFY));

    memcpy(positions->notifies, notify, howmuch * sizeof(DSBPOSITIONNOTIFY));
    positions->nrofnotifies = howmuch;


    return S_OK;
}


static ICOM_VTABLE(IDirectSoundNotify) dsnvt =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IDirectSoundNotifyImpl_QueryInterface,
    IDirectSoundNotifyImpl_AddRef,
    IDirectSoundNotifyImpl_Release,
    IDirectSoundNotifyImpl_SetNotificationPositions,
};


HRESULT WINAPI IDirectSoundNotifyImpl_Create(
    IDirectSoundBufferImpl *        buffer,
    IDirectSoundCaptureBufferImpl * capture,
    IDirectSoundNotifyImpl **       pnotify)
{
    IDirectSoundNotifyImpl *dsn;


    TRACE("creating a sound notify object {buffer = %p, capture = %p}\n", buffer, capture);

    if (buffer && capture){
        ERR("only one of the buffer objects may be specified when creating a notification object!  Assuming an IDirectSoundBuffer object\n");

        capture = NULL;
    }


    dsn = (IDirectSoundNotifyImpl *)HeapAlloc(dsound_heap, 0, sizeof(*dsn));
    dsn->ref = 1;
    dsn->dsb = buffer;
    dsn->dscb = capture;

    if (buffer)
        IDirectSoundBufferImpl_AddRefAggregate((LPDIRECTSOUNDBUFFER8)buffer);

    if (capture)
        IDirectSoundCaptureBufferImpl_AddRef((LPDIRECTSOUNDCAPTUREBUFFER8)capture);

    
    ICOM_VTBL(dsn) = &dsnvt;

    *pnotify = dsn;
    TRACE("created the notification object %p\n", dsn);
    return S_OK;
}

HRESULT WINAPI IDirectSoundNotifyImpl_Duplicate(
    IDirectSoundBufferImpl *    original,
    IDirectSoundBufferImpl *    owner)
{
    TRACE("duplicating the sound notify object %p for the buffer %p\n", original, owner);

    if (owner == NULL){
        ERR("no owning buffer object was specified\n");

        return E_FAIL;
    }


    /* share the notification positions list between the two objects */
    owner->positions = original->positions;

    /* increment the ref count for the shared notification positions list */
    if (owner->positions)
        InterlockedIncrement((LONG *)&(owner->positions->refCount));

    return S_OK;
}
