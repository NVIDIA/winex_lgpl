name applemp3
file applemp3.acm
type win32

import winmm.dll
import user32.dll
import kernel32.dll
import ntdll.dll


@ stdcall DriverProc (long long long long long) APPLEMP3_DriverProc
