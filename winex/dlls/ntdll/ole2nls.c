/*
 *	National Language Support library
 *
 *	Copyright 1995	Martin von Loewis
 *      Copyright 1998  David Lee Lambert
 *      Copyright 2000  Julio C�sar G�zquez
 */

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <locale.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/unicode.h"
#include "options.h"
#include "winver.h"
#include "winnls.h"
#include "winreg.h"
#include "winerror.h"
#include "wine/debug.h"
#include "heap.h"


WINE_DEFAULT_DEBUG_CHANNEL(string);

static INT OLE_GetFormatW(LCID locale, DWORD flags, DWORD tflags,
			  const SYSTEMTIME * xtime, LPCWSTR _format,
			  LPWSTR date, INT datelen);

/* Locale name to id map. used by EnumSystemLocales, GetLocaleInfoA
 * MUST contain all #defines from winnls.h
 * last entry has NULL name, 0 id.
 */
#define LOCALE_ENTRY(x)	{#x,LOCALE_##x}
static const struct tagLOCALE_NAME2ID {
    const char	*name;
    LCTYPE	id;
} locale_name2id[]= {
	LOCALE_ENTRY(ILANGUAGE),
	LOCALE_ENTRY(SLANGUAGE),
	LOCALE_ENTRY(SENGLANGUAGE),
	LOCALE_ENTRY(SABBREVLANGNAME),
	LOCALE_ENTRY(SNATIVELANGNAME),
	LOCALE_ENTRY(ICOUNTRY),
	LOCALE_ENTRY(SCOUNTRY),
	LOCALE_ENTRY(SENGCOUNTRY),
	LOCALE_ENTRY(SABBREVCTRYNAME),
	LOCALE_ENTRY(SNATIVECTRYNAME),
	LOCALE_ENTRY(IDEFAULTLANGUAGE),
	LOCALE_ENTRY(IDEFAULTCOUNTRY),
	LOCALE_ENTRY(IDEFAULTCODEPAGE),
	LOCALE_ENTRY(IDEFAULTANSICODEPAGE),
	LOCALE_ENTRY(IDEFAULTMACCODEPAGE),
	LOCALE_ENTRY(SLIST),
	LOCALE_ENTRY(IMEASURE),
	LOCALE_ENTRY(SDECIMAL),
	LOCALE_ENTRY(STHOUSAND),
	LOCALE_ENTRY(SGROUPING),
	LOCALE_ENTRY(IDIGITS),
	LOCALE_ENTRY(ILZERO),
	LOCALE_ENTRY(INEGNUMBER),
	LOCALE_ENTRY(SNATIVEDIGITS),
	LOCALE_ENTRY(SCURRENCY),
	LOCALE_ENTRY(SINTLSYMBOL),
	LOCALE_ENTRY(SMONDECIMALSEP),
	LOCALE_ENTRY(SMONTHOUSANDSEP),
	LOCALE_ENTRY(SMONGROUPING),
	LOCALE_ENTRY(ICURRDIGITS),
	LOCALE_ENTRY(IINTLCURRDIGITS),
	LOCALE_ENTRY(ICURRENCY),
	LOCALE_ENTRY(INEGCURR),
	LOCALE_ENTRY(SDATE),
	LOCALE_ENTRY(STIME),
	LOCALE_ENTRY(SSHORTDATE),
	LOCALE_ENTRY(SLONGDATE),
	LOCALE_ENTRY(STIMEFORMAT),
	LOCALE_ENTRY(IDATE),
	LOCALE_ENTRY(ILDATE),
	LOCALE_ENTRY(ITIME),
	LOCALE_ENTRY(ITIMEMARKPOSN),
	LOCALE_ENTRY(ICENTURY),
	LOCALE_ENTRY(ITLZERO),
	LOCALE_ENTRY(IDAYLZERO),
	LOCALE_ENTRY(IMONLZERO),
	LOCALE_ENTRY(S1159),
	LOCALE_ENTRY(S2359),
	LOCALE_ENTRY(ICALENDARTYPE),
	LOCALE_ENTRY(IOPTIONALCALENDAR),
	LOCALE_ENTRY(IFIRSTDAYOFWEEK),
	LOCALE_ENTRY(IFIRSTWEEKOFYEAR),
	LOCALE_ENTRY(SDAYNAME1),
	LOCALE_ENTRY(SDAYNAME2),
	LOCALE_ENTRY(SDAYNAME3),
	LOCALE_ENTRY(SDAYNAME4),
	LOCALE_ENTRY(SDAYNAME5),
	LOCALE_ENTRY(SDAYNAME6),
	LOCALE_ENTRY(SDAYNAME7),
	LOCALE_ENTRY(SABBREVDAYNAME1),
	LOCALE_ENTRY(SABBREVDAYNAME2),
	LOCALE_ENTRY(SABBREVDAYNAME3),
	LOCALE_ENTRY(SABBREVDAYNAME4),
	LOCALE_ENTRY(SABBREVDAYNAME5),
	LOCALE_ENTRY(SABBREVDAYNAME6),
	LOCALE_ENTRY(SABBREVDAYNAME7),
	LOCALE_ENTRY(SMONTHNAME1),
	LOCALE_ENTRY(SMONTHNAME2),
	LOCALE_ENTRY(SMONTHNAME3),
	LOCALE_ENTRY(SMONTHNAME4),
	LOCALE_ENTRY(SMONTHNAME5),
	LOCALE_ENTRY(SMONTHNAME6),
	LOCALE_ENTRY(SMONTHNAME7),
	LOCALE_ENTRY(SMONTHNAME8),
	LOCALE_ENTRY(SMONTHNAME9),
	LOCALE_ENTRY(SMONTHNAME10),
	LOCALE_ENTRY(SMONTHNAME11),
	LOCALE_ENTRY(SMONTHNAME12),
	LOCALE_ENTRY(SMONTHNAME13),
	LOCALE_ENTRY(SABBREVMONTHNAME1),
	LOCALE_ENTRY(SABBREVMONTHNAME2),
	LOCALE_ENTRY(SABBREVMONTHNAME3),
	LOCALE_ENTRY(SABBREVMONTHNAME4),
	LOCALE_ENTRY(SABBREVMONTHNAME5),
	LOCALE_ENTRY(SABBREVMONTHNAME6),
	LOCALE_ENTRY(SABBREVMONTHNAME7),
	LOCALE_ENTRY(SABBREVMONTHNAME8),
	LOCALE_ENTRY(SABBREVMONTHNAME9),
	LOCALE_ENTRY(SABBREVMONTHNAME10),
	LOCALE_ENTRY(SABBREVMONTHNAME11),
	LOCALE_ENTRY(SABBREVMONTHNAME12),
	LOCALE_ENTRY(SABBREVMONTHNAME13),
	LOCALE_ENTRY(SPOSITIVESIGN),
	LOCALE_ENTRY(SNEGATIVESIGN),
	LOCALE_ENTRY(IPOSSIGNPOSN),
	LOCALE_ENTRY(INEGSIGNPOSN),
	LOCALE_ENTRY(IPOSSYMPRECEDES),
	LOCALE_ENTRY(IPOSSEPBYSPACE),
	LOCALE_ENTRY(INEGSYMPRECEDES),
	LOCALE_ENTRY(INEGSEPBYSPACE),
	LOCALE_ENTRY(FONTSIGNATURE),
	LOCALE_ENTRY(SISO639LANGNAME),
	LOCALE_ENTRY(SISO3166CTRYNAME),
	{NULL,0}
};

static char *GetLocaleSubkeyName( DWORD lctype );

/***********************************************************************
 *		GetUserDefaultLCID (KERNEL32.@)
 */
LCID WINAPI GetUserDefaultLCID(void)
{
	return MAKELCID( GetUserDefaultLangID() , SORT_DEFAULT );
}

/***********************************************************************
 *		GetSystemDefaultLCID (KERNEL32.@)
 */
LCID WINAPI GetSystemDefaultLCID(void)
{
	return GetUserDefaultLCID();
}

#define NLS_MAX_LANGUAGES 20
typedef struct {
    char lang[128];
    char country[128];
    LANGID found_lang_id[NLS_MAX_LANGUAGES];
    char found_language[NLS_MAX_LANGUAGES][3];
    char found_country[NLS_MAX_LANGUAGES][3];
    int n_found;
} LANG_FIND_DATA;

static BOOL CALLBACK NLS_FindLanguageID_ProcA(HMODULE hModule, LPCSTR type,
                                              LPCSTR name, WORD LangID, LONG lParam)
{
    LANG_FIND_DATA *l_data = (LANG_FIND_DATA *)lParam;
    LCID lcid = MAKELCID(LangID, SORT_DEFAULT);
    char buf_language[128];
    char buf_country[128];
    char buf_en_language[128];

    TRACE("%04X\n", (UINT)LangID);
    if(PRIMARYLANGID(LangID) == LANG_NEUTRAL)
        return TRUE; /* continue search */

    buf_language[0] = 0;
    buf_country[0] = 0;

    GetLocaleInfoA(lcid, LOCALE_SISO639LANGNAME|LOCALE_NOUSEROVERRIDE,
                   buf_language, sizeof(buf_language));
    TRACE("LOCALE_SISO639LANGNAME: %s\n", buf_language);

    GetLocaleInfoA(lcid, LOCALE_SISO3166CTRYNAME|LOCALE_NOUSEROVERRIDE,
                   buf_country, sizeof(buf_country));
    TRACE("LOCALE_SISO3166CTRYNAME: %s\n", buf_country);

    if(l_data->lang && strlen(l_data->lang) > 0 && !strcasecmp(l_data->lang, buf_language))
    {
	if(l_data->country && strlen(l_data->country) > 0)
	{
	    if(!strcasecmp(l_data->country, buf_country))
	    {
		l_data->found_lang_id[0] = LangID;
		l_data->n_found = 1;
		TRACE("Found lang_id %04X for %s_%s\n", LangID, l_data->lang, l_data->country);
		return FALSE; /* stop enumeration */
	    }
	}
	else /* l_data->country not specified */
	{
	    if(l_data->n_found < NLS_MAX_LANGUAGES)
	    {
		l_data->found_lang_id[l_data->n_found] = LangID;
		strncpy(l_data->found_country[l_data->n_found], buf_country, 3);
		strncpy(l_data->found_language[l_data->n_found], buf_language, 3);
		l_data->n_found++;
		TRACE("Found lang_id %04X for %s\n", LangID, l_data->lang);
		return TRUE; /* continue search */
	    }
	}
    }

    /* Just in case, check LOCALE_SENGLANGUAGE too,
     * in hope that possible alias name might have that value.
     */
    buf_en_language[0] = 0;
    GetLocaleInfoA(lcid, LOCALE_SENGLANGUAGE|LOCALE_NOUSEROVERRIDE,
                   buf_en_language, sizeof(buf_en_language));
    TRACE("LOCALE_SENGLANGUAGE: %s\n", buf_en_language);

    if(l_data->lang && strlen(l_data->lang) > 0 && !strcasecmp(l_data->lang, buf_en_language))
    {
	l_data->found_lang_id[l_data->n_found] = LangID;
	strncpy(l_data->found_country[l_data->n_found], buf_country, 3);
	strncpy(l_data->found_language[l_data->n_found], buf_language, 3);
	l_data->n_found++;
	TRACE("Found lang_id %04X for %s\n", LangID, l_data->lang);
    }

    return TRUE; /* continue search */
}

/***********************************************************************
 *           NLS_GetLanguageID
 *
 * INPUT:
 *	Lang: a string whose two first chars are the iso name of a language.
 *	Country: a string whose two first chars are the iso name of country
 *	Charset: a string defining the chossen charset encoding
 *	Dialect: a string defining a variation of the locale
 *
 *	all those values are from the standardized format of locale
 *	name in unix which is: Lang[_Country][.Charset][@Dialect]
 *
 * RETURNS:
 *	the numeric code of the language used by Windows
 *
 * FIXME: Charset and Dialect are not handled
 */
static LANGID NLS_GetLanguageID(LPCSTR Lang, LPCSTR Country, LPCSTR Charset, LPCSTR Dialect)
{
    LANG_FIND_DATA l_data;
    char lang_string[256];

    if(!Lang)
    {
	l_data.found_lang_id[0] = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT);
	goto END;
    }

    memset(&l_data, 0, sizeof(LANG_FIND_DATA));
    strncpy(l_data.lang, Lang, sizeof(l_data.lang));

    if(Country && strlen(Country) > 0)
	strncpy(l_data.country, Country, sizeof(l_data.country));

    EnumResourceLanguagesA(GetModuleHandleA("KERNEL32"), RT_STRINGA,
	(LPCSTR)LOCALE_ILANGUAGE, NLS_FindLanguageID_ProcA, (LONG)&l_data);

    strcpy(lang_string, l_data.lang);
    if(l_data.country && strlen(l_data.country) > 0)
    {
	strcat(lang_string, "_");
	strcat(lang_string, l_data.country);
    }

    if(!l_data.n_found)
    {
	if(l_data.country && strlen(l_data.country) > 0)
	{
	    MESSAGE("Warning: Language '%s' was not found, retrying without country name...\n", lang_string);
	    l_data.country[0] = 0;
	    EnumResourceLanguagesA(GetModuleHandleA("KERNEL32"), RT_STRINGA,
		(LPCSTR)LOCALE_ILANGUAGE, NLS_FindLanguageID_ProcA, (LONG)&l_data);
	}
    }

    /* Re-evaluate lang_string */
    strcpy(lang_string, l_data.lang);
    if(l_data.country && strlen(l_data.country) > 0)
    {
	strcat(lang_string, "_");
	strcat(lang_string, l_data.country);
    }

    if(!l_data.n_found)
    {
	MESSAGE("Warning: Language '%s' was not recognized, defaulting to English\n", lang_string);
	l_data.found_lang_id[0] = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT);
    }
    else
    {
	if(l_data.n_found == 1)
	    TRACE("For language '%s' lang_id %04X was found\n", lang_string, l_data.found_lang_id[0]);
	else /* l_data->n_found > 1 */
	{
	    int i;
	    MESSAGE("For language '%s' several language ids were found:\n", lang_string);
	    for(i = 0; i < l_data.n_found; i++)
		MESSAGE("%s_%s - %04X; ", l_data.found_language[i], l_data.found_country[i], l_data.found_lang_id[i]);

	    MESSAGE("\nInstead of using first in the list, suggest to define\n"
		    "your LANG environment variable like this: LANG=%s_%s\n",
		    l_data.found_language[0], l_data.found_country[0]);
	}
    }
END:
    TRACE("Returning %04X\n", l_data.found_lang_id[0]);
    return l_data.found_lang_id[0];
}

/***********************************************************************
 *		GetUserDefaultLangID (KERNEL32.@)
 */
LANGID WINAPI GetUserDefaultLangID(void)
{
	/* caching result, if defined from environment, which should (?) not change during a WINE session */
	static	LANGID	userLCID = 0;

	if (userLCID == 0)
        {
                char buf[256];
		char *lang,*country,*charset,*dialect,*next;

		if (GetEnvironmentVariableA( "LANGUAGE", buf, sizeof(buf) )) goto ok;
		if (GetEnvironmentVariableA( "LANG", buf, sizeof(buf) )) goto ok;
		if (GetEnvironmentVariableA( "LC_ALL", buf, sizeof(buf) )) goto ok;
		if (GetEnvironmentVariableA( "LC_MESSAGES", buf, sizeof(buf) )) goto ok;
		if (GetEnvironmentVariableA( "LC_CTYPE", buf, sizeof(buf) )) goto ok;

		return userLCID = MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT );

        ok:
		if (!strcmp(buf,"POSIX") || !strcmp(buf,"C"))
                    return userLCID = MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT );

		lang=buf;

		do {
			next=strchr(lang,':'); if (next) *next++='\0';
			dialect=strchr(lang,'@'); if (dialect) *dialect++='\0';
			charset=strchr(lang,'.'); if (charset) *charset++='\0';
			country=strchr(lang,'_'); if (country) *country++='\0';

			userLCID = NLS_GetLanguageID(lang, country, charset, dialect);

			lang=next;
		} while (lang && !userLCID);

		if (!userLCID)
                {
                    MESSAGE( "Warning: language '%s' not recognized, defaulting to English\n",
                             buf );
                    userLCID = MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT );
                }
	}
	return userLCID;
}

/***********************************************************************
 *		GetUserDefaultUILanguage (KERNEL32.@)
 */
LANGID WINAPI GetUserDefaultUILanguage(void)
{
	return GetUserDefaultLangID();
}

/***********************************************************************
 *		GetSystemDefaultLangID (KERNEL32.@)
 */
LANGID WINAPI GetSystemDefaultLangID(void)
{
	return GetUserDefaultLangID();
}

/***********************************************************************
 *		GetSystemDefaultUILanguage (KERNEL32.@)
 */
LANGID WINAPI GetSystemDefaultUILanguage(void)
{
	return GetUserDefaultLangID();
}

/******************************************************************************
 *		ConvertDefaultLocale (KERNEL32.@)
 */
LCID WINAPI ConvertDefaultLocale (LCID lcid)
{	switch (lcid)
	{  case LOCALE_SYSTEM_DEFAULT:
	     return GetSystemDefaultLCID();
	   case LOCALE_USER_DEFAULT:
	     return GetUserDefaultLCID();
	   case LOCALE_NEUTRAL:
	     return MAKELCID (LANG_NEUTRAL, SUBLANG_NEUTRAL);
	}
	return MAKELANGID( PRIMARYLANGID(lcid), SUBLANG_NEUTRAL);
}

/* Enhanced version of LoadStringW.
 * It takes LanguageId to find and load language dependant resources.
 * Resource is copied as is: "binary" strings are not truncated.
 * Return: length of resource + 1 to distinguish absent resources
 * from the resources with zero length.
 */
static INT NLS_LoadStringExW(HMODULE hModule, LANGID lang_id, UINT res_id, LPWSTR buffer, INT buflen)
{
    HRSRC hrsrc;
    HGLOBAL hmem;
    WCHAR *p;
    int string_num;
    int i;

    /* Replace SUBLANG_NEUTRAL by SUBLANG_DEFAULT */
    if(SUBLANGID(lang_id) == SUBLANG_NEUTRAL)
        lang_id = MAKELANGID(PRIMARYLANGID(lang_id), SUBLANG_DEFAULT);

    hrsrc = FindResourceExW(hModule, RT_STRINGW, (LPCWSTR)((res_id >> 4) + 1), lang_id);

    if(!hrsrc) return 0;
    hmem = LoadResource(hModule, hrsrc);
    if(!hmem) return 0;

    p = LockResource(hmem);
    string_num = res_id & 0x000f;
    for(i = 0; i < string_num; i++)
	p += *p + 1;

    TRACE("strlen = %d\n", (int)*p );

    if (buffer == NULL) return *p;
    i = min(buflen - 1, *p);
    if (i > 0) {
	memcpy(buffer, p + 1, i * sizeof (WCHAR));
	buffer[i] = (WCHAR) 0;
    } else {
	if (buflen > 1)
	    buffer[0] = (WCHAR) 0;
    }

    FreeResource(hmem);
    TRACE("%s loaded!\n", debugstr_w(buffer));
    return (i + 1);
}

/******************************************************************************
 *		GetLocaleInfoA (KERNEL32.@)
 *
 * NOTES
 *  LANG_NEUTRAL is equal to LOCALE_SYSTEM_DEFAULT
 *
 *  MS online documentation states that the string returned is NULL terminated
 *  except for LOCALE_FONTSIGNATURE  which "will return a non-NULL
 *  terminated string".
 */
INT WINAPI GetLocaleInfoA(LCID lcid,LCTYPE LCType,LPSTR buf,INT len)
{
    LPCSTR  retString = NULL;
    int	found = 0, i;
    char    *pacKey;
    char    acBuffer[128];
    DWORD   dwBufferSize=128;
    BOOL NoUserOverride;

  TRACE("(lcid=0x%lx,lctype=0x%lx,%p,%x)\n",lcid,LCType,buf,len);

  if (len && (! buf) ) {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}

	if (lcid == LOCALE_NEUTRAL || lcid == LANG_SYSTEM_DEFAULT)
	{
            lcid = GetSystemDefaultLCID();
	}
	else if (lcid == LANG_USER_DEFAULT) /*0x800*/
	{
            lcid = GetUserDefaultLCID();
	}

    /* LOCALE_NOUSEROVERRIDE means: do not get user redefined settings
       from the registry. Instead, use system default values. */
    NoUserOverride = (LCType & LOCALE_NOUSEROVERRIDE) != 0;

	LCType &= ~(LOCALE_NOUSEROVERRIDE|LOCALE_USE_CP_ACP);

    /* First, check if it's in the registry. */
    /* All user customized values are stored in the registry by SetLocaleInfo */
    if ( !NoUserOverride && (pacKey = GetLocaleSubkeyName(LCType)) )
    {
        char    acRealKey[128];
        HKEY    hKey;

        sprintf( acRealKey, "Control Panel\\International\\%s", pacKey );

        if ( RegOpenKeyExA( HKEY_CURRENT_USER, acRealKey,
                            0, KEY_READ, &hKey) == ERROR_SUCCESS )
        {
            if ( RegQueryValueExA( hKey, NULL, NULL, NULL, (LPBYTE)acBuffer,
                                   &dwBufferSize ) == ERROR_SUCCESS )
            {
                retString = acBuffer;
                found = 1;
            }
            RegCloseKey(hKey);
        }
    }

    /* If not in the registry, get it from the NLS entries. */
    if(!found) {
	WCHAR wcBuffer[128];
	int res_size;

	/* check if language is registered in the kernel32 resources */
	if((res_size = NLS_LoadStringExW(GetModuleHandleA("KERNEL32"), LANGIDFROMLCID(lcid),
		LCType, wcBuffer, sizeof(wcBuffer)/sizeof(wcBuffer[0])))) {
	    WideCharToMultiByte(CP_ACP, 0, wcBuffer, res_size, acBuffer, dwBufferSize, NULL, NULL);
	    retString = acBuffer;
	    found = 1;
	}
    }

    /* if not found report a most descriptive error */
    if(!found) {
	retString=0;
	/* If we are through all of this, retLen should not be zero anymore.
	   If it is, the value is not supported */
	i=0;
	while (locale_name2id[i].name!=NULL) {
	    if (LCType == locale_name2id[i].id) {
		retString = locale_name2id[i].name;
		break;
	    }
	    i++;
	}
	if(!retString)
	    FIXME("Unkown LC type %lX\n", LCType);
	else
	    FIXME("'%s' is not defined for your language (%04X).\n"
		"Please define it in dlls/kernel/nls/YourLanguage.nls\n"
		"and submit patch for inclusion into the next Wine release.\n",
			retString, LOWORD(lcid));
	SetLastError(ERROR_INVALID_PARAMETER);
	return 0;
    }

    /* a FONTSIGNATURE is not a string, just 6 DWORDs  */
    if (LCType == LOCALE_FONTSIGNATURE) {
        if (len) {
	    len = (len < sizeof(FONTSIGNATURE)) ? len : sizeof(FONTSIGNATURE);
            memcpy(buf, retString, len);
	    return len;
	}
        return sizeof(FONTSIGNATURE);
    }
    /* if len=0 return only the length, don't touch the buffer*/
    if (len) {
	lstrcpynA(buf,retString,len);
	return strlen(buf) + 1;
    }
    return strlen(retString)+1;
}

/******************************************************************************
 *		GetLocaleInfoW (KERNEL32.@)
 *
 * NOTES
 *  MS documentation states that len "specifies the size, in bytes (ANSI version)
 *  or characters (Unicode version), of" wbuf. Thus the number returned is
 *  the same between GetLocaleInfoW and GetLocaleInfoA.
 */
INT WINAPI GetLocaleInfoW(LCID lcid,LCTYPE LCType,LPWSTR wbuf,INT len)
{	WORD wlen;
	LPSTR abuf;

	if (len && (! wbuf) )
	{ SetLastError(ERROR_INSUFFICIENT_BUFFER);
	  return 0;
	}

	abuf = (LPSTR)HeapAlloc(GetProcessHeap(),0,len);
	wlen = GetLocaleInfoA(lcid, LCType, abuf, len);

	if (wlen && len)	/* if len=0 return only the length*/
            MultiByteToWideChar( CP_ACP, 0, abuf, -1, wbuf, len );

	HeapFree(GetProcessHeap(),0,abuf);
	return wlen;
}

/******************************************************************************
 *
 *		GetLocaleSubkeyName [helper function]
 *
 *          - For use with the registry.
 *          - Gets the registry subkey name for a given lctype.
 */
static char *GetLocaleSubkeyName( DWORD lctype )
{
    char    *pacKey=NULL;

    switch ( lctype )
    {
    /* These values are used by SetLocaleInfo and GetLocaleInfo, and
     * the values are stored in the registry, confirmed under Windows,
     * for the ones that actually assign pacKey. Cases that aren't finished
     * have not been confirmed, so that must be done before they can be
     * added.
     */
    case LOCALE_SDATE :        /* The date separator. */
        pacKey = "sDate";
        break;
    case LOCALE_ICURRDIGITS:
        pacKey = "iCurrDigits";
        break;
    case LOCALE_SDECIMAL :
        pacKey = "sDecimal";
        break;
    case LOCALE_ICURRENCY:
        pacKey = "iCurrency";
        break;
    case LOCALE_SGROUPING :
        pacKey = "sGrouping";
        break;
    case LOCALE_IDIGITS:
        pacKey = "iDigits";
        break;
    case LOCALE_SLIST :
        pacKey = "sList";
        break;
    /* case LOCALE_ICALENDARTYPE: */
    /* case LOCALE_IFIRSTDAYOFWEEK: */
    /* case LOCALE_IFIRSTWEEKOFYEAR: */
    /* case LOCALE_SYEARMONTH : */
    /* case LOCALE_SPOSITIVESIGN : */
    /* case LOCALE_IPAPERSIZE: */
    /*     break; */
    case LOCALE_SLONGDATE :
        pacKey = "sLongDate";
        break;
    case LOCALE_SMONDECIMALSEP :
        pacKey = "sMonDecimalSep";
        break;
    case LOCALE_SMONGROUPING:
        pacKey = "sMonGrouping";
        break;
    case LOCALE_IMEASURE:
        pacKey = "iMeasure";
        break;
    case LOCALE_SMONTHOUSANDSEP :
        pacKey = "sMonThousandSep";
        break;
    case LOCALE_INEGCURR:
        pacKey = "iNegCurr";
        break;
    case LOCALE_SNEGATIVESIGN :
        pacKey = "sNegativeSign";
        break;
    case LOCALE_INEGNUMBER:
        pacKey = "iNegNumber";
        break;
    case LOCALE_SSHORTDATE :
        pacKey = "sShortDate";
        break;
    case LOCALE_ILDATE:        /* Long Date format ordering specifier. */
        pacKey = "iLDate";
        break;
    case LOCALE_ILZERO:
        pacKey = "iLZero";
        break;
    case LOCALE_ITLZERO:
        pacKey = "iTLZero";
        break;
    case LOCALE_ITIME:        /* Time format specifier. */
        pacKey = "iTime";
        break;
    case LOCALE_STHOUSAND :
        pacKey = "sThousand";
        break;
    case LOCALE_S1159:        /* AM */
        pacKey = "s1159";
        break;
    case LOCALE_STIME:
        pacKey = "sTime";
        break;
    case LOCALE_S2359:        /* PM */
        pacKey = "s2359";
        break;
    case LOCALE_STIMEFORMAT :
        pacKey = "sTimeFormat";
        break;
    case LOCALE_SCURRENCY:
        pacKey = "sCurrency";
        break;

    /* The following are not listed under MSDN as supported,
     * but seem to be used and also stored in the registry.
     */

    case LOCALE_IDATE:
        pacKey = "iDate";
        break;
    case LOCALE_SCOUNTRY:
        pacKey = "sCountry";
        break;
    case LOCALE_ICOUNTRY:
        pacKey = "iCountry";
        break;
    case LOCALE_SLANGUAGE:
        pacKey = "sLanguage";
        break;

    default:
        break;
    }

    return( pacKey );
}

/******************************************************************************
 *		SetLocaleInfoA	[KERNEL32.@]
 */
BOOL WINAPI SetLocaleInfoA(LCID lcid, LCTYPE lctype, LPCSTR data)
{
    HKEY    hKey;
    char    *pacKey;
    char    acRealKey[128];

    if ( (pacKey = GetLocaleSubkeyName(lctype)) )
    {
        sprintf( acRealKey, "Control Panel\\International\\%s", pacKey );
        if ( RegCreateKeyA( HKEY_CURRENT_USER, acRealKey,
                               &hKey ) == ERROR_SUCCESS )
        {
            if ( RegSetValueExA( hKey, NULL, 0, REG_SZ, (BYTE *)data,
                                 strlen(data)+1 ) != ERROR_SUCCESS )
            {
                ERR("SetLocaleInfoA: %s did not work\n", pacKey );
            }
            RegCloseKey( hKey );
        }
    }
    else
    {
    FIXME("(%ld,%ld,%s): stub\n",lcid,lctype,data);
    }
    return TRUE;
}

/******************************************************************************
 *		IsValidLocale	[KERNEL32.@]
 */
BOOL WINAPI IsValidLocale(LCID lcid,DWORD flags)
{
    /* check if language is registered in the kernel32 resources */
    if(!FindResourceExW(GetModuleHandleA("KERNEL32"), RT_STRINGW, (LPCWSTR)LOCALE_ILANGUAGE, LOWORD(lcid)))
	return FALSE;
    else
	return TRUE;
}

static BOOL CALLBACK EnumResourceLanguagesProcW(HMODULE hModule, LPCWSTR type,
		LPCWSTR name, WORD LangID, LONG lParam)
{
    CHAR bufA[20];
    WCHAR bufW[20];
    LOCALE_ENUMPROCW lpfnLocaleEnum = (LOCALE_ENUMPROCW)lParam;
    sprintf(bufA, "%08X", (UINT)LangID);
    MultiByteToWideChar(CP_ACP, 0, bufA, -1, bufW, sizeof(bufW)/sizeof(bufW[0]));
    return lpfnLocaleEnum(bufW);
}

/******************************************************************************
 *		EnumSystemLocalesW	[KERNEL32.@]
 */
BOOL WINAPI EnumSystemLocalesW( LOCALE_ENUMPROCW lpfnLocaleEnum,
                                    DWORD flags )
{
    TRACE("(%p,%08lx)\n", lpfnLocaleEnum,flags);

    EnumResourceLanguagesW(GetModuleHandleA("KERNEL32"), RT_STRINGW,
	    (LPCWSTR)LOCALE_ILANGUAGE, EnumResourceLanguagesProcW,
	    (LONG)lpfnLocaleEnum);

    return TRUE;
}

static BOOL CALLBACK EnumResourceLanguagesProcA(HMODULE hModule, LPCSTR type,
		LPCSTR name, WORD LangID, LONG lParam)
{
    CHAR bufA[20];
    LOCALE_ENUMPROCA lpfnLocaleEnum = (LOCALE_ENUMPROCA)lParam;
    sprintf(bufA, "%08X", (UINT)LangID);
    return lpfnLocaleEnum(bufA);
}

/******************************************************************************
 *		EnumSystemLocalesA	[KERNEL32.@]
 */
BOOL WINAPI EnumSystemLocalesA(LOCALE_ENUMPROCA lpfnLocaleEnum,
                                   DWORD flags)
{
    TRACE("(%p,%08lx)\n", lpfnLocaleEnum,flags);

    EnumResourceLanguagesA(GetModuleHandleA("KERNEL32"), RT_STRINGA,
	    (LPCSTR)LOCALE_ILANGUAGE, EnumResourceLanguagesProcA,
	    (LONG)lpfnLocaleEnum);

    return TRUE;
}

/***********************************************************************
 *           VerLanguageNameA              [KERNEL32.@]
 */
DWORD WINAPI VerLanguageNameA( UINT wLang, LPSTR szLang, UINT nSize )
{
    if(!szLang)
	return 0;

    return GetLocaleInfoA(MAKELCID(wLang, SORT_DEFAULT), LOCALE_SENGLANGUAGE, szLang, nSize);
}

/***********************************************************************
 *           VerLanguageNameW              [KERNEL32.@]
 */
DWORD WINAPI VerLanguageNameW( UINT wLang, LPWSTR szLang, UINT nSize )
{
    if(!szLang)
	return 0;

    return GetLocaleInfoW(MAKELCID(wLang, SORT_DEFAULT), LOCALE_SENGLANGUAGE, szLang, nSize);
}


static const unsigned char LCM_Unicode_LUT[] = {
  6      ,   3, /*   -   1 */
  6      ,   4, /*   -   2 */
  6      ,   5, /*   -   3 */
  6      ,   6, /*   -   4 */
  6      ,   7, /*   -   5 */
  6      ,   8, /*   -   6 */
  6      ,   9, /*   -   7 */
  6      ,  10, /*   -   8 */
  7      ,   5, /*   -   9 */
  7      ,   6, /*   -  10 */
  7      ,   7, /*   -  11 */
  7      ,   8, /*   -  12 */
  7      ,   9, /*   -  13 */
  6      ,  11, /*   -  14 */
  6      ,  12, /*   -  15 */
  6      ,  13, /*   -  16 */
  6      ,  14, /*   -  17 */
  6      ,  15, /*   -  18 */
  6      ,  16, /*   -  19 */
  6      ,  17, /*   -  20 */
  6      ,  18, /*   -  21 */
  6      ,  19, /*   -  22 */
  6      ,  20, /*   -  23 */
  6      ,  21, /*   -  24 */
  6      ,  22, /*   -  25 */
  6      ,  23, /*   -  26 */
  6      ,  24, /*   -  27 */
  6      ,  25, /*   -  28 */
  6      ,  26, /*   -  29 */
  6      ,  27, /*   -  30 */
  6      ,  28, /*   -  31 */
  7      ,   2, /*   -  32 */
  7      ,  28, /* ! -  33 */
  7      ,  29, /* " -  34 */ /* " */
  7      ,  31, /* # -  35 */
  7      ,  33, /* $ -  36 */
  7      ,  35, /* % -  37 */
  7      ,  37, /* & -  38 */
  6      , 128, /* ' -  39 */
  7      ,  39, /* ( -  40 */
  7      ,  42, /* ) -  41 */
  7      ,  45, /* * -  42 */
  8      ,   3, /* + -  43 */
  7      ,  47, /* , -  44 */
  6      , 130, /* - -  45 */
  7      ,  51, /* . -  46 */
  7      ,  53, /* / -  47 */
 12      ,   3, /* 0 -  48 */
 12      ,  33, /* 1 -  49 */
 12      ,  51, /* 2 -  50 */
 12      ,  70, /* 3 -  51 */
 12      ,  88, /* 4 -  52 */
 12      , 106, /* 5 -  53 */
 12      , 125, /* 6 -  54 */
 12      , 144, /* 7 -  55 */
 12      , 162, /* 8 -  56 */
 12      , 180, /* 9 -  57 */
  7      ,  55, /* : -  58 */
  7      ,  58, /* ; -  59 */
  8      ,  14, /* < -  60 */
  8      ,  18, /* = -  61 */
  8      ,  20, /* > -  62 */
  7      ,  60, /* ? -  63 */
  7      ,  62, /* @ -  64 */
 14      ,   2, /* A -  65 */
 14      ,   9, /* B -  66 */
 14      ,  10, /* C -  67 */
 14      ,  26, /* D -  68 */
 14      ,  33, /* E -  69 */
 14      ,  35, /* F -  70 */
 14      ,  37, /* G -  71 */
 14      ,  44, /* H -  72 */
 14      ,  50, /* I -  73 */
 14      ,  53, /* J -  74 */
 14      ,  54, /* K -  75 */
 14      ,  72, /* L -  76 */
 14      ,  81, /* M -  77 */
 14      , 112, /* N -  78 */
 14      , 124, /* O -  79 */
 14      , 126, /* P -  80 */
 14      , 137, /* Q -  81 */
 14      , 138, /* R -  82 */
 14      , 145, /* S -  83 */
 14      , 153, /* T -  84 */
 14      , 159, /* U -  85 */
 14      , 162, /* V -  86 */
 14      , 164, /* W -  87 */
 14      , 166, /* X -  88 */
 14      , 167, /* Y -  89 */
 14      , 169, /* Z -  90 */
  7      ,  63, /* [ -  91 */
  7      ,  65, /* \ -  92 */
  7      ,  66, /* ] -  93 */
  7      ,  67, /* ^ -  94 */
  7      ,  68, /* _ -  95 */
  7      ,  72, /* ` -  96 */
 14      ,   2, /* a -  97 */
 14      ,   9, /* b -  98 */
 14      ,  10, /* c -  99 */
 14      ,  26, /* d - 100 */
 14      ,  33, /* e - 101 */
 14      ,  35, /* f - 102 */
 14      ,  37, /* g - 103 */
 14      ,  44, /* h - 104 */
 14      ,  50, /* i - 105 */
 14      ,  53, /* j - 106 */
 14      ,  54, /* k - 107 */
 14      ,  72, /* l - 108 */
 14      ,  81, /* m - 109 */
 14      , 112, /* n - 110 */
 14      , 124, /* o - 111 */
 14      , 126, /* p - 112 */
 14      , 137, /* q - 113 */
 14      , 138, /* r - 114 */
 14      , 145, /* s - 115 */
 14      , 153, /* t - 116 */
 14      , 159, /* u - 117 */
 14      , 162, /* v - 118 */
 14      , 164, /* w - 119 */
 14      , 166, /* x - 120 */
 14      , 167, /* y - 121 */
 14      , 169, /* z - 122 */
  7      ,  74, /* { - 123 */
  7      ,  76, /* | - 124 */
  7      ,  78, /* } - 125 */
  7      ,  80, /* ~ - 126 */
  6      ,  29, /*  - 127 */
  6      ,  30, /* � - 128 */
  6      ,  31, /* � - 129 */
  7      , 123, /* � - 130 */
 14      ,  35, /* � - 131 */
  7      , 127, /* � - 132 */
 10      ,  21, /* � - 133 */
 10      ,  15, /* � - 134 */
 10      ,  16, /* � - 135 */
  7      ,  67, /* � - 136 */
 10      ,  22, /* � - 137 */
 14      , 145, /* � - 138 */
  7      , 136, /* � - 139 */
 14 + 16 , 124, /* � - 140 */
  6      ,  43, /* � - 141 */
  6      ,  44, /* � - 142 */
  6      ,  45, /* � - 143 */
  6      ,  46, /* � - 144 */
  7      , 121, /* � - 145 */
  7      , 122, /* � - 146 */
  7      , 125, /* � - 147 */
  7      , 126, /* � - 148 */
 10      ,  17, /* � - 149 */
  6      , 137, /* � - 150 */
  6      , 139, /* � - 151 */
  7      ,  93, /* � - 152 */
 14      , 156, /* � - 153 */
 14      , 145, /* � - 154 */
  7      , 137, /* � - 155 */
 14 + 16 , 124, /* � - 156 */
  6      ,  59, /* � - 157 */
  6      ,  60, /* � - 158 */
 14      , 167, /* � - 159 */
  7      ,   4, /* � - 160 */
  7      ,  81, /* � - 161 */
 10      ,   2, /* � - 162 */
 10      ,   3, /* � - 163 */
 10      ,   4, /* � - 164 */
 10      ,   5, /* � - 165 */
  7      ,  82, /* � - 166 */
 10      ,   6, /* � - 167 */
  7      ,  83, /* � - 168 */
 10      ,   7, /* � - 169 */
 14      ,   2, /* � - 170 */
  8      ,  24, /* � - 171 */
 10      ,   8, /* � - 172 */
  6      , 131, /* � - 173 */
 10      ,   9, /* � - 174 */
  7      ,  84, /* � - 175 */
 10      ,  10, /* � - 176 */
  8      ,  23, /* � - 177 */
 12      ,  51, /* � - 178 */
 12      ,  70, /* � - 179 */
  7      ,  85, /* � - 180 */
 10      ,  11, /* � - 181 */
 10      ,  12, /* � - 182 */
 10      ,  13, /* � - 183 */
  7      ,  86, /* � - 184 */
 12      ,  33, /* � - 185 */
 14      , 124, /* � - 186 */
  8      ,  26, /* � - 187 */
 12      ,  21, /* � - 188 */
 12      ,  25, /* � - 189 */
 12      ,  29, /* � - 190 */
  7      ,  87, /* � - 191 */
 14      ,   2, /* � - 192 */
 14      ,   2, /* � - 193 */
 14      ,   2, /* � - 194 */
 14      ,   2, /* � - 195 */
 14      ,   2, /* � - 196 */
 14      ,   2, /* � - 197 */
 14 + 16 ,   2, /* � - 198 */
 14      ,  10, /* � - 199 */
 14      ,  33, /* � - 200 */
 14      ,  33, /* � - 201 */
 14      ,  33, /* � - 202 */
 14      ,  33, /* � - 203 */
 14      ,  50, /* � - 204 */
 14      ,  50, /* � - 205 */
 14      ,  50, /* � - 206 */
 14      ,  50, /* � - 207 */
 14      ,  26, /* � - 208 */
 14      , 112, /* � - 209 */
 14      , 124, /* � - 210 */
 14      , 124, /* � - 211 */
 14      , 124, /* � - 212 */
 14      , 124, /* � - 213 */
 14      , 124, /* � - 214 */
  8      ,  28, /* � - 215 */
 14      , 124, /* � - 216 */
 14      , 159, /* � - 217 */
 14      , 159, /* � - 218 */
 14      , 159, /* � - 219 */
 14      , 159, /* � - 220 */
 14      , 167, /* � - 221 */
 14 + 32 , 153, /* � - 222 */
 14 + 48 , 145, /* � - 223 */
 14      ,   2, /* � - 224 */
 14      ,   2, /* � - 225 */
 14      ,   2, /* � - 226 */
 14      ,   2, /* � - 227 */
 14      ,   2, /* � - 228 */
 14      ,   2, /* � - 229 */
 14 + 16 ,   2, /* � - 230 */
 14      ,  10, /* � - 231 */
 14      ,  33, /* � - 232 */
 14      ,  33, /* � - 233 */
 14      ,  33, /* � - 234 */
 14      ,  33, /* � - 235 */
 14      ,  50, /* � - 236 */
 14      ,  50, /* � - 237 */
 14      ,  50, /* � - 238 */
 14      ,  50, /* � - 239 */
 14      ,  26, /* � - 240 */
 14      , 112, /* � - 241 */
 14      , 124, /* � - 242 */
 14      , 124, /* � - 243 */
 14      , 124, /* � - 244 */
 14      , 124, /* � - 245 */
 14      , 124, /* � - 246 */
  8      ,  29, /* � - 247 */
 14      , 124, /* � - 248 */
 14      , 159, /* � - 249 */
 14      , 159, /* � - 250 */
 14      , 159, /* � - 251 */
 14      , 159, /* � - 252 */
 14      , 167, /* � - 253 */
 14 + 32 , 153, /* � - 254 */
 14      , 167  /* � - 255 */ };

static const unsigned char LCM_Unicode_LUT_2[] = { 33, 44, 145 };

#define LCM_Diacritic_Start 131

static const unsigned char LCM_Diacritic_LUT[] = {
123,  /* � - 131 */
  2,  /* � - 132 */
  2,  /* � - 133 */
  2,  /* � - 134 */
  2,  /* � - 135 */
  3,  /* � - 136 */
  2,  /* � - 137 */
 20,  /* � - 138 */
  2,  /* � - 139 */
  2,  /* � - 140 */
  2,  /* � - 141 */
  2,  /* � - 142 */
  2,  /* � - 143 */
  2,  /* � - 144 */
  2,  /* � - 145 */
  2,  /* � - 146 */
  2,  /* � - 147 */
  2,  /* � - 148 */
  2,  /* � - 149 */
  2,  /* � - 150 */
  2,  /* � - 151 */
  2,  /* � - 152 */
  2,  /* � - 153 */
 20,  /* � - 154 */
  2,  /* � - 155 */
  2,  /* � - 156 */
  2,  /* � - 157 */
  2,  /* � - 158 */
 19,  /* � - 159 */
  2,  /* � - 160 */
  2,  /* � - 161 */
  2,  /* � - 162 */
  2,  /* � - 163 */
  2,  /* � - 164 */
  2,  /* � - 165 */
  2,  /* � - 166 */
  2,  /* � - 167 */
  2,  /* � - 168 */
  2,  /* � - 169 */
  3,  /* � - 170 */
  2,  /* � - 171 */
  2,  /* � - 172 */
  2,  /* � - 173 */
  2,  /* � - 174 */
  2,  /* � - 175 */
  2,  /* � - 176 */
  2,  /* � - 177 */
  2,  /* � - 178 */
  2,  /* � - 179 */
  2,  /* � - 180 */
  2,  /* � - 181 */
  2,  /* � - 182 */
  2,  /* � - 183 */
  2,  /* � - 184 */
  2,  /* � - 185 */
  3,  /* � - 186 */
  2,  /* � - 187 */
  2,  /* � - 188 */
  2,  /* � - 189 */
  2,  /* � - 190 */
  2,  /* � - 191 */
 15,  /* � - 192 */
 14,  /* � - 193 */
 18,  /* � - 194 */
 25,  /* � - 195 */
 19,  /* � - 196 */
 26,  /* � - 197 */
  2,  /* � - 198 */
 28,  /* � - 199 */
 15,  /* � - 200 */
 14,  /* � - 201 */
 18,  /* � - 202 */
 19,  /* � - 203 */
 15,  /* � - 204 */
 14,  /* � - 205 */
 18,  /* � - 206 */
 19,  /* � - 207 */
104,  /* � - 208 */
 25,  /* � - 209 */
 15,  /* � - 210 */
 14,  /* � - 211 */
 18,  /* � - 212 */
 25,  /* � - 213 */
 19,  /* � - 214 */
  2,  /* � - 215 */
 33,  /* � - 216 */
 15,  /* � - 217 */
 14,  /* � - 218 */
 18,  /* � - 219 */
 19,  /* � - 220 */
 14,  /* � - 221 */
  2,  /* � - 222 */
  2,  /* � - 223 */
 15,  /* � - 224 */
 14,  /* � - 225 */
 18,  /* � - 226 */
 25,  /* � - 227 */
 19,  /* � - 228 */
 26,  /* � - 229 */
  2,  /* � - 230 */
 28,  /* � - 231 */
 15,  /* � - 232 */
 14,  /* � - 233 */
 18,  /* � - 234 */
 19,  /* � - 235 */
 15,  /* � - 236 */
 14,  /* � - 237 */
 18,  /* � - 238 */
 19,  /* � - 239 */
104,  /* � - 240 */
 25,  /* � - 241 */
 15,  /* � - 242 */
 14,  /* � - 243 */
 18,  /* � - 244 */
 25,  /* � - 245 */
 19,  /* � - 246 */
  2,  /* � - 247 */
 33,  /* � - 248 */
 15,  /* � - 249 */
 14,  /* � - 250 */
 18,  /* � - 251 */
 19,  /* � - 252 */
 14,  /* � - 253 */
  2,  /* � - 254 */
 19,  /* � - 255 */
} ;

/******************************************************************************
 * OLE2NLS_isPunctuation [INTERNAL]
 */
static int OLE2NLS_isPunctuation(unsigned char c)
{
  /* "punctuation character" in this context is a character which is
     considered "less important" during word sort comparison.
     See LCMapString implementation for the precise definition
     of "less important". */

  return (LCM_Unicode_LUT[-2+2*c]==6);
}

/******************************************************************************
 * OLE2NLS_isNonSpacing [INTERNAL]
 */
static int OLE2NLS_isNonSpacing(unsigned char c)
{
  /* This function is used by LCMapStringA.  Characters
     for which it returns true are ignored when mapping a
     string with NORM_IGNORENONSPACE */
  return ((c==136) || (c==170) || (c==186));
}

/******************************************************************************
 * OLE2NLS_isSymbol [INTERNAL]
 * FIXME: handle current locale
 */
static int OLE2NLS_isSymbol(unsigned char c)
{
  /* This function is used by LCMapStringA.  Characters
     for which it returns true are ignored when mapping a
     string with NORM_IGNORESYMBOLS */
  return ( (c!=0) && !(isalpha(c) || isdigit(c)) );
}

/******************************************************************************
 *		identity	[Internal]
 */
static int identity(int c)
{
  return c;
}

/*************************************************************************
 *              LCMapStringA                [KERNEL32.@]
 *
 * Convert a string, or generate a sort key from it.
 *
 * If (mapflags & LCMAP_SORTKEY), the function will generate
 * a sort key for the source string.  Else, it will convert it
 * accordingly to the flags LCMAP_UPPERCASE, LCMAP_LOWERCASE,...
 *
 * RETURNS
 *    Error : 0.
 *    Success : length of the result string.
 *
 * NOTES
 *    If called with scrlen = -1, the function will compute the length
 *      of the 0-terminated string strsrc by itself.
 *
 *    If called with dstlen = 0, returns the buffer length that
 *      would be required.
 *
 *    NORM_IGNOREWIDTH means to compare ASCII and wide characters
 *    as if they are equal.
 *    In the only code page implemented so far, there may not be
 *    wide characters in strings passed to LCMapStringA,
 *    so there is nothing to be done for this flag.
 */
INT WINAPI LCMapStringA(
	LCID lcid,      /* [in] locale identifier created with MAKELCID;
		                LOCALE_SYSTEM_DEFAULT and LOCALE_USER_DEFAULT are
                                predefined values. */
	DWORD mapflags, /* [in] flags */
	LPCSTR srcstr,  /* [in] source buffer */
	INT srclen,     /* [in] source length */
	LPSTR dststr,   /* [out] destination buffer */
	INT dstlen)     /* [in] destination buffer length */
{
  int i;

  TRACE("(0x%04lx,0x%08lx,%s,%d,%p,%d)\n",
	lcid,mapflags,debugstr_an(srcstr,srclen),srclen,dststr,dstlen);

  if ( ((dstlen!=0) && (dststr==NULL)) || (srcstr==NULL) )
  {
    ERR("(src=%s,dest=%s): Invalid NULL string\n",
	debugstr_an(srcstr,srclen), dststr);
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  if (srclen == -1)
    srclen = strlen(srcstr) + 1 ;    /* (include final '\0') */

#define LCMAPSTRINGA_SUPPORTED_FLAGS (LCMAP_UPPERCASE     | \
                                        LCMAP_LOWERCASE     | \
                                        LCMAP_SORTKEY       | \
                                        NORM_IGNORECASE     | \
                                        NORM_IGNORENONSPACE | \
                                        SORT_STRINGSORT     | \
                                        NORM_IGNOREWIDTH    | \
                                        NORM_IGNOREKANATYPE)
  /* FIXME: as long as we don't support Katakana nor Hiragana
   * characters, we can support NORM_IGNOREKANATYPE
   */
  if (mapflags & ~LCMAPSTRINGA_SUPPORTED_FLAGS)
  {
    FIXME("(0x%04lx,0x%08lx,%p,%d,%p,%d): "
	  "unimplemented flags: 0x%08lx\n",
	  lcid,
	  mapflags,
	  srcstr,
	  srclen,
	  dststr,
	  dstlen,
	  mapflags & ~LCMAPSTRINGA_SUPPORTED_FLAGS
     );
  }

  if ( !(mapflags & LCMAP_SORTKEY) )
  {
    int i,j;
    int (*f)(int) = identity;
    int flag_ignorenonspace = mapflags & NORM_IGNORENONSPACE;
    int flag_ignoresymbols = mapflags & NORM_IGNORESYMBOLS;

    if (flag_ignorenonspace || flag_ignoresymbols)
    {
      /* For some values of mapflags, the length of the resulting
	 string is not known at this point.  Windows does map the string
	 and does not SetLastError ERROR_INSUFFICIENT_BUFFER in
	 these cases. */
      if (dstlen==0)
      {
	/* Compute required length */
	for (i=j=0; i < srclen; i++)
	{
	  if ( !(flag_ignorenonspace && OLE2NLS_isNonSpacing(srcstr[i]))
	       && !(flag_ignoresymbols && OLE2NLS_isSymbol(srcstr[i])) )
	    j++;
	}
	return j;
      }
    }
    else
    {
      if (dstlen==0)
	return srclen;
      if (dstlen<srclen)
	   {
	     SetLastError(ERROR_INSUFFICIENT_BUFFER);
	     return 0;
	   }
    }
    if (mapflags & LCMAP_UPPERCASE)
      f = toupper;
    else if (mapflags & LCMAP_LOWERCASE)
      f = tolower;
    /* FIXME: NORM_IGNORENONSPACE requires another conversion */
    for (i=j=0; (i<srclen) && (j<dstlen) ; i++)
    {
      if ( !(flag_ignorenonspace && OLE2NLS_isNonSpacing(srcstr[i]))
	   && !(flag_ignoresymbols && OLE2NLS_isSymbol(srcstr[i])) )
      {
	dststr[j] = (CHAR) f(srcstr[i]);
	j++;
      }
    }
    return j;
  }

  /* FIXME: This function completely ignores the "lcid" parameter. */
  /* else ... (mapflags & LCMAP_SORTKEY)  */
  {
    int unicode_len=0;
    int case_len=0;
    int diacritic_len=0;
    int delayed_punctuation_len=0;
    char *case_component;
    char *diacritic_component;
    char *delayed_punctuation_component;
    int room,count;
    int flag_stringsort = mapflags & SORT_STRINGSORT;

    /* compute how much room we will need */
    for (i=0;i<srclen;i++)
    {
      int ofs;
      unsigned char source_char = srcstr[i];
      if (source_char!='\0')
      {
	if (flag_stringsort || !OLE2NLS_isPunctuation(source_char))
	{
	  unicode_len++;
	  if ( LCM_Unicode_LUT[-2+2*source_char] & ~15 )
	    unicode_len++;             /* double letter */
	}
	else
	{
	  delayed_punctuation_len++;
	}
      }

      if (isupper(source_char))
	case_len=unicode_len;

      ofs = source_char - LCM_Diacritic_Start;
      if ((ofs>=0) && (LCM_Diacritic_LUT[ofs]!=2))
	diacritic_len=unicode_len;
    }

    if (mapflags & NORM_IGNORECASE)
      case_len=0;
    if (mapflags & NORM_IGNORENONSPACE)
      diacritic_len=0;

    room =  2 * unicode_len              /* "unicode" component */
      +     diacritic_len                /* "diacritic" component */
      +     case_len                     /* "case" component */
      +     4 * delayed_punctuation_len  /* punctuation in word sort mode */
      +     4                            /* four '\1' separators */
      +     1  ;                         /* terminal '\0' */
    if (dstlen==0)
      return room;
    else if (dstlen<room)
    {
      SetLastError(ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
#if 0
    /*FIXME the Pointercheck should not be nessesary */
    if (IsBadWritePtr (dststr,room))
    { ERR("bad destination buffer (dststr) : %p,%d\n",dststr,dstlen);
      SetLastError(ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
#endif
    /* locate each component, write separators */
    diacritic_component = dststr + 2*unicode_len ;
    *diacritic_component++ = '\1';
    case_component = diacritic_component + diacritic_len ;
    *case_component++ = '\1';
    delayed_punctuation_component = case_component + case_len ;
    *delayed_punctuation_component++ = '\1';
    *delayed_punctuation_component++ = '\1';

    /* read source string char by char, write
       corresponding weight in each component. */
    for (i=0,count=0;i<srclen;i++)
    {
      unsigned char source_char=srcstr[i];
      if (source_char!='\0')
      {
	int type,longcode;
	type = LCM_Unicode_LUT[-2+2*source_char];
	longcode = type >> 4;
	type &= 15;
	if (!flag_stringsort && OLE2NLS_isPunctuation(source_char))
	{
	  WORD encrypted_location = (1<<15) + 7 + 4*count;
	  *delayed_punctuation_component++ = (unsigned char) (encrypted_location>>8);
	  *delayed_punctuation_component++ = (unsigned char) (encrypted_location&255);
                     /* big-endian is used here because it lets string comparison be
			compatible with numerical comparison */

	  *delayed_punctuation_component++ = type;
	  *delayed_punctuation_component++ = LCM_Unicode_LUT[-1+2*source_char];
                     /* assumption : a punctuation character is never a
			double or accented letter */
	}
	else
	{
	  dststr[2*count] = type;
	  dststr[2*count+1] = LCM_Unicode_LUT[-1+2*source_char];
	  if (longcode)
	  {
	    if (count<case_len)
	      case_component[count] = ( isupper(source_char) ? 18 : 2 ) ;
	    if (count<diacritic_len)
	      diacritic_component[count] = 2; /* assumption: a double letter
						 is never accented */
	    count++;

	    dststr[2*count] = type;
	    dststr[2*count+1] = *(LCM_Unicode_LUT_2 - 1 + longcode);
	    /* 16 in the first column of LCM_Unicode_LUT  -->  longcode = 1
	       32 in the first column of LCM_Unicode_LUT  -->  longcode = 2
	       48 in the first column of LCM_Unicode_LUT  -->  longcode = 3 */
	  }

	  if (count<case_len)
	    case_component[count] = ( isupper(source_char) ? 18 : 2 ) ;
	  if (count<diacritic_len)
	  {
	    int ofs = source_char - LCM_Diacritic_Start;
	    diacritic_component[count] = (ofs>=0 ? LCM_Diacritic_LUT[ofs] : 2);
	  }
	  count++;
	}
      }
    }
    dststr[room-1] = '\0';
    return room;
  }
}

/*************************************************************************
 *              LCMapStringW                [KERNEL32.@]
 *
 * Convert a string, or generate a sort key from it.
 *
 * NOTE
 *
 * See LCMapStringA for documentation
 */
INT WINAPI LCMapStringW(
	LCID lcid,DWORD mapflags,LPCWSTR srcstr,INT srclen,LPWSTR dststr,
	INT dstlen)
{
  int i;

  TRACE("(0x%04lx,0x%08lx,%p,%d,%p,%d)\n",
                 lcid, mapflags, srcstr, srclen, dststr, dstlen);

  if ( ((dstlen!=0) && (dststr==NULL)) || (srcstr==NULL) )
  {
    ERR("(src=%p,dst=%p): Invalid NULL string\n", srcstr, dststr);
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  if (srclen==-1)
    srclen = strlenW(srcstr)+1;

  /* FIXME: Both this function and it's companion LCMapStringA()
   * completely ignore the "lcid" parameter.  In place of the "lcid"
   * parameter the application must set the "LC_COLLATE" or "LC_ALL"
   * environment variable prior to invoking this function.  */
  if (mapflags & LCMAP_SORTKEY)
  {
      /* Possible values of LC_COLLATE. */
      char *lc_collate_default = 0; /* value prior to this function */
      char *lc_collate_env = 0;     /* value retrieved from the environment */

      /* General purpose index into strings of any type. */
      int str_idx = 0;

      /* Lengths of various strings where the length is measured in
       * wide characters for wide character strings and in bytes for
       * native strings.  The lengths include the NULL terminator.  */
      size_t returned_len    = 0;
      size_t src_native_len  = 0;
      size_t dst_native_len  = 0;
      size_t dststr_libc_len = 0;

      /* Native (character set determined by locale) versions of the
       * strings source and destination strings.  */
      LPSTR src_native = 0;
      LPSTR dst_native = 0;

      /* Version of the source and destination strings using the
       * "wchar_t" Unicode data type needed by various libc functions.  */
      wchar_t *srcstr_libc = 0;
      wchar_t *dststr_libc = 0;

      if(!(srcstr_libc = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                       srclen * sizeof(wchar_t))))
      {
          ERR("Unable to allocate %d bytes for srcstr_libc\n",
              srclen * sizeof(wchar_t));
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);
          return 0;
      }

      /* Convert source string to a libc Unicode string. */
      for(str_idx = 0; str_idx < srclen; str_idx++)
      {
          srcstr_libc[str_idx] = srcstr[str_idx];
      }

      /* src_native should contain at most 3 bytes for each
       * multibyte characters in the original srcstr string.  */
      src_native_len = 3 * srclen;
      if(!(src_native = (LPSTR)HeapAlloc(GetProcessHeap(), 0,
                                          src_native_len)))
      {
          ERR("Unable to allocate %d bytes for src_native\n", src_native_len);
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          return 0;
      }

      /* FIXME: Prior to to setting the LC_COLLATE locale category the
       * current value is backed up so it can be restored after the
       * last LC_COLLATE sensitive function returns.
       *
       * Even though the locale is adjusted for a minimum amount of
       * time a race condition exists where other threads may be
       * affected if they invoke LC_COLLATE sensitive functions.  One
       * possible solution is to wrap all LC_COLLATE sensitive Wine
       * functions, like LCMapStringW(), in a mutex.
       *
       * Another enhancement to the following would be to set the
       * LC_COLLATE locale category as a function of the "lcid"
       * parameter instead of the "LC_COLLATE" environment variable. */
      if(!(lc_collate_default = setlocale(LC_COLLATE, NULL)))
      {
          ERR("Unable to query the LC_COLLATE catagory\n");
          SetLastError(ERROR_INVALID_PARAMETER);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          return 0;
      }

      if(!(lc_collate_env = setlocale(LC_COLLATE, "")))
      {
          ERR("Unable to inherit the LC_COLLATE locale category from the "
              "environment.  The \"LC_COLLATE\" environment variable is "
              "\"%s\".\n", getenv("LC_COLLATE") ?
              getenv("LC_COLLATE") : "<unset>");
          SetLastError(ERROR_INVALID_PARAMETER);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          return 0;
      }

      TRACE("lc_collate_default = %s\n", lc_collate_default);
      TRACE("lc_collate_env = %s\n", lc_collate_env);

      /* Convert the libc Unicode string to a native multibyte character
       * string. */
      returned_len = wcstombs(src_native, srcstr_libc, src_native_len) + 1;
      if(returned_len == 0)
      {
          ERR("wcstombs failed.  The string specified (%s) may contain an invalid character.\n",
              debugstr_w(srcstr));
          SetLastError(ERROR_INVALID_PARAMETER);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          setlocale(LC_COLLATE, lc_collate_default);
          return 0;
      }
      else if(returned_len > src_native_len)
      {
          src_native[src_native_len - 1] = 0;
          ERR("wcstombs returned a string (%s) that was longer (%d bytes) "
              "than expected (%d bytes).\n", src_native, returned_len,
              dst_native_len);

          /* Since this is an internal error I'm not sure what the correct
           * error code is.  */
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);

          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          setlocale(LC_COLLATE, lc_collate_default);
          return 0;
      }
      src_native_len = returned_len;

      TRACE("src_native = %s  src_native_len = %d\n",
             src_native, src_native_len);

      /* dst_native seems to contain at most 4 bytes for each byte in
       * the original src_native string.  Change if need be since this
       * isn't documented by the strxfrm() man page. */
      dst_native_len = 4 * src_native_len;
      if(!(dst_native = (LPSTR)HeapAlloc(GetProcessHeap(), 0, dst_native_len)))
      {
          ERR("Unable to allocate %d bytes for dst_native\n", dst_native_len);
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          setlocale(LC_COLLATE, lc_collate_default);
          return 0;
      }

      /* The actual translation is done by the following call to
       * strxfrm().  The surrounding code could have been simplified
       * by calling wcsxfrm() instead except that wcsxfrm() is not
       * available on older Linux systems (RedHat 4.1 with
       * libc-5.3.12-17).
       *
       * Also, it is possible that the translation could be done by
       * various tables as it is done in LCMapStringA().  However, I'm
       * not sure what those tables are. */
      returned_len = strxfrm(dst_native, src_native, dst_native_len) + 1;

      if(returned_len > dst_native_len)
      {
          dst_native[dst_native_len - 1] = 0;
          ERR("strxfrm returned a string (%s) that was longer (%d bytes) "
              "than expected (%d bytes).\n", dst_native, returned_len,
              dst_native_len);

          /* Since this is an internal error I'm not sure what the correct
           * error code is.  */
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);

          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          if(dst_native) HeapFree(GetProcessHeap(), 0, dst_native);
          setlocale(LC_COLLATE, lc_collate_default);
          return 0;
      }
      dst_native_len = returned_len;

      TRACE("dst_native = %s  dst_native_len = %d\n",
             dst_native, dst_native_len);

      dststr_libc_len = dst_native_len;
      if(!(dststr_libc = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                       dststr_libc_len * sizeof(wchar_t))))
      {
          ERR("Unable to allocate %d bytes for dststr_libc\n",
              dststr_libc_len * sizeof(wchar_t));
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          if(dst_native) HeapFree(GetProcessHeap(), 0, dst_native);
          setlocale(LC_COLLATE, lc_collate_default);
          return 0;
      }

      /* Convert the native multibyte string to a libc Unicode string. */
      returned_len = mbstowcs(dststr_libc, dst_native, dst_native_len) + 1;

      /* Restore LC_COLLATE now that the last LC_COLLATE sensitive
       * function has returned. */
      setlocale(LC_COLLATE, lc_collate_default);

      if(returned_len == 0)
      {
          ERR("mbstowcs failed.  The native version of the translated string "
              "(%s) may contain an invalid character.\n", dst_native);
          SetLastError(ERROR_INVALID_PARAMETER);
          if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
          if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
          if(dst_native) HeapFree(GetProcessHeap(), 0, dst_native);
          if(dststr_libc) HeapFree(GetProcessHeap(), 0, dststr_libc);
          return 0;
      }
      if(dstlen)
      {
          if(returned_len > dstlen)
          {
              ERR("mbstowcs returned a string that was longer (%d chars) "
                  "than the buffer provided (%d chars).\n", returned_len,
                  dstlen);
              SetLastError(ERROR_INSUFFICIENT_BUFFER);
              if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
              if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
              if(dst_native) HeapFree(GetProcessHeap(), 0, dst_native);
              if(dststr_libc) HeapFree(GetProcessHeap(), 0, dststr_libc);
              return 0;
          }
          dstlen = returned_len;

          /* Convert a libc Unicode string to the destination string. */
          for(str_idx = 0; str_idx < dstlen; str_idx++)
          {
              dststr[str_idx] = dststr_libc[str_idx];
          }
          TRACE("1st 4 int sized chunks of dststr = %x %x %x %x\n",
                         *(((int *)dststr) + 0),
                         *(((int *)dststr) + 1),
                         *(((int *)dststr) + 2),
                         *(((int *)dststr) + 3));
      }
      else
      {
          dstlen = returned_len;
      }
      TRACE("dstlen (return) = %d\n", dstlen);
      if(srcstr_libc) HeapFree(GetProcessHeap(), 0, srcstr_libc);
      if(src_native) HeapFree(GetProcessHeap(), 0, src_native);
      if(dst_native) HeapFree(GetProcessHeap(), 0, dst_native);
      if(dststr_libc) HeapFree(GetProcessHeap(), 0, dststr_libc);
      return dstlen;
  }
  else
  {
    int (*f)(int)=identity;

    if (dstlen==0)
        return srclen;
    if (dstlen<srclen)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    if (mapflags & LCMAP_UPPERCASE)
      f = toupper;
    else if (mapflags & LCMAP_LOWERCASE)
      f = tolower;
    for (i=0; i < srclen; i++)
      dststr[i] = (WCHAR) f(srcstr[i]);
    return srclen;
  }
}


/***********************************************************************
 *           OLE2NLS_EstimateMappingLength
 *
 * Estimates the number of characters required to hold the string
 * computed by LCMapStringA.
 *
 * The size is always over-estimated, with a fixed limit on the
 * amount of estimation error.
 *
 * Note that len == -1 is not permitted.
 */
static inline int OLE2NLS_EstimateMappingLength(LCID lcid, DWORD dwMapFlags,
						LPCSTR str, DWORD len)
{
    /* Estimate only for small strings to keep the estimation error from
     * becoming too large. */
    if (len < 128) return len * 8 + 5;
    else return LCMapStringA(lcid, dwMapFlags, str, len, NULL, 0);
}

/******************************************************************************
 *		CompareStringA	[KERNEL32.@]
 * Compares two strings using locale
 *
 * RETURNS
 *
 * success: CSTR_LESS_THAN, CSTR_EQUAL, CSTR_GREATER_THAN
 * failure: 0
 *
 * NOTES
 *
 * Defaults to a word sort, but uses a string sort if
 * SORT_STRINGSORT is set.
 * Calls SetLastError for ERROR_INVALID_FLAGS, ERROR_INVALID_PARAMETER.
 *
 * BUGS
 *
 * This implementation ignores the locale
 *
 * FIXME
 *
 * Quite inefficient.
 */
int WINAPI CompareStringA(
    LCID lcid,      /* [in] locale ID */
    DWORD fdwStyle, /* [in] comparison-style options */
    LPCSTR s1,      /* [in] first string */
    int l1,         /* [in] length of first string */
    LPCSTR s2,      /* [in] second string */
    int l2)         /* [in] length of second string */
{
  int mapstring_flags;
  int len1,len2;
  int result;
  LPSTR sk1,sk2;
  TRACE("%s and %s\n",
	debugstr_an (s1,l1), debugstr_an (s2,l2));

  if ( (s1==NULL) || (s2==NULL) )
  {
    ERR("(s1=%s,s2=%s): Invalid NULL string\n",
	debugstr_an(s1,l1), debugstr_an(s2,l2));
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }

  if(fdwStyle & NORM_IGNORESYMBOLS)
    FIXME("IGNORESYMBOLS not supported\n");

  if (l1 == -1) l1 = strlen(s1);
  if (l2 == -1) l2 = strlen(s2);

  mapstring_flags = LCMAP_SORTKEY | fdwStyle ;
  len1 = OLE2NLS_EstimateMappingLength(lcid, mapstring_flags, s1, l1);
  len2 = OLE2NLS_EstimateMappingLength(lcid, mapstring_flags, s2, l2);

  if ((len1==0)||(len2==0))
    return 0;     /* something wrong happened */

  sk1 = (LPSTR)HeapAlloc(GetProcessHeap(), 0, len1 + len2);
  sk2 = sk1 + len1;
  if ( (!LCMapStringA(lcid,mapstring_flags,s1,l1,sk1,len1))
	 || (!LCMapStringA(lcid,mapstring_flags,s2,l2,sk2,len2)) )
  {
    ERR("Bug in LCmapStringA.\n");
    result = 0;
  }
  else
  {
    /* strcmp doesn't necessarily return -1, 0, or 1 */
    result = strcmp(sk1,sk2);
  }
  HeapFree(GetProcessHeap(),0,sk1);

  if (result < 0)
    return 1;
  if (result == 0)
    return 2;

  /* must be greater, if we reach this point */
  return 3;
}

/******************************************************************************
 *		CompareStringW	[KERNEL32.@]
 * This implementation ignores the locale
 * FIXME :  Does only string sort.  Should
 * be reimplemented the same way as CompareStringA.
 */
int WINAPI CompareStringW(LCID lcid, DWORD fdwStyle,
                          LPCWSTR s1, int l1, LPCWSTR s2, int l2)
{
	int len,ret;
	if(fdwStyle & NORM_IGNORENONSPACE)
		FIXME("IGNORENONSPACE not supported\n");
	if(fdwStyle & NORM_IGNORESYMBOLS)
		FIXME("IGNORESYMBOLS not supported\n");

	/* Is strcmp defaulting to string sort or to word sort?? */
	/* FIXME: Handle NORM_STRINGSORT */
	l1 = (l1==-1)?strlenW(s1):l1;
	l2 = (l2==-1)?strlenW(s2):l2;
	len = l1<l2 ? l1:l2;
	ret = (fdwStyle & NORM_IGNORECASE) ? strncmpiW(s1,s2,len) : strncmpW(s1,s2,len);
	/* not equal, return 1 or 3 */
	if(ret!=0) {
		/* need to translate result */
		return ((int)ret < 0) ? 1 : 3;
	}
	/* same len, return 2 */
	if(l1==l2) return 2;
	/* the longer one is lexically greater */
	return (l1<l2)? 1 : 3;
}

/******************************************************************************
 *		OLE_GetFormatA	[Internal]
 *
 * FIXME
 *    If datelen == 0, it should return the reguired string length.
 *
 This function implements stuff for GetDateFormat() and
 GetTimeFormat().

  d    single-digit (no leading zero) day (of month)
  dd   two-digit day (of month)
  ddd  short day-of-week name
  dddd long day-of-week name
  M    single-digit month
  MM   two-digit month
  MMM  short month name
  MMMM full month name
  y    two-digit year, no leading 0
  yy   two-digit year
  yyyy four-digit year
  gg   era string
  h    hours with no leading zero (12-hour)
  hh   hours with full two digits
  H    hours with no leading zero (24-hour)
  HH   hours with full two digits
  m    minutes with no leading zero
  mm   minutes with full two digits
  s    seconds with no leading zero
  ss   seconds with full two digits
  t    time marker (A or P)
  tt   time marker (AM, PM)
  ''   used to quote literal characters
  ''   (within a quoted string) indicates a literal '

 These functions REQUIRE valid locale, date,  and format.
 */
static INT OLE_GetFormatA(LCID locale,
			    DWORD flags,
			    DWORD tflags,
			    const SYSTEMTIME* xtime,
			    LPCSTR _format, 	/*in*/
			    LPSTR date,		/*out*/
			    INT datelen)
{
  WCHAR *w_format = NULL;
  WCHAR *w_date = NULL;
  int w_ret;
  
  /* report, for debugging */
  TRACE("(0x%lx,0x%lx, 0x%lx, time(y=%d m=%d wd=%d d=%d,h=%d,m=%d,s=%d), fmt=%p \'%s\' , %p, len=%d)\n",
	locale, flags, tflags,
	xtime->wYear,xtime->wMonth,xtime->wDayOfWeek,xtime->wDay, xtime->wHour,
	xtime->wMinute, xtime->wSecond, _format, _format, date, datelen);
  
  /*
   * Map format string into wide character form,
   * execute OLE_GetFormatW(), and convert the result
   * into ASCII
   */
  w_format = (WCHAR *) HEAP_strdupAtoW( GetProcessHeap(), 0, _format );
  
  if ( datelen > 0 )
      w_date = (WCHAR *) HeapAlloc( GetProcessHeap(), 0, datelen * sizeof(WCHAR) );
  w_ret = OLE_GetFormatW( locale, flags, tflags, xtime, w_format, w_date, datelen );
  

  HeapFree( GetProcessHeap(), 0, w_format );
  if ( w_date != NULL )
  {
      WideCharToMultiByte( CP_ACP, 0, w_date, -1, date, datelen, NULL, FALSE );
      HeapFree( GetProcessHeap(), 0, w_date );
  }

  return( w_ret );
}


/******************************************************************************
 * OLE_GetFormatW [INTERNAL]
 */
static INT OLE_GetFormatW(LCID locale, DWORD flags, DWORD tflags,
			  const SYSTEMTIME * xtime, LPCWSTR format,
			  LPWSTR date, INT datelen)
{
    INT inpos, outpos;
    int count, type, inquote, Overflow;
    WCHAR buf[40];
    WCHAR *pos;
    int buflen;
    char tmp[40];

    const CHAR *_dgfmt[] = { "%d", "%02d" };
    const CHAR **dgfmt = _dgfmt - 1;


    /* report, for debugging */
    TRACE("(0x%lx,0x%lx, 0x%lx, time(y=%d m=%d wd=%d d=%d,h=%d,m=%d,s=%d), fmt=%p \'%s\' , %p, len=%d)\n", locale, flags, tflags, xtime->wYear, xtime->wMonth, xtime->wDayOfWeek, xtime->wDay, xtime->wHour, xtime->wMinute, xtime->wSecond, debugstr_w(format), debugstr_w(format), date, datelen);

    if ( datelen < 0 )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return( 0 );
    }

    if(datelen == 0)
    {
        FIXME("datelen = 0, returning 255\n");
        return 255;
    }

    /* initalize state variables and output buffer */
    inpos = outpos = 0;
    count = 0;
    inquote = 0;
    Overflow = 0;
    type = '\0';
    date[0] = buf[0] = '\0';


    if ( flags & LOCALE_NOUSEROVERRIDE || tflags & LOCALE_NOUSEROVERRIDE )
        WARN( "LOCALE_NOUSEROVERRIDE is not implemented" );
	
    if ( flags & LOCALE_USE_CP_ACP || tflags & LOCALE_USE_CP_ACP )
        WARN( "LOCALE_USE_CP_ACP is not implemented" );
	
    if ( flags & DATE_SHORTDATE )
        WARN( "DATE_SHORTDATE is not implemented" );
	
    if ( flags & DATE_YEARMONTH )
        WARN( "DATE_YEARMONTH is not implemented" );
	
    if ( flags & DATE_USE_ALT_CALENDAR )
        WARN( "DATE_USE_ALT_CALENDAR is not implemented" );
	
    if ( flags & DATE_LTRREADING || flags & DATE_RTLREADING )
        WARN( "DATE_LTRREADING and DATE_RTRREADING are not implemented" );
	

    if ( tflags & TIME_NOMINUTESORSECONDS )
    {
        const WCHAR seconds_expression[] = { ':', 'm', 'm', ':', 's', 's', 0 };

        if ( (pos = strstrW(format, seconds_expression)) )
        {
            size_t len = lstrlenW(pos) * sizeof(WCHAR) - (6 * sizeof(WCHAR));
            memmove( pos, pos + 6, len );
            pos[ len / sizeof(WCHAR) ] = 0;	
        }
    }
	
    if ( tflags & TIME_NOSECONDS )
    {
        const WCHAR seconds_expression[] = { ':', 's', 's', 0 };

        if ( (pos = strstrW(format, seconds_expression)) )
        {
            size_t len = lstrlenW(pos) * sizeof(WCHAR) - (3 * sizeof(WCHAR));
            memmove( pos, pos + 3, len );
            pos[ len / sizeof(WCHAR) ] = 0;	
        }
    }
	
    for (inpos = 0;; inpos++)
    {
        if(inquote)
        {
            if(format[inpos] == '\'')
            {
                if(format[inpos + 1] == '\'')
                {
                    inpos ++;
                    date[outpos++] = '\'';
                }
                else
                {
                    inquote = 0;
                    continue;	/* we did nothing to the output */
                }
            }
            else if(format[inpos] == '\0')
            {
                date[outpos++] = '\0';
                if(outpos > datelen)
                    Overflow = 1;
                break;
            }
            else
            {
                date[outpos++] = format[inpos];
                if(outpos > datelen)
                {
                    Overflow = 1;
                    date[outpos - 1] = '\0';	/* this is the last place where
                                                   it's safe to write */
                    break;
                }
            }
        }
        else if((count && (format[inpos] != type))
                || count == 4 || (count == 2 && strchr("ghHmst", type)))
        {
            if(type == 'h' && (tflags & TIME_FORCE24HOURFORMAT))
                type = 'H';
				
            if(type == 'd')
            {
                if(count == 4)
                {
                    GetLocaleInfoW(locale,
                                   LOCALE_SDAYNAME1 +
                                   (xtime->wDayOfWeek + 6) % 7, buf,
                                   sizeof(buf));
                }
                else if(count == 3)
                {
                    GetLocaleInfoW(locale,
                                   LOCALE_SABBREVDAYNAME1 + 
                                   (xtime->wDayOfWeek + 6) % 7, buf,
                                   sizeof(buf));
                }
                else
                {
                    sprintf(tmp, dgfmt[count], xtime->wDay);
                    MultiByteToWideChar(CP_ACP, 0, tmp, -1,
                                        buf,
                                        sizeof(buf) / sizeof(WCHAR));
                }
            }
            else if(type == 'M')
            {
                if(count == 3)
                {
                    GetLocaleInfoW(locale,
                                   LOCALE_SABBREVMONTHNAME1 +
                                   xtime->wMonth - 1, buf,
                                   sizeof(buf));
                }
                else if(count == 4)
                {
                    GetLocaleInfoW(locale,
                                   LOCALE_SMONTHNAME1 +
                                   xtime->wMonth - 1, buf,
                                   sizeof(buf));
                }
                else
                {
                    sprintf(tmp, dgfmt[count],
                            xtime->wMonth);
                    MultiByteToWideChar(CP_ACP, 0, tmp, -1,
                                        buf,
                                        sizeof(buf) / sizeof(WCHAR));
                }
            }
            else if(type == 'y')
            {
                if(count >= 3 && count <= 5 )
                {
                    sprintf(tmp, dgfmt[1], xtime->wYear);
                    MultiByteToWideChar(CP_ACP, 0, tmp, -1,
                                        buf,
                                        sizeof(buf) / sizeof(WCHAR));
                }
                else if ( count == 2 )
                {
                    sprintf(tmp, dgfmt[count],
                            xtime->wYear % 100);
                    MultiByteToWideChar(CP_ACP, 0, tmp, -1,
                                        buf,
                                        sizeof(buf) / sizeof(WCHAR));
                }
                else
                {
                    WCHAR yyy_c[] = { 'y', 'y', 'y', 0 };
                    lstrcpyW(buf, yyy_c);
                    WARN("unknown format, c=%c, n=%d\n",
                         type, count);
                }
			       
            }
            else if(type == 'g')
            {
                if(count == 2)
                {
                    FIXME("LOCALE_ICALENDARTYPE unimp.\n");
                    WCHAR ad_c[] = { 'A', 'D', 0 };
                    lstrcpyW(buf, ad_c);
                }
                else
                {
                    WCHAR g_c[] = { 'g', 0 };
                    lstrcpyW(buf, g_c);
                    WARN("unknown format, c=%c, n=%d\n",
                         type, count);
                }
            }
            else if(type == 'h')
            {
                /* gives us hours 1:00 -- 12:00 */
                sprintf(tmp, dgfmt[count],
                        (xtime->wHour - 1) % 12 + 1);
                MultiByteToWideChar(CP_ACP, 0, tmp, -1, buf,
                                    sizeof(buf) /
                                    sizeof(WCHAR));
            }
            else if(type == 'H')
            {
                /* 24-hour time */
                sprintf(tmp, dgfmt[count], xtime->wHour);
                MultiByteToWideChar(CP_ACP, 0, tmp, -1, buf,
                                    sizeof(buf) /
                                    sizeof(WCHAR));
            }
            else if(type == 'm')
            {
                sprintf(tmp, dgfmt[count], xtime->wMinute);
                MultiByteToWideChar(CP_ACP, 0, tmp, -1, buf,
                                    sizeof(buf) /
                                    sizeof(WCHAR));
            }
            else if(type == 's')
            {
                sprintf(tmp, dgfmt[count], xtime->wSecond);
                MultiByteToWideChar(CP_ACP, 0, tmp, -1, buf,
                                    sizeof(buf) /
                                    sizeof(WCHAR));
            }
            else if(type == 't')
            {
                if((tflags & TIME_NOTIMEMARKER))
                    buf[0] = '\0';
                else if(count == 1)
                {
                    sprintf(tmp, "%c",
                            (xtime->wHour < 12) ? 'A' : 'P');
                    MultiByteToWideChar(CP_ACP, 0, tmp, -1,
                                        buf,
                                        sizeof(buf) / sizeof(WCHAR));
                }
                else if(count == 2)
                {
                    GetLocaleInfoW(locale,
                                   (xtime->wHour < 12) ? LOCALE_S1159 : LOCALE_S2359, buf,
                                   sizeof(buf));
                }
            };

            /* we need to check the next char in the format string
               again, no matter what happened */
            inpos--;

            /* add the contents of buf to the output */
            buflen = strlenW(buf);
            if(outpos + buflen < datelen)
            {
                date[outpos] = '\0';	/* for strcat to hook onto */
                strcatW(date, buf);
                outpos += buflen;
            }
            else
            {
                date[outpos] = '\0';
                lstrcpynW(date + strlenW(date), buf,
                         datelen - outpos);
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                WARN("insufficient buffer\n");
                return 0;
            }

            /* reset the variables we used to keep track of this item */
            count = 0;
            type = '\0';
        }
        else if(format[inpos] == '\0')
        {
            /* we can't check for this at the loop-head, because
               that breaks the printing of the last format-item */
            date[outpos] = '\0';
            break;
        }
        else if(count)
        {
            /* continuing a code for an item */
            count += 1;
            continue;
        }
        else if(strchr("hHmstyMdg", format[inpos]))
        {
            type = format[inpos];
            count = 1;
            continue;
        }
        else if(format[inpos] == '\'')
        {
            inquote = 1;
            continue;
        }
        else
        {
            date[outpos++] = format[inpos];
        }
        /* now deal with a possible buffer overflow */
        if(outpos >= datelen)
        {
            date[datelen - 1] = '\0';
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return 0;
        }
    }

    if(Overflow)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return( 0 );
    }

    /* finish it off with a string terminator */
    outpos++;
    /* sanity check */
    if(outpos > datelen - 1)
        outpos = datelen - 1;
    date[outpos] = '\0';

    /* strip trail whitespace */
    while( outpos > 0 && (date[outpos] == ' ' || date[outpos] == 0) )
    {
        date[ outpos ] = 0;
        outpos --;
    }

    TRACE("returns string '%s', len %d\n", debugstr_w(date), outpos);
    return outpos + 2; // +2: last char + null terminator
}


/******************************************************************************
 *		GetDateFormatA	[KERNEL32.@]
 * Makes an ASCII string of the date
 *
 * This function uses format to format the date,  or,  if format
 * is NULL, uses the default for the locale.  format is a string
 * of literal fields and characters as follows:
 *
 * - d    single-digit (no leading zero) day (of month)
 * - dd   two-digit day (of month)
 * - ddd  short day-of-week name
 * - dddd long day-of-week name
 * - M    single-digit month
 * - MM   two-digit month
 * - MMM  short month name
 * - MMMM full month name
 * - y    two-digit year, no leading 0
 * - yy   two-digit year
 * - yyyy four-digit year
 * - gg   era string
 *
 */
INT WINAPI GetDateFormatA(LCID locale,DWORD flags,
			      const SYSTEMTIME* xtime,
			      LPCSTR format, LPSTR date,INT datelen)
{

  char format_buf[40];
  LPCSTR thisformat;
  SYSTEMTIME t;
  LPSYSTEMTIME thistime;
  LCID thislocale;
  INT ret;
  FILETIME ft;
  BOOL res;

  TRACE("(0x%04lx,0x%08lx,%p,%s,%p,%d)\n",
	      locale,flags,xtime,format,date,datelen);

  if (!locale) {
     locale = LOCALE_SYSTEM_DEFAULT;
     };

  if (locale == LOCALE_SYSTEM_DEFAULT) {
     thislocale = GetSystemDefaultLCID();
  } else if (locale == LOCALE_USER_DEFAULT) {
     thislocale = GetUserDefaultLCID();
  } else {
     thislocale = locale;
   };

  if (xtime == NULL) {
     GetSystemTime(&t);
  } else {
      /* Silently correct wDayOfWeek by transforming to FileTime and back again */
      res=SystemTimeToFileTime(xtime,&ft);
      /* Check year(?)/month and date for range and set ERROR_INVALID_PARAMETER  on error */
      /*FIXME: SystemTimeToFileTime doesn't yet do that check */
      if(!res)
	{
	  SetLastError(ERROR_INVALID_PARAMETER);
	  return 0;
	}
      FileTimeToSystemTime(&ft,&t);

  };
  thistime = &t;

  if (format == NULL) {
     GetLocaleInfoA(thislocale, ((flags&DATE_LONGDATE)
				   ? LOCALE_SLONGDATE
				   : LOCALE_SSHORTDATE),
		      format_buf, sizeof(format_buf));
     thisformat = format_buf;
  } else {
     thisformat = format;
  };


  ret = OLE_GetFormatA(thislocale, flags, 0, thistime, thisformat,
		       date, datelen);


   TRACE(
	       "GetDateFormatA() returning %d, with data=%s\n",
	       ret, date);
  return ret;
}





/******************************************************************************
 *		GetDateFormatW	[KERNEL32.@]
 * Makes a Unicode string of the date
 *
 * Acts the same as GetDateFormatA(),  except that it's Unicode.
 * Accepts & returns sizes as counts of Unicode characters.
 *
 */
INT WINAPI GetDateFormatW(LCID locale, DWORD flags,
			  const SYSTEMTIME * xtime,
			  LPCWSTR format, LPWSTR date, INT datelen)
{
    WCHAR format_buf[40];
    LPCWSTR thisformat;
    SYSTEMTIME t;
    LPSYSTEMTIME thistime;
    LCID thislocale;
    INT ret;
    FILETIME ft;
    BOOL res;

    TRACE("(0x%04lx,0x%08lx,%p,%s,%p,%d)\n",
          locale, flags, xtime, debugstr_w(format), date, datelen);

    if(!locale)
        locale = LOCALE_SYSTEM_DEFAULT;

    if(locale == LOCALE_SYSTEM_DEFAULT)
        thislocale = GetSystemDefaultLCID();
    else if(locale == LOCALE_USER_DEFAULT)
        thislocale = GetUserDefaultLCID();
    else
        thislocale = locale;

    if(xtime == NULL)
        GetSystemTime(&t);
    else
    {
        /* Silently correct wDayOfWeek by transforming to FileTime and back again */
        res = SystemTimeToFileTime(xtime, &ft);
        /* Check year(?)/month and date for range and set ERROR_INVALID_PARAMETER  on error */
        /*FIXME: SystemTimeToFileTime doesn't yet do that check */
        if(!res)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
        }
        FileTimeToSystemTime(&ft, &t);

    };
    thistime = &t;

    if(format == NULL)
    {
        GetLocaleInfoW(thislocale, ((flags & DATE_LONGDATE)
                                    ? LOCALE_SLONGDATE
                                    : LOCALE_SSHORTDATE),
                       format_buf, sizeof(format_buf));
        thisformat = format_buf;
    }
    else
        thisformat = format;


    ret = OLE_GetFormatW(thislocale, flags, 0, thistime, thisformat,
                         date, datelen);


    TRACE("GetDateFormatW() returning %d, with data=%s\n",
          ret, debugstr_w(date));
    return ret;
}

/**************************************************************************
 *              EnumDateFormatsA	(KERNEL32.@)
 */
BOOL WINAPI EnumDateFormatsA(
  DATEFMT_ENUMPROCA lpDateFmtEnumProc, LCID Locale,  DWORD dwFlags)
{
  LCID Loc = GetUserDefaultLCID();
  if(!lpDateFmtEnumProc)
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }

  switch( Loc )
 {

   case 0x00000407:  /* (Loc,"de_DE") */
   {
    switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd.MM.yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d.M.yyyy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d.MM.yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d.M.yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dddd,d. MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d. MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d. MMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
     }
   }

   case 0x0000040c:  /* (Loc,"fr_FR") */
   {
    switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd.MM.yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd-MM-yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd/MM/yyyy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dddd d MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d MMM yy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
     }
   }

   case 0x00000c0c:  /* (Loc,"fr_CA") */
   {
    switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("yy-MM-dd")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd-MM-yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("yy MM dd")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("d MMMM, yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d MMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
     }
   }

   case 0x00000809:  /* (Loc,"en_UK") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd/MM/yyyy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d/M/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d.M.yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dd MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00000c09:  /* (Loc,"en_AU") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("d/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d/M/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dddd,d MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00001009:  /* (Loc,"en_CA") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d/M/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("yy-MM-dd")) return TRUE;
	if(!(*lpDateFmtEnumProc)("M/dd/yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("d-MMM-yy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("MMMM d, yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00001409:  /* (Loc,"en_NZ") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("d/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d.MM.yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("d MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("dddd, d MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00001809:  /* (Loc,"en_IE") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd/MM/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d/M/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("d.M.yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dd MMMM yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("d MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00001c09:  /* (Loc,"en_ZA") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("yy/MM/dd")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dd MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00002009:  /* (Loc,"en_JM") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd/MM/yyyy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dddd,MMMM dd,yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("MMMM dd,yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("dddd,dd MMMM,yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("dd MMMM,yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   case 0x00002809:  /* (Loc,"en_BZ") */
   case 0x00002c09:  /* (Loc,"en_TT") */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("dd/MM/yyyy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dddd,dd MMMM yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }

   default:  /* default to US English "en_US" */
  {
   switch(dwFlags)
    {
      case DATE_SHORTDATE:
	if(!(*lpDateFmtEnumProc)("M/d/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("M/d/yyyy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("MM/dd/yy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("MM/dd/yyyy")) return TRUE;
	if(!(*lpDateFmtEnumProc)("yy/MM/dd")) return TRUE;
	if(!(*lpDateFmtEnumProc)("dd-MMM-yy")) return TRUE;
	return TRUE;
      case DATE_LONGDATE:
        if(!(*lpDateFmtEnumProc)("dddd, MMMM dd, yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("MMMM dd, yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("dddd, dd MMMM, yyyy")) return TRUE;
        if(!(*lpDateFmtEnumProc)("dd MMMM, yyyy")) return TRUE;
	return TRUE;
      default:
	FIXME("Unknown date format (%ld)\n", dwFlags);
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
  }
 }
}

/**************************************************************************
 *              EnumDateFormatsW	(KERNEL32.@)
 */
BOOL WINAPI EnumDateFormatsW(
  DATEFMT_ENUMPROCW lpDateFmtEnumProc, LCID Locale, DWORD dwFlags)
{
  FIXME("(%p, %ld, %ld): stub\n", lpDateFmtEnumProc, Locale, dwFlags);
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}

/**************************************************************************
 *              EnumTimeFormatsA	(KERNEL32.@)
 */
BOOL WINAPI EnumTimeFormatsA(
  TIMEFMT_ENUMPROCA lpTimeFmtEnumProc, LCID Locale, DWORD dwFlags)
{
  LCID Loc = GetUserDefaultLCID();
  if(!lpTimeFmtEnumProc)
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
  if(dwFlags)
    {
      FIXME("Unknown time format (%ld)\n", dwFlags);
    }

  switch( Loc )
 {
   case 0x00000407:  /* (Loc,"de_DE") */
   {
    if(!(*lpTimeFmtEnumProc)("HH.mm")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("HH:mm:ss")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("H:mm:ss")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("H.mm")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("H.mm'Uhr'")) return TRUE;
    return TRUE;
   }

   case 0x0000040c:  /* (Loc,"fr_FR") */
   case 0x00000c0c:  /* (Loc,"fr_CA") */
   {
    if(!(*lpTimeFmtEnumProc)("H:mm")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("HH:mm:ss")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("H:mm:ss")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("HH.mm")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("HH'h'mm")) return TRUE;
    return TRUE;
   }

   case 0x00000809:  /* (Loc,"en_UK") */
   case 0x00000c09:  /* (Loc,"en_AU") */
   case 0x00001409:  /* (Loc,"en_NZ") */
   case 0x00001809:  /* (Loc,"en_IE") */
   {
    if(!(*lpTimeFmtEnumProc)("h:mm:ss tt")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("HH:mm:ss")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("H:mm:ss")) return TRUE;
    return TRUE;
   }

   case 0x00001c09:  /* (Loc,"en_ZA") */
   case 0x00002809:  /* (Loc,"en_BZ") */
   case 0x00002c09:  /* (Loc,"en_TT") */
   {
    if(!(*lpTimeFmtEnumProc)("h:mm:ss tt")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("hh:mm:ss tt")) return TRUE;
    return TRUE;
   }

   default:  /* default to US style "en_US" */
   {
    if(!(*lpTimeFmtEnumProc)("h:mm:ss tt")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("hh:mm:ss tt")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("H:mm:ss")) return TRUE;
    if(!(*lpTimeFmtEnumProc)("HH:mm:ss")) return TRUE;
    return TRUE;
   }
 }
}

/**************************************************************************
 *              EnumTimeFormatsW	(KERNEL32.@)
 */
BOOL WINAPI EnumTimeFormatsW(
  TIMEFMT_ENUMPROCW lpTimeFmtEnumProc, LCID Locale, DWORD dwFlags)
{
  FIXME("(%p,%ld,%ld): stub\n", lpTimeFmtEnumProc, Locale, dwFlags);
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}

/**************************************************************************
 *           This function is used just locally !
 *  Description: Inverts a string.
 */
static void OLE_InvertString(char* string)
{
    char    sTmpArray[128];
    INT     counter, i = 0;

    for (counter = strlen(string); counter > 0; counter--)
    {
        memcpy(sTmpArray + i, string + counter-1, 1);
        i++;
    }
    memcpy(sTmpArray + i, "\0", 1);
    strcpy(string, sTmpArray);
}

/***************************************************************************************
 *           This function is used just locally !
 *  Description: Test if the given string (psNumber) is valid or not.
 *               The valid characters are the following:
 *               - Characters '0' through '9'.
 *               - One decimal point (dot) if the number is a floating-point value.
 *               - A minus sign in the first character position if the number is
 *                 a negative value.
 *              If the function succeeds, psBefore/psAfter will point to the string
 *              on the right/left of the decimal symbol. pbNegative indicates if the
 *              number is negative.
 */
static INT OLE_GetNumberComponents(char* pInput, char* psBefore, char* psAfter, BOOL* pbNegative)
{
char	sNumberSet[] = "0123456789";
BOOL	bInDecimal = FALSE;

	/* Test if we do have a minus sign */
	if ( *pInput == '-' )
	{
		*pbNegative = TRUE;
		pInput++; /* Jump to the next character. */
	}

	while(*pInput != '\0')
	{
		/* Do we have a valid numeric character */
		if ( strchr(sNumberSet, *pInput) != NULL )
		{
			if (bInDecimal == TRUE)
                *psAfter++ = *pInput;
			else
                *psBefore++ = *pInput;
		}
		else
		{
			/* Is this a decimal point (dot) */
			if ( *pInput == '.' )
			{
				/* Is it the first time we find it */
				if ((bInDecimal == FALSE))
					bInDecimal = TRUE;
				else
					return -1; /* ERROR: Invalid parameter */
			}
			else
			{
				/* It's neither a numeric character, nor a decimal point.
				 * Thus, return an error.
                 */
				return -1;
			}
		}
        pInput++;
	}

	/* Add an End of Line character to the output buffers */
	*psBefore = '\0';
	*psAfter = '\0';

	return 0;
}

/**************************************************************************
 *           This function is used just locally !
 *  Description: A number could be formatted using different numbers
 *               of "digits in group" (example: 4;3;2;0).
 *               The first parameter of this function is an array
 *               containing the rule to be used. Its format is the following:
 *               |NDG|DG1|DG2|...|0|
 *               where NDG is the number of used "digits in group" and DG1, DG2,
 *               are the corresponding "digits in group".
 *               Thus, this function returns the grouping value in the array
 *               pointed by the second parameter.
 */
static INT OLE_GetGrouping(char* sRule, INT index)
{
    char    sData[2], sRuleSize[2];
    INT     nData, nRuleSize;

    memcpy(sRuleSize, sRule, 1);
    memcpy(sRuleSize+1, "\0", 1);
    nRuleSize = atoi(sRuleSize);

    if (index > 0 && index < nRuleSize)
    {
        memcpy(sData, sRule+index, 1);
        memcpy(sData+1, "\0", 1);
        nData = atoi(sData);
    }

    else
    {
        memcpy(sData, sRule+nRuleSize-1, 1);
        memcpy(sData+1, "\0", 1);
        nData = atoi(sData);
    }

    return nData;
}

/**************************************************************************
 *              GetNumberFormatA	(KERNEL32.@)
 */
INT WINAPI GetNumberFormatA(LCID locale, DWORD dwflags,
			       LPCSTR lpvalue,   const NUMBERFMTA * lpFormat,
			       LPSTR lpNumberStr, int cchNumber)
{
    char   sNumberDigits[3], sDecimalSymbol[5], sDigitsInGroup[11], sDigitGroupSymbol[5], sILZero[2];
    INT    nNumberDigits, nNumberDecimal, i, j, nCounter, nStep, nRuleIndex, nGrouping, nDigits, retVal, nLZ;
    char   sNumber[128], sDestination[128], sDigitsAfterDecimal[10], sDigitsBeforeDecimal[128];
    char   sRule[10], sSemiColumn[]=";", sBuffer[5], sNegNumber[2];
    char   *pStr = NULL, *pTmpStr = NULL;
    LCID   systemDefaultLCID;
    BOOL   bNegative = FALSE;
    enum   Operations
    {
        USE_PARAMETER,
        USE_LOCALEINFO,
        USE_SYSTEMDEFAULT,
        RETURN_ERROR
    } used_operation;

    strncpy(sNumber, lpvalue, 128);
    sNumber[127] = '\0';

    /* Make sure we have a valid input string, get the number
     * of digits before and after the decimal symbol, and check
     * if this is a negative number.
     */
    if ( OLE_GetNumberComponents(sNumber, sDigitsBeforeDecimal, sDigitsAfterDecimal, &bNegative) != -1)
    {
        nNumberDecimal = strlen(sDigitsBeforeDecimal);
        nDigits = strlen(sDigitsAfterDecimal);
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    /* Which source will we use to format the string */
    used_operation = RETURN_ERROR;
    if (lpFormat != NULL)
    {
        if (dwflags == 0)
            used_operation = USE_PARAMETER;
    }
    else
    {
        if (dwflags & LOCALE_NOUSEROVERRIDE)
            used_operation = USE_LOCALEINFO;
        else
            used_operation = USE_SYSTEMDEFAULT;
    }

    /* Load the fields we need */
    switch(used_operation)
    {
    case USE_LOCALEINFO:
        GetLocaleInfoA(locale, LOCALE_IDIGITS, sNumberDigits, sizeof(sNumberDigits));
        GetLocaleInfoA(locale, LOCALE_SDECIMAL, sDecimalSymbol, sizeof(sDecimalSymbol));
        GetLocaleInfoA(locale, LOCALE_SGROUPING, sDigitsInGroup, sizeof(sDigitsInGroup));
        GetLocaleInfoA(locale, LOCALE_STHOUSAND, sDigitGroupSymbol, sizeof(sDigitGroupSymbol));
        GetLocaleInfoA(locale, LOCALE_ILZERO, sILZero, sizeof(sILZero));
        GetLocaleInfoA(locale, LOCALE_INEGNUMBER, sNegNumber, sizeof(sNegNumber));
        break;
    case USE_PARAMETER:
        sprintf(sNumberDigits, "%d",lpFormat->NumDigits);
        strcpy(sDecimalSymbol, lpFormat->lpDecimalSep);
        sprintf(sDigitsInGroup, "%d;0",lpFormat->Grouping);
        strcpy(sDigitGroupSymbol, lpFormat->lpThousandSep);
        sprintf(sILZero, "%d",lpFormat->LeadingZero);
        sprintf(sNegNumber, "%d",lpFormat->NegativeOrder);
        break;
    case USE_SYSTEMDEFAULT:
        systemDefaultLCID = GetSystemDefaultLCID();
        GetLocaleInfoA(systemDefaultLCID, LOCALE_IDIGITS, sNumberDigits, sizeof(sNumberDigits));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_SDECIMAL, sDecimalSymbol, sizeof(sDecimalSymbol));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_SGROUPING, sDigitsInGroup, sizeof(sDigitsInGroup));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_STHOUSAND, sDigitGroupSymbol, sizeof(sDigitGroupSymbol));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_ILZERO, sILZero, sizeof(sILZero));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_INEGNUMBER, sNegNumber, sizeof(sNegNumber));
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    nNumberDigits = atoi(sNumberDigits);

    /* Remove the ";" */
    i=0;
    j = 1;
    for (nCounter=0; nCounter<strlen(sDigitsInGroup); nCounter++)
    {
        if ( memcmp(sDigitsInGroup + nCounter, sSemiColumn, 1) != 0 )
        {
            memcpy(sRule + j, sDigitsInGroup + nCounter, 1);
            i++;
            j++;
        }
    }
    sprintf(sBuffer, "%d", i);
    memcpy(sRule, sBuffer, 1); /* Number of digits in the groups ( used by OLE_GetGrouping() ) */
    memcpy(sRule + j, "\0", 1);

    /* First, format the digits before the decimal. */
    if ((nNumberDecimal>0) && (atoi(sDigitsBeforeDecimal) != 0))
    {
        /* Working on an inverted string is easier ! */
        OLE_InvertString(sDigitsBeforeDecimal);

        nStep = nCounter = i = j = 0;
        nRuleIndex = 1;
        nGrouping = OLE_GetGrouping(sRule, nRuleIndex);

        /* Here, we will loop until we reach the end of the string.
         * An internal counter (j) is used in order to know when to
         * insert the "digit group symbol".
         */
        while (nNumberDecimal > 0)
        {
            i = nCounter + nStep;
            memcpy(sDestination + i, sDigitsBeforeDecimal + nCounter, 1);
            nCounter++;
            j++;
            if (j >= nGrouping)
            {
                j = 0;
                if (nRuleIndex < sRule[0])
                    nRuleIndex++;
                nGrouping = OLE_GetGrouping(sRule, nRuleIndex);
                memcpy(sDestination + i+1, sDigitGroupSymbol, strlen(sDigitGroupSymbol));
                nStep+= strlen(sDigitGroupSymbol);
            }

            nNumberDecimal--;
        }

        memcpy(sDestination + i+1, "\0", 1);
        /* Get the string in the right order ! */
        OLE_InvertString(sDestination);
     }
     else
     {
        nLZ = atoi(sILZero);
        if (nLZ != 0)
        {
            /* Use 0.xxx instead of .xxx */
            memcpy(sDestination, "0", 1);
            memcpy(sDestination+1, "\0", 1);
        }
        else
            memcpy(sDestination, "\0", 1);

     }

    /* Second, format the digits after the decimal. */
    j = 0;
    nCounter = nNumberDigits;
    if ( (nDigits>0) && (pStr = strstr (sNumber, ".")) )
    {
        i = strlen(sNumber) - strlen(pStr) + 1;
        strncpy ( sDigitsAfterDecimal, sNumber + i, nNumberDigits);
        j = strlen(sDigitsAfterDecimal);
        if (j < nNumberDigits)
            nCounter = nNumberDigits-j;
    }
    for (i=0;i<nCounter;i++)
         memcpy(sDigitsAfterDecimal+i+j, "0", 1);
    memcpy(sDigitsAfterDecimal + nNumberDigits, "\0", 1);

    i = strlen(sDestination);
    j = strlen(sDigitsAfterDecimal);
    /* Finally, construct the resulting formatted string. */

    for (nCounter=0; nCounter<i; nCounter++)
        memcpy(sNumber + nCounter, sDestination + nCounter, 1);

    memcpy(sNumber + nCounter, sDecimalSymbol, strlen(sDecimalSymbol));

    for (i=0; i<j; i++)
        memcpy(sNumber + nCounter+i+strlen(sDecimalSymbol), sDigitsAfterDecimal + i, 1);
    memcpy(sNumber + nCounter+i+strlen(sDecimalSymbol), "\0", 1);

    /* Is it a negative number */
    if (bNegative == TRUE)
    {
        i = atoi(sNegNumber);
        pStr = sDestination;
        pTmpStr = sNumber;
        switch (i)
        {
        case 0:
            *pStr++ = '(';
            while (*sNumber != '\0')
                *pStr++ =  *pTmpStr++;
            *pStr++ = ')';
            break;
        case 1:
            *pStr++ = '-';
            while (*pTmpStr != '\0')
                *pStr++ =  *pTmpStr++;
            break;
        case 2:
            *pStr++ = '-';
            *pStr++ = ' ';
            while (*pTmpStr != '\0')
                *pStr++ =  *pTmpStr++;
            break;
        case 3:
            while (*pTmpStr != '\0')
                *pStr++ =  *pTmpStr++;
            *pStr++ = '-';
            break;
        case 4:
            while (*pTmpStr != '\0')
                *pStr++ =  *pTmpStr++;
            *pStr++ = ' ';
            *pStr++ = '-';
            break;
        default:
            while (*pTmpStr != '\0')
                *pStr++ =  *pTmpStr++;
            break;
        }
    }
    else
        strcpy(sDestination, sNumber);

    /* If cchNumber is zero, then returns the number of bytes or characters
     * required to hold the formatted number string
     */
    if (cchNumber==0)
        retVal = strlen(sDestination) + 1;
    else
    {
        strncpy (lpNumberStr, sDestination, cchNumber-1);
        *(lpNumberStr+cchNumber-1) = '\0';   /* ensure we got a NULL at the end */
        retVal = strlen(lpNumberStr);
    }

    return retVal;
}

/**************************************************************************
 *              GetNumberFormatW	(KERNEL32.@)
 */
INT WINAPI GetNumberFormatW(LCID locale, DWORD dwflags,
			       LPCWSTR lpvalue,  const NUMBERFMTW * lpFormat,
			       LPWSTR lpNumberStr, int cchNumber)
{
 FIXME("%s: stub, no reformatting done\n",debugstr_w(lpvalue));

 lstrcpynW( lpNumberStr, lpvalue, cchNumber );
 return cchNumber? strlenW( lpNumberStr ) : 0;
}

/**************************************************************************
 *              GetCurrencyFormatA	(KERNEL32.@)
 */
INT WINAPI GetCurrencyFormatA(LCID locale, DWORD dwflags,
			       LPCSTR lpvalue,   const CURRENCYFMTA * lpFormat,
			       LPSTR lpCurrencyStr, int cchCurrency)
{
    UINT   nPosOrder, nNegOrder;
    INT    retVal;
    char   sDestination[128], sNegOrder[8], sPosOrder[8], sCurrencySymbol[8];
    char   *pDestination = sDestination;
    char   sNumberFormated[128];
    char   *pNumberFormated = sNumberFormated;
    LCID   systemDefaultLCID;
    BOOL   bIsPositive = FALSE, bValidFormat = FALSE;
    enum   Operations
    {
        USE_PARAMETER,
        USE_LOCALEINFO,
        USE_SYSTEMDEFAULT,
        RETURN_ERROR
    } used_operation;

    NUMBERFMTA  numberFmt;

    /* Which source will we use to format the string */
    used_operation = RETURN_ERROR;
    if (lpFormat != NULL)
    {
        if (dwflags == 0)
            used_operation = USE_PARAMETER;
    }
    else
    {
        if (dwflags & LOCALE_NOUSEROVERRIDE)
            used_operation = USE_LOCALEINFO;
        else
            used_operation = USE_SYSTEMDEFAULT;
    }

    /* Load the fields we need */
    switch(used_operation)
    {
    case USE_LOCALEINFO:
        /* Specific to CURRENCYFMTA */
        GetLocaleInfoA(locale, LOCALE_INEGCURR, sNegOrder, sizeof(sNegOrder));
        GetLocaleInfoA(locale, LOCALE_ICURRENCY, sPosOrder, sizeof(sPosOrder));
        GetLocaleInfoA(locale, LOCALE_SCURRENCY, sCurrencySymbol, sizeof(sCurrencySymbol));

        nPosOrder = atoi(sPosOrder);
        nNegOrder = atoi(sNegOrder);
        break;
    case USE_PARAMETER:
        /* Specific to CURRENCYFMTA */
        nNegOrder = lpFormat->NegativeOrder;
        nPosOrder = lpFormat->PositiveOrder;
        strcpy(sCurrencySymbol, lpFormat->lpCurrencySymbol);
        break;
    case USE_SYSTEMDEFAULT:
        systemDefaultLCID = GetSystemDefaultLCID();
        /* Specific to CURRENCYFMTA */
        GetLocaleInfoA(systemDefaultLCID, LOCALE_INEGCURR, sNegOrder, sizeof(sNegOrder));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_ICURRENCY, sPosOrder, sizeof(sPosOrder));
        GetLocaleInfoA(systemDefaultLCID, LOCALE_SCURRENCY, sCurrencySymbol, sizeof(sCurrencySymbol));

        nPosOrder = atoi(sPosOrder);
        nNegOrder = atoi(sNegOrder);
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    /* Construct a temporary number format structure */
    if (lpFormat != NULL)
    {
        numberFmt.NumDigits     = lpFormat->NumDigits;
        numberFmt.LeadingZero   = lpFormat->LeadingZero;
        numberFmt.Grouping      = lpFormat->Grouping;
        numberFmt.NegativeOrder = 0;
        numberFmt.lpDecimalSep = lpFormat->lpDecimalSep;
        numberFmt.lpThousandSep = lpFormat->lpThousandSep;
        bValidFormat = TRUE;
    }

    /* Make a call to GetNumberFormatA() */
    if (*lpvalue == '-')
    {
        bIsPositive = FALSE;
        retVal = GetNumberFormatA(locale,0,lpvalue+1,(bValidFormat)?&numberFmt:NULL,pNumberFormated,128);
    }
    else
    {
        bIsPositive = TRUE;
        retVal = GetNumberFormatA(locale,0,lpvalue,(bValidFormat)?&numberFmt:NULL,pNumberFormated,128);
    }

    if (retVal == 0)
        return 0;

    /* construct the formatted string */
    if (bIsPositive)
    {
        switch (nPosOrder)
        {
        case 0:   /* Prefix, no separation */
            strcpy (pDestination, sCurrencySymbol);
            strcat (pDestination, pNumberFormated);
            break;
        case 1:   /* Suffix, no separation */
            strcpy (pDestination, pNumberFormated);
            strcat (pDestination, sCurrencySymbol);
            break;
        case 2:   /* Prefix, 1 char separation */
            strcpy (pDestination, sCurrencySymbol);
            strcat (pDestination, " ");
            strcat (pDestination, pNumberFormated);
            break;
        case 3:   /* Suffix, 1 char separation */
            strcpy (pDestination, pNumberFormated);
            strcat (pDestination, " ");
            strcat (pDestination, sCurrencySymbol);
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
        }
    }
    else  /* negative number */
    {
        switch (nNegOrder)
        {
        case 0:   /* format: ($1.1) */
            strcpy (pDestination, "(");
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, ")");
            break;
        case 1:   /* format: -$1.1 */
            strcpy (pDestination, "-");
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, pNumberFormated);
            break;
        case 2:   /* format: $-1.1 */
            strcpy (pDestination, sCurrencySymbol);
            strcat (pDestination, "-");
            strcat (pDestination, pNumberFormated);
            break;
        case 3:   /* format: $1.1- */
            strcpy (pDestination, sCurrencySymbol);
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, "-");
            break;
        case 4:   /* format: (1.1$) */
            strcpy (pDestination, "(");
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, ")");
            break;
        case 5:   /* format: -1.1$ */
            strcpy (pDestination, "-");
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, sCurrencySymbol);
            break;
        case 6:   /* format: 1.1-$ */
            strcpy (pDestination, pNumberFormated);
            strcat (pDestination, "-");
            strcat (pDestination, sCurrencySymbol);
            break;
        case 7:   /* format: 1.1$- */
            strcpy (pDestination, pNumberFormated);
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, "-");
            break;
        case 8:   /* format: -1.1 $ */
            strcpy (pDestination, "-");
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, " ");
            strcat (pDestination, sCurrencySymbol);
            break;
        case 9:   /* format: -$ 1.1 */
            strcpy (pDestination, "-");
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, " ");
            strcat (pDestination, pNumberFormated);
            break;
        case 10:   /* format: 1.1 $- */
            strcpy (pDestination, pNumberFormated);
            strcat (pDestination, " ");
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, "-");
            break;
        case 11:   /* format: $ 1.1- */
            strcpy (pDestination, sCurrencySymbol);
            strcat (pDestination, " ");
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, "-");
            break;
        case 12:   /* format: $ -1.1 */
            strcpy (pDestination, sCurrencySymbol);
            strcat (pDestination, " ");
            strcat (pDestination, "-");
            strcat (pDestination, pNumberFormated);
            break;
        case 13:   /* format: 1.1- $ */
            strcpy (pDestination, pNumberFormated);
            strcat (pDestination, "-");
            strcat (pDestination, " ");
            strcat (pDestination, sCurrencySymbol);
            break;
        case 14:   /* format: ($ 1.1) */
            strcpy (pDestination, "(");
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, " ");
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, ")");
            break;
        case 15:   /* format: (1.1 $) */
            strcpy (pDestination, "(");
            strcat (pDestination, pNumberFormated);
            strcat (pDestination, " ");
            strcat (pDestination, sCurrencySymbol);
            strcat (pDestination, ")");
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
        }
    }

    if (cchCurrency == 0)
        return strlen(pDestination) + 1;

    else
    {
        strncpy (lpCurrencyStr, pDestination, cchCurrency-1);
        *(lpCurrencyStr+cchCurrency-1) = '\0';   /* ensure we got a NULL at the end */
        return  strlen(lpCurrencyStr);
    }
}

/**************************************************************************
 *              GetCurrencyFormatW	(KERNEL32.@)
 */
INT WINAPI GetCurrencyFormatW(LCID locale, DWORD dwflags,
			       LPCWSTR lpvalue,   const CURRENCYFMTW * lpFormat,
			       LPWSTR lpCurrencyStr, int cchCurrency)
{
    FIXME("This API function is NOT implemented !\n");
    return 0;
}

/******************************************************************************
 *		OLE2NLS_CheckLocale	[intern]
 */
static LCID OLE2NLS_CheckLocale (LCID locale)
{
	if (!locale)
	{ locale = LOCALE_SYSTEM_DEFAULT;
	}

	if (locale == LOCALE_SYSTEM_DEFAULT)
  	{ return GetSystemDefaultLCID();
	}
	else if (locale == LOCALE_USER_DEFAULT)
	{ return GetUserDefaultLCID();
	}
	else
	{ return locale;
	}
}
/******************************************************************************
 *		GetTimeFormatA	[KERNEL32.@]
 * Makes an ASCII string of the time
 *
 * Formats date according to format,  or locale default if format is
 * NULL. The format consists of literal characters and fields as follows:
 *
 * h  hours with no leading zero (12-hour)
 * hh hours with full two digits
 * H  hours with no leading zero (24-hour)
 * HH hours with full two digits
 * m  minutes with no leading zero
 * mm minutes with full two digits
 * s  seconds with no leading zero
 * ss seconds with full two digits
 * t  time marker (A or P)
 * tt time marker (AM, PM)
 *
 */
INT WINAPI
GetTimeFormatA(LCID locale,        /* [in]  */
	       DWORD flags,        /* [in]  */
	       const SYSTEMTIME* xtime, /* [in]  */
	       LPCSTR format,      /* [in]  */
	       LPSTR timestr,      /* [out] */
	       INT timelen         /* [in]  */)
{ char format_buf[40];
  LPCSTR thisformat;
  SYSTEMTIME t;
  const SYSTEMTIME* thistime;
  LCID thislocale=0;
  DWORD thisflags=LOCALE_STIMEFORMAT; /* standard timeformat */
  INT ret;

  TRACE("GetTimeFormat(0x%04lx,0x%08lx,%p,%s,%p,%d)\n",locale,flags,xtime,format,timestr,timelen);

  thislocale = OLE2NLS_CheckLocale ( locale );

  if (format == NULL)
  { if (flags & LOCALE_NOUSEROVERRIDE)  /* use system default */
    { thislocale = GetSystemDefaultLCID();
    }
    GetLocaleInfoA(thislocale, thisflags, format_buf, sizeof(format_buf));
    thisformat = format_buf;
  }
  else
  { thisformat = format;
  }

  if (xtime == NULL) /* NULL means use the current local time */
  { GetLocalTime(&t);
    thistime = &t;
  }
  else
  { thistime = xtime;
  /* Check that hour,min and sec is in range */
  }
  ret = OLE_GetFormatA(thislocale, thisflags, flags, thistime, thisformat,
  			 timestr, timelen);
  return ret;
}


/******************************************************************************
 *		GetTimeFormatW	[KERNEL32.@]
 * Makes a Unicode string of the time
 */
INT WINAPI
GetTimeFormatW(LCID locale,        /* [in]  */
	       DWORD flags,        /* [in]  */
	       const SYSTEMTIME* xtime, /* [in]  */
	       LPCWSTR format,     /* [in]  */
	       LPWSTR timestr,     /* [out] */
	       INT timelen         /* [in]  */)
{	
	WCHAR format_buf[40];
	LPCWSTR thisformat;
	SYSTEMTIME t;
	const SYSTEMTIME* thistime;
	LCID thislocale=0;
	DWORD thisflags=LOCALE_STIMEFORMAT; /* standard timeformat */
	INT ret;

	TRACE("GetTimeFormat(0x%04lx,0x%08lx,%p,%s,%p,%d)\n",locale,flags,
	xtime,debugstr_w(format),timestr,timelen);

	thislocale = OLE2NLS_CheckLocale ( locale );

	if (format == NULL)
	{ if (flags & LOCALE_NOUSEROVERRIDE)  /* use system default */
	  { thislocale = GetSystemDefaultLCID();
	  }
	  GetLocaleInfoW(thislocale, thisflags, format_buf, 40);
	  thisformat = format_buf;
	}
	else
	{ thisformat = format;
	}

	if (xtime == NULL) /* NULL means use the current local time */
	{ GetLocalTime(&t);
	  thistime = &t;
	}
	else
	{ thistime = xtime;
	}

	ret = OLE_GetFormatW(thislocale, thisflags, flags, thistime, thisformat,
  			 timestr, timelen);
	return ret;
}

/******************************************************************************
 *		EnumCalendarInfoA	[KERNEL32.@]
 */
BOOL WINAPI EnumCalendarInfoA(
	CALINFO_ENUMPROCA calinfoproc,LCID locale,CALID calendar,CALTYPE caltype
) {
	FIXME("(%p,0x%04lx,0x%08lx,0x%08lx),stub!\n",calinfoproc,locale,calendar,caltype);
	return FALSE;
}
