#ifndef __WINE_OLEAUT32_OLE2DISP_H
#define __WINE_OLEAUT32_OLE2DISP_H
/* ole2disp.h
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "wtypes.h"
#include "wine/windef16.h"

BSTR16 WINAPI SysAllocString16(LPCOLESTR16);
BSTR16 WINAPI SysAllocStringLen16(const char*, int);
VOID   WINAPI SysFreeString16(BSTR16);
INT16  WINAPI SysReAllocString16(LPBSTR16,LPCOLESTR16);
int    WINAPI SysReAllocStringLen16(BSTR16*, const char*,  int);
int    WINAPI SysStringLen16(BSTR16);

#endif /* !defined(__WINE_OLEAUT32_OLE2DISP_H) */
