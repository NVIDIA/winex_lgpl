

#ifndef __WINE_SFC_H
#define __WINE_SFC_H

#ifdef __cplusplus
extern "C" {
#endif



typedef struct _PROTECTED_FILE_DATA {
 WCHAR FileName[MAX_PATH];
 DWORD FileNumber;
} PROTECTED_FILE_DATA, *PPROTECTED_FILE_DATA;



BOOL WINAPI SfcGetNextProtectedFile(HANDLE, PPROTECTED_FILE_DATA);
BOOL WINAPI SfcIsFileProtected(HANDLE, LPCWSTR);
BOOL WINAPI SfpVerifyFile(LPCSTR, LPSTR, DWORD);

#ifdef __cplusplus
}
#endif

#endif
