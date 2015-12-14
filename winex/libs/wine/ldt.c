/*
 * LDT manipulation functions
 *
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Alexandre Julliard
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "winbase.h"
#include "wine/library.h"

#ifdef __i386__

#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#ifdef __APPLE__
/* MacOS X requires a pthread mutex around ldt calls to prevent kernel panics (?) */
#include <pthread.h>
pthread_mutex_t LDT_pthread_mutex;
#endif

struct modify_ldt_s
{
    unsigned int  entry_number;
    unsigned long base_addr;
    unsigned int  limit;
    unsigned int  seg_32bit : 1;
    unsigned int  contents : 2;
    unsigned int  read_exec_only : 1;
    unsigned int  limit_in_pages : 1;
    unsigned int  seg_not_present : 1;
    unsigned int  useable : 1;
    unsigned int  __wine_unused : 25; /* This field doesn't exist in modify_ldt_t */
};

#ifdef linux

#if !defined( SYS_set_thread_area ) /* linux only */
# define SYS_set_thread_area 243
#endif


static inline int modify_ldt( int func, struct modify_ldt_s *ptr,
                                  unsigned long count )
{
    int res;
#ifdef __PIC__
    __asm__ __volatile__( "pushl %%ebx\n\t"
                          "movl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "popl %%ebx"
                          : "=a" (res)
                          : "0" (SYS_modify_ldt),
                            "r" (func),
                            "c" (ptr),
                            "d" (count) );
#else
    __asm__ __volatile__("int $0x80"
                         : "=a" (res)
                         : "0" (SYS_modify_ldt),
                           "b" (func),
                           "c" (ptr),
                           "d" (count) );
#endif  /* __PIC__ */
    if (res >= 0) return res;
    errno = -res;
    return -1;
}

/* Newer linux kernels actually have TLS support. This will set the area which is swapped
 * on thread switches. */
static inline int set_thread_area( struct modify_ldt_s* ptr )
{
    int res;

    // PH: FIXME: What about  TLS_FLAG_LIMIT_IN_PAGES and such for the call??   
    
#ifdef __PIC__
    __asm__ __volatile__( "pushl %%ebx\n\t"
                          "movl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "popl %%ebx"
                          : "=a" (res)
                          : "0" (SYS_set_thread_area),
                            "r" (ptr) );
#else
    __asm__ __volatile__("int $0x80"
                         : "=a" (res)
                         : "0" (SYS_set_thread_area),
                           "b" (ptr) );
#endif
    if (res >= 0) return res;
    errno = -res;
    return -1;
}


#endif  /* linux */

#if defined(__svr4__) || defined(_SCO_DS)
#include <sys/sysi86.h>
extern int sysi86(int,void*);
#ifndef __sun__
#include <sys/seg.h>
#endif
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__) 
#include <machine/segments.h>

extern int i386_get_ldt(int, union descriptor *, int);
extern int i386_set_ldt(int, union descriptor *, int);
#endif  /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */

#ifdef __APPLE__
#include <i386/user_ldt.h>
#include <architecture/i386/table.h>
#endif


#if defined( __linux__ )
/***********************************************************************
 *           ldt_initialize_modify_ldt_struct
 *
 * Initialize a struct modify_ldt_s
 */
static void wine_ldt_initialize_modify_ldt_struct( struct modify_ldt_s* ldt_info, int index, const LDT_ENTRY* entry )
{
    ldt_info->entry_number    = index;
    ldt_info->base_addr       = (unsigned long)wine_ldt_get_base(entry);
    ldt_info->limit           = entry->LimitLow | (entry->HighWord.Bits.LimitHi << 16);
    ldt_info->seg_32bit       = entry->HighWord.Bits.Default_Big;
    ldt_info->contents        = (entry->HighWord.Bits.Type >> 2) & 3;
    ldt_info->read_exec_only  = !(entry->HighWord.Bits.Type & 2);
    ldt_info->limit_in_pages  = entry->HighWord.Bits.Granularity;
    ldt_info->seg_not_present = !entry->HighWord.Bits.Pres;
    ldt_info->useable         = entry->HighWord.Bits.Sys;
    ldt_info->__wine_unused   = 0;
}
#endif /* __linux__ */

#endif  /* __i386__ */

/* local copy of the LDT */
struct __wine_ldt_copy wine_ldt_copy;



/***********************************************************************
 *           ldt_get_entry
 *
 * Retrieve an LDT entry.
 */
void wine_ldt_get_entry( unsigned short sel, LDT_ENTRY *entry )
{
    int index = sel >> 3;

    memset (entry, 0, sizeof (*entry));
    wine_ldt_set_base(  entry, wine_ldt_copy.base[index] );
    wine_ldt_set_limit( entry, wine_ldt_copy.limit[index] );
    wine_ldt_set_flags( entry, wine_ldt_copy.flags[index] );
}


/***********************************************************************
 *           ldt_set_entry
 *
 * Set an LDT entry.
 */
int wine_ldt_set_entry( unsigned short sel, const LDT_ENTRY *entry )
{
    int ret = 0, index = sel >> 3;

    /* Entry 0 must not be modified; its base and limit are always 0 */
    if (!index) return 0;

#ifdef __i386__

#ifdef linux
    {
        struct modify_ldt_s ldt_info;

        wine_ldt_initialize_modify_ldt_struct( &ldt_info, index, entry );

        if ((ret = modify_ldt(1, &ldt_info, sizeof(ldt_info))) < 0)
            perror( "modify_ldt" );
    }
#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    {
	LDT_ENTRY entry_copy = *entry;
	/* The kernel will only let us set LDTs with user priority level */
	if (entry_copy.HighWord.Bits.Pres
	    && entry_copy.HighWord.Bits.Dpl != 3)
		entry_copy.HighWord.Bits.Dpl = 3;
        ret = i386_set_ldt(index, (union descriptor *)&entry_copy, 1);
        if (ret < 0)
        {
            perror("i386_set_ldt");
            fprintf( stderr, "Did you reconfigure the kernel with \"options USER_LDT\"?\n" );
            exit(1);
        }
    }
#elif defined(__svr4__) || defined(_SCO_DS)
    {
        struct ssd ldt_mod;
        ldt_mod.sel  = sel;
        ldt_mod.bo   = (unsigned long)wine_ldt_get_base(entry);
        ldt_mod.ls   = entry->LimitLow | (entry->HighWord.Bits.LimitHi << 16);
        ldt_mod.acc1 = entry->HighWord.Bytes.Flags1;
        ldt_mod.acc2 = entry->HighWord.Bytes.Flags2 >> 4;
        if ((ret = sysi86(SI86DSCR, &ldt_mod)) == -1) perror("sysi86");
    }
#elif defined(__APPLE__)
    /* First, lock the critsec to prevent kernel panics */
    {
	LDT_ENTRY entry_copy = *entry;
	pthread_mutex_lock(&LDT_pthread_mutex);

	/* The kernel will only let us set LDTs with user priority level (true on darwin??) */
	if (entry_copy.HighWord.Bits.Pres
	    && entry_copy.HighWord.Bits.Dpl != 3)
		entry_copy.HighWord.Bits.Dpl = 3;
        ret = i386_set_ldt(index, (ldt_entry_t *)&entry_copy, 1);
        if (ret < 0)
        {
            perror("i386_set_ldt");
            fprintf( stderr, "Problem with MacOS i386_set_ldt function!\n" );
        }
	pthread_mutex_unlock(&LDT_pthread_mutex);

    }
#else
#error "Define for your OS"
#endif

#endif  /* __i386__ */

    if (ret >= 0)
    {
        wine_ldt_copy.base[index]  = wine_ldt_get_base(entry);
        wine_ldt_copy.limit[index] = wine_ldt_get_limit(entry);
        wine_ldt_copy.flags[index] = (entry->HighWord.Bits.Type |
                                 (entry->HighWord.Bits.Default_Big ? WINE_LDT_FLAGS_32BIT : 0) |
                                 (wine_ldt_copy.flags[index] & WINE_LDT_FLAGS_ALLOCATED));
    }
    return ret;
}

#if defined( __linux__ )
static int tried_already = 0;
#endif /* __linux__ */
static unsigned short thread_area_selector = 0;
static const LDT_ENTRY null_entry;  /* all-zeros, used to clear LDT entries */

/* Get, if supported, the LDT or GDT entry to be used for the %fs register */
unsigned short wine_ldt_alloc_os_supported_thread_area_selector( const void* base, DWORD size, unsigned char flags )
{
#if defined( __linux__ )
  if( !tried_already )
  {
    struct modify_ldt_s ldt_info;

    /* No race can exist for this as there's only 1 thread at startup */
    tried_already = 1;

    wine_ldt_initialize_modify_ldt_struct( &ldt_info, -1, &null_entry );

    if( set_thread_area( &ldt_info ) >= 0 )
    {
      thread_area_selector = (unsigned short)(ldt_info.entry_number << 3) | 3;
    }
    else if( errno != ENOSYS )
    {
      perror( "set_thread_area" );
    }
  }
#endif /* __linux__ */
  
  return thread_area_selector;
}

/* Inform the OS of the thread area for this thread. Too bad they didn't implement the ability
 * to set another thread's tls...then we wouldn't have needed this to be called in the new thread's
 * context */
void wine_ldt_os_set_thread_area_for_thread( unsigned short sel, const void* base, DWORD size, unsigned char flags )
{
#if defined( __linux__ )

  if( thread_area_selector == sel )
  {
    struct modify_ldt_s ldt_info;
    LDT_ENTRY fs_entry;

    memset (&fs_entry, 0, sizeof (fs_entry));
    wine_ldt_set_base( &fs_entry, base );
    wine_ldt_set_limit( &fs_entry, size-1 );
    wine_ldt_set_flags( &fs_entry, flags );

    wine_ldt_initialize_modify_ldt_struct( &ldt_info, sel>>3, &fs_entry );

    if( set_thread_area( &ldt_info ) < 0 )
    {
      perror( "set_thread_area" );
    }
  }
#endif /* __linux__ */
}

/* If we've allocated it, "free" it. No need to actually attempt to free it since
 * the only time we'll do this is if this is the last thread of the process in which
 * case the OS will do it for us when required.
 */
int wine_ldt_os_supported_thread_area_selector_free( unsigned short sel )
{
  if( thread_area_selector == sel )
  {
    return 1;
  }

  return 0;
}

