/*
 * Implementation of IFilterGraph and related interfaces
 *	+ IGraphVersion
 *
 * FIXME - create a thread to process some methods correctly.
 *
 * FIXME - ReconnectEx
 * FIXME - process Pause/Stop asynchronously and notify when completed.
 *
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
 */

#include "config.h"

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"
#include "strmif.h"
#include "control.h"
#include "uuids.h"
#include "vfwmsgs.h"
#include "evcode.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "fgraph.h"
#include "enumunk.h"
#include "sysclock.h"
#include "regsvr.h"


#ifndef NUMELEMS
#define NUMELEMS(elem)	(sizeof(elem)/sizeof(elem[0]))
#endif	/* NUMELEMS */

static HRESULT CFilterGraph_DisconnectAllPins( IBaseFilter* pFilter )
{
	IEnumPins*	pEnum = NULL;
	IPin*	pPin;
	IPin*	pConnTo;
	ULONG	cFetched;
	HRESULT	hr;

	hr = IBaseFilter_EnumPins( pFilter, &pEnum );
	if ( FAILED(hr) )
		return hr;
	if ( pEnum == NULL )
		return E_FAIL;

	while ( 1 )
	{
		pPin = NULL;
		cFetched = 0;
		hr = IEnumPins_Next( pEnum, 1, &pPin, &cFetched );
		if ( FAILED(hr) )
			break;
		if ( hr != NOERROR || pPin == NULL || cFetched != 1 )
		{
			hr = NOERROR;
			break;
		}

		pConnTo = NULL;
		hr = IPin_ConnectedTo(pPin,&pConnTo);
		if ( hr == NOERROR && pConnTo != NULL )
		{
			IPin_Disconnect(pPin);
			IPin_Disconnect(pConnTo);
			IPin_Release(pConnTo);
		}

		IPin_Release( pPin );
	}

	IEnumPins_Release( pEnum );

	return hr;
}


static HRESULT CFilterGraph_GraphChanged( CFilterGraph* This )
{
	/* IDistributorNotify_NotifyGraphChange() */

	This->m_lGraphVersion ++;

	return NOERROR;
}

/***************************************************************************
 *
 *	CFilterGraph internal methods for IFilterGraph2::AddSourceFilter().
 *
 */

static HRESULT QUARTZ_PeekFile(
	const WCHAR* pwszFileName,
	BYTE* pData, DWORD cbData, DWORD* pcbRead )
{
	HANDLE	hFile;
	HRESULT hr = E_FAIL;

	*pcbRead = 0;
	hFile = CreateFileW( pwszFileName,
		GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL );
	if ( hFile == INVALID_HANDLE_VALUE )
		return E_FAIL;
	if ( ReadFile( hFile, pData, cbData, pcbRead, NULL ) )
		hr = NOERROR;
	CloseHandle( hFile );

	return hr;
}


static const WCHAR* skip_space(const WCHAR* pwsz)
{
	if ( pwsz == NULL ) return NULL;
	while ( *pwsz == (WCHAR)' ' ) pwsz++;
	return pwsz;
}

static const WCHAR* get_dword(const WCHAR* pwsz,DWORD* pdw)
{
	DWORD	dw = 0;

	*pdw = 0;
	if ( pwsz == NULL ) return NULL;
	while ( *pwsz >= (WCHAR)'0' && *pwsz <= (WCHAR)'9' )
	{
		dw = dw * 10 + (DWORD)(*pwsz-(WCHAR)'0');
		pwsz ++;
	}
	*pdw = dw;
	return pwsz;
}

static int wchar_to_hex(WCHAR wch)
{
	if ( wch >= (WCHAR)'0' && wch <= (WCHAR)'9' )
		return (int)(wch - (WCHAR)'0');
	if ( wch >= (WCHAR)'A' && wch <= (WCHAR)'F' )
		return (int)(wch - (WCHAR)'A' + 10);
	if ( wch >= (WCHAR)'a' && wch <= (WCHAR)'f' )
		return (int)(wch - (WCHAR)'a' + 10);
	return -1;
}

static const WCHAR* get_hex(const WCHAR* pwsz,BYTE* pb)
{
	int	hi,lo;

	*pb = 0;
	if ( pwsz == NULL ) return NULL;
	hi = wchar_to_hex(*pwsz); if ( hi < 0 ) return NULL; pwsz++;
	lo = wchar_to_hex(*pwsz); if ( lo < 0 ) return NULL; pwsz++;
	*pb = (BYTE)( (hi << 4) | lo );
	return pwsz;
}

static const WCHAR* skip_hex(const WCHAR* pwsz)
{
	if ( pwsz == NULL ) return NULL;
	while ( 1 )
	{
		if ( wchar_to_hex(*pwsz) < 0 )
			break;
		pwsz++;
	}
	return pwsz;
}

static const WCHAR* next_token(const WCHAR* pwsz)
{
	if ( pwsz == NULL ) return NULL;
	pwsz = skip_space(pwsz);
	if ( *pwsz != (WCHAR)',' ) return NULL; pwsz++;
	return skip_space(pwsz);
}


static HRESULT QUARTZ_SourceTypeIsMatch(
	const BYTE* pData, DWORD cbData,
	const WCHAR* pwszTempl, DWORD cchTempl )
{
	DWORD	dwOfs;
	DWORD	n;
	DWORD	cbLen;
	const WCHAR*	pwszMask;
	const WCHAR*	pwszValue;
	BYTE	bMask;
	BYTE	bValue;

	TRACE("(%p,%u,%s,%u)\n",pData,cbData,debugstr_w(pwszTempl),cchTempl);

	pwszTempl = skip_space(pwszTempl);
	while ( 1 )
	{
		pwszTempl = get_dword(pwszTempl,&dwOfs);
		pwszTempl = next_token(pwszTempl);
		pwszTempl = get_dword(pwszTempl,&cbLen);
		pwszMask = pwszTempl = next_token(pwszTempl);
		pwszTempl = skip_hex(pwszTempl);
		pwszValue = pwszTempl = next_token(pwszTempl);
		pwszTempl = skip_hex(pwszValue);
		pwszTempl = skip_space(pwszTempl);
		if ( pwszValue == NULL )
		{
			WARN( "parse error\n" );
			return S_FALSE;
		}

		if ( dwOfs >= cbData || ( (dwOfs+cbLen) >= cbData ) )
		{
			WARN( "length of given data is too short\n" );
			return S_FALSE;
		}

		for ( n = 0; n < cbLen; n++ )
		{
			pwszMask = get_hex(pwszMask,&bMask);
			if ( pwszMask == NULL ) bMask = 0xff;
			pwszValue = get_hex(pwszValue,&bValue);
			if ( pwszValue == NULL )
			{
				WARN( "parse error - invalid hex data\n" );
				return S_FALSE;
			}
			if ( (pData[dwOfs+n]&bMask) != (bValue&bMask) )
			{
				TRACE( "not matched\n" );
				return S_FALSE;
			}
		}

		if ( *pwszTempl == 0 )
			break;
		pwszTempl = next_token(pwszTempl);
		if ( pwszTempl == NULL )
		{
			WARN( "parse error\n" );
			return S_FALSE;
		}
	}

	TRACE( "matched\n" );
	return NOERROR;
}

static HRESULT QUARTZ_GetSourceTypeFromData(
	const BYTE* pData, DWORD cbData,
	GUID* pidMajor, GUID* pidSub, CLSID* pidSource )
{
	HRESULT	hr = S_FALSE;
	LONG	lr;
	WCHAR	wszMajor[128];
	WCHAR	wszSub[128];
	WCHAR	wszSource[128];
	WCHAR	wszSourceFilter[128];
	WCHAR*	pwszLocalBuf = NULL;
	WCHAR*	pwszTemp;
	DWORD	cbLocalBuf = 0;
	DWORD	cbPath;
	DWORD	dwIndexMajor;
	HKEY	hkMajor;
	DWORD	dwIndexSub;
	HKEY	hkSub;
	DWORD	dwIndexSource;
	HKEY	hkSource;
	DWORD	dwRegType;
	DWORD	cbRegData;
	FILETIME	ftLastWrite;
	static const WCHAR wszFmt[] = {'%','l','u',0};

	if ( RegOpenKeyExW( HKEY_CLASSES_ROOT, QUARTZ_wszMediaType, 0, KEY_READ, &hkMajor ) == ERROR_SUCCESS )
	{
		dwIndexMajor = 0;
		while ( hr == S_FALSE )
		{
			cbPath = NUMELEMS(wszMajor)-1;
			lr = RegEnumKeyExW(
				hkMajor, dwIndexMajor ++, wszMajor, &cbPath,
				NULL, NULL, NULL, &ftLastWrite );
			if ( lr != ERROR_SUCCESS )
				break;
			if ( RegOpenKeyExW( hkMajor, wszMajor, 0, KEY_READ, &hkSub ) == ERROR_SUCCESS )
			{
				dwIndexSub = 0;
				while ( hr == S_FALSE )
				{
					cbPath = NUMELEMS(wszSub)-1;
					lr = RegEnumKeyExW(
						hkSub, dwIndexSub ++, wszSub, &cbPath,
						NULL, NULL, NULL, &ftLastWrite );
					if ( lr != ERROR_SUCCESS )
						break;
					if ( RegOpenKeyExW( hkSub, wszSub, 0, KEY_READ, &hkSource ) == ERROR_SUCCESS )
					{
						dwIndexSource = 0;
						while ( hr == S_FALSE )
						{
							wsprintfW(wszSource,wszFmt,dwIndexSource++);
							lr = RegQueryValueExW(
								hkSource, wszSource, NULL,
								&dwRegType, NULL, &cbRegData );
							if ( lr != ERROR_SUCCESS )
								break;
							if ( cbLocalBuf < cbRegData )
							{
								pwszTemp = (WCHAR*)QUARTZ_ReallocMem( pwszLocalBuf, cbRegData+sizeof(WCHAR) );
								if ( pwszTemp == NULL )
								{
									hr = E_OUTOFMEMORY;
									break;
								}
								pwszLocalBuf = pwszTemp;
								cbLocalBuf = cbRegData+sizeof(WCHAR);
							}
							cbRegData = cbLocalBuf;
							lr = RegQueryValueExW(
								hkSource, wszSource, NULL,
								&dwRegType, (BYTE*)pwszLocalBuf, &cbRegData );
							if ( lr != ERROR_SUCCESS )
								break;

							hr = QUARTZ_SourceTypeIsMatch(
								pData, cbData,
								pwszLocalBuf, cbRegData / sizeof(WCHAR) );
							if ( hr == S_OK )
							{
								hr = CLSIDFromString(wszMajor,pidMajor);
								if ( hr == NOERROR )
									hr = CLSIDFromString(wszSub,pidSub);
								if ( hr == NOERROR )
								{
									lstrcpyW(wszSource,QUARTZ_wszSourceFilter);
									cbRegData = NUMELEMS(wszSourceFilter)-sizeof(WCHAR);
									lr = RegQueryValueExW(
										hkSource, wszSource, NULL,
										&dwRegType,
										(BYTE*)wszSourceFilter, &cbRegData );
									if ( lr == ERROR_SUCCESS )
									{
										hr = CLSIDFromString(wszSourceFilter,pidSource);
									}
									else
										hr = E_FAIL;
								}

								if ( hr != NOERROR && SUCCEEDED(hr) )
									hr = E_FAIL;
							}
							if ( hr != S_FALSE )
								break;
						}
						RegCloseKey( hkSource );
					}
				}
				RegCloseKey( hkSub );
			}
		}
		RegCloseKey( hkMajor );
	}

	if ( pwszLocalBuf != NULL )
		QUARTZ_FreeMem(pwszLocalBuf);

	return hr;
}



/***************************************************************************
 *
 *	CFilterGraph::IFilterGraph2 methods
 *
 */

static HRESULT WINAPI
IFilterGraph2_fnQueryInterface(IFilterGraph2* iface,REFIID riid,void** ppobj)
{
	CFilterGraph_THIS(iface,fgraph);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IFilterGraph2_fnAddRef(IFilterGraph2* iface)
{
	CFilterGraph_THIS(iface,fgraph);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IFilterGraph2_fnRelease(IFilterGraph2* iface)
{
	CFilterGraph_THIS(iface,fgraph);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IFilterGraph2_fnAddFilter(IFilterGraph2* iface,IBaseFilter* pFilter, LPCWSTR pName)
{
	CFilterGraph_THIS(iface,fgraph);
	FILTER_STATE fs;
	FILTER_INFO	info;
	IBaseFilter*	pTempFilter;
	FG_FilterData*	pActiveFiltersNew;
	HRESULT	hr;
	HRESULT	hrSucceeded = S_OK;
	int i,iLen;

	TRACE( "(%p)->(%p,%s)\n",This,pFilter,debugstr_w(pName) );

	EnterCriticalSection( &This->m_csFilters );

	hr = IMediaFilter_GetState(CFilterGraph_IMediaFilter(This),0,&fs);
	if ( hr == VFW_S_STATE_INTERMEDIATE )
		hr = VFW_E_STATE_CHANGED;
	if ( fs != State_Stopped )
		hr = VFW_E_NOT_STOPPED;
	if ( FAILED(hr) )
		goto end;

	TRACE( "(%p) search the specified name.\n",This );

	if ( pName != NULL )
	{
		hr = IFilterGraph2_FindFilterByName( CFilterGraph_IFilterGraph2(This), pName, &pTempFilter );
		if ( hr == S_OK )
		{
			IBaseFilter_Release(pTempFilter);
			hrSucceeded = VFW_S_DUPLICATE_NAME;
		}
		else
		{
			goto name_ok;
		}

		iLen = lstrlenW(pName);
		if ( iLen > 32 )
			iLen = 32;
		memcpy( info.achName, pName, sizeof(WCHAR)*iLen );
		info.achName[iLen] = 0;
	}
	else
	{
		ZeroMemory( &info, sizeof(info) );
		hr = IBaseFilter_QueryFilterInfo( pFilter, &info );
		if ( FAILED(hr) )
			goto end;
		if ( info.pGraph != NULL )
		{
			IFilterGraph_Release(info.pGraph);
			hr = E_FAIL;	/* FIXME */
			goto end;
		}

		hr = IFilterGraph2_FindFilterByName( CFilterGraph_IFilterGraph2(This), pName, &pTempFilter );
		if ( hr != S_OK )
		{
			pName = info.achName;
			goto name_ok;
		}
	}

	/* generate modified names for this filter.. */
	iLen = lstrlenW(info.achName);
	if ( iLen > 32 )
		iLen = 32;
	info.achName[iLen++] = ' ';

	for ( i = 0; i <= 99; i++ )
	{
		info.achName[iLen+0] = (i%10) + '0';
		info.achName[iLen+1] = ((i/10)%10) + '0';
		info.achName[iLen+2] = 0;

		hr = IFilterGraph2_FindFilterByName( CFilterGraph_IFilterGraph2(This), info.achName, &pTempFilter );
		if ( hr != S_OK )
		{
			pName = info.achName;
			goto name_ok;
		}
	}

	hr = ( pName == NULL ) ? E_FAIL : VFW_E_DUPLICATE_NAME;
	goto end;

name_ok:
	TRACE( "(%p) add this filter - %s.\n",This,debugstr_w(pName) );

	/* register this filter. */
	pActiveFiltersNew = (FG_FilterData*)QUARTZ_ReallocMem(
		This->m_pActiveFilters,
		sizeof(FG_FilterData) * (This->m_cActiveFilters+1) );
	if ( pActiveFiltersNew == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto end;
	}
	This->m_pActiveFilters = pActiveFiltersNew;
	pActiveFiltersNew = &This->m_pActiveFilters[This->m_cActiveFilters];

	pActiveFiltersNew->pFilter = NULL;
	pActiveFiltersNew->pPosition = NULL;
	pActiveFiltersNew->pSeeking = NULL;
	pActiveFiltersNew->pwszName = NULL;
	pActiveFiltersNew->cbName = 0;

	pActiveFiltersNew->cbName = sizeof(WCHAR)*(lstrlenW(pName)+1);
	pActiveFiltersNew->pwszName = (WCHAR*)QUARTZ_AllocMem( pActiveFiltersNew->cbName );
	if ( pActiveFiltersNew->pwszName == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto end;
	}
	memcpy( pActiveFiltersNew->pwszName, pName, pActiveFiltersNew->cbName );

	pActiveFiltersNew->pFilter = pFilter;
	IBaseFilter_AddRef(pFilter);
	IBaseFilter_QueryInterface( pFilter, &IID_IMediaPosition, (void**)&pActiveFiltersNew->pPosition );
	IBaseFilter_QueryInterface( pFilter, &IID_IMediaSeeking, (void**)&pActiveFiltersNew->pSeeking );

	This->m_cActiveFilters ++;

	hr = IBaseFilter_JoinFilterGraph(pFilter,(IFilterGraph*)iface,pName);
	if ( SUCCEEDED(hr) )
	{
		EnterCriticalSection( &This->m_csClock );
		hr = IBaseFilter_SetSyncSource( pFilter, This->m_pClock );
		LeaveCriticalSection( &This->m_csClock );
	}
	if ( FAILED(hr) )
	{
		IBaseFilter_JoinFilterGraph(pFilter,NULL,pName);
		IFilterGraph2_RemoveFilter(CFilterGraph_IFilterGraph2(This),pFilter);
		goto end;
	}

	hr = CFilterGraph_GraphChanged(This);
	if ( FAILED(hr) )
		goto end;

	hr = hrSucceeded;
end:
	LeaveCriticalSection( &This->m_csFilters );

	TRACE( "(%p) return %08x\n", This, hr );

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnRemoveFilter(IFilterGraph2* iface,IBaseFilter* pFilter)
{
	CFilterGraph_THIS(iface,fgraph);
	FILTER_STATE fs;
	DWORD	n,copy;
	HRESULT	hr = NOERROR;

	TRACE( "(%p)->(%p)\n",This,pFilter );

	EnterCriticalSection( &This->m_csFilters );

	hr = IMediaFilter_GetState(CFilterGraph_IMediaFilter(This),0,&fs);
	if ( hr == VFW_S_STATE_INTERMEDIATE )
		hr = VFW_E_STATE_CHANGED;
	if ( fs != State_Stopped )
		hr = VFW_E_NOT_STOPPED;
	if ( FAILED(hr) )
		goto end;

	hr = S_FALSE; /* FIXME? */

	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		if ( This->m_pActiveFilters[n].pFilter == pFilter )
		{
			CFilterGraph_DisconnectAllPins(pFilter);
			(void)IBaseFilter_SetSyncSource( pFilter, NULL );
			(void)IBaseFilter_JoinFilterGraph(
				pFilter, NULL, This->m_pActiveFilters[n].pwszName );

			if ( This->m_pActiveFilters[n].pFilter != NULL )
				IBaseFilter_Release(This->m_pActiveFilters[n].pFilter);
			if ( This->m_pActiveFilters[n].pPosition != NULL )
				IMediaPosition_Release(This->m_pActiveFilters[n].pPosition);
			if ( This->m_pActiveFilters[n].pSeeking != NULL )
				IMediaSeeking_Release(This->m_pActiveFilters[n].pSeeking);
			if ( This->m_pActiveFilters[n].pwszName != NULL )
				QUARTZ_FreeMem(This->m_pActiveFilters[n].pwszName);

			copy = This->m_cActiveFilters - n - 1;
			if ( copy > 0 )
				memmove( &This->m_pActiveFilters[n],
						 &This->m_pActiveFilters[n+1],
						 sizeof(FG_FilterData) * copy );
			This->m_cActiveFilters --;

			hr = CFilterGraph_GraphChanged(This);
			break;
		}
	}

end:;
	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnEnumFilters(IFilterGraph2* iface,IEnumFilters** ppEnum)
{
	CFilterGraph_THIS(iface,fgraph);
	QUARTZ_CompList*	pList = NULL;
	DWORD	n;
	HRESULT	hr;

	TRACE( "(%p)->(%p)\n",This,ppEnum );

	EnterCriticalSection( &This->m_csFilters );

	pList = QUARTZ_CompList_Alloc();
	if ( pList == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto err;
	}
	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		hr = QUARTZ_CompList_AddTailComp(
			pList, (IUnknown*)This->m_pActiveFilters[n].pFilter, NULL, 0 );
		if ( FAILED(hr) )
			goto err;
	}

	hr = QUARTZ_CreateEnumUnknown(
		&IID_IEnumFilters, (void**)ppEnum, pList );
err:
	if ( pList != NULL )
		QUARTZ_CompList_Free( pList );

	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnFindFilterByName(IFilterGraph2* iface,LPCWSTR pName,IBaseFilter** ppFilter)
{
	CFilterGraph_THIS(iface,fgraph);
	DWORD	n;
	DWORD	cbName;
	HRESULT	hr = E_FAIL;	/* FIXME */

	TRACE( "(%p)->(%s,%p)\n",This,debugstr_w(pName),ppFilter );

	if ( pName == NULL || ppFilter == NULL )
		return E_POINTER;
	*ppFilter = NULL;

	cbName = sizeof(WCHAR) * (lstrlenW(pName) + 1);

	EnterCriticalSection( &This->m_csFilters );

	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		if ( This->m_pActiveFilters[n].cbName == cbName &&
			 !memcmp( This->m_pActiveFilters[n].pwszName, pName, cbName ) )
		{
			*ppFilter = This->m_pActiveFilters[n].pFilter;
			IBaseFilter_AddRef(*ppFilter);
			hr = NOERROR;
		}
	}

	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnConnectDirect(IFilterGraph2* iface,IPin* pOut,IPin* pIn,const AM_MEDIA_TYPE* pmt)
{
	CFilterGraph_THIS(iface,fgraph);
	IPin*	pConnTo;
	PIN_INFO	infoIn;
	PIN_INFO	infoOut;
	FILTER_INFO	finfoIn;
	FILTER_INFO	finfoOut;
	FILTER_STATE	fs;
	HRESULT	hr;

	TRACE( "(%p)->(%p,%p,%p)\n",This,pOut,pIn,pmt );

	infoIn.pFilter = NULL;
	infoOut.pFilter = NULL;
	finfoIn.pGraph = NULL;
	finfoOut.pGraph = NULL;

	EnterCriticalSection( &This->m_csFilters );

	hr = IMediaFilter_GetState(CFilterGraph_IMediaFilter(This),0,&fs);
	if ( hr == VFW_S_STATE_INTERMEDIATE )
		hr = VFW_E_STATE_CHANGED;
	if ( fs != State_Stopped )
		hr = VFW_E_NOT_STOPPED;
	if ( FAILED(hr) )
		goto end;

	hr = IPin_QueryPinInfo(pIn,&infoIn);
	if ( FAILED(hr) )
		goto end;
	hr = IPin_QueryPinInfo(pOut,&infoOut);
	if ( FAILED(hr) )
		goto end;
	if ( infoIn.pFilter == NULL || infoOut.pFilter == NULL ||
		 infoIn.dir != PINDIR_INPUT || infoOut.dir != PINDIR_OUTPUT )
	{
		hr = E_FAIL;
		goto end;
	}

	hr = IBaseFilter_QueryFilterInfo(infoIn.pFilter,&finfoIn);
	if ( FAILED(hr) )
		goto end;
	hr = IBaseFilter_QueryFilterInfo(infoOut.pFilter,&finfoOut);
	if ( FAILED(hr) )
		goto end;
	if ( finfoIn.pGraph != ((IFilterGraph*)iface) ||
		 finfoOut.pGraph != ((IFilterGraph*)iface) )
	{
		hr = E_FAIL;
		goto end;
	}

	pConnTo = NULL;
	hr = IPin_ConnectedTo(pIn,&pConnTo);
	if ( hr == NOERROR && pConnTo != NULL )
	{
		IPin_Release(pConnTo);
		hr = VFW_E_ALREADY_CONNECTED;
		goto end;
	}

	pConnTo = NULL;
	hr = IPin_ConnectedTo(pOut,&pConnTo);
	if ( hr == NOERROR && pConnTo != NULL )
	{
		IPin_Release(pConnTo);
		hr = VFW_E_ALREADY_CONNECTED;
		goto end;
	}

	TRACE("(%p) try to connect %p<->%p\n",This,pIn,pOut);
	hr = IPin_Connect(pOut,pIn,pmt);
	if ( FAILED(hr) )
	{
		TRACE("(%p)->Connect(%p,%p) hr = %08x\n",pOut,pIn,pmt,hr);
		IPin_Disconnect(pOut);
		IPin_Disconnect(pIn);
		goto end;
	}

	hr = CFilterGraph_GraphChanged(This);
	if ( FAILED(hr) )
		goto end;

end:
	LeaveCriticalSection( &This->m_csFilters );

	if ( infoIn.pFilter != NULL )
		IBaseFilter_Release(infoIn.pFilter);
	if ( infoOut.pFilter != NULL )
		IBaseFilter_Release(infoOut.pFilter);
	if ( finfoIn.pGraph != NULL )
		IFilterGraph_Release(finfoIn.pGraph);
	if ( finfoOut.pGraph != NULL )
		IFilterGraph_Release(finfoOut.pGraph);

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnReconnect(IFilterGraph2* iface,IPin* pPin)
{
	CFilterGraph_THIS(iface,fgraph);

	TRACE( "(%p)->(%p)\n",This,pPin );

	return IFilterGraph2_ReconnectEx(iface,pPin,NULL);
}

static HRESULT WINAPI
IFilterGraph2_fnDisconnect(IFilterGraph2* iface,IPin* pPin)
{
	CFilterGraph_THIS(iface,fgraph);
	IPin* pConnTo;
	HRESULT hr;

	TRACE( "(%p)->(%p)\n",This,pPin );

	EnterCriticalSection( &This->m_csFilters );

	pConnTo = NULL;
	hr = IPin_ConnectedTo(pPin,&pConnTo);
	if ( hr == NOERROR && pConnTo != NULL )
	{
		IPin_Disconnect(pConnTo);
		IPin_Release(pConnTo);
	}
	hr = IPin_Disconnect(pPin);
	if ( FAILED(hr) )
		goto end;

	hr = CFilterGraph_GraphChanged(This);
	if ( FAILED(hr) )
		goto end;

end:
	LeaveCriticalSection( &This->m_csFilters );

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnSetDefaultSyncSource(IFilterGraph2* iface)
{
	CFilterGraph_THIS(iface,fgraph);
	IUnknown* punk;
	IReferenceClock* pClock = NULL;
	HRESULT hr = E_NOINTERFACE;
	DWORD	n;

	FIXME( "(%p)->() stub!\n", This );

	/* FIXME - search all filters from *renderer*, not like this. */
	EnterCriticalSection( &This->m_csFilters );

	for ( n = 0; n < This->m_cActiveFilters; n++ )
	{
		hr = IBaseFilter_QueryInterface( This->m_pActiveFilters[n].pFilter, &IID_IReferenceClock, (void**)&pClock );
		if ( SUCCEEDED(hr) )
			break;
	}

	LeaveCriticalSection( &This->m_csFilters );

	if ( FAILED(hr) )
	{
		TRACE("using system clock\n");
		hr = QUARTZ_CreateSystemClock( NULL, (void**)&punk );
		if ( FAILED(hr) )
			return hr;
		hr = IUnknown_QueryInterface( punk, &IID_IReferenceClock, (void**)&pClock );	IUnknown_Release( punk );
		if ( FAILED(hr) )
			return hr;
	}

	hr = IMediaFilter_SetSyncSource(
		CFilterGraph_IMediaFilter(This), pClock );
	IReferenceClock_Release( pClock );
	This->m_defClock = TRUE;

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnConnect(IFilterGraph2* iface,IPin* pOut,IPin* pIn)
{
	CFilterGraph_THIS(iface,fgraph);
	IFilterMapper2*	pMap2 = NULL;
	IEnumMoniker*	pEnumMon = NULL;
	IMoniker*	pMon = NULL;
	IBaseFilter*	pFilter = NULL;
	IEnumPins*	pEnumPin = NULL;
	IPin*	pPinTry = NULL;
	PIN_INFO	info;
	PIN_DIRECTION	pindir;
	ULONG	cReturned;
	BOOL	bTryConnect;
	BOOL	bConnected = FALSE;
	CLSID	clsidOutFilter;
	CLSID	clsidInFilter;
	CLSID	clsid;
	HRESULT	hr;

	TRACE( "(%p)->(%p,%p)\n",This,pOut,pIn );

	/* At first, try to connect directly. */
	hr = IFilterGraph_ConnectDirect(iface,pOut,pIn,NULL);
	if ( hr == NOERROR )
		return NOERROR;

	/* FIXME - try to connect indirectly. */
	FIXME( "(%p)->(%p,%p) not fully implemented\n",This,pOut,pIn );

	info.pFilter = NULL;
	hr = IPin_QueryPinInfo(pOut,&info);
	if ( FAILED(hr) )
		return hr;
	if ( info.pFilter == NULL )
		return E_FAIL;
	hr = IBaseFilter_GetClassID(info.pFilter,&clsidOutFilter);
	IBaseFilter_Release(info.pFilter);
	if ( FAILED(hr) )
		return hr;

	info.pFilter = NULL;
	hr = IPin_QueryPinInfo(pIn,&info);
	if ( FAILED(hr) )
		return hr;
	if ( info.pFilter == NULL )
		return E_FAIL;
	hr = IBaseFilter_GetClassID(info.pFilter,&clsidInFilter);
	IBaseFilter_Release(info.pFilter);
	if ( FAILED(hr) )
		return hr;

	/* FIXME - try to connect with unused filters. */
	/* FIXME - try to connect with cached filters. */
	/* FIXME - enumerate transform filters and try to connect */
	hr = CoCreateInstance(
		&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
		&IID_IFilterMapper2, (void**)&pMap2 );
	if ( FAILED(hr) )
		return hr;
	hr = IFilterMapper2_EnumMatchingFilters(
		pMap2,&pEnumMon,0,FALSE,MERIT_DO_NOT_USE+1,
		TRUE,0,NULL,NULL,NULL,FALSE,
		TRUE,0,NULL,NULL,NULL);
	IFilterMapper2_Release(pMap2);
	if ( FAILED(hr) )
		return hr;
	TRACE("try to connect indirectly\n");

	if ( hr == S_OK )
	{
		while ( !bConnected && hr == S_OK )
		{
			hr = IEnumMoniker_Next(pEnumMon,1,&pMon,&cReturned);
			if ( hr != S_OK )
				break;
			hr = IMoniker_BindToObject(pMon,NULL,NULL,&IID_IBaseFilter,(void**)&pFilter );
			if ( hr == S_OK )
			{
				hr = IBaseFilter_GetClassID(pFilter,&clsid);
				if ( hr == S_OK &&
					 ( IsEqualGUID(&clsidOutFilter,&clsid) || IsEqualGUID(&clsidInFilter,&clsid) ) )
					hr = S_FALSE;
				else
					hr = IFilterGraph2_AddFilter(iface,pFilter,NULL);
				if ( hr == S_OK )
				{
					bTryConnect = FALSE;
					hr = IBaseFilter_EnumPins(pFilter,&pEnumPin);
					if ( hr == S_OK )
					{
						{
							while ( !bTryConnect )
							{
								hr = IEnumPins_Next(pEnumPin,1,&pPinTry,&cReturned);
								if ( hr != S_OK )
									break;
								hr = IPin_QueryDirection(pPinTry,&pindir);
								if ( hr == S_OK && pindir == PINDIR_INPUT )
								{
									/* try to connect directly. */
									hr = IFilterGraph2_ConnectDirect(iface,pOut,pPinTry,NULL);
									if ( hr == S_OK )
										bTryConnect = TRUE;
									hr = S_OK;
								}
								IPin_Release(pPinTry); pPinTry = NULL;
							}
						}
						IEnumPins_Release(pEnumPin); pEnumPin = NULL;
					}
					TRACE("TryConnect %d\n",bTryConnect);

					if ( bTryConnect )
					{
						hr = IBaseFilter_EnumPins(pFilter,&pEnumPin);
						if ( hr == S_OK )
						{
							while ( !bConnected )
							{
								hr = IEnumPins_Next(pEnumPin,1,&pPinTry,&cReturned);
								if ( hr != S_OK )
									break;
								hr = IPin_QueryDirection(pPinTry,&pindir);
								if ( hr == S_OK && pindir == PINDIR_OUTPUT )
								{
									/* try to connect indirectly. */
									hr = IFilterGraph2_Connect(iface,pPinTry,pIn);
									if ( hr == S_OK )
										bConnected = TRUE;
									hr = S_OK;
								}
								IPin_Release(pPinTry); pPinTry = NULL;
							}
							IEnumPins_Release(pEnumPin); pEnumPin = NULL;
						}
					}
					if ( !bConnected )
						hr = IFilterGraph2_RemoveFilter(iface,pFilter);
				}
				IBaseFilter_Release(pFilter); pFilter = NULL;
				if ( SUCCEEDED(hr) )
					hr = S_OK;
			}
			else
			{
				hr = S_OK;
			}
			IMoniker_Release(pMon); pMon = NULL;
		}
		IEnumMoniker_Release(pEnumMon); pEnumMon = NULL;
	}

	if ( SUCCEEDED(hr) && !bConnected )
		hr = VFW_E_CANNOT_CONNECT;

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnRender(IFilterGraph2* iface,IPin* pOut)
{
	CFilterGraph_THIS(iface,fgraph);

	TRACE( "(%p)->(%p)\n",This,pOut);

	return IFilterGraph2_RenderEx( CFilterGraph_IFilterGraph2(This), pOut, 0, NULL );
}

static HRESULT WINAPI
IFilterGraph2_fnRenderFile(IFilterGraph2* iface,LPCWSTR lpFileName,LPCWSTR lpPlayList)
{
	CFilterGraph_THIS(iface,fgraph);
	HRESULT	hr;
	IBaseFilter*	pFilter = NULL;
	IEnumPins*	pEnum = NULL;
	IPin*	pPin;
	ULONG	cFetched;
	PIN_DIRECTION	dir;
	ULONG	cTryToRender;
	ULONG	cActRender;

	TRACE( "(%p)->(%s,%s)\n",This,
		debugstr_w(lpFileName),debugstr_w(lpPlayList) );

	if ( lpPlayList != NULL )
		return E_INVALIDARG;

	pFilter = NULL;
	hr = IFilterGraph2_AddSourceFilter(iface,lpFileName,NULL,&pFilter);
	if ( FAILED(hr) )
		goto end;
	if ( pFilter == NULL )
	{
		hr = E_FAIL;
		goto end;
	}
	TRACE("(%p) source filter %p\n",This,pFilter);

	pEnum = NULL;
	hr = IBaseFilter_EnumPins( pFilter, &pEnum );
	if ( FAILED(hr) )
		goto end;
	if ( pEnum == NULL )
	{
		hr = E_FAIL;
		goto end;
	}

	cTryToRender = 0;
	cActRender = 0;

	while ( 1 )
	{
		pPin = NULL;
		cFetched = 0;
		hr = IEnumPins_Next( pEnum, 1, &pPin, &cFetched );
		if ( FAILED(hr) )
			goto end;
		if ( hr != NOERROR || pPin == NULL || cFetched != 1 )
		{
			hr = NOERROR;
			break;
		}
		hr = IPin_QueryDirection( pPin, &dir );
		if ( hr == NOERROR && dir == PINDIR_OUTPUT )
		{
			cTryToRender ++;
			hr = IFilterGraph2_Render( iface, pPin );
			if ( hr == NOERROR )
				cActRender ++;
		}
		IPin_Release( pPin );
	}

	if ( hr == NOERROR )
	{
		if ( cTryToRender > cActRender )
			hr = VFW_S_PARTIAL_RENDER;
		if ( cActRender == 0 )
			hr = E_FAIL;
	}

end:
	if ( pEnum != NULL )
		IEnumPins_Release( pEnum );
	if ( pFilter != NULL )
		IBaseFilter_Release( pFilter );

	return hr;
}

static HRESULT WINAPI
IFilterGraph2_fnAddSourceFilter(IFilterGraph2* iface,LPCWSTR lpFileName,LPCWSTR lpFilterName,IBaseFilter** ppBaseFilter)
{
	CFilterGraph_THIS(iface,fgraph);
	HRESULT	hr;
	BYTE	bStartData[512];
	DWORD	cbStartData;
	AM_MEDIA_TYPE	mt;
	CLSID	clsidSource;
	IFileSourceFilter*	pSource;

	TRACE( "(%p)->(%s,%s,%p)\n",This,
		debugstr_w(lpFileName),debugstr_w(lpFilterName),ppBaseFilter );

	if ( lpFileName == NULL || ppBaseFilter == NULL )
		return E_POINTER;
	*ppBaseFilter = NULL;

	hr = QUARTZ_PeekFile( lpFileName, bStartData, 512, &cbStartData );
	if ( FAILED(hr) )
	{
		FIXME("cannot open %s (NOTE: URL is not implemented)\n", debugstr_w(lpFileName));
		return hr;
	}
	ZeroMemory( &mt, sizeof(AM_MEDIA_TYPE) );
	mt.bFixedSizeSamples = 1;
	mt.lSampleSize = 1;
	memcpy( &mt.majortype, &MEDIATYPE_Stream, sizeof(GUID) );
	memcpy( &mt.subtype, &MEDIASUBTYPE_NULL, sizeof(GUID) );
	memcpy( &mt.formattype, &FORMAT_None, sizeof(GUID) );
	hr = QUARTZ_GetSourceTypeFromData(
		bStartData, cbStartData,
		&mt.majortype, &mt.subtype, &clsidSource );
	if ( FAILED(hr) )
	{
		ERR("QUARTZ_GetSourceTypeFromData() failed - return %08x\n",hr);
		return hr;
	}
	if ( hr != S_OK )
	{
		FIXME( "file %s - unknown format\n", debugstr_w(lpFileName) );
		return VFW_E_INVALID_FILE_FORMAT;
	}

	hr = CoCreateInstance(
		&clsidSource, NULL, CLSCTX_INPROC_SERVER,
		&IID_IBaseFilter, (void**)ppBaseFilter );
	if ( FAILED(hr) )
		return hr;
	hr = IBaseFilter_QueryInterface(*ppBaseFilter,&IID_IFileSourceFilter,(void**)&pSource);
	if ( SUCCEEDED(hr) )
	{
		hr = IFileSourceFilter_Load(pSource,lpFileName,&mt);
		IFileSourceFilter_Release(pSource);
	}
	if ( SUCCEEDED(hr) )
		hr = IFilterGraph2_AddFilter(iface,*ppBaseFilter,lpFilterName);
	if ( FAILED(hr) )
	{
		IBaseFilter_Release(*ppBaseFilter);
		*ppBaseFilter = NULL;
		return hr;
	}

	return S_OK;
}

static HRESULT WINAPI
IFilterGraph2_fnSetLogFile(IFilterGraph2* iface,DWORD_PTR hFile)
{
	CFilterGraph_THIS(iface,fgraph);

	FIXME( "(%p)->() stub!\n", This );

	return S_OK;	/* no debug output */
}

static HRESULT WINAPI
IFilterGraph2_fnAbort(IFilterGraph2* iface)
{
	CFilterGraph_THIS(iface,fgraph);

	FIXME( "(%p)->() stub!\n", This );

	/* FIXME - abort the current asynchronous task. */

	return S_OK;
}

static HRESULT WINAPI
IFilterGraph2_fnShouldOperationContinue(IFilterGraph2* iface)
{
	CFilterGraph_THIS(iface,fgraph);

	FIXME( "(%p)->() stub!\n", This );

	return E_NOTIMPL;
}

static HRESULT WINAPI
IFilterGraph2_fnAddSourceFilterForMoniker(IFilterGraph2* iface,IMoniker* pMon,IBindCtx* pCtx,LPCWSTR pFilterName,IBaseFilter** ppFilter)
{
	CFilterGraph_THIS(iface,fgraph);

	FIXME( "(%p)->() stub!\n", This );
	return E_NOTIMPL;
}

static HRESULT WINAPI
IFilterGraph2_fnReconnectEx(IFilterGraph2* iface,IPin* pPin,const AM_MEDIA_TYPE* pmt)
{
	CFilterGraph_THIS(iface,fgraph);
	HRESULT hr;

	FIXME( "(%p)->(%p,%p) stub!\n",This,pPin,pmt );

	/* reconnect asynchronously. */

	EnterCriticalSection( &This->m_csFilters );
	hr = CFilterGraph_GraphChanged(This);
	LeaveCriticalSection( &This->m_csFilters );

	return E_NOTIMPL;
}

static HRESULT QUARTZ_RenderNextPin(IFilterGraph2* iface,IPin* pIn)
{
	HRESULT	hr, hr2 = S_OK;
	IPin*		pPin;
	IPin*		pConnTo;
	PIN_INFO	info;
	IEnumPins*	pEnumPin = NULL;
	PIN_DIRECTION	pindir;
	ULONG	cReturned;

	/* find the filter connected to the input pin */
	hr = IPin_QueryPinInfo(pIn, &info);
	if ( hr == S_OK ) {
		/* render all output pins of filter */
		hr = IBaseFilter_EnumPins(info.pFilter,&pEnumPin);
		if ( hr == S_OK )
		{
			while ( TRUE )
			{
				hr = IEnumPins_Next(pEnumPin,1,&pPin,&cReturned);
				if ( hr != S_OK )
					break;
				hr = IPin_QueryDirection(pPin,&pindir);
				if ( hr == S_OK && pindir == PINDIR_OUTPUT )
				{
					/* is this output pin connected? */
					hr = IPin_ConnectedTo(pPin,&pConnTo);
					if ( hr == NOERROR && pConnTo != NULL )
					{
						/* yes, see if whatever it's connected to needs rendering */
						hr = QUARTZ_RenderNextPin(iface, pConnTo);
						IPin_Release(pConnTo); pConnTo = NULL;
					}
					else
					{
						/* not connected, we'll try to render this pin */
						hr = IFilterGraph2_RenderEx(iface, pPin, 0, NULL);
					}
					if (hr != S_OK)
						hr2 = VFW_S_PARTIAL_RENDER;
				}
				IPin_Release(pPin); pPin = NULL;
				if (FAILED(hr)) {
					hr2 = hr;
					break;
				}
			}
			IEnumPins_Release(pEnumPin); pEnumPin = NULL;
		}
		else hr2 = hr;
		IBaseFilter_Release(info.pFilter);
	}
	else hr2 = hr;
	return hr2;
}


static HRESULT WINAPI
IFilterGraph2_fnRenderEx(IFilterGraph2* iface,IPin* pOut,DWORD dwFlags,DWORD* pdwReserved)
{
	CFilterGraph_THIS(iface,fgraph);
	HRESULT	hr;
	IFilterMapper2*	pMap2 = NULL;
	IEnumMoniker*	pEnumMon = NULL;
	IMoniker*	pMon = NULL;
	IBaseFilter*	pFilter = NULL;
	IEnumPins*	pEnumPin = NULL;
	IPin*	pPin = NULL;
	PIN_DIRECTION	pindir;
	BOOL	bRendered = FALSE;
	ULONG	cReturned;

	TRACE( "(%p)->(%p,%08x,%p)\n",This,pOut,dwFlags,pdwReserved);

	if ( pdwReserved != NULL )
		return E_INVALIDARG;

	if ( dwFlags != 0 )
	{
		FIXME( "dwFlags != 0 (0x%08x)\n", dwFlags );
		return E_INVALIDARG;
	}

	/* FIXME - must be locked */
	/*EnterCriticalSection( &This->m_csFilters );*/

	if ( pOut == NULL )
		return E_POINTER;

	hr = CoCreateInstance(
		&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
		&IID_IFilterMapper2, (void**)&pMap2 );
	if ( FAILED(hr) )
		return hr;
	hr = IFilterMapper2_EnumMatchingFilters(
		pMap2,&pEnumMon,0,FALSE,MERIT_DO_NOT_USE+1,
		TRUE,0,NULL,NULL,NULL,TRUE,
		FALSE,0,NULL,NULL,NULL);
	IFilterMapper2_Release(pMap2);
	if ( FAILED(hr) )
		return hr;
	TRACE("try to render pin\n");

	if ( hr == S_OK )
	{
		/* try to render pin. */
		while ( !bRendered && hr == S_OK )
		{
			hr = IEnumMoniker_Next(pEnumMon,1,&pMon,&cReturned);
			if ( hr != S_OK )
				break;
			hr = IMoniker_BindToObject(pMon,NULL,NULL,&IID_IBaseFilter,(void**)&pFilter );
			if ( hr == S_OK )
			{
				hr = IFilterGraph2_AddFilter(iface,pFilter,NULL);
				if ( hr == S_OK )
				{
					hr = IBaseFilter_EnumPins(pFilter,&pEnumPin);
					if ( hr == S_OK )
					{
						while ( !bRendered )
						{
							hr = IEnumPins_Next(pEnumPin,1,&pPin,&cReturned);
							if ( hr != S_OK )
								break;
							hr = IPin_QueryDirection(pPin,&pindir);
							if ( hr == S_OK && pindir == PINDIR_INPUT )
							{
								/* try to connect. */
								hr = IFilterGraph2_Connect(iface,pOut,pPin);
								if ( hr == S_OK )
									bRendered = TRUE;
								hr = S_OK;
							}
							IPin_Release(pPin); pPin = NULL;
						}
						IEnumPins_Release(pEnumPin); pEnumPin = NULL;
					}
					if ( !bRendered )
						hr = IFilterGraph2_RemoveFilter(iface,pFilter);
				}
				IBaseFilter_Release(pFilter); pFilter = NULL;
				if ( SUCCEEDED(hr) )
					hr = S_OK;
			}
			else
			{
				hr = S_OK;
			}
			IMoniker_Release(pMon); pMon = NULL;
		}
		IEnumMoniker_Release(pEnumMon); pEnumMon = NULL;
	}

	if ( bRendered )
	{
		IPin*		pConnTo;

		TRACE("pin is rendered\n");
		This->m_cRenders++;

		/* successfully rendered(but may be partial now) */
		hr = S_OK;

		/* try to render all inserted filters */
		/* start with the filter our output pin is connected to now */
		hr = IPin_ConnectedTo(pOut,&pConnTo);
		if ( hr == NOERROR && pConnTo != NULL )
		{
			hr = QUARTZ_RenderNextPin(iface, pConnTo);
			IPin_Release(pConnTo); pConnTo = NULL;
		}
		if ( FAILED(hr) )
			hr = VFW_S_PARTIAL_RENDER;
	}
	else
	{
		if ( SUCCEEDED(hr) )
			hr = VFW_E_CANNOT_RENDER;
	}

	/*LeaveCriticalSection( &This->m_csFilters );*/

	TRACE("returns %08x\n", hr);

	return hr;
}




static ICOM_VTABLE(IFilterGraph2) ifgraph =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IFilterGraph2_fnQueryInterface,
	IFilterGraph2_fnAddRef,
	IFilterGraph2_fnRelease,
	/* IFilterGraph fields */
	IFilterGraph2_fnAddFilter,
	IFilterGraph2_fnRemoveFilter,
	IFilterGraph2_fnEnumFilters,
	IFilterGraph2_fnFindFilterByName,
	IFilterGraph2_fnConnectDirect,
	IFilterGraph2_fnReconnect,
	IFilterGraph2_fnDisconnect,
	IFilterGraph2_fnSetDefaultSyncSource,
	/* IGraphBuilder fields */
	IFilterGraph2_fnConnect,
	IFilterGraph2_fnRender,
	IFilterGraph2_fnRenderFile,
	IFilterGraph2_fnAddSourceFilter,
	IFilterGraph2_fnSetLogFile,
	IFilterGraph2_fnAbort,
	IFilterGraph2_fnShouldOperationContinue,
	/* IFilterGraph2 fields */
	IFilterGraph2_fnAddSourceFilterForMoniker,
	IFilterGraph2_fnReconnectEx,
	IFilterGraph2_fnRenderEx,
};

HRESULT CFilterGraph_InitIFilterGraph2( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
	ICOM_VTBL(&pfg->fgraph) = &ifgraph;

	CRITICAL_SECTION_DEFINE( &pfg->m_csFilters );
	pfg->m_cActiveFilters = 0;
	pfg->m_pActiveFilters = NULL;
	pfg->m_cRenders = 0;
	pfg->m_cCompletes = 0;

	return NOERROR;
}

void CFilterGraph_UninitIFilterGraph2( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);

	/* remove all filters... */
	while ( pfg->m_cActiveFilters > 0 )
	{
		IFilterGraph2_RemoveFilter(
			CFilterGraph_IFilterGraph2(pfg),
			pfg->m_pActiveFilters[pfg->m_cActiveFilters-1].pFilter );
	}

	if ( pfg->m_pActiveFilters != NULL )
		QUARTZ_FreeMem( pfg->m_pActiveFilters );

	DeleteCriticalSection( &pfg->m_csFilters );
}

/***************************************************************************
 *
 *	CFilterGraph::IGraphVersion methods
 *
 */

static HRESULT WINAPI
IGraphVersion_fnQueryInterface(IGraphVersion* iface,REFIID riid,void** ppobj)
{
	CFilterGraph_THIS(iface,graphversion);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IGraphVersion_fnAddRef(IGraphVersion* iface)
{
	CFilterGraph_THIS(iface,graphversion);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IGraphVersion_fnRelease(IGraphVersion* iface)
{
	CFilterGraph_THIS(iface,graphversion);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}


static HRESULT WINAPI
IGraphVersion_fnQueryVersion(IGraphVersion* iface,LONG* plVersion)
{
	CFilterGraph_THIS(iface,graphversion);

	TRACE("(%p)->(%p)\n",This,plVersion);

	if ( plVersion == NULL )
		return E_POINTER;

	EnterCriticalSection( &This->m_csFilters );
	*plVersion = This->m_lGraphVersion;
	LeaveCriticalSection( &This->m_csFilters );

	return NOERROR;
}


static ICOM_VTABLE(IGraphVersion) igraphversion =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IGraphVersion_fnQueryInterface,
	IGraphVersion_fnAddRef,
	IGraphVersion_fnRelease,
	/* IGraphVersion fields */
	IGraphVersion_fnQueryVersion,
};



HRESULT CFilterGraph_InitIGraphVersion( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
	ICOM_VTBL(&pfg->graphversion) = &igraphversion;

	pfg->m_lGraphVersion = 1;

	return NOERROR;
}

void CFilterGraph_UninitIGraphVersion( CFilterGraph* pfg )
{
	TRACE("(%p)\n",pfg);
}


