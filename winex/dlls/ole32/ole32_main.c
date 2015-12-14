/*
 *  OLE32 Initialization
 *
 */
#include "windef.h"
#include "winerror.h"
#include "objbase.h"
#include "ole32_main.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

HINSTANCE OLE32_hInstance = 0;

/***********************************************************************
 *		DllEntryPoint (OLE32.@)
 */

BOOL WINAPI OLE32_DllEntryPoint(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID fImpLoad)
{
    TRACE("0x%x 0x%lx %p\n", hinstDLL, fdwReason, fImpLoad);

    switch(fdwReason) {
    case DLL_PROCESS_ATTACH:
        OLE32_hInstance = hinstDLL;
        COMPOBJ_InitProcess();
	if (TRACE_ON(ole)) CoRegisterMallocSpy((LPVOID)-1);
	break;

    case DLL_PROCESS_DETACH:
        if (TRACE_ON(ole)) CoRevokeMallocSpy();
        COMPOBJ_UninitProcess();
        OLE32_hInstance = 0;
	break;
    }
    return TRUE;
}

/***********************************************************************
 *		DllRegisterServer (OLE32.194)
 */
HRESULT WINAPI OLE32_DllRegisterServer() {
    /* FIXME: what Interfaces should we register ... */
    FIXME("(), stub!\n");
    return S_OK;
}
