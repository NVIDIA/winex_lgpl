name	shdocvw
type	win32
init	DllMain
rsrc	shdocvw.res

import shell32.dll
import shlwapi.dll
import user32.dll
import advapi32.dll
import kernel32.dll
import -delay version.dll
import -delay urlmon.dll
import -delay ole32.dll
import -delay oleaut32.dll

# ordinal exports
101 stdcall IEWinMain(str long)
102 stub CreateShortcutInDirA
103 stub CreateShortcutInDirW
104 stdcall WhichPlatformFORWARD()
105 stub CreateShortcutInDirEx
106 stub HlinkFindFrame
107 stub SetShellOfflineState
108 stub AddUrlToFavorites
110 stdcall WinList_Init()
111 stub WinList_Terminate
115 stub CreateFromDesktop
116 stub DDECreatePostNotify
117 stub DDEHandleViewFolderNotify
118 stdcall ShellDDEInit(long)
119 stub SHCreateDesktop
120 stub SHDesktopMessageLoop
121 stdcall StopWatchModeFORWARD()
122 stdcall StopWatchFlushFORWARD()
123 stdcall StopWatchAFORWARD(long str long long long)
124 stdcall StopWatchWFORWARD(long wstr long long long)
125 stdcall RunInstallUninstallStubs()
130 stub RunInstallUninstallStubs2
131 stub SHCreateSplashScreen
135 stub IsFileUrl
136 stub IsFileUrlW
137 stub PathIsFilePath
138 stub URLSubLoadString
139 stub OpenPidlOrderStream
140 stub DragDrop
141 stub IEInvalidateImageList
142 stub IEMapPIDLToSystemImageListIndex
143 stub ILIsWeb
145 stub IEGetAttributesOf
146 stub IEBindToObject
147 stub IEGetNameAndFlags
148 stub IEGetDisplayName
149 stub IEBindToObjectEx
150 stub _GetStdLocation
151 stub URLSubRegQueryA
152 stub CShellUIHelper_CreateInstance2
153 stub IsURLChild
158 stub SHRestricted2A
159 stub SHRestricted2W
160 stub SHIsRestricted2W
161 stub @ # CSearchAssistantOC::OnDraw
162 stub CDDEAuto_Navigate
163 stub SHAddSubscribeFavorite
164 stub ResetProfileSharing
165 stub URLSubstitution
167 stub IsIEDefaultBrowser
169 stub ParseURLFromOutsideSourceA
170 stub ParseURLFromOutsideSourceW
171 stub _DeletePidlDPA
172 stub IURLQualify
173 stub SHIsRestricted
174 stub SHIsGlobalOffline
175 stub DetectAndFixAssociations
176 stub EnsureWebViewRegSettings
177 stub WinList_NotifyNewLocation
178 stub WinList_FindFolderWindow
179 stub WinList_GetShellWindows
180 stub WinList_RegisterPending
181 stub WinList_Revoke
183 stub SHMapNbspToSp
185 stub FireEvent_Quit
187 stub SHDGetPageLocation
188 stub SHIEErrorMsgBox
189 stub @ # FIXME: same as ordinal 148
190 stub SHRunIndirectRegClientCommandForward
191 stub SHIsRegisteredClient
192 stub SHGetHistoryPIDL
194 stub IECleanUpAutomationObject
195 stub IEOnFirstBrowserCreation
196 stub IEDDE_WindowDestroyed
197 stub IEDDE_NewWindow
198 stub IsErrorUrl
199 stub @
200 stub SHGetViewStream
203 stub NavToUrlUsingIEA
204 stub NavToUrlUsingIEW
208 stub SearchForElementInHead
209 stub JITCoCreateInstance
210 stub UrlHitsNetW
211 stub ClearAutoSuggestForForms
212 stub GetLinkInfo
213 stub UseCustomInternetSearch
214 stub GetSearchAssistantUrlW
215 stub GetSearchAssistantUrlA
216 stub GetDefaultInternetSearchUrlW
217 stub GetDefaultInternetSearchUrlA
218 stub IEParseDisplayNameWithBCW
219 stub IEILIsEqual
220 stub @
221 stub IECreateFromPathCPWithBCA
222 stub IECreateFromPathCPWithBCW
223 stub ResetWebSettings
224 stub IsResetWebSettingsRequired
225 stub PrepareURLForDisplayUTF8W
226 stub IEIsLinkSafe
227 stub SHUseClassicToolbarGlyphs
228 stub SafeOpenPromptForShellExec
229 stub SafeOpenPromptForPackager

@ stdcall DllCanUnloadNow()
@ stdcall DllGetClassObject(ptr ptr ptr)
@ stdcall DllGetVersion(ptr)
@ stdcall DllInstall(long wstr)
@ stdcall DllRegisterServer()
@ stub DllRegisterWindowClasses
@ stdcall DllUnregisterServer()
@ stub DoAddToFavDlg
@ stub DoAddToFavDlgW
@ stub DoFileDownload
@ stub DoFileDownloadEx
@ stub DoOrganizeFavDlg
@ stub DoOrganizeFavDlgW
@ stub DoPrivacyDlg
@ stub HlinkFrameNavigate
@ stub HlinkFrameNavigateNHL
@ stub IEAboutBox
@ stub IEWriteErrorLog
@ stub ImportPrivacySettings
@ stub InstallReg_RunDLL
@ stdcall OpenURL(long long str long)
@ stub SHGetIDispatchForFolder
@ stdcall SetQueryNetSessionCount(long)
@ stub SoftwareUpdateMessageBox
@ stub URLQualifyA
@ stub URLQualifyW
