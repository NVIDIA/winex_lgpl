#ifndef WINE_ACLAPI_H
#define WINE_ACLAPI_H


#include <accctrl.h>

#ifdef __cplusplus
extern "C" {
#endif

DWORD WINAPI SetEntriesInAclA(ULONG cCountOfExplicitEntries,
                              PEXPLICIT_ACCESS pListOfExplicitEntries,
                              PACL OldAcl,
                              PACL* NewAcl);


#ifdef __cplusplus
};
#endif

#endif /* WINE_ACLAPI_H */
