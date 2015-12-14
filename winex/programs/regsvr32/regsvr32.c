/*
 * PURPOSE: Register an OLE component in the registry
 *
 * Copyright 2001 ReactOS project
 * Copyright 2001 Jurgen Van Gael [jurgen.vangael@student.kuleuven.ac.be]
 * Copyright 2002 Andriy Palamarchuk
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *
 *  regsvr32 [/u] [/s] [/n] [/i[:cmdline]] dllname
 *  [/u]    unregister server
 *  [/s]    silent (no message boxes)
 *  [/i]    Call DllInstall passing it an optional [cmdline];
 *          when used with /u calls dll uninstall.
 *  [/n]    Do not call DllRegisterServer; this option must be used with [/i]
 */

/**
 * FIXME - currently receives command-line parameters in ASCII only and later
 * converts to Unicode. Ideally the function should have wWinMain etry point
 * and then work in Unicode only, but it seems Wine does not have necessary
 * support.
 */

#include "config.h"
#include "wine/port.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef HRESULT (*DLLREGISTER)          (void);
typedef HRESULT (*DLLUNREGISTER)        (void);
typedef HRESULT (*DLLINSTALL)           (BOOL,LPCWSTR);

int Silent = 0;

int Usage()
{
    printf("regsvr32 [/u] [/s] [/n] [/i[:cmdline]] dllname\n");
    printf("\t[/u]  unregister server\n");
    printf("\t[/s]  silent (no message boxes)\n");
    printf("\t[/i]  Call DllInstall passing it an optional [cmdline];\n");
    printf("\t      when used with /u calls dll uninstall\n");
    printf("\t[/n]  Do not call DllRegisterServer; this option "
           "must be used with [/i]\n");
    return 0;
}

/**
 * Loads procedure.
 *
 * Parameters:
 * strDll - name of the dll.
 * procName - name of the procedure to load from dll
 * pDllHanlde - output variable receives handle of the loaded dll.
 */
VOID *LoadProc(char* strDll, char* procName, HMODULE* DllHandle)
{
    VOID* (*proc)(void);

    *DllHandle = LoadLibrary(strDll);
    if(!*DllHandle)
    {
        if(!Silent)
            printf("Dll %s not found\n", strDll);

        exit(-1);
    }
    proc = (VOID *) GetProcAddress(*DllHandle, procName);
    if(!proc)
    {
        if(!Silent)
            printf("%s not implemented in dll %s\n", procName, strDll);
        FreeLibrary(*DllHandle);
        exit(-1);
    }
    return proc;
}

int RegisterDll(char* strDll)
{
    HRESULT hr;
    DLLREGISTER pfRegister;
    HMODULE DllHandle = NULL;

    pfRegister = LoadProc(strDll, "DllRegisterServer", &DllHandle);

    hr = pfRegister();
    if(FAILED(hr))
    {
        if(!Silent)
            printf("Failed to register dll %s\n", strDll);

        return -1;
    }
    if(!Silent)
        printf("Succesfully registered dll %s\n", strDll);

    if(DllHandle)
        FreeLibrary(DllHandle);
    return 0;
}

int UnregisterDll(char* strDll)
{
    HRESULT hr;
    DLLUNREGISTER pfUnregister;
    HMODULE DllHandle = NULL;

    pfUnregister = LoadProc(strDll, "DllUnregisterServer", &DllHandle);
    hr = pfUnregister();
    if(FAILED(hr))
    {
        if(!Silent)
            printf("Failed to unregister dll %s\n", strDll);

        return -1;
    }
    if(!Silent)
        printf("Succesfully unregistered dll %s\n", strDll);

    if(DllHandle)
        FreeLibrary(DllHandle);
    return 0;
}

int InstallDll(BOOL install, char *strDll, WCHAR *command_line)
{
    HRESULT hr;
    DLLINSTALL pfInstall;
    HMODULE DllHandle = NULL;

    pfInstall = LoadProc(strDll, "DllInstall", &DllHandle);
    hr = pfInstall(install, command_line);
    if(FAILED(hr))
    {
        if(!Silent)
            printf("Failed to %s dll %s\n", install ? "install" : "uninstall",
                   strDll);
        return -1;
    }
    if(!Silent)
        printf("Succesfully %s dll %s\n",  install ? "installed" : "uninstalled",
               strDll);

    if(DllHandle)
        FreeLibrary(DllHandle);
    return 0;
}

int main(int argc, char* argv[])
{
    int             i;
    BOOL            CallRegister = TRUE;
    BOOL            CallInstall = FALSE;
    BOOL            Unregister = FALSE;
    BOOL            DllFound = FALSE;
    WCHAR*          wsCommandLine = NULL;
    WCHAR           EmptyLine[1] = {0};


    for(i = 1; i < argc; i++)
    {
        if (!strcasecmp(argv[i], "/u"))
                Unregister = TRUE;
        else if (!strcasecmp(argv[i], "/s"))
                Silent = 1;
        else if (!strncasecmp(argv[i], "/i", strlen("/i")))
        {
            CHAR* command_line = argv[i] + strlen("/i");

            CallInstall = TRUE;
            if (command_line[0] == ':' && command_line[1])
            {
                int len = strlen(command_line);

                command_line++;
                len--;
                /* remove double quotes */
                if (command_line[0] == '"')
                {
                    command_line++;
                    len--;
                    if (command_line[0])
                    {
                        len--;
                        command_line[len] = 0;
                    }
                }
                if (command_line[0])
                {
                    len = MultiByteToWideChar(CP_ACP, 0, command_line, -1,
                                              NULL, 0);
                    wsCommandLine = HeapAlloc(GetProcessHeap(), 0,
                                              len * sizeof(WCHAR));
                    if (wsCommandLine)
                        MultiByteToWideChar(CP_ACP, 0, command_line, -1,
                                            wsCommandLine, len);
                }
                else
                {
                    wsCommandLine = EmptyLine;
                }
            }
            else
            {
                wsCommandLine = EmptyLine;
            }
        }
        else if(!strcasecmp(argv[i], "/n"))
            CallRegister = FALSE;
        else if (argv[i][0] == '/' && (!argv[i][2] || argv[i][2] == ':'))
            printf("Unrecognized switch %s\n", argv[i]);
        else
        {
            char *DllName = argv[i];
            int res = 0;

            DllFound = TRUE;
            if (!CallInstall || (CallInstall && CallRegister))
            {
                if(Unregister)
                    res = UnregisterDll(DllName);
                else
                    res = RegisterDll(DllName);
            }

            if (res)
                return res;

            if (CallInstall)
            {
                res = InstallDll(!Unregister, DllName, wsCommandLine);
            }

            return res;
        }
    }

    if (!DllFound)
    {
        if(!Silent)
            return Usage();
        else
            return -1;
    }

    return 0;
}
