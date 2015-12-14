/* main.c
 * Human Input Devices
 *
 * Copyright (c) 2011-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "config.h"

#include <stdarg.h>

#include "initguid.h"
#include "windef.h"
#include "winternl.h"
#include "wine/hiddevice.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(hid);

DEFINE_GUID(HID_GUID, 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);


/* struct HIDDEV_PreparsedData: internal representation of an opaque object passed back to the
     caller when querying an HID device for information.  This struct should contain all of the
     information that can be retrieved from the various HidP_Get*() functions. */
typedef struct
{
    HANDLE              device;
    hid_device_info_t   info;
    /* FIXME!! add more members to provide values for other device queries. */
} HIDDEV_PreparsedData;


/***********************************************************************/

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch(fdwReason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

void WINAPI HidD_GetHidGuid(LPGUID guid)
{
    TRACE("{guid = %p}\n", guid);

    *guid = HID_GUID;
}

BOOLEAN WINAPI HidD_GetAttributes(HANDLE device, PHIDD_ATTRIBUTES attributes)
{
    hid_device_info_t   devInfo;
    NTSTATUS            status;


    TRACE("{device = 0x%08x, attributes = %p}\n", device, attributes);

    status = HIDDEV_getInfo(device, &devInfo);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the device info for the handle 0x%08x {status = 0x%08x}\n", device, status);
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }


    /* MSDN claims the <Size> member must be set before calling this function, but that's
       clearly not the case from testing.  No matter what value this is set to before the
       call, it will be clobbered to sizeof(HIDD_ATTRIBUTES) on a successful return. */
    attributes->Size = sizeof(HIDD_ATTRIBUTES);

    attributes->VendorID = devInfo.vid;
    attributes->ProductID = devInfo.pid;
    attributes->VersionNumber = (USHORT)devInfo.version;

    return TRUE;
}

BOOLEAN WINAPI HidD_SetNumInputBuffers(HANDLE device, ULONG numReports)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x}\n", device);

    status = HIDDEV_setNumInputBuffers(device, numReports);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to set the input queue size for the handle 0x%08x {status = 0x%08x}\n", device, status);
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_GetNumInputBuffers(HANDLE device, PULONG numReports)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x}\n", device);

    status = HIDDEV_getNumInputBuffers(device, numReports);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to get the input queue size for the handle 0x%08x {status = 0x%08x}\n", device, status);
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_FlushQueue(HANDLE device)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x}\n", device);

    status = HIDDEV_flushInputBuffers(device);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to flush the input queue for the handle 0x%08x {status = 0x%08x}\n", device, status);
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_SetOutputReport(HANDLE device, PVOID buffer, ULONG length)
{
    NTSTATUS    status;
    DWORD       bytesWritten;


    TRACE("{device = 0x%08x, buffer = %p, length = %u}\n", device, buffer, length);

    status = HIDDEV_write(device, buffer, length, &bytesWritten, NULL, 0, FALSE);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to write a report\n");
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_GetInputReport(HANDLE device, PVOID buffer, ULONG length)
{
    NTSTATUS    status;
    DWORD       bytesRead;
    BYTE *      reportBuffer = (BYTE *)buffer;


    TRACE("{device = 0x%08x, buffer = %p, length = %u}\n", device, buffer, length);

    /* attempt to read a report from the device.  Note that for devices that support report IDs,
       this will remove the report ID byte from each report that is returned.  The report size
       will remain the same, but the last byte of each report will be cleared to 0.  The report
       will be returned unmodified for devices that do not support report IDs. */
    status = HIDDEV_read(device, buffer, length, &bytesRead, NULL, 0, buffer != NULL ? reportBuffer[0] : 0, FALSE);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to read a report {status = 0x%08x}\n", status);
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}


/************************* device string and info retrieval functions ****************************/
BOOLEAN WINAPI HidD_GetManufacturerString(HANDLE device, PVOID buffer, ULONG bufferLength)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x, buffer = %p, bufferLength = %u}\n", device, buffer, bufferLength);

    status = HIDDEV_getString(device, HIDDEV_STRING_MANUFACTURER, buffer, bufferLength);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the string\n");
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_GetPhysicalDescriptor(HANDLE device, PVOID buffer, ULONG bufferLength)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x, buffer = %p, bufferLength = %u}\n", device, buffer, bufferLength);

    status = HIDDEV_getString(device, HIDDEV_STRING_DESCRIPTOR, buffer, bufferLength);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the string\n");
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_GetProductString(HANDLE device, PVOID buffer, ULONG bufferLength)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x, buffer = %p, bufferLength = %u}\n", device, buffer, bufferLength);

    status = HIDDEV_getString(device, HIDDEV_STRING_PRODUCT, buffer, bufferLength);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the string\n");
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_GetSerialNumberString(HANDLE device, PVOID buffer, ULONG bufferLength)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x, buffer = %p, bufferLength = %u}\n", device, buffer, bufferLength);

    status = HIDDEV_getString(device, HIDDEV_STRING_SERIAL, buffer, bufferLength);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the string\n");
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

BOOLEAN WINAPI HidD_GetIndexedString(HANDLE device, ULONG stringIndex, PVOID buffer, ULONG bufferLength)
{
    NTSTATUS status;


    TRACE("{device = 0x%08x, stringIndex = %u, buffer = %p, bufferLength = %u}\n", device, stringIndex, buffer, bufferLength);

    status = HIDDEV_getString(device, stringIndex, buffer, bufferLength);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the string\n");
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

NTSTATUS WINAPI HidP_GetCaps(PHIDP_PREPARSED_DATA preparsedData, PHIDP_CAPS caps)
{
    HIDDEV_PreparsedData *data;


    TRACE("{preparsedData = %p, caps = %p}\n", preparsedData, caps);

    if (preparsedData == NULL || caps == NULL)
        return HIDP_STATUS_INVALID_PREPARSED_DATA;


    data = (HIDDEV_PreparsedData *)preparsedData;

    caps->Usage =                     data->info.primary_usage.usage;
    caps->UsagePage =                 data->info.primary_usage.page;
    caps->InputReportByteLength =     data->info.max_report_size.input;
    caps->OutputReportByteLength =    data->info.max_report_size.output;
    caps->FeatureReportByteLength =   data->info.max_report_size.feature;
    caps->NumberLinkCollectionNodes = 0;
    caps->NumberInputButtonCaps =     data->info.button_counts.input;
    caps->NumberInputValueCaps =      data->info.value_counts.input;
    caps->NumberInputDataIndices =    data->info.data_counts.input;
    caps->NumberOutputButtonCaps =    data->info.button_counts.output;
    caps->NumberOutputValueCaps =     data->info.value_counts.output;
    caps->NumberOutputDataIndices =   data->info.data_counts.output;
    caps->NumberFeatureButtonCaps =   data->info.button_counts.feature;
    caps->NumberFeatureValueCaps =    data->info.value_counts.feature;
    caps->NumberFeatureDataIndices =  data->info.data_counts.feature;

    memset(caps->Reserved, 0, sizeof(caps->Reserved));

    return STATUS_SUCCESS;
}

BOOLEAN WINAPI HidD_GetPreparsedData(HANDLE device, PHIDP_PREPARSED_DATA *data)
{
    HIDDEV_PreparsedData *  info;
    NTSTATUS                status;


    TRACE("{device = 0x%08x, data = %p}\n", device, data);

    /* nothing to return the info block in => fail */
    if (data == NULL)
    {
        ERR("invalid output buffer\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }


    info = HeapAlloc(GetProcessHeap(), 0, sizeof(HIDDEV_PreparsedData));

    if (info == NULL)
    {
        ERR("failed to allocate memory for a preparsed data buffer\n");
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }


    info->device = device;

    status = HIDDEV_getInfo(device, &info->info);

    if (status != STATUS_SUCCESS)
    {
        TRACE("failed to retrieve the device info\n");
        HeapFree(GetProcessHeap(), 0, info);
        SetLastError(RtlNtStatusToDosError(status));
        return FALSE;
    }


    *data = (PHIDP_PREPARSED_DATA)info;
    TRACE("created the preparsed data block %p\n", info);

    return TRUE;
}

BOOLEAN WINAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA data)
{
    TRACE("{data = %p}\n", data);

    if (data == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    HeapFree(GetProcessHeap(), 0, data);

    return TRUE;
}
