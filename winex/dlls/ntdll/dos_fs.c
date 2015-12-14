/*
 * DOS file system functions
 *
 * Copyright 1993 Erik Bos
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <time.h>
#include <unistd.h>

#include "windef.h"
#include "winerror.h"
#include "wingdi.h"

#include "wine/unicode.h"
#include "wine/winbase16.h"
#include "drive.h"
#include "wine/file.h"
#include "heap.h"
#include "msdos.h"
#include "winternl.h"
#include "options.h"
#include "wine/server.h"
#include "msvcrt/excpt.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dosfs);
WINE_DECLARE_DEBUG_CHANNEL(file);

/* Ugly workaround needed on MacOS X due to dir position being undefined
   in between closedir() -> opendir(). This has the side-effect of
   possibly messing up some buggy multi-cd installers if they forget to call
   FindClose() at the proper times. See RT#1675 */
#ifndef __APPLE__
#define CACHE_DIR_ENTRY
#endif

/* Define the VFAT ioctl to get both short and long file names */
/* FIXME: is it possible to get this to work on other systems? */
#ifdef linux
/* We want the real kernel dirent structure, not the libc one */
typedef struct
{
    long d_ino;
    long d_off;
    unsigned short d_reclen;
    char d_name[256];
} KERNEL_DIRENT;

#define VFAT_IOCTL_READDIR_BOTH  _IOR('r', 1, KERNEL_DIRENT [2] )

#else   /* linux */
#undef VFAT_IOCTL_READDIR_BOTH  /* just in case... */
#endif  /* linux */

/* Chars we don't want to see in file names */
#define INVALID_NT_CHARS "*?<>|\""
#define INVALID_DOS_CHARS  "*?<>|\"+=,;[] \345"

static const DOS_DEVICE DOSFS_Devices[] =
/* name, device flags (see Int 21/AX=0x4400) */
{
    { "CON",		0xc0d3 },
    { "PRN",		0xa0c0 },
    { "NUL",		0x80c4 },
    { "AUX",		0x80c0 },
    { "LPT1",		0xa0c0 },
    { "LPT2",		0xa0c0 },
    { "LPT3",		0xa0c0 },
    { "LPT4",		0xc0d3 },
    { "COM1",		0x80c0 },
    { "COM2",		0x80c0 },
    { "COM3",		0x80c0 },
    { "COM4",		0x80c0 },
    { "SCSIMGR$",	0xc0c0 },
    { "HPSCAN",         0xc0c0 },
    { "EMMXXXX0",       0x0000 }
};

#define GET_DRIVE(path) \
    (((path)[1] == ':') ? FILE_toupper((path)[0]) - 'A' : DOSFS_CurDrive)

typedef enum {RDSTATE_START, RDSTATE_DOT, RDSTATE_DOTDOT,
              RDSTATE_REST} ReadDirState_t;

/* Directory info for DOSFS_ReadDir */
typedef struct
{
    DIR           *dir;
    ReadDirState_t state;
#ifdef VFAT_IOCTL_READDIR_BOTH
    int            fd;
    char           short_name[12];
    KERNEL_DIRENT  dirent[2];
#endif
} DOS_DIR;

/* Info structure for FindFirstFile handle */
typedef struct
{
    LPSTR path;
    LPSTR long_mask;
    LPSTR short_mask;
    BYTE  attr;
    int   drive;
    int   cur_pos;
    DOS_DIR *dir; /* only used by DOSFS_FindNext() */
#ifdef CACHE_DIR_ENTRY
    off_t cur_dir_entry;
    ReadDirState_t state;
#endif
} FIND_FIRST_INFO;


static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}


/***********************************************************************
 *           DOSFS_ValidDOSName
 *
 * Return 1 if Unix file 'name' is also a valid MS-DOS name
 * (i.e. contains only valid DOS chars, lower-case only, fits in 8.3 format).
 * File name can be terminated by '\0', '\\' or '/'.
 */
static int DOSFS_ValidDOSName( const char *name, int ignore_case )
{
    static const char invalid_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" INVALID_DOS_CHARS;
    const char *p = name;
    const char *invalid = ignore_case ? (invalid_chars + 26) : invalid_chars;
    int len = 0;

    if (*p == '.')
    {
        /* Check for "." and ".." */
        p++;
        if (*p == '.') p++;
        /* All other names beginning with '.' are invalid */
        return (IS_END_OF_NAME(*p));
    }
    while (!IS_END_OF_NAME(*p))
    {
        if (strchr( invalid, *p )) return 0;  /* Invalid char */
        if (*p == '.') break;  /* Start of the extension */
        if (++len > 8) return 0;  /* Name too long */
        p++;
    }
    if (*p != '.') return 1;  /* End of name */
    p++;
    if (IS_END_OF_NAME(*p)) return 0;  /* Empty extension not allowed */
    len = 0;
    while (!IS_END_OF_NAME(*p))
    {
        if (strchr( invalid, *p )) return 0;  /* Invalid char */
        if (*p == '.') return 0;  /* Second extension not allowed */
        if (++len > 3) return 0;  /* Extension too long */
        p++;
    }
    return 1;
}


/***********************************************************************
 *           DOSFS_ToDosFCBFormat
 *
 * Convert a file name to DOS FCB format (8+3 chars, padded with blanks),
 * expanding wild cards and converting to upper-case in the process.
 * File name can be terminated by '\0', '\\' or '/'.
 * Return FALSE if the name is not a valid DOS name.
 * 'buffer' must be at least 12 characters long.
 */
BOOL DOSFS_ToDosFCBFormat( LPCSTR name, LPSTR buffer )
{
    static const char invalid_chars[] = INVALID_DOS_CHARS;
    const char *p = name;
    int i;

    /* Check for "." and ".." */
    if (*p == '.')
    {
        p++;
        strcpy( buffer, ".          " );
        if (*p == '.')
        {
            buffer[1] = '.';
            p++;
        }
        return (!*p || (*p == '/') || (*p == '\\'));
    }

    for (i = 0; i < 8; i++)
    {
        switch(*p)
        {
        case '\0':
        case '\\':
        case '/':
        case '.':
            buffer[i] = ' ';
            break;
        case '?':
            p++;
            /* fall through */
        case '*':
            buffer[i] = '?';
            break;
        default:
            if (strchr( invalid_chars, *p )) return FALSE;
            buffer[i] = FILE_toupper(*p);
            p++;
            break;
        }
    }

    if (*p == '*')
    {
        /* Skip all chars after wildcard up to first dot */
        while (*p && (*p != '/') && (*p != '\\') && (*p != '.')) p++;
    }
    else
    {
        /* Check if name too long */
        if (*p && (*p != '/') && (*p != '\\') && (*p != '.')) return FALSE;
    }
    if (*p == '.') p++;  /* Skip dot */

    for (i = 8; i < 11; i++)
    {
        switch(*p)
        {
        case '\0':
        case '\\':
        case '/':
            buffer[i] = ' ';
            break;
        case '.':
            return FALSE;  /* Second extension not allowed */
        case '?':
            p++;
            /* fall through */
        case '*':
            buffer[i] = '?';
            break;
        default:
            if (strchr( invalid_chars, *p )) return FALSE;
            buffer[i] = FILE_toupper(*p);
            p++;
            break;
        }
    }
    buffer[11] = '\0';

    /* at most 3 character of the extension are processed
     * is something behind this ?
     */
    while (*p == '*' || *p == ' ') p++; /* skip wildcards and spaces */
    return IS_END_OF_NAME(*p);
}


/***********************************************************************
 *           DOSFS_ToDosDTAFormat
 *
 * Convert a file name from FCB to DTA format (name.ext, null-terminated)
 * converting to upper-case in the process.
 * File name can be terminated by '\0', '\\' or '/'.
 * 'buffer' must be at least 13 characters long.
 */
static void DOSFS_ToDosDTAFormat( LPCSTR name, LPSTR buffer )
{
    char *p;

    memcpy( buffer, name, 8 );
    p = buffer + 8;
    while ((p > buffer) && (p[-1] == ' ')) p--;
    *p++ = '.';
    memcpy( p, name + 8, 3 );
    p += 3;
    while (p[-1] == ' ') p--;
    if (p[-1] == '.') p--;
    *p = '\0';
}


/***********************************************************************
 *           DOSFS_MatchShort
 *
 * Check a DOS file name against a mask (both in FCB format).
 */
static int DOSFS_MatchShort( const char *mask, const char *name )
{
    int i;
    for (i = 11; i > 0; i--, mask++, name++)
        if ((*mask != '?') && (*mask != *name)) return 0;
    return 1;
}


/***********************************************************************
 *           DOSFS_MatchLong
 *
 * Check a long file name against a mask.
 *
 * Tests (done in W95 DOS shell - case insensitive):
 * *.txt			test1.test.txt				*
 * *st1*			test1.txt				*
 * *.t??????.t*			test1.ta.tornado.txt			*
 * *tornado*			test1.ta.tornado.txt			*
 * t*t				test1.ta.tornado.txt			*
 * ?est*			test1.txt				*
 * ?est???			test1.txt				-
 * *test1.txt*			test1.txt				*
 * h?l?o*t.dat			hellothisisatest.dat			*
 */
static int DOSFS_MatchLong( const char *mask, const char *name,
                            int case_sensitive )
{
    const char *lastjoker = NULL;
    const char *next_to_retry = NULL;

    if (!strcmp( mask, "*.*" )) return 1;
    while (*name && *mask)
    {
        if (*mask == '*')
        {
            mask++;
            while (*mask == '*') mask++;  /* Skip consecutive '*' */
            lastjoker = mask;
            if (!*mask) return 1; /* end of mask is all '*', so match */

            /* skip to the next match after the joker(s) */
            if (case_sensitive) while (*name && (*name != *mask)) name++;
            else while (*name && (FILE_toupper(*name) != FILE_toupper(*mask))) name++;

            if (!*name) break;
            next_to_retry = name;
        }
        else if (*mask != '?')
        {
            int mismatch = 0;
            if (case_sensitive)
            {
                if (*mask != *name) mismatch = 1;
            }
            else
            {
                if (FILE_toupper(*mask) != FILE_toupper(*name)) mismatch = 1;
            }
            if (!mismatch)
            {
                mask++;
                name++;
                if (*mask == '\0')
                {
                    if (*name == '\0')
                        return 1;
                    if (lastjoker)
                        mask = lastjoker;
                }
            }
            else /* mismatch ! */
            {
                if (lastjoker) /* we had an '*', so we can try unlimitedly */
                {
                    mask = lastjoker;

                    /* this scan sequence was a mismatch, so restart
                     * 1 char after the first char we checked last time */
                    next_to_retry++;
                    name = next_to_retry;
                }
                else
                    return 0; /* bad luck */
            }
        }
        else /* '?' */
        {
            mask++;
            name++;
        }
    }
    while ((*mask == '.') || (*mask == '*'))
        mask++;  /* Ignore trailing '.' or '*' in mask */
    return (!*name && !*mask);
}


/***********************************************************************
 *           DOSFS_OpenDir
 */
static DOS_DIR *DOSFS_OpenDir( LPCSTR path )
{
    DOS_DIR *dir = HeapAlloc( GetProcessHeap(), 0, sizeof(*dir) );
    if (!dir)
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return NULL;
    }

    /* Treat empty path as root directory. This simplifies path split into
       directory and mask in several other places */
    if (!*path) path = "/";

    dir->state = RDSTATE_START;

#ifdef VFAT_IOCTL_READDIR_BOTH
    /* Check if the VFAT ioctl is supported on this directory */

    if ((dir->fd = open( path, O_RDONLY )) != -1)
    {
        if (ioctl( dir->fd, VFAT_IOCTL_READDIR_BOTH, (long)dir->dirent ) == -1)
        {
            close( dir->fd );
            dir->fd = -1;
        }
        else
        {
            /* Set the file pointer back at the start of the directory */
            lseek( dir->fd, 0, SEEK_SET );
            dir->dir = NULL;
            return dir;
        }
    }
#endif  /* VFAT_IOCTL_READDIR_BOTH */

    /* Now use the standard opendir/readdir interface */

    if (!(dir->dir = opendir( path )))
    {
        HeapFree( GetProcessHeap(), 0, dir );
        return NULL;
    }
    return dir;
}


/***********************************************************************
 *           DOSFS_CloseDir
 */
static void DOSFS_CloseDir( DOS_DIR *dir )
{
#ifdef VFAT_IOCTL_READDIR_BOTH
    if (dir->fd != -1) close( dir->fd );
#endif  /* VFAT_IOCTL_READDIR_BOTH */
    if (dir->dir) closedir( dir->dir );
    HeapFree( GetProcessHeap(), 0, dir );
}


/***********************************************************************
 *           DOSFS_ReadDir
 */
static BOOL DOSFS_ReadDir( DOS_DIR *dir, LPCSTR *long_name,
                             LPCSTR *short_name )
{
    struct dirent *dirent;

    /* Why this? Some games depend on the returned list of names starting
       with . and .., and this isn't guaranteed by UNIX filesystems
       (although it normally is the case) */
    if (dir->state == RDSTATE_START)
    {
       *long_name = ".";
       *short_name = NULL;
       dir->state = RDSTATE_DOT;
       return TRUE;
    }
    else if (dir->state == RDSTATE_DOT)
    {
       *long_name = "..";
       *short_name = NULL;
       dir->state = RDSTATE_DOTDOT;
       return TRUE;
    }
    else if (dir->state == RDSTATE_DOTDOT)
       dir->state = RDSTATE_REST;

#ifdef VFAT_IOCTL_READDIR_BOTH
    if (dir->fd != -1)
    {
        if (ioctl( dir->fd, VFAT_IOCTL_READDIR_BOTH, (long)dir->dirent ) != -1) {
	    if (!dir->dirent[0].d_reclen) return FALSE;
	    if (!DOSFS_ToDosFCBFormat( dir->dirent[0].d_name, dir->short_name ))
		dir->short_name[0] = '\0';
	    *short_name = dir->short_name;
	    if (dir->dirent[1].d_name[0]) *long_name = dir->dirent[1].d_name;
	    else *long_name = dir->dirent[0].d_name;
	    return TRUE;
	}
    }
#endif  /* VFAT_IOCTL_READDIR_BOTH */

    if (!(dirent = readdir( dir->dir ))) return FALSE;
    *long_name  = dirent->d_name;
    *short_name = NULL;
    return TRUE;
}


/***********************************************************************
 *           DOSFS_Hash
 *
 * Transform a Unix file name into a hashed DOS name. If the name is a valid
 * DOS name, it is converted to upper-case; otherwise it is replaced by a
 * hashed version that fits in 8.3 format.
 * File name can be terminated by '\0', '\\' or '/'.
 * 'buffer' must be at least 13 characters long.
 */
static void DOSFS_Hash( LPCSTR name, LPSTR buffer, BOOL dir_format,
                        BOOL ignore_case )
{
    static const char invalid_chars[] = INVALID_DOS_CHARS "~.";
    static const char hash_chars[32] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

    const char *p, *ext;
    char *dst;
    unsigned short hash;
    int i;

    if (dir_format) strcpy( buffer, "           " );

    if (DOSFS_ValidDOSName( name, ignore_case ))
    {
        /* Check for '.' and '..' */
        if (*name == '.')
        {
            buffer[0] = '.';
            if (!dir_format) buffer[1] = buffer[2] = '\0';
            if (name[1] == '.') buffer[1] = '.';
            return;
        }

        /* Simply copy the name, converting to uppercase */

        for (dst = buffer; !IS_END_OF_NAME(*name) && (*name != '.'); name++)
            *dst++ = FILE_toupper(*name);
        if (*name == '.')
        {
            if (dir_format) dst = buffer + 8;
            else *dst++ = '.';
            for (name++; !IS_END_OF_NAME(*name); name++)
                *dst++ = FILE_toupper(*name);
        }
        if (!dir_format) *dst = '\0';
        return;
    }

    /* Compute the hash code of the file name */
    /* If you know something about hash functions, feel free to */
    /* insert a better algorithm here... */
    if (ignore_case)
    {
        for (p = name, hash = 0xbeef; !IS_END_OF_NAME(p[1]); p++)
            hash = (hash<<3) ^ (hash>>5) ^ FILE_tolower(*p) ^ (FILE_tolower(p[1]) << 8);
        hash = (hash<<3) ^ (hash>>5) ^ FILE_tolower(*p); /* Last character*/
    }
    else
    {
        for (p = name, hash = 0xbeef; !IS_END_OF_NAME(p[1]); p++)
            hash = (hash << 3) ^ (hash >> 5) ^ *p ^ (p[1] << 8);
        hash = (hash << 3) ^ (hash >> 5) ^ *p;  /* Last character */
    }

    /* Find last dot for start of the extension */
    for (p = name+1, ext = NULL; !IS_END_OF_NAME(*p); p++)
        if (*p == '.') ext = p;
    if (ext && IS_END_OF_NAME(ext[1]))
        ext = NULL;  /* Empty extension ignored */

    /* Copy first 4 chars, replacing invalid chars with '_' */
    for (i = 4, p = name, dst = buffer; i > 0; i--, p++)
    {
        if (IS_END_OF_NAME(*p) || (p == ext)) break;
        *dst++ = strchr( invalid_chars, *p ) ? '_' : FILE_toupper(*p);
    }
    /* Pad to 5 chars with '~' */
    while (i-- >= 0) *dst++ = '~';

    /* Insert hash code converted to 3 ASCII chars */
    *dst++ = hash_chars[(hash >> 10) & 0x1f];
    *dst++ = hash_chars[(hash >> 5) & 0x1f];
    *dst++ = hash_chars[hash & 0x1f];

    /* Copy the first 3 chars of the extension (if any) */
    if (ext)
    {
        if (!dir_format) *dst++ = '.';
        for (i = 3, ext++; (i > 0) && !IS_END_OF_NAME(*ext); i--, ext++)
            *dst++ = strchr( invalid_chars, *ext ) ? '_' : FILE_toupper(*ext);
    }
    if (!dir_format) *dst = '\0';
}


/***********************************************************************
 *           DOSFS_FindUnixName
 *
 * Find the Unix file name in a given directory that corresponds to
 * a file name (either in Unix or DOS format).
 * File name can be terminated by '\0', '\\' or '/'.
 * Return TRUE if OK, FALSE if no file name matches.
 *
 * 'long_buf' must be at least 'long_len' characters long. If the long name
 * turns out to be larger than that, the function returns FALSE.
 * 'short_buf' must be at least 13 characters long.
 */
BOOL DOSFS_FindUnixName (LPCSTR path, LPCSTR name, LPSTR long_buf,
                         INT long_len, LPSTR short_buf, BOOL ignore_case,
                         BOOL *InvalidChars)
{
    DOS_DIR *dir;
    LPCSTR long_name, short_name;
    char dos_name[12], tmp_buf[13];
    BOOL ret;
    char *full_path, *cur_char;
    struct stat st;
    char *pChar;

    const char *p = strchr( name, '/' );
    int len = p ? (int)(p - name) : strlen(name);
    if ((p = strchr( name, '\\' ))) len = min( (int)(p - name), len );
    /* Ignore trailing dots and spaces */
    while (len > 1 && (name[len-1] == '.' || name[len-1] == ' ')) len--;
    if (long_len < len + 1) return FALSE;

    TRACE ("%s,%s\n", path, name);

    if ((pChar = strpbrk (name, INVALID_NT_CHARS)) && (pChar < (name + len)))
    {
       *InvalidChars = TRUE;
       return FALSE;
    }

    if (!DOSFS_ToDosFCBFormat( name, dos_name )) dos_name[0] = '\0';

    /* As an optimization - try the path and the first part of the
       name, and see if that combination exists. If so, we're
       done. From examining traces, this is the case a large
       percentage of the time */
    full_path = HeapAlloc (GetProcessHeap (), 0,
                           strlen (path) + len + 2);
    if (!full_path)
    {
       ERR ("Couldn't allocate full_path - OOM!\n");
       return FALSE;
    }

    strcpy (full_path, path);
    cur_char = full_path + strlen (full_path);
    *cur_char++ = '/';
    strncpy (cur_char, name, len);
    cur_char[len] = '\0';
    if (lstat (full_path, &st) != -1)
    {
        if (long_buf)
           strcpy (long_buf, cur_char);
        if (short_buf)
           DOSFS_Hash (cur_char, short_buf, FALSE, ignore_case);
        TRACE("(%s,%s) -> %s (%s)\n",
              path, name, cur_char, short_buf ? short_buf : "***");

        HeapFree (GetProcessHeap (), 0, full_path);
        return TRUE;
    }
    HeapFree (GetProcessHeap (), 0, full_path);

    if (!(dir = DOSFS_OpenDir( path )))
    {
        WARN("(%s,%s): can't open dir: %s\n",
                       path, name, strerror(errno) );
        return FALSE;
    }

    while ((ret = DOSFS_ReadDir( dir, &long_name, &short_name )))
    {
       if ((dir->state == RDSTATE_REST) &&
           (!strcmp (long_name, ".") || !strcmp (long_name, "..")))
          continue;

        /* Check against Unix name */
        if (len == strlen(long_name))
        {
            if (!ignore_case)
            {
                if (!strncmp( long_name, name, len )) break;
            }
            else
            {
                if (!FILE_strncasecmp( long_name, name, len )) break;
            }
        }

        if (dos_name[0])
        {
            /* Check against hashed DOS name */
            if (!short_name)
            {
                DOSFS_Hash( long_name, tmp_buf, TRUE, ignore_case );
                short_name = tmp_buf;
            }
            if (!strcmp( dos_name, short_name )) break;
        }
    }
    if (ret)
    {
        if (long_buf) strcpy( long_buf, long_name );
        if (short_buf)
        {
            if (short_name)
                DOSFS_ToDosDTAFormat( short_name, short_buf );
            else
                DOSFS_Hash( long_name, short_buf, FALSE, ignore_case );
        }
        TRACE("(%s,%s) -> %s (%s)\n",
              path, name, long_name, short_buf ? short_buf : "***");
    }
    else
        WARN("'%s' not found in '%s'\n", name, path);
    DOSFS_CloseDir( dir );
    return ret;
}


/***********************************************************************
 *           DOSFS_GetDevice
 *
 * Check if a DOS file name represents a DOS device and return the device.
 */
const DOS_DEVICE *DOSFS_GetDevice( const char *name )
{
    int	i;
    const char *p;

    if (!name) return NULL; /* if FILE_DupUnixHandle was used */
    if (name[0] && (name[1] == ':')) name += 2;
    if ((p = strrchr( name, '/' ))) name = p + 1;
    if ((p = strrchr( name, '\\' ))) name = p + 1;
    for (i = 0; i < sizeof(DOSFS_Devices)/sizeof(DOSFS_Devices[0]); i++)
    {
        const char *dev = DOSFS_Devices[i].name;
        if (!FILE_strncasecmp( dev, name, strlen(dev) ))
        {
            p = name + strlen( dev );
            if (!*p || (*p == '.') || (*p == ':')) return &DOSFS_Devices[i];
        }
    }
    return NULL;
}


/***********************************************************************
 *           DOSFS_GetDeviceByHandle
 */
const DOS_DEVICE *DOSFS_GetDeviceByHandle( HFILE hFile )
{
    const DOS_DEVICE *ret = NULL;
    SERVER_START_REQ( get_file_info )
    {
        req->handle = hFile;
        if (!wine_server_call( req ) && (reply->type == FILE_TYPE_UNKNOWN))
        {
            if ((reply->attr >= 0) &&
                (reply->attr < sizeof(DOSFS_Devices)/sizeof(DOSFS_Devices[0])))
                ret = &DOSFS_Devices[reply->attr];
        }
    }
    SERVER_END_REQ;
    return ret;
}


/**************************************************************************
 *         DOSFS_CreateCommPort
 */
static HANDLE DOSFS_CreateCommPort(LPCSTR name, DWORD access, DWORD attributes, LPSECURITY_ATTRIBUTES sa)
{
    HANDLE ret;
    char devname[40];

    TRACE_(file)("%s %lx %lx\n", name, access, attributes);

    PROFILE_GetWineIniString("serialports",name,"",devname,sizeof devname);
    if(!devname[0])
        return 0;

    TRACE("opening %s as %s\n", devname, name);

    SERVER_START_REQ( create_serial )
    {
        req->access  = access;
        req->inherit = (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle);
        req->attributes = attributes;
        req->sharing = FILE_SHARE_READ|FILE_SHARE_WRITE;
        wine_server_add_data( req, devname, strlen(devname) );
        SetLastError(0);
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;

    if(!ret)
        ERR("Couldn't open device '%s' ! (check permissions)\n",devname);
    else
        TRACE("return %08X\n", ret );
    return ret;
}

/***********************************************************************
 *           DOSFS_OpenDevice
 *
 * Open a DOS device. This might not map 1:1 into the UNIX device concept.
 * Returns 0 on failure.
 */
HANDLE DOSFS_OpenDevice( const char *name, DWORD access, DWORD attributes, LPSECURITY_ATTRIBUTES sa )
{
    int i;
    const char *p;
    HANDLE handle;

    if (name[0] && (name[1] == ':')) name += 2;
    if ((p = strrchr( name, '/' ))) name = p + 1;
    if ((p = strrchr( name, '\\' ))) name = p + 1;
    for (i = 0; i < sizeof(DOSFS_Devices)/sizeof(DOSFS_Devices[0]); i++)
    {
        const char *dev = DOSFS_Devices[i].name;
        if (!FILE_strncasecmp( dev, name, strlen(dev) ))
        {
            p = name + strlen( dev );
            if (!*p || (*p == '.') || (*p == ':')) {
	    	/* got it */
		if (!strcmp(DOSFS_Devices[i].name,"NUL"))
                    return FILE_CreateFile( "/dev/null", access,
                                            FILE_SHARE_READ|FILE_SHARE_WRITE, sa,
                                            OPEN_EXISTING, 0, 0, DRIVE_UNKNOWN, 0 );
		if (!strcmp(DOSFS_Devices[i].name,"CON")) {
			HANDLE to_dup;
			switch (access & (GENERIC_READ|GENERIC_WRITE)) {
			case GENERIC_READ:
				to_dup = GetStdHandle( STD_INPUT_HANDLE );
				break;
			case GENERIC_WRITE:
				to_dup = GetStdHandle( STD_OUTPUT_HANDLE );
				break;
			default:
				FIXME("can't open CON read/write\n");
				return 0;
			}
			if (!DuplicateHandle( GetCurrentProcess(), to_dup, GetCurrentProcess(),
					      &handle, 0,
					      sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle,
					      DUPLICATE_SAME_ACCESS ))
			    handle = 0;
			return handle;
		}
		if (!strcmp(DOSFS_Devices[i].name,"SCSIMGR$") ||
                    !strcmp(DOSFS_Devices[i].name,"HPSCAN") ||
                    !strcmp(DOSFS_Devices[i].name,"EMMXXXX0"))
                {
                    return FILE_CreateDevice( i, access, sa );
		}

                if( (handle=DOSFS_CreateCommPort(DOSFS_Devices[i].name,access,attributes,sa)) )
                    return handle;
                FIXME("device open %s not supported (yet)\n",DOSFS_Devices[i].name);
    		return 0;
	    }
        }
    }
    return 0;
}


/***********************************************************************
 *           DOSFS_GetPathDrive
 *
 * Get the drive specified by a given path name (DOS or Unix format).
 */
static int DOSFS_GetPathDrive( const char **name )
{
    int drive;
    const char *p = *name;

    if (*p && (p[1] == ':'))
    {
        drive = FILE_toupper(*p) - 'A';
        *name += 2;
    }
    else if (*p == '/') /* Absolute Unix path? */
    {
        if ((drive = DRIVE_FindDriveRoot( name )) == -1)
        {
            MESSAGE("Warning: %s not accessible from a configured DOS drive\n", *name );
            /* Assume it really was a DOS name */
            drive = DRIVE_GetCurrentDrive();
        }
    }
    else drive = DRIVE_GetCurrentDrive();

    if (!DRIVE_IsValid(drive))
    {
        SetLastError( ERROR_INVALID_DRIVE );
        return -1;
    }
    return drive;
}


/***********************************************************************
 *           DOSFS_GetFullName
 *
 * Convert a file name (DOS or mixed DOS/Unix format) to a valid
 * Unix name / short DOS name pair.
 * Return FALSE if one of the path components does not exist. The last path
 * component is only checked if 'check_last' is non-zero.
 * The buffers pointed to by 'long_buf' and 'short_buf' must be
 * at least MAX_PATHNAME_LEN long.
 */
BOOL DOSFS_GetFullName( LPCSTR name, BOOL check_last, DOS_FULL_NAME *full )
{
    BOOL found;
    UINT flags;
    char *p_l, *p_s, *root;
    BOOL InvalidChars = FALSE;

    TRACE("%s (last=%d)\n", name, check_last );

    if ((!*name) || (*name=='\n'))
    { /* error code for Win98 */
        SetLastError(ERROR_BAD_PATHNAME);
        return FALSE;
    }

    if ((full->drive = DOSFS_GetPathDrive( &name )) == -1) return FALSE;
    flags = DRIVE_GetFlags( full->drive );

    lstrcpynA( full->long_name, DRIVE_GetRoot( full->drive ),
                 sizeof(full->long_name) );
    if (full->long_name[1]) root = full->long_name + strlen(full->long_name);
    else root = full->long_name;  /* root directory */

    strcpy( full->short_name, "A:\\" );
    full->short_name[0] += full->drive;

    if ((*name == '\\') || (*name == '/'))  /* Absolute path */
    {
        while ((*name == '\\') || (*name == '/')) name++;
    }
    else  /* Relative path */
    {
        lstrcpynA( root + 1, DRIVE_GetUnixCwd( full->drive ),
                     sizeof(full->long_name) - (root - full->long_name) - 1 );
        if (root[1]) *root = '/';
        lstrcpynA( full->short_name + 3, DRIVE_GetDosCwd( full->drive ),
                     sizeof(full->short_name) - 3 );
    }

    p_l = full->long_name[1] ? full->long_name + strlen(full->long_name)
                             : full->long_name;
    p_s = full->short_name[3] ? full->short_name + strlen(full->short_name)
                              : full->short_name + 2;
    found = TRUE;

    while (*name && found)
    {
        /* Check for '.' and '..' */

        if (*name == '.')
        {
            if (IS_END_OF_NAME(name[1]))
            {
                name++;
                while ((*name == '\\') || (*name == '/')) name++;
                continue;
            }
            else if ((name[1] == '.') && IS_END_OF_NAME(name[2]))
            {
                name += 2;
                while ((*name == '\\') || (*name == '/')) name++;
                while ((p_l > root) && (*p_l != '/')) p_l--;
                while ((p_s > full->short_name + 2) && (*p_s != '\\')) p_s--;
                *p_l = *p_s = '\0';  /* Remove trailing separator */
                continue;
            }
        }

        /* Make sure buffers are large enough */

        if ((p_s >= full->short_name + sizeof(full->short_name) - 14) ||
            (p_l >= full->long_name + sizeof(full->long_name) - 1))
        {
            SetLastError( ERROR_PATH_NOT_FOUND );
            return FALSE;
        }

        /* Get the long and short name matching the file name */

        if ((found =
             DOSFS_FindUnixName (full->long_name, name, p_l + 1,
                                 sizeof (full->long_name) - (p_l - full->long_name) - 1,
                                 p_s + 1,
                                 !(flags & DRIVE_CASE_SENSITIVE),
                                 &InvalidChars)))
        {
            *p_l++ = '/';
            p_l   += strlen(p_l);
            *p_s++ = '\\';
            p_s   += strlen(p_s);
            while (!IS_END_OF_NAME(*name)) name++;
        }
        else if (!check_last)
        {
            *p_l++ = '/';
            *p_s++ = '\\';
            while (!IS_END_OF_NAME(*name) &&
                   (p_s < full->short_name + sizeof(full->short_name) - 1) &&
                   (p_l < full->long_name + sizeof(full->long_name) - 1))
            {
                *p_s++ = FILE_tolower(*name);
                /* If the drive is case-sensitive we want to create new */
                /* files in lower-case otherwise we can't reopen them   */
                /* under the same short name. */
	    	if (flags & DRIVE_CASE_SENSITIVE) *p_l++ = FILE_tolower(*name);
		else *p_l++ = *name;
                name++;
            }
	    /* Ignore trailing dots and spaces */
	    while(p_l[-1] == '.' || p_l[-1] == ' ') {
		--p_l;
		--p_s;
	    }
            *p_l = *p_s = '\0';
            /* Allow files and directories ending in a slash-dot */
            /* We don't care about leaving wrong numbers of trailing */
            /* slashes because we'll leave the while loop now anyways */
            while ((*name == '\\') || (*name == '/'))
            {
              while ((*name == '\\') || (*name == '/')) name++;
              if ((name[0] == '.') && (name[1] == 0))
                name++;
            }
        }
        while ((*name == '\\') || (*name == '/')) name++;
    }

    if (!found)
    {
        if (check_last)
        {
           if (InvalidChars)
           {
              SetLastError (ERROR_INVALID_NAME);
              return FALSE;
           }

           SetLastError( ERROR_FILE_NOT_FOUND );
           return FALSE;
        }

        if (*name)  /* Not last */
        {
           if (InvalidChars)
           {
              SetLastError (ERROR_INVALID_NAME);
              return FALSE;
           }

           SetLastError( ERROR_PATH_NOT_FOUND );
           return FALSE;
        }
    }
    if (!full->long_name[0]) strcpy( full->long_name, "/" );
    if (!full->short_name[2]) strcpy( full->short_name + 2, "\\" );
    TRACE("returning %s = %s\n", full->long_name, full->short_name );
    return TRUE;
}


/***********************************************************************
 *           GetShortPathNameA   (KERNEL32.@)
 *
 * NOTES
 *  observed:
 *  longpath=NULL: LastError=ERROR_INVALID_PARAMETER, ret=0
 *  longpath="" or invalid: LastError=ERROR_BAD_PATHNAME, ret=0
 *
 * more observations ( with NT 3.51 (WinDD) ):
 * longpath <= 8.3 -> just copy longpath to shortpath
 * longpath > 8.3  ->
 *             a) file does not exist -> return 0, LastError = ERROR_FILE_NOT_FOUND
 *             b) file does exist     -> set the short filename.
 * - trailing slashes are reproduced in the short name, even if the
 *   file is not a directory
 * - the absolute/relative path of the short name is reproduced like found
 *   in the long name
 * - longpath and shortpath may have the same address
 * Peter Ganten, 1999
 */
DWORD WINAPI GetShortPathNameA( LPCSTR longpath, LPSTR shortpath,
                                  DWORD shortlen )
{
    DOS_FULL_NAME full_name;
    LPSTR tmpshortpath;
    DWORD sp = 0, lp = 0;
    int tmplen, drive;
    UINT flags;

    TRACE("%s\n", debugstr_a(longpath));

    if (!longpath) {
      SetLastError(ERROR_INVALID_PARAMETER);
      return 0;
    }
    if (!longpath[0]) {
      SetLastError(ERROR_BAD_PATHNAME);
      return 0;
    }

    if ( ( tmpshortpath = HeapAlloc ( GetProcessHeap(), 0, MAX_PATHNAME_LEN ) ) == NULL ) {
      SetLastError ( ERROR_NOT_ENOUGH_MEMORY );
      return 0;
    }

    /* check for drive letter */
    if ( longpath[1] == ':' ) {
      tmpshortpath[0] = longpath[0];
      tmpshortpath[1] = ':';
      sp = 2;
    }

    if ( ( drive = DOSFS_GetPathDrive ( &longpath )) == -1 ) return 0;
    flags = DRIVE_GetFlags ( drive );

    while ( longpath[lp] ) {

      /* check for path delimiters and reproduce them */
      if ( longpath[lp] == '\\' || longpath[lp] == '/' ) {
	if (!sp || tmpshortpath[sp-1]!= '\\')
        {
	    /* strip double "\\" */
	    tmpshortpath[sp] = '\\';
	    sp++;
        }
        tmpshortpath[sp]=0;/*terminate string*/
	lp++;
	continue;
      }

      tmplen = strcspn ( longpath + lp, "\\/" );
      lstrcpynA ( tmpshortpath+sp, longpath + lp, tmplen+1 );

      /* Check, if the current element is a valid dos name */
      if ( DOSFS_ValidDOSName ( longpath + lp, !(flags & DRIVE_CASE_SENSITIVE) ) ) {
	sp += tmplen;
	lp += tmplen;
	continue;
      }

      /* Check if the file exists and use the existing file name */
      if ( DOSFS_GetFullName ( tmpshortpath, TRUE, &full_name ) ) {
	strcpy( tmpshortpath+sp, strrchr ( full_name.short_name, '\\' ) + 1 );
	sp += strlen ( tmpshortpath+sp );
	lp += tmplen;
	continue;
      }

      TRACE("not found!\n" );
      SetLastError ( ERROR_FILE_NOT_FOUND );
      return 0;
    }
    tmpshortpath[sp] = 0;

    lstrcpynA ( shortpath, tmpshortpath, shortlen );
    TRACE("returning %s\n", debugstr_a(shortpath) );
    tmplen = strlen ( tmpshortpath );
    HeapFree ( GetProcessHeap(), 0, tmpshortpath );

    return tmplen;
}


/***********************************************************************
 *           GetShortPathNameW   (KERNEL32.@)
 */
DWORD WINAPI GetShortPathNameW( LPCWSTR longpath, LPWSTR shortpath,
                                  DWORD shortlen )
{
    LPSTR longpathA, shortpathA;
    DWORD ret = 0;

    longpathA = FILE_strdupWtoA( GetProcessHeap(), 0, longpath );
    shortpathA = HeapAlloc ( GetProcessHeap(), 0, shortlen );

    ret = GetShortPathNameA ( longpathA, shortpathA, shortlen );
    if (shortlen > 0 && !MultiByteToWideChar( CP_ACP, 0, shortpathA, -1, shortpath, shortlen ))
        shortpath[shortlen-1] = 0;
    HeapFree( GetProcessHeap(), 0, longpathA );
    HeapFree( GetProcessHeap(), 0, shortpathA );

    return ret;
}


/***********************************************************************
 *           GetLongPathNameA   (KERNEL32.@)
 *
 * NOTES
 *  observed (Win2000):
 *  shortpath=NULL: LastError=ERROR_INVALID_PARAMETER, ret=0
 *  shortpath="":   LastError=ERROR_PATH_NOT_FOUND, ret=0
 */
DWORD WINAPI GetLongPathNameA( LPCSTR shortpath, LPSTR longpath,
                                  DWORD longlen )
{
    DOS_FULL_NAME full_name;
    char *p, *r, *ll, *ss;

    if (!shortpath) {
      SetLastError(ERROR_INVALID_PARAMETER);
      return 0;
    }
    if (!shortpath[0]) {
      SetLastError(ERROR_PATH_NOT_FOUND);
      return 0;
    }

    if (!DOSFS_GetFullName( shortpath, TRUE, &full_name )) return 0;
    lstrcpynA( longpath, full_name.short_name, longlen );

    /* Do some hackery to get the long filename. */

    if (longpath) {
     ss=longpath+strlen(longpath);
     ll=full_name.long_name+strlen(full_name.long_name);
     p=NULL;
     while (ss>=longpath)
     {
       /* FIXME: aren't we more paranoid, than needed? */
       while ((ss>=longpath) && (ss[0]=='\\')) ss--;
       p=ss;
       while ((ss>=longpath) && (ss[0]!='\\')) ss--;
       if (ss>=longpath)
         {
         /* FIXME: aren't we more paranoid, than needed? */
         while ((ll[0]=='/') && (ll>=full_name.long_name)) ll--;
         while ((ll[0]!='/') && (ll>=full_name.long_name)) ll--;
         if (ll<full_name.long_name)
              {
              ERR("Bad longname! (ss=%s ll=%s)\n This should never happen !\n"
		  ,ss ,ll );
              return 0;
              }
         }
     }

   /* FIXME: fix for names like "C:\\" (ie. with more '\'s) */
      if (p && p[2])
        {
	p+=1;
	if ((p-longpath)>0) longlen -= (p-longpath);
	lstrcpynA( p, ll , longlen);

        /* Now, change all '/' to '\' */
        for (r=p; r[0] != '\0'; r++ )
          if (r[0]=='/') r[0]='\\';
        return strlen(longpath);
        }
    }

    return strlen(longpath);
}


/***********************************************************************
 *           GetLongPathNameW   (KERNEL32.@)
 */
DWORD WINAPI GetLongPathNameW( LPCWSTR shortpath, LPWSTR longpath,
                                  DWORD longlen )
{
    LPSTR longpathA;
    DWORD ret = 0;
    LPSTR shortpathA;

    if (!shortpath)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return ret;
    }
    shortpathA = FILE_strdupWtoA(GetProcessHeap(), 0, shortpath);
    if (!shortpathA)
    {
        ERR("Error allocating memory\n");
        return ret;
    }
    longpathA = HeapAlloc(GetProcessHeap(), 0, longlen);
    if (!longpathA)
    {
        ERR("Error allocating memory\n");
        HeapFree(GetProcessHeap(), 0, shortpathA);
        return ret;
    }

    ret = GetLongPathNameA(shortpathA, longpathA, longlen);
    if ((ret > 0) && (longlen > 0)
        && !MultiByteToWideChar(CP_ACP, 0, longpathA, -1, longpath, longlen))
    {
        longpath[longlen-1] = 0;
    }
    HeapFree(GetProcessHeap(), 0, longpathA);
    HeapFree(GetProcessHeap(), 0, shortpathA);

    return ret;
}


/***********************************************************************
 *           DOSFS_DoGetFullPathName
 *
 * Implementation of GetFullPathNameA/W.
 *
 * bon@elektron 000331:
 * A test for GetFullPathName with many pathological cases
 * now gives identical output for Wine and OSR2
 */
static DWORD DOSFS_DoGetFullPathName( LPCSTR name, DWORD len, LPSTR result,
                                      BOOL unicode )
{
    DWORD ret;
    DOS_FULL_NAME full_name;
    char *p,*q;
    const char * root;
    char drivecur[]="c:.";
    char driveletter=0;
    int namelen,drive=0;
    char trailing_slash=0;

    if ((strlen(name) >1)&& (name[1]==':'))
      /* drive letter given */
      {
	driveletter = name[0];
      }
    if (name[strlen(name)-1] == '\\' ||
            name[strlen(name)-1] == '/')
        /* if the in name has a trailing slash, out should too */
      {
        trailing_slash = '/';
      }
    if ((strlen(name) >2)&& (name[1]==':') &&
	     ((name[2]=='\\') || (name[2]=='/')))
      /* absolute path given */
      {
	lstrcpynA(full_name.short_name,name,MAX_PATHNAME_LEN);
	drive = (int)FILE_toupper(name[0]) - 'A';
      }
    else
      {
	if (driveletter)
	  drivecur[0]=driveletter;
	else
	  strcpy(drivecur,".");
	if (!DOSFS_GetFullName( drivecur, FALSE, &full_name ))
	  {
	    FIXME("internal: error getting drive/path\n");
	    return 0;
	  }
	/* find path that drive letter substitutes*/
	drive = (int)FILE_toupper(full_name.short_name[0]) -0x41;
	root= DRIVE_GetRoot(drive);
	if (!root)
	  {
	    FIXME("internal: error getting DOS Drive Root\n");
	    return 0;
	  }
	if (!strcmp(root,"/"))
	  {
	    /* we have just the last / and we need it. */
	    p= full_name.long_name;
	  }
	else
	  {
	    p= full_name.long_name +strlen(root);
	  }
	/* append long name (= unix name) to drive */
	lstrcpynA(full_name.short_name+2,p,MAX_PATHNAME_LEN-3);
	/* append name to treat */
	namelen= strlen(full_name.short_name);
	p = (char*)name;
	if (driveletter)
	  p += +2; /* skip drive name when appending */
	if (namelen +2  + strlen(p) > MAX_PATHNAME_LEN)
	  {
	    FIXME("internal error: buffer too small\n");
	     return 0;
	  }
	full_name.short_name[namelen++] ='\\';
	full_name.short_name[namelen] = 0;
	lstrcpynA(full_name.short_name +namelen,p,MAX_PATHNAME_LEN-namelen);
      }
    /* reverse all slashes */
    for (p=full_name.short_name;
	 p < full_name.short_name+strlen(full_name.short_name);
	 p++)
      {
	if ( *p == '/' )
	  *p = '\\';
      }
     /* Use memmove, as areas overlap */
     /* Delete .. */
    while ((p = strstr(full_name.short_name,"\\..\\")))
      {
	if (p > full_name.short_name+2)
	  {
	    *p = 0;
	    q = strrchr(full_name.short_name,'\\');
	    memmove(q+1,p+4,strlen(p+4)+1);
	  }
	else
	  {
	    memmove(full_name.short_name+3,p+4,strlen(p+4)+1);
	  }
      }
    if ((full_name.short_name[2]=='.')&&(full_name.short_name[3]=='.'))
	{
	  /* This case istn't treated yet : c:..\test */
	  memmove(full_name.short_name+2,full_name.short_name+4,
		  strlen(full_name.short_name+4)+1);
	}
     /* Delete . */
    while ((p = strstr(full_name.short_name,"\\.\\")))
      {
	*(p+1) = 0;
	memmove(p+1,p+3,strlen(p+3)+1);
      }
    if (!(DRIVE_GetFlags(drive) & DRIVE_CASE_PRESERVING))
        for (p = full_name.short_name; *p; p++) *p = FILE_toupper(*p);
    namelen=strlen(full_name.short_name);
    if (!strcmp(full_name.short_name+namelen-3,"\\.."))
	{
	  /* one more strange case: "c:\test\test1\.."
	   return "c:\test" */
	  *(full_name.short_name+namelen-3)=0;
	  q = strrchr(full_name.short_name,'\\');
	  *q =0;
	}
    if (full_name.short_name[namelen-1]=='.')
	full_name.short_name[(namelen--)-1] =0;
    if (!driveletter)
      if (full_name.short_name[namelen-1]=='\\')
	full_name.short_name[(namelen--)-1] =0;
    /* add trailing slash */
    if (trailing_slash && namelen < MAX_PATHNAME_LEN)
    {
      full_name.short_name[(namelen++)] = trailing_slash;
      full_name.short_name[namelen] = 0;
    }
    /* sometimes there will be multiple trailing slashes, remove them
     * it all depends on which path of the above mess we have come through */
    while (namelen > 3 &&
            full_name.short_name[(namelen-1)] == '/' &&
            full_name.short_name[(namelen-2)] == '/')
    {
      full_name.short_name[(namelen--)] = 0;
    }
    TRACE("got %s\n",full_name.short_name);

    /* If the lpBuffer buffer is too small, the return value is the
    size of the buffer, in characters, required to hold the path
    plus the terminating \0 (tested against win95osr2, bon 001118)
    . */
    ret = strlen(full_name.short_name);
    if (ret >= len )
      {
	/* don't touch anything when the buffer is not large enough */
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
	return ret+1;
      }
    if (result)
    {
	if (unicode)
            MultiByteToWideChar( CP_ACP, 0, full_name.short_name, -1, (LPWSTR)result, len );
	else
	    lstrcpynA( result, full_name.short_name, len );
    }

    TRACE("returning '%s'\n", full_name.short_name );
    return ret;
}


/***********************************************************************
 *           GetFullPathNameA   (KERNEL32.@)
 * NOTES
 *   if the path closed with '\', *lastpart is 0
 */
DWORD WINAPI GetFullPathNameA( LPCSTR name, DWORD len, LPSTR buffer,
                                 LPSTR *lastpart )
{
    DWORD ret;
    if (!*name) return 0;
    ret = DOSFS_DoGetFullPathName( name, len, buffer, FALSE );
    if (ret && (ret<=len) && buffer && lastpart)
    {
        LPSTR p = buffer + strlen(buffer);

	if (*p != '\\')
        {
	  while ((p > buffer + 2) && (*p != '\\')) p--;
          *lastpart = p + 1;
	}
	else *lastpart = NULL;
    }
    return ret;
}


/***********************************************************************
 *           GetFullPathNameW   (KERNEL32.@)
 */
DWORD WINAPI GetFullPathNameW( LPCWSTR name, DWORD len, LPWSTR buffer,
                                 LPWSTR *lastpart )
{
    LPSTR nameA;
    DWORD ret;
    if (!*name) return 0;
    nameA = FILE_strdupWtoA( GetProcessHeap(), 0, name );
    ret = DOSFS_DoGetFullPathName( nameA, len, (LPSTR)buffer, TRUE );
    HeapFree( GetProcessHeap(), 0, nameA );
    if (ret && (ret<=len) && buffer && lastpart)
    {
        LPWSTR p = buffer + strlenW(buffer);
        if (*p != (WCHAR)'\\')
        {
            while ((p > buffer + 2) && (*p != (WCHAR)'\\')) p--;
            *lastpart = p + 1;
        }
        else *lastpart = NULL;
    }
    return ret;
}


/***********************************************************************
 *           wine_get_unix_file_name (KERNEL32.@) Not a Windows API
 *
 * Return the full Unix file name for a given path.
 */
BOOL WINAPI wine_get_unix_file_name( LPCSTR dos, LPSTR buffer, DWORD len )
{
    BOOL ret;
    DOS_FULL_NAME path;
    if ((ret = DOSFS_GetFullName( dos, FALSE, &path ))) lstrcpynA( buffer, path.long_name, len );
    return ret;
}


#ifdef CACHE_DIR_ENTRY
/***********************************************************************
 *           DOSFS_FindNextEx_OpenDir
 *
 *  A simple helper for opening our directory, that handles the different
 *  semantics of seeking to the correct position within the directory.
 *  (Used by DOSFS_FindNextEx)
 */
static DOS_DIR *DOSFS_FindNextEx_OpenDir(FIND_FIRST_INFO *pInfo)
{
    DOS_DIR *pDir = DOSFS_OpenDir (pInfo->path);

   /* Seek to the place where we were at before, since we no longer
    * cache the directory pointer with FindFirstFile and friends.
    * We need to do something different depending on whether
    * we get an fd or a DIR* from DOSFS_OpenDir. */
    if (pDir)
    {
       pDir->state = pInfo->state;

       if (pDir->dir)
       { 
          seekdir (pDir->dir, pInfo->cur_dir_entry);
       }
#ifdef VFAT_IOCTL_READDIR_BOTH
       else if(pDir->fd)
       {
	  lseek(pDir->fd, pInfo->cur_dir_entry, SEEK_SET);
       }
#endif
       else
       {
	   ERR("DOSFS_OpenDir call returned without a directory pointer\n");
       }
    }
    else
    {
       ERR("Unable to open the dir\n");
    }


    return(pDir);
}


/***********************************************************************
 *           DOSFS_FindNextEx_CloseDir
 *
 *  A simple helper for closing a directory pointer.  It caches the current
 *  position in the directory option in the find first information structure, 
 *  using the correct semantics (depending on if we have a DIR* or an fd)
 *  This is all done only if we didn't have a cached DIR* in our pInfo.
 *  (The usual case)
 *  (Used by DOSFS_FindNextEx)
 */
static void DOSFS_FindNextEx_CloseDir(FIND_FIRST_INFO *pInfo, DOS_DIR *pDir)
{
    pInfo->state = pDir->state;

    /* Cache the current directory pointer location. This has a different semantics 
     * depending on whether we are dealing with a DIR* or an fd */
    if (!pInfo->dir)
    {
       if (pDir->dir)	
       {
          pInfo->cur_dir_entry = telldir(pDir->dir);
       }
#ifdef VFAT_IOCTL_READDIR_BOTH
       else if(pDir->fd)
       {
          pInfo->cur_dir_entry = lseek(pDir->fd, 0, SEEK_CUR);   
       }
#endif

       /* Close 'er, we're done */
       DOSFS_CloseDir(pDir);
    }

}
#endif /* CACHE_DIR_ENTRY */


/***********************************************************************
 *           DOSFS_FindNextEx
 */
static int DOSFS_FindNextEx( FIND_FIRST_INFO *info, WIN32_FIND_DATAA *entry )
{
    DWORD attr = info->attr | FA_UNUSED | FA_ARCHIVE | FA_RDONLY | FILE_ATTRIBUTE_SYMLINK;
    UINT flags = DRIVE_GetFlags( info->drive );
    char *p, buffer[MAX_PATHNAME_LEN];
    const char *drive_path;
    int drive_root;
    LPCSTR long_name, short_name;
    BY_HANDLE_FILE_INFORMATION fileinfo;
    char dos_name[13];
    DOS_DIR* DirPtr;

    if ((info->attr & ~(FA_UNUSED | FA_ARCHIVE | FA_RDONLY)) == FA_LABEL)
    {
        LARGE_INTEGER li;

        if (info->cur_pos) return 0;
        entry->dwFileAttributes  = FILE_ATTRIBUTE_LABEL;
        RtlSecondsSince1970ToTime( (time_t)0, &li );
        entry->ftCreationTime.dwLowDateTime = li.u.LowPart;
        entry->ftCreationTime.dwHighDateTime = li.u.HighPart;
        RtlSecondsSince1970ToTime( (time_t)0, &li );
        entry->ftLastAccessTime.dwLowDateTime = li.u.LowPart;
        entry->ftLastAccessTime.dwHighDateTime = li.u.HighPart;
        RtlSecondsSince1970ToTime( (time_t)0, &li );
        entry->ftLastWriteTime.dwLowDateTime = li.u.LowPart;
        entry->ftLastWriteTime.dwHighDateTime = li.u.HighPart;
        entry->nFileSizeHigh     = 0;
        entry->nFileSizeLow      = 0;
        entry->dwReserved0       = 0;
        entry->dwReserved1       = 0;
        DOSFS_ToDosDTAFormat( DRIVE_GetLabel( info->drive ), entry->cFileName );
        strcpy( entry->cAlternateFileName, entry->cFileName );
        info->cur_pos++;
        TRACE("returning %s (%s) as label\n",
               entry->cFileName, entry->cAlternateFileName);
        return 1;
    }

    drive_path = info->path + strlen(DRIVE_GetRoot( info->drive ));
    while ((*drive_path == '/') || (*drive_path == '\\')) drive_path++;
    drive_root = !*drive_path;

    lstrcpynA( buffer, info->path, sizeof(buffer) - 1 );
    strcat( buffer, "/" );
    p = buffer + strlen(buffer);

    
#ifdef CACHE_DIR_ENTRY
    /* Open up the directory and advance to where we were before. (We can't cache it
     * due to some buggy installers not calling FindClose between CD changes, preventing
     * the CD from being unmounted) 
     *
     * We could get a dir cached entry from DOSFS_FindNext though, so we still need to 
     * check for that */
    if (info->dir)
    {
       DirPtr = info->dir;
    }
    else
    {
       DirPtr = DOSFS_FindNextEx_OpenDir(info);
    }
    
    if( !DirPtr ) return 0; /* File or directory does not exist */
#else
    DirPtr = info->dir;
#endif

    while (DOSFS_ReadDir( DirPtr, &long_name, &short_name ))
    {
        if ((DirPtr->state == RDSTATE_REST) &&
            (!strcmp (long_name, ".") || !strcmp (long_name, "..")))
            continue;

        info->cur_pos++;

        /* Don't return '.' and '..' in the root of the drive */
        if (drive_root && (long_name[0] == '.') &&
            (!long_name[1] || ((long_name[1] == '.') && !long_name[2])))
            continue;

        /* Check the long mask */

        if (info->long_mask)
        {
            if (!DOSFS_MatchLong( info->long_mask, long_name,
                                  flags & DRIVE_CASE_SENSITIVE )) continue;
        }

        /* Check the short mask */

        if (info->short_mask)
        {
            if (!short_name)
            {
                DOSFS_Hash( long_name, dos_name, TRUE,
                            !(flags & DRIVE_CASE_SENSITIVE) );
                short_name = dos_name;
            }
            if (!DOSFS_MatchShort( info->short_mask, short_name )) continue;
        }

        /* Check the file attributes */

        lstrcpynA( p, long_name, sizeof(buffer) - (int)(p - buffer) );
        if (!FILE_Stat( buffer, &fileinfo ))
        {
            WARN("can't stat %s\n", buffer);
            continue;
        }
        if ((fileinfo.dwFileAttributes & FILE_ATTRIBUTE_SYMLINK) &&
            (fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            static int show_dir_symlinks = -1;
            if (show_dir_symlinks == -1)
                show_dir_symlinks = PROFILE_GetWineIniBool("wine", "ShowDirSymlinks", 0);
            if (!show_dir_symlinks) continue;
        }

        if (fileinfo.dwFileAttributes & ~attr) continue;

        /* We now have a matching entry; fill the result and return */

        entry->dwFileAttributes = fileinfo.dwFileAttributes;
        entry->ftCreationTime   = fileinfo.ftCreationTime;
        entry->ftLastAccessTime = fileinfo.ftLastAccessTime;
        entry->ftLastWriteTime  = fileinfo.ftLastWriteTime;
        entry->nFileSizeHigh    = fileinfo.nFileSizeHigh;
        entry->nFileSizeLow     = fileinfo.nFileSizeLow;

        if (short_name)
            DOSFS_ToDosDTAFormat( short_name, entry->cAlternateFileName );
        else
            DOSFS_Hash( long_name, entry->cAlternateFileName, FALSE,
                        !(flags & DRIVE_CASE_SENSITIVE) );

        lstrcpynA( entry->cFileName, long_name, sizeof(entry->cFileName) );
        if (!(flags & DRIVE_CASE_PRESERVING)) _strlwr( entry->cFileName );
        TRACE("returning %s (%s) %02lx %ld\n",
              entry->cFileName, entry->cAlternateFileName,
              entry->dwFileAttributes, entry->nFileSizeLow );

#ifdef CACHE_DIR_ENTRY
        DOSFS_FindNextEx_CloseDir(info, DirPtr);
#endif
        return 1;
    }

#ifdef CACHE_DIR_ENTRY
    DOSFS_FindNextEx_CloseDir(info, DirPtr);
#endif
    return 0;  /* End of directory */
}

/***********************************************************************
 *           DOSFS_FindNext
 *
 * Find the next matching file. Return the number of entries read to find
 * the matching one, or 0 if no more entries.
 * 'short_mask' is the 8.3 mask (in FCB format), 'long_mask' is the long
 * file name mask. Either or both can be NULL.
 *
 * NOTE: This is supposed to be only called by the int21 emulation
 *       routines. Thus, we should own the Win16Mutex anyway.
 *       Nevertheless, we explicitly enter it to ensure the static
 *       directory cache is protected.
 */
int DOSFS_FindNext( const char *path, const char *short_mask,
                    const char *long_mask, int drive, BYTE attr,
                    int skip, WIN32_FIND_DATAA *entry )
{
    static FIND_FIRST_INFO info;
    LPCSTR short_name, long_name;
    int count;
    _EnterWin16Lock();

    /* Check the cached directory */
    if (!(info.dir && info.path == path && info.short_mask == short_mask
                   && info.long_mask == long_mask && info.drive == drive
                   && info.attr == attr && info.cur_pos <= skip))
    {
        /* Not in the cache, open it anew */
        if (info.dir) DOSFS_CloseDir( info.dir );

        info.path = (LPSTR)path;
        info.long_mask = (LPSTR)long_mask;
        info.short_mask = (LPSTR)short_mask;
        info.attr = attr;
        info.drive = drive;
        info.cur_pos = 0;
#ifdef CACHE_DIR_ENTRY
	info.cur_dir_entry = -1;
	info.state = RDSTATE_START;
#endif
        info.dir = DOSFS_OpenDir( info.path );
    }

    /* Skip to desired position */
    while (info.cur_pos < skip)
        if (info.dir && DOSFS_ReadDir( info.dir, &long_name, &short_name ))
            info.cur_pos++;
        else
            break;

    if (info.dir && info.cur_pos == skip && DOSFS_FindNextEx( &info, entry ))
        count = info.cur_pos - skip;
    else
        count = 0;

    if (!count)
    {
        if (info.dir) DOSFS_CloseDir( info.dir );
        memset( &info, '\0', sizeof(info) );
    }

    _LeaveWin16Lock();

    return count;
}

/*************************************************************************
 *           FindFirstFileExA  (KERNEL32.@)
 */
HANDLE WINAPI FindFirstFileExA(
	LPCSTR lpFileName,
	FINDEX_INFO_LEVELS fInfoLevelId,
	LPVOID lpFindFileData,
	FINDEX_SEARCH_OPS fSearchOp,
	LPVOID lpSearchFilter,
	DWORD dwAdditionalFlags)
{
    DOS_FULL_NAME full_name;
    HGLOBAL handle;
    FIND_FIRST_INFO *info;
    if (((fSearchOp != FindExSearchNameMatch) && (fSearchOp != FindExSearchLimitToDirectories)) ||
        (dwAdditionalFlags != 0))
    {
        FIXME("options not implemented 0x%08x 0x%08lx\n", fSearchOp, dwAdditionalFlags );
        SetLastError (ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    if (!lpFileName)
    {
       SetLastError (ERROR_INVALID_PARAMETER);
       return INVALID_HANDLE_VALUE;
    }

    switch(fInfoLevelId)
    {
      case FindExInfoStandard:
        {
          WIN32_FIND_DATAA * data = (WIN32_FIND_DATAA *) lpFindFileData;
          const DOS_DEVICE *pDev;

          data->dwReserved0 = data->dwReserved1 = 0x0;
          if (!strncmp(lpFileName, "\\\\?\\", 4)) lpFileName += 4;

          /* Check for root directory */
          if ((strlen (lpFileName) == 3) && (lpFileName[1] == ':') &&
              (lpFileName[2] == '\\'))
          {
             SetLastError (ERROR_FILE_NOT_FOUND);
             break;
          }

          if ((strrchr(lpFileName, '\\') - lpFileName + 1) == strlen(lpFileName) )
          {
             SetLastError (ERROR_PATH_NOT_FOUND);
             break;
          }
          if (!DOSFS_GetFullName( lpFileName, FALSE, &full_name ))
             break;
          if (!(handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(FIND_FIRST_INFO)))) break;
          info = (FIND_FIRST_INFO *)GlobalLock( handle );
          info->path = HeapAlloc( GetProcessHeap(), 0, strlen(full_name.long_name)+1 );
          strcpy( info->path, full_name.long_name );
          info->long_mask = strrchr( info->path, '/' );
          *(info->long_mask++) = '\0';
          info->short_mask = NULL;
          info->attr = 0xff;
          if (lpFileName[0] && (lpFileName[1] == ':'))
              info->drive = FILE_toupper(*lpFileName) - 'A';
          else info->drive = DRIVE_GetCurrentDrive();
          info->cur_pos = 0;

          if ((pDev = DOSFS_GetDevice (lpFileName)))
          {
             info->dir = NULL;
#ifdef CACHE_DIR_ENTRY
             info->cur_dir_entry = 0;
#endif

             /* Values taken from XP SP 2 */
             memset (data, 0, sizeof (*data));
             data->dwFileAttributes = FILE_ATTRIBUTE_ARCHIVE;
             strcpy (data->cFileName, pDev->name);
             GlobalUnlock (handle);
          }
          else
          {
#ifdef CACHE_DIR_ENTRY
             info->dir = NULL;
             info->cur_dir_entry = 0;
             info->state = RDSTATE_START;
#else
             info->dir = DOSFS_OpenDir (info->path);
#endif

             GlobalUnlock( handle );
             if (!FindNextFileA( handle, data ))
             {
                FindClose( handle );
                SetLastError( ERROR_NO_MORE_FILES );
                break;
             }
          }
          return handle;
        }
        break;
      default:
        FIXME("fInfoLevelId 0x%08x not implemented\n", fInfoLevelId );
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    return INVALID_HANDLE_VALUE;
}

/*************************************************************************
 *           FindFirstFileA   (KERNEL32.@)
 */
HANDLE WINAPI FindFirstFileA(
	LPCSTR lpFileName,
	WIN32_FIND_DATAA *lpFindData )
{
    return FindFirstFileExA(lpFileName, FindExInfoStandard, lpFindData,
                            FindExSearchNameMatch, NULL, 0);
}

/*************************************************************************
 *           FindFirstFileExW   (KERNEL32.@)
 */
HANDLE WINAPI FindFirstFileExW(
	LPCWSTR lpFileName,
	FINDEX_INFO_LEVELS fInfoLevelId,
	LPVOID lpFindFileData,
	FINDEX_SEARCH_OPS fSearchOp,
	LPVOID lpSearchFilter,
	DWORD dwAdditionalFlags)
{
    HANDLE handle;
    WIN32_FIND_DATAA dataA;
    LPVOID _lpFindFileData;
    LPSTR pathA;

    switch(fInfoLevelId)
    {
      case FindExInfoStandard:
        {
          _lpFindFileData = &dataA;
        }
        break;
      default:
        FIXME("fInfoLevelId 0x%08x not implemented\n", fInfoLevelId );
	return INVALID_HANDLE_VALUE;
    }

    pathA = FILE_strdupWtoA( GetProcessHeap(), 0, lpFileName );
    handle = FindFirstFileExA(pathA, fInfoLevelId, _lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
    HeapFree( GetProcessHeap(), 0, pathA );
    if (handle == INVALID_HANDLE_VALUE) return handle;

    switch(fInfoLevelId)
    {
      case FindExInfoStandard:
        {
          WIN32_FIND_DATAW *dataW = (WIN32_FIND_DATAW*) lpFindFileData;
          dataW->dwFileAttributes = dataA.dwFileAttributes;
          dataW->ftCreationTime   = dataA.ftCreationTime;
          dataW->ftLastAccessTime = dataA.ftLastAccessTime;
          dataW->ftLastWriteTime  = dataA.ftLastWriteTime;
          dataW->nFileSizeHigh    = dataA.nFileSizeHigh;
          dataW->nFileSizeLow     = dataA.nFileSizeLow;
          MultiByteToWideChar( CP_ACP, 0, dataA.cFileName, -1,
                               dataW->cFileName, sizeof(dataW->cFileName)/sizeof(WCHAR) );
          MultiByteToWideChar( CP_ACP, 0, dataA.cAlternateFileName, -1,
                               dataW->cAlternateFileName,
                               sizeof(dataW->cAlternateFileName)/sizeof(WCHAR) );
        }
        break;
      default:
        FIXME("fInfoLevelId 0x%08x not implemented\n", fInfoLevelId );
        return INVALID_HANDLE_VALUE;
    }
    return handle;
}

/*************************************************************************
 *           FindFirstFileW   (KERNEL32.@)
 */
HANDLE WINAPI FindFirstFileW( LPCWSTR lpFileName, WIN32_FIND_DATAW *lpFindData )
{
    return FindFirstFileExW(lpFileName, FindExInfoStandard, lpFindData,
                            FindExSearchNameMatch, NULL, 0);
}

/*************************************************************************
 *           FindNextFileA   (KERNEL32.@)
 */
BOOL WINAPI FindNextFileA( HANDLE handle, WIN32_FIND_DATAA *data )
{
    FIND_FIRST_INFO *info;

    if ((handle == INVALID_HANDLE_VALUE) ||
       !(info = (FIND_FIRST_INFO *)GlobalLock( handle )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }
    GlobalUnlock( handle );
#ifdef CACHE_DIR_ENTRY
    if (!info->path)
#else
    if (!info->path || !info->dir)
#endif
    {
        SetLastError( ERROR_NO_MORE_FILES );
        return FALSE;
    }
    if (!DOSFS_FindNextEx( info, data ))
    {
#ifndef CACHE_DIR_ENTRY
        DOSFS_CloseDir (info->dir);
        info->dir = NULL;
#endif
        HeapFree( GetProcessHeap(), 0, info->path );
        info->path = info->long_mask = NULL;
        SetLastError( ERROR_NO_MORE_FILES );
        return FALSE;
    }
    return TRUE;
}


/*************************************************************************
 *           FindNextFileW   (KERNEL32.@)
 */
BOOL WINAPI FindNextFileW( HANDLE handle, WIN32_FIND_DATAW *data )
{
    WIN32_FIND_DATAA dataA;
    if (!FindNextFileA( handle, &dataA )) return FALSE;
    data->dwFileAttributes = dataA.dwFileAttributes;
    data->ftCreationTime   = dataA.ftCreationTime;
    data->ftLastAccessTime = dataA.ftLastAccessTime;
    data->ftLastWriteTime  = dataA.ftLastWriteTime;
    data->nFileSizeHigh    = dataA.nFileSizeHigh;
    data->nFileSizeLow     = dataA.nFileSizeLow;
    MultiByteToWideChar( CP_ACP, 0, dataA.cFileName, -1,
                         data->cFileName, sizeof(data->cFileName)/sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, dataA.cAlternateFileName, -1,
                         data->cAlternateFileName,
                         sizeof(data->cAlternateFileName)/sizeof(WCHAR) );
    return TRUE;
}

/*************************************************************************
 *           FindClose   (KERNEL32.@)
 */
BOOL WINAPI FindClose( HANDLE handle )
{
    FIND_FIRST_INFO *info;
    DWORD Err;

    if ((handle == INVALID_HANDLE_VALUE) ||
        !(info = (FIND_FIRST_INFO *)GlobalLock( handle )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }
    __TRY
    {
#ifndef CACHE_DIR_ENTRY
        if (info->dir)
            DOSFS_CloseDir (info->dir);
#endif
        if (info->path) {
            HeapFree( GetProcessHeap(), 0, info->path );
            info->path = NULL;
        }
    }
    __EXCEPT(page_fault)
    {
        WARN("Illegal handle %x\n", handle);
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }
    __ENDTRY

    /* Need to preserve error, as GlobalUnlock will clear it */
    Err = GetLastError ();       
    GlobalUnlock( handle );
    GlobalFree( handle );
    SetLastError (Err);

    return TRUE;
}

/***********************************************************************
 *           DOSFS_UnixTimeToFileTime
 *
 * Convert a Unix time to FILETIME format.
 * The FILETIME structure is a 64-bit value representing the number of
 * 100-nanosecond intervals since January 1, 1601, 0:00.
 * 'remainder' is the nonnegative number of 100-ns intervals
 * corresponding to the time fraction smaller than 1 second that
 * couldn't be stored in the time_t value.
 */
void DOSFS_UnixTimeToFileTime( time_t unix_time, FILETIME *filetime,
                               DWORD remainder )
{
    /* NOTES:

       CONSTANTS:
       The time difference between 1 January 1601, 00:00:00 and
       1 January 1970, 00:00:00 is 369 years, plus the leap years
       from 1604 to 1968, excluding 1700, 1800, 1900.
       This makes (1968 - 1600) / 4 - 3 = 89 leap days, and a total
       of 134774 days.

       Any day in that period had 24 * 60 * 60 = 86400 seconds.

       The time difference is 134774 * 86400 * 10000000, which can be written
       116444736000000000
       27111902 * 2^32 + 3577643008
       413 * 2^48 + 45534 * 2^32 + 54590 * 2^16 + 32768

       If you find that these constants are buggy, please change them in all
       instances in both conversion functions.

       VERSIONS:
       There are two versions, one of them uses long long variables and
       is presumably faster but not ISO C. The other one uses standard C
       data types and operations but relies on the assumption that negative
       numbers are stored as 2's complement (-1 is 0xffff....). If this
       assumption is violated, dates before 1970 will not convert correctly.
       This should however work on any reasonable architecture where WINE
       will run.

       DETAILS:

       Take care not to remove the casts. I have tested these functions
       (in both versions) for a lot of numbers. I would be interested in
       results on other compilers than GCC.

       The operations have been designed to account for the possibility
       of 64-bit time_t in future UNICES. Even the versions without
       internal long long numbers will work if time_t only is 64 bit.
       A 32-bit shift, which was necessary for that operation, turned out
       not to work correctly in GCC, besides giving the warning. So I
       used a double 16-bit shift instead. Numbers are in the ISO version
       represented by three limbs, the most significant with 32 bit, the
       other two with 16 bit each.

       As the modulo-operator % is not well-defined for negative numbers,
       negative divisors have been avoided in DOSFS_FileTimeToUnixTime.

       There might be quicker ways to do this in C. Certainly so in
       assembler.

       Claus Fischer, fischer@iue.tuwien.ac.at
       */

#if SIZEOF_LONG_LONG >= 8
#  define USE_LONG_LONG 1
#else
#  define USE_LONG_LONG 0
#endif

#if USE_LONG_LONG		/* gcc supports long long type */

    long long int t = unix_time;
    t *= 10000000;
    t += 116444736000000000LL;
    t += remainder;
    filetime->dwLowDateTime  = (UINT)t;
    filetime->dwHighDateTime = (UINT)(t >> 32);

#else  /* ISO version */

    UINT a0;			/* 16 bit, low    bits */
    UINT a1;			/* 16 bit, medium bits */
    UINT a2;			/* 32 bit, high   bits */

    /* Copy the unix time to a2/a1/a0 */
    a0 =  unix_time & 0xffff;
    a1 = (unix_time >> 16) & 0xffff;
    /* This is obsolete if unix_time is only 32 bits, but it does not hurt.
       Do not replace this by >> 32, it gives a compiler warning and it does
       not work. */
    a2 = (unix_time >= 0 ? (unix_time >> 16) >> 16 :
	  ~((~unix_time >> 16) >> 16));

    /* Multiply a by 10000000 (a = a2/a1/a0)
       Split the factor into 10000 * 1000 which are both less than 0xffff. */
    a0 *= 10000;
    a1 = a1 * 10000 + (a0 >> 16);
    a2 = a2 * 10000 + (a1 >> 16);
    a0 &= 0xffff;
    a1 &= 0xffff;

    a0 *= 1000;
    a1 = a1 * 1000 + (a0 >> 16);
    a2 = a2 * 1000 + (a1 >> 16);
    a0 &= 0xffff;
    a1 &= 0xffff;

    /* Add the time difference and the remainder */
    a0 += 32768 + (remainder & 0xffff);
    a1 += 54590 + (remainder >> 16   ) + (a0 >> 16);
    a2 += 27111902                     + (a1 >> 16);
    a0 &= 0xffff;
    a1 &= 0xffff;

    /* Set filetime */
    filetime->dwLowDateTime  = (a1 << 16) + a0;
    filetime->dwHighDateTime = a2;
#endif
}


/***********************************************************************
 *           DOSFS_FileTimeToUnixTime
 *
 * Convert a FILETIME format to Unix time.
 * If not NULL, 'remainder' contains the fractional part of the filetime,
 * in the range of [0..9999999] (even if time_t is negative).
 */
time_t DOSFS_FileTimeToUnixTime( const FILETIME *filetime, DWORD *remainder )
{
    /* Read the comment in the function DOSFS_UnixTimeToFileTime. */
#if USE_LONG_LONG

    long long int t = filetime->dwHighDateTime;
    t <<= 32;
    t += (UINT)filetime->dwLowDateTime;
    t -= 116444736000000000LL;
    if (t < 0)
    {
	if (remainder) *remainder = 9999999 - (-t - 1) % 10000000;
	return -1 - ((-t - 1) / 10000000);
    }
    else
    {
	if (remainder) *remainder = t % 10000000;
	return t / 10000000;
    }

#else  /* ISO version */

    UINT a0;			/* 16 bit, low    bits */
    UINT a1;			/* 16 bit, medium bits */
    UINT a2;			/* 32 bit, high   bits */
    UINT r;			/* remainder of division */
    unsigned int carry;		/* carry bit for subtraction */
    int negative;		/* whether a represents a negative value */

    /* Copy the time values to a2/a1/a0 */
    a2 =  (UINT)filetime->dwHighDateTime;
    a1 = ((UINT)filetime->dwLowDateTime ) >> 16;
    a0 = ((UINT)filetime->dwLowDateTime ) & 0xffff;

    /* Subtract the time difference */
    if (a0 >= 32768           ) a0 -=             32768        , carry = 0;
    else                        a0 += (1 << 16) - 32768        , carry = 1;

    if (a1 >= 54590    + carry) a1 -=             54590 + carry, carry = 0;
    else                        a1 += (1 << 16) - 54590 - carry, carry = 1;

    a2 -= 27111902 + carry;

    /* If a is negative, replace a by (-1-a) */
    negative = (a2 >= ((UINT)1) << 31);
    if (negative)
    {
	/* Set a to -a - 1 (a is a2/a1/a0) */
	a0 = 0xffff - a0;
	a1 = 0xffff - a1;
	a2 = ~a2;
    }

    /* Divide a by 10000000 (a = a2/a1/a0), put the rest into r.
       Split the divisor into 10000 * 1000 which are both less than 0xffff. */
    a1 += (a2 % 10000) << 16;
    a2 /=       10000;
    a0 += (a1 % 10000) << 16;
    a1 /=       10000;
    r   =  a0 % 10000;
    a0 /=       10000;

    a1 += (a2 % 1000) << 16;
    a2 /=       1000;
    a0 += (a1 % 1000) << 16;
    a1 /=       1000;
    r  += (a0 % 1000) * 10000;
    a0 /=       1000;

    /* If a was negative, replace a by (-1-a) and r by (9999999 - r) */
    if (negative)
    {
	/* Set a to -a - 1 (a is a2/a1/a0) */
	a0 = 0xffff - a0;
	a1 = 0xffff - a1;
	a2 = ~a2;

        r  = 9999999 - r;
    }

    if (remainder) *remainder = r;

    /* Do not replace this by << 32, it gives a compiler warning and it does
       not work. */
    return ((((time_t)a2) << 16) << 16) + (a1 << 16) + a0;
#endif
}


/***********************************************************************
 *           MulDiv   (KERNEL32.@)
 * RETURNS
 *	Result of multiplication and division
 *	-1: Overflow occurred or Divisor was 0
 */
INT WINAPI MulDiv(
	     INT nMultiplicand,
	     INT nMultiplier,
	     INT nDivisor)
{
#if SIZEOF_LONG_LONG >= 8
    long long ret;

    if (!nDivisor) return -1;

    /* We want to deal with a positive divisor to simplify the logic. */
    if (nDivisor < 0)
    {
      nMultiplicand = - nMultiplicand;
      nDivisor = -nDivisor;
    }

    /* If the result is positive, we "add" to round. else, we subtract to round. */
    if ( ( (nMultiplicand <  0) && (nMultiplier <  0) ) ||
	 ( (nMultiplicand >= 0) && (nMultiplier >= 0) ) )
      ret = (((long long)nMultiplicand * nMultiplier) + (nDivisor/2)) / nDivisor;
    else
      ret = (((long long)nMultiplicand * nMultiplier) - (nDivisor/2)) / nDivisor;

    if ((ret > 2147483647) || (ret < -2147483647)) return -1;
    return ret;
#else
    if (!nDivisor) return -1;

    /* We want to deal with a positive divisor to simplify the logic. */
    if (nDivisor < 0)
    {
      nMultiplicand = - nMultiplicand;
      nDivisor = -nDivisor;
    }

    /* If the result is positive, we "add" to round. else, we subtract to round. */
    if ( ( (nMultiplicand <  0) && (nMultiplier <  0) ) ||
	 ( (nMultiplicand >= 0) && (nMultiplier >= 0) ) )
      return ((nMultiplicand * nMultiplier) + (nDivisor/2)) / nDivisor;

    return ((nMultiplicand * nMultiplier) - (nDivisor/2)) / nDivisor;

#endif
}


/***********************************************************************
 *           DosDateTimeToFileTime   (KERNEL32.@)
 */
BOOL WINAPI DosDateTimeToFileTime( WORD fatdate, WORD fattime, LPFILETIME ft)
{
    struct tm newtm;
    LARGE_INTEGER li;
#ifndef HAVE_TIMEGM
    struct tm *gtm;
    time_t time1, time2;
#endif

    newtm.tm_sec  = (fattime & 0x1f) * 2;
    newtm.tm_min  = (fattime >> 5) & 0x3f;
    newtm.tm_hour = (fattime >> 11);
    newtm.tm_mday = (fatdate & 0x1f);
    newtm.tm_mon  = ((fatdate >> 5) & 0x0f) - 1;
    newtm.tm_year = (fatdate >> 9) + 80;
#ifdef HAVE_TIMEGM
    RtlSecondsSince1970ToTime( timegm(&newtm), &li );
#else
    time1 = mktime(&newtm);
    gtm = gmtime(&time1);
    time2 = mktime(gtm);
    RtlSecondsSince1970ToTime( 2*time1-time2, &li );
#endif
    ft->dwLowDateTime = li.u.LowPart;
    ft->dwHighDateTime = li.u.HighPart;
    return TRUE;
}


/***********************************************************************
 *           FileTimeToDosDateTime   (KERNEL32.@)
 */
BOOL WINAPI FileTimeToDosDateTime( const FILETIME *ft, LPWORD fatdate,
                                     LPWORD fattime )
{
    time_t unixtime = DOSFS_FileTimeToUnixTime( ft, NULL );
    struct tm *tm = gmtime( &unixtime );
    if (fattime)
        *fattime = (tm->tm_hour << 11) + (tm->tm_min << 5) + (tm->tm_sec / 2);
    if (fatdate)
        *fatdate = ((tm->tm_year - 80) << 9) + ((tm->tm_mon + 1) << 5)
                   + tm->tm_mday;
    return TRUE;
}


/***********************************************************************
 *           LocalFileTimeToFileTime   (KERNEL32.@)
 */
BOOL WINAPI LocalFileTimeToFileTime( const FILETIME *localft,
                                       LPFILETIME utcft )
{
    struct tm *xtm;
    DWORD remainder;
    time_t utctime;

    /* Converts from local to UTC. */
    time_t localtime = DOSFS_FileTimeToUnixTime( localft, &remainder );
    xtm = gmtime( &localtime );
    utctime = mktime(xtm);
    if(xtm->tm_isdst > 0) utctime-=3600;
    DOSFS_UnixTimeToFileTime( utctime, utcft, remainder );
    return TRUE;
}


/***********************************************************************
 *           FileTimeToLocalFileTime   (KERNEL32.@)
 */
BOOL WINAPI FileTimeToLocalFileTime( const FILETIME *utcft,
                                       LPFILETIME localft )
{
    DWORD remainder;
    /* Converts from UTC to local. */
    time_t unixtime = DOSFS_FileTimeToUnixTime( utcft, &remainder );
#ifdef HAVE_TIMEGM
    struct tm *xtm = localtime( &unixtime );
    time_t localtime;

    localtime = timegm(xtm);
    DOSFS_UnixTimeToFileTime( localtime, localft, remainder );

#else
    struct tm *xtm;
    time_t time;

    xtm = gmtime( &unixtime );
    time = mktime(xtm);
    if(xtm->tm_isdst > 0) time-=3600;
    DOSFS_UnixTimeToFileTime( 2*unixtime-time, localft, remainder );
#endif
    return TRUE;
}


/***********************************************************************
 *           FileTimeToSystemTime   (KERNEL32.@)
 */
BOOL WINAPI FileTimeToSystemTime( const FILETIME *ft, LPSYSTEMTIME syst )
{
   LARGE_INTEGER li;
   TIME_FIELDS tf;

   li.u.LowPart = ft->dwLowDateTime;
   li.u.HighPart = ft->dwHighDateTime;
   RtlTimeToTimeFields (&li, &tf);

   syst->wYear         = tf.Year;
   syst->wMonth        = tf.Month;
   syst->wDayOfWeek    = tf.Weekday;
   syst->wDay          = tf.Day;
   syst->wHour	       = tf.Hour;
   syst->wMinute       = tf.Minute;
   syst->wSecond       = tf.Second;
   syst->wMilliseconds = tf.Milliseconds;

   TRACE ("(ft: %llu)\n", li.QuadPart);
   TRACE ("Returning %hu years, %hu months, %hu days (%hu), %hu:%hu:%hu:%hu\n",
          tf.Year, tf.Month, tf.Day, tf.Weekday, tf.Hour,
          tf.Minute, tf.Second, tf.Milliseconds);
   return TRUE;
}

/***********************************************************************
 *           SystemTimeToFileTime   (KERNEL32.@)
 */
BOOL WINAPI SystemTimeToFileTime( const SYSTEMTIME *syst, LPFILETIME ft )
{
#ifdef HAVE_TIMEGM
    struct tm xtm;
    time_t utctime;
#else
    struct tm xtm,*utc_tm;
    time_t localtim,utctime;
#endif

    xtm.tm_year	= syst->wYear-1900;
    xtm.tm_mon	= syst->wMonth - 1;
    xtm.tm_wday	= syst->wDayOfWeek;
    xtm.tm_mday	= syst->wDay;
    xtm.tm_hour	= syst->wHour;
    xtm.tm_min	= syst->wMinute;
    xtm.tm_sec	= syst->wSecond; /* this is UTC */
    xtm.tm_isdst = -1;
#ifdef HAVE_TIMEGM
    utctime = timegm(&xtm);
    DOSFS_UnixTimeToFileTime( utctime, ft,
			      syst->wMilliseconds * 10000 );
#else
    localtim = mktime(&xtm);    /* now we've got local time */
    utc_tm = gmtime(&localtim);
    utctime = mktime(utc_tm);
    DOSFS_UnixTimeToFileTime( 2*localtim -utctime, ft,
			      syst->wMilliseconds * 10000 );
#endif
    return TRUE;
}

/***********************************************************************
 *           DefineDosDeviceA       (KERNEL32.@)
 */
BOOL WINAPI DefineDosDeviceA(DWORD flags,LPCSTR devname,LPCSTR targetpath) {
	FIXME("(0x%08lx,%s,%s),stub!\n",flags,devname,targetpath);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

/*
   --- 16 bit functions ---
*/

/*************************************************************************
 *           FindFirstFile   (KERNEL.413)
 */
HANDLE16 WINAPI FindFirstFile16( LPCSTR path, WIN32_FIND_DATAA *data )
{
    DOS_FULL_NAME full_name;
    HGLOBAL16 handle;
    FIND_FIRST_INFO *info;

    data->dwReserved0 = data->dwReserved1 = 0x0;
    if (!path) return 0;
    if (!DOSFS_GetFullName( path, FALSE, &full_name ))
        return INVALID_HANDLE_VALUE16;
    if (!(handle = GlobalAlloc16( GMEM_MOVEABLE, sizeof(FIND_FIRST_INFO) )))
        return INVALID_HANDLE_VALUE16;
    info = (FIND_FIRST_INFO *)GlobalLock16( handle );
    info->path = HeapAlloc( GetProcessHeap(), 0, strlen(full_name.long_name)+1 );
    strcpy( info->path, full_name.long_name );
    info->long_mask = strrchr( info->path, '/' );
    if (info->long_mask )
        *(info->long_mask++) = '\0';
    info->short_mask = NULL;
    info->attr = 0xff;
    if (path[0] && (path[1] == ':')) info->drive = FILE_toupper(*path) - 'A';
    else info->drive = DRIVE_GetCurrentDrive();
    info->cur_pos = 0;
#ifdef CACHE_DIR_ENTRY
    info->dir = NULL;
    info->state = RDSTATE_START;
#else
    info->dir = DOSFS_OpenDir (info->path);
#endif

    GlobalUnlock16( handle );
    if (!FindNextFile16( handle, data ))
    {
        FindClose16( handle );
        SetLastError( ERROR_NO_MORE_FILES );
        return INVALID_HANDLE_VALUE16;
    }
    return handle;
}

/*************************************************************************
 *           FindNextFile   (KERNEL.414)
 */
BOOL16 WINAPI FindNextFile16( HANDLE16 handle, WIN32_FIND_DATAA *data )
{
    FIND_FIRST_INFO *info;

    if ((handle == INVALID_HANDLE_VALUE16) ||
       !(info = (FIND_FIRST_INFO *)GlobalLock16( handle )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }
    GlobalUnlock16( handle );

#ifdef CACHE_DIR_ENTRY
    if (!info->path)
#else
    if (!info->path || !info->dir)
#endif
    {
        SetLastError( ERROR_NO_MORE_FILES );
        return FALSE;
    }

    if (!DOSFS_FindNextEx( info, data ))
    {
#ifndef CACHE_DIR_ENTRY
        DOSFS_CloseDir (info->dir);
        info->dir = NULL;
#endif
        HeapFree( GetProcessHeap(), 0, info->path );
        info->path = info->long_mask = NULL;
        SetLastError( ERROR_NO_MORE_FILES );
        return FALSE;
    }
    return TRUE;
}

/*************************************************************************
 *           FindClose   (KERNEL.415)
 */
BOOL16 WINAPI FindClose16( HANDLE16 handle )
{
    FIND_FIRST_INFO *info;

    if ((handle == INVALID_HANDLE_VALUE16) ||
        !(info = (FIND_FIRST_INFO *)GlobalLock16( handle )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }

#ifndef CACHE_DIR_ENTRY
    if (info->dir)
        DOSFS_CloseDir (info->dir);
#endif
    if (info->path) HeapFree( GetProcessHeap(), 0, info->path );
    GlobalUnlock16( handle );
    GlobalFree16( handle );
    return TRUE;
}

