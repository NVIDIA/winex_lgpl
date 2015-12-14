name setupapi
type win32
rsrc setupapi.res

import user32.dll
import advapi32.dll
import kernel32.dll
import ntdll.dll
import version.dll
import rpcrt4.dll
import shell32.dll
import wow32.dll


@ stub AcquireSCMLock
@ stub AddMiniIconToList
@ stub AddTagToGroupOrderListEntry
@ stub AppendStringToMultiSz
@ stub AssertFail
@ stub CMP_Init_Detection
@ stub CMP_RegisterNotification
@ stub CMP_Report_LogOn
@ stub CMP_UnregisterNotification
@ stub CMP_WaitNoPendingInstallEvents
@ stub CMP_WaitServices
@ stub CM_Add_Empty_Log_Conf
@ stub CM_Add_Empty_Log_Conf_Ex
@ stub CM_Add_IDA
@ stub CM_Add_IDW
@ stub CM_Add_ID_ExA
@ stub CM_Add_ID_ExW
@ stub CM_Add_Range
@ stub CM_Add_Res_Des
@ stub CM_Add_Res_Des_Ex
@ stub CM_Connect_MachineA
@ stdcall CM_Connect_MachineW(wstr ptr) CM_Connect_MachineW
@ stub CM_Create_DevNodeA
@ stub CM_Create_DevNodeW
@ stub CM_Create_DevNode_ExA
@ stub CM_Create_DevNode_ExW
@ stub CM_Create_Range_List
@ stub CM_Delete_Class_Key
@ stub CM_Delete_Class_Key_Ex
@ stub CM_Delete_DevNode_Key
@ stub CM_Delete_DevNode_Key_Ex
@ stub CM_Delete_Range
@ stub CM_Detect_Resource_Conflict
@ stub CM_Detect_Resource_Conflict_Ex
@ stub CM_Disable_DevNode
@ stub CM_Disable_DevNode_Ex
@ stdcall CM_Disconnect_Machine(long) CM_Disconnect_Machine
@ stub CM_Dup_Range_List
@ stub CM_Enable_DevNode
@ stub CM_Enable_DevNode_Ex
@ stub CM_Enumerate_Classes
@ stub CM_Enumerate_Classes_Ex
@ stub CM_Enumerate_EnumeratorsA
@ stub CM_Enumerate_EnumeratorsW
@ stub CM_Enumerate_Enumerators_ExA
@ stub CM_Enumerate_Enumerators_ExW
@ stub CM_Find_Range
@ stub CM_First_Range
@ stub CM_Free_Log_Conf
@ stub CM_Free_Log_Conf_Ex
@ stub CM_Free_Log_Conf_Handle
@ stub CM_Free_Range_List
@ stub CM_Free_Res_Des
@ stub CM_Free_Res_Des_Ex
@ stub CM_Free_Res_Des_Handle
@ stub CM_Get_Child
@ stub CM_Get_Child_Ex
@ stub CM_Get_Class_Key_NameA
@ stub CM_Get_Class_Key_NameW
@ stub CM_Get_Class_Key_Name_ExA
@ stub CM_Get_Class_Key_Name_ExW
@ stub CM_Get_Class_NameA
@ stub CM_Get_Class_NameW
@ stub CM_Get_Class_Name_ExA
@ stub CM_Get_Class_Name_ExW
@ stub CM_Get_Depth
@ stub CM_Get_Depth_Ex
@ stub CM_Get_DevNode_Registry_PropertyA
@ stub CM_Get_DevNode_Registry_PropertyW
@ stub CM_Get_DevNode_Registry_Property_ExA
@ stub CM_Get_DevNode_Registry_Property_ExW
@ stub CM_Get_DevNode_Status
@ stub CM_Get_DevNode_Status_Ex
@ stub CM_Get_Device_IDA
@ stub CM_Get_Device_IDW
@ stub CM_Get_Device_ID_ExA
@ stub CM_Get_Device_ID_ExW
@ stdcall CM_Get_Device_ID_ListA(ptr ptr long long) CM_Get_Device_ID_ListA
@ stub CM_Get_Device_ID_ListW
@ stub CM_Get_Device_ID_List_ExA
@ stub CM_Get_Device_ID_List_ExW
@ stub CM_Get_Device_ID_List_SizeA
@ stub CM_Get_Device_ID_List_SizeW
@ stub CM_Get_Device_ID_List_Size_ExA
@ stub CM_Get_Device_ID_List_Size_ExW
@ stub CM_Get_Device_ID_Size
@ stub CM_Get_Device_ID_Size_Ex
@ stub CM_Get_Device_Interface_AliasA
@ stub CM_Get_Device_Interface_AliasW
@ stub CM_Get_Device_Interface_Alias_ExA
@ stub CM_Get_Device_Interface_Alias_ExW
@ stub CM_Get_Device_Interface_ListA
@ stub CM_Get_Device_Interface_ListW
@ stub CM_Get_Device_Interface_List_ExA
@ stub CM_Get_Device_Interface_List_ExW
@ stub CM_Get_Device_Interface_List_SizeA
@ stub CM_Get_Device_Interface_List_SizeW
@ stub CM_Get_Device_Interface_List_Size_ExA
@ stub CM_Get_Device_Interface_List_Size_ExW
@ stub CM_Get_First_Log_Conf
@ stub CM_Get_First_Log_Conf_Ex
@ stub CM_Get_Global_State
@ stub CM_Get_Global_State_Ex
@ stub CM_Get_HW_Prof_FlagsA
@ stub CM_Get_HW_Prof_FlagsW
@ stub CM_Get_HW_Prof_Flags_ExA
@ stub CM_Get_HW_Prof_Flags_ExW
@ stub CM_Get_Hardware_Profile_InfoA
@ stub CM_Get_Hardware_Profile_InfoW
@ stub CM_Get_Hardware_Profile_Info_ExA
@ stub CM_Get_Hardware_Profile_Info_ExW
@ stub CM_Get_Log_Conf_Priority
@ stub CM_Get_Log_Conf_Priority_Ex
@ stub CM_Get_Next_Log_Conf
@ stub CM_Get_Next_Log_Conf_Ex
@ stub CM_Get_Next_Res_Des
@ stub CM_Get_Next_Res_Des_Ex
@ stub CM_Get_Parent
@ stub CM_Get_Parent_Ex
@ stub CM_Get_Res_Des_Data
@ stub CM_Get_Res_Des_Data_Ex
@ stub CM_Get_Res_Des_Data_Size
@ stub CM_Get_Res_Des_Data_Size_Ex
@ stub CM_Get_Sibling
@ stub CM_Get_Sibling_Ex
@ stub CM_Get_Version
@ stub CM_Get_Version_Ex
@ stub CM_Intersect_Range_List
@ stub CM_Invert_Range_List
@ stub CM_Is_Dock_Station_Present
@ stub CM_Locate_DevNodeA
@ stub CM_Locate_DevNodeW
@ stdcall CM_Locate_DevNode_ExA(ptr str long long)
@ stdcall CM_Locate_DevNode_ExW(ptr wstr long long)
@ stub CM_Merge_Range_List
@ stub CM_Modify_Res_Des
@ stub CM_Modify_Res_Des_Ex
@ stub CM_Move_DevNode
@ stub CM_Move_DevNode_Ex
@ stub CM_Next_Range
@ stub CM_Open_Class_KeyA
@ stub CM_Open_Class_KeyW
@ stub CM_Open_Class_Key_ExA
@ stub CM_Open_Class_Key_ExW
@ stub CM_Open_DevNode_Key
@ stub CM_Open_DevNode_Key_Ex
@ stub CM_Query_Arbitrator_Free_Data
@ stub CM_Query_Arbitrator_Free_Data_Ex
@ stub CM_Query_Arbitrator_Free_Size
@ stub CM_Query_Arbitrator_Free_Size_Ex
@ stub CM_Query_Remove_SubTree
@ stub CM_Query_Remove_SubTree_Ex
@ stub CM_Reenumerate_DevNode
@ stub CM_Reenumerate_DevNode_Ex
@ stub CM_Register_Device_Driver
@ stub CM_Register_Device_Driver_Ex
@ stub CM_Register_Device_InterfaceA
@ stub CM_Register_Device_InterfaceW
@ stub CM_Register_Device_Interface_ExA
@ stub CM_Register_Device_Interface_ExW
@ stub CM_Remove_SubTree
@ stub CM_Remove_SubTree_Ex
@ stub CM_Remove_Unmarked_Children
@ stub CM_Remove_Unmarked_Children_Ex
@ stub CM_Request_Device_EjectA
@ stub CM_Request_Device_EjectW
@ stub CM_Request_Eject_PC
@ stub CM_Reset_Children_Marks
@ stub CM_Reset_Children_Marks_Ex
@ stub CM_Run_Detection
@ stub CM_Run_Detection_Ex
@ stub CM_Set_DevNode_Problem
@ stub CM_Set_DevNode_Problem_Ex
@ stub CM_Set_DevNode_Registry_PropertyA
@ stub CM_Set_DevNode_Registry_PropertyW
@ stub CM_Set_DevNode_Registry_Property_ExA
@ stub CM_Set_DevNode_Registry_Property_ExW
@ stub CM_Set_HW_Prof
@ stub CM_Set_HW_Prof_Ex
@ stub CM_Set_HW_Prof_FlagsA
@ stub CM_Set_HW_Prof_FlagsW
@ stub CM_Set_HW_Prof_Flags_ExA
@ stub CM_Set_HW_Prof_Flags_ExW
@ stub CM_Setup_DevNode
@ stub CM_Setup_DevNode_Ex
@ stub CM_Test_Range_Available
@ stub CM_Uninstall_DevNode
@ stub CM_Uninstall_DevNode_Ex
@ stub CM_Unregister_Device_InterfaceA
@ stub CM_Unregister_Device_InterfaceW
@ stub CM_Unregister_Device_Interface_ExA
@ stub CM_Unregister_Device_Interface_ExW
@ stdcall CaptureAndConvertAnsiArg(str ptr) CaptureAndConvertAnsiArg
@ stdcall CaptureStringArg(wstr ptr) CaptureStringArg
@ stub CenterWindowRelativeToParent
@ stub ConcatenatePaths
@ stdcall DelayedMove(wstr wstr) DelayedMove
@ stub DelimStringToMultiSz
@ stub DestroyTextFileReadBuffer
@ stdcall DoesUserHavePrivilege(wstr) DoesUserHavePrivilege
@ stdcall DuplicateString(wstr) DuplicateString
@ stdcall EnablePrivilege(wstr long) EnablePrivilege
@ stub ExtensionPropSheetPageProc
@ stdcall FileExists(wstr ptr) FileExists
@ stub FreeStringArray
@ stub GetCurrentDriverSigningPolicy
@ stub GetNewInfName
@ stub GetSetFileTimestamp
@ stub GetVersionInfoFromImage
@ stub InfIsFromOemLocation
@ stub InstallCatalog
@ stdcall InstallHinfSection(long long str long) InstallHinfSectionA
@ stdcall InstallHinfSectionA(long long str long) InstallHinfSectionA
@ stdcall InstallHinfSectionW(long long wstr long) InstallHinfSectionW
@ stub InstallStop
@ stub InstallStopEx
@ stdcall IsUserAdmin() IsUserAdmin
@ stub LookUpStringInTable
@ stub MemoryInitialize
@ stdcall MultiByteToUnicode(str long) MultiByteToUnicode
@ stub MultiSzFromSearchControl
@ stdcall MyFree(ptr) MyFree
@ stub MyGetFileTitle
@ stdcall MyMalloc(long) MyMalloc
@ stdcall MyRealloc(ptr long) MyRealloc
@ stdcall OpenAndMapFileForRead(wstr ptr ptr ptr ptr) OpenAndMapFileForRead
@ stub OutOfMemory
@ stub QueryMultiSzValueToArray
@ stdcall QueryRegistryValue(long wstr ptr ptr ptr) QueryRegistryValue
@ stub ReadAsciiOrUnicodeTextFile
@ stub RegistryDelnode
@ stdcall RetreiveFileSecurity(wstr ptr) RetreiveFileSecurity
@ stub RetrieveServiceConfig
@ stub SearchForInfFile
@ stub SetArrayToMultiSzValue
@ stdcall SetupAddInstallSectionToDiskSpaceListA(long long long str ptr long) SetupAddInstallSectionToDiskSpaceListA
@ stub SetupAddInstallSectionToDiskSpaceListW
@ stub SetupAddSectionToDiskSpaceListA
@ stub SetupAddSectionToDiskSpaceListW
@ stub SetupAddToDiskSpaceListA
@ stub SetupAddToDiskSpaceListW
@ stub SetupAddToSourceListA
@ stub SetupAddToSourceListW
@ stub SetupAdjustDiskSpaceListA
@ stub SetupAdjustDiskSpaceListW
@ stub SetupCancelTemporarySourceList
@ stdcall SetupCloseFileQueue(ptr) SetupCloseFileQueue
@ stdcall SetupCloseInfFile(long) SetupCloseInfFile
@ stub SetupCloseLog
@ stdcall SetupCommitFileQueue(long long ptr ptr) SetupCommitFileQueueW
@ stdcall SetupCommitFileQueueA(long long ptr ptr) SetupCommitFileQueueA
@ stdcall SetupCommitFileQueueW(long long ptr ptr) SetupCommitFileQueueW
@ stdcall SetupCopyErrorA(long str str str str str long long str long ptr) SetupCopyErrorA
@ stdcall SetupCopyErrorW(long wstr wstr wstr wstr wstr long long wstr long ptr) SetupCopyErrorW
@ stdcall SetupCopyOEMInfA(str str long long ptr long ptr ptr) SetupCopyOEMInfA
@ stdcall SetupCopyOEMInfW(wstr wstr long long ptr long ptr ptr) SetupCopyOEMInfW
@ stdcall SetupCreateDiskSpaceListA(ptr long long) SetupCreateDiskSpaceListA
@ stdcall SetupCreateDiskSpaceListW(ptr long long) SetupCreateDiskSpaceListW
@ stub SetupDecompressOrCopyFileA
@ stub SetupDecompressOrCopyFileW
@ stub SetupDefaultQueueCallback
@ stdcall SetupDefaultQueueCallbackA(ptr long long long) SetupDefaultQueueCallbackA
@ stdcall SetupDefaultQueueCallbackW(ptr long long long) SetupDefaultQueueCallbackW
@ stdcall SetupDeleteErrorA(long str str long long) SetupDeleteErrorA
@ stdcall SetupDeleteErrorW(long wstr wstr long long) SetupDeleteErrorW
@ stdcall SetupDestroyDiskSpaceList(long) SetupDestroyDiskSpaceList
@ stub SetupDiAskForOEMDisk
@ stdcall SetupDiBuildClassInfoList(long ptr long ptr) SetupDiBuildClassInfoList
@ stdcall SetupDiBuildClassInfoListExA(long ptr long ptr str ptr) SetupDiBuildClassInfoListExA
@ stdcall SetupDiBuildClassInfoListExW(long ptr long ptr wstr ptr) SetupDiBuildClassInfoListExW
@ stub SetupDiBuildDriverInfoList
@ stdcall SetupDiCallClassInstaller(long ptr ptr) SetupDiCallClassInstaller
@ stub SetupDiCancelDriverInfoSearch
@ stub SetupDiChangeState
@ stdcall SetupDiClassGuidsFromNameA(str ptr long ptr) SetupDiClassGuidsFromNameA
@ stdcall SetupDiClassGuidsFromNameExA(str ptr long ptr str ptr) SetupDiClassGuidsFromNameExA
@ stdcall SetupDiClassGuidsFromNameExW(wstr ptr long ptr wstr ptr) SetupDiClassGuidsFromNameExW
@ stdcall SetupDiClassGuidsFromNameW(wstr ptr long ptr) SetupDiClassGuidsFromNameW
@ stdcall SetupDiClassNameFromGuidA(ptr str long ptr) SetupDiClassNameFromGuidA
@ stdcall SetupDiClassNameFromGuidExA(ptr str long ptr wstr ptr) SetupDiClassNameFromGuidExA
@ stdcall SetupDiClassNameFromGuidExW(ptr wstr long ptr wstr ptr) SetupDiClassNameFromGuidExW
@ stdcall SetupDiClassNameFromGuidW(ptr wstr long ptr) SetupDiClassNameFromGuidW
@ stub SetupDiCreateDevRegKeyA
@ stub SetupDiCreateDevRegKeyW
@ stub SetupDiCreateDeviceInfoA
@ stdcall SetupDiCreateDeviceInfoList(ptr ptr) SetupDiCreateDeviceInfoList
@ stdcall SetupDiCreateDeviceInfoListExA(ptr long str ptr) SetupDiCreateDeviceInfoListExA
@ stdcall SetupDiCreateDeviceInfoListExW(ptr long str ptr) SetupDiCreateDeviceInfoListExW
@ stub SetupDiCreateDeviceInfoW
@ stub SetupDiDeleteDevRegKey
@ stub SetupDiDeleteDeviceInfo
@ stub SetupDiDeleteDeviceInterfaceData
@ stub SetupDiDeleteDeviceRegKey
@ stub SetupDiDestroyClassImageList
@ stdcall SetupDiDestroyDeviceInfoList(long) SetupDiDestroyDeviceInfoList
@ stub SetupDiDestroyDriverInfoList
@ stub SetupDiDrawMiniIcon
@ stdcall SetupDiEnumDeviceInfo(ptr long ptr) SetupDiEnumDeviceInfo
@ stdcall SetupDiEnumDeviceInterfaces(long ptr ptr long ptr) SetupDiEnumDeviceInterfaces
@ stub SetupDiEnumDriverInfoA
@ stub SetupDiEnumDriverInfoW
@ stdcall SetupDiGetActualSectionToInstallA(long str str long ptr ptr) SetupDiGetActualSectionToInstallA
@ stdcall SetupDiGetActualSectionToInstallW(long wstr wstr long ptr ptr) SetupDiGetActualSectionToInstallW
@ stub SetupDiGetClassBitmapIndex
@ stdcall SetupDiGetClassDescriptionA(ptr str long ptr) SetupDiGetClassDescriptionA
@ stdcall SetupDiGetClassDescriptionExA(ptr str long ptr str ptr) SetupDiGetClassDescriptionExA
@ stdcall SetupDiGetClassDescriptionExW(ptr wstr long ptr wstr ptr) SetupDiGetClassDescriptionExW
@ stdcall SetupDiGetClassDescriptionW(ptr wstr long ptr) SetupDiGetClassDescriptionW
@ stub SetupDiGetClassDevPropertySheetsA
@ stub SetupDiGetClassDevPropertySheetsW
@ stdcall SetupDiGetClassDevsA(ptr ptr long long) SetupDiGetClassDevsA
@ stdcall SetupDiGetClassDevsExA(ptr str ptr long ptr str ptr) SetupDiGetClassDevsExA
@ stdcall SetupDiGetClassDevsExW(ptr wstr ptr long ptr wstr ptr) SetupDiGetClassDevsExW
@ stdcall SetupDiGetClassDevsW(ptr ptr long long) SetupDiGetClassDevsW
@ stub SetupDiGetClassImageIndex
@ stub SetupDiGetClassImageList
@ stub SetupDiGetClassImageListExA
@ stub SetupDiGetClassImageListExW
@ stub SetupDiGetClassInstallParamsA
@ stub SetupDiGetClassInstallParamsW
@ stub SetupDiGetDeviceInfoListClass
@ stdcall SetupDiGetDeviceInfoListDetailA(ptr ptr) SetupDiGetDeviceInfoListDetailA
@ stdcall SetupDiGetDeviceInfoListDetailW(ptr ptr) SetupDiGetDeviceInfoListDetailW
@ stdcall SetupDiGetDeviceInstallParamsA(ptr ptr ptr) SetupDiGetDeviceInstallParamsA
@ stub SetupDiGetDeviceInstallParamsW
@ stub SetupDiGetDeviceInstanceIdA
@ stub SetupDiGetDeviceInstanceIdW
@ stub SetupDiGetDeviceInterfaceAlias
@ stdcall SetupDiGetDeviceInterfaceDetailA(long ptr ptr long ptr ptr) SetupDiGetDeviceInterfaceDetailA
@ stdcall SetupDiGetDeviceInterfaceDetailW(long ptr ptr long ptr ptr) SetupDiGetDeviceInterfaceDetailW
@ stdcall SetupDiGetDeviceRegistryPropertyA(long ptr long ptr ptr long ptr) SetupDiGetDeviceRegistryPropertyA
@ stdcall SetupDiGetDeviceRegistryPropertyW(long ptr long ptr ptr long ptr)
@ stub SetupDiGetDriverInfoDetailA
@ stub SetupDiGetDriverInfoDetailW
@ stub SetupDiGetDriverInstallParamsA
@ stub SetupDiGetDriverInstallParamsW
@ stub SetupDiGetHwProfileFriendlyNameA
@ stub SetupDiGetHwProfileFriendlyNameExA
@ stub SetupDiGetHwProfileFriendlyNameExW
@ stub SetupDiGetHwProfileFriendlyNameW
@ stub SetupDiGetHwProfileList
@ stub SetupDiGetHwProfileListExA
@ stub SetupDiGetHwProfileListExW
@ stub SetupDiGetINFClassA
@ stub SetupDiGetINFClassW
@ stub SetupDiGetSelectedDevice
@ stub SetupDiGetSelectedDriverA
@ stub SetupDiGetSelectedDriverW
@ stub SetupDiGetWizardPage
@ stdcall SetupDiInstallClassA(long str long ptr) SetupDiInstallClassA
@ stub SetupDiInstallClassExA
@ stub SetupDiInstallClassExW
@ stdcall SetupDiInstallClassW(long wstr long ptr) SetupDiInstallClassW
@ stub SetupDiInstallDevice
@ stub SetupDiInstallDriverFiles
@ stub SetupDiLoadClassIcon
@ stub SetupDiMoveDuplicateDevice
@ stdcall SetupDiOpenClassRegKey(ptr long) SetupDiOpenClassRegKey
@ stdcall SetupDiOpenClassRegKeyExA(ptr long long str ptr) SetupDiOpenClassRegKeyExA
@ stdcall SetupDiOpenClassRegKeyExW(ptr long long wstr ptr) SetupDiOpenClassRegKeyExW
@ stdcall SetupDiOpenDevRegKey(ptr ptr long long long long) SetupDiOpenDevRegKey
@ stub SetupDiOpenDeviceInfoA
@ stub SetupDiOpenDeviceInfoW
@ stdcall SetupDiOpenDeviceInterfaceA(ptr str long ptr) SetupDiOpenDeviceInterfaceA
@ stub SetupDiOpenDeviceInterfaceRegKey
@ stdcall SetupDiOpenDeviceInterfaceW(ptr wstr long ptr) SetupDiOpenDeviceInterfaceW
@ stub SetupDiRegisterDeviceInfo
@ stub SetupDiRemoveDevice
@ stub SetupDiRemoveDeviceInterface
@ stub SetupDiSelectDevice
@ stub SetupDiSelectOEMDrv
@ stdcall SetupDiSetClassInstallParamsA(ptr ptr ptr long) SetupDiSetClassInstallParamsA
@ stub SetupDiSetClassInstallParamsW
@ stub SetupDiSetDeviceInstallParamsA
@ stub SetupDiSetDeviceInstallParamsW
@ stub SetupDiSetDeviceRegistryPropertyA
@ stub SetupDiSetDeviceRegistryPropertyW
@ stub SetupDiSetDriverInstallParamsA
@ stub SetupDiSetDriverInstallParamsW
@ stub SetupDiSetSelectedDevice
@ stub SetupDiSetSelectedDriverA
@ stub SetupDiSetSelectedDriverW
@ stub SetupDiUnremoveDevice
@ stub SetupDuplicateDiskSpaceListA
@ stub SetupDuplicateDiskSpaceListW
@ stdcall SetupFindFirstLineA(long str str ptr) SetupFindFirstLineA
@ stdcall SetupFindFirstLineW(long wstr wstr ptr) SetupFindFirstLineW
@ stdcall SetupFindNextLine(ptr ptr) SetupFindNextLine
@ stdcall SetupFindNextMatchLineA(ptr str ptr) SetupFindNextMatchLineA
@ stdcall SetupFindNextMatchLineW(ptr wstr ptr) SetupFindNextMatchLineW
@ stub SetupFreeSourceListA
@ stub SetupFreeSourceListW
@ stub SetupGetBackupInformationA
@ stub SetupGetBackupInformationW
@ stdcall SetupGetBinaryField(ptr long ptr long ptr) SetupGetBinaryField
@ stdcall SetupGetFieldCount(ptr) SetupGetFieldCount
@ stub SetupGetFileCompressionInfoA
@ stub SetupGetFileCompressionInfoW
@ stdcall SetupGetFileQueueCount(long long ptr) SetupGetFileQueueCount
@ stdcall SetupGetFileQueueFlags(long ptr) SetupGetFileQueueFlags
@ stub SetupGetInfFileListA
@ stub SetupGetInfFileListW 
@ stdcall SetupGetInfInformationA(ptr long ptr long ptr) SetupGetInfInformationA
@ stub SetupGetInfInformationW
@ stub SetupGetInfSections
@ stdcall SetupGetIntField(ptr long ptr) SetupGetIntField
@ stdcall SetupGetLineByIndexA(long str long ptr) SetupGetLineByIndexA
@ stdcall SetupGetLineByIndexW(long wstr long ptr) SetupGetLineByIndexW
@ stdcall SetupGetLineCountA(long str) SetupGetLineCountA
@ stdcall SetupGetLineCountW(long wstr) SetupGetLineCountW
@ stdcall SetupGetLineTextA(ptr long str str ptr long ptr) SetupGetLineTextA
@ stdcall SetupGetLineTextW(ptr long wstr wstr ptr long ptr) SetupGetLineTextW
@ stdcall SetupGetMultiSzFieldA(ptr long ptr long ptr) SetupGetMultiSzFieldA
@ stdcall SetupGetMultiSzFieldW(ptr long ptr long ptr) SetupGetMultiSzFieldW
@ stub SetupGetSourceFileLocationA
@ stub SetupGetSourceFileLocationW
@ stub SetupGetSourceFileSizeA
@ stub SetupGetSourceFileSizeW
@ stub SetupGetSourceInfoA
@ stub SetupGetSourceInfoW
@ stdcall SetupGetStringFieldA(ptr long ptr long ptr) SetupGetStringFieldA
@ stdcall SetupGetStringFieldW(ptr long ptr long ptr) SetupGetStringFieldW
@ stub SetupGetTargetPathA
@ stub SetupGetTargetPathW
@ stdcall SetupInitDefaultQueueCallback(long) SetupInitDefaultQueueCallback
@ stdcall SetupInitDefaultQueueCallbackEx(long long long long ptr) SetupInitDefaultQueueCallbackEx
@ stdcall SetupInitializeFileLogA (str long) SetupInitializeFileLogA
@ stdcall SetupInitializeFileLogW (wstr long) SetupInitializeFileLogW
@ stub SetupInstallFileA
@ stub SetupInstallFileExA
@ stub SetupInstallFileExW
@ stub SetupInstallFileW
@ stdcall SetupInstallFilesFromInfSectionA(long long long str str long) SetupInstallFilesFromInfSectionA
@ stdcall SetupInstallFilesFromInfSectionW(long long long wstr wstr long) SetupInstallFilesFromInfSectionW
@ stdcall SetupInstallFromInfSectionA(long long str long long str long ptr ptr long ptr) SetupInstallFromInfSectionA
@ stdcall SetupInstallFromInfSectionW(long long wstr long long wstr long ptr ptr long ptr) SetupInstallFromInfSectionW
@ stub SetupInstallServicesFromInfSectionA
@ stub SetupInstallServicesFromInfSectionExA
@ stub SetupInstallServicesFromInfSectionExW
@ stub SetupInstallServicesFromInfSectionW
@ stdcall SetupIterateCabinetA(str long ptr ptr) SetupIterateCabinetA
@ stdcall SetupIterateCabinetW(wstr long ptr ptr) SetupIterateCabinetW
@ stub SetupLogErrorA
@ stub SetupLogErrorW
@ stub SetupLogFileA
@ stub SetupLogFileW
@ stdcall SetupOpenAppendInfFileA(str long ptr) SetupOpenAppendInfFileA
@ stdcall SetupOpenAppendInfFileW(wstr long ptr) SetupOpenAppendInfFileW
@ stdcall SetupOpenFileQueue() SetupOpenFileQueue
@ stdcall SetupOpenInfFileA(str str long ptr) SetupOpenInfFileA
@ stdcall SetupOpenInfFileW(wstr wstr long ptr) SetupOpenInfFileW
@ stdcall SetupOpenMasterInf() SetupOpenMasterInf
@ stub SetupPromptForDiskA
@ stub SetupPromptForDiskW
@ stub SetupPromptReboot
@ stub SetupQueryDrivesInDiskSpaceListA
@ stub SetupQueryDrivesInDiskSpaceListW
@ stub SetupQueryFileLogA
@ stub SetupQueryFileLogW
@ stub SetupQueryInfFileInformationA
@ stub SetupQueryInfFileInformationW
@ stub SetupQueryInfOriginalFileInformationA
@ stub SetupQueryInfOriginalFileInformationW
@ stub SetupQueryInfVersionInformationA
@ stub SetupQueryInfVersionInformationW
@ stub SetupQuerySourceListA
@ stub SetupQuerySourceListW
@ stdcall SetupQuerySpaceRequiredOnDriveA(long str ptr ptr long) SetupQuerySpaceRequiredOnDriveA
@ stub SetupQuerySpaceRequiredOnDriveW
@ stdcall SetupQueueCopyA(long str str str str str str str long) SetupQueueCopyA
@ stdcall SetupQueueCopyIndirectA(ptr) SetupQueueCopyIndirectA
@ stdcall SetupQueueCopyIndirectW(ptr) SetupQueueCopyIndirectW
@ stdcall SetupQueueCopySectionA(long str long long str long) SetupQueueCopySectionA
@ stdcall SetupQueueCopySectionW(long wstr long long wstr long) SetupQueueCopySectionW
@ stdcall SetupQueueCopyW(long wstr wstr wstr wstr wstr wstr wstr long) SetupQueueCopyW
@ stdcall SetupQueueDefaultCopyA(long long str str str long) SetupQueueDefaultCopyA
@ stdcall SetupQueueDefaultCopyW(long long wstr wstr wstr long) SetupQueueDefaultCopyW
@ stdcall SetupQueueDeleteA(long str str) SetupQueueDeleteA
@ stdcall SetupQueueDeleteSectionA(long long long str) SetupQueueDeleteSectionA
@ stdcall SetupQueueDeleteSectionW(long long long wstr) SetupQueueDeleteSectionW
@ stdcall SetupQueueDeleteW(long wstr wstr) SetupQueueDeleteW
@ stdcall SetupQueueRenameA(long str str str str) SetupQueueRenameA
@ stdcall SetupQueueRenameSectionA(long long long str) SetupQueueRenameSectionA
@ stdcall SetupQueueRenameSectionW(long long long wstr) SetupQueueRenameSectionW
@ stdcall SetupQueueRenameW(long wstr wstr wstr wstr) SetupQueueRenameW
@ stub SetupRemoveFileLogEntryA 
@ stub SetupRemoveFileLogEntryW
@ stub SetupRemoveFromDiskSpaceListA
@ stub SetupRemoveFromDiskSpaceListW
@ stub SetupRemoveFromSourceListA
@ stub SetupRemoveFromSourceListW
@ stub SetupRemoveInstallSectionFromDiskSpaceListA
@ stub SetupRemoveInstallSectionFromDiskSpaceListW
@ stub SetupRemoveSectionFromDiskSpaceListA
@ stub SetupRemoveSectionFromDiskSpaceListW
@ stdcall SetupRenameErrorA(long str str str long long) SetupRenameErrorA
@ stdcall SetupRenameErrorW(long wstr wstr wstr long long) SetupRenameErrorW
@ stub SetupScanFileQueue
@ stdcall SetupScanFileQueueA(long long long ptr ptr ptr) SetupScanFileQueueA
@ stdcall SetupScanFileQueueW(long long long ptr ptr ptr) SetupScanFileQueueW
@ stdcall SetupSetDirectoryIdA(long long str) SetupSetDirectoryIdA
@ stub SetupSetDirectoryIdExA
@ stub SetupSetDirectoryIdExW
@ stdcall SetupSetDirectoryIdW(long long wstr) SetupSetDirectoryIdW
@ stdcall SetupSetFileQueueAlternatePlatformA(ptr ptr str) SetupSetFileQueueAlternatePlatformA
@ stdcall SetupSetFileQueueAlternatePlatformW(ptr ptr wstr) SetupSetFileQueueAlternatePlatformW
@ stdcall SetupSetFileQueueFlags(long long long) SetupSetFileQueueFlags
@ stub SetupSetPlatformPathOverrideA
@ stub SetupSetPlatformPathOverrideW
@ stub SetupSetSourceListA
@ stub SetupSetSourceListW
@ stdcall SetupTermDefaultQueueCallback(ptr) SetupTermDefaultQueueCallback
@ stdcall SetupTerminateFileLog(long) SetupTerminateFileLog
@ stub ShouldDeviceBeExcluded
@ stdcall StampFileSecurity(wstr ptr) StampFileSecurity
@ stub StringTableAddString
@ stub StringTableAddStringEx
@ stub StringTableDestroy
@ stub StringTableDuplicate
@ stub StringTableEnum
@ stub StringTableGetExtraData
@ stub StringTableInitialize
@ stub StringTableInitializeEx
@ stub StringTableLookUpString
@ stub StringTableLookUpStringEx
@ stub StringTableSetExtraData
@ stub StringTableStringFromId
@ stub StringTableTrim
@ stdcall TakeOwnershipOfFile(wstr) TakeOwnershipOfFile
@ stdcall UnicodeToMultiByte(wstr long) UnicodeToMultiByte
@ stdcall UnmapAndCloseFile(long long ptr) UnmapAndCloseFile
@ stub VerifyCatalogFile
@ stub VerifyFile
@ stub pSetupAccessRunOnceNodeList
@ stub pSetupAddMiniIconToList
@ stub pSetupAddTagToGroupOrderListEntry
@ stub pSetupAppendStringToMultiSz
@ stub pSetupDestroyRunOnceNodeList
@ stub pSetupDirectoryIdToPath
@ stub pSetupGetField
@ stdcall pSetupGetGlobalFlags() pSetupGetGlobalFlags
@ stub pSetupGetOsLoaderDriveAndPath 
@ stub pSetupGetQueueFlags
@ stub pSetupGetVersionDatum
@ stub pSetupGuidFromString
@ stub pSetupIsGuidNull
@ stub pSetupMakeSurePathExists
@ stdcall pSetupSetGlobalFlags(long) pSetupSetGlobalFlags
@ stub pSetupSetQueueFlags
@ stub pSetupSetSystemSourceFlags
@ stub pSetupStringFromGuid
@ stub pSetupVerifyQueuedCatalogs
