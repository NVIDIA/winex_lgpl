/*
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Martin von Loewis
 * Copyright 1995, 1996, 1997 Alexandre Julliard
 * Copyright 1997 Eric Youngdale
 * Copyright 1999 Ulrich Weigand
 */

#ifndef __WINE_BUILD_H
#define __WINE_BUILD_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_DIRECT_H
# include <direct.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "wine/port.h"
#include "wine/winebuild.h"

#if !defined(HAVE_POPEN) && defined(HAVE__POPEN)
#define popen _popen
#endif

#if !defined(HAVE_PCLOSE) && defined(HAVE__PCLOSE)
#define pclose _pclose
#endif

#if !defined(HAVE_STRNCASECMP) && defined(HAVE__STRNICMP)
# define strncasecmp _strnicmp
#endif

#if !defined(HAVE_STRCASECMP) && defined(HAVE__STRICMP)
# define strcasecmp _stricmp
#endif

#define PUT_WORD(ptr, w)  (*(WORD *)(ptr) = (w))
#define PUT_LE_WORD(ptr, w) \
        do { ((BYTE *)(ptr))[0] = LOBYTE(w); \
             ((BYTE *)(ptr))[1] = HIBYTE(w); } while (0)
#define PUT_BE_WORD(ptr, w) \
        do { ((BYTE *)(ptr))[1] = LOBYTE(w); \
             ((BYTE *)(ptr))[0] = HIBYTE(w); } while (0)

#if defined(ALLOW_UNALIGNED_ACCESS)
#define PUT_UA_WORD(ptr, w)  PUT_WORD(ptr, w)
#elif defined(WORDS_BIGENDIAN)
#define PUT_UA_WORD(ptr, w)  PUT_BE_WORD(ptr, w)
#else
#define PUT_UA_WORD(ptr, w)  PUT_LE_WORD(ptr, w)
#endif

#ifdef NEED_UNDERSCORE_PREFIX
# define PREFIX "_"
#else
# define PREFIX
#endif

#ifdef HAVE_ASM_STRING
# define STRING ".string"
#else
# define STRING ".asciz"
#endif

#if defined(__GNUC__) && !defined(__svr4__) && !defined(__APPLE__)
# define USE_STABS
#else
# undef USE_STABS
#endif

typedef enum
{
    TYPE_VARIABLE,     /* variable */
    TYPE_PASCAL_16,    /* pascal function with 16-bit return (Win16) */
    TYPE_PASCAL,       /* pascal function with 32-bit return (Win16) */
    TYPE_ABS,          /* absolute value (Win16) */
    TYPE_STUB,         /* unimplemented stub */
    TYPE_STDCALL,      /* stdcall function (Win32) */
    TYPE_CDECL,        /* cdecl function (Win32) */
    TYPE_VARARGS,      /* varargs function (Win32) */
    TYPE_EXTERN,       /* external symbol (Win32) */
    TYPE_FORWARD,      /* forwarded function (Win32) */
    TYPE_NBTYPES
} ORD_TYPE;

typedef enum
{
    SPEC_INVALID,
    SPEC_WIN16,
    SPEC_WIN32
} SPEC_TYPE;

typedef enum
{
    ARG_TYPE_DWORD = 0,     /* relay prints argument as a DWORD (default). */
    ARG_TYPE_STR =   1,     /* relay prints argument as an ASCII string. */
    ARG_TYPE_WSTR =  2,     /* relay prints argument as a wide string. */
    ARG_TYPE_QWORD = 3,     /* relay prints argument as a 64-bit QWORD. */
} ARG_TYPE;

typedef enum
{
    SPEC_MODE_DLL,
    SPEC_MODE_GUIEXE,
    SPEC_MODE_CUIEXE,
    SPEC_MODE_GUIEXE_UNICODE,
    SPEC_MODE_CUIEXE_UNICODE
} SPEC_MODE;

typedef struct
{
    int n_values;
    int *values;
} ORD_VARIABLE;

typedef struct
{
    /* FIXME!! this should have a size of 15 since the relay outputs use a 32-bit mask variable
               where each argument occupies 2 bits, and the high order bit is reserved to mark
               a 64-bit return type.  This will not become a problem unless a function with more
               than 15 parameters has a 'str' or 'wstr' parameter at index 15 or higher. */
    char arg_types[21];
} ORD_FUNCTION;

typedef struct
{
    int value;
} ORD_ABS;

typedef struct
{
    ORD_TYPE    type;
    int         ordinal;
    int         offset;
    int         lineno;
    int         flags;
    char       *export_name;
    char       *name;
    char       *link_name;
    union
    {
        ORD_VARIABLE   var;
        ORD_FUNCTION   func;
        ORD_ABS        abs;
    } u;
} ORDDEF;

enum target_type
{
    PLATFORM_UNSPECIFIED,
    PLATFORM_WINDOWS
};

  /* Offset of a structure field relative to the start of the struct */
#define STRUCTOFFSET(type,field) ((int)&((type *)0)->field)

  /* Offset of register relative to the start of the CONTEXT struct */
#define CONTEXTOFFSET(reg)  STRUCTOFFSET(CONTEXT86,reg)

  /* Offset of register relative to the start of the STACK16FRAME struct */
#define STACK16OFFSET(reg)  STRUCTOFFSET(STACK16FRAME,reg)

  /* Offset of register relative to the start of the STACK32FRAME struct */
#define STACK32OFFSET(reg)  STRUCTOFFSET(STACK32FRAME,reg)

  /* Offset of the stack pointer relative to %fs:(0) */
#define STACKOFFSET (STRUCTOFFSET(TEB, WOW32Reserved))


#define MAX_ORDINALS  65535

/* global functions */

extern void *xmalloc (size_t size);
extern void *xrealloc (void *ptr, size_t size);
extern char *xstrdup( const char *str );
extern char *strupper(char *s);
extern void fatal_error( const char *msg, ... );
extern void fatal_perror( const char *msg, ... );
extern void warning( const char *msg, ... );
extern void dump_bytes( FILE *outfile, const unsigned char *data, int len,
                        const char *label, int constant );
extern int get_alignment(int alignBoundary);
extern void output_function_name (FILE *outfile, const char *pName);

extern void add_import_dll( const char *name, int delay );
extern void add_ignore_symbol( const char *name );
extern int resolve_imports( void );
extern int output_imports( FILE *outfile );
extern void load_res32_file( const char *name );
extern int output_resources( FILE *outfile );
extern void load_res16_file( const char *name );
extern int output_res16_data( FILE *outfile );
extern int output_res16_directory( unsigned char *buffer );

extern void BuildGlue( FILE *outfile, FILE *infile );
extern void BuildRelays16( FILE *outfile );
extern void BuildRelays32( FILE *outfile );
extern void BuildSpec16File( FILE *outfile );
extern void BuildSpec32File( FILE *outfile );
extern void BuildDef32File( FILE *outfile );
extern SPEC_TYPE ParseTopLevel( FILE *file );

extern int find_debugchannel( const char *name );


/* global variables */

extern int current_line;
extern int nb_entry_points;
extern int nb_names;
extern int Base;
extern int Limit;
extern int DLLHeapSize;
extern int UsePIC;
extern int debugging;
extern int stack_size;
extern int nb_debug_channels;
extern int nb_lib_paths;
extern int delay_initialization;
extern int size_of_image;

extern char DLLName[80];
extern char DLLFileName[80];
extern char owner_name[80];
extern char *init_func;
extern const char *input_file_name;
extern const char *output_file_name;
extern char **debug_channels;
extern char **lib_path;
extern enum target_type target_compiler;

extern ORDDEF *EntryPoints[MAX_ORDINALS];
extern ORDDEF *Ordinals[MAX_ORDINALS];
extern ORDDEF *Names[MAX_ORDINALS];
extern SPEC_MODE SpecMode;

extern int need_force_init;
#endif  /* __WINE_BUILD_H */
