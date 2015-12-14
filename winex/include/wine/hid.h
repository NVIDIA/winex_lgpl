/*
 * HID Monitoring functions ('hid.h')
 *
 * Copyright (c) 2011-2015 NVIDIA CORPORATION. All rights reserved.
 * Authored by: Remi Dufour
 *
 * The HID Monitor is responsible for maintaining a platform-independent list
 * of currently connected HID devices.  The monitor must be launched using
 * the HID_Init() function.  HID_Shutdown() must also be called on shutdown.
 *
 * On attach and detach events, a device will generate notifications for
 * different device classes:
 *  - a raw USB device
 *  - an HID device
 *  - a type-specific device (if applicable)
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef __HID_MONITOR_H__
# define __HID_MONITOR_H__

# include "winbase.h"

/*
 * Types, macros and enums
 */
# ifndef TYPEOF
#  define TYPEOF(name)   name##_Func
# endif


typedef void * HIDDeviceID; 

typedef enum
{
    USB_RAW_DEVICE_INDEX = 0,
    HID_DEVICE_INDEX,
    TYPE_SPECIFIC_DEVICE_START_INDEX
} NAME_PAIR_INDEX;

typedef struct
{
    UINT page;
    UINT usage;
} hid_usage_pair_t;

typedef struct
{
    GUID  guid;
    WCHAR name[MAX_PATH];
} hid_device_name_pair_t;

typedef struct
{
    USHORT  input;
    USHORT  output;
    USHORT  feature;
} hid_device_count_t;

/* struct hid_device_info_t: contains a subset of the identifying information
     for a device.  This will include the vendor and product ID, and the version
     number. */
typedef struct
{
    hid_usage_pair_t    primary_usage;
    USHORT              vid;
    USHORT              pid;
    UINT                version;
    hid_device_count_t  max_report_size;
    hid_device_count_t  button_counts;
    hid_device_count_t  value_counts;
    hid_device_count_t  data_counts;
    BOOL                uses_report_ids;
} hid_device_info_t;

typedef struct
{
    UINT                    location_id;
    hid_device_info_t       info;
    hid_usage_pair_t       *usage_pairs;
    hid_device_name_pair_t *device_names;
    UINT                    usage_count;
    UINT                    name_count;
    WCHAR                  *transport;
    WCHAR                  *manufacturer;
    WCHAR                  *product_name;
    WCHAR                  *serial;
    HIDDeviceID             reference_id;
} hid_device_t;


/***********************************************************************
 *           HID_ENUM_APPLIER_FUNC()
 *
 * device enumeration function prototype.
 *
 * Parameters:
 *  <device>    [in] pointer to a device information block.
 *  <user_data> [in] the user data pointer passed to HID_DeviceEnumerate().
 *
 * Returns:
 *  TRUE if the enumeration should continue.
 *
 *  FALSE if the enumeration should stop immediately.
 *
 * Remarks:
 *  This is the prototype for the caller provided callback function used
 *  for device enumeration.  The function should execute as quickly as
 *  possible to prevent the device list from being locked for longer than
 *  necessary (since device removals and additions are inherently
 *  asynchronous).
 */
typedef BOOL (* HID_ENUM_APPLIER_FUNC) (const hid_device_t *device,
                                        void *              user_data);

/***********************************************************************
 *           HID_NOTIFICATION_FUNC()
 *
 * device change notification function prototype.
 *
 * Parameters:
 *  <device>            [in] the device that was added or removed.
 *  <added>             [in] TRUE if the device was added to the system.
 *                      FALSE if the device was removed from the system.
 *  <change_complete>   [in] FALSE if the device list is still changing.
 *                      TRUE if the change is complete and the list can
 *                      be accessed again.
 *  <user_data>         [in] user context value that was provided when
 *                      the callback was registered.
 *
 * Returns:
 *  No return value.
 *
 * Remarks:
 *  This function is used as a notification each time the HID device
 *  list is  modified.  Each device that changes will make one notification
 *  call.
 *
 *  The <change_complete> parameter allos the notification function a
 *  chance to know when it is safe to attempt to access the device list
 *  again.  When the parameter is FALSE, it is not safe to access the
 *  device list from a different thread.  When the parameter is TRUE,
 *  the device list can be accessed again.  Note that on device removals,
 *  the <device> parameter will be NULL when <change_complete> is TRUE.
 */
typedef void (* HID_NOTIFICATION_FUNC) (const hid_device_t *device,
                                        BOOL                added,
                                        BOOL                change_complete,
                                        void *              user_data);


/***********************************************************************
 *           HID_Init()
 *
 *  Kicks-off and Initializes the HID monitoring thread.
 */
void HID_Init(void);


/***********************************************************************
 *           HID_Shutdown()
 *
 *  Shuts down the HID monitoring thread.
 */
void HID_Shutdown(void);


/***********************************************************************
 *           HID_DeviceEnumerate()
 *
 *  Enumerates the HID devices using a callback function.
 *
 * Parameters:
 *  <function>         [in] pointer to an enum callback function.
 *  <user_data>        [in] pointer to user_data.
 */
void HID_DeviceEnumerate(HID_ENUM_APPLIER_FUNC function, void *user_data);


/***********************************************************************
 *           HID_GetDeviceCount()
 *
 * Retrieves the HID device count.
 */
UINT HID_GetDeviceCount(void);

/***********************************************************************
 *           HID_FindDeviceIDByName()
 *
 * attempts to find a device's ID based on its filename.
 *
 * Parameters:
 *  <name>  [in] name of the device to attempt to find.  This will be
 *          the device's filename.
 *  <id>    [out] receives the platform-specific ID of the device.
 *
 * Returns:
 *  TRUE if the device was found in the list and the ID was returned.
 *
 *  FALSE if the device was not found in the list.
 *
 * Remarks:
 *  This function is used to look up a device's ID by its filename.  The
 *  name will be the same one that was passed to CreateFile() when opening
 *  a handle to it.
 *
 *  If the device has been removed from the system since the name was
 *  retrieved, this lookup will fail.  Callers must handle this case
 *  appropriately.
 */
BOOL HID_FindDeviceIDByName(LPCWSTR name, HIDDeviceID *id);

/***********************************************************************
 *           HID_GetDeviceInfo()
 *
 * retrieves a device's basic identifying information.
 *
 * Parameters:
 *  <device>    [in] the platform-specific ID of the device to retrieve info for.
 *  <info>      [out] buffer to receive the device's information.
 *
 * Returns:
 *  TRUE if the device was found in the list and the info was returned.
 *
 *  FALSE if the device was not found in the list.
 *
 * Remarks:
 *  This function attempts to retrieve the identifying information for a device
 *  based on its platform specific ID.  If the device is found in the list, the
 *  information will be returned in the <info> buffer.  The buffer will be
 *  cleared to zero if the device cannot be found.
 */
BOOL HID_GetDeviceInfo(HIDDeviceID device, hid_device_info_t *info);


/***********************************************************************
 *              HID_SetUsesReportIDs()
 *
 * marks that a device uses report IDs in all of its reports.
 *
 * Parameters:
 *  <device>    [in] the platform-specific ID of the device to retrieve info for.
 *
 * Returns:
 *  TRUE if the device was found in the list and the report sizes updated.
 *
 *  FALSE if the device was not found in the list.
 *
 * Remarks:
 *  This function attempts to mark that a device uses report IDs in all of its
 *  reports and corrects the size of each supported report type to account for
 *  that.  This function is expected to be called by backend report handlers
 *  when it is discovered that a particular device uses report IDs.  The report
 *  sizes will be correctly set for any future open instances of the same device.
 *
 *  This function will ignore any attempts to change a device's report size if
 *  it has already been done.
 */
BOOL HID_SetUsesReportIDs(HIDDeviceID device);


/***********************************************************************
 *           HID_RegisterChangeNotification()
 *
 * Registers a notification callback function.
 *
 * Parameter:
 *  <function>    [in] the notification function to register.
 *  <user_data>   [in] pointer to user_data.  This parameter should be
 *                a unique value if multiple callbacks for the same
 *                function are to be registered.
 *
 * Returns:
 *  TRUE if the notification function was registered successfully.
 *
 *  FALSE otherwise.
 *
 * Remarks:
 *  This registers a callback function that will be performed each time
 *  a new HID device is added to or removed from the system.  The user
 *  data context pointer must be unique if a single function is to be
 *  registered more than once.  Failing to do so will result in undefined
 *  behaviour.  For example, if the function DeviceNotify() is registered
 *  twice, it cannot be registered with the context pointer NULL or the
 *  same pointer twice.
 */
BOOL HID_RegisterChangeNotification(HID_NOTIFICATION_FUNC function, void *user_data);


/***********************************************************************
 *           HID_UnregisterChangeNotification()
 *
 * Unregisters a notification callback function.
 *
 * Parameter:
 *  <function>    [in] the notification function to unregister.
 *  <user_data>   [in] the same user data context pointer that was used
 *                to register the callback.
 *
 * Returns:
 *  TRUE if the notification function was unregistered successfully.
 *
 *  FALSE otherwise.
 *
 * Remarks:
 *  This function unregisters a device change notification function that
 *  was previously registered with HID_RegisterChangeNotification().  The
 *  user data context pointer must match the value that was originally
 *  used when the callback was originally registered.  Failing to do this
 *  will result in undefined behaviour.
 */
BOOL HID_UnregisterChangeNotification(HID_NOTIFICATION_FUNC function, const void *user_data);


/* Prototype definitions for the functions in this header */
typedef void (* TYPEOF(HID_Init)) (void);
typedef void (* TYPEOF(HID_Shutdown)) (void);
typedef void (* TYPEOF(HID_DeviceEnumerate)) (HID_ENUM_APPLIER_FUNC function, void *user_data);
typedef void (* TYPEOF(HID_GetDeviceCount)) (void);
typedef BOOL (* TYPEOF(HID_FindDeviceIDByName)) (LPCWSTR name, HIDDeviceID *id);
typedef BOOL (* TYPEOF(HID_GetDeviceInfo)) (HIDDeviceID device, hid_device_info_t *info);
typedef BOOL (* TYPEOF(HID_RegisterChangeNotification)) (HID_NOTIFICATION_FUNC function, void *user_data);
typedef BOOL (* TYPEOF(HID_UnregisterChangeNotification)) (HID_NOTIFICATION_FUNC function);

#endif
