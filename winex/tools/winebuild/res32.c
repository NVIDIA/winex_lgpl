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
#include "wine/unicode.h"
#include "build.h"

/* Unicode string or integer id */
struct string_id
{
    WCHAR *str;  /* ptr to Unicode string */
    WORD   id;   /* integer id if str is NULL */
};

/* descriptor for a resource */
struct resource
{
    struct string_id type;
    struct string_id name;
    const void      *data;
    unsigned int     data_size;
    WORD             lang;
};

/* name level of the resource tree */
struct res_name
{
    const struct string_id  *name;         /* name */
    const struct resource   *res;          /* resource */
    int                      nb_languages; /* number of languages */
};

/* type level of the resource tree */
struct res_type
{
    const struct string_id  *type;         /* type name */
    struct res_name         *names;        /* names array */
    unsigned int             nb_names;     /* total number of names */
    unsigned int             nb_id_names;  /* number of names that have a numeric id */
};

static struct resource *resources;
static int nb_resources;

static struct res_type *res_types;
static int nb_types;     /* total number of types */
static int nb_id_types;  /* number of types that have a numeric id */

static const unsigned char *file_pos;   /* current position in resource file */
static const unsigned char *file_end;   /* end of resource file */
static const char *file_name;  /* current resource file name */


inline static struct resource *add_resource(void)
{
    resources = xrealloc( resources, (nb_resources + 1) * sizeof(*resources) );
    return &resources[nb_resources++];
}

static struct res_name *add_name( struct res_type *type, const struct resource *res )
{
    struct res_name *name;
    type->names = xrealloc( type->names, (type->nb_names + 1) * sizeof(*type->names) );
    name = &type->names[type->nb_names++];
    name->name         = &res->name;
    name->res          = res;
    name->nb_languages = 1;
    if (!name->name->str) type->nb_id_names++;
    return name;
}

static struct res_type *add_type( const struct resource *res )
{
    struct res_type *type;
    res_types = xrealloc( res_types, (nb_types + 1) * sizeof(*res_types) );
    type = &res_types[nb_types++];
    type->type        = &res->type;
    type->names       = NULL;
    type->nb_names    = 0;
    type->nb_id_names = 0;
    if (!type->type->str) nb_id_types++;
    return type;
}

/* get the next word from the current resource file */
static WORD get_word(void)
{
    WORD ret = *(WORD *)file_pos;
    file_pos += sizeof(WORD);
    if (file_pos > file_end) fatal_error( "%s is a truncated file\n", file_name );
    return ret;
}

/* get the next dword from the current resource file */
static DWORD get_dword(void)
{
    DWORD ret = *(DWORD *)file_pos;
    file_pos += sizeof(DWORD);
    if (file_pos > file_end) fatal_error( "%s is a truncated file\n", file_name );
    return ret;
}

/* get a string from the current resource file */
static void get_string( struct string_id *str )
{
    if (*(WCHAR *)file_pos == 0xffff)
    {
        get_word();  /* skip the 0xffff */
        str->str = NULL;
        str->id = get_word();
    }
    else
    {
        WCHAR *p = xmalloc( (strlenW((WCHAR*)file_pos) + 1) * sizeof(WCHAR) );
        str->str = p;
        str->id  = 0;
        while ((*p++ = get_word()));
    }
}

/* check the file header */
/* all values must be zero except header size */
static void check_header(void)
{
    if (get_dword()) goto error;        /* data size */
    if (get_dword() != 32) goto error;  /* header size */
    if (get_word() != 0xffff || get_word()) goto error;  /* type, must be id 0 */
    if (get_word() != 0xffff || get_word()) goto error;  /* name, must be id 0 */
    if (get_dword()) goto error;        /* data version */
    if (get_word()) goto error;         /* mem options */
    if (get_word()) goto error;         /* language */
    if (get_dword()) goto error;        /* version */
    if (get_dword()) goto error;        /* characteristics */
    return;
 error:
    fatal_error( "%s is not a valid Win32 resource file\n", file_name );
}

/* load the next resource from the current file */
static void load_next_resource(void)
{
    DWORD hdr_size;
    struct resource *res = add_resource();

    res->data_size = (get_dword() + 3) & ~3;
    hdr_size = get_dword();
    if (hdr_size & 3) fatal_error( "%s header size not aligned\n", file_name );

    res->data = file_pos - 2*sizeof(DWORD) + hdr_size;
    get_string( &res->type );
    get_string( &res->name );
    if ((int)file_pos & 2) get_word();  /* align to dword boundary */
    get_dword();                        /* skip data version */
    get_word();                         /* skip mem options */
    res->lang = get_word();
    get_dword();                        /* skip version */
    get_dword();                        /* skip characteristics */

    file_pos = (unsigned char *)res->data + res->data_size;
    if (file_pos > file_end) fatal_error( "%s is a truncated file\n", file_name );
}

/* load a Win32 .res file */
void load_res32_file( const char *name )
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
    check_header();
    while (file_pos < file_end) load_next_resource();
}

/* compare two unicode strings/ids */
static int cmp_string( const struct string_id *str1, const struct string_id *str2 )
{
    if (!str1->str)
    {
        if (!str2->str) return str1->id - str2->id;
        return 1;  /* an id compares larger than a string */
    }
    if (!str2->str) return -1;
    return strcmpiW( str1->str, str2->str );
}

/* compare two resources for sorting the resource directory */
/* resources are stored first by type, then by name, then by language */
static int cmp_res( const void *ptr1, const void *ptr2 )
{
    const struct resource *res1 = ptr1;
    const struct resource *res2 = ptr2;
    int ret;

    if ((ret = cmp_string( &res1->type, &res2->type ))) return ret;
    if ((ret = cmp_string( &res1->name, &res2->name ))) return ret;
    return res1->lang - res2->lang;
}

/* build the 3-level (type,name,language) resource tree */
static void build_resource_tree(void)
{
    int i;
    struct res_type *type = NULL;
    struct res_name *name = NULL;

    qsort( resources, nb_resources, sizeof(*resources), cmp_res );

    for (i = 0; i < nb_resources; i++)
    {
        if (!i || cmp_string( &resources[i].type, &resources[i-1].type ))  /* new type */
        {
            type = add_type( &resources[i] );
            name = add_name( type, &resources[i] );
        }
        else if (cmp_string( &resources[i].name, &resources[i-1].name )) /* new name */
        {
            name = add_name( type, &resources[i] );
        }
        else name->nb_languages++;
    }
}

/* output a Unicode string */
static void output_string( FILE *outfile, const WCHAR *name )
{
    int i, len = strlenW(name);
    fprintf( outfile, "0x%04x", len );
    for (i = 0; i < len; i++) fprintf( outfile, ", 0x%04x", name[i] );
    fprintf( outfile, " /* " );
    for (i = 0; i < len; i++) fprintf( outfile, "%c", isprint((char)name[i]) ? (char)name[i] : '?' );
    fprintf( outfile, " */" );
}

/* output the resource definitions */
int output_resources( FILE *outfile )
{
    int i, j, k;
    unsigned int n;
    const struct res_type *type;
    const struct res_name *name;
    const struct resource *res;

    if (!nb_resources) return 0;

    build_resource_tree();

    /* resource data */

    for (i = 0, res = resources; i < nb_resources; i++, res++)
    {
        const unsigned int *p = res->data;
        int size = res->data_size / 4;
        /* dump data as ints to ensure correct alignment */
        fprintf( outfile, "static const unsigned int res_%d[%d] = {\n  ", i, size );
        for (j = 0; j < size - 1; j++, p++)
        {
            fprintf( outfile, "0x%08x,", *p );
            if ((j % 8) == 7) fprintf( outfile, "\n  " );
        }
        fprintf( outfile, "0x%08x\n};\n\n", *p );
    }

    /* directory structures */

    fprintf( outfile, "struct res_dir {\n" );
    fprintf( outfile, "  unsigned int Characteristics;\n" );
    fprintf( outfile, "  unsigned int TimeDateStamp;\n" );
    fprintf( outfile, "  unsigned short MajorVersion, MinorVersion;\n" );
    fprintf( outfile, "  unsigned short NumerOfNamedEntries, NumberOfIdEntries;\n};\n\n" );
    fprintf( outfile, "struct res_dir_entry {\n" );
    fprintf( outfile, "  unsigned int Name;\n" );
    fprintf( outfile, "  unsigned int OffsetToData;\n};\n\n" );
    fprintf( outfile, "struct res_data_entry {\n" );
    fprintf( outfile, "  const unsigned int *OffsetToData;\n" );
    fprintf( outfile, "  unsigned int Size;\n" );
    fprintf( outfile, "  unsigned int CodePage;\n" );
    fprintf( outfile, "  unsigned int ResourceHandle;\n};\n\n" );

    /* resource directory definition */

    fprintf( outfile, "#define OFFSETOF(field) ((char*)&((struct res_struct *)0)->field - (char*)((struct res_struct *) 0))\n" );
    fprintf( outfile, "static struct res_struct{\n" );
    fprintf( outfile, "  struct res_dir        type_dir;\n" );
    fprintf( outfile, "  struct res_dir_entry  type_entries[%d];\n", nb_types );

    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        fprintf( outfile, "  struct res_dir        name_%d_dir;\n", i );
        fprintf( outfile, "  struct res_dir_entry  name_%d_entries[%d];\n", i, type->nb_names );
        for (n = 0, name = type->names; n < type->nb_names; n++, name++)
        {
            fprintf( outfile, "  struct res_dir        lang_%d_%d_dir;\n", i, n );
            fprintf( outfile, "  struct res_dir_entry  lang_%d_%d_entries[%d];\n",
                     i, n, name->nb_languages );
        }
    }

    fprintf( outfile, "  struct res_data_entry data_entries[%d];\n", nb_resources );

    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        if (type->type->str)
            fprintf( outfile, "  unsigned short        type_%d_name[%d];\n",
                     i, strlenW(type->type->str)+1 );
        for (n = 0, name = type->names; n < type->nb_names; n++, name++)
        {
            if (name->name->str)
                fprintf( outfile, "  unsigned short        name_%d_%d_name[%d];\n",
                         i, n, strlenW(name->name->str)+1 );
        }
    }

    /* resource directory contents */

    fprintf( outfile, "} resources = {\n" );
    fprintf( outfile, "  { 0, 0, 0, 0, %d, %d },\n", nb_types - nb_id_types, nb_id_types );

    /* dump the type directory */
    fprintf( outfile, "  {\n" );
    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        if (!type->type->str)
            fprintf( outfile, "    { 0x%04x, OFFSETOF(name_%d_dir) | 0x80000000 },\n",
                     type->type->id, i );
        else
            fprintf( outfile, "    { OFFSETOF(type_%d_name) | 0x80000000, OFFSETOF(name_%d_dir) | 0x80000000 },\n",
                     i, i );
    }
    fprintf( outfile, "  },\n" );

    /* dump the names and languages directories */
    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        fprintf( outfile, "  { 0, 0, 0, 0, %d, %d }, /* name_%d_dir */\n  {\n",
                 type->nb_names - type->nb_id_names, type->nb_id_names, i );
        for (n = 0, name = type->names; n < type->nb_names; n++, name++)
        {
            if (!name->name->str)
                fprintf( outfile, "    { 0x%04x, OFFSETOF(lang_%d_%d_dir) | 0x80000000 },\n",
                         name->name->id, i, n );
            else
                fprintf( outfile, "    { OFFSETOF(name_%d_%d_name) | 0x80000000, OFFSETOF(lang_%d_%d_dir) | 0x80000000 },\n",
                         i, n, i, n );
        }
        fprintf( outfile, "  },\n" );

        for (n = 0, name = type->names; n < type->nb_names; n++, name++)
        {
            fprintf( outfile, "  { 0, 0, 0, 0, 0, %d }, /* lang_%d_%d_dir */\n  {\n",
                     name->nb_languages, i, n );
            for (k = 0, res = name->res; k < name->nb_languages; k++, res++)
            {
                fprintf( outfile, "    { 0x%04x, OFFSETOF(data_entries[%d]) },\n",
                         res->lang, res - resources );
            }
            fprintf( outfile, "  },\n" );
        }
    }

    /* dump the resource data entries */
    fprintf( outfile, "  {\n" );
    for (i = 0, res = resources; i < nb_resources; i++, res++)
    {
        fprintf( outfile, "    { res_%d, sizeof(res_%d), 0, 0 },\n", i, i );
    }

    /* dump the name strings */
    for (i = 0, type = res_types; i < nb_types; i++, type++)
    {
        if (type->type->str)
        {
            fprintf( outfile, "  },\n  { " );
            output_string( outfile, type->type->str );
        }
        for (n = 0, name = type->names; n < type->nb_names; n++, name++)
        {
            if (name->name->str)
            {
                fprintf( outfile, "  },\n  { " );
                output_string( outfile, name->name->str );
            }
        }
    }
    fprintf( outfile, "  }\n};\n#undef OFFSETOF\n\n" );
    return nb_resources;
}
