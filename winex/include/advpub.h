#ifndef _ADVPUB_H
#define _ADVPUB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CabInfo {
    PSTR  pszCab;
    PSTR  pszInf;
    PSTR  pszSection;
    char  szSrcPath[MAX_PATH];
    DWORD dwFlags;
} CABINFO, *PCABINFO;

typedef struct _StrEntry {
    LPSTR pszName;
    LPSTR pszValue;
} STRENTRY, STRENTRYA, *LPSTRENTRY, *LPSTRENTRYA;

typedef const STRENTRY CSTRENTRY;
typedef CSTRENTRY *LPCSTRENTRY;

typedef struct _StrTable {
    DWORD cEntries;
    LPSTRENTRY pse;
} STRTABLE, STRTABLEA, *LPSTRTABLE, *LPSTRTABLEA;

typedef const STRTABLE CSTRTABLE;
typedef CSTRTABLE *LPCSTRTABLE;

#define RSC_FLAG_INF                0x00000001
#define RSC_FLAG_SKIPDISKSPACECHECK 0x00000002
#define RSC_FLAG_QUIET              0x00000004
#define RSC_FLAG_NGCONV             0x00000008
#define RSC_FLAG_UPDHLPDLLS         0x00000010
#define RSC_FLAG_DELAYREGISTEROCX   0x00000200
#define RSC_FLAG_SETUPAPI           0x00000400
HRESULT WINAPI RunSetupCommand(HWND hWnd,
     LPCSTR szCmdName, LPCSTR szInfSection, LPCSTR szDir, LPCSTR lpszTitle,
     HANDLE *phEXE, DWORD dwFlags, LPVOID pvReserved);

#define ADN_DEL_IF_EMPTY            0x00000001
#define ADN_DONT_DEL_SUBDIRS        0x00000002
#define ADN_DONT_DEL_DIR            0x00000004
#define ADN_DEL_UNC_PATHS           0x00000008  
HRESULT WINAPI DelNode(LPCSTR pszFileOrDirName, DWORD dwFlags);

DWORD WINAPI NeedRebootInit(VOID);
BOOL WINAPI NeedReboot(DWORD dwRebootCheck);
BOOL WINAPI IsNTAdmin( DWORD dwReserved, DWORD *lpdwReserved );
HRESULT WINAPI RegInstall(HMODULE hm, LPCSTR pszSection, LPCSTRTABLE pstTable);
HRESULT WINAPI GetVersionFromFile(LPSTR lpszFilename, LPDWORD pdwMSVer, LPDWORD pdwLSVer, BOOL bVersion);
HRESULT WINAPI GetVersionFromFileEx(LPSTR lpszFilename, LPDWORD pdwMSVer, LPDWORD pdwLSVer, BOOL bVersion);

#ifdef __cplusplus
}
#endif

#endif /* _ADVPUB_H */
