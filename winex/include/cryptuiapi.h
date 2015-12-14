#ifndef __CRYPTUIAPI_H__
#define __CRYPTUIAPI_H__

#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <wintrust.h>
#include <wincrypt.h>
#include <prsht.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <pshpack8.h>

#define CERT_CREDENTIAL_PROVIDER_ID            -509

BOOL
WINAPI
CryptUIDlgViewContext(
    IN DWORD dwContextType,
    IN const void *pvContext,
    IN OPTIONAL HWND hwnd,
    IN OPTIONAL LPCWSTR pwszTitle,
    IN DWORD dwFlags,
    IN void *pvReserved
    );


PCCERT_CONTEXT
WINAPI
CryptUIDlgSelectCertificateFromStore(
    IN HCERTSTORE hCertStore,
    IN OPTIONAL HWND hwnd,
    IN OPTIONAL LPCWSTR pwszTitle,
    IN OPTIONAL LPCWSTR pwszDisplayString,
    IN DWORD dwDontUseColumn,
    IN DWORD dwFlags,
    IN void *pvReserved
    );

#define CRYPTUI_SELECT_ISSUEDTO_COLUMN                   0x000000001
#define CRYPTUI_SELECT_ISSUEDBY_COLUMN                   0x000000002
#define CRYPTUI_SELECT_INTENDEDUSE_COLUMN                0x000000004
#define CRYPTUI_SELECT_FRIENDLYNAME_COLUMN               0x000000008
#define CRYPTUI_SELECT_LOCATION_COLUMN                   0x000000010
#define CRYPTUI_SELECT_EXPIRATION_COLUMN                 0x000000020

typedef BOOL (WINAPI * PFNCFILTERPROC) (
        PCCERT_CONTEXT  pCertContext,
        BOOL            *pfInitialSelectedCert,
        void            *pvCallbackData
        );

typedef struct {
    HCERTSTORE hStore;
    PCCERT_CHAIN_CONTEXT * prgpChain;
    DWORD cChain;
}CERT_SELECTUI_INPUT, *PCERT_SELECTUI_INPUT;

HRESULT
WINAPI
CertSelectionGetSerializedBlob(
            IN PCERT_SELECTUI_INPUT pcsi,
            OUT void ** ppOutBuffer,
            OUT ULONG *pulOutBufferSize);

#define CRYPTUI_CERT_MGR_TAB_MASK                       0x0000000F
#define CRYPTUI_CERT_MGR_PUBLISHER_TAB                  0x00000004
#define CRYPTUI_CERT_MGR_SINGLE_TAB_FLAG                0x00008000

typedef struct _CRYPTUI_CERT_MGR_STRUCT
{
    DWORD               dwSize;
    HWND                hwndParent;
    DWORD               dwFlags;
    LPCWSTR             pwszTitle;
    LPCSTR              pszInitUsageOID;
} CRYPTUI_CERT_MGR_STRUCT, *PCRYPTUI_CERT_MGR_STRUCT;

typedef const CRYPTUI_CERT_MGR_STRUCT *PCCRYPTUI_CERT_MGR_STRUCT;


BOOL
WINAPI
CryptUIDlgCertMgr(
    IN                  PCCRYPTUI_CERT_MGR_STRUCT pCryptUICertMgr
    );
        
typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_BLOB_INFO
{
    DWORD               dwSize;			
    GUID                *pGuidSubject;
    DWORD               cbBlob;				
    BYTE                *pbBlob;			
    LPCWSTR             pwszDisplayName;
} CRYPTUI_WIZ_DIGITAL_SIGN_BLOB_INFO, *PCRYPTUI_WIZ_DIGITAL_SIGN_BLOB_INFO;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_BLOB_INFO *PCCRYPTUI_WIZ_DIGITAL_SIGN_BLOB_INFO;

typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_STORE_INFO
{
    DWORD               dwSize;	
    DWORD               cCertStore;			
    HCERTSTORE          *rghCertStore;
    PFNCFILTERPROC      pFilterCallback;
    void *              pvCallbackData;
} CRYPTUI_WIZ_DIGITAL_SIGN_STORE_INFO, *PCRYPTUI_WIZ_DIGITAL_SIGN_STORE_INFO;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_STORE_INFO *PCCRYPTUI_WIZ_DIGITAL_SIGN_STORE_INFO;

typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE_INFO
{
    DWORD               dwSize;
    LPWSTR              pwszPvkFileName;
    LPWSTR              pwszProvName;
    DWORD               dwProvType;
} CRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE_INFO, *PCRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE_INFO;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE_INFO *PCCRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE_INFO;

#define CRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE                0x01
#define CRYPTUI_WIZ_DIGITAL_SIGN_PVK_PROV                0x02

typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_CERT_PVK_INFO
{
    DWORD                                         dwSize;
    LPWSTR                                        pwszSigningCertFileName;
    DWORD                                         dwPvkChoice;		
    union
    {
        PCCRYPTUI_WIZ_DIGITAL_SIGN_PVK_FILE_INFO  pPvkFileInfo;
        PCRYPT_KEY_PROV_INFO                      pPvkProvInfo;
    } DUMMYUNIONNAME;

} CRYPTUI_WIZ_DIGITAL_SIGN_CERT_PVK_INFO, *PCRYPTUI_WIZ_DIGITAL_SIGN_CERT_PVK_INFO;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_CERT_PVK_INFO *PCCRYPTUI_WIZ_DIGITAL_SIGN_CERT_PVK_INFO;

#define CRYPTUI_WIZ_DIGITAL_SIGN_COMMERCIAL              0x0001
#define CRYPTUI_WIZ_DIGITAL_SIGN_INDIVIDUAL              0x0002

typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO
{
    DWORD                   dwSize;			
    DWORD                   dwAttrFlags;
    LPCWSTR                 pwszDescription;
    LPCWSTR                 pwszMoreInfoLocation;		
    LPCSTR                  pszHashAlg;
    LPCWSTR                 pwszSigningCertDisplayString;
    HCERTSTORE              hAdditionalCertStore;
    PCRYPT_ATTRIBUTES       psAuthenticated;	
    PCRYPT_ATTRIBUTES       psUnauthenticated;	
} CRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO, *PCRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO *PCCRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO;

#define CRYPTUI_WIZ_DIGITAL_SIGN_SUBJECT_FILE            0x01
#define CRYPTUI_WIZ_DIGITAL_SIGN_SUBJECT_BLOB            0x02

#define CRYPTUI_WIZ_DIGITAL_SIGN_CERT                    0x01
#define CRYPTUI_WIZ_DIGITAL_SIGN_STORE                   0x02
#define CRYPTUI_WIZ_DIGITAL_SIGN_PVK                     0x03

#define CRYPTUI_WIZ_DIGITAL_SIGN_ADD_CHAIN               0x00000001
#define CRYPTUI_WIZ_DIGITAL_SIGN_ADD_CHAIN_NO_ROOT       0x00000002

typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_INFO
{
    DWORD                                           dwSize;			
    DWORD                                           dwSubjectChoice;	
    union
    {
        LPCWSTR                                     pwszFileName;	
        PCCRYPTUI_WIZ_DIGITAL_SIGN_BLOB_INFO        pSignBlobInfo;	
    } DUMMYUNIONNAME1;
    DWORD                                           dwSigningCertChoice;
    union
    {
        PCCERT_CONTEXT                              pSigningCertContext;
        PCCRYPTUI_WIZ_DIGITAL_SIGN_STORE_INFO       pSigningCertStore;
        PCCRYPTUI_WIZ_DIGITAL_SIGN_CERT_PVK_INFO    pSigningCertPvkInfo;
    } DUMMYUNIONNAME2;
    LPCWSTR                                         pwszTimestampURL;
    DWORD                                           dwAdditionalCertChoice;
    PCCRYPTUI_WIZ_DIGITAL_SIGN_EXTENDED_INFO        pSignExtInfo;
} CRYPTUI_WIZ_DIGITAL_SIGN_INFO, *PCRYPTUI_WIZ_DIGITAL_SIGN_INFO;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_INFO *PCCRYPTUI_WIZ_DIGITAL_SIGN_INFO;

typedef struct _CRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT
{
    DWORD               dwSize;			
    DWORD               cbBlob;				
    BYTE                *pbBlob;			
} CRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT, *PCRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT;

typedef const CRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT *PCCRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT;

#define CRYPTUI_WIZ_NO_UI                               0x0001
#define CRYPTUI_WIZ_DIGITAL_SIGN_EXCLUDE_PAGE_HASHES    0x0002

#define CRYPTUI_WIZ_DIGITAL_SIGN_INCLUDE_PAGE_HASHES    0x0004

BOOL
WINAPI
CryptUIWizDigitalSign(
    IN                  DWORD                               dwFlags,
    IN     OPTIONAL     HWND                                hwndParent,
    IN     OPTIONAL     LPCWSTR                             pwszWizardTitle,
    IN                  PCCRYPTUI_WIZ_DIGITAL_SIGN_INFO     pDigitalSignInfo,
    OUT    OPTIONAL     PCCRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT *ppSignContext
    );


BOOL
WINAPI
CryptUIWizFreeDigitalSignContext(
    IN                  PCCRYPTUI_WIZ_DIGITAL_SIGN_CONTEXT  pSignContext
    );
     

#define CRYPTUI_HIDE_HIERARCHYPAGE          0x00000001
#define CRYPTUI_HIDE_DETAILPAGE             0x00000002
#define CRYPTUI_DISABLE_EDITPROPERTIES      0x00000004
#define CRYPTUI_ENABLE_EDITPROPERTIES       0x00000008
#define CRYPTUI_DISABLE_ADDTOSTORE          0x00000010
#define CRYPTUI_ENABLE_ADDTOSTORE           0x00000020
#define CRYPTUI_ACCEPT_DECLINE_STYLE        0x00000040
#define CRYPTUI_IGNORE_UNTRUSTED_ROOT       0x00000080
#define CRYPTUI_DONT_OPEN_STORES            0x00000100
#define CRYPTUI_ONLY_OPEN_ROOT_STORE        0x00000200
#define CRYPTUI_WARN_UNTRUSTED_ROOT         0x00000400
#define CRYPTUI_ENABLE_REVOCATION_CHECKING  0x00000800
#define CRYPTUI_WARN_REMOTE_TRUST           0x00001000
#define CRYPTUI_DISABLE_EXPORT              0x00002000
                                                                
#define CRYPTUI_ENABLE_REVOCATION_CHECK_END_CERT           0x00004000
#define CRYPTUI_ENABLE_REVOCATION_CHECK_CHAIN              0x00008000
#define CRYPTUI_ENABLE_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT CRYPTUI_ENABLE_REVOCATION_CHECKING
        
#define CRYPTUI_DISABLE_HTMLLINK                           0x00010000
#define CRYPTUI_DISABLE_ISSUERSTATEMENT                    0x00020000

#define CRYPTUI_CACHE_ONLY_URL_RETRIEVAL                   0x00040000

typedef struct tagCRYPTUI_INITDIALOG_STRUCT {
    LPARAM          lParam;
    PCCERT_CONTEXT  pCertContext;
} CRYPTUI_INITDIALOG_STRUCT, *PCRYPTUI_INITDIALOG_STRUCT;


typedef struct tagCRYPTUI_VIEWCERTIFICATE_STRUCTW {
    DWORD                       dwSize;
    HWND                        hwndParent;
    DWORD                       dwFlags;
    LPCWSTR                     szTitle;
    PCCERT_CONTEXT              pCertContext;
    LPCSTR *                    rgszPurposes;
    DWORD                       cPurposes;
    union
    {
        CRYPT_PROVIDER_DATA const * pCryptProviderData;
        HANDLE                      hWVTStateData;
    } DUMMYUNIONNAME;
    BOOL                        fpCryptProviderDataTrustedUsage;
    DWORD                       idxSigner;
    DWORD                       idxCert;
    BOOL                        fCounterSigner;
    DWORD                       idxCounterSigner;
    DWORD                       cStores;
    HCERTSTORE *                rghStores;
    DWORD                       cPropSheetPages;
    LPCPROPSHEETPAGEW           rgPropSheetPages;
    DWORD                       nStartPage;
} CRYPTUI_VIEWCERTIFICATE_STRUCTW, *PCRYPTUI_VIEWCERTIFICATE_STRUCTW;
typedef const CRYPTUI_VIEWCERTIFICATE_STRUCTW *PCCRYPTUI_VIEWCERTIFICATE_STRUCTW;


typedef struct tagCRYPTUI_VIEWCERTIFICATE_STRUCTA {
    DWORD                       dwSize;
    HWND                        hwndParent;
    DWORD                       dwFlags;
    LPCSTR                      szTitle;
    PCCERT_CONTEXT              pCertContext;
    LPCSTR *                    rgszPurposes;
    DWORD                       cPurposes;
    union
    {
        CRYPT_PROVIDER_DATA const * pCryptProviderData;
        HANDLE                      hWVTStateData;
    } DUMMYUNIONNAME;
    BOOL                        fpCryptProviderDataTrustedUsage;
    DWORD                       idxSigner;
    DWORD                       idxCert;
    BOOL                        fCounterSigner;
    DWORD                       idxCounterSigner;
    DWORD                       cStores;
    HCERTSTORE *                rghStores;
    DWORD                       cPropSheetPages;
    LPCPROPSHEETPAGEA           rgPropSheetPages;
    DWORD                       nStartPage;
} CRYPTUI_VIEWCERTIFICATE_STRUCTA, *PCRYPTUI_VIEWCERTIFICATE_STRUCTA;
typedef const CRYPTUI_VIEWCERTIFICATE_STRUCTA *PCCRYPTUI_VIEWCERTIFICATE_STRUCTA;

BOOL
WINAPI
CryptUIDlgViewCertificateW(
        IN  PCCRYPTUI_VIEWCERTIFICATE_STRUCTW   pCertViewInfo,
        OUT BOOL                                *pfPropertiesChanged
        );

BOOL
WINAPI
CryptUIDlgViewCertificateA(
        IN  PCCRYPTUI_VIEWCERTIFICATE_STRUCTA   pCertViewInfo,
        OUT BOOL                                *pfPropertiesChanged
        );

#ifdef UNICODE
#define CryptUIDlgViewCertificate           CryptUIDlgViewCertificateW
#define PCRYPTUI_VIEWCERTIFICATE_STRUCT     PCRYPTUI_VIEWCERTIFICATE_STRUCTW
#define CRYPTUI_VIEWCERTIFICATE_STRUCT      CRYPTUI_VIEWCERTIFICATE_STRUCTW
#define PCCRYPTUI_VIEWCERTIFICATE_STRUCT    PCCRYPTUI_VIEWCERTIFICATE_STRUCTW
#else
#define CryptUIDlgViewCertificate           CryptUIDlgViewCertificateA
#define PCRYPTUI_VIEWCERTIFICATE_STRUCT     PCRYPTUI_VIEWCERTIFICATE_STRUCTA
#define CRYPTUI_VIEWCERTIFICATE_STRUCT      CRYPTUI_VIEWCERTIFICATE_STRUCTA
#define PCCRYPTUI_VIEWCERTIFICATE_STRUCT    PCCRYPTUI_VIEWCERTIFICATE_STRUCTA
#endif

#define     CRYPTUI_WIZ_EXPORT_CERT_CONTEXT 			        1
#define     CRYPTUI_WIZ_EXPORT_CTL_CONTEXT  			        2
#define     CRYPTUI_WIZ_EXPORT_CRL_CONTEXT  			        3
#define     CRYPTUI_WIZ_EXPORT_CERT_STORE   			        4
#define     CRYPTUI_WIZ_EXPORT_CERT_STORE_CERTIFICATES_ONLY   	5
#define     CRYPTUI_WIZ_EXPORT_FORMAT_CRL                       6
#define     CRYPTUI_WIZ_EXPORT_FORMAT_CTL                       7

typedef struct _CRYPTUI_WIZ_EXPORT_INFO
{
    DWORD                   dwSize;
    LPCWSTR                 pwszExportFileName;
    DWORD                   dwSubjectChoice;
    union
    {
        PCCERT_CONTEXT      pCertContext;
        PCCTL_CONTEXT       pCTLContext;
        PCCRL_CONTEXT       pCRLContext;
        HCERTSTORE          hCertStore;
    } DUMMYUNIONNAME;

    DWORD                   cStores;
    HCERTSTORE *            rghStores;
}CRYPTUI_WIZ_EXPORT_INFO, *PCRYPTUI_WIZ_EXPORT_INFO;

typedef const CRYPTUI_WIZ_EXPORT_INFO *PCCRYPTUI_WIZ_EXPORT_INFO;


#define     CRYPTUI_WIZ_EXPORT_FORMAT_DER                   1
#define     CRYPTUI_WIZ_EXPORT_FORMAT_PFX                   2
#define     CRYPTUI_WIZ_EXPORT_FORMAT_PKCS7                 3
#define     CRYPTUI_WIZ_EXPORT_FORMAT_BASE64                4
#define     CRYPTUI_WIZ_EXPORT_FORMAT_SERIALIZED_CERT_STORE 5

typedef struct _CRYPTUI_WIZ_EXPORT_CERTCONTEXT_INFO
{
	DWORD					dwSize;
	DWORD					dwExportFormat;
    BOOL                    fExportChain;
    BOOL                    fExportPrivateKeys;
    LPCWSTR                 pwszPassword;
    BOOL                    fStrongEncryption;
}CRYPTUI_WIZ_EXPORT_CERTCONTEXT_INFO, *PCRYPTUI_WIZ_EXPORT_CERTCONTEXT_INFO;

typedef const CRYPTUI_WIZ_EXPORT_CERTCONTEXT_INFO *PCCRYPTUI_WIZ_EXPORT_CERTCONTEXT_INFO;

BOOL
WINAPI
CryptUIWizExport(
     DWORD                                  dwFlags,
     HWND                                   hwndParent,
     LPCWSTR                                pwszWizardTitle,
     PCCRYPTUI_WIZ_EXPORT_INFO              pExportInfo,
     void                                   *pvoid
);

#define     CRYPTUI_WIZ_IMPORT_SUBJECT_FILE                 1
#define     CRYPTUI_WIZ_IMPORT_SUBJECT_CERT_CONTEXT         2
#define     CRYPTUI_WIZ_IMPORT_SUBJECT_CTL_CONTEXT          3
#define     CRYPTUI_WIZ_IMPORT_SUBJECT_CRL_CONTEXT          4
#define     CRYPTUI_WIZ_IMPORT_SUBJECT_CERT_STORE           5

typedef struct _CRYPTUI_WIZ_IMPORT_SUBJECT_INFO
{
    DWORD                   dwSize;
    DWORD                   dwSubjectChoice;
    union
    {
        LPCWSTR             pwszFileName;	
        PCCERT_CONTEXT      pCertContext;
        PCCTL_CONTEXT       pCTLContext;
        PCCRL_CONTEXT       pCRLContext;
        HCERTSTORE          hCertStore;
    } DUMMYUNIONNAME;

    DWORD                   dwFlags;
    LPCWSTR                 pwszPassword;
}CRYPTUI_WIZ_IMPORT_SRC_INFO, *PCRYPTUI_WIZ_IMPORT_SRC_INFO;

typedef const CRYPTUI_WIZ_IMPORT_SRC_INFO *PCCRYPTUI_WIZ_IMPORT_SRC_INFO;

#define   CRYPTUI_WIZ_IMPORT_NO_CHANGE_DEST_STORE           0x00010000
#define   CRYPTUI_WIZ_IMPORT_ALLOW_CERT                     0x00020000
#define   CRYPTUI_WIZ_IMPORT_ALLOW_CRL                      0x00040000
#define   CRYPTUI_WIZ_IMPORT_ALLOW_CTL                      0x00080000
#define   CRYPTUI_WIZ_IMPORT_TO_LOCALMACHINE                0x00100000
#define   CRYPTUI_WIZ_IMPORT_TO_CURRENTUSER                 0x00200000
#define   CRYPTUI_WIZ_IMPORT_REMOTE_DEST_STORE              0x00400000

BOOL
WINAPI
CryptUIWizImport(
     DWORD                               dwFlags,
     HWND                                hwndParent,
     LPCWSTR                             pwszWizardTitle,
     PCCRYPTUI_WIZ_IMPORT_SRC_INFO       pImportSrc,
     HCERTSTORE                          hDestCertStore
);

typedef BOOL (WINAPI *PFNCCERTDISPLAYPROC)(PCCERT_CONTEXT pCertContext,
 HWND hWndSelCertDlg, void *pvCallbackData);

#define CRYPTUI_SELECTCERT_MULTISELECT 0x00000001

typedef struct _CRYPTUI_SELECTCERTIFICATE_STRUCTA
{
    DWORD               dwSize;
    HWND                hwndParent;
    DWORD               dwFlags;
    LPCSTR              szTitle;
    DWORD               dwDontUseColumn;
    LPCSTR              szDisplayString;
    PFNCFILTERPROC      pFilterCallback;
    PFNCCERTDISPLAYPROC pDisplayCallback;
    void               *pvCallbackData;
    DWORD               cDisplayStores;
    HCERTSTORE         *rghDisplayStores;
    DWORD               cStores;
    HCERTSTORE         *rghStores;
    DWORD               cPropSheetPages;
    LPCPROPSHEETPAGEA   rgPropSheetPages;
    HCERTSTORE          hSelectedCertStore;
} CRYPTUI_SELECTCERTIFICATE_STRUCTA, *PCRYPTUI_SELECTCERTIFICATE_STRUCTA;
typedef const CRYPTUI_SELECTCERTIFICATE_STRUCTA *
 PCCRYPTUI_SELECTCERTIFICATE_STRUCTA;

typedef struct _CRYPTUI_SELECTCERTIFICATE_STRUCTW
{
    DWORD               dwSize;
    HWND                hwndParent;
    DWORD               dwFlags;
    LPCWSTR             szTitle;
    DWORD               dwDontUseColumn;
    LPCWSTR             szDisplayString;
    PFNCFILTERPROC      pFilterCallback;
    PFNCCERTDISPLAYPROC pDisplayCallback;
    void               *pvCallbackData;
    DWORD               cDisplayStores;
    HCERTSTORE         *rghDisplayStores;
    DWORD               cStores;
    HCERTSTORE         *rghStores;
    DWORD               cPropSheetPages;
    LPCPROPSHEETPAGEW   rgPropSheetPages;
    HCERTSTORE          hSelectedCertStore;
} CRYPTUI_SELECTCERTIFICATE_STRUCTW, *PCRYPTUI_SELECTCERTIFICATE_STRUCTW;
typedef const CRYPTUI_SELECTCERTIFICATE_STRUCTW *
 PCCRYPTUI_SELECTCERTIFICATE_STRUCTW;

PCCERT_CONTEXT WINAPI CryptUIDlgSelectCertificateA(
 PCCRYPTUI_SELECTCERTIFICATE_STRUCTA pcsc);
PCCERT_CONTEXT WINAPI CryptUIDlgSelectCertificateW(
 PCCRYPTUI_SELECTCERTIFICATE_STRUCTW pcsc);

typedef struct tagCRYPTUI_VIEWSIGNERINFO_STRUCTA
{
    DWORD             dwSize;
    HWND              hwndParent;
    DWORD             dwFlags;
    LPCSTR            szTitle;
    CMSG_SIGNER_INFO *pSignerInfo;
    HCRYPTMSG         hMsg;
    LPCSTR            pszOID;
    DWORD_PTR         dwReserved;
    DWORD             cStores;
    HCERTSTORE       *rghStores;
    DWORD             cPropSheetPages;
    LPCPROPSHEETPAGEA rgPropSheetPages;
} CRYPTUI_VIEWSIGNERINFO_STRUCTA, *PCRYPTUI_VIEWSIGNERINFO_STRUCTA;

typedef struct tagCRYPTUI_VIEWSIGNERINFO_STRUCTW
{
    DWORD             dwSize;
    HWND              hwndParent;
    DWORD             dwFlags;
    LPCWSTR           szTitle;
    CMSG_SIGNER_INFO *pSignerInfo;
    HCRYPTMSG         hMsg;
    LPCSTR            pszOID;
    DWORD_PTR         dwReserved;
    DWORD             cStores;
    HCERTSTORE       *rghStores;
    DWORD             cPropSheetPages;
    LPCPROPSHEETPAGEW rgPropSheetPages;
} CRYPTUI_VIEWSIGNERINFO_STRUCTW, *PCRYPTUI_VIEWSIGNERINFO_STRUCTW;

BOOL WINAPI CryptUIDlgViewSignerInfoA(CRYPTUI_VIEWSIGNERINFO_STRUCTA *pcvsi);
BOOL WINAPI CryptUIDlgViewSignerInfoW(CRYPTUI_VIEWSIGNERINFO_STRUCTW *pcvsi);

#include <poppack.h>

#ifdef __cplusplus
}
#endif

#endif

