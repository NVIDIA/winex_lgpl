#ifndef _GDIPLUSCOLORMATRIX_H
#define _GDIPLUSCOLORMATRIX_H

struct ColorMatrix
{
    REAL m[5][5];
};

enum ColorMatrixFlags
{
    ColorMatrixFlagsDefault    = 0,
    ColorMatrixFlagsSkipGrays  = 1,
    ColorMatrixFlagsAltGray    = 2
};

enum ColorAdjustType
{
    ColorAdjustTypeDefault,
    ColorAdjustTypeBitmap,
    ColorAdjustTypeBrush,
    ColorAdjustTypePen,
    ColorAdjustTypeText,
    ColorAdjustTypeCount,
    ColorAdjustTypeAny
};

struct ColorMap
{
    Color oldColor;
    Color newColor;
};

#ifndef __cplusplus

typedef enum ColorAdjustType ColorAdjustType;
typedef enum ColorMatrixFlags ColorMatrixFlags;
typedef struct ColorMatrix ColorMatrix;
typedef struct ColorMap ColorMap;

#endif

#endif  /* _GDIPLUSCOLORMATRIX_H */
