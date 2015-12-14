/*
 * Metafile driver initialisation functions
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "windef.h"
#include "wine/winbase16.h"
#include "gdi.h"
#include "metafile.h"
#include "metafiledrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(metafile);

#include <string.h>

static const DC_FUNCTIONS MFDRV_Funcs =
{
    NULL,                            /* pAbortDoc */
    MFDRV_AbortPath,                 /* pAbortPath */
    NULL,                            /* pAngleArc */
    MFDRV_Arc,                       /* pArc */
    NULL,                            /* pArcTo */
    MFDRV_BeginPath,                 /* pBeginPath */
    MFDRV_BitBlt,                    /* pBitBlt */
    NULL,                            /* pBitmapBits */
    NULL,                            /* pChoosePixelFormat */
    MFDRV_Chord,                     /* pChord */
    MFDRV_CloseFigure,               /* pCloseFigure */
    NULL,                            /* pCreateBitmap */
    NULL, /* no implementation */    /* pCreateDC */
    NULL,                            /* pCreateDIBSection */
    NULL, /* no implementation */    /* pDeleteDC */
    NULL,                            /* pDeleteObject */
    NULL,                            /* pDescribePixelFormat */
    NULL,                            /* pDeviceCapabilities */
    MFDRV_Ellipse,                   /* pEllipse */
    NULL,                            /* pEndDoc */
    NULL,                            /* pEndPage */
    MFDRV_EndPath,                   /* pEndPath */
    NULL,                            /* pEnumDeviceFonts */
    MFDRV_ExcludeClipRect,           /* pExcludeClipRect */
    NULL,                            /* pExtDeviceMode */
    NULL,                            /* pExtEscape */
    MFDRV_ExtFloodFill,              /* pExtFloodFill */
    MFDRV_ExtTextOut,                /* pExtTextOut */
    MFDRV_FillPath,                  /* pFillPath */
    MFDRV_FillRgn,                   /* pFillRgn */
    MFDRV_FlattenPath,               /* pFlattenPath */
    MFDRV_FrameRgn,                  /* pFrameRgn */
    NULL,                            /* pGetCharWidth */
    NULL,                            /* pGetDCOrgEx */
    NULL,                            /* pGetDeviceCaps */
    NULL,                            /* pGetDeviceGammaRamp */
    NULL, /* no implementation */    /* pGetPixel */
    NULL,                            /* pGetPixelFormat */
    NULL,                            /* pGetTextExtentPoint */
    NULL,                            /* pGetTextMetrics */
    MFDRV_IntersectClipRect,         /* pIntersectClipRect */
    MFDRV_InvertRgn,                 /* pInvertRgn */
    MFDRV_LineTo,                    /* pLineTo */
    MFDRV_MoveTo,                    /* pMoveTo */
    MFDRV_OffsetClipRgn,             /* pOffsetClipRgn */
    MFDRV_OffsetViewportOrg,         /* pOffsetViewportOrg */
    MFDRV_OffsetWindowOrg,           /* pOffsetWindowOrg */
    MFDRV_PaintRgn,                  /* pPaintRgn */
    MFDRV_PatBlt,                    /* pPatBlt */
    MFDRV_Pie,                       /* pPie */
    MFDRV_PolyBezier,                /* pPolyBezier */
    MFDRV_PolyBezierTo,              /* pPolyBezierTo */
    NULL,                            /* pPolyDraw */
    MFDRV_PolyPolygon,               /* pPolyPolygon */
    NULL,                            /* pPolyPolyline */
    MFDRV_Polygon,                   /* pPolygon */
    MFDRV_Polyline,                  /* pPolyline */
    NULL,                            /* pPolylineTo */
    NULL,                            /* pRealizePalette */
    MFDRV_Rectangle,                 /* pRectangle */
    MFDRV_RestoreDC,                 /* pRestoreDC */
    MFDRV_RoundRect,                 /* pRoundRect */
    MFDRV_SaveDC,                    /* pSaveDC */
    MFDRV_ScaleViewportExt,          /* pScaleViewportExt */
    MFDRV_ScaleWindowExt,            /* pScaleWindowExt */
    MFDRV_SelectClipPath,            /* pSelectClipPath */
    NULL,                            /* pSelectClipRgn */
    MFDRV_SelectObject,              /* pSelectObject */
    NULL,                            /* pSelectPalette */
    MFDRV_SetBkColor,                /* pSetBkColor */
    MFDRV_SetBkMode,                 /* pSetBkMode */
    NULL,                            /* pSetDeviceClipping */
    NULL,                            /* pSetDeviceGammaRamp */
    MFDRV_SetDIBitsToDevice,         /* pSetDIBitsToDevice */
    MFDRV_SetMapMode,                /* pSetMapMode */
    MFDRV_SetMapperFlags,            /* pSetMapperFlags */
    MFDRV_SetPixel,                  /* pSetPixel */
    NULL,                            /* pSetPixelFormat */
    MFDRV_SetPolyFillMode,           /* pSetPolyFillMode */
    MFDRV_SetROP2,                   /* pSetROP2 */
    MFDRV_SetRelAbs,                 /* pSetRelAbs */
    MFDRV_SetStretchBltMode,         /* pSetStretchBltMode */
    MFDRV_SetTextAlign,              /* pSetTextAlign */
    MFDRV_SetTextCharacterExtra,     /* pSetTextCharacterExtra */
    MFDRV_SetTextColor,              /* pSetTextColor */
    MFDRV_SetTextJustification,      /* pSetTextJustification */
    MFDRV_SetViewportExt,            /* pSetViewportExt */
    MFDRV_SetViewportOrg,            /* pSetViewportOrg */
    MFDRV_SetWindowExt,              /* pSetWindowExt */
    MFDRV_SetWindowOrg,              /* pSetWindowOrg */
    NULL,                            /* pStartDoc */
    NULL,                            /* pStartPage */
    MFDRV_StretchBlt,                /* pStretchBlt */
    MFDRV_StretchDIBits,             /* pStretchDIBits */
    MFDRV_StrokeAndFillPath,         /* pStrokeAndFillPath */
    MFDRV_StrokePath,                /* pStrokePath */
    NULL,                            /* pSwapBuffers */
    MFDRV_WidenPath                  /* pWidenPath */
};



/**********************************************************************
 *	     MFDRV_AllocMetaFile
 */
static DC *MFDRV_AllocMetaFile(void)
{
    DC *dc;
    METAFILEDRV_PDEVICE *physDev;

    if (!(dc = DC_AllocDC( &MFDRV_Funcs ))) return NULL;
    dc->header.wMagic = METAFILE_DC_MAGIC;

    physDev = (METAFILEDRV_PDEVICE *)HeapAlloc(GetProcessHeap(),0,sizeof(*physDev));
    if (!physDev)
    {
        GDI_FreeObject( dc->hSelf, dc );
        return NULL;
    }
    dc->physDev = physDev;

    if (!(physDev->mh = HeapAlloc( GetProcessHeap(), 0, sizeof(*physDev->mh) )))
    {
        HeapFree( GetProcessHeap(), 0, physDev );
        GDI_FreeObject( dc->hSelf, dc );
        return NULL;
    }

    physDev->nextHandle = 0;
    physDev->hFile = 0;

    physDev->mh->mtHeaderSize   = sizeof(METAHEADER) / sizeof(WORD);
    physDev->mh->mtVersion      = 0x0300;
    physDev->mh->mtSize         = physDev->mh->mtHeaderSize;
    physDev->mh->mtNoObjects    = 0;
    physDev->mh->mtMaxRecord    = 0;
    physDev->mh->mtNoParameters = 0;

    return dc;
}


/**********************************************************************
 *	     MFDRV_DeleteDC
 */
static BOOL MFDRV_DeleteDC( DC *dc )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dc->physDev;

    if (physDev->mh) HeapFree( GetProcessHeap(), 0, physDev->mh );
    HeapFree( GetProcessHeap(), 0, physDev );
    dc->physDev = NULL;
    GDI_FreeObject( dc->hSelf, dc );
    return TRUE;
}

/**********************************************************************
 *	     CreateMetaFile     (GDI.125)
 *	     CreateMetaFile16   (GDI32.@)
 *
 *  Create a new DC and associate it with a metafile. Pass a filename
 *  to create a disk-based metafile, NULL to create a memory metafile.
 *
 * RETURNS
 *  A handle to the metafile DC if successful, NULL on failure.
 */
HDC16 WINAPI CreateMetaFile16(
			      LPCSTR filename /* [in] Filename of disk metafile */
)
{
    HDC ret;
    DC *dc;
    METAFILEDRV_PDEVICE *physDev;
    HFILE hFile;

    TRACE("'%s'\n", filename );

    if (!(dc = MFDRV_AllocMetaFile())) return 0;
    physDev = (METAFILEDRV_PDEVICE *)dc->physDev;

    if (filename)  /* disk based metafile */
    {
        physDev->mh->mtType = METAFILE_DISK;
        if ((hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, 0, 0)) == INVALID_HANDLE_VALUE) {
            MFDRV_DeleteDC( dc );
            return 0;
        }
        if (!WriteFile( hFile, (LPSTR)physDev->mh, sizeof(*physDev->mh), NULL,
			NULL )) {
            MFDRV_DeleteDC( dc );
            return 0;
	}
	physDev->hFile = hFile;

	/* Grow METAHEADER to include filename */
	physDev->mh = MF_CreateMetaHeaderDisk(physDev->mh, filename);
    }
    else  /* memory based metafile */
	physDev->mh->mtType = METAFILE_MEMORY;

    TRACE("returning %04x\n", dc->hSelf);
    ret = dc->hSelf;
    GDI_ReleaseObj( dc->hSelf );
    return ret;
}

/**********************************************************************
 *	     CreateMetaFileA   (GDI32.@)
 */
HDC WINAPI CreateMetaFileA(
			      LPCSTR filename /* [in] Filename of disk metafile */
)
{
  return CreateMetaFile16( filename );
}

/**********************************************************************
 *          CreateMetaFileW   (GDI32.@)
 */
HDC WINAPI CreateMetaFileW(LPCWSTR filename)
{
    LPSTR filenameA;
    DWORD len;
    HDC hReturnDC;

    len = WideCharToMultiByte( CP_ACP, 0, filename, -1, NULL, 0, NULL, NULL );
    filenameA = HeapAlloc( GetProcessHeap(), 0, len );
    WideCharToMultiByte( CP_ACP, 0, filename, -1, filenameA, len, NULL, NULL );

    hReturnDC = CreateMetaFileA(filenameA);

    HeapFree( GetProcessHeap(), 0, filenameA );

    return hReturnDC;
}


/**********************************************************************
 *          MFDRV_CloseMetaFile
 */
static DC *MFDRV_CloseMetaFile( HDC hdc )
{
    DC *dc;
    METAFILEDRV_PDEVICE *physDev;

    TRACE("(%04x)\n", hdc );

    if (!(dc = (DC *) GDI_GetObjPtr( hdc, METAFILE_DC_MAGIC ))) return 0;
    physDev = (METAFILEDRV_PDEVICE *)dc->physDev;

    /* Construct the end of metafile record - this is documented
     * in SDK Knowledgebase Q99334.
     */

    if (!MFDRV_MetaParam0(dc, META_EOF))
    {
        MFDRV_DeleteDC( dc );
	return 0;
    }

    if (physDev->mh->mtType == METAFILE_DISK)  /* disk based metafile */
    {
        if (SetFilePointer(physDev->hFile, 0, NULL, FILE_BEGIN) != 0) {
            MFDRV_DeleteDC( dc );
            return 0;
        }

	physDev->mh->mtType = METAFILE_MEMORY; /* This is what windows does */
        if (!WriteFile(physDev->hFile, (LPSTR)physDev->mh,
                       sizeof(*physDev->mh), NULL, NULL)) {
            MFDRV_DeleteDC( dc );
            return 0;
        }
        CloseHandle(physDev->hFile);
	physDev->mh->mtType = METAFILE_DISK;
    }

    return dc;
}

/******************************************************************
 *	     CloseMetaFile     (GDI.126)
 *	     CloseMetaFile16   (GDI32.@)
 */
HMETAFILE16 WINAPI CloseMetaFile16(
				   HDC16 hdc /* [in] Metafile DC to close */
)
{
    HMETAFILE16 hmf;
    METAFILEDRV_PDEVICE *physDev;
    DC *dc = MFDRV_CloseMetaFile(hdc);
    if (!dc) return 0;
    physDev = (METAFILEDRV_PDEVICE *)dc->physDev;

    /* Now allocate a global handle for the metafile */

    hmf = MF_Create_HMETAFILE16( physDev->mh );

    physDev->mh = NULL;  /* So it won't be deleted */
    MFDRV_DeleteDC( dc );
    return hmf;
}

/******************************************************************
 *	     CloseMetaFile   (GDI32.@)
 *
 *  Stop recording graphics operations in metafile associated with
 *  hdc and retrieve metafile.
 *
 * RETURNS
 *  Handle of newly created metafile on success, NULL on failure.
 */
HMETAFILE WINAPI CloseMetaFile(
				   HDC hdc /* [in] Metafile DC to close */
)
{
    HMETAFILE hmf;
    METAFILEDRV_PDEVICE *physDev;
    DC *dc = MFDRV_CloseMetaFile(hdc);
    if (!dc) return 0;
    physDev = (METAFILEDRV_PDEVICE *)dc->physDev;

    /* Now allocate a global handle for the metafile */

    hmf = MF_Create_HMETAFILE( physDev->mh );

    physDev->mh = NULL;  /* So it won't be deleted */
    MFDRV_DeleteDC( dc );
    return hmf;
}


/******************************************************************
 *         MFDRV_WriteRecord
 *
 * Warning: this function can change the pointer to the metafile header.
 */
BOOL MFDRV_WriteRecord( DC *dc, METARECORD *mr, DWORD rlen)
{
    DWORD len;
    METAHEADER *mh;
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dc->physDev;

    switch(physDev->mh->mtType)
    {
    case METAFILE_MEMORY:
	len = physDev->mh->mtSize * 2 + rlen;
	mh = HeapReAlloc( GetProcessHeap(), 0, physDev->mh, len );
        if (!mh) return FALSE;
        physDev->mh = mh;
	memcpy((WORD *)physDev->mh + physDev->mh->mtSize, mr, rlen);
        break;
    case METAFILE_DISK:
        TRACE("Writing record to disk\n");
	if (!WriteFile(physDev->hFile, (char *)mr, rlen, NULL, NULL))
	    return FALSE;
        break;
    default:
        ERR("Unknown metafile type %d\n", physDev->mh->mtType );
        return FALSE;
    }

    physDev->mh->mtSize += rlen / 2;
    physDev->mh->mtMaxRecord = max(physDev->mh->mtMaxRecord, rlen / 2);
    return TRUE;
}


/******************************************************************
 *         MFDRV_MetaParam0
 */

BOOL MFDRV_MetaParam0(DC *dc, short func)
{
    char buffer[8];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = 3;
    mr->rdFunction = func;
    return MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
}


/******************************************************************
 *         MFDRV_MetaParam1
 */
BOOL MFDRV_MetaParam1(DC *dc, short func, short param1)
{
    char buffer[8];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = 4;
    mr->rdFunction = func;
    *(mr->rdParm) = param1;
    return MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
}


/******************************************************************
 *         MFDRV_MetaParam2
 */
BOOL MFDRV_MetaParam2(DC *dc, short func, short param1, short param2)
{
    char buffer[10];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = 5;
    mr->rdFunction = func;
    *(mr->rdParm) = param2;
    *(mr->rdParm + 1) = param1;
    return MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
}


/******************************************************************
 *         MFDRV_MetaParam4
 */

BOOL MFDRV_MetaParam4(DC *dc, short func, short param1, short param2,
		      short param3, short param4)
{
    char buffer[14];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = 7;
    mr->rdFunction = func;
    *(mr->rdParm) = param4;
    *(mr->rdParm + 1) = param3;
    *(mr->rdParm + 2) = param2;
    *(mr->rdParm + 3) = param1;
    return MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
}


/******************************************************************
 *         MFDRV_MetaParam6
 */

BOOL MFDRV_MetaParam6(DC *dc, short func, short param1, short param2,
		      short param3, short param4, short param5, short param6)
{
    char buffer[18];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = 9;
    mr->rdFunction = func;
    *(mr->rdParm) = param6;
    *(mr->rdParm + 1) = param5;
    *(mr->rdParm + 2) = param4;
    *(mr->rdParm + 3) = param3;
    *(mr->rdParm + 4) = param2;
    *(mr->rdParm + 5) = param1;
    return MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
}


/******************************************************************
 *         MFDRV_MetaParam8
 */
BOOL MFDRV_MetaParam8(DC *dc, short func, short param1, short param2,
		      short param3, short param4, short param5,
		      short param6, short param7, short param8)
{
    char buffer[22];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = 11;
    mr->rdFunction = func;
    *(mr->rdParm) = param8;
    *(mr->rdParm + 1) = param7;
    *(mr->rdParm + 2) = param6;
    *(mr->rdParm + 3) = param5;
    *(mr->rdParm + 4) = param4;
    *(mr->rdParm + 5) = param3;
    *(mr->rdParm + 6) = param2;
    *(mr->rdParm + 7) = param1;
    return MFDRV_WriteRecord( dc, mr, mr->rdSize * 2);
}


/******************************************************************
 *         MFDRV_AddHandleDC
 *
 * Note: this function assumes that we never delete objects.
 * If we do someday, we'll need to maintain a table to re-use deleted
 * handles.
 */
int MFDRV_AddHandleDC( DC *dc )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dc->physDev;
    physDev->mh->mtNoObjects++;
    return physDev->nextHandle++;
}


