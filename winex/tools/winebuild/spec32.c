/*
 * 32-bit spec files
 *
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Martin von Loewis
 * Copyright 1995, 1996, 1997 Alexandre Julliard
 * Copyright 1997 Eric Youngdale
 * Copyright 1999 Ulrich Weigand
 * Copyright (c) 2002-2015 NVIDIA CORPORATION. All rights reserved.
 */

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "winbase.h"
#include "wine/exception.h"
#include "build.h"

/* Make mingw-compatible .def files */
#define NEED_STDCALL_DECORATION 1

static int string_compare( const void *ptr1, const void *ptr2 )
{
    const char * const *str1 = ptr1;
    const char * const *str2 = ptr2;
    return strcmp( *str1, *str2 );
}


/*******************************************************************
 *         make_internal_name
 *
 * Generate an internal name for an entry point. Used for stubs etc.
 */
static const char *make_internal_name( const ORDDEF *odp, const char *prefix )
{
    static char buffer[256];
    if (odp->name[0])
    {
        char *p;
        sprintf( buffer, "__wine_%s_%s_%s", prefix, DLLName, odp->name );
        /* make sure name is a legal C identifier */
        for (p = buffer; *p; p++) if (!isalnum(*p) && *p != '_') break;
        if (!*p) return buffer;
    }
    sprintf( buffer, "__wine_%s_%s_%d", prefix, DLLName, odp->ordinal );
    return buffer;
}

/*******************************************************************
 *         AssignOrdinals
 *
 * Assign ordinals to all entry points.
 */
static void AssignOrdinals(void)
{
    int i, ordinal;

    if ( !nb_names ) return;

    /* start assigning from Base, or from 1 if no ordinal defined yet */
    if (Base == MAX_ORDINALS) Base = 1;
    for (i = 0, ordinal = Base; i < nb_names; i++)
    {
        if (Names[i]->ordinal != -1) continue;  /* already has an ordinal */
        while (Ordinals[ordinal]) ordinal++;
        if (ordinal >= MAX_ORDINALS)
        {
            current_line = Names[i]->lineno;
            fatal_error( "Too many functions defined (max %d)\n", MAX_ORDINALS );
        }
        Names[i]->ordinal = ordinal;
        Ordinals[ordinal] = Names[i];
    }
    if (ordinal > Limit) Limit = ordinal;
}


/*******************************************************************
 *         output_debug
 *
 * Output the debug channels.
 */
static int output_debug( FILE *outfile )
{
    int i;

    if (!nb_debug_channels) return 0;
    qsort( debug_channels, nb_debug_channels, sizeof(debug_channels[0]), string_compare );

#if defined(__APPLE__)
    fprintf(outfile, "\nasm(\".data\\n\\t\");\n");
#endif

    /* Format:
       byte 0: bitmask of default enabled dbg levels (eg, 3 would be __WINE_DBCL_FIXME and __WINE_DBCL_ERR)
       byte 1: has this channel been changed (eg, via --debugmsg), or is it at default value?
       byte2+: channel name */
    for (i = 0; i < nb_debug_channels; i++)
        fprintf( outfile, "char __wine_dbch_%s[] = \"\\000\\000%s\";\n",
                 debug_channels[i], debug_channels[i] );

    fprintf( outfile, "\nstatic char * const debug_channels[%d] =\n{\n", nb_debug_channels );
    for (i = 0; i < nb_debug_channels; i++)
    {
        fprintf( outfile, "    __wine_dbch_%s", debug_channels[i] );
        if (i < nb_debug_channels - 1) fprintf( outfile, ",\n" );
    }
    fprintf( outfile, "\n};\n\n" );
    fprintf( outfile, "static void *debug_registration;\n\n" );

    return nb_debug_channels;
}


/*******************************************************************
 *         output_exports
 *
 * Output the export table for a Win32 module.
 */
static int output_exports( FILE *outfile, int nr_exports )
{
    int i, fwd_size = 0, total_size = 0;
    char *p;

    if (!nr_exports) return 0;

    fprintf( outfile, "asm(\".data\\n\"\n" );
    fprintf( outfile, "    \"\\t.align %d\\n\"\n", get_alignment(4) );
    fprintf( outfile, "    \"" PREFIX "__wine_spec_exports:\\n\"\n" );

    /* export directory header */

    fprintf( outfile, "    \"\\t.long 0\\n\"\n" );                 /* Characteristics */
    fprintf( outfile, "    \"\\t.long 0\\n\"\n" );                 /* TimeDateStamp */
    fprintf( outfile, "    \"\\t.long 0\\n\"\n" );                 /* MajorVersion/MinorVersion */
    fprintf( outfile, "    \"\\t.long " PREFIX "dllname\\n\"\n" ); /* Name */
    fprintf( outfile, "    \"\\t.long %d\\n\"\n", Base );          /* Base */
    fprintf( outfile, "    \"\\t.long %d\\n\"\n", nr_exports );    /* NumberOfFunctions */
    fprintf( outfile, "    \"\\t.long %d\\n\"\n", nb_names );      /* NumberOfNames */
    fprintf( outfile, "    \"\\t.long __wine_spec_exports_funcs\\n\"\n" ); /* AddressOfFunctions */
    if (nb_names)
    {
        fprintf( outfile, "    \"\\t.long __wine_spec_exp_name_ptrs\\n\"\n" );     /* AddressOfNames */
        fprintf( outfile, "    \"\\t.long __wine_spec_exp_ordinals\\n\"\n" );  /* AddressOfNameOrdinals */
    }
    else
    {
        fprintf( outfile, "    \"\\t.long 0\\n\"\n" );  /* AddressOfNames */
        fprintf( outfile, "    \"\\t.long 0\\n\"\n" );  /* AddressOfNameOrdinals */
    }
    total_size += 10 * sizeof(int);

    /* output the function pointers */

    fprintf( outfile, "    \"__wine_spec_exports_funcs:\\n\"\n" );
    for (i = Base; i <= Limit; i++)
    {
        ORDDEF *odp = Ordinals[i];
        if (!odp) fprintf( outfile, "    \"\\t.long 0\\n\"\n" );
        else switch(odp->type)
        {
        case TYPE_EXTERN:
            fprintf( outfile, "    \"\\t.long " PREFIX "%s\\n\"\n", odp->link_name );
            break;
        case TYPE_STDCALL:
        case TYPE_VARARGS:
        case TYPE_CDECL:
            fprintf( outfile, "    \"\\t.long " PREFIX "%s\\n\"\n",
                     (odp->flags & FLAG_REGISTER) ? make_internal_name(odp,"regs") : odp->link_name );
            break;
        case TYPE_STUB:
            fprintf( outfile, "    \"\\t.long " PREFIX "%s\\n\"\n", make_internal_name( odp, "stub" ) );
            break;
        case TYPE_VARIABLE:
            fprintf( outfile, "    \"\\t.long " PREFIX "%s\\n\"\n", make_internal_name( odp, "var" ) );
            break;
        case TYPE_FORWARD:
            fprintf( outfile, "    \"\\t.long __wine_spec_forwards+%d\\n\"\n", fwd_size );
            fwd_size += strlen(odp->link_name) + 1;
            break;
        default:
            assert(0);
        }
    }
    total_size += (Limit - Base + 1) * sizeof(int);

    if (nb_names)
    {
        /* output the function name pointers */

        int namepos = 0;

        fprintf( outfile, "    \"__wine_spec_exp_name_ptrs:\\n\"\n" );
        for (i = 0; i < nb_names; i++)
        {
            fprintf( outfile, "    \"\\t.long __wine_spec_exp_names+%d\\n\"\n", namepos );
            namepos += strlen(Names[i]->export_name) + 1;
        }
        total_size += nb_names * sizeof(int);

        /* output the function names */
#if defined(__APPLE__)
        fprintf( outfile, "    \"\\t.text\\n\"\n" );
#else
        fprintf( outfile, "    \"\\t.text 1\\n\"\n" );
#endif
        fprintf( outfile, "    \"__wine_spec_exp_names:\\n\"\n" );
        for (i = 0; i < nb_names; i++)
            fprintf( outfile, "    \"\\t" STRING " \\\"%s\\\"\\n\"\n", Names[i]->export_name );
        fprintf( outfile, "    \"\\t.data\\n\"\n" );

        /* output the function ordinals */

        fprintf( outfile, "    \"__wine_spec_exp_ordinals:\\n\"\n" );
        for (i = 0; i < nb_names; i++)
        {
            fprintf( outfile, "    \"\\t.short %d\\n\"\n", Names[i]->ordinal - Base );
        }
        total_size += nb_names * sizeof(short);
        if (nb_names % 2)
        {
            fprintf( outfile, "    \"\\t.short 0\\n\"\n" );
            total_size += sizeof(short);
        }
    }

    /* output forward strings */

    if (fwd_size)
    {
        fprintf( outfile, "    \"__wine_spec_forwards:\\n\"\n" );
        for (i = Base; i <= Limit; i++)
        {
            ORDDEF *odp = Ordinals[i];
            if (odp && odp->type == TYPE_FORWARD)
                fprintf( outfile, "    \"\\t" STRING " \\\"%s\\\"\\n\"\n", odp->link_name );
        }
        fprintf( outfile, "    \"\\t.align %d\\n\"\n", get_alignment(4) );
        total_size += (fwd_size + 3) & ~3;
    }

    /* output relays */

#if 0
    /* Still needed even if debugging disabled */
    if (debugging)
    {
#endif
        for (i = Base; i <= Limit; i++)
        {
            ORDDEF *odp = Ordinals[i];
            unsigned int j, args = 0, mask = 0;
            const char *name;

            /* skip non-existent entry points */
            if (!odp) goto ignore;
            /* skip non-functions */
            if ((odp->type != TYPE_STDCALL) && (odp->type != TYPE_CDECL)) goto ignore;
            /* skip norelay entry points */
            if (odp->flags & FLAG_NORELAY) goto ignore;

            for (j = 0; odp->u.func.arg_types[j]; j++)
            {
                /* argument is a pointer to an ASCII string => mark it as a 32-bit string pointer. */
                if (odp->u.func.arg_types[j] == 't')
                {
                    mask |= ARG_TYPE_STR << (j * 2);
                    args += sizeof(int);
                }

                /* argument is a pointer to a wide string => mark it as a 32-bit wide string pointer. */
                else if (odp->u.func.arg_types[j] == 'W')
                {
                    mask |= ARG_TYPE_WSTR << (j * 2);
                    args += sizeof(int);
                }

                /* argument is a long long or double value => mark it as a 64-bit immediate value. */
                else if (odp->u.func.arg_types[j] == 'L' || odp->u.func.arg_types[j] == 'D')
                {
                    mask |= ARG_TYPE_QWORD << (j * 2);
                    args += sizeof(long long);
                }

                /* argument is a long value => mark it as a 32-bit immediate value. */
                else
                    args += sizeof(int);
            }
            if ((odp->flags & FLAG_RET64) && (j < 16)) mask |= 0x80000000;

            name = odp->link_name;
            if (odp->flags & FLAG_REGISTER)
            {
                name = make_internal_name( odp, "regs" );
                args |= 0x8000;
            }

            if (*odp->name && !strchr(odp->name, '?'))
            {
#ifdef USE_STABS
                fprintf( outfile, " \"\\t.stabs \\\"" PREFIX "__wine_call_%s:F1\\\",36,0,0," PREFIX "__wine_call_%s\\n\"\n", odp->name, odp->name);
#endif
#ifdef TYPE_ASM_SUPPORTED
                fprintf( outfile, " \"\\t.type " PREFIX "__wine_call_%s,@function\\n\"\n", odp->name);
#elif defined(NEED_TYPE_IN_DEF)
                fprintf( outfile, " \"\\t.def " PREFIX "__wine_call_%s; .scl 2; .type 32; .endef\\n\"\n", odp->name );
#endif
                fprintf( outfile, " \".globl " PREFIX "__wine_call_%s\\n\"\n", odp->name);
                fprintf( outfile, " \"\\t" PREFIX "__wine_call_%s:\\n\"\n", odp->name);
            }

            /* Layout from relay386.c
               typedef struct
               {
                  BYTE          nops[8];
                  BYTE          call;
                  DWORD         callfrom32 WINE_PACKED;
                  BYTE          ret;
                  WORD          args;
                  void         *orig;
                  DWORD         argtypes;
                  DWORD         flags;
               } DEBUG_ENTRY_POINT;

               We store 0x03 in 'call' as our magic identifier.
               'call' and 'callfrom32' will be overwritten at load-time in RELAY_SetupDLL(), and are
               thus not filled in here to avoid linker warnings on Leopard
               'nops' is to provide some "safe" code which copy protection systems can replace/hook
               without causing problems.
            */

            switch(odp->type)
            {
            case TYPE_STDCALL:
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\t.long 0x03\\n\"\n" );
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tret $0x%04x\\n\"\n", args );
                fprintf( outfile, "    \"\\t.long " PREFIX "%s,0x%08x\\n\"\n", name, mask );
                fprintf( outfile, "    \"\\t.long 0x%08x\\n\"\n", odp->flags );
                break;
            case TYPE_CDECL:
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\t.long 0x03\\n\"\n" );
                fprintf( outfile, "    \"\\tnop\\n\"\n") ;
                fprintf( outfile, "    \"\\tret\\n\"\n" );
                fprintf( outfile, "    \"\\t.short 0x%04x\\n\"\n", args );
                fprintf( outfile, "    \"\\t.long " PREFIX "%s,0x%08x\\n\"\n", name, mask );
                fprintf( outfile, "    \"\\t.long 0x%08x\\n\"\n", odp->flags );
                break;
            default:
                assert(0);
            }
            continue;

        ignore:
            fprintf( outfile, "    \"\\t.long 0,0,0,0,0,0,0\\n\"\n" );
        }
#if 0
    }
#endif


    /* output __wine_dllexport symbols */

    for (i = 0; i < nb_names; i++)
    {
        if (Names[i]->flags & FLAG_NOIMPORT) continue;
        /* check for invalid characters in the name */
        for (p = Names[i]->name; *p; p++)
            if (!isalnum(*p) && *p != '_' && *p != '.') break;
        if (*p) continue;
        fprintf( outfile, "    \"\\t.globl " PREFIX "__wine_dllexport_%s_%s\\n\"\n",
                 DLLName, Names[i]->name );
        fprintf( outfile, "    \"" PREFIX "__wine_dllexport_%s_%s:\\n\"\n",
                 DLLName, Names[i]->name );
    }
    fprintf( outfile, "    \"\\t.long 0xffffffff\\n\"\n" );

    /* output variables */

    for (i = 0; i < nb_entry_points; i++)
    {
        ORDDEF *odp = EntryPoints[i];
        if (odp->type == TYPE_VARIABLE)
        {
            int j;
            fprintf( outfile, "    \"" PREFIX "%s:\\n\"\n", make_internal_name( odp, "var" ) );
            fprintf( outfile, "    \"\\t.long " );
            for (j = 0; j < odp->u.var.n_values; j++)
            {
                fprintf( outfile, "0x%08x", odp->u.var.values[j] );
                if (j < odp->u.var.n_values-1) fputc( ',', outfile );
            }
            fprintf( outfile, "\\n\"\n" );
        }
    }

    fprintf( outfile, "    \"\\t.text\\n\"\n" );
    fprintf( outfile, "    \"\\t.align %d\\n\"\n", get_alignment(4) );
    fprintf( outfile, ");\n\n" );

    return total_size;
}


/*******************************************************************
 *         output_stub_funcs
 *
 * Output the functions for stub entry points
*/
static void output_stub_funcs( FILE *outfile )
{
    int i;

    for (i = 0; i < nb_entry_points; i++)
    {
        ORDDEF *odp = EntryPoints[i];
        if (odp->type != TYPE_STUB) continue;
        fprintf( outfile, "#ifdef __GNUC__\n" );
        fprintf( outfile, "static void __wine_unimplemented( const char *func ) WINE_NORETURN;\n" );
        fprintf( outfile, "#endif\n\n" );
        fprintf( outfile, "struct exc_record {\n" );
        fprintf( outfile, "  unsigned int code, flags;\n" );
        fprintf( outfile, "  void *rec, *addr;\n" );
        fprintf( outfile, "  unsigned int params;\n" );
        fprintf( outfile, "  const void *info[15];\n" );
        fprintf( outfile, "};\n\n" );
        fprintf( outfile, "extern void __stdcall RtlRaiseException( struct exc_record * );\n\n" );
        fprintf( outfile, "static void __wine_unimplemented( const char *func )\n{\n" );
        fprintf( outfile, "  struct exc_record rec;\n" );
        fprintf( outfile, "  rec.code    = 0x%08x;\n", EXCEPTION_WINE_STUB );
        fprintf( outfile, "  rec.flags   = %d;\n", EH_NONCONTINUABLE );
        fprintf( outfile, "  rec.rec     = 0;\n" );
        fprintf( outfile, "  rec.params  = 2;\n" );
        fprintf( outfile, "  rec.info[0] = dllname;\n" );
        fprintf( outfile, "  rec.info[1] = func;\n" );
        fprintf( outfile, "#ifdef __GNUC__\n" );
        fprintf( outfile, "  rec.addr = __builtin_return_address(1);\n" );
        fprintf( outfile, "#else\n" );
        fprintf( outfile, "  rec.addr = 0;\n" );
        fprintf( outfile, "#endif\n" );
        fprintf( outfile, "  for (;;) RtlRaiseException( &rec );\n}\n\n" );
        break;
    }

    for (i = 0; i < nb_entry_points; i++)
    {
        ORDDEF *odp = EntryPoints[i];
        if (odp->type != TYPE_STUB) continue;
        fprintf( outfile, "void %s(void) ", make_internal_name( odp, "stub" ) );
        if (odp->name[0])
            fprintf( outfile, "{ __wine_unimplemented(\"%s\"); }\n", odp->name );
        else
            fprintf( outfile, "{ __wine_unimplemented(\"%d\"); }\n", odp->ordinal );
    }
}


/*******************************************************************
 *         BuildSpec32File
 *
 * Build a Win32 C file from a spec file.
 */
void BuildSpec32File( FILE *outfile )
{
    int exports_size = 0, image_size = 0;
    int nr_exports, nr_imports, nr_resources, nr_debug;
    int characteristics, subsystem;
    DWORD page_size;
    char upcase_dllname[sizeof (DLLName)];
    int i = 0;

    char DeferredInitFunctionName[666];
    char BuiltinEntryPointName[666];

    sprintf( BuiltinEntryPointName,    "__wine_%s_entry_point", DLLName );
    sprintf( DeferredInitFunctionName, "__wine_%s_deferred_init", DLLName );

    while (DLLName[i])
    {
        upcase_dllname[i] = toupper (DLLName[i]);
        i++;
    }
    upcase_dllname[i] = '\0';

#ifdef HAVE_GETPAGESIZE
    page_size = getpagesize();
#elif defined(__svr4__)
    page_size = sysconf(_SC_PAGESIZE);
#elif defined(_WINDOWS)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        page_size = si.dwPageSize;
    }
#else
#   error Cannot get the page size on this platform
#endif

    AssignOrdinals();
    nr_exports = Base <= Limit ? Limit - Base + 1 : 0;

    resolve_imports();

    fprintf( outfile, "/* File generated automatically from %s; do not edit! */\n\n",
             input_file_name );

    fprintf( outfile, "#include \"wine/compiler_defines.h\"\n\n" );

    /* Reserve some space for the PE header */
#if defined(__APPLE__)
    /* This is needed to support Mac OS X. These three DLLs have to be built and handled differently,
     * due to the way libraries are handled on that platform. */
    if (strstr (input_file_name, "gdi32.spec") ||
        strstr (input_file_name, "user32.spec") ||
        strstr (input_file_name, "kernel32.spec") ||
        strstr (input_file_name, "comdlg32.spec"))
    {
        fprintf( outfile, "#include \"winnt.h\"\n");
    }

#endif

    fprintf( outfile, "extern char pe_header[];\n" );
    fprintf( outfile, "#ifndef __GNUC__\n" );
    fprintf( outfile, "static void __asm__dummy_header(void) {\n" );
    fprintf( outfile, "#endif\n" );
#if defined(__APPLE__)
    fprintf( outfile, "asm(\".text\\n\\t\"\n" );
    fprintf( outfile, "    \".align %d\\n\"\n", get_alignment(page_size) );
    fprintf( outfile, "    \"" PREFIX "pe_header:\\t.fill %u,1,0\\n\\t\");\n", page_size );
#else
    fprintf( outfile, "asm(\".section \\\".text\\\"\\n\\t\"\n" );
    fprintf( outfile, "    \".align %d\\n\"\n", get_alignment(page_size) );
    fprintf( outfile, "    \"" PREFIX "pe_header:\\t.skip %ld\\n\\t\");\n", page_size );
#endif
    fprintf( outfile, "#ifndef __GNUC__\n" );
    fprintf( outfile, "}\n" );
    fprintf( outfile, "#endif\n" );

    fprintf( outfile, "#if defined( __GNUC__ )\n" );
    fprintf( outfile, "static const char dllname[] WINE_USED = \"%s.dll\";\n", upcase_dllname );
    fprintf( outfile, "#else\n" );
    fprintf( outfile, "const char dllname[] = \"%s.dll\";\n", upcase_dllname );
    fprintf( outfile, "#endif\n\n" );

    fprintf( outfile, "extern int __wine_spec_exports[];\n\n" );

#ifdef __i386__
    fprintf( outfile, "#define __stdcall __attribute__((__stdcall__))\n\n" );
#else
    fprintf( outfile, "#define __stdcall\n\n" );
#endif

    if (nr_exports)
    {
        /* Output the stub functions */

        output_stub_funcs( outfile );

        fprintf( outfile, "#ifndef __GNUC__\n" );
        fprintf( outfile, "static void __asm__dummy(void) {\n" );
        fprintf( outfile, "#endif /* !defined(__GNUC__) */\n" );

        /* Output the exports and relay entry points */

        exports_size = output_exports( outfile, nr_exports );

        fprintf( outfile, "#ifndef __GNUC__\n" );
        fprintf( outfile, "}\n" );
        fprintf( outfile, "#endif /* !defined(__GNUC__) */\n" );
    }

    /* Output the DLL imports */

    nr_imports = output_imports( outfile );

    /* Output the resources */

    nr_resources = output_resources( outfile );

    /* Output the debug channels */

    nr_debug = output_debug( outfile );

    /* Output LibMain function */

    characteristics = subsystem = 0;



    switch(SpecMode)
    {
    case SPEC_MODE_DLL:
        if (init_func && !delay_initialization) fprintf( outfile, "extern void %s();\n", init_func );
        characteristics = IMAGE_FILE_DLL;
        break;
    case SPEC_MODE_GUIEXE:
        if (!init_func) init_func = "WinMain";
        fprintf( outfile,
                 "\ntypedef struct {\n"
                 "    unsigned int cb;\n"
                 "    char *lpReserved, *lpDesktop, *lpTitle;\n"
                 "    unsigned int dwX, dwY, dwXSize, dwYSize;\n"
                 "    unsigned int dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;\n"
                 "    unsigned short wShowWindow, cbReserved2;\n"
                 "    char *lpReserved2;\n"
                 "    void *hStdInput, *hStdOutput, *hStdError;\n"
                 "} STARTUPINFOA;\n"
                 "int _ARGC;\n"
                 "char **_ARGV;\n"
                 "extern int __stdcall %s(void *,void *,char *,int);\n"
                 "extern char * __stdcall GetCommandLineA(void);\n"
                 "extern void * __stdcall GetModuleHandleA(char *);\n"
                 "extern void __stdcall GetStartupInfoA(STARTUPINFOA *);\n"
                 "extern void __stdcall ExitProcess(unsigned int);\n"
                 "static void __wine_exe_main(void)\n"
                 "{\n"
                 "    extern int __wine_get_main_args( char ***argv );\n"
                 "    STARTUPINFOA info;\n"
                 "    char *cmdline = GetCommandLineA();\n"
                 "    if (*cmdline == '\"')\n"
                 "    {\n"
                 "        cmdline++;\n"
                 "        while (*cmdline && *cmdline != '\"') cmdline++;\n"
                 "    }\n"
                 "    while (*cmdline && *cmdline != ' ') cmdline++;\n"
                 "    if (*cmdline) cmdline++;\n"
                 "    GetStartupInfoA( &info );\n"
                 "    if (!(info.dwFlags & 1)) info.wShowWindow = 1;\n"
                 "    _ARGC = __wine_get_main_args( &_ARGV );\n"
                 "    ExitProcess( %s( GetModuleHandleA(0), 0, cmdline, info.wShowWindow ) );\n"
                 "}\n\n", init_func, init_func );
        init_func = "__wine_exe_main";
        subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
        break;
    case SPEC_MODE_GUIEXE_UNICODE:
        if (!init_func) init_func = "WinMain";
        fprintf( outfile,
                 "\ntypedef unsigned short WCHAR;\n"
                 "typedef struct {\n"
                 "    unsigned int cb;\n"
                 "    char *lpReserved, *lpDesktop, *lpTitle;\n"
                 "    unsigned int dwX, dwY, dwXSize, dwYSize;\n"
                 "    unsigned int dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;\n"
                 "    unsigned short wShowWindow, cbReserved2;\n"
                 "    char *lpReserved2;\n"
                 "    void *hStdInput, *hStdOutput, *hStdError;\n"
                 "} STARTUPINFOA;\n"
                 "int _ARGC;\n"
                 "WCHAR **_ARGV;\n"
                 "extern int __stdcall %s(void *,void *,char *,int);\n"
                 "extern char * __stdcall GetCommandLineA(void);\n"
                 "extern void * __stdcall GetModuleHandleA(char *);\n"
                 "extern void __stdcall GetStartupInfoA(STARTUPINFOA *);\n"
                 "extern void __stdcall ExitProcess(unsigned int);\n"
                 "static void __wine_exe_main(void)\n"
                 "{\n"
                 "    extern int __wine_get_wmain_args( WCHAR ***argv );\n"
                 "    STARTUPINFOA info;\n"
                 "    char *cmdline = GetCommandLineA();\n"
                 "    if (*cmdline == '\"')\n"
                 "    {\n"
                 "        cmdline++;\n"
                 "        while (*cmdline && *cmdline != '\"') cmdline++;\n"
                 "    }\n"
                 "    while (*cmdline && *cmdline != ' ') cmdline++;\n"
                 "    if (*cmdline) cmdline++;\n"
                 "    GetStartupInfoA( &info );\n"
                 "    if (!(info.dwFlags & 1)) info.wShowWindow = 1;\n"
                 "    _ARGC = __wine_get_wmain_args( &_ARGV );\n"
                 "    ExitProcess( %s( GetModuleHandleA(0), 0, cmdline, info.wShowWindow ) );\n"
                 "}\n\n", init_func, init_func );
        init_func = "__wine_exe_main";
        subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
        break;
    case SPEC_MODE_CUIEXE:
        if (!init_func) init_func = "main";
        fprintf( outfile,
                 "\nint _ARGC;\n"
                 "char **_ARGV;\n"
                 "extern void __stdcall ExitProcess(int);\n"
                 "static void __wine_exe_main(void)\n"
                 "{\n"
                 "    extern int %s( int argc, char *argv[] );\n"
                 "    extern int __wine_get_main_args( char ***argv );\n"
                 "    _ARGC = __wine_get_main_args( &_ARGV );\n"
                 "    ExitProcess( %s( _ARGC, _ARGV ) );\n"
                 "}\n\n", init_func, init_func );
        init_func = "__wine_exe_main";
        subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
        break;
    case SPEC_MODE_CUIEXE_UNICODE:
        if (!init_func) init_func = "wmain";
        fprintf( outfile,
                 "\ntypedef unsigned short WCHAR;\n"
                 "int _ARGC;\n"
                 "WCHAR **_ARGV;\n"
                 "extern void __stdcall ExitProcess(int);\n"
                 "static void __wine_exe_main(void)\n"
                 "{\n"
                 "    extern int %s( int argc, WCHAR *argv[] );\n"
                 "    extern int __wine_get_wmain_args( WCHAR ***argv );\n"
                 "    _ARGC = __wine_get_wmain_args( &_ARGV );\n"
                 "    ExitProcess( %s( _ARGC, _ARGV ) );\n"
                 "}\n\n", init_func, init_func );
        init_func = "__wine_exe_main";
        subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
        break;
    }

    if( delay_initialization )
    {
        fprintf( outfile,
                 "/* A place to store things until elf entry is actually run */\n"
                 "static int    __wine_saved_argc;\n"
                 "static char** __wine_saved_argv;\n"
                 "static char** __wine_saved_env;\n"
                 "\n"
                 "static void __wine_spec_%s_init(void);\n",
                 DLLName );

        fprintf( outfile,
                 "void WINE_USED\n"
#if defined(__GNUC__)
                 "__attribute__((section (\".wine.init\")))\n"
#endif
                 "__wine_deferred_elf_initialization(int argc, char** argv, char** env)\n"
                 "{\n"
                 "  /* FIXME: Do I need to copy these things or is maintaining a reference sufficent? */\n"
                 "  __wine_saved_argc = argc;\n"
                 "  __wine_saved_argv = argv;\n"
                 "  __wine_saved_env  = env;\n"
                 "\n"
                 "  __wine_spec_%s_init();\n"
                 "}\n",
                 DLLName );

        /* Crank out an entry point which always calls into the deferred .init section code
         * and then invokes the indicated initialization function. Thus all builtin dlls will
         * now have entry points despite what's indicated in the spec file.
         */
        fprintf( outfile, "extern void WINE_USED\n"
#if defined(__GNUC__)
                          " __attribute__ ((section (\".init\")))\n"
#endif
                          " _init (int argc,...);\n" );

        if( SpecMode == SPEC_MODE_DLL )
        {
            if( init_func != NULL )
            {
                fprintf( outfile,
                         "extern int __stdcall %s( void* a, unsigned long b, void* c );\n",
                         init_func );
            }

            fprintf( outfile,
                     "\nint __stdcall %s( void* a,unsigned long b, void* c )\n",
                     BuiltinEntryPointName );

        }
        else /* EXE with WinMain type entry point */
        {
            fprintf( outfile,
                     "\nvoid %s(void)\n",
                     BuiltinEntryPointName );
        }

        fprintf( outfile,
                 "{\n"
                 "  static int times_initialized = 0;\n"
                 "\n"
                 "  /* Perform all deferred .init section actions provided we haven't already done so */\n"
                 "  if( times_initialized == 0 )\n"
                 "  {\n"
                 "    times_initialized = 1;\n"
                 "    _init( __wine_saved_argc, __wine_saved_argv, __wine_saved_env );\n"
                 "  }\n" );

        if( SpecMode == SPEC_MODE_DLL )
        {
            /* If there is no real DLL entry point, then just return success */
            if( init_func == NULL )
            {
                fprintf( outfile,
                         "  return 1;\n"
                         "}\n\n" );
            }
            else
            {
                fprintf( outfile,
                   "\n"
                   "  /* Now call the real entry point */\n"
                   "  return %s( a, b, c );\n"
                   "}\n\n",
                   init_func );
            }
        }
        else /* EXE with WinMain type entry point - there is a wine wrapper */
        {
            fprintf( outfile,
                     "\n"
                     "  /* Now call the real entry point */\n"
                     "  %s();\n"
                     "}\n\n",
                     init_func );
        }

        /* We now hijack the entry point name */
        init_func = BuiltinEntryPointName;
    }

    /* Output the NT header */

#define NUMBER_OF_SECTIONS 1

    image_size = ((size_of_image+page_size-1)&~(page_size-1));
    if (image_size < page_size) image_size = page_size;

#define NUMBER_OF_SECTIONS 1

    /* this is the IMAGE_NT_HEADERS structure, but we cannot include winnt.h here */
    fprintf( outfile, "static const struct image_nt_headers\n{\n" );
    fprintf( outfile, "  int Signature;\n" );
    fprintf( outfile, "  struct file_header {\n" );
    fprintf( outfile, "    short Machine;\n" );
    fprintf( outfile, "    short NumberOfSections;\n" );
    fprintf( outfile, "    int   TimeDateStamp;\n" );
    fprintf( outfile, "    void *PointerToSymbolTable;\n" );
    fprintf( outfile, "    int   NumberOfSymbols;\n" );
    fprintf( outfile, "    short SizeOfOptionalHeader;\n" );
    fprintf( outfile, "    short Characteristics;\n" );
    fprintf( outfile, "  } FileHeader;\n" );
    fprintf( outfile, "  struct opt_header {\n" );
    fprintf( outfile, "    short Magic;\n" );
    fprintf( outfile, "    char  MajorLinkerVersion, MinorLinkerVersion;\n" );
    fprintf( outfile, "    int   SizeOfCode;\n" );
    fprintf( outfile, "    int   SizeOfInitializedData;\n" );
    fprintf( outfile, "    int   SizeOfUninitializedData;\n" );
    fprintf( outfile, "    void *AddressOfEntryPoint;\n" );
    fprintf( outfile, "    void *BaseOfCode;\n" );
    fprintf( outfile, "    void *BaseOfData;\n" );
    fprintf( outfile, "    void *ImageBase;\n" );
    fprintf( outfile, "    int   SectionAlignment;\n" );
    fprintf( outfile, "    int   FileAlignment;\n" );
    fprintf( outfile, "    short MajorOperatingSystemVersion;\n" );
    fprintf( outfile, "    short MinorOperatingSystemVersion;\n" );
    fprintf( outfile, "    short MajorImageVersion;\n" );
    fprintf( outfile, "    short MinorImageVersion;\n" );
    fprintf( outfile, "    short MajorSubsystemVersion;\n" );
    fprintf( outfile, "    short MinorSubsystemVersion;\n" );
    fprintf( outfile, "    int   Win32VersionValue;\n" );
    fprintf( outfile, "    int   SizeOfImage;\n" );
    fprintf( outfile, "    int   SizeOfHeaders;\n" );
    fprintf( outfile, "    int   CheckSum;\n" );
    fprintf( outfile, "    short Subsystem;\n" );
    fprintf( outfile, "    short DllCharacteristics;\n" );
    fprintf( outfile, "    int   SizeOfStackReserve;\n" );
    fprintf( outfile, "    int   SizeOfStackCommit;\n" );
    fprintf( outfile, "    int   SizeOfHeapReserve;\n" );
    fprintf( outfile, "    int   SizeOfHeapCommit;\n" );
    fprintf( outfile, "    int   LoaderFlags;\n" );
    fprintf( outfile, "    int   NumberOfRvaAndSizes;\n" );
    fprintf( outfile, "    struct { const void *VirtualAddress; int Size; } DataDirectory[%d];\n",
             IMAGE_NUMBEROF_DIRECTORY_ENTRIES );
    fprintf( outfile, "  } OptionalHeader;\n" );
    fprintf( outfile, "  struct sec_header {\n" );
    fprintf( outfile, "    char  Name[%d];\n", IMAGE_SIZEOF_SHORT_NAME );
    fprintf( outfile, "    int   VirtualSize;\n" );
    fprintf( outfile, "    void *VirtualAddress;\n" );
    fprintf( outfile, "    int   SizeOfRawData;\n" );
    fprintf( outfile, "    void *PointerToRawData;\n" );
    fprintf( outfile, "    void *PointerToRelocations;\n" );
    fprintf( outfile, "    void *PointerToLinenumbers;\n" );
    fprintf( outfile, "    short NumberOfRelocations;\n" );
    fprintf( outfile, "    short NumberOfLinenumbers;\n" );
    fprintf( outfile, "    int   Characteristics;\n" );
    fprintf( outfile, "  } SectionHeader[%d];\n", NUMBER_OF_SECTIONS );
    fprintf( outfile, "} nt_header = {\n" );
    fprintf( outfile, "  0x%04x,\n", IMAGE_NT_SIGNATURE );   /* Signature */

    fprintf( outfile, "  { 0x%04x,\n", IMAGE_FILE_MACHINE_I386 );  /* Machine */
    fprintf( outfile, "    %d,\n", NUMBER_OF_SECTIONS );           /* NumberOfSections */
    fprintf( outfile, "    0, 0, 0,\n" );
    fprintf( outfile, "    sizeof(nt_header.OptionalHeader),\n" ); /* SizeOfOptionalHeader */
    fprintf( outfile, "    0x%04x },\n", characteristics );        /* Characteristics */

    fprintf( outfile, "  { 0x%04x,\n", IMAGE_NT_OPTIONAL_HDR_MAGIC );  /* Magic */
    fprintf( outfile, "    0, 0,\n" );                   /* Major/MinorLinkerVersion */
    fprintf( outfile, "    0, 0, 0,\n" );                /* SizeOfCode/Data */
    fprintf( outfile, "    %s,\n", init_func ? init_func : "0" );  /* AddressOfEntryPoint */
    fprintf( outfile, "    0, 0,\n" );                   /* BaseOfCode/Data */
    fprintf( outfile, "    pe_header,\n" );              /* ImageBase */
    fprintf( outfile, "    %u,\n", page_size );          /* SectionAlignment */
    fprintf( outfile, "    %u,\n", page_size );          /* FileAlignment */
    fprintf( outfile, "    1, 0,\n" );                   /* Major/MinorOperatingSystemVersion */
    fprintf( outfile, "    0, 0,\n" );                   /* Major/MinorImageVersion */
    fprintf( outfile, "    4, 0,\n" );                   /* Major/MinorSubsystemVersion */
    fprintf( outfile, "    0,\n" );                      /* Win32VersionValue */
    fprintf( outfile, "    %u,\n", image_size );         /* SizeOfImage */
    fprintf( outfile, "    %u,\n", page_size );          /* SizeOfHeaders */
    fprintf( outfile, "    0,\n" );                      /* CheckSum */
    fprintf( outfile, "    0x%04x,\n", subsystem );      /* Subsystem */
    fprintf( outfile, "    0,\n" );                      /* DllCharacteristics */
    fprintf( outfile, "    %d, 0,\n", stack_size*1024 ); /* SizeOfStackReserve/Commit */
    fprintf( outfile, "    %d, 0,\n", DLLHeapSize*1024 );/* SizeOfHeapReserve/Commit */
    fprintf( outfile, "    0,\n" );                      /* LoaderFlags */
    fprintf( outfile, "    %d,\n", IMAGE_NUMBEROF_DIRECTORY_ENTRIES );  /* NumberOfRvaAndSizes */
    fprintf( outfile, "    {\n" );
    fprintf( outfile, "      { %s, %d },\n",  /* IMAGE_DIRECTORY_ENTRY_EXPORT */
             exports_size ? "__wine_spec_exports" : "0", exports_size );
    fprintf( outfile, "      { %s, %s },\n",  /* IMAGE_DIRECTORY_ENTRY_IMPORT */
             nr_imports ? "&imports" : "0", nr_imports ? "sizeof(imports)" : "0" );
    fprintf( outfile, "      { %s, %s },\n",   /* IMAGE_DIRECTORY_ENTRY_RESOURCE */
             nr_resources ? "&resources" : "0", nr_resources ? "sizeof(resources)" : "0" );
    fprintf( outfile, "    }\n" );
    fprintf( outfile, "  },\n" );
    fprintf( outfile, "  {\n" );
    fprintf( outfile, "    { \".text   \",\n" );         /* Name */
    fprintf( outfile, "      %d,\n", image_size );       /* VirtualSize */
    fprintf( outfile, "      pe_header,\n" );            /* VirtualAddress */
    fprintf( outfile, "      %d,\n", image_size );       /* SizeOfRawData */
    fprintf( outfile, "      0,\n" );                    /* PointerToRawData */
    fprintf( outfile, "      0, 0, 0, 0,\n" );
    fprintf( outfile, "      0x20000020,\n" );           /* Characteristics */
    fprintf( outfile, "    }\n" );
    fprintf( outfile, "  }\n" );
    fprintf( outfile, "};\n\n" );

    /* Output the DLL constructor */

    fprintf( outfile, "#ifndef __GNUC__\n" );
    fprintf( outfile, "static void __asm__dummy_dll_init(void) {\n" );
    fprintf( outfile, "#endif /* defined(__GNUC__) */\n" );

#if defined(__i386__) && !defined(__APPLE__)

   if( !delay_initialization )
   {
       fprintf( outfile, "asm(\"\\t.section\\t\\\".init\\\" ,\\\"ax\\\"\\n\"\n" );
       fprintf( outfile, "    \"\\tcall " PREFIX "__wine_spec_%s_init\\n\"\n", DLLName );
       fprintf( outfile, "    \"\\t.section\\t\\\".text\\\"\\n\");\n" );
   }

    if (nr_debug)
    {
        fprintf( outfile, "asm(\"\\t.section\\t\\\".fini\\\" ,\\\"ax\\\"\\n\"\n" );
        fprintf( outfile, "    \"\\tcall " PREFIX "__wine_spec_%s_fini\\n\"\n", DLLName );
        fprintf( outfile, "    \"\\t.section\\t\\\".text\\\"\\n\");\n" );
    }
#elif defined(__sparc__)
#error "This is out of date. Please add in the appropriate stuff for deferred initialization"
    fprintf( outfile, "asm(\"\\t.section\\t\\\".init\\\" ,\\\"ax\\\"\\n\"\n" );
    fprintf( outfile, "    \"\\tcall " PREFIX "__wine_spec_%s_init\\n\"\n", DLLName );
    fprintf( outfile, "    \"\\tnop\\n\"\n" );
    fprintf( outfile, "    \"\\t.section\t\\\".text\\\"\\n\");\n" );
    if (nr_debug)
    {
        fprintf( outfile, "asm(\"\\t.section\\t\\\".fini\\\" ,\\\"ax\\\"\\n\"\n" );
        fprintf( outfile, "    \"\\tcall " PREFIX "__wine_spec_%s_fini\\n\"\n", DLLName );
        fprintf( outfile, "    \"\\tnop\\n\"\n" );
        fprintf( outfile, "    \"\\t.section\t\\\".text\\\"\\n\");\n" );
    }
#elif defined(__APPLE__)
    if (!strstr(input_file_name, "gdi32.spec") && !strstr(input_file_name, "user32.spec") && !strstr(input_file_name, "kernel32.spec") && !strstr(input_file_name, "comdlg32.spec"))
        fprintf( outfile, "void __wine_spec_%s_init(void) __attribute__((constructor));\n\n", DLLName);
#else
#error "You need to define the DLL constructor for your architecture"
#endif

    fprintf( outfile, "#ifndef __GNUC__\n" );
    fprintf( outfile, "}\n" );
    fprintf( outfile, "#endif /* defined(__GNUC__) */\n\n" );

    if( delay_initialization )
    {
        fprintf( outfile, "static\n" );
    }

    fprintf( outfile,
             "void __wine_spec_%s_init(void)\n"
             "{\n"
             "    extern void __wine_dll_register( const struct image_nt_headers *, const char * );\n"
             "    extern void *__wine_dbg_register( char * const *, int, const char * );\n",
             DLLName );

#if defined(__APPLE__)
    fprintf( outfile,
             "    static int initdone;\n"
             "    if (initdone) return;\n"
             "    initdone = 1;\n" );
#endif

    fprintf( outfile,
             "    __wine_dll_register( &nt_header, \"%s\" );\n",
             DLLFileName );


    if (nr_debug)
        fprintf( outfile, "    debug_registration = __wine_dbg_register( debug_channels, %d, \"%s\" );\n",
                 nr_debug, DLLFileName );

#if defined(__APPLE__)
    /* This is needed to support Mac OS X. These four DLLs have to be built and handled differently,
       due to the way libraries are handled on that platform.

       FIXME: should change it from having these four spec files hardcoded, to having a special flag,
              like: -AddInitFnc with a colon or comma delimited list of all these functions, and moved
              into their individual make files
    */
    if ( strstr (input_file_name, "gdi32.spec") )
    {
        fprintf(outfile,
                "    extern void __wine_spec_DISPDIB_init();\n"
                "    __wine_spec_DISPDIB_init();\n"
                "    extern void __wine_spec_GDI_init();\n"
                "    __wine_spec_GDI_init();\n"
                "    extern void __wine_spec_WING_init();\n"
                "    __wine_spec_WING_init();\n");
    }
    else if ( strstr (input_file_name, "user32.spec") )
    {
        fprintf(outfile,
                "    extern void __wine_spec_DISPLAY_init();\n"
                "    __wine_spec_DISPLAY_init();\n"
                "    extern void __wine_spec_DDEML_init();\n"
                "    __wine_spec_DDEML_init();\n"
                "    extern void __wine_spec_KEYBOARD_init();\n"
                "    __wine_spec_KEYBOARD_init();\n"
                "    extern void __wine_spec_MOUSE_init();\n"
                "    __wine_spec_MOUSE_init();\n"
                "    extern void __wine_spec_USER_init();\n"
                "    __wine_spec_USER_init();\n");
    }
    else if ( strstr (input_file_name, "kernel32.spec") )
    {
        fprintf(outfile,
                "    extern void __wine_spec_COMM_init();\n"
                "    __wine_spec_COMM_init();\n"
                "    extern void __wine_spec_SYSTEM_init();\n"
                "    __wine_spec_SYSTEM_init();\n"
                "    extern void __wine_spec_WIN87EM_init();\n"
                "    __wine_spec_WIN87EM_init();\n"
                "    extern void __wine_spec_WPROCS_init();\n"
                "    __wine_spec_WPROCS_init();\n"
                "    extern void __wine_spec_KERNEL_init();\n"
                "    __wine_spec_KERNEL_init();\n"
                "    extern void __wine_spec_STRESS_init();\n"
                "    __wine_spec_STRESS_init();\n"
                "    extern void __wine_spec_TOOLHELP_init();\n"
                "    __wine_spec_TOOLHELP_init();\n"
                "    extern void __wine_spec_WINDEBUG_init();\n"
                "    __wine_spec_WINDEBUG_init();\n");
    }
    else if ( strstr (input_file_name, "comdlg32.spec") )
    {
        fprintf(outfile,
                "    extern void __wine_spec_COMMDLG_init();\n"
                "    __wine_spec_COMMDLG_init();\n");
    }
#endif

    fprintf( outfile, "}\n" );

#if defined(__APPLE__)
    fprintf( outfile, "#ifdef __GNUC__\n" );
    /*fprintf( outfile, "__private_extern__ void __macho_linker_init(void) {\n" );*/
    fprintf( outfile, "void __macho_linker_init(void) {\n" );
    fprintf( outfile, "    __wine_spec_%s_init();\n", DLLName);

    fprintf( outfile, "}\n" );
    fprintf( outfile, "#endif /* defined(__GNUC__) */\n\n" );
#endif

#if defined(__APPLE__)
    /* On MacOS X (at least), the DLL (if built as a dylib) does not load properly unless there is an
       dummy initialization function within it */
    if (need_force_init)
    {
        char   lowerstring[80];
        strncpy(lowerstring, DLLName, 80);
        int i, len = strlen(lowerstring);
        for (i = 0; i < len; i++)
            lowerstring[i] = tolower(lowerstring[i]);
        fprintf(outfile, "\n\nvoid force_%s_init(void)\n{\n}\n", DLLName);
    }
#endif

    if (nr_debug)
    {
        fprintf( outfile,
                 "\nvoid __wine_spec_%s_fini(void)\n"
                 "{\n"
                 "    extern void __wine_dbg_unregister( void* );\n"
                 "    __wine_dbg_unregister( debug_registration );\n"
                 "}\n", DLLName );
    }
}


/*******************************************************************
 *         BuildDef32File
 *
 * Build a Win32 def file from a spec file.
 */
void BuildDef32File(FILE *outfile)
{
    int i;

    AssignOrdinals();

    fprintf(outfile, "; File generated automatically from %s; do not edit!\n\n",
            input_file_name );

    fprintf(outfile, "LIBRARY lib%s\n\n", DLLFileName);

    fprintf(outfile, "EXPORTS\n");

    /* Output the exports and relay entry points */

    for(i = 0; i < nb_entry_points; i++)
    {
        ORDDEF *odp = EntryPoints[i];
        if(!odp || !*odp->name || (odp->flags & FLAG_NOIMPORT)) continue;

        fprintf(outfile, "  %s", odp->name);

        switch(odp->type)
        {
        case TYPE_EXTERN:
        case TYPE_VARARGS:
        case TYPE_CDECL:
        case TYPE_VARIABLE:
            /* try to reduce output */
            if(strcmp(odp->name, odp->link_name))
                fprintf(outfile, "=%s", odp->link_name);
            break;
        case TYPE_STDCALL:
        {
#ifdef NEED_STDCALL_DECORATION
            int at_param = strlen(odp->u.func.arg_types) * sizeof(int);
            fprintf(outfile, "@%d", at_param);
#endif /* NEED_STDCALL_DECORATION */
            /* try to reduce output */
            if(strcmp(odp->name, odp->link_name))
            {
                fprintf(outfile, "=%s", odp->link_name);
#ifdef NEED_STDCALL_DECORATION
                fprintf(outfile, "@%d", at_param);
#endif /* NEED_STDCALL_DECORATION */
            }
            break;
        }
        case TYPE_STUB:
            fprintf(outfile, "=%s", make_internal_name( odp, "stub" ));
            break;
        case TYPE_FORWARD:
            fprintf(outfile, "=lib%s", odp->link_name);
            break;
        default:
            assert(0);
        }
        fprintf(outfile, " @%d\n", odp->ordinal);
    }
}
