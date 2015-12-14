/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 *      MSACM32 library
 *
 *      Copyright 1998  Patrik Stridvall
 *		  2000	Eric Pouech
 */

#include <string.h>
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "windef.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "mmsystem.h"
#include "msacm.h"
#include "msacmdrv.h"
#include "wineacm.h"

WINE_DEFAULT_DEBUG_CHANNEL(msacm);

static	PACMFORMATCHOOSEA	afc;

struct MSACM_FillFormatData {
    HWND		hWnd;
#define WINE_ACMFF_TAG		0
#define WINE_ACMFF_FORMAT	1
#define WINE_ACMFF_WFX		2
    int			mode;
    char		szFormatTag[ACMFORMATTAGDETAILS_FORMATTAG_CHARS];
    PACMFORMATCHOOSEA	afc;
    DWORD		ret;
};

static BOOL CALLBACK MSACM_FillFormatTagsCB(HACMDRIVERID hadid,
					    PACMFORMATTAGDETAILSA paftd,
					    DWORD dwInstance, DWORD fdwSupport)
{
    struct MSACM_FillFormatData*	affd = (struct MSACM_FillFormatData*)dwInstance;

    switch (affd->mode) {
    case WINE_ACMFF_TAG:
	if (SendDlgItemMessageA(affd->hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG,
				CB_FINDSTRINGEXACT,
				(WPARAM)-1, (LPARAM)paftd->szFormatTag) == CB_ERR)
	    SendDlgItemMessageA(affd->hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG,
				CB_ADDSTRING, 0, (DWORD)paftd->szFormatTag);
	break;
    case WINE_ACMFF_FORMAT:
	if (strcmp(affd->szFormatTag, paftd->szFormatTag) == 0) {
	    HACMDRIVER		had;

	    if (acmDriverOpen(&had, hadid, 0) == MMSYSERR_NOERROR) {
		ACMFORMATDETAILSA	afd;
		int			i, idx;
		MMRESULT		mmr;
		char			buffer[ACMFORMATDETAILS_FORMAT_CHARS+16];

		afd.cbStruct = sizeof(afd);
		afd.dwFormatTag = paftd->dwFormatTag;
		afd.pwfx = HeapAlloc(MSACM_hHeap, 0, paftd->cbFormatSize);
		if (!afd.pwfx) return FALSE;
		afd.pwfx->wFormatTag = paftd->dwFormatTag;
		afd.pwfx->cbSize = paftd->cbFormatSize;
		afd.cbwfx = paftd->cbFormatSize;

		for (i = 0; i < paftd->cStandardFormats; i++) {
		    afd.dwFormatIndex = i;
		    mmr = acmFormatDetailsA(had, &afd, ACM_FORMATDETAILSF_INDEX);
		    if (mmr == MMSYSERR_NOERROR) {
			strncpy(buffer, afd.szFormat, ACMFORMATTAGDETAILS_FORMATTAG_CHARS);
			for (idx = strlen(buffer);
			     idx < ACMFORMATTAGDETAILS_FORMATTAG_CHARS; idx++)
			    buffer[idx] = ' ';
			wsprintfA(buffer + ACMFORMATTAGDETAILS_FORMATTAG_CHARS,
				  "%d Ko/s",
				  (afd.pwfx->nAvgBytesPerSec + 512) / 1024);
			SendDlgItemMessageA(affd->hWnd,
					    IDD_ACMFORMATCHOOSE_CMB_FORMAT,
					    CB_ADDSTRING, 0, (DWORD)buffer);
		    }
		}
		acmDriverClose(had, 0);
		SendDlgItemMessageA(affd->hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMAT,
				    CB_SETCURSEL, 0, 0);
		HeapFree(MSACM_hHeap, 0, afd.pwfx);
	    }
	}
	break;
    case WINE_ACMFF_WFX:
	if (strcmp(affd->szFormatTag, paftd->szFormatTag) == 0) {
	    HACMDRIVER		had;

	    if (acmDriverOpen(&had, hadid, 0) == MMSYSERR_NOERROR) {
		ACMFORMATDETAILSA	afd;

		afd.cbStruct = sizeof(afd);
		afd.dwFormatTag = paftd->dwFormatTag;
		afd.pwfx = affd->afc->pwfx;
		afd.cbwfx = affd->afc->cbwfx;

		afd.dwFormatIndex = SendDlgItemMessageA(affd->hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMAT,
							CB_GETCURSEL, 0, 0);
		affd->ret = acmFormatDetailsA(had, &afd, ACM_FORMATDETAILSF_INDEX);
		acmDriverClose(had, 0);
		return TRUE;
	    }
	}
	break;
    default:
	FIXME("Unknown mode (%d)\n", affd->mode);
	break;
    }
    return TRUE;
}

static BOOL MSACM_FillFormatTags(HWND hWnd)
{
    ACMFORMATTAGDETAILSA	aftd;
    struct MSACM_FillFormatData	affd;

    memset(&aftd, 0, sizeof(aftd));
    aftd.cbStruct = sizeof(aftd);

    affd.hWnd = hWnd;
    affd.mode = WINE_ACMFF_TAG;

    acmFormatTagEnumA((HACMDRIVER)0, &aftd, MSACM_FillFormatTagsCB, (DWORD)&affd, 0);
    SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG, CB_SETCURSEL, 0, 0);
    return TRUE;
}

static BOOL MSACM_FillFormat(HWND hWnd)
{
    ACMFORMATTAGDETAILSA	aftd;
    struct MSACM_FillFormatData	affd;

    SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMAT, CB_RESETCONTENT, 0, 0);

    memset(&aftd, 0, sizeof(aftd));
    aftd.cbStruct = sizeof(aftd);

    affd.hWnd = hWnd;
    affd.mode = WINE_ACMFF_FORMAT;
    SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG,
			CB_GETLBTEXT,
			SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG,
					    CB_GETCURSEL, 0, 0),
			(DWORD)affd.szFormatTag);

    acmFormatTagEnumA((HACMDRIVER)0, &aftd, MSACM_FillFormatTagsCB, (DWORD)&affd, 0);
    SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMAT, CB_SETCURSEL, 0, 0);
    return TRUE;
}

static MMRESULT MSACM_GetWFX(HWND hWnd, PACMFORMATCHOOSEA afc)
{
    ACMFORMATTAGDETAILSA	aftd;
    struct MSACM_FillFormatData	affd;

    memset(&aftd, 0, sizeof(aftd));
    aftd.cbStruct = sizeof(aftd);

    affd.hWnd = hWnd;
    affd.mode = WINE_ACMFF_WFX;
    affd.afc = afc;
    affd.ret = MMSYSERR_NOERROR;
    SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG,
			CB_GETLBTEXT,
			SendDlgItemMessageA(hWnd, IDD_ACMFORMATCHOOSE_CMB_FORMATTAG,
					    CB_GETCURSEL, 0, 0),
			(DWORD)affd.szFormatTag);

    acmFormatTagEnumA((HACMDRIVER)0, &aftd, MSACM_FillFormatTagsCB, (DWORD)&affd, 0);
    return affd.ret;
}

static BOOL WINAPI FormatChooseDlgProc(HWND hWnd, UINT msg,
				       WPARAM wParam, LPARAM lParam)
{

    TRACE("hwnd=%i msg=%i 0x%08x 0x%08lx\n", hWnd,  msg, wParam, lParam );

    switch (msg) {
    case WM_INITDIALOG:
	afc = (PACMFORMATCHOOSEA)lParam;
	MSACM_FillFormatTags(hWnd);
	MSACM_FillFormat(hWnd);
	if ((afc->fdwStyle & ~(ACMFORMATCHOOSE_STYLEF_CONTEXTHELP|
			       ACMFORMATCHOOSE_STYLEF_SHOWHELP)) != 0)
	    FIXME("Unsupported style %08lx\n", ((PACMFORMATCHOOSEA)lParam)->fdwStyle);
	if (!(afc->fdwStyle & ACMFORMATCHOOSE_STYLEF_SHOWHELP))
	    ShowWindow(GetDlgItem(hWnd, IDD_ACMFORMATCHOOSE_BTN_HELP), SW_HIDE);
	return TRUE;

    case WM_COMMAND:
	switch (LOWORD(wParam)) {
	case IDOK:
	    EndDialog(hWnd, MSACM_GetWFX(hWnd, afc));
	    return TRUE;
	case IDCANCEL:
	    EndDialog(hWnd, ACMERR_CANCELED);
	    return TRUE;
	case IDD_ACMFORMATCHOOSE_CMB_FORMATTAG:
	    switch (HIWORD(wParam)) {
	    case CBN_SELCHANGE:
		MSACM_FillFormat(hWnd);
		break;
	    default:
		TRACE("Dropped dlgNotif (fmtTag): 0x%08x 0x%08lx\n",
		      HIWORD(wParam), lParam);
		break;
	    }
	    break;
	case IDD_ACMFORMATCHOOSE_BTN_HELP:
	    if (afc->fdwStyle & ACMFORMATCHOOSE_STYLEF_SHOWHELP)
		SendMessageA(afc->hwndOwner,
			     RegisterWindowMessageA(ACMHELPMSGSTRINGA), 0L, 0L);
	    break;

	default:
	    TRACE("Dropped dlgCmd: ctl=%d ntf=0x%04x 0x%08lx\n",
		  LOWORD(wParam), HIWORD(wParam), lParam);
	    break;
	}
	break;
    case WM_CONTEXTMENU:
	if (afc->fdwStyle & ACMFORMATCHOOSE_STYLEF_CONTEXTHELP)
	    SendMessageA(afc->hwndOwner,
			 RegisterWindowMessageA(ACMHELPMSGCONTEXTMENUA),
			 wParam, lParam);
	break;
#if defined(WM_CONTEXTHELP)
    case WM_CONTEXTHELP:
	if (afc->fdwStyle & ACMFORMATCHOOSE_STYLEF_CONTEXTHELP)
	    SendMessageA(afc->hwndOwner,
			 RegisterWindowMessageA(ACMHELPMSGCONTEXTHELPA),
			 wParam, lParam);
	break;
#endif
    default:
	TRACE("Dropped dlgMsg: hwnd=%i msg=%i 0x%08x 0x%08lx\n",
	      hWnd,  msg, wParam, lParam );
	break;
    }
    return FALSE;
}

/***********************************************************************
 *           acmFormatChooseA (MSACM32.@)
 */
MMRESULT WINAPI acmFormatChooseA(PACMFORMATCHOOSEA pafmtc)
{
    return DialogBoxParamA(MSACM_hInstance32, MAKEINTRESOURCEA(DLG_ACMFORMATCHOOSE_ID),
			   pafmtc->hwndOwner, FormatChooseDlgProc, (INT)pafmtc);
}

/***********************************************************************
 *           acmFormatChooseW (MSACM32.@)
 */
MMRESULT WINAPI acmFormatChooseW(PACMFORMATCHOOSEW pafmtc)
{
    FIXME("(%p): stub\n", pafmtc);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return MMSYSERR_ERROR;
}

/***********************************************************************
 *           acmFormatDetailsA (MSACM32.@)
 */
MMRESULT WINAPI acmFormatDetailsA(HACMDRIVER had, PACMFORMATDETAILSA pafd,
				  DWORD fdwDetails)
{
    ACMFORMATDETAILSW	afdw;
    MMRESULT		mmr;

    memset(&afdw, 0, sizeof(afdw));
    afdw.cbStruct = sizeof(afdw);
    afdw.dwFormatIndex = pafd->dwFormatIndex;
    afdw.dwFormatTag = pafd->dwFormatTag;
    afdw.pwfx = pafd->pwfx;
    afdw.cbwfx = pafd->cbwfx;

    mmr = acmFormatDetailsW(had, &afdw, fdwDetails);
    if (mmr == MMSYSERR_NOERROR) {
	pafd->dwFormatTag = afdw.dwFormatTag;
	pafd->fdwSupport = afdw.fdwSupport;
        WideCharToMultiByte( CP_ACP, 0, afdw.szFormat, -1,
                             pafd->szFormat, sizeof(pafd->szFormat), NULL, NULL );
    }
    return mmr;
}

/***********************************************************************
 *           acmFormatDetailsW (MSACM32.@)
 */
MMRESULT WINAPI acmFormatDetailsW(HACMDRIVER had, PACMFORMATDETAILSW pafd, DWORD fdwDetails)
{
    MMRESULT			mmr;
    static WCHAR		fmt1[] = {'%','d',' ','H','z',0};
    static WCHAR		fmt2[] = {';',' ','%','d',' ','b','i','t','s',0};
    ACMFORMATTAGDETAILSA	aftd;

    TRACE("(0x%08x, %p, %ld)\n", had, pafd, fdwDetails);

    memset(&aftd, 0, sizeof(aftd));
    aftd.cbStruct = sizeof(aftd);

    if (pafd->cbStruct < sizeof(*pafd)) return MMSYSERR_INVALPARAM;

    switch (fdwDetails) {
    case ACM_FORMATDETAILSF_FORMAT:
	if (pafd->dwFormatTag != pafd->pwfx->wFormatTag) {
	    mmr = MMSYSERR_INVALPARAM;
	    break;
	}
	if (had == (HACMDRIVER)NULL) {
	    PWINE_ACMDRIVERID		padid;

	    mmr = ACMERR_NOTPOSSIBLE;
	    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
		/* should check for codec only */
		if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		    acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == 0) {
		    mmr = MSACM_Message(had, ACMDM_FORMAT_DETAILS, (LPARAM)pafd, fdwDetails);
		    acmDriverClose(had, 0);
		    if (mmr == MMSYSERR_NOERROR) break;
		}
	    }
	} else {
	    mmr = MSACM_Message(had, ACMDM_FORMAT_DETAILS, (LPARAM)pafd, fdwDetails);
	}
	break;
    case ACM_FORMATDETAILSF_INDEX:
	/* should check pafd->dwFormatIndex < aftd->cStandardFormats */
	mmr = MSACM_Message(had, ACMDM_FORMAT_DETAILS, (LPARAM)pafd, fdwDetails);
	break;
    default:
	WARN("Unknown fdwDetails %08lx\n", fdwDetails);
	mmr = MMSYSERR_INVALFLAG;
	break;
    }

    if (mmr == MMSYSERR_NOERROR && pafd->szFormat[0] == (WCHAR)0) {
	wsprintfW(pafd->szFormat, fmt1, pafd->pwfx->nSamplesPerSec);
	if (pafd->pwfx->wBitsPerSample) {
	    wsprintfW(pafd->szFormat + lstrlenW(pafd->szFormat), fmt2,
		      pafd->pwfx->wBitsPerSample);
	}
        MultiByteToWideChar( CP_ACP, 0, (pafd->pwfx->nChannels == 1) ? "; Mono" : "; Stereo", -1,
                             pafd->szFormat + strlenW(pafd->szFormat),
                             sizeof(pafd->szFormat)/sizeof(WCHAR) - strlenW(pafd->szFormat) );
    }

    TRACE("=> %d\n", mmr);
    return mmr;
}

struct MSACM_FormatEnumWtoA_Instance {
    PACMFORMATDETAILSA	pafda;
    DWORD		dwInstance;
    ACMFORMATENUMCBA 	fnCallback;
};

static BOOL CALLBACK MSACM_FormatEnumCallbackWtoA(HACMDRIVERID hadid,
						  PACMFORMATDETAILSW pafdw,
						  DWORD dwInstance,
						  DWORD fdwSupport)
{
    struct MSACM_FormatEnumWtoA_Instance* pafei;

    pafei = (struct MSACM_FormatEnumWtoA_Instance*)dwInstance;

    pafei->pafda->dwFormatIndex = pafdw->dwFormatIndex;
    pafei->pafda->dwFormatTag = pafdw->dwFormatTag;
    pafei->pafda->fdwSupport = pafdw->fdwSupport;
    WideCharToMultiByte( CP_ACP, 0, pafdw->szFormat, -1,
                         pafei->pafda->szFormat, sizeof(pafei->pafda->szFormat), NULL, NULL );

    return (pafei->fnCallback)(hadid, pafei->pafda,
			       pafei->dwInstance, fdwSupport);
}

/***********************************************************************
 *           acmFormatEnumA (MSACM32.@)
 */
MMRESULT WINAPI acmFormatEnumA(HACMDRIVER had, PACMFORMATDETAILSA pafda,
			       ACMFORMATENUMCBA fnCallback, DWORD dwInstance,
			       DWORD fdwEnum)
{
    ACMFORMATDETAILSW		afdw;
    struct MSACM_FormatEnumWtoA_Instance afei;

    memset(&afdw, 0, sizeof(afdw));
    afdw.cbStruct = sizeof(afdw);
    afdw.dwFormatIndex = pafda->dwFormatIndex;
    afdw.dwFormatTag = pafda->dwFormatTag;
    afdw.pwfx = pafda->pwfx;
    afdw.cbwfx = pafda->cbwfx;

    afei.pafda = pafda;
    afei.dwInstance = dwInstance;
    afei.fnCallback = fnCallback;

    return acmFormatEnumW(had, &afdw, MSACM_FormatEnumCallbackWtoA,
			  (DWORD)&afei, fdwEnum);
}

/***********************************************************************
 *           acmFormatEnumW (MSACM32.@)
 */
static BOOL MSACM_FormatEnumHelper(PWINE_ACMDRIVERID padid, HACMDRIVER had,
				   PACMFORMATDETAILSW pafd, PWAVEFORMATEX pwfxRef,
				   ACMFORMATENUMCBW fnCallback, DWORD dwInstance,
				   DWORD fdwEnum)
{
    ACMFORMATTAGDETAILSW	aftd;
    int				i, j;

    for (i = 0; i < padid->cFormatTags; i++) {
	memset(&aftd, 0, sizeof(aftd));
	aftd.cbStruct = sizeof(aftd);
	aftd.dwFormatTagIndex = i;
	if (acmFormatTagDetailsW(had, &aftd, ACM_FORMATTAGDETAILSF_INDEX) != MMSYSERR_NOERROR)
	    continue;

	if ((fdwEnum & ACM_FORMATENUMF_WFORMATTAG) && aftd.dwFormatTag != pwfxRef->wFormatTag)
	    continue;

	for (j = 0; j < aftd.cStandardFormats; j++) {
	    pafd->dwFormatIndex = j;
	    pafd->dwFormatTag = aftd.dwFormatTag;
	    if (acmFormatDetailsW(had, pafd, ACM_FORMATDETAILSF_INDEX) != MMSYSERR_NOERROR)
		continue;

	    if ((fdwEnum & ACM_FORMATENUMF_NCHANNELS) &&
		pafd->pwfx->nChannels != pwfxRef->nChannels)
		continue;
	    if ((fdwEnum & ACM_FORMATENUMF_NSAMPLESPERSEC) &&
		pafd->pwfx->nSamplesPerSec != pwfxRef->nSamplesPerSec)
		continue;
	    if ((fdwEnum & ACM_FORMATENUMF_WBITSPERSAMPLE) &&
		pafd->pwfx->wBitsPerSample != pwfxRef->wBitsPerSample)
		continue;
	    if ((fdwEnum & ACM_FORMATENUMF_HARDWARE) &&
		!(pafd->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_HARDWARE))
		continue;

	    /* more checks to be done on fdwEnum */

	    if (!(fnCallback)((HACMDRIVERID)padid, pafd, dwInstance, padid->fdwSupport))
		return FALSE;
	}
	/* the "formats" used by the filters are also reported */
    }
    return TRUE;
}

/**********************************************************************/

MMRESULT WINAPI acmFormatEnumW(HACMDRIVER had, PACMFORMATDETAILSW pafd,
			       ACMFORMATENUMCBW fnCallback, DWORD dwInstance,
			       DWORD fdwEnum)
{
    PWINE_ACMDRIVERID		padid;
    WAVEFORMATEX		wfxRef;
    BOOL			ret;

    TRACE("(0x%08x, %p, %p, %ld, %ld)\n",
	  had, pafd, fnCallback, dwInstance, fdwEnum);

    if (pafd->cbStruct < sizeof(*pafd)) return MMSYSERR_INVALPARAM;

    if (fdwEnum & (ACM_FORMATENUMF_WFORMATTAG|ACM_FORMATENUMF_NCHANNELS|
		   ACM_FORMATENUMF_NSAMPLESPERSEC|ACM_FORMATENUMF_WBITSPERSAMPLE|
		   ACM_FORMATENUMF_CONVERT|ACM_FORMATENUMF_SUGGEST))
        wfxRef = *pafd->pwfx;

    if ((fdwEnum & ACM_FORMATENUMF_HARDWARE) &&
	!(fdwEnum & (ACM_FORMATENUMF_INPUT|ACM_FORMATENUMF_OUTPUT)))
	return MMSYSERR_INVALPARAM;

    if ((fdwEnum & ACM_FORMATENUMF_WFORMATTAG) &&
	(pafd->dwFormatTag != pafd->pwfx->wFormatTag))
	return MMSYSERR_INVALPARAM;

    if (fdwEnum & (ACM_FORMATENUMF_CONVERT|ACM_FORMATENUMF_SUGGEST|
		   ACM_FORMATENUMF_INPUT|ACM_FORMATENUMF_OUTPUT))
	FIXME("Unsupported fdwEnum values %08lx\n", fdwEnum);

    if (had) {
	HACMDRIVERID	hadid;

	if (acmDriverID((HACMOBJ)had, &hadid, 0) != MMSYSERR_NOERROR)
	    return MMSYSERR_INVALHANDLE;
	MSACM_FormatEnumHelper(MSACM_GetDriverID(hadid), had, pafd, &wfxRef,
			       fnCallback, dwInstance, fdwEnum);
	return MMSYSERR_NOERROR;
    }
    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
	    /* should check for codec only */
	    if ((padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) ||
		acmDriverOpen(&had, (HACMDRIVERID)padid, 0) != MMSYSERR_NOERROR)
		continue;
	    ret = MSACM_FormatEnumHelper(padid, had, pafd, &wfxRef,
					 fnCallback, dwInstance, fdwEnum);
	    acmDriverClose(had, 0);
	    if (!ret) break;
    }
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmFormatSuggest (MSACM32.@)
 */
MMRESULT WINAPI acmFormatSuggest(HACMDRIVER had, PWAVEFORMATEX pwfxSrc,
				 PWAVEFORMATEX pwfxDst, DWORD cbwfxDst, DWORD fdwSuggest)
{
    ACMDRVFORMATSUGGEST	adfg;
    MMRESULT		mmr;

    TRACE("(0x%08x, %p, %p, %ld, %ld)\n",
	  had, pwfxSrc, pwfxDst, cbwfxDst, fdwSuggest);

    if (fdwSuggest & ~(ACM_FORMATSUGGESTF_NCHANNELS|ACM_FORMATSUGGESTF_NSAMPLESPERSEC|
		       ACM_FORMATSUGGESTF_WBITSPERSAMPLE|ACM_FORMATSUGGESTF_WFORMATTAG))
	return MMSYSERR_INVALFLAG;

    adfg.cbStruct = sizeof(adfg);
    adfg.fdwSuggest = fdwSuggest;
    adfg.pwfxSrc = pwfxSrc;
    adfg.cbwfxSrc = (pwfxSrc->wFormatTag == WAVE_FORMAT_PCM) ?
	sizeof(WAVEFORMATEX) : (sizeof(WAVEFORMATEX) + pwfxSrc->cbSize);
    adfg.pwfxDst = pwfxDst;
    adfg.cbwfxDst = cbwfxDst;

    if (had == (HACMDRIVER)NULL) {
	PWINE_ACMDRIVERID	padid;

	/* MS doc says: ACM finds the best suggestion.
	 * Well, first found will be the "best"
	 */
	mmr = ACMERR_NOTPOSSIBLE;
	for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
	    /* should check for codec only */
	    if ((padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) ||
		acmDriverOpen(&had, (HACMDRIVERID)padid, 0) != MMSYSERR_NOERROR)
		continue;

	    if (MSACM_Message(had, ACMDM_FORMAT_SUGGEST, (LPARAM)&adfg, 0L) == MMSYSERR_NOERROR) {
		mmr = MMSYSERR_NOERROR;
		break;
	    }
	    acmDriverClose(had, 0);
	}
    } else {
	mmr = MSACM_Message(had, ACMDM_FORMAT_SUGGEST, (LPARAM)&adfg, 0L);
    }
    return mmr;
}

/***********************************************************************
 *           acmFormatTagDetailsA (MSACM32.@)
 */
MMRESULT WINAPI acmFormatTagDetailsA(HACMDRIVER had, PACMFORMATTAGDETAILSA paftda,
				     DWORD fdwDetails)
{
    ACMFORMATTAGDETAILSW	aftdw;
    MMRESULT			mmr;

    memset(&aftdw, 0, sizeof(aftdw));
    aftdw.cbStruct = sizeof(aftdw);
    aftdw.dwFormatTagIndex = paftda->dwFormatTagIndex;
    aftdw.dwFormatTag = paftda->dwFormatTag;

    mmr = acmFormatTagDetailsW(had, &aftdw, fdwDetails);
    if (mmr == MMSYSERR_NOERROR) {
	paftda->dwFormatTag = aftdw.dwFormatTag;
	paftda->dwFormatTagIndex = aftdw.dwFormatTagIndex;
	paftda->cbFormatSize = aftdw.cbFormatSize;
	paftda->fdwSupport = aftdw.fdwSupport;
	paftda->cStandardFormats = aftdw.cStandardFormats;
        WideCharToMultiByte( CP_ACP, 0, aftdw.szFormatTag, -1, paftda->szFormatTag,
                             sizeof(paftda->szFormatTag), NULL, NULL );
    }
    return mmr;
}

/***********************************************************************
 *           acmFormatTagDetailsW (MSACM32.@)
 */
MMRESULT WINAPI acmFormatTagDetailsW(HACMDRIVER had, PACMFORMATTAGDETAILSW paftd,
				     DWORD fdwDetails)
{
    PWINE_ACMDRIVERID	padid;
    MMRESULT		mmr = ACMERR_NOTPOSSIBLE;

    TRACE("(0x%08x, %p, %ld)\n", had, paftd, fdwDetails);

    if (fdwDetails & ~(ACM_FORMATTAGDETAILSF_FORMATTAG|ACM_FORMATTAGDETAILSF_INDEX|
		       ACM_FORMATTAGDETAILSF_LARGESTSIZE))
	return MMSYSERR_INVALFLAG;

    switch (fdwDetails) {
    case ACM_FORMATTAGDETAILSF_FORMATTAG:
	if (had == (HACMDRIVER)NULL) {
	    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
		/* should check for codec only */
		if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		    MSACM_FindFormatTagInCache(padid, paftd->dwFormatTag, NULL) &&
		    acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == 0) {
		    mmr = MSACM_Message(had, ACMDM_FORMATTAG_DETAILS, (LPARAM)paftd, fdwDetails);
		    acmDriverClose(had, 0);
		    if (mmr == MMSYSERR_NOERROR) break;
		}
	    }
	} else {
	    PWINE_ACMDRIVER	pad = MSACM_GetDriver(had);

	    if (pad && MSACM_FindFormatTagInCache(pad->obj.pACMDriverID, paftd->dwFormatTag, NULL))
		mmr = MSACM_Message(had, ACMDM_FORMATTAG_DETAILS, (LPARAM)paftd, fdwDetails);
	}
	break;

    case ACM_FORMATTAGDETAILSF_INDEX:
	if (had != (HACMDRIVER)NULL) {
	    PWINE_ACMDRIVER	pad = MSACM_GetDriver(had);

	    if (pad && paftd->dwFormatTagIndex < pad->obj.pACMDriverID->cFormatTags)
		mmr = MSACM_Message(had, ACMDM_FORMATTAG_DETAILS, (LPARAM)paftd, fdwDetails);
	}
	break;

    case ACM_FORMATTAGDETAILSF_LARGESTSIZE:
	if (had == (HACMDRIVER)NULL) {
	    ACMFORMATTAGDETAILSW	tmp;
	    DWORD			ft = paftd->dwFormatTag;

	    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
		/* should check for codec only */
		if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		    acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == 0) {

		    memset(&tmp, 0, sizeof(tmp));
		    tmp.cbStruct = sizeof(tmp);
		    tmp.dwFormatTag = ft;

		    if (MSACM_Message(had, ACMDM_FORMATTAG_DETAILS,
				      (LPARAM)&tmp, fdwDetails) == MMSYSERR_NOERROR) {
			if (mmr == ACMERR_NOTPOSSIBLE ||
			    paftd->cbFormatSize < tmp.cbFormatSize) {
			    *paftd = tmp;
			    mmr = MMSYSERR_NOERROR;
			}
		    }
		    acmDriverClose(had, 0);
		}
	    }
	} else {
	    mmr = MSACM_Message(had, ACMDM_FORMATTAG_DETAILS, (LPARAM)paftd, fdwDetails);
	}
	break;

    default:
	WARN("Unsupported fdwDetails=%08lx\n", fdwDetails);
	mmr = MMSYSERR_ERROR;
    }

    if (mmr == MMSYSERR_NOERROR &&
	paftd->dwFormatTag == WAVE_FORMAT_PCM && paftd->szFormatTag[0] == 0)
        MultiByteToWideChar( CP_ACP, 0, "PCM", -1, paftd->szFormatTag,
                             sizeof(paftd->szFormatTag)/sizeof(WCHAR) );

    return mmr;
}

struct MSACM_FormatTagEnumWtoA_Instance {
    PACMFORMATTAGDETAILSA	paftda;
    DWORD			dwInstance;
    ACMFORMATTAGENUMCBA 	fnCallback;
};

static BOOL CALLBACK MSACM_FormatTagEnumCallbackWtoA(HACMDRIVERID hadid,
						     PACMFORMATTAGDETAILSW paftdw,
						     DWORD dwInstance,
						     DWORD fdwSupport)
{
    struct MSACM_FormatTagEnumWtoA_Instance* paftei;

    paftei = (struct MSACM_FormatTagEnumWtoA_Instance*)dwInstance;

    paftei->paftda->dwFormatTagIndex = paftdw->dwFormatTagIndex;
    paftei->paftda->dwFormatTag = paftdw->dwFormatTag;
    paftei->paftda->cbFormatSize = paftdw->cbFormatSize;
    paftei->paftda->fdwSupport = paftdw->fdwSupport;
    paftei->paftda->cStandardFormats = paftdw->cStandardFormats;
    WideCharToMultiByte( CP_ACP, 0, paftdw->szFormatTag, -1, paftei->paftda->szFormatTag,
                         sizeof(paftei->paftda->szFormatTag), NULL, NULL );

    return (paftei->fnCallback)(hadid, paftei->paftda,
				paftei->dwInstance, fdwSupport);
}

/***********************************************************************
 *           acmFormatTagEnumA (MSACM32.@)
 */
MMRESULT WINAPI acmFormatTagEnumA(HACMDRIVER had, PACMFORMATTAGDETAILSA paftda,
				  ACMFORMATTAGENUMCBA fnCallback, DWORD dwInstance,
				  DWORD fdwEnum)
{
    ACMFORMATTAGDETAILSW	aftdw;
    struct MSACM_FormatTagEnumWtoA_Instance aftei;

    memset(&aftdw, 0, sizeof(aftdw));
    aftdw.cbStruct = sizeof(aftdw);
    aftdw.dwFormatTagIndex = paftda->dwFormatTagIndex;
    aftdw.dwFormatTag = paftda->dwFormatTag;

    aftei.paftda = paftda;
    aftei.dwInstance = dwInstance;
    aftei.fnCallback = fnCallback;

    return acmFormatTagEnumW(had, &aftdw, MSACM_FormatTagEnumCallbackWtoA,
			     (DWORD)&aftei, fdwEnum);
}

/***********************************************************************
 *           acmFormatTagEnumW (MSACM32.@)
 */
MMRESULT WINAPI acmFormatTagEnumW(HACMDRIVER had, PACMFORMATTAGDETAILSW paftd,
				  ACMFORMATTAGENUMCBW fnCallback, DWORD dwInstance,
				  DWORD fdwEnum)
{
    PWINE_ACMDRIVERID		padid;
    int				i;
    BOOL			bPcmDone = FALSE;

    TRACE("(0x%08x, %p, %p, %ld, %ld)\n",
	  had, paftd, fnCallback, dwInstance, fdwEnum);

    if (paftd->cbStruct < sizeof(*paftd)) return MMSYSERR_INVALPARAM;

    if (had) FIXME("had != NULL, not supported\n");

    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
	/* should check for codec only */
	if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
	    acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == MMSYSERR_NOERROR) {
	    for (i = 0; i < padid->cFormatTags; i++) {
		paftd->dwFormatTagIndex = i;
		if (MSACM_Message(had, ACMDM_FORMATTAG_DETAILS,
				  (LPARAM)paftd, ACM_FORMATTAGDETAILSF_INDEX) == MMSYSERR_NOERROR) {
		    if (paftd->dwFormatTag == WAVE_FORMAT_PCM) {
			if (paftd->szFormatTag[0] == 0)
			    MultiByteToWideChar( CP_ACP, 0, "PCM", -1, paftd->szFormatTag,
						 sizeof(paftd->szFormatTag)/sizeof(WCHAR) );
			/* FIXME (EPP): I'm not sure this is the correct
			 * algorithm (should make more sense to apply the same
			 * for all already loaded formats, but this will do
			 * for now
			 */
			if (bPcmDone) continue;
			bPcmDone = TRUE;
		    }
		    if (!(fnCallback)((HACMDRIVERID)padid, paftd, dwInstance, padid->fdwSupport)) {
                        acmDriverClose(had, 0);
                        return MMSYSERR_NOERROR;
		    }
		}
	    }
	}
	acmDriverClose(had, 0);
    }
    return MMSYSERR_NOERROR;
}
