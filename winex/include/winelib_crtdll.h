/* <wine>/include/winelib_crtdll.h
 *
 * This file defines the utilities available in MSDEV's basic include files
 * (e.g. stdlib.h) that are not generally available to wine or have different
 * implementations under UNIX
 */

#ifndef __WINELIB_CRTDLL_H
#define __WINELIB_CRTDLL_H

/***********************************************************************
 * The following declarations are for the clients of winelib, not for
 * the implementation of wine itself.
 *
 * Theee declarations map the standard C API functions and global
 * variables to their wine implementations.  */
#ifndef __WINE__

#include "windef.h"
#include "wine/windef16.h"

/* This header overides many definitions that are usually in the
   following headers by using the C preprocessor.  We include them now
   so that they will not be included again later with confusing
   results and piles of error messages. */
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <stddef.h>

/* The following headers define types that are used within this header */
#include <time.h>
#include <setjmp.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

/********************************************************
 *   Types
 */


typedef struct
{
    HANDLE handle;
    int      pad[7];
} CRTDLL_FILE;

typedef void (*_INITTERMFUN)();

struct find_t
{   unsigned    attrib;
    time_t      time_create;    /* -1 when not avaiable */
    time_t      time_access;    /* -1 when not avaiable */
    time_t      time_write;
    unsigned long       size;   /* fixme: 64 bit ??*/
    char        name[260];
};

typedef VOID (*new_handler_type)(VOID);

typedef VOID (*sig_handler_type)(VOID);

struct win_stat
{
    UINT16 win_st_dev;
    UINT16 win_st_ino;
    UINT16 win_st_mode;
    INT16  win_st_nlink;
    INT16  win_st_uid;
    INT16  win_st_gid;
    UINT win_st_rdev;
    INT  win_st_size;
    INT  win_st_atime;
    INT  win_st_mtime;
    INT  win_st_ctime;
};

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************
 *   Prototypes for functions implemented in crtdll.c
 */

unsigned char * __cdecl CRTDLL__mbsinc(unsigned char *x);
char __cdecl *CRTDLL__strrev(char *string);
DWORD __cdecl CRTDLL__GetMainArgs(LPDWORD argc,LPSTR **argv, LPSTR *environ,DWORD flag);
INT __cdecl CRTDLL__abnormal_termination(void);
INT __cdecl CRTDLL__access(LPCSTR filename, INT mode);
void __cdecl CRTDLL__cexit(INT ret);
INT __cdecl CRTDLL__chdir(LPCSTR newdir);
BOOL __cdecl CRTDLL__chdrive(INT newdrive);
INT __cdecl CRTDLL__close(HFILE fd);
VOID __cdecl CRTDLL__dllonexit ();
LPINT __cdecl CRTDLL__errno();
int __cdecl CRTDLL__fileno( CRTDLL_FILE *stream);
DWORD __cdecl CRTDLL__findfirst(LPCSTR fname,  struct find_t * x2);
INT __cdecl CRTDLL__findnext(DWORD hand, struct find_t * x2);
VOID __cdecl CRTDLL__fpreset(void);
int __cdecl CRTDLL__fstat(int file, struct stat* buf);
LONG __cdecl CRTDLL__ftol(void);
LPSTR __cdecl CRTDLL__fullpath(LPSTR buf, LPCSTR name, INT size);
HFILE __cdecl CRTDLL__get_osfhandle(INT flags);
CHAR* __cdecl CRTDLL__getcwd(LPSTR buf, INT size);
CHAR* __cdecl CRTDLL__getdcwd(INT drive,LPSTR buf, INT size);
INT __cdecl CRTDLL__getdrive(VOID);
DWORD __cdecl CRTDLL__initterm(_INITTERMFUN *start,_INITTERMFUN *end);
BOOL __cdecl CRTDLL__isatty(DWORD x);
BOOL __cdecl CRTDLL__isctype(CHAR x,CHAR type);
LPSTR  __cdecl CRTDLL__itoa(INT x,LPSTR buf,INT buflen);
DWORD __cdecl CRTDLL__lrotl(DWORD x,INT shift);
LPSTR  __cdecl CRTDLL__ltoa(long x,LPSTR buf,INT radix);
void __cdecl CRTDLL__makepath(CHAR *path, const CHAR *drive, const CHAR *dir, const CHAR *fname, const CHAR *ext);
int __cdecl CRTDLL__mbsicmp(unsigned char *x,unsigned char *y);
LPSTR __cdecl CRTDLL__mbsrchr(LPSTR s,CHAR x);
INT __cdecl CRTDLL__memicmp(	LPCSTR s1, LPCSTR s2, DWORD len);
INT __cdecl CRTDLL__mkdir(LPCSTR newdir);
HFILE __cdecl CRTDLL__open(LPCSTR path,INT flags);
HFILE __cdecl CRTDLL__open_osfhandle(LONG osfhandle, INT flags);
INT __cdecl CRTDLL__read(INT fd, LPVOID buf, UINT count);
int __cdecl CRTDLL__rmdir(const char *dirname);
UINT __cdecl CRTDLL__rotl(UINT x,INT shift);
INT __cdecl CRTDLL__setmode( INT fh,INT mode);
VOID __cdecl CRTDLL__sleep(unsigned long timeout);
VOID __cdecl CRTDLL__splitpath(LPCSTR path, LPSTR drive, LPSTR directory, LPSTR filename, LPSTR extension );
int __cdecl CRTDLL__stat(const char * filename, struct win_stat * buf);
INT __cdecl CRTDLL__strcmpi( LPCSTR s1, LPCSTR s2 );
LPSTR __cdecl CRTDLL__strdate (LPSTR date);
LPSTR __cdecl CRTDLL__strdup(LPCSTR ptr);
LPSTR __cdecl CRTDLL__strlwr(LPSTR x);
INT __cdecl CRTDLL__strnicmp( LPCSTR s1, LPCSTR s2, INT n );
LPSTR __cdecl CRTDLL__strtime (LPSTR date);
LPSTR __cdecl CRTDLL__strupr(LPSTR x);
LPSTR __cdecl CRTDLL__tempnam(LPCSTR dir, LPCSTR prefix);
LPSTR  __cdecl CRTDLL__ultoa(long x,LPSTR buf,INT radix);
INT __cdecl CRTDLL__unlink(LPCSTR pathname);
LPWSTR __cdecl CRTDLL__wcsdup(LPCWSTR ptr);
DWORD __cdecl CRTDLL__wcsicmp( LPCWSTR s1, LPCWSTR s2 );
DWORD __cdecl CRTDLL__wcsicoll(LPCWSTR a1,LPCWSTR a2);
LPWSTR __cdecl CRTDLL__wcslwr(LPWSTR x);
DWORD __cdecl CRTDLL__wcsnicmp( LPCWSTR s1, LPCWSTR s2, INT len );
VOID __cdecl CRTDLL__wcsrev(LPWSTR s);
LPWSTR __cdecl CRTDLL__wcsupr(LPWSTR x);
INT __cdecl CRTDLL__write(INT fd,LPCVOID buf,UINT count);
INT __cdecl CRTDLL_atexit(LPVOID x);
VOID* __cdecl CRTDLL_calloc(DWORD size, DWORD count);
void __cdecl CRTDLL_clearerr( CRTDLL_FILE *stream);
clock_t __cdecl CRTDLL_clock(void);
VOID __cdecl CRTDLL_delete(VOID* ptr);
void __cdecl CRTDLL_exit(DWORD ret);
INT __cdecl CRTDLL_fclose( CRTDLL_FILE *file );
INT __cdecl CRTDLL_feof( CRTDLL_FILE *file );
int __cdecl CRTDLL_ferror( CRTDLL_FILE *file);
int __cdecl CRTDLL_ferror(CRTDLL_FILE *_stream);
INT __cdecl CRTDLL_fflush( CRTDLL_FILE *file );
INT __cdecl CRTDLL_fgetc( CRTDLL_FILE *file );
CHAR* __cdecl CRTDLL_fgets( LPSTR s, INT size, CRTDLL_FILE *file );
INT __cdecl CRTDLL_fprintf( CRTDLL_FILE *file, LPSTR format, ... );
INT __cdecl CRTDLL_fputc( INT c, CRTDLL_FILE *file );
INT __cdecl CRTDLL_fputs( LPCSTR s, CRTDLL_FILE *file );
DWORD __cdecl CRTDLL_fread(LPVOID ptr, INT size, INT nmemb, CRTDLL_FILE *file);
VOID __cdecl CRTDLL_free(LPVOID ptr);
DWORD __cdecl CRTDLL_freopen(LPCSTR path, LPCSTR mode, LPVOID stream);
INT __cdecl CRTDLL_fscanf( CRTDLL_FILE *stream, LPSTR format, ... );
LONG __cdecl CRTDLL_fseek( CRTDLL_FILE *file, LONG offset, INT whence);
INT __cdecl CRTDLL_fsetpos( CRTDLL_FILE *file, INT *pos );
LONG __cdecl CRTDLL_ftell( CRTDLL_FILE *file );
DWORD __cdecl CRTDLL_fwrite( const LPVOID ptr, INT size, INT nmemb, CRTDLL_FILE *file );
INT __cdecl CRTDLL_getc( CRTDLL_FILE *file );
LPSTR __cdecl CRTDLL_getenv(const char *name);
LPSTR __cdecl CRTDLL_gets(LPSTR buf);
VOID __cdecl CRTDLL_longjmp(jmp_buf env, int val);
VOID* __cdecl CRTDLL_malloc(DWORD size);
WCHAR  __cdecl CRTDLL_mblen(CHAR *mb,INT size);
INT __cdecl CRTDLL_mbstowcs(LPWSTR wcs, LPCSTR mbs, INT size);
WCHAR __cdecl CRTDLL_mbtowc(WCHAR* wc,CHAR* mb,INT size) ;
VOID* __cdecl CRTDLL_new(DWORD size);
INT __cdecl CRTDLL_putc( INT c, CRTDLL_FILE *file );
void __cdecl CRTDLL_putchar( INT x );
INT __cdecl CRTDLL_puts(LPCSTR s);
INT __cdecl CRTDLL_rand();
VOID* __cdecl CRTDLL_realloc( VOID *ptr, DWORD size );
INT __cdecl CRTDLL_remove(LPCSTR file);
INT __cdecl CRTDLL_rename(LPCSTR oldpath,LPCSTR newpath);
new_handler_type __cdecl CRTDLL_set_new_handler(new_handler_type func);
INT __cdecl CRTDLL_setbuf(CRTDLL_FILE *file, LPSTR buf);
LPSTR __cdecl CRTDLL_setlocale(INT category,LPCSTR locale);
VOID __cdecl CRTDLL_signal(int sig, sig_handler_type ptr);
void __cdecl CRTDLL_srand(DWORD seed);
LPSTR __cdecl CRTDLL_strdup(LPCSTR ptr);
INT __cdecl CRTDLL_system(LPSTR x);
time_t __cdecl CRTDLL_time(time_t *timeptr);
LPSTR __cdecl CRTDLL_tmpnam(LPSTR s);
INT __cdecl CRTDLL_vfprintf( CRTDLL_FILE *file, LPSTR format, va_list args );
INT __cdecl CRTDLL_vsprintf( LPSTR buffer, LPCSTR spec, va_list args );
INT __cdecl CRTDLL_vswprintf( LPWSTR buffer, LPCWSTR spec, va_list args );
LPWSTR __cdecl CRTDLL_wcscat( LPWSTR s1, LPCWSTR s2 );
LPWSTR __cdecl CRTDLL_wcschr(LPCWSTR str,WCHAR xchar);
INT __cdecl CRTDLL_wcscmp( LPCWSTR s1, LPCWSTR s2 );
DWORD __cdecl CRTDLL_wcscoll(LPWSTR a1,LPWSTR a2);
LPWSTR __cdecl CRTDLL_wcscpy( LPWSTR s1, LPCWSTR s2 );
INT __cdecl CRTDLL_wcscspn(LPWSTR str,LPWSTR reject);
INT __cdecl CRTDLL_wcslen( LPCWSTR s );
LPWSTR __cdecl CRTDLL_wcsncat( LPWSTR s1, LPCWSTR s2, INT n );
INT __cdecl CRTDLL_wcsncmp( LPCWSTR s1, LPCWSTR s2, INT n );
LPWSTR __cdecl CRTDLL_wcsncpy( LPWSTR s1, LPCWSTR s2, INT n );
LPWSTR __cdecl CRTDLL_wcsrchr(LPWSTR str,WCHAR xchar);
INT __cdecl CRTDLL_wcsspn(LPWSTR str,LPWSTR accept);
LPWSTR __cdecl CRTDLL_wcsstr(LPWSTR s,LPWSTR b);
LPWSTR __cdecl CRTDLL_wcstok(LPWSTR s,LPCWSTR delim);
INT __cdecl CRTDLL_wcstol(LPWSTR s,LPWSTR *end,INT base);
INT __cdecl CRTDLL_wcstombs( LPSTR dst, LPCWSTR src, INT len );
CRTDLL_FILE * __cdecl CRTDLL__fdopen(INT handle, LPCSTR mode);
unsigned char* __cdecl CRTDLL__mbscat(unsigned char *x,unsigned char *y);
unsigned char* __cdecl CRTDLL__mbscpy(unsigned char *x,unsigned char *y);
CRTDLL_FILE * __cdecl CRTDLL_fopen(LPCSTR path, LPCSTR mode);

#if 0
#define FILE CRTDLL_FILE

extern CRTDLL_FILE *CRTDLL_stdin, *CRTDLL_stdout, *CRTDLL_stderr;

#ifdef stdin
#undef stdin
#define stdin CRTDLL_stdin
#endif
#ifdef stdout
#undef stdout
#define stdout CRTDLL_stdout
#endif
#ifdef stderr
#undef stderr
#define stderr CRTDLL_stderr
#endif

/*******************************************************************
 *  Mappings for ANSI(?) and basic SDK functions.  Some are implemented
 * in Wine; others are implemented by the host operating system
 */
#define _GetMainArgs CRTDLL__GetMainArgs
#define _abnormal_termination CRTDLL__abnormal_termination
#define _access CRTDLL__access
#define _cexit CRTDLL__cexit
#define _chdir CRTDLL__chdir
#define _chdrive CRTDLL__chdrive
#define _chmod CRTDLL__chmod
#define _close CRTDLL__close
#define _creat creat
#define _dllonexit  CRTDLL__dllonexit
#define _errno CRTDLL__errno
#define _fdopen CRTDLL__fdopen
#define _fileno CRTDLL__fileno
#define _findfirst CRTDLL__findfirst
#define _findnext CRTDLL__findnext
#define _fpreset CRTDLL__fpreset
#define _fstat CRTDLL__fstat
#define _ftol CRTDLL__ftol
#define _fullpath CRTDLL__fullpath
#define _get_osfhandle CRTDLL__get_osfhandle
#define _getcwd CRTDLL__getcwd
#define _getdcwd CRTDLL__getdcwd
#define _getdrive CRTDLL__getdrive
#define _global_unwind2 CRTDLL__global_unwind2
#define _initterm CRTDLL__initterm
#define _isatty CRTDLL__isatty
#define _isctype CRTDLL__isctype
LPSTR  __cdecl _itoa( int x, LPSTR buf, INT radix );
/* #define _itoa CRTDLL__itoa */
#define _local_unwind2 CRTDLL__local_unwind2
#define _lrotl CRTDLL__lrotl
#define _ltoa CRTDLL__ltoa
#define _makepath CRTDLL__makepath
#define _mbscat CRTDLL__mbscat
#define _mbscpy CRTDLL__mbscpy
#define _mbsicmp CRTDLL__mbsicmp
#define _mbsinc CRTDLL__mbsinc
#define _mbsrchr CRTDLL__mbsrchr
#define _memicmp CRTDLL__memicmp
#define _mkdir CRTDLL__mkdir
#define _open CRTDLL__open
#define _open_osfhandle CRTDLL__open_osfhandle
#define _popen popen
#define _read CRTDLL__read
#define _rmdir CRTDLL__rmdir
#define _rotl CRTDLL__rotl
#define _setmode CRTDLL__setmode
#define _sleep CRTDLL__sleep
#define _splitpath CRTDLL__splitpath
#define _stat stat
/* NOT for Winelib (use host OS's implementation) #define _stat CRTDLL__stat */
#define _strcmpi strcasecmp
/* NOT for Winelib (use host OS's implementation) #define _strcmpi CRTDLL__strcmpi */
#define _stricmp strcasecmp
/* FIXME: stricoll is not implemented but strcasecmp is probably close enough in most cases */
#define _stricoll strcasecmp
#define _strdate CRTDLL__strdate
#define _strdup CRTDLL__strdup
#define _strlwr CRTDLL__strlwr
#define _strnicmp strncasecmp
/* NOT for Winelib (use host OS's implementation) #define _strnicmp CRTDLL__strnicmp */
#define _strrev CRTDLL__strrev
#define _strtime CRTDLL__strtime
#define _strupr CRTDLL__strupr
#define _tempnam CRTDLL__tempnam
#define _ultoa CRTDLL__ultoa
#define _unlink CRTDLL__unlink
#define _utime utime
#define _vsnprintf vsnprintf
#define _wcsdup CRTDLL__wcsdup
#define _wcsicmp CRTDLL__wcsicmp
#define _wcsicoll CRTDLL__wcsicoll
#define _wcslwr CRTDLL__wcslwr
#define _wcsnicmp CRTDLL__wcsnicmp
#define _wcsrev CRTDLL__wcsrev
#define _wcsupr CRTDLL__wcsupr
#define _write CRTDLL__write
#define atexit CRTDLL_atexit
#define calloc CRTDLL_calloc
#define clock CRTDLL_clock
#define clearerr CRTDLL_clearerr
#define exit CRTDLL_exit
#define fclose CRTDLL_fclose
#define feof CRTDLL_feof
#define ferror CRTDLL_ferror
#define fflush CRTDLL_fflush
#define fgetc CRTDLL_fgetc
#define fgetchar getchar
#define fgets CRTDLL_fgets
#define fopen CRTDLL_fopen
#define fprintf CRTDLL_fprintf
#define fputc CRTDLL_fputc
#define fputchar putchar
#define fputs CRTDLL_fputs
#define fread CRTDLL_fread
#define free CRTDLL_free
#define freopen CRTDLL_freopen
#define fscanf CRTDLL_fscanf
#define fseek CRTDLL_fseek
#define fsetpos CRTDLL_fsetpos
#define ftell CRTDLL_ftell
#define fwrite CRTDLL_fwrite
#undef getc
#define getc CRTDLL_getc
#define getenv CRTDLL_getenv
#define gets CRTDLL_gets
#define itoa _itoa
#define longjmp CRTDLL_longjmp
#define malloc CRTDLL_malloc
#define mblen CRTDLL_mblen
#define mbstowcs CRTDLL_mbstowcs
#define mbtowc CRTDLL_mbtowc
/* Use HOST OS putc     #define putc CRTDLL_putc */
#ifdef putchar
	#undef putchar /* Gets rid of annoying warning */
#endif /* putchar */
#define putchar CRTDLL_putchar
#define puts CRTDLL_puts
#define rand CRTDLL_rand
#define realloc CRTDLL_realloc
#define remove CRTDLL_remove
#define rename CRTDLL_rename
#define setbuf CRTDLL_setbuf
#define setlocale CRTDLL_setlocale
#define signal CRTDLL_signal
#define sopen _sopen
/* FIXME: _sopen is not implemented */
#define srand CRTDLL_srand
#define strcmpi strcasecmp
#define strdup CRTDLL_strdup
#define stricoll _stricoll
/* FIXME: stricoll is not implemented but strcasecmp is probably close enough in most cases */
#define strlwr _strlwr
#define strnset _strnset
/* FIXME: _strnset is not implemented */
#define strnicmp _strnicmp
#define strset _strset
/* FIXME: _strset is not implemented */
#define strupr _strupr
#define system CRTDLL_system
#define time CRTDLL_time
#define tmpnam CRTDLL_tmpnam
#define ultoa _ultoa
#define vfprintf CRTDLL_vfprintf
#define vsprintf CRTDLL_vsprintf
#define vswprintf CRTDLL_vswprintf
#define wcscat CRTDLL_wcscat
#define wcschr CRTDLL_wcschr
#define wcscmp CRTDLL_wcscmp
#define wcscoll CRTDLL_wcscoll
#define wcscpy CRTDLL_wcscpy
#define wcscspn CRTDLL_wcscspn
#define wcslen CRTDLL_wcslen
#define wcsncat CRTDLL_wcsncat
#define wcsncmp CRTDLL_wcsncmp
#define wcsncpy CRTDLL_wcsncpy
#define wcsrchr CRTDLL_wcsrchr
#define wcsspn CRTDLL_wcsspn
#define wcsstr CRTDLL_wcsstr
#define wcstok CRTDLL_wcstok
#define wcstol CRTDLL_wcstol
#define wcstombs CRTDLL_wcstombs
#endif

#ifndef __cplusplus
/* the following defines conflict with C++ builtin operators and functions */
#define delete CRTDLL_delete
#define new CRTDLL_new
#define set_new_handler CRTDLL_set_new_handler
#endif

#endif /* __WINE__ */

#ifdef __cplusplus
}
#endif

#endif /* __WINELIB_CRTDLL_H */
