name winealsa
file winealsa.drv
type win32

import advapi32.dll
import winmm.dll
import user32.dll
import kernel32.dll
import ntdll.dll
import msacm32.dll


@ stdcall DriverProc(long long long long long) ALSA_DriverProc
@ stdcall widMessage(long long long long long) ALSA_widMessage
@ stdcall wodMessage(long long long long long) ALSA_wodMessage
@ stdcall midMessage(long long long long long) ALSA_midMessage
@ stdcall modMessage(long long long long long) ALSA_modMessage
