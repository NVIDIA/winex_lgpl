
/*
 *	Drag & Drop Target-only IDataObject implementation
 *
 *  Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
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
