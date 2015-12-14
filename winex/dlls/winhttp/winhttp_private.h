#ifndef  __WINHTTP_PRIVATE_H__
# define __WINHTTP_PRIVATE_H__


# define _WINE_WINHTTP_INTERNAL_ONLY_
# include "wininet.h"
# include "winhttp.h"


/* these protocol scheme values conflict between WinHTTP and WinINet 
   even though they share the same names.  We'll define the proper
   values here for WinHTTP. */
# define WINHTTP_INTERNET_SCHEME_HTTP        (1)
# define WINHTTP_INTERNET_SCHEME_HTTPS       (2)

#endif
