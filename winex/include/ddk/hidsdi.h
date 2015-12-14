

#ifndef __WINE_HIDSDI_H
#define __WINE_HIDSDI_H




#ifndef WINE_NTSTATUS_DECLARED
#define WINE_NTSTATUS_DECLARED
typedef LONG NTSTATUS;
#endif

void WINAPI HidD_GetHidGuid(LPGUID guid);

#endif  
