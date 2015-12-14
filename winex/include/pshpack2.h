#if defined(__WINE_PSHPACK_H4)

   /* Depth > 4 */
#  error "Alignment nesting > 4 is not supported"

#else

#  if !defined(__WINE_PSHPACK_H)
#    define __WINE_PSHPACK_H  2
     /* Depth == 1 */
#  elif !defined(__WINE_PSHPACK_H2)
#    define __WINE_PSHPACK_H2 2
     /* Depth == 2 */
#    define __WINE_INTERNAL_POPPACK
#    include "poppack.h"
#  elif !defined(__WINE_PSHPACK_H3)
#    define __WINE_PSHPACK_H3 2
     /* Depth == 3 */
#    define __WINE_INTERNAL_POPPACK
#    include "poppack.h"
#  elif !defined(__WINE_PSHPACK_H4)
#    define __WINE_PSHPACK_H4 2
     /* Depth == 4 */
#    define __WINE_INTERNAL_POPPACK
#    include "poppack.h"
#  endif

#  if _MSC_VER >= 800
#   pragma warning(disable:4103)
#  endif

#  if defined(__GNUC__) || defined(__SUNPRO_C) || defined(__SUNPRO_CC) || defined(_MSC_VER)
#    pragma pack(2)
#  elif !defined(RC_INVOKED)
#    error "Adjusting the alignment is not supported with this compiler"
#  endif

#endif
