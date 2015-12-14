/*
 * secur32 private definitions.
 *
 * Copyright (C) 2004 Juan Lang
 * Copyright (c) 2005-2015 NVIDIA CORPORATION. All rights reserved.
 */

#ifndef __SECUR32_PRIV_H__
#define __SECUR32_PRIV_H__

/* Memory allocation functions for memory accessible by callers of secur32.
 * There is no REALLOC, because LocalReAlloc can only work if used in
 * conjunction with LMEM_MOVEABLE and LocalLock, but callers aren't using
 * LocalLock.  I don't use the Heap functions because there seems to be an
 * implicit assumption that LocalAlloc and Free will be used--MS' secur32
 * imports them (but not the heap functions), the sample SSP uses them, and
 * there isn't an exported secur32 function to allocate memory.
 */
#define SECUR32_ALLOC(bytes) LocalAlloc(0, (bytes))
#define SECUR32_FREE(p)      LocalFree(p)

typedef struct _SecureProvider
{
    PWSTR                   moduleName;
    HMODULE                 lib;
    SecurityFunctionTableA  fnTableA;
    SecurityFunctionTableW  fnTableW;
} SecureProvider;

typedef struct _SecurePackage
{
    SecPkgInfoW     infoW;
    SecureProvider *provider;
} SecurePackage;

/* Tries to find the package named packageName.  If it finds it, implicitly
 * loads the package if it isn't already loaded.
 */
SecurePackage *SECUR32_findPackageW(PWSTR packageName);

/* Tries to find the package named packageName.  (Thunks to _findPackageW)
 */
SecurePackage *SECUR32_findPackageA(PSTR packageName);

/* A few string helpers; will return NULL if str is NULL.  Free return with
 * SECUR32_FREE */
PWSTR SECUR32_strdupW(PCWSTR str);
PWSTR SECUR32_AllocWideFromMultiByte(PCSTR str);
PSTR  SECUR32_AllocMultiByteFromWide(PCWSTR str);

#endif /* ndef __SECUR32_PRIV_H__ */
