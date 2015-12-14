name winhttp
type win32
init DllMain

import kernel32.dll
import wininet.dll


@ stdcall DllCanUnloadNow()
@ stdcall DllGetClassObject(ptr ptr ptr)
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
@ stdcall WinHttpAddRequestHeaders(long wstr long long)
@ stdcall WinHttpCheckPlatform()
@ stdcall WinHttpCloseHandle(long)
@ stdcall WinHttpConnect(long wstr long long)
@ stdcall WinHttpCrackUrl(wstr long long ptr)
@ stdcall WinHttpCreateUrl(ptr long ptr ptr)
@ stdcall WinHttpDetectAutoProxyConfigUrl(long ptr)
@ stdcall WinHttpGetDefaultProxyConfiguration(ptr)
@ stdcall WinHttpGetIEProxyConfigForCurrentUser(ptr)
@ stdcall WinHttpGetProxyForUrl(long wstr ptr ptr)
@ stdcall WinHttpOpen(wstr long wstr wstr long)
@ stdcall WinHttpOpenRequest(long wstr wstr wstr wstr ptr long)
@ stdcall WinHttpQueryAuthSchemes(long ptr ptr ptr)
@ stdcall WinHttpQueryDataAvailable(long ptr)
@ stdcall WinHttpQueryHeaders(long long wstr ptr ptr ptr)
@ stdcall WinHttpQueryOption(long long ptr ptr)
@ stdcall WinHttpReadData(long ptr long ptr)
@ stdcall WinHttpReceiveResponse(long ptr)
@ stdcall WinHttpSendRequest(long wstr long ptr long long ptr)
@ stdcall WinHttpSetCredentials(long long long wstr wstr ptr)
@ stdcall WinHttpSetDefaultProxyConfiguration(ptr)
@ stdcall WinHttpSetOption(long long ptr long)
@ stdcall WinHttpSetStatusCallback(long ptr long ptr)
@ stdcall WinHttpSetTimeouts(long long long long long)
@ stdcall WinHttpTimeFromSystemTime(ptr ptr)
@ stdcall WinHttpTimeToSystemTime(wstr ptr)
@ stdcall WinHttpWriteData(long ptr long ptr)
