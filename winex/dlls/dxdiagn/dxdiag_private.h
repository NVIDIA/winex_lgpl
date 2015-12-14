/*
 * DXDiag private include file
 *
 * Copyright 2004 Raphael Junqueira
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_DXDIAG_PRIVATE_H
#define __WINE_DXDIAG_PRIVATE_H

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "objbase.h"
#include "oleauto.h"

#include "dxdiag.h"

/* DXDiag Interfaces: */
typedef struct IDxDiagProviderImpl  IDxDiagProviderImpl;
typedef struct IDxDiagContainerImpl IDxDiagContainerImpl;

/* ---------------- */
/* IDxDiagProvider  */
/* ---------------- */

/*****************************************************************************
 * IDxDiagProvider implementation structure
 */
struct IDxDiagProviderImpl {
  /* IUnknown fields */
  const IDxDiagProviderVtbl *lpVtbl;
  LONG        ref;
  /* IDxDiagProvider fields */
  BOOL        init;
  DXDIAG_INIT_PARAMS params;
  IDxDiagContainer* pRootContainer;
};

/* ---------------- */
/* IDxDiagContainer  */
/* ---------------- */

typedef struct IDxDiagContainerImpl_SubContainer {
  IDxDiagContainer* pCont;
  WCHAR* contName;
  struct IDxDiagContainerImpl_SubContainer* next;
} IDxDiagContainerImpl_SubContainer;

typedef struct IDxDiagContainerImpl_Property {
  LPWSTR vName;
  VARIANT v;
  struct IDxDiagContainerImpl_Property* next;
} IDxDiagContainerImpl_Property;


/*****************************************************************************
 * IDxDiagContainer implementation structure
 */
struct IDxDiagContainerImpl {
  /* IUnknown fields */
  const IDxDiagContainerVtbl *lpVtbl;
  LONG        ref;
  /* IDxDiagContainer fields */
  IDxDiagContainerImpl_Property* properties;  
  IDxDiagContainerImpl_SubContainer* subContainers;
  DWORD nProperties;
  DWORD nSubContainers;
};

/* IUnknown: */
extern HRESULT WINAPI IDxDiagContainerImpl_QueryInterface(PDXDIAGCONTAINER iface, REFIID riid, LPVOID *ppobj);
/** Internal */
extern HRESULT WINAPI IDxDiagContainerImpl_AddProp(PDXDIAGCONTAINER iface, LPCWSTR pwszPropName, VARIANT* pVarProp);
extern HRESULT WINAPI IDxDiagContainerImpl_AddChildContainer(PDXDIAGCONTAINER iface, LPCWSTR pszContName, PDXDIAGCONTAINER pSubCont);

/**
 * factories
 */
extern HRESULT DXDiag_CreateDXDiagProvider(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj);

/** internal factory */
extern HRESULT DXDiag_CreateDXDiagContainer(REFIID riid, LPVOID *ppobj);

/**********************************************************************
 * Dll lifetime tracking declaration for dxdiagn.dll
 */
extern LONG DXDIAGN_refCount;
static inline void DXDIAGN_LockModule(void) { InterlockedIncrement( &DXDIAGN_refCount ); }
static inline void DXDIAGN_UnlockModule(void) { InterlockedDecrement( &DXDIAGN_refCount ); }

#endif
