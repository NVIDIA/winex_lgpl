/*
 * Win32 file change notification functions
 *
 * FIXME: this is VERY difficult to implement with proper Unix support
 * at the wineserver side.
 * (Unix doesn't really support this)
 * See http://x57.deja.com/getdoc.xp?AN=575483053 for possible solutions.
 *
 * Copyright 1998 Ulrich Weigand
 *  Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "winbase.h"
#include "winerror.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(file);

/****************************************************************************
 *		FindFirstChangeNotificationA (KERNEL32.@)
 */
HANDLE WINAPI FindFirstChangeNotificationA( LPCSTR lpPathName, BOOL bWatchSubtree,
                                            DWORD dwNotifyFilter )
{
    HANDLE ret = INVALID_HANDLE_VALUE;

    FIXME("this is not supported yet (non-trivial).\n");

    SERVER_START_REQ( create_change_notification )
    {
        req->subtree = bWatchSubtree;
        req->filter  = dwNotifyFilter;
        if (!wine_server_call_err( req )) ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/****************************************************************************
 *		FindFirstChangeNotificationW (KERNEL32.@)
 */
HANDLE WINAPI FindFirstChangeNotificationW( LPCWSTR lpPathName,
                                                BOOL bWatchSubtree,
                                                DWORD dwNotifyFilter)
{
    HANDLE ret = INVALID_HANDLE_VALUE;

    FIXME("this is not supported yet (non-trivial).\n");

    SERVER_START_REQ( create_change_notification )
    {
        req->subtree = bWatchSubtree;
        req->filter  = dwNotifyFilter;
        if (!wine_server_call_err( req )) ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/****************************************************************************
 *		FindNextChangeNotification (KERNEL32.@)
 */
BOOL WINAPI FindNextChangeNotification( HANDLE handle )
{
    /* FIXME: do something */
    return TRUE;
}

/****************************************************************************
 *		FindCloseChangeNotification (KERNEL32.@)
 */
BOOL WINAPI FindCloseChangeNotification( HANDLE handle)
{
    return CloseHandle( handle );
}

