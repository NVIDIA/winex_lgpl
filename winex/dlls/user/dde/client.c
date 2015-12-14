/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 * DDEML library
 *
 * Copyright 1997 Alexandre Julliard
 * Copyright 1997 Len White
 * Copyright 1999 Keith Matthews
 * Copyright 2000 Corel
 * Copyright 2001 Eric Pouech
 */

#include <string.h>
#include "winbase.h"
#include "windef.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "winnls.h"
#include "dde.h"
#include "ddeml.h"
#include "win.h"
#include "wine/debug.h"
#include "dde/dde_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(ddeml);

static LRESULT CALLBACK WDML_ClientProc(HWND, UINT, WPARAM, LPARAM);	/* only for one client, not conv list */
const char  WDML_szClientConvClassA[] = "DdeClientAnsi";
const WCHAR WDML_szClientConvClassW[] = {'D','d','e','C','l','i','e','n','t','U','n','i','c','o','d','e',0};

/******************************************************************************
 * DdeConnectList [USER32.@]  Establishes conversation with DDE servers
 *
 * PARAMS
 *    idInst     [I] Instance identifier
 *    hszService [I] Handle to service name string
 *    hszTopic   [I] Handle to topic name string
 *    hConvList  [I] Handle to conversation list
 *    pCC        [I] Pointer to structure with context data
 *
 * RETURNS
 *    Success: Handle to new conversation list
 *    Failure: 0
 */
HCONVLIST WINAPI DdeConnectList(DWORD idInst, HSZ hszService, HSZ hszTopic,
				HCONVLIST hConvList, LPCONVCONTEXT pCC)
{
    FIXME("(%ld,%d,%d,%d,%p): stub\n", idInst, hszService, hszTopic, hConvList, pCC);
    return (HCONVLIST)1;
}

/*****************************************************************
 * DdeQueryNextServer [USER32.@]
 */
HCONV WINAPI DdeQueryNextServer(HCONVLIST hConvList, HCONV hConvPrev)
{
    FIXME("(%d,%d): stub\n", hConvList, hConvPrev);
    return 0;
}

/******************************************************************************
 * DdeDisconnectList [USER32.@]  Destroys list and terminates conversations
 *
 *
 * PARAMS
 *    hConvList  [I] Handle to conversation list
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI DdeDisconnectList(HCONVLIST hConvList)
{
    FIXME("(%d): stub\n", hConvList);
    return TRUE;
}

/*****************************************************************
 *            DdeConnect   (USER32.@)
 */
HCONV WINAPI DdeConnect(DWORD idInst, HSZ hszService, HSZ hszTopic,
			LPCONVCONTEXT pCC)
{
    HWND		hwndClient;
    WDML_INSTANCE*	pInstance;
    WDML_CONV*		pConv = NULL;
    ATOM		aSrv = 0, aTpc = 0;

    TRACE("(0x%lx,0x%x,0x%x,%p)\n", idInst, hszService, hszTopic, pCC);

    EnterCriticalSection(&WDML_CritSect);

    pInstance = WDML_GetInstance(idInst);
    if (!pInstance)
    {
	goto theEnd;
    }

    /* make sure this conv is never created */
    pConv = WDML_FindConv(pInstance, WDML_CLIENT_SIDE, hszService, hszTopic);
    if (pConv != NULL)
    {
	ERR("This Conv already exists: (0x%lx)\n", (DWORD)pConv);
	goto theEnd;
    }

    /* we need to establish a conversation with
       server, so create a window for it       */

    if (pInstance->unicode)
    {
	WNDCLASSEXW	wndclass;

	wndclass.cbSize        = sizeof(wndclass);
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = WDML_ClientProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 2 * sizeof(DWORD);
	wndclass.hInstance     = 0;
	wndclass.hIcon         = 0;
	wndclass.hCursor       = 0;
	wndclass.hbrBackground = 0;
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = WDML_szClientConvClassW;
	wndclass.hIconSm       = 0;

	RegisterClassExW(&wndclass);

	hwndClient = CreateWindowW(WDML_szClientConvClassW, NULL, WS_POPUP, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    else
    {
	WNDCLASSEXA	wndclass;

	wndclass.cbSize        = sizeof(wndclass);
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = WDML_ClientProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 2 * sizeof(DWORD);
	wndclass.hInstance     = 0;
	wndclass.hIcon         = 0;
	wndclass.hCursor       = 0;
	wndclass.hbrBackground = 0;
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = WDML_szClientConvClassA;
	wndclass.hIconSm       = 0;

	RegisterClassExA(&wndclass);

	hwndClient = CreateWindowA(WDML_szClientConvClassA, NULL, WS_POPUP, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    SetWindowLongA(hwndClient, GWL_WDML_INSTANCE, (DWORD)pInstance);

    if (hszService)
    {
	aSrv = WDML_MakeAtomFromHsz(hszService);
	if (!aSrv) goto theEnd;
    }
    if (hszTopic)
    {
	aTpc = WDML_MakeAtomFromHsz(hszTopic);
	if (!aTpc) goto theEnd;
    }

    LeaveCriticalSection(&WDML_CritSect);

    /* note: sent messages shall not use packing */
    SendMessageA(HWND_BROADCAST, WM_DDE_INITIATE, (WPARAM)hwndClient, MAKELPARAM(aSrv, aTpc));

    EnterCriticalSection(&WDML_CritSect);

    pInstance = WDML_GetInstance(idInst);
    if (!pInstance)
    {
	goto theEnd;
    }

    /* At this point, Client WM_DDE_ACK should have saved hwndServer
       for this instance id and hwndClient if server responds.
       So get HCONV and return it. And add it to conv list */
    pConv = WDML_GetConvFromWnd(hwndClient);
    if (pConv == NULL || pConv->hwndServer == 0)
    {
	ERR("Done with INITIATE, but no Server window available\n");
	pConv = NULL;
	goto theEnd;
    }
    TRACE("Connected to Server window (%x)\n", pConv->hwndServer);
    pConv->wConvst = XST_CONNECTED;

    /* finish init of pConv */
    if (pCC != NULL)
    {
	pConv->convContext = *pCC;
    }
    else
    {
	memset(&pConv->convContext, 0, sizeof(pConv->convContext));
	pConv->convContext.cb = sizeof(pConv->convContext);
	pConv->convContext.iCodePage = (pInstance->unicode) ? CP_WINUNICODE : CP_WINANSI;
    }

 theEnd:
    LeaveCriticalSection(&WDML_CritSect);

    if (aSrv) GlobalDeleteAtom(aSrv);
    if (aTpc) GlobalDeleteAtom(aTpc);
    return (HCONV)pConv;
}

/*****************************************************************
 *            DdeReconnect   (DDEML.37)
 *            DdeReconnect   (USER32.@)
 */
HCONV WINAPI DdeReconnect(HCONV hConv)
{
    WDML_CONV*	pConv;
    WDML_CONV*	pNewConv = NULL;
    ATOM	aSrv = 0, aTpc = 0;

    EnterCriticalSection(&WDML_CritSect);
    pConv = WDML_GetConv(hConv, FALSE);
    if (pConv != NULL && (pConv->wStatus & ST_CLIENT))
    {
	BOOL	ret;

	/* to reestablist a connection, we have to make sure that:
	 * 1/ pConv is the converstation attached to the client window (it wouldn't be
	 *    if a call to DdeReconnect would have already been done...)
	 *    FIXME: is this really an error ???
	 * 2/ the pConv conversation had really been deconnected
	 */
	if (pConv == WDML_GetConvFromWnd(pConv->hwndClient) &&
	    (pConv->wStatus & ST_TERMINATED) && !(pConv->wStatus & ST_CONNECTED))
	{
	    HWND	hwndClient = pConv->hwndClient;
	    HWND	hwndServer = pConv->hwndServer;
	    ATOM	aSrv, aTpc;

	    SetWindowLongA(pConv->hwndClient, GWL_WDML_CONVERSATION, 0);

	    aSrv = WDML_MakeAtomFromHsz(pConv->hszService);
	    aTpc = WDML_MakeAtomFromHsz(pConv->hszTopic);
	    if (!aSrv || !aTpc)	goto theEnd;

	    LeaveCriticalSection(&WDML_CritSect);

            /* note: sent messages shall not use packing */
	    ret = SendMessageA(hwndServer, WM_DDE_INITIATE, (WPARAM)hwndClient,
                               MAKELPARAM(aSrv, aTpc));

	    EnterCriticalSection(&WDML_CritSect);

	    pConv = WDML_GetConv(hConv, FALSE);
	    if (pConv == NULL)
	    {
		FIXME("Should fail reconnection\n");
		goto theEnd;
	    }

	    if (ret && (pNewConv = WDML_GetConvFromWnd(pConv->hwndClient)) != NULL)
	    {
		/* re-establish all links... */
		WDML_LINK* pLink;

		for (pLink = pConv->instance->links[WDML_CLIENT_SIDE]; pLink; pLink = pLink->next)
		{
		    if (pLink->hConv == hConv)
		    {
			/* try to reestablish the links... */
			DdeClientTransaction(NULL, 0, (HCONV)pNewConv, pLink->hszItem, pLink->uFmt,
					     pLink->transactionType, 1000, NULL);
		    }
		}
	    }
	    else
	    {
		/* reset the conversation as it was */
		SetWindowLongA(pConv->hwndClient, GWL_WDML_CONVERSATION, (DWORD)pConv);
	    }
	}
    }

 theEnd:
    LeaveCriticalSection(&WDML_CritSect);

    if (aSrv) GlobalDeleteAtom(aSrv);
    if (aTpc) GlobalDeleteAtom(aTpc);
    return (HCONV)pNewConv;
}

/******************************************************************
 *		WDML_ClientQueueAdvise
 *
 * Creates and queue an WM_DDE_ADVISE transaction
 */
static WDML_XACT*	WDML_ClientQueueAdvise(WDML_CONV* pConv, UINT wType, UINT wFmt, HSZ hszItem)
{
    DDEADVISE*		pDdeAdvise;
    WDML_XACT*		pXAct;
    ATOM		atom;

    TRACE("XTYP_ADVSTART (with%s data) transaction\n", (wType & XTYPF_NODATA) ? "out" : "");

    atom = WDML_MakeAtomFromHsz(hszItem);
    if (!atom) return NULL;

    pXAct = WDML_AllocTransaction(pConv->instance, WM_DDE_ADVISE, wFmt, hszItem);
    if (!pXAct)
    {
	GlobalDeleteAtom(atom);
	return NULL;
    }

    pXAct->wType = wType & ~0x0F;
    pXAct->hMem = GlobalAlloc(GHND | GMEM_DDESHARE, sizeof(DDEADVISE));
    /* FIXME: hMem is unfreed for now... should be deleted in server */

    /* pack DdeAdvise	*/
    pDdeAdvise = (DDEADVISE*)GlobalLock(pXAct->hMem);
    pDdeAdvise->fAckReq   = (wType & XTYPF_ACKREQ) ? TRUE : FALSE;
    pDdeAdvise->fDeferUpd = (wType & XTYPF_NODATA) ? TRUE : FALSE;
    pDdeAdvise->cfFormat  = wFmt;
    GlobalUnlock(pXAct->hMem);

    pXAct->lParam = PackDDElParam(WM_DDE_ADVISE, (UINT)pXAct->hMem, atom);

    return pXAct;
}

/******************************************************************
 *		WDML_HandleAdviseReply
 *
 * handles the reply to an advise request
 */
static WDML_QUEUE_STATE WDML_HandleAdviseReply(WDML_CONV* pConv, MSG* msg, WDML_XACT* pXAct)
{
    DDEACK		ddeAck;
    UINT		uiLo, uiHi;
    HSZ			hsz;

    if (msg->message != WM_DDE_ACK || WIN_GetFullHandle(msg->wParam) != pConv->hwndServer)
    {
	return WDML_QS_PASS;
    }

    UnpackDDElParam(WM_DDE_ACK, msg->lParam, &uiLo, &uiHi);
    hsz = WDML_MakeHszFromAtom(pConv->instance, uiHi);

    if (DdeCmpStringHandles(hsz, pXAct->hszItem) != 0)
	return WDML_QS_PASS;

    GlobalDeleteAtom(uiHi);
    FreeDDElParam(WM_DDE_ACK, msg->lParam);

    WDML_ExtractAck(uiLo, &ddeAck);

    if (ddeAck.fAck)
    {
	WDML_LINK*	pLink;

	/* billx: first to see if the link is already created. */
	pLink = WDML_FindLink(pConv->instance, (HCONV)pConv, WDML_CLIENT_SIDE,
			      pXAct->hszItem, TRUE, pXAct->wFmt);
	if (pLink != NULL)
	{
	    /* we found a link, and only need to modify it in case it changes */
	    pLink->transactionType = pXAct->wType;
	}
	else
	{
	    WDML_AddLink(pConv->instance, (HCONV)pConv, WDML_CLIENT_SIDE,
			 pXAct->wType, pXAct->hszItem, pXAct->wFmt);
	}
        pXAct->hDdeData = (HDDEDATA)1;
    }
    else
    {
	TRACE("Returning FALSE on XTYP_ADVSTART - fAck was FALSE\n");
	GlobalFree(pXAct->hMem);
        pXAct->hDdeData = (HDDEDATA)0;
    }

    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_ClientQueueUnadvise
 *
 * queues an unadvise transaction
 */
static WDML_XACT*	WDML_ClientQueueUnadvise(WDML_CONV* pConv, UINT wFmt, HSZ hszItem)
{
    WDML_XACT*	pXAct;
    ATOM	atom;

    TRACE("XTYP_ADVSTOP transaction\n");

    atom = WDML_MakeAtomFromHsz(hszItem);
    if (!atom) return NULL;

    pXAct = WDML_AllocTransaction(pConv->instance, WM_DDE_UNADVISE, wFmt, hszItem);
    if (!pXAct)
    {
	GlobalDeleteAtom(atom);
	return NULL;
    }

    /* end advise loop: post WM_DDE_UNADVISE to server to terminate link
     * on the specified item.
     */
    pXAct->lParam = PackDDElParam(WM_DDE_UNADVISE, wFmt, atom);
    return pXAct;
}

/******************************************************************
 *		WDML_HandleUnadviseReply
 *
 *
 */
static WDML_QUEUE_STATE WDML_HandleUnadviseReply(WDML_CONV* pConv, MSG* msg, WDML_XACT* pXAct)
{
    DDEACK	ddeAck;
    UINT	uiLo, uiHi;
    HSZ		hsz;

    if (msg->message != WM_DDE_ACK || WIN_GetFullHandle(msg->wParam) != pConv->hwndServer)
    {
	return WDML_QS_PASS;
    }

    UnpackDDElParam(WM_DDE_ACK, msg->lParam, &uiLo, &uiHi);
    hsz = WDML_MakeHszFromAtom(pConv->instance, uiHi);

    if (DdeCmpStringHandles(hsz, pXAct->hszItem) != 0)
	return WDML_QS_PASS;

    FreeDDElParam(WM_DDE_ACK, msg->lParam);
    GlobalDeleteAtom(uiHi);

    WDML_ExtractAck(uiLo, &ddeAck);

    TRACE("WM_DDE_ACK received while waiting for a timeout\n");

    if (!ddeAck.fAck)
    {
	TRACE("Returning FALSE on XTYP_ADVSTOP - fAck was FALSE\n");
        pXAct->hDdeData = (HDDEDATA)0;
    }
    else
    {
	/* billx: remove the link */
	WDML_RemoveLink(pConv->instance, (HCONV)pConv, WDML_CLIENT_SIDE,
			pXAct->hszItem, pXAct->wFmt);
        pXAct->hDdeData = (HDDEDATA)1;
    }
    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_ClientQueueRequest
 *
 *
 */
static WDML_XACT*	WDML_ClientQueueRequest(WDML_CONV* pConv, UINT wFmt, HSZ hszItem)
{
    WDML_XACT*	pXAct;
    ATOM	atom;

    TRACE("XTYP_REQUEST transaction\n");

    atom = WDML_MakeAtomFromHsz(hszItem);
    if (!atom) return NULL;

    pXAct = WDML_AllocTransaction(pConv->instance, WM_DDE_REQUEST, wFmt, hszItem);
    if (!pXAct)
    {
	GlobalDeleteAtom(atom);
	return NULL;
    }

    pXAct->lParam = PackDDElParam(WM_DDE_REQUEST, wFmt, atom);

    return pXAct;
}

/******************************************************************
 *		WDML_HandleRequestReply
 *
 *
 */
static WDML_QUEUE_STATE WDML_HandleRequestReply(WDML_CONV* pConv, MSG* msg, WDML_XACT* pXAct)
{
    DDEACK		ddeAck;
    WINE_DDEHEAD	wdh;
    UINT		uiLo, uiHi;
    HSZ			hsz;

    if (WIN_GetFullHandle(msg->wParam) != pConv->hwndServer)
	return WDML_QS_PASS;

    switch (msg->message)
    {
    case WM_DDE_ACK:
        UnpackDDElParam(WM_DDE_ACK, msg->lParam, &uiLo, &uiHi);
        FreeDDElParam(WM_DDE_ACK, msg->lParam);
	GlobalDeleteAtom(uiHi);
	WDML_ExtractAck(uiLo, &ddeAck);
	pXAct->hDdeData = 0;
	if (ddeAck.fAck)
	    ERR("Positive answer should appear in NACK for a request, assuming negative\n");
	TRACE("Negative answer...\n");
	break;

    case WM_DDE_DATA:
        UnpackDDElParam(WM_DDE_DATA, msg->lParam, &uiLo, &uiHi);
	TRACE("Got the result (%08lx)\n", (DWORD)uiLo);

	hsz = WDML_MakeHszFromAtom(pConv->instance, uiHi);

	if (DdeCmpStringHandles(hsz, pXAct->hszItem) != 0)
	    return WDML_QS_PASS;

	pXAct->hDdeData = WDML_Global2DataHandle((HGLOBAL)uiLo, &wdh);
	if (wdh.fRelease)
	{
	    GlobalFree((HGLOBAL)uiLo);
	}
	if (wdh.fAckReq)
	{
	    WDML_PostAck(pConv, WDML_CLIENT_SIDE, 0, FALSE, TRUE, uiHi, msg->lParam, WM_DDE_DATA);
	}
	else
	{
	    GlobalDeleteAtom(uiHi);
            FreeDDElParam(WM_DDE_ACK, msg->lParam);
	}
	break;

    default:
        FreeDDElParam(msg->message, msg->lParam);
	return WDML_QS_PASS;
    }

    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_BuildExecuteCommand
 *
 * Creates a DDE block suitable for sending in WM_DDE_COMMAND
 * It also takes care of string conversion between the two window procedures
 */
static	HGLOBAL	WDML_BuildExecuteCommand(WDML_CONV* pConv, LPCVOID pData, DWORD cbData)
{
    HGLOBAL	hMem;
    BOOL	clientUnicode, serverUnicode;
    DWORD	memSize;

    clientUnicode = IsWindowUnicode(pConv->hwndClient);
    serverUnicode = IsWindowUnicode(pConv->hwndServer);

    if (clientUnicode == serverUnicode)
    {
	memSize = cbData;
    }
    else
    {
	if (clientUnicode)
	{
	    memSize = WideCharToMultiByte( CP_ACP, 0, pData, cbData, NULL, 0, NULL, NULL);
	}
	else
	{
	    memSize = MultiByteToWideChar( CP_ACP, 0, pData, cbData, NULL, 0);
	}
    }

    hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, memSize);

    if (hMem)
    {
	LPBYTE	pDst;

	pDst = GlobalLock(hMem);
	if (pDst)
	{
	    if (clientUnicode == serverUnicode)
	    {
		memcpy(pDst, pData, cbData);
	    }
	    else
	    {
		if (clientUnicode)
		{
		    WideCharToMultiByte( CP_ACP, 0, pData, cbData, pDst, memSize, NULL, NULL);
		}
		else
		{
		    MultiByteToWideChar( CP_ACP, 0, pData, cbData, (LPWSTR)pDst, memSize);
		}
	    }

	    GlobalUnlock(hMem);
	}
	else
	{
	    GlobalFree(hMem);
	    hMem = 0;
	}
    }
    return hMem;
}

/******************************************************************
 *		WDML_ClientQueueExecute
 *
 *
 */
static WDML_XACT*	WDML_ClientQueueExecute(WDML_CONV* pConv, LPCVOID pData, DWORD cbData)
{
    WDML_XACT*	pXAct;

    TRACE("XTYP_EXECUTE transaction\n");

    pXAct = WDML_AllocTransaction(pConv->instance, WM_DDE_EXECUTE, 0, 0);
    if (!pXAct)
	return NULL;

    if (cbData == (DWORD)-1)
    {
	HDDEDATA	hDdeData = (HDDEDATA)pData;

	pData = DdeAccessData(hDdeData, &cbData);
	if (pData)
	{
	    pXAct->hMem = WDML_BuildExecuteCommand(pConv, pData, cbData);
	    DdeUnaccessData(hDdeData);
	}
    }
    else
    {
	pXAct->hMem = WDML_BuildExecuteCommand(pConv, pData, cbData);
    }

    pXAct->lParam = pXAct->hMem;

    return pXAct;
}

/******************************************************************
 *		WDML_HandleExecuteReply
 *
 *
 */
static WDML_QUEUE_STATE WDML_HandleExecuteReply(WDML_CONV* pConv, MSG* msg, WDML_XACT* pXAct)
{
    DDEACK	ddeAck;
    UINT	uiLo, uiHi;

    if (msg->message != WM_DDE_ACK || WIN_GetFullHandle(msg->wParam) != pConv->hwndServer)
    {
	return WDML_QS_PASS;
    }

    UnpackDDElParam(WM_DDE_ACK, msg->lParam, &uiLo, &uiHi);
    FreeDDElParam(WM_DDE_ACK, msg->lParam);

    if (uiHi != pXAct->hMem)
    {
        return WDML_QS_PASS;
    }

    WDML_ExtractAck(uiLo, &ddeAck);
    pXAct->hDdeData = (HDDEDATA)ddeAck.fAck;

    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_ClientQueuePoke
 *
 *
 */
static WDML_XACT*	WDML_ClientQueuePoke(WDML_CONV* pConv, LPCVOID pData, DWORD cbData,
					     UINT wFmt, HSZ hszItem)
{
    WDML_XACT*	pXAct;
    ATOM	atom;

    TRACE("XTYP_POKE transaction\n");

    atom = WDML_MakeAtomFromHsz(hszItem);
    if (!atom) return NULL;

    pXAct = WDML_AllocTransaction(pConv->instance, WM_DDE_POKE, wFmt, hszItem);
    if (!pXAct)
    {
	GlobalDeleteAtom(atom);
	return NULL;
    }

    if (cbData == (DWORD)-1)
    {
	pXAct->hMem = (HDDEDATA)pData;
    }
    else
    {
	DDEPOKE*	ddePoke;

	pXAct->hMem = GlobalAlloc(GHND | GMEM_DDESHARE, sizeof(DDEPOKE) + cbData);
	ddePoke = GlobalLock(pXAct->hMem);
	if (ddePoke)
	{
	    memcpy(ddePoke->Value, pData, cbData);
	    ddePoke->fRelease = FALSE; /* FIXME: app owned ? */
	    ddePoke->cfFormat = wFmt;
	    GlobalUnlock(pXAct->hMem);
	}
    }

    pXAct->lParam = PackDDElParam(WM_DDE_POKE, pXAct->hMem, atom);

    return pXAct;
}

/******************************************************************
 *		WDML_HandlePokeReply
 *
 *
 */
static WDML_QUEUE_STATE WDML_HandlePokeReply(WDML_CONV* pConv, MSG* msg, WDML_XACT* pXAct)
{
    DDEACK	ddeAck;
    UINT	uiLo, uiHi;
    HSZ		hsz;

    if (msg->message != WM_DDE_ACK && WIN_GetFullHandle(msg->wParam) != pConv->hwndServer)
    {
	return WDML_QS_PASS;
    }

    UnpackDDElParam(WM_DDE_ACK, msg->lParam, &uiLo, &uiHi);
    hsz = WDML_MakeHszFromAtom(pConv->instance, uiHi);
    if (DdeCmpStringHandles(hsz, pXAct->hszItem) != 0)
    {
	return WDML_QS_PASS;
    }
    FreeDDElParam(WM_DDE_ACK, msg->lParam);
    GlobalDeleteAtom(uiHi);

    WDML_ExtractAck(uiLo, &ddeAck);
    GlobalFree(pXAct->hMem);

    pXAct->hDdeData = (HDDEDATA)TRUE;
    return TRUE;
}

/******************************************************************
 *		WDML_ClientQueueTerminate
 *
 * Creates and queue an WM_DDE_TERMINATE transaction
 */
static WDML_XACT*	WDML_ClientQueueTerminate(WDML_CONV* pConv)
{
    WDML_XACT*		pXAct;

    pXAct = WDML_AllocTransaction(pConv->instance, WM_DDE_TERMINATE, 0, 0);
    if (!pXAct)
	return NULL;

    pXAct->lParam = 0;
    pConv->wStatus &= ~ST_CONNECTED;

    return pXAct;
}

/******************************************************************
 *		WDML_HandleTerminateReply
 *
 * handles the reply to a terminate request
 */
static WDML_QUEUE_STATE WDML_HandleTerminateReply(WDML_CONV* pConv, MSG* msg, WDML_XACT* pXAct)
{
    if (msg->message != WM_DDE_TERMINATE)
    {
	/* FIXME: should delete data passed here */
	return WDML_QS_SWALLOWED;
    }

    if (WIN_GetFullHandle(msg->wParam) != pConv->hwndServer)
    {
	FIXME("hmmm shouldn't happen\n");
	return WDML_QS_PASS;
    }
    if (!pConv->instance->CBFflags & CBF_SKIP_DISCONNECTS)
    {
	WDML_InvokeCallback(pConv->instance, XTYP_DISCONNECT, 0, (HCONV)pConv,
			    0, 0, 0, 0, (pConv->wStatus & ST_ISSELF) ? 1 : 0);
    }
    WDML_RemoveConv(pConv, WDML_CLIENT_SIDE);
    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_HandleReplyData
 *
 *
 */
static WDML_QUEUE_STATE WDML_HandleIncomingData(WDML_CONV* pConv, MSG* msg, HDDEDATA* hdd)
{
    UINT		uiLo, uiHi;
    HDDEDATA		hDdeDataIn, hDdeDataOut;
    WDML_LINK*		pLink;
    WINE_DDEHEAD	wdh;
    HSZ			hsz;

    TRACE("WM_DDE_DATA message received in the Client Proc!\n");
    /* wParam -- sending window handle	*/
    /* lParam -- hDdeData & item HSZ	*/

    UnpackDDElParam(WM_DDE_DATA, msg->lParam, &uiLo, &uiHi);
    hsz = WDML_MakeHszFromAtom(pConv->instance, uiHi);

    hDdeDataIn = WDML_Global2DataHandle((HGLOBAL)uiLo, &wdh);

    /* billx:
     *  For hot link, data should be passed to its callback with
     * XTYP_ADVDATA and callback should return the proper status.
     */
    pLink = WDML_FindLink(pConv->instance, (HCONV)pConv, WDML_CLIENT_SIDE, hsz,
                          uiLo ? TRUE : FALSE, wdh.cfFormat);
    if (!pLink)
    {
	WDML_DecHSZ(pConv->instance, hsz);
        DdeFreeDataHandle(hDdeDataIn);
	return WDML_QS_PASS;
    }

    if (hDdeDataIn != 0 && wdh.fAckReq)
    {
	WDML_PostAck(pConv, WDML_CLIENT_SIDE, 0, FALSE, TRUE, uiHi, msg->lParam, WM_DDE_DATA);
	if (msg->lParam)
	    msg->lParam = 0;
    }
    else
    {
	GlobalDeleteAtom(uiHi);
    }

    hDdeDataOut = WDML_InvokeCallback(pConv->instance, XTYP_ADVDATA, pLink->uFmt, pLink->hConv,
				      pConv->hszTopic, pLink->hszItem, hDdeDataIn, 0, 0);

    if (hDdeDataOut != (HDDEDATA)DDE_FACK || wdh.fRelease)
    {
        if (uiLo)
        {
            GlobalFree(uiLo);
        }
    }

    DdeFreeDataHandle(hDdeDataIn);

    WDML_DecHSZ(pConv->instance, hsz);
    if (msg->lParam)
	FreeDDElParam(WM_DDE_DATA, msg->lParam);

    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_HandleIncomingTerminate
 *
 *
 */
static WDML_QUEUE_STATE WDML_HandleIncomingTerminate(WDML_CONV* pConv, MSG* msg, HDDEDATA* hdd)
{
    if (pConv->hwndServer != WIN_GetFullHandle(msg->wParam))
	return WDML_QS_PASS;

    pConv->wStatus |= ST_TERMINATED;
    if (!pConv->instance->CBFflags & CBF_SKIP_DISCONNECTS)
    {
	WDML_InvokeCallback(pConv->instance, XTYP_DISCONNECT, 0, (HCONV)pConv,
			    0, 0, 0, 0, (pConv->wStatus & ST_ISSELF) ? 1 : 0);
    }
    if (pConv->wStatus & ST_CONNECTED)
    {
	/* don't care about result code (if server exists or not) */
	PostMessageA(pConv->hwndServer, WM_DDE_TERMINATE, (WPARAM)pConv->hwndClient, 0L);
	pConv->wStatus &= ~ST_CONNECTED;
    }
    /* have to keep connection around to allow reconnection */
    return WDML_QS_HANDLED;
}

/******************************************************************
 *		WDML_HandleReply
 *
 * handles any incoming reply, and try to match to an already sent request
 */
static WDML_QUEUE_STATE	WDML_HandleReply(WDML_CONV* pConv, MSG* msg, HDDEDATA* hdd)
{
    WDML_XACT*		pXAct = pConv->transactions;
    WDML_QUEUE_STATE	qs;

    if (pConv->transactions)
    {
	/* first check message against a pending transaction, if any */
	switch (pXAct->ddeMsg)
	{
	case WM_DDE_ADVISE:
	    qs = WDML_HandleAdviseReply(pConv, msg, pXAct);
	    break;
	case WM_DDE_UNADVISE:
	    qs = WDML_HandleUnadviseReply(pConv, msg, pXAct);
	    break;
	case WM_DDE_EXECUTE:
	    qs = WDML_HandleExecuteReply(pConv, msg, pXAct);
	    break;
	case WM_DDE_REQUEST:
	    qs = WDML_HandleRequestReply(pConv, msg, pXAct);
	    break;
	case WM_DDE_POKE:
	    qs = WDML_HandlePokeReply(pConv, msg, pXAct);
	    break;
	case WM_DDE_TERMINATE:
	    qs = WDML_HandleTerminateReply(pConv, msg, pXAct);
	    break;
	default:
	    qs = WDML_QS_ERROR;
	    FIXME("oooch\n");
	}
    }
    else
    {
	qs = WDML_QS_PASS;
    }

    /* now check the results */
    switch (qs)
    {
    case WDML_QS_ERROR:
    case WDML_QS_SWALLOWED:
	*hdd = 0;
	break;
    case WDML_QS_HANDLED:
	/* ok, we have resolved a pending transaction
	 * notify callback if asynchronous, and remove it in any case
	 */
	WDML_UnQueueTransaction(pConv, pXAct);
	if (pXAct->dwTimeout == TIMEOUT_ASYNC && pXAct->ddeMsg != WM_DDE_TERMINATE)
	{
	    WDML_InvokeCallback(pConv->instance, XTYP_XACT_COMPLETE, pXAct->wFmt,
				(HCONV)pConv, pConv->hszTopic, pXAct->hszItem,
				pXAct->hDdeData, MAKELONG(0, pXAct->xActID), 0 /* FIXME */);
	    qs = WDML_QS_PASS;
	}
	else
	{
	    *hdd = pXAct->hDdeData;
	}
	WDML_FreeTransaction(pConv->instance, pXAct, TRUE);
	break;
    case WDML_QS_PASS:
	/* no pending transaction found, try a warm/hot link or a termination request */
	switch (msg->message)
	{
	case WM_DDE_DATA:
	    qs = WDML_HandleIncomingData(pConv, msg, hdd);
	    break;
	case WM_DDE_TERMINATE:
	    qs = WDML_HandleIncomingTerminate(pConv, msg, hdd);
	    break;
	}
	break;
    case WDML_QS_BLOCK:
	FIXME("shouldn't be used on client side\n");
	break;
    }

    return qs;
}

/******************************************************************
 *		WDML_SyncWaitTransactionReply
 *
 * waits until an answer for a sent request is received
 * time out is also handled. only used for synchronous transactions
 */
static HDDEDATA WDML_SyncWaitTransactionReply(HCONV hConv, DWORD dwTimeout, WDML_XACT* pXAct)
{
    DWORD	dwTime;
    DWORD	err;
    WDML_CONV*	pConv;

    TRACE("Starting wait for a timeout of %ld ms\n", dwTimeout);

    /* FIXME: time 32 bit wrap around */
    dwTimeout += GetCurrentTime();

    while ((dwTime = GetCurrentTime()) < dwTimeout)
    {
	/* we cannot be in the crit sect all the time because when client and server run in a
	 * single process they need to share the access to the internal data
	 */
	if (MsgWaitForMultipleObjects(0, NULL, FALSE,
				      dwTimeout - dwTime, QS_POSTMESSAGE) == WAIT_OBJECT_0)
	{
	    BOOL	ret = FALSE;
	    MSG		msg;
	    WDML_CONV*	pConv;
	    HDDEDATA	hdd;

	    EnterCriticalSection(&WDML_CritSect);

	    pConv = WDML_GetConv(hConv, FALSE);
	    if (pConv == NULL)
	    {
		LeaveCriticalSection(&WDML_CritSect);
		/* conversation no longer available... return failure */
		break;
	    }
	    while (PeekMessageA(&msg, pConv->hwndClient, WM_DDE_FIRST, WM_DDE_LAST, PM_REMOVE))
	    {
		/* check that either pXAct has been processed or no more xActions are pending */
		ret = (pConv->transactions == pXAct);
		ret = WDML_HandleReply(pConv, &msg, &hdd) == WDML_QS_HANDLED &&
		    (pConv->transactions == NULL || ret);
		if (ret) break;
	    }
	    LeaveCriticalSection(&WDML_CritSect);
	    if (ret)
	    {
		return hdd;
	    }
	}
    }

    TRACE("Timeout !!\n");

    EnterCriticalSection(&WDML_CritSect);

    pConv = WDML_GetConv(hConv, FALSE);
    if (pConv != NULL)
    {
	if (pConv->transactions)
	{
	    switch (pConv->transactions->ddeMsg)
	    {
	    case WM_DDE_ADVISE:		err = DMLERR_ADVACKTIMEOUT;	break;
	    case WM_DDE_REQUEST:	err = DMLERR_DATAACKTIMEOUT; 	break;
	    case WM_DDE_EXECUTE:	err = DMLERR_EXECACKTIMEOUT;	break;
	    case WM_DDE_POKE:		err = DMLERR_POKEACKTIMEOUT;	break;
	    case WM_DDE_UNADVISE:	err = DMLERR_UNADVACKTIMEOUT;	break;
	    default:			err = DMLERR_INVALIDPARAMETER;	break;
	    }

	    pConv->instance->lastError = err;
	}
    }
    LeaveCriticalSection(&WDML_CritSect);

    return 0;
}

/*****************************************************************
 *            DdeClientTransaction  (USER32.@)
 */
HDDEDATA WINAPI DdeClientTransaction(LPBYTE pData, DWORD cbData, HCONV hConv, HSZ hszItem, UINT wFmt,
				     UINT wType, DWORD dwTimeout, LPDWORD pdwResult)
{
    WDML_CONV*		pConv;
    WDML_XACT*		pXAct;
    HDDEDATA		hDdeData = 0;

    TRACE("(%p,%ld,0x%lx,0x%x,%d,%d,%ld,%p)\n",
	  pData, cbData, (DWORD)hConv, hszItem, wFmt, wType, dwTimeout, pdwResult);

    if (hConv == 0)
    {
	ERR("Invalid conversation handle\n");
	return 0;
    }

    EnterCriticalSection(&WDML_CritSect);

    pConv = WDML_GetConv(hConv, TRUE);
    if (pConv == NULL)
    {
	/* cannot set error... cannot get back to DDE instance */
	goto theError;
    }

    switch (wType)
    {
    case XTYP_EXECUTE:
	if (hszItem != 0 || wFmt != 0)
	{
	    pConv->instance->lastError = DMLERR_INVALIDPARAMETER;
	    goto theError;
	}
	pXAct = WDML_ClientQueueExecute(pConv, pData, cbData);
	break;
    case XTYP_POKE:
	pXAct = WDML_ClientQueuePoke(pConv, pData, cbData, wFmt, hszItem);
	break;
    case XTYP_ADVSTART|XTYPF_NODATA:
    case XTYP_ADVSTART|XTYPF_NODATA|XTYPF_ACKREQ:
    case XTYP_ADVSTART:
    case XTYP_ADVSTART|XTYPF_ACKREQ:
	if (pData)
	{
	    pConv->instance->lastError = DMLERR_INVALIDPARAMETER;
	    goto theError;
	}
	pXAct = WDML_ClientQueueAdvise(pConv, wType, wFmt, hszItem);
	break;
    case XTYP_ADVSTOP:
	if (pData)
	{
	    pConv->instance->lastError = DMLERR_INVALIDPARAMETER;
	    goto theError;
	}
	pXAct = WDML_ClientQueueUnadvise(pConv, wFmt, hszItem);
	break;
    case XTYP_REQUEST:
	if (pData)
	{
	    pConv->instance->lastError = DMLERR_INVALIDPARAMETER;
	    goto theError;
	}
	pXAct = WDML_ClientQueueRequest(pConv, wFmt, hszItem);
	break;
    default:
	FIXME("Unknown transation\n");
	/* unknown transaction type */
	pConv->instance->lastError = DMLERR_INVALIDPARAMETER;
	goto theError;
    }

    if (pXAct == NULL)
    {
	pConv->instance->lastError = DMLERR_MEMORY_ERROR;
	goto theError;
    }

    WDML_QueueTransaction(pConv, pXAct);

    if (!PostMessageA(pConv->hwndServer, pXAct->ddeMsg, (WPARAM)pConv->hwndClient, pXAct->lParam))
    {
	TRACE("Failed posting message %d to 0x%04x (error=0x%lx)\n",
	      pXAct->ddeMsg, pConv->hwndServer, GetLastError());
	pConv->wStatus &= ~ST_CONNECTED;
	WDML_UnQueueTransaction(pConv, pXAct);
	WDML_FreeTransaction(pConv->instance, pXAct, TRUE);
	goto theError;
    }
    pXAct->dwTimeout = dwTimeout;
    /* FIXME: should set the app bits on *pdwResult */

    if (dwTimeout == TIMEOUT_ASYNC)
    {
	if (pdwResult)
	{
	    *pdwResult = MAKELONG(0, pXAct->xActID);
	}
	hDdeData = (HDDEDATA)1;
    }
    else
    {
	DWORD	count, i;

	if (pdwResult)
	{
	    *pdwResult = 0L;
	}
	count = WDML_CritSect.RecursionCount;
	for (i = 0; i < count; i++)
	    LeaveCriticalSection(&WDML_CritSect);
	hDdeData = WDML_SyncWaitTransactionReply((HCONV)pConv, dwTimeout, pXAct);
	for (i = 0; i < count; i++)
	    EnterCriticalSection(&WDML_CritSect);
    }
    LeaveCriticalSection(&WDML_CritSect);

    return hDdeData;
 theError:
    LeaveCriticalSection(&WDML_CritSect);
    return 0;
}

/*****************************************************************
 *            DdeAbandonTransaction (USER32.@)
 */
BOOL WINAPI DdeAbandonTransaction(DWORD idInst, HCONV hConv, DWORD idTransaction)
{
    WDML_INSTANCE*	pInstance;
    WDML_CONV*		pConv;
    WDML_XACT*          pXAct;

    TRACE("(%08lx,%08lx,%08ld);\n", idInst, (DWORD)hConv, idTransaction);

    EnterCriticalSection(&WDML_CritSect);
    if ((pInstance = WDML_GetInstance(idInst)))
    {
        if (hConv)
        {
            if ((pConv = WDML_GetConv(hConv, TRUE)) && pConv->instance == pInstance)
            {
                for (pXAct = pConv->transactions; pXAct; pXAct = pXAct->next)
                {
                    if (pXAct->dwTimeout == TIMEOUT_ASYNC &&
                        (idTransaction == 0 || pXAct->xActID == idTransaction))
                    {
                        WDML_UnQueueTransaction(pConv, pXAct);
                        WDML_FreeTransaction(pInstance, pXAct, TRUE);
                    }
                }
            }
        }
        else
        {
            for (pConv = pInstance->convs[WDML_CLIENT_SIDE]; pConv; pConv = pConv->next)
            {
                if (!(pConv->wStatus & ST_CONNECTED)) continue;
                for (pXAct = pConv->transactions; pXAct; pXAct = pXAct->next)
                {
                    if (pXAct->dwTimeout == TIMEOUT_ASYNC)
                    {
                        WDML_UnQueueTransaction(pConv, pXAct);
                        WDML_FreeTransaction(pInstance, pXAct, TRUE);
                    }
                }
            }
        }
    }
    LeaveCriticalSection(&WDML_CritSect);

    return TRUE;
}

/******************************************************************
 *		WDML_ClientProc
 *
 * Window Proc created on client side for each conversation
 */
static LRESULT CALLBACK WDML_ClientProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    UINT	uiLo, uiHi;
    WDML_CONV*	pConv = NULL;
    HSZ		hszSrv, hszTpc;

    if (iMsg == WM_DDE_ACK &&
	/* in the initial WM_INITIATE sendmessage */
	((pConv = WDML_GetConvFromWnd(hwnd)) == NULL || pConv->wStatus == XST_INIT1))
    {
	/* In response to WM_DDE_INITIATE, save server window  */
	char		buf[256];
	WDML_INSTANCE*	pInstance;

        /* note: sent messages do not need packing */
	uiLo = LOWORD(lParam);
        uiHi = HIWORD(lParam);

	/* FIXME: convlist should be handled here */
	if (pConv)
	{
	    /* we already have started the conv with a server, drop other replies */
	    GlobalDeleteAtom(uiLo);
	    GlobalDeleteAtom(uiHi);
            PostMessageA((HWND)wParam, WM_DDE_TERMINATE, (WPARAM)hwnd, 0);
	    return 0;
	}

	pInstance = WDML_GetInstanceFromWnd(hwnd);

	hszSrv = WDML_MakeHszFromAtom(pInstance, uiLo);
	hszTpc = WDML_MakeHszFromAtom(pInstance, uiHi);

	pConv = WDML_AddConv(pInstance, WDML_CLIENT_SIDE, hszSrv, hszTpc, hwnd, (HWND)wParam);

	SetWindowLongA(hwnd, GWL_WDML_CONVERSATION, (DWORD)pConv);
	pConv->wStatus |= ST_CONNECTED;
	pConv->wConvst = XST_INIT1;

	/* check if server is handled by DDEML */
	if ((GetClassNameA((HWND)wParam, buf, sizeof(buf)) &&
	     strcmp(buf, WDML_szServerConvClassA) == 0) ||
	    (GetClassNameW((HWND)wParam, (LPWSTR)buf, sizeof(buf)/sizeof(WCHAR)) &&
	     lstrcmpW((LPWSTR)buf, WDML_szServerConvClassW) == 0))
	{
	    pConv->wStatus |= ST_ISLOCAL;
	}

	WDML_BroadcastDDEWindows(WDML_szEventClass, WM_WDML_CONNECT_CONFIRM, (WPARAM)hwnd, wParam);

	GlobalDeleteAtom(uiLo);
	GlobalDeleteAtom(uiHi);

	/* accept conversation */
	return 1;
    }

    if (iMsg >= WM_DDE_FIRST && iMsg <= WM_DDE_LAST)
    {
	EnterCriticalSection(&WDML_CritSect);

	pConv = WDML_GetConvFromWnd(hwnd);

	if (pConv)
	{
	    MSG		msg;
	    HDDEDATA	hdd;

	    msg.hwnd = hwnd;
	    msg.message = iMsg;
	    msg.wParam = wParam;
	    msg.lParam = lParam;

	    WDML_HandleReply(pConv, &msg, &hdd);
	}

	LeaveCriticalSection(&WDML_CritSect);
	return 0;
    }

    return (IsWindowUnicode(hwnd)) ?
	DefWindowProcA(hwnd, iMsg, wParam, lParam) : DefWindowProcW(hwnd, iMsg, wParam, lParam);
}

/*****************************************************************
 *            DdeDisconnect   (USER32.@)
 */
BOOL WINAPI DdeDisconnect(HCONV hConv)
{
    WDML_CONV*	pConv = NULL;
    WDML_XACT*	pXAct;
    DWORD	count, i;
    BOOL	ret = FALSE;

    TRACE("(%ld)\n", (DWORD)hConv);

    if (hConv == 0)
    {
	ERR("DdeDisconnect(): hConv = 0\n");
	return FALSE;
    }

    EnterCriticalSection(&WDML_CritSect);
    pConv = WDML_GetConv(hConv, TRUE);
    if (pConv != NULL)
    {
        if (pConv->wStatus & ST_CLIENT)
        {
            /* FIXME: should abandon all pending transactions */
            pXAct = WDML_ClientQueueTerminate(pConv);
            if (pXAct != NULL)
            {
                count = WDML_CritSect.RecursionCount;
                for (i = 0; i < count; i++)
                    LeaveCriticalSection(&WDML_CritSect);
                if (PostMessageA(pConv->hwndServer, pXAct->ddeMsg,
                                 (WPARAM)pConv->hwndClient, pXAct->lParam))
                    WDML_SyncWaitTransactionReply(hConv, 10000, pXAct);
                for (i = 0; i < count; i++)
                    EnterCriticalSection(&WDML_CritSect);
                ret = TRUE;
                WDML_FreeTransaction(pConv->instance, pXAct, TRUE);
                /* still have to destroy data assosiated with conversation */
                WDML_RemoveConv(pConv, WDML_CLIENT_SIDE);
            }
            else
            {
                FIXME("Not implemented yet for a server side conversation\n");
            }
        }
    }
    LeaveCriticalSection(&WDML_CritSect);

    return ret;
}

/*****************************************************************
 *            DdeImpersonateClient (USER32.@)
 */
BOOL WINAPI DdeImpersonateClient(HCONV hConv)
{
    WDML_CONV*	pConv;
    BOOL	ret = FALSE;

    EnterCriticalSection(&WDML_CritSect);
    pConv = WDML_GetConv(hConv, TRUE);
    if (pConv)
    {
	ret = ImpersonateDdeClientWindow(pConv->hwndClient, pConv->hwndServer);
    }
    LeaveCriticalSection(&WDML_CritSect);
    return ret;
}
