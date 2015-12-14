#include "config.h"

#include <stdio.h>

#include "winbase.h"
#include "winreg.h"
#include "win.h"
#include "wine/debug.h"
#include <winnt.h>
#include "winver.h"
#include "thread.h"
#include "dbghelp.h"
#include "psapi.h"
#include "wine/unhandledException.h"
#include <stdarg.h>


WINE_DEFAULT_DEBUG_CHANNEL(seh);


/* struct used by the module enumeration callback function */
typedef struct{
    HANDLE  process;
    HANDLE  outFile;
} EnumModulesData;


/* dbghelp functions used in this file */
typedef BOOL    (WINAPI *SymInitialize_Func)(HANDLE, PCSTR, BOOL);
typedef DWORD   (WINAPI *SymGetOptions_Func)(void);
typedef DWORD   (WINAPI *SymSetOptions_Func)(DWORD);
typedef PVOID   (WINAPI *SymFunctionTableAccess_Func)(HANDLE, DWORD);
typedef BOOL    (WINAPI *SymFromAddr_Func)(HANDLE, DWORD64, DWORD64*, SYMBOL_INFO*);
typedef DWORD   (WINAPI *SymGetModuleBase_Func)(HANDLE, DWORD);
typedef BOOL    (WINAPI *SymEnumerateModules_Func)(HANDLE, PSYM_ENUMMODULES_CALLBACK, PVOID);
typedef BOOL    (WINAPI *SymGetModuleInfo_Func)(HANDLE, DWORD, PIMAGEHLP_MODULE);
typedef BOOL    (WINAPI *SymCleanup_Func)(HANDLE);
typedef BOOL    (WINAPI *StackWalk_Func)( DWORD, HANDLE, HANDLE, LPSTACKFRAME, PVOID,
                                          PREAD_PROCESS_MEMORY_ROUTINE,
                                          PFUNCTION_TABLE_ACCESS_ROUTINE,
                                          PGET_MODULE_BASE_ROUTINE,
                                          PTRANSLATE_ADDRESS_ROUTINE);
typedef BOOL    (WINAPI *MiniDumpWriteDump_Func)( HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                                  const PMINIDUMP_EXCEPTION_INFORMATION,
                                                  const PMINIDUMP_USER_STREAM_INFORMATION,
                                                  const PMINIDUMP_CALLBACK_INFORMATION);

/* psapi functions used in this file */
typedef DWORD   (WINAPI *GetModuleBaseNameA_Func)(  HANDLE hProcess,
                                                    HMODULE hModule,
                                                    LPSTR lpBaseName,
                                                    DWORD nSize);


typedef struct {
    BOOL                        initialized;
    HMODULE                     module;

    SymInitialize_Func          SymInitialize;
    SymGetOptions_Func          SymGetOptions;
    SymSetOptions_Func          SymSetOptions;
    SymFunctionTableAccess_Func SymFunctionTableAccess;
    SymFromAddr_Func            SymFromAddr;
    SymGetModuleBase_Func       SymGetModuleBase;
    SymEnumerateModules_Func    SymEnumerateModules;
    SymGetModuleInfo_Func       SymGetModuleInfo;
    SymCleanup_Func             SymCleanup;
    StackWalk_Func              StackWalk;
    MiniDumpWriteDump_Func      MiniDumpWriteDump;
} DBGHELP_FUNCTIONS;

typedef struct{
    BOOL                        initialized;
    HMODULE                     module;
    
    GetModuleBaseNameA_Func     GetModuleBaseNameA;
} PSAPI_FUNCTIONS;


static DBGHELP_FUNCTIONS    g_dbghelp = {FALSE, 0};
static PSAPI_FUNCTIONS      g_psapi = {FALSE, 0};


/* initFunctions(): grabs the function pointers for the dbghelp and psapi functions that
     are used in this file.  This allows us to dynamically load these libraries instead
     of always linking to them. */
static BOOL initFunctions(){
#define CHECKFUNC(set, name) \
            if ((set).name == NULL){ \
                ERR("could not get the address for the function " #set "." #name "()\n"); \
                FreeLibrary((set).module); \
                memset(&(set), 0, sizeof(set)); \
                (set).initialized = FALSE; \
                return FALSE; \
            }
#define GETFUNC(set, name)  { \
                (set).name = (name##_Func)GetProcAddress((set).module, #name); \
                CHECKFUNC(set, name) \
            }


    if (!g_dbghelp.initialized){
        g_dbghelp.module = LoadLibraryA("dbghelp.dll");

        if (!g_dbghelp.module){
            ERR("could not load 'dbghelp.dll'\n");

            return FALSE;
        }


        GETFUNC(g_dbghelp, SymInitialize);
        GETFUNC(g_dbghelp, SymGetOptions);
        GETFUNC(g_dbghelp, SymSetOptions);
        GETFUNC(g_dbghelp, SymFunctionTableAccess);
        GETFUNC(g_dbghelp, SymFromAddr);
        GETFUNC(g_dbghelp, SymGetModuleBase);
        GETFUNC(g_dbghelp, SymEnumerateModules);
        GETFUNC(g_dbghelp, SymGetModuleInfo);
        GETFUNC(g_dbghelp, SymCleanup);
        GETFUNC(g_dbghelp, StackWalk);
        GETFUNC(g_dbghelp, MiniDumpWriteDump);

        g_dbghelp.initialized = TRUE;
    }

    if (!g_psapi.initialized){
        g_psapi.module = LoadLibraryA("psapi.dll");

        if (!g_psapi.module){
            ERR("could not load 'psapi.dll'\n");

            return FALSE;
        }


        GETFUNC(g_psapi, GetModuleBaseNameA);


        g_psapi.initialized = TRUE;
    }

#undef GETFUNC
#undef CHECKFUNC

    return TRUE;
}


/* Fprintf(): helper function to print formatted ANSI text to a windows file handle */
static void Fprintf(HANDLE hFile, const char *fmt, ...){
    va_list     args;
    static char buffer[4096];
    size_t      len;


    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), fmt, args);
    va_end(args);


    /* buffer overflowed => just print what made it into the buffer and report an error */
    if (len < 0){
        ERR("buffer overflow!\n");

        len = sizeof(buffer) / sizeof(buffer[0]);
    }


    WriteFile(hFile, buffer, len, NULL, NULL);
}


/* enumModulesCallback(): callback function for the module enumeration.  This function just
     writes information about each found module to the crash report log. */
static BOOL CALLBACK enumModulesCallback(PSTR ModuleName, ULONG BaseOfDll, PVOID UserContext){
    EnumModulesData *   data = (EnumModulesData *)UserContext;
    IMAGEHLP_MODULE     modInfo;
    BOOL                badInfo = FALSE;

    
    modInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE);

    if (!g_dbghelp.SymGetModuleInfo(data->process, BaseOfDll, &modInfo)){
        ERR("could not retrieve the information for the module '%s'\n", ModuleName);

        /* couldn't find the module information => set its size to 1 */
        modInfo.ImageSize = 1;
        badInfo = TRUE;
    }


    Fprintf(data->outFile, 
            "   0x%08lx -> 0x%08lx   '%s'%s\n", 
            BaseOfDll, 
            BaseOfDll + modInfo.ImageSize - 1, 
            ModuleName,
            badInfo ? " (could not get module size)" : "");

    return TRUE;
}



static BOOL doMiniDump(struct _EXCEPTION_POINTERS *exceptionInfo, const char *filename){
    HANDLE                          hFile;
    MINIDUMP_EXCEPTION_INFORMATION  mdiInfo;
    BOOL                            retCode;


    TRACE("writing a minidump to the file '%s'\n", filename);

    hFile = CreateFileA(filename, 
                        FILE_ALL_ACCESS, 
                        0, 
                        NULL, 
                        CREATE_ALWAYS, 
                        FILE_ATTRIBUTE_NORMAL, 
                        0);


    if (hFile == INVALID_HANDLE_VALUE){
        ERR("could not open the minidump file '%s'\n", filename);

        return FALSE;
    }
    

    mdiInfo.ThreadId = GetCurrentThreadId();
    mdiInfo.ExceptionPointers = exceptionInfo;
    mdiInfo.ClientPointers = FALSE;


    /* this method comes from the windows 'dbghelp.dll' and 
       does all the work to write out the minidump file */
    retCode = g_dbghelp.MiniDumpWriteDump(	GetCurrentProcess(),
                                            GetCurrentProcessId(),
                                            hFile,
                                            MiniDumpNormal,
                                            &mdiInfo,
                                            NULL,
                                            NULL);

    TRACE("minidump complete {retCode = %s}\n", retCode ? "TRUE" : "FALSE");

    CloseHandle(hFile);


    return (retCode == TRUE);
}


static BOOL doStackWalk(struct _EXCEPTION_POINTERS *exceptionInfo, const char *filename){
    const int                   MAX_STACK_DEPTH = 128;
    DWORD                       code = exceptionInfo->ExceptionRecord->ExceptionCode;
    const EXCEPTION_POINTERS *  ptrs = exceptionInfo;
    HANDLE                      crashReport;
    STACKFRAME                  stackFrame;
    time_t                      curTime;
    DWORD                       symOptCurr;
    int                         i;
    int                         depth;
    BOOL                        dbghelpInitialized;
    EnumModulesData             enumModulesData;


    TRACE("writing the crash report log to the file '%s'\n", filename);


    crashReport = CreateFileA(  filename, 
                                FILE_ALL_ACCESS, 
                                0, 
                                NULL, 
                                CREATE_ALWAYS, 
                                FILE_ATTRIBUTE_NORMAL, 
                                0);


    /* couldn't open the file => fail */
    if (!crashReport){
        ERR("could not open the crash report file '%s'\n", filename);

        return FALSE;
    }

    
    /* write the crash timestamp to the log */
    curTime = time(NULL);

    Fprintf(crashReport, "[------- Crash Report -------]\n\n");
    Fprintf(crashReport, "   Created: %s", ctime(&curTime));


    /* write some simple exception information to the log */
    switch (code){
        case EXCEPTION_ACCESS_VIOLATION:
            Fprintf(crashReport,
                    "   Exception 0x%08lx occurred at 0x%p: Access violation %s location 0x%08lx\n",
                    code,
                    ptrs->ExceptionRecord->ExceptionAddress,
                    ptrs->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                    ptrs->ExceptionRecord->ExceptionInformation[1]);

            break;

        default:
            Fprintf(crashReport,
                    "   Exception 0x%08lx occurred at 0x%p {flags = 0x%08lx}",
                    code,
                    ptrs->ExceptionRecord->ExceptionAddress,
                    ptrs->ExceptionRecord->ExceptionFlags);

            for (i = 0; i < ptrs->ExceptionRecord->NumberParameters; i++)
                Fprintf(crashReport, "      info[%02d] = 0x%08lx\n", i, ptrs->ExceptionRecord->ExceptionInformation[i]);

            break;
    }

    /************************************************************************/
    FlushFileBuffers( crashReport );
    Fprintf(crashReport, "\n[------- Process Info -------]\n\n");


    /* write some basic process information about the crash */
    Fprintf(crashReport,
            "   PID %ld crashed in TID %ld\n\n",
            GetCurrentProcessId(),
            GetCurrentThreadId());
    

    
    /* dump a list of all the loaded modules and their base addresses to the log */

    /* initialize the dbghelp symbol system now so that it gathers the 
       module information for us.  We'll still have to enumerate it again
       after this is done, but we may as well get this step out of the
       way first. */
    enumModulesData.process = GetCurrentProcess();
    enumModulesData.outFile = crashReport;

    dbghelpInitialized = g_dbghelp.SymInitialize(enumModulesData.process, NULL, TRUE);


    /* write a table header for the modules list */
    Fprintf(crashReport, "   Base          End          Name\n");

    if (!g_dbghelp.SymEnumerateModules(enumModulesData.process, enumModulesCallback, &enumModulesData)){
        ERR("could not enumerate the loaded modules\n");

        Fprintf(crashReport, "   ERROR-> could not enumerate loaded modules\n");
    }


    /************************************************************************/
    FlushFileBuffers( crashReport );


    /* Print out the values of the main registers */
    Fprintf(crashReport,
            "\n[-------- Registers ---------]\n\n"
            "   EIP=%08lx EAX=%08lx EBX=%08lx ECX=%08lx EDX=%08lx\n"
            "   EBP=%08lx ESP=%08lx EDI=%08lx ESI=%08lx EFL=%08lx\n",
            ptrs->ContextRecord->Eip,
            ptrs->ContextRecord->Eax,
            ptrs->ContextRecord->Ebx,
            ptrs->ContextRecord->Ecx,
            ptrs->ContextRecord->Edx,
            ptrs->ContextRecord->Ebp,
            ptrs->ContextRecord->Esp,
            ptrs->ContextRecord->Edi,
            ptrs->ContextRecord->Esi,
            ptrs->ContextRecord->EFlags);


    /************************************************************************/
    FlushFileBuffers( crashReport );
    Fprintf(crashReport, "\n[------- Stack trace --------]\n\n");


    /* print out a stack trace for the exception point */
    ZeroMemory(&stackFrame, sizeof(stackFrame));

    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrPC.Offset = (int)( ptrs->ExceptionRecord->ExceptionAddress );

    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = ptrs->ContextRecord->Ebp;

    symOptCurr = g_dbghelp.SymGetOptions();
    g_dbghelp.SymSetOptions( symOptCurr | SYMOPT_EXACT_SYMBOLS );


    if (dbghelpInitialized){
        for (depth = 0; depth < MAX_STACK_DEPTH; depth++){
            char *          symbuf[sizeof(SYMBOL_INFO) + 1024 + 1];
            SYMBOL_INFO *   symbolInfo = (SYMBOL_INFO*)symbuf;
            DWORD64         disp64;
            BOOL            result;


            memset(symbuf, 0, sizeof(symbuf));

            symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbolInfo->MaxNameLen   = 1023;


            result = g_dbghelp.StackWalk(  IMAGE_FILE_MACHINE_I386,
                                           GetCurrentProcess(),
                                           GetCurrentThread(),
                                           &stackFrame,
                                           ptrs->ContextRecord,
                                           NULL,
                                           g_dbghelp.SymFunctionTableAccess,
                                           g_dbghelp.SymGetModuleBase,
                                           NULL);


            /* break out on error */
            if (result == FALSE)
                break;

            disp64 = 0;


            /* take the symbol address and get its readable name */
            if (g_dbghelp.SymFromAddr(  GetCurrentProcess(),
                                        stackFrame.AddrPC.Offset,
                                        &disp64,
                                        symbolInfo))
            {
                /* output the symbol */
                Fprintf(crashReport, "   %s() {0x%llx + 0x%llx, modBase = 0x%llx}\n",
                                        symbolInfo->Name,
                                        symbolInfo->Address,
                                        disp64,
                                        symbolInfo->ModBase);
            }

            /* couldn't get the symbol name => output its address and module info instead */
            else{
                DWORD64 base;
                char    module[MAX_PATH] = {0};


                base = g_dbghelp.SymGetModuleBase(GetCurrentProcess(), stackFrame.AddrPC.Offset);
                g_psapi.GetModuleBaseNameA(GetCurrentProcess(), (HMODULE)base, module, MAX_PATH);

                WARN("could not retrieve symbol information {AddrPC = 0x%lx, module = 0x%llx + 0x%llx ('%s')}\n", stackFrame.AddrPC.Offset, base, stackFrame.AddrPC.Offset - base, module);

                Fprintf(crashReport, 
                        "   no symbol information {AddrPC = 0x%lx, module = 0x%llx + 0x%llx ('%s')}\n", 
                        stackFrame.AddrPC.Offset, 
                        base, 
                        stackFrame.AddrPC.Offset - base, module);
            }
        }

        g_dbghelp.SymCleanup( GetCurrentProcess() );
    }

    else{
        ERR("could not initialize the symbol management system\n");

        Fprintf( crashReport, "SymInitialize() failure\n" );
    }

    /************************************************************************/
    Fprintf(crashReport, "\n[--- End of Crash Report ----]\n\n");

    CloseHandle(crashReport);


    return TRUE;
}


BOOL dumpExceptionInfo(struct _EXCEPTION_POINTERS *exceptionInfo, const char *stackTraceFile, const char *minidumpFile){
    static BOOL firstException = TRUE;
    int         i;


    TRACE("caught an exception {code = 0x%08lx}\n", exceptionInfo->ExceptionRecord->ExceptionCode);


    switch (exceptionInfo->ExceptionRecord->ExceptionCode){
        case STATUS_ACCESS_VIOLATION:
            TRACE("     type =      ACCESS_VIOLATION\n");
            TRACE("     EIP =       %p\n", exceptionInfo->ExceptionRecord->ExceptionAddress);
            TRACE("     address =   0x%08lx\n", exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
            TRACE("     access =    %s\n",     exceptionInfo->ExceptionRecord->ExceptionInformation[0] ? "WRITE" : "READ");

            break;

        default:
            TRACE("     code =      0x%08lx\n", exceptionInfo->ExceptionRecord->ExceptionCode);
            TRACE("     EIP =       %p\n", exceptionInfo->ExceptionRecord->ExceptionAddress);
            TRACE("     infoCount = %ld\n",     exceptionInfo->ExceptionRecord->NumberParameters);

            for (i = 0; i < (signed)exceptionInfo->ExceptionRecord->NumberParameters; i++)
                TRACE("     info[%2d] = 0x%08lx\n", i, exceptionInfo->ExceptionRecord->ExceptionInformation[i]);

            break;
    }


    if (!firstException){
        ERR("another exception has occurred while dumping debug info\n");

        return FALSE;
    }


    firstException = FALSE;


    /* grab the dbghelp functions that we need */
    if (!initFunctions()){
        ERR("could not load the dbghelp functions!  Bailing...\n");

        return FALSE;
    }



    /* perform the minidump */
    TRACE("writing the minidump file to '%s'\n", minidumpFile);
    doMiniDump(exceptionInfo, minidumpFile);

    /* perform the stack walk */
    TRACE("writing the crash report file to '%s'\n", stackTraceFile);
    doStackWalk(exceptionInfo, stackTraceFile);

    return TRUE;
}


LONG WINAPI filterFunction(struct _EXCEPTION_POINTERS *exceptionInfo){
    dumpExceptionInfo(exceptionInfo, "crashReport.txt", "minidump.dat");

    return 1; /*EXCEPTION_EXECUTE_HANDLER;*/
}


BOOL setupHandler(){
    SetUnhandledExceptionFilter(filterFunction);

    return TRUE;
}
