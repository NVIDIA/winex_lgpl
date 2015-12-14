name icmp
type win32
init DllMain

import kernel32.dll
import ntdll.dll

@ stub IcmpCloseHandle
@ stub IcmpCreateFile
@ stub  IcmpParseReplies
@ stub  IcmpSendEcho
@ stub  IcmpSendEcho2
@ stub  do_echo_rep
@ stub  do_echo_req
@ stub  register_icmp


