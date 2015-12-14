/*
 * MPR Multinet functions
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "winbase.h"
#include "winnetwk.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mpr);


/*****************************************************************
 *     MultinetGetConnectionPerformanceA [MPR.@]
 *
 * RETURNS
 *    Success: NO_ERROR
 *    Failure: ERROR_NOT_SUPPORTED, ERROR_NOT_CONNECTED,
 *             ERROR_NO_NET_OR_BAD_PATH, ERROR_BAD_DEVICE,
 *             ERROR_BAD_NET_NAME, ERROR_INVALID_PARAMETER,
 *             ERROR_NO_NETWORK, ERROR_EXTENDED_ERROR
 */
DWORD WINAPI MultinetGetConnectionPerformanceA(
	LPNETRESOURCEA lpNetResource,
	LPNETCONNECTINFOSTRUCT lpNetConnectInfoStruct )
{
    FIXME( "(%p, %p): stub\n", lpNetResource, lpNetConnectInfoStruct );

    SetLastError(WN_NO_NETWORK);
    return WN_NO_NETWORK;
}

/*****************************************************************
 *     MultinetGetConnectionPerformanceW [MPR.@]
 */
DWORD WINAPI MultinetGetConnectionPerformanceW(
	LPNETRESOURCEW lpNetResource,
	LPNETCONNECTINFOSTRUCT lpNetConnectInfoStruct )
{
    FIXME( "(%p, %p): stub\n", lpNetResource, lpNetConnectInfoStruct );

    SetLastError(WN_NO_NETWORK);
    return WN_NO_NETWORK;
}

/*****************************************************************
 *  MultinetGetErrorTextA [MPR.@]
 */
DWORD WINAPI MultinetGetErrorTextA( DWORD x, DWORD y, DWORD z )
{
    FIXME( "(%lx, %lx, %lx): stub\n", x, y, z );
    return 0;
}

/*****************************************************************
 *  MultinetGetErrorTextW [MPR.@]
 */
DWORD WINAPI MultinetGetErrorTextW( DWORD x, DWORD y, DWORD z )
{
    FIXME( "(%lx, %lx, %lx ): stub\n", x, y, z );
    return 0;
}

