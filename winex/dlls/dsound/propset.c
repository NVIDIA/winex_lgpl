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


/*******************************************************************************
 *              IKsPropertySet
 */

/* IUnknown methods */
static HRESULT WINAPI IKsPropertySetImpl_QueryInterface(
	LPKSPROPERTYSET iface, REFIID riid, LPVOID *ppobj
) {
	ICOM_THIS(IKsPropertySetImpl,iface);

	TRACE("(%p,%s,%p)\n",This,debugstr_guid(riid),ppobj);
	return IDirectSoundBuffer_QueryInterface((LPDIRECTSOUNDBUFFER8)This->dsb, riid, ppobj);
}

static ULONG WINAPI IKsPropertySetImpl_AddRef(LPKSPROPERTYSET iface) {
	ICOM_THIS(IKsPropertySetImpl,iface);
	ULONG ulReturn;

	ulReturn = InterlockedIncrement(&This->ref);
	if (ulReturn == 1)
		IDirectSoundBuffer_AddRef((LPDIRECTSOUND3DBUFFER)This->dsb);
	return ulReturn;
}

static ULONG WINAPI IKsPropertySetImpl_Release(LPKSPROPERTYSET iface) {
	ICOM_THIS(IKsPropertySetImpl,iface);
	ULONG ulReturn;

	ulReturn = InterlockedDecrement(&This->ref);
	if (ulReturn)
		return ulReturn;
	IDirectSoundBuffer_Release((LPDIRECTSOUND3DBUFFER)This->dsb);
	return 0;
}

static HRESULT WINAPI IKsPropertySetImpl_Get(LPKSPROPERTYSET iface,
	REFGUID guidPropSet, ULONG dwPropID,
	LPVOID pInstanceData, ULONG cbInstanceData,
	LPVOID pPropData, ULONG cbPropData,
	PULONG pcbReturned
) {
	ICOM_THIS(IKsPropertySetImpl,iface);

	FIXME("(%p,%s,%ld,%p,%ld,%p,%ld,%p), stub!\n",This,debugstr_guid(guidPropSet),dwPropID,pInstanceData,cbInstanceData,pPropData,cbPropData,pcbReturned);
	return E_PROP_ID_UNSUPPORTED;
}

static HRESULT WINAPI IKsPropertySetImpl_Set(LPKSPROPERTYSET iface,
	REFGUID guidPropSet, ULONG dwPropID,
	LPVOID pInstanceData, ULONG cbInstanceData,
	LPVOID pPropData, ULONG cbPropData
) {
	ICOM_THIS(IKsPropertySetImpl,iface);

	FIXME("(%p,%s,%ld,%p,%ld,%p,%ld), stub!\n",This,debugstr_guid(guidPropSet),dwPropID,pInstanceData,cbInstanceData,pPropData,cbPropData);
	return E_PROP_ID_UNSUPPORTED;
}

static HRESULT WINAPI IKsPropertySetImpl_QuerySupport(LPKSPROPERTYSET iface,
	REFGUID guidPropSet, ULONG dwPropID, PULONG pTypeSupport
) {
	ICOM_THIS(IKsPropertySetImpl,iface);

	FIXME("(%p,%s,%ld,%p), stub!\n",This,debugstr_guid(guidPropSet),dwPropID,pTypeSupport);
	return E_PROP_ID_UNSUPPORTED;
}

static ICOM_VTABLE(IKsPropertySet) iksvt = {
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IKsPropertySetImpl_QueryInterface,
	IKsPropertySetImpl_AddRef,
	IKsPropertySetImpl_Release,
	IKsPropertySetImpl_Get,
	IKsPropertySetImpl_Set,
	IKsPropertySetImpl_QuerySupport
};

HRESULT WINAPI IKsPropertySetImpl_Create(
	IDirectSoundBufferImpl *This,
	IKsPropertySetImpl **piks)
{
	IKsPropertySetImpl *iks;

	iks = (IKsPropertySetImpl*)HeapAlloc(dsound_heap,0,sizeof(*iks));
	iks->ref = 0;
	iks->dsb = This;
	ICOM_VTBL(iks) = &iksvt;

	*piks = iks;
	return S_OK;
}
