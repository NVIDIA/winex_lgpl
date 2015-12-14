/*
 * GDI initialization code
 */

#include <string.h>
#include "windef.h"
#include "wingdi.h"
#include "wine/winbase16.h"

#include "gdi.h"
#include "win16drv.h"
#include "winbase.h"

/***********************************************************************
 *           GDI initialisation routine
 */
BOOL WINAPI MAIN_GdiInit(HINSTANCE hinstDLL, DWORD reason, LPVOID lpvReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        return GDI_Init();
    case DLL_PROCESS_DETACH:
        return GDI_Finalize();
    default:
        return TRUE;
    }
}


/***********************************************************************
 *           Copy   (GDI.250)
 */
void WINAPI Copy16( LPVOID src, LPVOID dst, WORD size )
{
    memcpy( dst, src, size );
}
