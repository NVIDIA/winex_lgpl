/*
 * wine browser linker
 *
 * Launches the required unix browser
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * David Hammerton <david@transgaming.com>
 *
 * parts take from sims installer and sims browser proxy.
*
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "config.h"

#define WINE_NOWINSOCK
#include <windows.h>
#undef WINE_NOWINSOCK


/* unix includes (i hope) */
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <unistd.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#include <strings.h>



typedef struct
{
    char *name;
    char *filename;
    char *exec;
} browser_data;

/* list shamelessly (mostly) stolen from sims installer */
static const browser_data init_browser_data[] =
{
      { "Firefox (/usr/bin)", "/usr/bin/firefox", "/usr/bin/firefox %s" },
      { "Firefox (/usr/local/bin)", "/usr/local/bin/firefox", "/usr/local/bin/firefox %s" },
      { "Mozilla-Firefox (/usr/bin)", "/usr/bin/mozilla-firefox", "/usr/bin/mozilla-firefox %s" },
      { "Mozilla-Firefox (/usr/local/bin)", "/usr/local/bin/mozilla-firefox", "/usr/local/bin/mozilla-firefox %s" },
      { "Mozilla (/usr/bin)", "/usr/bin/mozilla", "/usr/bin/mozilla %s" },
      { "Mozilla (/usr/local/bin)", "/usr/local/bin/mozilla", "/usr/local/bin/mozilla %s" },
      { "Konqueror (/usr/bin)", "/usr/bin/konqueror", "/usr/bin/konqueror %s" },
      { "Konqueror (/usr/local/bin)", "/usr/local/bin/konqueror", "/usr/local/bin/konqueror %s" },
      { "Galeon (/usr/bin)", "/usr/bin/galeon", "/usr/bin/galeon %s" },
      { "Galeon (/usr/local/bin)", "/usr/local/bin/galeon", "/usr/local/bin/galeon %s" },
      { "Opera (/usr/bin)", "/usr/bin/opera", "/usr/bin/opera -newpage %s" },
      { "Opera (/usr/local/bin)", "/usr/local/bin/opera", "/usr/local/bin/opera -newpage %s" },
      { "Netscape (/usr/bin)", "/usr/bin/netscape", "/usr/bin/netscape %s" },
      { "Netscape (/usr/X11R6/bin)", "/usr/X11R6/bin/netscape", "/usr/X11R6/bin/netscape %s" },
      { "Netscape (/usr/local/bin)", "/usr/local/bin/netscape", "/usr/local/bin/netscape %s" },
      { NULL, NULL }
};

int get_working_builtin_browser()
{
    int i;
    struct stat fs;
    for (i = 0; init_browser_data[i].name != NULL; i++)
    {
        if (stat(init_browser_data[i].filename, &fs) == 0) /* 0 === success */
            return i;
    }
    return -1;
}

BOOL get_browser_line(char *buffer, int buffer_len)
{
    HKEY hkey;
    buffer[0] = 0;
    char *tmp;
    int formatcount;

    /* this stuff kinda taken from shellink.c */
    /* need to get the winebrowser (eg, web browser used by wine */
    if (!GetEnvironmentVariableA("WINEBROWSER", buffer, buffer_len))
    {
        /* otherwise get from registry. the config file puts its entry
         * into the registry */
        if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Wine",
                           0, KEY_ALL_ACCESS, &hkey))
        {
            DWORD type, count = buffer_len;
            if (RegQueryValueExA(hkey, "Browser", 0, &type, buffer, &count)) buffer[0] = 0;
            RegCloseKey(hkey);
        }
    }
    if (!*buffer)
    {
        int i;
        fprintf(stderr, "WARNING: no browser entry found in config, will try to find the best match\n");
        if ((i = get_working_builtin_browser()) == -1)
            return FALSE;
        if (strlen(init_browser_data[i].exec) > buffer_len) return FALSE;
        strncpy(buffer, init_browser_data[i].exec, buffer_len);
    }

    tmp = buffer;
    formatcount = 0;
    while ((tmp = strchr(tmp, '%')))
    {
        /* remove all format chars apart from the first %s */
        if (formatcount || tmp[1] != 's')
        {
            tmp[0] = '\\';
        }
        else formatcount++;
        tmp++;
    }
    if (!formatcount)
    {
        /* append a " %s" onto the end of the browser line */
        const char *append = " %s";
        if (buffer_len < (strlen(buffer) + strlen(append) + 1))
            return FALSE;
        strcat(buffer, append);
    }

    return TRUE;
}

/* next two functions are for reading an Internet Shortcut file
 * evil things be here (yuk, quick and hacky file passing)
 */
static char *read_line(HANDLE file)
{
    char *buf;
    char *c;
    DWORD read;
    int n = 0;

    buf = malloc(1024);
    memset(buf, 0, 1024);

    c = buf;

    do
    {
        if (n >= 1023 || ReadFile(file, c, 1, &read, NULL) == 0 || read != 1)
        {
            free(buf);
            return NULL;
        }
        n++;
    } while (*(c++) != '\n');

    return buf;
}

char *get_url_from_urlfile(char *urlfile)
{
    HANDLE file;
    char *line = NULL;
    enum
    {
        SEARCHING_SECTION,
        SEARCHING_URL_ENTRY
    } expecting = SEARCHING_SECTION;

    file = CreateFile(urlfile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return NULL;

    while ((line = read_line(file)))
    {

        switch (expecting)
        {
        case SEARCHING_SECTION:
            if (line[0] == '[')
            {
                /* turn line from '[section]\r\n' into 'section' */
                char *section = line+1;
                char *c = section;
                while (*c && *c != ']') c++;
                if (!(*c)) goto bad;
                *c = '\0';

                if (strcasecmp(section, "InternetShortcut") == 0)
                {
                    expecting = SEARCHING_URL_ENTRY;
                }
            }
            break;
        case SEARCHING_URL_ENTRY:
        {
            char *split;
            /* replace = with \0. so we can do a standard strcasecmp.
             * not everyone has strncasecmp :(.
             */
            split = strchr(line, '=');
            *split = '\0';
            /* split now points to the value */
            split++;
            /* check if the key is 'url' */
            if (strcasecmp(line, "url") == 0)
            {
                char *url = NULL;
                char *c;
                /* put value into url */
                url = malloc(strlen(split)+1);
                strcpy(url, split);
                c = url;
                /* make sure it ends before a new line */
                while (*c && *c != '\r' && *c != '\n') c++;
                if (*c) *c = '\0';

                /* free line and return */
                free(line);
                CloseHandle(file);
                return url;
            }
            break;
        }
        }

        free(line);
        line = NULL;
    }

    CloseHandle(file);

bad:
    if (line) free(line);
    return NULL;
}

/* child death will be handled in wait*. */
void setup_sigchld()
{
    /* The default action is to ignore SIGCHLD. */
    struct sigaction sa;

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSTOP);
    sigaddset(&sa.sa_mask, SIGKILL);
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;

    sigaction(SIGCHLD, &sa, NULL);
}


#define MAX_ARGS 8
BOOL fork_exec_and_wait(char *browser_fmt, char *url)
{
    pid_t child;
    char buffer[MAX_PATH];
    char *cur, *next, *ins, *ptr;
    char *args[MAX_ARGS+1];
    int status, argc, len;

    argc = 0;
    ins = strstr(browser_fmt, "%s");
    cur = browser_fmt;
    ptr = buffer;
    while (argc < MAX_ARGS) {
        next = strchr(cur, ' ');
        if (!next) next = cur + strlen(cur);

        if (ins >= cur && ins < next) {
            /* it's the %s argument, I'm going to assume it's always its own parameter */
            args[argc++] = url;
        } else {
            /* anything else */
            args[argc++] = ptr;
            len = next - cur;
            if ((ptr + len - buffer) >= MAX_PATH)
                len = buffer + MAX_PATH - ptr - 1;
            memcpy(ptr, cur, len);
            ptr += len;
            *ptr++ = 0;
            if ((ptr - buffer) == MAX_PATH)
                break;
        }

        if (!*next) break;
        cur = next+1;
    }
    args[argc] = NULL;

    setup_sigchld();
    child = fork();
    if (child == -1)
    {
        return FALSE;
    }
    if (child == 0)
    { /* child */
        fprintf(stderr, "using browser %s\n", args[0]);
        unsetenv("TEMP"); /* mozilla doesn't like wines TEMP (or TMP) env variable */
        unsetenv("TMP");
        execvp(args[0], args);
        _exit(1);
    }
    /* parent */

    waitpid(child, &status, 0);

    return TRUE;
}

int CALLBACK WinMain (HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char buffer[MAX_PATH];
    BOOL ret;
    char *url = cmdline;

    /* we have a %1 in the registry that ShellExec uses, but if the app tries to run it
     * directly it may foul things up, so we'll just skip it */
    if ( strncmp("%1", url, 2) == 0 ) {
      url += 2;
    }

    ret = get_browser_line(buffer, sizeof(buffer));
    if (!ret)
    {
        fprintf(stderr, "ERROR: failed to find a browser. couldn't launch url '%s'\n",
                        url);
        return 0;
    }

    /* if we're launched with /urlfile it means the next parameter is the
     * windows style path to a .url Internet Shortcut. Have to read it
     * and extract URL
     */
    if (strncmp("/urlfile", url, 8) == 0)
    {
        char *old_url;
        url+=8;
        while (*url && *url == ' ') url++;
        old_url = url;
        url = get_url_from_urlfile(url);
        if (!url)
        {
            fprintf(stderr, "ERROR: failed to load URL file '%s'\n", old_url);
            return 0;
        }
        /* will leak url */
    }

    ret = fork_exec_and_wait(buffer, url);
    if (!ret)
    {
        fprintf(stderr, "ERROR: failed execute your browser (%s). couldn't launch url '%s'\n",
                        buffer, url);
        return 0;
    }

    return 1;
}

