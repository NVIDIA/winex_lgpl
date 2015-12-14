name msg711
file msg711.acm
type win32

import winmm.dll
import user32.dll
import kernel32.dll
import ntdll.dll


@ stdcall DriverProc (long long long long long) G711_DriverProc
