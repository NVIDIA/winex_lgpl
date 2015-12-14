name msadp32
type win32
file msadp32.acm

import winmm.dll
import user32.dll
import kernel32.dll
import ntdll.dll


@ stdcall DriverProc (long long long long long) ADPCM_DriverProc
