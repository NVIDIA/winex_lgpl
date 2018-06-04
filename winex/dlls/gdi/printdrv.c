/*
 * Implementation of some printer driver bits
 *
 * Copyright 1996 John Harvey
 * Copyright 1998 Huw Davies
 * Copyright 1998 Andreas Mohr
 * Copyright 1999 Klaas van Gend
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "winbase.h"
#include "winuser.h"
#include "wine/winbase16.h"
#include "wine/wingdi16.h"
#include "winspool.h"
#include "winerror.h"
#include "winreg.h"
#include "wine/debug.h"
#include "gdi.h"
#include "wine/heapstr.h"
#include "wine/file.h"

WINE_DEFAULT_DEBUG_CHANNEL(print);

static char PrinterModel[]	= "Printer Model";
static char DefaultDevMode[]	= "Default DevMode";
static char PrinterDriverData[] = "PrinterDriverData";
static char Printers[]		= "System\\CurrentControlSet\\Control\\Print\\Printers\\";

/******************************************************************
 *                  StartDoc  [GDI.377]
 *
 */
INT16 WINAPI StartDoc16( HDC16 hdc, const DOCINFO16 *lpdoc )
{
    DOCINFOA docA;

    docA.cbSize = lpdoc->cbSize;
    docA.lpszDocName = MapSL(lpdoc->lpszDocName);
    docA.lpszOutput = MapSL(lpdoc->lpszOutput);

    if(lpdoc->cbSize >= 14)
        docA.lpszDatatype = MapSL(lpdoc->lpszDatatype);
    else
        docA.lpszDatatype = NULL;

    if(lpdoc->cbSize >= 18)
        docA.fwType = lpdoc->fwType;
    else
        docA.fwType = 0;

    return StartDocA(hdc, &docA);
}

/******************************************************************
 *                  StartDocA  [GDI32.@]
 *
 * StartDoc calls the STARTDOC Escape with the input data pointing to DocName
 * and the output data (which is used as a second input parameter).pointing at
 * the whole docinfo structure.  This seems to be an undocumented feature of
 * the STARTDOC Escape.
 *
 * Note: we now do it the other way, with the STARTDOC Escape calling StartDoc.
 */
INT WINAPI StartDocA(HDC hdc, const DOCINFOA* doc)
{
    INT ret = 0;
    DC *dc = DC_GetDCPtr( hdc );

    TRACE("DocName = '%s' Output = '%s' Datatype = '%s'\n",
	  doc->lpszDocName, doc->lpszOutput, doc->lpszDatatype);

    if(!dc) return SP_ERROR;

    if (dc->funcs->pStartDoc) ret = dc->funcs->pStartDoc( dc, doc );
    GDI_ReleaseObj( hdc );
    return ret;
}

/*************************************************************************
 *                  StartDocW [GDI32.@]
 *
 */
INT WINAPI StartDocW(HDC hdc, const DOCINFOW* doc)
{
    DOCINFOA docA;
    INT ret;

    docA.cbSize = doc->cbSize;
    docA.lpszDocName = doc->lpszDocName ?
      HEAP_strdupWtoA( GetProcessHeap(), 0, doc->lpszDocName ) : NULL;
    docA.lpszOutput = doc->lpszOutput ?
      HEAP_strdupWtoA( GetProcessHeap(), 0, doc->lpszOutput ) : NULL;
    docA.lpszDatatype = doc->lpszDatatype ?
      HEAP_strdupWtoA( GetProcessHeap(), 0, doc->lpszDatatype ) : NULL;
    docA.fwType = doc->fwType;

    ret = StartDocA(hdc, &docA);

    if(docA.lpszDocName)
        HeapFree( GetProcessHeap(), 0, (LPSTR)docA.lpszDocName );
    if(docA.lpszOutput)
        HeapFree( GetProcessHeap(), 0, (LPSTR)docA.lpszOutput );
    if(docA.lpszDatatype)
        HeapFree( GetProcessHeap(), 0, (LPSTR)docA.lpszDatatype );

    return ret;
}

/******************************************************************
 *                  EndDoc  [GDI.378]
 *
 */
INT16 WINAPI EndDoc16(HDC16 hdc)
{
    return EndDoc(hdc);
}

/******************************************************************
 *                  EndDoc  [GDI32.@]
 *
 */
INT WINAPI EndDoc(HDC hdc)
{
    INT ret = 0;
    DC *dc = DC_GetDCPtr( hdc );
    if(!dc) return SP_ERROR;

    if (dc->funcs->pEndDoc) ret = dc->funcs->pEndDoc( dc );
    GDI_ReleaseObj( hdc );
    return ret;
}

/******************************************************************
 *                  StartPage  [GDI.379]
 *
 */
INT16 WINAPI StartPage16(HDC16 hdc)
{
    return StartPage(hdc);
}

/******************************************************************
 *                  StartPage  [GDI32.@]
 *
 */
INT WINAPI StartPage(HDC hdc)
{
    INT ret = 1;
    DC *dc = DC_GetDCPtr( hdc );
    if(!dc) return SP_ERROR;

    if(dc->funcs->pStartPage)
        ret = dc->funcs->pStartPage( dc );
    else
        FIXME("stub\n");
    GDI_ReleaseObj( hdc );
    return ret;
}

/******************************************************************
 *                  EndPage  [GDI.380]
 *
 */
INT16 WINAPI EndPage16( HDC16 hdc )
{
    return EndPage(hdc);
}

/******************************************************************
 *                  EndPage  [GDI32.@]
 *
 */
INT WINAPI EndPage(HDC hdc)
{
    INT ret = 0;
    DC *dc = DC_GetDCPtr( hdc );
    if(!dc) return SP_ERROR;

    if (dc->funcs->pEndPage) ret = dc->funcs->pEndPage( dc );
    GDI_ReleaseObj( hdc );
    if (!QueryAbort16( hdc, 0 ))
    {
        EndDoc( hdc );
        ret = 0;
    }
    return ret;
}

/******************************************************************************
 *                 AbortDoc  [GDI.382]
 */
INT16 WINAPI AbortDoc16(HDC16 hdc)
{
    return AbortDoc(hdc);
}

/******************************************************************************
 *                 AbortDoc  [GDI32.@]
 */
INT WINAPI AbortDoc(HDC hdc)
{
    INT ret = 0;
    DC *dc = DC_GetDCPtr( hdc );
    if(!dc) return SP_ERROR;

    if (dc->funcs->pAbortDoc) ret = dc->funcs->pAbortDoc( dc );
    GDI_ReleaseObj( hdc );
    return ret;
}

/**********************************************************************
 *           QueryAbort   (GDI.155)
 *
 *  Calls the app's AbortProc function if avail.
 *
 * RETURNS
 * TRUE if no AbortProc avail or AbortProc wants to continue printing.
 * FALSE if AbortProc wants to abort printing.
 */
BOOL16 WINAPI QueryAbort16(HDC16 hdc, INT16 reserved)
{
    BOOL ret = TRUE;
    DC *dc = DC_GetDCPtr( hdc );
    ABORTPROC abproc;

    if(!dc) {
        ERR("Invalid hdc %04x\n", hdc);
	return FALSE;
    }

    abproc = dc->pAbortProc;
    GDI_ReleaseObj( hdc );

    if (abproc)
	ret = abproc(hdc, 0);
    return ret;
}

/* ### start build ### */
extern WORD CALLBACK PRTDRV_CallTo16_word_ww(FARPROC16,WORD,WORD);
/* ### stop build ### */

/**********************************************************************
 *           call_abort_proc16
 */
static BOOL CALLBACK call_abort_proc16( HDC hdc, INT code )
{
    ABORTPROC16 proc16;
    DC *dc = DC_GetDCPtr( hdc );

    if (!dc) return FALSE;
    proc16 = dc->pAbortProc16;
    GDI_ReleaseObj( hdc );
    if (proc16) return PRTDRV_CallTo16_word_ww( (FARPROC16)proc16, hdc, code );
    return TRUE;
}


/**********************************************************************
 *           SetAbortProc   (GDI.381)
 */
INT16 WINAPI SetAbortProc16(HDC16 hdc, ABORTPROC16 abrtprc)
{
    DC *dc = DC_GetDCPtr( hdc );

    if (!dc) return FALSE;
    dc->pAbortProc16 = abrtprc;
    GDI_ReleaseObj( hdc );
    return SetAbortProc( hdc, call_abort_proc16 );
}

/**********************************************************************
 *           SetAbortProc   (GDI32.@)
 *
 */
INT WINAPI SetAbortProc(HDC hdc, ABORTPROC abrtprc)
{
    DC *dc = DC_GetDCPtr( hdc );

    if (!dc) return FALSE;
    dc->pAbortProc = abrtprc;
    GDI_ReleaseObj( hdc );
    return TRUE;
}


/****************** misc. printer related functions */

/*
 * The following function should implement a queing system
 */
struct hpq
{
    struct hpq 	*next;
    int		 tag;
    int		 key;
};

static struct hpq *hpqueue;

/**********************************************************************
 *           CreatePQ   (GDI.230)
 *
 */
HPQ16 WINAPI CreatePQ16(INT16 size)
{
#if 0
    HGLOBAL16 hpq = 0;
    WORD tmp_size;
    LPWORD pPQ;

    tmp_size = size << 2;
    if (!(hpq = GlobalAlloc16(GMEM_SHARE|GMEM_MOVEABLE, tmp_size + 8)))
       return 0xffff;
    pPQ = GlobalLock16(hpq);
    *pPQ++ = 0;
    *pPQ++ = tmp_size;
    *pPQ++ = 0;
    *pPQ++ = 0;
    GlobalUnlock16(hpq);

    return (HPQ16)hpq;
#else
    FIXME("(%d): stub\n",size);
    return 1;
#endif
}

/**********************************************************************
 *           DeletePQ   (GDI.235)
 *
 */
INT16 WINAPI DeletePQ16(HPQ16 hPQ)
{
    return GlobalFree16((HGLOBAL16)hPQ);
}

/**********************************************************************
 *           ExtractPQ   (GDI.232)
 *
 */
INT16 WINAPI ExtractPQ16(HPQ16 hPQ)
{
    struct hpq *queue, *prev, *current, *currentPrev;
    int key = 0, tag = -1;
    currentPrev = prev = NULL;
    queue = current = hpqueue;
    if (current)
        key = current->key;

    while (current)
    {
        currentPrev = current;
        current = current->next;
        if (current)
        {
            if (current->key < key)
            {
                queue = current;
                prev = currentPrev;
            }
        }
    }
    if (queue)
    {
        tag = queue->tag;

        if (prev)
            prev->next = queue->next;
        else
            hpqueue = queue->next;
        HeapFree(GetProcessHeap(), 0, queue);
    }

    TRACE("%x got tag %d key %d\n", hPQ, tag, key);

    return tag;
}

/**********************************************************************
 *           InsertPQ   (GDI.233)
 *
 */
INT16 WINAPI InsertPQ16(HPQ16 hPQ, INT16 tag, INT16 key)
{
    struct hpq *queueItem = HeapAlloc(GetProcessHeap(), 0, sizeof(struct hpq));
    if(queueItem == NULL) {
        ERR("Memory exausted!\n");
        return FALSE;
    }
    queueItem->next = hpqueue;
    hpqueue = queueItem;
    queueItem->key = key;
    queueItem->tag = tag;

    FIXME("(%x %d %d): stub???\n", hPQ, tag, key);
    return TRUE;
}

/**********************************************************************
 *           MinPQ   (GDI.231)
 *
 */
INT16 WINAPI MinPQ16(HPQ16 hPQ)
{
    FIXME("(%x): stub\n", hPQ);
    return 0;
}

/**********************************************************************
 *           SizePQ   (GDI.234)
 *
 */
INT16 WINAPI SizePQ16(HPQ16 hPQ, INT16 sizechange)
{
    FIXME("(%x %d): stub\n", hPQ, sizechange);
    return -1;
}



/*
 * The following functions implement part of the spooling process to
 * print manager.  I would like to see wine have a version of print managers
 * that used LPR/LPD.  For simplicity print jobs will be sent to a file for
 * now.
 */
typedef struct PRINTJOB
{
    char	*pszOutput;
    char 	*pszTitle;
    HDC16  	hDC;
    HANDLE16 	hHandle;
    int		nIndex;
    int		fd;
} PRINTJOB, *PPRINTJOB;

#define MAX_PRINT_JOBS 1
#define SP_OK 1

PPRINTJOB gPrintJobsTable[MAX_PRINT_JOBS];


static PPRINTJOB FindPrintJobFromHandle(HANDLE16 hHandle)
{
    return gPrintJobsTable[0];
}

static int CreateSpoolFile(LPCSTR pszOutput)
{
    int fd=-1;
    char psCmd[1024];
    char *psCmdP = psCmd;

    /* TTD convert the 'output device' into a spool file name */

    if (pszOutput == NULL || *pszOutput == '\0')
      return -1;

    if (!strncmp("LPR:",pszOutput,4))
      sprintf(psCmd,"|lpr -P%s",pszOutput+4);
    else
    {
	HKEY hkey;
	/* default value */
	psCmd[0] = 0;
	if(!RegOpenKeyA(HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\spooler", &hkey))
	{
	    DWORD type, count = sizeof(psCmd);
	    RegQueryValueExA(hkey, pszOutput, 0, &type, psCmd, &count);
	    RegCloseKey(hkey);
	}
    }
    TRACE("Got printerSpoolCommand '%s' for output device '%s'\n",
	  psCmd, pszOutput);
    if (!*psCmd)
        psCmdP = (char *)pszOutput;
    else
    {
        while (*psCmdP && isspace(*psCmdP))
        {
            psCmdP++;
        };
        if (!*psCmdP)
            return -1;
    }
    if (*psCmdP == '|')
    {
        int fds[2];
        if (pipe(fds))
            return -1;
        if (fork() == 0)
        {
            psCmdP++;

            TRACE("In child need to exec %s\n",psCmdP);
            close(0);
            dup2(fds[0],0);
            close (fds[1]);

            signal (SIGPIPE, SIG_DFL);
            signal (SIGCHLD, SIG_DFL);
            execl ("/bin/sh", "/bin/sh", "-c", psCmdP, NULL);
            _exit (1);
        }
        close (fds[0]);
        fd = fds[1];
        TRACE("Need to execute a cmnd and pipe the output to it\n");
    }
    else
    {
	char buffer[MAX_PATH];

        TRACE("Just assume it's a file\n");

        /**
         * The file name can be dos based, we have to find its
         * Unix correspondant file name
         */
	wine_get_unix_file_name(psCmdP, buffer, sizeof(buffer));

        if ((fd = open(buffer, O_CREAT | O_TRUNC | O_WRONLY , 0600)) < 0)
        {
            ERR("Failed to create spool file '%s' ('%s'). (error %s)\n",
                buffer, psCmdP, strerror(errno));
        }
    }
    return fd;
}

static int FreePrintJob(HANDLE16 hJob)
{
    int nRet = SP_ERROR;
    PPRINTJOB pPrintJob;

    pPrintJob = FindPrintJobFromHandle(hJob);
    if (pPrintJob != NULL)
    {
	gPrintJobsTable[pPrintJob->nIndex] = NULL;
	HeapFree(GetProcessHeap(), 0, pPrintJob->pszOutput);
	HeapFree(GetProcessHeap(), 0, pPrintJob->pszTitle);
	if (pPrintJob->fd >= 0) close(pPrintJob->fd);
	HeapFree(GetProcessHeap(), 0, pPrintJob);
	nRet = SP_OK;
    }
    return nRet;
}

/**********************************************************************
 *           OpenJob   (GDI.240)
 *           OpenJob16 (GDI32.@)
 *
 */
HPJOB16 WINAPI OpenJob16(LPCSTR lpOutput, LPCSTR lpTitle, HDC16 hDC)
{
    HPJOB16 hHandle = (HPJOB16)SP_ERROR;
    PPRINTJOB pPrintJob;

    TRACE("'%s' '%s' %04x\n", lpOutput, lpTitle, hDC);

    pPrintJob = gPrintJobsTable[0];
    if (pPrintJob == NULL)
    {
	int fd;

	/* Try and create a spool file */
	fd = CreateSpoolFile(lpOutput);
	if (fd >= 0)
	{
	    pPrintJob = HeapAlloc(GetProcessHeap(), 0, sizeof(PRINTJOB));
            if(pPrintJob == NULL) {
                WARN("Memory exausted!\n");
                return hHandle;
            }

            hHandle = 1;

	    pPrintJob->pszOutput = HeapAlloc(GetProcessHeap(), 0, strlen(lpOutput)+1);
	    strcpy( pPrintJob->pszOutput, lpOutput );
	    if(lpTitle)
            {
	        pPrintJob->pszTitle = HeapAlloc(GetProcessHeap(), 0, strlen(lpTitle)+1);
	        strcpy( pPrintJob->pszTitle, lpTitle );
            }
	    pPrintJob->hDC = hDC;
	    pPrintJob->fd = fd;
	    pPrintJob->nIndex = 0;
	    pPrintJob->hHandle = hHandle;
	    gPrintJobsTable[pPrintJob->nIndex] = pPrintJob;
	}
    }
    TRACE("return %04x\n", hHandle);
    return hHandle;
}

/**********************************************************************
 *           CloseJob   (GDI.243)
 *           CloseJob16 (GDI32.@)
 *
 */
INT16 WINAPI CloseJob16(HPJOB16 hJob)
{
    int nRet = SP_ERROR;
    PPRINTJOB pPrintJob = NULL;

    TRACE("%04x\n", hJob);

    pPrintJob = FindPrintJobFromHandle(hJob);
    if (pPrintJob != NULL)
    {
	/* Close the spool file */
	close(pPrintJob->fd);
	FreePrintJob(hJob);
	nRet  = 1;
    }
    return nRet;
}

/**********************************************************************
 *           WriteSpool   (GDI.241)
 *           WriteSpool16 (GDI32.@)
 *
 */
INT16 WINAPI WriteSpool16(HPJOB16 hJob, LPSTR lpData, INT16 cch)
{
    int nRet = SP_ERROR;
    PPRINTJOB pPrintJob = NULL;

    TRACE("%04x %08lx %04x\n", hJob, (DWORD)lpData, cch);

    pPrintJob = FindPrintJobFromHandle(hJob);
    if (pPrintJob != NULL && pPrintJob->fd >= 0 && cch)
    {
	if (write(pPrintJob->fd, lpData, cch) != cch)
	  nRet = SP_OUTOFDISK;
	else
	  nRet = cch;
#if 0
	/* FIXME: We just cannot call 16 bit functions from here, since we
	 * have acquired several locks (DC). And we do not really need to.
	 */
	if (pPrintJob->hDC == 0) {
	    TRACE("hDC == 0 so no QueryAbort\n");
	}
        else if (!(QueryAbort16(pPrintJob->hDC, (nRet == SP_OUTOFDISK) ? nRet : 0 )))
	{
	    CloseJob16(hJob); /* printing aborted */
	    nRet = SP_APPABORT;
	}
#endif
    }
    return nRet;
}

typedef INT (WINAPI *MSGBOX_PROC)( HWND, LPCSTR, LPCSTR, UINT );

/**********************************************************************
 *           WriteDialog   (GDI.242)
 *
 */
INT16 WINAPI WriteDialog16(HPJOB16 hJob, LPSTR lpMsg, INT16 cchMsg)
{
    HMODULE mod;
    MSGBOX_PROC pMessageBoxA;
    INT16 ret = 0;

    TRACE("%04x %04x '%s'\n", hJob,  cchMsg, lpMsg);

    if ((mod = GetModuleHandleA("user32.dll")))
    {
        if ((pMessageBoxA = (MSGBOX_PROC)GetProcAddress( mod, "MessageBoxA" )))
            ret = pMessageBoxA(0, lpMsg, "Printing Error", MB_OKCANCEL);
    }
    return ret;
}


/**********************************************************************
 *           DeleteJob  (GDI.244)
 *
 */
INT16 WINAPI DeleteJob16(HPJOB16 hJob, INT16 nNotUsed)
{
    int nRet;

    TRACE("%04x\n", hJob);

    nRet = FreePrintJob(hJob);
    return nRet;
}

/*
 * The following two function would allow a page to be sent to the printer
 * when it has been processed.  For simplicity they havn't been implemented.
 * This means a whole job has to be processed before it is sent to the printer.
 */

/**********************************************************************
 *           StartSpoolPage   (GDI.246)
 *
 */
INT16 WINAPI StartSpoolPage16(HPJOB16 hJob)
{
    FIXME("StartSpoolPage GDI.246 unimplemented\n");
    return 1;

}


/**********************************************************************
 *           EndSpoolPage   (GDI.247)
 *
 */
INT16 WINAPI EndSpoolPage16(HPJOB16 hJob)
{
    FIXME("EndSpoolPage GDI.247 unimplemented\n");
    return 1;
}


/**********************************************************************
 *           GetSpoolJob   (GDI.245)
 *
 */
DWORD WINAPI GetSpoolJob16(int nOption, LONG param)
{
    DWORD retval = 0;
    TRACE("In GetSpoolJob param 0x%lx noption %d\n",param, nOption);
    return retval;
}


/******************************************************************
 *                  DrvGetPrinterDataInternal
 *
 * Helper for DrvGetPrinterData
 */
static DWORD DrvGetPrinterDataInternal(LPSTR RegStr_Printer,
LPBYTE lpPrinterData, int cbData, int what)
{
    DWORD res = -1;
    HKEY hkey;
    DWORD dwType, cbQueryData;

    if (!(RegOpenKeyA(HKEY_LOCAL_MACHINE, RegStr_Printer, &hkey))) {
        if (what == INT_PD_DEFAULT_DEVMODE) { /* "Default DevMode" */
            if (!(RegQueryValueExA(hkey, DefaultDevMode, 0, &dwType, 0, &cbQueryData))) {
                if (!lpPrinterData)
		    res = cbQueryData;
		else if ((cbQueryData) && (cbQueryData <= cbData)) {
		    cbQueryData = cbData;
		    if (RegQueryValueExA(hkey, DefaultDevMode, 0,
				&dwType, lpPrinterData, &cbQueryData))
		        res = cbQueryData;
		}
	    }
	} else { /* "Printer Driver" */
	    cbQueryData = 32;
	    RegQueryValueExA(hkey, "Printer Driver", 0,
			&dwType, lpPrinterData, &cbQueryData);
	    res = cbQueryData;
	}
    }
    if (hkey) RegCloseKey(hkey);
    return res;
}

/******************************************************************
 *                DrvGetPrinterData     (GDI.282)
 *                DrvGetPrinterData16   (GDI32.@)
 *
 */
DWORD WINAPI DrvGetPrinterData16(LPSTR lpPrinter, LPSTR lpProfile,
                               LPDWORD lpType, LPBYTE lpPrinterData,
                               int cbData, LPDWORD lpNeeded)
{
    LPSTR RegStr_Printer;
    HKEY hkey = 0, hkey2 = 0;
    DWORD res = 0;
    DWORD dwType, PrinterAttr, cbPrinterAttr, SetData, size;

    if (HIWORD(lpPrinter))
            TRACE("printer %s\n",lpPrinter);
    else
            TRACE("printer %p\n",lpPrinter);
    if (HIWORD(lpProfile))
            TRACE("profile %s\n",lpProfile);
    else
            TRACE("profile %p\n",lpProfile);
    TRACE("lpType %p\n",lpType);

    if ((!lpPrinter) || (!lpProfile) || (!lpNeeded))
	return ERROR_INVALID_PARAMETER;

    RegStr_Printer = HeapAlloc(GetProcessHeap(), 0,
                               strlen(Printers) + strlen(lpPrinter) + 2);
    strcpy(RegStr_Printer, Printers);
    strcat(RegStr_Printer, lpPrinter);

    if (((DWORD)lpProfile == INT_PD_DEFAULT_DEVMODE) || (HIWORD(lpProfile) &&
    (!strcmp(lpProfile, DefaultDevMode)))) {
	size = DrvGetPrinterDataInternal(RegStr_Printer, lpPrinterData, cbData,
					 INT_PD_DEFAULT_DEVMODE);
	if (size+1) {
	    *lpNeeded = size;
	    if ((lpPrinterData) && (*lpNeeded > cbData))
		res = ERROR_MORE_DATA;
	}
	else res = ERROR_INVALID_PRINTER_NAME;
    }
    else
    if (((DWORD)lpProfile == INT_PD_DEFAULT_MODEL) || (HIWORD(lpProfile) &&
    (!strcmp(lpProfile, PrinterModel)))) {
	*lpNeeded = 32;
	if (!lpPrinterData) goto failed;
	if (cbData < 32) {
	    res = ERROR_MORE_DATA;
	    goto failed;
	}
	size = DrvGetPrinterDataInternal(RegStr_Printer, lpPrinterData, cbData,
					 INT_PD_DEFAULT_MODEL);
	if ((size+1) && (lpType))
	    *lpType = REG_SZ;
	else
	    res = ERROR_INVALID_PRINTER_NAME;
    }
    else
    {
	if ((res = RegOpenKeyA(HKEY_LOCAL_MACHINE, RegStr_Printer, &hkey)))
	    goto failed;
        cbPrinterAttr = 4;
        if ((res = RegQueryValueExA(hkey, "Attributes", 0,
                        &dwType, (LPBYTE)&PrinterAttr, &cbPrinterAttr)))
	    goto failed;
	if ((res = RegOpenKeyA(hkey, PrinterDriverData, &hkey2)))
	    goto failed;
        *lpNeeded = cbData;
        res = RegQueryValueExA(hkey2, lpProfile, 0,
                lpType, lpPrinterData, lpNeeded);
        if ((res != ERROR_CANTREAD) &&
         ((PrinterAttr &
        (PRINTER_ATTRIBUTE_ENABLE_BIDI|PRINTER_ATTRIBUTE_NETWORK))
        == PRINTER_ATTRIBUTE_NETWORK))
        {
	    if (!(res) && (*lpType == REG_DWORD) && (*(LPDWORD)lpPrinterData == -1))
	        res = ERROR_INVALID_DATA;
	}
	else
        {
	    SetData = -1;
	    RegSetValueExA(hkey2, lpProfile, 0, REG_DWORD, (LPBYTE)&SetData, 4); /* no result returned */
	}
    }

failed:
    if (hkey2) RegCloseKey(hkey2);
    if (hkey) RegCloseKey(hkey);
    HeapFree(GetProcessHeap(), 0, RegStr_Printer);
    return res;
}


/******************************************************************
 *                 DrvSetPrinterData     (GDI.281)
 *                 DrvSetPrinterData16   (GDI32.@)
 *
 */
DWORD WINAPI DrvSetPrinterData16(LPSTR lpPrinter, LPSTR lpProfile,
                               DWORD lpType, LPBYTE lpPrinterData,
                               DWORD dwSize)
{
    LPSTR RegStr_Printer;
    HKEY hkey = 0;
    DWORD res = 0;

    if (HIWORD(lpPrinter))
            TRACE("printer %s\n",lpPrinter);
    else
            TRACE("printer %p\n",lpPrinter);
    if (HIWORD(lpProfile))
            TRACE("profile %s\n",lpProfile);
    else
            TRACE("profile %p\n",lpProfile);
    TRACE("lpType %08lx\n",lpType);

    if ((!lpPrinter) || (!lpProfile) ||
    ((DWORD)lpProfile == INT_PD_DEFAULT_MODEL) || (HIWORD(lpProfile) &&
    (!strcmp(lpProfile, PrinterModel))))
	return ERROR_INVALID_PARAMETER;

    RegStr_Printer = HeapAlloc(GetProcessHeap(), 0,
			strlen(Printers) + strlen(lpPrinter) + 2);
    strcpy(RegStr_Printer, Printers);
    strcat(RegStr_Printer, lpPrinter);

    if (((DWORD)lpProfile == INT_PD_DEFAULT_DEVMODE) || (HIWORD(lpProfile) &&
    (!strcmp(lpProfile, DefaultDevMode)))) {
	if ( RegOpenKeyA(HKEY_LOCAL_MACHINE, RegStr_Printer, &hkey)
	     != ERROR_SUCCESS ||
	     RegSetValueExA(hkey, DefaultDevMode, 0, REG_BINARY,
			      lpPrinterData, dwSize) != ERROR_SUCCESS )
	        res = ERROR_INVALID_PRINTER_NAME;
    }
    else
    {
	strcat(RegStr_Printer, "\\");

	if( (res = RegOpenKeyA(HKEY_LOCAL_MACHINE, RegStr_Printer, &hkey)) ==
	    ERROR_SUCCESS ) {

	    if (!lpPrinterData)
	        res = RegDeleteValueA(hkey, lpProfile);
	    else
                res = RegSetValueExA(hkey, lpProfile, 0, lpType,
				       lpPrinterData, dwSize);
	}
    }

    if (hkey) RegCloseKey(hkey);
    HeapFree(GetProcessHeap(), 0, RegStr_Printer);
    return res;
}
