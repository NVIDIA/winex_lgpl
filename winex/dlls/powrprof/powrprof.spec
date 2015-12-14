name	powrprof
type	win32
init	DllMain

import advapi32.dll
import kernel32.dll
import ntdll.dll


@ stdcall CallNtPowerInformation(long ptr long ptr long) CallNtPowerInformation
@ stdcall CanUserWritePwrScheme() CanUserWritePwrScheme
@ stdcall DeletePwrScheme(long) DeletePwrScheme
@ stdcall EnumPwrSchemes(ptr long) EnumPwrSchemes
@ stdcall GetActivePwrScheme(ptr) GetActivePwrScheme
@ stdcall GetCurrentPowerPolicies(ptr ptr) GetCurrentPowerPolicies
@ stdcall GetPwrCapabilities(ptr) GetPwrCapabilities
@ stdcall GetPwrDiskSpindownRange(ptr ptr) GetPwrDiskSpindownRange
@ stdcall IsAdminOverrideActive(ptr) IsAdminOverrideActive
@ stdcall IsPwrHibernateAllowed() IsPwrHibernateAllowed
@ stdcall IsPwrShutdownAllowed() IsPwrShutdownAllowed
@ stdcall IsPwrSuspendAllowed() IsPwrSuspendAllowed
@ stdcall ReadGlobalPwrPolicy(ptr) ReadGlobalPwrPolicy
@ stdcall ReadProcessorPwrScheme(long ptr) ReadProcessorPwrScheme
@ stdcall ReadPwrScheme(long ptr) ReadPwrScheme
@ stdcall SetActivePwrScheme(long ptr ptr) SetActivePwrScheme
@ stdcall SetSuspendState(long long long) SetSuspendState
@ stdcall WriteGlobalPwrPolicy(ptr) WriteGlobalPwrPolicy
@ stdcall WriteProcessorPwrScheme(long ptr) WriteProcessorPwrScheme
@ stdcall WritePwrScheme(ptr wstr wstr ptr) WritePwrScheme
