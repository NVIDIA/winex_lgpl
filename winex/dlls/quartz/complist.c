/*
 * List of components. (for internal use)
 *
 * Copyright (c) 2011-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * hidenori@a2.ctktv.ne.jp
 */

#include "config.h"

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "objbase.h"

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "quartz_private.h"
#include "complist.h"



struct QUARTZ_CompList
{
	QUARTZ_CompListItem*	pFirst;
	QUARTZ_CompListItem*	pLast;
	CRITICAL_SECTION		csList;
};

struct QUARTZ_CompListItem
{
	IUnknown*	punk;
	QUARTZ_CompListItem*	pNext;
	QUARTZ_CompListItem*	pPrev;
	void*	pvData;
	DWORD	dwDataLen;
};


QUARTZ_CompList* QUARTZ_CompList_Alloc( void )
{
	QUARTZ_CompList*	pList;

	pList = (QUARTZ_CompList*)QUARTZ_AllocMem( sizeof(QUARTZ_CompList) );
	if ( pList != NULL )
	{
		/* construct. */
		pList->pFirst = NULL;
		pList->pLast = NULL;

		CRITICAL_SECTION_DEFINE( &pList->csList );
	}

	return pList;
}

void QUARTZ_CompList_Free( QUARTZ_CompList* pList )
{
	QUARTZ_CompListItem*	pCur;
	QUARTZ_CompListItem*	pNext;

	if ( pList != NULL )
	{
		pCur = pList->pFirst;
		while ( pCur != NULL )
		{
			pNext = pCur->pNext;
			if ( pCur->punk != NULL )
				IUnknown_Release( pCur->punk );
			if ( pCur->pvData != NULL )
				QUARTZ_FreeMem( pCur->pvData );
			QUARTZ_FreeMem( pCur );
			pCur = pNext;
		}

		DeleteCriticalSection( &pList->csList );

		QUARTZ_FreeMem( pList );
	}
}

void QUARTZ_CompList_Lock( QUARTZ_CompList* pList )
{
	EnterCriticalSection( &pList->csList );
}

void QUARTZ_CompList_Unlock( QUARTZ_CompList* pList )
{
	LeaveCriticalSection( &pList->csList );
}

QUARTZ_CompList* QUARTZ_CompList_Dup(
	const QUARTZ_CompList* pList, BOOL fDupData )
{
	QUARTZ_CompList*	pNewList;
	const QUARTZ_CompListItem*	pCur;
	HRESULT	hr;

	pNewList = QUARTZ_CompList_Alloc();
	if ( pNewList == NULL )
		return NULL;

	pCur = pList->pFirst;
	while ( pCur != NULL )
	{
		if ( pCur->punk != NULL )
		{
			if ( fDupData )
				hr = QUARTZ_CompList_AddComp(
					pNewList, pCur->punk,
					pCur->pvData, pCur->dwDataLen );
			else
				hr = QUARTZ_CompList_AddComp(
					pNewList, pCur->punk, NULL, 0 );
			if ( FAILED(hr) )
			{
				QUARTZ_CompList_Free( pNewList );
				return NULL;
			}
		}
		pCur = pCur->pNext;
	}

	return pNewList;
}

static QUARTZ_CompListItem* QUARTZ_CompList_AllocComp(
	QUARTZ_CompList* pList, IUnknown* punk,
	const void* pvData, DWORD dwDataLen )
{
	QUARTZ_CompListItem*	pItem;

	pItem = (QUARTZ_CompListItem*)QUARTZ_AllocMem( sizeof(QUARTZ_CompListItem) );
	if ( pItem == NULL )
		return NULL;

	pItem->pvData = NULL;
	pItem->dwDataLen = 0;
	if ( pvData != NULL )
	{
		pItem->pvData = (void*)QUARTZ_AllocMem( dwDataLen );
		if ( pItem->pvData == NULL )
		{
			QUARTZ_FreeMem( pItem );
			return NULL;
		}
		memcpy( pItem->pvData, pvData, dwDataLen );
		pItem->dwDataLen = dwDataLen;
	}

	pItem->punk = punk; IUnknown_AddRef(punk);

	return pItem;
}

HRESULT QUARTZ_CompList_AddComp(
	QUARTZ_CompList* pList, IUnknown* punk,
	const void* pvData, DWORD dwDataLen )
{
	QUARTZ_CompListItem*	pItem;

	pItem = QUARTZ_CompList_AllocComp( pList, punk, pvData, dwDataLen );
	if ( pItem == NULL )
		return E_OUTOFMEMORY;

	if ( pList->pFirst != NULL )
		pList->pFirst->pPrev = pItem;
	else
		pList->pLast = pItem;
	pItem->pNext = pList->pFirst;
	pList->pFirst = pItem;
	pItem->pPrev = NULL;

	return S_OK;
}

HRESULT QUARTZ_CompList_AddTailComp(
	QUARTZ_CompList* pList, IUnknown* punk,
	const void* pvData, DWORD dwDataLen )
{
	QUARTZ_CompListItem*	pItem;

	pItem = QUARTZ_CompList_AllocComp( pList, punk, pvData, dwDataLen );
	if ( pItem == NULL )
		return E_OUTOFMEMORY;

	if ( pList->pLast != NULL )
		pList->pLast->pNext = pItem;
	else
		pList->pFirst = pItem;
	pItem->pPrev = pList->pLast;
	pList->pLast = pItem;
	pItem->pNext = NULL;

	return S_OK;
}

HRESULT QUARTZ_CompList_RemoveComp( QUARTZ_CompList* pList, IUnknown* punk )
{
	QUARTZ_CompListItem*	pCur;

	pCur = QUARTZ_CompList_SearchComp( pList, punk );
	if ( pCur == NULL )
		return S_FALSE; /* already removed. */

	/* remove from list. */
	if ( pCur->pNext != NULL )
		pCur->pNext->pPrev = pCur->pPrev;
	else
		pList->pLast = pCur->pPrev;
	if ( pCur->pPrev != NULL )
		pCur->pPrev->pNext = pCur->pNext;
	else
		pList->pFirst = pCur->pNext;

	/* release this item. */
	if ( pCur->punk != NULL )
		IUnknown_Release( pCur->punk );
	if ( pCur->pvData != NULL )
		QUARTZ_FreeMem( pCur->pvData );
	QUARTZ_FreeMem( pCur );

	return S_OK;
}

QUARTZ_CompListItem* QUARTZ_CompList_SearchComp(
	QUARTZ_CompList* pList, IUnknown* punk )
{
	QUARTZ_CompListItem*	pCur;

	pCur = pList->pFirst;
	while ( pCur != NULL )
	{
		if ( pCur->punk == punk )
			return pCur;
		pCur = pCur->pNext;
	}

	return NULL;
}

QUARTZ_CompListItem* QUARTZ_CompList_SearchData(
	QUARTZ_CompList* pList, const void* pvData, DWORD dwDataLen )
{
	QUARTZ_CompListItem*	pCur;

	pCur = pList->pFirst;
	while ( pCur != NULL )
	{
		if ( pCur->dwDataLen == dwDataLen &&
		     !memcmp( pCur->pvData, pvData, dwDataLen ) )
			return pCur;
		pCur = pCur->pNext;
	}

	return NULL;
}

QUARTZ_CompListItem* QUARTZ_CompList_GetFirst(
	QUARTZ_CompList* pList )
{
	return pList->pFirst;
}

QUARTZ_CompListItem* QUARTZ_CompList_GetLast(
	QUARTZ_CompList* pList )
{
	return pList->pLast;
}

QUARTZ_CompListItem* QUARTZ_CompList_GetNext(
	QUARTZ_CompList* pList, QUARTZ_CompListItem* pPrev )
{
	return pPrev->pNext;
}

QUARTZ_CompListItem* QUARTZ_CompList_GetPrev(
	QUARTZ_CompList* pList, QUARTZ_CompListItem* pNext )
{
	return pNext->pPrev;
}

IUnknown* QUARTZ_CompList_GetItemPtr( QUARTZ_CompListItem* pItem )
{
	return pItem->punk;
}

const void* QUARTZ_CompList_GetDataPtr( QUARTZ_CompListItem* pItem )
{
	return pItem->pvData;
}

DWORD QUARTZ_CompList_GetDataLength( QUARTZ_CompListItem* pItem )
{
	return pItem->dwDataLen;
}
