/*
 * Kernel pointer functions, added in XP SP2
 *
 * Copyright Transgaming Technologies, 2006
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
