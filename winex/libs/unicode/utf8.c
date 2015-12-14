/*
 * UTF-8 support routines
 *
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <string.h>

#include "wine/unicode.h"

/* number of following bytes in sequence based on first byte value (for bytes above 0x7f) */
static const char utf8_length[128] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x80-0x8f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x90-0x9f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xa0-0xaf */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xb0-0xbf */
    0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xc0-0xcf */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xd0-0xdf */
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* 0xe0-0xef */
    3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0  /* 0xf0-0xff */
};

/* first byte mask depending on UTF-8 sequence length */
static const unsigned char utf8_mask[4] = { 0x7f, 0x1f, 0x0f, 0x07 };

/* minimum Unicode value depending on UTF-8 sequence length */
static const unsigned int utf8_minval[4] = { 0x0, 0x80, 0x800, 0x10000 };


/* get the next char value taking surrogates into account */
static inline unsigned int get_surrogate_value( const WCHAR *src, unsigned int srclen )
{
    if (src[0] >= 0xd800 && src[0] <= 0xdfff)  /* surrogate pair */
    {
        if (src[0] > 0xdbff || /* invalid high surrogate */
            srclen <= 1 ||     /* missing low surrogate */
            src[1] < 0xdc00 || src[1] > 0xdfff) /* invalid low surrogate */
            return 0;
        return 0x10000 + ((src[0] & 0x3ff) << 10) + (src[1] & 0x3ff);
    }
    return src[0];
}

/* query necessary dst length for src string */
static inline int get_length_wcs_utf8( int flags, const WCHAR *src, unsigned int srclen )
{
    int len;
    unsigned int val;

    for (len = 0; srclen; srclen--, src++)
    {
        if (*src < 0x80)  /* 0x00-0x7f: 1 byte */
        {
            len++;
            continue;
        }
        if (*src < 0x800)  /* 0x80-0x7ff: 2 bytes */
        {
            len += 2;
            continue;
        }
        if (!(val = get_surrogate_value( src, srclen )))
        {
            if (flags & WC_ERR_INVALID_CHARS) return -2;
            continue;
        }
        if (val < 0x10000)  /* 0x800-0xffff: 3 bytes */
            len += 3;
        else   /* 0x10000-0x10ffff: 4 bytes */
            len += 4;
    }
    return len;
}

/* wide char to UTF-8 string conversion */
/* return -1 on dst buffer overflow, -2 on invalid input char */
int wine_utf8_wcstombs( int flags, const WCHAR *src, int srclen, char *dst, int dstlen )
{
    int len;

    if (!dstlen) return get_length_wcs_utf8( flags, src, srclen );

    for (len = dstlen; srclen; srclen--, src++)
    {
        WCHAR ch = *src;
        unsigned int val;

        if (ch < 0x80)  /* 0x00-0x7f: 1 byte */
        {
            if (!len--) return -1;  /* overflow */
            *dst++ = ch;
            continue;
        }

        if (ch < 0x800)  /* 0x80-0x7ff: 2 bytes */
        {
            if ((len -= 2) < 0) return -1;  /* overflow */
            dst[1] = 0x80 | (ch & 0x3f);
            ch >>= 6;
            dst[0] = 0xc0 | ch;
            dst += 2;
            continue;
        }

        if (!(val = get_surrogate_value( src, srclen )))
        {
            if (flags & WC_ERR_INVALID_CHARS) return -2;
            continue;
        }

        if (val < 0x10000)  /* 0x800-0xffff: 3 bytes */
        {
            if ((len -= 3) < 0) return -1;  /* overflow */
            dst[2] = 0x80 | (val & 0x3f);
            val >>= 6;
            dst[1] = 0x80 | (val & 0x3f);
            val >>= 6;
            dst[0] = 0xe0 | val;
            dst += 3;
        }
        else   /* 0x10000-0x10ffff: 4 bytes */
        {
            if ((len -= 4) < 0) return -1;  /* overflow */
            dst[3] = 0x80 | (val & 0x3f);
            val >>= 6;
            dst[2] = 0x80 | (val & 0x3f);
            val >>= 6;
            dst[1] = 0x80 | (val & 0x3f);
            val >>= 6;
            dst[0] = 0xf0 | val;
            dst += 4;
        }
    }
    return dstlen - len;
}

/* query necessary dst length for src string */
static inline int get_length_mbs_utf8( int flags, const char *src, int srclen )
{
    int len, ret = 0;
    unsigned int res;
    const char *srcend = src + srclen;

    while (src < srcend)
    {
        unsigned char ch = *src++;
        if (ch < 0x80)  /* special fast case for 7-bit ASCII */
        {
            ret++;
            continue;
        }
        len = utf8_length[ch-0x80];
        if (src + len > srcend) goto bad;
        res = ch & utf8_mask[len];

        switch(len)
        {
        case 3:
            if ((ch = *src ^ 0x80) >= 0x40) goto bad;
            res = (res << 6) | ch;
            src++;
        case 2:
            if ((ch = *src ^ 0x80) >= 0x40) goto bad;
            res = (res << 6) | ch;
            src++;
        case 1:
            if ((ch = *src ^ 0x80) >= 0x40) goto bad;
            res = (res << 6) | ch;
            src++;
            if (res < utf8_minval[len]) goto bad;
            if (res > 0x10ffff) goto bad;
            if (res > 0xffff) ret++;
            ret++;
            continue;
        }
    bad:
        if (flags & MB_ERR_INVALID_CHARS) return -2;  /* bad char */
        /* otherwise ignore it */
    }
    return ret;
}

/* UTF-8 to wide char string conversion */
/* return -1 on dst buffer overflow, -2 on invalid input char */
int wine_utf8_mbstowcs( int flags, const char *src, int srclen, WCHAR *dst, int dstlen )
{
    int len;
    unsigned int res;
    const char *srcend = src + srclen;
    WCHAR *dstend = dst + dstlen;

    if (!dstlen) return get_length_mbs_utf8( flags, src, srclen );

    while ((dst < dstend) && (src < srcend))
    {
        unsigned char ch = *src++;
        if (ch < 0x80)  /* special fast case for 7-bit ASCII */
        {
            *dst++ = ch;
            continue;
        }
        len = utf8_length[ch-0x80];
        if (src + len > srcend) goto bad;
        res = ch & utf8_mask[len];

        switch(len)
        {
        case 3:
            if ((ch = *src ^ 0x80) >= 0x40) goto bad;
            res = (res << 6) | ch;
            src++;
        case 2:
            if ((ch = *src ^ 0x80) >= 0x40) goto bad;
            res = (res << 6) | ch;
            src++;
        case 1:
            if ((ch = *src ^ 0x80) >= 0x40) goto bad;
            res = (res << 6) | ch;
            src++;
            if (res < utf8_minval[len]) goto bad;
            if (res > 0x10ffff) goto bad;
            if (res <= 0xffff) *dst++ = res;
            else /* we need surrogates */
            {
                if (dst == dstend - 1) return -1;  /* overflow */
                res -= 0x10000;
                *dst++ = 0xd800 | (res >> 10);
                *dst++ = 0xdc00 | (res & 0x3ff);
            }
            continue;
        }
    bad:
        if (flags & MB_ERR_INVALID_CHARS) return -2;  /* bad char */
        /* otherwise ignore it */
    }
    if (src < srcend) return -1;  /* overflow */
    return dstlen - (dstend - dst);
}
