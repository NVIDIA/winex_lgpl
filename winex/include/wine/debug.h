/*
 * Wine debugging interface
 */

#ifndef __WINE_WINE_DEBUG_H
#define __WINE_WINE_DEBUG_H

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include "windef.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _GUID;

/*
 * Internal definitions (do not use these directly)
 */

enum __WINE_DEBUG_CLASS {
    __WINE_DBCL_FIXME,
    __WINE_DBCL_ERR,
    __WINE_DBCL_WARN,
    __WINE_DBCL_TRACE,
    __WINE_DBCL_COUNT
};

#ifndef NO_TRACE_MSGS
# define __WINE_GET_DEBUGGING_TRACE(dbch) ((dbch)[0] & (1 << __WINE_DBCL_TRACE))
#else
# define __WINE_GET_DEBUGGING_TRACE(dbch) 0
#endif

#ifndef NO_DEBUG_MSGS
# define __WINE_GET_DEBUGGING_WARN(dbch)  ((dbch)[0] & (1 << __WINE_DBCL_WARN))
# define __WINE_GET_DEBUGGING_FIXME(dbch) ((dbch)[0] & (1 << __WINE_DBCL_FIXME))
#else
# define __WINE_GET_DEBUGGING_WARN(dbch)  0
# define __WINE_GET_DEBUGGING_FIXME(dbch) 0
#endif

/* define error macro regardless of what is configured */
#define __WINE_GET_DEBUGGING_ERR(dbch)  ((dbch)[0] & (1 << __WINE_DBCL_ERR))

#define __WINE_GET_DEBUGGING(dbcl,dbch)  __WINE_GET_DEBUGGING##dbcl(dbch)
#define __WINE_SET_DEBUGGING(dbcl,dbch,on) \
    ((on) ? ((dbch)[0] |= 1 << (dbcl)) : ((dbch)[0] &= ~(1 << (dbcl))))

#ifdef __GNUC__

#define __WINE_DPRINTF(dbcl,dbch) \
  do { if(WINE_EXPECT( __WINE_GET_DEBUGGING(dbcl,(dbch)), 0)) { \
       const char * const __dbch = (dbch); \
       const enum __WINE_DEBUG_CLASS __dbcl = __WINE_DBCL##dbcl; \
       __WINE_DBG_LOG

#define __WINE_DBG_LOG(args...) \
    wine_dbg_log( __dbcl, __dbch, __FUNCTION__, args); } } while(0)

#define __WINE_PRINTF_ATTR(fmt,args) __attribute__((format (printf,fmt,args)))

#else  /* __GNUC__ */

#define __WINE_DPRINTF(dbcl,dbch) \
    (!__WINE_GET_DEBUGGING(dbcl,(dbch)) || \
     (wine_dbg_log(__WINE_DBCL##dbcl,(dbch),__FILE__,"%d: ",__LINE__),0)) ? \
     (void)0 : (void)wine_dbg_printf

#define __WINE_PRINTF_ATTR(fmt, args)

#endif  /* __GNUC__ */


/*
 * Exported definitions and macros
 */

/* These function return a printable version of a string, including
   quotes.  The string will be valid for some time, but not indefinitely
   as strings are re-used.  */
extern const char *wine_dbgstr_an( const char * s, int n );
extern const char *wine_dbgstr_wn( const WCHAR *s, int n );
extern const char *wine_dbgstr_guid( const struct _GUID *id );
extern const char *wine_dbgstack( unsigned int upto, CONTEXT *ctx );
extern const char *wine_dbgstr_longlong( ULONGLONG val );

extern int wine_dbg_vprintf( const char *format, va_list args ) __WINE_PRINTF_ATTR(1,0);
extern const char *wine_dbg_sprintf( const char *format, ... ) __WINE_PRINTF_ATTR(1, 2);
extern int wine_dbg_printf( const char *format, ... ) __WINE_PRINTF_ATTR(1,2);
extern int wine_dbg_log( enum __WINE_DEBUG_CLASS cls, const char *ch,
                         const char *func, const char *format, ... ) __WINE_PRINTF_ATTR(4,5);

inline static const char *wine_dbgstr_a( const char *s )  { return wine_dbgstr_an( s, 80 ); }
inline static const char *wine_dbgstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, 80 ); }

#define WINE_TRACE                 __WINE_DPRINTF(_TRACE,__wine_dbch___default)
#define WINE_TRACE_(ch)            __WINE_DPRINTF(_TRACE,__wine_dbch_##ch)
#define WINE_TRACE_ON(ch)          __WINE_GET_DEBUGGING(_TRACE,__wine_dbch_##ch)

#define WINE_WARN                  __WINE_DPRINTF(_WARN,__wine_dbch___default)
#define WINE_WARN_(ch)             __WINE_DPRINTF(_WARN,__wine_dbch_##ch)
#define WINE_WARN_ON(ch)           __WINE_GET_DEBUGGING(_WARN,__wine_dbch_##ch)

#define WINE_FIXME                 __WINE_DPRINTF(_FIXME,__wine_dbch___default)
#define WINE_FIXME_(ch)            __WINE_DPRINTF(_FIXME,__wine_dbch_##ch)
#define WINE_FIXME_ON(ch)          __WINE_GET_DEBUGGING(_FIXME,__wine_dbch_##ch)

#define WINE_ERR                   __WINE_DPRINTF(_ERR,__wine_dbch___default)
#define WINE_ERR_(ch)              __WINE_DPRINTF(_ERR,__wine_dbch_##ch)
#define WINE_ERR_ON(ch)            __WINE_GET_DEBUGGING(_ERR,__wine_dbch_##ch)

#define WINE_DECLARE_DEBUG_CHANNEL(ch) \
    extern char __wine_dbch_##ch[]
#define WINE_DEFAULT_DEBUG_CHANNEL(ch) \
    extern char __wine_dbch_##ch[]; \
    static char * const __wine_dbch___default = __wine_dbch_##ch

#define WINE_DPRINTF               wine_dbg_printf
#define WINE_MESSAGE               wine_dbg_printf

#ifdef __WINE__
/* Wine uses shorter names that are very likely to conflict with other software */

inline static const char *debugstr_an( const char * s, int n ) { return wine_dbgstr_an( s, n ); }
inline static const char *debugstr_wn( const WCHAR *s, int n ) { return wine_dbgstr_wn( s, n ); }
inline static const char *debugstr_guid( const struct _GUID *id ) { return wine_dbgstr_guid(id); }
inline static const char *debugstr_longlong( ULONGLONG val ) { return wine_dbgstr_longlong(val); }
inline static const char *debugstr_a( const char *s )  { return wine_dbgstr_an( s, 80 ); }
inline static const char *debugstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, 80 ); }
inline static const char *debugres_a( const char *s )  { return wine_dbgstr_an( s, 80 ); }
inline static const char *debugres_w( const WCHAR *s ) { return wine_dbgstr_wn( s, 80 ); }
inline static const char *debugstack( unsigned int upto, CONTEXT *ctx ) { return wine_dbgstack( upto, ctx ); }

#define TRACE                      WINE_TRACE
#define TRACE_(ch)                 WINE_TRACE_(ch)
#define TRACE_ON(ch)               WINE_TRACE_ON(ch)

#define WARN                       WINE_WARN
#define WARN_(ch)                  WINE_WARN_(ch)
#define WARN_ON(ch)                WINE_WARN_ON(ch)

#define FIXME                      WINE_FIXME
#define FIXME_(ch)                 WINE_FIXME_(ch)
#define FIXME_ON(ch)               WINE_FIXME_ON(ch)

#undef ERR  /* Solaris got an 'ERR' define in <sys/reg.h> */
#define ERR                        WINE_ERR
#define ERR_(ch)                   WINE_ERR_(ch)
#define ERR_ON(ch)                 WINE_ERR_ON(ch)

#define DECLARE_DEBUG_CHANNEL(ch)  WINE_DECLARE_DEBUG_CHANNEL(ch)
#define DEFAULT_DEBUG_CHANNEL(ch)  WINE_DEFAULT_DEBUG_CHANNEL(ch)

#define DPRINTF                    WINE_DPRINTF
#define MESSAGE                    WINE_MESSAGE

#endif /* __WINE__ */

#define SHOW_MSG_ONCE(type, ...) do{ \
        static int _msgShown = 0; \
        if (!_msgShown){ \
            type(__VA_ARGS__); \
            _msgShown = 1; \
        } \
    } while (0)

#define __CRITICAL_SECTION_DEFINE(crit, function) do{ \
        static char name[sizeof(__FUNCTION__) + sizeof(#crit) + 16]; \
        snprintf(name, sizeof(name), "%d %s() %s", __LINE__, __FUNCTION__, #crit); \
        function(crit); \
        CRITICAL_SECTION_NAME(crit, name); \
    } while (0)

#define CRITICAL_SECTION_DEFINE(crit)     __CRITICAL_SECTION_DEFINE(crit, InitializeCriticalSection)
#define RTL_CRITICAL_SECTION_DEFINE(crit) __CRITICAL_SECTION_DEFINE(crit, RtlInitializeCriticalSection)


#ifdef __i386__
/* Memory watchpoints */
/* - generates a trace line whenever a memory address is written */
/* - not compatible with other debuggers nor with copy protection */
/* - each watch only applies to the thread it was set in */
/* - up to 4 watches per thread (n is between 0 and 3) */
/* - size is either 1, 2, or 4 */
extern void wine_start_memwatch( unsigned n, const void * a, unsigned size );
extern void wine_stop_memwatch( unsigned n );
#endif

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINE_DEBUG_H */
