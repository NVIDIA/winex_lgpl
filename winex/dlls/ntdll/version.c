/*
 * Windows and DOS version functions
 *
 * Copyright 1997 Alexandre Julliard
 * Copyright 1997 Marcus Meissner
 * Copyright 1998 Patrik Stridvall
 * Copyright 1998 Andreas Mohr
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/winbase16.h"
#include "winreg.h"
#include "module.h"
#include "options.h"
#include "wine/debug.h"
#include "winerror.h"

WINE_DEFAULT_DEBUG_CHANNEL(ver);

typedef enum
{
    WIN20,   /* Windows 2.0 */
    WIN30,   /* Windows 3.0 */
    WIN31,   /* Windows 3.1 */
    WIN95,   /* Windows 95 */
    WIN98,   /* Windows 98 */
    WINME,   /* Windows Me */
    NT351,   /* Windows NT 3.51 */
    NT40,    /* Windows NT 4.0 */
    NT2K,    /* Windows 2000 */
    WINXP,   /* Windows XP */
    NB_WINDOWS_VERSIONS
} WINDOWS_VERSION;

typedef struct
{
    LONG             getVersion16;
    LONG             getVersion32;
    const CHAR       getVersionProdName[64];
    OSVERSIONINFOEXA getVersionEx;
} VERSION_DATA;

/* FIXME: compare values below with original and fix.
 * An *excellent* win9x version page (ALL versions !)
 * can be found at members.aol.com/axcel216/ver.htm */
static VERSION_DATA VersionData[NB_WINDOWS_VERSIONS] =
{
    /* WIN20 FIXME: verify values */
    {
	MAKELONG( 0x0002, 0x0303 ), /* assume DOS 3.3 */
	MAKELONG( 0x0002, 0x8000 ),
	"",
	{
            sizeof(OSVERSIONINFOA), 2, 0, 0,
            VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, 0, 0, 0
	}
    },
    /* WIN30 FIXME: verify values */
    {
	MAKELONG( 0x0003, 0x0500 ), /* assume DOS 5.00 */
	MAKELONG( 0x0003, 0x8000 ),
	"",
	{
            sizeof(OSVERSIONINFOA), 3, 0, 0,
            VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, 0, 0, 0
	}
    },
    /* WIN31 */
    {
	MAKELONG( 0x0a03, 0x0616 ), /* DOS 6.22 */
	MAKELONG( 0x0a03, 0x8000 ),
	"",
	{
            sizeof(OSVERSIONINFOA), 3, 10, 0,
            VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, 0, 0, 0
	}
    },
    /* WIN95 */
    {
        0x07005F03,
        0xC0000004,
        "",
        {
            /* Win95:       4, 0, 0x40003B6, ""
             * Win95sp1:    4, 0, 0x40003B6, " A " (according to doc)
             * Win95osr2:   4, 0, 0x4000457, " B " (according to doc)
             * Win95osr2.1: 4, 3, 0x40304BC, " B " (according to doc)
             * Win95osr2.5: 4, 3, 0x40304BE, " C " (according to doc)
             * Win95a/b can be discerned via regkey SubVersionNumber
             * See also:
             * http://support.microsoft.com/support/kb/articles/q158/2/38.asp
             */
            sizeof(OSVERSIONINFOA), 4, 0, 0x40003B6,
            VER_PLATFORM_WIN32_WINDOWS, "", 0, 0, 0, 0, 0
        }
    },
    /* WIN98 (second edition) */
    {
        0x070A5F03,
        0xC0000A04,
        "",
        {
            /* Win98:   4, 10, 0x40A07CE, " "   4.10.1998
             * Win98SE: 4, 10, 0x40A08AE, " A " 4.10.2222
             */
            sizeof(OSVERSIONINFOA), 4, 10, 0x40A07CF, /* XXX */
            VER_PLATFORM_WIN32_WINDOWS, "A", 0, 0, 0, 0, 0
        }
    },
    /* WINME */
    {
        0x08005F03,
        0xC0005A04,
        "",
        {
            sizeof(OSVERSIONINFOA), 4, 90, 0x45A0BB8,
            VER_PLATFORM_WIN32_WINDOWS, " ", 0, 0, 0, 0, 0
        }
    },
    /* NT351 */
    {
        0x05000A03,
        0x04213303,
        "",
        {
            sizeof(OSVERSIONINFOA), 3, 51, 0x421,
            VER_PLATFORM_WIN32_NT, "Service Pack 2", 0, 0, 0, 0, 0
        }
    },
    /* NT40 */
    {
        0x05000A03,
        0x05650004,
        "",
        {
            sizeof(OSVERSIONINFOA), 4, 0, 0x565,
            VER_PLATFORM_WIN32_NT, "Service Pack 6", 0, 0, 0, 0, 0
        }
    },
    /* NT2K */
    {
        0x05005F03,
        0x08930005,
        "Microsoft Windows 2000",
        {
            sizeof(OSVERSIONINFOEXA), 5, 0, 0x893,
            VER_PLATFORM_WIN32_NT, "Service Pack 4", 4, 0, 0,
            VER_NT_WORKSTATION, 0
        }
    },
    /* WINXP */
    {
        0x05005F03, /* Assuming DOS 5 like the other NT */
        0x0A280105,
        "Microsoft Windows XP",
        {
            sizeof(OSVERSIONINFOEXA), 5, 1, 0xA28,
            VER_PLATFORM_WIN32_NT, "Service Pack 2", 2, 0,
            VER_SUITE_SINGLEUSERTS, VER_NT_WORKSTATION, 0
        }
    }
};

static const char *WinVersionNames[NB_WINDOWS_VERSIONS] =
{ /* no spaces in here ! */
    "win20",
    "win30",
    "win31",
    "win95",
    "win98",
    "winme",
    "nt351",
    "nt40",
    "win2000,win2k,nt2k,nt2000",
    "winxp"
};

/* if one of the following dlls is importing ntdll the windows
version autodetection switches wine to unicode (nt 3.51 or 4.0) */
static char * special_dlls[] =
{
	"COMDLG32",
	"COMCTL32",
	"SHELL32",
	"OLE32",
	"RPCRT4",
	NULL
};

/* the current version has not been autodetected but forced via cmdline */
static BOOL versionForced = FALSE, dosversionForced = FALSE;
static WINDOWS_VERSION forcedWinVersion = WIN31;

/**********************************************************************
 *         VERSION_ParseWinVersion
 */
void VERSION_ParseWinVersion( const char *arg )
{
    int i, len;
    const char *pCurr, *p;
    for (i = 0; i < NB_WINDOWS_VERSIONS; i++)
    {
        pCurr = WinVersionNames[i];
        /* iterate through all winver aliases separated by comma */
        do {
            p = strchr(pCurr, ',');
            len = p ? (int)p - (int)pCurr : strlen(pCurr);
            if ( (!strncmp( pCurr, arg, len )) && (arg[len] == '\0') )
            {
                forcedWinVersion = (WINDOWS_VERSION)i;
                versionForced = TRUE;
                return;
            }
            pCurr = p+1;
        } while (p);
    }
    MESSAGE("Invalid winver value '%s' specified.\n", arg );
    MESSAGE("Valid versions are:" );
    for (i = 0; i < NB_WINDOWS_VERSIONS; i++)
    {
        /* only list the first, "official" alias in case of aliases */
        pCurr = WinVersionNames[i];
        p = strchr(pCurr, ',');
        len = (p) ? (int)p - (int)pCurr : strlen(pCurr);

        MESSAGE(" '%.*s'%c", len, pCurr,
                (i == NB_WINDOWS_VERSIONS - 1) ? '\n' : ',' );
    }
    ExitProcess(1);
}


/**********************************************************************
 *         VERSION_ParseDosVersion
 */
void VERSION_ParseDosVersion( const char *arg )
{
    int hi, lo;
    if (sscanf( arg, "%d.%d", &hi, &lo ) == 2)
    {
        VersionData[WIN31].getVersion16 =
            MAKELONG(LOWORD(VersionData[WIN31].getVersion16),
                     (hi<<8) + lo);
        dosversionForced = TRUE;
    }
    else
    {
        MESSAGE("--dosver: Wrong version format. Use \"--dosver x.xx\"\n");
        ExitProcess(1);
    }
}

/**********************************************************************
 *	VERSION_GetSystemDLLVersion
 *
 * This function tries to figure out if a given (native) dll comes from
 * win95/98 or winnt. Since all values in the OptionalHeader are not a
 * usable hint, we test if a dll imports the ntdll.
 * This is at least working for all system dlls like comctl32, comdlg32 and
 * shell32.
 * If you have a better idea to figure this out...
 */
static DWORD VERSION_GetSystemDLLVersion( HMODULE hmod )
{
    IMAGE_DATA_DIRECTORY *dir = PE_HEADER(hmod)->OptionalHeader.DataDirectory
                                + IMAGE_DIRECTORY_ENTRY_IMPORT;
    if (dir->Size && dir->VirtualAddress)
    {
        IMAGE_IMPORT_DESCRIPTOR *pe_imp = (IMAGE_IMPORT_DESCRIPTOR *)((char *)hmod + dir->VirtualAddress);
        for ( ; pe_imp->Name; pe_imp++)
        {
	    char * name = (char *)hmod + (unsigned int)pe_imp->Name;
	    TRACE("%s\n", name);

	    if (!strncasecmp(name, "ntdll", 5))
	    {
	      switch(PE_HEADER(hmod)->OptionalHeader.MajorOperatingSystemVersion) {
		  case 3:
			  MESSAGE("WARNING: very old native DLL (NT 3.x) used, might cause instability.\n");
			  return NT351;
		  case 4: return NT40;
		  case 5: return NT2K;
		  case 6: return WINXP;
		  default:
			  FIXME("Unknown DLL OS version, please report !!\n");
			  return WINXP;
	      }
	    }
        }
    }
    return WIN95;
}
/**********************************************************************
 *	VERSION_GetLinkedDllVersion
 *
 * Some version data (not reliable!):
 * linker/OS/image/subsys
 *
 * x.xx/1.00/0.00/3.10	Win32s		(any version ?)
 * 2.39/1.00/0.00/3.10	Win32s		freecell.exe (any version)
 * 2.50/1.00/4.00/4.00	Win32s 1.30	winhlp32.exe
 * 2.60/3.51/3.51/3.51	NT351SP5	system dlls
 * 2.60/3.51/3.51/4.00	NT351SP5	comctl32 dll
 * 2.xx/1.00/0.00/4.00	Win95 		system files
 * x.xx/4.00/0.00/4.00	Win95		most applications
 * 3.10/4.00/0.00/4.00	Win98		notepad
 * x.xx/5.00/5.00/4.00	Win98 		system dlls (e.g. comctl32.dll)
 * x.xx/4.00/4.00/4.00	NT 4 		most apps
 * 5.12/5.00/5.00/4.00	NT4+IE5		comctl32.dll
 * 5.12/5.00/5.00/4.00	Win98		calc
 * x.xx/5.00/5.00/4.00	win95/win98/NT4	IE5 files
 */
DWORD VERSION_GetLinkedDllVersion(void)
{
	WINE_MODREF *wm;
	DWORD WinVersion = NB_WINDOWS_VERSIONS;
	PIMAGE_OPTIONAL_HEADER ophd;

	/* First check the native dlls provided. These have to be
	from one windows version */
	for ( wm = MODULE_modref_list; wm; wm=wm->next )
	{
	  ophd = &(PE_HEADER(wm->module)->OptionalHeader);

	  TRACE("%s: %02x.%02x/%02x.%02x/%02x.%02x/%02x.%02x\n",
	    wm->modname,
	    ophd->MajorLinkerVersion, ophd->MinorLinkerVersion,
	    ophd->MajorOperatingSystemVersion, ophd->MinorOperatingSystemVersion,
	    ophd->MajorImageVersion, ophd->MinorImageVersion,
	    ophd->MajorSubsystemVersion, ophd->MinorSubsystemVersion);

	  /* test if it is an external (native) dll */
	  if (!(wm->flags & WINE_MODREF_INTERNAL))
	  {
	    int i;
	    for (i = 0; special_dlls[i]; i++)
	    {
	      /* test if it is a special dll */
	      if (!strncasecmp(wm->modname, special_dlls[i], strlen(special_dlls[i]) ))
	      {
	        DWORD DllVersion = VERSION_GetSystemDLLVersion(wm->module);
	        if (WinVersion == NB_WINDOWS_VERSIONS)
	          WinVersion = DllVersion;
	        else {
	          if (WinVersion != DllVersion) {
	            ERR("You mixed system DLLs from different windows versions! Expect a crash! (%s: expected version '%s', but is '%s')\n",
			wm->modname,
			VersionData[WinVersion].getVersionEx.szCSDVersion,
			VersionData[DllVersion].getVersionEx.szCSDVersion);
	            return WIN20; /* this may let the exe exiting */
	          }
	        }
	        break;
	      }
	    }
	  }
	}

	if(WinVersion != NB_WINDOWS_VERSIONS) return WinVersion;

	/* we are using no external system dlls, look at the exe */
	ophd = &(PE_HEADER(GetModuleHandleA(NULL))->OptionalHeader);

	TRACE("%02x.%02x/%02x.%02x/%02x.%02x/%02x.%02x\n",
	    ophd->MajorLinkerVersion, ophd->MinorLinkerVersion,
	    ophd->MajorOperatingSystemVersion, ophd->MinorOperatingSystemVersion,
	    ophd->MajorImageVersion, ophd->MinorImageVersion,
	    ophd->MajorSubsystemVersion, ophd->MinorSubsystemVersion);

	/* special nt 3.51 */
	if (3 == ophd->MajorOperatingSystemVersion && 51 == ophd->MinorOperatingSystemVersion)
	{
	    return NT351;
	}

	/* the MajorSubsystemVersion is the only usable sign */
	if (ophd->MajorSubsystemVersion < 4)
	{
	  if ( ophd->MajorOperatingSystemVersion == 1
	    && ophd->MinorOperatingSystemVersion == 0)
	  {
	    return WIN31; /* win32s */
	  }

	  if (ophd->Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
	    return NT351; /* FIXME: NT 3.1, not tested */
	  else
	    return WIN95;
	}

	return WIN95;
}


/***********************************************************************
 *		get_config_key
 *
 * Get a config key from either the app-specific or the default config
 */
inline static DWORD get_config_key( HKEY defkey, HKEY appkey, const char *name,
                                    char *buffer, DWORD size )
{
    if (appkey && !RegQueryValueExA( appkey, name, 0, NULL, (LPBYTE)buffer,
                                     &size )) return 0;
    return RegQueryValueExA( defkey, name, 0, NULL, (LPBYTE)buffer, &size );
}


/***********************************************************************
 *    create_version_reg_entries
 */

static void create_version_reg_entries (WINDOWS_VERSION ver)
{
    HKEY hKey;
    char buf[256];
    OSVERSIONINFOEXA *VD = &VersionData[ver].getVersionEx;

    if (ver < NT2K)
    {
        FIXME ("Need to fill in pre-win2k registry entries for OS version\n");
        return;
    }

    if (RegCreateKeyExA (HKEY_LOCAL_MACHINE,
                         "Software\\Microsoft\\Windows NT\\CurrentVersion",
                         0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL))
    {
        ERR ("Unable to create registry key SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\n");
        return;
    }

    sprintf (buf, "%lu.%lu", VD->dwMajorVersion, VD->dwMinorVersion);
    if (RegSetValueExA (hKey, "CurrentVersion", 0, REG_SZ, buf,
                        strlen (buf) + 1))
        ERR ("Unable to create CurrentVersion value\n");

    sprintf (buf, "%lu", VD->dwBuildNumber);
    if (RegSetValueExA (hKey, "CurrentBuildNumber", 0, REG_SZ, buf,
                        strlen (buf) + 1))
        ERR ("Unable to create CurrentBuildNumber value\n");

    if (RegSetValueExA (hKey, "CSDVersion", 0, REG_SZ, VD->szCSDVersion,
                        strlen (VD->szCSDVersion) + 1))
        ERR ("Unable to create CSDVersion value\n");

    if (RegSetValueExA (hKey, "ProductName", 0, REG_SZ,
                        VersionData[ver].getVersionProdName,
                        strlen (VersionData[ver].getVersionProdName) + 1))
        ERR ("Unable to create ProductName value\n");

    RegCloseKey (hKey);
}


/***********************************************************************
 *		setup_version
 */
static void setup_version(void)
{
    char buffer[MAX_PATH+16];
    HKEY hkey, appkey = 0;
    static BOOL done = FALSE;
    DWORD error;
    
    if (done) return;
    done = TRUE;

    if (RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Version", 0, NULL,
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
        strcat( appname, "\\Version" );
        if (!RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\AppDefaults", &tmpkey ))
        {
            if (RegOpenKeyA( tmpkey, appname, &appkey )) appkey = 0;
            RegCloseKey( tmpkey );
        }
    }

    else
        ERR("could not retrieve the module file name (reason: '%s')\n", error == 0 ? "bad module" : "buffer too small");


    if (!versionForced && !get_config_key( hkey, appkey, "Windows", buffer, sizeof(buffer) ))
    {
        /* I picked this size since that is the one used in OPTIONS_ParseOptions() */
	char wineoptions[1024]; 


	/* Set the windows version to emulate */
        VERSION_ParseWinVersion( buffer );

	/* Get the WINEOPTIONS env.variable passed in from parent processes, if any... */
	if (!GetEnvironmentVariableA( "WINEOPTIONS", wineoptions, sizeof(wineoptions) ) )
	{
	    wineoptions[0]='\0';
        } 

	/* construct "-winver <givenver>" out of what was passed in, being sure not to overshoot
	 * the length of our buffer, and append it to whatever options might already exist */
	strncat(wineoptions, " -winver ", sizeof(wineoptions) - strlen(wineoptions) );
	strncat(wineoptions, buffer, sizeof(wineoptions) - strlen(wineoptions) );

	/* We're done, Set WINEOPTIONS environment variable up */
        SetEnvironmentVariableA( "WINEOPTIONS", wineoptions);
    }

    if (!dosversionForced && !get_config_key( hkey, appkey, "DOS", buffer, sizeof(buffer) ))
    {
        VERSION_ParseDosVersion( buffer );
    }

    if (appkey) RegCloseKey( appkey );
    RegCloseKey( hkey );
}


/**********************************************************************
 *         VERSION_GetVersion
 *
 * WARNING !!!
 * Don't call this function too early during the Wine init,
 * as pdb->exe_modref (required by VERSION_GetImageVersion()) might still
 * be NULL in such cases, which causes the winver to ALWAYS be detected
 * as WIN31.
 * And as we cache the winver once it has been determined, this is bad.
 * This can happen much easier than you might think, as this function
 * is called by EVERY GetVersion*() API !
 *
 */
static WINDOWS_VERSION VERSION_GetVersion(void)
{
    static WORD winver = 0xffff;

    if (winver == 0xffff) /* to be determined */
    {
        WINDOWS_VERSION retver;

        setup_version();
        if (versionForced) /* user has overridden any sensible checks */
	    winver = forcedWinVersion;
	else
	{
	    retver = VERSION_GetLinkedDllVersion();

	    /* cache determined value, but do not store in case of WIN31 */
	    if (retver != WIN31) winver = retver;

	    return retver;
	}

        create_version_reg_entries (winver);
    }
    return winver;
}


/***********************************************************************
 *         GetVersion   (KERNEL.3)
 */
LONG WINAPI GetVersion16(void)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    
    TRACE( "\n" );

    return VersionData[ver].getVersion16;
}


/***********************************************************************
 *         GetVersion   (KERNEL32.@)
 */
LONG WINAPI GetVersion(void)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    
    TRACE( "\n" );

    return VersionData[ver].getVersion32;
}


/***********************************************************************
 *         GetVersionEx   (KERNEL.149)
 */
BOOL16 WINAPI GetVersionEx16(OSVERSIONINFO16 *v)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    
    TRACE( "\n" );

    if (v->dwOSVersionInfoSize < sizeof(OSVERSIONINFO16))
    {
        WARN("wrong OSVERSIONINFO size from app\n");
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    v->dwMajorVersion = VersionData[ver].getVersionEx.dwMajorVersion;
    v->dwMinorVersion = VersionData[ver].getVersionEx.dwMinorVersion;
    v->dwBuildNumber  = VersionData[ver].getVersionEx.dwBuildNumber;
    v->dwPlatformId   = VersionData[ver].getVersionEx.dwPlatformId;
    strcpy( v->szCSDVersion, VersionData[ver].getVersionEx.szCSDVersion );
    return TRUE;
}


/***********************************************************************
 *         GetVersionExA   (KERNEL32.@)
 */
BOOL WINAPI GetVersionExA(OSVERSIONINFOA *v)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    OSVERSIONINFOEXA *VD = &VersionData[ver].getVersionEx;
    
    TRACE( "\n" );

    if ((v->dwOSVersionInfoSize != sizeof(OSVERSIONINFOA)) &&
        (v->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXA)))
    {
        WARN("wrong OSVERSIONINFO size from app (got: %ld, expected: %d or %d)\n",
             v->dwOSVersionInfoSize, sizeof(OSVERSIONINFOA),
             sizeof (OSVERSIONINFOEXA));
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    v->dwMajorVersion = VD->dwMajorVersion;
    v->dwMinorVersion = VD->dwMinorVersion;
    v->dwBuildNumber  = VD->dwBuildNumber;
    v->dwPlatformId   = VD->dwPlatformId;
    strcpy( v->szCSDVersion, VD->szCSDVersion );

    if ((v->dwOSVersionInfoSize >= sizeof (OSVERSIONINFOEXA)) &&
        (VD->dwOSVersionInfoSize == sizeof (OSVERSIONINFOEXA)))
    {
       LPOSVERSIONINFOEXA vex = (LPOSVERSIONINFOEXA)v;
       vex->wServicePackMajor = VD->wServicePackMajor;
       vex->wServicePackMinor = VD->wServicePackMinor;
       vex->wSuiteMask = VD->wSuiteMask;
       vex->wProductType = VD->wProductType;
       vex->wReserved = VD->wReserved;
    }

    return TRUE;
}


/***********************************************************************
 *         GetVersionExW   (KERNEL32.@)
 */
BOOL WINAPI GetVersionExW(OSVERSIONINFOW *v)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    OSVERSIONINFOEXA *VD = &VersionData[ver].getVersionEx;
    
    TRACE( "\n" );

    if ((v->dwOSVersionInfoSize != sizeof(OSVERSIONINFOW)) &&
        (v->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXW)))
    {
        WARN("wrong OSVERSIONINFO size from app (got: %ld, expected: %d or %d)\n",
             v->dwOSVersionInfoSize, sizeof(OSVERSIONINFOW),
             sizeof (OSVERSIONINFOEXW));
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    v->dwMajorVersion = VD->dwMajorVersion;
    v->dwMinorVersion = VD->dwMinorVersion;
    v->dwBuildNumber  = VD->dwBuildNumber;
    v->dwPlatformId   = VD->dwPlatformId;
    MultiByteToWideChar( CP_ACP, 0, VD->szCSDVersion, -1,
                         v->szCSDVersion,
                         sizeof(v->szCSDVersion)/sizeof(WCHAR) );

    if ((v->dwOSVersionInfoSize >= sizeof (OSVERSIONINFOEXW)) &&
        (VD->dwOSVersionInfoSize == sizeof (OSVERSIONINFOEXA)))
    {
       LPOSVERSIONINFOEXW vex = (LPOSVERSIONINFOEXW)v;
       vex->wServicePackMajor = VD->wServicePackMajor;
       vex->wServicePackMinor = VD->wServicePackMinor;
       vex->wSuiteMask = VD->wSuiteMask;
       vex->wProductType = VD->wProductType;
       vex->wReserved = VD->wReserved;
    }

    return TRUE;
}


/******************************************************************************
 *        VerifyVersionInfoA   (KERNEL32.@)
 */
BOOL WINAPI VerifyVersionInfoA( LPOSVERSIONINFOEXA lpVersionInfo, DWORD dwTypeMask,
                                DWORDLONG dwlConditionMask)
{
    OSVERSIONINFOEXW verW;

    verW.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    verW.dwMajorVersion = lpVersionInfo->dwMajorVersion;
    verW.dwMinorVersion = lpVersionInfo->dwMinorVersion;
    verW.dwBuildNumber = lpVersionInfo->dwBuildNumber;
    verW.dwPlatformId = lpVersionInfo->dwPlatformId;
    verW.wServicePackMajor = lpVersionInfo->wServicePackMajor;
    verW.wServicePackMinor = lpVersionInfo->wServicePackMinor;
    verW.wSuiteMask = lpVersionInfo->wSuiteMask;
    verW.wProductType = lpVersionInfo->wProductType;
    verW.wReserved = lpVersionInfo->wReserved;

    return VerifyVersionInfoW(&verW, dwTypeMask, dwlConditionMask);
}


/******************************************************************************
 *        VerifyVersionInfoW   (KERNEL32.@)
 */
BOOL WINAPI VerifyVersionInfoW( LPOSVERSIONINFOEXW lpVersionInfo, DWORD dwTypeMask,
                                DWORDLONG dwlConditionMask)
{
    OSVERSIONINFOEXW ver;
    BOOL res, error_set;

    FIXME("(%p,%lu,%llx): Not all cases correctly implemented yet\n", lpVersionInfo, dwTypeMask, dwlConditionMask);
    /* FIXME:
	- Check the following special case on Windows (various versions):
	  o lp->wSuiteMask == 0 and ver.wSuiteMask != 0 and VER_AND/VER_OR
	  o lp->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXW)
	- MSDN talks about some tests being impossible. Check what really happens.
     */

    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    if(!GetVersionExW((LPOSVERSIONINFOW) &ver))
	return FALSE;

    res = TRUE;
    error_set = FALSE;
    if(!(dwTypeMask && dwlConditionMask)) {
	res = FALSE;
	SetLastError(ERROR_BAD_ARGUMENTS);
	error_set = TRUE;
    }
    if(dwTypeMask & VER_PRODUCT_TYPE)
	switch(dwlConditionMask >> 7*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.wProductType != lpVersionInfo->wProductType)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.wProductType <= lpVersionInfo->wProductType)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.wProductType < lpVersionInfo->wProductType)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.wProductType >= lpVersionInfo->wProductType)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.wProductType > lpVersionInfo->wProductType)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_SUITENAME && res)
	switch(dwlConditionMask >> 6*3 & 0x07) {
	    case VER_AND:
		if((lpVersionInfo->wSuiteMask & ver.wSuiteMask) != lpVersionInfo->wSuiteMask)
		    res = FALSE;
		break;
	    case VER_OR:
		if(!(lpVersionInfo->wSuiteMask & ver.wSuiteMask) && lpVersionInfo->wSuiteMask)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_PLATFORMID && res)
	switch(dwlConditionMask >> 3*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.dwPlatformId != lpVersionInfo->dwPlatformId)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.dwPlatformId <= lpVersionInfo->dwPlatformId)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.dwPlatformId < lpVersionInfo->dwPlatformId)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.dwPlatformId >= lpVersionInfo->dwPlatformId)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.dwPlatformId > lpVersionInfo->dwPlatformId)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_BUILDNUMBER && res)
	switch(dwlConditionMask >> 2*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.dwBuildNumber != lpVersionInfo->dwBuildNumber)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.dwBuildNumber <= lpVersionInfo->dwBuildNumber)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.dwBuildNumber < lpVersionInfo->dwBuildNumber)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.dwBuildNumber >= lpVersionInfo->dwBuildNumber)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.dwBuildNumber > lpVersionInfo->dwBuildNumber)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_MAJORVERSION && res)
	switch(dwlConditionMask >> 1*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.dwMajorVersion != lpVersionInfo->dwMajorVersion)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.dwMajorVersion <= lpVersionInfo->dwMajorVersion)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.dwMajorVersion < lpVersionInfo->dwMajorVersion)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.dwMajorVersion >= lpVersionInfo->dwMajorVersion)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.dwMajorVersion > lpVersionInfo->dwMajorVersion)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_MINORVERSION && res)
	switch(dwlConditionMask >> 0*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.dwMinorVersion != lpVersionInfo->dwMinorVersion)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.dwMinorVersion <= lpVersionInfo->dwMinorVersion)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.dwMinorVersion < lpVersionInfo->dwMinorVersion)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.dwMinorVersion >= lpVersionInfo->dwMinorVersion)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.dwMinorVersion > lpVersionInfo->dwMinorVersion)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_SERVICEPACKMAJOR && res)
	switch(dwlConditionMask >> 5*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.wServicePackMajor != lpVersionInfo->wServicePackMajor)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.wServicePackMajor <= lpVersionInfo->wServicePackMajor)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.wServicePackMajor < lpVersionInfo->wServicePackMajor)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.wServicePackMajor >= lpVersionInfo->wServicePackMajor)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.wServicePackMajor > lpVersionInfo->wServicePackMajor)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}
    if(dwTypeMask & VER_SERVICEPACKMINOR && res)
	switch(dwlConditionMask >> 4*3 & 0x07) {
	    case VER_EQUAL:
		if(ver.wServicePackMinor != lpVersionInfo->wServicePackMinor)
		    res = FALSE;
		break;
	    case VER_GREATER:
		if(ver.wServicePackMinor <= lpVersionInfo->wServicePackMinor)
		    res = FALSE;
		break;
	    case VER_GREATER_EQUAL:
		if(ver.wServicePackMinor < lpVersionInfo->wServicePackMinor)
		    res = FALSE;
		break;
	    case VER_LESS:
		if(ver.wServicePackMinor >= lpVersionInfo->wServicePackMinor)
		    res = FALSE;
		break;
	    case VER_LESS_EQUAL:
		if(ver.wServicePackMinor > lpVersionInfo->wServicePackMinor)
		    res = FALSE;
		break;
	    default:
	        res = FALSE;
		SetLastError(ERROR_BAD_ARGUMENTS);
		error_set = TRUE;
	}

    if(!(res || error_set))
	SetLastError(ERROR_OLD_WIN_VERSION);
    return res;
}


/***********************************************************************
 *	    GetWinFlags   (KERNEL.132)
 */
DWORD WINAPI GetWinFlags16(void)
{
  static const long cpuflags[5] =
    { WF_CPU086, WF_CPU186, WF_CPU286, WF_CPU386, WF_CPU486 };
  SYSTEM_INFO si;
  OSVERSIONINFOA ovi;
  DWORD result;

  GetSystemInfo(&si);

  /* There doesn't seem to be any Pentium flag.  */
  result = cpuflags[min(si.wProcessorLevel, 4)] | WF_ENHANCED | WF_PMODE | WF_80x87 | WF_PAGING;
  if (si.wProcessorLevel >= 4) result |= WF_HASCPUID;
  ovi.dwOSVersionInfoSize = sizeof(ovi);
  GetVersionExA(&ovi);
  if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT)
      result |= WF_WIN32WOW; /* undocumented WF_WINNT */
  return result;
}


#if 0
/* Not used at this time. This is here for documentation only */

/* WINDEBUGINFO flags values */
#define WDI_OPTIONS         0x0001
#define WDI_FILTER          0x0002
#define WDI_ALLOCBREAK      0x0004

/* dwOptions values */
#define DBO_CHECKHEAP       0x0001
#define DBO_BUFFERFILL      0x0004
#define DBO_DISABLEGPTRAPPING 0x0010
#define DBO_CHECKFREE       0x0020

#define DBO_SILENT          0x8000

#define DBO_TRACEBREAK      0x2000
#define DBO_WARNINGBREAK    0x1000
#define DBO_NOERRORBREAK    0x0800
#define DBO_NOFATALBREAK    0x0400
#define DBO_INT3BREAK       0x0100

/* DebugOutput flags values */
#define DBF_TRACE           0x0000
#define DBF_WARNING         0x4000
#define DBF_ERROR           0x8000
#define DBF_FATAL           0xc000

/* dwFilter values */
#define DBF_KERNEL          0x1000
#define DBF_KRN_MEMMAN      0x0001
#define DBF_KRN_LOADMODULE  0x0002
#define DBF_KRN_SEGMENTLOAD 0x0004
#define DBF_USER            0x0800
#define DBF_GDI             0x0400
#define DBF_MMSYSTEM        0x0040
#define DBF_PENWIN          0x0020
#define DBF_APPLICATION     0x0008
#define DBF_DRIVER          0x0010

#endif /* NOLOGERROR */


/***********************************************************************
 *	    GetWinDebugInfo   (KERNEL.355)
 */
BOOL16 WINAPI GetWinDebugInfo16(WINDEBUGINFO16 *lpwdi, UINT16 flags)
{
    FIXME("(%8lx,%d): stub returning 0\n",
	  (unsigned long)lpwdi, flags);
    /* 0 means not in debugging mode/version */
    /* Can this type of debugging be used in wine ? */
    /* Constants: WDI_OPTIONS WDI_FILTER WDI_ALLOCBREAK */
    return 0;
}


/***********************************************************************
 *	    SetWinDebugInfo   (KERNEL.356)
 */
BOOL16 WINAPI SetWinDebugInfo16(WINDEBUGINFO16 *lpwdi)
{
    FIXME("(%8lx): stub returning 0\n", (unsigned long)lpwdi);
    /* 0 means not in debugging mode/version */
    /* Can this type of debugging be used in wine ? */
    /* Constants: WDI_OPTIONS WDI_FILTER WDI_ALLOCBREAK */
    return 0;
}


/***********************************************************************
 *           K329                    (KERNEL.329)
 *
 * TODO:
 * Should fill lpBuffer only if DBO_BUFFERFILL has been set by SetWinDebugInfo()
 */
void WINAPI DebugFillBuffer(LPSTR lpBuffer, WORD wBytes)
{
	memset(lpBuffer, DBGFILL_BUFFER, wBytes);
}

/***********************************************************************
 *           DiagQuery                          (KERNEL.339)
 *
 * returns TRUE if Win called with "/b" (bootlog.txt)
 */
BOOL16 WINAPI DiagQuery16()
{
	/* perhaps implement a Wine "/b" command line flag sometime ? */
	return FALSE;
}

/***********************************************************************
 *           DiagOutput                         (KERNEL.340)
 *
 * writes a debug string into <windir>\bootlog.txt
 */
void WINAPI DiagOutput16(LPCSTR str)
{
        /* FIXME */
	DPRINTF("DIAGOUTPUT:%s\n", debugstr_a(str));
}
