name    psapi
type    win32

import	kernel32.dll
import	ntdll.dll


@ stdcall EmptyWorkingSet(long) EmptyWorkingSet
@ stdcall EnumDeviceDrivers(ptr long ptr) EnumDeviceDrivers
@ stdcall EnumPageFilesA(ptr ptr)
@ stdcall EnumPageFilesW(ptr ptr)
@ stdcall EnumProcessModules(long ptr long ptr) EnumProcessModules
@ stdcall EnumProcesses(ptr long ptr) EnumProcesses
@ stdcall GetDeviceDriverBaseNameA(ptr str long) GetDeviceDriverBaseNameA
@ stdcall GetDeviceDriverBaseNameW(ptr wstr long) GetDeviceDriverBaseNameW
@ stdcall GetDeviceDriverFileNameA(ptr str long) GetDeviceDriverFileNameA
@ stdcall GetDeviceDriverFileNameW(ptr wstr long) GetDeviceDriverFileNameW
@ stdcall GetMappedFileNameA(long ptr str long) GetMappedFileNameA
@ stdcall GetMappedFileNameW(long ptr wstr long) GetMappedFileNameW
@ stdcall GetModuleBaseNameA(long long str long) GetModuleBaseNameA
@ stdcall GetModuleBaseNameW(long long wstr long) GetModuleBaseNameW
@ stdcall GetModuleFileNameExA(long long str long) GetModuleFileNameExA 
@ stdcall GetModuleFileNameExW(long long wstr long) GetModuleFileNameExW
@ stdcall GetModuleInformation(long long ptr long) GetModuleInformation
@ stdcall GetProcessMemoryInfo(long ptr long) GetProcessMemoryInfo
@ stdcall GetWsChanges(long ptr long) GetWsChanges
@ stdcall InitializeProcessForWsWatch(long) InitializeProcessForWsWatch

@ stdcall QueryWorkingSet(long ptr long) QueryWorkingSet
