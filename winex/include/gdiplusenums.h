#ifndef _GDIPLUSENUMS_H
#define _GDIPLUSENUMS_H

typedef UINT GraphicsState;
typedef UINT GraphicsContainer;

enum Unit
{
    UnitWorld       = 0,
    UnitDisplay     = 1,
    UnitPixel       = 2,
    UnitPoint       = 3,
    UnitInch        = 4,
    UnitDocument    = 5,
    UnitMillimeter  = 6
};

enum BrushType
{
   BrushTypeSolidColor       = 0,
   BrushTypeHatchFill        = 1,
   BrushTypeTextureFill      = 2,
   BrushTypePathGradient     = 3,
   BrushTypeLinearGradient   = 4
};

enum FillMode
{
    FillModeAlternate   = 0,
    FillModeWinding     = 1
};

enum LineCap
{
    LineCapFlat             = 0x00,
    LineCapSquare           = 0x01,
    LineCapRound            = 0x02,
    LineCapTriangle         = 0x03,

    LineCapNoAnchor         = 0x10,
    LineCapSquareAnchor     = 0x11,
    LineCapRoundAnchor      = 0x12,
    LineCapDiamondAnchor    = 0x13,
    LineCapArrowAnchor      = 0x14,

    LineCapCustom           = 0xff,
    LineCapAnchorMask       = 0xf0
};

enum PathPointType{
    PathPointTypeStart          = 0,
    PathPointTypeLine           = 1,
    PathPointTypeBezier         = 3,
    PathPointTypePathTypeMask   = 7,
    PathPointTypePathDashMode   = 16,
    PathPointTypePathMarker     = 32,
    PathPointTypeCloseSubpath   = 128,
    PathPointTypeBezier3        = 3
};

enum PenType
{
   PenTypeSolidColor       = BrushTypeSolidColor,
   PenTypeHatchFill        = BrushTypeHatchFill,
   PenTypeTextureFill      = BrushTypeTextureFill,
   PenTypePathGradient     = BrushTypePathGradient,
   PenTypeLinearGradient   = BrushTypeLinearGradient,
   PenTypeUnknown          = -1
};

enum LineJoin
{
    LineJoinMiter           = 0,
    LineJoinBevel           = 1,
    LineJoinRound           = 2,
    LineJoinMiterClipped    = 3
};

enum QualityMode
{
    QualityModeInvalid  = -1,
    QualityModeDefault  = 0,
    QualityModeLow      = 1,
    QualityModeHigh     = 2
};

enum SmoothingMode
{
    SmoothingModeInvalid     = QualityModeInvalid,
    SmoothingModeDefault     = QualityModeDefault,
    SmoothingModeHighSpeed   = QualityModeLow,
    SmoothingModeHighQuality = QualityModeHigh,
    SmoothingModeNone,
    SmoothingModeAntiAlias
};

enum CompositingQuality
{
    CompositingQualityInvalid          = QualityModeInvalid,
    CompositingQualityDefault          = QualityModeDefault,
    CompositingQualityHighSpeed        = QualityModeLow,
    CompositingQualityHighQuality      = QualityModeHigh,
    CompositingQualityGammaCorrected,
    CompositingQualityAssumeLinear
};

enum InterpolationMode
{
    InterpolationModeInvalid        = QualityModeInvalid,
    InterpolationModeDefault        = QualityModeDefault,
    InterpolationModeLowQuality     = QualityModeLow,
    InterpolationModeHighQuality    = QualityModeHigh,
    InterpolationModeBilinear,
    InterpolationModeBicubic,
    InterpolationModeNearestNeighbor,
    InterpolationModeHighQualityBilinear,
    InterpolationModeHighQualityBicubic
};

enum PenAlignment
{
    PenAlignmentCenter   = 0,
    PenAlignmentInset    = 1
};

enum PixelOffsetMode
{
    PixelOffsetModeInvalid     = QualityModeInvalid,
    PixelOffsetModeDefault     = QualityModeDefault,
    PixelOffsetModeHighSpeed   = QualityModeLow,
    PixelOffsetModeHighQuality = QualityModeHigh,
    PixelOffsetModeNone,
    PixelOffsetModeHalf
};

enum DashCap
{
    DashCapFlat     = 0,
    DashCapRound    = 2,
    DashCapTriangle = 3
};

enum DashStyle
{
    DashStyleSolid,
    DashStyleDash,
    DashStyleDot,
    DashStyleDashDot,
    DashStyleDashDotDot,
    DashStyleCustom
};

enum MatrixOrder
{
    MatrixOrderPrepend = 0,
    MatrixOrderAppend  = 1
};

enum ImageType
{
    ImageTypeUnknown,
    ImageTypeBitmap,
    ImageTypeMetafile
};

enum WarpMode {
    WarpModePerspective,
    WarpModeBilinear
};

enum WrapMode
{
    WrapModeTile,
    WrapModeTileFlipX,
    WrapModeTileFlipY,
    WrapModeTileFlipXY,
    WrapModeClamp
};

enum MetafileType
{
    MetafileTypeInvalid,
    MetafileTypeWmf,
    MetafileTypeWmfPlaceable,
    MetafileTypeEmf,
    MetafileTypeEmfPlusOnly,
    MetafileTypeEmfPlusDual
};

enum LinearGradientMode
{
    LinearGradientModeHorizontal,
    LinearGradientModeVertical,
    LinearGradientModeForwardDiagonal,
    LinearGradientModeBackwardDiagonal
};

enum EmfType
{
    EmfTypeEmfOnly     = MetafileTypeEmf,
    EmfTypeEmfPlusOnly = MetafileTypeEmfPlusOnly,
    EmfTypeEmfPlusDual = MetafileTypeEmfPlusDual
};

enum CompositingMode
{
    CompositingModeSourceOver,
    CompositingModeSourceCopy
};

enum TextRenderingHint
{
    TextRenderingHintSystemDefault = 0,
    TextRenderingHintSingleBitPerPixelGridFit,
    TextRenderingHintSingleBitPerPixel,
    TextRenderingHintAntiAliasGridFit,
    TextRenderingHintAntiAlias,
    TextRenderingHintClearTypeGridFit
};

enum StringAlignment
{
    StringAlignmentNear    = 0,
    StringAlignmentCenter  = 1,
    StringAlignmentFar     = 2
};

enum  StringDigitSubstitute
{
    StringDigitSubstituteUser        = 0,
    StringDigitSubstituteNone        = 1,
    StringDigitSubstituteNational    = 2,
    StringDigitSubstituteTraditional = 3
};

enum StringFormatFlags
{
    StringFormatFlagsDirectionRightToLeft  = 0x00000001,
    StringFormatFlagsDirectionVertical     = 0x00000002,
    StringFormatFlagsNoFitBlackBox         = 0x00000004,
    StringFormatFlagsDisplayFormatControl  = 0x00000020,
    StringFormatFlagsNoFontFallback        = 0x00000400,
    StringFormatFlagsMeasureTrailingSpaces = 0x00000800,
    StringFormatFlagsNoWrap                = 0x00001000,
    StringFormatFlagsLineLimit             = 0x00002000,
    StringFormatFlagsNoClip                = 0x00004000
};

enum StringTrimming
{
    StringTrimmingNone                 = 0,
    StringTrimmingCharacter            = 1,
    StringTrimmingWord                 = 2,
    StringTrimmingEllipsisCharacter    = 3,
    StringTrimmingEllipsisWord         = 4,
    StringTrimmingEllipsisPath         = 5
};

enum FontStyle
{
    FontStyleRegular    = 0,
    FontStyleBold       = 1,
    FontStyleItalic     = 2,
    FontStyleBoldItalic = 3,
    FontStyleUnderline  = 4,
    FontStyleStrikeout  = 8
};

enum HotkeyPrefix
{
    HotkeyPrefixNone   = 0,
    HotkeyPrefixShow   = 1,
    HotkeyPrefixHide   = 2
};

enum PaletteFlags
{
    PaletteFlagsHasAlpha        = 1,
    PaletteFlagsGrayScale       = 2,
    PaletteFlagsHalftone        = 4
};

enum ImageCodecFlags
{
    ImageCodecFlagsEncoder          = 1,
    ImageCodecFlagsDecoder          = 2,
    ImageCodecFlagsSupportBitmap    = 4,
    ImageCodecFlagsSupportVector    = 8,
    ImageCodecFlagsSeekableEncode   = 16,
    ImageCodecFlagsBlockingDecode   = 32,
    ImageCodecFlagsBuiltin          = 65536,
    ImageCodecFlagsSystem           = 131072,
    ImageCodecFlagsUser             = 262144
};

enum ImageFlags
{
    ImageFlagsNone              = 0,
    ImageFlagsScalable          = 0x0001,
    ImageFlagsHasAlpha          = 0x0002,
    ImageFlagsHasTranslucent    = 0x0004,
    ImageFlagsPartiallyScalable = 0x0008,
    ImageFlagsColorSpaceRGB     = 0x0010,
    ImageFlagsColorSpaceCMYK    = 0x0020,
    ImageFlagsColorSpaceGRAY    = 0x0040,
    ImageFlagsColorSpaceYCBCR   = 0x0080,
    ImageFlagsColorSpaceYCCK    = 0x0100,
    ImageFlagsHasRealDPI        = 0x1000,
    ImageFlagsHasRealPixelSize  = 0x2000,
    ImageFlagsReadOnly          = 0x00010000,
    ImageFlagsCaching           = 0x00020000
};

enum CombineMode
{
    CombineModeReplace,
    CombineModeIntersect,
    CombineModeUnion,
    CombineModeXor,
    CombineModeExclude,
    CombineModeComplement
};

enum FlushIntention
{
    FlushIntentionFlush = 0,
    FlushIntentionSync  = 1
};

enum CoordinateSpace
{
    CoordinateSpaceWorld,
    CoordinateSpacePage,
    CoordinateSpaceDevice
};

enum GpTestControlEnum
{
    TestControlForceBilinear  = 0,
    TestControlNoICM          = 1,
    TestControlGetBuildNumber = 2
};

enum MetafileFrameUnit
{
    MetafileFrameUnitPixel      = UnitPixel,
    MetafileFrameUnitPoint      = UnitPoint,
    MetafileFrameUnitInch       = UnitInch,
    MetafileFrameUnitDocument   = UnitDocument,
    MetafileFrameUnitMillimeter = UnitMillimeter,
    MetafileFrameUnitGdi
};

enum HatchStyle
{
	HatchStyleHorizontal = 0,
	HatchStyleVertical = 1,
	HatchStyleForwardDiagonal = 2,
	HatchStyleBackwardDiagonal = 3,
	HatchStyleCross = 4,
	HatchStyleDiagonalCross = 5,
	HatchStyle05Percent = 6,
	HatchStyle10Percent = 7,
	HatchStyle20Percent = 8,
	HatchStyle25Percent = 9,
	HatchStyle30Percent = 10,
	HatchStyle40Percent = 11,
	HatchStyle50Percent = 12,
	HatchStyle60Percent = 13,
	HatchStyle70Percent = 14,
	HatchStyle75Percent = 15,
	HatchStyle80Percent = 16,
	HatchStyle90Percent = 17,
	HatchStyleLightDownwardDiagonal = 18,
	HatchStyleLightUpwardDiagonal = 19,
	HatchStyleDarkDownwardDiagonal = 20,
	HatchStyleDarkUpwardDiagonal = 21,
	HatchStyleWideDownwardDiagonal = 22,
	HatchStyleWideUpwardDiagonal = 23,
	HatchStyleLightVertical = 24,
	HatchStyleLightHorizontal = 25,
	HatchStyleNarrowVertical = 26,
	HatchStyleNarrowHorizontal = 27,
	HatchStyleDarkVertical = 28,
	HatchStyleDarkHorizontal = 29,
	HatchStyleDashedDownwardDiagonal = 30,
	HatchStyleDashedUpwardDiagonal = 31,
	HatchStyleDashedHorizontal = 32,
	HatchStyleDashedVertical = 33,
	HatchStyleSmallConfetti = 34,
	HatchStyleLargeConfetti = 35,
	HatchStyleZigZag = 36,
	HatchStyleWave = 37,
	HatchStyleDiagonalBrick = 38,
	HatchStyleHorizontalBrick = 39,
	HatchStyleWeave = 40,
	HatchStylePlaid = 41,
	HatchStyleDivot = 42,
	HatchStyleDottedGrid = 43,
	HatchStyleDottedDiamond = 44,
	HatchStyleShingle = 45,
	HatchStyleTrellis = 46,
	HatchStyleSphere = 47,
	HatchStyleSmallGrid = 48,
	HatchStyleSmallCheckerBoard = 49,
	HatchStyleLargeCheckerBoard = 50,
	HatchStyleOutlinedDiamond = 51,
	HatchStyleSolidDiamond = 52,
	HatchStyleTotal = 53,
	HatchStyleLargeGrid = HatchStyleCross,
	HatchStyleMin = HatchStyleHorizontal,
	HatchStyleMax = HatchStyleTotal - 1
};

#ifndef __cplusplus

typedef enum Unit Unit;
typedef enum BrushType BrushType;
typedef enum FillMode FillMode;
typedef enum LineCap LineCap;
typedef enum PathPointType PathPointType;
typedef enum LineJoin LineJoin;
typedef enum QualityMode QualityMode;
typedef enum SmoothingMode SmoothingMode;
typedef enum CompositingQuality CompositingQuality;
typedef enum InterpolationMode InterpolationMode;
typedef enum PixelOffsetMode PixelOffsetMode;
typedef enum DashCap DashCap;
typedef enum DashStyle DashStyle;
typedef enum MatrixOrder MatrixOrder;
typedef enum ImageType ImageType;
typedef enum ImageFlags ImageFlags;
typedef enum WarpMode WarpMode;
typedef enum WrapMode WrapMode;
typedef enum MetafileType MetafileType;
typedef enum LinearGradientMode LinearGradientMode;
typedef enum EmfType EmfType;
typedef enum CompositingMode CompositingMode;
typedef enum TextRenderingHint TextRenderingHint;
typedef enum StringAlignment StringAlignment;
typedef enum StringDigitSubstitute StringDigitSubstitute;
typedef enum StringTrimming StringTrimming;
typedef enum FontStyle FontStyle;
typedef enum StringFormatFlags StringFormatFlags;
typedef enum HotkeyPrefix HotkeyPrefix;
typedef enum PenAlignment GpPenAlignment;
typedef enum PaletteFlags PaletteFlags;
typedef enum ImageCodecFlags ImageCodecFlags;
typedef enum CombineMode CombineMode;
typedef enum FlushIntention FlushIntention;
typedef enum CoordinateSpace CoordinateSpace;
typedef enum GpTestControlEnum GpTestControlEnum;
typedef enum MetafileFrameUnit MetafileFrameUnit;
typedef enum PenType PenType;
typedef enum HatchStyle HatchStyle;

#endif /* end of c typedefs */

#endif
