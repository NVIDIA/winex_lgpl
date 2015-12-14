/*
 * Win32 processes
 *
 * Copyright 1996, 1998 Alexandre Julliard
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_PTHREADS
#include <pthread.h>
#include <signal.h>
#endif

#ifdef __APPLE__
#include <mach/mach_init.h>
#if TG_MAC_OS_X_SDK_VERSION >= TG_MAC_OS_X_VERSION_MAVERICKS
#include <mach/thread_act.h>
#include <mach/thread_status.h>
#else
#include <mach/i386/thread_act.h>
#include <mach/i386/thread_status.h>
#endif
#include <mach/vm_map.h>
#include <servers/bootstrap.h>
#endif

#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "wine/exception.h"
#include "wine/library.h"
#include "drive.h"
#include "wine/module.h"
#include "wine/file.h"
#include "thread.h"
#include "winerror.h"
#include "wincon.h"
#include "wine/server.h"
#include "options.h"
#include "wine/hardware.h"
#include "wine/debug.h"
#include "drive.h"
#include "wine/sxs.h"

DEFAULT_DEBUG_CHANNEL(process);
DECLARE_DEBUG_CHANNEL(relay);
DECLARE_DEBUG_CHANNEL(win32);

struct _ENVDB;

/* Win32 process database */
typedef struct _PDB
{
    LONG             header[2];        /* 00 Kernel object header */
    HMODULE          module;           /* 08 Main exe module (NT) */
    void            *event;            /* 0c Pointer to an event object (unused) */
    DWORD            exit_code;        /* 10 Process exit code */
    DWORD            unknown2;         /* 14 Unknown */
    HANDLE           heap;             /* 18 Default process heap */
    HANDLE           mem_context;      /* 1c Process memory context */
    DWORD            flags;            /* 20 Flags */
    void            *pdb16;            /* 24 DOS PSP */
    WORD             PSP_sel;          /* 28 Selector to DOS PSP */
    WORD             imte;             /* 2a IMTE for the process module */
    WORD             threads;          /* 2c Number of threads */
    WORD             running_threads;  /* 2e Number of running threads */
    WORD             free_lib_count;   /* 30 Recursion depth of FreeLibrary calls */
    WORD             ring0_threads;    /* 32 Number of ring 0 threads */
    HANDLE           system_heap;      /* 34 System heap to allocate handles */
    HTASK            task;             /* 38 Win16 task */
    void            *mem_map_files;    /* 3c Pointer to mem-mapped files */
    struct _ENVDB   *env_db;           /* 40 Environment database */
    void            *handle_table;     /* 44 Handle table */
    struct _PDB     *parent;           /* 48 Parent process */
    void            *modref_list;      /* 4c MODREF list */
    void            *thread_list;      /* 50 List of threads */
    void            *debuggee_CB;      /* 54 Debuggee context block */
    void            *local_heap_free;  /* 58 Head of local heap free list */
    DWORD            unknown4;         /* 5c Unknown */
    CRITICAL_SECTION crit_section;     /* 60 Critical section */
    DWORD            unknown5[3];      /* 78 Unknown */
    void            *console;          /* 84 Console */
    DWORD            tls_bits[2];      /* 88 TLS in-use bits */
    DWORD            process_dword;    /* 90 Unknown */
    struct _PDB     *group;            /* 94 Process group */
    void            *exe_modref;       /* 98 MODREF for the process EXE */
    void            *top_filter;       /* 9c Top exception filter */
    DWORD            priority;         /* a0 Priority level */
    HANDLE           heap_list;        /* a4 Head of process heap list */
    void            *heap_handles;     /* a8 Head of heap handles list */
    DWORD            unknown6;         /* ac Unknown */
    void            *console_provider; /* b0 Console provider (??) */
    WORD             env_selector;     /* b4 Selector to process environment */
    WORD             error_mode;       /* b6 Error mode */
    HANDLE           load_done_evt;    /* b8 Event for process loading done */
    void            *UTState;          /* bc Head of Univeral Thunk list */
    DWORD            unknown8;         /* c0 Unknown (NT) */
    LCID             locale;           /* c4 Locale to be queried by GetThreadLocale (NT) */
} PDB;

PDB current_PDB;
PEB current_PEB;

static RTL_BITMAP TlsBitmap;
static RTL_BITMAP TlsExpansionBitmap;

/* Process flags */
#define PDB32_DEBUGGED      0x0001  /* Process is being debugged */
#define PDB32_WIN16_PROC    0x0008  /* Win16 process */
#define PDB32_DOS_PROC      0x0010  /* Dos process */
#define PDB32_CONSOLE_PROC  0x0020  /* Console process */
#define PDB32_FILE_APIS_OEM 0x0040  /* File APIs are OEM */
#define PDB32_WIN32S_PROC   0x8000  /* Win32s process */

static int app_argc;   /* argc/argv seen by the application */
static char **app_argv;
static WCHAR **app_wargv;
static char main_exe_name[MAX_PATH];
static char *main_exe_name_ptr = main_exe_name;
static HANDLE main_exe_file;

struct timeval server_starttime;

/* memory/environ.c */
extern struct _ENVDB *ENV_BuildEnvironment(void);
extern BOOL ENV_BuildCommandLine( char **argv );
extern STARTUPINFOA current_startupinfo;
extern void PROCESSOR_Init(void);

/* misc/options.c */
extern char** modify_commandline_from_config( char* argv[], int* pargc );

#ifndef USE_PTHREADS
/* scheduler/pthread.c */
extern void PTHREAD_init_done(void);
#endif

/* scheduler/client.c */
extern int can_exec_binary(const char *binary);
extern char *find_binary_path( const char *oldcwd, const char *binary );
extern char *find_preloader_path(const char *oldcwd);

extern BOOL MAIN_MainInit(void);

/* Initialization routines */
extern void THREAD_Init(void);
extern void SYSDEPS_Init(void);
extern void VIRTUAL_Init(void);
extern void VIRTUAL_Setup(void);
extern void INSTR_Init(void);
extern void LOADER_Init(void);
extern void TIME_Init(void);
extern void PROCESS_Init(void);
extern void INIT_CritSects(void);
extern void HEAP_Init (BOOL);

#ifdef USE_PTHREADS
#define CHECK_ERR( op ) do { int err = (op); if( err ) { fprintf( stderr, "\"" # op "\" failed at %s:%d with %s\n",  __FILE__, __LINE__, strerror( err ) ); goto pthread_error; } } while(0)
#endif

typedef WORD (WINAPI *pUserSignalProc)( UINT, DWORD, DWORD, HMODULE16 );

/***********************************************************************
 *           PROCESS_CallUserSignalProc
 *
 * FIXME:  Some of the signals aren't sent correctly!
 *
 * The exact meaning of the USER signals is undocumented, but this
 * should cover the basic idea:
 *
 * USIG_DLL_UNLOAD_WIN16
 *     This is sent when a 16-bit module is unloaded.
 *
 * USIG_DLL_UNLOAD_WIN32
 *     This is sent when a 32-bit module is unloaded.
 *
 * USIG_DLL_UNLOAD_ORPHANS
 *     This is sent after the last Win3.1 module is unloaded,
 *     to allow removal of orphaned menus.
 *
 * USIG_FAULT_DIALOG_PUSH
 * USIG_FAULT_DIALOG_POP
 *     These are called to allow USER to prepare for displaying a
 *     fault dialog, even though the fault might have happened while
 *     inside a USER critical section.
 *
 * USIG_THREAD_INIT
 *     This is called from the context of a new thread, as soon as it
 *     has started to run.
 *
 * USIG_THREAD_EXIT
 *     This is called, still in its context, just before a thread is
 *     about to terminate.
 *
 * USIG_PROCESS_CREATE
 *     This is called, in the parent process context, after a new process
 *     has been created.
 *
 * USIG_PROCESS_INIT
 *     This is called in the new process context, just after the main thread
 *     has started execution (after the main thread's USIG_THREAD_INIT has
 *     been sent).
 *
 * USIG_PROCESS_LOADED
 *     This is called after the executable file has been loaded into the
 *     new process context.
 *
 * USIG_PROCESS_RUNNING
 *     This is called immediately before the main entry point is called.
 *
 * USIG_PROCESS_EXIT
 *     This is called in the context of a process that is about to
 *     terminate (but before the last thread's USIG_THREAD_EXIT has
 *     been sent).
 *
 * USIG_PROCESS_DESTROY
 *     This is called after a process has terminated.
 *
 *
 * The meaning of the dwFlags bits is as follows:
 *
 * USIG_FLAGS_WIN32
 *     Current process is 32-bit.
 *
 * USIG_FLAGS_GUI
 *     Current process is a (Win32) GUI process.
 *
 * USIG_FLAGS_FEEDBACK
 *     Current process needs 'feedback' (determined from the STARTUPINFO
 *     flags STARTF_FORCEONFEEDBACK / STARTF_FORCEOFFFEEDBACK).
 *
 * USIG_FLAGS_FAULT
 *     The signal is being sent due to a fault.
 */
void PROCESS_CallUserSignalProc( UINT uCode, HMODULE16 hModule )
{
    DWORD dwFlags = 0;
    HMODULE user;
    pUserSignalProc proc;

    if (!(user = GetModuleHandleA( "user32.dll" ))) return;
    if (!(proc = (pUserSignalProc)GetProcAddress( user, "UserSignalProc" ))) return;

    /* Determine dwFlags */

    if ( !(current_PDB.flags & PDB32_WIN16_PROC) ) dwFlags |= USIG_FLAGS_WIN32;
    if ( !(current_PDB.flags & PDB32_CONSOLE_PROC) ) dwFlags |= USIG_FLAGS_GUI;

    if ( dwFlags & USIG_FLAGS_GUI )
    {
        /* Feedback defaults to ON */
        if ( !(current_startupinfo.dwFlags & STARTF_FORCEOFFFEEDBACK) )
            dwFlags |= USIG_FLAGS_FEEDBACK;
    }
    else
    {
        /* Feedback defaults to OFF */
        if (current_startupinfo.dwFlags & STARTF_FORCEONFEEDBACK)
            dwFlags |= USIG_FLAGS_FEEDBACK;
    }

    /* Call USER signal proc */

    if ( uCode == USIG_THREAD_INIT || uCode == USIG_THREAD_EXIT )
        proc( uCode, GetCurrentThreadId(), dwFlags, hModule );
    else
        proc( uCode, GetCurrentProcessId(), dwFlags, hModule );
}


/***********************************************************************
 *           process_init
 *
 * Main process initialisation code
 */
static BOOL process_init( char *argv[] )
{
    BOOL ret;
    int create_flags = 0;
    int unique_msg_id;
    pid_t wineserver_pid;
    int use_new_allocator = 0;
#ifdef __APPLE__
    char PortName[BOOTSTRAP_MAX_NAME_LEN];
    mach_port_t ServerPort;
    mach_msg_header_t msg;
#endif

    /* Initialize the boot thread to have a TEB */
    THREAD_Init();
    
    /* Initialize Critical Sections */
    INIT_CritSects();

    /* Initialize the virtual memory code */
    SYSDEPS_Init();
    VIRTUAL_Init();
    
    /* Setup the PEB and loader lock */
    PROCESS_Init();
    LOADER_Init();

    /* Initialize the SxS assembly loader */
    SXS_Init();
    
    /* setup the timer critical sections */
    TIME_Init();
    
    /* store the program name */
    argv0 = argv[0];
    app_argv = argv;

    /* Fill the initial process structure */
    current_PDB.exit_code       = STILL_ACTIVE;
    current_PDB.threads         = 1;
    current_PDB.running_threads = 1;
    current_PDB.ring0_threads   = 1;
    current_PDB.group           = &current_PDB;
    current_PDB.priority        = 8;  /* Normal */

    /* initialize the process environment structure TLS fields */
    current_PEB.TlsBitmap = &TlsBitmap;
    RtlInitializeBitMap(current_PEB.TlsBitmap,(LPBYTE)current_PEB.TlsBitmapBits,
                        sizeof(current_PEB.TlsBitmapBits) * 8);
    RtlClearAllBits(current_PEB.TlsBitmap);
    current_PEB.TlsExpansionBitmap = &TlsExpansionBitmap;
    RtlInitializeBitMap(current_PEB.TlsExpansionBitmap, 
                        (LPBYTE)current_PEB.TlsExpansionBitmapBits,
                        sizeof(current_PEB.TlsExpansionBitmapBits) * 8);
    RtlClearAllBits(current_PEB.TlsExpansionBitmap);

    /* Setup the server connection */
    CLIENT_InitServer();

    /* Retrieve startup info from the server */
    SERVER_START_REQ( init_process )
    {
        req->ldt_copy  = &wine_ldt_copy;
        wine_server_set_reply( req, main_exe_name, sizeof(main_exe_name)-1 );
        if ((ret = !wine_server_call_err( req )))
        {
            size_t len = wine_server_reply_size( reply );
            main_exe_name[len] = 0;
            main_exe_file = reply->exe_file;
            create_flags  = reply->create_flags;
            current_startupinfo.dwFlags     = reply->start_flags;
            server_starttime.tv_sec         = reply->server_start_sec;
            server_starttime.tv_usec        = reply->server_start_usec;
            current_startupinfo.wShowWindow = reply->cmd_show;
            current_startupinfo.hStdInput   = reply->hstdin;
            current_startupinfo.hStdOutput  = reply->hstdout;
            current_startupinfo.hStdError   = reply->hstderr;
            wineserver_pid                  = reply->wineserver_pid;
            unique_msg_id                   = reply->unique_msg_id;
            use_new_allocator               = reply->use_new_allocator;
        }
    }
    SERVER_END_REQ;

    if (!ret)
       return FALSE;

#ifdef __APPLE__
    /* Send port */
    snprintf (PortName, sizeof (PortName),
              "com.transgaming.server:%d", wineserver_pid);
    if (bootstrap_look_up (bootstrap_port, PortName, &ServerPort) !=
        KERN_SUCCESS)
       server_protocol_error ("Unable to retrieve bootstrap port\n");

    msg.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_MOVE_SEND,
                                    MACH_MSG_TYPE_COPY_SEND);
    msg.msgh_size = sizeof (mach_msg_header_t);
    msg.msgh_remote_port = ServerPort;
    msg.msgh_local_port = mach_task_self ();
    msg.msgh_id = unique_msg_id;

    mach_msg (&msg, MACH_SEND_MSG, msg.msgh_size, 0, MACH_PORT_NULL,
              MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    SERVER_START_REQ(thread_port_sent)
    {
       req->unique_msg_id = unique_msg_id;
       req->entry         = NULL;
       req->is_process    = 1;
       wine_server_call (req);
    }
    SERVER_END_REQ;
#endif

    /* Create the process heap */
    HEAP_Init (use_new_allocator);
    current_PDB.heap = HeapCreate( HEAP_GROWABLE, 0, 0 );
    current_PEB.ProcessHeap = current_PDB.heap;

    if (create_flags == 0 &&
	current_startupinfo.hStdInput  == 0 &&
	current_startupinfo.hStdOutput == 0 &&
	current_startupinfo.hStdError  == 0)
    {
	/* no parent, and no new console requested.  Windows deals with this by sending all output 
	   to the bitbucket, unless the user has explicitly redirected the output to a file.  We need 
	   to do the same to avoid spewing output when we shouldn't. */
	   
        int fd = open( "/dev/null", O_RDWR );

        /* Always pass through stdinput */
        SetStdHandle( STD_INPUT_HANDLE,  FILE_DupUnixHandle( STDIN_FILENO, GENERIC_READ|SYNCHRONIZE, TRUE ));
        
        if (isatty(STDOUT_FILENO) && (fd != -1))
	    SetStdHandle( STD_OUTPUT_HANDLE, FILE_DupUnixHandle( fd, GENERIC_WRITE|SYNCHRONIZE, TRUE ));
	else
	    /* If we end up here, std output has been redirected to a file so allow the output to pass through */
	    SetStdHandle( STD_OUTPUT_HANDLE, FILE_DupUnixHandle( STDOUT_FILENO, GENERIC_WRITE|SYNCHRONIZE, TRUE ));

        if (isatty(STDERR_FILENO) && (fd != -1))
	    SetStdHandle( STD_ERROR_HANDLE,  FILE_DupUnixHandle( fd, GENERIC_WRITE|SYNCHRONIZE, TRUE ));
	else
	    /* If we end up here, std err has been redirected to a file so allow the output to pass through */
	    SetStdHandle( STD_ERROR_HANDLE,  FILE_DupUnixHandle( STDERR_FILENO, GENERIC_WRITE|SYNCHRONIZE, TRUE ));

        if (fd != -1)
            close(fd);        
    }
    else if (!(create_flags & (DETACHED_PROCESS|CREATE_NEW_CONSOLE)))
    {
	SetStdHandle( STD_INPUT_HANDLE,  current_startupinfo.hStdInput  );
	SetStdHandle( STD_OUTPUT_HANDLE, current_startupinfo.hStdOutput );
	SetStdHandle( STD_ERROR_HANDLE,  current_startupinfo.hStdError  );
    }

#if !defined( USE_PTHREADS )
    /* 
     * Now we can use the wine pthreads routines. If we're using pthreads,
     * pthreads is actually providing this functionality for glibc
     */
    PTHREAD_init_done();
#endif    

    /* Configure the virtual memory code */
    VIRTUAL_Setup();

    /* Copy the parent environment */
    if (!(current_PDB.env_db = ENV_BuildEnvironment())) return FALSE;

    /* Parse command line arguments */
    OPTIONS_ParseOptions( argv );
    app_argc = 0;
    while (argv[app_argc]) app_argc++;
    
    /* Initialize instruction emulation if required */
    INSTR_Init();
    
    PROCESSOR_Init();
    
    ret = MAIN_MainInit();

#if defined( __APPLE__ )
/* this is set automatically by the packaged version of a .app on MacOSX. 
 * Should not be set manually in the environment. */
    char* gameName = getenv ("CEDEGAGAMENAME");
    if (gameName)
    {
	app_argc++;
        argv = realloc(argv, sizeof(char*) * (app_argc + 1));
	argv[app_argc-1] = gameName;
        argv[app_argc] = NULL;
    }
#endif

    /* Bit of a kludge to stuff in cmdline from config file */
    app_argv = modify_commandline_from_config( argv, &app_argc );

    if (create_flags & CREATE_NEW_CONSOLE)
	AllocConsole();
    return ret;
}


static inline BOOL IS_OPTION_TRUE(ch)
{
    return (ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1';
}

static BOOL is_process_disabled(void)
{
    char buffer[MAX_PATH+16];
    HKEY appkey = 0;
    BOOL disabled = FALSE;
    DWORD size;
    DWORD error;

    if (GetModuleFileName16( GetCurrentTask(), buffer, MAX_PATH ) ||
        ((error = GetModuleFileNameA( 0, buffer, MAX_PATH )) != 0 && error != MAX_PATH))
    {
        HKEY tmpkey;
        char *p, *appname = buffer;
        if ((p = strrchr( appname, '/' ))) appname = p + 1;
        if ((p = strrchr( appname, '\\' ))) appname = p + 1;
        strcat( appname, "\\transgaming" );
        if (!RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\AppDefaults", &tmpkey ))
        {
            if (RegOpenKeyA( tmpkey, appname, &appkey )) appkey = 0;
            RegCloseKey( tmpkey );
        }
    }

    else
        ERR("could not retrieve the module file name (reason: '%s')\n", error == 0 ? "bad module" : "buffer too small");


    size = sizeof(buffer);
    if (appkey && !RegQueryValueExA( appkey, "Disable", 0, NULL, (LPBYTE)buffer, &size ))
        disabled = IS_OPTION_TRUE( buffer[0] );

    return disabled;
}

/***********************************************************************
 *           start_process
 *
 * Startup routine of a new process. Runs on the new process stack.
 */
static void start_process(void)
{
    int debugged, console_app;
    LPTHREAD_START_ROUTINE entry;
    WINE_MODREF *wm;
    HFILE main_file = main_exe_file;
    int use_shm;
    int shm_type;
    size_t shm_size;
    void *shm_addr;
    OSVERSIONINFOEXA VerInfo;

    /* use original argv[0] as name for the main module */
    if (!main_exe_name[0])
    {
        if (!GetLongPathNameA( full_argv0, main_exe_name, sizeof(main_exe_name) ))
            lstrcpynA( main_exe_name, full_argv0, sizeof(main_exe_name) );
    }

    if (main_file)
    {
        UINT drive_type;
	char main_exe_drive_name[4] = "_:\\";
	if( main_exe_name[1] == ':' )
	    main_exe_drive_name[0] = main_exe_name[0];
	else
	    FIXME("unknown drive letter for file %s", main_exe_name);
	drive_type = GetDriveTypeA( main_exe_drive_name );
        /* don't keep the file handle open on removable media */
        if (drive_type == DRIVE_REMOVABLE || drive_type == DRIVE_CDROM) main_file = 0;
    }

    /* Retrieve entry point address */
    entry = (LPTHREAD_START_ROUTINE)((char*)current_PDB.module +
                         PE_HEADER(current_PDB.module)->OptionalHeader.AddressOfEntryPoint);
    console_app = (PE_HEADER(current_PDB.module)->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI);
    if (console_app) current_PDB.flags |= PDB32_CONSOLE_PROC;

    if (console_app)
    {
       /* Ensure that console app output is properly directed to unix stdin/stdout.  Default in process_init 
          is to assume a GUI app, and redirect these to the bitbucket. Doing this below is probably not 100%
          correct, as it should probably interface with the console APIs in some ways, in case a console has
          already been created for this, etc... */
       SetStdHandle( STD_OUTPUT_HANDLE, FILE_DupUnixHandle( STDOUT_FILENO, GENERIC_WRITE|SYNCHRONIZE, TRUE ));
       SetStdHandle( STD_ERROR_HANDLE,  FILE_DupUnixHandle( STDERR_FILENO, GENERIC_WRITE|SYNCHRONIZE, TRUE ));
    }       

    /* create the main modref and load dependencies */
    /* Note, we should do this before init_process_done. If we're created suspended,
     * calling init_process_done will suspend us, and we want stuff like kernel32 to
     * have been loaded before the parent process starts messing around with us. */
    if (!(wm = PE_CreateModule( current_PDB.module, main_exe_name, 0, 0, FALSE )))
        goto error;
    wm->refCount++;

    /* ensure kernel32 is initialized, but hold off on initializing anything else */
    if ((wm = MODULE_FindModule( "kernel32.dll" )))
        MODULE_DllProcessAttach( wm, (LPVOID)1 );

    /* Signal the parent process to continue */
    SERVER_START_REQ( init_process_done )
    {
        req->module    = (void *)current_PDB.module;
        req->base_size = PE_HEADER(current_PDB.module)->OptionalHeader.SizeOfImage;
        req->entry    = entry;
        /* API requires a double indirection */
        req->name     = &main_exe_name_ptr;
        req->exe_file = main_file;
        req->gui      = !console_app;
        wine_server_add_data( req, main_exe_name, strlen(main_exe_name) );
        wine_server_call( req );
        debugged = reply->debugged;
        use_shm = reply->shm_server;
        shm_size = reply->shm_size;
        shm_addr = reply->shm_addr;
        shm_type = reply->shm_type;
    }
    SERVER_END_REQ;

    /* Install signal handlers; this cannot be done before, since we cannot
     * send exceptions to the debugger before the create process event that
     * is sent by REQ_INIT_PROCESS_DONE */
    if (!SIGNAL_Init()) goto error;

    CLIENT_InitServerDone();

    if (main_exe_file) CloseHandle( main_exe_file ); /* we no longer need it */

    if (use_shm) setup_shared_memory_wineserver (shm_type, shm_size, shm_addr);

    VerInfo.dwOSVersionInfoSize = sizeof (VerInfo);
    GetVersionExA ((LPOSVERSIONINFOA)&VerInfo);
    if (VerInfo.dwPlatformId >= VER_PLATFORM_WIN32_NT)
    {
       NtCurrentTeb ()->process.PEB = &current_PEB;
       NtCurrentTeb ()->has_peb = 1;
       current_PEB.OSMajorVersion = VerInfo.dwMajorVersion;
       current_PEB.OSMinorVersion = VerInfo.dwMinorVersion;
       current_PEB.OSBuildNumber = VerInfo.dwBuildNumber;
       current_PEB.OSPlatformId = VerInfo.dwPlatformId;
    }

    MODULE_DllProcessAttach( NULL, (LPVOID)1 );

    /* Note: The USIG_PROCESS_CREATE signal is supposed to be sent in the
     *       context of the parent process.  Actually, the USER signal proc
     *       doesn't really care about that, but it *does* require that the
     *       startup parameters are correctly set up, so that GetProcessDword
     *       works.  Furthermore, before calling the USER signal proc the
     *       16-bit stack must be set up, which it is only after TASK_Create
     *       in the case of a 16-bit process. Thus, we send the signal here.
     */
    PROCESS_CallUserSignalProc( USIG_PROCESS_CREATE, 0 );
    PROCESS_CallUserSignalProc( USIG_THREAD_INIT, 0 );
    PROCESS_CallUserSignalProc( USIG_PROCESS_INIT, 0 );
    PROCESS_CallUserSignalProc( USIG_PROCESS_LOADED, 0 );
    /* Call UserSignalProc ( USIG_PROCESS_RUNNING ... ) only for non-GUI win32 apps */
    if (console_app) PROCESS_CallUserSignalProc( USIG_PROCESS_RUNNING, 0 );

    if (TRACE_ON(relay))
        DPRINTF( "%04lx:Starting process %s (entryproc=%p)\n",
                 GetCurrentThreadId(), main_exe_name, entry );
    if (debugged) DbgBreakPoint();
    if (is_process_disabled()) ExitThread( 0 );
    SetLastError(0);  /* clear error code */
    ExitThread( entry(&current_PEB) );

 error:
    ExitProcess( GetLastError() );
}

#ifdef USE_PTHREADS

struct pthread_startup_info
{
    TEB *teb;
    pid_t old_unix_tid;
};

/* Setup the first pthread of the new process. */
static void *start_process_pthread(void *param)
{
    struct pthread_startup_info* psi = param;
    pid_t new_unix_tid = wine_gettid_or_pid();
    int unique_msg_id;
    pid_t wineserver_pid;
#ifdef __APPLE__
    char PortName[BOOTSTRAP_MAX_NAME_LEN];
    mach_port_t ServerPort;
    mach_msg_header_t msg;
#endif

    SYSDEPS_SetCurThread( psi->teb );

    /*
     * We've started communication with the wineserver
     * as psi->old_unix_tid but this thread has a different unix
     * tid. This will cause a problem if the wineserver needs
     * to communicate with this thread using the old_unix_tid.
     * Update so that ptrace and kills will affect the correct
     * thread.
     */
    SERVER_START_REQ( update_thread_tid )
    {
        req->old_unix_tid = psi->old_unix_tid;
        req->new_unix_tid = new_unix_tid;
        wine_server_call(req);
        unique_msg_id = reply->unique_msg_id;
        wineserver_pid = reply->wineserver_pid;
    }
    SERVER_END_REQ;

#ifdef __APPLE__
    /* Send port */
    snprintf (PortName, sizeof (PortName),
              "com.transgaming.server:%d", wineserver_pid);
    if (bootstrap_look_up (bootstrap_port, PortName, &ServerPort) !=
        KERN_SUCCESS)
       server_protocol_error ("Unable to retrieve bootstrap port\n");

    msg.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_MOVE_SEND,
                                    MACH_MSG_TYPE_COPY_SEND);
    msg.msgh_size = sizeof (mach_msg_header_t);
    msg.msgh_remote_port = ServerPort;
    msg.msgh_local_port = mach_thread_self ();
    msg.msgh_id = unique_msg_id;

    mach_msg (&msg, MACH_SEND_MSG, msg.msgh_size, 0, MACH_PORT_NULL,
              MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    SERVER_START_REQ(thread_port_sent)
    {
       req->unique_msg_id = unique_msg_id;
       req->entry         = NULL;
       req->is_process    = 0;
       wine_server_call (req);
    }
    SERVER_END_REQ;
#endif

    /* Just in case start_process returns due to exception, we setup a filter */
    __TRY
    {
        start_process();
    }
    __EXCEPT(UnhandledExceptionFilter)
    {
        TerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    SYSDEPS_ExitThread(0);  /* should never get here */
}
#endif /* USE_PTHREADS */


/***********************************************************************
 *           open_winelib_app
 *
 * Try to open the Winelib app .so file based on argv[0] or WINEPRELOAD.
 */
void *open_winelib_app( char *argv[] )
{
    void *ret = NULL;
    char *tmp;
    const char *name;
    char errStr[100];

    if ((name = getenv( "WINEPRELOAD" )))
    {
        if (!(ret = wine_dll_load_main_exe( name, 0, errStr, sizeof(errStr) )))
        {
            MESSAGE( "%s: could not load '%s' as specified in the WINEPRELOAD environment variable: %s\n",
                     argv[0], name, errStr );
            ExitProcess(1);
        }
    }
    else
    {
        const char *argv0 = main_exe_name;
        if (!*argv0)
        {
            /* if argv[0] is "wine", don't try to load anything */
            argv0 = argv[0];
            if (!(name = strrchr( argv0, '/' ))) name = argv0;
            else name++;
            if (!strcmp( name, "wine" )) return NULL;
        }

        /* now try argv[0] with ".so" appended */
        if ((tmp = HeapAlloc( GetProcessHeap(), 0, strlen(argv0) + 4 )))
        {
            strcpy( tmp, argv0 );
            strcat( tmp, ".so" );
            /* search in PATH only if there was no '/' in argv[0] */
            ret = wine_dll_load_main_exe( tmp, (name == argv0), errStr, sizeof(errStr) );
            if (!ret && !argv[1])
            {
                /* if no argv[1], this will be better than displaying usage */
                MESSAGE( "%s: could not load library '%s' as Winelib application: %s\n",
                         argv[0], tmp, errStr );
                ExitProcess(1);
            }
            HeapFree( GetProcessHeap(), 0, tmp );
        }
    }
    return ret;
}

/***********************************************************************
 *           PROCESS_InitWine
 *
 * Wine initialisation: load and start the main exe file.
 */
void PROCESS_InitWine( int argc, char *argv[], LPSTR win16_exe_name, HANDLE *win16_exe_file )
{
    DWORD stack_size = 0;

    /* Initialize everything */
    if (!process_init( argv )) exit(1);

    if (open_winelib_app( app_argv )) goto found; /* try to open argv[0] as a winelib app */

    app_argv++;  /* remove argv[0] (wine itself) */
    app_argc--;

    if (!main_exe_name[0])
    {
        char *argv_noquotes;
        DWORD argv_len;

        if (!app_argv[0]) OPTIONS_Usage();

        /* Remove any quotes around the file name, without modifying argv[0] */
        argv_len = strlen(app_argv[0]);
        if ((argv_noquotes = HeapAlloc( GetProcessHeap(), 0, argv_len+1 )))
        {
            char *p = app_argv[0];
            char *q;
            /* If there are quotes around the *full* argument only, remove them */
            if ((*p == '\"') && (q = strchr( p + 1, '\"' )) && (q == (p+argv_len-1)))
            {
                strncpy(argv_noquotes, p+1, argv_len-2);
                argv_noquotes[argv_len-2] = 0;
            }
            else
                strcpy(argv_noquotes, p);

            /* open the exe file */
            if (!SearchPathA( NULL, argv_noquotes, ".exe", sizeof(main_exe_name), main_exe_name, NULL ) &&
                !SearchPathA( NULL, argv_noquotes, NULL, sizeof(main_exe_name), main_exe_name, NULL ))
            {
                MESSAGE( "%s: cannot find '%s'\n", argv0, app_argv[0] );
                goto error;
            }
            HeapFree( GetProcessHeap(), 0, argv_noquotes);
        }
    }

    if (!main_exe_file)
    {
        if ((main_exe_file = CreateFileA( main_exe_name, GENERIC_READ, FILE_SHARE_READ,
                                          NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE)
        {
            MESSAGE( "%s: cannot open '%s'\n", argv0, main_exe_name );
            goto error;
        }
    }

    /* first try Win32 format; this will fail if the file is not a PE binary */
    if ((current_PDB.module = PE_LoadImage( main_exe_file, main_exe_name, 0 )))
    {
        if (PE_HEADER(current_PDB.module)->FileHeader.Characteristics & IMAGE_FILE_DLL)
            ExitProcess( ERROR_BAD_EXE_FORMAT );
        goto found;
    }

    /* it must be 16-bit or DOS format */
    NtCurrentTeb()->tibflags &= ~TEBF_WIN32;
    current_PDB.flags |= PDB32_WIN16_PROC;
    strcpy( win16_exe_name, main_exe_name );
    main_exe_name[0] = 0;
    *win16_exe_file = main_exe_file;
    main_exe_file = 0;

 found:
    /* build command line */
    if (!ENV_BuildCommandLine( app_argv )) goto error;

#if 1
    /* if this is a subprocess launched inside a mount point,
     * get out of it now. (PROCESS_Create should chdir back to
     * the mount point for any further subprocesses if needed.)
     * Provided the parent process also stays out of the mount
     * point, this ought to avoid cwds blocking umount attempts,
     * at least. Actual open files are another matter, of course. */
    chdir("/");
#endif

    /* create 32-bit module for main exe */
    if (!(current_PDB.module = BUILTIN32_LoadExeModule( current_PDB.module ))) goto error;
    stack_size = PE_HEADER(current_PDB.module)->OptionalHeader.SizeOfStackReserve;

    /* allocate main thread stack */
    if (!THREAD_InitStack( NtCurrentTeb(), stack_size )) goto error;

    DRIVE_ServerInit();

#if defined (USE_PTHREADS)
    {
	pthread_t main_thread;
        struct pthread_startup_info psi;
	pthread_attr_t attr;
	TEB* teb = NtCurrentTeb();

        psi.teb = teb;
	psi.old_unix_tid = wine_gettid_or_pid();

	CHECK_ERR( pthread_attr_init(&attr) );

	CHECK_ERR( pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) );

        /* Setup valid stack, including alt signal stack */
        /* FIXME: Should add to port.c if doesn't exist */
# ifdef HAVE_PTHREAD_ATTR_SETSTACK
        CHECK_ERR( pthread_attr_setstack(&attr, teb->stack_low,
                               (ULONG_PTR)teb->stack_top
                                 - (ULONG_PTR)teb->stack_low) );
# else
	CHECK_ERR( pthread_attr_setstackaddr(&attr, teb->stack_top) );
	CHECK_ERR( pthread_attr_setstacksize(&attr,(ULONG_PTR)teb->stack_top
				          - (ULONG_PTR)teb->stack_low) );
#endif

#if defined(__i386__)
        /* On i386 we will have set the teb to be in %fs. It has not been
	 * initialized into anything pthread specific so there is nothing
	 * to do on other platforms.
	 */
        __set_fs(0);
#endif
	
        /* For LinuxThreads, this will also create the manager thread. */
        CHECK_ERR( pthread_create(&main_thread, &attr, start_process_pthread, &psi) );
	
        CHECK_ERR( pthread_attr_destroy(&attr) );

        SYSDEPS_PTHREADS_RunMainThread(); /* doesn't return */

pthread_error:
        /* Shouldn't ever reach here... */
        exit( 1 );
    }
#else /* USE_PTHREADS */

      /* switch to the new stack */
      SYSDEPS_SwitchToThreadStack( start_process );
#endif

 error:
    ExitProcess( GetLastError() );
}


/***********************************************************************
 *              __wine_get_main_args (NTDLL.@)
 *
 * Return the argc/argv that the application should see.
 * Used by the startup code generated in the .spec.c file.
 */
int __wine_get_main_args( char ***argv )
{
    *argv = app_argv;
    return app_argc;
}


/***********************************************************************
 *              __wine_get_wmain_args (NTDLL.@)
 *
 * Same as __wine_get_main_args but for Unicode.
 */
int __wine_get_wmain_args( WCHAR ***argv )
{
    if (!app_wargv)
    {
        int i;
        WCHAR *p;
        DWORD total = 0;

        for (i = 0; i < app_argc; i++)
            total += MultiByteToWideChar( CP_ACP, 0, app_argv[i], -1, NULL, 0 );

        app_wargv = HeapAlloc( GetProcessHeap(), 0,
                                    total * sizeof(WCHAR) + (app_argc + 1) * sizeof(*app_wargv) );
        p = (WCHAR *)(app_wargv + app_argc + 1);
        for (i = 0; i < app_argc; i++)
        {
            DWORD len = MultiByteToWideChar( CP_ACP, 0, app_argv[i], -1, p, total );
            app_wargv[i] = p;
            p += len;
            total -= len;
        }
        app_wargv[app_argc] = NULL;
    }
    *argv = app_wargv;
    return app_argc;
}


/***********************************************************************
 *           build_argv
 *
 * Build an argv array from a command-line.
 * The command-line is modified to insert nulls.
 * 'reserved' is the number of args to reserve before the first one.
 */
static char **build_argv( char *cmdline, int reserved )
{
    int argc;
    char** argv;
    char *arg,*s,*d;
    int in_quotes,bcount;

    argc=reserved+1;
    bcount=0;
    in_quotes=0;
    s=cmdline;
    while (1) {
        if (*s=='\0' || ((*s==' ' || *s=='\t') && !in_quotes)) {
            /* space */
            argc++;
            /* skip the remaining spaces */
            while (*s==' ' || *s=='\t') {
                s++;
            }
            if (*s=='\0')
                break;
            bcount=0;
            continue;
        } else if (*s=='\\') {
            /* '\', count them */
            bcount++;
        } else if ((*s=='"') && ((bcount & 1)==0)) {
            /* unescaped '"' */
            in_quotes=!in_quotes;
            bcount=0;
        } else {
            /* a regular character */
            bcount=0;
        }
        s++;
    }
    argv=malloc(argc*sizeof(*argv));
    if (!argv)
        return NULL;

    arg=d=s=cmdline;
    bcount=0;
    in_quotes=0;
    argc=reserved;
    while (*s) {
        if ((*s==' ' || *s=='\t') && !in_quotes) {
            /* Close the argument and copy it */
            *d=0;
            argv[argc++]=arg;

            /* skip the remaining spaces */
            do {
                s++;
            } while (*s==' ' || *s=='\t');

            /* Start with a new argument */
            arg=d=s;
            bcount=0;
        } else if (*s=='\\') {
            /* '\\' */
            *d++=*s++;
            bcount++;
        } else if (*s=='"') {
            /* '"' */
            if ((bcount & 1)==0) {
                /* Preceeded by an even number of '\', this is half that
                 * number of '\', plus a '"' which we discard.
                 */
                d-=bcount/2;
                s++;
                in_quotes=!in_quotes;
            } else {
                /* Preceeded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d=d-bcount/2-1;
                *d++='"';
                s++;
            }
            bcount=0;
        } else {
            /* a regular character */
            *d++=*s++;
            bcount=0;
        }
    }
    if (*arg) {
        *d='\0';
        argv[argc++]=arg;
    }
    argv[argc]=NULL;

    return argv;
}


/***********************************************************************
 *           build_envp
 *
 * Build the environment of a new child process.
 */
static char **build_envp( const char *env, const char *extra_env )
{
    const char *p;
    char **envp;
    int count = 0;

    if (extra_env) for (p = extra_env; *p; count++) p += strlen(p) + 1;
    for (p = env; *p; count++) p += strlen(p) + 1;
    count += 3;

    if ((envp = malloc( count * sizeof(*envp) )))
    {
        extern char **environ;
        char **envptr = envp;
        char **unixptr = environ;
        /* first the extra strings */
        if (extra_env) for (p = extra_env; *p; p += strlen(p) + 1) *envptr++ = (char *)p;
        /* then put PATH, HOME and WINEPREFIX from the unix env */
        for (unixptr = environ; unixptr && *unixptr; unixptr++)
            if (!memcmp( *unixptr, "PATH=", 5 ) ||
                !memcmp( *unixptr, "HOME=", 5 ) ||
                !memcmp( *unixptr, "WINEPREFIX=", 11 )) *envptr++ = *unixptr;
        /* now put the Windows environment strings */
        for (p = env; *p; p += strlen(p) + 1)
        {
            if (!memcmp( p, "PATH=", 5 )) {
                *envptr = malloc(strlen(p)+5);
                strcat(strcpy(*envptr, "WINE"), p);
                envptr++;
            }
            else if (memcmp( p, "HOME=", 5 ) &&
                     memcmp( p, "WINEPRELOAD=", 12 ) &&
                     memcmp( p, "WINEPREFIX=", 11 )) *envptr++ = (char *)p;
        }
        *envptr = 0;
    }
    return envp;
}

static char *find_wine_binary_path()
{
    char *path, *p;

    /* check the env variable */
    p = getenv("WINELOADER");
    if (p)
    {
        path = malloc(strlen(p) + 1);
        strcpy(path, p);
        goto got_path;
    }

    if ((path = find_binary_path(NULL, "wine")))
        goto got_path;

    return NULL;

got_path:
    return path;
}

#ifdef __linux__
static char **preloader_fixup_argv(char **argv, char *preloader_path)
{
    int size;
    char **new_argv;

    /* get the size */
    for (size = 0; argv[size] != NULL; size++) {}

    /* add one for the preloader */
    size++;

    /* allocate new argv */
    new_argv = malloc(sizeof(char*) * size);

    /* put in the preloader as new_argv[0] */
    new_argv[0] = preloader_path;

    /* copy in the old argv, shifted one up */
    for (size = 0; argv[size] != NULL; size++)
    {
        new_argv[size+1] = argv[size];
    }
    /* and terminate */
    new_argv[size+1] = NULL;

    return new_argv;
}
#endif /* __linux__ */

/***********************************************************************
 *           exec_wine_binary
 *
 * Locate the Wine binary to exec for a new Win32 process.
 */
static void exec_wine_binary( char **argv, char **envp )
{
    char *wine_path;

    if ((wine_path = find_wine_binary_path()))
    {
#ifdef __linux__
        char *preloader_path;
        argv[0] = wine_path;
        if ((preloader_path = find_preloader_path(NULL)))
        {
            /* have to fix up argv for the preloader ;\ */
            char **new_argv = preloader_fixup_argv(argv, preloader_path);

            execve(new_argv[0], new_argv, envp);
            free(preloader_path);
            free(new_argv);
        } else
#else
        argv[0] = wine_path;
#endif
        {
            execve( argv[0], argv, envp );
        }

        free(wine_path);
        return; /* exec failed catch */
    }
    ERR("couldn't find wine binary\n");
}


/***********************************************************************
 *           fork_and_exec
 *
 * Fork and exec a new Unix process, checking for errors.
 */
static int fork_and_exec( HANDLE hFile, LPCSTR filename, char *cmdline,
                          const char *env, BOOL inherit,
			  DWORD flags, LPSTARTUPINFOA startup, LPCSTR lpCurrentDirectory,
			  HANDLE* process_info )
{
    const char* unixfilename = NULL;
    int exec_failed_fd[2];  /* child -> parent communication */
    int wait_for_exec[2];   /* parent -> child communication */
    int pid, err;
    char *extra_env = NULL;
    char start_exec;
    DOS_FULL_NAME full_dir, full_name;
    const char *unixdir = NULL;
    BOOL ret;

    TRACE("filename: %s cmdline: %s\n", filename, cmdline);

    if (!env)
    {
        env = GetEnvironmentStringsA();
        extra_env = DRIVE_BuildEnv();
    }

    if (pipe(exec_failed_fd) == -1)
    {
        ERR( "pipe1 failed %d\n", errno );
        FILE_SetDosError();
        return -1;
    }
    fcntl( exec_failed_fd[1], F_SETFD, 1 );  /* set close on exec */

    if (pipe(wait_for_exec) == -1)
    {
        ERR( "pipe2 failed %d\n", errno );
        FILE_SetDosError();
        return -1;
    }

    if (lpCurrentDirectory && *lpCurrentDirectory)
    {
        if (DOSFS_GetFullName( lpCurrentDirectory, TRUE, &full_dir ))
            unixdir = full_dir.long_name;
    }
    else
    {
        char buf[MAX_PATH];
        if (GetCurrentDirectoryA(sizeof(buf),buf))
        {
            if (DOSFS_GetFullName( buf, TRUE, &full_dir ))
                unixdir = full_dir.long_name;
        }
    }


    /* We now know the new pid (at this point pid == tid even if using pthreads )
     * create the process on the server side
     */
    SERVER_START_REQ( new_process )
    {
        char buf[MAX_PATH];

        req->inherit_all  = inherit;
        req->create_flags = flags;
        req->start_flags  = startup->dwFlags;
        req->exe_file     = hFile;
        if (startup->dwFlags & STARTF_USESTDHANDLES)
        {
            req->hstdin  = startup->hStdInput;
            req->hstdout = startup->hStdOutput;
            req->hstderr = startup->hStdError;
        }
        else
        {
            req->hstdin  = GetStdHandle( STD_INPUT_HANDLE );
            req->hstdout = GetStdHandle( STD_OUTPUT_HANDLE );
            req->hstderr = GetStdHandle( STD_ERROR_HANDLE );
        }
        req->cmd_show = startup->wShowWindow;

        if (!hFile)  /* unix process */
        {
            unixfilename = filename;
            if (DOSFS_GetFullName( filename, TRUE, &full_name ))
                unixfilename = full_name.long_name;
            wine_server_add_data( req, unixfilename, strlen(unixfilename) );
        }
        else  /* new wine process */
        {
            if (GetLongPathNameA( filename, buf, MAX_PATH ))
                wine_server_add_data( req, buf, strlen(buf) );
            else
                wine_server_add_data( req, filename, strlen(filename) );

            unixfilename = filename;
        }



/* FIXME: START BLOCK OF UGLY */


	if (!(pid = fork()))  /* child */
	{
		char **envp = build_envp( env, extra_env );
		
		close( exec_failed_fd[0] );
		close( wait_for_exec[1] );

		if( read( wait_for_exec[0], &start_exec, sizeof( start_exec ) ) != sizeof( start_exec ) )
		{
		  /* Wait until we are allowed to start */
		  perror( "Hmmm. Read:" );
		}

		close( wait_for_exec[0] );
		
		if ( unixdir )
		{
		  if( chdir(unixdir) == -1 )
		  {
		    fprintf( stderr, "Changdir to %s failed: %d/%s\n", unixdir, errno, strerror( errno ) );
		  }
		}

		if (hFile != 0)
		{
		   /* exec_wine_binary will fill in argv[0].
		    * build_argv is not used because it will mangle command
		    * lines with quotes. (Quotation marks must be passed through
		    * to the child command line as-is.)
		    * Ask Andrew for an example.
		    */
		    const char *argv[5] = { NULL, "-cmdline", cmdline, unixfilename,
			    		NULL };
					
		    if (envp) exec_wine_binary( (char **)argv, envp );
		}
		else
		{
		    char **argv = build_argv( cmdline, 0 );
		    
		    if (argv && envp) execve( unixfilename, argv, envp );
		}

		err = errno;
		write( exec_failed_fd[1], &err, sizeof(err) );
		close( exec_failed_fd[1] );
		fprintf( stderr, "hFile = %d unix file: '%s' cmdline '%s'\n", hFile, unixfilename, cmdline );
		perror( "exec failed:" );
		_exit(1);
	}
	
	if( pid == -1 ) { FILE_SetDosError(); return -1; }

/* FIXME: END BLOCK OF UGLY */
    
	req->unix_pid     = pid;	    
	    
        ret = !wine_server_call_err( req );
        *process_info = reply->info;
    }
    SERVER_END_REQ;
    
    
    if( write( wait_for_exec[1], &start_exec, sizeof( start_exec ) ) != sizeof( start_exec ) )
    {
      ERR( "Hmm. write failed %s\n", strerror( errno ) );
    }
    
    /* FIXME: Many error conditions not handled. */
    
    if (!ret) return -1;

    close( wait_for_exec[0] );
    close( wait_for_exec[1] );
    close( exec_failed_fd[1] );

    if ((pid != -1) && (read( exec_failed_fd[0], &err, sizeof(err) ) > 0))  /* exec failed */
    {
        errno = err;
        ERR( "Hmm. pid %d exec failed %s\n", pid, strerror( errno ) );
        pid = -1;
    }
    if (pid == -1) FILE_SetDosError();
    close( exec_failed_fd[0] );
    if (extra_env) HeapFree( GetProcessHeap(), 0, extra_env );
    return pid;
}


/***********************************************************************
 *           PROCESS_Create
 *
 * Create a new process. If hFile is a valid handle we have an exe
 * file, and we exec a new copy of wine to load it; otherwise we
 * simply exec the specified filename as a Unix process.
 */
BOOL PROCESS_Create( HANDLE hFile, LPCSTR filename, LPSTR cmd_line, LPCSTR env,
                     LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                     BOOL inherit, DWORD flags, LPSTARTUPINFOA startup,
                     LPPROCESS_INFORMATION info, LPCSTR lpCurrentDirectory )
{
    BOOL ret;
    DWORD wait_result;
    HANDLE load_done_evt = 0;
    HANDLE process_info = INVALID_HANDLE_VALUE;

    info->hThread = info->hProcess = 0;
    info->dwProcessId = info->dwThreadId = 0;

     /* fork and execute */
    if (fork_and_exec( hFile, filename, cmd_line, env, inherit,
                       flags, startup, lpCurrentDirectory, &process_info  ) == -1)
    {
        ERR( "Fork/exec failed\n" );
        CloseHandle( process_info );
        return FALSE;
    }

    /* wait for the new process info to be ready - but only for so long */

    ret = FALSE;
    if ( (wait_result = WaitForSingleObject( process_info, 5000 )) == STATUS_WAIT_0)
    {
        SERVER_START_REQ( get_new_process_info )
        {
            req->info     = process_info;
            req->pinherit = (psa && (psa->nLength >= sizeof(*psa)) && psa->bInheritHandle);
            req->tinherit = (tsa && (tsa->nLength >= sizeof(*tsa)) && tsa->bInheritHandle);
            if ((ret = !wine_server_call_err( req )))
            {
                info->dwProcessId = (DWORD)reply->pid;
                info->dwThreadId  = (DWORD)reply->tid;
                info->hProcess    = reply->phandle;
                info->hThread     = reply->thandle;
                load_done_evt     = reply->event;
            }
        }
        SERVER_END_REQ;
	
	ret = TRUE;
    }
    else
    {
      ERR( "Failed on waiting for process_info 0x%lx\n", wait_result );
    }
 
    CloseHandle( process_info );
    if (!ret) return FALSE;

    /* Wait until process is initialized (or initialization failed) */
    if (load_done_evt)
    {
        DWORD res;
        HANDLE handles[2];

        handles[0] = info->hProcess;
        handles[1] = load_done_evt;
        res = WaitForMultipleObjects( 2, handles, FALSE, INFINITE );
        CloseHandle( load_done_evt );
        if (res == STATUS_WAIT_0)  /* the process died */
        {
            DWORD exitcode;
            if (GetExitCodeProcess( info->hProcess, &exitcode )) SetLastError( exitcode );
            CloseHandle( info->hThread );
            CloseHandle( info->hProcess );
            return FALSE;
        }
    }
    return TRUE;
}


/***********************************************************************
 *           ExitProcess   (KERNEL32.@)
 */
void WINAPI ExitProcess( DWORD status )
{
    SERVER_START_REQ( terminate_process )
    {
        /* send the exit code to the server */
        req->handle    = GetCurrentProcess();
        req->exit_code = status;
        wine_server_call( req );
    }
    SERVER_END_REQ;

    MODULE_DllProcessDetach( TRUE, (LPVOID)1 );

    /* sometimes the nvidia drivers crash in their atexit handlers...
     * if that happens, we'll try again, since we usually really,
     * really, do need to exit. */
    __TRY
    {
        exit( status );
    }
    __EXCEPT(NULL)
    {
        exit( status );
        /* if this keeps failing, we could try with _exit() instead,
         * but at least on my system, that won't kill all the other
         * threads; at least the RunMainThread thing remains. */
    }
    __ENDTRY
}

/***********************************************************************
 *           ExitProcess   (KERNEL.466)
 */
void WINAPI ExitProcess16( WORD status )
{
    DWORD count;
    ReleaseThunkLock( &count );
    ExitProcess( status );
}

/******************************************************************************
 *           TerminateProcess   (KERNEL32.@)
 */
BOOL WINAPI TerminateProcess( HANDLE handle, DWORD exit_code )
{
    NTSTATUS status = NtTerminateProcess( handle, exit_code );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/***********************************************************************
 *           GetProcessDword    (KERNEL.485)
 *           GetProcessDword    (KERNEL32.18)
 * 'Of course you cannot directly access Windows internal structures'
 */
DWORD WINAPI GetProcessDword( DWORD dwProcessID, INT offset )
{
    DWORD x, y;

    TRACE_(win32)("(%ld, %d)\n", dwProcessID, offset );

    if (dwProcessID && dwProcessID != GetCurrentProcessId())
    {
        ERR("%d: process %lx not accessible\n", offset, dwProcessID);
        return 0;
    }

    switch ( offset )
    {
    case GPD_APP_COMPAT_FLAGS:
        return GetAppCompatFlags16(0);

    case GPD_LOAD_DONE_EVENT:
        return current_PDB.load_done_evt;

    case GPD_HINSTANCE16:
        return GetTaskDS16();

    case GPD_WINDOWS_VERSION:
        return GetExeVersion16();

    case GPD_THDB:
        return (DWORD)NtCurrentTeb() - 0x10 /* FIXME */;

    case GPD_PDB:
        return (DWORD)&current_PDB;

    case GPD_STARTF_SHELLDATA: /* return stdoutput handle from startupinfo ??? */
        return current_startupinfo.hStdOutput;

    case GPD_STARTF_HOTKEY: /* return stdinput handle from startupinfo ??? */
        return current_startupinfo.hStdInput;

    case GPD_STARTF_SHOWWINDOW:
        return current_startupinfo.wShowWindow;

    case GPD_STARTF_SIZE:
        x = current_startupinfo.dwXSize;
        if ( (INT)x == CW_USEDEFAULT ) x = CW_USEDEFAULT16;
        y = current_startupinfo.dwYSize;
        if ( (INT)y == CW_USEDEFAULT ) y = CW_USEDEFAULT16;
        return MAKELONG( x, y );

    case GPD_STARTF_POSITION:
        x = current_startupinfo.dwX;
        if ( (INT)x == CW_USEDEFAULT ) x = CW_USEDEFAULT16;
        y = current_startupinfo.dwY;
        if ( (INT)y == CW_USEDEFAULT ) y = CW_USEDEFAULT16;
        return MAKELONG( x, y );

    case GPD_STARTF_FLAGS:
        return current_startupinfo.dwFlags;

    case GPD_PARENT:
        return 0;

    case GPD_FLAGS:
        return current_PDB.flags;

    case GPD_USERDATA:
        return current_PDB.process_dword;

    default:
        ERR_(win32)("Unknown offset %d\n", offset );
        return 0;
    }
}

/***********************************************************************
 *           SetProcessDword    (KERNEL.484)
 * 'Of course you cannot directly access Windows internal structures'
 */
void WINAPI SetProcessDword( DWORD dwProcessID, INT offset, DWORD value )
{
    TRACE_(win32)("(%ld, %d)\n", dwProcessID, offset );

    if (dwProcessID && dwProcessID != GetCurrentProcessId())
    {
        ERR("%d: process %lx not accessible\n", offset, dwProcessID);
        return;
    }

    switch ( offset )
    {
    case GPD_APP_COMPAT_FLAGS:
    case GPD_LOAD_DONE_EVENT:
    case GPD_HINSTANCE16:
    case GPD_WINDOWS_VERSION:
    case GPD_THDB:
    case GPD_PDB:
    case GPD_STARTF_SHELLDATA:
    case GPD_STARTF_HOTKEY:
    case GPD_STARTF_SHOWWINDOW:
    case GPD_STARTF_SIZE:
    case GPD_STARTF_POSITION:
    case GPD_STARTF_FLAGS:
    case GPD_PARENT:
    case GPD_FLAGS:
        ERR_(win32)("Not allowed to modify offset %d\n", offset );
        break;

    case GPD_USERDATA:
        current_PDB.process_dword = value;
        break;

    default:
        ERR_(win32)("Unknown offset %d\n", offset );
        break;
    }
}


/*********************************************************************
 *           OpenProcess   (KERNEL32.@)
 */
HANDLE WINAPI OpenProcess( DWORD access, BOOL inherit, DWORD id )
{
    HANDLE ret = 0;
    SERVER_START_REQ( open_process )
    {
        req->pid     = id;
        req->access  = access;
        req->inherit = inherit;
        if (!wine_server_call_err( req )) ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

/*********************************************************************
 *           MapProcessHandle   (KERNEL.483)
 *           GetProcessId       (KERNEL32.@)
 */
DWORD WINAPI GetProcessId( HANDLE handle )
{
    DWORD ret = 0;
    SERVER_START_REQ( get_process_info )
    {
        req->handle = handle;
        if (!wine_server_call_err( req )) ret = (DWORD)reply->pid;
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           SetPriorityClass   (KERNEL32.@)
 */
BOOL WINAPI SetPriorityClass( HANDLE hprocess, DWORD priorityclass )
{
    BOOL ret;
    SERVER_START_REQ( set_process_info )
    {
        req->handle   = hprocess;
        req->priority = priorityclass;
        req->mask     = SET_PROCESS_INFO_PRIORITY;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           GetPriorityClass   (KERNEL32.@)
 */
DWORD WINAPI GetPriorityClass(HANDLE hprocess)
{
    DWORD ret = 0;
    SERVER_START_REQ( get_process_info )
    {
        req->handle = hprocess;
        if (!wine_server_call_err( req )) ret = reply->priority;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *          SetProcessAffinityMask   (KERNEL32.@)
 */
BOOL WINAPI SetProcessAffinityMask( HANDLE hProcess, DWORD affmask )
{
    BOOL ret;
    SERVER_START_REQ( set_process_info )
    {
        req->handle   = hProcess;
        req->affinity = affmask;
        req->mask     = SET_PROCESS_INFO_AFFINITY;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/**********************************************************************
 *          GetProcessAffinityMask    (KERNEL32.@)
 */
BOOL WINAPI GetProcessAffinityMask( HANDLE hProcess,
                                      LPDWORD lpProcessAffinityMask,
                                      LPDWORD lpSystemAffinityMask )
{
    BOOL ret = FALSE;
    SERVER_START_REQ( get_process_info )
    {
        req->handle = hProcess;
        if (!wine_server_call_err( req ))
        {
            if (lpProcessAffinityMask) *lpProcessAffinityMask = reply->process_affinity;
            if (lpSystemAffinityMask) *lpSystemAffinityMask = reply->system_affinity;
            ret = TRUE;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           GetProcessVersion    (KERNEL32.@)
 */
DWORD WINAPI GetProcessVersion( DWORD processid )
{
    IMAGE_NT_HEADERS *nt;

    if (processid && processid != GetCurrentProcessId())
    {
	FIXME("should use ReadProcessMemory\n");
        return 0;
    }
    if ((nt = RtlImageNtHeader( current_PDB.module )))
        return ((nt->OptionalHeader.MajorSubsystemVersion << 16) |
                nt->OptionalHeader.MinorSubsystemVersion);
    return 0;
}

/***********************************************************************
 *           GetProcessFlags    (KERNEL32.@)
 */
DWORD WINAPI GetProcessFlags( DWORD processid )
{
    if (processid && processid != GetCurrentProcessId()) return 0;
    return current_PDB.flags;
}


/***********************************************************************
 *		SetProcessWorkingSetSize	[KERNEL32.@]
 * Sets the min/max working set sizes for a specified process.
 *
 * PARAMS
 *    hProcess [I] Handle to the process of interest
 *    minset   [I] Specifies minimum working set size
 *    maxset   [I] Specifies maximum working set size
 *
 * RETURNS  STD
 */
BOOL WINAPI SetProcessWorkingSetSize(HANDLE hProcess,DWORD minset,
                                       DWORD maxset)
{
    FIXME("(0x%08x,%ld,%ld): stub - harmless\n",hProcess,minset,maxset);
    if(( minset == (DWORD)-1) && (maxset == (DWORD)-1)) {
        /* Trim the working set to zero */
        /* Swap the process out of physical RAM */
    }
    return TRUE;
}

/***********************************************************************
 *           GetProcessWorkingSetSize    (KERNEL32.@)
 */
BOOL WINAPI GetProcessWorkingSetSize(HANDLE hProcess,LPDWORD minset,
                                       LPDWORD maxset)
{
	FIXME("(0x%08x,%p,%p): stub\n",hProcess,minset,maxset);
	/* 32 MB working set size */
	if (minset) *minset = 32*1024*1024;
	if (maxset) *maxset = 32*1024*1024;
	return TRUE;
}

/***********************************************************************
 *           SetProcessShutdownParameters    (KERNEL32.@)
 *
 * CHANGED - James Sutherland (JamesSutherland@gmx.de)
 * Now tracks changes made (but does not act on these changes)
 */
static DWORD shutdown_flags = 0;
static DWORD shutdown_priority = 0x280;

BOOL WINAPI SetProcessShutdownParameters(DWORD level, DWORD flags)
{
    FIXME("(%08lx, %08lx): partial stub.\n", level, flags);
    shutdown_flags = flags;
    shutdown_priority = level;
    return TRUE;
}


/***********************************************************************
 * GetProcessShutdownParameters                 (KERNEL32.@)
 *
 */
BOOL WINAPI GetProcessShutdownParameters( LPDWORD lpdwLevel, LPDWORD lpdwFlags )
{
    *lpdwLevel = shutdown_priority;
    *lpdwFlags = shutdown_flags;
    return TRUE;
}


/***********************************************************************
 *           SetProcessPriorityBoost    (KERNEL32.@)
 */
BOOL WINAPI SetProcessPriorityBoost(HANDLE hprocess,BOOL disableboost)
{
    FIXME("(%d,%d): stub\n",hprocess,disableboost);
    /* Say we can do it. I doubt the program will notice that we don't. */
    return TRUE;
}


/***********************************************************************
 *		ReadProcessMemory (KERNEL32.@)
 *
 *  NOTES:
 *  bytes_read can be NULL.
 */
BOOL WINAPI ReadProcessMemory( HANDLE process, LPCVOID addr, LPVOID buffer, DWORD size,
                               LPDWORD bytes_read )
{
    const BYTE* address = (const BYTE*)addr;
    LPBYTE buf = buffer;
    DWORD transferred = 0;
    DWORD check_length;

    DWORD ret;

    /* On the first read, we pass a non-zero check_length that is the total
     * size we will eventually want to transfer. This lets the server verify
     * that the entire range is valid. */
    check_length = size;

    while (size > 0)
    {
	DWORD xfer_len = min( REQUEST_MAX_VAR_SIZE, size );

	SERVER_START_REQ( read_process_memory )
	{
	    req->handle	= process;
	    req->addr	= (LPVOID)address;
	    req->len	= xfer_len;
	    req->check_len = check_length;
	    wine_server_set_reply( req, buffer, size );
	    if ((ret = wine_server_call_err( req ))) xfer_len = 0;
	}
	SERVER_END_REQ;
	if (ret)
	{
	    if (bytes_read)
		*bytes_read = transferred;
	    return FALSE;
	}

	check_length = 0;

	address += xfer_len;
	buf += xfer_len;
	size -= xfer_len;
	transferred += xfer_len;
    }

    if (bytes_read)
	*bytes_read = transferred;
    return TRUE;
}


/***********************************************************************
 *           WriteProcessMemory    		(KERNEL32.@)
 *
 *  NOTES:
 *  bytes_written can be NULL.
 */
BOOL WINAPI WriteProcessMemory( HANDLE process, LPVOID addr, LPCVOID buffer, DWORD size,
                                LPDWORD bytes_written )
{
    LPBYTE address = (LPBYTE)addr;
    const BYTE* buf = buffer;
    DWORD transferred = 0;

    DWORD check_length = size;

    DWORD ret;

    if (!size)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
	if (bytes_written) *bytes_written = 0;
        return FALSE;
    }

    while (size > 0)
    {
	DWORD xfer_len = min( REQUEST_MAX_VAR_SIZE, size );

	SERVER_START_REQ( write_process_memory )
	{
	    req->handle	= process;
	    req->addr	= address;
	    req->len	= xfer_len;
	    req->check_len = check_length;
	    wine_server_add_data( req, buffer, xfer_len );

	    if ((ret = wine_server_call_err( req ))) xfer_len = 0;
	}
	SERVER_END_REQ;
	if (ret)
	{
	    if (bytes_written)
		*bytes_written = transferred;
	    return FALSE;
	}

	check_length = 0;

	address += xfer_len;
	buf += xfer_len;
	size -= xfer_len;
	transferred += xfer_len;
    }

    if (bytes_written)
	*bytes_written = transferred;
    return TRUE;
}


/***********************************************************************
 *		RegisterServiceProcess (KERNEL.491)
 *		RegisterServiceProcess (KERNEL32.@)
 *
 * A service process calls this function to ensure that it continues to run
 * even after a user logged off.
 */
DWORD WINAPI RegisterServiceProcess(DWORD dwProcessId, DWORD dwType)
{
	/* I don't think that Wine needs to do anything in that function */
	return 1; /* success */
}

/***********************************************************************
 * GetExitCodeProcess [KERNEL32.@]
 *
 * Gets termination status of specified process
 *
 * RETURNS
 *   Success: TRUE
 *   Failure: FALSE
 */
BOOL WINAPI GetExitCodeProcess(
    HANDLE hProcess,    /* [in] handle to the process */
    LPDWORD lpExitCode) /* [out] address to receive termination status */
{
    BOOL ret;
    SERVER_START_REQ( get_process_info )
    {
        req->handle = hProcess;
        ret = !wine_server_call_err( req );
        if (ret && lpExitCode) *lpExitCode = reply->exit_code;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           SetErrorMode   (KERNEL32.@)
 */
UINT WINAPI SetErrorMode( UINT mode )
{
    UINT old = current_PDB.error_mode;
    current_PDB.error_mode = mode;
    return old;
}


/**************************************************************************
 *              SetFileApisToOEM   (KERNEL32.@)
 */
VOID WINAPI SetFileApisToOEM(void)
{
    current_PDB.flags |= PDB32_FILE_APIS_OEM;
}


/**************************************************************************
 *              SetFileApisToANSI   (KERNEL32.@)
 */
VOID WINAPI SetFileApisToANSI(void)
{
    current_PDB.flags &= ~PDB32_FILE_APIS_OEM;
}


/******************************************************************************
 * AreFileApisANSI [KERNEL32.@]  Determines if file functions are using ANSI
 *
 * RETURNS
 *    TRUE:  Set of file functions is using ANSI code page
 *    FALSE: Set of file functions is using OEM code page
 */
BOOL WINAPI AreFileApisANSI(void)
{
    return !(current_PDB.flags & PDB32_FILE_APIS_OEM);
}

#define NORMAL_TLS_COUNT 64
#define EXPANSION_TLS_COUNT (sizeof(current_PEB.TlsExpansionBitmapBits) * 8)

/**********************************************************************
 * TlsAlloc [KERNEL32.@]  Allocates a TLS index.
 *
 * Allocates a thread local storage index
 *
 * RETURNS
 *    Success: TLS Index
 *    Failure: 0xFFFFFFFF
 */
DWORD WINAPI TlsAlloc( void )
{
    TEB *teb = NtCurrentTeb();
    PEB *peb = teb->PEB;
    DWORD tls_index;

    RtlAcquirePebLock();
    tls_index = RtlFindClearBitsAndSet(peb->TlsBitmap, 1, 0);
    if (tls_index != -1u)
    {
        /* a normal TLS was allocated */
        teb->tls_array[tls_index] = 0; /* clear the value */
        RtlReleasePebLock();
        return tls_index;
    }
    else
    {
        /* no normal TLS available, so check the expansion set */
        DWORD expansion_index 
                    = RtlFindClearBitsAndSet(peb->TlsExpansionBitmap, 1, 0);
        if (expansion_index != -1u)
        {
            /* allocated an expansion TLS */
            if (!teb->TlsExpansionSlots)
            {
                teb->TlsExpansionSlots = HeapAlloc(GetProcessHeap(),
                        HEAP_ZERO_MEMORY, EXPANSION_TLS_COUNT * sizeof(PVOID));
                if (!teb->TlsExpansionSlots)
                {
                    /* failed to allocate storage for the expansion slots, so
                       free the previous allocation and return an error */
                    RtlClearBits(peb->TlsExpansionBitmap, expansion_index, 1);
                    RtlReleasePebLock();
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    return 0xffffffff;
                }
            }
            
            teb->TlsExpansionSlots[expansion_index] = 0;
            RtlReleasePebLock();

            tls_index = expansion_index + NORMAL_TLS_COUNT;
            return tls_index;
        }
        else
        {
            /* no TLS could be allocated, so return an error */
            RtlReleasePebLock();
            SetLastError(ERROR_NO_MORE_ITEMS);
            return 0xffffffff;
        }
    }
}


/**********************************************************************
 * TlsFree [KERNEL32.@]  Releases a TLS index.
 *
 * Releases a thread local storage index, making it available for reuse
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI TlsFree(
    DWORD index) /* [in] TLS Index to free */
{
    TEB *teb;

    if (index >= (NORMAL_TLS_COUNT + EXPANSION_TLS_COUNT))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    teb = NtCurrentTeb();

    if (index < NORMAL_TLS_COUNT)
    {
        /* free the TLS in the normal TLS bitmap */
        RtlAcquirePebLock();
        RtlClearBits(teb->PEB->TlsBitmap, index, 1);
        teb->tls_array[index] = 0;
        RtlReleasePebLock();
    }
    else
    {
        /* free the TLS in the expansion bitmap */
        DWORD expansion_index = index - NORMAL_TLS_COUNT;
        RtlAcquirePebLock();
        RtlClearBits(teb->PEB->TlsExpansionBitmap, expansion_index, 1);
        if (teb->TlsExpansionSlots)
            teb->TlsExpansionSlots[expansion_index] = 0;
        RtlReleasePebLock();
    }

    /* FIXME: should zero all other thread TLS slots with a call to 
       NtSetInformationThread with a class of ThreadZeroTlsCell, but it is
       not implemented. */

    return TRUE;
}


/**********************************************************************
 * TlsGetValue [KERNEL32.@]  Gets value in a thread's TLS slot
 *
 * RETURNS
 *    Success: Value stored in calling thread's TLS slot for index
 *    Failure: 0 and GetLastError returns NO_ERROR
 */
#ifdef __i386__
__ASM_GLOBAL_FUNC(TlsGetValue,
                  "mov 0x4(%esp),%edx\n\t"
                  "cmp $0x3f,%edx\n\t"
                  "ja 1f\n\t"
                  "xor %eax,%eax\n\t"
                  "mov %eax,%fs:0x60\n\t"
                  "mov %fs:0x18,%eax\n\t"
                  "mov 0xe10(%eax,%edx,4),%eax\n\t"
                  "ret $0x4\n\t"
                  "1: cmp $0x43f,%edx\n\t"
                  "ja 2f\n\t"
                  "xor %eax,%eax\n\t"
                  "mov %eax,%fs:0x60\n\t"
                  "sub $0x40,%edx\n\t"
                  "mov %fs:0x18,%eax\n\t"
                  "mov 0xf94(%eax),%eax\n\t"
                  "cmp $0x0,%eax\n\t"
                  "je 3f\n\t"
                  "mov 0x0(%eax,%edx,4),%eax\n\t"
                  "ret $0x4\n\t"
                  "2: mov $0x57,%eax\n\t"
                  "mov %eax,%fs:0x60\n\t"
                  "3: xor %eax,%eax\n\t"
                  "ret $0x4\n");
#else
LPVOID WINAPI TlsGetValue(
    DWORD index) /* [in] TLS index to retrieve value for */
{
    LPVOID ret;
    TEB *teb;

    if (index >= (NORMAL_TLS_COUNT + EXPANSION_TLS_COUNT))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    teb = NtCurrentTeb();
    if (index < NORMAL_TLS_COUNT)
        ret = teb->tls_array[index];
    else if (teb->TlsExpansionSlots)
        ret = teb->TlsExpansionSlots[index - NORMAL_TLS_COUNT];
    else
        ret = NULL;
    
    SetLastError(ERROR_SUCCESS);
    return ret;
}
#endif


/**********************************************************************
 * TlsSetValue [KERNEL32.@]  Stores a value in the thread's TLS slot.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI TlsSetValue(
    DWORD index,  /* [in] TLS index to set value for */
    LPVOID value) /* [in] Value to be stored */
{
    TEB *teb;

    if (index >= (NORMAL_TLS_COUNT + EXPANSION_TLS_COUNT))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    teb = NtCurrentTeb();

    if (index < NORMAL_TLS_COUNT)
        teb->tls_array[index] = value;
    else
    {
        if (!teb->TlsExpansionSlots)
        {
            /* the expansion slot storage hasn't been allocated for this
               thread yet, so allocate it now */
            teb->TlsExpansionSlots = HeapAlloc(GetProcessHeap(),
                    HEAP_ZERO_MEMORY, EXPANSION_TLS_COUNT * sizeof(PVOID));
            if (!teb->TlsExpansionSlots)
            {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                return FALSE;
            }
        }
        teb->TlsExpansionSlots[index - NORMAL_TLS_COUNT] = value;
    }

    return TRUE;
}

/***********************************************************************
 *           GetCurrentProcess   (KERNEL32.@)
 */
#undef GetCurrentProcess
HANDLE WINAPI GetCurrentProcess(void)
{
    return 0xffffffff;
}

/**********************************************************************
 * GetProcessPriorityBoost [KERNEL32.@]  Returns priority boost for process.
 *
 * Always reports that priority boost is disabled.
 *
 * RETURNS
 *    Success: TRUE.
 *    Failure: FALSE
 */
BOOL WINAPI GetProcessPriorityBoost(
    HANDLE hprocess, /* [in] Handle to thread */
    PBOOL pstate)   /* [out] pointer to var that receives the boost state */
{
    if (pstate) *pstate = FALSE;
    return NO_ERROR;
}

