/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 * DDEML library
 *
 * Copyright 1997 Alexandre Julliard
 * Copyright 1997 Len White
 * Copyright 1999 Keith Matthews
 * Copyright 2000 Corel
 * Copyright 2001 Eric Pouech
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <string.h>
#include <stdio.h>
#include "winbase.h"
#include "windef.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "dde.h"
#include "ddeml.h"
#include "win.h"
#include "wine/debug.h"
#include "dde/dde_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(ddeml);

static WDML_INSTANCE*	WDML_InstanceList = NULL;
static DWORD		WDML_MaxInstanceID = 0;  /* OK for present, have to worry about wrap-around later */
const char		WDML_szEventClass[] = "DdeEventClass";
CRITICAL_SECTION	WDML_CritSect;


void create_wdml_cs(void)
{
  CRITICAL_SECTION_DEFINE( &WDML_CritSect );
}

void destroy_wdml_cs(void)
{
  DeleteCriticalSection( &WDML_CritSect );
}

/* ================================================================
 *
 * 			Pure DDE (non DDEML) management
 *
 * ================================================================ */

static BOOL DDE_RequirePacking(UINT msg)
{
    BOOL	ret;

    switch (msg)
    {
    case WM_DDE_ACK:
    case WM_DDE_ADVISE:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
	ret = TRUE;
	break;
    case WM_DDE_EXECUTE:	/* strange, NT 2000 (at least) really uses packing here... */
    case WM_DDE_INITIATE:
    case WM_DDE_REQUEST:	/* assuming clipboard formats are 16 bit */
    case WM_DDE_TERMINATE:
    case WM_DDE_UNADVISE:	/* assuming clipboard formats are 16 bit */
	ret = FALSE;
	break;
    default:
	TRACE("Unknown message %04x\n", msg);
	ret = FALSE;
	break;
    }
    return ret;
}

/*****************************************************************
 *            PackDDElParam (USER32.@)
 *
 * RETURNS
 *   the packed lParam
 */
LPARAM WINAPI PackDDElParam(UINT msg, UINT uiLo, UINT uiHi)
{
     HGLOBAL	hMem;
     UINT*	params;

     if (!DDE_RequirePacking(msg))
	 return MAKELONG(uiLo, uiHi);

     if (!(hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(UINT) * 2)))
     {
	  ERR("GlobalAlloc failed\n");
	  return 0;
     }

     params = GlobalLock(hMem);
     if (params == NULL)
     {
	  ERR("GlobalLock failed (%x)\n", hMem);
	  return 0;
     }

     params[0] = uiLo;
     params[1] = uiHi;

     GlobalUnlock(hMem);

     return (LPARAM)hMem;
}


/*****************************************************************
 *            UnpackDDElParam (USER32.@)
 *
 * RETURNS
 *   success: nonzero
 *   failure: zero
 */
BOOL WINAPI UnpackDDElParam(UINT msg, LPARAM lParam,
			    PUINT uiLo, PUINT uiHi)
{
     HGLOBAL hMem;
     UINT *params;

     if (!DDE_RequirePacking(msg))
     {
	 *uiLo = LOWORD(lParam);
	 *uiHi = HIWORD(lParam);

	 return TRUE;
     }

     if (lParam == 0)
     {
	  return FALSE;
     }

     hMem = (HGLOBAL)lParam;

     params = GlobalLock(hMem);
     if (params == NULL)
     {
	  ERR("GlobalLock failed (%x)\n", hMem);
	  return FALSE;
     }

     *uiLo = params[0];
     *uiHi = params[1];

     GlobalUnlock(hMem);

     return TRUE;
}


/*****************************************************************
 *            FreeDDElParam (USER32.@)
 *
 * RETURNS
 *   success: nonzero
 *   failure: zero
 */
BOOL WINAPI FreeDDElParam(UINT msg, LPARAM lParam)
{
     HGLOBAL hMem = (HGLOBAL)lParam;

     if (!DDE_RequirePacking(msg))
	 return TRUE;

     if (lParam == 0)
     {
	  return FALSE;
     }
     return GlobalFree(hMem) == (HGLOBAL)NULL;
}


/*****************************************************************
 *            ReuseDDElParam (USER32.@)
 *
 * RETURNS
 *   the packed lParam
 */
LPARAM WINAPI ReuseDDElParam(LPARAM lParam, UINT msgIn, UINT msgOut,
                             UINT uiLo, UINT uiHi)
{
     HGLOBAL	hMem;
     UINT*	params;
     BOOL	in, out;

     in = DDE_RequirePacking(msgIn);
     out = DDE_RequirePacking(msgOut);

     if (!in)
     {
	 return PackDDElParam(msgOut, uiLo, uiHi);
     }

     if (lParam == 0)
     {
	  return FALSE;
     }

     if (!out)
     {
	 FreeDDElParam(msgIn, lParam);
	 return MAKELONG(uiLo, uiHi);
     }

     hMem = (HGLOBAL)lParam;

     params = GlobalLock(hMem);
     if (params == NULL)
     {
	  ERR("GlobalLock failed\n");
	  return 0;
     }

     params[0] = uiLo;
     params[1] = uiHi;

     TRACE("Reusing pack %08x %08x\n", uiLo, uiHi);

     GlobalLock(hMem);
     return lParam;
}

/*****************************************************************
 *            ImpersonateDdeClientWindow (USER32.@)
 *
 * PARAMS
 * hWndClient	  [I] handle to DDE client window
 * hWndServer	  [I] handle to DDE server window
 */
BOOL WINAPI ImpersonateDdeClientWindow(HWND hWndClient, HWND hWndServer)
{
     FIXME("(%04x %04x): stub\n", hWndClient, hWndServer);
     return FALSE;
}

/*****************************************************************
 *            DdeSetQualityOfService (USER32.@)
 */

BOOL WINAPI DdeSetQualityOfService(HWND hwndClient, CONST SECURITY_QUALITY_OF_SERVICE *pqosNew,
				   PSECURITY_QUALITY_OF_SERVICE pqosPrev)
{
     FIXME("(%04x %p %p): stub\n", hwndClient, pqosNew, pqosPrev);
     return TRUE;
}

/* ================================================================
 *
 * 			Instance management
 *
 * ================================================================ */

/******************************************************************************
 *		IncrementInstanceId
 *
 *	generic routine to increment the max instance Id and allocate a new application instance
 */
static void WDML_IncrementInstanceId(WDML_INSTANCE* pInstance)
{
    DWORD	id = InterlockedIncrement(&WDML_MaxInstanceID);

    pInstance->instanceID = id;
    TRACE("New instance id %ld allocated\n", id);
}

/******************************************************************
 *		WDML_EventProc
 *
 *
 */
static LRESULT CALLBACK WDML_EventProc(HWND hwndEvent, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WDML_INSTANCE*	pInstance;
    HSZ			hsz1, hsz2;

    switch (uMsg)
    {
    case WM_WDML_REGISTER:
	pInstance = WDML_GetInstanceFromWnd(hwndEvent);
        /* try calling the Callback */
	if (pInstance && !(pInstance->CBFflags & CBF_SKIP_REGISTRATIONS))
	{
	    hsz1 = WDML_MakeHszFromAtom(pInstance, wParam);
	    hsz2 = WDML_MakeHszFromAtom(pInstance, lParam);
	    WDML_InvokeCallback(pInstance, XTYP_REGISTER, 0, 0, hsz1, hsz2, 0, 0, 0);
	    WDML_DecHSZ(pInstance, hsz1);
	    WDML_DecHSZ(pInstance, hsz2);
	}
	break;

    case WM_WDML_UNREGISTER:
	pInstance = WDML_GetInstanceFromWnd(hwndEvent);
	if (pInstance && !(pInstance->CBFflags & CBF_SKIP_UNREGISTRATIONS))
	{
	    hsz1 = WDML_MakeHszFromAtom(pInstance, wParam);
	    hsz2 = WDML_MakeHszFromAtom(pInstance, lParam);
	    WDML_InvokeCallback(pInstance, XTYP_UNREGISTER, 0, 0, hsz1, hsz2, 0, 0, 0);
	    WDML_DecHSZ(pInstance, hsz1);
	    WDML_DecHSZ(pInstance, hsz2);
	}
	break;

    case WM_WDML_CONNECT_CONFIRM:
	pInstance = WDML_GetInstanceFromWnd(hwndEvent);
	if (pInstance && !(pInstance->CBFflags & CBF_SKIP_CONNECT_CONFIRMS))
	{
	    WDML_CONV*	pConv;
	    /* confirm connection...
	     * lookup for this conv handle
	     */
            HWND client = WIN_GetFullHandle( (HWND)wParam );
            HWND server = WIN_GetFullHandle( (HWND)lParam );
	    for (pConv = pInstance->convs[WDML_SERVER_SIDE]; pConv != NULL; pConv = pConv->next)
	    {
		if (pConv->hwndClient == client && pConv->hwndServer == server)
		    break;
	    }
	    if (pConv)
	    {
		pConv->wStatus |= ST_ISLOCAL;

		WDML_InvokeCallback(pInstance, XTYP_CONNECT_CONFIRM, 0, (HCONV)pConv,
				    pConv->hszTopic, pConv->hszService, 0, 0,
				    (pConv->wStatus & ST_ISSELF) ? 1 : 0);
	    }
	}
	break;
    default:
	return DefWindowProcA(hwndEvent, uMsg, wParam, lParam);
    }
    return 0;
}

/******************************************************************
 *		WDML_Initialize
 *
 *
 */
UINT WDML_Initialize(LPDWORD pidInst, PFNCALLBACK pfnCallback,
		     DWORD afCmd, DWORD ulRes, BOOL bUnicode, BOOL b16)
{
    WDML_INSTANCE*		pInstance;
    WDML_INSTANCE*		reference_inst;
    UINT			ret;
    WNDCLASSEXA			wndclass;

    TRACE("(%p,%p,0x%lx,%ld)\n",
	  pidInst, pfnCallback, afCmd, ulRes);

    if (ulRes)
    {
	ERR("Reserved value not zero?  What does this mean?\n");
	/* trap this and no more until we know more */
	return DMLERR_NO_ERROR;
    }

    /* grab enough heap for one control struct - not really necessary for re-initialise
     *	but allows us to use same validation routines */
    pInstance = (WDML_INSTANCE*)HeapAlloc(GetProcessHeap(), 0, sizeof(WDML_INSTANCE));
    if (pInstance == NULL)
    {
	/* catastrophe !! warn user & abort */
	ERR("Instance create failed - out of memory\n");
	return DMLERR_SYS_ERROR;
    }
    pInstance->next = NULL;
    pInstance->monitor = (afCmd | APPCLASS_MONITOR);

    /* messy bit, spec implies that 'Client Only' can be set in 2 different ways, catch 1 here */

    pInstance->clientOnly = afCmd & APPCMD_CLIENTONLY;
    pInstance->instanceID = *pidInst; /* May need to add calling proc Id */
    pInstance->threadID = GetCurrentThreadId();
    pInstance->callback = *pfnCallback;
    pInstance->unicode = bUnicode;
    pInstance->win16 = b16;
    pInstance->nodeList = NULL; /* node will be added later */
    pInstance->monitorFlags = afCmd & MF_MASK;
    pInstance->servers = NULL;
    pInstance->convs[0] = NULL;
    pInstance->convs[1] = NULL;
    pInstance->links[0] = NULL;
    pInstance->links[1] = NULL;

    /* isolate CBF flags in one go, expect this will go the way of all attempts to be clever !! */

    pInstance->CBFflags = afCmd^((afCmd&MF_MASK)|((afCmd&APPCMD_MASK)|(afCmd&APPCLASS_MASK)));

    if (!pInstance->clientOnly)
    {
	/* Check for other way of setting Client-only !! */
	pInstance->clientOnly =
	    (pInstance->CBFflags & CBF_FAIL_ALLSVRXACTIONS) == CBF_FAIL_ALLSVRXACTIONS;
    }

    TRACE("instance created - checking validity \n");

    if (*pidInst == 0)
    {
	/*  Initialisation of new Instance Identifier */
	TRACE("new instance, callback %p flags %lX\n",pfnCallback,afCmd);

	EnterCriticalSection(&WDML_CritSect);

	if (WDML_InstanceList == NULL)
	{
	    /* can't be another instance in this case, assign to the base pointer */
	    WDML_InstanceList = pInstance;

	    /* since first must force filter of XTYP_CONNECT and XTYP_WILDCONNECT for
	     *		present
	     *	-------------------------------      NOTE NOTE NOTE    --------------------------
	     *
	     *	the manual is not clear if this condition
	     *	applies to the first call to DdeInitialize from an application, or the
	     *	first call for a given callback !!!
	     */

	    pInstance->CBFflags = pInstance->CBFflags|APPCMD_FILTERINITS;
	    TRACE("First application instance detected OK\n");
	    /*	allocate new instance ID */
	    WDML_IncrementInstanceId(pInstance);
	}
	else
	{
	    /* really need to chain the new one in to the latest here, but after checking conditions
	     *	such as trying to start a conversation from an application trying to monitor */
	    reference_inst = WDML_InstanceList;
	    TRACE("Subsequent application instance - starting checks\n");
	    while (reference_inst->next != NULL)
	    {
		/*
		 *	This set of tests will work if application uses same instance Id
		 *	at application level once allocated - which is what manual implies
		 *	should happen. If someone tries to be
		 *	clever (lazy ?) it will fail to pick up that later calls are for
		 *	the same application - should we trust them ?
		 */
		if (pInstance->instanceID == reference_inst->instanceID)
		{
		    /* Check 1 - must be same Client-only state */

		    if (pInstance->clientOnly != reference_inst->clientOnly)
		    {
			ret = DMLERR_DLL_USAGE;
			goto theError;
		    }

		    /* Check 2 - cannot use 'Monitor' with any non-monitor modes */

		    if (pInstance->monitor != reference_inst->monitor)
		    {
			ret = DMLERR_INVALIDPARAMETER;
			goto theError;
		    }

		    /* Check 3 - must supply different callback address */

		    if (pInstance->callback == reference_inst->callback)
		    {
			ret = DMLERR_DLL_USAGE;
			goto theError;
		    }
		}
		reference_inst = reference_inst->next;
	    }
	    /*  All cleared, add to chain */

	    TRACE("Application Instance checks finished\n");
	    WDML_IncrementInstanceId(pInstance);
	    reference_inst->next = pInstance;
	}
	LeaveCriticalSection(&WDML_CritSect);

	*pidInst = pInstance->instanceID;

	/* for deadlock issues, windows must always be created when outside the critical section */
	wndclass.cbSize        = sizeof(wndclass);
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = WDML_EventProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof(DWORD);
	wndclass.hInstance     = 0;
	wndclass.hIcon         = 0;
	wndclass.hCursor       = 0;
	wndclass.hbrBackground = 0;
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = WDML_szEventClass;
	wndclass.hIconSm       = 0;

	RegisterClassExA(&wndclass);

	pInstance->hwndEvent = CreateWindowA(WDML_szEventClass, NULL,
						WS_POPUP, 0, 0, 0, 0,
						0, 0, 0, 0);

	SetWindowLongA(pInstance->hwndEvent, GWL_WDML_INSTANCE, (DWORD)pInstance);

	TRACE("New application instance processing finished OK\n");
    }
    else
    {
	/* Reinitialisation situation   --- FIX  */
	TRACE("reinitialisation of (%p,%p,0x%lx,%ld): stub\n", pidInst, pfnCallback, afCmd, ulRes);

	EnterCriticalSection(&WDML_CritSect);

	if (WDML_InstanceList == NULL)
	{
	    ret = DMLERR_DLL_USAGE;
	    goto theError;
	}
	HeapFree(GetProcessHeap(), 0, pInstance); /* finished - release heap space used as work store */
	/* can't reinitialise if we have initialised nothing !! */
	reference_inst = WDML_InstanceList;
	/* must first check if we have been given a valid instance to re-initialise !!  how do we do that ? */
	/*
	 *	MS allows initialisation without specifying a callback, should we allow addition of the
	 *	callback by a later call to initialise ? - if so this lot will have to change
	 */
	while (reference_inst->next != NULL)
	{
	    if (*pidInst == reference_inst->instanceID && pfnCallback == reference_inst->callback)
	    {
		/* Check 1 - cannot change client-only mode if set via APPCMD_CLIENTONLY */

		if (reference_inst->clientOnly)
		{
		    if  ((reference_inst->CBFflags & CBF_FAIL_ALLSVRXACTIONS) != CBF_FAIL_ALLSVRXACTIONS)
		    {
				/* i.e. Was set to Client-only and through APPCMD_CLIENTONLY */

			if (!(afCmd & APPCMD_CLIENTONLY))
			{
			    ret = DMLERR_DLL_USAGE;
			    goto theError;
			}
		    }
		}
		/* Check 2 - cannot change monitor modes */

		if (pInstance->monitor != reference_inst->monitor)
		{
		    ret = DMLERR_DLL_USAGE;
		    goto theError;
		}

		/* Check 3 - trying to set Client-only via APPCMD when not set so previously */

		if ((afCmd&APPCMD_CLIENTONLY) && !reference_inst->clientOnly)
		{
		    ret = DMLERR_DLL_USAGE;
		    goto theError;
		}
		break;
	    }
	    reference_inst = reference_inst->next;
	}
	if (reference_inst->next == NULL)
	{
	    /* Crazy situation - trying to re-initialize something that has not beeen initialized !!
	     *
	     *	Manual does not say what we do, cannot return DMLERR_NOT_INITIALIZED so what ?
	     */
	    ret = DMLERR_INVALIDPARAMETER;
	    goto theError;
	}
	/* All checked - change relevant flags */

	reference_inst->CBFflags = pInstance->CBFflags;
	reference_inst->clientOnly = pInstance->clientOnly;
	reference_inst->monitorFlags = pInstance->monitorFlags;
	LeaveCriticalSection(&WDML_CritSect);
    }

    return DMLERR_NO_ERROR;
 theError:
    HeapFree(GetProcessHeap(), 0, pInstance);
    LeaveCriticalSection(&WDML_CritSect);
    return ret;
}

/******************************************************************************
 *            DdeInitializeA   (USER32.@)
 */
UINT WINAPI DdeInitializeA(LPDWORD pidInst, PFNCALLBACK pfnCallback,
			   DWORD afCmd, DWORD ulRes)
{
    return WDML_Initialize(pidInst, pfnCallback, afCmd, ulRes, FALSE, FALSE);
}

/******************************************************************************
 * DdeInitializeW [USER32.@]
 * Registers an application with the DDEML
 *
 * PARAMS
 *    pidInst     [I] Pointer to instance identifier
 *    pfnCallback [I] Pointer to callback function
 *    afCmd       [I] Set of command and filter flags
 *    ulRes       [I] Reserved
 *
 * RETURNS
 *    Success: DMLERR_NO_ERROR
 *    Failure: DMLERR_DLL_USAGE, DMLERR_INVALIDPARAMETER, DMLERR_SYS_ERROR
 */
UINT WINAPI DdeInitializeW(LPDWORD pidInst, PFNCALLBACK pfnCallback,
			   DWORD afCmd, DWORD ulRes)
{
    return WDML_Initialize(pidInst, pfnCallback, afCmd, ulRes, TRUE, FALSE);
}

/*****************************************************************
 * DdeUninitialize [USER32.@]  Frees DDEML resources
 *
 * PARAMS
 *    idInst [I] Instance identifier
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */

BOOL WINAPI DdeUninitialize(DWORD idInst)
{
    /*  Stage one - check if we have a handle for this instance
     */
    WDML_INSTANCE*		pInstance;
    WDML_CONV*			pConv;
    WDML_CONV*			pConvNext;

    EnterCriticalSection(&WDML_CritSect);

    /*  First check instance
     */
    pInstance = WDML_GetInstance(idInst);
    if (pInstance == NULL)
    {
	LeaveCriticalSection(&WDML_CritSect);
	/*
	 *	Needs something here to record NOT_INITIALIZED ready for DdeGetLastError
	 */
	return FALSE;
    }

    /* first terminate all conversations client side
     * this shall close existing links...
     */
    for (pConv = pInstance->convs[WDML_CLIENT_SIDE]; pConv != NULL; pConv = pConvNext)
    {
	pConvNext = pConv->next;
	DdeDisconnect((HCONV)pConv);
    }
    if (pInstance->convs[WDML_CLIENT_SIDE])
	FIXME("still pending conversations\n");

    /* then unregister all known service names */
    DdeNameService(idInst, 0, 0, DNS_UNREGISTER);

    /* Free the nodes that were not freed by this instance
     * and remove the nodes from the list of HSZ nodes.
     */
    WDML_FreeAllHSZ(pInstance);

    DestroyWindow(pInstance->hwndEvent);

    /* OK now delete the instance handle itself */

    if (WDML_InstanceList == pInstance)
    {
	/* special case - the first/only entry */
	WDML_InstanceList = pInstance->next;
    }
    else
    {
	/* general case, remove entry */
	WDML_INSTANCE*	inst;

	for (inst = WDML_InstanceList; inst->next != pInstance; inst = inst->next);
	inst->next = pInstance->next;
    }
    /* leave crit sect and release the heap entry
     */
    HeapFree(GetProcessHeap(), 0, pInstance);
    LeaveCriticalSection(&WDML_CritSect);
    return TRUE;
}

/******************************************************************
 *		WDML_NotifyThreadExit
 *
 *
 */
void WDML_NotifyThreadDetach(void)
{
    WDML_INSTANCE*	pInstance;
    WDML_INSTANCE*	next;
    DWORD		tid = GetCurrentThreadId();

    EnterCriticalSection(&WDML_CritSect);
    for (pInstance = WDML_InstanceList; pInstance != NULL; pInstance = next)
    {
	next = pInstance->next;
	if (pInstance->threadID == tid)
	{
	    DdeUninitialize(pInstance->instanceID);
	}
    }
    LeaveCriticalSection(&WDML_CritSect);
}

/******************************************************************
 *		WDML_InvokeCallback
 *
 *
 */
HDDEDATA 	WDML_InvokeCallback(WDML_INSTANCE* pInstance, UINT uType, UINT uFmt, HCONV hConv,
				    HSZ hsz1, HSZ hsz2, HDDEDATA hdata,
				    DWORD dwData1, DWORD dwData2)
{
    HDDEDATA	ret;

    if (pInstance == NULL)
	return (HDDEDATA)0;
    TRACE("invoking CB%d[%08lx] (%u %u %08lx 0x%x 0x%x %u %lu %lu)\n",
	  pInstance->win16 ? 16 : 32, (DWORD)pInstance->callback, uType, uFmt,
	  (DWORD)hConv, hsz1, hsz2, hdata, dwData1, dwData2);
    if (pInstance->win16)
    {
	ret = WDML_InvokeCallback16(pInstance->callback, uType, uFmt, hConv,
				    hsz1, hsz2, hdata, dwData1, dwData2);
    }
    else
    {
	ret = pInstance->callback(uType, uFmt, hConv, hsz1, hsz2, hdata, dwData1, dwData2);
    }
    TRACE("done => %08lx\n", (DWORD)ret);
    return ret;
}

/*****************************************************************************
 *	WDML_GetInstance
 *
 *	generic routine to return a pointer to the relevant DDE_HANDLE_ENTRY
 *	for an instance Id, or NULL if the entry does not exist
 *
 */
WDML_INSTANCE*	WDML_GetInstance(DWORD instId)
{
    WDML_INSTANCE*	pInstance;

    for (pInstance = WDML_InstanceList; pInstance != NULL; pInstance = pInstance->next)
    {
	if (pInstance->instanceID == instId)
	{
	    if (GetCurrentThreadId() != pInstance->threadID)
	    {
		FIXME("Tried to get instance from wrong thread\n");
		continue;
	    }
	    return pInstance;
	}
    }
    TRACE("Instance entry missing\n");
    return NULL;
}

/******************************************************************
 *		WDML_GetInstanceFromWnd
 *
 *
 */
WDML_INSTANCE*	WDML_GetInstanceFromWnd(HWND hWnd)
{
    return (WDML_INSTANCE*)GetWindowLongA(hWnd, GWL_WDML_INSTANCE);
}

/******************************************************************************
 * DdeGetLastError [USER32.@]  Gets most recent error code
 *
 * PARAMS
 *    idInst [I] Instance identifier
 *
 * RETURNS
 *    Last error code
 */
UINT WINAPI DdeGetLastError(DWORD idInst)
{
    DWORD		error_code;
    WDML_INSTANCE*	pInstance;

    FIXME("(%ld): error reporting is weakly implemented\n", idInst);

    EnterCriticalSection(&WDML_CritSect);

    /*  First check instance
     */
    pInstance = WDML_GetInstance(idInst);
    if  (pInstance == NULL)
    {
	error_code = DMLERR_DLL_NOT_INITIALIZED;
    }
    else
    {
	error_code = pInstance->lastError;
	pInstance->lastError = 0;
    }

    LeaveCriticalSection(&WDML_CritSect);
    return error_code;
}

/* ================================================================
 *
 * 			String management
 *
 * ================================================================ */


/******************************************************************
 *		WDML_FindNode
 *
 *
 */
static HSZNode*	WDML_FindNode(WDML_INSTANCE* pInstance, HSZ hsz)
{
    HSZNode*	pNode;

    if (pInstance == NULL) return NULL;

    for (pNode = pInstance->nodeList; pNode != NULL; pNode = pNode->next)
    {
	if (pNode->hsz == hsz) break;
    }
    if (!pNode) WARN("HSZ 0x%x not found\n", hsz);
    return pNode;
}

/******************************************************************
 *		WDML_MakeAtomFromHsz
 *
 * Creates a global atom from an existing HSZ
 * Generally used before sending an HSZ as an atom to a remote app
 */
ATOM	WDML_MakeAtomFromHsz(HSZ hsz)
{
    WCHAR nameBuffer[MAX_BUFFER_LEN];

    if (GetAtomNameW((ATOM)hsz, nameBuffer, MAX_BUFFER_LEN))
	return GlobalAddAtomW(nameBuffer);
    WARN("HSZ 0x%x not found\n", hsz);
    return 0;
}

/******************************************************************
 *		WDML_MakeHszFromAtom
 *
 * Creates a HSZ from an existing global atom
 * Generally used while receiving a global atom and transforming it
 * into an HSZ
 */
HSZ	WDML_MakeHszFromAtom(WDML_INSTANCE* pInstance, ATOM atom)
{
    WCHAR nameBuffer[MAX_BUFFER_LEN];

    if (!atom) return (HSZ)0;

    if (GlobalGetAtomNameW(atom, nameBuffer, MAX_BUFFER_LEN))
    {
	TRACE("%x => %s\n", atom, debugstr_w(nameBuffer));
	return DdeCreateStringHandleW(pInstance->instanceID, nameBuffer, CP_WINUNICODE);
    }
    WARN("ATOM 0x%x not found\n", atom);
    return 0;
}

/******************************************************************
 *		WDML_IncHSZ
 *
 *
 */
BOOL WDML_IncHSZ(WDML_INSTANCE* pInstance, HSZ hsz)
{
    HSZNode*	pNode;

    pNode = WDML_FindNode(pInstance, hsz);
    if (!pNode) return FALSE;

    pNode->refCount++;
    return TRUE;
}

/******************************************************************************
 *           WDML_DecHSZ    (INTERNAL)
 *
 * Decrease the ref count of an HSZ. If it reaches 0, the node is removed from the list
 * of HSZ nodes
 * Returns -1 is the HSZ isn't found, otherwise it's the current (after --) of the ref count
 */
BOOL WDML_DecHSZ(WDML_INSTANCE* pInstance, HSZ hsz)
{
    HSZNode* 	pPrev = NULL;
    HSZNode* 	pCurrent;

    for (pCurrent = pInstance->nodeList; pCurrent != NULL; pCurrent = (pPrev = pCurrent)->next)
    {
	/* If we found the node we were looking for and its ref count is one,
	 * we can remove it
	 */
	if (pCurrent->hsz == hsz)
	{
	    if (--pCurrent->refCount == 0)
	    {
		if (pCurrent == pInstance->nodeList)
		{
		    pInstance->nodeList = pCurrent->next;
		}
		else
		{
		    pPrev->next = pCurrent->next;
		}
		HeapFree(GetProcessHeap(), 0, pCurrent);
		DeleteAtom((ATOM)hsz);
	    }
	    return TRUE;
	}
    }
    WARN("HSZ 0x%x not found\n", hsz);

    return FALSE;
}

/******************************************************************************
 *            WDML_FreeAllHSZ    (INTERNAL)
 *
 * Frees up all the strings still allocated in the list and
 * remove all the nodes from the list of HSZ nodes.
 */
void WDML_FreeAllHSZ(WDML_INSTANCE* pInstance)
{
    /* Free any strings created in this instance.
     */
    while (pInstance->nodeList != NULL)
    {
	DdeFreeStringHandle(pInstance->instanceID, pInstance->nodeList->hsz);
    }
}

/******************************************************************************
 *            InsertHSZNode    (INTERNAL)
 *
 * Insert a node to the head of the list.
 */
static void WDML_InsertHSZNode(WDML_INSTANCE* pInstance, HSZ hsz)
{
    if (hsz != 0)
    {
	HSZNode* pNew = NULL;
	/* Create a new node for this HSZ.
	 */
	pNew = (HSZNode*)HeapAlloc(GetProcessHeap(), 0, sizeof(HSZNode));
	if (pNew != NULL)
	{
	    pNew->hsz      = hsz;
	    pNew->next     = pInstance->nodeList;
	    pNew->refCount = 1;
	    pInstance->nodeList = pNew;
	}
	else
	{
	    ERR("Primary HSZ Node allocation failed - out of memory\n");
	}
    }
}

/******************************************************************
 *		WDML_QueryString
 *
 *
 */
static int	WDML_QueryString(WDML_INSTANCE* pInstance, HSZ hsz, LPVOID ptr, DWORD cchMax,
				 int codepage)
{
    WCHAR	pString[MAX_BUFFER_LEN];
    int		ret;
    /* If psz is null, we have to return only the length
     * of the string.
     */
    if (ptr == NULL)
    {
	ptr = pString;
	cchMax = MAX_BUFFER_LEN;
    }

    switch (codepage)
    {
    case CP_WINANSI:
	ret = GetAtomNameA((ATOM)hsz, ptr, cchMax);
	break;
    case CP_WINUNICODE:
	ret = GetAtomNameW((ATOM)hsz, ptr, cchMax);
    default:
	ERR("Unknown code page %d\n", codepage);
	ret = 0;
    }
    return ret;
}

/*****************************************************************
 * DdeQueryStringA [USER32.@]
 */
DWORD WINAPI DdeQueryStringA(DWORD idInst, HSZ hsz, LPSTR psz, DWORD cchMax, INT iCodePage)
{
    DWORD		ret = 0;
    WDML_INSTANCE*	pInstance;

    TRACE("(%ld, 0x%x, %p, %ld, %d)\n", idInst, hsz, psz, cchMax, iCodePage);

    EnterCriticalSection(&WDML_CritSect);

    /*  First check instance
     */
    pInstance = WDML_GetInstance(idInst);
    if (pInstance != NULL)
    {
	if (iCodePage == 0) iCodePage = CP_WINANSI;
	ret = WDML_QueryString(pInstance, hsz, psz, cchMax, iCodePage);
    }
    LeaveCriticalSection(&WDML_CritSect);

    TRACE("returning %s\n", debugstr_a(psz));
    return ret;
}

/*****************************************************************
 * DdeQueryStringW [USER32.@]
 */

DWORD WINAPI DdeQueryStringW(DWORD idInst, HSZ hsz, LPWSTR psz, DWORD cchMax, INT iCodePage)
{
    DWORD		ret = 0;
    WDML_INSTANCE*	pInstance;

    TRACE("(%ld, 0x%x, %p, %ld, %d)\n", idInst, hsz, psz, cchMax, iCodePage);

    EnterCriticalSection(&WDML_CritSect);

    /*  First check instance
     */
    pInstance = WDML_GetInstance(idInst);
    if (pInstance != NULL)
    {
	if (iCodePage == 0) iCodePage = CP_WINUNICODE;
	ret = WDML_QueryString(pInstance, hsz, psz, cchMax, iCodePage);
    }
    LeaveCriticalSection(&WDML_CritSect);

    TRACE("returning %s\n", debugstr_w(psz));
    return ret;
}

/******************************************************************
 *		DML_CreateString
 *
 *
 */
static	HSZ	WDML_CreateString(WDML_INSTANCE* pInstance, LPCVOID ptr, int codepage)
{
    HSZ		hsz;

    switch (codepage)
    {
    case CP_WINANSI:
	hsz = (HSZ)AddAtomA(ptr);
	TRACE("added atom %s with HSZ 0x%x, \n", debugstr_a(ptr), hsz);
	break;
    case CP_WINUNICODE:
	hsz = (HSZ)AddAtomW(ptr);
	TRACE("added atom %s with HSZ 0x%x, \n", debugstr_w(ptr), hsz);
	break;
    default:
	ERR("Unknown code page %d\n", codepage);
	return 0;
    }
    WDML_InsertHSZNode(pInstance, hsz);
    return hsz;
}

/*****************************************************************
 * DdeCreateStringHandleA [USER32.@]
 *
 * RETURNS
 *    Success: String handle
 *    Failure: 0
 */
HSZ WINAPI DdeCreateStringHandleA(DWORD idInst, LPCSTR psz, INT codepage)
{
    HSZ			hsz = 0;
    WDML_INSTANCE*	pInstance;

    TRACE("(%ld,%p,%d)\n", idInst, psz, codepage);

    EnterCriticalSection(&WDML_CritSect);

    pInstance = WDML_GetInstance(idInst);
    if (pInstance)
    {
	if (codepage == 0) codepage = CP_WINANSI;
	hsz = WDML_CreateString(pInstance, psz, codepage);
    }

    LeaveCriticalSection(&WDML_CritSect);
    return hsz;
}


/******************************************************************************
 * DdeCreateStringHandleW [USER32.@]  Creates handle to identify string
 *
 * PARAMS
 * 	idInst   [I] Instance identifier
 * 	psz      [I] Pointer to string
 *	codepage [I] Code page identifier
 * RETURNS
 *    Success: String handle
 *    Failure: 0
 */
HSZ WINAPI DdeCreateStringHandleW(DWORD idInst, LPCWSTR psz, INT codepage)
{
    WDML_INSTANCE*	pInstance;
    HSZ			hsz = 0;

    TRACE("(%ld,%p,%d)\n", idInst, psz, codepage);

    EnterCriticalSection(&WDML_CritSect);

    pInstance = WDML_GetInstance(idInst);
    if (pInstance)
    {
	if (codepage == 0) codepage = CP_WINUNICODE;
	hsz = WDML_CreateString(pInstance, psz, codepage);
    }
    LeaveCriticalSection(&WDML_CritSect);

    return hsz;
}

/*****************************************************************
 *            DdeFreeStringHandle   (USER32.@)
 * RETURNS: success: nonzero
 *          fail:    zero
 */
BOOL WINAPI DdeFreeStringHandle(DWORD idInst, HSZ hsz)
{
    WDML_INSTANCE*	pInstance;
    BOOL		ret = FALSE;

    TRACE("(%ld,0x%x): \n", idInst, hsz);

    EnterCriticalSection(&WDML_CritSect);

    /*  First check instance
     */
    pInstance = WDML_GetInstance(idInst);
    if (pInstance)
	ret = WDML_DecHSZ(pInstance, hsz);

    LeaveCriticalSection(&WDML_CritSect);

    return ret;
}

/*****************************************************************
 *            DdeKeepStringHandle  (USER32.@)
 *
 * RETURNS: success: nonzero
 *          fail:    zero
 */
BOOL WINAPI DdeKeepStringHandle(DWORD idInst, HSZ hsz)
{
    WDML_INSTANCE*	pInstance;
    BOOL		ret = FALSE;

    TRACE("(%ld,0x%x): \n", idInst, hsz);

    EnterCriticalSection(&WDML_CritSect);

    /*  First check instance
     */
    pInstance = WDML_GetInstance(idInst);
    if (pInstance)
	ret = WDML_IncHSZ(pInstance, hsz);

    LeaveCriticalSection(&WDML_CritSect);
    return ret;
}

/*****************************************************************
 *            DdeCmpStringHandles (USER32.@)
 *
 * Compares the value of two string handles.  This comparison is
 * not case sensitive.
 *
 * Returns:
 * -1 The value of hsz1 is zero or less than hsz2
 * 0  The values of hsz 1 and 2 are the same or both zero.
 * 1  The value of hsz2 is zero of less than hsz1
 */
INT WINAPI DdeCmpStringHandles(HSZ hsz1, HSZ hsz2)
{
    WCHAR	psz1[MAX_BUFFER_LEN];
    WCHAR	psz2[MAX_BUFFER_LEN];
    int		ret = 0;
    int		ret1, ret2;

    ret1 = GetAtomNameW((ATOM)hsz1, psz1, MAX_BUFFER_LEN);
    ret2 = GetAtomNameW((ATOM)hsz2, psz2, MAX_BUFFER_LEN);

    TRACE("(%x<%s> %x<%s>);\n", hsz1, debugstr_w(psz1), hsz2, debugstr_w(psz2));

    /* Make sure we found both strings. */
    if (ret1 == 0 && ret2 == 0)
    {
	/* If both are not found, return both  "zero strings". */
	ret = 0;
    }
    else if (ret1 == 0)
    {
	/* If hsz1 is a not found, return hsz1 is "zero string". */
	ret = -1;
    }
    else if (ret2 == 0)
    {
	/* If hsz2 is a not found, return hsz2 is "zero string". */
	ret = 1;
    }
    else
    {
	/* Compare the two strings we got (case insensitive). */
	ret = lstrcmpiW(psz1, psz2);
	/* Since strcmp returns any number smaller than
	 * 0 when the first string is found to be less than
	 * the second one we must make sure we are returning
	 * the proper values.
	 */
	if (ret < 0)
	{
	    ret = -1;
	}
	else if (ret > 0)
	{
	    ret = 1;
	}
    }

    return ret;
}

/* ================================================================
 *
 * 			Data handle management
 *
 * ================================================================ */

/*****************************************************************
 *            DdeCreateDataHandle (USER32.@)
 */
HDDEDATA WINAPI DdeCreateDataHandle(DWORD idInst, LPBYTE pSrc, DWORD cb, DWORD cbOff,
                                    HSZ hszItem, UINT wFmt, UINT afCmd)
{
    /* For now, we ignore idInst, hszItem.
     * The purpose of these arguments still need to be investigated.
     */

    HGLOBAL     		hMem;
    LPBYTE      		pByte;
    DDE_DATAHANDLE_HEAD*	pDdh;

    TRACE("(%ld,%p,%ld,%ld,0x%lx,%d,%d)\n",
	  idInst, pSrc, cb, cbOff, (DWORD)hszItem, wFmt, afCmd);

    if (afCmd != 0 && afCmd != HDATA_APPOWNED)
        return 0;

    /* we use the first 4 bytes to store the size */
    if (!(hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cb + sizeof(DDE_DATAHANDLE_HEAD))))
    {
	ERR("GlobalAlloc failed\n");
	return 0;
    }

    pDdh = (DDE_DATAHANDLE_HEAD*)GlobalLock(hMem);
    if (!pDdh)
    {
        GlobalFree(hMem);
        return 0;
    }

    pDdh->cfFormat = wFmt;
    pDdh->bAppOwned = (afCmd == HDATA_APPOWNED);

    pByte = (LPBYTE)(pDdh + 1);
    if (pSrc)
    {
	memcpy(pByte, pSrc + cbOff, cb);
    }
    GlobalUnlock(hMem);

    return (HDDEDATA)hMem;
}

/*****************************************************************
 *
 *            DdeAddData (USER32.@)
 */
HDDEDATA WINAPI DdeAddData(HDDEDATA hData, LPBYTE pSrc, DWORD cb, DWORD cbOff)
{
    DWORD	old_sz, new_sz;
    LPBYTE	pDst;

    pDst = DdeAccessData(hData, &old_sz);
    if (!pDst) return 0;

    new_sz = cb + cbOff;
    if (new_sz > old_sz)
    {
	DdeUnaccessData(hData);
	hData = GlobalReAlloc((HGLOBAL)hData, new_sz + sizeof(DDE_DATAHANDLE_HEAD),
			      GMEM_MOVEABLE | GMEM_DDESHARE);
	pDst = DdeAccessData(hData, &old_sz);
    }

    if (!pDst) return 0;

    memcpy(pDst + cbOff, pSrc, cb);
    DdeUnaccessData(hData);
    return hData;
}

/******************************************************************************
 * DdeGetData [USER32.@]  Copies data from DDE object to local buffer
 *
 *
 * PARAMS
 * hData	[I] Handle to DDE object
 * pDst		[I] Pointer to destination buffer
 * cbMax	[I] Amount of data to copy
 * cbOff	[I] Offset to beginning of data
 *
 * RETURNS
 *    Size of memory object associated with handle
 */
DWORD WINAPI DdeGetData(HDDEDATA hData, LPBYTE pDst, DWORD cbMax, DWORD cbOff)
{
    DWORD   dwSize, dwRet;
    LPBYTE  pByte;

    TRACE("(%08lx,%p,%ld,%ld)\n",(DWORD)hData, pDst, cbMax, cbOff);

    pByte = DdeAccessData(hData, &dwSize);

    if (pByte)
    {
        if (!pDst)
        {
            dwRet = dwSize;
        }
        else if (cbOff + cbMax < dwSize)
	{
	    dwRet = cbMax;
	}
	else if (cbOff < dwSize)
	{
	    dwRet = dwSize - cbOff;
	}
	else
	{
	    dwRet = 0;
	}
	if (pDst && dwRet != 0)
	{
	    memcpy(pDst, pByte + cbOff, dwRet);
	}
	DdeUnaccessData(hData);
    }
    else
    {
	dwRet = 0;
    }
    return dwRet;
}

/*****************************************************************
 *            DdeAccessData (USER32.@)
 */
LPBYTE WINAPI DdeAccessData(HDDEDATA hData, LPDWORD pcbDataSize)
{
    HGLOBAL			hMem = (HGLOBAL)hData;
    DDE_DATAHANDLE_HEAD*	pDdh;

    TRACE("(%08lx,%p)\n", (DWORD)hData, pcbDataSize);

    pDdh = (DDE_DATAHANDLE_HEAD*)GlobalLock(hMem);
    if (pDdh == NULL)
    {
	ERR("Failed on GlobalLock(%04x)\n", hMem);
	return 0;
    }

    if (pcbDataSize != NULL)
    {
	*pcbDataSize = GlobalSize(hMem) - sizeof(DDE_DATAHANDLE_HEAD);
    }
    TRACE("=> %08lx (%lu)\n", (DWORD)(pDdh + 1), GlobalSize(hMem) - sizeof(DDE_DATAHANDLE_HEAD));
    return (LPBYTE)(pDdh + 1);
}

/*****************************************************************
 *            DdeUnaccessData (USER32.@)
 */
BOOL WINAPI DdeUnaccessData(HDDEDATA hData)
{
    HGLOBAL hMem = (HGLOBAL)hData;

    TRACE("(0x%lx)\n", (DWORD)hData);

    GlobalUnlock(hMem);

    return TRUE;
}

/*****************************************************************
 *            DdeFreeDataHandle   (USER32.@)
 */
BOOL WINAPI DdeFreeDataHandle(HDDEDATA hData)
{
    return GlobalFree((HGLOBAL)hData) == 0;
}

/******************************************************************
 *		WDML_IsAppOwned
 *
 *
 */
BOOL WDML_IsAppOwned(HDDEDATA hData)
{
    DDE_DATAHANDLE_HEAD*	pDdh;
    BOOL                        ret = FALSE;

    pDdh = (DDE_DATAHANDLE_HEAD*)GlobalLock((HGLOBAL)hData);
    if (pDdh != NULL)
    {
        ret = pDdh->bAppOwned;
        GlobalUnlock((HGLOBAL)hData);
    }
    return ret;
}

/* ================================================================
 *
 *                  Global <=> Data handle management
 *
 * ================================================================ */

/* Note: we use a DDEDATA, but layout of DDEDATA, DDEADVISE and DDEPOKE structures is similar:
 *    offset	  size
 *    (bytes)	 (bits)	comment
 *	0	   16	bit fields for options (release, ackreq, response...)
 *	2	   16	clipboard format
 *	4	   ?	data to be used
 */
HDDEDATA        WDML_Global2DataHandle(HGLOBAL hMem, WINE_DDEHEAD* p)
{
    DDEDATA*    pDd;
    HDDEDATA	ret = 0;
    DWORD       size;

    if (hMem)
    {
        pDd = GlobalLock(hMem);
        size = GlobalSize(hMem) - sizeof(WINE_DDEHEAD);
        if (pDd)
        {
	    if (p) memcpy(p, pDd, sizeof(WINE_DDEHEAD));
            switch (pDd->cfFormat)
            {
            default:
                FIXME("Unsupported format (%d) for data... assuming raw information\n",
                      pDd->cfFormat);
                /* fall thru */
            case 0:
            case CF_TEXT:
                ret = DdeCreateDataHandle(0, pDd->Value, size, 0, 0, pDd->cfFormat, 0);
                break;
            case CF_BITMAP:
                if (size >= sizeof(BITMAP))
                {
                    BITMAP*     bmp = (BITMAP*)pDd->Value;
                    int         count = bmp->bmWidthBytes * bmp->bmHeight * bmp->bmPlanes;
                    if (size >= sizeof(BITMAP) + count)
                    {
                        HBITMAP hbmp;

                        if ((hbmp = CreateBitmap(bmp->bmWidth, bmp->bmHeight,
                                                 bmp->bmPlanes, bmp->bmBitsPixel,
                                                 pDd->Value + sizeof(BITMAP))))
                        {
                            ret = DdeCreateDataHandle(0, (LPBYTE)&hbmp, sizeof(hbmp),
                                                      0, 0, CF_BITMAP, 0);
                        }
                        else ERR("Can't create bmp\n");
                    }
                    else
                    {
                        ERR("Wrong count: %lu / %d\n", size, sizeof(BITMAP) + count);
                    }
                } else ERR("No bitmap header\n");
                break;
            }
            GlobalUnlock(hMem);
        }
    }
    return ret;
}

/******************************************************************
 *		WDML_DataHandle2Global
 *
 *
 */
HGLOBAL WDML_DataHandle2Global(HDDEDATA hDdeData, BOOL fResponse, BOOL fRelease,
			       BOOL fDeferUpd, BOOL fAckReq)
{
    DDE_DATAHANDLE_HEAD*	pDdh;
    DWORD                       dwSize;
    HGLOBAL                     hMem = 0;

    dwSize = GlobalSize((HGLOBAL)hDdeData) - sizeof(DDE_DATAHANDLE_HEAD);
    pDdh = (DDE_DATAHANDLE_HEAD*)GlobalLock((HGLOBAL)hDdeData);
    if (dwSize && pDdh)
    {
        WINE_DDEHEAD*    wdh = NULL;

        switch (pDdh->cfFormat)
        {
        default:
            FIXME("Unsupported format (%d) for data... passing raw information\n", pDdh->cfFormat);
            /* fall thru */
        case 0:
        case CF_TEXT:
            hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(WINE_DDEHEAD) + dwSize);
            if (hMem && (wdh = GlobalLock(hMem)))
            {
                memcpy(wdh + 1, pDdh + 1, dwSize);
            }
            break;
        case CF_BITMAP:
            if (dwSize >= sizeof(HBITMAP))
            {
                BITMAP  bmp;
                DWORD   count;
                HBITMAP hbmp = *(HBITMAP*)(pDdh + 1);

                if (GetObjectA(hbmp, sizeof(bmp), &bmp))
                {
                    count = bmp.bmWidthBytes * bmp.bmHeight;
                    hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
                                       sizeof(WINE_DDEHEAD) + sizeof(bmp) + count);
                    if (hMem && (wdh = GlobalLock(hMem)))
                    {
                        memcpy(wdh + 1, &bmp, sizeof(bmp));
                        GetBitmapBits(hbmp, count, ((char*)(wdh + 1)) + sizeof(bmp));
                    }
                }
            }
            break;
        }
        if (wdh)
        {
            wdh->fResponse = fResponse;
            wdh->fRelease = fRelease;
            wdh->fDeferUpd = fDeferUpd;
            wdh->fAckReq = fAckReq;
            wdh->cfFormat = pDdh->cfFormat;
            GlobalUnlock(hMem);
        }
        GlobalUnlock((HGLOBAL)hDdeData);
    }

    return hMem;
}

/* ================================================================
 *
 * 			Server management
 *
 * ================================================================ */

/******************************************************************
 *		WDML_AddServer
 *
 *
 */
WDML_SERVER*	WDML_AddServer(WDML_INSTANCE* pInstance, HSZ hszService, HSZ hszTopic)
{
    WDML_SERVER* 	pServer;
    char		buf1[256];
    char		buf2[256];

    pServer = (WDML_SERVER*)HeapAlloc(GetProcessHeap(), 0, sizeof(WDML_SERVER));
    if (pServer == NULL) return NULL;

    WDML_IncHSZ(pInstance, pServer->hszService = hszService);

    DdeQueryStringA(pInstance->instanceID, hszService, buf1, sizeof(buf1), CP_WINANSI);
    snprintf(buf2, sizeof(buf2), "%s(0x%08lx)", buf1, GetCurrentProcessId());
    pServer->hszServiceSpec = DdeCreateStringHandleA(pInstance->instanceID, buf2, CP_WINANSI);

    pServer->atomService = WDML_MakeAtomFromHsz(pServer->hszService);
    pServer->atomServiceSpec = WDML_MakeAtomFromHsz(pServer->hszServiceSpec);

    pServer->filterOn = TRUE;

    pServer->next = pInstance->servers;
    pInstance->servers = pServer;
    return pServer;
}

/******************************************************************
 *		WDML_RemoveServer
 *
 *
 */
void WDML_RemoveServer(WDML_INSTANCE* pInstance, HSZ hszService, HSZ hszTopic)
{
    WDML_SERVER*	pPrev = NULL;
    WDML_SERVER*	pServer = NULL;
    WDML_CONV*		pConv;
    WDML_CONV*		pConvNext;

    pServer = pInstance->servers;

    while (pServer != NULL)
    {
	if (DdeCmpStringHandles(pServer->hszService, hszService) == 0)
	{
	    WDML_BroadcastDDEWindows(WDML_szEventClass, WM_WDML_UNREGISTER,
				     pServer->atomService, pServer->atomServiceSpec);
	    /* terminate all conversations for given topic */
	    for (pConv = pInstance->convs[WDML_SERVER_SIDE]; pConv != NULL; pConv = pConvNext)
	    {
		pConvNext = pConv->next;
		if (DdeCmpStringHandles(pConv->hszService, hszService) == 0)
		{
		    WDML_RemoveConv(pConv, WDML_SERVER_SIDE);
		    /* don't care about return code (whether client window is present or not) */
		    PostMessageA(pConv->hwndClient, WM_DDE_TERMINATE, (WPARAM)pConv->hwndServer, 0L);
		}
	    }
	    if (pServer == pInstance->servers)
	    {
		pInstance->servers = pServer->next;
	    }
	    else
	    {
		pPrev->next = pServer->next;
	    }

	    DestroyWindow(pServer->hwndServer);
	    WDML_DecHSZ(pInstance, pServer->hszServiceSpec);
	    WDML_DecHSZ(pInstance, pServer->hszService);

	    GlobalDeleteAtom(pServer->atomService);
	    GlobalDeleteAtom(pServer->atomServiceSpec);

	    HeapFree(GetProcessHeap(), 0, pServer);
	    break;
	}

	pPrev = pServer;
	pServer = pServer->next;
    }
}

/*****************************************************************************
 *	WDML_FindServer
 *
 *	generic routine to return a pointer to the relevant ServiceNode
 *	for a given service name, or NULL if the entry does not exist
 *
 */
WDML_SERVER*	WDML_FindServer(WDML_INSTANCE* pInstance, HSZ hszService, HSZ hszTopic)
{
    WDML_SERVER*	pServer;

    for (pServer = pInstance->servers; pServer != NULL; pServer = pServer->next)
    {
	if (hszService == pServer->hszService)
	{
	    return pServer;
	}
    }
    TRACE("Service name missing\n");
    return NULL;
}

/* ================================================================
 *
 * 		Conversation management
 *
 * ================================================================ */

/******************************************************************
 *		WDML_AddConv
 *
 *
 */
WDML_CONV*	WDML_AddConv(WDML_INSTANCE* pInstance, WDML_SIDE side,
			     HSZ hszService, HSZ hszTopic, HWND hwndClient, HWND hwndServer)
{
    WDML_CONV*	pConv;

    /* no converstation yet, add it */
    pConv = HeapAlloc(GetProcessHeap(), 0, sizeof(WDML_CONV));
    if (!pConv) return NULL;

    pConv->instance = pInstance;
    WDML_IncHSZ(pInstance, pConv->hszService = hszService);
    WDML_IncHSZ(pInstance, pConv->hszTopic = hszTopic);
    pConv->hwndServer = hwndServer;
    pConv->hwndClient = hwndClient;
    pConv->transactions = NULL;
    pConv->hUser = 0;
    pConv->wStatus = (side == WDML_CLIENT_SIDE) ? ST_CLIENT : 0L;
    /* check if both side of the conversation are of the same instance */
    if (GetWindowThreadProcessId(hwndClient, NULL) == GetWindowThreadProcessId(hwndServer, NULL) &&
	WDML_GetInstanceFromWnd(hwndClient) == WDML_GetInstanceFromWnd(hwndServer))
    {
	pConv->wStatus |= ST_ISSELF;
    }
    pConv->wConvst = XST_NULL;

    pConv->next = pInstance->convs[side];
    pInstance->convs[side] = pConv;

    return pConv;
}

/******************************************************************
 *		WDML_FindConv
 *
 *
 */
WDML_CONV*	WDML_FindConv(WDML_INSTANCE* pInstance, WDML_SIDE side,
			      HSZ hszService, HSZ hszTopic)
{
    WDML_CONV*	pCurrent = NULL;

    for (pCurrent = pInstance->convs[side]; pCurrent != NULL; pCurrent = pCurrent->next)
    {
	if (DdeCmpStringHandles(pCurrent->hszService, hszService) == 0 &&
	    DdeCmpStringHandles(pCurrent->hszTopic, hszTopic) == 0)
	{
	    return pCurrent;
	}

    }
    return NULL;
}

/******************************************************************
 *		WDML_RemoveConv
 *
 *
 */
void WDML_RemoveConv(WDML_CONV* pRef, WDML_SIDE side)
{
    WDML_CONV*	pPrev = NULL;
    WDML_CONV* 	pCurrent;
    WDML_XACT*	pXAct;
    WDML_XACT*	pXActNext;
    HWND	hWnd;

    if (!pRef)
	return;

    /* remove any pending transaction */
    for (pXAct = pRef->transactions; pXAct != NULL; pXAct = pXActNext)
    {
	pXActNext = pXAct->next;
	WDML_FreeTransaction(pRef->instance, pXAct, TRUE);
    }

    WDML_RemoveAllLinks(pRef->instance, pRef, side);

    /* FIXME: should we keep the window around ? it seems so (at least on client side
     * to let QueryConvInfo work after conv termination, but also to implement
     * DdeReconnect...
     */
    /* destroy conversation window, but first remove pConv from hWnd.
     * this would help the wndProc do appropriate handling upon a WM_DESTROY message
     */
    hWnd = (side == WDML_CLIENT_SIDE) ? pRef->hwndClient : pRef->hwndServer;
    SetWindowLongA(hWnd, GWL_WDML_CONVERSATION, 0);

    DestroyWindow((side == WDML_CLIENT_SIDE) ? pRef->hwndClient : pRef->hwndServer);

    WDML_DecHSZ(pRef->instance, pRef->hszService);
    WDML_DecHSZ(pRef->instance, pRef->hszTopic);

    for (pCurrent = pRef->instance->convs[side]; pCurrent != NULL; pCurrent = (pPrev = pCurrent)->next)
    {
	if (pCurrent == pRef)
	{
	    if (pCurrent == pRef->instance->convs[side])
	    {
		pRef->instance->convs[side] = pCurrent->next;
	    }
	    else
	    {
		pPrev->next = pCurrent->next;
	    }

	    HeapFree(GetProcessHeap(), 0, pCurrent);
	    break;
	}
    }
}

/*****************************************************************
 *            DdeEnableCallback (USER32.@)
 */
BOOL WINAPI DdeEnableCallback(DWORD idInst, HCONV hConv, UINT wCmd)
{
    FIXME("(%ld, 0x%x, %d) stub\n", idInst, hConv, wCmd);

    return 0;
}

/******************************************************************
 *		WDML_GetConv
 *
 *
 */
WDML_CONV*	WDML_GetConv(HCONV hConv, BOOL checkConnected)
{
    WDML_CONV*	pConv = (WDML_CONV*)hConv;

    /* FIXME: should do better checking */

    if (checkConnected && !(pConv->wStatus & ST_CONNECTED))
    {
	FIXME("found conv but ain't connected\n");
	return NULL;
    }
    if (GetCurrentThreadId() != pConv->instance->threadID)
    {
	FIXME("wrong thread ID\n");
	return NULL;
    }

    return pConv;
}

/******************************************************************
 *		WDML_GetConvFromWnd
 *
 *
 */
WDML_CONV*	WDML_GetConvFromWnd(HWND hWnd)
{
    return (WDML_CONV*)GetWindowLongA(hWnd, GWL_WDML_CONVERSATION);
}

/******************************************************************
 *		WDML_PostAck
 *
 *
 */
BOOL		WDML_PostAck(WDML_CONV* pConv, WDML_SIDE side, WORD appRetCode,
			     BOOL fBusy, BOOL fAck, UINT pmt, LPARAM lParam, UINT oldMsg)
{
    DDEACK	ddeAck;
    HWND	from, to;

    if (side == WDML_SERVER_SIDE)
    {
	from = pConv->hwndServer;
	to   = pConv->hwndClient;
    }
    else
    {
	to   = pConv->hwndServer;
	from = pConv->hwndClient;
    }

    ddeAck.bAppReturnCode = appRetCode;
    ddeAck.reserved       = 0;
    ddeAck.fBusy          = fBusy;
    ddeAck.fAck           = fAck;

    TRACE("Posting a %s ack\n", ddeAck.fAck ? "positive" : "negative");

    lParam = (lParam) ? ReuseDDElParam(lParam, oldMsg, WM_DDE_ACK, *(WORD*)&ddeAck, pmt) :
        PackDDElParam(WM_DDE_ACK, *(WORD*)&ddeAck, pmt);
    if (!PostMessageA(to, WM_DDE_ACK, (WPARAM)from, lParam))
    {
	pConv->wStatus &= ~ST_CONNECTED;
        FreeDDElParam(WM_DDE_ACK, lParam);
        return FALSE;
    }
    return TRUE;
}

/*****************************************************************
 *            DdeSetUserHandle (USER32.@)
 */
BOOL WINAPI DdeSetUserHandle(HCONV hConv, DWORD id, DWORD hUser)
{
    WDML_CONV*	pConv;
    BOOL	ret = TRUE;

    EnterCriticalSection(&WDML_CritSect);

    pConv = WDML_GetConv(hConv, FALSE);
    if (pConv == NULL)
    {
	ret = FALSE;
	goto theError;
    }
    if (id == QID_SYNC)
    {
	pConv->hUser = hUser;
    }
    else
    {
	WDML_XACT*	pXAct;

	pXAct = WDML_FindTransaction(pConv, id);
	if (pXAct)
	{
	    pXAct->hUser = hUser;
	}
	else
	{
	    pConv->instance->lastError = DMLERR_UNFOUND_QUEUE_ID;
	    ret = FALSE;
	}
    }
 theError:
    LeaveCriticalSection(&WDML_CritSect);
    return ret;
}

/******************************************************************
 *		WDML_GetLocalConvInfo
 *
 *
 */
static	BOOL	WDML_GetLocalConvInfo(WDML_CONV* pConv, CONVINFO* ci, DWORD id)
{
    BOOL 	ret = TRUE;
    WDML_LINK*	pLink;
    WDML_SIDE	side;

    ci->hConvPartner = (pConv->wStatus & ST_ISLOCAL) ? (HCONV)((DWORD)pConv | 1) : 0;
    ci->hszSvcPartner = pConv->hszService;
    ci->hszServiceReq = pConv->hszService; /* FIXME: they shouldn't be the same, should they ? */
    ci->hszTopic = pConv->hszTopic;
    ci->wStatus = pConv->wStatus;

    side = (pConv->wStatus & ST_CLIENT) ? WDML_CLIENT_SIDE : WDML_SERVER_SIDE;

    for (pLink = pConv->instance->links[side]; pLink != NULL; pLink = pLink->next)
    {
	if (pLink->hConv == (HCONV)pConv)
	{
	    ci->wStatus |= ST_ADVISE;
	    break;
	}
    }

    /* FIXME: non handled status flags:
       ST_BLOCKED
       ST_BLOCKNEXT
       ST_INLIST
    */

    ci->wConvst = pConv->wConvst; /* FIXME */

    ci->wLastError = 0; /* FIXME: note it's not the instance last error */
    ci->hConvList = 0;
    ci->ConvCtxt = pConv->convContext;
    if (ci->wStatus & ST_CLIENT)
    {
	ci->hwnd = pConv->hwndClient;
	ci->hwndPartner = pConv->hwndServer;
    }
    else
    {
	ci->hwnd = pConv->hwndServer;
	ci->hwndPartner = pConv->hwndClient;
    }
    if (id == QID_SYNC)
    {
	ci->hUser = pConv->hUser;
	ci->hszItem = 0;
	ci->wFmt = 0;
	ci->wType = 0;
    }
    else
    {
	WDML_XACT*	pXAct;

	pXAct = WDML_FindTransaction(pConv, id);
	if (pXAct)
	{
	    ci->hUser = pXAct->hUser;
	    ci->hszItem = pXAct->hszItem;
	    ci->wFmt = pXAct->wFmt;
	    ci->wType = pXAct->wType;
	}
	else
	{
	    ret = 0;
	    pConv->instance->lastError = DMLERR_UNFOUND_QUEUE_ID;
	}
    }
    return ret;
}

/******************************************************************
 *		DdeQueryConvInfo (USER32.@)
 *
 */
UINT WINAPI DdeQueryConvInfo(HCONV hConv, DWORD id, LPCONVINFO lpConvInfo)
{
    UINT	ret = lpConvInfo->cb;
    CONVINFO	ci;
    WDML_CONV*	pConv;

    EnterCriticalSection(&WDML_CritSect);

    pConv = WDML_GetConv(hConv, FALSE);
    if (pConv != NULL && !WDML_GetLocalConvInfo(pConv, &ci, id))
    {
	ret = 0;
    }
    else if ((DWORD)hConv & 1)
    {
	pConv = WDML_GetConv((DWORD)hConv & ~1, FALSE);
	if (pConv != NULL)
	{
	    FIXME("Request on remote conversation information is not implemented yet\n");
	    ret = 0;
	}
    }
    LeaveCriticalSection(&WDML_CritSect);
    if (ret != 0)
	memcpy(lpConvInfo, &ci, min((size_t)lpConvInfo->cb, sizeof(ci)));
    return ret;
}

/* ================================================================
 *
 * 			Link (hot & warm) management
 *
 * ================================================================ */

/******************************************************************
 *		WDML_AddLink
 *
 *
 */
void WDML_AddLink(WDML_INSTANCE* pInstance, HCONV hConv, WDML_SIDE side,
		  UINT wType, HSZ hszItem, UINT wFmt)
{
    WDML_LINK*	pLink;

    pLink = HeapAlloc(GetProcessHeap(), 0, sizeof(WDML_LINK));
    if (pLink == NULL)
    {
	ERR("OOM\n");
	return;
    }

    pLink->hConv = hConv;
    pLink->transactionType = wType;
    WDML_IncHSZ(pInstance, pLink->hszItem = hszItem);
    pLink->uFmt = wFmt;
    pLink->next = pInstance->links[side];
    pInstance->links[side] = pLink;
}

/******************************************************************
 *		WDML_RemoveLink
 *
 *
 */
void WDML_RemoveLink(WDML_INSTANCE* pInstance, HCONV hConv, WDML_SIDE side,
		     HSZ hszItem, UINT uFmt)
{
    WDML_LINK* pPrev = NULL;
    WDML_LINK* pCurrent = NULL;

    pCurrent = pInstance->links[side];

    while (pCurrent != NULL)
    {
	if (pCurrent->hConv == hConv &&
	    DdeCmpStringHandles(pCurrent->hszItem, hszItem) == 0 &&
	    pCurrent->uFmt == uFmt)
	{
	    if (pCurrent == pInstance->links[side])
	    {
		pInstance->links[side] = pCurrent->next;
	    }
	    else
	    {
		pPrev->next = pCurrent->next;
	    }

	    WDML_DecHSZ(pInstance, pCurrent->hszItem);
	    HeapFree(GetProcessHeap(), 0, pCurrent);
	    break;
	}

	pPrev = pCurrent;
	pCurrent = pCurrent->next;
    }
}

/* this function is called to remove all links related to the conv.
   It should be called from both client and server when terminating
   the conversation.
*/
/******************************************************************
 *		WDML_RemoveAllLinks
 *
 *
 */
void WDML_RemoveAllLinks(WDML_INSTANCE* pInstance, WDML_CONV* pConv, WDML_SIDE side)
{
    WDML_LINK* pPrev = NULL;
    WDML_LINK* pCurrent = NULL;
    WDML_LINK* pNext = NULL;

    pCurrent = pInstance->links[side];

    while (pCurrent != NULL)
    {
	if (pCurrent->hConv == (HCONV)pConv)
	{
	    if (pCurrent == pInstance->links[side])
	    {
		pInstance->links[side] = pCurrent->next;
		pNext = pCurrent->next;
	    }
	    else
	    {
		pPrev->next = pCurrent->next;
		pNext = pCurrent->next;
	    }

	    WDML_DecHSZ(pInstance, pCurrent->hszItem);

	    HeapFree(GetProcessHeap(), 0, pCurrent);
	    pCurrent = NULL;
	}

	if (pCurrent)
	{
	    pPrev = pCurrent;
	    pCurrent = pCurrent->next;
	}
	else
	{
	    pCurrent = pNext;
	}
    }
}

/******************************************************************
 *		WDML_FindLink
 *
 *
 */
WDML_LINK* 	WDML_FindLink(WDML_INSTANCE* pInstance, HCONV hConv, WDML_SIDE side,
			      HSZ hszItem, BOOL use_fmt, UINT uFmt)
{
    WDML_LINK*	pCurrent = NULL;

    for (pCurrent = pInstance->links[side]; pCurrent != NULL; pCurrent = pCurrent->next)
    {
	/* we don't need to check for transaction type as it can be altered */

	if (pCurrent->hConv == hConv &&
	    DdeCmpStringHandles(pCurrent->hszItem, hszItem) == 0 &&
	    (!use_fmt || pCurrent->uFmt == uFmt))
	{
	    break;
	}

    }

    return pCurrent;
}

/* ================================================================
 *
 * 			Transaction management
 *
 * ================================================================ */

/******************************************************************
 *		WDML_AllocTransaction
 *
 * Alloc a transaction structure for handling the message ddeMsg
 */
WDML_XACT*	WDML_AllocTransaction(WDML_INSTANCE* pInstance, UINT ddeMsg,
				      UINT wFmt, HSZ hszItem)
{
    WDML_XACT*		pXAct;
    static WORD		tid = 1;	/* FIXME: wrap around */

    pXAct = HeapAlloc(GetProcessHeap(), 0, sizeof(WDML_XACT));
    if (!pXAct)
    {
	pInstance->lastError = DMLERR_MEMORY_ERROR;
	return NULL;
    }

    pXAct->xActID = tid++;
    pXAct->ddeMsg = ddeMsg;
    pXAct->hDdeData = 0;
    pXAct->hUser = 0;
    pXAct->next = NULL;
    pXAct->wType = 0;
    pXAct->wFmt = wFmt;
    if ((pXAct->hszItem = hszItem)) WDML_IncHSZ(pInstance, pXAct->hszItem);
    pXAct->atom = 0;
    pXAct->hMem = 0;
    pXAct->lParam = 0;

    return pXAct;
}

/******************************************************************
 *		WDML_QueueTransaction
 *
 * Adds a transaction to the list of transaction
 */
void	WDML_QueueTransaction(WDML_CONV* pConv, WDML_XACT* pXAct)
{
    WDML_XACT**	pt;

    /* advance to last in queue */
    for (pt = &pConv->transactions; *pt != NULL; pt = &(*pt)->next);
    *pt = pXAct;
}

/******************************************************************
 *		WDML_UnQueueTransaction
 *
 *
 */
BOOL	WDML_UnQueueTransaction(WDML_CONV* pConv, WDML_XACT*  pXAct)
{
    WDML_XACT**	pt;

    for (pt = &pConv->transactions; *pt; pt = &(*pt)->next)
    {
	if (*pt == pXAct)
	{
	    *pt = pXAct->next;
	    return TRUE;
	}
    }
    return FALSE;
}

/******************************************************************
 *		WDML_FreeTransaction
 *
 *
 */
void	WDML_FreeTransaction(WDML_INSTANCE* pInstance, WDML_XACT* pXAct, BOOL doFreePmt)
{
    /* free pmt(s) in pXAct too. check against one for not deleting TRUE return values */
    if (doFreePmt && (DWORD)pXAct->hMem > 1)
    {
	GlobalFree(pXAct->hMem);
    }
    if (pXAct->hszItem) WDML_DecHSZ(pInstance, pXAct->hszItem);

    HeapFree(GetProcessHeap(), 0, pXAct);
}

/******************************************************************
 *		WDML_FindTransaction
 *
 *
 */
WDML_XACT*	WDML_FindTransaction(WDML_CONV* pConv, DWORD tid)
{
    WDML_XACT* pXAct;

    tid = HIWORD(tid);
    for (pXAct = pConv->transactions; pXAct; pXAct = pXAct->next)
    {
	if (pXAct->xActID == tid)
	    break;
    }
    return pXAct;
}

/* ================================================================
 *
 * 	   Information broadcast across DDEML implementations
 *
 * ================================================================ */

struct tagWDML_BroadcastPmt
{
    LPCSTR	clsName;
    UINT	uMsg;
    WPARAM	wParam;
    LPARAM	lParam;
};

/******************************************************************
 *		WDML_BroadcastEnumProc
 *
 *
 */
static	BOOL CALLBACK WDML_BroadcastEnumProc(HWND hWnd, LPARAM lParam)
{
    struct tagWDML_BroadcastPmt*	s = (struct tagWDML_BroadcastPmt*)lParam;
    char				buffer[128];

    if (GetClassNameA(hWnd, buffer, sizeof(buffer)) > 0 &&
	strcmp(buffer, s->clsName) == 0)
    {
	PostMessageA(hWnd, s->uMsg, s->wParam, s->lParam);
    }
    return TRUE;
}

/******************************************************************
 *		WDML_BroadcastDDEWindows
 *
 *
 */
void WDML_BroadcastDDEWindows(const char* clsName, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    struct tagWDML_BroadcastPmt	s;

    s.clsName = clsName;
    s.uMsg    = uMsg;
    s.wParam  = wParam;
    s.lParam  = lParam;
    EnumWindows(WDML_BroadcastEnumProc, (LPARAM)&s);
}
