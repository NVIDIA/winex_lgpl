name jsproxy
type win32
init DllMain

import oleaut32
import ole32
import kernel32

@ stdcall InternetInitializeAutoProxyDll(long str str ptr ptr) JSPROXY_InternetInitializeAutoProxyDll
@ stub InternetInitializeExAutoProxyDll
@ stdcall InternetDeInitializeAutoProxyDll(str long)
@ stub InternetDeInitializeExAutoProxyDll
@ stdcall InternetGetProxyInfo(str long str long ptr ptr)
