/*
 * Wine library reentrant errno support
 *
 * Copyright 1998 Alexandre Julliard
 * Copyright 2003 TransGaming Technologies
 */

#include "config.h"

#define _GNU_SOURCE

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#include <dlfcn.h>
#include <unistd.h>

static int *default_errno_location(void);
static int *default_h_errno_location(void);

static int static_errno;
static int static_h_errno;
static int *libc_errno = &static_errno;
static int *libc_h_errno = &static_h_errno;

int* (*wine_errno_location)(void) = default_errno_location;
int* (*wine_h_errno_location)(void) = default_h_errno_location;


/* Get pointers to the static errno and h_errno variables used by Xlib. This
   must be done before including <errno.h> makes the variables invisible.  */
static int *default_errno_location(void)
{
    return libc_errno;
}

static int *default_h_errno_location(void)
{
    return libc_h_errno;
}

#if !defined( USE_PTHREADS )

                                                                                                                                                                                      
/***********************************************************************
 *           __errno_location/__error/___errno
 *
 * Get the per-thread errno location.
 */
#ifdef ERRNO_LOCATION
int *ERRNO_LOCATION(void)
{
    return wine_errno_location();
}
#endif /* ERRNO_LOCATION */
                                                                                                                                                                                      
/***********************************************************************
 *           __h_errno_location
 *
 * Get the per-thread h_errno location.
 */
int *__h_errno_location(void)
{
    return wine_h_errno_location();
}

#ifndef USE_PTHREADS
static void patch_libc(void *func, void *location)
{
#ifdef __i386__
    unsigned char *code = func;
    unsigned char *page = code - (((unsigned long)code)%getpagesize());
    mprotect(page, 5, PROT_READ|PROT_EXEC|PROT_WRITE);
    code[0] = 0xff; /* ff 25 = jmp indirect */
    code[1] = 0x25;
    *((void**)&code[2]) = location;
    mprotect(page, 5, PROT_READ|PROT_EXEC);
#endif
}
#endif

#endif

void ERRNO_init(void)
{
#if !defined( USE_PTHREADS ) && defined( linux )
    void *libc_handle = RTLD_NEXT;
    void *tmp;
    tmp = dlsym(libc_handle, "__errno_location");
    patch_libc(tmp, &wine_errno_location);
    tmp = dlsym(libc_handle, "__h_errno_location");
    patch_libc(tmp, &wine_h_errno_location);
#endif
}

