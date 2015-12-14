/*
 * Win32 kernel functions
 *
 * Copyright 1995 Martin von Loewis, Sven Verdoolaege, and Cameron Heide
 */

#include "config.h"
#include "wine/port.h"

#include <errno.h>
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include "winbase.h"
#include "winerror.h"

#include "wine/winbase16.h"
#include "wine/file.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(file);

/**************************************************************************
 *              SetFileAttributes	(KERNEL.421)
 */
BOOL16 WINAPI SetFileAttributes16( LPCSTR lpFileName, DWORD attributes )
{
    return SetFileAttributesA( lpFileName, attributes );
}


/**************************************************************************
 *              SetFileAttributesA	(KERNEL32.@)
 */
BOOL WINAPI SetFileAttributesA(LPCSTR lpFileName, DWORD attributes)
{
    struct stat buf;
    DOS_FULL_NAME full_name;

    if (!DOSFS_GetFullName( lpFileName, TRUE, &full_name ))
        return FALSE;

    TRACE("(%s,%lx)\n",lpFileName,attributes);
    if (attributes & FILE_ATTRIBUTE_NORMAL) {
      attributes &= ~FILE_ATTRIBUTE_NORMAL;
      if (attributes)
        FIXME("(%s):%lx illegal combination with FILE_ATTRIBUTE_NORMAL.\n",
	      lpFileName,attributes);
    }
    if(stat(full_name.long_name,&buf)==-1)
    {
        FILE_SetDosError();
        return FALSE;
    }
    if (attributes & FILE_ATTRIBUTE_READONLY)
    {
        if(S_ISDIR(buf.st_mode))
            /* FIXME */
            WARN("FILE_ATTRIBUTE_READONLY ignored for directory.\n");
        else
            buf.st_mode &= ~0222; /* octal!, clear write permission bits */
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    else
    {
        /* add write permission */
        buf.st_mode |= 0600 | ((buf.st_mode & 044) >> 1);
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (!S_ISDIR(buf.st_mode))
            FIXME("SetFileAttributes expected the file '%s' to be a directory",
                  lpFileName);
	attributes &= ~FILE_ATTRIBUTE_DIRECTORY;
    }


    /* requested executable permissions => set the executable bits for owner, group, and other */
    if (attributes & FILE_ATTRIBUTE_EXECUTABLE){
        buf.st_mode |= 0111;

        attributes &= ~FILE_ATTRIBUTE_EXECUTABLE;
    }

    /* not requesting executable permissions => clear the executable bits */
    else
        buf.st_mode |= 0111;


    attributes &= ~(FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM);
    if (attributes)
        FIXME("(%s):%lx attribute(s) not implemented.\n",
	      lpFileName,attributes);
    if (-1==chmod(full_name.long_name,buf.st_mode))
    {
        char drive[4] = "A:\\";
        drive[0] = lpFileName[0];
        if(GetDriveTypeA(drive) == DRIVE_CDROM) {
           SetLastError( ERROR_ACCESS_DENIED );
           return FALSE;
        }

        /*
        * FIXME: We don't return FALSE here because of differences between
        *        Linux and Windows privileges. Under Linux only the owner of
        *        the file is allowed to change file attributes. Under Windows,
        *        applications expect that if you can write to a file, you can also
        *        change its attributes (see GENERIC_WRITE). We could try to be
        *        clever here but that would break multi-user installations where
        *        users share read-only DLLs. This is because some installers like
        *        to change attributes of already installed DLLs.
        */
        FIXME("Couldn't set file attributes for existing file \"%s\".\n"
              "Check permissions or set VFAT \"quiet\" mount flag\n", full_name.long_name);
    }
    return TRUE;
}


/**************************************************************************
 *              SetFileAttributesW	(KERNEL32.@)
 */
BOOL WINAPI SetFileAttributesW(LPCWSTR lpFileName, DWORD attributes)
{
    BOOL res;
    DWORD len = WideCharToMultiByte( CP_ACP, 0, lpFileName, -1, NULL, 0, NULL, NULL );
    LPSTR afn = HeapAlloc( GetProcessHeap(), 0, len );

    WideCharToMultiByte( CP_ACP, 0, lpFileName, -1, afn, len, NULL, NULL );
    res = SetFileAttributesA( afn, attributes );
    HeapFree( GetProcessHeap(), 0, afn );
    return res;
}
