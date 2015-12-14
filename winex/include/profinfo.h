

#ifndef __WINE_PROFINFO_H
#define __WINE_PROFINFO_H

typedef struct _PROFILEINFOA {
    DWORD dwSize;
    DWORD dwFlags;
    LPSTR lpUserName;
    LPSTR lpProfilePath;
    LPSTR lpDefaultPath;
    LPSTR lpServerName;
    LPSTR lpPolicyPath;
    HANDLE hProfile;
} PROFILEINFOA, *LPPROFILEINFOA;

typedef struct _PROFILEINFOW {
    DWORD dwSize;
    DWORD dwFlags;
    LPWSTR lpUserName;
    LPWSTR lpProfilePath;
    LPWSTR lpDefaultPath;
    LPWSTR lpServerName;
    LPWSTR lpPolicyPath;
    HANDLE hProfile;
} PROFILEINFOW, *LPPROFILEINFOW;

DECL_WINELIB_TYPE_AW(PROFILEINFO)
DECL_WINELIB_TYPE_AW(LPPROFILEINFO)

#endif 
