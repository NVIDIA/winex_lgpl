/*
 * Misc. functions for systems that don't have them
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"
#include "wine/port.h"

#ifdef __BEOS__
#include <be/kernel/fs_info.h>
#include <be/kernel/OS.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <signal.h>
#include <errno.h>
#ifdef HAVE_SYS_TIME_h
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#ifdef HAVE_LIBIO_H
# include <libio.h>
#endif
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef __APPLE__
#include <mach/mach_init.h>
#endif

/***********************************************************************
 *		usleep
 */
#ifndef HAVE_USLEEP
unsigned int usleep (unsigned int useconds)
{
#if defined(__EMX__)
    DosSleep(useconds);
    return 0;
#elif defined(__BEOS__)
    return snooze(useconds);
#elif defined(HAVE_SELECT)
    struct timeval delay;

    delay.tv_sec = useconds / 1000000;
    delay.tv_usec = useconds % 1000000;

    select( 0, 0, 0, 0, &delay );
    return 0;
#else /* defined(__EMX__) || defined(__BEOS__) || defined(HAVE_SELECT) */
    errno = ENOSYS;
    return -1;
#endif /* defined(__EMX__) || defined(__BEOS__) || defined(HAVE_SELECT) */
}
#endif /* HAVE_USLEEP */

/***********************************************************************
 *		memmove
 */
#ifndef HAVE_MEMMOVE
void *memmove( void *dest, const void *src, unsigned int len )
{
    register char *dst = dest;

    /* Use memcpy if not overlapping */
    if ((dst + len <= (char *)src) || ((char *)src + len <= dst))
    {
        memcpy( dst, src, len );
    }
    /* Otherwise do it the hard way (FIXME: could do better than this) */
    else if (dst < src)
    {
        while (len--) *dst++ = *((char *)src)++;
    }
    else
    {
        dst += len - 1;
        src = (char *)src + len - 1;
        while (len--) *dst-- = *((char *)src)--;
    }
    return dest;
}
#endif  /* HAVE_MEMMOVE */

/***********************************************************************
 *		strerror
 */
#ifndef HAVE_STRERROR
const char *strerror( int err )
{
    /* Let's hope we have sys_errlist then */
    return sys_errlist[err];
}
#endif  /* HAVE_STRERROR */


/***********************************************************************
 *		getpagesize
 */
#ifndef HAVE_GETPAGESIZE
size_t getpagesize(void)
{
# ifdef __svr4__
    return sysconf(_SC_PAGESIZE);
# else
#  error Cannot get the page size on this platform
# endif
}
#endif  /* HAVE_GETPAGESIZE */


/***********************************************************************
 *		clone
 */
#if !defined(HAVE_CLONE) && defined(__linux__)
int clone( int (*fn)(void *), void *stack, int flags, void *arg )
{
#ifdef __i386__
    int ret;
    void **stack_ptr = (void **)stack;
    *--stack_ptr = arg;  /* Push argument on stack */
    *--stack_ptr = fn;   /* Push function pointer (popped into ebx) */
    __asm__ __volatile__( "pushl %%ebx\n\t"
                          "movl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "popl %%ebx\n\t"   /* Contains fn in the child */
                          "testl %%eax,%%eax\n\t"
                          "jnz 0f\n\t"
                          "xorl %ebp,%ebp\n\t"    /* Terminate the stack frames */
                          "call *%%ebx\n\t"       /* Should never return */
                          "xorl %%eax,%%eax\n\t"  /* Just in case it does*/
                          "0:"
                          : "=a" (ret)
                          : "0" (SYS_clone), "r" (flags), "c" (stack_ptr) );
    assert( ret );  /* If ret is 0, we returned from the child function */
    if (ret > 0) return ret;
    errno = -ret;
    return -1;
#else
    errno = EINVAL;
    return -1;
#endif  /* __i386__ */
}
#endif  /* !HAVE_CLONE && __linux__ */

/***********************************************************************
 *		strcasecmp
 */
#ifndef HAVE_STRCASECMP
int strcasecmp( const char *str1, const char *str2 )
{
    const unsigned char *ustr1 = (const unsigned char *)str1;
    const unsigned char *ustr2 = (const unsigned char *)str2;

    while (*ustr1 && toupper(*ustr1) == toupper(*ustr2)) {
        ustr1++;
	ustr2++;
    }
    return toupper(*ustr1) - toupper(*ustr2);
}
#endif /* HAVE_STRCASECMP */

/***********************************************************************
 *		strncasecmp
 */
#ifndef HAVE_STRNCASECMP
int strncasecmp( const char *str1, const char *str2, size_t n )
{
    const unsigned char *ustr1 = (const unsigned char *)str1;
    const unsigned char *ustr2 = (const unsigned char *)str2;
    int res;

    if (!n) return 0;
    while ((--n > 0) && *ustr1) {
	if ((res = toupper(*ustr1) - toupper(*ustr2))) return res;
	ustr1++;
	ustr2++;
    }
    return toupper(*ustr1) - toupper(*ustr2);
}
#endif /* HAVE_STRNCASECMP */

/***********************************************************************
 *		openpty
 * NOTE
 *   It looks like the openpty that comes with glibc in RedHat 5.0
 *   is buggy (second call returns what looks like a dup of 0 and 1
 *   instead of a new pty), this is a generic replacement.
 *
 * FIXME
 *   We should have a autoconf check for this.
 */
#ifndef HAVE_OPENPTY
int openpty(int *master, int *slave, char *name, struct termios *term, struct winsize *winsize)
{
    const char *ptr1, *ptr2;
    char pts_name[512];

    strcpy (pts_name, "/dev/ptyXY");

    for (ptr1 = "pqrstuvwxyzPQRST"; *ptr1 != 0; ptr1++) {
        pts_name[8] = *ptr1;
        for (ptr2 = "0123456789abcdef"; *ptr2 != 0; ptr2++) {
            pts_name[9] = *ptr2;

            if ((*master = open(pts_name, O_RDWR)) < 0) {
                if (errno == ENOENT)
                    return -1;
                else
                    continue;
            }
            pts_name[5] = 't';
            if ((*slave = open(pts_name, O_RDWR)) < 0) {
                pts_name[5] = 'p';
		close (*master);
                continue;
            }

            if (term != NULL)
                tcsetattr(*slave, TCSANOW, term);
            if (winsize != NULL)
                ioctl(*slave, TIOCSWINSZ, winsize);
            if (name != NULL)
                strcpy(name, pts_name);
            return *slave;
        }
    }
    errno = EMFILE;
    return -1;
}
#endif  /* HAVE_OPENPTY */

/***********************************************************************
 *		getnetbyaddr
 */
#ifndef HAVE_GETNETBYADDR
struct netent *getnetbyaddr(unsigned long net, int type)
{
    errno = ENOSYS;
    return NULL;
}
#endif /* defined(HAVE_GETNETBYNAME) */

/***********************************************************************
 *		getnetbyname
 */
#ifndef HAVE_GETNETBYNAME
struct netent *getnetbyname(const char *name)
{
    errno = ENOSYS;
    return NULL;
}
#endif /* defined(HAVE_GETNETBYNAME) */

/***********************************************************************
 *		getprotobyname
 */
#ifndef HAVE_GETPROTOBYNAME
struct protoent *getprotobyname(const char *name)
{
    errno = ENOSYS;
    return NULL;
}
#endif /* !defined(HAVE_GETPROTOBYNAME) */

/***********************************************************************
 *		getprotobynumber
 */
#ifndef HAVE_GETPROTOBYNUMBER
struct protoent *getprotobynumber(int proto)
{
    errno = ENOSYS;
    return NULL;
}
#endif /* !defined(HAVE_GETPROTOBYNUMBER) */

/***********************************************************************
 *		getservbyport
 */
#ifndef HAVE_GETSERVBYPORT
struct servent *getservbyport(int port, const char *proto)
{
    errno = ENOSYS;
    return NULL;
}
#endif /* !defined(HAVE_GETSERVBYPORT) */

/***********************************************************************
 *		getsockopt
 */
#ifndef HAVE_GETSOCKOPT
int getsockopt(int socket, int level, int option_name,
               void *option_value, size_t *option_len)
{
    errno = ENOSYS;
    return -1;
}
#endif /* !defined(HAVE_GETSOCKOPT) */

/***********************************************************************
 *		inet_network
 */
#ifndef HAVE_INET_NETWORK
unsigned long inet_network(const char *cp)
{
    errno = ENOSYS;
    return 0;
}
#endif /* defined(HAVE_INET_NETWORK) */

/***********************************************************************
 *		statfs
 */
#ifndef HAVE_STATFS
int statfs(const char *name, struct statfs *info)
{
#ifdef __BEOS__
    dev_t mydev;
    fs_info fsinfo;

    if(!info) {
        errno = ENOSYS;
        return -1;
    }

    if ((mydev = dev_for_path(name)) < 0) {
        errno = ENOSYS;
        return -1;
    }

    if (fs_stat_dev(mydev,&fsinfo) < 0) {
        errno = ENOSYS;
        return -1;
    }

    info->f_bsize = fsinfo.block_size;
    info->f_blocks = fsinfo.total_blocks;
    info->f_bfree = fsinfo.free_blocks;
    return 0;
#else /* defined(__BEOS__) */
    errno = ENOSYS;
    return -1;
#endif /* defined(__BEOS__) */
}
#endif /* !defined(HAVE_STATFS) */


/***********************************************************************
 *		lstat
 */
#ifndef HAVE_LSTAT
int lstat(const char *file_name, struct stat *buf)
{
    return stat( file_name, buf );
}
#endif /* HAVE_LSTAT */


/***********************************************************************
 *		pread
 *
 * FIXME: this is not thread-safe
 */
#ifndef HAVE_PREAD
ssize_t pread( int fd, void *buf, size_t count, off_t offset )
{
    ssize_t ret;
    off_t old_pos;

    if ((old_pos = lseek( fd, 0, SEEK_CUR )) == -1) return -1;
    if (lseek( fd, offset, SEEK_SET ) == -1) return -1;
    if ((ret = read( fd, buf, count )) == -1)
    {
        int err = errno;  /* save errno */
        lseek( fd, old_pos, SEEK_SET );
        errno = err;
        return -1;
    }
    if (lseek( fd, old_pos, SEEK_SET ) == -1) return -1;
    return ret;
}
#endif /* HAVE_PREAD */


/***********************************************************************
 *		pwrite
 *
 * FIXME: this is not thread-safe
 */
#ifndef HAVE_PWRITE
ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset )
{
    ssize_t ret;
    off_t old_pos;

    if ((old_pos = lseek( fd, 0, SEEK_CUR )) == -1) return -1;
    if (lseek( fd, offset, SEEK_SET ) == -1) return -1;
    if ((ret = write( fd, buf, count )) == -1)
    {
        int err = errno;  /* save errno */
        lseek( fd, old_pos, SEEK_SET );
        errno = err;
        return -1;
    }
    if (lseek( fd, old_pos, SEEK_SET ) == -1) return -1;
    return ret;
}
#endif /* HAVE_PWRITE */


/***********************************************************************
 *		getrlimit
 */
#ifndef HAVE_GETRLIMIT
int getrlimit (int resource, struct rlimit *rlim)
{
    return -1; /* FAIL */
}
#endif /* HAVE_GETRLIMIT */


/***********************************************************************
 *		wine_dlopen
 */
void *wine_dlopen( const char *filename, int flag, char *error, int errorsize )
{
#ifdef HAVE_DLOPEN
    void *ret;
    char *s;
    dlerror(); dlerror();
    ret = dlopen( filename, flag );
#ifdef __APPLE__
    /* On the Mac, in a packaged build, there is no way for us to set up the 
       LD_LIBRARY_PATH once the app has launched.  This prevents certain dynamic
       loading mechansms (ie: shmserver) from working properly in that environment
       unless we manually add the required path in. We do so here if the above call 
       otherwise failed, adding the WINEDLLPATH in as needed */
    if (!ret)
    {
        static char mac_dll_path[4096];
	unsigned int base_path_len = strlen(getenv("WINEDLLPATH"));
	if (base_path_len + strlen(filename) < 4096)
	{
	    strcpy(mac_dll_path, getenv("WINEDLLPATH"));
	    mac_dll_path[base_path_len] = '/';
	    strcpy(mac_dll_path + base_path_len + 1, filename);
	    
	    ret = dlopen( mac_dll_path, flag );
	}
    }
#endif
    s = dlerror();
    if (error)
    {
	strncpy( error, s ? s : "", errorsize );
	error[errorsize - 1] = '\0';
    }
    dlerror();
    return ret;
#else
    if (error)
    {
	strncpy( error, "dlopen interface not detected by configure", errorsize );
	error[errorsize - 1] = '\0';
    }
    return NULL;
#endif
}

/***********************************************************************
 *		wine_dlsym
 */
void *wine_dlsym( void *handle, const char *symbol, char *error, int errorsize )
{
#ifdef HAVE_DLOPEN
    void *ret;
    char *s;
    dlerror(); dlerror();
    ret = dlsym( handle, symbol );
    s = dlerror();
    if (error)
    {
	strncpy( error, s ? s : "", errorsize );
	error[errorsize - 1] = '\0';
    }
    dlerror();
    return ret;
#else
    if (error)
    {
	strncpy( error, "dlopen interface not detected by configure", errorsize );
	error[errorsize - 1] = '\0';
    }
    return NULL;
#endif
}

/***********************************************************************
 *		wine_dlclose
 */
int wine_dlclose( void *handle, char *error, int errorsize )
{
#ifdef HAVE_DLOPEN
    int ret;
    char *s;
    dlerror(); dlerror();
    ret = dlclose( handle );
    s = dlerror();
    if (error)
    {
	strncpy( error, s ? s : "", errorsize );
	error[errorsize - 1] = '\0';
    }
    dlerror();
    return ret;
#else
    if (error)
    {
	strncpy( error, "dlopen interface not detected by configure", errorsize );
	error[errorsize - 1] = '\0';
    }
    return 1;
#endif
}

/***********************************************************************
 *		wine_rewrite_s4tos2
 *
 * Convert 4 byte Unicode strings to 2 byte Unicode strings in-place.
 * This is only practical if literal strings are writable.
 */
unsigned short* wine_rewrite_s4tos2(const wchar_t* str4 )
{
    unsigned short *str2,*s2;

    if (str4==NULL)
      return NULL;

    if ((*str4 & 0xffff0000) != 0) {
        /* This string has already been converted. Return it as is */
        return (unsigned short*)str4;
    }

    /* Note that we can also end up here if the string has a single
     * character. In such a case we will convert the string over and
     * over again. But this is harmless.
     */
    str2=s2=(unsigned short*)str4;
    do {
        *s2=(unsigned short)*str4;
	s2++;
    } while (*str4++ != L'\0');

    return str2;
}

#ifndef HAVE_ECVT
/*
 * NetBSD 1.5 doesn't have ecvt, fcvt, gcvt. We just check for ecvt, though.
 * Fix/verify these implementations !
 */

/***********************************************************************
 *		ecvt
 */
char *ecvt (double number, int  ndigits,  int  *decpt,  int *sign)
{
    static char buf[40]; /* ought to be enough */
    char *dec;
    sprintf(buf, "%.*e", ndigits /* FIXME wrong */, number);
    *sign = (number < 0);
    dec = strchr(buf, '.');
    *decpt = (dec) ? (int)dec - (int)buf : -1;
    return buf;
}

/***********************************************************************
 *		fcvt
 */
char *fcvt (double number, int  ndigits,  int  *decpt,  int *sign)
{
    static char buf[40]; /* ought to be enough */
    char *dec;
    sprintf(buf, "%.*e", ndigits, number);
    *sign = (number < 0);
    dec = strchr(buf, '.');
    *decpt = (dec) ? (int)dec - (int)buf : -1;
    return buf;
}

/***********************************************************************
 *		gcvt
 *
 * FIXME: uses both E and F.
 */
char *gcvt (double number, size_t  ndigit,  char *buff)
{
    sprintf(buff, "%.*E", (int)ndigit, number);
    return buff;
}
#endif /* HAVE_ECVT */



/* FIXME: Would be better to not have this duplicated, but we don't have a proper
 *        port library at this point.
 */

#if defined(__linux__) && defined(__i386__)

#  if !defined( SYS_tkill )
#    define SYS_tkill 238
#  endif

#  if !defined( SYS_gettid )
#    define SYS_gettid 224
#  endif


/* Get the id of a thread */
pid_t wine_get_global_tid(void)
{
    pid_t ret;
  
    __asm__("int $0x80" : "=a" (ret) : "0" (SYS_gettid));

    if( ret > 0 ) return ret;
    
    return 0;
}


/* Send a signal to a specific thread */
int wine_tkill( pid_t tid, int sig )
{
  int ret;

  __asm__( "pushl %%ebx\n\t"
           "movl %2,%%ebx\n\t"
           "int $0x80\n\t"
           "popl %%ebx\n\t"
             : "=a" (ret)
             : "0" (SYS_tkill), "r" (tid), "c" (sig) );

  if( ret >= 0 ) return ret;

  return -1;
}
#endif /* __linux__ && __i386__ */


pid_t wine_get_inprocess_tid(void)
{
#if defined(__linux__) && (__i386__)
   return wine_get_global_tid();
#elif defined(__APPLE__)
   return mach_thread_self();
#else
   #error "wine_get_inprocess_tid unimplemented for this platform!"
#endif
}


/* Get the id of a thread if platform has global tids; otherwise, return pid */
pid_t wine_gettid_or_pid(void)
{
  
#if defined( __linux__ ) && defined( __i386__ )
    pid_t ret;

    ret = wine_get_global_tid ();
    if( ret >= 0 ) return ret;
#endif
    
   return getpid();
}


/* Send a signal to a specific thread if supported by platform; otherwise,
   send to process */
int wine_tkill_or_kill( pid_t tid, int sig )
{
  int ret;

#if defined( __linux__ ) &&  defined( __i386__ )
  ret = wine_tkill (tid, sig);
  if( ret >= 0 ) return ret;
#endif

  ret = kill( tid, sig );
  
  return ret;
}
