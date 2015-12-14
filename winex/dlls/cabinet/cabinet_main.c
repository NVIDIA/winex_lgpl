/*
 * cabinet.dll main
 *
 * Copyright 2002 Patrik Stridvall
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#define NO_SHLWAPI_STREAM
#define NO_SHLWAPI_REG
#include "shlwapi.h"
#undef NO_SHLWAPI_REG
#undef NO_SHLWAPI_STREAM

#include "cabinet.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cabinet);

/***********************************************************************
 * DllGetVersion (CABINET.2)
 *
 * Retrieves version information of the 'CABINET.DLL'
 *
 * PARAMS
 *     pdvi [O] pointer to version information structure.
 *
 * RETURNS
 *     Success: S_OK
 *     Failure: E_INVALIDARG
 *
 * NOTES
 *     Supposedly returns version from IE6SP1RP1
 */
HRESULT WINAPI DllGetVersion (DLLVERSIONINFO *pdvi)
{
  WARN("hmmm... not right version number \"5.1.1106.1\"?\n");

  if (pdvi->cbSize != sizeof(DLLVERSIONINFO)) return E_INVALIDARG;

  pdvi->dwMajorVersion = 5;
  pdvi->dwMinorVersion = 1;
  pdvi->dwBuildNumber = 1106;
  pdvi->dwPlatformID = 1;

  return S_OK;
}

/***********************************************************************
 * Extract (CABINET.3)
 *
 * Apparently an undocumented function, presumably to extract a CAB file
 * to somewhere...
 *
 * PARAMS
 *   dest         pointer to a buffer of 0x32c bytes containing
 *           [I]  - number with value 1 at index 0x18
 *                - the dest path starting at index 0x1c
 *           [O]  - a linked list with the filename existing inside the
 *                  CAB file at idx 0x10
 *                - the number of files inside the CAB file at index 0x14
 *                - the name of the last file with dest path at idx 0x120
 *   what    [I]  char* describing what to uncompress, I guess.
 *
 * RETURNS
 *     Success: S_OK
 *     Failure: E_OUTOFMEMORY (?)
 */
HRESULT WINAPI Extract(EXTRACTdest *dest, LPCSTR what)
{
#define DUMPC(idx)      idx >= sizeof(EXTRACTdest) ? ' ' : \
                        ((unsigned char*) dest)[idx] >= 0x20 ? \
                        ((unsigned char*) dest)[idx] : '.'

#define DUMPH(idx)      idx >= sizeof(EXTRACTdest) ? 0x55 : ((unsigned char*) dest)[idx]

  LPSTR dir;
  unsigned int i;

  TRACE("(dest == %0lx, what == %s)\n", (long) dest, debugstr_a(what));

  if (!dest) {
    /* win2k will crash here */
    FIXME("called without valid parameter dest!\n");
    return E_OUTOFMEMORY;
  }
  for (i=0; i < sizeof(EXTRACTdest); i+=8)
    TRACE( "dest[%04x]:%02x %02x %02x %02x %02x %02x %02x %02x %c%c%c%c%c%c%c%c\n",
           i,
           DUMPH(i+0), DUMPH(i+1), DUMPH(i+2), DUMPH(i+3),
           DUMPH(i+4), DUMPH(i+5), DUMPH(i+6), DUMPH(i+7),
           DUMPC(i+0), DUMPC(i+1), DUMPC(i+2), DUMPC(i+3),
           DUMPC(i+4), DUMPC(i+5), DUMPC(i+6), DUMPC(i+7));

  dir = (LPSTR)LocalAlloc(LPTR, strlen(dest->directory)+1); 
  if (!dir) return E_OUTOFMEMORY;
  lstrcpyA(dir, dest->directory);
  dest->filecount=0;
  dest->filelist = NULL;

  TRACE("extracting to dir: %s\n", debugstr_a(dir));

  /* FIXME: what to do on failure? */
  if (!process_cabinet(what, dir, FALSE, FALSE, dest)) {
    LocalFree((HGLOBAL)dir);
    return E_OUTOFMEMORY;
  }

  LocalFree((HGLOBAL)dir);

  TRACE("filecount %08lx,lastfile %s\n",
         dest->filecount, debugstr_a(dest->lastfile));

  return S_OK;
}
