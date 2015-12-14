# advpack
name advpack
type win32
init DllMain

import ntdll.dll
import kernel32.dll
import user32.dll
import version.dll
import setupapi.dll


@ stub AddDelBackupEntry
@ stub AdvInstallFile
@ stub CloseINFEngine
@ stdcall DelNode(str long) DelNode
@ stdcall DelNodeRunDLL32(ptr ptr str long) DelNodeRunDLL32
#@ stdcall -private DllMain(long long ptr) DllMain
@ stdcall DoInfInstall(ptr) DoInfInstall 
@ stdcall ExecuteCab(ptr ptr ptr) ExecuteCab
@ stub ExtractFiles
@ stub FileSaveMarkNotExist
@ stub FileSaveRestore
@ stub FileSaveRestoreOnINF
@ stdcall GetVersionFromFile(str ptr ptr long) GetVersionFromFile
@ stdcall GetVersionFromFileEx(str ptr ptr long) GetVersionFromFileEx
@ stdcall IsNTAdmin(long ptr) IsNTAdmin
@ stdcall LaunchINFSection(ptr ptr str long) LaunchINFSection
@ stdcall LaunchINFSectionEx(ptr ptr str long) LaunchINFSectionEx
@ stdcall NeedReboot(long) NeedReboot
@ stdcall NeedRebootInit() NeedRebootInit
@ stub OpenINFEngine
@ stub RebootCheckOnInstall
@ stdcall RegInstall(ptr str ptr) RegInstall
@ stub RegRestoreAll
@ stub RegSaveRestore
@ stub RegSaveRestoreOnINF
@ stdcall RegisterOCX(ptr ptr str long) RegisterOCX
@ stdcall RunSetupCommand(long str str str str ptr long ptr) RunSetupCommand
@ stub SetPerUserSecValues
@ stdcall TranslateInfString(str str str str ptr long ptr ptr) TranslateInfString
@ stub TranslateInfStringEx
@ stub UserInstStubWrapper
@ stub UserUnInstStubWrapper
