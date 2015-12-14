
typedef struct
{
    GUID  guid;
    WCHAR name[MAX_PATH];
} hid_device_name_pair_t;

typedef struct
{
	hid_device_name_pair_t *device_names;
	
} hid_device_t;

#define HID_DEVICE_INDEX 1

typedef BOOL (* HID_ENUM_APPLIER_FUNC) (const hid_device_t *device, void *user_data);

extern int HID_GetDeviceCount();
extern void HID_DeviceEnumerate(HID_ENUM_APPLIER_FUNC function, void *user_data);
