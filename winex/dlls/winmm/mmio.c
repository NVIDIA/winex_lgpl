/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * MMIO functions
 *
 * Copyright 1998 Andrew Taylor
 * Copyright 1998 Ove K�ven
 * Copyright 2000 Eric Pouech
 */

/* Still to be done:
 * 	+ correct handling of global/local IOProcs
 *	+ mode of mmio objects is not used (read vs write vs readwrite)
 *	+ optimization of internal buffers (seg / lin)
 *	+ even in 32 bit only, a seg ptr IO buffer is allocated (after this is
 *	  fixed, we'll have a proper 32/16 separation)
 *	+ thread safeness
 *	+ rename operation is broken
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "mmsystem.h"
#include "windef.h"

#include "wine/mmsystem16.h"
#include "wine/winbase16.h"
#include "wine/heapstr.h"
#include "wine/file.h"
#include "winemm.h"

#include "wine/debug.h"
#include "wine/wine_byteswap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmio);

#if 0
static void dump_mmioinfo(const MMIOINFO* i)
{
    if (!i) return;
    MESSAGE("lpmmioinfo = {\n");
    MESSAGE("\tdwFlags     %08lx\n", i->dwFlags);
    MESSAGE("\tfccIOProc   %c%c%c%c\n",
            (char)i->fccIOProc,
            (char)(i->fccIOProc >> 8),
            (char)(i->fccIOProc >> 16),
            (char)(i->fccIOProc >> 24));
    MESSAGE("\tpIOProc     %p\n", i->pIOProc);
    MESSAGE("\twErrorRet   %08x\n", i->hTask);
    MESSAGE("\tcchBuffer   %08lx\n", i->cchBuffer);
    MESSAGE("\tpchBuffer   %p\n", i->pchBuffer);
    MESSAGE("\tpchNext     %p (diff: %08x)\n", i->pchNext, (char*)i->pchNext - (char*)i->pchBuffer);
    MESSAGE("\tpchEndRead  %p (diff: %08x)\n", i->pchEndRead, (char*)i->pchEndRead - (char*)i->pchNext);
    MESSAGE("\tpchEndWrite %p\n", i->pchEndWrite);
    MESSAGE("\tlBufOFfset  %li\n", i->lBufOffset);
    MESSAGE("\tlDiskOffset %li\n", i->lDiskOffset);
    MESSAGE("}\n");
}
#endif

/**************************************************************************
 *               	mmioDosIOProc           		[internal]
 */
static LRESULT CALLBACK mmioDosIOProc(LPMMIOINFO lpmmioinfo, UINT uMessage,
				      LPARAM lParam1, LPARAM lParam2)
{
    LRESULT	ret = MMSYSERR_NOERROR;

    TRACE("(%p, %X, 0x%lx, 0x%lx);\n", lpmmioinfo, uMessage, lParam1, lParam2);

    switch (uMessage) {
    case MMIOM_OPEN:
	{
	    /* Parameters:
	     * lParam1 = szFileName parameter from mmioOpen
	     * lParam2 = reserved (we use it for 16-bitness)
	     * Returns: zero on success, error code on error
	     * NOTE: lDiskOffset automatically set to zero
	     */
	    OFSTRUCT    ofs;
	    LPCSTR      szFileName = (LPCSTR)lParam1;

	    if (lpmmioinfo->dwFlags & MMIO_GETTEMP) {
		FIXME("MMIO_GETTEMP not implemented\n");
		return MMIOERR_CANNOTOPEN;
	    }

	    /* if filename NULL, assume open file handle in adwInfo[0] */
	    if (!szFileName) {
		if (lParam2)
		    lpmmioinfo->adwInfo[0] = DosFileHandleToWin32Handle(lpmmioinfo->adwInfo[0]);
		break;
	    }
	    lpmmioinfo->adwInfo[0] = (DWORD)OpenFile(szFileName, &ofs, lpmmioinfo->dwFlags & 0xFFFF);
	    if (lpmmioinfo->adwInfo[0] == (DWORD)HFILE_ERROR)
		ret = MMIOERR_CANNOTOPEN;
	}
	break;

    case MMIOM_CLOSE:
	/* Parameters:
	 * lParam1 = wFlags parameter from mmioClose
	 * lParam2 = unused
	 * Returns: zero on success, error code on error
	 */
	if (!(lParam1 & MMIO_FHOPEN))
	    _lclose((HFILE)lpmmioinfo->adwInfo[0]);
	break;

    case MMIOM_READ:
	/* Parameters:
	 * lParam1 = huge pointer to read buffer
	 * lParam2 = number of bytes to read
	 * Returns: number of bytes read, 0 for EOF, -1 for error (error code
	 *	   in wErrorRet)
	 */
	ret = _lread((HFILE)lpmmioinfo->adwInfo[0], (HPSTR)lParam1, (LONG)lParam2);
	if (ret != -1)
	    lpmmioinfo->lDiskOffset += ret;

	break;

    case MMIOM_WRITE:
    case MMIOM_WRITEFLUSH:
	/* no internal buffering, so WRITEFLUSH handled same as WRITE */

	/* Parameters:
	 * lParam1 = huge pointer to write buffer
	 * lParam2 = number of bytes to write
	 * Returns: number of bytes written, -1 for error (error code in
	 *		wErrorRet)
	 */
	ret = _hwrite((HFILE)lpmmioinfo->adwInfo[0], (HPSTR)lParam1, (LONG)lParam2);
	if (ret != -1)
	    lpmmioinfo->lDiskOffset += ret;
	break;

    case MMIOM_SEEK:
	/* Parameters:
	 * lParam1 = new position
	 * lParam2 = from whence to seek (SEEK_SET, SEEK_CUR, SEEK_END)
	 * Returns: new file postion, -1 on error
	 */
	ret = _llseek((HFILE)lpmmioinfo->adwInfo[0], (LONG)lParam1, (LONG)lParam2);
	if (ret != -1)
	    lpmmioinfo->lDiskOffset = ret;
	return ret;

    case MMIOM_RENAME:
	/* Parameters:
	 * lParam1 = old name
	 * lParam2 = new name
	 * Returns: zero on success, non-zero on failure
	 */
 	 if (!MoveFileA((const char*)lParam1, (const char*)lParam2))
	     ret = MMIOERR_FILENOTFOUND;
	 break;

    default:
	FIXME("unexpected message %u\n", uMessage);
	return 0;
    }

    return ret;
}

/**************************************************************************
 *               	mmioMemIOProc           		[internal]
 */
static LRESULT CALLBACK mmioMemIOProc(LPMMIOINFO lpmmioinfo, UINT uMessage,
				      LPARAM lParam1, LPARAM lParam2)
{
    TRACE("(%p,0x%04x,0x%08lx,0x%08lx)\n", lpmmioinfo, uMessage, lParam1, lParam2);

    switch (uMessage) {

    case MMIOM_OPEN:
	/* Parameters:
	 * lParam1 = filename (must be NULL)
	 * lParam2 = reserved (we use it for 16-bitness)
	 * Returns: zero on success, error code on error
	 * NOTE: lDiskOffset automatically set to zero
	 */
	/* FIXME: io proc shouldn't change it */
	if (!(lpmmioinfo->dwFlags & MMIO_CREATE))
	    lpmmioinfo->pchEndRead = lpmmioinfo->pchEndWrite;
        lpmmioinfo->adwInfo[0] = HFILE_ERROR;
	return 0;

    case MMIOM_CLOSE:
	/* Parameters:
	 * lParam1 = wFlags parameter from mmioClose
	 * lParam2 = unused
	 * Returns: zero on success, error code on error
	 */
	return 0;

    case MMIOM_READ:
	/* Parameters:
	 * lParam1 = huge pointer to read buffer
	 * lParam2 = number of bytes to read
	 * Returns: number of bytes read, 0 for EOF, -1 for error (error code
	 *	   in wErrorRet)
	 * NOTE: lDiskOffset should be updated
	 */
	FIXME("MMIOM_READ on memory files should not occur, buffer may be lost!\n");
	return 0;

    case MMIOM_WRITE:
    case MMIOM_WRITEFLUSH:
	/* no internal buffering, so WRITEFLUSH handled same as WRITE */

	/* Parameters:
	 * lParam1 = huge pointer to write buffer
	 * lParam2 = number of bytes to write
	 * Returns: number of bytes written, -1 for error (error code in
	 *		wErrorRet)
	 * NOTE: lDiskOffset should be updated
	 */
	FIXME("MMIOM_WRITE on memory files should not occur, buffer may be lost!\n");
	return 0;

    case MMIOM_SEEK:
	/* Parameters:
	 * lParam1 = new position
	 * lParam2 = from whence to seek (SEEK_SET, SEEK_CUR, SEEK_END)
	 * Returns: new file postion, -1 on error
	 * NOTE: lDiskOffset should be updated
	 */
	FIXME("MMIOM_SEEK on memory files should not occur, buffer may be lost!\n");
	return -1;

    default:
	FIXME("unexpected message %u\n", uMessage);
	return 0;
    }

    return 0;
}


enum mmioProcType {MMIO_PROC_16,MMIO_PROC_32A,MMIO_PROC_32W};

struct IOProcList
{
    struct IOProcList*pNext;       /* Next item in linked list */
    FOURCC            fourCC;      /* four-character code identifying IOProc */
    LPMMIOPROC	      pIOProc;     /* pointer to IProc */
    enum mmioProcType type;        /* 16, 32A or 32W */
    int		      count;	   /* number of objects linked to it */
};

/* This array will be the entire list for most apps */

static struct IOProcList defaultProcs[] = {
    {&defaultProcs[1], FOURCC_DOS, (LPMMIOPROC)mmioDosIOProc, MMIO_PROC_32A, 0},
    {NULL,             FOURCC_MEM, (LPMMIOPROC)mmioMemIOProc, MMIO_PROC_32A, 0},
};

static struct IOProcList*	pIOProcListAnchor = &defaultProcs[0];

/****************************************************************
 *       	MMIO_FindProcNode 			[INTERNAL]
 *
 * Finds the ProcList node associated with a given FOURCC code.
 */
static struct IOProcList*	MMIO_FindProcNode(FOURCC fccIOProc)
{
    struct IOProcList*	pListNode;

    for (pListNode = pIOProcListAnchor; pListNode; pListNode = pListNode->pNext) {
	if (pListNode->fourCC == fccIOProc) {
	    return pListNode;
	}
    }
    return NULL;
}

/****************************************************************
 *       	MMIO_InstallIOProc 			[INTERNAL]
 */
static LPMMIOPROC MMIO_InstallIOProc(FOURCC fccIOProc, LPMMIOPROC pIOProc,
				     DWORD dwFlags, enum mmioProcType type)
{
    LPMMIOPROC	        lpProc = NULL;
    struct IOProcList*  pListNode;
    struct IOProcList** ppListNode;

    TRACE("(%ld, %p, %08lX, %i)\n", fccIOProc, pIOProc, dwFlags, type);

    if (dwFlags & MMIO_GLOBALPROC)
	FIXME("Global procedures not implemented\n");

    /* just handle the known procedures for now */
    switch (dwFlags & (MMIO_INSTALLPROC|MMIO_REMOVEPROC|MMIO_FINDPROC)) {
    case MMIO_INSTALLPROC:
	/* Create new entry for the IOProc list */
	pListNode = HeapAlloc(GetProcessHeap(), 0, sizeof(*pListNode));
	if (pListNode) {
	    /* Fill in this node */
	    pListNode->fourCC = fccIOProc;
	    pListNode->pIOProc = pIOProc;
	    pListNode->type = type;
	    pListNode->count = 0;

	    /* Stick it on the end of the list */
	    pListNode->pNext = pIOProcListAnchor;
	    pIOProcListAnchor = pListNode;

	    /* Return this IOProc - that's how the caller knows we succeeded */
	    lpProc = pIOProc;
	}
	break;

    case MMIO_REMOVEPROC:
	/*
	 * Search for the node that we're trying to remove - note
	 * that this method won't find the first item on the list, but
	 * since the first two items on this list are ones we won't
	 * let the user delete anyway, that's okay
	 */
	ppListNode = &pIOProcListAnchor;
	while ((*ppListNode) && (*ppListNode)->fourCC != fccIOProc)
	    ppListNode = &((*ppListNode)->pNext);

	if (*ppListNode) { /* found it */
	    /* FIXME: what should be done if an open mmio object uses this proc ?
	     * shall we return an error, nuke the mmio object ?
	     */
	    if ((*ppListNode)->count) {
		ERR("Cannot remove a mmIOProc while in use\n");
		break;
	    }
	    /* remove it, but only if it isn't builtin */
	    if ((*ppListNode) >= defaultProcs &&
		(*ppListNode) < defaultProcs + sizeof(defaultProcs)) {
		WARN("Tried to remove built-in mmio proc. Skipping\n");
	    } else {
		/* Okay, nuke it */
		struct IOProcList*  ptmpNode = *ppListNode;
		lpProc = (*ppListNode)->pIOProc;
		*ppListNode = (*ppListNode)->pNext;
		HeapFree(GetProcessHeap(), 0, ptmpNode);
	    }
	}
	break;

    case MMIO_FINDPROC:
	if ((pListNode = MMIO_FindProcNode(fccIOProc))) {
	    lpProc = pListNode->pIOProc;
	}
	break;
    }

    return lpProc;
}

/****************************************************************
 *       		MMIO_Map32To16			[INTERNAL]
 */
static LRESULT	MMIO_Map32To16(DWORD wMsg, LPARAM* lp1, LPARAM* lp2)
{
    switch (wMsg) {
    case MMIOM_CLOSE:
    case MMIOM_SEEK:
	/* nothing to do */
	break;
    case MMIOM_OPEN:
    case MMIOM_READ:
    case MMIOM_WRITE:
    case MMIOM_WRITEFLUSH:
        *lp1 = MapLS( (void *)*lp1 );
	break;
    default:
	TRACE("Not a mappable message (%ld)\n", wMsg);
    }
    return MMSYSERR_NOERROR;
}

/****************************************************************
 *       	MMIO_UnMap32To16 			[INTERNAL]
 */
static LRESULT	MMIO_UnMap32To16(DWORD wMsg, LPARAM lParam1, LPARAM lParam2,
				 LPARAM lp1, LPARAM lp2)
{
    switch (wMsg) {
    case MMIOM_CLOSE:
    case MMIOM_SEEK:
	/* nothing to do */
	break;
    case MMIOM_OPEN:
    case MMIOM_READ:
    case MMIOM_WRITE:
    case MMIOM_WRITEFLUSH:
        UnMapLS( lp1 );
	break;
    default:
	TRACE("Not a mappable message (%ld)\n", wMsg);
    }
    return MMSYSERR_NOERROR;
}

/****************************************************************
 *       	MMIO_SendMessage			[INTERNAL]
 */
static LRESULT	MMIO_SendMessage(LPWINE_MMIO wm, DWORD wMsg, LPARAM lParam1,
				 LPARAM lParam2, enum mmioProcType type)
{
    MMIOINFO16 mmioInfo16;
    LRESULT 		result;
    SEGPTR		segmmioInfo16;
    LPARAM		lp1 = lParam1, lp2 = lParam2;

    if (!wm->ioProc || !wm->info.pIOProc) {
	ERR("brrr\n");
	result = MMSYSERR_INVALPARAM;
    }

    switch (wm->ioProc->type) {
    case MMIO_PROC_16:
        memset( &mmioInfo16, 0, sizeof(MMIOINFO16));
        mmioInfo16.lDiskOffset = wm->info.lDiskOffset;
        mmioInfo16.adwInfo[0]  = wm->info.adwInfo[0];
        mmioInfo16.adwInfo[1]  = wm->info.adwInfo[1];
        mmioInfo16.adwInfo[2]  = wm->info.adwInfo[2];
        mmioInfo16.adwInfo[3]  = wm->info.adwInfo[3];
	if (wm->ioProc->type != type) {
	    /* map (lParam1, lParam2) into (lp1, lp2) 32=>16 */
	    if ((result = MMIO_Map32To16(wMsg, &lp1, &lp2)) != MMSYSERR_NOERROR)
		return result;
	}
	/* FIXME: is wm->info.pIOProc a segmented or a linear address ?
	 * sounds to me it's a segmented one, should use a thunk somewhere
	 */
        segmmioInfo16 = MapLS( &mmioInfo16 );
        result = ((LPMMIOPROC16)wm->info.pIOProc)((LPSTR)segmmioInfo16, wMsg, lp1, lp2);
        UnMapLS( segmmioInfo16 );
	if (wm->ioProc->type != type) {
	    MMIO_UnMap32To16(wMsg, lParam1, lParam2, lp1, lp2);
	}
        wm->info.lDiskOffset = mmioInfo16.lDiskOffset;
        wm->info.adwInfo[0]  = mmioInfo16.adwInfo[0];
        wm->info.adwInfo[1]  = mmioInfo16.adwInfo[1];
        wm->info.adwInfo[2]  = mmioInfo16.adwInfo[2];
        wm->info.adwInfo[3]  = mmioInfo16.adwInfo[3];
	break;
    case MMIO_PROC_32A:
    case MMIO_PROC_32W:
	if (wm->ioProc->type != type) {
	    /* map (lParam1, lParam2) into (lp1, lp2) 16=>32 */
	    WARN("NIY\n");
	}
	result = (wm->info.pIOProc)((LPSTR)&wm->info, wMsg, lp1, lp2);

#if 0
	if (wm->ioProc->type != type) {
	    /* unmap (lParam1, lParam2) into (lp1, lp2) 16=>32 */
	}
#endif
	break;
    default:
	FIXME("Internal error\n");
	result = MMSYSERR_ERROR;
    }

    return result;
}

/**************************************************************************
 *      			MMIO_ParseExt 		        [internal]
 *
 * Parses a filename for the extension.
 *
 * RETURNS
 *  The FOURCC code for the extension if found, else 0.
 */
static FOURCC MMIO_ParseExt(LPCSTR szFileName)
{
    /* Filenames are of the form file.ext+ABC
       FIXME: What if a '+' is part of the file name?
       For now, we take the last '+' present */

    FOURCC ret = 0;

    /* Note that ext{Start,End} point to the . and + respectively */
    LPSTR extEnd;

    TRACE("(%s)\n",debugstr_a(szFileName));

    if (!szFileName)
	return ret;
    extEnd = strrchr(szFileName,'+');
    if (extEnd) {
	/* Need to parse to find the extension */
	LPSTR extStart;

	extStart = extEnd;
	while (extStart >= szFileName && extStart[0] != '.') {
	    extStart--;
	}

	if (extStart < szFileName) {
	    ERR("+ but no . in szFileName: %s\n", debugstr_a(szFileName));
	} else {
	    CHAR ext[5];

	    if (extEnd - extStart - 1 > 4)
		WARN("Extension length > 4\n");
	    lstrcpynA(ext,extStart + 1,min(extEnd-extStart,5));
	    TRACE("Got extension: %s\n", debugstr_a(ext));
	    /* FOURCC codes identifying file-extensions must be uppercase */
	    ret = mmioStringToFOURCCA(ext, MMIO_TOUPPER);
	}
    }
    return ret;
}

/**************************************************************************
 *				MMIO_Get			[internal]
 *
 * Retrieves the mmio object from current process
 */
static	LPWINE_MMIO	MMIO_Get(LPWINE_MM_IDATA iData, HMMIO h)
{
    LPWINE_MMIO		wm = NULL;

    if (!iData) iData = MULTIMEDIA_GetIData();

    EnterCriticalSection(&iData->cs);
    for (wm = iData->lpMMIO; wm; wm = wm->lpNext) {
	if (wm->info.hmmio == h)
	    break;
    }
    LeaveCriticalSection(&iData->cs);
    return wm;
}

/**************************************************************************
 *				MMIO_Create			[internal]
 *
 * Creates an internal representation for a mmio instance
 */
static	LPWINE_MMIO		MMIO_Create(void)
{
    static	WORD	MMIO_counter = 0;
    LPWINE_MMIO		wm;
    LPWINE_MM_IDATA	iData = MULTIMEDIA_GetIData();

    wm = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WINE_MMIO));
    if (wm) {
	EnterCriticalSection(&iData->cs);
	while (MMIO_Get(iData, HMMIO_32(++MMIO_counter)));
	wm->info.hmmio = HMMIO_32(MMIO_counter);
	wm->lpNext = iData->lpMMIO;
	iData->lpMMIO = wm;
	LeaveCriticalSection(&iData->cs);
    }
    return wm;
}

/**************************************************************************
 *				MMIO_Destroy			[internal]
 *-
 * Destroys an internal representation for a mmio instance
 */
static	BOOL		MMIO_Destroy(LPWINE_MMIO wm)
{
    LPWINE_MM_IDATA	iData = MULTIMEDIA_GetIData();
    LPWINE_MMIO*	m;

    EnterCriticalSection(&iData->cs);
    /* search for the matching one... */
    m = &(iData->lpMMIO);
    while (*m && *m != wm) m = &(*m)->lpNext;
    /* ...and destroy */
    if (*m) {
	*m = (*m)->lpNext;
	HeapFree(GetProcessHeap(), 0, wm);
	wm = NULL;
    }
    LeaveCriticalSection(&iData->cs);
    return wm ? FALSE : TRUE;
}

/****************************************************************
 *       		MMIO_Flush 			[INTERNAL]
 */
static	MMRESULT MMIO_Flush(WINE_MMIO* wm, UINT uFlags)
{
    if (wm->info.cchBuffer && (wm->info.fccIOProc != FOURCC_MEM)) {
	/* not quite sure what to do here, but I'll guess */
	if (wm->info.dwFlags & MMIO_DIRTY) {
            /* FIXME: error handling */
	    MMIO_SendMessage(wm, MMIOM_SEEK, wm->info.lBufOffset,
			     SEEK_SET, MMIO_PROC_32A);
	    MMIO_SendMessage(wm, MMIOM_WRITE, (LPARAM)wm->info.pchBuffer,
			     wm->info.pchNext - wm->info.pchBuffer, MMIO_PROC_32A);
	}
	if (uFlags & MMIO_EMPTYBUF)
	    wm->info.pchNext = wm->info.pchEndRead = wm->info.pchBuffer;
    }
    wm->info.dwFlags &= ~MMIO_DIRTY;

    return MMSYSERR_NOERROR;
}

/***************************************************************************
 *       		MMIO_GrabNextBuffer			[INTERNAL]
 */
static LONG	MMIO_GrabNextBuffer(LPWINE_MMIO wm, int for_read)
{
    LONG        still_to_read = (char*)wm->info.pchEndRead - (char*)wm->info.pchNext;
    LONG        size;

    if (still_to_read < 0)
    {
        /* basic testing on windows shows that windows doesn't seek,
         * and really just does this.
         * but perhaps more testing is needed. */
        WARN("buggy app? still_to_read < 0: %lx\n", still_to_read);
        still_to_read = 0;
    }

    size = wm->info.cchBuffer - still_to_read;

    TRACE("bo=%lx do=%lx of=%lx. size: 0x%lx, still_to_read: 0x%lx\n",
	  wm->info.lBufOffset, wm->info.lDiskOffset,
	  MMIO_SendMessage(wm, MMIOM_SEEK, 0, SEEK_CUR, MMIO_PROC_32A),
	  size, still_to_read);

    wm->info.lBufOffset = wm->info.lDiskOffset - still_to_read;
    wm->info.pchNext = wm->info.pchBuffer;
    wm->info.pchEndRead = wm->info.pchBuffer + still_to_read;
    wm->info.pchEndWrite = wm->info.pchBuffer + wm->info.cchBuffer;

    if (still_to_read)
        memmove(wm->info.pchBuffer, wm->info.pchNext, still_to_read);

    wm->bBufferLoaded = TRUE;
    if (for_read) {
	size = MMIO_SendMessage(wm, MMIOM_READ,
                                (LPARAM)((char*)wm->info.pchBuffer + still_to_read),
				size, MMIO_PROC_32A);
	if (size > 0)
	    wm->info.pchEndRead += size;
        else
        {
            wm->bBufferLoaded = FALSE;
            WARN("failed to read buffer (return from MMIOM_READ is %li)\n", size);
        }
    }

    return size;
}

/***************************************************************************
 *       		MMIO_SetBuffer 				[INTERNAL]
 */
static MMRESULT MMIO_SetBuffer(WINE_MMIO* wm, void* pchBuffer, LONG cchBuffer,
			       UINT uFlags, BOOL bFrom32)
{
    TRACE("(%p %p %ld %u %d)\n", wm, pchBuffer, cchBuffer, uFlags, bFrom32);

    if (uFlags)			return MMSYSERR_INVALPARAM;
    if (cchBuffer > 0xFFFF)
	WARN("Untested handling of huge mmio buffers (%ld >= 64k)\n", cchBuffer);

    if (MMIO_Flush(wm, 0) != MMSYSERR_NOERROR)
	return MMIOERR_CANNOTWRITE;

    if (wm->hMem && (wm->info.dwFlags & MMIO_ALLOCBUF)) {
	GlobalUnlock16(wm->hMem);
	GlobalFree16(wm->hMem);
	wm->hMem = 0;
	wm->info.dwFlags &= ~MMIO_ALLOCBUF;
    }

    if (pchBuffer) {
	if (bFrom32) {
	    wm->info.pchBuffer = pchBuffer;
	    wm->buffer16 = 0;
	} else {
	    wm->info.pchBuffer = MapSL((SEGPTR)pchBuffer);
	    wm->buffer16 = (SEGPTR)pchBuffer;
	}
	wm->hMem = 0;
    } else if (cchBuffer) {
	if (!(wm->hMem = GlobalAlloc16(GMEM_MOVEABLE, cchBuffer)))
	    return MMIOERR_OUTOFMEMORY;
	wm->info.dwFlags |= MMIO_ALLOCBUF;
    } else {
	wm->info.pchBuffer = NULL;
	wm->hMem = 0;
	wm->buffer16 = 0;
    }

    if (wm->hMem) {
	wm->buffer16 = K32WOWGlobalLock16(wm->hMem);
	wm->info.pchBuffer = MapSL(wm->buffer16);
    }

    wm->info.cchBuffer = cchBuffer;
    wm->info.pchNext = wm->info.pchBuffer;
    wm->info.pchEndRead = wm->info.pchBuffer;
    wm->info.pchEndWrite = wm->info.pchBuffer + cchBuffer;
    wm->info.lBufOffset = wm->info.lDiskOffset;
    wm->bBufferLoaded = FALSE;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 			MMIO_Open      				[internal]
 */
static HMMIO MMIO_Open(LPSTR szFileName, MMIOINFO* refmminfo,
		       DWORD dwOpenFlags, enum mmioProcType type)
{
    LPWINE_MMIO		wm;

    TRACE("('%s', %p, %08lX, %d);\n", szFileName, refmminfo, dwOpenFlags, type);

    if (dwOpenFlags & (MMIO_PARSE|MMIO_EXIST)) {
	char	buffer[MAX_PATH];

	if (GetFullPathNameA(szFileName, sizeof(buffer), buffer, NULL) >= sizeof(buffer))
	    return (HMMIO16)FALSE;
	if ((dwOpenFlags & MMIO_EXIST) && (GetFileAttributesA(buffer) == -1))
	    return (HMMIO)FALSE;
	strcpy(szFileName, buffer);
	return (HMMIO)TRUE;
    }

    if ((wm = MMIO_Create()) == NULL)
	return 0;

    /* If both params are NULL, then parse the file name if available */
    if (refmminfo->fccIOProc == 0 && refmminfo->pIOProc == NULL) {
	wm->info.fccIOProc = MMIO_ParseExt(szFileName);
	/* Handle any unhandled/error case. Assume DOS file */
	if (wm->info.fccIOProc == 0)
	    wm->info.fccIOProc = FOURCC_DOS;
	if (!(wm->ioProc = MMIO_FindProcNode(wm->info.fccIOProc))) goto error2;
	wm->info.pIOProc = wm->ioProc->pIOProc;
	wm->bTmpIOProc = FALSE;
    }
    /* if just the four character code is present, look up IO proc */
    else if (refmminfo->pIOProc == NULL) {
	wm->info.fccIOProc = refmminfo->fccIOProc;
	if (!(wm->ioProc = MMIO_FindProcNode(wm->info.fccIOProc))) goto error2;
	wm->info.pIOProc = wm->ioProc->pIOProc;
	wm->bTmpIOProc = FALSE;
    }
    /* if IO proc specified, use it and specified four character code */
    else {
	wm->info.fccIOProc = refmminfo->fccIOProc;
	wm->info.pIOProc = refmminfo->pIOProc;
	MMIO_InstallIOProc(wm->info.fccIOProc, wm->info.pIOProc,
			   MMIO_INSTALLPROC, type);
	if (!(wm->ioProc = MMIO_FindProcNode(wm->info.fccIOProc))) goto error2;
	assert(wm->ioProc->pIOProc == refmminfo->pIOProc);
	wm->info.pIOProc = wm->ioProc->pIOProc;
	wm->bTmpIOProc = TRUE;
    }

    wm->bBufferLoaded = FALSE;
    wm->ioProc->count++;

    if (dwOpenFlags & MMIO_ALLOCBUF) {
	if ((refmminfo->wErrorRet = MMIO_SetBuffer(wm, NULL, MMIO_DEFAULTBUFFER, 0,
                                                   type != MMIO_PROC_16)))
	    goto error1;
    } else if (wm->info.fccIOProc == FOURCC_MEM) {
	refmminfo->wErrorRet = MMIO_SetBuffer(wm, refmminfo->pchBuffer,
					      refmminfo->cchBuffer, 0,
					      type != MMIO_PROC_16);
	if (refmminfo->wErrorRet != MMSYSERR_NOERROR)
	    goto error1;
	wm->bBufferLoaded = TRUE;
    } /* else => unbuffered, wm->info.pchBuffer == NULL */

    /* see mmioDosIOProc for that one */
    wm->info.adwInfo[0] = refmminfo->adwInfo[0];
    wm->info.dwFlags = dwOpenFlags;

    /* call IO proc to actually open file */
    refmminfo->wErrorRet = MMIO_SendMessage(wm, MMIOM_OPEN, (LPARAM)szFileName,
                                            type == MMIO_PROC_16, MMIO_PROC_32A);

    /* grab file size, when possible */
    wm->dwFileSize = GetFileSize(wm->info.adwInfo[0], NULL);

    if (refmminfo->wErrorRet == 0)
	return wm->info.hmmio;
 error1:
    if (wm->ioProc) wm->ioProc->count--;
 error2:
    MMIO_Destroy(wm);
    return 0;
}

/**************************************************************************
 * 				mmioOpenW       		[WINMM.@]
 */
HMMIO WINAPI mmioOpenW(LPWSTR szFileName, MMIOINFO* lpmmioinfo,
		       DWORD dwOpenFlags)
{
    HMMIO 	ret;
    LPSTR	szFn = FILE_strdupWtoA(GetProcessHeap(), 0, szFileName);

    if (lpmmioinfo) {
	ret = MMIO_Open(szFn, lpmmioinfo, dwOpenFlags, MMIO_PROC_32W);
    } else {
	MMIOINFO	mmioinfo;

	mmioinfo.fccIOProc = 0;
	mmioinfo.pIOProc = NULL;
	mmioinfo.pchBuffer = NULL;
	mmioinfo.cchBuffer = 0;

	ret = MMIO_Open(szFn, &mmioinfo, dwOpenFlags, MMIO_PROC_32W);
    }

    HeapFree(GetProcessHeap(), 0, szFn);
    return ret;
}

/**************************************************************************
 * 				mmioOpenA       		[WINMM.@]
 */
HMMIO WINAPI mmioOpenA(LPSTR szFileName, MMIOINFO* lpmmioinfo,
		       DWORD dwOpenFlags)
{
    HMMIO 	ret;

    if (lpmmioinfo) {
	ret = MMIO_Open(szFileName, lpmmioinfo, dwOpenFlags, MMIO_PROC_32A);
    } else {
	MMIOINFO	mmioinfo;

	mmioinfo.fccIOProc = 0;
	mmioinfo.pIOProc = NULL;
	mmioinfo.pchBuffer = NULL;
	mmioinfo.cchBuffer = 0;

	ret = MMIO_Open(szFileName, &mmioinfo, dwOpenFlags, MMIO_PROC_32A);
    }
    return ret;
}

/**************************************************************************
 * 				mmioOpen       		[MMSYSTEM.1210]
 */
HMMIO16 WINAPI mmioOpen16(LPSTR szFileName, MMIOINFO16* lpmmioinfo16,
			  DWORD dwOpenFlags)
{
    HMMIO 	ret;
    MMIOINFO	mmio;

    if (lpmmioinfo16) {
	MMIOINFO	mmioinfo;

	memset(&mmioinfo, 0, sizeof(mmioinfo));

	mmioinfo.dwFlags     = lpmmioinfo16->dwFlags;
	mmioinfo.fccIOProc   = lpmmioinfo16->fccIOProc;
	mmioinfo.pIOProc     = (LPMMIOPROC)lpmmioinfo16->pIOProc;
	mmioinfo.cchBuffer   = lpmmioinfo16->cchBuffer;
	mmioinfo.pchBuffer   = lpmmioinfo16->pchBuffer;
	mmioinfo.adwInfo[0]  = lpmmioinfo16->adwInfo[0];
	mmioinfo.adwInfo[1]  = lpmmioinfo16->adwInfo[1];
	mmioinfo.adwInfo[2]  = lpmmioinfo16->adwInfo[2];
	mmioinfo.adwInfo[3]  = lpmmioinfo16->adwInfo[3];

	ret = MMIO_Open(szFileName, &mmioinfo, dwOpenFlags, MMIO_PROC_16);

	mmioGetInfo16(HMMIO_16(mmioinfo.hmmio), lpmmioinfo16, 0);
	lpmmioinfo16->wErrorRet = HMMIO_16(ret);
    } else {
	mmio.fccIOProc = 0;
	mmio.pIOProc = NULL;
	mmio.pchBuffer = NULL;
	mmio.cchBuffer = 0;
	ret = MMIO_Open(szFileName, &mmio, dwOpenFlags, FALSE);
    }
    return HMMIO_16(ret);
}


/**************************************************************************
 * 				mmioClose      		[WINMM.@]
 */
MMRESULT WINAPI mmioClose(HMMIO hmmio, UINT uFlags)
{
    LPWINE_MMIO	wm;
    MMRESULT 	result;

    TRACE("(%04X, %04X);\n", hmmio, uFlags);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    if ((result = MMIO_Flush(wm, 0)) != MMSYSERR_NOERROR)
	return result;

    result = MMIO_SendMessage(wm, MMIOM_CLOSE, uFlags, 0, MMIO_PROC_32A);

    MMIO_SetBuffer(wm, NULL, 0, 0, TRUE);

    wm->ioProc->count--;

    if (wm->bTmpIOProc)
	MMIO_InstallIOProc(wm->info.fccIOProc, NULL,
			   MMIO_REMOVEPROC, wm->ioProc->type);

    MMIO_Destroy(wm);

    return result;
}

/**************************************************************************
 * 				mmioClose      		[MMSYSTEM.1211]
 */
MMRESULT16 WINAPI mmioClose16(HMMIO16 hmmio, UINT16 uFlags)
{
    return mmioClose(HMMIO_32(hmmio), uFlags);
}

/**************************************************************************
 * 				mmioRead	       	[WINMM.@]
 */
LONG WINAPI mmioRead(HMMIO hmmio, HPSTR pch, LONG cch)
{
    LPWINE_MMIO	wm;
    LONG 	count;

    TRACE("(%04X, %p, %ld);\n", hmmio, pch, cch);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return -1;

    /* unbuffered case first */
    if (!wm->info.pchBuffer)
	return MMIO_SendMessage(wm, MMIOM_READ, (LPARAM)pch, cch, MMIO_PROC_32A);

    /* first try from current buffer */
    if (wm->info.pchNext != wm->info.pchEndRead) {
	count = wm->info.pchEndRead - wm->info.pchNext;
	if (count > cch || count < 0) count = cch;
	memcpy(pch, wm->info.pchNext, count);
	wm->info.pchNext += count;
	pch += count;
	cch -= count;
    } else
	count = 0;

    if (cch && (wm->info.fccIOProc != FOURCC_MEM)) {
	assert(wm->info.cchBuffer);

	while (cch) {
	    LONG size;

	    size = MMIO_GrabNextBuffer(wm, TRUE);
	    if (size <= 0) break;
	    if (size > cch) size = cch;
	    memcpy(pch, wm->info.pchBuffer, size);
	    wm->info.pchNext += size;
	    pch += size;
	    cch -= size;
	    count += size;
	}
    }

    TRACE("count=%ld\n", count);
    return count;
}

/**************************************************************************
 * 				mmioRead	       	[MMSYSTEM.1212]
 */
LONG WINAPI mmioRead16(HMMIO16 hmmio, HPSTR pch, LONG cch)
{
    return mmioRead(HMMIO_32(hmmio), pch, cch);
}

/**************************************************************************
 * 				mmioWrite      		[WINMM.@]
 */
LONG WINAPI mmioWrite(HMMIO hmmio, HPCSTR pch, LONG cch)
{
    LPWINE_MMIO	wm;
    LONG	count;

    TRACE("(%04X, %p, %ld);\n", hmmio, pch, cch);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return -1;

    if (wm->info.cchBuffer) {
	LONG	bytesW = 0;

        count = 0;
        while (cch) {
            if (wm->info.pchNext != wm->info.pchEndWrite) {
                count = wm->info.pchEndWrite - wm->info.pchNext;
                if (count > cch || count < 0) count = cch;
                memcpy(wm->info.pchNext, pch, count);
                wm->info.pchNext += count;
                pch += count;
                cch -= count;
                bytesW += count;
                wm->info.dwFlags |= MMIO_DIRTY;
	    } else {
                if (wm->info.fccIOProc == FOURCC_MEM) {
                    if (wm->info.adwInfo[0]) {
                        /* from where would we get the memory handle? */
                        FIXME("memory file expansion not implemented!\n");
                        break;
		    } else break;
                }
            }

            if (wm->info.pchNext == wm->info.pchEndWrite)
            {
                MMIO_Flush(wm, MMIO_EMPTYBUF);
                MMIO_GrabNextBuffer(wm, FALSE);
            }
            else break;
        }
	count = bytesW;
    } else {
	count = MMIO_SendMessage(wm, MMIOM_WRITE, (LPARAM)pch, cch, MMIO_PROC_32A);
	wm->info.lBufOffset = wm->info.lDiskOffset;
    }

    TRACE("bytes written=%ld\n", count);
    return count;
}

/**************************************************************************
 * 				mmioWrite      		[MMSYSTEM.1213]
 */
LONG WINAPI mmioWrite16(HMMIO16 hmmio, HPCSTR pch, LONG cch)
{
    return mmioWrite(HMMIO_32(hmmio),pch,cch);
}

/**************************************************************************
 * 				mmioSeek		[WINMM.@]
 */
LONG WINAPI mmioSeek(HMMIO hmmio, LONG lOffset, INT iOrigin)
{
    LPWINE_MMIO	wm;
    LONG 	offset;

    TRACE("(%04X, %08lX, %d);\n", hmmio, lOffset, iOrigin);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    /* not buffered, direct seek on file */
    if (!wm->info.pchBuffer)
	return MMIO_SendMessage(wm, MMIOM_SEEK, lOffset, iOrigin, MMIO_PROC_32A);

    switch (iOrigin) {
    case SEEK_SET:
	offset = lOffset;
	break;
    case SEEK_CUR:
	offset = wm->info.lBufOffset + (wm->info.pchNext - wm->info.pchBuffer) + lOffset;
	break;
    case SEEK_END:
	offset = ((wm->info.fccIOProc == FOURCC_MEM)? wm->info.cchBuffer : wm->dwFileSize) - lOffset;
	break;
    default:
	return -1;
    }

    if (offset && offset >= wm->dwFileSize && wm->info.fccIOProc != FOURCC_MEM) {
        /* should check that write mode exists */
        if (MMIO_Flush(wm, 0) != MMSYSERR_NOERROR)
            return -1;
        wm->info.lBufOffset = offset;
        wm->info.pchEndRead = wm->info.pchBuffer;
        wm->info.pchEndWrite = wm->info.pchBuffer + wm->info.cchBuffer;
        if ((wm->info.dwFlags & MMIO_RWMODE) == MMIO_READ) {
            wm->info.lDiskOffset = wm->dwFileSize;
        }
    } else if ((wm->info.cchBuffer > 0) &&
	((offset < wm->info.lBufOffset) ||
	 (offset >= wm->info.lBufOffset + wm->info.cchBuffer) ||
	 !wm->bBufferLoaded)) {
        /* stay in same buffer ? */
        /* some memory mapped buffers are defined with -1 as a size */

	/* condition to change buffer */
	if ((wm->info.fccIOProc == FOURCC_MEM) ||
	    MMIO_Flush(wm, 0) != MMSYSERR_NOERROR ||
	    /* this also sets the wm->info.lDiskOffset field */
	    MMIO_SendMessage(wm, MMIOM_SEEK,
			     (offset / wm->info.cchBuffer) * wm->info.cchBuffer,
			     SEEK_SET, MMIO_PROC_32A) == -1)
	    return -1;
	MMIO_GrabNextBuffer(wm, TRUE);
    }

    wm->info.pchNext = wm->info.pchBuffer + (offset - wm->info.lBufOffset);

    TRACE("=> %ld\n", offset);
    return offset;
}

/**************************************************************************
 * 				mmioSeek       		[MMSYSTEM.1214]
 */
LONG WINAPI mmioSeek16(HMMIO16 hmmio, LONG lOffset, INT16 iOrigin)
{
    return mmioSeek(HMMIO_32(hmmio), lOffset, iOrigin);
}

/**************************************************************************
 * 				mmioGetInfo	       	[MMSYSTEM.1215]
 */
MMRESULT16 WINAPI mmioGetInfo16(HMMIO16 hmmio, MMIOINFO16* lpmmioinfo, UINT16 uFlags)
{
    LPWINE_MMIO	wm;

    TRACE("(0x%04x,%p,0x%08x)\n",hmmio,lpmmioinfo,uFlags);

    if ((wm = MMIO_Get(NULL, HMMIO_32(hmmio))) == NULL)
	return MMSYSERR_INVALHANDLE;

    if (!wm->buffer16)
	return MMSYSERR_ERROR;

    lpmmioinfo->dwFlags     = wm->info.dwFlags;
    lpmmioinfo->fccIOProc   = wm->info.fccIOProc;
    lpmmioinfo->pIOProc     = (LPMMIOPROC16)wm->info.pIOProc;
    lpmmioinfo->wErrorRet   = wm->info.wErrorRet;
    lpmmioinfo->hTask       = wm->info.hTask;
    lpmmioinfo->cchBuffer   = wm->info.cchBuffer;
    lpmmioinfo->pchBuffer   = (void*)wm->buffer16;
    lpmmioinfo->pchNext     = (void*)(wm->buffer16 + (wm->info.pchNext - wm->info.pchBuffer));
    lpmmioinfo->pchEndRead  = (void*)(wm->buffer16 + (wm->info.pchEndRead - wm->info.pchBuffer));
    lpmmioinfo->pchEndWrite = (void*)(wm->buffer16 + (wm->info.pchEndWrite - wm->info.pchBuffer));
    lpmmioinfo->lBufOffset  = wm->info.lBufOffset;
    lpmmioinfo->lDiskOffset = wm->info.lDiskOffset;
    lpmmioinfo->adwInfo[0]  = wm->info.adwInfo[0];
    lpmmioinfo->adwInfo[1]  = wm->info.adwInfo[1];
    lpmmioinfo->adwInfo[2]  = wm->info.adwInfo[2];
    lpmmioinfo->adwInfo[3]  = wm->info.adwInfo[3];
    lpmmioinfo->dwReserved1 = 0;
    lpmmioinfo->dwReserved2 = 0;
    lpmmioinfo->hmmio = HMMIO_16(wm->info.hmmio);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioGetInfo	       	[WINMM.@]
 */
MMRESULT WINAPI mmioGetInfo(HMMIO hmmio, MMIOINFO* lpmmioinfo, UINT uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("(0x%04x,%p,0x%08x)\n",hmmio,lpmmioinfo,uFlags);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    memcpy(lpmmioinfo, &wm->info, sizeof(MMIOINFO));

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioSetInfo  		[MMSYSTEM.1216]
 */
MMRESULT16 WINAPI mmioSetInfo16(HMMIO16 hmmio, const MMIOINFO16* lpmmioinfo, UINT16 uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("(0x%04x,%p,0x%08x)\n",hmmio,lpmmioinfo,uFlags);

    if ((wm = MMIO_Get(NULL, HMMIO_32(hmmio))) == NULL)
	return MMSYSERR_INVALHANDLE;

    /* check if seg and lin buffers are the same */
    if (wm->info.cchBuffer != lpmmioinfo->cchBuffer ||
	wm->info.pchBuffer != MapSL(wm->buffer16))
	return MMSYSERR_INVALPARAM;

    /* check pointers coherence */
    if (lpmmioinfo->pchNext < lpmmioinfo->pchBuffer ||
	lpmmioinfo->pchNext > lpmmioinfo->pchBuffer + lpmmioinfo->cchBuffer ||
	lpmmioinfo->pchEndRead < lpmmioinfo->pchBuffer ||
	lpmmioinfo->pchEndRead > lpmmioinfo->pchBuffer + lpmmioinfo->cchBuffer ||
	lpmmioinfo->pchEndWrite < lpmmioinfo->pchBuffer ||
	lpmmioinfo->pchEndWrite > lpmmioinfo->pchBuffer + lpmmioinfo->cchBuffer)
	return MMSYSERR_INVALPARAM;

    wm->info.pchNext     = wm->info.pchBuffer + (lpmmioinfo->pchNext     - lpmmioinfo->pchBuffer);
    wm->info.pchEndRead  = wm->info.pchBuffer + (lpmmioinfo->pchEndRead  - lpmmioinfo->pchBuffer);
    wm->info.pchEndWrite = wm->info.pchBuffer + (lpmmioinfo->pchEndWrite - lpmmioinfo->pchBuffer);
    if (lpmmioinfo->dwFlags & MMIO_DIRTY)
        wm->info.dwFlags |= MMIO_DIRTY;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioSetInfo    		[WINMM.@]
 */
MMRESULT WINAPI mmioSetInfo(HMMIO hmmio, const MMIOINFO* lpmmioinfo, UINT uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("(0x%04x,%p,0x%08x)\n",hmmio,lpmmioinfo,uFlags);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    /* check pointers coherence */
    if (lpmmioinfo->pchNext < wm->info.pchBuffer ||
	lpmmioinfo->pchNext > wm->info.pchBuffer + wm->info.cchBuffer ||
	lpmmioinfo->pchEndRead < wm->info.pchBuffer ||
	lpmmioinfo->pchEndRead > wm->info.pchBuffer + wm->info.cchBuffer ||
	lpmmioinfo->pchEndWrite < wm->info.pchBuffer ||
	lpmmioinfo->pchEndWrite > wm->info.pchBuffer + wm->info.cchBuffer)
	return MMSYSERR_INVALPARAM;

    wm->info.pchNext = lpmmioinfo->pchNext;
    wm->info.pchEndRead = lpmmioinfo->pchEndRead;
    if (lpmmioinfo->dwFlags & MMIO_DIRTY)
        wm->info.dwFlags |= MMIO_DIRTY;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
* 				mmioSetBuffer		[WINMM.@]
*/
MMRESULT WINAPI mmioSetBuffer(HMMIO hmmio, LPSTR pchBuffer, LONG cchBuffer, UINT uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("(hmmio=%04x, pchBuf=%p, cchBuf=%ld, uFlags=%#08x)\n",
	  hmmio, pchBuffer, cchBuffer, uFlags);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    return MMIO_SetBuffer(wm, pchBuffer, cchBuffer, uFlags, TRUE);
}

/**************************************************************************
 * 				mmioSetBuffer		[MMSYSTEM.1217]
 */
MMRESULT16 WINAPI mmioSetBuffer16(HMMIO16 hmmio, LPSTR segpchBuffer,
                                  LONG cchBuffer, UINT16 uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("(hmmio=%04x, segpchBuf=%p, cchBuf=%ld, uFlags=%#08x)\n",
	  hmmio, segpchBuffer, cchBuffer, uFlags);

    if ((wm = MMIO_Get(NULL, HMMIO_32(hmmio))) == NULL)
	return MMSYSERR_INVALHANDLE;

    return MMIO_SetBuffer(wm, segpchBuffer, cchBuffer, uFlags, FALSE);
}

/**************************************************************************
 * 				mmioFlush      		[WINMM.@]
 */
MMRESULT WINAPI mmioFlush(HMMIO hmmio, UINT uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("(%04X, %04X)\n", hmmio, uFlags);

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    return MMIO_Flush(wm, uFlags);
}

/**************************************************************************
 * 				mmioFlush      		[MMSYSTEM.1218]
 */
MMRESULT16 WINAPI mmioFlush16(HMMIO16 hmmio, UINT16 uFlags)
{
    return mmioFlush(HMMIO_32(hmmio), uFlags);
}

/**************************************************************************
 * 				mmioAdvance      	[WINMM.@]
 */
MMRESULT WINAPI mmioAdvance(HMMIO hmmio, MMIOINFO* lpmmioinfo, UINT uFlags)
{
    LPWINE_MMIO		wm;
    MMRESULT ret;

    TRACE("hmmio=%04X, lpmmioinfo=%p, uFlags=%04X\n", hmmio, lpmmioinfo, uFlags);

    if (lpmmioinfo && (ret = mmioSetInfo(hmmio, lpmmioinfo, 0)) != MMSYSERR_NOERROR)
        return ret;

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    if (!wm->info.cchBuffer)
	return MMIOERR_UNBUFFERED;

    if (uFlags != MMIO_READ && uFlags != MMIO_WRITE)
	return MMSYSERR_INVALPARAM;

    if (uFlags == MMIO_WRITE && (lpmmioinfo->dwFlags & MMIO_DIRTY))
    {
        MMIO_SendMessage(wm, MMIOM_SEEK, lpmmioinfo->lBufOffset, SEEK_SET, MMIO_PROC_32A);
        MMIO_SendMessage(wm, MMIOM_WRITE, (LPARAM)lpmmioinfo->pchBuffer,
                         lpmmioinfo->pchNext - lpmmioinfo->pchBuffer, MMIO_PROC_32A);
        lpmmioinfo->dwFlags &= ~MMIO_DIRTY;
    }
    if (MMIO_Flush(wm, 0) != MMSYSERR_NOERROR)
	return MMIOERR_CANNOTWRITE;
    wm->dwFileSize = max(wm->dwFileSize, lpmmioinfo->lBufOffset + (lpmmioinfo->pchNext - lpmmioinfo->pchBuffer));

    MMIO_GrabNextBuffer(wm, uFlags == MMIO_READ);

    lpmmioinfo->pchNext = lpmmioinfo->pchBuffer;
    lpmmioinfo->pchEndRead  = lpmmioinfo->pchBuffer +
	(wm->info.pchEndRead - wm->info.pchBuffer);
    lpmmioinfo->pchEndWrite = lpmmioinfo->pchBuffer +
	(wm->info.pchEndWrite - wm->info.pchBuffer);
    lpmmioinfo->lDiskOffset = wm->info.lDiskOffset;
    lpmmioinfo->lBufOffset = wm->info.lBufOffset;

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 * 				mmioAdvance    		[MMSYSTEM.1219]
 */
MMRESULT16 WINAPI mmioAdvance16(HMMIO16 hmmio, MMIOINFO16* lpmmioinfo, UINT16 uFlags)
{
    LPWINE_MMIO		wm;

    TRACE("hmmio=%04X, lpmmioinfo=%p, uFlags=%04X\n", hmmio, lpmmioinfo, uFlags);

    if ((wm = MMIO_Get(NULL, HMMIO_32(hmmio))) == NULL)
	return MMSYSERR_INVALHANDLE;

    if (!wm->info.cchBuffer)
	return MMIOERR_UNBUFFERED;

    if (uFlags != MMIO_READ && uFlags != MMIO_WRITE)
	return MMSYSERR_INVALPARAM;

    if (MMIO_Flush(wm, 0) != MMSYSERR_NOERROR)
	return MMIOERR_CANNOTWRITE;

    MMIO_GrabNextBuffer(wm, uFlags == MMIO_READ);

    lpmmioinfo->pchNext = lpmmioinfo->pchBuffer;
    lpmmioinfo->pchEndRead  = lpmmioinfo->pchBuffer +
	(wm->info.pchEndRead - wm->info.pchBuffer);
    lpmmioinfo->pchEndWrite = lpmmioinfo->pchBuffer +
	(wm->info.pchEndWrite - wm->info.pchBuffer);
    lpmmioinfo->lDiskOffset = wm->info.lDiskOffset;
    lpmmioinfo->lBufOffset = wm->info.lBufOffset;

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioStringToFOURCCA	[WINMM.@]
 */
FOURCC WINAPI mmioStringToFOURCCA(LPCSTR sz, UINT uFlags)
{
    CHAR cc[4];
    int i = 0;

    for (i = 0; i < 4 && sz[i]; i++) {
	if (uFlags & MMIO_TOUPPER) {
	    cc[i] = toupper(sz[i]);
	} else {
	    cc[i] = sz[i];
	}
    }

    /* Pad with spaces */
    while (i < 4) cc[i++] = ' ';

    TRACE("Got '%.4s'\n",cc);
    return mmioFOURCC(cc[0],cc[1],cc[2],cc[3]);
}

/**************************************************************************
 * 				mmioStringToFOURCCW	[WINMM.@]
 */
FOURCC WINAPI mmioStringToFOURCCW(LPCWSTR sz, UINT uFlags)
{
    LPSTR	szA = HEAP_strdupWtoA(GetProcessHeap(),0,sz);
    FOURCC	ret = mmioStringToFOURCCA(szA,uFlags);

    HeapFree(GetProcessHeap(), 0, szA);
    return ret;
}

/**************************************************************************
 * 				mmioStringToFOURCC	[MMSYSTEM.1220]
 */
FOURCC WINAPI mmioStringToFOURCC16(LPCSTR sz, UINT16 uFlags)
{
    return mmioStringToFOURCCA(sz, uFlags);
}

/**************************************************************************
 *              mmioInstallIOProc    [MMSYSTEM.1221]
 */
LPMMIOPROC16 WINAPI mmioInstallIOProc16(FOURCC fccIOProc, LPMMIOPROC16 pIOProc,
                                        DWORD dwFlags)
{
    return (LPMMIOPROC16)MMIO_InstallIOProc(fccIOProc, (LPMMIOPROC)pIOProc,
					    dwFlags, MMIO_PROC_16);
}

/**************************************************************************
 * 				mmioInstallIOProcA	   [WINMM.@]
 */
LPMMIOPROC WINAPI mmioInstallIOProcA(FOURCC fccIOProc,
				     LPMMIOPROC pIOProc, DWORD dwFlags)
{
    return MMIO_InstallIOProc(fccIOProc, pIOProc, dwFlags, MMIO_PROC_32A);
}

/**************************************************************************
 * 				mmioInstallIOProcW	   [WINMM.@]
 */
LPMMIOPROC WINAPI mmioInstallIOProcW(FOURCC fccIOProc,
				     LPMMIOPROC pIOProc, DWORD dwFlags)
{
    return MMIO_InstallIOProc(fccIOProc, pIOProc, dwFlags, MMIO_PROC_32W);
}

/**************************************************************************
 * 				mmioSendMessage	[MMSYSTEM.1222]
 */
LRESULT WINAPI mmioSendMessage16(HMMIO16 hmmio, UINT16 uMessage,
				 LPARAM lParam1, LPARAM lParam2)
{
    LPWINE_MMIO		wm;

    TRACE("(%04X, %u, %ld, %ld)\n", hmmio, uMessage, lParam1, lParam2);

    if (uMessage < MMIOM_USER)
	return MMSYSERR_INVALPARAM;

    if ((wm = MMIO_Get(NULL, HMMIO_32(hmmio))) == NULL)
	return MMSYSERR_INVALHANDLE;

    return MMIO_SendMessage(wm, uMessage, lParam1, lParam2, MMIO_PROC_16);
}

/**************************************************************************
 * 				mmioSendMessage		[WINMM.@]
 */
LRESULT WINAPI mmioSendMessage(HMMIO hmmio, UINT uMessage,
			       LPARAM lParam1, LPARAM lParam2)
{
    LPWINE_MMIO		wm;

    TRACE("(%04X, %u, %ld, %ld)\n", hmmio, uMessage, lParam1, lParam2);

    if (uMessage < MMIOM_USER)
	return MMSYSERR_INVALPARAM;

    if ((wm = MMIO_Get(NULL, hmmio)) == NULL)
	return MMSYSERR_INVALHANDLE;

    return MMIO_SendMessage(wm, uMessage, lParam1, lParam2, MMIO_PROC_32A);
}

/**************************************************************************
 * 				mmioDescend         	[WINMM.@]
 */
MMRESULT WINAPI mmioDescend(HMMIO hmmio, LPMMCKINFO lpck,
                            const MMCKINFO* lpckParent, UINT uFlags)
{
    DWORD		dwOldPos;
    FOURCC		srchCkId;
    FOURCC		srchType;


    TRACE("(%04X, %p, %p, %04X);\n", hmmio, lpck, lpckParent, uFlags);

    if (lpck == NULL)
	return MMSYSERR_INVALPARAM;

    dwOldPos = mmioSeek(hmmio, 0, SEEK_CUR);
    TRACE("dwOldPos=%ld\n", dwOldPos);

    if (lpckParent != NULL) {
	TRACE("seek inside parent at %ld !\n", lpckParent->dwDataOffset);
	/* EPP: was dwOldPos = mmioSeek(hmmio,lpckParent->dwDataOffset,SEEK_SET); */
	if (dwOldPos < lpckParent->dwDataOffset ||
	    dwOldPos >= lpckParent->dwDataOffset + lpckParent->cksize) {
	    WARN("outside parent chunk\n");
	    return MMIOERR_CHUNKNOTFOUND;
	}
    }

    /* The SDK docu says 'ckid' is used for all cases. Real World
     * examples disagree -Marcus,990216.
     */

    srchType = 0;
    /* find_chunk looks for 'ckid' */
    if (uFlags & MMIO_FINDCHUNK)
	srchCkId = lpck->ckid;
    /* find_riff and find_list look for 'fccType' */
    if (uFlags & MMIO_FINDLIST) {
	srchCkId = FOURCC_LIST;
	srchType = lpck->fccType;
    }
    if (uFlags & MMIO_FINDRIFF) {
	srchCkId = FOURCC_RIFF;
	srchType = lpck->fccType;
    }

    if (uFlags & (MMIO_FINDCHUNK|MMIO_FINDLIST|MMIO_FINDRIFF)) {
	TRACE("searching for %.4s.%.4s\n",
	      (LPSTR)&srchCkId,
	      srchType?(LPSTR)&srchType:"any ");

	while (TRUE) {
	    LONG ix;

	    ix = mmioRead(hmmio, (LPSTR)lpck, 3 * sizeof(DWORD));
#ifdef WORDS_BIGENDIAN
            wine_mmio_swap32(&(lpck->ckid));
            wine_mmio_swap32(&(lpck->cksize));
            wine_mmio_swap32(&(lpck->fccType));
#endif
	    if (ix < 2*sizeof(DWORD)) {
		mmioSeek(hmmio, dwOldPos, SEEK_SET);
		WARN("return ChunkNotFound\n");
		return MMIOERR_CHUNKNOTFOUND;
	    }
	    lpck->dwDataOffset = dwOldPos + 2 * sizeof(DWORD);
	    if (ix < lpck->dwDataOffset - dwOldPos) {
		mmioSeek(hmmio, dwOldPos, SEEK_SET);
		WARN("return ChunkNotFound\n");
		return MMIOERR_CHUNKNOTFOUND;
	    }
	    TRACE("ckid=%.4s fcc=%.4s cksize=%08lX !\n",
		  (LPSTR)&lpck->ckid,
		  srchType?(LPSTR)&lpck->fccType:"<na>",
		  lpck->cksize);
	    if ((srchCkId == lpck->ckid) &&
		(!srchType || (srchType == lpck->fccType))
		)
		break;

	    dwOldPos = lpck->dwDataOffset + ((lpck->cksize + 1) & ~1);
	    mmioSeek(hmmio, dwOldPos, SEEK_SET);
	}
    } else {
	/* FIXME: unverified, does it do this? */
	/* NB: This part is used by WAVE_mciOpen, among others */
	if (mmioRead(hmmio, (LPSTR)lpck, 3 * sizeof(DWORD)) < 3 * sizeof(DWORD)) {
	    mmioSeek(hmmio, dwOldPos, SEEK_SET);
	    WARN("return ChunkNotFound 2nd\n");
	    return MMIOERR_CHUNKNOTFOUND;
	}
#ifdef WORDS_BIGENDIAN 
        wine_mmio_swap32(&(lpck->ckid));
        wine_mmio_swap32(&(lpck->cksize));
        wine_mmio_swap32(&(lpck->fccType));
#endif
	lpck->dwDataOffset = dwOldPos + 2 * sizeof(DWORD);
    }
    lpck->dwFlags = 0;
    /* If we were looking for RIFF/LIST chunks, the final file position
     * is after the chunkid. If we were just looking for the chunk
     * it is after the cksize. So add 4 in RIFF/LIST case.
     */
    if (lpck->ckid == FOURCC_RIFF || lpck->ckid == FOURCC_LIST)
	mmioSeek(hmmio, lpck->dwDataOffset + sizeof(DWORD), SEEK_SET);
    else
	mmioSeek(hmmio, lpck->dwDataOffset, SEEK_SET);
    TRACE("lpck: ckid=%.4s, cksize=%ld, dwDataOffset=%ld fccType=%08lX (%.4s)!\n",
	  (LPSTR)&lpck->ckid, lpck->cksize, lpck->dwDataOffset,
	  lpck->fccType, srchType?(LPSTR)&lpck->fccType:"");
    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioDescend	       	[MMSYSTEM.1223]
 */
MMRESULT16 WINAPI mmioDescend16(HMMIO16 hmmio, LPMMCKINFO lpck,
                                const MMCKINFO* lpckParent, UINT16 uFlags)
{
    return mmioDescend(HMMIO_32(hmmio), lpck, lpckParent, uFlags);
}

/**************************************************************************
 * 				mmioAscend     		[WINMM.@]
 */
MMRESULT WINAPI mmioAscend(HMMIO hmmio, LPMMCKINFO lpck, UINT uFlags)
{
    TRACE("(%04X, %p, %04X);\n", hmmio, lpck, uFlags);

    if (lpck->dwFlags & MMIO_DIRTY) {
	DWORD	dwOldPos, dwNewSize;

	TRACE("Chunk is dirty, checking if chunk's size is correct\n");
	dwOldPos = mmioSeek(hmmio, 0, SEEK_CUR);
	TRACE("dwOldPos=%ld lpck->dwDataOffset = %ld\n", dwOldPos, lpck->dwDataOffset);
	dwNewSize = dwOldPos - lpck->dwDataOffset;
	if (dwNewSize != lpck->cksize) {
	    TRACE("Nope: lpck->cksize=%ld dwNewSize=%ld\n", lpck->cksize, dwNewSize);
	    lpck->cksize = dwNewSize;

	    /* pad odd size with 0 */
	    if (dwNewSize & 1) {
		char ch = 0;
		mmioWrite(hmmio, &ch, 1);
	    }
	    mmioSeek(hmmio, lpck->dwDataOffset - sizeof(DWORD), SEEK_SET);
	    mmioWrite(hmmio, (LPSTR)&dwNewSize, sizeof(DWORD));
	}
	lpck->dwFlags = 0;
    }

    mmioSeek(hmmio, lpck->dwDataOffset + ((lpck->cksize + 1) & ~1), SEEK_SET);

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioAscend     		[MMSYSTEM.1224]
 */
MMRESULT16 WINAPI mmioAscend16(HMMIO16 hmmio, MMCKINFO* lpck, UINT16 uFlags)
{
    return mmioAscend(HMMIO_32(hmmio),lpck,uFlags);
}

/**************************************************************************
 * 			mmioCreateChunk				[WINMM.@]
 */
MMRESULT WINAPI mmioCreateChunk(HMMIO hmmio, MMCKINFO* lpck, UINT uFlags)
{
    DWORD	dwOldPos;
    LONG 	size;
    LONG 	ix;

    TRACE("(%04X, %p, %04X);\n", hmmio, lpck, uFlags);

    dwOldPos = mmioSeek(hmmio, 0, SEEK_CUR);
    TRACE("dwOldPos=%ld\n", dwOldPos);

    if (uFlags == MMIO_CREATELIST)
	lpck->ckid = FOURCC_LIST;
    else if (uFlags == MMIO_CREATERIFF)
	lpck->ckid = FOURCC_RIFF;

    TRACE("ckid=%.4s\n", (LPSTR)&lpck->ckid);

    size = 2 * sizeof(DWORD);
    lpck->dwDataOffset = dwOldPos + size;

    if (lpck->ckid == FOURCC_RIFF || lpck->ckid == FOURCC_LIST)
	size += sizeof(DWORD);
    lpck->dwFlags = MMIO_DIRTY;

    ix = mmioWrite(hmmio, (LPSTR)lpck, size);
    TRACE("after mmioWrite ix = %ld req = %ld, errno = %d\n",ix, size, errno);
    if (ix < size) {
	mmioSeek(hmmio, dwOldPos, SEEK_SET);
	WARN("return CannotWrite\n");
	return MMIOERR_CANNOTWRITE;
    }

    return MMSYSERR_NOERROR;
}

/**************************************************************************
 * 				mmioCreateChunk		[MMSYSTEM.1225]
 */
MMRESULT16 WINAPI mmioCreateChunk16(HMMIO16 hmmio, MMCKINFO* lpck, UINT16 uFlags)
{
    return mmioCreateChunk(HMMIO_32(hmmio), lpck, uFlags);
}

/**************************************************************************
 * 				mmioRename     		[MMSYSTEM.1226]
 */
MMRESULT16 WINAPI mmioRename16(LPCSTR szFileName, LPCSTR szNewFileName,
                               MMIOINFO16* lpmmioinfo, DWORD dwRenameFlags)
{
    UINT16 		result = MMSYSERR_ERROR;
    LPMMIOPROC16	ioProc;

    TRACE("('%s', '%s', %p, %08lX);\n",
	  szFileName, szNewFileName, lpmmioinfo, dwRenameFlags);

    /* If both params are NULL, then parse the file name */
    if (lpmmioinfo && lpmmioinfo->fccIOProc == 0 && lpmmioinfo->pIOProc == NULL)
	lpmmioinfo->fccIOProc = MMIO_ParseExt(szFileName);

    /* Handle any unhandled/error case from above. Assume DOS file */
    if (!lpmmioinfo || (lpmmioinfo->fccIOProc == 0 && lpmmioinfo->pIOProc == NULL))
	ioProc = (LPMMIOPROC16)mmioDosIOProc;
    /* if just the four character code is present, look up IO proc */
    else if (lpmmioinfo->pIOProc == NULL)
	ioProc = mmioInstallIOProc16(lpmmioinfo->fccIOProc, NULL, MMIO_FINDPROC);
    else
 	ioProc = lpmmioinfo->pIOProc;

    /* FIXME: ioProc is likely a segmented address, thus needing a
     * thunk somewhere. The main issue is that Wine's current thunking
     * 32 to 16 only supports pascal calling convention
     */
    if (ioProc)
	result = (ioProc)(0, MMIOM_RENAME,
			  (LPARAM)szFileName, (LPARAM)szNewFileName);

    return result;
}

/**************************************************************************
 * 				mmioRenameA    			[WINMM.@]
 */
MMRESULT WINAPI mmioRenameA(LPCSTR szFileName, LPCSTR szNewFileName,
                            MMIOINFO* lpmmioinfo, DWORD dwRenameFlags)
{
    UINT	result = MMSYSERR_ERROR;
    LPMMIOPROC	ioProc;

    TRACE("('%s', '%s', %p, %08lX);\n",
	  szFileName, szNewFileName, lpmmioinfo, dwRenameFlags);

    /* If both params are NULL, then parse the file name */
    if (lpmmioinfo && lpmmioinfo->fccIOProc == 0 && lpmmioinfo->pIOProc == NULL)
	lpmmioinfo->fccIOProc = MMIO_ParseExt(szFileName);

    /* Handle any unhandled/error case from above. Assume DOS file */
    if (!lpmmioinfo || (lpmmioinfo->fccIOProc == 0 && lpmmioinfo->pIOProc == NULL))
	ioProc = (LPMMIOPROC)mmioDosIOProc;
    /* if just the four character code is present, look up IO proc */
    else if (lpmmioinfo->pIOProc == NULL)
	ioProc = MMIO_InstallIOProc(lpmmioinfo->fccIOProc, NULL,
				    MMIO_FINDPROC, MMIO_PROC_32A);
    else /* use relevant ioProc */
 	ioProc = lpmmioinfo->pIOProc;

    if (ioProc)
	result = (ioProc)(0, MMIOM_RENAME,
			  (LPARAM)szFileName, (LPARAM)szNewFileName);

    return result;
}

/**************************************************************************
 * 				mmioRenameW    			[WINMM.@]
 */
MMRESULT WINAPI mmioRenameW(LPCWSTR szFileName, LPCWSTR szNewFileName,
                            MMIOINFO* lpmmioinfo, DWORD dwRenameFlags)
{
    LPSTR	szFn = HEAP_strdupWtoA(GetProcessHeap(), 0, szFileName);
    LPSTR	sznFn = HEAP_strdupWtoA(GetProcessHeap(), 0, szNewFileName);
    UINT	ret = mmioRenameA(szFn, sznFn, lpmmioinfo, dwRenameFlags);

    HeapFree(GetProcessHeap(),0,szFn);
    HeapFree(GetProcessHeap(),0,sznFn);
    return ret;
}
