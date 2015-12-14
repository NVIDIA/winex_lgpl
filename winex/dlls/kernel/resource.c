/*
 * KERNEL32 thunks and other undocumented stuff
 *
 * Copyright 1997-1998 Marcus Meissner
 * Copyright 1998      Ulrich Weigand
 *
 */
#include "config.h"

#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <unistd.h>

#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32);

/***********************************************************************
 *           UpdateResourceA                 (KERNEL32.@)
 */
BOOL WINAPI UpdateResourceA(
  HANDLE  hUpdate,
  LPCSTR  lpType,
  LPCSTR  lpName,
  WORD    wLanguage,
  LPVOID  lpData,
  DWORD   cbData) {

  FIXME(": stub\n");
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}

/***********************************************************************
 *           UpdateResourceW                 (KERNEL32.@)
 */
BOOL WINAPI UpdateResourceW(
  HANDLE  hUpdate,
  LPCWSTR lpType,
  LPCWSTR lpName,
  WORD    wLanguage,
  LPVOID  lpData,
  DWORD   cbData) {

  FIXME(": stub\n");
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}
