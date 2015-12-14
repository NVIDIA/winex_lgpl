/*
 * 386-specific Win32 relay functions
 *
 * Copyright 1997 Alexandre Julliard
 */


#include "config.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "winnt.h"
#include "stackframe.h"
#include "module.h"
#include "wine/port.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(relay);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);

char **debug_relay_excludelist = NULL, **debug_relay_includelist = NULL;

/***********************************************************************
 *           RELAY_ShowDebugmsgRelay
 *
 * Simple function to decide if a particular debugging message is
 * wanted.  Called from RELAY_CallFrom32 and from in if1632/relay.c
 */
int RELAY_ShowDebugmsgRelay(const char *func) {

  if(debug_relay_excludelist || debug_relay_includelist) {
    const char *term = strchr(func, ':');
    char **listitem;
    int len, len2, itemlen, show;

    if(debug_relay_excludelist) {
      show = 1;
      listitem = debug_relay_excludelist;
    } else {
      show = 0;
      listitem = debug_relay_includelist;
    }
    assert(term);
    assert(strlen(term) > 2);
    len = term - func;
    len2 = strchr(func, '.') - func;
    assert(len2 && len2 > 0 && len2 < 64);
    term += 2;
    for(; *listitem; listitem++) {
      itemlen = strlen(*listitem);
      if((itemlen == len && !strncasecmp(*listitem, func, len)) ||
         (itemlen == len2 && !strncasecmp(*listitem, func, len2)) ||
         !strcasecmp(*listitem, term)) {
        show = !show;
       break;
      }
    }
    return show;
  }
  return 1;
}


#ifdef __i386__

typedef struct
{
    BYTE          call;                    /* 0xe8 call callfrom32 (relative) or 0xe9 call */
    DWORD         callfrom32 WINE_PACKED;  /* RELAY_CallFrom32 relative addr */
    BYTE          ret;                     /* 0xc2 ret $n  or  0xc3 ret */
    WORD          args;                    /* nb of args to remove from the stack */
    void         *orig;                    /* original entry point */
    DWORD         argtypes;                /* argument types */
} DEBUG_ENTRY_POINT;


/***********************************************************************
 *           find_exported_name
 *
 * Find the name of an exported function.
 */
static const char *find_exported_name( const char *module,
                                       IMAGE_EXPORT_DIRECTORY *exp, int ordinal )
{
    int i;
    const char *ret = NULL;

    WORD *ordptr = (WORD *)(module + exp->AddressOfNameOrdinals);
    for (i = 0; i < exp->NumberOfNames; i++, ordptr++)
        if (*ordptr + exp->Base == ordinal) break;
    if (i < exp->NumberOfNames)
        ret = module + ((DWORD*)(module + exp->AddressOfNames))[i];
    return ret;
}


/***********************************************************************
 *           get_entry_point
 *
 * Get the name of the DLL entry point corresponding to a relay address.
 */
static void get_entry_point( char *buffer, DEBUG_ENTRY_POINT *relay )
{
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_EXPORT_DIRECTORY *exp = NULL;
    DEBUG_ENTRY_POINT *debug;
    char *base = NULL;
    const char *name;
    int ordinal = 0;
    WINE_MODREF *wm;

    /* First find the module */

    for (wm = MODULE_modref_list; wm; wm = wm->next)
    {
        if (!(wm->flags & WINE_MODREF_INTERNAL)) continue;
        base = (char *)wm->module;
        dir = &PE_HEADER(base)->OptionalHeader.DataDirectory[IMAGE_FILE_EXPORT_DIRECTORY];
        if (!dir->Size) continue;
        exp = (IMAGE_EXPORT_DIRECTORY *)(base + dir->VirtualAddress);
        debug = (DEBUG_ENTRY_POINT *)((char *)exp + dir->Size);
        if (debug <= relay && relay < debug + exp->NumberOfFunctions)
        {
            ordinal = relay - debug;
            break;
        }
    }

    /* Now find the function */

    if ((name = find_exported_name( base, exp, ordinal + exp->Base )))
        sprintf( buffer, "%s.%s", base + exp->Name, name );
    else
        sprintf( buffer, "%s.%ld", base + exp->Name, ordinal + exp->Base );
}


/***********************************************************************
 *           RELAY_PrintArgs
 */
static inline void RELAY_PrintArgs( int *args, int nb_args, unsigned int typemask )
{
    while (nb_args--)
    {
	if ((typemask & 3) && HIWORD(*args))
        {
	    if (typemask & 2)
	    	DPRINTF( "%08x %s", *args, debugstr_w((LPWSTR)*args) );
            else
	    	DPRINTF( "%08x %s", *args, debugstr_a((LPCSTR)*args) );
	}
        else DPRINTF( "%08x", *args );
        if (nb_args) DPRINTF( "," );
        args++;
        typemask >>= 2;
    }
}


typedef LONGLONG (*LONGLONG_CPROC)();
typedef LONGLONG (WINAPI *LONGLONG_FARPROC)();


/***********************************************************************
 *           call_cdecl_function
 */
static LONGLONG __dynamicstackalign call_cdecl_function( LONGLONG_CPROC func, int nb_args, const int *args )
{
    LONGLONG ret;
    switch(nb_args)
    {
    case 0: ret = func(); break;
    case 1: ret = func(args[0]); break;
    case 2: ret = func(args[0],args[1]); break;
    case 3: ret = func(args[0],args[1],args[2]); break;
    case 4: ret = func(args[0],args[1],args[2],args[3]); break;
    case 5: ret = func(args[0],args[1],args[2],args[3],args[4]); break;
    case 6: ret = func(args[0],args[1],args[2],args[3],args[4],
                       args[5]); break;
    case 7: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                       args[6]); break;
    case 8: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                       args[6],args[7]); break;
    case 9: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                       args[6],args[7],args[8]); break;
    case 10: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9]); break;
    case 11: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10]); break;
    case 12: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],
                        args[11]); break;
    case 13: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12]); break;
    case 14: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12],args[13]); break;
    case 15: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12],args[13],args[14]); break;
    case 16: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12],args[13],args[14],args[15]); break;
    default:
        ERR( "Unsupported nb of args %d\n", nb_args );
        assert(FALSE);
    }
    return ret;
}


/***********************************************************************
 *           call_stdcall_function
 */
static LONGLONG __dynamicstackalign call_stdcall_function( LONGLONG_FARPROC func, int nb_args, const int *args )
{
    LONGLONG ret;
    switch(nb_args)
    {
    case 0: ret = func(); break;
    case 1: ret = func(args[0]); break;
    case 2: ret = func(args[0],args[1]); break;
    case 3: ret = func(args[0],args[1],args[2]); break;
    case 4: ret = func(args[0],args[1],args[2],args[3]); break;
    case 5: ret = func(args[0],args[1],args[2],args[3],args[4]); break;
    case 6: ret = func(args[0],args[1],args[2],args[3],args[4],
                       args[5]); break;
    case 7: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                       args[6]); break;
    case 8: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                       args[6],args[7]); break;
    case 9: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                       args[6],args[7],args[8]); break;
    case 10: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9]); break;
    case 11: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10]); break;
    case 12: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],
                        args[11]); break;
    case 13: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12]); break;
    case 14: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12],args[13]); break;
    case 15: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12],args[13],args[14]); break;
    case 16: ret = func(args[0],args[1],args[2],args[3],args[4],args[5],
                        args[6],args[7],args[8],args[9],args[10],args[11],
                        args[12],args[13],args[14],args[15]); break;
    default:
        ERR( "Unsupported nb of args %d\n", nb_args );
        assert(FALSE);
    }
    return ret;
}


/***********************************************************************
 *           RELAY_CallFrom32
 *
 * Stack layout on entry to this function:
 *  ...      ...
 * (esp+12)  arg2
 * (esp+8)   arg1
 * (esp+4)   ret_addr
 * (esp)     return addr to relay code
 */
LONGLONG __dynamicstackalign RELAY_CallFrom32( int ret_addr, ... )
{
    LONGLONG ret;
    char buffer[80];
    BOOL ret64;

    int *args = &ret_addr + 1;
    /* Relay addr is the return address for this function */
    BYTE *relay_addr = (BYTE *)__builtin_return_address(0);
    DEBUG_ENTRY_POINT *relay = (DEBUG_ENTRY_POINT *)(relay_addr - 5); /* 5 is size of call instruction */
    WORD nb_args = relay->args / sizeof(int);

    get_entry_point( buffer, relay );

    if( TRACE_ON(timestamp) ) { DPRINTF( "%ld - ", NtGetTickCount() ); }
    DPRINTF( "%04lx:Call(%u) %s(",
             GetCurrentThreadId(),
	     (NtCurrentTeb()->uRelayLevel)++,
	     buffer );
    RELAY_PrintArgs( args, nb_args, relay->argtypes );
    DPRINTF( ") ret=%08x\n", ret_addr );
    ret64 = (relay->argtypes & 0x80000000) && (nb_args < 16);

    if (relay->ret == 0xc3) /* cdecl */
    {
        ret = call_cdecl_function( (LONGLONG_CPROC)relay->orig, nb_args, args );
    }
    else  /* stdcall */
    {
        ret = call_stdcall_function( (LONGLONG_FARPROC)relay->orig, nb_args, args );
    }

    if( TRACE_ON(timestamp) ) { DPRINTF( "%ld - ", NtGetTickCount() ); }
    if (ret64)
        DPRINTF( "%04lx:Ret (%u) %s() retval=%08x%08x ret=%08x\n",
                 GetCurrentThreadId(),
		 --(NtCurrentTeb()->uRelayLevel),
                 buffer, (UINT)(ret >> 32), (UINT)ret, ret_addr );
    else
        DPRINTF( "%04lx:Ret (%u) %s() retval=%08x ret=%08x\n",
                 GetCurrentThreadId(),
		 --(NtCurrentTeb()->uRelayLevel),
                 buffer, (UINT)ret, ret_addr );

    return ret;
}


/***********************************************************************
 *           RELAY_CallFrom32Regs
 *
 * Stack layout (esp is context->Esp, not the current %esp):
 *
 * ...
 * (esp+4) first arg
 * (esp)   return addr to caller
 * (esp-4) return addr to DEBUG_ENTRY_POINT
 * (esp-8) ptr to relay entry code for RELAY_CallFrom32Regs
 *  ...    >128 bytes space free to be modified (ensured by the assembly glue)
 */
void WINAPI RELAY_DoCallFrom32Regs( CONTEXT86 *context )
{
    char buffer[80];
    int* args;
    int args_copy[17];
    BYTE *entry_point;

    BYTE *relay_addr = *((BYTE **)context->Esp - 1);
    DEBUG_ENTRY_POINT *relay = (DEBUG_ENTRY_POINT *)(relay_addr - 5); /* 5 is size of call instruction */
    WORD nb_args = (relay->args & ~0x8000) / sizeof(int);

    /* remove extra stuff from the stack */
    context->Eip = stack32_pop(context);
    args = (int *)context->Esp;
    if (relay->ret == 0xc2) /* stdcall */
        context->Esp += nb_args * sizeof(int);

    assert(TRACE_ON(relay));

    entry_point = (BYTE *)relay->orig;
    assert( *entry_point == 0xe8 /* lcall */ );

    get_entry_point( buffer, relay );

    if( TRACE_ON(timestamp) ) { DPRINTF( "%ld - ", NtGetTickCount() ); }
    DPRINTF( "%04lx:Call(%u) %s(",
             GetCurrentThreadId(),
	     (NtCurrentTeb()->uRelayLevel)++,
	     buffer );
    RELAY_PrintArgs( args, nb_args, relay->argtypes );
    DPRINTF( ") ret=%08lx fs=%04lx\n", context->Eip, context->SegFs );

    DPRINTF(" eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx\n",
            context->Eax, context->Ebx, context->Ecx,
            context->Edx, context->Esi, context->Edi );
    DPRINTF(" ebp=%08lx esp=%08lx ds=%04lx es=%04lx gs=%04lx flags=%08lx\n",
            context->Ebp, context->Esp, context->SegDs,
            context->SegEs, context->SegGs, context->EFlags );

    /* Now call the real function */

    memcpy( args_copy, args, nb_args * sizeof(args[0]) );
    args_copy[nb_args] = (int)context;  /* append context argument */
    if (relay->ret == 0xc3) /* cdecl */
    {
        call_cdecl_function( *(LONGLONG_CPROC *)(entry_point + 5), nb_args+1, args_copy );
    }
    else  /* stdcall */
    {
        call_stdcall_function( *(LONGLONG_FARPROC *)(entry_point + 5), nb_args+1, args_copy );
    }

    if( TRACE_ON(timestamp) ) { DPRINTF( "%ld - ", NtGetTickCount() ); }
    DPRINTF( "%04lx:Ret (%u) %s() retval=%08lx ret=%08lx fs=%04lx\n",
             GetCurrentThreadId(),
	     --(NtCurrentTeb()->uRelayLevel),
             buffer, context->Eax, context->Eip, context->SegFs );

    DPRINTF(" eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx\n",
            context->Eax, context->Ebx, context->Ecx,
            context->Edx, context->Esi, context->Edi );
    DPRINTF(" ebp=%08lx esp=%08lx ds=%04lx es=%04lx gs=%04lx flags=%08lx\n",
            context->Ebp, context->Esp, context->SegDs,
            context->SegEs, context->SegGs, context->EFlags );
}

void WINAPI RELAY_CallFrom32Regs(void);
__ASM_GLOBAL_FUNC( RELAY_CallFrom32Regs,
                   "call " __ASM_NAME("__wine_call_from_32_regs") "\n\t"
                   ".long " __ASM_NAME("RELAY_DoCallFrom32Regs") ",0" );

/***********************************************************************
 *           RELAY_SetupDLL
 *
 * Setup relay debugging for a built-in dll.
 */
void RELAY_SetupDLL( const char *module )
{
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_EXPORT_DIRECTORY *exports;
    DEBUG_ENTRY_POINT *debug;
    DWORD *funcs;
    int i;
    const char *name, *dllname;

    dir = &PE_HEADER(module)->OptionalHeader.DataDirectory[IMAGE_FILE_EXPORT_DIRECTORY];
    if (!dir->Size) return;
    exports = (IMAGE_EXPORT_DIRECTORY *)(module + dir->VirtualAddress);
    debug = (DEBUG_ENTRY_POINT *)((char *)exports + dir->Size);
    funcs = (DWORD *)(module + exports->AddressOfFunctions);
    dllname = module + exports->Name;

    for (i = 0; i < exports->NumberOfFunctions; i++, funcs++, debug++)
    {
        int on = 1;
        char buffer[200];

        if (!debug->call) continue;  /* not a normal function */
        if (debug->call != 0xe8 && debug->call != 0xe9) break; /* not a debug thunk at all */

        if (!(name = find_exported_name( module, exports, i + exports->Base )))
           name = "Nameless";

        sprintf( buffer, "%s.%d: %s", dllname, i, name );
        on = RELAY_ShowDebugmsgRelay(buffer);

        if (on)
        {
            debug->call = 0xe8;  /* call relative */
            if (debug->args & 0x8000)  /* register func */
                debug->callfrom32 = (char *)RELAY_CallFrom32Regs - (char *)&debug->ret;
            else
                debug->callfrom32 = (char *)RELAY_CallFrom32 - (char *)&debug->ret;
        }
        else
        {
            debug->call = 0xe9;  /* jmp relative */
            debug->callfrom32 = (char *)debug->orig - (char *)&debug->ret;
        }

        debug->orig = (FARPROC)(module + (DWORD)*funcs);
        *funcs = (char *)debug - module;
    }
}

#else  /* __i386__ */

void RELAY_SetupDLL( const char *module )
{
}

#endif /* __i386__ */
