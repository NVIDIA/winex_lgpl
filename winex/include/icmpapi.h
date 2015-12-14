/*
 * Interface to the ICMP functions.
 *
 * Depends on ipexport.h (there is no include directive in the original)
 */

#ifndef __WINE_ICMPAPI_H
#define __WINE_ICMPAPI_H

#include "windef.h"

HANDLE WINAPI IcmpCreateFile(
    VOID
    );

BOOL WINAPI IcmpCloseHandle(
    HANDLE  IcmpHandle
    );

DWORD WINAPI IcmpSendEcho(
    HANDLE                 IcmpHandle,
    IPAddr                 DestinationAddress,
    LPVOID                 RequestData,
    WORD                   RequestSize,
    PIP_OPTION_INFORMATION RequestOptions,
    LPVOID                 ReplyBuffer,
    DWORD                  ReplySize,
    DWORD                  Timeout
    );


#endif /* __WINE_ICMPAPI_H */
