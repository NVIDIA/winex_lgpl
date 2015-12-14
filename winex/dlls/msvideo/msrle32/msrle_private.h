/*
 * Copyright 2002 Michael Günnewig
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2004-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MSRLE32_PRIVATE_H
#define __MSRLE32_PRIVATE_H

#include "winbase.h"
#include "winuser.h"
#include "mmsystem.h"
#include "vfw.h"

#define IDS_NAME        100
#define IDS_DESCRIPTION 101
#define IDS_ABOUT       102

#define MSRLE32_VERSION  0x00010000 /* Version 1.0 build 0 */
#define MSRLE32_DEFAULTQUALITY (75 * ICQUALITY_HIGH) / 100

#define FOURCC_RLE   mmioFOURCC('R','L','E',' ')
#define FOURCC_RLE4  mmioFOURCC('R','L','E','4')
#define FOURCC_RLE8  mmioFOURCC('R','L','E','8')
#define FOURCC_MRLE  mmioFOURCC('M','R','L','E')

#define WIDTHBYTES(i)     ((WORD)((i+31u)&(~31u))/8u) /* ULONG aligned ! */
#define DIBWIDTHBYTES(bi) WIDTHBYTES((WORD)(bi).biWidth * (WORD)(bi).biBitCount)

typedef struct _CodecInfo {
  FOURCC  fccHandler;
  DWORD   dwQuality;

  BOOL    bCompress;
  LONG    nPrevFrame;
  LPWORD  pPrevFrame;
  LPWORD  pCurFrame;

  BOOL    bDecompress;
  LPBYTE  palette_map;
} CodecInfo;

typedef const BITMAPINFOHEADER * LPCBITMAPINFOHEADER;

#endif
