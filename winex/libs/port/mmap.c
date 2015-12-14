/*
 * mmap functions
 *
 *  Copyright 1996 Alexandre Julliard
 *  Copyright 2003,2005 TransGaming Technologies
 *
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

typedef struct reserved_area reserved_area;
struct reserved_area
{
    const void *addr;
    size_t size;
    reserved_area *next;
};

static reserved_area *reserved_list = NULL;

#if 0
static void wine_mmap_dump_reserve()
{
    reserved_area *cur = reserved_list;

    fprintf(stderr, "************* reserve list ***************\n");
    while (cur)
    {
        fprintf(stderr, "(%p) %p -> %p (%08x)\n", cur,
                cur->addr, (char*)cur->addr + cur->size, cur->size);
        cur = cur->next;
    }
    fprintf(stderr, "******************************************\n");
}
#endif

void wine_mmap_add_reserve(const void *addr, size_t size)
{
    reserved_area *prev, *next, *new = NULL;
    reserved_area *pprev; /* previous to the previous */
    int overlap_prev = 0;
    const void *end = NULL;

    pprev = NULL;
    prev = reserved_list;
    while (prev)
    {
        if (prev->addr > addr) { prev = pprev; break; } /* start is free */
        if (prev->addr <= addr && ((const char*)prev->addr + prev->size) >= (const char*)addr)
        { /* start overlaps with previous block, merge */
            overlap_prev = 1;
            break;
        }

        pprev = prev;
        prev = prev->next;
    }
    if (!prev) prev = pprev;

    if (!overlap_prev)
    {
        new = malloc(sizeof(reserved_area));
        new->addr = addr;
        new->size = 0;
        if (prev)
        {
           new->next = prev->next;
           prev->next = new;
        }
        else
        {
            new->next = reserved_list;
            reserved_list = new;
        }
    }
    else
    {
        new = prev;
    }

    end = (const char*)addr + size;
    if (((const char*)new->addr + new->size) > (const char*)end)
        end = (const char*)new->addr + new->size;
    next = new->next;
    while (next)
    {
        if (end > next->addr) /* merge with next */
        {
            /* increase end if merged end is greater than new end */
            if (((const char*)next->addr + next->size) > (const char*)end)
                end = ((const char*)next->addr + next->size);
            new->next = next->next;
            free(next);
            next = new->next;
        }
        else break;
    }

    new->size = (const char*)end - (const char*)new->addr;
}

void wine_mmap_remove_reserve(const void *addr, size_t size)
{
    fprintf(stderr, "ERROR: wine_mmap_remove_reserve unimplemented\n");
}

/* returns:  0 on not reserved,
 *          -1 on partial reserved,
 *           1 on fully reserved
 */
int wine_mmap_is_reserved(const void *addr, size_t size)
{
    reserved_area *cur = reserved_list;
    const void *end = (const char*)addr + size;

    while (cur)
    {
        const void *cur_end = (const char *)cur->addr + cur->size;
        if (cur->addr <= addr)
        {
            if (cur_end >= end) return 1; /* fully inside this block */
            if (addr < cur_end && end > cur_end) return -1; /* start is in this
                                                               block, end isn't */
        }
        if (cur->addr > addr)
        {
            if (end > cur->addr) return -1; /* start is out of block, end is in block */
            break;
        }
        cur = cur->next;
    }
    return 0;
}

#if defined(__svr4__) || defined(__NetBSD__)
/***********************************************************************
 *             try_mmap_fixed
 *
 * The purpose of this routine is to emulate the behaviour of
 * the Linux mmap() routine if a non-NULL address is passed,
 * but the MAP_FIXED flag is not set.  Linux in this case tries
 * to place the mapping at the specified address, *unless* the
 * range is already in use.  Solaris, however, completely ignores
 * the address argument in this case.
 *
 * As Wine code occasionally relies on the Linux behaviour, e.g. to
 * be able to map non-relocateable PE executables to their proper
 * start addresses, or to map the DOS memory to 0, this routine
 * emulates the Linux behaviour by checking whether the desired
 * address range is still available, and placing the mapping there
 * using MAP_FIXED if so.
 */
static int try_mmap_fixed (void *addr, size_t len, int prot, int flags,
                           int fildes, off_t off)
{
    char * volatile result = NULL;
    int pagesize = getpagesize();
    pid_t pid;

    /* We only try to map to a fixed address if
       addr is non-NULL and properly aligned,
       and MAP_FIXED isn't already specified. */

    if ( !addr )
        return 0;
    if ( (uintptr_t)addr & (pagesize-1) )
        return 0;
    if ( flags & MAP_FIXED )
        return 0;

    /* We use vfork() to freeze all threads of the
       current process.  This allows us to check without
       race condition whether the desired memory range is
       already in use.  Note that because vfork() shares
       the address spaces between parent and child, we
       can actually perform the mapping in the child. */

    if ( (pid = vfork()) == -1 )
    {
        perror("try_mmap_fixed: vfork");
        exit(1);
    }
    if ( pid == 0 ) /* Child process */
    {
        int i;
        char vec;

        /* We call mincore() for every page in the desired range.
           If any of these calls succeeds, the page is already
           mapped and we must fail. */
        for ( i = 0; i < len; i += pagesize )
            if ( mincore( (caddr_t)addr + i, pagesize, &vec ) != -1 )
               _exit(1);

        /* Perform the mapping with MAP_FIXED set.  This is safe
           now, as none of the pages is currently in use. */
        result = mmap( addr, len, prot, flags | MAP_FIXED, fildes, off );
        if ( result == addr )
            _exit(0);

        if ( result != (void *) -1 ) /* This should never happen ... */
            munmap( result, len );

       _exit(1);
    }

    /* vfork() lets the parent continue only after the child
       has exited.  Furthermore, Wine sets SIGCHLD to SIG_IGN,
       so we don't need to wait for the child. */

    return result == addr;
}
#endif

/***********************************************************************
 *		wine_anon_mmap
 *
 * Portable wrapper for anonymous mmaps
 */
void *wine_anon_mmap( void *start, size_t size, int prot, int flags )
{
    static int fdzero = -1;

#ifdef MAP_ANON
    flags |= MAP_ANON;
#else
    if (fdzero == -1)
    {
        if ((fdzero = open( "/dev/zero", O_RDONLY )) == -1)
        {
            perror( "/dev/zero: open" );
            exit(1);
        }
    }
#endif  /* MAP_ANON */

#ifdef MAP_SHARED
    flags &= ~MAP_SHARED;
#endif

    /* Linux EINVAL's on us if we don't pass MAP_PRIVATE to an anon mmap */
#ifdef MAP_PRIVATE
    flags |= MAP_PRIVATE;
#endif

#if defined(__svr4__) || defined(__NetBSD__)
    if ( try_mmap_fixed( start, size, prot, flags, fdzero, 0 ) )
        return start;
#endif

    return mmap( start, size, prot, flags, fdzero, 0 );
}


/*
 * These functions provide wrappers around dlopen() and associated
 * functions.  They work around a bug in glibc 2.1.x where calling
 * a dl*() function after a previous dl*() function has failed
 * without a dlerror() call between the two will cause a crash.
 * They all take a pointer to a buffer that
 * will receive the error description (from dlerror()).  This
 * parameter may be NULL if the error description is not required.
 */


