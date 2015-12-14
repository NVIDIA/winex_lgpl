/*
 * Win32 threads
 *
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "config.h"
#include "wine/port.h"

#include <sched.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);

/***********************************************************************
 *		SwitchToThread (KERNEL.462)
 */
BOOL WINAPI SwitchToThread (void)
{
   /* This has been verified under XP to not modify the current error,
      regardless of whether we return TRUE or FALSE */
   return (NtYieldExecution () != STATUS_NO_YIELD_PERFORMED);
}
