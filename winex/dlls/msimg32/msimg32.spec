name msimg32
type win32

import ntdll.dll
import gdi32.dll


@ stdcall AlphaBlend(long long long long long long long long long long long) AlphaBlend
@ stub DllInitialize
@ stdcall GradientFill(long ptr long ptr long long) GradientFill
@ stdcall TransparentBlt(long long long long long long long long long long long) TransparentBlt
@ stub vSetDdrawflag
