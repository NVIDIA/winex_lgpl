/*
 *
 *	Copyright 1997	Marcus Meissner
 *	Copyright 1998	Juergen Schmied
 *
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include "wine/debug.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "winnls.h"
#include "winreg.h"

#include "shlobj.h"
#include "shobjidl.h"
#include "undocshell.h"
#include "bitmaps/wine.xpm"

#include "heap.h"
#include "wine/file.h"
#include "pidl.h"
#include "shell32_main.h"
#include "shlguid.h"

#ifdef HAVE_LIBPNG
# include <png.h>
#endif /* HAVE_LIBPNG */


WINE_DEFAULT_DEBUG_CHANNEL(shell);

/* link file formats */

#include "pshpack1.h"

/* flag1: lnk elements: simple link has 0x0B */
#define	WORKDIR		0x10
#define	ARGUMENT	0x20
#define	ICON		0x40
#define UNC		0x80

/* fStartup */
#define	NORMAL		0x01
#define	MAXIMIZED	0x03
#define	MINIMIZED	0x07

typedef struct _LINK_HEADER
{	DWORD	MagicStr;	/* 0x00 'L','\0','\0','\0' */
	GUID	MagicGuid;	/* 0x04 is CLSID_ShellLink */
	DWORD	Flag1;		/* 0x14 describes elements following */
	DWORD	Flag2;		/* 0x18 */
	FILETIME Time1;		/* 0x1c */
	FILETIME Time2;		/* 0x24 */
	FILETIME Time3;		/* 0x2c */
	DWORD	Unknown1;	/* 0x34 */
	DWORD	Unknown2;	/* 0x38 icon number */
	DWORD	fStartup;	/* 0x3c startup type */
	DWORD	wHotKey;	/* 0x40 hotkey */
	DWORD	Unknown5;	/* 0x44 */
	DWORD	Unknown6;	/* 0x48 */
	USHORT	PidlSize;	/* 0x4c */
	ITEMIDLIST Pidl;	/* 0x4e */
} LINK_HEADER, * PLINK_HEADER;

#define LINK_HEADER_SIZE (sizeof(LINK_HEADER)-sizeof(ITEMIDLIST))

typedef struct
{
	BYTE bWidth;
	BYTE bHeight;
	BYTE bColorCount;
	BYTE bReserved;
	WORD wPlanes;
	WORD wBitCount;
	DWORD dwBytesInRes;
	WORD nID;
} GRPICONDIRENTRY;

typedef struct
{
	WORD idReserved;
	WORD idType;
	WORD idCount;
	GRPICONDIRENTRY idEntries[1];
} GRPICONDIR;

typedef struct
{
	BYTE bWidth;
	BYTE bHeight;
	BYTE bColorCount;
	BYTE bReserved;
	WORD wPlanes;
	WORD wBitCount;
	DWORD dwBytesInRes;
	DWORD dwImageOffset;
} ICONDIRENTRY;

typedef struct
{
	WORD idReserved;
	WORD idType;
	WORD idCount;
} ICONDIR;


#include "poppack.h"

/* Simple linked list of icons that need to be processed at exit. */
struct tagIconsAtExit
{
   /* Basically we need all the parameters that were passed into IPersistFile_fnSave() 
    * originally, so that we can re-enact the function call at exit time. The call is
    * being deferred since the .exe file that need to extract the icon from doesn't 
    * exist yet - this is allowed in windows since it seems to extract the icon lazily.
    */	
   IPersistFile* iface;
   LPOLESTR pszFileName;
   BOOL fRemember;
   struct tagIconsAtExit* pNext;
};
struct tagIconsAtExit *pLLIconsAtExit = NULL;
BOOL bCurrentlyAtExit = FALSE;

static HRESULT WINAPI IPersistFile_fnSave(IPersistFile* iface, LPCOLESTR pszFileName, BOOL fRemember);
#ifdef HAVE_LIBPNG
  static void writepng_error_handler(png_structp png_ptr, png_const_charp msg);
#endif /* HAVE_LIBPNG */

typedef struct
{
        HRSRC *pResInfo;
        int   nIndex;
} ENUMRESSTRUCT;

static ICOM_VTABLE(IShellLinkA)		slvt;
static ICOM_VTABLE(IShellLinkW)		slvtw;
static ICOM_VTABLE(IPersistFile)	pfvt;
static ICOM_VTABLE(IPersistStream)	psvt;
static ICOM_VTABLE(IShellLinkDataList)  dlvt;

/* IShellLink Implementation */

typedef struct
{
	ICOM_VFIELD(IShellLinkA);
	DWORD				ref;

	ICOM_VTABLE(IShellLinkW)*	lpvtblw;
	ICOM_VTABLE(IPersistFile)*	lpvtblPersistFile;
	ICOM_VTABLE(IPersistStream)*	lpvtblPersistStream;
	ICOM_VTABLE(IShellLinkDataList)* lpvtblShellLinkDataList;

	/* internal stream of the IPersistFile interface */
	IStream*			lpFileStream;

	/* data structures according to the informations in the lnk */
	LPSTR		sPath;
	LPITEMIDLIST	pPidl;
	WORD		wHotKey;
	SYSTEMTIME	time1;
	SYSTEMTIME	time2;
	SYSTEMTIME	time3;

	LPSTR		sIcoPath;
	INT		iIcoNdx;
	LPSTR		sArgs;
	LPSTR		sWorkDir;
	LPSTR		sDescription;
} IShellLinkImpl;

#define _IShellLinkW_Offset ((int)(&(((IShellLinkImpl*)0)->lpvtblw)))
#define _ICOM_THIS_From_IShellLinkW(class, name) class* This = (class*)(((char*)name)-_IShellLinkW_Offset);

#define _IPersistFile_Offset ((int)(&(((IShellLinkImpl*)0)->lpvtblPersistFile)))
#define _ICOM_THIS_From_IPersistFile(class, name) class* This = (class*)(((char*)name)-_IPersistFile_Offset);

#define _IPersistStream_Offset ((int)(&(((IShellLinkImpl*)0)->lpvtblPersistStream)))
#define _ICOM_THIS_From_IPersistStream(class, name) class* This = (class*)(((char*)name)-_IPersistStream_Offset);
#define _IPersistStream_From_ICOM_THIS(class, name) class* StreamThis = (class*)(((char*)name)+_IPersistStream_Offset);


/* strdup on the process heap */
inline static LPSTR heap_strdup( LPCSTR str )
{
    if ( str == NULL )
    	return( NULL );
    	
    INT len = strlen(str) + 1;
    LPSTR p = HeapAlloc( GetProcessHeap(), 0, len );
    if (p) memcpy( p, str, len );
    return p;
}


/**************************************************************************
 *  IPersistFile_QueryInterface
 */
static HRESULT WINAPI IPersistFile_fnQueryInterface(
	IPersistFile* iface,
	REFIID riid,
	LPVOID *ppvObj)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface)

	TRACE("(%p)\n",This);

	return IShellLinkA_QueryInterface((IShellLinkA*)This, riid, ppvObj);
}

/******************************************************************************
 * IPersistFile_AddRef
 */
static ULONG WINAPI IPersistFile_fnAddRef(IPersistFile* iface)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface)

	TRACE("(%p)->(count=%lu)\n",This,This->ref);

	return IShellLinkA_AddRef((IShellLinkA*)This);
}
/******************************************************************************
 * IPersistFile_Release
 */
static ULONG WINAPI IPersistFile_fnRelease(IPersistFile* iface)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface)

	TRACE("(%p)->(count=%lu)\n",This,This->ref);

	return IShellLinkA_Release((IShellLinkA*)This);
}

static HRESULT WINAPI IPersistFile_fnGetClassID(IPersistFile* iface, CLSID *pClassID)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface)
	FIXME("(%p)\n",This);
	return NOERROR;
}
static HRESULT WINAPI IPersistFile_fnIsDirty(IPersistFile* iface)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface)
	FIXME("(%p)\n",This);
	return NOERROR;
}
static HRESULT WINAPI IPersistFile_fnLoad(IPersistFile* iface, LPCOLESTR pszFileName, DWORD dwMode)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface)
	_IPersistStream_From_ICOM_THIS(IPersistStream, This)

	LPSTR		sFile = FILE_strdupWtoA ( GetProcessHeap(), 0, pszFileName);
	HRESULT		hRet = E_FAIL;

	TRACE("(%p, %s)\n",This, sFile);


	if (This->lpFileStream)
	  IStream_Release(This->lpFileStream);

	if SUCCEEDED(CreateStreamOnFile(sFile, &(This->lpFileStream)))
	{
	  if SUCCEEDED (IPersistStream_Load(StreamThis, This->lpFileStream))
	  {
	    return NOERROR;
	  }
	}

	return hRet;
}


/* Icon extraction routines
 *
 * FIXME: should use PrivateExtractIcons and friends
 * FIXME: should not use stdio
 */

#ifdef HAVE_LIBPNG
static void writepng_error_handler(png_structp png_ptr, png_const_charp msg)
{
    jmp_buf *jmpbuf;

    /* This function, aside from the extra step of retrieving the "error
     * pointer" (below) and the fact that it exists within the application
     * rather than within libpng, is essentially identical to libpng's
     * default error handler.  The second point is critical:  since both
     * setjmp() and longjmp() are called from the same code, they are
     * guaranteed to have compatible notions of how big a jmp_buf is,
     * regardless of whether _BSD_SOURCE or anything else has (or has not)
     * been defined. */

    fprintf(stderr, "LIBPNG Error: %s\n", msg);
    fflush(stderr);

    jmpbuf = png_get_error_ptr(png_ptr);
    if (jmpbuf == NULL) 
    {         
        /* we are completely hosed now */
        fprintf(stderr,
          "LIBPNG Severe Error:  jmpbuf not recoverable; terminating.\n");
        fflush(stderr);
        exit(99);
    }

    longjmp(*jmpbuf, 1);
}


static BOOL SaveIconResAsPNG(const BITMAPINFO *pIcon, const char *szPNGFilename)
{
   jmp_buf jmpbuf;
   png_structp  png_ptr  = NULL;
   png_infop    info_ptr = NULL;
   FILE        *fPNGFile = NULL;
   BYTE *pImage = (BYTE*) pIcon;
   BYTE *pRow = NULL;
   int colour_type=-1, interlace_type=-1;
   int imageHeight = pIcon->bmiHeader.biHeight/2;
   int imageWidth = pIcon->bmiHeader.biWidth;
   int i, j;
   const RGBQUAD *pRGBQuad = NULL;


   TRACE("szPNGFilename=%s Height=%d, width=%d\n ", szPNGFilename, imageHeight, imageWidth);

   /* Transparency colour */
   pRGBQuad = pIcon->bmiColors;

   /* Skip the info header */
   pImage += sizeof (BITMAPINFOHEADER) + sizeof(RGBQUAD);

   /* Create initial header structures */
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                     &jmpbuf, writepng_error_handler, NULL);
   if (!png_ptr)
   {
      ERR("Ran out of memory allocating PNG structure\n");
      return 0;
   }
  
   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr) 
   {
       ERR("Ran out of memory allocating PNG structure\n");
       png_destroy_write_struct(&png_ptr, NULL);
       return 0;
   }

   /* Open up our file for writing */
   if (!(fPNGFile = fopen(szPNGFilename, "w")))
   {
       ERR("Error opening PNG output file %s\n", szPNGFilename); 
       png_destroy_write_struct(&png_ptr, &info_ptr);
       return 0;
   }

   /* setjmp() must be called in every function that calls a PNG-writing
    * libpng function, unless an alternate error handler was installed--
    * but compatible error handlers must either use longjmp() themselves
    * (as in this program) or exit immediately, so here we go: */
   if (setjmp(jmpbuf)) 
   {
       png_destroy_write_struct(&png_ptr, &info_ptr);
       if (pRow)
       {
          HeapFree(GetProcessHeap(), 0, pRow);
       }
       fclose(fPNGFile);
       return 0;
   }


   /* Use libpng's default I/O handlers */
   png_init_io(png_ptr, fPNGFile);

   /* set the compression levels--in general, always want to leave filtering
    * turned on (except for palette images) and allow all of the filters,
    * which is the default; want 32K zlib window, unless entire image buffer
    * is 16K or smaller (unknown here)--also the default; usually want max
    * compression (NOT the default); and remaining compression flags should
    * be left alone */
   png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
   
   /* We should (for now) always be an RGB icon that is not interlaced */
   colour_type = PNG_COLOR_TYPE_RGB;
   interlace_type = PNG_INTERLACE_NONE;

   /* Set up the PNG image header  - The sample size seems to be for each element
      of RGB. So for a 24 bit image the sample size should be 8. */  
   png_set_IHDR(png_ptr, info_ptr, imageWidth, imageHeight,
                8, colour_type, interlace_type,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

   /* Set up the transparency colour */
   if (pRGBQuad)
   {
      png_color_16 transp;

      transp.red = pRGBQuad->rgbRed;
      transp.blue = pRGBQuad->rgbBlue;
      transp.green = pRGBQuad->rgbGreen;

      png_set_tRNS (png_ptr, info_ptr, NULL, 1, &transp);
   }

   /* write all chunks up to (but not including) first IDAT */
   png_write_info(png_ptr, info_ptr);

   /* set up the transformations:  for now, just pack low-bit-depth pixels
    * into bytes (one, two or four pixels per byte) */
   png_set_packing(png_ptr);

   /* Make sure IDAT is prepared properly */
   png_write_flush (png_ptr);

   {
      BYTE *pRow = HeapAlloc(GetProcessHeap(), 0, imageWidth * (pIcon->bmiHeader.biBitCount/8));

      /* Now for the meat of the function - here we go, row by row, writing into the
         new PNG file */
      for (i=0; i<imageHeight ; i++)   
      {
         for (j=0; j< (imageWidth)*3; j++)
         {
            pRow[j] = pImage[j + ((imageHeight - i - 1) * (imageWidth*3))];  
         }
         png_write_row(png_ptr, pRow);
      }

      HeapFree(GetProcessHeap(), 0, pRow);
   }

   /* We're done.. clean up after ourselves */
   png_write_end(png_ptr, info_ptr);
   if (png_ptr && info_ptr)
      png_destroy_write_struct(&png_ptr, &info_ptr);
   
   fflush(fPNGFile);
   fclose(fPNGFile);

   return (1);
}
#endif /* HAVE_LIBPNG */


static BOOL SaveIconResAsXPM(const BITMAPINFO *pIcon, const char *szXPMFileName)
{
    FILE *fXPMFile;
    int nHeight;
    int nXORWidthBytes;
    int nANDWidthBytes;
    BOOL b8BitColors;
    int nColors;
    BYTE *pXOR;
    BYTE *pAND;
    BOOL aColorUsed[256] = {0};
    int nColorsUsed = 0;
    int i,j;

    if (!((pIcon->bmiHeader.biBitCount == 4) || (pIcon->bmiHeader.biBitCount == 8)))
        return 0;

    if (!(fXPMFile = fopen(szXPMFileName, "w")))
        return 0;

    nHeight = pIcon->bmiHeader.biHeight / 2;
    nXORWidthBytes = 4 * ((pIcon->bmiHeader.biWidth * pIcon->bmiHeader.biBitCount / 32)
                          + ((pIcon->bmiHeader.biWidth * pIcon->bmiHeader.biBitCount % 32) > 0));
    nANDWidthBytes = 4 * ((pIcon->bmiHeader.biWidth / 32)
                          + ((pIcon->bmiHeader.biWidth % 32) > 0));
    b8BitColors = pIcon->bmiHeader.biBitCount == 8;
    nColors = pIcon->bmiHeader.biClrUsed ? pIcon->bmiHeader.biClrUsed
        : 1 << pIcon->bmiHeader.biBitCount;
    pXOR = (BYTE*) pIcon + sizeof (BITMAPINFOHEADER) + (nColors * sizeof (RGBQUAD));
    pAND = pXOR + nHeight * nXORWidthBytes;

#define MASK(x,y) (pAND[(x) / 8 + (nHeight - (y) - 1) * nANDWidthBytes] & (1 << (7 - (x) % 8)))
#define COLOR(x,y) (b8BitColors ? pXOR[(x) + (nHeight - (y) - 1) * nXORWidthBytes] : (x) % 2 ? pXOR[(x) / 2 + (nHeight - (y) - 1) * nXORWidthBytes] & 0xF : (pXOR[(x) / 2 + (nHeight - (y) - 1) * nXORWidthBytes] & 0xF0) >> 4)

    for (i = 0; i < nHeight; i++)
        for (j = 0; j < pIcon->bmiHeader.biWidth; j++)
            if (!aColorUsed[COLOR(j,i)] && !MASK(j,i))
            {
                aColorUsed[COLOR(j,i)] = TRUE;
                nColorsUsed++;
            }

    if (fprintf(fXPMFile, "/* XPM */\nstatic char *icon[] = {\n") <= 0)
        goto error;
    if (fprintf(fXPMFile, "\"%d %d %d %d\",\n",
                (int) pIcon->bmiHeader.biWidth, nHeight, nColorsUsed + 1, 2) <=0)
        goto error;

    for (i = 0; i < nColors; i++)
        if (aColorUsed[i])
            if (fprintf(fXPMFile, "\"%.2X c #%.2X%.2X%.2X\",\n", i, pIcon->bmiColors[i].rgbRed,
                        pIcon->bmiColors[i].rgbGreen, pIcon->bmiColors[i].rgbBlue) <= 0)
                goto error;
    if (fprintf(fXPMFile, "\"   c None\"") <= 0)
        goto error;

    for (i = 0; i < nHeight; i++)
    {
        if (fprintf(fXPMFile, ",\n\"") <= 0)
            goto error;
        for (j = 0; j < pIcon->bmiHeader.biWidth; j++)
        {
            if MASK(j,i)
                {
                    if (fprintf(fXPMFile, "  ") <= 0)
                        goto error;
                }
            else
                if (fprintf(fXPMFile, "%.2X", COLOR(j,i)) <= 0)
                    goto error;
        }
        if (fprintf(fXPMFile, "\"") <= 0)
            goto error;
    }
    if (fprintf(fXPMFile, "};\n") <= 0)
        goto error;

#undef MASK
#undef COLOR

    fclose(fXPMFile);
    return 1;

 error:
    fclose(fXPMFile);
    unlink( szXPMFileName ); 
    return 0;
}

static BOOL CALLBACK EnumResNameProc(HANDLE hModule, const char *lpszType, char *lpszName, LONG lParam)
{
    ENUMRESSTRUCT *sEnumRes = (ENUMRESSTRUCT *) lParam;

    if (!sEnumRes->nIndex--)
    {
      *sEnumRes->pResInfo = FindResourceA(hModule, lpszName, RT_GROUP_ICONA);
      return FALSE;
    }
    else
      return TRUE;
}

static int ExtractFromEXEDLL(const char *szFileName, int nIndex, const char *szXPMFileName, int *png)
{
    HMODULE hModule;
    HRSRC hResInfo;
    char *lpName = NULL;
    HGLOBAL hResData;
    GRPICONDIR *pIconDir;
    BITMAPINFO *pIcon;
    ENUMRESSTRUCT sEnumRes;
    int nMax = 0;
    int nMaxBits = 0;
    int i;
    *png = 0;


    TRACE("szFileName=%s, nIndex=%d, szXPMFileName=%s, png=%p\n", 
          szFileName, nIndex, szXPMFileName, png);
    if (!(hModule = LoadLibraryExA(szFileName, 0, LOAD_LIBRARY_AS_DATAFILE)))
    {
        TRACE("LoadLibraryExA (%s) failed, error %ld\n", szFileName, GetLastError());
        goto error1;
    }

    if (nIndex < 0)
    {
        hResInfo = FindResourceA(hModule, MAKEINTRESOURCEA(-nIndex), RT_GROUP_ICONA);
        TRACE("FindResourceA (%s) called, return 0x%x, error %ld\n", szFileName, hResInfo, GetLastError());
    }
    else
    {
        sEnumRes.pResInfo = &hResInfo;
        sEnumRes.nIndex = nIndex;
        if (EnumResourceNamesA(hModule, RT_GROUP_ICONA, &EnumResNameProc, (LONG) &sEnumRes))
        {
            TRACE("EnumResourceNamesA failed, error %ld\n", GetLastError());
            goto error2;
        }
    }

    if (!hResInfo)
    {
        TRACE("ExtractFromEXEDLL failed, error %ld\n", GetLastError());
        goto error2;
    }

    if (!(hResData = LoadResource(hModule, hResInfo)))
    {
        TRACE("LoadResource failed, error %ld\n", GetLastError());
        goto error2;
    }
    if (!(pIconDir = LockResource(hResData)))
    {
        TRACE("LockResource failed, error %ld\n", GetLastError());
        goto error3;
    }

    for (i = 0; i < pIconDir->idCount; i++)
    {

        if ((pIconDir->idEntries[i].wBitCount >= nMaxBits) && (pIconDir->idEntries[i].wBitCount <= 
#ifdef HAVE_LIBPNG
        /* 24 bit icons are only supported if there is PNG support */
        24))
#else
        /* With no PNG support we can only support up to 8 bit icons */
        8))
#endif /* HAVE_LIBPNG */
        {
          if (pIconDir->idEntries[i].wBitCount > nMaxBits)
          {
              nMaxBits = pIconDir->idEntries[i].wBitCount;
              nMax = 0;
          }
          if ((pIconDir->idEntries[i].bHeight * pIconDir->idEntries[i].bWidth) > nMax)
          {
              lpName = MAKEINTRESOURCEA(pIconDir->idEntries[i].nID);
              nMax = pIconDir->idEntries[i].bHeight * pIconDir->idEntries[i].bWidth;
          }
        }
    }
    FreeResource(hResData);

    if (!(hResInfo = FindResourceA(hModule, lpName, RT_ICONA)))
    {
        TRACE("Second FindResourceA failed, error %ld\n", GetLastError());
        goto error2;
    }
    if (!(hResData = LoadResource(hModule, hResInfo)))
    {
        TRACE("Second LoadResource failed, error %ld\n", GetLastError());
        goto error2;
    }
    if (!(pIcon = LockResource(hResData)))
    {
        TRACE("Second LockResource failed, error %ld\n", GetLastError());
        goto error3;
    }

#ifdef HAVE_PNG_H
# ifdef HAVE_LIBPNG
    if (pIcon->bmiHeader.biBitCount == 24)
    {
       if(!SaveIconResAsPNG(pIcon, szXPMFileName))
       {
          TRACE("Failed saving icon as PNG, error %ld\n", GetLastError());
          goto error3;
       }
       *png = 1; /* Icon extracted was a PNG not XPM */
    } else
# endif /* HAVE_LIBPNG */
#endif /* HAVE_PNG_H */
    if(!SaveIconResAsXPM(pIcon, szXPMFileName))
    {
        TRACE("Failed saving icon as XPM, error %ld\n", GetLastError());
        goto error3;
    }

    FreeResource(hResData);
    FreeLibrary(hModule);

    return 1;

 error3:
    FreeResource(hResData);
 error2:
    FreeLibrary(hModule);
 error1:
    return 0;
}

static int ExtractFromICO(const char *szFileName, const char *szXPMFileName)
{
    HANDLE hICOFile;
    ICONDIR iconDir;
    ICONDIRENTRY *pIconDirEntry;
    DWORD dwRead;
    int nMax = 0;
    int nIndex = 0;
    void *pIcon;
    int i;

    hICOFile = CreateFileA(szFileName, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, 0, 0);
    if (hICOFile == INVALID_HANDLE_VALUE)
        goto error1;

    if (!ReadFile(hICOFile, &iconDir, sizeof(ICONDIR), &dwRead, NULL)
        || dwRead != sizeof(ICONDIR))
        goto error2;
    if ((iconDir.idReserved != 0) || (iconDir.idType != 1))
        goto error2;

    if (!(pIconDirEntry = HeapAlloc(GetProcessHeap(), 0, iconDir.idCount * sizeof(ICONDIRENTRY))))
        goto error2;
    if (!ReadFile(hICOFile, pIconDirEntry, sizeof(ICONDIRENTRY) * iconDir.idCount, &dwRead, NULL)
        || dwRead != sizeof(ICONDIRENTRY) * iconDir.idCount)
        goto error3;

    for (i = 0; i < iconDir.idCount; i++)
        if ((pIconDirEntry[i].bHeight * pIconDirEntry[i].bWidth) > nMax)
        {
            nIndex = i;
            nMax = pIconDirEntry[i].bHeight * pIconDirEntry[i].bWidth;
        }
    if (!(pIcon = HeapAlloc(GetProcessHeap(), 0, pIconDirEntry[nIndex].dwBytesInRes)))
        goto error3;
    if (SetFilePointer(hICOFile, pIconDirEntry[nIndex].dwImageOffset, NULL, FILE_BEGIN)
        != pIconDirEntry[nIndex].dwImageOffset)
        goto error4;
    if (!ReadFile(hICOFile, pIcon, pIconDirEntry[nIndex].dwBytesInRes, &dwRead, NULL)
        || dwRead != pIconDirEntry[nIndex].dwBytesInRes)
        goto error4;

    if(!SaveIconResAsXPM(pIcon, szXPMFileName))
        goto error4;

    HeapFree(GetProcessHeap(), 0, pIcon);
    HeapFree(GetProcessHeap(), 0, pIconDirEntry);
    CloseHandle(hICOFile);

    return 1;

 error4:
    HeapFree(GetProcessHeap(), 0, pIcon);
 error3:
    HeapFree(GetProcessHeap(), 0, pIconDirEntry);
 error2:
    CloseHandle(hICOFile);
 error1:
    return 0;
}

/* get the Unix file name for a given path, allocating the string */
inline static char *get_unix_file_name( const char *dos )
{
    char buffer[MAX_PATH];

    if (!wine_get_unix_file_name( dos, buffer, sizeof(buffer) )) return NULL;
    return heap_strdup( buffer );
}

static BOOL create_default_icon( const char *filename )
{
    FILE *fXPM;
    int i;

    if (!(fXPM = fopen(filename, "w"))) return FALSE;
    fprintf(fXPM, "/* XPM */\nstatic char * icon[] = {");
    for (i = 0; i < sizeof(wine_xpm)/sizeof(wine_xpm[0]); i++)
        fprintf( fXPM, "\n\"%s\",", wine_xpm[i]);
    fprintf( fXPM, "};\n" );
    fclose( fXPM );
    return TRUE;
}


/* This function gets registered as an atexit() function. It will
 * attempt to process the saving of icons for .exes that couldn't be saved
 * because the .exe was not present on disk. This is an attempt to eliminate the
 * issue of default TG icons being created. */
void atexit_ProcessRemainingIcons(void)
{
   struct tagIconsAtExit *pLLSurf=NULL, *pTemp=NULL;   

   /* This global variable needs to be set to handle the
    * following error case:
    *  - We are currently in the at exit state
    *  - the .exe file is STILL not on disk
    *   -> We do NOT want it to attempt to add the iface to the
    *      linked list of icons to process, we need to now take the
    *      old error route, create a default icon.. this is handled in 
    *      IPersistFile_fnSave()
    */
   bCurrentlyAtExit = TRUE;

   TRACE("Seems we have some icons that need to be processed...\n");
   
   /* Go through the LL, attempt to process the icon, then free the
    * memory */
   pLLSurf = pLLIconsAtExit;
   while (pLLSurf)
   {
       TRACE ("Attempting to atexit process %s\n", debugstr_w(pLLSurf->pszFileName) );

       /* Attempt to create the icon by recreating the fncall that would have
        * failed */
       IPersistFile_fnSave(pLLSurf->iface, pLLSurf->pszFileName, pLLSurf->fRemember);    
       
       /* Release the memory for members */
       IPersistFile_Release(pLLSurf->iface);
       HeapFree(GetProcessHeap(), 0, pLLSurf->pszFileName);

       /* Get the next entry pointer and free the memory for this linked-list entry */
       pTemp = pLLSurf;
       pLLSurf = pLLSurf->pNext;
       HeapFree(GetProcessHeap(), 0, pTemp);
   }
}
    
/* extract an icon from an exe or icon file; helper for IPersistFile_fnSave */
static char *extract_icon( const char *path, int index, int *png )
{
    char *filename = heap_strdup( tmpnam(NULL) );
    *png = 0;

    if (ExtractFromEXEDLL( path, index, filename, png )) return filename;
    if (ExtractFromICO( path, filename )) return filename;
    if (bCurrentlyAtExit && create_default_icon( filename )) 
    {
       /* Only do this if we are atexit state... otherwise we defer the icon
        * creation until later to see if we can avoid creating the default icon */
       return filename;
    }

    HeapFree( GetProcessHeap(), 0, filename );
    return NULL;
}


static HRESULT WINAPI IPersistFile_fnSave(IPersistFile* iface, LPCOLESTR pszFileName, BOOL fRemember)
{
    HRESULT ret = NOERROR;
    int pid, status;
    char buffer[MAX_PATH], buff2[MAX_PATH];
    char *filename, *link_name, *p;
    char *shell_link_app = NULL;
    char *icon_name = NULL;
    char *work_dir = NULL;
    BOOL bDesktop;
    HKEY hkey;
    static BOOL atExitRegistered=FALSE;
    int png = 0;
    
    char *new_sPath = NULL;
    char file_path[ MAX_PATH ];
    char *url_protocols[] =
    { "http://", "https://", "file://", NULL };
    int x;
    

    _ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface);

    TRACE("(%p)->(%s)\n",This,debugstr_w(pszFileName));

    if (!pszFileName || !This->sPath)
        return ERROR_UNKNOWN;

    /*
     * Set the working sPath.
     *
     *   If it is a remote URL? accept it.
     *   If it is an EXE file, accept it.
     *   If it is a valid .htm or .html path, resolve it to the full UNIX path.
     */
    for ( x = 0; url_protocols[x] != NULL; x ++ )
    {
        if ( !strncasecmp(url_protocols[x], This->sPath, strlen(url_protocols[x])) )
        {
            new_sPath = This->sPath;
            break;
        }
    }
    
    if ( new_sPath == NULL && (p = strrchr(This->sPath, '.')) )
    {
        if ( !strcasecmp(p, ".exe") )
	    new_sPath = This->sPath;
        else if ( (!strcasecmp(p, ".html") || !strcasecmp(p, ".htm")) &&
                  wine_get_unix_file_name(This->sPath, file_path, sizeof(file_path)) )
        {
            strcpy( file_path, "file://" );
            wine_get_unix_file_name( This->sPath, file_path + strlen(file_path), sizeof(file_path) - strlen(file_path) );
            new_sPath = file_path;
        }
    }
    
    if ( new_sPath == NULL )
        return( NOERROR );
    
    
    /* check if ShellLinker configured */
    buffer[0] = 0;

    /* WINESHELLLINK environement variable takes precidence */
    if (!GetEnvironmentVariableA( "WINESHELLLINK", buffer, sizeof(buffer) ) )
    {
       /* If no env variable set, let's do this the old fashioned way, from the registry */
       if (!RegOpenKeyExA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Wine",
                           0, KEY_ALL_ACCESS, &hkey ))
       {
          DWORD type, count = sizeof(buffer);
          if (RegQueryValueExA( hkey, "ShellLinker", 0, &type, (LPBYTE)buffer, &count )) buffer[0] = 0;
          RegCloseKey( hkey );
       }
    } 

    /* If no shellink app set at all, we just bail out... */
    if (!*buffer) return NOERROR;
    shell_link_app = heap_strdup( buffer );

    if (!WideCharToMultiByte( CP_ACP, 0, pszFileName, -1, buffer, sizeof(buffer), NULL, NULL))
        return ERROR_UNKNOWN;
    GetFullPathNameA( buffer, sizeof(buff2), buff2, NULL );
    filename = heap_strdup( buff2 );

    if (SHGetSpecialFolderPathA( 0, buffer, CSIDL_STARTUP, FALSE ))
    {
        /* ignore startup for now */
        if (!strncasecmp( filename, buffer, strlen(buffer) )) goto done;
    }
    if (SHGetSpecialFolderPathA( 0, buffer, CSIDL_DESKTOPDIRECTORY, FALSE ))
    {
        if (!strncasecmp( filename, buffer, strlen(buffer) ))
        {
            link_name = filename + strlen(buffer);
            bDesktop = TRUE;
            goto found;
        }
    }
    if (SHGetSpecialFolderPathA( 0, buffer, CSIDL_STARTMENU, FALSE ))
    {
        if (!strncasecmp( filename, buffer, strlen(buffer) ))
        {
            link_name = filename + strlen(buffer);
            bDesktop = FALSE;
            goto found;
        }
    }
    if (SHGetSpecialFolderPathA( 0, buffer, CSIDL_COMMON_PROGRAMS, FALSE ))
    {
        if (!strncasecmp( filename, buffer, strlen(buffer) ))
        {
            link_name = filename + strlen(buffer);
            bDesktop = FALSE;
            goto found;
        }
    }
    goto done;

 found:
    /* make link name a Unix name */
    for (p = link_name; *p; p++) if (*p == '\\') *p = '/';
    /* strip leading slashes */
    while (*link_name == '/') link_name++;
    /* remove extension */
    if ((p = strrchr( link_name, '.' ))) *p = 0;

    /* convert app working dir */
    if (This->sWorkDir) work_dir = get_unix_file_name( This->sWorkDir );

    /* extract the icon */
    if (!(icon_name = extract_icon( This->sIcoPath && strlen(This->sIcoPath) ?
                                    This->sIcoPath : new_sPath,
                                    This->iIcoNdx, &png )))
    {
       struct tagIconsAtExit *pLLSurf=NULL;
       int filename_len = (lstrlenW(pszFileName)+1) * sizeof(WCHAR);

       /* If we are currently in atexit time, bail out - default icon was
        * created... Nothing more we can do */
       if (bCurrentlyAtExit)
       {
          TRACE("Icon %s could still not be created. Default used instead.\n", debugstr_w(pszFileName));
          goto done;
       }

       TRACE("Cannot create icon %s. Deferring until atexit.\n", debugstr_w(pszFileName));
       
       /* If we got here, that means the icon extraction failed. Most likely
        * this is due to the fact that the .exe file we are trying to extract
        * the icon from doesn't actually exist yet... */
       if(!atExitRegistered)
       {
          atexit(atexit_ProcessRemainingIcons);
          atExitRegistered = TRUE;
       }

       /* Now to add ourselves to the list of icons that need atexit processing */
       pLLSurf = pLLIconsAtExit;

       if (pLLSurf == NULL)
       {
          /* Start out the linked list */
          pLLIconsAtExit = pLLSurf = HeapAlloc(GetProcessHeap(), 0, sizeof (struct tagIconsAtExit));
       }
       else
       {
          /* Go to the end of the linked list and add an entry */     
          while(pLLSurf->pNext)
             pLLSurf = pLLSurf->pNext;
          pLLSurf->pNext = HeapAlloc(GetProcessHeap(), 0, sizeof (struct tagIconsAtExit));
          pLLSurf = pLLSurf->pNext;
       }

       /* Add our new entry that needs to be processed */
       pLLSurf->iface = iface;
       /* Ensure the reference count is incremented so that it doesn't get freed on us */
       IPersistFile_AddRef(pLLSurf->iface);

       /* Grab the filename */
       pLLSurf->pszFileName = HeapAlloc(GetProcessHeap(), 0, filename_len);
       memcpy(pLLSurf->pszFileName, pszFileName, filename_len );

       pLLSurf->fRemember = fRemember;
       pLLSurf->pNext = NULL;

       goto done;
    }

    TRACE("linker app='%s' link='%s' mode=%s path='%s' args='%s' icon='%s' workdir='%s' descr='%s'\n",
        shell_link_app, link_name, bDesktop ? "desktop" : "menu", new_sPath,
        This->sArgs ? This->sArgs : "", icon_name, work_dir ? work_dir : "",
        This->sDescription ? This->sDescription : "" );

    if ((pid = fork()) == -1) goto done;
    if (!pid)
    {
        int pos = 0;
        char *argv[20];
        argv[pos++] = shell_link_app;
        argv[pos++] = "--link";
        argv[pos++] = link_name;
        argv[pos++] = "--path";
        argv[pos++] = new_sPath;
        argv[pos++] = bDesktop ? "--desktop" : "--menu";
        if (This->sArgs && strlen(This->sArgs))
        {
            argv[pos++] = "--args";
            argv[pos++] = This->sArgs;
        }
        if (icon_name)
        {
            argv[pos++] = "--icon";
            argv[pos++] = icon_name;
        }
        if (work_dir && strlen(work_dir))
        {
            argv[pos++] = "--workdir";
            argv[pos++] = work_dir;
        }
        if (This->sDescription && strlen(This->sDescription))
        {
            argv[pos++] = "--descr";
            argv[pos++] = This->sDescription;
        }
        if (png)
        {
           argv[pos++] = "--png";
        }

        argv[pos] = NULL;
        execvp( shell_link_app, argv );
        _exit(1);
    }

    while (waitpid( pid, &status, 0 ) == -1)
    {
        if (errno != EINTR)
        {
            ret = ERROR_UNKNOWN;
            goto done;
        }
    }
    if (status) ret = E_ACCESSDENIED;

 done:
    if (icon_name) unlink( icon_name ); 
    HeapFree( GetProcessHeap(), 0, shell_link_app );
    HeapFree( GetProcessHeap(), 0, filename );
    HeapFree( GetProcessHeap(), 0, icon_name );
    HeapFree( GetProcessHeap(), 0, work_dir );
    
    return ret;
}

static HRESULT WINAPI IPersistFile_fnSaveCompleted(IPersistFile* iface, LPCOLESTR pszFileName)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface);
	FIXME("(%p)->(%s)\n",This,debugstr_w(pszFileName));
	return NOERROR;
}
static HRESULT WINAPI IPersistFile_fnGetCurFile(IPersistFile* iface, LPOLESTR *ppszFileName)
{
	_ICOM_THIS_From_IPersistFile(IShellLinkImpl, iface);
	FIXME("(%p)\n",This);
	return NOERROR;
}

static ICOM_VTABLE(IPersistFile) pfvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IPersistFile_fnQueryInterface,
	IPersistFile_fnAddRef,
	IPersistFile_fnRelease,
	IPersistFile_fnGetClassID,
	IPersistFile_fnIsDirty,
	IPersistFile_fnLoad,
	IPersistFile_fnSave,
	IPersistFile_fnSaveCompleted,
	IPersistFile_fnGetCurFile
};

/************************************************************************
 * IPersistStream_QueryInterface
 */
static HRESULT WINAPI IPersistStream_fnQueryInterface(
	IPersistStream* iface,
	REFIID     riid,
	VOID**     ppvoid)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)\n",This);

	return IShellLinkA_QueryInterface((IShellLinkA*)This, riid, ppvoid);
}

/************************************************************************
 * IPersistStream_Release
 */
static ULONG WINAPI IPersistStream_fnRelease(
	IPersistStream* iface)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)\n",This);

	return IShellLinkA_Release((IShellLinkA*)This);
}

/************************************************************************
 * IPersistStream_AddRef
 */
static ULONG WINAPI IPersistStream_fnAddRef(
	IPersistStream* iface)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)\n",This);

	return IShellLinkA_AddRef((IShellLinkA*)This);
}

/************************************************************************
 * IPersistStream_GetClassID
 *
 */
static HRESULT WINAPI IPersistStream_fnGetClassID(
	IPersistStream* iface,
	CLSID* pClassID)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)\n", This);

	if (pClassID==0)
	  return E_POINTER;

/*	memcpy(pClassID, &CLSID_???, sizeof(CLSID_???)); */

	return S_OK;
}

/************************************************************************
 * IPersistStream_IsDirty (IPersistStream)
 */
static HRESULT WINAPI IPersistStream_fnIsDirty(
	IPersistStream*  iface)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)\n", This);

	return S_OK;
}
/************************************************************************
 * IPersistStream_Load (IPersistStream)
 */

static HRESULT WINAPI IPersistStream_fnLoad(
	IPersistStream*  iface,
	IStream*         pLoadStream)
{
	PLINK_HEADER lpLinkHeader = HeapAlloc(GetProcessHeap(), 0, LINK_HEADER_SIZE);
	ULONG	dwBytesRead;
	DWORD	ret = E_FAIL;
	char	sTemp[MAX_PATH];

	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)(%p)\n", This, pLoadStream);

	if ( ! pLoadStream)
	{
	  return STG_E_INVALIDPOINTER;
	}

	IStream_AddRef (pLoadStream);
	if(lpLinkHeader)
	{
	  if (SUCCEEDED(IStream_Read(pLoadStream, lpLinkHeader, LINK_HEADER_SIZE, &dwBytesRead)))
	  {
	    if ((lpLinkHeader->MagicStr == 0x0000004CL) && IsEqualIID(&lpLinkHeader->MagicGuid, &CLSID_ShellLink))
	    {
	      lpLinkHeader = HeapReAlloc(GetProcessHeap(), 0, lpLinkHeader, LINK_HEADER_SIZE+lpLinkHeader->PidlSize);
	      if (lpLinkHeader)
	      {
	        if (SUCCEEDED(IStream_Read(pLoadStream, &(lpLinkHeader->Pidl), lpLinkHeader->PidlSize, &dwBytesRead)))
	        {
	          if (pcheck (&lpLinkHeader->Pidl))
	          {
	            This->pPidl = ILClone (&lpLinkHeader->Pidl);

	            SHGetPathFromIDListA(&lpLinkHeader->Pidl, sTemp);
	            This->sPath = heap_strdup( sTemp );
	          }
		  This->wHotKey = lpLinkHeader->wHotKey;
		  FileTimeToSystemTime (&lpLinkHeader->Time1, &This->time1);
		  FileTimeToSystemTime (&lpLinkHeader->Time2, &This->time2);
		  FileTimeToSystemTime (&lpLinkHeader->Time3, &This->time3);
#if 1
		  GetDateFormatA(LOCALE_USER_DEFAULT,DATE_SHORTDATE,&This->time1, NULL, sTemp, 256);
		  TRACE("-- time1: %s\n", sTemp);
		  GetDateFormatA(LOCALE_USER_DEFAULT,DATE_SHORTDATE,&This->time2, NULL, sTemp, 256);
		  TRACE("-- time1: %s\n", sTemp);
		  GetDateFormatA(LOCALE_USER_DEFAULT,DATE_SHORTDATE,&This->time3, NULL, sTemp, 256);
		  TRACE("-- time1: %s\n", sTemp);
		  pdump (This->pPidl);
#endif
		  ret = S_OK;
	        }
	      }
	    }
	    else
	    {
	      WARN("stream contains no link!\n");
	    }
	  }
	}

	IStream_Release (pLoadStream);

	pdump(This->pPidl);

	HeapFree(GetProcessHeap(), 0, lpLinkHeader);

	return ret;
}

/************************************************************************
 * IPersistStream_Save (IPersistStream)
 */
static HRESULT WINAPI IPersistStream_fnSave(
	IPersistStream*  iface,
	IStream*         pOutStream,
	BOOL             fClearDirty)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p) %p %x\n", This, pOutStream, fClearDirty);

	return E_NOTIMPL;
}

/************************************************************************
 * IPersistStream_GetSizeMax (IPersistStream)
 */
static HRESULT WINAPI IPersistStream_fnGetSizeMax(
	IPersistStream*  iface,
	ULARGE_INTEGER*  pcbSize)
{
	_ICOM_THIS_From_IPersistStream(IShellLinkImpl, iface);

	TRACE("(%p)\n", This);

	return E_NOTIMPL;
}

static ICOM_VTABLE(IPersistStream) psvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IPersistStream_fnQueryInterface,
	IPersistStream_fnAddRef,
	IPersistStream_fnRelease,
	IPersistStream_fnGetClassID,
	IPersistStream_fnIsDirty,
	IPersistStream_fnLoad,
	IPersistStream_fnSave,
	IPersistStream_fnGetSizeMax
};


static HRESULT WINAPI
ShellLink_DataList_QueryInterface( IShellLinkDataList* iface, REFIID riid, void** ppvObject)
{
	FIXME( "Stub\n" );
	return( 0 );
}
        
static ULONG WINAPI
ShellLink_DataList_AddRef( IShellLinkDataList* iface )
{
	FIXME( "Stub\n" );
	return( 0 );
}

static ULONG WINAPI
ShellLink_DataList_Release( IShellLinkDataList* iface )
{
	FIXME( "Stub\n" );
	return( 0 );
}

static HRESULT WINAPI
ShellLink_AddDataBlock( IShellLinkDataList* iface, void* pDataBlock )
{
	FIXME( "Stub\n" );
	return( 0 );
}

static HRESULT WINAPI
ShellLink_CopyDataBlock( IShellLinkDataList* iface, DWORD dwSig, void** ppDataBlock )
{
	FIXME( "Stub\n" );
	return( 0 );
}

static HRESULT WINAPI
ShellLink_RemoveDataBlock( IShellLinkDataList* iface, DWORD dwSig )
{
	FIXME( "Stub\n" );
	return( 0 );
}

static HRESULT WINAPI
ShellLink_GetFlags( IShellLinkDataList* iface, DWORD* pdwFlags )
{
	FIXME( "Stub\n" );
	return( 0 );
}


static HRESULT WINAPI
ShellLink_SetFlags( IShellLinkDataList* iface, DWORD dwFlags )
{
	FIXME( "Stub\n" );
	return( 0 );
}


static ICOM_VTABLE(IShellLinkDataList) dlvt =
{
	ShellLink_DataList_QueryInterface,
	ShellLink_DataList_AddRef,
	ShellLink_DataList_Release,
	ShellLink_AddDataBlock,
        ShellLink_CopyDataBlock,
        ShellLink_RemoveDataBlock,
        ShellLink_GetFlags,
        ShellLink_SetFlags
};
                                


/**************************************************************************
 *	  IShellLink_Constructor
 */
IShellLinkA * IShellLink_Constructor(BOOL bUnicode)
{	IShellLinkImpl * sl;

	sl = (IShellLinkImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IShellLinkImpl));
	sl->ref = 1;
	ICOM_VTBL(sl) = &slvt;
	sl->lpvtblw = &slvtw;
	sl->lpvtblPersistFile = &pfvt;
	sl->lpvtblPersistStream = &psvt;
	sl->lpvtblShellLinkDataList = &dlvt;

	TRACE("(%p)->()\n",sl);
	shell32_ObjCount++;
	return bUnicode ? (IShellLinkA *) &(sl->lpvtblw) : (IShellLinkA *)sl;
}

/**************************************************************************
 *  IShellLinkA_QueryInterface
 */
static HRESULT WINAPI IShellLinkA_fnQueryInterface( IShellLinkA * iface, REFIID riid,  LPVOID *ppvObj)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(\n\tIID:\t%s)\n",This,debugstr_guid(riid));

	*ppvObj = NULL;

	if(IsEqualIID(riid, &IID_IUnknown) ||
	   IsEqualIID(riid, &IID_IShellLinkA))
	{
	  *ppvObj = This;
	}
	else if(IsEqualIID(riid, &IID_IShellLinkW))
	{
	  *ppvObj = (IShellLinkW *)&(This->lpvtblw);
	}
	else if(IsEqualIID(riid, &IID_IPersistFile))
	{
	  *ppvObj = (IPersistFile *)&(This->lpvtblPersistFile);
	}
	else if(IsEqualIID(riid, &IID_IPersistStream))
	{
	  *ppvObj = (IPersistStream *)&(This->lpvtblPersistStream);
	}
	else if (IsEqualIID(riid, &IID_IShellLinkDataList))
	{
	  *ppvObj = (IShellLinkDataList *)&(This->lpvtblShellLinkDataList);
	}

	if(*ppvObj)
	{
	  IUnknown_AddRef((IUnknown*)(*ppvObj));
	  TRACE("-- Interface: (%p)->(%p)\n",ppvObj,*ppvObj);
	  return S_OK;
	}
	TRACE("-- Interface: E_NOINTERFACE\n");
	return E_NOINTERFACE;
}
/******************************************************************************
 * IShellLinkA_AddRef
 */
static ULONG WINAPI IShellLinkA_fnAddRef(IShellLinkA * iface)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(count=%lu)\n",This,This->ref);

	shell32_ObjCount++;
	return ++(This->ref);
}
/******************************************************************************
 *	IShellLinkA_Release
 */
static ULONG WINAPI IShellLinkA_fnRelease(IShellLinkA * iface)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(count=%lu)\n",This,This->ref);

	shell32_ObjCount--;
	if (!--(This->ref))
	{ TRACE("-- destroying IShellLink(%p)\n",This);

	  if (This->sIcoPath)
	    HeapFree(GetProcessHeap(), 0, This->sIcoPath);

	  if (This->sArgs)
	    HeapFree(GetProcessHeap(), 0, This->sArgs);

	  if (This->sWorkDir)
	    HeapFree(GetProcessHeap(), 0, This->sWorkDir);

	  if (This->sDescription)
	    HeapFree(GetProcessHeap(), 0, This->sDescription);

	  if (This->sPath)
	    HeapFree(GetProcessHeap(),0,This->sPath);

	  if (This->pPidl)
	    SHFree(This->pPidl);

	  if (This->lpFileStream)
	    IStream_Release(This->lpFileStream);

	  This->iIcoNdx = 0;

	  HeapFree(GetProcessHeap(),0,This);
	  return 0;
	}
	return This->ref;
}

static HRESULT WINAPI IShellLinkA_fnGetPath(IShellLinkA * iface, LPSTR pszFile,INT cchMaxPath, WIN32_FIND_DATAA *pfd, DWORD fFlags)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(pfile=%p len=%u find_data=%p flags=%lu)(%s)\n",This, pszFile, cchMaxPath, pfd, fFlags, debugstr_a(This->sPath));

	if (This->sPath)
	  lstrcpynA(pszFile,This->sPath, cchMaxPath);
	else
	  return E_FAIL;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetIDList(IShellLinkA * iface, LPITEMIDLIST * ppidl)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(ppidl=%p)\n",This, ppidl);

	*ppidl = ILClone(This->pPidl);
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetIDList(IShellLinkA * iface, LPCITEMIDLIST pidl)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(pidl=%p)\n",This, pidl);

	if (This->pPidl)
	    SHFree(This->pPidl);
	This->pPidl = ILClone (pidl);
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetDescription(IShellLinkA * iface, LPSTR pszName,INT cchMaxName)
{
	ICOM_THIS(IShellLinkImpl, iface);

	FIXME("(%p)->(%p len=%u)\n",This, pszName, cchMaxName);
	lstrcpynA(pszName,"Description, FIXME",cchMaxName);
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetDescription(IShellLinkA * iface, LPCSTR pszName)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(pName=%s)\n", This, pszName);

	if (This->sDescription)
	    HeapFree(GetProcessHeap(), 0, This->sDescription);
	if (!(This->sDescription = heap_strdup(pszName)))
	    return E_OUTOFMEMORY;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetWorkingDirectory(IShellLinkA * iface, LPSTR pszDir,INT cchMaxPath)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(%p len=%u)\n", This, pszDir, cchMaxPath);

	lstrcpynA( pszDir, This->sWorkDir ? This->sWorkDir : "", cchMaxPath );

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetWorkingDirectory(IShellLinkA * iface, LPCSTR pszDir)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(dir=%s)\n",This, pszDir);

	if (This->sWorkDir)
	    HeapFree(GetProcessHeap(), 0, This->sWorkDir);
	if (!(This->sWorkDir = heap_strdup(pszDir)))
	    return E_OUTOFMEMORY;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetArguments(IShellLinkA * iface, LPSTR pszArgs,INT cchMaxPath)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(%p len=%u)\n", This, pszArgs, cchMaxPath);

	lstrcpynA( pszArgs, This->sArgs ? This->sArgs : "", cchMaxPath );

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetArguments(IShellLinkA * iface, LPCSTR pszArgs)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(args=%s)\n",This, pszArgs);

	if (This->sArgs)
	    HeapFree(GetProcessHeap(), 0, This->sArgs);
	if (!(This->sArgs = heap_strdup(pszArgs)))
	    return E_OUTOFMEMORY;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetHotkey(IShellLinkA * iface, WORD *pwHotkey)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(%p)(0x%08x)\n",This, pwHotkey, This->wHotKey);

	*pwHotkey = This->wHotKey;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetHotkey(IShellLinkA * iface, WORD wHotkey)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(hotkey=%x)\n",This, wHotkey);

	This->wHotKey = wHotkey;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetShowCmd(IShellLinkA * iface, INT *piShowCmd)
{
	ICOM_THIS(IShellLinkImpl, iface);

	FIXME("(%p)->(%p)\n",This, piShowCmd);
	*piShowCmd=0;
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetShowCmd(IShellLinkA * iface, INT iShowCmd)
{
	ICOM_THIS(IShellLinkImpl, iface);

	FIXME("(%p)->(showcmd=%x)\n",This, iShowCmd);
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnGetIconLocation(IShellLinkA * iface, LPSTR pszIconPath,INT cchIconPath,INT *piIcon)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(%p len=%u iicon=%p)\n", This, pszIconPath, cchIconPath, piIcon);

	lstrcpynA( pszIconPath, This->sIcoPath ? This->sIcoPath : "", cchIconPath );
	*piIcon = This->iIcoNdx;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetIconLocation(IShellLinkA * iface, LPCSTR pszIconPath,INT iIcon)
{
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(path=%s iicon=%u)\n",This, pszIconPath, iIcon);

	if (This->sIcoPath)
	    HeapFree(GetProcessHeap(), 0, This->sIcoPath);
	if (!(This->sIcoPath = heap_strdup(pszIconPath)))
	    return E_OUTOFMEMORY;
	This->iIcoNdx = iIcon;

	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetRelativePath(IShellLinkA * iface, LPCSTR pszPathRel, DWORD dwReserved)
{
	ICOM_THIS(IShellLinkImpl, iface);

	FIXME("(%p)->(path=%s %lx)\n",This, pszPathRel, dwReserved);
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnResolve(IShellLinkA * iface, HWND hwnd, DWORD fFlags)
{
	ICOM_THIS(IShellLinkImpl, iface);

	FIXME("(%p)->(hwnd=%x flags=%lx)\n",This, hwnd, fFlags);
	return NOERROR;
}
static HRESULT WINAPI IShellLinkA_fnSetPath(IShellLinkA * iface, LPCSTR pszFile)
{
	char buffer[MAX_PATH];
	int ret = 0;
	size_t len;
	ICOM_THIS(IShellLinkImpl, iface);

	TRACE("(%p)->(path=%s)\n",This, pszFile);

	/* FIXME: If we can get the LongPathName of the file, great. If it fails, 
	 * most likely that is because the file doesn't exist (yet). In Windows,
	 * it seems like we can create links to files that don't exist, the
	 * actual icon gets resolved when you click on the start menu (or something
	 * like that) This behaviour was observed with the StarCraft installer, 
	 * where the icon to the game's .exe was being created prior to copying the
	 * file to the C: drive.  This is a bit of a hack, because the icons that
	 * will be created will be the default "Spike" icon rather than, for example,
	 * the Starcraft icon, but that I guess is better than nothing being created at
	 * all. A slightly more inteligent way might be to use a mechanism such as
	 * atexit() to defer the extraction of the game's icon from the .exe file and
	 * creation of the icon in the start menu until the end of the program, at 
	 * which time the files should be in the right place. */
	ret = GetLongPathNameA(pszFile, buffer, sizeof(buffer));

	if (This->sPath)
	    HeapFree(GetProcessHeap(), 0, This->sPath);

	if (ret)
	{
	    if (!(This->sPath = heap_strdup(buffer)))
	        return E_OUTOFMEMORY;
	}
	else
	{
	    if (!(This->sPath = heap_strdup(pszFile)))
	        return E_OUTOFMEMORY;
	}

	len = strlen (This->sPath);
	if ((len > 1) && (This->sPath[0] == '\"') &&
	    (This->sPath[len - 1] == '\"'))
	{
	    memmove (This->sPath, This->sPath + 1, len);
	    This->sPath[len - 2] = '\0';
	}

	return NOERROR;
}

/**************************************************************************
* IShellLink Implementation
*/

static ICOM_VTABLE(IShellLinkA) slvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IShellLinkA_fnQueryInterface,
	IShellLinkA_fnAddRef,
	IShellLinkA_fnRelease,
	IShellLinkA_fnGetPath,
	IShellLinkA_fnGetIDList,
	IShellLinkA_fnSetIDList,
	IShellLinkA_fnGetDescription,
	IShellLinkA_fnSetDescription,
	IShellLinkA_fnGetWorkingDirectory,
	IShellLinkA_fnSetWorkingDirectory,
	IShellLinkA_fnGetArguments,
	IShellLinkA_fnSetArguments,
	IShellLinkA_fnGetHotkey,
	IShellLinkA_fnSetHotkey,
	IShellLinkA_fnGetShowCmd,
	IShellLinkA_fnSetShowCmd,
	IShellLinkA_fnGetIconLocation,
	IShellLinkA_fnSetIconLocation,
	IShellLinkA_fnSetRelativePath,
	IShellLinkA_fnResolve,
	IShellLinkA_fnSetPath
};


/**************************************************************************
 *  IShellLinkW_fnQueryInterface
 */
static HRESULT WINAPI IShellLinkW_fnQueryInterface(
  IShellLinkW * iface, REFIID riid, LPVOID *ppvObj)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	return IShellLinkA_QueryInterface((IShellLinkA*)This, riid, ppvObj);
}

/******************************************************************************
 * IShellLinkW_fnAddRef
 */
static ULONG WINAPI IShellLinkW_fnAddRef(IShellLinkW * iface)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(count=%lu)\n",This,This->ref);

	return IShellLinkA_AddRef((IShellLinkA*)This);
}
/******************************************************************************
 * IShellLinkW_fnRelease
 */

static ULONG WINAPI IShellLinkW_fnRelease(IShellLinkW * iface)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(count=%lu)\n",This,This->ref);

	return IShellLinkA_Release((IShellLinkA*)This);
}

static HRESULT WINAPI IShellLinkW_fnGetPath(IShellLinkW * iface, LPWSTR pszFile,INT cchMaxPath, WIN32_FIND_DATAA *pfd, DWORD fFlags)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(pfile=%p len=%u find_data=%p flags=%lu)\n",This, pszFile, cchMaxPath, pfd, fFlags);
        MultiByteToWideChar( CP_ACP, 0, "c:\\foo.bar", -1, pszFile, cchMaxPath );
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetIDList(IShellLinkW * iface, LPITEMIDLIST * ppidl)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(ppidl=%p)\n",This, ppidl);
	*ppidl = _ILCreateDesktop();
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetIDList(IShellLinkW * iface, LPCITEMIDLIST pidl)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(pidl=%p)\n",This, pidl);
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetDescription(IShellLinkW * iface, LPWSTR pszName,INT cchMaxName)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(%p len=%u)\n",This, pszName, cchMaxName);
        MultiByteToWideChar( CP_ACP, 0, "Description, FIXME", -1, pszName, cchMaxName );
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetDescription(IShellLinkW * iface, LPCWSTR pszName)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(desc=%s)\n",This, debugstr_w(pszName));

	if (This->sDescription)
	    HeapFree(GetProcessHeap(), 0, This->sDescription);
	if (!(This->sDescription = HEAP_strdupWtoA(GetProcessHeap(), 0, pszName)))
	    return E_OUTOFMEMORY;

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetWorkingDirectory(IShellLinkW * iface, LPWSTR pszDir,INT cchMaxPath)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(%p len %u)\n", This, pszDir, cchMaxPath);

	MultiByteToWideChar( CP_ACP, 0, This->sWorkDir ? This->sWorkDir : "", -1, pszDir, cchMaxPath );

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetWorkingDirectory(IShellLinkW * iface, LPCWSTR pszDir)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(dir=%s)\n",This, debugstr_w(pszDir));

	if (This->sWorkDir)
	    HeapFree(GetProcessHeap(), 0, This->sWorkDir);
	if (!(This->sWorkDir = FILE_strdupWtoA(GetProcessHeap(), 0, pszDir)))
	    return E_OUTOFMEMORY;

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetArguments(IShellLinkW * iface, LPWSTR pszArgs,INT cchMaxPath)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(%p len=%u)\n", This, pszArgs, cchMaxPath);

	MultiByteToWideChar( CP_ACP, 0, This->sArgs ? This->sArgs : "", -1, pszArgs, cchMaxPath );

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetArguments(IShellLinkW * iface, LPCWSTR pszArgs)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(args=%s)\n",This, debugstr_w(pszArgs));

	if (This->sArgs)
	    HeapFree(GetProcessHeap(), 0, This->sArgs);
	if (!(This->sArgs = HEAP_strdupWtoA(GetProcessHeap(), 0, pszArgs)))
	    return E_OUTOFMEMORY;

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetHotkey(IShellLinkW * iface, WORD *pwHotkey)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(%p)\n",This, pwHotkey);
	*pwHotkey=0x0;
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetHotkey(IShellLinkW * iface, WORD wHotkey)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(hotkey=%x)\n",This, wHotkey);
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetShowCmd(IShellLinkW * iface, INT *piShowCmd)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(%p)\n",This, piShowCmd);
	*piShowCmd=0;
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetShowCmd(IShellLinkW * iface, INT iShowCmd)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(showcmd=%x)\n",This, iShowCmd);
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnGetIconLocation(IShellLinkW * iface, LPWSTR pszIconPath,INT cchIconPath,INT *piIcon)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(%p len=%u iicon=%p)\n", This, pszIconPath, cchIconPath, piIcon);

        MultiByteToWideChar( CP_ACP, 0, This->sIcoPath ? This->sIcoPath : "", -1, pszIconPath, cchIconPath );
	*piIcon = This->iIcoNdx;

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetIconLocation(IShellLinkW * iface, LPCWSTR pszIconPath,INT iIcon)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(path=%s iicon=%u)\n",This, debugstr_w(pszIconPath), iIcon);

	if (This->sIcoPath)
	    HeapFree(GetProcessHeap(), 0, This->sIcoPath);
	if (!(This->sIcoPath = FILE_strdupWtoA(GetProcessHeap(), 0, pszIconPath)))
	    return E_OUTOFMEMORY;
	This->iIcoNdx = iIcon;

	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetRelativePath(IShellLinkW * iface, LPCWSTR pszPathRel, DWORD dwReserved)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(path=%s %lx)\n",This, debugstr_w(pszPathRel), dwReserved);
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnResolve(IShellLinkW * iface, HWND hwnd, DWORD fFlags)
{
	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	FIXME("(%p)->(hwnd=%x flags=%lx)\n",This, hwnd, fFlags);
	return NOERROR;
}

static HRESULT WINAPI IShellLinkW_fnSetPath(IShellLinkW * iface, LPCWSTR pszFile)
{
	WCHAR buffer[MAX_PATH];
	int ret = 0;
	size_t len;

	_ICOM_THIS_From_IShellLinkW(IShellLinkImpl, iface);

	TRACE("(%p)->(path=%s)\n",This, debugstr_w(pszFile));
	
	/* FIXME: If we can get the LongPathName of the file, great. If it fails, 
	 * most likely that is because the file doesn't exist (yet). In Windows,
	 * it seems like we can create links to files that don't exist, the
	 * actual icon gets resolved when you click on the start menu (or something
	 * like that) This behaviour was observed with the StarCraft installer, 
	 * where the icon to the game's .exe was being created prior to copying the
	 * file to the C: drive.  This is a bit of a hack, because the icons that
	 * will be created will be the default "Spike" icon rather than, for example,
	 * the Starcraft icon, but that I guess is better than nothing being created at
	 * all. A slightly more inteligent way might be to use a mechanism such as
	 * atexit() to defer the extraction of the game's icon from the .exe file and
	 * creation of the icon in the start menu until the end of the program, at 
	 * which time the files should be in the right place. */
        ret = GetLongPathNameW( pszFile, buffer, sizeof(buffer));

	if (This->sPath)
	    HeapFree(GetProcessHeap(), 0, This->sPath);
	if (ret)
	{
	    if (!(This->sPath = FILE_strdupWtoA(GetProcessHeap(), 0, buffer)))
	        return E_OUTOFMEMORY;
	}
	else
	{
	    if (!(This->sPath = FILE_strdupWtoA(GetProcessHeap(), 0, pszFile)))
	        return E_OUTOFMEMORY;
	}

	len = strlen (This->sPath);
	if ((len > 1) && (This->sPath[0] == '\"') &&
	    (This->sPath[len - 1] == '\"'))
	{
	    memmove (This->sPath, This->sPath + 1, len);
	    This->sPath[len - 2] = '\0';
	}

	return NOERROR;
}

/**************************************************************************
* IShellLinkW Implementation
*/

static ICOM_VTABLE(IShellLinkW) slvtw =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IShellLinkW_fnQueryInterface,
	IShellLinkW_fnAddRef,
	IShellLinkW_fnRelease,
	IShellLinkW_fnGetPath,
	IShellLinkW_fnGetIDList,
	IShellLinkW_fnSetIDList,
	IShellLinkW_fnGetDescription,
	IShellLinkW_fnSetDescription,
	IShellLinkW_fnGetWorkingDirectory,
	IShellLinkW_fnSetWorkingDirectory,
	IShellLinkW_fnGetArguments,
	IShellLinkW_fnSetArguments,
	IShellLinkW_fnGetHotkey,
	IShellLinkW_fnSetHotkey,
	IShellLinkW_fnGetShowCmd,
	IShellLinkW_fnSetShowCmd,
	IShellLinkW_fnGetIconLocation,
	IShellLinkW_fnSetIconLocation,
	IShellLinkW_fnSetRelativePath,
	IShellLinkW_fnResolve,
	IShellLinkW_fnSetPath
};

