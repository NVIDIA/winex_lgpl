#ifndef  __WINEX_D3DX8_H__
# define __WINEX_D3DX8_H__

# include "d3dx8.h"


/************************************************************
 * Defines
 */

#define D3DX_FILTER_NONE            (1 << 0)
#define D3DX_FILTER_POINT           (2 << 0)
#define D3DX_FILTER_LINEAR          (3 << 0)
#define D3DX_FILTER_TRIANGLE        (4 << 0)
#define D3DX_FILTER_BOX             (5 << 0)

#define D3DX_FILTER_MIRROR_U        (1 << 16)
#define D3DX_FILTER_MIRROR_V        (2 << 16)
#define D3DX_FILTER_MIRROR_W        (4 << 16)
#define D3DX_FILTER_MIRROR          (7 << 16)
#define D3DX_FILTER_DITHER          (1 << 19)
#define D3DX_FILTER_SRGB_IN         (1 << 20)
#define D3DX_FILTER_SRGB_OUT        (2 << 20)
#define D3DX_FILTER_SRGB            (3 << 20)

#define D3DX_NORMALMAP_MIRROR_U     (1 << 16)
#define D3DX_NORMALMAP_MIRROR_V     (2 << 16)
#define D3DX_NORMALMAP_MIRROR       (3 << 16)
#define D3DX_NORMALMAP_INVERTSIGN   (8 << 16)
#define D3DX_NORMALMAP_COMPUTE_OCCLUSION (16 << 16)

#define D3DX_CHANNEL_RED            (1 << 0)
#define D3DX_CHANNEL_BLUE           (1 << 1)
#define D3DX_CHANNEL_GREEN          (1 << 2)
#define D3DX_CHANNEL_ALPHA          (1 << 3)
#define D3DX_CHANNEL_LUMINANCE      (1 << 4)


/* macro to create the bit flags for the MipFilter parameter to D3DXCreateTexture*() functions */
/* NOTE: according to MSDN, the mipSkip count is supposed to be in bits 27-31 of the MipFilter
         parameter to D3DXCreate*TextureFromFileInMemoryEx(), however, the headers for d3dx9
         from the October 2006 DXSDK use bits 26-30 instead. */
#define D3DX_SKIP_DDS_MIP_LEVELS_MASK   0x1F
#define D3DX_SKIP_DDS_MIP_LEVELS_SHIFT  26
#define D3DX_SKIP_DDS_MIP_LEVELS(levels, filter) ((((levels) & D3DX_SKIP_DDS_MIP_LEVELS_MASK) << D3DX_SKIP_DDS_MIP_LEVELS_SHIFT) | ((filter) == D3DX_DEFAULT ? D3DX_FILTER_BOX : (filter)))



/*********************************************************
 * Types, structures, and enums
 */

typedef enum _D3DXIMAGE_FILEFORMAT
{
    D3DXIFF_BMP         = 0,
    D3DXIFF_JPG         = 1,
    D3DXIFF_TGA         = 2,
    D3DXIFF_PNG         = 3,
    D3DXIFF_DDS         = 4,
    D3DXIFF_PPM         = 5,
    D3DXIFF_DIB         = 6,
    D3DXIFF_HDR         = 7,
    D3DXIFF_PFM         = 8,
    D3DXIFF_FORCE_DWORD = 0x7fffffff

} D3DXIMAGE_FILEFORMAT;

typedef struct _D3DXIMAGE_INFO
{
    UINT                    Width;
    UINT                    Height;
    UINT                    Depth;
    UINT                    MipLevels;
    D3DFORMAT               Format;
    D3DRESOURCETYPE         ResourceType;
    D3DXIMAGE_FILEFORMAT    ImageFileFormat;

} D3DXIMAGE_INFO;



HRESULT WINAPI D3DXCreateTextureFromFileExA( LPDIRECT3DDEVICE8         pDevice,
                                             LPCSTR                    pSrcFile,
                                             UINT                      Width,
                                             UINT                      Height,
                                             UINT                      MipLevels,
                                             DWORD                     Usage,
                                             D3DFORMAT                 Format,
                                             D3DPOOL                   Pool,
                                             DWORD                     Filter,
                                             DWORD                     MipFilter,
                                             D3DCOLOR                  ColorKey,
                                             D3DXIMAGE_INFO*           pSrcInfo,
                                             PALETTEENTRY*             pPalette,
                                             LPDIRECT3DTEXTURE8*       ppTexture);


HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx( LPDIRECT3DDEVICE8   pDevice,
                                                    LPCVOID             pSrcData,
                                                    UINT                SrcDataSize,
                                                    UINT                Width,
                                                    UINT                Height,
                                                    UINT                MipLevels,
                                                    DWORD               Usage,
                                                    D3DFORMAT           Format,
                                                    D3DPOOL             Pool,
                                                    DWORD               Filter,
                                                    DWORD               MipFilter,
                                                    D3DCOLOR            ColorKey,
                                                    D3DXIMAGE_INFO *    pSrcInfo,
                                                    PALETTEENTRY *      pPalette,
                                                    LPDIRECT3DTEXTURE8 *ppTexture);
#endif
