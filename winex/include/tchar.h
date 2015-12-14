#ifndef __WINE_TCHAR_H
#define __WINE_TCHAR_H

#ifdef __WINE__
#error Wine should not include tchar.h internally
#endif

#include "windef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _ftcscat    _tcscat
#define _ftcschr    _tcschr
#define _ftcscpy    _tcscpy
#define _ftcscspn   _tcscspn
#define _ftcslen    _tcslen
#define _ftcsncat   _tcsncat
#define _ftcsncpy   _tcsncpy
#define _ftcspbrk   _tcspbrk
#define _ftcsrchr   _tcsrchr
#define _ftcsspn    _tcsspn
#define _ftcsstr    _tcsstr
#define _ftcstok    _tcstok

#define _ftcsdup    _tcsdup
#define _ftcsnset   _tcsnset
#define _ftcsrev    _tcsrev
#define _ftcsset    _tcsset

#define _ftcscmp      _tcscmp
#define _ftcsicmp     _tcsicmp
#define _ftcsnccmp    _tcsnccmp
#define _ftcsncmp     _tcsncmp
#define _ftcsncicmp   _tcsncicmp
#define _ftcsnicmp    _tcsnicmp

#define _ftcscoll     _tcscoll
#define _ftcsicoll    _tcsicoll
#define _ftcsnccoll   _tcsnccoll
#define _ftcsncoll    _tcsncoll
#define _ftcsncicoll  _tcsncicoll
#define _ftcsnicoll   _tcsnicoll


#define _ftcsclen   _tcsclen
#define _ftcsnccat  _tcsnccat
#define _ftcsnccpy  _tcsnccpy
#define _ftcsncset  _tcsncset

#define _ftcsdec    _tcsdec
#define _ftcsinc    _tcsinc
#define _ftcsnbcnt  _tcsnbcnt
#define _ftcsnccnt  _tcsnccnt
#define _ftcsnextc  _tcsnextc
#define _ftcsninc   _tcsninc
#define _ftcsspnp   _tcsspnp

#define _ftcslwr    _tcslwr
#define _ftcsupr    _tcsupr

#define _ftclen     _tclen
#define _ftccpy     _tccpy
#define _ftccmp     _tccmp

/*****************************************************************************
 * tchar routines
 */
#define _strdec(start,current)  ((start)<(current) ? ((char*)(current))-1 : NULL)
#define _strinc(current)        (((char*)(current))+1)
#define _strncnt(str,max)       (strlen(str)>(max) ? (max) : strlen(str))
#define _strnextc(str)          ((unsigned int)*(str))
#define _strninc(str,n)         (((char*)(str))+(n))
#define _strspnp(s1,s2)         (*((s1)+=strspn((s1),(s2))) ? (s1) : NULL)


/*****************************************************************************
 * tchar mappings
 */
#ifndef _UNICODE
#  ifndef _MBCS
#    include "string.h"
#    define WINE_choose_tchar(std,mbcs,unicode) std
#  else /* _MBCS defined */
#    include "mbstring.h"
#    define WINE_choose_tchar(std,mbcs,unicode) mbcs
#  endif
#else /* _UNICODE defined */
#  include "wchar.h"
#  define WINE_choose_tchar(std,mbcs,unicode) unicode

#  ifndef _WCTYPE_T_DEFINED
     typedef wchar_t wint_t;
     typedef wchar_t wctype_t;
#    define _WCTYPE_T_DEFINED
#  endif /* _WCTYPE_T_DEFINED */

#endif


#if defined( _MBCS ) && !defined( _MB_MAP_DIRECT )
/* Life is compilcated by the MBCS as it requires special casting wackiness.
 * Use this macro if the SBCS and MBCS routine are exactly the same */
#define WINE_tchar_routine_simple(std,mbcs,unicode) THIS_SHOULD_NEVER_HAPPEN_AND_THE_COMPILE_SHOULD_FAIL
#else
#define WINE_tchar_routine_simple(std,mbcs,unicode) WINE_choose_tchar(std,mbcs,unicode)
#endif
#define WINE_tchar_routine_similar(std_mbcs,unicode) WINE_choose_tchar(std_mbcs,std_mbcs,unicode)
#define WINE_tchar_typedef(std,mbcs,unicode,new_def) typedef WINE_choose_tchar(std,mbcs,unicode) new_def

#define WINE_tchar_true(a) (1)
#define WINE_tchar_false(a) (0)
#define WINE_tchar_tclen(a) (1)
#define WINE_tchar_tccpy(a,b) do { *(a)=*(b); } while (0)

#if !defined( _MBCS ) || (defined( _MBCS ) && defined( _MB_MAP_DIRECT ))
#define _istalnum     WINE_tchar_routine_simple(isalnum,         _ismbcalnum, iswalnum)
#define _istalpha     WINE_tchar_routine_simple(isalpha,         _ismbcalpha, iswalpha)
#define _istdigit     WINE_tchar_routine_simple(isdigit,         _ismbcdigit, iswdigit)
#define _istgraph     WINE_tchar_routine_simple(isgraph,         _ismbcgraph, iswgraph)
#define _istlead      WINE_tchar_routine_simple(WINE_tchar_false,_ismbblead,  WINE_tchar_false)
#define _istleadbyte  WINE_tchar_routine_simple(WINE_tchar_false,isleadbyte,  WINE_tchar_false)
#define _istlegal     WINE_tchar_routine_simple(WINE_tchar_true, _ismbclegal, WINE_tchar_true)
#define _istlower     WINE_tchar_routine_simple(islower,         _ismbclower, iswlower)
#define _istprint     WINE_tchar_routine_simple(isprint,         _ismbcprint, iswprint)
#define _istpunct     WINE_tchar_routine_simple(ispunct,         _ismbcpunct, iswpunct)
#define _istspace     WINE_tchar_routine_simple(isspace,         _ismbcspace, iswspace)
#define _istupper     WINE_tchar_routine_simple(isupper,         _ismbcupper, iswupper)
#define _tccpy        WINE_tchar_routine_simple(WINE_tchar_tccpy,_mbccpy,     WINE_tchar_tccpy)
#define _tclen        WINE_tchar_routine_simple(WINE_tchar_tclen,_mbclen,     WINE_tchar_tclen)
#define _tcschr       WINE_tchar_routine_simple(strchr,          _mbschr,     wcschr)
#define _tcsclen      WINE_tchar_routine_simple(strlen,          _mbslen,     wcslen)
#define _tcscmp       WINE_tchar_routine_simple(strcmp,          _mbscmp,     wcscmp)
#define _tcscoll      WINE_tchar_routine_simple(strcoll,         _mbscoll,    wcscoll)
#define _tcscspn      WINE_tchar_routine_simple(strcspn,         _mbscspn,    wcscspn)
#define _tcsdec       WINE_tchar_routine_simple(_strdec,         _mbsdec,     _wcsdec)
#define _tcsdup       WINE_tchar_routine_simple(_strdup,         _mbsdup,     _wcsdup)
#define _tcsicmp      WINE_tchar_routine_simple(_stricmp,        _mbsicmp,    _wcsicmp)
#define _tcsicoll     WINE_tchar_routine_simple(_stricoll,       _mbsicoll,   _wcsicoll)
#define _tcsinc       WINE_tchar_routine_simple(_strinc,         _mbsinc,     _wcsinc)
#define _tcslwr       WINE_tchar_routine_simple(_strlwr,         _mbslwr,     _wcslwr)
#define _tcsnbcnt     WINE_tchar_routine_simple(_strncnt,        _mbsnbcnt,   _wcnscnt)
#define _tcsncat      WINE_tchar_routine_simple(strncat,         _mbsnbcat,   wcsncat)
#define _tcsnccat     WINE_tchar_routine_simple(strncat,         _mbsncat,    wcsncat)
#define _tcsnccoll    WINE_tchar_routine_simple(_strncoll,       _mbsncoll,   _wcsncoll)
#define _tcsncmp      WINE_tchar_routine_simple(strncmp,         _mbsnbcmp,   wcsncmp)
#define _tcsncoll     WINE_tchar_routine_simple(_strncoll,       _mbsnbcoll,  _wcsncoll)
#define _tcsnccmp     WINE_tchar_routine_simple(strncmp,         _mbsncmp,    wcsncmp)
#define _tcsnccnt     WINE_tchar_routine_simple(_strncnt,        _mbsnccnt,   _wcsncnt)
#define _tcsnccpy     WINE_tchar_routine_simple(strncpy,         _mbsncpy,    wcsncpy)
#define _tcsncicmp    WINE_tchar_routine_simple(_strnicmp,       _mbsnicmp,   _wcsnicmp)
#define _tcsncicoll   WINE_tchar_routine_simple(_strnicoll,      _mbsnicoll,  _wcsnicoll)
#define _tcsncpy      WINE_tchar_routine_simple(strncpy,         _mbsnbcpy,   wcsncpy)
#define _tcsncset     WINE_tchar_routine_simple(_strnset,        _mbsnset,    _wcsnset)
#define _tcsnextc     WINE_tchar_routine_simple(_strnextc,       _mbsnextc,   _wcsnextc)
#define _tcsnicmp     WINE_tchar_routine_simple(_strnicmp,       _mbsnicmp,   _wcsnicmp)
#define _tcsnicoll    WINE_tchar_routine_simple(_strnicoll,      _mbsnbicoll, _wcsnicoll)
#define _tcsninc      WINE_tchar_routine_simple(_strninc,        _mbsninc,    _wcsninc)
#define _tcsnccnt     WINE_tchar_routine_simple(_strncnt,        _mbsnccnt,   _wcsncnt)
#define _tcsnset      WINE_tchar_routine_simple(_strnset,        _mbsnbset,   _wcsnset)
#define _tcspbrk      WINE_tchar_routine_simple(strpbrk,         _mbspbrk,    wcspbrk)
#define _tcsspnp      WINE_tchar_routine_simple(_strspnp,        _mbsspnp,    _wcsspnp)
#define _tcsrchr      WINE_tchar_routine_simple(strrchr,         _mbsrchr,    wcsrchr)
#define _tcsrev       WINE_tchar_routine_simple(_strrev,         _mbsrev,     _wcsrev)
#define _tcsset       WINE_tchar_routine_simple(_strset,         _mbsset,     _wcsset)
#define _tcsspn       WINE_tchar_routine_simple(strspn,          _mbsspn,     wcsspn)
#define _tcsstr       WINE_tchar_routine_simple(strstr,          _mbsstr,     wcsstr)
#define _tcstok       WINE_tchar_routine_simple(strtok,          _mbstok,     wcstok)
#define _tcsupr       WINE_tchar_routine_simple(_strupr,         _mbsupr,     _wcsupr)
#define _topen        WINE_tchar_routine_simple(_open, _wopen)
#define _totlower     WINE_tchar_routine_simple(tolower, towlower)
#define _totupper     WINE_tchar_routine_simple(toupper, towupper)
#endif /* #if defined( _MBCS ) && !defined( _MB_MAP_DIRECT ) */

#define __targv       WINE_tchar_routine_similar(__argv, __wargv)
#define _fgettc       WINE_tchar_routine_similar(fgetc, fgetwc)
#define _fgettchar    WINE_tchar_routine_similar(fgetchar, _fgetwchar)
#define _fgetts       WINE_tchar_routine_similar(fgets, fgetws)
#define _fputtc       WINE_tchar_routine_similar(fputc, fputwc)
#define _fputtchar    WINE_tchar_routine_similar(fputchar, _fputwchar)
#define _fputts       WINE_tchar_routine_similar(fputs, fputws)
#define _ftprintf     WINE_tchar_routine_similar(fprintf, fwprintf)
#define _ftscanf      WINE_tchar_routine_similar(fscanf, fwscanf)
#define _gettc        WINE_tchar_routine_similar(getc, getwc)
#define _gettchar     WINE_tchar_routine_similar(getchar, getwchar)
#define _getts        WINE_tchar_routine_similar(gets, getws)
#define _istascii     WINE_tchar_routine_similar(__isascii, iswascii)
#define _istcntrl     WINE_tchar_routine_similar(iscntrl, iswcntrl)
#define _istxdigit    WINE_tchar_routine_similar(isxdigit, iswxdigit)
#define _itot         WINE_tchar_routine_similar(_itoa, _itow)
#define _ltot         WINE_tchar_routine_similar(_ltoa, _ltow)
#define _puttc        WINE_tchar_routine_similar(putc, putwc)
#define _puttchar     WINE_tchar_routine_similar(putchar, putwchar)
#define _putts        WINE_tchar_routine_similar(puts, putws)
#define _sntprintf    WINE_tchar_routine_similar(_snprintf, _snwprintf)
#define _stprintf     WINE_tchar_routine_similar(sprintf, swprintf)
#define _stscanf      WINE_tchar_routine_similar(sscanf, swscanf)
#define _taccess      WINE_tchar_routine_similar(_access, _waccess)
#define _tasctime     WINE_tchar_routine_similar(asctime, _wasctime)
#define _tchdir       WINE_tchar_routine_similar(_chdir, _wchdir)
#define _tchmod       WINE_tchar_routine_similar(_chmod, _wchmod)
#define _tcreat       WINE_tchar_routine_similar(_creat, _wcreat)
#define _tcscat       WINE_tchar_routine_similar(strcat, wcscat)
#define _tcsftime     WINE_tchar_routine_similar(strftime, wcsftime)
#define _tcslen       WINE_tchar_routine_similar(strlen, wcslen)
#define _tcstod       WINE_tchar_routine_similar(strtod, wcstod)
#define _tcstol       WINE_tchar_routine_similar(strtol, wcstol)
#define _tcstoul      WINE_tchar_routine_similar(strtoul, wcstoul)
#define _tcsxfrm      WINE_tchar_routine_similar(strxfrm, wcsxfrm)
#define _tctime       WINE_tchar_routine_similar(ctime, _wctime)
#define _tenviron     WINE_tchar_routine_similar(_environ, _wenviron)
#define _texecl       WINE_tchar_routine_similar(_execl, _wexecl)
#define _texecle      WINE_tchar_routine_similar(_execle, _wexecle)
#define _texeclp      WINE_tchar_routine_similar(_execlp, _wexeclp)
#define _texeclpe     WINE_tchar_routine_similar(_execlpe, _wexeclpe)
#define _texecv       WINE_tchar_routine_similar(_execv, _wexecv)
#define _texecve      WINE_tchar_routine_similar(_execve, _wexecve)
#define _texecvp      WINE_tchar_routine_similar(_execvp, _wexecvp)
#define _texecvpe     WINE_tchar_routine_similar(_execvpe, _wexecvpe)
#define _tfdopen      WINE_tchar_routine_similar(_fdopen, _wfdopen)
#define _tfinddata_t  WINE_tchar_routine_similar(_finddata_t, _wfinddata_t)
#define _tfinddatai64_t WINE_tchar_routine_similar(_finddatai64_t, _wfinddatai64_t)
#define _tfindfirst   WINE_tchar_routine_similar(_findfirst, _wfindfirst)
#define _tfindnext    WINE_tchar_routine_similar(_findnext, _wfindnext)
#define _tfopen       WINE_tchar_routine_similar(fopen, _wfopen)
#define _tfreopen     WINE_tchar_routine_similar(freopen, _wfreopen)
#define _tfsopen      WINE_tchar_routine_similar(_fsopen, _wfsopen)
#define _tfullpath    WINE_tchar_routine_similar(_fullpath, _wfullpath)
#define _tgetcwd      WINE_tchar_routine_similar(_getcwd, _wgetcwd)
#define _tgetdcwd     WINE_tchar_routine_similar(_getdcwd, _wgetdcwd)
#define _tgetenv      WINE_tchar_routine_similar(getenv, _wgetenv)
#define _tmain        WINE_tchar_routine_similar(main, wmain)
#define _tmakepath    WINE_tchar_routine_similar(_makepath, _wmakepath)
#define _tmkdir       WINE_tchar_routine_similar(_mkdir, _wmkdir)
#define _tmktemp      WINE_tchar_routine_similar(_mktemp, _wmktemp)
#define _tperror      WINE_tchar_routine_similar(perror, _wperror)
#define _tpopen       WINE_tchar_routine_similar(_popen, _wpopen)
#define _tprintf      WINE_tchar_routine_similar(printf, wprintf)
#define _tputenv      WINE_tchar_routine_similar(_putenv, _wputenv)
#define _tremove      WINE_tchar_routine_similar(remove, _wremove)
#define _trename      WINE_tchar_routine_similar(rename, _wrename)
#define _trmdir       WINE_tchar_routine_similar(_rmdir, _wrmdir)
#define _tsearchenv   WINE_tchar_routine_similar(_searchenv, _wsearchenv)
#define _tscanf       WINE_tchar_routine_similar(scanf, wscanf)
#define _tsetlocale   WINE_tchar_routine_similar(setlocale, _wsetlocale)
#define _tsopen       WINE_tchar_routine_similar(_sopen, _wsopen)
#define _tspawnl      WINE_tchar_routine_similar(_spawnl, _wspawnl)
#define _tspawnle     WINE_tchar_routine_similar(_spawnle, _wspawnle)
#define _tspawnlp     WINE_tchar_routine_similar(_spawnlp, _wspawnlp)
#define _tspawnlpe    WINE_tchar_routine_similar(_spawnlpe, _wspawnlpe)
#define _tspawnv      WINE_tchar_routine_similar(_spawnv, _wspawnv)
#define _tspawnve     WINE_tchar_routine_similar(_spawnve, _wspawnve)
#define _tspawnvp     WINE_tchar_routine_similar(_spawnvp, _tspawnvp)
#define _tspawnvpe    WINE_tchar_routine_similar(_spawnvpe, _tspawnvpe)
#define _tsplitpath   WINE_tchar_routine_similar(_splitpath, _wsplitpath)
#define _tstat        WINE_tchar_routine_similar(_stat, _wstat)
#define _tcscpy       WINE_tchar_routine_similar(strcpy, wcscpy)
#define _tstrdate     WINE_tchar_routine_similar(_strdate, _wstrdate)
#define _tstrtime     WINE_tchar_routine_similar(_strtime, _wstrtime)
#define _tsystem      WINE_tchar_routine_similar(system, _wsystem)
#define _ttempnam     WINE_tchar_routine_similar(_tempnam, _wtempnam)
#define _ttmpnam      WINE_tchar_routine_similar(tmpnam, _wtmpnam)
#define _ttoi         WINE_tchar_routine_similar(atoi, _wtoi)
#define _ttol         WINE_tchar_routine_similar(atol, _wtol)
#define _tunlink      WINE_tchar_routine_similare(_unlink, _wunlink)
#define _tutime       WINE_tchar_routine_similar(_utime, _wutime)
#define _tWinMain     WINE_tchar_routine_similar(WinMain, wWinMain)
#define _ultot        WINE_tchar_routine_similar(_ultoa, _ultow)
#define _ungettc      WINE_tchar_routine_similar(ungetc, ungetwc)
#define _vftprintf    WINE_tchar_routine_similar(vfprintf, vfwprintf)
#define _vsntprintf   WINE_tchar_routine_similar(_vsnprintf, _vsnwprintf)
#define _vstprintf    WINE_tchar_routine_similar(vsprintf, vswprintf)
#define _vtprintf     WINE_tchar_routine_similar(vprintf, vwprintf)

#if defined( _MBCS ) && !defined( _MB_MAP_DIRECT )
#  if defined (_NO_INLINING)
#    error "type safe link function thunks not available (or are they?)"
#  else /* __STDC__ || defined (_NO_INLINING) */

/* Use type safe inline versions of the functions */
#define _PUC    unsigned char *
#define _CPUC   const unsigned char *
#define _PC     char *
#define _CPC    const char *
#define _UI     unsigned int

static inline _PC _tcschr(_CPC _s1,_UI _c) {return (_PC)_mbschr((_CPUC)_s1,_c);}
static inline size_t _tcscspn(_CPC _s1,_CPC _s2) {return _mbscspn((_CPUC)_s1,(_CPUC)_s2);}
static inline _PC _tcsncat(_PC _s1,_CPC _s2,size_t _n) {return (_PC)_mbsnbcat((_PUC)_s1,(_CPUC)_s2,_n);}
static inline _PC _tcsncpy(_PC _s1,_CPC _s2,size_t _n) {return (_PC)_mbsnbcpy((_PUC)_s1,(_CPUC)_s2,_n);}
static inline _PC _tcspbrk(_CPC _s1,_CPC _s2) {return (_PC)_mbspbrk((_CPUC)_s1,(_CPUC)_s2);}
static inline _PC _tcsrchr(_CPC _s1,_UI _c) {return (_PC)_mbsrchr((_CPUC)_s1,_c);}
static inline size_t _tcsspn(_CPC _s1,_CPC _s2) {return _mbsspn((_CPUC)_s1,(_CPUC)_s2);}
static inline _PC _tcsstr(_CPC _s1,_CPC _s2) {return (_PC)_mbsstr((_CPUC)_s1,(_CPUC)_s2);}
static inline _PC _tcstok(_PC _s1,_CPC _s2) {return (_PC)_mbstok((_PUC)_s1,(_CPUC)_s2);}
static inline _PC _tcsnset(_PC _s1,_UI _c,size_t _n) {return (_PC)_mbsnbset((_PUC)_s1,_c,_n);}
static inline _PC _tcsrev(_PC _s1) {return (_PC)_mbsrev((_PUC)_s1);}
static inline _PC _tcsset(_PC _s1,_UI _c) {return (_PC)_mbsset((_PUC)_s1,_c);}
static inline int _tcscmp(_CPC _s1,_CPC _s2) {return _mbscmp((_CPUC)_s1,(_CPUC)_s2);}
static inline int _tcsicmp(_CPC _s1,_CPC _s2) {return _mbsicmp((_CPUC)_s1,(_CPUC)_s2);}
static inline int _tcsnccmp(_CPC _s1,_CPC _s2,size_t _n) {return _mbsncmp((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcsncmp(_CPC _s1,_CPC _s2,size_t _n) {return _mbsnbcmp((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcsncicmp(_CPC _s1,_CPC _s2,size_t _n) {return _mbsnicmp((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcsnicmp(_CPC _s1,_CPC _s2,size_t _n) {return _mbsnbicmp((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcscoll(_CPC _s1,_CPC _s2) {return _mbscoll((_CPUC)_s1,(_CPUC)_s2);}
static inline int _tcsicoll(_CPC _s1,_CPC _s2) {return _mbsicoll((_CPUC)_s1,(_CPUC)_s2);}
static inline int _tcsnccoll(_CPC _s1,_CPC _s2,size_t _n) {return _mbsncoll((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcsncoll(_CPC _s1,_CPC _s2,size_t _n) {return _mbsnbcoll((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcsncicoll(_CPC _s1,_CPC _s2,size_t _n) {return _mbsnicoll((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline int _tcsnicoll(_CPC _s1,_CPC _s2,size_t _n) {return _mbsnbicoll((_CPUC)_s1,(_CPUC)_s2,_n);}
static inline size_t _tcsclen(_CPC _s1) {return _mbslen((_CPUC)_s1);}
static inline _PC _tcsnccat(_PC _s1,_CPC _s2,size_t _n) {return (_PC)_mbsncat((_PUC)_s1,(_CPUC)_s2,_n);}
static inline _PC _tcsnccpy(_PC _s1,_CPC _s2,size_t _n) {return (_PC)_mbsncpy((_PUC)_s1,(_CPUC)_s2,_n);}
static inline _PC _tcsncset(_PC _s1,_UI _c,size_t _n) {return (_PC)_mbsnset((_PUC)_s1,_c,_n);}
static inline _PC _tcsdec(_CPC _s1,_CPC _s2) {return (_PC)_mbsdec((_CPUC)_s1,(_CPUC)_s2);}
static inline _PC _tcsinc(_CPC _s1) {return (_PC)_mbsinc((_CPUC)_s1);}
static inline size_t _tcsnbcnt(_CPC _s1,size_t _n) {return _mbsnbcnt((_CPUC)_s1,_n);}
static inline size_t _tcsnccnt(_CPC _s1,size_t _n) {return _mbsnccnt((_CPUC)_s1,_n);}
static inline _PC _tcsninc(_CPC _s1,size_t _n) {return (_PC)_mbsninc((_CPUC)_s1,_n);}
static inline _PC _tcsspnp(_CPC _s1,_CPC _s2) {return (_PC)_mbsspnp((_CPUC)_s1,(_CPUC)_s2);}
static inline _PC _tcslwr(_PC _s1) {return (_PC)_mbslwr((_PUC)_s1);}
static inline _PC _tcsupr(_PC _s1) {return (_PC)_mbsupr((_PUC)_s1);}
static inline size_t _tclen(_CPC _s1) {return _mbclen((_CPUC)_s1);}
static inline void _tccpy(_PC _s1,_CPC _s2) {_mbccpy((_PUC)_s1,(_CPUC)_s2); return;}
static inline _UI _tcsnextc(_CPC _s1) {_UI _n=0; if (_ismbblead((_UI)*(_PUC)_s1)) _n=((_UI)*_s1++)<<8; _n+=(_UI)*_s1; return(_n);}

#  endif /* __STDC__ || defined (_NO_INLINING) */
#endif /* defined( _MBCS ) && defined( _MB_MAP_DIRECT ) */

#define __T(x) __TEXT(x)
#define _T(x) __T(x)
#define _TEXT(x) __T(x)


#ifndef __TCHAR_DEFINED
  WINE_tchar_typedef( char, char, WCHAR, _TCHAR );
  WINE_tchar_typedef( signed char, signed char, WCHAR, _TSCHAR );
  WINE_tchar_typedef( unsigned char, unsigned char, WCHAR, _TUCHAR );
  WINE_tchar_typedef( char, unsigned char, WCHAR, _TXCHAR );
  WINE_tchar_typedef( int, unsigned int, wint_t,  _TINT );
#define __TCHAR_DEFINED
#endif /* __TCHAR_DEFINED */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __WINE_TCHAR_H */
