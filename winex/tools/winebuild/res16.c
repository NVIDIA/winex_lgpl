/*
 * Builtin dlls resource support
 *
 * Copyright 2000 Alexandre Julliard
 */

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "winbase.h"
#include "build.h"

#define ALIGNMENT 2 /* alignment for resource data */
#define ALIGN_MASK ((1 << ALIGNMENT) - 1)

/* Unicode string or integer id */
struct string_id
{
    char  *str;  /* ptr to string */
    WORD   id;   /* integer id if str is NULL */
};

/* descriptor for a resource */
struct resource
{
    struct string_id type;
    struct string_id name;
    const void      *data;
    unsigned int     data_size;
    WORD             memopt;
};

/* type level of the resource tree */
struct res_type
{
    const struct string_id  *type;         /* type name */
    const struct resource   *res;          /* first resource of this type */
    unsigned int             nb_names;     /* total number of names */
};

static struct resource *resources;
static int nb_resources;

static struct res_type *res_types;
static int nb_types;     /* total number of types */

static const unsigned char *file_pos;   /* current position in resource file */
static const unsigned char *file_end;   /* end of resource file */
static const char *file_name;  /* current resource file name */


inline static struct resource *add_resource(void)
{
    resources = xrealloc( resources, (nb_resources + 1) * sizeof(*resources) );
    return &resources[nb_resources++];
}

static struct res_type *add_type( const struct resource *res )
{
    struct res_type *type;
    res_types = xrealloc( res_types, (nb_types + 1) * sizeof(*res_types) );
    type = &res_types[nb_types++];
    type->type        = &res->type;
    type->res         = res;
    type->nb_names    = 0;
    return type;
}

/* get the next byte from the current resource file */
static WORD get_byte(void)
{
    unsigned char ret = *file_pos++;
    if (file_pos > file_end) fatal_error( "%s is a truncated/corrupted file\n", file_name );
    return ret;
}

/* get the next word from the current resource file */
static WORD get_word(void)
{
    /* might not be aligned */
#ifdef WORDS_BIGENDIAN
    unsigned char high = get_byte();
    unsigned char low = get_byte();
#else
    unsigned char low = get_byte();
    unsigned char high = get_byte();
#endif
    return low | (high << 8);
}

/* get the next dword from the current resource file */
static DWORD get_dword(void)
{
#ifdef WORDS_BIGENDIAN
    WORD high = get_word();
    WORD low = get_word();
#else
    WORD low = get_word();
    WORD high = get_word();
#endif
    return low | (high << 16);
}

/* get a string from the current resource file */
static void get_string( struct string_id *str )
{
    if (*file_pos == 0xff)
    {
        get_byte();  /* skip the 0xff */
        str->str = NULL;
        str->id = get_word();
    }
    else
    {
        char *p = xmalloc( (strlen((char *)file_pos) + 1) );
        str->str = p;
        str->id = 0;
        while ((*p++ = get_byte()));
    }
}

/* load the next resource from the current file */
static void load_next_resource(void)
{
    struct resource *res = add_resource();

    get_string( &res->type );
    get_string( &res->name );
    res->memopt    = get_word();
    res->data_size = get_dword();
    res->data      = file_pos;
    file_pos += res->data_size;
    if (file_pos > file_end) fatal_error( "%s is a truncated/corrupted file\n", file_name );
}

/* load a Win16 .res file */
void load_res16_file( const char *name )
{
    int fd;
    void *base;
    struct stat st;

    if ((fd = open( name, O_RDONLY )) == -1) fatal_perror( "Cannot open %s", name );
    if ((fstat( fd, &st ) == -1)) fatal_perror( "Cannot stat %s", name );
    if (!st.st_size) fatal_error( "%s is an empty file\n" );
#ifdef	HAVE_MMAP
    if ((base = mmap( NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0 )) == (void*)-1)
#endif	/* HAVE_MMAP */
    {
        base = xmalloc( st.st_size );
        if (read( fd, base, st.st_size ) != st.st_size)
            fatal_error( "Cannot read %s\n", name );
    }

    file_name = name;
    file_pos  = base;
    file_end  = file_pos + st.st_size;
    while (file_pos < file_end) load_next_resource();
}

/* compare two strings/ids */
static int cmp_string( const struct string_id *str1, const struct string_id *str2 )
{
    if (!str1->str)
    {
        if (!str2->str) return str1->id - str2->id;
        return 1;  /* an id compares larger than a string */
    }
    if (!str2->str) return -1;
    return strcasecmp( str1->str, str2->str );
}

/* compare two resources for sorting the resource directory */
/* resources are stored first by type, then by name */
static int cmp_res( const void *ptr1, const void *ptr2 )
{
    const struct resource *res1 = ptr1;
    const struct resource *res2 = ptr2;
    int ret;

    if ((ret = cmp_string( &res1->type, &res2->type ))) return ret;
    return cmp_string( &res1->name, &res2->name );
}

/* build the 2-level (type,name) resource tree */
static void build_resource_tree(void)
{
    int i;
    struct res_type *type = NULL;

    qsort( resources, nb_resources, sizeof(*resources), cmp_res );

    for (i = 0; i < nb_resources; i++)
    {
        if (!i || cmp_string( &resources[i].type, &resources[i-1].type ))  /* new type */
            type = add_type( &resources[i] );
        type->nb_names++;
    }
}

inline static void put_byte( unsigned char **buffer, unsigned char val )
{
    *(*buffer)++ = val;
}

inline static void put_word( unsigned char **buffer, WORD val )
{
#ifdef WORDS_BIGENDIAN
    put_byte( buffer, HIBYTE(val) );
    put_byte( buffer, LOBYTE(val) );
#else
    put_byte( buffer, LOBYTE(val) );
    put_byte( buffer, HIBYTE(val) );
#endif
}

/* output a string preceded by its length */
static void output_string( unsigned char **buffer, const char *str )
{
    int len = strlen(str);
    put_byte( buffer, len );
    while (len--) put_byte( buffer, *str++ );
}

/* output the resource data */
int output_res16_data( FILE *outfile )
{
    const struct resource *res;
    unsigned char *buffer, *p;
    int i, total;

    if (!nb_resources) return 0;

    for (i = total = 0, res = resources; i < nb_resources; i++, res++)
        total += (res->data_size + ALIGN_MASK) & ~ALIGN_MASK;

    buffer = p = xmalloc( total );
    for (i = 0, res = resources; i < nb_resources; i++, res++)
    {
        memcpy( p, res->data, res->data_size );
        p += res->data_size;
        while ((int)p & ALIGN_MASK) *p++ = 0;
    }
    dump_bytes( outfile, buffer, total, "resource_data", 1 );
    free( buffer );
    return total;
}

/* output the resource definitions */
int output_res16_directory( unsigned char *buffer )
{
    int i, offset, res_offset = 0;
    unsigned int j;
    const struct res_type *type;
    const struct resource *res;
    unsigned char *start = buffer;

    build_resource_tree();

    offset = 4;  /* alignment + terminator */
    offset += nb_types * 8;  /* typeinfo structures */
    offset += nb_resources * 12;  /* nameinfo structures */

    put_word( &buffer, ALIGNMENT );

    /* type and name structures */

    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        if (type->type->str)
        {
            put_word( &buffer, offset );
            offset += strlen(type->type->str) + 1;
        }
        else
            put_word( &buffer, type->type->id | 0x8000 );

        put_word( &buffer, type->nb_names );
        put_word( &buffer, 0 );
        put_word( &buffer, 0 );

        for (j = 0, res = type->res; j < type->nb_names; j++, res++)
        {
            put_word( &buffer, res_offset >> ALIGNMENT );
            put_word( &buffer, (res->data_size + ALIGN_MASK) >> ALIGNMENT );
            put_word( &buffer, res->memopt );
            if (res->name.str)
            {
                put_word( &buffer, offset );
                offset += strlen(res->name.str) + 1;
            }
            else
                put_word( &buffer, res->name.id | 0x8000 );
            put_word( &buffer, 0 );
            put_word( &buffer, 0 );
            res_offset += (res->data_size + ALIGN_MASK) & ~ALIGN_MASK;
        }
    }
    put_word( &buffer, 0 );  /* terminator */

    /* name strings */

    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        if (type->type->str) output_string( &buffer, type->type->str );
        for (j = 0, res = type->res; j < type->nb_names; j++, res++)
        {
            if (res->name.str) output_string( &buffer, res->name.str );
        }
    }
    put_byte( &buffer, 0 );  /* names terminator */
    if ((buffer - start) & 1) put_byte( &buffer, 0 );  /* align on word boundary */

    return buffer - start;
}
