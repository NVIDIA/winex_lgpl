/*
 * DOS drives handling functions
 *
 * Copyright 1993 Erik Bos
 * Copyright 1996 Alexandre Julliard
 *
 * Label & serial number read support.
 *  (c) 1999 Petr Tomasek <tomasek@etf.cuni.cz>
 *  (c) 2000 Andreas Mohr (changes)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"
#include "options.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_UTMP_H
#include <utmp.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef STATFS_DEFINED_BY_SYS_VFS
# include <sys/vfs.h>
#else
# ifdef STATFS_DEFINED_BY_SYS_MOUNT
#  include <sys/mount.h>
# else
#  ifdef STATFS_DEFINED_BY_SYS_STATFS
#   include <sys/statfs.h>
#  endif
# endif
#endif

#include "winbase.h"
#include "winternl.h"
#include "wine/winbase16.h"   /* for GetCurrentTask */
#include "winerror.h"
#include "drive.h"
#include "wine/file.h"
#include "wine/heapstr.h"
#include "msdos.h"
#include "options.h"
#include "task.h"
#include "wine/debug.h"
#include "wine/server.h"
#include "winioctl.h"
#include "ntddstor.h"
#include "ntddcdrm.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define CDPROBE_FSTAB_MAXLEN 1024

WINE_DEFAULT_DEBUG_CHANNEL(dosfs);
WINE_DECLARE_DEBUG_CHANNEL(file);

typedef struct
{
    char     *root;      /* root dir in Unix format without trailing / */
    char     *dos_cwd;   /* cwd in DOS format without leading or trailing \ */
    char     *unix_cwd;  /* cwd in Unix format without leading or trailing / */
    char     *device;    /* raw device path */
    char      label_conf[12]; /* drive label as cfg'd in wine config */
    char      label_read[12]; /* drive label as read from device */
    DWORD     serial_conf;    /* drive serial number as cfg'd in wine config */
    UINT      type;      /* drive type */
    UINT      flags;     /* drive flags */
    dev_t     dev;       /* unix device number */
    ino_t     ino;       /* unix inode number */
    dev_t     dev_dev;   /* unix device number for device */
    dev_t     dev_ino;   /* unix device number for device */
    char     *dos_device_name; /* name for QueryDosDevice */
    char      volume_id[40]; /* GUID */
} DOSDRIVE;


typedef struct {
   int CurDrive;
} FindVolume_t;


static const char * const DRIVE_Types[] =
{
    "",         /* DRIVE_UNKNOWN */
    "",         /* DRIVE_NO_ROOT_DIR */
    "floppy",   /* DRIVE_REMOVABLE */
    "hd",       /* DRIVE_FIXED */
    "network",  /* DRIVE_REMOTE */
    "cdrom",    /* DRIVE_CDROM */
    "ramdisk"   /* DRIVE_RAMDISK */
};


/* Known filesystem types */

typedef struct
{
    const char *name;
    UINT      flags;
} FS_DESCR;

static const FS_DESCR DRIVE_Filesystems[] =
{
    { "unix",   DRIVE_CASE_SENSITIVE | DRIVE_CASE_PRESERVING },
    { "msdos",  DRIVE_SHORT_NAMES },
    { "dos",    DRIVE_SHORT_NAMES },
    { "fat",    DRIVE_SHORT_NAMES },
    { "vfat",   DRIVE_CASE_PRESERVING },
    { "win95",  DRIVE_CASE_PRESERVING },
    { NULL, 0 }
};


static DOSDRIVE DOSDrives[MAX_DOS_DRIVES];
static int DRIVE_CurDrive = -1;

static HTASK16 DRIVE_LastTask = 0;
static int HDCounter = 0;
static int CDCounter = 0;
static int FloppyCounter = 0;

/* strdup on the process heap */
inline static char *heap_strdup( const char *str )
{
    INT len = strlen(str) + 1;
    LPSTR p = HeapAlloc( GetProcessHeap(), 0, len );
    if (p) memcpy( p, str, len );
    return p;
}

static char *heap_strndup (const char *str, const int len )
{
    LPSTR p = HeapAlloc( GetProcessHeap(), 0, len + 1 );
    if (p)
    {
        strncpy( p, str, len );
	p[len] = '\0';
    }
    return p;
}

inline static int DRIVE_isspace(char c)
{
    if (isspace(c)) return 1;
    if (c=='\r' || c==0x1a) return 1;
    /* CR and ^Z (DOS EOF) are spaces too  (found on CD-ROMs) */
    return 0;
}


extern void CDROM_InitRegistry(int dev);

/***********************************************************************
 *           DRIVE_GetDriveType
 */
static UINT DRIVE_GetDriveType( const char *name )
{
    char buffer[20];
    int i;

    PROFILE_GetWineIniString( name, "Type", "hd", buffer, sizeof(buffer) );
    for (i = 0; i < sizeof(DRIVE_Types)/sizeof(DRIVE_Types[0]); i++)
    {
        if (!strcasecmp( buffer, DRIVE_Types[i] )) return i;
    }
    MESSAGE("%s: unknown drive type '%s', defaulting to 'hd'.\n",
	name, buffer );
    return DRIVE_FIXED;
}


/***********************************************************************
 *           DRIVE_GetFSFlags
 */
static UINT DRIVE_GetFSFlags( const char *name, const char *value )
{
    const FS_DESCR *descr;

    for (descr = DRIVE_Filesystems; descr->name; descr++)
        if (!strcasecmp( value, descr->name )) return descr->flags;
    MESSAGE("%s: unknown filesystem type '%s', defaulting to 'win95'.\n",
	name, value );
    return DRIVE_CASE_PRESERVING;
}

/***********************************************************************
 *          DRIVE_IsDevPathCD
 *          looks through all the current WINE cd drives to see if the
 *          device path specified by char *dev_path is there.
 *          **HELPER function to DRIVE_ProbeCD()
 */

static BOOL DRIVE_IsDevPathCD( const char *dev_path )
{
	int i;
	const DOSDRIVE *drive;

	for (i = 0, drive = DOSDrives; i < MAX_DOS_DRIVES; i++, drive++)
	{
		if (drive->type != DRIVE_CDROM || !drive->device)
			continue;
		if (strcmp(drive->device, dev_path) == 0)
			return TRUE;
	}
	return FALSE;
}

inline static void DRIVE_RegValueStr(HKEY hkey, const char *valueName, const char *dataStr)
{
	RegSetValueExA(hkey, valueName, 0, REG_SZ, (LPBYTE) dataStr, strlen (dataStr) + 1);
}

static void DRIVE_AddAdapterReg ()
{
	HKEY hkey;
	TRACE("Added Primary CD Adapter reg keys.\n");
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Enum", 0, NULL,
		0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Enum\\Root", 0, NULL,
		0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Enum\\Root\\Adapter", 0, NULL,
		0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Enum\\Root\\Adapter\\0000", 0, NULL,
		0, KEY_ALL_ACCESS, NULL, &hkey, NULL);

	DRIVE_RegValueStr(hkey, "Class", "Adapter");
	DRIVE_RegValueStr(hkey, "DeviceDesc", "CD-ROM Adapter");
	DRIVE_RegValueStr(hkey, "Driver", "Adapter\\0000");
	DRIVE_RegValueStr(hkey, "Mfg", "TransGaming");
}

static void DRIVE_AddCDReg ( char letter )
{
	char regentry[MAX_PATH];
	static int calls = 0;
	DWORD regentry_dw;
	HKEY hkey;

	TRACE("adding entries into reg for CD-ROMs. Drive %c.\n", letter);
	if (!calls)
	{
		DRIVE_AddAdapterReg();
	}
	calls++;
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Enum\\SCSI", 0, NULL, 0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	sprintf(regentry, "Enum\\SCSI\\CDROM_%i", calls);
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, regentry, 0, NULL, 0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	sprintf(regentry, "Enum\\SCSI\\CDROM_%i\\ROOT&ADAPTER&000000", calls);
	RegCreateKeyExA( HKEY_LOCAL_MACHINE, regentry, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);

	DRIVE_RegValueStr(hkey, "Class", "CDROM");
	regentry[0] = letter; regentry[1] = '\0';
	DRIVE_RegValueStr(hkey, "CurrentDriveLetterAssignment", regentry);
	DRIVE_RegValueStr(hkey, "DeviceDesc", "TransGaming CDROM");
	regentry_dw = DRIVE_CDROM;
	RegSetValueExA(hkey, "DeviceType", 0, REG_DWORD, (LPBYTE) &regentry_dw, sizeof(DWORD));
	DRIVE_RegValueStr(hkey, "Driver", "CDROM\\0000");
	sprintf(regentry, "CDROM_%i,GenCD,SCSI\\CDROM_%i", calls, calls);
	DRIVE_RegValueStr(hkey, "HardwareID", regentry);
	DRIVE_RegValueStr(hkey, "Manufacturer", "TransGaming");
	DRIVE_RegValueStr(hkey, "Mfg", "(Standard CD-ROM Device)");
	DRIVE_RegValueStr(hkey, "ProductId", "CDROM");
	regentry_dw = 1;
	RegSetValueExA(hkey, "Removable", 0, REG_DWORD, (LPBYTE) &regentry_dw, sizeof(DWORD));
	DRIVE_RegValueStr(hkey, "RevisionLevel", "1.0");
	DRIVE_RegValueStr(hkey, "SCSILun", "0");
	DRIVE_RegValueStr(hkey, "SCSITargetID", "0");

	RegCreateKeyExA( HKEY_DYN_DATA, "Config Manager", 0, NULL, 0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	RegCreateKeyExA( HKEY_DYN_DATA, "Config Manager\\ENUM", 0, NULL, 0, KEY_ALL_ACCESS, NULL, NULL, NULL);
	sprintf(regentry, "Config Manager\\ENUM\\A%07i", calls);
	RegCreateKeyExA( HKEY_DYN_DATA, regentry, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);
	sprintf(regentry, "SCSI\\CDROM_%i\\ROOT&ADAPTER&000000", calls);
	DRIVE_RegValueStr(hkey, "HardWareKey", regentry);
}

/***********************************************************************
 * 	   DRIVE_AddCDDrive
 * 	   Adds a CD drive (autodetected from fstab) to the drive list
 * 	   **HELPER function to DRIVE_ProbeCD()
 */

static void DRIVE_AddCDDrive( char *dev_path, char *mount_path )
{
	int i;
	DOSDRIVE *drive;
	char name[] = "Drive D";
	BOOL found = FALSE;
	struct stat drive_stat_buffer;
 	char dos_dev_name[32];

	for (i = 3, drive = DOSDrives+3; i < MAX_DOS_DRIVES && !found; i++, name[6]++, drive++)
	{
		if (!drive->root) /* its free!, lets use it! */
		{
			found = TRUE;
			break;
		}
	}
	if (!found)
	{
		WARN("Warning. Found CD-ROM drive in /etc/fstab (%s) but ran out of drive letters. Can't add.\n", mount_path);
		return;
	}
	drive->root = heap_strdup(mount_path);
	stat( drive->root, &drive_stat_buffer );
	drive->unix_cwd = heap_strdup( "" );
	drive->dos_cwd = heap_strdup( "" );
	drive->device = heap_strdup(dev_path);
	strcpy( drive->label_conf, "Auto-CD     " );
	drive->serial_conf = strtoul("12345678", NULL, 16); /*default*/
	drive->type = DRIVE_CDROM;
	drive->flags = DRIVE_GetFSFlags( name, "win95" );
	drive->flags |= DRIVE_READ_VOL_INFO;
	drive->dev = drive_stat_buffer.st_dev;
	drive->ino = drive_stat_buffer.st_ino;
	stat( drive->device, &drive_stat_buffer );
	drive->dev_dev = drive_stat_buffer.st_dev;
	drive->dev_ino = drive_stat_buffer.st_ino;
	sprintf (dos_dev_name, "\\Device\\CdRom%d", CDCounter++);
	drive->dos_device_name = heap_strdup (dos_dev_name);

	DRIVE_AddCDReg(name[6]);       

	TRACE("Found from fstab - %s: path=%s type=%s label='%s' serial=%08lx"
			" flags=%08x dev=%x ino=%x dev_dev=%x dev_ino=%x\n",
			name, drive->root, DRIVE_Types[drive->type],
		        drive->label_conf, drive->serial_conf, drive->flags,
			(int)drive->dev, (int)drive->ino,
			(int)drive->dev_dev, (int)drive->dev_ino );
}

/***********************************************************************
 *          DRIVE_UpdateCDMountpoints
 *          searches /proc/mounts for CD-Rom drives with different
 *          mount points from what was indicated in /etc/fstab,
 *          updating them as appropriate.
 *          Required for SuSE 9.3+ which uses D-Bus/HAL to generate
 *          dynamic mount point locations based on the disc's label.
 */
void DRIVE_UpdateCDMountpoints()
{
	FILE *mtab_file = NULL;
	char buffer[CDPROBE_FSTAB_MAXLEN];
	char *p, *p_tmp;
	char *device_path, *mount_path, *mount_type;
	int i;
	DOSDRIVE *drive;
	size_t len;
	struct stat drive_stat_buffer;
	
	mtab_file = fopen ("/proc/mounts", "r");
	if (!mtab_file)
	{
		WARN("Can't open %s. Unable to check if media mount point location changed\n", "/proc/mounts");
		return;
	}
	
	/* iterate through mtab */
	while (fgets (buffer, CDPROBE_FSTAB_MAXLEN, mtab_file))
	{
		p = buffer;
		
		/* skip everything that obviously isn't a /dev device */
		while (*p && DRIVE_isspace(*p)) p++;
		if (*p != '/')
			continue;
		
		/* copy the device location */
		p_tmp = p;
		while (*p && !DRIVE_isspace(*p)) p++;
		device_path = heap_strndup (p_tmp, p - p_tmp);
		
		while (*p && DRIVE_isspace(*p)) p++;
		
		/* copy the mount point location */
		if (*p != '/')
		{
			HeapFree( GetProcessHeap(), 0, device_path );
			continue; /* we want mount points starting with / */
		}
		p_tmp = p;
		/* we dont want trailing /'s on our paths */
		while (*p && !DRIVE_isspace(*p)) p++;
		mount_path = heap_strndup (p_tmp, p - p_tmp);
		len = strlen (mount_path);
		if (mount_path[len] == '/')
		{
			mount_path[len] = '\0';
			len--;
		}

                /* Handle octal escapes in mount path - newer linux distros
                   can use these to escape spaces in the mount point */
                for (i = 0; (len > 3) && (i < (len - 3)); i++)
                {
                   int value;

                   if (mount_path[i] != '\\')
                      continue;

                   /* Check for valid escape */
                   if ((i >= (len - 3)) || !isdigit (mount_path[i + 1]) ||
                       !isdigit (mount_path[i + 2]) ||
                       !isdigit (mount_path[i + 3]) ||
                       (mount_path[i + 1] > '3'))
                      continue;

                   /* Convert octal to an integer */
                   value = mount_path[i + 1] - '0';
                   value = (value << 3) | (mount_path[i + 2] - '0');
                   value = (value << 3) | (mount_path[i + 3] - '0');

                   /* replace \\ with the value, move rest of string down
                      by 3 */
                   mount_path[i] = (char)value;
                   memmove (&mount_path[i + 1], &mount_path[i + 4],
                            len - i - 2);
                   len -= 3;
                }
		
		while (*p && DRIVE_isspace(*p)) p++;
		
		/* mount type */
		p_tmp = p;
		while (*p && !DRIVE_isspace(*p)) p++;
		mount_type = heap_strndup (p_tmp, p - p_tmp);
		
		/* so if it isn't an optical format, move along */
		if (strcmp(mount_type, "subfs") && 
		    !strstr(mount_type, "udf") && !strstr(mount_type, "iso9660"))
		{
			HeapFree( GetProcessHeap(), 0, device_path );
			HeapFree( GetProcessHeap(), 0, mount_path );
			HeapFree( GetProcessHeap(), 0, mount_type );
			continue;
		}
		
		/* find the cdrom in our DOSDrives list (if we can) */
		stat( device_path, &drive_stat_buffer );
		for (i = 3, drive = DOSDrives+3; i < MAX_DOS_DRIVES; i++, drive++)
			if ((drive->type == DRIVE_CDROM) && 
			    ((drive_stat_buffer.st_dev == drive->dev_dev) &&
			     (drive_stat_buffer.st_ino == drive->dev_ino)) &&
			     strcmp(drive->root, mount_path))
			{
				/* change mount point info in DOSDrives */
				TRACE("Mount point for %c: (%s) changed from %s to %s\n", 
				      'A'+i, device_path, drive->root, mount_path);
				HeapFree( GetProcessHeap(), 0, drive->root );
				drive->root = heap_strdup( mount_path );
				stat( drive->root, &drive_stat_buffer );
				drive->dev = drive_stat_buffer.st_dev;
				drive->ino = drive_stat_buffer.st_ino;
			}
		
		/* no memory leaks please.. */
		HeapFree( GetProcessHeap(), 0, device_path );
		HeapFree( GetProcessHeap(), 0, mount_path );
		HeapFree( GetProcessHeap(), 0, mount_type );
	}
	fclose(mtab_file);
}

/***********************************************************************
 *          DRIVE_ProbeCD
 *          searches /etc/fstab for any CD-Rom drives not in the wine
 *          config and adds them to spare drive spaces after 'Drive C'
 */

static void DRIVE_ProbeCD()
{
	FILE *fstab_file = NULL;
	char buffer[CDPROBE_FSTAB_MAXLEN];
	char *p, *p_tmp;
	char *device_path, *mount_path, *mount_type, *options_dev;
	BOOL drivecd;

	fstab_file = fopen ("/etc/fstab", "r");
	if (!fstab_file)
	{
		WARN("Can't open %s. I'm not probing for CD-ROM drives that arn't in your config!\n", "/etc/fstab");
		return;
	}
	/* FIXME: Do something if fstab is corrupt!
         * FIXME: this code really doesnt seem to care about str length, oh well. */
	while (fgets (buffer, CDPROBE_FSTAB_MAXLEN, fstab_file))
	{
		drivecd = FALSE;
		p = buffer;
		while (*p && DRIVE_isspace(*p)) p++;
                /* agh. ugliness because of mandrake 9.0's stupid new fstab (device = 'none') */
		if (*p != '/' && strncmp(p, "none", 4) != 0)
                        continue; /* we are only interested in devices starting with / */
		/* device path */
		p_tmp = p;
		while (*p && !DRIVE_isspace(*p)) p++;
		device_path = heap_strndup (p_tmp, p - p_tmp);

		while (*p && DRIVE_isspace(*p)) p++;
		if (*p != '/')
		{
			HeapFree( GetProcessHeap(), 0, device_path );
			continue; /* we want mount points starting with / */
		}

		/* mount path */
		p_tmp = p;
		/* we dont want trailing /'s on our paths */
		while (*p && !DRIVE_isspace(*p)) p++;
		mount_path = heap_strndup (p_tmp, p - p_tmp);
		if (mount_path[strlen(mount_path)] == '/')
			mount_path[strlen(mount_path)] = '\0';

		while (*p && DRIVE_isspace(*p)) p++;
		/* mount type */
		p_tmp = p;
		while (*p && !DRIVE_isspace(*p)) p++;
		mount_type = heap_strndup (p_tmp, p - p_tmp);
		/* this is required for supermount when the device is specified in the options
		   section, so thus we must get this information aswell */
		while (*p && DRIVE_isspace(*p)) p++;
		/* this isn't very nice, but basically we want to get to the start of the
		   dev= part of the options in the fstab, if it exists. Used by supermount in
		   mandrake */
		while (*p && !((*p == '=') && (*(p-1) == 'v') && (*(p-2) == 'e') && (*(p-3) == 'd'))
				&& !DRIVE_isspace(*p))
			p++;
		if (!DRIVE_isspace(*p))
		{
			p++;
			p_tmp = p;
			while (*p && !DRIVE_isspace(*p) && *p != ',') p++;
			options_dev = heap_strndup (p_tmp, p - p_tmp);
		}
		else
		{
			options_dev = NULL;
		}

		/* now that we have the info on the mount thing - its time to see if its a CD-ROM */
		/* cd drives have to be iso9660 or auto. if its iso9660 it _is_ cd */
		if (strstr(mount_type, "iso9660"))
			drivecd = TRUE;
		else if (!strcmp(mount_type, "auto"))
			drivecd = TRUE;
		else if (!strcmp(mount_type, "supermount"))
			drivecd = TRUE;
		else if (strstr(mount_type, "udf"))
			drivecd = TRUE;
		else if (!strcmp(mount_type, "subfs"))
			drivecd = TRUE;
		else if (strstr(device_path, "cdr") != NULL)
			drivecd = TRUE;
		else if (strstr(device_path, "scd") != NULL)
			drivecd = TRUE;
		else if (strstr(device_path, "dvd") != NULL)
			drivecd = TRUE;
		else if (strstr(mount_path, "cdr") != NULL)
			drivecd = TRUE;
		else if (strstr(mount_path, "dvd") != NULL)
			drivecd = TRUE;

		/* Disk drives should not be mapped as a cdrom. Let's
		 * make sure that this isn't a floppy drive... */
		if (strstr(device_path, "fd") != NULL)
		{
			drivecd = FALSE;
		}

		if (drivecd)
		{
			if (!strcmp(device_path, mount_path) && !strcmp(mount_type, "supermount") &&
				options_dev)
			/* if its supermount and the device path is the same as the mount path.
			   This causes problems with direct access to the device (Eg, it doesnt work) */
			{
				HeapFree( GetProcessHeap(), 0, device_path );
				device_path = heap_strdup ( options_dev );
			}
			TRACE("found CD-ROM in fstab. dev: '%s' mount: '%s' type: '%s'.\n",
				device_path, mount_path, mount_type);

			if (!DRIVE_IsDevPathCD( (options_dev != NULL) ? options_dev : device_path ))
			{
				/* its not there, lets add it! */
                                DRIVE_AddCDDrive( (options_dev != NULL) ? options_dev : device_path,
                                                mount_path );
			}

		}
		else
			TRACE( "I'm thinking that fs %s -> %s @ %s is not a CD-ROM drive.\n", mount_type, device_path, mount_path );

		HeapFree( GetProcessHeap(), 0, device_path );
		HeapFree( GetProcessHeap(), 0, mount_path );
		HeapFree( GetProcessHeap(), 0, mount_type );
		HeapFree( GetProcessHeap(), 0, options_dev );
	}
	fclose (fstab_file);
	DRIVE_UpdateCDMountpoints();
}
/***********************************************************************
 *          DRIVE_CheckForDynamicPath 
 */
static int DRIVE_CheckForDynamicPath( char* path, size_t path_size )
{
   if (strstr(path, "@WORKINGFOLDER@"))
      return (getcwd (path, path_size) != NULL);
   else if (strstr (path, "@USERNAME@"))
   {
#if defined(HAVE_UTMP_H) && defined(HAVE_GETLOGIN_R)
      char userBuf[UT_NAMESIZE + 1];
#endif
      char *pUserName= NULL;
      char *pNewPath;
      char *tmp_str;
      int length;
      
      /* get the username */
      pUserName = getenv ("LOGNAME");
#if defined(HAVE_UTMP_H) && defined(HAVE_GETLOGIN_R)
      if (!pUserName && 
          (getlogin_r (userBuf, sizeof (userBuf)) == 0))
         pUserName = userBuf;
#endif

      pNewPath = HeapAlloc (GetProcessHeap (), 0,
                            sizeof (char) * path_size);
      if (!pNewPath) {
         ERR("out of memory!\n");
         return 0;
      }

      /* we need to replace the part of the string between (and including)
       * the @ symbols with pUserName */
      strncpy(pNewPath, path, path_size);
      pNewPath[path_size - 1] = '\0';

      tmp_str = strchr(pNewPath, '@');
      tmp_str[0] = '\0';
      length = strlen(pNewPath);

      tmp_str = strrchr (path, '@') + 1;

      snprintf (&pNewPath[length], path_size-length, "%s%s", pUserName,
                tmp_str);

      strncpy (path, pNewPath, path_size);
      path[path_size - 1] = '\0';
      HeapFree (GetProcessHeap (), 0, pNewPath);

      /* Create it if it doesn't exist; ignore failure (as it may well exist) */
      mkdir (path, S_IRWXU);

   }
   else if (strstr (path, "@USERPREFS@"))
   {
      char *pNewPath;
      char *pEnvPtr;
      const char *pRestOfPath;

      /* On the Apple, this contains the path to the user's individual
         Cider game directory:
         /Users/foo/Library/Preferences/Cider <game> Preferences/ */
      pEnvPtr = getenv ("WINEPREFIX");
      if (!pEnvPtr)
         return 0;

      pNewPath = HeapAlloc (GetProcessHeap (), 0,
                            sizeof (char) * path_size);
      if (!pNewPath)
         return 0;

      pRestOfPath = strrchr (path, '@') + 1;

      snprintf (pNewPath, path_size, "%s%s", pEnvPtr,
                pRestOfPath);

      strncpy (path, pNewPath, path_size);
      path[path_size - 1] = '\0';
      HeapFree (GetProcessHeap (), 0, pNewPath);
   }
#ifdef __APPLE__
   else 
   {
      CFURLRef bundle_url = NULL;
      CFBundleRef mainbundle = CFBundleGetMainBundle();
      if (strstr(path,"@BUNDLEPATH@"))
      {
         CFURLRef bundle_url = CFBundleCopyBundleURL(mainbundle);
         if (!CFURLGetFileSystemRepresentation(bundle_url, true, path,
                                               path_size))
         {
            CFRelease (bundle_url);
            return 0;
         }
      }
      else if (strstr(path,"@BUNDLEPATHRESOURCE@"))
      {
         int path_space_remaining = path_size;
         char* temp_string;
         char* saved_path = HeapAlloc (GetProcessHeap (), 0,
                                       sizeof (char) * path_size);
         if (!saved_path)
            return 0;

         strncpy(saved_path, path, path_size);
         saved_path[path_size - 1] = '\0';

         CFURLRef bundle_url = CFBundleCopyResourcesDirectoryURL(mainbundle);
         if (!CFURLGetFileSystemRepresentation(bundle_url, true, path,
                                               path_size))
         {
            CFRelease (bundle_url);
            strcpy (path, saved_path);
            HeapFree (GetProcessHeap (), 0, saved_path);
            return 0;
         }

         path_space_remaining -= strlen(path);

         /* now let's see if we have a path on the end of the string */
         temp_string = strrchr(saved_path, '@');
         if ((temp_string - saved_path + 1) < strlen(saved_path))
         {
            temp_string++;
            strncat(path, temp_string, path_space_remaining);
            path[path_size - 1] = '\0';
         }

         HeapFree (GetProcessHeap (), 0, saved_path);
      }
      else if (strstr(path, "@BUNDLEPATHPARENT@"))
      {
         int path_space_remaining = path_size;
         char* temp_string;
         char* saved_path = HeapAlloc (GetProcessHeap (), 0,
                                       sizeof (char) * path_size);
         if (!saved_path)
            return 0;

         strncpy(saved_path, path, path_size);
         saved_path[path_size - 1] = '\0';

         CFURLRef bundle_url = CFBundleCopyBundleURL(mainbundle);
         if (!CFURLGetFileSystemRepresentation(bundle_url, true, path,
                                               path_size))
         {
            CFRelease (bundle_url);
            strcpy (path, saved_path);
            HeapFree (GetProcessHeap (), 0, saved_path);
            return 0;
         }

         temp_string = strrchr(path, '/');
         if ((temp_string - path + 1) == strlen(path))
         {
            temp_string[0] = '\0';
            temp_string = strrchr(path, '/');
         }
         temp_string[0] = '\0';

         path_space_remaining -= strlen(path);

         /* now let's see if we have a path on the end of the string */
         temp_string = strrchr(saved_path, '@');
         if ((temp_string - saved_path + 1) < strlen(saved_path))
         {
            temp_string++;
            strncat(path, temp_string, path_space_remaining);
            path[path_size - 1] = '\0';
         }
         HeapFree (GetProcessHeap (), 0, saved_path);
      }
      if (bundle_url) CFRelease(bundle_url);
   }
#endif  /* __APPLE__ */
   return 1;
}

/***********************************************************************
 *           DRIVE_Init
 */
int DRIVE_Init(void)
{
    int i, len, count = 0;
    char name[] = "Drive A";
    char drive_env[] = "=A:";
    char path[MAX_PATHNAME_LEN];
    char buffer[80];
    struct stat drive_stat_buffer;
    char *p;
    DOSDRIVE *drive;

    for (i = 0, drive = DOSDrives; i < MAX_DOS_DRIVES; i++, name[6]++, drive++)
    {
        PROFILE_GetWineIniString( name, "Path", "", path, sizeof(path)-1 );
        if (path[0])
        {
            if (!DRIVE_CheckForDynamicPath (path, sizeof (path)))
            {
               MESSAGE ("Unable to retrieve dynamic path portion of drive %c\n",
                        'A' + i);
               continue;
            }

            p = path + strlen(path) - 1;
            while ((p > path) && (*p == '/')) *p-- = '\0';

            if (path[0] == '/')
            {
                drive->root = heap_strdup( path );
            }
            else
            {
                /* relative paths are relative to config dir */
                const char *config = get_config_dir();
                drive->root = HeapAlloc( GetProcessHeap(), 0, strlen(config) + strlen(path) + 2 );
                sprintf( drive->root, "%s/%s", config, path );
            }

            if (stat( drive->root, &drive_stat_buffer ))
            {
                MESSAGE("Could not stat %s (%s), ignoring drive %c:\n",
                        drive->root, strerror(errno), 'A' + i);
                HeapFree( GetProcessHeap(), 0, drive->root );
                drive->root = NULL;
                continue;
            }
            if (!S_ISDIR(drive_stat_buffer.st_mode))
            {
                MESSAGE("%s is not a directory, ignoring drive %c:\n",
                        drive->root, 'A' + i );
                HeapFree( GetProcessHeap(), 0, drive->root );
                drive->root = NULL;
                continue;
            }
        } else {
            /* If it's an optical drive without a defined mount point,
             * give it a fake directory for now; the proper one will be
             * detected later by dbus, when a disc is inserted. */
            if (DRIVE_GetDriveType( name ) == DRIVE_CDROM)
            {
                drive->root = heap_strdup( "/media/detect_later" );
                drive_stat_buffer.st_mode = 0;
                drive_stat_buffer.st_dev = 0;
                drive_stat_buffer.st_ino = 0;
            }
        }

        if (drive->root)
        {
            char dos_dev_name[32];

            drive->dos_cwd  = heap_strdup( "" );
            drive->unix_cwd = heap_strdup( "" );
            drive->type     = DRIVE_GetDriveType( name );
            drive->device   = NULL;
            drive->flags    = 0;
            drive->dev      = drive_stat_buffer.st_dev;
            drive->ino      = drive_stat_buffer.st_ino;
            drive->dev_dev  = 0; /* only needed for cdrom */
            drive->dev_ino  = 0; /* only needed for cdrom */

            if (drive->type == DRIVE_FIXED)
               sprintf (dos_dev_name, "\\Device\\HarddiskVolume%d",
                        HDCounter++);
            else if (drive->type == DRIVE_REMOVABLE)
               sprintf (dos_dev_name, "\\Device\\Floppy%d", FloppyCounter++);
            else if (drive->type == DRIVE_CDROM)
               sprintf (dos_dev_name, "\\Device\\CdRom%d", CDCounter++);
            else
            {
               WARN ("Unable to create dos device name for drive '%s'\n",
                     name);
               dos_dev_name[0] = '\0';
            }
            drive->dos_device_name = heap_strdup (dos_dev_name);

            /* Get the drive label */
            PROFILE_GetWineIniString( name, "Label", "", drive->label_conf, 12 );
            if ((len = strlen(drive->label_conf)) < 11)
            {
                /* Pad label with spaces */
                memset( drive->label_conf + len, ' ', 11 - len );
                drive->label_conf[11] = '\0';
            }

            /* Get the serial number */
            PROFILE_GetWineIniString( name, "Serial", "12345678",
                                      buffer, sizeof(buffer) );
            drive->serial_conf = strtoul( buffer, NULL, 16 );

            /* Get the filesystem type */
            PROFILE_GetWineIniString( name, "Filesystem", "win95",
                                      buffer, sizeof(buffer) );
            drive->flags = DRIVE_GetFSFlags( name, buffer );

            /* Get the device */
            PROFILE_GetWineIniString( name, "Device", "",
                                      buffer, sizeof(buffer) );
            if (buffer[0])
	    {
		int cd_fd;
                drive->device = heap_strdup( buffer );
		if (PROFILE_GetWineIniBool( name, "ReadVolInfo", 1))
                    drive->flags |= DRIVE_READ_VOL_INFO;
                if (drive->type == DRIVE_CDROM)
                {
                    stat( drive->device, &drive_stat_buffer );
                    drive->dev_dev = drive_stat_buffer.st_dev;
                    drive->dev_ino = drive_stat_buffer.st_ino;

                    if ((cd_fd = open(buffer,O_RDONLY|O_NONBLOCK)) != -1) {
                        CDROM_InitRegistry(cd_fd);
                        close(cd_fd);
                    }
                }
	    }
#ifdef __APPLE__
            /* We don't give device path on MacOS X */
            else if (drive->type == DRIVE_CDROM)
            {
               CDROM_InitRegistry (0);
               DRIVE_AddCDReg (name[6]);
               drive->flags |= DRIVE_READ_VOL_INFO;
            }
#endif

            /* Make the first hard disk the current drive */
            if ((DRIVE_CurDrive == -1) && (drive->type == DRIVE_FIXED))
                DRIVE_CurDrive = i;

	    /* if its a CD-ROM, add the reg entry */
	    if (drive->type == DRIVE_CDROM)
		DRIVE_AddCDReg(name[6]);

            count++;
            TRACE("%s: path=%s type=%s label='%s' serial=%08lx "
                  "flags=%08x dev=%x ino=%x\n",
                  name, drive->root, DRIVE_Types[drive->type],
                  drive->label_conf, drive->serial_conf, drive->flags,
                  (int)drive->dev, (int)drive->ino );
        }
        else WARN("%s: not defined\n", name );
    }

    if (!count)
    {
        MESSAGE("Warning: no valid DOS drive found, check your configuration file.\n" );
        /* Create a C drive pointing to Unix root dir */
        DOSDrives[2].root     = heap_strdup( "/" );
        DOSDrives[2].dos_cwd  = heap_strdup( "" );
        DOSDrives[2].unix_cwd = heap_strdup( "" );
        strcpy( DOSDrives[2].label_conf, "Drive C    " );
        DOSDrives[2].serial_conf   = 12345678;
        DOSDrives[2].type     = DRIVE_FIXED;
        DOSDrives[2].device   = NULL;
        DOSDrives[2].flags    = 0;
        DOSDrives[2].dos_device_name = heap_strdup ("\\Device\\HarddiskVolume0");
        HDCounter++;
        DRIVE_CurDrive = 2;
    }

    /* Make sure the current drive is valid */
    if (DRIVE_CurDrive == -1)
    {
        for (i = 0, drive = DOSDrives; i < MAX_DOS_DRIVES; i++, drive++)
        {
            if (drive->root && !(drive->flags & DRIVE_DISABLED))
            {
                DRIVE_CurDrive = i;
                break;
            }
        }
    }
    DRIVE_ProbeCD ();

    /* get current working directory info for all drives */
    for (i = 0; i < MAX_DOS_DRIVES; i++, drive_env[1]++)
    {
        if (!GetEnvironmentVariableA(drive_env, path, sizeof(path))) continue;
        /* sanity check */
        if (toupper(path[0]) != drive_env[1] || path[1] != ':') continue;
        DRIVE_Chdir( i, path + 2 );
    }
    return 1;
}

/***********************************************************************
 *           DRIVE_ServerInit
 */
void DRIVE_ServerInit( void )
{
    int i;
    DOSDRIVE *drive;
    char dummy_buffer[10];

    for (i = 0, drive = DOSDrives; i < MAX_DOS_DRIVES; i++, drive++)
    {
        if (drive->root && !(drive->flags & DRIVE_DISABLED) && 	
           (drive->type == DRIVE_CDROM) && drive->device)
        {
            /* Tell the WineServer about the CD drive */
            SERVER_START_REQ( add_cdrom_device_info )
            {
                req->cd_drive = 'A' + i;
                wine_server_add_data(req, drive->device, strlen(drive->device));
                wine_server_call( req );
            }
            SERVER_END_REQ;
        }
    }

    if (Options.monitor_cdrom_eject)
    {
        /* Tell the WineServer to begin monitoring drives */
        SERVER_START_REQ( add_cdrom_device_info )
        {
            req->cd_drive = 0;
            wine_server_add_data(req, dummy_buffer, 0);
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }
}


/***********************************************************************
 *           DRIVE_InitializeRegistryEntries
 *
 * - can't be done as part of DRIVE_Init(), as the registry won't have
 * been set up yet
 */
void DRIVE_InitializeRegistryEntries ()
{
   HKEY hKey;
   int i;
   unsigned int CurrentId = 1;
   char VolName[64];

   /* This is actually supposed to persist; however, it is easier to just
      regenerate on startup instead of trying to figure out what's
      changed... */
   RegCreateKeyExA (HKEY_LOCAL_MACHINE, "System\\MountedDevices", 0, NULL,
                    REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
   for (i = 0; i < MAX_DOS_DRIVES; i++)
   {
      if (!DRIVE_IsValid (i))
         continue;

      sprintf (DOSDrives[i].volume_id,
               "{5f11fe%02x-e455-11da-af98-806d6172696f}", CurrentId);
      sprintf (VolName, "\\??\\Volume%s", DOSDrives[i].volume_id);
      RegSetValueExA (hKey, VolName, 0, REG_BINARY, (LPVOID)&CurrentId, 4);

      sprintf (VolName, "\\DosDevices\\%c:", 'A' + i);
      RegSetValueExA (hKey, VolName, 0, REG_BINARY, (LPVOID)&CurrentId, 4);

      CurrentId++;
   }

   RegCloseKey (hKey);
}


/***********************************************************************
 *           FindFirstVolumeA   (KERNEL32.@)
 */
HANDLE WINAPI FindFirstVolumeA (LPSTR lpszVolumeName, DWORD cchBufferLength)
{
   int i;
   FindVolume_t *hFindVol;

   TRACE ("(%p, %lu)\n", lpszVolumeName, cchBufferLength);

   /* XP SP 2 crashes with NULL first param; I guess we shouldn't
      check it either */

   /* Find first valid drive and return its info */
   for (i = 0; i < MAX_DOS_DRIVES; i++)
   {
      if (!DRIVE_IsValid (i))
         continue;

      if (cchBufferLength < (strlen (DOSDrives[i].volume_id) + 12))
      {
         SetLastError (ERROR_FILENAME_EXCED_RANGE);
         return INVALID_HANDLE_VALUE;
      }

      sprintf (lpszVolumeName, "\\\\?\\Volume%s\\",
               DOSDrives[i].volume_id);

      hFindVol = HeapAlloc (GetProcessHeap (), 0, sizeof (*hFindVol));
      if (!hFindVol)
      {
         ERR ("Unable to allocate memory for hFindVol!\n");
         return INVALID_HANDLE_VALUE;
      }

      hFindVol->CurDrive = i;
      TRACE ("=> '%s'\n", lpszVolumeName);

      return (HANDLE)hFindVol;
   }

   /* No valid drives found! */
   SetLastError (ERROR_NO_MORE_FILES);
   return INVALID_HANDLE_VALUE;
}


/***********************************************************************
 *           FindFirstVolumeW   (KERNEL32.@)
 */
HANDLE WINAPI FindFirstVolumeW (LPWSTR lpszVolumeName, DWORD cchBufferLength)
{
   char *pCStr;
   HANDLE Ret;

   TRACE ("(%p, %lu)\n", lpszVolumeName, cchBufferLength);

   if (!cchBufferLength)
   {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return INVALID_HANDLE_VALUE;
   }

   pCStr = HeapAlloc (GetProcessHeap (), 0, cchBufferLength);
   if (!pCStr)
   {
      ERR ("Unable to allocate temp string\n");
      return INVALID_HANDLE_VALUE;
   }

   Ret = FindFirstVolumeA (pCStr, cchBufferLength);
   if (Ret != INVALID_HANDLE_VALUE)
   {
      if (!MultiByteToWideChar (CP_ACP, 0, pCStr, -1, lpszVolumeName,
                                cchBufferLength))
         Ret = INVALID_HANDLE_VALUE;
   }

   HeapFree (GetProcessHeap (), 0, pCStr);
   return Ret;
}


/***********************************************************************
 *           FindNextVolumeA   (KERNEL32.@)
 */
BOOL WINAPI FindNextVolumeA (HANDLE hFindVolume, LPSTR lpszVolumeName,
                             DWORD cchBufferLength)
{
   int i;
   FindVolume_t *pFindVol = (FindVolume_t *)hFindVolume;

   TRACE ("(0x%lx, %p, %lu)\n", (unsigned long)hFindVolume, lpszVolumeName,
          cchBufferLength);

   /* XP SP 2 crashes with either ; I guess we shouldn't
      check it either */

   /* Find first valid drive and return its info */
   for (i = pFindVol->CurDrive + 1; i < MAX_DOS_DRIVES; i++)
   {
      if (!DRIVE_IsValid (i))
         continue;

      if (cchBufferLength < (strlen (DOSDrives[i].volume_id) + 12))
      {
         SetLastError (ERROR_FILENAME_EXCED_RANGE);
         return FALSE;
      }

      sprintf (lpszVolumeName, "\\\\?\\Volume%s\\",
               DOSDrives[i].volume_id);
      TRACE ("=> '%s'\n", lpszVolumeName);
      pFindVol->CurDrive = i;
      return TRUE;
   }

   /* No valid drives found! */
   SetLastError (ERROR_NO_MORE_FILES);
   return FALSE;
}


/***********************************************************************
 *           FindNextVolumeW   (KERNEL32.@)
 */
BOOL WINAPI FindNextVolumeW (HANDLE hFindVolume, LPWSTR lpszVolumeName,
                             DWORD cchBufferLength)
{
   char *pCStr;
   BOOL Ret;

   TRACE ("(0x%lx, %p, %lu)\n", (unsigned long)hFindVolume, lpszVolumeName,
          cchBufferLength);

   if (!cchBufferLength)
   {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return FALSE;
   }

   pCStr = HeapAlloc (GetProcessHeap (), 0, cchBufferLength);
   if (!pCStr)
   {
      ERR ("Unable to allocate temp string\n");
      return FALSE;
   }

   Ret = FindNextVolumeA (hFindVolume, pCStr, cchBufferLength);
   if (Ret)
   {
      if (!MultiByteToWideChar (CP_ACP, 0, pCStr, -1, lpszVolumeName,
                                cchBufferLength))
         Ret = FALSE;
   }

   HeapFree (GetProcessHeap (), 0, pCStr);
   return Ret;
}


/***********************************************************************
 *           FindVolumeClose   (KERNEL32.@)
 */
BOOL WINAPI FindVolumeClose (HANDLE hFindVolume)
{
   TRACE ("(0x%lx)\n", (unsigned long)hFindVolume);

   /* No checking of first param; XP SP2 always returns TRUE, and our
      HeapFree implementation should catch invalid pointers */
   HeapFree (GetProcessHeap (), 0, (PVOID)hFindVolume);
   return TRUE;
}


/***********************************************************************
 *           GetVolumePathNamesForVolumeNameA   (KERNEL32.@)
 */
BOOL WINAPI GetVolumePathNamesForVolumeNameA (LPCSTR lpszVolumeName,
                                              LPSTR lpszVolumePathNames,
                                              DWORD cchBufferLength,
                                              PDWORD lpcchReturnLength)
{
   char *pIdStart;
   int i;

   TRACE ("(%s, %p, %lu, %p)\n", lpszVolumeName, lpszVolumePathNames,
          cchBufferLength, lpcchReturnLength);

   /* XP SP2 crashes if either of first params are invalid; so we
      won't bother checking them */

   if (strncmp (lpszVolumeName, "\\\\?\\Volume{", 11))
   {
      SetLastError (ERROR_INVALID_NAME);
      return FALSE;
   }

   pIdStart = strchr (lpszVolumeName, '{');

   for (i = 0; i < MAX_DOS_DRIVES; i++)
   {
      if (!DRIVE_IsValid (i))
         continue;

      if (strncmp (DOSDrives[i].volume_id, pIdStart,
                   strlen (DOSDrives[i].volume_id)))
         continue;

      if (lpcchReturnLength)
         *lpcchReturnLength = 5;
      if (cchBufferLength < 5)
      {
         SetLastError (ERROR_MORE_DATA);
         return FALSE;
      }

      sprintf (lpszVolumePathNames, "%c:\\", 'A' + i);
      lpszVolumePathNames[4] = '\0';
      return TRUE;
   }

   SetLastError (ERROR_INVALID_NAME);
   return FALSE;
}


/***********************************************************************
 *           GetVolumePathNamesForVolumeNameW   (KERNEL32.@)
 */
BOOL WINAPI GetVolumePathNamesForVolumeNameW (LPCWSTR lpszVolumeName,
                                              LPWSTR lpszVolumePathNames,
                                              DWORD cchBufferLength,
                                              PDWORD lpcchReturnLength)
{
   char *pCVolumeName, *pCStr;
   BOOL Ret;
   INT Len;
   DWORD RetLen = 0;

   TRACE ("(%s, %p, %lu, %p)\n", debugstr_w (lpszVolumeName),
          lpszVolumePathNames, cchBufferLength, lpcchReturnLength);

   if (!cchBufferLength)
   {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return FALSE;
   }

   Len = strlenW (lpszVolumeName) + 1;
   pCVolumeName = HeapAlloc (GetProcessHeap (), 0, Len);
   if (!pCVolumeName)
      return FALSE;

   if (!WideCharToMultiByte (CP_ACP, 0, lpszVolumeName, Len,
                             pCVolumeName, Len, NULL, NULL))
   {
      HeapFree (GetProcessHeap (), 0, pCVolumeName);
      return FALSE;
   }

   pCStr = HeapAlloc (GetProcessHeap (), 0, cchBufferLength);
   if (!pCStr)
   {
      HeapFree (GetProcessHeap (), 0, pCVolumeName);
      return FALSE;
   }

   Ret = GetVolumePathNamesForVolumeNameA (pCVolumeName, pCStr,
                                           cchBufferLength, &RetLen);
   if (lpcchReturnLength)
      *lpcchReturnLength = RetLen;
   HeapFree (GetProcessHeap (), 0, pCVolumeName);
   if (Ret)
   {
      if (!MultiByteToWideChar (CP_ACP, 0, pCStr, RetLen, lpszVolumePathNames,
                                RetLen))
         Ret = FALSE;
   }

   HeapFree (GetProcessHeap (), 0, pCStr);
   return Ret;
}


/***********************************************************************
 *           DRIVE_IsValid
 */
int DRIVE_IsValid( int drive )
{
    if ((drive < 0) || (drive >= MAX_DOS_DRIVES)) return 0;
    return (DOSDrives[drive].root &&
            !(DOSDrives[drive].flags & DRIVE_DISABLED));
}


/***********************************************************************
 *           DRIVE_GetCurrentDrive
 */
int DRIVE_GetCurrentDrive(void)
{
    TDB *pTask = TASK_GetCurrent();
    if (pTask && (pTask->curdrive & 0x80)) return pTask->curdrive & ~0x80;
    return DRIVE_CurDrive;
}


/***********************************************************************
 *           DRIVE_SetCurrentDrive
 */
int DRIVE_SetCurrentDrive( int drive )
{
    TDB *pTask = TASK_GetCurrent();
    if (!DRIVE_IsValid( drive ))
    {
        SetLastError( ERROR_INVALID_DRIVE );
        return 0;
    }
    TRACE("%c:\n", 'A' + drive );
    DRIVE_CurDrive = drive;
    if (pTask) pTask->curdrive = drive | 0x80;
#if 0
    chdir(DRIVE_GetUnixCwd(drive));
#endif
    return 1;
}


/***********************************************************************
 *           DRIVE_FindDriveRoot
 *
 * Find a drive for which the root matches the beginning of the given path.
 * This can be used to translate a Unix path into a drive + DOS path.
 * Return value is the drive, or -1 on error. On success, path is modified
 * to point to the beginning of the DOS path.
 *
 * NOTE: This only works on a full unix path, no relative paths
 */
int DRIVE_FindDriveRoot( const char **path )
{
    /* idea: check at all '/' positions.
     * If the device and inode of that path is identical with the
     * device and inode of the current drive then we found a solution.
     * If there is another drive pointing to a deeper position in
     * the file tree, we want to find that one, not the earlier solution.
     */
    int drive, rootdrive = -1;
    char buffer[MAX_PATHNAME_LEN];
    char *next = buffer;
    const char *p = *path;
    struct stat st;

    strcpy( buffer, "/" );
    for (;;)
    {
        if (stat( buffer, &st ) || !S_ISDIR( st.st_mode )) break;

        /* Find the drive */

        for (drive = 0; drive < MAX_DOS_DRIVES; drive++)
        {
           if (!DOSDrives[drive].root ||
               (DOSDrives[drive].flags & DRIVE_DISABLED)) continue;

           if ((DOSDrives[drive].dev == st.st_dev) &&
               (DOSDrives[drive].ino == st.st_ino))
           {
               rootdrive = drive;
               *path = p;
	       break;
           }
        }

        /* Get the next path component */

        *next++ = '/';
        while ((*p == '/') || (*p == '\\')) p++;
        if (!*p) break;
        while (!IS_END_OF_NAME(*p)) *next++ = *p++;
        *next = 0;
    }
    *next = 0;

    if (rootdrive != -1)
        TRACE("%s -> drive %c:, root='%s', name='%s'\n",
              buffer, 'A' + rootdrive, DOSDrives[rootdrive].root, *path );
    return rootdrive;
}


/***********************************************************************
 *           DRIVE_GetRoot
 */
const char * DRIVE_GetRoot( int drive )
{
    if (!DRIVE_IsValid( drive )) return NULL;
    return DOSDrives[drive].root;
}


/***********************************************************************
 *           DRIVE_GetDosCwd
 */
const char * DRIVE_GetDosCwd( int drive )
{
    TDB *pTask = TASK_GetCurrent();
    if (!DRIVE_IsValid( drive )) return NULL;

    /* Check if we need to change the directory to the new task. */
    if (pTask && (pTask->curdrive & 0x80) &&    /* The task drive is valid */
        ((pTask->curdrive & ~0x80) == drive) && /* and it's the one we want */
        (DRIVE_LastTask != GetCurrentTask()))   /* and the task changed */
    {
        /* Perform the task-switch */
        if (!DRIVE_Chdir( drive, (char *)pTask->curdir ))
            DRIVE_Chdir( drive, "\\" );
        DRIVE_LastTask = GetCurrentTask();
    }
    return DOSDrives[drive].dos_cwd;
}


/***********************************************************************
 *           DRIVE_GetUnixCwd
 */
const char * DRIVE_GetUnixCwd( int drive )
{
    TDB *pTask = TASK_GetCurrent();
    if (!DRIVE_IsValid( drive )) return NULL;

    /* Check if we need to change the directory to the new task. */
    if (pTask && (pTask->curdrive & 0x80) &&    /* The task drive is valid */
        ((pTask->curdrive & ~0x80) == drive) && /* and it's the one we want */
        (DRIVE_LastTask != GetCurrentTask()))   /* and the task changed */
    {
        /* Perform the task-switch */
        if (!DRIVE_Chdir( drive, (char *)pTask->curdir ))
            DRIVE_Chdir( drive, "\\" );
        DRIVE_LastTask = GetCurrentTask();
    }
    return DOSDrives[drive].unix_cwd;
}


/***********************************************************************
 *           DRIVE_GetDevice
 */
const char * DRIVE_GetDevice( int drive )
{
    return (DRIVE_IsValid( drive )) ? DOSDrives[drive].device : NULL;
}

/******************************************************************
 *		static WORD CDROM_Data_FindBestVoldesc
 *
 *
 */
static WORD CDROM_Data_FindBestVoldesc(int fd)
{
    BYTE cur_vd_type, max_vd_type = 0;
    unsigned int offs, best_offs = 0, extra_offs = 0;
    char sig[3];

    for (offs = 0x8000; offs <= 0x9800; offs += 0x800)
    {
        /* if 'CDROM' occurs at position 8, this is a pre-iso9660 cd, and
         * the volume label is displaced forward by 8
         */
        lseek(fd, offs + 11, SEEK_SET); /* check for non-ISO9660 signature */
        read(fd, &sig, 3);
        if ((sig[0] == 'R') && (sig[1] == 'O') && (sig[2]=='M'))
        {
            extra_offs = 8;
        }
        lseek(fd, offs + extra_offs, SEEK_SET);
        read(fd, &cur_vd_type, 1);
        if (cur_vd_type == 0xff) /* voldesc set terminator */
            break;
        if (cur_vd_type > max_vd_type)
        {
            max_vd_type = cur_vd_type;
            best_offs = offs + extra_offs;
        }
    }
    return best_offs;
}

/***********************************************************************
 *           DRIVE_ReadSuperblock
 *
 * NOTE
 *      DRIVE_SetLabel and DRIVE_SetSerialNumber use this in order
 * to check, that they are writing on a FAT filesystem !
 */
int DRIVE_ReadSuperblock (int drive, char * buff)
{
#define DRIVE_SUPER 96
    int fd;
    off_t offs;
    int ret = 0;

    if (memset(buff,0,DRIVE_SUPER)!=buff) return -1;
    if ((fd=open(DOSDrives[drive].device,O_RDONLY)) == -1)
    {
	struct stat st;
	if (!DOSDrives[drive].device)
	    ERR("No device configured for drive %c: !\n", 'A'+drive);
	else
	    ERR("Couldn't open device '%s' for drive %c: ! (%s)\n", DOSDrives[drive].device, 'A'+drive,
		 (stat(DOSDrives[drive].device, &st)) ?
			"not available or symlink not valid ?" : "no permission");
	ERR("Can't read drive volume info ! Either pre-set it or make sure the device to read it from is accessible !\n");
	PROFILE_UsageWineIni();
	return -1;
    }

    switch(DOSDrives[drive].type)
    {
	case DRIVE_REMOVABLE:
	case DRIVE_FIXED:
	    offs = 0;
	    break;
	case DRIVE_CDROM:
	    offs = CDROM_Data_FindBestVoldesc(fd);
	    break;
        default:
            offs = 0;
            break;
    }

    if ((offs) && (lseek(fd,offs,SEEK_SET)!=offs))
    {
        ret = -4;
        goto the_end;
    }
    if (read(fd,buff,DRIVE_SUPER)!=DRIVE_SUPER)
    {
        ret = -2;
        goto the_end;
    }

    switch(DOSDrives[drive].type)
    {
	case DRIVE_REMOVABLE:
	case DRIVE_FIXED:
	    if ((buff[0x26]!=0x29) ||  /* Check for FAT present */
                /* FIXME: do really all FAT have their name beginning with
                   "FAT" ? (At least FAT12, FAT16 and FAT32 have :) */
	    	memcmp( buff+0x36,"FAT",3))
            {
                ERR("The filesystem is not FAT !! (device=%s)\n",
                    DOSDrives[drive].device);
                ret = -3;
                goto the_end;
            }
            break;
	case DRIVE_CDROM:
	    if (strncmp(&buff[1],"CD001",5)) /* Check for iso9660 present */
            {
		ret = -3;
                goto the_end;
            }
	    /* FIXME: do we need to check for "CDROM", too ? (high sierra) */
            break;
	default:
            ret = -3;
            goto the_end;
    }

    return close(fd);
 the_end:
    close(fd);
    return ret;
}


/***********************************************************************
 *           DRIVE_WriteSuperblockEntry
 *
 * NOTE
 *	We are writing as little as possible (ie. not the whole SuperBlock)
 * not to interfere with kernel. The drive can be mounted !
 */
int DRIVE_WriteSuperblockEntry (int drive, off_t ofs, size_t len, char * buff)
{
    int fd;

    if ((fd=open(DOSDrives[drive].device,O_WRONLY))==-1)
    {
        ERR("Cannot open the device %s (for writing)\n",
            DOSDrives[drive].device);
        return -1;
    }
    if (lseek(fd,ofs,SEEK_SET)!=ofs)
    {
        ERR("lseek failed on device %s !\n",
            DOSDrives[drive].device);
        close(fd);
        return -2;
    }
    if (write(fd,buff,len)!=len)
    {
        close(fd);
        ERR("Cannot write on %s !\n",
            DOSDrives[drive].device);
        return -3;
    }
    return close (fd);
}


/******************************************************************
 *		static HANDLE   CDROM_Open
 *
 *
 */
static HANDLE   CDROM_Open(int drive)
{
    char       root[7];

    strcpy(root, "\\\\.\\A:");
    root[4] += drive;

    return CreateFileA(root, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
}

/**************************************************************************
 *                              CDROM_Data_GetLabel             [internal]
 */
DWORD CDROM_Data_GetLabel(int drive, char *label)
{
#define LABEL_LEN       32+1
    int dev = open(DOSDrives[drive].device, O_RDONLY|O_NONBLOCK);
    WORD offs = CDROM_Data_FindBestVoldesc(dev);
    WCHAR label_read[LABEL_LEN]; /* Unicode possible, too */
    DWORD unicode_id = 0;

    if (offs)
    {
        if ((lseek(dev, offs+0x58, SEEK_SET) == offs+0x58)
        &&  (read(dev, &unicode_id, 3) == 3))
        {
            int ver = (unicode_id & 0xff0000) >> 16;

            if ((lseek(dev, offs+0x28, SEEK_SET) != offs+0x28)
            ||  (read(dev, &label_read, LABEL_LEN) != LABEL_LEN))
                goto failure;

            close(dev);
            if ((LOWORD(unicode_id) == 0x2f25) /* Unicode ID */
            &&  ((ver == 0x40) || (ver == 0x43) || (ver == 0x45)))
            { /* yippee, unicode */
                int i;
                WORD ch;
                for (i=0; i<LABEL_LEN;i++)
                { /* Motorola -> Intel Unicode conversion :-\ */
                     ch = label_read[i];
                     label_read[i] = (ch << 8) | (ch >> 8);
                }
                WideCharToMultiByte( CP_ACP, 0, label_read, -1, label, 12, NULL, NULL );
                label[11] = 0;
            }
            else
            {
                strncpy(label, (LPSTR)label_read, 11);
                label[11] = '\0';
            }
            return 1;
        }
    }
failure:
    close(dev);
    ERR("error reading label !\n");
    return 0;
}

/**************************************************************************
 *				CDROM_GetLabel			[internal]
 */
static DWORD CDROM_GetLabel(int drive, char *label)
{
    HANDLE              h = CDROM_Open(drive);
    CDROM_DISK_DATA     cdd;
    DWORD               br;
    DWORD               ret = 1;

    if (!h || !DeviceIoControl(h, IOCTL_CDROM_DISK_TYPE, NULL, 0, &cdd, sizeof(cdd), &br, 0))
        return 0;

    switch (cdd.DiskData & 0x03)
    {
    case CDROM_DISK_DATA_TRACK:
        if (!CDROM_Data_GetLabel(drive, label))
            ret = 0;
        break;
    case CDROM_DISK_AUDIO_TRACK:
        strcpy(label, "Audio CD   ");
        break;
    case CDROM_DISK_DATA_TRACK|CDROM_DISK_AUDIO_TRACK:
        FIXME("Need to get the label of a mixed mode CD: not implemented yet !\n");
        /* fall through */
    case 0:
        ret = 0;
        break;
    }
    TRACE("CD: label is '%s'.\n", label);

    return ret;
}

/***********************************************************************
 *           DRIVE_GetLabel
 */
const char * DRIVE_GetLabel( int drive )
{
    int read = 0;
    char buff[DRIVE_SUPER];
    int offs = -1;

    if (!DRIVE_IsValid( drive )) return NULL;
    if (DOSDrives[drive].type == DRIVE_CDROM)
    {
	read = CDROM_GetLabel(drive, DOSDrives[drive].label_read);
    }
    else
    if (DOSDrives[drive].flags & DRIVE_READ_VOL_INFO)
    {
	if (DRIVE_ReadSuperblock(drive,(char *) buff))
	    ERR("Invalid or unreadable superblock on %s (%c:).\n",
		DOSDrives[drive].device, (char)(drive+'A'));
	else {
	    if (DOSDrives[drive].type == DRIVE_REMOVABLE ||
		DOSDrives[drive].type == DRIVE_FIXED)
		offs = 0x2b;

	    /* FIXME: ISO9660 uses a 32 bytes long label. Should we do also? */
	    if (offs != -1) memcpy(DOSDrives[drive].label_read,buff+offs,11);
	    DOSDrives[drive].label_read[11]='\0';
	    read = 1;
	}
    }

    return (read) ?
	DOSDrives[drive].label_read : DOSDrives[drive].label_conf;
}

#define CDFRAMES_PERSEC                 75
#define CDFRAMES_PERMIN                 (CDFRAMES_PERSEC * 60)
#define FRAME_OF_ADDR(a) ((a)[0] * CDFRAMES_PERMIN + (a)[1] * CDFRAMES_PERSEC + (a)[2])
#define FRAME_OF_TOC(toc, idx)  FRAME_OF_ADDR((toc).TrackData[idx - (toc).FirstTrack].Address)

/**************************************************************************
 *                              CDROM_Audio_GetSerial           [internal]
 */
static DWORD CDROM_Audio_GetSerial(HANDLE h)
{
    unsigned long serial = 0;
    int i;
    WORD wMagic;
    DWORD dwStart, dwEnd, br;
    CDROM_TOC toc;

    if (!DeviceIoControl(h, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), &br, 0))
        return 0;

    /*
     * wMagic collects the wFrames from track 1
     * dwStart, dwEnd collect the beginning and end of the disc respectively, in
     * frames.
     * There it is collected for correcting the serial when there are less than
     * 3 tracks.
     */
    wMagic = toc.TrackData[0].Address[2];
    dwStart = FRAME_OF_TOC(toc, toc.FirstTrack);

    for (i = 0; i <= toc.LastTrack - toc.FirstTrack; i++) {
        serial += (toc.TrackData[i].Address[0] << 16) |
            (toc.TrackData[i].Address[1] << 8) | toc.TrackData[i].Address[2];
    }
    dwEnd = FRAME_OF_TOC(toc, toc.LastTrack + 1);

    if (toc.LastTrack - toc.FirstTrack + 1 < 3)
        serial += wMagic + (dwEnd - dwStart);

    return serial;
}

/**************************************************************************
 *                              CDROM_Data_GetSerial            [internal]
 */
static DWORD CDROM_Data_GetSerial(int drive)
{
    int dev = open(DOSDrives[drive].device, O_RDONLY|O_NONBLOCK);
    WORD offs;
    union {
        unsigned long val;
        unsigned char p[4];
    } serial;
    BYTE b0 = 0, b1 = 1, b2 = 2, b3 = 3;


    if (dev == -1) return 0;
    offs = CDROM_Data_FindBestVoldesc(dev);

    serial.val = 0;
    if (offs)
    {
        BYTE buf[2048];
        OSVERSIONINFOA ovi;
        int i;

        lseek(dev, offs, SEEK_SET);
        read(dev, buf, 2048);
        /*
         * OK, another braindead one... argh. Just believe it.
         * Me$$ysoft chose to reverse the serial number in NT4/W2K.
         * It's true and nobody will ever be able to change it.
         */
        ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        GetVersionExA(&ovi);
        if ((ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) &&  (ovi.dwMajorVersion >= 4))
        {
            b0 = 3; b1 = 2; b2 = 1; b3 = 0;
        }
        for (i = 0; i < 2048; i += 4)
        {
            /* DON'T optimize this into DWORD !! (breaks overflow) */
            serial.p[b0] += buf[i+b0];
            serial.p[b1] += buf[i+b1];
            serial.p[b2] += buf[i+b2];
            serial.p[b3] += buf[i+b3];
        }
    }
    close(dev);
    return serial.val;
}

/**************************************************************************
 *				CDROM_GetSerial			[internal]
 */
static DWORD CDROM_GetSerial(int drive)
{
    DWORD               serial = 0;
    HANDLE              h = CDROM_Open(drive);
    CDROM_DISK_DATA     cdd;
    DWORD               br;

    if (!h || ! !DeviceIoControl(h, IOCTL_CDROM_DISK_TYPE, NULL, 0, &cdd, sizeof(cdd), &br, 0))
        return 0;

    switch (cdd.DiskData & 0x03)
    {
    case CDROM_DISK_DATA_TRACK:
        /* hopefully a data CD */
        serial = CDROM_Data_GetSerial(drive);
        break;
    case CDROM_DISK_AUDIO_TRACK:
        /* fall thru */
    case CDROM_DISK_DATA_TRACK|CDROM_DISK_AUDIO_TRACK:
        serial = CDROM_Audio_GetSerial(h);
        break;
    case 0:
        break;
    }

    if (serial)
        TRACE("CD serial number is %04x-%04x.\n", HIWORD(serial), LOWORD(serial));

    CloseHandle(h);

    return serial;
}

/***********************************************************************
 *           DRIVE_GetSerialNumber
 */
DWORD DRIVE_GetSerialNumber( int drive )
{
    DWORD serial = 0;
    char buff[DRIVE_SUPER];

    if (!DRIVE_IsValid( drive )) return 0;

    if (DOSDrives[drive].flags & DRIVE_READ_VOL_INFO)
    {
	switch(DOSDrives[drive].type)
	{
        case DRIVE_REMOVABLE:
        case DRIVE_FIXED:
            if (DRIVE_ReadSuperblock(drive,(char *) buff))
                MESSAGE("Invalid or unreadable superblock on %s (%c:)."
                        " Maybe not FAT?\n" ,
                        DOSDrives[drive].device, 'A'+drive);
            else
                serial = *((DWORD*)(buff+0x27));
            break;
        case DRIVE_CDROM:
            serial = CDROM_GetSerial(drive);
            break;
        default:
            FIXME("Serial number reading from file system on drive %c: not supported yet.\n", drive+'A');
	}
    }

    return (serial) ? serial : DOSDrives[drive].serial_conf;
}


/***********************************************************************
 *           DRIVE_SetSerialNumber
 */
int DRIVE_SetSerialNumber( int drive, DWORD serial )
{
    char buff[DRIVE_SUPER];

    if (!DRIVE_IsValid( drive )) return 0;

    if (DOSDrives[drive].flags & DRIVE_READ_VOL_INFO)
    {
        if ((DOSDrives[drive].type != DRIVE_REMOVABLE) &&
            (DOSDrives[drive].type != DRIVE_FIXED)) return 0;
        /* check, if the drive has a FAT filesystem */
        if (DRIVE_ReadSuperblock(drive, buff)) return 0;
        if (DRIVE_WriteSuperblockEntry(drive, 0x27, 4, (char *) &serial)) return 0;
        return 1;
    }

    if (DOSDrives[drive].type == DRIVE_CDROM) return 0;
    DOSDrives[drive].serial_conf = serial;
    return 1;
}


/***********************************************************************
 *           DRIVE_GetType
 */
UINT DRIVE_GetType( int drive )
{
    if (!DRIVE_IsValid( drive )) return DRIVE_UNKNOWN;
    return DOSDrives[drive].type;
}


/***********************************************************************
 *           DRIVE_GetFlags
 */
UINT DRIVE_GetFlags( int drive )
{
    if ((drive < 0) || (drive >= MAX_DOS_DRIVES)) return 0;
    return DOSDrives[drive].flags;
}


/***********************************************************************
 *           DRIVE_Chdir
 */
int DRIVE_Chdir( int drive, const char *path )
{
    DOS_FULL_NAME full_name;
    char buffer[MAX_PATHNAME_LEN];
    LPSTR unix_cwd;
    BY_HANDLE_FILE_INFORMATION info;
    TDB *pTask = TASK_GetCurrent();

    strcpy( buffer, "A:" );
    buffer[0] += drive;
    TRACE("(%s,%s)\n", buffer, path );
    lstrcpynA( buffer + 2, path, sizeof(buffer) - 2 );

    if (!DOSFS_GetFullName( buffer, TRUE, &full_name ))
    {
       if (!DOSFS_GetFullName (buffer, FALSE, &full_name))
          SetLastError (ERROR_PATH_NOT_FOUND);
       return 0;
    }
    if (!FILE_Stat( full_name.long_name, &info )) return 0;
    if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        SetLastError( ERROR_FILE_NOT_FOUND );
        return 0;
    }
    unix_cwd = full_name.long_name + strlen( DOSDrives[drive].root );
    while (*unix_cwd == '/') unix_cwd++;

    TRACE("(%c:): unix_cwd=%s dos_cwd=%s\n",
                   'A' + drive, unix_cwd, full_name.short_name + 3 );

    HeapFree( GetProcessHeap(), 0, DOSDrives[drive].dos_cwd );
    HeapFree( GetProcessHeap(), 0, DOSDrives[drive].unix_cwd );
    DOSDrives[drive].dos_cwd  = heap_strdup( full_name.short_name + 3 );
    DOSDrives[drive].unix_cwd = heap_strdup( unix_cwd );

    if (pTask && (pTask->curdrive & 0x80) &&
        ((pTask->curdrive & ~0x80) == drive))
    {
        lstrcpynA( (char *)pTask->curdir, full_name.short_name + 2,
                     sizeof(pTask->curdir) );
        DRIVE_LastTask = GetCurrentTask();
#if 0
        chdir(unix_cwd); /* Only change if on current drive */
#endif
    }
    return 1;
}


/***********************************************************************
 *           DRIVE_Disable
 */
int DRIVE_Disable( int drive  )
{
    if ((drive < 0) || (drive >= MAX_DOS_DRIVES) || !DOSDrives[drive].root)
    {
        SetLastError( ERROR_INVALID_DRIVE );
        return 0;
    }
    DOSDrives[drive].flags |= DRIVE_DISABLED;
    return 1;
}


/***********************************************************************
 *           DRIVE_Enable
 */
int DRIVE_Enable( int drive  )
{
    if ((drive < 0) || (drive >= MAX_DOS_DRIVES) || !DOSDrives[drive].root)
    {
        SetLastError( ERROR_INVALID_DRIVE );
        return 0;
    }
    DOSDrives[drive].flags &= ~DRIVE_DISABLED;
    return 1;
}


/***********************************************************************
 *           DRIVE_SetLogicalMapping
 */
int DRIVE_SetLogicalMapping ( int existing_drive, int new_drive )
{
 /* If new_drive is already valid, do nothing and return 0
    otherwise, copy DOSDrives[existing_drive] to DOSDrives[new_drive] */

    DOSDRIVE *old, *new;

    old = DOSDrives + existing_drive;
    new = DOSDrives + new_drive;

    if ((existing_drive < 0) || (existing_drive >= MAX_DOS_DRIVES) ||
        !old->root ||
	(new_drive < 0) || (new_drive >= MAX_DOS_DRIVES))
    {
        SetLastError( ERROR_INVALID_DRIVE );
        return 0;
    }

    if ( new->root )
    {
        TRACE("Can't map drive %c: to already existing drive %c:\n",
              'A' + existing_drive, 'A' + new_drive );
	/* it is already mapped there, so return success */
	if (!strcmp(old->root,new->root))
	    return 1;
	return 0;
    }

    new->root     = heap_strdup( old->root );
    new->dos_cwd  = heap_strdup( old->dos_cwd );
    new->unix_cwd = heap_strdup( old->unix_cwd );
    new->device   = heap_strdup( old->device );
    new->dos_device_name = heap_strdup( old->dos_device_name );
    memcpy ( new->label_conf, old->label_conf, 12 );
    memcpy ( new->label_read, old->label_read, 12 );
    new->serial_conf = old->serial_conf;
    new->type = old->type;
    new->flags = old->flags;
    new->dev = old->dev;
    new->ino = old->ino;
    new->dev_dev = old->dev_dev;
    new->dev_ino = old->dev_ino;

    TRACE("Drive %c: is now equal to drive %c:\n",
          'A' + new_drive, 'A' + existing_drive );

    return 1;
}


/***********************************************************************
 *           DRIVE_OpenDevice
 *
 * Open the drive raw device and return a Unix fd (or -1 on error).
 */
int DRIVE_OpenDevice( int drive, int flags )
{
    if (!DRIVE_IsValid( drive )) return -1;
    return open( DOSDrives[drive].device, flags );
}


/***********************************************************************
 *           DRIVE_RawRead
 *
 * Read raw sectors from a device
 */
int DRIVE_RawRead(BYTE drive, DWORD begin, DWORD nr_sect, BYTE *dataptr, BOOL fake_success)
{
    int fd;

    if ((fd = DRIVE_OpenDevice( drive, O_RDONLY )) != -1)
    {
        lseek( fd, begin * 512, SEEK_SET );
        /* FIXME: check errors */
        read( fd, dataptr, nr_sect * 512 );
        close( fd );
    }
    else
    {
        memset(dataptr, 0, nr_sect * 512);
	if (fake_success)
        {
	    if (begin == 0 && nr_sect > 1) *(dataptr + 512) = 0xf8;
	    if (begin == 1) *dataptr = 0xf8;
	}
	else
	    return 0;
    }
    return 1;
}


/***********************************************************************
 *           DRIVE_RawWrite
 *
 * Write raw sectors to a device
 */
int DRIVE_RawWrite(BYTE drive, DWORD begin, DWORD nr_sect, BYTE *dataptr, BOOL fake_success)
{
    int fd;

    if ((fd = DRIVE_OpenDevice( drive, O_RDONLY )) != -1)
    {
        lseek( fd, begin * 512, SEEK_SET );
        /* FIXME: check errors */
        write( fd, dataptr, nr_sect * 512 );
        close( fd );
    }
    else
    if (!(fake_success))
	return 0;

    return 1;
}


/***********************************************************************
 *           DRIVE_GetFreeSpace
 */
static int DRIVE_GetFreeSpace( int drive, PULARGE_INTEGER size,
			       PULARGE_INTEGER available )
{
    struct statfs info;

    if (!DRIVE_IsValid(drive))
    {
        SetLastError( ERROR_INVALID_DRIVE );
        return 0;
    }

/* FIXME: add autoconf check for this */
#if defined(__svr4__) || defined(_SCO_DS) || defined(__sun)
    if (statfs( DOSDrives[drive].root, &info, 0, 0) < 0)
#else
    if (statfs( DOSDrives[drive].root, &info) < 0)
#endif
    {
        FILE_SetDosError();
        WARN("cannot do statfs(%s)\n", DOSDrives[drive].root);
        return 0;
    }

    size->QuadPart = RtlEnlargedUnsignedMultiply( info.f_bsize, info.f_blocks );
#ifdef STATFS_HAS_BAVAIL
    available->QuadPart = RtlEnlargedUnsignedMultiply( info.f_bavail, info.f_bsize );
#else
# ifdef STATFS_HAS_BFREE
    available->QuadPart = RtlEnlargedUnsignedMultiply( info.f_bfree, info.f_bsize );
# else
#  error "statfs has no bfree/bavail member!"
# endif
#endif
    if (DOSDrives[drive].type == DRIVE_CDROM)
    { /* ALWAYS 0, even if no real CD-ROM mounted there !! */
        available->QuadPart = 0;
    }
    return 1;
}

/***********************************************************************
 *       DRIVE_GetCurrentDirectory
 * Returns "X:\\path\\etc\\".
 *
 * Despite the API description, return required length including the
 * terminating null when buffer too small. This is the real behaviour.
*/

static UINT DRIVE_GetCurrentDirectory( UINT buflen, LPSTR buf )
{
    UINT ret;
    const char *s = DRIVE_GetDosCwd( DRIVE_GetCurrentDrive() );

    assert(s);
    ret = strlen(s) + 3; /* length of WHOLE current directory */
    if (ret >= buflen) return ret + 1;
    lstrcpynA( buf, "A:\\", min( 4u, buflen ) );
    if (buflen) buf[0] += DRIVE_GetCurrentDrive();
    if (buflen > 3) lstrcpynA( buf + 3, s, buflen - 3 );
    return ret;
}


/***********************************************************************
 *           DRIVE_BuildEnv
 *
 * Build the environment array containing the drives' current directories.
 * Resulting pointer must be freed with HeapFree.
 */
char *DRIVE_BuildEnv(void)
{
    int i, length = 0;
    const char *cwd[MAX_DOS_DRIVES];
    char *env, *p;

    for (i = 0; i < MAX_DOS_DRIVES; i++)
    {
        if ((cwd[i] = DRIVE_GetDosCwd(i)) && cwd[i][0]) length += strlen(cwd[i]) + 8;
    }
    if (!(env = HeapAlloc( GetProcessHeap(), 0, length+1 ))) return NULL;
    for (i = 0, p = env; i < MAX_DOS_DRIVES; i++)
    {
        if (cwd[i] && cwd[i][0])
            p += sprintf( p, "=%c:=%c:\\%s", 'A'+i, 'A'+i, cwd[i] ) + 1;
    }
    *p = 0;
    return env;
}


/***********************************************************************
 *           GetDiskFreeSpace   (KERNEL.422)
 */
BOOL16 WINAPI GetDiskFreeSpace16( LPCSTR root, LPDWORD cluster_sectors,
                                  LPDWORD sector_bytes, LPDWORD free_clusters,
                                  LPDWORD total_clusters )
{
    return GetDiskFreeSpaceA( root, cluster_sectors, sector_bytes,
                                free_clusters, total_clusters );
}


/***********************************************************************
 *           GetDiskFreeSpaceA   (KERNEL32.@)
 *
 * Fails if expression resulting from current drive's dir and "root"
 * is not a root dir of the target drive.
 *
 * UNDOC: setting some LPDWORDs to NULL is perfectly possible
 * if the corresponding info is unneeded.
 *
 * FIXME: needs to support UNC names from Win95 OSR2 on.
 *
 * Behaviour under Win95a:
 * CurrDir     root   result
 * "E:\\TEST"  "E:"   FALSE
 * "E:\\"      "E:"   TRUE
 * "E:\\"      "E"    FALSE
 * "E:\\"      "\\"   TRUE
 * "E:\\TEST"  "\\"   TRUE
 * "E:\\TEST"  ":\\"  FALSE
 * "E:\\TEST"  "E:\\" TRUE
 * "E:\\TEST"  ""     FALSE
 * "E:\\"      ""     FALSE (!)
 * "E:\\"      0x0    TRUE
 * "E:\\TEST"  0x0    TRUE  (!)
 * "E:\\TEST"  "C:"   TRUE  (when CurrDir of "C:" set to "\\")
 * "E:\\TEST"  "C:"   FALSE (when CurrDir of "C:" set to "\\TEST")
 */
BOOL WINAPI GetDiskFreeSpaceA( LPCSTR root, LPDWORD cluster_sectors,
                                   LPDWORD sector_bytes, LPDWORD free_clusters,
                                   LPDWORD total_clusters )
{
    int	drive, sec_size;
    ULARGE_INTEGER size,available;
    LPCSTR path;
    DWORD cluster_sec, version;

    if ((!root) || (strcmp(root,"\\") == 0))
	drive = DRIVE_GetCurrentDrive();
    else
    if ( (strlen(root) >= 2) && (root[1] == ':')) /* root contains drive tag */
    {
        drive = toupper(root[0]) - 'A';
	path = &root[2];
	if (path[0] == '\0')
	    path = DRIVE_GetDosCwd(drive);
	else
	if (path[0] == '\\')
	    path++;
        if (path[0]) /* oops, we are in a subdir */
        {
            SetLastError(ERROR_INVALID_NAME);
            return FALSE;
        }
    }
    else
    {
        SetLastError(ERROR_INVALID_NAME);
        return FALSE;
    }

    if (!DRIVE_GetFreeSpace(drive, &size, &available)) return FALSE;

    version = GetVersion();
    if (version & 0x80000000) {
	/* win9x: Cap the size and available at 2GB as per specs.  */
	if ((size.s.HighPart) || (size.s.LowPart > 0x7fffffff))
	{
	    size.s.HighPart = 0;
	    size.s.LowPart = 0x7fffffff;
	}
	if ((available.s.HighPart) || (available.s.LowPart > 0x7fffffff))
	{
	    available.s.HighPart =0;
	    available.s.LowPart = 0x7fffffff;
	}
    }
    sec_size = (DRIVE_GetType(drive)==DRIVE_CDROM) ? 2048 : 512;
    size.QuadPart = RtlLargeIntegerDivide( size.QuadPart, sec_size, NULL);
    available.QuadPart = RtlLargeIntegerDivide( available.QuadPart, sec_size, NULL);
    /* FIXME: probably have to adjust those variables too for CDFS */
    if (version & 0x80000000) {
	/* win9x: cannot have more than 65536 clusters. */
	cluster_sec = 1;
	while (cluster_sec * 65536 < size.s.LowPart) cluster_sec *= 2;
    } else {
	/* find smallest power-of-two cluster size which allows the
	 * returned cluster count to fit into 32 bits; we won't
	 * care if the count is larger than 65536. */
	cluster_sec = 1;
	while (cluster_sec <= size.s.HighPart) cluster_sec *= 2;
    }

    if (cluster_sectors) {
	*cluster_sectors = cluster_sec;
	TRACE("cluster sectors: %ld\n", *cluster_sectors);
    }
    if (sector_bytes) {
	*sector_bytes    = sec_size;
	TRACE("sector bytes: %ld\n", *sector_bytes);
    }
    if (free_clusters) {
	*free_clusters   = RtlEnlargedUnsignedDivide(available.QuadPart, cluster_sec, NULL);
	TRACE("free clusters: %ld\n", *free_clusters);
    }
    if (total_clusters) {
	*total_clusters   = RtlEnlargedUnsignedDivide(size.QuadPart, cluster_sec, NULL);
	TRACE("total clusters: %ld\n", *total_clusters);
    }
    return TRUE;
}


/***********************************************************************
 *           GetDiskFreeSpaceW   (KERNEL32.@)
 */
BOOL WINAPI GetDiskFreeSpaceW( LPCWSTR root, LPDWORD cluster_sectors,
                                   LPDWORD sector_bytes, LPDWORD free_clusters,
                                   LPDWORD total_clusters )
{
    LPSTR xroot;
    BOOL ret;

    xroot = FILE_strdupWtoA( GetProcessHeap(), 0, root);
    ret = GetDiskFreeSpaceA( xroot,cluster_sectors, sector_bytes,
                               free_clusters, total_clusters );
    HeapFree( GetProcessHeap(), 0, xroot );
    return ret;
}


/***********************************************************************
 *           GetDiskFreeSpaceExA   (KERNEL32.@)
 *
 *  This function is used to acquire the size of the available and
 *  total space on a logical volume.
 *
 * RETURNS
 *
 *  Zero on failure, nonzero upon success. Use GetLastError to obtain
 *  detailed error information.
 *
 */
BOOL WINAPI GetDiskFreeSpaceExA( LPCSTR root,
				     PULARGE_INTEGER avail,
				     PULARGE_INTEGER total,
				     PULARGE_INTEGER totalfree)
{
    int	drive;
    ULARGE_INTEGER size,available;

    if (!root) drive = DRIVE_GetCurrentDrive();
    else
    { /* C: always works for GetDiskFreeSpaceEx */
        if ((root[1]) && ((root[1] != ':') || (root[2] && root[2] != '\\')))
        {
            if ((drive = DRIVE_FindDriveRoot( &root )) == -1)
            {
                FIXME("there are valid root names which are not supported yet (for root %s\n",
                      debugstr_a(root));
                /* ..like UNC names, for instance. */
                return FALSE;
            }
        }
        else drive = toupper(root[0]) - 'A';
    }

    if (!DRIVE_GetFreeSpace(drive, &size, &available)) return FALSE;

    if (total)
    {
        total->s.HighPart = size.s.HighPart;
        total->s.LowPart = size.s.LowPart;
    }

    if (totalfree)
    {
        totalfree->s.HighPart = available.s.HighPart;
        totalfree->s.LowPart = available.s.LowPart;
    }

    if (avail)
    {
        if (FIXME_ON(dosfs))
	{
            /* On Windows2000, we need to check the disk quota
	       allocated for the user owning the calling process. We
	       don't want to be more obtrusive than necessary with the
	       FIXME messages, so don't print the FIXME unless Wine is
	       actually masquerading as Windows2000. */

            OSVERSIONINFOA ovi;
	    ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	    if (GetVersionExA(&ovi))
	    {
	      if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT && ovi.dwMajorVersion > 4)
                  FIXME("no per-user quota support yet\n");
	    }
	}

        /* Quick hack, should eventually be fixed to work 100% with
           Windows2000 (see comment above). */
        avail->s.HighPart = available.s.HighPart;
        avail->s.LowPart = available.s.LowPart;
    }

    return TRUE;
}

/***********************************************************************
 *           GetDiskFreeSpaceExW   (KERNEL32.@)
 */
BOOL WINAPI GetDiskFreeSpaceExW( LPCWSTR root, PULARGE_INTEGER avail,
				     PULARGE_INTEGER total,
				     PULARGE_INTEGER  totalfree)
{
    LPSTR xroot;
    BOOL ret;

    xroot = FILE_strdupWtoA( GetProcessHeap(), 0, root);
    ret = GetDiskFreeSpaceExA( xroot, avail, total, totalfree);
    HeapFree( GetProcessHeap(), 0, xroot );
    return ret;
}

/***********************************************************************
 *           GetDriveType   (KERNEL.136)
 * This function returns the type of a drive in Win16.
 * Note that it returns DRIVE_REMOTE for CD-ROMs, since MSCDEX uses the
 * remote drive API. The return value DRIVE_REMOTE for CD-ROMs has been
 * verified on Win 3.11 and Windows 95. Some programs rely on it, so don't
 * do any pseudo-clever changes.
 *
 * RETURNS
 *	drivetype DRIVE_xxx
 */
UINT16 WINAPI GetDriveType16( UINT16 drive ) /* [in] number (NOT letter) of drive */
{
    UINT type = DRIVE_GetType(drive);
    TRACE("(%c:)\n", 'A' + drive );
    if (type == DRIVE_CDROM) type = DRIVE_REMOTE;
    return type;
}


/***********************************************************************
 *           GetDriveTypeA   (KERNEL32.@)
 *
 * Returns the type of the disk drive specified. If root is NULL the
 * root of the current directory is used.
 *
 * RETURNS
 *
 *  Type of drive (from Win32 SDK):
 *
 *   DRIVE_UNKNOWN     unable to find out anything about the drive
 *   DRIVE_NO_ROOT_DIR nonexistent root dir
 *   DRIVE_REMOVABLE   the disk can be removed from the machine
 *   DRIVE_FIXED       the disk can not be removed from the machine
 *   DRIVE_REMOTE      network disk
 *   DRIVE_CDROM       CDROM drive
 *   DRIVE_RAMDISK     virtual disk in RAM
 */
UINT WINAPI GetDriveTypeA(LPCSTR root) /* [in] String describing drive */
{
    int drive;
    TRACE("(%s)\n", debugstr_a(root));

    if (NULL == root) drive = DRIVE_GetCurrentDrive();
    else
    {
        if ((root[1]) && (root[1] != ':'))
	{
	    WARN("invalid root %s\n", debugstr_a(root));
	    return DRIVE_NO_ROOT_DIR;
	}

	/* Check to make sure that the path given is its root */
	{
	    int i = 0;

	    /* Find the first \ */
	    while( (root[i] != '\\') && (root[i] != 0) ) i++;

	    /* Skip through any extra \'s */
	    while( root[i] == '\\' ) i++;

	    /* Make sure we aren't at the end of the string */
	    if( root[i] != 0 )
	    {
		WARN("passed string isn't root %s\n", debugstr_a(root));
		return DRIVE_NO_ROOT_DIR;
	    }
	}

	drive = toupper(root[0]) - 'A';
    }
    return DRIVE_GetType(drive);
}


/***********************************************************************
 *           GetDriveTypeW   (KERNEL32.@)
 */
UINT WINAPI GetDriveTypeW( LPCWSTR root )
{
    LPSTR xpath = FILE_strdupWtoA( GetProcessHeap(), 0, root );
    UINT ret = GetDriveTypeA( xpath );
    HeapFree( GetProcessHeap(), 0, xpath );
    return ret;
}


/***********************************************************************
 *           GetCurrentDirectory   (KERNEL.411)
 */
UINT16 WINAPI GetCurrentDirectory16( UINT16 buflen, LPSTR buf )
{
    return (UINT16)DRIVE_GetCurrentDirectory(buflen, buf);
}


/***********************************************************************
 *           GetCurrentDirectoryA   (KERNEL32.@)
 */
UINT WINAPI GetCurrentDirectoryA( UINT buflen, LPSTR buf )
{
    UINT ret;
    char longname[MAX_PATHNAME_LEN];
    char shortname[MAX_PATHNAME_LEN];
    ret = DRIVE_GetCurrentDirectory(MAX_PATHNAME_LEN, shortname);
    if ( ret > MAX_PATHNAME_LEN ) {
      ERR_(file)("pathnamelength (%d) > MAX_PATHNAME_LEN!\n", ret );
      return ret;
    }
    GetLongPathNameA(shortname, longname, MAX_PATHNAME_LEN);
    ret = strlen( longname ) + 1;
    if (ret > buflen) return ret;
    strcpy(buf, longname);
    return ret - 1;
}

/***********************************************************************
 *           GetCurrentDirectoryW   (KERNEL32.@)
 */
UINT WINAPI GetCurrentDirectoryW( UINT buflen, LPWSTR buf )
{
    LPSTR xpath = HeapAlloc( GetProcessHeap(), 0, buflen+1 );
    UINT ret = GetCurrentDirectoryA( buflen, xpath );
    if (ret < buflen) ret = MultiByteToWideChar( CP_ACP, 0, xpath, -1, buf, buflen ) - 1;
    HeapFree( GetProcessHeap(), 0, xpath );
    return ret;
}


/***********************************************************************
 *           SetCurrentDirectory   (KERNEL.412)
 */
BOOL16 WINAPI SetCurrentDirectory16( LPCSTR dir )
{
    return SetCurrentDirectoryA( dir );
}


/***********************************************************************
 *           SetCurrentDirectoryA   (KERNEL32.@)
 */
BOOL WINAPI SetCurrentDirectoryA( LPCSTR dir )
{
    int drive, olddrive = DRIVE_GetCurrentDrive();

    if (!dir) {
    	ERR_(file)("(NULL)!\n");
	return FALSE;
    }
    if (dir[0] && (dir[1]==':'))
    {
        drive = toupper( *dir ) - 'A';
        dir += 2;
    }
    else
	drive = olddrive;

    /* WARNING: we need to set the drive before the dir, as DRIVE_Chdir
       sets pTask->curdir only if pTask->curdrive is drive */
    if (!(DRIVE_SetCurrentDrive( drive )))
	return FALSE;
    /* FIXME: what about empty strings? Add a \\ ? */
    if (!DRIVE_Chdir( drive, dir )) {
	DRIVE_SetCurrentDrive(olddrive);
	return FALSE;
    }
    return TRUE;
}


/***********************************************************************
 *           SetCurrentDirectoryW   (KERNEL32.@)
 */
BOOL WINAPI SetCurrentDirectoryW( LPCWSTR dirW )
{
    LPSTR dir = FILE_strdupWtoA( GetProcessHeap(), 0, dirW );
    BOOL res = SetCurrentDirectoryA( dir );
    HeapFree( GetProcessHeap(), 0, dir );
    return res;
}


/***********************************************************************
 *           GetLogicalDriveStringsA   (KERNEL32.@)
 */
UINT WINAPI GetLogicalDriveStringsA( UINT len, LPSTR buffer )
{
    int drive, count;

    for (drive = count = 0; drive < MAX_DOS_DRIVES; drive++)
        if (DRIVE_IsValid(drive)) count++;
    if ((count * 4) + 1 <= len)
    {
        LPSTR p = buffer;
        for (drive = 0; drive < MAX_DOS_DRIVES; drive++)
            if (DRIVE_IsValid(drive))
            {
                *p++ = 'a' + drive;
                *p++ = ':';
                *p++ = '\\';
                *p++ = '\0';
            }
        *p = '\0';
        return count * 4;
    }
    else
        return (count * 4) + 1; /* account for terminating null */
    /* The API tells about these different return values */
}


/***********************************************************************
 *           GetLogicalDriveStringsW   (KERNEL32.@)
 */
UINT WINAPI GetLogicalDriveStringsW( UINT len, LPWSTR buffer )
{
    int drive, count;

    for (drive = count = 0; drive < MAX_DOS_DRIVES; drive++)
        if (DRIVE_IsValid(drive)) count++;
    if (count * 4 * sizeof(WCHAR) <= len)
    {
        LPWSTR p = buffer;
        for (drive = 0; drive < MAX_DOS_DRIVES; drive++)
            if (DRIVE_IsValid(drive))
            {
                *p++ = (WCHAR)('a' + drive);
                *p++ = (WCHAR)':';
                *p++ = (WCHAR)'\\';
                *p++ = (WCHAR)'\0';
            }
        *p = (WCHAR)'\0';
    }
    return count * 4 * sizeof(WCHAR);
}


/***********************************************************************
 *           GetLogicalDrives   (KERNEL32.@)
 */
DWORD WINAPI GetLogicalDrives(void)
{
    DWORD ret = 0;
    int drive;

    for (drive = 0; drive < MAX_DOS_DRIVES; drive++)
    {
        if ( (DRIVE_IsValid(drive)) ||
            (DOSDrives[drive].type == DRIVE_CDROM)) /* audio CD is also valid */
            ret |= (1 << drive);
    }
    return ret;
}


/***********************************************************************
 *           GetVolumeInformationA   (KERNEL32.@)
 */
BOOL WINAPI GetVolumeInformationA( LPCSTR root, LPSTR label,
                                       DWORD label_len, DWORD *serial,
                                       DWORD *filename_len, DWORD *flags,
                                       LPSTR fsname, DWORD fsname_len )
{
    int drive;
    char *cp;

    /* FIXME, SetLastError()s missing */

    if (!root) drive = DRIVE_GetCurrentDrive();
    else
    {
        if ((root[1]) && (root[1] != ':'))
        {
            WARN("invalid root '%s'\n",root);
            return FALSE;
        }
        drive = toupper(root[0]) - 'A';
    }
    if (!DRIVE_IsValid( drive )) return FALSE;
    if (label)
    {
       lstrcpynA( label, DRIVE_GetLabel(drive), label_len );
       cp = label + strlen(label);
       while (cp != label && *(cp-1) == ' ') cp--;
       *cp = '\0';
    }
    if (serial) *serial = DRIVE_GetSerialNumber(drive);

    /* Set the filesystem information */
    /* Note: we only emulate a FAT fs at present */

    if (filename_len) {
    	if (DOSDrives[drive].flags & DRIVE_SHORT_NAMES)
	    *filename_len = 12;
 	else if (DOSDrives[drive].type == DRIVE_CDROM)
	    *filename_len = 221;
	else
	    *filename_len = 255;
    }
    if (flags)
      {
         OSVERSIONINFOA ovi;

       *flags=0;

       /* Should this be instead done via a separate FS type in
          DRIVE_Types? Needs further examination of the implications first */
       if (DOSDrives[drive].type == DRIVE_CDROM)
          *flags |= FILE_CASE_SENSITIVE_SEARCH;
       else
       {
          if (DOSDrives[drive].flags & DRIVE_CASE_SENSITIVE)
             *flags|=FILE_CASE_SENSITIVE_SEARCH;
          if (DOSDrives[drive].flags & DRIVE_CASE_PRESERVING)
             *flags|=FILE_CASE_PRESERVED_NAMES;
       }

       ovi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOA);
       GetVersionExA (&ovi);
       if ((ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) &&
           ((ovi.dwMajorVersion > 5) ||
            ((ovi.dwMajorVersion == 5) && (ovi.dwMinorVersion >= 1))))
       {
          if (DOSDrives[drive].type == DRIVE_CDROM)
             *flags |= FILE_READ_ONLY_VOLUME;
       }
      }
    if (fsname) {
    	/* Diablo checks that return code ... */
        if (DOSDrives[drive].type == DRIVE_CDROM)
	    lstrcpynA( fsname, "CDFS", fsname_len );
	else
	    lstrcpynA( fsname, "FAT", fsname_len );
    }
    return TRUE;
}


/***********************************************************************
 *           GetVolumeInformationW   (KERNEL32.@)
 */
BOOL WINAPI GetVolumeInformationW( LPCWSTR root, LPWSTR label,
                                       DWORD label_len, DWORD *serial,
                                       DWORD *filename_len, DWORD *flags,
                                       LPWSTR fsname, DWORD fsname_len )
{
    LPSTR xroot    = FILE_strdupWtoA( GetProcessHeap(), 0, root );
    LPSTR xvolname = label ? HeapAlloc(GetProcessHeap(),0,label_len) : NULL;
    LPSTR xfsname  = fsname ? HeapAlloc(GetProcessHeap(),0,fsname_len) : NULL;
    BOOL ret = GetVolumeInformationA( xroot, xvolname, label_len, serial,
                                          filename_len, flags, xfsname,
                                          fsname_len );
    if (ret)
    {
        if (label) MultiByteToWideChar( CP_ACP, 0, xvolname, -1, label, label_len );
        if (fsname) MultiByteToWideChar( CP_ACP, 0, xfsname, -1, fsname, fsname_len );
    }
    HeapFree( GetProcessHeap(), 0, xroot );
    HeapFree( GetProcessHeap(), 0, xvolname );
    HeapFree( GetProcessHeap(), 0, xfsname );
    return ret;
}

/***********************************************************************
 *           SetVolumeLabelA   (KERNEL32.@)
 */
BOOL WINAPI SetVolumeLabelA( LPCSTR root, LPCSTR volname )
{
    int drive;

    /* FIXME, SetLastErrors missing */

    if (!root) drive = DRIVE_GetCurrentDrive();
    else
    {
        if ((root[1]) && (root[1] != ':'))
        {
            WARN("invalid root '%s'\n",root);
            return FALSE;
        }
        drive = toupper(root[0]) - 'A';
    }
    if (!DRIVE_IsValid( drive )) return FALSE;

    /* some copy protection stuff check this */
    if (DOSDrives[drive].type == DRIVE_CDROM) return FALSE;

    FIXME("(%s,%s),stub!\n", root, volname);
    return TRUE;
}

/***********************************************************************
 *           SetVolumeLabelW   (KERNEL32.@)
 */
BOOL WINAPI SetVolumeLabelW(LPCWSTR rootpath,LPCWSTR volname)
{
    LPSTR xroot, xvol;
    BOOL ret;

    xroot = FILE_strdupWtoA( GetProcessHeap(), 0, rootpath);
    xvol = HEAP_strdupWtoA( GetProcessHeap(), 0, volname);
    ret = SetVolumeLabelA( xroot, xvol );
    HeapFree( GetProcessHeap(), 0, xroot );
    HeapFree( GetProcessHeap(), 0, xvol );
    return ret;
}


/***********************************************************************
 *           QueryDosDeviceA   (KERNEL32.@)
 *
 * returns array of strings terminated by \0, terminated by \0
 */
DWORD WINAPI QueryDosDeviceA(LPCSTR devname,LPSTR target,DWORD bufsize)
{
    LPSTR s;
    char  buffer[200];

    TRACE("(%s,...)\n", devname ? devname : "<null>");
    if (!devname) {
	/* return known MSDOS devices */
        static const char devices[24] = "CON\0COM1\0COM2\0LPT1\0NUL\0\0";
        memcpy( target, devices, min(bufsize,sizeof(devices)) );
        return min(bufsize,sizeof(devices));
    }
    /* In theory all that are possible and have been defined.
     * Now just those below, since mirc uses it to check for special files.
     *
     * (It is more complex, and supports netmounted stuff, and \\.\ stuff,
     *  but currently we just ignore that.)
     */
#define CHECK(x) (strstr(devname,#x)==devname)
    if (CHECK(con) || CHECK(com) || CHECK(lpt) || CHECK(nul)) {
	strcpy(buffer,"\\DEV\\");
	strcat(buffer,devname);
	if ((s=strchr(buffer,':'))) *s='\0';
	lstrcpynA(target,buffer,bufsize);
	return strlen(buffer)+1;
    } else {
       if ((strlen (devname) == 2) && (devname[1] == ':'))
       {
          int drive = toupper (devname[0]) - 'A';
          if (DRIVE_IsValid (drive))
          {
             size_t len;

             if (bufsize < (strlen (DOSDrives[drive].dos_device_name) + 2))
             {
                SetLastError (ERROR_INSUFFICIENT_BUFFER);
                return 0;
             }

             strcpy (target, DOSDrives[drive].dos_device_name);

             /* Needs to end in a double-null */
             len = strlen (target);
             target[len + 1] = '\0';
             return len + 2;
          }
       }

	if (strchr(devname,':') || devname[0]=='\\') {
	    /* This might be a DOS device we do not handle yet ... */
	    FIXME("(%s) not detected as DOS device!\n",devname);
	}
	SetLastError(ERROR_DEV_NOT_EXIST);
	return 0;
    }

}


/***********************************************************************
 *           QueryDosDeviceW   (KERNEL32.@)
 *
 * returns array of strings terminated by \0, terminated by \0
 */
DWORD WINAPI QueryDosDeviceW(LPCWSTR devname,LPWSTR target,DWORD bufsize)
{
    LPSTR devnameA = devname?HEAP_strdupWtoA(GetProcessHeap(),0,devname):NULL;
    LPSTR targetA = (LPSTR)HeapAlloc(GetProcessHeap(),0,bufsize);
    DWORD ret = QueryDosDeviceA(devnameA,targetA,bufsize);

    ret = MultiByteToWideChar( CP_ACP, 0, targetA, ret, target, bufsize );
    if (devnameA) HeapFree(GetProcessHeap(),0,devnameA);
    if (targetA) HeapFree(GetProcessHeap(),0,targetA);
    return ret;
}


/***********************************************************************
 *           GetVolumePathNameA   (KERNEL32.@)
 */
BOOL WINAPI GetVolumePathNameA( LPCSTR lpszFileName, LPSTR lpszVolumePathName, DWORD cchBufferLength )
{
    char fullpath[MAX_PATH];
    DWORD fullpathsize = 0;
    BOOL retval = TRUE;
    BOOL returnRoot = FALSE;
    
    TRACE("FileName: %s, OutputPathPtr=%p, bufferLength = %lu\n", debugstr_a(lpszFileName), lpszVolumePathName, cchBufferLength );
    FIXME("Semi-stub!  Much unsupported behavior\n");
    
    fullpathsize = GetFullPathNameA(lpszFileName, MAX_PATH, fullpath, NULL);
    if ((fullpathsize >= MAX_PATH) || (cchBufferLength < 3))
    {
        SetLastError (ERROR_MORE_DATA);
        return FALSE;
    }

    /* If GetFullPathName failed, fill the buffer with the boot volume name */
    if (fullpathsize == 0)
    {
        returnRoot = TRUE;
    }
    
    if (fullpath[1] != ':')
    {
        FIXME("UNC and other format paths are not supported yet!\n");
        returnRoot = TRUE;
    }
    
    if (returnRoot)
    {
        fullpath[0] = 'C';
        fullpath[1] = ':';
        fullpath[2] = '\\';
        fullpath[3] = 0;            
    }
    
    lpszVolumePathName[0] = fullpath[0];
    lpszVolumePathName[1] = fullpath[1];
    if (cchBufferLength > 3)
    {
        lpszVolumePathName[2] = fullpath[2];
        lpszVolumePathName[3] = 0;
    }
    else
    {
        lpszVolumePathName[2] = 0;
    }    

    TRACE("Final Volume Pathname: %s\n", debugstr_a(lpszVolumePathName) );
    
    return retval;        
}
 
/***********************************************************************
 *           GetVolumePathNameW   (KERNEL32.@)
 */
BOOL WINAPI GetVolumePathNameW( LPCWSTR lpszFileName, LPWSTR lpszVolumePathName, DWORD cchBufferLength )
{
    WCHAR fullpath[MAX_PATH];
    DWORD fullpathsize = 0;
    BOOL retval = TRUE;
    BOOL returnRoot = FALSE;
    
    TRACE("FileName: %s, OutputPathPtr=%p, bufferLength = %lu\n", debugstr_w(lpszFileName), lpszVolumePathName, cchBufferLength );
    FIXME("Semi-stub!  Much unsupported behavior\n");
    
    fullpathsize = GetFullPathNameW(lpszFileName, MAX_PATH, fullpath, NULL);
    if ((fullpathsize >= MAX_PATH) || (cchBufferLength < 3))
    {
        SetLastError (ERROR_MORE_DATA);
        return FALSE;
    }

    /* If GetFullPathName failed, fill the buffer with the boot volume name */
    if (fullpathsize == 0)
    {
        returnRoot = TRUE;
    }
    
    if (fullpath[1] != ':')
    {
        FIXME("UNC and other format paths are not supported yet!\n");
        returnRoot = TRUE;
    }
    
    if (returnRoot)
    {
        fullpath[0] = 'C';
        fullpath[1] = ':';
        fullpath[2] = '\\';
        fullpath[3] = 0;            
    }
    
    lpszVolumePathName[0] = fullpath[0];
    lpszVolumePathName[1] = fullpath[1];
    if (cchBufferLength > 3)
    {
        lpszVolumePathName[2] = fullpath[2];
        lpszVolumePathName[3] = 0;
    }
    else
    {
        lpszVolumePathName[2] = 0;
    }

    TRACE("Final Volume Pathname: %s\n", debugstr_w(lpszVolumePathName) );
    
    return retval;        
}
 
