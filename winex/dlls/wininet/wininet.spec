name	wininet
type	win32
init	DllMain
rsrc	wininet.res

import mpr.dll
import shlwapi.dll
import shell32.dll
import user32.dll
import advapi32.dll
import kernel32.dll
import ntdll.dll
import -delay secur32.dll
import -delay crypt32.dll
import -delay cryptui.dll

101 stub DoConnectoidsExist
102 stdcall GetDiskInfoA(str ptr ptr ptr)
103 stub PerformOperationOverUrlCacheA
104 stub HttpCheckDavComplianceA
105 stub HttpCheckDavComplianceW
108 stub ImportCookieFileA
109 stub ExportCookieFileA
110 stub ImportCookieFileW
111 stub ExportCookieFileW
112 stub IsProfilesEnabled
116 stub IsDomainlegalCookieDomainA
117 stdcall IsDomainLegalCookieDomainW(wstr wstr)
118 stub FindP3PPolicySymbol
120 stub MapResourceToPolicy
121 stub GetP3PPolicy
122 stub FreeP3PObject
123 stub GetP3PRequestStatus

@ stdcall CommitUrlCacheEntryA(str str longlong longlong long str long str str)
@ stdcall CommitUrlCacheEntryW(wstr wstr longlong longlong long wstr long wstr wstr)
@ stdcall CreateMD5SSOHash(wstr wstr wstr ptr)
@ stdcall CreateUrlCacheContainerA(long long long long long long long long)
@ stdcall CreateUrlCacheContainerW(long long long long long long long long)
@ stdcall CreateUrlCacheEntryA(str long str ptr long)
@ stdcall CreateUrlCacheEntryW(wstr long wstr ptr long)
@ stdcall CreateUrlCacheGroup(long ptr)
@ stdcall DeleteIE3Cache(ptr ptr str long)
@ stdcall DeleteUrlCacheContainerA(long long)
@ stdcall DeleteUrlCacheContainerW(long long)
@ stdcall DeleteUrlCacheEntry(str) DeleteUrlCacheEntryA
@ stdcall DeleteUrlCacheEntryA(str)
@ stdcall DeleteUrlCacheEntryW(wstr)
@ stdcall DeleteUrlCacheGroup(longlong long ptr)
@ stdcall DeleteWpadCacheForNetworks(long)
@ stdcall DetectAutoProxyUrl(str long long)
@ stdcall DllInstall(long wstr)
@ stdcall FindCloseUrlCache(long)
@ stdcall FindFirstUrlCacheContainerA(ptr ptr ptr long)
@ stdcall FindFirstUrlCacheContainerW(ptr ptr ptr long)
@ stdcall FindFirstUrlCacheEntryA(str ptr ptr)
@ stdcall FindFirstUrlCacheEntryExA(str long long longlong ptr ptr ptr ptr ptr)
@ stdcall FindFirstUrlCacheEntryExW(wstr long long longlong ptr ptr ptr ptr ptr)
@ stdcall FindFirstUrlCacheEntryW(wstr ptr ptr)
@ stdcall FindFirstUrlCacheGroup(long long ptr long ptr ptr)
@ stdcall FindNextUrlCacheContainerA(long ptr ptr)
@ stdcall FindNextUrlCacheContainerW(long ptr ptr)
@ stdcall FindNextUrlCacheEntryA(long ptr ptr)
@ stdcall FindNextUrlCacheEntryExA(long ptr ptr ptr ptr ptr)
@ stdcall FindNextUrlCacheEntryExW(long ptr ptr ptr ptr ptr)
@ stdcall FindNextUrlCacheEntryW(long ptr ptr)
@ stdcall FindNextUrlCacheGroup(long ptr ptr)
@ stub ForceNexusLookup
@ stub ForceNexusLookupExW
@ stdcall FreeUrlCacheSpaceA(str long long)
@ stdcall FreeUrlCacheSpaceW(wstr long long)
@ stdcall FtpCommandA(long long long str ptr ptr)
@ stdcall FtpCommandW(long long long wstr ptr ptr)
@ stdcall FtpCreateDirectoryA(ptr str)
@ stdcall FtpCreateDirectoryW(ptr wstr)
@ stdcall FtpDeleteFileA(ptr str)
@ stdcall FtpDeleteFileW(ptr wstr)
@ stdcall FtpFindFirstFileA(ptr str ptr long long)
@ stdcall FtpFindFirstFileW(ptr wstr ptr long long)
@ stdcall FtpGetCurrentDirectoryA(ptr str ptr)
@ stdcall FtpGetCurrentDirectoryW(ptr wstr ptr)
@ stdcall FtpGetFileA(ptr str str long long long long)
@ stub FtpGetFileEx
@ stdcall FtpGetFileSize(long ptr)
@ stdcall FtpGetFileW(ptr wstr wstr long long long long)
@ stdcall FtpOpenFileA(ptr str long long long)
@ stdcall FtpOpenFileW(ptr wstr long long long)
@ stdcall FtpPutFileA(ptr str str long long)
@ stub FtpPutFileEx
@ stdcall FtpPutFileW(ptr wstr wstr long long)
@ stdcall FtpRemoveDirectoryA(ptr str)
@ stdcall FtpRemoveDirectoryW(ptr wstr)
@ stdcall FtpRenameFileA(ptr str str)
@ stdcall FtpRenameFileW(ptr wstr wstr)
@ stdcall FtpSetCurrentDirectoryA(ptr str)
@ stdcall FtpSetCurrentDirectoryW(ptr wstr)
@ stdcall GetUrlCacheConfigInfoA(ptr ptr long)
@ stdcall GetUrlCacheConfigInfoW(ptr ptr long)
@ stdcall GetUrlCacheEntryInfoA(str ptr long)
@ stdcall GetUrlCacheEntryInfoExA(str ptr ptr str ptr ptr long)
@ stdcall GetUrlCacheEntryInfoExW(wstr ptr ptr wstr ptr ptr long)
@ stdcall GetUrlCacheEntryInfoW(wstr ptr long)
@ stdcall GetUrlCacheGroupAttributeA(longlong long long ptr ptr ptr)
@ stdcall GetUrlCacheGroupAttributeW(longlong long long ptr ptr ptr)
@ stub GetUrlCacheHeaderData
@ stdcall GopherCreateLocatorA(str long str str long str ptr)
@ stdcall GopherCreateLocatorW(wstr long wstr wstr long wstr ptr)
@ stdcall GopherFindFirstFileA(ptr str str ptr long long)
@ stdcall GopherFindFirstFileW(ptr wstr wstr ptr long long)
@ stdcall GopherGetAttributeA(ptr str str ptr long ptr ptr long)
@ stdcall GopherGetAttributeW(ptr wstr wstr ptr long ptr ptr long)
@ stdcall GopherGetLocatorTypeA(str ptr)
@ stdcall GopherGetLocatorTypeW(wstr ptr)
@ stdcall GopherOpenFileA(ptr str str long long)
@ stdcall GopherOpenFileW(ptr wstr wstr long long)
@ stdcall HttpAddRequestHeadersA(ptr str long long)
@ stdcall HttpAddRequestHeadersW(ptr wstr long long)
@ stub HttpCheckDavCompliance
@ stdcall HttpEndRequestA(ptr ptr long long)
@ stdcall HttpEndRequestW(ptr ptr long long)
@ stdcall HttpOpenRequestA(ptr str str str str ptr long long)
@ stdcall HttpOpenRequestW(ptr wstr wstr wstr wstr ptr long long)
@ stdcall HttpQueryInfoA(ptr long ptr ptr ptr)
@ stdcall HttpQueryInfoW(ptr long ptr ptr ptr)
@ stdcall HttpSendRequestA(ptr str long ptr long)
@ stdcall HttpSendRequestExA(long ptr ptr long long)
@ stdcall HttpSendRequestExW(long ptr ptr long long)
@ stdcall HttpSendRequestW(ptr wstr long ptr long)
@ stdcall IncrementUrlCacheHeaderData(long ptr)
@ stub InternetAlgIdToStringA
@ stub InternetAlgIdToStringW
@ stdcall InternetAttemptConnect(long)
@ stdcall InternetAutodial(long ptr)
@ stub InternetAutodialCallback
@ stdcall InternetAutodialHangup(long)
@ stdcall InternetCanonicalizeUrlA(str ptr ptr long)
@ stdcall InternetCanonicalizeUrlW(wstr ptr ptr long)
@ stdcall InternetCheckConnectionA(ptr long long)
@ stdcall InternetCheckConnectionW(ptr long long)
@ stdcall InternetClearAllPerSiteCookieDecisions()
@ stdcall InternetCloseHandle(long)
@ stdcall InternetCombineUrlA(str str str ptr long)
@ stdcall InternetCombineUrlW(wstr wstr wstr ptr long)
@ stdcall InternetConfirmZoneCrossing(long str str long) InternetConfirmZoneCrossingA
@ stdcall InternetConfirmZoneCrossingA(long str str long)
@ stdcall InternetConfirmZoneCrossingW(long wstr wstr long)
@ stdcall InternetConnectA(ptr str long str str long long long)
@ stdcall InternetConnectW(ptr wstr long wstr wstr long long long)
@ stdcall InternetCrackUrlA(str long long ptr)
@ stdcall InternetCrackUrlW(wstr long long ptr)
@ stdcall InternetCreateUrlA(ptr long ptr ptr)
@ stdcall InternetCreateUrlW(ptr long ptr ptr)
@ stub InternetDebugGetLocalTime
@ stdcall InternetDial(long str long ptr long) InternetDialA
@ stdcall InternetDialA(long str long ptr long)
@ stdcall InternetDialW(long wstr long ptr long)
@ stdcall InternetEnumPerSiteCookieDecisionA(ptr ptr ptr long)
@ stdcall InternetEnumPerSiteCookieDecisionW(ptr ptr ptr long)
@ stdcall InternetErrorDlg(long long long long ptr)
@ stdcall InternetFindNextFileA(ptr ptr)
@ stdcall InternetFindNextFileW(ptr ptr)
@ stub InternetFortezzaCommand
@ stub InternetGetCertByURL
@ stub InternetGetCertByURLA
@ stdcall InternetGetConnectedState(ptr long)
@ stdcall InternetGetConnectedStateEx(ptr ptr long long) InternetGetConnectedStateExA
@ stdcall InternetGetConnectedStateExA(ptr ptr long long)
@ stdcall InternetGetConnectedStateExW(ptr ptr long long)
@ stdcall InternetGetCookieA(str str ptr long)
@ stdcall InternetGetCookieExA(str str ptr ptr long ptr)
@ stdcall InternetGetCookieExW(wstr wstr ptr ptr long ptr)
@ stdcall InternetGetCookieW(wstr wstr ptr long)
@ stdcall InternetGetLastResponseInfoA(ptr ptr ptr)
@ stdcall InternetGetLastResponseInfoW(ptr ptr ptr)
@ stdcall InternetGetPerSiteCookieDecisionA(str ptr)
@ stdcall InternetGetPerSiteCookieDecisionW(wstr ptr)
@ stdcall InternetGetSecurityInfoByURL(str ptr ptr) InternetGetSecurityInfoByURLA
@ stdcall InternetGetSecurityInfoByURLA(str ptr ptr)
@ stdcall InternetGetSecurityInfoByURLW(wstr ptr ptr)
@ stdcall InternetGoOnline(str long long) InternetGoOnlineA
@ stdcall InternetGoOnlineA(str long long)
@ stdcall InternetGoOnlineW(wstr long long)
@ stdcall InternetHangUp(long long)
@ stdcall InternetInitializeAutoProxyDll(long)
@ stdcall InternetLockRequestFile(ptr ptr)
@ stdcall InternetOpenA(str long str str long)
@ stub InternetOpenServerPushParse
@ stdcall InternetOpenUrlA(ptr str str long long long)
@ stdcall InternetOpenUrlW(ptr wstr wstr long long long)
@ stdcall InternetOpenW(wstr long wstr wstr long)
@ stdcall InternetQueryDataAvailable(ptr ptr long long)
@ stdcall InternetQueryFortezzaStatus(ptr long)
@ stdcall InternetQueryOptionA(ptr long ptr ptr)
@ stdcall InternetQueryOptionW(ptr long ptr ptr)
@ stdcall InternetReadFile(ptr ptr long ptr)
@ stdcall InternetReadFileExA(ptr ptr long long)
@ stdcall InternetReadFileExW(ptr ptr long long)
@ stub InternetSecurityProtocolToStringA
@ stub InternetSecurityProtocolToStringW
@ stub InternetServerPushParse
@ stdcall InternetSetCookieA(str str str)
@ stdcall InternetSetCookieExA(str str str long ptr)
@ stdcall InternetSetCookieExW(wstr wstr wstr long ptr)
@ stdcall InternetSetCookieW(wstr wstr wstr)
@ stub InternetSetDialState
@ stub InternetSetDialStateA
@ stub InternetSetDialStateW
@ stdcall InternetSetFilePointer(ptr long ptr long long)
@ stdcall InternetSetOptionA(ptr long ptr long)
@ stdcall InternetSetOptionExA(ptr long ptr long long)
@ stdcall InternetSetOptionExW(ptr long ptr long long)
@ stdcall InternetSetOptionW(ptr long ptr long)
@ stdcall InternetSetPerSiteCookieDecisionA(str long)
@ stdcall InternetSetPerSiteCookieDecisionW(wstr long)
@ stdcall InternetSetStatusCallback(ptr ptr) InternetSetStatusCallbackA
@ stdcall InternetSetStatusCallbackA(ptr ptr)
@ stdcall InternetSetStatusCallbackW(ptr ptr)
@ stdcall InternetShowSecurityInfoByURL(str ptr) InternetShowSecurityInfoByURLA
@ stdcall InternetShowSecurityInfoByURLA(str ptr)
@ stdcall InternetShowSecurityInfoByURLW(wstr ptr)
@ stdcall InternetTimeFromSystemTime(ptr long ptr long) InternetTimeFromSystemTimeA
@ stdcall InternetTimeFromSystemTimeA(ptr long ptr long)
@ stdcall InternetTimeFromSystemTimeW(ptr long ptr long)
@ stdcall InternetTimeToSystemTime(str ptr long) InternetTimeToSystemTimeA
@ stdcall InternetTimeToSystemTimeA(str ptr long)
@ stdcall InternetTimeToSystemTimeW(wstr ptr long)
@ stdcall InternetUnlockRequestFile(ptr)
@ stdcall InternetWriteFile(ptr ptr long ptr)
@ stub InternetWriteFileExA
@ stub InternetWriteFileExW
@ stdcall IsHostInProxyBypassList(long str long)
@ stdcall IsUrlCacheEntryExpiredA(str long ptr)
@ stdcall IsUrlCacheEntryExpiredW(wstr long ptr)
@ stdcall LoadUrlCacheContent()
@ stub ParseX509EncodedCertificateForListBoxEntry
@ stdcall PrivacyGetZonePreferenceW(long long ptr ptr ptr)
@ stdcall PrivacySetZonePreferenceW(long long long wstr)
@ stdcall ReadUrlCacheEntryStream(ptr long ptr ptr long)
@ stdcall RegisterUrlCacheNotification(ptr long long long long long)
@ stdcall ResumeSuspendedDownload(long long)
@ stdcall RetrieveUrlCacheEntryFileA(str ptr ptr long)
@ stdcall RetrieveUrlCacheEntryFileW(wstr ptr ptr long)
@ stdcall RetrieveUrlCacheEntryStreamA(str ptr ptr long long)
@ stdcall RetrieveUrlCacheEntryStreamW(wstr ptr ptr long long)
@ stdcall RunOnceUrlCache(ptr ptr str long)
@ stdcall SetUrlCacheConfigInfoA(ptr long)
@ stdcall SetUrlCacheConfigInfoW(ptr long)
@ stdcall SetUrlCacheEntryGroup(str long longlong ptr long ptr) SetUrlCacheEntryGroupA
@ stdcall SetUrlCacheEntryGroupA(str long longlong ptr long ptr)
@ stdcall SetUrlCacheEntryGroupW(wstr long longlong ptr long ptr)
@ stdcall SetUrlCacheEntryInfoA(str ptr long)
@ stdcall SetUrlCacheEntryInfoW(wstr ptr long)
@ stdcall SetUrlCacheGroupAttributeA(longlong long long ptr ptr)
@ stdcall SetUrlCacheGroupAttributeW(longlong long long ptr ptr)
@ stub SetUrlCacheHeaderData
@ stub ShowCertificate
@ stdcall ShowClientAuthCerts(ptr)
@ stub ShowSecurityInfo
@ stdcall ShowX509EncodedCertificate(ptr ptr long)
@ stdcall UnlockUrlCacheEntryFile(str long) UnlockUrlCacheEntryFileA
@ stdcall UnlockUrlCacheEntryFileA(str long)
@ stdcall UnlockUrlCacheEntryFileW(wstr long)
@ stdcall UnlockUrlCacheEntryStream(ptr long)
@ stub UpdateUrlCacheContentPath
@ stub UrlZonesDetach
