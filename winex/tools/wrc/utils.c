/*
 * Utility routines
 *
 * Copyright 1998 Bertho A. Stultiens
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "wine/unicode.h"
#include "wrc.h"
#include "utils.h"
#include "parser.h"
#include "preproc.h"

/* #define WANT_NEAR_INDICATION */

static const union cptable *current_codepage;

#ifdef WANT_NEAR_INDICATION
void make_print(char *str)
{
	while(*str)
	{
		if(!isprint(*str))
			*str = ' ';
		str++;
	}
}
#endif

static void generic_msg(const char *s, const char *t, const char *n, va_list ap)
{
	fprintf(stderr, "%s:%d:%d: %s: ", input_name ? input_name : "stdin", line_number, char_number, t);
	vfprintf(stderr, s, ap);
#ifdef WANT_NEAR_INDICATION
	{
		char *cpy;
		if(n)
		{
			cpy = xstrdup(n);
			make_print(cpy);
			fprintf(stderr, " near '%s'", cpy);
			free(cpy);
		}
	}
#endif
	fprintf(stderr, "\n");
}


int yyerror(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Error", yytext, ap);
	va_end(ap);
	exit(1);
	return 1;
}

int yywarning(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Warning", yytext, ap);
	va_end(ap);
	return 0;
}

int pperror(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Error", pptext, ap);
	va_end(ap);
	exit(1);
	return 1;
}

int ppwarning(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Warning", pptext, ap);
	va_end(ap);
	return 0;
}


void internal_error(const char *file, int line, const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Internal error (please report) %s %d: ", file, line);
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(3);
}

void error(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(2);
}

void warning(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Warning: ");
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void chat(const char *s, ...)
{
	if(debuglevel & DEBUGLEVEL_CHAT)
	{
		va_list ap;
		va_start(ap, s);
		fprintf(stderr, "FYI: ");
		vfprintf(stderr, s, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}
}

char *dup_basename(const char *name, const char *ext)
{
	int namelen;
	int extlen = strlen(ext);
	char *base;
	char *slash;

	if(!name)
		name = "wrc.tab";

	slash = strrchr(name, '/');
	if (slash)
		name = slash + 1;

	namelen = strlen(name);

	/* +4 for later extension and +1 for '\0' */
	base = (char *)xmalloc(namelen +4 +1);
	strcpy(base, name);
	if(!strcasecmp(name + namelen-extlen, ext))
	{
		base[namelen - extlen] = '\0';
	}
	return base;
}

void *xmalloc(size_t size)
{
    void *res;

    assert(size > 0);
    res = malloc(size);
    if(res == NULL)
    {
	error("Virtual memory exhausted.\n");
    }
    /*
     * We set it to 0.
     * This is *paramount* because we depend on it
     * just about everywhere in the rest of the code.
     */
    memset(res, 0, size);
    return res;
}


void *xrealloc(void *p, size_t size)
{
    void *res;

    assert(size > 0);
    res = realloc(p, size);
    if(res == NULL)
    {
	error("Virtual memory exhausted.\n");
    }
    return res;
}

char *xstrdup(const char *str)
{
	char *s;

	assert(str != NULL);
	s = (char *)xmalloc(strlen(str)+1);
	return strcpy(s, str);
}

WCHAR *dupcstr2wstr(const char *str)
{
	int len;
	WCHAR *ws;

	if (!current_codepage) set_language( LANG_NEUTRAL, SUBLANG_NEUTRAL );
	len = wine_cp_mbstowcs( current_codepage, 0, str, strlen(str), NULL, 0 );
	ws = xmalloc( sizeof(WCHAR) * (len + 1) );
	len = wine_cp_mbstowcs( current_codepage, 0, str, strlen(str), ws, len );
	ws[len] = 0;
	return ws;
}

char *dupwstr2cstr(const WCHAR *str)
{
	int len;
	char *cs;

	if (!current_codepage) set_language( LANG_NEUTRAL, SUBLANG_NEUTRAL );
	len = wine_cp_wcstombs( current_codepage, 0, str, strlenW(str), NULL, 0, NULL, NULL );
	cs = xmalloc( len + 1 );
	len = wine_cp_wcstombs( current_codepage, 0, str, strlenW(str),  cs, len, NULL, NULL );
	cs[len] = 0;
	return cs;
}

/*
 *****************************************************************************
 * Function	: compare_name_id
 * Syntax	: int compare_name_id(name_id_t *n1, name_id_t *n2)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
int compare_name_id(name_id_t *n1, name_id_t *n2)
{
	if(n1->type == name_ord && n2->type == name_ord)
	{
		return n1->name.i_name - n2->name.i_name;
	}
	else if(n1->type == name_str && n2->type == name_str)
	{
		if(n1->name.s_name->type == str_char
		&& n2->name.s_name->type == str_char)
		{
			return strcasecmp(n1->name.s_name->str.cstr, n2->name.s_name->str.cstr);
		}
		else if(n1->name.s_name->type == str_unicode
		&& n2->name.s_name->type == str_unicode)
		{
			return strcmpiW(n1->name.s_name->str.wstr, n2->name.s_name->str.wstr);
		}
		else
		{
			internal_error(__FILE__, __LINE__, "Can't yet compare strings of mixed type");
		}
	}
	else if(n1->type == name_ord && n2->type == name_str)
		return 1;
	else if(n1->type == name_str && n2->type == name_ord)
		return -1;
	else
		internal_error(__FILE__, __LINE__, "Comparing name-ids with unknown types (%d, %d)",
				n1->type, n2->type);

	return 0; /* Keep the compiler happy */
}

string_t *convert_string(const string_t *str, enum str_e type)
{
        string_t *ret = xmalloc(sizeof(*ret));

        if((str->type == str_char) && (type == str_unicode))
	{
		ret->str.wstr = dupcstr2wstr(str->str.cstr);
		ret->type     = str_unicode;
		ret->size     = strlenW(ret->str.wstr);
	}
	else if((str->type == str_unicode) && (type == str_char))
	{
	        ret->str.cstr = dupwstr2cstr(str->str.wstr);
	        ret->type     = str_char;
	        ret->size     = strlen(ret->str.cstr);
	}
	else if(str->type == str_unicode)
        {
	        ret->type     = str_unicode;
		ret->size     = strlenW(str->str.wstr);
		ret->str.wstr = xmalloc(sizeof(WCHAR)*(ret->size+1));
		strcpyW(ret->str.wstr, str->str.wstr);
	}
	else /* str->type == str_char */
        {
	        ret->type     = str_char;
		ret->size     = strlen(str->str.cstr);
		ret->str.cstr = xmalloc( ret->size + 1 );
		strcpy(ret->str.cstr, str->str.cstr);
        }
	return ret;
}


struct lang2cp
{
    unsigned short lang;
    unsigned short sublang;
    unsigned int   cp;
} lang2cp_t;

/* language to codepage conversion table */
/* specific sublanguages need only be specified if their codepage */
/* differs from the default (SUBLANG_NEUTRAL) */
static const struct lang2cp lang2cps[] =
{
    { LANG_AFRIKAANS,      SUBLANG_NEUTRAL,              1252 },
    { LANG_ALBANIAN,       SUBLANG_NEUTRAL,              1250 },
    { LANG_ARABIC,         SUBLANG_NEUTRAL,              1256 },
    { LANG_BASQUE,         SUBLANG_NEUTRAL,              1252 },
    { LANG_BELARUSIAN,     SUBLANG_NEUTRAL,		 1251 },
    { LANG_BRETON,         SUBLANG_NEUTRAL,              1252 },
    { LANG_BULGARIAN,      SUBLANG_NEUTRAL,              1251 },
    { LANG_CATALAN,        SUBLANG_NEUTRAL,              1252 },
    { LANG_CHINESE,        SUBLANG_NEUTRAL,              936  },
    { LANG_CORNISH,        SUBLANG_NEUTRAL,              1252 },
    { LANG_CZECH,          SUBLANG_NEUTRAL,              1250 },
    { LANG_DANISH,         SUBLANG_NEUTRAL,              1252 },
    { LANG_DUTCH,          SUBLANG_NEUTRAL,              1252 },
    { LANG_ENGLISH,        SUBLANG_NEUTRAL,              1252 },
    { LANG_ESPERANTO,      SUBLANG_NEUTRAL,              1252 },
    { LANG_ESTONIAN,       SUBLANG_NEUTRAL,              1257 },
    { LANG_FAEROESE,       SUBLANG_NEUTRAL,              1252 },
    { LANG_FINNISH,        SUBLANG_NEUTRAL,              1252 },
    { LANG_FRENCH,         SUBLANG_NEUTRAL,              1252 },
    { LANG_GAELIC,         SUBLANG_NEUTRAL,              1252 },
    { LANG_GERMAN,         SUBLANG_NEUTRAL,              1252 },
    { LANG_GREEK,          SUBLANG_NEUTRAL,              1253 },
    { LANG_HEBREW,         SUBLANG_NEUTRAL,              1255 },
    { LANG_HUNGARIAN,      SUBLANG_NEUTRAL,              1250 },
    { LANG_ICELANDIC,      SUBLANG_NEUTRAL,              1252 },
    { LANG_INDONESIAN,     SUBLANG_NEUTRAL,              1252 },
    { LANG_ITALIAN,        SUBLANG_NEUTRAL,              1252 },
    { LANG_JAPANESE,       SUBLANG_NEUTRAL,              932  },
    { LANG_KOREAN,         SUBLANG_NEUTRAL,              949  },
    { LANG_LATVIAN,        SUBLANG_NEUTRAL,              1257 },
    { LANG_LITHUANIAN,     SUBLANG_NEUTRAL,              1257 },
    { LANG_MACEDONIAN,     SUBLANG_NEUTRAL,              1251 },
    { LANG_MALAY,          SUBLANG_NEUTRAL,              1252 },
    { LANG_NEUTRAL,        SUBLANG_NEUTRAL,              1252 },
    { LANG_NORWEGIAN,      SUBLANG_NEUTRAL,              1252 },
    { LANG_POLISH,         SUBLANG_NEUTRAL,              1250 },
    { LANG_PORTUGUESE,     SUBLANG_NEUTRAL,              1252 },
    { LANG_ROMANIAN,       SUBLANG_NEUTRAL,              1250 },
    { LANG_RUSSIAN,        SUBLANG_NEUTRAL,              1251 },
    { LANG_SERBO_CROATIAN, SUBLANG_NEUTRAL,              1250 },
    { LANG_SERBO_CROATIAN, SUBLANG_SERBIAN_CYRILLIC,     1251 },
    { LANG_SLOVAK,         SUBLANG_NEUTRAL,              1250 },
    { LANG_SLOVENIAN,      SUBLANG_NEUTRAL,              1250 },
    { LANG_SPANISH,        SUBLANG_NEUTRAL,              1252 },
    { LANG_SWEDISH,        SUBLANG_NEUTRAL,              1252 },
    { LANG_THAI,           SUBLANG_NEUTRAL,              874  },
    { LANG_TURKISH,        SUBLANG_NEUTRAL,              1254 },
    { LANG_UKRAINIAN,      SUBLANG_NEUTRAL,              1251 },
    { LANG_VIETNAMESE,     SUBLANG_NEUTRAL,              1258 },
    { LANG_WALON,          SUBLANG_NEUTRAL,              1252 },
    { LANG_WELSH,          SUBLANG_NEUTRAL,              1252 }
};

void set_language( unsigned short lang, unsigned short sublang )
{
    unsigned int i;
    unsigned int cp = 0, defcp = 0;

    for (i = 0; i < sizeof(lang2cps)/sizeof(lang2cps[0]); i++)
    {
        if (lang2cps[i].lang != lang) continue;
        if (lang2cps[i].sublang == sublang)
        {
            cp = lang2cps[i].cp;
            break;
        }
        if (lang2cps[i].sublang == SUBLANG_NEUTRAL) defcp = lang2cps[i].cp;
    }

    if (!cp) cp = defcp;
    if (!cp)
        error( "No codepage value for language %04x", MAKELANGID(lang,sublang) );
    if (!(current_codepage = wine_cp_get_table( cp )))
        error( "Bad codepage %d for language %04x", cp, MAKELANGID(lang,sublang) );
}
