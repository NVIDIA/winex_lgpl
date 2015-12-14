/*
 *          NT Config file access API ('nt_config.h')
 *
 *  Copyright 2008, Transgaming Inc.
 *  Author: Eric van Beurden
 *  Date: Dec 17, 2008
 *
 *  Contains all the API definitions for the config file access API
 *  in NTDLL.  This allows simplified access to the values in the
 *  app's config file.
 *
 */

#ifndef  __NT_CONFIG_H__
# define __NT_CONFIG_H__

# include "config.h"
# include "wine/port.h"
# include "windef.h"


/* opaque handle type for a config file context */
typedef void *HCONFIG;


/* enum CONFIG_FLAGS: flags to control the behaviour of the config API functions. */
typedef enum{
    CONFIG_FLAG_NONE =          0x00,
    CONFIG_FLAG_NOAPPDEFAULTS = 0x01,
} CONFIG_FLAGS;


/* Nt_openConfig(): opens the config file to the section <section> and prepares
     it for being read from.  Returns a handle to the config context if successful.
     Returns NULL on failure. */
HCONFIG Nt_openConfigA(LPCSTR  section, DWORD flags);
HCONFIG Nt_openConfigW(LPCWSTR section, DWORD flags);

/* Nt_closeConfig(): closes a config context that was previously opened by a call
     to Nt_openConfig().  The handle is no longer valid after this point. */
void    Nt_closeConfig(HCONFIG config);

/* Nt_getConfigKey(): retrieves a single key value from the config section identified
     by <config>.  The value for the key <name> will be copied into the buffer <buffer>.
     No more than <size> bytes will be copied into the buffer.  Returns TRUE if the
     value was successfully retrieved.  Returns FALSE otherwise. */
BOOL    Nt_getConfigKeyA(HCONFIG config, LPCSTR  name, CHAR * buffer, DWORD size);
BOOL    Nt_getConfigKeyW(HCONFIG config, LPCWSTR name, WCHAR *buffer, DWORD size);

/* Nt_getSingleConfigValue(): get the value of a single config option.   Handles all registry
     operations internally.  Returns TRUE if the value was successfully retrieved, and FALSE
     otherwise.
     If testAppDefault is true, we'll check for AppDefaults\\<exe name>\\<section> for <name> as
     well and use that to override the global setting, if it exists */
BOOL Nt_getSingleConfigValueW(LPCWSTR section, LPCWSTR name, BOOL testAppDefault, WCHAR *buffer, DWORD size);
BOOL Nt_getSingleConfigValueA(LPCSTR  section, LPCSTR  name, BOOL testAppDefault, CHAR * buffer, DWORD size);


/* Helper functions */
static inline BOOL IS_OPTION_TRUE(WCHAR ch){
    return (ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1';
}

static inline BOOL IS_OPTION_FALSE(WCHAR ch){
    return (ch) == 'n' || (ch) == 'N' || (ch) == 'f' || (ch) == 'F' || (ch) == '0';
}


#endif
