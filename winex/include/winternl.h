/*
 * Internal NT APIs and data structures
 *
 * Copyright (C) the Wine project
 */

#ifndef __WINE_WINTERNAL_H
#define __WINE_WINTERNAL_H

#include "windef.h"
#include "winnt.h"
#include "winreg.h"

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */



/**********************************************************************
 * Fundamental types and data structures
 */

#ifndef WINE_NTSTATUS_DECLARED
#define WINE_NTSTATUS_DECLARED
typedef LONG NTSTATUS;
#endif

typedef CONST char *PCSZ;

typedef short CSHORT;
typedef CSHORT *PCSHORT;

#ifndef __STRING_DEFINED__
#define __STRING_DEFINED__
typedef struct _STRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR Buffer;
} STRING, *PSTRING;
#endif

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;
typedef const STRING *PCANSI_STRING;

typedef STRING OEM_STRING;
typedef PSTRING POEM_STRING;
typedef const STRING *PCOEM_STRING;

#ifndef __UNICODE_STRING_DEFINED__
#define __UNICODE_STRING_DEFINED__
typedef struct _UNICODE_STRING {
  USHORT Length;        /* bytes */
  USHORT MaximumLength; /* bytes */
  PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
#endif

typedef const UNICODE_STRING *PCUNICODE_STRING;


typedef struct _FILE_COMPLETION_INFORMATION {
   HANDLE CompletionPort;
   ULONG_PTR CompletionKey;
} FILE_COMPLETION_INFORMATION, *PFILE_COMPLETION_INFORMATION;


/***********************************************************************
 * PEB data structure
 */

typedef struct tagRTL_BITMAP {
    ULONG  SizeOfBitMap; /* Number of bits in the bitmap */
    LPBYTE BitMapBuffer; /* Bitmap data, assumed sized to a DWORD boundary */
} RTL_BITMAP, *PRTL_BITMAP;

typedef const RTL_BITMAP *PCRTL_BITMAP;

typedef struct _PEB_LDR_DATA
{
    ULONG               Length;
    BOOLEAN             Initialized;
    PVOID               SsHandle;
    LIST_ENTRY          InLoadOrderModuleList;
    LIST_ENTRY          InMemoryOrderModuleList;
    LIST_ENTRY          InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _GDI_TEB_BATCH
{
    ULONG  Offset;
    HANDLE HDC;
    ULONG  Buffer[0x136];
} GDI_TEB_BATCH;

typedef struct RTL_DRIVE_LETTER_CURDIR
{
    USHORT              Flags;
    USHORT              Length;
    ULONG               TimeStamp;
    UNICODE_STRING      DosPath;
} RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

typedef struct _CURDIR
{
    UNICODE_STRING DosPath;
    PVOID Handle;
} CURDIR, *PCURDIR;

typedef struct _RTL_USER_PROCESS_PARAMETERS
{
    ULONG               AllocationSize;
    ULONG               Size;
    ULONG               Flags;
    ULONG               DebugFlags;
    HANDLE              ConsoleHandle;
    ULONG               ConsoleFlags;
    HANDLE              hStdInput;
    HANDLE              hStdOutput;
    HANDLE              hStdError;
    CURDIR              CurrentDirectory;
    UNICODE_STRING      DllPath;
    UNICODE_STRING      ImagePathName;
    UNICODE_STRING      CommandLine;
    PWSTR               Environment;
    ULONG               dwX;
    ULONG               dwY;
    ULONG               dwXSize;
    ULONG               dwYSize;
    ULONG               dwXCountChars;
    ULONG               dwYCountChars;
    ULONG               dwFillAttribute;
    ULONG               dwFlags;
    ULONG               wShowWindow;
    UNICODE_STRING      WindowTitle;
    UNICODE_STRING      Desktop;
    UNICODE_STRING      ShellInfo;
    UNICODE_STRING      RuntimeInfo;
    RTL_DRIVE_LETTER_CURDIR DLCurrentDirectory[0x20];
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef struct _PEB
{
    BOOLEAN                      InheritedAddressSpace;           /* 0x00 */
    BOOLEAN                      ReadImageFileExecOptions;        /* 0x01 */
    BOOLEAN                      BeingDebugged;                   /* 0x02 */
    BOOLEAN                      SpareBool;                       /* 0x03 */
    HANDLE                       Mutant;                          /* 0x04 */
    HMODULE                      ImageBaseAddress;                /* 0x08 */
    PPEB_LDR_DATA                LdrData;                         /* 0x0c */
    RTL_USER_PROCESS_PARAMETERS *ProcessParameters;               /* 0x10 */
    PVOID                        SubSystemData;                   /* 0x14 */
    HANDLE                       ProcessHeap;                     /* 0x18 */
    PRTL_CRITICAL_SECTION        FastPebLock;                     /* 0x1c */
    PVOID                        FastPebLockRoutine;              /* 0x20 */
    PVOID                        FastPebUnlockRoutine;            /* 0x24 */
    ULONG                        EnvironmentUpdateCount;          /* 0x28 */
    PVOID                        KernelCallbackTable;             /* 0x2c */
    PVOID                        EventLogSection;                 /* 0x30 */
    PVOID                        EventLog;                        /* 0x34 */
    PVOID                        FreeList;                        /* 0x38 */
    ULONG                        TlsExpansionCounter;             /* 0x3c */
    PRTL_BITMAP                  TlsBitmap;                       /* 0x40 */
    ULONG                        TlsBitmapBits[2];                /* 0x44 */
    PVOID                        ReadOnlySharedMemoryBase;        /* 0x4c */
    PVOID                        ReadOnlySharedMemoryHeap;        /* 0x50 */
    PVOID                       *ReadOnlyStaticServerData;        /* 0x54 */
    PVOID                        AnsiCodePageData;                /* 0x58 */
    PVOID                        OemCodePageData;                 /* 0x5c */
    PVOID                        UnicodeCaseTableData;            /* 0x60 */
    ULONG                        NumberOfProcessors;              /* 0x64 */
    ULONG                        NtGlobalFlag;                    /* 0x68 */
    BYTE                         Spare2[4];                       /* 0x6c */
    LARGE_INTEGER                CriticalSectionTimeout;          /* 0x70 */
    ULONG                        HeapSegmentReserve;              /* 0x78 */
    ULONG                        HeapSegmentCommit;               /* 0x7c */
    ULONG                        HeapDeCommitTotalFreeThreshold;  /* 0x80 */
    ULONG                        HeapDeCommitFreeBlockThreshold;  /* 0x84 */
    ULONG                        NumberOfHeaps;                   /* 0x88 */
    ULONG                        MaximumNumberOfHeaps;            /* 0x8c */
    PVOID                       *ProcessHeaps;                    /* 0x90 */
    PVOID                        GdiSharedHandleTable;            /* 0x94 */
    PVOID                        ProcessStarterHelper;            /* 0x98 */
    PVOID                        GdiDCAttributeList;              /* 0x9c */
    PVOID                        LoaderLock;                      /* 0xa0 */
    ULONG                        OSMajorVersion;                  /* 0xa4 */
    ULONG                        OSMinorVersion;                  /* 0xa8 */
    ULONG                        OSBuildNumber;                   /* 0xac */
    ULONG                        OSPlatformId;                    /* 0xb0 */
    ULONG                        ImageSubSystem;                  /* 0xb4 */
    ULONG                        ImageSubSystemMajorVersion;      /* 0xb8 */
    ULONG                        ImageSubSystemMinorVersion;      /* 0xbc */
    ULONG                        ImageProcessAffinityMask;        /* 0xc0 */
    ULONG                        GdiHandleBuffer[34];             /* 0xc4 */
    ULONG                        PostProcessInitRoutine;          /* 0x14c */
    PRTL_BITMAP                  TlsExpansionBitmap;              /* 0x150 */
    ULONG                        TlsExpansionBitmapBits[32];      /* 0x154 */
    ULONG                        SessionId;                       /* 0x1d4 */
} PEB, *PPEB;

/***********************************************************************
 * Enums
 */

typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation,
    FileBothDirectoryInformation,
    FileBasicInformation,
    FileStandardInformation,
    FileInternalInformation,
    FileEaInformation,
    FileAccessInformation,
    FileNameInformation,
    FileRenameInformation,
    FileLinkInformation,
    FileNamesInformation,
    FileDispositionInformation,
    FilePositionInformation,
    FileFullEaInformation,
    FileModeInformation,
    FileAlignmentInformation,
    FileAllInformation,
    FileAllocationInformation,
    FileEndOfFileInformation,
    FileAlternateNameInformation,
    FileStreamInformation,
    FilePipeInformation,
    FilePipeLocalInformation,
    FilePipeRemoteInformation,
    FileMailslotQueryInformation,
    FileMailslotSetInformation,
    FileCompressionInformation,
    FileObjectIdInformation,
    FileCompletionInformation,
    FileMoveClusterInformation,
    FileQuotaInformation,
    FileReparsePointInformation,
    FileNetworkOpenInformation,
    FileAttributeTagInformation,
    FileTrackingInformation,
    FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef enum _FSINFOCLASS {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation,
    FileFsSizeInformation,
    FileFsDeviceInformation,
    FileFsAttributeInformation,
    FileFsControlInformation,
    FileFsFullSizeInformation,
    FileFsObjectIdInformation,
    FileFsMaximumInformation
} FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;

typedef enum _KEY_INFORMATION_CLASS {
    KeyBasicInformation,
    KeyNodeInformation,
    KeyFullInformation
} KEY_INFORMATION_CLASS;

typedef enum _KEY_VALUE_INFORMATION_CLASS {
    KeyValueBasicInformation,
    KeyValueFullInformation,
    KeyValuePartialInformation,
    KeyValueFullInformationAlign64,
    KeyValuePartialInformationAlign64
} KEY_VALUE_INFORMATION_CLASS;

typedef enum _OBJECT_INFORMATION_CLASS {
    DunnoTheConstants1 /* FIXME */
} OBJECT_INFORMATION_CLASS, *POBJECT_INFORMATION_CLASS;

typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    ProcessQuotaLimits = 1,
    ProcessIoCounters = 2,
    ProcessVmCounters = 3,
    ProcessTimes = 4,
    ProcessBasePriority = 5,
    ProcessRaisePriority = 6,
    ProcessDebugPort = 7,
    ProcessExceptionPort = 8,
    ProcessAccessToken = 9,
    ProcessLdtInformation = 10,
    ProcessLdtSize = 11,
    ProcessDefaultHardErrorMode = 12,
    ProcessIoPortHandlers = 13,
    ProcessPooledUsageAndLimits = 14,
    ProcessWorkingSetWatch = 15,
    ProcessUserModeIOPL = 16,
    ProcessEnableAlignmentFaultFixup = 17,
    ProcessPriorityClass = 18,
    ProcessWx86Information = 19,
    ProcessHandleCount = 20,
    ProcessAffinityMask = 21,
    ProcessPriorityBoost = 22,
    ProcessDeviceMap = 23,
    ProcessSessionInformation = 24,
    ProcessForegroundInformation = 25,
    ProcessWow64Information = 26,
    ProcessImageFileName = 27,
    ProcessLUIDDeviceMapsEnabled = 28,
    ProcessBreakOnTermination = 29,
    ProcessDebugObjectHandle = 30,
    ProcessDebugFlags = 31,
    ProcessHandleTracing = 32,
    MaxProcessInfoClass
} PROCESSINFOCLASS;

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;

typedef enum SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation = 0,
    Unknown1,
    SystemPerformanceInformation = 2,
    SystemTimeOfDayInformation = 3, /* was SystemTimeInformation */
    Unknown4,
    SystemProcessInformation = 5,
    Unknown6,
    Unknown7,
    SystemProcessorPerformanceInformation = 8,
    Unknown9,
    Unknown10,
    SystemDriverInformation,
    Unknown12,
    Unknown13,
    Unknown14,
    Unknown15,
    SystemHandleList,
    Unknown17,
    Unknown18,
    Unknown19,
    Unknown20,
    SystemCacheInformation,
    Unknown22,
    SystemInterruptInformation = 23,
    SystemExceptionInformation = 33,
    SystemDebuggerInformation = 35,
    SystemRegistryQuotaInformation = 37,
    SystemLookasideInformation = 45
} SYSTEM_INFORMATION_CLASS, *PSYSTEM_INFORMATION_CLASS;

typedef enum _TIMER_TYPE {
    NotificationTimer,
    SynchronizationTimer
} TIMER_TYPE;

typedef enum _THREADINFOCLASS {
    ThreadBasicInformation,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,
    ThreadIsIoPending,
    MaxThreadInfoClass
} THREADINFOCLASS;

typedef struct _CLIENT_ID
{
   HANDLE UniqueProcess;
   HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

/* struct THREAD_BASIC_INFORMATION: return struct for the ThreadBasicInformation thread info class
   in the call to the NtQueryInformationThread() function. */
typedef struct _THREAD_BASIC_INFORMATION {
    NTSTATUS  ExitStatus;
    PVOID     TebBaseAddress;
    CLIENT_ID ClientId;
    ULONG_PTR AffinityMask;
    LONG      Priority;
    LONG      BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;


typedef enum _WINSTATIONINFOCLASS {
    WinStationInformation = 8
} WINSTATIONINFOCLASS;

#define IRP_MJ_CREATE 0
#define IRP_MJ_CREATE_NAMED_PIPE 1
#define IRP_MJ_CLOSE 2
#define IRP_MJ_READ 3
#define IRP_MJ_WRITE 4
#define IRP_MJ_QUERY_INFORMATION 5
#define IRP_MJ_SET_INFORMATION 6
#define IRP_MJ_QUERY_EA 7
#define IRP_MJ_SET_EA 8
#define IRP_MJ_FLUSH_BUFFERS 9
#define IRP_MJ_QUERY_VOLUME_INFORMATION 10
#define IRP_MJ_SET_VOLUME_INFORMATION 11
#define IRP_MJ_DIRECTORY_CONTROL 12
#define IRP_MJ_FILE_SYSTEM_CONTROL 13
#define IRP_MJ_DEVICE_CONTROL 14
#define IRP_MJ_INTERNAL_DEVICE_CONTROL 15
#define IRP_MJ_SHUTDOWN 16
#define IRP_MJ_LOCK_CONTROL 17
#define IRP_MJ_CLEANUP 18
#define IRP_MJ_CREATE_MAILSLOT 19
#define IRP_MJ_QUERY_SECURITY 20
#define IRP_MJ_SET_SECURITY 21
#define IRP_MJ_POWER 22
#define IRP_MJ_SYSTEM_CONTROL 23
#define IRP_MJ_DEVICE_CHANGE 24
#define IRP_MJ_QUERY_QUOTA 25
#define IRP_MJ_SET_QUOTA 26
#define IRP_MJ_PNP 27
#define IRP_MJ_MAXIMUM_FUNCTION 27
#define IRP_MJ_FUNCTIONS (IRP_MJ_MAXIMUM_FUNCTION+1)

/***********************************************************************
 * IA64 specific types and data structures
 */

#ifdef __ia64__

typedef struct _FRAME_POINTERS {
  ULONGLONG MemoryStackFp;
  ULONGLONG BackingStoreFp;
} FRAME_POINTERS, *PFRAME_POINTERS;

#define UNWIND_HISTORY_TABLE_SIZE 12

typedef struct _RUNTIME_FUNCTION {
  ULONG BeginAddress;
  ULONG EndAddress;
  ULONG UnwindInfoAddress;
} RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

typedef struct _UNWIND_HISTORY_TABLE_ENTRY {
  ULONG64 ImageBase;
  ULONG64 Gp;
  PRUNTIME_FUNCTION FunctionEntry;
} UNWIND_HISTORY_TABLE_ENTRY, *PUNWIND_HISTORY_TABLE_ENTRY;

typedef struct _UNWIND_HISTORY_TABLE {
  ULONG Count;
  UCHAR Search;
  ULONG64 LowAddress;
  ULONG64 HighAddress;
  UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
} UNWIND_HISTORY_TABLE, *PUNWIND_HISTORY_TABLE;

#endif /* defined(__ia64__) */

/***********************************************************************
 * LUID defines
 */
#define SE_MIN_WELL_KNOWN_PRIVILEGE       0x2
#define SE_CREATE_TOKEN_PRIVILEGE         0x2
#define SE_ASSIGNPRIMARYTOKEN_PRIVILEGE   0x3
#define SE_LOCK_MEMORY_PRIVILEGE          0x4
#define SE_INCREASE_QUOTA_PRIVILEGE       0x5
#define SE_MACHINE_ACCOUNT_PRIVILEGE      0x6
#define SE_TCB_PRIVILEGE                  0x7
#define SE_SECURITY_PRIVILEGE             0x8
#define SE_TAKE_OWNERSHIP_PRIVILEGE       0x9
#define SE_LOAD_DRIVER_PRIVILEGE          0xa
#define SE_SYSTEM_PROFILE_PRIVILEGE       0xb
#define SE_SYSTEMTIME_PRIVILEGE           0xc
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE  0xd
#define SE_INC_BASE_PRIORITY_PRIVILEGE    0xe
#define SE_CREATE_PAGEFILE_PRIVILEGE      0xf
#define SE_CREATE_PERMANENT_PRIVILEGE     0x10
#define SE_BACKUP_PRIVILEGE               0x11
#define SE_RESTORE_PRIVILEGE              0x12
#define SE_SHUTDOWN_PRIVILEGE             0x13
#define SE_DEBUG_PRIVILEGE                0x14
#define SE_AUDIT_PRIVILEGE                0x15
#define SE_SYSTEM_ENVIRONMENT_PRIVILEGE   0x16
#define SE_CHANGE_NOTIFY_PRIVILEGE        0x17
#define SE_REMOTE_SHUTDOWN_PRIVILEGE      0x18
#define SE_UNDOCK_PRIVILEGE               0x19
#define SE_SYNC_AGENT_PRIVILEGE           0x1a
#define SE_ENABLE_DELEGATION_PRIVILEGE    0x1b
#define SE_MANAGE_VOLUME_PRIVILEGE        0x1c
#define SE_IMPERSONATE_PRIVILEGE          0x1d
#define SE_CREATE_GLOBAL_PRIVILEGE        0x1e
#define SE_MAX_WELL_KNOWN_PRIVILEGE       SE_CREATE_GLOBAL_PRIVILEGE


/***********************************************************************
 * Types and data structures
 */

/* This is used by NtQuerySystemInformation */
typedef struct _SYSTEM_THREAD_INFORMATION{
    FILETIME    ftKernelTime;
    FILETIME    ftUserTime;
    FILETIME    ftCreateTime;
    DWORD       dwTickCount;
    DWORD       dwStartAddress;
    DWORD       dwOwningPID;
    DWORD       dwThreadID;
    DWORD       dwCurrentPriority;
    DWORD       dwBasePriority;
    DWORD       dwContextSwitches;
    DWORD       dwThreadState;
    DWORD       dwWaitReason;
    DWORD       dwUnknown;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;


typedef struct _VPB* PVPB;
#define DEVICE_TYPE DWORD
typedef PVOID PDRIVER_CONTROL; /* XXX some function pointer */
typedef UINT_PTR KSPIN_LOCK;

typedef struct _KDEVICE_QUEUE_ENTRY
{
    LIST_ENTRY DeviceListEntry;
    DWORD SortKey;
    BOOLEAN Inserted;
} KDEVICE_QUEUE_ENTRY, *PKDEVICE_QUEUE_ENTRY;

typedef struct _KDEVICE_QUEUE
{
    SHORT Type;
    SHORT Size;
    LIST_ENTRY DeviceListHead;
    KSPIN_LOCK Lock;
    BOOLEAN Busy;
} KDEVICE_QUEUE, *PKDEVICE_QUEUE;

typedef PVOID PKDEFERRED_ROUTINE; /* XXX function pointer */

typedef struct _KDPC
{
    SHORT Type;
    BYTE Number;
    BYTE Importance;
    LIST_ENTRY DpcListEntry;
    PKDEFERRED_ROUTINE DeferredRoutine;
    PVOID DeferredContext;
    PVOID SystemArgument1;
    PVOID SystemArgument2;
    PDWORD_PTR Lock;
} KDPC, *PKDPC;

typedef struct _DISPATCHER_HEADER
{
    BYTE Type;
    BYTE Absolute;
    BYTE Size;
    BYTE Inserted;
    BYTE SignalState;
    LIST_ENTRY WaitListHead;
} DISPATCHER_HEADER, *PDISPATCHER_HEADER;

typedef struct _KEVENT
{
    DISPATCHER_HEADER Header;
} KEVENT, *PKEVENT;

typedef struct _WAIT_CONTEXT_BLOCK
{
    KDEVICE_QUEUE_ENTRY WaitQueueEntry;
    PDRIVER_CONTROL DeviceRoutine;
    PVOID DeviceContext;
    DWORD NumberOfMapRegisters;
    PVOID DeviceObject;
    PVOID CurrentIrp;
    PKDPC BufferChainingDpc;
} WAIT_CONTEXT_BLOCK, *PWAIT_CONTEXT_BLOCK;

typedef struct _DEVICE_OBJECT
{
    SHORT Type;
    WORD Size;
    LONG ReferenceCount;
    struct _DRIVER_OBJECT* DriverObject;
    struct _DRIVER_OBJECT* NextDevice;
    struct _DRIVER_OBJECT* AttachedDevice;
    struct _IRP* CurrentIrp;
    struct _PIO_TIMER* Timer;
    DWORD Flags;
    DWORD Characteristics;
    PVPB Vpb;
    PVOID DeviceExtension;
    DEVICE_TYPE DeviceType;
    CHAR StackSize;
    union
    {
        LIST_ENTRY ListEntry;
        WAIT_CONTEXT_BLOCK Wcb;
    } Queue;
    DWORD AlignmentRequirement;
    KDEVICE_QUEUE DeviceQueue;
    KDPC Dpc;
    DWORD ActiveThreadCount;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    KEVENT DeviceLock;
    WORD SectorSize;
    WORD Spare1;
    struct _DEVOBJ_EXTENSION* DeviceObjectExtension;
    PVOID Reserved;
} DEVICE_OBJECT, *PDEVICE_OBJECT;

typedef struct _DRIVER_EXTENSION* PDRIVER_EXTENSION;
typedef PVOID PFAST_IO_DISPATCH; /* XXX function pointer */
typedef PVOID PDRIVER_INITIALIZE; /* XXX function pointer */
typedef PVOID PDRIVER_STARTIO; /* XXX function pointer */
typedef PVOID PDRIVER_UNLOAD; /* XXX function pointer */
typedef PVOID PDRIVER_DISPATCH; /* XXX function pointer */

typedef struct _DRIVER_OBJECT
{
    SHORT Type;
    SHORT Size;
    PDEVICE_OBJECT DeviceObject;
    DWORD Flags;
    PVOID DriverStart;
    DWORD DriverSize;
    PVOID DriverSection;
    PDRIVER_EXTENSION DriverExtension;
    UNICODE_STRING DriverName;
    PUNICODE_STRING HardwareDatabase;
    PFAST_IO_DISPATCH FastIoDispatch;
    PDRIVER_INITIALIZE DriverInit;
    PDRIVER_STARTIO DriverStartIo;
    PDRIVER_UNLOAD DriverUnload;
    PDRIVER_DISPATCH MajorFunction[IRP_MJ_FUNCTIONS];
} DRIVER_OBJECT, *PDRIVER_OBJECT;

typedef struct _SYSTEM_MODULE {
    DWORD Reserved1;
    DWORD dwUnknown;
    PVOID BaseAddress;
    DWORD Size;
    DWORD Flags;
    WORD Id;
    WORD Rank;
    WORD LoadCount;
    WORD NameOffset;
    CHAR Name[256];
} SYSTEM_MODULE;

typedef struct _SYSTEM_MODULE_INFORMATION
{
    DWORD Count;
    SYSTEM_MODULE Modules[1];
} SYSTEM_MODULE_INFORMATION;

/* This is used by NtQuerySystemInformation */
/* FIXME: Isn't THREAD_INFO and THREADINFO the same structure? */
typedef struct {
    FILETIME ftCreationTime;
    DWORD dwUnknown1;
    DWORD dwStartAddress;
    DWORD dwOwningPID;
    DWORD dwThreadID;
    DWORD dwCurrentPriority;
    DWORD dwBasePriority;
    DWORD dwContextSwitches;
    DWORD dwThreadState;
    DWORD dwWaitReason;
    DWORD dwUnknown2[5];
} THREADINFO, *PTHREADINFO;

/* FIXME: Isn't THREAD_INFO and THREADINFO the same structure? */
typedef struct _THREAD_INFO{
    DWORD Unknown1[6];
    DWORD ThreadID;
    DWORD Unknown2[3];
    DWORD Status;
    DWORD WaitReason;
    DWORD Unknown3[4];
} THREAD_INFO, PTHREAD_INFO;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } DUMMYUNIONNAME;

  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef void (WINAPI * PIO_APC_ROUTINE)(PVOID,PIO_STATUS_BLOCK,ULONG);

typedef struct _KEY_BASIC_INFORMATION {
    LARGE_INTEGER LastWriteTime;
    ULONG         TitleIndex;
    ULONG         NameLength;
    WCHAR         Name[1];
} KEY_BASIC_INFORMATION, *PKEY_BASIC_INFORMATION;

typedef struct _KEY_NODE_INFORMATION
{
    LARGE_INTEGER LastWriteTime;
    ULONG         TitleIndex;
    ULONG         ClassOffset;
    ULONG         ClassLength;
    ULONG         NameLength;
    WCHAR         Name[1];
   /* Class[1]; */
} KEY_NODE_INFORMATION, *PKEY_NODE_INFORMATION;

typedef struct _KEY_FULL_INFORMATION
{
    LARGE_INTEGER LastWriteTime;
    ULONG         TitleIndex;
    ULONG         ClassOffset;
    ULONG         ClassLength;
    ULONG         SubKeys;
    ULONG         MaxNameLen;
    ULONG         MaxClassLen;
    ULONG         Values;
    ULONG         MaxValueNameLen;
    ULONG         MaxValueDataLen;
    WCHAR         Class[1];
} KEY_FULL_INFORMATION, *PKEY_FULL_INFORMATION;

typedef struct _KEY_VALUE_ENTRY
{
    PUNICODE_STRING ValueName;
    ULONG           DataLength;
    ULONG           DataOffset;
    ULONG           Type;
} KEY_VALUE_ENTRY, *PKEY_VALUE_ENTRY;

typedef struct _KEY_VALUE_BASIC_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_BASIC_INFORMATION, *PKEY_VALUE_BASIC_INFORMATION;

typedef struct _KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;
    ULONG DataLength;
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataLength;
    UCHAR Data[1];
} KEY_VALUE_PARTIAL_INFORMATION, *PKEY_VALUE_PARTIAL_INFORMATION;

#ifndef __OBJECT_ATTRIBUTES_DEFINED__
#define __OBJECT_ATTRIBUTES_DEFINED__
typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;       /* type SECURITY_DESCRIPTOR */
  PVOID SecurityQualityOfService; /* type SECURITY_QUALITY_OF_SERVICE */
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
#endif

typedef struct _PROCESS_BASIC_INFORMATION {
#ifdef __WINE__
    DWORD ExitStatus;
    DWORD PebBaseAddress;
    DWORD AffinityMask;
    DWORD BasePriority;
    ULONG UniqueProcessId;
    ULONG InheritedFromUniqueProcessId;
#else
    PVOID Reserved1;
    PPEB PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
#endif
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

typedef struct _PROCESS_INFO {
    DWORD    Offset;             /* 00 offset to next PROCESS_INFO ok*/
    DWORD    ThreadCount;        /* 04 number of ThreadInfo member ok */
    DWORD    Unknown1[6];
    FILETIME CreationTime;       /* 20 */
    DWORD    Unknown2[5];
    PWCHAR   ProcessName;        /* 3c ok */
    DWORD    BasePriority;
    DWORD    ProcessID;          /* 44 ok*/
    DWORD    ParentProcessID;
    DWORD    HandleCount;
    DWORD    Unknown3[2];        /* 50 */
    ULONG    PeakVirtualSize;
    ULONG    VirtualSize;
    ULONG    PageFaultCount;
    ULONG    PeakWorkingSetSize;
    ULONG    WorkingSetSize;
    ULONG    QuotaPeakPagedPoolUsage;
    ULONG    QuotaPagedPoolUsage;
    ULONG    QuotaPeakNonPagedPoolUsage;
    ULONG    QuotaNonPagedPoolUsage;
    ULONG    PagefileUsage;
    ULONG    PeakPagefileUsage;
    DWORD    PrivateBytes;
    DWORD    Unknown6[4];
    THREAD_INFO ati[ANYSIZE_ARRAY]; /* 94 size=0x40*/
} PROCESS_INFO, PPROCESS_INFO;

typedef struct _RTL_HEAP_DEFINITION {
    ULONG Length; /* = sizeof(RTL_HEAP_DEFINITION) */

    ULONG Unknown[11];
} RTL_HEAP_DEFINITION, *PRTL_HEAP_DEFINITION;

typedef struct tagRTL_BITMAP_RUN {
    ULONG StartOfRun; /* Bit position at which run starts - FIXME: Name? */
    ULONG SizeOfRun;  /* Size of the run in bits - FIXME: Name? */
} RTL_BITMAP_RUN, *PRTL_BITMAP_RUN;

typedef const RTL_BITMAP_RUN *PCRTL_BITMAP_RUN;

typedef NTSTATUS WINAPI (*PRTL_QUERY_REGISTRY_ROUTINE)(
        IN LPWSTR ValueName,
        IN ULONG ValueType,
        IN PVOID ValueData,
        IN ULONG ValueLength,
        IN PVOID Context,
        IN PVOID EntryContext);

typedef struct _RTL_QUERY_REGISTRY_TABLE
{
    PRTL_QUERY_REGISTRY_ROUTINE QueryRoutine;
    ULONG Flags;
    LPWSTR Name;
    PVOID EntryContext;
    ULONG DefaultType;
    PVOID DefaultData;
    ULONG DefaultLength;
} RTL_QUERY_REGISTRY_TABLE, *PRTL_QUERY_REGISTRY_TABLE;

typedef struct _RTL_RWLOCK {
    RTL_CRITICAL_SECTION rtlCS;

    HANDLE hSharedReleaseSemaphore;
    UINT   uSharedWaiters;

    HANDLE hExclusiveReleaseSemaphore;
    UINT   uExclusiveWaiters;

    INT    iNumberActive;
    HANDLE hOwningThreadId;
    DWORD  dwTimeoutBoost;
    PVOID  pDebugInfo;
} RTL_RWLOCK, *LPRTL_RWLOCK;

/* System Information Class 0x00 */
typedef struct _SYSTEM_BASIC_INFORMATION {
#ifdef __WINE__
    DWORD dwUnknown1;
    ULONG uKeMaximumIncrement;
    ULONG uPageSize;
    ULONG uMmNumberOfPhysicalPages;
    ULONG uMmLowestPhysicalPage;
    ULONG uMmHighestPhysicalPage;
    ULONG uAllocationGranularity;
    PVOID pLowestUserAddress;
    PVOID pMmHighestUserAddress;
    ULONG uKeActiveProcessors;
    BYTE bKeNumberProcessors;
    BYTE bUnknown2;
    WORD wUnknown3;
#else
    BYTE Reserved1[24];
    PVOID Reserved2[4];
    CCHAR NumberOfProcessors;
#endif
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

/* System Information Class 0x15 */
typedef struct {
    ULONG CurrentSize;
    ULONG PeakSize;
    ULONG PageFaultCount;
    ULONG MinimumWorkingSet;
    ULONG MaximumWorkingSet;
    ULONG unused[4];
} SYSTEM_CACHE_INFORMATION;

typedef struct _SYSTEM_CONFIGURATION_INFO {
    union {
        ULONG	OemId;
        struct {
	    WORD ProcessorArchitecture;
	    WORD Reserved;
	} tag1;
    } tag2;
    ULONG PageSize;
    PVOID MinimumApplicationAddress;
    PVOID MaximumApplicationAddress;
    ULONG ActiveProcessorMask;
    ULONG NumberOfProcessors;
    ULONG ProcessorType;
    ULONG AllocationGranularity;
    WORD  ProcessorLevel;
    WORD  ProcessorRevision;
} SYSTEM_CONFIGURATION_INFO, *PSYSTEM_CONFIGURATION_INFO;

/* System Information Class 0x0b */
typedef struct {
    PVOID pvAddress;
    DWORD dwUnknown1;
    DWORD dwUnknown2;
    DWORD dwEntryIndex;
    DWORD dwUnknown3;
    char szName[MAX_PATH + 1];
} SYSTEM_DRIVER_INFORMATION;

typedef struct _SYSTEM_EXCEPTION_INFORMATION {
    BYTE Reserved1[16];
} SYSTEM_EXCEPTION_INFORMATION, *PSYSTEM_EXCEPTION_INFORMATION;

typedef struct _SYSTEM_LOOKASIDE_INFORMATION {
    BYTE Reserved1[32];
} SYSTEM_LOOKASIDE_INFORMATION, *PSYSTEM_LOOKASIDE_INFORMATION;

typedef struct _SYSTEM_INTERRUPT_INFORMATION {
    BYTE Reserved1[24];
} SYSTEM_INTERRUPT_INFORMATION, *PSYSTEM_INTERRUPT_INFORMATION;

/* System Information Class 0x10 */
typedef struct {
    USHORT dwPID;
    USHORT dwCreatorBackTraceIndex;
    BYTE bObjectType;
    BYTE bHandleAttributes;
    USHORT usHandleOffset;
    DWORD dwKeObject;
    ULONG ulGrantedAccess;
} HANDLEINFO, *PHANDLEINFO; /* FIXME: SYSTEM_HANDLE_INFORMATION? */

typedef struct _SYSTEM_PERFORMANCE_INFORMATION {
    BYTE Reserved1[312];
} SYSTEM_PERFORMANCE_INFORMATION, *PSYSTEM_PERFORMANCE_INFORMATION;

/* System Information Class 0x02 */
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
#ifdef __WINE__
    LARGE_INTEGER liIdleTime;
    DWORD dwSpare[76];
#else
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER Reserved1[2];
    ULONG Reserved2;
#endif
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef struct _VM_COUNTERS_ {
    ULONG PeakVirtualSize;
    ULONG VirtualSize;
    ULONG PageFaultCount;
    ULONG PeakWorkingSetSize;
    ULONG WorkingSetSize;
    ULONG QuotaPeakPagedPoolUsage;
    ULONG QuotaPagedPoolUsage;
    ULONG QuotaPeakNonPagedPoolUsage;
    ULONG QuotaNonPagedPoolUsage;
    ULONG PagefileUsage;
    ULONG PeakPagefileUsage;
} VM_COUNTERS, *PVM_COUNTERS;

/* System Information Class 0x05 */
typedef struct _SYSTEM_PROCESS_INFORMATION {
#ifdef __WINE__
    DWORD dwOffset;
    DWORD dwThreadCount;
    DWORD dwUnknown1[6];
    FILETIME ftCreationTime;
    FILETIME ftUserTime;
    FILETIME ftKernelTime;
    UNICODE_STRING ProcessName;
    DWORD dwBasePriority;
    DWORD dwProcessID;
    DWORD dwParentProcessID;
    DWORD dwHandleCount;
    DWORD dwUnknown3;
    DWORD dwUnknown4;
    VM_COUNTERS vmCounters;
    IO_COUNTERS ioCounters;
    SYSTEM_THREAD_INFORMATION ti[1];
#else
    ULONG NextEntryOffset;
    BYTE Reserved1[52];
    PVOID Reserved2[3];
    HANDLE UniqueProcessId;
    PVOID Reserved3;
    ULONG HandleCount;
    BYTE Reserved4[4];
    PVOID Reserved5[11];
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER Reserved6[6];
#endif
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef struct _SYSTEM_REGISTRY_QUOTA_INFORMATION {
    ULONG RegistryQuotaAllowed;
    ULONG RegistryQuotaUsed;
    PVOID Reserved1;
} SYSTEM_REGISTRY_QUOTA_INFORMATION, *PSYSTEM_REGISTRY_QUOTA_INFORMATION;

typedef struct _SYSTEM_TIME_ADJUSTMENT {
    ULONG   TimeAdjustment;
    BOOLEAN TimeAdjustmentDisabled;
} SYSTEM_TIME_ADJUSTMENT, *PSYSTEM_TIME_ADJUSTMENT;

/* System Information Class 0x03 */
typedef struct _SYSTEM_TIMEOFDAY_INFORMATION {
#ifdef __WINE__
    LARGE_INTEGER liKeBootTime;
    LARGE_INTEGER liKeSystemTime;
    LARGE_INTEGER liExpTimeZoneBias;
    ULONG uCurrentTimeZoneId;
    DWORD dwReserved;
#else
    BYTE Reserved1[48];
#endif
} SYSTEM_TIMEOFDAY_INFORMATION, *PSYSTEM_TIMEOFDAY_INFORMATION; /* was SYSTEM_TIME_INFORMATION */

typedef struct _TIME_FIELDS
{   CSHORT Year;
    CSHORT Month;
    CSHORT Day;
    CSHORT Hour;
    CSHORT Minute;
    CSHORT Second;
    CSHORT Milliseconds;
    CSHORT Weekday;
} TIME_FIELDS, *PTIME_FIELDS;

typedef struct _WINSTATIONINFORMATIONW {
  BYTE Reserved2[70];
  ULONG LogonId;
  BYTE Reserved3[1140];
} WINSTATIONINFORMATIONW, *PWINSTATIONINFORMATIONW;


typedef BOOLEAN (WINAPI * PWINSTATIONQUERYINFORMATIONW)(HANDLE,ULONG,WINSTATIONINFOCLASS,PVOID,ULONG,PULONG);

/***********************************************************************
 * Defines
 */

/* flags for NtCreateFile and NtOpenFile */
#define FILE_DIRECTORY_FLAG  0x00000001
#define FILE_WRITE_THROUGH   0x00000002
#define FILE_SEQUENTIAL_ONLY 0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING 0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT    0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
#define FILE_NON_DIRECTORY_FILE      0x00000040
#define FILE_CREATE_TREE_CONNECTION  0x00000080

/* status for NtCreateFile or NtOpenFile */
#define FILE_SUPERSEDED  0x00000000
#define FILE_OPENED      0x00000001
#define FILE_CREATED     0x00000002
#define FILE_OVERWRITTEN 0x00000003
#define FILE_EXISTS      0x00000004
#define FILE_DOES_NOT_EXIST 0x00000005

#if (_WIN32_WINNT >= 0x0501)
#define INTERNAL_TS_ACTIVE_CONSOLE_ID ( *((volatile ULONG*)(0x7ffe02d8)) )
#endif /* (_WIN32_WINNT >= 0x0501) */

#define LOGONID_CURRENT    ((ULONG)-1)

#define OBJ_INHERIT          0x00000002L
#define OBJ_PERMANENT        0x00000010L
#define OBJ_EXCLUSIVE        0x00000020L
#define OBJ_CASE_INSENSITIVE 0x00000040L
#define OBJ_OPENIF           0x00000080L
#define OBJ_OPENLINK         0x00000100L
#define OBJ_KERNEL_HANDLE    0x00000200L
#define OBJ_VALID_ATTRIBUTES 0x000003F2L

#define SERVERNAME_CURRENT ((HANDLE)NULL)

/* For the Flags field of RTL_QUERY_REGISTRY_TABLE.
 * All of these are guesses. */
#define RTL_QUERY_REGISTRY_SUBKEY        1
#define RTL_QUERY_REGISTRY_TOPKEY        2
#define RTL_QUERY_REGISTRY_REQUIRED      4
#define RTL_QUERY_REGISTRY_NOVALUE       8
#define RTL_QUERY_REGISTRY_NOEXPAND     16
#define RTL_QUERY_REGISTRY_DIRECT       32
#define RTL_QUERY_REGISTRY_DELETE       64

/* For the RelativeTo parameter of RtlQueryRegistryValues.
 * All except RTL_REGISTRY_WINDOWS_NT are guesses, but the documentation
 * I have lists them in this order, so this seems sensible. */
#define RTL_REGISTRY_ABSOLUTE   0
#define RTL_REGISTRY_SERVICES   1
#define RTL_REGISTRY_CONTROL    2
#define RTL_REGISTRY_WINDOWS_NT 3
#define RTL_REGISTRY_DEVICEMAP  4
#define RTL_REGISTRY_USER       5

#define RTL_REGISTRY_OPTIONAL   0x80000000
#define RTL_REGISTRY_HANDLE     0x40000000

/***********************************************************************
 * Function declarations
 */

extern LPSTR _strlwr(LPSTR str); /* FIXME: Doesn't belong here */
extern LPSTR _strupr(LPSTR str); /* FIXME: Doesn't belong here */

#if defined(__i386__) && defined(__GNUC__)
static inline void WINAPI DbgBreakPoint(void) { __asm__ __volatile__("int3"); }
static inline void WINAPI DbgUserBreakPoint(void) { __asm__ __volatile__("int3"); }
#else  /* __i386__ && __GNUC__ */
void WINAPI DbgBreakPoint(void);
void WINAPI DbgUserBreakPoint(void);
#endif  /* __i386__ && __GNUC__ */
void WINAPIV DbgPrint(LPCSTR fmt, ...);

NTSTATUS  WINAPI NtAccessCheck(PSECURITY_DESCRIPTOR,HANDLE,ACCESS_MASK,PGENERIC_MAPPING,PPRIVILEGE_SET,PULONG,PULONG,PBOOLEAN);
NTSTATUS  WINAPI NtAdjustPrivilegesToken(HANDLE,BOOLEAN,PTOKEN_PRIVILEGES,DWORD,PTOKEN_PRIVILEGES,PDWORD);
NTSTATUS  WINAPI NtClearEvent(HANDLE);
NTSTATUS  WINAPI NtClose(HANDLE);
NTSTATUS  WINAPI NtCreateEvent(PHANDLE,ACCESS_MASK,const OBJECT_ATTRIBUTES *,BOOLEAN,BOOLEAN);
NTSTATUS  WINAPI NtCreateFile(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,PLARGE_INTEGER,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG);
NTSTATUS  WINAPI NtCreateIoCompletion(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,
                                      ULONG);
NTSTATUS  WINAPI NtCreateKey(PHANDLE,ACCESS_MASK,const OBJECT_ATTRIBUTES*,ULONG,const UNICODE_STRING*,ULONG,PULONG);
NTSTATUS  WINAPI NtCreateSemaphore(PHANDLE,ACCESS_MASK,const OBJECT_ATTRIBUTES*,ULONG,ULONG);
NTSTATUS  WINAPI NtDeleteKey(HANDLE);
NTSTATUS  WINAPI NtDeleteValueKey(HANDLE,const UNICODE_STRING *);
NTSTATUS  WINAPI NtDeviceIoControlFile(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,PIO_STATUS_BLOCK,ULONG,PVOID,ULONG,PVOID,ULONG);
NTSTATUS  WINAPI NtEnumerateKey(HANDLE,ULONG,KEY_INFORMATION_CLASS,void *,DWORD,DWORD *);
NTSTATUS  WINAPI NtEnumerateValueKey(HANDLE,ULONG,KEY_VALUE_INFORMATION_CLASS,PVOID,ULONG,PULONG);
NTSTATUS  WINAPI NtFlushKey(HANDLE);
ULONG     WINAPI NtGetTickCount(VOID);
ULONGLONG WINAPI NtGetTickCount64(VOID);
NTSTATUS  WINAPI NtLoadKey(const OBJECT_ATTRIBUTES *,const OBJECT_ATTRIBUTES *);
NTSTATUS  WINAPI NtNotifyChangeKey(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,PIO_STATUS_BLOCK,ULONG,BOOLEAN,PVOID,ULONG,BOOLEAN);
NTSTATUS  WINAPI NtOpenEvent(PHANDLE,ACCESS_MASK,const OBJECT_ATTRIBUTES *);
NTSTATUS  WINAPI NtOpenFile(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,ULONG,ULONG);
NTSTATUS  WINAPI NtOpenKey(PHANDLE,ACCESS_MASK,const OBJECT_ATTRIBUTES *);
NTSTATUS  WINAPI NtOpenProcessToken(HANDLE,DWORD,HANDLE *);
NTSTATUS  WINAPI NtOpenThreadToken(HANDLE,DWORD,BOOLEAN,HANDLE *);
NTSTATUS  WINAPI NtPowerInformation(POWER_INFORMATION_LEVEL,PVOID,ULONG,PVOID,ULONG);
NTSTATUS  WINAPI NtPulseEvent(HANDLE,PULONG);
NTSTATUS  WINAPI NtQueryInformationProcess(HANDLE,PROCESSINFOCLASS,PVOID,ULONG,PULONG);
NTSTATUS  WINAPI NtQueryInformationThread(HANDLE,THREADINFOCLASS,PVOID,ULONG,PULONG);
NTSTATUS  WINAPI NtQueryInformationToken(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD);
NTSTATUS  WINAPI NtQueryKey(HANDLE,KEY_INFORMATION_CLASS,void *,DWORD,DWORD *);
NTSTATUS  WINAPI NtQueryMultipleValueKey(HANDLE,PVALENTW,ULONG,PVOID,ULONG,PULONG);
NTSTATUS  WINAPI NtQueryPerformanceCounter(PLARGE_INTEGER,PLARGE_INTEGER);
NTSTATUS  WINAPI NtQuerySecurityObject(HANDLE,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR,ULONG,PULONG);
NTSTATUS  WINAPI NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS,PVOID,ULONG,PULONG);
NTSTATUS  WINAPI NtQuerySystemTime(PLARGE_INTEGER);
NTSTATUS  WINAPI NtQueryValueKey(HANDLE,const UNICODE_STRING *,KEY_VALUE_INFORMATION_CLASS,void *,DWORD,DWORD *);
void      WINAPI NtRaiseException(PEXCEPTION_RECORD,PCONTEXT,BOOL);
NTSTATUS  WINAPI NtReadFile(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,PIO_STATUS_BLOCK,PVOID,ULONG,PLARGE_INTEGER,PULONG);
NTSTATUS  WINAPI NtReleaseSemaphore(HANDLE,ULONG,PULONG);
NTSTATUS  WINAPI NtRemoveIoCompletion(HANDLE,PULONG_PTR,LPOVERLAPPED *,
                                      PIO_STATUS_BLOCK,PLARGE_INTEGER);
NTSTATUS  WINAPI NtResetEvent(HANDLE,PULONG);
NTSTATUS  WINAPI NtRestoreKey(HANDLE,HANDLE,ULONG);
NTSTATUS  WINAPI NtReplaceKey(POBJECT_ATTRIBUTES,HANDLE,POBJECT_ATTRIBUTES);
NTSTATUS  WINAPI NtSaveKey(HANDLE,HANDLE);
NTSTATUS  WINAPI NtSetEvent(HANDLE,PULONG);
NTSTATUS  WINAPI NtSetInformationKey(HANDLE,const int,PVOID,ULONG);
NTSTATUS  WINAPI NtSetInformationFile(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS);
NTSTATUS  WINAPI NtSetIoCompletion(HANDLE,ULONG_PTR,LPOVERLAPPED,ULONG,ULONG);
NTSTATUS  WINAPI NtSetSecurityObject(HANDLE,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR);
NTSTATUS  WINAPI NtSetValueKey(HANDLE,const UNICODE_STRING *,ULONG,ULONG,const void *,ULONG);
NTSTATUS  WINAPI NtTerminateProcess(HANDLE,LONG);
NTSTATUS  WINAPI NtTerminateThread(HANDLE,LONG);
NTSTATUS  WINAPI NtUnloadKey(HANDLE);
NTSTATUS  WINAPI NtWaitForSingleObject(HANDLE,BOOLEAN,PLARGE_INTEGER);
NTSTATUS  WINAPI NtYieldExecution(void);

void      WINAPI RtlAcquirePebLock(void);
BYTE      WINAPI RtlAcquireResourceExclusive(LPRTL_RWLOCK,BYTE);
BYTE      WINAPI RtlAcquireResourceShared(LPRTL_RWLOCK,BYTE);
NTSTATUS  WINAPI RtlAddAce(PACL,DWORD,DWORD,PACE_HEADER,DWORD);
NTSTATUS  WINAPI RtlAddAccessAllowedAce(PACL,DWORD,DWORD,PSID);
NTSTATUS  WINAPI RtlAddAccessAllowedAceEx(PACL,DWORD,DWORD,DWORD,PSID);
NTSTATUS  WINAPI RtlAddAccessDeniedAce(PACL,DWORD,DWORD,PSID);
DWORD     WINAPI RtlAdjustPrivilege(DWORD,DWORD,DWORD,DWORD);
BOOLEAN   WINAPI RtlAllocateAndInitializeSid(PSID_IDENTIFIER_AUTHORITY,BYTE,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,PSID *);
PVOID     WINAPI RtlAllocateHeap(HANDLE,ULONG,ULONG);
DWORD     WINAPI RtlAnsiStringToUnicodeSize(const STRING *);
NTSTATUS  WINAPI RtlAnsiStringToUnicodeString(PUNICODE_STRING,PCANSI_STRING,BOOLEAN);
NTSTATUS  WINAPI RtlAppendAsciizToString(STRING *,LPCSTR);
NTSTATUS  WINAPI RtlAppendStringToString(STRING *,const STRING *);
NTSTATUS  WINAPI RtlAppendUnicodeStringToString(UNICODE_STRING *,const UNICODE_STRING *);
NTSTATUS  WINAPI RtlAppendUnicodeToString(UNICODE_STRING *,LPCWSTR);
BOOLEAN   WINAPI RtlAreBitsSet(PCRTL_BITMAP,ULONG,ULONG);
BOOLEAN   WINAPI RtlAreBitsClear(PCRTL_BITMAP,ULONG,ULONG);

NTSTATUS  WINAPI RtlCharToInteger(PCSZ,ULONG,PULONG);
void      WINAPI RtlClearAllBits(PRTL_BITMAP);
void      WINAPI RtlClearBits(PRTL_BITMAP,ULONG,ULONG);
ULONG     WINAPI RtlCompactHeap(HANDLE,ULONG);
LONG      WINAPI RtlCompareString(const STRING*,const STRING*,BOOLEAN);
LONG      WINAPI RtlCompareUnicodeString(const UNICODE_STRING*,const UNICODE_STRING*,BOOLEAN);
NTSTATUS  WINAPI RtlConvertSidToUnicodeString(PUNICODE_STRING,PSID,BOOLEAN);
LONGLONG  WINAPI RtlConvertLongToLargeInteger(LONG);
ULONGLONG WINAPI RtlConvertUlongToLargeInteger(ULONG);
DWORD     WINAPI RtlCopySid(DWORD,PSID,PSID);
void      WINAPI RtlCopyString(STRING*,const STRING*);
void      WINAPI RtlCopyUnicodeString(UNICODE_STRING*,const UNICODE_STRING*);
NTSTATUS  WINAPI RtlCreateAcl(PACL,DWORD,DWORD);
DWORD     WINAPI RtlCreateEnvironment(DWORD,DWORD);
HANDLE    WINAPI RtlCreateHeap(ULONG,PVOID,ULONG,ULONG,PVOID,PRTL_HEAP_DEFINITION);
NTSTATUS  WINAPI RtlCreateSecurityDescriptor(PSECURITY_DESCRIPTOR,DWORD);
BOOLEAN   WINAPI RtlCreateUnicodeString(PUNICODE_STRING,LPCWSTR);
BOOLEAN   WINAPI RtlCreateUnicodeStringFromAsciiz(PUNICODE_STRING,LPCSTR);

NTSTATUS  WINAPI RtlDeleteCriticalSection(RTL_CRITICAL_SECTION *);
void      WINAPI RtlDeleteResource(LPRTL_RWLOCK);
DWORD     WINAPI RtlDeleteSecurityObject(DWORD);
DWORD     WINAPI RtlDestroyEnvironment(DWORD);
HANDLE    WINAPI RtlDestroyHeap(HANDLE);
BOOLEAN   WINAPI RtlDosPathNameToNtPathName_U(LPWSTR,PUNICODE_STRING,DWORD,DWORD);
void      WINAPI RtlDumpResource(LPRTL_RWLOCK);

LONGLONG  WINAPI RtlEnlargedIntegerMultiply(INT,INT);
ULONGLONG WINAPI RtlEnlargedUnsignedMultiply(UINT,UINT);
UINT      WINAPI RtlEnlargedUnsignedDivide(ULONGLONG,UINT,UINT *);
NTSTATUS  WINAPI RtlEnterCriticalSection(RTL_CRITICAL_SECTION *);
void      WINAPI RtlEraseUnicodeString(UNICODE_STRING*);
BOOL      WINAPI RtlEqualPrefixSid(PSID,PSID);
BOOL      WINAPI RtlEqualSid(PSID,PSID);
BOOLEAN   WINAPI RtlEqualString(const STRING*,const STRING*,BOOLEAN);
BOOLEAN   WINAPI RtlEqualUnicodeString(const UNICODE_STRING*,const UNICODE_STRING*,BOOLEAN);
LONGLONG  WINAPI RtlExtendedMagicDivide(LONGLONG,LONGLONG,INT);
LONGLONG  WINAPI RtlExtendedIntegerMultiply(LONGLONG,INT);
LONGLONG  WINAPI RtlExtendedLargeIntegerDivide(LONGLONG,INT,INT *);

ULONG     WINAPI RtlFindClearBits(PCRTL_BITMAP,ULONG,ULONG);
ULONG     WINAPI RtlFindClearBitsAndSet(PRTL_BITMAP,ULONG,ULONG);
ULONG     WINAPI RtlFindClearRuns(PCRTL_BITMAP,PRTL_BITMAP_RUN,ULONG,BOOLEAN);
ULONG     WINAPI RtlFindLastBackwardRunSet(PCRTL_BITMAP,ULONG,PULONG);
ULONG     WINAPI RtlFindLastBackwardRunClear(PCRTL_BITMAP,ULONG,PULONG);
CCHAR     WINAPI RtlFindLeastSignificantBit(ULONGLONG);
ULONG     WINAPI RtlFindLongestRunSet(PCRTL_BITMAP,PULONG);
ULONG     WINAPI RtlFindLongestRunClear(PCRTL_BITMAP,PULONG);
CCHAR     WINAPI RtlFindMostSignificantBit(ULONGLONG);
ULONG     WINAPI RtlFindNextForwardRunSet(PCRTL_BITMAP,ULONG,PULONG);
ULONG     WINAPI RtlFindNextForwardRunClear(PCRTL_BITMAP,ULONG,PULONG);
ULONG     WINAPI RtlFindSetBits(PCRTL_BITMAP,ULONG,ULONG);
ULONG     WINAPI RtlFindSetBitsAndClear(PRTL_BITMAP,ULONG,ULONG);
ULONG     WINAPI RtlFindSetRuns(PCRTL_BITMAP,PRTL_BITMAP_RUN,ULONG,BOOLEAN);
BOOLEAN   WINAPI RtlFirstFreeAce(PACL,PACE_HEADER *);
NTSTATUS  WINAPI RtlFormatCurrentUserKeyPath(PUNICODE_STRING);
void      WINAPI RtlFreeAnsiString(PANSI_STRING);
BOOLEAN   WINAPI RtlFreeHeap(HANDLE,ULONG,PVOID);
void      WINAPI RtlFreeOemString(POEM_STRING);
DWORD     WINAPI RtlFreeSid(PSID);
void      WINAPI RtlFreeUnicodeString(PUNICODE_STRING);

DWORD     WINAPI RtlGetAce(PACL,DWORD,LPVOID *);
NTSTATUS  WINAPI RtlGetDaclSecurityDescriptor(PSECURITY_DESCRIPTOR,PBOOLEAN,PACL *,PBOOLEAN);
NTSTATUS  WINAPI RtlGetControlSecurityDescriptor(PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR_CONTROL,LPDWORD);
NTSTATUS  WINAPI RtlGetGroupSecurityDescriptor(PSECURITY_DESCRIPTOR,PSID *,PBOOLEAN);
BOOLEAN   WINAPI RtlGetNtProductType(LPDWORD);
NTSTATUS  WINAPI RtlGetOwnerSecurityDescriptor(PSECURITY_DESCRIPTOR,PSID *,PBOOLEAN);
ULONG     WINAPI RtlGetProcessHeaps(ULONG,HANDLE*);
NTSTATUS  WINAPI RtlGetSaclSecurityDescriptor(PSECURITY_DESCRIPTOR,PBOOLEAN,PACL *,PBOOLEAN);
NTSTATUS  WINAPI RtlGUIDFromString(PUNICODE_STRING,GUID*);

PSID_IDENTIFIER_AUTHORITY WINAPI RtlIdentifierAuthoritySid(PSID);
PVOID     WINAPI RtlImageDirectoryEntryToData(HMODULE,BOOL,WORD,ULONG *);
PIMAGE_NT_HEADERS WINAPI RtlImageNtHeader(HMODULE);
PIMAGE_SECTION_HEADER WINAPI RtlImageRvaToSection(const IMAGE_NT_HEADERS *,HMODULE,DWORD);
PVOID     WINAPI RtlImageRvaToVa(const IMAGE_NT_HEADERS *,HMODULE,DWORD,IMAGE_SECTION_HEADER **);
BOOL      WINAPI RtlImpersonateSelf(SECURITY_IMPERSONATION_LEVEL);
void      WINAPI RtlInitString(PSTRING,PCSZ);
void      WINAPI RtlInitAnsiString(PANSI_STRING,PCSZ);
void      WINAPI RtlInitUnicodeString(PUNICODE_STRING,PCWSTR);
NTSTATUS  WINAPI RtlInitializeCriticalSection(RTL_CRITICAL_SECTION *);
NTSTATUS  WINAPI RtlInitializeCriticalSectionAndSpinCount(RTL_CRITICAL_SECTION *,DWORD);
void      WINAPI RtlInitializeBitMap(PRTL_BITMAP,LPBYTE,ULONG);
void      WINAPI RtlInitializeResource(LPRTL_RWLOCK);
BOOL      WINAPI RtlInitializeSid(PSID,PSID_IDENTIFIER_AUTHORITY,BYTE);

DWORD     WINAPI RtlIntegerToChar(DWORD,DWORD,DWORD,DWORD);
BOOLEAN   WINAPI RtlIsNameLegalDOS8Dot3(PUNICODE_STRING,POEM_STRING,PBOOLEAN);
DWORD     WINAPI RtlIsTextUnicode(CONST VOID *,int,LPINT);

LONGLONG  WINAPI RtlLargeIntegerAdd(LONGLONG,LONGLONG);
LONGLONG  WINAPI RtlLargeIntegerArithmeticShift(LONGLONG,INT);
ULONGLONG WINAPI RtlLargeIntegerDivide( ULONGLONG,ULONGLONG,ULONGLONG *);
LONGLONG  WINAPI RtlLargeIntegerNegate(LONGLONG);
LONGLONG  WINAPI RtlLargeIntegerShiftLeft(LONGLONG,INT);
LONGLONG  WINAPI RtlLargeIntegerShiftRight(LONGLONG,INT);
LONGLONG  WINAPI RtlLargeIntegerSubtract(LONGLONG,LONGLONG);
NTSTATUS  WINAPI RtlLeaveCriticalSection(RTL_CRITICAL_SECTION *);
DWORD     WINAPI RtlLengthRequiredSid(DWORD);
ULONG     WINAPI RtlLengthSecurityDescriptor(PSECURITY_DESCRIPTOR);
DWORD     WINAPI RtlLengthSid(PSID);
NTSTATUS  WINAPI RtlLocalTimeToSystemTime(PLARGE_INTEGER,PLARGE_INTEGER);
BOOLEAN   WINAPI RtlLockHeap(HANDLE);

NTSTATUS  WINAPI RtlMakeSelfRelativeSD(PSECURITY_DESCRIPTOR,PSECURITY_DESCRIPTOR,LPDWORD);
NTSTATUS  WINAPI RtlMultiByteToUnicodeN(LPWSTR,DWORD,LPDWORD,LPCSTR,DWORD);
NTSTATUS  WINAPI RtlMultiByteToUnicodeSize(DWORD*,LPCSTR,UINT);

DWORD     WINAPI RtlNewSecurityObject(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD);
LPVOID    WINAPI RtlNormalizeProcessParams(LPVOID);
ULONG     WINAPI RtlNtStatusToDosError(NTSTATUS);
ULONG     WINAPI RtlNumberOfSetBits(PCRTL_BITMAP);
ULONG     WINAPI RtlNumberOfClearBits(PCRTL_BITMAP);

UINT      WINAPI RtlOemStringToUnicodeSize(const STRING*);
NTSTATUS  WINAPI RtlOemStringToUnicodeString(UNICODE_STRING*,const STRING*,BOOLEAN);
NTSTATUS  WINAPI RtlOemToUnicodeN(LPWSTR,DWORD,LPDWORD,LPCSTR,DWORD);
DWORD     WINAPI RtlOpenCurrentUser(ACCESS_MASK,PHANDLE);

BOOLEAN   WINAPI RtlPrefixString(const STRING*,const STRING*,BOOLEAN);
BOOLEAN   WINAPI RtlPrefixUnicodeString(const UNICODE_STRING*,const UNICODE_STRING*,BOOLEAN);

DWORD     WINAPI RtlQueryEnvironmentVariable_U(DWORD,PUNICODE_STRING,PUNICODE_STRING) ;
NTSTATUS  WINAPI RtlQueryInformationAcl(PACL,LPVOID,DWORD,ACL_INFORMATION_CLASS);
NTSTATUS  WINAPI RtlQueryRegistryValues(ULONG,LPCWSTR,PRTL_QUERY_REGISTRY_TABLE,PVOID,PVOID);

void      WINAPI RtlRaiseException(PEXCEPTION_RECORD);
void      WINAPI RtlRaiseStatus(NTSTATUS);
PVOID     WINAPI RtlReAllocateHeap(HANDLE,ULONG,PVOID,ULONG);
void      WINAPI RtlReleasePebLock(void);
void      WINAPI RtlReleaseResource(LPRTL_RWLOCK);

void      WINAPI RtlSecondsSince1970ToTime(DWORD,LARGE_INTEGER *);
void      WINAPI RtlSecondsSince1980ToTime(DWORD,LARGE_INTEGER *);
void      WINAPI RtlSetAllBits(PRTL_BITMAP);
void      WINAPI RtlSetBits(PRTL_BITMAP,ULONG,ULONG);
NTSTATUS  WINAPI RtlSetDaclSecurityDescriptor(PSECURITY_DESCRIPTOR,BOOLEAN,PACL,BOOLEAN);
DWORD     WINAPI RtlSetEnvironmentVariable(DWORD,PUNICODE_STRING,PUNICODE_STRING);
NTSTATUS  WINAPI RtlSetOwnerSecurityDescriptor(PSECURITY_DESCRIPTOR,PSID,BOOLEAN);
NTSTATUS  WINAPI RtlSetGroupSecurityDescriptor(PSECURITY_DESCRIPTOR,PSID,BOOLEAN);
NTSTATUS  WINAPI RtlSetSaclSecurityDescriptor(PSECURITY_DESCRIPTOR,BOOLEAN,PACL,BOOLEAN);
ULONG     WINAPI RtlSizeHeap(HANDLE,ULONG,PVOID);
LPDWORD   WINAPI RtlSubAuthoritySid(PSID,DWORD);
LPBYTE    WINAPI RtlSubAuthorityCountSid(PSID);
void      WINAPI RtlSystemTimeToLocalTime(PLARGE_INTEGER,PLARGE_INTEGER);

void      WINAPI RtlTimeToTimeFields(PLARGE_INTEGER,PTIME_FIELDS);
BOOLEAN   WINAPI RtlTimeFieldsToTime(PTIME_FIELDS,PLARGE_INTEGER);
void      WINAPI RtlTimeToElapsedTimeFields(PLARGE_INTEGER,PTIME_FIELDS);
BOOLEAN   WINAPI RtlTimeToSecondsSince1970(const LARGE_INTEGER *,PULONG);
BOOLEAN   WINAPI RtlTimeToSecondsSince1980(const LARGE_INTEGER *,LPDWORD);
BOOL      WINAPI RtlTryEnterCriticalSection(RTL_CRITICAL_SECTION *);

DWORD     WINAPI RtlUnicodeStringToAnsiSize(const UNICODE_STRING*);
NTSTATUS  WINAPI RtlUnicodeStringToAnsiString(PANSI_STRING,PCUNICODE_STRING,BOOLEAN);
DWORD     WINAPI RtlUnicodeStringToOemSize(const UNICODE_STRING*);
NTSTATUS  WINAPI RtlUnicodeStringToOemString(POEM_STRING,PCUNICODE_STRING,BOOLEAN);
NTSTATUS  WINAPI RtlUnicodeToMultiByteN(LPSTR,DWORD,LPDWORD,LPCWSTR,DWORD);
NTSTATUS  WINAPI RtlUnicodeToMultiByteSize(PULONG,PCWSTR,ULONG);
NTSTATUS  WINAPI RtlUnicodeToOemN(LPSTR,DWORD,LPDWORD,LPCWSTR,DWORD);
ULONG     WINAPI RtlUniform(PULONG);
BOOLEAN   WINAPI RtlUnlockHeap(HANDLE);
void      WINAPI RtlUnwind(PVOID,PVOID,PEXCEPTION_RECORD,PVOID);
#ifdef __ia64__
void      WINAPI RtlUnwind2(FRAME_POINTERS,PVOID,PEXCEPTION_RECORD,PVOID,PCONTEXT);
void      WINAPI RtlUnwindEx(FRAME_POINTERS,PVOID,PEXCEPTION_RECORD,PVOID,PCONTEXT,PUNWIND_HISTORY_TABLE);
#endif
NTSTATUS  WINAPI RtlUpcaseUnicodeString(UNICODE_STRING*,const UNICODE_STRING *,BOOLEAN);
NTSTATUS  WINAPI RtlUpcaseUnicodeStringToAnsiString(STRING*,const UNICODE_STRING*,BOOLEAN);
NTSTATUS  WINAPI RtlUpcaseUnicodeStringToOemString(STRING*,const UNICODE_STRING*,BOOLEAN);
NTSTATUS  WINAPI RtlUpcaseUnicodeToMultiByteN(LPSTR,DWORD,LPDWORD,LPCWSTR,DWORD);
NTSTATUS  WINAPI RtlUpcaseUnicodeToOemN(LPSTR,DWORD,LPDWORD,LPCWSTR,DWORD);

NTSTATUS  WINAPI RtlValidSecurityDescriptor(PSECURITY_DESCRIPTOR);
BOOL      WINAPI RtlValidSid(PSID);
BOOLEAN   WINAPI RtlValidateHeap(HANDLE,ULONG,PCVOID);

NTSTATUS  WINAPI RtlWalkHeap(HANDLE,PVOID);

NTSTATUS  WINAPI RtlpWaitForCriticalSection(RTL_CRITICAL_SECTION *);
NTSTATUS  WINAPI RtlpUnWaitCriticalSection(RTL_CRITICAL_SECTION *);

/***********************************************************************
 * Inline functions
 */

#define InitializeObjectAttributes(p,n,a,r,s) \
    do { \
        (p)->Length = sizeof(OBJECT_ATTRIBUTES); \
        (p)->RootDirectory = r; \
        (p)->Attributes = a; \
        (p)->ObjectName = n; \
        (p)->SecurityDescriptor = s; \
        (p)->SecurityQualityOfService = NULL; \
    } while (0)

#define NtCurrentProcess() ((HANDLE)-1)

#define RtlFillMemory(Destination,Length,Fill) memset((Destination),(Fill),(Length))
#define RtlMoveMemory(Destination,Source,Length) memmove((Destination),(Source),(Length))
#define RtlStoreUlong(p,v)  do { ULONG _v = (v); memcpy((p), &_v, sizeof(_v)); } while (0)
#define RtlStoreUlonglong(p,v) do { ULONGLONG _v = (v); memcpy((p), &_v, sizeof(_v)); } while (0)
#define RtlRetrieveUlong(p,s) memcpy((p), (s), sizeof(ULONG))
#define RtlRetrieveUlonglong(p,s) memcpy((p), (s), sizeof(ULONGLONG))
#define RtlZeroMemory(Destination,Length) memset((Destination),0,(Length))

static inline USHORT RtlUshortByteSwap(USHORT s)
{
    return (s >> 8) | (s << 8);
}

inline static BOOLEAN RtlCheckBit(PCRTL_BITMAP lpBits, ULONG ulBit)
{
    if (lpBits && ulBit < lpBits->SizeOfBitMap &&
        lpBits->BitMapBuffer[ulBit >> 3] & (1 << (ulBit & 7)))
        return TRUE;
    return FALSE;
}

#define RtlClearAllBits(p) \
    do { \
        PRTL_BITMAP _p = (p); \
        memset(_p->BitMapBuffer,0,((_p->SizeOfBitMap + 31) & 0xffffffe0) >> 3); \
    } while (0)

#define RtlInitializeBitMap(p,b,s) \
    do { \
        PRTL_BITMAP _p = (p); \
        _p->SizeOfBitMap = (s); \
        _p->BitMapBuffer = (b); \
    } while (0)

#define RtlSetAllBits(p) \
    do { \
        PRTL_BITMAP _p = (p); \
        memset(_p->BitMapBuffer,0xff,((_p->SizeOfBitMap + 31) & 0xffffffe0) >> 3); \
    } while (0)

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif  /* __WINE_WINTERNAL_H */
