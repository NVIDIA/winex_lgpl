/*
 * internal pidl functions
 * 1998 <juergen.schmied@metronet.de>
 *
 * DO NOT use this definitions outside the shell32.dll !
 *
 * The contents of a pidl should never used from a application
 * directly.
 *
 * Undocumented:
 * MS says: the abID of SHITEMID should be treated as binary data and not
 * be interpreted by applications. Applies to everyone but MS itself.
 * Word95 interprets the contents of abID (Filesize/Date) so we have to go
 * for binary compatibility here.
 */

#ifndef __WINE_PIDL_H
#define __WINE_PIDL_H

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "shlobj.h"

/*
* the pidl does cache fileattributes to speed up SHGetAttributes when
* displaying a big number of files.
*
* a pidl of NULL means the desktop
*
* The structure of the pidl seems to be a union. The first byte of the
* PIDLDATA desribes the type of pidl.
*
*	object        ! first byte /  ! format       ! living space
*	              ! size
*	----------------------------------------------------------------
*	my computer	0x1F/20		mycomp (2)	(usual)
*	network		0x1F		mycomp
*	bitbucket	0x1F		mycomp
*	drive		0x23/25		drive		(usual)
*	drive		0x25/25		drive		(lnk/persistant)
*	drive		0x29/25		drive
*	shell extension	0x2E		mycomp
*	drive		0x2F		drive		(lnk/persistant)
*	folder/file	0x30		folder/file (1)	(lnk/persistant)
*	folder		0x31		folder		(usual)
*	value		0x32		file		(usual)
*	workgroup	0x41		network (3)
*	computer	0x42		network (4)
*	whole network	0x47		network (5)
*	MSITStore	0x61		htmlhlp (7)
*	history/favorites 0xb1		file
*	share		0xc3		network (6)
*
* guess: the persistant elements are non tracking
*
* (1) dummy byte is used, attributes are empty
* (2) IID_MyComputer = 20D04FE0L-3AEA-1069-A2D8-08002B30309D
* (3) two strings	"workgroup" "microsoft network"
* (4) one string	"\\sirius"
* (5) one string	"whole network"
* (6) one string	"\\sirius\c"
* (7) contains string   "mk:@MSITStore:C:\path\file.chm::/path/filename.htm"
*		GUID	871C5380-42A0-1069-A2EA-08002B30309D
*/

#define PT_DESKTOP	0x00 /* internal */
#define PT_MYCOMP	0x1F
#define PT_DRIVE	0x23
#define PT_DRIVE2	0x25
#define PT_DRIVE3	0x29
#define PT_SPECIAL	0x2E
#define PT_DRIVE1	0x2F
#define PT_FOLDER1	0x30
#define PT_FOLDER	0x31
#define PT_VALUE	0x32
#define PT_WORKGRP	0x41
#define PT_COMP		0x42
#define PT_NETWORK	0x47
#define PT_IESPECIAL1	0x61
#define PT_IESPECIAL2	0xb1
#define PT_SHARE	0xc3

#include "pshpack1.h"
typedef BYTE PIDLTYPE;

typedef struct tagPIDLDATA
{	PIDLTYPE type;			/*00*/
	union
	{ struct
	  { BYTE dummy;			/*01*/
	    GUID guid;			/*02*/
	    BYTE dummy1;		/*18*/
	  } mycomp;
	  struct
	  { CHAR szDriveName[20];	/*01*/
	    DWORD dwUnknown;		/*21*/
	    /* the drive seems to be 25 bytes every time */
	  } drive;
	  struct
	  { BYTE dummy;			/*01 is 0x00 for files or dirs */
	    DWORD dwFileSize;		/*02*/
	    WORD uFileDate;		/*06*/
	    WORD uFileTime;		/*08*/
	    WORD uFileAttribs;		/*10*/
	    CHAR szNames[1];		/*12*/
	    /* Here are comming two strings. The first is the long name.
	    The second the dos name when needed or just 0x00 */
	  } file, folder, generic;
	  struct
	  { WORD dummy;		/*01*/
	    CHAR szNames[1];	/*03*/
	  } network;
	  struct
	  { WORD dummy;		/*01*/
	    DWORD dummy1;	/*02*/
	    CHAR szName[1];	/*06*/ /* teminated by 0x00 0x00 */
	  } htmlhelp;
	}u;
} PIDLDATA, *LPPIDLDATA;
#include "poppack.h"

/*
 * getting special values from simple pidls
 */
DWORD	_ILSimpleGetText	(LPCITEMIDLIST pidl, LPSTR pOut, UINT uOutSize);
BOOL	_ILGetFileDate 		(LPCITEMIDLIST pidl, LPSTR pOut, UINT uOutSize);
DWORD	_ILGetFileSize		(LPCITEMIDLIST pidl, LPSTR pOut, UINT uOutSize);
BOOL	_ILGetExtension		(LPCITEMIDLIST pidl, LPSTR pOut, UINT uOutSize);
void	_ILGetFileType		(LPCITEMIDLIST pidl, LPSTR pOut, UINT uOutSize);
DWORD	_ILGetFileAttributes	(LPCITEMIDLIST pidl, LPSTR pOut, UINT uOutSize);

BOOL	_ILGetFileDateTime	(LPCITEMIDLIST pidl, FILETIME *ft);
DWORD	_ILGetDrive		(LPCITEMIDLIST, LPSTR, UINT);

/*
 * testing simple pidls
 */
BOOL	_ILIsDesktop		(LPCITEMIDLIST pidl);
BOOL	_ILIsMyComputer		(LPCITEMIDLIST pidl);
BOOL	_ILIsDrive		(LPCITEMIDLIST pidl);
BOOL	_ILIsFolder		(LPCITEMIDLIST pidl);
BOOL	_ILIsValue		(LPCITEMIDLIST pidl);
BOOL	_ILIsSpecialFolder	(LPCITEMIDLIST pidl);
BOOL	_ILIsPidlSimple		(LPCITEMIDLIST pidl);

/*
 * simple pidls from strings
 */
LPITEMIDLIST	_ILCreate	(PIDLTYPE,LPCVOID,UINT);

LPITEMIDLIST	_ILCreateDesktop	(void);
LPITEMIDLIST	_ILCreateMyComputer	(void);
LPITEMIDLIST	_ILCreateIExplore	(void);
LPITEMIDLIST	_ILCreateControl	(void);
LPITEMIDLIST	_ILCreatePrinter	(void);
LPITEMIDLIST	_ILCreateNetwork	(void);
LPITEMIDLIST	_ILCreateBitBucket	(void);
LPITEMIDLIST	_ILCreateDrive		(LPCSTR);
LPITEMIDLIST	_ILCreateFolder		(WIN32_FIND_DATAA * stffile);
LPITEMIDLIST	_ILCreateValue		(WIN32_FIND_DATAA * stffile);
LPITEMIDLIST	_ILCreateSpecial	(LPCSTR szGUID);

/*
 * helper functions (getting struct-pointer)
 */
LPPIDLDATA	_ILGetDataPointer	(LPCITEMIDLIST);
LPSTR		_ILGetTextPointer	(LPCITEMIDLIST);
LPSTR		_ILGetSTextPointer	(LPCITEMIDLIST);
REFIID		_ILGetGUIDPointer	(LPCITEMIDLIST pidl);

/*
 * debug helper
 */
void	pdump	(LPCITEMIDLIST pidl);
BOOL	pcheck	(LPCITEMIDLIST pidl);

/*
 * aPidl helper
 */
void _ILFreeaPidl(LPITEMIDLIST * apidl, UINT cidl);
LPITEMIDLIST * _ILCopyaPidl(LPITEMIDLIST * apidlsrc, UINT cidl);
LPITEMIDLIST * _ILCopyCidaToaPidl(LPITEMIDLIST* pidl, LPCIDA cida);

#endif
