/*
 * Helper functions for debugging
 *
 * Copyright 1998, 2002 Juergen Schmied
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pidl.h"
#include "shlguid.h"
#include "wine/debug.h"
#include "debughlp.h"


WINE_DEFAULT_DEBUG_CHANNEL(pidl);

LPITEMIDLIST  _dbg_ILGetNext(LPITEMIDLIST pidl)
{
	WORD len;

	if(pidl)
	{
	  len =  pidl->mkid.cb;
	  if (len)
	  {
	    pidl = (LPITEMIDLIST) (((LPBYTE)pidl)+len);
	    return pidl;
	  }
	}
	return NULL;
}

BOOL _dbg_ILIsDesktop(LPCITEMIDLIST pidl)
{
	return ( !pidl || (pidl && pidl->mkid.cb == 0x00) );
}

LPPIDLDATA _dbg_ILGetDataPointer(LPITEMIDLIST pidl)
{
	if(pidl && pidl->mkid.cb != 0x00)
	  return (LPPIDLDATA) &(pidl->mkid.abID);
	return NULL;
}

LPSTR _dbg_ILGetTextPointer(LPCITEMIDLIST pidl)
{
	LPPIDLDATA pdata =_dbg_ILGetDataPointer(pidl);

	if (pdata)
	{
	  switch (pdata->type)
	  {
	    case PT_MYCOMP:
	    case PT_SPECIAL:
	      return NULL;

	    case PT_DRIVE:
	    case PT_DRIVE1:
	    case PT_DRIVE2:
	    case PT_DRIVE3:
	      return (LPSTR)&(pdata->u.drive.szDriveName);

	    case PT_FOLDER:
	    case PT_FOLDER1:
	    case PT_VALUE:
	    case PT_IESPECIAL1:
	    case PT_IESPECIAL2:
	      return (LPSTR)&(pdata->u.file.szNames);

	    case PT_WORKGRP:
	    case PT_COMP:
	    case PT_NETWORK:
	    case PT_SHARE:
	      return (LPSTR)&(pdata->u.network.szNames);
	  }
	}
	return NULL;
}

LPSTR _dbg_ILGetSTextPointer(LPCITEMIDLIST pidl)
{
	LPPIDLDATA pdata =_dbg_ILGetDataPointer(pidl);

	if (pdata)
	{
	  switch (pdata->type)
	  {
	    case PT_FOLDER:
	    case PT_VALUE:
	    case PT_IESPECIAL1:
	    case PT_IESPECIAL2:
	      return (LPSTR)(pdata->u.file.szNames + strlen (pdata->u.file.szNames) + 1);

	    case PT_WORKGRP:
	      return (LPSTR)(pdata->u.network.szNames + strlen (pdata->u.network.szNames) + 1);
	  }
	}
	return NULL;
}

REFIID _dbg_ILGetGUIDPointer(LPCITEMIDLIST pidl)
{
	LPPIDLDATA pdata =_ILGetDataPointer(pidl);

	if (pdata)
	{
	  switch (pdata->type)
	  {
	    case PT_SPECIAL:
	    case PT_MYCOMP:
	      return (REFIID) &(pdata->u.mycomp.guid);

	    default:
		TRACE("Unknown pidl type 0x%04x\n", pdata->type);
		break;
	  }
	}
	return NULL;
}

DWORD _dbg_ILSimpleGetText (LPCITEMIDLIST pidl, LPSTR szOut, UINT uOutSize)
{
	DWORD		dwReturn=0;
	LPSTR		szSrc;
	GUID const * 	riid;
	char szTemp[MAX_PATH];

	if (!pidl) return 0;

	if (szOut)
	  *szOut = 0;

	if (_dbg_ILIsDesktop(pidl))
	{
	 /* desktop */
	  if (szOut) strncpy(szOut, "Desktop", uOutSize);
	  dwReturn = strlen ("Desktop");
	}
	else if (( szSrc = _dbg_ILGetTextPointer(pidl) ))
	{
	  /* filesystem */
	  if (szOut) strncpy(szOut, szSrc, uOutSize);
	  dwReturn = strlen(szSrc);
	}
	else if (( riid = _dbg_ILGetGUIDPointer(pidl) ))
	{
	  if (szOut)
            sprintf( szOut, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 riid->Data1, riid->Data2, riid->Data3,
                 riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3],
                 riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7] );
	  dwReturn = strlen (szTemp);
	}
	else
	{
	  ERR("-- no text\n");
	}
	return dwReturn;
}




void pdump (LPCITEMIDLIST pidl)
{
	LPITEMIDLIST pidltemp = pidl;

	if (!TRACE_ON(pidl)) return;

	if (! pidltemp)
	{
	  MESSAGE ("-------- pidl=NULL (Desktop)\n");
	}
	else
	{
	  MESSAGE ("-------- pidl=%p\n", pidl);
	  if (pidltemp->mkid.cb)
	  {
	    do
	    {
	      DWORD dwAttrib = 0;
	      LPPIDLDATA pData   = _dbg_ILGetDataPointer(pidltemp);
	      DWORD type         = pData->type;
	      LPSTR szLongName   = _dbg_ILGetTextPointer(pidltemp);
	      LPSTR szShortName  = _dbg_ILGetSTextPointer(pidltemp);
	      char szName[MAX_PATH];

	      _dbg_ILSimpleGetText(pidltemp, szName, MAX_PATH);
	      if( PT_FOLDER == type)
	        dwAttrib = pData->u.folder.uFileAttribs;
	      else if( PT_VALUE == type)
	        dwAttrib = pData->u.file.uFileAttribs;

	      MESSAGE ("-- pidl=%p size=%u type=%lx attr=0x%08lx name=%s (%s,%s)\n",
	               pidltemp, pidltemp->mkid.cb,type,dwAttrib,szName,debugstr_a(szLongName), debugstr_a(szShortName));

	      pidltemp = _dbg_ILGetNext(pidltemp);

	    } while (pidltemp->mkid.cb);
	  }
	  else
	  {
	    MESSAGE ("empty pidl (Desktop)\n");
	  }
	  pcheck(pidl);
	}
}
#define BYTES_PRINTED 32
BOOL pcheck (LPCITEMIDLIST pidl)
{
        DWORD type, ret=TRUE;
        LPITEMIDLIST pidltemp = pidl;

        if (pidltemp && pidltemp->mkid.cb)
        { do
          { type   = _dbg_ILGetDataPointer(pidltemp)->type;
            switch (type)
	    { case PT_DESKTOP:
	      case PT_MYCOMP:
	      case PT_SPECIAL:
	      case PT_DRIVE:
	      case PT_DRIVE1:
	      case PT_DRIVE2:
	      case PT_DRIVE3:
	      case PT_FOLDER:
	      case PT_VALUE:
	      case PT_FOLDER1:
	      case PT_WORKGRP:
	      case PT_COMP:
	      case PT_NETWORK:
	      case PT_IESPECIAL1:
	      case PT_IESPECIAL2:
	      case PT_SHARE:
		break;
	      default:
	      {
		char szTemp[BYTES_PRINTED*4 + 1];
		int i;
		unsigned char c;

		memset(szTemp, ' ', BYTES_PRINTED*4 + 1);
		for ( i = 0; (i<pidltemp->mkid.cb) && (i<BYTES_PRINTED); i++)
		{
		  c = ((unsigned char *)pidltemp)[i];

		  szTemp[i*3+0] = ((c>>4)>9)? (c>>4)+55 : (c>>4)+48;
		  szTemp[i*3+1] = ((0x0F&c)>9)? (0x0F&c)+55 : (0x0F&c)+48;
		  szTemp[i*3+2] = ' ';
		  szTemp[i+BYTES_PRINTED*3]  =  (c>=0x20 && c <=0x80) ? c : '.';
		}
		szTemp[BYTES_PRINTED*4] = 0x00;
		ERR("unknown IDLIST type size=%u type=%lx\n%s\n",pidltemp->mkid.cb,type, szTemp);
		ret = FALSE;
	      }
	    }
	    pidltemp = _dbg_ILGetNext(pidltemp);
	  } while (pidltemp->mkid.cb);
	}
	return ret;
}
