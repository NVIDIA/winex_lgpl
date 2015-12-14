/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright (c) 2000-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>	/* Insomnia - pow() function */

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "mmsystem.h"
#include "winternl.h"
#include "mmddk.h"
#include "wine/windef16.h"
#include "wine/debug.h"
#include "dsound.h"
#include "dsdriver.h"
#include "dsound_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dsound);

/* stereo pan calculation may take considerable CPU resources...
 * disabling it may improve performance, but won't be quite as fun */
#define CALC_PANNING

static inline D3DVALUE Distance3D(const D3DVECTOR *v)
{
	return sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
}

static inline void Normalize3D(D3DVECTOR *v)
{
	D3DVALUE d = Distance3D(v);
	if (d == 0.0) return;
	v->x /= d;
	v->y /= d;
	v->z /= d;
}

static inline void CrossProduct3D(const D3DVECTOR *v1, const D3DVECTOR *v2, D3DVECTOR *v3)
{
	v3->x = v1->y * v2->z - v1->z * v2->y;
	v3->y = v1->z * v2->x - v1->x * v2->z;
	v3->z = v1->x * v2->y - v1->y * v2->x;
}

static inline D3DVALUE DotProduct3D( const D3DVECTOR *v1, const D3DVECTOR *v2)
{
	return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

void DSOUND_Recalc3DBuffer(IDirectSoundBufferImpl *dsb)
{
	IDirectSound3DBufferImpl *ds3db = dsb->ds3db;
	IDirectSound3DListenerImpl *dsl = dsb->dsound->listener;
	DSVOLUMEPAN *volpan = &dsb->volpan;
	D3DVECTOR wp, lp;
	D3DVALUE r, d, p;
	LONG vol, rvol;

	if (!dsl) {
		DSOUND_RecalcVolPan(volpan);
		return;
	}

	/* compute distance between source and listener */
	switch (ds3db->ds3db.dwMode) {
	case DS3DMODE_NORMAL:
		wp.x = ds3db->ds3db.vPosition.x - dsl->ds3dl.vPosition.x;
		wp.y = ds3db->ds3db.vPosition.y - dsl->ds3dl.vPosition.y;
		wp.z = ds3db->ds3db.vPosition.z - dsl->ds3dl.vPosition.z;
		r = Distance3D(&wp);
		break;
	case DS3DMODE_HEADRELATIVE:
		wp.x = ds3db->ds3db.vPosition.x;
		wp.y = ds3db->ds3db.vPosition.y;
		wp.z = ds3db->ds3db.vPosition.z;
		r = Distance3D(&wp);
		break;
	case DS3DMODE_DISABLE:
	default:
		volpan->lVolume = ds3db->lVolume;
		volpan->lPan = 0;
		DSOUND_RecalcVolPan(volpan);
		return;
	}
	TRACE("range: %f (%f,%f,%f)\n", r, wp.x, wp.y, wp.z);

	if (r >= ds3db->ds3db.flMaxDistance) {
		if (dsb->dsbd.dwFlags & DSBCAPS_MUTE3DATMAXDISTANCE) {
			volpan->lVolume = DSBVOLUME_MIN;
			volpan->lPan = 0;
			volpan->dwVolAmpFactor = 0;
			/* FIXME: dwPan{Left|Right}AmpFactor */
			volpan->dwTotalLeftAmpFactor = 0;
			volpan->dwTotalRightAmpFactor = 0;
			TRACE("muted\n");
			return;
		}
		r = ds3db->ds3db.flMaxDistance;
	}

	/* the minimum distance controls how fast the sound diminishes */
	r = r * dsl->ds3dl.flRolloffFactor / ds3db->ds3db.flMinDistance;
	if (r < 1.0) r = 1.0;
	vol = -(sqrt(r)-1.0) * 600;

	/* FIXME: calculate sound cone stuff */

	/* FIXME: more light-weight calculations? */
	volpan->lVolume = ds3db->lVolume + vol;

	/* remaining volume range */
	rvol = volpan->lVolume - DSBVOLUME_MIN;

#ifdef CALC_PANNING
	/* convert to head-relative coordinates */
	if (ds3db->ds3db.dwMode == DS3DMODE_NORMAL) {
		lp.x = DotProduct3D(&wp, &dsl->vOrientRight);
		lp.y = DotProduct3D(&wp, &dsl->ds3dl.vOrientTop);
		lp.z = DotProduct3D(&wp, &dsl->ds3dl.vOrientFront);
	}
	else {
		lp.x = wp.x;
		lp.y = wp.y;
		lp.z = wp.z;
	}

	/* calculate stereo panning */
	d = sqrt(lp.y*lp.y + lp.z*lp.z);
	if (d == 0.0) p = 0.0;
	else p = lp.x / d;
	/* p is now the sine of the angle at which the sound arrives */

	/* FIXME: I doubt this is mathematically the right thing... */
	volpan->lPan = 1200 * p;

	TRACE("panning: %f (%f,%f,%f)\n", p, lp.x, lp.y, lp.z);
#else
	volpan->lPan = 0;
#endif

	DSOUND_RecalcVolPan(volpan);

	/* FIXME: calculate doppler shift */
}

static void DSOUND_RecalcAllBuffers(IDirectSoundImpl *dsound, BOOL force)
{
	INT			i;
	IDirectSoundBufferImpl *dsb;
	IDirectSound3DBufferImpl *ds3db;

	RtlAcquireResourceShared(&dsound->lock, TRUE);
	for (i = dsound->nrofbuffers - 1; i >= 0; i--) {
		dsb = dsound->buffers[i];

		if (!dsb || !(ICOM_VTBL(dsb)) || !(ds3db = dsb->ds3db))
			continue;
		if (ds3db->need_recalc || force) {
			TRACE("Recalculating %p\n", dsb);
			DSOUND_Recalc3DBuffer(dsb);
			ds3db->need_recalc = FALSE;
			if (!force) DSOUND_ForceRemix(dsb);
		}
	}

	if (force) {
		dsound->need_remix = TRUE;
		dsound->listener->need_recalc = FALSE;
	}
	RtlReleaseResource(&dsound->lock);
}

static void DSOUND_ApplyBuffer(IDirectSound3DBufferImpl *ds3db, DWORD dwApply)
{
	IDirectSoundBufferImpl *dsb = ds3db->dsb;
	if (dwApply == DS3D_IMMEDIATE) {
		DSOUND_Recalc3DBuffer(dsb);
		ds3db->need_recalc = FALSE;
		DSOUND_ForceRemix(dsb);
	}
	else ds3db->need_recalc = TRUE;
}

static void DSOUND_ApplyListener(IDirectSound3DListenerImpl *dsl, DWORD dwApply)
{
	if (dwApply == DS3D_IMMEDIATE) {
		DSOUND_RecalcAllBuffers(dsl->dsb->dsound, TRUE);
	}
	else dsl->need_recalc = TRUE;
}

static void DSOUND_UpdateListenerOrientation(IDirectSound3DListenerImpl *dsl)
{
	D3DVECTOR *v1, *v2, *v3;
	D3DVALUE f;

	v1 = &dsl->ds3dl.vOrientFront;
	v2 = &dsl->ds3dl.vOrientTop;
	v3 = &dsl->vOrientRight;
	Normalize3D(v2);
	/* adjust vOrientFront to be orthogonal to vOrientTop */
	f = DotProduct3D(v1, v2);
	v1->x -= f * v2->x;
	v1->y -= f * v2->y;
	v1->z -= f * v2->z;
	Normalize3D(v1);
	/* compute vOrientRight as the cross product of vOrientTop and vOrientFront */
	CrossProduct3D(v2, v1, v3);
}


/*******************************************************************************
 *              IDirectSound3DBuffer
 */

/* IUnknown methods */
static HRESULT WINAPI IDirectSound3DBufferImpl_QueryInterface(
	LPDIRECTSOUND3DBUFFER iface, REFIID riid, LPVOID *ppobj)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);

	TRACE("(%p,%s,%p)\n",This,debugstr_guid(riid),ppobj);
	return IDirectSoundBuffer_QueryInterface((LPDIRECTSOUNDBUFFER8)This->dsb, riid, ppobj);
}

static ULONG WINAPI IDirectSound3DBufferImpl_AddRef(LPDIRECTSOUND3DBUFFER iface)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	ULONG ulReturn;

	TRACE("(%p) ref was %ld\n", This, This->ref);
	
	IDirectSoundBufferImpl_AddRefAggregate((LPDIRECTSOUNDBUFFER8)This->dsb);

	ulReturn = InterlockedIncrement(&(This->ref));
	
	return ulReturn;
}

static ULONG WINAPI IDirectSound3DBufferImpl_Release(LPDIRECTSOUND3DBUFFER iface)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	ULONG ulReturn=0;

	TRACE("(%p) ref was %ld\n", This, This->ref);
	/* NOTE: due to the aggregation of SoundBuffers and Sound3DBuffers, it is possible
	   that there are aggregate refs, but no actual refs on this buffer, so to 
	   prevent negative refcounts... (as tested on WinXP) */
	if( This->ref > 0 )
		ulReturn = InterlockedDecrement(&(This->ref));
	
	IDirectSoundBufferImpl_ReleaseAggregate((LPDIRECTSOUNDBUFFER8)This->dsb);
	
	return ulReturn;
}

/* IDirectSound3DBuffer methods */
static HRESULT WINAPI IDirectSound3DBufferImpl_GetAllParameters(
	LPDIRECTSOUND3DBUFFER iface,
	LPDS3DBUFFER lpDs3dBuffer)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpDs3dBuffer);

	if (lpDs3dBuffer == NULL)
		return DSERR_INVALIDPARAM;

	*lpDs3dBuffer = This->ds3db;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetConeAngles(
	LPDIRECTSOUND3DBUFFER iface,
	LPDWORD lpdwInsideConeAngle,
	LPDWORD lpdwOutsideConeAngle)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p,%p)\n", This, lpdwInsideConeAngle, lpdwOutsideConeAngle);
	*lpdwInsideConeAngle = This->ds3db.dwInsideConeAngle;
	*lpdwOutsideConeAngle = This->ds3db.dwOutsideConeAngle;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetConeOrientation(
	LPDIRECTSOUND3DBUFFER iface,
	LPD3DVECTOR lpvConeOrientation)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpvConeOrientation);
	*lpvConeOrientation = This->ds3db.vConeOrientation;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetConeOutsideVolume(
	LPDIRECTSOUND3DBUFFER iface,
	LPLONG lplConeOutsideVolume)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lplConeOutsideVolume);
	*lplConeOutsideVolume = This->ds3db.lConeOutsideVolume;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetMaxDistance(
	LPDIRECTSOUND3DBUFFER iface,
	LPD3DVALUE lpfMaxDistance)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpfMaxDistance);
	*lpfMaxDistance = This->ds3db.flMaxDistance;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetMinDistance(
	LPDIRECTSOUND3DBUFFER iface,
	LPD3DVALUE lpfMinDistance)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpfMinDistance);
	*lpfMinDistance = This->ds3db.flMinDistance;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetMode(
	LPDIRECTSOUND3DBUFFER iface,
	LPDWORD lpdwMode)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpdwMode);
	*lpdwMode = This->ds3db.dwMode;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetPosition(
	LPDIRECTSOUND3DBUFFER iface,
	LPD3DVECTOR lpvPosition)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpvPosition);
	*lpvPosition = This->ds3db.vPosition;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_GetVelocity(
	LPDIRECTSOUND3DBUFFER iface,
	LPD3DVECTOR lpvVelocity)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpvVelocity);
	*lpvVelocity = This->ds3db.vVelocity;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetAllParameters(
	LPDIRECTSOUND3DBUFFER iface,
	LPCDS3DBUFFER lpcDs3dBuffer,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%p,%ld)\n", This, lpcDs3dBuffer, dwApply);
	This->ds3db = *lpcDs3dBuffer;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetConeAngles(
	LPDIRECTSOUND3DBUFFER iface,
	DWORD dwInsideConeAngle,
	DWORD dwOutsideConeAngle,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%ld,%ld,%ld)\n", This, dwInsideConeAngle, dwOutsideConeAngle, dwApply);
	This->ds3db.dwInsideConeAngle = dwInsideConeAngle;
	This->ds3db.dwOutsideConeAngle = dwOutsideConeAngle;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetConeOrientation(
	LPDIRECTSOUND3DBUFFER iface,
	D3DVALUE x, D3DVALUE y, D3DVALUE z,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%f,%f,%f,%ld)\n", This, x, y, z, dwApply);
	This->ds3db.vConeOrientation.x = x;
	This->ds3db.vConeOrientation.y = y;
	This->ds3db.vConeOrientation.z = z;
	Normalize3D(&This->ds3db.vConeOrientation);
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetConeOutsideVolume(
	LPDIRECTSOUND3DBUFFER iface,
	LONG lConeOutsideVolume,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%ld,%ld)\n", This, lConeOutsideVolume, dwApply);
	This->ds3db.lConeOutsideVolume = lConeOutsideVolume;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetMaxDistance(
	LPDIRECTSOUND3DBUFFER iface,
	D3DVALUE fMaxDistance,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%f,%ld)\n", This, fMaxDistance, dwApply);
	This->ds3db.flMaxDistance = fMaxDistance;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetMinDistance(
	LPDIRECTSOUND3DBUFFER iface,
	D3DVALUE fMinDistance,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%f,%ld)\n", This, fMinDistance, dwApply);
	This->ds3db.flMinDistance = fMinDistance;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetMode(
	LPDIRECTSOUND3DBUFFER iface,
	DWORD dwMode,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%ld,%ld)\n", This, dwMode, dwApply);
	This->ds3db.dwMode = dwMode;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetPosition(
	LPDIRECTSOUND3DBUFFER iface,
	D3DVALUE x, D3DVALUE y, D3DVALUE z,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%f,%f,%f,%ld)\n", This, x, y, z, dwApply);
	This->ds3db.vPosition.x = x;
	This->ds3db.vPosition.y = y;
	This->ds3db.vPosition.z = z;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DBufferImpl_SetVelocity(
	LPDIRECTSOUND3DBUFFER iface,
	D3DVALUE x, D3DVALUE y, D3DVALUE z,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DBufferImpl,iface);
	TRACE("(%p)->(%f,%f,%f,%ld)\n", This, x, y, z, dwApply);
	This->ds3db.vVelocity.x = x;
	This->ds3db.vVelocity.y = y;
	This->ds3db.vVelocity.z = z;
	DSOUND_ApplyBuffer(This, dwApply);
	return DS_OK;
}

static ICOM_VTABLE(IDirectSound3DBuffer) ds3dbvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown methods */
	IDirectSound3DBufferImpl_QueryInterface,
	IDirectSound3DBufferImpl_AddRef,
	IDirectSound3DBufferImpl_Release,
	/* IDirectSound3DBuffer methods */
	IDirectSound3DBufferImpl_GetAllParameters,
	IDirectSound3DBufferImpl_GetConeAngles,
	IDirectSound3DBufferImpl_GetConeOrientation,
	IDirectSound3DBufferImpl_GetConeOutsideVolume,
	IDirectSound3DBufferImpl_GetMaxDistance,
	IDirectSound3DBufferImpl_GetMinDistance,
	IDirectSound3DBufferImpl_GetMode,
	IDirectSound3DBufferImpl_GetPosition,
	IDirectSound3DBufferImpl_GetVelocity,
	IDirectSound3DBufferImpl_SetAllParameters,
	IDirectSound3DBufferImpl_SetConeAngles,
	IDirectSound3DBufferImpl_SetConeOrientation,
	IDirectSound3DBufferImpl_SetConeOutsideVolume,
	IDirectSound3DBufferImpl_SetMaxDistance,
	IDirectSound3DBufferImpl_SetMinDistance,
	IDirectSound3DBufferImpl_SetMode,
	IDirectSound3DBufferImpl_SetPosition,
	IDirectSound3DBufferImpl_SetVelocity,
};

HRESULT WINAPI IDirectSound3DBufferImpl_Create(
	IDirectSoundBufferImpl *This,
	IDirectSound3DBufferImpl **pds3db)
{
	IDirectSound3DBufferImpl *ds3db;

	ds3db = (IDirectSound3DBufferImpl*)HeapAlloc(dsound_heap,0,sizeof(*ds3db));
	/* due to aggregation this object exists, but doesn't have any references
	 * until explicitly requested by the application */
	ds3db->ref = 0;
	ds3db->dsb = This;
	ICOM_VTBL(ds3db) = &ds3dbvt;
	CRITICAL_SECTION_DEFINE(&ds3db->lock);
	ds3db->need_recalc = FALSE;

	ds3db->ds3db.dwSize = sizeof(DS3DBUFFER);
	ds3db->ds3db.vPosition.x = 0.0;
	ds3db->ds3db.vPosition.y = 0.0;
	ds3db->ds3db.vPosition.z = 0.0;
	ds3db->ds3db.vVelocity.x = 0.0;
	ds3db->ds3db.vVelocity.y = 0.0;
	ds3db->ds3db.vVelocity.z = 0.0;
	ds3db->ds3db.dwInsideConeAngle = DS3D_DEFAULTCONEANGLE;
	ds3db->ds3db.dwOutsideConeAngle = DS3D_DEFAULTCONEANGLE;
	ds3db->ds3db.vConeOrientation.x = 0.0;
	ds3db->ds3db.vConeOrientation.y = 0.0;
	ds3db->ds3db.vConeOrientation.z = 0.0;
	ds3db->ds3db.lConeOutsideVolume = DS3D_DEFAULTCONEOUTSIDEVOLUME;
	ds3db->ds3db.flMinDistance = DS3D_DEFAULTMINDISTANCE;
	ds3db->ds3db.flMaxDistance = DS3D_DEFAULTMAXDISTANCE;
	ds3db->ds3db.dwMode = DS3DMODE_NORMAL;

	*pds3db = ds3db;
	TRACE("created 3D buffer %p for sound buffer %p\n", ds3db, This);
	return S_OK;
}

/*******************************************************************************
 *	      IDirectSound3DListener
 */

/* IUnknown methods */
static HRESULT WINAPI IDirectSound3DListenerImpl_QueryInterface(
	LPDIRECTSOUND3DLISTENER iface, REFIID riid, LPVOID *ppobj)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);

	TRACE("(%p,%s,%p)\n",This,debugstr_guid(riid),ppobj);
	return IDirectSoundBuffer_QueryInterface((LPDIRECTSOUNDBUFFER8)This->dsb, riid, ppobj);
}

static ULONG WINAPI IDirectSound3DListenerImpl_AddRef(LPDIRECTSOUND3DLISTENER iface)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI IDirectSound3DListenerImpl_Release(LPDIRECTSOUND3DLISTENER iface)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	ULONG ulReturn;

	TRACE("(%p) ref was %ld\n", This, This->ref);

	ulReturn = InterlockedDecrement(&This->ref);

	/* Free all resources */
	if( ulReturn == 0 ) {
		if(This->dsb)
			IDirectSoundBuffer8_Release((LPDIRECTSOUNDBUFFER8)This->dsb);
		DeleteCriticalSection(&This->lock);
		HeapFree(dsound_heap,0,This);
	}

	return ulReturn;
}

/* IDirectSound3DListener methods */
static HRESULT WINAPI IDirectSound3DListenerImpl_GetAllParameter(
	LPDIRECTSOUND3DLISTENER iface,
	LPDS3DLISTENER lpDS3DL)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpDS3DL);

	if (lpDS3DL == NULL)
		return DSERR_INVALIDPARAM;

	*lpDS3DL = This->ds3dl;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_GetDistanceFactor(
	LPDIRECTSOUND3DLISTENER iface,
	LPD3DVALUE lpfDistanceFactor)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpfDistanceFactor);
	*lpfDistanceFactor = This->ds3dl.flDistanceFactor;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_GetDopplerFactor(
	LPDIRECTSOUND3DLISTENER iface,
	LPD3DVALUE lpfDopplerFactor)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpfDopplerFactor);
	*lpfDopplerFactor = This->ds3dl.flDopplerFactor;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_GetOrientation(
	LPDIRECTSOUND3DLISTENER iface,
	LPD3DVECTOR lpvOrientFront,
	LPD3DVECTOR lpvOrientTop)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p,%p)\n", This, lpvOrientFront, lpvOrientTop);
	*lpvOrientFront = This->ds3dl.vOrientFront;
	*lpvOrientTop = This->ds3dl.vOrientTop;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_GetPosition(
	LPDIRECTSOUND3DLISTENER iface,
	LPD3DVECTOR lpvPosition)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpvPosition);
	*lpvPosition = This->ds3dl.vPosition;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_GetRolloffFactor(
	LPDIRECTSOUND3DLISTENER iface,
	LPD3DVALUE lpfRolloffFactor)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpfRolloffFactor);
	*lpfRolloffFactor = This->ds3dl.flRolloffFactor;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_GetVelocity(
	LPDIRECTSOUND3DLISTENER iface,
	LPD3DVECTOR lpvVelocity)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p)\n", This, lpvVelocity);
	*lpvVelocity = This->ds3dl.vVelocity;
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetAllParameters(
	LPDIRECTSOUND3DLISTENER iface,
	LPCDS3DLISTENER lpcDS3DL,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%p,%ld)\n", This, lpcDS3DL, dwApply);
	This->ds3dl = *lpcDS3DL;
	DSOUND_UpdateListenerOrientation(This);
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetDistanceFactor(
	LPDIRECTSOUND3DLISTENER iface,
	D3DVALUE fDistanceFactor,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%f,%ld)\n", This, fDistanceFactor, dwApply);
	This->ds3dl.flDistanceFactor = fDistanceFactor;
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetDopplerFactor(
	LPDIRECTSOUND3DLISTENER iface,
	D3DVALUE fDopplerFactor,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%f,%ld)\n", This, fDopplerFactor, dwApply);
	This->ds3dl.flDopplerFactor = fDopplerFactor;
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetOrientation(
	LPDIRECTSOUND3DLISTENER iface,
	D3DVALUE xFront, D3DVALUE yFront, D3DVALUE zFront,
	D3DVALUE xTop, D3DVALUE yTop, D3DVALUE zTop,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%f,%f,%f,%f,%f,%f,%ld)\n", This, xFront, yFront, zFront, xTop, yTop, zTop, dwApply);
	This->ds3dl.vOrientFront.x = xFront;
	This->ds3dl.vOrientFront.y = yFront;
	This->ds3dl.vOrientFront.z = zFront;
	This->ds3dl.vOrientTop.x = xTop;
	This->ds3dl.vOrientTop.y = yTop;
	This->ds3dl.vOrientTop.z = zTop;
	DSOUND_UpdateListenerOrientation(This);
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetPosition(
	LPDIRECTSOUND3DLISTENER iface,
	D3DVALUE x, D3DVALUE y, D3DVALUE z,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%f,%f,%f,%ld)\n", This, x, y, z, dwApply);
	This->ds3dl.vPosition.x = x;
	This->ds3dl.vPosition.y = y;
	This->ds3dl.vPosition.z = z;
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetRolloffFactor(
	LPDIRECTSOUND3DLISTENER iface,
	D3DVALUE fRolloffFactor,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%f,%ld)\n", This, fRolloffFactor, dwApply);
	This->ds3dl.flRolloffFactor = fRolloffFactor;
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_SetVelocity(
	LPDIRECTSOUND3DLISTENER iface,
	D3DVALUE x, D3DVALUE y, D3DVALUE z,
	DWORD dwApply)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)->(%f,%f,%f,%ld)\n", This, x, y, z, dwApply);
	This->ds3dl.vVelocity.x = x;
	This->ds3dl.vVelocity.y = y;
	This->ds3dl.vVelocity.z = z;
	DSOUND_ApplyListener(This, dwApply);
	return DS_OK;
}

static HRESULT WINAPI IDirectSound3DListenerImpl_CommitDeferredSettings(
	LPDIRECTSOUND3DLISTENER iface)
{
	ICOM_THIS(IDirectSound3DListenerImpl,iface);
	TRACE("(%p)\n", This);
	DSOUND_RecalcAllBuffers(This->dsb->dsound, This->need_recalc);
	return DS_OK;
}

static ICOM_VTABLE(IDirectSound3DListener) ds3dlvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown methods */
	IDirectSound3DListenerImpl_QueryInterface,
	IDirectSound3DListenerImpl_AddRef,
	IDirectSound3DListenerImpl_Release,
	/* IDirectSound3DListener methods */
	IDirectSound3DListenerImpl_GetAllParameter,
	IDirectSound3DListenerImpl_GetDistanceFactor,
	IDirectSound3DListenerImpl_GetDopplerFactor,
	IDirectSound3DListenerImpl_GetOrientation,
	IDirectSound3DListenerImpl_GetPosition,
	IDirectSound3DListenerImpl_GetRolloffFactor,
	IDirectSound3DListenerImpl_GetVelocity,
	IDirectSound3DListenerImpl_SetAllParameters,
	IDirectSound3DListenerImpl_SetDistanceFactor,
	IDirectSound3DListenerImpl_SetDopplerFactor,
	IDirectSound3DListenerImpl_SetOrientation,
	IDirectSound3DListenerImpl_SetPosition,
	IDirectSound3DListenerImpl_SetRolloffFactor,
	IDirectSound3DListenerImpl_SetVelocity,
	IDirectSound3DListenerImpl_CommitDeferredSettings,
};

HRESULT WINAPI IDirectSound3DListenerImpl_Create(
	PrimaryBufferImpl *This,
	IDirectSound3DListenerImpl **pdsl)
{
	IDirectSound3DListenerImpl *dsl;

	dsl = (IDirectSound3DListenerImpl*)HeapAlloc(dsound_heap,0,sizeof(*dsl));
	dsl->ref = 1;
	ICOM_VTBL(dsl) = &ds3dlvt;
	dsl->need_recalc = FALSE;

	dsl->ds3dl.dwSize = sizeof(DS3DLISTENER);
	dsl->ds3dl.vPosition.x = 0.0;
	dsl->ds3dl.vPosition.y = 0.0;
	dsl->ds3dl.vPosition.z = 0.0;
	dsl->ds3dl.vVelocity.x = 0.0;
	dsl->ds3dl.vVelocity.y = 0.0;
	dsl->ds3dl.vVelocity.z = 0.0;
	dsl->ds3dl.vOrientFront.x = 0.0;
	dsl->ds3dl.vOrientFront.y = 0.0;
	dsl->ds3dl.vOrientFront.z = 1.0;
	dsl->ds3dl.vOrientTop.x = 0.0;
	dsl->ds3dl.vOrientTop.y = 1.0;
	dsl->ds3dl.vOrientTop.z = 0.0;
	dsl->ds3dl.flDistanceFactor = DS3D_DEFAULTDISTANCEFACTOR;
	dsl->ds3dl.flRolloffFactor = DS3D_DEFAULTROLLOFFFACTOR;
	dsl->ds3dl.flDopplerFactor = DS3D_DEFAULTDOPPLERFACTOR;

	CRITICAL_SECTION_DEFINE(&dsl->lock);

	dsl->dsb = This;
	IDirectSoundBuffer8_AddRef((LPDIRECTSOUNDBUFFER8)This);

	*pdsl = dsl;
	return S_OK;
}
