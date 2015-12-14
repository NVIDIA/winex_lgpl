/*
 * 				Shell Library Functions
 *
 *  1998 Marcus Meissner
 *  2002 Eric Pouech
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "windef.h"
#include "winerror.h"
#include "winreg.h"
#include "winbase.h"
#include "winuser.h"
#include "heap.h"
#include "wine/file.h"
#include "shellapi.h"
#include "shlobj.h"
#include "shlwapi.h"
#include "ddeml.h"

#include "wine/winbase16.h"
#include "shell32_main.h"
#include "undocshell.h"

#include "wine/debug.h"
#include "wine/sdldrv_ext.h"

WINE_DEFAULT_DEBUG_CHANNEL(exec);

#define SIZEOFARRAY(x)  (sizeof(x) / sizeof((x)[0]))

/* this function is supposed to expand the escape sequences found in the registry
 * some diving reported that the following were used:
 * + %1, %2...  seem to report to parameter of index N in ShellExecute pmts
 *	%1 file
 *	%2 printer
 *	%3 driver
 *	%4 port
 * %I adress of a global item ID (explorer switch /idlist)
 * %L seems to be %1 as long filename followed by the 8+3 variation
 * %S ???
 * %* all following parameters (see batfile)
 */
static BOOL argify(char* res, int len, const char* fmt, const char* lpFile)
{
    BOOL        done = FALSE;
    char *      end = res + len;


    while (*fmt && res < end)
    {
        if (*fmt == '%')
        {
            switch (*++fmt)
            {
            case '\0':
            case '%':
                *res++ = '%';
                break;
            case '1':
            case '*':
                if (!done || (*fmt == '1'))
                {
                    char xlpFile[MAX_PATH];
                    
                    
                    if (SearchPathA(NULL, lpFile, ".exe", SIZEOFARRAY(xlpFile), xlpFile, NULL))
                    {
                        strncpy(res, xlpFile, end - res);
                        res += strlen(xlpFile);
                    }
                    else
                    {
                        strncpy(res, lpFile, end - res);
                        res += strlen(lpFile);
                    }
                }
                break;
            default: FIXME("Unknown escape sequence %%%c\n", *fmt);
            }
            fmt++;
            done = TRUE;
        }
        else
            *res++ = *fmt++;
    }
    *res = '\0';
    return done;
}

/*****************************************************************************
 *              getRegKeyValue()
 * 
 *  retrieves the value of a registry key.  The buffer for the key will be
 *  automatically allocated to be an appropriate size for the value, and
 *  will be returned in the <buffer> parameter.  The allocated size of the
 *  buffer will be returned in the <bufferLength> parameter.  The <extraBytes>
 *  parameter can be used to allocate extra space in the value buffer and
 *  can be used in two ways:
 *      - extraBytes > 0: adds <extraBytes> bytes to the buffer size
 *      - extraBytes < 0: allocates a minimum of -<extraBytes> bytes
 *  
 *  the return value is the result of the RegQueryValueA() call.  On success,
 *  this will be S_OK.  On failure, an error code will be returned and <buffer>
 *  and <bufferLength> will be set to NULL and 0 respectively.  The returned
 *  buffer must be destroyed using the destroyRegKeyValue() function (below).
 *  
 */
static LONG getRegKeyValue(HKEY root, LPCSTR subKey, LPSTR *buffer, LONG *bufferLength, INT extraBytes){
    LONG    len = 0;
    LONG    error;
    char *  str;
    
    
    error = RegQueryValueA(root, subKey, NULL, &len);

    if (error != ERROR_SUCCESS){
        ERR("could not get the length of the data for key %s\n", debugstr_a(subKey));
        *buffer = NULL;
        *bufferLength = 0;
        
        return error;    
    }
    
    
    if (extraBytes < 0)
        len = max(len, -extraBytes);
        
    else
        len = len + extraBytes + 1;
        
        
    str = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
    
    if (str == NULL){
        ERR("could not allocate %ld bytes to store the value for the key %s\n", len, debugstr_a(subKey));
        *buffer = NULL;
        *bufferLength = 0;
        
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    
    
    error = RegQueryValueA(root, subKey, str, &len);
    
    *buffer = str;
    *bufferLength = len;
    
    return error;
}

/*****************************************************************************
 *              destroyRegKeyValue()
 * 
 *  function to handle the destruction of a key value buffer returned by 
 *  getRegKeyValue().
 */
static void destroyRegKeyValue(LPSTR value){
    if (value)
        HeapFree(GetProcessHeap(), 0, value);
}

/*************************************************************************
 *	SHELL_ExecuteA [Internal]
 *
 */
static HINSTANCE SHELL_ExecuteA(char *lpCmd, LPSHELLEXECUTEINFOA sei, BOOL is32)
{
    STARTUPINFOA  startup;
    PROCESS_INFORMATION info;
    HINSTANCE retval = 31;

    TRACE("Execute %s from directory %s\n", lpCmd, sei->lpDirectory);
    ZeroMemory(&startup,sizeof(STARTUPINFOA));
    startup.cb = sizeof(STARTUPINFOA);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = sei->nShow;
    if (is32)
    {
        if (CreateProcessA(NULL, lpCmd, NULL, NULL, FALSE, 0,
                        NULL, sei->lpDirectory, &startup, &info))
        {
            retval = (HINSTANCE)33;
            if(sei->fMask & SEE_MASK_NOCLOSEPROCESS)
	        sei->hProcess = info.hProcess;
            else
                CloseHandle( info.hProcess );

            CloseHandle( info.hThread );
        }
        else if ((retval = GetLastError()) >= (HINSTANCE)32)
        {
            FIXME("Strange error set by CreateProcess: %d\n", retval);
            retval = (HINSTANCE)ERROR_BAD_FORMAT;
        }
    }
    else
        retval = WinExec16(lpCmd, sei->nShow);

    sei->hInstApp = retval;
    return retval;
}

/*************************************************************************
 *	SHELL_FindExecutable [Internal]
 *
 * Utility for code sharing between FindExecutable and ShellExecute
 * in:
 *      lpFile the name of a file
 *      lpOperation the operation on it (open)
 * out:
 *      lpResult a buffer, big enough :-(, to store the command to do the
 *              operation on the file
 *      key a buffer, big enough, to get the key name to do actually the
 *              command (it'll be used afterwards for more information
 *              on the operation)
 */
static HINSTANCE SHELL_FindExecutable(LPCSTR lpPath, LPCSTR lpFile, LPCSTR lpOperation,
                                      LPSTR lpResult, LPSTR key)
{
    char *extension = NULL; /* pointer to file extension */
    char tmpext[5];         /* local copy to mung as we please */
    char filetype[256];     /* registry name for this filetype */
    LONG filetypelen = 256; /* length of above */
    char command[256];      /* command from registry */
    LONG commandlen = 256;  /* This is the most DOS can handle :) */
    char buffer[256];       /* Used to GetProfileString */
    HINSTANCE retval = 31;  /* default - 'No association was found' */
    char *tok;              /* token pointer */
    int i;                  /* random counter */
    char xlpFile[256] = ""; /* result of SearchPath */

    TRACE("%s\n", (lpFile != NULL) ? lpFile : "-");

    lpResult[0] = '\0'; /* Start off with an empty return string */

    /* trap NULL parameters on entry */
    if ((lpFile == NULL) || (lpResult == NULL) || (lpOperation == NULL))
    {
        WARN("(lpFile=%s,lpResult=%s,lpOperation=%s): NULL parameter\n",
             lpFile, lpOperation, lpResult);
        return 2; /* File not found. Close enough, I guess. */
    }

    if (SearchPathA(lpPath, lpFile, ".exe", sizeof(xlpFile), xlpFile, NULL))
    {
        TRACE("SearchPathA returned non-zero\n");
        lpFile = xlpFile;
    }

    /* First thing we need is the file's extension */
    extension = strrchr(xlpFile, '.'); /* Assume last "." is the one; */
                                       /* File->Run in progman uses */
                                       /* .\FILE.EXE :( */
    TRACE("xlpFile=%s,extension=%s\n", xlpFile, extension);

    if ((extension == NULL) || (extension == &xlpFile[strlen(xlpFile)]))
    {
        WARN("Returning 31 - No association\n");
        return 31; /* no association */
    }

    /* Make local copy & lowercase it for reg & 'programs=' lookup */
    lstrcpynA(tmpext, extension, 5);
    CharLowerA(tmpext);
    TRACE("%s file\n", tmpext);

    /* Three places to check: */
    /* 1. win.ini, [windows], programs (NB no leading '.') */
    /* 2. Registry, HKEY_CLASS_ROOT\<filetype>\shell\open\command */
    /* 3. win.ini, [extensions], extension (NB no leading '.' */
    /* All I know of the order is that registry is checked before */
    /* extensions; however, it'd make sense to check the programs */
    /* section first, so that's what happens here. */

    if (key) *key = '\0';

    /* See if it's a program - if GetProfileString fails, we skip this
     * section. Actually, if GetProfileString fails, we've probably
     * got a lot more to worry about than running a program... */
    if (GetProfileStringA("windows", "programs", "exe pif bat com",
                          buffer, sizeof(buffer)) > 0)
    {
        for (i = 0;i<strlen(buffer); i++) buffer[i] = tolower(buffer[i]);

        tok = strtok(buffer, " \t"); /* ? */
        while (tok!= NULL)
        {
            if (strcmp(tok, &tmpext[1]) == 0) /* have to skip the leading "." */
            {
                strcpy(lpResult, xlpFile);
                /* Need to perhaps check that the file has a path
                 * attached */
                TRACE("found %s\n", lpResult);
                return 33;

		/* Greater than 32 to indicate success FIXME According to the
		 * docs, I should be returning a handle for the
		 * executable. Does this mean I'm supposed to open the
		 * executable file or something? More RTFM, I guess... */
            }
            tok = strtok(NULL, " \t");
        }
    }

    /* Check registry */
    if (RegQueryValueA(HKEY_CLASSES_ROOT, tmpext, filetype,
                       &filetypelen) == ERROR_SUCCESS)
    {
	filetype[filetypelen] = '\0';
	TRACE("File type: %s\n", filetype);

	/* Looking for ...buffer\shell\lpOperation\command */
	strcat(filetype, "\\shell\\");
	strcat(filetype, lpOperation);
	strcat(filetype, "\\command");

	if (RegQueryValueA(HKEY_CLASSES_ROOT, filetype, command,
                           &commandlen) == ERROR_SUCCESS)
	{
            if (key) strcpy(key, filetype);
#if 0
            LPSTR tmp;
            char param[256];
	    LONG paramlen = 256;

            /* FIXME: it seems all Windows version don't behave the same here.
             * the doc states that this ddeexec information can be found after
             * the exec names.
             * on Win98, it doesn't appear, but I think it does on Win2k
             */
	    /* Get the parameters needed by the application
	       from the associated ddeexec key */
	    tmp = strstr(filetype, "command");
	    tmp[0] = '\0';
	    strcat(filetype, "ddeexec");

	    if (RegQueryValueA(HKEY_CLASSES_ROOT, filetype, param, &paramlen) == ERROR_SUCCESS)
	    {
                strcat(command, " ");
                strcat(command, param);
                commandlen += paramlen;
	    }
#endif
	    command[commandlen] = '\0';
            argify(lpResult, sizeof(lpResult), command, xlpFile);
	    retval = 33; /* FIXME see above */
	}
    }
    else /* Check win.ini */
    {
	/* Toss the leading dot */
	extension++;
	if (GetProfileStringA("extensions", extension, "", command,
                              sizeof(command)) > 0)
        {
            if (strlen(command) != 0)
            {
                strcpy(lpResult, command);
                tok = strstr(lpResult, "^"); /* should be ^.extension? */
                if (tok != NULL)
                {
                    tok[0] = '\0';
                    strcat(lpResult, xlpFile); /* what if no dir in xlpFile? */
                    tok = strstr(command, "^"); /* see above */
                    if ((tok != NULL) && (strlen(tok)>5))
                    {
                        strcat(lpResult, &tok[5]);
                    }
                }
                retval = 33; /* FIXME - see above */
            }
        }
    }

    TRACE("returning %s\n", lpResult);
    return retval;
}

/******************************************************************
 *		dde_cb
 *
 * callback for the DDE connection. not really usefull
 */
static HDDEDATA CALLBACK dde_cb(UINT uType, UINT uFmt, HCONV hConv,
                                HSZ hsz1, HSZ hsz2,
                                HDDEDATA hData, DWORD dwData1, DWORD dwData2)
{
    return (HDDEDATA)0;
}

/******************************************************************
 *		dde_connect
 *
 * ShellExecute helper. Used to do an operation with a DDE connection
 *
 * Handles both the direct connection (try #1), and if it fails,
 * launching an application and trying (#2) to connect to it
 *
 */
static unsigned dde_connect(char* key, char* start, char* ddeexec,
                            const char* lpFile,
                            LPSHELLEXECUTEINFOA sei, BOOL is32)
{
    char*       endkey = key + strlen(key);
    char        app[256], topic[256], ifexec[256], res[256];
    LONG        applen, topiclen, ifexeclen;
    char*       exec;
    DWORD       ddeInst = 0;
    DWORD       tid;
    HSZ         hszApp, hszTopic;
    HCONV       hConv;
    unsigned    ret = 31;

    strcpy(endkey, "\\application");
    applen = sizeof(app);
    if (RegQueryValueA(HKEY_CLASSES_ROOT, key, app, &applen) != ERROR_SUCCESS)
    {
        FIXME("default app name NIY %s\n", key);
        return 2;
    }

    strcpy(endkey, "\\topic");
    topiclen = sizeof(topic);
    if (RegQueryValueA(HKEY_CLASSES_ROOT, key, topic, &topiclen) != ERROR_SUCCESS)
    {
        strcpy(topic, "System");
    }

    if (DdeInitializeA(&ddeInst, dde_cb, APPCMD_CLIENTONLY, 0L) != DMLERR_NO_ERROR)
    {
        return 2;
    }

    hszApp = DdeCreateStringHandleA(ddeInst, app, CP_WINANSI);
    hszTopic = DdeCreateStringHandleA(ddeInst, topic, CP_WINANSI);

    hConv = DdeConnect(ddeInst, hszApp, hszTopic, NULL);
    exec = ddeexec;
    if (!hConv)
    {
        TRACE("Launching '%s'\n", start);
        ret = SHELL_ExecuteA(start, sei, is32);
        if (ret < 32)
        {
            TRACE("Couldn't launch\n");
            goto error;
        }
        hConv = DdeConnect(ddeInst, hszApp, hszTopic, NULL);
        if (!hConv)
        {
            ret = 30; /* whatever */
            goto error;
        }
        strcpy(endkey, "\\ifexec");
        ifexeclen = sizeof(ifexec);
        if (RegQueryValueA(HKEY_CLASSES_ROOT, key, ifexec, &ifexeclen) == ERROR_SUCCESS)
        {
            exec = ifexec;
        }
    }

    argify(res, sizeof(res), exec, lpFile);
    TRACE("%s %s => %s\n", exec, lpFile, res);

    ret = (DdeClientTransaction(res, strlen(res) + 1, hConv, 0L, 0,
                                XTYP_EXECUTE, 10000, &tid) != DMLERR_NO_ERROR) ? 31 : 32;
    DdeDisconnect(hConv);
 error:
    DdeUninitialize(ddeInst);
    return ret;
}

/*************************************************************************
 *	execute_from_key [Internal]
 */
static HINSTANCE execute_from_key(LPCSTR _key, LPCSTR lpFile, LPSHELLEXECUTEINFOA sei, BOOL is32)
{
    char *      cmd = NULL;
    char *      key;
    int         keyLen = strlen(_key) + 1;
    LONG        cmdlen;
    HINSTANCE   retval = 31;
    
    
    key = (char *)HeapAlloc(GetProcessHeap(), 0, keyLen);
    
    if (key == NULL){
        ERR("could not allocate %d bytes for a copy of the key name\n", keyLen);
        
        return retval;
    }
    
    memcpy(key, _key, keyLen);
    

    /* Get the application for the registry.  Make sure the command buffer is at least
       MAX_PATH characters in size */
    if (getRegKeyValue(HKEY_CLASSES_ROOT, key, &cmd, &cmdlen, -MAX_PATH) == ERROR_SUCCESS)
    {
        LPSTR   tmp;
        char *  param = NULL;
        LONG    paramlen;


        /* Get the parameters needed by the application
           from the associated ddeexec key */
        tmp = strstr(key, "command");
        assert(tmp);
        strncpy(tmp, "ddeexec", keyLen - (tmp - key) - 1);
        key[keyLen - 1] = 0;


        if (getRegKeyValue(HKEY_CLASSES_ROOT, key, &param, &paramlen, 0) == ERROR_SUCCESS)
        {
            retval = dde_connect(key, cmd, param, lpFile, sei, is32);
            
            destroyRegKeyValue(param);
        }
        else
        {
            /* allocate enough space for the original command string, plus one copy 
               of the filename, plus some buffer space */
            paramlen = strlen(cmd) + strlen(lpFile) + MAX_PATH;
         
            
            /* Is there a replace() function anywhere? */
            param = HeapAlloc(GetProcessHeap(), 0, paramlen);
            
            if (param == NULL){
                ERR("could not allocate %ld bytes for the parameter buffer\n", paramlen);
                HeapFree(GetProcessHeap(), 0, key);
                
                return retval;
            }

            argify(param, paramlen, cmd, lpFile);

            TRACE("About to execute: '%s' using %s.\n", param,
                    (is32) ? "WinExec" : "WinExec16");

            retval = SHELL_ExecuteA(param, sei, is32);
            HeapFree(GetProcessHeap(), 0, param);
        }
        
        destroyRegKeyValue(cmd);
    }
    else TRACE("ooch\n");

    HeapFree(GetProcessHeap(), 0, key);    
 
    return retval;
}

/*************************************************************************
 * FindExecutableA			[SHELL32.@]
 */
HINSTANCE WINAPI FindExecutableA(LPCSTR lpFile, LPCSTR lpDirectory, LPSTR lpResult)
{
    HINSTANCE retval = 31;    /* default - 'No association was found' */
    char old_dir[1024];

    TRACE("File %s, Dir %s\n",
          (lpFile != NULL ? lpFile : "-"), (lpDirectory != NULL ? lpDirectory : "-"));

    lpResult[0] = '\0'; /* Start off with an empty return string */

    /* trap NULL parameters on entry */
    if ((lpFile == NULL) || (lpResult == NULL))
    {
        /* FIXME - should throw a warning, perhaps! */
	return 2; /* File not found. Close enough, I guess. */
    }

    if (lpDirectory)
    {
        GetCurrentDirectoryA(sizeof(old_dir), old_dir);
        SetCurrentDirectoryA(lpDirectory);
    }

    retval = SHELL_FindExecutable(lpDirectory, lpFile, "open", lpResult, NULL);

    TRACE("returning %s\n", lpResult);
    if (lpDirectory)
        SetCurrentDirectoryA(old_dir);
    return retval;
}

/*************************************************************************
 * FindExecutableW			[SHELL32.@]
 */
HINSTANCE WINAPI FindExecutableW(LPCWSTR lpFile, LPCWSTR lpDirectory, LPWSTR lpResult)
{
    FIXME("(%p,%p,%p): stub\n", lpFile, lpDirectory, lpResult);
    return 31;    /* default - 'No association was found' */
}

/*************************************************************************
 *	ShellExecuteExA32 [Internal]
 */
BOOL WINAPI ShellExecuteExA32 (LPSHELLEXECUTEINFOA sei, BOOL is32)
{
    CHAR *szApplicationName;
    CHAR *szCommandline;
    CHAR *szFinalCmdLine;
    char *fileName;
    char *cmd;
    int  sizeApplicationName;
    int  sizeCommandline;
    int  sizeFinalCmdLine;
    int  sizeFileName;
    int  sizecmd;
    char lpstrProtocol[256];
    CHAR szPidl[20];
    LPSTR pos;
    int gap, len;
    LPCSTR lpFile,lpOperation;
    HINSTANCE retval = 31;
    BOOL appname_quoted, done;

    /* allocate buffers */
    szApplicationName=NULL;
    szCommandline=NULL;
    szFinalCmdLine=NULL;
    fileName=NULL;
    cmd=NULL;
    sizeApplicationName=0;
    sizeCommandline=0;
    sizeFinalCmdLine=0;
    sizeFileName = 0;
    sizecmd=0;

    if (sei->lpFile)
      sizeApplicationName=max(strlen(sei->lpFile)+1, MAX_PATH);
    else
      sizeApplicationName=MAX_PATH;

    if (sei->lpParameters)
      sizeCommandline=max(strlen(sei->lpParameters)+1, MAX_PATH);
    else
      sizeCommandline=MAX_PATH;

    /* there's a possibility that the command line string will later be 
       appended to the application name.  Make sure there's enough room
       to accomodate it */
    sizeApplicationName += sizeCommandline + 1;

    szApplicationName=(CHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeApplicationName);
    if (szApplicationName==NULL)
    {
      ERR("malloc szApplicationName failed\n");
      retval = SE_ERR_OOM;
      goto FreeAndExit;
    }


    szCommandline=(CHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeCommandline);
    if (szCommandline==NULL)
    {
      ERR("malloc szCommandline failed\n");
      retval = SE_ERR_OOM;
      goto FreeAndExit;
    }  
    
    sizeFileName = max(sizeCommandline, sizeApplicationName);
    fileName=(char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeFileName);
    if (fileName==NULL)
    {
      ERR("malloc fileName failed\n");
      retval = SE_ERR_OOM;
      goto FreeAndExit;
    }

    sizeFinalCmdLine=sizeApplicationName+sizeCommandline+3;
    szFinalCmdLine=(CHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeFinalCmdLine);
    if (szFinalCmdLine==NULL)
    {
      ERR("malloc szFinalCmdLine failed\n");
      retval = SE_ERR_OOM;
      goto FreeAndExit;
    }

    sizecmd=sizeFinalCmdLine*2;
    cmd=(CHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizecmd);
    if (cmd==NULL)
    {
      ERR("malloc cmd failed\n");
      retval = SE_ERR_OOM;
      goto FreeAndExit;
    }
    /* allocate buffers done */

    TRACE("mask=0x%08lx hwnd=0x%04x verb=%s file=%s parm=%s dir=%s show=0x%08x class=%s\n",
            sei->fMask, sei->hwnd, debugstr_a(sei->lpVerb),
            debugstr_a(sei->lpFile), debugstr_a(sei->lpParameters),
            debugstr_a(sei->lpDirectory), sei->nShow,
            (sei->fMask & SEE_MASK_CLASSNAME) ? debugstr_a(sei->lpClass) : "not used");

    sei->hProcess = (HANDLE)NULL;
    ZeroMemory(szApplicationName,MAX_PATH);
    if (sei->lpFile)
        strcpy(szApplicationName, sei->lpFile);

    ZeroMemory(szCommandline,MAX_PATH);
    if (sei->lpParameters)
        strcpy(szCommandline, sei->lpParameters);

    if (sei->fMask & ((SEE_MASK_CLASSKEY & ~SEE_MASK_CLASSNAME) |
        SEE_MASK_INVOKEIDLIST | SEE_MASK_ICON | SEE_MASK_HOTKEY |
        SEE_MASK_CONNECTNETDRV | SEE_MASK_FLAG_DDEWAIT |
        SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_UNICODE |
        SEE_MASK_NO_CONSOLE | SEE_MASK_ASYNCOK | SEE_MASK_HMONITOR ))
    {
        FIXME("flags ignored: 0x%08lx\n", sei->fMask);
    }

    /* process the IDList */
    if ( (sei->fMask & SEE_MASK_INVOKEIDLIST) == SEE_MASK_INVOKEIDLIST) /*0x0c*/
    {
        SHGetPathFromIDListA (sei->lpIDList,szApplicationName);
        TRACE("-- idlist=%p (%s)\n", sei->lpIDList, szApplicationName);
    }
    else
    {
        if (sei->fMask & SEE_MASK_IDLIST )
        {
            pos = strstr(szCommandline, "%I");
            if (pos)
            {
                LPVOID pv;
                HGLOBAL hmem = SHAllocShared ( sei->lpIDList, ILGetSize(sei->lpIDList), 0);
                pv = SHLockShared(hmem,0);
                sprintf(szPidl,":%p",pv );
                SHUnlockShared(pv);

                gap = strlen(szPidl);
                len = strlen(pos)-2;
                memmove(pos+gap,pos+2,len);
                memcpy(pos,szPidl,gap);
            }
        }
    }

    if (sei->fMask & SEE_MASK_CLASSNAME)
    {
	/* launch a document by fileclass like 'WordPad.Document.1' */
        /* the Commandline contains 'c:\Path\wordpad.exe "%1"' */
        /* FIXME: szCommandline should not be of a fixed size. Plus MAX_PATH is way too short! */
        HCR_GetExecuteCommand(sei->lpClass, (sei->lpVerb) ? sei->lpVerb : "open", szCommandline, sizeof(szCommandline));
        /* FIXME: get the extension of lpFile, check if it fits to the lpClass */
        TRACE("SEE_MASK_CLASSNAME->'%s', doc->'%s'\n", szCommandline, szApplicationName);

        cmd[0] = '\0';
        done = argify(cmd, sizeof(cmd), szCommandline, szApplicationName);
        if (!done && szApplicationName[0])
        {
            strcat(cmd, " ");
            strcat(cmd, szApplicationName);
        }
        retval = SHELL_ExecuteA(cmd, sei, is32);
        goto FreeAndExit;
    }

    /* We set the default to open, and that should generally work.
       But that is not really the way the MS docs say to do it. */
    if (sei->lpVerb == NULL)
        lpOperation = "open";
    else
        lpOperation = sei->lpVerb;

    /* Else, try to execute the filename */
    TRACE("execute:'%s','%s'\n",szApplicationName, szCommandline);

    if ((szApplicationName[0] == '"') && (szApplicationName[strlen(szApplicationName)-1] == '"'))
        appname_quoted = TRUE;
    else
        appname_quoted = FALSE;


    strncpy(fileName, szApplicationName, sizeFileName);
    lpFile = fileName;
    ZeroMemory(szFinalCmdLine, sizeFinalCmdLine);
    if (!appname_quoted)
        szFinalCmdLine[0] = '"';
    strcat(szFinalCmdLine, szApplicationName);
    if (!appname_quoted)
        strcat(szFinalCmdLine, "\"");
        
    if (szCommandline[0]) {
        strncat(szApplicationName, " ", sizeApplicationName);
        strncat(szFinalCmdLine, " ", sizeFinalCmdLine);
        strncat(szApplicationName, szCommandline, sizeApplicationName);
        strncat(szFinalCmdLine, szCommandline, sizeFinalCmdLine);
    }


    retval = SHELL_ExecuteA(szFinalCmdLine, sei, is32);
    if (retval > 32)
      goto FreeAndExit;

    /* Else, try to find the executable */
    cmd[0] = '\0';
    retval = SHELL_FindExecutable(sei->lpDirectory, lpFile, lpOperation, cmd, lpstrProtocol);
    if (retval > 32)  /* Found */
    {
        if (szCommandline[0]) {
            strcat(cmd, " ");
            strcat(cmd, szCommandline);
        }
        TRACE("%s/%s => %s/%s\n", szApplicationName, lpOperation, cmd, lpstrProtocol);
        if (*lpstrProtocol)
            retval = execute_from_key(lpstrProtocol, szApplicationName, sei, is32);
        else
            retval = SHELL_ExecuteA(cmd, sei, is32);
    }
    else if (PathIsURLA((LPSTR)lpFile) ||
             (!strcasecmp (lpFile, "IEXPLORE.EXE") &&
              PathIsURLA (szCommandline)))

    {
        LPSTR lpstrRes;
        INT iSize;

        /* Handle attempts to directly invoke IE */
        if (!strcasecmp (lpFile, "IEXPLORE.EXE"))
           lpFile = szCommandline;

#ifdef __APPLE__
        HINSTANCE SDLDRV_hInstance = 0;
        SDLDRV_SwitchOutToBrowserURL_Func SDLDRV_SwitchOutToBrowserURL = NULL;

        TRACE("Got URL: %s\n", lpFile);
        if (sei->hwnd)
            FIXME("Opening URL in an window not supported\n");

        SDLDRV_hInstance = GetModuleHandleA("sdldrv.dll");
        if (SDLDRV_hInstance)
        {
            SDLDRV_SwitchOutToBrowserURL = (SDLDRV_SwitchOutToBrowserURL_Func)GetProcAddress(SDLDRV_hInstance, "SDLDRV_SwitchOutToBrowserURL");
            if (SDLDRV_SwitchOutToBrowserURL)
            {
                SDLDRV_SwitchOutToBrowserURL((char*)lpFile);
                retval=33;
                goto FreeAndExit;
            }
            else
                ERR("Failed to get SDLDRV_SwitchOutToBrowserURL from sdldrv.dll\n");
        }
        else
            ERR("Failed to load sdldrv.dll\n");
#endif

        lpstrRes = strchr(lpFile, ':');
        /* PathIsURLA probably should fail on strings without a ':', but it doesn't */
        if (lpstrRes)
            iSize = lpstrRes - lpFile;
        else
            iSize = strlen(lpFile);

        TRACE("Got URL: %s\n", lpFile);
        /* Looking for ...protocol\shell\lpOperation\command */
        strncpy(lpstrProtocol, lpFile, iSize);
        lpstrProtocol[iSize] = '\0';
        strcat(lpstrProtocol, "\\shell\\");
        strcat(lpstrProtocol, lpOperation);
        strcat(lpstrProtocol, "\\command");

        /* Remove File Protocol from lpFile */
        /* In the case file://path/file     */
        if (!strncasecmp(lpFile, "file", iSize))
        {
            lpFile += iSize;
            while (*lpFile == ':') lpFile++;
        }
        retval = execute_from_key(lpstrProtocol, lpFile, sei, is32);
    }
    /* Check if file specified is in the form www.??????.*** */
    else if (!strncasecmp(lpFile, "www", 3))
    {
        /* if so, append lpFile http:// and call ShellExecute */
        char lpstrTmpFile[256] = "http://" ;
        strcat(lpstrTmpFile, lpFile);
        retval = ShellExecuteA(sei->hwnd, lpOperation, lpstrTmpFile, NULL, NULL, 0);
    }

FreeAndExit:
    /* free buffers */
    if (szApplicationName)
      HeapFree( GetProcessHeap(), 0, szApplicationName );
    if (szCommandline)
      HeapFree( GetProcessHeap(), 0, szCommandline );
    if (szFinalCmdLine)
      HeapFree( GetProcessHeap(), 0, szFinalCmdLine );
    if (fileName)
      HeapFree( GetProcessHeap(), 0, fileName );
    if (cmd)
      HeapFree( GetProcessHeap(), 0, cmd );

    if (retval <= 32)
    {
        sei->hInstApp = retval;
        return FALSE;
    }

    sei->hInstApp = 33;
    return TRUE;
}


/*************************************************************************
 *				ShellExecute		[SHELL.20]
 */
HINSTANCE16 WINAPI ShellExecute16( HWND16 hWnd, LPCSTR lpOperation,
                                   LPCSTR lpFile, LPCSTR lpParameters,
                                   LPCSTR lpDirectory, INT16 iShowCmd )
{
    SHELLEXECUTEINFOA sei;
    HANDLE hProcess = 0;

    sei.cbSize = sizeof(sei);
    sei.fMask = 0;
    sei.hwnd = hWnd;
    sei.lpVerb = lpOperation;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = iShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = hProcess;

    ShellExecuteExA32 (&sei, FALSE);
    return (HINSTANCE16)sei.hInstApp;
}

/*************************************************************************
 * ShellExecuteA			[SHELL32.290]
 */
HINSTANCE WINAPI ShellExecuteA(HWND hWnd, LPCSTR lpOperation,LPCSTR lpFile,
                               LPCSTR lpParameters,LPCSTR lpDirectory, INT iShowCmd)
{
    SHELLEXECUTEINFOA sei;
    HANDLE hProcess = 0;

    TRACE("\n");
    sei.cbSize = sizeof(sei);
    sei.fMask = 0;
    sei.hwnd = hWnd;
    sei.lpVerb = lpOperation;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = iShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = hProcess;

    ShellExecuteExA32 (&sei, TRUE);
    return sei.hInstApp;
}

/*************************************************************************
 * ShellExecuteEx				[SHELL32.291]
 *
 */
BOOL WINAPI ShellExecuteExAW (LPVOID sei)
{
    if (SHELL_OsIsUnicode())
	return ShellExecuteExW (sei);
    return ShellExecuteExA32 (sei, TRUE);
}

/*************************************************************************
 * ShellExecuteExA				[SHELL32.292]
 *
 */
BOOL WINAPI ShellExecuteExA (LPSHELLEXECUTEINFOA sei)
{
    return  ShellExecuteExA32 (sei, TRUE);
}

/*************************************************************************
 * ShellExecuteExW				[SHELL32.293]
 *
 */
BOOL WINAPI ShellExecuteExW (LPSHELLEXECUTEINFOW sei)
{
    SHELLEXECUTEINFOA seiA;
    DWORD ret;

    TRACE("%p\n", sei);

    memcpy(&seiA, sei, sizeof(SHELLEXECUTEINFOA));

    if (sei->lpVerb)
        seiA.lpVerb = HEAP_strdupWtoA( GetProcessHeap(), 0, sei->lpVerb);

    if (sei->lpFile)
        seiA.lpFile = FILE_strdupWtoA( GetProcessHeap(), 0, sei->lpFile);

    if (sei->lpParameters)
        seiA.lpParameters = HEAP_strdupWtoA( GetProcessHeap(), 0, sei->lpParameters);

    if (sei->lpDirectory)
        seiA.lpDirectory = FILE_strdupWtoA( GetProcessHeap(), 0, sei->lpDirectory);

    if ((sei->fMask & SEE_MASK_CLASSNAME) && sei->lpClass)
        seiA.lpClass = HEAP_strdupWtoA( GetProcessHeap(), 0, sei->lpClass);
    else
        seiA.lpClass = NULL;

    ret = ShellExecuteExA(&seiA);

    if (seiA.lpVerb)	HeapFree( GetProcessHeap(), 0, (LPSTR) seiA.lpVerb );
    if (seiA.lpFile)	HeapFree( GetProcessHeap(), 0, (LPSTR) seiA.lpFile );
    if (seiA.lpParameters)	HeapFree( GetProcessHeap(), 0, (LPSTR) seiA.lpParameters );
    if (seiA.lpDirectory)	HeapFree( GetProcessHeap(), 0, (LPSTR) seiA.lpDirectory );
    if (seiA.lpClass)	HeapFree( GetProcessHeap(), 0, (LPSTR) seiA.lpClass );

    return ret;
}

/*************************************************************************
 * ShellExecuteW			[SHELL32.294]
 * from shellapi.h
 * WINSHELLAPI HINSTANCE APIENTRY ShellExecuteW(HWND hwnd, LPCWSTR lpOperation,
 * LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd);
 */
HINSTANCE WINAPI ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile,
                               LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd)
{
    SHELLEXECUTEINFOW sei;
    HANDLE hProcess = 0;

    TRACE("\n");
    sei.cbSize = sizeof(sei);
    sei.fMask = 0;
    sei.hwnd = hwnd;
    sei.lpVerb = lpOperation;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = nShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = hProcess;

    ShellExecuteExW (&sei);
    return sei.hInstApp;
}
