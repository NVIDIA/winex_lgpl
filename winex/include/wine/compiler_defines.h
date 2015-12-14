

/* Define a number of compiler hints:
 *
 * WINE_UNUSED - This variable is unused in this context 
 *               Don't warn me about it.
 * WINE_USED   - This variable or function is actually used
 *               (probably in ASM) and the compiler should
 *               trust that I know more than it.
 * WINE_NORETURN - A compiler hint that this function will not
 *                 return.
 *
 * WINE_PACKED - We need special packing of the structure for
 *               binary compatibility.
 *
 * WINE_EXPECT - A compiler hint as to the expected outcome of
 *               a logical comparison.
 *
 * WINE_ALIGN -  A compiler hint that allows the explicit 
 *               alignment of struct members.
 */
#ifdef __GNUC__
#define WINE_PACKED   __attribute__((packed))

/* unused attribute changed to used in 3.3.3 for this purpose */
#  if (__GNUC__ < 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ < 3)) || (( __GNUC__ == 3 ) && (__GNUC_MINOR__ == 3) && (__GNUC_PATCHLEVEL__ < 3))
#    define WINE_USED   __attribute__((unused)) /* old way */
#  else
#    define WINE_USED   __attribute__((used)) /* new way */
#  endif

#define WINE_UNUSED __attribute__((unused))
#define WINE_NORETURN __attribute__((noreturn))

/* __builtin_expect support starts at gcc 2.96 */
#  if (__GNUC__ < 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ < 96))
#    define WINE_EXPECT( exp, c ) (exp)
#  else  /* gcc is >= 2.96 */
#    define WINE_EXPECT( exp, c ) __builtin_expect( (exp), (c) )
#  endif /* GCC >= 2.96 */

#define WINE_ALIGN(x)   __attribute__((aligned (x)))

#else

#define WINE_PACKED    /* nothing */
#define WINE_UNUSED    /* nothing */
#define WINE_USED    /* nothing */
#define WINE_NORETURN  /* nothing */
#define WINE_EXPECT( exp, c ) (exp)
#define WINE_ALIGN(x)   /* nothing */
#endif
