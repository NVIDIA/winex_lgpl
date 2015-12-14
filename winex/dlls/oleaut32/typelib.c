/*
 *	TYPELIB
 *
 *	Copyright 1997	Marcus Meissner
 *		      1999  Rein Klazes
 *		      2000  Francois Jacques
 *		      2001  Huw D M Davies for CodeWeavers
 *		      2003  TransGaming Technologies
 *
 *
 * --------------------------------------------------------------------------------------
 * Known problems (2000, Francois Jacques)
 *
 * - Tested using OLEVIEW (Platform SDK tool) only.
 *
 * - dual interface dispinterfaces. vtable-interface ITypeInfo instances are
 *   creating by doing a straight copy of the dispinterface instance and just changing
 *   its typekind. Pointed structures aren't copied - only the address of the pointers.
 *   So when you release the dispinterface, you delete the vtable-interface structures
 *   as well... fortunately, clean up of structures is not implemented.
 *
 * - locale stuff is partially implemented but hasn't been tested.
 *
 * - typelib file is still read in its entirety, but it is released now.
 * - some garbage is read from function names on some very rare occasions.
 *
 * --------------------------------------------------------------------------------------
 *  Known problems left from previous implementation (1999, Rein Klazes) :
 *
 * -. Data structures are straightforward, but slow for look-ups.
 * -. (related) nothing is hashed
 * -. there are a number of stubs in ITypeLib and ITypeInfo interfaces. Most
 *      of them I don't know yet how to implement them.
 * -. Most error return values are just guessed not checked with windows
 *      behaviour.
 * -. didn't bother with a c++ interface
 * -. lousy fatal error handling
 * -. some methods just return pointers to internal data structures, this is
 *      partly laziness, partly I want to check how windows does it.
 *
 */

#include "config.h"
#include "wine/port.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "winerror.h"
#include "winnls.h"         /* for PRIMARYLANGID */
#include "winreg.h"         /* for HKEY_LOCAL_MACHINE */
#include "winuser.h"

#include "wine/unicode.h"
#include "objbase.h"
#include "heap.h"
#include "ole2disp.h"
#include "typelib.h"
#include "wine/debug.h"
#include "winternl.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);
WINE_DECLARE_DEBUG_CHANNEL(typelib);

const CLSID CLSID_PSOAInterface = {
  0x20424, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}
};

/****************************************************************************
 *		QueryPathOfRegTypeLib	[TYPELIB.14]
 *
 * the path is "Classes\Typelib\<guid>\<major>.<minor>\<lcid>\win16\"
 * RETURNS
 *	path of typelib
 */
HRESULT WINAPI
QueryPathOfRegTypeLib16(
	REFGUID guid,	/* [in] referenced guid */
	WORD wMaj,	/* [in] major version */
	WORD wMin,	/* [in] minor version */
	LCID lcid,	/* [in] locale id */
	LPBSTR16 path	/* [out] path of typelib */
) {
	char	xguid[80];
	char	typelibkey[100],pathname[260];
	DWORD	plen;

       	TRACE("\n");

	if (HIWORD(guid)) {
            sprintf( typelibkey, "SOFTWARE\\Classes\\Typelib\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\%d.%d\\%lx\\win16",
                     guid->Data1, guid->Data2, guid->Data3,
                     guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                     guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7],
                     wMaj,wMin,lcid);
	} else {
		sprintf(xguid,"<guid 0x%08lx>",(DWORD)guid);
		FIXME("(%s,%d,%d,0x%04lx,%p),can't handle non-string guids.\n",xguid,wMaj,wMin,(DWORD)lcid,path);
		return E_FAIL;
	}
	plen = sizeof(pathname);
	if (RegQueryValueA(HKEY_LOCAL_MACHINE,typelibkey,pathname,&plen)) {
		/* try again without lang specific id */
		if (SUBLANGID(lcid))
			return QueryPathOfRegTypeLib16(guid,wMaj,wMin,PRIMARYLANGID(lcid),path);
		FIXME("key %s not found\n",typelibkey);
		return E_FAIL;
	}
	*path = SysAllocString16(pathname);
	return S_OK;
}

/****************************************************************************
 *		QueryPathOfRegTypeLib	[OLEAUT32.164]
 * RETURNS
 *	path of typelib
 */
HRESULT WINAPI
QueryPathOfRegTypeLib(
	REFGUID guid,	/* [in] referenced guid */
	WORD wMaj,	/* [in] major version */
	WORD wMin,	/* [in] minor version */
	LCID lcid,	/* [in] locale id */
	LPBSTR path )	/* [out] path of typelib */
{
    /* don't need to ZeroMemory those arrays since sprintf and RegQueryValue add
       string termination character on output strings */

    HRESULT hr        = E_FAIL;

    DWORD   dwPathLen = _MAX_PATH;
    LCID    myLCID    = lcid;

    char    szXGUID[80];
    char    szTypeLibKey[100];
    char    szPath[dwPathLen];

    if ( !HIWORD(guid) )
    {
        sprintf(szXGUID,
            "<guid 0x%08lx>",
            (DWORD) guid);

        FIXME("(%s,%d,%d,0x%04lx,%p),stub!\n", szXGUID, wMaj, wMin, (DWORD)lcid, path);
        return E_FAIL;
    }

    while (hr != S_OK)
    {
        sprintf(szTypeLibKey,
            "SOFTWARE\\Classes\\Typelib\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\%d.%d\\%lx\\win32",
            guid->Data1,    guid->Data2,    guid->Data3,
            guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
            guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7],
            wMaj,
            wMin,
            myLCID);

        if (RegQueryValueA(HKEY_LOCAL_MACHINE, szTypeLibKey, szPath, &dwPathLen))
        {
            if (!lcid)
                break;
            else if (myLCID == lcid)
            {
                /* try with sub-langid */
                myLCID = SUBLANGID(lcid);
            }
            else if ((myLCID == SUBLANGID(lcid)) && myLCID)
            {
                /* try with system langid */
                myLCID = 0;
            }
            else
            {
                break;
            }
        }
        else
        {
            DWORD len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szPath, dwPathLen, NULL, 0 );
            BSTR bstrPath = SysAllocStringLen(NULL,len);

            MultiByteToWideChar(CP_ACP,
                                MB_PRECOMPOSED,
                                szPath,
                                dwPathLen,
                                bstrPath,
                                len);
           *path = bstrPath;
           hr = S_OK;
        }
    }

    if (hr != S_OK)
		TRACE_(typelib)("%s not found\n", szTypeLibKey);

    return hr;
}

/******************************************************************************
 * CreateTypeLib [OLEAUT32.160]  creates a typelib
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: Status
 */
HRESULT WINAPI CreateTypeLib(
	SYSKIND syskind, LPCOLESTR szFile, ICreateTypeLib** ppctlib
) {
    FIXME("(%d,%s,%p), stub!\n",syskind,debugstr_w(szFile),ppctlib);
    return E_FAIL;
}
/******************************************************************************
 * LoadTypeLib [TYPELIB.3]  Loads and registers a type library
 * NOTES
 *    Docs: OLECHAR FAR* szFile
 *    Docs: iTypeLib FAR* FAR* pptLib
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: Status
 */
HRESULT WINAPI LoadTypeLib16(
    LPOLESTR szFile, /* [in] Name of file to load from */
    ITypeLib** pptLib) /* [out] Pointer to pointer to loaded type library */
{
    FIXME("(%s,%p): stub\n",debugstr_w((LPWSTR)szFile),pptLib);

    if (pptLib!=0)
      *pptLib=0;

    return E_FAIL;
}

/******************************************************************************
 *		LoadTypeLib	[OLEAUT32.161]
 * Loads and registers a type library
 * NOTES
 *    Docs: OLECHAR FAR* szFile
 *    Docs: iTypeLib FAR* FAR* pptLib
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: Status
 */
int TLB_ReadTypeLib(LPCWSTR file, INT index, ITypeLib2 **ppTypelib);

HRESULT WINAPI LoadTypeLib(
    const OLECHAR *szFile,/* [in] Name of file to load from */
    ITypeLib * *pptLib)   /* [out] Pointer to pointer to loaded type library */
{
    TRACE("\n");
    return LoadTypeLibEx(szFile, REGKIND_DEFAULT, pptLib);
}

/******************************************************************************
 *		LoadTypeLibEx	[OLEAUT32.183]
 * Loads and optionally registers a type library
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: Status
 */
HRESULT WINAPI LoadTypeLibEx(
    LPCOLESTR szFile,  /* [in] Name of file to load from */
    REGKIND  regkind,  /* [in] Specify kind of registration */
    ITypeLib **pptLib) /* [out] Pointer to pointer to loaded type library */
{
    WCHAR szPath[MAX_PATH+1], szFileCopy[MAX_PATH+1];
    WCHAR *pIndexStr;
    HRESULT res;
    INT index = 1;
    TRACE("(%s,%d,%p)\n",debugstr_w(szFile), regkind, pptLib);

    if(!SearchPathW(NULL,szFile,NULL,sizeof(szPath)/sizeof(WCHAR),szPath,
		    NULL)) {

        /* Look for a trailing '\\' followed by an index */
        pIndexStr = strrchrW(szFile, '\\');
	if(pIndexStr && pIndexStr != szFile && *++pIndexStr != '\0') {
	    index = strtolW(pIndexStr, NULL, 10);
	    memcpy(szFileCopy, szFile,
		   (pIndexStr - szFile - 1) * sizeof(WCHAR));
	    szFileCopy[pIndexStr - szFile - 1] = '\0';
	    if(!SearchPathW(NULL,szFileCopy,NULL,sizeof(szPath)/sizeof(WCHAR),
			    szPath,NULL))
	        return TYPE_E_CANTLOADLIBRARY;
	    if (GetFileAttributesW(szFileCopy) & FILE_ATTRIBUTE_DIRECTORY)
		return TYPE_E_CANTLOADLIBRARY;
	} else
	    return TYPE_E_CANTLOADLIBRARY;
    }

    TRACE("File %s index %d\n", debugstr_w(szPath), index);

    res = TLB_ReadTypeLib(szPath, index, (ITypeLib2**)pptLib);

    if (SUCCEEDED(res))
        switch(regkind)
        {
            case REGKIND_DEFAULT:
                /* FIXME: is this correct? */
                if (!szFile || !szFile[0] ||
		   (szFile[0] != '\\' && szFile[0] != '/' && szFile[1] != ':'))
                    break;
                /* else fall-through */
            case REGKIND_REGISTER:
                /* FIXME: Help path? */
                if (!SUCCEEDED(res = RegisterTypeLib(*pptLib, (LPOLESTR)szFile, NULL)))
                {
                    IUnknown_Release(*pptLib);
                    *pptLib = 0;
                }
                break;
            case REGKIND_NONE:
                break;
        }

    TRACE(" returns %08lx\n",res);
    return res;
}

/******************************************************************************
 *		LoadRegTypeLib	[OLEAUT32.162]
 */
HRESULT WINAPI LoadRegTypeLib(
	REFGUID rguid,		/* [in] referenced guid */
	WORD wVerMajor,		/* [in] major version */
	WORD wVerMinor,		/* [in] minor version */
	LCID lcid,		/* [in] locale id */
	ITypeLib **ppTLib)	/* [out] path of typelib */
{
    BSTR bstr=NULL;
    HRESULT res=QueryPathOfRegTypeLib( rguid, wVerMajor, wVerMinor, lcid, &bstr);

    if(SUCCEEDED(res))
    {
        res= LoadTypeLib(bstr, ppTLib);
        SysFreeString(bstr);
    }

    TRACE("(IID: %s) load %s (%p)\n",debugstr_guid(rguid), SUCCEEDED(res)? "SUCCESS":"FAILED", *ppTLib);

    return res;
}


/******************************************************************************
 *		RegisterTypeLib	[OLEAUT32.163]
 * Adds information about a type library to the System Registry
 * NOTES
 *    Docs: ITypeLib FAR * ptlib
 *    Docs: OLECHAR FAR* szFullPath
 *    Docs: OLECHAR FAR* szHelpDir
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: Status
 */
HRESULT WINAPI RegisterTypeLib(
     ITypeLib * ptlib,     /* [in] Pointer to the library*/
     OLECHAR * szFullPath, /* [in] full Path of the library*/
     OLECHAR * szHelpDir)  /* [in] dir to the helpfile for the library,
							 may be NULL*/
{
    HRESULT res;
    TLIBATTR *attr;
    OLECHAR guid[80];
    LPSTR guidA;
    CHAR keyName[120];
    HKEY key, subKey;
    UINT types, tidx;
    TYPEKIND kind;
    static const char *PSOA = "{00020424-0000-0000-C000-000000000046}";

    if (ptlib == NULL || szFullPath == NULL)
        return E_INVALIDARG;

    if (!SUCCEEDED(ITypeLib_GetLibAttr(ptlib, &attr)))
        return E_FAIL;

    StringFromGUID2(&attr->guid, guid, 80);
    guidA = HEAP_strdupWtoA(GetProcessHeap(), 0, guid);
    snprintf(keyName, sizeof(keyName), "TypeLib\\%s\\%x.%x",
        guidA, attr->wMajorVerNum, attr->wMinorVerNum);
    HeapFree(GetProcessHeap(), 0, guidA);

    res = S_OK;
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, keyName, 0, NULL, 0,
        KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        LPOLESTR doc;

        if (SUCCEEDED(ITypeLib_GetDocumentation(ptlib, -1, NULL, &doc, NULL, NULL)))
        {
            if (RegSetValueExW(key, NULL, 0, REG_SZ,
                (BYTE *)doc, lstrlenW(doc) * sizeof(OLECHAR)) != ERROR_SUCCESS)
                res = E_FAIL;

            SysFreeString(doc);
        }
        else
            res = E_FAIL;

        /* FIXME: This *seems* to be 0 always, not sure though */
        if (res == S_OK && RegCreateKeyExA(key, "0\\win32", 0, NULL, 0,
            KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS)
        {
            if (RegSetValueExW(subKey, NULL, 0, REG_SZ,
                (BYTE *)szFullPath, lstrlenW(szFullPath) * sizeof(OLECHAR)) != ERROR_SUCCESS)
                res = E_FAIL;

            RegCloseKey(subKey);
        }
        else
            res = E_FAIL;

        if (res == S_OK && RegCreateKeyExA(key, "FLAGS", 0, NULL, 0,
            KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS)
        {
            CHAR buf[20];
            /* FIXME: is %u correct? */
            snprintf(buf, sizeof(buf), "%u", attr->wLibFlags);
            if (RegSetValueExA(subKey, NULL, 0, REG_SZ,
                buf, lstrlenA(buf) + 1) != ERROR_SUCCESS)
                res = E_FAIL;
        }
        RegCloseKey(key);
    }
    else
        res = E_FAIL;

    /* register OLE Automation-compatible interfaces for this typelib */
    types = ITypeLib_GetTypeInfoCount(ptlib);
    for (tidx=0; tidx<types; tidx++) {
	if (SUCCEEDED(ITypeLib_GetTypeInfoType(ptlib, tidx, &kind))) {
	    LPOLESTR name = NULL;
	    ITypeInfo *tinfo = NULL;
	    BOOL stop = FALSE;
	    ITypeLib_GetDocumentation(ptlib, tidx, &name, NULL, NULL, NULL);
	    switch (kind) {
	    case TKIND_INTERFACE:
	        TRACE_(typelib)("%d: interface %s\n", tidx, debugstr_w(name));
	        ITypeLib_GetTypeInfo(ptlib, tidx, &tinfo);
	        break;
	    case TKIND_DISPATCH:
	        {
		        TYPEATTR *tattr = NULL;
		        ITypeLib_GetTypeInfo(ptlib, tidx, &tinfo);
		        ITypeInfo_GetTypeAttr(tinfo, &tattr);
		        TRACE_(typelib)("%d: dispinterface %s\n", tidx, debugstr_w(name));

		        if( !(tattr && tattr->wTypeFlags & TYPEFLAG_FDUAL) ) {
	                  TRACE( "Not registering TKIND_DISPATCH\n" );
	                  ITypeInfo_ReleaseTypeAttr( tinfo, tattr );
			          ITypeInfo_Release(tinfo);
			          tinfo = NULL;
			    }
			    else ITypeInfo_ReleaseTypeAttr( tinfo, tattr );
		    }
		    break;
	    case TKIND_COCLASS:
            TRACE_(typelib)("%d: coclass %s\n", tidx, debugstr_w(name));
            /* coclasses should probably not be registered? */
            break;
        case TKIND_ENUM:
            TRACE_(typelib)("%d: enum %s\n", tidx, debugstr_w(name));
            break;
        case TKIND_ALIAS:
            TRACE_(typelib)("%d: alias %s\n", tidx, debugstr_w(name));
            break;
	    default:
            ERR_(typelib)("%d: unhandled kind (%d) %s\n", tidx, kind, debugstr_w(name));
            break;
	    }
	    if (tinfo) {
		TYPEATTR *tattr = NULL;
		ITypeInfo_GetTypeAttr(tinfo, &tattr);
		if (tattr) {
		    TRACE_(typelib)("guid=%s, flags=%04x\n",
				    debugstr_guid(&tattr->guid),
				    tattr->wTypeFlags);
#undef INSTALLSHIELD_HACK
#ifndef INSTALLSHIELD_HACK
		    if (tattr->wTypeFlags & TYPEFLAG_FOLEAUTOMATION)
#endif
		    {
			/* register interface<->typelib coupling */
			StringFromGUID2(&tattr->guid, guid, 80);
			guidA = HEAP_strdupWtoA(GetProcessHeap(), 0, guid);
			snprintf(keyName, sizeof(keyName), "Interface\\%s", guidA);
			HeapFree(GetProcessHeap(), 0, guidA);

			if (RegCreateKeyExA(HKEY_CLASSES_ROOT, keyName, 0, NULL, 0,
					    KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
			    if (name)
				RegSetValueExW(key, NULL, 0, REG_SZ,
					       (BYTE *)name, lstrlenW(name) * sizeof(OLECHAR));

			    if (RegCreateKeyExA(key, "ProxyStubClsid", 0, NULL, 0,
				KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS) {
				RegSetValueExA(subKey, NULL, 0, REG_SZ,
					       PSOA, strlen(PSOA));
				RegCloseKey(subKey);
			    }
			    if (RegCreateKeyExA(key, "ProxyStubClsid32", 0, NULL, 0,
				KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS) {
				RegSetValueExA(subKey, NULL, 0, REG_SZ,
					       PSOA, strlen(PSOA));
				RegCloseKey(subKey);
			    }

			    if (RegCreateKeyExA(key, "TypeLib", 0, NULL, 0,
				KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS) {
				CHAR ver[32];
				StringFromGUID2(&attr->guid, guid, 80);
				snprintf(ver, sizeof(ver), "%x.%x",
					 attr->wMajorVerNum, attr->wMinorVerNum);
				RegSetValueExW(subKey, NULL, 0, REG_SZ,
					       (BYTE *)guid, lstrlenW(guid) * sizeof(OLECHAR));
				RegSetValueExA(subKey, "Version", 0, REG_SZ,
					       ver, lstrlenA(ver));
				RegCloseKey(subKey);
			    }
			    RegCloseKey(key);
			}
		    }
		    ITypeInfo_ReleaseTypeAttr(tinfo, tattr);
		}
		ITypeInfo_Release(tinfo);
	    }
	    SysFreeString(name);
	    if (stop) break;
	}
    }

    ITypeLib_ReleaseTLibAttr(ptlib, attr);

    return res;
}


/******************************************************************************
 *	UnRegisterTypeLib	[OLEAUT32.186]
 * Removes information about a type library from the System Registry
 * NOTES
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: Status
 */
HRESULT WINAPI UnRegisterTypeLib(
    REFGUID libid,	/* [in] Guid of the library */
	WORD wVerMajor,	/* [in] major version */
	WORD wVerMinor,	/* [in] minor version */
	LCID lcid,	/* [in] locale id */
	SYSKIND syskind)
{
    TRACE("(IID: %s): stub\n",debugstr_guid(libid));
    return S_OK;	/* FIXME: pretend everything is OK */
}

/****************************************************************************
 *	OaBuildVersion				(TYPELIB.15)
 *
 * known TYPELIB.DLL versions:
 *
 * OLE 2.01 no OaBuildVersion() avail	1993	--	---
 * OLE 2.02				1993-94	02     3002
 * OLE 2.03					23	730
 * OLE 2.03					03     3025
 * OLE 2.03 W98 SE orig. file !!	1993-95	10     3024
 * OLE 2.1   NT				1993-95	??	???
 * OLE 2.3.1 W95				23	700
 * OLE2 4.0  NT4SP6			1993-98	40     4277
 */
DWORD WINAPI OaBuildVersion16(void)
{
    /* FIXME: I'd like to return the highest currently known version value
     * in case the user didn't force a --winver, but I don't know how
     * to retrieve the "versionForced" info from misc/version.c :(
     * (this would be useful in other places, too) */
    FIXME("If you get version error messages, please report them\n");
    switch(GetVersion() & 0x8000ffff)  /* mask off build number */
    {
    case 0x80000a03:  /* WIN31 */
		return MAKELONG(3027, 3); /* WfW 3.11 */
    case 0x80000004:  /* WIN95 */
		return MAKELONG(700, 23); /* Win95A */
    case 0x80000a04:  /* WIN98 */
		return MAKELONG(3024, 10); /* W98 SE */
    case 0x00000004:  /* NT4 */
		return MAKELONG(4277, 40); /* NT4 SP6 */
    default:
	FIXME("Version value not known yet. Please investigate it!\n");
		return 0;
    }
}

/* for better debugging info leave the static out for the time being */
#define static

/*======================= ITypeLib implementation =======================*/

typedef struct tagTLBCustData
{
    GUID guid;
    VARIANT data;
    struct tagTLBCustData* next;
} TLBCustData;

/* data structure for import typelibs */
typedef struct tagTLBImpLib
{
    int offset;                 /* offset in the file (MSFT)
				   offset in nametable (SLTG)
				   just used to identify library while reading
				   data from file */
    GUID guid;                  /* libid */
    BSTR name;                  /* name */

    LCID lcid;                  /* lcid of imported typelib */

    WORD wVersionMajor;         /* major version number */
    WORD wVersionMinor;         /* minor version number */

    struct tagITypeLibImpl *pImpTypeLib; /* pointer to loaded typelib, or
					    NULL if not yet loaded */
    struct tagTLBImpLib * next;
} TLBImpLib;

/* internal ITypeLib data */
typedef struct tagITypeLibImpl
{
    ICOM_VFIELD(ITypeLib2);
    UINT ref;
    TLIBATTR LibAttr;            /* guid,lcid,syskind,version,flags */

    /* strings can be stored in tlb as multibyte strings BUT they are *always*
     * exported to the application as a UNICODE string.
     */
    BSTR Name;
    BSTR DocString;
    BSTR HelpFile;
    BSTR HelpStringDll;
    unsigned long  dwHelpContext;
    int TypeInfoCount;          /* nr of typeinfo's in librarry */
    struct tagITypeInfoImpl *pTypeInfo;   /* linked list of type info data */
    int ctCustData;             /* number of items in cust data list */
    TLBCustData * pCustData;    /* linked list to cust data */
    TLBImpLib   * pImpLibs;     /* linked list to all imported typelibs */
    TYPEDESC * pTypeDesc;       /* array of TypeDescriptions found in the
				   libary. Only used while read MSFT
				   typelibs */

    struct tagITypeLibImpl *prev, *next;
    LPCWSTR path;
} ITypeLibImpl;

static struct ICOM_VTABLE(ITypeLib2) tlbvt;

static CRITICAL_SECTION csTypeLibImpl;
static ITypeLibImpl* firstTypeLibImpl;

/* ITypeLib methods */
static ITypeLib2* ITypeLib2_Constructor_MSFT(LPVOID pLib, DWORD dwTLBLength);
static ITypeLib2* ITypeLib2_Constructor_SLTG(LPVOID pLib, DWORD dwTLBLength);

/*======================= ITypeInfo implementation =======================*/

/* data for refernced types */
typedef struct tagTLBRefType
{
    INT index;              /* Type index for internal ref or for external ref
			       it the format is SLTG.  -2 indicates to
			       use guid */

    GUID guid;              /* guid of the referenced type */
                            /* if index == TLB_REF_USE_GUID */

    HREFTYPE reference;     /* The href of this ref */
    TLBImpLib *pImpTLInfo;  /* If ref is external ptr to library data
			       TLB_REF_INTERNAL for internal refs
			       TLB_REF_NOT_FOUND for broken refs */

    struct tagTLBRefType * next;
} TLBRefType;

#define TLB_REF_USE_GUID -2

#define TLB_REF_INTERNAL (void*)-2
#define TLB_REF_NOT_FOUND (void*)-1

/* internal Parameter data */
typedef struct tagTLBParDesc
{
    BSTR Name;
    int ctCustData;
    TLBCustData * pCustData;        /* linked list to cust data */
} TLBParDesc;

/* internal Function data */
typedef struct tagTLBFuncDesc
{
    FUNCDESC funcdesc;      /* lots of info on the function and its attributes. */
    BSTR Name;             /* the name of this function */
    TLBParDesc *pParamDesc; /* array with param names and custom data */
    int helpcontext;
    int HelpStringContext;
    BSTR HelpString;
    BSTR Entry;            /* if its Hiword==0, it numeric; -1 is not present*/
    int ctCustData;
    TLBCustData * pCustData;        /* linked list to cust data; */
    struct tagTLBFuncDesc * next;
} TLBFuncDesc;

/* internal Variable data */
typedef struct tagTLBVarDesc
{
    VARDESC vardesc;        /* lots of info on the variable and its attributes. */
    BSTR Name;             /* the name of this variable */
    int HelpContext;
    int HelpStringContext;  /* FIXME: where? */
    BSTR HelpString;
    int ctCustData;
    TLBCustData * pCustData;/* linked list to cust data; */
    struct tagTLBVarDesc * next;
} TLBVarDesc;

/* internal implemented interface data */
typedef struct tagTLBImplType
{
    HREFTYPE hRef;          /* hRef of interface */
    int implflags;          /* IMPLFLAG_*s */
    int ctCustData;
    TLBCustData * pCustData;/* linked list to custom data; */
    struct tagTLBImplType *next;
} TLBImplType;

/* internal TypeInfo data */
typedef struct tagITypeInfoImpl
{
    ICOM_VFIELD(ITypeInfo2);
    UINT ref;
    TYPEATTR TypeAttr ;         /* _lots_ of type information. */
    ITypeLibImpl * pTypeLib;        /* back pointer to typelib */
    int index;                  /* index in this typelib; */
    /* type libs seem to store the doc strings in ascii
     * so why should we do it in unicode?
     */
    BSTR Name;
    BSTR DocString;
    unsigned long  dwHelpContext;
    unsigned long  dwHelpStringContext;

    /* functions  */
    TLBFuncDesc * funclist;     /* linked list with function descriptions */

    /* variables  */
    TLBVarDesc * varlist;       /* linked list with variable descriptions */

    /* Implemented Interfaces  */
    TLBImplType * impltypelist;

    TLBRefType * reflist;
    int ctCustData;
    TLBCustData * pCustData;        /* linked list to cust data; */
    struct tagITypeInfoImpl * next;
} ITypeInfoImpl;

static struct ICOM_VTABLE(ITypeInfo2) tinfvt;

static ITypeInfo2 * WINAPI ITypeInfo_Constructor();

typedef struct tagTLBContext
{
	unsigned int oStart;  /* start of TLB in file */
	unsigned int pos;     /* current pos */
	unsigned int length;  /* total length */
	void *mapping;        /* memory mapping */
	MSFT_SegDir * pTblDir;
	ITypeLibImpl* pLibInfo;
} TLBContext;


static void MSFT_DoRefType(TLBContext *pcx, ITypeInfoImpl *pTI, int offset);

void create_typelibcache_cs(void)
{
    InitializeCriticalSection( &csTypeLibImpl );
    CRITICAL_SECTION_NAME( &csTypeLibImpl, "csTypeLibImpl" );
}

void destroy_typelibcache_cs(void)
{
    DeleteCriticalSection( &csTypeLibImpl );
}


/*
 debug
*/
static void dump_VarType(VARTYPE vt,char *szVarType) {
    /* FIXME : we could have better trace here, depending on the VARTYPE
     * of the variant
     */
    if (vt & VT_RESERVED)
	szVarType += strlen(strcpy(szVarType, "reserved | "));
    if (vt & VT_BYREF)
	szVarType += strlen(strcpy(szVarType, "ref to "));
    if (vt & VT_ARRAY)
	szVarType += strlen(strcpy(szVarType, "array of "));
    if (vt & VT_VECTOR)
	szVarType += strlen(strcpy(szVarType, "vector of "));
    switch(vt & VT_TYPEMASK) {
    case VT_UI1: sprintf(szVarType, "VT_UI"); break;
    case VT_I2: sprintf(szVarType, "VT_I2"); break;
    case VT_I4: sprintf(szVarType, "VT_I4"); break;
    case VT_R4: sprintf(szVarType, "VT_R4"); break;
    case VT_R8: sprintf(szVarType, "VT_R8"); break;
    case VT_BOOL: sprintf(szVarType, "VT_BOOL"); break;
    case VT_ERROR: sprintf(szVarType, "VT_ERROR"); break;
    case VT_CY: sprintf(szVarType, "VT_CY"); break;
    case VT_DATE: sprintf(szVarType, "VT_DATE"); break;
    case VT_BSTR: sprintf(szVarType, "VT_BSTR"); break;
    case VT_UNKNOWN: sprintf(szVarType, "VT_UNKNOWN"); break;
    case VT_DISPATCH: sprintf(szVarType, "VT_DISPATCH"); break;
    case VT_I1: sprintf(szVarType, "VT_I1"); break;
    case VT_UI2: sprintf(szVarType, "VT_UI2"); break;
    case VT_UI4: sprintf(szVarType, "VT_UI4"); break;
    case VT_INT: sprintf(szVarType, "VT_INT"); break;
    case VT_UINT: sprintf(szVarType, "VT_UINT"); break;
    case VT_VARIANT: sprintf(szVarType, "VT_VARIANT"); break;
    case VT_VOID: sprintf(szVarType, "VT_VOID"); break;
    case VT_USERDEFINED: sprintf(szVarType, "VT_USERDEFINED\n"); break;
    default: sprintf(szVarType, "unknown(%d)", vt & VT_TYPEMASK); break;
    }
}

static void dump_TypeDesc(TYPEDESC *pTD,char *szVarType) {
    if (pTD->vt & VT_RESERVED)
	szVarType += strlen(strcpy(szVarType, "reserved | "));
    if (pTD->vt & VT_BYREF)
	szVarType += strlen(strcpy(szVarType, "ref to "));
    if (pTD->vt & VT_ARRAY)
	szVarType += strlen(strcpy(szVarType, "array of "));
    if (pTD->vt & VT_VECTOR)
	szVarType += strlen(strcpy(szVarType, "vector of "));
    switch(pTD->vt & VT_TYPEMASK) {
    case VT_UI1: sprintf(szVarType, "VT_UI1"); break;
    case VT_I2: sprintf(szVarType, "VT_I2"); break;
    case VT_I4: sprintf(szVarType, "VT_I4"); break;
    case VT_R4: sprintf(szVarType, "VT_R4"); break;
    case VT_R8: sprintf(szVarType, "VT_R8"); break;
    case VT_BOOL: sprintf(szVarType, "VT_BOOL"); break;
    case VT_ERROR: sprintf(szVarType, "VT_ERROR"); break;
    case VT_CY: sprintf(szVarType, "VT_CY"); break;
    case VT_DATE: sprintf(szVarType, "VT_DATE"); break;
    case VT_BSTR: sprintf(szVarType, "VT_BSTR"); break;
    case VT_UNKNOWN: sprintf(szVarType, "VT_UNKNOWN"); break;
    case VT_DISPATCH: sprintf(szVarType, "VT_DISPATCH"); break;
    case VT_I1: sprintf(szVarType, "VT_I1"); break;
    case VT_UI2: sprintf(szVarType, "VT_UI2"); break;
    case VT_UI4: sprintf(szVarType, "VT_UI4"); break;
    case VT_INT: sprintf(szVarType, "VT_INT"); break;
    case VT_UINT: sprintf(szVarType, "VT_UINT"); break;
    case VT_VARIANT: sprintf(szVarType, "VT_VARIANT"); break;
    case VT_VOID: sprintf(szVarType, "VT_VOID"); break;
    case VT_USERDEFINED: sprintf(szVarType, "VT_USERDEFINED ref = %lx",
				 pTD->u.hreftype); break;
    case VT_PTR: sprintf(szVarType, "ptr to ");
      dump_TypeDesc(pTD->u.lptdesc, szVarType + 7);
      break;
    case VT_SAFEARRAY: sprintf(szVarType, "safearray of ");
      dump_TypeDesc(pTD->u.lptdesc, szVarType + 13);
      break;
    case VT_CARRAY: sprintf(szVarType, "%d dim array of ",
			    pTD->u.lpadesc->cDims); /* FIXME print out sizes */
      dump_TypeDesc(&pTD->u.lpadesc->tdescElem, szVarType + strlen(szVarType));
      break;

    default: sprintf(szVarType, "unknown(%d)", pTD->vt & VT_TYPEMASK); break;
    }
}

static void dump_ELEMDESC(ELEMDESC *edesc) {
  char buf[200];
  dump_TypeDesc(&edesc->tdesc,buf);
  MESSAGE("\t\ttdesc.vartype %d (%s)\n",edesc->tdesc.vt,buf);
  MESSAGE("\t\tu.parmadesc.flags %x\n",edesc->u.paramdesc.wParamFlags);
  MESSAGE("\t\tu.parmadesc.lpex %p\n",edesc->u.paramdesc.pparamdescex);
}
static void dump_FUNCDESC(FUNCDESC *funcdesc) {
  int i;
  MESSAGE("memid is %08lx\n",funcdesc->memid);
  for (i=0;i<funcdesc->cParams;i++) {
      MESSAGE("Param %d:\n",i);
      dump_ELEMDESC(funcdesc->lprgelemdescParam+i);
  }
  MESSAGE("\tfunckind: %d (",funcdesc->funckind);
  switch (funcdesc->funckind) {
  case FUNC_VIRTUAL: MESSAGE("virtual");break;
  case FUNC_PUREVIRTUAL: MESSAGE("pure virtual");break;
  case FUNC_NONVIRTUAL: MESSAGE("nonvirtual");break;
  case FUNC_STATIC: MESSAGE("static");break;
  case FUNC_DISPATCH: MESSAGE("dispatch");break;
  default: MESSAGE("unknown");break;
  }
  MESSAGE(")\n\tinvkind: %d (",funcdesc->invkind);
  switch (funcdesc->invkind) {
  case INVOKE_FUNC: MESSAGE("func");break;
  case INVOKE_PROPERTYGET: MESSAGE("property get");break;
  case INVOKE_PROPERTYPUT: MESSAGE("property put");break;
  case INVOKE_PROPERTYPUTREF: MESSAGE("property put ref");break;
  }
  MESSAGE(")\n\tcallconv: %d (",funcdesc->callconv);
  switch (funcdesc->callconv) {
  case CC_CDECL: MESSAGE("cdecl");break;
  case CC_PASCAL: MESSAGE("pascal");break;
  case CC_STDCALL: MESSAGE("stdcall");break;
  case CC_SYSCALL: MESSAGE("syscall");break;
  default:break;
  }
  MESSAGE(")\n\toVft: %d\n", funcdesc->oVft);
  MESSAGE("\tcParamsOpt: %d\n", funcdesc->cParamsOpt);
  MESSAGE("\twFlags: %x\n", funcdesc->wFuncFlags);
}
static void dump_TLBFuncDescOne(TLBFuncDesc * pfd)
{
  int i;
  if (!TRACE_ON(typelib))
      return;
  MESSAGE("%s(%u)\n", debugstr_w(pfd->Name), pfd->funcdesc.cParams);
  for (i=0;i<pfd->funcdesc.cParams;i++)
      MESSAGE("\tparm%d: %s\n",i,debugstr_w(pfd->pParamDesc[i].Name));


  dump_FUNCDESC(&(pfd->funcdesc));

  MESSAGE("\thelpstring: %s\n", debugstr_w(pfd->HelpString));
  MESSAGE("\tentry: %s\n", debugstr_w(pfd->Entry));
}
static void dump_TLBFuncDesc(TLBFuncDesc * pfd)
{
	while (pfd)
	{
	  dump_TLBFuncDescOne(pfd);
	  pfd = pfd->next;
	};
}
static void dump_TLBVarDesc(TLBVarDesc * pvd)
{
	while (pvd)
	{
	  TRACE_(typelib)("%s\n", debugstr_w(pvd->Name));
	  pvd = pvd->next;
	};
}

static void dump_TLBImpLib(TLBImpLib *import)
{
    TRACE_(typelib)("%s %s\n", debugstr_guid(&(import->guid)),
		    debugstr_w(import->name));
    TRACE_(typelib)("v%d.%d lcid=%lx offset=%x\n", import->wVersionMajor,
		    import->wVersionMinor, import->lcid, import->offset);
}

static void dump_TLBRefType(TLBRefType * prt)
{
	while (prt)
	{
	  TRACE_(typelib)("href:0x%08lx\n", prt->reference);
	  if(prt->index == -1)
	    TRACE_(typelib)("%s\n", debugstr_guid(&(prt->guid)));
	  else
	    TRACE_(typelib)("type no: %d\n", prt->index);

	  if(prt->pImpTLInfo != TLB_REF_INTERNAL &&
	     prt->pImpTLInfo != TLB_REF_NOT_FOUND) {
	      TRACE_(typelib)("in lib\n");
	      dump_TLBImpLib(prt->pImpTLInfo);
	  }
	  prt = prt->next;
	};
}

static void dump_TLBImplType(TLBImplType * impl)
{
    while (impl) {
        TRACE_(typelib)(
		"implementing/inheriting interface hRef = %lx implflags %x\n",
		impl->hRef, impl->implflags);
	impl = impl->next;
    }
}

static void dump_Variant(VARIANT * pvar)
{
    char szVarType[32];
    LPVOID ref;

    TRACE("(%p)\n", pvar);

    if (!pvar)  return;

    ZeroMemory(szVarType, sizeof(szVarType));

    /* FIXME : we could have better trace here, depending on the VARTYPE
     * of the variant
     */
    dump_VarType(V_VT(pvar),szVarType);

    TRACE("VARTYPE: %s\n", szVarType);

    if (V_VT(pvar) & VT_BYREF) {
      ref = V_UNION(pvar, byref);
      TRACE("%p\n", ref);
    }
    else ref = &V_UNION(pvar, cVal);

    if (V_VT(pvar) & VT_ARRAY) {
      /* FIXME */
      return;
    }
    if (V_VT(pvar) & VT_VECTOR) {
      /* FIXME */
      return;
    }

    switch (V_VT(pvar) & VT_TYPEMASK)
    {
        case VT_I2:
            TRACE("%d\n", *(short*)ref);
            break;

        case VT_I4:
            TRACE("%d\n", *(INT*)ref);
            break;

        case VT_R4:
            TRACE("%3.3e\n", *(float*)ref);
            break;

        case VT_R8:
            TRACE("%3.3e\n", *(double*)ref);
            break;

        case VT_BOOL:
            TRACE("%s\n", *(VARIANT_BOOL*)ref ? "TRUE" : "FALSE");
            break;

        case VT_BSTR:
            TRACE("%s\n", debugstr_w(*(BSTR*)ref));
            break;

        case VT_UNKNOWN:
        case VT_DISPATCH:
            TRACE("%p\n", *(LPVOID*)ref);
            break;

        case VT_VARIANT:
            if (V_VT(pvar) & VT_BYREF) dump_Variant(ref);
            break;

        default:
            TRACE("(?)%ld\n", *(long*)ref);
            break;
    }
}

static void dump_DispParms(DISPPARAMS * pdp)
{
    int index = 0;

    TRACE("args=%u named args=%u\n", pdp->cArgs, pdp->cNamedArgs);

    while (index < pdp->cArgs)
    {
        dump_Variant( &pdp->rgvarg[index] );
        ++index;
    }
}

static char * typekind_desc[] =
{
	"TKIND_ENUM",
	"TKIND_RECORD",
	"TKIND_MODULE",
	"TKIND_INTERFACE",
	"TKIND_DISPATCH",
	"TKIND_COCLASS",
	"TKIND_ALIAS",
	"TKIND_UNION",
	"TKIND_MAX"
};

static void dump_TypeInfo(ITypeInfoImpl * pty)
{
    TRACE("%p ref=%u\n", pty, pty->ref);
    TRACE("attr:%s\n", debugstr_guid(&(pty->TypeAttr.guid)));
    TRACE("kind:%s\n", typekind_desc[pty->TypeAttr.typekind]);
    TRACE("fct:%u var:%u impl:%u\n",
      pty->TypeAttr.cFuncs, pty->TypeAttr.cVars, pty->TypeAttr.cImplTypes);
    TRACE("parent tlb:%p index in TLB:%u\n",pty->pTypeLib, pty->index);
    TRACE("%s %s\n", debugstr_w(pty->Name), debugstr_w(pty->DocString));
    dump_TLBFuncDesc(pty->funclist);
    dump_TLBVarDesc(pty->varlist);
    dump_TLBImplType(pty->impltypelist);
}

static TYPEDESC stndTypeDesc[VT_LPWSTR+1]=
{
    /* VT_LPWSTR is largest type that */
    /* may appear in type description*/
    {{0}, 0},{{0}, 1},{{0}, 2},{{0}, 3},{{0}, 4},
    {{0}, 5},{{0}, 6},{{0}, 7},{{0}, 8},{{0}, 9},
    {{0},10},{{0},11},{{0},12},{{0},13},{{0},14},
    {{0},15},{{0},16},{{0},17},{{0},18},{{0},19},
    {{0},20},{{0},21},{{0},22},{{0},23},{{0},24},
    {{0},25},{{0},26},{{0},27},{{0},28},{{0},29},
    {{0},30},{{0},31}
};

static void TLB_abort()
{
    DebugBreak();
}
static void * TLB_Alloc(unsigned size)
{
    void * ret;
    if((ret=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size))==NULL){
        /* FIXME */
        ERR("cannot allocate memory\n");
    }
    return ret;
}

static void TLB_Free(void * ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}


/**********************************************************************
 *
 *  Functions for reading MSFT typelibs (those created by CreateTypeLib2)
 */
/* read function */
DWORD MSFT_Read(void *buffer,  DWORD count, TLBContext *pcx, long where )
{
    TRACE_(typelib)("pos=0x%08x len=0x%08lx 0x%08x 0x%08x 0x%08lx\n",
       pcx->pos, count, pcx->oStart, pcx->length, where);

    if (where != DO_NOT_SEEK)
    {
        where += pcx->oStart;
        if (where > pcx->length)
        {
            /* FIXME */
            ERR("seek beyond end (%ld/%d)\n", where, pcx->length );
            TLB_abort();
        }
        pcx->pos = where;
    }
    if (pcx->pos + count > pcx->length) count = pcx->length - pcx->pos;
    memcpy( buffer, (char *)pcx->mapping + pcx->pos, count );
    pcx->pos += count;
    return count;
}

static void MSFT_ReadGuid( GUID *pGuid, int offset, TLBContext *pcx)
{
    TRACE_(typelib)("%s\n", debugstr_guid(pGuid));

    if(offset<0 || pcx->pTblDir->pGuidTab.offset <0){
        memset(pGuid,0, sizeof(GUID));
        return;
    }
    MSFT_Read(pGuid, sizeof(GUID), pcx, pcx->pTblDir->pGuidTab.offset+offset );
}

BSTR MSFT_ReadName( TLBContext *pcx, int offset)
{
    char * name;
    MSFT_NameIntro niName;
    int lengthInChars;
    WCHAR* pwstring = NULL;
    BSTR bstrName = NULL;

    MSFT_Read(&niName, sizeof(niName), pcx,
				pcx->pTblDir->pNametab.offset+offset);
    niName.namelen &= 0xFF; /* FIXME: correct ? */
    name=TLB_Alloc((niName.namelen & 0xff) +1);
    MSFT_Read(name, (niName.namelen & 0xff), pcx, DO_NOT_SEEK);
    name[niName.namelen & 0xff]='\0';

    lengthInChars = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                                        name, -1, NULL, 0);

    /* no invalid characters in string */
    if (lengthInChars)
    {
        pwstring = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR)*lengthInChars);

        /* don't check for invalid character since this has been done previously */
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, pwstring, lengthInChars);

        bstrName = SysAllocStringLen(pwstring, lengthInChars);
        lengthInChars = SysStringLen(bstrName);
        HeapFree(GetProcessHeap(), 0, pwstring);
    }

    TRACE_(typelib)("%s %d\n", debugstr_w(bstrName), lengthInChars);
    return bstrName;
}

BSTR MSFT_ReadString( TLBContext *pcx, int offset)
{
    char * string;
    INT16 length;
    int lengthInChars;
    BSTR bstr = NULL;

    if(offset<0) return NULL;
    MSFT_Read(&length, sizeof(INT16), pcx, pcx->pTblDir->pStringtab.offset+offset);
    if(length <= 0) return 0;
    string=TLB_Alloc(length +1);
    MSFT_Read(string, length, pcx, DO_NOT_SEEK);
    string[length]='\0';

    lengthInChars = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                                        string, -1, NULL, 0);

    /* no invalid characters in string */
    if (lengthInChars)
    {
        WCHAR* pwstring = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR)*lengthInChars);

        /* don't check for invalid character since this has been done previously */
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, string, -1, pwstring, lengthInChars);

        bstr = SysAllocStringLen(pwstring, lengthInChars);
        lengthInChars = SysStringLen(bstr);
        HeapFree(GetProcessHeap(), 0, pwstring);
    }

    TRACE_(typelib)("%s %d\n", debugstr_w(bstr), lengthInChars);
    return bstr;
}
/*
 * read a value and fill a VARIANT structure
 */
static void MSFT_ReadValue( VARIANT * pVar, int offset, TLBContext *pcx )
{
    int size;

    TRACE_(typelib)("\n");

    if(offset <0) { /* data are packed in here */
        V_VT(pVar) = (offset & 0x7c000000 )>> 26;
        V_UNION(pVar, iVal) = offset & 0xffff;
        return;
    }
    MSFT_Read(&(V_VT(pVar)), sizeof(VARTYPE), pcx,
        pcx->pTblDir->pCustData.offset + offset );
    TRACE_(typelib)("Vartype = %x\n", V_VT(pVar));
    switch (V_VT(pVar)){
        case VT_EMPTY:  /* FIXME: is this right? */
        case VT_NULL:   /* FIXME: is this right? */
        case VT_I2  :   /* this should not happen */
        case VT_I4  :
        case VT_R4  :
        case VT_ERROR   :
        case VT_BOOL    :
        case VT_I1  :
        case VT_UI1 :
        case VT_UI2 :
        case VT_UI4 :
        case VT_INT :
        case VT_UINT    :
        case VT_VOID    : /* FIXME: is this right? */
        case VT_HRESULT :
            size=4; break;
        case VT_R8  :
        case VT_CY  :
        case VT_DATE    :
        case VT_I8  :
        case VT_UI8 :
        case VT_DECIMAL :  /* FIXME: is this right? */
        case VT_FILETIME :
            size=8;break;
            /* pointer types with known behaviour */
        case VT_BSTR    :{
            char * ptr;
            MSFT_Read(&size, sizeof(INT), pcx, DO_NOT_SEEK );
	    if(size <= 0) {
	        FIXME("BSTR length = %d?\n", size);
	    } else {
                ptr=TLB_Alloc(size);/* allocate temp buffer */
		MSFT_Read(ptr, size, pcx, DO_NOT_SEEK);/* read string (ANSI) */
		V_UNION(pVar, bstrVal)=SysAllocStringLen(NULL,size);
		/* FIXME: do we need a AtoW conversion here? */
		V_UNION(pVar, bstrVal[size])=L'\0';
		while(size--) V_UNION(pVar, bstrVal[size])=ptr[size];
		TLB_Free(ptr);
	    }
	}
	size=-4; break;
    /* FIXME: this will not work AT ALL when the variant contains a pointer */
        case VT_DISPATCH :
        case VT_VARIANT :
        case VT_UNKNOWN :
        case VT_PTR :
        case VT_SAFEARRAY :
        case VT_CARRAY  :
        case VT_USERDEFINED :
        case VT_LPSTR   :
        case VT_LPWSTR  :
        case VT_BLOB    :
        case VT_STREAM  :
        case VT_STORAGE :
        case VT_STREAMED_OBJECT :
        case VT_STORED_OBJECT   :
        case VT_BLOB_OBJECT :
        case VT_CF  :
        case VT_CLSID   :
        default:
            size=0;
            FIXME("VARTYPE %d is not supported, setting pointer to NULL\n",
                V_VT(pVar));
    }

    if(size>0) /* (big|small) endian correct? */
        MSFT_Read(&(V_UNION(pVar, iVal)), size, pcx, DO_NOT_SEEK );
    return;
}
/*
 * create a linked list with custom data
 */
static int MSFT_CustData( TLBContext *pcx, int offset, TLBCustData** ppCustData )
{
    MSFT_CDGuid entry;
    TLBCustData* pNew;
    int count=0;

    TRACE_(typelib)("\n");

    while(offset >=0){
        count++;
        pNew=TLB_Alloc(sizeof(TLBCustData));
        MSFT_Read(&entry, sizeof(entry), pcx,
            pcx->pTblDir->pCDGuids.offset+offset);
        MSFT_ReadGuid(&(pNew->guid), entry.GuidOffset , pcx);
        MSFT_ReadValue(&(pNew->data), entry.DataOffset, pcx);
        /* add new custom data at head of the list */
        pNew->next=*ppCustData;
        *ppCustData=pNew;
        offset = entry.next;
    }
    return count;
}

static void MSFT_GetTdesc(TLBContext *pcx, INT type, TYPEDESC *pTd,
			  ITypeInfoImpl *pTI)
{
    if(type <0)
        pTd->vt=type & VT_TYPEMASK;
    else
        *pTd=pcx->pLibInfo->pTypeDesc[type/(2*sizeof(INT))];

    if(pTd->vt == VT_USERDEFINED)
      MSFT_DoRefType(pcx, pTI, pTd->u.hreftype);

    TRACE_(typelib)("vt type = %X\n", pTd->vt);
}

static void
MSFT_DoFuncs(TLBContext*     pcx,
	    ITypeInfoImpl*  pTI,
            int             cFuncs,
            int             cVars,
            int             offset,
            TLBFuncDesc**   pptfd)
{
    /*
     * member information is stored in a data structure at offset
     * indicated by the memoffset field of the typeinfo structure
     * There are several distinctive parts.
     * the first part starts with a field that holds the total length
     * of this (first) part excluding this field. Then follow the records,
     * for each member there is one record.
     *
     * First entry is always the length of the record (excluding this
     * length word).
     * Rest of the record depends on the type of the member. If there is
     * a field indicating the member type (function variable intereface etc)
     * I have not found it yet. At this time we depend on the information
     * in the type info and the usual order how things are stored.
     *
     * Second follows an array sized nrMEM*sizeof(INT) with a memeber id
     * for each member;
     *
     * Third is a equal sized array with file offsets to the name entry
     * of each member.
     *
     * Forth and last (?) part is an array with offsets to the records in the
     * first part of this file segment.
     */

    int infolen, nameoffset, reclength, nrattributes, i;
    int recoffset = offset + sizeof(INT);

    char recbuf[512];
    MSFT_FuncRecord * pFuncRec=(MSFT_FuncRecord *) recbuf;

    TRACE_(typelib)("\n");

    MSFT_Read(&infolen, sizeof(INT), pcx, offset);

    for ( i = 0; i < cFuncs ; i++ )
    {
        *pptfd = TLB_Alloc(sizeof(TLBFuncDesc));

        /* name, eventually add to a hash table */
        MSFT_Read(&nameoffset,
                 sizeof(INT),
                 pcx,
                 offset + infolen + (cFuncs + cVars + i + 1) * sizeof(INT));

        (*pptfd)->Name = MSFT_ReadName(pcx, nameoffset);

        /* read the function information record */
        MSFT_Read(&reclength, sizeof(INT), pcx, recoffset);

        reclength &= 0x1ff;

        MSFT_Read(pFuncRec, reclength - sizeof(INT), pcx, DO_NOT_SEEK) ;

        /* do the attributes */
        nrattributes = (reclength - pFuncRec->nrargs * 3 * sizeof(int) - 0x18)
                       / sizeof(int);

        if ( nrattributes > 0 )
        {
            (*pptfd)->helpcontext = pFuncRec->OptAttr[0] ;

            if ( nrattributes > 1 )
            {
                (*pptfd)->HelpString = MSFT_ReadString(pcx,
                                                      pFuncRec->OptAttr[1]) ;

                if ( nrattributes > 2 )
                {
                    if ( pFuncRec->FKCCIC & 0x2000 )
                    {
                       (*pptfd)->Entry = (WCHAR*) pFuncRec->OptAttr[2] ;
                    }
                    else
                    {
                        (*pptfd)->Entry = MSFT_ReadString(pcx,
                                                         pFuncRec->OptAttr[2]);
                    }
                    if( nrattributes > 5 )
                    {
                        (*pptfd)->HelpStringContext = pFuncRec->OptAttr[5] ;

                        if ( nrattributes > 6 && pFuncRec->FKCCIC & 0x80 )
                        {
                            MSFT_CustData(pcx,
					  pFuncRec->OptAttr[6],
					  &(*pptfd)->pCustData);
                        }
                    }
                }
            }
        }

        /* fill the FuncDesc Structure */
        MSFT_Read( & (*pptfd)->funcdesc.memid,
                  sizeof(INT), pcx,
                  offset + infolen + ( i + 1) * sizeof(INT));

        (*pptfd)->funcdesc.funckind   =  (pFuncRec->FKCCIC)      & 0x7;
        (*pptfd)->funcdesc.invkind    =  (pFuncRec->FKCCIC) >> 3 & 0xF;
        (*pptfd)->funcdesc.callconv   =  (pFuncRec->FKCCIC) >> 8 & 0xF;
        (*pptfd)->funcdesc.cParams    =   pFuncRec->nrargs  ;
        (*pptfd)->funcdesc.cParamsOpt =   pFuncRec->nroargs ;
        (*pptfd)->funcdesc.oVft       =   pFuncRec->VtableOffset ;
        (*pptfd)->funcdesc.wFuncFlags =   LOWORD(pFuncRec->Flags) ;

        MSFT_GetTdesc(pcx,
		      pFuncRec->DataType,
		      &(*pptfd)->funcdesc.elemdescFunc.tdesc,
		      pTI);

        /* do the parameters/arguments */
        if(pFuncRec->nrargs)
        {
            int j = 0;
            MSFT_ParameterInfo paraminfo;

            (*pptfd)->funcdesc.lprgelemdescParam =
                TLB_Alloc(pFuncRec->nrargs * sizeof(ELEMDESC));

            (*pptfd)->pParamDesc =
                TLB_Alloc(pFuncRec->nrargs * sizeof(TLBParDesc));

            MSFT_Read(&paraminfo,
		      sizeof(paraminfo),
		      pcx,
		      recoffset + reclength -
		      pFuncRec->nrargs * sizeof(MSFT_ParameterInfo));

            for ( j = 0 ; j < pFuncRec->nrargs ; j++ )
            {
                TYPEDESC* lpArgTypeDesc = 0;

                MSFT_GetTdesc(pcx,
			      paraminfo.DataType,
			      &(*pptfd)->funcdesc.lprgelemdescParam[j].tdesc,
			      pTI);

                (*pptfd)->funcdesc.lprgelemdescParam[j].u.paramdesc.wParamFlags = paraminfo.Flags;

                (*pptfd)->pParamDesc[j].Name = (void *) paraminfo.oName;

                /* SEEK value = jump to offset,
                 * from there jump to the end of record,
                 * go back by (j-1) arguments
                 */
                MSFT_Read( &paraminfo ,
			   sizeof(MSFT_ParameterInfo), pcx,
			   recoffset + reclength - ((pFuncRec->nrargs - j - 1)
					       * sizeof(MSFT_ParameterInfo)));
                lpArgTypeDesc =
                    & ((*pptfd)->funcdesc.lprgelemdescParam[j].tdesc);

                while ( lpArgTypeDesc != NULL )
                {
                    switch ( lpArgTypeDesc->vt )
                    {
                    case VT_PTR:
                        lpArgTypeDesc = lpArgTypeDesc->u.lptdesc;
                        break;

                    case VT_CARRAY:
                        lpArgTypeDesc = & (lpArgTypeDesc->u.lpadesc->tdescElem);
                        break;

                    case VT_USERDEFINED:
                        MSFT_DoRefType(pcx, pTI,
				       lpArgTypeDesc->u.hreftype);

                        lpArgTypeDesc = NULL;
                        break;

                    default:
                        lpArgTypeDesc = NULL;
                    }
                }
            }


            /* parameter is the return value! */
            if ( paraminfo.Flags & PARAMFLAG_FRETVAL )
            {
                TYPEDESC* lpArgTypeDesc;

                (*pptfd)->funcdesc.elemdescFunc =
                (*pptfd)->funcdesc.lprgelemdescParam[j];

                lpArgTypeDesc = & ((*pptfd)->funcdesc.elemdescFunc.tdesc) ;

                while ( lpArgTypeDesc != NULL )
                {
                    switch ( lpArgTypeDesc->vt )
                    {
                    case VT_PTR:
                        lpArgTypeDesc = lpArgTypeDesc->u.lptdesc;
                        break;
                    case VT_CARRAY:
                        lpArgTypeDesc =
                        & (lpArgTypeDesc->u.lpadesc->tdescElem);

                        break;

                    case VT_USERDEFINED:
                        MSFT_DoRefType(pcx,
				       pTI,
				       lpArgTypeDesc->u.hreftype);

                        lpArgTypeDesc = NULL;
                        break;

                    default:
                        lpArgTypeDesc = NULL;
                    }
                }
            }

            /* second time around */
            for(j=0;j<pFuncRec->nrargs;j++)
            {
                /* name */
                (*pptfd)->pParamDesc[j].Name =
                    MSFT_ReadName( pcx, (int)(*pptfd)->pParamDesc[j].Name );

                /* default value */
                if ( (PARAMFLAG_FHASDEFAULT &
                      (*pptfd)->funcdesc.lprgelemdescParam[j].u.paramdesc.wParamFlags) &&
                     ((pFuncRec->FKCCIC) & 0x1000) )
                {
                    INT* pInt = (INT *)((char *)pFuncRec +
                                   reclength -
                                   (pFuncRec->nrargs * 4 + 1) * sizeof(INT) );

                    PARAMDESC* pParamDesc = & (*pptfd)->funcdesc.lprgelemdescParam[j].u.paramdesc;

                    pParamDesc->pparamdescex = TLB_Alloc(sizeof(PARAMDESCEX));
                    pParamDesc->pparamdescex->cBytes = sizeof(PARAMDESCEX);

		    MSFT_ReadValue(&(pParamDesc->pparamdescex->varDefaultValue),
                        pInt[j], pcx);
                }
                /* custom info */
                if ( nrattributes > 7 + j && pFuncRec->FKCCIC & 0x80 )
                {
                    MSFT_CustData(pcx,
				  pFuncRec->OptAttr[7+j],
				  &(*pptfd)->pParamDesc[j].pCustData);
                }
           }
        }

        /* scode is not used: archaic win16 stuff FIXME: right? */
        (*pptfd)->funcdesc.cScodes   = 0 ;
        (*pptfd)->funcdesc.lprgscode = NULL ;

        pptfd      = & ((*pptfd)->next);
        recoffset += reclength;
    }
}
static void MSFT_DoVars(TLBContext *pcx, ITypeInfoImpl *pTI, int cFuncs,
		       int cVars, int offset, TLBVarDesc ** pptvd)
{
    int infolen, nameoffset, reclength;
    char recbuf[256];
    MSFT_VarRecord * pVarRec=(MSFT_VarRecord *) recbuf;
    int i;
    int recoffset;

    TRACE_(typelib)("\n");

    MSFT_Read(&infolen,sizeof(INT), pcx, offset);
    MSFT_Read(&recoffset,sizeof(INT), pcx, offset + infolen +
        ((cFuncs+cVars)*2+cFuncs + 1)*sizeof(INT));
    recoffset += offset+sizeof(INT);
    for(i=0;i<cVars;i++){
        *pptvd=TLB_Alloc(sizeof(TLBVarDesc));
    /* name, eventually add to a hash table */
        MSFT_Read(&nameoffset, sizeof(INT), pcx,
            offset + infolen + (cFuncs + cVars + i + 1) * sizeof(INT));
        (*pptvd)->Name=MSFT_ReadName(pcx, nameoffset);
    /* read the variable information record */
        MSFT_Read(&reclength, sizeof(INT), pcx, recoffset);
        reclength &=0xff;
        MSFT_Read(pVarRec, reclength - sizeof(INT), pcx, DO_NOT_SEEK) ;
    /* Optional data */
        if(reclength >(6*sizeof(INT)) )
            (*pptvd)->HelpContext=pVarRec->HelpContext;
        if(reclength >(7*sizeof(INT)) )
            (*pptvd)->HelpString = MSFT_ReadString(pcx, pVarRec->oHelpString) ;
        if(reclength >(8*sizeof(INT)) )
        if(reclength >(9*sizeof(INT)) )
            (*pptvd)->HelpStringContext=pVarRec->HelpStringContext;
    /* fill the VarDesc Structure */
        MSFT_Read(&(*pptvd)->vardesc.memid, sizeof(INT), pcx,
            offset + infolen + ( i + 1) * sizeof(INT));
        (*pptvd)->vardesc.varkind = pVarRec->VarKind;
        (*pptvd)->vardesc.wVarFlags = pVarRec->Flags;
        MSFT_GetTdesc(pcx, pVarRec->DataType,
            &(*pptvd)->vardesc.elemdescVar.tdesc, pTI);
/*   (*pptvd)->vardesc.lpstrSchema; is reserved (SDK) FIXME?? */
        if(pVarRec->VarKind == VAR_CONST ){
            (*pptvd)->vardesc.u.lpvarValue=TLB_Alloc(sizeof(VARIANT));
            MSFT_ReadValue((*pptvd)->vardesc.u.lpvarValue,
                pVarRec->OffsValue, pcx);
        } else
            (*pptvd)->vardesc.u.oInst=pVarRec->OffsValue;
        pptvd=&((*pptvd)->next);
        recoffset += reclength;
    }
}
/* fill in data for a hreftype (offset). When the refernced type is contained
 * in the typelib, it's just an (file) offset in the type info base dir.
 * If comes from import, it's an offset+1 in the ImpInfo table
 * */
static void MSFT_DoRefType(TLBContext *pcx, ITypeInfoImpl *pTI,
                          int offset)
{
    int j;
    TLBRefType **ppRefType = &pTI->reflist;

    TRACE_(typelib)("TLB context %p, TLB offset %x\n", pcx, offset);

    while(*ppRefType) {
        if((*ppRefType)->reference == offset)
	    return;
	ppRefType = &(*ppRefType)->next;
    }

    *ppRefType = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			   sizeof(**ppRefType));

    if(!MSFT_HREFTYPE_INTHISFILE( offset)) {
        /* external typelib */
        MSFT_ImpInfo impinfo;
        TLBImpLib *pImpLib=(pcx->pLibInfo->pImpLibs);

        TRACE_(typelib)("offset %x, masked offset %x\n", offset, offset + (offset & 0xfffffffc));

        MSFT_Read(&impinfo, sizeof(impinfo), pcx,
            pcx->pTblDir->pImpInfo.offset + (offset & 0xfffffffc));
        for(j=0;pImpLib;j++){   /* search the known offsets of all import libraries */
            if(pImpLib->offset==impinfo.oImpFile) break;
            pImpLib=pImpLib->next;
        }
        if(pImpLib){
            (*ppRefType)->reference=offset;
            (*ppRefType)->pImpTLInfo = pImpLib;
            MSFT_ReadGuid(&(*ppRefType)->guid, impinfo.oGuid, pcx);
	    (*ppRefType)->index = TLB_REF_USE_GUID;
        }else{
            ERR("Cannot find a reference\n");
            (*ppRefType)->reference=-1;
            (*ppRefType)->pImpTLInfo=TLB_REF_NOT_FOUND;
        }
    }else{
        /* in this typelib */
        (*ppRefType)->index = MSFT_HREFTYPE_INDEX(offset);
        (*ppRefType)->reference=offset;
        (*ppRefType)->pImpTLInfo=TLB_REF_INTERNAL;
    }
}

/* process Implemented Interfaces of a com class */
static void MSFT_DoImplTypes(TLBContext *pcx, ITypeInfoImpl *pTI, int count,
			    int offset)
{
    int i;
    MSFT_RefRecord refrec;
    TLBImplType **ppImpl = &pTI->impltypelist;

    TRACE_(typelib)("\n");

    for(i=0;i<count;i++){
        if(offset<0) break; /* paranoia */
        *ppImpl=TLB_Alloc(sizeof(**ppImpl));
        MSFT_Read(&refrec,sizeof(refrec),pcx,offset+pcx->pTblDir->pRefTab.offset);
        MSFT_DoRefType(pcx, pTI, refrec.reftype);
	(*ppImpl)->hRef = refrec.reftype;
	(*ppImpl)->implflags=refrec.flags;
        (*ppImpl)->ctCustData=
            MSFT_CustData(pcx, refrec.oCustData, &(*ppImpl)->pCustData);
        offset=refrec.onext;
        ppImpl=&((*ppImpl)->next);
    }
}
/*
 * process a typeinfo record
 */
ITypeInfoImpl * MSFT_DoTypeInfo(
    TLBContext *pcx,
    int count,
    ITypeLibImpl * pLibInfo)
{
    MSFT_TypeInfoBase tiBase;
    ITypeInfoImpl *ptiRet;

    TRACE_(typelib)("count=%u\n", count);

    ptiRet = (ITypeInfoImpl*) ITypeInfo_Constructor();
    MSFT_Read(&tiBase, sizeof(tiBase) ,pcx ,
        pcx->pTblDir->pTypeInfoTab.offset+count*sizeof(tiBase));
/* this is where we are coming from */
    ptiRet->pTypeLib = pLibInfo;
    ITypeLib2_AddRef((ITypeLib2 *)pLibInfo);
    ptiRet->index=count;
/* fill in the typeattr fields */
    FIXME("Assign constructor/destructor memid\n");

    MSFT_ReadGuid(&ptiRet->TypeAttr.guid, tiBase.posguid, pcx);
    ptiRet->TypeAttr.lcid=pLibInfo->LibAttr.lcid;   /* FIXME: correct? */
    ptiRet->TypeAttr.memidConstructor=MEMBERID_NIL ;/* FIXME */
    ptiRet->TypeAttr.memidDestructor=MEMBERID_NIL ; /* FIXME */
    ptiRet->TypeAttr.lpstrSchema=NULL;              /* reserved */
    ptiRet->TypeAttr.cbSizeInstance=tiBase.size;
    ptiRet->TypeAttr.typekind=tiBase.typekind & 0xF;
    ptiRet->TypeAttr.cFuncs=LOWORD(tiBase.cElement);
    ptiRet->TypeAttr.cVars=HIWORD(tiBase.cElement);
    ptiRet->TypeAttr.cbAlignment=(tiBase.typekind >> 11 )& 0x1F; /* there are more flags there */
    ptiRet->TypeAttr.wTypeFlags=tiBase.flags;
    ptiRet->TypeAttr.wMajorVerNum=LOWORD(tiBase.version);
    ptiRet->TypeAttr.wMinorVerNum=HIWORD(tiBase.version);
    ptiRet->TypeAttr.cImplTypes=tiBase.cImplTypes;
    ptiRet->TypeAttr.cbSizeVft=tiBase.cbSizeVft; /* FIXME: this is only the non inherited part */
    if(ptiRet->TypeAttr.typekind == TKIND_ALIAS)
        MSFT_GetTdesc(pcx, tiBase.datatype1,
            &ptiRet->TypeAttr.tdescAlias, ptiRet);

/*  FIXME: */
/*    IDLDESC  idldescType; *//* never saw this one != zero  */

/* name, eventually add to a hash table */
    ptiRet->Name=MSFT_ReadName(pcx, tiBase.NameOffset);
    TRACE_(typelib)("reading %s\n", debugstr_w(ptiRet->Name));
    /* help info */
    ptiRet->DocString=MSFT_ReadString(pcx, tiBase.docstringoffs);
    ptiRet->dwHelpStringContext=tiBase.helpstringcontext;
    ptiRet->dwHelpContext=tiBase.helpcontext;
/* note: InfoType's Help file and HelpStringDll come from the containing
 * library. Further HelpString and Docstring appear to be the same thing :(
 */
    /* functions */
    if(ptiRet->TypeAttr.cFuncs >0 )
        MSFT_DoFuncs(pcx, ptiRet, ptiRet->TypeAttr.cFuncs,
		    ptiRet->TypeAttr.cVars,
		    tiBase.memoffset, & ptiRet->funclist);
    /* variables */
    if(ptiRet->TypeAttr.cVars >0 )
        MSFT_DoVars(pcx, ptiRet, ptiRet->TypeAttr.cFuncs,
		   ptiRet->TypeAttr.cVars,
		   tiBase.memoffset, & ptiRet->varlist);
    if(ptiRet->TypeAttr.cImplTypes >0 ) {
        switch(ptiRet->TypeAttr.typekind)
        {
        case TKIND_COCLASS:
            MSFT_DoImplTypes(pcx, ptiRet, ptiRet->TypeAttr.cImplTypes ,
                tiBase.datatype1);
            break;
        case TKIND_DISPATCH:
            ptiRet->impltypelist=TLB_Alloc(sizeof(TLBImplType));

            if (tiBase.datatype1 != -1)
            {
              MSFT_DoRefType(pcx, ptiRet, tiBase.datatype1);
	      ptiRet->impltypelist->hRef = tiBase.datatype1;
            }
            else
	    { /* FIXME: This is a really bad hack to add IDispatch */
              char* szStdOle     = "stdole2.tlb\0";
              int   nStdOleLen = strlen(szStdOle);
	      TLBRefType **ppRef = &ptiRet->reflist;

	      while(*ppRef) {
		if((*ppRef)->reference == -1)
		  break;
		ppRef = &(*ppRef)->next;
	      }
	      if(!*ppRef) {
		*ppRef = TLB_Alloc(sizeof(**ppRef));
		(*ppRef)->guid             = IID_IDispatch;
		(*ppRef)->reference        = -1;
		(*ppRef)->index            = TLB_REF_USE_GUID;
		(*ppRef)->pImpTLInfo       = TLB_Alloc(sizeof(TLBImpLib));
		(*ppRef)->pImpTLInfo->guid = IID_StdOle;
		(*ppRef)->pImpTLInfo->name = SysAllocStringLen(NULL,
							      nStdOleLen  + 1);

		MultiByteToWideChar(CP_ACP,
				    MB_PRECOMPOSED,
				    szStdOle,
				    -1,
				    (*ppRef)->pImpTLInfo->name,
				    SysStringLen((*ppRef)->pImpTLInfo->name));

		(*ppRef)->pImpTLInfo->lcid          = 0;
		(*ppRef)->pImpTLInfo->wVersionMajor = 2;
		(*ppRef)->pImpTLInfo->wVersionMinor = 0;
	      }
	    }
            break;
        default:
            ptiRet->impltypelist=TLB_Alloc(sizeof(TLBImplType));
            MSFT_DoRefType(pcx, ptiRet, tiBase.datatype1);
	    ptiRet->impltypelist->hRef = tiBase.datatype1;
            break;
       }
    }
    ptiRet->ctCustData=
        MSFT_CustData(pcx, tiBase.oCustData, &ptiRet->pCustData);

    TRACE_(typelib)("%s guid: %s kind:%s\n",
       debugstr_w(ptiRet->Name),
       debugstr_guid(&ptiRet->TypeAttr.guid),
       typekind_desc[ptiRet->TypeAttr.typekind]);

    return ptiRet;
}

/****************************************************************************
 *	TLB_ReadTypeLib
 *
 * find the type of the typelib file and map the typelib resource into
 * the memory
 */
#define MSFT_SIGNATURE 0x5446534D /* "MSFT" */
#define SLTG_SIGNATURE 0x47544c53 /* "SLTG" */
int TLB_ReadTypeLib(LPCWSTR pszFileName, INT index, ITypeLib2 **ppTypeLib)
{
    int ret = TYPE_E_CANTLOADLIBRARY;
    ITypeLibImpl* current;
    DWORD dwSignature = 0;
    HFILE hFile;

    TRACE_(typelib)("%s:%d\n", debugstr_w(pszFileName), index);

    *ppTypeLib = NULL;

    /* check if typelib is cached */
    EnterCriticalSection(&csTypeLibImpl);
    current = firstTypeLibImpl;
    while (current) {
      if (!strcmpiW(current->path, pszFileName)) {
        ITypeLib2_AddRef((ITypeLib2*)current);
        break;
      }
      current = current->next;
    }
    LeaveCriticalSection(&csTypeLibImpl);
    if (current) {
      /* cache hit */
      TRACE_(typelib)("returning cached typelib %p\n", current);
      *ppTypeLib = (ITypeLib2*)current;
      return S_OK;
    }

    /* check the signature of the file */
    hFile = CreateFileW( pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0 );
    if (INVALID_HANDLE_VALUE != hFile)
    {
      HANDLE hMapping = CreateFileMappingA( hFile, NULL, PAGE_READONLY | SEC_COMMIT, 0, 0, NULL );
      if (hMapping)
      {
        LPVOID pBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if(pBase)
        {
	  /* retrieve file size */
	  DWORD dwTLBLength = GetFileSize(hFile, NULL);

          /* first try to load as *.tlb */
          dwSignature = *((DWORD*) pBase);
          if ( dwSignature == MSFT_SIGNATURE)
          {
            *ppTypeLib = ITypeLib2_Constructor_MSFT(pBase, dwTLBLength);
          }
	  else if ( dwSignature == SLTG_SIGNATURE)
          {
            *ppTypeLib = ITypeLib2_Constructor_SLTG(pBase, dwTLBLength);
	  }
          UnmapViewOfFile(pBase);
        }
        CloseHandle(hMapping);
      }
      CloseHandle(hFile);
    }

    if( (WORD)dwSignature == IMAGE_DOS_SIGNATURE )
    {
      /* find the typelibrary resource*/
      HINSTANCE hinstDLL = LoadLibraryExW(pszFileName, 0, DONT_RESOLVE_DLL_REFERENCES|
                                          LOAD_LIBRARY_AS_DATAFILE|LOAD_WITH_ALTERED_SEARCH_PATH);
      if (hinstDLL)
      {
        HRSRC hrsrc = FindResourceA(hinstDLL, MAKEINTRESOURCEA(index),
	  "TYPELIB");
        if (hrsrc)
        {
          HGLOBAL hGlobal = LoadResource(hinstDLL, hrsrc);
          if (hGlobal)
          {
            LPVOID pBase = LockResource(hGlobal);
            DWORD  dwTLBLength = SizeofResource(hinstDLL, hrsrc);

            if (pBase)
            {
              /* try to load as incore resource */
              dwSignature = *((DWORD*) pBase);
              if ( dwSignature == MSFT_SIGNATURE)
              {
                  *ppTypeLib = ITypeLib2_Constructor_MSFT(pBase, dwTLBLength);
	      }
	      else if ( dwSignature == SLTG_SIGNATURE)
	      {
                  *ppTypeLib = ITypeLib2_Constructor_SLTG(pBase, dwTLBLength);
	      }
              else
              {
                  FIXME("Header type magic 0x%08lx not supported.\n",dwSignature);
              }
            }
            FreeResource( hGlobal );
          }
        }
        FreeLibrary(hinstDLL);
      }
    }

    if(*ppTypeLib) {
      /* check if someone else cached it already */
      EnterCriticalSection(&csTypeLibImpl);
      current = firstTypeLibImpl;
      while (current) {
        if (!strcmpiW(current->path, pszFileName)) {
          ITypeLib2_AddRef((ITypeLib2*)current);
          break;
        }
        current = current->next;
      }
      if (current) {
        /* cache hit */
        LeaveCriticalSection(&csTypeLibImpl);
        ITypeLib2_Release(*ppTypeLib);
        TRACE_(typelib)("returning cached typelib %p\n", current);
        *ppTypeLib = (ITypeLib2*)current;
      } else {
        /* cache miss - add newly loaded typelib to cache */
        LPWSTR path;
        path = HeapAlloc(GetProcessHeap(), 0, (strlenW(pszFileName)+1)*sizeof(WCHAR));
        strcpyW(path, pszFileName);
        current = (ITypeLibImpl*)*ppTypeLib;
        current->path = path;
        current->prev = NULL;
        current->next = firstTypeLibImpl;
        if (firstTypeLibImpl) firstTypeLibImpl->prev = current;
        firstTypeLibImpl = current;
        LeaveCriticalSection(&csTypeLibImpl);
        TRACE_(typelib)("caching typelib %p\n", current);
      }
      ret = S_OK;
    }
    else
      ERR("Loading of typelib %s failed with error %ld\n",
	  debugstr_w(pszFileName), GetLastError());

    return ret;
}

/*================== ITypeLib(2) Methods ===================================*/

/****************************************************************************
 *	ITypeLib2_Constructor_MSFT
 *
 * loading an MSFT typelib from an in-memory image
 */
static ITypeLib2* ITypeLib2_Constructor_MSFT(LPVOID pLib, DWORD dwTLBLength)
{
    TLBContext cx;
    long lPSegDir;
    MSFT_Header tlbHeader;
    MSFT_SegDir tlbSegDir;
    ITypeLibImpl * pTypeLibImpl;

    TRACE("%p, TLB length = %ld\n", pLib, dwTLBLength);

    pTypeLibImpl = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(ITypeLibImpl));
    if (!pTypeLibImpl) return NULL;

    ICOM_VTBL(pTypeLibImpl) = &tlbvt;
    pTypeLibImpl->ref = 1;

    /* get pointer to beginning of typelib data */
    cx.pos = 0;
    cx.oStart=0;
    cx.mapping = pLib;
    cx.pLibInfo = pTypeLibImpl;
    cx.length = dwTLBLength;

    /* read header */
    MSFT_Read((void*)&tlbHeader, sizeof(tlbHeader), &cx, 0);
    TRACE("header:\n");
    TRACE("\tmagic1=0x%08x ,magic2=0x%08x\n",tlbHeader.magic1,tlbHeader.magic2 );
    if (memcmp(&tlbHeader.magic1,TLBMAGIC2,4)) {
	FIXME("Header type magic 0x%08x not supported.\n",tlbHeader.magic1);
	return NULL;
    }
    /* there is a small amount of information here until the next important
     * part:
     * the segment directory . Try to calculate the amount of data */
    lPSegDir = sizeof(tlbHeader) + (tlbHeader.nrtypeinfos)*4 + ((tlbHeader.varflags & HELPDLLFLAG)? 4 :0);

    /* now read the segment directory */
    TRACE("read segment directory (at %ld)\n",lPSegDir);
    MSFT_Read((void*)&tlbSegDir, sizeof(tlbSegDir), &cx, lPSegDir);
    cx.pTblDir = &tlbSegDir;

    /* just check two entries */
    if ( tlbSegDir.pTypeInfoTab.res0c != 0x0F || tlbSegDir.pImpInfo.res0c != 0x0F)
    {
        ERR("cannot find the table directory, ptr=0x%lx\n",lPSegDir);
	HeapFree(GetProcessHeap(),0,pTypeLibImpl);
	return NULL;
    }

    /* now fill our internal data */
    /* TLIBATTR fields */
    MSFT_ReadGuid(&pTypeLibImpl->LibAttr.guid, tlbHeader.posguid, &cx);

    /*    pTypeLibImpl->LibAttr.lcid = tlbHeader.lcid;*/
    /* Windows seems to have zero here, is this correct? */
    if(SUBLANGID(tlbHeader.lcid) == SUBLANG_NEUTRAL)
      pTypeLibImpl->LibAttr.lcid = PRIMARYLANGID(tlbHeader.lcid);
    else
      pTypeLibImpl->LibAttr.lcid = 0;

    pTypeLibImpl->LibAttr.syskind = tlbHeader.varflags & 0x0f; /* check the mask */
    pTypeLibImpl->LibAttr.wMajorVerNum = LOWORD(tlbHeader.version);
    pTypeLibImpl->LibAttr.wMinorVerNum = HIWORD(tlbHeader.version);
    pTypeLibImpl->LibAttr.wLibFlags = (WORD) tlbHeader.flags & 0xffff;/* check mask */

    /* name, eventually add to a hash table */
    pTypeLibImpl->Name = MSFT_ReadName(&cx, tlbHeader.NameOffset);

    /* help info */
    pTypeLibImpl->DocString = MSFT_ReadString(&cx, tlbHeader.helpstring);
    pTypeLibImpl->HelpFile = MSFT_ReadString(&cx, tlbHeader.helpfile);

    if( tlbHeader.varflags & HELPDLLFLAG)
    {
            int offset;
            MSFT_Read(&offset, sizeof(offset), &cx, sizeof(tlbHeader));
            pTypeLibImpl->HelpStringDll = MSFT_ReadString(&cx, offset);
    }

    pTypeLibImpl->dwHelpContext = tlbHeader.helpstringcontext;

    /* custom data */
    if(tlbHeader.CustomDataOffset >= 0)
    {
        pTypeLibImpl->ctCustData = MSFT_CustData(&cx, tlbHeader.CustomDataOffset, &pTypeLibImpl->pCustData);
    }

    /* fill in typedescriptions */
    if(tlbSegDir.pTypdescTab.length > 0)
    {
        int i, j, cTD = tlbSegDir.pTypdescTab.length / (2*sizeof(INT));
        INT16 td[4];
        pTypeLibImpl->pTypeDesc = TLB_Alloc( cTD * sizeof(TYPEDESC));
        MSFT_Read(td, sizeof(td), &cx, tlbSegDir.pTypdescTab.offset);
        for(i=0; i<cTD; )
	{
            /* FIXME: add several sanity checks here */
            pTypeLibImpl->pTypeDesc[i].vt = td[0] & VT_TYPEMASK;
            if(td[0] == VT_PTR || td[0] == VT_SAFEARRAY)
	    {
	        /* FIXME: check safearray */
                if(td[3] < 0)
                    pTypeLibImpl->pTypeDesc[i].u.lptdesc= & stndTypeDesc[td[2]];
                else
                    pTypeLibImpl->pTypeDesc[i].u.lptdesc= & pTypeLibImpl->pTypeDesc[td[2]/8];
            }
	    else if(td[0] == VT_CARRAY)
            {
	        /* array descr table here */
	        pTypeLibImpl->pTypeDesc[i].u.lpadesc = (void *)((int) td[2]);  /* temp store offset in*/
            }
            else if(td[0] == VT_USERDEFINED)
	    {
                pTypeLibImpl->pTypeDesc[i].u.hreftype = MAKELONG(td[2],td[3]);
            }
	    if(++i<cTD) MSFT_Read(td, sizeof(td), &cx, DO_NOT_SEEK);
        }

        /* second time around to fill the array subscript info */
        for(i=0;i<cTD;i++)
	{
            if(pTypeLibImpl->pTypeDesc[i].vt != VT_CARRAY) continue;
            if(tlbSegDir.pArrayDescriptions.offset>0)
	    {
                MSFT_Read(td, sizeof(td), &cx, tlbSegDir.pArrayDescriptions.offset + (int) pTypeLibImpl->pTypeDesc[i].u.lpadesc);
                pTypeLibImpl->pTypeDesc[i].u.lpadesc = TLB_Alloc(sizeof(ARRAYDESC)+sizeof(SAFEARRAYBOUND)*(td[3]-1));

                if(td[1]<0)
                    pTypeLibImpl->pTypeDesc[i].u.lpadesc->tdescElem.vt = td[0] & VT_TYPEMASK;
                else
                    pTypeLibImpl->pTypeDesc[i].u.lpadesc->tdescElem = stndTypeDesc[td[0]/8];

                pTypeLibImpl->pTypeDesc[i].u.lpadesc->cDims = td[2];

                for(j = 0; j<td[2]; j++)
		{
                    MSFT_Read(& pTypeLibImpl->pTypeDesc[i].u.lpadesc->rgbounds[j].cElements,
                        sizeof(INT), &cx, DO_NOT_SEEK);
                    MSFT_Read(& pTypeLibImpl->pTypeDesc[i].u.lpadesc->rgbounds[j].lLbound,
                        sizeof(INT), &cx, DO_NOT_SEEK);
                }
            }
	    else
	    {
                pTypeLibImpl->pTypeDesc[i].u.lpadesc = NULL;
                ERR("didn't find array description data\n");
            }
        }
    }

    /* imported type libs */
    if(tlbSegDir.pImpFiles.offset>0)
    {
        TLBImpLib **ppImpLib = &(pTypeLibImpl->pImpLibs);
        int oGuid, offset = tlbSegDir.pImpFiles.offset;
        UINT16 size;

        while(offset < tlbSegDir.pImpFiles.offset +tlbSegDir.pImpFiles.length)
	{
            *ppImpLib = TLB_Alloc(sizeof(TLBImpLib));
            (*ppImpLib)->offset = offset - tlbSegDir.pImpFiles.offset;
            MSFT_Read(&oGuid, sizeof(INT), &cx, offset);

	    MSFT_Read(&(*ppImpLib)->lcid,          sizeof(LCID),   &cx, DO_NOT_SEEK);
            MSFT_Read(&(*ppImpLib)->wVersionMajor, sizeof(WORD),   &cx, DO_NOT_SEEK);
            MSFT_Read(&(*ppImpLib)->wVersionMinor, sizeof(WORD),   &cx, DO_NOT_SEEK);
            MSFT_Read(& size,                      sizeof(UINT16), &cx, DO_NOT_SEEK);

            size >>= 2;
            (*ppImpLib)->name = TLB_Alloc(size+1);
            MSFT_Read((*ppImpLib)->name, size, &cx, DO_NOT_SEEK);
            MSFT_ReadGuid(&(*ppImpLib)->guid, oGuid, &cx);
            offset = (offset + sizeof(INT) + sizeof(DWORD) + sizeof(LCID) + sizeof(UINT16) + size + 3) & 0xfffffffc;

            ppImpLib = &(*ppImpLib)->next;
        }
    }

    /* type info's */
    if(tlbHeader.nrtypeinfos >= 0 )
    {
        /*pTypeLibImpl->TypeInfoCount=tlbHeader.nrtypeinfos; */
        ITypeInfoImpl **ppTI = &(pTypeLibImpl->pTypeInfo);
        int i;

        for(i = 0; i<(int)tlbHeader.nrtypeinfos; i++)
        {
            *ppTI = MSFT_DoTypeInfo(&cx, i, pTypeLibImpl);

            ITypeInfo_AddRef((ITypeInfo*) *ppTI);
            ppTI = &((*ppTI)->next);
            (pTypeLibImpl->TypeInfoCount)++;
        }
    }

    TRACE("(%p)\n", pTypeLibImpl);
    return (ITypeLib2*) pTypeLibImpl;
}


static BSTR TLB_MultiByteToBSTR(char *ptr)
{
    DWORD len;
    WCHAR *nameW;
    BSTR ret;

    len = MultiByteToWideChar(CP_ACP, 0, ptr, -1, NULL, 0);
    nameW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, ptr, -1, nameW, len);
    ret = SysAllocString(nameW);
    HeapFree(GetProcessHeap(), 0, nameW);
    return ret;
}

static BOOL TLB_GUIDFromString(char *str, GUID *guid)
{
  char b[3];
  int i;
  short s;

  if(sscanf(str, "%lx-%hx-%hx-%hx", &guid->Data1, &guid->Data2, &guid->Data3, &s) != 4) {
    FIXME("Can't parse guid %s\n", debugstr_guid(guid));
    return FALSE;
  }

  guid->Data4[0] = s >> 8;
  guid->Data4[1] = s & 0xff;

  b[2] = '\0';
  for(i = 0; i < 6; i++) {
    memcpy(b, str + 24 + 2 * i, 2);
    guid->Data4[i + 2] = strtol(b, NULL, 16);
  }
  return TRUE;
}

static WORD SLTG_ReadString(char *ptr, BSTR *pBstr)
{
    WORD bytelen;
    DWORD len;
    WCHAR *nameW;

    *pBstr = NULL;
    bytelen = *(WORD*)ptr;
    if(bytelen == 0xffff) return 2;
    len = MultiByteToWideChar(CP_ACP, 0, ptr + 2, bytelen, NULL, 0);
    nameW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    len = MultiByteToWideChar(CP_ACP, 0, ptr + 2, bytelen, nameW, len);
    *pBstr = SysAllocStringLen(nameW, len);
    HeapFree(GetProcessHeap(), 0, nameW);
    return bytelen + 2;
}

static WORD SLTG_ReadStringA(char *ptr, char **str)
{
    WORD bytelen;

    *str = NULL;
    bytelen = *(WORD*)ptr;
    if(bytelen == 0xffff) return 2;
    *str = HeapAlloc(GetProcessHeap(), 0, bytelen + 1);
    memcpy(*str, ptr + 2, bytelen);
    (*str)[bytelen] = '\0';
    return bytelen + 2;
}

static DWORD SLTG_ReadLibBlk(LPVOID pLibBlk, ITypeLibImpl *pTypeLibImpl)
{
    char *ptr = pLibBlk;
    WORD w;

    if((w = *(WORD*)ptr) != SLTG_LIBBLK_MAGIC) {
        FIXME("libblk magic = %04x\n", w);
	return 0;
    }

    ptr += 6;
    if((w = *(WORD*)ptr) != 0xffff) {
        FIXME("LibBlk.res06 = %04x. Assumung string and skipping\n", w);
        ptr += w;
    }
    ptr += 2;

    ptr += SLTG_ReadString(ptr, &pTypeLibImpl->DocString);

    ptr += SLTG_ReadString(ptr, &pTypeLibImpl->HelpFile);

    pTypeLibImpl->dwHelpContext = *(DWORD*)ptr;
    ptr += 4;

    pTypeLibImpl->LibAttr.syskind = *(WORD*)ptr;
    ptr += 2;

    pTypeLibImpl->LibAttr.lcid = *(WORD*)ptr;
    ptr += 2;

    ptr += 4; /* skip res12 */

    pTypeLibImpl->LibAttr.wLibFlags = *(WORD*)ptr;
    ptr += 2;

    pTypeLibImpl->LibAttr.wMajorVerNum = *(WORD*)ptr;
    ptr += 2;

    pTypeLibImpl->LibAttr.wMinorVerNum = *(WORD*)ptr;
    ptr += 2;

    memcpy(&pTypeLibImpl->LibAttr.guid, ptr, sizeof(GUID));
    ptr += sizeof(GUID);

    return ptr - (char*)pLibBlk;
}

static WORD *SLTG_DoType(WORD *pType, char *pBlk, ELEMDESC *pElem)
{
    BOOL done = FALSE;
    TYPEDESC *pTD = &pElem->tdesc;

    /* Handle [in/out] first */
    if((*pType & 0xc000) == 0xc000)
        pElem->u.paramdesc.wParamFlags = PARAMFLAG_NONE;
    else if(*pType & 0x8000)
        pElem->u.paramdesc.wParamFlags = PARAMFLAG_FIN | PARAMFLAG_FOUT;
    else if(*pType & 0x4000)
        pElem->u.paramdesc.wParamFlags = PARAMFLAG_FOUT;
    else
        pElem->u.paramdesc.wParamFlags = PARAMFLAG_FIN;

    if(*pType & 0x2000)
        pElem->u.paramdesc.wParamFlags |= PARAMFLAG_FLCID;

    if(*pType & 0x80)
        pElem->u.paramdesc.wParamFlags |= PARAMFLAG_FRETVAL;

    while(!done) {
        if((*pType & 0xe00) == 0xe00) {
	    pTD->vt = VT_PTR;
	    pTD->u.lptdesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				       sizeof(TYPEDESC));
	    pTD = pTD->u.lptdesc;
	}
	switch(*pType & 0x7f) {
	case VT_PTR:
	    pTD->vt = VT_PTR;
	    pTD->u.lptdesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				       sizeof(TYPEDESC));
	    pTD = pTD->u.lptdesc;
	    break;

	case VT_USERDEFINED:
	    pTD->vt = VT_USERDEFINED;
	    pTD->u.hreftype = *(++pType) / 4;
	    done = TRUE;
	    break;

	case VT_CARRAY:
	  {
	    /* *(pType+1) is offset to a SAFEARRAY, *(pType+2) is type of
	       array */

	    SAFEARRAY *pSA = (SAFEARRAY *)(pBlk + *(++pType));

	    pTD->vt = VT_CARRAY;
	    pTD->u.lpadesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			        sizeof(ARRAYDESC) +
				(pSA->cDims - 1) * sizeof(SAFEARRAYBOUND));
	    pTD->u.lpadesc->cDims = pSA->cDims;
	    memcpy(pTD->u.lpadesc->rgbounds, pSA->rgsabound,
		   pSA->cDims * sizeof(SAFEARRAYBOUND));

	    pTD = &pTD->u.lpadesc->tdescElem;
	    break;
	  }

	case VT_SAFEARRAY:
	  {
	    /* FIXME: *(pType+1) gives an offset to SAFEARRAY, is this
	       useful? */

	    pType++;
	    pTD->vt = VT_SAFEARRAY;
	    pTD->u.lptdesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				       sizeof(TYPEDESC));
	    pTD = pTD->u.lptdesc;
	    break;
	  }
	default:
	    pTD->vt = *pType & 0x7f;
	    done = TRUE;
	    break;
	}
	pType++;
    }
    return pType;
}


static void SLTG_DoRefs(SLTG_RefInfo *pRef, ITypeInfoImpl *pTI,
			char *pNameTable)
{
    int ref;
    char *name;
    TLBRefType **ppRefType;

    if(pRef->magic != SLTG_REF_MAGIC) {
        FIXME("Ref magic = %x\n", pRef->magic);
	return;
    }
    name = ( (char*)(&pRef->names) + pRef->number);

    ppRefType = &pTI->reflist;
    for(ref = 0; ref < pRef->number >> 3; ref++) {
        char *refname;
	unsigned int lib_offs, type_num;

	*ppRefType = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			       sizeof(**ppRefType));

	name += SLTG_ReadStringA(name, &refname);
	if(sscanf(refname, "*\\R%x*#%x", &lib_offs, &type_num) != 2)
	    FIXME("Can't sscanf ref\n");
	if(lib_offs != 0xffff) {
	    TLBImpLib **import = &pTI->pTypeLib->pImpLibs;

	    while(*import) {
	        if((*import)->offset == lib_offs)
		    break;
		import = &(*import)->next;
	    }
	    if(!*import) {
	        char fname[MAX_PATH+1];
		int len;

		*import = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				    sizeof(**import));
		(*import)->offset = lib_offs;
		TLB_GUIDFromString( pNameTable + lib_offs + 4,
				    &(*import)->guid);
		if(sscanf(pNameTable + lib_offs + 40, "}#%hd.%hd#%lx#%s",
			  &(*import)->wVersionMajor,
			  &(*import)->wVersionMinor,
			  &(*import)->lcid, fname) != 4) {
		  FIXME("can't sscanf ref %s\n",
			pNameTable + lib_offs + 40);
		}
		len = strlen(fname);
		if(fname[len-1] != '#')
		    FIXME("fname = %s\n", fname);
		fname[len-1] = '\0';
		(*import)->name = TLB_MultiByteToBSTR(fname);
	    }
	    (*ppRefType)->pImpTLInfo = *import;
	} else { /* internal ref */
	  (*ppRefType)->pImpTLInfo = TLB_REF_INTERNAL;
	}
	(*ppRefType)->reference = ref;
	(*ppRefType)->index = type_num;

	HeapFree(GetProcessHeap(), 0, refname);
	ppRefType = &(*ppRefType)->next;
    }
    if((BYTE)*name != SLTG_REF_MAGIC)
      FIXME("End of ref block magic = %x\n", *name);
    dump_TLBRefType(pTI->reflist);
}

static char *SLTG_DoImpls(char *pBlk, ITypeInfoImpl *pTI,
			  BOOL OneOnly)
{
    SLTG_ImplInfo *info;
    TLBImplType **ppImplType = &pTI->impltypelist;
    /* I don't really get this structure, usually it's 0x16 bytes
       long, but iuser.tlb contains some that are 0x18 bytes long.
       That's ok because we can use the next ptr to jump to the next
       one. But how do we know the length of the last one?  The WORD
       at offs 0x8 might be the clue.  For now I'm just assuming that
       the last one is the regular 0x16 bytes. */

    info = (SLTG_ImplInfo*)pBlk;
    while(1) {
	*ppImplType = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				sizeof(**ppImplType));
	(*ppImplType)->hRef = info->ref;
	(*ppImplType)->implflags = info->impltypeflags;
	pTI->TypeAttr.cImplTypes++;
	ppImplType = &(*ppImplType)->next;

        if(info->next == 0xffff)
	    break;
	if(OneOnly)
	    FIXME("Interface inheriting more than one interface\n");
	info = (SLTG_ImplInfo*)(pBlk + info->next);
    }
    info++; /* see comment at top of function */
    return (char*)info;
}

static SLTG_TypeInfoTail *SLTG_ProcessCoClass(char *pBlk, ITypeInfoImpl *pTI,
					      char *pNameTable)
{
    SLTG_TypeInfoHeader *pTIHeader = (SLTG_TypeInfoHeader*)pBlk;
    SLTG_MemberHeader *pMemHeader;
    char *pFirstItem, *pNextItem;

    if(pTIHeader->href_table != 0xffffffff) {
        SLTG_DoRefs((SLTG_RefInfo*)(pBlk + pTIHeader->href_table), pTI,
		    pNameTable);
    }


    pMemHeader = (SLTG_MemberHeader*)(pBlk + pTIHeader->elem_table);

    pFirstItem = pNextItem = (char*)(pMemHeader + 1);

    if(*(WORD*)pFirstItem == SLTG_IMPL_MAGIC) {
        pNextItem = SLTG_DoImpls(pFirstItem, pTI, FALSE);
    }

    return (SLTG_TypeInfoTail*)(pFirstItem + pMemHeader->cbExtra);
}


static SLTG_TypeInfoTail *SLTG_ProcessInterface(char *pBlk, ITypeInfoImpl *pTI,
						char *pNameTable)
{
    SLTG_TypeInfoHeader *pTIHeader = (SLTG_TypeInfoHeader*)pBlk;
    SLTG_MemberHeader *pMemHeader;
    SLTG_Function *pFunc;
    char *pFirstItem, *pNextItem;
    TLBFuncDesc **ppFuncDesc = &pTI->funclist;
    int num = 0;

    if(pTIHeader->href_table != 0xffffffff) {
        SLTG_DoRefs((SLTG_RefInfo*)(pBlk + pTIHeader->href_table), pTI,
		    pNameTable);
    }

    pMemHeader = (SLTG_MemberHeader*)(pBlk + pTIHeader->elem_table);

    pFirstItem = pNextItem = (char*)(pMemHeader + 1);

    if(*(WORD*)pFirstItem == SLTG_IMPL_MAGIC) {
        pNextItem = SLTG_DoImpls(pFirstItem, pTI, TRUE);
    }

    for(pFunc = (SLTG_Function*)pNextItem, num = 1; 1;
	pFunc = (SLTG_Function*)(pFirstItem + pFunc->next), num++) {

        int param;
	WORD *pType, *pArg;

	if(pFunc->magic != SLTG_FUNCTION_MAGIC &&
	   pFunc->magic != SLTG_FUNCTION_WITH_FLAGS_MAGIC) {
	    FIXME("func magic = %02x\n", pFunc->magic);
	    return NULL;
	}
	*ppFuncDesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				sizeof(**ppFuncDesc));
	(*ppFuncDesc)->Name = TLB_MultiByteToBSTR(pFunc->name + pNameTable);

	(*ppFuncDesc)->funcdesc.memid = pFunc->dispid;
	(*ppFuncDesc)->funcdesc.invkind = pFunc->inv >> 4;
	(*ppFuncDesc)->funcdesc.callconv = pFunc->nacc & 0x7;
	(*ppFuncDesc)->funcdesc.cParams = pFunc->nacc >> 3;
	(*ppFuncDesc)->funcdesc.cParamsOpt = (pFunc->retnextopt & 0x7e) >> 1;
	(*ppFuncDesc)->funcdesc.oVft = pFunc->vtblpos;

	if(pFunc->magic == SLTG_FUNCTION_WITH_FLAGS_MAGIC)
	    (*ppFuncDesc)->funcdesc.wFuncFlags = pFunc->funcflags;

	if(pFunc->retnextopt & 0x80)
	    pType = &pFunc->rettype;
	else
	    pType = (WORD*)(pFirstItem + pFunc->rettype);


	SLTG_DoType(pType, pFirstItem, &(*ppFuncDesc)->funcdesc.elemdescFunc);

	(*ppFuncDesc)->funcdesc.lprgelemdescParam =
	  HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		    (*ppFuncDesc)->funcdesc.cParams * sizeof(ELEMDESC));
	(*ppFuncDesc)->pParamDesc =
	  HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		    (*ppFuncDesc)->funcdesc.cParams * sizeof(TLBParDesc));

	pArg = (WORD*)(pFirstItem + pFunc->arg_off);

	for(param = 0; param < (*ppFuncDesc)->funcdesc.cParams; param++) {
	    char *paramName = pNameTable + *pArg;
	    BOOL HaveOffs;
	    /* If arg type follows then paramName points to the 2nd
	       letter of the name, else the next WORD is an offset to
	       the arg type and paramName points to the first letter.
	       So let's take one char off paramName and see if we're
	       pointing at an alpha-numeric char.  However if *pArg is
	       0xffff or 0xfffe then the param has no name, the former
	       meaning that the next WORD is the type, the latter
	       meaning the the next WORD is an offset to the type. */

	    HaveOffs = FALSE;
	    if(*pArg == 0xffff)
	        paramName = NULL;
	    else if(*pArg == 0xfffe) {
	        paramName = NULL;
		HaveOffs = TRUE;
	    }
	    else if(!isalnum(*(paramName-1)))
	        HaveOffs = TRUE;

	    pArg++;

	    if(HaveOffs) { /* the next word is an offset to type */
	        pType = (WORD*)(pFirstItem + *pArg);
		SLTG_DoType(pType, pFirstItem,
			    &(*ppFuncDesc)->funcdesc.lprgelemdescParam[param]);
		pArg++;
	    } else {
		if(paramName)
		  paramName--;
		pArg = SLTG_DoType(pArg, pFirstItem,
			   &(*ppFuncDesc)->funcdesc.lprgelemdescParam[param]);
	    }

	    /* Are we an optional param ? */
	    if((*ppFuncDesc)->funcdesc.cParams - param <=
	       (*ppFuncDesc)->funcdesc.cParamsOpt)
	      (*ppFuncDesc)->funcdesc.lprgelemdescParam[param].u.paramdesc.wParamFlags |= PARAMFLAG_FOPT;

	    if(paramName) {
	        (*ppFuncDesc)->pParamDesc[param].Name =
		  TLB_MultiByteToBSTR(paramName);
	    }
	}

	ppFuncDesc = &((*ppFuncDesc)->next);
	if(pFunc->next == 0xffff) break;
    }
    pTI->TypeAttr.cFuncs = num;
    dump_TLBFuncDesc(pTI->funclist);
    return (SLTG_TypeInfoTail*)(pFirstItem + pMemHeader->cbExtra);
}

static SLTG_TypeInfoTail *SLTG_ProcessRecord(char *pBlk, ITypeInfoImpl *pTI,
					     char *pNameTable)
{
  SLTG_TypeInfoHeader *pTIHeader = (SLTG_TypeInfoHeader*)pBlk;
  SLTG_MemberHeader *pMemHeader;
  SLTG_RecordItem *pItem;
  char *pFirstItem;
  TLBVarDesc **ppVarDesc = &pTI->varlist;
  int num = 0;
  WORD *pType;
  char buf[300];

  pMemHeader = (SLTG_MemberHeader*)(pBlk + pTIHeader->elem_table);

  pFirstItem = (char*)(pMemHeader + 1);
  for(pItem = (SLTG_RecordItem *)pFirstItem, num = 1; 1;
      pItem = (SLTG_RecordItem *)(pFirstItem + pItem->next), num++) {
      if(pItem->magic != SLTG_RECORD_MAGIC) {
	  FIXME("record magic = %02x\n", pItem->magic);
	  return NULL;
      }
      *ppVarDesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			     sizeof(**ppVarDesc));
      (*ppVarDesc)->Name = TLB_MultiByteToBSTR(pItem->name + pNameTable);
      (*ppVarDesc)->vardesc.memid = pItem->memid;
      (*ppVarDesc)->vardesc.u.oInst = pItem->byte_offs;
      (*ppVarDesc)->vardesc.varkind = VAR_PERINSTANCE;

      if(pItem->typepos == 0x02)
	  pType = &pItem->type;
      else if(pItem->typepos == 0x00)
	  pType = (WORD*)(pFirstItem + pItem->type);
      else {
	  FIXME("typepos = %02x\n", pItem->typepos);
	  break;
      }

      SLTG_DoType(pType, pFirstItem,
		  &(*ppVarDesc)->vardesc.elemdescVar);

      /* FIXME("helpcontext, helpstring\n"); */

      dump_TypeDesc(&(*ppVarDesc)->vardesc.elemdescVar.tdesc, buf);

      ppVarDesc = &((*ppVarDesc)->next);
      if(pItem->next == 0xffff) break;
  }
  pTI->TypeAttr.cVars = num;
  return (SLTG_TypeInfoTail*)(pFirstItem + pMemHeader->cbExtra);
}

static SLTG_TypeInfoTail *SLTG_ProcessEnum(char *pBlk, ITypeInfoImpl *pTI,
					   char *pNameTable)
{
  SLTG_TypeInfoHeader *pTIHeader = (SLTG_TypeInfoHeader*)pBlk;
  SLTG_MemberHeader *pMemHeader;
  SLTG_EnumItem *pItem;
  char *pFirstItem;
  TLBVarDesc **ppVarDesc = &pTI->varlist;
  int num = 0;

  pMemHeader = (SLTG_MemberHeader*)(pBlk + pTIHeader->elem_table);

  pFirstItem = (char*)(pMemHeader + 1);
  for(pItem = (SLTG_EnumItem *)pFirstItem, num = 1; 1;
      pItem = (SLTG_EnumItem *)(pFirstItem + pItem->next), num++) {
      if(pItem->magic != SLTG_ENUMITEM_MAGIC) {
	  FIXME("enumitem magic = %04x\n", pItem->magic);
	  return NULL;
      }
      *ppVarDesc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			     sizeof(**ppVarDesc));
      (*ppVarDesc)->Name = TLB_MultiByteToBSTR(pItem->name + pNameTable);
      (*ppVarDesc)->vardesc.memid = pItem->memid;
      (*ppVarDesc)->vardesc.u.lpvarValue = HeapAlloc(GetProcessHeap(), 0,
						     sizeof(VARIANT));
      V_VT((*ppVarDesc)->vardesc.u.lpvarValue) = VT_INT;
      V_UNION((*ppVarDesc)->vardesc.u.lpvarValue, intVal) =
	*(INT*)(pItem->value + pFirstItem);
      (*ppVarDesc)->vardesc.elemdescVar.tdesc.vt = VT_I4;
      (*ppVarDesc)->vardesc.varkind = VAR_CONST;
      /* FIXME("helpcontext, helpstring\n"); */

      ppVarDesc = &((*ppVarDesc)->next);
      if(pItem->next == 0xffff) break;
  }
  pTI->TypeAttr.cVars = num;
  return (SLTG_TypeInfoTail*)(pFirstItem + pMemHeader->cbExtra);
}

/* Because SLTG_OtherTypeInfo is such a painfull struct, we make a more
   managable copy of it into this */
typedef struct {
  WORD small_no;
  char *index_name;
  char *other_name;
  WORD res1a;
  WORD name_offs;
  WORD more_bytes;
  char *extra;
  WORD res20;
  DWORD helpcontext;
  WORD res26;
  GUID uuid;
} SLTG_InternalOtherTypeInfo;

/****************************************************************************
 *	ITypeLib2_Constructor_SLTG
 *
 * loading a SLTG typelib from an in-memory image
 */
static ITypeLib2* ITypeLib2_Constructor_SLTG(LPVOID pLib, DWORD dwTLBLength)
{
    ITypeLibImpl *pTypeLibImpl;
    SLTG_Header *pHeader;
    SLTG_BlkEntry *pBlkEntry;
    SLTG_Magic *pMagic;
    SLTG_Index *pIndex;
    SLTG_Pad9 *pPad9;
    LPVOID pBlk, pFirstBlk;
    SLTG_LibBlk *pLibBlk;
    SLTG_InternalOtherTypeInfo *pOtherTypeInfoBlks;
    char *pAfterOTIBlks = NULL;
    char *pNameTable, *ptr;
    int i;
    DWORD len, order;
    ITypeInfoImpl **ppTypeInfoImpl;

    TRACE("%p, TLB length = %ld\n", pLib, dwTLBLength);

    pTypeLibImpl = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(ITypeLibImpl));
    if (!pTypeLibImpl) return NULL;

    ICOM_VTBL(pTypeLibImpl) = &tlbvt;
    pTypeLibImpl->ref = 1;

    pHeader = pLib;

    TRACE("header:\n");
    TRACE("\tmagic=0x%08lx, file blocks = %d\n", pHeader->SLTG_magic,
	  pHeader->nrOfFileBlks );
    if (memcmp(&pHeader->SLTG_magic, TLBMAGIC1, 4)) {
	FIXME("Header type magic 0x%08lx not supported.\n",
	      pHeader->SLTG_magic);
	return NULL;
    }

    /* There are pHeader->nrOfFileBlks - 2 TypeInfo records in this typelib */
    pTypeLibImpl->TypeInfoCount = pHeader->nrOfFileBlks - 2;

    /* This points to pHeader->nrOfFileBlks - 1 of SLTG_BlkEntry */
    pBlkEntry = (SLTG_BlkEntry*)(pHeader + 1);

    /* Next we have a magic block */
    pMagic = (SLTG_Magic*)(pBlkEntry + pHeader->nrOfFileBlks - 1);

    /* Let's see if we're still in sync */
    if(memcmp(pMagic->CompObj_magic, SLTG_COMPOBJ_MAGIC,
	      sizeof(SLTG_COMPOBJ_MAGIC))) {
        FIXME("CompObj magic = %s\n", pMagic->CompObj_magic);
	return NULL;
    }
    if(memcmp(pMagic->dir_magic, SLTG_DIR_MAGIC,
	      sizeof(SLTG_DIR_MAGIC))) {
        FIXME("dir magic = %s\n", pMagic->dir_magic);
	return NULL;
    }

    pIndex = (SLTG_Index*)(pMagic+1);

    pPad9 = (SLTG_Pad9*)(pIndex + pTypeLibImpl->TypeInfoCount);

    pFirstBlk = (LPVOID)(pPad9 + 1);

    /* We'll set up a ptr to the main library block, which is the last one. */

    for(pBlk = pFirstBlk, order = pHeader->first_blk - 1, i = 0;
	  pBlkEntry[order].next != 0;
	  order = pBlkEntry[order].next - 1, i++) {
	pBlk += pBlkEntry[order].len;
    }
    pLibBlk = pBlk;

    len = SLTG_ReadLibBlk(pLibBlk, pTypeLibImpl);

    /* Now there's 0x40 bytes of 0xffff with the numbers 0 to TypeInfoCount
       interspersed */

    len += 0x40;

    /* And now TypeInfoCount of SLTG_OtherTypeInfo */

    pOtherTypeInfoBlks = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				   sizeof(*pOtherTypeInfoBlks) *
				   pTypeLibImpl->TypeInfoCount);


    ptr = (char*)pLibBlk + len;

    for(i = 0; i < pTypeLibImpl->TypeInfoCount; i++) {
	WORD w, extra;
	len = 0;

	pOtherTypeInfoBlks[i].small_no = *(WORD*)ptr;

	w = *(WORD*)(ptr + 2);
	if(w != 0xffff) {
	    len += w;
	    pOtherTypeInfoBlks[i].index_name = HeapAlloc(GetProcessHeap(),0,
							 w+1);
	    memcpy(pOtherTypeInfoBlks[i].index_name, ptr + 4, w);
	    pOtherTypeInfoBlks[i].index_name[w] = '\0';
	}
	w = *(WORD*)(ptr + 4 + len);
	if(w != 0xffff) {
	    TRACE("\twith %s\n", debugstr_an(ptr + 6 + len, w));
	    len += w;
	    pOtherTypeInfoBlks[i].other_name = HeapAlloc(GetProcessHeap(),0,
							 w+1);
	    memcpy(pOtherTypeInfoBlks[i].other_name, ptr + 6 + len, w);
	    pOtherTypeInfoBlks[i].other_name[w] = '\0';
	}
	pOtherTypeInfoBlks[i].res1a = *(WORD*)(ptr + len + 6);
	pOtherTypeInfoBlks[i].name_offs = *(WORD*)(ptr + len + 8);
	extra = pOtherTypeInfoBlks[i].more_bytes = *(WORD*)(ptr + 10 + len);
	if(extra) {
	    pOtherTypeInfoBlks[i].extra = HeapAlloc(GetProcessHeap(),0,
						    extra);
	    memcpy(pOtherTypeInfoBlks[i].extra, ptr + 12, extra);
	    len += extra;
	}
	pOtherTypeInfoBlks[i].res20 = *(WORD*)(ptr + 12 + len);
	pOtherTypeInfoBlks[i].helpcontext = *(DWORD*)(ptr + 14 + len);
	pOtherTypeInfoBlks[i].res26 = *(WORD*)(ptr + 18 + len);
	memcpy(&pOtherTypeInfoBlks[i].uuid, ptr + 20 + len, sizeof(GUID));
	len += sizeof(SLTG_OtherTypeInfo);
	ptr += len;
    }

    pAfterOTIBlks = ptr;

    /* Skip this WORD and get the next DWORD */
    len = *(DWORD*)(pAfterOTIBlks + 2);

    /* Now add this to pLibBLk and then add 0x216, sprinkle a bit a
       magic dust and we should be pointing at the beginning of the name
       table */

    pNameTable = (char*)pLibBlk + len + 0x216;

    pNameTable += 2;

    TRACE("Library name is %s\n", pNameTable + pLibBlk->name);

    pTypeLibImpl->Name = TLB_MultiByteToBSTR(pNameTable + pLibBlk->name);


    /* Hopefully we now have enough ptrs set up to actually read in
       some TypeInfos.  It's not clear which order to do them in, so
       I'll just follow the links along the BlkEntry chain and read
       them in in the order in which they're in the file */

    ppTypeInfoImpl = &(pTypeLibImpl->pTypeInfo);

    for(pBlk = pFirstBlk, order = pHeader->first_blk - 1, i = 0;
	pBlkEntry[order].next != 0;
	order = pBlkEntry[order].next - 1, i++) {

      SLTG_TypeInfoHeader *pTIHeader;
      SLTG_TypeInfoTail *pTITail;

      if(strcmp(pBlkEntry[order].index_string + (char*)pMagic,
		pOtherTypeInfoBlks[i].index_name)) {
	FIXME("Index strings don't match\n");
	return NULL;
      }

      pTIHeader = pBlk;
      if(pTIHeader->magic != SLTG_TIHEADER_MAGIC) {
	FIXME("TypeInfoHeader magic = %04x\n", pTIHeader->magic);
	return NULL;
      }
      *ppTypeInfoImpl = (ITypeInfoImpl*)ITypeInfo_Constructor();
      (*ppTypeInfoImpl)->pTypeLib = pTypeLibImpl;
	  ITypeLib2_AddRef((ITypeLib2 *)pTypeLibImpl);
      (*ppTypeInfoImpl)->index = i;
      (*ppTypeInfoImpl)->Name = TLB_MultiByteToBSTR(
					     pOtherTypeInfoBlks[i].name_offs +
					     pNameTable);
      (*ppTypeInfoImpl)->dwHelpContext = pOtherTypeInfoBlks[i].helpcontext;
      memcpy(&((*ppTypeInfoImpl)->TypeAttr.guid), &pOtherTypeInfoBlks[i].uuid,
	     sizeof(GUID));
      (*ppTypeInfoImpl)->TypeAttr.typekind = pTIHeader->typekind;
      (*ppTypeInfoImpl)->TypeAttr.wMajorVerNum = pTIHeader->major_version;
      (*ppTypeInfoImpl)->TypeAttr.wMinorVerNum = pTIHeader->minor_version;
      (*ppTypeInfoImpl)->TypeAttr.wTypeFlags =
	(pTIHeader->typeflags1 >> 3) | (pTIHeader->typeflags2 << 5);

      if((pTIHeader->typeflags1 & 7) != 2)
	FIXME("typeflags1 = %02x\n", pTIHeader->typeflags1);
      if(pTIHeader->typeflags3 != 2)
	FIXME("typeflags3 = %02x\n", pTIHeader->typeflags3);

      TRACE("TypeInfo %s of kind %s guid %s typeflags %04x\n",
	    debugstr_w((*ppTypeInfoImpl)->Name),
	    typekind_desc[pTIHeader->typekind],
	    debugstr_guid(&(*ppTypeInfoImpl)->TypeAttr.guid),
	    (*ppTypeInfoImpl)->TypeAttr.wTypeFlags);

      switch(pTIHeader->typekind) {
      case TKIND_ENUM:
	pTITail = SLTG_ProcessEnum(pBlk, *ppTypeInfoImpl, pNameTable);
	break;

      case TKIND_RECORD:
	pTITail = SLTG_ProcessRecord(pBlk, *ppTypeInfoImpl, pNameTable);
	break;

      case TKIND_INTERFACE:
	pTITail = SLTG_ProcessInterface(pBlk, *ppTypeInfoImpl, pNameTable);
	break;

      case TKIND_COCLASS:
	pTITail = SLTG_ProcessCoClass(pBlk, *ppTypeInfoImpl, pNameTable);
	break;

      default:
	FIXME("Not processing typekind %d\n", pTIHeader->typekind);
	pTITail = NULL;
	break;

      }

      if(pTITail) { /* could get cFuncs, cVars and cImplTypes from here
		       but we've already set those */
	  (*ppTypeInfoImpl)->TypeAttr.cbAlignment = pTITail->cbAlignment;
	  (*ppTypeInfoImpl)->TypeAttr.cbSizeInstance = pTITail->cbSizeInstance;
	  (*ppTypeInfoImpl)->TypeAttr.cbSizeVft = pTITail->cbSizeVft;
      }
      ppTypeInfoImpl = &((*ppTypeInfoImpl)->next);
      pBlk += pBlkEntry[order].len;
    }

    if(i != pTypeLibImpl->TypeInfoCount) {
      FIXME("Somehow processed %d TypeInfos\n", i);
      return NULL;
    }

    HeapFree(GetProcessHeap(), 0, pOtherTypeInfoBlks);
    return (ITypeLib2*)pTypeLibImpl;
}

/* ITypeLib::QueryInterface
 */
static HRESULT WINAPI ITypeLib2_fnQueryInterface(
	ITypeLib2 * iface,
	REFIID riid,
	VOID **ppvObject)
{
    ICOM_THIS( ITypeLibImpl, iface);

    TRACE("(%p)->(IID: %s)\n",This,debugstr_guid(riid));

    *ppvObject=NULL;
    if(IsEqualIID(riid, &IID_IUnknown) ||
       IsEqualIID(riid,&IID_ITypeLib)||
       IsEqualIID(riid,&IID_ITypeLib2))
    {
        *ppvObject = This;
    }

    if(*ppvObject)
    {
        ITypeLib2_AddRef(iface);
        TRACE("-- Interface: (%p)->(%p)\n",ppvObject,*ppvObject);
        return S_OK;
    }
    TRACE("-- Interface: E_NOINTERFACE\n");
    return E_NOINTERFACE;
}

/* ITypeLib::AddRef
 */
static ULONG WINAPI ITypeLib2_fnAddRef( ITypeLib2 *iface)
{
    ICOM_THIS( ITypeLibImpl, iface);

    TRACE("(%p)->ref is %u\n",This, This->ref);

    return ++(This->ref);
}

/* ITypeLib::Release
 */
static ULONG WINAPI ITypeLib2_fnRelease( ITypeLib2 *iface)
{
    ICOM_THIS( ITypeLibImpl, iface);

    --(This->ref);

    TRACE("(%p)->(%u)\n",This, This->ref);

    if (!This->ref)
    {
      /* remove from cache */
      EnterCriticalSection(&csTypeLibImpl);
      if (This->next) This->next->prev = This->prev;
      if (This->prev) This->prev->next = This->next;
      else firstTypeLibImpl = This->next;
      LeaveCriticalSection(&csTypeLibImpl);

      if (This->path) {
          HeapFree(GetProcessHeap(), 0, This->path);
          This->path = NULL;
      }

      /* FIXME destroy child objects */

      TRACE(" destroying ITypeLib(%p)\n",This);

      if (This->Name)
      {
          SysFreeString(This->Name);
          This->Name = NULL;
      }

      if (This->DocString)
      {
          SysFreeString(This->DocString);
          This->DocString = NULL;
      }

      if (This->HelpFile)
      {
          SysFreeString(This->HelpFile);
          This->HelpFile = NULL;
      }

      if (This->HelpStringDll)
      {
          SysFreeString(This->HelpStringDll);
          This->HelpStringDll = NULL;
      }

      ITypeInfo_Release((ITypeInfo*) This->pTypeInfo);
      HeapFree(GetProcessHeap(),0,This);
      return 0;
    }

    return This->ref;
}

/* ITypeLib::GetTypeInfoCount
 *
 * Returns the number of type descriptions in the type library
 */
static UINT WINAPI ITypeLib2_fnGetTypeInfoCount( ITypeLib2 *iface)
{
    ICOM_THIS( ITypeLibImpl, iface);
    TRACE("(%p)->count is %d\n",This, This->TypeInfoCount);
    return This->TypeInfoCount;
}

/* ITypeLib::GetTypeInfo
 *
 * retrieves the specified type description in the library.
 */
static HRESULT WINAPI ITypeLib2_fnGetTypeInfo(
    ITypeLib2 *iface,
    UINT index,
    ITypeInfo **ppTInfo)
{
    int i;

    ICOM_THIS( ITypeLibImpl, iface);
    ITypeInfoImpl *pTypeInfo = This->pTypeInfo;

    TRACE("(%p)->(index=%d) \n", This, index);

    if (!ppTInfo) return E_INVALIDARG;

    /* search element n in list */
    for(i=0; i < index; i++)
    {
      pTypeInfo = pTypeInfo->next;
      if (!pTypeInfo)
      {
        TRACE("-- element not found\n");
        return TYPE_E_ELEMENTNOTFOUND;
      }
    }

    *ppTInfo = (ITypeInfo *) pTypeInfo;

    ITypeInfo_AddRef(*ppTInfo);
    TRACE("-- found (%p)\n",*ppTInfo);
    return S_OK;
}


/* ITypeLibs::GetTypeInfoType
 *
 * Retrieves the type of a type description.
 */
static HRESULT WINAPI ITypeLib2_fnGetTypeInfoType(
    ITypeLib2 *iface,
    UINT index,
    TYPEKIND *pTKind)
{
    ICOM_THIS( ITypeLibImpl, iface);
    int i;
    ITypeInfoImpl *pTInfo = This->pTypeInfo;

    TRACE("(%p) index %d \n",This, index);

    if(!pTKind) return E_INVALIDARG;

    /* search element n in list */
    for(i=0; i < index; i++)
    {
      if(!pTInfo)
      {
        TRACE("-- element not found\n");
        return TYPE_E_ELEMENTNOTFOUND;
      }
      pTInfo = pTInfo->next;
    }

    *pTKind = pTInfo->TypeAttr.typekind;
    TRACE("-- found Type (%d)\n", *pTKind);
    return S_OK;
}

/* ITypeLib::GetTypeInfoOfGuid
 *
 * Retrieves the type description that corresponds to the specified GUID.
 *
 */
static HRESULT WINAPI ITypeLib2_fnGetTypeInfoOfGuid(
    ITypeLib2 *iface,
    REFGUID guid,
    ITypeInfo **ppTInfo)
{
    ICOM_THIS( ITypeLibImpl, iface);
    ITypeInfoImpl *pTypeInfo = This->pTypeInfo; /* head of list */

    TRACE("(%p)\n\tguid:\t%s)\n",This,debugstr_guid(guid));

    if (!pTypeInfo) return TYPE_E_ELEMENTNOTFOUND;

    /* search linked list for guid */
    while( !IsEqualIID(guid,&pTypeInfo->TypeAttr.guid) )
    {
      pTypeInfo = pTypeInfo->next;

      if (!pTypeInfo)
      {
        /* end of list reached */
        TRACE("-- element not found\n");
        return TYPE_E_ELEMENTNOTFOUND;
      }
    }

    TRACE("-- found (%p, %s)\n",
          pTypeInfo,
          debugstr_w(pTypeInfo->Name));

    *ppTInfo = (ITypeInfo*)pTypeInfo;
    ITypeInfo_AddRef(*ppTInfo);
    return S_OK;
}

/* ITypeLib::GetLibAttr
 *
 * Retrieves the structure that contains the library's attributes.
 *
 */
static HRESULT WINAPI ITypeLib2_fnGetLibAttr(
	ITypeLib2 *iface,
	LPTLIBATTR *ppTLibAttr)
{
    ICOM_THIS( ITypeLibImpl, iface);
    TRACE("(%p)\n",This);
    *ppTLibAttr = HeapAlloc(GetProcessHeap(), 0, sizeof(**ppTLibAttr));
    memcpy(*ppTLibAttr, &This->LibAttr, sizeof(**ppTLibAttr));
    return S_OK;
}

/* ITypeLib::GetTypeComp
 *
 * Enables a client compiler to bind to a library's types, variables,
 * constants, and global functions.
 *
 */
static HRESULT WINAPI ITypeLib2_fnGetTypeComp(
	ITypeLib2 *iface,
	ITypeComp **ppTComp)
{
    ICOM_THIS( ITypeLibImpl, iface);
    FIXME("(%p): stub!\n",This);
    return E_NOTIMPL;
}

/* ITypeLib::GetDocumentation
 *
 * Retrieves the library's documentation string, the complete Help file name
 * and path, and the context identifier for the library Help topic in the Help
 * file.
 *
 * On a successful return all non-null BSTR pointers will have been set,
 * possibly to NULL.
 */
static HRESULT WINAPI ITypeLib2_fnGetDocumentation(
    ITypeLib2 *iface,
    INT index,
    BSTR *pBstrName,
    BSTR *pBstrDocString,
    DWORD *pdwHelpContext,
    BSTR *pBstrHelpFile)
{
    ICOM_THIS( ITypeLibImpl, iface);

    HRESULT result = E_INVALIDARG;

    ITypeInfo *pTInfo;


    TRACE("(%p) index %d Name(%p) DocString(%p) HelpContext(%p) HelpFile(%p)\n",
        This, index,
        pBstrName, pBstrDocString,
        pdwHelpContext, pBstrHelpFile);

    if(index<0)
    {
        /* documentation for the typelib */
        if(pBstrName)
        {
            if (This->Name)
                if(!(*pBstrName = SysAllocString(This->Name))) goto memerr1;else;
            else
                *pBstrName = NULL;
        }
        if(pBstrDocString)
        {
            if (This->DocString)
                if(!(*pBstrDocString = SysAllocString(This->DocString))) goto memerr2;else;
            else if (This->Name)
                if(!(*pBstrDocString = SysAllocString(This->Name))) goto memerr2;else;
            else
                *pBstrDocString = NULL;
        }
        if(pdwHelpContext)
        {
            *pdwHelpContext = This->dwHelpContext;
        }
        if(pBstrHelpFile)
        {
            if (This->HelpFile)
                if(!(*pBstrHelpFile = SysAllocString(This->HelpFile))) goto memerr3;else;
            else
                *pBstrHelpFile = NULL;
        }

        result = S_OK;
    }
    else
    {
        /* for a typeinfo */
        result = ITypeLib2_fnGetTypeInfo(iface, index, &pTInfo);

        if(SUCCEEDED(result))
        {
            result = ITypeInfo_GetDocumentation(pTInfo,
                                          MEMBERID_NIL,
                                          pBstrName,
                                          pBstrDocString,
                                          pdwHelpContext, pBstrHelpFile);

            ITypeInfo_Release(pTInfo);
        }
    }
    return result;
memerr3:
    if (pBstrDocString) SysFreeString (*pBstrDocString);
memerr2:
    if (pBstrName) SysFreeString (*pBstrName);
memerr1:
    return STG_E_INSUFFICIENTMEMORY;
}

/* ITypeLib::IsName
 *
 * Indicates whether a passed-in string contains the name of a type or member
 * described in the library.
 *
 */
static HRESULT WINAPI ITypeLib2_fnIsName(
	ITypeLib2 *iface,
	LPOLESTR szNameBuf,
	ULONG lHashVal,
	BOOL *pfName)
{
    ICOM_THIS( ITypeLibImpl, iface);
    ITypeInfoImpl *pTInfo;
    TLBFuncDesc *pFInfo;
    TLBVarDesc *pVInfo;
    int i;
    UINT nNameBufLen = SysStringLen(szNameBuf);

    TRACE("(%p)->(%s,%08lx,%p)\n", This, debugstr_w(szNameBuf), lHashVal,
	  pfName);

    *pfName=TRUE;
    for(pTInfo=This->pTypeInfo;pTInfo;pTInfo=pTInfo->next){
        if(!memcmp(szNameBuf,pTInfo->Name, nNameBufLen)) goto ITypeLib2_fnIsName_exit;
        for(pFInfo=pTInfo->funclist;pFInfo;pFInfo=pFInfo->next) {
            if(!memcmp(szNameBuf,pFInfo->Name, nNameBufLen)) goto ITypeLib2_fnIsName_exit;
            for(i=0;i<pFInfo->funcdesc.cParams;i++)
                if(!memcmp(szNameBuf,pFInfo->pParamDesc[i].Name, nNameBufLen))
                    goto ITypeLib2_fnIsName_exit;
        }
        for(pVInfo=pTInfo->varlist;pVInfo;pVInfo=pVInfo->next)
            if(!memcmp(szNameBuf,pVInfo->Name, nNameBufLen)) goto ITypeLib2_fnIsName_exit;

    }
    *pfName=FALSE;

ITypeLib2_fnIsName_exit:
    TRACE("(%p)slow! search for %s: %s found!\n", This,
          debugstr_w(szNameBuf), *pfName?"NOT":"");

    return S_OK;
}

/* ITypeLib::FindName
 *
 * Finds occurrences of a type description in a type library. This may be used
 * to quickly verify that a name exists in a type library.
 *
 */
static HRESULT WINAPI ITypeLib2_fnFindName(
	ITypeLib2 *iface,
	LPOLESTR szNameBuf,
	ULONG lHashVal,
	ITypeInfo **ppTInfo,
	MEMBERID *rgMemId,
	UINT16 *pcFound)
{
    ICOM_THIS( ITypeLibImpl, iface);
    ITypeInfoImpl *pTInfo;
    TLBFuncDesc *pFInfo;
    TLBVarDesc *pVInfo;
    int i,j = 0;

    UINT nNameBufLen = SysStringLen(szNameBuf);

    for(pTInfo=This->pTypeInfo;pTInfo && j<*pcFound; pTInfo=pTInfo->next){
        if(!memcmp(szNameBuf,pTInfo->Name, nNameBufLen)) goto ITypeLib2_fnFindName_exit;
        for(pFInfo=pTInfo->funclist;pFInfo;pFInfo=pFInfo->next) {
            if(!memcmp(szNameBuf,pFInfo->Name,nNameBufLen)) goto ITypeLib2_fnFindName_exit;
            for(i=0;i<pFInfo->funcdesc.cParams;i++)
                if(!memcmp(szNameBuf,pFInfo->pParamDesc[i].Name,nNameBufLen))
                    goto ITypeLib2_fnFindName_exit;
        }
        for(pVInfo=pTInfo->varlist;pVInfo;pVInfo=pVInfo->next)
            if(!memcmp(szNameBuf,pVInfo->Name, nNameBufLen)) goto ITypeLib2_fnFindName_exit;
        continue;
ITypeLib2_fnFindName_exit:
        ITypeInfo_AddRef((ITypeInfo*)pTInfo);
        ppTInfo[j]=(LPTYPEINFO)pTInfo;
        j++;
    }
    TRACE("(%p)slow! search for %d with %s: found %d TypeInfo's!\n",
          This, *pcFound, debugstr_w(szNameBuf), j);

    *pcFound=j;

    return S_OK;
}

/* ITypeLib::ReleaseTLibAttr
 *
 * Releases the TLIBATTR originally obtained from ITypeLib::GetLibAttr.
 *
 */
static VOID WINAPI ITypeLib2_fnReleaseTLibAttr(
	ITypeLib2 *iface,
	TLIBATTR *pTLibAttr)
{
    ICOM_THIS( ITypeLibImpl, iface);
    TRACE("freeing (%p)\n",This);
    HeapFree(GetProcessHeap(),0,pTLibAttr);

}

/* ITypeLib2::GetCustData
 *
 * gets the custom data
 */
static HRESULT WINAPI ITypeLib2_fnGetCustData(
	ITypeLib2 * iface,
	REFGUID guid,
        VARIANT *pVarVal)
{
    ICOM_THIS( ITypeLibImpl, iface);
    TLBCustData *pCData;

    for(pCData=This->pCustData; pCData; pCData = pCData->next)
    {
      if( IsEqualIID(guid, &pCData->guid)) break;
    }

    TRACE("(%p) guid %s %s found!x)\n", This, debugstr_guid(guid), pCData? "" : "NOT");

    if(pCData)
    {
        VariantInit( pVarVal);
        VariantCopy( pVarVal, &pCData->data);
        return S_OK;
    }
    return E_INVALIDARG;  /* FIXME: correct? */
}

/* ITypeLib2::GetLibStatistics
 *
 * Returns statistics about a type library that are required for efficient
 * sizing of hash tables.
 *
 */
static HRESULT WINAPI ITypeLib2_fnGetLibStatistics(
	ITypeLib2 * iface,
        ULONG *pcUniqueNames,
	ULONG *pcchUniqueNames)
{
    ICOM_THIS( ITypeLibImpl, iface);

    FIXME("(%p): stub!\n", This);

    if(pcUniqueNames) *pcUniqueNames=1;
    if(pcchUniqueNames) *pcchUniqueNames=1;
    return S_OK;
}

/* ITypeLib2::GetDocumentation2
 *
 * Retrieves the library's documentation string, the complete Help file name
 * and path, the localization context to use, and the context ID for the
 * library Help topic in the Help file.
 *
 */
static HRESULT WINAPI ITypeLib2_fnGetDocumentation2(
	ITypeLib2 * iface,
        INT index,
	LCID lcid,
	BSTR *pbstrHelpString,
        DWORD *pdwHelpStringContext,
	BSTR *pbstrHelpStringDll)
{
    ICOM_THIS( ITypeLibImpl, iface);
    HRESULT result;
    ITypeInfo *pTInfo;

    FIXME("(%p) index %d lcid %ld half implemented stub!\n", This, index, lcid);

    /* the help string should be obtained from the helpstringdll,
     * using the _DLLGetDocumentation function, based on the supplied
     * lcid. Nice to do sometime...
     */
    if(index<0)
    {
      /* documentation for the typelib */
      if(pbstrHelpString)
        *pbstrHelpString=SysAllocString(This->DocString);
      if(pdwHelpStringContext)
        *pdwHelpStringContext=This->dwHelpContext;
      if(pbstrHelpStringDll)
        *pbstrHelpStringDll=SysAllocString(This->HelpStringDll);

      result = S_OK;
    }
    else
    {
      /* for a typeinfo */
      result=ITypeLib2_GetTypeInfo(iface, index, &pTInfo);

      if(SUCCEEDED(result))
      {
        ITypeInfo2 * pTInfo2;
        result = ITypeInfo_QueryInterface(pTInfo,
                                          &IID_ITypeInfo2,
                                          (LPVOID*) &pTInfo2);

        if(SUCCEEDED(result))
        {
          result = ITypeInfo2_GetDocumentation2(pTInfo2,
                                           MEMBERID_NIL,
                                           lcid,
                                           pbstrHelpString,
                                           pdwHelpStringContext,
                                           pbstrHelpStringDll);

          ITypeInfo2_Release(pTInfo2);
        }

        ITypeInfo_Release(pTInfo);
      }
    }
    return result;
}

/* ITypeLib2::GetAllCustData
 *
 * Gets all custom data items for the library.
 *
 */
static HRESULT WINAPI ITypeLib2_fnGetAllCustData(
	ITypeLib2 * iface,
        CUSTDATA *pCustData)
{
    ICOM_THIS( ITypeLibImpl, iface);
    TLBCustData *pCData;
    int i;
    TRACE("(%p) returning %d items\n", This, This->ctCustData);
    pCustData->prgCustData = TLB_Alloc(This->ctCustData * sizeof(CUSTDATAITEM));
    if(pCustData->prgCustData ){
        pCustData->cCustData=This->ctCustData;
        for(i=0, pCData=This->pCustData; pCData; i++, pCData = pCData->next){
            pCustData->prgCustData[i].guid=pCData->guid;
            VariantCopy(& pCustData->prgCustData[i].varValue, & pCData->data);
        }
    }else{
        ERR(" OUT OF MEMORY! \n");
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

static ICOM_VTABLE(ITypeLib2) tlbvt = {
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    ITypeLib2_fnQueryInterface,
    ITypeLib2_fnAddRef,
    ITypeLib2_fnRelease,
    ITypeLib2_fnGetTypeInfoCount,
    ITypeLib2_fnGetTypeInfo,
    ITypeLib2_fnGetTypeInfoType,
    ITypeLib2_fnGetTypeInfoOfGuid,
    ITypeLib2_fnGetLibAttr,
    ITypeLib2_fnGetTypeComp,
    ITypeLib2_fnGetDocumentation,
    ITypeLib2_fnIsName,
    ITypeLib2_fnFindName,
    ITypeLib2_fnReleaseTLibAttr,

    ITypeLib2_fnGetCustData,
    ITypeLib2_fnGetLibStatistics,
    ITypeLib2_fnGetDocumentation2,
    ITypeLib2_fnGetAllCustData
 };

/*================== ITypeInfo(2) Methods ===================================*/
static ITypeInfo2 * WINAPI ITypeInfo_Constructor(void)
{
    ITypeInfoImpl * pTypeInfoImpl;

    pTypeInfoImpl = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(ITypeInfoImpl));
    if (pTypeInfoImpl)
    {
      ICOM_VTBL(pTypeInfoImpl) = &tinfvt;
      pTypeInfoImpl->ref=1;
    }
    TRACE("(%p)\n", pTypeInfoImpl);
    return (ITypeInfo2*) pTypeInfoImpl;
}

/* ITypeInfo::QueryInterface
 */
static HRESULT WINAPI ITypeInfo_fnQueryInterface(
	ITypeInfo2 *iface,
	REFIID riid,
	VOID **ppvObject)
{
    ICOM_THIS( ITypeLibImpl, iface);

    TRACE("(%p)->(IID: %s)\n",This,debugstr_guid(riid));

    *ppvObject=NULL;
    if(IsEqualIID(riid, &IID_IUnknown) ||
            IsEqualIID(riid,&IID_ITypeInfo)||
            IsEqualIID(riid,&IID_ITypeInfo2))
        *ppvObject = This;

    if(*ppvObject){
        ITypeInfo_AddRef(iface);
        TRACE("-- Interface: (%p)->(%p)\n",ppvObject,*ppvObject);
        return S_OK;
    }
    TRACE("-- Interface: E_NOINTERFACE\n");
    return E_NOINTERFACE;
}

/* ITypeInfo::AddRef
 */
static ULONG WINAPI ITypeInfo_fnAddRef( ITypeInfo2 *iface)
{
    ICOM_THIS( ITypeInfoImpl, iface);

    ++(This->ref);

    TRACE("(%p)->ref is %u\n",This, This->ref);
    return This->ref;
}

/* ITypeInfo::Release
 */
static ULONG WINAPI ITypeInfo_fnRelease( ITypeInfo2 *iface)
{
    ICOM_THIS( ITypeInfoImpl, iface);

    --(This->ref);

    TRACE("(%p)->(%u)\n",This, This->ref);

    if (!This->ref)
    {
      FIXME("destroy child objects\n");

      TRACE("destroying ITypeInfo(%p)\n",This);
      if (This->Name)
      {
          SysFreeString(This->Name);
          This->Name = 0;
      }

      if (This->DocString)
      {
          SysFreeString(This->DocString);
          This->DocString = 0;
      }

      if (This->next)
      {
        ITypeInfo_Release((ITypeInfo*)This->next);
      }

      HeapFree(GetProcessHeap(),0,This);
      return 0;
    }
    return This->ref;
}

/* ITypeInfo::GetTypeAttr
 *
 * Retrieves a TYPEATTR structure that contains the attributes of the type
 * description.
 *
 */
static HRESULT WINAPI ITypeInfo_fnGetTypeAttr( ITypeInfo2 *iface,
        LPTYPEATTR  *ppTypeAttr)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TRACE("(%p)\n",This);
    /* FIXME: must do a copy here */
    *ppTypeAttr=&This->TypeAttr;
    return S_OK;
}

/* ITypeInfo::GetTypeComp
 *
 * Retrieves the ITypeComp interface for the type description, which enables a
 * client compiler to bind to the type description's members.
 *
 */
static HRESULT WINAPI ITypeInfo_fnGetTypeComp( ITypeInfo2 *iface,
        ITypeComp  * *ppTComp)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    FIXME("(%p) stub!\n", This);
    return S_OK;
}

/* ITypeInfo::GetFuncDesc
 *
 * Retrieves the FUNCDESC structure that contains information about a
 * specified function.
 *
 */
static HRESULT WINAPI ITypeInfo_fnGetFuncDesc( ITypeInfo2 *iface, UINT index,
        LPFUNCDESC  *ppFuncDesc)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    int i;
    TLBFuncDesc * pFDesc;
    TRACE("(%p) index %d\n", This, index);
    for(i=0, pFDesc=This->funclist; i!=index && pFDesc; i++, pFDesc=pFDesc->next)
        ;
    if(pFDesc){
        /* FIXME: must do a copy here */
        *ppFuncDesc=&pFDesc->funcdesc;
        return S_OK;
    }
    return E_INVALIDARG;
}

/* ITypeInfo::GetVarDesc
 *
 * Retrieves a VARDESC structure that describes the specified variable.
 *
 */
static HRESULT WINAPI ITypeInfo_fnGetVarDesc( ITypeInfo2 *iface, UINT index,
        LPVARDESC  *ppVarDesc)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    int i;
    TLBVarDesc * pVDesc;
    TRACE("(%p) index %d\n", This, index);
    for(i=0, pVDesc=This->varlist; i!=index && pVDesc; i++, pVDesc=pVDesc->next)
        ;
    if(pVDesc){
        /* FIXME: must do a copy here */
        *ppVarDesc=&pVDesc->vardesc;
        return S_OK;
    }
    return E_INVALIDARG;
}

/* ITypeInfo_GetNames
 *
 * Retrieves the variable with the specified member ID (or the name of the
 * property or method and its parameters) that correspond to the specified
 * function ID.
 */
static HRESULT WINAPI ITypeInfo_fnGetNames( ITypeInfo2 *iface, MEMBERID memid,
        BSTR  *rgBstrNames, UINT cMaxNames, UINT  *pcNames)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBFuncDesc * pFDesc;
    TLBVarDesc * pVDesc;
    int i;
    TRACE("(%p) memid=0x%08lx Maxname=%d\n", This, memid, cMaxNames);
    for(pFDesc=This->funclist; pFDesc && pFDesc->funcdesc.memid != memid; pFDesc=pFDesc->next);
    if(pFDesc)
    {
      /* function found, now return function and parameter names */
      for(i=0; i<cMaxNames && i <= pFDesc->funcdesc.cParams; i++)
      {
        if(!i)
	  *rgBstrNames=SysAllocString(pFDesc->Name);
        else
          rgBstrNames[i]=SysAllocString(pFDesc->pParamDesc[i-1].Name);
      }
      *pcNames=i;
    }
    else
    {
      for(pVDesc=This->varlist; pVDesc && pVDesc->vardesc.memid != memid; pVDesc=pVDesc->next);
      if(pVDesc)
      {
        *rgBstrNames=SysAllocString(pVDesc->Name);
        *pcNames=1;
      }
      else
      {
        if(This->TypeAttr.typekind==TKIND_INTERFACE && This->TypeAttr.cImplTypes )
        {
          /* recursive search */
          ITypeInfo *pTInfo;
          HRESULT result;
          result=ITypeInfo_GetRefTypeInfo(iface, This->impltypelist->hRef,
					  &pTInfo);
          if(SUCCEEDED(result))
	  {
            result=ITypeInfo_GetNames(pTInfo, memid, rgBstrNames, cMaxNames, pcNames);
            ITypeInfo_Release(pTInfo);
            return result;
          }
          WARN("Could not search inherited interface!\n");
        }
        else
	{
          WARN("no names found\n");
	}
        *pcNames=0;
        return TYPE_E_ELEMENTNOTFOUND;
      }
    }
    return S_OK;
}


/* ITypeInfo::GetRefTypeOfImplType
 *
 * If a type description describes a COM class, it retrieves the type
 * description of the implemented interface types. For an interface,
 * GetRefTypeOfImplType returns the type information for inherited interfaces,
 * if any exist.
 *
 */
static HRESULT WINAPI ITypeInfo_fnGetRefTypeOfImplType(
	ITypeInfo2 *iface,
        UINT index,
	HREFTYPE  *pRefType)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    int(i);
    TLBImplType *pImpl = This->impltypelist;

    TRACE("(%p) index %d\n", This, index);
    dump_TypeInfo(This);

    if(index==(UINT)-1)
    {
      /* only valid on dual interfaces;
         retrieve the associated TKIND_INTERFACE handle for the current TKIND_DISPATCH
      */
      if( This->TypeAttr.typekind != TKIND_DISPATCH) return E_INVALIDARG;

      if (This->TypeAttr.wTypeFlags & TYPEFLAG_FDISPATCHABLE &&
          This->TypeAttr.wTypeFlags & TYPEFLAG_FDUAL )
      {
        *pRefType = -1;
      }
      else
      {
        if (!pImpl) return TYPE_E_ELEMENTNOTFOUND;
        *pRefType = pImpl->hRef;
      }
    }
    else
    {
      /* get element n from linked list */
      for(i=0; pImpl && i<index; i++)
      {
        pImpl = pImpl->next;
      }

      if (!pImpl) return TYPE_E_ELEMENTNOTFOUND;

      *pRefType = pImpl->hRef;

      TRACE("-- 0x%08lx\n", pImpl->hRef );
    }

    return S_OK;

}

/* ITypeInfo::GetImplTypeFlags
 *
 * Retrieves the IMPLTYPEFLAGS enumeration for one implemented interface
 * or base interface in a type description.
 */
static HRESULT WINAPI ITypeInfo_fnGetImplTypeFlags( ITypeInfo2 *iface,
        UINT index, INT  *pImplTypeFlags)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    int i;
    TLBImplType *pImpl;

    TRACE("(%p) index %d\n", This, index);
    for(i=0, pImpl=This->impltypelist; i<index && pImpl;
	i++, pImpl=pImpl->next)
        ;
    if(i==index && pImpl){
        *pImplTypeFlags=pImpl->implflags;
        return S_OK;
    }
    *pImplTypeFlags=0;
    return TYPE_E_ELEMENTNOTFOUND;
}

/* GetIDsOfNames
 * Maps between member names and member IDs, and parameter names and
 * parameter IDs.
 */
static HRESULT WINAPI ITypeInfo_fnGetIDsOfNames( ITypeInfo2 *iface,
        LPOLESTR  *rgszNames, UINT cNames, MEMBERID  *pMemId)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBFuncDesc * pFDesc;
    TLBVarDesc * pVDesc;
    HRESULT ret=S_OK;

    TRACE("(%p) Name %s cNames %d\n", This, debugstr_w(*rgszNames),
            cNames);
    for(pFDesc=This->funclist; pFDesc; pFDesc=pFDesc->next) {
        int i, j;
        if(!lstrcmpiW(*rgszNames, pFDesc->Name)) {
            if(cNames) *pMemId=pFDesc->funcdesc.memid;
            for(i=1; i < cNames; i++){
                for(j=0; j<pFDesc->funcdesc.cParams; j++)
                    if(!lstrcmpiW(rgszNames[i],pFDesc->pParamDesc[j].Name))
                            break;
                if( j<pFDesc->funcdesc.cParams)
                    pMemId[i]=j;
                else
                   ret=DISP_E_UNKNOWNNAME;
            };
            return ret;
        }
    }
    for(pVDesc=This->varlist; pVDesc; pVDesc=pVDesc->next) {
        if(!lstrcmpiW(*rgszNames, pVDesc->Name)) {
            if(cNames) *pMemId=pVDesc->vardesc.memid;
            return ret;
        }
    }
    /* not found, see if this is and interface with an inheritance */
    if((This->TypeAttr.typekind==TKIND_INTERFACE ||
        (This->TypeAttr.typekind==TKIND_DISPATCH && (This->TypeAttr.wTypeFlags & TYPEFLAG_FDUAL))
	   ) &&
       This->TypeAttr.cImplTypes )
    {
        /* recursive search */
        ITypeInfo *pTInfo;
        ret=ITypeInfo_GetRefTypeInfo(iface,
                This->impltypelist->hRef, &pTInfo);
        if(SUCCEEDED(ret)){
            ret=ITypeInfo_GetIDsOfNames(pTInfo, rgszNames, cNames, pMemId );
            ITypeInfo_Release(pTInfo);
            return ret;
        }
        WARN("Could not search inherited interface!\n");
    } else
        WARN("no names found\n");
    return DISP_E_UNKNOWNNAME;
}

/* ITypeInfo::Invoke
 *
 * Invokes a method, or accesses a property of an object, that implements the
 * interface described by the type description.
 */
DWORD
_invoke(LPVOID func,CALLCONV callconv, int nrargs, DWORD *args) {
    DWORD res;

    if (TRACE_ON(ole)) {
	int i;
	MESSAGE("Calling %p(",func);
	for (i=0;i<nrargs;i++) MESSAGE("%08lx,",args[i]);
	MESSAGE(")\n");
    }

    switch (callconv) {
    case CC_STDCALL:

	switch (nrargs) {
	case 0: {
		DWORD (WINAPI *xfunc)() = func;
		res = xfunc();
		break;
	}
	case 1: {
		DWORD (WINAPI *xfunc)(DWORD) = func;
		res = xfunc(args[0]);
		break;
	}
	case 2: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD) = func;
		res = xfunc(args[0],args[1]);
		break;
	}
	case 3: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD,DWORD) = func;
		res = xfunc(args[0],args[1],args[2]);
		break;
	}
	case 4: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD,DWORD,DWORD) = func;
		res = xfunc(args[0],args[1],args[2],args[3]);
		break;
	}
	case 5: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD,DWORD,DWORD,DWORD) = func;
		res = xfunc(args[0],args[1],args[2],args[3],args[4]);
		break;
	}
	case 6: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD) = func;
		res = xfunc(args[0],args[1],args[2],args[3],args[4],args[5]);
		break;
	}
	case 7: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD) = func;
		res = xfunc(args[0],args[1],args[2],args[3],args[4],args[5],args[6]);
		break;
	}
	case 8: {
		DWORD (WINAPI *xfunc)(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD) = func;
		res = xfunc(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
		break;
	}
	default:
		FIXME("unsupported number of arguments %d in stdcall\n",nrargs);
		res = -1;
		break;
	}
	break;
    default:
	FIXME("unsupported calling convention %d\n",callconv);
	res = -1;
	break;
    }
    TRACE("returns %08lx\n",res);
    return res;
}

static HRESULT set_arg_for_invoke( const LPTYPEINFO pTInfo, VARIANT* pVar, const TYPEDESC* pTDescDest, LPDWORD lpdwArg, VARIANT* pVarStorage )
{
    HRESULT hr = S_OK;

    if (pTDescDest->vt == VT_PTR) {
	TRACE( "%d to ptr to %d\n", V_VT(pVar), pTDescDest->u.lptdesc->vt );
    } else {
	TRACE( "%d to %d\n", V_VT(pVar), pTDescDest->vt );
    }

	if( V_VT(pVar) == pTDescDest->vt )
	{
	    /* Already the same type, just copy */
        *lpdwArg = V_UNION(pVar,lVal);
	    return hr;
	}

	/*
	 * If the provided type and the function type don't match,
	 * we must coerce our argument into the correct type.
	 */
	if( pTDescDest->vt == VT_USERDEFINED )
	{
        HREFTYPE hType = pTDescDest->u.hreftype;
        LPTYPEINFO pTInfo2;
        LPTYPEATTR pAttr;

        hr = ITypeInfo_GetRefTypeInfo(pTInfo, hType, &pTInfo2);
        if( FAILED(hr) ) return hr;

        hr = ITypeInfo_GetTypeAttr(pTInfo2, &pAttr);
        if( FAILED(hr) )
        {
            ITypeInfo_Release(pTInfo2);
            return hr;
        }

        switch (pAttr->typekind)
        {
          case TKIND_ENUM:
            TRACE( "ENUM type %d\n", V_VT(pVar) );
            if( V_VT(pVar) == VT_I4 )
            {
              *lpdwArg = V_UNION(pVar,lVal);
            }
            else if( V_VT(pVar) == (VT_I4 | VT_BYREF) )
            {
              *lpdwArg = *V_UNION(pVar,plVal);
            }
            else
            {
              /* Would it just be good enough to copy over lVal as well for all cases? */
              ERR( "TKIND_ENUM type of %d unhandled\n", V_VT(pVar) );
              hr = DISP_E_TYPEMISMATCH;
            }
            break;
          case TKIND_ALIAS:
            TRACE( "TKIND_ALIAS to vt=%d\n", pAttr->tdescAlias.vt );
            hr = set_arg_for_invoke( pTInfo2, pVar, &pAttr->tdescAlias, lpdwArg, pVarStorage );
            break;
          default:
            ERR( "Unhandled typekind %d\n", pAttr->typekind );
            hr = DISP_E_TYPEMISMATCH;
            break;
        }

        ITypeInfo_ReleaseTypeAttr(pTInfo2, pAttr);
        ITypeInfo_Release(pTInfo2);
	}
    else if( (pTDescDest->vt == VT_PTR) && V_ISBYREF(pVar) &&
             (pTDescDest->u.lptdesc->vt == (V_VT(pVar) & ~VT_BYREF))  )
    {
        *(PVOID*)lpdwArg = V_UNION(pVar,byref);
    }
	else
	{
        hr = VariantChangeTypeEx( pVarStorage, pVar, 0, 0, pTDescDest->vt );

        if( SUCCEEDED(hr) )
        {
          /* FIXME: pVarStorage could be up to 8 bytes...and we're passing in 4 bytes */
          *lpdwArg = V_UNION(pVarStorage,lVal);
        }
        else
        {
          ERR( "failed hr=0x%lx\n", hr );
        }
    }

    return hr;
}
static HRESULT WINAPI ITypeInfo_fnInvoke(
    ITypeInfo2 *iface,
    VOID  *pIUnk,
    MEMBERID memid,
    UINT16 dwFlags,
    DISPPARAMS  *pDispParams,
    VARIANT  *pVarResult,
    EXCEPINFO  *pExcepInfo,
    UINT  *pArgErr)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBFuncDesc * pFDesc;
    TLBVarDesc * pVDesc;
    int i;

    TRACE("(%p)(%p,id=%ld,flags=0x%08x,%p,%p,%p,%p) partial stub!\n",
      This,pIUnk,memid,dwFlags,pDispParams,pVarResult,pExcepInfo,pArgErr
    );
    dump_DispParms(pDispParams);

    for(pFDesc=This->funclist; pFDesc; pFDesc=pFDesc->next)
	if (pFDesc->funcdesc.memid == memid) {
	    if (pFDesc->funcdesc.invkind & dwFlags)
		break;
	}
    if (pFDesc) {
	dump_TLBFuncDescOne(pFDesc);
	switch (pFDesc->funcdesc.funckind) {
	case FUNC_PUREVIRTUAL:
	case FUNC_VIRTUAL: {
	    DWORD res;
	    DWORD *args = (DWORD*)HeapAlloc(GetProcessHeap(),0,sizeof(DWORD)*(pFDesc->funcdesc.cParams+1));
	    DWORD *args2 = (DWORD*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(DWORD)*(pFDesc->funcdesc.cParams));
	    VARIANT* vars = (VARIANT*)HeapAlloc(GetProcessHeap(), 0, sizeof(VARIANT)*(pFDesc->funcdesc.cParams));

	    args[0] = (DWORD)pIUnk;

	    for (i=0;i<pFDesc->funcdesc.cParams;i++) {
		TYPEDESC *tdesc = &(pFDesc->funcdesc.lprgelemdescParam[i].tdesc);

		VariantInit( &vars[i] );

		if (i<pDispParams->cArgs) {
		    VARIANT *varg = &pDispParams->rgvarg[pDispParams->cArgs-i-1];
		    HRESULT hr;

		    TRACE("set %d to disparg type %d vs %d\n",i,V_VT(varg),tdesc->vt);

		    hr = set_arg_for_invoke( iface, varg, tdesc, &args[i+1], &vars[i] );

		    if( FAILED(hr) )
		    {
			   /* Should set pArgErr if hr is DISP_E_TYPEMISMATCH or DISP_E_PARAMNOTFOUND */
			   FIXME( "set_arg_for_invoke failed 0x%lx\n", hr );
			   return hr;
		    }

		} else {
		    TRACE("set %d to pointer for get (type is %d)\n",i,tdesc->vt);
		    /*FIXME: give pointers for the rest, so propertyget works*/
		    args[i+1] = (DWORD)&args2[i];

		    /* If pointer to variant, pass reference to variant
		     * in result variant array.
		     */
		    if ((tdesc->vt == VT_PTR) &&
			    (tdesc->u.lptdesc->vt == VT_VARIANT) &&
			     pVarResult
		       )
			   args[i+1] = (DWORD)(pVarResult+(i-pDispParams->cArgs));
		}
	    }
	    if (pFDesc->funcdesc.cParamsOpt)
            FIXME("Does not support optional parameters (%d)\n",
                pFDesc->funcdesc.cParamsOpt
		);

	    res = _invoke((*(DWORD***)pIUnk)[pFDesc->funcdesc.oVft/4],
		    pFDesc->funcdesc.callconv,
		    pFDesc->funcdesc.cParams+1,
		    args
	    );

            /* Update any out parms with the values obtained from invoke */
            for( i=0; i < pDispParams->cArgs; i++ )
            {
                if( pFDesc->funcdesc.lprgelemdescParam[i].u.paramdesc.wParamFlags & PARAMFLAG_FOUT )
		{
#if 0
	  	    /* Copy updated value from args[i] to the correct position in disparms */
		    VARIANT *varg = &pDispParams->rgvarg[pDispParams->cArgs-i-1];
                    V_UNION(varg,lVal) = args[i+1];
	            dump_Variant( varg );
#endif
		    FIXME( "out non retval parms won't work\n" );
		}
	    }

           if (pVarResult && !(dwFlags & (DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF)) )
	   {
		for (i=0;i<pFDesc->funcdesc.cParams-pDispParams->cArgs;i++) {
		    TYPEDESC *tdesc = &(pFDesc->funcdesc.lprgelemdescParam[i+pDispParams->cArgs].tdesc);
		    /* If we are a pointer to a variant, we are done already */
		    if ((tdesc->vt==VT_PTR)&&(tdesc->u.lptdesc->vt==VT_VARIANT))
			continue;

		    VariantInit(pVarResult);
		    V_UNION(pVarResult,intVal) = args2[i+pDispParams->cArgs];

		    if (tdesc->vt == VT_PTR)
			tdesc = tdesc->u.lptdesc;
		    V_VT(pVarResult) = tdesc->vt;

		    /* HACK: VB5 likes this.
		     * I do not know why. There is 1 example in MSDN which uses
		     * this which appears broken (mixes int vals and
		     * IDispatch*.).
		     */
		    if ((tdesc->vt == VT_PTR) && (dwFlags & DISPATCH_METHOD))
			V_VT(pVarResult) = VT_DISPATCH;
		    TRACE("storing into varresult: [%d]\n", i);
		    dump_Variant(pVarResult);
		}
	    }

	    for (i=0;i<pFDesc->funcdesc.cParams;i++)
	    {
		   VariantClear( &vars[i] );
	    }

	    HeapFree(GetProcessHeap(),0,vars);
	    HeapFree(GetProcessHeap(),0,args2);
	    HeapFree(GetProcessHeap(),0,args);

	    if (pFDesc->funcdesc.elemdescFunc.tdesc.vt == VT_HRESULT &&
	        FAILED(res)) {
		   TRACE("return exception\n");
		   if (pExcepInfo) {
		       pExcepInfo->scode = res;
		       /* FIXME: fill in the rest? */
		   }
		   return DISP_E_EXCEPTION;
	    }

	    return S_OK;
	}
	case FUNC_DISPATCH:  {
	   IDispatch *disp;
	   HRESULT hr;

	   hr = IUnknown_QueryInterface((LPUNKNOWN)pIUnk,&IID_IDispatch,(LPVOID*)&disp);
	   if (hr) {
	       FIXME("FUNC_DISPATCH used on object without IDispatch iface?\n");
	       return hr;
	   }
	   FIXME("Calling Invoke in IDispatch iface. untested!\n");
	   hr = IDispatch_Invoke(
	       disp,memid,&IID_NULL,LOCALE_USER_DEFAULT,dwFlags,pDispParams,
	       pVarResult,pExcepInfo,pArgErr
	   );
	   if (hr)
	       FIXME("IDispatch::Invoke failed with %08lx. (Could be not a real error?)\n",hr);
	   IDispatch_Release(disp);
	   return hr;
	}
	default:
	   FIXME("Unknown function invocation type %d\n",pFDesc->funcdesc.funckind);
	   return E_FAIL;
	}
    } else {
	for(pVDesc=This->varlist; pVDesc; pVDesc=pVDesc->next) {
	    if (pVDesc->vardesc.memid == memid) {
		FIXME("varseek: Found memid name %s, but variable-based invoking not supported\n",debugstr_w(((LPWSTR)pVDesc->Name)));
		dump_TLBVarDesc(pVDesc);
		break;
	    }
	}
    }

    /* not found, look for it in inherited interfaces */
    if ( ( This->TypeAttr.typekind==TKIND_INTERFACE ||
         (This->TypeAttr.typekind==TKIND_DISPATCH && (This->TypeAttr.wTypeFlags & TYPEFLAG_FDUAL)))
        && This->TypeAttr.cImplTypes) {

        /* recursive search */
        ITypeInfo *pTInfo;
        HRESULT hr;
        hr=ITypeInfo_GetRefTypeInfo(iface, This->impltypelist->hRef, &pTInfo);
        if(SUCCEEDED(hr)){
            hr=ITypeInfo_Invoke(pTInfo,pIUnk,memid,dwFlags,pDispParams,pVarResult,pExcepInfo,pArgErr);
            ITypeInfo_Release(pTInfo);
            return hr;
        }
        WARN("Could not search inherited interface!\n");
    }
    ERR("did not find member id %d, flags %d!\n", (int)memid, dwFlags);
    return DISP_E_MEMBERNOTFOUND;
}

/* ITypeInfo::GetDocumentation
 *
 * Retrieves the documentation string, the complete Help file name and path,
 * and the context ID for the Help topic for a specified type description.
 */
static HRESULT WINAPI ITypeInfo_fnGetDocumentation( ITypeInfo2 *iface,
        MEMBERID memid, BSTR  *pBstrName, BSTR  *pBstrDocString,
        DWORD  *pdwHelpContext, BSTR  *pBstrHelpFile)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBFuncDesc * pFDesc;
    TLBVarDesc * pVDesc;
    TRACE("(%p) memid %ld Name(%p) DocString(%p)"
          " HelpContext(%p) HelpFile(%p)\n",
        This, memid, pBstrName, pBstrDocString, pdwHelpContext, pBstrHelpFile);
    if(memid==MEMBERID_NIL){ /* documentation for the typeinfo */
        if(pBstrName)
            *pBstrName=SysAllocString(This->Name);
        if(pBstrDocString)
            *pBstrDocString=SysAllocString(This->DocString);
        if(pdwHelpContext)
            *pdwHelpContext=This->dwHelpContext;
        if(pBstrHelpFile)
            *pBstrHelpFile=SysAllocString(This->DocString);/* FIXME */
        return S_OK;
    }else {/* for a member */
    for(pFDesc=This->funclist; pFDesc; pFDesc=pFDesc->next)
        if(pFDesc->funcdesc.memid==memid){
	  if(pBstrName)
	    *pBstrName = SysAllocString(pFDesc->Name);
	  if(pBstrDocString)
            *pBstrDocString=SysAllocString(pFDesc->HelpString);
	  if(pdwHelpContext)
            *pdwHelpContext=pFDesc->helpcontext;
	  return S_OK;
        }
    for(pVDesc=This->varlist; pVDesc; pVDesc=pVDesc->next)
        if(pVDesc->vardesc.memid==memid){
	    FIXME("Not implemented\n");
            return S_OK;
        }
    }
    return TYPE_E_ELEMENTNOTFOUND;
}

/*  ITypeInfo::GetDllEntry
 *
 * Retrieves a description or specification of an entry point for a function
 * in a DLL.
 */
static HRESULT WINAPI ITypeInfo_fnGetDllEntry( ITypeInfo2 *iface, MEMBERID memid,
        INVOKEKIND invKind, BSTR  *pBstrDllName, BSTR  *pBstrName,
        WORD  *pwOrdinal)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    FIXME("(%p) stub!\n", This);
    return E_FAIL;
}

/* ITypeInfo::GetRefTypeInfo
 *
 * If a type description references other type descriptions, it retrieves
 * the referenced type descriptions.
 */
static HRESULT WINAPI ITypeInfo_fnGetRefTypeInfo(
	ITypeInfo2 *iface,
        HREFTYPE hRefType,
	ITypeInfo  **ppTInfo)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    HRESULT result = E_FAIL;


    if (hRefType == -1 &&
	(((ITypeInfoImpl*) This)->TypeAttr.typekind   == TKIND_DISPATCH) &&
	(((ITypeInfoImpl*) This)->TypeAttr.wTypeFlags &  TYPEFLAG_FDUAL))
    {
	  /* when we meet a DUAL dispinterface, we must create the interface
	  * version of it.
	  */
	  ITypeInfoImpl* pTypeInfoImpl = (ITypeInfoImpl*) ITypeInfo_Constructor();


	  /* the interface version contains the same information as the dispinterface
	   * copy the contents of the structs.
	   */
	  *pTypeInfoImpl = *This;
	  pTypeInfoImpl->ref = 1;

	  /* change the type to interface */
	  pTypeInfoImpl->TypeAttr.typekind = TKIND_INTERFACE;

	  *ppTInfo = (ITypeInfo*) pTypeInfoImpl;

	  ITypeInfo_AddRef((ITypeInfo*) pTypeInfoImpl);

	  result = S_OK;

    } else {
        TLBRefType *pRefType;
        for(pRefType = This->reflist; pRefType; pRefType = pRefType->next) {
	    if(pRefType->reference == hRefType)
	        break;
	}
	if(!pRefType)
	  FIXME("Can't find pRefType for ref %lx\n", hRefType);
	if(pRefType && hRefType != -1) {
            ITypeLib *pTLib = NULL;

	    if(pRefType->pImpTLInfo == TLB_REF_INTERNAL) {
	        int Index;
		result = ITypeInfo_GetContainingTypeLib(iface, &pTLib, &Index);
	    } else {
	        if(pRefType->pImpTLInfo->pImpTypeLib) {
		    TRACE("typeinfo in imported typelib that is already loaded\n");
		    pTLib = (ITypeLib*)pRefType->pImpTLInfo->pImpTypeLib;
		    ITypeLib2_AddRef((ITypeLib*) pTLib);
		    result = S_OK;
		} else {
		    TRACE("typeinfo in imported typelib that isn't already loaded\n");
		    result = LoadRegTypeLib( &pRefType->pImpTLInfo->guid,
					     pRefType->pImpTLInfo->wVersionMajor,
					     pRefType->pImpTLInfo->wVersionMinor,
					     pRefType->pImpTLInfo->lcid,
					     &pTLib);

		    if(!SUCCEEDED(result)) {
		        BSTR libnam=SysAllocString(pRefType->pImpTLInfo->name);
			result=LoadTypeLib(libnam, &pTLib);
			SysFreeString(libnam);
		    }
		    if(SUCCEEDED(result)) {
		        pRefType->pImpTLInfo->pImpTypeLib = (ITypeLibImpl*)pTLib;
			ITypeLib2_AddRef(pTLib);
		    }
		}
	    }
	    if(SUCCEEDED(result)) {
	        if(pRefType->index == TLB_REF_USE_GUID)
		    result = ITypeLib2_GetTypeInfoOfGuid(pTLib,
							 &pRefType->guid,
							 ppTInfo);
		else
		    result = ITypeLib2_GetTypeInfo(pTLib, pRefType->index,
						   ppTInfo);
	    }
	    if (pTLib != NULL)
		ITypeLib2_Release(pTLib);
	}
    }

    TRACE("(%p) hreftype 0x%04lx load %s (%p)\n", This, hRefType,
          SUCCEEDED(result)? "SUCCESS":"FAILURE", *ppTInfo);
    return result;
}

/* ITypeInfo::AddressOfMember
 *
 * Retrieves the addresses of static functions or variables, such as those
 * defined in a DLL.
 */
static HRESULT WINAPI ITypeInfo_fnAddressOfMember( ITypeInfo2 *iface,
        MEMBERID memid, INVOKEKIND invKind, PVOID *ppv)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    FIXME("(%p) stub!\n", This);
    return S_OK;
}

/* ITypeInfo::CreateInstance
 *
 * Creates a new instance of a type that describes a component object class
 * (coclass).
 */
static HRESULT WINAPI ITypeInfo_fnCreateInstance( ITypeInfo2 *iface,
        IUnknown *pUnk, REFIID riid, VOID  **ppvObj)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    FIXME("(%p) stub!\n", This);
    return S_OK;
}

/* ITypeInfo::GetMops
 *
 * Retrieves marshaling information.
 */
static HRESULT WINAPI ITypeInfo_fnGetMops( ITypeInfo2 *iface, MEMBERID memid,
				BSTR  *pBstrMops)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    FIXME("(%p) stub!\n", This);
    return S_OK;
}

/* ITypeInfo::GetContainingTypeLib
 *
 * Retrieves the containing type library and the index of the type description
 * within that type library.
 */
static HRESULT WINAPI ITypeInfo_fnGetContainingTypeLib( ITypeInfo2 *iface,
        ITypeLib  * *ppTLib, UINT  *pIndex)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    if (!pIndex)
        return E_INVALIDARG;
    *ppTLib=(LPTYPELIB )(This->pTypeLib);
    *pIndex=This->index;
    ITypeLib2_AddRef(*ppTLib);
    TRACE("(%p) returns (%p) index %d!\n", This, *ppTLib, *pIndex);
    return S_OK;
}

/* ITypeInfo::ReleaseTypeAttr
 *
 * Releases a TYPEATTR previously returned by GetTypeAttr.
 *
 */
static void WINAPI ITypeInfo_fnReleaseTypeAttr( ITypeInfo2 *iface,
        TYPEATTR* pTypeAttr)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TRACE("(%p)->(%p)\n", This, pTypeAttr);
}

/* ITypeInfo::ReleaseFuncDesc
 *
 * Releases a FUNCDESC previously returned by GetFuncDesc. *
 */
static void WINAPI ITypeInfo_fnReleaseFuncDesc(
	ITypeInfo2 *iface,
        FUNCDESC *pFuncDesc)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TRACE("(%p)->(%p)\n", This, pFuncDesc);
}

/* ITypeInfo::ReleaseVarDesc
 *
 * Releases a VARDESC previously returned by GetVarDesc.
 */
static void WINAPI ITypeInfo_fnReleaseVarDesc( ITypeInfo2 *iface,
        VARDESC *pVarDesc)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TRACE("(%p)->(%p)\n", This, pVarDesc);
}

/* ITypeInfo2::GetTypeKind
 *
 * Returns the TYPEKIND enumeration quickly, without doing any allocations.
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetTypeKind( ITypeInfo2 * iface,
    TYPEKIND *pTypeKind)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    *pTypeKind=This->TypeAttr.typekind;
    TRACE("(%p) type 0x%0x\n", This,*pTypeKind);
    return S_OK;
}

/* ITypeInfo2::GetTypeFlags
 *
 * Returns the type flags without any allocations. This returns a DWORD type
 * flag, which expands the type flags without growing the TYPEATTR (type
 * attribute).
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetTypeFlags( ITypeInfo2 * iface,
    ULONG *pTypeFlags)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    *pTypeFlags=This->TypeAttr.wTypeFlags;
    TRACE("(%p) flags 0x%04lx\n", This,*pTypeFlags);
    return S_OK;
}

/* ITypeInfo2::GetFuncIndexOfMemId
 * Binds to a specific member based on a known DISPID, where the member name
 * is not known (for example, when binding to a default member).
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetFuncIndexOfMemId( ITypeInfo2 * iface,
    MEMBERID memid, INVOKEKIND invKind, UINT *pFuncIndex)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBFuncDesc *pFuncInfo;
    int i;
    HRESULT result;
    /* FIXME: should check for invKind??? */
    for(i=0, pFuncInfo=This->funclist;pFuncInfo &&
            memid != pFuncInfo->funcdesc.memid; i++, pFuncInfo=pFuncInfo->next);
    if(pFuncInfo){
        *pFuncIndex=i;
        result= S_OK;
    }else{
        *pFuncIndex=0;
        result=E_INVALIDARG;
    }
    TRACE("(%p) memid 0x%08lx invKind 0x%04x -> %s\n", This,
          memid, invKind, SUCCEEDED(result)? "SUCCES":"FAILED");
    return result;
}

/* TypeInfo2::GetVarIndexOfMemId
 *
 * Binds to a specific member based on a known DISPID, where the member name
 * is not known (for example, when binding to a default member).
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetVarIndexOfMemId( ITypeInfo2 * iface,
    MEMBERID memid, UINT *pVarIndex)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBVarDesc *pVarInfo;
    int i;
    HRESULT result;
    for(i=0, pVarInfo=This->varlist; pVarInfo &&
            memid != pVarInfo->vardesc.memid; i++, pVarInfo=pVarInfo->next)
        ;
    if(pVarInfo){
        *pVarIndex=i;
        result= S_OK;
    }else{
        *pVarIndex=0;
        result=E_INVALIDARG;
    }
    TRACE("(%p) memid 0x%08lx -> %s\n", This,
          memid, SUCCEEDED(result)? "SUCCES":"FAILED");
    return result;
}

/* ITypeInfo2::GetCustData
 *
 * Gets the custom data
 */
static HRESULT WINAPI ITypeInfo2_fnGetCustData(
	ITypeInfo2 * iface,
	REFGUID guid,
	VARIANT *pVarVal)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData;

    for(pCData=This->pCustData; pCData; pCData = pCData->next)
        if( IsEqualIID(guid, &pCData->guid)) break;

    TRACE("(%p) guid %s %s found!x)\n", This, debugstr_guid(guid), pCData? "" : "NOT");

    if(pCData)
    {
        VariantInit( pVarVal);
        VariantCopy( pVarVal, &pCData->data);
        return S_OK;
    }
    return E_INVALIDARG;  /* FIXME: correct? */
}

/* ITypeInfo2::GetFuncCustData
 *
 * Gets the custom data
 */
static HRESULT WINAPI ITypeInfo2_fnGetFuncCustData(
	ITypeInfo2 * iface,
	UINT index,
	REFGUID guid,
	VARIANT *pVarVal)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData=NULL;
    TLBFuncDesc * pFDesc;
    int i;
    for(i=0, pFDesc=This->funclist; i!=index && pFDesc; i++,
            pFDesc=pFDesc->next);

    if(pFDesc)
        for(pCData=pFDesc->pCustData; pCData; pCData = pCData->next)
            if( IsEqualIID(guid, &pCData->guid)) break;

    TRACE("(%p) guid %s %s found!x)\n", This, debugstr_guid(guid), pCData? "" : "NOT");

    if(pCData){
        VariantInit( pVarVal);
        VariantCopy( pVarVal, &pCData->data);
        return S_OK;
    }
    return E_INVALIDARG;  /* FIXME: correct? */
}

/* ITypeInfo2::GetParamCustData
 *
 * Gets the custom data
 */
static HRESULT WINAPI ITypeInfo2_fnGetParamCustData(
	ITypeInfo2 * iface,
	UINT indexFunc,
	UINT indexParam,
	REFGUID guid,
	VARIANT *pVarVal)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData=NULL;
    TLBFuncDesc * pFDesc;
    int i;

    for(i=0, pFDesc=This->funclist; i!=indexFunc && pFDesc; i++,pFDesc=pFDesc->next);

    if(pFDesc && indexParam >=0 && indexParam<pFDesc->funcdesc.cParams)
        for(pCData=pFDesc->pParamDesc[indexParam].pCustData; pCData;
                pCData = pCData->next)
            if( IsEqualIID(guid, &pCData->guid)) break;

    TRACE("(%p) guid %s %s found!x)\n", This, debugstr_guid(guid), pCData? "" : "NOT");

    if(pCData)
    {
        VariantInit( pVarVal);
        VariantCopy( pVarVal, &pCData->data);
        return S_OK;
    }
    return E_INVALIDARG;  /* FIXME: correct? */
}

/* ITypeInfo2::GetVarCustData
 *
 * Gets the custom data
 */
static HRESULT WINAPI ITypeInfo2_fnGetVarCustData(
	ITypeInfo2 * iface,
	UINT index,
	REFGUID guid,
	VARIANT *pVarVal)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData=NULL;
    TLBVarDesc * pVDesc;
    int i;

    for(i=0, pVDesc=This->varlist; i!=index && pVDesc; i++, pVDesc=pVDesc->next);

    if(pVDesc)
    {
      for(pCData=pVDesc->pCustData; pCData; pCData = pCData->next)
      {
        if( IsEqualIID(guid, &pCData->guid)) break;
      }
    }

    TRACE("(%p) guid %s %s found!x)\n", This, debugstr_guid(guid), pCData? "" : "NOT");

    if(pCData)
    {
        VariantInit( pVarVal);
        VariantCopy( pVarVal, &pCData->data);
        return S_OK;
    }
    return E_INVALIDARG;  /* FIXME: correct? */
}

/* ITypeInfo2::GetImplCustData
 *
 * Gets the custom data
 */
static HRESULT WINAPI ITypeInfo2_fnGetImplTypeCustData(
	ITypeInfo2 * iface,
	UINT index,
	REFGUID guid,
	VARIANT *pVarVal)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData=NULL;
    TLBImplType * pRDesc;
    int i;

    for(i=0, pRDesc=This->impltypelist; i!=index && pRDesc; i++, pRDesc=pRDesc->next);

    if(pRDesc)
    {
      for(pCData=pRDesc->pCustData; pCData; pCData = pCData->next)
      {
        if( IsEqualIID(guid, &pCData->guid)) break;
      }
    }

    TRACE("(%p) guid %s %s found!x)\n", This, debugstr_guid(guid), pCData? "" : "NOT");

    if(pCData)
    {
        VariantInit( pVarVal);
        VariantCopy( pVarVal, &pCData->data);
        return S_OK;
    }
    return E_INVALIDARG;  /* FIXME: correct? */
}

/* ITypeInfo2::GetDocumentation2
 *
 * Retrieves the documentation string, the complete Help file name and path,
 * the localization context to use, and the context ID for the library Help
 * topic in the Help file.
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetDocumentation2(
	ITypeInfo2 * iface,
	MEMBERID memid,
	LCID lcid,
	BSTR *pbstrHelpString,
	DWORD *pdwHelpStringContext,
	BSTR *pbstrHelpStringDll)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBFuncDesc * pFDesc;
    TLBVarDesc * pVDesc;
    TRACE("(%p) memid %ld lcid(0x%lx)  HelpString(%p) "
          "HelpStringContext(%p) HelpStringDll(%p)\n",
          This, memid, lcid, pbstrHelpString, pdwHelpStringContext,
          pbstrHelpStringDll );
    /* the help string should be obtained from the helpstringdll,
     * using the _DLLGetDocumentation function, based on the supplied
     * lcid. Nice to do sometime...
     */
    if(memid==MEMBERID_NIL){ /* documentation for the typeinfo */
        if(pbstrHelpString)
            *pbstrHelpString=SysAllocString(This->Name);
        if(pdwHelpStringContext)
            *pdwHelpStringContext=This->dwHelpStringContext;
        if(pbstrHelpStringDll)
            *pbstrHelpStringDll=
                SysAllocString(This->pTypeLib->HelpStringDll);/* FIXME */
        return S_OK;
    }else {/* for a member */
    for(pFDesc=This->funclist; pFDesc; pFDesc=pFDesc->next)
        if(pFDesc->funcdesc.memid==memid){
             if(pbstrHelpString)
                *pbstrHelpString=SysAllocString(pFDesc->HelpString);
            if(pdwHelpStringContext)
                *pdwHelpStringContext=pFDesc->HelpStringContext;
            if(pbstrHelpStringDll)
                *pbstrHelpStringDll=
                    SysAllocString(This->pTypeLib->HelpStringDll);/* FIXME */
        return S_OK;
    }
    for(pVDesc=This->varlist; pVDesc; pVDesc=pVDesc->next)
        if(pVDesc->vardesc.memid==memid){
             if(pbstrHelpString)
                *pbstrHelpString=SysAllocString(pVDesc->HelpString);
            if(pdwHelpStringContext)
                *pdwHelpStringContext=pVDesc->HelpStringContext;
            if(pbstrHelpStringDll)
                *pbstrHelpStringDll=
                    SysAllocString(This->pTypeLib->HelpStringDll);/* FIXME */
            return S_OK;
        }
    }
    return TYPE_E_ELEMENTNOTFOUND;
}

/* ITypeInfo2::GetAllCustData
 *
 * Gets all custom data items for the Type info.
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetAllCustData(
	ITypeInfo2 * iface,
	CUSTDATA *pCustData)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData;
    int i;

    TRACE("(%p) returning %d items\n", This, This->ctCustData);

    pCustData->prgCustData = TLB_Alloc(This->ctCustData * sizeof(CUSTDATAITEM));
    if(pCustData->prgCustData ){
        pCustData->cCustData=This->ctCustData;
        for(i=0, pCData=This->pCustData; pCData; i++, pCData = pCData->next){
            pCustData->prgCustData[i].guid=pCData->guid;
            VariantCopy(& pCustData->prgCustData[i].varValue, & pCData->data);
        }
    }else{
        ERR(" OUT OF MEMORY! \n");
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

/* ITypeInfo2::GetAllFuncCustData
 *
 * Gets all custom data items for the specified Function
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetAllFuncCustData(
	ITypeInfo2 * iface,
	UINT index,
	CUSTDATA *pCustData)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData;
    TLBFuncDesc * pFDesc;
    int i;
    TRACE("(%p) index %d\n", This, index);
    for(i=0, pFDesc=This->funclist; i!=index && pFDesc; i++,
            pFDesc=pFDesc->next)
        ;
    if(pFDesc){
        pCustData->prgCustData =
            TLB_Alloc(pFDesc->ctCustData * sizeof(CUSTDATAITEM));
        if(pCustData->prgCustData ){
            pCustData->cCustData=pFDesc->ctCustData;
            for(i=0, pCData=pFDesc->pCustData; pCData; i++,
                    pCData = pCData->next){
                pCustData->prgCustData[i].guid=pCData->guid;
                VariantCopy(& pCustData->prgCustData[i].varValue,
                        & pCData->data);
            }
        }else{
            ERR(" OUT OF MEMORY! \n");
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
    return TYPE_E_ELEMENTNOTFOUND;
}

/* ITypeInfo2::GetAllParamCustData
 *
 * Gets all custom data items for the Functions
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetAllParamCustData( ITypeInfo2 * iface,
    UINT indexFunc, UINT indexParam, CUSTDATA *pCustData)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData=NULL;
    TLBFuncDesc * pFDesc;
    int i;
    TRACE("(%p) index %d\n", This, indexFunc);
    for(i=0, pFDesc=This->funclist; i!=indexFunc && pFDesc; i++,
            pFDesc=pFDesc->next)
        ;
    if(pFDesc && indexParam >=0 && indexParam<pFDesc->funcdesc.cParams){
        pCustData->prgCustData =
            TLB_Alloc(pFDesc->pParamDesc[indexParam].ctCustData *
                    sizeof(CUSTDATAITEM));
        if(pCustData->prgCustData ){
            pCustData->cCustData=pFDesc->pParamDesc[indexParam].ctCustData;
            for(i=0, pCData=pFDesc->pParamDesc[indexParam].pCustData;
                    pCData; i++, pCData = pCData->next){
                pCustData->prgCustData[i].guid=pCData->guid;
                VariantCopy(& pCustData->prgCustData[i].varValue,
                        & pCData->data);
            }
        }else{
            ERR(" OUT OF MEMORY! \n");
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
    return TYPE_E_ELEMENTNOTFOUND;
}

/* ITypeInfo2::GetAllVarCustData
 *
 * Gets all custom data items for the specified Variable
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetAllVarCustData( ITypeInfo2 * iface,
    UINT index, CUSTDATA *pCustData)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData;
    TLBVarDesc * pVDesc;
    int i;
    TRACE("(%p) index %d\n", This, index);
    for(i=0, pVDesc=This->varlist; i!=index && pVDesc; i++,
            pVDesc=pVDesc->next)
        ;
    if(pVDesc){
        pCustData->prgCustData =
            TLB_Alloc(pVDesc->ctCustData * sizeof(CUSTDATAITEM));
        if(pCustData->prgCustData ){
            pCustData->cCustData=pVDesc->ctCustData;
            for(i=0, pCData=pVDesc->pCustData; pCData; i++,
                    pCData = pCData->next){
                pCustData->prgCustData[i].guid=pCData->guid;
                VariantCopy(& pCustData->prgCustData[i].varValue,
                        & pCData->data);
            }
        }else{
            ERR(" OUT OF MEMORY! \n");
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
    return TYPE_E_ELEMENTNOTFOUND;
}

/* ITypeInfo2::GetAllImplCustData
 *
 * Gets all custom data items for the specified implementation type
 *
 */
static HRESULT WINAPI ITypeInfo2_fnGetAllImplTypeCustData(
	ITypeInfo2 * iface,
	UINT index,
	CUSTDATA *pCustData)
{
    ICOM_THIS( ITypeInfoImpl, iface);
    TLBCustData *pCData;
    TLBImplType * pRDesc;
    int i;
    TRACE("(%p) index %d\n", This, index);
    for(i=0, pRDesc=This->impltypelist; i!=index && pRDesc; i++,
            pRDesc=pRDesc->next)
        ;
    if(pRDesc){
        pCustData->prgCustData =
            TLB_Alloc(pRDesc->ctCustData * sizeof(CUSTDATAITEM));
        if(pCustData->prgCustData ){
            pCustData->cCustData=pRDesc->ctCustData;
            for(i=0, pCData=pRDesc->pCustData; pCData; i++,
                    pCData = pCData->next){
                pCustData->prgCustData[i].guid=pCData->guid;
                VariantCopy(& pCustData->prgCustData[i].varValue,
                        & pCData->data);
            }
        }else{
            ERR(" OUT OF MEMORY! \n");
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
    return TYPE_E_ELEMENTNOTFOUND;
}

static ICOM_VTABLE(ITypeInfo2) tinfvt =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE

    ITypeInfo_fnQueryInterface,
    ITypeInfo_fnAddRef,
    ITypeInfo_fnRelease,

    ITypeInfo_fnGetTypeAttr,
    ITypeInfo_fnGetTypeComp,
    ITypeInfo_fnGetFuncDesc,
    ITypeInfo_fnGetVarDesc,
    ITypeInfo_fnGetNames,
    ITypeInfo_fnGetRefTypeOfImplType,
    ITypeInfo_fnGetImplTypeFlags,
    ITypeInfo_fnGetIDsOfNames,
    ITypeInfo_fnInvoke,
    ITypeInfo_fnGetDocumentation,
    ITypeInfo_fnGetDllEntry,
    ITypeInfo_fnGetRefTypeInfo,
    ITypeInfo_fnAddressOfMember,
    ITypeInfo_fnCreateInstance,
    ITypeInfo_fnGetMops,
    ITypeInfo_fnGetContainingTypeLib,
    ITypeInfo_fnReleaseTypeAttr,
    ITypeInfo_fnReleaseFuncDesc,
    ITypeInfo_fnReleaseVarDesc,

    ITypeInfo2_fnGetTypeKind,
    ITypeInfo2_fnGetTypeFlags,
    ITypeInfo2_fnGetFuncIndexOfMemId,
    ITypeInfo2_fnGetVarIndexOfMemId,
    ITypeInfo2_fnGetCustData,
    ITypeInfo2_fnGetFuncCustData,
    ITypeInfo2_fnGetParamCustData,
    ITypeInfo2_fnGetVarCustData,
    ITypeInfo2_fnGetImplTypeCustData,
    ITypeInfo2_fnGetDocumentation2,
    ITypeInfo2_fnGetAllCustData,
    ITypeInfo2_fnGetAllFuncCustData,
    ITypeInfo2_fnGetAllParamCustData,
    ITypeInfo2_fnGetAllVarCustData,
    ITypeInfo2_fnGetAllImplTypeCustData,
};
