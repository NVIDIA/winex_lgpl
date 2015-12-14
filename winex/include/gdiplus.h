

#ifndef _GDIPLUS_H
#define _GDIPLUS_H

#ifdef __cplusplus

namespace Gdiplus
{
    namespace DllExports
    {
#include "gdiplusmem.h"
    };

#include "gdiplustypes.h"
#include "gdiplusenums.h"
#include "gdiplusinit.h"
#include "gdipluspixelformats.h"
#include "gdiplusmetaheader.h"
#include "gdiplusimaging.h"
#include "gdipluscolor.h"
#include "gdipluscolormatrix.h"
#include "gdiplusgpstubs.h"

    namespace DllExports
    {
#include "gdiplusflat.h"
    };
};

#else 

#include "gdiplusmem.h"

#include "gdiplustypes.h"
#include "gdiplusenums.h"
#include "gdiplusinit.h"
#include "gdipluspixelformats.h"
#include "gdiplusmetaheader.h"
#include "gdiplusimaging.h"
#include "gdipluscolor.h"
#include "gdipluscolormatrix.h"
#include "gdiplusgpstubs.h"

#include "gdiplusflat.h"

#endif 

#endif 
