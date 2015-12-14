/*
 * Multibyte string definitions
 *
 * Copyright 2001 Francois Gouget.
 */
#ifndef __WINE_MBSTRING_H
#define __WINE_MBSTRING_H

#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif

#include "msvcrt/mbctype.h"

#ifdef USE_MSVCRT_PREFIX
#define MSVCRT(x)    MSVCRT_##x
#else
#define MSVCRT(x)    x
#endif

#ifndef MSVCRT_SIZE_T_DEFINED
typedef unsigned int MSVCRT(size_t);
#define MSVCRT_SIZE_T_DEFINED
#endif

#define _NLSCMPERROR               ((unsigned int)0x7fffffff)


#ifdef __cplusplus
extern "C" {
#endif

int         _ismbcalnum(unsigned int);
int         _ismbcalpha(unsigned int);
int         _ismbcdigit(unsigned int);
int         _ismbcgraph(unsigned int);
int         _ismbchira(unsigned int);
int         _ismbckata(unsigned int);
int         _ismbcl0(unsigned int);
int         _ismbcl1(unsigned int);
int         _ismbcl2(unsigned int);
int         _ismbclegal(unsigned int);
int         _ismbclower(unsigned int);
int         _ismbcprint(unsigned int);
int         _ismbcpunct(unsigned int);
int         _ismbcspace(unsigned int);
int         _ismbcsymbol(unsigned int);
int         _ismbcupper(unsigned int);
unsigned int _mbbtombc(unsigned int);
int         _mbbtype(unsigned char,int);
#define     _mbccmp(_cpc1,_cpc2) _mbsncmp((_cpc1),(_cpc2),1)
void        _mbccpy(unsigned char*,const unsigned char*);
unsigned int _mbcjistojms(unsigned int);
unsigned int _mbcjmstojis(unsigned int);
MSVCRT(size_t) _mbclen(const unsigned char*);
unsigned int _mbctohira(unsigned int);
unsigned int _mbctokata(unsigned int);
unsigned int _mbctolower(unsigned int);
unsigned int _mbctombb(unsigned int);
unsigned int _mbctoupper(unsigned int);
int         _mbsbtype(const unsigned char*,MSVCRT(size_t));
unsigned char* _mbscat(unsigned char*,const unsigned char*);
unsigned char* _mbschr(const unsigned char*,unsigned int);
int         _mbscmp(const unsigned char*,const unsigned char*);
int         _mbscoll(const unsigned char*,const unsigned char*);
unsigned char* _mbscpy(unsigned char*,const unsigned char*);
MSVCRT(size_t) _mbscspn(const unsigned char*,const unsigned char*);
unsigned char* _mbsdec(const unsigned char*,const unsigned char*);
unsigned char* _mbsdup(const unsigned char*);
int         _mbsicmp(const unsigned char*,const unsigned char*);
int         _mbsicoll(const unsigned char*,const unsigned char*);
unsigned char* _mbsinc(const unsigned char*);
MSVCRT(size_t) _mbslen(const unsigned char*);
unsigned char* _mbslwr(unsigned char*);
unsigned char* _mbsnbcat(unsigned char*,const unsigned char*,MSVCRT(size_t));
int         _mbsnbcmp(const unsigned char*,const unsigned char*,MSVCRT(size_t));
int         _mbsnbcoll(const unsigned char*,const unsigned char*,MSVCRT(size_t));
MSVCRT(size_t) _mbsnbcnt(const unsigned char*,MSVCRT(size_t));
unsigned char* _mbsnbcpy(unsigned char*,const unsigned char*
,MSVCRT(size_t));
int         _mbsnbicmp(const unsigned char*,const unsigned char*,MSVCRT(size_t));
int         _mbsnbicoll(const unsigned char*,const unsigned char*,MSVCRT(size_t));
unsigned char* _mbsnbset(unsigned char*,unsigned int,MSVCRT(size_t))
;
unsigned char* _mbsncat(unsigned char*,const unsigned char*,
 MSVCRT(size_t));
MSVCRT(size_t) _mbsnccnt(const unsigned char*,MSVCRT(size_t));
int         _mbsncmp(const unsigned char*,const unsigned char*,MSVCRT(size_t));
int         _mbsncoll(const unsigned char*,const unsigned char*,MSVCRT(size_t));
unsigned char* _mbsncpy(unsigned char*,const unsigned char*,MSVCRT(size_t));
unsigned int _mbsnextc (const unsigned char*);
int         _mbsnicmp(const unsigned char*,const unsigned char*,MSVCRT(size_t));
int         _mbsnicoll(const unsigned char*,const unsigned char*,MSVCRT(size_t));
unsigned char* _mbsninc(const unsigned char*,MSVCRT(size_t));
unsigned char* _mbsnset(unsigned char*,unsigned int,MSVCRT(size_t));
unsigned char* _mbspbrk(const unsigned char*,const unsigned char*);
unsigned char* _mbsrchr(const unsigned char*,unsigned int);
unsigned char* _mbsrev(unsigned char*);
unsigned char* _mbsset(unsigned char*,unsigned int);
MSVCRT(size_t) _mbsspn(const unsigned char*,const unsigned char*);
unsigned char* _mbsspnp(const unsigned char*,const unsigned char*);
unsigned char* _mbsstr(const unsigned char*,const unsigned char*);
unsigned char* _mbstok(unsigned char*,const unsigned char*);
unsigned char* _mbsupr(unsigned char*);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_MBSTRING_H */
