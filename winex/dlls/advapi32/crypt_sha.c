/* Copyright 2003 TransGaming Technologies
 * David Hammerton <david@transgaming.com>
 *   Parts taken from Putty (MIT license)
 *     Copyright 1997-2003 Simon Tatham.
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

#include "winbase.h"
#include "windef.h"
#include "winnls.h"
#include "winerror.h"

#include "wine/debug.h"

#include "sha.h"

WINE_DEFAULT_DEBUG_CHANNEL(advapi);


/******************************************************************************
 * A_SHAInit [ADVAPI32.@]
 *
 */
void WINAPI A_SHAInit(SHA_CTX *ctx)
{
    TRACE("(%p)\n", ctx);
    ctx->nLo = ctx->nHi = 0;
    ctx->len = 0;
    ctx->h[0] = 0x67452301;
    ctx->h[1] = 0xefcdab89;
    ctx->h[2] = 0x98badcfe;
    ctx->h[3] = 0x10325476;
    ctx->h[4] = 0xc3d2e1f0;
}

#define rol(x,y) ( ((x) << (y)) | (((UINT32)x) >> (32-y)) )
void SHATransform(UINT32 * digest, UINT32 * block)
{
    UINT32 w[80];
    UINT32 a, b, c, d, e;
    int t;

    for (t = 0; t < 16; t++)
        w[t] = block[t];

    for (t = 16; t < 80; t++) {
        UINT32 tmp = w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16];
        w[t] = rol(tmp, 1);
    }

    a = digest[0];
    b = digest[1];
    c = digest[2];
    d = digest[3];
    e = digest[4];

    if (TRACE_ON(advapi)) {
        TRACE("(before) digest is:\n");
        DPRINTF("\t%08x\n", a);
        DPRINTF("\t%08x\n", b);
        DPRINTF("\t%08x\n", c);
        DPRINTF("\t%08x\n", d);
        DPRINTF("\t%08x\n", e);
    }

    for (t = 0; t < 20; t++) {
        UINT32 tmp =
            rol(a, 5) + ((b & c) | (d & ~b)) + e + w[t] + 0x5a827999;
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = tmp;
    }
    for (t = 20; t < 40; t++) {
        UINT32 tmp = rol(a, 5) + (b ^ c ^ d) + e + w[t] + 0x6ed9eba1;
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = tmp;
    }
    for (t = 40; t < 60; t++) {
        UINT32 tmp = rol(a,
                         5) + ((b & c) | (b & d) | (c & d)) + e + w[t] +
            0x8f1bbcdc;
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = tmp;
    }
    for (t = 60; t < 80; t++) {
        UINT32 tmp = rol(a, 5) + (b ^ c ^ d) + e + w[t] + 0xca62c1d6;
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = tmp;
    }

    digest[0] += a;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;

    if (TRACE_ON(advapi)) {
        TRACE("(after) digest is:\n");
        DPRINTF("\t%08x\n", a);
        DPRINTF("\t%08x\n", b);
        DPRINTF("\t%08x\n", c);
        DPRINTF("\t%08x\n", d);
        DPRINTF("\t%08x\n", e);
    }
}


/******************************************************************************
 * A_SHAUpdate [ADVAPI32.@]
 *
 */
void WINAPI A_SHAUpdate(SHA_CTX *ctx, const unsigned char *buf, unsigned int len)
{
    unsigned char *q = (unsigned char *) buf;
    UINT32 wordblock[16];
    UINT32 lenw = len;
    int i;

    TRACE("(%p, %p (%s), %u)\n", ctx, buf, debugstr_an((LPCSTR)buf, len), len);
    /*
     * Update the length field.
     */
    ctx->nLo += lenw;
    ctx->nHi += (ctx->nLo < lenw);

    if (ctx->len && ctx->len + len < 64) {
        /*
         * Trivial case: just add to the block.
         */
        memcpy(ctx->data + ctx->len, q, len);
        ctx->len += len;
    } else {
        /*
         * We must complete and process at least one block.
         */
        while (ctx->len + len >= 64) {
            memcpy(ctx->data + ctx->len, q, 64 - ctx->len);
            q += 64 - ctx->len;
            len -= 64 - ctx->len;
            /* Now process the block. Gather bytes big-endian into words */
            for (i = 0; i < 16; i++) {
                wordblock[i] =
                    (((UINT32) ctx->data[i * 4 + 0]) << 24) |
                    (((UINT32) ctx->data[i * 4 + 1]) << 16) |
                    (((UINT32) ctx->data[i * 4 + 2]) << 8) |
                    (((UINT32) ctx->data[i * 4 + 3]) << 0);
            }
            SHATransform(ctx->h, wordblock);
            ctx->len = 0;
        }
        memcpy(ctx->data, q, len);
        ctx->len = len;
    }
}

/******************************************************************************
 * A_SHAFinal [ADVAPI32.@]
 *
 */
void WINAPI A_SHAFinal(SHA_CTX * ctx, unsigned char *output)
{
    int i;
    int pad;
    unsigned char c[64];
    UINT32 lenhi, lenlo;

    TRACE("(%p, %p)\n", ctx, output);

    if (ctx->len >= 56)
        pad = 56 + 64 - ctx->len;
    else
        pad = 56 - ctx->len;

    lenhi = (ctx->nHi << 3) | (ctx->nLo >> (32 - 3));
    lenlo = (ctx->nLo << 3);

    memset(c, 0, pad);
    c[0] = 0x80;
    A_SHAUpdate(ctx, c, pad);

    c[0] = (lenhi >> 24) & 0xFF;
    c[1] = (lenhi >> 16) & 0xFF;
    c[2] = (lenhi >> 8) & 0xFF;
    c[3] = (lenhi >> 0) & 0xFF;
    c[4] = (lenlo >> 24) & 0xFF;
    c[5] = (lenlo >> 16) & 0xFF;
    c[6] = (lenlo >> 8) & 0xFF;
    c[7] = (lenlo >> 0) & 0xFF;

    A_SHAUpdate(ctx, c, 8);

    for (i = 0; i < 5; i++) {
        output[i * 4] = (ctx->h[i] >> 24) & 0xFF;
        output[i * 4 + 1] = (ctx->h[i] >> 16) & 0xFF;
        output[i * 4 + 2] = (ctx->h[i] >> 8) & 0xFF;
        output[i * 4 + 3] = (ctx->h[i]) & 0xFF;
    }
    if (TRACE_ON(advapi)) {
        for (i = 0; i < 20; i++)
        {
            DPRINTF("%02x ", output[i]);
        }
        DPRINTF("\n\n");
    }
}

