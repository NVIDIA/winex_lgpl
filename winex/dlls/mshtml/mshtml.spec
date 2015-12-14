name	mshtml
type	win32
init	DllMain
rsrc	rsrc.res

import urlmon.dll
import shlwapi.dll
import ole32.dll
import oleaut32.dll
import user32.dll
import gdi32.dll
import advapi32.dll
import kernel32.dll

@ stub CreateHTMLPropertyPage
@ stdcall DllCanUnloadNow()
@ stub DllEnumClassObjects
@ stdcall DllGetClassObject(ptr ptr ptr)
@ stdcall DllInstall(long wstr)
@ stdcall DllRegisterServer()
@ stdcall DllUnregisterServer()
@ stub MatchExactGetIDsOfNames
@ stub PrintHTML
@ stdcall RNIGetCompatibleVersion()
@ stdcall RunHTMLApplication(long long str long)
@ stdcall ShowHTMLDialog(ptr ptr ptr wstr ptr)
@ stub ShowModalDialog
@ stub ShowModelessHTMLDialog
@ stub SvrTri_ClearCache
@ stub SvrTri_GetDLText
@ stub SvrTri_NormalizeUA
@ stub com_ms_osp_ospmrshl_classInit
@ stub com_ms_osp_ospmrshl_copyToExternal
@ stub com_ms_osp_ospmrshl_releaseByValExternal
@ stub com_ms_osp_ospmrshl_toJava
