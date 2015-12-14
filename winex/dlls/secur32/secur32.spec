name    secur32 
type    win32
init    DllMain

import user32.dll 
import advapi32.dll
import kernel32.dll
import ntdll.dll


@ stdcall AcceptSecurityContext(ptr ptr ptr long long ptr ptr ptr ptr) AcceptSecurityContext
@ stdcall AcquireCredentialsHandleA(str str long ptr ptr ptr ptr ptr ptr) AcquireCredentialsHandleA
@ stdcall AcquireCredentialsHandleW(wstr wstr long ptr ptr ptr ptr ptr ptr) AcquireCredentialsHandleW
@ stdcall AddCredentialsA(ptr str str long ptr ptr ptr ptr) AddCredentialsA
@ stdcall AddCredentialsW(ptr wstr wstr long ptr ptr ptr ptr) AddCredentialsW
@ stub AddSecurityPackageA
@ stub AddSecurityPackageW
@ stdcall ApplyControlToken(ptr ptr) ApplyControlToken
@ stdcall CompleteAuthToken(ptr ptr) CompleteAuthToken
@ stdcall DecryptMessage(ptr ptr long ptr) DecryptMessage
@ stdcall DeleteSecurityContext(ptr) DeleteSecurityContext
@ stub DeleteSecurityPackageA
@ stub DeleteSecurityPackageW
@ stdcall EncryptMessage(ptr long ptr long) EncryptMessage
@ stdcall EnumerateSecurityPackagesA(ptr ptr) EnumerateSecurityPackagesA
@ stdcall EnumerateSecurityPackagesW(ptr ptr) EnumerateSecurityPackagesW
@ stdcall ExportSecurityContext(ptr long ptr ptr) ExportSecurityContext
@ stdcall FreeContextBuffer(ptr) FreeContextBuffer
@ stdcall FreeCredentialsHandle(ptr) FreeCredentialsHandle
@ stub GetComputerObjectNameA
@ stub GetComputerObjectNameW
@ stub GetSecurityUserInfo
@ stub GetUserNameExA
@ stub GetUserNameExW
@ stdcall ImpersonateSecurityContext(ptr) ImpersonateSecurityContext
@ stdcall ImportSecurityContextA(str ptr ptr ptr) ImportSecurityContextA
@ stdcall ImportSecurityContextW(wstr ptr ptr ptr) ImportSecurityContextW
@ stdcall InitSecurityInterfaceA() InitSecurityInterfaceA
@ stdcall InitSecurityInterfaceW() InitSecurityInterfaceW
@ stdcall InitializeSecurityContextA(ptr ptr str long long long ptr long ptr ptr ptr ptr) InitializeSecurityContextA
@ stdcall InitializeSecurityContextW(ptr ptr wstr long long long ptr long ptr ptr ptr ptr) InitializeSecurityContextW
@ stub LsaCallAuthenticationPackage
@ stub LsaConnectUntrusted
@ stub LsaDeregisterLogonProcess
@ stub LsaEnumerateLogonSessions
@ stub LsaFreeReturnBuffer
@ stub LsaGetLogonSessionData
@ stub LsaLogonUser
@ stub LsaLookupAuthenticationPackage
@ stub LsaRegisterLogonProcess
@ stub LsaRegisterPolicyChangeNotification
@ stub LsaUnregisterPolicyChangeNotification
@ stdcall MakeSignature(ptr long ptr long) MakeSignature
@ stdcall QueryContextAttributesA(ptr long ptr) QueryContextAttributesA
@ stdcall QueryContextAttributesW(ptr long ptr) QueryContextAttributesW
@ stdcall QueryCredentialsAttributesA(ptr long ptr) QueryCredentialsAttributesA
@ stdcall QueryCredentialsAttributesW(ptr long ptr) QueryCredentialsAttributesW
@ stdcall QuerySecurityContextToken(ptr ptr) QuerySecurityContextToken
@ stdcall QuerySecurityPackageInfoA(str ptr) QuerySecurityPackageInfoA
@ stdcall QuerySecurityPackageInfoW(wstr ptr) QuerySecurityPackageInfoW
@ stdcall RevertSecurityContext(ptr) RevertSecurityContext
@ stub SaslAcceptSecurityContext
@ stub SaslEnumerateProfilesA
@ stub SaslEnumerateProfilesW
@ stub SaslGetProfilePackageA
@ stub SaslGetProfilePackageW
@ stub SaslIdentifyPackageA
@ stub SaslIdentifyPackageW
@ stub SaslInitializeSecurityContextA
@ stub SaslInitializeSecurityContextW
@ stub SealMessage
@ stub SecCacheSspiPackages
@ stub SecDeleteUserModeContext
@ stub SecGetLocaleSpecificEncryptionRules
@ stub SecInitUserModeContext
@ stub SecpFreeMemory
@ stub SecpTranslateName
@ stub SecpTranslateNameEx
@ stub TranslateNameA
@ stub TranslateNameW
@ stub UnsealMessage
@ stdcall VerifySignature(ptr ptr long ptr) VerifySignature
