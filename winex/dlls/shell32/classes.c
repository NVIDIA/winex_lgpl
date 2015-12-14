/*
 *	file type mapping
 *	(HKEY_CLASSES_ROOT - Stuff)
 *
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wine/debug.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "winuser.h"

#include "shlobj.h"
#include "shell32_main.h"
#include "shlguid.h"
#include "shresdef.h"
#include "wine/obj_queryassociations.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

#define MAX_EXTENSION_LENGTH 20

BOOL HCR_MapTypeToValue ( LPCSTR szExtension, LPSTR szFileType, DWORD len, BOOL bPrependDot)
{	HKEY	hkey;
	char	szTemp[MAX_EXTENSION_LENGTH + 2];

	TRACE("%s %p\n",szExtension, szFileType );

    /* added because we do not want to have double dots */
    if (szExtension[0]=='.')
        bPrependDot=0;

	if (bPrependDot)
	  strcpy(szTemp, ".");

	lstrcpynA(szTemp+((bPrependDot)?1:0), szExtension, MAX_EXTENSION_LENGTH);

	if (RegOpenKeyExA(HKEY_CLASSES_ROOT,szTemp,0,0x02000000,&hkey))
	{ return FALSE;
	}

	if (RegQueryValueA(hkey,NULL,szFileType,&len))
	{ RegCloseKey(hkey);
	  return FALSE;
	}

	RegCloseKey(hkey);

	TRACE("-- %s\n", szFileType );

	return TRUE;
}
BOOL HCR_GetExecuteCommand ( LPCSTR szClass, LPCSTR szVerb, LPSTR szDest, DWORD len )
{
	HKEY	hkey;
	char	sTemp[MAX_PATH];
	DWORD	dwType;
	BOOL	ret = FALSE;

	TRACE("%s %s\n",szClass, szVerb );

	sprintf(sTemp, "%s\\shell\\%s\\command",szClass, szVerb);

	if (!RegOpenKeyExA(HKEY_CLASSES_ROOT,sTemp,0,0x02000000,&hkey))
	{
	  if (!RegQueryValueExA(hkey, NULL, 0, &dwType, szDest, &len))
	  {
	    if (dwType == REG_EXPAND_SZ)
	    {
	      ExpandEnvironmentStringsA(szDest, sTemp, MAX_PATH);
	      strcpy(szDest, sTemp);
	    }
	    ret = TRUE;
	  }
	  RegCloseKey(hkey);
	}
	TRACE("-- %s\n", szDest );
	return ret;
}
/***************************************************************************************
*	HCR_GetDefaultIcon	[internal]
*
* Gets the icon for a filetype
*/
BOOL HCR_GetDefaultIcon (LPCSTR szClass, LPSTR szDest, DWORD len, LPDWORD dwNr)
{
	HKEY	hkey;
	char	sTemp[MAX_PATH];
	char	sNum[5];
	DWORD	dwType;
	BOOL	ret = FALSE;

	TRACE("%s\n",szClass );

	sprintf(sTemp, "%s\\DefaultIcon",szClass);

	if (!RegOpenKeyExA(HKEY_CLASSES_ROOT,sTemp,0,0x02000000,&hkey))
	{
	  if (!RegQueryValueExA(hkey, NULL, 0, &dwType, szDest, &len))
	  {
	    if (dwType == REG_EXPAND_SZ)
	    {
	      ExpandEnvironmentStringsA(szDest, sTemp, MAX_PATH);
	      strcpy(szDest, sTemp);
	    }
	    if (ParseFieldA (szDest, 2, sNum, 5))
               *dwNr=atoi(sNum);
            else
               *dwNr=0; /* sometimes the icon number is missing */
	    ParseFieldA (szDest, 1, szDest, len);
	    ret = TRUE;
	  }
	  RegCloseKey(hkey);
	}
	TRACE("-- %s %li\n", szDest, *dwNr );
	return ret;
}

/***************************************************************************************
*	HCR_GetClassName	[internal]
*
* Gets the name of a registred class
*/
BOOL HCR_GetClassName (REFIID riid, LPSTR szDest, DWORD len)
{	HKEY	hkey;
	char	xriid[50];
	BOOL ret = FALSE;
	DWORD buflen = len;

        sprintf( xriid, "CLSID\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 riid->Data1, riid->Data2, riid->Data3,
                 riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3],
                 riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7] );

	TRACE("%s\n",xriid );

	szDest[0] = 0;
	if (!RegOpenKeyExA(HKEY_CLASSES_ROOT,xriid,0,KEY_READ,&hkey))
	{
	  if (!RegQueryValueExA(hkey,"",0,NULL,szDest,&len))
	  {
	    ret = TRUE;
	  }
	  RegCloseKey(hkey);
	}

	if (!ret || !szDest[0])
	{
	  if(IsEqualIID(riid, &CLSID_ShellDesktop))
	  {
	    if (LoadStringA(shell32_hInstance, IDS_DESKTOP, szDest, buflen))
	      ret = TRUE;
	  }
	  else if (IsEqualIID(riid, &CLSID_MyComputer))
	  {
	    if(LoadStringA(shell32_hInstance, IDS_MYCOMPUTER, szDest, buflen))
	      ret = TRUE;
	  }
	}

	TRACE("-- %s\n", szDest);

	return ret;
}

/***************************************************************************************
*	HCR_GetFolderAttributes	[internal]
*
* gets the folder attributes of a class
*
* FIXME
*	verify the defaultvalue for *szDest
*/
BOOL HCR_GetFolderAttributes (REFIID riid, LPDWORD szDest)
{	HKEY	hkey;
	char	xriid[60];
	DWORD	attributes;
	DWORD	len = 4;

        sprintf( xriid, "CLSID\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 riid->Data1, riid->Data2, riid->Data3,
                 riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3],
                 riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7] );
	TRACE("%s\n",xriid );

	if (!szDest) return FALSE;
	*szDest = SFGAO_FOLDER|SFGAO_FILESYSTEM;

	strcat (xriid, "\\ShellFolder");

	if (RegOpenKeyExA(HKEY_CLASSES_ROOT,xriid,0,KEY_READ,&hkey))
	{
	  return FALSE;
	}

	if (RegQueryValueExA(hkey,"Attributes",0,NULL,(LPBYTE)&attributes,&len))
	{
	  RegCloseKey(hkey);
	  return FALSE;
	}

	RegCloseKey(hkey);

	TRACE("-- 0x%08lx\n", attributes);

	*szDest = attributes;

	return TRUE;
}

typedef struct
{	ICOM_VFIELD(IQueryAssociations);
	DWORD	ref;
} IQueryAssociationsImpl;

static struct ICOM_VTABLE(IQueryAssociations) qavt;

/**************************************************************************
*  IQueryAssociations_Constructor
*/
IQueryAssociations* IQueryAssociations_Constructor(void)
{
	IQueryAssociationsImpl* ei;

	ei=(IQueryAssociationsImpl*)HeapAlloc(GetProcessHeap(),0,sizeof(IQueryAssociationsImpl));
	ei->ref=1;
	ICOM_VTBL(ei) = &qavt;

	TRACE("(%p)\n",ei);
	shell32_ObjCount++;
	return (IQueryAssociations *)ei;
}
/**************************************************************************
 *  IQueryAssociations_QueryInterface
 */
static HRESULT WINAPI IQueryAssociations_fnQueryInterface(
	IQueryAssociations * iface,
	REFIID riid,
	LPVOID *ppvObj)
{
	ICOM_THIS(IQueryAssociationsImpl,iface);

	 TRACE("(%p)->(\n\tIID:\t%s,%p)\n",This,debugstr_guid(riid),ppvObj);

	*ppvObj = NULL;

	if(IsEqualIID(riid, &IID_IUnknown))		/*IUnknown*/
	{
	  *ppvObj = This;
	}
	else if(IsEqualIID(riid, &IID_IQueryAssociations))	/*IExtractIcon*/
	{
	  *ppvObj = (IQueryAssociations*)This;
	}

	if(*ppvObj)
	{
	  IQueryAssociations_AddRef((IQueryAssociations*) *ppvObj);
	  TRACE("-- Interface: (%p)->(%p)\n",ppvObj,*ppvObj);
	  return S_OK;
	}
	TRACE("-- Interface: E_NOINTERFACE\n");
	return E_NOINTERFACE;
}

/**************************************************************************
*  IQueryAssociations_AddRef
*/
static ULONG WINAPI IQueryAssociations_fnAddRef(IQueryAssociations * iface)
{
	ICOM_THIS(IQueryAssociationsImpl,iface);

	TRACE("(%p)->(count=%lu)\n",This, This->ref );

	shell32_ObjCount++;

	return ++(This->ref);
}
/**************************************************************************
*  IQueryAssociations_Release
*/
static ULONG WINAPI IQueryAssociations_fnRelease(IQueryAssociations * iface)
{
	ICOM_THIS(IQueryAssociationsImpl,iface);

	TRACE("(%p)->()\n",This);

	shell32_ObjCount--;

	if (!--(This->ref))
	{
	  TRACE(" destroying IExtractIcon(%p)\n",This);
	  HeapFree(GetProcessHeap(),0,This);
	  return 0;
	}
	return This->ref;
}

static HRESULT WINAPI IQueryAssociations_fnInit(
	IQueryAssociations * iface,
	ASSOCF flags,
	LPCWSTR pszAssoc,
	HKEY hkProgid,
	HWND hwnd)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI IQueryAssociations_fnGetString(
	IQueryAssociations * iface,
	ASSOCF flags,
	ASSOCSTR str,
	LPCWSTR pszExtra,
	LPWSTR pszOut,
	DWORD *pcchOut)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI IQueryAssociations_fnGetKey(
	IQueryAssociations * iface,
	ASSOCF flags,
	ASSOCKEY key,
	LPCWSTR pszExtra,
	HKEY *phkeyOut)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI IQueryAssociations_fnGetData(
	IQueryAssociations * iface,
	ASSOCF flags,
	ASSOCDATA data,
	LPCWSTR pszExtra,
	LPVOID pvOut,
	DWORD *pcbOut)
{
	return E_NOTIMPL;
}
static HRESULT WINAPI IQueryAssociations_fnGetEnum(
	IQueryAssociations * iface,
	ASSOCF flags,
	ASSOCENUM assocenum,
	LPCWSTR pszExtra,
	REFIID riid,
	LPVOID *ppvOut)
{
	return E_NOTIMPL;
}

static struct ICOM_VTABLE(IQueryAssociations) qavt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IQueryAssociations_fnQueryInterface,
	IQueryAssociations_fnAddRef,
	IQueryAssociations_fnRelease,
	IQueryAssociations_fnInit,
	IQueryAssociations_fnGetString,
	IQueryAssociations_fnGetKey,
	IQueryAssociations_fnGetData,
	IQueryAssociations_fnGetEnum
};
