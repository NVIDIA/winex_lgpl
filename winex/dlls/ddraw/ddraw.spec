name ddraw
type win32
init DDRAW_DllMain

import user32.dll
import gdi32.dll
import kernel32.dll
import ntdll.dll

@ stub DDHAL32_VidMemAlloc
@ stub DDHAL32_VidMemFree
@ stub DDInternalLock
@ stub DDInternalUnlock
@ stub DSoundHelp
@ stub DirectDrawCreate
@ stub DirectDrawCreateClipper
@ stub DirectDrawCreateEx
@ stub DirectDrawEnumerateA
@ stub DirectDrawEnumerateW
@ stub DirectDrawEnumerateExA
@ stub DirectDrawEnumerateExW
@ stub DllCanUnloadNow
@ stub DllGetClassObject
@ stub GetNextMipMap
@ stub GetSurfaceFromDC
@ stub HeapVidMemAllocAligned
@ stub InternalLock
@ stub InternalUnlock
@ stub LateAllocateSurfaceMem
@ stub VidMemAlloc
@ stub VidMemAmountFree
@ stub VidMemFini
@ stub VidMemFree
@ stub VidMemInit
@ stub VidMemLargestFree
@ stub thk1632_ThunkData32
@ stub thk3216_ThunkData32
