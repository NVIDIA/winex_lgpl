name	netapi32
type    win32
init	NETAPI32_LibMain

#import	user32.dll
#import	advapi32.dll
import	kernel32.dll
import	ntdll.dll


1 stdcall Netbios(ptr) Netbios
@ stdcall NetApiBufferFree(ptr)
@ stub NetWkstaTransportEnum
@ stdcall NetStatisticsGet(wstr wstr long long ptr)
