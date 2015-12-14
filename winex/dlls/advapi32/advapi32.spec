name	advapi32
type	win32
init	DllMain

import	kernel32.dll
import	ntdll.dll


@ stdcall A_SHAFinal(ptr ptr) A_SHAFinal
@ stdcall A_SHAInit(ptr) A_SHAInit
@ stdcall A_SHAUpdate(ptr ptr ptr) A_SHAUpdate
@ stub AbortSystemShutdownA
@ stub AbortSystemShutdownW
@ stdcall AccessCheck(ptr long long ptr ptr ptr ptr ptr) AccessCheck
@ stub AccessCheckAndAuditAlarmA
@ stub AccessCheckAndAuditAlarmW
@ stub AccessCheckByType
@ stdcall AddAccessAllowedAce (ptr long long ptr) AddAccessAllowedAce
@ stub AddAccessAllowedAceEx
@ stdcall AddAccessDeniedAce(ptr long long ptr) AddAccessDeniedAce
@ stub AddAce
@ stub AddAuditAccessAce
@ stub AdjustTokenGroups
@ stdcall AdjustTokenPrivileges(long long ptr long ptr ptr) AdjustTokenPrivileges
@ stdcall AllocateAndInitializeSid(ptr long long long long long long long long long ptr) AllocateAndInitializeSid
@ stdcall AllocateLocallyUniqueId(ptr) AllocateLocallyUniqueId
@ stub AreAllAccessesGranted
@ stub AreAnyAccessesGranted
@ stdcall BackupEventLogA (long str) BackupEventLogA
@ stdcall BackupEventLogW (long wstr) BackupEventLogW
@ stdcall ChangeServiceConfigA(long long long long str  str  ptr  str  str  str  str) ChangeServiceConfigA
@ stdcall ChangeServiceConfigW(long long long long wstr wstr ptr wstr wstr wstr wstr) ChangeServiceConfigW
@ stdcall ChangeServiceConfig2A(long long ptr) ChangeServiceConfig2A
@ stdcall ChangeServiceConfig2W(long long ptr) ChangeServiceConfig2W
@ stdcall ClearEventLogA (long str) ClearEventLogA
@ stdcall ClearEventLogW (long wstr) ClearEventLogW
@ stdcall CloseEventLog (long) CloseEventLog
@ stdcall CloseServiceHandle(long) CloseServiceHandle
@ stub CommandLineFromMsiDescriptor
@ stdcall ControlService(long long ptr) ControlService
@ stdcall ConvertSidToStringSidA (ptr ptr) ConvertSidToStringSidA
@ stdcall ConvertSidToStringSidW (ptr ptr) ConvertSidToStringSidW
@ stub ConvertStringSecurityDescriptorToSecurityDescriptorW
@ stub ConvertStringSecurityDescriptorToSecurityDescriptorA
@ stdcall ConvertStringSidToSidA (str ptr) ConvertStringSidToSidA
@ stdcall CopySid(long ptr ptr) CopySid
@ stub CreatePrivateObjectSecurity
@ stub CreateProcessAsUserA
@ stub CreateProcessAsUserW
@ stdcall CreateServiceA(long ptr ptr long long long long ptr ptr ptr ptr ptr ptr) CreateServiceA
@ stdcall CreateServiceW (long ptr ptr long long long long ptr ptr ptr ptr ptr ptr) CreateServiceW
@ stub CredProfileLoaded
@ stdcall CryptAcquireContextA(ptr str str long long) CryptAcquireContextA
@ stdcall CryptAcquireContextW(ptr wstr wstr long long) CryptAcquireContextW
@ stdcall CryptContextAddRef(long ptr long) CryptContextAddRef
@ stdcall CryptCreateHash(long long long long ptr) CryptCreateHash
@ stdcall CryptDecrypt(long long long long ptr ptr) CryptDecrypt
@ stdcall CryptDeriveKey(long long long long ptr) CryptDeriveKey
@ stdcall CryptDestroyHash(long) CryptDestroyHash
@ stdcall CryptDestroyKey(long) CryptDestroyKey
@ stdcall CryptDuplicateHash(long ptr long ptr) CryptDuplicateHash
@ stdcall CryptDuplicateKey(long ptr long ptr) CryptDuplicateKey
@ stdcall CryptEncrypt(long long long long ptr ptr long) CryptEncrypt
@ stdcall CryptEnumProvidersA(long ptr long ptr ptr ptr) CryptEnumProvidersA
@ stdcall CryptEnumProvidersW(long ptr long ptr ptr ptr) CryptEnumProvidersW
@ stdcall CryptEnumProviderTypesA(long ptr long ptr ptr ptr) CryptEnumProviderTypesA
@ stdcall CryptEnumProviderTypesW(long ptr long ptr ptr ptr) CryptEnumProviderTypesW
@ stdcall CryptExportKey(long long long long ptr ptr) CryptExportKey
@ stdcall CryptGenKey(long long long ptr) CryptGenKey
@ stdcall CryptGenRandom(long long ptr) CryptGenRandom
@ stdcall CryptGetDefaultProviderA(long ptr long ptr ptr) CryptGetDefaultProviderA
@ stdcall CryptGetDefaultProviderW(long ptr long ptr ptr) CryptGetDefaultProviderW
@ stdcall CryptGetHashParam(long long ptr ptr long) CryptGetHashParam
@ stdcall CryptGetKeyParam(long long ptr ptr long) CryptGetKeyParam
@ stdcall CryptGetProvParam(long long ptr ptr long) CryptGetProvParam
@ stdcall CryptGetUserKey(long long ptr) CryptGetUserKey
@ stdcall CryptHashData(long ptr long long) CryptHashData
@ stdcall CryptHashSessionKey(long long long) CryptHashSessionKey
@ stdcall CryptImportKey(long ptr long long long ptr) CryptImportKey
@ stdcall CryptReleaseContext(long long) CryptReleaseContext
@ stdcall CryptSignHashA(long long ptr long ptr ptr) CryptSignHashA
@ stdcall CryptSignHashW(long long ptr long ptr ptr) CryptSignHashA
@ stdcall CryptSetHashParam(long long ptr long) CryptSetHashParam
@ stdcall CryptSetKeyParam(long long ptr long) CryptSetKeyParam
@ stdcall CryptSetProviderA(str long) CryptSetProviderA
@ stdcall CryptSetProviderW(wstr long) CryptSetProviderW
@ stdcall CryptSetProviderExA(str long ptr long) CryptSetProviderExA
@ stdcall CryptSetProviderExW(wstr long ptr long) CryptSetProviderExW
@ stdcall CryptSetProvParam(long long ptr long) CryptSetProvParam
@ stdcall CryptVerifySignatureA(long ptr long long ptr long) CryptVerifySignatureA
@ stdcall CryptVerifySignatureW(long ptr long long ptr long) CryptVerifySignatureA
@ stub DeleteAce
@ stdcall DeleteService(long) DeleteService
@ stdcall DeregisterEventSource(long) DeregisterEventSource
@ stub DestroyPrivateObjectSecurity
@ stdcall DuplicateToken(long long ptr) DuplicateToken
@ stub DuplicateTokenEx
@ stub EnumDependentServicesA
@ stub EnumDependentServicesW
@ stdcall EnumServicesStatusA (long long long ptr long ptr ptr ptr) EnumServicesStatusA
@ stdcall EnumServicesStatusW (long long long ptr long ptr ptr ptr) EnumServicesStatusW
@ stdcall EqualPrefixSid(ptr ptr) EqualPrefixSid
@ stdcall EqualSid(ptr ptr) EqualSid
@ stub FindFirstFreeAce
@ stdcall FreeSid(ptr) FreeSid
@ stdcall GetAce(ptr long ptr) GetAce
@ stub GetAclInformation
@ stub GetCurrentHwProfileA
@ stub GetCurrentHwProfileW
@ stdcall GetFileSecurityA(str long ptr long ptr) GetFileSecurityA
@ stdcall GetFileSecurityW(wstr long ptr long ptr) GetFileSecurityW
@ stub GetKernelObjectSecurity
@ stdcall GetLengthSid(ptr) GetLengthSid
@ stub GetMangledSiteSid
@ stub GetNamedSecurityInfoA
@ stub GetNamedSecurityInfoW
@ stdcall GetNumberOfEventLogRecords (long ptr) GetNumberOfEventLogRecords
@ stdcall GetOldestEventLogRecord (long ptr) GetOldestEventLogRecord
@ stub GetPrivateObjectSecurity
@ stdcall GetSecurityDescriptorControl (ptr ptr ptr) GetSecurityDescriptorControl
@ stdcall GetSecurityDescriptorDacl (ptr ptr ptr ptr) GetSecurityDescriptorDacl
@ stdcall GetSecurityDescriptorGroup(ptr ptr ptr) GetSecurityDescriptorGroup
@ stdcall GetSecurityDescriptorLength(ptr) GetSecurityDescriptorLength
@ stdcall GetSecurityDescriptorOwner(ptr ptr ptr) GetSecurityDescriptorOwner
@ stdcall GetSecurityDescriptorSacl (ptr ptr ptr ptr) GetSecurityDescriptorSacl
@ stub GetSecurityInfo
@ stub GetServiceDisplayNameA
@ stub GetServiceDisplayNameW
@ stub GetServiceKeyNameA
@ stub GetServiceKeyNameW
@ stdcall GetSidIdentifierAuthority(ptr) GetSidIdentifierAuthority
@ stdcall GetSidLengthRequired(long) GetSidLengthRequired
@ stdcall GetSidSubAuthority(ptr long) GetSidSubAuthority
@ stdcall GetSidSubAuthorityCount(ptr) GetSidSubAuthorityCount
@ stub GetSiteSidFromToken
@ stdcall GetTokenInformation(long long ptr long ptr) GetTokenInformation
@ stub GetTraceEnableFlags
@ stub GetTraceEnableLevel
@ stub GetTraceLoggerHandle
@ stdcall GetUserNameA(ptr ptr) GetUserNameA
@ stdcall GetUserNameW(ptr ptr) GetUserNameW
@ stub ImpersonateLoggedOnUser
@ stub ImpersonateNamedPipeClient
@ stdcall ImpersonateSelf(long) ImpersonateSelf
@ stdcall InitializeAcl(ptr long long) InitializeAcl
@ stdcall InitializeSecurityDescriptor(ptr long) InitializeSecurityDescriptor
@ stdcall InitializeSid(ptr ptr long) InitializeSid
@ stub InitiateSystemShutdownA
@ stub InitiateSystemShutdownW
@ stub InstallApplication
@ stub IsProcessRestricted
@ forward IsTextUnicode ntdll.RtlIsTextUnicode
@ stdcall IsTokenRestricted(long) IsTokenRestricted
@ stub IsValidAcl
@ stdcall IsValidSecurityDescriptor(ptr) IsValidSecurityDescriptor
@ stdcall IsValidSid(ptr) IsValidSid
@ stdcall LockServiceDatabase(long) LockServiceDatabase
@ stub LogonUserA
@ stub LogonUserW
@ stdcall LookupAccountNameA(str str ptr ptr ptr ptr ptr) LookupAccountNameA
@ stdcall LookupAccountNameW(wstr wstr ptr ptr ptr ptr ptr) LookupAccountNameW
@ stdcall LookupAccountSidA(ptr ptr ptr ptr ptr ptr ptr) LookupAccountSidA
@ stdcall LookupAccountSidW(ptr ptr ptr ptr ptr ptr ptr) LookupAccountSidW
@ stub LookupPrivilegeDisplayNameA
@ stub LookupPrivilegeDisplayNameW
@ stdcall LookupPrivilegeNameA(ptr ptr ptr ptr)
@ stdcall LookupPrivilegeNameW(ptr ptr ptr ptr)
@ stdcall LookupPrivilegeValueA(ptr ptr ptr) LookupPrivilegeValueA
@ stdcall LookupPrivilegeValueW(ptr ptr ptr) LookupPrivilegeValueW
@ stub MakeAbsoluteSD
@ stdcall MakeSelfRelativeSD(ptr ptr ptr) MakeSelfRelativeSD
@ stub MapGenericMask
@ stdcall NotifyBootConfigStatus(long) NotifyBootConfigStatus
@ stdcall NotifyChangeEventLog (long long) NotifyChangeEventLog
@ stub ObjectCloseAuditAlarmA
@ stub ObjectCloseAuditAlarmW
@ stub ObjectOpenAuditAlarmA
@ stub ObjectOpenAuditAlarmW
@ stub ObjectPrivilegeAuditAlarmA
@ stub ObjectPrivilegeAuditAlarmW
@ stdcall OpenBackupEventLogA (str str) OpenBackupEventLogA
@ stdcall OpenBackupEventLogW (wstr wstr) OpenBackupEventLogW
@ stdcall OpenEventLogA (str str) OpenEventLogA
@ stdcall OpenEventLogW (wstr wstr) OpenEventLogW
@ stdcall OpenProcessToken(long long ptr) OpenProcessToken
@ stdcall OpenSCManagerA(ptr ptr long) OpenSCManagerA
@ stdcall OpenSCManagerW(ptr ptr long) OpenSCManagerW
@ stdcall OpenServiceA(long str long) OpenServiceA
@ stdcall OpenServiceW(long wstr long) OpenServiceW
@ stdcall OpenThreadToken(long long long ptr) OpenThreadToken
@ stdcall PrivilegeCheck(long ptr ptr) PrivilegeCheck
@ stub PrivilegedServiceAuditAlarmA
@ stub PrivilegedServiceAuditAlarmW
@ stub QueryServiceConfigA
@ stub QueryServiceConfigW
@ stub QueryServiceLockStatusA
@ stub QueryServiceLockStatusW
@ stub QueryServiceObjectSecurity
@ stdcall QueryServiceStatus(long ptr) QueryServiceStatus
@ stdcall QueryServiceStatusEx(long long ptr long ptr) QueryServiceStatusEx
@ stdcall ReadEventLogA (long long long ptr long ptr ptr) ReadEventLogA
@ stdcall ReadEventLogW (long long long ptr long ptr ptr) ReadEventLogW
@ stdcall RegCloseKey(long) RegCloseKey
@ stdcall RegConnectRegistryA(str long ptr) RegConnectRegistryA
@ stdcall RegConnectRegistryW(wstr long ptr) RegConnectRegistryW
@ stdcall RegCreateKeyA(long str ptr) RegCreateKeyA
@ stdcall RegCreateKeyExA(long str long ptr long long ptr ptr ptr) RegCreateKeyExA
@ stdcall RegCreateKeyExW(long wstr long ptr long long ptr ptr ptr) RegCreateKeyExW
@ stdcall RegCreateKeyW(long wstr ptr) RegCreateKeyW
@ stdcall RegDeleteKeyA(long str) RegDeleteKeyA
@ stdcall RegDeleteKeyW(long wstr) RegDeleteKeyW
@ stdcall RegDeleteTreeA(long str) RegDeleteKeyA
@ stdcall RegDeleteTreeW(long wstr) RegDeleteKeyW
@ stdcall RegDeleteValueA(long str) RegDeleteValueA
@ stdcall RegDeleteValueW(long wstr) RegDeleteValueW
@ stdcall RegEnumKeyA(long long ptr long) RegEnumKeyA
@ stdcall RegEnumKeyExA(long long ptr ptr ptr ptr ptr ptr) RegEnumKeyExA
@ stdcall RegEnumKeyExW(long long ptr ptr ptr ptr ptr ptr) RegEnumKeyExW
@ stdcall RegEnumKeyW(long long ptr long) RegEnumKeyW
@ stdcall RegEnumValueA(long long ptr ptr ptr ptr ptr ptr) RegEnumValueA
@ stdcall RegEnumValueW(long long ptr ptr ptr ptr ptr ptr) RegEnumValueW
@ stdcall RegFlushKey(long) RegFlushKey
@ stdcall RegGetKeySecurity(long long ptr ptr) RegGetKeySecurity
@ stdcall RegisterTraceGuidsW(ptr ptr ptr long ptr wstr wstr ptr)
@ stub RegGetValueW
@ stdcall RegLoadKeyA(long str str) RegLoadKeyA
@ stdcall RegLoadKeyW(long wstr wstr) RegLoadKeyW
@ stdcall RegNotifyChangeKeyValue(long long long long long) RegNotifyChangeKeyValue
@ stdcall RegOpenCurrentUser(long ptr) RegOpenCurrentUser
@ stdcall RegOpenKeyA(long str ptr) RegOpenKeyA
@ stdcall RegOpenKeyExA(long str long long ptr) RegOpenKeyExA
@ stdcall RegOpenKeyExW(long wstr long long ptr) RegOpenKeyExW
@ stdcall RegOpenKeyW(long wstr ptr) RegOpenKeyW
@ stub RegOpenUserClassesRoot
@ stdcall RegQueryInfoKeyA(long ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr) RegQueryInfoKeyA
@ stdcall RegQueryInfoKeyW(long ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr) RegQueryInfoKeyW
@ stub RegQueryMultipleValuesA
@ stub RegQueryMultipleValuesW
@ stdcall RegQueryValueA(long str ptr ptr) RegQueryValueA
@ stdcall RegQueryValueExA(long str ptr ptr ptr ptr) RegQueryValueExA
@ stdcall RegQueryValueExW(long wstr ptr ptr ptr ptr) RegQueryValueExW
@ stdcall RegQueryValueW(long wstr ptr ptr) RegQueryValueW
@ stub RegRemapPreDefKey
@ stdcall RegReplaceKeyA(long str str str) RegReplaceKeyA
@ stdcall RegReplaceKeyW(long wstr wstr wstr) RegReplaceKeyW
@ stdcall RegRestoreKeyA(long str long) RegRestoreKeyA
@ stdcall RegRestoreKeyW(long wstr long) RegRestoreKeyW
@ stdcall RegSaveKeyA(long ptr ptr) RegSaveKeyA
@ stdcall RegSaveKeyW(long ptr ptr) RegSaveKeyW
@ stdcall RegSetKeySecurity(long long ptr) RegSetKeySecurity
@ stdcall RegSetValueA(long str long ptr long) RegSetValueA
@ stdcall RegSetValueExA(long str long long ptr long) RegSetValueExA
@ stdcall RegSetValueExW(long wstr long long ptr long) RegSetValueExW
@ stdcall RegSetValueW(long wstr long ptr long) RegSetValueW
@ stdcall RegUnLoadKeyA(long str) RegUnLoadKeyA
@ stdcall RegUnLoadKeyW(long wstr) RegUnLoadKeyW
@ stdcall RegisterEventSourceA(ptr ptr) RegisterEventSourceA
@ stdcall RegisterEventSourceW(ptr ptr) RegisterEventSourceW
@ stdcall RegisterServiceCtrlHandlerA (ptr ptr) RegisterServiceCtrlHandlerA
@ stdcall RegisterServiceCtrlHandlerW (ptr ptr) RegisterServiceCtrlHandlerW
@ stdcall ReportEventA (long long long long ptr long long str ptr) ReportEventA
@ stdcall ReportEventW (long long long long ptr long long wstr ptr) ReportEventW
@ stdcall RevertToSelf() RevertToSelf
@ stub SetAclInformation
@ stdcall SetEntriesInAclA(long ptr ptr ptr) SetEntriesInAclA
@ stub SetEntriesInAclW
@ stdcall SetFileSecurityA(str long ptr ) SetFileSecurityA
@ stdcall SetFileSecurityW(wstr long ptr) SetFileSecurityW
@ stdcall SetKernelObjectSecurity(long long ptr) SetKernelObjectSecurity
@ stub SetNamedSecurityInfoA
@ stub SetNamedSecurityInfoW
@ stub SetPrivateObjectSecurity
@ stub SetSecurityDescriptorControl
@ stdcall SetSecurityDescriptorDacl(ptr long ptr long) SetSecurityDescriptorDacl
@ stdcall SetSecurityDescriptorGroup (ptr ptr long) SetSecurityDescriptorGroup
@ stdcall SetSecurityDescriptorOwner (ptr ptr long) SetSecurityDescriptorOwner
@ stdcall SetSecurityDescriptorSacl(ptr long ptr long) SetSecurityDescriptorSacl
@ stdcall SetSecurityInfo(long long long ptr ptr ptr ptr) SetSecurityInfo
@ stub SetServiceBits
@ stub SetServiceObjectSecurity
@ stdcall SetServiceStatus(long long)SetServiceStatus
@ stdcall SetThreadToken (ptr ptr) SetThreadToken
@ stub SetTokenInformation
@ stdcall StartServiceA(long long ptr) StartServiceA
@ stdcall StartServiceCtrlDispatcherA(ptr) StartServiceCtrlDispatcherA
@ stdcall StartServiceCtrlDispatcherW(ptr) StartServiceCtrlDispatcherW
@ stdcall StartServiceW(long long ptr) StartServiceW
@ stub TraceMessage
@ stdcall UnlockServiceDatabase(long) UnlockServiceDatabase
@ stub UnregisterTraceGuids
@ stdcall LsaOpenPolicy(long long long long) LsaOpenPolicy
@ stdcall LsaLookupSids(ptr long ptr ptr ptr) LsaLookupSids
@ stdcall LsaFreeMemory(ptr)LsaFreeMemory
@ stdcall LsaQueryInformationPolicy(ptr long ptr)LsaQueryInformationPolicy
@ stdcall LsaClose(ptr)LsaClose
@ stub LsaSetInformationPolicy
@ stub LsaLookupNames
@ stdcall MD5Final(ptr) MD5Final
@ stdcall MD5Init(ptr) MD5Init
@ stdcall MD5Update(ptr ptr long) MD5Update
@ stub SystemFunction001
@ stub SystemFunction002
@ stub SystemFunction003
@ stub SystemFunction004
@ stub SystemFunction005
@ stub SystemFunction006
@ stub SystemFunction007
@ stub SystemFunction008
@ stub SystemFunction009
@ stub SystemFunction010
@ stub SystemFunction011
@ stub SystemFunction012
@ stub SystemFunction013
@ stub SystemFunction014
@ stub SystemFunction015
@ stub SystemFunction016
@ stub SystemFunction017
@ stub SystemFunction018
@ stub SystemFunction019
@ stub SystemFunction020
@ stub SystemFunction021
@ stub SystemFunction022
@ stub SystemFunction023
@ stub SystemFunction024
@ stub SystemFunction025
@ stub SystemFunction026
@ stub SystemFunction027
@ stub SystemFunction028
@ stub SystemFunction029
@ stub SystemFunction030
@ stub SystemFunction031
@ stub SystemFunction032
@ stub SystemFunction033
@ stub SystemFunction034
@ stub SystemFunction035
@ stdcall SystemFunction036(ptr long) RtlGenRandom
@ stub SystemFunction037
@ stub SystemFunction038
@ stub SystemFunction039
@ stub SystemFunction040
@ stub SystemFunction041
@ stub LsaQueryInfoTrustedDomain
@ stub LsaQuerySecret
@ stub LsaCreateSecret
@ stub LsaOpenSecret
@ stub LsaCreateTrustedDomain
@ stub LsaOpenTrustedDomain
@ stub LsaSetSecret
@ stub LsaCreateAccount
@ stub LsaAddPrivilegesToAccount
@ stub LsaRemovePrivilegesFromAccount
@ stub LsaDelete
@ stub LsaSetSystemAccessAccount
@ stub LsaEnumeratePrivilegesOfAccount
@ stub LsaEnumerateAccounts
@ stub LsaGetSystemAccessAccount
@ stub LsaSetInformationTrustedDomain
@ stub LsaEnumerateTrustedDomains
@ stub LsaOpenAccount
@ stub LsaEnumeratePrivileges
@ stub LsaLookupPrivilegeDisplayName
@ stub LsaICLookupNames
@ stub ElfRegisterEventSourceW
@ stub ElfReportEventW
@ stub ElfDeregisterEventSource
@ stub ElfDeregisterEventSourceW
@ stub I_ScSetServiceBit
@ stdcall SynchronizeWindows31FilesAndWindowsNTRegistry(long long long long) SynchronizeWindows31FilesAndWindowsNTRegistry
@ stdcall QueryWindows31FilesMigration(long) QueryWindows31FilesMigration
@ stub LsaICLookupSids
@ stub I_ScSetServiceBitsA
@ stub EnumServiceGroupA
@ stub EnumServiceGroupW
@ stdcall CheckTokenMembership(long ptr ptr) CheckTokenMembership
@ stdcall RtlGenRandom(ptr long) RtlGenRandom
