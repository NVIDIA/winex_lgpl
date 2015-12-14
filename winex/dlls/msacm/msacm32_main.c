/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 *      MSACM32 library
 *
 *      Copyright 1998  Patrik Stridvall
 *		  1999	Eric Pouech
 */

#include "winbase.h"
#include "winerror.h"
#include "windef.h"
#include "wine/debug.h"
#include "msacm.h"
#include "msacmdrv.h"
#include "wineacm.h"

WINE_DEFAULT_DEBUG_CHANNEL(msacm);

/**********************************************************************/

HINSTANCE	MSACM_hInstance32 = 0;

/***********************************************************************
 *           MSACM_LibMain (MSACM32.init)
 */
BOOL WINAPI MSACM32_LibMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("0x%x 0x%lx %p\n", hInstDLL, fdwReason, lpvReserved);

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        MSACM_hHeap = HeapCreate(0, 0x10000, 0);
        MSACM_hInstance32 = hInstDLL;
        MSACM_RegisterAllDrivers();
	break;
    case DLL_PROCESS_DETACH:
        MSACM_UnregisterAllDrivers();
        HeapDestroy(MSACM_hHeap);
        MSACM_hHeap = (HANDLE)NULL;
        MSACM_hInstance32 = (HINSTANCE)NULL;
	break;
    case DLL_THREAD_ATTACH:
	break;
    case DLL_THREAD_DETACH:
	break;
    default:
	break;
    }
    return TRUE;
}

/***********************************************************************
 *           XRegThunkEntry (MSACM32.1)
 * FIXME
 *   No documentation found.
 */

/***********************************************************************
 *           acmGetVersion (MSACM32.@)
 */
DWORD WINAPI acmGetVersion(void)
{
    OSVERSIONINFOA version;

    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    if (!GetVersionExA( &version ))
	return 0x04030000;

    switch (version.dwPlatformId) {
    case VER_PLATFORM_WIN32_NT:
	return 0x04000565; /* 4.0.1381 */
    default:
        FIXME("%lx not supported\n", version.dwPlatformId);
    case VER_PLATFORM_WIN32_WINDOWS:
	return 0x04030000; /* 4.3.0 */
    }
}

/***********************************************************************
 *           acmMessage32 (MSACM32.35)
 * FIXME
 *   No documentation found.
 */

/***********************************************************************
 *           acmMetrics (MSACM32.@)
 */
MMRESULT WINAPI acmMetrics(HACMOBJ hao, UINT uMetric, LPVOID pMetric)
{
    PWINE_ACMOBJ 	pao = MSACM_GetObj(hao, WINE_ACMOBJ_DONTCARE);
    BOOL 		bLocal = TRUE;
    PWINE_ACMDRIVERID	padid;
    DWORD		val = 0;
    int			i;
    MMRESULT		mmr = MMSYSERR_NOERROR;

    TRACE("(0x%08x, %d, %p);\n", hao, uMetric, pMetric);

#define CheckLocal(padid) (!bLocal || ((padid)->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_LOCAL))

    switch (uMetric) {
    case ACM_METRIC_COUNT_DRIVERS:
	bLocal = FALSE;
	/* fall through */
    case ACM_METRIC_COUNT_LOCAL_DRIVERS:
	if (hao) return MMSYSERR_INVALHANDLE;
	for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID)
	    if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) && CheckLocal(padid))
		val++;
	*(LPDWORD)pMetric = val;
	break;

    case ACM_METRIC_COUNT_CODECS:
	bLocal = FALSE;
	/* fall through */
    case ACM_METRIC_COUNT_LOCAL_CODECS:
	if (hao) return MMSYSERR_INVALHANDLE;
	for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID)
	    if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC) &&
		CheckLocal(padid))
		val++;
	*(LPDWORD)pMetric = val;
	break;

    case ACM_METRIC_COUNT_CONVERTERS:
	bLocal = FALSE;
	/* fall through */
    case ACM_METRIC_COUNT_LOCAL_CONVERTERS:
	if (hao) return MMSYSERR_INVALHANDLE;
	for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID)
	    if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		 (padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CONVERTER) &&
		CheckLocal(padid))
		val++;
	*(LPDWORD)pMetric = val;
	break;

    case ACM_METRIC_COUNT_FILTERS:
	bLocal = FALSE;
	/* fall through */
    case ACM_METRIC_COUNT_LOCAL_FILTERS:
	if (hao) return MMSYSERR_INVALHANDLE;
	for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID)
	    if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_FILTER) &&
		CheckLocal(padid))
		val++;
	*(LPDWORD)pMetric = val;
	break;

    case ACM_METRIC_COUNT_DISABLED:
	bLocal = FALSE;
	/* fall through */
    case ACM_METRIC_COUNT_LOCAL_DISABLED:
	if (hao) return MMSYSERR_INVALHANDLE;
	for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID)
	    if ((padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) && CheckLocal(padid))
		val++;
	*(LPDWORD)pMetric = val;
	break;

    case ACM_METRIC_MAX_SIZE_FORMAT:
	if (hao == (HACMOBJ)NULL) {
	    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
		if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED)) {
		    for (i = 0; i < padid->cFormatTags; i++) {
			if (val < padid->aFormatTag[i].cbwfx)
			    val = padid->aFormatTag[i].cbwfx;
		    }
		}
	    }
	} else if (pao != NULL) {
	    switch (pao->dwType) {
	    case WINE_ACMOBJ_DRIVER:
	    case WINE_ACMOBJ_DRIVERID:
		padid = pao->pACMDriverID;
		break;
	    default:
		return MMSYSERR_INVALHANDLE;
	    }
	    if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED)) {
		for (i = 0; i < padid->cFormatTags; i++) {
		    if (val < padid->aFormatTag[i].cbwfx)
			val = padid->aFormatTag[i].cbwfx;
		}
	    }
	} else {
	    return MMSYSERR_INVALHANDLE;
	}
	*(LPDWORD)pMetric = val;
        break;

    case ACM_METRIC_COUNT_HARDWARE:
    case ACM_METRIC_HARDWARE_WAVE_INPUT:
    case ACM_METRIC_HARDWARE_WAVE_OUTPUT:
    case ACM_METRIC_MAX_SIZE_FILTER:
    case ACM_METRIC_DRIVER_SUPPORT:
    case ACM_METRIC_DRIVER_PRIORITY:
    default:
	FIXME("(0x%08x, %d, %p): stub\n", hao, uMetric, pMetric);
	mmr = MMSYSERR_NOTSUPPORTED;
    }
    return mmr;
}
