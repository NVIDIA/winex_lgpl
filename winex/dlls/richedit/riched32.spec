name	riched32
type	win32
init	DllMain
rsrc	version.res

import riched20.dll
import user32.dll
import kernel32.dll

debug_channels (richedit)

2 stdcall DllGetVersion(ptr) DllGetVersion
