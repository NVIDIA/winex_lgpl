/*
 * Definitions for debugging
 *
 * Copyright 1998, 2002 Juergen Schmied
 */

#ifndef __WINE_SHELL32_DEBUGHLP_H
#define __WINE_SHELL32_DEBUGHLP_H

#include "winbase.h"
#include "shlobj.h"

extern void pdump (LPCITEMIDLIST pidl);
extern BOOL pcheck (LPCITEMIDLIST pidl);

#endif /* __WINE_SHELL32_DEBUGHLP_H */
