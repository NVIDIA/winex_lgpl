name	cryptnet
type	win32
init	DllMain

import crypt32.dll
import kernel32.dll
import wininet.dll
import ntdll.dll

@ stub CertDllVerifyCTLUsage
@ stdcall CertDllVerifyRevocation(long long long ptr long ptr ptr)
@ stub CryptnetWlxLogoffEvent
@ stub LdapProvOpenStore
@ stub CryptCancelAsyncRetrieval
@ stub CryptFlushTimeValidObject
@ stdcall CryptGetObjectUrl(ptr ptr long ptr ptr ptr ptr ptr)
@ stub CryptGetTimeValidObject
@ stub CryptInstallCancelRetrieval
@ stdcall CryptRetrieveObjectByUrlA(str str long long ptr ptr ptr ptr ptr)
@ stdcall CryptRetrieveObjectByUrlW(wstr str long long ptr ptr ptr ptr ptr)
@ stub CryptUninstallCancelRetrieval
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
@ stub I_CryptNetEnumUrlCacheEntry
@ stub I_CryptNetGetHostNameFromUrl
@ stub I_CryptNetGetUserDsStoreUrl
@ stub I_CryptNetIsConnected
