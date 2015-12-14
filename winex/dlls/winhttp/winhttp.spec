name winhttp
type win32
init DllMain

import jsproxy.dll
import user32.dll
import kernel32.dll
import advapi32.dll
import -delay oleaut32
import -delay ole32
import -delay crypt32
import -delay secur32

@ stdcall DllCanUnloadNow()
@ stdcall DllGetClassObject(ptr ptr ptr)
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
@ stdcall WinHttpAddRequestHeaders(ptr wstr long long)
@ stdcall WinHttpCheckPlatform()
@ stdcall WinHttpCloseHandle(ptr)
@ stdcall WinHttpConnect(ptr wstr long long)
@ stdcall WinHttpCrackUrl(wstr long long ptr)
@ stdcall WinHttpCreateUrl(ptr long ptr ptr)
@ stdcall WinHttpDetectAutoProxyConfigUrl(long ptr)
@ stdcall WinHttpGetDefaultProxyConfiguration(ptr)
@ stdcall WinHttpGetIEProxyConfigForCurrentUser(ptr)
@ stdcall WinHttpGetProxyForUrl(ptr wstr ptr ptr)
@ stdcall WinHttpOpen(wstr long wstr wstr long)
@ stdcall WinHttpOpenRequest(ptr wstr wstr wstr wstr ptr long)
@ stdcall WinHttpQueryAuthSchemes(ptr ptr ptr ptr)
@ stdcall WinHttpQueryDataAvailable(ptr ptr)
@ stdcall WinHttpQueryHeaders(ptr long wstr ptr ptr ptr)
@ stdcall WinHttpQueryOption(ptr long ptr ptr)
@ stdcall WinHttpReadData(ptr ptr long ptr)
@ stdcall WinHttpReceiveResponse(ptr ptr)
@ stdcall WinHttpSendRequest(ptr wstr long ptr long long ptr)
@ stdcall WinHttpSetCredentials(ptr long long wstr ptr ptr)
@ stdcall WinHttpSetDefaultProxyConfiguration(ptr)
@ stdcall WinHttpSetOption(ptr long ptr long)
@ stdcall WinHttpSetStatusCallback(ptr ptr long ptr)
@ stdcall WinHttpSetTimeouts(ptr long long long long)
@ stdcall WinHttpTimeFromSystemTime(ptr ptr)
@ stdcall WinHttpTimeToSystemTime(wstr ptr)
@ stdcall WinHttpWriteData(ptr ptr long ptr)
