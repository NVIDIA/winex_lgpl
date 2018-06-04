/*
 * dlls/advapi32/security.c
 *  FIXME: for all functions thunking down to Rtl* functions:  implement SetLastError()
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2005-2015 NVIDIA CORPORATION. All rights reserved.
 */
#include <string.h>

#include "windef.h"
#include "winerror.h"
#include "wine/heapstr.h"
#include "winternl.h"
#include "ntsecapi.h"
#include "accctrl.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(advapi);


static int RunAsAdmin = -1;


static const WCHAR SE_CREATE_TOKEN_NAME_W[] =
   {'S','e','C','r','e','a','t','e','T','o','k','e','n','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_ASSIGNPRIMARYTOKEN_NAME_W[] =
   {'S','e','A','s','s','i','g','n','P','r','i','m','a','r','y','T','o','k','e','n','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_LOCK_MEMORY_NAME_W[] =
   {'S','e','L','o','c','k','M','e','m','o','r','y','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_INCREASE_QUOTA_NAME_W[] =
   {'S','e','I','n','c','r','e','a','s','e','Q','u','o','t','a','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_MACHINE_ACCOUNT_NAME_W[] =
   {'S','e','M','a','c','h','i','n','e','A','c','c','o','u','n','t','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_TCB_NAME_W[] =
   {'S','e','T','c','b','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_SECURITY_NAME_W[] =
   {'S','e','S','e','c','u','r','i','t','y','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_TAKE_OWNERSHIP_NAME_W[] =
   {'S','e','T','a','k','e','O','w','n','e','r','s','h','i','p','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_LOAD_DRIVER_NAME_W[] =
   {'S','e','L','o','a','d','D','r','i','v','e','r','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_SYSTEM_PROFILE_NAME_W[] =
   {'S','e','S','y','s','t','e','m','P','r','o','f','i','l','e','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_SYSTEMTIME_NAME_W[] =
   {'S','e','S','y','s','t','e','m','t','i','m','e','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_PROF_SINGLE_PROCESS_NAME_W[] =
   {'S','e','P','r','o','f','i','l','e','S','i','n','g','l','e','P','r','o','c','e','s','s','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_INC_BASE_PRIORITY_NAME_W[] =
   {'S','e','I','n','c','r','e','a','s','e','B','a','s','e','P','r','i','o','r','i','t','y','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_CREATE_PAGEFILE_NAME_W[] =
   {'S','e','C','r','e','a','t','e','P','a','g','e','f','i','l','e','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_CREATE_PERMANENT_NAME_W[] =
   {'S','e','C','r','e','a','t','e','P','e','r','m','a','n','e','n','t','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_BACKUP_NAME_W[] =
   {'S','e','B','a','c','k','u','p','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_RESTORE_NAME_W[] =
   {'S','e','R','e','s','t','o','r','e','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_SHUTDOWN_NAME_W[] =
   {'S','e','S','h','u','t','d','o','w','n','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_DEBUG_NAME_W[] =
   {'S','e','D','e','b','u','g','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_AUDIT_NAME_W[] =
   {'S','e','A','u','d','i','t','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_SYSTEM_ENVIRONMENT_NAME_W[] =
   {'S','e','S','y','s','t','e','m','E','n','v','i','r','o','n','m','e','n','t','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_CHANGE_NOTIFY_NAME_W[] =
   {'S','e','C','h','a','n','g','e','N','o','t','i','f','y','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_REMOTE_SHUTDOWN_NAME_W[] =
   {'S','e','R','e','m','o','t','e','S','h','u','t','d','o','w','n','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_UNDOCK_NAME_W[] =
   {'S','e','U','n','d','o','c','k','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_SYNC_AGENT_NAME_W[] =
   {'S','e','S','y','n','c','A','g','e','n','t','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_ENABLE_DELEGATION_NAME_W[] =
   {'S','e','E','n','a','b','l','e','D','e','l','e','g','a','t','i','o','n','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_MANAGE_VOLUME_NAME_W[] =
   {'S','e','M','a','n','a','g','e','V','o','l','u','m','e','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_IMPERSONATE_NAME_W[] =
   {'S','e','I','m','p','e','r','s','o','n','a','t','e','P','r','i','v','i','l','e','g','e',0};
static const WCHAR SE_CREATE_GLOBAL_NAME_W[] =
   {'S','e','C','r','e','a','t','e','G','l','o','b','a','l','P','r','i','v','i','l','e','g','e',0};

static const WCHAR *const SePrivNames[SE_MAX_WELL_KNOWN_PRIVILEGE + 1] =
{
   NULL,
   NULL,
   SE_CREATE_TOKEN_NAME_W,
   SE_ASSIGNPRIMARYTOKEN_NAME_W,
   SE_LOCK_MEMORY_NAME_W,
   SE_INCREASE_QUOTA_NAME_W,
   SE_MACHINE_ACCOUNT_NAME_W,
   SE_TCB_NAME_W,
   SE_SECURITY_NAME_W,
   SE_TAKE_OWNERSHIP_NAME_W,
   SE_LOAD_DRIVER_NAME_W,
   SE_SYSTEM_PROFILE_NAME_W,
   SE_SYSTEMTIME_NAME_W,
   SE_PROF_SINGLE_PROCESS_NAME_W,
   SE_INC_BASE_PRIORITY_NAME_W,
   SE_CREATE_PAGEFILE_NAME_W,
   SE_CREATE_PERMANENT_NAME_W,
   SE_BACKUP_NAME_W,
   SE_RESTORE_NAME_W,
   SE_SHUTDOWN_NAME_W,
   SE_DEBUG_NAME_W,
   SE_AUDIT_NAME_W,
   SE_SYSTEM_ENVIRONMENT_NAME_W,
   SE_CHANGE_NOTIFY_NAME_W,
   SE_REMOTE_SHUTDOWN_NAME_W,
   SE_UNDOCK_NAME_W,
   SE_SYNC_AGENT_NAME_W,
   SE_ENABLE_DELEGATION_NAME_W,
   SE_MANAGE_VOLUME_NAME_W,
   SE_IMPERSONATE_NAME_W,
   SE_CREATE_GLOBAL_NAME_W,
};


#define CallWin32ToNt(func) \
	{ NTSTATUS ret; \
	  ret = (func); \
	  if (ret !=STATUS_SUCCESS) \
	  { SetLastError (RtlNtStatusToDosError(ret)); return FALSE; } \
	  return TRUE; \
	}

static void dumpLsaAttributes( PLSA_OBJECT_ATTRIBUTES oa )
{
	if (oa)
	{
	  TRACE("\n\tlength=%u, rootdir=0x%08x, objectname=%s\n\tattr=0x%08x, sid=%p qos=%p\n",
		oa->Length, oa->RootDirectory,
		oa->ObjectName?debugstr_w(oa->ObjectName->Buffer):"null",
     		oa->Attributes, oa->SecurityDescriptor, oa->SecurityQualityOfService);
	}
}

/*	##############################
	######	TOKEN FUNCTIONS ######
	##############################
*/

/******************************************************************************
 * OpenProcessToken			[ADVAPI32.@]
 * Opens the access token associated with a process
 *
 * PARAMS
 *   ProcessHandle [I] Handle to process
 *   DesiredAccess [I] Desired access to process
 *   TokenHandle   [O] Pointer to handle of open access token
 *
 * RETURNS STD
 */
BOOL WINAPI
OpenProcessToken( HANDLE ProcessHandle, DWORD DesiredAccess,
                  HANDLE *TokenHandle )
{
	CallWin32ToNt(NtOpenProcessToken( ProcessHandle, DesiredAccess, TokenHandle ));
}

/******************************************************************************
 * OpenThreadToken [ADVAPI32.@]
 *
 * PARAMS
 *   thread        []
 *   desiredaccess []
 *   openasself    []
 *   thandle       []
 */
BOOL WINAPI
OpenThreadToken( HANDLE ThreadHandle, DWORD DesiredAccess,
		 BOOL OpenAsSelf, HANDLE *TokenHandle)
{
	CallWin32ToNt (NtOpenThreadToken(ThreadHandle, DesiredAccess, OpenAsSelf, TokenHandle));
}

/******************************************************************************
 * AdjustTokenPrivileges [ADVAPI32.@]
 *
 * PARAMS
 *   TokenHandle          []
 *   DisableAllPrivileges []
 *   NewState             []
 *   BufferLength         []
 *   PreviousState        []
 *   ReturnLength         []
 */
BOOL WINAPI
AdjustTokenPrivileges( HANDLE TokenHandle, BOOL DisableAllPrivileges,
                       LPVOID NewState, DWORD BufferLength,
                       LPVOID PreviousState, LPDWORD ReturnLength )
{
	SetLastError(ERROR_SUCCESS);
	CallWin32ToNt(NtAdjustPrivilegesToken(TokenHandle, DisableAllPrivileges, NewState, BufferLength, PreviousState, ReturnLength));
}

/*******************************************************************************
 * DuplicateToken [ADVAPI32.@]
 *
 * PARAMS
 *   ExistingTokenHandle   []
 *   ImpersonationLevel    []
 *   DuplicateTokenHandle  []
 */
BOOL WINAPI DuplicateToken(
  HANDLE ExistingTokenHandle,
  SECURITY_IMPERSONATION_LEVEL ImpersonationLevel,
  PHANDLE DuplicateTokenHandle
) {
  FIXME("(0x%08x %d %p) stub!\n", ExistingTokenHandle, ImpersonationLevel, DuplicateTokenHandle);

  return TRUE;
}

/******************************************************************************
 * CheckTokenMembership [ADVAPI32.@]
 *
 * PARAMS
 *   TokenHandle []
 *   SidToCheck  []
 *   IsMember    []
 */
BOOL WINAPI
CheckTokenMembership( HANDLE TokenHandle, PSID SidToCheck,
                      PBOOL IsMember )
{
   FIXME("(0x%08x %p %p) stub!\n", TokenHandle, SidToCheck, IsMember);

   if (RunAsAdmin == -1)
   {
      HKEY hKey = 0;
      char buf[16];
      DWORD size = sizeof (buf);

      RunAsAdmin = 1;

      if (!RegCreateKeyExA (HKEY_LOCAL_MACHINE,
                            "Software\\Wine\\Wine\\Config\\wine", 0, NULL,
                            REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL,
                            &hKey, NULL) &&
          !RegQueryValueExA (hKey, "NotAdministrator", 0, NULL, (LPBYTE)buf,
                             &size) && size)
      {
         if ((buf[0] == 'y') || (buf[0] == 'Y') || (buf[0] == 't') ||
             (buf[0] == 'T') || (buf[0] == '1'))
            RunAsAdmin = 0;
      }

      if (hKey)
         RegCloseKey (hKey);
   }

   *IsMember = RunAsAdmin;

   return TRUE;
}

/******************************************************************************
 * GetTokenInformation [ADVAPI32.@]
 *
 * PARAMS
 *   token           []
 *   tokeninfoclass  []
 *   tokeninfo       []
 *   tokeninfolength []
 *   retlen          []
 *
 */
BOOL WINAPI
GetTokenInformation( HANDLE token, TOKEN_INFORMATION_CLASS tokeninfoclass,
		     LPVOID tokeninfo, DWORD tokeninfolength, LPDWORD retlen )
{
	CallWin32ToNt (NtQueryInformationToken( token, tokeninfoclass, tokeninfo, tokeninfolength, retlen));
}

/*************************************************************************
 * SetThreadToken [ADVAPI32.@]
 *
 * Assigns an "impersonation token" to a thread so it can assume the
 * security privledges of another thread or process.  Can also remove
 * a previously assigned token.  Only supported on NT - it's a stub
 * exactly like this one on Win9X.
 *
 */

BOOL WINAPI SetThreadToken(PHANDLE thread, HANDLE token)
{
    FIXME("(%p, %x): stub (NT impl. only)\n", thread, token);

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return FALSE;
}

/**************************************************************************
 * IsTokenRestricted
 *
 * Indicates whether a token contains a list of restricted SIDs
 */
BOOL WINAPI IsTokenRestricted(HANDLE token)
{
    FIXME("(%x): stub!\n", token);

    /* SetLastError(ERROR_CALL_NOT_IMPLEMENTED); */
    
    return FALSE;
}

/*	##############################
	######	SID FUNCTIONS	######
	##############################
*/

/******************************************************************************
 * AllocateAndInitializeSid [ADVAPI32.@]
 *
 * PARAMS
 *   pIdentifierAuthority []
 *   nSubAuthorityCount   []
 *   nSubAuthority0       []
 *   nSubAuthority1       []
 *   nSubAuthority2       []
 *   nSubAuthority3       []
 *   nSubAuthority4       []
 *   nSubAuthority5       []
 *   nSubAuthority6       []
 *   nSubAuthority7       []
 *   pSid                 []
 */
BOOL WINAPI
AllocateAndInitializeSid( PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
                          BYTE nSubAuthorityCount,
                          DWORD nSubAuthority0, DWORD nSubAuthority1,
                          DWORD nSubAuthority2, DWORD nSubAuthority3,
                          DWORD nSubAuthority4, DWORD nSubAuthority5,
                          DWORD nSubAuthority6, DWORD nSubAuthority7,
                          PSID *pSid )
{
	CallWin32ToNt (RtlAllocateAndInitializeSid(
		pIdentifierAuthority, nSubAuthorityCount,
		nSubAuthority0, nSubAuthority1,	nSubAuthority2, nSubAuthority3,
		nSubAuthority4, nSubAuthority5, nSubAuthority6, nSubAuthority7,
		pSid ));
}

/******************************************************************************
 * FreeSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
PVOID WINAPI
FreeSid( PSID pSid )
{
    	RtlFreeSid(pSid);
	return NULL; /* is documented like this */
}

/******************************************************************************
 * CopySid [ADVAPI32.@]
 *
 * PARAMS
 *   nDestinationSidLength []
 *   pDestinationSid       []
 *   pSourceSid            []
 */
BOOL WINAPI
CopySid( DWORD nDestinationSidLength, PSID pDestinationSid, PSID pSourceSid )
{
	return RtlCopySid(nDestinationSidLength, pDestinationSid, pSourceSid);
}

/******************************************************************************
 * IsValidSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
BOOL WINAPI
IsValidSid( PSID pSid )
{
	return RtlValidSid( pSid );
}

/******************************************************************************
 * EqualSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid1 []
 *   pSid2 []
 */
BOOL WINAPI
EqualSid( PSID pSid1, PSID pSid2 )
{
	return RtlEqualSid( pSid1, pSid2 );
}

/******************************************************************************
 * EqualPrefixSid [ADVAPI32.@]
 */
BOOL WINAPI EqualPrefixSid (PSID pSid1, PSID pSid2)
{
	return RtlEqualPrefixSid(pSid1, pSid2);
}

/******************************************************************************
 * GetSidLengthRequired [ADVAPI32.@]
 *
 * PARAMS
 *   nSubAuthorityCount []
 */
DWORD WINAPI
GetSidLengthRequired( BYTE nSubAuthorityCount )
{
	return RtlLengthRequiredSid(nSubAuthorityCount);
}

/******************************************************************************
 * InitializeSid [ADVAPI32.@]
 *
 * PARAMS
 *   pIdentifierAuthority []
 */
BOOL WINAPI
InitializeSid (
	PSID pSid,
	PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
	BYTE nSubAuthorityCount)
{
	return RtlInitializeSid(pSid, pIdentifierAuthority, nSubAuthorityCount);
}

/******************************************************************************
 * GetSidIdentifierAuthority [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
PSID_IDENTIFIER_AUTHORITY WINAPI
GetSidIdentifierAuthority( PSID pSid )
{
	return RtlIdentifierAuthoritySid(pSid);
}

/******************************************************************************
 * GetSidSubAuthority [ADVAPI32.@]
 *
 * PARAMS
 *   pSid          []
 *   nSubAuthority []
 */
PDWORD WINAPI
GetSidSubAuthority( PSID pSid, DWORD nSubAuthority )
{
	return RtlSubAuthoritySid(pSid, nSubAuthority);
}

/******************************************************************************
 * GetSidSubAuthorityCount [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
PUCHAR WINAPI
GetSidSubAuthorityCount (PSID pSid)
{
	return RtlSubAuthorityCountSid(pSid);
}

/******************************************************************************
 * GetLengthSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
DWORD WINAPI
GetLengthSid (PSID pSid)
{
	return RtlLengthSid(pSid);
}

/*	##############################################
	######	SECURITY DESCRIPTOR FUNCTIONS	######
	##############################################
*/

/******************************************************************************
 * InitializeSecurityDescriptor [ADVAPI32.@]
 *
 * PARAMS
 *   pDescr   []
 *   revision []
 */
BOOL WINAPI
InitializeSecurityDescriptor( SECURITY_DESCRIPTOR *pDescr, DWORD revision )
{
	CallWin32ToNt (RtlCreateSecurityDescriptor(pDescr, revision ));
}

/******************************************************************************
 * GetSecurityDescriptorLength [ADVAPI32.@]
 */
DWORD WINAPI GetSecurityDescriptorLength( SECURITY_DESCRIPTOR *pDescr)
{
	return (RtlLengthSecurityDescriptor(pDescr));
}

/******************************************************************************
 * GetSecurityDescriptorOwner [ADVAPI32.@]
 *
 * PARAMS
 *   pOwner            []
 *   lpbOwnerDefaulted []
 */
BOOL WINAPI
GetSecurityDescriptorOwner( SECURITY_DESCRIPTOR *pDescr, PSID *pOwner,
			    LPBOOL lpbOwnerDefaulted )
{
	CallWin32ToNt (RtlGetOwnerSecurityDescriptor( pDescr, pOwner, (PBOOLEAN)lpbOwnerDefaulted ));
}

/******************************************************************************
 * SetSecurityDescriptorOwner [ADVAPI32.@]
 *
 * PARAMS
 */
BOOL WINAPI SetSecurityDescriptorOwner( PSECURITY_DESCRIPTOR pSecurityDescriptor,
				   PSID pOwner, BOOL bOwnerDefaulted)
{
	CallWin32ToNt (RtlSetOwnerSecurityDescriptor(pSecurityDescriptor, pOwner, bOwnerDefaulted));
}
/******************************************************************************
 * GetSecurityDescriptorGroup			[ADVAPI32.@]
 */
BOOL WINAPI GetSecurityDescriptorGroup(
	PSECURITY_DESCRIPTOR SecurityDescriptor,
	PSID *Group,
	LPBOOL GroupDefaulted)
{
	CallWin32ToNt (RtlGetGroupSecurityDescriptor(SecurityDescriptor, Group, (PBOOLEAN)GroupDefaulted));
}
/******************************************************************************
 * SetSecurityDescriptorGroup [ADVAPI32.@]
 */
BOOL WINAPI SetSecurityDescriptorGroup ( PSECURITY_DESCRIPTOR SecurityDescriptor,
					   PSID Group, BOOL GroupDefaulted)
{
	CallWin32ToNt (RtlSetGroupSecurityDescriptor( SecurityDescriptor, Group, GroupDefaulted));
}

/******************************************************************************
 * IsValidSecurityDescriptor [ADVAPI32.@]
 *
 * PARAMS
 *   lpsecdesc []
 */
BOOL WINAPI
IsValidSecurityDescriptor( PSECURITY_DESCRIPTOR SecurityDescriptor )
{
	CallWin32ToNt (RtlValidSecurityDescriptor(SecurityDescriptor));
}

/******************************************************************************
 *  GetSecurityDescriptorDacl			[ADVAPI32.@]
 */
BOOL WINAPI GetSecurityDescriptorDacl(
	IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
	OUT LPBOOL lpbDaclPresent,
	OUT PACL *pDacl,
	OUT LPBOOL lpbDaclDefaulted)
{
	CallWin32ToNt (RtlGetDaclSecurityDescriptor(pSecurityDescriptor, (PBOOLEAN)lpbDaclPresent,
					       pDacl, (PBOOLEAN)lpbDaclDefaulted));
}

/******************************************************************************
 *  SetSecurityDescriptorDacl			[ADVAPI32.@]
 */
BOOL WINAPI
SetSecurityDescriptorDacl (
	PSECURITY_DESCRIPTOR lpsd,
	BOOL daclpresent,
	PACL dacl,
	BOOL dacldefaulted )
{
	CallWin32ToNt (RtlSetDaclSecurityDescriptor (lpsd, daclpresent, dacl, dacldefaulted ));
}
/******************************************************************************
 *  GetSecurityDescriptorSacl			[ADVAPI32.@]
 */
BOOL WINAPI GetSecurityDescriptorSacl(
	IN PSECURITY_DESCRIPTOR lpsd,
	OUT LPBOOL lpbSaclPresent,
	OUT PACL *pSacl,
	OUT LPBOOL lpbSaclDefaulted)
{
	CallWin32ToNt (RtlGetSaclSecurityDescriptor(lpsd,
	   (PBOOLEAN)lpbSaclPresent, pSacl, (PBOOLEAN)lpbSaclDefaulted));
}

/**************************************************************************
 * SetSecurityDescriptorSacl			[ADVAPI32.@]
 */
BOOL WINAPI SetSecurityDescriptorSacl (
	PSECURITY_DESCRIPTOR lpsd,
	BOOL saclpresent,
	PACL lpsacl,
	BOOL sacldefaulted)
{
	CallWin32ToNt (RtlSetSaclSecurityDescriptor(lpsd, saclpresent, lpsacl, sacldefaulted));
}

/**************************************************************************
 * SetSecurityInfo                              [ADVAPI32.@]
 */
DWORD WINAPI SetSecurityInfo(
         IN HANDLE handle,
         IN SE_OBJECT_TYPE ObjectType,
         IN SECURITY_INFORMATION SecurityInfo,
         IN PSID psidOwner,
         IN PSID psidGroup,
         IN PACL pDacl,
         IN PACL pSacl)
{
	TRACE("(0x%08x %x %x %p %p %p %p): stub!\n",
              handle, ObjectType, SecurityInfo,
              psidOwner, psidGroup, pDacl, pSacl);
	return ERROR_SUCCESS;
}

/******************************************************************************
 * MakeSelfRelativeSD [ADVAPI32.@]
 *
 * PARAMS
 *   lpabssecdesc  []
 *   lpselfsecdesc []
 *   lpbuflen      []
 */
BOOL WINAPI
MakeSelfRelativeSD(
	IN PSECURITY_DESCRIPTOR pAbsoluteSecurityDescriptor,
	IN PSECURITY_DESCRIPTOR pSelfRelativeSecurityDescriptor,
	IN OUT LPDWORD lpdwBufferLength)
{
	CallWin32ToNt (RtlMakeSelfRelativeSD(pAbsoluteSecurityDescriptor,pSelfRelativeSecurityDescriptor, lpdwBufferLength));
}

/******************************************************************************
 * GetSecurityDescriptorControl			[ADVAPI32.@]
 */

BOOL WINAPI GetSecurityDescriptorControl ( PSECURITY_DESCRIPTOR  pSecurityDescriptor,
		 PSECURITY_DESCRIPTOR_CONTROL pControl, LPDWORD lpdwRevision)
{
	CallWin32ToNt (RtlGetControlSecurityDescriptor(pSecurityDescriptor,pControl,lpdwRevision));
}

/*	##############################
	######	ACL FUNCTIONS	######
	##############################
*/

/*************************************************************************
 * InitializeAcl [ADVAPI32.@]
 */
DWORD WINAPI InitializeAcl(PACL acl, DWORD size, DWORD rev)
{
	CallWin32ToNt (RtlCreateAcl(acl, size, rev));
}

/*	##############################
	######	MISC FUNCTIONS	######
	##############################
*/


/*************************************************************************
 * LookupPrivilegeNameW [ADVAPI32.@]
 */
BOOL WINAPI LookupPrivilegeNameW (LPCWSTR lpSystemName, PLUID lpLuid,
                                  LPWSTR lpName, LPDWORD cchName)
{
   INT Len;

   TRACE ("(%s, %p, %p, %p)\n", debugstr_w (lpSystemName), lpLuid,
          lpName, cchName);

   /* FIXME - check lpSystemName */

   if (lpLuid->HighPart ||
       (lpLuid->LowPart < SE_MIN_WELL_KNOWN_PRIVILEGE) ||
       (lpLuid->LowPart > SE_MAX_WELL_KNOWN_PRIVILEGE))
   {
      SetLastError (ERROR_NO_SUCH_PRIVILEGE);
      return FALSE;
   }

   Len = strlenW (SePrivNames[lpLuid->LowPart]);
   if (*cchName < (Len + 1))
   {
      *cchName = Len + 1;
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return FALSE;
   }

   strcpyW (lpName, SePrivNames[lpLuid->LowPart]);
   *cchName = Len;
   return TRUE;
}


/*************************************************************************
 * LookupPrivilegeNameA [ADVAPI32.@]
 */
BOOL WINAPI LookupPrivilegeNameA (LPCSTR lpSystemName, PLUID lpLuid,
                                  LPSTR lpName, LPDWORD cchName)
{
   LPWSTR lpSystemNameW = HEAP_strdupAtoW (GetProcessHeap (), 0, lpSystemName);
   LPWSTR lpNameW = NULL;
   BOOL Ret;

   TRACE ("(%s, %p, %p, %p)\n", lpSystemName, lpLuid, lpName, cchName);

   if (*cchName)
      lpNameW = HeapAlloc (GetProcessHeap (), 0, *cchName * sizeof (WCHAR));

   Ret = LookupPrivilegeNameW (lpSystemNameW, lpLuid, lpNameW, cchName);
   if (Ret)
      WideCharToMultiByte (CP_ACP, 0, lpNameW, -1, lpName, *cchName,
                           NULL, NULL);

   if (lpNameW)
      HeapFree (GetProcessHeap (), 0, lpNameW);
   HeapFree (GetProcessHeap (), 0, lpSystemNameW);

   return Ret;
}


/******************************************************************************
 * LookupPrivilegeValueW			[ADVAPI32.@]
 * Retrieves LUID used on a system to represent the privilege name.
 *
 * NOTES
 *   lpLuid should be PLUID
 *
 * PARAMS
 *   lpSystemName [I] Address of string specifying the system
 *   lpName       [I] Address of string specifying the privilege
 *   lpLuid       [I] Address of locally unique identifier
 *
 * RETURNS STD
 */
BOOL WINAPI
LookupPrivilegeValueW( LPCWSTR lpSystemName, LPCWSTR lpName, PLUID lpLuid )
{
   int i;

   TRACE ("(%s, %s, %p)\n", debugstr_w (lpSystemName), debugstr_w (lpName),
          lpLuid);

   /* FIXME - check lpSystemName */

   if (!lpName)
   {
      SetLastError (ERROR_NO_SUCH_PRIVILEGE);
      return FALSE;
   }

   for (i = SE_MIN_WELL_KNOWN_PRIVILEGE; i <= SE_MAX_WELL_KNOWN_PRIVILEGE; i++)
   {
      if (strcmpiW (SePrivNames[i], lpName))
         continue;

      lpLuid->HighPart = 0;
      lpLuid->LowPart = i;
      return TRUE;
   }

   SetLastError (ERROR_NO_SUCH_PRIVILEGE);
   return FALSE;
}

/******************************************************************************
 * LookupPrivilegeValueA			[ADVAPI32.@]
 */
BOOL WINAPI
LookupPrivilegeValueA( LPCSTR lpSystemName, LPCSTR lpName, PLUID lpLuid )
{
    LPWSTR lpSystemNameW = HEAP_strdupAtoW(GetProcessHeap(), 0, lpSystemName);
    LPWSTR lpNameW = HEAP_strdupAtoW(GetProcessHeap(), 0, lpName);
    BOOL ret;

    TRACE ("(%s, %s, %p)\n", lpSystemName, lpName, lpLuid);
    ret = LookupPrivilegeValueW( lpSystemNameW, lpNameW, lpLuid);
    HeapFree(GetProcessHeap(), 0, lpNameW);
    HeapFree(GetProcessHeap(), 0, lpSystemNameW);
    return ret;
}

/******************************************************************************
 * GetFileSecurityA [ADVAPI32.@]
 *
 * Obtains Specified information about the security of a file or directory
 * The information obtained is constrained by the callers access rights and
 * privileges
 */
BOOL WINAPI
GetFileSecurityA( LPCSTR lpFileName,
                    SECURITY_INFORMATION RequestedInformation,
                    PSECURITY_DESCRIPTOR pSecurityDescriptor,
                    DWORD nLength, LPDWORD lpnLengthNeeded )
{
  FIXME("(%s) : stub\n", debugstr_a(lpFileName));
  return TRUE;
}

/******************************************************************************
 * GetFileSecurityW [ADVAPI32.@]
 *
 * Obtains Specified information about the security of a file or directory
 * The information obtained is constrained by the callers access rights and
 * privileges
 *
 * PARAMS
 *   lpFileName           []
 *   RequestedInformation []
 *   pSecurityDescriptor  []
 *   nLength              []
 *   lpnLengthNeeded      []
 */
BOOL WINAPI
GetFileSecurityW( LPCWSTR lpFileName,
                    SECURITY_INFORMATION RequestedInformation,
                    PSECURITY_DESCRIPTOR pSecurityDescriptor,
                    DWORD nLength, LPDWORD lpnLengthNeeded )
{
  FIXME("(%s) : stub\n", debugstr_w(lpFileName) );
  return TRUE;
}


/******************************************************************************
 * LookupAccountSidA [ADVAPI32.@]
 */
BOOL WINAPI
LookupAccountSidA(
	IN LPCSTR system,
	IN PSID sid,
	OUT LPSTR account,
	IN OUT LPDWORD accountSize,
	OUT LPSTR domain,
	IN OUT LPDWORD domainSize,
	OUT PSID_NAME_USE name_use )
{
	static const char ac[] = "Administrator";
	static const char dm[] = "DOMAIN";
	FIXME("(%s,sid=%p,%p,%p(%u),%p,%p(%u),%p): semi-stub\n",
	      debugstr_a(system),sid,
	      account,accountSize,accountSize?*accountSize:0,
	      domain,domainSize,domainSize?*domainSize:0,
	      name_use);

	if (accountSize) *accountSize = strlen(ac)+1;
	if (account && (*accountSize > strlen(ac)))
	  strcpy(account, ac);

	if (domainSize) *domainSize = strlen(dm)+1;
	if (domain && (*domainSize > strlen(dm)))
	  strcpy(domain,dm);

	if (name_use) *name_use = SidTypeUser;
	return TRUE;
}

/******************************************************************************
 * LookupAccountSidW [ADVAPI32.@]
 *
 * PARAMS
 *   system      []
 *   sid         []
 *   account     []
 *   accountSize []
 *   domain      []
 *   domainSize  []
 *   name_use    []
 */
BOOL WINAPI
LookupAccountSidW(
	IN LPCWSTR system,
	IN PSID sid,
	OUT LPWSTR account,
	IN OUT LPDWORD accountSize,
	OUT LPWSTR domain,
	IN OUT LPDWORD domainSize,
	OUT PSID_NAME_USE name_use )
{
    static const WCHAR ac[] = {'A','d','m','i','n','i','s','t','r','a','t','o','r',0};
    static const WCHAR dm[] = {'D','O','M','A','I','N',0};
	FIXME("(%s,sid=%p,%p,%p(%u),%p,%p(%u),%p): semi-stub\n",
	      debugstr_w(system),sid,
	      account,accountSize,accountSize?*accountSize:0,
	      domain,domainSize,domainSize?*domainSize:0,
	      name_use);

	if (accountSize) *accountSize = strlenW(ac)+1;
	if (account && (*accountSize > strlenW(ac)))
            strcpyW(account, ac);

	if (domainSize) *domainSize = strlenW(dm)+1;
	if (domain && (*domainSize > strlenW(dm)))
            strcpyW(domain,dm);

	if (name_use) *name_use = SidTypeUser;
	return TRUE;
}

/******************************************************************************
 * SetFileSecurityA [ADVAPI32.@]
 * Sets the security of a file or directory
 */
BOOL WINAPI SetFileSecurityA( LPCSTR lpFileName,
                                SECURITY_INFORMATION RequestedInformation,
                                PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
  FIXME("(%s) : stub\n", debugstr_a(lpFileName));
  return TRUE;
}

/******************************************************************************
 * SetFileSecurityW [ADVAPI32.@]
 * Sets the security of a file or directory
 *
 * PARAMS
 *   lpFileName           []
 *   RequestedInformation []
 *   pSecurityDescriptor  []
 */
BOOL WINAPI
SetFileSecurityW( LPCWSTR lpFileName,
                    SECURITY_INFORMATION RequestedInformation,
                    PSECURITY_DESCRIPTOR pSecurityDescriptor )
{
  FIXME("(%s) : stub\n", debugstr_w(lpFileName) );
  return TRUE;
}

/******************************************************************************
 * QueryWindows31FilesMigration [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 */
BOOL WINAPI
QueryWindows31FilesMigration( DWORD x1 )
{
	FIXME("(%u):stub\n",x1);
	return TRUE;
}

/******************************************************************************
 * SynchronizeWindows31FilesAndWindowsNTRegistry [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 *   x2 []
 *   x3 []
 *   x4 []
 */
BOOL WINAPI
SynchronizeWindows31FilesAndWindowsNTRegistry( DWORD x1, DWORD x2, DWORD x3,
                                               DWORD x4 )
{
	FIXME("(0x%08x,0x%08x,0x%08x,0x%08x):stub\n",x1,x2,x3,x4);
	return TRUE;
}

/******************************************************************************
 * LsaOpenPolicy [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 *   x2 []
 *   x3 []
 *   x4 []
 */
NTSTATUS WINAPI
LsaOpenPolicy(
	IN PLSA_UNICODE_STRING SystemName,
	IN PLSA_OBJECT_ATTRIBUTES ObjectAttributes,
	IN ACCESS_MASK DesiredAccess,
	IN OUT PLSA_HANDLE PolicyHandle)
{
	FIXME("(%s,%p,0x%08x,%p):stub\n",
              SystemName?debugstr_w(SystemName->Buffer):"null",
	      ObjectAttributes, DesiredAccess, PolicyHandle);
	dumpLsaAttributes(ObjectAttributes);
	if(PolicyHandle) *PolicyHandle = (LSA_HANDLE)0xcafe;
	return TRUE;
}

/******************************************************************************
 * LsaQueryInformationPolicy [ADVAPI32.@]
 */
NTSTATUS WINAPI
LsaQueryInformationPolicy(
	IN LSA_HANDLE PolicyHandle,
        IN POLICY_INFORMATION_CLASS InformationClass,
	OUT PVOID *Buffer)
{
	FIXME("(%p,0x%08x,%p):stub\n",
              PolicyHandle, InformationClass, Buffer);

	if(!Buffer) return FALSE;
	switch (InformationClass)
	{
	  case PolicyAuditEventsInformation: /* 2 */
	    {
	      PPOLICY_AUDIT_EVENTS_INFO p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(POLICY_AUDIT_EVENTS_INFO));
	      p->AuditingMode = FALSE; /* no auditing */
	      *Buffer = p;
	    }
	    break;
	  case PolicyPrimaryDomainInformation: /* 3 */
	  case PolicyAccountDomainInformation: /* 5 */
	    {
	      struct di
	      { POLICY_PRIMARY_DOMAIN_INFO ppdi;
		SID sid;
	      };
	      SID_IDENTIFIER_AUTHORITY localSidAuthority = {SECURITY_NT_AUTHORITY};

	      struct di * xdi = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(xdi));
	      RtlInitUnicodeString(&(xdi->ppdi.Name), HEAP_strdupAtoW(GetProcessHeap(),0,"DOMAIN"));
	      xdi->ppdi.Sid = &(xdi->sid);
	      xdi->sid.Revision = SID_REVISION;
	      xdi->sid.SubAuthorityCount = 1;
	      xdi->sid.IdentifierAuthority = localSidAuthority;
	      xdi->sid.SubAuthority[0] = SECURITY_LOCAL_SYSTEM_RID;
	      *Buffer = xdi;
	    }
	    break;
	  case 	PolicyAuditLogInformation:
	  case 	PolicyPdAccountInformation:
	  case 	PolicyLsaServerRoleInformation:
	  case 	PolicyReplicaSourceInformation:
	  case 	PolicyDefaultQuotaInformation:
	  case 	PolicyModificationInformation:
	  case 	PolicyAuditFullSetInformation:
	  case 	PolicyAuditFullQueryInformation:
	  case 	PolicyDnsDomainInformation:
	    {
	      FIXME("category not implemented\n");
	      return FALSE;
	    }
	}
	return TRUE;
}

/******************************************************************************
 * LsaLookupSids [ADVAPI32.@]
 */
NTSTATUS WINAPI
LsaLookupSids(
	IN LSA_HANDLE PolicyHandle,
	IN ULONG Count,
	IN PSID *Sids,
	OUT PLSA_REFERENCED_DOMAIN_LIST *ReferencedDomains,
	OUT PLSA_TRANSLATED_NAME *Names )
{
	FIXME("%p %u %p %p %p\n",
	  PolicyHandle, Count, Sids, ReferencedDomains, Names);
	return FALSE;
}

/******************************************************************************
 * LsaFreeMemory [ADVAPI32.@]
 */
NTSTATUS WINAPI
LsaFreeMemory(IN PVOID Buffer)
{
	TRACE("(%p)\n",Buffer);
	return HeapFree(GetProcessHeap(), 0, Buffer);
}
/******************************************************************************
 * LsaClose [ADVAPI32.@]
 */
NTSTATUS WINAPI
LsaClose(IN LSA_HANDLE ObjectHandle)
{
	FIXME("(%p):stub\n",ObjectHandle);
	return 0xc0000000;
}
/******************************************************************************
 * NotifyBootConfigStatus [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 */
BOOL WINAPI
NotifyBootConfigStatus( DWORD x1 )
{
	FIXME("(0x%08x):stub\n",x1);
	return 1;
}

/******************************************************************************
 * RevertToSelf [ADVAPI32.@]
 *
 * PARAMS
 *   void []
 */
BOOL WINAPI
RevertToSelf( void )
{
	FIXME("(), stub\n");
	return TRUE;
}

/******************************************************************************
 * ImpersonateSelf [ADVAPI32.@]
 */
BOOL WINAPI
ImpersonateSelf(SECURITY_IMPERSONATION_LEVEL ImpersonationLevel)
{
	return RtlImpersonateSelf(ImpersonationLevel);
}

/******************************************************************************
 * AccessCheck [ADVAPI32.@]
 *
 * FIXME check cast LPBOOL to PBOOLEAN
 */
BOOL WINAPI
AccessCheck(
	PSECURITY_DESCRIPTOR SecurityDescriptor,
	HANDLE ClientToken,
	DWORD DesiredAccess,
	PGENERIC_MAPPING GenericMapping,
	PPRIVILEGE_SET PrivilegeSet,
	LPDWORD PrivilegeSetLength,
	LPDWORD GrantedAccess,
	LPBOOL AccessStatus)
{
	CallWin32ToNt (NtAccessCheck(SecurityDescriptor, ClientToken, DesiredAccess,
	  GenericMapping, PrivilegeSet, PrivilegeSetLength, GrantedAccess, (PBOOLEAN)AccessStatus));
}

/*************************************************************************
 * SetKernelObjectSecurity [ADVAPI32.@]
 */
BOOL WINAPI SetKernelObjectSecurity (
	IN HANDLE Handle,
	IN SECURITY_INFORMATION SecurityInformation,
	IN PSECURITY_DESCRIPTOR SecurityDescriptor )
{
	CallWin32ToNt (NtSetSecurityObject (Handle, SecurityInformation, SecurityDescriptor));
}

/******************************************************************************
 *  AddAccessAllowedAce [ADVAPI32.@]
 */
BOOL WINAPI AddAccessAllowedAce(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
        IN DWORD AccessMask,
        IN PSID pSid)
{
        CallWin32ToNt( RtlAddAccessAllowedAce(pAcl, dwAceRevision, AccessMask, pSid) );
}

/******************************************************************************
 *  AddAccessDeniedAce [ADVAPI32.@]
 */
BOOL WINAPI AddAccessDeniedAce(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
        IN DWORD AccessMask,
        IN PSID pSid)
{
        CallWin32ToNt( RtlAddAccessDeniedAce(pAcl, dwAceRevision, AccessMask, pSid) );
}


/******************************************************************************
 * LookupAccountNameA [ADVAPI32.@]
 */
BOOL WINAPI
LookupAccountNameA(
	IN LPCSTR system,
	IN LPCSTR account,
	OUT PSID sid,
	OUT LPDWORD cbSid,
	LPSTR ReferencedDomainName,
	IN OUT LPDWORD cbReferencedDomainName,
	OUT PSID_NAME_USE name_use )
{
    FIXME("(%s,%s,%p,%p,%p,%p,%p), stub\n",
          debugstr_a(system), debugstr_a(account), sid, cbSid,
          ReferencedDomainName, cbReferencedDomainName, name_use);
    return FALSE;
}

/******************************************************************************
 * LookupAccountNameW [ADVAPI32.@]
 */
BOOL WINAPI
LookupAccountNameW(
	IN LPCWSTR system,
	IN LPCWSTR account,
	OUT PSID sid,
	OUT LPDWORD cbSid,
	LPWSTR ReferencedDomainName,
	IN OUT LPDWORD cbReferencedDomainName,
	OUT PSID_NAME_USE name_use )
{
    FIXME("(%s,%s,%p,%p,%p,%p,%p), stub\n",
          debugstr_w(system), debugstr_w(account), sid, cbSid,
          ReferencedDomainName, cbReferencedDomainName, name_use);
    return FALSE;
}

/******************************************************************************
 * GetAce [ADVAPI32.@]
 */
BOOL WINAPI GetAce(PACL pAcl,DWORD dwAceIndex,LPVOID *pAce )
{
    CallWin32ToNt(RtlGetAce(pAcl, dwAceIndex, pAce));
}

/******************************************************************************
 * ConvertSidToStringSidW [ADVAPI32.@]
 */
BOOL WINAPI ConvertSidToStringSidW(PSID sid, LPWSTR* stringSid)
{
    NTSTATUS ret;
    UNICODE_STRING str = { 0, 0, NULL };
    ret = RtlConvertSidToUnicodeString(&str, sid, TRUE);

    if (ret == STATUS_NO_MEMORY)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    /* FIXME: error codes for other ret values,
     * which don't exist in NTDLL yet */

    *stringSid = (LPWSTR)LocalAlloc(LMEM_ZEROINIT, (str.Length+1)*sizeof(WCHAR));
    lstrcpynW(*stringSid, str.Buffer, str.Length + 1);
    RtlFreeUnicodeString(&str);

    return TRUE;
}

/******************************************************************************
 * ConvertSidToStringSidA [ADVAPI32.@]
 */
BOOL WINAPI ConvertSidToStringSidA(PSID sid, LPSTR* stringSid)
{
    WCHAR *wstr = NULL;
    int len;

    if (!ConvertSidToStringSidW(sid, &wstr))
        return FALSE;

    len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    *stringSid = (LPSTR)LocalAlloc(LMEM_ZEROINIT, len);
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, *stringSid, len, NULL, NULL);

    LocalFree((HLOCAL)wstr);

    return TRUE;
}

/******************************************************************************
 * ConvertStringSidToSidA [ADVAPI32.@]
 */
BOOL WINAPI ConvertStringSidToSidA(LPCSTR stringSid, PSID* sid)
{
    FIXME("stub\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/******************************************************************************
 * PrivilegeCheck [ADVAPI32.@]
 *
 * Stub - can be used to determine whether user is in administrative mode.
 * Returns yes to all valid privileges queries.
 */
BOOL WINAPI PrivilegeCheck( HANDLE ClientToken, PPRIVILEGE_SET RequiredPrivileges,
                            LPBOOL lpResult )
{
    FIXME( "%x %p %p: stub (entry)\n", ClientToken, RequiredPrivileges, lpResult );
    if ( lpResult == NULL )
        return( FALSE );
    
    *lpResult = TRUE;
    
    return( TRUE );
}


/******************************************************************************
 * SetEntriesInAclA [ADVAPI32.@]
 */
DWORD WINAPI SetEntriesInAclA(ULONG cCountOfExplicitEntries,
        /*PEXPLICIT_ACCESS*/ void* pListOfExplicitEntries,
        PACL OldAcl,
        PACL* NewAcl)
{
    FIXME("%u %p %p %p: stub\n", cCountOfExplicitEntries, pListOfExplicitEntries,
            OldAcl, NewAcl);
    *NewAcl = NULL;
    return ERROR_UNKNOWN;
}


