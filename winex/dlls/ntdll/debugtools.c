/* debugtools.c
 *
 * Debugging functions
 *
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#include "wine/debug.h"
#include "wine/exception.h"
#include "wine/thread.h"
#include "winbase.h"
#include "winnt.h"
#include "winternl.h"
#include "wtypes.h"
#include "wine/server.h"
#include "msvcrt/excpt.h"

WINE_DECLARE_DEBUG_CHANNEL(tid);
WINE_DECLARE_DEBUG_CHANNEL(server);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);
WINE_DECLARE_DEBUG_CHANNEL(nolock);

/* ---------------------------------------------------------------------- */

struct debug_info
{
    char *str_pos;       /* current position in strings buffer */
    char *out_pos;       /* current position in output buffer */
    char  strings[1024]; /* buffer for temporary strings */
    char  output[1024];  /* current output line */
};

static struct debug_info initial_thread_info;  /* debug info for initial thread */

static BOOL deferredTrace = FALSE, traceEnabled = TRUE;

#ifdef USE_PTHREADS
static pthread_mutex_t WriteMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

/* get the debug info pointer for the current thread */
static inline struct debug_info *get_info(void)
{
    struct debug_info *info = NtCurrentTeb()->debug_info;

    if (!info) NtCurrentTeb()->debug_info = info = &initial_thread_info;
    if (!info->str_pos)
    {
        info->str_pos = info->strings;
        info->out_pos = info->output;
    }
    return info;
}

/* allocate some tmp space for a string */
static void *gimme1(int n)
{
    struct debug_info *info = get_info();
    char *res = info->str_pos;

    if (res + n >= &info->strings[sizeof(info->strings)]) res = info->strings;
    info->str_pos = res + n;
    return res;
}

/* release extra space that we requested in gimme1() */
static inline void release( void *ptr )
{
    struct debug_info *info = NtCurrentTeb()->debug_info;
    info->str_pos = ptr;
}

/* put an ASCII string into the debug buffer */
inline static char *put_string_a( const char *src, int n )
{
    char *dst, *res;

    if (n < 0) n = 0;
    else if (n > 200) n = 200;
    dst = res = gimme1 (n * 4 + 6);
    *dst++ = '"';
    while (n-- > 0 && *src)
    {
        unsigned char c = *src++;
        switch (c)
        {
        case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
        case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
        case '\t': *dst++ = '\\'; *dst++ = 't'; break;
        case '"': *dst++ = '\\'; *dst++ = '"'; break;
        case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
        default:
            if (c >= ' ' && c <= 126)
                *dst++ = c;
            else
            {
                *dst++ = '\\';
                *dst++ = '0' + ((c >> 6) & 7);
                *dst++ = '0' + ((c >> 3) & 7);
                *dst++ = '0' + ((c >> 0) & 7);
            }
        }
    }
    *dst++ = '"';
    if (*src)
    {
        *dst++ = '.';
        *dst++ = '.';
        *dst++ = '.';
    }
    *dst++ = '\0';
    release( dst );
    return res;
}

/* put a Unicode string into the debug buffer */
inline static char *put_string_w( const WCHAR *src, int n )
{
    char *dst, *res;

    if (n < 0) n = 0;
    else if (n > 200) n = 200;
    dst = res = gimme1 (n * 5 + 7);
    *dst++ = 'L';
    *dst++ = '"';
    while (n-- > 0 && *src)
    {
        WCHAR c = *src++;
        switch (c)
        {
        case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
        case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
        case '\t': *dst++ = '\\'; *dst++ = 't'; break;
        case '"': *dst++ = '\\'; *dst++ = '"'; break;
        case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
        default:
            if (c >= ' ' && c <= 126)
                *dst++ = c;
            else
            {
                *dst++ = '\\';
                sprintf(dst,"%04x",c);
                dst+=4;
            }
        }
    }
    *dst++ = '"';
    if (*src)
    {
        *dst++ = '.';
        *dst++ = '.';
        *dst++ = '.';
    }
    *dst++ = '\0';
    release( dst );
    return res;
}

/***********************************************************************
 *		wine_dbgstr_an (NTDLL.@)
 */
const char *wine_dbgstr_an( const char *src, int n )
{
    char *res, *old_pos;
    struct debug_info *info = get_info();

    if (!HIWORD(src))
    {
        if (!src) return "(null)";
        res = gimme1(6);
        sprintf(res, "#%04x", LOWORD(src) );
        return res;
    }
    /* save current position to restore it on exception */
    old_pos = info->str_pos;
    __TRY
    {
        res = put_string_a( src, n );
    }
    __EXCEPT(page_fault)
    {
        release( old_pos );
        return "(invalid)";
    }
    __ENDTRY
    return res;
}

/***********************************************************************
 *		wine_dbgstr_wn (NTDLL.@)
 */
const char *wine_dbgstr_wn( const WCHAR *src, int n )
{
    char *res, *old_pos;
    struct debug_info *info = get_info();

    if (!HIWORD(src))
    {
        if (!src) return "(null)";
        res = gimme1(6);
        sprintf(res, "#%04x", LOWORD(src) );
        return res;
    }

    /* save current position to restore it on exception */
    old_pos = info->str_pos;
    __TRY
    {
        res = put_string_w( src, n );
    }
    __EXCEPT(page_fault)
    {
        release( old_pos );
        return "(invalid)";
    }
    __ENDTRY
     return res;
}

/***********************************************************************
 *		wine_dbgstr_guid (NTDLL.@)
 */
const char *wine_dbgstr_guid( const GUID *id )
{
    char *str;

    if (!id) return "(null)";
    if (!HIWORD(id))
    {
        str = gimme1(12);
        sprintf( str, "<guid-0x%04x>", LOWORD(id) );
    }
    else
    {
        str = gimme1(40);
        sprintf( str, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 id->Data1, id->Data2, id->Data3,
                 id->Data4[0], id->Data4[1], id->Data4[2], id->Data4[3],
                 id->Data4[4], id->Data4[5], id->Data4[6], id->Data4[7] );
    }
    return str;
}

/***********************************************************************
 *		wine_dbgstr_longlong (NTDLL.@)
 */
const char *wine_dbgstr_longlong( ULONGLONG val )
{
    char *str;


    if (sizeof(ULONGLONG) > sizeof(unsigned long)){
        str = gimme1(20);

        sprintf(str, "0x%llx", val);
    }

    else{ 
        str = gimme1(12);

        sprintf(str, "0x%lx", (unsigned long)val);
    }


    return str;
}


/***********************************************************************
 *		wine_return_address (internal)
 */
static void* wine_return_address( unsigned int frame_no, CONTEXT *ctx )
{
#if defined(__i386__) && defined(__GNUC__)

    struct frame
    {
      struct frame* prev_frame_ptr;
      void* calling_addr;
    };
    struct frame* frame_ptr;
    unsigned int level;

    if (ctx) {
      /* Eip conveniently follows Ebp in CONTEXT86,
       * so the frame_ptr->calling_addr is right too. */
      frame_ptr = (struct frame*)&ctx->Ebp;
    }
    else {
      __asm__("movl %%ebp,%0" : "=r" (frame_ptr));
    }

    /* FIXME: Can we guarantee a valid ebp and that this code is ebp based? */
    for( level = 0; level <= frame_no; level++ )
    {
      /* Break if we reach a NULL frame. Frame must also be at least 4 byte aligned */
      if( frame_ptr == NULL ) return NULL;
      if( (long)frame_ptr->prev_frame_ptr % 4 ) return NULL;

      frame_ptr = frame_ptr->prev_frame_ptr;
    }

    return frame_ptr ? frame_ptr->calling_addr : NULL;
#else
#warning "Not implemented for this setup"
  return NULL;
#endif
}

typedef struct bt_elem
{
  BOOL bUsed;
  char* bt;
} bt_elem;

#define NUM_BT_ELEMS 1024
static bt_elem backtrace_array[NUM_BT_ELEMS] = { {FALSE,NULL} };

/***********************************************************************
 *		CreateAndStoreBacktrace (internal)
 *
 * Store a backtrace into a given index. Must be called appropriately locked.
 * This is not exactly what windows would do. They have a field in
 * HKLM\Software\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\YourProgram
 * which contains the max size of the allowed area for these backtraces and if they should
 * be enabled. This implementation just cheats and does it for everything since the overhead
 * shouldn't be that nasty (famous last words?)
 */
WORD CreateAndStoreBacktrace(void)
{
  const char* backtrace; 
  WORD wIndex;

  backtrace = wine_dbgstack(0, NULL);
  
  if( backtrace == NULL ) return 0;

  for( wIndex = 0; wIndex < NUM_BT_ELEMS; wIndex++ )
  {
    if( backtrace_array[ wIndex ].bUsed == FALSE ) break;
  }

  if( wIndex == 1024 ) return 0;

  backtrace_array[ wIndex ].bt = malloc( strlen( backtrace ) + 1 );

  if( backtrace_array[ wIndex ].bt != NULL )
  {
    strcpy( backtrace_array[ wIndex ].bt, backtrace );
    backtrace_array[ wIndex ].bUsed = TRUE;
  }

  release( (void*)backtrace );

  return wIndex + 1;
}

/***********************************************************************
 *		RetrieveBacktrace (internal)
 */
const char* RetrieveBacktrace( WORD wIndex )
{
  if( wIndex == 0 ) return NULL;

  if( backtrace_array[ wIndex-1 ].bUsed == FALSE ) return NULL;

  return backtrace_array[ wIndex-1 ].bt;
}

/***********************************************************************
 *		ReleaseBacktrace (internal)
 */
void ReleaseBacktrace( WORD wIndex )
{
  if( wIndex )
  {
    free( backtrace_array[ wIndex-1 ].bt );
    backtrace_array[ wIndex-1 ].bUsed = FALSE;
  }
}

/***********************************************************************
 *		wine_dbgstack (NTDLL.@)
 *
 * Dump at most dump_upto levels of stack frames or unlimited if dump_upto is 0.
 */
const char *wine_dbgstack( unsigned int dump_upto, CONTEXT *ctx )
{
  unsigned int maxlevel = 0;
  unsigned int level;
  void* addr = NULL;
  char* buf;
  char* buf_orig;

  /* FIXME: gimme1 is pretty brain damaged so limit the number of levels otherwise we may overflow */
  if( dump_upto == 0 ) dump_upto = 20;

  while(TRUE)
  {
        __TRY
        {
            addr = wine_return_address( maxlevel, ctx );
        }
        __EXCEPT(page_fault)
        {
            /* Assume we can never fault on frame 0 */
            addr = NULL;
        }
        __ENDTRY

        if( addr == NULL ) break;
        if( (dump_upto != 0) && (maxlevel >= dump_upto) ) break;
        maxlevel++;
  }

  buf = buf_orig = gimme1( (maxlevel)*( 12 ) ); /* # levels * sizeof( ptr string + ',') */ 

  if( !buf ) return NULL;

  for( level = 1; level <= maxlevel; level++ )
  {
    buf += sprintf( buf, "%p%c", wine_return_address( level-1, ctx ), (maxlevel - level) ? ',' : ' ' );
  }
 
  return buf_orig;
}


/***********************************************************************
 *		strrevchr (internal)
 *  finds the character <c> in the string <base> when the end of the
 *  string is already known.  <end> points to location in the string to
 *  begin the reverse search at.  <end> must be larger than <base>.
 *  if <c> is not found in the string NULL is returned.  If <c> is found,
 *  a pointer to the last occurrence of that character is returned.
 */
static char *strrevchr(char *base, char *end, int c){
    char *pos = end;


    for (pos = end; pos >= base; pos--){
        if (*pos == c)
            return pos;
    }

    return NULL;
}

/***********************************************************************
 *		wine_dbg_vprintf (NTDLL.@)
 */
int wine_dbg_vprintf( const char *format, va_list args )
{
    struct debug_info *info;
    char *p;
    int ret;

    if (!traceEnabled)
        return 0;

    info = get_info();

    ret = vsnprintf( info->out_pos, sizeof(info->output) - (info->out_pos - info->output),
                         format, args );

    /* make sure we didn't exceed the buffer length
     * the two checks are due to glibc changes in vsnprintfs return value
     * the buffer size can be exceeded in case of a missing \n in
     * debug output */
    if ((ret == -1) || (ret >= sizeof(info->output) - (info->out_pos - info->output)))
    {
       fprintf( stderr, "err:wine_dbg_vprintf: !!FATAL!! debugstr buffer overflow (contents: '%s', ret = %d, sizeof(output) = %lu, out_pos = %p, output = %p, spaceLeft = %ld)\nAborting...\n",
                info->output, ret, sizeof(info->output), info->out_pos, info->output, sizeof(info->output) - (info->out_pos - info->output));
       info->out_pos = info->output;
       abort();
    }

    p = strrevchr( info->out_pos, info->out_pos + ret - 1, '\n' );
    if (!p) info->out_pos += ret;
    else
    {
        char *pos = info->output;
        p++;
#ifdef USE_PTHREADS
        if (!TRACE_ON (nolock))
           pthread_mutex_lock (&WriteMutex);
#endif
        write( 2, pos, p - pos );
#ifdef USE_PTHREADS
        if (!TRACE_ON (nolock))
           pthread_mutex_unlock (&WriteMutex);
#endif
        /* move beginning of next line to start of buffer */
        while ((*pos = *p++)) pos++;
        info->out_pos = pos;
    }
    return ret;
}

/***********************************************************************
 *		wine_dbg_printf (NTDLL.@)
 */
int wine_dbg_printf(const char *format, ...)
{
    int ret;
    va_list valist;

    if (!traceEnabled)
        return 0;
    va_start(valist, format);
    ret = wine_dbg_vprintf( format, valist );
    va_end(valist);
    return ret;
}

/***********************************************************************
 *		wine_dbg_sprintf (NTDLL.@)
 */
const char *wine_dbg_sprintf( const char *format, ... )
{
    const int           bufSize = 256;
    struct debug_info * info;
    char *              str;
    int                 ret;
    va_list             args;


    if (!traceEnabled)
        return "";


    info = get_info();


    va_start(args, format);
    
    /* allocate a buffer to write to.  String length + 2 seems to be pretty
        standard for all other similar functions here. */
    str = (char *)gimme1(bufSize);

    /* print the string */
    ret = vsnprintf( str, bufSize - 1, format, args );

    va_end(args);


    /* the buffer was too small => truncate the string */
    if (ret == -1 || ret >= bufSize){
        str[bufSize - 2] = '@';
        str[bufSize - 1] = 0;
    }

    /* only a part of the buffer was used => release the extra space back to the string pool */
    else{
        str[ret] = 0;
        info->str_pos -= (bufSize - ret - 1);
    }


    return str;
}

/***********************************************************************
 *		wine_dbg_switch_trace (NTDLL.@)
 * get/set debug trace state
 *  init = TRUE: init feature
 *  switchIt : TRUE to switch trace state
 *  returns -1 if feature not used, 0 if trace disabled, 1 if enabled
 */
int wine_dbg_switch_trace(BOOL init, BOOL switchIt)
{
    if (init)
    {
         deferredTrace = TRUE;
         traceEnabled = FALSE;
         return 0;
    }
    if (!deferredTrace)
        return -1;
    if (switchIt)
    {
        traceEnabled = !traceEnabled;
        if (TRACE_ON(server))
        {
           SERVER_START_REQ( enable_trace )
           {
               req->debug_level = (traceEnabled == 1) ? TRACE_ON(server) : 0;
               wine_server_call( req );
           }
           SERVER_END_REQ;
        }
    }
    return traceEnabled ? 1 : 0;
}

/***********************************************************************
 *		wine_dbg_log (NTDLL.@)
 */
int wine_dbg_log(enum __WINE_DEBUG_CLASS cls, const char *channel,
                 const char *function, const char *format, ... )
{
    static const char *classes[__WINE_DBCL_COUNT] = { "fixme", "err", "warn", "trace" };
    va_list valist;
    int ret = 0;

    if (!traceEnabled)
        return 0;

    va_start(valist, format);

    if (TRACE_ON(timestamp))
        ret += wine_dbg_printf( "%ld - ", NtGetTickCount());
    if (TRACE_ON(tid))
        ret += wine_dbg_printf( "%04lx:", (DWORD)NtCurrentTeb()->tid );
    if (cls < __WINE_DBCL_COUNT)
        ret += wine_dbg_printf( "%s:%s:%s ", classes[cls], channel + 1, function );
    if (format)
        ret += wine_dbg_vprintf( format, valist );

    va_end(valist);

    return ret;
}
