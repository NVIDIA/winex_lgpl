name	odbccp32
type	win32

import advapi32.dll
import kernel32.dll
import ntdll.dll

@ stdcall ODBCCPlApplet( long long ptr ptr) ODBCCPlApplet
@ stdcall SQLConfigDataSource(ptr long wstr wstr) SQLConfigDataSource
@ stdcall SQLConfigDataSourceW(ptr long str str) SQLConfigDataSourceW
@ stdcall SQLConfigDriver(ptr long str str ptr long ptr) SQLConfigDriver
@ stdcall SQLConfigDriverW(ptr long wstr wstr ptr long ptr) SQLConfigDriverW
@ stdcall SQLCreateDataSource(ptr str) SQLCreateDataSource
@ stub SQLCreateDataSourceEx
@ stub SQLCreateDataSourceExW
@ stdcall SQLCreateDataSourceW(ptr wstr) SQLCreateDataSourceW
@ stdcall SQLGetAvailableDrivers(str ptr long ptr) SQLGetAvailableDrivers
@ stdcall SQLGetAvailableDriversW(wstr ptr long ptr) SQLGetAvailableDriversW
@ stdcall SQLGetConfigMode(ptr) SQLGetConfigMode
@ stdcall SQLGetInstalledDrivers(str long ptr) SQLGetInstalledDrivers
@ stdcall SQLGetInstalledDriversW(wstr long ptr) SQLGetInstalledDriversW
@ stdcall SQLGetPrivateProfileString(str str str str long str) SQLGetPrivateProfileString
@ stdcall SQLGetPrivateProfileStringW(wstr wstr wstr wstr long wstr) SQLGetPrivateProfileStringW
@ stdcall SQLGetTranslator(ptr str long ptr ptr long ptr ptr) SQLGetTranslator
@ stdcall SQLGetTranslatorW(ptr wstr long ptr ptr long ptr ptr) SQLGetTranslatorW
@ stdcall SQLInstallDriver(str str str long ptr) SQLInstallDriver
@ stdcall SQLInstallDriverEx(str str str long ptr long ptr) SQLInstallDriverEx
@ stdcall SQLInstallDriverExW(wstr wstr wstr long ptr long ptr) SQLInstallDriverExW
@ stdcall SQLInstallDriverManager(ptr long ptr) SQLInstallDriverManager
@ stdcall SQLInstallDriverManagerW(ptr long ptr) SQLInstallDriverManagerW
@ stdcall SQLInstallDriverW(wstr wstr wstr long ptr) SQLInstallDriverW
@ stdcall SQLInstallODBC(ptr str str str) SQLInstallODBC
@ stdcall SQLInstallODBCW(ptr wstr wstr wstr) SQLInstallODBCW
@ stdcall SQLInstallTranslator(str str str ptr long ptr long ptr) SQLInstallTranslator
@ stdcall SQLInstallTranslatorEx(str str ptr long ptr long ptr) SQLInstallTranslatorEx
@ stdcall SQLInstallTranslatorExW(wstr wstr ptr long ptr long ptr) SQLInstallTranslatorExW
@ stdcall SQLInstallTranslatorW(wstr wstr wstr ptr long ptr long ptr) SQLInstallTranslatorW
@ stdcall SQLInstallerError(long ptr ptr long ptr) SQLInstallerError
@ stdcall SQLInstallerErrorW(long ptr ptr long ptr) SQLInstallerErrorW
@ stub SQLLoadDataSourcesListBox
@ stub SQLLoadDriverListBox
@ stdcall SQLManageDataSources(ptr) SQLManageDataSources
@ stdcall SQLPostInstallerError(long ptr) SQLPostInstallerError
@ stdcall SQLPostInstallerErrorW(long ptr) SQLPostInstallerErrorW
@ stdcall SQLReadFileDSN(str str str ptr long ptr) SQLReadFileDSN
@ stdcall SQLReadFileDSNW(wstr wstr wstr ptr long ptr) SQLReadFileDSNW
@ stdcall SQLRemoveDSNFromIni(str) SQLRemoveDSNFromIni
@ stdcall SQLRemoveDSNFromIniW(wstr) SQLRemoveDSNFromIniW
@ stdcall SQLRemoveDefaultDataSource() SQLRemoveDefaultDataSource
@ stdcall SQLRemoveDriver(str long ptr) SQLRemoveDriver
@ stdcall SQLRemoveDriverManager(ptr) SQLRemoveDriverManager
@ stdcall SQLRemoveDriverW(wstr long ptr) SQLRemoveDriverW
@ stdcall SQLRemoveTranslator(str ptr) SQLRemoveTranslator
@ stdcall SQLRemoveTranslatorW(wstr ptr) SQLRemoveTranslatorW
@ stdcall SQLSetConfigMode(long) SQLSetConfigMode
@ stdcall SQLValidDSN(str) SQLValidDSN
@ stdcall SQLValidDSNW(wstr) SQLValidDSNW
@ stdcall SQLWriteDSNToIni(str str) SQLWriteDSNToIni
@ stdcall SQLWriteDSNToIniW(wstr wstr) SQLWriteDSNToIniW
@ stdcall SQLWriteFileDSN(str str str str) SQLWriteFileDSN
@ stdcall SQLWriteFileDSNW(wstr wstr wstr wstr) SQLWriteFileDSNW
@ stdcall SQLWritePrivateProfileString(str str str str) SQLWritePrivateProfileString
@ stdcall SQLWritePrivateProfileStringW(wstr wstr wstr wstr) SQLWritePrivateProfileStringW
@ stub SelectTransDlg
