/*
 * Option parsing
 *
 * Copyright 2000 Alexandre Julliard
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>

#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "wine/library.h"
#include "wine/version.h"
#include "options.h"
#include "module.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(profile);

struct option_descr
{
    const char *longname;
    char        shortname;
    int         has_arg;
    int         inherit;
    void      (*func)( const char *arg );
    const char *usage;
};

/* default options */
struct options Options =
{
    FALSE,           /* Managed windows */
    FALSE,	     /* preset DOS cwd */
    FALSE,	     /* cdrom eject monitoring */
    ""               /* cwd */
};

const char *argv0;       /* the original argv[0] */
const char *full_argv0;  /* the full path of argv[0] (if known) */

static char *inherit_str;  /* options to pass to child processes */

static void out_of_memory(void) WINE_NORETURN;
static void out_of_memory(void)
{
    MESSAGE( "Virtual memory exhausted\n" );
    ExitProcess(1);
}

static void do_debugmsg( const char *arg );
static void do_deferred( const char *arg );
static void do_help( const char *arg );
static void do_managed( const char *arg );
static void do_version( const char *arg );
static void do_monitor_cdrom_eject( const char *arg );

static const struct option_descr option_table[] =
{
    { "debugmsg",     0, 1, 1, do_debugmsg,
      "--debugmsg name  Turn debugging-messages on or off" },
    { "dll",          0, 1, 1, MODULE_AddLoadOrderOption,
      "--dll name       Enable or disable built-in DLLs" },
    { "dosver",       0, 1, 1, VERSION_ParseDosVersion,
      "--dosver x.xx    DOS version to imitate (e.g. 6.22)\n"
      "                    Only valid with --winver win31" },
    { "help",       'h', 0, 0, do_help,
      "--help,-h        Show this help message" },
    { "managed",      0, 0, 0, do_managed,
      "--managed        Allow the window manager to manage created windows" },
    { "version",    'v', 0, 0, do_version,
      "--version,-v     Display the Wine version" },
    { "winver",       0, 1, 1, VERSION_ParseWinVersion,
      "--winver         Version to imitate (win95,win98,winme,nt351,nt40,win2k,winxp,win20,win30,win31)" },
    { "dt",           0, 0, 1, do_deferred,
      "--dt             Defer trace until Alt+F12" },
    { "use-dos-cwd",  0, 1, 0, DIR_OptionSetDosCWD, 
      "--use-dos-cwd    Used to set the DOS current working directory for the process (needs a path)"},  
    { "cmdline",      0, 1, 0, ENV_CmdlinePassthrough,
      "--cmdline        Specifies the application's command line"},
    { "monitor-cdrom-eject", 0, 0, 1, do_monitor_cdrom_eject,
      "--monitor-cdrom-eject        Activate monitoring of CD-ROM ejection requests"},
    { NULL,           0, 0, 0, NULL, NULL }  /* terminator */
};


static void do_help( const char *arg )
{
    OPTIONS_Usage();
}

static void do_version( const char *arg )
{
    MESSAGE( "%s\n", WINE_RELEASE_INFO );
    ExitProcess(0);
}

static void do_managed( const char *arg )
{
    Options.managed = TRUE;
}

static void do_monitor_cdrom_eject( const char *arg )
{
    Options.monitor_cdrom_eject = TRUE;
}

extern int wine_dbg_switch_trace(BOOL init, BOOL switchIt);
static void do_deferred( const char *arg )
{
    wine_dbg_switch_trace(TRUE, FALSE);
}

static void do_debugmsg( const char *arg )
{
    static const char * const debug_class_names[__WINE_DBCL_COUNT] = { "fixme", "err", "warn", "trace" };

    char *opt, *options = strdup(arg);
    int i;
    /* defined in relay32/relay386.c */
    extern char **debug_relay_includelist;
    extern char **debug_relay_excludelist;
    /* defined in relay32/snoop.c */
    extern char **debug_snoop_includelist;
    extern char **debug_snoop_excludelist;

    if (!(opt = strtok( options, "," ))) goto error;
    do
    {
        unsigned char set = 0, clear = 0;
        char *p = strchr( opt, '+' );
        if (!p) p = strchr( opt, '-' );
        if (!p || !p[1]) goto error;
        if (p > opt)
        {
            for (i = 0; i < __WINE_DBCL_COUNT; i++)
            {
                int len = strlen(debug_class_names[i]);
                if (len != (p - opt)) continue;
                if (!memcmp( opt, debug_class_names[i], len ))  /* found it */
                {
                    if (*p == '+') set |= 1 << i;
                    else clear |= 1 << i;
                    break;
                }
            }
            if (i == __WINE_DBCL_COUNT) goto error;  /* class name not found */
        }
        else
        {
            if (*p == '+') set = ~0;
            else clear = ~0;
	    if (!strncasecmp(p+1, "relay=", 6) ||
		!strncasecmp(p+1, "snoop=", 6))
		{
		    int i, l;
		    char *s, *s2, ***output, c;

		    if (strchr(p,','))
			l=strchr(p,',')-p;
		    else
			l=strlen(p);
		    set = ~0;
		    clear = 0;
		    output = (*p == '+') ?
			((*(p+1) == 'r') ?
			 &debug_relay_includelist :
			 &debug_snoop_includelist) :
			((*(p+1) == 'r') ?
			 &debug_relay_excludelist :
			 &debug_snoop_excludelist);
		    s = p + 7;
		    /* if there are n ':', there are n+1 modules, and we need
                       n+2 slots, last one being for the sentinel (NULL) */
		    i = 2;
		    while((s = strchr(s, ':'))) i++, s++;
		    *output = malloc(sizeof(char **) * i);
		    i = 0;
		    s = p + 7;
		    while((s2 = strchr(s, ':'))) {
			c = *s2;
			*s2 = '\0';
			*((*output)+i) = _strupr(strdup(s));
			*s2 = c;
			s = s2 + 1;
			i++;
		    }
		    c = *(p + l);
		    *(p + l) = '\0';
		    *((*output)+i) = _strupr(strdup(s));
		    *(p + l) = c;
		    *((*output)+i+1) = NULL;
		    *(p + 6) = '\0';
		}
        }
        p++;
        if (!strcmp( p, "all" )) p = "";  /* empty string means all */
        wine_dbg_add_option( p, set, clear );
        opt = strtok( NULL, "," );
    } while(opt);

    free( options );
    return;

 error:
    MESSAGE("wine: Syntax: --debugmsg [class]+xxx,...  or "
            "-debugmsg [class]-xxx,...\n");
    MESSAGE("Example: --debugmsg +all,warn-heap\n"
            "  turn on all messages except warning heap messages\n");
    MESSAGE("Available message classes:\n");
    for( i = 0; i < __WINE_DBCL_COUNT; i++) MESSAGE( "%-9s", debug_class_names[i] );
    MESSAGE("\n\n");
    ExitProcess(1);
}

static void remove_options( char *argv[], int pos, int count, int inherit )
{
    if (inherit)
    {
        int i, len = 0;
        for (i = 0; i < count; i++) len += strlen(argv[pos+i]) + 1;
        if (inherit_str)
        {
            if (!(inherit_str = realloc( inherit_str, strlen(inherit_str) + 1 + len )))
                out_of_memory();
            strcat( inherit_str, " " );
        }
        else
        {
            if (!(inherit_str = malloc( len ))) out_of_memory();
            inherit_str[0] = 0;
        }
        for (i = 0; i < count; i++)
        {
            strcat( inherit_str, argv[pos+i] );
            if (i < count-1) strcat( inherit_str, " " );
        }
    }
    while ((argv[pos] = argv[pos+count])) pos++;
}

/* parse options from the argv array and remove all the recognized ones */
static void parse_options( char *argv[] )
{
    const struct option_descr *opt;
    int i;

    for (i = 0; argv[i]; i++)
    {
        const char *equalarg = NULL;
        char *p = argv[i];
        if (*p++ != '-') continue;  /* not an option */
        if (*p && !p[1]) /* short name */
        {
            if (*p == '-') break; /* "--" option */
            for (opt = option_table; opt->longname; opt++) if (opt->shortname == *p) break;
        }
        else  /* long name */
        {
	    const char *equal = strchr  (p, '=');
            if (*p == '-') p++;
            /* check for the long name */
            for (opt = option_table; opt->longname; opt++) {
	        /* Plain --option */
                if (!strcmp( p, opt->longname )) break;

		/* --option=value */
		if (opt->has_arg &&
		    equal &&
		    strlen (opt->longname) == equal - p &&
		    !strncmp (p, opt->longname, equal - p)) {
		        equalarg = equal + 1;
		        break;
		    }
	    }
        }
        if (!opt->longname) continue;

	if (equalarg)
	{
            opt->func( equalarg );
            remove_options( argv, i, 1, opt->inherit );
	}
        else if (opt->has_arg && argv[i+1])
        {
            opt->func( argv[i+1] );
            remove_options( argv, i, 2, opt->inherit );
        }
        else
        {
            opt->func( "" );
            remove_options( argv, i, 1, opt->inherit );
        }
        i--;
    }
}

/* inherit options from WINEOPTIONS variable */
static void inherit_options( char *buffer )
{
    char *argv[256];
    unsigned int n;

    char *p = strtok( buffer, " \t" );
    for (n = 0; n < sizeof(argv)/sizeof(argv[0])-1 && p; n++)
    {
        argv[n] = p;
        p = strtok( NULL, " \t" );
    }
    argv[n] = NULL;
    parse_options( argv );
    if (argv[0])  /* an option remains */
    {
        MESSAGE( "Unknown option '%s' in WINEOPTIONS variable\n\n", argv[0] );
        OPTIONS_Usage();
    }
}

/***********************************************************************
 *              OPTIONS_Usage
 */
void OPTIONS_Usage(void)
{
    const struct option_descr *opt;
    MESSAGE( "%s\n\n", WINE_RELEASE_INFO );
    MESSAGE( "Usage: %s [options] [--] program_name [arguments]\n", argv0 );
    MESSAGE("The -- has to be used if you specify arguments (of the program)\n\n");
    MESSAGE( "Options:\n" );
    for (opt = option_table; opt->longname; opt++) MESSAGE( "   %s\n", opt->usage );
    ExitProcess(0);
}

/***********************************************************************
 *              OPTIONS_ParseOptions
 */
void OPTIONS_ParseOptions( char *argv[] )
{
    char buffer[1024];
    int i;

    if (GetEnvironmentVariableA( "WINEOPTIONS", buffer, sizeof(buffer) ) && buffer[0])
        inherit_options( buffer );

    parse_options( argv + 1 );

    SetEnvironmentVariableA( "WINEOPTIONS", inherit_str );

    /* check if any option remains */
    for (i = 1; argv[i]; i++)
    {
        if (!strcmp( argv[i], "--" ))
        {
            remove_options( argv, i, 1, 0 );
            break;
        }
        if (argv[i][0] == '-')
        {
            MESSAGE( "Unknown option '%s'\n\n", argv[i] );
            OPTIONS_Usage();
        }
    }
}

/***********************************************************************
 * get_config_key
 *
 * Get a config key from either the app-specific or the default config
 */
inline static DWORD get_config_key( HKEY defkey, HKEY appkey, const char *name,
                                    char *buffer, DWORD size )
{
    if (appkey && !RegQueryValueExA( appkey, name, 0, NULL, (LPBYTE)buffer,
                                     &size )) return 0;
    return RegQueryValueExA( defkey, name, 0, NULL, (LPBYTE)buffer, &size );
}


#ifndef SIZEOFARRAY
# define SIZEOFARRAY(x)  (sizeof(x) / sizeof((x)[0]))
#endif

/*
 * This function is a bit of a kludge to read command line modifications for particular applications
 * from the configuration file and apply them.
 * Presently, this only allows arguments to be added, but could be extended to include removing args.
 * 
 * NOTE: this function cannot be called until the heap functions have been initialized!
 */
char** modify_commandline_from_config( char* argv[], int* pargc )
{
    const char  keySuffix[] = "\\TransGaming";
    HKEY        hAppKey;
    char *      tmpBuf;
    char *      appname;
    char *      p1;
    char *      p2;
    HKEY        tmpkey;
    int         argc = *pargc;
    size_t      bufSize;


    if ( argc < 2 || !*argv[1] )
        return argv;

    /* argv[0] at this point is wine, and argv[1] should be the program. The exception
     * would be winelib applications.
     */
    bufSize = strlen(argv[1]) + SIZEOFARRAY(keySuffix) + 2;
    tmpBuf = HeapAlloc(GetProcessHeap(), 0, bufSize);
    
    /* couldn't allocate memory for the scratch buffer (??) => fail */
    if (tmpBuf == NULL){
        ERR("could not allocate %d bytes for a scratch buffer\n", bufSize);
        
        return argv;
    }
    
    
    strncpy(tmpBuf, argv[1], bufSize);
    appname = tmpBuf;


    /* find the last path separator */    
    p1 = strrchr(appname, '/');
    p2 = strrchr(appname, '\\');
    
    if (p1 && p2)
        appname = (p1 > p2 ? p1 : p2) + 1;
    
    else if (p1 || p2)
        appname = (p1 ? p1 : p2) + 1;
  
    
    /* add the suffix to the key name */
    strcat(appname, keySuffix);

    /* make sure the buffer is terminated */
    tmpBuf[bufSize - 1] = 0;
    
    
    if (!RegOpenKeyA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\AppDefaults", &tmpkey ))
    {
        char buffer[1024];


        if (!RegOpenKeyA( tmpkey, appname, &hAppKey ))
        {
            if( !get_config_key( tmpkey, hAppKey, "cmdlineadd", buffer, sizeof(buffer) ) )
            {
                char ** args = NULL;
                char *  newarg = NULL;
                int     len;


                buffer[SIZEOFARRAY(buffer) - 1] = 0;
	            
                /* FIXME: this will not properly handle arguments that have internal spaces (ie: quoted args).
                          A proper quote-aware string tokenizer needs to be created to handle this. */
                newarg = strtok(buffer, " \t");
                
                /* no arguments were actually specified (??) => fail */
                if (newarg == NULL)
                    return argv;
                
                
                do
                {
                    if (strchr(newarg, '\"') || strchr(newarg, '\''))
                        ERR("The argument %s is either quoted or contains internal quotes.  Currently these are not handled correctly\n", debugstr_a(newarg));

                    if (args == NULL)
                    {
                        args = HeapAlloc(GetProcessHeap(), 0, (argc + 2) * sizeof(*argv) );
                        
                        if (args == NULL){
                            ERR("could not allocate %d bytes for the argv list\n", (argc + 2) * sizeof(*argv));
                            args = argv;
                            
                            break;
                        }
                        
                        memcpy( args, argv, sizeof( *argv ) * argc );
                    }
                    else{
                        args = HeapReAlloc(GetProcessHeap(), 0, args, (argc + 2) * sizeof(*argv) );
                        
                        if (args == NULL){
                            ERR("could not allocate %d bytes to resize the argv list\n", (argc + 2) * sizeof(*argv));
                            args = argv;
                            
                            break;
                        }
                    }
                     
                    len = strlen( newarg ) + 1;
                
                    TRACE( "Adding %s to cmdline for %s in slot %d\n", debugstr_a(newarg), argv[0], argc );
                    
                    args[argc] = HeapAlloc(GetProcessHeap(), 0, len);
                    
                    if (args[argc] == NULL){
                        ERR("could not allocate %d bytes for the new argument %s\n", len, debugstr_a(newarg));
                        args = argv;
                        
                        break;
                    }


                    strncpy( args[argc], newarg, len);
                    args[argc + 1] = NULL;
                    argc++;

                    *pargc += 1;
                } while ((newarg = strtok(NULL, " \t")));

                RegCloseKey( hAppKey );
                RegCloseKey( tmpkey );
                
                HeapFree(GetProcessHeap(), 0, tmpBuf);

                return args;
            }
            RegCloseKey( hAppKey );
       }
       RegCloseKey( tmpkey );
    }


    HeapFree(GetProcessHeap(), 0, tmpBuf);
    
    return argv;
}
