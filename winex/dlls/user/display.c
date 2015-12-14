/*
 * DISPLAY driver
 *
 * Copyright 1998 Ulrich Weigand
 *
 */

#include "wine/debug.h"
#include "windef.h"
#include "user.h"
#include "wine/winuser16.h"

WINE_DEFAULT_DEBUG_CHANNEL(cursor);

#include "pshpack1.h"
typedef struct tagCURSORINFO16
{
    WORD wXMickeys;
    WORD wYMickeys;
} CURSORINFO16, *PCURSORINFO16, *LPCURSORINFO16;
#include "poppack.h"

/***********************************************************************
 *           Inquire			(DISPLAY.101)
 */
WORD WINAPI DISPLAY_Inquire(LPCURSORINFO16 lpCursorInfo)
{
    lpCursorInfo->wXMickeys = 1;
    lpCursorInfo->wYMickeys = 1;

    return sizeof(CURSORINFO16);
}

/***********************************************************************
 *           SetCursor			(DISPLAY.102)
 */
VOID WINAPI DISPLAY_SetCursor( struct tagCURSORICONINFO *lpCursor )
{
    USER_Driver.pSetCursor(lpCursor);
}

/***********************************************************************
 *           MoveCursor			(DISPLAY.103)
 */
VOID WINAPI DISPLAY_MoveCursor( WORD wAbsX, WORD wAbsY )
{
    USER_Driver.pSetCursorPos(wAbsX, wAbsY);
}

/***********************************************************************
 *           CheckCursor                  (DISPLAY.104)
 */
VOID WINAPI DISPLAY_CheckCursor( void )
{
    TRACE("stub\n" );
}

/***********************************************************************
 *           GetDriverResourceID                  (DISPLAY.450)
 *
 * Used by USER to check if driver contains better version of a builtin
 * resource than USER (yes, our DISPLAY does !).
 * wQueriedResID is the ID USER asks about.
 * lpsResName does often contain "OEMBIN".
 */
DWORD WINAPI DISPLAY_GetDriverResourceID( WORD wQueriedResID, LPSTR lpsResName )
{
	if (wQueriedResID == 3)
		return (DWORD)1;

	return (DWORD)wQueriedResID;
}

/***********************************************************************
 *           UserRepaintDisable			(DISPLAY.500)
 */
VOID WINAPI UserRepaintDisable16( BOOL16 disable )
{
    FIXME("stub\n");
}

/***********************************************************************
 *           DisplayEmergencyExit
 */
void WINAPI DisplayEmergencyExit()
{
        if (USER_Driver.pDisplayEmergencyExit) {
                USER_Driver.pDisplayEmergencyExit();
        }
}
