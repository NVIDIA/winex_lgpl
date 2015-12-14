/*
 * Misc. USER functions
 *
 * Copyright 1993 Robert J. Amstadt
 *	     1996 Alex Korobka
 */

#include <stdlib.h>
#include <string.h>
#include "wine/winbase16.h"
#include "windef.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/winuser16.h"
#include "user.h"
#include "win.h"
#include "controls.h"
#include "cursoricon.h"
#include "hook.h"
#include "message.h"
#include "miscemu.h"
#include "sysmetrics.h"
#include "local.h"
#include "module.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(hook);
WINE_DECLARE_DEBUG_CHANNEL(local);
WINE_DECLARE_DEBUG_CHANNEL(system);
WINE_DECLARE_DEBUG_CHANNEL(win);
WINE_DECLARE_DEBUG_CHANNEL(win32);

SYSLEVEL USER_SysLevel;
static BOOL USER32_SleepState=FALSE;

void create_user_syslevel_cs(void)
{
    _CreateSysLevel( &USER_SysLevel, 2 );
    CRITICAL_SECTION_NAME( &USER_SysLevel.crst, "USER_SysLevel" );
}

void destroy_syslevel_cs(void)
{
    /* FIXME: No way to destroy them? */
}


/***********************************************************************
 *		GetFreeSystemResources (USER.284)
 */
WORD WINAPI GetFreeSystemResources16( WORD resType )
{
    HINSTANCE16 gdi_inst;
    WORD gdi_heap;
    int userPercent, gdiPercent;

    if ((gdi_inst = LoadLibrary16( "GDI" )) < 32) return 0;
    gdi_heap = gdi_inst | 7;

    switch(resType)
    {
    case GFSR_USERRESOURCES:
        userPercent = (int)LOCAL_CountFree( USER_HeapSel ) * 100 /
                               LOCAL_HeapSize( USER_HeapSel );
        gdiPercent  = 100;
        break;

    case GFSR_GDIRESOURCES:
        gdiPercent  = (int)LOCAL_CountFree( gdi_inst ) * 100 /
                               LOCAL_HeapSize( gdi_inst );
        userPercent = 100;
        break;

    case GFSR_SYSTEMRESOURCES:
        userPercent = (int)LOCAL_CountFree( USER_HeapSel ) * 100 /
                               LOCAL_HeapSize( USER_HeapSel );
        gdiPercent  = (int)LOCAL_CountFree( gdi_inst ) * 100 /
                               LOCAL_HeapSize( gdi_inst );
        break;

    default:
        userPercent = gdiPercent = 0;
        break;
    }
    FreeLibrary16( gdi_inst );
    return (WORD)min( userPercent, gdiPercent );
}


/**********************************************************************
 *		InitApp (USER.5)
 */
INT16 WINAPI InitApp16( HINSTANCE16 hInstance )
{
    /* Hack: restore the divide-by-zero handler */
    /* FIXME: should set a USER-specific handler that displays a msg box */
    INT_SetPMHandler( 0, INT_GetPMHandler( 0xff ) );

    /* Create task message queue */
    if ( !InitThreadInput16( 0, 0 ) ) return 0;

    return 1;
}


/***********************************************************************
 *           USER_Lock
 */
void USER_Lock(void)
{
    _EnterSysLevel( &USER_SysLevel );
}


/***********************************************************************
 *           USER_Unlock
 */
void USER_Unlock(void)
{
    _LeaveSysLevel( &USER_SysLevel );
}


/***********************************************************************
 *           USER_CheckNotLock
 *
 * Make sure that we don't hold the user lock.
 */
void USER_CheckNotLock(void)
{
    _CheckNotSysLevel( &USER_SysLevel );
}


/**********************************************************************
 *           USER_ModuleUnload
 */
static void USER_ModuleUnload( HMODULE16 hModule )
{
    HOOK_FreeModuleHooks( hModule );
    CLASS_FreeModuleClasses( hModule );
    CURSORICON_FreeModuleIcons( hModule );
}

/***********************************************************************
 *		SignalProc (USER.314)
 */
void WINAPI USER_SignalProc( HANDLE16 hTaskOrModule, UINT16 uCode,
                             UINT16 uExitFn, HINSTANCE16 hInstance,
                             HQUEUE16 hQueue )
{
    FIXME_(win)("Win 3.1 USER signal %04x\n", uCode );
}

/***********************************************************************
 *		FinalUserInit (USER.400)
 */
void WINAPI FinalUserInit16( void )
{
    /* FIXME: Should chain to FinalGdiInit now. */
}

/***********************************************************************
 *		SignalProc32 (USER.391)
 *		UserSignalProc (USER32.@)
 *
 * For comments about the meaning of uCode and dwFlags
 * see PROCESS_CallUserSignalProc.
 *
 */
WORD WINAPI UserSignalProc( UINT uCode, DWORD dwThreadOrProcessID,
                            DWORD dwFlags, HMODULE16 hModule )
{
    /* FIXME: Proper reaction to most signals still missing. */

    switch ( uCode )
    {
    case USIG_DLL_UNLOAD_WIN16:
    case USIG_DLL_UNLOAD_WIN32:
        USER_ModuleUnload( hModule );
        break;

    case USIG_DLL_UNLOAD_ORPHANS:
    case USIG_FAULT_DIALOG_PUSH:
    case USIG_FAULT_DIALOG_POP:
    case USIG_THREAD_INIT:
    case USIG_THREAD_EXIT:
    case USIG_PROCESS_CREATE:
    case USIG_PROCESS_INIT:
    case USIG_PROCESS_LOADED:
    case USIG_PROCESS_RUNNING:
    case USIG_PROCESS_EXIT:
    case USIG_PROCESS_DESTROY:
      break;

    default:
        FIXME_(win)("(%04x, %08lx, %04lx, %04x)\n",
                    uCode, dwThreadOrProcessID, dwFlags, hModule );
        break;
    }

    /* FIXME: Should chain to GdiSignalProc now. */

    return 0;
}

/***********************************************************************
 *		ExitWindows (USER.7)
 */
BOOL16 WINAPI ExitWindows16( DWORD dwReturnCode, UINT16 wReserved )
{
    return ExitWindowsEx( EWX_LOGOFF, 0xffffffff );
}


/***********************************************************************
 *		ExitWindowsExec (USER.246)
 */
BOOL16 WINAPI ExitWindowsExec16( LPCSTR lpszExe, LPCSTR lpszParams )
{
    TRACE_(system)("Should run the following in DOS-mode: \"%s %s\"\n",
	lpszExe, lpszParams);
    return ExitWindowsEx( EWX_LOGOFF, 0xffffffff );
}


/***********************************************************************
 *		ExitWindowsEx (USER32.@)
 */
BOOL WINAPI ExitWindowsEx( UINT flags, DWORD reserved )
{
    int i;
    BOOL result;
    HWND *list, *phwnd;

    /* We have to build a list of all windows first, as in EnumWindows */

    if (!(list = WIN_ListChildren( GetDesktopWindow() ))) return FALSE;

    /* Send a WM_QUERYENDSESSION message to every window */

    for (i = 0; list[i]; i++)
    {
        /* Make sure that the window still exists */
        if (!IsWindow( list[i] )) continue;
        if (!SendMessageW( list[i], WM_QUERYENDSESSION, 0, 0 )) break;
    }
    result = !list[i];

    /* Now notify all windows that got a WM_QUERYENDSESSION of the result */

    for (phwnd = list; i > 0; i--, phwnd++)
    {
        if (!IsWindow( *phwnd )) continue;
        SendMessageW( *phwnd, WM_ENDSESSION, result, 0 );
    }
    HeapFree( GetProcessHeap(), 0, list );

    if (result) ExitKernel16();
    return FALSE;
}

#if 0
static void _dump_CDS_flags(DWORD flags) {
#define X(x) if (flags & CDS_##x) MESSAGE(""#x ",");
	X(UPDATEREGISTRY);X(TEST);X(FULLSCREEN);X(GLOBAL);
	X(SET_PRIMARY);X(RESET);X(SETRECT);X(NORESET);
#undef X
}
#endif

/***********************************************************************
 *		ChangeDisplaySettingsA (USER32.@)
 */
LONG WINAPI ChangeDisplaySettingsA( LPDEVMODEA devmode, DWORD flags )
{
    TRACE_(system)("%p,0x%08lx\n", devmode, flags);
    return ChangeDisplaySettingsExA( NULL, devmode, 0, flags, 0 );
}

/***********************************************************************
 *		ChangeDisplaySettingsW (USER32.@)
 */
LONG WINAPI ChangeDisplaySettingsW( LPDEVMODEW devmode, DWORD flags )
{
    TRACE_(system)("%p,0x%08lx\n", devmode, flags);
    return ChangeDisplaySettingsExW( NULL, devmode, 0, flags, 0 );
}

/***********************************************************************
 *		ChangeDisplaySettings (USER.620)
 */
LONG WINAPI ChangeDisplaySettings16( LPDEVMODEA devmode, DWORD flags )
{
	TRACE_(system)("(%p,0x%08lx), stub\n",devmode,flags);
	return ChangeDisplaySettingsA(devmode, flags);
}

/***********************************************************************
 *		ChangeDisplaySettingsExA (USER32.@)
 */
LONG WINAPI ChangeDisplaySettingsExA(
	LPCSTR devname, LPDEVMODEA devmode, HWND hwnd, DWORD flags,
	LPARAM lparam
) 
{
   /* Only the lowest common denominator since that's all that the underlying
      ChangeDisplaySettings expects */
   WINE_WIN31_DEVMODEA *localDevMode = NULL;
   TRACE_(system)("%s,%p,%08x,0x%08lx,0x%08lx\n", devname, devmode,
                 hwnd, flags, lparam);

  if (devmode)
  {
     localDevMode = HeapAlloc(GetProcessHeap(), 0, sizeof(WINE_WIN31_DEVMODEA));
     if (!localDevMode)
     {
        ERR_(system)("Error allocating memory. Expect problems\n");
        return DISP_CHANGE_FAILED; 
     }

     memcpy(localDevMode, devmode, sizeof(WINE_WIN31_DEVMODEA));

     /* Fixup missing values that we intend to use. */
     if (!(localDevMode->dmFields & DM_PELSWIDTH))
        localDevMode->dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);

     if (!(localDevMode->dmFields & DM_PELSHEIGHT))
        localDevMode->dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);

     if (!(localDevMode->dmFields & DM_BITSPERPEL))
        localDevMode->dmBitsPerPel = GetSystemMetrics(SM_WINE_BPP);
  }

  if (USER_Driver.pChangeDisplayMode ((DEVMODEA*)localDevMode) == TRUE)
  {
   /* NOTE: the moving of existing windows has been taken out of
    * the wine premerge tree because of some things that dont exist.
    */
    /* the sysMetrics also needs to be modified */
    SYSMETRICS_Set (SM_CXSCREEN, localDevMode->dmPelsWidth);
    SYSMETRICS_Set (SM_CYSCREEN, localDevMode->dmPelsHeight);
    SYSMETRICS_Set (SM_WINE_BPP, localDevMode->dmBitsPerPel);
  } else {
/* if the dislpay change mode 'failed' that could mean it was given NULL and hence
 * was supposed to go back to default mode
 * so we set it back to the defaults from GetDeviceCaps()
 */
/* FIXME:
 * update the res that GetDeviceCaps is storing (do this in ChangeDisplayMode???)
 * then use this for both cases of failing or succeeding
 */
    HDC hdc = CreateDCA ("DISPLAY", NULL, NULL, NULL);
    SYSMETRICS_Set (SM_CXSCREEN, GetDeviceCaps(hdc, HORZRES));
    SYSMETRICS_Set (SM_CYSCREEN, GetDeviceCaps(hdc, VERTRES));
    SYSMETRICS_Set (SM_WINE_BPP, GetDeviceCaps(hdc, BITSPIXEL));
    DeleteDC (hdc);
  }
  SendNotifyMessageA(HWND_BROADCAST, WM_DISPLAYCHANGE, GetSystemMetrics(SM_WINE_BPP),
                     MAKELONG(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));

  if (localDevMode)
     HeapFree (GetProcessHeap(), 0, localDevMode);

  /* But, always return successfull anyway. */
  return DISP_CHANGE_SUCCESSFUL;
}

/***********************************************************************
 *		ChangeDisplaySettingsExW (USER32.@)
 */
LONG WINAPI ChangeDisplaySettingsExW(
	LPCWSTR devname, LPDEVMODEW devmode, HWND hwnd, DWORD flags,
	LPARAM lparam
) 
{
    DEVMODEA devmodeA;
    BOOL ret;
    DWORD len = WideCharToMultiByte( CP_ACP, 0, devname, -1, NULL, 0, NULL, NULL );
    LPSTR nameA = HeapAlloc( GetProcessHeap(), 0, len );

    memset(&devmodeA, 0, sizeof(DEVMODEA));
    WideCharToMultiByte( CP_ACP, 0, devname, -1, nameA, len, NULL, NULL );
    devmodeA.dmDeviceName[0] = 0;
	if (devmode)
	{
      devmodeA.dmBitsPerPel = devmode->dmBitsPerPel;
      devmodeA.dmPelsHeight = devmode->dmPelsHeight;
      devmodeA.dmPelsWidth = devmode->dmPelsWidth;
      devmodeA.dmDisplayFlags = devmode->dmDisplayFlags;
      devmodeA.dmDisplayFrequency = devmode->dmDisplayFrequency;
      devmodeA.dmFields = devmode->dmFields;
    }
	
    ret = ChangeDisplaySettingsExA(nameA, &devmodeA, hwnd, flags, lparam);

    HeapFree(GetProcessHeap(),0,nameA);
    return ret;
}

/***********************************************************************
 *		EnumDisplaySettingsA (USER32.@)
 * FIXME: Doesn't handle supplying "name"
 *
 * RETURNS
 *	TRUE if nth setting exists found (described in the LPDEVMODEA struct)
 *	FALSE if we do not have the nth setting
 */
BOOL WINAPI EnumDisplaySettingsA(
	LPCSTR name,		/* [in] Must be null for win95 */
	DWORD n,		/* [in] nth entry in display settings list*/
	LPDEVMODEA devmode	/* [out] devmode for that setting */
) {
	TRACE_(system)("(%s,%ld,%p)\n",name,n,devmode);
        return USER_Driver.pEnumDisplayModes (devmode, n);
}

/***********************************************************************
 *		EnumDisplaySettingsW (USER32.@)
 */
BOOL WINAPI EnumDisplaySettingsW(LPCWSTR name,DWORD n,LPDEVMODEW devmode)
{
    DEVMODEA devmodeA;
    BOOL ret;
    LPSTR nameA = NULL;

    if (name){
        DWORD len = WideCharToMultiByte( CP_ACP, 0, name, -1, NULL, 0, NULL, NULL );
        
        
        nameA = HeapAlloc( GetProcessHeap(), 0, len );

        if (nameA == NULL){
            ERR_(system)("not enough memory to allocate a string conversion buffer for '%s'\n", debugstr_w(name));

            return FALSE;
        }
        
        WideCharToMultiByte( CP_ACP, 0, name, -1, nameA, len, NULL, NULL );
    }


    ret = EnumDisplaySettingsA(nameA,n,&devmodeA);
    if (ret)
    {
        devmode->dmBitsPerPel       = devmodeA.dmBitsPerPel;
        devmode->dmPelsHeight       = devmodeA.dmPelsHeight;
        devmode->dmPelsWidth        = devmodeA.dmPelsWidth;
        devmode->dmDisplayFlags     = devmodeA.dmDisplayFlags;
        devmode->dmDisplayFrequency = devmodeA.dmDisplayFrequency;
        devmode->dmFields           = devmodeA.dmFields;
        /* FIXME: convert rest too, if they are ever returned */
    }


    if (nameA)
        HeapFree(GetProcessHeap(),0,nameA);

    return ret;
}

/***********************************************************************
 *		EnumDisplaySettings (USER.621)
 */
BOOL16 WINAPI EnumDisplaySettings16(
	LPCSTR name,		/* [in] huh? */
	DWORD n,		/* [in] nth entry in display settings list*/
	LPDEVMODEA devmode	/* [out] devmode for that setting */
) {
	TRACE_(system)("(%s, %ld, %p)\n", name, n, devmode);
	return (BOOL16)EnumDisplaySettingsA(name, n, devmode);
}

/***********************************************************************
 *		EnumDisplaySettingsExA (USER32.@)
 */
BOOL WINAPI EnumDisplaySettingsExA(LPCSTR lpszDeviceName, DWORD iModeNum,
				   LPDEVMODEA lpDevMode, DWORD dwFlags)
{
        FIXME_(system)("(%s,%lu,%p,%08lx): stub\n",
		       debugstr_a(lpszDeviceName), iModeNum, lpDevMode, dwFlags);

	return EnumDisplaySettingsA(lpszDeviceName, iModeNum, lpDevMode);
}

/***********************************************************************
 *		EnumDisplaySettingsExW (USER32.@)
 */
BOOL WINAPI EnumDisplaySettingsExW(LPCWSTR lpszDeviceName, DWORD iModeNum,
				   LPDEVMODEW lpDevMode, DWORD dwFlags)
{
	FIXME_(system)("(%s,%lu,%p,%08lx): stub\n",
			debugstr_w(lpszDeviceName), iModeNum, lpDevMode, dwFlags);

	return EnumDisplaySettingsW(lpszDeviceName, iModeNum, lpDevMode);
}

/***********************************************************************
 *		EnumDisplayDevicesA (USER32.@)
 */
BOOL WINAPI
EnumDisplayDevicesA (LPCSTR lpDevice, DWORD iDevNum,
                     LPDISPLAY_DEVICEA lpDisplayDevice, DWORD dwFlags)
{
   DISPLAY_DEVICEW DispDev;
   LPWSTR pDeviceW = NULL;
   BOOL Ret;

   TRACE_(system) ("(%s, %ld, %p, 0x%08lx)\n", lpDevice, iDevNum,
                   lpDisplayDevice, dwFlags);

   if (lpDevice)
   {
      INT Len;

      Len = MultiByteToWideChar (CP_ACP, 0, lpDevice, -1, NULL, 0);
      pDeviceW = HeapAlloc (GetProcessHeap (), 0, Len * sizeof (WCHAR));
      if (!pDeviceW)
      {
         ERR_(system) ("Out of memory copying lpDevice!\n");
         return FALSE;
      }
      MultiByteToWideChar (CP_ACP, 0, lpDevice, -1, pDeviceW, Len);
   }

   DispDev.cb = sizeof (DispDev);
   Ret = EnumDisplayDevicesW (pDeviceW, iDevNum, &DispDev, dwFlags);

   if (pDeviceW)
      HeapFree (GetProcessHeap (), 0, pDeviceW);

   if (!Ret)
      return FALSE;

   /* Don't bother checking for lpDisplayDevice != NULL; Windows XP crashes
      in that case too */
   lpDisplayDevice->StateFlags = DispDev.StateFlags;
   WideCharToMultiByte (CP_ACP, 0, DispDev.DeviceName, -1,
                        lpDisplayDevice->DeviceName,
                        sizeof (lpDisplayDevice->DeviceName), NULL, NULL);
   WideCharToMultiByte (CP_ACP, 0, DispDev.DeviceString, -1,
                        lpDisplayDevice->DeviceString,
                        sizeof (lpDisplayDevice->DeviceString), NULL, NULL);
   WideCharToMultiByte (CP_ACP, 0, DispDev.DeviceID, -1,
                        lpDisplayDevice->DeviceID,
                        sizeof (lpDisplayDevice->DeviceID), NULL, NULL);
   WideCharToMultiByte (CP_ACP, 0, DispDev.DeviceKey, -1,
                        lpDisplayDevice->DeviceKey,
                        sizeof (lpDisplayDevice->DeviceKey), NULL, NULL);

   return TRUE;
}


/* All values except for DeviceString taken from XP SP 2; the DeviceKey
   needs to match what we use to store info obtained during ddraw startup */
static WCHAR DeviceName[] = {'\\', '\\', '.', '\\', 'D', 'I', 'S', 'P', 'L',
                             'A', 'Y', '1', 0};
static WCHAR DeviceString[] = {'T', 'r', 'a', 'n', 's', 'G', 'a', 'm', 'i',
                               'n', 'g', ' ', 'D', 'i', 's', 'p', 'l', 'a',
                               'y', ' ', 'D', 'e', 'v', 'i', 'c', 'e', 0};
static WCHAR DeviceID[] = {'P', 'C', 'I', '\\', 'V', 'E', 'N', '_', '1', '0',
                           'D', 'E', '&', 'D', 'E', 'V', '_', '0', '0', '4',
                           '5', '&', 'S', 'U', 'B', 'S', 'Y', 'S', '_', 'A',
                           '3', '4', '4', '3', '8', '4', '2', '&', 'R', 'E',
                           'V', '_', 'A', '1', 0};
static WCHAR DeviceKey[] = {'\\', 'R', 'e', 'g', 'i', 's', 't', 'r', 'y', '\\',
                            'M', 'a', 'c', 'h', 'i', 'n', 'e', '\\',
                            'S', 'y', 's', 't', 'e', 'm', '\\',
                            'C', 'u', 'r', 'r', 'e', 'n', 't', 'C', 'o', 'n',
                            't', 'r', 'o', 'l', 'S', 'e', 't', '\\',
                            'C', 'o', 'n', 't', 'r', 'o', 'l', '\\',
                            'V', 'i', 'd', 'e', 'o', '\\',
                            '{', '5', '2', '8', '7', '3', '3', 'A', '0', '-',
                            '2', '8', 'F', '6', '-', '4', '9', '8', 'D', '-',
                            'A', '2', 'B', 'D', '-',
                            'C', '8', '3', 'B', 'E', 'E', 'B', '5', 'A', '9',
                            'B', '7', '}', '\\',
                            '0', '0', '0', '0', 0};
static WCHAR MDeviceName[] = {'\\', '\\', '.', '\\', 'D', 'I', 'S', 'P', 'L',
                              'A', 'Y', '1', '\\', 'M', 'o', 'n', 'i', 't',
                              'o', 'r', '0', 0};
static WCHAR MDeviceString[] = {'T', 'r', 'a', 'n', 's', 'G', 'a', 'm', 'i',
                                'n', 'g', ' ', 'D', 'i', 's', 'p', 'l', 'a',
                                'y', ' ', 'M', 'o', 'n', 'i', 't', 'o', 'r',
                                0};
static WCHAR MDeviceID[] = {'M', 'o', 'n', 'i', 't', 'o', 'r', '\\',
                            'V', 'S', 'C', 'E', '4', '1', 'B', '\\',
                            '{', '4', 'D', '3', '6', 'E', '9', '6', 'E', '-',
                            'E', '3', '2', '5', '-',
                            '1', '1', 'C', 'E', '-',
                            'B', 'F', 'C', '1', '-',
                            '0', '8', '0', '0', '2', 'B', 'E', '1', '0', '3',
                            '1', '8', '}', '\\', '0', '0', '1', '9', 0};
static WCHAR MDeviceKey[] = {'\\', 'R', 'e', 'g', 'i', 's', 't', 'r', 'y',
                             '\\', 'M', 'a', 'c', 'h', 'i', 'n', 'e', '\\',
                             'S', 'y', 's', 't', 'e', 'm', '\\',
                             'C', 'u', 'r', 'r', 'e', 'n', 't', 'C', 'o', 'n',
                             't', 'r', 'o', 'l', 'S', 'e', 't', '\\',
                             'C', 'o', 'n', 't', 'r', 'o', 'l', '\\',
                             'C', 'l', 'a', 's', 's', '\\',
                             '{', '4', 'D', '3', '6', 'E', '9', '6', 'E', '-',
                             'E', '3', '2', '5', '-',
                             '1', '1', 'C', 'E', '-',
                             'B', 'F', 'C', '1', '-',
                             '0', '8', '0', '0', '2', 'B', 'E', '1', '0', '3',
                             '1', '8', '}', '\\', '0', '0', '1', '9', 0};


/***********************************************************************
 *		EnumDisplayDevicesW (USER32.@)
 */
BOOL WINAPI
EnumDisplayDevicesW (LPCWSTR lpDevice, DWORD iDevNum,
                     LPDISPLAY_DEVICEW lpDisplayDevice, DWORD dwFlags)
{
   TRACE_(system) ("(%s, %ld, %p, 0x%08lx)\n", debugstr_w (lpDevice), iDevNum,
                   lpDisplayDevice, dwFlags);

   if (dwFlags)
      FIXME_(system) ("dwFlags 0x%lx is not supported!\n", dwFlags);

   /* We only present one device to the application */
   if (iDevNum)
      return FALSE;

   FIXME_(system) ("Real display device and monitor names should be retrieved\n");

   /* Don't bother checking for lpDisplayDevice != NULL; Windows XP crashes
      in that case too */

   if (lpDevice == NULL)
   {
      strcpyW (lpDisplayDevice->DeviceName, DeviceName);
      strcpyW (lpDisplayDevice->DeviceString, DeviceString);
      strcpyW (lpDisplayDevice->DeviceID, DeviceID);
      strcpyW (lpDisplayDevice->DeviceKey, DeviceKey);
      lpDisplayDevice->StateFlags =
         DISPLAY_DEVICE_ATTACHED_TO_DESKTOP |
         DISPLAY_DEVICE_PRIMARY_DEVICE |
         DISPLAY_DEVICE_VGA_COMPATIBLE;
   }
   else
   {
      if (strcmpW (lpDevice, DeviceName))
      {
         ERR_(system) ("Attempted to get non-existant monitor info!\n");
         return FALSE;
      }

      strcpyW (lpDisplayDevice->DeviceName, MDeviceName);
      strcpyW (lpDisplayDevice->DeviceString, MDeviceString);
      strcpyW (lpDisplayDevice->DeviceID, MDeviceID);
      strcpyW (lpDisplayDevice->DeviceKey, MDeviceKey);
      lpDisplayDevice->StateFlags =
         DISPLAY_DEVICE_ATTACHED_TO_DESKTOP |
         DISPLAY_DEVICE_MULTI_DRIVER;
   }

   return TRUE;
}

/***********************************************************************
 *		SetEventHook (USER.321)
 *
 *	Used by Turbo Debugger for Windows
 */
FARPROC16 WINAPI SetEventHook16(FARPROC16 lpfnEventHook)
{
	FIXME_(hook)("(lpfnEventHook=%08x): stub\n", (UINT)lpfnEventHook);
	return NULL;
}

/***********************************************************************
 *		UserSeeUserDo (USER.216)
 */
DWORD WINAPI UserSeeUserDo16(WORD wReqType, WORD wParam1, WORD wParam2, WORD wParam3)
{
    switch (wReqType)
    {
    case USUD_LOCALALLOC:
        return LOCAL_Alloc(USER_HeapSel, wParam1, wParam3);
    case USUD_LOCALFREE:
        return LOCAL_Free(USER_HeapSel, wParam1);
    case USUD_LOCALCOMPACT:
        return LOCAL_Compact(USER_HeapSel, wParam3, 0);
    case USUD_LOCALHEAP:
        return USER_HeapSel;
    case USUD_FIRSTCLASS:
        FIXME_(local)("return a pointer to the first window class.\n");
        return (DWORD)-1;
    default:
        WARN_(local)("wReqType %04x (unknown)", wReqType);
        return (DWORD)-1;
    }
}

/***********************************************************************
 *		GetSystemDebugState (USER.231)
 */
WORD WINAPI GetSystemDebugState16(void)
{
    return 0;  /* FIXME */
}

/***********************************************************************
 *		RegisterLogonProcess (USER32.@)
 */
DWORD WINAPI RegisterLogonProcess(HANDLE hprocess,BOOL x) {
	FIXME_(win32)("(%d,%d),stub!\n",hprocess,x);
	return 1;
}

/***********************************************************************
 *		CreateWindowStationW (USER32.@)
 */
HWINSTA WINAPI CreateWindowStationW(
	LPWSTR winstation,DWORD res1,DWORD desiredaccess,
	LPSECURITY_ATTRIBUTES lpsa
) {
	FIXME_(win32)("(%s,0x%08lx,0x%08lx,%p),stub!\n",debugstr_w(winstation),
		res1,desiredaccess,lpsa
	);
	return (HWINSTA)0xdeadcafe;
}

/***********************************************************************
 *		SetProcessWindowStation (USER32.@)
 */
BOOL WINAPI SetProcessWindowStation(HWINSTA hWinSta) {
	FIXME_(win32)("(%d),stub!\n",hWinSta);
	return TRUE;
}

/***********************************************************************
 *		SetUserObjectSecurity (USER32.@)
 */
BOOL WINAPI SetUserObjectSecurity(
	HANDLE hObj,
	PSECURITY_INFORMATION pSIRequested,
	PSECURITY_DESCRIPTOR pSID
) {
	FIXME_(win32)("(0x%08x,%p,%p),stub!\n",hObj,pSIRequested,pSID);
	return TRUE;
}

/***********************************************************************
 *		CreateDesktopA (USER32.@)
 */
HDESK WINAPI CreateDesktopA(
	LPSTR lpszDesktop,LPSTR lpszDevice,LPDEVMODEA pDevmode,
	DWORD dwFlags,DWORD dwDesiredAccess,LPSECURITY_ATTRIBUTES lpsa
) {
	FIXME_(win32)("(%s,%s,%p,0x%08lx,0x%08lx,%p),stub!\n",
		lpszDesktop,lpszDevice,pDevmode,
		dwFlags,dwDesiredAccess,lpsa
	);
	return (HDESK)0xcafedead;
}

/***********************************************************************
 *		CreateDesktopW (USER32.@)
 */
HDESK WINAPI CreateDesktopW(
	LPWSTR lpszDesktop,LPWSTR lpszDevice,LPDEVMODEW pDevmode,
	DWORD dwFlags,DWORD dwDesiredAccess,LPSECURITY_ATTRIBUTES lpsa
) {
	FIXME_(win32)("(%s,%s,%p,0x%08lx,0x%08lx,%p),stub!\n",
		debugstr_w(lpszDesktop),debugstr_w(lpszDevice),pDevmode,
		dwFlags,dwDesiredAccess,lpsa
	);
	return (HDESK)0xcafedead;
}

/***********************************************************************
 *		EnumDesktopWindows (USER32.@)
 */
BOOL WINAPI EnumDesktopWindows( HDESK hDesktop, WNDENUMPROC lpfn, LPARAM lParam ) {
  FIXME_(win32)("(0x%08x, %p, 0x%08lx), stub!\n", hDesktop, lpfn, lParam );
  return TRUE;
}


/***********************************************************************
 *		CloseWindowStation (USER32.@)
 */
BOOL WINAPI CloseWindowStation(HWINSTA hWinSta)
{
    FIXME_(win32)("(0x%08x)\n", hWinSta);
    return TRUE;
}

/***********************************************************************
 *		CloseDesktop (USER32.@)
 */
BOOL WINAPI CloseDesktop(HDESK hDesk)
{
    FIXME_(win32)("(0x%08x)\n", hDesk);
    return TRUE;
}

/***********************************************************************
 *		SetWindowStationUser (USER32.@)
 */
DWORD WINAPI SetWindowStationUser(DWORD x1,DWORD x2) {
	FIXME_(win32)("(0x%08lx,0x%08lx),stub!\n",x1,x2);
	return 1;
}

/***********************************************************************
 *		SetLogonNotifyWindow (USER32.@)
 */
DWORD WINAPI SetLogonNotifyWindow(HWINSTA hwinsta,HWND hwnd) {
	FIXME_(win32)("(0x%x,%04x),stub!\n",hwinsta,hwnd);
	return 1;
}

/***********************************************************************
 *		LoadLocalFonts (USER32.@)
 */
VOID WINAPI LoadLocalFonts(VOID) {
	/* are loaded. */
	return;
}
/***********************************************************************
 *		GetUserObjectInformationA (USER32.@)
 */
BOOL WINAPI GetUserObjectInformationA( HANDLE hObj, INT nIndex, LPVOID pvInfo, DWORD nLength, LPDWORD lpnLen )
{	FIXME_(win32)("(0x%x %i %p %ld %p),stub!\n", hObj, nIndex, pvInfo, nLength, lpnLen );
	return TRUE;
}
/***********************************************************************
 *		GetUserObjectInformationW (USER32.@)
 */
BOOL WINAPI GetUserObjectInformationW( HANDLE hObj, INT nIndex, LPVOID pvInfo, DWORD nLength, LPDWORD lpnLen )
{	FIXME_(win32)("(0x%x %i %p %ld %p),stub!\n", hObj, nIndex, pvInfo, nLength, lpnLen );
	return TRUE;
}
/***********************************************************************
 *		GetUserObjectSecurity (USER32.@)
 */
BOOL WINAPI GetUserObjectSecurity(HANDLE hObj, PSECURITY_INFORMATION pSIRequested,
	PSECURITY_DESCRIPTOR pSID, DWORD nLength, LPDWORD lpnLengthNeeded)
{	FIXME_(win32)("(0x%x %p %p len=%ld %p),stub!\n",  hObj, pSIRequested, pSID, nLength, lpnLengthNeeded);
	return TRUE;
}

/***********************************************************************
 *		SetSystemCursor (USER32.@)
 */
BOOL WINAPI SetSystemCursor(HCURSOR hcur, DWORD id)
{	FIXME_(win32)("(%08x,%08lx),stub!\n",  hcur, id);
	return TRUE;
}

/***********************************************************************
 *		RegisterSystemThread (USER32.@)
 */
void WINAPI RegisterSystemThread(DWORD flags, DWORD reserved)
{
	FIXME_(win32)("(%08lx, %08lx)\n", flags, reserved);
}


/***********************************************************************
 *		RegisterDeviceNotificationA (USER32.@)
 */
HDEVNOTIFY WINAPI RegisterDeviceNotificationA(
	HANDLE hnd, LPVOID notifyfilter, DWORD flags
) {

    FIXME_(win32)("Not converting strings in notifyfilter(%p) to WCHAR\n");
    HDEVNOTIFY result = RegisterDeviceNotificationW( hnd, notifyfilter, flags );

    return result;
}

/***********************************************************************
 *		UnregisterDeviceNotification (USER32.@)
 */
BOOL WINAPI UnregisterDeviceNotification(HDEVNOTIFY handle)
{
    FIXME_(win32)("(handle = %p), STUB!  Freeing fake handle\n", handle);


    if (handle){
        HeapFree(GetProcessHeap(), 0, handle);

        return TRUE;
    }

	return FALSE;
}

/***********************************************************************
 *		RegisterDeviceNotificationW (USER32.@)
 */
HDEVNOTIFY WINAPI RegisterDeviceNotificationW(
	HANDLE hnd, LPVOID notifyfilter, DWORD flags
) {
	FIXME_(win32)("(hwnd=%08x, filter=%p, flags=0x%08lx), semi-stub!  Returning a fake handle\n",
                    hnd, notifyfilter, flags);
                    
    DEVTYP_UNION dev;
    DWORD devType;

    /* FIXME: currently we only send the WM_DEVICECHANGE messages to top level windows so report an error if someone is trying to
       register a child window for device notifications 
       FIXME: Since we aren't using the name parameter in the notifyfilter structure, we don't bother doing the string conversion if
       this function is called from RegisterDeviceNotificationA() */      
    if (GetParent(hnd))
        ERR_(win32)("This is a child window: right now it can't receive WM_DEVICECHANGE messages!\n");
    
    if (notifyfilter)
    {
        dev.pVoid = notifyfilter;

        switch(dev.pHdr->dbch_devicetype)
        {
        case DBT_DEVTYP_OEM:
            FIXME_(win32)("Unimplemented support for: OEM Device 0x%x -- dbco_size: %d, dbco_identifier: %d, dbco_suppfunc: %d\n", dev.pVoid, dev.pOEM->dbco_size, dev.pOEM->dbco_identifier, dev.pOEM->dbco_suppfunc);
            break;
        case DBT_DEVTYP_VOLUME:
            FIXME_(win32)("Unimplemented support for: Volume 0x%x -- dbcv_size: %d, dbcv_unitmask: %d, dbcv_flags: %d\n", dev.pVoid, dev.pVol->dbcv_size, dev.pVol->dbcv_unitmask, dev.pVol->dbcv_flags);
            break;
        case DBT_DEVTYP_PORT:
            FIXME_(win32)("Unimplemented support for: Port Device 0x%x -- dbcp_size: %d, dbcp_name: %s\n", dev.pVoid, dev.pPort->dbcp_size, dev.pPort->dbcp_name);
            break;
        case DBT_DEVTYP_DEVICEINTERFACE:
            FIXME_(win32)("Class of devices 0x%x -- dbcc_size: %d, dbcc_guid: %s, dbcc_name: %s\n", dev.pVoid, dev.pDevInt->dbcc_size, debugstr_guid(&(dev.pDevInt->dbcc_classguid)), dev.pDevInt->dbcc_name);
            /*if (flags == DEVICE_NOTIFY_WINDOW_HANDLE)
                USER_Driver.pRegisterDeviceNotification(hnd, notifyfilter);*/
            break;
        case DBT_DEVTYP_HANDLE:
            FIXME_(win32)("Unimplemented support for: File Handle 0x%x -- dbch_size: %d, dbch_handle: %d, dbch_hdevnotify: %d, dbch_eventguid: %s\n", dev.pVoid, dev.pHnd->dbch_size, dev.pHnd->dbch_handle, dev.pHnd->dbch_hdevnotify, debugstr_guid(&(dev.pHnd->dbch_eventguid)));
            break;
        default:
            FIXME_(win32)("Unknown device type\n");
            break;
        }
    }

    /* used for debug output */
    switch (flags)
    {
    case DEVICE_NOTIFY_WINDOW_HANDLE:
        TRACE_(win32)("Recipient is a window handle\n");
        break;
    case DEVICE_NOTIFY_SERVICE_HANDLE:
        FIXME_(win32)("Unimplemented: Recipient is a service handle\n");
        break;
    case DEVICE_NOTIFY_ALL_INTERFACE_CLASSES:
        FIXME_(win32)("Unimplemented: Recipient is all interface classes\n");
        break;
    default:
        break;
    }

    HDEVNOTIFY result = (HDEVNOTIFY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(int));

    return result;
}

void WINAPI USER32_SetSleepState(BOOL state)
{
    USER32_SleepState = state;
}

BOOL WINAPI USER32_GetSleepState(void)
{
    return(USER32_SleepState);
}

void WINAPI USER32_PollSleepState(void)
{
  while (USER32_SleepState)
  {
    /* Yield to next ready thread */
    Sleep(5);
  }
}
