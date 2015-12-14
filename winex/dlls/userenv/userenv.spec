name	userenv
type	win32
init	DllMain

import kernel32.dll
import ntdll.dll

@ stdcall CreateEnvironmentBlock(ptr ptr long)
@ stub DestroyEnvironmentBlock
@ stdcall ExpandEnvironmentStringsForUserA(ptr str ptr long)
@ stdcall ExpandEnvironmentStringsForUserW(ptr wstr ptr long)
@ stdcall GetProfilesDirectoryA(ptr ptr)
@ stdcall GetProfilesDirectoryW(ptr ptr)
@ stdcall GetProfileType(ptr)
@ stdcall GetUserProfileDirectoryA(ptr ptr ptr)
@ stdcall GetUserProfileDirectoryW(ptr ptr ptr)
@ stdcall LoadUserProfileA(ptr ptr)
@ stub LoadUserProfileW
@ stdcall RegisterGPNotification(long long)
@ stdcall UnloadUserProfile(ptr ptr)
@ stdcall UnregisterGPNotification(long)
