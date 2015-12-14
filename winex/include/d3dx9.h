#ifndef __WINEX_D3DX9_H
#define __WINEX_D3DX9_H

#include <limits.h>

#include "d3d9.h"
#include "d3dx9math.h"
#include "d3dx9core.h"
#include "d3dx9xof.h"
#include "d3dx9mesh.h"
#include "d3dx9tex.h"
#include "d3dx9shader.h"
#include "d3dx9effect.h"
#include "d3dx9shape.h"
#include "d3dx9anim.h"

#define D3DX_DEFAULT            ((UINT) -1)
#define D3DX_DEFAULT_NONPOW2    ((UINT) -2)
#define D3DX_DEFAULT_FLOAT      FLT_MAX
#define D3DX_FROM_FILE          ((UINT) -3)
#define D3DFMT_FROM_FILE        ((D3DFORMAT) -3)


#ifdef __cplusplus
#define D3DXINLINE inline
#else
#define D3DXINLINE
#endif

#define _FACDD  0x876
#define MAKE_DDHRESULT(code)    MAKE_HRESULT(1, _FACDD, code)

#define D3DXERR_CANNOTMODIFYINDEXBUFFER     MAKE_DDHRESULT(2900)
#define D3DXERR_INVALIDMESH                 MAKE_DDHRESULT(2901)
#define D3DXERR_CANNOTATTRSORT              MAKE_DDHRESULT(2902)
#define D3DXERR_SKINNINGNOTSUPPORTED        MAKE_DDHRESULT(2903)
#define D3DXERR_TOOMANYINFLUENCES           MAKE_DDHRESULT(2904)
#define D3DXERR_INVALIDDATA                 MAKE_DDHRESULT(2905)
#define D3DXERR_LOADEDMESHASNODATA          MAKE_DDHRESULT(2906)
#define D3DXERR_DUPLICATENAMEDFRAGMENT      MAKE_DDHRESULT(2907)

#undef MAKE_DDHRESULT

#endif /* __WINEX_D3DX9_H */

