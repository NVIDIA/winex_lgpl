/* d3dhalp.h
 *
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef WINE_D3DHALP_H_INCLUDED
#define WINE_D3DHALP_H_INCLUDED

#include "windef.h"
#include "ddrawi.h"
#include "d3dtypes.h"

struct _D3DHAL_CALLBACKS;
struct _D3DHAL_CALLBACKS2;
struct _VertexShader;
struct _PixelShader;

typedef struct {
    D3DLIGHT7 l;
    DWORD flags, devnum;
    D3DVALUE devpos[4];
} D3D_Light;


#define D3DHALP_TEXTURE_DATA_FIELDS 26

typedef struct
{
    DDRAWI_DDRAWSURFACE_LCL lcl;
    DDRAWI_DDRAWSURFACE_MORE more;
    LPDDRAWI_DDRAWSURFACE_GBL_MORE gmore;
    DDRAWI_DDRAWSURFACE_GBL gbl;
    DDRAWI_DDRAWSURFACE_GBL_MORE gbl_more;
    DWORD tdata[D3DHALP_TEXTURE_DATA_FIELDS];
} D3D_T,*LPD3D_T;


#define D3DHALP_TEXTURE_STAGE_STATES 34

typedef struct
{
    LPDDRAWI_DIRECTDRAW_LCL lcl;
    LPDDRAWI_DDRAWSURFACE_LCL surf, zbuf;
    ULONG_PTR dwhContext;
    LPDDHAL_GETDRIVERINFO gdi;
    struct _D3DHAL_CALLBACKS *cbs1;
    struct _D3DHAL_CALLBACKS2 *cbs2;
    struct _D3DHAL_CALLBACKS3 *cbs3;
    HANDLE surface_heap;
    BOOL in_scene;
    DWORD renderstate[256];
    DWORD texstagestate[16][D3DHALP_TEXTURE_STAGE_STATES];
    D3DMATRIX xfrm[8], txfrm[8];
    D3DVIEWPORT7 viewport;
    D3DMATERIAL7 material;
    DWORD numlights;
    D3D_Light *light;
    D3DVALUE clipplane[32][4];
    struct _VertexShader* vertex_shader;
    struct _PixelShader* pixel_shader;
    DWORD vertex_fvf;
    float* vertex_const;
    INT vertex_consti[4*16];
    BOOL vertex_constb[16];
    float* pixel_const;
    INT pixel_consti[4*16];
    BOOL pixel_constb[16];
    struct _PixelShader* pixel_shader_cache;
    /* PLEASE do not add new fields to D3D_P if you can avoid it.
     * I've been working on phasing it out for a while. */
} D3D_P,*LPD3D_P;

/*----------------------------------------------------------------------------------
 * Private VertexShader structure and macros that are shared between D3D8 and D3DGL
 * - can be modified as necessary
 *----------------------------------------------------------------------------------*/

/* to distinguish a handle from a FVF, we'll use D3DFVF_RESERVED0,
 * assuming that HeapAlloc-ed memory is always DWORD-aligned so
 * that the lower bit of the returned pointer is always zero */
#define VS_TO_HANDLE(x) (((DWORD)x) | D3DFVF_RESERVED0)
#define VS_FROM_HANDLE(x) ((VertexShader*)(x & ~D3DFVF_RESERVED0))
#define IS_VS(x) (x & D3DFVF_RESERVED0)
#define IS_FVF(x) (!IS_VS(x))

#define SET_INPUT_REG_TYPE(x) ((x) | (1<<7))
#define GET_INPUT_REG_TYPE(x) ((x) & ~(1<<7))
#define IS_INPUT_REG_ENABLED(x) ((x) & (1<<7))

typedef struct {
        DWORD index;
  const VOID  *ptr;
} ShaderDefEntry;

typedef struct {
  ShaderDefEntry  *def_const;    /* constants defined by def  */
  ShaderDefEntry  *defi_const;   /* constants defined by defi */
  ShaderDefEntry  *defb_const;   /* constants defined by defb */
  DWORD *set_def, *set_defi, *set_defb;
} ShaderDefBlock;

typedef struct _VertexShaderCacheState {
  DWORD clipspace_fix; 
  DWORD pos_offset;
  DWORD ypos_inverted;
  DWORD colorfix;               /* bitfield for color swizzle enable */
  DWORD tex_scale;              /* bitfield for tex coord scaling enable */
  DWORD tex_offset;             /* bitfield for tex coord offsets enable */
  BOOL  is_w_required;          /* ensure the w component is written for texture coordinates */
#ifdef __APPLE__
  DWORD ati_fog_in_tc_hack;
#endif
} VertexShaderCacheState;

typedef struct _VertexShaderCache {
  VertexShaderCacheState state;
  char *arb_program;
  DWORD DriverReserved0;      /* used by d3dgl to store driver specific info (name) */
  DWORD DriverReserved1;      /* (context) */
  DWORD pos_offset_size[2];   /* cached viewport size for pos fragment offset */
  DWORD tex_offset_size[8][2];/* cached texture sizes for tex fragment offset */
  DWORD tex_scale_size[8][2]; /* cached texture sizes for fragment_offset */
  BYTE dcl_component_masks[16];/* cached component masks for declared inputs and outputs */
  DWORD stolen_constants;     /* stolen constants */
  void *ExtensionSpecific;
  struct _VertexShaderCache *next;
} VertexShaderCache;

typedef struct _VertexDeclaration {
  LPVOID pDeclaration;  	/* app given declaration, dependant on dx version */
  DWORD DeclarationLength;	/* length of declaration in bytes */
  DWORD FVF;
  BOOL is_fvf_compatible;	/* are all of the elements properly represented in FVF? */
  BOOL is_direct;
} VertexDeclaration;

typedef struct _VertexShader {
  VertexDeclaration vdecl;   /* dx8 vertex decl */
  CONST DWORD* pFunction;
  DWORD FunctionLength;
  DWORD Usage;
  void *orig_shaderprogram;  /* original (ie app specified) shader program */
  void *shaderprogram;       /* parsed structure */
  struct _VertexShader *prev;
  struct _VertexShader *next;
  VertexShaderCache *cache;
  VertexShaderCache *current; 	
  DWORD colorfix;             /* bit field indicating which input_regs had the colorfix applied */
  BYTE  has_fog;
  BYTE  has_pointsize;
  BYTE  is_software;
  BYTE  fixed_mode;
  struct {
    BYTE stream;   /* stream source number */
    BYTE offset;   /* offset into stream in BYTES */
    BYTE type;     /* D3DVSDT_* type of data, with high bit used a flag (see macros above) */
    int gltype;    /* gl type that the D3DVSDT_* type maps to */
    int size;	   /* the number of data elements in field of type */
    int normalize; /* should fixed point data be normalized when converting to floating point? */
  } input_reg[17];
  ShaderDefBlock defs;
  void *vtbl;  /* extension-specific vtable of function pointers */
} VertexShader;

typedef struct _PixelShaderCacheState {
  DWORD texture_type[16];
  DWORD texture_projected[8];
  DWORD fog_type;
  BYTE dcl_component_masks[16]; /* cached component masks for declared inputs and outputs */
  BOOL has_dcl_component_masks;
  BOOL  srgb_write;
#ifdef __APPLE__
  DWORD ati_fog_in_tc_hack;
#endif
} PixelShaderCacheState;

typedef struct _PixelShaderCache {
  char *programstring;
  struct _PixelShaderCacheState state;
  DWORD DriverReserved0;      /* used by d3dgl to store driver specific info (name) */
  DWORD DriverReserved1;      /* (context) */
  void *ExtensionSpecific;
  struct _PixelShaderCache *next;
} PixelShaderCache;

typedef struct _PixelShader {
  CONST DWORD* pFunction;
  DWORD FunctionLength;
  CHAR  *programstring;
  void  *orig_shaderprogram;
  void  *shaderprogram;
  VOID  *ExtensionSpecific;
  ShaderDefBlock defs;
  DWORD used_texture[1];     /* bitfield: textures used */
  struct _PixelShader *prev;
  struct _PixelShader *next;
  struct _PixelShaderCache *cache;
  struct _PixelShaderCache *current;
  void *vtbl;  /* extension-specific vtable of function pointers */
} PixelShader;

typedef struct _Query {
  DWORD type;
  DWORD valid;
  DWORD DriverReserved0;      /* used by d3dgl to store driver specific info (id) */
  DWORD DriverReserved1;      /* (context) */
} Query;
  

typedef struct GL_shader_caps
{
  /* both */
  int glsl_max_varying;
  int max_texture_coords;
  /* vertex */
  BOOL arb_vp_hardware_processing;
  int arb_vp_max_vertex_addressregs;
  int arb_vp_max_vertex_attributes;
  int arb_vp_max_vertex_constants;
  int arb_vp_max_vertex_instructions;
  int arb_vp_max_vertex_temporaries;
  int arb_vs_max_vertex_attributes;
  int arb_vs_max_vertex_constants;
  int max_vertex_addressregs;
  int max_vertex_attributes;
  int max_vertex_constants;
  int max_vertex_instructions;
  int max_vertex_temporaries;
  /* pixel */
  int arb_fp_max_pixel_attributes;
  int arb_fp_max_pixel_constants;
  int arb_fp_max_pixel_instructions;
  int arb_fp_max_pixel_alu_instructions;
  int arb_fp_max_pixel_tex_instructions;
  int arb_fp_max_pixel_tex_indirections;
  int arb_fp_max_pixel_temporaries;
  int arb_fs_max_pixel_constants;
  int max_pixel_instructions;
  int max_pixel_samplers;  /* loaded in arb_fp setup, but valid for both */
  int max_pixel_constants;
} GL_shader_caps;

#endif
