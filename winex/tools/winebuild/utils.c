/* small utility functions for winebuild */

#include "config.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"

void *xmalloc (size_t size)
{
    void *res;

    res = malloc (size ? size : 1);
    if (res == NULL)
    {
        fprintf (stderr, "Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

void *xrealloc (void *ptr, size_t size)
{
    void *res = realloc (ptr, size);
    if (res == NULL)
    {
        fprintf (stderr, "Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

char *xstrdup( const char *str )
{
    char *res = strdup( str );
    if (!res)
    {
        fprintf (stderr, "Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

char *strupper(char *s)
{
    char *p;
    for (p = s; *p; p++) *p = toupper(*p);
    return s;
}

void fatal_error( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    vfprintf( stderr, msg, valist );
    va_end( valist );
    exit(1);
}

void fatal_perror( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    vfprintf( stderr, msg, valist );
    perror( " " );
    va_end( valist );
    exit(1);
}

void warning( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    fprintf( stderr, "warning: " );
    vfprintf( stderr, msg, valist );
    va_end( valist );
}

/* dump a byte stream into the assembly code */
void dump_bytes( FILE *outfile, const unsigned char *data, int len,
                 const char *label, int constant )
{
    int i;

    fprintf( outfile, "\nstatic %sunsigned char %s[%d] = {",
             constant ? "const " : "", label, len );
    for (i = 0; i < len; i++)
    {
        if (!(i & 7)) fprintf( outfile, "\n  " );
        fprintf( outfile, "0x%02x", *data++ );
        if (i < len - 1) fprintf( outfile, "," );
    }
    fprintf( outfile, "\n};\n" );
}

/*****************************************************************
 *  Function:    get_alignment
 *
 *  Description:
 *    According to the info page for gas, the .align directive behaves
 * differently on different systems.  On some architectures, the
 * argument of a .align directive is the number of bytes to pad to, so
 * to align on an 8-byte boundary you'd say
 *     .align 8
 * On other systems, the argument is "the number of low-order zero bits
 * that the location counter must have after advancement."  So to
 * align on an 8-byte boundary you'd say
 *     .align 3
 *
 * The reason gas is written this way is that it's trying to mimick
 * native assemblers for the various architectures it runs on.  gas
 * provides other directives that work consistantly across
 * architectures, but of course we want to work on all arches with or
 * without gas.  Hence this function.
 *
 *
 *  Parameters:
 *                alignBoundary  --  the number of bytes to align to.
 *                                   If we're on an architecture where
 *                                   the assembler requires a 'number
 *                                   of low-order zero bits' as a
 *                                   .align argument, then this number
 *                                   must be a power of 2.
 *
 */
int get_alignment(int alignBoundary)
{
#if defined __APPLE__ 

    int n = 0;

    switch(alignBoundary)
    {
    case 2:
        n = 1;
        break;
    case 4:
        n = 2;
        break;
    case 8:
        n = 3;
        break;
    case 16:
        n = 4;
        break;
    case 32:
        n = 5;
        break;
    case 64:
        n = 6;
        break;
    case 128:
        n = 7;
        break;
    case 256:
        n = 8;
        break;
    case 512:
        n = 9;
        break;
    case 1024:
        n = 10;
        break;
    case 2048:
        n = 11;
        break;
    case 4096:
        n = 12;
        break;
    case 8192:
        n = 13;
        break;
    case 16384:
        n = 14;
        break;
    case 32768:
        n = 15;
        break;
    case 65536:
        n = 16;
        break;
    default:
        fatal_error("Alignment to %d-byte boundary not supported on this architecture.\n",
                    alignBoundary);
    }
    return n;

#elif defined(__i386__) || defined(__sparc__)

    return alignBoundary;

#else
#error "How does the '.align' assembler directive work on your architecture?"
#endif
}
