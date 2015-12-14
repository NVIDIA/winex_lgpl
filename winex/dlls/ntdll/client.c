/*
 * Client part of the client/server communication
 *
 * Copyright (C) 1998 Alexandre Julliard
 * Copyright (C) 2004 TransGaming Technologies
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/un.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <sys/uio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/poll.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#endif

#include "wine/winbase16.h"
#include "thread.h"
#include "wine/server.h"
#include "winerror.h"
#include "options.h"
#include "wine/debug.h"
#include "wine/profile.h"

WINE_DEFAULT_DEBUG_CHANNEL(client);

/* Some versions of glibc don't define this */
#ifndef SCM_RIGHTS
#define SCM_RIGHTS 1
#endif

#define CONFDIR    "/.wine"        /* directory for Wine config relative to $HOME */
#define SERVERDIR  "/wineserver-"  /* server socket directory (hostname appended) */
#define SOCKETNAME "socket"        /* name of the socket file */
#define FDSOCKETNAME "fd-socket"   /* name of the fd server socket file */

#ifndef HAVE_MSGHDR_ACCRIGHTS
/* data structure used to pass an fd with sendmsg/recvmsg */
struct cmsg_fd
{
    int len;   /* sizeof structure */
    int level; /* SOL_SOCKET */
    int type;  /* SCM_RIGHTS */
    int fd;    /* fd to pass */
};
#endif  /* HAVE_MSGHDR_ACCRIGHTS */

static thread_id_t boot_thread_id;
static sigset_t block_set;  /* signals to block during server calls */
static int fd_socket;  /* socket to exchange file descriptors with the server */
static int fd_server_socket = -1; /* socket to swap file descriptors over */

/* Shared Memory Server stuff */
static void* server_shared_memory = NULL; /* ptr to data */
typedef int (*shared_server_fcn)( thread_id_t threadid,
                                  const struct __server_request_info *req,
                                  union generic_reply *reply,
                                  void* variable_reply_data );
static shared_server_fcn shared_server_call = NULL;

/* PH: FIXME: Don't need some of these interfaces anymore */
typedef void* (*alloc_server_memory_fcn)( size_t size );
typedef void  (*free_server_memory_fcn)( void* ptr );
extern alloc_server_memory_fcn alloc_server_memory;
extern free_server_memory_fcn  free_server_memory;

typedef void* (*get_server_shared_memory_area_fcn)( int shm_server_type, const char* wineserver_exe_name, size_t shm_size, void *shm_addr, size_t shm_res_size, void *shm_res_addr );
static get_server_shared_memory_area_fcn get_server_shared_memory_area;

typedef void (*shared_server_set_debug_level_fcn)( int debuglevel );
static shared_server_set_debug_level_fcn shared_server_set_debug_level;

typedef void (*set_context_variables_fcn)( int fd_server_socket );
static set_context_variables_fcn set_context_variables;

struct shm_pollfd;
typedef int (*shared_server_complete_poll_fcn)( thread_id_t threadid, const void* cookie,
                                                struct shm_pollfd* shm_pollfd, int size,
                                                int hit, int signaled );
shared_server_complete_poll_fcn shared_server_complete_poll;

#ifdef __APPLE__
static const char wineserver_so_name[] = "libwineserver.dylib";
#else
static const char wineserver_so_name[] = "libwineserver.so";
#endif
static const char shared_server_entry_fcn_name[] = "shared_server_call";
static const char get_server_shared_memory_area_fcn_name[] = "get_server_shared_memory_area";
static const char shared_server_data_name[] = "shared_memory_data_area";
static const char alloc_server_memory_fcn_name[] = "alloc_server_memory";
static const char free_server_memory_fcn_name[]  = "free_server_memory";
static const char shared_server_set_debug_level_fcn_name[] = "set_debug_level";
static const char set_context_variables_fcn_name[] = "set_context_variables";
static const char shared_server_complete_poll_fcn_name[] = "shared_server_complete_poll";

alloc_server_memory_fcn alloc_server_memory;
free_server_memory_fcn  free_server_memory;

static void setup_shared_memory_debugging(void);

static BOOL shared_memory_allowed_calls[ REQ_NB_REQUESTS ] = {FALSE};
static BOOL use_scheduler = FALSE;

/* #define COLLECT_SHM_STATS */
#if defined( COLLECT_SHM_STATS )
static DWORD fast_server_call_stats[ REQ_NB_REQUESTS ] = {0};
static DWORD slow_server_call_stats[ REQ_NB_REQUESTS ] = {0};
#endif

/* will be set if the preloader reserved memory for the shm server */
size_t shm_res_size = 0;
void *shm_res_addr = NULL;

#ifdef __APPLE__
static pthread_mutex_t SendMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

BOOL call_supported_through_shared_memory( int req )
{
  if( req < REQ_NB_REQUESTS && req >= 0 )
    return shared_memory_allowed_calls[ req ];

  ERR( "Request %d is invalid\n", req );

  return FALSE;
}

BOOL server_scheduler_active( void )
{
  return use_scheduler;
}

#if defined( COLLECT_SHM_STATS )
/* Dump some very crude stats */
static void report_collected_shm_stats(void)
{
  unsigned int i;
  unsigned int highest;
  DWORD largest;

  highest = 0;
  largest = 0;

  fprintf( stderr, "Fast stats:\n" );
  for( i=0; i < REQ_NB_REQUESTS; i++ )
  {
    fprintf( stderr, "[%03u]=%010ld%c", i, fast_server_call_stats[i], ((i+1)%6 ) ? '\t' : '\n' );
    if( fast_server_call_stats[i] > largest )
    {
       largest = fast_server_call_stats[i];
       highest = i;
    }
  } 
  fprintf( stderr, "\nFastest with largest value is %d at %ld\n", highest, largest );

  highest = 0;
  largest = 0;

  fprintf( stderr, "Slow stats:\n" );
  for( i=0; i < REQ_NB_REQUESTS; i++ )
  {
    fprintf( stderr, "[%03u]=%010ld%c", i, slow_server_call_stats[i], ((i+1)%6 ) ? '\t' : '\n' );
    if( slow_server_call_stats[i] > largest )
    {
       largest = slow_server_call_stats[i];
       highest = i;
    }
  } 
  fprintf( stderr, "\nSlowest with largest value is %d at %ld\n", highest, largest );

}
#endif

inline static DWORD get_config_key( HKEY defkey, HKEY appkey, const char *name,
                                    char *buffer, DWORD size )
{
    if (appkey && !RegQueryValueExA( appkey, name, 0, NULL, (LPBYTE)buffer,
                                     &size )) return 0;
    return RegQueryValueExA( defkey, name, 0, NULL, (LPBYTE)buffer, &size );
}

static inline BOOL IS_OPTION_FALSE(ch)
{
    return (ch) == 'n' || (ch) == 'N' || (ch) == 'f' || (ch) == 'F' || (ch) == '0';
}


/* Check if SHM is enabled for this process in the registry. Would be nicer to do
 * in the server. */
static BOOL is_shm_disabled( void )
{
    char buffer[MAX_PATH+16];
    HKEY hkey, appkey = 0;
    BOOL disabled = TRUE;
    DWORD error;

    /* If we don't have an fd_server socket, we can't do shm. */
    if( fd_server_socket == -1 ) return TRUE;

    if (RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Wineserver", 0, NULL,
                         REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ))
    {
        ERR("Cannot create config registry key\n" );
        ExitProcess(1);
    }

    /* open the app-specific key */
    if (GetModuleFileName16( GetCurrentTask(), buffer, MAX_PATH ) ||
        ((error = GetModuleFileNameA( 0, buffer, MAX_PATH )) != 0 && error != MAX_PATH))
    {
        HKEY tmpkey;
        char *p, *appname = buffer;
        if ((p = strrchr( appname, '/' ))) appname = p + 1;
        if ((p = strrchr( appname, '\\' ))) appname = p + 1;
        strcat( appname, "\\Wineserver" );
        if (!RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\AppDefaults", &tmpkey ))
        {
            if (RegOpenKeyA( tmpkey, appname, &appkey )) appkey = 0;
            RegCloseKey( tmpkey );
        }
    }

    else
        ERR("could not retrieve the module file name (reason: '%s')\n", error == 0 ? "bad module" : "buffer too small");


    if (!get_config_key( hkey, appkey, "SHMWineserver", buffer, sizeof(buffer) ))
        disabled = IS_OPTION_FALSE( buffer[0] );

    return disabled;
}

static BOOL is_scheduler_disabled( void )
{
    char buffer[MAX_PATH+16];
    HKEY hkey, appkey = 0;
    BOOL disabled = TRUE;
#if 0
    DWORD error;
#endif

    if (RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Wineserver", 0, NULL,
                         REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ))
    {
        ERR("Cannot create config registry key\n" );
        ExitProcess(1);
    }

/* App specific key not supported for this, since the main module name hasn't
 *  * been set up when this function is called.  */
#if 0

    /* open the app-specific key */
    if (GetModuleFileName16( GetCurrentTask(), buffer, MAX_PATH ) ||
        ((error = GetModuleFileNameA( 0, buffer, MAX_PATH )) != 0 && error != MAX_PATH))
    {
        HKEY tmpkey;
        char *p, *appname = buffer;
        if ((p = strrchr( appname, '/' ))) appname = p + 1;
        if ((p = strrchr( appname, '\\' ))) appname = p + 1;
        strcat( appname, "\\Wineserver" );
        if (!RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\AppDefaults", &tmpkey ))
        {
            if (RegOpenKeyA( tmpkey, appname, &appkey )) appkey = 0;
            RegCloseKey( tmpkey );
        }
    }

    else
        ERR("could not retrieve the module file name (reason: '%s')\n", error == 0 ? "bad module" : "buffer too small");
#endif

    if (!get_config_key( hkey, appkey, "Scheduler", buffer, sizeof(buffer) ))
        disabled = IS_OPTION_FALSE( buffer[0] );

    return disabled;
}

/* will be called if the preloader reserved memory for the shm server */
void set_shared_memory_reserved(size_t size, void *addr)
{
    shm_res_size = size;
    shm_res_addr = addr;
}

/* Perform any required setup to allow communication and data exchange with the
 * shared memory wineserver.
 */
void setup_shared_memory_wineserver( int shm_type, size_t shm_size,
                                     void *shm_addr )
{
  void* so_handle;
  char error[256];

  /* We only need to setup once per process */
  assert( !server_shared_memory );

  if( is_shm_disabled() ) return;

#if defined( COLLECT_SHM_STATS )
  atexit( report_collected_shm_stats );
#endif

  /* Load the library */
  so_handle = wine_dlopen(wineserver_so_name, RTLD_NOW, error, sizeof(error) );
  if( !so_handle )
  {
     ERR( "can't load shared memory wineserver (%s): %s\n",
           wineserver_so_name, error );
     goto load_error;
  }

  /* Get the important symbols */
  shared_server_call = (shared_server_fcn)wine_dlsym( so_handle, shared_server_entry_fcn_name, error, sizeof( error ) );
  if( !shared_server_call )
  {
    ERR( "can't find the shared server entry point(%s): %s\n",
           shared_server_entry_fcn_name, error );
    goto load_error;
  }

#define GET_SHM_SERVER_SYMBOL(variable) \
  do { \
    variable = (variable##_fcn)wine_dlsym( so_handle, variable##_fcn_name, error, sizeof( error ) ); \
    if( !variable ) \
    { \
      ERR( "can't find shm function (%s): %s\n", #variable, error ); \
      goto load_error; \
    } \
  } while(0)

  GET_SHM_SERVER_SYMBOL(alloc_server_memory);
  GET_SHM_SERVER_SYMBOL(free_server_memory);
  GET_SHM_SERVER_SYMBOL(get_server_shared_memory_area);
  GET_SHM_SERVER_SYMBOL(shared_server_set_debug_level);
  GET_SHM_SERVER_SYMBOL(set_context_variables);
  GET_SHM_SERVER_SYMBOL(shared_server_complete_poll);

#undef GET_SHM_SERVER_SYMBOL

  server_shared_memory =
     get_server_shared_memory_area (shm_type, get_config_dir (), shm_size,
                                    shm_addr, shm_res_size, shm_res_addr);
  if( !server_shared_memory )
  {
    ERR( "can't get shared memory\n" );
    goto load_error;
  }

  /* Process and thread initialization operations */
  shared_memory_allowed_calls[ REQ_new_process ] = FALSE;
  shared_memory_allowed_calls[ REQ_get_new_process_info ] = FALSE;
  shared_memory_allowed_calls[ REQ_new_thread ] = FALSE; /* fd transmission */
  shared_memory_allowed_calls[ REQ_boot_done ] = FALSE;
  shared_memory_allowed_calls[ REQ_init_process ] = FALSE;
  shared_memory_allowed_calls[ REQ_init_process_done ] = FALSE;
  shared_memory_allowed_calls[ REQ_init_thread ] = FALSE; /* fd transmission */
  shared_memory_allowed_calls[ REQ_update_thread_tid ] = TRUE;

  /* DLL related operations */
  shared_memory_allowed_calls[ REQ_load_dll ] = FALSE; /* ptrace */
  shared_memory_allowed_calls[ REQ_unload_dll ] = FALSE; /* ptrace */


  /* APC operations */
  shared_memory_allowed_calls[ REQ_queue_apc ] = TRUE;
  shared_memory_allowed_calls[ REQ_get_apc ] = TRUE;

  /* Handle operations */
  shared_memory_allowed_calls[ REQ_close_handle ] = FALSE; /* fd copy (need ref count) */
  shared_memory_allowed_calls[ REQ_set_handle_info ] = TRUE;
  shared_memory_allowed_calls[ REQ_dup_handle ] = FALSE; /* fd */


  /* Thread/Process operations */
  shared_memory_allowed_calls[ REQ_set_thread_moduleload ] = TRUE;
  shared_memory_allowed_calls[ REQ_get_thread_moduleload ] = TRUE;
  shared_memory_allowed_calls[ REQ_terminate_process ] = FALSE;
  shared_memory_allowed_calls[ REQ_terminate_thread ] = FALSE;
  shared_memory_allowed_calls[ REQ_get_process_info ] = TRUE;
  shared_memory_allowed_calls[ REQ_set_process_info ] = TRUE;
  shared_memory_allowed_calls[ REQ_get_thread_info ] = TRUE;
  shared_memory_allowed_calls[ REQ_set_thread_info ] = TRUE;
  shared_memory_allowed_calls[ REQ_suspend_thread ] = FALSE;
  shared_memory_allowed_calls[ REQ_resume_thread ] = FALSE;
  shared_memory_allowed_calls[ REQ_open_process ] = TRUE;
  shared_memory_allowed_calls[ REQ_open_thread ] = TRUE;

  /* Synchronization operations */
  shared_memory_allowed_calls[ REQ_select ] = FALSE; /* must be FALSE */
  shared_memory_allowed_calls[ REQ_shm_select ] = TRUE; /* must be TRUE */

  shared_memory_allowed_calls[ REQ_create_event ] = TRUE;
  shared_memory_allowed_calls[ REQ_event_op ] = TRUE; /* sync */
  shared_memory_allowed_calls[ REQ_open_event ] = TRUE;

  shared_memory_allowed_calls[ REQ_create_mutex ] = TRUE;
  shared_memory_allowed_calls[ REQ_release_mutex ] = TRUE; /* sync */
  shared_memory_allowed_calls[ REQ_open_mutex ] = TRUE;

  shared_memory_allowed_calls[ REQ_create_semaphore ] = TRUE;
  shared_memory_allowed_calls[ REQ_release_semaphore ] = TRUE; /* sync */
  shared_memory_allowed_calls[ REQ_open_semaphore ] = TRUE;

  shared_memory_allowed_calls[ REQ_create_timer ] = TRUE;
  shared_memory_allowed_calls[ REQ_open_timer ] = TRUE;
  shared_memory_allowed_calls[ REQ_set_timer ] = FALSE; /* timer callback */
  shared_memory_allowed_calls[ REQ_cancel_timer ] = FALSE; /* timer callback */

  /* File operations */
  shared_memory_allowed_calls[ REQ_create_file ] = FALSE; /* create fd */
  shared_memory_allowed_calls[ REQ_alloc_file_handle ] = FALSE; /* create fd */
  shared_memory_allowed_calls[ REQ_get_handle_fd ] = TRUE; /* fd */
  shared_memory_allowed_calls[ REQ_set_file_pointer ] = TRUE; /* fd */
  shared_memory_allowed_calls[ REQ_truncate_file ] = TRUE; /* fd */
  shared_memory_allowed_calls[ REQ_set_file_time ] = TRUE; /* fd */
  shared_memory_allowed_calls[ REQ_flush_file ] = TRUE; /* fd */
  shared_memory_allowed_calls[ REQ_get_file_info ] = TRUE; /* fd */
  shared_memory_allowed_calls[ REQ_lock_file ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_unlock_file ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_register_async ] = FALSE; /* fd */

  /* Socket operations */
  shared_memory_allowed_calls[ REQ_create_socket ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_accept_socket ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_set_socket_event ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_socket_event ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_enable_socket_event ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_set_socket_deferred ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_network_interface_failed ] = TRUE; /* fd */

  /* Console operations */
  shared_memory_allowed_calls[ REQ_alloc_console ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_free_console ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_console_renderer_events ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_open_console ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_console_mode ] = FALSE; /* fd */;
  shared_memory_allowed_calls[ REQ_set_console_mode ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_set_console_input_info ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_console_input_info ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_append_console_input_history ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_console_input_history ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_create_console_output ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_set_console_output_info ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_console_output_info ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_write_console_input ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_read_console_input ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_write_console_output ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_fill_console_output ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_read_console_output ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_move_console_output ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_send_console_signal ] = FALSE; /* fd */

  /* PH: FIXME: ??? */
  shared_memory_allowed_calls[ REQ_create_change_notification ] = FALSE; /* fd */

  /* Mapping operations */
  shared_memory_allowed_calls[ REQ_create_mapping ] = FALSE; /* fd create */
  shared_memory_allowed_calls[ REQ_open_mapping ] = TRUE;
  shared_memory_allowed_calls[ REQ_get_mapping_info ] = TRUE; /* fd */

  /* Device operations */
  shared_memory_allowed_calls[ REQ_create_device ] = TRUE;

  /* Snapshot operations */
  shared_memory_allowed_calls[ REQ_create_snapshot ] = TRUE;
  shared_memory_allowed_calls[ REQ_next_process ] = TRUE;
  shared_memory_allowed_calls[ REQ_next_thread ] = TRUE;
  shared_memory_allowed_calls[ REQ_next_module ] = TRUE;

  /* Exception/Debugger operations - FIXME not checked */
  shared_memory_allowed_calls[ REQ_wait_debug_event ] = FALSE;
  shared_memory_allowed_calls[ REQ_queue_exception_event ] = FALSE;
  shared_memory_allowed_calls[ REQ_get_exception_status ] = FALSE;
  shared_memory_allowed_calls[ REQ_get_thread_context ] = FALSE;
  shared_memory_allowed_calls[ REQ_set_thread_context ] = FALSE;
  shared_memory_allowed_calls[ REQ_output_debug_string ] = FALSE;
  shared_memory_allowed_calls[ REQ_continue_debug_event ] = FALSE;
  shared_memory_allowed_calls[ REQ_debug_process ] = FALSE;
  shared_memory_allowed_calls[ REQ_debug_break ] = FALSE;
  shared_memory_allowed_calls[ REQ_set_debugger_kill_on_exit ] = FALSE;

  /* Memory operations */
  shared_memory_allowed_calls[ REQ_read_process_memory ] = FALSE; /* ptrace */
  shared_memory_allowed_calls[ REQ_write_process_memory ] = FALSE; /* ptrace */

  /* Registry operations */
  shared_memory_allowed_calls[ REQ_create_key ] = TRUE;
  shared_memory_allowed_calls[ REQ_open_key ] = TRUE;
  shared_memory_allowed_calls[ REQ_delete_key ] = TRUE;
  shared_memory_allowed_calls[ REQ_flush_key ] = FALSE; /* for extra safety of the .reg files */
  shared_memory_allowed_calls[ REQ_enum_key ] = TRUE;
  shared_memory_allowed_calls[ REQ_set_key_value ] = TRUE;
  shared_memory_allowed_calls[ REQ_get_key_value ] = TRUE;
  shared_memory_allowed_calls[ REQ_enum_key_value ] = TRUE;
  shared_memory_allowed_calls[ REQ_delete_key_value ] = TRUE;
  shared_memory_allowed_calls[ REQ_load_registry ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_save_registry ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_save_registry_atexit ] = FALSE; /* timer callback */
  shared_memory_allowed_calls[ REQ_set_registry_levels ] = FALSE; /* timer callback */

  /* Selector operations */
  shared_memory_allowed_calls[ REQ_get_selector_entry ] = FALSE; /* ptrace */

  /* Atom operations */
  shared_memory_allowed_calls[ REQ_add_atom ] = TRUE;
  shared_memory_allowed_calls[ REQ_delete_atom ] = TRUE;
  shared_memory_allowed_calls[ REQ_find_atom ] = TRUE;
  shared_memory_allowed_calls[ REQ_get_atom_name ] = TRUE;
  shared_memory_allowed_calls[ REQ_init_atom_table ] = TRUE;

  /* Msg queue operations */
  shared_memory_allowed_calls[ REQ_get_msg_queue ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_set_queue_mask ] = TRUE; /* sync */
  shared_memory_allowed_calls[ REQ_get_queue_status ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_wait_input_idle ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_send_message ] = TRUE; /* sync */
  shared_memory_allowed_calls[ REQ_get_message ] = TRUE; /* timer callback */ /* sync? */
  shared_memory_allowed_calls[ REQ_reply_message ] = TRUE; /* sync */
  shared_memory_allowed_calls[ REQ_get_message_reply ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_set_win_timer ] = TRUE; /* timer callback */ /* sync? */
  shared_memory_allowed_calls[ REQ_kill_win_timer ] = TRUE; /* timer callback */ /* sync? */

  /* Serial operations */
  shared_memory_allowed_calls[ REQ_create_serial ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_serial_info ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_set_serial_info ] = FALSE; /* fd */

  /* Pipe operations */
  shared_memory_allowed_calls[ REQ_create_named_pipe ] = TRUE;
  shared_memory_allowed_calls[ REQ_open_named_pipe ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_connect_named_pipe ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_wait_named_pipe ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_disconnect_named_pipe ] = FALSE; /* fd */
  shared_memory_allowed_calls[ REQ_get_named_pipe_info ] = TRUE;

  /* Window operations */
  shared_memory_allowed_calls[ REQ_create_window ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_link_window ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_destroy_window ] = TRUE; /* timeout */ /* sync? */
  shared_memory_allowed_calls[ REQ_set_window_owner ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_info ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_set_window_info ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_parents ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_children ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_tree ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_set_window_rectangles ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_rectangles ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_text ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_set_window_text ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_inc_window_paint_count ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_windows_offset ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_set_window_property ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_remove_window_property ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_property ] = TRUE; /* sync? */
  shared_memory_allowed_calls[ REQ_get_window_properties ] = TRUE; /* sync? */

  /* Internal debugging operations */
  shared_memory_allowed_calls[ REQ_enable_trace ] = FALSE;

  /* RPC endpoint operations */
  shared_memory_allowed_calls[ REQ_register_rpc_endpoints ] =  FALSE; /* FIXME: Bug in server code TRUE; */
  shared_memory_allowed_calls[ REQ_unregister_rpc_endpoints ] = FALSE; /* FIXME: Bug in server code TRUE; */
  shared_memory_allowed_calls[ REQ_resolve_rpc_endpoint ] = TRUE;

  /* eject - needs to close fds in the wineserver */
  shared_memory_allowed_calls[ REQ_get_cdrom_eject_fd_count ] = FALSE;
  shared_memory_allowed_calls[ REQ_get_cdrom_eject_fd_list ] = FALSE;
  shared_memory_allowed_calls[ REQ_add_cdrom_device_info ] = FALSE;

  /* signals */
  shared_memory_allowed_calls[ REQ_send_fast_unix_signal ] = TRUE;
  shared_memory_allowed_calls[ REQ_send_unix_signal ] = FALSE; /* may resume thread */
  shared_memory_allowed_calls[ REQ_recv_unix_signal ] = TRUE;

  /* scheduling */
  shared_memory_allowed_calls[ REQ_set_scheduling_mode ] = TRUE;

  setup_shared_memory_debugging();
  set_context_variables( fd_server_socket );

  TRACE( "Using Shared Memory Wineserver\n" );

  return;

load_error:
    server_shared_memory = NULL;
    shared_server_call = NULL;

}

static void setup_shared_memory_debugging(void)
{
    /* Is this the boot thread and are we using shared memory? */
    if( !shared_server_set_debug_level ) return;

    shared_server_set_debug_level( TRACE_ON(client) ? 1 : 0 );
}

/* die on a fatal error; use only during initialization */
static void fatal_error( const char *err, ... ) WINE_NORETURN;
static void fatal_error( const char *err, ... )
{
    va_list args;

    va_start( args, err );
    fprintf( stderr, "wine: " );
    vfprintf( stderr, err, args );
    va_end( args );
    exit(1);
}

/* die on a fatal error; use only during initialization */
static void fatal_perror( const char *err, ... ) WINE_NORETURN;
static void fatal_perror( const char *err, ... )
{
    va_list args;

    va_start( args, err );
    fprintf( stderr, "wine: " );
    vfprintf( stderr, err, args );
    perror( " " );
    va_end( args );
    exit(1);
}

/***********************************************************************
 *           server_protocol_error
 */
void server_protocol_error( const char *err, ... ) /* WINE_NORETURN */
{
    va_list args;

    va_start( args, err );
    fprintf( stderr, "wine client error:%u: ", NtCurrentTeb()->tid );
    vfprintf( stderr, err, args );
    va_end( args );
    SYSDEPS_AbortThread(1);
}


/***********************************************************************
 *           server_protocol_perror
 */
void server_protocol_perror( const char *err ) /* WINE_NORETURN */
{
    fprintf( stderr, "wine client perror:%u: ", NtCurrentTeb()->tid );
    perror( err );
    SYSDEPS_AbortThread(1);
}


#if ANDREWS_REENTRANT_STUFF
/***********************************************************************
 *           wine_server_exception_handler
 *
 * This exception handler is used to cleanup exceptions when a
 * server request is on the stack.
 */
static DWORD
wine_server_exception_handler( PEXCEPTION_RECORD record,
			       EXCEPTION_FRAME *frame,
			       CONTEXT *context,
			       EXCEPTION_FRAME **pdispatcher )
{
    struct wine_server_request *req;

    req = (struct wine_server_request*)((LPBYTE)frame - offsetof(struct wine_server_request, frame));

    if (record->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND))
    {
	NtCurrentTeb()->buffer_pos = req->saved_buffer_pos;

	assert(NtCurrentTeb()->server_request_list_tail == req);
	NtCurrentTeb()->server_request_list_tail = req->prev;

	if (NtCurrentTeb()->server_request_list_head == req)
	    NtCurrentTeb()->server_request_list_head = NULL;

	/* What about pending data on the socket? */
    }

    return ExceptionContinueSearch;
}


/***********************************************************************
 *           wine_server_alloc_req (NTDLL.@)
 */
void wine_server_alloc_req( union generic_request *req, size_t size )
{
    unsigned int pos = NtCurrentTeb()->buffer_pos;

    assert( size <= REQUEST_MAX_VAR_SIZE );

    if (pos + size > NtCurrentTeb()->buffer_size)
        server_protocol_error( "buffer overflow %d bytes\n",
                               pos + size - NtCurrentTeb()->buffer_pos );

    NtCurrentTeb()->buffer_pos = pos + size;
    req->header.var_offset = pos;
    req->header.var_size = size;
}

/***********************************************************************
 *           wine_server_init_req (NTDLL.@)
 */
void wine_server_init_req( struct wine_server_request *req,
			   enum request request_type, size_t variable_size )
{
    memset(req, 0, sizeof(*req));

    req->saved_buffer_pos = NtCurrentTeb()->buffer_pos;
    req->req.header.req = request_type;

    if (variable_size)
	wine_server_alloc_req( &req->req, variable_size );

    req->frame.Handler = wine_server_exception_handler;
    __wine_push_frame( &req->frame );

    if (NtCurrentTeb()->server_request_list_tail)
    {
	NtCurrentTeb()->server_request_list_tail->next = req;
	req->prev = NtCurrentTeb()->server_request_list_tail;
	NtCurrentTeb()->server_request_list_tail = req;
    }
    else
    {
	NtCurrentTeb()->server_request_list_head = req;
	NtCurrentTeb()->server_request_list_tail = req;
    }
}

/***********************************************************************
 *           wine_server_end_req (NTDLL.@)
 */
void wine_server_end_req( struct wine_server_request *req)
{
    NtCurrentTeb()->buffer_pos = req->saved_buffer_pos;

    assert(NtCurrentTeb()->server_request_list_tail == req);
    NtCurrentTeb()->server_request_list_tail = req->prev;

    if (NtCurrentTeb()->server_request_list_head == req)
	NtCurrentTeb()->server_request_list_head = NULL;

    __wine_pop_frame( &req->frame );
}
#endif

/***********************************************************************
 *           send_request
 *
 * Send a request to the server.
 */
static void send_request( const struct __server_request_info *req )
{
    int i, ret;

    if (!req->u.req.request_header.request_size)
    {
        if ((ret = write( NtCurrentTeb()->request_fd, &req->u.req,
                          sizeof(req->u.req) )) == sizeof(req->u.req)) return;

    }
    else
    {
        struct iovec vec[__SERVER_MAX_DATA+1];

        vec[0].iov_base = (void *)&req->u.req;
        vec[0].iov_len = sizeof(req->u.req);
        for (i = 0; i < req->data_count; i++)
        {
            vec[i+1].iov_base = (void *)req->data[i].ptr;
            vec[i+1].iov_len = req->data[i].size;
        }
        if ((ret = writev( NtCurrentTeb()->request_fd, vec, i+1 )) ==
            req->u.req.request_header.request_size + sizeof(req->u.req)) return;
    }

    if (ret >= 0) server_protocol_error( "partial write %d\n", ret );
    if (errno == EPIPE) SYSDEPS_AbortThread(0);
    server_protocol_perror( "write/writev" );
}


/***********************************************************************
 *           send_request
 *
 * Send a request to the server.
 */
static int send_shared_request( struct __server_request_info *req )
{
  int reply_generated;
  union generic_reply reply;
  void* reply_variable_data = req->reply_data;

  /* Measure the rough amount of time spent doing fast shm server calls. This is
   * approximate due to the fact that this thread may block waiting for the
   * semaphore in addition to just the usual scheduler fun that you have to guess at.
   */
  WINE_START_TIMER("fast shm w/ blocking");

  /* Call into the server code */
  reply_generated = shared_server_call(  NtCurrentTeb()->tid,
                                         req,
                                         &reply, reply_variable_data );
  WINE_STOP_TIMER("fast shm w/ blocking");

  if( reply_generated )
  {
    /* Copy the reply. We can't do any better than this because req is a union of request/reply */
    memcpy( &req->u.reply, &reply, sizeof( req->u.reply ) );

    req->reply_data = reply_variable_data;
  }
  else
  {
    ERR( "No reply for request %d\n", req->u.req.request_header.req );
  }

  return !reply_generated;
}


/***********************************************************************
 *           read_reply_data
 *
 * Read data from the reply buffer; helper for wait_reply.
 */
static void read_reply_data( void *buffer, size_t size )
{
    int ret;

    for (;;)
    {
        if ((ret = read( NtCurrentTeb()->reply_fd, buffer, size )) > 0)
        {
            if (!(size -= ret)) return;
            buffer = (char *)buffer + ret;
            continue;
        }
        if (!ret) break;
        if (errno == EINTR) continue;
        if (errno == EPIPE) break;
        server_protocol_perror("read");
    }
    /* the server closed the connection; time to die... */
    SYSDEPS_AbortThread(0);
}

/***********************************************************************
 *           wait_reply
 *
 * Wait for a reply from the server.
 */
inline static void wait_reply( struct __server_request_info *req )
{
    read_reply_data( &req->u.reply, sizeof(req->u.reply) );
    if (req->u.reply.reply_header.reply_size)
        read_reply_data( req->reply_data, req->u.reply.reply_header.reply_size );
}


#if ANDREWS_REENTRANT_STUFF
static void flush_old_replies(void)
{
    struct wine_server_request *req = NtCurrentTeb()->server_request_list_head;
    struct wine_server_request *tail = NtCurrentTeb()->server_request_list_tail;

    for (; req != tail; req = req->next)
    {
	if (!req->reply_received)
	{
	    wait_reply(&req->req);
	    req->reply_received = TRUE;
	}
    }
}
#endif

/***********************************************************************
 *           wine_server_call (NTDLL.@)
 *
 * Perform a server call.
 */
unsigned int wine_server_call( void *req_ptr )
{
    struct __server_request_info * const req = req_ptr;
    sigset_t old_set;

    memset( (char *)&req->u.req + req->size, 0, sizeof(req->u.req) - req->size );
    SYSDEPS_sigprocmask( SIG_BLOCK, &block_set, &old_set );
#if ANDREWS_REENTRANT_STUFF
    flush_old_replies();
    send_request( &req->req );
    if (!req->reply_received)
    {
        wait_reply( &req->req );
        req->reply_received = TRUE;
    }
    req->reply_received = TRUE;
#else

    if( call_supported_through_shared_memory( req->u.req.request_header.req ) )
    {
#if defined( COLLECT_SHM_STATS )
        fast_server_call_stats[ req->u.req.request_header.req ]++;
#endif
        if( send_shared_request( req ) )
            wait_reply( req );
    }
    else
    {
#if defined( COLLECT_SHM_STATS )
        slow_server_call_stats[ req->u.req.request_header.req ]++;
#endif
        send_request( req );
        wait_reply( req );
    }

#endif
    SYSDEPS_sigprocmask( SIG_SETMASK, &old_set, NULL );
    return req->u.reply.reply_header.error;
}


/***********************************************************************
 *           wine_server_send_fd
 *
 * Send a file descriptor to the server.
 */
void wine_server_send_fd( int fd )
{
#ifndef HAVE_MSGHDR_ACCRIGHTS
    struct cmsg_fd cmsg;
#endif
    struct send_fd data;
    struct msghdr msghdr;
    struct iovec vec;
    int ret;

    vec.iov_base = (void *)&data;
    vec.iov_len  = sizeof(data);

    msghdr.msg_name    = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov     = &vec;
    msghdr.msg_iovlen  = 1;

#ifdef HAVE_MSGHDR_ACCRIGHTS
    msghdr.msg_accrights    = (void *)&fd;
    msghdr.msg_accrightslen = sizeof(fd);
#else  /* HAVE_MSGHDR_ACCRIGHTS */
    cmsg.len   = sizeof(cmsg);
    cmsg.level = SOL_SOCKET;
    cmsg.type  = SCM_RIGHTS;
    cmsg.fd    = fd;
    msghdr.msg_control    = &cmsg;
    msghdr.msg_controllen = sizeof(cmsg);
    msghdr.msg_flags      = 0;
#endif  /* HAVE_MSGHDR_ACCRIGHTS */

    data.tid = GetCurrentThreadId();
    data.fd  = fd;

    for (;;)
    {
#ifdef __APPLE__
        /* 10.4 and pre-10.5 have a kernel crash bug that can be hit by
           multiple sends on the same socket from multiple threads
           simultaneously; let's avoid that for now */
        pthread_mutex_lock (&SendMutex);
#endif
        ret = sendmsg( fd_socket, &msghdr, 0 );
#ifdef __APPLE__
        pthread_mutex_unlock (&SendMutex);
#endif
        if (ret == sizeof(data)) return;
        if (ret >= 0) server_protocol_error( "partial write %d\n", ret );
        if (errno == EINTR) continue;
        if (errno == EPIPE) SYSDEPS_AbortThread(0);
        server_protocol_perror( "sendmsg" );
    }
}


/***********************************************************************
 *           receive_fd
 *
 * Receive a file descriptor passed from the server.
 */
static int receive_fd( handle_t *handle )
{
    struct iovec vec;
    int ret, fd;

#ifdef HAVE_MSGHDR_ACCRIGHTS
    struct msghdr msghdr;

    fd = -1;
    msghdr.msg_accrights    = (void *)&fd;
    msghdr.msg_accrightslen = sizeof(fd);
#else  /* HAVE_MSGHDR_ACCRIGHTS */
    struct msghdr msghdr;
    struct cmsg_fd cmsg;

    cmsg.len   = sizeof(cmsg);
    cmsg.level = SOL_SOCKET;
    cmsg.type  = SCM_RIGHTS;
    cmsg.fd    = -1;
    msghdr.msg_control    = &cmsg;
    msghdr.msg_controllen = sizeof(cmsg);
    msghdr.msg_flags      = 0;
#endif  /* HAVE_MSGHDR_ACCRIGHTS */

    msghdr.msg_name    = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov     = &vec;
    msghdr.msg_iovlen  = 1;
    vec.iov_base = (void *)handle;
    vec.iov_len  = sizeof(*handle);

    for (;;)
    {
        struct timeval timeout;
        fd_set fds;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        FD_ZERO (&fds);
        FD_SET (fd_socket, &fds);

        /* Mac OS X, at least as of 10.4 and pre-10.5, can periodically
           lose an in-flight fd; thus we need a timeout here to kick off
           a retry mechanism. The timeout needs to be large enough to deal
           with the fact that, on the Mac, a fd message can sometimes take a
           few moments to arrive after having been sent. */
        ret = select (fd_socket + 1, &fds, NULL, NULL, &timeout);
        if (ret == 0)
        {
           ERR ("FD went missing; attempting recovery\n");
           return -2;
        }
        else if (ret == -1)
        {
           if (errno == EINTR)
              continue;
           if (errno == EPIPE)
              break;
           server_protocol_perror ("select in receive_fd");
        }

#ifdef __APPLE__
        /* 10.4 and pre-10.5 have a kernel crash bug that can be hit by
           activity on the same UNIX socket from multiple threads
           simultaneously when passing around fds; let's avoid that for now */
        pthread_mutex_lock (&SendMutex);
#endif
        ret = recvmsg( fd_socket, &msghdr, 0 );
#ifdef __APPLE__
        pthread_mutex_unlock (&SendMutex);
#endif
        if (ret > 0)
        {
#ifndef HAVE_MSGHDR_ACCRIGHTS
            fd = cmsg.fd;
#endif
            if (fd == -1) server_protocol_error( "no fd received for handle %d\n", *handle );
            fcntl( fd, F_SETFD, 1 ); /* set close on exec flag */
            return fd;
        }
        if (!ret) break;
        if (errno == EINTR) continue;
        if (errno == EPIPE) break;
        server_protocol_perror("recvmsg");
    }
    /* the server closed the connection; time to die... */
    SYSDEPS_AbortThread(0);
}


/***********************************************************************
 *           wine_server_recv_fd
 *
 * Receive a file descriptor passed from the server.
 * The file descriptor must not be closed.
 * Return -2 if a race condition stole our file descriptor.
 */
int wine_server_recv_fd( handle_t handle )
{
    handle_t fd_handle;

    int fd = receive_fd( &fd_handle );
    if (fd == -2)
       return -2;

    /* now store it in the server fd cache for this handle */
    SERVER_START_REQ( set_handle_info )
    {
        req->handle = fd_handle;
        req->flags  = 0;
        req->mask   = 0;
        req->fd     = fd;
        if (!wine_server_call( req ))
        {
            if (reply->cur_fd != fd)
            {
                /* someone was here before us */
                close( fd );
                fd = reply->cur_fd;
            }
        }
        else
        {
            close( fd );
            fd = -1;
        }
    }
    SERVER_END_REQ;

    if (handle != fd_handle) fd = -2;  /* not the one we expected */
    return fd;
}


/***********************************************************************
 *           wine_server_complete_shm_poll
 */
int wine_server_complete_shm_poll( const void* cookie,
                                   struct shm_pollfd* shm_pollfd,
                                   unsigned int size,
                                   int hits,
                                   int signaled )
{
    int ret;
    sigset_t old_set;

    SYSDEPS_sigprocmask( SIG_BLOCK, &block_set, &old_set );
    ret = shared_server_complete_poll( NtCurrentTeb()->tid,
                                       cookie,
                                       shm_pollfd,
                                       size,
                                       hits,
                                       signaled );
    SYSDEPS_sigprocmask( SIG_SETMASK, &old_set, NULL );
    return ret;
}


/***********************************************************************
 *           get_config_dir
 *
 * Return the configuration directory ($WINEPREFIX or $HOME/.wine)
 */
const char *get_config_dir(void)
{
    static char *confdir;
    if (!confdir)
    {
        const char *prefix = getenv( "WINEPREFIX" );
        if (prefix)
        {
            int len = strlen(prefix);
            if (!(confdir = strdup( prefix ))) fatal_error( "out of memory\n" );
            if (len > 1 && confdir[len-1] == '/') confdir[len-1] = 0;
        }
        else
        {
            const char *home = getenv( "HOME" );
            if (!home)
            {
                struct passwd *pwd = getpwuid( getuid() );
                if (!pwd) fatal_error( "could not find your home directory\n" );
                home = pwd->pw_dir;
            }
            if (!(confdir = malloc( strlen(home) + strlen(CONFDIR) + 1 )))
                fatal_error( "out of memory\n" );
            strcpy( confdir, home );
            strcat( confdir, CONFDIR );
        }
    }
    return confdir;
}

/* helper for find_binary_path, determines if the binary
 * at the given path exists and is executable
 */
int can_exec_binary(const char *binary)
{
    if (access(binary, X_OK | F_OK) == 0) return 1;
    return 0;
}

/* a semi-standard search for finding if a binary exists
 * somewhere that is useful.
 * Used to find the path of the wineserver and the preloader.
 * string returned must be free'd with free().
 */
char *find_binary_path( const char *oldcwd, const char *binary )
{
    char *path = NULL, *p;
    char *unix_path;

    /* try the installation dir */
    path = malloc(strlen(BINDIR) + 1 + strlen(binary) + 1);
    strcpy(path, BINDIR);
    strcat(path, "/");
    strcat(path, binary);
    if (can_exec_binary(path)) goto got_path;
    free(path);

    /* now try the dir we were launched from */
    if (full_argv0)
    {
        if (!(path = malloc( strlen(full_argv0) + strlen(binary) + 3 )))
            fatal_error( "out of memory\n" );
        if ((p = strrchr( strcpy( path, full_argv0 ), '/' )))
        {
            *p = '/'; p++;
            strcpy( p, binary );
            if (can_exec_binary(path)) goto got_path;
        }
        free(path);
    }

    /* now try the unix path */
    if ((unix_path = getenv("PATH")))
    {
        char *tmppath;
        char *p = unix_path;
        tmppath = malloc(strlen(p) + 1 + strlen(binary) + 1);
        while (p && *p)
        {
            char *p_end = strchr(p, ':');
            int p_len = p_end ? (p_end - p) : (strlen(p));

            strncpy(tmppath, p, p_len);
            *(tmppath + p_len) = '/';
            strcpy(tmppath + p_len + 1, binary);
            path = tmppath;
            if (can_exec_binary(path)) goto got_path;
            p = p_end ? p_end + 1 : NULL;;
        }
        free(tmppath);
        path = NULL;
    }

    /* and finally the current dir */
    if (!(path = malloc( (oldcwd ? strlen(oldcwd) : 0) + strlen(binary) + 3 )))
        fatal_error( "out of memory\n" );
    if (oldcwd)
    {
        p = strcpy( path, oldcwd ) + strlen( oldcwd );
        *p = '/'; p++;
    }
    else
        p = strcpy( path, "./");
    strcpy( p, binary );
    if (can_exec_binary(path)) goto got_path;
    free(path);

    path = NULL;

got_path:
    return path;
}

static char *find_server_path(const char *oldcwd)
{
    char *p;
    char *path = NULL;

    /* if server is explicitly specified, use this */
    if ((p = getenv("WINESERVER")))
    {
        if (p[0] != '/' && oldcwd && oldcwd[0] == '/')  /* make it an absolute path */
        {
            if (!(path = malloc( strlen(oldcwd) + strlen(p) + 1 )))
                fatal_error( "out of memory\n" );
            sprintf( path, "%s/%s", oldcwd, p );
            p = path;
        }
        else
        {
            path = malloc(strlen(p) + 1);
            strcpy(path, p);
        }
        if (can_exec_binary(path)) goto got_path;
        fatal_perror( "could not exec the server '%s'\n"
                      "    specified in the WINESERVER environment variable", p );
    }

    /* search for "wineserver" */
    if ((path = find_binary_path(oldcwd, "wineserver")))
        goto got_path;

    /* search for "server/wineserver" */
    if ((path = find_binary_path(oldcwd, "server/wineserver")))
        goto got_path;

    return NULL;

got_path:
    return path;
}

char *find_preloader_path(const char *oldcwd)
{
    char *p;
    char *path = NULL;

    /* if preloader is explicitly specified, use it */
    if ((p = getenv("WINEPRELOADER")))
    {
        if (p[0] != '/' && oldcwd && oldcwd[0] == '/')  /* make it an absolute path */
        {
            if (!(path = malloc( strlen(oldcwd) + strlen(p) + 1 )))
                fatal_error( "out of memory\n" );
            sprintf( path, "%s/%s", oldcwd, p );
            p = path;
        }
        else
        {
            path = malloc(strlen(p) + 1);
            strcpy(path, p);
        }
        if (can_exec_binary(path)) goto got_path;
	if (p[0] == 0) /* Preloader disabled */
	    return NULL;

        fatal_perror( "could not exec the preloader '%s'\n"
                      "    specified in the WINEPRELOADER environment variable", p );
    }

    /* search for "wine-preloader" */
    if ((path = find_binary_path(oldcwd, "wine-preloader")))
        goto got_path;

    /* search for "miscemu/wine-preloader" */
    if ((path = find_binary_path(oldcwd, "miscemu/wine-preloader")))
        goto got_path;

    return NULL;

got_path:
    return path;
}

/***********************************************************************
 *           start_server
 *
 * Start a new wine server.
 */
static void start_server( const char *oldcwd )
{
    static int started;  /* we only try once */
    if (!started)
    {
        int status;
        int pid = fork();
        if (pid == -1) fatal_perror( "fork" );
        if (!pid)
        {
            char *server_path = NULL;
#ifdef __linux__
            char *preloader_path = NULL;
#endif

            if ((server_path = find_server_path(oldcwd)) == NULL)
            {
                fatal_error( "could not exec wineserver\n" );
            }
#ifdef __linux__
            if ((preloader_path = find_preloader_path(oldcwd)))
            {
                TRACE("executing with preloader '%s: %s %s'\n",
                        preloader_path, preloader_path, server_path);
                /* ensure legacy layout is turned off for the server */
                setenv("WINEPRELOADER_SETVALEGACY", "no", 1);
                /* tell the preloader it's using the server */
                setenv("WINEPRELOADER_SERVER", "yes", 1);
                execl(preloader_path, preloader_path, server_path, NULL);
                free(preloader_path);
                WARN("couldn't load wineserver through preloader\n");
                /* fall through to try just the server */
            }
#endif
            TRACE("executing without preloader '%s: %s'\n", server_path, "wineserver");
            execl(server_path, "wineserver", NULL);
            free(server_path);
            fatal_error( "could not exec wineserver\n" );
        }
        started = 1;
        waitpid( pid, &status, 0 );
        status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        if (status) exit(status);  /* server failed */
    }
}

static int socket_exists( const char* const sock_name )
{
    struct stat st;

    if (lstat( sock_name, &st ) == -1)
    {
        if (errno != ENOENT) fatal_perror( "exists lstat %s", sock_name );
        return 0;
    }
  
    return 1;
}

static int check_socket_sanity( const char* const sock_name, const char* const serverdir )
{
    struct stat st;

    if (lstat( sock_name, &st ) == -1) return 0;         

    /* make sure the socket is sane (ISFIFO needed for Solaris) */
    if (!S_ISSOCK(st.st_mode) && !S_ISFIFO(st.st_mode))
        fatal_error( "'%s/%s' is not a socket\n", serverdir, sock_name );

    if (st.st_uid != getuid())
        fatal_error( "'%s/%s' is not owned by you\n", serverdir, sock_name );

    return 1;
}

static int connect_to_socket( const char* const sock_name )
{
    struct sockaddr_un addr;
    int s, slen;

    addr.sun_family = AF_UNIX;
    strcpy( addr.sun_path, sock_name );
    slen = sizeof(addr) - sizeof(addr.sun_path) + strlen(addr.sun_path) + 1;
#ifdef HAVE_SOCKADDR_SUN_LEN
    addr.sun_len = slen;
#endif
    if ((s = socket( AF_UNIX, SOCK_STREAM, 0 )) == -1) fatal_perror( "socket" );
    if (connect( s, (struct sockaddr *)&addr, slen ) == -1)
    {
        close( s );
        s = -1;
    }
    else
    {
        fcntl( s, F_SETFD, 1 ); /* set close on exec flag */ 
    }

    return s;
}


/***********************************************************************
 *           server_connect
 *
 * Attempt to connect to an existing server socket.
 * We need to be in the server directory already.
 */
static int server_connect( const char *oldcwd, const char *serverdir )
{
    struct stat st;
    int s, retry;
    BOOL unlinked = FALSE;

    /* chdir to the server directory */
    if (chdir( serverdir ) == -1)
    {
        if (errno != ENOENT) fatal_perror( "chdir to %s", serverdir );
        start_server( "." );
        if (chdir( serverdir ) == -1) fatal_perror( "chdir to %s", serverdir );
    }

    /* make sure we are at the right place */
    if (stat( ".", &st ) == -1) fatal_perror( "stat %s", serverdir );
    if (st.st_uid != getuid()) fatal_error( "'%s' is not owned by you\n", serverdir );
    if (st.st_mode & 077) fatal_error( "'%s' must not be accessible by other users\n", serverdir );

restart:
    for (retry = 0; retry < 3; retry++)
    {
        /* if not the first try, wait a bit to leave the server time to exit */
        if (retry) usleep( 100000 * retry * retry );

        /* Check if both sockets exist */
        if ( !socket_exists( SOCKETNAME ) &&
             !socket_exists( FDSOCKETNAME ) )
        {
            start_server( oldcwd );
            if( !socket_exists( SOCKETNAME ) )
                fatal_perror( "lstat %s/%s", serverdir, SOCKETNAME );
        }

        if( check_socket_sanity( SOCKETNAME, serverdir ) &&
            check_socket_sanity( FDSOCKETNAME, serverdir ) )
        {
            s = connect_to_socket( SOCKETNAME );

            if( s != -1 )  return s;
        }
    }

    if (!unlinked++)
    {
        unlink(SOCKETNAME);
        unlink(FDSOCKETNAME);
        goto restart;
    }

    fatal_error( "file '%s/%s' exists,\n"
                 "   but I cannot connect to it; maybe the wineserver has crashed?\n"
                 "   If this is the case, you should remove this socket file and try again.\n",
                 serverdir, SOCKETNAME );
}

static int fd_server_connect( const char *serverdir )
{
    int s;

    s = connect_to_socket( FDSOCKETNAME );

    if( s != -1 ) return s;

    fatal_error( "file '%s/%s' exists,\n"
                 "   but I cannot connect to it; maybe the wineserver has crashed?\n"
                 "   If this is the case, you should remove this socket file and try again.\n",
                 serverdir, FDSOCKETNAME );

    return -1;
}



/***********************************************************************
 *           CLIENT_InitServer
 *
 * Start the server and create the initial socket pair.
 */
void CLIENT_InitServer(void)
{
    int size;
    char hostname[64];
    char *user_env;
    char *oldcwd;
    char *serverdir;
    const char *configdir;
    handle_t dummy_handle;

    /* retrieve the current directory */
    for (size = 512; ; size *= 2)
    {
        if (!(oldcwd = malloc( size ))) break;
        if (getcwd( oldcwd, size )) break;
        free( oldcwd );
        if (errno == ERANGE) continue;
        oldcwd = NULL;
        break;
    }

    /* if argv[0] is a relative path, make it absolute */
    full_argv0 = argv0;
    if (oldcwd && argv0[0] != '/' && strchr( argv0, '/' ))
    {
        char *new_argv0 = malloc( strlen(oldcwd) + strlen(argv0) + 2 );
        if (new_argv0)
        {
            strcpy( new_argv0, oldcwd );
            strcat( new_argv0, "/" );
            strcat( new_argv0, argv0 );
            full_argv0 = new_argv0;
        }
    }

    /* get the server directory name */
    if (gethostname( hostname, sizeof(hostname) ) == -1) fatal_perror( "gethostname" );
    if ((user_env = getenv( "USER" )) == NULL) user_env = "DefaultUser";
    configdir = get_config_dir();
    serverdir = malloc( strlen(configdir) + strlen(SERVERDIR) + strlen(hostname) + 1 + strlen(user_env) + 1 );
    if (!serverdir) fatal_error( "out of memory\n" );
    strcpy( serverdir, configdir );
    strcat( serverdir, SERVERDIR );
    strcat( serverdir, hostname );
    strcat( serverdir, "-" );
    strcat( serverdir, user_env );

    /* connect to the server */
    fd_socket = server_connect( oldcwd, serverdir );
    fd_server_socket = fd_server_connect( serverdir );

    /* switch back to the starting directory */
    if (oldcwd)
    {
        chdir( oldcwd );
        free( oldcwd );
    }

    /* setup the signal mask */
    sigemptyset( &block_set );
    sigaddset( &block_set, SIGALRM );
    sigaddset( &block_set, SIGIO );
    sigaddset( &block_set, SIGINT );
    sigaddset( &block_set, SIGHUP );
    sigaddset( &block_set, SIGUSR1 );
    sigaddset( &block_set, SIGUSR2 );

    SYSDEPS_sigprocmask( SIG_BLOCK, &block_set, NULL );

    /* receive the first thread request fd on the main socket */
    NtCurrentTeb()->request_fd = receive_fd( &dummy_handle );

    CLIENT_InitThread (TRUE);
}


/***********************************************************************
 *           CLIENT_InitServerDone
 */
void CLIENT_InitServerDone(void)
{
    SYSDEPS_sigprocmask( SIG_UNBLOCK, &block_set, NULL );
}


/***********************************************************************
 *           CLIENT_InitThread
 *
 * Send an init thread request. Return 0 if OK.
 */
void CLIENT_InitThread (BOOL IsMainThread)
{
    TEB *teb = NtCurrentTeb();
    int version, ret;
    int reply_pipe[2];
    int unique_msg_id;
    pid_t wineserver_pid;
#ifdef __APPLE__
    char PortName[BOOTSTRAP_MAX_NAME_LEN];
    mach_port_t ServerPort;
    mach_msg_header_t msg;
#endif

    /* ignore SIGPIPE so that we get a EPIPE error instead  */
    signal( SIGPIPE, SIG_IGN );

    /* create the server->client communication pipes */
    if (IsMainThread)
    {
       if (pipe (reply_pipe) == -1)
          server_protocol_perror ("reply_pipe");
       wine_server_send_fd (reply_pipe[1]);
       teb->reply_fd = reply_pipe[0];
       fcntl (teb->reply_fd, F_SETFD, 1);
    }

 restart:
    if (pipe( teb->wait_fd ) == -1) server_protocol_perror( "wait_pipe" );
    wine_server_send_fd( teb->wait_fd[1] );

    /* set close on exec flag */
    fcntl( teb->wait_fd[0], F_SETFD, 1 );
    fcntl( teb->wait_fd[1], F_SETFD, 1 );

    SERVER_START_REQ( init_thread )
    {
        req->unix_tid_or_pid    = wine_gettid_or_pid();
        req->teb         = teb;
        req->entry       = teb->entry_point;
        if (IsMainThread)
           req->reply_fd = reply_pipe[1];
        else
           req->reply_fd = -1;
        req->wait_fd     = teb->wait_fd[1];
        ret = wine_server_call( req );
        teb->pid = reply->pid;
        wineserver_pid = reply->wineserver_pid;
        unique_msg_id = reply->unique_msg_id;
        teb->tid = reply->tid;
        version  = reply->version;
        if (reply->boot) boot_thread_id = teb->tid;
        else if (boot_thread_id == teb->tid) boot_thread_id = 0;
        if (IsMainThread)
           close (reply_pipe[1]);
    }
    SERVER_END_REQ;

    if (ret == STATUS_INVALID_HANDLE)
    {
       close (teb->wait_fd[0]);
       close (teb->wait_fd[1]);
       IsMainThread = FALSE; /* avoid re-sending reply_fd; we don't
                                handle case of reply_fd going missing on
                                initial main thread startup */
       goto restart;
    }

    if (ret) server_protocol_error( "init_thread failed with status %x\n", ret );
    if (version != SERVER_PROTOCOL_VERSION)
        server_protocol_error( "version mismatch %d/%d.\n"
                               "Your %s binary was not upgraded correctly,\n"
                               "or you have an older one somewhere in your PATH.\n"
                               "Or maybe the wrong wineserver is still running?\n",
                               version, SERVER_PROTOCOL_VERSION,
                               (version > SERVER_PROTOCOL_VERSION) ? "wine" : "wineserver" );

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
       req->entry         = teb->entry_point;
       req->is_process    = 0;
       wine_server_call (req);
    }
    SERVER_END_REQ;
#endif
}


/***********************************************************************
 *           CLIENT_BootDone
 *
 * Signal that we have finished booting, and set debug level.
 */
void CLIENT_BootDone( int debug_level )
{
    use_scheduler = !is_scheduler_disabled();

    SERVER_START_REQ( boot_done )
    {
        req->debug_level = debug_level;
        req->use_scheduler = use_scheduler;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}


/***********************************************************************
 *           CLIENT_IsBootThread
 *
 * Return TRUE if current thread is the boot thread.
 */
int CLIENT_IsBootThread(void)
{
    return (GetCurrentThreadId() == (DWORD)boot_thread_id);
}
