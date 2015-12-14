name	mlang
type	win32
init	DllMain

import ntdll.dll
import kernel32.dll
import gdi32.dll
import -delay oleaut32.dll

110 stdcall IsConvertINetStringAvailable(long long)
111 stdcall ConvertINetString(ptr long long ptr ptr ptr ptr)
112 stdcall ConvertINetUnicodeToMultiByte(ptr long ptr ptr ptr ptr)
113 stdcall ConvertINetMultiByteToUnicode(ptr long ptr ptr ptr ptr)
114 stub ConvertINetReset
120 stdcall LcidToRfc1766A(long ptr long)
121 stdcall LcidToRfc1766W(long ptr long)
122 stdcall Rfc1766ToLcidA(ptr str)
123 stdcall Rfc1766ToLcidW(ptr wstr)

@ stdcall DllCanUnloadNow()
@ stdcall DllGetClassObject(ptr ptr ptr)
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
@ stdcall GetGlobalFontLinkObject()
