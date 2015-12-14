/*
 * Generate include file dependencies
 *
 * Copyright 1996 Alexandre Julliard
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Max first-level includes per file */
#define MAX_INCLUDES 200

typedef struct _INCL_FILE
{
    struct _INCL_FILE *next;
    char              *name;
    char              *filename;
    struct _INCL_FILE *included_by;   /* file that included this one */
    int                included_line; /* line where this file was included */
    int                system;        /* is it a system include (#include <name>) */
    struct _INCL_FILE *owner;
    struct _INCL_FILE *files[MAX_INCLUDES];
} INCL_FILE;

static INCL_FILE *firstSrc;
static INCL_FILE *firstInclude;

typedef struct _INCL_PATH
{
    struct _INCL_PATH *next;
    const char        *name;
} INCL_PATH;

static INCL_PATH *firstPath;

static const char *SrcDir = NULL;
static const char *OutputFileName = "Makefile";
static const char *Separator = "### Dependencies";
static const char *ProgramName;

static const char Usage[] =
    "Usage: %s [options] [files]\n"
    "Options:\n"
    "   -Idir   Search for include files in directory 'dir'\n"
    "   -Cdir   Search for source files in directory 'dir'\n"
    "   -fxxx   Store output in file 'xxx' (default: Makefile)\n"
    "   -sxxx   Use 'xxx' as separator (default: \"### Dependencies\")\n";


/*******************************************************************
 *         xmalloc
 */
static void *xmalloc( int size )
{
    void *res;
    if (!(res = malloc (size ? size : 1)))
    {
        fprintf( stderr, "%s: Virtual memory exhausted.\n", ProgramName );
        exit(1);
    }
    return res;
}


/*******************************************************************
 *         xstrdup
 */
static char *xstrdup( const char *str )
{
    char *res = strdup( str );
    if (!res)
    {
        fprintf( stderr, "%s: Virtual memory exhausted.\n", ProgramName );
        exit(1);
    }
    return res;
}


/*******************************************************************
 *         is_generated
 *
 * Test if a given file type is generated during the make process
 */
static int is_generated( const char *name )
{
    static const char * const extensions[] = { ".tab.h", ".mc.rc" };
    size_t i, len = strlen(name);
    for (i = 0; i < sizeof(extensions)/sizeof(extensions[0]); i++)
    {
        if (len <= strlen(extensions[i])) continue;
        if (!strcmp( name + len - strlen(extensions[i]), extensions[i] )) return 1;
    }
    return 0;
}

/*******************************************************************
 *         add_include_path
 *
 * Add a directory to the include path.
 */
static void add_include_path( const char *name )
{
    INCL_PATH *path = xmalloc( sizeof(*path) );
    INCL_PATH **p = &firstPath;
    while (*p) p = &(*p)->next;
    *p = path;
    path->next = NULL;
    path->name = name;
}


/*******************************************************************
 *         add_src_file
 *
 * Add a source file to the list.
 */
static INCL_FILE *add_src_file( const char *name )
{
    INCL_FILE **p = &firstSrc;
    INCL_FILE *file = xmalloc( sizeof(*file) );
    memset( file, 0, sizeof(*file) );
    file->name = xstrdup(name);
    while (*p) p = &(*p)->next;
    *p = file;
    return file;
}


/*******************************************************************
 *         add_include
 *
 * Add an include file if it doesn't already exists.
 */
static INCL_FILE *add_include( INCL_FILE *pFile, const char *name, int line, int system )
{
    INCL_FILE **p = &firstInclude;
    int pos;

    for (pos = 0; pos < MAX_INCLUDES; pos++) if (!pFile->files[pos]) break;
    if (pos >= MAX_INCLUDES)
    {
        fprintf( stderr, "%s: %s: too many included files, please fix MAX_INCLUDES\n",
                 ProgramName, pFile->name );
        exit(1);
    }

    while (*p && strcmp( name, (*p)->name )) p = &(*p)->next;
    if (!*p)
    {
        *p = xmalloc( sizeof(INCL_FILE) );
        memset( *p, 0, sizeof(INCL_FILE) );
        (*p)->name = xstrdup(name);
        (*p)->included_by = pFile;
        (*p)->included_line = line;
        (*p)->system = system || pFile->system;
    }
    pFile->files[pos] = *p;
    return *p;
}


/*******************************************************************
 *         open_src_file
 */
static FILE *open_src_file( INCL_FILE *pFile )
{
    FILE *file;

    if (SrcDir)
    {
        pFile->filename = xmalloc( strlen(SrcDir) + strlen(pFile->name) + 2 );
        strcpy( pFile->filename, SrcDir );
        strcat( pFile->filename, "/" );
        strcat( pFile->filename, pFile->name );
    }
    else pFile->filename = xstrdup( pFile->name );

    if (!(file = fopen( pFile->filename, "r" )))
    {
        perror( pFile->filename );
        exit(1);
    }
    return file;
}


/*******************************************************************
 *         open_include_file
 */
static FILE *open_include_file( INCL_FILE *pFile )
{
    FILE *file = NULL;
    INCL_PATH *path;

    for (path = firstPath; path; path = path->next)
    {
        char *filename = xmalloc(strlen(path->name) + strlen(pFile->name) + 2);
        strcpy( filename, path->name );
        strcat( filename, "/" );
        strcat( filename, pFile->name );
        if ((file = fopen( filename, "r" )))
        {
            pFile->filename = filename;
            break;
        }
        free( filename );
    }
    if (!file && pFile->system) return NULL;  /* ignore system files we cannot find */

    /* try in src file directory */
    if (!file)
    {
        char *p = strrchr(pFile->included_by->name, '/');
        if (p)
        {
            int l = p - pFile->included_by->name + 1;
            char *filename = xmalloc(l + strlen(pFile->name) + 1);
            memcpy( filename, pFile->included_by->name, l );
            strcpy( filename + l, pFile->name );
            if ((file = fopen( filename, "r" ))) pFile->filename = filename;
            else free( filename );
        }
    }

    if (!file)
    {
        if (pFile->included_by->system) return NULL;  /* ignore if included by a system file */
        if (firstPath) perror( pFile->name );
        else fprintf( stderr, "%s: %s: File not found\n",
                      ProgramName, pFile->name );
        while (pFile->included_by)
        {
            fprintf( stderr, "  %s was first included from %s:%d\n",
                     pFile->name, pFile->included_by->name, pFile->included_line );
            pFile = pFile->included_by;
        }
        exit(1);
    }
    return file;
}


/*******************************************************************
 *         parse_file
 */
static void parse_file( INCL_FILE *pFile, int src )
{
    char buffer[1024];
    char *include;
    int line = 0;
    FILE *file;

    if (is_generated( pFile->name ))
    {
        /* file is generated during make, don't try to open it */
        pFile->filename = xstrdup( pFile->name );
        return;
    }

    file = src ? open_src_file( pFile ) : open_include_file( pFile );
    if (!file) return;

    while (fgets( buffer, sizeof(buffer)-1, file ))
    {
        char quote;
        char *p = buffer;
        line++;
        while (*p && isspace(*p)) p++;
        if (*p++ != '#') continue;
        while (*p && isspace(*p)) p++;
        if (strncmp( p, "include", 7 )) continue;
        p += 7;
        while (*p && isspace(*p)) p++;
        if (*p != '\"' && *p != '<' ) continue;
        quote = *p++;
        if (quote == '<') quote = '>';
        include = p;
        while (*p && (*p != quote)) p++;
        if (!*p)
        {
            fprintf( stderr, "%s:%d: Malformed #include directive\n",
                     pFile->filename, line );
            exit(1);
        }
        *p = 0;
        add_include( pFile, include, line, (quote == '>') );
    }
    fclose(file);
}


/*******************************************************************
 *         output_include
 */
static void output_include( FILE *file, INCL_FILE *pFile,
                            INCL_FILE *owner, int *column )
{
    int i;

    if (pFile->owner == owner) return;
    if (!pFile->filename) return;
    pFile->owner = owner;
    if (*column + strlen(pFile->filename) + 1 > 70)
    {
        fprintf( file, " \\\n" );
        *column = 0;
    }
    fprintf( file, " %s", pFile->filename );
    *column += strlen(pFile->filename) + 1;
    for (i = 0; i < MAX_INCLUDES; i++)
        if (pFile->files[i]) output_include( file, pFile->files[i],
                                             owner, column );
}


/*******************************************************************
 *         output_src
 */
static void output_src( FILE *file, INCL_FILE *pFile, int *column )
{
    char *obj = xstrdup( pFile->name );
    char *ext = strrchr( obj, '.' );
    if (ext && strchr( ext, '/' )) ext = NULL;
    if (ext)
    {
        *ext++ = 0;
        if (!strcmp( ext, "y" ))  /* yacc file */
        {
            *column += fprintf( file, "y.tab.o: y.tab.c" );
        }
        else if (!strcmp( ext, "l" ))  /* lex file */
        {
            *column += fprintf( file, "%s.o: %s.c", LEX_OUTPUT_ROOT, LEX_OUTPUT_ROOT );
        }
        else if (!strcmp( ext, "rc" ))  /* resource file */
        {
            *column += fprintf( file, "%s.res: %s", obj, pFile->filename );
        }
        else if (!strcmp( ext, "mc" ))  /* message file */
        {
            *column += fprintf( file, "%s.mc.rc: %s", obj, pFile->filename );
        }
        else
        {
            *column += fprintf( file, "%s.o: %s", obj, pFile->filename );
        }
    }
    free( obj );
}


/*******************************************************************
 *         output_dependencies
 */
static void output_dependencies(void)
{
    INCL_FILE *pFile;
    int i, column;
    FILE *file = NULL;
    char buffer[1024];

    if (Separator && ((file = fopen( OutputFileName, "r+" ))))
    {
        while (fgets( buffer, sizeof(buffer), file ))
            if (!strncmp( buffer, Separator, strlen(Separator) )) break;
        ftruncate( fileno(file), ftell(file) );
        fseek( file, 0L, SEEK_END );
    }
    if (!file)
    {
        if (!(file = fopen( OutputFileName, Separator ? "a" : "w" )))
        {
            perror( OutputFileName );
            exit(1);
        }
    }
    for( pFile = firstSrc; pFile; pFile = pFile->next)
    {
        column = 0;
        output_src( file, pFile, &column );
        for (i = 0; i < MAX_INCLUDES; i++)
            if (pFile->files[i]) output_include( file, pFile->files[i],
                                                 pFile, &column );
        fprintf( file, "\n" );
    }
    fclose(file);
}


/*******************************************************************
 *         parse_option
 */
static void parse_option( const char *opt )
{
    switch(opt[1])
    {
    case 'I':
        if (opt[2]) add_include_path( opt + 2 );
        break;
    case 'C':
        if (opt[2]) SrcDir = opt + 2;
        else SrcDir = NULL;
        break;
    case 'f':
        if (opt[2]) OutputFileName = opt + 2;
        break;
    case 's':
        if (opt[2]) Separator = opt + 2;
        else Separator = NULL;
        break;
    default:
        fprintf( stderr, "Unknown option '%s'\n", opt );
        fprintf( stderr, Usage, ProgramName );
        exit(1);
    }
}


/*******************************************************************
 *         main
 */
int main( int argc, char *argv[] )
{
    INCL_FILE *pFile;

    ProgramName = argv[0];
    while (argc > 1)
    {
        if (*argv[1] == '-') parse_option( argv[1] );
        else
        {
            pFile = add_src_file( argv[1] );
            parse_file( pFile, 1 );
        }
        argc--;
        argv++;
    }
    for (pFile = firstInclude; pFile; pFile = pFile->next)
        parse_file( pFile, 0 );
    if( firstSrc ) output_dependencies();
    return 0;
}
