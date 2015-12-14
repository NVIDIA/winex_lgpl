/*
 * Definitions for inter-dll snooping
 */
#ifndef __WINE_SNOOP_H
#define __WINE_SNOOP_H

#include "wine/module.h"

extern void SNOOP_RegisterDLL(HMODULE,LPCSTR,DWORD,DWORD);
extern FARPROC SNOOP_GetProcAddress(HMODULE,LPCSTR,DWORD,FARPROC);
extern void SNOOP16_RegisterDLL(NE_MODULE*,LPCSTR);
extern FARPROC16 SNOOP16_GetProcAddress16(HMODULE16,DWORD,FARPROC16);
extern int SNOOP_ShowDebugmsgSnoop(const char *dll,int ord,const char *fname);
extern void SNOOP_ThreadExit ();
#endif
