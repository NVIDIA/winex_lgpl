/*
 * Preloader for ld.so
 *
 * Copyright (C) 1995,96,97,98,99,2000,2001,2002 Free Software Foundation, Inc.
 * Copyright (C) 2004 Mike McCormack for CodeWeavers
 * Copyright (C) 2004 Alexandre Julliard
 * Copyright (c) 2005-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Design notes
 *
 * The goal of this program is to be a workaround for exec-shield, as used
 *  by the Linux kernel distributed with Fedora Core and other distros.
 *
 * To do this, we implement our own shared object loader that reserves memory
 * that is important to Wine, and then loads the main binary and its ELF
 * interpreter.
 *
 * We will try to set up the stack and memory area so that the program that
 * loads after us (eg. the wine binary) never knows we were here, except that
 * areas of memory it needs are already magically reserved.
 *
 * If this program is used as the shared object loader, the only difference
 * that the loaded programs should see is that this loader will be mapped
 * into memory when it starts.
 */

/*
 * References (things I consulted to understand how ELF loading works):
 *
 * glibc 2.3.2   elf/dl-load.c
 *  http://www.gnu.org/directory/glibc.html
 *
 * Linux 2.6.4   fs/binfmt_elf.c
 *  ftp://ftp.kernel.org/pub/linux/kernel/v2.6/linux-2.6.4.tar.bz2
 *
 * Userland exec, by <grugq@hcunix.net>
 *  http://cert.uni-stuttgart.de/archive/bugtraq/2004/01/msg00002.html
 *
 * The ELF specification:
 *  http://www.linuxbase.org/spec/booksets/LSB-Embedded/LSB-Embedded/book387.html
 */

#if !defined(__linux__) && !defined(__i386__)
# error "Not supposed to be built"
#endif

#if !defined(__GNUC__)
# error "Need to implement asm for your compiler"
#endif

#include "config.h"
#include "wine/port.h"
#include "wine/vmlayout.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_ELF_H
# include <elf.h>
#endif
#ifdef HAVE_LINK_H
# include <link.h>
#endif
#ifdef HAVE_SYS_LINK_H
# include <sys/link.h>
#endif

#include "wine/main.h"

#ifndef PRELOADER_BINARY_NAME
# define PRELOADER_BINARY_NAME wine-preloader
#endif
#define xstr(s) str(s)
#define str(s) #s

/* ELF definitions */
#define ELF_PREFERRED_ADDRESS(loader, maplength, mapstartpref) (mapstartpref)
#define ELF_FIXED_ADDRESS(loader, mapstart) ((void) 0)

#define MAP_BASE_ADDR(l)     0

#ifndef MAP_COPY
#define MAP_COPY MAP_PRIVATE
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

static struct wine_preload_info preload_info[] =
{
    { (void *)DOSAREA_ADDR,     DOSAREA_RANGE,     0 },  /* DOS area */
    { (void *)SHAREDHEAP_ADDR,  SHAREDHEAP_RANGE,  0 },  /* shared heap */
    { (void *)PE_EXE_ADDR,      PE_EXE_RANGE,      0 },  /* default PE exe range (may be set with WINEPRELOADRESERVE) */
    { (void *)SHMSERVER_ADDR,   SHMSERVER_SIZE,    PRELOAD_INFO_SERVERADDR }, /* shared memory server */
    { (void *)KUSERDATA_ADDR,   KUSERDATA_RANGE,   0 },  /* KUSER_SHARED_DATA, should actually be size of the structure */
    { (void *)CRTS_ADDR,        CRTS_RANGE,        0 },  /* reserve the range requested by the CRTs */
    { 0, 0, PRELOAD_INFO_NULL }                          /* end of list */
};

static struct wine_preload_info preload_server_info[] =
{
    { (void *)SHMSERVER_ADDR, SHMSERVER_SIZE, PRELOAD_INFO_SERVERADDR },
    { 0, 0, PRELOAD_INFO_NULL }
};

/* points to the one of the above two we are using. */
static struct wine_preload_info const *using_preload_info = NULL;

/* debugging */
#undef DUMP_SEGMENTS
#undef DUMP_AUX_INFO
#undef DUMP_SYMS

/* older systems may not define these */
#ifndef PT_TLS
#define PT_TLS 7
#endif

#ifndef AT_SYSINFO
#define AT_SYSINFO 32
#endif
#ifndef AT_SYSINFO_EHDR
#define AT_SYSINFO_EHDR 33
#endif

#ifndef DT_GNU_HASH
#define DT_GNU_HASH 0x6ffffef5
#endif

static unsigned int page_size, page_mask;
static char *preloader_start, *preloader_end;

struct wld_link_map {
    ElfW(Addr) l_addr;
    ElfW(Dyn) *l_ld;
    ElfW(Phdr)*l_phdr;
    ElfW(Addr) l_entry;
    ElfW(Half) l_ldnum;
    ElfW(Half) l_phnum;
    ElfW(Addr) l_map_start, l_map_end;
    ElfW(Addr) l_interp;
};


/*
 * The __bb_init_func is an empty function only called when file is
 * compiled with gcc flags "-fprofile-arcs -ftest-coverage".  This
 * function is normally provided by libc's startup files, but since we
 * build the preloader with "-nostartfiles -nodefaultlibs", we have to
 * provide our own (empty) version, otherwise linker fails.
 */
void __bb_init_func(void) { return; }

/* similar to the above but for -fstack-protector */
void *__stack_chk_guard = 0;
void __stack_chk_fail(void) { return; }

/* data for setting up the glibc-style thread-local storage in %gs */

static int thread_data[256];

struct
{
    /* this is the kernel modify_ldt struct */
    unsigned int  entry_number;
    unsigned long base_addr;
    unsigned int  limit;
    unsigned int  seg_32bit : 1;
    unsigned int  contents : 2;
    unsigned int  read_exec_only : 1;
    unsigned int  limit_in_pages : 1;
    unsigned int  seg_not_present : 1;
    unsigned int  useable : 1;
    unsigned int  garbage : 25;
} thread_ldt = { -1, (unsigned long)thread_data, 0xfffff, 1, 0, 0, 1, 0, 1, 0 };


/*
 * The _start function is the entry and exit point of this program
 *
 *  It calls wld_start, passing a pointer to the args it receives
 *  then jumps to the address wld_start returns.
 */
void _start();
extern char _end[];
__ASM_GLOBAL_FUNC(_start,
                  "\tmovl $243,%eax\n"        /* SYS_set_thread_area */
                  "\tmovl $thread_ldt,%ebx\n"
                  "\tint $0x80\n"             /* allocate gs segment */
                  "\torl %eax,%eax\n"
                  "\tjl 1f\n"
                  "\tmovl thread_ldt,%eax\n"  /* thread_ldt.entry_number */
                  "\tshl $3,%eax\n"
                  "\torl $3,%eax\n"
                  "\tmov %ax,%gs\n"
                  "\tmov %ax,%fs\n"           /* set %fs too so libwine can retrieve it later on */
                  "1:\tmovl %esp,%eax\n"
                  "\tleal -136(%esp),%esp\n"  /* allocate some space for extra aux values */
                  "\tpushl %eax\n"            /* orig stack pointer */
                  "\tpushl %esp\n"            /* ptr to orig stack pointer */
                  "\tcall wld_start\n"
                  "\tpopl %ecx\n"             /* remove ptr to stack pointer */
                  "\tpopl %esp\n"             /* new stack pointer */
                  "\tpush %eax\n"             /* ELF interpreter entry point */
                  "\txor %eax,%eax\n"
                  "\txor %ecx,%ecx\n"
                  "\txor %edx,%edx\n"
                  "\tmov %ax,%gs\n"           /* clear %gs again */
                  "\tret\n")

/* wrappers for Linux system calls */

#define SYSCALL_RET(ret) (((ret) < 0 && (ret) > -4096) ? -1 : (ret))

static inline __attribute__((noreturn)) void wld_exit( int code )
{
    for (;;)  /* avoid warning */
        __asm__ __volatile__( "pushl %%ebx; movl %1,%%ebx; int $0x80; popl %%ebx"
                              : : "a" (SYS_exit), "r" (code) );
}

static inline int wld_open( const char *name, int flags )
{
    int ret;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_open), "r" (name), "c" (flags) );
    return SYSCALL_RET(ret);
}

static inline int wld_close( int fd )
{
    int ret;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_close), "r" (fd) );
    return SYSCALL_RET(ret);
}

static inline ssize_t wld_read( int fd, void *buffer, size_t len )
{
    int ret;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret)
                          : "0" (SYS_read), "r" (fd), "c" (buffer), "d" (len)
                          : "memory" );
    return SYSCALL_RET(ret);
}

static inline ssize_t wld_write( int fd, const void *buffer, size_t len )
{
    int ret;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_write), "r" (fd), "c" (buffer), "d" (len) );
    return SYSCALL_RET(ret);
}

static inline int wld_mprotect( const void *addr, size_t len, int prot )
{
    int ret;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_mprotect), "r" (addr), "c" (len), "d" (prot) );
    return SYSCALL_RET(ret);
}

static void *wld_mmap( void *start, size_t len, int prot, int flags, int fd, off_t offset )
{
    int ret;

    struct
    {
        void        *addr;
        unsigned int length;
        unsigned int prot;
        unsigned int flags;
        unsigned int fd;
        unsigned int offset;
    } args;

    args.addr   = start;
    args.length = len;
    args.prot   = prot;
    args.flags  = flags;
    args.fd     = fd;
    args.offset = offset;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_mmap), "q" (&args) : "memory" );
    return (void *)SYSCALL_RET(ret);
}

static inline uid_t wld_getuid(void)
{
    uid_t ret;
    __asm__( "int $0x80" : "=a" (ret) : "0" (SYS_getuid) );
    return ret;
}

static inline uid_t wld_geteuid(void)
{
    uid_t ret;
    __asm__( "int $0x80" : "=a" (ret) : "0" (SYS_geteuid) );
    return ret;
}

static inline gid_t wld_getgid(void)
{
    gid_t ret;
    __asm__( "int $0x80" : "=a" (ret) : "0" (SYS_getgid) );
    return ret;
}

static inline gid_t wld_getegid(void)
{
    gid_t ret;
    __asm__( "int $0x80" : "=a" (ret) : "0" (SYS_getegid) );
    return ret;
}

static inline int wld_execve(const char *file,
                             char *argv[],
                             char *envp[])
{
    int ret;
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_execve), "r" (file),
                            "c" (argv), "d" (envp) );
    return SYSCALL_RET(ret);
}

static inline int wld_personality(int personality)
{
    int ret;
#ifndef SYS_personality
#define SYS_personality 136
#endif
    __asm__ __volatile__( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                          : "=a" (ret) : "0" (SYS_personality), "r" (personality) );
    return SYSCALL_RET(ret);
}

static inline int wld_prctl(int option, unsigned long arg2, unsigned long arg3,
                            unsigned long arg4, unsigned long arg5)
{
    int ret;
    __asm__ __volatile__ ( "pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                           : "=a" (ret) : "0" (SYS_prctl), "r" (option),
                           "c" (arg2), "d" (arg3), "S" (arg4), "D" (arg5) );
    return SYSCALL_RET(ret);
}

/* replacement for libc functions */

static int wld_strcmp( const char *str1, const char *str2 )
{
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

static int wld_strncmp( const char *str1, const char *str2, size_t len )
{
    if (len <= 0) return 0;
    while ((--len > 0) && *str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

static inline void *wld_memset( void *dest, int val, size_t len )
{
    char *dst = dest;
    while (len--) *dst++ = val;
    return dest;
}

/* unused
static inline char *wld_strcpy(char *dest, const char *src)
{
    while ((*dest = *src)) { dest++; src++; }
    return dest;
}

static inline char *wld_strncpy(char *dest, const char *src, size_t len)
{
    while (len && (*dest = *src)) { dest++; src++; len--; }
    return dest;
}*/

static inline size_t wld_strlen(const char *s)
{
    size_t len = 0;
    while (*(s++)) len++;
    return len;
}

static inline char *wld_strrchr(char *s, int c)
{
    int i = wld_strlen(s);
    char *s_iter = s+i;
    while (s_iter >= s) {
        if (*s_iter == c) return s_iter;
        s_iter--;
    }
    return 0;
}

/*
 * wld_printf - just the basics
 *
 *  %x prints a hex number
 *  %s prints a string
 */
static int wld_vsprintf(char *buffer, const char *fmt, va_list args )
{
    static const char hex_chars[16] = "0123456789abcdef";
    const char *p = fmt;
    char *str = buffer;

    while( *p )
    {
        if( *p == '%' )
        {
            p++;
            if( *p == 'x' )
            {
                int i;
                unsigned int x = va_arg( args, unsigned int );
                for(i=7; i>=0; i--)
                    *str++ = hex_chars[(x>>(i*4))&0xf];
            }
            else if( *p == 's' )
            {
                char *s = va_arg( args, char * );
                while(*s)
                    *str++ = *s++;
            }
            else if( *p == 0 )
                break;
            p++;
        }
        *str++ = *p++;
    }
    *str = 0;
    return str - buffer;
}

static void wld_printf(const char *fmt, ... )
{
    va_list args;
    char buffer[1024];
    int len;

    va_start( args, fmt );
    len = wld_vsprintf(buffer, fmt, args );
    va_end( args );
    wld_write(2, buffer, len);
}

static __attribute__((noreturn)) void fatal_error(const char *fmt, ... )
{
    va_list args;
    char buffer[256];
    int len;

    va_start( args, fmt );
    len = wld_vsprintf(buffer, fmt, args );
    va_end( args );
    wld_write(2, buffer, len);
    wld_exit(1);
}

#ifdef DUMP_AUX_INFO
/*
 *  Dump interesting bits of the ELF auxv_t structure that is passed
 *   as the 4th parameter to the _start function
 */
static void dump_auxiliary( ElfW(auxv_t) *av )
{
#define NAME(at) { at, #at }
    static const struct { int val; const char *name; } names[] =
    {
        NAME(AT_BASE),
        NAME(AT_CLKTCK),
        NAME(AT_EGID),
        NAME(AT_ENTRY),
        NAME(AT_EUID),
        NAME(AT_FLAGS),
        NAME(AT_GID),
        NAME(AT_HWCAP),
        NAME(AT_PAGESZ),
        NAME(AT_PHDR),
        NAME(AT_PHENT),
        NAME(AT_PHNUM),
        NAME(AT_PLATFORM),
        NAME(AT_SYSINFO),
        NAME(AT_SYSINFO_EHDR),
        NAME(AT_UID),
        { 0, NULL }
    };
#undef NAME

    int i;

    for (  ; av->a_type != AT_NULL; av++)
    {
        for (i = 0; names[i].name; i++) if (names[i].val == av->a_type) break;
        if (names[i].name) wld_printf("%s = %x\n", names[i].name, av->a_un.a_val);
        else wld_printf( "%x = %x\n", av->a_type, av->a_un.a_val );
    }
}
#endif

/*
 * set_auxiliary_values
 *
 * Set the new auxiliary values
 */
static void set_auxiliary_values( ElfW(auxv_t) *av, const ElfW(auxv_t) *new_av,
                                  const ElfW(auxv_t) *delete_av, void **stack )
{
    int i, j, av_count = 0, new_count = 0, delete_count = 0;
    char *src, *dst;

    /* count how many aux values we have already */
    while (av[av_count].a_type != AT_NULL) av_count++;

    /* delete unwanted values */
    for (j = 0; delete_av[j].a_type != AT_NULL; j++)
    {
        for (i = 0; i < av_count; i++) if (av[i].a_type == delete_av[j].a_type)
        {
            av[i].a_type = av[av_count-1].a_type;
            av[i].a_un.a_val = av[av_count-1].a_un.a_val;
            av[--av_count].a_type = AT_NULL;
            delete_count++;
            break;
        }
    }

    /* count how many values we have in new_av that aren't in av */
    for (j = 0; new_av[j].a_type != AT_NULL; j++)
    {
        for (i = 0; i < av_count; i++) if (av[i].a_type == new_av[j].a_type) break;
        if (i == av_count) new_count++;
    }

    src = (char *)*stack;
    dst = src - (new_count - delete_count) * sizeof(*av);
    if (new_count > delete_count)   /* need to make room for the extra values */
    {
        int len = (char *)(av + av_count + 1) - src;
        for (i = 0; i < len; i++) dst[i] = src[i];
    }
    else if (new_count < delete_count)  /* get rid of unused values */
    {
        int len = (char *)(av + av_count + 1) - dst;
        for (i = len - 1; i >= 0; i--) dst[i] = src[i];
    }
    *stack = dst;
    av -= (new_count - delete_count);

    /* now set the values */
    for (j = 0; new_av[j].a_type != AT_NULL; j++)
    {
        for (i = 0; i < av_count; i++) if (av[i].a_type == new_av[j].a_type) break;
        if (i < av_count) av[i].a_un.a_val = new_av[j].a_un.a_val;
        else
        {
            av[av_count].a_type     = new_av[j].a_type;
            av[av_count].a_un.a_val = new_av[j].a_un.a_val;
            av_count++;
        }
    }

#ifdef DUMP_AUX_INFO
    wld_printf("New auxiliary info:\n");
    dump_auxiliary( av );
#endif
}

/*
 * get_auxiliary
 *
 * Get a field of the auxiliary structure
 */
static int get_auxiliary( ElfW(auxv_t) *av, int type, int def_val )
{
  for ( ; av->a_type != AT_NULL; av++)
      if( av->a_type == type ) return av->a_un.a_val;
  return def_val;
}

int range_overlaps_reserved(void const *p, size_t size)
{
    int i;
    for (i = 0; !(using_preload_info[i].info & PRELOAD_INFO_SERVERADDR); i++)
    {
        if ((char*)p + size > (char*)using_preload_info[i].addr &&
            (char*)p <= (char*)using_preload_info[i].addr + using_preload_info[i].size)
            return 1;
    }
    return 0;
}


/*
 * map_so_lib
 *
 * modelled after _dl_map_object_from_fd() from glibc-2.3.1/elf/dl-load.c
 *
 * This function maps the segments from an ELF object, and optionally
 *  stores information about the mapping into the auxv_t structure.
 */
static void map_so_lib( const char *name, struct wld_link_map *l)
{
    int fd;
    unsigned char buf[0x800];
    ElfW(Ehdr) *header = (ElfW(Ehdr)*)buf;
    ElfW(Phdr) *phdr, *ph;
    /* Scan the program header table, collecting its load commands.  */
    struct loadcmd
      {
        ElfW(Addr) mapstart, mapend, dataend, allocend;
        off_t mapoff;
        int prot;
      } loadcmds[16], *c;
    size_t nloadcmds = 0, maplength;

    fd = wld_open( name, O_RDONLY );
    if (fd == -1) fatal_error("%s: could not open\n", name );

    if (wld_read( fd, buf, sizeof(buf) ) != sizeof(buf))
        fatal_error("%s: failed to read ELF header\n", name);

    phdr = (void*) (((unsigned char*)buf) + header->e_phoff);

    if( ( header->e_ident[0] != 0x7f ) ||
        ( header->e_ident[1] != 'E' ) ||
        ( header->e_ident[2] != 'L' ) ||
        ( header->e_ident[3] != 'F' ) )
        fatal_error( "%s: not an ELF binary... don't know how to load it\n", name );

    if( header->e_machine != EM_386 )
        fatal_error("%s: not an i386 ELF binary... don't know how to load it\n", name );

    if (header->e_phnum > sizeof(loadcmds)/sizeof(loadcmds[0]))
        fatal_error( "%s: oops... not enough space for load commands\n", name );

    maplength = header->e_phnum * sizeof (ElfW(Phdr));
    if (header->e_phoff + maplength > sizeof(buf))
        fatal_error( "%s: oops... not enough space for ELF headers\n", name );

    l->l_ld = 0;
    l->l_addr = 0;
    l->l_phdr = 0;
    l->l_phnum = header->e_phnum;
    l->l_entry = header->e_entry;
    l->l_interp = 0;

    for (ph = phdr; ph < &phdr[l->l_phnum]; ++ph)
    {

#ifdef DUMP_SEGMENTS
      wld_printf( "ph = %x\n", ph );
      wld_printf( " p_type   = %x\n", ph->p_type );
      wld_printf( " p_flags  = %x\n", ph->p_flags );
      wld_printf( " p_offset = %x\n", ph->p_offset );
      wld_printf( " p_vaddr  = %x\n", ph->p_vaddr );
      wld_printf( " p_paddr  = %x\n", ph->p_paddr );
      wld_printf( " p_filesz = %x\n", ph->p_filesz );
      wld_printf( " p_memsz  = %x\n", ph->p_memsz );
      wld_printf( " p_align  = %x\n", ph->p_align );
#endif

      switch (ph->p_type)
        {
          /* These entries tell us where to find things once the file's
             segments are mapped in.  We record the addresses it says
             verbatim, and later correct for the run-time load address.  */
        case PT_DYNAMIC:
          l->l_ld = (void *) ph->p_vaddr;
          l->l_ldnum = ph->p_memsz / sizeof (Elf32_Dyn);
          break;

        case PT_PHDR:
          l->l_phdr = (void *) ph->p_vaddr;
          break;

        case PT_LOAD:
          {
            if ((ph->p_align & page_mask) != 0)
              fatal_error( "%s: ELF load command alignment not page-aligned\n", name );

            if (((ph->p_vaddr - ph->p_offset) & (ph->p_align - 1)) != 0)
              fatal_error( "%s: ELF load command address/offset not properly aligned\n", name );

            c = &loadcmds[nloadcmds++];
            c->mapstart = ph->p_vaddr & ~(ph->p_align - 1);
            c->mapend = ((ph->p_vaddr + ph->p_filesz + page_mask) & ~page_mask);
            c->dataend = ph->p_vaddr + ph->p_filesz;
            c->allocend = ph->p_vaddr + ph->p_memsz;
            c->mapoff = ph->p_offset & ~(ph->p_align - 1);

            c->prot = 0;
            if (ph->p_flags & PF_R)
              c->prot |= PROT_READ;
            if (ph->p_flags & PF_W)
              c->prot |= PROT_WRITE;
            if (ph->p_flags & PF_X)
              c->prot |= PROT_EXEC;
          }
          break;

        case PT_INTERP:
          l->l_interp = ph->p_vaddr;
          break;

        case PT_TLS:
          /*
           * We don't need to set anything up because we're
           * emulating the kernel, not ld-linux.so.2
           * The ELF loader will set up the TLS data itself.
           */
        case PT_SHLIB:
        case PT_NOTE:
        default:
          break;
        }
    }

    /* Now process the load commands and map segments into memory.  */
    c = loadcmds;

    /* Length of the sections to be loaded.  */
    maplength = loadcmds[nloadcmds - 1].allocend - c->mapstart;

    if( header->e_type == ET_DYN )
    {
        ElfW(Addr) mappref;
        mappref = (ELF_PREFERRED_ADDRESS (loader, maplength, c->mapstart)
                   - MAP_BASE_ADDR (l));

        /* Remember which part of the address space this object uses.  */
        l->l_map_start = (ElfW(Addr)) wld_mmap ((void *) mappref, maplength,
                                              c->prot, MAP_COPY | MAP_FILE,
                                              fd, c->mapoff);
        /* wld_printf("set  : offset = %x\n", c->mapoff); */
        /* wld_printf("l->l_map_start = %x\n", l->l_map_start); */

        l->l_map_end = l->l_map_start + maplength;
        l->l_addr = l->l_map_start - c->mapstart;

        wld_mprotect ((caddr_t) (l->l_addr + c->mapend),
                    loadcmds[nloadcmds - 1].allocend - c->mapend,
                    PROT_NONE);
        goto postmap;
    }
    else
    {
        /* sanity check */
        if ((char *)c->mapstart + maplength > preloader_start &&
            (char *)c->mapstart <= preloader_end)
            fatal_error( "%s: binary overlaps preloader (%x-%x)\n",
                         name, c->mapstart, (char *)c->mapstart + maplength );

        /* This should be a fatal error.. But then wine will (almost) always fail to start.
         * We should really just always relocate the wine binary when we can..
         * Since we have the preloader, there shouldn't be a problem with doing
         * that all the time on Linux systems, I think?
         * Anyhow, FIXME DH: make this a fatal error...
         */
        /* If the binary we are trying to load overlaps with what we have
         * reserved, that is bad. Because it may then try to mmap to that
         * section, not realising it's already been used.. Then when
         * it puts something there, it'll destroy itself. Yay!
         */
        if (range_overlaps_reserved((void*)c->mapstart, maplength))
            /*fatal_error*/wld_printf("%s: binary overlaps reserved area (%x-%x)\n",
                    name, c->mapstart, (char*)c->mapstart + maplength);

        ELF_FIXED_ADDRESS (loader, c->mapstart);
    }

    /* Remember which part of the address space this object uses.  */
    l->l_map_start = c->mapstart + l->l_addr;
    l->l_map_end = l->l_map_start + maplength;

    while (c < &loadcmds[nloadcmds])
      {
        if (c->mapend > c->mapstart)
            /* Map the segment contents from the file.  */
            wld_mmap ((void *) (l->l_addr + c->mapstart),
                        c->mapend - c->mapstart, c->prot,
                        MAP_FIXED | MAP_COPY | MAP_FILE, fd, c->mapoff);

      postmap:
        if (l->l_phdr == 0
            && (ElfW(Off)) c->mapoff <= header->e_phoff
            && ((size_t) (c->mapend - c->mapstart + c->mapoff)
                >= header->e_phoff + header->e_phnum * sizeof (ElfW(Phdr))))
          /* Found the program header in this segment.  */
          l->l_phdr = (void *)(unsigned int) (c->mapstart + header->e_phoff - c->mapoff);

        if (c->allocend > c->dataend)
          {
            /* Extra zero pages should appear at the end of this segment,
               after the data mapped from the file.   */
            ElfW(Addr) zero, zeroend, zeropage;

            zero = l->l_addr + c->dataend;
            zeroend = l->l_addr + c->allocend;
            zeropage = (zero + page_mask) & ~page_mask;

            /*
             * This is different from the dl-load load...
             *  ld-linux.so.2 relies on the whole page being zero'ed
             */
            zeroend = (zeroend + page_mask) & ~page_mask;

            if (zeroend < zeropage)
            {
              /* All the extra data is in the last page of the segment.
                 We can just zero it.  */
              zeropage = zeroend;
            }

            if (zeropage > zero)
              {
                /* Zero the final part of the last page of the segment.  */
                if ((c->prot & PROT_WRITE) == 0)
                  {
                    /* Dag nab it.  */
                    wld_mprotect ((caddr_t) (zero & ~page_mask), page_size, c->prot|PROT_WRITE);
                  }
                wld_memset ((void *) zero, '\0', zeropage - zero);
                if ((c->prot & PROT_WRITE) == 0)
                  wld_mprotect ((caddr_t) (zero & ~page_mask), page_size, c->prot);
              }

            if (zeroend > zeropage)
              {
                /* Map the remaining zero pages in from the zero fill FD.  */
                caddr_t mapat;
                mapat = wld_mmap ((caddr_t) zeropage, zeroend - zeropage,
                                c->prot, MAP_ANON|MAP_PRIVATE|MAP_FIXED,
                                -1, 0);
              }
          }

        ++c;
      }

    if (l->l_phdr == NULL) fatal_error("no program header\n");

    l->l_phdr = (void *)((ElfW(Addr))l->l_phdr + l->l_addr);
    l->l_entry += l->l_addr;

    wld_close( fd );
}

static unsigned int elf_hash( const char *name )
{
    unsigned int hi, hash = 0;
    while (*name)
    {
        hash = (hash << 4) + (unsigned char)*name++;
        hi = hash & 0xf0000000;
        hash ^= hi;
        hash ^= hi >> 24;
    }
    return hash;
}

static unsigned int gnu_hash( const char* name )
{
    unsigned int h = 5381;
    while (*name) h = h * 33 + (unsigned char)*name++;
    return h;
}

/*
 * Find a symbol in the symbol table of the executable loaded
 */
static void *find_symbol( const ElfW(Phdr) *phdr, int num, char *var, int type )
{
    const ElfW(Dyn) *dyn = NULL;
    const ElfW(Phdr) *ph;
    const ElfW(Sym) *symtab = NULL;
    const Elf_Symndx *hashtab = NULL;
    const Elf32_Word *gnu_hashtab = NULL;
    const char *strings = NULL;
    Elf_Symndx idx;

    /* check the values */
#ifdef DUMP_SYMS
    wld_printf("%x %x\n", phdr, num );
#endif
    if( ( phdr == NULL ) || ( num == 0 ) )
    {
        wld_printf("could not find PT_DYNAMIC header entry\n");
        return NULL;
    }

    /* parse the (already loaded) ELF executable's header */
    for (ph = phdr; ph < &phdr[num]; ++ph)
    {
        if( PT_DYNAMIC == ph->p_type )
        {
            dyn = (void *) ph->p_vaddr;
            num = ph->p_memsz / sizeof (Elf32_Dyn);
            break;
        }
    }
    if( !dyn ) return NULL;

    while( dyn->d_tag )
    {
        if( dyn->d_tag == DT_STRTAB )
            strings = (const char*) dyn->d_un.d_ptr;
        if( dyn->d_tag == DT_SYMTAB )
            symtab = (const ElfW(Sym) *)dyn->d_un.d_ptr;
        if( dyn->d_tag == DT_HASH )
            hashtab = (const Elf_Symndx *)dyn->d_un.d_ptr;
        if( dyn->d_tag == DT_GNU_HASH )
	    gnu_hashtab = (const Elf32_Word *)dyn->d_un.d_ptr;
#ifdef DUMP_SYMS
        wld_printf("%x %x\n", dyn->d_tag, dyn->d_un.d_ptr );
#endif
        dyn++;
    }

    if( (!symtab) || (!strings) ) return NULL;

    if (gnu_hashtab)  /* new style hash table */
    {
	const unsigned int hash   = gnu_hash(var);
	const Elf32_Word nbuckets = gnu_hashtab[0];
	const Elf32_Word symbias  = gnu_hashtab[1];
	const Elf32_Word nwords   = gnu_hashtab[2];
	const ElfW(Addr) *bitmask = (const ElfW(Addr) *)(gnu_hashtab + 4);
	const Elf32_Word *buckets = (const Elf32_Word *)(bitmask + nwords);
	const Elf32_Word *chains  = buckets + nbuckets - symbias;

	if (!(idx = buckets[hash % nbuckets])) return NULL;
	do
	{
	    if ((chains[idx] & ~1u) == (hash & ~1u) &&
		symtab[idx].st_info == ELF32_ST_INFO( STB_GLOBAL, type ) &&
		!wld_strcmp( strings + symtab[idx].st_name, var ))
		goto found;
	} while (!(chains[idx++] & 1u));
    }
    else if (hashtab)  /* old style hash table */
    {
	const unsigned int hash   = elf_hash(var);
	const Elf_Symndx nbuckets = hashtab[0];
        const Elf_Symndx *buckets = hashtab + 2;
	const Elf_Symndx *chains  = buckets + nbuckets;

        for (idx = buckets[hash % nbuckets]; idx != STN_UNDEF; idx = chains[idx])
        {
	    if (symtab[idx].st_info == ELF32_ST_INFO( STB_GLOBAL, type ) &&
                !wld_strcmp( strings + symtab[idx].st_name, var ))
	    	goto found;
        }
    }
    return NULL;

found:
#ifdef DUMP_SYMS
    wld_printf("Found %s -> %x\n", strings + symtab[idx].st_name, symtab[idx].st_value );
#endif
    return (void *)symtab[idx].st_value;
}

static unsigned long wld_strtol16(const char *str, const char **endptr)
{
    const char *p;
    unsigned long result = 0;

    for (p = str; *p; p++)
    {
        if (*p >= '0' && *p <= '9') result = result * 16 + *p - '0';
        else if (*p >= 'a' && *p <= 'f') result = result * 16 + *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') result = result * 16 + *p - 'A' + 10;
        else goto done;
    }
done:
    if (endptr) *endptr = p;
    return result;
}

static void wineserver_reserve_addr(const char *str)
{
    unsigned long result = 0;

    result = wld_strtol16(str, NULL);
    if (result == 0) /* take this as 'unset'. really shouldn't put
                        shm at 0 anyhow. */
        return;

    /* no error checking done here.. */
    preload_info[3].addr = (void*)(result & ~page_mask);
    preload_server_info[0].addr = (void*)(result & ~page_mask);
}

static void wineserver_reserve_size(const char *str)
{
    unsigned long result = 0;
    unsigned long size = 0;

    result = wld_strtol16(str, NULL);

    if (result) size = ((result + page_size) & ~page_mask);

    /* no error checking done here.. */
    preload_info[3].size = size;
    preload_server_info[0].size = size;
}

/*
 *  preload_reserve
 *
 * Reserve a range specified in string format
 */
static void preload_reserve( const char *str )
{
    const char *p;
    unsigned long result = 0;
    void *start = NULL, *end = NULL;

    /* get first part */
    result = wld_strtol16(str, &p);
    if (*p != '\0')
    {
        if (*p != '-') goto error;
        start = (void*)(result & ~page_mask);
        result = wld_strtol16(p+1, &p);
        end = (void*)((result + page_mask) & ~page_mask);
    }
    else if (result) goto error; /* single '0' allowed */

    /* sanity checks */
    if (end <= start) start = end = NULL;
    else if ((char *)end > preloader_start &&
             (char *)start <= preloader_end)
    {
        wld_printf( "WINEPRELOADRESERVE range %x-%x overlaps preloader %x-%x\n",
                     start, end, preloader_start, preloader_end );
        start = end = NULL;
    }

    /* entry 2 is for the PE exe */
    preload_info[2].addr = start;
    preload_info[2].size = (char *)end - (char *)start;
    return;

error:
    fatal_error( "invalid WINEPRELOADRESERVE value '%s'\n", str );
}

/*
 *  is_in_preload_range
 *
 * Check if address of the given aux value is in one of the reserved ranges
 */
static int is_in_preload_range( const ElfW(auxv_t) *av, int type )
{
    int i;

    while (av->a_type != type && av->a_type != AT_NULL) av++;

    if (av->a_type == type)
    {
        for (i = 0; preload_info[i].size; i++)
        {
            if ((char *)av->a_un.a_val >= (char *)preload_info[i].addr &&
                (char *)av->a_un.a_val < (char *)preload_info[i].addr + preload_info[i].size)
                return 1;
        }
    }
    return 0;
}

static void set_legacy_va_layout(int on)
{
    int cur;
#ifndef ADDR_COMPAT_LAYOUT
#define ADDR_COMPAT_LAYOUT 0x0200000
#endif
    cur = wld_personality(-1);
    if (cur == -1) return;
    if (on) cur |= ADDR_COMPAT_LAYOUT;
    else cur &= ~ADDR_COMPAT_LAYOUT;
    wld_personality(cur);
}

static int str_is_preloadbin(char *s)
{
    char *bin = wld_strrchr(s, '/');
    if (!bin) bin = s;
    else bin++;
    if (wld_strcmp(bin, xstr(PRELOADER_BINARY_NAME)) == 0) return 1;
    return 0;
}

/*
 *  wld_start
 *
 *  Repeat the actions the kernel would do when loading a dynamically linked .so
 *  Load the binary and then its ELF interpreter.
 *  Note, we assume that the binary is a dynamically linked ELF shared object.
 */
void* wld_start( void **stack )
{
    int i, *pargc;
    int binary_arg = 1;
    char **argv, **p;
    char *interp, *reserve = NULL;
    ElfW(auxv_t) new_av[12], delete_av[3], *av;
    struct wld_link_map main_binary_map, ld_so_map;
    struct wine_preload_info **wine_main_preload_info;
    int server = 0;
    const char *binary_name;
    int need_restart = 0;

    pargc = *stack;
    argv = (char **)pargc + 1;
    if (*pargc < 2 && str_is_preloadbin(argv[0]))
        fatal_error( "Usage: %s wine_binary [args]\n", argv[0] );
    else if (!str_is_preloadbin(argv[0]))
        binary_arg = 0;

    /* skip over the parameters */
    p = argv + *pargc + 1;

    /* skip over the environment */
    while (*p)
    {
        static const char res[] = "WINEPRELOADRESERVE=";
        static const char isserver[] = "WINEPRELOADER_SERVER=";
        static const char shmres_addr[] = "WINEPRELOADER_SHMADDR=";
        static const char shmres_size[] = "WINEPRELOADER_SHMSIZE=";
        static const char set_va_legacy[] = "WINEPRELOADER_SETVALEGACY=";
        if (!wld_strncmp( *p, res, sizeof(res)-1 )) reserve = *p + sizeof(res) - 1;

        else if (!wld_strncmp(*p, isserver, sizeof(isserver)-1))
        {
            if (*(*p + sizeof(isserver) - 1) != '0') /* anything but '=0' means yes */
                server = 1;
        }

        else if (!wld_strncmp(*p, shmres_addr, sizeof(shmres_addr)-1))
        {
            const char *val = *p + sizeof(shmres_addr) - 1;
            wineserver_reserve_addr(val);
        }
        else if (!wld_strncmp(*p, shmres_size, sizeof(shmres_size)-1))
        {
            const char *val = *p + sizeof(shmres_size) - 1;
            wineserver_reserve_size(val);
        }

        /* should we set legacy layout and restart?
         * unfortunately after setting the legacy we require a
         * kernel space exec call to take effect.
         * Fun, hey?
         */
        else if (!wld_strncmp(*p, set_va_legacy, sizeof(set_va_legacy)-1))
        {
            /* the values of WINEPRELOADER_SETVALEGACY have the following effect:
             *   "yes" -> set va legacy layout to on, restart the preloader
             *   "no"  -> set va legacy layout to off, restart the preloader
             *   "0" (or anything else) -> ignore it
             */
            char *val = *p + sizeof(set_va_legacy) - 1;
            if (wld_strncmp(val, "yes", 3) == 0)
            {
                /* first set it to "0" - meaning done.
                 * Basically overwriting the env var value (from 'yes' to '0')
                 * so when we exec the preloader again, it will skip all this stuff.. */
                val[0] = '0';
                val[1] = '\0';

                set_legacy_va_layout(1);
                need_restart = 1;
            }
            else if (wld_strncmp(val, "no", 2) == 0)
            {
                val[0] = '0';
                val[1] = '\0';

                set_legacy_va_layout(0);
                need_restart = 1;
            }
            /* now the kernel space exec takes place next,
             * in the renaming thing */
        }
        p++;
    }

    /* this is the name of the binary we are going to launch */
    binary_name = wld_strrchr(argv[0], '/');
    if (binary_name) binary_name++;
    else binary_name = argv[0];

    /* now restart in order to set the argv[0] name properly.
     * This will also restart for setting the legacy_va_layout.
     * This method modifies it where ps uses it. */
    /* However, we only restart with the binary name as argv[0] if
     * the kernel supports changing the name with PR_SET_NAME.
     * 2.4 kernels (and presumably earlier) don't actually support
     * PR_SET_NAME. So we don't want to rename the binary.
     * The two methods used here are:
     *  * PR_SET_NAME changes the name reported by /proc/PID/stat (and status)
     *    this is what killall uses.
     *  * argv[0] is what 'ps'. So we only want to change the ps
     *    output if we can change what killall uses. (Unless we want to
     *    confuse boo^H^H^Hthe user.
     */
    /* SYS_prctl : PR_SET_NAME, sets the name - this method
     * modifies it in the kernel where it is used by killall. */
#ifndef PR_SET_NAME
# define PR_SET_NAME 15
#endif
    if (str_is_preloadbin(argv[0]) &&
        wld_prctl(PR_SET_NAME, (unsigned long)binary_name, 0, 0, 0) >= 0)
    {
        wld_execve(argv[0], argv + binary_arg, (argv + *pargc + 1));
        wld_exit(1);
    } else if (need_restart) {
        /* or if we just need a restart from changing the va_legacy.
         * (although really this is redundant cause that's
         * only used for 2.6...  but to be safe */
        wld_execve(argv[0], argv, (argv + *pargc + 1));
        wld_exit(1);
    }
    /* now try again..(this will only happen on first run if the syscall
     * failed or wasn't called or on the second run, where we still need
     * it). ignore failure.
     */
    wld_prctl(PR_SET_NAME, (unsigned long)binary_name, 0, 0, 0);

    av = (ElfW(auxv_t)*) (p+1);
    page_size = get_auxiliary( av, AT_PAGESZ, 4096 );
    page_mask = page_size - 1;

    preloader_start = (char *)_start - ((unsigned int)_start & page_mask);
    preloader_end = (char *)((unsigned int)(_end + page_mask) & ~page_mask);

#ifdef DUMP_AUX_INFO
    wld_printf( "stack = %x\n", *stack );
    for( i = 0; i < *pargc; i++ ) wld_printf("argv[%x] = %s\n", i, argv[i]);
    dump_auxiliary( av );
#endif

    /* reserve memory that Wine needs */
    if (reserve) preload_reserve( reserve );
    if (server)
    {
        for (i = 0; !(preload_server_info[i].info & PRELOAD_INFO_NULL); i++)
        {
            void *ret;
            if (!preload_server_info[i].size) continue;
            ret = wld_mmap( preload_server_info[i].addr, preload_server_info[i].size,
                      PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0 );
            if (ret == (void*)-1) /* mmap failed */
            {
                wld_printf("warning: mmap failed on reservation of %x\n",
                           preload_server_info[i].addr);
                preload_server_info[i].addr = NULL;
                preload_server_info[i].size = 0;
            }
        }
        using_preload_info = preload_server_info;
    }
    else
    {
        for (i = 0; !(preload_info[i].info & PRELOAD_INFO_NULL); i++)
        {
            void *ret;
            if (!preload_info[i].size) continue;
            ret = wld_mmap( preload_info[i].addr, preload_info[i].size,
                      PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0 );
            if (ret == (void*)-1 && i == 0) {
                /* Special case: if this is entry 0 (the DOS area) then try again,
                   but raise the floor to 64k */
                preload_info[i].addr += 0x10000;
                preload_info[i].size -= 0x10000;
                ret = wld_mmap( preload_info[i].addr, preload_info[i].size,
                                PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0 );
            }
            if (ret == (void*)-1) /* mmap failed */
            {
                wld_printf("warning: mmap failed on reservation of %x (%x)\n",
                           preload_info[i].addr,
                           preload_info[i].size);
                preload_info[i].addr = NULL;
                preload_info[i].size = 0;
            }
        }
        using_preload_info = preload_info;
    }

    /* load the main binary */
    map_so_lib( argv[binary_arg], &main_binary_map );

    /* load the ELF interpreter */
    interp = (char *)main_binary_map.l_addr + main_binary_map.l_interp;
    map_so_lib( interp, &ld_so_map );

    /* store pointer to the preload info into the appropriate main binary variable */
    wine_main_preload_info = find_symbol( main_binary_map.l_phdr, main_binary_map.l_phnum,
                                          "wine_main_preload_info", STT_OBJECT );
    if (wine_main_preload_info) *wine_main_preload_info = preload_info;
    else wld_printf( "wine_main_preload_info not found\n" );

#define SET_NEW_AV(n,type,val) new_av[n].a_type = (type); new_av[n].a_un.a_val = (val);
    SET_NEW_AV( 0, AT_PHDR, (unsigned long)main_binary_map.l_phdr );
    SET_NEW_AV( 1, AT_PHENT, sizeof(ElfW(Phdr)) );
    SET_NEW_AV( 2, AT_PHNUM, main_binary_map.l_phnum );
    SET_NEW_AV( 3, AT_PAGESZ, page_size );
    SET_NEW_AV( 4, AT_BASE, ld_so_map.l_addr );
    SET_NEW_AV( 5, AT_FLAGS, get_auxiliary( av, AT_FLAGS, 0 ) );
    SET_NEW_AV( 6, AT_ENTRY, main_binary_map.l_entry );
    SET_NEW_AV( 7, AT_UID, get_auxiliary( av, AT_UID, wld_getuid() ) );
    SET_NEW_AV( 8, AT_EUID, get_auxiliary( av, AT_EUID, wld_geteuid() ) );
    SET_NEW_AV( 9, AT_GID, get_auxiliary( av, AT_GID, wld_getgid() ) );
    SET_NEW_AV(10, AT_EGID, get_auxiliary( av, AT_EGID, wld_getegid() ) );
    SET_NEW_AV(11, AT_NULL, 0 );
#undef SET_NEW_AV

    i = 0;
    /* delete sysinfo values if addresses conflict */
    if (is_in_preload_range( av, AT_SYSINFO )) delete_av[i++].a_type = AT_SYSINFO;
    if (is_in_preload_range( av, AT_SYSINFO_EHDR )) delete_av[i++].a_type = AT_SYSINFO_EHDR;
    delete_av[i].a_type = AT_NULL;

    /* get rid of first argument(s) */
    pargc[binary_arg] = pargc[0] - binary_arg;
    *stack = pargc + binary_arg;

    set_auxiliary_values( av, new_av, delete_av, stack );

#ifdef DUMP_AUX_INFO
    wld_printf("new stack = %x\n", *stack);
    wld_printf("jumping to %x\n", ld_so_map.l_entry);
#endif

    return (void *)ld_so_map.l_entry;
}
