/*
 * Multibyte char definitions
 *
 * Copyright 2001 Francois Gouget.
 */
#ifndef __WINE_MBCTYPE_H
#define __WINE_MBCTYPE_H

#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif


#ifdef __cplusplus
extern "C" {
#endif

unsigned char* __p__mbctype(void);
#define _mbctype                   (__p__mbctype())

int         _getmbcp(void);
int         _ismbbalnum(unsigned int);
int         _ismbbalpha(unsigned int);
int         _ismbbgraph(unsigned int);
int         _ismbbkalnum(unsigned int);
int         _ismbbkana(unsigned int);
int         _ismbbkprint(unsigned int);
int         _ismbbkpunct(unsigned int);
int         _ismbblead(unsigned int);
int         _ismbbprint(unsigned int);
int         _ismbbpunct(unsigned int);
int         _ismbbtrail(unsigned int);
int         _ismbslead(const unsigned char*,const unsigned char*);
int         _ismbstrail(const unsigned char*,const unsigned char*);
int         _setmbcp(int);

/* Parms for _setmbcp */
#define _MB_CP_SBCS     0
#define _MB_CP_OEM      -2
#define _MB_CP_ANSI     -3
#define _MB_CP_LOCALE   -4

#ifdef __cplusplus
}
#endif

#endif /* __WINE_MBCTYPE_H */
