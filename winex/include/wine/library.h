/*
 * Definitions for the Wine library
 *
 * Copyright 2000 Alexandre Julliard
 */

#ifndef __WINE_WINE_LIBRARY_H
#define __WINE_WINE_LIBRARY_H

#include "config.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "winbase.h"

/* dll loading */

typedef void (*load_dll_callback_t)( void *, const char * );

extern void wine_dll_set_callback( load_dll_callback_t load );
extern void *wine_dll_load( const char *filename, char *error, int errorsize );
extern void *wine_dll_load_main_exe( const char *name, int search_path,
                                     char *error, int errorsize );
extern char *wine_dll_enum_load_path( int index );
extern void wine_dll_unload( void *handle );

/* build_native_dll_name(): builds a system specific library name from a windows DLL name
     (ie: 'kernel32.dll').  On success, returns the number of characters written to the 
     buffer <buf> not including the null terminator.  On failure, returns the required
     number of characters in the buffer including the null terminator.  To determine the
     required size of the buffer, call the function with the <buf> parameter set to NULL
     or <len> set to 0. */
size_t build_native_dll_name(const char *name, char *buf, size_t len);

/* build_dll_pathname(): builds a path and file name for the windows DLL named <name>
     (ie: 'kernel32.dll').  The name is appended to the <n>th DLL path in the search
     order list and returned in <buf>.  If <isNativeName> is non-zero, the string passed
     in <name> will be used directly assuming it is already a native library name.  If
     <isNativeName> is 0, the name will be converted to a native library name using the
     build_native_dll_name() function.  The length of the string will not exceed <len>
     characters.  If the buffer <buf> is not large enough, required length of the buffer,
     including the null terminator character, in characters will be returned.  To
     retrieve the maximum length of a DLL path segment for a given name, pass NULL the
     <buf> parameter and 0 for the <len> parameter.  In this case, the return value will
     be the maximum length, in characters, including the null terminator, of any path name
     that will be returned for the given name.  Returns 0 if <n> is larger than or equal
     to the number of DLL path segments.  Returns a the number of characters written to 
     <buf> not including the null terminator.  No guarantees are made that the file for 
     the created pathname actually exists. */
size_t build_dll_pathname(const char *name, int n, int isNativeName, char *buf, size_t len);


/* debugging */

extern void wine_dbg_add_option( const char *name, unsigned char set, unsigned char clear );

/* portability */

extern void *wine_anon_mmap( void *start, size_t size, int prot, int flags );

extern void wine_mmap_add_reserve(const void *addr, size_t size);
extern void wine_mmap_remove_reserve(const void *addr, size_t size);
extern int wine_mmap_is_reserved(const void *addr, size_t size);

/* errno support */
extern int* (*wine_errno_location)(void);
extern int* (*wine_h_errno_location)(void);

/* LDT management */

extern void wine_ldt_get_entry( unsigned short sel, LDT_ENTRY *entry );
extern int wine_ldt_set_entry( unsigned short sel, const LDT_ENTRY *entry );

/* Linux kernel thread area support */
unsigned short wine_ldt_alloc_os_supported_thread_area_selector( const void* base, DWORD size, unsigned char flags );
void wine_ldt_os_set_thread_area_for_thread( unsigned short sel, const void* base, DWORD size, unsigned char flags );
int wine_ldt_os_supported_thread_area_selector_free( unsigned short sel );

/* the local copy of the LDT */
extern struct __wine_ldt_copy
{
    void         *base[8192];  /* base address or 0 if entry is free   */
    unsigned long limit[8192]; /* limit in bytes or 0 if entry is free */
    unsigned char flags[8192]; /* flags (defined below) */
} wine_ldt_copy;

#define WINE_LDT_FLAGS_DATA      0x13  /* Data segment */
#define WINE_LDT_FLAGS_STACK     0x17  /* Stack segment */
#define WINE_LDT_FLAGS_CODE      0x1b  /* Code segment */
#define WINE_LDT_FLAGS_TYPE_MASK 0x1f  /* Mask for segment type */
#define WINE_LDT_FLAGS_32BIT     0x40  /* Segment is 32-bit (code or stack) */
#define WINE_LDT_FLAGS_ALLOCATED 0x80  /* Segment is allocated (no longer free) */

/* helper functions to manipulate the LDT_ENTRY structure */
inline static void wine_ldt_set_base( LDT_ENTRY *ent, const void *base )
{
    ent->BaseLow               = (WORD)(unsigned long)base;
    ent->HighWord.Bits.BaseMid = (BYTE)((unsigned long)base >> 16);
    ent->HighWord.Bits.BaseHi  = (BYTE)((unsigned long)base >> 24);
}
inline static void wine_ldt_set_limit( LDT_ENTRY *ent, unsigned int limit )
{
    if ((ent->HighWord.Bits.Granularity = (limit >= 0x100000))) limit >>= 12;
    ent->LimitLow = (WORD)limit;
    ent->HighWord.Bits.LimitHi = (limit >> 16);
}
inline static void *wine_ldt_get_base( const LDT_ENTRY *ent )
{
    return (void *)(ent->BaseLow |
                    (unsigned long)ent->HighWord.Bits.BaseMid << 16 |
                    (unsigned long)ent->HighWord.Bits.BaseHi << 24);
}
inline static unsigned int wine_ldt_get_limit( const LDT_ENTRY *ent )
{
    unsigned int limit = ent->LimitLow | (ent->HighWord.Bits.LimitHi << 16);
    if (ent->HighWord.Bits.Granularity) limit = (limit << 12) | 0xfff;
    return limit;
}
inline static void wine_ldt_set_flags( LDT_ENTRY *ent, unsigned char flags )
{
    ent->HighWord.Bits.Dpl         = 3;
    ent->HighWord.Bits.Pres        = 1;
    ent->HighWord.Bits.Type        = flags;
    ent->HighWord.Bits.Sys         = 0;
    ent->HighWord.Bits.Reserved_0  = 0;
    ent->HighWord.Bits.Default_Big = (flags & WINE_LDT_FLAGS_32BIT) != 0;
}
inline static unsigned char wine_ldt_get_flags( const LDT_ENTRY *ent )
{
    unsigned char ret = ent->HighWord.Bits.Type;
    if (ent->HighWord.Bits.Default_Big) ret |= WINE_LDT_FLAGS_32BIT;
    return ret;
}

#endif  /* __WINE_WINE_LIBRARY_H */
