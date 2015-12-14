/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 *      MSACM32 library
 *
 *      Copyright 1998  Patrik Stridvall
 */

#include <string.h>
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "mmsystem.h"
#include "msacm.h"
#include "msacmdrv.h"
#include "wineacm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msacm);

/***********************************************************************
 *           acmFilterChooseA (MSACM32.@)
 */
MMRESULT WINAPI acmFilterChooseA(PACMFILTERCHOOSEA pafltrc)
{
    FIXME("(%p): stub\n", pafltrc);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return MMSYSERR_ERROR;
}

/***********************************************************************
 *           acmFilterChooseW (MSACM32.@)
 */
MMRESULT WINAPI acmFilterChooseW(PACMFILTERCHOOSEW pafltrc)
{
    FIXME("(%p): stub\n", pafltrc);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return MMSYSERR_ERROR;
}

/***********************************************************************
 *           acmFilterDetailsA (MSACM32.@)
 */
MMRESULT WINAPI acmFilterDetailsA(HACMDRIVER had, PACMFILTERDETAILSA pafd,
				  DWORD fdwDetails)
{
    ACMFILTERDETAILSW	afdw;
    MMRESULT		mmr;

    memset(&afdw, 0, sizeof(afdw));
    afdw.cbStruct = sizeof(afdw);
    afdw.dwFilterIndex = pafd->dwFilterIndex;
    afdw.dwFilterTag = pafd->dwFilterTag;
    afdw.pwfltr = pafd->pwfltr;
    afdw.cbwfltr = pafd->cbwfltr;

    mmr = acmFilterDetailsW(had, &afdw, fdwDetails);
    if (mmr == MMSYSERR_NOERROR) {
	pafd->dwFilterTag = afdw.dwFilterTag;
	pafd->fdwSupport = afdw.fdwSupport;
        WideCharToMultiByte( CP_ACP, 0, afdw.szFilter, -1, pafd->szFilter,
                             sizeof(pafd->szFilter), NULL, NULL );
    }
    return mmr;
}

/***********************************************************************
 *           acmFilterDetailsW (MSACM32.@)
 */
MMRESULT WINAPI acmFilterDetailsW(HACMDRIVER had, PACMFILTERDETAILSW pafd,
				  DWORD fdwDetails)
{
    MMRESULT			mmr;
    ACMFILTERTAGDETAILSA	aftd;

    TRACE("(0x%08x, %p, %ld)\n", had, pafd, fdwDetails);

    memset(&aftd, 0, sizeof(aftd));
    aftd.cbStruct = sizeof(aftd);

    if (pafd->cbStruct < sizeof(*pafd)) return MMSYSERR_INVALPARAM;

    switch (fdwDetails) {
    case ACM_FILTERDETAILSF_FILTER:
	if (pafd->dwFilterTag != pafd->pwfltr->dwFilterTag) {
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
		    mmr = MSACM_Message(had, ACMDM_FILTER_DETAILS,
					(LPARAM)pafd, (LPARAM)fdwDetails);
		    acmDriverClose(had, 0);
		    if (mmr == MMSYSERR_NOERROR) break;
		}
	    }
	} else {
	    mmr = MSACM_Message(had, ACMDM_FILTER_DETAILS, (LPARAM)pafd, fdwDetails);
	}
	break;
    case ACM_FILTERDETAILSF_INDEX:
	/* should check pafd->dwFilterIndex < aftd->cStandardFilters */
	mmr = MSACM_Message(had, ACMDM_FILTER_DETAILS, (LPARAM)pafd, fdwDetails);
	break;
    default:
	WARN("Unknown fdwDetails %08lx\n", fdwDetails);
	mmr = MMSYSERR_INVALFLAG;
	break;
    }

    TRACE("=> %d\n", mmr);
    return mmr;
}

struct MSACM_FilterEnumWtoA_Instance {
    PACMFILTERDETAILSA	pafda;
    DWORD		dwInstance;
    ACMFILTERENUMCBA 	fnCallback;
};

static BOOL CALLBACK MSACM_FilterEnumCallbackWtoA(HACMDRIVERID hadid,
						  PACMFILTERDETAILSW pafdw,
						  DWORD dwInstance,
						  DWORD fdwSupport)
{
    struct MSACM_FilterEnumWtoA_Instance* pafei;

    pafei = (struct MSACM_FilterEnumWtoA_Instance*)dwInstance;

    pafei->pafda->dwFilterIndex = pafdw->dwFilterIndex;
    pafei->pafda->dwFilterTag = pafdw->dwFilterTag;
    pafei->pafda->fdwSupport = pafdw->fdwSupport;
    WideCharToMultiByte( CP_ACP, 0, pafdw->szFilter, -1, pafei->pafda->szFilter,
                         sizeof(pafei->pafda->szFilter), NULL, NULL );

    return (pafei->fnCallback)(hadid, pafei->pafda,
			       pafei->dwInstance, fdwSupport);
}

/***********************************************************************
 *           acmFilterEnumA (MSACM32.@)
 */
MMRESULT WINAPI acmFilterEnumA(HACMDRIVER had, PACMFILTERDETAILSA pafda,
			       ACMFILTERENUMCBA fnCallback, DWORD dwInstance,
			       DWORD fdwEnum)
{
    ACMFILTERDETAILSW		afdw;
    struct MSACM_FilterEnumWtoA_Instance afei;

    memset(&afdw, 0, sizeof(afdw));
    afdw.cbStruct = sizeof(afdw);
    afdw.dwFilterIndex = pafda->dwFilterIndex;
    afdw.dwFilterTag = pafda->dwFilterTag;
    afdw.pwfltr = pafda->pwfltr;
    afdw.cbwfltr = pafda->cbwfltr;

    afei.pafda = pafda;
    afei.dwInstance = dwInstance;
    afei.fnCallback = fnCallback;

    return acmFilterEnumW(had, &afdw, MSACM_FilterEnumCallbackWtoA,
			  (DWORD)&afei, fdwEnum);
}

static BOOL MSACM_FilterEnumHelper(PWINE_ACMDRIVERID padid, HACMDRIVER had,
				   PACMFILTERDETAILSW pafd,
				   ACMFILTERENUMCBW fnCallback, DWORD dwInstance,
				   DWORD fdwEnum)
{
    ACMFILTERTAGDETAILSW	aftd;
    int				i, j;

    for (i = 0; i < padid->cFilterTags; i++) {
	memset(&aftd, 0, sizeof(aftd));
	aftd.cbStruct = sizeof(aftd);
	aftd.dwFilterTagIndex = i;
	if (acmFilterTagDetailsW(had, &aftd, ACM_FILTERTAGDETAILSF_INDEX) != MMSYSERR_NOERROR)
	    continue;

	if ((fdwEnum & ACM_FILTERENUMF_DWFILTERTAG) &&
	    aftd.dwFilterTag != pafd->pwfltr->dwFilterTag)
	    continue;

	for (j = 0; j < aftd.cStandardFilters; j++) {
	    pafd->dwFilterIndex = j;
	    pafd->dwFilterTag = aftd.dwFilterTag;
	    if (acmFilterDetailsW(had, pafd, ACM_FILTERDETAILSF_INDEX) != MMSYSERR_NOERROR)
		continue;

	    if (!(fnCallback)((HACMDRIVERID)padid, pafd, dwInstance, padid->fdwSupport))
		return FALSE;
	}
    }
    return TRUE;
}

/***********************************************************************
 *           acmFilterEnumW (MSACM32.@)
 */
MMRESULT WINAPI acmFilterEnumW(HACMDRIVER had, PACMFILTERDETAILSW pafd,
			       ACMFILTERENUMCBW fnCallback, DWORD dwInstance,
			       DWORD fdwEnum)
{
    PWINE_ACMDRIVERID		padid;
    BOOL			ret;

    TRACE("(0x%08x, %p, %p, %ld, %ld)\n",
	  had, pafd, fnCallback, dwInstance, fdwEnum);

    if (pafd->cbStruct < sizeof(*pafd)) return MMSYSERR_INVALPARAM;

    if (fdwEnum & ~(ACM_FILTERENUMF_DWFILTERTAG))
	FIXME("Unsupported fdwEnum values\n");

    if (had) {
	HACMDRIVERID	hadid;

	if (acmDriverID((HACMOBJ)had, &hadid, 0) != MMSYSERR_NOERROR)
	    return MMSYSERR_INVALHANDLE;
	MSACM_FilterEnumHelper(MSACM_GetDriverID(hadid), had, pafd,
			       fnCallback, dwInstance, fdwEnum);
	return MMSYSERR_NOERROR;
    }
    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
	    /* should check for codec only */
	    if ((padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) ||
		acmDriverOpen(&had, (HACMDRIVERID)padid, 0) != MMSYSERR_NOERROR)
		continue;
	    ret = MSACM_FilterEnumHelper(padid, had, pafd,
					 fnCallback, dwInstance, fdwEnum);
	    acmDriverClose(had, 0);
	    if (!ret) break;
    }
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmFilterTagDetailsA (MSACM32.@)
 */
MMRESULT WINAPI acmFilterTagDetailsA(HACMDRIVER had, PACMFILTERTAGDETAILSA paftda,
				     DWORD fdwDetails)
{
    ACMFILTERTAGDETAILSW	aftdw;
    MMRESULT			mmr;

    memset(&aftdw, 0, sizeof(aftdw));
    aftdw.cbStruct = sizeof(aftdw);
    aftdw.dwFilterTagIndex = paftda->dwFilterTagIndex;
    aftdw.dwFilterTag = paftda->dwFilterTag;

    mmr = acmFilterTagDetailsW(had, &aftdw, fdwDetails);
    if (mmr == MMSYSERR_NOERROR) {
	paftda->dwFilterTag = aftdw.dwFilterTag;
	paftda->dwFilterTagIndex = aftdw.dwFilterTagIndex;
	paftda->cbFilterSize = aftdw.cbFilterSize;
	paftda->fdwSupport = aftdw.fdwSupport;
	paftda->cStandardFilters = aftdw.cStandardFilters;
        WideCharToMultiByte( CP_ACP, 0, aftdw.szFilterTag, -1, paftda->szFilterTag,
                             sizeof(paftda->szFilterTag), NULL, NULL );
    }
    return mmr;
}

/***********************************************************************
 *           acmFilterTagDetailsW (MSACM32.@)
 */
MMRESULT WINAPI acmFilterTagDetailsW(HACMDRIVER had, PACMFILTERTAGDETAILSW paftd,
				     DWORD fdwDetails)
{
    PWINE_ACMDRIVERID	padid;
    MMRESULT		mmr;

    TRACE("(0x%08x, %p, %ld)\n", had, paftd, fdwDetails);

    if (fdwDetails & ~(ACM_FILTERTAGDETAILSF_FILTERTAG|ACM_FILTERTAGDETAILSF_INDEX|
		       ACM_FILTERTAGDETAILSF_LARGESTSIZE))
	return MMSYSERR_INVALFLAG;

    switch (fdwDetails) {
    case ACM_FILTERTAGDETAILSF_FILTERTAG:
	if (had == (HACMDRIVER)NULL) {
	    mmr = ACMERR_NOTPOSSIBLE;
	    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
		/* should check for codec only */
		if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		      acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == 0) {
		    mmr = MSACM_Message(had, ACMDM_FILTERTAG_DETAILS, (LPARAM)paftd, fdwDetails);
		    acmDriverClose(had, 0);
		    if (mmr == MMSYSERR_NOERROR) break;
		}
	    }
	} else {
	    mmr = MSACM_Message(had, ACMDM_FILTERTAG_DETAILS, (LPARAM)paftd, fdwDetails);
	}
	break;

    case ACM_FILTERTAGDETAILSF_INDEX:
	/* FIXME should check paftd->dwFilterTagIndex < add.cFilterTags */
	mmr = MSACM_Message(had, ACMDM_FILTERTAG_DETAILS, (LPARAM)paftd, fdwDetails);
	break;

    case ACM_FILTERTAGDETAILSF_LARGESTSIZE:
	if (had == (HACMDRIVER)NULL) {
	    ACMFILTERTAGDETAILSW	tmp;
	    DWORD			ft = paftd->dwFilterTag;

	    mmr = ACMERR_NOTPOSSIBLE;
	    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
		/* should check for codec only */
		if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
		    acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == 0) {

		    memset(&tmp, 0, sizeof(tmp));
		    tmp.cbStruct = sizeof(tmp);
		    tmp.dwFilterTag = ft;

		    if (MSACM_Message(had, ACMDM_FILTERTAG_DETAILS,
				      (LPARAM)&tmp, fdwDetails) == MMSYSERR_NOERROR) {
			if (mmr == ACMERR_NOTPOSSIBLE ||
			    paftd->cbFilterSize < tmp.cbFilterSize) {
			    *paftd = tmp;
			    mmr = MMSYSERR_NOERROR;
			}
		    }
		    acmDriverClose(had, 0);
		}
	    }
	} else {
	    mmr = MSACM_Message(had, ACMDM_FILTERTAG_DETAILS, (LPARAM)paftd, fdwDetails);
	}
	break;

    default:
	WARN("Unsupported fdwDetails=%08lx\n", fdwDetails);
	mmr = MMSYSERR_ERROR;
    }

    if (mmr == MMSYSERR_NOERROR &&
	paftd->dwFilterTag == WAVE_FORMAT_PCM && paftd->szFilterTag[0] == 0)
        MultiByteToWideChar( CP_ACP, 0, "PCM", -1, paftd->szFilterTag,
                             sizeof(paftd->szFilterTag)/sizeof(WCHAR) );

    return mmr;
}

struct MSACM_FilterTagEnumWtoA_Instance {
    PACMFILTERTAGDETAILSA	paftda;
    DWORD			dwInstance;
    ACMFILTERTAGENUMCBA 	fnCallback;
};

static BOOL CALLBACK MSACM_FilterTagEnumCallbackWtoA(HACMDRIVERID hadid,
						     PACMFILTERTAGDETAILSW paftdw,
						     DWORD dwInstance,
						     DWORD fdwSupport)
{
    struct MSACM_FilterTagEnumWtoA_Instance* paftei;

    paftei = (struct MSACM_FilterTagEnumWtoA_Instance*)dwInstance;

    paftei->paftda->dwFilterTagIndex = paftdw->dwFilterTagIndex;
    paftei->paftda->dwFilterTag = paftdw->dwFilterTag;
    paftei->paftda->cbFilterSize = paftdw->cbFilterSize;
    paftei->paftda->fdwSupport = paftdw->fdwSupport;
    paftei->paftda->cStandardFilters = paftdw->cStandardFilters;
    WideCharToMultiByte( CP_ACP, 0, paftdw->szFilterTag, -1, paftei->paftda->szFilterTag,
                         sizeof(paftei->paftda->szFilterTag), NULL, NULL );

    return (paftei->fnCallback)(hadid, paftei->paftda,
				paftei->dwInstance, fdwSupport);
}

/***********************************************************************
 *           acmFilterTagEnumA (MSACM32.@)
 */
MMRESULT WINAPI acmFilterTagEnumA(HACMDRIVER had, PACMFILTERTAGDETAILSA paftda,
				  ACMFILTERTAGENUMCBA fnCallback, DWORD dwInstance,
				  DWORD fdwEnum)
{
    ACMFILTERTAGDETAILSW	aftdw;
    struct MSACM_FilterTagEnumWtoA_Instance aftei;

    memset(&aftdw, 0, sizeof(aftdw));
    aftdw.cbStruct = sizeof(aftdw);
    aftdw.dwFilterTagIndex = paftda->dwFilterTagIndex;
    aftdw.dwFilterTag = paftda->dwFilterTag;

    aftei.paftda = paftda;
    aftei.dwInstance = dwInstance;
    aftei.fnCallback = fnCallback;

    return acmFilterTagEnumW(had, &aftdw, MSACM_FilterTagEnumCallbackWtoA,
			     (DWORD)&aftei, fdwEnum);
}

/***********************************************************************
 *           acmFilterTagEnumW (MSACM32.@)
 */
MMRESULT WINAPI acmFilterTagEnumW(HACMDRIVER had, PACMFILTERTAGDETAILSW paftd,
				  ACMFILTERTAGENUMCBW fnCallback, DWORD dwInstance,
				  DWORD fdwEnum)
{
    PWINE_ACMDRIVERID		padid;
    int				i;

    TRACE("(0x%08x, %p, %p, %ld, %ld)\n",
	  had, paftd, fnCallback, dwInstance, fdwEnum);

    if (paftd->cbStruct < sizeof(*paftd)) return MMSYSERR_INVALPARAM;

    if (had) FIXME("had != NULL, not supported\n");

    for (padid = MSACM_pFirstACMDriverID; padid; padid = padid->pNextACMDriverID) {
	/* should check for codec only */
	if (!(padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_DISABLED) &&
	    acmDriverOpen(&had, (HACMDRIVERID)padid, 0) == MMSYSERR_NOERROR) {

	    for (i = 0; i < padid->cFilterTags; i++) {
		paftd->dwFilterTagIndex = i;
		if (acmFilterTagDetailsW(had, paftd, ACM_FILTERTAGDETAILSF_INDEX) == MMSYSERR_NOERROR) {
		    if (!(fnCallback)((HACMDRIVERID)padid, paftd, dwInstance, padid->fdwSupport)) {
			padid = NULL;
			break;
		    }
		}
	    }
	}
	acmDriverClose(had, 0);
    }
    return MMSYSERR_NOERROR;
}
