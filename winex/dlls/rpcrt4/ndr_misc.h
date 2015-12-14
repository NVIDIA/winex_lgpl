/*
 * NDR definitions
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 */

#ifndef __WINE_NDR_MISC_H
#define __WINE_NDR_MISC_H

#include <stdarg.h>

#include "rpcndr.h"

struct IPSFactoryBuffer;

LONG_PTR RPCRT4_NdrClientCall2(PMIDL_STUB_DESC pStubDesc,
			       PFORMAT_STRING pFormat, va_list args );

HRESULT RPCRT4_GetPSFactory(REFIID riid, struct IPSFactoryBuffer **ppPS);

PFORMAT_STRING ComputeConformance(MIDL_STUB_MESSAGE *pStubMsg, unsigned char *pMemory,
                                  PFORMAT_STRING pFormat, ULONG_PTR def);

typedef unsigned char* (WINAPI *NDR_MARSHALL)  (PMIDL_STUB_MESSAGE, unsigned char*, PFORMAT_STRING);
typedef unsigned char* (WINAPI *NDR_UNMARSHALL)(PMIDL_STUB_MESSAGE, unsigned char**,PFORMAT_STRING, unsigned char);
typedef void           (WINAPI *NDR_BUFFERSIZE)(PMIDL_STUB_MESSAGE, unsigned char*, PFORMAT_STRING);
typedef unsigned long  (WINAPI *NDR_MEMORYSIZE)(PMIDL_STUB_MESSAGE,                 PFORMAT_STRING);
typedef void           (WINAPI *NDR_FREE)      (PMIDL_STUB_MESSAGE, unsigned char*, PFORMAT_STRING);

extern NDR_MARSHALL   NdrMarshaller[];
extern NDR_UNMARSHALL NdrUnmarshaller[];
extern NDR_BUFFERSIZE NdrBufferSizer[];
extern NDR_MEMORYSIZE NdrMemorySizer[];
extern NDR_FREE       NdrFreer[];

#endif  /* __WINE_NDR_MISC_H */
