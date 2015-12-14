/*
 * Implements CLSID_VideoRenderer.
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * FIXME - use clock
 */

#include "config.h"

#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "mmsystem.h"
#include "strmif.h"
#include "control.h"
#include "vfwmsgs.h"
#include "uuids.h"
#include "amvideo.h"
#include "evcode.h"

#include <unistd.h> /* for usleep() */

#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "vidren.h"
#include "seekpass.h"

/* #define QUALITY_CONTROL */
/* #define QUALITY_CHANGE */
#define PREFER_EARLY (QUARTZ_TIMEUNITS*2/10) /* ask upstream to deliver frames at least 0.2s before presentation time */
#define TOO_EARLY    (QUARTZ_TIMEUNITS)      /* ask upstream to not deliver more than 1s before presentation time */
#define MAX_SKIP 20 /* max number of frames to skip */

/* Set this define if we are to render to the topmost HWND instead of our own Window */
#define RENDER_TO_TOPMOST_HWND
/* Set this define to allow no-copy rendering when frames don't arrive on time */
#define ALLOW_NOCOPY
/* Set this to render from the receiving thread rather than the window thread */
#define DIRECT_UPDATE
/* Set this to block with usleep() instead of IReferenceClock */
#define USE_USLEEP

static const WCHAR QUARTZ_VideoRenderer_Name[] =
{ 'V','i','d','e','o',' ','R','e','n','d','e','r','e','r',0 };
static const WCHAR QUARTZ_VideoRendererPin_Name[] =
{ 'I','n',0 };

#define VIDRENMSG_UPDATE	(WM_APP+0)
#define VIDRENMSG_NEWFRAME	(WM_APP+1)
#define VIDRENMSG_ENDTHREAD	(WM_APP+2)

static const CHAR VIDREN_szWndClass[] = "Wine_VideoRenderer";
static const CHAR VIDREN_szWndName[] = "Wine Video Renderer";

static HRESULT DDRender_Init(CVideoRendererImpl *This)
{
	typedef HRESULT (WINAPI *DirectDrawCreateProc)(LPGUID,LPDIRECTDRAW *,LPUNKNOWN);
	static DirectDrawCreateProc pDirectDrawCreate = NULL;
	LRESULT	res;

	if (This->m_lpddraw)
		return(S_OK);

#ifndef __APPLE__
	/* don't init ddraw before we've tested it a bit */
	return S_OK;
#endif

	if (!pDirectDrawCreate)
	{
		HMODULE hmod;

		hmod = LoadLibraryA("ddraw.dll");
		if (hmod)
		{
			pDirectDrawCreate = (DirectDrawCreateProc)GetProcAddress(hmod, "DirectDrawCreate");
		}

		if (!pDirectDrawCreate) 
		{
			ERR("Can't load DirectDraw\n");
			return E_FAIL;
		}
	}

	res = pDirectDrawCreate(NULL, &This->m_lpddraw, NULL);
	if (res!=S_OK || !This->m_lpddraw) 
	{
		ERR("Can't initialize DirectDraw (res = %lx)\n",res);
		return res;
	}

	TRACE("ok\n");
	return S_OK;
}

static HRESULT DDRender_Exit(CVideoRendererImpl* This)
{
	if (This->m_lpddraw)
	{
		IDirectDraw_Release(This->m_lpddraw);
		This->m_lpddraw 	= NULL;
		This->m_UseDirectDraw 	= 0;
	}

	TRACE("ok\n");
	return S_OK;
}

static HRESULT DDRender_DestroySurfaces(CVideoRendererImpl *This)
{
	HRESULT hr;

	if (This->m_pDDSSource)
		IDirectDrawSurface_Release(This->m_pDDSSource);
	This->m_pDDSSource	= NULL;

	if (This->m_pDDSBack)
		IDirectDrawSurface_Release(This->m_pDDSBack);
	This->m_pDDSBack	= NULL;

	if (This->m_pDDSFront)
		IDirectDrawSurface_Release(This->m_pDDSFront);
	This->m_pDDSFront	= NULL;

	if (This->m_lpddraw && (hr = IDirectDraw_SetCooperativeLevel(This->m_lpddraw, 0, DDSCL_NORMAL)))
	{
		ERR("Could not set cooperative level to normal (%x)\n",hr);
		return hr;
	}

	TRACE("ok\n");
	return S_OK; 
}

static HRESULT DDRender_CreateSurfaces(CVideoRendererImpl *This, int width, int height, int depth)
{
	HRESULT		hr;
	DDSURFACEDESC 	ddsd;
	DDSCAPS		caps;

	if (This->m_lpddraw == NULL)
		return(DDERR_UNSUPPORTED);

	if ((hr = IDirectDraw_SetCooperativeLevel(This->m_lpddraw, This->m_hwnd, DDSCL_FULLSCREEN|DDSCL_EXCLUSIVE)))
	{
		ERR("Could not set cooperative level to exclusive (%x)\n",hr);
		return hr;
	}

	TRACE("Creating surface %dx%dx%d\n", width, height, depth);

	/* Clear all members of the structure to 0 */
	memset(&ddsd, 0, sizeof(ddsd));
	/* The first parameter of the structure must contain the size of the structure */
	ddsd.dwSize = sizeof(ddsd);

	/* -- Create the primary surface */

	/* The dwFlags paramater tell DirectDraw which DDSURFACEDESC */
	/*  fields will contain valid values */
	/* When quartz releases these surfaces, it will attempt to free the primary
	 * surface which can cause problems on macdrv, since it doesn't have proper
	 * DIB sections. We'll need to check for the primary surface during the release
	 * call. */

	ddsd.ddsCaps.dwCaps 			= DDSCAPS_PRIMARYSURFACE;
	ddsd.dwFlags 				= DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.dwBackBufferCount			= 1;

	hr = IDirectDraw_CreateSurface(This->m_lpddraw, &ddsd, &This->m_pDDSFront, NULL);
	if (hr != S_OK)
		return hr;

	TRACE("Created primary surface: %p\n", This->m_pDDSFront);

	caps.dwCaps = DDSCAPS_BACKBUFFER;
	hr = IDirectDrawSurface_GetAttachedSurface(This->m_pDDSFront, &caps, &This->m_pDDSBack);
	if (hr != S_OK)
		return hr;

	TRACE("Backbuffer: %p\n", This->m_pDDSBack);

	/* -- Create the source surface */
	ddsd.ddsCaps.dwCaps			= DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwFlags				= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	ddsd.dwWidth				= width;
	ddsd.dwHeight				= height;

	ddsd.ddpfPixelFormat.dwSize		= sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags		= DDPF_RGB;
	ddsd.ddpfPixelFormat.u1.dwRGBBitCount	= (DWORD)depth*8;
	ddsd.ddpfPixelFormat.u2.dwRBitMask	= 0x00FF0000;
	ddsd.ddpfPixelFormat.u3.dwGBitMask	= 0x0000FF00;
	ddsd.ddpfPixelFormat.u4.dwBBitMask	= 0x000000FF;

	hr = IDirectDraw_CreateSurface(This->m_lpddraw, &ddsd, &This->m_pDDSSource, NULL);
	if (hr != S_OK)
		return hr;
		
	This->m_Width = width;
	This->m_Height = height;

	TRACE("ok\n");

	return S_OK; 
}

static void DDRender_Render(CVideoRendererImpl *This, int left, int top, int right, int bottom, int movW, int movH)
{
	HRESULT		hr;
	RECT		rcSrc;	/* source blit rectangle */
	RECT		rcDst;	/* destination blit rectangle */

	SetRect(&rcSrc, 0, 0, movW, movH);		/* movie size */
	SetRect(&rcDst, left, top, right, bottom);	/* screen size */

	hr = IDirectDrawSurface_Blt(This->m_pDDSBack, &rcDst, This->m_pDDSSource, &rcSrc, DDBLT_WAIT, NULL);
	hr = IDirectDrawSurface_Flip(This->m_pDDSFront, NULL, DDFLIP_WAIT);

	TRACE("ok\n");
}

static void VIDREN_DrawFrame( CVideoRendererImpl* This, HWND hwnd, HDC hdc, BYTE* pData )
{
	const VIDEOINFOHEADER* pinfo;
	const AM_MEDIA_TYPE* pmt;
	RECT rect;
	int width, height;

	TRACE("(%p,%p,%p,%p)\n",This,hwnd,hdc,pData);
	pmt = This->pPin->pin.pmtConn;
	if ( pmt == NULL )
		return;
	if ( !pData )
	{
		if ( !This->m_bSampleIsValid )
			return;
		if ( !This->m_bSampleNeedView )
			return;
	}
	This->m_bSampleNeedView = FALSE;
	pinfo = (const VIDEOINFOHEADER*)pmt->pbFormat;
#ifdef RENDER_TO_TOPMOST_HWND
	hwnd = GetForegroundWindow();
	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);
	GetClientRect(hwnd, &rect);
	if (rect.right < width) width = rect.right;
	if (rect.bottom < height) height = rect.bottom;
#else
        GetClientRect(hwnd, &rect);
        width = rect.right;
        height = rect.bottom;
#endif
	if ( pData )
		StretchDIBits(
			hdc,
			(width - abs(pinfo->bmiHeader.biWidth)) / 2,
			(height - abs(pinfo->bmiHeader.biHeight)) / 2,
			abs(pinfo->bmiHeader.biWidth), abs(pinfo->bmiHeader.biHeight),
			0, 0,
			abs(pinfo->bmiHeader.biWidth), abs(pinfo->bmiHeader.biHeight),
			pData, (BITMAPINFO*)(&pinfo->bmiHeader),
			DIB_RGB_COLORS, SRCCOPY );
	else
	{
		if (This->m_UseDirectDraw)
		{
			RECT		rect;
#if 1 /* if fullscreen */
			DDSURFACEDESC	Desc;
			Desc.dwSize = sizeof(Desc);
			IDirectDrawSurface_GetSurfaceDesc(This->m_pDDSFront, &Desc);
			SetRect(&rect, 0, 0, Desc.dwWidth, Desc.dwHeight);
#else /* if windowed */
			GetClientRect(hwnd, &rect);
			MapWindowPoints(hwnd, 0, (LPPOINT)&rect, 2);
#endif

			DDRender_Render(	This,
						rect.left,	rect.top,
						rect.right, 	rect.bottom,
						abs(pinfo->bmiHeader.biWidth), abs(pinfo->bmiHeader.biHeight));
		}
		else
		{
			StretchBlt(
				hdc,
				(width - abs(pinfo->bmiHeader.biWidth)) / 2,
				(height - abs(pinfo->bmiHeader.biHeight)) / 2,
				abs(pinfo->bmiHeader.biWidth), abs(pinfo->bmiHeader.biHeight),
				This->m_hMemDC,
				0, 0,
				abs(pinfo->bmiHeader.biWidth), abs(pinfo->bmiHeader.biHeight),
				SRCCOPY );
		}
	}
}

static void VIDREN_OnPaint( CVideoRendererImpl* This, HWND hwnd )
{
	PAINTSTRUCT ps;

	TRACE("(%p,%p)\n",This,hwnd);

	if ( !BeginPaint( hwnd, &ps ) )
		return;

#ifndef RENDER_TO_TOPMOST_HWND
	VIDREN_DrawFrame( This, hwnd, ps.hdc, NULL );
#endif

	EndPaint( hwnd, &ps );
}

static void VIDREN_OnQueryNewPalette( CVideoRendererImpl* This, HWND hwnd )
{
	FIXME("(%p,%p)\n",This,hwnd);
}

static void VIDREN_DoUpdate( CVideoRendererImpl* This, HWND hwnd, BYTE* pData )
{
	HDC	hdc;

	TRACE("(%p,%p,%p)\n",This,hwnd,pData);

#ifdef RENDER_TO_TOPMOST_HWND
	hwnd = GetForegroundWindow();
#endif

	hdc = GetDC(hwnd);
	VIDREN_DrawFrame( This, hwnd, hdc, pData );
	ReleaseDC(hwnd, hdc);
}

static void VIDREN_OnUpdate( CVideoRendererImpl* This, HWND hwnd )
{
	MSG	msg;

	VIDREN_DoUpdate( This, hwnd, NULL );

	/* FIXME */
	while ( PeekMessageA(&msg,hwnd,
		VIDRENMSG_UPDATE,VIDRENMSG_UPDATE,
		PM_REMOVE) != FALSE )
	{
		/* discard this message. */
	}
}

static HRESULT VIDREN_CopySample( CVideoRendererImpl* This, HWND hwnd, IMediaSample* pSample )
{
	BYTE*	pData = NULL;
	LONG	lLength;
	HRESULT hr;

	/* duplicate this sample. */
	hr = IMediaSample_GetPointer(pSample,&pData);
	if ( FAILED(hr) )
		return hr;
	lLength = (LONG)IMediaSample_GetActualDataLength(pSample);
	TRACE("length=%d (alloc=%u)\n", lLength, This->m_cbSampleData);
	if ( lLength <= 0 || (lLength < (LONG)This->m_cbSampleData) )
	{
		ERR( "invalid length: %d\n", lLength );
		return NOERROR;
	}
	if (lLength > (LONG)This->m_cbSampleData)
		lLength = This->m_cbSampleData;

	if (This->m_UseDirectDraw)
	{
		DDSURFACEDESC	Desc;
		/* memcpy(This->m_pSampleData,pData,lLength); */
		Desc.dwSize = sizeof(Desc);
		if (IDirectDrawSurface_Lock(This->m_pDDSSource, NULL, &Desc, DDLOCK_WAIT, 0) == S_OK)
		{
			int y;
			const VIDEOINFOHEADER* pinfo;
			const AM_MEDIA_TYPE* pmt;
			LPBYTE	Src = (LPBYTE)pData;
			LPBYTE	Dst = (LPBYTE)Desc.lpSurface;

			pmt = This->pPin->pin.pmtConn;
			pinfo = (const VIDEOINFOHEADER*)pmt->pbFormat;

			/* Invert the sample rows when copying if the video bitmap has a positive height */
			if(pinfo->bmiHeader.biHeight >= 0) {
				Dst += Desc.u1.lPitch*Desc.dwHeight;

			  	for (y=0; y<Desc.dwHeight; y++) {
					Dst -= Desc.u1.lPitch;
					memcpy(Dst, Src, Desc.dwWidth * sizeof(DWORD));
					Src += Desc.dwWidth * sizeof(DWORD);
				}
			}
			/* Otherwise copy the rows in direct order */
			else {
				for (y=0; y<Desc.dwHeight; y++)
				{
					memcpy(Dst, Src, Desc.dwWidth * sizeof(DWORD));
					Dst += Desc.u1.lPitch;
					Src += Desc.dwWidth * sizeof(DWORD);
				}
			}
			IDirectDrawSurface_Unlock(This->m_pDDSSource, NULL);
		}
		else
		{
			TRACE("Surface lock failed\n");
		}
	}
	else
	{
		memcpy(This->m_pSampleData,pData,lLength);
	}

	This->m_bSampleIsValid = TRUE;
	This->m_bSampleNeedView = TRUE;
	if (hwnd) PostMessageA( hwnd, VIDRENMSG_UPDATE, 0, 0 );
	return S_OK;
}

static void VIDREN_OnNewFrame( CVideoRendererImpl* This, HWND hwnd, IMediaSample* pSample )
{
	REFERENCE_TIME rtStart, rtStop, rtNow;

	if (This->m_fInFlush) {
		IMediaSample_Release(pSample);
		goto done;
	}

	if (SUCCEEDED(IMediaSample_GetTime(pSample, &rtStart, &rtStop)) && This->basefilter.pClock) {
		IReferenceClock_GetTime(This->basefilter.pClock, &rtNow);
		rtNow -= This->basefilter.rtStart;
		TRACE("time is now %lld, presentation time is %lld\n", rtNow, rtStart);
		if (rtStart > rtNow) {
			/* draw previous frame */
			VIDREN_OnUpdate( This, hwnd );
			/* get new time */
			IReferenceClock_GetTime(This->basefilter.pClock, &rtNow);
			rtNow -= This->basefilter.rtStart;
		}
		/* copy new frame */
		VIDREN_CopySample( This, 0, pSample );
		This->m_rtSample = rtStart;
		This->m_bSampleNeedWait = TRUE;
		IMediaSample_Release(pSample);
		if (rtStart > rtNow) {
			DWORD_PTR dwCookie;
			/* wait for presentation time */
			IReferenceClock_AdviseTime(This->basefilter.pClock, This->basefilter.rtStart, rtStart,
						   (HEVENT)This->m_hEventWait, &dwCookie);
			TRACE("frame %p is waiting for %lld\n", pSample, rtStart);
			WaitForSingleObject(This->m_hEventWait, INFINITE);
			ResetEvent(This->m_hEventWait);

			TRACE("frame %p wait complete\n", pSample);
		}
		/* draw frame after we receive next frame or end of stream, in case it must be skipped */
	}
	else {
		TRACE("frame %p is for immediate presentation\n", pSample);
		VIDREN_CopySample( This, hwnd, pSample );
		This->m_bSampleNeedWait = FALSE;
		IMediaSample_Release(pSample);
	}

done:
	InterlockedDecrement(&This->m_numFrames);
}


static LRESULT CALLBACK
VIDREN_WndProc(
	HWND hwnd, UINT message,
	WPARAM wParam, LPARAM lParam )
{
	CVideoRendererImpl*	This = (CVideoRendererImpl*)
		GetWindowLongPtrA( hwnd, 0L );

	TRACE("(%p) - %u/%u/%ld\n",This,message,wParam,lParam);

	if ( message == WM_NCCREATE )
	{
		This = (CVideoRendererImpl*)(((CREATESTRUCTA*)lParam)->lpCreateParams);
		SetWindowLongPtrA( hwnd, 0L, (LONG_PTR)This );
		This->m_hwnd = hwnd;
	}

	if ( message == WM_NCDESTROY )
	{
		PostQuitMessage(0);
		This->m_hwnd = (HWND)NULL;
		SetWindowLongPtrA( hwnd, 0L, (LONG_PTR)NULL );
		This = NULL;
	}

	if ( This != NULL )
	{
		switch ( message )
		{
		case WM_PAINT:
			TRACE("WM_PAINT begin\n");
			EnterCriticalSection( &This->m_csReceive );
			VIDREN_OnPaint( This, hwnd );
			LeaveCriticalSection( &This->m_csReceive );
			TRACE("WM_PAINT end\n");
			return 0;
		case WM_CLOSE:
			ShowWindow( hwnd, SW_HIDE );
			return 0;
		case WM_PALETTECHANGED:
			if ( hwnd == (HWND)wParam )
				break;
			/* fall through */
		case WM_QUERYNEWPALETTE:
			VIDREN_OnQueryNewPalette( This, hwnd );
			break;
		case VIDRENMSG_UPDATE:
			if ( !This->m_numFrames )
				VIDREN_OnUpdate( This, hwnd );
			return 0;
		case VIDRENMSG_NEWFRAME:
			VIDREN_OnNewFrame( This, hwnd, (IMediaSample*)lParam );
			return 0;
		case VIDRENMSG_ENDTHREAD:
			DestroyWindow(hwnd);
			return 0;
		default:
			break;
		}
	}

	return DefWindowProcA( hwnd, message, wParam, lParam );
}

static BOOL VIDREN_Register( HINSTANCE hInst )
{
	WNDCLASSA	wc;
	ATOM	atom;

	wc.style = 0;
	wc.lpfnWndProc = VIDREN_WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = sizeof(LONG);
	wc.hInstance = hInst;
	wc.hIcon = LoadIconA((HINSTANCE)NULL,IDI_WINLOGOA);
	wc.hCursor = LoadCursorA((HINSTANCE)NULL,IDC_ARROWA);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = VIDREN_szWndClass;

	atom = RegisterClassA( &wc );
	if ( atom != (ATOM)0 )
		return TRUE;

	/* FIXME */
	return FALSE;
}

static HWND VIDREN_Create( HWND hwndOwner, CVideoRendererImpl* This )
{
	HINSTANCE hInst = (HINSTANCE)GetModuleHandleA(NULL);
	const VIDEOINFOHEADER* pinfo;
	DWORD	dwExStyle = 0;
	DWORD	dwStyle = WS_POPUP|WS_CAPTION|WS_CLIPCHILDREN;
	RECT	rcWnd;
	HWND	hwnd;

	if ( !VIDREN_Register( hInst ) )
		return (HWND)NULL;

	pinfo = (const VIDEOINFOHEADER*)This->pPin->pin.pmtConn->pbFormat;

	TRACE("width %d, height %d\n", pinfo->bmiHeader.biWidth, pinfo->bmiHeader.biHeight);

	rcWnd.left = 0;
	rcWnd.top = 0;
	rcWnd.right = pinfo->bmiHeader.biWidth;
	rcWnd.bottom = abs(pinfo->bmiHeader.biHeight);
	AdjustWindowRectEx( &rcWnd, dwStyle, FALSE, dwExStyle );

	TRACE("window width %d,height %d\n",
		rcWnd.right-rcWnd.left,rcWnd.bottom-rcWnd.top);

	hwnd = CreateWindowExA(
		dwExStyle,
		VIDREN_szWndClass, VIDREN_szWndName,
		dwStyle,
		100,100, /* FIXME */
		rcWnd.right-rcWnd.left, rcWnd.bottom-rcWnd.top,
		hwndOwner, (HMENU)NULL,
		hInst, (LPVOID)This );

#ifndef RENDER_TO_TOPMOST_HWND
	if ( hwnd != (HWND)NULL )
		ShowWindow(hwnd,SW_SHOWNA);
#endif

	return hwnd;
}

static DWORD WINAPI VIDREN_ThreadEntry( LPVOID pv )
{
	CVideoRendererImpl*	This = (CVideoRendererImpl*)pv;
	MSG	msg;

	TGSetThreadName(-1, "QUARTZ video renderer");

	TRACE("(%p)\n",This);
	if ( !VIDREN_Create( (HWND)NULL, This ) )
		return 0;
	TRACE("VIDREN_Create succeeded\n");

	SetEvent( This->m_hEventInit );
	TRACE("Enter message loop\n");

	This->m_hEventWait = CreateEventA(NULL,TRUE,FALSE,NULL);

	while ( GetMessageA(&msg,(HWND)NULL,0,0) )
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	CloseHandle(This->m_hEventWait);
	This->m_hEventWait = (HANDLE)NULL;

	UnregisterClassA(VIDREN_szWndClass, (HINSTANCE)GetModuleHandleA(NULL));

	return 0;
}

static HRESULT VIDREN_StartThread( CVideoRendererImpl* This )
{
	DWORD dwRes;
	DWORD dwThreadId;
	HANDLE	hEvents[2];

	if ( This->m_hEventInit != (HANDLE)NULL ||
		 This->m_hwnd != (HWND)NULL ||
		 This->m_hThread != (HANDLE)NULL ||
		 This->pPin->pin.pmtConn == NULL )
		return E_UNEXPECTED;

	This->m_hEventInit = CreateEventA(NULL,TRUE,FALSE,NULL);
	if ( This->m_hEventInit == (HANDLE)NULL )
		return E_OUTOFMEMORY;

	This->m_hThread = CreateThread(
		NULL, 0,
		VIDREN_ThreadEntry,
		(LPVOID)This,
		0, &dwThreadId );
	if ( This->m_hThread == (HANDLE)NULL )
		return E_FAIL;

	hEvents[0] = This->m_hEventInit;
	hEvents[1] = This->m_hThread;

	dwRes = WaitForMultipleObjects(2,hEvents,FALSE,INFINITE);
	if ( dwRes != WAIT_OBJECT_0 )
		return E_FAIL;

	return S_OK;
}

static void VIDREN_EndThread( CVideoRendererImpl* This )
{
	if ( This->m_hwnd != (HWND)NULL )
		PostMessageA( This->m_hwnd, VIDRENMSG_ENDTHREAD, 0, 0 );

	if ( This->m_hThread != (HANDLE)NULL )
	{
		/* dispatch any waiting sendmessages */
		{
			MSG msg;
			PeekMessageA(&msg, 0, 0, 0, PM_NOREMOVE);
		}
		/* to avoid deadlocks, allow SendMessages from render thread
		 * through while we wait for it */
		while (MsgWaitForMultipleObjects(1, &This->m_hThread, FALSE,
						 INFINITE, QS_SENDMESSAGE) == WAIT_OBJECT_0+1)
		{
			MSG msg;
			PeekMessageA(&msg, 0, 0, 0, PM_NOREMOVE);
		}
		CloseHandle( This->m_hThread );
		This->m_hThread = (HANDLE)NULL;
	}
	if ( This->m_hEventInit != (HANDLE)NULL )
	{
		CloseHandle( This->m_hEventInit );
		This->m_hEventInit = (HANDLE)NULL;
	}
}



/***************************************************************************
 *
 *	CVideoRendererImpl methods
 *
 */

static HRESULT CVideoRendererImpl_OnActive( CBaseFilterImpl* pImpl )
{
	CVideoRendererImpl_THIS(pImpl,basefilter);

	TRACE( "(%p)\n", This );

	This->m_bSampleIsValid = FALSE;
	This->m_bSampleNeedView = FALSE;
	This->m_bSampleNeedWait = FALSE;

	return NOERROR;
}

static HRESULT CVideoRendererImpl_OnInactive( CBaseFilterImpl* pImpl )
{
	CVideoRendererImpl_THIS(pImpl,basefilter);

	TRACE( "(%p)\n", This );

	EnterCriticalSection( &This->m_csReceive );
	This->m_bSampleIsValid = FALSE;
	This->m_bSampleNeedView = FALSE;
	This->m_bSampleNeedWait = FALSE;
	LeaveCriticalSection( &This->m_csReceive );

	return NOERROR;
}

static const CBaseFilterHandlers filterhandlers =
{
	CVideoRendererImpl_OnActive, /* pOnActive */
	CVideoRendererImpl_OnInactive, /* pOnInactive */
	NULL, /* pOnStop */
};

/***************************************************************************
 *
 *	CVideoRendererPinImpl methods
 *
 */

static HRESULT CVideoRendererPinImpl_OnPreConnect( CPinBaseImpl* pImpl, IPin* pPin )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	TRACE("(%p,%p)\n",This,pPin);

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_OnPostConnect( CPinBaseImpl* pImpl, IPin* pPin )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);
	const VIDEOINFOHEADER* pinfo;
	HRESULT hr;

	TRACE("(%p,%p)\n",This,pPin);

	if ( This->pRender->m_pSampleData != NULL )
	{
		SelectObject(This->pRender->m_hMemDC, This->pRender->m_hOldBitmap);
		DeleteObject(This->pRender->m_hBitmap);
		This->pRender->m_hBitmap = (HBITMAP)NULL;
		This->pRender->m_hOldBitmap = (HBITMAP)NULL;
		This->pRender->m_pSampleData = NULL;
	}
	This->pRender->m_cbSampleData = 0;
	This->pRender->m_bSampleIsValid = FALSE;
	This->pRender->m_bSampleNeedView = FALSE;
	This->pRender->m_bSampleNeedWait = FALSE;

	pinfo = (const VIDEOINFOHEADER*)This->pin.pmtConn->pbFormat;
	if ( pinfo == NULL )
		return E_FAIL;

	This->pRender->m_bSampleIsValid = FALSE;
	This->pRender->m_cbSampleData = DIBSIZE(pinfo->bmiHeader);
	if ( !This->pRender->m_hMemDC )
		This->pRender->m_hMemDC = CreateCompatibleDC(0);
	This->pRender->m_hBitmap = CreateDIBSection(This->pRender->m_hMemDC,
						    (BITMAPINFO*)&pinfo->bmiHeader,
						    DIB_RGB_COLORS, /* FIXME */
						    (LPVOID*)&This->pRender->m_pSampleData,
						    0, 0);
	if ( !This->pRender->m_hBitmap )
		return E_OUTOFMEMORY;
	This->pRender->m_hOldBitmap = SelectObject(This->pRender->m_hMemDC, This->pRender->m_hBitmap);

	hr = VIDREN_StartThread(This->pRender);
	if ( FAILED(hr) )
		return hr;

	This->pRender->m_UseDirectDraw = 0;
	DDRender_DestroySurfaces(This->pRender);
	if (DDRender_CreateSurfaces(This->pRender, abs(pinfo->bmiHeader.biWidth), abs(pinfo->bmiHeader.biHeight), 4) == S_OK)
	{
		TRACE("Rendering with DirectDraw\n");
		This->pRender->m_UseDirectDraw = 1;
	}
	else
		WARN("DirectDraw not available - Rendering with GDI\n");

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_OnDisconnect( CPinBaseImpl* pImpl )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	TRACE("(%p)\n",This);

	VIDREN_EndThread(This->pRender);

	if ( This->pRender->m_pSampleData != NULL )
	{
		SelectObject(This->pRender->m_hMemDC, This->pRender->m_hOldBitmap);
		DeleteObject(This->pRender->m_hBitmap);
		This->pRender->m_hBitmap = (HBITMAP)NULL;
		This->pRender->m_hOldBitmap = (HBITMAP)NULL;
		This->pRender->m_pSampleData = NULL;
	}
	if ( This->pRender->m_hMemDC )
	{
		DeleteDC( This->pRender->m_hMemDC );
		This->pRender->m_hMemDC = (HDC)NULL;
	}
	This->pRender->m_cbSampleData = 0;
	This->pRender->m_bSampleIsValid = FALSE;
	This->pRender->m_bSampleNeedView = FALSE;
	This->pRender->m_bSampleNeedWait = FALSE;

	if ( This->meminput.pAllocator != NULL )
	{
		IMemAllocator_Decommit(This->meminput.pAllocator);
		IMemAllocator_Release(This->meminput.pAllocator);
		This->meminput.pAllocator = NULL;
	}

	DDRender_DestroySurfaces(This->pRender);

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_CheckMediaType( CPinBaseImpl* pImpl, const AM_MEDIA_TYPE* pmt )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);
	const VIDEOINFOHEADER* pinfo;

	TRACE("(%p,%p)\n",This,pmt);

	if ( !IsEqualGUID( &pmt->majortype, &MEDIATYPE_Video ) )
		return E_FAIL;
	if ( !IsEqualGUID( &pmt->formattype, &FORMAT_VideoInfo ) )
		return E_FAIL;

	/*
	 * check subtype.
	 */
#ifdef __APPLE__
	if ( !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_RGB32 ) )
		return E_FAIL;
#else
	if ( !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_RGB555 ) &&
	     !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_RGB565 ) &&
	     !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_RGB24 ) &&
	     !IsEqualGUID( &pmt->subtype, &MEDIASUBTYPE_RGB32 ) )
		return E_FAIL;
#endif

	pinfo = (const VIDEOINFOHEADER*)pmt->pbFormat;
	if ( pinfo == NULL ||
		 pinfo->bmiHeader.biSize < sizeof(BITMAPINFOHEADER) ||
		 pinfo->bmiHeader.biWidth <= 0 ||
		 pinfo->bmiHeader.biHeight == 0 ||
		 pinfo->bmiHeader.biPlanes != 1 ||
		 pinfo->bmiHeader.biCompression != 0 )
		return E_FAIL;

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_Receive( CPinBaseImpl* pImpl, IMediaSample* pSample )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);
	HWND hwnd;
	REFERENCE_TIME rtStart = 0, rtStop = 0, rtNow = 0, rtPref = 0;
	HRESULT hr, thr = VFW_E_SAMPLE_TIME_NOT_SET;

	TRACE( "(%p,%p)\n",This,pSample );

	hwnd = This->pRender->m_hwnd;
	if ( hwnd == (HWND)NULL ||
		 This->pRender->m_hThread == (HWND)NULL )
		return E_UNEXPECTED;
	if ( This->pRender->m_fInFlush )
		return S_FALSE;
	if ( pSample == NULL )
		return E_POINTER;

	if ( This->pRender->basefilter.pClock ) {
		thr = IMediaSample_GetTime(pSample, &rtStart, &rtStop);
		/* S_OK = rtStart and rtStop is valid */
		/* VFW_S_NO_STOP_TIME = rtStart is valid */
		/* VFW_E_SAMPLE_TIME_NOT_SET = neither is valid */
		if ( SUCCEEDED(thr) ) {
			Quality q;
			BOOL do_quality = FALSE;

			/* rtStart is valid */
			if ( thr != S_OK )
				rtStop = rtStart;
			hr = IReferenceClock_GetTime(This->pRender->basefilter.pClock, &rtNow);
			if ( FAILED(hr) )
				return hr;
			rtNow -= This->pRender->basefilter.rtStart;

			TRACE("start=%lld, stop=%lld, clock=%lld, delta=%lld\n", rtStart, rtStop, rtNow, rtStart - rtNow);

			rtPref = rtStart - PREFER_EARLY;

			q.Proportion = 1000; /* no change */
			q.Late = rtNow - rtPref;
			q.TimeStamp = rtStart;

			if (rtStart < rtNow) {
				q.Type = Famine;
				do_quality = TRUE;
#ifdef QUALITY_CHANGE
				if (rtStop < rtNow) {
					ERR("video frame too late - requesting 20%% quality reduction\n");
					/* ask upstream to work faster */
					/* FIXME: calculate proportion properly */
					q.Proportion = 1200; /* 120% */
				}
				else if (rtStart < rtNow) {
					WARN("video frame late - requesting 10%% quality reduction\n");
					/* FIXME: calculate proportion properly */
					q.Proportion = 1100; /* 110% */
				}
#endif
				TRACE("sending Famine quality control message\n");
			}
			if (rtNow < (rtStart - TOO_EARLY) && rtNow >= 0) {
				q.Type = Flood;
				WARN("video frame early - requesting 10%% quality increase\n");
				do_quality = TRUE;
#ifdef QUALITY_CHANGE
				/* FIXME: calculate proportion properly */
				q.Proportion = 900; /* 90% */
#endif
				TRACE("sending Flood quality control message\n");
			}
#ifdef QUALITY_CONTROL
			if (do_quality) {
				IQualityControl *qc;
				hr = IPin_QueryInterface(This->pin.pPinConnectedTo, &IID_IQualityControl, (LPVOID*)&qc);
				if ( SUCCEEDED(hr) ) {
					IQualityControl_Notify(qc, (IBaseFilter*)&This->pRender->basefilter, q);
					IQualityControl_Release(qc);
				}
			}
#endif
		}
		else TRACE("media time result %08x\n", thr);
	}
	else TRACE("no clock\n");

	/* FIXME - wait/skip/qualitycontrol */

#if 1
	if ( This->pRender->m_bSampleNeedView && This->pRender->m_bSampleNeedWait && SUCCEEDED(thr) ) {
		if ( rtStart > rtNow ) {
			if (This->pRender->m_rtSample > rtNow) {
				/* wait for presentation time of previous frame */
				TRACE("previous frame is waiting for %lld\n", This->pRender->m_rtSample);
#ifdef USE_USLEEP
				usleep((This->pRender->m_rtSample - rtNow) / 10);
#else
				Sleep((This->pRender->m_rtSample - rtNow) / 10000);
#endif
				TRACE("wait complete\n");
			}
#ifdef DIRECT_UPDATE
			VIDREN_DoUpdate( This->pRender, hwnd, NULL );
#else
			SendMessageA( hwnd, VIDRENMSG_UPDATE, 0, 0 );
#endif
		}
		else {
			TRACE("need to skip previous frame\n");
			if ( ++This->pRender->m_skipFrames >= MAX_SKIP ) {
				This->pRender->m_skipFrames = 0;
				TRACE("max skip exceeded, rendering anyway\n");
#ifdef DIRECT_UPDATE
				VIDREN_DoUpdate( This->pRender, hwnd, NULL );
#else
				SendMessageA( hwnd, VIDRENMSG_UPDATE, 0, 0 );
#endif
			}
		}
	}
#ifdef ALLOW_NOCOPY
	if ( rtStop < rtNow ) {
		TRACE("need to skip this frame\n");
		if ( ++This->pRender->m_skipFrames >= MAX_SKIP ) {
			BYTE* pData;
			This->pRender->m_skipFrames = 0;
			TRACE("max skip exceeded, rendering frame without storing it\n");
			IMediaSample_GetPointer( pSample, &pData );
			VIDREN_DoUpdate( This->pRender, hwnd, pData );
		}
	}
	else if ( rtStart < rtNow ) {
		BYTE* pData;
		TRACE("rendering frame without storing it\n");
		This->pRender->m_skipFrames = 0;
		IMediaSample_GetPointer( pSample, &pData );
		VIDREN_DoUpdate( This->pRender, hwnd, pData );
	}
	else
#endif
	{
		/* duplicate this sample. */
		This->pRender->m_rtSample = rtStart;
		This->pRender->m_bSampleNeedWait = SUCCEEDED(thr);
		VIDREN_CopySample( This->pRender, SUCCEEDED(thr) ? 0 : hwnd, pSample );
	}
#else
	IMediaSample_AddRef(pSample);
	InterlockedIncrement(&This->pRender->m_numFrames);
	if (!PostMessageA( hwnd, VIDRENMSG_NEWFRAME, 0, (LPARAM)pSample )) {
		WARN("could not deliver video frame\n");
		IMediaSample_Release(pSample);
		InterlockedDecrement(&This->pRender->m_numFrames);
	}
#endif

	/* FIXME: block if paused */

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_ReceiveCanBlock( CPinBaseImpl* pImpl )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	/* might block. */
	return S_OK;
}

static HRESULT CVideoRendererPinImpl_EndOfStream( CPinBaseImpl* pImpl )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	FIXME( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;
	This->pRender->m_skipFrames = 0;

	/* in case the last frame isn't drawn yet */
	PostMessageA( This->pRender->m_hwnd, VIDRENMSG_UPDATE, 0, 0 );

	/* FIXME - don't notify twice until stopped or seeked. */
	return CBaseFilterImpl_MediaEventNotify(
		&This->pRender->basefilter, EC_COMPLETE,
		(LONG_PTR)S_OK, (LONG_PTR)(IBaseFilter*)(This->pRender) );
}

static HRESULT CVideoRendererPinImpl_BeginFlush( CPinBaseImpl* pImpl )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	This->pRender->m_fInFlush = TRUE;
	EnterCriticalSection( &This->pRender->m_csReceive );
	This->pRender->m_bSampleIsValid = FALSE;
	This->pRender->m_bSampleNeedView = FALSE;
	This->pRender->m_bSampleNeedWait = FALSE;
	if (This->pRender->m_hEventWait)
		SetEvent(This->pRender->m_hEventWait);
	LeaveCriticalSection( &This->pRender->m_csReceive );

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_EndFlush( CPinBaseImpl* pImpl )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	TRACE( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;
	This->pRender->m_skipFrames = 0;
	if (This->pRender->m_hEventWait)
		ResetEvent(This->pRender->m_hEventWait);

	return NOERROR;
}

static HRESULT CVideoRendererPinImpl_NewSegment( CPinBaseImpl* pImpl, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double rate )
{
	CVideoRendererPinImpl_THIS(pImpl,pin);

	FIXME( "(%p)\n", This );

	This->pRender->m_fInFlush = FALSE;
	This->pRender->m_skipFrames = 0;

	return NOERROR;
}




static const CBasePinHandlers pinhandlers =
{
	CVideoRendererPinImpl_OnPreConnect, /* pOnPreConnect */
	CVideoRendererPinImpl_OnPostConnect, /* pOnPostConnect */
	CVideoRendererPinImpl_OnDisconnect, /* pOnDisconnect */
	CVideoRendererPinImpl_CheckMediaType, /* pCheckMediaType */
	NULL, /* pQualityNotify */
	CVideoRendererPinImpl_Receive, /* pReceive */
	CVideoRendererPinImpl_ReceiveCanBlock, /* pReceiveCanBlock */
	CVideoRendererPinImpl_EndOfStream, /* pEndOfStream */
	CVideoRendererPinImpl_BeginFlush, /* pBeginFlush */
	CVideoRendererPinImpl_EndFlush, /* pEndFlush */
	CVideoRendererPinImpl_NewSegment, /* pNewSegment */
};


/***************************************************************************
 *
 *	new/delete CVideoRendererImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry FilterIFEntries[] =
{
  { &IID_IPersist, offsetof(CVideoRendererImpl,basefilter)-offsetof(CVideoRendererImpl,unk) },
  { &IID_IMediaFilter, offsetof(CVideoRendererImpl,basefilter)-offsetof(CVideoRendererImpl,unk) },
  { &IID_IBaseFilter, offsetof(CVideoRendererImpl,basefilter)-offsetof(CVideoRendererImpl,unk) },
  { &IID_IBasicVideo, offsetof(CVideoRendererImpl,basvid)-offsetof(CVideoRendererImpl,unk) },
  { &IID_IVideoWindow, offsetof(CVideoRendererImpl,vidwin)-offsetof(CVideoRendererImpl,unk) },
};

static HRESULT CVideoRendererImpl_OnQueryInterface(
	IUnknown* punk, const IID* piid, void** ppobj )
{
	CVideoRendererImpl_THIS(punk,unk);

	if ( ppobj == NULL )
		return E_POINTER;
	*ppobj = NULL;

	if ( This->pSeekPass == NULL )
		return E_NOINTERFACE;

	if ( IsEqualGUID( &IID_IMediaPosition, piid ) ||
		 IsEqualGUID( &IID_IMediaSeeking, piid ) )
	{
		TRACE( "IMediaSeeking(or IMediaPosition) is queried\n" );
		return IUnknown_QueryInterface( (IUnknown*)(&This->pSeekPass->unk), piid, ppobj );
	}

	return E_NOINTERFACE;
}

static void QUARTZ_DestroyVideoRenderer(IUnknown* punk)
{
	CVideoRendererImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );
	CVideoRendererImpl_OnInactive(&This->basefilter);
	VIDREN_EndThread(This);

	if ( This->pPin != NULL )
	{
		IUnknown_Release(This->pPin->unk.punkControl);
		This->pPin = NULL;
	}
	if ( This->pSeekPass != NULL )
	{
		IUnknown_Release((IUnknown*)&This->pSeekPass->unk);
		This->pSeekPass = NULL;
	}

	CVideoRendererImpl_UninitIBasicVideo(This);
	CVideoRendererImpl_UninitIVideoWindow(This);
	CBaseFilterImpl_UninitIBaseFilter(&This->basefilter);

	DeleteCriticalSection( &This->m_csReceive );
}

HRESULT QUARTZ_CreateVideoRenderer(IUnknown* punkOuter,void** ppobj)
{
	CVideoRendererImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p)\n",punkOuter,ppobj);

	This = (CVideoRendererImpl*)
		QUARTZ_AllocObj( sizeof(CVideoRendererImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;
	This->pSeekPass = NULL;
	This->pPin = NULL;
	This->m_fInFlush = FALSE;

	This->m_hEventInit = (HANDLE)NULL;
	This->m_hThread = (HANDLE)NULL;
	This->m_hEventWait = (HANDLE)NULL;
	This->m_hwnd = (HWND)NULL;
	This->m_bSampleIsValid = FALSE;
	This->m_bSampleNeedView = FALSE;
	This->m_bSampleNeedWait = FALSE;
	This->m_pSampleData = NULL;
	This->m_cbSampleData = 0;
	This->m_hBitmap = (HBITMAP)NULL;
	This->m_hOldBitmap = (HBITMAP)NULL;
	This->m_hMemDC = (HDC)NULL;
	This->m_pSample = NULL;
	This->m_numFrames = 0;
	This->m_skipFrames = 0;
	This->m_pDDSFront = NULL;
	This->m_pDDSBack = NULL;
	This->m_pDDSSource = NULL;
	This->m_lpddraw = NULL;

	QUARTZ_IUnkInit( &This->unk, punkOuter );
	This->qiext.pNext = NULL;
	This->qiext.pOnQueryInterface = &CVideoRendererImpl_OnQueryInterface;
	QUARTZ_IUnkAddDelegation( &This->unk, &This->qiext );

	hr = CBaseFilterImpl_InitIBaseFilter(
		&This->basefilter,
		This->unk.punkControl,
		&CLSID_VideoRenderer,
		QUARTZ_VideoRenderer_Name,
		&filterhandlers );
	if ( SUCCEEDED(hr) )
	{
		hr = CVideoRendererImpl_InitIBasicVideo(This);
		if ( SUCCEEDED(hr) )
		{
			hr = CVideoRendererImpl_InitIVideoWindow(This);
			if ( FAILED(hr) )
			{
				CVideoRendererImpl_UninitIBasicVideo(This);
			}
		}
		if ( FAILED(hr) )
		{
			CBaseFilterImpl_UninitIBaseFilter(&This->basefilter);
		}
	}

	if ( FAILED(hr) )
	{
		QUARTZ_FreeObj(This);
		return hr;
	}

	This->unk.pEntries = FilterIFEntries;
	This->unk.dwEntries = sizeof(FilterIFEntries)/sizeof(FilterIFEntries[0]);
	This->unk.pOnFinalRelease = QUARTZ_DestroyVideoRenderer;

	CRITICAL_SECTION_DEFINE( &This->m_csReceive );

	hr = QUARTZ_CreateVideoRendererPin(
		This,
		&This->basefilter.csFilter,
		&This->m_csReceive,
		&This->pPin );
	if ( SUCCEEDED(hr) )
		hr = QUARTZ_CompList_AddComp(
			This->basefilter.pInPins,
			(IUnknown*)&This->pPin->pin,
			NULL, 0 );
	if ( SUCCEEDED(hr) )
		hr = QUARTZ_CreateSeekingPassThruInternal(
			(IUnknown*)&(This->unk), &This->pSeekPass,
			TRUE, (IPin*)&(This->pPin->pin) );

	if ( FAILED(hr) )
	{
		IUnknown_Release( This->unk.punkControl );
		return hr;
	}

	*ppobj = (void*)&(This->unk);

	return S_OK;
}

/***************************************************************************
 *
 *	new/delete CVideoRendererPinImpl
 *
 */

/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry PinIFEntries[] =
{
  { &IID_IPin, offsetof(CVideoRendererPinImpl,pin)-offsetof(CVideoRendererPinImpl,unk) },
  { &IID_IMemInputPin, offsetof(CVideoRendererPinImpl,meminput)-offsetof(CVideoRendererPinImpl,unk) },
};

static void QUARTZ_DestroyVideoRendererPin(IUnknown* punk)
{
	CVideoRendererPinImpl_THIS(punk,unk);

	TRACE( "(%p)\n", This );

	CPinBaseImpl_UninitIPin( &This->pin );
	CMemInputPinBaseImpl_UninitIMemInputPin( &This->meminput );
}

HRESULT QUARTZ_CreateVideoRendererPin(
        CVideoRendererImpl* pFilter,
        CRITICAL_SECTION* pcsPin,
        CRITICAL_SECTION* pcsPinReceive,
        CVideoRendererPinImpl** ppPin)
{
	CVideoRendererPinImpl*	This = NULL;
	HRESULT hr;

	TRACE("(%p,%p,%p,%p)\n",pFilter,pcsPin,pcsPinReceive,ppPin);

	This = (CVideoRendererPinImpl*)
		QUARTZ_AllocObj( sizeof(CVideoRendererPinImpl) );
	if ( This == NULL )
		return E_OUTOFMEMORY;

	QUARTZ_IUnkInit( &This->unk, NULL );
	This->pRender = pFilter;

	hr = CPinBaseImpl_InitIPin(
		&This->pin,
		This->unk.punkControl,
		pcsPin, pcsPinReceive,
		&pFilter->basefilter,
		QUARTZ_VideoRendererPin_Name,
		FALSE,
		&pinhandlers );

	if ( SUCCEEDED(hr) )
	{
		hr = CMemInputPinBaseImpl_InitIMemInputPin(
			&This->meminput,
			This->unk.punkControl,
			&This->pin );
		if ( FAILED(hr) )
		{
			CPinBaseImpl_UninitIPin( &This->pin );
		}
	}

	if ( FAILED(hr) )
	{
		QUARTZ_FreeObj(This);
		return hr;
	}

	This->unk.pEntries = PinIFEntries;
	This->unk.dwEntries = sizeof(PinIFEntries)/sizeof(PinIFEntries[0]);
	This->unk.pOnFinalRelease = QUARTZ_DestroyVideoRendererPin;

	*ppPin = This;

	TRACE("returned successfully.\n");

	return S_OK;
}

/***************************************************************************
 *
 *	CVideoRendererImpl::IBasicVideo
 *
 */


static HRESULT WINAPI
IBasicVideo_fnQueryInterface(IBasicVideo* iface,REFIID riid,void** ppobj)
{
	CVideoRendererImpl_THIS(iface,basvid);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IBasicVideo_fnAddRef(IBasicVideo* iface)
{
	CVideoRendererImpl_THIS(iface,basvid);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IBasicVideo_fnRelease(IBasicVideo* iface)
{
	CVideoRendererImpl_THIS(iface,basvid);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IBasicVideo_fnGetTypeInfoCount(IBasicVideo* iface,UINT* pcTypeInfo)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetTypeInfo(IBasicVideo* iface,UINT iTypeInfo, LCID lcid, ITypeInfo** ppobj)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetIDsOfNames(IBasicVideo* iface,REFIID riid, LPOLESTR* ppwszName, UINT cNames, LCID lcid, DISPID* pDispId)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnInvoke(IBasicVideo* iface,DISPID DispId, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarRes, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}


static HRESULT WINAPI
IBasicVideo_fnget_AvgTimePerFrame(IBasicVideo* iface,REFTIME* prefTime)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_BitRate(IBasicVideo* iface,long* plRate)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_BitErrorRate(IBasicVideo* iface,long* plRate)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_VideoWidth(IBasicVideo* iface,long* plWidth)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_VideoHeight(IBasicVideo* iface,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_SourceLeft(IBasicVideo* iface,long lLeft)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_SourceLeft(IBasicVideo* iface,long* plLeft)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_SourceWidth(IBasicVideo* iface,long lWidth)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_SourceWidth(IBasicVideo* iface,long* plWidth)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_SourceTop(IBasicVideo* iface,long lTop)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_SourceTop(IBasicVideo* iface,long* plTop)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_SourceHeight(IBasicVideo* iface,long lHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_SourceHeight(IBasicVideo* iface,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_DestinationLeft(IBasicVideo* iface,long lLeft)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_DestinationLeft(IBasicVideo* iface,long* plLeft)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_DestinationWidth(IBasicVideo* iface,long lWidth)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_DestinationWidth(IBasicVideo* iface,long* plWidth)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_DestinationTop(IBasicVideo* iface,long lTop)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_DestinationTop(IBasicVideo* iface,long* plTop)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnput_DestinationHeight(IBasicVideo* iface,long lHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnget_DestinationHeight(IBasicVideo* iface,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnSetSourcePosition(IBasicVideo* iface,long lLeft,long lTop,long lWidth,long lHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetSourcePosition(IBasicVideo* iface,long* plLeft,long* plTop,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnSetDefaultSourcePosition(IBasicVideo* iface)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnSetDestinationPosition(IBasicVideo* iface,long lLeft,long lTop,long lWidth,long lHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetDestinationPosition(IBasicVideo* iface,long* plLeft,long* plTop,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnSetDefaultDestinationPosition(IBasicVideo* iface)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetVideoSize(IBasicVideo* iface,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetVideoPaletteEntries(IBasicVideo* iface,long lStart,long lCount,long* plRet,long* plPaletteEntry)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnGetCurrentImage(IBasicVideo* iface,long* plBufferSize,long* plDIBBuffer)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnIsUsingDefaultSource(IBasicVideo* iface)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IBasicVideo_fnIsUsingDefaultDestination(IBasicVideo* iface)
{
	CVideoRendererImpl_THIS(iface,basvid);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}




static ICOM_VTABLE(IBasicVideo) ibasicvideo =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IBasicVideo_fnQueryInterface,
	IBasicVideo_fnAddRef,
	IBasicVideo_fnRelease,
	/* IDispatch fields */
	IBasicVideo_fnGetTypeInfoCount,
	IBasicVideo_fnGetTypeInfo,
	IBasicVideo_fnGetIDsOfNames,
	IBasicVideo_fnInvoke,
	/* IBasicVideo fields */
	IBasicVideo_fnget_AvgTimePerFrame,
	IBasicVideo_fnget_BitRate,
	IBasicVideo_fnget_BitErrorRate,
	IBasicVideo_fnget_VideoWidth,
	IBasicVideo_fnget_VideoHeight,
	IBasicVideo_fnput_SourceLeft,
	IBasicVideo_fnget_SourceLeft,
	IBasicVideo_fnput_SourceWidth,
	IBasicVideo_fnget_SourceWidth,
	IBasicVideo_fnput_SourceTop,
	IBasicVideo_fnget_SourceTop,
	IBasicVideo_fnput_SourceHeight,
	IBasicVideo_fnget_SourceHeight,
	IBasicVideo_fnput_DestinationLeft,
	IBasicVideo_fnget_DestinationLeft,
	IBasicVideo_fnput_DestinationWidth,
	IBasicVideo_fnget_DestinationWidth,
	IBasicVideo_fnput_DestinationTop,
	IBasicVideo_fnget_DestinationTop,
	IBasicVideo_fnput_DestinationHeight,
	IBasicVideo_fnget_DestinationHeight,
	IBasicVideo_fnSetSourcePosition,
	IBasicVideo_fnGetSourcePosition,
	IBasicVideo_fnSetDefaultSourcePosition,
	IBasicVideo_fnSetDestinationPosition,
	IBasicVideo_fnGetDestinationPosition,
	IBasicVideo_fnSetDefaultDestinationPosition,
	IBasicVideo_fnGetVideoSize,
	IBasicVideo_fnGetVideoPaletteEntries,
	IBasicVideo_fnGetCurrentImage,
	IBasicVideo_fnIsUsingDefaultSource,
	IBasicVideo_fnIsUsingDefaultDestination,
};


HRESULT CVideoRendererImpl_InitIBasicVideo( CVideoRendererImpl* This )
{
	TRACE("(%p)\n",This);
	ICOM_VTBL(&This->basvid) = &ibasicvideo;

	DDRender_Init(This);

	return NOERROR;
}

void CVideoRendererImpl_UninitIBasicVideo( CVideoRendererImpl* This )
{
	TRACE("(%p)\n",This);

	DDRender_DestroySurfaces(This);
	DDRender_Exit(This);
}

/***************************************************************************
 *
 *	CVideoRendererImpl::IVideoWindow
 *
 */


static HRESULT WINAPI
IVideoWindow_fnQueryInterface(IVideoWindow* iface,REFIID riid,void** ppobj)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IVideoWindow_fnAddRef(IVideoWindow* iface)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IVideoWindow_fnRelease(IVideoWindow* iface)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IVideoWindow_fnGetTypeInfoCount(IVideoWindow* iface,UINT* pcTypeInfo)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IVideoWindow_fnGetTypeInfo(IVideoWindow* iface,UINT iTypeInfo, LCID lcid, ITypeInfo** ppobj)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IVideoWindow_fnGetIDsOfNames(IVideoWindow* iface,REFIID riid, LPOLESTR* ppwszName, UINT cNames, LCID lcid, DISPID* pDispId)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}

static HRESULT WINAPI
IVideoWindow_fnInvoke(IVideoWindow* iface,DISPID DispId, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarRes, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
	CVideoRendererImpl_THIS(iface,vidwin);

	FIXME("(%p)->()\n",This);

	return E_NOTIMPL;
}



static HRESULT WINAPI
IVideoWindow_fnput_Caption(IVideoWindow* iface,BSTR strCaption)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Caption(IVideoWindow* iface,BSTR* pstrCaption)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_WindowStyle(IVideoWindow* iface,long lStyle)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	SetLastError(0);
	if ( SetWindowLongPtrA( This->m_hwnd, GWL_STYLE, (DWORD_PTR)lStyle ) == 0 &&
		 GetLastError() != 0 )
	{
		hr = E_FAIL;
		goto end;
	}

	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_WindowStyle(IVideoWindow* iface,long* plStyle)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	*plStyle = (LONG)GetWindowLongA( This->m_hwnd, GWL_STYLE );
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_WindowStyleEx(IVideoWindow* iface,long lExStyle)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	SetLastError(0);
	if ( SetWindowLongPtrA( This->m_hwnd, GWL_EXSTYLE, (DWORD_PTR)lExStyle ) == 0 &&
		 GetLastError() != 0 )
	{
		hr = E_FAIL;
		goto end;
	}

	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_WindowStyleEx(IVideoWindow* iface,long* plExStyle)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	if ( plExStyle == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	*plExStyle = (LONG)GetWindowLongA( This->m_hwnd, GWL_EXSTYLE );
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_AutoShow(IVideoWindow* iface,long lAutoShow)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_AutoShow(IVideoWindow* iface,long* plAutoShow)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_WindowState(IVideoWindow* iface,long lState)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_WindowState(IVideoWindow* iface,long* plState)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_BackgroundPalette(IVideoWindow* iface,long lBackPal)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_BackgroundPalette(IVideoWindow* iface,long* plBackPal)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_Visible(IVideoWindow* iface,long lVisible)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Visible(IVideoWindow* iface,long* plVisible)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_Left(IVideoWindow* iface,long lLeft)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	if ( ! MoveWindow( This->m_hwnd, lLeft, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE ) )
	{
		hr = E_FAIL;
		goto end;
	}
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Left(IVideoWindow* iface,long* plLeft)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( plLeft == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	*plLeft = rc.left;
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_Width(IVideoWindow* iface,long lWidth)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( lWidth < 0 )
		return E_INVALIDARG;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	if ( ! MoveWindow( This->m_hwnd, rc.left, rc.top, lWidth, rc.bottom-rc.top, TRUE ) )
	{
		hr = E_FAIL;
		goto end;
	}
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Width(IVideoWindow* iface,long* plWidth)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( plWidth == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	*plWidth = rc.right-rc.left;
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_Top(IVideoWindow* iface,long lTop)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	if ( ! MoveWindow( This->m_hwnd, rc.left, lTop, rc.right-rc.left, rc.bottom-rc.top, TRUE ) )
	{
		hr = E_FAIL;
		goto end;
	}
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Top(IVideoWindow* iface,long* plTop)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( plTop == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	*plTop = rc.top;
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_Height(IVideoWindow* iface,long lHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( lHeight < 0 )
		return E_INVALIDARG;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	if ( ! MoveWindow( This->m_hwnd, rc.left, rc.top, rc.right-rc.left, lHeight, TRUE ) )
	{
		hr = E_FAIL;
		goto end;
	}
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Height(IVideoWindow* iface,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( plHeight == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}
	*plHeight = rc.bottom-rc.top;
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_Owner(IVideoWindow* iface,OAHWND hwnd)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	DWORD	dwStyle;
	RECT	rc;

	FIXME("(%p)->(0x%08lx)\n",This,hwnd);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}
	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}

	dwStyle = (DWORD)GetWindowLongA( This->m_hwnd, GWL_STYLE );
	if ( hwnd == (OAHWND)0 )
		SetWindowLongA( This->m_hwnd, GWL_STYLE, dwStyle & (~WS_CHILD) );
	SetParent( This->m_hwnd, (HWND)hwnd );
	if ( (HWND)hwnd != (HWND)NULL )
	{
		SetWindowLongA( This->m_hwnd, GWL_STYLE, dwStyle | WS_CHILD );
		MoveWindow( This->m_hwnd, 0, 0, rc.right-rc.left, rc.bottom-rc.top, TRUE );
	}

	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_Owner(IVideoWindow* iface,OAHWND* phwnd)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	if ( phwnd == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	*phwnd = (OAHWND)GetParent( This->m_hwnd );
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_MessageDrain(IVideoWindow* iface,OAHWND hwnd)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = S_OK;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_MessageDrain(IVideoWindow* iface,OAHWND* phwnd)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = S_OK;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_BorderColor(IVideoWindow* iface,long* plColor)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_BorderColor(IVideoWindow* iface,long lColor)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnget_FullScreenMode(IVideoWindow* iface,long* plMode)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnput_FullScreenMode(IVideoWindow* iface,long lMode)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnSetWindowForeground(IVideoWindow* iface,long lFocus)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnNotifyOwnerMessage(IVideoWindow* iface,OAHWND hwnd,long message,LONG_PTR wParam,LONG_PTR lParam)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnSetWindowPosition(IVideoWindow* iface,long lLeft,long lTop,long lWidth,long lHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	if ( ! MoveWindow( This->m_hwnd, lLeft, lTop, lWidth, lHeight, TRUE ) )
	{
		hr = E_FAIL;
		goto end;
	}
	hr = S_OK;
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnGetWindowPosition(IVideoWindow* iface,long* plLeft,long* plTop,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;
	RECT	rc;

	FIXME("(%p)->()\n",This);

	if ( plLeft == NULL || plTop == NULL ||
		 plWidth == NULL || plHeight == NULL )
		return E_POINTER;

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
	if ( ! GetWindowRect( This->m_hwnd, &rc ) )
	{
		hr = E_FAIL;
		goto end;
	}

	*plLeft = rc.left;
	*plTop = rc.top;
	*plWidth = rc.right-rc.left;
	*plHeight = rc.bottom-rc.top;
	hr = S_OK;

end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnGetMinIdealImageSize(IVideoWindow* iface,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnGetMaxIdealImageSize(IVideoWindow* iface,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnGetRestorePosition(IVideoWindow* iface,long* plLeft,long* plTop,long* plWidth,long* plHeight)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnHideCursor(IVideoWindow* iface,long lHide)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}

static HRESULT WINAPI
IVideoWindow_fnIsCursorHidden(IVideoWindow* iface,long* plHide)
{
	CVideoRendererImpl_THIS(iface,vidwin);
	HRESULT hr = E_NOTIMPL;

	FIXME("(%p)->()\n",This);

	EnterCriticalSection ( &This->basefilter.csFilter );
	if ( This->m_hwnd == (HWND)NULL )
	{
		hr = VFW_E_NOT_CONNECTED;
		goto end;
	}

	/* FIXME */
end:
	LeaveCriticalSection ( &This->basefilter.csFilter );

	return hr;
}




static ICOM_VTABLE(IVideoWindow) ivideowindow =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IVideoWindow_fnQueryInterface,
	IVideoWindow_fnAddRef,
	IVideoWindow_fnRelease,
	/* IDispatch fields */
	IVideoWindow_fnGetTypeInfoCount,
	IVideoWindow_fnGetTypeInfo,
	IVideoWindow_fnGetIDsOfNames,
	IVideoWindow_fnInvoke,
	/* IVideoWindow fields */
	IVideoWindow_fnput_Caption,
	IVideoWindow_fnget_Caption,
	IVideoWindow_fnput_WindowStyle,
	IVideoWindow_fnget_WindowStyle,
	IVideoWindow_fnput_WindowStyleEx,
	IVideoWindow_fnget_WindowStyleEx,
	IVideoWindow_fnput_AutoShow,
	IVideoWindow_fnget_AutoShow,
	IVideoWindow_fnput_WindowState,
	IVideoWindow_fnget_WindowState,
	IVideoWindow_fnput_BackgroundPalette,
	IVideoWindow_fnget_BackgroundPalette,
	IVideoWindow_fnput_Visible,
	IVideoWindow_fnget_Visible,
	IVideoWindow_fnput_Left,
	IVideoWindow_fnget_Left,
	IVideoWindow_fnput_Width,
	IVideoWindow_fnget_Width,
	IVideoWindow_fnput_Top,
	IVideoWindow_fnget_Top,
	IVideoWindow_fnput_Height,
	IVideoWindow_fnget_Height,
	IVideoWindow_fnput_Owner,
	IVideoWindow_fnget_Owner,
	IVideoWindow_fnput_MessageDrain,
	IVideoWindow_fnget_MessageDrain,
	IVideoWindow_fnget_BorderColor,
	IVideoWindow_fnput_BorderColor,
	IVideoWindow_fnget_FullScreenMode,
	IVideoWindow_fnput_FullScreenMode,
	IVideoWindow_fnSetWindowForeground,
	IVideoWindow_fnNotifyOwnerMessage,
	IVideoWindow_fnSetWindowPosition,
	IVideoWindow_fnGetWindowPosition,
	IVideoWindow_fnGetMinIdealImageSize,
	IVideoWindow_fnGetMaxIdealImageSize,
	IVideoWindow_fnGetRestorePosition,
	IVideoWindow_fnHideCursor,
	IVideoWindow_fnIsCursorHidden,

};


HRESULT CVideoRendererImpl_InitIVideoWindow( CVideoRendererImpl* This )
{
	TRACE("(%p)\n",This);
	ICOM_VTBL(&This->vidwin) = &ivideowindow;

	return NOERROR;
}

void CVideoRendererImpl_UninitIVideoWindow( CVideoRendererImpl* This )
{
	TRACE("(%p)\n",This);
}

