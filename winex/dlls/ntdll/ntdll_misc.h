#ifndef __WINE_NTDLL_MISC_H
#define __WINE_NTDLL_MISC_H

#include "winnt.h"
#include "winternl.h"

/* debug helper */
extern LPCSTR debugstr_us( const UNICODE_STRING *str );
extern void dump_ObjectAttributes (const OBJECT_ATTRIBUTES *ObjectAttributes);

#endif
