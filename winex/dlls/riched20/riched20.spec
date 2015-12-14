name	riched20
type	win32
init	DllMain
rsrc	version.res

import ole32.dll
import user32.dll
import gdi32.dll
import kernel32.dll

debug_channels (richedit richedit_check richedit_lists richedit_style)

2 extern IID_IRichEditOle IID_IRichEditOle
3 extern IID_IRichEditOleCallback IID_IRichEditOleCallback
4 stdcall CreateTextServices(ptr ptr ptr) CreateTextServices
5 extern IID_ITextServices IID_ITextServices
6 extern IID_ITextHost IID_ITextHost
7 extern IID_ITextHost2 IID_ITextHost2
8 stdcall REExtendedRegisterClass() REExtendedRegisterClass
9 stdcall RichEdit10ANSIWndProc(ptr long long long) RichEdit10ANSIWndProc
10 stdcall RichEditANSIWndProc(ptr long long long) RichEditANSIWndProc
