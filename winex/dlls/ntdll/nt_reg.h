/*
 * nt_reg.h - simplified registry access functions for use in ntdll
 *
 * Copyright 2008 TransGaming Inc.
 *
 */
#ifndef  __NT_REG_H__
# define __NT_REG_H__


# include "config.h"
# include "wine/port.h"
# include "windef.h"
# include "winbase.h"
# include "wine/server.h"
# include "wine/winbase16.h"
# include "winreg.h"



/* Nt_regCreateKeyExW(): a slightly modified version of the real implementation of RegCreateKeyExA().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
NTSTATUS Nt_regCreateKeyExW(HKEY hkey, LPCWSTR name, DWORD options, REGSAM access, LPHKEY retkey);

/* Nt_regOpenKeyExW(): a slightly modified version of the real implementation of RegOpenKey().
     This version has been simplified to suit the needs of accessing the config areas of the
     registry. */
NTSTATUS Nt_regOpenKeyExW(HKEY hkey, LPCWSTR name, REGSAM access, LPHKEY retkey );

/* Nt_regCloseKey(): a slightly modified version of the real implementation of RegCloseKey().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
NTSTATUS Nt_regCloseKey(HKEY hkey);

/* Nt_regQueryValueExW(): a slightly modified version of the real implementation of RegQueryValueExA().
     This version has been simplified to suit the needs of the memory files system.  This has been
     implemented here to avoid a dependency on 'advapi32.dll'. */
NTSTATUS Nt_regQueryValueExW(HKEY hkey, LPCWSTR name, LPBYTE data, DWORD *count);


/* Nt_getConfigKey(): get a config key from either the app-specific or the default config */
BOOL Nt_getConfigKey(HKEY defkey, LPCWSTR name, WCHAR *buffer, DWORD size);


#endif
