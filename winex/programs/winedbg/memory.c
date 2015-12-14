/*
 * Debugger memory handling
 *
 * Copyright 1993 Eric Youngdale
 * Copyright 1995 Alexandre Julliard
 * Copyright 2000 Eric Pouech
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>

#include "debugger.h"
#include "winbase.h"

#ifdef __i386__
#define IS_VM86_MODE() (DEBUG_context.EFlags & V86_FLAG)
#endif

static	void	DEBUG_Die(const char* msg)
{
   DEBUG_Printf(DBG_CHN_MESG, msg);
   exit(1);
}

void*	DEBUG_XMalloc(size_t size)
{
   void *res = malloc(size ? size : 1);
   if (res == NULL)
      DEBUG_Die("Memory exhausted.\n");
   memset(res, 0, size);
   return res;
}

void* DEBUG_XReAlloc(void *ptr, size_t size)
{
   void* res = realloc(ptr, size);
   if ((res == NULL) && size)
      DEBUG_Die("Memory exhausted.\n");
   return res;
}

char*	DEBUG_XStrDup(const char *str)
{
   char *res = strdup(str);
   if (!res)
      DEBUG_Die("Memory exhausted.\n");
   return res;
}

enum dbg_mode DEBUG_GetSelectorType( WORD sel )
{
#ifdef __i386__
    LDT_ENTRY	le;

    if (IS_VM86_MODE()) return MODE_VM86;
    if (sel == 0) return MODE_32;
    if (GetThreadSelectorEntry( DEBUG_CurrThread->handle, sel, &le))
        return le.HighWord.Bits.Default_Big ? MODE_32 : MODE_16;
    /* selector doesn't exist */
    return MODE_INVALID;
#else
    return MODE_32;
#endif
}
#ifdef __i386__
void DEBUG_FixAddress( DBG_ADDR *addr, DWORD def)
{
   if (addr->seg == 0xffffffff) addr->seg = def;
   if (DEBUG_IsSelectorSystem(addr->seg)) addr->seg = 0;
}

/* Determine if sel is a system selector (i.e. not managed by Wine) */
BOOL	DEBUG_IsSelectorSystem(WORD sel)
{
    if (IS_VM86_MODE()) return FALSE;  /* no system selectors in vm86 mode */
    return !(sel & 4) || ((sel >> 3) < 17);
}
#endif /* __i386__ */

DWORD DEBUG_ToLinear( const DBG_ADDR *addr )
{
#ifdef __i386__
   LDT_ENTRY	le;

   if (IS_VM86_MODE()) return (DWORD)(LOWORD(addr->seg) << 4) + addr->off;

   if (DEBUG_IsSelectorSystem(addr->seg))
      return addr->off;

   if (GetThreadSelectorEntry( DEBUG_CurrThread->handle, addr->seg, &le)) {
      return (le.HighWord.Bits.BaseHi << 24) + (le.HighWord.Bits.BaseMid << 16) + le.BaseLow + addr->off;
   }
   return 0;
#else
   return addr->off;
#endif
}

void DEBUG_GetCurrentAddress( DBG_ADDR *addr )
{
#ifdef __i386__
    addr->seg  = DEBUG_context.SegCs;

    if (DEBUG_IsSelectorSystem(addr->seg))
       addr->seg = 0;
    addr->off  = DEBUG_context.Eip;
#elif defined(__sparc__)
	 addr->seg = 0;
    addr->off = DEBUG_context.pc;
#else
#	error You must define GET_IP for this CPU
#endif
}

void	DEBUG_InvalAddr( const DBG_ADDR* addr )
{
   DEBUG_Printf(DBG_CHN_MESG,"*** Invalid address ");
   DEBUG_PrintAddress(addr, DEBUG_CurrThread->dbg_mode, FALSE);
   DEBUG_Printf(DBG_CHN_MESG,"\n");
   if (DBG_IVAR(ExtDbgOnInvalidAddress)) DEBUG_ExternalDebugger();
}

void	DEBUG_InvalLinAddr( void* addr )
{
   DBG_ADDR address;

   address.seg = 0;
   address.off = (unsigned long)addr;
   DEBUG_InvalAddr( &address );
}

/***********************************************************************
 *           DEBUG_ReadMemory
 *
 * Read a memory value.
 */
/* FIXME: this function is now getting closer and closer to
 * DEBUG_ExprGetValue. They should be merged...
 */
int DEBUG_ReadMemory( const DBG_VALUE* val )
{
    int		value = 0; /* to clear any unused byte */
    int		os = DEBUG_GetObjectSize(val->type);

    assert(sizeof(value) >= os);

    /* FIXME: only works on little endian systems */

    if (val->cookie == DV_TARGET) {
       DBG_ADDR	addr = val->addr;
       void*	lin;

#ifdef __i386__
       DEBUG_FixAddress( &addr, DEBUG_context.SegDs );
#endif
       lin = (void*)DEBUG_ToLinear( &addr );

       DEBUG_READ_MEM_VERBOSE(lin, &value, os);
    } else {
       if (val->addr.off)
	  memcpy(&value, (void*)val->addr.off, os);
    }
    return value;
}


/***********************************************************************
 *           DEBUG_WriteMemory
 *
 * Store a value in memory.
 */
void DEBUG_WriteMemory( const DBG_VALUE* val, int value )
{
    int		os = DEBUG_GetObjectSize(val->type);

    assert(sizeof(value) >= os);

    /* FIXME: only works on little endian systems */

    if (val->cookie == DV_TARGET) {
       DBG_ADDR addr = val->addr;
       void*	lin;

#ifdef __i386__
       DEBUG_FixAddress( &addr, DEBUG_context.SegDs );
#endif
       lin = (void*)DEBUG_ToLinear( &addr );
       DEBUG_WRITE_MEM_VERBOSE(lin, &value, os);
    } else {
       memcpy((void*)val->addr.off, &value, os);
    }
}

/***********************************************************************
 *           DEBUG_GrabAddress
 *
 * Get the address from a value
 */
BOOL DEBUG_GrabAddress( DBG_VALUE* value, BOOL fromCode )
{
    assert(value->cookie == DV_TARGET || value->cookie == DV_HOST);

#ifdef __i386__
    DEBUG_FixAddress( &value->addr,
		      (fromCode) ? DEBUG_context.SegCs : DEBUG_context.SegDs);
#endif

    /*
     * Dereference pointer to get actual memory address we need to be
     * reading.  We will use the same segment as what we have already,
     * and hope that this is a sensible thing to do.
     */
    if (value->type != NULL) {
        if (value->type == DEBUG_GetBasicType(DT_BASIC_CONST_INT)) {
	    /*
	     * We know that we have the actual offset stored somewhere
	     * else in 32-bit space.  Grab it, and we
	     * should be all set.
	     */
	    unsigned int  seg2 = value->addr.seg;
	    value->addr.seg = 0;
	    value->addr.off = DEBUG_GetExprValue(value, NULL);
	    value->addr.seg = seg2;
	} else {
	    struct datatype	* testtype;

	    if (DEBUG_TypeDerefPointer(value, &testtype) == 0)
	        return FALSE;
	    if (testtype != NULL || value->type == DEBUG_GetBasicType(DT_BASIC_CONST_INT))
	        value->addr.off = DEBUG_GetExprValue(value, NULL);
	}
    } else if (!value->addr.seg && !value->addr.off) {
        DEBUG_Printf(DBG_CHN_MESG,"Invalid expression\n");
	return FALSE;
    }
    return TRUE;
}

/***********************************************************************
 *           DEBUG_ExamineMemory
 *
 * Implementation of the 'x' command.
 */
void DEBUG_ExamineMemory( const DBG_VALUE *_value, int count, char format )
{
    DBG_VALUE		  value = *_value;
    int			  i;
    unsigned char	* pnt;

    if (!DEBUG_GrabAddress(&value, (format == 'i'))) return;

    if (format != 'i' && count > 1)
    {
        DEBUG_PrintAddress( &value.addr, DEBUG_CurrThread->dbg_mode, FALSE );
        DEBUG_Printf(DBG_CHN_MESG,": ");
    }

    pnt = (void*)DEBUG_ToLinear( &value.addr );

    switch(format)
    {
         case 'u':
                if (count == 1) count = 256;
                DEBUG_nchar += DEBUG_PrintStringW(DBG_CHN_MESG, &value.addr, count);
		DEBUG_Printf(DBG_CHN_MESG, "\n");
		return;
        case 's':
                DEBUG_nchar += DEBUG_PrintStringA(DBG_CHN_MESG, &value.addr, count);
		DEBUG_Printf(DBG_CHN_MESG, "\n");
		return;
	case 'i':
		while (count-- && DEBUG_DisassembleInstruction( &value.addr ));
		return;
#define DO_DUMP2(_t,_l,_f,_vv) { \
	        _t _v; \
		for(i=0; i<count; i++) { \
                    if (!DEBUG_READ_MEM_VERBOSE(pnt, &_v, sizeof(_t))) break; \
                    DEBUG_Printf(DBG_CHN_MESG,_f,(_vv)); \
                    pnt += sizeof(_t); value.addr.off += sizeof(_t); \
                    if ((i % (_l)) == (_l)-1) { \
                        DEBUG_Printf(DBG_CHN_MESG,"\n"); \
                        DEBUG_PrintAddress( &value.addr, DEBUG_CurrThread->dbg_mode, FALSE );\
                        DEBUG_Printf(DBG_CHN_MESG,": ");\
                    } \
		} \
		DEBUG_Printf(DBG_CHN_MESG,"\n"); \
        } \
	return
#define DO_DUMP(_t,_l,_f) DO_DUMP2(_t,_l,_f,_v)

        case 'x': DO_DUMP(int, 4, " %8.8x");
	case 'd': DO_DUMP(unsigned int, 4, " %10d");
	case 'w': DO_DUMP(unsigned short, 8, " %04x");
        case 'c': DO_DUMP2(char, 32, " %c", (_v < 0x20) ? ' ' : _v);
	case 'b': DO_DUMP2(char, 16, " %02x", (_v) & 0xff);
	}
}

/******************************************************************
 *		DEBUG_PrintStringA
 *
 * Prints on channel chnl, the string starting at address in target
 * address space. The string stops when either len chars (if <> -1)
 * have been printed, or the '\0' char is printed
 */
int  DEBUG_PrintStringA(int chnl, const DBG_ADDR* address, int len)
{
    char*       lin = (void*)DEBUG_ToLinear(address);
    char        ach[16+1];
    int         i, l;

    if (len == -1) len = 32767; /* should be big enough */

    /* so that the ach is always terminated */
    ach[sizeof(ach) - 1] = '\0';
    for (i = len; i >= 0; i -= sizeof(ach) - 1)
    {
        l = min(sizeof(ach) - 1, i);
        DEBUG_READ_MEM_VERBOSE(lin, ach, l);
        l = strlen(ach);
        DEBUG_OutputA(chnl, ach, l);
        if (l < sizeof(ach) - 1) break;
        lin += l;
        if (l < sizeof(ach) - 1) break;
    }
    return len - i; /* number of actually written chars */
}

int  DEBUG_PrintStringW(int chnl, const DBG_ADDR* address, int len)
{
    char*       lin = (void*)DEBUG_ToLinear(address);
    WCHAR       wch;
    int         ret = 0;

    if (len == -1) len = 32767; /* should be big enough */
    while (len--)
    {
        if (!DEBUG_READ_MEM_VERBOSE(lin, &wch, sizeof(wch)) || !wch)
            break;
        lin += sizeof(wch);
        DEBUG_OutputW(chnl, &wch, 1);
        ret++;
    }
    return ret;
}
