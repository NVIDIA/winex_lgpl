/*
 * Win32 threads
 *
 * Copyright 2008 TransGaming Technologies
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
