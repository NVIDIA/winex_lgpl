#ifndef _WINGDI_
#define _WINGDI_
#ifndef NOGDI

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ABCFLOAT {
    FLOAT   abcfA;
    FLOAT   abcfB;
    FLOAT   abcfC;
} ABCFLOAT, *PABCFLOAT, *LPABCFLOAT;

#define FONTMAPPER_MAX 10

typedef struct
{
    WORD   wFirst;
    WORD   wSecond;
    INT  iKernAmount;
} KERNINGPAIR, *LPKERNINGPAIR;

typedef struct tagPIXELFORMATDESCRIPTOR {
    WORD  nSize;
    WORD  nVersion;
    DWORD dwFlags;
    BYTE  iPixelType;
    BYTE  cColorBits;
    BYTE  cRedBits;
    BYTE  cRedShift;
    BYTE  cGreenBits;
    BYTE  cGreenShift;
    BYTE  cBlueBits;
    BYTE  cBlueShift;
    BYTE  cAlphaBits;
    BYTE  cAlphaShift;
    BYTE  cAccumBits;
    BYTE  cAccumRedBits;
    BYTE  cAccumGreenBits;
    BYTE  cAccumBlueBits;
    BYTE  cAccumAlphaBits;
    BYTE  cDepthBits;
    BYTE  cStencilBits;
    BYTE  cAuxBuffers;
    BYTE  iLayerType;
    BYTE  bReserved;
    DWORD dwLayerMask;
    DWORD dwVisibleMask;
    DWORD dwDamageMask;
} PIXELFORMATDESCRIPTOR, *PPIXELFORMATDESCRIPTOR, *LPPIXELFORMATDESCRIPTOR;

#define PFD_TYPE_RGBA        0
#define PFD_TYPE_COLORINDEX  1

#define PFD_MAIN_PLANE       0
#define PFD_OVERLAY_PLANE    1
#define PFD_UNDERLAY_PLANE   (-1)

#define PFD_DOUBLEBUFFER          0x00000001
#define PFD_STEREO                0x00000002
#define PFD_DRAW_TO_WINDOW        0x00000004
#define PFD_DRAW_TO_BITMAP        0x00000008
#define PFD_SUPPORT_GDI           0x00000010
#define PFD_SUPPORT_OPENGL        0x00000020
#define PFD_GENERIC_FORMAT        0x00000040
#define PFD_NEED_PALETTE          0x00000080
#define PFD_NEED_SYSTEM_PALETTE   0x00000100
#define PFD_SWAP_EXCHANGE         0x00000200
#define PFD_SWAP_COPY             0x00000400
#define PFD_SWAP_LAYER_BUFFERS    0x00000800
#define PFD_GENERIC_ACCELERATED   0x00001000

#define PFD_DEPTH_DONTCARE        0x20000000
#define PFD_DOUBLEBUFFER_DONTCARE 0x40000000
#define PFD_STEREO_DONTCARE       0x80000000

typedef struct tagCOLORADJUSTMENT
{
	WORD   caSize;
	WORD   caFlags;
	WORD   caIlluminantIndex;
	WORD   caRedGamma;
	WORD   caGreenGamma;
	WORD   caBlueGamma;
	WORD   caReferenceBlack;
	WORD   caReferenceWhite;
	SHORT  caContrast;
	SHORT  caBrightness;
	SHORT  caColorfulness;
	SHORT  caRedGreenTint;
} COLORADJUSTMENT, *PCOLORADJUSTMENT, *LPCOLORADJUSTMENT;

#define CA_NEGATIVE          0x0001
#define CA_LOG_FILTER        0x0002

#define ILLUMINANT_DEVICE_DEFAULT   0
#define ILLUMINANT_A                1
#define ILLUMINANT_B                2
#define ILLUMINANT_C                3
#define ILLUMINANT_D50              4
#define ILLUMINANT_D55              5
#define ILLUMINANT_D65              6
#define ILLUMINANT_D75              7
#define ILLUMINANT_F2               8
#define ILLUMINANT_MAX_INDEX        ILLUMINANT_F2

#define ILLUMINANT_TUNGSTEN         ILLUMINANT_A
#define ILLUMINANT_DAYLIGHT         ILLUMINANT_C
#define ILLUMINANT_FLUORESCENT      ILLUMINANT_F2
#define ILLUMINANT_NTSC             ILLUMINANT_C

#define RGB_GAMMA_MIN               (WORD)02500
#define RGB_GAMMA_MAX               (WORD)65000

#define REFERENCE_WHITE_MIN         (WORD)6000
#define REFERENCE_WHITE_MAX         (WORD)10000
#define REFERENCE_BLACK_MIN         (WORD)0
#define REFERENCE_BLACK_MAX         (WORD)4000

#define COLOR_ADJ_MIN               ((SHORT)-100)
#define COLOR_ADJ_MAX               (SHORT) 100

typedef LONG FXPT16DOT16, *LPFXPT16DOT16;
typedef LONG FXPT2DOT30, *LPFXPT2DOT30;
typedef LONG LCSCSTYPE;
typedef LONG LCSGAMUTMATCH;

#define LCS_CALIBRATED_RGB    0x00000000L
#define LCS_DEVICE_RGB        0x00000001L
#define LCS_DEVICE_CMYK       0x00000002L

#define LCS_GM_BUSINESS       0x00000001L
#define LCS_GM_GRAPHICS       0x00000002L
#define LCS_GM_IMAGES         0x00000004L

#define CM_OUT_OF_GAMUT       255
#define CM_IN_GAMUT           0

typedef struct tagCIEXYZ
{
  FXPT2DOT30 ciexyzX;
  FXPT2DOT30 ciexyzY;
  FXPT2DOT30 ciexyzZ;
} CIEXYZ, *LPCIEXYZ;

typedef struct tagCIEXYZTRIPLE
{
  CIEXYZ ciexyzRed;
  CIEXYZ ciexyzGreen;
  CIEXYZ ciexyzBlue;
} CIEXYZTRIPLE, *LPCIEXYZTRIPLE;

typedef struct tagLOGCOLORSPACEA
{
  DWORD lcsSignature;
  DWORD lcsVersion;
  DWORD lcsSize;
  LCSCSTYPE lcsCSType;
  LCSGAMUTMATCH lcsIntent;
  CIEXYZTRIPLE lcsEndpoints;
  DWORD lcsGammaRed;
  DWORD lcsGammaGreen;
  DWORD lcsGammaBlue;
  CHAR lcsFilename[MAX_PATH];
} LOGCOLORSPACEA, *LPLOGCOLORSPACEA;

 typedef struct tagLOGCOLORSPACEW
{
  DWORD lcsSignature;
  DWORD lcsVersion;
  DWORD lcsSize;
  LCSCSTYPE lcsCSType;
  LCSGAMUTMATCH lcsIntent;
  CIEXYZTRIPLE lcsEndpoints;
  DWORD lcsGammaRed;
  DWORD lcsGammaGreen;
  DWORD lcsGammaBlue;
  WCHAR lcsFilename[MAX_PATH];
} LOGCOLORSPACEW, *LPLOGCOLORSPACEW;

DECL_WINELIB_TYPE_AW(LPLOGCOLORSPACE)
DECL_WINELIB_TYPE_AW(LOGCOLORSPACE)

#define DC_FIELDS		1
#define DC_PAPERS		2
#define DC_PAPERSIZE		3
#define DC_MINEXTENT		4
#define DC_MAXEXTENT		5
#define DC_BINS			6
#define DC_DUPLEX		7
#define DC_SIZE			8
#define DC_EXTRA		9
#define DC_VERSION		10
#define DC_DRIVER		11
#define DC_BINNAMES		12
#define DC_ENUMRESOLUTIONS	13
#define DC_FILEDEPENDENCIES	14
#define DC_TRUETYPE		15
#define DC_PAPERNAMES		16
#define DC_ORIENTATION		17
#define DC_COPIES		18
#define DC_BINADJUST            19
#define DC_EMF_COMPLIANT        20
#define DC_DATATYPE_PRODUCED    21
#define DC_COLLATE              22
#define DC_MANUFACTURER         23
#define DC_MODEL                24
#define DC_PERSONALITY          25
#define DC_PRINTRATE            26
#define DC_PRINTRATEUNIT        27
#define DC_PRINTERMEM           28
#define DC_MEDIAREADY           29
#define DC_STAPLE               30
#define DC_PRINTRATEPPM         31
#define DC_COLORDEVICE          32
#define DC_NUP                  33

#define DCTT_BITMAP             0x00000001L
#define DCTT_DOWNLOAD           0x00000002L
#define DCTT_SUBDEV             0x00000004L
#define DCTT_DOWNLOAD_OUTLINE   0x00000008L

#define DCBA_FACEUPNONE         0x0000
#define DCBA_FACEUPCENTER       0x0001
#define DCBA_FACEUPLEFT         0x0002
#define DCBA_FACEUPRIGHT        0x0003
#define DCBA_FACEDOWNNONE       0x0100
#define DCBA_FACEDOWNCENTER     0x0101
#define DCBA_FACEDOWNLEFT       0x0102
#define DCBA_FACEDOWNRIGHT      0x0103

#define PRINTRATEUNIT_PPM       1
#define PRINTRATEUNIT_CPS       2
#define PRINTRATEUNIT_LPM       3
#define PRINTRATEUNIT_IPM       4

/* Flag returned from Escape QUERYDIBSUPPORT */
#define	QDI_SETDIBITS		1
#define	QDI_GETDIBITS		2
#define	QDI_DIBTOSCREEN		4
#define	QDI_STRETCHDIB		8


/* GDI Escape commands */
#define	NEWFRAME		1
#define	ABORTDOC		2
#define	NEXTBAND		3
#define	SETCOLORTABLE		4
#define	GETCOLORTABLE		5
#define	FLUSHOUTPUT		6
#define	DRAFTMODE		7
#define	QUERYESCSUPPORT		8
#define	SETABORTPROC		9
#define	STARTDOC		10
#define	ENDDOC			11
#define	GETPHYSPAGESIZE		12
#define	GETPRINTINGOFFSET	13
#define	GETSCALINGFACTOR	14
#define	MFCOMMENT		15
#define	GETPENWIDTH		16
#define	SETCOPYCOUNT		17
#define	SELECTPAPERSOURCE	18
#define	DEVICEDATA		19
#define	PASSTHROUGH		19
#define	GETTECHNOLGY		20
#define	GETTECHNOLOGY		20 /* yes, both of them */
#define	SETLINECAP		21
#define	SETLINEJOIN		22
#define	SETMITERLIMIT		23
#define	BANDINFO		24
#define	DRAWPATTERNRECT		25
#define	GETVECTORPENSIZE	26
#define	GETVECTORBRUSHSIZE	27
#define	ENABLEDUPLEX		28
#define	GETSETPAPERBINS		29
#define	GETSETPRINTORIENT	30
#define	ENUMPAPERBINS		31
#define	SETDIBSCALING		32
#define	EPSPRINTING		33
#define	ENUMPAPERMETRICS	34
#define	GETSETPAPERMETRICS	35
#define	POSTSCRIPT_DATA		37
#define	POSTSCRIPT_IGNORE	38
#define	MOUSETRAILS		39
#define	GETDEVICEUNITS		42

#define DESKTOPVERTRES          117
#define DESKTOPHORZRES          118

#define	GETEXTENDEDTEXTMETRICS	256
#define	GETEXTENTTABLE		257
#define	GETPAIRKERNTABLE	258
#define	GETTRACKKERNTABLE	259
#define	EXTTEXTOUT		512
#define	GETFACENAME		513
#define	DOWNLOADFACE		514
#define	ENABLERELATIVEWIDTHS	768
#define	ENABLEPAIRKERNING	769
#define	SETKERNTRACK		770
#define	SETALLJUSTVALUES	771
#define	SETCHARSET		772

#define	STRETCHBLT		2048
#define	GETSETSCREENPARAMS	3072
#define	QUERYDIBSUPPORT		3073
#define	BEGIN_PATH		4096
#define	CLIP_TO_PATH		4097
#define	END_PATH		4098
#define	EXT_DEVICE_CAPS		4099
#define	RESTORE_CTM		4100
#define	SAVE_CTM		4101
#define	SET_ARC_DIRECTION	4102
#define	SET_BACKGROUND_COLOR	4103
#define	SET_POLY_MODE		4104
#define	SET_SCREEN_ANGLE	4105
#define	SET_SPREAD		4106
#define	TRANSFORM_CTM		4107
#define	SET_CLIP_BOX		4108
#define	SET_BOUNDS		4109
#define	SET_MIRROR_MODE		4110
#define	OPENCHANNEL		4110
#define	DOWNLOADHEADER		4111
#define CLOSECHANNEL		4112
#define	POSTSCRIPT_PASSTHROUGH	4115
#define	ENCAPSULATED_POSTSCRIPT	4116
#define	POSTSCRIPT_IDENTIFY	4117
#define	POSTSCRIPT_INJECTION	4118

/* for POSTSCRIPT_IDENTIFY */
#define PSIDENT_GDICENTRIC	0
#define PSIDENT_PSCENTRIC	1


#define QDI_SETDIBITS           1
#define QDI_GETDIBITS           2
#define QDI_DIBTOSCREEN         4
#define QDI_STRETCHDIB          8

/* Spooler Error Codes */
#define	SP_NOTREPORTED	0x4000
#define	SP_ERROR	(-1)
#define	SP_APPABORT	(-2)
#define	SP_USERABORT	(-3)
#define	SP_OUTOFDISK	(-4)
#define	SP_OUTOFMEMORY	(-5)

#define PR_JOBSTATUS     0

  /* Raster operations */

#define R2_BLACK         1
#define R2_NOTMERGEPEN   2
#define R2_MASKNOTPEN    3
#define R2_NOTCOPYPEN    4
#define R2_MASKPENNOT    5
#define R2_NOT           6
#define R2_XORPEN        7
#define R2_NOTMASKPEN    8
#define R2_MASKPEN       9
#define R2_NOTXORPEN    10
#define R2_NOP          11
#define R2_MERGENOTPEN  12
#define R2_COPYPEN      13
#define R2_MERGEPENNOT  14
#define R2_MERGEPEN     15
#define R2_WHITE        16

#define SRCCOPY         0xcc0020
#define SRCPAINT        0xee0086
#define SRCAND          0x8800c6
#define SRCINVERT       0x660046
#define SRCERASE        0x440328
#define NOTSRCCOPY      0x330008
#define NOTSRCERASE     0x1100a6
#define MERGECOPY       0xc000ca
#define MERGEPAINT      0xbb0226
#define PATCOPY         0xf00021
#define PATPAINT        0xfb0a09
#define PATINVERT       0x5a0049
#define DSTINVERT       0x550009
#define BLACKNESS       0x000042
#define WHITENESS       0xff0062

  /* StretchBlt() modes */
#define BLACKONWHITE         1
#define WHITEONBLACK         2
#define COLORONCOLOR	     3
#define HALFTONE             4
#define MAXSTRETCHBLTMODE    4

#define STRETCH_ANDSCANS     BLACKONWHITE
#define STRETCH_ORSCANS      WHITEONBLACK
#define STRETCH_DELETESCANS  COLORONCOLOR
#define STRETCH_HALFTONE     HALFTONE

  /* Colors */

#define RGB(r,g,b)          ((COLORREF)((r) | ((g) << 8) | ((b) << 16)))
#define PALETTERGB(r,g,b)   (0x02000000 | RGB(r,g,b))
#define PALETTEINDEX(i)     ((COLORREF)(0x01000000 | (WORD)(i)))

#define GetRValue(rgb)	    ((rgb) & 0xff)
#define GetGValue(rgb)      (((rgb) >> 8) & 0xff)
#define GetBValue(rgb)	    (((rgb) >> 16) & 0xff)

#define GetKValue(cmyk)     ((BYTE)  (cmyk) )
#define GetYValue(cmyk)     ((BYTE) ((cymk) >> 8))
#define GetMValue(cmyk)     ((BYTE) ((cymk) >> 16))
#define GetCValue(cmyk)     ((BYTE) ((cymk) >> 24))

#define CMYK(c,m,y,k)       ((COLOREF)((((BYTE)(k)|((WORD)((BYTE)(y))<<8))|(((DWORD)(BYTE)(m))<<16))|(((DWORD)(BYTE)(c))<<24)))


#define ICM_OFF   1
#define ICM_ON    2
#define ICM_QUERY 3

  /* Bounds Accumulation APIs */
#define DCB_RESET       0x0001
#define DCB_ACCUMULATE  0x0002
#define DCB_DIRTY       DCB_ACCUMULATE
#define DCB_SET         (DCB_RESET | DCB_ACCUMULATE)
#define DCB_ENABLE      0x0004
#define DCB_DISABLE     0x0008

typedef struct
{
    LONG paXCount;
    LONG paYCount;
    LONG paXExt;
    LONG paYExt;
    BYTE paRGBs;
} PELARRAY, *PPELARRAY, *LPPELARRAY;

  /* Bitmaps */

typedef struct
{
    INT  bmType;
    INT  bmWidth;
    INT  bmHeight;
    INT  bmWidthBytes;
    WORD   bmPlanes;
    WORD   bmBitsPixel;
    LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;


  /* Brushes */

typedef struct
{
    UINT     lbStyle;
    COLORREF   lbColor;
    INT      lbHatch;
} LOGBRUSH, *PLOGBRUSH, *LPLOGBRUSH;

typedef LOGBRUSH PATTERN, *PPATTERN, *LPPATTERN;


  /* Brush styles */
#define BS_SOLID	    0
#define BS_NULL		    1
#define BS_HOLLOW	    1
#define BS_HATCHED	    2
#define BS_PATTERN	    3
#define BS_INDEXED	    4
#define	BS_DIBPATTERN	    5
#define	BS_DIBPATTERNPT	    6
#define BS_PATTERN8X8	    7
#define	BS_DIBPATTERN8X8    8
#define BS_MONOPATTERN      9

  /* Hatch styles */
#define HS_HORIZONTAL       0
#define HS_VERTICAL         1
#define HS_FDIAGONAL        2
#define HS_BDIAGONAL        3
#define HS_CROSS            4
#define HS_DIAGCROSS        5

  /* Fonts */

#define LF_FACESIZE     32
#define LF_FULLFACESIZE 64

#define RASTER_FONTTYPE     0x0001
#define DEVICE_FONTTYPE     0x0002
#define TRUETYPE_FONTTYPE   0x0004

typedef struct
{
    LONG   lfHeight;
    LONG   lfWidth;
    LONG   lfEscapement;
    LONG   lfOrientation;
    LONG   lfWeight;
    BYTE   lfItalic;
    BYTE   lfUnderline;
    BYTE   lfStrikeOut;
    BYTE   lfCharSet;
    BYTE   lfOutPrecision;
    BYTE   lfClipPrecision;
    BYTE   lfQuality;
    BYTE   lfPitchAndFamily;
    CHAR   lfFaceName[LF_FACESIZE];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;

typedef struct
{
    LONG   lfHeight;
    LONG   lfWidth;
    LONG   lfEscapement;
    LONG   lfOrientation;
    LONG   lfWeight;
    BYTE   lfItalic;
    BYTE   lfUnderline;
    BYTE   lfStrikeOut;
    BYTE   lfCharSet;
    BYTE   lfOutPrecision;
    BYTE   lfClipPrecision;
    BYTE   lfQuality;
    BYTE   lfPitchAndFamily;
    WCHAR  lfFaceName[LF_FACESIZE];
} LOGFONTW, *PLOGFONTW, *LPLOGFONTW;

DECL_WINELIB_TYPE_AW(LOGFONT)
DECL_WINELIB_TYPE_AW(PLOGFONT)
DECL_WINELIB_TYPE_AW(LPLOGFONT)

typedef struct
{
  LOGFONTA elfLogFont;
  BYTE       elfFullName[LF_FULLFACESIZE];
  BYTE       elfStyle[LF_FACESIZE];
} ENUMLOGFONTA, *LPENUMLOGFONTA;

typedef struct
{
  LOGFONTW elfLogFont;
  WCHAR      elfFullName[LF_FULLFACESIZE];
  WCHAR      elfStyle[LF_FACESIZE];
} ENUMLOGFONTW, *LPENUMLOGFONTW;

DECL_WINELIB_TYPE_AW(ENUMLOGFONT)
DECL_WINELIB_TYPE_AW(LPENUMLOGFONT)

typedef struct
{
  LOGFONTA elfLogFont;
  BYTE       elfFullName[LF_FULLFACESIZE];
  BYTE       elfStyle[LF_FACESIZE];
  BYTE       elfScript[LF_FACESIZE];
} ENUMLOGFONTEXA,*LPENUMLOGFONTEXA;

typedef struct
{
  LOGFONTW elfLogFont;
  WCHAR      elfFullName[LF_FULLFACESIZE];
  WCHAR      elfStyle[LF_FACESIZE];
  WCHAR      elfScript[LF_FACESIZE];
} ENUMLOGFONTEXW,*LPENUMLOGFONTEXW;

DECL_WINELIB_TYPE_AW(ENUMLOGFONTEX)
DECL_WINELIB_TYPE_AW(LPENUMLOGFONTEX)

/*
 * The FONTSIGNATURE tells which Unicode ranges and which code pages
 * have glyphs in a font.
 *
 * fsUsb  128-bit bitmap. The most significant bits are 10 (magic number).
 *        The remaining 126 bits map the Unicode ISO 10646 subranges
 *        for which the font provides glyphs.
 *
 * fsCsb  64-bit bitmap. The low 32 bits map the Windows codepages for
 *        which the font provides glyphs. The high 32 bits are for
 *        non Windows codepages.
 */
typedef struct
{
  DWORD fsUsb[4];
  DWORD fsCsb[2];
} FONTSIGNATURE, *PFONTSIGNATURE, *LPFONTSIGNATURE;

typedef struct
{
  UINT	ciCharset; /* character set */
  UINT	ciACP; /* ANSI code page */
  FONTSIGNATURE	fs;
} CHARSETINFO, *PCHARSETINFO, *LPCHARSETINFO;

/* Flags for TranslateCharsetInfo */
#define TCI_SRCCHARSET    1
#define TCI_SRCCODEPAGE   2
#define TCI_SRCFONTSIG    3

typedef struct
{
    DWORD lsUsb[4];
    DWORD lsCsbDefault[2];
    DWORD lsCsbSupported[2];
} LOCALESIGNATURE, *PLOCALESIGNATUR, *LPLOCALESIGNATUREE;


/* Flags for ModifyWorldTransform */
#define MWT_IDENTITY      1
#define MWT_LEFTMULTIPLY  2
#define MWT_RIGHTMULTIPLY 3
#define MWT_MIN           MWT_IDENTITY
#define MWT_MAX           MWT_RIGHTMULTIPLY

/* Object Definitions for EnumObjects() */
#define OBJ_PEN             1
#define OBJ_BRUSH           2
#define OBJ_DC              3
#define OBJ_METADC          4
#define OBJ_PAL             5
#define OBJ_FONT            6
#define OBJ_BITMAP          7
#define OBJ_REGION          8
#define OBJ_METAFILE        9
#define OBJ_MEMDC           10
#define OBJ_EXTPEN          11
#define OBJ_ENHMETADC       12
#define OBJ_ENHMETAFILE     13

typedef struct
{
    FLOAT  eM11;
    FLOAT  eM12;
    FLOAT  eM21;
    FLOAT  eM22;
    FLOAT  eDx;
    FLOAT  eDy;
} XFORM, *PXFORM, *LPXFORM;

  /* lfWeight values */
#define FW_DONTCARE	    0
#define FW_THIN 	    100
#define FW_EXTRALIGHT	    200
#define FW_ULTRALIGHT	    200
#define FW_LIGHT	    300
#define FW_NORMAL	    400
#define FW_REGULAR	    400
#define FW_MEDIUM	    500
#define FW_SEMIBOLD	    600
#define FW_DEMIBOLD	    600
#define FW_BOLD 	    700
#define FW_EXTRABOLD	    800
#define FW_ULTRABOLD	    800
#define FW_HEAVY	    900
#define FW_BLACK	    900

  /* lfCharSet values */
#define ANSI_CHARSET	      (BYTE)0   /* CP1252, ansi-0, iso8859-{1,15} */
#define DEFAULT_CHARSET       (BYTE)1
#define SYMBOL_CHARSET	      (BYTE)2
#define SHIFTJIS_CHARSET      (BYTE)128 /* CP932 */
#define HANGEUL_CHARSET       (BYTE)129 /* CP949, ksc5601.1987-0 */
#define HANGUL_CHARSET        HANGEUL_CHARSET
#define GB2312_CHARSET        (BYTE)134 /* CP936, gb2312.1980-0 */
#define CHINESEBIG5_CHARSET   (BYTE)136 /* CP950, big5.et-0 */
#define GREEK_CHARSET         (BYTE)161	/* CP1253 */
#define TURKISH_CHARSET       (BYTE)162	/* CP1254, -iso8859-9 */
#define VIETNAMESE_CHARSET    (BYTE)163
#define HEBREW_CHARSET        (BYTE)177	/* CP1255, -iso8859-8 */
#define ARABIC_CHARSET        (BYTE)178	/* CP1256, -iso8859-6 */
#define BALTIC_CHARSET        (BYTE)186	/* CP1257, -iso8859-13 */
#define RUSSIAN_CHARSET       (BYTE)204	/* CP1251, -iso8859-5 */
#define EE_CHARSET	      (BYTE)238	/* CP1250, -iso8859-2 */
#define EASTEUROPE_CHARSET    EE_CHARSET
#define THAI_CHARSET	      (BYTE)222 /* CP874, iso8859-11, tis620 */
#define JOHAB_CHARSET         (BYTE)130 /* korean (johab) CP1361 */
#define MAC_CHARSET           (BYTE)77
#define OEM_CHARSET	      (BYTE)255
/* I don't know if the values of *_CHARSET macros are defined in Windows
 * or if we can choose them as we want. -- srtxg
 */
#define VISCII_CHARSET        (BYTE)240 /* viscii1.1-1 */
#define TCVN_CHARSET          (BYTE)241 /* tcvn-0 */
#define KOI8_CHARSET          (BYTE)242 /* koi8-{r,u,ru} */
#define ISO3_CHARSET          (BYTE)243 /* iso8859-3 */
#define ISO4_CHARSET          (BYTE)244 /* iso8859-4 */
#define ISO10_CHARSET         (BYTE)245 /* iso8859-10 */
#define CELTIC_CHARSET        (BYTE)246 /* iso8859-14 */

#define FS_LATIN1              0x00000001L
#define FS_LATIN2              0x00000002L
#define FS_CYRILLIC            0x00000004L
#define FS_GREEK               0x00000008L
#define FS_TURKISH             0x00000010L
#define FS_HEBREW              0x00000020L
#define FS_ARABIC              0x00000040L
#define FS_BALTIC              0x00000080L
#define FS_VIETNAMESE          0x00000100L
#define FS_THAI                0x00010000L
#define FS_JISJAPAN            0x00020000L
#define FS_CHINESESIMP         0x00040000L
#define FS_WANSUNG             0x00080000L
#define FS_CHINESETRAD         0x00100000L
#define FS_JOHAB               0x00200000L
#define FS_SYMBOL              0x80000000L

  /* lfOutPrecision values */
#define OUT_DEFAULT_PRECIS	0
#define OUT_STRING_PRECIS	1
#define OUT_CHARACTER_PRECIS	2
#define OUT_STROKE_PRECIS	3
#define OUT_TT_PRECIS		4
#define OUT_DEVICE_PRECIS	5
#define OUT_RASTER_PRECIS	6
#define OUT_TT_ONLY_PRECIS	7
#define OUT_OUTLINE_PRECIS      8

  /* lfClipPrecision values */
#define CLIP_DEFAULT_PRECIS     0x00
#define CLIP_CHARACTER_PRECIS   0x01
#define CLIP_STROKE_PRECIS      0x02
#define CLIP_MASK		0x0F
#define CLIP_LH_ANGLES		0x10
#define CLIP_TT_ALWAYS		0x20
#define CLIP_EMBEDDED		0x80

  /* lfQuality values */
#define DEFAULT_QUALITY        0
#define DRAFT_QUALITY          1
#define PROOF_QUALITY          2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY    4

  /* lfPitchAndFamily pitch values */
#define DEFAULT_PITCH       0x00
#define FIXED_PITCH         0x01
#define VARIABLE_PITCH      0x02
#define MONO_FONT           0x08

#define FF_DONTCARE         0x00
#define FF_ROMAN            0x10
#define FF_SWISS            0x20
#define FF_MODERN           0x30
#define FF_SCRIPT           0x40
#define FF_DECORATIVE       0x50

#ifndef _TEXTMETRIC_DEFINED
#define _TEXTMETRIC_DEFINED

typedef struct
{
    LONG      tmHeight;
    LONG      tmAscent;
    LONG      tmDescent;
    LONG      tmInternalLeading;
    LONG      tmExternalLeading;
    LONG      tmAveCharWidth;
    LONG      tmMaxCharWidth;
    LONG      tmWeight;
    LONG      tmOverhang;
    LONG      tmDigitizedAspectX;
    LONG      tmDigitizedAspectY;
    BYTE      tmFirstChar;
    BYTE      tmLastChar;
    BYTE      tmDefaultChar;
    BYTE      tmBreakChar;
    BYTE      tmItalic;
    BYTE      tmUnderlined;
    BYTE      tmStruckOut;
    BYTE      tmPitchAndFamily;
    BYTE      tmCharSet;
} TEXTMETRICA, *LPTEXTMETRICA, *PTEXTMETRICA;

typedef struct
{
    LONG      tmHeight;
    LONG      tmAscent;
    LONG      tmDescent;
    LONG      tmInternalLeading;
    LONG      tmExternalLeading;
    LONG      tmAveCharWidth;
    LONG      tmMaxCharWidth;
    LONG      tmWeight;
    LONG      tmOverhang;
    LONG      tmDigitizedAspectX;
    LONG      tmDigitizedAspectY;
    WCHAR     tmFirstChar;
    WCHAR     tmLastChar;
    WCHAR     tmDefaultChar;
    WCHAR     tmBreakChar;
    BYTE      tmItalic;
    BYTE      tmUnderlined;
    BYTE      tmStruckOut;
    BYTE      tmPitchAndFamily;
    BYTE      tmCharSet;
} TEXTMETRICW, *LPTEXTMETRICW, *PTEXTMETRICW;

DECL_WINELIB_TYPE_AW(TEXTMETRIC)
DECL_WINELIB_TYPE_AW(PTEXTMETRIC)
DECL_WINELIB_TYPE_AW(LPTEXTMETRIC)
#endif /* _TEXTMETRIC_DEFINED */


typedef struct tagPANOSE
{
    BYTE bFamilyType;
    BYTE bSerifStyle;
    BYTE bWeight;
    BYTE bProportion;
    BYTE bContrast;
    BYTE bStrokeVariation;
    BYTE bArmStyle;
    BYTE bLetterform;
    BYTE bMidline;
    BYTE bXHeight;
} PANOSE, *LPPANOSE;

#define PANOSE_COUNT                   10

#define PAN_FAMILYTYPE_INDEX           0
#define PAN_SERIFSTYLE_INDEX           1
#define PAN_WEIGTH_INDEX               2
#define PAN_PROPORTION_INDEX           3
#define PAN_CONTRAST_INDEX             4
#define PAN_STROKEVARIATION_INDEX      5
#define PAN_ARMSTYLE_INDEX             6
#define PAN_LETTERFORM_INDEX           7
#define PAN_MIDLINE_INDEX              8
#define PAN_XHEIGHT_INDEX              9

#define PAN_CULTURE_LATIN              0

#define PAN_ANY                        0
#define PAN_NO_FIT                     1

#define PAN_FAMILY_TEXT_DISPLAY        2
#define PAN_FAMILY_SCRIPT              3
#define PAN_FAMILY_DECORATIVE          4
#define PAN_FAMILY_PICTORIAL           5

#define PAN_SERIF_COVE                 2
#define PAN_SERIF_OBTUSE_COVE          3
#define PAN_SERIF_SQUARE_COVE          4
#define PAN_SERIF_OBTUSE_SQUARE_COVE   5
#define PAN_SERIF_SQUARE               6
#define PAN_SERIF_THIN                 7
#define PAN_SERIF_BONE                 8
#define PAN_SERIF_EXAGGERATED          9
#define PAN_SERIF_TRIANGLE             10
#define PAN_SERIF_NORMAL_SANS          11
#define PAN_SERIF_OBTUSE_SANS          12
#define PAN_SERIF_PERP_SANS            13
#define PAN_SERIF_FLARED               14
#define PAN_SERIF_ROUNDED              15

#define PAN_WEIGHT_VERY_LIGHT          2
#define PAN_WEIGHT_LIGHT               3
#define PAN_WEIGHT_THIN                4
#define PAN_WEIGHT_BOOK                5
#define PAN_WEIGHT_MEDIUM              6
#define PAN_WEIGHT_DEMI                7
#define PAN_WEIGHT_BOLD                8
#define PAN_WEIGHT_HEAVY               9
#define PAN_WEIGHT_BLACK               10
#define PAN_WEIGHT_NORD                11

#define PAN_PROP_OLD_STYLE             2
#define PAN_PROP_MODERN                3
#define PAN_PROP_EVEN_WIDTH            4
#define PAN_PROP_EXPANDED              5
#define PAN_PROP_CONDENSED             6
#define PAN_PROP_VERY_EXPANDED         7
#define PAN_PROP_VERY_CONDENSED        8
#define PAN_PROP_MONOSPACED            9

#define PAN_CONTRAST_NONE              2
#define PAN_CONTRAST_VERY_LOW          3
#define PAN_CONTRAST_LOW               4
#define PAN_CONTRAST_MEDIUM_LOW        5
#define PAN_CONTRAST_MEDIUM            6
#define PAN_CONTRAST_MEDIUM_HIGH       7
#define PAN_CONTRAST_HIGH              8
#define PAN_CONTRAST_VERY_HIGH         9

#define PAN_STROKE_GRADUAL_DIAG        2
#define PAN_STROKE_GRADUAL_TRAN        3
#define PAN_STROKE_GRADUAL_VERT        4
#define PAN_STROKE_GRADUAL_HORZ        5
#define PAN_STROKE_RAPID_VERT          6
#define PAN_STROKE_RAPID_HORZ          7
#define PAN_STROKE_INSTANT_VERT        8

#define PAN_STRAIGHT_ARMS_HORZ         2
#define PAN_STRAIGHT_ARMS_WEDGE        3
#define PAN_STRAIGHT_ARMS_VERT         4
#define PAN_STRAIGHT_ARMS_SINGLE_SERIF 5
#define PAN_STRAIGHT_ARMS_DOUBLE_SERIF 6
#define PAN_BENT_ARMS_HORZ             7
#define PAN_BENT_ARMS_WEDGE            8
#define PAN_BENT_ARMS_VERT             9
#define PAN_BENT_ARMS_SINGLE_SERIF     10
#define PAN_BENT_ARMS_DOUBLE_SERIF     11

#define PAN_LETT_NORMAL_COMPACT        2
#define PAN_LETT_NORMAL_WEIGHTED       3
#define PAN_LETT_NORMAL_BOXED          4
#define PAN_LETT_NORMAL_FLATTENED      5
#define PAN_LETT_NORMAL_ROUNDED        6
#define PAN_LETT_NORMAL_OFF_CENTER     7
#define PAN_LETT_NORMAL_SQUARE         8
#define PAN_LETT_OBLIQUE_COMPACT       9
#define PAN_LETT_OBLIQUE_WEIGHTED      10
#define PAN_LETT_OBLIQUE_BOXED         11
#define PAN_LETT_OBLIQUE_FLATTENED     12
#define PAN_LETT_OBLIQUE_ROUNDED       13
#define PAN_LETT_OBLIQUE_OFF_CENTER    14
#define PAN_LETT_OBLIQUE_SQUARE        15

#define PAN_MIDLINE_STANDARD_TRIMMED   2
#define PAN_MIDLINE_STANDARD_POINTED   3
#define PAN_MIDLINE_STANDARD_SERIFED   4
#define PAN_MIDLINE_HIGH_TRIMMED       5
#define PAN_MIDLINE_HIGH_POINTED       6
#define PAN_MIDLINE_HIGH_SERIFED       7
#define PAN_MIDLINE_CONSTANT_TRIMMED   8
#define PAN_MIDLINE_CONSTANT_POINTED   9
#define PAN_MIDLINE_CONSTANT_SERIFED   10
#define PAN_MIDLINE_LOW_TRIMMED        11
#define PAN_MIDLINE_LOW_POINTED        12
#define PAN_MIDLINE_LOW_SERIFED        13

#define PAN_XHEIGHT_CONSTANT_SMALL     2
#define PAN_XHEIGHT_CONSTANT_STANDARD  3
#define PAN_XHEIGHT_CONSTANT_LARGE     4
#define PAN_XHEIGHT_DUCKING_SMALL      5
#define PAN_XHEIGHT_DUCKING_STANDARD   6
#define PAN_XHEIGHT_DUCKING_LARGE      7

#define ELF_VENDOR_SIZE 4
typedef struct
{
  LOGFONTA    elfLogFont;
  BYTE        elfFullName[LF_FULLFACESIZE];
  BYTE        elfStyle[LF_FACESIZE];
  DWORD       elfVersion;
  DWORD       elfStyleSize;
  DWORD       elfMatch;
  DWORD       elfReserved;
  BYTE        elfVendorId[ELF_VENDOR_SIZE];
  DWORD       elfCulture;
  PANOSE      elfPanose;
} EXTLOGFONTA, *PEXTLOGFONTA, *LPEXTLOGFONTA;

typedef struct
{
  LOGFONTW    elfLogFont;
  WCHAR       elfFullName[LF_FULLFACESIZE];
  WCHAR       elfStyle[LF_FACESIZE];
  DWORD       elfVersion;
  DWORD       elfStyleSize;
  DWORD       elfMatch;
  DWORD       elfReserved;
  BYTE        elfVendorId[ELF_VENDOR_SIZE];
  DWORD       elfCulture;
  PANOSE      elfPanose;
} EXTLOGFONTW, *PEXTLOGFONTW, *LPEXTLOGFONTW;

DECL_WINELIB_TYPE_AW(EXTLOGFONT)
DECL_WINELIB_TYPE_AW(PEXTLOGFONT)
DECL_WINELIB_TYPE_AW(LPEXTLOGFONT)

#define ELF_VERSION         0
#define ELF_CULTURE_LATIN   0

typedef struct _OUTLINETEXTMETRICA
{
    UINT          otmSize;
    TEXTMETRICA   otmTextMetrics;
    BYTE            otmFiller;
    PANOSE          otmPanoseNumber;
    UINT          otmfsSelection;
    UINT          otmfsType;
    INT           otmsCharSlopeRise;
    INT           otmsCharSlopeRun;
    INT           otmItalicAngle;
    UINT          otmEMSquare;
    INT           otmAscent;
    INT           otmDescent;
    UINT          otmLineGap;
    UINT          otmsCapEmHeight;
    UINT          otmsXHeight;
    RECT          otmrcFontBox;
    INT           otmMacAscent;
    INT           otmMacDescent;
    UINT          otmMacLineGap;
    UINT          otmusMinimumPPEM;
    POINT         otmptSubscriptSize;
    POINT         otmptSubscriptOffset;
    POINT         otmptSuperscriptSize;
    POINT         otmptSuperscriptOffset;
    UINT          otmsStrikeoutSize;
    INT           otmsStrikeoutPosition;
    INT           otmsUnderscoreSize;
    INT           otmsUnderscorePosition;
    LPSTR           otmpFamilyName;
    LPSTR           otmpFaceName;
    LPSTR           otmpStyleName;
    LPSTR           otmpFullName;
} OUTLINETEXTMETRICA, *POUTLINETEXTMETRICA, *LPOUTLINETEXTMETRICA;

typedef struct _OUTLINETEXTMETRICW
{
    UINT          otmSize;
    TEXTMETRICW   otmTextMetrics;
    BYTE            otmFiller;
    PANOSE          otmPanoseNumber;
    UINT          otmfsSelection;
    UINT          otmfsType;
    INT           otmsCharSlopeRise;
    INT           otmsCharSlopeRun;
    INT           otmItalicAngle;
    UINT          otmEMSquare;
    INT           otmAscent;
    INT           otmDescent;
    UINT          otmLineGap;
    UINT          otmsCapEmHeight;
    UINT          otmsXHeight;
    RECT          otmrcFontBox;
    INT           otmMacAscent;
    INT           otmMacDescent;
    UINT          otmMacLineGap;
    UINT          otmusMinimumPPEM;
    POINT         otmptSubscriptSize;
    POINT         otmptSubscriptOffset;
    POINT         otmptSuperscriptSize;
    POINT         otmptSuperscriptOffset;
    UINT          otmsStrikeoutSize;
    INT           otmsStrikeoutPosition;
    INT           otmsUnderscoreSize;
    INT           otmsUnderscorePosition;
    LPSTR           otmpFamilyName;
    LPSTR           otmpFaceName;
    LPSTR           otmpStyleName;
    LPSTR           otmpFullName;
} OUTLINETEXTMETRICW, *POUTLINETEXTMETRICW, *LPOUTLINETEXTMETRICW;

DECL_WINELIB_TYPE_AW(OUTLINETEXTMETRIC)
DECL_WINELIB_TYPE_AW(POUTLINETEXTMETRIC)
DECL_WINELIB_TYPE_AW(LPOUTLINETEXTMETRIC)

typedef struct
{
    INT       x;
    INT       y;
    UINT      n;
    LPCSTR    lpstr;
    UINT      uiFlags;
    RECT      rcl;
    INT       *pdx;
} POLYTEXTA, *PPOLYTEXTA, *LPPOLYTEXTA;

typedef struct
{
    INT       x;
    INT       y;
    UINT      n;
    LPCWSTR   lpstr;
    UINT      uiFlags;
    RECT      rcl;
    INT       *pdx;
} POLYTEXTW, *PPOLYTEXTW, *LPPOLYTEXTW;

DECL_WINELIB_TYPE_AW(POLYTEXT)
DECL_WINELIB_TYPE_AW(PPOLYTEXT)
DECL_WINELIB_TYPE_AW(LPPOLYTEXT)


/* ntmFlags field flags */
#define NTM_REGULAR     0x00000040L
#define NTM_BOLD        0x00000020L
#define NTM_ITALIC      0x00000001L

typedef struct
{
    LONG      tmHeight;
    LONG      tmAscent;
    LONG      tmDescent;
    LONG      tmInternalLeading;
    LONG      tmExternalLeading;
    LONG      tmAveCharWidth;
    LONG      tmMaxCharWidth;
    LONG      tmWeight;
    LONG      tmOverhang;
    LONG      tmDigitizedAspectX;
    LONG      tmDigitizedAspectY;
    BYTE      tmFirstChar;
    BYTE      tmLastChar;
    BYTE      tmDefaultChar;
    BYTE      tmBreakChar;
    BYTE      tmItalic;
    BYTE      tmUnderlined;
    BYTE      tmStruckOut;
    BYTE      tmPitchAndFamily;
    BYTE      tmCharSet;
    DWORD     ntmFlags;
    UINT    ntmSizeEM;
    UINT    ntmCellHeight;
    UINT    ntmAvgWidth;
} NEWTEXTMETRICA, *PNEWTEXTMETRICA, *LPNEWTEXTMETRICA;

typedef struct
{
    LONG      tmHeight;
    LONG      tmAscent;
    LONG      tmDescent;
    LONG      tmInternalLeading;
    LONG      tmExternalLeading;
    LONG      tmAveCharWidth;
    LONG      tmMaxCharWidth;
    LONG      tmWeight;
    LONG      tmOverhang;
    LONG      tmDigitizedAspectX;
    LONG      tmDigitizedAspectY;
    WCHAR     tmFirstChar;
    WCHAR     tmLastChar;
    WCHAR     tmDefaultChar;
    WCHAR     tmBreakChar;
    BYTE      tmItalic;
    BYTE      tmUnderlined;
    BYTE      tmStruckOut;
    BYTE      tmPitchAndFamily;
    BYTE      tmCharSet;
    DWORD     ntmFlags;
    UINT      ntmSizeEM;
    UINT      ntmCellHeight;
    UINT      ntmAvgWidth;
} NEWTEXTMETRICW, *PNEWTEXTMETRICW, *LPNEWTEXTMETRICW;

DECL_WINELIB_TYPE_AW(NEWTEXTMETRIC)
DECL_WINELIB_TYPE_AW(PNEWTEXTMETRIC)
DECL_WINELIB_TYPE_AW(LPNEWTEXTMETRIC)

typedef struct
{
    NEWTEXTMETRICA	ntmTm;
    FONTSIGNATURE       ntmFontSig;
} NEWTEXTMETRICEXA,*LPNEWTEXTMETRICEXA;

typedef struct
{
    NEWTEXTMETRICW	ntmTm;
    FONTSIGNATURE       ntmFontSig;
} NEWTEXTMETRICEXW,*LPNEWTEXTMETRICEXW;

DECL_WINELIB_TYPE_AW(NEWTEXTMETRICEX)
DECL_WINELIB_TYPE_AW(LPNEWTEXTMETRICEX)

typedef int (CALLBACK *OLDFONTENUMPROCA)(const LOGFONTA*,const TEXTMETRICA*,
                                          DWORD,LPARAM);
typedef int (CALLBACK *OLDFONTENUMPROCW)(const LOGFONTW*,const TEXTMETRICW*,
                                          DWORD,LPARAM);
DECL_WINELIB_TYPE_AW(OLDFONTENUMPROC)

typedef OLDFONTENUMPROCA FONTENUMPROCA;
typedef OLDFONTENUMPROCW FONTENUMPROCW;
DECL_WINELIB_TYPE_AW(FONTENUMPROC)

typedef int (CALLBACK *FONTENUMPROCEXA)(LPENUMLOGFONTEXA,LPNEWTEXTMETRICEXA,DWORD,LPARAM);
typedef int (CALLBACK *FONTENUMPROCEXW)(LPENUMLOGFONTEXW,LPNEWTEXTMETRICEXW,DWORD,LPARAM);
DECL_WINELIB_TYPE_AW(FONTENUMPROCEX)

typedef INT     (CALLBACK *GOBJENUMPROC)(LPVOID,LPARAM);
typedef VOID    (CALLBACK *LINEDDAPROC)(INT,INT,LPARAM);

  /* tmPitchAndFamily bits */
#define TMPF_FIXED_PITCH    1		/* means variable pitch */
#define TMPF_VECTOR	    2
#define TMPF_TRUETYPE	    4
#define TMPF_DEVICE	    8

  /* Text alignment */
#define TA_NOUPDATECP       0x00
#define TA_UPDATECP         0x01
#define TA_LEFT             0x00
#define TA_RIGHT            0x02
#define TA_CENTER           0x06
#define TA_TOP              0x00
#define TA_BOTTOM           0x08
#define TA_BASELINE         0x18
#define TA_RTLREADING        0x100
#define TA_MASK             TA_BASELINE+TA_CENTER+TA_UPDATECP+TA_RTLREADING

#define VTA_BASELINE        TA_BASELINE
#define VTA_LEFT            TA_BOTTOM
#define VTA_RIGHT           TA_TOP
#define VTA_CENTER          TA_CENTER
#define VTA_BOTTOM          TA_RIGHT
#define VTA_TOP             TA_LEFT


  /* ExtTextOut() parameters */
#define ETO_GRAYED          0x0001
#define ETO_OPAQUE          0x0002
#define ETO_CLIPPED         0x0004
#define ETO_GLYPH_INDEX     0x0010
#define ETO_RTLREADING      0x0080
#define ETO_IGNORELANGUAGE  0x1000

#define ASPECT_FILTERING    0x0001

typedef struct
{
    UINT	gmBlackBoxX;
    UINT	gmBlackBoxY;
    POINT	gmptGlyphOrigin;
    SHORT	gmCellIncX;
    SHORT	gmCellIncY;
} GLYPHMETRICS, *LPGLYPHMETRICS;


#define GGO_METRICS         0
#define GGO_BITMAP          1
#define GGO_NATIVE          2
#define GGO_GRAY2_BITMAP    4
#define GGO_GRAY4_BITMAP    5
#define GGO_GRAY8_BITMAP    6
#define GGO_GLYPH_INDEX     0x80

typedef struct _POINTFLOAT {
  FLOAT   x;
  FLOAT   y;
} POINTFLOAT, *PPOINTFLOAT;

typedef struct _GLYPHMETRICSFLOAT {
  FLOAT       gmfBlackBoxX;
  FLOAT       gmfBlackBoxY;
  POINTFLOAT  gmfptGlyphOrigin;
  FLOAT       gmfCellIncX;
  FLOAT       gmfCellIncY;
} GLYPHMETRICSFLOAT, *PGLYPHMETRICSFLOAT, *LPGLYPHMETRICSFLOAT;

typedef struct
{
    WORD    fract;
    SHORT   value;
} FIXED;

typedef struct tagPOINTFX
{
	FIXED x;
	FIXED y;
} POINTFX, *LPPOINTFX;

typedef struct tagTTPOLYCURVE
{
	WORD wType;
	WORD cpfx;
	POINTFX apfx[1];
} TTPOLYCURVE, *LPTTPOLYCURVE;

typedef struct tagTTPOLYGONHEADER
{
	DWORD cb;
	DWORD dwType;
	POINTFX pfxStart;
} TTPOLYGONHEADER, *LPTTPOLYGONHEADER;

typedef struct
{
     FIXED  eM11;
     FIXED  eM12;
     FIXED  eM21;
     FIXED  eM22;
} MAT2, *LPMAT2;

  /* for GetCharABCWidths() */
typedef struct
{
    INT   abcA;
    UINT  abcB;
    INT   abcC;
} ABC, *PABC, *LPABC;


  /* for GetCharacterPlacement () */

#define GCP_DBCS          0x0001
#define GCP_REORDER       0x0002
#define GCP_USEKERNING    0x0008
#define GCP_GLYPHSHAPE    0x0010
#define GCP_LIGATE        0x0020
#define GCP_DIACRITIC     0x0100
#define GCP_KASHIDA       0x0200
#define GCP_ERROR         0x8000
#define FLI_MASK          0x103b
#define GCP_JUSTIFY         0x00010000L
#define FLI_GLYPHS          0x00040000L
#define GCP_CLASSIN         0x00080000L
#define GCP_MAXEXTENT       0x00100000L
#define GCP_JUSTIFYIN       0x00200000L
#define GCP_DISPLAYZWG      0x00400000L
#define GCP_SYMSWAPOFF      0x00800000L
#define GCP_NUMERICOVERRIDE 0x01000000L
#define GCP_NEUTRALOVERRIDE 0x02000000L
#define GCP_NUMERICSLATIN   0x04000000L
#define GCP_NUMERICSLOCAL   0x08000000L

#define GCPCLASS_LATIN                     1
#define GCPCLASS_HEBREW                    2
#define GCPCLASS_ARABIC                    3
#define GCPCLASS_NEUTRAL                   4
#define GCPCLASS_LOCALNUMBER               5
#define GCPCLASS_LATINNUMBER               6
#define GCPCLASS_LATINNUMERICTERMINATOR    7
#define GCPCLASS_LATINNUMERICSEPARATOR     8
#define GCPCLASS_NUMERICSEPARATOR          9
#define GCPCLASS_PREBOUNDLTR               0x80
#define GCPCLASS_PREBOUNDRLT               0x40
#define GCPCLASS_POSTBOUNDLTR              0x20
#define GCPCLASS_POSTBOUNDRTL              0x10

#define GCPGLYPH_LINKBEFORE                0x8000
#define GCPGLYPH_LINKAFTER                 0x4000


typedef struct tagGCP_RESULTSA{
    DWORD  lStructSize;
    LPSTR  lpOutString;
    UINT   *lpOrder;
    INT    *lpDx;
    INT    *lpCaretPos;
    LPSTR  lpClass;
    LPWSTR lpGlyphs;
    UINT   nGlyphs;
    UINT   nMaxFit;
} GCP_RESULTSA, *LPGCP_RESULTSA;

typedef struct tagGCP_RESULTSW
{
    DWORD  lStructSize;
    LPWSTR lpOutString;
    UINT   *lpOrder;
    INT    *lpDx;
    INT    *lpCaretPos;
    LPWSTR lpClass;
    LPWSTR lpGlyphs;
    UINT   nGlyphs;
    UINT   nMaxFit;
} GCP_RESULTSW, *LPGCP_RESULTSW;

DECL_WINELIB_TYPE_AW(GCP_RESULTS)
DECL_WINELIB_TYPE_AW(LPGCP_RESULTS)

/* For GetFontUnicodeRanges */
typedef struct tagWCRANGE
{
    WCHAR wcLow;
    USHORT cGlyphs;
} WCRANGE, *PWCRANGE, *LPWCRANGE;

#define GS_8BIT_INDICES 0x1

typedef struct tagGLYPHSET {
    DWORD   cbThis;
    DWORD   flAccel;
    DWORD   cGlyphsSupported;
    DWORD   cRanges;
    WCRANGE ranges[1];
} GLYPHSET, *PGLYPHSET, *LPGLYPHSET;

/* image blt types */  
typedef USHORT COLOR16;

typedef struct _TRIVERTEX
{
    LONG    x;
    LONG    y;
    COLOR16 Red;
    COLOR16 Green;
    COLOR16 Blue;
    COLOR16 Alpha;
} TRIVERTEX,*PTRIVERTEX,*LPTRIVERTEX;

typedef struct _GRADIENT_TRIANGLE
{
    ULONG Vertex1;
    ULONG Vertex2;
    ULONG Vertex3;
} GRADIENT_TRIANGLE,*PGRADIENT_TRIANGLE,*LPGRADIENT_TRIANGLE;

typedef struct _GRADIENT_RECT
{
    ULONG UpperLeft;
    ULONG LowerRight;
}GRADIENT_RECT,*PGRADIENT_RECT,*LPGRADIENT_RECT;

typedef struct _BLENDFUNCTION
{
    BYTE   BlendOp;
    BYTE   BlendFlags;
    BYTE   SourceConstantAlpha;
    BYTE   AlphaFormat;
}BLENDFUNCTION,*PBLENDFUNCTION;

#define AC_SRC_OVER                 0x00
#define AC_SRC_ALPHA                0x01
BOOL WINAPI AlphaBlend( HDC hdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, 
                        HDC hdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, 
			BLENDFUNCTION blendFunction);
BOOL WINAPI TransparentBlt(HDC hdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, 
                           HDC hdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, 
			   UINT crTransparent);

#define GRADIENT_FILL_RECT_H    0x00000000
#define GRADIENT_FILL_RECT_V    0x00000001
#define GRADIENT_FILL_TRIANGLE  0x00000002
#define GRADIENT_FILL_OP_FLAG   0x000000ff
BOOL WINAPI GradientFill( HDC hdc, PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG dwNumMesh, ULONG dwMode );


  /* Rasterizer status */
typedef struct
{
    SHORT nSize;
    SHORT wFlags;
    SHORT nLanguageID;
} RASTERIZER_STATUS, *LPRASTERIZER_STATUS;

#define TT_AVAILABLE        0x0001
#define TT_ENABLED          0x0002

#define TT_PRIM_LINE    1
#define TT_PRIM_QSPLINE 2
#define TT_POLYGON_TYPE 24

/* Get/SetSystemPaletteUse() values */
#define SYSPAL_ERROR    0
#define SYSPAL_STATIC   1
#define SYSPAL_NOSTATIC 2

#ifndef _PALETTEENTRY_DEFINED
#define _PALETTEENTRY_DEFINED
typedef struct tagPALETTEENTRY
{
	BYTE peRed, peGreen, peBlue, peFlags;
} PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;
#endif

/* Logical palette entry flags */
#define PC_RESERVED     0x01
#define PC_EXPLICIT     0x02
#define PC_NOCOLLAPSE   0x04

#ifndef _LOGPALETTE_DEFINED
#define _LOGPALETTE_DEFINED
typedef struct tagLOGPALETTE
{
    WORD           palVersion;
    WORD           palNumEntries;
    PALETTEENTRY   palPalEntry[1];
} LOGPALETTE, *PLOGPALETTE, *LPLOGPALETTE, *NPLOGPALETTE;
#endif

  /* Pens */

typedef struct
{
    UINT     lopnStyle;
    POINT    lopnWidth;
    COLORREF lopnColor;
} LOGPEN, *LPLOGPEN;


typedef struct tagEXTLOGPEN
{
    DWORD    elpPenStyle;
    DWORD    elpWidth;
    UINT     elpBrushStyle;
    COLORREF elpColor;
    LONG     elpHatch;
    DWORD elpNumEntries;
    DWORD elpStyleEntry[1];
} EXTLOGPEN, *PEXTLOGPEN, *NPEXTLOGPEN, *LPEXTLOGPEN;

#define PS_SOLID         0x00000000
#define PS_DASH          0x00000001
#define PS_DOT           0x00000002
#define PS_DASHDOT       0x00000003
#define PS_DASHDOTDOT    0x00000004
#define PS_NULL          0x00000005
#define PS_INSIDEFRAME   0x00000006
#define PS_USERSTYLE     0x00000007
#define PS_ALTERNATE     0x00000008
#define PS_STYLE_MASK    0x0000000f

#define PS_ENDCAP_ROUND  0x00000000
#define PS_ENDCAP_SQUARE 0x00000100
#define PS_ENDCAP_FLAT   0x00000200
#define PS_ENDCAP_MASK   0x00000f00

#define PS_JOIN_ROUND    0x00000000
#define PS_JOIN_BEVEL    0x00001000
#define PS_JOIN_MITER    0x00002000
#define PS_JOIN_MASK     0x0000f000

#define PS_COSMETIC      0x00000000
#define PS_GEOMETRIC     0x00010000
#define PS_TYPE_MASK     0x000f0000

  /* Regions */

#define ERROR             0
#define NULLREGION        1
#define SIMPLEREGION      2
#define COMPLEXREGION     3
#define RGN_ERROR         ERROR

#define RGN_AND           1
#define RGN_OR            2
#define RGN_XOR           3
#define RGN_DIFF          4
#define RGN_COPY          5
#define RGN_MIN           RGN_AND
#define RGN_MAX           RGN_COPY
  /* Device contexts */

  /* Polygon modes */
#define ALTERNATE         1
#define WINDING           2
#define POLYFILL_LAST     2

/* Background modes */
/* Apparently some broken svr4 includes define TRANSPARENT */
#undef TRANSPARENT
#define TRANSPARENT       1
#define OPAQUE            2
#define BKMODE_LAST       2

  /* Graphics Modes */
#define GM_COMPATIBLE     1
#define GM_ADVANCED       2
#define GM_LAST           2

  /* Arc direction modes */
#define AD_COUNTERCLOCKWISE 1
#define AD_CLOCKWISE        2

  /* Map modes */
#define MM_TEXT		  1
#define MM_LOMETRIC	  2
#define MM_HIMETRIC	  3
#define MM_LOENGLISH	  4
#define MM_HIENGLISH	  5
#define MM_TWIPS	  6
#define MM_ISOTROPIC	  7
#define MM_ANISOTROPIC	  8

#define MM_MIN            MM_TEXT
#define MM_MAX            MM_ANISOTROPIC
#define MM_MAX_FIXEDSCALE MM_TWIPS

  /* Coordinate modes */
#define ABSOLUTE          1
#define RELATIVE          2

  /* Flood fill modes */
#define FLOODFILLBORDER   0
#define FLOODFILLSURFACE  1

  /* Device parameters for GetDeviceCaps() */
#define DRIVERVERSION     0
#define TECHNOLOGY        2
#define HORZSIZE          4
#define VERTSIZE          6
#define HORZRES           8
#define VERTRES           10
#define BITSPIXEL         12
#define PLANES            14
#define NUMBRUSHES        16
#define NUMPENS           18
#define NUMMARKERS        20
#define NUMFONTS          22
#define NUMCOLORS         24
#define PDEVICESIZE       26
#define CURVECAPS         28
#define LINECAPS          30
#define POLYGONALCAPS     32
#define TEXTCAPS          34
#define CLIPCAPS          36
#define RASTERCAPS        38
#define ASPECTX           40
#define ASPECTY           42
#define ASPECTXY          44
#define LOGPIXELSX        88
#define LOGPIXELSY        90
#define CAPS1             94
#define SIZEPALETTE       104
#define NUMRESERVED       106
#define COLORRES          108

#define PHYSICALWIDTH     110
#define PHYSICALHEIGHT    111
#define PHYSICALOFFSETX   112
#define PHYSICALOFFSETY   113
#define SCALINGFACTORX    114
#define SCALINGFACTORY    115
#define VREFRESH          116
#define DESKTOPVERTRES    117
#define DESKTOPHORZRES    118
#define BTLALIGNMENT      119

/* TECHNOLOGY */
#define DT_PLOTTER        0
#define DT_RASDISPLAY     1
#define DT_RASPRINTER     2
#define DT_RASCAMERA      3
#define DT_CHARSTREAM     4
#define DT_METAFILE       5
#define DT_DISPFILE       6

/* CURVECAPS */
#define CC_NONE           0x0000
#define CC_CIRCLES        0x0001
#define CC_PIE            0x0002
#define CC_CHORD          0x0004
#define CC_ELLIPSES       0x0008
#define CC_WIDE           0x0010
#define CC_STYLED         0x0020
#define CC_WIDESTYLED     0x0040
#define CC_INTERIORS      0x0080
#define CC_ROUNDRECT      0x0100

/* LINECAPS */
#define LC_NONE           0x0000
#define LC_POLYLINE       0x0002
#define LC_MARKER         0x0004
#define LC_POLYMARKER     0x0008
#define LC_WIDE           0x0010
#define LC_STYLED         0x0020
#define LC_WIDESTYLED     0x0040
#define LC_INTERIORS      0x0080

/* POLYGONALCAPS */
#define PC_NONE           0x0000
#define PC_POLYGON        0x0001
#define PC_RECTANGLE      0x0002
#define PC_WINDPOLYGON    0x0004
#define PC_TRAPEZOID      0x0004
#define PC_SCANLINE       0x0008
#define PC_WIDE           0x0010
#define PC_STYLED         0x0020
#define PC_WIDESTYLED     0x0040
#define PC_INTERIORS      0x0080
#define PC_POLYPOLYGON    0x0100
#define PC_PATHS          0x0200

/* TEXTCAPS */
#define TC_OP_CHARACTER   0x0001
#define TC_OP_STROKE      0x0002
#define TC_CP_STROKE      0x0004
#define TC_CR_90          0x0008
#define TC_CR_ANY         0x0010
#define TC_SF_X_YINDEP    0x0020
#define TC_SA_DOUBLE      0x0040
#define TC_SA_INTEGER     0x0080
#define TC_SA_CONTIN      0x0100
#define TC_EA_DOUBLE      0x0200
#define TC_IA_ABLE        0x0400
#define TC_UA_ABLE        0x0800
#define TC_SO_ABLE        0x1000
#define TC_RA_ABLE        0x2000
#define TC_VA_ABLE        0x4000
#define TC_RESERVED       0x8000
#define TC_SCROLLBLT      0x00010000

/* CLIPCAPS */
#define CP_NONE           0x0000
#define CP_RECTANGLE      0x0001
#define CP_REGION         0x0002

/* RASTERCAPS */
#define RC_NONE           0x0000
#define RC_BITBLT         0x0001
#define RC_BANDING        0x0002
#define RC_SCALING        0x0004
#define RC_BITMAP64       0x0008
#define RC_GDI20_OUTPUT   0x0010
#define RC_GDI20_STATE    0x0020
#define RC_SAVEBITMAP     0x0040
#define RC_DI_BITMAP      0x0080
#define RC_PALETTE        0x0100
#define RC_DIBTODEV       0x0200
#define RC_BIGFONT        0x0400
#define RC_STRETCHBLT     0x0800
#define RC_FLOODFILL      0x1000
#define RC_STRETCHDIB     0x2000
#define RC_OP_DX_OUTPUT   0x4000
#define RC_DEVBITS        0x8000

/* CAPS1 */

#define C1_TRANSPARENT    0x0001
#define TC_TT_ABLE        0x0002
#define C1_TT_CR_ANY      0x0004
#define C1_EMF_COMPLIANT  0x0008
#define C1_DIBENGINE      0x0010
#define C1_GAMMA_RAMP     0x0040
#define C1_REINIT_ABLE    0x0080
#define C1_GLYPH_INDEX    0x0100
#define C1_BIT_PACKED     0x0200
#define C1_BYTE_PACKED    0x0400
#define C1_COLORCURSOR    0x0800
#define C1_CMYK_ABLE      0x1000
#define C1_SLOW_CARD      0x2000

  /* Device-independent bitmaps */

typedef struct {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD, *LPRGBQUAD;

typedef struct {
  BYTE rgbtBlue;
  BYTE rgbtGreen;
  BYTE rgbtRed;
} RGBTRIPLE;

#include "pshpack2.h"
typedef struct
{
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#include "poppack.h"

#define MAKEPOINTS(l)  (*((POINTS *)&(l)))

typedef struct
{
    DWORD 	biSize;
    LONG  	biWidth;
    LONG  	biHeight;
    WORD 	biPlanes;
    WORD 	biBitCount;
    DWORD 	biCompression;
    DWORD 	biSizeImage;
    LONG  	biXPelsPerMeter;
    LONG  	biYPelsPerMeter;
    DWORD 	biClrUsed;
    DWORD 	biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct
{
    DWORD        bV4Size;
    LONG         bV4Width;
    LONG         bV4Height;
    WORD         bV4Planes;
    WORD         bV4BitCount;
    DWORD        bV4Compression;
    DWORD        bV4SizeImage;
    LONG         bV4XPelsPerMeter;
    LONG         bV4YPelsPerMeter;
    DWORD        bV4ClrUsed;
    DWORD        bV4ClrImportant;
    DWORD        bV4RedMask;
    DWORD        bV4GreenMask;
    DWORD        bV4BlueMask;
    DWORD        bV4AlphaMask;
    DWORD        bV4CSType;
    CIEXYZTRIPLE bV4EndPoints;
    DWORD        bV4GammaRed;
    DWORD        bV4GammaGreen;
    DWORD        bV4GammaBlue;
} BITMAPV4HEADER, *PBITMAPV4HEADER;

typedef struct {
    DWORD        bV5Size;
    LONG         bV5Width;
    LONG         bV5Height;
    WORD         bV5Planes;
    WORD         bV5BitCount;
    DWORD        bV5Compression;
    DWORD        bV5SizeImage;
    LONG         bV5XPelsPerMeter;
    LONG         bV5YPelsPerMeter;
    DWORD        bV5ClrUsed;
    DWORD        bV5ClrImportant;
    DWORD        bV5RedMask;
    DWORD        bV5GreenMask;
    DWORD        bV5BlueMask;
    DWORD        bV5AlphaMask;
    DWORD        bV5CSType;
    CIEXYZTRIPLE bV5Endpoints;
    DWORD        bV5GammaRed;
    DWORD        bV5GammaGreen;
    DWORD        bV5GammaBlue;
    DWORD        bV5Intent;
    DWORD        bV5ProfileData;
    DWORD        bV5ProfileSize;
    DWORD        bV5Reserved;
} BITMAPV5HEADER, *LPBITMAPV5HEADER, *PBITMAPV5HEADER;

#define PROFILE_LINKED   'LINK'
#define PROFILE_EMBEDDED 'MBED'


  /* biCompression */
#define BI_RGB           0
#define BI_RLE8          1
#define BI_RLE4          2
#define BI_BITFIELDS     3

typedef struct {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD	bmiColors[1];
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;

typedef struct
{
    DWORD   bcSize;
    WORD    bcWidth;
    WORD    bcHeight;
    WORD    bcPlanes;
    WORD    bcBitCount;
} BITMAPCOREHEADER, *PBITMAPCOREHEADER, *LPBITMAPCOREHEADER;

typedef struct
{
    BITMAPCOREHEADER bmciHeader;
    RGBTRIPLE        bmciColors[1];
} BITMAPCOREINFO, *PBITMAPCOREINFO, *LPBITMAPCOREINFO;

#define DIB_RGB_COLORS   0
#define DIB_PAL_COLORS   1
#define CBM_INIT         4

typedef struct
{
	BITMAP		dsBm;
	BITMAPINFOHEADER	dsBmih;
	DWORD			dsBitfields[3];
	HANDLE		dshSection;
	DWORD			dsOffset;
} DIBSECTION, *PDIBSECTION, *LPDIBSECTION;

  /* Stock GDI objects for GetStockObject() */

#define WHITE_BRUSH	    0
#define LTGRAY_BRUSH	    1
#define GRAY_BRUSH	    2
#define DKGRAY_BRUSH	    3
#define BLACK_BRUSH	    4
#define NULL_BRUSH	    5
#define HOLLOW_BRUSH	    5
#define WHITE_PEN	    6
#define BLACK_PEN	    7
#define NULL_PEN	    8
#define OEM_FIXED_FONT	    10
#define ANSI_FIXED_FONT     11
#define ANSI_VAR_FONT	    12
#define SYSTEM_FONT	    13
#define DEVICE_DEFAULT_FONT 14
#define DEFAULT_PALETTE     15
#define SYSTEM_FIXED_FONT   16
#define DEFAULT_GUI_FONT    17

#define STOCK_LAST          17

#define CLR_INVALID         0xffffffff
/* Metafile header structure */
#include "pshpack2.h"
typedef struct
{
    WORD       mtType;
    WORD       mtHeaderSize;
    WORD       mtVersion;
    DWORD      mtSize;
    WORD       mtNoObjects;
    DWORD      mtMaxRecord;
    WORD       mtNoParameters;
} METAHEADER, *PMETAHEADER, *LPMETAHEADER;
#include "poppack.h"

/* Metafile typical record structure */
typedef struct
{
    DWORD      rdSize;
    WORD       rdFunction;
    WORD       rdParm[1];
} METARECORD, *PMETARECORD, *LPMETARECORD;

/* Handle table structure */

typedef struct
{
    HGDIOBJ objectHandle[1];
} HANDLETABLE, *PHANDLETABLE, *LPHANDLETABLE;


/* Clipboard metafile picture structure */
typedef struct
{
    LONG       mm;
    LONG       xExt;
    LONG       yExt;
    HMETAFILE  hMF;
} METAFILEPICT, *LPMETAFILEPICT;


/* Metafile functions */
#define META_SETBKCOLOR              0x0201
#define META_SETBKMODE               0x0102
#define META_SETMAPMODE              0x0103
#define META_SETROP2                 0x0104
#define META_SETRELABS               0x0105
#define META_SETPOLYFILLMODE         0x0106
#define META_SETSTRETCHBLTMODE       0x0107
#define META_SETTEXTCHAREXTRA        0x0108
#define META_SETTEXTCOLOR            0x0209
#define META_SETTEXTJUSTIFICATION    0x020A
#define META_SETWINDOWORG            0x020B
#define META_SETWINDOWEXT            0x020C
#define META_SETVIEWPORTORG          0x020D
#define META_SETVIEWPORTEXT          0x020E
#define META_OFFSETWINDOWORG         0x020F
#define META_SCALEWINDOWEXT          0x0410
#define META_OFFSETVIEWPORTORG       0x0211
#define META_SCALEVIEWPORTEXT        0x0412
#define META_LINETO                  0x0213
#define META_MOVETO                  0x0214
#define META_EXCLUDECLIPRECT         0x0415
#define META_INTERSECTCLIPRECT       0x0416
#define META_ARC                     0x0817
#define META_ELLIPSE                 0x0418
#define META_FLOODFILL               0x0419
#define META_PIE                     0x081A
#define META_RECTANGLE               0x041B
#define META_ROUNDRECT               0x061C
#define META_PATBLT                  0x061D
#define META_SAVEDC                  0x001E
#define META_SETPIXEL                0x041F
#define META_OFFSETCLIPRGN           0x0220
#define META_TEXTOUT                 0x0521
#define META_BITBLT                  0x0922
#define META_STRETCHBLT              0x0B23
#define META_POLYGON                 0x0324
#define META_POLYLINE                0x0325
#define META_ESCAPE                  0x0626
#define META_RESTOREDC               0x0127
#define META_FILLREGION              0x0228
#define META_FRAMEREGION             0x0429
#define META_INVERTREGION            0x012A
#define META_PAINTREGION             0x012B
#define META_SELECTCLIPREGION        0x012C
#define META_SELECTOBJECT            0x012D
#define META_SETTEXTALIGN            0x012E
#define META_DRAWTEXT                0x062F
#define META_CHORD                   0x0830
#define META_SETMAPPERFLAGS          0x0231
#define META_EXTTEXTOUT              0x0A32
#define META_SETDIBTODEV             0x0D33
#define META_SELECTPALETTE           0x0234
#define META_REALIZEPALETTE          0x0035
#define META_ANIMATEPALETTE          0x0436
#define META_SETPALENTRIES           0x0037
#define META_POLYPOLYGON             0x0538
#define META_RESIZEPALETTE           0x0139
#define META_DIBBITBLT               0x0940
#define META_DIBSTRETCHBLT           0x0B41
#define META_DIBCREATEPATTERNBRUSH   0x0142
#define META_STRETCHDIB              0x0F43
#define META_EXTFLOODFILL            0x0548
#define META_RESETDC                 0x014C
#define META_STARTDOC                0x014D
#define META_STARTPAGE               0x004F
#define META_ENDPAGE                 0x0050
#define META_ABORTDOC                0x0052
#define META_ENDDOC                  0x005E
#define META_DELETEOBJECT            0x01F0
#define META_CREATEPALETTE           0x00F7
#define META_CREATEBRUSH             0x00F8
#define META_CREATEPATTERNBRUSH      0x01F9
#define META_CREATEPENINDIRECT       0x02FA
#define META_CREATEFONTINDIRECT      0x02FB
#define META_CREATEBRUSHINDIRECT     0x02FC
#define META_CREATEBITMAPINDIRECT    0x02FD
#define META_CREATEBITMAP            0x06FE
#define META_CREATEREGION            0x06FF
#define META_UNKNOWN                 0x0529  /* FIXME: unknown meta magic */

typedef INT (CALLBACK *MFENUMPROC)(HDC,HANDLETABLE*,METARECORD*,
                                       INT,LPARAM);

/* enhanced metafile structures and functions */

/* note that ENHMETAHEADER is just a particular kind of ENHMETARECORD,
   ie. the header is just the first record in the metafile */
typedef struct {
    DWORD iType;
    DWORD nSize;
    RECTL rclBounds;
    RECTL rclFrame;
    DWORD dSignature;
    DWORD nVersion;
    DWORD nBytes;
    DWORD nRecords;
    WORD  nHandles;
    WORD  sReserved;
    DWORD nDescription;
    DWORD offDescription;
    DWORD nPalEntries;
    SIZEL szlDevice;
    SIZEL szlMillimeters;

    /* Fields for winver >= win95 */
    DWORD cbPixelFormat;
    DWORD offPixelFormat;
    DWORD bOpenGL;

    /* Fields for winver >= win98 */
    SIZEL szlMicrometers;

} ENHMETAHEADER, *PENHMETAHEADER, *LPENHMETAHEADER;

typedef struct {
    DWORD iType;
    DWORD nSize;
    DWORD dParm[1];
} ENHMETARECORD, *LPENHMETARECORD;

typedef struct {
    DWORD iType;
    DWORD nSize;
} EMR, *PEMR;

typedef struct {
    POINTL ptlReference;
    DWORD  nChars;
    DWORD  offString;
    DWORD  fOptions;
    RECTL  rcl;
    DWORD  offDx;
} EMRTEXT, *PEMRTEXT;

typedef struct {
    EMR emr;
} EMRABORTPATH,      *PEMRABORTPATH,
  EMRBEGINPATH,      *PEMRBEGINPATH,
  EMRENDPATH,        *PEMRENDPATH,
  EMRCLOSEFIGURE,    *PEMRCLOSEFIGURE,
  EMRFLATTENPATH,    *PEMRFLATTENPATH,
  EMRWIDENPATH,      *PEMRWIDENPATH,
  EMRSETMETARGN,     *PEMRSETMETARGN,
  EMRSAVEDC,         *PEMRSAVEDC,
  EMRREALIZEPALETTE, *PEMRREALIZEPALETTE;

typedef struct {
    EMR    emr;
    POINTL ptlCenter;
    DWORD  nRadius;
    FLOAT  eStartAngle;
    FLOAT  eSweepAngle;
} EMRANGLEARC, *PEMRANGLEARC;

typedef struct {
    EMR    emr;
    RECTL  rclBox;
    POINTL ptlStart;
    POINTL ptlEnd;
} EMRARC,   *PEMRARC,
  EMRARCTO, *PEMRARCTO,
  EMRCHORD, *PEMRCHORD,
  EMRPIE,   *PEMRPIE;

typedef struct {
    EMR      emr;
    RECTL    rclBounds;
    LONG     xDest;
    LONG     yDest;
    LONG     cxDest;
    LONG     cyDest;
    DWORD    dwRop;
    LONG     xSrc;
    LONG     ySrc;
    XFORM    xformSrc;
    COLORREF crBkColorSrc;
    DWORD    iUsageSrc;
    DWORD    offBmiSrc;
    DWORD    cbBmiSrc;
    DWORD    offBitsSrc;
    DWORD    cbBitsSrc;
} EMRBITBLT, *PEMRBITBLT;

typedef struct {
    EMR      emr;
    DWORD    ihBrush;
    LOGBRUSH lb;
} EMRCREATEBRUSHINDIRECT, *PEMRCREATEBRUSHINDIRECT;

typedef struct {
    EMR            emr;
    DWORD          ihCS;
    LOGCOLORSPACEA lcs;
} EMRCREATECOLORSPACE, *PEMRCREATECOLORSPACE;

typedef struct {
  EMR            emr;
  DWORD          ihCS;
  LOGCOLORSPACEW lcs;
  DWORD          dwFlags;
  DWORD          cbData;
  BYTE           Data[1];
} EMRCREATECOLORSPACEW, *PEMRCREATECOLORSPACEW;

typedef struct {
    EMR   emr;
    DWORD ihBrush;
    DWORD iUsage;
    DWORD offBmi;
    DWORD cbBmi;
    DWORD offBits;
    DWORD cbBits;
} EMRCREATEDIBPATTERNBRUSHPT, *PEMRCREATEDIBPATTERNBRUSHPT;

typedef struct {
    EMR   emr;
    DWORD ihBrush;
    DWORD iUsage;
    DWORD offBmi;
    DWORD cbBmi;
    DWORD offBits;
    DWORD cbBits;
} EMRCREATEMONOBRUSH, *PEMRCREATEMONOBRUSH;

typedef struct {
    EMR        emr;
    DWORD      ihPal;
    LOGPALETTE lgpl;
} EMRCREATEPALETTE, *PEMRCREATEPALETTE;

typedef struct {
    EMR    emr;
    DWORD  ihPen;
    LOGPEN lopn;
} EMRCREATEPEN, *PEMRCREATEPEN;

typedef struct {
    EMR           emr;
    DWORD         ihCS;
} EMRDELETECOLORSPACE, *PEMRDELETECOLORSPACE,
  EMRSELECTCOLORSPACE, *PEMRSELECTCOLORSPACE,
  EMRSETCOLORSPACE,    *PEMRSETCOLORSPACE;

typedef struct {
    EMR   emr;
    DWORD ihObject;
} EMRDELETEOBJECT, *PEMRDELETEOBJECT,
  EMRSELECTOBJECT, *PEMRSELECTOBJECT;

typedef struct {
    EMR   emr;
    RECTL rclBox;
} EMRELLIPSE,   *PEMRELLIPSE,
  EMRRECTANGLE, *PEMRRECTANGLE;

typedef struct {
    EMR   emr;
    DWORD nPalEntries;
    DWORD offPalEntries;
    DWORD nSizeLast;
} EMREOF, *PEMREOF;

typedef struct {
    EMR   emr;
    RECTL rclClip;
} EMREXCLUDECLIPRECT,   *PEMREXCLUDECLIPRECT,
  EMRINTERSECTCLIPRECT, *PEMRINTERSECTCLIPRECT;

typedef struct {
    EMR         emr;
    DWORD       ihFont;
    EXTLOGFONTW elfw;
} EMREXTCREATEFONTINDIRECTW, *PEMREXTCREATEFONTINDIRECTW;

typedef struct {
    EMR       emr;
    DWORD     ihPen;
    DWORD     offBmi;
    DWORD     cbBmi;
    DWORD     offBits;
    DWORD     cbBits;
    EXTLOGPEN elp;
} EMREXTCREATEPEN, *PEMREXTCREATEPEN;

typedef struct {
    EMR      emr;
    POINTL   ptlStart;
    COLORREF crColor;
    DWORD    iMode;
} EMREXTFLOODFILL, *PEMREXTFLOODFILL;

typedef struct {
    EMR   emr;
    DWORD cbRgnData;
    DWORD iMode;
    BYTE  RgnData[1];
} EMREXTSELECTCLIPRGN, *PEMREXTSELECTCLIPRGN;

typedef struct {
    EMR     emr;
    RECTL   rclBounds;
    DWORD   iGraphicsMode;
    FLOAT   exScale;
    FLOAT   eyScale;
    EMRTEXT emrtext;
} EMREXTTEXTOUTA, *PEMREXTTEXTOUTA,
  EMREXTTEXTOUTW, *PEMREXTTEXTOUTW;

typedef struct {
    EMR emr;
    RECTL rclBounds;
} EMRFILLPATH,          *PEMRFILLPATH,
  EMRSTROKEANDFILLPATH, *PEMRSTROKEANDFILLPATH,
  EMRSTROKEPATH,        *PEMRSTROKEPATH;

typedef struct {
    EMR   emr;
    RECTL rclBounds;
    DWORD cbRgnData;
    DWORD ihBrush;
    BYTE  RgnData[1];
} EMRFILLRGN, *PEMRFILLRGN;

typedef struct {
    DWORD signature;
    DWORD nVersion;
    DWORD cbData;
    DWORD offData;
} EMRFORMAT, *PEMRFORMAT;

typedef struct {
    EMR   emr;
    RECTL rclBounds;
    DWORD cbRgnData;
    DWORD ihBrush;
    SIZEL szlStroke;
    BYTE  RgnData[1];
} EMRFRAMERGN, *PEMRFRAMERGN;

typedef struct {
    EMR   emr;
    DWORD cbData;
    BYTE  Data[1];
} EMRGDICOMMENT, *PEMRGDICOMMENT;

#if 0
typedef struct {
    EMR       emr;
    RECTL     rclBounds;
    DWORD     nVer;
    DWORD     nTri;
    ULONG     ulMode;
    TRIVERTEX Ver[1];
} EMRGRADIENTFILL, *PEMRGRADIENTFILL;
#endif

typedef struct {
    EMR   emr;
    RECTL rclBounds;
    DWORD cbRgnData;
    BYTE  RgnData[1];
} EMRINVERTRGN, *PEMRINVERTRGN,
  EMRPAINTRGN,  *PEMRPAINTRGN;

typedef struct {
    EMR    emr;
    POINTL ptl;
} EMRLINETO,   *PEMRLINETO,
  EMRMOVETOEX, *PEMRMOVETOEX;

typedef struct {
    EMR      emr;
    RECTL    rclBounds;
    LONG     xDest;
    LONG     yDest;
    LONG     cxDest;
    LONG     cyDest;
    DWORD    dwRop;
    LONG     xSrc;
    LONG     ySrc;
    XFORM    xformSrc;
    COLORREF crBkColorSrc;
    DWORD    iUsageSrc;
    DWORD    offBmiSrc;
    DWORD    cbBmiSrc;
    DWORD    offBitsSrc;
    DWORD    cbBitsSrc;
    LONG     xMask;
    LONG     yMask;
    DWORD    iUsageMask;
    DWORD    offBmiMask;
    DWORD    cbBmiMask;
    DWORD    offBitsMask;
    DWORD    cbBitsMask;
} EMRMASKBLT, *PEMRMASKBLT;

typedef struct {
    EMR   emr;
    XFORM xform;
    DWORD iMode;
} EMRMODIFYWORLDTRANSFORM, *PEMRMODIFYWORLDTRANSFORM;

typedef struct {
    EMR    emr;
    POINTL ptlOffset;
} EMROFFSETCLIPRGN, *PEMROFFSETCLIPRGN;

typedef struct {
    EMR      emr;
    RECTL    rclBounds;
    POINTL   aptlDst[3];
    LONG     xSrc;
    LONG     ySrc;
    LONG     cxSrc;
    LONG     cySrc;
    XFORM    xformSrc;
    COLORREF crBkColorSrc;
    DWORD    iUsageSrc;
    DWORD    offBmiSrc;
    DWORD    cbBmiSrc;
    DWORD    offBitsSrc;
    DWORD    cbBitsSrc;
    LONG     xMask;
    LONG     yMask;
    DWORD    iUsageMask;
    DWORD    offBmiMask;
    DWORD    cbBmiMask;
    DWORD    offBitsMask;
    DWORD    cbBitsMask;
} EMRPLGBLT, *PEMRPLGBLT;

typedef struct {
    EMR    emr;
    RECTL  rclBounds;
    DWORD  cptl;
    POINTL aptl[1];
} EMRPOLYLINE,     *PEMRPOLYLINE,
  EMRPOLYBEZIER,   *PEMRPOLYBEZIER,
  EMRPOLYGON,      *PEMRPOLYGON,
  EMRPOLYBEZIERTO, *PEMRPOLYBEZIERTO,
  EMRPOLYLINETO,   *PEMRPOLYLINETO;

typedef struct {
    EMR    emr;
    RECTL  rclBounds;
    DWORD  cptl;
    POINTL aptl[1];
    BYTE   abTypes[1];
} EMRPOLYDRAW, *PEMRPOLYDRAW;

typedef struct {
    EMR     emr;
    RECTL   rclBounds;
    DWORD   nPolys;
    DWORD   cptl;
    DWORD   aPolyCounts[1];
    POINTL  aptl[1];
} EMRPOLYPOLYLINE, *PEMRPOLYPOLYLINE,
  EMRPOLYPOLYGON,  *PEMRPOLYPOLYGON;

typedef struct {
    EMR     emr;
    RECTL   rclBounds;
    DWORD   iGraphicsMode;
    FLOAT   exScale;
    FLOAT   eyScale;
    LONG    cStrings;
    EMRTEXT aemrtext[1];
} EMRPOLYTEXTOUTA, *PEMRPOLYTEXTOUTA,
  EMRPOLYTEXTOUTW, *PEMRPOLYTEXTOUTW;

typedef struct {
    EMR   emr;
    DWORD ihPal;
    DWORD cEntries;
} EMRRESIZEPALETTE, *PEMRRESIZEPALETTE;

typedef struct {
    EMR  emr;
    LONG iRelative;
} EMRRESTOREDC, *PEMRRESTOREDC;

typedef struct {
    EMR   emr;
    RECTL rclBox;
    SIZEL szlCorner;
} EMRROUNDRECT, *PEMRROUNDRECT;

typedef struct {
    EMR  emr;
    LONG xNum;
    LONG xDenom;
    LONG yNum;
    LONG yDenom;
} EMRSCALEVIEWPORTEXTEX, *PEMRSCALEVIEWPORTEXTEX,
  EMRSCALEWINDOWEXTEX,   *PEMRSCALEWINDOWEXTEX;

typedef struct {
    EMR   emr;
    DWORD iMode;
} EMRSELECTCLIPPATH,    *PEMRSELECTCLIPPATH,
  EMRSETBKMODE,         *PEMRSETBKMODE,
  EMRSETMAPMODE,        *PEMRSETMAPMODE,
  EMRSETPOLYFILLMODE,   *PEMRSETPOLYFILLMODE,
  EMRSETROP2,           *PEMRSETROP2,
  EMRSETSTRETCHBLTMODE, *PEMRSETSTRETCHBLTMODE,
  EMRSETTEXTALIGN,      *PEMRSETTEXTALIGN,
  EMRSETICMMODE,        *PERMSETICMMODE,
  EMRSETLAYOUT,         *PEMRSETLAYOUT;

typedef struct {
    EMR   emr;
    DWORD ihPal;
} EMRSELECTPALETTE, *PEMRSELECTPALETTE;

typedef struct {
    EMR   emr;
    DWORD iArcDirection;
} EMRSETARCDIRECTION, *PEMRSETARCDIRECTION;

typedef struct {
    EMR      emr;
    COLORREF crColor;
} EMRSETBKCOLOR,   *PEMRSETBKCOLOR,
  EMRSETTEXTCOLOR, *PEMRSETTEXTCOLOR;

typedef struct {
    EMR   emr;
    POINTL ptlOrigin;
} EMRSETBRUSHORGEX,    *PEMRSETBRUSHORGEX,
  EMRSETVIEWPORTORGEX, *PEMRSETVIEWPORTORGEX,
  EMRSETWINDOWORGEX,   *PEMRSETWINDOWORGEX;

typedef struct {
    EMR  emr;
    COLORADJUSTMENT ColorAdjustment;
} EMRSETCOLORADJUSTMENT, *PEMRSETCOLORADJUSTMENT;

typedef struct {
    EMR   emr;
    RECTL rclBounds;
    LONG  xDest;
    LONG  yDest;
    LONG  xSrc;
    LONG  ySrc;
    LONG  cxSrc;
    LONG  cySrc;
    DWORD offBmiSrc;
    DWORD cbBmiSrc;
    DWORD offBitsSrc;
    DWORD cbBitsSrc;
    DWORD iUsageSrc;
    DWORD iStartScan;
    DWORD cScans;
} EMRSETDIBITSTODEIVCE, *PEMRSETDIBITSTODEVICE;

typedef struct {
    EMR   emr;
    DWORD dwFlags;
} EMRSETMAPPERFLAGS, *PEMRSETMAPPERFLAGS;

typedef struct {
    EMR   emr;
    FLOAT eMiterLimit;
} EMRSETMITERLIMIT, *PEMRSETMITERLIMIT;

typedef struct {
    EMR          emr;
    DWORD        ihPal;
    DWORD        iStart;
    DWORD        cEntries;
    PALETTEENTRY aPalEntries[1];
} EMRSETPALETTEENTRIES, *PEMRSETPALETTEENTRIES;

typedef struct {
    EMR     emr;
    POINTL  ptlPixel;
    COLORREF crColor;
} EMRSETPIXELV, *PEMRSETPIXELV;

typedef struct {
    EMR   emr;
    SIZEL szlExtent;
} EMRSETVIEWPORTEXTEX, *PEMRSETVIEWPORTEXTEX,
  EMRSETWINDOWEXTEX,   *PEMRSETWINDOWEXTEX;

typedef struct {
    EMR   emr;
    XFORM xform;
} EMRSETWORLDTRANSFORM, *PEMRSETWORLDTRANSFORM;

typedef struct {
    EMR      emr;
    RECTL    rclBounds;
    LONG     xDest;
    LONG     yDest;
    LONG     cxDest;
    LONG     cyDest;
    DWORD    dwRop;
    LONG     xSrc;
    LONG     ySrc;
    XFORM    xformSrc;
    COLORREF crBkColorSrc;
    DWORD    iUsageSrc;
    DWORD    offBmiSrc;
    DWORD    cbBmiSrc;
    DWORD    offBitsSrc;
    DWORD    cbBitsSrc;
    LONG     cxSrc;
    LONG     cySrc;
} EMRSTRETCHBLT, *PEMRSTRETCHBLT;

typedef struct {
    EMR   emr;
    RECTL rclBounds;
    LONG  xDest;
    LONG  yDest;
    LONG  xSrc;
    LONG  ySrc;
    LONG  cxSrc;
    LONG  cySrc;
    DWORD offBmiSrc;
    DWORD cbBmiSrc;
    DWORD offBitsSrc;
    DWORD cbBitsSrc;
    DWORD iUsageSrc;
    DWORD dwRop;
    LONG  cxDest;
    LONG  cyDest;
} EMRSTRETCHDIBITS, *PEMRSTRETCHDIBITS;

typedef struct {
    EMR                   emr;
    PIXELFORMATDESCRIPTOR pfd;
} EMRPIXELFORMAT, *PEMRPIXELFORMAT;

typedef struct tagEMRGLSRECORD {
  EMR   emr;
  DWORD cbData;
  BYTE  Data[1];
} EMRGLSRECORD, *PEMRGLSRECORD;

typedef struct {
  EMR   emr;
  RECTL rclBounds;
  DWORD cbData;
  BYTE  Data[1];
} EMRGLSBOUNDEDRECORD, *PEMRGLSBOUNDEDRECORD;

typedef INT (CALLBACK *ENHMFENUMPROC)(HDC, LPHANDLETABLE,
					  LPENHMETARECORD, INT, LPVOID);

#define EMR_HEADER	1
#define EMR_POLYBEZIER	2
#define EMR_POLYGON	3
#define EMR_POLYLINE	4
#define EMR_POLYBEZIERTO	5
#define EMR_POLYLINETO	6
#define EMR_POLYPOLYLINE	7
#define EMR_POLYPOLYGON	8
#define EMR_SETWINDOWEXTEX	9
#define EMR_SETWINDOWORGEX	10
#define EMR_SETVIEWPORTEXTEX	11
#define EMR_SETVIEWPORTORGEX	12
#define EMR_SETBRUSHORGEX	13
#define EMR_EOF	14
#define EMR_SETPIXELV	15
#define EMR_SETMAPPERFLAGS	16
#define EMR_SETMAPMODE	17
#define EMR_SETBKMODE	18
#define EMR_SETPOLYFILLMODE	19
#define EMR_SETROP2	20
#define EMR_SETSTRETCHBLTMODE	21
#define EMR_SETTEXTALIGN	22
#define EMR_SETCOLORADJUSTMENT	23
#define EMR_SETTEXTCOLOR	24
#define EMR_SETBKCOLOR	25
#define EMR_OFFSETCLIPRGN	26
#define EMR_MOVETOEX	27
#define EMR_SETMETARGN	28
#define EMR_EXCLUDECLIPRECT	29
#define EMR_INTERSECTCLIPRECT	30
#define EMR_SCALEVIEWPORTEXTEX	31
#define EMR_SCALEWINDOWEXTEX	32
#define EMR_SAVEDC	33
#define EMR_RESTOREDC	34
#define EMR_SETWORLDTRANSFORM	35
#define EMR_MODIFYWORLDTRANSFORM	36
#define EMR_SELECTOBJECT	37
#define EMR_CREATEPEN	38
#define EMR_CREATEBRUSHINDIRECT	39
#define EMR_DELETEOBJECT	40
#define EMR_ANGLEARC	41
#define EMR_ELLIPSE	42
#define EMR_RECTANGLE	43
#define EMR_ROUNDRECT	44
#define EMR_ARC	45
#define EMR_CHORD	46
#define EMR_PIE	47
#define EMR_SELECTPALETTE	48
#define EMR_CREATEPALETTE	49
#define EMR_SETPALETTEENTRIES	50
#define EMR_RESIZEPALETTE	51
#define EMR_REALIZEPALETTE	52
#define EMR_EXTFLOODFILL	53
#define EMR_LINETO	54
#define EMR_ARCTO	55
#define EMR_POLYDRAW	56
#define EMR_SETARCDIRECTION	57
#define EMR_SETMITERLIMIT	58
#define EMR_BEGINPATH	59
#define EMR_ENDPATH	60
#define EMR_CLOSEFIGURE	61
#define EMR_FILLPATH	62
#define EMR_STROKEANDFILLPATH	63
#define EMR_STROKEPATH	64
#define EMR_FLATTENPATH	65
#define EMR_WIDENPATH	66
#define EMR_SELECTCLIPPATH	67
#define EMR_ABORTPATH	68
#define EMR_GDICOMMENT	70
#define EMR_FILLRGN	71
#define EMR_FRAMERGN	72
#define EMR_INVERTRGN	73
#define EMR_PAINTRGN	74
#define EMR_EXTSELECTCLIPRGN	75
#define EMR_BITBLT	76
#define EMR_STRETCHBLT	77
#define EMR_MASKBLT	78
#define EMR_PLGBLT	79
#define EMR_SETDIBITSTODEVICE	80
#define EMR_STRETCHDIBITS	81
#define EMR_EXTCREATEFONTINDIRECTW	82
#define EMR_EXTTEXTOUTA	83
#define EMR_EXTTEXTOUTW	84
#define EMR_POLYBEZIER16	85
#define EMR_POLYGON16	86
#define EMR_POLYLINE16	87
#define EMR_POLYBEZIERTO16	88
#define EMR_POLYLINETO16	89
#define EMR_POLYPOLYLINE16	90
#define EMR_POLYPOLYGON16	91
#define EMR_POLYDRAW16	92
#define EMR_CREATEMONOBRUSH	93
#define EMR_CREATEDIBPATTERNBRUSHPT	94
#define EMR_EXTCREATEPEN	95
#define EMR_POLYTEXTOUTA	96
#define EMR_POLYTEXTOUTW	97
#define EMR_SETICMMODE	98
#define EMR_CREATECOLORSPACE	99
#define EMR_SETCOLORSPACE	100
#define EMR_DELETECOLORSPACE	101
#define EMR_GLSRECORD	102
#define EMR_GLSBOUNDEDRECORD	103
#define EMR_PIXELFORMAT 104

#define EMR_MIN 1
#define EMR_MAX 104

#define ENHMETA_SIGNATURE	1179469088
#define ENHMETA_STOCK_OBJECT	0x80000000

#define GDICPMMENT_INDENTIFIER        0x43494447
#define GDICOMMENT_WINDOWS_METAFILE   0x80000000
#define GDICOMMENT_BEGINGROUP         0x80000001
#define GDICOMMENT_ENDGROUP           0x80000002
#define GDICOMMENT_MULTIFORMATS       0x80000003
#define EPS_SIGNATURE                 0x46535045

#define CCHDEVICENAME 32
#define CCHFORMNAME   32

typedef struct
{
    BYTE   dmDeviceName[CCHDEVICENAME];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
      struct {
	SHORT  dmOrientation;
	SHORT  dmPaperSize;
	SHORT  dmPaperLength;
	SHORT  dmPaperWidth;
      } DUMMYSTRUCTNAME1;
      POINTL dmPosition;
    } DUMMYUNIONNAME1;
    SHORT  dmScale;
    SHORT  dmCopies;
    SHORT  dmDefaultSource;
    SHORT  dmPrintQuality;
    SHORT  dmColor;
    SHORT  dmDuplex;
    SHORT  dmYResolution;
    SHORT  dmTTOption;
    SHORT  dmCollate;
    BYTE   dmFormName[CCHFORMNAME];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
/* #if(WINVER >= 0x0400) */
    DWORD  dmICMMethod;
    DWORD  dmICMIntent;
    DWORD  dmMediaType;
    DWORD  dmDitherType;
    DWORD  dmReserved1;
    DWORD  dmReserved2;
/* #if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400) */
    DWORD  dmPanningWidth;
    DWORD  dmPanningHeight;
/* #endif(WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400) */
/* #endif WINVER >= 0x0400 */
} DEVMODEA, *PDEVMODEA, *LPDEVMODEA;

typedef struct
{
    WCHAR  dmDeviceName[CCHDEVICENAME];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
      struct {
	SHORT  dmOrientation;
	SHORT  dmPaperSize;
	SHORT  dmPaperLength;
	SHORT  dmPaperWidth;
      } DUMMYSTRUCTNAME1;
      POINTL dmPosition;
    } DUMMYUNIONNAME1;
    SHORT  dmScale;
    SHORT  dmCopies;
    SHORT  dmDefaultSource;
    SHORT  dmPrintQuality;
    SHORT  dmColor;
    SHORT  dmDuplex;
    SHORT  dmYResolution;
    SHORT  dmTTOption;
    SHORT  dmCollate;
    WCHAR  dmFormName[CCHFORMNAME];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
/* #if(WINVER >= 0x0400) */
    DWORD  dmICMMethod;
    DWORD  dmICMIntent;
    DWORD  dmMediaType;
    DWORD  dmDitherType;
    DWORD  dmReserved1;
    DWORD  dmReserved2;
/* #if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400) */
    DWORD  dmPanningWidth;
    DWORD  dmPanningHeight;
/* #endif (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400) */
/* #endif  WINVER >= 0x0400 */
} DEVMODEW, *PDEVMODEW, *LPDEVMODEW;

DECL_WINELIB_TYPE_AW(DEVMODE)
DECL_WINELIB_TYPE_AW(PDEVMODE)
DECL_WINELIB_TYPE_AW(LPDEVMODE)

#if defined(__WINE__)
/* Since MS so kindly has different DEVMODE structs for different OS versions we'll provide
 * actualy versions that we can use internally since we need to potentially behave as multiple
 * versions.
 */
typedef struct
{
    BYTE   dmDeviceName[CCHDEVICENAME];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
      struct {
	SHORT  dmOrientation;
	SHORT  dmPaperSize;
	SHORT  dmPaperLength;
	SHORT  dmPaperWidth;
      } DUMMYSTRUCTNAME1;
      POINTL dmPosition;
    } DUMMYUNIONNAME1;
    SHORT  dmScale;
    SHORT  dmCopies;
    SHORT  dmDefaultSource;
    SHORT  dmPrintQuality;
    SHORT  dmColor;
    SHORT  dmDuplex;
    SHORT  dmYResolution;
    SHORT  dmTTOption;
    SHORT  dmCollate;
    BYTE   dmFormName[CCHFORMNAME];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
} WINE_WIN31_DEVMODEA, *WINE_WIN31_PDEVMODEA, *WINE_WIN31_LPDEVMODEA;

typedef struct
{
    BYTE   dmDeviceName[CCHDEVICENAME];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
      struct {
	SHORT  dmOrientation;
	SHORT  dmPaperSize;
	SHORT  dmPaperLength;
	SHORT  dmPaperWidth;
      } DUMMYSTRUCTNAME1;
      POINTL dmPosition;
    } DUMMYUNIONNAME1;
    SHORT  dmScale;
    SHORT  dmCopies;
    SHORT  dmDefaultSource;
    SHORT  dmPrintQuality;
    SHORT  dmColor;
    SHORT  dmDuplex;
    SHORT  dmYResolution;
    SHORT  dmTTOption;
    SHORT  dmCollate;
    BYTE   dmFormName[CCHFORMNAME];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
    DWORD  dmICMMethod;
    DWORD  dmICMIntent;
    DWORD  dmMediaType;
    DWORD  dmDitherType;
    DWORD  dmReserved1;
    DWORD  dmReserved2;
} WINE_WIN95_DEVMODEA, *WINE_WIN95_PDEVMODEA, *WINE_WIN95_LPDEVMODEA;

typedef struct
{
    WCHAR  dmDeviceName[CCHDEVICENAME];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
      struct {
	SHORT  dmOrientation;
	SHORT  dmPaperSize;
	SHORT  dmPaperLength;
	SHORT  dmPaperWidth;
      } DUMMYSTRUCTNAME1;
      POINTL dmPosition;
    } DUMMYUNIONNAME1;
    SHORT  dmScale;
    SHORT  dmCopies;
    SHORT  dmDefaultSource;
    SHORT  dmPrintQuality;
    SHORT  dmColor;
    SHORT  dmDuplex;
    SHORT  dmYResolution;
    SHORT  dmTTOption;
    SHORT  dmCollate;
    WCHAR  dmFormName[CCHFORMNAME];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
} WINE_WIN31_DEVMODEW, *WINE_WIN31_PDEVMODEW, *WINE_WIN31_LPDEVMODEW;

typedef struct
{
    WCHAR  dmDeviceName[CCHDEVICENAME];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
      struct {
	SHORT  dmOrientation;
	SHORT  dmPaperSize;
	SHORT  dmPaperLength;
	SHORT  dmPaperWidth;
      } DUMMYSTRUCTNAME1;
      POINTL dmPosition;
    } DUMMYUNIONNAME1;
    SHORT  dmScale;
    SHORT  dmCopies;
    SHORT  dmDefaultSource;
    SHORT  dmPrintQuality;
    SHORT  dmColor;
    SHORT  dmDuplex;
    SHORT  dmYResolution;
    SHORT  dmTTOption;
    SHORT  dmCollate;
    WCHAR  dmFormName[CCHFORMNAME];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
    DWORD  dmICMMethod;
    DWORD  dmICMIntent;
    DWORD  dmMediaType;
    DWORD  dmDitherType;
    DWORD  dmReserved1;
    DWORD  dmReserved2;
} WINE_WIN95_DEVMODEW, *WINE_WIN95_PDEVMODEW, *WINE_WIN95_LPDEVMODEW;

#endif /* defined(__WINE__) */


#define DM_SPECVERSION  0x401
#define DM_UPDATE	1
#define DM_COPY		2
#define DM_PROMPT	4
#define DM_MODIFY	8

#define DM_IN_BUFFER      DM_MODIFY
#define DM_IN_PROMPT      DM_PROMPT
#define DM_OUT_BUFFER     DM_COPY
#define DM_OUT_DEFAULT    DM_UPDATE

#define DM_ORIENTATION		0x00000001L
#define DM_PAPERSIZE		0x00000002L
#define DM_PAPERLENGTH		0x00000004L
#define DM_PAPERWIDTH		0x00000008L
#define DM_SCALE		0x00000010L
#define DM_POSITION             0x00000020L
#define DM_COPIES		0x00000100L
#define DM_DEFAULTSOURCE	0x00000200L
#define DM_PRINTQUALITY		0x00000400L
#define DM_COLOR		0x00000800L
#define DM_DUPLEX		0x00001000L
#define DM_YRESOLUTION		0x00002000L
#define DM_TTOPTION		0x00004000L
#define DM_COLLATE              0x00008000L
#define DM_FORMNAME             0x00010000L
#define DM_LOGPIXELS            0x00020000L
#define DM_BITSPERPEL           0x00040000L
#define DM_PELSWIDTH            0x00080000L
#define DM_PELSHEIGHT           0x00100000L
#define DM_DISPLAYFLAGS         0x00200000L
#define DM_DISPLAYFREQUENCY     0x00400000L
#define DM_ICMMETHOD            0x00800000L
#define DM_ICMINTENT            0x01000000L
#define DM_MEDIATYPE            0x02000000L
#define DM_DITHERTYPE           0x04000000L
#define DM_PANNINGWIDTH         0x08000000L
#define DM_PANNINGHEIGHT        0x10000000L

#define DMORIENT_PORTRAIT	1
#define DMORIENT_LANDSCAPE	2

#define DMPAPER_FIRST              DMPAPER_LETTER
#define DMPAPER_LETTER		   1
#define DMPAPER_LETTERSMALL        2
#define DMPAPER_TABLOID            3
#define DMPAPER_LEDGER             4
#define DMPAPER_LEGAL		   5
#define DMPAPER_STATEMENT          6
#define DMPAPER_EXECUTIVE	   7
#define DMPAPER_A3		   8
#define DMPAPER_A4		   9
#define DMPAPER_A4SMALL            10
#define DMPAPER_A5		   11
#define DMPAPER_B4                 12
#define DMPAPER_B5                 13
#define DMPAPER_FOLIO              14
#define DMPAPER_QUARTO             15
#define DMPAPER_10X14              16
#define DMPAPER_11X17              17
#define DMPAPER_NOTE               18
#define DMPAPER_ENV_9              19
#define DMPAPER_ENV_10		   20
#define DMPAPER_ENV_11             21
#define DMPAPER_ENV_12             22
#define DMPAPER_ENV_14             23
#define DMPAPER_CSHEET             24
#define DMPAPER_DSHEET             25
#define DMPAPER_ESHEET             26
#define DMPAPER_ENV_DL		   27
#define DMPAPER_ENV_C5		   28
#define DMPAPER_ENV_C3             29
#define DMPAPER_ENV_C4             30
#define DMPAPER_ENV_C6             31
#define DMPAPER_ENV_C65            32
#define DMPAPER_ENV_B4             33
#define DMPAPER_ENV_B5		   34
#define DMPAPER_ENV_B6             35
#define DMPAPER_ENV_ITALY          36
#define DMPAPER_ENV_MONARCH	   37
#define DMPAPER_ENV_PERSONAL       38
#define DMPAPER_FANFOLD_US         39
#define DMPAPER_FANFOLD_STD_GERMAN 40
#define DMPAPER_FANFOLD_LGL_GERMAN 41
#define DMPAPER_ISO_B4             42
#define DMPAPER_JAPANESE_POSTCARD  43
#define DMPAPER_9X11               44
#define DMPAPER_10X11              45
#define DMPAPER_15X11              46
#define DMPAPER_ENV_INVITE         47
#define DMPAPER_RESERVED_48        48
#define DMPAPER_RESERVED_49        49
#define DMPAPER_LETTER_EXTRA       50
#define DMPAPER_LEGAL_EXTRA        51
#define DMPAPER_TABLOID_EXTRA      52
#define DMPAPER_A4_EXTRA           53
#define DMPAPER_LETTER_TRANSVERSE  54
#define DMPAPER_A4_TRANSVERSE      55
#define DMPAPER_LETTER_EXTRA_TRANSVERSE 56
#define DMPAPER_A_PLUS             57
#define DMPAPER_B_PLUS             58
#define DMPAPER_LETTER_PLUS        59
#define DMPAPER_A4_PLUS            60
#define DMPAPER_A5_TRANSVERSE      61
#define DMPAPER_B5_TRANSVERSE      62
#define DMPAPER_A3_EXTRA           63
#define DMPAPER_A5_EXTRA           64
#define DMPAPER_B5_EXTRA           65
#define DMPAPER_A2                 66
#define DMPAPER_A3_TRANSVERSE      67
#define DMPAPER_A3_EXTRA_TRANSVERSE           68
#define DMPAPER_DBL_JAPANESE_POSTCARD         69
#define DMPAPER_A6                 70
#define DMPAPER_JENV_KAKU2         71
#define DMPAPER_JENV_KAKU3         72
#define DMPAPER_JENV_CHOU3         73
#define DMPAPER_JENV_CHOU4         74
#define DMPAPER_LETTER_ROTATED     75
#define DMPAPER_A3_ROTATED         76
#define DMPAPER_A4_ROTATED         77
#define DMPAPER_A5_ROTATED         78
#define DMPAPER_B4_JIS_ROTATED     79
#define DMPAPER_B5_JIS_ROTATED     80
#define DMPAPER_JAPANESE_POSTCARD_ROTATED     81
#define DMPAPER_DBL_JAPANESE_POSTCARD_ROTATED 82
#define DMPAPER_A6_ROTATED         83
#define DMPAPER_JENV_KAKU2_ROTATED 84
#define DMPAPER_JENV_KAKU3_ROTATED 85
#define DMPAPER_JENV_CHOU3_ROTATED 86
#define DMPAPER_JENV_CHOU4_ROTATED 87
#define DMPAPER_B6_JIS             88
#define DMPAPER_B6_JIS_ROTATED     89
#define DMPAPER_12X11              90
#define DMPAPER_JENV_YOU4          91
#define DMPAPER_JENV_YOU4_ROTATED  92
#define DMPAPER_P16K               93
#define DMPAPER_P32K               94
#define DMPAPER_P32KBIG            95
#define DMPAPER_PENV_1             96
#define DMPAPER_PENV_2             97
#define DMPAPER_PENV_3             98
#define DMPAPER_PENV_4             99
#define DMPAPER_PENV_5             100
#define DMPAPER_PENV_6             101
#define DMPAPER_PENV_7             102
#define DMPAPER_PENV_8             103
#define DMPAPER_PENV_9             104
#define DMPAPER_PENV_10            105
#define DMPAPER_P16K_ROTATED       106
#define DMPAPER_P32K_ROTATED       107
#define DMPAPER_P32KBIG_ROTATED    108
#define DMPAPER_PENV_1_ROTATED     109
#define DMPAPER_PENV_2_ROTATED     110
#define DMPAPER_PENV_3_ROTATED     111
#define DMPAPER_PENV_4_ROTATED     112
#define DMPAPER_PENV_5_ROTATED     113
#define DMPAPER_PENV_6_ROTATED     114
#define DMPAPER_PENV_7_ROTATED     115
#define DMPAPER_PENV_8_ROTATED     116
#define DMPAPER_PENV_9_ROTATED     117
#define DMPAPER_PENV_10_ROTATED    118

#define DMPAPER_LAST               DMPAPER_PENV_10_ROTATED
#define DMPAPER_USER               256

#define DMBIN_FIRST             DMBIN_UPPER
#define DMBIN_UPPER		1
#define DMBIN_ONLYONE           1
#define DMBIN_LOWER		2
#define DMBIN_MIDDLE		3
#define DMBIN_MANUAL		4
#define DMBIN_ENVELOPE		5
#define DMBIN_ENVMANUAL		6
#define DMBIN_AUTO		7
#define DMBIN_TRACTOR           8
#define DMBIN_SMALLFMT          9
#define DMBIN_LARGEFMT          10
#define DMBIN_LARGECAPACITY	11
#define DMBIN_CASSETTE          14
#define DMBIN_FORMSOURCE        15
#define DMBIN_LAST              DMBIN_FORMSOURCE
#define DMBIN_USER              256

#define DMRES_DRAFT             (-1)
#define DMRES_LOW               (-2)
#define DMRES_MEDIUM            (-3)
#define DMRES_HIGH              (-4)

#define DMCOLOR_MONOCHROME	1
#define DMCOLOR_COLOR		2

#define DMDUP_SIMPLEX           1
#define DMDUP_VERTICAL          2
#define DMDUP_HORIZONTAL        3

#define DMTT_BITMAP		1
#define DMTT_DOWNLOAD		2
#define DMTT_SUBDEV		3
#define DMTT_DOWNLOAD_OUTLINE   4

#define DMCOLLATE_FALSE         0
#define DMCOLLATE_TRUE          1

#define DMICMMETHOD_NONE        1
#define DMICMMETHOD_SYSTEM      2
#define DMICMMETHOD_DRIVER      3
#define DMICMMETHOD_DEVICE      4
#define DMICMMETHOD_USER        256

#define DMICM_SATURATE          1
#define DMICM_CONTRAST          2
#define DMICM_COLORMETRIC       3
#define DMICM_USER              256

#define DMMEDIA_STANDARD        1
#define DMMEDIA_TRANSPARENCY    2
#define DMMEDIA_GLOSSY          3
#define DMMEDIA_USER            256

#define DMDITHER_NONE           1
#define DMDITHER_COARSE         2
#define DMDITHER_FINE           3
#define DMDITHER_LINEART        4
#define DMDITHER_GRAYSCALE      5
#define DMDITHER_USER           256

typedef struct
{
    INT    cbSize;
    LPCSTR   lpszDocName;
    LPCSTR   lpszOutput;
    LPCSTR   lpszDatatype;
    DWORD    fwType;
} DOCINFOA, *LPDOCINFOA;

typedef struct
{
    INT    cbSize;
    LPCWSTR  lpszDocName;
    LPCWSTR  lpszOutput;
    LPCWSTR  lpszDatatype;
    DWORD    fwType;
} DOCINFOW, *LPDOCINFOW;

DECL_WINELIB_TYPE_AW(DOCINFO)
DECL_WINELIB_TYPE_AW(LPDOCINFO)

#define DI_APPBANDING     0x0001

/* Flags for PolyDraw and GetPath */
#define PT_CLOSEFIGURE          0x0001
#define PT_LINETO               0x0002
#define PT_BEZIERTO             0x0004
#define PT_MOVETO               0x0006

#define	RDH_RECTANGLES  1

typedef struct _RGNDATAHEADER {
    DWORD	dwSize;
    DWORD	iType;
    DWORD	nCount;
    DWORD	nRgnSize;
    RECT	rcBound;
} RGNDATAHEADER,*PRGNDATAHEADER;

typedef struct _RGNDATA {
    RGNDATAHEADER	rdh;
    char		Buffer[1];
} RGNDATA,*PRGNDATA,*LPRGNDATA;

typedef BOOL (CALLBACK *ABORTPROC)(HDC, INT);

typedef struct {
    DWORD	cb;
    CHAR	DeviceName[32];
    CHAR	DeviceString[128];
    DWORD	StateFlags;
    CHAR	DeviceID[128];
    CHAR	DeviceKey[128];
} DISPLAY_DEVICEA,*PDISPLAY_DEVICEA,*LPDISPLAY_DEVICEA;

typedef struct {
    DWORD	cb;
    WCHAR	DeviceName[32];
    WCHAR	DeviceString[128];
    DWORD	StateFlags;
    WCHAR	DeviceID[128];
    WCHAR	DeviceKey[128];
} DISPLAY_DEVICEW,*PDISPLAY_DEVICEW,*LPDISPLAY_DEVICEW;
DECL_WINELIB_TYPE_AW(DISPLAY_DEVICE)
DECL_WINELIB_TYPE_AW(PDISPLAY_DEVICE)
DECL_WINELIB_TYPE_AW(LPDISPLAY_DEVICE)

/* DISPLAY_DEVICE.StateFlags */
#define	DISPLAY_DEVICE_ATTACHED_TO_DESKTOP	0x00000001
#define	DISPLAY_DEVICE_MULTI_DRIVER		0x00000002
#define	DISPLAY_DEVICE_PRIMARY_DEVICE		0x00000004
#define	DISPLAY_DEVICE_MIRRORING_DRIVER		0x00000008
#define	DISPLAY_DEVICE_VGA_COMPATIBLE		0x00000010
#define	DISPLAY_DEVICE_REMOVABLE		0x00000020
#define	DISPLAY_DEVICE_MODESPRUNED		0x08000000
#define	DISPLAY_DEVICE_REMOTE			0x04000000
#define	DISPLAY_DEVICE_DISCONNECT		0x02000000

#define GDI_ERROR                               (0xFFFFFFFFL)
#define HGDI_ERROR                              ((HANDLE)0xFFFFFFFFL)

/* flag used in GetGlyphIndices() */
#define GGI_MARK_NONEXISTING_GLYPHS  0X0001


INT       WINAPI AbortDoc(HDC);
BOOL      WINAPI AbortPath(HDC);
INT       WINAPI AddFontResourceA(LPCSTR);
INT       WINAPI AddFontResourceW(LPCWSTR);
#define     AddFontResource WINELIB_NAME_AW(AddFontResource)
INT       WINAPI AddFontResourceExA(LPCSTR, DWORD, PVOID);
INT       WINAPI AddFontResourceExW(LPCWSTR, DWORD, PVOID);
#define     AddFontResourceEx WINELIB_NAME_AW(AddFontResourceEx)
BOOL      WINAPI AngleArc(HDC, INT, INT, DWORD, FLOAT, FLOAT);
BOOL      WINAPI AnimatePalette(HPALETTE,UINT,UINT,const PALETTEENTRY*);
BOOL      WINAPI Arc(HDC,INT,INT,INT,INT,INT,INT,INT,INT);
BOOL      WINAPI ArcTo(HDC, INT, INT, INT, INT, INT, INT, INT, INT);
BOOL      WINAPI BeginPath(HDC);
BOOL      WINAPI BitBlt(HDC,INT,INT,INT,INT,HDC,INT,INT,DWORD);
INT       WINAPI ChoosePixelFormat(HDC,const LPPIXELFORMATDESCRIPTOR);
BOOL      WINAPI Chord(HDC,INT,INT,INT,INT,INT,INT,INT,INT);
HENHMETAFILE WINAPI CloseEnhMetaFile(HDC);
BOOL      WINAPI CloseFigure(HDC);
HMETAFILE WINAPI CloseMetaFile(HDC);
INT       WINAPI CombineRgn(HRGN,HRGN,HRGN,INT);
BOOL      WINAPI CombineTransform(LPXFORM,const XFORM *,const XFORM *);
HENHMETAFILE WINAPI CopyEnhMetaFileA(HENHMETAFILE,LPCSTR);
HENHMETAFILE WINAPI CopyEnhMetaFileW(HENHMETAFILE,LPCWSTR);
#define     CopyEnhMetaFile WINELIB_NAME_AW(CopyEnhMetaFile)
HMETAFILE WINAPI CopyMetaFileA(HMETAFILE,LPCSTR);
HMETAFILE WINAPI CopyMetaFileW(HMETAFILE,LPCWSTR);
#define     CopyMetaFile WINELIB_NAME_AW(CopyMetaFile)
HBITMAP   WINAPI CreateBitmap(INT,INT,UINT,UINT,LPCVOID);
HBITMAP   WINAPI CreateBitmapIndirect(const BITMAP*);
HBRUSH    WINAPI CreateBrushIndirect(const LOGBRUSH*);
HCOLORSPACE WINAPI CreateColorSpaceA(LPLOGCOLORSPACEA);
HCOLORSPACE WINAPI CreateColorSpaceW(LPLOGCOLORSPACEW);
#define     CreateColorSpace WINELIB_NAME_AW(CreateColorSpace)
HBITMAP   WINAPI CreateCompatibleBitmap(HDC,INT,INT);
HDC       WINAPI CreateCompatibleDC(HDC);
HDC       WINAPI CreateDCA(LPCSTR,LPCSTR,LPCSTR,const DEVMODEA*);
HDC       WINAPI CreateDCW(LPCWSTR,LPCWSTR,LPCWSTR,const DEVMODEW*);
#define     CreateDC WINELIB_NAME_AW(CreateDC)
HBITMAP   WINAPI CreateDIBitmap(HDC,const BITMAPINFOHEADER*,DWORD,
                                    LPCVOID,const BITMAPINFO*,UINT);
HBRUSH    WINAPI CreateDIBPatternBrush(HGLOBAL,UINT);
HBRUSH    WINAPI CreateDIBPatternBrushPt(const void*,UINT);
HBITMAP   WINAPI CreateDIBSection (HDC, BITMAPINFO *, UINT,
				       LPVOID *, HANDLE, DWORD offset);
HBITMAP   WINAPI CreateDiscardableBitmap(HDC,INT,INT);
HRGN      WINAPI CreateEllipticRgn(INT,INT,INT,INT);
HRGN      WINAPI CreateEllipticRgnIndirect(const RECT *);
HDC       WINAPI CreateEnhMetaFileA(HDC,LPCSTR,const RECT*,LPCSTR);
HDC       WINAPI CreateEnhMetaFileW(HDC,LPCWSTR,const RECT*,LPCWSTR);
#define     CreateEnhMetaFile WINELIB_NAME_AW(CreateEnhMetaFile)
HFONT     WINAPI CreateFontA(INT,INT,INT,INT,INT,DWORD,DWORD,
                                 DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);
HFONT     WINAPI CreateFontW(INT,INT,INT,INT,INT,DWORD,DWORD,
                                 DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCWSTR);
#define     CreateFont WINELIB_NAME_AW(CreateFont)
HFONT     WINAPI CreateFontIndirectA(const LOGFONTA*);
HFONT     WINAPI CreateFontIndirectW(const LOGFONTW*);
#define     CreateFontIndirect WINELIB_NAME_AW(CreateFontIndirect)
HPALETTE  WINAPI CreateHalftonePalette(HDC);
HBRUSH    WINAPI CreateHatchBrush(INT,COLORREF);
HDC       WINAPI CreateICA(LPCSTR,LPCSTR,LPCSTR,const DEVMODEA*);
HDC       WINAPI CreateICW(LPCWSTR,LPCWSTR,LPCWSTR,const DEVMODEW*);
#define     CreateIC WINELIB_NAME_AW(CreateIC)
HDC       WINAPI CreateMetaFileA(LPCSTR);
HDC       WINAPI CreateMetaFileW(LPCWSTR);
#define     CreateMetaFile WINELIB_NAME_AW(CreateMetaFile)
HPALETTE  WINAPI CreatePalette(const LOGPALETTE*);
HBRUSH    WINAPI CreatePatternBrush(HBITMAP);
HPEN      WINAPI CreatePen(INT,INT,COLORREF);
HPEN      WINAPI CreatePenIndirect(const LOGPEN*);
HRGN      WINAPI CreatePolyPolygonRgn(const POINT*,const INT*,INT,INT);
HRGN      WINAPI CreatePolygonRgn(const POINT*,INT,INT);
HRGN      WINAPI CreateRectRgn(INT,INT,INT,INT);
HRGN      WINAPI CreateRectRgnIndirect(const RECT*);
HRGN      WINAPI CreateRoundRectRgn(INT,INT,INT,INT,INT,INT);
BOOL      WINAPI CreateScalableFontResourceA(DWORD,LPCSTR,LPCSTR,LPCSTR);
BOOL      WINAPI CreateScalableFontResourceW(DWORD,LPCWSTR,LPCWSTR,LPCWSTR);
#define     CreateScalableFontResource WINELIB_NAME_AW(CreateScalableFontResource)
HBRUSH    WINAPI CreateSolidBrush(COLORREF);
BOOL      WINAPI DPtoLP(HDC,LPPOINT,INT);
BOOL      WINAPI DeleteColorSpace(HCOLORSPACE);
BOOL      WINAPI DeleteDC(HDC);
BOOL      WINAPI DeleteEnhMetaFile(HENHMETAFILE);
BOOL      WINAPI DeleteMetaFile(HMETAFILE);
BOOL      WINAPI DeleteObject(HGDIOBJ);
INT       WINAPI DescribePixelFormat(HDC,int,UINT,
                                       LPPIXELFORMATDESCRIPTOR);
INT       WINAPI DrawEscape(HDC,INT,INT,LPCSTR);
BOOL      WINAPI Ellipse(HDC,INT,INT,INT,INT);
INT       WINAPI EndDoc(HDC);
BOOL      WINAPI EndPath(HDC);
BOOL      WINAPI EnumEnhMetaFile(HDC,HENHMETAFILE,ENHMFENUMPROC,LPVOID,const RECT*);
INT       WINAPI EnumFontFamiliesA(HDC,LPCSTR,FONTENUMPROCA,LPARAM);
INT       WINAPI EnumFontFamiliesW(HDC,LPCWSTR,FONTENUMPROCW,LPARAM);
#define     EnumFontFamilies WINELIB_NAME_AW(EnumFontFamilies)
INT       WINAPI EnumFontFamiliesExA(HDC,LPLOGFONTA,FONTENUMPROCEXA,LPARAM,DWORD);
INT       WINAPI EnumFontFamiliesExW(HDC,LPLOGFONTW,FONTENUMPROCEXW,LPARAM,DWORD);
#define     EnumFontFamiliesEx WINELIB_NAME_AW(EnumFontFamiliesEx)
INT       WINAPI EnumFontsA(HDC,LPCSTR,FONTENUMPROCA,LPARAM);
INT       WINAPI EnumFontsW(HDC,LPCWSTR,FONTENUMPROCW,LPARAM);
#define     EnumFonts WINELIB_NAME_AW(EnumFonts)
BOOL      WINAPI EnumMetaFile(HDC,HMETAFILE,MFENUMPROC,LPARAM);
INT       WINAPI EnumObjects(HDC,INT,GOBJENUMPROC,LPARAM);
BOOL      WINAPI EqualRgn(HRGN,HRGN);
INT       WINAPI Escape(HDC,INT,INT,LPCSTR,LPVOID);
INT       WINAPI ExcludeClipRect(HDC,INT,INT,INT,INT);
HPEN      WINAPI ExtCreatePen(DWORD,DWORD,const LOGBRUSH*,DWORD,const DWORD*);
HRGN      WINAPI ExtCreateRegion(const XFORM*,DWORD,const RGNDATA*);
INT       WINAPI ExtEscape(HDC,INT,INT,LPCSTR,INT,LPSTR);
BOOL      WINAPI ExtFloodFill(HDC,INT,INT,COLORREF,UINT);
INT       WINAPI ExtSelectClipRgn(HDC,HRGN,INT);
BOOL      WINAPI ExtTextOutA(HDC,INT,INT,UINT,const RECT*,
                                 LPCSTR,UINT,const INT*);
BOOL      WINAPI ExtTextOutW(HDC,INT,INT,UINT,const RECT*,
                                 LPCWSTR,UINT,const INT*);
#define     ExtTextOut WINELIB_NAME_AW(ExtTextOut)
BOOL      WINAPI FillPath(HDC);
BOOL      WINAPI FillRgn(HDC,HRGN,HBRUSH);
BOOL      WINAPI FixBrushOrgEx(HDC,INT,INT,LPPOINT);
BOOL      WINAPI FlattenPath(HDC);
BOOL      WINAPI FloodFill(HDC,INT,INT,COLORREF);
BOOL      WINAPI FrameRgn(HDC,HRGN,HBRUSH,INT,INT);
BOOL      WINAPI GdiAlphaBlend(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
BOOL      WINAPI GdiComment(HDC,UINT,const BYTE *);
BOOL      WINAPI GdiFlush(void);
INT       WINAPI GetArcDirection(HDC);
BOOL      WINAPI GetAspectRatioFilterEx(HDC,LPSIZE);
LONG      WINAPI GetBitmapBits(HBITMAP,LONG,LPVOID);
BOOL      WINAPI GetBitmapDimensionEx(HBITMAP,LPSIZE);
BOOL      WINAPI GetBrushOrgEx(HDC,LPPOINT);
COLORREF  WINAPI GetBkColor(HDC);
INT       WINAPI GetBkMode(HDC);
UINT      WINAPI GetBoundsRect(HDC,LPRECT,UINT);
BOOL      WINAPI GetCharABCWidthsA(HDC,UINT,UINT,LPABC);
BOOL      WINAPI GetCharABCWidthsW(HDC,UINT,UINT,LPABC);
#define     GetCharABCWidths WINELIB_NAME_AW(GetCharABCWidths)
BOOL      WINAPI GetCharABCWidthsI(HDC,UINT,UINT,LPWORD,LPABC);
BOOL      WINAPI GetCharABCWidthsFloatA(HDC,UINT,UINT,LPABCFLOAT);
BOOL      WINAPI GetCharABCWidthsFloatW(HDC,UINT,UINT,LPABCFLOAT);
#define     GetCharABCWidthsFloat WINELIB_NAME_AW(GetCharABCWidthsFloat)
DWORD     WINAPI GetCharacterPlacementA(HDC,LPCSTR,INT,INT,GCP_RESULTSA*,DWORD);
DWORD     WINAPI GetCharacterPlacementW(HDC,LPCWSTR,INT,INT,GCP_RESULTSW*,DWORD);
#define     GetCharacterPlacement WINELIB_NAME_AW(GetCharacterPlacement)
BOOL      WINAPI GetCharWidth32A(HDC,UINT,UINT,LPINT);
BOOL      WINAPI GetCharWidth32W(HDC,UINT,UINT,LPINT);
#define     GetCharWidthA GetCharWidth32A
#define     GetCharWidthW GetCharWidth32W
#define     GetCharWidth32 WINELIB_NAME_AW(GetCharWidth32)
#define     GetCharWidth WINELIB_NAME_AW(GetCharWidth)
BOOL      WINAPI GetCharWidthFloatA(HDC,UINT,UINT,PFLOAT);
BOOL      WINAPI GetCharWidthFloatW(HDC,UINT,UINT,PFLOAT);
#define     GetCharWidthFloat WINELIB_NAME_AW(GetCharWidthFloat)
INT       WINAPI GetClipBox(HDC,LPRECT);
INT       WINAPI GetClipRgn(HDC,HRGN);
BOOL      WINAPI GetColorAdjustment(HDC, LPCOLORADJUSTMENT);
HANDLE    WINAPI GetCurrentObject(HDC,UINT);
BOOL      WINAPI GetCurrentPositionEx(HDC,LPPOINT);
INT       WINAPI GetDeviceCaps(HDC,INT);
BOOL      WINAPI GetDeviceGammaRamp(HDC,LPVOID);
COLORREF  WINAPI GetDCBrushColor(HDC);
BOOL      WINAPI GetDCOrgEx(HDC,LPPOINT);
COLORREF  WINAPI GetDCPenColor(HDC);
UINT      WINAPI GetDIBColorTable(HDC,UINT,UINT,RGBQUAD*);
INT       WINAPI GetDIBits(HDC,HBITMAP,UINT,UINT,LPVOID,LPBITMAPINFO,UINT);
HENHMETAFILE WINAPI GetEnhMetaFileA(LPCSTR);
HENHMETAFILE WINAPI GetEnhMetaFileW(LPCWSTR);
#define     GetEnhMetaFile WINELIB_NAME_AW(GetEnhMetaFile)
UINT      WINAPI GetEnhMetaFileBits(HENHMETAFILE,UINT,LPBYTE);
UINT        WINAPI GetEnhMetaFileDescriptionA(HENHMETAFILE,UINT,LPSTR);
UINT        WINAPI GetEnhMetaFileDescriptionW(HENHMETAFILE,UINT,LPWSTR);
#define     GetEnhMetaFileDescription WINELIB_NAME_AW(GetEnhMetaFileDescription)
UINT      WINAPI GetEnhMetaFileHeader(HENHMETAFILE,UINT,LPENHMETAHEADER);
UINT      WINAPI GetEnhMetaFilePaletteEntries(HENHMETAFILE,UINT,LPPALETTEENTRY);
DWORD     WINAPI GetFontData(HDC,DWORD,DWORD,LPVOID,DWORD);
DWORD     WINAPI GetFontLanguageInfo(HDC);
DWORD     WINAPI GetFontUnicodeRanges(HDC,LPGLYPHSET);
DWORD     WINAPI GetGlyphIndicesA(HDC,LPCSTR,INT,LPWORD,DWORD);
DWORD     WINAPI GetGlyphIndicesW(HDC,LPCWSTR,INT,LPWORD,DWORD);
#define     GetGlyphIndices WINELIB_NAME_AW(GetGlyphIndices)
DWORD     WINAPI GetGlyphOutlineA(HDC,UINT,UINT,LPGLYPHMETRICS,DWORD,LPVOID,const MAT2*);
DWORD     WINAPI GetGlyphOutlineW(HDC,UINT,UINT,LPGLYPHMETRICS,DWORD,LPVOID,const MAT2*);
#define     GetGlyphOutline WINELIB_NAME_AW(GetGlyphOutline)
INT       WINAPI GetGraphicsMode(HDC);
DWORD     WINAPI GetKerningPairsA(HDC,DWORD,LPKERNINGPAIR);
DWORD     WINAPI GetKerningPairsW(HDC,DWORD,LPKERNINGPAIR);
#define     GetKerningPairs WINELIB_NAME_AW(GetKerningPairs)
DWORD       WINAPI GetLayout(HDC);
BOOL        WINAPI GetLogColorSpaceA(HCOLORSPACE,LPLOGCOLORSPACEA,DWORD);
BOOL        WINAPI GetLogColorSpaceW(HCOLORSPACE,LPLOGCOLORSPACEW,DWORD);
#define     GetLogColorSpace WINELIB_NAME_AW(GetLogColorSpace)
INT       WINAPI GetMapMode(HDC);
HMETAFILE WINAPI GetMetaFileA(LPCSTR);
HMETAFILE WINAPI GetMetaFileW(LPCWSTR);
#define     GetMetaFile WINELIB_NAME_AW(GetMetaFile)
UINT        WINAPI GetMetaFileBitsEx(HMETAFILE,UINT,LPVOID);
INT         WINAPI GetMetaRgn(HDC,HRGN);
BOOL        WINAPI GetMiterLimit(HDC, PFLOAT);
DWORD       WINAPI GetNearestColor(HDC,DWORD);
UINT        WINAPI GetNearestPaletteIndex(HPALETTE,COLORREF);
INT         WINAPI GetObjectA(HANDLE,INT,LPVOID);
INT         WINAPI GetObjectW(HANDLE,INT,LPVOID);
#define     GetObject WINELIB_NAME_AW(GetObject)
DWORD     WINAPI GetObjectType(HANDLE);
UINT      WINAPI GetOutlineTextMetricsA(HDC,UINT,LPOUTLINETEXTMETRICA);
UINT      WINAPI GetOutlineTextMetricsW(HDC,UINT,LPOUTLINETEXTMETRICW);
#define     GetOutlineTextMetrics WINELIB_NAME_AW(GetOutlineTextMetrics)
UINT      WINAPI GetPaletteEntries(HPALETTE,UINT,UINT,LPPALETTEENTRY);
INT       WINAPI GetPath(HDC,LPPOINT,LPBYTE,INT);
COLORREF  WINAPI GetPixel(HDC,INT,INT);
INT       WINAPI GetPixelFormat(HDC);
INT       WINAPI GetPolyFillMode(HDC);
BOOL      WINAPI GetRasterizerCaps(LPRASTERIZER_STATUS,UINT);
DWORD     WINAPI GetRegionData(HRGN,DWORD,LPRGNDATA);
INT       WINAPI GetRelAbs(HDC,DWORD);
INT       WINAPI GetRgnBox(HRGN,LPRECT);
INT       WINAPI GetROP2(HDC);
HGDIOBJ   WINAPI GetStockObject(INT);
INT       WINAPI GetStretchBltMode(HDC);
UINT      WINAPI GetSystemPaletteEntries(HDC,UINT,UINT,LPPALETTEENTRY);
UINT      WINAPI GetSystemPaletteUse(HDC);
UINT      WINAPI GetTextAlign(HDC);
INT       WINAPI GetTextCharacterExtra(HDC);
UINT      WINAPI GetTextCharset(HDC);
UINT      WINAPI GetTextCharsetInfo(HDC,LPFONTSIGNATURE,DWORD);
COLORREF  WINAPI GetTextColor(HDC);
BOOL      WINAPI GetTextExtentExPointA(HDC,LPCSTR,INT,INT,
                                           LPINT,LPINT,LPSIZE);
BOOL      WINAPI GetTextExtentExPointW(HDC,LPCWSTR,INT,INT,
                                           LPINT,LPINT,LPSIZE);
BOOL      WINAPI GetTextExtentPointA(HDC,LPCSTR,INT,LPSIZE);
BOOL      WINAPI GetTextExtentPointW(HDC,LPCWSTR,INT,LPSIZE);
#define     GetTextExtentPoint WINELIB_NAME_AW(GetTextExtentPoint)
BOOL      WINAPI GetTextExtentPointI(HDC,LPWORD,INT,LPSIZE);
BOOL      WINAPI GetTextExtentPoint32A(HDC,LPCSTR,INT,LPSIZE);
BOOL      WINAPI GetTextExtentPoint32W(HDC,LPCWSTR,INT,LPSIZE);
#define     GetTextExtentPoint32 WINELIB_NAME_AW(GetTextExtentPoint32)
#define     GetTextExtentExPoint WINELIB_NAME_AW(GetTextExtentExPoint)
INT       WINAPI GetTextFaceA(HDC,INT,LPSTR);
INT       WINAPI GetTextFaceW(HDC,INT,LPWSTR);
#define     GetTextFace WINELIB_NAME_AW(GetTextFace)
BOOL      WINAPI GetTextMetricsA(HDC,LPTEXTMETRICA);
BOOL      WINAPI GetTextMetricsW(HDC,LPTEXTMETRICW);
#define     GetTextMetrics WINELIB_NAME_AW(GetTextMetrics)
BOOL      WINAPI GetViewportExtEx(HDC,LPSIZE);
BOOL      WINAPI GetViewportOrgEx(HDC,LPPOINT);
BOOL      WINAPI GetWindowExtEx(HDC,LPSIZE);
BOOL      WINAPI GetWindowOrgEx(HDC,LPPOINT);
BOOL      WINAPI GetWorldTransform(HDC,LPXFORM);
INT       WINAPI IntersectClipRect(HDC,INT,INT,INT,INT);
BOOL      WINAPI InvertRgn(HDC,HRGN);
BOOL      WINAPI LineDDA(INT,INT,INT,INT,LINEDDAPROC,LPARAM);
BOOL      WINAPI LineTo(HDC,INT,INT);
BOOL      WINAPI LPtoDP(HDC,LPPOINT,INT);
BOOL      WINAPI MaskBlt(HDC,INT,INT,INT,INT,HDC,INT,INT,HBITMAP,INT,INT,DWORD);
BOOL      WINAPI ModifyWorldTransform(HDC,const XFORM *, DWORD);
BOOL      WINAPI MoveToEx(HDC,INT,INT,LPPOINT);
/* FIXME This is defined in kernel32.spec !?*/
INT       WINAPI MulDiv(INT,INT,INT);
INT       WINAPI OffsetClipRgn(HDC,INT,INT);
INT       WINAPI OffsetRgn(HRGN,INT,INT);
BOOL      WINAPI OffsetViewportOrgEx(HDC,INT,INT,LPPOINT);
BOOL      WINAPI OffsetWindowOrgEx(HDC,INT,INT,LPPOINT);
BOOL      WINAPI PaintRgn(HDC,HRGN);
BOOL      WINAPI PatBlt(HDC,INT,INT,INT,INT,DWORD);
HRGN      WINAPI PathToRegion(HDC);
BOOL      WINAPI Pie(HDC,INT,INT,INT,INT,INT,INT,INT,INT);
BOOL      WINAPI PlayEnhMetaFile(HDC,HENHMETAFILE,const RECT*);
BOOL      WINAPI PlayEnhMetaFileRecord(HDC,LPHANDLETABLE,const ENHMETARECORD*,UINT);
BOOL      WINAPI PlayMetaFile(HDC,HMETAFILE);
BOOL      WINAPI PlayMetaFileRecord(HDC,LPHANDLETABLE,LPMETARECORD,UINT);
BOOL      WINAPI PlgBlt(HDC,const POINT*,HDC,INT,INT,INT,INT,HBITMAP,INT,INT);
BOOL      WINAPI PolyBezier(HDC,const POINT*,DWORD);
BOOL      WINAPI PolyBezierTo(HDC,const POINT*,DWORD);
BOOL      WINAPI PolyDraw(HDC,const POINT*,const BYTE*,DWORD);
BOOL      WINAPI PolyPolygon(HDC,const POINT*,const INT*,UINT);
BOOL      WINAPI PolyPolyline(HDC,const POINT*,const DWORD*,DWORD);
BOOL      WINAPI Polygon(HDC,const POINT*,INT);
BOOL      WINAPI Polyline(HDC,const POINT*,INT);
BOOL      WINAPI PolylineTo(HDC,const POINT*,DWORD);
BOOL      WINAPI PtInRegion(HRGN,INT,INT);
BOOL      WINAPI PtVisible(HDC,INT,INT);
UINT      WINAPI RealizePalette(HDC);
BOOL      WINAPI Rectangle(HDC,INT,INT,INT,INT);
BOOL      WINAPI RectInRegion(HRGN,const RECT *);
BOOL      WINAPI RectVisible(HDC,const RECT*);
BOOL      WINAPI RemoveFontResourceA(LPCSTR);
BOOL      WINAPI RemoveFontResourceW(LPCWSTR);
#define     RemoveFontResource WINELIB_NAME_AW(RemoveFontResource)
BOOL      WINAPI RemoveFontResourceExA(LPCSTR, DWORD, PVOID);
BOOL      WINAPI RemoveFontResourceExW(LPCWSTR, DWORD, PVOID);
#define     RemoveFontResourceEx WINELIB_NAME_AW(RemoveFontResourceEx)
HDC       WINAPI ResetDCA(HDC,const DEVMODEA *);
HDC       WINAPI ResetDCW(HDC,const DEVMODEW *);
#define     ResetDC WINELIB_NAME_AW(ResetDC)
BOOL      WINAPI ResizePalette(HPALETTE,UINT);
BOOL      WINAPI RestoreDC(HDC,INT);
BOOL      WINAPI RoundRect(HDC,INT,INT,INT,INT,INT,INT);
INT       WINAPI SaveDC(HDC);
BOOL      WINAPI ScaleViewportExtEx(HDC,INT,INT,INT,INT,LPSIZE);
BOOL      WINAPI ScaleWindowExtEx(HDC,INT,INT,INT,INT,LPSIZE);
BOOL      WINAPI SelectClipPath(HDC,INT);
INT       WINAPI SelectClipRgn(HDC,HRGN);
HGDIOBJ   WINAPI SelectObject(HDC,HGDIOBJ);
HPALETTE  WINAPI SelectPalette(HDC,HPALETTE,BOOL);
INT       WINAPI SetAbortProc(HDC,ABORTPROC);
INT       WINAPI SetArcDirection(HDC,INT);
LONG      WINAPI SetBitmapBits(HBITMAP,LONG,LPCVOID);
BOOL      WINAPI SetBitmapDimensionEx(HBITMAP,INT,INT,LPSIZE);
COLORREF  WINAPI SetBkColor(HDC,COLORREF);
INT       WINAPI SetBkMode(HDC,INT);
UINT      WINAPI SetBoundsRect(HDC,const RECT*,UINT);
BOOL      WINAPI SetBrushOrgEx(HDC,INT,INT,LPPOINT);
BOOL      WINAPI SetColorAdjustment(HDC,const COLORADJUSTMENT*);
HCOLORSPACE WINAPI SetColorSpace(HDC,HCOLORSPACE);
BOOL      WINAPI SetDeviceGammaRamp(HDC,LPVOID);
UINT      WINAPI SetDIBColorTable(HDC,UINT,UINT,RGBQUAD*);
INT       WINAPI SetDIBits(HDC,HBITMAP,UINT,UINT,LPCVOID,const BITMAPINFO*,UINT);
INT       WINAPI SetDIBitsToDevice(HDC,INT,INT,DWORD,DWORD,INT,
                         INT,UINT,UINT,LPCVOID,const BITMAPINFO*,UINT);
HENHMETAFILE WINAPI SetEnhMetaFileBits(UINT,const BYTE *);
INT         WINAPI SetGraphicsMode(HDC,INT);
INT         WINAPI SetICMMode(HDC,INT);
DWORD       WINAPI SetLayout(HDC,DWORD);
INT         WINAPI SetMapMode(HDC,INT);
DWORD       WINAPI SetMapperFlags(HDC,DWORD);
HMETAFILE   WINAPI SetMetaFileBitsEx(UINT,const BYTE*);
INT         WINAPI SetMetaRgn(HDC);
BOOL      WINAPI SetMiterLimit(HDC, FLOAT, PFLOAT);
UINT      WINAPI SetPaletteEntries(HPALETTE,UINT,UINT,LPPALETTEENTRY);
COLORREF  WINAPI SetPixel(HDC,INT,INT,COLORREF);
BOOL      WINAPI SetPixelV(HDC,INT,INT,COLORREF);
BOOL      WINAPI SetPixelFormat(HDC,int,const PIXELFORMATDESCRIPTOR*);
INT       WINAPI SetPolyFillMode(HDC,INT);
BOOL      WINAPI SetRectRgn(HRGN,INT,INT,INT,INT);
INT       WINAPI SetRelAbs(HDC,INT);
INT       WINAPI SetROP2(HDC,INT);
INT       WINAPI SetStretchBltMode(HDC,INT);
UINT      WINAPI SetSystemPaletteUse(HDC,UINT);
UINT      WINAPI SetTextAlign(HDC,UINT);
INT       WINAPI SetTextCharacterExtra(HDC,INT);
COLORREF  WINAPI SetTextColor(HDC,COLORREF);
BOOL      WINAPI SetTextJustification(HDC,INT,INT);
BOOL      WINAPI SetViewportExtEx(HDC,INT,INT,LPSIZE);
BOOL      WINAPI SetViewportOrgEx(HDC,INT,INT,LPPOINT);
BOOL      WINAPI SetWindowExtEx(HDC,INT,INT,LPSIZE);
BOOL      WINAPI SetWindowOrgEx(HDC,INT,INT,LPPOINT);
HENHMETAFILE WINAPI SetWinMetaFileBits(UINT,CONST BYTE*,HDC,CONST METAFILEPICT *);
BOOL      WINAPI SetWorldTransform(HDC,const XFORM*);
INT       WINAPI StartDocA(HDC,const DOCINFOA*);
INT       WINAPI StartDocW(HDC,const DOCINFOW*);
#define     StartDoc WINELIB_NAME_AW(StartDoc)
INT       WINAPI StartPage(HDC);
INT       WINAPI EndPage(HDC);
BOOL      WINAPI StretchBlt(HDC,INT,INT,INT,INT,HDC,INT,
                                INT,INT,INT,DWORD);
INT       WINAPI StretchDIBits(HDC,INT,INT,INT,INT,INT,INT,
                       INT,INT,const VOID*,const BITMAPINFO*,UINT,DWORD);
BOOL      WINAPI StrokeAndFillPath(HDC);
BOOL      WINAPI StrokePath(HDC);
BOOL      WINAPI SwapBuffers(HDC);
BOOL      WINAPI TextOutA(HDC,INT,INT,LPCSTR,INT);
BOOL      WINAPI TextOutW(HDC,INT,INT,LPCWSTR,INT);
#define     TextOut WINELIB_NAME_AW(TextOut)
BOOL      WINAPI TranslateCharsetInfo(LPDWORD,LPCHARSETINFO,DWORD);
BOOL      WINAPI UnrealizeObject(HGDIOBJ);
BOOL      WINAPI UpdateColors(HDC);
BOOL      WINAPI WidenPath(HDC);
BOOL      WINAPI PolyTextOutA(HDC,PPOLYTEXTA,INT);
BOOL      WINAPI PolyTextOutW(HDC,PPOLYTEXTW,INT);
#define   PolyTextOut WINELIB_NAME_AW(PolyTextOut)

/* These defines are used by wglSwapLayerBuffers */
#define WGL_SWAP_MAIN_PLANE (1 <<  0)
#define WGL_SWAP_OVERLAY1   (1 <<  1)
#define WGL_SWAP_OVERLAY2   (1 <<  2)
#define WGL_SWAP_OVERLAY3   (1 <<  3)
#define WGL_SWAP_OVERLAY4   (1 <<  4)
#define WGL_SWAP_OVERLAY5   (1 <<  5)
#define WGL_SWAP_OVERLAY6   (1 <<  6)
#define WGL_SWAP_OVERLAY7   (1 <<  7)
#define WGL_SWAP_OVERLAY8   (1 <<  8)
#define WGL_SWAP_OVERLAY9   (1 <<  9)
#define WGL_SWAP_OVERLAY10  (1 << 10)
#define WGL_SWAP_OVERLAY11  (1 << 11)
#define WGL_SWAP_OVERLAY12  (1 << 12)
#define WGL_SWAP_OVERLAY13  (1 << 13)
#define WGL_SWAP_OVERLAY14  (1 << 14)
#define WGL_SWAP_OVERLAY15  (1 << 15)
#define WGL_SWAP_UNDERLAY1  (1 << 16)
#define WGL_SWAP_UNDERLAY2  (1 << 17)
#define WGL_SWAP_UNDERLAY3  (1 << 18)
#define WGL_SWAP_UNDERLAY4  (1 << 19)
#define WGL_SWAP_UNDERLAY5  (1 << 20)
#define WGL_SWAP_UNDERLAY6  (1 << 21)
#define WGL_SWAP_UNDERLAY7  (1 << 22)
#define WGL_SWAP_UNDERLAY8  (1 << 23)
#define WGL_SWAP_UNDERLAY9  (1 << 24)
#define WGL_SWAP_UNDERLAY10 (1 << 25)
#define WGL_SWAP_UNDERLAY11 (1 << 26)
#define WGL_SWAP_UNDERLAY12 (1 << 27)
#define WGL_SWAP_UNDERLAY13 (1 << 28)
#define WGL_SWAP_UNDERLAY14 (1 << 29)
#define WGL_SWAP_UNDERLAY15 (1 << 30)

#ifdef __cplusplus
}
#endif

#endif /* !NOGDI */
#endif /* _WINGDI_ */
