

#ifndef __SDDL_H__
#define __SDDL_H__


#ifdef __cplusplus
extern "C" {
#endif


#define SDDL_REVISION_1     1
#define SDDL_REVISION       SDDL_REVISION_1


#define SDDL_OWNER                          TEXT("O")
#define SDDL_GROUP                          TEXT("G")
#define SDDL_DACL                           TEXT("D")
#define SDDL_SACL                           TEXT("S")



#define SDDL_SEPERATORC                     TEXT(";")
#define SDDL_DELIMINATORC                   TEXT(":")
#define SDDL_ACE_BEGINC                     TEXT("(")
#define SDDL_ACE_ENDC                       TEXT(")")



#define SDDL_SEPERATOR                     TEXT(";")
#define SDDL_DELIMINATOR                   TEXT(":")
#define SDDL_ACE_BEGIN                     TEXT("(")
#define SDDL_ACE_END                       TEXT(")")

BOOL WINAPI ConvertSidToStringSidA( PSID, LPSTR* );
BOOL WINAPI ConvertSidToStringSidW( PSID, LPWSTR* );
#define ConvertSidToStringSid WINELIB_NAME_AW(ConvertSidToStringSid)

BOOL WINAPI ConvertStringSidToSidA( LPCSTR, PSID* );
BOOL WINAPI ConvertStringSidToSidW( LPCWSTR, PSID* );
#define ConvertStringSidToSid WINELIB_NAME_AW(ConvertStringSidToSid)

BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorA(
    LPCSTR, DWORD, PSECURITY_DESCRIPTOR*, PULONG );
BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorW(
    LPCWSTR, DWORD, PSECURITY_DESCRIPTOR*, PULONG );
#define ConvertStringSecurityDescriptorToSecurityDescriptor WINELIB_NAME_AW(ConvertStringSecurityDescriptorToSecurityDescriptor)

BOOL WINAPI ConvertSecurityDescriptorToStringSecurityDescriptorA(
    PSECURITY_DESCRIPTOR, DWORD, SECURITY_INFORMATION, LPSTR*, PULONG );
BOOL WINAPI ConvertSecurityDescriptorToStringSecurityDescriptorW(
    PSECURITY_DESCRIPTOR, DWORD, SECURITY_INFORMATION, LPWSTR*, PULONG );
#define ConvertSecurityDescriptorToStringSecurityDescriptor WINELIB_NAME_AW(ConvertSecurityDescriptorToStringSecurityDescriptor)

#ifdef __cplusplus
}
#endif

#endif  
