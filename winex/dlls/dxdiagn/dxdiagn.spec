# dxdiagn
name dxdiagn
type win32
init DllMain
rsrc version.res

import ntdll.dll
import kernel32.dll
import oleaut32.dll
import ole32.dll
import advapi32.dll
import ddraw.dll
import version.dll

@ stdcall DllCanUnloadNow()
@ stdcall DllGetClassObject(ptr ptr ptr)
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
