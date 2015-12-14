/*
 * Kernel pointer functions, added in XP SP2
 *
 * Copyright (c) 2006-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"
#include "winternl.h"


LPVOID WINAPI EncodePointer (LPVOID ptr)
{
   return ptr;
}


LPVOID WINAPI DecodePointer (LPVOID ptr)
{
   return ptr;
}


LPVOID WINAPI EncodeSystemPointer (LPVOID ptr)
{
   return ptr;
}


LPVOID WINAPI DecodeSystemPointer (LPVOID ptr)
{
   return ptr;
}
