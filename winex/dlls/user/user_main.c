/*
 * USER initialization code
 */

#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"

#include "controls.h"
#include "cursoricon.h"
#include "global.h"
#include "input.h"
#include "hook.h"
#include "message.h"
#include "queue.h"
#include "spy.h"
#include "sysmetrics.h"
#include "user.h"
#include "win.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(graphics);

USER_DRIVER USER_Driver;

WINE_LOOK TWEAK_WineLook = WIN31_LOOK;

WORD USER_HeapSel = 0;  /* USER heap selector */

static HMODULE graphics_driver;
static DWORD exiting_thread_id;

extern void COMM_Init(void);
extern void WDML_NotifyThreadDetach(void);

extern void create_user_syslevel_cs(void);
extern void create_timer_cs(void);
extern void create_queue_cs(void);
extern void create_icon_cs(void);
extern void create_dde_cs(void);
extern void create_wdml_cs(void);


#define GET_USER_FUNC(name) USER_Driver.p##name = (void*)GetProcAddress( graphics_driver, #name )

/* load the graphics driver */
static BOOL load_driver(void)
{
    char buffer[MAX_PATH];
    HKEY hkey;
    DWORD type, count;

    strcpy( buffer, "x11drv" );  /* default value */
    if (!RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Wine", &hkey ))
    {
        count = sizeof(buffer);
        RegQueryValueExA( hkey, "GraphicsDriver", 0, &type, buffer, &count );
        RegCloseKey( hkey );
    }

    if (!(graphics_driver = LoadLibraryA( buffer )))
    {
        MESSAGE( "Could not load graphics driver '%s'\n", buffer );
        return FALSE;
    }

    GET_USER_FUNC(InitKeyboard);
    GET_USER_FUNC(VkKeyScan);
    GET_USER_FUNC(MapVirtualKey);
    GET_USER_FUNC(GetKeyNameText);
    GET_USER_FUNC(ToUnicode);
    GET_USER_FUNC(GetCharMessages);
    GET_USER_FUNC(Beep);
    GET_USER_FUNC(InitMouse);
    GET_USER_FUNC(SetCursor);
    GET_USER_FUNC(GetCursorPos);
    GET_USER_FUNC(SetCursorPos);
    GET_USER_FUNC(GetScreenSaveActive);
    GET_USER_FUNC(SetScreenSaveActive);
    GET_USER_FUNC(AcquireClipboard);
    GET_USER_FUNC(ReleaseClipboard);
    GET_USER_FUNC(SetClipboardData);
    GET_USER_FUNC(GetClipboardData);
    GET_USER_FUNC(IsClipboardFormatAvailable);
    GET_USER_FUNC(RegisterClipboardFormat);
    GET_USER_FUNC(IsSelectionOwner);
    GET_USER_FUNC(ResetSelectionOwner);
    GET_USER_FUNC(CreateWindow);
    GET_USER_FUNC(DestroyWindow);
    GET_USER_FUNC(GetDC);
    GET_USER_FUNC(ForceWindowRaise);
    GET_USER_FUNC(MsgWaitForMultipleObjectsEx);
    GET_USER_FUNC(ScrollDC);
    GET_USER_FUNC(ScrollWindowEx);
    GET_USER_FUNC(SetFocus);
    GET_USER_FUNC(SetParent);
    GET_USER_FUNC(SetWindowPos);
    GET_USER_FUNC(SetWindowRgn);
    GET_USER_FUNC(SetWindowIcon);
    GET_USER_FUNC(SetWindowStyle);
    GET_USER_FUNC(SetWindowText);
    GET_USER_FUNC(ShowWindow);
    GET_USER_FUNC(SysCommandSizeMove);
    GET_USER_FUNC(ChangeDisplayMode);
    GET_USER_FUNC(EnumDisplayModes);
    GET_USER_FUNC(GetSystemMetrics);
    GET_USER_FUNC(MessageBox);
    GET_USER_FUNC(DisplayEmergencyExit);
    GET_USER_FUNC(FlashWindow);
    GET_USER_FUNC(IsExclusiveFullscreenWindow);

    return TRUE;
}


/***********************************************************************
 *           controls_init
 *
 * Register the classes for the builtin controls
 */
static void controls_init(void)
{
    extern const struct builtin_class_descr BUTTON_builtin_class;
    extern const struct builtin_class_descr COMBO_builtin_class;
    extern const struct builtin_class_descr COMBOLBOX_builtin_class;
    extern const struct builtin_class_descr DIALOG_builtin_class;
    extern const struct builtin_class_descr DESKTOP_builtin_class;
    extern const struct builtin_class_descr EDIT_builtin_class;
    extern const struct builtin_class_descr ICONTITLE_builtin_class;
    extern const struct builtin_class_descr LISTBOX_builtin_class;
    extern const struct builtin_class_descr MDICLIENT_builtin_class;
    extern const struct builtin_class_descr MENU_builtin_class;
    extern const struct builtin_class_descr SCROLL_builtin_class;
    extern const struct builtin_class_descr STATIC_builtin_class;

    CLASS_RegisterBuiltinClass( &BUTTON_builtin_class );
    CLASS_RegisterBuiltinClass( &COMBO_builtin_class );
    CLASS_RegisterBuiltinClass( &COMBOLBOX_builtin_class );
    CLASS_RegisterBuiltinClass( &DIALOG_builtin_class );
    CLASS_RegisterBuiltinClass( &DESKTOP_builtin_class );
    CLASS_RegisterBuiltinClass( &EDIT_builtin_class );
    CLASS_RegisterBuiltinClass( &ICONTITLE_builtin_class );
    CLASS_RegisterBuiltinClass( &LISTBOX_builtin_class );
    CLASS_RegisterBuiltinClass( &MDICLIENT_builtin_class );
    CLASS_RegisterBuiltinClass( &MENU_builtin_class );
    CLASS_RegisterBuiltinClass( &SCROLL_builtin_class );
    CLASS_RegisterBuiltinClass( &STATIC_builtin_class );
}


/***********************************************************************
 *           palette_init
 *
 * Patch the function pointers in GDI for SelectPalette and RealizePalette
 */
static void palette_init(void)
{
    void **ptr;
    HMODULE module = GetModuleHandleA( "gdi32" );
    if (!module)
    {
        ERR( "cannot get GDI32 handle\n" );
        return;
    }
    if ((ptr = (void**)GetProcAddress( module, "pfnSelectPalette" ))) *ptr = SelectPalette16;
    else ERR( "cannot find pfnSelectPalette in GDI32\n" );
    if ((ptr = (void**)GetProcAddress( module, "pfnRealizePalette" ))) *ptr = UserRealizePalette;
    else ERR( "cannot find pfnRealizePalette in GDI32\n" );
}


/***********************************************************************
 *           tweak_init
 */
static void tweak_init(void)
{
    static const char *OS = "Win3.1";
    char buffer[80];
    HKEY hkey;
    DWORD type, count = sizeof(buffer);

    if (RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Tweak.Layout", &hkey ))
        return;
    if (RegQueryValueExA( hkey, "WineLook", 0, &type, buffer, &count ))
        strcpy( buffer, "Win31" );  /* default value */
    RegCloseKey( hkey );

    /* WIN31_LOOK is default */
    if (!strncasecmp( buffer, "Win95", 5 ))
    {
        TWEAK_WineLook = WIN95_LOOK;
        OS = "Win95";
    }
    else if (!strncasecmp( buffer, "Win98", 5 ))
    {
        TWEAK_WineLook = WIN98_LOOK;
        OS = "Win98";
    }
    TRACE("Using %s look and feel.\n", OS);
}


/***********************************************************************
 *           USER initialisation routine
 */
static BOOL process_attach(void)
{
    HINSTANCE16 instance;
    
    create_user_syslevel_cs();
    create_timer_cs();
    create_queue_cs();
    create_icon_cs();
    
    create_dde_cs();
    create_wdml_cs();

    /* Create USER heap */
    if ((instance = LoadLibrary16( "USER.EXE" )) < 32) return FALSE;
    USER_HeapSel = instance | 7;

     /* Global atom table initialisation */
    if (!ATOM_Init( USER_HeapSel )) return FALSE;

    /* Load the graphics driver */
    tweak_init();
    if (!load_driver()) return FALSE;

    /* Initialize system colors and metrics */
    SYSMETRICS_Init();
    SYSCOLOR_Init();

    /* Setup palette function pointers */
    palette_init();

    /* Initialize window procedures */
    if (!WINPROC_Init()) return FALSE;

    /* Initialize built-in window classes */
    controls_init();

    /* Initialize dialog manager */
    if (!DIALOG_Init()) return FALSE;

    /* Initialize menus */
    if (!MENU_Init()) return FALSE;

    /* Initialize message spying */
    if (!SPY_Init()) return FALSE;

    /* Create message queue of initial thread */
    InitThreadInput16( 0, 0 );

    /* Create desktop window */
    if (!WIN_CreateDesktopWindow()) return FALSE;

    /* Initialize keyboard driver */
    if (USER_Driver.pInitKeyboard) USER_Driver.pInitKeyboard( InputKeyStateTable );

    /* Initialize mouse driver */
    if (USER_Driver.pInitMouse) USER_Driver.pInitMouse( InputKeyStateTable );


    /* duplicate the initial keyboard state into the msg queue state table as well.  This
       should handle updating both tables with the initial states of the locking key (ie:
       caps lock, num lock, etc). */
    memcpy(QueueKeyStateTable, InputKeyStateTable, sizeof(QueueKeyStateTable));

    /* Initialize 16-bit serial communications */
    COMM_Init();

    return TRUE;
}


/**********************************************************************
 *           USER_IsExitingThread
 */
BOOL USER_IsExitingThread( DWORD tid )
{
    return (tid == exiting_thread_id);
}


/**********************************************************************
 *           thread_detach
 */
static void thread_detach(void)
{
    HQUEUE16 hQueue = GetThreadQueue16( 0 );

    exiting_thread_id = GetCurrentThreadId();

    WDML_NotifyThreadDetach();

    if (hQueue)
    {
        TIMER_RemoveQueueTimers( hQueue );
        HOOK_FreeQueueHooks();
        WIN_DestroyThreadWindows( GetDesktopWindow() );
        QUEUE_DeleteMsgQueue();
    }

    if (!(NtCurrentTeb()->tibflags & TEBF_WIN32))
    {
        HMODULE16 hModule = GetExePtr( MapHModuleLS(0) );

        /* FIXME: maybe destroy menus (Windows only complains about them
         * but does nothing);
         */
        if (GetModuleUsage16( hModule ) <= 1)
        {
            /* ModuleUnload() in "Internals" */
            HOOK_FreeModuleHooks( hModule );
            CLASS_FreeModuleClasses( hModule );
            CURSORICON_FreeModuleIcons( hModule );
        }
    }
    exiting_thread_id = 0;
}

/***********************************************************************
 *           UserClientDllInitialize  (USER32.@)
 *
 * USER dll initialisation routine
 */
BOOL WINAPI UserClientDllInitialize( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    BOOL ret = TRUE;
    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        ret = process_attach();
        break;
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        thread_detach();
        break;
    }
    return ret;
}
