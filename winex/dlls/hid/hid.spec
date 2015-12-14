name	hid
type	win32
init	DllMain
rsrc	version.res

import kernel32.dll

@ stdcall HidD_FlushQueue(long)
@ stdcall HidD_FreePreparsedData(ptr)
@ stdcall HidD_GetAttributes(long ptr)
@ stub HidD_GetConfiguration
@ stub HidD_GetFeature
@ stdcall HidD_GetHidGuid(ptr)
@ stdcall HidD_GetIndexedString(long long ptr long)
@ stdcall HidD_GetInputReport(long ptr long)
@ stdcall HidD_GetManufacturerString(long ptr long)
@ stub HidD_GetMsGenreDescriptor
@ stdcall HidD_GetNumInputBuffers(long ptr)
@ stdcall HidD_GetPhysicalDescriptor(long ptr long)
@ stdcall HidD_GetPreparsedData(long ptr)
@ stdcall HidD_GetProductString(long ptr long)
@ stdcall HidD_GetSerialNumberString(long ptr long)
@ stub HidD_Hello
@ stub HidD_SetConfiguration
@ stub HidD_SetFeature
@ stdcall HidD_SetNumInputBuffers(long long)
@ stdcall HidD_SetOutputReport(long ptr long)
@ stub HidP_GetButtonCaps
@ stdcall HidP_GetCaps(ptr ptr)
@ stub HidP_GetData
@ stub HidP_GetExtendedAttributes
@ stub HidP_GetLinkCollectionNodes
@ stub HidP_GetScaledUsageValue
@ stub HidP_GetSpecificButtonCaps
@ stub HidP_GetSpecificValueCaps
@ stub HidP_GetUsageValue
@ stub HidP_GetUsageValueArray
@ stub HidP_GetUsages
@ stub HidP_GetUsagesEx
@ stub HidP_GetValueCaps
@ stub HidP_InitializeReportForID
@ stub HidP_MaxDataListLength
@ stub HidP_MaxUsageListLength
@ stub HidP_SetData
@ stub HidP_SetScaledUsageValue
@ stub HidP_SetUsageValue
@ stub HidP_SetUsageValueArray
@ stub HidP_SetUsages
@ stub HidP_TranslateUsagesToI8042ScanCodes
@ stub HidP_UnsetUsages
@ stub HidP_UsageListDifference
