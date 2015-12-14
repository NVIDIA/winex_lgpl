

#ifndef _WINE_WININETI_H_
#define _WINE_WININETI_H_


#include <schannel.h>
#define SECURITY_WIN32
#include <sspi.h>

#define INTERNET_FLAG_BGUPDATE          0x00000008
#define INTERNET_FLAG_FTP_FOLDER_VIEW   0x00000004
#define INTERNET_FLAGS_MASK_INTERNAL (INTERNET_FLAGS_MASK | \
                                      INTERNET_FLAG_FTP_FOLDER_VIEW)

typedef struct _INTERNET_CACHE_CONFIG_PATH_ENTRYA
{
    CHAR CachePath[MAX_PATH];
    DWORD dwCacheSize;
} INTERNET_CACHE_CONFIG_PATH_ENTRYA, *LPINTERNET_CACHE_CONFIG_PATH_ENTRYA;

typedef struct _INTERNET_CACHE_CONFIG_PATH_ENTRYW
{
    WCHAR CachePath[MAX_PATH];
    DWORD dwCacheSize;
} INTERNET_CACHE_CONFIG_PATH_ENTRYW, *LPINTERNET_CACHE_CONFIG_PATH_ENTRYW;

DECL_WINELIB_TYPE_AW(INTERNET_CACHE_CONFIG_PATH_ENTRY)
DECL_WINELIB_TYPE_AW(LPINTERNET_CACHE_CONFIG_PATH_ENTRY)

typedef struct _INTERNET_CACHE_CONFIG_INFOA
{
    DWORD dwStructSize;
    DWORD dwContainer;
    DWORD dwQuota;
    DWORD dwReserved4;
    BOOL fPerUser;
    DWORD dwSyncMode;
    DWORD dwNumCachePaths;
    union
    {
        struct
        {
            CHAR CachePath[MAX_PATH];
            DWORD dwCacheSize;
        } DUMMYSTRUCTNAME;
        INTERNET_CACHE_CONFIG_PATH_ENTRYA CachePaths[ANYSIZE_ARRAY];
    } DUMYUNIONNAME;
    DWORD dwNormalUsage;
    DWORD dwExemptUsage;
} INTERNET_CACHE_CONFIG_INFOA, *LPINTERNET_CACHE_CONFIG_INFOA;

typedef struct _INTERNET_CACHE_CONFIG_INFOW
{
    DWORD dwStructSize;
    DWORD dwContainer;
    DWORD dwQuota;
    DWORD dwReserved4;
    BOOL  fPerUser;
    DWORD dwSyncMode;
    DWORD dwNumCachePaths;
    union
    {
        struct
        {
            WCHAR CachePath[MAX_PATH];
            DWORD dwCacheSize;
        } DUMMYSTRUCTNAME;
        INTERNET_CACHE_CONFIG_PATH_ENTRYW CachePaths[ANYSIZE_ARRAY];
    } ;
    DWORD dwNormalUsage;
    DWORD dwExemptUsage;
} INTERNET_CACHE_CONFIG_INFOW, *LPINTERNET_CACHE_CONFIG_INFOW;

DECL_WINELIB_TYPE_AW(INTERNET_CACHE_CONFIG_INFO)
DECL_WINELIB_TYPE_AW(LPINTERNET_CACHE_CONFIG_INFO)


#ifdef __cplusplus
extern "C" {
#endif

DWORD       WINAPI DeleteIE3Cache(HWND,HINSTANCE,LPSTR,int);
BOOL        WINAPI GetUrlCacheConfigInfoA(LPINTERNET_CACHE_CONFIG_INFOA,LPDWORD,DWORD);
BOOL        WINAPI GetUrlCacheConfigInfoW(LPINTERNET_CACHE_CONFIG_INFOW,LPDWORD,DWORD);
#define     GetUrlCacheConfigInfo WINELIB_NAME_AW(GetUrlCacheConfigInfo)
BOOL        WINAPI InternetQueryFortezzaStatus(DWORD*,DWORD_PTR);
BOOL        WINAPI IsUrlCacheEntryExpiredA(LPCSTR,DWORD,FILETIME*);
BOOL        WINAPI IsUrlCacheEntryExpiredW(LPCWSTR,DWORD,FILETIME*);
#define     IsUrlCacheEntryExpired WINELIB_NAME_AW(IsUrlCacheEntryExpired)
BOOL        WINAPI SetUrlCacheConfigInfoA(LPINTERNET_CACHE_CONFIG_INFOA,DWORD);
BOOL        WINAPI SetUrlCacheConfigInfoW(LPINTERNET_CACHE_CONFIG_INFOW,DWORD);
#define     SetUrlCacheConfigInfo WINELIB_NAME_AW(SetUrlCacheConfigInfo)
BOOL        WINAPI InternetGetSecurityInfoByURLA(LPSTR, PCCERT_CHAIN_CONTEXT *, DWORD *);
BOOL        WINAPI InternetGetSecurityInfoByURLW(LPCWSTR,PCCERT_CHAIN_CONTEXT *,DWORD *);
#define     InternetGetSecurityInfoByURL WINELIB_NAME_AW(InternetGetSecurityInfoByURL)
BOOLAPI     FreeUrlCacheSpaceA(LPCSTR,DWORD,DWORD);
BOOLAPI     FreeUrlCacheSpaceW(LPCWSTR,DWORD,DWORD);
#define     FreeUrlCacheSpace WINELIB_NAME_AW(FreeUrlCacheSpace)
BOOL        WINAPI GetDiskInfoA(PCSTR,PDWORD,PDWORDLONG,PDWORDLONG);


#ifdef __cplusplus
}
#endif

#endif 
