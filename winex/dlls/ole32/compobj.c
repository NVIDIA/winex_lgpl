/*
 *	COMPOBJ library
 *
 *	Copyright 1995	Martin von Loewis
 *	Copyright 1998	Justin Bradford
 *      Copyright 1999  Francis Beaudet
 *  Copyright 1999  Sylvain St-Germain
 *  Copyright 2002  Marcus Meissner
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "winbase.h"
#include "winuser.h"
#include "objbase.h"
#include "ole2.h"
#include "ole2ver.h"
#include "rpc.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"
#include "wownt32.h"
#include "wine/unicode.h"
#include "objbase.h"
#include "wine/winbase16.h"
#include "ole32_main.h"
#include "compobj_private.h"
#include "ifs.h"
#include "dcom.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/****************************************************************************
 * This section defines variables internal to the COM module.
 *
 * TODO: Most of these things will have to be made thread-safe.
 */
HINSTANCE       COMPOBJ_hInstance32 = 0;

static void COM_RevokeAllClasses();
static void COM_ExternalLockFreeList();

APARTMENT MTA, *apts;
static CRITICAL_SECTION csApartment;

void create_apartment_cs(void)
{
  CRITICAL_SECTION_DEFINE( &csApartment );
}

void destroy_apartment_cs(void)
{
  DeleteCriticalSection( &csApartment );
}


/*
 * This lock count counts the number of times CoInitialize is called. It is
 * decreased every time CoUninitialize is called. When it hits 0, the COM
 * libraries are freed
 */
static LONG s_COMLockCount = 0;

/*
 * This linked list contains the list of registered class objects. These
 * are mostly used to register the factories for out-of-proc servers of OLE
 * objects.
 *
 * TODO: Make this data structure aware of inter-process communication. This
 *       means that parts of this will be exported to the Wine Server.
 */
typedef struct tagRegisteredClass
{
  CLSID     classIdentifier;
  DWORD     runContext;
  DWORD     connectFlags;
  DWORD     dwCookie;
  LPSTREAM  pMarshal;
  struct tagRegisteredClass* nextClass;
} RegisteredClass;

static CRITICAL_SECTION csRegisteredClassList;
static RegisteredClass* firstRegisteredClass = NULL;

/*****************************************************************************
 * This section contains OpenDllList definitions
 *
 * The OpenDllList contains only handles of dll loaded by CoGetClassObject or
 * other functions what do LoadLibrary _without_ giving back a HMODULE.
 * Without this list these handles would be freed never.
 *
 * FIXME: a DLL what says OK whenn asked for unloading is unloaded in the
 * next unload-call but not before 600 sec.
 */

typedef struct tagOpenDll {
  HINSTANCE hLibrary;
  struct tagOpenDll *next;
} OpenDll;

static CRITICAL_SECTION csOpenDllList;
static OpenDll *openDllList = NULL; /* linked list of open dlls */

static const char aptWinClass[] = "WINE_OLE32_APT_CLASS";

static void COMPOBJ_DLLList_Add(HANDLE hLibrary);
static void COMPOBJ_DllList_FreeUnused(int Timeout);

void create_proxy_cs(void);
void destroy_proxy_cs(void);
void create_malloc_cs(void);
void destroy_malloc_cs(void);
void create_apartment_cs(void);
void destroy_apartment_cs(void);

/*****************************************************************************
 * This section contains prototypes to internal methods for this
 * module
 */
static void COM_RevokeAllClasses();


/******************************************************************************
 * Initialize/Unitialize threading stuff.
 */
void COMPOBJ_InitProcess( void )
{
    WNDCLASSA wclass;
    
    InitializeCriticalSection( &csOpenDllList );
    CRITICAL_SECTION_NAME( &csOpenDllList, "csOpenDllList" );

    InitializeCriticalSection( &csRegisteredClassList );
    CRITICAL_SECTION_NAME( &csRegisteredClassList, "csRegisteredClassList" );

    InitializeCriticalSection( &csApartment );
    CRITICAL_SECTION_NAME( &csApartment, "csApartment" );
    
    create_proxy_cs();
    create_malloc_cs();
    create_apartment_cs();
    
    memset(&wclass, 0, sizeof(wclass));
    wclass.lpfnWndProc = &COM_AptWndProc;
    wclass.hInstance = OLE32_hInstance;
    wclass.lpszClassName = aptWinClass;
    RegisterClassA(&wclass);
}

void COMPOBJ_UninitProcess( void )
{
    UnregisterClassA(aptWinClass, OLE32_hInstance);
    
    destroy_apartment_cs();
    destroy_malloc_cs();
    destroy_proxy_cs();

    DeleteCriticalSection( &csOpenDllList );
    DeleteCriticalSection( &csRegisteredClassList );
    DeleteCriticalSection( &csApartment );
}

/******************************************************************************
 * Manage apartments.
 */
static void COM_InitMTA(void)
{
    /* FIXME: how does windoze create OXIDs?
     * this method will only work for local RPC */
    MTA.oxid = ((OXID)GetCurrentProcessId() << 32);
    CRITICAL_SECTION_DEFINE(&MTA.cs);
}

static void COM_UninitMTA(void)
{
    DeleteCriticalSection(&MTA.cs);
    MTA.oxid = 0;
}

static APARTMENT* COM_CreateApartment(DWORD model)
{
    APARTMENT *apt;

    apt = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(APARTMENT));
    apt->model = model;
    apt->tid = GetCurrentThreadId();
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                    GetCurrentProcess(), &apt->thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    if (model & COINIT_APARTMENTTHREADED) {
      /* FIXME: how does windoze create OXIDs? */
      apt->oxid = MTA.oxid | GetCurrentThreadId();
      apt->win = CreateWindowA(aptWinClass, NULL, 0,
			       0, 0, 0, 0,
			       0, 0, OLE32_hInstance, NULL);
      CRITICAL_SECTION_DEFINE(&apt->cs);
    }
    else {
      apt->parent = &MTA;
      apt->oxid = MTA.oxid;
    }
    EnterCriticalSection(&csApartment);
    if (apts) apts->prev = apt;
    apt->next = apts;
    apts = apt;
    LeaveCriticalSection(&csApartment);
    NtCurrentTeb()->ErrorInfo = apt;
    return apt;
}

static void COM_DestroyApartment(APARTMENT *apt)
{
    EnterCriticalSection(&csApartment);
    if (apt->prev) apt->prev->next = apt->next;
    if (apt->next) apt->next->prev = apt->prev;
    if (apts == apt) apts = apt->next;
    apt->prev = NULL; apt->next = NULL;
    LeaveCriticalSection(&csApartment);
    if (apt->model & COINIT_APARTMENTTHREADED) {
      if (apt->win) DestroyWindow(apt->win);
      DeleteCriticalSection(&apt->cs);
    }
    CloseHandle(apt->thread);
    HeapFree(GetProcessHeap(), 0, apt);
}

HWND COM_GetApartmentWin(OXID oxid)
{
    APARTMENT *apt;
    HWND win = 0;

    EnterCriticalSection(&csApartment);
    apt = apts;
    while (apt && apt->oxid != oxid) apt = apt->next;
    if (apt) win = apt->win;
    LeaveCriticalSection(&csApartment);
    return win;
}

/*****************************************************************************
 * This section contains OpenDllList implemantation
 */

static void COMPOBJ_DLLList_Add(HANDLE hLibrary)
{
    OpenDll *ptr;
    OpenDll *tmp;

    TRACE("\n");

    EnterCriticalSection( &csOpenDllList );

    if (openDllList == NULL) {
        /* empty list -- add first node */
        openDllList = (OpenDll*)HeapAlloc(GetProcessHeap(),0, sizeof(OpenDll));
	openDllList->hLibrary=hLibrary;
	openDllList->next = NULL;
    } else {
        /* search for this dll */
        int found = FALSE;
        for (ptr = openDllList; ptr->next != NULL; ptr=ptr->next) {
  	    if (ptr->hLibrary == hLibrary) {
	        found = TRUE;
		break;
	    }
        }
	if (!found) {
	    /* dll not found, add it */
 	    tmp = openDllList;
	    openDllList = (OpenDll*)HeapAlloc(GetProcessHeap(),0, sizeof(OpenDll));
	    openDllList->hLibrary = hLibrary;
	    openDllList->next = tmp;
	}
    }

    LeaveCriticalSection( &csOpenDllList );
}

static void COMPOBJ_DllList_FreeUnused(int Timeout)
{
    OpenDll *curr, *next, *prev = NULL;
    typedef HRESULT(*DllCanUnloadNowFunc)(void);
    DllCanUnloadNowFunc DllCanUnloadNow;

    TRACE("\n");

    EnterCriticalSection( &csOpenDllList );

    for (curr = openDllList; curr != NULL; ) {
	DllCanUnloadNow = (DllCanUnloadNowFunc) GetProcAddress(curr->hLibrary, "DllCanUnloadNow");

	if ( (DllCanUnloadNow != NULL) && (DllCanUnloadNow() == S_OK) ) {
	    next = curr->next;

	    TRACE("freeing 0x%08x\n", curr->hLibrary);
	    FreeLibrary(curr->hLibrary);

	    HeapFree(GetProcessHeap(), 0, curr);
	    if (curr == openDllList) {
		openDllList = next;
	    } else {
	      prev->next = next;
	    }

	    curr = next;
	} else {
	    prev = curr;
	    curr = curr->next;
	}
    }

    LeaveCriticalSection( &csOpenDllList );
}

/******************************************************************************
 *           CoBuildVersion [COMPOBJ.1]
 *           CoBuildVersion [OLE32.4]
 *
 * RETURNS
 *	Current build version, hiword is majornumber, loword is minornumber
 */
DWORD WINAPI CoBuildVersion(void)
{
    TRACE("Returning version %d, build %d.\n", rmm, rup);
    return (rmm<<16)+rup;
}

/******************************************************************************
 *		CoInitialize	[OLE32.26]
 *
 * Initializes the COM libraries.
 *
 * See CoInitializeEx
 */
HRESULT WINAPI CoInitialize(
	LPVOID lpReserved	/* [in] pointer to win32 malloc interface
                                   (obsolete, should be NULL) */
)
{
  /*
   * Just delegate to the newer method.
   */
  return CoInitializeEx(lpReserved, COINIT_APARTMENTTHREADED);
}

/******************************************************************************
 *		CoInitializeEx	[OLE32.163]
 *
 * Initializes the COM libraries. The behavior used to set the win32 IMalloc
 * used for memory management is obsolete.
 *
 * RETURNS
 *  S_OK               if successful,
 *  S_FALSE            if this function was called already.
 *  RPC_E_CHANGED_MODE if a previous call to CoInitialize specified another
 *                      threading model.
 */
HRESULT WINAPI CoInitializeEx(
	LPVOID lpReserved,	/* [in] pointer to win32 malloc interface
                                   (obsolete, should be NULL) */
	DWORD dwCoInit		/* [in] A value from COINIT specifies the threading model */
)
{
  HRESULT hr = S_OK;
  APARTMENT *apt;

  TRACE("(%p, %x)\n", lpReserved, (int)dwCoInit);

  if (lpReserved!=NULL)
  {
    ERR("(%p, %x) - Bad parameter passed-in %p, must be an old Windows Application\n", lpReserved, (int)dwCoInit, lpReserved);
  }

  apt = NtCurrentTeb()->ErrorInfo;
  if (apt && dwCoInit != apt->model) return RPC_E_CHANGED_MODE;
  hr = apt ? S_FALSE : S_OK;

  /*
   * Check the lock count. If this is the first time going through the initialize
   * process, we have to initialize the libraries.
   *
   * And crank-up that lock count.
   */
  if (InterlockedExchangeAdd(&s_COMLockCount,1)==0)
  {
    /*
     * Initialize the various COM libraries and data structures.
     */
    TRACE("() - Initializing the COM libraries\n");

    COM_InitMTA();

    RunningObjectTableImpl_Initialize();
  }

  if (!apt) apt = COM_CreateApartment(dwCoInit);

  InterlockedIncrement(&apt->inits);
  if (hr == S_OK) NtCurrentTeb()->ErrorInfo = apt;

  return hr;
}

/***********************************************************************
 *           CoUninitialize   [OLE32.47]
 *
 * This method will release the COM libraries.
 *
 * See the windows documentation for more details.
 */
void WINAPI CoUninitialize(void)
{
  LONG lCOMRefCnt;
  APARTMENT *apt;

  TRACE("()\n");

  apt = NtCurrentTeb()->ErrorInfo;
  if (!apt) return;
  if (InterlockedDecrement(&apt->inits)==0) {
    NtCurrentTeb()->ErrorInfo = NULL;
    COM_DestroyApartment(apt);
    apt = NULL;
  }

  /*
   * Decrease the reference count.
   * If we are back to 0 locks on the COM library, make sure we free
   * all the associated data structures.
   */
  lCOMRefCnt = InterlockedExchangeAdd(&s_COMLockCount,-1);
  if (lCOMRefCnt==1)
  {
    /*
     * Release the various COM libraries and data structures.
     */
    TRACE("() - Releasing the COM libraries\n");

    RunningObjectTableImpl_UnInitialize();
    /*
     * Release the references to the registered class objects.
     */
    COM_RevokeAllClasses();

    /*
     * This will free the loaded COM Dlls.
     */
    CoFreeAllLibraries();

    /*
     * This will free list of external references to COM objects.
     */
    COM_ExternalLockFreeList();

    COM_UninitMTA();
  }
  else if (lCOMRefCnt<1) {
    ERR( "CoUninitialize() - not CoInitialized.\n" );
    InterlockedExchangeAdd(&s_COMLockCount,1); /* restore the lock count. */
  }
}

/******************************************************************************
 *		CoDisconnectObject	[COMPOBJ.15]
 *		CoDisconnectObject	[OLE32.8]
 */
HRESULT WINAPI CoDisconnectObject( LPUNKNOWN lpUnk, DWORD reserved )
{
    TRACE("(%p, %lx)\n",lpUnk,reserved);
    return S_OK;
}

/******************************************************************************
 *		CoCreateGuid[OLE32.6]
 *
 */
HRESULT WINAPI CoCreateGuid(
	GUID *pguid /* [out] points to the GUID to initialize */
) {
    return UuidCreate(pguid);
}

/******************************************************************************
 *		CLSIDFromString	[OLE32.3]
 *		IIDFromString   [OLE32.74]
 * Converts a unique identifier from its string representation into
 * the GUID struct.
 *
 * UNDOCUMENTED
 *      If idstr is not a valid CLSID string then it gets treated as a ProgID
 *
 * RETURNS
 *	the converted GUID
 */
HRESULT WINAPI __CLSIDFromStringA(
	LPCSTR idstr,	        /* [in] string representation of guid */
	CLSID *id)		/* [out] GUID converted from string */
{
  BYTE *s = (BYTE *) idstr;
  BYTE *p;
  int	i;
  BYTE table[256];

  if (!s)
	  s = "{00000000-0000-0000-0000-000000000000}";
  else {  /* validate the CLSID string */

      if (strlen(s) != 38)
          return CO_E_CLASSSTRING;

      if ((s[0]!='{') || (s[9]!='-') || (s[14]!='-') || (s[19]!='-') || (s[24]!='-') || (s[37]!='}'))
          return CO_E_CLASSSTRING;

      for (i=1; i<37; i++) {
          if ((i == 9)||(i == 14)||(i == 19)||(i == 24)) continue;
          if (!(((s[i] >= '0') && (s[i] <= '9'))  ||
                ((s[i] >= 'a') && (s[i] <= 'f'))  ||
                ((s[i] >= 'A') && (s[i] <= 'F'))))
              return CO_E_CLASSSTRING;
      }
  }

  TRACE("%s -> %p\n", s, id);

  /* quick lookup table */
  memset(table, 0, 256);

  for (i = 0; i < 10; i++) {
    table['0' + i] = i;
  }
  for (i = 0; i < 6; i++) {
    table['A' + i] = i+10;
    table['a' + i] = i+10;
  }

  /* in form {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} */

  p = (BYTE *) id;

  s++;	/* skip leading brace  */
  for (i = 0; i < 4; i++) {
    p[3 - i] = table[*s]<<4 | table[*(s+1)];
    s += 2;
  }
  p += 4;
  s++;	/* skip - */

  for (i = 0; i < 2; i++) {
    p[1-i] = table[*s]<<4 | table[*(s+1)];
    s += 2;
  }
  p += 2;
  s++;	/* skip - */

  for (i = 0; i < 2; i++) {
    p[1-i] = table[*s]<<4 | table[*(s+1)];
    s += 2;
  }
  p += 2;
  s++;	/* skip - */

  /* these are just sequential bytes */
  for (i = 0; i < 2; i++) {
    *p++ = table[*s]<<4 | table[*(s+1)];
    s += 2;
  }
  s++;	/* skip - */

  for (i = 0; i < 6; i++) {
    *p++ = table[*s]<<4 | table[*(s+1)];
    s += 2;
  }

  return S_OK;
}

HRESULT WINAPI CLSIDFromString(
	LPCOLESTR idstr,	/* [in] string representation of GUID */
	CLSID *id )		/* [out] GUID represented by above string */
{
    char xid[40];
    HRESULT ret;

    if (!WideCharToMultiByte( CP_ACP, 0, idstr, -1, xid, sizeof(xid), NULL, NULL ))
        return CO_E_CLASSSTRING;


    ret = __CLSIDFromStringA(xid,id);
    if(ret != S_OK) { /* It appears a ProgID is also valid */
        ret = CLSIDFromProgID(idstr, id);
    }
    return ret;
}

/******************************************************************************
 *		WINE_StringFromCLSID	[Internal]
 * Converts a GUID into the respective string representation.
 *
 * NOTES
 *
 * RETURNS
 *	the string representation and HRESULT
 */
HRESULT WINE_StringFromCLSID(
	const CLSID *id,	/* [in] GUID to be converted */
	LPSTR idstr		/* [out] pointer to buffer to contain converted guid */
) {
  static const char *hex = "0123456789ABCDEF";
  char *s;
  int	i;

  if (!id)
	{ ERR("called with id=Null\n");
	  *idstr = 0x00;
	  return E_FAIL;
	}

  sprintf(idstr, "{%08lX-%04X-%04X-%02X%02X-",
	  id->Data1, id->Data2, id->Data3,
	  id->Data4[0], id->Data4[1]);
  s = &idstr[25];

  /* 6 hex bytes */
  for (i = 2; i < 8; i++) {
    *s++ = hex[id->Data4[i]>>4];
    *s++ = hex[id->Data4[i] & 0xf];
  }

  *s++ = '}';
  *s++ = '\0';

  TRACE("%p->%s\n", id, idstr);

  return S_OK;
}


/******************************************************************************
 *		StringFromCLSID	[OLE32.151]
 *		StringFromIID   [OLE32.153]
 * Converts a GUID into the respective string representation.
 * The target string is allocated using the OLE IMalloc.
 * RETURNS
 *	the string representation and HRESULT
 */
HRESULT WINAPI StringFromCLSID(
        REFCLSID id,            /* [in] the GUID to be converted */
	LPOLESTR *idstr	/* [out] a pointer to a to-be-allocated pointer pointing to the resulting string */
) {
	char            buf[80];
	HRESULT       ret;
	LPMALLOC	mllc;

	if ((ret=CoGetMalloc(0,&mllc)))
		return ret;

	ret=WINE_StringFromCLSID(id,buf);
	if (!ret) {
            DWORD len = MultiByteToWideChar( CP_ACP, 0, buf, -1, NULL, 0 );
            *idstr = IMalloc_Alloc( mllc, len * sizeof(WCHAR) );
            MultiByteToWideChar( CP_ACP, 0, buf, -1, *idstr, len );
	}
	return ret;
}

/******************************************************************************
 *		StringFromGUID2	[COMPOBJ.76]
 *		StringFromGUID2	[OLE32.152]
 *
 * Converts a global unique identifier into a string of an API-
 * specified fixed format. (The usual {.....} stuff.)
 *
 * RETURNS
 *	The (UNICODE) string representation of the GUID in 'str'
 *	The length of the resulting string, 0 if there was any problem.
 */
INT WINAPI
StringFromGUID2(REFGUID id, LPOLESTR str, INT cmax)
{
  char		xguid[80];

  if (WINE_StringFromCLSID(id,xguid))
  	return 0;
  return MultiByteToWideChar( CP_ACP, 0, xguid, -1, str, cmax );
}

/******************************************************************************
 * ProgIDFromCLSID [OLE32.133]
 * Converts a class id into the respective Program ID. (By using a registry lookup)
 * RETURNS S_OK on success
 * riid associated with the progid
 */

HRESULT WINAPI ProgIDFromCLSID(
  REFCLSID clsid, /* [in] class id as found in registry */
  LPOLESTR *lplpszProgID/* [out] associated Prog ID */
)
{
  char     strCLSID[50], *buf, *buf2;
  DWORD    buf2len;
  HKEY     xhkey;
  LPMALLOC mllc;
  HRESULT  ret = S_OK;

  WINE_StringFromCLSID(clsid, strCLSID);

  buf = HeapAlloc(GetProcessHeap(), 0, strlen(strCLSID)+14);
  sprintf(buf,"CLSID\\%s\\ProgID", strCLSID);
  if (RegOpenKeyA(HKEY_CLASSES_ROOT, buf, &xhkey))
    ret = REGDB_E_CLASSNOTREG;

  HeapFree(GetProcessHeap(), 0, buf);

  if (ret == S_OK)
  {
    buf2 = HeapAlloc(GetProcessHeap(), 0, 255);
    buf2len = 255;
    if (RegQueryValueA(xhkey, NULL, buf2, &buf2len))
      ret = REGDB_E_CLASSNOTREG;

    if (ret == S_OK)
    {
      if (CoGetMalloc(0,&mllc))
        ret = E_OUTOFMEMORY;
      else
      {
          DWORD len = MultiByteToWideChar( CP_ACP, 0, buf2, -1, NULL, 0 );
          *lplpszProgID = IMalloc_Alloc(mllc, len * sizeof(WCHAR) );
          MultiByteToWideChar( CP_ACP, 0, buf2, -1, *lplpszProgID, len );
      }
    }
    HeapFree(GetProcessHeap(), 0, buf2);
  }

  RegCloseKey(xhkey);
  return ret;
}

/******************************************************************************
 *		CLSIDFromProgID	[OLE32.2]
 * Converts a program id into the respective GUID. (By using a registry lookup)
 * RETURNS
 *	riid associated with the progid
 */
HRESULT WINAPI CLSIDFromProgID(
	LPCOLESTR progid,	/* [in] program id as found in registry */
	LPCLSID riid )		/* [out] associated CLSID */
{
    static const WCHAR clsidW[] = { '\\','C','L','S','I','D',0 };
    char buf2[80];
    DWORD buf2len = sizeof(buf2);
    HKEY xhkey;

    WCHAR *buf = HeapAlloc( GetProcessHeap(),0,(strlenW(progid)+8) * sizeof(WCHAR) );
    strcpyW( buf, progid );
    strcatW( buf, clsidW );
    if (RegOpenKeyW(HKEY_CLASSES_ROOT,buf,&xhkey))
    {
        HeapFree(GetProcessHeap(),0,buf);
        return CO_E_CLASSSTRING;
    }
    HeapFree(GetProcessHeap(),0,buf);

    if (RegQueryValueA(xhkey,NULL,buf2,&buf2len))
    {
        RegCloseKey(xhkey);
        return CO_E_CLASSSTRING;
    }
    RegCloseKey(xhkey);
    return __CLSIDFromStringA(buf2,riid);
}



/*****************************************************************************
 *             CoGetPSClsid [OLE32.22]
 *
 * This function returns the CLSID of the DLL that implements the proxy and stub
 * for the specified interface.
 *
 * It determines this by searching the
 * HKEY_CLASSES_ROOT\Interface\{string form of riid}\ProxyStubClsid32 in the registry
 * and any interface id registered by CoRegisterPSClsid within the current process.
 *
 * FIXME: We only search the registry, not ids registered with CoRegisterPSClsid.
 */
HRESULT WINAPI CoGetPSClsid(
          REFIID riid,     /* [in]  Interface whose proxy/stub CLSID is to be returned */
          CLSID *pclsid )    /* [out] Where to store returned proxy/stub CLSID */
{
    char *buf, buf2[40];
    DWORD buf2len;
    HKEY xhkey;

    TRACE("() riid=%s, pclsid=%p\n", debugstr_guid(riid), pclsid);

    /* Get the input iid as a string */
    WINE_StringFromCLSID(riid, buf2);
    /* Allocate memory for the registry key we will construct.
       (length of iid string plus constant length of static text */
    buf = HeapAlloc(GetProcessHeap(), 0, strlen(buf2)+27);
    if (buf == NULL)
    {
       return (E_OUTOFMEMORY);
    }

    /* Construct the registry key we want */
    sprintf(buf,"Interface\\%s\\ProxyStubClsid32", buf2);

    /* Open the key.. */
    if (RegOpenKeyA(HKEY_CLASSES_ROOT, buf, &xhkey))
    {
       HeapFree(GetProcessHeap(),0,buf);
       return (E_INVALIDARG);
    }
    HeapFree(GetProcessHeap(),0,buf);

    /* ... Once we have the key, query the registry to get the
       value of CLSID as a string, and convert it into a
       proper CLSID structure to be passed back to the app */
    buf2len = sizeof(buf2);
    if ( (RegQueryValueA(xhkey,NULL,buf2,&buf2len)) )
    {
       RegCloseKey(xhkey);
       return E_INVALIDARG;
    }
    RegCloseKey(xhkey);

    /* We have the CLSid we want back from the registry as a string, so
       lets convert it into a CLSID structure */
    if ( (__CLSIDFromStringA(buf2,pclsid)) != NOERROR) {
       return E_INVALIDARG;
    }

    TRACE ("() Returning CLSID=%s\n", debugstr_guid(pclsid));
    return (S_OK);
}



/***********************************************************************
 *		WriteClassStm (OLE32.159)
 *
 * This function write a CLSID on stream
 */
HRESULT WINAPI WriteClassStm(IStream *pStm,REFCLSID rclsid)
{
    TRACE("(%p,%p)\n",pStm,rclsid);

    if (rclsid==NULL)
        return E_INVALIDARG;

    return IStream_Write(pStm,rclsid,sizeof(CLSID),NULL);
}

/***********************************************************************
 *		ReadClassStm (OLE32.135)
 *
 * This function read a CLSID from a stream
 */
HRESULT WINAPI ReadClassStm(IStream *pStm,CLSID *pclsid)
{
    ULONG nbByte;
    HRESULT res;

    TRACE("(%p,%p)\n",pStm,pclsid);

    if (pclsid==NULL)
        return E_INVALIDARG;

    res = IStream_Read(pStm,(void*)pclsid,sizeof(CLSID),&nbByte);

    if (FAILED(res))
        return res;

    if (nbByte != sizeof(CLSID))
        return S_FALSE;
    else
        return S_OK;
}


/***
 * COM_GetRegisteredClassObject
 *
 * This internal method is used to scan the registered class list to
 * find a class object.
 *
 * Params:
 *   rclsid        Class ID of the class to find.
 *   ppUnk         [out] returns a pointer to the class object. Complying
 *                 to normal COM usage, this method will increase the
 *                 reference count on this object.
 *   pObjRef       [out] DCOM standard object reference
 *   pMIF          [out] DCOM marshalled interface pointer
 */
HRESULT COM_GetRegisteredClassObject(
	REFCLSID rclsid,
	LPUNKNOWN* ppUnk,
	STDOBJREF* pObjRef,
	MInterfacePointer** ppMIF)
{
  HRESULT hr = S_FALSE;
  RegisteredClass* curClass;

  EnterCriticalSection( &csRegisteredClassList );

  /*
   * Iterate through the whole list and try to match the class ID.
   */
  curClass = firstRegisteredClass;

  while (curClass != 0)
  {
    /*
     * Check if we have a match on the class ID.
     */
    if (IsEqualGUID(&(curClass->classIdentifier), rclsid))
    {
      /*
       * We have a match, unmarshal the class object reference.
       */
      if (ppUnk || pObjRef || ppMIF)
      {
        LARGE_INTEGER pos;

        pos.s.LowPart = 0;
        pos.s.HighPart = 0;
        IStream_Seek(curClass->pMarshal, pos, 0, NULL);
      }
      if (ppUnk) {
        hr = CoUnmarshalInterface(curClass->pMarshal, &IID_IUnknown, (void**)ppUnk);
      }
      else if (pObjRef) {
        hr = COM_GetStdObjRef(curClass->pMarshal, pObjRef, NULL);
      }
      else if (ppMIF) {
        FIXME("create MInterfacePointer\n");
        hr = E_FAIL;
      }
      else hr = S_OK;

      goto end;
    }

    /*
     * Step to the next class in the list.
     */
    curClass = curClass->nextClass;
  }

end:
  LeaveCriticalSection( &csRegisteredClassList );
  /*
   * If we get to here, we haven't found our class.
   */
  return hr;
}

/******************************************************************************
 *		CoRegisterClassObject	[OLE32.36]
 *
 * This method will register the class object for a given class ID.
 *
 * See the Windows documentation for more details.
 */
HRESULT WINAPI CoRegisterClassObject(
	REFCLSID rclsid,
	LPUNKNOWN pUnk,
	DWORD dwClsContext, /* [in] CLSCTX flags indicating the context in which to run the executable */
	DWORD flags,        /* [in] REGCLS flags indicating how connections are made */
	LPDWORD lpdwRegister
)
{
  RegisteredClass* newClass;
  DWORD            dwCtx;
  HRESULT          hr;
  char buf[80];

  WINE_StringFromCLSID(rclsid,buf);

  TRACE("(%s,%p,0x%08lx,0x%08lx,%p)\n",
	buf,pUnk,dwClsContext,flags,lpdwRegister);
  TRACE("vtbl=%p\n", pUnk->lpVtbl);

  if ( (lpdwRegister==0) || (pUnk==0) )
    return E_INVALIDARG;

  *lpdwRegister = 0;

  /*
   * First, check if the class is already registered.
   * If it is, this should cause an error.
   */
  hr = COM_GetRegisteredClassObject(rclsid, NULL, NULL, NULL);

  if (hr == S_OK)
    return CO_E_OBJISREG;

  /*
   * If it is not registered, we must create a new entry for this class and
   * append it to the registered class list.
   * We use the address of the chain node as the cookie since we are sure it's
   * unique.
   */
  newClass = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RegisteredClass));
  if ( newClass == NULL )
    return E_OUTOFMEMORY;

  /*
   * Initialize the node.
   */
  newClass->classIdentifier = *rclsid;
  newClass->runContext      = dwClsContext;
  newClass->connectFlags    = flags;
  /*
   * Use the address of the chain node as the cookie since we are sure it's
   * unique.
   */
  newClass->dwCookie        = (DWORD)newClass;

  /*
   * Marshal a reference to the class object.
   */
  hr = CreateStreamOnHGlobal(0, TRUE, &newClass->pMarshal);
  if (FAILED(hr)) {
    HeapFree(GetProcessHeap(), 0, newClass);
    return hr;
  }

  if (dwClsContext & CLSCTX_REMOTE_SERVER) dwCtx = MSHCTX_DIFFERENTMACHINE;
  else if (dwClsContext & CLSCTX_LOCAL_SERVER) dwCtx = MSHCTX_LOCAL;
  else dwCtx = MSHCTX_INPROC;

  hr = CoMarshalInterface(newClass->pMarshal, &IID_IUnknown, pUnk, dwCtx, NULL, MSHLFLAGS_TABLESTRONG);
  if (FAILED(hr)) {
    IStream_Release(newClass->pMarshal);
    HeapFree(GetProcessHeap(), 0, newClass);
    return hr;
  }

  /*
   * Insert the class into the chain.
   */
  EnterCriticalSection( &csRegisteredClassList );

  newClass->nextClass       = firstRegisteredClass;
  firstRegisteredClass = newClass;
  LeaveCriticalSection( &csRegisteredClassList );

  /*
   * Register the exported object.
   */
  COM_RpcExportClass(&newClass->classIdentifier, dwCtx);

  /*
   * Assign the out parameter (cookie)
   */
  *lpdwRegister = newClass->dwCookie;

  return S_OK;
}

/******************************************************************************
 *		CoRegisterPSClsid	[OLE32.171]
 *
 */
HRESULT WINAPI CoRegisterPSClsid(REFIID riid, REFCLSID rclsid)
{
  TRACE("(%s,%s)\n", debugstr_guid(riid), debugstr_guid(rclsid));

  FIXME("stub\n");
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

  return S_OK;
}

/***********************************************************************
 *           CoRevokeClassObject [OLE32.40]
 *
 * This method will remove a class object from the class registry
 *
 * See the Windows documentation for more details.
 */
HRESULT WINAPI CoRevokeClassObject(
        DWORD dwRegister)
{
  HRESULT hr = E_INVALIDARG;
  DWORD             dwCtx;
  RegisteredClass** prevClassLink;
  RegisteredClass*  curClass;

  TRACE("(%08lx)\n",dwRegister);

  EnterCriticalSection( &csRegisteredClassList );

  /*
   * Iterate through the whole list and try to match the cookie.
   */
  curClass      = firstRegisteredClass;
  prevClassLink = &firstRegisteredClass;

  while (curClass != 0)
  {
    /*
     * Check if we have a match on the cookie.
     */
    if (curClass->dwCookie == dwRegister)
    {
      LARGE_INTEGER pos;

      if (curClass->runContext & CLSCTX_REMOTE_SERVER) dwCtx = MSHCTX_DIFFERENTMACHINE;
      else if (curClass->runContext & CLSCTX_LOCAL_SERVER) dwCtx = MSHCTX_LOCAL;
      else dwCtx = MSHCTX_INPROC;

      /*
       * Unregister the exported object.
       */
      COM_RpcUnexportClass(&curClass->classIdentifier, dwCtx);

      /*
       * Remove the class from the chain.
       */
      *prevClassLink = curClass->nextClass;

      /*
       * Release the marshalled reference.
       */
      pos.s.LowPart = 0;
      pos.s.HighPart = 0;
      IStream_Seek(curClass->pMarshal, pos, 0, NULL);

      CoReleaseMarshalData(curClass->pMarshal);

      IStream_Release(curClass->pMarshal);

      /*
       * Free the memory used by the chain node.
       */
      HeapFree(GetProcessHeap(), 0, curClass);

      hr = S_OK;
      goto end;
    }

    /*
     * Step to the next class in the list.
     */
    prevClassLink = &(curClass->nextClass);
    curClass      = curClass->nextClass;
  }

end:
  LeaveCriticalSection( &csRegisteredClassList );
  /*
   * If we get to here, we haven't found our class.
   */
  return E_INVALIDARG;
}

#if 0
extern HRESULT COM_ConnectProxy(LPVOID pv, LPRPCCHANNELBUFFER pChannel);

static HRESULT LRPC_GetClassObject(HANDLE svc, REFCLSID rclsid, REFIID iid, LPVOID *ppv)
{
  HRESULT hr;
  DWORD dwSize;
  LPRPCCHANNELBUFFER chan;

  /* transmit the IID we want over the pipe */
  WriteFile(svc, iid, sizeof(*iid), NULL, NULL);
  /* read packet size */
  if (!ReadFile(svc, &dwSize, sizeof(dwSize), NULL, NULL)) goto gen_error;
  /* read packet */
  if (dwSize) {
    LPSTREAM pStm;
    ULARGE_INTEGER cbSize;
    HGLOBAL hGlobal = 0;
    BOOL ok = FALSE;

    hr = CreateStreamOnHGlobal(0, TRUE, &pStm);
    if (FAILED(hr)) goto error;
    cbSize.s.HighPart = 0;
    cbSize.s.LowPart = dwSize;
    if (SUCCEEDED(IStream_SetSize(pStm, cbSize)) &&
        SUCCEEDED(GetHGlobalFromStream(pStm, &hGlobal))) {
      ok = ReadFile(svc, GlobalLock(hGlobal), dwSize, NULL, NULL);
      GlobalUnlock(hGlobal);
    }
    if (!ok) {
      IStream_Release(pStm);
      goto gen_error;
    }
    hr = CoUnmarshalInterface(pStm, iid, ppv);
    IStream_Release(pStm);
    if (FAILED(hr)) goto error;
  }
  /* read result code */
  if (!ReadFile(svc, &hr, sizeof(hr), NULL, NULL)) goto gen_error;
  if (FAILED(hr)) goto error;

  chan = RpcChannel_Construct(svc);
  if (!chan) {
    hr = E_OUTOFMEMORY;
    goto error;
  }

  /* attach channel to proxy */
/*  hr = COM_ConnectProxy(*ppv, chan); */
  if (FAILED(hr)) {
    IRpcChannelBuffer_Release(chan);
    goto error;
  }

  /* establish reverse channel listener */
  /* chan->hThread = CreateThread(NULL, 0, LRPC_client_thread, chan, 0, NULL); */

  /* success */
  return hr;

gen_error:
  hr = E_FAIL;
error:
  CloseHandle(svc);
  return hr;
}
#endif

typedef HRESULT (CALLBACK *DllGetClassObjectFunc)(REFCLSID clsid,
			  REFIID iid, LPVOID *ppv);

/***********************************************************************
 *	compobj_RegReadPath	[internal]
 *
 *	Reads a registry value and expands it when nessesary
 */
HRESULT compobj_RegReadPath(char * keyname, char * valuename, char * dst, int dstlen)
{
	HRESULT hres;
	HKEY key;
	DWORD keytype;
	char src[MAX_PATH];
	DWORD dwLength = dstlen;

	if((hres = RegOpenKeyExA(HKEY_CLASSES_ROOT, keyname, 0, KEY_READ, &key)) == ERROR_SUCCESS) {
          if( (hres = RegQueryValueExA(key, NULL, NULL, &keytype, (LPBYTE)src, &dwLength)) == ERROR_SUCCESS ) {
            if (keytype == REG_EXPAND_SZ) {
              if (dstlen <= ExpandEnvironmentStringsA(src, dst, dstlen)) hres = ERROR_MORE_DATA;
            } else {
              strncpy(dst, src, dstlen);
            }
	  }
          RegCloseKey (key);
	}
	return hres;
}

/***********************************************************************
 *           CoGetClassObject [COMPOBJ.7]
 *           CoGetClassObject [OLE32.16]
 *
 * FIXME.  If request allows of several options and there is a failure
 *         with one (other than not being registered) do we try the
 *         others or return failure?  (E.g. inprocess is registered but
 *         the DLL is not found but the server version works)
 */
HRESULT WINAPI CoGetClassObject(
    REFCLSID rclsid, DWORD dwClsContext, COSERVERINFO *pServerInfo,
    REFIID iid, LPVOID *ppv
) {
    LPUNKNOWN	regClassObject;
    HRESULT	hres = E_UNEXPECTED;
    char	xclsid[80];
    HINSTANCE hLibrary;
    typedef HRESULT (CALLBACK *DllGetClassObjectFunc)(REFCLSID clsid, REFIID iid, LPVOID *ppv);
    DllGetClassObjectFunc DllGetClassObject;

    WINE_StringFromCLSID((LPCLSID)rclsid,xclsid);

    TRACE("\n\tCLSID:\t%s,\n\tIID:\t%s\n", debugstr_guid(rclsid), debugstr_guid(iid));

    if (pServerInfo) {
	FIXME("\tpServerInfo: name=%s\n",debugstr_w(pServerInfo->pwszName));
	FIXME("\t\tpAuthInfo=%p\n",pServerInfo->pAuthInfo);
    }

    /*
     * First, try and see if we can't match the class ID with one of the
     * registered classes.
     */
    if (S_OK == COM_GetRegisteredClassObject(rclsid, &regClassObject, NULL, NULL))
    {
      /*
       * Get the required interface from the retrieved pointer.
       */
      hres = IUnknown_QueryInterface(regClassObject, iid, ppv);

      /*
       * Since QI got another reference on the pointer, we want to release the
       * one we already have. If QI was unsuccessful, this will release the object. This
       * is good since we are not returning it in the "out" parameter.
       */
      IUnknown_Release(regClassObject);

      return hres;
    }

    /* first try: in-process */
    if ((CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER) & dwClsContext) {
	char keyname[MAX_PATH];
	char dllpath[MAX_PATH+1];

	sprintf(keyname,"CLSID\\%s\\InprocServer32",xclsid);

	if ( compobj_RegReadPath(keyname, NULL, dllpath, sizeof(dllpath)) != ERROR_SUCCESS) {
	    /* failure: CLSID is not found in registry */
	    WARN("class %s not registred\n", xclsid);
            hres = REGDB_E_CLASSNOTREG;
	} else {
	  if ((hLibrary = LoadLibraryExA(dllpath, 0, LOAD_WITH_ALTERED_SEARCH_PATH)) == 0) {
	    /* failure: DLL could not be loaded */
	    ERR("couldn't load InprocServer32 dll %s\n", dllpath);
	    hres = E_ACCESSDENIED; /* FIXME: or should this be CO_E_DLLNOTFOUND? */
	  } else if (!(DllGetClassObject = (DllGetClassObjectFunc)GetProcAddress(hLibrary, "DllGetClassObject"))) {
	    /* failure: the dll did not export DllGetClassObject */
	    ERR("couldn't find function DllGetClassObject in %s\n", dllpath);
	    FreeLibrary( hLibrary );
	    hres = CO_E_DLLNOTFOUND;
	  } else {
	    /* OK: get the ClassObject */
	    COMPOBJ_DLLList_Add( hLibrary );
	    return DllGetClassObject(rclsid, iid, ppv);
	  }
	}
    }

    if (CLSCTX_LOCAL_SERVER & dwClsContext) {
        HKEY key;
        WCHAR ProviderName[MAX_PATH+1];
        DWORD ProviderNameLen = sizeof(ProviderName);
	char buf[200];
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	int wait;

	hres = COM_RpcImportClass(ppv, rclsid, iid, NULL);
	if (hres != EPT_S_NOT_REGISTERED)
	    return hres;

	/* we need to load the server
	 * (FIXME: this is a job for IRemoteActivation,
	 * but I haven't implemented that stuff) */
	/* FIXME: protect this with a named mutex,
	 * so the local server won't be launched twice */
	sprintf(buf,"CLSID\\%s\\LocalServer32",xclsid);
	hres = RegOpenKeyExA(HKEY_CLASSES_ROOT, buf, 0, KEY_READ, &key);

	if (hres != ERROR_SUCCESS) {
	    return REGDB_E_CLASSNOTREG;
	}

	memset(ProviderName,0,sizeof(ProviderName));
	hres = RegQueryValueExW(key,NULL,NULL,NULL,(LPBYTE)ProviderName,&ProviderNameLen);
	RegCloseKey(key);
	if (hres) {
	    return REGDB_E_CLASSNOTREG; /* FIXME: check retval */
	}
	TRACE("found LocalServer32 app %s\n", debugstr_w(ProviderName));

	/* create process (FIXME: creation flags?) */
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	/* FIXME: I think we're supposed to add an argument,
	 * see MS's COM samples */
	if (!CreateProcessW(ProviderName, NULL, NULL, NULL, FALSE, 0,
			    NULL, NULL, &si, &pi)) {
	    FIXME("couldn't load LocalServer32 app %s\n", debugstr_w(ProviderName));
	    return CO_E_APPNOTFOUND;
	}

	/* wait for the local server to load
	 * (don't remember how long MS gives it, was it 2 minutes?) */
	wait = 0;
	do {
	    Sleep(1000);
	    wait++;
	    hres = COM_RpcImportClass(ppv, rclsid, iid, NULL);
	} while (hres == EPT_S_NOT_REGISTERED && wait < 120);
	if (hres == EPT_S_NOT_REGISTERED) {
	    /* wait failed */
	    return CO_E_APPDIDNTREG;
	}
	return hres;
    }

    /* Finally try remote */
    if (CLSCTX_REMOTE_SERVER & dwClsContext)
    {
        FIXME ("CLSCTX_REMOTE_SERVER not supported\n");
        hres = E_NOINTERFACE;
    }

    return hres;
}
/***********************************************************************
 *        CoResumeClassObjects (OLE32.173)
 *
 * Resumes classobjects registered with REGCLS suspended
 */
HRESULT WINAPI CoResumeClassObjects(void)
{
	FIXME("\n");
	return S_OK;
}

/***********************************************************************
 *        GetClassFile (OLE32.67)
 *
 * This function supplies the CLSID associated with the given filename.
 */
HRESULT WINAPI GetClassFile(LPCOLESTR filePathName,CLSID *pclsid)
{
    IStorage *pstg=0;
    HRESULT res;
    int nbElm=0,length=0,i=0;
    LONG sizeProgId=20;
    LPOLESTR *pathDec=0,absFile=0,progId=0;
    WCHAR extention[100]={0};

    TRACE("()\n");

    /* if the file contain a storage object the return the CLSID writen by IStorage_SetClass method*/
    if((StgIsStorageFile(filePathName))==S_OK){

        res=StgOpenStorage(filePathName,NULL,STGM_READ | STGM_SHARE_DENY_WRITE,NULL,0,&pstg);

        if (SUCCEEDED(res))
            res=ReadClassStg(pstg,pclsid);

        IStorage_Release(pstg);

        return res;
    }
    /* if the file is not a storage object then attemps to match various bits in the file against a
       pattern in the registry. this case is not frequently used ! so I present only the psodocode for
       this case

     for(i=0;i<nFileTypes;i++)

        for(i=0;j<nPatternsForType;j++){

            PATTERN pat;
            HANDLE  hFile;

            pat=ReadPatternFromRegistry(i,j);
            hFile=CreateFileW(filePathName,,,,,,hFile);
            SetFilePosition(hFile,pat.offset);
            ReadFile(hFile,buf,pat.size,NULL,NULL);
            if (memcmp(buf&pat.mask,pat.pattern.pat.size)==0){

                *pclsid=ReadCLSIDFromRegistry(i);
                return S_OK;
            }
        }
     */

    /* if the obove strategies fail then search for the extension key in the registry */

    /* get the last element (absolute file) in the path name */
    nbElm=FileMonikerImpl_DecomposePath(filePathName,&pathDec);
    absFile=pathDec[nbElm-1];

    /* failed if the path represente a directory and not an absolute file name*/
    if (lstrcmpW(absFile,(LPOLESTR)"\\"))
        return MK_E_INVALIDEXTENSION;

    /* get the extension of the file */
    length=lstrlenW(absFile);
    for(i=length-1; ( (i>=0) && (extention[i]=absFile[i]) );i--);

    /* get the progId associated to the extension */
    progId=CoTaskMemAlloc(sizeProgId);

    res=RegQueryValueW(HKEY_CLASSES_ROOT,extention,progId,&sizeProgId);

    if (res==ERROR_MORE_DATA){

        progId = CoTaskMemRealloc(progId,sizeProgId);
        res=RegQueryValueW(HKEY_CLASSES_ROOT,extention,progId,&sizeProgId);
    }
    if (res==ERROR_SUCCESS)
        /* return the clsid associated to the progId */
        res= CLSIDFromProgID(progId,pclsid);

    for(i=0; pathDec[i]!=NULL;i++)
        CoTaskMemFree(pathDec[i]);
    CoTaskMemFree(pathDec);

    CoTaskMemFree(progId);

    if (res==ERROR_SUCCESS)
        return res;

    return MK_E_INVALIDEXTENSION;
}
/***********************************************************************
 *           CoCreateInstance [COMPOBJ.13]
 *           CoCreateInstance [OLE32.7]
 */
HRESULT WINAPI CoCreateInstance(
	REFCLSID rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD dwClsContext,
	REFIID iid,
	LPVOID *ppv)
{
	HRESULT hres;
	LPCLASSFACTORY lpclf = 0;

  /*
   * Sanity check
   */
  if (ppv==0)
    return E_POINTER;

  /*
   * Initialize the "out" parameter
   */
  *ppv = 0;

  /*
   * Get a class factory to construct the object we want.
   */
  hres = CoGetClassObject(rclsid,
			  dwClsContext,
			  NULL,
			  &IID_IClassFactory,
			  (LPVOID)&lpclf);

  if (FAILED(hres)) {
    FIXME("no classfactory created for CLSID %s, hres is 0x%08lx\n",
	  debugstr_guid(rclsid),hres);
    return hres;
  }

  /*
   * Create the object and don't forget to release the factory
   */
	hres = IClassFactory_CreateInstance(lpclf, pUnkOuter, iid, ppv);
	IClassFactory_Release(lpclf);
	if(FAILED(hres))
	  FIXME("no instance created for interface %s of class %s, hres is 0x%08lx\n",
		debugstr_guid(iid), debugstr_guid(rclsid),hres);

	return hres;
}

/***********************************************************************
 *           CoCreateInstanceEx [OLE32.165]
 */
HRESULT WINAPI CoCreateInstanceEx(
  REFCLSID      rclsid,
  LPUNKNOWN     pUnkOuter,
  DWORD         dwClsContext,
  COSERVERINFO* pServerInfo,
  ULONG         cmq,
  MULTI_QI*     pResults)
{
  IUnknown* pUnk = NULL;
  HRESULT   hr;
  ULONG     index;
  int       successCount = 0;

  /*
   * Sanity check
   */
  if ( (cmq==0) || (pResults==NULL))
    return E_INVALIDARG;

  if (pServerInfo!=NULL)
    FIXME("() non-NULL pServerInfo not supported!\n");

  /*
   * Initialize all the "out" parameters.
   */
  for (index = 0; index < cmq; index++)
  {
    pResults[index].pItf = NULL;
    pResults[index].hr   = E_NOINTERFACE;
  }

  /*
   * Get the object and get its IUnknown pointer.
   */
  hr = CoCreateInstance(rclsid,
			pUnkOuter,
			dwClsContext,
			&IID_IUnknown,
			(VOID**)&pUnk);

  if (hr)
    return hr;

  /*
   * Then, query for all the interfaces requested.
   */
  for (index = 0; index < cmq; index++)
  {
    pResults[index].hr = IUnknown_QueryInterface(pUnk,
						 pResults[index].pIID,
						 (VOID**)&(pResults[index].pItf));

    if (pResults[index].hr == S_OK)
      successCount++;
  }

  /*
   * Release our temporary unknown pointer.
   */
  IUnknown_Release(pUnk);

  if (successCount == 0)
    return E_NOINTERFACE;

  if (successCount!=cmq)
    return CO_S_NOTALLINTERFACES;

  return S_OK;
}

/***********************************************************************
 *           CoLoadLibrary (OLE32.30)
 */
HINSTANCE WINAPI CoLoadLibrary(LPOLESTR lpszLibName, BOOL bAutoFree)
{
    TRACE("(%s, %d)\n", debugstr_w(lpszLibName), bAutoFree);

    return LoadLibraryExW(lpszLibName, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
}

/***********************************************************************
 *           CoFreeLibrary [OLE32.13]
 *
 * NOTES: don't belive the docu
 */
void WINAPI CoFreeLibrary(HINSTANCE hLibrary)
{
	FreeLibrary(hLibrary);
}


/***********************************************************************
 *           CoFreeAllLibraries [OLE32.12]
 *
 * NOTES: don't belive the docu
 */
void WINAPI CoFreeAllLibraries(void)
{
        /* NOP */
}


/***********************************************************************
 *           CoFreeUnusedLibraries [COMPOBJ.17]
 *           CoFreeUnusedLibraries [OLE32.14]
 *
 * FIXME: Calls to CoFreeUnusedLibraries from any thread always route
 * through the main apartment's thread to call DllCanUnloadNow
 */
void WINAPI CoFreeUnusedLibraries(void)
{
    COMPOBJ_DllList_FreeUnused(0);
}

/***********************************************************************
 *           CoFileTimeNow [COMPOBJ.82]
 *           CoFileTimeNow [OLE32.10]
 *
 * RETURNS
 *	the current system time in lpFileTime
 */
HRESULT WINAPI CoFileTimeNow( FILETIME *lpFileTime ) /* [out] the current time */
{
    GetSystemTimeAsFileTime( lpFileTime );
    return S_OK;
}

/***********************************************************************
 *           CoLoadLibrary (OLE32.30)
 */
static void COM_RevokeAllClasses()
{
  EnterCriticalSection( &csRegisteredClassList );

  while (firstRegisteredClass!=0)
  {
    CoRevokeClassObject(firstRegisteredClass->dwCookie);
  }

  LeaveCriticalSection( &csRegisteredClassList );
}

/****************************************************************************
 *  COM External Lock methods implementation
 *
 *  This api provides a linked list to managed external references to
 *  COM objects.
 *
 *  The public interface consists of three calls:
 *      COM_ExternalLockAddRef
 *      COM_ExternalLockRelease
 *      COM_ExternalLockFreeList
 */

#define EL_END_OF_LIST 0
#define EL_NOT_FOUND   0

/*
 * Declaration of the static structure that manage the
 * external lock to COM  objects.
 */
typedef struct COM_ExternalLock     COM_ExternalLock;
typedef struct COM_ExternalLockList COM_ExternalLockList;

struct COM_ExternalLock
{
  IUnknown         *pUnk;     /* IUnknown referenced */
  ULONG            uRefCount; /* external lock counter to IUnknown object*/
  COM_ExternalLock *next;     /* Pointer to next element in list */
};

struct COM_ExternalLockList
{
  COM_ExternalLock *head;     /* head of list */
};

/*
 * Declaration and initialization of the static structure that manages
 * the external lock to COM objects.
 */
static COM_ExternalLockList elList = { EL_END_OF_LIST };

/*
 * Private methods used to managed the linked list
 */


static COM_ExternalLock* COM_ExternalLockLocate(
  COM_ExternalLock *element,
  IUnknown         *pUnk);

/****************************************************************************
 * Internal - Insert a new IUnknown* to the linked list
 */
static BOOL COM_ExternalLockInsert(
  IUnknown *pUnk)
{
  COM_ExternalLock *newLock      = NULL;
  COM_ExternalLock *previousHead = NULL;

  /*
   * Allocate space for the new storage object
   */
  newLock = HeapAlloc(GetProcessHeap(), 0, sizeof(COM_ExternalLock));

  if (newLock!=NULL) {
    if ( elList.head == EL_END_OF_LIST ) {
      elList.head = newLock;    /* The list is empty */
    } else {
      /* insert does it at the head */
      previousHead  = elList.head;
      elList.head = newLock;
    }

    /* Set new list item data member */
    newLock->pUnk      = pUnk;
    newLock->uRefCount = 1;
    newLock->next      = previousHead;

    return TRUE;
  }
  return FALSE;
}

/****************************************************************************
 * Internal - Method that removes an item from the linked list.
 */
static void COM_ExternalLockDelete(
  COM_ExternalLock *itemList)
{
  COM_ExternalLock *current = elList.head;

  if ( current == itemList ) {
    /* this section handles the deletion of the first node */
    elList.head = itemList->next;
    HeapFree( GetProcessHeap(), 0, itemList);
  } else {
    do {
      if ( current->next == itemList ){   /* We found the item to free  */
        current->next = itemList->next;  /* readjust the list pointers */
        HeapFree( GetProcessHeap(), 0, itemList);
        break;
      }

      /* Skip to the next item */
      current = current->next;

    } while ( current != EL_END_OF_LIST );
  }
}

/****************************************************************************
 * Internal - Recursivity agent for IUnknownExternalLockList_Find
 *
 * NOTES: how long can the list be ?? (recursive!!!)
 */
static COM_ExternalLock* COM_ExternalLockLocate( COM_ExternalLock *element, IUnknown *pUnk)
{
  if ( element == EL_END_OF_LIST )
    return EL_NOT_FOUND;
  else if ( element->pUnk == pUnk )    /* We found it */
    return element;
  else                                 /* Not the right guy, keep on looking */
    return COM_ExternalLockLocate( element->next, pUnk);
}

/****************************************************************************
 * Public - Method that increments the count for a IUnknown* in the linked
 * list.  The item is inserted if not already in the list.
 */
static void COM_ExternalLockAddRef(IUnknown *pUnk)
{
  COM_ExternalLock *externalLock = COM_ExternalLockLocate(elList.head, pUnk);

  /*
   * Add an external lock to the object. If it was already externally
   * locked, just increase the reference count. If it was not.
   * add the item to the list.
   */
  if ( externalLock == EL_NOT_FOUND )
    COM_ExternalLockInsert(pUnk);
  else
    externalLock->uRefCount++;

  /*
   * Add an internal lock to the object
   */
  IUnknown_AddRef(pUnk);
}

/****************************************************************************
 * Public - Method that decrements the count for a IUnknown* in the linked
 * list.  The item is removed from the list if its count end up at zero or if
 * bRelAll is TRUE.
 */
static void COM_ExternalLockRelease(
  IUnknown *pUnk,
  BOOL   bRelAll)
{
  COM_ExternalLock *externalLock = COM_ExternalLockLocate(elList.head, pUnk);

  if ( externalLock != EL_NOT_FOUND ) {
    do {
      externalLock->uRefCount--;  /* release external locks      */
      IUnknown_Release(pUnk);     /* release local locks as well */

      if ( bRelAll == FALSE ) break;  /* perform single release */

    } while ( externalLock->uRefCount > 0 );

    if ( externalLock->uRefCount == 0 )  /* get rid of the list entry */
      COM_ExternalLockDelete(externalLock);
  }
}
/****************************************************************************
 * Public - Method that frees the content of the list.
 */
static void COM_ExternalLockFreeList()
{
  COM_ExternalLock *head;

  head = elList.head;                 /* grab it by the head             */
  while ( head != EL_END_OF_LIST ) {
    COM_ExternalLockDelete(head);     /* get rid of the head stuff       */
    head = elList.head;               /* get the new head...             */
  }
}

/****************************************************************************
 * Public - Method that dump the content of the list.
 */
void COM_ExternalLockDump()
{
  COM_ExternalLock *current = elList.head;

  DPRINTF("\nExternal lock list contains:\n");

  while ( current != EL_END_OF_LIST ) {
    DPRINTF( "\t%p with %lu references count.\n", current->pUnk, current->uRefCount);

    /* Skip to the next item */
    current = current->next;
  }
}

/******************************************************************************
 *		CoLockObjectExternal	[OLE32.31]
 */
HRESULT WINAPI CoLockObjectExternal(
    LPUNKNOWN pUnk,		/* [in] object to be locked */
    BOOL fLock,		/* [in] do lock */
    BOOL fLastUnlockReleases) /* [in] unlock all */
{

	if (fLock) {
            /*
             * Increment the external lock coutner, COM_ExternalLockAddRef also
             * increment the object's internal lock counter.
             */
	    COM_ExternalLockAddRef( pUnk);
        } else {
            /*
             * Decrement the external lock coutner, COM_ExternalLockRelease also
             * decrement the object's internal lock counter.
             */
            COM_ExternalLockRelease( pUnk, fLastUnlockReleases);
        }

        return S_OK;
}

/***********************************************************************
 *           CoInitializeWOW (OLE32.27)
 */
HRESULT WINAPI CoInitializeWOW(DWORD x,DWORD y) {
    FIXME("(0x%08lx,0x%08lx),stub!\n",x,y);
    return 0;
}

static IUnknown * pUnkState = 0; /* FIXME: thread local */
static int nStatCounter = 0;	 /* global */
static HMODULE hOleAut32 = 0;	 /* global */

/***********************************************************************
 *           CoGetState [OLE32.@]
 *
 * NOTES: might be incomplete
 */
HRESULT WINAPI CoGetState(IUnknown ** ppv)
{
	FIXME("\n");

	if(pUnkState) {
	    IUnknown_AddRef(pUnkState);
	    *ppv = pUnkState;
	    FIXME("-- %p\n", *ppv);
	    return S_OK;
	}
	*ppv = NULL;
	return E_FAIL;

}

/***********************************************************************
 *           CoSetState [OLE32.42]
 *
 * NOTES: FIXME: protect this with a crst
 */
HRESULT WINAPI CoSetState(IUnknown * pv)
{
	FIXME("(%p),stub!\n", pv);

	if (pv) {
	    IUnknown_AddRef(pv);
	    nStatCounter++;
	    if (nStatCounter == 1) LoadLibraryA("OLEAUT32.DLL");
	}

	if (pUnkState) {
	    TRACE("-- release %p now\n", pUnkState);
	    IUnknown_Release(pUnkState);
	    nStatCounter--;
	    if (!nStatCounter) FreeLibrary(hOleAut32);
	}
	pUnkState = pv;
	return S_OK;
}


/******************************************************************************
 *              OleGetAutoConvert        [OLE32.104]
 */
HRESULT WINAPI OleGetAutoConvert(REFCLSID clsidOld, LPCLSID pClsidNew)
{
    HKEY hkey = 0;
    char buf[200];
    WCHAR wbuf[200];
    DWORD len;
    HRESULT res = S_OK;

    sprintf(buf,"CLSID\\");WINE_StringFromCLSID(clsidOld,&buf[6]);
    if (RegOpenKeyA(HKEY_CLASSES_ROOT,buf,&hkey))
    {
        res = REGDB_E_CLASSNOTREG;
	goto done;
    }
    len = 200;
    /* we can just query for the default value of AutoConvertTo key like that,
       without opening the AutoConvertTo key and querying for NULL (default) */
    if (RegQueryValueA(hkey,"AutoConvertTo",buf,&len))
    {
        res = REGDB_E_KEYMISSING;
	goto done;
    }
    MultiByteToWideChar( CP_ACP, 0, buf, -1, wbuf, sizeof(wbuf)/sizeof(WCHAR) );
    CLSIDFromString(wbuf,pClsidNew);
done:
    if (hkey) RegCloseKey(hkey);
    return res;
}

/******************************************************************************
 *              OleSetAutoConvert        [OLE32.126]
 */
HRESULT WINAPI OleSetAutoConvert(REFCLSID clsidOld, REFCLSID clsidNew)
{
    HKEY hkey = 0;
    char buf[200], szClsidNew[200];
    HRESULT res = S_OK;

    TRACE("(%s,%s)\n", debugstr_guid(clsidOld), debugstr_guid(clsidNew));
    sprintf(buf,"CLSID\\");WINE_StringFromCLSID(clsidOld,&buf[6]);
    WINE_StringFromCLSID(clsidNew, szClsidNew);
    if (RegOpenKeyA(HKEY_CLASSES_ROOT,buf,&hkey))
    {
        res = REGDB_E_CLASSNOTREG;
	goto done;
    }
    if (RegSetValueA(hkey, "AutoConvertTo", REG_SZ, szClsidNew, strlen(szClsidNew)+1))
    {
        res = REGDB_E_WRITEREGDB;
	goto done;
    }

done:
    if (hkey) RegCloseKey(hkey);
    return res;
}

/******************************************************************************
 *              CoTreatAsClass        [OLE32.46]
 */
HRESULT WINAPI CoTreatAsClass(REFCLSID clsidOld, REFCLSID clsidNew)
{
    HKEY hkey = 0;
    char buf[200], szClsidNew[200];
    HRESULT res = S_OK;

    FIXME("(%s,%s)\n", debugstr_guid(clsidOld), debugstr_guid(clsidNew));
    sprintf(buf,"CLSID\\");WINE_StringFromCLSID(clsidOld,&buf[6]);
    WINE_StringFromCLSID(clsidNew, szClsidNew);
    if (RegOpenKeyA(HKEY_CLASSES_ROOT,buf,&hkey))
    {
        res = REGDB_E_CLASSNOTREG;
	goto done;
    }
    if (RegSetValueA(hkey, "AutoTreatAs", REG_SZ, szClsidNew, strlen(szClsidNew)+1))
    {
        res = REGDB_E_WRITEREGDB;
	goto done;
    }

done:
    if (hkey) RegCloseKey(hkey);
    return res;
}


/***********************************************************************
 *           IsEqualGUID [OLE32.76]
 *
 * Compares two Unique Identifiers.
 *
 * RETURNS
 *	TRUE if equal
 */
#undef IsEqualGUID
BOOL WINAPI IsEqualGUID(
     REFGUID rguid1, /* [in] unique id 1 */
     REFGUID rguid2  /* [in] unique id 2 */
     )
{
    return !memcmp(rguid1,rguid2,sizeof(GUID));
}

/***********************************************************************
 *           CoInitializeSecurity [OLE32.@]
 */
HRESULT WINAPI CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
                                    SOLE_AUTHENTICATION_SERVICE* asAuthSvc,
                                    void* pReserved1, DWORD dwAuthnLevel,
                                    DWORD dwImpLevel, void* pReserved2,
                                    DWORD dwCapabilities, void* pReserved3)
{
  FIXME("(%p,%ld,%p,%p,%ld,%ld,%p,%ld,%p) - stub!\n", pSecDesc, cAuthSvc,
        asAuthSvc, pReserved1, dwAuthnLevel, dwImpLevel, pReserved2,
        dwCapabilities, pReserved3);
  return S_OK;
}

