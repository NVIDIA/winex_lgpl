/*
 *  Ldr*() functions ('ldr.c')
 *
 *  Copyright (c) 2010-2015 NVIDIA CORPORATION. All rights reserved.
 *  Date: May 2010
 *
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "winbase.h"
#include "winternl.h"
#include "winnt.h"

NTSTATUS WINAPI LdrDisableThreadCalloutsForDll(HANDLE hModule)
{
    if (DisableThreadLibraryCalls(hModule))
	return STATUS_SUCCESS;
    else
	return STATUS_DLL_NOT_FOUND;
}
