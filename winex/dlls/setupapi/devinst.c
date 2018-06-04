/*
 * SetupAPI device installer
 *
 * Copyright 2000 Andreas Mohr for CodeWeavers
 * Copyright (c) 2005-2015 NVIDIA CORPORATION. All rights reserved.
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
#include "wine/port.h"
 
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winreg.h"
#include "winternl.h"
#include "wingdi.h"
#include "winuser.h"
#include "winnls.h"
#include "setupapi.h"
#include "wine/debug.h"
#include "wine/hid.h"
#include "wine/unicode.h"
#include "cfgmgr32.h"
#include "initguid.h"
#include "winioctl.h"
#include "rpc.h"
#include "wine/heapstr.h"
#include "rpcdce.h"

#include "setupapi_private.h"

#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(setupapi);

/* Use a local name for this since it's an educated guess
 * rather than a well-defined name.
 */
DEFINE_GUID(GUID_XBOX360_CONTROLLER_TG,
            0xec87f1e3L, 0xc13b, 0x4100, 0xb5, 0xf7, 
            0x8b, 0x84, 0xd5, 0x42, 0x60, 0xcb);

DEFINE_GUID(GUID_DISPLAY_DEVICE, 0x4d36e968, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);
DEFINE_GUID(GUID_CDROM_DEVICE,   0x4d36e965, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);


#define SETUP_DEVICE_MAGIC 0xd00ff055

typedef enum
{
    SERIAL_PORT_DEVICE = 0,
    HID_DEVICE,
    DISPLAY_DEVICE,
} SETUPAPI_DEVICE_TYPE;

typedef struct
{
    SETUPAPI_DEVICE_TYPE  type;
    WCHAR                *name;
} DeviceHeader;

typedef struct
{
    DWORD        magic;
    UINT         count;
    DeviceHeader devices[1];
} DevicesHeader;

typedef struct 
{
    DWORD          count;
    DevicesHeader *devices;
} DevicesCurrentHeader;


/* Unicode constants */
static const WCHAR ClassGUID[]  = {'C','l','a','s','s','G','U','I','D',0};
static const WCHAR Class[]  = {'C','l','a','s','s',0};
static const WCHAR ClassInstall32[]  = {'C','l','a','s','s','I','n','s','t','a','l','l','3','2',0};
static const WCHAR NoDisplayClass[]  = {'N','o','D','i','s','p','l','a','y','C','l','a','s','s',0};
static const WCHAR NoInstallClass[]  = {'N','o','I','s','t','a','l','l','C','l','a','s','s',0};
static const WCHAR NoUseClass[]  = {'N','o','U','s','e','C','l','a','s','s',0};
static const WCHAR NtExtension[]  = {'.','N','T',0};
static const WCHAR NtPlatformExtension[]  = {'.','N','T','x','8','6',0};
static const WCHAR Version[]  = {'V','e','r','s','i','o','n',0};
static const WCHAR WinExtension[]  = {'.','W','i','n',0};

/* Registry key and value names */
static const WCHAR ControlClass[] = {'S','y','s','t','e','m','\\',
                                  'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                  'C','o','n','t','r','o','l','\\',
                                  'C','l','a','s','s',0};

static const WCHAR DeviceClasses[] = {'S','y','s','t','e','m','\\',
                                  'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                  'C','o','n','t','r','o','l','\\',
                                  'D','e','v','i','c','e','C','l','a','s','s','e','s',0};

/***********************************************************************
 *              SetupDiBuildClassInfoList  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiBuildClassInfoList(
        DWORD Flags,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize)
{
    TRACE("\n");
    return SetupDiBuildClassInfoListExW(Flags, ClassGuidList,
                                        ClassGuidListSize, RequiredSize,
                                        NULL, NULL);
}

/***********************************************************************
 *              SetupDiBuildClassInfoListExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiBuildClassInfoListExA(
        DWORD Flags,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCSTR MachineName,
        PVOID Reserved)
{
    LPWSTR MachineNameW = NULL;
    BOOL bResult;

    TRACE("\n");

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL) return FALSE;
    }

    bResult = SetupDiBuildClassInfoListExW(Flags, ClassGuidList,
                                           ClassGuidListSize, RequiredSize,
                                           MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return bResult;
}

/***********************************************************************
 *		SetupDiBuildClassInfoListExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiBuildClassInfoListExW(
        DWORD Flags,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCWSTR MachineName,
        PVOID Reserved)
{
    WCHAR szKeyName[40];
    HKEY hClassesKey;
    HKEY hClassKey;
    DWORD dwLength;
    DWORD dwIndex;
    LONG lError;
    DWORD dwGuidListIndex = 0;

    TRACE("\n");

    if (RequiredSize != NULL)
	*RequiredSize = 0;

    hClassesKey = SetupDiOpenClassRegKeyExW(NULL,
                                            KEY_ALL_ACCESS,
                                            DIOCR_INSTALLER,
                                            MachineName,
                                            Reserved);
    if (hClassesKey == INVALID_HANDLE_VALUE)
    {
	return FALSE;
    }

    for (dwIndex = 0; ; dwIndex++)
    {
	dwLength = 40;
	lError = RegEnumKeyExW(hClassesKey,
			       dwIndex,
			       szKeyName,
			       &dwLength,
			       NULL,
			       NULL,
			       NULL,
			       NULL);
	TRACE("RegEnumKeyExW() returns %d\n", lError);
	if (lError == ERROR_SUCCESS || lError == ERROR_MORE_DATA)
	{
	    TRACE("Key name: %p\n", szKeyName);

	    if (RegOpenKeyExW(hClassesKey,
			      szKeyName,
			      0,
			      KEY_ALL_ACCESS,
			      &hClassKey))
	    {
		RegCloseKey(hClassesKey);
		return FALSE;
	    }

	    if (!RegQueryValueExW(hClassKey,
				  NoUseClass,
				  NULL,
				  NULL,
				  NULL,
				  NULL))
	    {
		TRACE("'NoUseClass' value found!\n");
		RegCloseKey(hClassKey);
		continue;
	    }

	    if ((Flags & DIBCI_NOINSTALLCLASS) &&
		(!RegQueryValueExW(hClassKey,
				   NoInstallClass,
				   NULL,
				   NULL,
				   NULL,
				   NULL)))
	    {
		TRACE("'NoInstallClass' value found!\n");
		RegCloseKey(hClassKey);
		continue;
	    }

	    if ((Flags & DIBCI_NODISPLAYCLASS) &&
		(!RegQueryValueExW(hClassKey,
				   NoDisplayClass,
				   NULL,
				   NULL,
				   NULL,
				   NULL)))
	    {
		TRACE("'NoDisplayClass' value found!\n");
		RegCloseKey(hClassKey);
		continue;
	    }

	    RegCloseKey(hClassKey);

	    TRACE("Guid: %p\n", szKeyName);
	    if (dwGuidListIndex < ClassGuidListSize)
	    {
		if (szKeyName[0] == L'{' && szKeyName[37] == L'}')
		{
		    szKeyName[37] = 0;
		}
		TRACE("Guid: %p\n", &szKeyName[1]);

		UuidFromStringW((RPC_WSTR)&szKeyName[1],
				&ClassGuidList[dwGuidListIndex]);
	    }

	    dwGuidListIndex++;
	}

	if (lError != ERROR_SUCCESS)
	    break;
    }

    RegCloseKey(hClassesKey);

    if (RequiredSize != NULL)
	*RequiredSize = dwGuidListIndex;

    if (ClassGuidListSize < dwGuidListIndex)
    {
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameA(
        LPCSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize)
{
  return SetupDiClassGuidsFromNameExA(ClassName, ClassGuidList,
                                      ClassGuidListSize, RequiredSize,
                                      NULL, NULL);
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameW(
        LPCWSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize)
{
  return SetupDiClassGuidsFromNameExW(ClassName, ClassGuidList,
                                      ClassGuidListSize, RequiredSize,
                                      NULL, NULL);
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameExA(
        LPCSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCSTR MachineName,
        PVOID Reserved)
{
    LPWSTR ClassNameW = NULL;
    LPWSTR MachineNameW = NULL;
    BOOL bResult;

    FIXME("\n");

    ClassNameW = MultiByteToUnicode(ClassName, CP_ACP);
    if (ClassNameW == NULL)
        return FALSE;

    if (MachineNameW)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
        {
            MyFree(ClassNameW);
            return FALSE;
        }
    }

    bResult = SetupDiClassGuidsFromNameExW(ClassNameW, ClassGuidList,
                                           ClassGuidListSize, RequiredSize,
                                           MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    MyFree(ClassNameW);

    return bResult;
}

/***********************************************************************
 *		SetupDiClassGuidsFromNameExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassGuidsFromNameExW(
        LPCWSTR ClassName,
        LPGUID ClassGuidList,
        DWORD ClassGuidListSize,
        PDWORD RequiredSize,
        LPCWSTR MachineName,
        PVOID Reserved)
{
    WCHAR szKeyName[40];
    WCHAR szClassName[256];
    HKEY hClassesKey;
    HKEY hClassKey;
    DWORD dwLength;
    DWORD dwIndex;
    LONG lError;
    DWORD dwGuidListIndex = 0;

    if (RequiredSize != NULL)
	*RequiredSize = 0;

    hClassesKey = SetupDiOpenClassRegKeyExW(NULL,
                                            KEY_ALL_ACCESS,
                                            DIOCR_INSTALLER,
                                            MachineName,
                                            Reserved);
    if (hClassesKey == INVALID_HANDLE_VALUE)
    {
	return FALSE;
    }

    for (dwIndex = 0; ; dwIndex++)
    {
	dwLength = 40;
	lError = RegEnumKeyExW(hClassesKey,
			       dwIndex,
			       szKeyName,
			       &dwLength,
			       NULL,
			       NULL,
			       NULL,
			       NULL);
	TRACE("RegEnumKeyExW() returns %d\n", lError);
	if (lError == ERROR_SUCCESS || lError == ERROR_MORE_DATA)
	{
	    TRACE("Key name: %p\n", szKeyName);

	    if (RegOpenKeyExW(hClassesKey,
			      szKeyName,
			      0,
			      KEY_ALL_ACCESS,
			      &hClassKey))
	    {
		RegCloseKey(hClassesKey);
		return FALSE;
	    }

	    dwLength = 256 * sizeof(WCHAR);
	    if (!RegQueryValueExW(hClassKey,
				  Class,
				  NULL,
				  NULL,
				  (LPBYTE)szClassName,
				  &dwLength))
	    {
		TRACE("Class name: %p\n", szClassName);

		if (strcmpiW(szClassName, ClassName) == 0)
		{
		    TRACE("Found matching class name\n");

		    TRACE("Guid: %p\n", szKeyName);
		    if (dwGuidListIndex < ClassGuidListSize)
		    {
			if (szKeyName[0] == L'{' && szKeyName[37] == L'}')
			{
			    szKeyName[37] = 0;
			}
			TRACE("Guid: %p\n", &szKeyName[1]);

			UuidFromStringW((RPC_WSTR)&szKeyName[1],
					&ClassGuidList[dwGuidListIndex]);
		    }

		    dwGuidListIndex++;
		}
	    }

	    RegCloseKey(hClassKey);
	}

	if (lError != ERROR_SUCCESS)
	    break;
    }

    RegCloseKey(hClassesKey);

    if (RequiredSize != NULL)
	*RequiredSize = dwGuidListIndex;

    if (ClassGuidListSize < dwGuidListIndex)
    {
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *              SetupDiClassNameFromGuidA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidA(
        const GUID* ClassGuid,
        PSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize)
{
  return SetupDiClassNameFromGuidExA(ClassGuid, ClassName,
                                     ClassNameSize, RequiredSize,
                                     NULL, NULL);
}

/***********************************************************************
 *              SetupDiClassNameFromGuidW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidW(
        const GUID* ClassGuid,
        PWSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize)
{
  return SetupDiClassNameFromGuidExW(ClassGuid, ClassName,
                                     ClassNameSize, RequiredSize,
                                     NULL, NULL);
}

/***********************************************************************
 *              SetupDiClassNameFromGuidExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidExA(
        const GUID* ClassGuid,
        PSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize,
        PCSTR MachineName,
        PVOID Reserved)
{
    WCHAR ClassNameW[MAX_CLASS_NAME_LEN];
    LPWSTR MachineNameW = NULL;
    BOOL ret;

    if (MachineName)
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
    ret = SetupDiClassNameFromGuidExW(ClassGuid, ClassNameW, MAX_CLASS_NAME_LEN,
     NULL, MachineNameW, Reserved);
    if (ret)
    {
        int len = WideCharToMultiByte(CP_ACP, 0, ClassNameW, -1, ClassName,
         ClassNameSize, NULL, NULL);

        if (!ClassNameSize && RequiredSize)
            *RequiredSize = len;
    }
    MyFree(MachineNameW);
    return ret;
}

/***********************************************************************
 *		SetupDiClassNameFromGuidExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiClassNameFromGuidExW(
        const GUID* ClassGuid,
        PWSTR ClassName,
        DWORD ClassNameSize,
        PDWORD RequiredSize,
        PCWSTR MachineName,
        PVOID Reserved)
{
    HKEY hKey;
    DWORD dwLength;

    hKey = SetupDiOpenClassRegKeyExW(ClassGuid,
                                     KEY_ALL_ACCESS,
                                     DIOCR_INSTALLER,
                                     MachineName,
                                     Reserved);
    if (hKey == INVALID_HANDLE_VALUE)
    {
	return FALSE;
    }

    if (RequiredSize != NULL)
    {
	dwLength = 0;
	if (RegQueryValueExW(hKey,
			     Class,
			     NULL,
			     NULL,
			     NULL,
			     &dwLength))
	{
	    RegCloseKey(hKey);
	    return FALSE;
	}

	*RequiredSize = dwLength / sizeof(WCHAR);
    }

    dwLength = ClassNameSize * sizeof(WCHAR);
    if (RegQueryValueExW(hKey,
			 Class,
			 NULL,
			 NULL,
			 (LPBYTE)ClassName,
			 &dwLength))
    {
	RegCloseKey(hKey);
	return FALSE;
    }

    RegCloseKey(hKey);

    return TRUE;
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoList (SETUPAPI.@)
 */
HDEVINFO WINAPI
SetupDiCreateDeviceInfoList(const GUID *ClassGuid,
			    HWND hwndParent)
{
  return SetupDiCreateDeviceInfoListExW(ClassGuid, hwndParent, NULL, NULL);
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoListExA (SETUPAPI.@)
 */
HDEVINFO WINAPI
SetupDiCreateDeviceInfoListExA(const GUID *ClassGuid,
			       HWND hwndParent,
			       PCSTR MachineName,
			       PVOID Reserved)
{
    LPWSTR MachineNameW = NULL;
    HDEVINFO hDevInfo;

    TRACE("\n");

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
            return INVALID_HANDLE_VALUE;
    }

    hDevInfo = SetupDiCreateDeviceInfoListExW(ClassGuid, hwndParent,
                                              MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return hDevInfo;
}

/***********************************************************************
 *		SetupDiCreateDeviceInfoListExW (SETUPAPI.@)
 */
HDEVINFO WINAPI
SetupDiCreateDeviceInfoListExW(const GUID *ClassGuid,
			       HWND hwndParent,
			       PCWSTR MachineName,
			       PVOID Reserved)
{
  FIXME("\n");
  return INVALID_HANDLE_VALUE;
}

/***********************************************************************
 *		SetupDiEnumDeviceInfo (SETUPAPI.@)
 */
BOOL WINAPI SetupDiEnumDeviceInfo(HDEVINFO          devinfo,
                                  DWORD             index,
                                  PSP_DEVINFO_DATA  info)
{
    DevicesHeader *list = (DevicesHeader *)devinfo;


    TRACE("{devinfo = %p, index = %d, info = %p}\n", devinfo, index, info);

    if (devinfo == NULL || devinfo == INVALID_HANDLE_VALUE || list->magic != SETUP_DEVICE_MAGIC)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (info == NULL || info->cbSize != sizeof(SP_DEVINFO_DATA))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (index >= list->count)
    {
        SetLastError(ERROR_NO_MORE_ITEMS);
        return FALSE;
    }

    switch (list->devices[index].type)
    {
        case SERIAL_PORT_DEVICE:
            memcpy(&info->ClassGuid, &GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR, sizeof(info->ClassGuid));
            break;

        case HID_DEVICE:
            memcpy(&info->ClassGuid, &GUID_DEVINTERFACE_HID, sizeof(info->ClassGuid));
            break;

        case DISPLAY_DEVICE:
            memcpy(&info->ClassGuid, &GUID_DISPLAY_DEVICE, sizeof(info->ClassGuid));
            break;

        default:
            FIXME("Device type not supported: %d\n", list->devices[index].type);
            SetLastError(ERROR_NOT_FOUND);
            return FALSE;
    }

    info->DevInst = 0;  /* FIXME!! this should be a handle or dummy value of some sort.  Perhaps a
                                   list index? */
    info->Reserved = (ULONG_PTR)&list->devices[index];

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/***********************************************************************
 *		SetupDiGetActualSectionToInstallA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetActualSectionToInstallA(
        HINF InfHandle,
        PCSTR InfSectionName,
        PSTR InfSectionWithExt,
        DWORD InfSectionWithExtSize,
        PDWORD RequiredSize,
        PSTR *Extension)
{
    FIXME("\n");
    return FALSE;
}

/***********************************************************************
 *		SetupDiGetActualSectionToInstallW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetActualSectionToInstallW(
        HINF InfHandle,
        PCWSTR InfSectionName,
        PWSTR InfSectionWithExt,
        DWORD InfSectionWithExtSize,
        PDWORD RequiredSize,
        PWSTR *Extension)
{
    WCHAR szBuffer[MAX_PATH];
    DWORD dwLength;
    DWORD dwFullLength;
    LONG lLineCount = -1;

    lstrcpyW(szBuffer, InfSectionName);
    dwLength = lstrlenW(szBuffer);

    if (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
	/* Test section name with '.NTx86' extension */
	lstrcpyW(&szBuffer[dwLength], NtPlatformExtension);
	lLineCount = SetupGetLineCountW(InfHandle, szBuffer);

	if (lLineCount == -1)
	{
	    /* Test section name with '.NT' extension */
	    lstrcpyW(&szBuffer[dwLength], NtExtension);
	    lLineCount = SetupGetLineCountW(InfHandle, szBuffer);
	}
    }
    else
    {
	/* Test section name with '.Win' extension */
	lstrcpyW(&szBuffer[dwLength], WinExtension);
	lLineCount = SetupGetLineCountW(InfHandle, szBuffer);
    }

    if (lLineCount == -1)
    {
	/* Test section name without extension */
	szBuffer[dwLength] = 0;
	lLineCount = SetupGetLineCountW(InfHandle, szBuffer);
    }

    if (lLineCount == -1)
    {
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }

    dwFullLength = lstrlenW(szBuffer);

    if (InfSectionWithExt != NULL && InfSectionWithExtSize != 0)
    {
	if (InfSectionWithExtSize < (dwFullLength + 1))
	{
	    SetLastError(ERROR_INSUFFICIENT_BUFFER);
	    return FALSE;
	}

	lstrcpyW(InfSectionWithExt, szBuffer);
	if (Extension != NULL)
	{
	    *Extension = (dwLength == dwFullLength) ? NULL : &InfSectionWithExt[dwLength];
	}
    }

    if (RequiredSize != NULL)
    {
	*RequiredSize = dwFullLength + 1;
    }

    return TRUE;
}

/***********************************************************************
 *		SetupDiGetClassDescriptionA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionA(
        const GUID* ClassGuid,
        PSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize)
{
  return SetupDiGetClassDescriptionExA(ClassGuid, ClassDescription,
                                       ClassDescriptionSize,
                                       RequiredSize, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassDescriptionW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionW(
        const GUID* ClassGuid,
        PWSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize)
{
  return SetupDiGetClassDescriptionExW(ClassGuid, ClassDescription,
                                       ClassDescriptionSize,
                                       RequiredSize, NULL, NULL);
}

/***********************************************************************
 *		SetupDiGetClassDescriptionExA  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionExA(
        const GUID* ClassGuid,
        PSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize,
        PCSTR MachineName,
        PVOID Reserved)
{
  FIXME("\n");
  return FALSE;
}

/***********************************************************************
 *		SetupDiGetClassDescriptionExW  (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetClassDescriptionExW(
        const GUID* ClassGuid,
        PWSTR ClassDescription,
        DWORD ClassDescriptionSize,
        PDWORD RequiredSize,
        PCWSTR MachineName,
        PVOID Reserved)
{
    HKEY hKey;
    DWORD dwLength;

    hKey = SetupDiOpenClassRegKeyExW(ClassGuid,
                                     KEY_ALL_ACCESS,
                                     DIOCR_INSTALLER,
                                     MachineName,
                                     Reserved);
    if (hKey == INVALID_HANDLE_VALUE)
    {
	WARN("SetupDiOpenClassRegKeyExW() failed (Error %u)\n", GetLastError());
	return FALSE;
    }

    if (RequiredSize != NULL)
    {
	dwLength = 0;
	if (RegQueryValueExW(hKey,
			     NULL,
			     NULL,
			     NULL,
			     NULL,
			     &dwLength))
	{
	    RegCloseKey(hKey);
	    return FALSE;
	}

	*RequiredSize = dwLength / sizeof(WCHAR);
    }

    dwLength = ClassDescriptionSize * sizeof(WCHAR);
    if (RegQueryValueExW(hKey,
			 NULL,
			 NULL,
			 NULL,
			 (LPBYTE)ClassDescription,
			 &dwLength))
    {
	RegCloseKey(hKey);
	return FALSE;
    }

    RegCloseKey(hKey);

    return TRUE;
}

/***********************************************************************
 *		SetupDiGetClassDevsExA (SETUPAPI.@)
 *
 * NO WINAPI in description given
 */
HDEVINFO WINAPI SetupDiGetClassDevsExA(CONST GUID * class,
                                       PCSTR        enumstr,
                                       HWND         parent,
                                       DWORD        flags,
                                       HDEVINFO     deviceset,
                                       PCSTR        machine,
                                       PVOID        reserved)
{
    HDEVINFO    ret;
    LPWSTR      enumstrW = NULL;
    LPWSTR      machineW = NULL;


    TRACE("{class = %s, enumstr = %s, parent = %p, flags = 0x%08x, deviceset = %p, machine = %s, reserved = %p}\n",
            debugstr_guid(class), enumstr,parent, flags, deviceset, machine, reserved);

    enumstrW = HEAP_strdupAtoW(GetProcessHeap(), 0, enumstr);

    if (enumstrW == NULL && enumstr != NULL)
    {
        ERR("failed to convert the enumeration string\n");
        return INVALID_HANDLE_VALUE;
    }

    machineW = HEAP_strdupAtoW(GetProcessHeap(), 0, machine);

    if (machineW == NULL && machine != NULL)
    {
        ERR("failed to convert the machine name\n");
        HeapFree(GetProcessHeap(), 0, enumstrW);
        return INVALID_HANDLE_VALUE;
    }


    ret = SetupDiGetClassDevsExW(class, enumstrW, parent, flags, deviceset, machineW, reserved);
    HeapFree(GetProcessHeap(), 0, enumstrW);
    HeapFree(GetProcessHeap(), 0, machineW);
    return ret;
}


/***********************************************************************
 *		SetupDiGetClassDevsA (SETUPAPI.@)
 */
HDEVINFO WINAPI SetupDiGetClassDevsA(
       CONST GUID *class,
       LPCSTR enumstr,
       HWND parent,
       DWORD flags)
{
    return SetupDiGetClassDevsExA(class, enumstr, parent, flags, NULL, NULL, NULL);
}

static HDEVINFO SETUP_CreateSerialDeviceList(void)
{
    static const size_t initialSize = 100;
    DevicesHeader      *list;
    size_t              size;
    static const WCHAR  comW[] = { 'C','O','M',0 };
    UINT                numSerialPorts = 0;
    WCHAR               buf[initialSize];
    LPWSTR              devices, ptr;
    HDEVINFO            ret;
    BOOL                failed = FALSE;

    devices = buf;
    size = initialSize;
    do {
        if (QueryDosDeviceW(NULL, devices, size) == 0)
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                size *= 2;
                if (devices != buf)
                    HeapFree(GetProcessHeap(), 0, devices);
                devices = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));
                if (!devices)
                    failed = TRUE;
                else
                    *devices = 0;
            }
            else
                failed = TRUE;
        }
    } while (!*devices && !failed);

    if (failed == TRUE)
    {
        if (devices != buf)
            HeapFree(GetProcessHeap(), 0, devices);

        return INVALID_HANDLE_VALUE;
    }

    for (ptr = devices; *ptr; ptr += strlenW(ptr) + 1)
    {
        if (!strncmpW(comW, ptr, sizeof(comW) / sizeof(comW[0]) - 1))
            numSerialPorts++;
    }

    list = HeapAlloc(GetProcessHeap(), 0, sizeof(DevicesHeader) +
                     (numSerialPorts - 1) * sizeof(DeviceHeader));
    if (list == NULL)
    {
        ERR("Heap allocation failure.\n");

        if (devices != buf)
            HeapFree(GetProcessHeap(), 0, devices);

        return INVALID_HANDLE_VALUE;
    }

    list->magic = SETUP_DEVICE_MAGIC;
    list->count = 0;
    for (ptr = devices; *ptr; ptr += strlenW(ptr) + 1)
    {
        if (!strncmpW(comW, ptr, sizeof(comW) / sizeof(comW[0]) - 1))
        {
            list->devices[list->count].type = SERIAL_PORT_DEVICE;
            list->devices[list->count].name = HeapAlloc(GetProcessHeap(), 0, (strlenW(ptr) + 1) * sizeof(WCHAR));
            if (list->devices[list->count].name == NULL)
            {
                UINT i;

                ERR("Heap allocation failure: %d\n", list->count);

                if (devices != buf)
                    HeapFree(GetProcessHeap(), 0, devices);

                for (i = 0; i < list->count; i++)
                    HeapFree(GetProcessHeap(), 0, list->devices[i].name);

                HeapFree(GetProcessHeap(), 0, list);

                return INVALID_HANDLE_VALUE;
            }
            lstrcpynW(list->devices[list->count].name, ptr,
             sizeof(list->devices[list->count].name) /
             sizeof(list->devices[list->count].name[0]));
            TRACE("Adding %s to list\n",
             debugstr_w(list->devices[list->count].name));
            list->count++;
        }
    }
    TRACE("list->count is %d\n", list->count);
    ret = (HDEVINFO)list;
    if (devices != buf)
        HeapFree(GetProcessHeap(), 0, devices);
    TRACE("returning %p\n", ret);
    return ret;
}

static BOOL populate_list(const hid_device_t *device, void *user_data)
{
    DevicesCurrentHeader *header = (DevicesCurrentHeader *)user_data;
    WCHAR                *name;

    TRACE("(%p, %p)\n", device, user_data);

    if (header->count < header->devices->count + 1)
    {
        DevicesHeader *new_devices;

        WARN("Device count changed before callback: %d vs. %d\n", header->count, header->devices->count + 1);

        if (header->devices == NULL)
            new_devices = HeapAlloc(GetProcessHeap(), 0, sizeof(DevicesHeader) + (header->devices->count) * sizeof(DeviceHeader));

        else
            new_devices = HeapReAlloc(GetProcessHeap(), 0, header->devices, sizeof(DevicesHeader) + (header->devices->count) * sizeof(DeviceHeader));

        if (new_devices == NULL)
        {
            ERR("Heap reallocation failure.\n");
            return FALSE;
        }

        header->count = header->devices->count + 1;
        header->devices = new_devices;
    }

    name = HeapAlloc(GetProcessHeap(), 0, (strlenW(device->device_names[HID_DEVICE_INDEX].name) + 1) * sizeof(WCHAR));
    if (name == NULL)
    {
        ERR("Heap allocation failure.\n");
        return FALSE;
    }

    lstrcpyW(name, device->device_names[HID_DEVICE_INDEX].name);
    header->devices->devices[header->devices->count].name = name;
    header->devices->devices[header->devices->count].type = HID_DEVICE;
    header->devices->count++;

    return TRUE;
}

static HDEVINFO SETUP_CreateHIDDeviceList(void)
{
    DevicesCurrentHeader header;

    TRACE("()\n");

    header.count = HID_GetDeviceCount();
    if (header.count == 0)
        return NULL;

    /* Device array already contains space for adding one device */
    header.devices = HeapAlloc(GetProcessHeap(), 0, sizeof(DevicesHeader) +
                               (header.count - 1) * sizeof(DeviceHeader));

    if (header.devices == NULL)
    {
        ERR("Heap allocation failure.\n");
        return INVALID_HANDLE_VALUE;
    }

    /* Fill basic descriptor properties */
    header.devices->magic = SETUP_DEVICE_MAGIC;
    header.devices->count = 0;

    /* Iterate through the device list to fill device information elements */
    HID_DeviceEnumerate(populate_list, &header);

    return (HDEVINFO)header.devices;
}

static BOOL SETUP_getDisplayDeviceID(WCHAR *devID, SIZE_T len)
{
    typedef void (*USER_getDisplayDeviceID_Func)(WCHAR *, SIZE_T, WCHAR *, SIZE_T);
    static USER_getDisplayDeviceID_Func getDisplayDeviceID = NULL;


    if (getDisplayDeviceID == NULL)
    {
        getDisplayDeviceID = (USER_getDisplayDeviceID_Func)GetProcAddress(GetModuleHandleA("user32"), "USER_getDisplayDeviceID");

        if (getDisplayDeviceID == NULL)
        {
            ERR("failed to import the helper function\n");
            return FALSE;
        }
    }

    getDisplayDeviceID(devID, len, NULL, 0);
    return TRUE;
}

static HDEVINFO SETUP_CreateDisplayDeviceList(const WCHAR *devName)
{
    DevicesCurrentHeader    header;
    WCHAR *                 name;
    size_t                  len;
    WCHAR                   displayDevID[MAX_PATH];


    TRACE("()\n");

    /* no device name passed in => retrieve it */
    if (devName == NULL)
    {
        if (!SETUP_getDisplayDeviceID(displayDevID, sizeof(displayDevID)))
        {
            ERR("failed to retrieve the display device ID\n");
            return INVALID_HANDLE_VALUE;
        }

        devName = displayDevID;
    }

    header.count = 1;
    header.devices = HeapAlloc(GetProcessHeap(), 0, sizeof(DevicesHeader) + ((header.count - 1) * sizeof(DeviceHeader)));

    if (header.devices == NULL)
    {
        ERR("failed to allocate memory for a device list\n");
        return INVALID_HANDLE_VALUE;
    }


    /* fill in the list header */
    header.devices->magic = SETUP_DEVICE_MAGIC;
    header.devices->count = 1;

    /* fill in the single item */
    len = strlenW(devName);
    name = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));

    if (name == NULL)
    {
        ERR("failed to allocate memory for the device name\n");
        HeapFree(GetProcessHeap(), 0, header.devices);
        return INVALID_HANDLE_VALUE;
    }

    lstrcpynW(name, devName, len + 1);
    header.devices->devices[0].type = DISPLAY_DEVICE;
    header.devices->devices[0].name = name;

    return (HDEVINFO)header.devices;
}

/***********************************************************************
 *		SetupDiGetClassDevsExW (SETUPAPI.@)
 *
 * NO WINAPI in description given
 */
HDEVINFO WINAPI SetupDiGetClassDevsExW(
	CONST GUID *class,
	PCWSTR enumstr,
	HWND parent,
	DWORD flags,
	HDEVINFO deviceset,
	PCWSTR machine,
	PVOID reserved)
{
    HDEVINFO ret = INVALID_HANDLE_VALUE;

    TRACE("{class = %s, enumstr = %s, parent = %p, flags = 0x%08x, deviceset = %p, machine = %s, reserved = %p}\n", debugstr_guid(class), debugstr_w(enumstr),parent, flags, deviceset, debugstr_w(machine), reserved);

    if (class == NULL && !(flags & DIGCF_ALLCLASSES))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return ret;
    }

    if (deviceset || machine || reserved)
        FIXME(": unimplimented parameters\n");

    if (enumstr)
    {
        WCHAR displayDevID[MAX_PATH];


        if (!SETUP_getDisplayDeviceID(displayDevID, sizeof(displayDevID)))
        {
            SetLastError(ERROR_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }

        /* asking for the display device information */
        if (strcmpiW(enumstr, displayDevID) == 0)
            ret = SETUP_CreateDisplayDeviceList(displayDevID);

        else
            FIXME(": unimplemented for enumerator strings (%s)\n", debugstr_w(enumstr));
    }
    else if (flags & DIGCF_ALLCLASSES)
    {
        FIXME(": unimplemented for DIGCF_ALLCLASSES\n");
        ret = SETUP_CreateSerialDeviceList();
        /* FIXME!! add HID devices to this list */
        /* FIXME!! add display devices to this list */
        /* FIXME!! add CD/DVD ROM devices to this list */
    }
    else
    {
        if (IsEqualIID(class, &GUID_DEVINTERFACE_COMPORT))
            ret = SETUP_CreateSerialDeviceList();
        else if (IsEqualIID(class, &GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR))
            ret = SETUP_CreateSerialDeviceList();
        else if (IsEqualIID(class, &GUID_DEVINTERFACE_HID))
            ret = SETUP_CreateHIDDeviceList();
        else if (IsEqualIID(class, &GUID_DISPLAY_DEVICE))
            ret = SETUP_CreateDisplayDeviceList(NULL);
        else if (IsEqualIID(class, &GUID_CDROM_DEVICE))
            FIXME("retrieve CD/DVD ROM device info\n");
        else if (IsEqualIID(class, &GUID_XBOX360_CONTROLLER_TG))
            /* Silence spew from xinput */
            SHOW_MSG_ONCE(FIXME, "(%s) - xinput request not handled\n",
                          debugstr_guid(class));
        else
            FIXME("(%s): stub\n", debugstr_guid(class));
    }

    SetLastError(ERROR_SUCCESS);
    return ret;
}

/***********************************************************************
 *		SetupDiGetClassDevsW (SETUPAPI.@)
 */
HDEVINFO WINAPI SetupDiGetClassDevsW(
       CONST GUID *class,
       LPCWSTR enumstr,
       HWND parent,
       DWORD flags)
{
    return SetupDiGetClassDevsExW(class, enumstr, parent, flags, NULL, NULL, NULL);
}

/***********************************************************************
 *		SetupDiEnumDeviceInterfaces (SETUPAPI.@)
 */
BOOL WINAPI SetupDiEnumDeviceInterfaces(
       HDEVINFO DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       CONST GUID * InterfaceClassGuid,
       DWORD MemberIndex,
       PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData)
{
    DevicesHeader *list = (DevicesHeader *)DeviceInfoSet;

    TRACE("%p, %p, %s, 0x%08x, %p\n", DeviceInfoSet, DeviceInfoData,
     debugstr_guid(InterfaceClassGuid), MemberIndex, DeviceInterfaceData);

    if (!DeviceInfoSet || DeviceInfoSet == INVALID_HANDLE_VALUE || list->magic != SETUP_DEVICE_MAGIC)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (!DeviceInterfaceData || (DeviceInfoData && DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA)))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (DeviceInterfaceData->cbSize != sizeof(SP_DEVICE_INTERFACE_DATA))
    {
        SetLastError(ERROR_INVALID_USER_BUFFER);
        return FALSE;
    }

    if (DeviceInfoData)
        FIXME(": unimplemented with PSP_DEVINFO_DATA set\n");

    if (MemberIndex >= list->count)
    {
        SetLastError(ERROR_NO_MORE_ITEMS);
        return FALSE;
    }

    switch (list->devices[MemberIndex].type)
    {
    case SERIAL_PORT_DEVICE:
        memcpy(&DeviceInterfaceData->InterfaceClassGuid,
               &GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR,
               sizeof(DeviceInterfaceData->InterfaceClassGuid));
        break;
    case HID_DEVICE:
        memcpy(&DeviceInterfaceData->InterfaceClassGuid,
               &GUID_DEVINTERFACE_HID,
               sizeof(DeviceInterfaceData->InterfaceClassGuid));
        break;
    case DISPLAY_DEVICE:
        memcpy(&DeviceInterfaceData->InterfaceClassGuid,
               &GUID_DISPLAY_DEVICE,
               sizeof(DeviceInterfaceData->InterfaceClassGuid));
        break;
    default:
        FIXME("Device type not supported: %d\n", list->devices[MemberIndex].type);
        return FALSE;
    }

    DeviceInterfaceData->Flags = 0;
    DeviceInterfaceData->Reserved = (ULONG_PTR)&list->devices[MemberIndex];

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/***********************************************************************
 *		SetupDiDestroyDeviceInfoList (SETUPAPI.@)
 */
BOOL WINAPI SetupDiDestroyDeviceInfoList(HDEVINFO devinfo)
{
    DevicesHeader *list = (DevicesHeader *)devinfo;
    UINT           index;

    TRACE("%p\n", devinfo);

    if (devinfo == NULL || devinfo == INVALID_HANDLE_VALUE || list->magic != SETUP_DEVICE_MAGIC)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    for (index = 0; index < list->count; index++)
        HeapFree(GetProcessHeap(), 0, list->devices[index].name);

    HeapFree(GetProcessHeap(), 0, list);

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/***********************************************************************
 *		SetupDiGetDeviceInterfaceDetailA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInterfaceDetailA(
      HDEVINFO DeviceInfoSet,
      PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
      PSP_DEVICE_INTERFACE_DETAIL_DATA_A DeviceInterfaceDetailData,
      DWORD DeviceInterfaceDetailDataSize,
      PDWORD RequiredSize,
      PSP_DEVINFO_DATA DeviceInfoData)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailDataW = NULL;
    DWORD                              DeviceInterfaceDetailDataSizeW = 0;
    DWORD                              RequiredSizeW = -1;
    BOOL                               success;

    TRACE("(%p, %p, %p, %u, %p, %p)\n", DeviceInfoSet,
     DeviceInterfaceData, DeviceInterfaceDetailData,
     DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);

    if (DeviceInterfaceDetailDataSize)
    {
        DeviceInterfaceDetailDataSizeW = (DeviceInterfaceDetailDataSize - sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A)) *
                                         sizeof(WCHAR) + sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        DeviceInterfaceDetailDataW = HeapAlloc(GetProcessHeap(), 0, DeviceInterfaceDetailDataSizeW);
        if (DeviceInterfaceDetailDataW == NULL)
        {
            ERR("Heap allocation failure.\n");
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    
        DeviceInterfaceDetailDataW->cbSize = DeviceInterfaceDetailData->cbSize - sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) +
                                             sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    }

    success = SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, DeviceInterfaceData, DeviceInterfaceDetailDataW,
                                               DeviceInterfaceDetailDataSizeW, &RequiredSizeW, DeviceInfoData);

    if (RequiredSize && RequiredSizeW != -1)
        *RequiredSize = (RequiredSizeW - sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) /
                        sizeof(WCHAR) + sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

    if (success == FALSE)
    {
        HeapFree(GetProcessHeap(), 0, DeviceInterfaceDetailDataW);
        return FALSE;
    }

    if (!WideCharToMultiByte(CP_ACP, 0, DeviceInterfaceDetailDataW->DevicePath, -1,
        DeviceInterfaceDetailData->DevicePath, DeviceInterfaceDetailDataSize, NULL, NULL))
    {
        ERR("DevicePath conversion failure: %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, DeviceInterfaceDetailDataW);
        return FALSE;
    }

    HeapFree(GetProcessHeap(), 0, DeviceInterfaceDetailDataW);
    return TRUE;
}

/***********************************************************************
 *		SetupDiGetDeviceInterfaceDetailW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInterfaceDetailW(
      HDEVINFO DeviceInfoSet,
      PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
      PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData,
      DWORD DeviceInterfaceDetailDataSize,
      PDWORD RequiredSize,
      PSP_DEVINFO_DATA DeviceInfoData)
{
    DevicesHeader *list = (DevicesHeader *)DeviceInfoSet;
    DeviceHeader  *devName;
    DWORD          sizeRequired;

    TRACE("(%p, %p, %p, %u, %p, %p)\n", DeviceInfoSet,
     DeviceInterfaceData, DeviceInterfaceDetailData,
     DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);

    if (!DeviceInfoSet || DeviceInfoSet == INVALID_HANDLE_VALUE || list->magic != SETUP_DEVICE_MAGIC)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (!DeviceInterfaceData ||
        ((DeviceInterfaceDetailDataSize && !DeviceInterfaceDetailData) ||
         (DeviceInterfaceDetailData && !DeviceInterfaceDetailDataSize)))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (DeviceInterfaceDetailData && DeviceInterfaceDetailData->cbSize != sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
    {
        SetLastError(ERROR_INVALID_USER_BUFFER);
        return FALSE;
    }

    devName = (DeviceHeader *)DeviceInterfaceData->Reserved;
    sizeRequired = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + lstrlenW(devName->name) * sizeof(WCHAR);
    if (RequiredSize)
        *RequiredSize = sizeRequired;

    if (sizeRequired > DeviceInterfaceDetailDataSize)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    lstrcpyW(DeviceInterfaceDetailData->DevicePath, devName->name);
    TRACE("DevicePath is %s\n", debugstr_w(DeviceInterfaceDetailData->DevicePath));
    if (DeviceInfoData)
    {
        DeviceInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
        switch (devName->type)
        {
        case SERIAL_PORT_DEVICE:
            memcpy(&DeviceInfoData->ClassGuid,
                   &GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR,
                   sizeof(DeviceInfoData->ClassGuid));
            break;
        case HID_DEVICE:
            memcpy(&DeviceInfoData->ClassGuid,
                   &GUID_DEVINTERFACE_HID,
                   sizeof(DeviceInfoData->ClassGuid));
            break;
        default:
            FIXME("Possible enumeration of serial ports and HID devices only: %d\n", devName->type);
        }
        DeviceInfoData->DevInst = 0;
        DeviceInfoData->Reserved = (ULONG_PTR)devName;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/***********************************************************************
 *		SetupDiGetDeviceRegistryPropertyA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceRegistryPropertyA(
        HDEVINFO  devinfo,
        PSP_DEVINFO_DATA  DeviceInfoData,
        DWORD   Property,
        PDWORD  PropertyRegDataType,
        PBYTE   PropertyBuffer,
        DWORD   PropertyBufferSize,
        PDWORD  RequiredSize)
{
    FIXME("stub! {definfo = %p, DeviceInfoData = %p, Property = 0x%08x, PropertyRegDataType = %p, PropertyBuffer = %p, PropertyBufferSize = %d, RequiredSize = %p}\n",
            devinfo, DeviceInfoData, Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize, RequiredSize);

    return FALSE;
}

/***********************************************************************
 *		SetupDiGetDeviceRegistryPropertyW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceRegistryPropertyW(
        HDEVINFO  devinfo,
        PSP_DEVINFO_DATA  DeviceInfoData,
        DWORD   Property,
        PDWORD  PropertyRegDataType,
        PBYTE   PropertyBuffer,
        DWORD   PropertyBufferSize,
        PDWORD  RequiredSize)
{
    DevicesHeader * list = (DevicesHeader *)devinfo;
    DeviceHeader *  dev;


    FIXME("stub! {definfo = %p, DeviceInfoData = %p, Property = 0x%08x, PropertyRegDataType = %p, PropertyBuffer = %p, PropertyBufferSize = %d, RequiredSize = %p}\n",
            devinfo, DeviceInfoData, Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize, RequiredSize);

    if (devinfo == NULL || devinfo == INVALID_HANDLE_VALUE || list->magic != SETUP_DEVICE_MAGIC)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (DeviceInfoData == NULL || DeviceInfoData->cbSize != sizeof(SP_DEVINFO_DATA))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }


    dev = (DeviceHeader *)DeviceInfoData->Reserved;

    switch (Property)
    {
        case SPDRP_DRIVER:
            if (dev->type == DISPLAY_DEVICE)
            {
                /* string has the character length of:
                    - a GUID printed in standard format - "{00000000-0000-0000-0000-000000000000}" ((sizeof(GUID) * 2) + 4 + 2)
                    - a path separator - "\\" (+1)
                    - a control set index - "0000" (+4)
                    - a null terminator - "\0" (+1)
                */
                DWORD required = ((sizeof(GUID) * 2) + 4 + 2 + 1 + 4 + 1) * sizeof(WCHAR);
                const WCHAR fmt[] = {'{', '%', '0', '8', 'l', 'x', '-', '%', '0', '4', 'x', '-', '%', '0', '4', 'x', '-',
                                     '%', '0', '2', 'x', '%', '0', '2', 'x', '-', '%', '0', '2', 'x', '%', '0', '2', 'x',
                                     '%', '0', '2', 'x', '%', '0', '2', 'x', '%', '0', '2', 'x', '%', '0', '2', 'x', '}',
                                     '\\', '0', '0', '0', '0', 0};


                if (PropertyRegDataType != NULL)
                    *PropertyRegDataType = REG_SZ;

                if (RequiredSize != NULL)
                    *RequiredSize = required;

                if (PropertyBufferSize < required)
                    SetLastError(ERROR_INSUFFICIENT_BUFFER);

                if (PropertyBuffer != NULL)
                {
                    WCHAR *buffer = (WCHAR *)PropertyBuffer;


                    snprintfW(buffer, PropertyBufferSize, fmt,
                                GUID_DISPLAY_DEVICE.Data1, GUID_DISPLAY_DEVICE.Data2, GUID_DISPLAY_DEVICE.Data3,
                                GUID_DISPLAY_DEVICE.Data4[0], GUID_DISPLAY_DEVICE.Data4[1], GUID_DISPLAY_DEVICE.Data4[2],
                                GUID_DISPLAY_DEVICE.Data4[3], GUID_DISPLAY_DEVICE.Data4[4], GUID_DISPLAY_DEVICE.Data4[5],
                                GUID_DISPLAY_DEVICE.Data4[6], GUID_DISPLAY_DEVICE.Data4[7]);

                    TRACE("returning the string %s\n", debugstr_w(buffer));
                    SetLastError(ERROR_SUCCESS);
                    return TRUE;
                }

                return FALSE;
            }

            FIXME("unsupported device type {type = %d}\n", dev->type);
            break;

        case SPDRP_DEVICEDESC:
        case SPDRP_HARDWAREID:
        case SPDRP_COMPATIBLEIDS:
        case SPDRP_UNUSED0:
        case SPDRP_SERVICE:
        case SPDRP_UNUSED1:
        case SPDRP_UNUSED2:
        case SPDRP_CLASS:
        case SPDRP_CLASSGUID:
        case SPDRP_CONFIGFLAGS:
        case SPDRP_MFG:
        case SPDRP_FRIENDLYNAME:
        case SPDRP_LOCATION_INFORMATION:
        case SPDRP_PHYSICAL_DEVICE_OBJECT_NAME:
        case SPDRP_CAPABILITIES:
        case SPDRP_UI_NUMBER:
        case SPDRP_UPPERFILTERS:
        case SPDRP_LOWERFILTERS:
        case SPDRP_BUSTYPEGUID:
        case SPDRP_LEGACYBUSTYPE:
        case SPDRP_BUSNUMBER:
        case SPDRP_ENUMERATOR_NAME:
        case SPDRP_SECURITY:
        case SPDRP_SECURITY_SDS:
        case SPDRP_DEVTYPE:
        case SPDRP_EXCLUSIVE:
        case SPDRP_CHARACTERISTICS:
        case SPDRP_ADDRESS:
        case SPDRP_UI_NUMBER_DESC_FORMAT:
        case SPDRP_DEVICE_POWER_DATA:
        case SPDRP_REMOVAL_POLICY:
        case SPDRP_REMOVAL_POLICY_HW_DEFAULT:
        case SPDRP_REMOVAL_POLICY_OVERRIDE:
        case SPDRP_INSTALL_STATE:
        case SPDRP_LOCATION_PATHS:
        case SPDRP_BASE_CONTAINERID:
            FIXME("unimplemented property {Property = 0x%08x}\n", Property);
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            break;

        default:
            ERR("unsupported property {Property = 0x%08x}\n", Property);
            SetLastError(ERROR_INVALID_PARAMETER);
            break;
    }

    return FALSE;
}

/***********************************************************************
 *		SetupDiInstallClassA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiInstallClassA(
        HWND hwndParent,
        PCSTR InfFileName,
        DWORD Flags,
        HSPFILEQ FileQueue)
{
    UNICODE_STRING FileNameW;
    BOOL Result;

    if (!RtlCreateUnicodeStringFromAsciiz(&FileNameW, InfFileName))
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    Result = SetupDiInstallClassW(hwndParent, FileNameW.Buffer, Flags, FileQueue);

    RtlFreeUnicodeString(&FileNameW);

    return Result;
}

static HKEY CreateClassKey(HINF hInf)
{
    WCHAR FullBuffer[MAX_PATH];
    WCHAR Buffer[MAX_PATH];
    DWORD RequiredSize;
    HKEY hClassKey;

    if (!SetupGetLineTextW(NULL,
			   hInf,
			   Version,
			   ClassGUID,
			   Buffer,
			   MAX_PATH,
			   &RequiredSize))
    {
	return INVALID_HANDLE_VALUE;
    }

    lstrcpyW(FullBuffer, ControlClass);
    lstrcatW(FullBuffer, Buffer);

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		      FullBuffer,
		      0,
		      KEY_ALL_ACCESS,
		      &hClassKey))
    {
	if (!SetupGetLineTextW(NULL,
			       hInf,
			       Version,
			       Class,
			       Buffer,
			       MAX_PATH,
			       &RequiredSize))
	{
	    return INVALID_HANDLE_VALUE;
	}

	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
			    FullBuffer,
			    0,
			    NULL,
			    REG_OPTION_NON_VOLATILE,
			    KEY_ALL_ACCESS,
			    NULL,
			    &hClassKey,
			    NULL))
	{
	    return INVALID_HANDLE_VALUE;
	}

    }

    if (RegSetValueExW(hClassKey,
		       Class,
		       0,
		       REG_SZ,
		       (LPBYTE)Buffer,
		       RequiredSize * sizeof(WCHAR)))
    {
	RegCloseKey(hClassKey);
	RegDeleteKeyW(HKEY_LOCAL_MACHINE,
		      FullBuffer);
	return INVALID_HANDLE_VALUE;
    }

    return hClassKey;
}

/***********************************************************************
 *		SetupDiInstallClassW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiInstallClassW(
        HWND hwndParent,
        PCWSTR InfFileName,
        DWORD Flags,
        HSPFILEQ FileQueue)
{
    WCHAR SectionName[MAX_PATH];
    DWORD SectionNameLength = 0;
    HINF hInf;
    BOOL bFileQueueCreated = FALSE;
    HKEY hClassKey;


    FIXME("\n");

    if ((Flags & DI_NOVCP) && (FileQueue == NULL || FileQueue == (HSPFILEQ)INVALID_HANDLE_VALUE))
    {
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }

    /* Open the .inf file */
    hInf = SetupOpenInfFileW(InfFileName,
			     NULL,
			     INF_STYLE_WIN4,
			     NULL);
    if (hInf == (HSPFILEQ)INVALID_HANDLE_VALUE)
    {

	return FALSE;
    }

    /* Create or open the class registry key 'HKLM\\CurrentControlSet\\Class\\{GUID}' */
    hClassKey = CreateClassKey(hInf);
    if (hClassKey == INVALID_HANDLE_VALUE)
    {
	SetupCloseInfFile(hInf);
	return FALSE;
    }


    /* Try to append a layout file */
#if 0
    SetupOpenAppendInfFileW(NULL, hInf, NULL);
#endif

    /* Retrieve the actual section name */
    SetupDiGetActualSectionToInstallW(hInf,
				      ClassInstall32,
				      SectionName,
				      MAX_PATH,
				      &SectionNameLength,
				      NULL);

#if 0
    if (!(Flags & DI_NOVCP))
    {
	FileQueue = SetupOpenFileQueue();
	if (FileQueue == INVALID_HANDLE_VALUE)
	{
	    SetupCloseInfFile(hInf);
	    return FALSE;
	}

	bFileQueueCreated = TRUE;

    }
#endif

    SetupInstallFromInfSectionW(0,
				hInf,
				SectionName,
				SPINST_REGISTRY,
				hClassKey,
				NULL,
				0,
				NULL,
				NULL,
				INVALID_HANDLE_VALUE,
				NULL);

    /* FIXME: More code! */

    if (bFileQueueCreated)
	SetupCloseFileQueue(FileQueue);

    SetupCloseInfFile(hInf);

    return TRUE;
}


/***********************************************************************
 *		SetupDiOpenClassRegKey  (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenClassRegKey(
        const GUID* ClassGuid,
        REGSAM samDesired)
{
    return SetupDiOpenClassRegKeyExW(ClassGuid, samDesired,
                                     DIOCR_INSTALLER, NULL, NULL);
}


/***********************************************************************
 *		SetupDiOpenClassRegKeyExA  (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenClassRegKeyExA(
        const GUID* ClassGuid,
        REGSAM samDesired,
        DWORD Flags,
        PCSTR MachineName,
        PVOID Reserved)
{
    PWSTR MachineNameW = NULL;
    HKEY hKey;

    TRACE("\n");

    if (MachineName)
    {
        MachineNameW = MultiByteToUnicode(MachineName, CP_ACP);
        if (MachineNameW == NULL)
            return INVALID_HANDLE_VALUE;
    }

    hKey = SetupDiOpenClassRegKeyExW(ClassGuid, samDesired,
                                     Flags, MachineNameW, Reserved);

    if (MachineNameW)
        MyFree(MachineNameW);

    return hKey;
}


/***********************************************************************
 *		SetupDiOpenClassRegKeyExW  (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenClassRegKeyExW(
        const GUID* ClassGuid,
        REGSAM samDesired,
        DWORD Flags,
        PCWSTR MachineName,
        PVOID Reserved)
{
    RPC_WSTR lpGuidString;
    HKEY hClassesKey;
    HKEY hClassKey;
    LPCWSTR lpKeyName;

    if (MachineName != NULL)
    {
        FIXME("Remote access not supported yet!\n");
        return INVALID_HANDLE_VALUE;
    }

    if (Flags == DIOCR_INSTALLER)
    {
        lpKeyName = ControlClass;
    }
    else if (Flags == DIOCR_INTERFACE)
    {
        lpKeyName = DeviceClasses;
    }
    else
    {
        ERR("Invalid Flags parameter!\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		      lpKeyName,
		      0,
		      KEY_ALL_ACCESS,
		      &hClassesKey))
    {
	return INVALID_HANDLE_VALUE;
    }

    if (ClassGuid == NULL)
        return hClassesKey;

    if (UuidToStringW((UUID*)ClassGuid, &lpGuidString) != RPC_S_OK)
    {
	RegCloseKey(hClassesKey);
	return FALSE;
    }

    if (RegOpenKeyExW(hClassesKey,
		      lpGuidString,
		      0,
		      KEY_ALL_ACCESS,
		      &hClassKey))
    {
	RpcStringFreeW(&lpGuidString);
	RegCloseKey(hClassesKey);
	return FALSE;
    }

    RpcStringFreeW(&lpGuidString);
    RegCloseKey(hClassesKey);

    return hClassKey;
}

/***********************************************************************
 *		SetupDiOpenDeviceInterfaceW (SETUPAPI.@)
 */
BOOL WINAPI SetupDiOpenDeviceInterfaceW(
       HDEVINFO DeviceInfoSet,
       PCWSTR DevicePath,
       DWORD OpenFlags,
       PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData)
{
    FIXME("%p %s %08x %p\n",
        DeviceInfoSet, debugstr_w(DevicePath), OpenFlags, DeviceInterfaceData);
    return FALSE;
}

/***********************************************************************
 *		SetupDiOpenDeviceInterfaceA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiOpenDeviceInterfaceA(
       HDEVINFO DeviceInfoSet,
       PCSTR DevicePath,
       DWORD OpenFlags,
       PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData)
{
    FIXME("%p %s %08x %p\n", DeviceInfoSet,
        debugstr_a(DevicePath), OpenFlags, DeviceInterfaceData);
    return FALSE;
}

/***********************************************************************
 *		SetupDiSetClassInstallParamsA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiSetClassInstallParamsA(
       HDEVINFO  DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       PSP_CLASSINSTALL_HEADER ClassInstallParams,
       DWORD ClassInstallParamsSize)
{
    FIXME("%p %p %x %u\n",DeviceInfoSet, DeviceInfoData,
          ClassInstallParams->InstallFunction, ClassInstallParamsSize);
    return FALSE;
}

/***********************************************************************
 *		SetupDiCallClassInstaller (SETUPAPI.@)
 */
BOOL WINAPI SetupDiCallClassInstaller(
       DI_FUNCTION InstallFunction,
       HDEVINFO DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData)
{
    FIXME("%d %p %p\n", InstallFunction, DeviceInfoSet, DeviceInfoData);
    return FALSE;
}

/***********************************************************************
 *		SetupDiGetDeviceInstallParamsA (SETUPAPI.@)
 */
BOOL WINAPI SetupDiGetDeviceInstallParamsA(
       HDEVINFO DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       PSP_DEVINSTALL_PARAMS_A DeviceInstallParams)
{
    FIXME("%p %p %p\n", DeviceInfoSet, DeviceInfoData, DeviceInstallParams);
    return FALSE;
}

/***********************************************************************
 *		SetupDiOpenDevRegKey (SETUPAPI.@)
 */
HKEY WINAPI SetupDiOpenDevRegKey(
       HDEVINFO DeviceInfoSet,
       PSP_DEVINFO_DATA DeviceInfoData,
       DWORD Scope,
       DWORD HwProfile,
       DWORD KeyType,
       REGSAM samDesired)
{
    FIXME("%p %p %u %u %u %x\n", DeviceInfoSet, DeviceInfoData,
          Scope, HwProfile, KeyType, samDesired);
    return INVALID_HANDLE_VALUE;
}
