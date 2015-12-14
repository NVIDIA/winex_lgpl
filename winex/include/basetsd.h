/*
 * Compilers that uses ILP32, LP64 or P64 type models
 * for both Win32 and Win64 are supported by this file.
 */

#ifndef __WINE_BASETSD_H
#define __WINE_BASETSD_H


#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

/**** Some Wine specific definitions *****/
/* Architecture dependent settings. */
/* These are hardcoded to avoid dependencies on config.h in Winelib apps. */
#if defined(__i386__)
# undef  WORDS_BIGENDIAN
# undef  BITFIELDS_BIGENDIAN
# define ALLOW_UNALIGNED_ACCESS
#elif defined(__sparc__)
# define WORDS_BIGENDIAN
# define BITFIELDS_BIGENDIAN
# undef  ALLOW_UNALIGNED_ACCESS
#elif defined(__PPC__)
# define WORDS_BIGENDIAN
# define BITFIELDS_BIGENDIAN
# undef  ALLOW_UNALIGNED_ACCESS
#elif defined(__mips__)
# undef  WORDS_BIGENDIAN
/*ECL for wine/ports.h*/
# define WORDS_LITTLEENDIAN
# undef  BITFIELDS_BIGENDIAN
# undef  ALLOW_UNALIGNED_ACCESS
#elif !defined(RC_INVOKED) && !defined(__WIDL__)
# error Unknown CPU architecture!
#endif

/*
 * Win32 was easy to implement under Unix since most (all?) 32-bit
 * Unices uses the same type model (ILP32) as Win32, where int, long
 * and pointer are 32-bit.
 *
 * Win64, however, will cause some problems when implemented under Unix.
 * Linux/{Alpha, Sparc64} and most (all?) other 64-bit Unices uses
 * the LP64 type model where int is 32-bit and long and pointer are
 * 64-bit. Win64 on the other hand uses the P64 (sometimes called LLP64)
 * type model where int and long are 32 bit and pointer is 64-bit.
 */

/* Type model indepent typedefs */
#if !defined(__WIDL__)

#if !defined(__int8)
       typedef signed char   __int8;
       typedef unsigned char __uint8;
#else
       typedef unsigned __int8  __uint8;
#endif
#if !defined(__int16)
       typedef signed short   __int16;
       typedef unsigned short __uint16;
#else
       typedef unsigned __int16 __uint16;
#endif
#if !defined(__int32)
       typedef signed int   __int32;
       typedef unsigned int __uint32;
#else
       typedef unsigned __int32 __uint32;
#endif
#if !defined(__int64)
       typedef signed long long   __int64;
       typedef unsigned long long __uint64;
#else
       typedef unsigned __int64 __uint64;
#endif

#endif // !defined(__WIDL__)

#if defined(_WIN64)

typedef __uint32 __ptr32;
typedef void    *__ptr64;

#else /* FIXME: defined(_WIN32) */

#if !defined(_MSC_VER)
typedef void    *__ptr32;
typedef unsigned __int64 __ptr64;
#endif /* !defined(_MSC_VER) */

#endif

typedef signed short INT16;
typedef INT16 *PINT16;
typedef unsigned short UINT16;
typedef UINT16 *PUINT16;

/* Always signed and 32 bit wide */

typedef signed int LONG32;
typedef signed int INT32;

typedef LONG32 *PLONG32;
typedef INT32  *PINT32;

/* Always unsigned and 32 bit wide */

typedef unsigned int ULONG32;
typedef unsigned int DWORD32;
typedef unsigned int UINT32;

typedef ULONG32 *PULONG32;
typedef DWORD32 *PDWORD32;
typedef UINT32  *PUINT32;

/* Always signed and 64 bit wide */

typedef signed __int64 LONG64;
typedef signed __int64 INT64;

typedef LONG64 *PLONG64;
typedef INT64  *PINT64;

/* Always unsigned and 64 bit wide */

typedef unsigned __int64 ULONG64;
typedef unsigned __int64 DWORD64;
typedef unsigned __int64 UINT64;

typedef ULONG64 *PULONG64;
typedef DWORD64 *PDWORD64;
typedef UINT64  *PUINT64;

/* Win32 or Win64 dependent typedef/defines. */

#ifdef _WIN64

typedef __int64 INT_PTR, *PINT_PTR;
typedef unsigned __int64 UINT_PTR, *PUINT_PTR;

#define MAXINT_PTR 0x7fffffffffffffff
#define MININT_PTR 0x8000000000000000
#define MAXUINT_PTR 0xffffffffffffffff

typedef int HALF_PTR, *PHALF_PTR;
typedef unsigned int UHALF_PTR, *PUHALF_PTR;

#define MAXHALF_PTR 0x7fffffff
#define MINHALF_PTR 0x80000000
#define MAXUHALF_PTR 0xffffffff

typedef __int64 LONG_PTR, *PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
typedef unsigned __int64 DWORD_PTR, *PDWORD_PTR;

#else /* FIXME: defined(_WIN32) */

typedef int INT_PTR, *PINT_PTR;
typedef unsigned int UINT_PTR, *PUINT_PTR;

#define MAXINT_PTR 0x7fffffff
#define MININT_PTR 0x80000000
#define MAXUINT_PTR 0xffffffff

typedef short HALF_PTR, *PHALF_PTR;
typedef unsigned short UHALF_PTR, *PUHALF_PTR;

#define MAXUHALF_PTR 0xffff
#define MAXHALF_PTR 0x7fff
#define MINHALF_PTR 0x8000

typedef long LONG_PTR, *PLONG_PTR;
typedef unsigned long ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;

#endif /* defined(_WIN64) || defined(_WIN32) */

typedef INT_PTR SSIZE_T, *PSSIZE_T;
typedef UINT_PTR SIZE_T, *PSIZE_T;


/**** Some Wine specific definitions *****/
                                                                                                                                                       
/* Architecture dependent settings. */
/* These are hardcoded to avoid dependencies on config.h in Winelib apps. */
#if defined(__i386__)
# undef  WORDS_BIGENDIAN
# undef  BITFIELDS_BIGENDIAN
# define ALLOW_UNALIGNED_ACCESS
#elif defined(__sparc__)
# define WORDS_BIGENDIAN
# define BITFIELDS_BIGENDIAN
# undef  ALLOW_UNALIGNED_ACCESS
#elif defined(__PPC__)
# define WORDS_BIGENDIAN
# define BITFIELDS_BIGENDIAN
# undef  ALLOW_UNALIGNED_ACCESS
#elif !defined(RC_INVOKED) && !defined(__WIDL__)
# error Unknown CPU architecture!
#endif


#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINE_BASETSD_H) */



