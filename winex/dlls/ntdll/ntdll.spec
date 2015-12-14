name	ntdll
type	win32

debug_channels (atom cdrom client console debug delayhlp dll dosfs dosmem file fixup
                global heap heapcheck history int int21 int31 io iocomp loaddll local memfile module nolock ntdll popen process
                profile reg relay resource segment seh selector server snoop
                string sync system tape task thread tid timer timestamp toolhelp ver virtual
                vxd win32)

#note that the Zw... functions are alternate names for the 
#Nt... functions.  (see www.sysinternals.com for details)
#if you change a Nt.. function DON'T FORGET to change the
#Zw one too.

@ stub CsrAllocateCaptureBuffer
@ stub CsrAllocateCapturePointer
@ stub CsrAllocateMessagePointer
@ stub CsrCaptureMessageBuffer
@ stub CsrCaptureMessageString
@ stub CsrCaptureTimeout
@ stub CsrClientCallServer
@ stub CsrClientConnectToServer
@ stub CsrClientMaxMessage
@ stub CsrClientSendMessage
@ stub CsrClientThreadConnect
@ stub CsrFreeCaptureBuffer
@ stub CsrIdentifyAlertableThread
@ stub CsrNewThread
@ stub CsrProbeForRead
@ stub CsrProbeForWrite
@ stub CsrSetPriorityClass
@ stub CsrpProcessCallbackRequest
@ stdcall DbgBreakPoint() DbgBreakPoint
@ varargs DbgPrint(str) DbgPrint
@ stub DbgPrompt
@ stub DbgSsHandleKmApiMsg
@ stub DbgSsInitialize
@ stub DbgUiConnectToDbg
@ stub DbgUiContinue
@ stub DbgUiWaitStateChange
@ stdcall DbgUserBreakPoint() DbgUserBreakPoint
@ stub KiUserApcDispatcher
@ stub KiUserCallbackDispatcher
@ stub KiUserExceptionDispatcher
@ stub LdrAccessResource
@ stdcall LdrDisableThreadCalloutsForDll(long) LdrDisableThreadCalloutsForDll
@ stub LdrEnumResources
@ stub LdrFindEntryForAddress
@ stub LdrFindResourceDirectory_U
@ stub LdrFindResource_U
@ stub LdrGetDllHandle
@ stub LdrGetProcedureAddress
@ stub LdrInitializeThunk
@ stub LdrLoadDll
@ stub LdrLockLoaderLock
@ stub LdrProcessRelocationBlock
@ stub LdrQueryImageFileExecutionOptions
@ stub LdrQueryProcessModuleInformation
@ stub LdrShutdownProcess
@ stub LdrShutdownThread
@ stub LdrUnloadDll
@ stub LdrUnlockLoaderLock
@ stub LdrVerifyImageMatchesChecksum
@ stub NPXEMULATORTABLE
@ extern NlsAnsiCodePage NlsAnsiCodePage
@ extern NlsMbCodePageTag NlsMbCodePageTag
@ extern NlsMbOemCodePageTag NlsMbOemCodePageTag
@ stdcall NtAcceptConnectPort(long long long long long long) NtAcceptConnectPort
@ stdcall NtAccessCheck(ptr long long ptr ptr ptr ptr ptr) NtAccessCheck
@ stub NtAccessCheckAndAuditAlarm
@ stub NtAdjustGroupsToken
@ stdcall NtAdjustPrivilegesToken(long long long long long long) NtAdjustPrivilegesToken
@ stub NtAlertResumeThread
@ stub NtAlertThread
@ stdcall NtAllocateLocallyUniqueId(ptr) NtAllocateLocallyUniqueId
@ stdcall NtAllocateUuids(ptr ptr ptr) NtAllocateUuids
@ stub NtAllocateVirtualMemory
@ stub NtCallbackReturn
@ stub NtCancelIoFile
@ stub NtCancelTimer
@ stdcall NtClearEvent(long) NtClearEvent
@ stdcall NtClose(long) NtClose
@ stub NtCloseObjectAuditAlarm
@ stdcall NtCompleteConnectPort(long) NtCompleteConnectPort
@ stdcall NtConnectPort(long long long long long long long long) NtConnectPort
@ stub NtContinue
@ stdcall NtCreateDirectoryObject(long long long) NtCreateDirectoryObject
@ stdcall NtCreateEvent(long long long long long) NtCreateEvent
@ stub NtCreateEventPair
@ stdcall NtCreateFile(ptr long ptr ptr long long long ptr long long ptr) NtCreateFile
@ stdcall NtCreateIoCompletion(ptr long ptr long)
@ stdcall NtCreateKey(long long long long long long long) NtCreateKey
@ stdcall NtCreateMailslotFile(long long long long long long long long) NtCreateMailslotFile
@ stub NtCreateMutant
@ stub NtCreateNamedPipeFile
@ stdcall NtCreatePagingFile(long long long long) NtCreatePagingFile
@ stdcall NtCreatePort(long long long long long) NtCreatePort
@ stub NtCreateProcess
@ stub NtCreateProfile
@ stdcall NtCreateSection(long long long long long long long) NtCreateSection
@ stdcall NtCreateSemaphore(ptr long ptr long long) NtCreateSemaphore
@ stdcall NtCreateSymbolicLinkObject(ptr long ptr ptr) NtCreateSymbolicLinkObject
@ stub NtCreateThread
@ stdcall NtCreateTimer(ptr long ptr long) NtCreateTimer
@ stub NtCreateToken
@ stdcall NtCurrentTeb() NtCurrentTeb
@ stub NtDelayExecution
@ stub NtDeleteFile
@ stdcall NtDeleteKey(long) NtDeleteKey
@ stdcall NtDeleteValueKey(long ptr) NtDeleteValueKey
@ stdcall NtDeviceIoControlFile(long long long long long long long long long long) NtDeviceIoControlFile
@ stdcall NtDisplayString(ptr)NtDisplayString
@ stdcall NtDuplicateObject(long long long long long long long) NtDuplicateObject
@ stdcall NtDuplicateToken(long long long long long long) NtDuplicateToken
@ stub NtEnumerateBus
@ stdcall NtEnumerateKey (long long long long long long) NtEnumerateKey
@ stdcall NtEnumerateValueKey (long long long long long long) NtEnumerateValueKey
@ stub NtExtendSection
@ stub NtFlushBuffersFile
@ stub NtFlushInstructionCache
@ stdcall NtFlushKey(long) NtFlushKey
@ stub NtFlushVirtualMemory
@ stub NtFlushWriteBuffer
@ stub NtFreeVirtualMemory
@ stdcall NtFsControlFile(long long long long long long long long long long) NtFsControlFile
@ stub NtGetContextThread
@ stub NtGetPlugPlayEvent
@ stdcall NtGetTickCount() NtGetTickCount
@ stub NtImpersonateClientOfPort
@ stub NtImpersonateThread
@ stub NtInitializeRegistry
@ stdcall NtListenPort(long long) NtListenPort
@ stub NtLoadDriver
@ stdcall NtLoadKey(ptr ptr) NtLoadKey
@ stub NtLockFile
@ stub NtLockVirtualMemory
@ stub NtMakeTemporaryObject
@ stdcall NtMapViewOfSection(long long long long long long long long long long) NtMapViewOfSection
@ stub NtNotifyChangeDirectoryFile
@ stdcall NtNotifyChangeKey(long long ptr ptr ptr long long ptr long long) NtNotifyChangeKey
@ stdcall NtOpenDirectoryObject(long long long) NtOpenDirectoryObject
@ stdcall NtOpenEvent(long long long) NtOpenEvent
@ stub NtOpenEventPair
@ stdcall NtOpenFile(ptr long ptr ptr long long) NtOpenFile
@ stub NtOpenIoCompletion
@ stdcall NtOpenKey(ptr long ptr) NtOpenKey
@ stub NtOpenMutant
@ stub NtOpenObjectAuditAlarm
@ stub NtOpenProcess
@ stdcall NtOpenProcessToken(long long long) NtOpenProcessToken
@ stdcall NtOpenSection(long long long) NtOpenSection
@ stdcall NtOpenSemaphore(long long ptr) NtOpenSemaphore
@ stdcall NtOpenSymbolicLinkObject (long long long) NtOpenSymbolicLinkObject
@ stub NtOpenThread
@ stdcall NtOpenThreadToken(long long long long) NtOpenThreadToken
@ stub NtOpenTimer
@ stub NtPlugPlayControl
@ stub NtPrivilegeCheck
@ stub NtPrivilegeObjectAuditAlarm
@ stub NtPrivilegedServiceAuditAlarm
@ stub NtProtectVirtualMemory
@ stdcall NtPulseEvent(long ptr) NtPulseEvent
@ stub NtQueryAttributesFile
@ stub NtQueryDefaultLocale
@ stdcall NtQueryDirectoryFile(long long  ptr ptr ptr ptr long long long ptr long)NtQueryDirectoryFile
@ stdcall NtQueryDirectoryObject(long long long long long long long) NtQueryDirectoryObject
@ stub NtQueryEaFile
@ stdcall NtQueryEvent(long long ptr long ptr) NtQueryEvent
@ stdcall NtQueryInformationFile(long long long long long) NtQueryInformationFile
@ stub NtQueryInformationPort
@ stdcall NtQueryInformationProcess(long long long long long) NtQueryInformationProcess
@ stdcall NtQueryInformationThread (long long ptr long ptr) NtQueryInformationThread
@ stdcall NtQueryInformationToken (long long long long long) NtQueryInformationToken
@ stub NtQueryIntervalProfile
@ stub NtQueryIoCompletion
@ stdcall NtQueryKey (long long ptr long ptr) NtQueryKey
@ stub NtQueryMutant
@ stdcall NtQueryObject(long long long long long) NtQueryObject
@ stub NtQueryOpenSubKeys
@ stdcall NtQueryPerformanceCounter (long long) NtQueryPerformanceCounter
@ stdcall NtQuerySection (long long long long long) NtQuerySection
@ stdcall NtQuerySecurityObject (long long long long long) NtQuerySecurityObject
@ stdcall NtQuerySemaphore (long long long long long) NtQuerySemaphore
@ stdcall NtQuerySymbolicLinkObject(long ptr ptr) NtQuerySymbolicLinkObject
@ stub NtQuerySystemEnvironmentValue
@ stdcall NtQuerySystemInformation(long long long long) NtQuerySystemInformation
@ stdcall NtQuerySystemTime(ptr) NtQuerySystemTime
@ stub NtQueryTimer
@ stdcall NtQueryTimerResolution(long long long) NtQueryTimerResolution
@ stdcall NtQueryValueKey(long long long long long long) NtQueryValueKey
@ stdcall NtQueryVirtualMemory(long ptr long ptr long ptr) NtQueryVirtualMemory
@ stdcall NtQueryVolumeInformationFile(long ptr ptr long long) NtQueryVolumeInformationFile
@ stdcall NtRaiseException(ptr ptr long) NtRaiseException
@ stub NtRaiseHardError
@ stdcall NtReadFile(long long long long long long long long long) NtReadFile
@ stub NtReadRequestData
@ stub NtReadVirtualMemory
@ stub NtRegisterNewDevice
@ stdcall NtRegisterThreadTerminatePort(long) NtRegisterThreadTerminatePort
@ stub NtReleaseMutant
@ stub NtReleaseProcessMutant
@ stdcall NtReleaseSemaphore(long long ptr) NtReleaseSemaphore
@ stdcall NtRemoveIoCompletion(long ptr ptr ptr ptr)
@ stdcall NtReplaceKey(ptr long ptr) NtReplaceKey 
@ stub NtReplyPort
@ stdcall NtReplyWaitReceivePort(long long long long) NtReplyWaitReceivePort
@ stub NtReplyWaitReceivePortEx
@ stub NtReplyWaitReplyPort
@ stub NtRequestPort
@ stdcall NtRequestWaitReplyPort(long long long) NtRequestWaitReplyPort
@ stdcall NtResetEvent(long ptr) NtResetEvent
@ stdcall NtRestoreKey(long long long) NtRestoreKey
@ stdcall NtResumeThread(long long) NtResumeThread
@ stdcall NtSaveKey(long long) NtSaveKey
@ stub NtSecureConnectPort
@ stub NtSetContextThread
@ stub NtSetDefaultHardErrorPort
@ stub NtSetDefaultLocale
@ stub NtSetEaFile
@ stdcall NtSetEvent(long long) NtSetEvent
@ stub NtSetHighEventPair
@ stub NtSetHighWaitLowEventPair
@ stub NtSetHighWaitLowThread
@ stdcall NtSetInformationFile(long long long long long) NtSetInformationFile
@ stdcall NtSetInformationKey(long long ptr long) NtSetInformationKey
@ stub NtSetInformationObject
@ stdcall NtSetInformationProcess(long long long long) NtSetInformationProcess
@ stdcall NtSetInformationThread(long long long long) NtSetInformationThread
@ stub NtSetInformationToken
@ stdcall NtSetIntervalProfile(long long) NtSetIntervalProfile
@ stdcall NtSetIoCompletion(long long ptr long long)
@ stub NtSetLdtEntries
@ stub NtSetLowEventPair
@ stub NtSetLowWaitHighEventPair
@ stub NtSetLowWaitHighThread
@ stdcall NtSetSecurityObject(long long ptr) NtSetSecurityObject
@ stub NtSetSystemEnvironmentValue
@ stdcall NtSetSystemInformation(long ptr long) NtSetSystemInformation
@ stub NtSetSystemPowerState
@ stub NtSetSystemTime
@ stdcall NtSetTimer(long ptr ptr ptr long long ptr) NtSetTimer
@ stub NtSetTimerResolution
@ stdcall NtSetValueKey(long long long long long long) NtSetValueKey
@ stdcall NtSetVolumeInformationFile(long ptr ptr long long) NtSetVolumeInformationFile
@ stub NtShutdownSystem
@ stub NtStartProfile
@ stub NtStopProfile
@ stub NtSuspendThread
@ stub NtSystemDebugControl
@ stdcall NtTerminateProcess(long long)NtTerminateProcess
@ stdcall NtTerminateThread(long long) NtTerminateThread
@ stub NtTestAlert
@ stub NtUnloadDriver
@ stdcall NtUnloadKey(long) NtUnloadKey
@ stub NtUnloadKeyEx
@ stub NtUnlockFile
@ stub NtUnlockVirtualMemory
@ stub NtUnmapViewOfSection
@ stub NtVdmControl
@ stub NtW32Call
@ stub NtWaitForMultipleObjects
@ stub NtWaitForProcessMutant
@ stdcall NtWaitForSingleObject(long long long) NtWaitForSingleObject
@ stub NtWaitHighEventPair
@ stub NtWaitLowEventPair
@ stdcall NtWriteFile(long long long long long long long long long) NtWriteFile
@ stub NtWriteRequestData
@ stub NtWriteVirtualMemory
@ stdcall NtYieldExecution()
@ stub PfxFindPrefix
@ stub PfxInitialize
@ stub PfxInsertPrefix
@ stub PfxRemovePrefix
@ stub RestoreEm87Context
@ stub RtlAbortRXact
@ stub RtlAbsoluteToSelfRelativeSD
@ stdcall RtlAcquirePebLock() RtlAcquirePebLock
@ stdcall RtlAcquireResourceExclusive(ptr long) RtlAcquireResourceExclusive
@ stdcall RtlAcquireResourceShared(ptr long) RtlAcquireResourceShared
@ stdcall RtlAddAccessAllowedAce(long long long long) RtlAddAccessAllowedAce
@ stdcall RtlAddAccessDeniedAce(long long long long) RtlAddAccessDeniedAce
@ stdcall RtlAddAce(ptr long long ptr long) RtlAddAce
@ stub RtlAddActionToRXact
@ stub RtlAddAttributeActionToRXact
@ stub RtlAddAuditAccessAce
@ stdcall RtlAdjustPrivilege(long long long long) RtlAdjustPrivilege
@ stdcall RtlAllocateAndInitializeSid (ptr long long long long long long long long long ptr) RtlAllocateAndInitializeSid
@ stdcall RtlAllocateHeap(long long long) RtlAllocateHeap
@ stub RtlAnsiCharToUnicodeChar
@ stdcall RtlAnsiStringToUnicodeSize(ptr) RtlAnsiStringToUnicodeSize
@ stdcall RtlAnsiStringToUnicodeString(ptr ptr long) RtlAnsiStringToUnicodeString
@ stdcall RtlAppendAsciizToString(ptr str) RtlAppendAsciizToString
@ stdcall RtlAppendStringToString(ptr ptr) RtlAppendStringToString
@ stdcall RtlAppendUnicodeStringToString(ptr ptr) RtlAppendUnicodeStringToString
@ stdcall RtlAppendUnicodeToString(ptr wstr) RtlAppendUnicodeToString
@ stub RtlApplyRXact
@ stub RtlApplyRXactNoFlush
@ stub RtlAreAllAccessesGranted
@ stub RtlAreAnyAccessesGranted
@ stdcall RtlAreBitsClear(ptr long long) RtlAreBitsClear
@ stdcall RtlAreBitsSet(ptr long long) RtlAreBitsSet
@ stdcall RtlAssert(ptr ptr long long) RtlAssert
@ stub RtlCaptureStackBackTrace
@ stub RtlCharToInteger
@ stub RtlCheckRegistryKey
@ stdcall RtlClearAllBits(ptr) RtlClearAllBits
@ stdcall RtlClearBits(ptr long long) RtlClearBits
@ stdcall RtlCompactHeap(long long) RtlCompactHeap
@ stdcall RtlCompareMemory(ptr ptr long) RtlCompareMemory
@ stub RtlCompareMemoryUlong
@ stdcall RtlCompareString(ptr ptr long) RtlCompareString
@ stdcall RtlCompareUnicodeString (ptr ptr long) RtlCompareUnicodeString
@ stub RtlCompressBuffer
@ stub RtlConsoleMultiByteToUnicodeN
@ stub RtlConvertExclusiveToShared
@ stdcall -ret64 RtlConvertLongToLargeInteger(long) RtlConvertLongToLargeInteger
@ stub RtlConvertSharedToExclusive
@ stdcall RtlConvertSidToUnicodeString(ptr ptr long)RtlConvertSidToUnicodeString
@ stub RtlConvertUiListToApiList
@ stdcall -ret64 RtlConvertUlongToLargeInteger(long) RtlConvertUlongToLargeInteger
@ stub RtlCopyLuid
@ stub RtlCopyLuidAndAttributesArray
@ stub RtlCopySecurityDescriptor
@ stdcall RtlCopySid(long ptr ptr) RtlCopySid
@ stub RtlCopySidAndAttributesArray
@ stdcall RtlCopyString(ptr ptr) RtlCopyString
@ stdcall RtlCopyUnicodeString(ptr ptr) RtlCopyUnicodeString
@ stdcall RtlCreateAcl(ptr long long) RtlCreateAcl
@ stub RtlCreateAndSetSD
@ stdcall RtlCreateEnvironment(long long) RtlCreateEnvironment
@ stdcall RtlCreateHeap(long ptr long long ptr ptr) RtlCreateHeap
@ stub RtlCreateProcessParameters
@ stdcall RtlCreateQueryDebugBuffer(long long) RtlCreateQueryDebugBuffer
@ stub RtlCreateRegistryKey
@ stdcall RtlCreateSecurityDescriptor(ptr long) RtlCreateSecurityDescriptor
@ stub RtlCreateTagHeap
@ stdcall RtlCreateUnicodeString(ptr wstr) RtlCreateUnicodeString
@ stdcall RtlCreateUnicodeStringFromAsciiz(ptr str) RtlCreateUnicodeStringFromAsciiz
@ stub RtlCreateUserProcess
@ stub RtlCreateUserSecurityObject
@ stub RtlCreateUserThread
@ stub RtlCustomCPToUnicodeN
@ stub RtlCutoverTimeToSystemTime
@ stub RtlDeNormalizeProcessParams
@ stub RtlDecompressBuffer
@ stub RtlDecompressFragment
@ stub RtlDelete
@ stub RtlDeleteAce
@ stdcall RtlDeleteCriticalSection(ptr) RtlDeleteCriticalSection
@ stub RtlDeleteElementGenericTable
@ stub RtlDeleteRegistryValue
@ stdcall RtlDeleteResource(ptr) RtlDeleteResource
@ stdcall RtlDeleteSecurityObject(long) RtlDeleteSecurityObject
@ stdcall RtlDestroyEnvironment(long) RtlDestroyEnvironment
@ stdcall RtlDestroyHeap(long) RtlDestroyHeap
@ stub RtlDestroyProcessParameters
@ stub RtlDestroyQueryDebugBuffer
@ stub RtlDetermineDosPathNameType_U
@ stub RtlDoesFileExists_U
@ stdcall RtlDosPathNameToNtPathName_U(ptr ptr long long) RtlDosPathNameToNtPathName_U
@ stub RtlDosSearchPath_U
@ stdcall RtlDumpResource(ptr) RtlDumpResource
@ stdcall -ret64 RtlEnlargedIntegerMultiply(long long) RtlEnlargedIntegerMultiply
@ stdcall RtlEnlargedUnsignedDivide(long long long ptr) RtlEnlargedUnsignedDivide
@ stdcall -ret64 RtlEnlargedUnsignedMultiply(long long) RtlEnlargedUnsignedMultiply
@ stdcall RtlEnterCriticalSection(ptr) RtlEnterCriticalSection
@ stub RtlEnumProcessHeaps
@ stub RtlEnumerateGenericTable
@ stub RtlEnumerateGenericTableWithoutSplaying
@ stub RtlEqualComputerName
@ stub RtlEqualDomainName
@ stub RtlEqualLuid
@ stdcall RtlEqualPrefixSid(ptr ptr) RtlEqualPrefixSid
@ stdcall RtlEqualSid (long long) RtlEqualSid
@ stdcall RtlEqualString(ptr ptr long) RtlEqualString
@ stdcall RtlEqualUnicodeString(ptr ptr long) RtlEqualUnicodeString
@ stdcall RtlEraseUnicodeString(ptr) RtlEraseUnicodeString
@ stub RtlExpandEnvironmentStrings_U
@ stub RtlExtendHeap
@ stdcall -ret64 RtlExtendedIntegerMultiply(long long long) RtlExtendedIntegerMultiply
@ stdcall -ret64 RtlExtendedLargeIntegerDivide(long long long ptr) RtlExtendedLargeIntegerDivide
@ stub RtlExtendedMagicDivide
@ stdcall RtlFillMemory(ptr long long) RtlFillMemory
@ stdcall RtlFillMemoryUlong(ptr long long) RtlFillMemoryUlong
@ stdcall RtlFindClearBits(ptr long long) RtlFindClearBits
@ stdcall RtlFindClearBitsAndSet(ptr long long) RtlFindClearBitsAndSet
@ stdcall RtlFindClearRuns(ptr ptr long long) RtlFindClearRuns
@ stdcall RtlFindLastBackwardRunClear(ptr long ptr) RtlFindLastBackwardRunClear
@ stdcall RtlFindLastBackwardRunSet(ptr long ptr) RtlFindLastBackwardRunSet
@ stdcall RtlFindLeastSignificantBit(ptr long long) RtlFindLeastSignificantBit
@ stdcall RtlFindLongestRunClear(ptr long) RtlFindLongestRunClear
@ stdcall RtlFindLongestRunSet(ptr long) RtlFindLongestRunSet
@ stub RtlFindMessage
@ stdcall RtlFindMostSignificantBit(ptr long long) RtlFindMostSignificantBit
@ stdcall RtlFindNextForwardRunClear(ptr long ptr) RtlFindNextForwardRunClear
@ stdcall RtlFindNextForwardRunSet(ptr long ptr) RtlFindNextForwardRunSet
@ stdcall RtlFindSetBits(ptr long long) RtlFindSetBits
@ stdcall RtlFindSetBitsAndClear(ptr long long) RtlFindSetBitsAndClear
@ stdcall RtlFindSetRuns(ptr ptr long long) RtlFindSetRuns
@ stdcall RtlFirstFreeAce(ptr ptr) RtlFirstFreeAce
@ stdcall RtlFormatCurrentUserKeyPath(ptr) RtlFormatCurrentUserKeyPath
@ stub RtlFormatMessage
@ stdcall RtlFreeAnsiString(long) RtlFreeAnsiString
@ stdcall RtlFreeHeap(long long long) RtlFreeHeap
@ stdcall RtlFreeOemString(ptr) RtlFreeOemString
@ stdcall RtlFreeSid (long) RtlFreeSid
@ stdcall RtlFreeUnicodeString(ptr) RtlFreeUnicodeString
@ stub RtlGenerate8dot3Name
@ stdcall RtlGetAce(ptr long ptr) RtlGetAce
@ stdcall RtlGetNtVersionNumbers(ptr ptr ptr) RtlGetNtVersionNumbers
@ stub RtlGetVersion
@ stub RtlGetCallersAddress
@ stub RtlGetCompressionWorkSpaceSize
@ stdcall RtlGetControlSecurityDescriptor(ptr ptr ptr) RtlGetControlSecurityDescriptor
@ stub RtlGetCurrentDirectory_U
@ stdcall RtlGetDaclSecurityDescriptor(ptr ptr ptr ptr) RtlGetDaclSecurityDescriptor
@ stub RtlGetElementGenericTable
@ stub RtlGetFullPathName_U
@ stdcall RtlGetGroupSecurityDescriptor(ptr ptr ptr) RtlGetGroupSecurityDescriptor
@ stdcall RtlGetLongestNtPathLength() RtlGetLongestNtPathLength
@ stub RtlGetNtGlobalFlags
@ stdcall RtlGetNtProductType(ptr) RtlGetNtProductType
@ stdcall RtlGetOwnerSecurityDescriptor(ptr ptr ptr) RtlGetOwnerSecurityDescriptor
@ stdcall RtlGetProcessHeaps(long ptr) RtlGetProcessHeaps
@ stdcall RtlGetSaclSecurityDescriptor(ptr ptr ptr ptr)RtlGetSaclSecurityDescriptor
@ stub RtlGetUserInfoHeap
@ stub RtlGUIDFromString
@ stdcall RtlIdentifierAuthoritySid(ptr) RtlIdentifierAuthoritySid
@ stdcall RtlImageDirectoryEntryToData(long long long ptr) RtlImageDirectoryEntryToData
@ stdcall RtlImageNtHeader(long) RtlImageNtHeader
@ stdcall RtlImageRvaToSection(ptr long long) RtlImageRvaToSection
@ stdcall RtlImageRvaToVa(ptr long long ptr) RtlImageRvaToVa
@ stdcall RtlImpersonateSelf(long) RtlImpersonateSelf
@ stdcall RtlInitAnsiString(ptr str) RtlInitAnsiString
@ stub RtlInitCodePageTable
@ stub RtlInitNlsTables
@ stdcall RtlInitString(ptr str) RtlInitString
@ stdcall RtlInitUnicodeString(ptr wstr) RtlInitUnicodeString
@ stdcall RtlInitializeBitMap(ptr long long) RtlInitializeBitMap
@ stub RtlInitializeContext
@ stdcall RtlInitializeCriticalSection(ptr) RtlInitializeCriticalSection
@ stdcall RtlInitializeCriticalSectionAndSpinCount(ptr long) RtlInitializeCriticalSectionAndSpinCount
@ stdcall RtlInitializeGenericTable() RtlInitializeGenericTable
@ stub RtlInitializeRXact
@ stdcall RtlInitializeResource(ptr) RtlInitializeResource
@ stdcall RtlInitializeSid(ptr ptr long) RtlInitializeSid
@ stdcall RtlInitializeSListHead(ptr)
@ stub RtlInsertElementGenericTable
@ stdcall RtlIntegerToChar(long long long long) RtlIntegerToChar
@ stub RtlIntegerToUnicodeString
@ stdcall RtlInterlockedFlushSList(ptr)
@ stdcall RtlInterlockedPopEntrySList(ptr)
@ stdcall RtlInterlockedPushEntrySList(ptr ptr)
@ stub RtlIsDosDeviceName_U
@ stub RtlIsGenericTableEmpty
@ stub RtlIsNameLegalDOS8Dot3
@ stdcall RtlIsTextUnicode(ptr long ptr) RtlIsTextUnicode
@ stdcall -ret64 RtlLargeIntegerAdd(long long long long) RtlLargeIntegerAdd
@ stdcall -ret64 RtlLargeIntegerArithmeticShift(long long long) RtlLargeIntegerArithmeticShift
@ stdcall -ret64 RtlLargeIntegerDivide(long long long long ptr) RtlLargeIntegerDivide
@ stdcall -ret64 RtlLargeIntegerNegate(long long) RtlLargeIntegerNegate
@ stdcall -ret64 RtlLargeIntegerShiftLeft(long long long) RtlLargeIntegerShiftLeft
@ stdcall -ret64 RtlLargeIntegerShiftRight(long long long) RtlLargeIntegerShiftRight
@ stdcall -ret64 RtlLargeIntegerSubtract(long long long long) RtlLargeIntegerSubtract
@ stub RtlLargeIntegerToChar
@ stdcall RtlLeaveCriticalSection(ptr) RtlLeaveCriticalSection
@ stdcall RtlLengthRequiredSid(long) RtlLengthRequiredSid
@ stdcall RtlLengthSecurityDescriptor(ptr) RtlLengthSecurityDescriptor
@ stdcall RtlLengthSid(ptr) RtlLengthSid
@ stub RtlLocalTimeToSystemTime
@ stdcall RtlLockHeap(long) RtlLockHeap
@ stub RtlLookupElementGenericTable
@ stdcall RtlMakeSelfRelativeSD(ptr ptr ptr) RtlMakeSelfRelativeSD
@ stub RtlMapGenericMask
@ stdcall RtlMoveMemory(ptr ptr long) RtlMoveMemory
@ stdcall RtlMultiByteToUnicodeN(ptr long ptr ptr long) RtlMultiByteToUnicodeN
@ stdcall RtlMultiByteToUnicodeSize(ptr str long) RtlMultiByteToUnicodeSize
@ stub RtlNewInstanceSecurityObject
@ stub RtlNewSecurityGrantedAccess
@ stdcall RtlNewSecurityObject(long long long long long long) RtlNewSecurityObject
@ stdcall RtlNormalizeProcessParams(ptr) RtlNormalizeProcessParams
@ stdcall RtlNtStatusToDosError(long) RtlNtStatusToDosError
@ stub RtlNumberGenericTableElements
@ stdcall RtlNumberOfClearBits(ptr) RtlNumberOfClearBits
@ stdcall RtlNumberOfSetBits(ptr) RtlNumberOfSetBits
@ stdcall RtlOemStringToUnicodeSize(ptr) RtlOemStringToUnicodeSize
@ stdcall RtlOemStringToUnicodeString(ptr ptr long) RtlOemStringToUnicodeString
@ stdcall RtlOemToUnicodeN(ptr long ptr ptr long) RtlOemToUnicodeN
@ stdcall RtlOpenCurrentUser(long ptr) RtlOpenCurrentUser
@ stub RtlPcToFileHeader
@ stdcall RtlPrefixString(ptr ptr long) RtlPrefixString
@ stdcall RtlPrefixUnicodeString(ptr ptr long) RtlPrefixUnicodeString
@ stub RtlProtectHeap
@ stdcall RtlQueryDepthSList(ptr)
@ stdcall RtlQueryEnvironmentVariable_U(long long long) RtlQueryEnvironmentVariable_U
@ stub RtlQueryInformationAcl
@ stub RtlQueryProcessBackTraceInformation
@ stub RtlQueryProcessDebugInformation
@ stub RtlQueryProcessHeapInformation
@ stub RtlQueryProcessLockInformation
@ stub RtlQueryRegistryValues
@ stub RtlQuerySecurityObject
@ stub RtlQueryTagHeap
@ stub RtlQueryTimeZoneInformation
@ stdcall RtlRaiseException(ptr) RtlRaiseException
@ stdcall RtlRaiseStatus(long) RtlRaiseStatus
@ stub RtlRandom
@ stdcall RtlReAllocateHeap(long long ptr long) RtlReAllocateHeap
@ stub RtlRealPredecessor
@ stub RtlRealSuccessor
@ stdcall RtlReleasePebLock() RtlReleasePebLock
@ stdcall RtlReleaseResource(ptr) RtlReleaseResource
@ stub RtlRemoteCall
@ stub RtlResetRtlTranslations
@ stub RtlRunDecodeUnicodeString
@ stub RtlRunEncodeUnicodeString
@ stdcall RtlSecondsSince1970ToTime(long ptr) RtlSecondsSince1970ToTime
@ stdcall RtlSecondsSince1980ToTime(long ptr) RtlSecondsSince1980ToTime
@ stub RtlSelfRelativeToAbsoluteSD
@ stdcall RtlSetAllBits(ptr) RtlSetAllBits
@ stdcall RtlSetBits(ptr long long) RtlSetBits
@ stdcall RtlSetCriticalSectionSpinCount(ptr long) RtlSetCriticalSectionSpinCount
@ stub RtlSetCurrentDirectory_U
@ stub RtlSetCurrentEnvironment
@ stdcall RtlSetDaclSecurityDescriptor(ptr long ptr long) RtlSetDaclSecurityDescriptor
@ stdcall RtlSetEnvironmentVariable(long long long) RtlSetEnvironmentVariable
@ stdcall RtlSetGroupSecurityDescriptor(ptr ptr long) RtlSetGroupSecurityDescriptor
@ stub RtlSetInformationAcl
@ stdcall RtlSetOwnerSecurityDescriptor(ptr ptr long) RtlSetOwnerSecurityDescriptor
@ stdcall RtlSetSaclSecurityDescriptor(ptr long ptr long) RtlSetSaclSecurityDescriptor
@ stub RtlSetSecurityObject
@ stub RtlSetTimeZoneInformation
@ stub RtlSetUserFlagsHeap
@ stub RtlSetUserValueHeap
@ stdcall RtlSizeHeap(long long ptr) RtlSizeHeap
@ stub RtlSplay
@ stub RtlStartRXact
@ stub RtlStringFromGUID
@ stdcall RtlSubAuthorityCountSid(ptr) RtlSubAuthorityCountSid
@ stdcall RtlSubAuthoritySid(ptr long) RtlSubAuthoritySid
@ stub RtlSubtreePredecessor
@ stub RtlSubtreeSuccessor
@ stdcall RtlSystemTimeToLocalTime (long long) RtlSystemTimeToLocalTime
@ stdcall RtlTimeFieldsToTime(ptr ptr) RtlTimeFieldsToTime
@ stdcall RtlTimeToElapsedTimeFields(long long) RtlTimeToElapsedTimeFields
@ stdcall RtlTimeToSecondsSince1970(ptr ptr) RtlTimeToSecondsSince1970
@ stdcall RtlTimeToSecondsSince1980(ptr ptr) RtlTimeToSecondsSince1980
@ stdcall RtlTimeToTimeFields (long long) RtlTimeToTimeFields
@ stub RtlUnhandledExceptionFilter
@ stdcall RtlUnicodeStringToAnsiSize(ptr) RtlUnicodeStringToAnsiSize
@ stdcall RtlUnicodeStringToAnsiString(ptr ptr long) RtlUnicodeStringToAnsiString
@ stub RtlUnicodeStringToCountedOemString
@ stub RtlUnicodeStringToInteger
@ stdcall RtlUnicodeStringToOemSize(ptr) RtlUnicodeStringToOemSize
@ stdcall RtlUnicodeStringToOemString(ptr ptr long) RtlUnicodeStringToOemString
@ stub RtlUnicodeToCustomCPN
@ stdcall RtlUnicodeToMultiByteN(ptr long ptr ptr long) RtlUnicodeToMultiByteN
@ stdcall RtlUnicodeToMultiByteSize(ptr wstr long) RtlUnicodeToMultiByteSize
@ stdcall RtlUnicodeToOemN(ptr long ptr ptr long) RtlUnicodeToOemN
@ stub RtlUniform
@ stdcall RtlUnlockHeap(long) RtlUnlockHeap
@ stdcall RtlUnwind(ptr ptr ptr long) RtlUnwind
@ stub RtlUpcaseUnicodeChar
@ stdcall RtlUpcaseUnicodeString(ptr ptr long) RtlUpcaseUnicodeString
@ stdcall RtlUpcaseUnicodeStringToAnsiString(ptr ptr long) RtlUpcaseUnicodeStringToAnsiString
@ stub RtlUpcaseUnicodeStringToCountedOemString
@ stdcall RtlUpcaseUnicodeStringToOemString(ptr ptr long) RtlUpcaseUnicodeStringToOemString
@ stub RtlUpcaseUnicodeToCustomCPN
@ stdcall RtlUpcaseUnicodeToMultiByteN(ptr long ptr ptr long) RtlUpcaseUnicodeToMultiByteN
@ stdcall RtlUpcaseUnicodeToOemN(ptr long ptr ptr long) RtlUpcaseUnicodeToOemN
@ stub RtlUpperChar
@ stdcall RtlUpperString(ptr ptr) RtlUpperString
@ stub RtlUsageHeap
@ stdcall RtlValidAcl(ptr) RtlValidAcl
@ stdcall RtlValidSecurityDescriptor(ptr) RtlValidSecurityDescriptor
@ stdcall RtlValidSid(ptr) RtlValidSid
@ stdcall RtlValidateHeap(long long ptr) RtlValidateHeap
@ stub RtlValidateProcessHeaps
@ stdcall RtlWalkHeap(long ptr) RtlWalkHeap
@ stub RtlWriteRegistryValue
@ stub RtlZeroHeap
@ stdcall RtlZeroMemory(ptr long) RtlZeroMemory
@ stub RtlpInitializeRtl
@ stub RtlpNtCreateKey
@ stub RtlpNtEnumerateSubKey
@ stub RtlpNtMakeTemporaryKey
@ stub RtlpNtOpenKey
@ stub RtlpNtQueryValueKey
@ stub RtlpNtSetValueKey
@ stdcall RtlpUnWaitCriticalSection(ptr) RtlpUnWaitCriticalSection
@ stdcall RtlpWaitForCriticalSection(ptr) RtlpWaitForCriticalSection
@ stdcall RtlxAnsiStringToUnicodeSize(ptr) RtlAnsiStringToUnicodeSize
@ stdcall RtlxOemStringToUnicodeSize(ptr) RtlOemStringToUnicodeSize
@ stdcall RtlxUnicodeStringToAnsiSize(ptr) RtlUnicodeStringToAnsiSize
@ stdcall RtlxUnicodeStringToOemSize(ptr) RtlUnicodeStringToOemSize
@ stub SaveEm87Context
@ stdcall ZwAcceptConnectPort(long long long long long long) NtAcceptConnectPort
@ stub ZwAccessCheck
@ stub ZwAccessCheckAndAuditAlarm
@ stdcall ZwAdjustGroupsToken(long long long long long long) NtAdjustPrivilegesToken
@ stub ZwAdjustPrivilegesToken
@ stub ZwAlertResumeThread
@ stub ZwAlertThread
@ stub ZwAllocateLocallyUniqueId
@ stub ZwAllocateUuids
@ stub ZwAllocateVirtualMemory
@ stub ZwCallbackReturn
@ stub ZwCancelIoFile
@ stub ZwCancelTimer
@ stub ZwClearEvent
@ stub ZwClose
@ stub ZwCloseObjectAuditAlarm
@ stdcall ZwCompleteConnectPort(long) NtCompleteConnectPort
@ stdcall ZwConnectPort(long long long long long long long long) NtConnectPort
@ stub ZwContinue
@ stdcall ZwCreateDirectoryObject(long long long) NtCreateDirectoryObject
@ stdcall ZwCreateEvent(long long long long long) NtCreateEvent
@ stub ZwCreateEventPair
@ stdcall ZwCreateFile(ptr long ptr ptr long long long ptr long long ptr) NtCreateFile
@ stub ZwCreateIoCompletion
@ stdcall ZwCreateKey(long long long long long long long) NtCreateKey
@ stdcall ZwCreateMailslotFile(long long long long long long long long) NtCreateMailslotFile
@ stub ZwCreateMutant
@ stub ZwCreateNamedPipeFile
@ stdcall ZwCreatePagingFile(long long long long) NtCreatePagingFile
@ stdcall ZwCreatePort(long long long long long) NtCreatePort
@ stub ZwCreateProcess
@ stub ZwCreateProfile
@ stdcall ZwCreateSection(long long long long long long long) NtCreateSection
@ stub ZwCreateSemaphore
@ stub ZwCreateSymbolicLinkObject
@ stub ZwCreateThread
@ stdcall ZwCreateTimer(ptr long ptr long) NtCreateTimer
@ stub ZwCreateToken
@ stub ZwDelayExecution
@ stub ZwDeleteFile
@ stdcall ZwDeleteKey(long) NtDeleteKey
@ stdcall ZwDeleteValueKey(long ptr) NtDeleteValueKey
@ stdcall ZwDeviceIoControlFile(long long long long long long long long long long) NtDeviceIoControlFile
@ stub ZwDisplayString
@ stdcall ZwDuplicateObject(long long long long long long long) NtDuplicateObject
@ stdcall ZwDuplicateToken(long long long long long long) NtDuplicateToken
@ stub ZwEnumerateBus
@ stdcall ZwEnumerateKey(long long long ptr long ptr) NtEnumerateKey
@ stdcall ZwEnumerateValueKey(long long long ptr long ptr) NtEnumerateValueKey
@ stub ZwExtendSection
@ stub ZwFlushBuffersFile
@ stub ZwFlushInstructionCache
@ stdcall ZwFlushKey(long) NtFlushKey
@ stub ZwFlushVirtualMemory
@ stub ZwFlushWriteBuffer
@ stub ZwFreeVirtualMemory
@ stdcall ZwFsControlFile(long long long long long long long long long long) NtFsControlFile
@ stub ZwGetContextThread
@ stub ZwGetPlugPlayEvent
@ stdcall ZwGetTickCount() NtGetTickCount
@ stub ZwImpersonateClientOfPort
@ stub ZwImpersonateThread
@ stub ZwInitializeRegistry
@ stdcall ZwListenPort(long long) NtListenPort
@ stub ZwLoadDriver
@ stdcall ZwLoadKey(ptr ptr) NtLoadKey
@ stub ZwLockFile
@ stub ZwLockVirtualMemory
@ stub ZwMakeTemporaryObject
@ stdcall ZwMapViewOfSection(long long long long long long long long long long) NtMapViewOfSection
@ stub ZwNotifyChangeDirectoryFile
@ stdcall ZwNotifyChangeKey(long long ptr ptr ptr long long ptr long long) NtNotifyChangeKey
@ stdcall ZwOpenDirectoryObject(long long long) NtOpenDirectoryObject
@ stdcall ZwOpenEvent(long long long) NtOpenEvent
@ stub ZwOpenEventPair
@ stdcall ZwOpenFile(ptr long ptr ptr long long) NtOpenFile
@ stub ZwOpenIoCompletion
@ stdcall ZwOpenKey(ptr long ptr) NtOpenKey
@ stub ZwOpenMutant
@ stub ZwOpenObjectAuditAlarm
@ stub ZwOpenProcess
@ stdcall ZwOpenProcessToken(long long long) NtOpenProcessToken
@ stdcall ZwOpenSection(long long long) NtOpenSection
@ stub ZwOpenSemaphore
@ stub ZwOpenSymbolicLinkObject
@ stub ZwOpenThread
@ stdcall ZwOpenThreadToken(long long long long) NtOpenThreadToken
@ stub ZwOpenTimer
@ stub ZwPlugPlayControl
@ stub ZwPrivilegeCheck
@ stub ZwPrivilegeObjectAuditAlarm
@ stub ZwPrivilegedServiceAuditAlarm
@ stub ZwProtectVirtualMemory
@ stub ZwPulseEvent
@ stub ZwQueryAttributesFile
@ stub ZwQueryDefaultLocale
@ stdcall ZwQueryDirectoryFile(long long  ptr ptr ptr ptr long long long ptr long)NtQueryDirectoryFile
@ stdcall ZwQueryDirectoryObject(long long long long long long long) NtQueryDirectoryObject
@ stub ZwQueryEaFile
@ stub ZwQueryEvent
@ stdcall ZwQueryInformationFile(long long long long long) NtQueryInformationFile
@ stub ZwQueryInformationPort
@ stdcall ZwQueryInformationProcess(long long long long long) NtQueryInformationProcess
@ stdcall ZwQueryInformationThread(long long ptr long ptr) NtQueryInformationThread
@ stdcall ZwQueryInformationToken(long long long long long) NtQueryInformationToken
@ stub ZwQueryIntervalProfile
@ stub ZwQueryIoCompletion
@ stdcall ZwQueryKey(long long ptr long ptr) NtQueryKey
@ stub ZwQueryMutant
@ stdcall ZwQueryObject(long long long long long) NtQueryObject
@ stdcall ZwQueryPerformanceCounter(long long) NtQueryPerformanceCounter
@ stub ZwQuerySection
@ stub ZwQuerySecurityObject
@ stub ZwQuerySemaphore
@ stub ZwQuerySymbolicLinkObject
@ stub ZwQuerySystemEnvironmentValue
@ stdcall ZwQuerySystemInformation(long long long long) NtQuerySystemInformation
@ stdcall ZwQuerySystemTime(ptr) NtQuerySystemTime
@ stub ZwQueryTimer
@ stub ZwQueryTimerResolution
@ stdcall ZwQueryValueKey(long ptr long ptr long ptr) NtQueryValueKey
@ stdcall ZwQueryVirtualMemory(long ptr long ptr long ptr) NtQueryVirtualMemory
@ stdcall ZwQueryVolumeInformationFile(long ptr ptr long long) NtQueryVolumeInformationFile
@ stub ZwRaiseException
@ stub ZwRaiseHardError
@ stdcall ZwReadFile(long long long long long long long long long) NtReadFile
@ stub ZwReadRequestData
@ stub ZwReadVirtualMemory
@ stub ZwRegisterNewDevice
@ stdcall ZwRegisterThreadTerminatePort(long) NtRegisterThreadTerminatePort
@ stub ZwReleaseMutant
@ stub ZwReleaseProcessMutant
@ stub ZwReleaseSemaphore
@ stub ZwRemoveIoCompletion
@ stdcall ZwReplaceKey(ptr long ptr) NtReplaceKey
@ stub ZwReplyPort
@ stdcall ZwReplyWaitReceivePort(long long long long) NtReplyWaitReceivePort
@ stub ZwReplyWaitReplyPort
@ stub ZwRequestPort
@ stdcall ZwRequestWaitReplyPort(long long long) NtRequestWaitReplyPort
@ stub ZwResetEvent
@ stdcall ZwRestoreKey(long long long) NtRestoreKey
@ stdcall ZwResumeThread(long long) NtResumeThread
@ stdcall ZwSaveKey(long long) NtSaveKey
@ stub ZwSetContextThread
@ stub ZwSetDefaultHardErrorPort
@ stub ZwSetDefaultLocale
@ stub ZwSetEaFile
@ stdcall ZwSetEvent(long long) NtSetEvent
@ stub ZwSetHighEventPair
@ stub ZwSetHighWaitLowEventPair
@ stub ZwSetHighWaitLowThread
@ stdcall ZwSetInformationFile(long long long long long) NtSetInformationFile
@ stdcall ZwSetInformationKey(long long ptr long) NtSetInformationKey
@ stub ZwSetInformationObject
@ stdcall ZwSetInformationProcess(long long long long) NtSetInformationProcess
@ stdcall ZwSetInformationThread(long long long long) NtSetInformationThread
@ stub ZwSetInformationToken
@ stdcall ZwSetIntervalProfile(long long) NtSetIntervalProfile
@ stub ZwSetIoCompletion
@ stub ZwSetLdtEntries
@ stub ZwSetLowEventPair
@ stub ZwSetLowWaitHighEventPair
@ stub ZwSetLowWaitHighThread
@ stub ZwSetSecurityObject
@ stub ZwSetSystemEnvironmentValue
@ stub ZwSetSystemInformation
@ stub ZwSetSystemPowerState
@ stub ZwSetSystemTime
@ stdcall ZwSetTimer(long ptr ptr ptr long long ptr) NtSetTimer
@ stub ZwSetTimerResolution
@ stdcall ZwSetValueKey(long long long long long long) NtSetValueKey
@ stdcall ZwSetVolumeInformationFile(long ptr ptr long long) NtSetVolumeInformationFile
@ stub ZwShutdownSystem
@ stub ZwStartProfile
@ stub ZwStopProfile
@ stub ZwSuspendThread
@ stub ZwSystemDebugControl
@ stub ZwTerminateProcess
@ stdcall ZwTerminateThread(long long) NtTerminateThread
@ stub ZwTestAlert
@ stub ZwUnloadDriver
@ stdcall ZwUnloadKey(long) NtUnloadKey
@ stub ZwUnlockFile
@ stub ZwUnlockVirtualMemory
@ stub ZwUnmapViewOfSection
@ stub ZwVdmControl
@ stub ZwW32Call
@ stub ZwWaitForMultipleObjects
@ stub ZwWaitForProcessMutant
@ stdcall ZwWaitForSingleObject(long long long) NtWaitForSingleObject
@ stub ZwWaitHighEventPair
@ stub ZwWaitLowEventPair
@ stub ZwWriteFile
@ stub ZwWriteRequestData
@ stub ZwWriteVirtualMemory
@ stdcall ZwYieldExecution() NtYieldExecution
@ cdecl _CIpow() NTDLL__CIpow
@ stub __eCommonExceptions
@ stub __eEmulatorInit
@ stub __eF2XM1
@ stub __eFABS
@ stub __eFADD32
@ stub __eFADD64
@ stub __eFADDPreg
@ stub __eFADDreg
@ stub __eFADDtop
@ stub __eFCHS
@ stub __eFCOM
@ stub __eFCOM32
@ stub __eFCOM64
@ stub __eFCOMP
@ stub __eFCOMP32
@ stub __eFCOMP64
@ stub __eFCOMPP
@ stub __eFCOS
@ stub __eFDECSTP
@ stub __eFDIV32
@ stub __eFDIV64
@ stub __eFDIVPreg
@ stub __eFDIVR32
@ stub __eFDIVR64
@ stub __eFDIVRPreg
@ stub __eFDIVRreg
@ stub __eFDIVRtop
@ stub __eFDIVreg
@ stub __eFDIVtop
@ stub __eFFREE
@ stub __eFIADD16
@ stub __eFIADD32
@ stub __eFICOM16
@ stub __eFICOM32
@ stub __eFICOMP16
@ stub __eFICOMP32
@ stub __eFIDIV16
@ stub __eFIDIV32
@ stub __eFIDIVR16
@ stub __eFIDIVR32
@ stub __eFILD16
@ stub __eFILD32
@ stub __eFILD64
@ stub __eFIMUL16
@ stub __eFIMUL32
@ stub __eFINCSTP
@ stub __eFINIT
@ stub __eFIST16
@ stub __eFIST32
@ stub __eFISTP16
@ stub __eFISTP32
@ stub __eFISTP64
@ stub __eFISUB16
@ stub __eFISUB32
@ stub __eFISUBR16
@ stub __eFISUBR32
@ stub __eFLD1
@ stub __eFLD32
@ stub __eFLD64
@ stub __eFLD80
@ stub __eFLDCW
@ stub __eFLDENV
@ stub __eFLDL2E
@ stub __eFLDLN2
@ stub __eFLDPI
@ stub __eFLDZ
@ stub __eFMUL32
@ stub __eFMUL64
@ stub __eFMULPreg
@ stub __eFMULreg
@ stub __eFMULtop
@ stub __eFPATAN
@ stub __eFPREM
@ stub __eFPREM1
@ stub __eFPTAN
@ stub __eFRNDINT
@ stub __eFRSTOR
@ stub __eFSAVE
@ stub __eFSCALE
@ stub __eFSIN
@ stub __eFSQRT
@ stub __eFST
@ stub __eFST32
@ stub __eFST64
@ stub __eFSTCW
@ stub __eFSTENV
@ stub __eFSTP
@ stub __eFSTP32
@ stub __eFSTP64
@ stub __eFSTP80
@ stub __eFSTSW
@ stub __eFSUB32
@ stub __eFSUB64
@ stub __eFSUBPreg
@ stub __eFSUBR32
@ stub __eFSUBR64
@ stub __eFSUBRPreg
@ stub __eFSUBRreg
@ stub __eFSUBRtop
@ stub __eFSUBreg
@ stub __eFSUBtop
@ stub __eFTST
@ stub __eFUCOM
@ stub __eFUCOMP
@ stub __eFUCOMPP
@ stub __eFXAM
@ stub __eFXCH
@ stub __eFXTRACT
@ stub __eFYL2X
@ stub __eFYL2XP1
@ stub __eGetStatusWord
@ stdcall -ret64 _alldiv(long long long long) _alldiv
@ stdcall -ret64 _allmul(long long long long) _allmul
@ stdcall -register -i386 _alloca_probe() NTDLL_alloca_probe
@ stdcall -ret64 _allrem(long long long long) _allrem
@ stdcall -ret64 _aulldiv(long long long long) _aulldiv
@ stdcall -ret64 _aullrem(long long long long) _aullrem
@ stdcall -register -i386 _chkstk() NTDLL_chkstk
@ stub _fltused
@ cdecl _ftol() NTDLL__ftol
@ cdecl _itoa(long ptr long) _itoa
@ cdecl _ltoa(long ptr long) _ltoa
@ cdecl _memccpy(ptr ptr long long) memccpy
@ cdecl _memicmp(str str long) _memicmp
@ varargs _snprintf(ptr long ptr) snprintf
@ varargs _snwprintf(wstr long wstr) _snwprintf
@ cdecl _splitpath(str ptr ptr ptr ptr) _splitpath
@ cdecl _strcmpi(str str) strcasecmp
@ cdecl _stricmp(str str) strcasecmp
@ cdecl _strlwr(str) _strlwr
@ cdecl _strnicmp(str str long) strncasecmp
@ cdecl _strupr(str) _strupr
@ cdecl _ultoa(long ptr long) _ultoa
@ cdecl _ultow(long ptr long) _ultow
@ cdecl _vsnprintf(ptr long str ptr) vsnprintf
@ cdecl _vsnwprintf(ptr long wstr ptr) NTDLL_vsnwprintf
@ cdecl _wcsicmp(wstr wstr) NTDLL__wcsicmp
@ cdecl _wcslwr(wstr) NTDLL__wcslwr
@ cdecl _wcsnicmp(wstr wstr long) NTDLL__wcsnicmp
@ cdecl _wcsupr(wstr) NTDLL__wcsupr
@ cdecl _wtoi(wstr) _wtoi
@ cdecl _wtol(wstr) _wtol
@ cdecl -noimport abs(long) abs
@ cdecl -noimport atan(double) atan
@ cdecl -noimport atoi(str) atoi
@ cdecl -noimport atol(str) atol
@ cdecl -noimport ceil(double) ceil
@ cdecl -noimport cos(double) cos
@ cdecl -noimport fabs(double) fabs
@ cdecl -noimport floor(double) floor
@ cdecl isalpha(long) isalpha
@ cdecl isdigit(long) isdigit
@ cdecl islower(long) islower
@ cdecl isprint(long) isprint
@ cdecl isspace(long) isspace
@ cdecl isupper(long) isupper
@ cdecl iswalpha(long) NTDLL_iswalpha
@ cdecl iswctype(long long) NTDLL_iswctype
@ cdecl iswlower(long) NTDLL_iswlower
@ cdecl iswspace(long) NTDLL_iswspace
@ cdecl iswdigit(long) NTDLL_iswdigit
@ cdecl iswxdigit(long) NTDLL_iswxdigit
@ cdecl isxdigit(long) isxdigit
@ cdecl -noimport labs(long) labs
@ cdecl -noimport log(double) log
@ cdecl mbstowcs(ptr str long) NTDLL_mbstowcs
@ cdecl -noimport memchr(ptr long long) memchr
@ cdecl -noimport memcmp(ptr ptr long) memcmp
@ cdecl -noimport memcpy(ptr ptr long) memcpy
@ cdecl -noimport memmove(ptr ptr long) memmove
@ cdecl -noimport memset(ptr long long) memset
@ cdecl -noimport pow(double double) pow
@ cdecl -noimport qsort(ptr long long ptr) qsort
@ cdecl -noimport sin(double) sin
@ varargs -noimport sprintf(str str) sprintf
@ cdecl -noimport sqrt(double) sqrt
@ varargs -noimport sscanf(str str) sscanf
@ cdecl -noimport strcat(str str) strcat
@ cdecl -noimport strchr(str long) strchr
@ cdecl -noimport strcmp(str str) strcmp
@ cdecl -noimport strcpy(ptr str) strcpy
@ cdecl -noimport strcspn(str str) strcspn
@ cdecl -noimport strlen(str) strlen
@ cdecl -noimport strncat(str str long) strncat
@ cdecl -noimport strncmp(str str long) strncmp
@ cdecl -noimport strncpy(ptr str long) strncpy
@ cdecl -noimport strpbrk(str str) strpbrk
@ cdecl -noimport strrchr(str long) strrchr
@ cdecl -noimport strspn(str str) strspn
@ cdecl -noimport strstr(str str) strstr
@ cdecl -noimport strtol(str ptr long) strtol
@ cdecl -noimport strtoul(str ptr long) strtoul
@ varargs swprintf(wstr wstr) NTDLL_swprintf
@ cdecl -noimport tan(double) tan
@ cdecl tolower(long) tolower
@ cdecl toupper(long) toupper
@ cdecl towlower(long) NTDLL_towlower
@ cdecl towupper(long) NTDLL_towupper
@ cdecl -noimport vsprintf(ptr str ptr) vsprintf
@ cdecl wcscat(wstr wstr) NTDLL_wcscat
@ cdecl wcschr(wstr long) NTDLL_wcschr
@ cdecl wcscmp(wstr wstr) NTDLL_wcscmp
@ cdecl wcscpy(ptr wstr) NTDLL_wcscpy
@ cdecl wcscspn(wstr wstr) NTDLL_wcscspn
@ cdecl wcslen(wstr) NTDLL_wcslen
@ cdecl wcsncat(wstr wstr long) NTDLL_wcsncat
@ cdecl wcsncmp(wstr wstr long) NTDLL_wcsncmp
@ cdecl wcsncpy(ptr wstr long) NTDLL_wcsncpy
@ cdecl wcspbrk(wstr wstr) NTDLL_wcspbrk
@ cdecl wcsrchr(wstr long) NTDLL_wcsrchr
@ cdecl wcsspn(wstr wstr) NTDLL_wcsspn
@ cdecl wcsstr(wstr wstr) NTDLL_wcsstr
@ cdecl wcstok(wstr wstr) NTDLL_wcstok
@ cdecl wcstol(wstr ptr long) NTDLL_wcstol
@ cdecl wcstombs(ptr ptr long) NTDLL_wcstombs
@ cdecl wcstoul(wstr ptr long) NTDLL_wcstoul
@ stub NtAddAtom
@ stub NtDeleteAtom
@ stub NtFindAtom
@ stub NtQueryFullAttributesFile
@ stub NtQueueApcThread
@ stub NtReadFileScatter
@ stub NtSignalAndWaitForSingleObject
@ stub NtWriteFileGather
@ stub RtlAddAtomToAtomTable
@ stub RtlAllocateHandle
@ stub RtlCreateAtomTable
@ stub RtlDeleteAtomFromAtomTable
@ stub RtlFreeHandle
@ stub RtlInitializeHandleTable
@ stub RtlIsValidHandle
@ stub RtlLookupAtomInAtomTable
@ stub RtlQueryAtomInAtomTable
@ stdcall RtlTryEnterCriticalSection(ptr) RtlTryEnterCriticalSection
@ stub RtlEnumerateProperties
@ stub RtlSetPropertyClassId
@ stub RtlSetPropertyNames
@ stub RtlQueryPropertyNames
@ stub RtlFlushPropertySet
@ stub RtlSetProperties
@ stub RtlQueryProperties
@ stub RtlQueryPropertySet
@ stub RtlSetUnicodeCallouts
@ stub RtlPropertySetNameToGuid
@ stub RtlGuidToPropertySetName
@ stub RtlClosePropertySet
@ stub RtlCreatePropertySet
@ stub RtlSetPropertySetClassId
@ stdcall NtPowerInformation(long long long long long) NtPowerInformation
@ stdcall -ret64 VerSetConditionMask(long long long long)VerSetConditionMask

##################
# Wine extensions
#
# All functions must be prefixed with '__wine_' (for internal functions)
# or 'wine_' (for user-visible functions) to avoid namespace conflicts.

# Exception handling
@ cdecl -norelay __wine_exception_handler(ptr ptr ptr ptr) __wine_exception_handler
@ cdecl -norelay __wine_finally_handler(ptr ptr ptr ptr) __wine_finally_handler

# Signals
@ cdecl -norelay __wine_hook_user_signal(ptr) __wine_hook_user_signal
@ cdecl -norelay __wine_send_user_signal(long) __wine_send_user_signal

# Relays
@ cdecl -norelay -i386 __wine_call_from_32_regs() __wine_call_from_32_regs

# Debugging interface
@ cdecl -norelay wine_dbgstr_an(str long) wine_dbgstr_an
@ cdecl -norelay wine_dbgstr_wn(str long) wine_dbgstr_wn
@ cdecl -norelay wine_dbgstr_guid(ptr) wine_dbgstr_guid
@ cdecl -norelay wine_dbgstr_longlong(ptr) wine_dbgstr_longlong
@ cdecl -norelay wine_dbg_vprintf(str ptr) wine_dbg_vprintf
@ cdecl -norelay wine_dbgstack(long) wine_dbgstack
@ varargs wine_dbg_printf(str) wine_dbg_printf
@ varargs wine_dbg_sprintf(str) wine_dbg_sprintf
@ varargs wine_dbg_log(long str str str) wine_dbg_log
@ cdecl -norelay wine_start_memwatch(long ptr long) wine_start_memwatch
@ cdecl -norelay wine_stop_memwatch(long) wine_stop_memwatch

# Profile
@ cdecl -norelay wine_get_timer_ref(str) wine_get_timer_ref
@ cdecl -norelay wine_start_timer(long) wine_start_timer
@ cdecl -norelay wine_stop_timer(long) wine_stop_timer
@ cdecl -norelay wine_increment_counter(long long) wine_increment_counter
@ cdecl -norelay wine_set_timer_flags(long long long) wine_set_timer_flags
@ cdecl -norelay wine_query_counters(ptr long long) wine_query_counters
@ cdecl -norelay wine_query_timers(ptr long long) wine_query_timers

# Command-line
@ cdecl __wine_get_main_args(ptr) __wine_get_main_args
@ cdecl __wine_get_wmain_args(ptr) __wine_get_wmain_args

# Server interface
@ cdecl -norelay wine_server_call(ptr) wine_server_call

# Codepages
@ cdecl __wine_init_codepages(ptr ptr) __wine_init_codepages
