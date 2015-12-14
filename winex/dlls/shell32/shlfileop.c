/*
 * SHFileOperation
 */
#include <string.h>

#include "winreg.h"
#include "winbase.h"
#include "winuser.h"
#include "heap.h"
#include "wine/file.h"
#include "shellapi.h"
#include "shlobj.h"
#include "shresdef.h"
#include "shell32_main.h"
#include "undocshell.h"
#include "shlwapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

BOOL SHELL_WarnItemDelete (int nKindOfDialog, LPCSTR szDir)
{
	char szCaption[255], szText[255], szBuffer[MAX_PATH + 256];

        if(nKindOfDialog == ASK_DELETE_FILE)
        {
	  LoadStringA(shell32_hInstance, IDS_DELETEITEM_TEXT, szText,
		sizeof(szText));
	  LoadStringA(shell32_hInstance, IDS_DELETEITEM_CAPTION,
		szCaption, sizeof(szCaption));
	}
        else if(nKindOfDialog == ASK_DELETE_FOLDER)
        {
	  LoadStringA(shell32_hInstance, IDS_DELETEITEM_TEXT, szText,
		sizeof(szText));
	  LoadStringA(shell32_hInstance, IDS_DELETEFOLDER_CAPTION,
		szCaption, sizeof(szCaption));
        }
        else if(nKindOfDialog == ASK_DELETE_MULTIPLE_ITEM)
        {
	  LoadStringA(shell32_hInstance, IDS_DELETEMULTIPLE_TEXT, szText,
		sizeof(szText));
	  LoadStringA(shell32_hInstance, IDS_DELETEITEM_CAPTION,
		szCaption, sizeof(szCaption));
        }
	else {
          FIXME("Called without a valid nKindOfDialog specified!\n");
	  LoadStringA(shell32_hInstance, IDS_DELETEITEM_TEXT, szText,
		sizeof(szText));
	  LoadStringA(shell32_hInstance, IDS_DELETEITEM_CAPTION,
		szCaption, sizeof(szCaption));
	}

	FormatMessageA(FORMAT_MESSAGE_FROM_STRING|FORMAT_MESSAGE_ARGUMENT_ARRAY,
	    szText, 0, 0, szBuffer, sizeof(szBuffer), (va_list*)&szDir);

	return (IDOK == MessageBoxA(GetActiveWindow(), szBuffer, szCaption, MB_OKCANCEL | MB_ICONEXCLAMATION));
}

/**************************************************************************
 *	SHELL_DeleteDirectoryA()
 *
 * like rm -r
 */

BOOL SHELL_DeleteDirectoryA(LPCSTR pszDir, BOOL bShowUI)
{
	BOOL		ret = FALSE;
	HANDLE		hFind;
	WIN32_FIND_DATAA wfd;
	char		szTemp[MAX_PATH];

	strcpy(szTemp, pszDir);
	PathAddBackslashA(szTemp);
	strcat(szTemp, "*.*");

	if (bShowUI && !SHELL_WarnItemDelete(ASK_DELETE_FOLDER, pszDir))
	  return FALSE;

	if(INVALID_HANDLE_VALUE != (hFind = FindFirstFileA(szTemp, &wfd)))
	{
	  do
	  {
	    if(strcasecmp(wfd.cFileName, ".") && strcasecmp(wfd.cFileName, ".."))
	    {
	      strcpy(szTemp, pszDir);
	      PathAddBackslashA(szTemp);
	      strcat(szTemp, wfd.cFileName);

	      if(FILE_ATTRIBUTE_DIRECTORY & wfd.dwFileAttributes)
	        SHELL_DeleteDirectoryA(szTemp, FALSE);
	      else
	        DeleteFileA(szTemp);
	    }
	  } while(FindNextFileA(hFind, &wfd));

	  FindClose(hFind);
	  ret = RemoveDirectoryA(pszDir);
	}

	return ret;
}

/**************************************************************************
 *	SHELL_DeleteFileA()
 */

BOOL SHELL_DeleteFileA(LPCSTR pszFile, BOOL bShowUI)
{
	if (bShowUI && !SHELL_WarnItemDelete(ASK_DELETE_FILE, pszFile))
		return FALSE;

        return DeleteFileA(pszFile);
}

/*************************************************************************
 * SHCreateDirectory				[SHELL32.165]
 *
 * NOTES
 *  exported by ordinal
 *  Seems to be Unicode on Win2k/XP, but may not be on Win 9x, so we'll 
 *  just forward to the appropriate version of SHCreateDirectoryEx
 */
int WINAPI SHCreateDirectory(HWND hwnd, LPCVOID path)
{
	if (SHELL_OsIsUnicode())
	  return SHCreateDirectoryExW(hwnd, path, NULL);
	return SHCreateDirectoryExA(hwnd, path, NULL);
}

/***************************************************************************
 * SHCreateDirectoryEx
 *
 * Creates a new file system folder, with the given security attributes.
 * Creates any intermediate folders as necessary.
 *
 * Returns: 
 *   ERROR_SUCCESS if successful. 
 *   If the operation fails, other system error codes can be returned.
 *
 * TODO: not fully implemented, hwnd isn't used if the user needs to be notified.
 *       
 * Both Unicode and ANSI versions 
 */
int WINAPI SHCreateDirectoryExA(HWND hwnd, LPCSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
	int ret;

	TRACE("(%08x,%s,%p)\n", hwnd, debugstr_a(pszPath), psa);

	/* bail out if given relative path */
	if( PathIsRelativeA(pszPath) ) 
	{
	  ret = ERROR_BAD_PATHNAME;
	} 
	/* try to create the given folder */
	else if (CreateDirectoryA(pszPath, psa))
	{
	  SHChangeNotifyA(SHCNE_MKDIR, SHCNF_PATHA, pszPath, NULL);
	  ret = ERROR_SUCCESS;
	} 
	else 
	{
	  ret = GetLastError();

	  /* it failed, check if we need to make intermediate paths */
	  if( ret == ERROR_PATH_NOT_FOUND ) 
	  {
	    char tempPath[MAX_PATH+1];
	    char *slash;
	    int len;

	    len = min(strlen(pszPath), MAX_PATH);
	    strncpy(tempPath, pszPath, len+1);

	    /* ensure we've got a trailing "\\" on the path and null-terminated */
	    if( tempPath[len-1] != '\\')
	    {
	      tempPath[len++] = '\\';
	    }
	    tempPath[len] = '\0';

	    /* skip the part before the first "\\", since this is a fully qualified path */
	    slash = strchr(tempPath, '\\');

	    /* find the next slash, replace with '\0', create directory, restore slash, repeat */
	    while( slash ) 
	    {
	      slash = strchr(slash+1, '\\');

	      if( slash ) 
	      {
		*slash = '\0';
		
		/* create the next directory in path */
		if( CreateDirectoryA( tempPath, psa ) ) 
		{
		  SHChangeNotifyA(SHCNE_MKDIR, SHCNF_PATHA, tempPath, NULL);
		  ret = ERROR_SUCCESS;
		} else {
		  ret = GetLastError();
		  /* some of the directories along the way may exist so we should continue */
		  if( ret != ERROR_FILE_EXISTS && ret != ERROR_ALREADY_EXISTS ) {
		    TRACE("error creating intermediate folders ret=%d\n", ret);
		    break;
		  }
		}
		*slash = '\\';
	      }
	    }
	  }
	    
	}
  	return ret;
}

int WINAPI SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
	int ret;
	LPSTR pathA;
	TRACE("(%08x,%s,%p)\n", hwnd, debugstr_w(pszPath), psa);

	pathA = FILE_strdupWtoA( GetProcessHeap(), 0, pszPath );
	ret = SHCreateDirectoryExA( hwnd, pathA, psa );
	HeapFree( GetProcessHeap(), 0, pathA );
	return ret;
}

/************************************************************************
 *      Win32DeleteFile                         [SHELL32.164]
 *
 * Deletes a file.  Also triggers a change notify if one exists.
 *
 * FIXME:
 * Verified on Win98 / IE 5 (SHELL32 4.72, March 1999 build) to be
 * ANSI.  Is this Unicode on NT?
 *
 */

BOOL WINAPI Win32DeleteFile(LPSTR fName)
{
	TRACE("%p(%s)\n", fName, fName);

	DeleteFileA(fName);
	SHChangeNotifyA(SHCNE_DELETE, SHCNF_PATHA, fName, NULL);
	return TRUE;
}

/*************************************************************************
 * SHFileOperationA				[SHELL32.@]
 *
 * NOTES
 *     exported by name
 */
DWORD WINAPI SHFileOperationA (LPSHFILEOPSTRUCTA lpFileOp)
{
	LPSTR pFrom = (LPSTR)lpFileOp->pFrom;
	LPSTR pTo = (LPSTR)lpFileOp->pTo;
	LPSTR pTempTo;
        TRACE("flags (0x%04x) : %s%s%s%s%s%s%s%s%s%s%s%s \n", lpFileOp->fFlags,
                lpFileOp->fFlags & FOF_MULTIDESTFILES ? "FOF_MULTIDESTFILES " : "",
                lpFileOp->fFlags & FOF_CONFIRMMOUSE ? "FOF_CONFIRMMOUSE " : "",
                lpFileOp->fFlags & FOF_SILENT ? "FOF_SILENT " : "",
                lpFileOp->fFlags & FOF_RENAMEONCOLLISION ? "FOF_RENAMEONCOLLISION " : "",
                lpFileOp->fFlags & FOF_NOCONFIRMATION ? "FOF_NOCONFIRMATION " : "",
                lpFileOp->fFlags & FOF_WANTMAPPINGHANDLE ? "FOF_WANTMAPPINGHANDLE " : "",
                lpFileOp->fFlags & FOF_ALLOWUNDO ? "FOF_ALLOWUNDO " : "",
                lpFileOp->fFlags & FOF_FILESONLY ? "FOF_FILESONLY " : "",
                lpFileOp->fFlags & FOF_SIMPLEPROGRESS ? "FOF_SIMPLEPROGRESS " : "",
                lpFileOp->fFlags & FOF_NOCONFIRMMKDIR ? "FOF_NOCONFIRMMKDIR " : "",
                lpFileOp->fFlags & FOF_NOERRORUI ? "FOF_NOERRORUI " : "",
                lpFileOp->fFlags & 0xf800 ? "MORE-UNKNOWN-Flags" : "");
	switch(lpFileOp->wFunc) {
	case FO_COPY: {
                /* establish when pTo is interpreted as the name of the destination file
                 * or the directory where the Fromfile should be copied to.
                 * This depends on:
                 * (1) pTo points to the name of an existing directory;
                 * (2) the flag FOF_MULTIDESTFILES is present;
                 * (3) whether pFrom point to multiple filenames.
                 *
                 * Some experiments:
                 *
                 * destisdir               1 1 1 1 0 0 0 0
                 * FOF_MULTIDESTFILES      1 1 0 0 1 1 0 0
                 * multiple from filenames 1 0 1 0 1 0 1 0
                 *                         ---------------
                 * copy files to dir       1 0 1 1 0 0 1 0
                 * create dir              0 0 0 0 0 0 1 0
                 */
                int multifrom = pFrom[strlen(pFrom) + 1] != '\0';
                int destisdir = PathIsDirectoryA( pTo );
                int copytodir = 0;
		TRACE("File Copy:\n");
                if( destisdir ) {
                    if ( !((lpFileOp->fFlags & FOF_MULTIDESTFILES) && !multifrom))
                        copytodir = 1;
                } else {
                    if ( !(lpFileOp->fFlags & FOF_MULTIDESTFILES) && multifrom)
                        copytodir = 1;
                }
                if ( copytodir ) {
                    char *fromfile;
                    int lenPTo;
                    if ( ! destisdir) {
                        TRACE("   creating directory %s\n",pTo);
                        SHCreateDirectoryExA((HWND)NULL, pTo, NULL);
                    }
                    lenPTo = strlen(pTo);
                    while(1) {
                        if(!pFrom[0]) break;
                        fromfile = PathFindFileNameA( pFrom);
                        pTempTo = HeapAlloc(GetProcessHeap(), 0, lenPTo + strlen(fromfile) + 2);
                        if (pTempTo) {
                            strcpy(pTempTo,pTo);
                            if(lenPTo && pTo[lenPTo] != '\\')
                                strcat(pTempTo,"\\");
                            strcat(pTempTo,fromfile);
                            TRACE("   From='%s' To='%s'\n", pFrom, pTempTo);
                            CopyFileA(pFrom, pTempTo, FALSE);
                            HeapFree(GetProcessHeap(), 0, pTempTo);
                        }
                        pFrom += strlen(pFrom) + 1;
                    }
                } else {
                    while(1) {
                            if(!pFrom[0]) break;
                            if(!pTo[0]) break;
                            TRACE("   From='%s' To='%s'\n", pFrom, pTo);

                            pTempTo = HeapAlloc(GetProcessHeap(), 0, strlen(pTo)+1);
                            if (pTempTo)
                            {
                                strcpy( pTempTo, pTo );
                                PathRemoveFileSpecA(pTempTo);
                                TRACE("   Creating Directory '%s'\n", pTempTo);
                                SHCreateDirectoryExA((HWND)NULL, pTempTo, NULL);
                                HeapFree(GetProcessHeap(), 0, pTempTo);
                            }
                            CopyFileA(pFrom, pTo, FALSE);

                            pFrom += strlen(pFrom) + 1;
                            pTo += strlen(pTo) + 1;
                    }
                }
		TRACE("Setting AnyOpsAborted=FALSE\n");
		lpFileOp->fAnyOperationsAborted=FALSE;
		return 0;
        }

	case FO_DELETE:
		TRACE("File Delete:\n");
		while(1) {
			if(!pFrom[0]) break;
			TRACE("   File='%s'\n", pFrom);
			DeleteFileA(pFrom);
			pFrom += strlen(pFrom) + 1;
		}
		TRACE("Setting AnyOpsAborted=FALSE\n");
		lpFileOp->fAnyOperationsAborted=FALSE;
		return 0;

	default:
		FIXME("Unhandled shell file operation %d\n", lpFileOp->wFunc);
	}

	return 1;
}

/*************************************************************************
 * SHFileOperationW				[SHELL32.@]
 *
 * NOTES
 *     exported by name
 */
DWORD WINAPI SHFileOperationW (LPSHFILEOPSTRUCTW lpFileOp)
{
	FIXME("(%p):stub.\n", lpFileOp);
	return 1;
}

/*************************************************************************
 * SHFileOperation				[SHELL32.@]
 *
 */
DWORD WINAPI SHFileOperationAW(LPVOID lpFileOp)
{
	if (SHELL_OsIsUnicode())
	  return SHFileOperationW(lpFileOp);
	return SHFileOperationA(lpFileOp);
}

/*************************************************************************
 * SheGetDirW [SHELL32.281]
 *
 */
HRESULT WINAPI SheGetDirW(LPWSTR u, LPWSTR v)
{	FIXME("%p %p stub\n",u,v);
	return 0;
}

/*************************************************************************
 * SheChangeDirW [SHELL32.274]
 *
 */
HRESULT WINAPI SheChangeDirW(LPWSTR u)
{	FIXME("(%s),stub\n",debugstr_w(u));
	return 0;
}

/*************************************************************************
 * IsNetDrive			[SHELL32.66]
 */
BOOL WINAPI IsNetDrive(DWORD drive)
{
	char root[4];
	strcpy(root, "A:\\");
	root[0] += drive;
	return (GetDriveTypeA(root) == DRIVE_REMOTE);
}
