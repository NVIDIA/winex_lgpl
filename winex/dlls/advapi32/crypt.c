/*
 * dlls/advapi32/crypt.c
 */

/***********************************************************************
 *
 *  TODO:
 *  - Reference counting
 *  - Thread-safing
 *  - Signature checking
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "wine/unicode.h"
#include "crypt.h"
#include "winnls.h"
#include "wincrypt.h"
#include "windef.h"
#include "winerror.h"
#include "winreg.h"
#include "winbase.h"
#include "winuser.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(crypt);

HWND crypt_hWindow = 0;

#define CRYPT_ReturnLastError(err) {SetLastError(err); return FALSE;}

#define CRYPT_Alloc(size) ((LPVOID)LocalAlloc(LMEM_ZEROINIT, size))
#define CRYPT_Free(buffer) (LocalFree((HLOCAL)buffer))

/* CSP context handle table - all valid CSP contexts must have a handle listed here */
static CSPHANDLETABLE csp_handle_table;

/* initialize table to hold the list of all valid context handles */
void init_csp_handle_table()
{
  TRACE("table=%p\n", &csp_handle_table);

  csp_handle_table.first = NULL;
  InitializeCriticalSection(&csp_handle_table.lock);
} 

/* destroy list of all valid context providers */
void deinit_csp_handle_table()
{
  TRACE("table=%p\n", &csp_handle_table);

  /* we could attempt to delete any remaining CSP handles here,
     but that is really the App's job, we are exiting now anyhow... */

  DeleteCriticalSection(&csp_handle_table.lock);
}

/* check if pProv is listed in the table of valid context handles */
static BOOL is_valid_csp_handle( PCRYPTPROV pProv )
{
  CSPHANDLE *cur;
  BOOL found = FALSE;

  EnterCriticalSection(&csp_handle_table.lock);
  cur = csp_handle_table.first;
  while( cur ) {
    if( cur->pProv == pProv ) {
      found = TRUE;
      break;
    }
    cur = cur->next;
  } 
  LeaveCriticalSection(&csp_handle_table.lock);
  TRACE("pProv=%p valid=%d\n", pProv, found);
  return found;
}

/* check if we have any existing contexts using the specified
 * provider name and provider type so that we can reuse the
 * context instead of createing a new one in CryptAcquireContext
 * all the time */
#if 0
static PCRYPTPROV find_existing_csp_handle( PSTR provname, DWORD provtype )
{
  CSPHANDLE *cur;
  PCRYPTPROV prov=NULL;

  EnterCriticalSection(&csp_handle_table.lock);
  cur = csp_handle_table.first;
  while( cur ) {
    if( (0 == strcmp(provname, cur->pProv->pVTable->pszProvName)) &&
        (provtype == cur->pProv->pVTable->dwProvType) ) {
      prov = cur->pProv;
      break;
    }
    cur = cur->next;
  }
  LeaveCriticalSection(&csp_handle_table.lock);
  TRACE("found %p matching (%s, %u)\n", prov, provname, provtype);
  return prov;
}
#endif

/* add newly created context handle to table */
static BOOL add_csp_handle_to_table( PCRYPTPROV pProv )
{
  CSPHANDLE *new;
  TRACE("adding prov %p\n", pProv);

  new = CRYPT_Alloc(sizeof(CSPHANDLE));
  if( !new ) {
    return FALSE;
  }
  new->pProv = pProv;
  /* insert new handle entry at the front of the list */
  EnterCriticalSection(&csp_handle_table.lock);
  new->next = csp_handle_table.first;
  csp_handle_table.first = new;
  LeaveCriticalSection(&csp_handle_table.lock);
  return TRUE;
}

/* delete context handle from table of valid context handles
 * - must call is_valid_csp_handle to ensure its really in the
 * list before trying to delete it */
static void delete_csp_handle_from_table( PCRYPTPROV pProv )
{
  CSPHANDLE *cur, *last;

  TRACE("deleting prov %p\n", pProv);
  EnterCriticalSection(&csp_handle_table.lock);
  last = cur = csp_handle_table.first;
  while( cur && (cur->pProv != pProv) ) {
    last = cur;
    cur = cur->next; 
  }
  if( cur && cur->pProv == pProv ) {
    /* actually found it, now remove it from list and delete it */
    if( cur == csp_handle_table.first ) 
      csp_handle_table.first = cur->next;
    else 
      last->next = cur->next;
    CRYPT_Free(cur);
  }

  LeaveCriticalSection(&csp_handle_table.lock);
}

static inline PSTR CRYPT_GetProvKeyName(PCSTR pProvName)
{
	const PSTR KEYSTR = "Software\\Microsoft\\Cryptography\\Defaults\\Provider\\";
	PSTR keyname;

	keyname = CRYPT_Alloc(strlen(KEYSTR) + strlen(pProvName) +1);
	if (keyname)
	{
		strcpy(keyname, KEYSTR);
		strcpy(keyname + strlen(KEYSTR), pProvName);
	} else
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return keyname;
}

static inline PSTR CRYPT_GetTypeKeyName(DWORD dwType, BOOL user)
{
	const PSTR MACHINESTR = "Software\\Microsoft\\Cryptography\\Defaults\\Provider Types\\Type XXX";
	const PSTR USERSTR = "Software\\Microsoft\\Cryptography\\Provider Type XXX";
	PSTR keyname;
	PSTR ptr;

	keyname = CRYPT_Alloc( (user ? strlen(USERSTR) : strlen(MACHINESTR)) +1);
	if (keyname)
	{
		user ? strcpy(keyname, USERSTR) : strcpy(keyname, MACHINESTR);
		ptr = keyname + strlen(keyname);
		*(--ptr) = (dwType % 10) + '0';
		*(--ptr) = (dwType / 10) + '0';
		*(--ptr) = (dwType / 100) + '0';
	} else
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return keyname;
}

/* CRYPT_UnicodeTOANSI
 * wstr - unicode string
 * str - pointer to ANSI string
 * strsize - size of buffer pointed to by str or -1 if we have to do the allocation
 *
 * returns TRUE if unsuccessfull, FALSE otherwise.
 * if wstr is NULL, returns TRUE and sets str to NULL! Value of str should be checked after call
 */
static inline BOOL CRYPT_UnicodeToANSI(LPCWSTR wstr, LPSTR* str, int strsize)
{
	int count;

	if (!wstr)
	{
		*str = NULL;
		return TRUE;
	}
	count = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (strsize != -1)
		count = count < strsize ? count : strsize;
	if (strsize == -1)
		*str = CRYPT_Alloc(count * sizeof(CHAR));
	if (*str)
	{
		WideCharToMultiByte(CP_ACP, 0, wstr, -1, *str, count, NULL, NULL);
		return TRUE;
	}
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;
}

/* CRYPT_ANSITOUnicode
 * str - ANSI string
 * wstr - pointer to unicode string
 * wstrsize - size of buffer pointed to by wstr or -1 if we have to do the allocation
 */
static inline BOOL CRYPT_ANSIToUnicode(LPCSTR str, LPWSTR* wstr, int wstrsize)
{
	int wcount;

	if (!str)
	{
		*wstr = NULL;
		return TRUE;
	}
	wcount = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
	if (wstrsize != -1)
		wcount = wcount < wstrsize/sizeof(WCHAR) ? wcount : wstrsize/sizeof(WCHAR);
	if (wstrsize == -1)
		*wstr = CRYPT_Alloc(wcount * sizeof(WCHAR));
	if (*wstr)
	{
		MultiByteToWideChar(CP_ACP, 0, str, -1, *wstr, wcount);
		return TRUE;
	}
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;
}

/* These next 2 functions are used by the VTableProvStruc structure */
BOOL CRYPT_VerifyImage(LPCSTR lpszImage, BYTE* pData)
{
	FIXME("(%p (%s), %p): stub\n", lpszImage, lpszImage, pData);
	if (!lpszImage || !pData)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	/* FIXME: Actually verify the image! */
	return TRUE;
}

BOOL CRYPT_ReturnhWnd(DWORD *phWnd)
{
	TRACE("(%p)\n", phWnd);
	if (!phWnd)
		return FALSE;
	*phWnd = crypt_hWindow;
	return TRUE;
}

#define CRYPT_GetProvFunc(name) \
	if ( !(provider->pFuncs->p##name = (void*)GetProcAddress(provider->hModule, #name)) ) goto error
#define CRYPT_GetProvFuncOpt(name) \
	provider->pFuncs->p##name = (void*)GetProcAddress(provider->hModule, #name)
PCRYPTPROV CRYPT_LoadProvider(PSTR pImage)
{
	PCRYPTPROV provider;
	DWORD errorcode = ERROR_NOT_ENOUGH_MEMORY;

	if ( !(provider = CRYPT_Alloc(sizeof(CRYPTPROV))) ) goto error;
	if ( !(provider->pFuncs = CRYPT_Alloc(sizeof(PROVFUNCS))) ) goto error;
	if ( !(provider->pVTable = CRYPT_Alloc(sizeof(VTableProvStruc))) ) goto error;
	if ( !(provider->hModule = LoadLibraryA(pImage)) )
	{
		errorcode = (GetLastError() == ERROR_FILE_NOT_FOUND) ? NTE_PROV_DLL_NOT_FOUND : NTE_PROVIDER_DLL_FAIL;
		goto error;
	}
  	provider->ref = 1;

	errorcode = NTE_PROVIDER_DLL_FAIL;
	CRYPT_GetProvFunc(CPAcquireContext);
	CRYPT_GetProvFunc(CPCreateHash);
	CRYPT_GetProvFunc(CPDecrypt);
	CRYPT_GetProvFunc(CPDeriveKey);
	CRYPT_GetProvFunc(CPDestroyHash);
	CRYPT_GetProvFunc(CPDestroyKey);
	CRYPT_GetProvFuncOpt(CPDuplicateHash);
	CRYPT_GetProvFuncOpt(CPDuplicateKey);
	CRYPT_GetProvFunc(CPEncrypt);
	CRYPT_GetProvFunc(CPExportKey);
	CRYPT_GetProvFunc(CPGenKey);
	CRYPT_GetProvFunc(CPGenRandom);
	CRYPT_GetProvFunc(CPGetHashParam);
	CRYPT_GetProvFunc(CPGetKeyParam);
	CRYPT_GetProvFunc(CPGetProvParam);
	CRYPT_GetProvFunc(CPGetUserKey);
	CRYPT_GetProvFunc(CPHashData);
	CRYPT_GetProvFunc(CPHashSessionKey);
	CRYPT_GetProvFunc(CPImportKey);
	CRYPT_GetProvFunc(CPReleaseContext);
	CRYPT_GetProvFunc(CPSetHashParam);
	CRYPT_GetProvFunc(CPSetKeyParam);
	CRYPT_GetProvFunc(CPSetProvParam);
	CRYPT_GetProvFunc(CPSignHash);
	CRYPT_GetProvFunc(CPVerifySignature);

	/* FIXME: Not sure what the pbContextInfo field is for.
	 *        Does it need memory allocation?
         */
	provider->pVTable->Version = 3;
	provider->pVTable->pFuncVerifyImage = (FARPROC)CRYPT_VerifyImage;
	provider->pVTable->pFuncReturnhWnd = (FARPROC)CRYPT_ReturnhWnd;
	provider->pVTable->dwProvType = 0;
	provider->pVTable->pbContextInfo = NULL;
	provider->pVTable->cbContextInfo = 0;
	provider->pVTable->pszProvName = NULL;
	return provider;

error:
	SetLastError(errorcode);
	if (provider)
	{
		if (provider->hModule)
			FreeLibrary(provider->hModule);
		CRYPT_Free(provider->pVTable);
		CRYPT_Free(provider->pFuncs);
		CRYPT_Free(provider);
	}
	return NULL;
}
#undef CRYPT_GetProvFunc
#undef CRYPT_GetProvFuncOpt


/******************************************************************************
 * CryptAcquireContextA (ADVAPI32.@)
 * Acquire a crypto provider context handle.
 *
 * PARAMS
 * phProv: Pointer to HCRYPTPROV for the output.
 * pszContainer: Key Container Name
 * pszProvider: Cryptographic Service Provider Name
 * dwProvType: Crypto provider type to get a handle.
 * dwFlags: flags for the operation
 *
 * RETURNS TRUE on success, FALSE on failure.
 */
BOOL WINAPI CryptAcquireContextA (HCRYPTPROV *phProv, LPCSTR pszContainer,
		LPCSTR pszProvider, DWORD dwProvType, DWORD dwFlags)
{
	PCRYPTPROV pProv = NULL;
	HKEY key;
	PSTR imagepath = NULL, keyname = NULL, provname = NULL, temp = NULL;
#if 0
	BYTE* signature;
#endif
	DWORD keytype, type, len;

	TRACE("(%p, %s, %s, %u, %08x)\n", phProv, pszContainer,
		pszProvider, dwProvType, dwFlags);

	if (!phProv || !dwProvType)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	*phProv = 0;
	
	if (dwProvType > MAXPROVTYPES)
		CRYPT_ReturnLastError(NTE_BAD_PROV_TYPE);

	if (!pszProvider)
	{
		/* No CSP name specified so try the user default CSP first
		 * then try the machine default CSP
		 */
		if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, TRUE)) )
			CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
		if (RegOpenKeyA(HKEY_CURRENT_USER, keyname, &key))
		{
			CRYPT_Free(keyname);
			if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, FALSE)) )
				CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
			if (RegOpenKeyA(HKEY_LOCAL_MACHINE, keyname, &key)) goto error;
		}
		CRYPT_Free(keyname);
		RegQueryValueExA(key, "Name", NULL, &keytype, NULL, &len);
		if (!len || keytype != REG_SZ || !(provname = CRYPT_Alloc(len))) goto error;
		RegQueryValueExA(key, "Name", NULL, NULL, (LPBYTE)provname, &len);
		RegCloseKey(key);
	} else {
		if ( !(provname = CRYPT_Alloc(strlen(pszProvider) +1)) )
			CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
		strcpy(provname, pszProvider);
	}

	/* TODO: 
	  pProv = find_existing_csp_handle( provname, dwProvType );
	 * to check for an existing handle for this csp, and return it instead of 
	 * recreating it. Don't forget CryptContextAddRef()!
	 * Need to test on Windows to verify this functionality  */

	keyname = CRYPT_GetProvKeyName(provname);
	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, keyname, &key)) goto error;
	CRYPT_Free(keyname);
	keyname = NULL;
	len = sizeof(DWORD);
	RegQueryValueExA(key, "Type", NULL, NULL, (BYTE*)&type, &len);
	if (type != dwProvType)
	{
		SetLastError(NTE_BAD_PROV_TYPE);
		goto error;
	}

	RegQueryValueExA(key, "Image Path", NULL, &keytype, NULL, &len);
	if (keytype != REG_SZ || !(temp = CRYPT_Alloc(len))) goto error;
	RegQueryValueExA(key, "Image Path", NULL, NULL, (LPBYTE)temp, &len);

	RegCloseKey(key);
	len = ExpandEnvironmentStringsA(temp, NULL, 0);
	if ( !(imagepath = CRYPT_Alloc(len)) )
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		goto error;
	}
	if (!ExpandEnvironmentStringsA(temp, imagepath, len)) goto error;

#if 0
	if (!CRYPT_VerifyImage(imagepath, signature)) goto error;
#endif
	pProv = CRYPT_LoadProvider(imagepath);
	
	CRYPT_Free(temp);
	temp = NULL;
	CRYPT_Free(imagepath);
	imagepath = NULL;
	
	if (!pProv) goto error;

	pProv->pVTable->pszProvName = provname;
	pProv->pVTable->dwProvType = dwProvType;
	if (pProv->pFuncs->pCPAcquireContext(&pProv->hPrivate, (CHAR*)pszContainer, dwFlags, pProv->pVTable))
	{
		/* MSDN: When this flag is set, the value returned in phProv is undefined,
		 *       and thus, the CryptReleaseContext function need not be called afterwards.
		 *       Therefore, we must clean up everything now.
                 */
		if (dwFlags & CRYPT_DELETEKEYSET)
		{
			FreeLibrary(pProv->hModule);
			pProv->pVTable->pszProvName = NULL;
			CRYPT_Free(provname);
			CRYPT_Free(pProv->pFuncs);
			CRYPT_Free(pProv);
		} else {
			if( !add_csp_handle_to_table( pProv ) ) {
				SetLastError(ERROR_NOT_ENOUGH_MEMORY);
				goto error;
			}
			*phProv = (HCRYPTPROV)pProv;
		}
		return TRUE;
	}
	else
	{
		WARN("Acquisition of provider context failed with %08x\n", GetLastError());
	}
	//FALLTHROUGH TO ERROR IF FALSE - CSP internal error!
error:
	WARN("Failed to acquire context with GLE=%08x\n", GetLastError());
	if (pProv)
	{
		FreeLibrary(pProv->hModule);
		CRYPT_Free(pProv->pVTable);
		CRYPT_Free(pProv->pFuncs);
		CRYPT_Free(pProv);
	}
	CRYPT_Free(provname);
	CRYPT_Free(temp);
	CRYPT_Free(imagepath);
	CRYPT_Free(keyname);
	return FALSE;
}

/******************************************************************************
 * CryptAcquireContextW (ADVAPI32.@)
 */
BOOL WINAPI CryptAcquireContextW (HCRYPTPROV *phProv, LPCWSTR pszContainer,
		LPCWSTR pszProvider, DWORD dwProvType, DWORD dwFlags)
{
	PSTR pProvider = NULL, pContainer = NULL;
	BOOL ret = FALSE;

	TRACE("(%p, %s, %s, %u, %08x)\n", phProv, debugstr_w(pszContainer),
		debugstr_w(pszProvider), dwProvType, dwFlags);

	if ( !CRYPT_UnicodeToANSI(pszContainer, &pContainer, -1) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if ( !CRYPT_UnicodeToANSI(pszProvider, &pProvider, -1) )
	{
		CRYPT_Free(pContainer);
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	}

	ret = CryptAcquireContextA(phProv, pContainer, pProvider, dwProvType, dwFlags);

	if (pContainer)
		CRYPT_Free(pContainer);
	if (pProvider)
		CRYPT_Free(pProvider);

	return ret;
}

/******************************************************************************
 * CryptContextAddRef (ADVAPI32.@)
 */
BOOL WINAPI CryptContextAddRef (HCRYPTPROV hProv, DWORD *pdwReserved, DWORD dwFlags)
{
	PCRYPTPROV pProv = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %p, %08x)\n", hProv, pdwReserved, dwFlags);

	if (!pProv || !is_valid_csp_handle(pProv) || pdwReserved || dwFlags)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	/* InterlockIncrement?? */
	pProv->ref++;
	return TRUE;
}

/******************************************************************************
 * CryptReleaseContext (ADVAPI32.@)
 */
BOOL WINAPI CryptReleaseContext (HCRYPTPROV hProv, ULONG_PTR dwFlags)
{
	PCRYPTPROV pProv = (PCRYPTPROV)hProv;
	BOOL ret = TRUE;

	TRACE("(0x%lx, %08lx)\n", hProv, dwFlags);

	if (!pProv)
		CRYPT_ReturnLastError(NTE_BAD_UID);
	if (!is_valid_csp_handle(pProv)) 
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	/* Decrement the ref counter */
	pProv->ref--; 

	if (pProv->ref == 0)
	{
		ret = pProv->pFuncs->pCPReleaseContext(pProv->hPrivate, dwFlags);
		
		delete_csp_handle_from_table( pProv );
		FreeLibrary(pProv->hModule);
#if 0
		CRYPT_Free(pProv->pVTable->pContextInfo);
#endif
		CRYPT_Free(pProv->pVTable->pszProvName);
		CRYPT_Free(pProv->pVTable);
		CRYPT_Free(pProv->pFuncs);
		CRYPT_Free(pProv);
	}

	/* MSDN: If dwFlags is not zero, this function returns FALSE but the CSP is released.
	     NTE_BAD_FLAGS - The dwFlags parameter is nonzero. */
	if (dwFlags)
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	return ret;
}

/******************************************************************************
 * CryptGenRandom (ADVAPI32.@)
 */
BOOL WINAPI CryptGenRandom (HCRYPTPROV hProv, DWORD dwLen, BYTE *pbBuffer)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %u, %p)\n", hProv, dwLen, pbBuffer);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPGenRandom(prov->hPrivate, dwLen, pbBuffer);
}

/******************************************************************************
 * CryptCreateHash (ADVAPI32.@)
 */
BOOL WINAPI CryptCreateHash (HCRYPTPROV hProv, ALG_ID Algid, HCRYPTKEY hKey,
		DWORD dwFlags, HCRYPTHASH *phHash)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTHASH hash;

	TRACE("(0x%lx, 0x%x, 0x%lx, %08x, %p)\n", hProv, Algid, hKey, dwFlags, phHash);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!phHash)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags)
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	*phHash = 0;
	if ( !(hash = CRYPT_Alloc(sizeof(CRYPTHASH))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	hash->pProvider = prov;

	if (prov->pFuncs->pCPCreateHash(prov->hPrivate, Algid,
			key ? key->hPrivate : 0, 0, &hash->hPrivate))
	{
		*phHash = (HCRYPTHASH)hash;
		return TRUE;
	}

	/* CSP error! */
	CRYPT_Free(hash);
	return FALSE;
}

/******************************************************************************
 * CryptDecrypt (ADVAPI32.@)
 */
BOOL WINAPI CryptDecrypt (HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
		DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, 0x%lx, %d, %08x, %p, %p)\n", hKey, hHash, Final, dwFlags, pbData, pdwDataLen);

	if (!key || !pbData || !pdwDataLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	if (!prov || !is_valid_csp_handle(prov)) 
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPDecrypt(prov->hPrivate, key->hPrivate, hash ? hash->hPrivate : 0,
			Final, dwFlags, pbData, pdwDataLen);
}

/******************************************************************************
 * CryptDeriveKey (ADVAPI32.@)
 */
BOOL WINAPI CryptDeriveKey (HCRYPTPROV hProv, ALG_ID Algid, HCRYPTHASH hBaseData,
		DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTHASH hash = (PCRYPTHASH)hBaseData;
	PCRYPTKEY key;

	TRACE("(0x%lx, %d, 0x%ld, %08x, %p)\n", hProv, Algid, hBaseData, dwFlags, phKey);

	*phKey = 0;
	if (!prov || !hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!is_valid_csp_handle(prov)) 
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!phKey)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if ( !(key = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	key->pProvider = prov;
	if (prov->pFuncs->pCPDeriveKey(prov->hPrivate, Algid, hash->hPrivate, dwFlags, &key->hPrivate)) {
		*phKey = (HCRYPTKEY)key;
		return TRUE;
	}

	/* CSP error! */
	CRYPT_Free(key);
	return FALSE;
}

/******************************************************************************
 * CryptDestroyHash (ADVAPI32.@)
 */
BOOL WINAPI CryptDestroyHash (HCRYPTHASH hHash)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTPROV prov;
	BOOL ret;

	TRACE("(0x%lx)\n", hHash);

	if (!hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	ret = prov->pFuncs->pCPDestroyHash(prov->hPrivate, hash->hPrivate);
	CRYPT_Free(hash);
	return ret;
}

/******************************************************************************
 *  CryptDestroyKey (ADVAPI32.@)
 */
BOOL WINAPI CryptDestroyKey (HCRYPTKEY hKey)
{
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTPROV prov;
	BOOL ret;

	TRACE("(0x%lx)\n", hKey);

	if (!key)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	prov = key->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	ret = prov->pFuncs->pCPDestroyKey(prov->hPrivate, key->hPrivate);
	CRYPT_Free(key);
	return ret;
}

/******************************************************************************
 * CryptDuplicateHash (ADVAPI32.@)
 */
BOOL WINAPI CryptDuplicateHash (HCRYPTHASH hHash, DWORD *pdwReserved,
		DWORD dwFlags, HCRYPTHASH *phHash)
{
	PCRYPTPROV prov;
	PCRYPTHASH orghash, newhash;

	TRACE("(0x%lx, %p, %08x, %p)\n", hHash, pdwReserved, dwFlags, phHash);

	orghash = (PCRYPTHASH)hHash;
	if (!orghash || pdwReserved || !phHash)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = orghash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!prov->pFuncs->pCPDuplicateHash)
		CRYPT_ReturnLastError(ERROR_CALL_NOT_IMPLEMENTED);

	if ( !(newhash = CRYPT_Alloc(sizeof(CRYPTHASH))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	newhash->pProvider = prov;
	if (prov->pFuncs->pCPDuplicateHash(prov->hPrivate, orghash->hPrivate, pdwReserved, dwFlags, &newhash->hPrivate))
	{
		*phHash = (HCRYPTHASH)newhash;
		return TRUE;
	}
	CRYPT_Free(newhash);
	return FALSE;
}

/******************************************************************************
 * CryptDuplicateKey (ADVAPI32.@)
 */
BOOL WINAPI CryptDuplicateKey (HCRYPTKEY hKey, DWORD *pdwReserved, DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov;
	PCRYPTKEY orgkey, newkey;

	TRACE("(0x%lx, %p, %08x, %p)\n", hKey, pdwReserved, dwFlags, phKey);

	orgkey = (PCRYPTKEY)hKey;
	if (!orgkey || pdwReserved || !phKey)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = orgkey->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!prov->pFuncs->pCPDuplicateKey)
		CRYPT_ReturnLastError(ERROR_CALL_NOT_IMPLEMENTED);

	if ( !(newkey = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	newkey->pProvider = prov;
	if (prov->pFuncs->pCPDuplicateKey(prov->hPrivate, orgkey->hPrivate, pdwReserved, dwFlags, &newkey->hPrivate))
	{
		*phKey = (HCRYPTKEY)newkey;
		return TRUE;
	}
	CRYPT_Free(newkey);
	return FALSE;
}

/******************************************************************************
 * CryptEncrypt (ADVAPI32.@)
 */
BOOL WINAPI CryptEncrypt (HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
		DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen, DWORD dwBufLen)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, 0x%lx, %d, %08x, %p, %p, %u)\n", hKey, hHash, Final, dwFlags, pbData, pdwDataLen, dwBufLen);

	if (!key || !pdwDataLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPEncrypt(prov->hPrivate, key->hPrivate, hash ? hash->hPrivate : 0,
			Final, dwFlags, pbData, pdwDataLen, dwBufLen);
}

/******************************************************************************
 * CryptEnumProvidersA (ADVAPI32.@)
 */
BOOL WINAPI CryptEnumProvidersA (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPSTR pszProvName, DWORD *pcbProvName)
{
	HKEY hKey;

	TRACE("(%u, %p, %x, %p, %p, %p)\n", dwIndex, pdwReserved, dwFlags,
			pdwProvType, pszProvName, pcbProvName);

	if (pdwReserved || !pcbProvName) CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags) CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Cryptography\\Defaults\\Provider", &hKey))
		CRYPT_ReturnLastError(NTE_FAIL);

	if (!pszProvName)
	{
		DWORD numkeys;
		RegQueryInfoKeyA(hKey, NULL, NULL, NULL, &numkeys, pcbProvName, NULL, NULL, NULL, NULL, NULL, NULL);
		(*pcbProvName)++;
		if (dwIndex >= numkeys)
			CRYPT_ReturnLastError(ERROR_NO_MORE_ITEMS);
	} else {
		DWORD size = sizeof(DWORD);
		HKEY subkey;
		if (RegEnumKeyA(hKey, dwIndex, pszProvName, *pcbProvName))
			return FALSE;
		if (RegOpenKeyA(hKey, pszProvName, &subkey))
			return FALSE;
		if (RegQueryValueExA(subkey, "Type", NULL, NULL, (BYTE*)pdwProvType, &size))
			return FALSE;
		RegCloseKey(subkey);
	}
	RegCloseKey(hKey);
	return TRUE;
}

/******************************************************************************
 * CryptEnumProvidersW (ADVAPI32.@)
 */
BOOL WINAPI CryptEnumProvidersW (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPWSTR pszProvName, DWORD *pcbProvName)
{
	PSTR str = NULL;
	DWORD strlen;
	BOOL ret; /* = FALSE; */

	TRACE("(%u, %p, %08x, %p, %p, %p)\n", dwIndex, pdwReserved, dwFlags,
			pdwProvType, pszProvName, pcbProvName);

	strlen = *pcbProvName / sizeof(WCHAR);
	if ( pszProvName && (str = CRYPT_Alloc(strlen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	ret = CryptEnumProvidersA(dwIndex, pdwReserved, dwFlags, pdwProvType, str, &strlen);
	if (str)
	{
		CRYPT_ANSIToUnicode(str, &pszProvName, *pcbProvName);
		CRYPT_Free(str);
	}
	*pcbProvName = strlen * sizeof(WCHAR);
	return ret;
}

/******************************************************************************
 * CryptEnumProviderTypesA (ADVAPI32.@)
 */
BOOL WINAPI CryptEnumProviderTypesA (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPSTR pszTypeName, DWORD *pcbTypeName)
{
	HKEY hKey, hSubkey;
	DWORD keylen, numkeys;
	PSTR keyname, ch;

	TRACE("(%u, %p, %08x, %p, %p, %p)\n", dwIndex, pdwReserved,
		dwFlags, pdwProvType, pszTypeName, pcbTypeName);

	if (pdwReserved || !pdwProvType || !pcbTypeName)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags) CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Cryptography\\Defaults\\Provider Types", &hKey))
		return FALSE;

	RegQueryInfoKeyA(hKey, NULL, NULL, NULL, &numkeys, &keylen, NULL, NULL, NULL, NULL, NULL, NULL);
	if (dwIndex >= numkeys)
		CRYPT_ReturnLastError(ERROR_NO_MORE_ITEMS);
	keylen++;
	if ( !(keyname = CRYPT_Alloc(keylen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if ( RegEnumKeyA(hKey, dwIndex, keyname, keylen) )
		return FALSE;
	RegOpenKeyA(hKey, keyname, &hSubkey);
	ch = keyname + strlen(keyname);
	/* Convert "Type 000" to 0, etc/ */
	*pdwProvType = *(--ch) - '0';
	*pdwProvType += (*(--ch) - '0') * 10;
	*pdwProvType += (*(--ch) - '0') * 100;
	CRYPT_Free(keyname);
	RegQueryValueA(hSubkey, "TypeName", pszTypeName, (LPLONG)pcbTypeName);
	RegCloseKey(hSubkey);
	RegCloseKey(hKey);
	return TRUE;
}

/******************************************************************************
 * CryptEnumProviderTypesW (ADVAPI32.@)
 */
BOOL WINAPI CryptEnumProviderTypesW (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPWSTR pszTypeName, DWORD *pcbTypeName)
{
	PSTR str = NULL;
	DWORD strlen;
	BOOL ret;

	TRACE("(%u, %p, %08x, %p, %p, %p)\n", dwIndex, pdwReserved, dwFlags,
			pdwProvType, pszTypeName, pcbTypeName);
	strlen = *pcbTypeName / sizeof(WCHAR);
	if ( pszTypeName && (str = CRYPT_Alloc(strlen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	ret = CryptEnumProviderTypesA(dwIndex, pdwReserved, dwFlags, pdwProvType, str, &strlen);
	if (str)
	{
		CRYPT_ANSIToUnicode(str, &pszTypeName, *pcbTypeName);
		CRYPT_Free(str);
	}
	*pcbTypeName = strlen * sizeof(WCHAR);
	return ret;
}

/******************************************************************************
 * CryptExportKey (ADVAPI32.@)
 */
BOOL WINAPI CryptExportKey (HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType,
		DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey, expkey = (PCRYPTKEY)hExpKey;

	TRACE("(0x%lx, 0x%lx, %u, %08x, %p, %p)\n", hKey, hExpKey, dwBlobType, dwFlags, pbData, pdwDataLen);

	if (!key || !pdwDataLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPExportKey(prov->hPrivate, key->hPrivate, expkey ? expkey->hPrivate : 0,
			dwBlobType, dwFlags, pbData, pdwDataLen);
}

/******************************************************************************
 * CryptGenKey (ADVAPI32.@)
 */
BOOL WINAPI CryptGenKey (HCRYPTPROV hProv, ALG_ID Algid, DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY key;

	TRACE("(0x%lx, %d, %08x, %p)\n", hProv, Algid, dwFlags, phKey);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!phKey)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if ( !(key = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	key->pProvider = prov;

	*phKey = (HCRYPTKEY)key;
	if (prov->pFuncs->pCPGenKey(prov->hPrivate, Algid, dwFlags, &key->hPrivate))
		return TRUE;

	/* CSP error! */
	CRYPT_Free(key);
	return FALSE;
}

/******************************************************************************
 * CryptGetDefaultProviderA (ADVAPI32.@)
 */
BOOL WINAPI CryptGetDefaultProviderA (DWORD dwProvType, DWORD *pdwReserved,
		DWORD dwFlags, LPSTR pszProvName, DWORD *pcbProvName)
{
	HKEY hKey;
	PSTR keyname;

	if (pdwReserved || !pcbProvName)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags & ~(CRYPT_USER_DEFAULT | CRYPT_MACHINE_DEFAULT))
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);
	if (dwProvType > MAXPROVTYPES)
		CRYPT_ReturnLastError(NTE_BAD_PROV_TYPE);
	if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, dwFlags & CRYPT_USER_DEFAULT)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if (RegOpenKeyA((dwFlags & CRYPT_USER_DEFAULT) ?  HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE ,keyname, &hKey))
	{
		CRYPT_Free(keyname);
		CRYPT_ReturnLastError(NTE_PROV_TYPE_NOT_DEF);
	}
	CRYPT_Free(keyname);
	if (RegQueryValueExA(hKey, "Name", NULL, NULL, (LPBYTE)pszProvName, (LPDWORD)pcbProvName))
	{
		if (GetLastError() != ERROR_MORE_DATA)
			SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		return FALSE;
	}
	RegCloseKey(hKey);
	return TRUE;
}

/******************************************************************************
 * CryptGetDefaultProviderW (ADVAPI32.@)
 */
BOOL WINAPI CryptGetDefaultProviderW (DWORD dwProvType, DWORD *pdwReserved,
		DWORD dwFlags, LPWSTR pszProvName, DWORD *pcbProvName)
{
	PSTR str = NULL;
	DWORD strlen;
	BOOL ret = FALSE;

	TRACE("(%u, %p, %08x, %p, %p)\n", dwProvType, pdwReserved, dwFlags, pszProvName, pcbProvName);

	strlen = *pcbProvName / sizeof(WCHAR);
	if ( pszProvName && !(str = CRYPT_Alloc(strlen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	ret = CryptGetDefaultProviderA(dwProvType, pdwReserved, dwFlags, str, &strlen);
	if (str)
	{
		CRYPT_ANSIToUnicode(str, &pszProvName, *pcbProvName);
		CRYPT_Free(str);
	}
	*pcbProvName = strlen * sizeof(WCHAR);
	return ret;
}

/******************************************************************************
 * CryptGetHashParam (ADVAPI32.@)
 */
BOOL WINAPI CryptGetHashParam (HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, %u, %p, %p, %08x)\n", hHash, dwParam, pbData, pdwDataLen, dwFlags);

	if (!hash || !pdwDataLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPGetHashParam(prov->hPrivate, hash->hPrivate, dwParam,
			pbData, pdwDataLen, dwFlags);
}

/******************************************************************************
 * CryptGetKeyParam (ADVAPI32.@)
 */
BOOL WINAPI CryptGetKeyParam (HCRYPTKEY hKey, DWORD dwParam, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;

	TRACE("(0x%lx, %u, %p, %p, %08x)\n", hKey, dwParam, pbData, pdwDataLen, dwFlags);

	if (!key || !pdwDataLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPGetKeyParam(prov->hPrivate, key->hPrivate, dwParam,
			pbData, pdwDataLen, dwFlags);
}

/******************************************************************************
 * CryptGetProvParam (ADVAPI32.@)
 */
BOOL WINAPI CryptGetProvParam (HCRYPTPROV hProv, DWORD dwParam, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwFlags)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %u, %p, %p, %08x)\n", hProv, dwParam, pbData, pdwDataLen, dwFlags);

	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPGetProvParam(prov->hPrivate, dwParam, pbData, pdwDataLen, dwFlags);
}

/******************************************************************************
 * CryptGetUserKey (ADVAPI32.@)
 */
BOOL WINAPI CryptGetUserKey (HCRYPTPROV hProv, DWORD dwKeySpec, HCRYPTKEY *phUserKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY key;

	TRACE("(0x%lx, %u, %p)\n", hProv, dwKeySpec, phUserKey);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!phUserKey)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	*phUserKey = 0;
	if ( !(key = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	key->pProvider = prov;

	if (prov->pFuncs->pCPGetUserKey(prov->hPrivate, dwKeySpec, &key->hPrivate))
	{
		*phUserKey = (HCRYPTKEY)key;
		return TRUE;
	}

	/* CSP Error */
	CRYPT_Free(key);
	return FALSE;
}

/******************************************************************************
 * CryptHashData (ADVAPI32.@)
 */
BOOL WINAPI CryptHashData (HCRYPTHASH hHash, const BYTE *pbData, DWORD dwDataLen, DWORD dwFlags)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTPROV prov;

	TRACE("(0x%lx, %p, %u, %08x)\n", hHash, pbData, dwDataLen, dwFlags);

	if (!hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!pbData || !dwDataLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPHashData(prov->hPrivate, hash->hPrivate, pbData, dwDataLen, dwFlags);
}

/******************************************************************************
 * CryptHashSessionKey (ADVAPI32.@)
 */
BOOL WINAPI CryptHashSessionKey (HCRYPTHASH hHash, HCRYPTKEY hKey, DWORD dwFlags)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTPROV prov;

	TRACE("(0x%lx, 0x%lx, %08x)\n", hHash, hKey, dwFlags);

	if (!hash || !key)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPHashSessionKey(prov->hPrivate, hash->hPrivate, key->hPrivate, dwFlags);
}

/******************************************************************************
 * CryptImportKey (ADVAPI32.@)
 */
WINADVAPI BOOL WINAPI CryptImportKey (HCRYPTPROV hProv, CONST BYTE *pbData, DWORD dwDataLen,
		HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY pubkey = (PCRYPTKEY)hPubKey, importkey;

	TRACE("(0x%lx, %p, %u, 0x%lx, %08x, %p)\n", hProv, pbData, dwDataLen, hPubKey, dwFlags, phKey);

	if (!prov || !pbData || !dwDataLen || !phKey)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (!is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	*phKey = 0;
	if ( !(importkey = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	importkey->pProvider = prov;
	if (prov->pFuncs->pCPImportKey(prov->hPrivate, pbData, dwDataLen,
			pubkey ? pubkey->hPrivate : 0, dwFlags, &importkey->hPrivate))
	{
		*phKey = (HCRYPTKEY)importkey;
		return TRUE;
	}

	CRYPT_Free(importkey);
	return FALSE;
}

/******************************************************************************
 * CryptSignHashA
 *
 * Note: Since the sDesciption (string) is supposed to be NULL and
 *	is only retained for compatibility no string conversions are required
 *	and only one implementation is required for both ANSI and Unicode.
 *	We still need to export both:
 *
 * CryptSignHashA (ADVAPI32.@)
 * CryptSignHashW (ADVAPI32.@)
 */
BOOL WINAPI CryptSignHashA (HCRYPTHASH hHash, DWORD dwKeySpec, LPCSTR sDescription,
		DWORD dwFlags, BYTE *pbSignature, DWORD *pdwSigLen)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTPROV prov;

	TRACE("(0x%lx, %u, %08x, %p, %p)\n", hHash, dwKeySpec, dwFlags, pbSignature, pdwSigLen);
	if (sDescription)
		WARN("The sDescription parameter is not supported (and no longer used).  Ignoring.");

	if (!hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!pdwSigLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPSignHash(prov->hPrivate, hash->hPrivate, dwKeySpec, NULL,
		dwFlags, pbSignature, pdwSigLen);
}

/******************************************************************************
 * CryptSetHashParam (ADVAPI32.@)
 */
WINADVAPI BOOL WINAPI CryptSetHashParam (HCRYPTHASH hHash, DWORD dwParam, CONST BYTE *pbData, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, %u, %p, %08x)\n", hHash, dwParam, pbData, dwFlags);

	if (!hash || !pbData)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPSetHashParam(prov->hPrivate, hash->hPrivate,
			dwParam, pbData, dwFlags);
}

/******************************************************************************
 * CryptSetKeyParam (ADVAPI32.@)
 */
WINADVAPI BOOL WINAPI CryptSetKeyParam (HCRYPTKEY hKey, DWORD dwParam, CONST BYTE *pbData, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;

	TRACE("(0x%lx, %u, %p, %08x)\n", hKey, dwParam, pbData, dwFlags);

	if (!key || !pbData)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPSetKeyParam(prov->hPrivate, key->hPrivate,
			dwParam, pbData, dwFlags);
}

/******************************************************************************
 * CryptSetProviderA (ADVAPI32.@)
 */
BOOL WINAPI CryptSetProviderA (LPCSTR pszProvName, DWORD dwProvType)
{
	TRACE("(%s, %u)\n", pszProvName, dwProvType);
	return CryptSetProviderExA(pszProvName, dwProvType, NULL, CRYPT_USER_DEFAULT);
}

/******************************************************************************
 * CryptSetProviderW (ADVAPI32.@)
 */
BOOL WINAPI CryptSetProviderW (LPCWSTR pszProvName, DWORD dwProvType)
{
	TRACE("(%s, %u)\n", debugstr_w(pszProvName), dwProvType);
	return CryptSetProviderExW(pszProvName, dwProvType, NULL, CRYPT_USER_DEFAULT);
}

/******************************************************************************
 * CryptSetProviderExA (ADVAPI32.@)
 */
BOOL WINAPI CryptSetProviderExA (LPCSTR pszProvName, DWORD dwProvType, DWORD *pdwReserved, DWORD dwFlags)
{
	HKEY hKey;
	PSTR keyname;

	TRACE("(%s, %u, %p, %08x)\n", pszProvName, dwProvType, pdwReserved, dwFlags);

	if (!pszProvName || pdwReserved)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwProvType > MAXPROVTYPES)
		CRYPT_ReturnLastError(NTE_BAD_PROV_TYPE);
	if (dwFlags & ~(CRYPT_MACHINE_DEFAULT | CRYPT_USER_DEFAULT | CRYPT_DELETE_DEFAULT)
			|| dwFlags == CRYPT_DELETE_DEFAULT)
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	if (dwFlags & CRYPT_DELETE_DEFAULT)
	{
		if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, dwFlags & CRYPT_USER_DEFAULT)) )
			CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
		RegDeleteKeyA( (dwFlags & CRYPT_USER_DEFAULT) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, keyname);
		CRYPT_Free(keyname);
		return TRUE;
	}

	if ( !(keyname = CRYPT_GetProvKeyName(pszProvName)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, keyname, &hKey))
	{
		CRYPT_Free(keyname);
		CRYPT_ReturnLastError(NTE_BAD_PROVIDER);
	}
	CRYPT_Free(keyname);
	RegCloseKey(hKey);
	if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, dwFlags & CRYPT_USER_DEFAULT)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	RegCreateKeyA( (dwFlags & CRYPT_USER_DEFAULT) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, keyname, &hKey);
	CRYPT_Free(keyname);
	if (RegSetValueExA(hKey, "Name", 0, REG_SZ, (LPBYTE)pszProvName, strlen(pszProvName) +1))
		return FALSE;
	return TRUE;
}

/******************************************************************************
 * CryptSetProviderExW (ADVAPI32.@)
 */
BOOL WINAPI CryptSetProviderExW (LPCWSTR pszProvName, DWORD dwProvType, DWORD *pdwReserved, DWORD dwFlags)
{
	BOOL ret = FALSE;
	PSTR str = NULL;

	TRACE("(%s, %u, %p, %08x)\n", debugstr_w(pszProvName), dwProvType, pdwReserved, dwFlags);

	if (CRYPT_UnicodeToANSI(pszProvName, &str, -1))
	{
		ret = CryptSetProviderExA(str, dwProvType, pdwReserved, dwFlags);
		CRYPT_Free(str);
	}
	return ret;
}

/******************************************************************************
 * CryptSetProvParam (ADVAPI32.@)
 */
WINADVAPI BOOL WINAPI CryptSetProvParam (HCRYPTPROV hProv, DWORD dwParam, CONST BYTE *pbData, DWORD dwFlags)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %u, %p, %08x)\n", hProv, dwParam, pbData, dwFlags);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags & PP_USE_HARDWARE_RNG)
	{
		FIXME("PP_USE_HARDWARE_RNG: What do I do with this?\n");
		FIXME("\tLetting the CSP decide.\n");
	}
	if (dwFlags & PP_CLIENT_HWND)
	{
		/* FIXME: Should verify the parameter */
		if (pbData /* && IsWindow((HWND)pbData) */)
		{
			crypt_hWindow = (HWND)(pbData);
			return TRUE;
		} else {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
	}
	/* All other flags go to the CSP */
	return prov->pFuncs->pCPSetProvParam(prov->hPrivate, dwParam, pbData, dwFlags);
}

/******************************************************************************
 * CryptVerifySignatureA
 *
 * Note: Since the sDesciption (string) is supposed to be NULL and
 *	is only retained for compatibility no string conversions are required
 *	and only one implementation is required for both ANSI and Unicode.
 *	We still need to export both:
 *
 * CryptVerifySignatureA (ADVAPI32.@)
 * CryptVerifySignatureW (ADVAPI32.@)
 */
WINADVAPI BOOL WINAPI CryptVerifySignatureA (HCRYPTHASH hHash, CONST BYTE *pbSignature, DWORD dwSigLen,
		HCRYPTKEY hPubKey, LPCSTR sDescription, DWORD dwFlags)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTKEY key = (PCRYPTKEY)hPubKey;
	PCRYPTPROV prov;

	TRACE("(0x%lx, %p, %u, 0x%lx, %08x)\n", hHash, pbSignature,
			dwSigLen, hPubKey, dwFlags);
	if (sDescription)
		WARN("The sDescription parameter is not supported (and no longer used).  Ignoring.");

	if (!hash || !key)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!pbSignature || !dwSigLen)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	if (!prov || !is_valid_csp_handle(prov))
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPVerifySignature(prov->hPrivate, hash->hPrivate, pbSignature, dwSigLen,
		key->hPrivate, NULL, dwFlags);
}

/******************************************************************************
 * RtlGenRandom
 *
 *
 * RtlGenRandom (ADVAPI32.@)
 */
BOOL WINAPI RtlGenRandom(BYTE * pbBuffer, DWORD dwLen)
{
#if defined(linux) || defined(__APPLE__)
    int random_file;

    random_file = open("/dev/urandom", O_RDONLY);
    if (random_file != -1)
    {
        read(random_file, pbBuffer, dwLen);
        close(random_file);
    }
    else
#endif
    /* On non-linux platforms, or if /dev/urandom isn't available, 
       revert to bad bad random numbers */
    {
        int i;

        WARN("this isn't very 'secure'\n");
        srand( (unsigned)time( NULL ) );
        for (i = 0; i < dwLen; i++)
                pbBuffer[i] = (unsigned char)(rand() % 255);

    }
    return TRUE;
}
