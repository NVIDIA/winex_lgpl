
/*
 *	Drag & Drop Target-only IDataObject implementation
 *
 *  Copyright 2008 TransGaming Inc.
 */

#ifndef  __DDOBJECT_H__
# define __DDOBJECT_H__

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winuser.h"
#include "winreg.h"
#include "shellapi.h"
#include "shlobj.h"
#include "objidl.h"


HRESULT OLEDD_createDataObject(FORMATETC *format, STGMEDIUM *medium, int count, IDataObject **ppDataObject);


#endif
