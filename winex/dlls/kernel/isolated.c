/*
 * Copyright (c) 2006-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "wine/winbase16.h"
#include "winternl.h"
#include "wine/module.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dll);

#define ACT_COOKIE	0xdeaf1337
#define ACT_HANDLE	0xdeaf7331

BOOL WINAPI ActivateActCtx( HANDLE hActCtx, ULONG_PTR* lpCookie )
{
    FIXME( "(%x, %p) Stub!\n", hActCtx, lpCookie );
    
    if ( NULL != lpCookie ) {
      FIXME("setting cookie=%x\n", ACT_COOKIE);
      *lpCookie = ACT_COOKIE;
    }
    
    return( TRUE );
}

HANDLE WINAPI CreateActCtxW( PCACTCTXW pActCtx )
{
    HANDLE ret = ACT_HANDLE; /* should probably be INVALID_HANDLE_VALUE but SWG isn't as happy about that */
    FIXME( "(%p) Stub\n", pActCtx );

    FIXME("returning %x\n", ret);
    return( ret );
}

HANDLE WINAPI CreateActCtxA( PCACTCTXA pActCtx )
{
    HANDLE ret = ACT_HANDLE; 
    FIXME( "(%p) Stub\n", pActCtx );

    /* TODO: duplicate structure and properly handle A->W strings and call CreateActCtxW */
    /* ret = CreateActCtxW( pActCtxw ) ); */
    /* TODO free dupiclated strings */

    FIXME("returning %x\n", ret);
    return( ret );
}


BOOL WINAPI DeactivateActCtx( DWORD dwFlags, ULONG_PTR ulCookie )
{
    FIXME( "(%08lx, %lx) Stub\n", dwFlags, ulCookie );
    return TRUE;
}
        

void WINAPI ReleaseActCtx( HANDLE hActCtx )
{
    FIXME( "(%x) Stub\n", hActCtx );
    return;
}
