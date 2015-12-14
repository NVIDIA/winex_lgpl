name dsound
type win32
init DSOUND_DllMain
rsrc version.res

import winmm.dll
import kernel32.dll
import ntdll.dll
import user32.dll


0 stub DirectSoundUnknown
1 stdcall DirectSoundCreate(ptr ptr ptr) DirectSoundCreate8
2 stdcall DirectSoundEnumerateA(ptr ptr) DirectSoundEnumerateA
3 stdcall DirectSoundEnumerateW(ptr ptr) DirectSoundEnumerateW
4 stdcall DllCanUnloadNow() DSOUND_DllCanUnloadNow
5 stdcall DllGetClassObject(ptr ptr ptr) DSOUND_DllGetClassObject
6 stdcall DirectSoundCaptureCreate(ptr ptr ptr) DirectSoundCaptureCreate8
7 stdcall DirectSoundCaptureEnumerateA(ptr ptr) DirectSoundCaptureEnumerateA
8 stdcall DirectSoundCaptureEnumerateW(ptr ptr) DirectSoundCaptureEnumerateW
9 stdcall GetDeviceID(ptr ptr) GetDeviceID
10 stub DirectSoundFullDuplexCreate
11 stdcall DirectSoundCreate8(ptr ptr ptr) DirectSoundCreate8
12 stdcall DirectSoundCaptureCreate8(ptr ptr ptr) DirectSoundCaptureCreate8
