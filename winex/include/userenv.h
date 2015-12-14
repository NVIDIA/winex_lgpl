

#ifndef __WINE_USERENV_H
#define __WINE_USERENV_H


#include <profinfo.h>

#define PT_TEMPORARY    0x00000001
#define PT_ROAMING      0x00000002
#define PT_MANDATORY    0x00000004

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI CreateEnvironmentBlock(LPVOID*,HANDLE,BOOL);
BOOL WINAPI ExpandEnvironmentStringsForUserA(HANDLE,LPCSTR,LPSTR,DWORD);
BOOL WINAPI ExpandEnvironmentStringsForUserW(HANDLE,LPCWSTR,LPWSTR,DWORD);
#define     ExpandEnvironmentStringsForUser WINELIB_NAME_AW(ExpandEnvironmentStringsForUser)
BOOL WINAPI GetUserProfileDirectoryA(HANDLE,LPSTR,LPDWORD);
BOOL WINAPI GetUserProfileDirectoryW(HANDLE,LPWSTR,LPDWORD);
#define     GetUserProfileDirectory WINELIB_NAME_AW(GetUserProfileDirectory)
BOOL WINAPI GetProfilesDirectoryA(LPSTR,LPDWORD);
BOOL WINAPI GetProfilesDirectoryW(LPWSTR,LPDWORD);
#define     GetProfilesDirectory WINELIB_NAME_AW(GetProfilesDirectory)
BOOL WINAPI GetProfileType(DWORD*);
BOOL WINAPI LoadUserProfileA(HANDLE,LPPROFILEINFOA);
BOOL WINAPI LoadUserProfileW(HANDLE,LPPROFILEINFOW);
#define     LoadUserProfile WINELIB_NAME_AW(LoadUserProfile)
BOOL WINAPI RegisterGPNotification(HANDLE,BOOL);
BOOL WINAPI UnloadUserProfile(HANDLE,HANDLE);
BOOL WINAPI UnregisterGPNotification(HANDLE);

#ifdef __cplusplus
}
#endif

#endif  
