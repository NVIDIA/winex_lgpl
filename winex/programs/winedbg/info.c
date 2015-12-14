/*
 * Wine debugger utility routines
 *
 * Copyright 1993 Eric Youngdale
 * Copyright 1995 Alexandre Julliard
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "tlhelp32.h"
#include "debugger.h"
#include "expr.h"

/***********************************************************************
 *           DEBUG_PrintBasic
 *
 * Implementation of the 'print' command.
 */
void DEBUG_PrintBasic( const DBG_VALUE* value, int count, char format )
{
    char        * default_format;
    long long int res;

    assert(value->cookie == DV_TARGET || value->cookie == DV_HOST);
    if (value->type == NULL)
    {
        DEBUG_Printf(DBG_CHN_MESG, "Unable to evaluate expression\n");
        return;
    }

    default_format = NULL;
    res = DEBUG_GetExprValue(value, &default_format);

    switch (format)
    {
    case 'x':
        if (value->addr.seg)
	{
            DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, "0x%04lx", (long unsigned int)res);
	}
        else
	{
            DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, "0x%08lx", (long unsigned int)res);
	}
        break;

    case 'd':
        DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, "%ld\n", (long int)res);
        break;

    case 'c':
        DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, "%d = '%c'",
                                    (char)(res & 0xff), (char)(res & 0xff));
        break;

    case 'u':
    {
        WCHAR wch = (WCHAR)(res & 0xFFFF);
        DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, "%d = '", (unsigned)(res & 0xffff));
        DEBUG_OutputW(DBG_CHN_MESG, &wch, 1);
        DEBUG_Printf(DBG_CHN_MESG, "'");
    }
    break;

    case 'i':
    case 's':
    case 'w':
    case 'b':
        DEBUG_Printf(DBG_CHN_MESG, "Format specifier '%c' is meaningless in 'print' command\n", format);
    case 0:
        if (default_format == NULL) break;

        if (strstr(default_format, "%S") != NULL)
        {
            char* 	ptr;
            int	state = 0;

            /* FIXME: simplistic implementation for default_format being
             * foo%Sbar => will print foo, then string then bar
             */
            for (ptr = default_format; *ptr; ptr++)
            {
                if (*ptr == '%')
                {
                    state++;
                }
                else if (state == 1)
                {
                    if (*ptr == 'S')
                    {
                        DBG_ADDR    addr;

                        addr.seg = 0;
                        addr.off = (long)res;
                        DEBUG_nchar += DEBUG_PrintStringA(DBG_CHN_MESG, &addr, -1);
                    }
                    else
                    {
                        /* shouldn't happen */
                        DEBUG_Printf(DBG_CHN_MESG, "%%%c", *ptr);
                        DEBUG_nchar += 2;
                    }
                    state = 0;
                }
                else
                {
                    DEBUG_OutputA(DBG_CHN_MESG, ptr, 1);
                    DEBUG_nchar++;
                }
            }
        }
        else if (strcmp(default_format, "%B") == 0)
        {
            DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, "%s", res ? "true" : "false");
        }
        else if (strcmp(default_format, "%R") == 0)
        {
            if (value->cookie == DV_HOST)
                DEBUG_InfoRegisters((CONTEXT*)value->addr.off);
            else
                DEBUG_Printf(DBG_CHN_MESG, "NIY: info on register struct in debuggee address space\n");
            DEBUG_nchar = 0;
        }
        else
        {
            DEBUG_nchar += DEBUG_Printf(DBG_CHN_MESG, default_format, res);
        }
        break;
    }
}


/***********************************************************************
 *           DEBUG_PrintAddress
 *
 * Print an 16- or 32-bit address, with the nearest symbol if any.
 */
struct symbol_info
DEBUG_PrintAddress( const DBG_ADDR *addr, enum dbg_mode mode, int flag )
{
    struct symbol_info rtn;

    const char *name = DEBUG_FindNearestSymbol( addr, flag, &rtn.sym, 0,
						&rtn.list );

    if (addr->seg) DEBUG_Printf( DBG_CHN_MESG, "0x%04lx:", addr->seg&0xFFFF );
    if (mode != MODE_32) DEBUG_Printf( DBG_CHN_MESG, "0x%04lx", addr->off );
    else DEBUG_Printf( DBG_CHN_MESG, "0x%08lx", addr->off );
    if (name) DEBUG_Printf( DBG_CHN_MESG, " (%s)", name );
    return rtn;
}
/***********************************************************************
 *           DEBUG_PrintAddressAndArgs
 *
 * Print an 16- or 32-bit address, with the nearest symbol if any.
 * Similar to DEBUG_PrintAddress, but we print the arguments to
 * each function (if known).  This is useful in a backtrace.
 */
struct symbol_info
DEBUG_PrintAddressAndArgs( const DBG_ADDR *addr, enum dbg_mode mode,
			   unsigned int ebp, int flag )
{
    struct symbol_info rtn;

    const char *name = DEBUG_FindNearestSymbol( addr, flag, &rtn.sym, ebp,
						&rtn.list );

    if (addr->seg) DEBUG_Printf( DBG_CHN_MESG, "0x%04lx:", addr->seg );
    if (mode != MODE_32) DEBUG_Printf( DBG_CHN_MESG, "0x%04lx", addr->off );
    else DEBUG_Printf( DBG_CHN_MESG, "0x%08lx", addr->off );
    if (name) DEBUG_Printf( DBG_CHN_MESG, " (%s)", name );

    return rtn;
}


/***********************************************************************
 *           DEBUG_Help
 *
 * Implementation of the 'help' command.
 */
void DEBUG_Help(void)
{
    int i = 0;
    static const char * const helptext[] =
{
"The commands accepted by the Wine debugger are a reasonable",
"subset of the commands that gdb accepts.",
"The commands currently are:",
"  help                                   quit",
"  break [*<addr>]                        watch *<addr>",
"  delete break bpnum                     disable bpnum",
"  enable bpnum                           condition <bpnum> [<expr>]",
"  finish                                 cont [N]",
"  step [N]                               next [N]",
"  stepi [N]                              nexti [N]",
"  x <addr>                               print <expr>",
"  display <expr>                         undisplay <disnum>",
"  delete display <disnum>                pass",
"  bt                                     frame <n>",
"  up                                     down",
"  list <lines>                           disassemble [<addr>][,<addr>]",
"  show dir                               dir <path>",
"  set <reg> = <expr>                     set *<addr> = <expr>",
"  mode [16,32,vm86]                      walk [wnd,class,queue,module,",
"  whatis                                       process,modref <pid>]",
"  info (see 'help info' for options)     debugmsg <class>[-+]<type>\n",

"The 'x' command accepts repeat counts and formats (including 'i') in the",
"same way that gdb does.\n",

" The following are examples of legal expressions:",
" $eax     $eax+0x3   0x1000   ($eip + 256)  *$eax   *($esp + 3)",
" Also, a nm format symbol table can be read from a file using the",
" symbolfile command.  Symbols can also be defined individually with",
" the define command.",
"",
NULL
};

    while(helptext[i]) DEBUG_Printf(DBG_CHN_MESG,"%s\n", helptext[i++]);
}


/***********************************************************************
 *           DEBUG_HelpInfo
 *
 * Implementation of the 'help info' command.
 */
void DEBUG_HelpInfo(void)
{
    int i = 0;
    static const char * const infotext[] =
{
"The info commands allow you to get assorted bits of interesting stuff",
"to be displayed.  The options are:",
"  info break           Dumps information about breakpoints",
"  info display         Shows auto-display expressions in use",
"  info locals          Displays values of all local vars for current frame",
"  info maps            Dumps all virtual memory mappings",
"  info module <handle> Displays internal module state",
"  info queue <handle>  Displays internal queue state",
"  info reg             Displays values in all registers at top of stack",
"  info segments        Dumps information about all known segments",
"  info share           Dumps information about shared libraries",
"  info stack           Dumps information about top of stack",
"  info wnd <handle>    Displays internal window state",
"",
NULL
};

    while(infotext[i]) DEBUG_Printf(DBG_CHN_MESG,"%s\n", infotext[i++]);
}

/* FIXME: merge InfoClass and InfoClass2 */
void DEBUG_InfoClass(const char* name)
{
   WNDCLASSEXA	wca;

   if (!GetClassInfoEx(0, name, &wca)) {
      DEBUG_Printf(DBG_CHN_MESG, "Cannot find class '%s'\n", name);
      return;
   }

   DEBUG_Printf(DBG_CHN_MESG,  "Class '%s':\n", name);
   DEBUG_Printf(DBG_CHN_MESG,
		"style=%08x  wndProc=%08lx\n"
		"inst=%p  icon=%p  cursor=%p  bkgnd=%p\n"
		"clsExtra=%d  winExtra=%d\n",
		wca.style, (DWORD)wca.lpfnWndProc, wca.hInstance,
		wca.hIcon, wca.hCursor, wca.hbrBackground,
		wca.cbClsExtra, wca.cbWndExtra);

   /* FIXME:
    * + print #windows (or even list of windows...)
    * + print extra bytes => this requires a window handle on this very class...
    */
}

static	void DEBUG_InfoClass2(HWND hWnd, const char* name)
{
   WNDCLASSEXA	wca;

   if (!GetClassInfoEx((HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), name, &wca)) {
      DEBUG_Printf(DBG_CHN_MESG, "Cannot find class '%s'\n", name);
      return;
   }

   DEBUG_Printf(DBG_CHN_MESG,  "Class '%s':\n", name);
   DEBUG_Printf(DBG_CHN_MESG,
		"style=%08x  wndProc=%08lx\n"
		"inst=%p  icon=%p  cursor=%p  bkgnd=%p\n"
		"clsExtra=%d  winExtra=%d\n",
		wca.style, (DWORD)wca.lpfnWndProc, wca.hInstance,
		wca.hIcon, wca.hCursor, wca.hbrBackground,
		wca.cbClsExtra, wca.cbWndExtra);

   if (wca.cbClsExtra) {
      int		i;
      WORD		w;

      DEBUG_Printf(DBG_CHN_MESG,  "Extra bytes:" );
      for (i = 0; i < wca.cbClsExtra / 2; i++) {
	 w = GetClassWord(hWnd, i * 2);
	 /* FIXME: depends on i386 endian-ity */
	 DEBUG_Printf(DBG_CHN_MESG,  " %02x", HIBYTE(w));
	 DEBUG_Printf(DBG_CHN_MESG,  " %02x", LOBYTE(w));
      }
      DEBUG_Printf(DBG_CHN_MESG,  "\n" );
    }
    DEBUG_Printf(DBG_CHN_MESG,  "\n" );
}

struct class_walker {
   ATOM*	table;
   int		used;
   int		alloc;
};

static	void DEBUG_WalkClassesHelper(HWND hWnd, struct class_walker* cw)
{
   char	clsName[128];
   int	i;
   ATOM	atom;
   HWND	child;

   if (!GetClassName(hWnd, clsName, sizeof(clsName)))
      return;
   if ((atom = FindAtom(clsName)) == 0)
      return;

   for (i = 0; i < cw->used; i++) {
      if (cw->table[i] == atom)
	 break;
   }
   if (i == cw->used) {
      if (cw->used >= cw->alloc) {
	 cw->alloc += 16;
	 cw->table = DBG_realloc(cw->table, cw->alloc * sizeof(ATOM));
      }
      cw->table[cw->used++] = atom;
      DEBUG_InfoClass2(hWnd, clsName);
   }
   do {
      if ((child = GetWindow(hWnd, GW_CHILD)) != 0)
	 DEBUG_WalkClassesHelper(child, cw);
   } while ((hWnd = GetWindow(hWnd, GW_HWNDNEXT)) != 0);
}

void DEBUG_WalkClasses(void)
{
   struct class_walker cw;

   cw.table = NULL;
   cw.used = cw.alloc = 0;
   DEBUG_WalkClassesHelper(GetDesktopWindow(), &cw);
   DBG_free(cw.table);
}

void DEBUG_DumpQueue(DWORD q)
{
   DEBUG_Printf(DBG_CHN_MESG, "No longer doing info queue '0x%08lx'\n", q);
}

void DEBUG_WalkQueues(void)
{
   DEBUG_Printf(DBG_CHN_MESG, "No longer walking queues list\n");
}

void DEBUG_InfoWindow(HWND hWnd)
{
   char	clsName[128];
   char	wndName[128];
   RECT	clientRect;
   RECT	windowRect;
   int	i;
   WORD	w;

   if (!GetClassName(hWnd, clsName, sizeof(clsName)))
       strcpy(clsName, "-- Unknown --");
   if (!GetWindowText(hWnd, wndName, sizeof(wndName)))
      strcpy(wndName, "-- Empty --");
   if (!GetClientRect(hWnd, &clientRect))
      SetRectEmpty(&clientRect);
   if (!GetWindowRect(hWnd, &windowRect))
      SetRectEmpty(&windowRect);

   /* FIXME missing fields: hmemTaskQ, hrgnUpdate, dce, flags, pProp, scroll */
   DEBUG_Printf(DBG_CHN_MESG,
		"next=%p  child=%p  parent=%p  owner=%p  class='%s'\n"
		"inst=%p  active=%p  idmenu=%08lx\n"
		"style=%08lx  exstyle=%08lx  wndproc=%08lx  text='%s'\n"
		"client=%d,%d-%d,%d  window=%d,%d-%d,%d sysmenu=%p\n",
		GetWindow(hWnd, GW_HWNDNEXT),
		GetWindow(hWnd, GW_CHILD),
		GetParent(hWnd),
		GetWindow(hWnd, GW_OWNER),
		clsName,
		(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE),
		GetLastActivePopup(hWnd),
		GetWindowLong(hWnd, GWL_ID),
		GetWindowLong(hWnd, GWL_STYLE),
		GetWindowLong(hWnd, GWL_EXSTYLE),
		GetWindowLong(hWnd, GWL_WNDPROC),
		wndName,
		clientRect.left, clientRect.top, clientRect.right, clientRect.bottom,
		windowRect.left, windowRect.top, windowRect.right, windowRect.bottom,
		GetSystemMenu(hWnd, FALSE));

    if (GetClassLong(hWnd, GCL_CBWNDEXTRA)) {
        DEBUG_Printf(DBG_CHN_MESG,  "Extra bytes:" );
        for (i = 0; i < GetClassLong(hWnd, GCL_CBWNDEXTRA) / 2; i++) {
	   w = GetWindowWord(hWnd, i * 2);
	   /* FIXME: depends on i386 endian-ity */
	   DEBUG_Printf(DBG_CHN_MESG, " %02x", HIBYTE(w));
	   DEBUG_Printf(DBG_CHN_MESG, " %02x", LOBYTE(w));
	}
        DEBUG_Printf(DBG_CHN_MESG, "\n");
    }
    DEBUG_Printf(DBG_CHN_MESG, "\n");
}

void DEBUG_WalkWindows(HWND hWnd, int indent)
{
   char	clsName[128];
   char	wndName[128];
   HWND	child;

   if (!IsWindow(hWnd))
      hWnd = GetDesktopWindow();

    if (!indent)  /* first time around */
       DEBUG_Printf(DBG_CHN_MESG,
		    "%-16.16s %-17.17s %-8.8s %s\n",
		    "hwnd", "Class Name", " Style", " WndProc Text");

    do {
       if (!GetClassName(hWnd, clsName, sizeof(clsName)))
	  strcpy(clsName, "-- Unknown --");
       if (!GetWindowText(hWnd, wndName, sizeof(wndName)))
	  strcpy(wndName, "-- Empty --");

       /* FIXME: missing hmemTaskQ */
       DEBUG_Printf(DBG_CHN_MESG, "%*s%04x%*s", indent, "", (UINT)hWnd, 13-indent,"");
       DEBUG_Printf(DBG_CHN_MESG, "%-17.17s %08lx %08lx %.14s\n",
		    clsName, GetWindowLong(hWnd, GWL_STYLE),
		    GetWindowLong(hWnd, GWL_WNDPROC), wndName);

       if ((child = GetWindow(hWnd, GW_CHILD)) != 0)
	  DEBUG_WalkWindows(child, indent + 1 );
    } while ((hWnd = GetWindow(hWnd, GW_HWNDNEXT)) != 0);
}

void DEBUG_WalkProcess(void)
{
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 entry;
        DWORD current = DEBUG_CurrProcess ? DEBUG_CurrProcess->pid : 0;
        BOOL ok;

        entry.dwSize = sizeof(entry);
        ok = Process32First( snap, &entry );

        DEBUG_Printf(DBG_CHN_MESG, " %-8.8s %-8.8s %-8.8s %s\n",
                     "pid", "threads", "parent", "executable" );
        while (ok)
        {
            if (entry.th32ProcessID != GetCurrentProcessId())
                DEBUG_Printf(DBG_CHN_MESG, "%c%08lx %-8ld %08lx '%s'\n",
                             (entry.th32ProcessID == current) ? '>' : ' ',
                             entry.th32ProcessID, entry.cntThreads,
                             entry.th32ParentProcessID, entry.szExeFile);
            ok = Process32Next( snap, &entry );
        }
        CloseHandle( snap );
    }
}

void DEBUG_WalkThreads(void)
{
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if (snap != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32	entry;
        DWORD 		current = DEBUG_CurrThread ? DEBUG_CurrThread->tid : 0;
        BOOL 		ok;
	DWORD		lastProcessId = 0;

	entry.dwSize = sizeof(entry);
	ok = Thread32First( snap, &entry );

        DEBUG_Printf(DBG_CHN_MESG, "%-8.8s %-8.8s %s\n", "process", "tid", "prio" );
        while (ok)
        {
            if (entry.th32OwnerProcessID != GetCurrentProcessId())
	    {
		/* FIXME: this assumes that, in the snapshot, all threads of a same process are
		 * listed sequentially, which is not specified in the doc (Wine's implementation
		 * does it)
		 */
		if (entry.th32OwnerProcessID != lastProcessId)
		{
		    DBG_PROCESS*	p = DEBUG_GetProcess(entry.th32OwnerProcessID);

		    DEBUG_Printf(DBG_CHN_MESG, "%08lx%s %s\n",
				 entry.th32OwnerProcessID,  p ? " (D)" : "", p ? p->imageName : "");
		    lastProcessId = entry.th32OwnerProcessID;
		}
                DEBUG_Printf(DBG_CHN_MESG, "\t%08lx %4ld%s\n",
                             entry.th32ThreadID, entry.tpBasePri,
			     (entry.th32ThreadID == current) ? " <==" : "");

	    }
            ok = Thread32Next( snap, &entry );
        }

        CloseHandle( snap );
    }
}

void DEBUG_WalkModref(DWORD p)
{
   DEBUG_Printf(DBG_CHN_MESG, "No longer walking module references list\n");
}

void DEBUG_InfoSegments(DWORD start, int length)
{
    char 	flags[3];
    DWORD 	i;
    LDT_ENTRY	le;

    if (length == -1) length = (8192 - start);

    for (i = start; i < start + length; i++)
    {
       if (!GetThreadSelectorEntry(DEBUG_CurrThread->handle, (i << 3)|7, &le))
	  continue;

        if (le.HighWord.Bits.Type & 0x08)
        {
            flags[0] = (le.HighWord.Bits.Type & 0x2) ? 'r' : '-';
            flags[1] = '-';
            flags[2] = 'x';
        }
        else
        {
            flags[0] = 'r';
            flags[1] = (le.HighWord.Bits.Type & 0x2) ? 'w' : '-';
            flags[2] = '-';
        }
        DEBUG_Printf(DBG_CHN_MESG,
		     "%04lx: sel=%04lx base=%08x limit=%08x %d-bit %c%c%c\n",
		     i, (i<<3)|7,
		     (le.HighWord.Bits.BaseHi << 24) +
		     (le.HighWord.Bits.BaseMid << 16) + le.BaseLow,
		     ((le.HighWord.Bits.LimitHi << 8) + le.LimitLow) <<
		     (le.HighWord.Bits.Granularity ? 12 : 0),
		     le.HighWord.Bits.Default_Big ? 32 : 16,
		     flags[0], flags[1], flags[2] );
    }
}

void DEBUG_InfoVirtual(void)
{
    MEMORY_BASIC_INFORMATION    mbi;
    char*                       addr = 0;
    char*                       state;
    char*                       type;
    char                        prot[3+1];

    if (DEBUG_CurrProcess == NULL)
        return;

    DEBUG_Printf(DBG_CHN_MESG, "Address  Size     State   Type    RWX\n");

    while (VirtualQueryEx(DEBUG_CurrProcess->handle, addr, &mbi, sizeof(mbi)) >= sizeof(mbi))
    {
        switch (mbi.State)
        {
        case MEM_COMMIT:        state = "commit "; break;
        case MEM_FREE:          state = "free   "; break;
        case MEM_RESERVE:       state = "reserve"; break;
        default:                state = "???    "; break;
        }
        if (mbi.State != MEM_FREE)
        {
            switch (mbi.Type)
            {
            case MEM_IMAGE:         type = "image  "; break;
            case MEM_MAPPED:        type = "mapped "; break;
            case MEM_PRIVATE:       type = "private"; break;
            case 0:                 type = "       "; break;
            default:                type = "???    "; break;
            }
            memset(prot, ' ' , sizeof(prot)-1);
            prot[sizeof(prot)-1] = '\0';
            if (mbi.AllocationProtect & (PAGE_READONLY|PAGE_READWRITE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))
                prot[0] = 'R';
            if (mbi.AllocationProtect & (PAGE_READWRITE|PAGE_EXECUTE_READWRITE))
                prot[1] = 'W';
            if (mbi.AllocationProtect & (PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY))
                prot[1] = 'C';
            if (mbi.AllocationProtect & (PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))
                prot[2] = 'X';
        }
        else
        {
            type = "";
            prot[0] = '\0';
        }
        DEBUG_Printf(DBG_CHN_MESG, "%08lx %08lx %s %s %s\n",
                     (DWORD)addr, mbi.RegionSize, state, type, prot);
        if (addr + mbi.RegionSize < addr) /* wrap around ? */
            break;
        addr += mbi.RegionSize;
    }
}

struct dll_option_layout
{
    void*               next;
    void*               prev;
    char* const*        channels;
    int                 nb_channels;
};

void DEBUG_DbgChannel(BOOL turn_on, const char* chnl, const char* name)
{
    DBG_VALUE                   val;
    struct dll_option_layout    dol;
    int                         i;
    char*                       str;
    unsigned char               buffer[32];
    unsigned char               mask;
    int                         done = 0;
    BOOL                        bAll;
    void*                       addr;

    if (DEBUG_GetSymbolValue("first_dll", -1, &val, FALSE) != gsv_found)
    {
        DEBUG_Printf(DBG_CHN_MESG, "Can't get first_option symbol");
        return;
    }
    addr = (void*)DEBUG_ToLinear(&val.addr);
    if (!chnl)                          mask = 15;
    else if (!strcmp(chnl, "fixme"))    mask = 1;
    else if (!strcmp(chnl, "err"))      mask = 2;
    else if (!strcmp(chnl, "warn"))     mask = 4;
    else if (!strcmp(chnl, "trace"))    mask = 8;
    else { DEBUG_Printf(DBG_CHN_MESG, "Unknown channel %s\n", chnl); return; }

    bAll = !strcmp("all", name);
    while (addr && DEBUG_READ_MEM(addr, &dol, sizeof(dol)))
    {
        for (i = 0; i < dol.nb_channels; i++)
        {
            if (DEBUG_READ_MEM((void*)(dol.channels + i), &str, sizeof(str)) &&
                DEBUG_READ_MEM(str, buffer, sizeof(buffer)) &&
                (!strcmp(buffer + 1, name) || bAll))
            {
                if (turn_on) buffer[0] |= mask; else buffer[0] &= ~mask;
                if (DEBUG_WRITE_MEM(str, buffer, 1)) done++;
            }
        }
        addr = dol.next;
    }
    if (!done) DEBUG_Printf(DBG_CHN_MESG, "Unable to find debug channel %s\n", name);
    else DEBUG_Printf(DBG_CHN_TRACE, "Changed %d channel instances\n", done);
}

