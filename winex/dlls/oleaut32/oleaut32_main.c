/* oleaut32_main.c
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */
#include "winbase.h"


extern void create_typelibcache_cs(void);
extern void destroy_typelibcache_cs(void);


BOOL WINAPI OLEAUT32_main( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
  switch(reason)
  {
    case DLL_PROCESS_ATTACH:
      create_typelibcache_cs();
      break;
    case DLL_PROCESS_DETACH:
      destroy_typelibcache_cs();
      break;
  }

  return TRUE;
}
