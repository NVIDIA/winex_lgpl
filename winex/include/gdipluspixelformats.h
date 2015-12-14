

#ifndef _GDIPLUSPIXELFORMATS_H
#define _GDIPLUSPIXELFORMATS_H

typedef DWORD ARGB;
typedef INT PixelFormat;

#define    PixelFormatIndexed   0x00010000
#define    PixelFormatGDI       0x00020000
#define    PixelFormatAlpha     0x00040000
#define    PixelFormatPAlpha    0x00080000
#define    PixelFormatExtended  0x00100000
#define    PixelFormatCanonical 0x00200000

#define    PixelFormatUndefined 0
#define    PixelFormatDontCare  0

#define    PixelFormat1bppIndexed       (1 | ( 1 << 8) | PixelFormatIndexed | PixelFormatGDI)
#define    PixelFormat4bppIndexed       (2 | ( 4 << 8) | PixelFormatIndexed | PixelFormatGDI)
#define    PixelFormat8bppIndexed       (3 | ( 8 << 8) | PixelFormatIndexed | PixelFormatGDI)
#define    PixelFormat16bppGrayScale    (4 | (16 << 8) | PixelFormatExtended)
#define    PixelFormat16bppRGB555       (5 | (16 << 8) | PixelFormatGDI)
#define    PixelFormat16bppRGB565       (6 | (16 << 8) | PixelFormatGDI)
#define    PixelFormat16bppARGB1555     (7 | (16 << 8) | PixelFormatAlpha | PixelFormatGDI)
#define    PixelFormat24bppRGB          (8 | (24 << 8) | PixelFormatGDI)
#define    PixelFormat32bppRGB          (9 | (32 << 8) | PixelFormatGDI)
#define    PixelFormat32bppARGB         (10 | (32 << 8) | PixelFormatAlpha | PixelFormatGDI | PixelFormatCanonical)
#define    PixelFormat32bppPARGB        (11 | (32 << 8) | PixelFormatAlpha | PixelFormatPAlpha | PixelFormatGDI)
#define    PixelFormat48bppRGB          (12 | (48 << 8) | PixelFormatExtended)
#define    PixelFormat64bppARGB         (13 | (64 << 8) | PixelFormatAlpha  | PixelFormatCanonical | PixelFormatExtended)
#define    PixelFormat64bppPARGB        (14 | (64 << 8) | PixelFormatAlpha  | PixelFormatPAlpha | PixelFormatExtended)
#define    PixelFormatMax               15

#ifdef __cplusplus

struct ColorPalette
{
public:
    UINT Flags;
    UINT Count;
    ARGB Entries[1];
};

#else 

typedef struct ColorPalette
{
    UINT Flags;
    UINT Count;
    ARGB Entries[1];
} ColorPalette;

#endif  

#endif
