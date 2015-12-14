name	sensapi
type	win32
init	DllMain

import kernel32.dll

debug_channels (sensapi)

@ stdcall IsDestinationReachableA(str ptr) IsDestinationReachableA
@ stdcall IsDestinationReachableW(wstr ptr) IsDestinationReachableW
@ stdcall IsNetworkAlive(ptr) IsNetworkAlive
