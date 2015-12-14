/*
 * Implements IEnumMediaTypes and helper functions. (internal)
 *
 * Copyright (C) Hidenori TAKESHIMA <hidenori@a2.ctktv.ne.jp>
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
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
#include "mmsystem.h"
#include "strmif.h"
#include "vfwmsgs.h"
#include "uuids.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "mtype.h"
#include "iunk.h"


/****************************************************************************/



HRESULT QUARTZ_MediaType_Copy(
	AM_MEDIA_TYPE* pmtDst,
	const AM_MEDIA_TYPE* pmtSrc )
{
	memcpy( &pmtDst->majortype, &pmtSrc->majortype, sizeof(GUID) );
	memcpy( &pmtDst->subtype, &pmtSrc->subtype, sizeof(GUID) );
	pmtDst->bFixedSizeSamples = pmtSrc->bFixedSizeSamples;
	pmtDst->bTemporalCompression = pmtSrc->bTemporalCompression;
	pmtDst->lSampleSize = pmtSrc->lSampleSize;
	memcpy( &pmtDst->formattype, &pmtSrc->formattype, sizeof(GUID) );
	pmtDst->pUnk = NULL;
	pmtDst->cbFormat = pmtSrc->cbFormat;
	pmtDst->pbFormat = NULL;

	if ( pmtSrc->pbFormat != NULL && pmtSrc->cbFormat != 0 )
	{
		pmtDst->pbFormat = (BYTE*)CoTaskMemAlloc( pmtSrc->cbFormat );
		if ( pmtDst->pbFormat == NULL )
		{
			return E_OUTOFMEMORY;
		}
		memcpy( pmtDst->pbFormat, pmtSrc->pbFormat, pmtSrc->cbFormat );
	}

	if ( pmtSrc->pUnk != NULL )
	{
		pmtDst->pUnk = pmtSrc->pUnk;
		IUnknown_AddRef( pmtSrc->pUnk );
	}

	return S_OK;
}

void QUARTZ_MediaType_Free(
	AM_MEDIA_TYPE* pmt )
{
	if ( pmt->pUnk != NULL )
	{
		IUnknown_Release( pmt->pUnk );
		pmt->pUnk = NULL;
	}
	if ( pmt->pbFormat != NULL )
	{
		CoTaskMemFree( pmt->pbFormat );
		pmt->cbFormat = 0;
		pmt->pbFormat = NULL;
	}
}

AM_MEDIA_TYPE* QUARTZ_MediaType_Duplicate(
	const AM_MEDIA_TYPE* pmtSrc )
{
	AM_MEDIA_TYPE*	pmtDup;

	pmtDup = (AM_MEDIA_TYPE*)CoTaskMemAlloc( sizeof(AM_MEDIA_TYPE) );
	if ( pmtDup == NULL )
		return NULL;
	if ( QUARTZ_MediaType_Copy( pmtDup, pmtSrc ) != S_OK )
	{
		CoTaskMemFree( pmtDup );
		return NULL;
	}

	return pmtDup;
}

void QUARTZ_MediaType_Destroy(
	AM_MEDIA_TYPE* pmt )
{
	QUARTZ_MediaType_Free( pmt );
	CoTaskMemFree( pmt );
}

void QUARTZ_MediaSubType_FromFourCC(
	GUID* psubtype, DWORD dwFourCC )
{
	TRACE( "FourCC %c%c%c%c\n",
			(int)(dwFourCC>> 0)&0xff,
			(int)(dwFourCC>> 8)&0xff,
			(int)(dwFourCC>>16)&0xff,
			(int)(dwFourCC>>24)&0xff );
	memcpy( psubtype, &MEDIASUBTYPE_PCM, sizeof(GUID) );
	psubtype->Data1 = dwFourCC;
}

BOOL QUARTZ_MediaSubType_IsFourCC(
	const GUID* psubtype )
{
	GUID guidTemp;

	QUARTZ_MediaSubType_FromFourCC(
		&guidTemp, psubtype->Data1 );
	return IsEqualGUID( psubtype, &guidTemp );
}

HRESULT QUARTZ_MediaSubType_FromBitmap(
	GUID* psubtype, const BITMAPINFOHEADER* pbi )
{
	HRESULT hr;
	DWORD*	pdwBitf;

	if ( (pbi->biCompression & 0xffff0000) != 0 )
		return S_FALSE;

	if ( pbi->biWidth <= 0 || pbi->biHeight == 0 )
		return E_FAIL;

	hr = E_FAIL;
	switch ( pbi->biCompression )
	{
	case 0:
		if ( pbi->biPlanes != 1 )
			break;
		switch ( pbi->biBitCount )
		{
		case  1:
			memcpy( psubtype, &MEDIASUBTYPE_RGB1, sizeof(GUID) );
			hr = S_OK;
			break;
		case  4:
			memcpy( psubtype, &MEDIASUBTYPE_RGB4, sizeof(GUID) );
			hr = S_OK;
			break;
		case  8:
			memcpy( psubtype, &MEDIASUBTYPE_RGB8, sizeof(GUID) );
			hr = S_OK;
			break;
		case 16:
			memcpy( psubtype, &MEDIASUBTYPE_RGB555, sizeof(GUID) );
			hr = S_OK;
			break;
		case 24:
			memcpy( psubtype, &MEDIASUBTYPE_RGB24, sizeof(GUID) );
			hr = S_OK;
			break;
		case 32:
			memcpy( psubtype, &MEDIASUBTYPE_RGB32, sizeof(GUID) );
			hr = S_OK;
			break;
		}
		break;
	case 1:
		if ( pbi->biPlanes == 1 && pbi->biHeight > 0 &&
			 pbi->biBitCount == 8 )
		{
			QUARTZ_MediaSubType_FromFourCC( psubtype, 1 );
			hr = S_OK;
		}
		break;
	case 2:
		if ( pbi->biPlanes == 1 && pbi->biHeight > 0 &&
			 pbi->biBitCount == 4 )
		{
			QUARTZ_MediaSubType_FromFourCC( psubtype, 2 );
			hr = S_OK;
		}
		break;
	case 3:
		if ( pbi->biPlanes != 1 )
			break;
		pdwBitf = (DWORD*)( (BYTE*)pbi + sizeof(BITMAPINFOHEADER) );
		switch ( pbi->biBitCount )
		{
		case 16:
			if ( pdwBitf[0] == 0x7c00 &&
				 pdwBitf[1] == 0x03e0 &&
				 pdwBitf[2] == 0x001f )
			{
				memcpy( psubtype, &MEDIASUBTYPE_RGB555, sizeof(GUID) );
				hr = S_OK;
			}
			if ( pdwBitf[0] == 0xf800 &&
				 pdwBitf[1] == 0x07e0 &&
				 pdwBitf[2] == 0x001f )
			{
				memcpy( psubtype, &MEDIASUBTYPE_RGB565, sizeof(GUID) );
				hr = S_OK;
			}
			break;
		case 32:
			if ( pdwBitf[0] == 0x00ff0000 &&
				 pdwBitf[1] == 0x0000ff00 &&
				 pdwBitf[2] == 0x000000ff )
			{
				memcpy( psubtype, &MEDIASUBTYPE_RGB32, sizeof(GUID) );
				hr = S_OK;
			}
			break;
		}
		break;
	}

	return hr;
}

void QUARTZ_PatchBitmapInfoHeader( BITMAPINFOHEADER* pbi )
{
	switch ( pbi->biCompression )
	{
	case mmioFOURCC('R','G','B',' '):
		pbi->biCompression = 0;
		break;
	case mmioFOURCC('R','L','E',' '):
	case mmioFOURCC('M','R','L','E'):
	case mmioFOURCC('R','L','E','8'):
	case mmioFOURCC('R','L','E','4'):
		if ( pbi->biBitCount == 4 )
			pbi->biCompression = 2;
		else
			pbi->biCompression = 1;
		break;
	}
}

BOOL QUARTZ_BitmapHasFixedSample( const BITMAPINFOHEADER* pbi )
{
	switch ( pbi->biCompression )
	{
	case 0:
	case 3:
	case mmioFOURCC('I','4','2','0'):
	case mmioFOURCC('I','Y','U','V'):
	case mmioFOURCC('Y','U','Y','V'):
	case mmioFOURCC('Y','V','U','9'):
	case mmioFOURCC('Y','4','1','1'):
	case mmioFOURCC('Y','4','1','P'):
	case mmioFOURCC('Y','U','Y','2'):
	case mmioFOURCC('Y','V','Y','U'):
	case mmioFOURCC('U','Y','V','Y'):
	case mmioFOURCC('Y','2','1','1'):
	case mmioFOURCC('Y','V','1','2'):
		return TRUE;
	}

	return FALSE;
}


/****************************************************************************/

typedef struct IEnumMediaTypesImpl
{
	ICOM_VFIELD(IEnumMediaTypes);
} IEnumMediaTypesImpl;

typedef struct
{
	QUARTZ_IUnkImpl	unk;
	IEnumMediaTypesImpl	enummtype;
	struct QUARTZ_IFEntry	IFEntries[1];
	CRITICAL_SECTION	cs;
	AM_MEDIA_TYPE*	pTypes;
	ULONG	cTypes;
	ULONG	cCur;
} CEnumMediaTypes;

#define	CEnumMediaTypes_THIS(iface,member)		CEnumMediaTypes*	This = ((CEnumMediaTypes*)(((char*)iface)-offsetof(CEnumMediaTypes,member)))



static HRESULT WINAPI
IEnumMediaTypes_fnQueryInterface(IEnumMediaTypes* iface,REFIID riid,void** ppobj)
{
	CEnumMediaTypes_THIS(iface,enummtype);

	TRACE("(%p)->()\n",This);

	return IUnknown_QueryInterface(This->unk.punkControl,riid,ppobj);
}

static ULONG WINAPI
IEnumMediaTypes_fnAddRef(IEnumMediaTypes* iface)
{
	CEnumMediaTypes_THIS(iface,enummtype);

	TRACE("(%p)->()\n",This);

	return IUnknown_AddRef(This->unk.punkControl);
}

static ULONG WINAPI
IEnumMediaTypes_fnRelease(IEnumMediaTypes* iface)
{
	CEnumMediaTypes_THIS(iface,enummtype);

	TRACE("(%p)->()\n",This);

	return IUnknown_Release(This->unk.punkControl);
}

static HRESULT WINAPI
IEnumMediaTypes_fnNext(IEnumMediaTypes* iface,ULONG cReq,AM_MEDIA_TYPE** ppmtype,ULONG* pcFetched)
{
	CEnumMediaTypes_THIS(iface,enummtype);
	HRESULT	hr;
	ULONG	cFetched;

	TRACE("(%p)->(%u,%p,%p)\n",This,cReq,ppmtype,pcFetched);

	if ( pcFetched == NULL && cReq > 1 )
		return E_INVALIDARG;
	if ( ppmtype == NULL )
		return E_POINTER;

	EnterCriticalSection( &This->cs );

	hr = NOERROR;
	cFetched = 0;
	while ( cReq > 0 )
	{
		if ( This->cCur >= This->cTypes )
		{
			hr = S_FALSE;
			break;
		}
		ppmtype[ cFetched ] =
			QUARTZ_MediaType_Duplicate( &This->pTypes[ This->cCur ] );
		if ( ppmtype[ cFetched ] == NULL )
		{
			hr = E_OUTOFMEMORY;
			while ( cFetched > 0 )
			{
				cFetched --;
				QUARTZ_MediaType_Destroy( ppmtype[ cFetched ] );
			}
			break;
		}

		cFetched ++;

		This->cCur ++;
		cReq --;
	}

	LeaveCriticalSection( &This->cs );

	if ( pcFetched != NULL )
		*pcFetched = cFetched;

	return hr;
}

static HRESULT WINAPI
IEnumMediaTypes_fnSkip(IEnumMediaTypes* iface,ULONG cSkip)
{
	CEnumMediaTypes_THIS(iface,enummtype);
	HRESULT	hr;

	TRACE("(%p)->()\n",This);

	EnterCriticalSection( &This->cs );

	hr = NOERROR;
	while ( cSkip > 0 )
	{
		if ( This->cCur >= This->cTypes )
		{
			hr = S_FALSE;
			break;
		}
		This->cCur ++;
		cSkip --;
	}

	LeaveCriticalSection( &This->cs );

	return hr;
}

static HRESULT WINAPI
IEnumMediaTypes_fnReset(IEnumMediaTypes* iface)
{
	CEnumMediaTypes_THIS(iface,enummtype);

	TRACE("(%p)->()\n",This);

	EnterCriticalSection( &This->cs );

	This->cCur = 0;

	LeaveCriticalSection( &This->cs );

	return NOERROR;
}

static HRESULT WINAPI
IEnumMediaTypes_fnClone(IEnumMediaTypes* iface,IEnumMediaTypes** ppobj)
{
	CEnumMediaTypes_THIS(iface,enummtype);
	HRESULT	hr;

	TRACE("(%p)->()\n",This);

	if ( ppobj == NULL )
		return E_POINTER;

	EnterCriticalSection( &This->cs );

	hr = QUARTZ_CreateEnumMediaTypes(
		ppobj,
		This->pTypes, This->cTypes );
	if ( SUCCEEDED(hr) )
		IEnumMediaTypes_Skip( *ppobj, This->cCur );

	LeaveCriticalSection( &This->cs );

	return hr;
}


static ICOM_VTABLE(IEnumMediaTypes) ienummtype =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	/* IUnknown fields */
	IEnumMediaTypes_fnQueryInterface,
	IEnumMediaTypes_fnAddRef,
	IEnumMediaTypes_fnRelease,
	/* IEnumMediaTypes fields */
	IEnumMediaTypes_fnNext,
	IEnumMediaTypes_fnSkip,
	IEnumMediaTypes_fnReset,
	IEnumMediaTypes_fnClone,
};


/* can I use offsetof safely? - FIXME? */
static QUARTZ_IFEntry IFEntries[] =
{
  { &IID_IEnumMediaTypes, offsetof(CEnumMediaTypes,enummtype)-offsetof(CEnumMediaTypes,unk) },
};


void QUARTZ_DestroyEnumMediaTypes(IUnknown* punk)
{
	CEnumMediaTypes_THIS(punk,unk);
	ULONG	i;

	if ( This->pTypes != NULL )
	{
		for ( i = 0; i < This->cTypes; i++ )
			QUARTZ_MediaType_Free( &This->pTypes[i] );
		QUARTZ_FreeMem( This->pTypes );
	}

	DeleteCriticalSection( &This->cs );
}

HRESULT QUARTZ_CreateEnumMediaTypes(
	IEnumMediaTypes** ppobj,
	const AM_MEDIA_TYPE* pTypes, ULONG cTypes )
{
	CEnumMediaTypes*	penum;
	AM_MEDIA_TYPE*	pTypesDup = NULL;
	ULONG	i;
	HRESULT	hr;

	TRACE("(%p,%p,%u)\n",ppobj,pTypes,cTypes);

	if ( cTypes > 0 )
	{
		pTypesDup = (AM_MEDIA_TYPE*)QUARTZ_AllocMem(
			sizeof( AM_MEDIA_TYPE ) * cTypes );
		if ( pTypesDup == NULL )
			return E_OUTOFMEMORY;

		i = 0;
		while ( i < cTypes )
		{
			hr = QUARTZ_MediaType_Copy( &pTypesDup[i], &pTypes[i] );
			if ( FAILED(hr) )
			{
				while ( i > 0 )
				{
					i --;
					QUARTZ_MediaType_Free( &pTypesDup[i] );
				}
				QUARTZ_FreeMem( pTypesDup );
				return hr;
			}

			i ++;
		}
	}

	penum = (CEnumMediaTypes*)QUARTZ_AllocObj( sizeof(CEnumMediaTypes) );
	if ( penum == NULL )
	{
		return E_OUTOFMEMORY;
	}
	penum->pTypes = pTypesDup;
	penum->cTypes = cTypes;
	penum->cCur = 0;

	QUARTZ_IUnkInit( &penum->unk, NULL );
	ICOM_VTBL(&penum->enummtype) = &ienummtype;

	penum->unk.pEntries = IFEntries;
	penum->unk.dwEntries = sizeof(IFEntries)/sizeof(IFEntries[0]);
	penum->unk.pOnFinalRelease = QUARTZ_DestroyEnumMediaTypes;

	CRITICAL_SECTION_DEFINE( &penum->cs );

	*ppobj = (IEnumMediaTypes*)(&penum->enummtype);

	return S_OK;
}


