name mciwave
file mciwave.drv
type win32

import winmm.dll
import -delay user32.dll
import kernel32.dll
import ntdll.dll


@ stdcall DriverProc(long long long long long) MCIWAVE_DriverProc
