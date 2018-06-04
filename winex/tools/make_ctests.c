/*
 * Generate a C file containing a list of tests
 *
 * Copyright 2002, 2005 Alexandre Julliard
 * Copyright 2002 Dimitrie O. Paun
 * Copyright 2005 Royce Mitchell III for the ReactOS Project
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 ****** Keep in sync with tools/winapi/msvcmaker:_generate_testlist_c *****
 */

#include "config.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static const char *output_file;

static void cleanup_files(void)
{
    if (output_file) unlink( output_file );
}

static void exit_on_signal( int sig )
{
    exit(1);  /* this will call the atexit functions */
}

static void fatal_error( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    fprintf( stderr, "make_ctests: " );
    vfprintf( stderr, msg, valist );
    va_end( valist );
    exit(1);
}

static void fatal_perror( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    fprintf( stderr, "make_ctests: " );
    vfprintf( stderr, msg, valist );
    perror( " " );
    va_end( valist );
    exit(1);
}

static void *xmalloc( size_t size )
{
    void *res = malloc (size ? size : 1);
    if (!res) fatal_error( "virtual memory exhausted.\n" );
    return res;
}

static char* basename( const char* filename )
{
    const char *p, *p2;
    char *out;
    size_t out_len;

    p = strrchr ( filename, '/' );
    if ( !p )
        p = filename;
    else
        ++p;

    /* look for backslashes, too... */
    p2 = strrchr ( p, '\\' );
    if ( p2 ) p = p2 + 1;

    /* find extension... */
    p2 = strrchr ( p, '.' );
    if ( !p2 )
        p2 = p + strlen(p);

    /* malloc a copy */
    out_len = p2-p;
    out = xmalloc ( out_len+1 );
    memcpy ( out, p, out_len );
    out[out_len] = '\0';
    return out;
}

static char *dirbasename( const char *filename )
{
    char *buffer;
    char *p[2];
    size_t len;


    len = strlen(filename);
    buffer = xmalloc(len + 1);
    memcpy(buffer, filename, sizeof(char) * (len + 1));

    /* remove the trailing separator (if any) for consistency in paths. */
    if (buffer[len - 1] == '/' || buffer[len - 1] == '\\')
        buffer[len - 1] = 0;

    /* chop off the file/folder name portion. */
    p[0] = strrchr(buffer, '/');
    p[1] = strrchr(buffer, '\\');

    if (p[1] > p[0])
        p[0] = p[1];

    if (p[0] != NULL)
        p[0][0] = 0;

    /* no path separators -> just a filename => no base directory, just return an empty string. */
    else
    {
        buffer[0] = 0;
        return buffer;
    }

    /* chop off the next folder name and return that as the path. */
    p[0] = strrchr(buffer, '/');
    p[1] = strrchr(buffer, '\\');

    if (p[1] > p[0])
        p[0] = p[1];

    /* no additional path separators -> assume root folder => return the remaining string as
         the base directory name. */
    if (p[0] == NULL)
        return buffer;

    p[0]++;
    len = strlen(p[0]);
    p[1] = xmalloc(len + 1);
    memcpy(p[1], p[0], sizeof(char) * (len + 1));
    free(buffer);

    return p[1];
}

int main( int argc, const char** argv )
{
    int i, count = 0;
    FILE *out = stdout;
    char **tests = xmalloc( argc * sizeof(*tests) );
    char *module;
    char  cwd[256];

    for (i = 1; i < argc; i++)
    {
        if (!strcmp( argv[i], "-o" ) && i < argc-1)
        {
            output_file = argv[++i];
            continue;
        }
        tests[count++] = basename( argv[i] );
    }

    getcwd(cwd, sizeof(cwd) / sizeof(cwd[0]));
    module = dirbasename(cwd);


    atexit( cleanup_files );
    signal( SIGTERM, exit_on_signal );
    signal( SIGINT, exit_on_signal );
#ifdef SIGHUP
    signal( SIGHUP, exit_on_signal );
#endif

    if (output_file)
    {
        if (!(out = fopen( output_file, "w" )))
            fatal_perror( "cannot create %s", output_file );
    }

    fprintf( out,
             "/* Automatically generated file; DO NOT EDIT!! */\n"
             "\n"
             "#define WIN32_LEAN_AND_MEAN\n"
             "#include <windows.h>\n\n"
             "#define STANDALONE\n"
             "#include \"wine/test.h\"\n\n" );

    for (i = 0; i < count; i++) fprintf( out, "extern void func_%s(void);\n", tests[i] );

    fprintf( out,
             "\n"
             "const struct test winetest_testlist[] =\n"
             "{\n" );

    for (i = 0; i < count; i++) fprintf( out, "    { \"%s\", \"%s\", func_%s },\n", module, tests[i], tests[i] );

    fprintf( out,
             "    { 0, 0, 0 }\n"
             "};\n" );

    if (output_file && fclose( out ))
        fatal_perror( "error writing to %s", output_file );

    output_file = NULL;
    return 0;
}
