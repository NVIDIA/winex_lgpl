name	cabinet
type	win32
rsrc	cabinet.res

import kernel32.dll


1 stub GetDllVersion
2 stdcall DllGetVersion(ptr) DllGetVersion
3 stdcall Extract(ptr str) Extract
4 stub DeleteExtractedFiles
10 cdecl FCICreate(ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr) FCICreate
11 cdecl FCIAddFile(long ptr ptr long ptr ptr ptr long) FCIAddFile
12 cdecl FCIFlushFolder(long ptr ptr) FCIFlushFolder
13 cdecl FCIFlushCabinet(long long ptr ptr) FCIFlushCabinet
14 cdecl FCIDestroy(long) FCIDestroy
20 cdecl FDICreate(ptr ptr ptr ptr ptr ptr ptr long ptr) FDICreate
21 cdecl FDIIsCabinet(long long ptr) FDIIsCabinet
22 cdecl FDICopy(long ptr ptr long ptr ptr ptr) FDICopy
23 cdecl FDIDestroy(long) FDIDestroy
24 cdecl FDITruncateCabinet(long ptr long) FDITruncateCabinet
