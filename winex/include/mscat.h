

#ifndef __WINE_MSCAT_H
#define __WINE_MSCAT_H

#include <mssip.h>

typedef HANDLE HCATADMIN;
typedef HANDLE HCATINFO;

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTCAT_OPEN_CREATENEW             0x00000001
#define CRYPTCAT_OPEN_ALWAYS                0x00000002
#define CRYPTCAT_OPEN_EXISTING              0x00000004
#define CRYPTCAT_OPEN_EXCLUDE_PAGE_HASHES   0x00010000
#define CRYPTCAT_OPEN_INCLUDE_PAGE_HASHES   0x00020000
#define CRYPTCAT_OPEN_VERIFYSIGHASH         0x10000000
#define CRYPTCAT_OPEN_NO_CONTENT_HCRYPTMSG  0x20000000
#define CRYPTCAT_OPEN_SORTED                0x40000000
#define CRYPTCAT_OPEN_FLAGS_MASK            0xffff0000

#define CRYPTCAT_E_AREA_HEADER              0x00000000
#define CRYPTCAT_E_AREA_MEMBER              0x00010000
#define CRYPTCAT_E_AREA_ATTRIBUTE           0x00020000

#define CRYPTCAT_E_CDF_UNSUPPORTED          0x00000001
#define CRYPTCAT_E_CDF_DUPLICATE            0x00000002
#define CRYPTCAT_E_CDF_TAGNOTFOUND          0x00000004

#define CRYPTCAT_E_CDF_MEMBER_FILE_PATH     0x00010001
#define CRYPTCAT_E_CDF_MEMBER_INDIRECTDATA  0x00010002
#define CRYPTCAT_E_CDF_MEMBER_FILENOTFOUND  0x00010004

#define CRYPTCAT_E_CDF_BAD_GUID_CONV        0x00020001
#define CRYPTCAT_E_CDF_ATTR_TOOFEWVALUES    0x00020002
#define CRYPTCAT_E_CDF_ATTR_TYPECOMBO       0x00020004

#include <pshpack8.h>

typedef struct CRYPTCATATTRIBUTE_
{
    DWORD cbStruct;
    LPWSTR pwszReferenceTag;
    DWORD dwAttrTypeAndAction;
    DWORD cbValue;
    BYTE *pbValue;
    DWORD dwReserved;
} CRYPTCATATTRIBUTE;

typedef struct CRYPTCATMEMBER_
{
    DWORD cbStruct;
    LPWSTR pwszReferenceTag;
    LPWSTR pwszFileName;
    GUID gSubjectType;
    DWORD fdwMemberFlags;
    struct SIP_INDIRECT_DATA_* pIndirectData;
    DWORD dwCertVersion;
    DWORD dwReserved;
    HANDLE hReserved;
    CRYPT_ATTR_BLOB sEncodedIndirectData;
    CRYPT_ATTR_BLOB sEncodedMemberInfo;
} CRYPTCATMEMBER;

typedef struct CATALOG_INFO_
{
    DWORD cbStruct;
    WCHAR wszCatalogFile[MAX_PATH];
} CATALOG_INFO;

typedef struct CRYPTCATCDF_
{
    DWORD cbStruct;
    HANDLE hFile;
    DWORD dwCurFilePos;
    DWORD dwLastMemberOffset;
    BOOL fEOF;
    LPWSTR pwszResultDir;
    HANDLE hCATStore;
} CRYPTCATCDF;

#include <poppack.h>

typedef void (WINAPI *PFN_CDF_PARSE_ERROR_CALLBACK)(DWORD, DWORD, WCHAR *);

BOOL      WINAPI CryptCATAdminAcquireContext(HCATADMIN*,const GUID*,DWORD);
HCATINFO  WINAPI CryptCATAdminAddCatalog(HCATADMIN,PWSTR,PWSTR,DWORD);
BOOL      WINAPI CryptCATAdminCalcHashFromFileHandle(HANDLE,DWORD*,BYTE*,DWORD);
HCATINFO  WINAPI CryptCATAdminEnumCatalogFromHash(HCATADMIN,BYTE*,DWORD,DWORD,HCATINFO*);
BOOL      WINAPI CryptCATAdminReleaseCatalogContext(HCATADMIN,HCATINFO,DWORD);
BOOL      WINAPI CryptCATAdminReleaseContext(HCATADMIN,DWORD);
BOOL      WINAPI CryptCATAdminRemoveCatalog(HCATADMIN,LPCWSTR,DWORD);
BOOL      WINAPI CryptCATAdminResolveCatalogPath(HCATADMIN, WCHAR *, CATALOG_INFO *, DWORD);
BOOL      WINAPI CryptCATCatalogInfoFromContext(HCATINFO, CATALOG_INFO *, DWORD);
BOOL      WINAPI CryptCATCDFClose(CRYPTCATCDF *);
CRYPTCATATTRIBUTE * WINAPI CryptCATCDFEnumCatAttributes(CRYPTCATCDF *, CRYPTCATATTRIBUTE *,
                                                        PFN_CDF_PARSE_ERROR_CALLBACK);
LPWSTR              WINAPI CryptCATCDFEnumMembersByCDFTagEx(CRYPTCATCDF *, LPWSTR,
                                                            PFN_CDF_PARSE_ERROR_CALLBACK,
                                                            CRYPTCATMEMBER **, BOOL, LPVOID);
CRYPTCATCDF       * WINAPI CryptCATCDFOpen(LPWSTR, PFN_CDF_PARSE_ERROR_CALLBACK);
BOOL                WINAPI CryptCATClose(HANDLE);
CRYPTCATATTRIBUTE * WINAPI CryptCATEnumerateAttr(HANDLE, CRYPTCATMEMBER *, CRYPTCATATTRIBUTE *);
CRYPTCATATTRIBUTE * WINAPI CryptCATEnumerateCatAttr(HANDLE, CRYPTCATATTRIBUTE *);
CRYPTCATMEMBER    * WINAPI CryptCATEnumerateMember(HANDLE,CRYPTCATMEMBER *);
CRYPTCATATTRIBUTE * WINAPI CryptCATGetAttrInfo(HANDLE, CRYPTCATMEMBER *, LPWSTR);
CRYPTCATATTRIBUTE * WINAPI CryptCATGetCatAttrInfo(HANDLE, LPWSTR);
CRYPTCATMEMBER    * WINAPI CryptCATGetMemberInfo(HANDLE, LPWSTR);
HANDLE    WINAPI CryptCATOpen(LPWSTR,DWORD,HCRYPTPROV,DWORD,DWORD);

#ifdef __cplusplus
}
#endif

#endif
