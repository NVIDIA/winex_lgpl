/* md5.h
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2015 NVIDIA CORPORATION. All rights reserved.
 *
 * Apparently there are some structures / functions that are "defined in md5.h"
 * (according to MSDN) - however that file doesn't seem to exist, either on
 * my windows box or on google. So I'm guessing its private.
 */

/* since this is kinda undocumented stuff, and what docu is available looks pretty
 * dodgy and conflics with what google offers, i've commented it pretty heavily.
 * Enjoy!
 */

#ifndef __WINE_MD5_H__
#define __WINE_MD5_H__

#include "winbase.h"

#define PROTO_LIST(list) list

/* i've also seen this as:
 * typedef struct {
 *     UINT4 state[4]; // or buf
 *     UINT4 count[2]; // or bits
 *     unsigned char buffer[64];
 * } MD5_CTX;
 * notice the reversing on the [4] array with the [2] array.
 * oh and I think digest[16] is used as win32 as a [out] value from
 * MD5Final, whereas others take another pointer (to a uchar *) in
 * that function
 */
typedef struct {
    ULONG i[2];
    ULONG buf[4];
    unsigned char in[64];
    unsigned char digest[16];
} MD5_CTX;

/* there appears to be different versions of these floating about on the net,
 * with same stuff but different parameters - using the ones on MSDN.
 * they could be wrong, though
 */
void WINAPI MD5Init PROTO_LIST ((MD5_CTX *));
void WINAPI MD5Update PROTO_LIST ((MD5_CTX *, const unsigned char *, unsigned int));
void WINAPI MD5Final PROTO_LIST ((MD5_CTX *));

#endif /* __WINE_MD5_H__ */
