

#ifndef __WINE_I_CRYPTASN1TLS_H
#define __WINE_I_CRYPTASN1TLS_H

typedef void *ASN1decoding_t;
typedef void *ASN1encoding_t;
typedef void *ASN1module_t;
typedef DWORD HCRYPTASN1MODULE;


#ifdef __cplusplus
extern "C" {
#endif

ASN1decoding_t WINAPI I_CryptGetAsn1Decoder(HCRYPTASN1MODULE);
ASN1encoding_t WINAPI I_CryptGetAsn1Encoder(HCRYPTASN1MODULE);
BOOL        WINAPI I_CryptInstallAsn1Module(ASN1module_t, DWORD, void*);
BOOL        WINAPI I_CryptUninstallAsn1Module(HCRYPTASN1MODULE);

#ifdef __cplusplus
}
#endif

#endif  
