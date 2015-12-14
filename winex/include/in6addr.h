#ifndef  __IN6ADDR_H__
# define __IN6ADDR_H__

# ifdef USE_WS_PREFIX
#  define WS(x)  WS_##x
# else
#  define WS(x)  x
# endif


typedef struct WS(in6_addr)
{
    union
    {
        UCHAR       Byte[16];
        USHORT      Word[8];
    } u;
} IN6_ADDR, *PIN6_ADDR, FAR *LPIN6_ADDR;

# define in_addr6 WS(in6_addr)

# ifdef USE_WS_PREFIX
#  define WS__S6_un      u
#  define WS__S6_u8      Byte
#  define WS_s6_addr     WS__S6_un._S6_u8
# else
#  define _S6_un         u
#  define _S6_u8         Byte
#  define s6_addr        _S6_un._S6_u8
# endif

# define WS_s6_bytes    u.Byte
# define WS_s6_words    u.Word

#endif
