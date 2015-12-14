/* So this header doesn't exist in windows anywhere. So its going here.
 * Everything here is also undocumented by MS.
 * And since MS tend to slightly change structures from public standards,
 * i'm using a combination of public standard (freebsd et al) code
 * and IDA to figure out these definitions.
 */

#ifndef __WINE_SHA_H__
#define __WINE_SHA_H__

#include "winbase.h"

typedef struct
{
    /* no idea what this is.
     * at first i thought it was the data below, but modified.. but
     * it doesn't look like it is.
     * actually, on second thoughts its probably the output from
     * A_SHAFinal (so why have two arguments to SHAFinal??
     * When i made windows hash "HashThis" it used data (just
     * put 'HashThis' in there with some extra stuff, 
     * len and nLo was 0. nHi was 8.
     */
    unsigned char padding[20];
    int len;
    UINT32 h[5];
    UINT32 nLo, nHi;
    unsigned char data[64];
} SHA_CTX;

void WINAPI A_SHAInit(SHA_CTX *);
void WINAPI A_SHAUpdate(SHA_CTX *, const unsigned char *, unsigned int);
void WINAPI A_SHAFinal(SHA_CTX *, unsigned char *);

#endif /* __WINE_SHA_H__ */

