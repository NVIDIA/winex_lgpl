/*
 * Metafile definitions
 *
 * Copyright  David W. Metcalfe, 1994
 */

#ifndef __WINE_METAFILE_H
#define __WINE_METAFILE_H

#include "gdi.h"
#include "windef.h"
#include "wingdi.h"

  /* GDI32 metafile object */
typedef struct
{
    GDIOBJHDR   header;
    METAHEADER  *mh;
} METAFILEOBJ;

#define META_EOF 0x0000

/* values of mtType in METAHEADER.  Note however that the disk image of a disk
   based metafile has mtType == 1 */
#define METAFILE_MEMORY 1
#define METAFILE_DISK   2

extern HMETAFILE MF_Create_HMETAFILE(METAHEADER *mh);
extern HMETAFILE16 MF_Create_HMETAFILE16(METAHEADER *mh);
extern METAHEADER *MF_CreateMetaHeaderDisk(METAHEADER *mr, LPCSTR filename);

#endif   /* __WINE_METAFILE_H */

