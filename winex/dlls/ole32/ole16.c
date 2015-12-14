/*
 * 16 bit ole functions
 *
 * Copyright 1995 Martin von Loewis
 * Copyright 1998 Justin Bradford
 * Copyright 1999 Francis Beaudet
 * Copyright 1999 Sylvain St-Germain
 * Copyright 2002 Marcus Meissner
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "wine/winbase16.h"
#include "windef.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"
#include "rpc.h"
#include "wownt32.h"
#include "wtypes.h"
#include "wine/unicode.h"
#include "objbase.h"
#include "compobj_private.h"
#include "ifs.h"
#include "objbase.h"
#include "ole2.h"
#include "ole2ver.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

HINSTANCE16     COMPOBJ_hInstance = 0;
static int      COMPOBJ_Attach = 0;

extern HRESULT WINAPI __CLSIDFromStringA( LPCSTR idstr, CLSID *id );

HTASK16 hETask = 0;
WORD Table_ETask[62];

LPMALLOC16 currentMalloc16=NULL;

/* --- IMalloc16 implementation */


typedef struct
{
        /* IUnknown fields */
        ICOM_VFIELD(IMalloc16);
        DWORD                   ref;
        /* IMalloc16 fields */
} IMalloc16Impl;

/******************************************************************************
 *		IMalloc16_QueryInterface	[COMPOBJ.500]
 */
HRESULT WINAPI IMalloc16_fnQueryInterface(IMalloc16* iface,REFIID refiid,LPVOID *obj) {
        ICOM_THIS(IMalloc16Impl,iface);

	TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(refiid),obj);
	if (	!memcmp(&IID_IUnknown,refiid,sizeof(IID_IUnknown)) ||
		!memcmp(&IID_IMalloc,refiid,sizeof(IID_IMalloc))
	) {
		*obj = This;
		return 0;
	}
	return OLE_E_ENUM_NOMORE;
}

/******************************************************************************
 *		IMalloc16_AddRef	[COMPOBJ.501]
 */
ULONG WINAPI IMalloc16_fnAddRef(IMalloc16* iface) {
        ICOM_THIS(IMalloc16Impl,iface);
	TRACE("(%p)->AddRef()\n",This);
	return 1; /* cannot be freed */
}

/******************************************************************************
 *		IMalloc16_Release	[COMPOBJ.502]
 */
ULONG WINAPI IMalloc16_fnRelease(IMalloc16* iface) {
        ICOM_THIS(IMalloc16Impl,iface);
	TRACE("(%p)->Release()\n",This);
	return 1; /* cannot be freed */
}

/******************************************************************************
 * IMalloc16_Alloc [COMPOBJ.503]
 */
SEGPTR WINAPI IMalloc16_fnAlloc(IMalloc16* iface,DWORD cb) {
        ICOM_THIS(IMalloc16Impl,iface);
	TRACE("(%p)->Alloc(%ld)\n",This,cb);
        return MapLS( HeapAlloc( GetProcessHeap(), 0, cb ) );
}

/******************************************************************************
 * IMalloc16_Realloc [COMPOBJ.504]
 */
SEGPTR WINAPI IMalloc16_fnRealloc(IMalloc16* iface,SEGPTR pv,DWORD cb)
{
    SEGPTR ret;
    ICOM_THIS(IMalloc16Impl,iface);
    TRACE("(%p)->Realloc(%08lx,%ld)\n",This,pv,cb);
    ret = MapLS( HeapReAlloc( GetProcessHeap(), 0, MapSL(pv), cb ) );
    UnMapLS(pv);
    return ret;
}

/******************************************************************************
 * IMalloc16_Free [COMPOBJ.505]
 */
VOID WINAPI IMalloc16_fnFree(IMalloc16* iface,SEGPTR pv)
{
    void *ptr = MapSL(pv);
    ICOM_THIS(IMalloc16Impl,iface);
    TRACE("(%p)->Free(%08lx)\n",This,pv);
    UnMapLS(pv);
    HeapFree( GetProcessHeap(), 0, ptr );
}

/******************************************************************************
 * IMalloc16_GetSize [COMPOBJ.506]
 */
DWORD WINAPI IMalloc16_fnGetSize(const IMalloc16* iface,SEGPTR pv)
{
	ICOM_CTHIS(IMalloc16Impl,iface);
        TRACE("(%p)->GetSize(%08lx)\n",This,pv);
        return HeapSize( GetProcessHeap(), 0, MapSL(pv) );
}

/******************************************************************************
 * IMalloc16_DidAlloc [COMPOBJ.507]
 */
INT16 WINAPI IMalloc16_fnDidAlloc(const IMalloc16* iface,LPVOID pv) {
        ICOM_CTHIS(IMalloc16,iface);
	TRACE("(%p)->DidAlloc(%p)\n",This,pv);
	return (INT16)-1;
}

/******************************************************************************
 * IMalloc16_HeapMinimize [COMPOBJ.508]
 */
LPVOID WINAPI IMalloc16_fnHeapMinimize(IMalloc16* iface) {
        ICOM_THIS(IMalloc16Impl,iface);
	TRACE("(%p)->HeapMinimize()\n",This);
	return NULL;
}

/******************************************************************************
 * IMalloc16_Constructor [VTABLE]
 */
LPMALLOC16
IMalloc16_Constructor()
{
    static ICOM_VTABLE(IMalloc16) vt16;
    static SEGPTR msegvt16;
    IMalloc16Impl* This;
    HMODULE16 hcomp = GetModuleHandle16("COMPOBJ");

    This = HeapAlloc( GetProcessHeap(), 0, sizeof(IMalloc16Impl) );
    if (!msegvt16)
    {
#define VTENT(x) vt16.x = (void*)GetProcAddress16(hcomp,"IMalloc16_"#x);assert(vt16.x)
        VTENT(QueryInterface);
        VTENT(AddRef);
        VTENT(Release);
        VTENT(Alloc);
        VTENT(Realloc);
        VTENT(Free);
        VTENT(GetSize);
        VTENT(DidAlloc);
        VTENT(HeapMinimize);
#undef VTENT
        msegvt16 = MapLS( &vt16 );
    }
    ICOM_VTBL(This) = (ICOM_VTABLE(IMalloc16)*)msegvt16;
    This->ref = 1;
    return (LPMALLOC16)MapLS( This );
}


/***********************************************************************
 *           CoGetMalloc    [COMPOBJ.4]
 * RETURNS
 *	The current win16 IMalloc
 */
HRESULT WINAPI CoGetMalloc16(
	DWORD dwMemContext,	/* [in] unknown */
	LPMALLOC16 * lpMalloc	/* [out] current win16 malloc interface */
) {
    if(!currentMalloc16)
	currentMalloc16 = IMalloc16_Constructor();
    *lpMalloc = currentMalloc16;
    return S_OK;
}

/***********************************************************************
 *           CoCreateStandardMalloc [COMPOBJ.71]
 */
HRESULT WINAPI CoCreateStandardMalloc16(DWORD dwMemContext,
					  LPMALLOC16 *lpMalloc)
{
    /* FIXME: docu says we shouldn't return the same allocator as in
     * CoGetMalloc16 */
    *lpMalloc = IMalloc16_Constructor();
    return S_OK;
}

/******************************************************************************
 *		CoInitialize	[COMPOBJ.2]
 * Set the win16 IMalloc used for memory management
 */
HRESULT WINAPI CoInitialize16(
	LPVOID lpReserved	/* [in] pointer to win16 malloc interface */
) {
    currentMalloc16 = (LPMALLOC16)lpReserved;
    return S_OK;
}

/***********************************************************************
 *           CoUninitialize   [COMPOBJ.3]
 * Don't know what it does.
 * 3-Nov-98 -- this was originally misspelled, I changed it to what I
 *   believe is the correct spelling
 */
void WINAPI CoUninitialize16(void)
{
  TRACE("()\n");
  CoFreeAllLibraries();
}

/***********************************************************************
 *           IsEqualGUID [COMPOBJ.18]
 *
 * Compares two Unique Identifiers.
 *
 * RETURNS
 *	TRUE if equal
 */
BOOL16 WINAPI IsEqualGUID16(
	GUID* g1,	/* [in] unique id 1 */
	GUID* g2)	/* [in] unique id 2 */
{
    return !memcmp( g1, g2, sizeof(GUID) );
}

/******************************************************************************
 *		CLSIDFromString	[COMPOBJ.20]
 * Converts a unique identifier from its string representation into
 * the GUID struct.
 *
 * Class id: DWORD-WORD-WORD-BYTES[2]-BYTES[6]
 *
 * RETURNS
 *	the converted GUID
 */
HRESULT WINAPI CLSIDFromString16(
	LPCOLESTR16 idstr,	/* [in] string representation of guid */
	CLSID *id)		/* [out] GUID converted from string */
{

  return __CLSIDFromStringA(idstr,id);
}

/******************************************************************************
 *		StringFromCLSID	[COMPOBJ.19]
 * Converts a GUID into the respective string representation.
 * The target string is allocated using the OLE IMalloc.
 * RETURNS
 *	the string representation and HRESULT
 */
HRESULT WINAPI StringFromCLSID16(
        REFCLSID id,            /* [in] the GUID to be converted */
	LPOLESTR16 *idstr	/* [out] a pointer to a to-be-allocated segmented pointer pointing to the resulting string */

) {
    extern BOOL WINAPI K32WOWCallback16Ex( DWORD vpfn16, DWORD dwFlags,
                                           DWORD cbArgs, LPVOID pArgs, LPDWORD pdwRetCode );
    LPMALLOC16	mllc;
    HRESULT	ret;
    DWORD	args[2];

    ret = CoGetMalloc16(0,&mllc);
    if (ret) return ret;

    args[0] = (DWORD)mllc;
    args[1] = 40;

    /* No need for a Callback entry, we have WOWCallback16Ex which does
     * everything we need.
     */
    if (!K32WOWCallback16Ex(
    	(DWORD)((ICOM_VTABLE(IMalloc16)*)MapSL(
            (SEGPTR)ICOM_VTBL(((LPMALLOC16)MapSL((SEGPTR)mllc))))
	)->Alloc,
	WCB16_CDECL,
	2*sizeof(DWORD),
	(LPVOID)args,
	(LPDWORD)idstr
    )) {
    	WARN("CallTo16 IMalloc16 failed\n");
    	return E_FAIL;
    }
    return WINE_StringFromCLSID(id,MapSL((SEGPTR)*idstr));
}

/******************************************************************************
 *		CLSIDFromProgID	[COMPOBJ.61]
 * Converts a program id into the respective GUID. (By using a registry lookup)
 * RETURNS
 *	riid associated with the progid
 */
HRESULT WINAPI CLSIDFromProgID16(
	LPCOLESTR16 progid,	/* [in] program id as found in registry */
	LPCLSID riid		/* [out] associated CLSID */
) {
	char	*buf,buf2[80];
	DWORD	buf2len;
	HRESULT	err;
	HKEY	xhkey;

	buf = HeapAlloc(GetProcessHeap(),0,strlen(progid)+8);
	sprintf(buf,"%s\\CLSID",progid);
	if ((err=RegOpenKeyA(HKEY_CLASSES_ROOT,buf,&xhkey))) {
		HeapFree(GetProcessHeap(),0,buf);
                return CO_E_CLASSSTRING;
	}
	HeapFree(GetProcessHeap(),0,buf);
	buf2len = sizeof(buf2);
	if ((err=RegQueryValueA(xhkey,NULL,buf2,&buf2len))) {
		RegCloseKey(xhkey);
                return CO_E_CLASSSTRING;
	}
	RegCloseKey(xhkey);
	return __CLSIDFromStringA(buf2,riid);
}

/***********************************************************************
 *           LookupETask (COMPOBJ.94)
 */
HRESULT WINAPI LookupETask16(HTASK16 *hTask,LPVOID p) {
	FIXME("(%p,%p),stub!\n",hTask,p);
	if ((*hTask = GetCurrentTask()) == hETask) {
		memcpy(p, Table_ETask, sizeof(Table_ETask));
	}
	return 0;
}

/***********************************************************************
 *           SetETask (COMPOBJ.95)
 */
HRESULT WINAPI SetETask16(HTASK16 hTask, LPVOID p) {
        FIXME("(%04x,%p),stub!\n",hTask,p);
	hETask = hTask;
	return 0;
}

/***********************************************************************
 *           CALLOBJECTINWOW (COMPOBJ.201)
 */
HRESULT WINAPI CallObjectInWOW(LPVOID p1,LPVOID p2) {
	FIXME("(%p,%p),stub!\n",p1,p2);
	return 0;
}

/******************************************************************************
 *		CoRegisterClassObject	[COMPOBJ.5]
 *
 * Don't know where it registers it ...
 */
HRESULT WINAPI CoRegisterClassObject16(
	REFCLSID rclsid,
	LPUNKNOWN pUnk,
	DWORD dwClsContext, /* [in] CLSCTX flags indicating the context in which to run the executable */
	DWORD flags,        /* [in] REGCLS flags indicating how connections are made */
	LPDWORD lpdwRegister
) {
	char	buf[80];

	WINE_StringFromCLSID(rclsid,buf);

	FIXME("(%s,%p,0x%08lx,0x%08lx,%p),stub\n",
		buf,pUnk,dwClsContext,flags,lpdwRegister
	);
	return 0;
}

/******************************************************************************
 *      CoRevokeClassObject [COMPOBJ.6]
 *
 */
HRESULT WINAPI CoRevokeClassObject16(DWORD dwRegister) /* [in] token on class obj */
{
    FIXME("(0x%08lx),stub!\n", dwRegister);
    return 0;
}

/******************************************************************************
 *      CoFileTimeToDosDateTime [COMPOBJ.30]
 */
BOOL16 WINAPI CoFileTimeToDosDateTime16(const FILETIME *ft, LPWORD lpDosDate, LPWORD lpDosTime)
{
    return FileTimeToDosDateTime(ft, lpDosDate, lpDosTime);
}

/******************************************************************************
 *      CoDosDateTimeToFileTime [COMPOBJ.31]
 */
BOOL16 WINAPI CoDosDateTimeToFileTime16(WORD wDosDate, WORD wDosTime, FILETIME *ft)
{
    return DosDateTimeToFileTime(wDosDate, wDosTime, ft);
}

/******************************************************************************
 *		CoRegisterMessageFilter	[COMPOBJ.27]
 */
HRESULT WINAPI CoRegisterMessageFilter16(
	LPMESSAGEFILTER lpMessageFilter,
	LPMESSAGEFILTER *lplpMessageFilter
) {
	FIXME("(%p,%p),stub!\n",lpMessageFilter,lplpMessageFilter);
	return 0;
}

/******************************************************************************
 *		CoLockObjectExternal	[COMPOBJ.63]
 */
HRESULT WINAPI CoLockObjectExternal16(
    LPUNKNOWN pUnk,		/* [in] object to be locked */
    BOOL16 fLock,		/* [in] do lock */
    BOOL16 fLastUnlockReleases	/* [in] ? */
) {
    FIXME("(%p,%d,%d),stub!\n",pUnk,fLock,fLastUnlockReleases);
    return S_OK;
}

/***********************************************************************
 *           CoGetState [COMPOBJ.115]
 */
HRESULT WINAPI CoGetState16(LPDWORD state)
{
    FIXME("(%p),stub!\n", state);

    *state = 0;
    return S_OK;
}

/***********************************************************************
 *      DllEntryPoint                   [COMPOBJ.116]
 *
 *    Initialization code for the COMPOBJ DLL
 *
 * RETURNS:
 */
BOOL WINAPI COMPOBJ_DllEntryPoint(DWORD Reason, HINSTANCE16 hInst, WORD ds, WORD HeapSize, DWORD res1, WORD res2)
{
        TRACE("(%08lx, %04x, %04x, %04x, %08lx, %04x)\n", Reason, hInst, ds, HeapSize, res1, res2);
        switch(Reason)
        {
        case DLL_PROCESS_ATTACH:
                if (!COMPOBJ_Attach++) COMPOBJ_hInstance = hInst;
                break;

        case DLL_PROCESS_DETACH:
                if(!--COMPOBJ_Attach)
                        COMPOBJ_hInstance = 0;
                break;
        }
        return TRUE;
}
