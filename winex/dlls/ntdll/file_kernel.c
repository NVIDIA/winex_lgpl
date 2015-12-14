/*
 * File handling functions
 *
 * Copyright 1993 John Burton
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 *
 * TODO:
 *    Fix the CopyFileEx methods to implement the "extended" functionality.
 *    Right now, they simply call the CopyFile method.
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
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "drive.h"
#include "wine/file.h"
#include "heap.h"
#include "msdos.h"
#include "wincon.h"
#include "wine/mem_file.h"
#include "wine/debug.h"

#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(file);

/***********************************************************************
 *                  Asynchronous file I/O                              *
 */
#include "async.h"

static DWORD fileio_get_async_status (const async_private *ovp);
static DWORD fileio_get_async_count (const async_private *ovp);
static void fileio_set_async_status (async_private *ovp, const DWORD status);
static void CALLBACK fileio_call_completion_func (ULONG_PTR data);
static void fileio_async_cleanup (async_private *ovp);

static async_ops fileio_async_ops =
{
    fileio_get_async_status,       /* get_status */
    fileio_set_async_status,       /* set_status */
    fileio_get_async_count,        /* get_count */
    fileio_call_completion_func,   /* call_completion */
    fileio_async_cleanup           /* cleanup */
};

static async_ops fileio_nocomp_async_ops =
{
    fileio_get_async_status,       /* get_status */
    fileio_set_async_status,       /* set_status */
    fileio_get_async_count,        /* get_count */
    NULL,                          /* call_completion */
    fileio_async_cleanup           /* cleanup */
};

typedef struct async_fileio
{
    struct async_private             async;
    LPOVERLAPPED_COMPLETION_ROUTINE  completion_func;
    char                             *buffer;
    int                              count;
    enum fd_type                     fd_type;
} async_fileio;

static DWORD fileio_get_async_status (const struct async_private *ovp)
{
    return ovp->lpOverlapped->Internal;
}

static void fileio_set_async_status (async_private *ovp, const DWORD status)
{
    ovp->lpOverlapped->Internal = status;
}

static DWORD fileio_get_async_count (const struct async_private *ovp)
{
    async_fileio *fileio = (async_fileio*) ovp;
    DWORD ret = fileio->count - fileio->async.lpOverlapped->InternalHigh;
    return (ret < 0 ? 0 : ret);
}

static void CALLBACK fileio_call_completion_func (ULONG_PTR data)
{
    async_fileio *ovp = (async_fileio*) data;
    TRACE ("data: %p\n", ovp);

    ovp->completion_func( RtlNtStatusToDosError ( ovp->async.lpOverlapped->Internal ),
                          ovp->async.lpOverlapped->InternalHigh,
                          ovp->async.lpOverlapped );

    fileio_async_cleanup ( &ovp->async );
}

static void fileio_async_cleanup ( struct async_private *ovp )
{
    HeapFree ( GetProcessHeap(), 0, ovp );
}

/***********************************************************************/

#if defined(MAP_ANONYMOUS) && !defined(MAP_ANON)
#define MAP_ANON MAP_ANONYMOUS
#endif

/* Size of per-process table of DOS handles */
#define DOS_TABLE_SIZE 256

/* Macro to derive file offset from OVERLAPPED struct */
#define OVERLAPPED_OFFSET(overlapped) ((off_t) (overlapped)->Offset + ((off_t) (overlapped)->OffsetHigh << 32))

static HANDLE dos_handles[DOS_TABLE_SIZE];

static CRITICAL_SECTION FILE_sync_cs;

void create_file_cs(void)
{
  InitializeCriticalSection( &FILE_sync_cs );
  CRITICAL_SECTION_NAME( &FILE_sync_cs, "FILE_sync_cs" );
}


/***********************************************************************
 *              set_scheduling_mode
 */
static int set_scheduling_mode( int mode )
{
    int old_mode;

    SERVER_START_REQ( set_scheduling_mode )
    {
        req->handle = GetCurrentThread();
        req->mode = mode;
        wine_server_call( req );
        old_mode = reply->old_mode;
    }
    SERVER_END_REQ;

    return old_mode;
}

/***********************************************************************
 *              FILE_ConvertOFMode
 *
 * Convert OF_* mode into flags for CreateFile.
 */
static void FILE_ConvertOFMode( INT mode, DWORD *access, DWORD *sharing )
{
    switch(mode & 0x03)
    {
    case OF_READ:      *access = GENERIC_READ; break;
    case OF_WRITE:     *access = GENERIC_WRITE; break;
    case OF_READWRITE: *access = GENERIC_READ | GENERIC_WRITE; break;
    default:           *access = 0; break;
    }
    switch(mode & 0x70)
    {
    case OF_SHARE_EXCLUSIVE:  *sharing = 0; break;
    case OF_SHARE_DENY_WRITE: *sharing = FILE_SHARE_READ; break;
    case OF_SHARE_DENY_READ:  *sharing = FILE_SHARE_WRITE; break;
    case OF_SHARE_DENY_NONE:
    case OF_SHARE_COMPAT:
    default:                  *sharing = FILE_SHARE_READ | FILE_SHARE_WRITE; break;
    }
}


/***********************************************************************
 *              FILE_strcasecmp
 *
 * locale-independent case conversion for file I/O
 */
int FILE_strcasecmp( const char *str1, const char *str2 )
{
    for (;;)
    {
        int ret = FILE_toupper(*str1) - FILE_toupper(*str2);
        if (ret || !*str1) return ret;
        str1++;
        str2++;
    }
}


/***********************************************************************
 *              FILE_strncasecmp
 *
 * locale-independent case conversion for file I/O
 */
int FILE_strncasecmp( const char *str1, const char *str2, int len )
{
    int ret = 0;
    for ( ; len > 0; len--, str1++, str2++)
        if ((ret = FILE_toupper(*str1) - FILE_toupper(*str2)) || !*str1) break;
    return ret;
}


/***********************************************************************
 *           FILE_GetNtStatus(void)
 *
 * Retrieve the Nt Status code from errno.
 * Try to be consistent with FILE_SetDosError().
 */
DWORD FILE_GetNtStatus(void)
{
    int err = errno;
    DWORD nt;
    TRACE ( "errno = %d\n", errno );
    switch ( err )
    {
    case EAGAIN:       nt = STATUS_SHARING_VIOLATION;       break;
    case EBADF:        nt = STATUS_INVALID_HANDLE;          break;
    case ENOSPC:       nt = STATUS_DISK_FULL;               break;
    case EPERM:
    case EROFS:
    case EACCES:       nt = STATUS_ACCESS_DENIED;           break;
    case ENOENT:       nt = STATUS_SHARING_VIOLATION;       break;
    case EISDIR:       nt = STATUS_FILE_IS_A_DIRECTORY;     break;
    case EMFILE:
    case ENFILE:       nt = STATUS_NO_MORE_FILES;           break;
    case EINVAL:
    case ENOTEMPTY:    nt = STATUS_DIRECTORY_NOT_EMPTY;     break;
    case EPIPE:        nt = STATUS_PIPE_BROKEN;             break;
    case EFAULT:       nt = STATUS_ACCESS_VIOLATION;        break;
    case ENOEXEC:      /* ?? */
    case ESPIPE:       /* ?? */
    case EEXIST:       /* ?? */
    default:
        FIXME ( "Converting errno %d to STATUS_UNSUCCESSFUL\n", err );
        nt = STATUS_UNSUCCESSFUL;
    }
    return nt;
}

/***********************************************************************
 *           FILE_SetDosError
 *
 * Set the DOS error code from errno.
 */
void FILE_SetDosError(void)
{
    int save_errno = errno; /* errno gets overwritten by printf */

    TRACE("errno = %d %s\n", errno, strerror(errno));
    switch (save_errno)
    {
    case EAGAIN:
        SetLastError( ERROR_SHARING_VIOLATION );
        break;
    case EBADF:
        SetLastError( ERROR_INVALID_HANDLE );
        break;
    case ENOSPC:
        SetLastError( ERROR_HANDLE_DISK_FULL );
        break;
    case EACCES:
    case EPERM:
    case EROFS:
        SetLastError( ERROR_ACCESS_DENIED );
        break;
    case EBUSY:
        SetLastError( ERROR_LOCK_VIOLATION );
        break;
    case ENOENT:
        SetLastError( ERROR_FILE_NOT_FOUND );
        break;
    case EISDIR:
        SetLastError( ERROR_CANNOT_MAKE );
        break;
    case ENFILE:
    case EMFILE:
        SetLastError( ERROR_NO_MORE_FILES );
        break;
    case EEXIST:
        SetLastError( ERROR_FILE_EXISTS );
        break;
    case EINVAL:
    case ESPIPE:
        SetLastError( ERROR_SEEK );
        break;
    case ENOTEMPTY:
        SetLastError( ERROR_DIR_NOT_EMPTY );
        break;
    case ENOEXEC:
        SetLastError( ERROR_BAD_FORMAT );
        break;
    case EFAULT:
        SetLastError (ERROR_NOACCESS);
        break;
    default:
        WARN("unknown file error: %s\n", strerror(save_errno) );
        SetLastError( ERROR_GEN_FAILURE );
        break;
    }
    errno = save_errno;
}


/***********************************************************************
 *           FILE_DupUnixHandle
 *
 * Duplicate a Unix handle into a task handle.
 * Returns 0 on failure.
 */
HANDLE FILE_DupUnixHandle( int fd, DWORD access, BOOL inherit )
{
    HANDLE ret;

    wine_server_send_fd( fd );

    SERVER_START_REQ( alloc_file_handle )
    {
        req->access  = access;
        req->inherit = inherit;
        req->fd      = fd;
        wine_server_call( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           wine_server_handle_to_fd
 */
int wine_server_handle_to_fd( handle_t handle, unsigned int access, int *unix_fd, enum fd_type *type, int *flags )
{
    int ret, fd = -1;

    *unix_fd = -1;
    do
    {
        SERVER_START_REQ( get_handle_fd )
        {
            req->handle = handle;
            req->access = access;
            if (!(ret = wine_server_call( req )))
            {
                fd = reply->fd;
            }
            if (type) *type = reply->type;
            if (flags) *flags = reply->flags;
        }
        SERVER_END_REQ;
        if (ret) return ret;

        if (fd == -1)  /* it wasn't in the cache, get it from the server */
            fd = wine_server_recv_fd( handle );

    } while (fd == -2);  /* -2 means race condition, so restart from scratch */

    if (fd == -1) /* Couldn't get the fd - possibly ejected disk */
        SetLastError( ERROR_NOT_READY);

    if (fd != -1)
    {
        if ((fd = dup(fd)) == -1)
            SetLastError( ERROR_TOO_MANY_OPEN_FILES );
    }

    *unix_fd = fd;
    return 0;
}

/***********************************************************************
 *           FILE_GetUnixHandleType
 *
 * Retrieve the Unix handle corresponding to a file handle.
 * Returns -1 on failure.
 */
int FILE_GetUnixHandleType( HANDLE handle, DWORD access, enum fd_type *type, DWORD *flags_ptr )
{
    int res, flags, fd = -1;
    res = wine_server_handle_to_fd( handle, access, &fd, type, &flags );
    if (flags_ptr) *flags_ptr = flags;
    if (res) {
        SetLastError( RtlNtStatusToDosError(res) );
        return -1;
    }
    if (((access & GENERIC_READ)  && (flags & FD_FLAG_RECV_SHUTDOWN)) ||
        ((access & GENERIC_WRITE) && (flags & FD_FLAG_SEND_SHUTDOWN)))
    {
        close (fd);
        SetLastError ( ERROR_PIPE_NOT_CONNECTED );
        return -1;
    }
    return fd;
}

/***********************************************************************
 *           FILE_GetUnixHandle
 *
 * Retrieve the Unix handle corresponding to a file handle.
 * Returns -1 on failure or if the handle <handle> represents
 * a directory handle.
 */
int FILE_GetUnixHandle( HANDLE handle, DWORD access )
{
    enum fd_type    type;
    int             result;
    

    result = FILE_GetUnixHandleType( handle, access, &type, NULL );

    if (type == FD_TYPE_DIRECTORY)
        result = -1;


    return result;
}

/*************************************************************************
 * 		FILE_OpenConsole
 *
 * Open a handle to the current process console.
 * Returns 0 on failure.
 */
static HANDLE FILE_OpenConsole( BOOL output, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES sa )
{
    HANDLE ret;

    SERVER_START_REQ( open_console )
    {
        req->from    = output;
        req->access  = access;
	req->share   = sharing;
        req->inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           FILE_CreateFile
 *
 * Implementation of CreateFile. Takes a Unix path name.
 * Returns 0 on failure.
 */
HANDLE FILE_CreateFile( LPCSTR filename, DWORD access, DWORD sharing,
                        LPSECURITY_ATTRIBUTES sa, DWORD creation,
                        DWORD attributes, HANDLE template,
                        UINT drive_type, UINT drive )
{
    unsigned int    err;
    HANDLE          ret;
    int             openForWrite;


    /* FIXME: Windows NT/XP/Vista file access is much more granular than 9x/Me.
     * A lot of our code was written in the 9x days where the only flags supported
     * were GENERIC_READ/WRITE, DELETE, and FILE_WRITE_ATTRIBUTES, so there are
     * plenty of places where GENERIC_READ/WRITE is assumed as being the only way
     * to read and write a file.  The proper way to implement this would be to
     * switch all of our underlying code here and in wineserver to use the more
     * granular FILE_WRITE/READ_DATA flags specifically, but that could be a
     * relatively big project.  This hack should suffice for now... */
    if (access & FILE_WRITE_DATA) {
	WARN("app requested FILE_WRITE_DATA access, but we're using GENERIC_WRITE\n");
	access |= GENERIC_WRITE;
    }
    if (access & FILE_READ_DATA) {
	WARN("app requested FILE_READ_DATA access, but we're using GENERIC_READ\n");
	access |= GENERIC_READ;
    }

    if( attributes & FILE_FLAG_OVERLAPPED ) {

        /* FIXME: ReadFile under Win95/98/Me requires that the overlapped parameter
         *  be NULL.  ReadFile under Win2k,XP,NT etc requires an overlapped parameter
         *  if the overlapped flag is set.  The quick and dirty solution is to just
         *  wipe out the overlapped flag so that ReadFile never hits this case, but
         *  we should probably consider a more substantial fix later on.
         */
        static BOOL windows_version_discovered = FALSE;
        static BOOL windows_version_is_nt = FALSE;

        if( windows_version_discovered == FALSE )
        {
            OSVERSIONINFOEXA osvi;
            BOOL bOsVersionInfoEx;

            TRACE("Discovering windows version..\n");

            /* Try calling GetVersionEx using the OSVERSIONINFOEX structure.
             * If that fails, try using the OSVERSIONINFO structure. */

            ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);

            if( !(bOsVersionInfoEx = GetVersionExA ((OSVERSIONINFOA *) &osvi)) )
            {
                osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOA);
                if (! GetVersionExA ( (OSVERSIONINFOA *) &osvi) )
                {
                    WARN("Somebody broke GetVersionExA..\n");
                    return FALSE;
                }
            }
            if( osvi.dwPlatformId == VER_PLATFORM_WIN32_NT )
                windows_version_is_nt = TRUE;
            else
                windows_version_is_nt = FALSE;

            windows_version_discovered = TRUE;
        }

        if( windows_version_is_nt == FALSE )
        {
            attributes &= ~FILE_FLAG_OVERLAPPED;
        }

    } /* FILE_FLAG_OVERLAPPED is valid check */

    for (;;)
    {
        int created = 0;

        SERVER_START_REQ( create_file )
        {
            req->access     = access;
            req->inherit    = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
            req->sharing    = sharing;
            req->create     = creation;
            req->attrs      = attributes;
            req->drive_type = drive_type;
            req->drive      = drive;
            wine_server_add_data( req, filename, strlen(filename) );
            SetLastError(0);
            err = wine_server_call( req );
            ret = reply->handle;
            openForWrite = reply->openForWrite;
            created = reply->created;
        }
        SERVER_END_REQ;

        if (err)
        {
           /* Special case - need to return ERROR_FILE_EXISTS instead of ERROR_ALREADY_EXISTS */
           if (err == STATUS_OBJECT_NAME_COLLISION)
              SetLastError (ERROR_FILE_EXISTS);
           else
              SetLastError (RtlNtStatusToDosError (err));
        }
        else
        {
           if ((creation == CREATE_ALWAYS && !created) ||
               (creation == OPEN_ALWAYS && !created))
              SetLastError (ERROR_ALREADY_EXISTS);
           else
              SetLastError (0);
        }

        if (!ret) WARN("Unable to create file '%s' (GLE %ld)\n", filename, GetLastError());
        else if (!(attributes & FILE_FLAG_OVERLAPPED) && access == GENERIC_READ)
        {

           /* only open a memory version of this file if it is not already open for writing elsewhere */
           if (openForWrite == 0){
              TRACE("creating a memory file for '%s' {handle = %u}\n", filename, ret);

              MEMFILE_CreateMemFile(filename, ret);
           }
        }

        return ret;
    }
}


/***********************************************************************
 *           FILE_CreateDevice
 *
 * Same as FILE_CreateFile but for a device
 * Returns 0 on failure.
 */
HANDLE FILE_CreateDevice( int client_id, DWORD access, LPSECURITY_ATTRIBUTES sa )
{
    HANDLE ret;
    SERVER_START_REQ( create_device )
    {
        req->access  = access;
        req->inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        req->id      = client_id;
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}

static HANDLE FILE_OpenPipe(LPCSTR name, DWORD access, DWORD attrs,
                            LPSECURITY_ATTRIBUTES sa)
{
    WCHAR buffer[MAX_PATH];
    HANDLE ret;
    DWORD len = 0;

    if (name && !(len = MultiByteToWideChar( CP_ACP, 0, name, strlen(name), buffer, MAX_PATH )))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( open_named_pipe )
    {
        req->access = access;
        req->attrs = attrs;
        req->inherit = (sa &&
                        (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        SetLastError(0);
        wine_server_add_data( req, buffer, len * sizeof(WCHAR) );
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    TRACE("Returned %d\n",ret);
    return ret;
}

/*************************************************************************
 * CreateFileA [KERNEL32.@]  Creates or opens a file or other object
 *
 * Creates or opens an object, and returns a handle that can be used to
 * access that object.
 *
 * PARAMS
 *
 * filename     [in] pointer to filename to be accessed
 * access       [in] access mode requested
 * sharing      [in] share mode
 * sa           [in] pointer to security attributes
 * creation     [in] how to create the file
 * attributes   [in] attributes for newly created file
 * template     [in] handle to file with extended attributes to copy
 *
 * RETURNS
 *   Success: Open handle to specified file
 *   Failure: INVALID_HANDLE_VALUE
 *
 * NOTES
 *  Should call SetLastError() on failure.
 *
 * BUGS
 *
 * Doesn't support character devices, template files, or a
 * lot of the 'attributes' flags yet.
 */
HANDLE WINAPI CreateFileA( LPCSTR filename, DWORD access, DWORD sharing,
                              LPSECURITY_ATTRIBUTES sa, DWORD creation,
                              DWORD attributes, HANDLE template )
{
    DOS_FULL_NAME full_name;
    HANDLE ret;

    if (!filename)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return INVALID_HANDLE_VALUE;
    }
    TRACE("%s %s%s%s%s%s%s%s%s\n",filename,
	  ((access & GENERIC_READ)==GENERIC_READ)?"GENERIC_READ ":"",
	  ((access & GENERIC_WRITE)==GENERIC_WRITE)?"GENERIC_WRITE ":"",
	  (!access)?"QUERY_ACCESS ":"",
	  ((sharing & FILE_SHARE_READ)==FILE_SHARE_READ)?"FILE_SHARE_READ ":"",
	  ((sharing & FILE_SHARE_WRITE)==FILE_SHARE_WRITE)?"FILE_SHARE_WRITE ":"",
	  ((sharing & FILE_SHARE_DELETE)==FILE_SHARE_DELETE)?"FILE_SHARE_DELETE ":"",
	  (creation ==CREATE_NEW)?"CREATE_NEW":
	  (creation ==CREATE_ALWAYS)?"CREATE_ALWAYS ":
	  (creation ==OPEN_EXISTING)?"OPEN_EXISTING ":
	  (creation ==OPEN_ALWAYS)?"OPEN_ALWAYS ":
 	  (creation ==TRUNCATE_EXISTING)?"TRUNCATE_EXISTING ":"",
 	  (attributes & FILE_FLAG_BACKUP_SEMANTICS)?
 			"FILE_FLAG_BACKUP_SEMANTICS ":"");

    /* If the name starts with '\\?\', ignore the first 4 chars. */
    if (!strncmp(filename, "\\\\?\\", 4))
    {
        filename += 4;
	if (!strncmp(filename, "UNC\\", 4))
	{
            FIXME("UNC name (%s) not supported.\n", filename );
            SetLastError( ERROR_PATH_NOT_FOUND );
            return INVALID_HANDLE_VALUE;
	}
    }

    if (!strncmp(filename, "\\\\.\\", 4)) {
        if(!strncasecmp(&filename[4],"pipe\\",5))
        {
            TRACE("Opening a pipe: %s\n",filename);
            ret = FILE_OpenPipe( filename, access, attributes, sa );
            goto done;
        }
        else if (isalpha(filename[4]) && filename[5] == ':' && filename[6] == '\0')
        {
            ret = FILE_CreateDevice( (toupper(filename[4]) - 'A') | 0x20000, access, sa );
            goto done;
        }
        else if (!DOSFS_GetDevice( filename ))
        {
            ret = DEVICE_Open( filename+4, access, sa );
            goto done;
        }
	else
        	filename+=4; /* fall into DOSFS_Device case below */
    }

    /* If the name still starts with '\\', it's a UNC name. */
    if (!strncmp(filename, "\\\\", 2))
    {
        FIXME("UNC name (%s) not supported.\n", filename );
        SetLastError( ERROR_PATH_NOT_FOUND );
        return INVALID_HANDLE_VALUE;
    }

    /* If the name contains a DOS wild card (* or ?), do not create a file */
    if(strchr(filename,'*') || strchr(filename,'?'))
    {
        SetLastError (ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }

    /* Open a console for CONIN$ or CONOUT$ */
    if (!strcasecmp(filename, "CONIN$"))
    {
        ret = FILE_OpenConsole( FALSE, access, sharing, sa );
        goto done;
    }
    if (!strcasecmp(filename, "CONOUT$"))
    {
        ret = FILE_OpenConsole( TRUE, access, sharing, sa );
        goto done;
    }

    if (DOSFS_GetDevice( filename ))
    {
        TRACE("opening device '%s'\n", filename );

        if (!(ret = DOSFS_OpenDevice( filename, access, attributes, sa )))
        {
            /* Do not silence this please. It is a critical error. -MM */
            ERR("Couldn't open device '%s'!\n",filename);
            SetLastError( ERROR_FILE_NOT_FOUND );
        }
        goto done;
    }

    /* check for filename, don't check for last entry if creating */
    if (!DOSFS_GetFullName( filename,
			    (creation == OPEN_EXISTING) ||
			    (creation == TRUNCATE_EXISTING),
			    &full_name )) {
	WARN("Unable to get full filename from '%s' (GLE %ld)\n",
	     filename, GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    ret = FILE_CreateFile( full_name.long_name, access, sharing,
                           sa, creation, attributes, template,
                           DRIVE_GetType(full_name.drive), full_name.drive );
 done:
    if (!ret) ret = INVALID_HANDLE_VALUE;
    return ret;
}



/*************************************************************************
 *              CreateFileW              (KERNEL32.@)
 */
HANDLE WINAPI CreateFileW( LPCWSTR filename, DWORD access, DWORD sharing,
                              LPSECURITY_ATTRIBUTES sa, DWORD creation,
                              DWORD attributes, HANDLE template)
{
    LPSTR afn;
    HANDLE res;

    if (!filename || !filename[0])
    {
        SetLastError (ERROR_PATH_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    afn = FILE_strdupWtoA( GetProcessHeap(), 0, filename );
    res = CreateFileA( afn, access, sharing, sa, creation, attributes, template );
    HeapFree( GetProcessHeap(), 0, afn );
    return res;
}


/***********************************************************************
 *           FILE_FillInfo
 *
 * Fill a file information from a struct stat.
 */
static void FILE_FillInfo( struct stat *st, BY_HANDLE_FILE_INFORMATION *info )
{
    LARGE_INTEGER li;

    if (S_ISDIR(st->st_mode))
        info->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    else
        info->dwFileAttributes = FILE_ATTRIBUTE_ARCHIVE;
    if (!(st->st_mode & S_IWUSR))
        info->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;

    RtlSecondsSince1970ToTime( st->st_mtime, &li );
    info->ftCreationTime.dwLowDateTime = li.u.LowPart;
    info->ftCreationTime.dwHighDateTime = li.u.HighPart;
    RtlSecondsSince1970ToTime( st->st_mtime, &li );
    info->ftLastWriteTime.dwLowDateTime = li.u.LowPart;
    info->ftLastWriteTime.dwHighDateTime = li.u.HighPart;
    RtlSecondsSince1970ToTime( st->st_atime, &li );
    info->ftLastAccessTime.dwLowDateTime = li.u.LowPart;
    info->ftLastAccessTime.dwHighDateTime = li.u.HighPart;

    info->dwVolumeSerialNumber = 0;  /* FIXME */
    info->nFileSizeHigh = 0;
    info->nFileSizeLow  = 0;
    if (!S_ISDIR(st->st_mode)) {
        info->nFileSizeHigh = st->st_size >> 32;
        info->nFileSizeLow  = st->st_size & 0xffffffff;
    }
    info->nNumberOfLinks = st->st_nlink;
    info->nFileIndexHigh = 0;
    info->nFileIndexLow  = st->st_ino;
}


/***********************************************************************
 *           FILE_Stat
 *
 * Stat a Unix path name. Return TRUE if OK.
 */
BOOL FILE_Stat( LPCSTR unixName, BY_HANDLE_FILE_INFORMATION *info )
{
    struct stat st;

    if (lstat( unixName, &st ) == -1)
    {
        FILE_SetDosError();
        return FALSE;
    }
    if (!S_ISLNK(st.st_mode)) FILE_FillInfo( &st, info );
    else
    {
        /* do a "real" stat to find out
	   about the type of the symlink destination */
        if (stat( unixName, &st ) == -1)
        {
            FILE_SetDosError();
            return FALSE;
        }
        FILE_FillInfo( &st, info );
        info->dwFileAttributes |= FILE_ATTRIBUTE_SYMLINK;
    }
    return TRUE;
}


/***********************************************************************
 *             GetFileInformationByHandle   (KERNEL32.@)
 */
DWORD WINAPI GetFileInformationByHandle( HANDLE hFile,
                                         BY_HANDLE_FILE_INFORMATION *info )
{
    DWORD ret;
    if (!info) return 0;

    SERVER_START_REQ( get_file_info )
    {
        req->handle = hFile;
        if ((ret = !wine_server_call_err( req )))
        {
            /* FIXME: which file types are supported ?
             * Serial ports (FILE_TYPE_CHAR) are not,
             * and MSDN also says that pipes are not supported.
             * FILE_TYPE_REMOTE seems to be supported according to
             * MSDN q234741.txt */
            if ((reply->type == FILE_TYPE_DISK) ||  (reply->type == FILE_TYPE_REMOTE))
            {
                LARGE_INTEGER li;

                RtlSecondsSince1970ToTime( reply->write_time, &li );
                info->ftCreationTime.dwLowDateTime = li.u.LowPart;
                info->ftCreationTime.dwHighDateTime = li.u.HighPart;
                RtlSecondsSince1970ToTime( reply->write_time, &li );
                info->ftLastWriteTime.dwLowDateTime = li.u.LowPart;
                info->ftLastWriteTime.dwHighDateTime = li.u.HighPart;
                RtlSecondsSince1970ToTime( reply->access_time, &li );
                info->ftLastAccessTime.dwLowDateTime = li.u.LowPart;
                info->ftLastAccessTime.dwHighDateTime = li.u.HighPart;
                info->dwFileAttributes     = reply->attr;
                info->dwVolumeSerialNumber = reply->serial;
                info->nFileSizeHigh        = reply->size_high;
                info->nFileSizeLow         = reply->size_low;
                info->nNumberOfLinks       = reply->links;
                info->nFileIndexHigh       = reply->index_high;
                info->nFileIndexLow        = reply->index_low;
            }
            else
            {
                SetLastError(ERROR_NOT_SUPPORTED);
                ret = 0;
            }
        }
    }
    SERVER_END_REQ;
    return ret;
}


/**************************************************************************
 *           GetFileAttributes   (KERNEL.420)
 */
DWORD WINAPI GetFileAttributes16( LPCSTR name )
{
    return GetFileAttributesA( name );
}


/**************************************************************************
 *           GetFileAttributesA   (KERNEL32.@)
 */
DWORD WINAPI GetFileAttributesA( LPCSTR name )
{
    DOS_FULL_NAME full_name;
    BY_HANDLE_FILE_INFORMATION info;

    if (name == NULL)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return -1;
    }
    if (!DOSFS_GetFullName( name, TRUE, &full_name) )
        return -1;
    if (!FILE_Stat( full_name.long_name, &info )) return -1;
    return info.dwFileAttributes;
}


/**************************************************************************
 *           GetFileAttributesW   (KERNEL32.@)
 */
DWORD WINAPI GetFileAttributesW( LPCWSTR name )
{
    LPSTR nameA = FILE_strdupWtoA( GetProcessHeap(), 0, name );
    DWORD res = GetFileAttributesA( nameA );
    HeapFree( GetProcessHeap(), 0, nameA );
    return res;
}


/***********************************************************************
 *           GetFileSizeEx   (KERNEL32.@)
 */
BOOL WINAPI GetFileSizeEx( HANDLE hFile, PLARGE_INTEGER lpFileSize )
{
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle( hFile, &info )) return FALSE;
    lpFileSize->u.HighPart = info.nFileSizeHigh;
    lpFileSize->u.LowPart = info.nFileSizeLow;
    return TRUE;
}


/***********************************************************************
 *           GetFileSize   (KERNEL32.@)
 */
DWORD WINAPI GetFileSize( HANDLE hFile, LPDWORD filesizehigh )
{
    LARGE_INTEGER filesize;
    if (GetFileSizeEx(hFile, &filesize) == FALSE)
        return INVALID_FILE_SIZE;
    if (filesizehigh)
        *filesizehigh = filesize.u.HighPart;
    return filesize.u.LowPart;
}


/***********************************************************************
 *           GetFileTime   (KERNEL32.@)
 */
BOOL WINAPI GetFileTime( HANDLE hFile, FILETIME *lpCreationTime,
                           FILETIME *lpLastAccessTime,
                           FILETIME *lpLastWriteTime )
{
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle( hFile, &info )) return FALSE;
    if (lpCreationTime)   *lpCreationTime   = info.ftCreationTime;
    if (lpLastAccessTime) *lpLastAccessTime = info.ftLastAccessTime;
    if (lpLastWriteTime)  *lpLastWriteTime  = info.ftLastWriteTime;
    return TRUE;
}

/***********************************************************************
 *           CompareFileTime   (KERNEL32.@)
 */
INT WINAPI CompareFileTime( LPFILETIME x, LPFILETIME y )
{
        if (!x || !y) return -1;

	if (x->dwHighDateTime > y->dwHighDateTime)
		return 1;
	if (x->dwHighDateTime < y->dwHighDateTime)
		return -1;
	if (x->dwLowDateTime > y->dwLowDateTime)
		return 1;
	if (x->dwLowDateTime < y->dwLowDateTime)
		return -1;
	return 0;
}

/***********************************************************************
 *           FILE_GetTempFileName : utility for GetTempFileName
 */
static UINT FILE_GetTempFileName( LPCSTR path, LPCSTR prefix, UINT unique,
                                  LPSTR buffer, BOOL isWin16 )
{
    static UINT unique_temp;
    DOS_FULL_NAME full_name;
    int i;
    LPSTR p;
    UINT num;

    if ( !path || !buffer )
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (!unique_temp) unique_temp = time(NULL) & 0xffff;
    num = unique ? (unique & 0xffff) : (unique_temp++ & 0xffff);

    strcpy( buffer, path );
    p = buffer + strlen(buffer);

    if (p == buffer || /* path is empty */
            p[-1] != '\\') /* doesn't have a trailing \ */
        *p++ = '\\'; /* add the trailing slash. */

    if (isWin16) *p++ = '~';
    if (prefix)
        for (i = 3; (i > 0) && (*prefix); i--) *p++ = *prefix++;
    sprintf( p, "%x.tmp", num );

    /* Now try to create it */

    if (!unique)
    {
        do
        {
            HFILE handle = CreateFileA( buffer, GENERIC_WRITE, 0, NULL,
                                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
            if (handle != INVALID_HANDLE_VALUE)
            {  /* We created it */
                TRACE("created %s\n",
			      buffer);
                CloseHandle( handle );
                break;
            }
            if (GetLastError() != ERROR_FILE_EXISTS)
                break;  /* No need to go on */
            num++;
            sprintf( p, "%04x.tmp", num );
        } while (num != (unique & 0xffff));
    }

    /* Get the full path name */

    if (DOSFS_GetFullName( buffer, FALSE, &full_name ))
    {
        /* Check if we have write access in the directory */
        if ((p = strrchr( full_name.long_name, '/' ))) *p = '\0';
        if (access( full_name.long_name, W_OK ) == -1)
            WARN("returns '%s', which doesn't seem to be writeable.\n",
		 buffer);
    }
    TRACE("returning %s\n", buffer );
    return unique ? unique : num;
}


/***********************************************************************
 *           GetTempFileNameA   (KERNEL32.@)
 */
UINT WINAPI GetTempFileNameA( LPCSTR path, LPCSTR prefix, UINT unique,
                                  LPSTR buffer)
{
    return FILE_GetTempFileName(path, prefix, unique, buffer, FALSE);
}

/***********************************************************************
 *           GetTempFileNameW   (KERNEL32.@)
 */
UINT WINAPI GetTempFileNameW( LPCWSTR path, LPCWSTR prefix, UINT unique,
                                  LPWSTR buffer )
{
    LPSTR   patha,prefixa;
    char    buffera[MAX_PATH];
    UINT  ret;

    if (!path) return 0;
    patha   = FILE_strdupWtoA( GetProcessHeap(), 0, path );
    if (prefix)
        prefixa = FILE_strdupWtoA( GetProcessHeap(), 0, prefix );
    else
        prefixa = NULL;
    ret     = FILE_GetTempFileName( patha, prefixa, unique, buffera, FALSE );
    MultiByteToWideChar( CP_ACP, 0, buffera, -1, buffer, MAX_PATH );
    HeapFree( GetProcessHeap(), 0, patha );
    if (prefix)
        HeapFree( GetProcessHeap(), 0, prefixa );
    return ret;
}


/***********************************************************************
 *           GetTempFileName   (KERNEL.97)
 */
UINT16 WINAPI GetTempFileName16( BYTE drive, LPCSTR prefix, UINT16 unique,
                                 LPSTR buffer )
{
    char temppath[144];

    if (!(drive & ~TF_FORCEDRIVE)) /* drive 0 means current default drive */
        drive |= DRIVE_GetCurrentDrive() + 'A';

    if ((drive & TF_FORCEDRIVE) &&
        !DRIVE_IsValid( toupper(drive & ~TF_FORCEDRIVE) - 'A' ))
    {
        drive &= ~TF_FORCEDRIVE;
        WARN("invalid drive %d specified\n", drive );
    }

    if (drive & TF_FORCEDRIVE)
        sprintf(temppath,"%c:", drive & ~TF_FORCEDRIVE );
    else
        GetTempPathA( 132, temppath );
    return (UINT16)FILE_GetTempFileName( temppath, prefix, unique, buffer, TRUE );
}

/***********************************************************************
 *           FILE_DoOpenFile
 *
 * Implementation of OpenFile16() and OpenFile32().
 */
static HFILE FILE_DoOpenFile( LPCSTR name, OFSTRUCT *ofs, UINT mode,
                                BOOL win32 )
{
    HFILE hFileRet;
    FILETIME filetime;
    WORD filedatetime[2];
    DOS_FULL_NAME full_name;
    DWORD access, sharing, len;
    char *p;

    if (!ofs) return HFILE_ERROR;

    TRACE("%s %s %s %s%s%s%s%s%s%s%s%s\n",name,
	  ((mode & 0x3 )==OF_READ)?"OF_READ":
	  ((mode & 0x3 )==OF_WRITE)?"OF_WRITE":
	  ((mode & 0x3 )==OF_READWRITE)?"OF_READWRITE":"unknown",
	  ((mode & 0x70 )==OF_SHARE_COMPAT)?"OF_SHARE_COMPAT":
	  ((mode & 0x70 )==OF_SHARE_DENY_NONE)?"OF_SHARE_DENY_NONE":
	  ((mode & 0x70 )==OF_SHARE_DENY_READ)?"OF_SHARE_DENY_READ":
	  ((mode & 0x70 )==OF_SHARE_DENY_WRITE)?"OF_SHARE_DENY_WRITE":
	  ((mode & 0x70 )==OF_SHARE_EXCLUSIVE)?"OF_SHARE_EXCLUSIVE":"unknown",
	  ((mode & OF_PARSE )==OF_PARSE)?"OF_PARSE ":"",
	  ((mode & OF_DELETE )==OF_DELETE)?"OF_DELETE ":"",
	  ((mode & OF_VERIFY )==OF_VERIFY)?"OF_VERIFY ":"",
	  ((mode & OF_SEARCH )==OF_SEARCH)?"OF_SEARCH ":"",
	  ((mode & OF_CANCEL )==OF_CANCEL)?"OF_CANCEL ":"",
	  ((mode & OF_CREATE )==OF_CREATE)?"OF_CREATE ":"",
	  ((mode & OF_PROMPT )==OF_PROMPT)?"OF_PROMPT ":"",
	  ((mode & OF_EXIST )==OF_EXIST)?"OF_EXIST ":"",
	  ((mode & OF_REOPEN )==OF_REOPEN)?"OF_REOPEN ":""
	  );


    ofs->nErrCode = 0;
    if (mode & OF_REOPEN) name = (char *)ofs->szPathName;

    if (!name) {
	ERR("called with `name' set to NULL ! Please debug.\n");
	return HFILE_ERROR;
    }

    TRACE("%s %04x\n", name, mode );

    /* the watcom 10.6 IDE relies on a valid path returned in ofs->szPathName
       Are there any cases where getting the path here is wrong?
       Uwe Bonnes 1997 Apr 2 */
    if (!(len = GetFullPathNameA( name, sizeof(ofs->szPathName),
                                  (char *)ofs->szPathName, NULL )))
       goto error;
    if (len > sizeof (ofs->szPathName))
    {
       SetLastError (ERROR_FILENAME_EXCED_RANGE);
       goto error;
    }

    FILE_ConvertOFMode( mode, &access, &sharing );

    /* OF_PARSE simply fills the structure */

    if (mode & OF_PARSE)
    {
        ofs->cBytes = sizeof (OFSTRUCT);
        ofs->fFixedDisk = (GetDriveType16( ofs->szPathName[0]-'A' )
                           != DRIVE_REMOVABLE);
        TRACE("(%s): OF_PARSE, res = '%s'\n",
                      name, ofs->szPathName );
        return 0;
    }

    /* OF_CREATE is completely different from all other options, so
       handle it first */

    if (mode & OF_CREATE)
    {
        if ((hFileRet = CreateFileA( name, GENERIC_READ | GENERIC_WRITE,
                                       sharing, NULL, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, 0 ))== INVALID_HANDLE_VALUE)
            goto error;
        goto success;
    }

    /* If OF_SEARCH is set, ignore the given path */

    if ((mode & OF_SEARCH) && !(mode & OF_REOPEN))
    {
        /* First try the file name as is */
        if (DOSFS_GetFullName( name, TRUE, &full_name )) goto found;
        /* Now remove the path */
        if (name[0] && (name[1] == ':')) name += 2;
        if ((p = strrchr( name, '\\' ))) name = p + 1;
        if ((p = strrchr( name, '/' ))) name = p + 1;
        if (!name[0]) goto not_found;
    }

    /* Now look for the file */

    if (!DIR_SearchPath( NULL, name, NULL, &full_name, win32 )) goto not_found;

found:
    TRACE("found %s = %s\n",
                  full_name.long_name, full_name.short_name );
    lstrcpynA( (char *)ofs->szPathName, full_name.short_name,
                 sizeof(ofs->szPathName) );

    if (mode & OF_SHARE_EXCLUSIVE)
      /* Some InstallShield version uses OF_SHARE_EXCLUSIVE
	 on the file <tempdir>/_ins0432._mp to determine how
	 far installation has proceeded.
	 _ins0432._mp is an executable and while running the
	 application expects the open with OF_SHARE_ to fail*/
      /* Probable FIXME:
	 As our loader closes the files after loading the executable,
	 we can't find the running executable with FILE_InUse.
	 The loader should keep the file open, as Windows does that, too.
       */
      {
	char *last = strrchr(full_name.long_name,'/');
	if (!last)
	  last = full_name.long_name - 1;
	if (GetModuleHandle16(last+1))
	  {
	    TRACE("Denying shared open for %s\n",full_name.long_name);
	    return HFILE_ERROR;
	  }
      }

    if (mode & OF_DELETE)
    {
        ofs->cBytes = sizeof (OFSTRUCT);
        if (unlink( full_name.long_name ) == -1) goto not_found;
        TRACE("(%s): OF_DELETE return = OK\n", name);
        return TRUE;
    }

    hFileRet = FILE_CreateFile( full_name.long_name, access, sharing,
                                NULL, OPEN_EXISTING, 0, 0,
                                DRIVE_GetType(full_name.drive), full_name.drive );
    if (!hFileRet) goto not_found;

    GetFileTime( hFileRet, NULL, NULL, &filetime );
    FileTimeToDosDateTime( &filetime, &filedatetime[0], &filedatetime[1] );
    if ((mode & OF_VERIFY) && (mode & OF_REOPEN))
    {
        if (memcmp( ofs->reserved, filedatetime, sizeof(ofs->reserved) ))
        {
            CloseHandle( hFileRet );
            WARN("(%s): OF_VERIFY failed\n", name );
            /* FIXME: what error here? */
            SetLastError( ERROR_FILE_NOT_FOUND );
            goto error;
        }
    }
    memcpy( ofs->reserved, filedatetime, sizeof(ofs->reserved) );

success:  /* We get here if the open was successful */
    TRACE("(%s): OK, return = %d\n", name, hFileRet );
    ofs->cBytes = sizeof (OFSTRUCT);
    if (win32)
    {
        if (mode & OF_EXIST)
        {
            CloseHandle( hFileRet );
            hFileRet = TRUE;
        }
    }
    else
    {
        hFileRet = Win32HandleToDosFileHandle( hFileRet );
        if (hFileRet == HFILE_ERROR16) goto error;
        if (mode & OF_EXIST) /* Return the handle, but close it first */
            _lclose16( hFileRet );
    }
    return hFileRet;

not_found:  /* We get here if the file does not exist */
    WARN("'%s' not found or sharing violation\n", name );
    SetLastError( ERROR_FILE_NOT_FOUND );
    /* fall through */

error:  /* We get here if there was an error opening the file */
    ofs->nErrCode = GetLastError();
    WARN("(%s): return = HFILE_ERROR error= %d\n",
		  name,ofs->nErrCode );
    return HFILE_ERROR;
}


/***********************************************************************
 *           OpenFile   (KERNEL.74)
 *           OpenFileEx (KERNEL.360)
 */
HFILE16 WINAPI OpenFile16( LPCSTR name, OFSTRUCT *ofs, UINT16 mode )
{
    return FILE_DoOpenFile( name, ofs, mode, FALSE );
}


/***********************************************************************
 *           OpenFile   (KERNEL32.@)
 */
HFILE WINAPI OpenFile( LPCSTR name, OFSTRUCT *ofs, UINT mode )
{
    return FILE_DoOpenFile( name, ofs, mode, TRUE );
}


/***********************************************************************
 *           FILE_InitProcessDosHandles
 *
 * Allocates the default DOS handles for a process. Called either by
 * Win32HandleToDosFileHandle below or by the DOSVM stuff.
 */
static void FILE_InitProcessDosHandles( void )
{
    dos_handles[0] = GetStdHandle(STD_INPUT_HANDLE);
    dos_handles[1] = GetStdHandle(STD_OUTPUT_HANDLE);
    dos_handles[2] = GetStdHandle(STD_ERROR_HANDLE);
    dos_handles[3] = GetStdHandle(STD_ERROR_HANDLE);
    dos_handles[4] = GetStdHandle(STD_ERROR_HANDLE);
}

/***********************************************************************
 *           Win32HandleToDosFileHandle   (KERNEL32.21)
 *
 * Allocate a DOS handle for a Win32 handle. The Win32 handle is no
 * longer valid after this function (even on failure).
 *
 * Note: this is not exactly right, since on Win95 the Win32 handles
 *       are on top of DOS handles and we do it the other way
 *       around. Should be good enough though.
 */
HFILE WINAPI Win32HandleToDosFileHandle( HANDLE handle )
{
    int i;

    if (!handle || (handle == INVALID_HANDLE_VALUE))
        return HFILE_ERROR;

    for (i = 5; i < DOS_TABLE_SIZE; i++)
        if (!dos_handles[i])
        {
            dos_handles[i] = handle;
            TRACE("Got %d for h32 %d\n", i, handle );
            return (HFILE)i;
        }
    CloseHandle( handle );
    SetLastError( ERROR_TOO_MANY_OPEN_FILES );
    return HFILE_ERROR;
}


/***********************************************************************
 *           DosFileHandleToWin32Handle   (KERNEL32.20)
 *
 * Return the Win32 handle for a DOS handle.
 *
 * Note: this is not exactly right, since on Win95 the Win32 handles
 *       are on top of DOS handles and we do it the other way
 *       around. Should be good enough though.
 */
HANDLE WINAPI DosFileHandleToWin32Handle( HFILE handle )
{
    HFILE16 hfile = (HFILE16)handle;
    if (hfile < 5 && !dos_handles[hfile]) FILE_InitProcessDosHandles();
    if ((hfile >= DOS_TABLE_SIZE) || !dos_handles[hfile])
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return INVALID_HANDLE_VALUE;
    }
    return dos_handles[hfile];
}


/***********************************************************************
 *           DisposeLZ32Handle   (KERNEL32.22)
 *
 * Note: this is not entirely correct, we should only close the
 *       32-bit handle and not the 16-bit one, but we cannot do
 *       this because of the way our DOS handles are implemented.
 *       It shouldn't break anything though.
 */
void WINAPI DisposeLZ32Handle( HANDLE handle )
{
    int i;

    if (!handle || (handle == INVALID_HANDLE_VALUE)) return;

    for (i = 5; i < DOS_TABLE_SIZE; i++)
        if (dos_handles[i] == handle)
        {
            dos_handles[i] = 0;
            CloseHandle( handle );
            break;
        }
}


/***********************************************************************
 *           FILE_Dup2
 *
 * dup2() function for DOS handles.
 */
HFILE16 FILE_Dup2( HFILE16 hFile1, HFILE16 hFile2 )
{
    HANDLE new_handle;

    if (hFile1 < 5 && !dos_handles[hFile1]) FILE_InitProcessDosHandles();

    if ((hFile1 >= DOS_TABLE_SIZE) || (hFile2 >= DOS_TABLE_SIZE) || !dos_handles[hFile1])
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return HFILE_ERROR16;
    }
    if (hFile2 < 5)
    {
        FIXME("stdio handle closed, need proper conversion\n" );
        SetLastError( ERROR_INVALID_HANDLE );
        return HFILE_ERROR16;
    }
    if (!DuplicateHandle( GetCurrentProcess(), dos_handles[hFile1],
                          GetCurrentProcess(), &new_handle,
                          0, FALSE, DUPLICATE_SAME_ACCESS ))
        return HFILE_ERROR16;
    if (dos_handles[hFile2]) CloseHandle( dos_handles[hFile2] );
    dos_handles[hFile2] = new_handle;
    return hFile2;
}


/***********************************************************************
 *           _lclose   (KERNEL.81)
 */
HFILE16 WINAPI _lclose16( HFILE16 hFile )
{
    if (hFile < 5)
    {
        FIXME("stdio handle closed, need proper conversion\n" );
        SetLastError( ERROR_INVALID_HANDLE );
        return HFILE_ERROR16;
    }
    if ((hFile >= DOS_TABLE_SIZE) || !dos_handles[hFile])
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return HFILE_ERROR16;
    }
    TRACE("%d (handle32=%d)\n", hFile, dos_handles[hFile] );
    CloseHandle( dos_handles[hFile] );
    dos_handles[hFile] = 0;
    return 0;
}


/***********************************************************************
 *           _lclose   (KERNEL32.@)
 */
HFILE WINAPI _lclose( HFILE hFile )
{
    TRACE("handle %d\n", hFile );
    return CloseHandle( hFile ) ? 0 : HFILE_ERROR;
}

/***********************************************************************
 *              GetOverlappedResult     (KERNEL32.@)
 *
 * Check the result of an Asynchronous data transfer from a file.
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 *  If successful (and relevant) lpTransferred will hold the number of
 *   bytes transferred during the async operation.
 *
 * BUGS
 *
 * Currently only works for WaitCommEvent, ReadFile, WriteFile
 *   with communications ports.
 *
 */
BOOL WINAPI GetOverlappedResult(
    HANDLE hFile,              /* [in] handle of file to check on */
    LPOVERLAPPED lpOverlapped, /* [in/out] pointer to overlapped  */
    LPDWORD lpTransferred,     /* [in/out] number of bytes transferred  */
    BOOL bWait                 /* [in] wait for the transfer to complete ? */
) {
    DWORD r;

    TRACE("(%d %p %p %x)\n", hFile, lpOverlapped, lpTransferred, bWait);

    if(lpOverlapped==NULL)
    {
        ERR("lpOverlapped was null\n");
        return FALSE;
    }
    if(!lpOverlapped->hEvent)
    {
	if (bWait)
	{
	    /* Wait for an APC to change the overlapped status.
	     * Hopefully this is good enough... */
	    while ( lpOverlapped->Internal == STATUS_PENDING ) {
		r = WaitForMultipleObjectsEx( 0, NULL, FALSE, INFINITE, TRUE );
	    }
	}
	else {
	    /* According to MSDN, the hEvent member of an OVERLAPPED structure can be
	     *  NULL: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/overlapped_str.asp
	     *
	     *  In this case, we sleep one millisecond to let the APCs run */
	    WaitForMultipleObjectsEx( 0, NULL, FALSE, 1, FALSE );
	}
    }
    else if ( bWait )
    {
        do {
            TRACE("waiting on %p\n",lpOverlapped);
            r = WaitForSingleObjectEx(lpOverlapped->hEvent, INFINITE, TRUE);
            TRACE("wait on %p returned %ld\n",lpOverlapped,r);
        } while (r==STATUS_USER_APC);
    }
    else if ( lpOverlapped->Internal == STATUS_PENDING )
    {
        /* Wait in order to give APCs a chance to run. */
        /* This is cheating, so we must set the event again in case of success -
           it may be a non-manual reset event. */
        do {
            TRACE("waiting on %p\n",lpOverlapped);
            r = WaitForSingleObjectEx(lpOverlapped->hEvent, 0, TRUE);
            TRACE("wait on %p returned %ld\n",lpOverlapped,r);
        } while (r==STATUS_USER_APC);
        if ( r == WAIT_OBJECT_0 )
            NtSetEvent ( lpOverlapped->hEvent, NULL );
    }

    if(lpTransferred && (lpOverlapped->Internal != STATUS_PENDING))
        *lpTransferred = lpOverlapped->InternalHigh;

    switch ( lpOverlapped->Internal )
    {
    case STATUS_SUCCESS:
        return TRUE;
    case STATUS_PENDING:
        SetLastError ( ERROR_IO_INCOMPLETE );
        if ( bWait ) ERR ("PENDING status after waiting!\n");
        return FALSE;
    default:
        SetLastError ( RtlNtStatusToDosError ( lpOverlapped->Internal ) );
        return FALSE;
    }
}

/***********************************************************************
 *             CancelIo                   (KERNEL32.@)
 */
BOOL WINAPI CancelIo(HANDLE handle)
{
    async_private *ovp,*t;

    TRACE("handle = %x\n",handle);

    for (ovp = NtCurrentTeb()->pending_list; ovp; ovp = t)
    {
        t = ovp->next;
        if ( ovp->handle == handle )
             cancel_async ( ovp );
    }
    WaitForMultipleObjectsEx(0,NULL,FALSE,1,TRUE);
    return TRUE;
}

/***********************************************************************
 *             FILE_AsyncReadService      (INTERNAL)
 *
 *  This function is called while the client is waiting on the
 *  server, so we can't make any server calls here.
 */
static void FILE_AsyncReadService(async_private *ovp)
{
    async_fileio *fileio = (async_fileio*) ovp;
    LPOVERLAPPED lpOverlapped = fileio->async.lpOverlapped;
    int result, r;
    int already = lpOverlapped->InternalHigh;

    TRACE("%p %p\n", lpOverlapped, fileio->buffer );

    /* check to see if the data is ready (non-blocking) */

    if ( fileio->fd_type == FD_TYPE_SOCKET )
        result = read (ovp->fd, &fileio->buffer[already], fileio->count - already);
    else
    {
        TRACE("count %d, ofs %Ld\n", fileio->count - already, OVERLAPPED_OFFSET(lpOverlapped) + already);
        result = pread (ovp->fd, &fileio->buffer[already], fileio->count - already,
                        OVERLAPPED_OFFSET (lpOverlapped) + already);
        if ((result < 0) && (errno == ESPIPE))
            result = read (ovp->fd, &fileio->buffer[already], fileio->count - already);
    }

    if ( (result<0) && ((errno == EAGAIN) || (errno == EINTR)))
    {
        TRACE("Deferred read %d\n",errno);
        r = STATUS_PENDING;
        goto async_end;
    }

    /* check to see if the transfer is complete */
    if(result<0)
    {
        r = FILE_GetNtStatus ();
        goto async_end;
    }
    else if ( result == 0 )
    {
        r = ( lpOverlapped->InternalHigh ? STATUS_SUCCESS : STATUS_END_OF_FILE );
        goto async_end;
    }

    lpOverlapped->InternalHigh += result;
    TRACE("read %d more bytes %ld/%d so far\n",result,lpOverlapped->InternalHigh,fileio->count);

    if(lpOverlapped->InternalHigh >= fileio->count || fileio->fd_type == FD_TYPE_SOCKET )
        r = STATUS_SUCCESS;
    else
        r = STATUS_PENDING;

async_end:
    lpOverlapped->Internal = r;
}

/***********************************************************************
 *              FILE_ReadFileEx                (INTERNAL)
 */
static BOOL FILE_ReadFileEx(HANDLE hFile, LPVOID buffer, DWORD bytesToRead,
                            LPOVERLAPPED overlapped,
                            LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
                            BOOL isEx)
{
    async_fileio *ovp;
    int fd;
    DWORD flags;
    enum fd_type type;
    struct stat StatBuf;

    TRACE("file %d to buf %p num %ld %p func %p\n",
	  hFile, buffer, bytesToRead, overlapped, lpCompletionRoutine);

    /* check that there is an overlapped struct */
    if (overlapped==NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    fd = FILE_GetUnixHandleType ( hFile, GENERIC_READ, &type, &flags);
    if ( fd < 0 )
    {
        WARN ( "Couldn't get FD\n" );
        return FALSE;
    }

    if ((flags & FD_FLAG_IOCOMPPORT) && lpCompletionRoutine)
    {
       close (fd);
       SetLastError (ERROR_INVALID_PARAMETER);
       return FALSE;
    }

    /* At least XP disallows using ReadFileEx if I/O comp port is bound */
    if ((flags & FD_FLAG_IOCOMPPORT) && isEx)
    {
       close (fd);
       SetLastError (ERROR_INVALID_PARAMETER);
       return FALSE;
    }

    /* attempting to read from a directory handle => fail */
    if (type == FD_TYPE_DIRECTORY){
        close (fd);
        ERR("attempt to read from a directory handle!\n");

        SetLastError(ERROR_INVALID_FUNCTION);

        return FALSE;
    }


    if ((overlapped->hEvent == INVALID_HANDLE_VALUE) &&
        !(flags & FD_FLAG_OVERLAPPED)) {
        /* ReadFileEx doesn't check this itself, so do it here. */
        close(fd);
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    /* Need to error out early if trying to read beyond end of file */
    if (fstat (fd, &StatBuf))
       ERR ("fstat failed! errno: %d\n", errno);
    else
    {
       if (OVERLAPPED_OFFSET (overlapped) > StatBuf.st_size)
       {
          close (fd);
          SetLastError (ERROR_HANDLE_EOF);
          return FALSE;
       }
    }

    ovp = (async_fileio*) HeapAlloc(GetProcessHeap(), 0, sizeof (async_fileio));
    if(!ovp)
    {
        TRACE("HeapAlloc Failed\n");
        close (fd);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto error;
    }

    if (NtCreateEvent (&ovp->async.hStartupEvent, SYNCHRONIZE, NULL, TRUE,
                       FALSE) != STATUS_SUCCESS)
    {
       ERR ("Failed to create startup event\n");
       HeapFree (GetProcessHeap (), 0, ovp);
       close (fd);
       SetLastError (ERROR_NOT_ENOUGH_MEMORY);
       goto error;
    }

    ovp->async.ops = ( lpCompletionRoutine ? &fileio_async_ops : &fileio_nocomp_async_ops );
    ovp->async.handle = hFile;
    ovp->async.fd = fd;
    ovp->async.forceOverlapped = FALSE;
    ovp->async.type = ASYNC_TYPE_READ;
    ovp->async.func = FILE_AsyncReadService;
    ovp->async.lpOverlapped = overlapped;
    ovp->count = bytesToRead;
    ovp->completion_func = lpCompletionRoutine;
    ovp->buffer = buffer;
    ovp->fd_type = type;

    /* Braindead MS API for forcing overlapped I/O even if an
       I/O completion port is bound; we don't need to check for this
       for other event functions as our handle conversion code in the
       wineserver does a >>2, which will just drop the offending bit */
    if ((overlapped->hEvent != INVALID_HANDLE_VALUE) &&
        ((UINT_PTR)overlapped->hEvent & 0x1))
       ovp->async.forceOverlapped = TRUE;

    return !register_new_async (&ovp->async);

error:
    close (fd);
    return FALSE;

}

/***********************************************************************
 *              ReadFileEx                (KERNEL32.@)
 */
BOOL WINAPI ReadFileEx(HANDLE hFile, LPVOID buffer, DWORD bytesToRead,
			 LPOVERLAPPED overlapped,
			 LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    overlapped->InternalHigh = 0;
    return FILE_ReadFileEx(hFile,buffer,bytesToRead,
                           overlapped,lpCompletionRoutine, TRUE);
}

static BOOL FILE_TimeoutRead(HANDLE hFile, LPVOID buffer, DWORD bytesToRead, LPDWORD bytesRead)
{
    OVERLAPPED ov;
    BOOL r = FALSE;

    TRACE("%d %p %ld %p\n", hFile, buffer, bytesToRead, bytesRead );

    ZeroMemory(&ov, sizeof (OVERLAPPED));
    if(STATUS_SUCCESS==NtCreateEvent(&ov.hEvent, SYNCHRONIZE, NULL, 0, 0))
    {
        if(FILE_ReadFileEx(hFile, buffer, bytesToRead,
                           &ov, NULL, FALSE))
        {
            r = GetOverlappedResult(hFile, &ov, bytesRead, TRUE);
        }
    }
    CloseHandle(ov.hEvent);
    return r;
}

/***********************************************************************
 *              ReadFile                (KERNEL32.@)
 */
BOOL WINAPI ReadFile( HANDLE hFile, LPVOID buffer, DWORD bytesToRead,
                        LPDWORD bytesRead, LPOVERLAPPED overlapped )
{
    int unix_handle, result;
    enum fd_type type;
    DWORD flags;
    int mode;
    struct pollfd pfd;
    off_t overlapped_offset = -1;

    TRACE("%d %p %ld %p %p\n", hFile, buffer, bytesToRead,
          bytesRead, overlapped );

    if (bytesRead) *bytesRead = 0;  /* Do this before anything else */
    if (!bytesToRead) return TRUE;

    if (overlapped == NULL && MEMFILE_ReadFile (hFile, buffer, bytesToRead, bytesRead))
       return TRUE;

    unix_handle = FILE_GetUnixHandleType( hFile, GENERIC_READ, &type, &flags );

    /* attempting to read from a directory handle => fail */
    if (type == FD_TYPE_DIRECTORY){
        ERR("attempt to read from a directory handle!\n");

        SetLastError(ERROR_INVALID_FUNCTION);        

        return FALSE;
    }


    if (flags & FD_FLAG_OVERLAPPED)
    {
	if (unix_handle == -1) return FALSE;
        if ( (overlapped==NULL) || (overlapped->hEvent && NtResetEvent( overlapped->hEvent, NULL )) )
        {
            TRACE("Overlapped not specified or invalid event flag\n");
	    close(unix_handle);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        close(unix_handle);
        overlapped->InternalHigh = 0;

        if(!FILE_ReadFileEx(hFile, buffer, bytesToRead, overlapped, NULL,
                            FALSE))
            return FALSE;

        if (flags & FD_FLAG_IOCOMPPORT)
        {
           SetLastError (ERROR_IO_PENDING);
           return FALSE;
        }

        if ( !GetOverlappedResult (hFile, overlapped, bytesRead, FALSE) )
        {
            if ( GetLastError() == ERROR_IO_INCOMPLETE )
                SetLastError ( ERROR_IO_PENDING );
            return FALSE;
        }

        return TRUE;
    }
    if (flags & FD_FLAG_TIMEOUT)
    {
        close(unix_handle);
        return FILE_TimeoutRead(hFile, buffer, bytesToRead, bytesRead);
    }
    switch(type)
    {
    case FD_TYPE_CONSOLE:
	return ReadConsoleA(hFile, buffer, bytesToRead, bytesRead, NULL);

    default:
	/* normal unix files */
	if (unix_handle == -1)
	    return FALSE;
	break;
    }

    /* code for synchronous reads */
    if (overlapped)
    {
        LARGE_INTEGER   offset = {{OVERLAPPED_OFFSET(overlapped)}};
        DWORD           actualBytes;
        BOOL            result;


        if (overlapped->hEvent)
            NtResetEvent (overlapped->hEvent, NULL);

        /* FIXME: we currently fail 8 tests in file_kernel:
            - negative overlapped offset:
              - fail: incorrect error status: returns STATUS_OK instead of STATUS_END_OF_FILE
              - fail: incorrect number of bytes read: reports successful reading of requested bytes instead of 0
              - fail: incorrect number of bytes read in struct: reports successful reading of requested bytes instead of 0
              - fail: read buffer is clobbered
              
            - overlapped offset beyond the end of the file:
              - fail: incorrect error status: returns STATUS_OK instead of STATUS_END_OF_FILE
              - fail: incorrect number of bytes read: reports successful reading of requested bytes instead of 0
              - fail: incorrect number of bytes read in struct: reports successful reading of requested bytes instead of 0
              - fail: read buffer is clobbered

            Note that both of these cases are actually treated the same since the overlapped offset value is
            an unsigned quantity.  The second case will actually fill the buffer with zeros instead of file
            data since it is reading from just beyond the file's end.  The first case will fill the buffer with
            partial file data since the seek operation that actually occurs treats the offset as a negative
            value.

            The overlapped event is successfully signalled in both of these cases.

            The problem here (in both fixing and detecting) is that we don't have a cheap way of accessing
            the file's size.  The GetFileSize[Ex]() functions eventually make a server call to query the
            file's information.  We would like to avoid adding another server call if possible.
        */

        overlapped->Internal = STATUS_PENDING;
        overlapped->InternalHigh = 0;


        /* try to read from the memory file for this handle.  If it succeeds, short-circuit the
           normal reading method and signal the overlapped event.  An overlapped offset of -2 
           indicates that we should read from the current file pointer.  These functions will
           only fail if memory files are disabled or the handle does not have a memory file
           associated with it. */
        if ((offset.QuadPart == -2))
             result = MEMFILE_ReadFile(hFile, buffer, bytesToRead, &actualBytes);
             
        else
             result = MEMFILE_ReadFileFromOffset(hFile, buffer, bytesToRead, &actualBytes, offset);
             
        
        /* the read from the memory file was successful => signal the overlapped event and
             update the structure. */
        if (result)
        { 
            if (overlapped->hEvent)
                NtSetEvent(overlapped->hEvent, NULL);
               
            if ((actualBytes == 0) && !overlapped->InternalHigh)
                overlapped->Internal = STATUS_END_OF_FILE;

            else
            {
                overlapped->Internal = STATUS_SUCCESS;
                overlapped->InternalHigh = actualBytes;
            }
               
               
            if (bytesRead)
                *bytesRead = actualBytes;

            return TRUE;
        }

        overlapped_offset = OVERLAPPED_OFFSET (overlapped);
    }

    /* check if we're going to block */
    pfd.fd = unix_handle;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 0) == 0) {
        /* have to wait, let the wineserver know */
        mode = set_scheduling_mode( SCDM_BLOCKED );
        poll(&pfd, 1, -1);
        set_scheduling_mode( mode );
    }
    /* Even if poll() says the fd is ready, we may still have delays
     * due to the disk spinning up, slow NFS mounts, whatever...
     * but if this is in fact an I/O-heavy thread, it turns out
     * that it may be too risky to leave it in the Linux scheduler's
     * hands, so we'll complete the read in normal scheduling mode. */
    while ((result = (overlapped_offset >= 0) ?
                pread( unix_handle, buffer, bytesToRead, overlapped_offset ) :
                read( unix_handle, buffer, bytesToRead )) == -1)
    {
        if ((errno == EAGAIN) || (errno == EINTR)) continue;
        if ((errno == EFAULT) && !IsBadWritePtr( buffer, bytesToRead )) continue;
        FILE_SetDosError();
        if (overlapped)
            overlapped->Internal = FILE_GetNtStatus ();
        break;
    }
    /* after the overlapped read is done, update the file pointer
       since pread doesn't update it but ReadFile is supposed to */
    if ((overlapped_offset >= 0) && (result >= 0))
    {
        if (lseek (unix_handle, overlapped_offset + result, SEEK_SET) 
             != overlapped_offset + result)
        {
            ERR ("Failed to set file offset to %Ld!\n", overlapped_offset);
        }
    }
    
    close( unix_handle );
    TRACE("read %d bytes\n", result);
    if (result == -1) return FALSE;
    if (bytesRead) *bytesRead = result;

    if (overlapped)
    {
       if (overlapped->hEvent)
          NtSetEvent (overlapped->hEvent, NULL);
       if ((result == 0) && !overlapped->InternalHigh)
          overlapped->Internal = STATUS_END_OF_FILE;
       else
       {
          overlapped->Internal = STATUS_SUCCESS;
          overlapped->InternalHigh = result;
       }
    }

    return TRUE;
}


/***********************************************************************
 *             FILE_AsyncWriteService      (INTERNAL)
 *
 *  This function is called while the client is waiting on the
 *  server, so we can't make any server calls here.
 */
static void FILE_AsyncWriteService(struct async_private *ovp)
{
    async_fileio *fileio = (async_fileio *) ovp;
    LPOVERLAPPED lpOverlapped = fileio->async.lpOverlapped;
    int result, r;
    int already = lpOverlapped->InternalHigh;

    TRACE("(%p %p)\n",lpOverlapped,fileio->buffer);

    /* write some data (non-blocking) */

    if ( fileio->fd_type == FD_TYPE_SOCKET )
        result = write(ovp->fd, &fileio->buffer[already], fileio->count - already);
    else
    {
        TRACE("count %d, ofs %Ld\n", fileio->count - already, OVERLAPPED_OFFSET(lpOverlapped) + already);
        result = pwrite(ovp->fd, &fileio->buffer[already], fileio->count - already,
                    OVERLAPPED_OFFSET (lpOverlapped) + already);
        if ((result < 0) && (errno == ESPIPE))
            result = write(ovp->fd, &fileio->buffer[already], fileio->count - already);
    }

    if ( (result<0) && ((errno == EAGAIN) || (errno == EINTR)))
    {
        r = STATUS_PENDING;
        goto async_end;
    }

    /* check to see if the transfer is complete */
    if(result<0)
    {
        r = FILE_GetNtStatus ();
        goto async_end;
    }

    lpOverlapped->InternalHigh += result;

    TRACE("wrote %d more bytes %ld/%d so far\n",result,lpOverlapped->InternalHigh,fileio->count);

    if(lpOverlapped->InternalHigh < fileio->count)
        r = STATUS_PENDING;
    else
        r = STATUS_SUCCESS;

async_end:
    lpOverlapped->Internal = r;
}

/***********************************************************************
 *              FILE_WriteFileEx
 */
static BOOL FILE_WriteFileEx(HANDLE hFile, LPCVOID buffer, DWORD bytesToWrite,
                             LPOVERLAPPED overlapped,
                             LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
                             BOOL isEx)
{
    async_fileio *ovp;
    int fd;
    DWORD flags;
    enum fd_type type;

    TRACE("file %d to buf %p num %ld %p func %p\n",
	  hFile, buffer, bytesToWrite, overlapped, lpCompletionRoutine);

    if (overlapped == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    fd = FILE_GetUnixHandleType ( hFile, GENERIC_WRITE, &type, &flags );
    if ( fd < 0 )
    {
        TRACE( "Couldn't get FD\n" );
        return FALSE;
    }

    if ((flags & FD_FLAG_IOCOMPPORT) && lpCompletionRoutine)
    {
       close (fd);
       SetLastError (ERROR_INVALID_PARAMETER);
       return FALSE;
    }

    /* At least XP disallows using WriteFileEx if I/O comp port is bound */
    if ((flags & FD_FLAG_IOCOMPPORT) && isEx)
    {
       close (fd);
       SetLastError (ERROR_INVALID_PARAMETER);
       return FALSE;
    }

    /* attempting to write to a directory handle => fail */
    if (type == FD_TYPE_DIRECTORY){
        close (fd);
        ERR("attempt to write to a directory handle!\n");

        SetLastError(ERROR_INVALID_FUNCTION);        

        return FALSE;
    }


    if ((overlapped->hEvent == INVALID_HANDLE_VALUE) &&
        !(flags & FD_FLAG_OVERLAPPED)) {
        /* WriteFileEx doesn't check this itself, so do it here. */
        close(fd);
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    ovp = (async_fileio*) HeapAlloc(GetProcessHeap(), 0, sizeof (async_fileio));
    if(!ovp)
    {
        TRACE("HeapAlloc Failed\n");
        close (fd);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto error;
    }

    if (NtCreateEvent (&ovp->async.hStartupEvent, SYNCHRONIZE, NULL, TRUE,
                       FALSE) != STATUS_SUCCESS)
    {
       ERR ("Failed to create startup event\n");
       HeapFree (GetProcessHeap (), 0, ovp);
       close (fd);
       SetLastError (ERROR_NOT_ENOUGH_MEMORY);
       goto error;
    }

    ovp->async.ops = ( lpCompletionRoutine ? &fileio_async_ops : &fileio_nocomp_async_ops );
    ovp->async.handle = hFile;
    ovp->async.fd = fd;
    ovp->async.forceOverlapped = FALSE;
    ovp->async.type = ASYNC_TYPE_WRITE;
    ovp->async.func = FILE_AsyncWriteService;
    ovp->async.lpOverlapped = overlapped;
    ovp->buffer = (LPVOID) buffer;
    ovp->count = bytesToWrite;
    ovp->completion_func = lpCompletionRoutine;
    ovp->fd_type = type;

    /* Braindead MS API for forcing overlapped I/O even if an
       I/O completion port is bound; we don't need to check for this
       for other event functions as our handle conversion code in the
       wineserver does a >>2, which will just drop the offending bit */
    if ((overlapped->hEvent != INVALID_HANDLE_VALUE) &&
        ((UINT_PTR)overlapped->hEvent & 0x1))
       ovp->async.forceOverlapped = TRUE;

    return !register_new_async (&ovp->async);

error:
    close (fd);
    return FALSE;
}

/***********************************************************************
 *              WriteFileEx                (KERNEL32.@)
 */
BOOL WINAPI WriteFileEx(HANDLE hFile, LPCVOID buffer, DWORD bytesToWrite,
			 LPOVERLAPPED overlapped,
			 LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    overlapped->InternalHigh = 0;

    return FILE_WriteFileEx(hFile, buffer, bytesToWrite,
                            overlapped, lpCompletionRoutine, TRUE);
}

/***********************************************************************
 *             WriteFile               (KERNEL32.@)
 */
BOOL WINAPI WriteFile( HANDLE hFile, LPCVOID buffer, DWORD bytesToWrite,
                         LPDWORD bytesWritten, LPOVERLAPPED overlapped )
{
    int unix_handle, result;
    enum fd_type type;
    DWORD flags;
    int mode;
    struct pollfd pfd;
    off_t overlapped_offset = -1;

    TRACE("%d %p %ld %p %p\n", hFile, buffer, bytesToWrite,
          bytesWritten, overlapped );

    if (bytesWritten) *bytesWritten = 0;  /* Do this before anything else */
    if (!bytesToWrite) return TRUE;

    unix_handle = FILE_GetUnixHandleType( hFile, GENERIC_WRITE, &type, &flags );

    /* attempting to write to a directory handle => fail */
    if (type == FD_TYPE_DIRECTORY){
        ERR("attempt to write to a directory handle!\n");

        SetLastError(ERROR_INVALID_FUNCTION);        

        return FALSE;
    }



    if (flags & FD_FLAG_OVERLAPPED)
    {
	if (unix_handle == -1) return FALSE;
        if ( (overlapped==NULL) || (overlapped->hEvent && NtResetEvent( overlapped->hEvent, NULL )) )
        {
            TRACE("Overlapped not specified or invalid event flag\n");
	    close(unix_handle);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        close(unix_handle);
        overlapped->InternalHigh = 0;

        if(!FILE_WriteFileEx(hFile, buffer, bytesToWrite, overlapped, NULL, FALSE))
            return FALSE;

        if (flags & FD_FLAG_IOCOMPPORT)
        {
           SetLastError (ERROR_IO_PENDING);
           return FALSE;
        }

        if ( !GetOverlappedResult (hFile, overlapped, bytesWritten, FALSE) )
        {
            if ( GetLastError() == ERROR_IO_INCOMPLETE )
                SetLastError ( ERROR_IO_PENDING );
            return FALSE;
        }

        return TRUE;
    }

    switch(type)
    {
    case FD_TYPE_CONSOLE:
	TRACE("%d %s %ld %p %p\n", hFile, debugstr_an(buffer, bytesToWrite), bytesToWrite,
	      bytesWritten, overlapped );
	return WriteConsoleA(hFile, buffer, bytesToWrite, bytesWritten, NULL);
    default:
	if (unix_handle == -1)
	    return FALSE;
    }

    /* synchronous file write */

    if (overlapped)
    {
       if (overlapped->hEvent)
          NtResetEvent (overlapped->hEvent, NULL);
       overlapped->Internal = STATUS_PENDING;

       overlapped_offset = OVERLAPPED_OFFSET (overlapped);
    }

    /* check if we're going to block */
    pfd.fd = unix_handle;
    pfd.events = POLLOUT;
    if (poll(&pfd, 1, 0) == 0) {
        /* have to wait, let the wineserver know */
        mode = set_scheduling_mode( SCDM_BLOCKED );
        poll(&pfd, 1, -1);
        set_scheduling_mode( mode );
    }
    /* Even if poll() says the fd is ready, we may still have delays
     * due to the disk spinning up, slow NFS mounts, whatever...
     * but if this is in fact an I/O-heavy thread, it turns out
     * that it may be too risky to leave it in the Linux scheduler's
     * hands, so we'll complete the write in normal scheduling mode. */
    while ((result = (overlapped_offset >= 0) ?
                pwrite( unix_handle, buffer, bytesToWrite, overlapped_offset ) :
                write( unix_handle, buffer, bytesToWrite )) == -1)
    {
        if ((errno == EAGAIN) || (errno == EINTR)) continue;
        if (errno == EFAULT)
        {
           if (!IsBadReadPtr( buffer, bytesToWrite ))
              continue;
           SetLastError (ERROR_INVALID_USER_BUFFER);
           if (overlapped)
              overlapped->Internal = STATUS_INVALID_USER_BUFFER;
           break;
        }
        if (errno == ENOSPC)
            SetLastError( ERROR_DISK_FULL );
        else
        FILE_SetDosError();
        if (overlapped)
           overlapped->Internal = FILE_GetNtStatus ();
        break;
    }
    /* after the overlapped write is done, update the file pointer
       since pwrite doesn't update it but WriteFile is supposed to */
    if ((overlapped_offset >= 0) && (result >= 0))
    {
        if (lseek (unix_handle, overlapped_offset + result, SEEK_SET) 
             != overlapped_offset + result)
        {
            ERR ("Failed to set file offset to %Ld!\n", 
                 overlapped_offset + result);
        }
    }

    close( unix_handle );
    if (result == -1) return FALSE;
    if (bytesWritten) *bytesWritten = result;

    if (overlapped)
    {
       if (overlapped->hEvent)
          NtSetEvent (overlapped->hEvent, NULL);
       overlapped->Internal = STATUS_SUCCESS;
       overlapped->InternalHigh += result;
    }

    return TRUE;
}


/***********************************************************************
 *           _hread (KERNEL.349)
 */
LONG WINAPI WIN16_hread( HFILE16 hFile, SEGPTR buffer, LONG count )
{
    LONG maxlen;

    TRACE("%d %08lx %ld\n",
                  hFile, (DWORD)buffer, count );

    /* Some programs pass a count larger than the allocated buffer */
    maxlen = GetSelectorLimit16( SELECTOROF(buffer) ) - OFFSETOF(buffer) + 1;
    if (count > maxlen) count = maxlen;
    return _lread(DosFileHandleToWin32Handle(hFile), MapSL(buffer), count );
}


/***********************************************************************
 *           _lread (KERNEL.82)
 */
UINT16 WINAPI WIN16_lread( HFILE16 hFile, SEGPTR buffer, UINT16 count )
{
    return (UINT16)WIN16_hread( hFile, buffer, (LONG)count );
}


/***********************************************************************
 *           _lread   (KERNEL32.@)
 */
UINT WINAPI _lread( HFILE handle, LPVOID buffer, UINT count )
{
    DWORD result;
    if (!ReadFile( handle, buffer, count, &result, NULL )) return -1;
    return result;
}


/***********************************************************************
 *           _lread16   (KERNEL.82)
 */
UINT16 WINAPI _lread16( HFILE16 hFile, LPVOID buffer, UINT16 count )
{
    return (UINT16)_lread(DosFileHandleToWin32Handle(hFile), buffer, (LONG)count );
}


/***********************************************************************
 *           _lcreat   (KERNEL.83)
 */
HFILE16 WINAPI _lcreat16( LPCSTR path, INT16 attr )
{
    return Win32HandleToDosFileHandle( _lcreat( path, attr ) );
}


/***********************************************************************
 *           _lcreat   (KERNEL32.@)
 */
HFILE WINAPI _lcreat( LPCSTR path, INT attr )
{
    /* Mask off all flags not explicitly allowed by the doc */
    attr &= FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    TRACE("%s %02x\n", path, attr );
    return CreateFileA( path, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        CREATE_ALWAYS, attr, 0 );
}


/***********************************************************************
 *           SetFilePointer   (KERNEL32.@)
 */
DWORD WINAPI SetFilePointer( HANDLE hFile, LONG distance, LONG *highword,
                             DWORD method )
{
    LARGE_INTEGER newdistance, newpos;

    TRACE("handle %d offset %ld high %ld origin %ld\n",
          hFile, distance, highword?*highword:0, method );

    newdistance.u.LowPart  = distance;
    newdistance.u.HighPart = highword ? *highword : (distance >= 0) ? 0 : -1;

    if (!SetFilePointerEx( hFile, newdistance, &newpos, method ))
        return INVALID_SET_FILE_POINTER;

    if (highword) *highword = newpos.u.HighPart;

    return newpos.u.LowPart;
}

/***********************************************************************
 *           SetFilePointerEx   (KERNEL32.@)
 */
BOOL WINAPI SetFilePointerEx( HANDLE hFile, LARGE_INTEGER liDistanceToMove,
                              PLARGE_INTEGER lpNewFilePointer,
                              DWORD dwMoveMethod)
{
    TRACE("handle %d distance(%ld %ld) newpointer %p origin %ld\n",
          hFile, liDistanceToMove.u.LowPart, liDistanceToMove.u.HighPart, lpNewFilePointer, dwMoveMethod );

    if (dwMoveMethod < FILE_BEGIN || dwMoveMethod > FILE_END) {
        ERR("Invalid dwMoveMethod\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (MEMFILE_SetFilePointerEx (hFile, liDistanceToMove, lpNewFilePointer,
                                  dwMoveMethod))
       return TRUE;


    /* NOTE: despite what MSDN claims, this function still works on directory handles.
             The file pointer will move and its current position is tracked between
             repeated calls.  It will not fail on a directory handle and still reports
             the correct position as its result.
             Currently we do not perform the actual seek operation in the wine server
             call, but just simulate it for the return value assuming a file size of
             0 and a current position of 0.  It's unlikely any app will depend on this
             functionality working so it's not worth the effort to simulate it properly.
    */
    SERVER_START_REQ( set_file_pointer )
    {
        req->handle = hFile;
        req->low = liDistanceToMove.u.LowPart;
        req->high = liDistanceToMove.u.HighPart;

        /* don't assume a 1:1 match between windows and unix seek method constants (even though they do match) */
        switch (dwMoveMethod){
            default:
            case FILE_BEGIN:    dwMoveMethod = SEEK_SET;    break;
            case FILE_CURRENT:  dwMoveMethod = SEEK_CUR;    break;
            case FILE_END:      dwMoveMethod = SEEK_END;    break;
        }

        req->whence = dwMoveMethod;
        SetLastError( 0 );
        if (!wine_server_call_err( req ))
        {
            if (lpNewFilePointer) {
                lpNewFilePointer->u.LowPart = reply->new_low;
                lpNewFilePointer->u.HighPart = reply->new_high;
            }
        }
    }
    SERVER_END_REQ;

    if (GetLastError() != NO_ERROR) {
        return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *           _llseek   (KERNEL.84)
 *
 * FIXME:
 *   Seeking before the start of the file should be allowed for _llseek16,
 *   but cause subsequent I/O operations to fail (cf. interrupt list)
 *
 */
LONG WINAPI _llseek16( HFILE16 hFile, LONG lOffset, INT16 nOrigin )
{
    return SetFilePointer( DosFileHandleToWin32Handle(hFile), lOffset, NULL, nOrigin );
}


/***********************************************************************
 *           _llseek   (KERNEL32.@)
 */
LONG WINAPI _llseek( HFILE hFile, LONG lOffset, INT nOrigin )
{
    return SetFilePointer( hFile, lOffset, NULL, nOrigin );
}


/***********************************************************************
 *           _lopen   (KERNEL.85)
 */
HFILE16 WINAPI _lopen16( LPCSTR path, INT16 mode )
{
    return Win32HandleToDosFileHandle( _lopen( path, mode ) );
}


/***********************************************************************
 *           _lopen   (KERNEL32.@)
 */
HFILE WINAPI _lopen( LPCSTR path, INT mode )
{
    DWORD access, sharing;

    TRACE("('%s',%04x)\n", path, mode );
    FILE_ConvertOFMode( mode, &access, &sharing );
    return CreateFileA( path, access, sharing, NULL, OPEN_EXISTING, 0, 0 );
}


/***********************************************************************
 *           _lwrite   (KERNEL.86)
 */
UINT16 WINAPI _lwrite16( HFILE16 hFile, LPCSTR buffer, UINT16 count )
{
    return (UINT16)_hwrite( DosFileHandleToWin32Handle(hFile), buffer, (LONG)count );
}

/***********************************************************************
 *           _lwrite   (KERNEL32.@)
 */
UINT WINAPI _lwrite( HFILE hFile, LPCSTR buffer, UINT count )
{
    return (UINT)_hwrite( hFile, buffer, (LONG)count );
}


/***********************************************************************
 *           _hread16   (KERNEL.349)
 */
LONG WINAPI _hread16( HFILE16 hFile, LPVOID buffer, LONG count)
{
    return _lread( DosFileHandleToWin32Handle(hFile), buffer, count );
}


/***********************************************************************
 *           _hread   (KERNEL32.@)
 */
LONG WINAPI _hread( HFILE hFile, LPVOID buffer, LONG count)
{
    return _lread( hFile, buffer, count );
}


/***********************************************************************
 *           _hwrite   (KERNEL.350)
 */
LONG WINAPI _hwrite16( HFILE16 hFile, LPCSTR buffer, LONG count )
{
    return _hwrite( DosFileHandleToWin32Handle(hFile), buffer, count );
}


/***********************************************************************
 *           _hwrite   (KERNEL32.@)
 *
 *	experimentation yields that _lwrite:
 *		o truncates the file at the current position with
 *		  a 0 len write
 *		o returns 0 on a 0 length write
 *		o works with console handles
 *
 */
LONG WINAPI _hwrite( HFILE handle, LPCSTR buffer, LONG count )
{
    DWORD result;

    TRACE("%d %p %ld\n", handle, buffer, count );

    if (!count)
    {
        /* Expand or truncate at current position */
        if (!SetEndOfFile( handle )) return HFILE_ERROR;
        return 0;
    }
    if (!WriteFile( handle, buffer, count, &result, NULL ))
        return HFILE_ERROR;
    return result;
}


/***********************************************************************
 *           SetHandleCount   (KERNEL.199)
 */
UINT16 WINAPI SetHandleCount16( UINT16 count )
{
    return SetHandleCount( count );
}


/*************************************************************************
 *           SetHandleCount   (KERNEL32.@)
 */
UINT WINAPI SetHandleCount( UINT count )
{
    return min( 256, count );
}


/***********************************************************************
 *           FlushFileBuffers   (KERNEL32.@)
 */
BOOL WINAPI FlushFileBuffers( HANDLE hFile )
{
    BOOL ret;
    SERVER_START_REQ( flush_file )
    {
        req->handle = hFile;
        ret = !wine_server_call_err( req );

        /* this server call will fail if <hFile> is a directory handle */
        if (!reply->result){
            SetLastError(ERROR_ACCESS_DENIED);
            ret = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/**************************************************************************
 *           SetEndOfFile   (KERNEL32.@)
 */
BOOL WINAPI SetEndOfFile( HANDLE hFile )
{
    BOOL ret;
    SERVER_START_REQ( truncate_file )
    {
        req->handle = hFile;
        ret = !wine_server_call_err( req );

        /* this server call will fail if <hFile> is a directory handle */
        if (!reply->result){
            SetLastError(ERROR_ACCESS_DENIED);
            ret = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           DeleteFile   (KERNEL.146)
 */
BOOL16 WINAPI DeleteFile16( LPCSTR path )
{
    return DeleteFileA( path );
}


/***********************************************************************
 *           DeleteFileA   (KERNEL32.@)
 */
BOOL WINAPI DeleteFileA( LPCSTR path )
{
    DOS_FULL_NAME full_name;

    if (!path)
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    TRACE("'%s'\n", path );

    if (!*path)
    {
        SetLastError (ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    if (DOSFS_GetDevice( path ))
    {
        WARN("cannot remove DOS device '%s'!\n", path);
        SetLastError( ERROR_FILE_NOT_FOUND );
        return FALSE;
    }

    if (!DOSFS_GetFullName( path, TRUE, &full_name )) return FALSE;

    if (GetVersion() & 0x80000000) {
        /* On win9x, the file can be deleted even if it's in use, with
         * potentially disastrous effects the docs gives dire warnings about. */
        if (unlink( full_name.long_name ) == -1)
        {
            FILE_SetDosError();
            return FALSE;
        }
    } else {
        HANDLE hFile;
        /* On NT, if the file is open for normal I/O, the delete fails,
         * otherwise it is deleted after the last handle to it is closed.
         * This should give a fair approximation of this behaviour. */
        hFile = FILE_CreateFile( full_name.long_name, GENERIC_READ | GENERIC_WRITE | DELETE,
                                 FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0,
                                 DRIVE_GetType(full_name.drive), full_name.drive );
        if (!hFile) return FALSE;
        CloseHandle(hFile);
    }

    return TRUE;
}


/***********************************************************************
 *           DeleteFileW   (KERNEL32.@)
 */
BOOL WINAPI DeleteFileW( LPCWSTR path )
{
    LPSTR xpath = FILE_strdupWtoA( GetProcessHeap(), 0, path );
    BOOL ret = DeleteFileA( xpath );
    HeapFree( GetProcessHeap(), 0, xpath );
    return ret;
}


/***********************************************************************
 *           GetFileType   (KERNEL32.@)
 */
DWORD WINAPI GetFileType( HANDLE hFile )
{
    DWORD ret = FILE_TYPE_UNKNOWN;
    SERVER_START_REQ( get_file_info )
    {
        req->handle = hFile;
        if (!wine_server_call_err( req )) ret = reply->type;
    }
    SERVER_END_REQ;
    return ret;
}


/* check if a file name is for an executable file (.exe or .com) */
inline static BOOL is_executable( const char *name )
{
    int len = strlen(name);

    if (len < 4) return FALSE;
    return (!strcasecmp( name + len - 4, ".exe" ) ||
            !strcasecmp( name + len - 4, ".com" ));
}


/***********************************************************************
 *           FILE_AddBootRenameEntry
 *
 * Adds an entry to the registry that is loaded when windows boots and
 * checks if there are some files to be removed or renamed/moved.
 * <fn1> has to be valid and <fn2> may be NULL. If both pointers are
 * non-NULL then the file is moved, otherwise it is deleted.  The
 * entry of the registrykey is always appended with two zero
 * terminated strings. If <fn2> is NULL then the second entry is
 * simply a single 0-byte. Otherwise the second filename goes
 * there. The entries are prepended with \??\ before the path and the
 * second filename gets also a '!' as the first character if
 * MOVEFILE_REPLACE_EXISTING is set. After the final string another
 * 0-byte follows to indicate the end of the strings.
 * i.e.:
 * \??\D:\test\file1[0]
 * !\??\D:\test\file1_renamed[0]
 * \??\D:\Test|delete[0]
 * [0]                        <- file is to be deleted, second string empty
 * \??\D:\test\file2[0]
 * !\??\D:\test\file2_renamed[0]
 * [0]                        <- indicates end of strings
 *
 * or:
 * \??\D:\test\file1[0]
 * !\??\D:\test\file1_renamed[0]
 * \??\D:\Test|delete[0]
 * [0]                        <- file is to be deleted, second string empty
 * [0]                        <- indicates end of strings
 *
 */
static BOOL FILE_AddBootRenameEntry( const char *fn1, const char *fn2, DWORD flags )
{
    static const char PreString[] = "\\??\\";
    static const char ValueName[] = "PendingFileRenameOperations";

    BOOL rc = FALSE;
    HKEY Reboot = 0;
    DWORD Type, len1, len2, l;
    DWORD DataSize = 0;
    BYTE *Buffer = NULL;

    if(RegCreateKeyA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                     &Reboot) != ERROR_SUCCESS)
    {
        WARN("Error creating key for reboot managment [%s]\n",
             "SYSTEM\\CurrentControlSet\\Control\\Session Manager");
        return FALSE;
    }

    l = strlen(PreString);
    len1 = strlen(fn1) + l + 1;
    if (fn2)
    {
        len2 = strlen(fn2) + l + 1;
        if (flags & MOVEFILE_REPLACE_EXISTING) len2++; /* Plus 1 because of the leading '!' */
    }
    else len2 = 1; /* minimum is the 0 byte for the empty second string */

    /* First we check if the key exists and if so how many bytes it already contains. */
    if (RegQueryValueExA(Reboot, ValueName, NULL, &Type, NULL, &DataSize) == ERROR_SUCCESS)
    {
        if (Type != REG_MULTI_SZ) goto Quit;
        if (!(Buffer = HeapAlloc( GetProcessHeap(), 0, DataSize + len1 + len2 + 1 ))) goto Quit;
        if (RegQueryValueExA(Reboot, ValueName, NULL, &Type, Buffer, &DataSize) != ERROR_SUCCESS)
            goto Quit;
        if (DataSize) DataSize--;  /* remove terminating null (will be added back later) */
    }
    else
    {
        if (!(Buffer = HeapAlloc( GetProcessHeap(), 0, len1 + len2 + 1 ))) goto Quit;
        DataSize = 0;
    }
    sprintf( (char *)Buffer + DataSize, "%s%s", PreString, fn1 );
    DataSize += len1;
    if (fn2)
    {
        sprintf( (char *)Buffer + DataSize, "%s%s%s",
                 (flags & MOVEFILE_REPLACE_EXISTING) ? "!" : "", PreString, fn2 );
        DataSize += len2;
    }
    else Buffer[DataSize++] = 0;

    Buffer[DataSize++] = 0;  /* add final null */
    rc = !RegSetValueExA( Reboot, ValueName, 0, REG_MULTI_SZ, Buffer, DataSize );

 Quit:
    if (Reboot) RegCloseKey(Reboot);
    if (Buffer) HeapFree( GetProcessHeap(), 0, Buffer );
    return(rc);
}


/**************************************************************************
 *           MoveFileExA   (KERNEL32.@)
 */
BOOL WINAPI MoveFileExA( LPCSTR fn1, LPCSTR fn2, DWORD flag )
{
    DOS_FULL_NAME full_name1, full_name2;
    HANDLE hSrcFile = INVALID_HANDLE_VALUE, hDestFile;
    BOOL Ret = FALSE;
    DWORD Attrs;

    TRACE("(%s,%s,%04lx)\n", fn1, fn2, flag);

    /* FIXME: <Gerhard W. Gruber>sparhawk@gmx.at
       In case of W9x and lesser this function should return 120 (ERROR_CALL_NOT_IMPLEMENTED)
       to be really compatible. Most programs wont have any problems though. In case
       you encounter one, this is what you should return here. I don't know what's up
       with NT 3.5. Is this function available there or not?
       Does anybody really care about 3.5? :)
    */

    /* Filename1 has to be always set to a valid path. Filename2 may be NULL
       if the source file has to be deleted.
    */
    if (!fn1) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* This function has to be run through in order to process the name properly.
       If the BOOTDELAY flag is set, the file doesn't need to exist though. At least
       that is the behaviour on NT 4.0. The operation accepts the filenames as
       they are given but it can't reply with a reasonable returncode. Success
       means in that case success for entering the values into the registry.
    */
    if(!DOSFS_GetFullName( fn1, TRUE, &full_name1 ))
    {
        if(!(flag & MOVEFILE_DELAY_UNTIL_REBOOT))
            return FALSE;
    }

    /* Test that we have appropriate perms for fn1, and keep it from
       disappearing from under us */
    hSrcFile = CreateFileA (fn1, GENERIC_READ, 0, NULL,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (hSrcFile == INVALID_HANDLE_VALUE)
       return FALSE;

    Attrs = GetFileAttributesA (fn1);
    if (Attrs == -1)
       goto done;

    if (fn2)  /* !fn2 means delete fn1 */
    {
        hDestFile = CreateFileA (fn2, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                 OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        if (hDestFile != INVALID_HANDLE_VALUE)
        {
           CloseHandle (hDestFile);

            if(!(flag & MOVEFILE_DELAY_UNTIL_REBOOT))
            {
                /* target exists, check if we may overwrite */
                if (!(flag & MOVEFILE_REPLACE_EXISTING))
                {
                    SetLastError( ERROR_ALREADY_EXISTS );
                    goto done;
                }
            }

            /* Can't overwrite something with a directory */
            if (Attrs & FILE_ATTRIBUTE_DIRECTORY)
            {
               SetLastError (ERROR_ACCESS_DENIED);
               goto done;
            }
        }
        else if (GetLastError () != ERROR_FILE_NOT_FOUND)
           goto done;

        if (!DOSFS_GetFullName( fn2, FALSE, &full_name2 ))
        {
           if(!(flag & MOVEFILE_DELAY_UNTIL_REBOOT))
              goto done;
        }

        /* Source name and target path are valid */

        if (flag & MOVEFILE_DELAY_UNTIL_REBOOT)
        {
            /* FIXME: (bon@elektron.ikp.physik.th-darmstadt.de 970706)
               Perhaps we should queue these command and execute it
               when exiting... What about using on_exit(2)
            */
            FIXME("Please move existing file '%s' to file '%s' when Wine has finished\n",
                  fn1, fn2);
            Ret = FILE_AddBootRenameEntry( fn1, fn2, flag );
            goto done;
        }

        if (full_name1.drive != full_name2.drive)
        {
            /* use copy, if allowed */
            if (!(flag & MOVEFILE_COPY_ALLOWED))
            {
                /* FIXME: Use right error code */
                SetLastError( ERROR_FILE_EXISTS );
                goto done;
            }
            if (!CopyFileA( fn1, fn2, !(flag & MOVEFILE_REPLACE_EXISTING) ))
                goto done;

            CloseHandle (hSrcFile);
            return DeleteFileA(fn1);
        }

        if (rename( full_name1.long_name, full_name2.long_name ) == -1)
	{
            FILE_SetDosError();
            goto done;
	}
        if (is_executable( full_name1.long_name ) != is_executable( full_name2.long_name ))
        {
            struct stat fstat;
            if (stat( full_name2.long_name, &fstat ) != -1)
            {
                if (is_executable( full_name2.long_name ))
                    /* set executable bit where read bit is set */
                    fstat.st_mode |= (fstat.st_mode & 0444) >> 2;
                else
                    fstat.st_mode &= ~0111;
                chmod( full_name2.long_name, fstat.st_mode );
            }
        }
        Ret = TRUE;
    }
    else /* fn2 == NULL means delete source */
    {
        CloseHandle (hSrcFile);

        if (flag & MOVEFILE_DELAY_UNTIL_REBOOT)
        {
            if (flag & MOVEFILE_COPY_ALLOWED) {
                WARN("Illegal flag\n");
                SetLastError( ERROR_GEN_FAILURE );
                return FALSE;
            }
            /* FIXME: (bon@elektron.ikp.physik.th-darmstadt.de 970706)
               Perhaps we should queue these command and execute it
               when exiting... What about using on_exit(2)
            */
            FIXME("Please delete file '%s' when Wine has finished\n", fn1);
            return FILE_AddBootRenameEntry( fn1, NULL, flag );
        }

        return DeleteFileA (fn1);
    }

 done:
    CloseHandle (hSrcFile);
    return Ret;
}

/**************************************************************************
 *           MoveFileExW   (KERNEL32.@)
 */
BOOL WINAPI MoveFileExW( LPCWSTR fn1, LPCWSTR fn2, DWORD flag )
{
    LPSTR afn1 = FILE_strdupWtoA( GetProcessHeap(), 0, fn1 );
    LPSTR afn2 = FILE_strdupWtoA( GetProcessHeap(), 0, fn2 );
    BOOL res = MoveFileExA( afn1, afn2, flag );
    HeapFree( GetProcessHeap(), 0, afn1 );
    HeapFree( GetProcessHeap(), 0, afn2 );
    return res;
}


/**************************************************************************
 *           MoveFileA   (KERNEL32.@)
 *
 *  Move file or directory
 */
BOOL WINAPI MoveFileA( LPCSTR fn1, LPCSTR fn2 )
{
    DOS_FULL_NAME full_name1, full_name2;
    struct stat fstat;

    TRACE("(%s,%s)\n", fn1, fn2 );

    if (!DOSFS_GetFullName( fn1, TRUE, &full_name1 )) return FALSE;
    if (DOSFS_GetFullName( fn2, TRUE, &full_name2 ))  {
      if (!strcmp(full_name1.long_name, full_name2.long_name)) {
        /* If the new name is identical to old, this is probably some
         * program's funky way of checking for a sharing violation. */
        HANDLE hFile;
        hFile = FILE_CreateFile( full_name1.long_name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_DELETE,
                                 NULL, OPEN_EXISTING, 0, 0,
                                 DRIVE_GetType(full_name1.drive), full_name1.drive );
        if (!hFile) return FALSE;
        CloseHandle(hFile);
        TRACE("doing nothing\n");
        return TRUE;
      }
      /* The new name must not already exist */
      TRACE("destination exists\n");
      SetLastError(ERROR_ALREADY_EXISTS);
      return FALSE;
    }
    if (!DOSFS_GetFullName( fn2, FALSE, &full_name2 )) return FALSE;

    if (full_name1.drive == full_name2.drive) /* move */
        return MoveFileExA( fn1, fn2, MOVEFILE_COPY_ALLOWED );

    /* copy */
    if (stat( full_name1.long_name, &fstat ))
    {
        WARN("Invalid source file %s\n",
             full_name1.long_name);
        FILE_SetDosError();
        return FALSE;
    }
    if (S_ISDIR(fstat.st_mode)) {
        /* No Move for directories across file systems */
        /* FIXME: Use right error code */
        SetLastError( ERROR_GEN_FAILURE );
        return FALSE;
    }
    if (!CopyFileA(fn1, fn2, TRUE)) /*fail, if exist */
        return FALSE;
    return DeleteFileA(fn1);
}


/**************************************************************************
 *           MoveFileW   (KERNEL32.@)
 */
BOOL WINAPI MoveFileW( LPCWSTR fn1, LPCWSTR fn2 )
{
    LPSTR afn1 = FILE_strdupWtoA( GetProcessHeap(), 0, fn1 );
    LPSTR afn2 = FILE_strdupWtoA( GetProcessHeap(), 0, fn2 );
    BOOL res = MoveFileA( afn1, afn2 );
    HeapFree( GetProcessHeap(), 0, afn1 );
    HeapFree( GetProcessHeap(), 0, afn2 );
    return res;
}


/**************************************************************************
 *           CopyFileA   (KERNEL32.@)
 */
BOOL WINAPI CopyFileA( LPCSTR source, LPCSTR dest, BOOL fail_if_exists )
{
    HFILE hSrc, hDest;
    BY_HANDLE_FILE_INFORMATION info;
    int count;
    BOOL ret = FALSE;
    int mode;
    char buffer[2048];

    if ((hSrc = _lopen( source, OF_READ )) == HFILE_ERROR) return FALSE;
    if (!GetFileInformationByHandle( hSrc, &info ))
    {
        CloseHandle( hSrc );
        return FALSE;
    }
    mode = (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;
    if ((hDest = CreateFileA( dest, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             fail_if_exists ? CREATE_NEW : CREATE_ALWAYS,
                             info.dwFileAttributes, hSrc )) == INVALID_HANDLE_VALUE)
    {
        CloseHandle( hSrc );
        return FALSE;
    }
    while ((count = _lread( hSrc, buffer, sizeof(buffer) )) > 0)
    {
        char *p = buffer;
        while (count > 0)
        {
            INT res = _lwrite( hDest, p, count );
            if (res <= 0) goto done;
            p += res;
            count -= res;
        }
    }

    if (count == -1)
    {
       ret = FALSE;
    }
    else
    {
       ret = TRUE;
    }

done:
    /* Preserve file time */
    SetFileTime (hDest, NULL, NULL, &info.ftLastWriteTime);

    CloseHandle( hSrc );
    CloseHandle( hDest );
    return ret;
}




/**************************************************************************
 *           CopyFileW   (KERNEL32.@)
 */
BOOL WINAPI CopyFileW( LPCWSTR source, LPCWSTR dest, BOOL fail_if_exists)
{
    LPSTR sourceA = FILE_strdupWtoA( GetProcessHeap(), 0, source );
    LPSTR destA   = FILE_strdupWtoA( GetProcessHeap(), 0, dest );
    BOOL ret = CopyFileA( sourceA, destA, fail_if_exists );
    HeapFree( GetProcessHeap(), 0, sourceA );
    HeapFree( GetProcessHeap(), 0, destA );
    return ret;
}


/**************************************************************************
 *           CopyFileExA   (KERNEL32.@)
 *
 * This implementation ignores most of the extra parameters passed-in into
 * the "ex" version of the method and calls the CopyFile method.
 * It will have to be fixed eventually.
 */
BOOL WINAPI CopyFileExA(LPCSTR             sourceFilename,
                           LPCSTR             destFilename,
                           LPPROGRESS_ROUTINE progressRoutine,
                           LPVOID             appData,
                           LPBOOL           cancelFlagPointer,
                           DWORD              copyFlags)
{
  BOOL failIfExists = FALSE;

  /*
   * Interpret the only flag that CopyFile can interpret.
   */
  if ( (copyFlags & COPY_FILE_FAIL_IF_EXISTS) != 0)
  {
    failIfExists = TRUE;
  }

  return CopyFileA(sourceFilename, destFilename, failIfExists);
}

/**************************************************************************
 *           CopyFileExW   (KERNEL32.@)
 */
BOOL WINAPI CopyFileExW(LPCWSTR            sourceFilename,
                           LPCWSTR            destFilename,
                           LPPROGRESS_ROUTINE progressRoutine,
                           LPVOID             appData,
                           LPBOOL           cancelFlagPointer,
                           DWORD              copyFlags)
{
    LPSTR sourceA = FILE_strdupWtoA( GetProcessHeap(), 0, sourceFilename );
    LPSTR destA   = FILE_strdupWtoA( GetProcessHeap(), 0, destFilename );

    BOOL ret = CopyFileExA(sourceA,
                              destA,
                              progressRoutine,
                              appData,
                              cancelFlagPointer,
                              copyFlags);

    HeapFree( GetProcessHeap(), 0, sourceA );
    HeapFree( GetProcessHeap(), 0, destA );

    return ret;
}


/***********************************************************************
 *              SetFileTime   (KERNEL32.@)
 */
BOOL WINAPI SetFileTime( HANDLE hFile,
                           const FILETIME *lpCreationTime,
                           const FILETIME *lpLastAccessTime,
                           const FILETIME *lpLastWriteTime )
{
    BOOL ret;
    SERVER_START_REQ( set_file_time )
    {
        req->handle = hFile;
        if (lpLastAccessTime)
            RtlTimeToSecondsSince1970( (PLARGE_INTEGER) lpLastAccessTime, (DWORD *)&req->access_time );
        else
            req->access_time = 0; /* FIXME */
        if (lpLastWriteTime)
            RtlTimeToSecondsSince1970( (PLARGE_INTEGER) lpLastWriteTime, (DWORD *)&req->write_time );
        else
            req->write_time = 0; /* FIXME */
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**************************************************************************
 *           LockFile   (KERNEL32.@)
 */
BOOL WINAPI LockFile( HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                        DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh )
{
    BOOL ret;
    SERVER_START_REQ( lock_file )
    {
        req->handle      = hFile;
        req->offset_low  = dwFileOffsetLow;
        req->offset_high = dwFileOffsetHigh;
        req->count_low   = nNumberOfBytesToLockLow;
        req->count_high  = nNumberOfBytesToLockHigh;
        ret = !wine_server_call_err( req );

        /* this server call will fail if <hFile> is a directory handle */
        if (!reply->result){
            SetLastError(ERROR_INVALID_PARAMETER);
            ret = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}

/**************************************************************************
 * LockFileEx [KERNEL32.@]
 *
 * Locks a byte range within an open file for shared or exclusive access.
 *
 * RETURNS
 *   success: TRUE
 *   failure: FALSE
 *
 * NOTES
 * Per Microsoft docs, the third parameter (reserved) must be set to 0.
 */
BOOL WINAPI LockFileEx( HANDLE hFile, DWORD flags, DWORD reserved,
		      DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh,
		      LPOVERLAPPED pOverlapped )
{
   WARN ("hFile=%d,flags=0x%x,reserved=%ld,lowbytes=%ld,highbytes=%ld,overlapped=%p: stub\n",
	  hFile, flags, reserved, nNumberOfBytesToLockLow,
          nNumberOfBytesToLockHigh, pOverlapped);
   if (reserved != 0)
   {
      ERR("reserved == %ld: Supposed to be 0??\n", reserved);
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
   }

   /* HACK - lie that we succeed */
   if (pOverlapped && (pOverlapped->hEvent))
      NtSetEvent (pOverlapped->hEvent, NULL);

   return TRUE;
}


/**************************************************************************
 *           UnlockFile   (KERNEL32.@)
 */
BOOL WINAPI UnlockFile( HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                          DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh )
{
    BOOL ret;
    SERVER_START_REQ( unlock_file )
    {
        req->handle      = hFile;
        req->offset_low  = dwFileOffsetLow;
        req->offset_high = dwFileOffsetHigh;
        req->count_low   = nNumberOfBytesToUnlockLow;
        req->count_high  = nNumberOfBytesToUnlockHigh;
        ret = !wine_server_call_err( req );

        /* this server call will fail if <hFile> is a directory handle */
        if (!reply->result){
            SetLastError(ERROR_INVALID_PARAMETER);
            ret = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/**************************************************************************
 *           UnlockFileEx   (KERNEL32.@)
 */
BOOL WINAPI UnlockFileEx(
		HFILE hFile,
		DWORD dwReserved,
		DWORD nNumberOfBytesToUnlockLow,
		DWORD nNumberOfBytesToUnlockHigh,
		LPOVERLAPPED lpOverlapped
)
{
   WARN("hFile=%d,reserved=%ld,lowbytes=%ld,highbytes=%ld,overlapped=%p: stub.\n",
         hFile, dwReserved, nNumberOfBytesToUnlockLow,
         nNumberOfBytesToUnlockHigh, lpOverlapped);
   if (dwReserved != 0)
   {
      ERR("reserved == %ld: Supposed to be 0??\n", dwReserved);
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
   }

   /* HACK - pretend we always succeed */
   return TRUE;
}


#if 0

struct DOS_FILE_LOCK {
  struct DOS_FILE_LOCK *	next;
  DWORD				base;
  DWORD				len;
  DWORD				processId;
  FILE_OBJECT *			dos_file;
/*  char *			unix_name;*/
};

typedef struct DOS_FILE_LOCK DOS_FILE_LOCK;

static DOS_FILE_LOCK *locks = NULL;
static void DOS_RemoveFileLocks(FILE_OBJECT *file);


/* Locks need to be mirrored because unix file locking is based
 * on the pid. Inside of wine there can be multiple WINE processes
 * that share the same unix pid.
 * Read's and writes should check these locks also - not sure
 * how critical that is at this point (FIXME).
 */

static BOOL DOS_AddLock(FILE_OBJECT *file, struct flock *f)
{
  DOS_FILE_LOCK *curr;
  DWORD		processId;

  processId = GetCurrentProcessId();

  /* check if lock overlaps a current lock for the same file */
#if 0
  for (curr = locks; curr; curr = curr->next) {
    if (strcmp(curr->unix_name, file->unix_name) == 0) {
      if ((f->l_start == curr->base) && (f->l_len == curr->len))
	return TRUE;/* region is identic */
      if ((f->l_start < (curr->base + curr->len)) &&
	  ((f->l_start + f->l_len) > curr->base)) {
	/* region overlaps */
	return FALSE;
      }
    }
  }
#endif

  curr = HeapAlloc( GetProcessHeap(), 0, sizeof(DOS_FILE_LOCK) );
  curr->processId = GetCurrentProcessId();
  curr->base = f->l_start;
  curr->len = f->l_len;
/*  curr->unix_name = HEAP_strdupA( GetProcessHeap(), 0, file->unix_name);*/
  curr->next = locks;
  curr->dos_file = file;
  locks = curr;
  return TRUE;
}

static void DOS_RemoveFileLocks(FILE_OBJECT *file)
{
  DWORD		processId;
  DOS_FILE_LOCK **curr;
  DOS_FILE_LOCK *rem;

  processId = GetCurrentProcessId();
  curr = &locks;
  while (*curr) {
    if ((*curr)->dos_file == file) {
      rem = *curr;
      *curr = (*curr)->next;
/*      HeapFree( GetProcessHeap(), 0, rem->unix_name );*/
      HeapFree( GetProcessHeap(), 0, rem );
    }
    else
      curr = &(*curr)->next;
  }
}

static BOOL DOS_RemoveLock(FILE_OBJECT *file, struct flock *f)
{
  DWORD		processId;
  DOS_FILE_LOCK **curr;
  DOS_FILE_LOCK *rem;

  processId = GetCurrentProcessId();
  for (curr = &locks; *curr; curr = &(*curr)->next) {
    if ((*curr)->processId == processId &&
	(*curr)->dos_file == file &&
	(*curr)->base == f->l_start &&
	(*curr)->len == f->l_len) {
      /* this is the same lock */
      rem = *curr;
      *curr = (*curr)->next;
/*      HeapFree( GetProcessHeap(), 0, rem->unix_name );*/
      HeapFree( GetProcessHeap(), 0, rem );
      return TRUE;
    }
  }
  /* no matching lock found */
  return FALSE;
}


/**************************************************************************
 *           LockFile   (KERNEL32.@)
 */
BOOL WINAPI LockFile(
	HFILE hFile,DWORD dwFileOffsetLow,DWORD dwFileOffsetHigh,
	DWORD nNumberOfBytesToLockLow,DWORD nNumberOfBytesToLockHigh )
{
  struct flock f;
  FILE_OBJECT *file;

  TRACE("handle %d offsetlow=%ld offsethigh=%ld nbyteslow=%ld nbyteshigh=%ld\n",
	       hFile, dwFileOffsetLow, dwFileOffsetHigh,
	       nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);

  if (dwFileOffsetHigh || nNumberOfBytesToLockHigh) {
    FIXME("Unimplemented bytes > 32bits\n");
    return FALSE;
  }

  f.l_start = dwFileOffsetLow;
  f.l_len = nNumberOfBytesToLockLow;
  f.l_whence = SEEK_SET;
  f.l_pid = 0;
  f.l_type = F_WRLCK;

  if (!(file = FILE_GetFile(hFile,0,NULL))) return FALSE;

  /* shadow locks internally */
  if (!DOS_AddLock(file, &f)) {
    SetLastError( ERROR_LOCK_VIOLATION );
    return FALSE;
  }

  /* FIXME: Unix locking commented out for now, doesn't work with Excel */
#ifdef USE_UNIX_LOCKS
  if (fcntl(file->unix_handle, F_SETLK, &f) == -1) {
    if (errno == EACCES || errno == EAGAIN) {
      SetLastError( ERROR_LOCK_VIOLATION );
    }
    else {
      FILE_SetDosError();
    }
    /* remove our internal copy of the lock */
    DOS_RemoveLock(file, &f);
    return FALSE;
  }
#endif
  return TRUE;
}


/**************************************************************************
 *           UnlockFile   (KERNEL32.@)
 */
BOOL WINAPI UnlockFile(
	HFILE hFile,DWORD dwFileOffsetLow,DWORD dwFileOffsetHigh,
	DWORD nNumberOfBytesToUnlockLow,DWORD nNumberOfBytesToUnlockHigh )
{
  FILE_OBJECT *file;
  struct flock f;

  TRACE("handle %d offsetlow=%ld offsethigh=%ld nbyteslow=%ld nbyteshigh=%ld\n",
	       hFile, dwFileOffsetLow, dwFileOffsetHigh,
	       nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh);

  if (dwFileOffsetHigh || nNumberOfBytesToUnlockHigh) {
    WARN("Unimplemented bytes > 32bits\n");
    return FALSE;
  }

  f.l_start = dwFileOffsetLow;
  f.l_len = nNumberOfBytesToUnlockLow;
  f.l_whence = SEEK_SET;
  f.l_pid = 0;
  f.l_type = F_UNLCK;

  if (!(file = FILE_GetFile(hFile,0,NULL))) return FALSE;

  DOS_RemoveLock(file, &f);	/* ok if fails - may be another wine */

  /* FIXME: Unix locking commented out for now, doesn't work with Excel */
#ifdef USE_UNIX_LOCKS
  if (fcntl(file->unix_handle, F_SETLK, &f) == -1) {
    FILE_SetDosError();
    return FALSE;
  }
#endif
  return TRUE;
}
#endif

/**************************************************************************
 * GetFileAttributesExA [KERNEL32.@]
 */
BOOL WINAPI GetFileAttributesExA(
	LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
	LPVOID lpFileInformation)
{
    DOS_FULL_NAME full_name;
    BY_HANDLE_FILE_INFORMATION info;

    if (lpFileName == NULL) return FALSE;
    if (lpFileInformation == NULL) return FALSE;

    if (fInfoLevelId == GetFileExInfoStandard) {
	LPWIN32_FILE_ATTRIBUTE_DATA lpFad =
	    (LPWIN32_FILE_ATTRIBUTE_DATA) lpFileInformation;
	if (!DOSFS_GetFullName( lpFileName, TRUE, &full_name )) return FALSE;
	if (!FILE_Stat( full_name.long_name, &info )) return FALSE;

	lpFad->dwFileAttributes = info.dwFileAttributes;
	lpFad->ftCreationTime   = info.ftCreationTime;
	lpFad->ftLastAccessTime = info.ftLastAccessTime;
	lpFad->ftLastWriteTime  = info.ftLastWriteTime;
	lpFad->nFileSizeHigh    = info.nFileSizeHigh;
	lpFad->nFileSizeLow     = info.nFileSizeLow;
    }
    else {
	FIXME("invalid info level %d!\n", fInfoLevelId);
	return FALSE;
    }

    return TRUE;
}


/**************************************************************************
 * GetFileAttributesExW [KERNEL32.@]
 */
BOOL WINAPI GetFileAttributesExW(
	LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
	LPVOID lpFileInformation)
{
    LPSTR nameA = FILE_strdupWtoA( GetProcessHeap(), 0, lpFileName );
    BOOL res =
	GetFileAttributesExA( nameA, fInfoLevelId, lpFileInformation);
    HeapFree( GetProcessHeap(), 0, nameA );
    return res;
}


/**************************************************************************
 *          FILE_DoForceEjectCDDrive 
 */
static BOOL FILE_DoForceEjectCDDrive(int drive)
{
    BOOL ret;
    int *fd_list;
    int count = 0;
    int i = 0;

    TRACE("entering\n");

    SERVER_START_REQ( get_cdrom_eject_fd_count)
    {
        req->cd_drive = drive;
        ret = !wine_server_call_err( req );
        if (ret)
            count = reply->count;
    }
    SERVER_END_REQ;

    if (count == 0 || ret == 0)
    {
        TRACE("leaving: no files to close\n");
	return FALSE;
    }

    /* Add some buffer in case a bunch of new files somehow get opened between 
     * these two server calls */
    count += 256;

    fd_list = (int*) HeapAlloc(GetProcessHeap(), 0, sizeof(int) * count);

    if (!fd_list)
    {
        TRACE("leaving: couldn't alloc memory\n");
	return FALSE;
    }

    SERVER_START_REQ( get_cdrom_eject_fd_list )
    {
        req->cd_drive = drive;
        wine_server_set_reply( req, fd_list, sizeof(int) * count );
        ret = !wine_server_call_err( req );
        if (ret)
        {
            size_t len = wine_server_reply_size( reply );
            count = len / sizeof(int);
        }
    }
    SERVER_END_REQ;

    for (i = 0; i < count; i++)
    {
        TRACE("closing fd: %d\n", fd_list[i]);
        close(fd_list[i]);
    }

    HeapFree( GetProcessHeap(), 0, fd_list);

    TRACE("leaving\n");
    return ret;
}



/**************************************************************************
 *          ForceEjectCDDrive 
 */
BOOL FILE_ForceEjectCDDrive(int drive)
{
    BOOL ret = TRUE;
    BOOL result;
    
    int  i;

    if (drive > 0)
    {
	TRACE("ejecting drive: %c\n", drive);
    	return FILE_DoForceEjectCDDrive (drive);
    }
    else /* Eject *ALL* CD drives */
    {
	TRACE("ejecting all drives\n");
    	for (i = 0; i < MAX_DOS_DRIVES; i++)
    	{
            if (DRIVE_GetType(i) == DRIVE_CDROM)
            {
	        TRACE("ejecting drive: %c\n", 'A' + i);
                result = FILE_DoForceEjectCDDrive('A' + i);
                ret = (result && ret);                
            }
    	}
    }

    return ret;
}

/**************************************************************************
 * ReplaceFileA 
 */
BOOL WINAPI ReplaceFileA(
	LPCSTR lpReplacedFileName, LPCSTR lpReplacementFileName,
	LPCSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude,
	LPVOID lpReserved)
{
    HANDLE hReplaced = INVALID_HANDLE_VALUE;
    HANDLE hReplacement = INVALID_HANDLE_VALUE;
    HANDLE hBackup = INVALID_HANDLE_VALUE;
    DOS_FULL_NAME ReplacedPath, ReplacementPath, BackupPath;
    BOOL Ret = FALSE;

    TRACE("(%s,%s,%s,%08lx,%p,%p)\n",
          lpReplacedFileName, lpReplacementFileName, lpBackupFileName,
	  dwReplaceFlags, lpExclude, lpReserved);

    if (!lpReplacedFileName || !lpReplacementFileName)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!DOSFS_GetFullName (lpReplacedFileName, TRUE, &ReplacedPath))
    {
       SetLastError (ERROR_FILE_NOT_FOUND);
       goto done;
    }

    /* Open target file; settings are from MSDN */
    hReplaced = CreateFileA (lpReplacedFileName, GENERIC_READ | GENERIC_WRITE |
                             DELETE | SYNCHRONIZE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             NULL, (lpBackupFileName ? OPEN_EXISTING : OPEN_ALWAYS), 0, 0);
    if (hReplaced == INVALID_HANDLE_VALUE)
    {
       if (GetLastError () != ERROR_FILE_NOT_FOUND)
          SetLastError (ERROR_UNABLE_TO_REMOVE_REPLACED);
       goto done;
    }

    if (!DOSFS_GetFullName (lpReplacementFileName, TRUE, &ReplacementPath))
    {
       SetLastError (ERROR_PATH_NOT_FOUND);
       goto done;
    }

    /* Open source file; settings are from MSDN */
    hReplacement = CreateFileA (lpReplacementFileName, GENERIC_READ | GENERIC_WRITE |
                                DELETE | WRITE_DAC | SYNCHRONIZE, 0, NULL,
                                OPEN_EXISTING, 0, 0);
    if (hReplacement == INVALID_HANDLE_VALUE)
       goto done;

    if (lpBackupFileName)
    {
       DWORD Attrs;

       Attrs = GetFileAttributesA (lpReplacedFileName);
       if (Attrs == -1)
          goto done;

       if (!DOSFS_GetFullName (lpBackupFileName, FALSE, &BackupPath))
       {
          SetLastError (ERROR_PATH_NOT_FOUND);
          goto done;
       }

       hBackup = CreateFileA (lpBackupFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, Attrs, 0);
       if (hBackup == INVALID_HANDLE_VALUE)
          goto done;

       if (rename (ReplacedPath.long_name, BackupPath.long_name) == -1)
       {
          SetLastError (ERROR_UNABLE_TO_REMOVE_REPLACED);
          goto done;
       }
    }

    if (rename (ReplacementPath.long_name, ReplacedPath.long_name) == -1)
    {
       if (errno == EACCES)
          SetLastError (ERROR_UNABLE_TO_REMOVE_REPLACED);
       else if (!lpBackupFileName)
          SetLastError (ERROR_UNABLE_TO_MOVE_REPLACEMENT);
       else
          SetLastError (ERROR_UNABLE_TO_MOVE_REPLACEMENT_2);

       goto done;
    }

    Ret = TRUE;
 done:
    if (hReplaced != INVALID_HANDLE_VALUE) CloseHandle (hReplaced);
    if (hReplacement != INVALID_HANDLE_VALUE) CloseHandle (hReplacement);
    if (hBackup != INVALID_HANDLE_VALUE) CloseHandle (hBackup);

    return Ret;
}

/**************************************************************************
 * ReplaceFileW
 */
BOOL WINAPI ReplaceFileW(
	LPCWSTR lpReplacedFileName, LPCWSTR lpReplacementFileName,
	LPCWSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude,
	LPVOID lpReserved)
{
    LPSTR lpaReplacedFileName = FILE_strdupWtoA(GetProcessHeap(), 0, lpReplacedFileName);
    LPSTR lpaReplacementFileName = FILE_strdupWtoA(GetProcessHeap(), 0, lpReplacementFileName);
    LPSTR lpaBackupFileName = FILE_strdupWtoA(GetProcessHeap(), 0, lpBackupFileName);
    BOOL res = ReplaceFileA(lpaReplacedFileName, lpaReplacementFileName, lpaBackupFileName,
                            dwReplaceFlags, lpExclude, lpReserved);
    HeapFree(GetProcessHeap(), 0, lpaReplacedFileName);
    HeapFree(GetProcessHeap(), 0, lpaReplacementFileName);
    HeapFree(GetProcessHeap(), 0, lpaBackupFileName);
    return res;
}


/**************************************************************************
 * ReadDirectoryChangesW
 */
BOOL WINAPI ReadDirectoryChangesW(
  HANDLE hDirectory,
  LPVOID lpBuffer,
  DWORD nBufferLength,
  BOOL bWatchSubtree,
  DWORD dwNotifyFilter,
  LPDWORD lpBytesReturned,
  LPOVERLAPPED lpOverlapped,
  LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    FIXME("STUB! hDirectory: %p lpBuffer: %p nBufferLength: %d bWatchSubtree: %d dwNotifyFilter: %d lpBytesReturned: %p lpOverlapped: %p lpCompletionRoutine: %p\n",  hDirectory, lpBuffer, nBufferLength, bWatchSubtree, 
           dwNotifyFilter, lpBytesReturned, lpOverlapped, lpCompletionRoutine);
    return TRUE;
}
