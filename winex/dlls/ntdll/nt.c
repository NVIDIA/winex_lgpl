/*
 * NT basis DLL
 *
 * This file contains the Nt* API functions of NTDLL.DLL.
 * In the original ntdll.dll they all seem to just call int 0x2e (down to the NTOSKRNL)
 *
 * Copyright 1996-1998 Marcus Meissner
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include "wine/exception.h"
#include "wine/debug.h"

#include "winternl.h"
#include "ntdll_misc.h"
#include "tlhelp32.h"
#include "wine/server.h"
#include "msvcrt/excpt.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

extern DWORD gdwTlsModuleList;


/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(any_fault)
{
   return EXCEPTION_EXECUTE_HANDLER;
}

/*
 *	Timer object
 */

/**************************************************************************
 *		NtCreateTimer				[NTDLL.@]
 *		ZwCreateTimer				[NTDLL.@]
 */
NTSTATUS WINAPI NtCreateTimer(
	OUT PHANDLE TimerHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
	IN TIMER_TYPE TimerType)
{
	FIXME("(%p,0x%08lx,%p,0x%08x) stub\n",
	TimerHandle,DesiredAccess,ObjectAttributes, TimerType);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}
/**************************************************************************
 *		NtSetTimer				[NTDLL.@]
 *		ZwSetTimer				[NTDLL.@]
 */
NTSTATUS WINAPI NtSetTimer(
	IN HANDLE TimerHandle,
	IN PLARGE_INTEGER DueTime,
	IN PTIMERAPCROUTINE TimerApcRoutine,
	IN PVOID TimerContext,
	IN BOOLEAN WakeTimer,
	IN ULONG Period OPTIONAL,
	OUT PBOOLEAN PreviousState OPTIONAL)
{
	FIXME("(0x%08x,%p,%p,%p,%08x,0x%08lx,%p) stub\n",
	TimerHandle,DueTime,TimerApcRoutine,TimerContext,WakeTimer,Period,PreviousState);
	return 0;
}

/******************************************************************************
 * NtQueryTimerResolution [NTDLL.@]
 */
NTSTATUS WINAPI NtQueryTimerResolution(DWORD x1,DWORD x2,DWORD x3)
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx), stub!\n",x1,x2,x3);
	return 1;
}

/*
 *	Process object
 */

/******************************************************************************
 *  NtTerminateProcess			[NTDLL.@]
 *
 *  Native applications must kill themselves when done
 */
NTSTATUS WINAPI NtTerminateProcess( HANDLE handle, LONG exit_code )
{
    NTSTATUS ret;
    BOOL self;

    TRACE("handle: %d, exit_code: %ld\n", handle, exit_code);

    SERVER_START_REQ( terminate_process )
    {
        req->handle    = handle;
        req->exit_code = exit_code;
        ret = wine_server_call( req );
        self = !ret && reply->self;
    }
    SERVER_END_REQ;

    /* Reset display in case this terminate preempts driver detach */
    __TRY
    {
       HINSTANCE hinst;
       FARPROC WINAPI pf;

       hinst = LoadLibraryA("user32.dll");
       if (hinst) {
          pf = GetProcAddress(hinst, "DisplayEmergencyExit");
          if (pf) {
             (*pf)();
          }
       }
    }
    __EXCEPT(any_fault)
    {
       ERR ("Exception caught from DisplayEmergencyExit!\n");
    }
    __ENDTRY

    if (self) exit( exit_code );
    return ret;
}

/******************************************************************************
*  NtQueryInformationProcess		[NTDLL.@]
*  ZwQueryInformationProcess		[NTDLL.@]
*
*/
NTSTATUS WINAPI NtQueryInformationProcess(
	IN HANDLE ProcessHandle,
	IN PROCESSINFOCLASS ProcessInformationClass,
	OUT PVOID ProcessInformation,
	IN ULONG ProcessInformationLength,
	OUT PULONG ReturnLength)
{
	FIXME("(0x%08x,0x%08x,%p,0x%08lx,%p),stub!\n",
		ProcessHandle,ProcessInformationClass,ProcessInformation,ProcessInformationLength,ReturnLength
	);
	/* "These are not the debuggers you are looking for." */
	if (ProcessInformationClass == ProcessDebugPort)
	    /* set it to 0 aka "no debugger" to satisfy copy protections */
	    memset(ProcessInformation,0,ProcessInformationLength);

	return 0;
}

/******************************************************************************
 * NtSetInformationProcess [NTDLL.@]
 * ZwSetInformationProcess [NTDLL.@]
 */
NTSTATUS WINAPI NtSetInformationProcess(
	IN HANDLE ProcessHandle,
	IN PROCESSINFOCLASS ProcessInformationClass,
	IN PVOID ProcessInformation,
	IN ULONG ProcessInformationLength)
{
	FIXME("(0x%08x,0x%08x,%p,0x%08lx) stub\n",
	ProcessHandle,ProcessInformationClass,ProcessInformation,ProcessInformationLength);
	return 0;
}

/*
 *	Thread
 */

/******************************************************************************
*  NtYieldExecution		[NTDLL.@]
*  ZwYieldExecution		[NTDLL.@]
*
*/
NTSTATUS WINAPI NtYieldExecution (void)
{
   LARGE_INTEGER Start, End;

   NtQueryPerformanceCounter (&Start, NULL);
   sched_yield ();
   NtQueryPerformanceCounter (&End, NULL);

   /* Ideally we could query whether a yield occurred from the OS; however,
      sched_yield doesn't return documented useful info. Thus, assume we
      yielded if the sched_yield took longer than 10 us */
   if (End.QuadPart > (Start.QuadPart + 10))
      return STATUS_SUCCESS;

   return STATUS_NO_YIELD_PERFORMED;
}


/******************************************************************************
 *  NtResumeThread	[NTDLL.@]
 *  ZwResumeThread	[NTDLL.@]
 */
NTSTATUS WINAPI NtResumeThread(
	IN HANDLE ThreadHandle,
	IN PULONG SuspendCount)
{
	FIXME("(0x%08x,%p),stub!\n",
	ThreadHandle,SuspendCount);
	return 0;
}

static NTSTATUS MODULE_THREAD_WaitThreadModuleSafe(HANDLE handle)
{
    BOOL self;
    int status;
    BOOL goodToKill = FALSE;
    NTSTATUS ret;

    do
    {
        SERVER_START_REQ(get_thread_moduleload)
        {
            req->handle = handle;
            ret = wine_server_call(req);
            status = reply->status;
            self = (BOOL)reply->self;
        }
        SERVER_END_REQ;

        if( ret )
        {
            break;
        }
        else if (status == THREAD_MODULE_NONE)
        {
            goodToKill = TRUE;
        }
        else if (self && status == THREAD_MODULE_LOADING)
        {
            void *modlist = TlsGetValue(gdwTlsModuleList);
            HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, modlist);
            TlsFree(gdwTlsModuleList);
            goodToKill = TRUE;
        }
        else if (self)
        {
            /* they shouldn't be able to kill themselves anywhere during thread
             * init unless its during module init (dll entry point code).
             * so if this triggers, its a bug in wine.
             * THREAD_MODULE_NONE is ok, and thats handled by the first case
             */
            /* if wine crashes during dll init, we kill the threads anyway. */
            ERR("this should never happen. (unless wine crashed)\n");
            sleep(1);
            goodToKill = FALSE;
        }
        else /* its another thread in BUILDING or LOADING,
              * so we wait for it to finish the loading. dodgy recursive hack,
              * but oh well. FIXME - use wine wait queues once they are
              * better.
              */
        {
            sleep(1);
            goodToKill = FALSE;
        }
    } while (!goodToKill);

    return ret;
}

/******************************************************************************
 *  NtTerminateThread	[NTDLL.@]
 *  ZwTerminateThread	[NTDLL.@]
 */
NTSTATUS WINAPI NtTerminateThread( HANDLE handle, LONG exit_code )
{
    NTSTATUS ret;
    BOOL self, last;

    MODULE_THREAD_WaitThreadModuleSafe(handle);

    SERVER_START_REQ( terminate_thread )
    {
        req->handle    = handle;
        req->exit_code = exit_code;
        ret = wine_server_call( req );
        self = !ret && reply->self;
        last = reply->last;
    }
    SERVER_END_REQ;

    if (self)
    {
        if (last) exit( exit_code );
        else SYSDEPS_ExitThread( exit_code );
    }
    return ret;
}


/******************************************************************************
*  NtQueryInformationThread		[NTDLL.@]
*  ZwQueryInformationThread		[NTDLL.@]
*
*/
NTSTATUS WINAPI NtQueryInformationThread(
	IN HANDLE ThreadHandle,
	IN THREADINFOCLASS ThreadInformationClass,
	OUT PVOID ThreadInformation,
	IN ULONG ThreadInformationLength,
	OUT PULONG ReturnLength)
{
    NTSTATUS result;


    FIXME("(0x%08x,0x%08x,%p,0x%08lx,%p), mostly-stub!\n",
            ThreadHandle, ThreadInformationClass, ThreadInformation,
            ThreadInformationLength, ReturnLength);


    /* passing NULL for the buffer results in an access violation and <ReturnLength> unmodified */
    if (ThreadInformation == NULL)
        return STATUS_ACCESS_VIOLATION;


    switch (ThreadInformationClass){
        case ThreadBasicInformation:
            TRACE("gathering basic thread information\n");

            /* make sure the buffer is large enough to hold our results.  Native always
               returns STATUS_INFO_LENGTH_MISMATCH if the buffer is too small. */
            if (ThreadInformationLength < sizeof(THREAD_BASIC_INFORMATION))
                return STATUS_INFO_LENGTH_MISMATCH;


            /* ask the wine server for info about the requested thread */
            SERVER_START_REQ(get_thread_info)
            {
                req->handle = ThreadHandle;
                req->tid_in = 0;

                result = wine_server_call(req);

                if (result == STATUS_SUCCESS){
                    THREAD_BASIC_INFORMATION *  tbi = (THREAD_BASIC_INFORMATION *)ThreadInformation;


                    tbi->ExitStatus =                reply->exit_code;
                    tbi->TebBaseAddress =            reply->teb;
                    tbi->ClientId.UniqueProcess =    ((HANDLE)(ULONG_PTR) (reply->pid) );
                    tbi->ClientId.UniqueThread =     ((HANDLE)(ULONG_PTR) (reply->tid) );
                    tbi->AffinityMask =              reply->affinity;
                    tbi->Priority =                  reply->priority;
                    tbi->BasePriority =              reply->priority;   /* FIXME: this info is neither stored nor passed back! */

                    /* set the number of bytes that was written to the output buffer */
                    if (ReturnLength)
                        *ReturnLength = sizeof(THREAD_BASIC_INFORMATION);
                }
            }
            SERVER_END_REQ;


            /* server call failed => make sure to return an error code.  This likely isn't the correct
               error code, but windows doesn't have an analogue to this case so i'm guessing here. */
            if (result != STATUS_SUCCESS)
                return STATUS_NO_MEMORY;

            break;

        case ThreadTimes:
        case ThreadPriority:
        case ThreadBasePriority:
        case ThreadAffinityMask:
        case ThreadImpersonationToken:
        case ThreadDescriptorTableEntry:
        case ThreadEnableAlignmentFaultFixup:
        case ThreadEventPair_Reusable:
        case ThreadQuerySetWin32StartAddress:
        case ThreadZeroTlsCell:
        case ThreadPerformanceCount:
        case ThreadAmILastThread:
        case ThreadIdealProcessor:
        case ThreadPriorityBoost:
        case ThreadSetTlsArrayAddress:
        case ThreadIsIoPending:
        case MaxThreadInfoClass:
            FIXME("implement me!! {ThreadInformationClass = 0x%08x}\n", ThreadInformationClass);
            break;

        default:
            ERR("unknown information class! {ThreadInformationClass = 0x%08x}\n", ThreadInformationClass);
            break;
    }

	return STATUS_SUCCESS;
}

/******************************************************************************
 *  NtSetInformationThread		[NTDLL.@]
 *  ZwSetInformationThread		[NTDLL.@]
 */
NTSTATUS WINAPI NtSetInformationThread(
	HANDLE ThreadHandle,
	THREADINFOCLASS ThreadInformationClass,
	PVOID ThreadInformation,
	ULONG ThreadInformationLength)
{
	FIXME("(0x%08x,0x%08x,%p,0x%08lx),stub!\n",
	ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength);
	return 0;
}

/*
 *	Token
 */

/******************************************************************************
 *  NtDuplicateToken		[NTDLL.@]
 *  ZwDuplicateToken		[NTDLL.@]
 */
NTSTATUS WINAPI NtDuplicateToken(
        IN HANDLE ExistingToken,
        IN ACCESS_MASK DesiredAccess,
        IN POBJECT_ATTRIBUTES ObjectAttributes,
        IN SECURITY_IMPERSONATION_LEVEL ImpersonationLevel,
        IN TOKEN_TYPE TokenType,
        OUT PHANDLE NewToken)
{
	FIXME("(0x%08x,0x%08lx,%p,0x%08x,0x%08x,%p),stub!\n",
	ExistingToken, DesiredAccess, ObjectAttributes,
	ImpersonationLevel, TokenType, NewToken);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtOpenProcessToken		[NTDLL.@]
 *  ZwOpenProcessToken		[NTDLL.@]
 */
NTSTATUS WINAPI NtOpenProcessToken(
	HANDLE ProcessHandle,
	DWORD DesiredAccess,
	HANDLE *TokenHandle)
{
	NTSTATUS ret;
	HANDLE token_handle;
	TRACE("(0x%08x,0x%08lx,%p)\n",
	ProcessHandle,DesiredAccess, TokenHandle);

	SERVER_START_REQ( open_token )
	{
		req->handle = ProcessHandle;
		req->access = DesiredAccess;
		req->thread = 0;
		ret = wine_server_call( req );
		token_handle = reply->handle;
	}
	SERVER_END_REQ;
	*TokenHandle = token_handle;
	return ret;
}

/******************************************************************************
 *  NtOpenThreadToken		[NTDLL.@]
 *  ZwOpenThreadToken		[NTDLL.@]
 */
NTSTATUS WINAPI NtOpenThreadToken(
	HANDLE ThreadHandle,
	DWORD DesiredAccess,
	BOOLEAN OpenAsSelf,
	HANDLE *TokenHandle)
{
	FIXME("(0x%08x,0x%08lx,0x%08x,%p): stub\n",
	ThreadHandle,DesiredAccess, OpenAsSelf, TokenHandle);
	*TokenHandle = 0xcafe;
	return 0;
}

/******************************************************************************
 *  NtAdjustPrivilegesToken		[NTDLL.@]
 *  ZwAdjustGroupsToken		[NTDLL.@]
 *
 * FIXME: parameters unsafe
 */
NTSTATUS WINAPI NtAdjustPrivilegesToken(
	IN HANDLE TokenHandle,
	IN BOOLEAN DisableAllPrivileges,
	IN PTOKEN_PRIVILEGES NewState,
	IN DWORD BufferLength,
	OUT PTOKEN_PRIVILEGES PreviousState,
	OUT PDWORD ReturnLength)
{
	FIXME("(0x%08x,0x%08x,%p,0x%08lx,%p,%p),stub!\n",
	TokenHandle, DisableAllPrivileges, NewState, BufferLength, PreviousState, ReturnLength);
	return 0;
}

/******************************************************************************
*  NtQueryInformationToken		[NTDLL.@]
*  ZwQueryInformationToken		[NTDLL.@]
*
* NOTES
*  Buffer for TokenUser:
*   0x00 TOKEN_USER the PSID field points to the SID
*   0x08 SID
*
*/
NTSTATUS WINAPI NtQueryInformationToken(
	HANDLE token,
	TOKEN_INFORMATION_CLASS tokeninfoclass,
	LPVOID tokeninfo,
	DWORD tokeninfolength,
	PDWORD retlen )
{
    unsigned int len = 0;

    FIXME("(%08x,%d,%p,%ld,%p): partial stub\n",
          token,tokeninfoclass,tokeninfo,tokeninfolength,retlen);

    switch (tokeninfoclass)
    {
    case TokenUser:
        len = sizeof(TOKEN_USER) + sizeof(SID);
        break;
    case TokenGroups:
        len = sizeof(TOKEN_GROUPS);
        break;
    case TokenPrivileges:
        len = sizeof(TOKEN_PRIVILEGES) + (19 * sizeof (LUID_AND_ATTRIBUTES));
        break;
    case TokenOwner:
        len = sizeof(TOKEN_OWNER);
        break;
    case TokenPrimaryGroup:
        len = sizeof(TOKEN_PRIMARY_GROUP);
        break;
    case TokenDefaultDacl:
        len = sizeof(TOKEN_DEFAULT_DACL);
        break;
    case TokenSource:
        len = sizeof(TOKEN_SOURCE);
        break;
    case TokenType:
        len = sizeof (TOKEN_TYPE);
        break;
#if 0
    case TokenImpersonationLevel:
    case TokenStatistics:
#endif /* 0 */
    default:
        FIXME("Unhandled type: %d\n", tokeninfoclass);
    }

    /* FIXME: what if retlen == NULL ? */
    *retlen = len;

    if (tokeninfolength < len)
        return STATUS_BUFFER_TOO_SMALL;

    switch (tokeninfoclass)
    {
    case TokenUser:
        if( tokeninfo )
        {
            TOKEN_USER * tuser = tokeninfo;
            PSID sid = (PSID) (tuser + 1);
            SID_IDENTIFIER_AUTHORITY localSidAuthority = {SECURITY_NT_AUTHORITY};
            RtlInitializeSid(sid, &localSidAuthority, 1);
            *(RtlSubAuthoritySid(sid, 0)) = SECURITY_INTERACTIVE_RID;
            tuser->User.Sid = sid;
        }
        break;
    case TokenGroups:
        if (tokeninfo)
        {
            TOKEN_GROUPS *tgroups = tokeninfo;
            SID_IDENTIFIER_AUTHORITY sid = {SECURITY_NT_AUTHORITY};

            /* we need to show admin privileges ! */
            tgroups->GroupCount = 1;
            RtlAllocateAndInitializeSid( &sid,
                                         2,
                                         SECURITY_BUILTIN_DOMAIN_RID,
                                         DOMAIN_ALIAS_RID_ADMINS,
                                         0, 0, 0, 0, 0, 0,
                                         &(tgroups->Groups->Sid));
        }
        break;
    case TokenPrivileges:
        if (tokeninfo)
        {
            TOKEN_PRIVILEGES *tpriv = tokeninfo;

            /* These settings match those for an admin-priv token on XP SP2 */
            LUID_AND_ATTRIBUTES LuidArr[20] = {
               {{SE_CHANGE_NOTIFY_PRIVILEGE, 0}, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT},
               {{SE_SECURITY_PRIVILEGE, 0}, 0},
               {{SE_BACKUP_PRIVILEGE, 0}, 0},
               {{SE_RESTORE_PRIVILEGE, 0}, 0},
               {{SE_SYSTEMTIME_PRIVILEGE, 0}, 0},
               {{SE_SHUTDOWN_PRIVILEGE, 0}, 0},
               {{SE_REMOTE_SHUTDOWN_PRIVILEGE, 0}, 0},
               {{SE_TAKE_OWNERSHIP_PRIVILEGE, 0}, 0},
               {{SE_DEBUG_PRIVILEGE, 0}, SE_PRIVILEGE_ENABLED},
               {{SE_SYSTEM_ENVIRONMENT_PRIVILEGE, 0}, 0},
               {{SE_SYSTEM_PROFILE_PRIVILEGE, 0}, 0},
               {{SE_PROF_SINGLE_PROCESS_PRIVILEGE, 0}, 0},
               {{SE_INC_BASE_PRIORITY_PRIVILEGE, 0}, 0},
               {{SE_LOAD_DRIVER_PRIVILEGE, 0}, SE_PRIVILEGE_ENABLED},
               {{SE_CREATE_PAGEFILE_PRIVILEGE, 0}, 0},
               {{SE_INCREASE_QUOTA_PRIVILEGE, 0}, 0},
               {{SE_UNDOCK_PRIVILEGE, 0}, SE_PRIVILEGE_ENABLED},
               {{SE_MANAGE_VOLUME_PRIVILEGE, 0}, 0},
               {{SE_IMPERSONATE_PRIVILEGE, 0}, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT},
               {{SE_CREATE_GLOBAL_PRIVILEGE, 0}, SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT}};

            tpriv->PrivilegeCount = 20;
            memcpy (tpriv->Privileges, LuidArr, sizeof (LuidArr));
        }
        break;
    case TokenOwner:
        if (tokeninfo)
        {
            TOKEN_OWNER *towner = tokeninfo;
            PSID sid = (PSID) (towner + 1);
            SID_IDENTIFIER_AUTHORITY localSidAuthority = {SECURITY_NT_AUTHORITY};
            RtlInitializeSid(sid, &localSidAuthority, 1);
            *(RtlSubAuthoritySid(sid, 0)) = SECURITY_INTERACTIVE_RID;
            towner->Owner = sid;
        }
        break;
	case TokenSource:
        if (tokeninfo)
        {
            TOKEN_SOURCE *tsrc = tokeninfo;
			memset(&tsrc->Sourcename,0,sizeof(tsrc->Sourcename));
			tsrc->Sourcename[0]='U';
			tsrc->Sourcename[1]='s';
			tsrc->Sourcename[2]='e';
			tsrc->Sourcename[3]='r';
			tsrc->Sourcename[4]='3';
			tsrc->Sourcename[5]='2';
			tsrc->Sourcename[6]=0;
			tsrc->Sourcename[7]=0;
			memset(&tsrc->SourceIdentifier,0,sizeof(tsrc->SourceIdentifier));
			FIXME("stub - TokenSource LUID not implimented\n");
        }
        break;
    case TokenPrimaryGroup:
        if (tokeninfo)
        {
            TOKEN_PRIMARY_GROUP *tgroup = tokeninfo;
            SID_IDENTIFIER_AUTHORITY sid = {SECURITY_NT_AUTHORITY};

            /* we need to show admin privileges ! */
            RtlAllocateAndInitializeSid( &sid,
                                         2,
                                         SECURITY_BUILTIN_DOMAIN_RID,
                                         DOMAIN_ALIAS_RID_ADMINS,
                                         0, 0, 0, 0, 0, 0,
                                         &(tgroup->PrimaryGroup));			
        }
        break;
	default:	
        FIXME("Unhandled type: %d\n", tokeninfoclass);
    }
    return 0;
}

/*
 *	Section
 */

/******************************************************************************
 *  NtCreateSection	[NTDLL.@]
 *  ZwCreateSection	[NTDLL.@]
 */
NTSTATUS WINAPI NtCreateSection(
	OUT PHANDLE SectionHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
	IN PLARGE_INTEGER MaximumSize OPTIONAL,
	IN ULONG SectionPageProtection OPTIONAL,
	IN ULONG AllocationAttributes,
	IN HANDLE FileHandle OPTIONAL)
{
	FIXME("(%p,0x%08lx,%p,%p,0x%08lx,0x%08lx,0x%08x) stub\n",
	SectionHandle,DesiredAccess, ObjectAttributes,
	MaximumSize,SectionPageProtection,AllocationAttributes,FileHandle);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtOpenSection	[NTDLL.@]
 *  ZwOpenSection	[NTDLL.@]
 */
NTSTATUS WINAPI NtOpenSection(
	PHANDLE SectionHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes)
{
	FIXME("(%p,0x%08lx,%p),stub!\n",
	SectionHandle,DesiredAccess,ObjectAttributes);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtQuerySection	[NTDLL.@]
 */
NTSTATUS WINAPI NtQuerySection(
	IN HANDLE SectionHandle,
	IN PVOID SectionInformationClass,
	OUT PVOID SectionInformation,
	IN ULONG Length,
	OUT PULONG ResultLength)
{
	FIXME("(0x%08x,%p,%p,0x%08lx,%p) stub!\n",
	SectionHandle,SectionInformationClass,SectionInformation,Length,ResultLength);
	return 0;
}

/******************************************************************************
 * NtMapViewOfSection	[NTDLL.@]
 * ZwMapViewOfSection	[NTDLL.@]
 * FUNCTION: Maps a view of a section into the virtual address space of a process
 *
 * ARGUMENTS:
 *  SectionHandle	Handle of the section
 *  ProcessHandle	Handle of the process
 *  BaseAddress		Desired base address (or NULL) on entry
 *			Actual base address of the view on exit
 *  ZeroBits		Number of high order address bits that must be zero
 *  CommitSize		Size in bytes of the initially committed section of the view
 *  SectionOffset	Offset in bytes from the beginning of the section to the beginning of the view
 *  ViewSize		Desired length of map (or zero to map all) on entry
 			Actual length mapped on exit
 *  InheritDisposition	Specified how the view is to be shared with
 *			child processes
 *  AllocateType	Type of allocation for the pages
 *  Protect		Protection for the committed region of the view
 */
NTSTATUS WINAPI NtMapViewOfSection(
	HANDLE SectionHandle,
	HANDLE ProcessHandle,
	PVOID* BaseAddress,
	ULONG ZeroBits,
	ULONG CommitSize,
	PLARGE_INTEGER SectionOffset,
	PULONG ViewSize,
	SECTION_INHERIT InheritDisposition,
	ULONG AllocationType,
	ULONG Protect)
{
	FIXME("(0x%08x,0x%08x,%p,0x%08lx,0x%08lx,%p,%p,0x%08x,0x%08lx,0x%08lx) stub\n",
	SectionHandle,ProcessHandle,BaseAddress,ZeroBits,CommitSize,SectionOffset,
	ViewSize,InheritDisposition,AllocationType,Protect);
	return 0;
}

/*
 *	ports
 */

/******************************************************************************
 *  NtCreatePort		[NTDLL.@]
 *  ZwCreatePort		[NTDLL.@]
 */
NTSTATUS WINAPI NtCreatePort(DWORD x1,DWORD x2,DWORD x3,DWORD x4,DWORD x5)
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4,x5);
	return 0;
}

/******************************************************************************
 *  NtConnectPort		[NTDLL.@]
 *  ZwConnectPort		[NTDLL.@]
 */
NTSTATUS WINAPI NtConnectPort(DWORD x1,PUNICODE_STRING uni,DWORD x3,DWORD x4,DWORD x5,DWORD x6,DWORD x7,DWORD x8)
{
	FIXME("(0x%08lx,%s,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",
	x1,debugstr_w(uni->Buffer),x3,x4,x5,x6,x7,x8);
	return 0;
}

/******************************************************************************
 *  NtListenPort		[NTDLL.@]
 *  ZwListenPort		[NTDLL.@]
 */
NTSTATUS WINAPI NtListenPort(DWORD x1,DWORD x2)
{
	FIXME("(0x%08lx,0x%08lx),stub!\n",x1,x2);
	return 0;
}

/******************************************************************************
 *  NtAcceptConnectPort	[NTDLL.@]
 *  ZwAcceptConnectPort	[NTDLL.@]
 */
NTSTATUS WINAPI NtAcceptConnectPort(DWORD x1,DWORD x2,DWORD x3,DWORD x4,DWORD x5,DWORD x6)
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4,x5,x6);
	return 0;
}

/******************************************************************************
 *  NtCompleteConnectPort	[NTDLL.@]
 *  ZwCompleteConnectPort	[NTDLL.@]
 */
NTSTATUS WINAPI NtCompleteConnectPort(DWORD x1)
{
	FIXME("(0x%08lx),stub!\n",x1);
	return 0;
}

/******************************************************************************
 *  NtRegisterThreadTerminatePort	[NTDLL.@]
 *  ZwRegisterThreadTerminatePort	[NTDLL.@]
 */
NTSTATUS WINAPI NtRegisterThreadTerminatePort(DWORD x1)
{
	FIXME("(0x%08lx),stub!\n",x1);
	return 0;
}

/******************************************************************************
 *  NtRequestWaitReplyPort		[NTDLL.@]
 *  ZwRequestWaitReplyPort		[NTDLL.@]
 */
NTSTATUS WINAPI NtRequestWaitReplyPort(DWORD x1,DWORD x2,DWORD x3)
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3);
	return 0;
}

/******************************************************************************
 *  NtReplyWaitReceivePort	[NTDLL.@]
 *  ZwReplyWaitReceivePort	[NTDLL.@]
 */
NTSTATUS WINAPI NtReplyWaitReceivePort(DWORD x1,DWORD x2,DWORD x3,DWORD x4)
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4);
	return 0;
}

/*
 *	Misc
 */

 /******************************************************************************
 *  NtSetIntervalProfile	[NTDLL.@]
 *  ZwSetIntervalProfile	[NTDLL.@]
 */
NTSTATUS WINAPI NtSetIntervalProfile(DWORD x1,DWORD x2) {
	FIXME("(0x%08lx,0x%08lx),stub!\n",x1,x2);
	return 0;
}

/******************************************************************************
 *  NtCreateMailslotFile	[NTDLL.@]
 *  ZwCreateMailslotFile	[NTDLL.@]
 */
NTSTATUS WINAPI NtCreateMailslotFile(DWORD x1,DWORD x2,DWORD x3,DWORD x4,DWORD x5,DWORD x6,DWORD x7,DWORD x8)
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4,x5,x6,x7,x8);
	return 0;
}


/******************************************************************************
 * Helper for NtQuerySystemInformation
 */
static NTSTATUS SnapCreate (HANDLE *pSnap)
{
   NTSTATUS Ret;

   /* Create initial snapshot */
   SERVER_START_REQ (create_snapshot)
   {
      req->flags = TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD;
      req->inherit = 0;
      req->pid = 0;
      if ((Ret = wine_server_call_err (req)) == STATUS_SUCCESS)
         *pSnap = reply->handle;
   }
   SERVER_END_REQ;

   if (*pSnap == INVALID_HANDLE_VALUE)
      ERR ("Unable to create process snapshot!\n");

   return Ret;
}



/******************************************************************************
 * Helper for NtQuerySystemInformation
 *
 *  <hSnap> is the handle to an open process snapshot
 *  <processID> is the ID of the parent process of the threads to be enumerated
 *  <length> is the size of the buffer <pSTI> in bytes
 *  <pSTI> is the buffer to start storing the thread information blocks at
 */
static NTSTATUS SnapWalkThreads(HANDLE hSnap, DWORD processID, DWORD length, SYSTEM_THREAD_INFORMATION *pSTI)
{
    NTSTATUS    Ret = STATUS_SUCCESS;
    DWORD       CurThread = 0;


    while (Ret == STATUS_SUCCESS){

        SERVER_START_REQ (next_thread){
            req->handle = hSnap;
            req->reset = (CurThread == 0);
            Ret = wine_server_call_err (req);

            CurThread++;

            if (Ret == STATUS_SUCCESS){

                /* only write the thread info if there is enough room in the buffer for another full block */
                if (pSTI && length >= sizeof(*pSTI)){

                    /* not my thread => get next thread */
                    if (reply->pid != processID)
                        continue;

                    memset (pSTI, 0, sizeof(*pSTI));
                    pSTI->dwOwningPID = (DWORD)reply->pid;
                    pSTI->dwThreadID = (DWORD)reply->tid;
                    pSTI->dwCurrentPriority = reply->base_pri + reply->delta_pri;
                    pSTI->dwBasePriority = reply->base_pri;
                    /* FIXME: several members of this struct are being left as 0 */

                    /* add a bad food marker to the end of the thread info block (native does it; 
                       perhaps for identification/sync/marking purposes?) */
                    pSTI->dwUnknown = 0xbaadf00d;

                    /* update the buffer size and move to the next thread info block */
                    length -= sizeof(*pSTI);
                    pSTI++;
                }
            }
        }
        SERVER_END_REQ;
    }

    if (Ret == STATUS_NO_MORE_FILES)
       Ret = STATUS_SUCCESS;

    return Ret;
}

/******************************************************************************
 * Helper for NtQuerySystemInformation
 */
static NTSTATUS SnapGetNextProcess (HANDLE hSnap, DWORD LenLeft,
                                    PSYSTEM_PROCESS_INFORMATION pSPI, ULONG *returnLength)
{
    NTSTATUS    Ret;
    CHAR        ProcName[1024];
    CHAR *      pExeName;
    DWORD       Len;
    STRING      AnsiStr;
    DWORD       threadCount;
    DWORD       processID;
    size_t      nameLen;
    size_t      nameSize;


    SERVER_START_REQ (next_process)
    {
        req->handle = hSnap;
        req->reset = FALSE;
        wine_server_set_reply (req, ProcName, sizeof (ProcName));
        if ((Ret = wine_server_call_err (req)) == STATUS_SUCCESS)
        {
            ProcName[wine_server_reply_size (reply)] = 0;

            /* save these since we can't necessarily access them from the <pSPI> block */
            threadCount = reply->threads;
            processID = (DWORD)reply->pid;

            if (pSPI && LenLeft >= sizeof(*pSPI)){
                memset (pSPI, 0, sizeof (*pSPI));
                pSPI->dwThreadCount = reply->threads;
                pSPI->dwBasePriority = reply->priority;
                pSPI->dwProcessID = (DWORD)reply->pid;
                pSPI->dwParentProcessID = (DWORD)reply->ppid;
            }
        }
    }
    SERVER_END_REQ;


    if (Ret != STATUS_SUCCESS){
        if (returnLength)
            *returnLength = 0;

        return Ret;
    }


    if ((pExeName = strrchr(ProcName, '\\')))
        pExeName++;

    else
        pExeName = ProcName;


    nameLen = strlen(pExeName);
    nameSize = ((((nameLen + 1) * sizeof(WCHAR)) + 7) & ~7);

    /* the size of the string buffer is aligned to an 8-byte boundary in native */
    Len = sizeof(*pSPI) + nameSize + (threadCount * sizeof (SYSTEM_THREAD_INFORMATION));


    /* write the thread information blocks to the buffer as long as we can fit at least one */
    if (pSPI && LenLeft > sizeof(*pSPI) + sizeof(SYSTEM_THREAD_INFORMATION)){

        /* grab the thread info blocks */
        Ret = SnapWalkThreads(  hSnap, 
                                processID, 
                                LenLeft >= sizeof(*pSPI) ? LenLeft - sizeof(*pSPI) : 0, 
                                pSPI ? pSPI->ti : NULL);


        /* write the process name at the end of the thread info blocks */
        if (LenLeft >= Len){
            NTSTATUS result;


            /* the <Length> member of the process's name is the string length in bytes, not 
                including the terminating null.  The <MaximumLength> member is the string's
                size in bytes, including the terminating null, and aligned to an 8-byte
                boundary. */
            pSPI->ProcessName.MaximumLength = nameSize;
            pSPI->ProcessName.Buffer = (LPWSTR)((CHAR *)pSPI + Len - nameSize);

            RtlInitAnsiString (&AnsiStr, pExeName);
            TRACE("pExeName = {'%s', length = %d, maxLength = %d}\n", AnsiStr.Buffer, AnsiStr.Length, AnsiStr.MaximumLength);


            result = RtlAnsiStringToUnicodeString (&pSPI->ProcessName, &AnsiStr, FALSE);

            /* couldn't convert the name string to unicode => clear out the name buffer
                and set its length to 0.  This is not an error case */
            if (result != STATUS_SUCCESS){
                ERR("Failed to create Unicode string! {result = 0x%08lx}\n", result);

                pSPI->ProcessName.Length = 0;
                memset(pSPI->ProcessName.Buffer, 0, pSPI->ProcessName.MaximumLength);
            }

            TRACE("pSPI->ProcessName = {'%s', length = %d, maxLength = %d}, nameSize = %ld\n", debugstr_w(pSPI->ProcessName.Buffer), pSPI->ProcessName.Length, pSPI->ProcessName.MaximumLength, nameLen * sizeof(WCHAR));
        }

        pSPI->dwOffset = Len;
    }


    if (returnLength)
        *returnLength = Len;

    return STATUS_SUCCESS;
}





static NTSTATUS collectProcessInformation(void *buf, ULONG length, ULONG *resultLength){
    ULONG                       resLength = 0;
    ULONG                       processLength;
    BOOL                        writeToBuffer = TRUE;
    PSYSTEM_PROCESS_INFORMATION pSPI = (PSYSTEM_PROCESS_INFORMATION)buf;
    PSYSTEM_PROCESS_INFORMATION pPrev;
    HANDLE                      hSnap = INVALID_HANDLE_VALUE;


    /* no buffer to write in and no return length to write in -> no point doing work => fail */
    if (buf == NULL && (resultLength == NULL || length != 0)){
        WARN("nothing to do! {buf = %p, resultLength = %p, length = %lu}\n", buf, resultLength, length);

        return STATUS_ACCESS_VIOLATION;
    }


    /* no output buffer => turn off buffer writing immediately */
    if (buf == NULL)
        writeToBuffer = FALSE;


    if (SnapCreate (&hSnap) != STATUS_SUCCESS){
        ERR("snapshot failed!\n");

        if (resultLength)
            *resultLength = 0;

        return STATUS_ACCESS_VIOLATION;
    }


    /* Iterate over processes */
    pPrev = pSPI;
    while (SnapGetNextProcess(hSnap, length - resLength, pSPI, &processLength) == STATUS_SUCCESS){

        /* accumulate the size of this process's buffer.  This value includes space for the
           main process info, all the thread info blocks, and the process's name */
        resLength += processLength;


        /* if we ran out of buffer space we can't trust the <pSPI->dwOffset> value.  Use the
           value returned through <callLength> instead. */
        if (resLength > length){            
            if (pSPI)
                pSPI->dwOffset = 0;

            length = 0;
            pSPI = NULL;
            pPrev = NULL;
        }

        else{
            pPrev = pSPI;
            pSPI = (PSYSTEM_PROCESS_INFORMATION)((char *)pSPI + pSPI->dwOffset);
        }
    }

    /* Clean up snapshot */
    NtClose (hSnap);

    if (pPrev)
        pPrev->dwOffset = 0;

    if (resultLength)
        *resultLength = resLength;


    /* the buffer wasn't sufficiently large to store the result => return size mismatch */
    if (length < resLength)
        return STATUS_INFO_LENGTH_MISMATCH;

    return STATUS_SUCCESS;
}


/******************************************************************************
 * NtQuerySystemInformation [NTDLL.@]
 * ZwQuerySystemInformation [NTDLL.@]
 *
 * ARGUMENTS:
 *  SystemInformationClass                  Index to a certain information structure    Size
 *      SystemTimeAdjustmentInformation     SYSTEM_TIME_ADJUSTMENT                      0x0008
 *      SystemCacheInformation              SYSTEM_CACHE_INFORMATION                    0x0018
 *      SystemConfigurationInformation      CONFIGURATION_INFORMATION                   ?
 *      SystemProcessInformation            SYSTEM_PROCESS_INFORMATION                  0x0160+
 *      SystemRegistryQuotaInformation      SYSTEM_REGISTRY_QUOTA_INFORMATION           0x000c
 *      SystemBasicInformation              SYSTEM_BASIC_INFORMATION                    0x002c
 *      SystemPagefileInformation           ?                                           0x0018
 *      SystemPerformanceInformation        SYSTEM_PERFORMANCE_INFORMATION              0x0138
 *      SystemProcessorCounters             ?                                           0x0600
 *  
 *  SystemInformation   caller supplies storage for the information structure
 *  Length              size of the structure
 *  ResultLength        Data written
 */
NTSTATUS WINAPI NtQuerySystemInformation(
    IN  SYSTEM_INFORMATION_CLASS    SystemInformationClass,
    OUT PVOID                       SystemInformation,
    IN  ULONG                       Length,
    OUT PULONG                      ResultLength)
{
    NTSTATUS result = STATUS_SUCCESS;


    FIXME("(0x%08x,%p,0x%08lx,%p), mostly-stub!\n",
            SystemInformationClass, SystemInformation,
            Length, ResultLength);


    /* native checks if the buffer length is 0 before checking if the buffer is NULL 
        (<ResultLength> is not modified in this case) => fail */
    if (Length == 0){
       
        /* NOTE: SystemProcessInformation handles this case differently - it returns the
            required buffer size in ResultLength */
        if (!(SystemInformationClass == SystemProcessInformation && ResultLength))
            return STATUS_INFO_LENGTH_MISMATCH;
    }

    /* native checks if this buffer is NULL on a case-by-case basis.  For now we'll just 
       do the same thing for all information classes */
    if (SystemInformation == NULL){

        /* SystemProcessInformation handles this case differently - it calculates the required
           buffer size and returns that in <ResultLength> */
        if (!(Length == 0 && SystemInformationClass == SystemProcessInformation && ResultLength)){

            if (ResultLength && SystemInformationClass != SystemProcessInformation)
                *ResultLength = 0;

            return STATUS_ACCESS_VIOLATION;
        }
    }


    switch(SystemInformationClass)
    {
        case SystemRegistryQuotaInformation:
            /* Something to do with the size of the registry             *
             * Since we don't have a size limitation, fake it            *
             * This is almost certainly wrong.                           *
             * This sets the max registry size to 32MB and the currently *
             * used amount to 2MB which is enough to make the IE 5       *
             * installer happy.                                          */
            /* NOTE: native returns 120MB for the max registry size on
                 an admin account on XP pro SP2.  The 'used' amount is
                 set to 4.3MB on a 1.5 year old system on the same account.
                 The <Reserved1> member is set to 0x16800000.  Not sure
                 what that's used for or what it means.  It's not a valid
                 memory address in the process.  Perhaps a permissions
                 mask? */
            if (SystemInformation){
                SYSTEM_REGISTRY_QUOTA_INFORMATION *quota = (SYSTEM_REGISTRY_QUOTA_INFORMATION *)SystemInformation;


                /* output buffer is too small */
                if (Length < sizeof(SYSTEM_REGISTRY_QUOTA_INFORMATION))
                    return STATUS_INFO_LENGTH_MISMATCH;


                FIXME("(0x%08x,%p,0x%08lx,%p) faking max registry size of 32 MB and used size of 2MB\n",
                      SystemInformationClass, SystemInformation, Length, ResultLength);

                quota->RegistryQuotaAllowed =   0x02000000;
                quota->RegistryQuotaUsed =      0x00200000;
                quota->Reserved1 =              (VOID *)0x16800000;

                if (ResultLength)
                    *ResultLength = sizeof(SYSTEM_REGISTRY_QUOTA_INFORMATION);
            }

            /* no buffer => access violation */
            else{

                /* native has some strange behaviour in this case => set <ResultLength> to 0 */
                if (Length == 0 && ResultLength)
                    *ResultLength = 0;

                result = STATUS_ACCESS_VIOLATION;
            }

            break;

        case SystemProcessInformation:
            TRACE("(0x%08x, %p, 0x%08lx, %p) gathering information about all running processes\n",
                    SystemInformationClass,SystemInformation,Length,ResultLength);

            result = collectProcessInformation(SystemInformation, Length, ResultLength);
            break;

        default:
            FIXME("(0x%08x,%p,0x%08lx,%p) stub\n",
                  SystemInformationClass,SystemInformation,Length,ResultLength);
            
            if (SystemInformation)
                ZeroMemory (SystemInformation, Length);
            
            if (ResultLength)
                *ResultLength = 0;

            result = STATUS_ACCESS_VIOLATION;
    }

    return result;
}

/******************************************************************************
 * NtSetSystemInformation       [NTDLL.@]
 */
NTSTATUS WINAPI NtSetSystemInformation(
	IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
	IN PVOID SystemInformation,
	IN ULONG SystemInformationLength )
{
   FIXME("(0x%08x,%p,0x%08lx) stub!\n", SystemInformationClass, SystemInformation, SystemInformationLength);

   return STATUS_SUCCESS;
}


/******************************************************************************
 *  NtCreatePagingFile		[NTDLL.@]
 *  ZwCreatePagingFile		[NTDLL.@]
 */
NTSTATUS WINAPI NtCreatePagingFile(
	IN PUNICODE_STRING PageFileName,
	IN ULONG MiniumSize,
	IN ULONG MaxiumSize,
	OUT PULONG ActualSize)
{
	FIXME("(%p(%s),0x%08lx,0x%08lx,%p),stub!\n",
	PageFileName->Buffer, debugstr_w(PageFileName->Buffer),MiniumSize,MaxiumSize,ActualSize);
	return 0;
}

/******************************************************************************
 *  NtDisplayString				[NTDLL.@]
 *
 * writes a string to the nt-textmode screen eg. during startup
 */
NTSTATUS WINAPI NtDisplayString ( PUNICODE_STRING string )
{
    STRING stringA;
    NTSTATUS ret;

    if (!(ret = RtlUnicodeStringToAnsiString( &stringA, string, TRUE )))
    {
        MESSAGE( "%.*s", stringA.Length, stringA.Buffer );
        RtlFreeAnsiString( &stringA );
    }
    return ret;
}

/******************************************************************************
 *  NtPowerInformation				[NTDLL.@]
 *
 */
NTSTATUS WINAPI NtPowerInformation (POWER_INFORMATION_LEVEL InformationLevel,
                                    PVOID lpInputBuffer,
                                    ULONG nInputBufferSize,
                                    PVOID lpOutputBuffer,
                                    ULONG nOutputBufferSize)
{
    PSYSTEM_POWER_CAPABILITIES PwrCaps =
        (PSYSTEM_POWER_CAPABILITIES)lpOutputBuffer;

    TRACE ("(%d,%p,%lu,%p,%lu)\n", InformationLevel,
           lpInputBuffer, nInputBufferSize, lpOutputBuffer, nOutputBufferSize);

    if (InformationLevel != SystemPowerCapabilities)
    {
        FIXME ("Unimplemented NtPowerInformation action: %d\n",
               InformationLevel);
        return STATUS_NOT_IMPLEMENTED;
    }

    /* hardcoded values taken from an XP desktop system; we should actually
       try to get them from Linux */
    FIXME ("partial stub\n");
    if (nOutputBufferSize < sizeof (SYSTEM_POWER_CAPABILITIES))
        return STATUS_BUFFER_TOO_SMALL;

    memset (PwrCaps, 0, sizeof (SYSTEM_POWER_CAPABILITIES));
    PwrCaps->PowerButtonPresent = TRUE;
    PwrCaps->SleepButtonPresent = TRUE;
    PwrCaps->LidPresent = FALSE;
    PwrCaps->SystemS1 = TRUE;
    PwrCaps->SystemS2 = FALSE;
    PwrCaps->SystemS3 = FALSE;
    PwrCaps->SystemS4 = TRUE;
    PwrCaps->SystemS5 = TRUE;
    PwrCaps->HiberFilePresent = TRUE;
    PwrCaps->FullWake = TRUE;
    PwrCaps->VideoDimPresent = FALSE;
    PwrCaps->ApmPresent = FALSE;
    PwrCaps->UpsPresent = FALSE;
    PwrCaps->ThermalControl = FALSE;
    PwrCaps->ProcessorThrottle = FALSE;
    PwrCaps->ProcessorMinThrottle = 100;
    PwrCaps->ProcessorMaxThrottle = 100;
    PwrCaps->DiskSpinDown = TRUE;
    PwrCaps->SystemBatteriesPresent = FALSE;
    PwrCaps->BatteriesAreShortTerm = FALSE;
    PwrCaps->AcOnLineWake = PowerSystemUnspecified;
    PwrCaps->SoftLidWake = PowerSystemUnspecified;
    PwrCaps->RtcWake = PowerSystemHibernate;
    PwrCaps->MinDeviceWakeState = PowerSystemUnspecified;
    PwrCaps->DefaultLowLatencyWake = PowerSystemUnspecified;

    return STATUS_SUCCESS;
}

/******************************************************************************
 *  NtAllocateLocallyUniqueId (NTDLL.@)
 *
 * FIXME: the server should do that
 */
NTSTATUS WINAPI NtAllocateLocallyUniqueId(PLUID Luid)
{
    static LUID luid;

    FIXME("%p (0x%08lx%08lx)\n", Luid, luid.HighPart, luid.LowPart);

    luid.LowPart++;
    if (luid.LowPart==0)
        luid.HighPart++;
    Luid->HighPart = luid.HighPart;
    Luid->LowPart = luid.LowPart;

    return STATUS_SUCCESS;
}

/******************************************************************************
 *        VerSetConditionMask   (NTDLL.@)
 */
ULONGLONG WINAPI VerSetConditionMask( ULONGLONG dwlConditionMask, DWORD dwTypeBitMask, BYTE dwConditionMask)
{
    if(dwTypeBitMask == 0)
	return dwlConditionMask;
    dwConditionMask &= 0x07;
    if(dwConditionMask == 0)
	return dwlConditionMask;

    if(dwTypeBitMask & VER_PRODUCT_TYPE)
	dwlConditionMask |= dwConditionMask << 7*3;
    else if (dwTypeBitMask & VER_SUITENAME)
	dwlConditionMask |= dwConditionMask << 6*3;
    else if (dwTypeBitMask & VER_SERVICEPACKMAJOR)
	dwlConditionMask |= dwConditionMask << 5*3;
    else if (dwTypeBitMask & VER_SERVICEPACKMINOR)
	dwlConditionMask |= dwConditionMask << 4*3;
    else if (dwTypeBitMask & VER_PLATFORMID)
	dwlConditionMask |= dwConditionMask << 3*3;
    else if (dwTypeBitMask & VER_BUILDNUMBER)
	dwlConditionMask |= dwConditionMask << 2*3;
    else if (dwTypeBitMask & VER_MAJORVERSION)
	dwlConditionMask |= dwConditionMask << 1*3;
    else if (dwTypeBitMask & VER_MINORVERSION)
	dwlConditionMask |= dwConditionMask << 0*3;
    return dwlConditionMask;
}

/* 
 * virtual memory 
 */

typedef enum _MEMORY_INFORMATION_CLASS
{
	MemoryBasicInformation

} MEMORY_INFORMATION_CLASS, *PMEMORY_INFORMATION_CLASS;

/**********************************************************************
 *  NtQueryVirtualMemory		[NTDLL.@]
 *  ZwQueryVirtualMemory		[NTDLL.@]
 *  
 *  Args: 
 *    [in] HANDLE ProcessHandle, 
 *    [in] PVOID BaseAddress, 
 *    [in] MEMORY_INFORMATION_CLASS MemoryInformationClass, 
 *    [out] PVOID Buffer, 
 *    [in] ULONG Length, 
 *    [out] PULONG ResultLength (optional)
 */
NTSTATUS WINAPI NtQueryVirtualMemory( HANDLE ProcessHandle, 
                                      PVOID BaseAddress, 
                                      MEMORY_INFORMATION_CLASS MemoryInformationClass, 
				      PVOID Buffer, 
				      ULONG Length, 
				      PULONG ResultLength )
{
  FIXME("(0x%04x, %p, %d, %p, 0x%08lx, %p), stub!\n", 
        ProcessHandle, BaseAddress, MemoryInformationClass, Buffer, Length, ResultLength);

  if (MemoryInformationClass == MemoryBasicInformation) {
    *ResultLength = VirtualQueryEx(ProcessHandle, BaseAddress, Buffer, Length);
  }

  return 0;
}
  
