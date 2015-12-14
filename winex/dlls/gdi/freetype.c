/*
 * FontConfig and Freetype stuff for gdi fonts (WineEng* implementations)
 *
 * Copyright (c) TransGaming Technologies 2003-2004
 * david@transgaming.com
 *
 * Copyright (C) Huw D M Davies for CodeWeavers 2001
 */

#include "config.h"

#include "windef.h"
#include "winerror.h"
#include "winreg.h"
#include "wingdi.h"
#include "gdi.h"
#include "font.h"

#include "wine/unicode.h"
#include "wine/port.h"
#include "wine/debug.h"
#include "wine/server.h"

#include <stdio.h>
#include <dirent.h>
#include <assert.h>

#ifdef __APPLE__
/* FIXME HACK - getting garbage from fcPatternGet ATM */
#undef HAVE_FONTCONFIG
#endif

WINE_DEFAULT_DEBUG_CHANNEL(font);

#define USE_FONT_CACHE 1

#ifdef HAVE_FREETYPE

#ifdef HAVE_FT2BUILD_H
# include <ft2build.h>
#endif
#ifdef HAVE_FREETYPE_FREETYPE_H
# include <freetype/freetype.h>
#endif
#ifdef HAVE_FREETYPE_FTGLYPH_H
# include <freetype/ftglyph.h>
#endif
#ifdef HAVE_FREETYPE_TTTABLES_H
# include <freetype/tttables.h>
#endif
#ifdef HAVE_FREETYPE_FTSNAMES_H
# include <freetype/ftsnames.h>
#else
# ifdef HAVE_FREETYPE_FTNAMES_H
# include <freetype/ftnames.h>
# endif
#endif
#ifdef HAVE_FREETYPE_TTNAMEID_H
# include <freetype/ttnameid.h>
#endif
#ifdef HAVE_FREETYPE_FTOUTLN_H
# include <freetype/ftoutln.h>
#endif
#ifdef HAVE_FREETYPE_INTERNAL_SFNT_H
# include <freetype/internal/sfnt.h>
#endif
#ifdef HAVE_FREETYPE_FTTRIGON_H
# include <freetype/fttrigon.h>
#endif

#if defined(__GNUC__)
#define MAKE_FUNCPTR(f) static typeof(f) * p##f
#else
typedef int (*unknown_function_type)(int,...);
#define MAKE_FUNCPTR(f) static unknown_function_type p##f;
#endif

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
# ifndef SONAME_LIBFONTCONFIG
#  ifdef __APPLE__
#   define SONAME_LIBFONTCONFIG "libfontconfig.1.dylib"
#  else
#   define SONAME_LIBFONTCONFIG "libfontconfig.so.1"
#  endif
# endif

static void *fontconfig_so_handle;
MAKE_FUNCPTR(FcInit);
MAKE_FUNCPTR(FcConfigGetCurrent);
MAKE_FUNCPTR(FcConfigSubstitute);
MAKE_FUNCPTR(FcDefaultSubstitute);
MAKE_FUNCPTR(FcFontList);
MAKE_FUNCPTR(FcFontSort);
MAKE_FUNCPTR(FcFontSetDestroy);
MAKE_FUNCPTR(FcPatternCreate);
MAKE_FUNCPTR(FcPatternAdd);
MAKE_FUNCPTR(FcPatternAddWeak);
MAKE_FUNCPTR(FcPatternGet);
MAKE_FUNCPTR(FcPatternDestroy);
MAKE_FUNCPTR(FcObjectSetCreate);
MAKE_FUNCPTR(FcObjectSetAdd);
MAKE_FUNCPTR(FcObjectSetDestroy);

#endif

#ifndef SONAME_LIBFREETYPE
# ifdef __APPLE__
#  define SONAME_LIBFREETYPE "FreeType.framework/FreeType"
# else
#  define SONAME_LIBFREETYPE "libfreetype.so.6"
# endif
#endif

static void *freetype_so_handle = NULL;
MAKE_FUNCPTR(FT_Init_FreeType);
MAKE_FUNCPTR(FT_New_Face);
MAKE_FUNCPTR(FT_Done_Face);
MAKE_FUNCPTR(FT_Get_Sfnt_Table);

#if defined( HAVE_FT_LOAD_SFNT_TABLE )
MAKE_FUNCPTR(FT_Load_Sfnt_Table);
#else
static int (*pFT_Load_Sfnt_Table)(int,...);
#endif
MAKE_FUNCPTR(FT_Set_Pixel_Sizes);
MAKE_FUNCPTR(FT_Load_Glyph);
MAKE_FUNCPTR(FT_Vector_Rotate);
MAKE_FUNCPTR(FT_Outline_Transform);
MAKE_FUNCPTR(FT_Outline_Translate);
MAKE_FUNCPTR(FT_Outline_Get_Bitmap);
MAKE_FUNCPTR(FT_Cos);
MAKE_FUNCPTR(FT_Sin);
#ifdef FT_MULFIX_INLINED
#define pFT_MulFix FT_MulFix
#else
MAKE_FUNCPTR(FT_MulFix);
#endif
MAKE_FUNCPTR(FT_Get_Char_Index);
MAKE_FUNCPTR(FT_Select_Charmap);
MAKE_FUNCPTR(FT_Get_First_Char);
MAKE_FUNCPTR(FT_Get_Next_Char);

#undef MAKE_FUNCPTR

static FT_Library library = 0;

/* if modifying this, you must modify the cache file structure. */
typedef struct tagFace {
    WCHAR *StyleName;
    char *file;
    BOOL Italic;
    BOOL Bold;
    INT Height;
    int SizeIndex;
    DWORD fsCsb[2]; /* codepage bitfield from FONTSIGNATURE */
    struct tagFace *next;

    /* cache: */
    time_t lastModified; /* of file */
} Face;

/* if modifying this, you must modify the cache file structure. */
typedef struct tagFamily {
    WCHAR *FamilyName;
    Face *FirstFace;
    BOOL ScalableFamily; /* to simplify things an entire family will be either
                            scalable or not */
    struct tagFamily *next;

    /* cache: */
    BOOL IsCacheEntry;
} Family;

typedef struct
{
    FT_Pos left, right, top, bottom;
    FT_Angle angle;
} ftGlyphMetrics; /* used internally to store some info needed for rendering outlines */

typedef struct {
    GLYPHMETRICS gm;
    ftGlyphMetrics extraMetrics;
    INT adv; /* These three hold to widths of the unrotated chars */
    INT lsb;
    INT bbx;
    BOOL init;
} GM;

struct tagGdiFont {
    FT_Face ft_face;
    int charset;
    BOOL fake_italic;
    BOOL fake_bold;
    INT orientation;
    GM *gm;
    DWORD gmsize;
    HFONT hfont;
    SHORT yMax;
    SHORT yMin;
    INT fixedSize;
    TEXTMETRICW *cachedMetrics;
    XFORM xform;
    struct tagGdiFont *next;
};

typedef struct TAGftFontAlias
{
    LPWSTR                 faTypeFace;
    Family                 *faRealFamily;
    struct TAGftFontAlias  *next;
} ftFontAlias;

static ftFontAlias *ftAliasTable = NULL;


#define INIT_GM_SIZE 128

static GdiFont GdiFontList = NULL;

static Family *FontList = NULL;

static WCHAR TimesNewRoman[] = {'T','i','m','e','s',' ','N','e','w',' ',
                                'R','o','m','a','n','\0'};
static WCHAR MSSerif[] = {'M','S',' ','S','e','r','i','f','\0'};
static WCHAR Helv[] = {'H','e','l','v','\0'};
static WCHAR Arial[] = {'A','r','i','a','l','\0'};
static WCHAR MSSansSerif[] = {'M','S',' ','S','a','n','s',' ',
                               'S','e','r','i','f','\0'};
static WCHAR System[] = {'S','y','s','t','e','m','\0'};
static WCHAR Tahoma[] = {'T', 'a', 'h', 'o', 'm', 'a', '\0'};
static WCHAR CourierNew[] = {'C','o','u','r','i','e','r',' ','N','e','w','\0'};
static WCHAR FixedSys[] = {'F','i','x','e','d','S','y','s','\0'};


static WCHAR ArabicW[] = {'A','r','a','b','i','c','\0'};
static WCHAR BalticW[] = {'B','a','l','t','i','c','\0'};
static WCHAR Central_EuropeanW[] = {'C','e','n','t','r','a','l',' ',
                                    'E','u','r','o','p','e','a','n','\0'};
static WCHAR CyrillicW[] = {'C','y','r','i','l','l','i','c','\0'};
static WCHAR GreekW[] = {'G','r','e','e','k','\0'};
static WCHAR HebrewW[] = {'H','e','b','r','e','w','\0'};
static WCHAR SymbolW[] = {'S','y','m','b','o','l','\0'};
static WCHAR ThaiW[] = {'T','h','a','i','\0'};
static WCHAR TurkishW[] = {'T','u','r','k','i','s','h','\0'};
static WCHAR VietnameseW[] = {'V','i','e','t','n','a','m','e','s','e','\0'};
static WCHAR WesternW[] = {'W','e','s','t','e','r','n','\0'};
static WCHAR JapaneseW[] = {'J','a','p','a','n','e','s','e','\0'};
static WCHAR HangulW[] = { 'H','a','n','g','u','l','\0' };
static WCHAR ChineseBig5W[] = { 'C','H','I','N','E','S','E','_','B','I','G','5','\0' };
static WCHAR ChineseGB2312W[] = { 'C','H','I','N','E','S','E','_','G','B','2','3','1','2','\0' };

static WCHAR *ElfScriptsW[32] = { /* these are in the order of the fsCsb[0] bits */
    WesternW, /*00*/
    Central_EuropeanW, /*01*/
    CyrillicW, /*02*/
    GreekW,/*03*/
    TurkishW,/*04*/
    HebrewW,/*05*/
    ArabicW,/*06*/
    BalticW,/*07*/
    VietnameseW, /*08*/
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 9 - 15 - reserved for ANSI */
    ThaiW,/*16*/
    JapaneseW, /* 17 - Shift-JIS */
    ChineseGB2312W, /* 18 - Chinese simplified */
    HangulW, /* 19 - Korean (Hangeul) */
    ChineseBig5W, /* 20 - Chinese traditional */
    NULL, /* 21 - Korean (Johab) */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 22 - 29 reserved for alternate ANSI and OEM */
    NULL, /* 30 - reserved by system */
    SymbolW /*31*/
};

/* defaults  -  aliasing should take care of fallbacks */
static WCHAR *defFixed = CourierNew;
static WCHAR *defSerif = TimesNewRoman;
static WCHAR *defSans = Arial;

/* -------------- CACHE STUFF ------------------ */

#ifdef USE_FONT_CACHE
#define FTFONTCACHE_VERSION 2
const char *CacheFileName = "ftfontcache";

typedef struct tagIgnoredFiles
{
    char *filename;
    time_t lastModified;

    struct tagIgnoredFiles *next;
} IgnoredFiles;

/* File format is as follows, it is not endian-safe, and may not be safe accross
 * other different machines, such as machines with non-standard integer sizes.
 * All data is dumped as binary for simplicity. It is assumed that the cache will
 * always be used on the same machine, anyhow. The cache does carry a small safety
 * net, in that it tells us the size of each element, and the version, in text.
 * If modifying this, please ensure you increase the FTFONTCACHE_VERSION.
 * This stuff comes from Face and Family structures, which it basically
 * serializes.
 * Lengths of strings are binary lengths not including \0.
 *
 * - version of file [string: "ftfontcache version %i\n"],
 * - sizeof(int), sizeof(char), sizeof(WCHAR), sizeof(BOOL), sizeof(INT), sizeof(time_t)
 *       [string: "%i, %i, %i, %i, %i, %i\n"]
 * - number of family cache entries [int],
 * - entries: [ - scalable [BOOL],
 *              - number of faces,
 *              - faces [ - len of stylename (length of string) [int],
 *                        - stylename [WCHAR[]],
 *                        - len of filename [int],
 *                        - filename [char[]],
 *                        - italic [BOOL],
 *                        - bold [BOOL],
 *                        - height [INT],
 *                        - sizeindex [INT],
 *                        - fscb [INT[2]],
 *                        - filewritetime [time_t]
 *                      ]
 *            ]
 * - number of entries in ignored file cache [int],
 * - entries: [ - len of filename (length of string) [int],
 *              - filename [char[]],
 *              - filewritetime [time_t]
 *            ]
 */

Family *cachedFontList = NULL;
IgnoredFiles *ignoredFilesList = NULL;

BOOL buildingCache = FALSE;
BOOL ignoringCache = FALSE;
HANDLE cacheMutex;

BOOL using_cache = TRUE;

BOOL rebuildCache = FALSE;

static void SetRebuildCache()
{
    IgnoredFiles *cur = ignoredFilesList;

    TRACE("rebuilding cache, will delete ignored files list\n");

    rebuildCache = TRUE;

    while (cur)
    {
        IgnoredFiles *next = cur->next;
        HeapFree(GetProcessHeap(), 0, cur->filename);
        HeapFree(GetProcessHeap(), 0, cur);
        cur = next;
    }
    ignoredFilesList = NULL;
}

static void RebuildIgnoredCache()
{
    TRACE("will rebuild ignored cache\n");
    rebuildCache = TRUE;
}

static BOOL ReadCacheFile()
{
    char *filename;
    char const *confdir;
    FILE *cacheFile;
    int version, n;
    int num_families, num_ignore;
    int i;

    confdir = get_config_dir();
    filename = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                         strlen(confdir) + 1 + strlen(CacheFileName) + 1);
    strcpy(filename, confdir);
    filename[strlen(filename)] = '/';
    strcat(filename, CacheFileName);

    cacheFile = fopen(filename, "r");
    HeapFree(GetProcessHeap(), 0, filename);

    if (!cacheFile)
        return FALSE;

    TRACE("reading font cache file\n");

    /* read the file */

    /* check the basics (ie the file is the right version, made on this machine) */
    if((fscanf(cacheFile, "ftfontcache version %i\n", &version) != 1) ||
       (version != FTFONTCACHE_VERSION) ||
       (fscanf(cacheFile, "%i", &n) != 1) ||
       (n != sizeof(int)) ||
       (fscanf(cacheFile, ", %i", &n) != 1) ||
       (n != sizeof(char)) ||
       (fscanf(cacheFile, ", %i", &n) != 1) ||
       (n != sizeof(WCHAR)) ||
       (fscanf(cacheFile, ", %i", &n) != 1) ||
       (n != sizeof(BOOL)) ||
       (fscanf(cacheFile, ", %i", &n) != 1) ||
       (n != sizeof(INT)) ||
       (fscanf(cacheFile, ", %i", &n) != 1) ||
       (n != sizeof(time_t)) ||
       (fgetc(cacheFile) != '\n') )
    {
        goto filetype_error;
    }

    /* now get down to the good stuff */
#define SAFE_READ(p,str) do { if (fread(&p, sizeof(p), 1, cacheFile) != 1) { ERR("Corrupt cache: " str ); goto read_error; } } while(0)
#define SAFE_READ_PN(p, n, str)  do { if (n && fread(p, n, 1, cacheFile) != 1) { ERR("Corrupt cache: " str ); goto read_error; } } while(0)
    SAFE_READ(num_families,"num_families");
    for (i = 0; i < num_families; i++)
    {
        int j, num_faces;
        Family *new_family = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       sizeof(Family));
        new_family->IsCacheEntry = TRUE;
        new_family->next = cachedFontList;
        cachedFontList = new_family;
        SAFE_READ(n,"FamilyName length");
        new_family->FamilyName = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                          sizeof(WCHAR) * (n + 1));
        SAFE_READ_PN(new_family->FamilyName, n * sizeof(WCHAR),"FamilyName");
        SAFE_READ(new_family->ScalableFamily,"ScalableFamily");
        SAFE_READ(num_faces,"num_faces");
        TRACE("family '%s' has %i faces, about to read (scalable? %i)\n",
                debugstr_w(new_family->FamilyName),
                num_faces, new_family->ScalableFamily);
        for (j = 0; j < num_faces; j++)
        {
            Face *new_face = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       sizeof(Face));
            new_face->next = new_family->FirstFace;
            new_family->FirstFace = new_face;
            SAFE_READ(n,"StyleName length");
            new_face->StyleName = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                            sizeof(WCHAR) * (n + 1));
            SAFE_READ_PN(new_face->StyleName, n * sizeof(WCHAR), "StyleName");
            SAFE_READ(n,"face file length");
            new_face->file = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       sizeof(char) * (n + 1));
            SAFE_READ_PN(new_face->file, n * sizeof(char),"face file");
            SAFE_READ(new_face->Italic,"Italic");
            SAFE_READ(new_face->Bold,"Bold");
            SAFE_READ(new_face->Height,"Height");
            SAFE_READ(new_face->SizeIndex,"SizeIndex");
            SAFE_READ(new_face->fsCsb[0],"fsCsb[0]");
            SAFE_READ(new_face->fsCsb[1],"fsCsb[1]");
            SAFE_READ(new_face->lastModified,"lastModified");
        }
    }

    SAFE_READ(num_ignore,"num_ignore");
    for (i = 0; i < num_ignore; i++)
    {
        IgnoredFiles *new = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                      sizeof(IgnoredFiles));
        new->next = ignoredFilesList;
        ignoredFilesList = new;

        SAFE_READ(n,"(ignore)filename size");
        new->filename = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                  sizeof(char) * (n+1));
        SAFE_READ_PN(new->filename, n * sizeof(char), "(ignore)filename");
        SAFE_READ(new->lastModified,"(ignore)lastModified");
    }

#undef SAFE_READ
#undef SAFE_READ_PN

    fclose(cacheFile);
    TRACE("cache file read successfuly\n");
    return TRUE;

filetype_error:
    WARN("font cache file format has changed\n");
read_error:
    WARN("could not read font cache file, will delete it\n");
    fclose(cacheFile);
    unlink(filename);
    return FALSE;
}

static void WriteCacheFile()
{
    char *filename;
    char const *confdir;
    FILE *cacheFile;
    Family *cur_family;
    IgnoredFiles *cur_ignored;
    int n;

    TRACE("trying to write cache file\n");

    confdir = get_config_dir();
    filename = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                         strlen(confdir) + 1 + strlen(CacheFileName) + 1);
    strcpy(filename, confdir);
    filename[strlen(filename)] = '/';
    strcat(filename, CacheFileName);

    cacheFile = fopen(filename, "w");
    HeapFree(GetProcessHeap(), 0, filename);

    if (!cacheFile)
    {
        ERR("couldn't not create font cache file\n");
        return;
    }

    /* write the basic info */
    fprintf(cacheFile, "ftfontcache version %i\n", FTFONTCACHE_VERSION);
    fprintf(cacheFile, "%i, %i, %i, %i, %i, %i\n",
            sizeof(int), sizeof(char), sizeof(WCHAR),
            sizeof(BOOL), sizeof(INT), sizeof(time_t));

    /* write the rest */
#define WRITE(p,str) do { if (fwrite(&p, sizeof(p), 1, cacheFile) != 1) { ERR("Problem writing cache: " str ); } } while(0)
#define WRITE_PN(p, n, str)  do { if (n && fwrite(p, n, 1, cacheFile) != 1) { ERR("Problem writing cache: " str ); } } while(0)

    cur_family = FontList;
    n = 0;
    while (cur_family)
    {
        n++;
        cur_family = cur_family->next;
    }
    WRITE(n,"num_families");

    cur_family = FontList;
    while (cur_family)
    {
        int n;
        Face *cur_face;
        n = strlenW(cur_family->FamilyName);
        WRITE(n,"FamilyName length");
        WRITE_PN(cur_family->FamilyName, n * sizeof(WCHAR),"FamilyName");
        WRITE(cur_family->ScalableFamily,"ScalableFamily");

        cur_face = cur_family->FirstFace;
        n = 0;
        while (cur_face)
        {
            n++;
            cur_face = cur_face->next;
        }
        WRITE(n,"num faces");

        cur_face = cur_family->FirstFace;
        while (cur_face)
        {
            n = strlenW(cur_face->StyleName);
            WRITE(n,"StyleName length");
            WRITE_PN(cur_face->StyleName, n * sizeof(WCHAR),"StyleName");
            n = strlen(cur_face->file);
            WRITE(n,"face file length");
            WRITE_PN(cur_face->file, n,"face file");
            WRITE(cur_face->Italic,"Italic");
            WRITE(cur_face->Bold,"Bold");
            WRITE(cur_face->Height,"Height");
            WRITE(cur_face->SizeIndex,"SizeIndex");
            WRITE(cur_face->fsCsb[0],"fsCsb[0]");
            WRITE(cur_face->fsCsb[1],"fsCsb[1]");
            WRITE(cur_face->lastModified,"lastModified");
            cur_face = cur_face->next;
        }

        cur_family = cur_family->next;
    }

    cur_ignored = ignoredFilesList;
    n = 0;
    while (cur_ignored)
    {
        n++;
        cur_ignored = cur_ignored->next;
    }
    WRITE(n,"ignored");

    cur_ignored = ignoredFilesList;
    while (cur_ignored)
    {
        n = strlen(cur_ignored->filename);
        WRITE(n,"ignored filename length");
        WRITE_PN(cur_ignored->filename, n,"ignored filename");
        WRITE(cur_ignored->lastModified,"ignored lastModified");

        cur_ignored = cur_ignored->next;
    }
#undef WRITE
#undef WRITE_PN

    TRACE("Finished writing cache file\n");

    fclose(cacheFile);
}

static const char *FREETYPE_FONTCACHE_MUTEX_NAME =
    "__WINE__FREETYPE_FONTCACHE";
static void FontListCache_Start()
{
    SECURITY_ATTRIBUTES sec_attr;
    DWORD result;

    sec_attr.nLength = sizeof(sec_attr);
    sec_attr.lpSecurityDescriptor = NULL;
    sec_attr.bInheritHandle = TRUE;

    cacheMutex = CreateMutexA(&sec_attr, FALSE, FREETYPE_FONTCACHE_MUTEX_NAME);
      /* get an instance of the system-wide named mutex */

    if (!cacheMutex)
    {
        ERR("failed to create mutex, ignoring cache\n");
        ignoringCache = TRUE;
        return;
    }
    if (cacheMutex && GetLastError() == ERROR_ALREADY_EXISTS)
        TRACE("create instance of mutex\n");
    else if (cacheMutex)
        TRACE("created initial mutex\n");

    result = WaitForSingleObject(cacheMutex, 6 * 10000); /* wait at most one minute */
    if (result != WAIT_OBJECT_0)
    {
        /* too long, ignore cache */
        ERR("mutex wait timeout, ignoring cache\n");
        ignoringCache = TRUE;
        return;
    }
    if (ReadCacheFile())
    {
        ReleaseMutex(cacheMutex);
    }
    else
    {
        buildingCache = TRUE;
        cachedFontList = NULL;
        /* no cache file, we must build it ourselves */
    }
}

static void FontListCache_End()
{
    if (rebuildCache)
    {
        DWORD result = WaitForSingleObject(cacheMutex, 6 * 10000);
            /* wait at most one minute */
        if (result != WAIT_OBJECT_0)
        {
            ERR("can't write cache file, couldn't get lock\n");
            goto done;
        }
    }
    else if (!buildingCache) goto done;

    WriteCacheFile();

    ReleaseMutex(cacheMutex);
done:
    CloseHandle(cacheMutex);
}

#else /* USE_FONT_CACHE */

static void FontListCache_Start()
{
}

static void FontListCache_End()
{
}

#endif /* USE_FONT_CACHE */
/* -------------- END CACHE STUFF -------------- */

static Family *FindFamilyW(WCHAR *familyName, BOOL fuzzyMatch)
{
    Family *cur = FontList;

    while (cur)
    {
        WCHAR *p = cur->FamilyName;
        do
        {
            if (strcmpiW(p, familyName) == 0)
                return cur;
            p++;
        } while (fuzzyMatch && *p);
        cur = cur->next;
    }
    return NULL;
}

/* cause I'm lazy */
static Family *FindFamilyA(const CHAR *familyName, BOOL fuzzyMatch)
{
    WCHAR *familyNameW;
    DWORD len;
    Family *ret;

    len = MultiByteToWideChar(CP_ACP, 0, familyName, -1, NULL, 0);
    familyNameW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, familyName, -1, familyNameW, len);

    ret= FindFamilyW(familyNameW, fuzzyMatch);

    HeapFree(GetProcessHeap(), 0, familyNameW);

    return ret;
}

static Family *FindFamilyOrAlias(WCHAR *familyName)
{
    Family *ret;
    ftFontAlias *alias;

    if ((ret = FindFamilyW(familyName, FALSE)))
        return ret;

    alias = ftAliasTable;
    while (alias)
    {
        if (strcmpiW(familyName, alias->faTypeFace) == 0)
            return alias->faRealFamily;
        alias = alias->next;
    }
    return NULL;
}

static void FTAlias_CreateAlias(WCHAR *typeFace, Family *faRealFamily)
{
    ftFontAlias *alias;

    /* first check that the alias isn't already there */
    alias = ftAliasTable;
    while (alias)
    {
        if (strcmpiW(typeFace, alias->faTypeFace) == 0)
            return;
        alias = alias->next;
    }

    alias = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                      sizeof(ftFontAlias));
    if (!alias) return;

    alias->faTypeFace = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,
                                  (strlenW(typeFace)+1) * sizeof(WCHAR));

    if (!alias->faTypeFace)
    {
        HeapFree(GetProcessHeap(), 0, alias);
        return;
    }
    strcpyW(alias->faTypeFace, typeFace);
    alias->faRealFamily = faRealFamily;

    alias->next = ftAliasTable;
    ftAliasTable = alias;
}

#ifdef HAVE_FONTCONFIG
/* searches font config for one of their familys
 * and returns the family name of the found font family.
 *
 * It may sound like an oximoron, but we do this in oreder to exploit
 * font configs built in (well, config filed) aliases (in /etc/font/fonts.conf).
 *
 * For example, it has aliases for serif, sans-serif and monospace.
 */
static BOOL FTFC_FindFamily(const char *searchFamily, char *foundFamily, int len)
{
    FcPattern *fcPattern;
    FcValue val;
    FcObjectSet *fcObjectSet;
    FcConfig *fcConfigDefault;
    FcFontSet *fcFontSetResult = NULL;

    BOOL ret = FALSE;

    if (!fontconfig_so_handle) return FALSE;

    TRACE("using FontConfig to find a family match for '%s'\n", searchFamily);

    fcConfigDefault = pFcConfigGetCurrent();

    /* create a match pattern */
    fcPattern = pFcPatternCreate();

    /* find the family we want */
    val.type = FcTypeString;
    val.u.s = searchFamily;
    pFcPatternAdd(fcPattern, FC_FAMILY, val, TRUE);

#if 0
    /* requires sorting, see below */
    /* prefer scalable fonts */
    val.type = FcTypeBool;
    val.u.b = FcTrue;
    pFcPatternAddWeak(fcPattern, FC_SCALABLE, val, TRUE);
#endif

    /* we want it to return the family name */
    fcObjectSet = pFcObjectSetCreate();
    pFcObjectSetAdd(fcObjectSet, FC_FAMILY);
    pFcObjectSetAdd(fcObjectSet, FC_SCALABLE);
    pFcObjectSetAdd(fcObjectSet, FC_FOUNDRY);

    /* fix up the patterns (Required for getting the aliases.. silly font config */
    pFcConfigSubstitute(fcConfigDefault, fcPattern, FcMatchPattern);
    /* pFcDefaultSubstitute(fcPattern); - Supposed to do this, however it breaks the
     *                                    alias matching? */

    /* match fonts */
    fcFontSetResult = pFcFontList(fcConfigDefault, fcPattern, fcObjectSet);
    /*FcResult res;
      fcFontSetResult = pFcFontSort(fcConfigDefault, fcPattern, FALSE, NULL, &res);
       Should be able to use this, but it isn't working. */

    /* pull off the first one (Really we don't need the loop.. but its safer */
    if (fcFontSetResult)
    {
        int i;
        BOOL found = FALSE;
        for (i = 0; i < fcFontSetResult->nfont; i++)
        {
            int j = 0;
            while (pFcPatternGet(fcFontSetResult->fonts[i], FC_FAMILY, j, &val) == FcResultMatch)
            {
                if (val.type == FcTypeString && val.u.s)
                {
                    FcValue tmpval;
                    BOOL skip = FALSE;

                    pFcPatternGet(fcFontSetResult->fonts[i], FC_SCALABLE, j, &tmpval);
                    TRACE("found '%s': %s\n", val.u.s, tmpval.u.b ? "Scalable" : "Bitmap");
                    /* skip over crap fonts (personal preferences, these ones kinda suck) */
                    if (fcFontSetResult->nfont > i+1 &&
                            (strcmp(val.u.s, "Luxi Sans") == 0 ||
                             strcmp(val.u.s, "Verdana") == 0 ||
                             strcmp(val.u.s, "Raghindi") == 0
                            ))
                        skip = TRUE;
                    /* hack to skip over non true type fonts, if possible */
                    if (fcFontSetResult->nfont > i+1 &&
                            tmpval.u.b == FcFalse)
                        skip = TRUE;

                    if (!found && !skip)
                    {
                        TRACE("using this one!\n");
                        strncpy(foundFamily, val.u.s, LF_FACESIZE);
                        foundFamily[LF_FACESIZE-1] = '\0';
                        ret = TRUE;
                        found = TRUE;

                        /* if we are debugging, i'd like to print out the rest */
                        if (!TRACE_ON(font))
                            goto fin;
                    }
                }
                j++;
            }
        }
    }

fin:
    if (fcFontSetResult)
        pFcFontSetDestroy(fcFontSetResult);
    pFcPatternDestroy(fcPattern);
    pFcObjectSetDestroy(fcObjectSet);
    return ret;
}
#endif

/* finds the best alias' for windows built in fonts:
 *   Tms Roman =
 *   Times New Roman -  Serif
 *   MS Serif        -  Serif
 *   Helv            -  Sans Serif
 *   MS Sans Serif   -  Sans Serif
 *   System          -  Sans Serif
 *   Arial           -  Sans Serif
 *
 *   Courier New     -  fixed
 */

/* FIXME: use registry aliasing */
static void FTAlias_FindBestBuiltins()
{
    BOOL needSerif = FALSE;
    BOOL needTimesNewRoman = FALSE;
    BOOL needMSSerif = FALSE;

    BOOL needSansSerif = FALSE;
    BOOL needHelv = FALSE;
    BOOL needArial = FALSE;
    BOOL needMSSansSerif = FALSE;
    BOOL needSystem = FALSE;
    BOOL needTahoma = FALSE;

    BOOL needMonospace = FALSE;
    BOOL needCourierNew = FALSE;
    BOOL needFixedSys = FALSE;


/* Serif Fonts */

    /* Times New Roman = Tms Roman */
    if (!FindFamilyOrAlias(TimesNewRoman))
    {
        needSerif = TRUE;
        needTimesNewRoman = TRUE;
    }

    /* MS Serif */
    if (!FindFamilyOrAlias(MSSerif))
    {
        needSerif = TRUE;
        needMSSerif = TRUE;
    }

    if (needSerif)
    {
        Family *serifFamily = NULL;
#ifdef HAVE_FONTCONFIG
        char foundFamily[LF_FACESIZE] = {0};
        if (FTFC_FindFamily("serif", foundFamily, sizeof(foundFamily)))
            serifFamily = FindFamilyA(foundFamily, FALSE);
        if (!serifFamily)
#endif
        { /* ifdef magic. well happen always without FontConfig, will happen
             with font config if it couldn't find a family */
            const char *possibleFamilys[] = {
                     "Bitstream Vera Serif",
                     "Times",
                     "Times New Roman",
                     "Luxi Serif",
                     NULL
                };
            const char **iter = &possibleFamilys[0];
            while (*iter && !serifFamily)
            {
                serifFamily = FindFamilyA(*iter, FALSE);
                iter++;
            }
        }

        if (serifFamily)
        {
            if (needTimesNewRoman)
                FTAlias_CreateAlias(TimesNewRoman, serifFamily);
            if (needMSSerif)
                FTAlias_CreateAlias(MSSerif, serifFamily);
        }
        else
            ERR("Could not create alias for serif fonts\n");
    }

/* Sans Serif Fonts */

    /* Helv - this one first, any sane system should have this. (as Helvetica) */
    if (!FindFamilyOrAlias(Helv))
    {
        needSansSerif = TRUE;
        needHelv = TRUE;
    }

    /* Arial */
    if (!FindFamilyOrAlias(Arial))
    {
        needSansSerif = TRUE;
        needArial = TRUE;
    }

    /* MS Sans Serif */
    if (!FindFamilyOrAlias(MSSansSerif))
    {
        needSansSerif = TRUE;
        needMSSansSerif = TRUE;
    }

    /* System */
    if (!FindFamilyOrAlias(System))
    {
        needSansSerif = TRUE;
        needSystem = TRUE;
    }

    /* Tahoma */
    if (!FindFamilyOrAlias(Tahoma))
    {
        needSansSerif = TRUE;
        needTahoma = TRUE;
    }

    if (needSansSerif)
    {
        Family *sansSerifFamily = NULL;
        char foundFamily[LF_FACESIZE] = {0};
#ifdef HAVE_FONTCONFIG
        if (FTFC_FindFamily("sans-serif", foundFamily, sizeof(foundFamily)))
            sansSerifFamily = FindFamilyA(foundFamily, FALSE);
        if (!sansSerifFamily)
#endif
        {
            const char *possibleFamilys[] = {
                    "Arial",
                    "Bitstream Vera Sans",
                    "Helvetica",
                    "Nimbus Sans L",
                    NULL
                };
            const char **iter = possibleFamilys;
            while (*iter && !sansSerifFamily)
            {
                sansSerifFamily = FindFamilyA(*iter, FALSE);
                iter++;
            }
        }

        if (sansSerifFamily)
        {
            if (needHelv)
                FTAlias_CreateAlias(Helv, sansSerifFamily);
            if (needArial)
                FTAlias_CreateAlias(Arial, sansSerifFamily);
            if (needMSSansSerif)
                FTAlias_CreateAlias(MSSansSerif, sansSerifFamily);
            if (needSystem)
                FTAlias_CreateAlias(System, sansSerifFamily);
            if (needTahoma)
                FTAlias_CreateAlias(Tahoma, sansSerifFamily);
        }
    }

/* Monospace fonts */
    /* Courier new */
    if (!FindFamilyOrAlias(CourierNew))
    {
        needMonospace = TRUE;
        needCourierNew = TRUE;
    }

    if (!FindFamilyOrAlias(FixedSys))
    {
        needMonospace = TRUE;
        needFixedSys = TRUE;
    }

    if (needMonospace)
    {
        Family *monospaceFamily = NULL;
        char foundFamily[LF_FACESIZE] = {0};
#ifdef HAVE_FONTCONFIG
        if (FTFC_FindFamily("monospace", foundFamily, sizeof(foundFamily)))
            monospaceFamily = FindFamilyA(foundFamily, FALSE);
        if (!monospaceFamily)
#endif
        {
            const char *possibleFamilys[] = {
                    "Courier",
                    "Courier New",
                    "Bitstream Vera Sans Mono",
                    "Andale Mono",
                    NULL
                };
            const char **iter = possibleFamilys;
            while (*iter && !monospaceFamily)
            {
                monospaceFamily = FindFamilyA(*iter, FALSE);
                iter++;
            }
        }

        if (monospaceFamily)
        {
            if (needCourierNew)
                FTAlias_CreateAlias(CourierNew, monospaceFamily);
            if (needFixedSys)
                FTAlias_CreateAlias(FixedSys, monospaceFamily);
        }
    }

}

static void FTAlias_FindFromConfig()
{
    HKEY hkey;
    DWORD valuelen, datalen;
    DWORD vlen, dlen;
    WCHAR *value;
    BYTE *data;
    DWORD type;
    int i = 0;

    if (RegOpenKeyA(HKEY_LOCAL_MACHINE,
                    "Software\\Wine\\Wine\\Config\\FontAlias",
                    &hkey) != ERROR_SUCCESS)
    {
        TRACE("no font alias section\n");
        return;
    }

    RegQueryInfoKeyW(hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                     &valuelen, &datalen, NULL, NULL);

    valuelen++;
    datalen++;

    value = HeapAlloc(GetProcessHeap(), 0, valuelen * sizeof(WCHAR));
    data = HeapAlloc(GetProcessHeap(), 0, datalen * sizeof(WCHAR));

    dlen = datalen;
    vlen = valuelen;

    while (RegEnumValueW(hkey, i++, value, &vlen, NULL, &type, data, &dlen)
            == ERROR_SUCCESS)
    {
        WCHAR *aliasfamily = (WCHAR*)data;
        Family *targetAlias = NULL;

        TRACE("(%s)->(%s)\n", debugstr_w(value), debugstr_w(aliasfamily));
        if (FindFamilyW(value, FALSE))
        {
            TRACE("skipping config alias for family %s, as family exists already\n",
                   debugstr_w(value));
        }
        else if (!(targetAlias = FindFamilyOrAlias(aliasfamily)))
        {
            WARN("skipping config alias for family %s to %s as target doesn't exist\n",
                    debugstr_w(value), debugstr_w(aliasfamily));
        }
        else
        {
            TRACE("adding config alias for family %s to %s\n",
                    debugstr_w(value), debugstr_w(aliasfamily));
            FTAlias_CreateAlias(value, targetAlias);
        }

        dlen = datalen;
        vlen = valuelen;
    }
    HeapFree(GetProcessHeap(), 0, data);
    HeapFree(GetProcessHeap(), 0, value);
    RegCloseKey(hkey);
}

static void FTAlias_Init()
{
    FTAlias_FindFromConfig();
    FTAlias_FindBestBuiltins();

    if (TRACE_ON(font))
    {
        ftFontAlias *alias = ftAliasTable;

        while (alias)
        {
            TRACE("Alias %s to (%p)%s\n", debugstr_w(alias->faTypeFace),
                  alias->faRealFamily->FamilyName,
                  debugstr_w(alias->faRealFamily->FamilyName));
            alias = alias->next;
        }
    }
}

#ifdef USE_FONT_CACHE
static BOOL AddFaceFromCache(Family *cache_family, Face *cache_face)
{
    Family *family = FontList;
    Family **insert = &FontList;

    Face **insertface;

    /* much of this code is semi-duplicate from
     * AddFontFileToList, can't avoid it.
     * Its different enough to need rewriting/duplicating,
     * but similar in what it does, i suppose.
     */

    while (family)
    {
        if (!strcmpW(family->FamilyName, cache_family->FamilyName))
            break;
        insert = &family->next;
        family = family->next;
    }
    if (!family)
    {
        family = *insert = HeapAlloc(GetProcessHeap(), 0, sizeof(*family));
        family->FamilyName = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       (strlenW(cache_family->FamilyName) + 1) * sizeof(WCHAR));
        strcpyW(family->FamilyName, cache_family->FamilyName);
        family->ScalableFamily = cache_family->ScalableFamily;
        family->FirstFace = NULL;
        family->next = NULL;
    }
    else
    { /* check they are the same */
        if (family->ScalableFamily != cache_family->ScalableFamily)
        {
            SetRebuildCache();
            TRACE("family differs, not using\n");
            return FALSE;
        }
    }

    /* check the face doesn't already exist */
    for (insertface = &family->FirstFace; *insertface;
         insertface = &(*insertface)->next)
    {
        if (!strcmpW((*insertface)->StyleName, cache_face->StyleName) &&
            (family->ScalableFamily ||
             (*insertface)->Height == cache_face->Height))
        {
            SetRebuildCache();
            TRACE("face already exists, not adding cached face (%s)\n",
                  debugstr_w(cache_face->StyleName));
            return FALSE;
        }
    }
    *insertface = HeapAlloc(GetProcessHeap(), 0, sizeof(**insertface));
    (*insertface)->StyleName = HeapAlloc(GetProcessHeap(), 0,
                                  (strlenW(cache_face->StyleName) + 1) * sizeof(WCHAR));
    strcpyW((*insertface)->StyleName, cache_face->StyleName);
    (*insertface)->file = HeapAlloc(GetProcessHeap(), 0, strlen(cache_face->file)+1);
    strcpy((*insertface)->file, cache_face->file);
    (*insertface)->next = NULL;
    (*insertface)->Italic = cache_face->Italic;
    (*insertface)->Bold = cache_face->Bold;
    (*insertface)->Height = cache_face->Height;
    (*insertface)->SizeIndex = cache_face->SizeIndex;

    (*insertface)->fsCsb[0] = cache_face->fsCsb[0];
    (*insertface)->fsCsb[1] = cache_face->fsCsb[1];
    (*insertface)->lastModified = cache_face->lastModified;

    return TRUE;
}
#endif /* USE_FONT_CACHE */

static void DeleteFamilyFaces(Family *family)
{
    Face *cur = family->FirstFace;
    while (cur)
    {
        Face *prev = cur;
        cur = cur->next;
        HeapFree(GetProcessHeap(), 0, prev->StyleName);
        HeapFree(GetProcessHeap(), 0, prev->file);
        HeapFree(GetProcessHeap(), 0, prev);
    }
    family->FirstFace = NULL;
}

static BOOL AddFontFileToList(const char *file)
{
    FT_Face ft_face;
    WCHAR *FamilyW, *StyleW;
    DWORD len;
    Family *family = FontList;
    Family **insert = &FontList;
    Face **insertface;
    FT_Error err;
    int i, size;
    BOOL scalable;
    struct stat stat_buf;
    int faces_from_file = 0;
    time_t fileLastModified;

#ifdef USE_FONT_CACHE
    int faces_from_cache = 0;
    Family *cur_cache_family;
    Family *prev_cache_family = NULL;

    IgnoredFiles *cur_ignore_cache;
    IgnoredFiles *prev_ignore_cache = NULL;
#endif

    stat(file, &stat_buf);
    fileLastModified = stat_buf.st_mtime;

#ifdef USE_FONT_CACHE
    /* check if we should ignore the file */
    cur_ignore_cache = ignoredFilesList;
    while (cur_ignore_cache && using_cache && !ignoringCache)
    {
        IgnoredFiles *next = cur_ignore_cache->next;

        if (strcmp(file, cur_ignore_cache->filename) == 0)
        {
            if (cur_ignore_cache->lastModified == fileLastModified)
            {
                return TRUE;
            }
            else /* remove file from ignore list */
            {
                HeapFree(GetProcessHeap(), 0, cur_ignore_cache->filename);
                HeapFree(GetProcessHeap(), 0, cur_ignore_cache);
                if (prev_ignore_cache) prev_ignore_cache->next = next;
                else ignoredFilesList = next;
                RebuildIgnoredCache();
            }
        }

        cur_ignore_cache = next;
    }

    cur_cache_family = cachedFontList;
    while (cur_cache_family && using_cache && !ignoringCache)
    {
        Face *cur_cache_face = cur_cache_family->FirstFace;
        Face *prev_cache_face = NULL;
        Family *next_family = cur_cache_family->next;
        while (cur_cache_face && using_cache)
        {
            Face *next_face = cur_cache_face->next;
            if (fileLastModified == cur_cache_face->lastModified &&
                strcmp(cur_cache_face->file, file) == 0)
                /* although it makes sense to check the filename first, its slower */
                /* if the file we are trying is cached with the same date */
            {
                using_cache = AddFaceFromCache(cur_cache_family, cur_cache_face);
                if (using_cache) faces_from_cache++;
                else TRACE("cache broke somewhere, will stop using it and rebuild\n");
                /* delete the face from the cache, since its used
                 * and we want to speed up other searches. */
                HeapFree(GetProcessHeap(), 0, cur_cache_face->StyleName);
                HeapFree(GetProcessHeap(), 0, cur_cache_face->file);
                if (prev_cache_face) prev_cache_face->next = cur_cache_face->next;
                else cur_cache_family->FirstFace = cur_cache_face->next;
                HeapFree(GetProcessHeap(), 0, cur_cache_face);

                if (!cur_cache_family->FirstFace) /* all faces deleted, delete the family */
                {
                    HeapFree(GetProcessHeap(), 0, cur_cache_family->FamilyName);
                    if (prev_cache_family) prev_cache_family->next = cur_cache_family->next;
                    else cachedFontList = cur_cache_family->next;
                    HeapFree(GetProcessHeap(), 0, cur_cache_family);
                    cur_cache_family = NULL;
                }
                cur_cache_face = NULL;
            }
            if (cur_cache_face) prev_cache_face = cur_cache_face;
            cur_cache_face = next_face;
        }
        if (cur_cache_family) prev_cache_family = cur_cache_family;
        cur_cache_family = next_family;
    }
    if (faces_from_cache)
    {
        TRACE("loaded %i faces from cache for file %s.\n",
              faces_from_cache, debugstr_a(file));
        return TRUE;
    }
#endif /* USE_FONT_CACHE */

    TRACE("Loading font file %s\n", debugstr_a(file));

    if((err = pFT_New_Face(library, file, 0, &ft_face)) != 0) {
        /* Unable to load the file. Probably an invalid or old font file.
         * This is not an error condition.
         */
        WARN("Unable to load font file %s err = %x\n", debugstr_a(file), err);
        return FALSE;
    }

    scalable = FT_IS_SCALABLE(ft_face);

    len = MultiByteToWideChar(CP_ACP, 0, ft_face->family_name, -1, NULL, 0);
    FamilyW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, ft_face->family_name, -1, FamilyW, len);

    TRACE("familyW: %s\n", debugstr_w(FamilyW));

    while(family) {
        if(!strcmpW(family->FamilyName, FamilyW))
            break;
        insert = &family->next;
        family = family->next;
    }
    if(!family) {
        family = *insert = HeapAlloc(GetProcessHeap(), 0, sizeof(*family));
        family->FamilyName = FamilyW;
        family->ScalableFamily = scalable;
        family->FirstFace = NULL;
        family->next = NULL;
    } else {
        HeapFree(GetProcessHeap(), 0, FamilyW);
        if (scalable && !family->ScalableFamily) /* overwrite with scalable */
        {
            DeleteFamilyFaces(family);
        }
        else if (!scalable && family->ScalableFamily)
        { /* ignore non-scalable if scalable exists */
            pFT_Done_Face(ft_face);
            return FALSE;
        }
    }

    size = -1;
    if (!scalable && ft_face->num_fixed_sizes)
    {
        TRACE("family isn't scalable\n");
        size = 0;
    }
    else if (!scalable)
        WARN("non scalable font has no fixed sizes?\n");

add_face:
    len = MultiByteToWideChar(CP_ACP, 0, ft_face->style_name, -1, NULL, 0);
    StyleW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, ft_face->style_name, -1, StyleW, len);

    for(insertface = &family->FirstFace; *insertface;
        insertface = &(*insertface)->next) {
        if(!strcmpW((*insertface)->StyleName, StyleW) &&
                (scalable ||
                  (*insertface)->Height == ft_face->available_sizes[size].height))
        {
            WARN("Already loaded font %s %s (%i)\n",
                 debugstr_w(family->FamilyName),
                 debugstr_w(StyleW),
                 (*insertface)->Height);
            HeapFree(GetProcessHeap(), 0, StyleW);
            StyleW = NULL;
            goto done_face;
        }
    }
    *insertface = HeapAlloc(GetProcessHeap(), 0, sizeof(**insertface));
    (*insertface)->StyleName = StyleW;
    (*insertface)->file = HeapAlloc(GetProcessHeap(),0,strlen(file)+1);
    strcpy((*insertface)->file, file);
    (*insertface)->next = NULL;
    (*insertface)->Italic = (ft_face->style_flags & FT_STYLE_FLAG_ITALIC) ? 1 : 0;
    (*insertface)->Bold = (ft_face->style_flags & FT_STYLE_FLAG_BOLD) ? 1 : 0;
    (*insertface)->Height = size == -1 ? -1 : ft_face->available_sizes[size].height;
    (*insertface)->SizeIndex = size;

    (*insertface)->fsCsb[0] = (*insertface)->fsCsb[1] = 0;

    faces_from_file++;
    TRACE("Added font to family: %s, style: %s, height: %i (-1 means scalable)\n",
            debugstr_w(family->FamilyName),
            debugstr_w((*insertface)->StyleName),
            (*insertface)->Height);

    if (FT_IS_SFNT(ft_face)) /* true type font */
    {
        TT_OS2 *pOS2;
        pOS2 = pFT_Get_Sfnt_Table(ft_face, ft_sfnt_os2);
        if(pOS2)
        {
            (*insertface)->fsCsb[0] = pOS2->ulCodePageRange1;
            (*insertface)->fsCsb[1] = pOS2->ulCodePageRange2;
        }
    }
    else
    {
        WARN("do charmaps for this font type\n");
    }

    if((*insertface)->fsCsb[0] == 0) { /* let's see if we can find any interesting cmaps */
        for(i = 0; i < ft_face->num_charmaps &&
              !(*insertface)->fsCsb[0]; i++) {
            switch(ft_face->charmaps[i]->encoding) {
            case ft_encoding_unicode:
                (*insertface)->fsCsb[0] = 1;
                break;
            case ft_encoding_symbol:
                (*insertface)->fsCsb[0] = 1L << 31;
                break;
            default:
                break;
            }
        }
    }

    (*insertface)->lastModified = fileLastModified;
done_face:
    if (!scalable)
    {
        size++;
        if (size < ft_face->num_fixed_sizes)
            goto add_face;
    }

    pFT_Done_Face(ft_face);

    if (faces_from_file == 0) /* add file to ignore list */
    {
#ifdef USE_FONT_CACHE
        IgnoredFiles *newIgnore = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                            sizeof(IgnoredFiles));
        newIgnore->filename = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                        strlen(file) + 1);
        strcpy(newIgnore->filename, file);
        newIgnore->lastModified = fileLastModified;
        newIgnore->next = ignoredFilesList;
        ignoredFilesList = newIgnore;
        RebuildIgnoredCache();
#endif /* USE_FONT_CACHE */
        TRACE("ignoring file (%s) with family (%s)\n",
              debugstr_a(file), debugstr_w(family->FamilyName));
    }
    else
        TRACE("Added font %s %s (%i faces)\n", debugstr_w(family->FamilyName),
              debugstr_w(StyleW), faces_from_file);
#ifdef USE_FONT_CACHE
    if (faces_from_file)
        SetRebuildCache();
#endif
    return TRUE;
}

/* dumps the current font family list,
 * also returns the number of font families
 */
static int DumpFontList(void)
{
    Family *family;
    Face *face;

    int familycount = 0;

    for(family = FontList; family; family = family->next) {
        TRACE("Family: %s\n", debugstr_w(family->FamilyName));
        for(face = family->FirstFace; face; face = face->next) {
            TRACE("\t%s\n", debugstr_w(face->StyleName));
        }
        familycount++;
    }
    return familycount;
}

static BOOL ReadFontDir(char *dirname)
{
    DIR *dir;
    struct dirent *dent;
    char path[MAX_PATH];

    TRACE("Loading fonts from %s\n", debugstr_a(dirname));

    dir = opendir(dirname);
    if(!dir) {
        ERR("Can't open directory %s\n", debugstr_a(dirname));
        return FALSE;
    }
    while((dent = readdir(dir)) != NULL) {
        struct stat statbuf;

        if(!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
            continue;

        TRACE("Found %s in %s\n", debugstr_a(dent->d_name), debugstr_a(dirname));

        sprintf(path, "%s/%s", dirname, dent->d_name);

        if(stat(path, &statbuf) == -1)
        {
            WARN("Can't stat %s\n", debugstr_a(path));
            continue;
        }
        if(S_ISDIR(statbuf.st_mode))
            ReadFontDir(path);
        else
            AddFontFileToList(path);
    }

    closedir(dir);

    return TRUE;
}

FT_Fixed FixedFromFloat(float f)
{
    int integer = f;
    unsigned int fraction = f - integer;
    return ((integer << 16) | (fraction * 65535));
}


#ifdef HAVE_FONTCONFIG
/*************************************************************
 *    Fontconfig_Init
 *
 * Initialize the Fontconfig library and adds the fonts
 * to the font list.
 */
static BOOL Fontconfig_Init(void)
{
    FcConfig *fcConfigDefault;
    FcFontSet *fcFontSetDefault;
    FcObjectSet *fcObjectSet;
    FcPattern *fcPattern;

    TRACE("\n");

    fontconfig_so_handle = wine_dlopen(SONAME_LIBFONTCONFIG, RTLD_NOW, NULL, 0);
    if (!fontconfig_so_handle)
    {
        goto nofontconfig;
    }

#define DYNLOAD_FONTCONFIG(f) \
    do { p##f = wine_dlsym(fontconfig_so_handle, #f, NULL, 0); \
    if (!p##f) \
    { \
        ERR("failed to load fontconfig symbol '%s'\n", #f); \
        goto fontconfig_initerror; \
    }} while (0) 

    DYNLOAD_FONTCONFIG(FcInit);
    DYNLOAD_FONTCONFIG(FcConfigGetCurrent);
    DYNLOAD_FONTCONFIG(FcConfigSubstitute);
    DYNLOAD_FONTCONFIG(FcDefaultSubstitute);
    DYNLOAD_FONTCONFIG(FcFontList);
    DYNLOAD_FONTCONFIG(FcFontSort);
    DYNLOAD_FONTCONFIG(FcFontSetDestroy);
    DYNLOAD_FONTCONFIG(FcPatternCreate);
    DYNLOAD_FONTCONFIG(FcPatternGet);
    DYNLOAD_FONTCONFIG(FcPatternAdd);
    DYNLOAD_FONTCONFIG(FcPatternAddWeak);
    DYNLOAD_FONTCONFIG(FcPatternDestroy);
    DYNLOAD_FONTCONFIG(FcObjectSetCreate);
    DYNLOAD_FONTCONFIG(FcObjectSetAdd);
    DYNLOAD_FONTCONFIG(FcObjectSetDestroy);

#undef DYNLOAD_FONTCONFIG

    if (!pFcInit()) goto fontconfig_invalidconfig;

    fcConfigDefault = pFcConfigGetCurrent();
    if (!fcConfigDefault) goto fontconfig_invalidconfig;

    fcObjectSet = pFcObjectSetCreate();
    pFcObjectSetAdd(fcObjectSet, FC_FILE);
    fcPattern = pFcPatternCreate();

    fcFontSetDefault = pFcFontList(fcConfigDefault, fcPattern, fcObjectSet);

    if (fcFontSetDefault)
    {
        int i;
        for (i = 0; i < fcFontSetDefault->nfont; i++)
        {
            int j = 0;
            FcValue value;
            while (pFcPatternGet(fcFontSetDefault->fonts[i], FC_FILE, j, &value) == FcResultMatch)
            {
                if (value.type == FcTypeString && value.u.s)
                {
                    TRACE("[%i:%i] adding font from fontconfig: %s\n",
                           i, j, debugstr_a(value.u.s));
                    AddFontFileToList(value.u.s);
                }
                j++;
                /* I don't think any of the fonts that we support should have
                 * multiple files, but just for kicks we'll check them all
                 */
            }
        }

        pFcFontSetDestroy(fcFontSetDefault);
    }
    else WARN("no fonts found in fontconfig\n");

    TRACE("done with font config fonts\n");

    pFcPatternDestroy(fcPattern);
    pFcObjectSetDestroy(fcObjectSet);

    return TRUE;

fontconfig_invalidconfig:
    MESSAGE("Fontconfig is installed but not correctly configured. Please consult\n"
            "\tthe fontconfig manuals for help configuring font config correctly\n");
fontconfig_initerror:
    wine_dlclose(fontconfig_so_handle, NULL, 0);
nofontconfig:
    MESSAGE("Fontconfig could not be loaded.\n"
            "\tThis is not necessarily a fatal error, however the font selection\n"
            "\tavailable to applications may be limited without it.\n"
            "\tFor more details please consult the Cedega Font FAQ.\n");
    return FALSE;
}

static void Fontconfig_Finalize(void)
{
    if (fontconfig_so_handle)
        wine_dlclose(fontconfig_so_handle, NULL, 0);
}

#else
static BOOL Fontconfig_Init(void)
{
 /* Not generally a problem on the Mac, since we can access the usual system fonts */        
#ifndef __APPLE__
    ERR("Fontconfig support was not compiled in at compile time.\n");
#endif
    return FALSE;
}

static void Fontconfig_Finalize(void)
{
}
#endif /* HAVE_FONTCONFIG */



/*************************************************************
 *    WineEngInit
 *
 * Initialize FreeType library and create a list of available faces
 * returns TRUE or FALSE on failure.
 */
BOOL WineEngInit(void)
{
    HKEY hkey;
    DWORD valuelen, datalen, i = 0, type, dlen, vlen;
    LPSTR value;
    LPVOID data;
    char windowsdir[MAX_PATH];
    char unixname[MAX_PATH];
    char *dllpath;

    if (freetype_so_handle)
    {
        TRACE("Already initialized\n");
        return TRUE;
    }
    else
    {
        TRACE("Trying to load FreeType\n");
    }

#ifdef __APPLE__
    dllpath = getenv("WINEDLLPATH");
    if (dllpath)
    {
        snprintf(unixname, MAX_PATH, "%s/%s", dllpath, SONAME_LIBFREETYPE);
        freetype_so_handle = wine_dlopen(unixname, RTLD_NOW, NULL, 0);
    }
    /* Might be a non-packaged build - try the /Library/Frameworks dir too */
    if (!freetype_so_handle)
    {
        snprintf(unixname, MAX_PATH, "/Library/Frameworks/%s", SONAME_LIBFREETYPE);
        freetype_so_handle = wine_dlopen(unixname, RTLD_NOW, NULL, 0);            
    }
#else
    freetype_so_handle = wine_dlopen(SONAME_LIBFREETYPE, RTLD_NOW, NULL, 0);
#endif
    if (!freetype_so_handle)
    {
        goto nofreetype;
    }

#define DYNLOAD_FREETYPE_SAFE(f) \
    do { p##f = wine_dlsym(freetype_so_handle, #f, NULL, 0); \
    if (!p##f) \
    { \
        WARN("failed to load freetype symbol '%s' but continuing anyways.\n", #f); \
    }} while (0) 

#define DYNLOAD_FREETYPE(f) \
    do { p##f = wine_dlsym(freetype_so_handle, #f, NULL, 0); \
    if (!p##f) \
    { \
        ERR("failed to load freetype symbol '%s'.\n", #f); \
        goto freetype_initerror; \
    }} while (0) 

    DYNLOAD_FREETYPE(FT_Init_FreeType);
    DYNLOAD_FREETYPE(FT_New_Face);
    DYNLOAD_FREETYPE(FT_Done_Face);
    DYNLOAD_FREETYPE(FT_Get_Sfnt_Table);
    DYNLOAD_FREETYPE(FT_Set_Pixel_Sizes);
    DYNLOAD_FREETYPE(FT_Load_Glyph);
    DYNLOAD_FREETYPE(FT_Vector_Rotate);
    DYNLOAD_FREETYPE(FT_Outline_Transform);
    DYNLOAD_FREETYPE(FT_Outline_Translate);
    DYNLOAD_FREETYPE(FT_Outline_Get_Bitmap);
    DYNLOAD_FREETYPE(FT_Cos);
    DYNLOAD_FREETYPE(FT_Sin);
#ifndef FT_MULFIX_INLINED
    DYNLOAD_FREETYPE(FT_MulFix);
#endif
    DYNLOAD_FREETYPE(FT_Get_Char_Index);
    DYNLOAD_FREETYPE(FT_Select_Charmap);
    DYNLOAD_FREETYPE(FT_Get_First_Char);
    DYNLOAD_FREETYPE(FT_Get_Next_Char);

    /* Special case FT_Load_Sfnt_Table to support older versions of freetype and btX2 as well.
     * If this entry point is not available we fall back to relying on freetype internals */
    DYNLOAD_FREETYPE_SAFE(FT_Load_Sfnt_Table);

#undef DYNLOAD_FREETYPE
#undef DYNLOAD_FREETYPE_SAFE


    if(pFT_Init_FreeType(&library) != 0) {
        ERR("Can't init FreeType library\n");
        goto freetype_initerror;
    }

    FontListCache_Start();

    /* first load the fonts in Fontconfig */
    Fontconfig_Init();

    /* load in the fonts from %WINDOWSDIR%\\Fonts first of all */
    GetWindowsDirectoryA(windowsdir, sizeof(windowsdir));
    strcat(windowsdir, "\\Fonts");
    wine_get_unix_file_name(windowsdir, unixname, sizeof(unixname));
    ReadFontDir(unixname);

#ifdef __APPLE__
    /* Bring in the system fonts on the Mac */
    ReadFontDir("/System/Library/Fonts");
#endif

    /* then look in any directories that we've specified in the config file */
    if(RegOpenKeyA(HKEY_LOCAL_MACHINE,
                   "Software\\Wine\\Wine\\Config\\FontDirs",
                   &hkey) == ERROR_SUCCESS) {

        RegQueryInfoKeyA(hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                         &valuelen, &datalen, NULL, NULL);

        valuelen++; /* returned value doesn't include room for '\0' */
        value = HeapAlloc(GetProcessHeap(), 0, valuelen);
        data = HeapAlloc(GetProcessHeap(), 0, datalen);

        dlen = datalen;
        vlen = valuelen;
        while(RegEnumValueA(hkey, i++, value, &vlen, NULL, &type, data,
                            &dlen) == ERROR_SUCCESS) {
            TRACE("Got %s=%s\n", value, (LPSTR)data);
            ReadFontDir((LPSTR)data);
            /* reset dlen and vlen */
            dlen = datalen;
            vlen = valuelen;
        }
        HeapFree(GetProcessHeap(), 0, data);
        HeapFree(GetProcessHeap(), 0, value);
        RegCloseKey(hkey);
    }


    /* finally load aliases */
    FTAlias_Init();

    /* and after we've done everything we might want to do with fontconfig
     * (find fonts, find alias') we can close it.
     */
    Fontconfig_Finalize();

    FontListCache_End();

    if (DumpFontList() < 4)
    {
        MESSAGE("Unable to find a sufficient number of fonts available to FreeType.\n"
                "\tFreeType will not be loaded as a result.\n");
        WineEngFinalize();
        goto nofreetype;
    }

    return TRUE;

freetype_initerror:
    wine_dlclose(freetype_so_handle, NULL, 0);
nofreetype:
    freetype_so_handle = NULL;
    
#ifdef __APPLE__
    MESSAGE("FreeType could not be loaded.\n");
#else
    MESSAGE("FreeType could not be loaded.\n"
            "\tThis is not necessarily a fatal error, however some\n"
            "\tapplications require FreeType to be installed and\n"
            "\tmay not function or display text correctly otherwise.\n"
            "\tPlease consult the Cedega Font FAQ for more details\n"
            "\tabout this problem\n");
#endif
    return FALSE;
}

/*************************************************************
 *    WineEngFinalize
 */
void WineEngFinalize(void)
{
    /* FIXME: free structs */
    if (freetype_so_handle)
        wine_dlclose(freetype_so_handle, NULL, 0);
}

static LONG calc_ppem_for_height(GdiFont font, LONG height)
{
    FT_Face ft_face = font->ft_face;
    LONG ppem;
    LONG divHeight = 0;
    TT_OS2 *pOS2 = NULL;

    if(height == 0) height = 16;

    /* Calc. height of EM square:
     *
     * For +ve lfHeight we have
     * lfHeight = (winAscent + winDescent) * ppem / units_per_em
     * Re-arranging gives:
     * ppem = units_per_em * lfheight / (winAscent + winDescent)
     *
     * For -ve lfHeight we have
     * |lfHeight| = ppem
     * [i.e. |lfHeight| = (winAscent + winDescent - il) * ppem / units_per_em
     * with il = winAscent + winDescent - units_per_em]
     *
     */

    if (height < 0) return -height;

    if (FT_IS_SFNT(ft_face))
        pOS2 = pFT_Get_Sfnt_Table(ft_face, ft_sfnt_os2);

    if (pOS2)
    {
        divHeight = (pOS2->usWinAscent + pOS2->usWinDescent);
    }
    else if (ft_face->height)
    {
        divHeight = ft_face->height;
    }
    else if (font->fixedSize)
    {
        divHeight = (ft_face->available_sizes[font->fixedSize].height);
    }
    /* some fixed size fonts (or freetype?) are broken. silly fonts. */
    if (!divHeight) divHeight = height;
    if (!divHeight) divHeight = 16;
    ppem = ft_face->units_per_EM * height / divHeight;

    return ppem;
}

static LONG load_VDMX(GdiFont, LONG);

static FT_Face OpenFontFile(GdiFont font, char *file, LONG height)
{
    FT_Error err;
    FT_Face ft_face;
    LONG ppem;

    TRACE("(%p, %s,%li)\n", font, file, height);

    err = pFT_New_Face(library, file, 0, &ft_face);
    if(err) {
        ERR("FT_New_Face rets %d\n", err);
        return 0;
    }

    /* set it here, as load_VDMX needs it */
    font->ft_face = ft_face;

    /* load the VDMX table if we have one */
    ppem = load_VDMX(font, height);
    if(ppem == 0)
        ppem = calc_ppem_for_height(font, height);

    pFT_Set_Pixel_Sizes(ft_face, 0, ppem);

    return ft_face;
}

static int get_nearest_charset(Face *face, int lfcharset)
{
    CHARSETINFO csi;
    TranslateCharsetInfo((DWORD*)lfcharset, &csi, TCI_SRCCHARSET);

    if(csi.fs.fsCsb[0] & face->fsCsb[0]) return lfcharset;

    if(face->fsCsb[0] & 0x1) return ANSI_CHARSET;

    if(face->fsCsb[0] & (1L << 31)) return SYMBOL_CHARSET;

    FIXME("returning DEFAULT_CHARSET face->fsCsb[0] = %08lx file = %s\n",
          face->fsCsb[0], face->file);
    return DEFAULT_CHARSET;
}

static GdiFont alloc_font(void)
{
    GdiFont ret = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ret));
    ret->gmsize = INIT_GM_SIZE;
    ret->gm = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                        ret->gmsize * sizeof(*ret->gm));
    ret->next = NULL;
    return ret;
}

static void free_font(GdiFont font)
{
    pFT_Done_Face(font->ft_face);
    if (font->cachedMetrics) HeapFree(GetProcessHeap(), 0, font->cachedMetrics);
    HeapFree(GetProcessHeap(), 0, font->gm);
    HeapFree(GetProcessHeap(), 0, font);
}


/*************************************************************
 * load_VDMX
 *
 * load the vdmx entry for the specified height
 */

#define MS_MAKE_TAG( _x1, _x2, _x3, _x4 ) \
          ( ( (FT_ULong)_x4 << 24 ) |     \
            ( (FT_ULong)_x3 << 16 ) |     \
            ( (FT_ULong)_x2 <<  8 ) |     \
              (FT_ULong)_x1         )

#define MS_VDMX_TAG MS_MAKE_TAG('V', 'D', 'M', 'X')

typedef struct {
    BYTE bCharSet;
    BYTE xRatio;
    BYTE yStartRatio;
    BYTE yEndRatio;
} Ratios;

static LONG load_VDMX(GdiFont font, LONG height)
{
    BYTE hdr[6], tmp[2], group[4];
    BYTE devXRatio, devYRatio;
    USHORT numRecs, numRatios;
    DWORD offset = -1;
    LONG ppem = 0;
    int i, result;

    result = WineEngGetFontData(font, MS_VDMX_TAG, 0, hdr, 6);

    if(result == GDI_ERROR) /* no vdmx table present, use linear scaling */
        return ppem;

    /* FIXME: need the real device aspect ratio */
    devXRatio = 1;
    devYRatio = 1;

    numRecs = GET_BE_WORD(&hdr[2]);
    numRatios = GET_BE_WORD(&hdr[4]);

    for(i = 0; i < numRatios; i++) {
        Ratios ratio;

        offset = (3 * 2) + (i * sizeof(Ratios));
        WineEngGetFontData(font, MS_VDMX_TAG, offset, &ratio, sizeof(Ratios));
        offset = -1;

        TRACE("Ratios[%d] %d  %d : %d -> %d\n", i, ratio.bCharSet, ratio.xRatio, ratio.yStartRatio, ratio.yEndRatio);

        if(ratio.bCharSet != 1)
            continue;

        if((ratio.xRatio == 0 &&
            ratio.yStartRatio == 0 &&
            ratio.yEndRatio == 0) ||
           (devXRatio == ratio.xRatio &&
            devYRatio >= ratio.yStartRatio &&
            devYRatio <= ratio.yEndRatio))
            {
                offset = (3 * 2) + (numRatios * 4) + (i * 2);
                WineEngGetFontData(font, MS_VDMX_TAG, offset, tmp, 2);
                offset = GET_BE_WORD(tmp);
                break;
            }
    }

    if(offset < 0) {
        FIXME("No suitable ratio found");
        return ppem;
    }

    if(WineEngGetFontData(font, MS_VDMX_TAG, offset, group, 4) != GDI_ERROR) {
        USHORT recs;
        BYTE startsz, endsz;
        BYTE *vTable;

        recs = GET_BE_WORD(group);
        startsz = group[2];
        endsz = group[3];

        TRACE("recs=%d  startsz=%d  endsz=%d\n", recs, startsz, endsz);

        vTable = HeapAlloc(GetProcessHeap(), 0, recs * 6);
        result = WineEngGetFontData(font, MS_VDMX_TAG, offset + 4, vTable, recs * 6);
        if(result == GDI_ERROR) {
            FIXME("Failed to retrieve vTable\n");
            goto end;
        }

        if(height > 0) {
            for(i = 0; i < recs; i++) {
                SHORT yMax = GET_BE_WORD(&vTable[(i * 6) + 2]);
                SHORT yMin = GET_BE_WORD(&vTable[(i * 6) + 4]);
                ppem = GET_BE_WORD(&vTable[i * 6]);

                if(yMax + -yMin == height) {
                    font->yMax = yMax;
                    font->yMin = yMin;
                    TRACE("ppem %ld found; height=%ld  yMax=%d  yMin=%d\n", ppem, height, font->yMax, font->yMin);
                    break;
                }
                if(yMax + -yMin > height) {
                    if(--i < 0) {
                        ppem = 0;
                        goto end; /* failed */
                    }
                    font->yMax = GET_BE_WORD(&vTable[(i * 6) + 2]);
                    font->yMin = GET_BE_WORD(&vTable[(i * 6) + 4]);
                    TRACE("ppem %ld found; height=%ld  yMax=%d  yMin=%d\n", ppem, height, font->yMax, font->yMin);
                    break;
                }
            }
            if(!font->yMax) {
                ppem = 0;
                TRACE("ppem not found for height %ld\n", height);
            }
        } else {
            ppem = -height;
            if(ppem < startsz || ppem > endsz)
                goto end;

            for(i = 0; i < recs; i++) {
                USHORT yPelHeight;
                yPelHeight = GET_BE_WORD(&vTable[i * 6]);

                if(yPelHeight > ppem)
                    break; /* failed */

                if(yPelHeight == ppem) {
                    font->yMax = GET_BE_WORD(&vTable[(i * 6) + 2]);
                    font->yMin = GET_BE_WORD(&vTable[(i * 6) + 4]);
                    TRACE("ppem %ld found; yMax=%d  yMin=%d\n", ppem, font->yMax, font->yMin);
                    break;
                }
            }
        }
        end:
        HeapFree(GetProcessHeap(), 0, vTable);
    }

    return ppem;
}

/*************************************************************
 * WineEngAddFontResourceEx
 *
 * FIXME: since the font list is a global, it is per-process. So adding
 *        a font won't add it to other processes. We will need to do wineserver
 *        stuff for this.
 */
INT WineEngAddFontResourceEx(LPCWSTR str, DWORD fl, PVOID pv)
{
    DWORD len;
    LPSTR astr;
    BOOL ret = FALSE;
    char unixname[MAX_PATH];

    TRACE("(%s, %08lx, %p)\n", debugstr_w(str), fl, pv);
    
    if (!freetype_so_handle)
    {
        ERR("can't without FreeType support!\n");
        return 0;
    }

    len = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    astr = HeapAlloc(GetProcessHeap(), 0, len * sizeof(CHAR));
    WideCharToMultiByte(CP_ACP, 0, str, -1, astr, len, NULL, NULL);

    if (wine_get_unix_file_name(astr, unixname, sizeof(unixname)))
        ret = AddFontFileToList(unixname);

    HeapFree(GetProcessHeap(), 0, astr);

    if (ret) return 1;
    return 0;
}

/*************************************************************
 * WineEngRemoveFontResourceEx
 *
 */
BOOL WineEngRemoveFontResourceEx(LPCWSTR str, DWORD fl, PVOID pv)
{
    FIXME("(%s, %ld, %p): stub\n", debugstr_w(str), fl, pv);
    return TRUE;
}


/*************************************************************
 * WineEngCreateFontInstance
 *
 */
GdiFont WineEngCreateFontInstance(DC *dc, HFONT hfont)
{
    GdiFont ret;
    Face *face;
    Family *family = NULL;
    WCHAR FaceName[LF_FACESIZE];
    BOOL bd, it;
    FONTOBJ *font = GDI_GetObjPtr(hfont, FONT_MAGIC);
    LOGFONTW *plf = &font->logfont;
    Face *bestBiggerFace, *bestSmallerFace;
    int biggerDiff, smallerDiff;

    float height;

    TRACE("%s, h=%ld, it=%d, weight=%ld, PandF=%02x, quality=%d, charset=%d orient %ld escapement %ld\n",
          debugstr_w(plf->lfFaceName), plf->lfHeight, plf->lfItalic,
          plf->lfWeight, plf->lfPitchAndFamily, plf->lfQuality,
          plf->lfCharSet, plf->lfOrientation,
          plf->lfEscapement);

    /* check the cache first */
    for(ret = GdiFontList; ret; ret = ret->next) {
        if(ret->hfont == hfont &&
                ret->xform.eM11 == dc->xformWorld2Vport.eM11 &&
                ret->xform.eM22 == dc->xformWorld2Vport.eM22) {
            GDI_ReleaseObj(hfont);
            TRACE("returning cached GdiFont(%p) for hFont %x\n", ret, hfont);
            return ret;
        }
    }

    if(!FontList) /* No fonts installed */
    {
        GDI_ReleaseObj(hfont);
        TRACE("No fonts installed\n");
        return NULL;
    }

    ret = alloc_font();
    memcpy(&(ret->xform), &(dc->xformWorld2Vport), sizeof(XFORM));

    strcpyW(FaceName, plf->lfFaceName);

    if (plf->lfOutPrecision & (OUT_TT_ONLY_PRECIS | OUT_TT_PRECIS))
        FIXME("Force selection of a TrueType font!\n");

    if(FaceName[0] != '\0') {
        family = FindFamilyOrAlias(FaceName);
    }

    if(!family) {
        if(plf->lfPitchAndFamily & FIXED_PITCH ||
           plf->lfPitchAndFamily & FF_MODERN)
          family = FindFamilyOrAlias(defFixed);
        else if(plf->lfPitchAndFamily & FF_ROMAN)
          family = FindFamilyOrAlias(defSerif);
        else if(plf->lfPitchAndFamily & FF_SWISS)
          family = FindFamilyOrAlias(defSans);
    }

    if (!family)
    {
        TRACE("no matches, attempting to use first sans\n");
        family = FindFamilyOrAlias(defSans);
    }

    if(!family) {
        family = FontList;
        FIXME("just using first face for now (no matches) (family: %s)\n",
              debugstr_w(FaceName));
    }

    it = plf->lfItalic ? 1 : 0;
    bd = plf->lfWeight > 550 ? 1 : 0;

    for(face = family->FirstFace; face; face = face->next) {
        TRACE("family has faces: %s, height: %i\n", face->file, face->Height);
    }

    bestBiggerFace = bestSmallerFace = NULL;
    biggerDiff = smallerDiff = 0;
    height = GDI_ROUND((float)plf->lfHeight * ret->xform.eM22);

    for(face = family->FirstFace; face; face = face->next)
    {
        int intheight = (int)height; /* floats will only mess up non-scalables */
        int findHeight = intheight < 0 ? -intheight : intheight;
        /* FIXME - neg (absolute height) vs pos (cell height) */

        int curHeightDiff = face->Height - findHeight;

        if (it ^ face->Italic) continue;
        if (bd ^ face->Bold) continue;

        if (family->ScalableFamily)
            break;

        if (curHeightDiff == 0)
        {
            bestBiggerFace = bestSmallerFace = face;
            biggerDiff = smallerDiff = 0;
            break;
        }
        else if (curHeightDiff > 0)
        { /* bigger */
            if (bestBiggerFace && curHeightDiff > biggerDiff)
                continue;
            else
            {
                bestBiggerFace = face;
                biggerDiff = curHeightDiff;
            }
        }
        else
        {/* smaller */
            if (bestSmallerFace && curHeightDiff < smallerDiff)
                continue;
            else
            {
                bestSmallerFace = face;
                smallerDiff = curHeightDiff;
            }
        }
    }

    if (!family->ScalableFamily)
    {
        TRACE("(%p, %p)\n", bestBiggerFace, bestSmallerFace);
        /* so we prefer smaller fonts, but if its too small,
         * bigger will have to do */
        if (!bestSmallerFace ||
                (bestBiggerFace && biggerDiff < (-smallerDiff * 0.50)))
            face = bestBiggerFace;
        else
            face = bestSmallerFace;
    }

    TRACE("Face: %p\n", face);

    if(!face) {
        face = family->FirstFace;
        if(it && !face->Italic) ret->fake_italic = TRUE;
        if(bd && !face->Bold) ret->fake_bold = TRUE;
    }
    ret->fixedSize = face->SizeIndex;
    ret->charset = get_nearest_charset(face, plf->lfCharSet);

    TRACE("Choosen %s %s (%i)\n", debugstr_w(family->FamilyName),
          debugstr_w(face->StyleName),
          face->Height);

    ret->ft_face = OpenFontFile(ret, face->file, height);

    if(ret->charset == SYMBOL_CHARSET)
        pFT_Select_Charmap(ret->ft_face, ft_encoding_symbol);
    ret->orientation = plf->lfOrientation;
    GDI_ReleaseObj(hfont);

    TRACE("caching: ftFont=%p  hfont=%x\n", ret, hfont);
    ret->hfont = hfont;
    ret->next = GdiFontList;
    GdiFontList = ret;

    return ret;
}

static void DumpGdiFontList(void)
{
    GdiFont ftFont;

    TRACE("---------- ftFont Cache ----------\n");
    for(ftFont = GdiFontList; ftFont; ftFont = ftFont->next) {
        FONTOBJ *font = GDI_GetObjPtr(ftFont->hfont, FONT_MAGIC);
        LOGFONTW *plf = &font->logfont;
        TRACE("ftFont=%p  hfont=%x (%s)\n",
               ftFont, ftFont->hfont, debugstr_w(plf->lfFaceName));
        GDI_ReleaseObj(ftFont->hfont);
    }
}

/*************************************************************
 * WineEngDestroyFontInstance
 *
 * free the ftFont associated with this handle
 *
 */
BOOL WineEngDestroyFontInstance(HFONT handle)
{
    GdiFont ftFont;
    GdiFont ftPrev = NULL;

    TRACE("destroying hfont=%x\n", handle);
    if(TRACE_ON(font))
        DumpGdiFontList();

    for(ftFont = GdiFontList; ftFont; ftFont = ftFont->next) {
        if(ftFont->hfont == handle) {
            if(ftPrev)
                ftPrev->next = ftFont->next;
            else
                GdiFontList = ftFont->next;

            free_font(ftFont);
            return TRUE;
        }
        ftPrev = ftFont;
    }
    return FALSE;
}

static void GetEnumStructs(Face *face, LPENUMLOGFONTEXW pelf,
                           LPNEWTEXTMETRICEXW pntm, LPDWORD ptype)
{
    UINT size;
    GdiFont font = alloc_font();
    TEXTMETRICW *tm = (TEXTMETRICW*)&pntm->ntmTm;
    WCHAR *family_nameW, *style_nameW;
    int lenfam, lensty;

    font->ft_face = OpenFontFile(font, face->file, 100);

    memset(&pelf->elfLogFont, 0, sizeof(LOGFONTW));


    if (FT_IS_SFNT(font->ft_face))
    {
        OUTLINETEXTMETRICW *potm;

        size = WineEngGetOutlineTextMetrics(font, 0, NULL);
        potm = HeapAlloc(GetProcessHeap(), 0, size);
        WineEngGetOutlineTextMetrics(font, size, potm);

#define OTM potm->otmTextMetrics
        pntm->ntmTm.tmHeight = OTM.tmHeight;
        pntm->ntmTm.tmAscent = OTM.tmAscent;
        pntm->ntmTm.tmDescent = OTM.tmDescent;
        pntm->ntmTm.tmInternalLeading = OTM.tmInternalLeading;
        pntm->ntmTm.tmExternalLeading = OTM.tmExternalLeading;
        pntm->ntmTm.tmAveCharWidth = OTM.tmAveCharWidth;
        pntm->ntmTm.tmMaxCharWidth = OTM.tmMaxCharWidth;
        pntm->ntmTm.tmWeight = OTM.tmWeight;
        pntm->ntmTm.tmOverhang = OTM.tmOverhang;
        pntm->ntmTm.tmDigitizedAspectX = OTM.tmDigitizedAspectX;
        pntm->ntmTm.tmDigitizedAspectY = OTM.tmDigitizedAspectY;
        pntm->ntmTm.tmFirstChar = OTM.tmFirstChar;
        pntm->ntmTm.tmLastChar = OTM.tmLastChar;
        pntm->ntmTm.tmDefaultChar = OTM.tmDefaultChar;
        pntm->ntmTm.tmBreakChar = OTM.tmBreakChar;
        pntm->ntmTm.tmItalic = OTM.tmItalic;
        pntm->ntmTm.tmUnderlined = pelf->elfLogFont.lfUnderline = OTM.tmUnderlined;
        pntm->ntmTm.tmStruckOut = pelf->elfLogFont.lfStrikeOut = OTM.tmStruckOut;
        pntm->ntmTm.tmPitchAndFamily = OTM.tmPitchAndFamily;
        pntm->ntmTm.tmCharSet = pelf->elfLogFont.lfCharSet = OTM.tmCharSet;

        pntm->ntmTm.ntmFlags = OTM.tmItalic ? NTM_ITALIC : 0;
        if(OTM.tmWeight > 550) pntm->ntmTm.ntmFlags |= NTM_BOLD;
        if(pntm->ntmTm.ntmFlags == 0) pntm->ntmTm.ntmFlags = NTM_REGULAR;

        pntm->ntmTm.ntmSizeEM = potm->otmEMSquare;
        pntm->ntmTm.ntmCellHeight = 0;
        pntm->ntmTm.ntmAvgWidth = 0;

        memset(&pntm->ntmFontSig, 0, sizeof(FONTSIGNATURE));
        *ptype = TRUETYPE_FONTTYPE;

        HeapFree(GetProcessHeap(), 0, potm);
#undef OTM
    }
    else
    {
        WineEngGetTextMetrics(font, tm);

        *ptype = RASTER_FONTTYPE;
    }

    pelf->elfLogFont.lfHeight = tm->tmHeight;
    pelf->elfLogFont.lfWidth = tm->tmAveCharWidth;
    pelf->elfLogFont.lfWeight = tm->tmWeight;
    pelf->elfLogFont.lfItalic = tm->tmItalic;
    pelf->elfLogFont.lfUnderline = tm->tmUnderlined;
    pelf->elfLogFont.lfStrikeOut = tm->tmStruckOut;
    pelf->elfLogFont.lfCharSet = tm->tmCharSet;

    pelf->elfLogFont.lfPitchAndFamily = (tm->tmPitchAndFamily & 0xf1) + 1;
    pelf->elfLogFont.lfCharSet = tm->tmCharSet;
    pelf->elfLogFont.lfOutPrecision = OUT_STROKE_PRECIS;
    pelf->elfLogFont.lfClipPrecision = CLIP_STROKE_PRECIS;
    pelf->elfLogFont.lfQuality = DRAFT_QUALITY;

    lenfam = MultiByteToWideChar(CP_ACP, 0, font->ft_face->family_name, -1, NULL, 0)
      * sizeof(WCHAR);
    family_nameW = HeapAlloc(GetProcessHeap(), 0, lenfam);
    MultiByteToWideChar(CP_ACP, 0, font->ft_face->family_name, -1,
                        family_nameW, lenfam);

    lensty = MultiByteToWideChar(CP_ACP, 0, font->ft_face->style_name, -1, NULL, 0)
      * sizeof(WCHAR);
    style_nameW = HeapAlloc(GetProcessHeap(), 0, lensty);
    MultiByteToWideChar(CP_ACP, 0, font->ft_face->style_name, -1,
                        style_nameW, lensty);

    /* FIXME: there are weird things to do with how the face name / family name
     * etc behave on different versions of windows for different types of
     * fonts - check MSDN.
     */
    lstrcpynW(pelf->elfLogFont.lfFaceName,
             family_nameW, LF_FACESIZE);
    lstrcpynW(pelf->elfFullName,
             family_nameW,  LF_FULLFACESIZE);
    lstrcpynW(pelf->elfStyle,
             style_nameW, LF_FACESIZE);
    pelf->elfScript[0] = '\0'; /* This will get set in GdiFont_EnumFonts */

    free_font(font);
    return;
}

/* returns TRUE if we should continue with the enumeration */
BOOL EnumFace(DEVICEFONTENUMPROC proc, LPARAM lparam, Face *face, DWORD *pRet, LPWSTR fakeName)
{
    ENUMLOGFONTEXW elf;
    NEWTEXTMETRICEXW ntm;
    DWORD type, ret = 1;
    FONTSIGNATURE fs;
    CHARSETINFO csi;
    int i;

    GetEnumStructs(face, &elf, &ntm, &type);
    if (fakeName)
    {
        strncmpW(elf.elfLogFont.lfFaceName, fakeName, LF_FACESIZE);
        elf.elfLogFont.lfFaceName[LF_FACESIZE-1] = '\0';
    }
    for(i = 0; i < 32; i++) {
        if(face->fsCsb[0] & (1L << i)) {
            fs.fsCsb[0] = 1L << i;
            fs.fsCsb[1] = 0;
            if(!TranslateCharsetInfo(fs.fsCsb, &csi,
                                     TCI_SRCFONTSIG))
                csi.ciCharset = DEFAULT_CHARSET;
            if(i == 31) csi.ciCharset = SYMBOL_CHARSET;
            if(csi.ciCharset != DEFAULT_CHARSET) {
                elf.elfLogFont.lfCharSet =
                  ntm.ntmTm.tmCharSet = csi.ciCharset;
                if(ElfScriptsW[i])
                    strcpyW(elf.elfScript, ElfScriptsW[i]);
                else
                    FIXME("Unknown elfscript for bit %d\n", i);
                TRACE("enuming face %s full %s style %s charset %d type %ld script %s it %d weight %ld ntmflags %08lx\n",
                      debugstr_w(elf.elfLogFont.lfFaceName),
                      debugstr_w(elf.elfFullName), debugstr_w(elf.elfStyle),
                      csi.ciCharset, type, debugstr_w(elf.elfScript),
                      elf.elfLogFont.lfItalic, elf.elfLogFont.lfWeight,
                      ntm.ntmTm.ntmFlags);
                ret = proc(&elf,
                           &ntm,
                           type, lparam);
                *pRet = ret;
                if(!ret) return FALSE;
            }
        }
    }
    return TRUE;
}

/*************************************************************
 * WineEngEnumFonts
 *
 */
DWORD WineEngEnumFonts(LPLOGFONTW plf, DEVICEFONTENUMPROC proc,
                       LPARAM lparam)
{
    Family *family;
    Face *face;
    ftFontAlias *alias;
    DWORD ret = 1;

    TRACE("facename = %s charset %d\n", debugstr_w(plf->lfFaceName), plf->lfCharSet);
    if(plf->lfFaceName[0]) {
        for(family = FontList; family; family = family->next) {
            if(!strcmpiW(plf->lfFaceName, family->FamilyName)) {
                for(face = family->FirstFace; face; face = face->next) {
                    if (!EnumFace(proc, lparam, face, &ret, NULL))
                        goto end;
                }
            }
        }
        for (alias = ftAliasTable; alias; alias = alias->next) {
            if (!strcmpiW(plf->lfFaceName, alias->faTypeFace)) {
                for (face = alias->faRealFamily->FirstFace; face; face = face->next) {
                    if (!EnumFace(proc, lparam, face, &ret, alias->faTypeFace))
                        goto end;
                }
            }
        }
    } else {
        for(family = FontList; family; family = family->next) {
            if (!EnumFace(proc, lparam, family->FirstFace, &ret, NULL))
                goto end;
        }
        for (alias = ftAliasTable; alias; alias = alias->next) {
            if (!EnumFace(proc, lparam, alias->faRealFamily->FirstFace, &ret, alias->faTypeFace))
                goto end;
        }
    }
end:
    return ret;
}

static void FTVectorToPOINTFX(FT_Vector *vec, POINTFX *pt)
{
    pt->x.value = vec->x >> 6;
    pt->x.fract = (vec->x & 0x3f) << 10;
    pt->x.fract |= ((pt->x.fract >> 6) | (pt->x.fract >> 12));
    pt->y.value = vec->y >> 6;
    pt->y.fract = (vec->y & 0x3f) << 10;
    pt->y.fract |= ((pt->y.fract >> 6) | (pt->y.fract >> 12));
    return;
}

static FT_UInt get_glyph_index(GdiFont font, UINT glyph)
{
    if(font->charset == SYMBOL_CHARSET && glyph < 0x100)
        glyph = glyph + 0xf000;
    return pFT_Get_Char_Index(font->ft_face, glyph);
}

BOOL OpenGlyphAndLoadMetrics(GdiFont font, UINT glyph_index, LPGLYPHMETRICS lpgm,
                             ftGlyphMetrics *extraMetrics, BOOL justMetrics)
{
    FT_Error err;
    FT_Pos left, right, top = 0, bottom = 0;
    FT_Face ft_face = font->ft_face;
    FT_Angle angle = 0;
    float widthXform;

    /* if they just want the metrics, we delay opening of the glyph
     * until we really have to, in the hope that we can avoid this hit
     * and actually use the cache.
     */
    if (!justMetrics)
    {
        err = pFT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
        if(err) {
            FIXME("FT_Load_Glyph on index %x returns %d\n", glyph_index, err);
            return FALSE;
        }
    }
    else TRACE("delaying glyph opening\n");

    if (glyph_index >= font->gmsize)
    {
        font->gmsize = (glyph_index / INIT_GM_SIZE + 1) * INIT_GM_SIZE;
        font->gm = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, font->gm,
                               font->gmsize * sizeof(*font->gm));
    }
    else if (font->gm[glyph_index].init && !extraMetrics)
    {
        memcpy(lpgm, &font->gm[glyph_index].gm, sizeof(*lpgm));
        if (extraMetrics)
            memcpy(extraMetrics, &font->gm[glyph_index].extraMetrics,
                   sizeof(ftGlyphMetrics));
        TRACE("returning cached\n");
        return TRUE;
    }

    if (justMetrics)
    {
        err = pFT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
        if(err) {
            FIXME("FT_Load_Glyph on index %x returns %d\n", glyph_index, err);
            return FALSE;
        }
    }

    widthXform = font->xform.eM11;
    left = (int)(ft_face->glyph->metrics.horiBearingX * widthXform) & -64;
    right = (int)(((ft_face->glyph->metrics.horiBearingX +
                  ft_face->glyph->metrics.width) * widthXform) + 63) & -64;

    if (FT_IS_SCALABLE(ft_face))
    {
        font->gm[glyph_index].adv =
            ((int)(ft_face->glyph->metrics.horiAdvance * widthXform) + 63) >> 6;
    }
    else
    {
        font->gm[glyph_index].adv =
            (((int)(ft_face->glyph->advance.x * widthXform) + 32) & -64 ) >> 6;
    }
    font->gm[glyph_index].lsb = left >> 6;
    font->gm[glyph_index].bbx = (right - left) >> 6;

    if(font->orientation == 0) {
        top = (ft_face->glyph->metrics.horiBearingY + 63) & -64;
        bottom = (ft_face->glyph->metrics.horiBearingY -
                  ft_face->glyph->metrics.height) & -64;
        lpgm->gmCellIncX = font->gm[glyph_index].adv;
        lpgm->gmCellIncY = 0;
    } else {
        INT xc, yc;
        FT_Vector vec;
        angle = font->orientation / 10 << 16;
        angle |= ((font->orientation % 10) * (1 << 16)) / 10;
        TRACE("angle %ld\n", angle >> 16);
        for(xc = 0; xc < 2; xc++) {
            for(yc = 0; yc < 2; yc++) {
                vec.x = ft_face->glyph->metrics.horiBearingX +
                  xc * ft_face->glyph->metrics.width;
                vec.y = ft_face->glyph->metrics.horiBearingY -
                  yc * ft_face->glyph->metrics.height;
                TRACE("Vec %ld,%ld\n", vec.x, vec.y);
                pFT_Vector_Rotate(&vec, angle);
                if(xc == 0 && yc == 0) {
                    left = right = vec.x;
                    top = bottom = vec.y;
                } else {
                    if(vec.x < left) left = vec.x;
                    else if(vec.x > right) right = vec.x;
                    if(vec.y < bottom) bottom = vec.y;
                    else if(vec.y > top) top = vec.y;
                }
            }
        }
        left = left & -64;
        right = (right + 63) & -64;
        bottom = bottom & -64;
        top = (top + 63) & -64;

        TRACE("transformed box: (%d,%d - %d,%d)\n",
                (int)(left >> 6),
                (int)(top >> 6),
                (int)(right >> 6),
                (int)(bottom >> 6));
        vec.x = ft_face->glyph->metrics.horiAdvance;
        vec.y = 0;
        pFT_Vector_Rotate(&vec, angle);
        lpgm->gmCellIncX = (vec.x+63) >> 6;
        lpgm->gmCellIncY = -(vec.y+63) >> 6;
    }
    lpgm->gmBlackBoxX = (right - left) >> 6;
    lpgm->gmBlackBoxY = (top - bottom) >> 6;
    lpgm->gmptGlyphOrigin.x = left >> 6;
    lpgm->gmptGlyphOrigin.y = top >> 6;

    memcpy(&font->gm[glyph_index].gm, lpgm, sizeof(*lpgm));
    font->gm[glyph_index].init = TRUE;

    font->gm[glyph_index].extraMetrics.left = left;
    font->gm[glyph_index].extraMetrics.right = right;
    font->gm[glyph_index].extraMetrics.top = top;
    font->gm[glyph_index].extraMetrics.bottom = bottom;
    font->gm[glyph_index].extraMetrics.angle = angle;

    if (extraMetrics)
        memcpy(extraMetrics, &font->gm[glyph_index].extraMetrics, sizeof(ftGlyphMetrics));

    return TRUE;
}

BOOL GetGlyphBitmap(GdiFont font, LPGLYPHMETRICS lpgm, LPVOID buf,
                    const MAT2 *lpMat, ftGlyphMetrics *extraMetrics,
                    BOOL gray, DWORD width, DWORD height, DWORD pitch)
{
    FT_Face ft_face = font->ft_face;
    FT_GlyphSlot glyphslot = ft_face->glyph;

    /* Note: FreeType will only set 'black' bits for us. */
    memset(buf, 0, pitch * height);

    if (glyphslot->format == ft_glyph_format_outline)
    { /* scalable */
        FT_Bitmap ft_bitmap;
        TRACE("(scalable) width %ld, height %ld, pitch %ld\n", width, height, pitch);

        ft_bitmap.width = width;
        ft_bitmap.rows = height;
        ft_bitmap.pitch = pitch;
        ft_bitmap.pixel_mode = gray ? ft_pixel_mode_grays : ft_pixel_mode_mono;
        ft_bitmap.num_grays = gray ? 256 : 0;
        ft_bitmap.palette_mode = 0;
        ft_bitmap.palette = NULL;
        ft_bitmap.buffer = buf;

        if(font->orientation) {
            FT_Matrix matrix;
            matrix.xx = matrix.yy = pFT_Cos(extraMetrics->angle);
            matrix.xy = -pFT_Sin(extraMetrics->angle);
            matrix.yx = -matrix.xy;

            pFT_Outline_Transform(&glyphslot->outline, &matrix);
        } else if (font->xform.eM11 != 1.0) {
            FT_Matrix matrix;
            matrix.xx = FixedFromFloat(font->xform.eM11);
            matrix.xy = matrix.yx = 0;
            matrix.yy = (1 << 16);
            pFT_Outline_Transform(&glyphslot->outline, &matrix);
        }

        pFT_Outline_Translate(&glyphslot->outline, -(extraMetrics->left),
                              -(extraMetrics->bottom) );

        pFT_Outline_Get_Bitmap(library, &glyphslot->outline, &ft_bitmap);
    }
    else if (glyphslot->format == ft_glyph_format_bitmap)
    {
        unsigned char *srcLine, *dstLine;
        int h, bytes;

        TRACE("bitmap\n");

        srcLine = glyphslot->bitmap.buffer;
        dstLine = buf;
        h = glyphslot->bitmap.rows;
        bytes = (glyphslot->bitmap.width + 7) >> 3;
        while (h--)
        {
            if (gray)
            {
                int x;
                for (x = 0; x < glyphslot->bitmap.width; x++)
                {
                    unsigned char a = ((srcLine[x >> 3] & (0x80 >> (x & 7))) ?
                                       0xff : 0x00);
                    dstLine[x] = a;
                }
                dstLine += pitch;
                srcLine += glyphslot->bitmap.pitch;
            }
            else
            {
                memcpy(dstLine, srcLine, bytes);
                dstLine += pitch;
                srcLine += glyphslot->bitmap.pitch;
            }
        }
    }
    else
    {
        FIXME("unknown freetype glyph format\n");
        return FALSE;
    }
    return TRUE;
}

/*************************************************************
 * WineEngGetGlyphIndices
 *
 */
DWORD WineEngGetGlyphIndices(GdiFont font, LPCWSTR lpstr, INT count,
                             LPWORD pgi, DWORD flags)
{
    DWORD c;
    TRACE("%p, %s, %d, %p, 0x%lx\n", font, debugstr_wn(lpstr, count), count, pgi, flags);
    for (c=0; c<count; c++) {
        FT_UInt glyph_index = get_glyph_index(font, lpstr[c]);
        /* FIXME: UTF-16 encode */
        pgi[c] = glyph_index;
    }
    return count;
}

/*************************************************************
 * WineEngGetGlyphOutline
 *
 */
DWORD WineEngGetGlyphOutline(GdiFont font, UINT glyph, UINT format,
                             LPGLYPHMETRICS lpgm, DWORD buflen, LPVOID buf,
                             const MAT2* lpmat)
{
    FT_Face ft_face = font->ft_face;
    FT_UInt glyph_index;
    DWORD width, height, pitch, needed = 0;
    ftGlyphMetrics extraMetrics; /* freetype metrics that we have to pass around */

    TRACE("%p, %04x, %08x, %p, %08lx, %p, %p\n", font, glyph, format, lpgm,
          buflen, buf, lpmat);

    if(format & GGO_GLYPH_INDEX) {
        glyph_index = glyph;
        format &= ~GGO_GLYPH_INDEX;
    } else
        glyph_index = get_glyph_index(font, glyph);

    if (!OpenGlyphAndLoadMetrics(font, glyph_index, lpgm, &extraMetrics, FALSE))
        return GDI_ERROR;

    width = lpgm->gmBlackBoxX;
    height = lpgm->gmBlackBoxY;

    if(format == GGO_METRICS)
        return 1; /* FIXME */

    pitch = (width + 3) / 4 * 4;

    switch(format) {
    case GGO_BITMAP:
        pitch = (width + 31) / 32 * 4;
    case GGO_GRAY2_BITMAP:
    case GGO_GRAY4_BITMAP:
    case GGO_GRAY8_BITMAP:
    case WINE_GGO_GRAY16_BITMAP:
      {
        int mult, row, col;
        BYTE *start, *ptr;

        needed = pitch * height;
        if (!buf || buflen < needed) return needed;

        if (!GetGlyphBitmap(font, lpgm, buf, lpmat,
                            &extraMetrics, format != GGO_BITMAP,
                            width, height, pitch))
            return GDI_ERROR;

        if (format == GGO_BITMAP)
            break;
        if(format == GGO_GRAY2_BITMAP)
            mult = 4;
        else if(format == GGO_GRAY4_BITMAP)
            mult = 16;
        else if(format == GGO_GRAY8_BITMAP)
            mult = 64;
        else if(format == WINE_GGO_GRAY16_BITMAP)
            break;
        else {
            assert(0);
            break;
        }

        start = buf;
        for(row = 0; row < height; row++) {
            ptr = start;
            for(col = 0; col < width; col++, ptr++) {
                *ptr = ((unsigned int)(*ptr) * mult) / 255;
            }
            start += pitch;
        }
        break;
      }

    case GGO_NATIVE:
      {
        int contour, point = 0, first_pt;
        FT_Outline *outline = &ft_face->glyph->outline;
        TTPOLYGONHEADER *pph;
        TTPOLYCURVE *ppc;
        DWORD pph_start, cpfx, type;

        if(ft_face->glyph->format != ft_glyph_format_outline)
            return GDI_ERROR;

        if(buflen == 0) buf = NULL;

        for(contour = 0; contour < outline->n_contours; contour++) {
            pph_start = needed;
            pph = buf + needed;
            first_pt = point;
            if(buf) {
                pph->dwType = TT_POLYGON_TYPE;
                FTVectorToPOINTFX(&outline->points[point], &pph->pfxStart);
            }
            needed += sizeof(*pph);
            point++;
            while(point <= outline->contours[contour]) {
                ppc = buf + needed;
                type = (outline->tags[point] == FT_Curve_Tag_On) ?
                  TT_PRIM_LINE : TT_PRIM_QSPLINE;
                cpfx = 0;
                do {
                    if(buf)
                        FTVectorToPOINTFX(&outline->points[point], &ppc->apfx[cpfx]);
                    cpfx++;
                    point++;
                } while(point <= outline->contours[contour] &&
                        outline->tags[point] == outline->tags[point-1]);
                /* At the end of a contour Windows adds the start point */
                if(point > outline->contours[contour]) {
                    if(buf)
                        FTVectorToPOINTFX(&outline->points[first_pt], &ppc->apfx[cpfx]);
                    cpfx++;
                } else if(outline->tags[point] == FT_Curve_Tag_On) {
                  /* add closing pt for bezier */
                    if(buf)
                        FTVectorToPOINTFX(&outline->points[point], &ppc->apfx[cpfx]);
                    cpfx++;
                    point++;
                }
                if(buf) {
                    ppc->wType = type;
                    ppc->cpfx = cpfx;
                }
                needed += sizeof(*ppc) + (cpfx - 1) * sizeof(POINTFX);
            }
            if(buf)
                pph->cb = needed - pph_start;
        }
        break;
      }
    default:
        FIXME("Unsupported format %d\n", format);
        return GDI_ERROR;
    }
    return needed;
}

/*************************************************************
 * WineEngGetTextMetrics_Other
 *
 * to be used for GetTextMetrics that don't have their own specific
 * function (for now thats just TT/OT etc).
 *
 * This is basically a copy of the TT version, however using the FreeType
 * font properties instead.. This may result in some wrongness, however.
 * MS don't really document how they get the properties for other fonts.
 * So I'll just guess its somewhat like Freetypes way. (They document
 * truetype stuff pretty well).
 */
BOOL WineEngGetTextMetrics_Other(GdiFont font, LPTEXTMETRICW ptm)
{
    FT_Face ft_face = font->ft_face;
    FT_Fixed x_scale, y_scale;
    FT_UInt gindex;

    x_scale = ft_face->size->metrics.x_scale;
    y_scale = ft_face->size->metrics.y_scale;

    TRACE("other\n");

    x_scale = ft_face->size->metrics.x_scale;
    y_scale = ft_face->size->metrics.y_scale;

    TRACE("a = %d, d = %d, h = %d, maxY = %ld, minY = %ld\n",
          ft_face->ascender, ft_face->descender, ft_face->height,
          ft_face->bbox.yMax, ft_face->bbox.yMin);

    if (font->yMax)
    {
        ptm->tmAscent = font->yMax;
        ptm->tmDescent = -font->yMin;
    }
    else if (FT_IS_SCALABLE(ft_face))
    {
        /* >> 6 because freetype stores them as fixed point values in 26.6 format.
         * FIXME: should we do a + 32 like Huw does below for TT fonts?
         */
        ptm->tmAscent = (pFT_MulFix(ft_face->ascender, y_scale) /* + 32 */) >> 6;
        ptm->tmDescent = -((pFT_MulFix(ft_face->descender, y_scale) /* + 32 */) >> 6);
    }
    else
    {
#if 0 /* damn you freetype! */
        ptm->tmAscent = ft_face->size->metrics.ascender >> 6;
        ptm->tmDescent = ft_face->size->metrics.descender >> 6;
#else
        /* FIXME how can I get ascent / descent for fixed fonts?!? silly freetype */
        ptm->tmAscent = ft_face->available_sizes[font->fixedSize].height;
#if 1
        WARN("fudging ascent\n");
       /* dodgy hack to give the fonts a bit of height.. since available_size[x].height
        * is baseline to baseline.. And I can't find ascent/descent on a per size
        * basis..
        */
        ptm->tmDescent = ptm->tmAscent * 0.20;
        ptm->tmAscent *= 0.90;
#else
        ptm->tmDescent = 0;
#endif
#endif
    }

    ptm->tmInternalLeading = (ptm->tmAscent + ptm->tmDescent) - ft_face->size->metrics.y_ppem;
    ptm->tmHeight = ptm->tmAscent + ptm->tmDescent;
    TRACE("ascent: %ld, descent: %ld\n", ptm->tmAscent, ptm->tmDescent);
    TRACE("height: %li\n", ptm->tmHeight);

    ptm->tmExternalLeading = 0; /* FIXME */

    if (FT_IS_SCALABLE(ft_face))
    {
        ptm->tmMaxCharWidth = (pFT_MulFix(ft_face->bbox.xMax - ft_face->bbox.xMin, x_scale) + 32) >> 6;
    }
    else
    {
        ptm->tmMaxCharWidth = ft_face->available_sizes[font->fixedSize].width;
    }

    ptm->tmAveCharWidth = ptm->tmMaxCharWidth;
          /* FIXME - freetype has no generic average width, afaics.
           *    go through glyphs and calculate it */

    ptm->tmWeight = (font->fake_bold || (ft_face->style_flags & FT_STYLE_FLAG_BOLD))
                         ? FW_BOLD : FW_REGULAR; /* seems freetype only likes 'bold or not bold'. no veriations on that. */
    ptm->tmOverhang = 0; /* FIXME */
    ptm->tmDigitizedAspectX = 300; /* no idea */
    ptm->tmDigitizedAspectY = 300;
    ptm->tmLastChar = ptm->tmFirstChar = pFT_Get_First_Char(ft_face, &gindex);
    while (gindex != 0)
    {
        ptm->tmLastChar = pFT_Get_Next_Char(ft_face, ptm->tmLastChar, &gindex);
    }
    ptm->tmDefaultChar = 0; /* FIXME */
    ptm->tmBreakChar = 0; /* FIXME */

    ptm->tmItalic = font->fake_italic ? 255 : ((ft_face->style_flags & FT_STYLE_FLAG_ITALIC) ? 255 : 0);
    ptm->tmUnderlined = 0; /* FIXME */
    ptm->tmStruckOut = 0;

    ptm->tmPitchAndFamily = FT_IS_FIXED_WIDTH(ft_face) ? 0 : TMPF_FIXED_PITCH;
    if(FT_IS_SCALABLE(ft_face))
        ptm->tmPitchAndFamily |= TMPF_VECTOR;
    if (ptm->tmPitchAndFamily & TMPF_FIXED_PITCH)
        ptm->tmPitchAndFamily |= FF_ROMAN;
    else
        ptm->tmPitchAndFamily |= FF_MODERN;

    ptm->tmCharSet = font->charset;

    return TRUE;
}


/*************************************************************
 * WineEngGetTextMetrics_TT
 *
 */
BOOL WineEngGetTextMetrics_TT(GdiFont font, LPTEXTMETRICW ptm)
{
    FT_Face ft_face = font->ft_face;
    TT_OS2 *pOS2;
    TT_HoriHeader *pHori;
    FT_Fixed x_scale, y_scale;

    TRACE("TrueType et al.\n");

    x_scale = ft_face->size->metrics.x_scale;
    y_scale = ft_face->size->metrics.y_scale;

    pHori = pFT_Get_Sfnt_Table(ft_face, ft_sfnt_hhea);
    if(!pHori) {
      FIXME("Can't find HHEA table - not TT font?\n");
      return 0;
    }
	
    pOS2 = pFT_Get_Sfnt_Table(ft_face, ft_sfnt_os2);
    if(!pOS2) {
      TRACE("Can't find OS/2 table - Use Generic Freetype path instead\n");
      return WineEngGetTextMetrics_Other(font, ptm);;
    }

    TRACE("OS/2 winA = %d winD = %d typoA = %d typoD = %d typoLG = %d FT_Face a = %d, d = %d, h = %d: HORZ a = %d, d = %d lg = %d maxY = %ld minY = %ld\n",
          pOS2->usWinAscent, pOS2->usWinDescent,
          pOS2->sTypoAscender, pOS2->sTypoDescender, pOS2->sTypoLineGap,
          ft_face->ascender, ft_face->descender, ft_face->height,
          pHori->Ascender, pHori->Descender, pHori->Line_Gap,
          ft_face->bbox.yMax, ft_face->bbox.yMin);

    if(font->yMax) {
        ptm->tmAscent = font->yMax;
        ptm->tmDescent = -font->yMin;
        ptm->tmInternalLeading = (ptm->tmAscent + ptm->tmDescent) - ft_face->size->metrics.y_ppem;
    } else {
        ptm->tmAscent = (pFT_MulFix(pOS2->usWinAscent, y_scale) + 32) >> 6;
        ptm->tmDescent = (pFT_MulFix(pOS2->usWinDescent, y_scale) + 32) >> 6;
        ptm->tmInternalLeading = (pFT_MulFix(pOS2->usWinAscent + pOS2->usWinDescent
                                            - ft_face->units_per_EM, y_scale) + 32) >> 6;
    }

    ptm->tmHeight = ptm->tmAscent + ptm->tmDescent;

    /* MSDN says:
     el = MAX(0, LineGap - ((WinAscent + WinDescent) - (Ascender - Descender)))
    */
    ptm->tmExternalLeading = max(0, (pFT_MulFix(pHori->Line_Gap -
                        ((pOS2->usWinAscent + pOS2->usWinDescent) -
                  (pHori->Ascender - pHori->Descender)), y_scale) + 32) >> 6);

    ptm->tmAveCharWidth = (pFT_MulFix(pOS2->xAvgCharWidth, x_scale) + 32) >> 6;
    ptm->tmMaxCharWidth = (pFT_MulFix(ft_face->bbox.xMax - ft_face->bbox.xMin, x_scale) + 32) >> 6;
    ptm->tmWeight = font->fake_bold ? FW_BOLD : pOS2->usWeightClass;
    ptm->tmOverhang = 0;
    ptm->tmDigitizedAspectX = 300;
    ptm->tmDigitizedAspectY = 300;
    ptm->tmFirstChar = pOS2->usFirstCharIndex;
    ptm->tmLastChar = pOS2->usLastCharIndex;
    ptm->tmDefaultChar = pOS2->usDefaultChar;
    ptm->tmBreakChar = pOS2->usBreakChar;
    ptm->tmItalic = font->fake_italic ? 255 : ((ft_face->style_flags & FT_STYLE_FLAG_ITALIC) ? 255 : 0);
    ptm->tmUnderlined = 0; /* entry in OS2 table */
    ptm->tmStruckOut = 0; /* entry in OS2 table */

    /* Yes this is correct; braindead api */
    ptm->tmPitchAndFamily = FT_IS_FIXED_WIDTH(ft_face) ? 0 : TMPF_FIXED_PITCH;
    ptm->tmPitchAndFamily |= TMPF_VECTOR | TMPF_TRUETYPE;

    if (ptm->tmPitchAndFamily & TMPF_FIXED_PITCH)
        ptm->tmPitchAndFamily |= FF_ROMAN;
    else
        ptm->tmPitchAndFamily |= FF_MODERN;

    ptm->tmCharSet = font->charset;
    return TRUE;
}


/*************************************************************
 * WineEngGetTextMetrics
 *
 */
BOOL WineEngGetTextMetrics(GdiFont font, LPTEXTMETRICW ptm)
{
    BOOL ret;
    TRACE("font=%p, ptm=%p\n", font, ptm);

    if (font->cachedMetrics)
    {
        TRACE("using cached metrics\n");
        memcpy(ptm, font->cachedMetrics, sizeof(TEXTMETRICW));
        return TRUE;
    }

    if (FT_IS_SFNT(font->ft_face))
        ret = WineEngGetTextMetrics_TT(font, ptm);
    else
        ret = WineEngGetTextMetrics_Other(font, ptm);

    if (ret)
    {
        ptm->tmAveCharWidth *= font->xform.eM11;
        font->cachedMetrics = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TEXTMETRICW));
        memcpy(font->cachedMetrics, ptm, sizeof(TEXTMETRICW));
    }
    return ret;
}

/*************************************************************
 * WineEngGetOutlineTextMetrics
 */
UINT WineEngGetOutlineTextMetrics(GdiFont font, UINT cbSize,
                                  OUTLINETEXTMETRICW *potm)
{
    FT_Face ft_face = font->ft_face;
    UINT needed, lenfam, lensty, ret;
    TT_OS2 *pOS2;
    TT_HoriHeader *pHori;
    FT_Fixed x_scale, y_scale;
    WCHAR *family_nameW, *style_nameW;
    WCHAR spaceW[] = {' ', '\0'};
    char *cp;

    TRACE("font=%p\n", font);

    if (!FT_IS_SFNT(font->ft_face)) return 0;

    needed = sizeof(*potm);

    lenfam = MultiByteToWideChar(CP_ACP, 0, ft_face->family_name, -1, NULL, 0)
      * sizeof(WCHAR);
    family_nameW = HeapAlloc(GetProcessHeap(), 0, lenfam);
    MultiByteToWideChar(CP_ACP, 0, ft_face->family_name, -1,
                        family_nameW, lenfam);

    lensty = MultiByteToWideChar(CP_ACP, 0, ft_face->style_name, -1, NULL, 0)
      * sizeof(WCHAR);
    style_nameW = HeapAlloc(GetProcessHeap(), 0, lensty);
    MultiByteToWideChar(CP_ACP, 0, ft_face->style_name, -1,
                        style_nameW, lensty);

    /* These names should be read from the TT name table */

    /* length of otmpFamilyName */
    needed += lenfam;

    /* length of otmpFaceName */
    if(!strcasecmp(ft_face->style_name, "regular")) {
      needed += lenfam; /* just the family name */
    } else {
      needed += lenfam + lensty; /* family + " " + style */
    }

    /* length of otmpStyleName */
    needed += lensty;

    /* length of otmpFullName */
    needed += lenfam + lensty;

    if(needed > cbSize) {
        ret = needed;
        goto end;
    }

    x_scale = ft_face->size->metrics.x_scale;
    y_scale = ft_face->size->metrics.y_scale;

    pOS2 = pFT_Get_Sfnt_Table(ft_face, ft_sfnt_os2);
    if(!pOS2) {
        FIXME("Can't find OS/2 table - not TT font?\n");
        ret = 0;
        goto end;
    }

    pHori = pFT_Get_Sfnt_Table(ft_face, ft_sfnt_hhea);
    if(!pHori) {
        FIXME("Can't find HHEA table - not TT font?\n");
        ret = 0;
        goto end;
    }

    potm->otmSize = needed;

    WineEngGetTextMetrics(font, &potm->otmTextMetrics);

    potm->otmFiller = 0;
    memcpy(&potm->otmPanoseNumber, pOS2->panose, PANOSE_COUNT);
    potm->otmfsSelection = pOS2->fsSelection;
    potm->otmfsType = pOS2->fsType;
    potm->otmsCharSlopeRise = pHori->caret_Slope_Rise;
    potm->otmsCharSlopeRun = pHori->caret_Slope_Run;
    potm->otmItalicAngle = 0; /* POST table */
    potm->otmEMSquare = ft_face->units_per_EM;
    potm->otmAscent = (pFT_MulFix(pOS2->sTypoAscender, y_scale) + 32) >> 6;
    potm->otmDescent = (pFT_MulFix(pOS2->sTypoDescender, y_scale) + 32) >> 6;
    potm->otmLineGap = (pFT_MulFix(pOS2->sTypoLineGap, y_scale) + 32) >> 6;
    potm->otmsCapEmHeight = (pFT_MulFix(pOS2->sCapHeight, y_scale) + 32) >> 6;
    potm->otmsXHeight = (pFT_MulFix(pOS2->sxHeight, y_scale) + 32) >> 6;
    potm->otmrcFontBox.left = ft_face->bbox.xMin;
    potm->otmrcFontBox.right = ft_face->bbox.xMax;
    potm->otmrcFontBox.top = ft_face->bbox.yMin;
    potm->otmrcFontBox.bottom = ft_face->bbox.yMax;
    potm->otmMacAscent = 0; /* where do these come from ? */
    potm->otmMacDescent = 0;
    potm->otmMacLineGap = 0;
    potm->otmusMinimumPPEM = 0; /* TT Header */
    potm->otmptSubscriptSize.x = (pFT_MulFix(pOS2->ySubscriptXSize, x_scale) + 32) >> 6;
    potm->otmptSubscriptSize.y = (pFT_MulFix(pOS2->ySubscriptYSize, y_scale) + 32) >> 6;
    potm->otmptSubscriptOffset.x = (pFT_MulFix(pOS2->ySubscriptXOffset, x_scale) + 32) >> 6;
    potm->otmptSubscriptOffset.y = (pFT_MulFix(pOS2->ySubscriptYOffset, y_scale) + 32) >> 6;
    potm->otmptSuperscriptSize.x = (pFT_MulFix(pOS2->ySuperscriptXSize, x_scale) + 32) >> 6;
    potm->otmptSuperscriptSize.y = (pFT_MulFix(pOS2->ySuperscriptYSize, y_scale) + 32) >> 6;
    potm->otmptSuperscriptOffset.x = (pFT_MulFix(pOS2->ySuperscriptXOffset, x_scale) + 32) >> 6;
    potm->otmptSuperscriptOffset.y = (pFT_MulFix(pOS2->ySuperscriptYOffset, y_scale) + 32) >> 6;
    potm->otmsStrikeoutSize = (pFT_MulFix(pOS2->yStrikeoutSize, y_scale) + 32) >> 6;
    potm->otmsStrikeoutPosition = (pFT_MulFix(pOS2->yStrikeoutPosition, y_scale) + 32) >> 6;
    potm->otmsUnderscoreSize = 0; /* POST Header */
    potm->otmsUnderscorePosition = 0; /* POST Header */

    /* otmp* members should clearly have type ptrdiff_t, but M$ knows best */
    cp = (char*)potm + sizeof(*potm);
    potm->otmpFamilyName = (LPSTR)(cp - (char*)potm);
    strcpyW((WCHAR*)cp, family_nameW);
    cp += lenfam;
    potm->otmpStyleName = (LPSTR)(cp - (char*)potm);
    strcpyW((WCHAR*)cp, style_nameW);
    cp += lensty;
    potm->otmpFaceName = (LPSTR)(cp - (char*)potm);
    strcpyW((WCHAR*)cp, family_nameW);
    if(strcasecmp(ft_face->style_name, "regular")) {
        strcatW((WCHAR*)cp, spaceW);
        strcatW((WCHAR*)cp, style_nameW);
        cp += lenfam + lensty;
    } else
        cp += lenfam;
    potm->otmpFullName = (LPSTR)(cp - (char*)potm);
    strcpyW((WCHAR*)cp, family_nameW);
    strcatW((WCHAR*)cp, spaceW);
    strcatW((WCHAR*)cp, style_nameW);
    ret = needed;

 end:
    HeapFree(GetProcessHeap(), 0, style_nameW);
    HeapFree(GetProcessHeap(), 0, family_nameW);

    return ret;
}

/*************************************************************
 * WineEngGetCharABCWidth
 */
BOOL WineEngGetCharABCWidth(GdiFont font, UINT firstChar,
                            UINT lastChar, LPABC abc)
{
    GLYPHMETRICS gm;
    int i;
    TRACE("(%p, %i, %i, %p)\n", font, firstChar, lastChar, abc);

    for (i=firstChar;i<=lastChar;i++) {
        UINT glyphIndex = get_glyph_index(font, i);

        OpenGlyphAndLoadMetrics(font, glyphIndex, &gm, NULL, TRUE);

        abc[i-firstChar].abcA = gm.gmptGlyphOrigin.x;
        abc[i-firstChar].abcB = gm.gmBlackBoxX;
        abc[i-firstChar].abcC = gm.gmCellIncX - gm.gmptGlyphOrigin.x - gm.gmBlackBoxX;
    }

    return TRUE;
}

/*************************************************************
 * WineEngGetCharABCWidthI
 */
BOOL WineEngGetCharABCWidthI(GdiFont font, UINT first,
                             UINT count, LPWORD pgi, LPABC abc)
{
    GLYPHMETRICS gm;
    int i;
    TRACE("(%p, %i, %i, %p, %p)\n", font, first, count, pgi, abc);

    for (i=0; i<count; i++) {
        UINT glyphIndex = pgi ? pgi[i] : (first + i);

        OpenGlyphAndLoadMetrics(font, glyphIndex, &gm, NULL, TRUE);

        abc[i].abcA = gm.gmptGlyphOrigin.x;
        abc[i].abcB = gm.gmBlackBoxX;
        abc[i].abcC = gm.gmCellIncX - gm.gmptGlyphOrigin.x - gm.gmBlackBoxX;
    }

    return TRUE;
}


/*************************************************************
 * WineEngGetCharWidthI
 */
BOOL WineEngGetCharWidthI(GdiFont font, UINT first,
                          UINT count, LPWORD pgi, LPINT lpBuffer)
{
    GLYPHMETRICS gm;
    int i;
    TRACE("(%p, %i, %i, %p, %p)\n", font, first, count, pgi, lpBuffer);

    for (i=0; i<count; i++) {
        UINT glyphIndex = pgi ? pgi[i] : (first + i);

        OpenGlyphAndLoadMetrics(font, glyphIndex, &gm, NULL, TRUE);

        lpBuffer[i] = gm.gmBlackBoxX;
    }

    return TRUE;
}

/*************************************************************
 * WineEngGetCharWidth
 *
 */
BOOL WineEngGetCharWidth(GdiFont font, UINT firstChar, UINT lastChar,
                         LPINT buffer)
{
    UINT c;
    GLYPHMETRICS gm;
    FT_UInt glyph_index;

    TRACE("%p, %d, %d, %p\n", font, firstChar, lastChar, buffer);

    for(c = firstChar; c <= lastChar; c++) {
        glyph_index = get_glyph_index(font, c);
        OpenGlyphAndLoadMetrics(font, glyph_index, &gm, NULL, TRUE);
        buffer[c - firstChar] = font->gm[glyph_index].adv;
    }
    return TRUE;
}

/*************************************************************
 * WineEngGetTextExtentPoint
 *
 */
BOOL WineEngGetTextExtentPoint(GdiFont font, LPCWSTR wstr, INT count,
                               LPSIZE size)
{
    UINT idx;
    GLYPHMETRICS gm;
    TEXTMETRICW tm;
    FT_UInt glyph_index;

    TRACE("%p, %s, %d, %p\n", font, debugstr_wn(wstr, count), count,
          size);

    /* FIXME: replace this with just WineEngGetGlyphIndices
     * and WineEngGetTextExtentPointI, perhaps */

    size->cx = 0;
    WineEngGetTextMetrics(font, &tm);
    size->cy = tm.tmHeight;

    for(idx = 0; idx < count; idx++) {
        glyph_index = get_glyph_index(font, wstr[idx]);
        OpenGlyphAndLoadMetrics(font, glyph_index, &gm, NULL, TRUE);
        size->cx += font->gm[glyph_index].adv;
        TRACE("char '%c' has width: %i, size->cx now: %li\n", (CHAR)wstr[idx],
              font->gm[glyph_index].adv, size->cx);
        if (size->cy < font->gm[glyph_index].gm.gmBlackBoxY)
        {
            WARN("broken font(?) - text metrics height too small. was %li, now %i\n",
                  size->cy, font->gm[glyph_index].gm.gmBlackBoxY);
            size->cy = font->gm[glyph_index].gm.gmBlackBoxY;
        }
    }
    TRACE("return %ld,%ld\n", size->cx, size->cy);
    return TRUE;
}

/*************************************************************
 * WineEngGetTextExtentPointI
 *
 */
BOOL WineEngGetTextExtentPointI(GdiFont font, LPWORD pgi, INT count,
                                LPSIZE size)
{
    UINT idx;
    GLYPHMETRICS gm;
    TEXTMETRICW tm;
    FT_UInt glyph_index;

    TRACE("%p, %p, %d, %p\n", font, pgi, count,
          size);

    size->cx = 0;
    WineEngGetTextMetrics(font, &tm);
    size->cy = tm.tmHeight;

    for(idx = 0; idx < count; idx++) {
        glyph_index = pgi[idx];
        OpenGlyphAndLoadMetrics(font, glyph_index, &gm, NULL, TRUE);
        size->cx += font->gm[glyph_index].adv;
        TRACE("glyph %d has width: %i, size->cx now: %li\n", glyph_index,
              font->gm[glyph_index].adv, size->cx);
        if (size->cy < font->gm[glyph_index].gm.gmBlackBoxY)
        {
            WARN("broken font(?) - text metrics height too small. was %li, now %i\n",
                  size->cy, font->gm[glyph_index].gm.gmBlackBoxY);
            size->cy = font->gm[glyph_index].gm.gmBlackBoxY;
        }
    }
    TRACE("return %ld,%ld\n", size->cx, size->cy);
    return TRUE;
}

/*************************************************************
 * WineEngGetFontData
 *
 */
DWORD WineEngGetFontData(GdiFont font, DWORD table, DWORD offset, LPVOID buf,
                         DWORD cbData)
{
    FT_Face ft_face = font->ft_face;
    DWORD len;
    FT_Error err;

    TRACE("font=%p, table=%08lx, offset=%08lx, buf=%p, cbData=%lx\n",
        font, table, offset, buf, cbData);

    if(!FT_IS_SFNT(ft_face))
        return GDI_ERROR;

    if(table) { /* MS tags differ in endidness from FT ones */
        table = table >> 24 | table << 24 |
          (table >> 8 & 0xff00) | (table << 8 & 0xff0000);
    }

    if(!buf || !cbData)
        len = 0;
    else
        len = cbData;

#if defined( HAVE_FT_LOAD_SFNT_TABLE )
    if (pFT_Load_Sfnt_Table)
    {
        err = pFT_Load_Sfnt_Table(ft_face, table, offset, buf, &len);
    }
    else
#endif
    { /* older freetypes that don't support FT_Load_Sfnt_Table */
#if defined( HAVE_FREETYPE_INTERNAL_SFNT_H )
        TT_Face tt_face;
        SFNT_Interface *sfnt;
        WARN("old freetype, using private freetype data\n");
        tt_face = (TT_Face) ft_face;
        sfnt = (SFNT_Interface*)tt_face->sfnt;

        err = sfnt->load_any(tt_face, table, offset, buf, &len);
#else
        err = FT_Err_Unimplemented_Feature;
#endif
    }
    if(err) {
        TRACE("Can't find table %08lx.\n", table);
        return GDI_ERROR;
    }
    return len;
}

#else /* HAVE_FREETYPE */

BOOL WineEngInit(void)
{
    ERR("FreeType support is not compiled in to wine, some font functionality"
        " will be disabled.\n");
    return FALSE;
}

void WineEngFinalize(void)
{
}

INT WineEngAddFontResourceEx(LPCWSTR str, DWORD fl, PVOID pv)
{
    ERR("can't without FreeType support!\n");
    return 0;
}

BOOL WineEngRemoveFontResourceEx(LPCWSTR str, DWORD fl, PVOID pv)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

GdiFont WineEngCreateFontInstance(DC *dc, HFONT hfont)
{
    WARN("Called but no FreeType support!\n");
    return NULL;
}

BOOL WineEngDestroyFontInstance(HFONT handle)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

DWORD WineEngEnumFonts(LPLOGFONTW plf, DEVICEFONTENUMPROC proc,
                       LPARAM lparam)
{
    WARN("Called but no FreeType support!\n");
    return GDI_ERROR;
}

DWORD WineEngGetGlyphOutline(GdiFont font, UINT glyph, UINT format,
                             LPGLYPHMETRICS lpgm, DWORD buflen, LPVOID buf,
                             const MAT2* lpmat)
{
    WARN("Called but no FreeType support!\n");
    return GDI_ERROR;
}

BOOL WineEngGetTextMetrics(GdiFont font, LPTEXTMETRICW ptm)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

UINT WineEngGetOutlineTextMetrics(GdiFont font, UINT cbSize,
                                  OUTLINETEXTMETRICW *potm)
{
    WARN("Called but no FreeType support!\n");
    return 0;
}

BOOL WineEngGetCharABCWidth(GdiFont font, UINT firstChar,
                            UINT lastChar, LPABC abc)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

BOOL WineEngGetCharWidthI(GdiFont font, UINT first,
                          UINT count, LPWORD pgi, LPINT lpBuffer)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

BOOL WineEngGetCharWidth(GdiFont font, UINT firstChar, UINT lastChar,
                         LPINT buffer)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

BOOL WineEngGetTextExtentPoint(GdiFont font, LPCWSTR wstr, INT count,
                               LPSIZE size)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

DWORD WineEngGetFontData(GdiFont font, DWORD table, DWORD offset, LPVOID buf,
                         DWORD cbData)
{
}

DWORD WineEngGetGlyphIndices(GdiFont font, LPCWSTR lpstr, INT count,
                             LPWORD pgi, DWORD flags)
{
    WARN("Called but no FreeType support!\n");
    return 0;
}

BOOL WineEngGetTextExtentPointI(GdiFont font, LPWORD pgi, INT count,
                                LPSIZE size)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}

BOOL WineEngGetCharABCWidthI(GdiFont font, UINT first,
                             UINT count, LPWORD pgi, LPABC abc)
{
    WARN("Called but no FreeType support!\n");
    return FALSE;
}


#warning Compiling without FreeType support. FreeType is now loaded \
         dynamically, so there is no reason to do this!


#endif /* HAVE_FREETYPE */
