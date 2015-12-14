/*
 * Command-line options.
 *
 * Copyright 1994 Alexandre Julliard
 */

#ifndef __WINE_OPTIONS_H
#define __WINE_OPTIONS_H

#include "windef.h"
#include "wine/file.h"

struct options
{
    int  managed;	  /* Managed windows */
    int  starting_cwd;    /* whether we have a user-set Windows CWD or not */
    int  monitor_cdrom_eject; /* whether we should monitor for cdrom eject requests */
    char cwd_to_use[MAX_PATHNAME_LEN+1]; /* The actual windows cwd to set */
};

extern struct options Options;
extern const char *argv0;
extern const char *full_argv0;
extern unsigned int server_startticks;

extern void OPTIONS_Usage(void) WINE_NORETURN;
extern void OPTIONS_ParseOptions( char *argv[] );

/* Profile functions */

extern int PROFILE_LoadWineIni(void);
extern void PROFILE_UsageWineIni(void);
extern int PROFILE_GetWineIniString( const char *section, const char *key_name,
                                     const char *def, char *buffer, int len );
extern BOOL PROFILE_EnumWineIniString( const char *section, int index,
                                       char *name, int name_len, char *buffer, int len );
extern int PROFILE_GetWineIniInt( const char *section, const char *key_name, int def );
extern int PROFILE_GetWineIniBool( char const *section, char const *key_name, int def );

/* Version functions */
extern void VERSION_ParseWinVersion( const char *arg );
extern void VERSION_ParseDosVersion( const char *arg );

/* Dir functions */
extern void DIR_OptionSetDosCWD( const char *arg );

/* Environment functions */
extern void ENV_CmdlinePassthrough( const char *arg );

#endif  /* __WINE_OPTIONS_H */
