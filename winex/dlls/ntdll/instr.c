/*
 * Emulation of privileged instructions
 *
 * Copyright 1995 Alexandre Julliard
 */

#include "windef.h"
#include "wingdi.h"
#include "wine/winuser16.h"
#include "module.h"
#include "miscemu.h"
#include "global.h"
#include "selectors.h"
#ifdef HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif
#include "wine/port.h"
#include "wine/hardware.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);
WINE_DECLARE_DEBUG_CHANNEL(io);

#ifdef __i386__

inline static void add_stack( CONTEXT86 *context, int offset )
{
    if (ISV86(context) || !IS_SELECTOR_32BIT(context->SegSs))
        ADD_LOWORD( context->Esp, offset );
    else
        context->Esp += offset;
}

inline static void *make_ptr( CONTEXT86 *context, DWORD seg, DWORD off, int long_addr )
{
    if (ISV86(context)) return PTR_REAL_TO_LIN( seg, off );
    if (IS_SELECTOR_SYSTEM(seg)) return (void *)off;
    if (!long_addr) off = LOWORD(off);
    return ((LPBYTE)MapSL( MAKESEGPTR( seg, 0 ) )) + off;
}

inline static void *get_stack( CONTEXT86 *context )
{
    if (ISV86(context))
        return PTR_REAL_TO_LIN( context->SegSs, context->Esp );
    if (IS_SELECTOR_SYSTEM(context->SegSs))
        return (void *)context->Esp;
    if (IS_SELECTOR_32BIT(context->SegSs))
        return ((LPBYTE)MapSL( MAKESEGPTR( context->SegSs, 0 ) )) + context->Esp;
    return MapSL( MAKESEGPTR( context->SegSs, LOWORD(context->Esp) ) );
}

/***********************************************************************
 *           INSTR_ReplaceSelector
 *
 * Try to replace an invalid selector by a valid one.
 * The only selector where it is allowed to do "mov ax,40;mov es,ax"
 * is the so called 'bimodal' selector 0x40, which points to the BIOS
 * data segment. Used by (at least) Borland products (and programs compiled
 * using Borland products).
 *
 * See Undocumented Windows, Chapter 5, __0040.
 */
static BOOL INSTR_ReplaceSelector( CONTEXT86 *context, WORD *sel )
{
    extern char Call16_Start, Call16_End;

    if (IS_SELECTOR_SYSTEM(context->SegCs))
        if (    (char *)context->Eip >= &Call16_Start
             && (char *)context->Eip <  &Call16_End   )
        {
            /* Saved selector may have become invalid when the relay code */
            /* tries to restore it. We simply clear it. */
            *sel = 0;
            return TRUE;
        }

    if (*sel == 0x40)
    {
        static WORD sys_timer = 0;
        if (!sys_timer)
            sys_timer = CreateSystemTimer( 55, DOSMEM_Tick );
        *sel = DOSMEM_BiosDataSeg;
        return TRUE;
    }
    if (!IS_SELECTOR_SYSTEM(*sel) && !IS_SELECTOR_FREE(*sel))
        ERR("Got protection fault on valid selector, maybe your kernel is too old?\n" );
    return FALSE;  /* Can't replace selector, crashdump */
}


/***********************************************************************
 *           INSTR_GetOperandAddr
 *
 * Return the address of an instruction operand (from the mod/rm byte).
 */
static BYTE *INSTR_GetOperandAddr( CONTEXT86 *context, BYTE *instr,
                                   int long_addr, int segprefix, int *len )
{
    int mod, rm, base = 0, index = 0, ss = 0, seg = 0, off;

#define GET_VAL(val,type) \
    { *val = *(type *)instr; instr += sizeof(type); *len += sizeof(type); }

    *len = 0;
    GET_VAL( &mod, BYTE );
    rm = mod & 7;
    mod >>= 6;

    if (mod == 3)
    {
        switch(rm)
        {
        case 0: return (BYTE *)&context->Eax;
        case 1: return (BYTE *)&context->Ecx;
        case 2: return (BYTE *)&context->Edx;
        case 3: return (BYTE *)&context->Ebx;
        case 4: return (BYTE *)&context->Esp;
        case 5: return (BYTE *)&context->Ebp;
        case 6: return (BYTE *)&context->Esi;
        case 7: return (BYTE *)&context->Edi;
        }
    }

    if (long_addr)
    {
        if (rm == 4)
        {
            BYTE sib;
            GET_VAL( &sib, BYTE );
            rm = sib & 7;
            ss = sib >> 6;
            switch(sib >> 3)
            {
            case 0: index = context->Eax; break;
            case 1: index = context->Ecx; break;
            case 2: index = context->Edx; break;
            case 3: index = context->Ebx; break;
            case 4: index = 0; break;
            case 5: index = context->Ebp; break;
            case 6: index = context->Esi; break;
            case 7: index = context->Edi; break;
            }
        }

        switch(rm)
        {
        case 0: base = context->Eax; seg = context->SegDs; break;
        case 1: base = context->Ecx; seg = context->SegDs; break;
        case 2: base = context->Edx; seg = context->SegDs; break;
        case 3: base = context->Ebx; seg = context->SegDs; break;
        case 4: base = context->Esp; seg = context->SegSs; break;
        case 5: base = context->Ebp; seg = context->SegSs; break;
        case 6: base = context->Esi; seg = context->SegDs; break;
        case 7: base = context->Edi; seg = context->SegDs; break;
        }
        switch (mod)
        {
        case 0:
            if (rm == 5)  /* special case: ds:(disp32) */
            {
                GET_VAL( &base, DWORD );
                seg = context->SegDs;
            }
            break;

        case 1:  /* 8-bit disp */
            GET_VAL( &off, BYTE );
            base += (signed char)off;
            break;

        case 2:  /* 32-bit disp */
            GET_VAL( &off, DWORD );
            base += (signed long)off;
            break;
        }
    }
    else  /* short address */
    {
        switch(rm)
        {
        case 0:  /* ds:(bx,si) */
            base = LOWORD(context->Ebx) + LOWORD(context->Esi);
            seg  = context->SegDs;
            break;
        case 1:  /* ds:(bx,di) */
            base = LOWORD(context->Ebx) + LOWORD(context->Edi);
            seg  = context->SegDs;
            break;
        case 2:  /* ss:(bp,si) */
            base = LOWORD(context->Ebp) + LOWORD(context->Esi);
            seg  = context->SegSs;
            break;
        case 3:  /* ss:(bp,di) */
            base = LOWORD(context->Ebp) + LOWORD(context->Edi);
            seg  = context->SegSs;
            break;
        case 4:  /* ds:(si) */
            base = LOWORD(context->Esi);
            seg  = context->SegDs;
            break;
        case 5:  /* ds:(di) */
            base = LOWORD(context->Edi);
            seg  = context->SegDs;
            break;
        case 6:  /* ss:(bp) */
            base = LOWORD(context->Ebp);
            seg  = context->SegSs;
            break;
        case 7:  /* ds:(bx) */
            base = LOWORD(context->Ebx);
            seg  = context->SegDs;
            break;
        }

        switch(mod)
        {
        case 0:
            if (rm == 6)  /* special case: ds:(disp16) */
            {
                GET_VAL( &base, WORD );
                seg  = context->SegDs;
            }
            break;

        case 1:  /* 8-bit disp */
            GET_VAL( &off, BYTE );
            base += (signed char)off;
            break;

        case 2:  /* 16-bit disp */
            GET_VAL( &off, WORD );
            base += (signed short)off;
            break;
        }
        base &= 0xffff;
    }
    if (segprefix != -1) seg = segprefix;

    /* Make sure the segment and offset are valid */
    if (IS_SELECTOR_SYSTEM(seg)) return (BYTE *)(base + (index << ss));
    if (((seg & 7) != 7) || IS_SELECTOR_FREE(seg)) return NULL;
    if (wine_ldt_copy.limit[seg >> 3] < (base + (index << ss))) return NULL;
    return MapSL( MAKESEGPTR( seg, 0 ) ) + base + (index << ss);
#undef GET_VAL
}


/***********************************************************************
 *           INSTR_EmulateLDS
 *
 * Emulate the LDS (and LES,LFS,etc.) instruction.
 */
static BOOL INSTR_EmulateLDS( CONTEXT86 *context, BYTE *instr, int long_op,
                              int long_addr, int segprefix, int *len )
{
    WORD seg;
    BYTE *regmodrm = instr + 1 + (*instr == 0x0f);
    BYTE *addr = INSTR_GetOperandAddr( context, regmodrm,
                                       long_addr, segprefix, len );
    if (!addr)
        return FALSE;  /* Unable to emulate it */
    seg = *(WORD *)(addr + (long_op ? 4 : 2));

    if (!INSTR_ReplaceSelector( context, &seg ))
        return FALSE;  /* Unable to emulate it */

    /* Now store the offset in the correct register */

    switch((*regmodrm >> 3) & 7)
    {
    case 0:
        if (long_op) context->Eax = *(DWORD *)addr;
        else SET_LOWORD(context->Eax,*(WORD *)addr);
        break;
    case 1:
        if (long_op) context->Ecx = *(DWORD *)addr;
        else SET_LOWORD(context->Ecx,*(WORD *)addr);
        break;
    case 2:
        if (long_op) context->Edx = *(DWORD *)addr;
        else SET_LOWORD(context->Edx,*(WORD *)addr);
        break;
    case 3:
        if (long_op) context->Ebx = *(DWORD *)addr;
        else SET_LOWORD(context->Ebx,*(WORD *)addr);
        break;
    case 4:
        if (long_op) context->Esp = *(DWORD *)addr;
        else SET_LOWORD(context->Esp,*(WORD *)addr);
        break;
    case 5:
        if (long_op) context->Ebp = *(DWORD *)addr;
        else SET_LOWORD(context->Ebp,*(WORD *)addr);
        break;
    case 6:
        if (long_op) context->Esi = *(DWORD *)addr;
        else SET_LOWORD(context->Esi,*(WORD *)addr);
        break;
    case 7:
        if (long_op) context->Edi = *(DWORD *)addr;
        else SET_LOWORD(context->Edi,*(WORD *)addr);
        break;
    }

    /* Store the correct segment in the segment register */

    switch(*instr)
    {
    case 0xc4: context->SegEs = seg; break;  /* les */
    case 0xc5: context->SegDs = seg; break;  /* lds */
    case 0x0f: switch(instr[1])
               {
               case 0xb2: context->SegSs = seg; break;  /* lss */
               case 0xb4: context->SegFs = seg; break;  /* lfs */
               case 0xb5: context->SegGs = seg; break;  /* lgs */
               }
               break;
    }

    /* Add the opcode size to the total length */

    *len += 1 + (*instr == 0x0f);
    return TRUE;
}

/***********************************************************************
 *           INSTR_inport
 *
 * input on a I/O port
 */
static DWORD INSTR_inport( WORD port, int size, CONTEXT86 *context )
{
    DWORD res = IO_inport( port, size );
    if (TRACE_ON(io))
    {
        switch(size)
        {
        case 1:
            DPRINTF( "0x%x < %02x @ %04x:%04x\n", port, LOBYTE(res),
                     (WORD)context->SegCs, LOWORD(context->Eip));
            break;
        case 2:
            DPRINTF( "0x%x < %04x @ %04x:%04x\n", port, LOWORD(res),
                     (WORD)context->SegCs, LOWORD(context->Eip));
            break;
        case 4:
            DPRINTF( "0x%x < %08lx @ %04x:%04x\n", port, res,
                     (WORD)context->SegCs, LOWORD(context->Eip));
            break;
        }
    }
    return res;
}


/***********************************************************************
 *           INSTR_outport
 *
 * output on a I/O port
 */
static void INSTR_outport( WORD port, int size, DWORD val, CONTEXT86 *context )
{
    IO_outport( port, size, val );
    if (TRACE_ON(io))
    {
        switch(size)
        {
        case 1:
            DPRINTF("0x%x > %02x @ %04x:%04x\n", port, LOBYTE(val),
                    (WORD)context->SegCs, LOWORD(context->Eip));
            break;
        case 2:
            DPRINTF("0x%x > %04x @ %04x:%04x\n", port, LOWORD(val),
                    (WORD)context->SegCs, LOWORD(context->Eip));
            break;
        case 4:
            DPRINTF("0x%x > %08lx @ %04x:%04x\n", port, val,
                    (WORD)context->SegCs, LOWORD(context->Eip));
            break;
        }
    }
}


/***********************************************************************
 *           INSTR_EmulateInstruction
 *
 * Emulate a privileged instruction. Returns TRUE if emulation successful.
 */
BOOL INSTR_EmulateInstruction( CONTEXT86 *context )
{
    int prefix, segprefix, prefixlen, len, repX, long_op, long_addr;
    SEGPTR gpHandler;
    BYTE *instr;

    long_op = long_addr = (!ISV86(context) && IS_SELECTOR_32BIT(context->SegCs));
    instr = make_ptr( context, context->SegCs, context->Eip, TRUE );
    if (!instr) return FALSE;

    /* First handle any possible prefix */

    segprefix = -1;  /* no prefix */
    prefix = 1;
    repX = 0;
    prefixlen = 0;
    while(prefix)
    {
        switch(*instr)
        {
        case 0x2e:
            segprefix = context->SegCs;
            break;
        case 0x36:
            segprefix = context->SegSs;
            break;
        case 0x3e:
            segprefix = context->SegDs;
            break;
        case 0x26:
            segprefix = context->SegEs;
            break;
        case 0x64:
            segprefix = context->SegFs;
            break;
        case 0x65:
            segprefix = context->SegGs;
            break;
        case 0x66:
            long_op = !long_op;  /* opcode size prefix */
            break;
        case 0x67:
            long_addr = !long_addr;  /* addr size prefix */
            break;
        case 0xf0:  /* lock */
	    break;
        case 0xf2:  /* repne */
	    repX = 1;
	    break;
        case 0xf3:  /* repe */
	    repX = 2;
            break;
        default:
            prefix = 0;  /* no more prefixes */
            break;
        }
        if (prefix)
        {
            instr++;
            prefixlen++;
        }
    }

    /* Now look at the actual instruction */

    TRACE("addr=%p, instr=%02x\n", instr, *instr);
    switch(*instr)
    {
        case 0x07: /* pop es */
        case 0x17: /* pop ss */
        case 0x1f: /* pop ds */
            {
                WORD seg = *(WORD *)get_stack( context );
                if (INSTR_ReplaceSelector( context, &seg ))
                {
                    switch(*instr)
                    {
                    case 0x07: context->SegEs = seg; break;
                    case 0x17: context->SegSs = seg; break;
                    case 0x1f: context->SegDs = seg; break;
                    }
                    add_stack(context, long_op ? 4 : 2);
                    context->Eip += prefixlen + 1;
                    return TRUE;
                }
            }
            break;  /* Unable to emulate it */

        case 0x0f: /* extended instruction */
            switch(instr[1])
            {
	    case 0x22: /* mov eax, crX */
	    	switch (instr[2]) {
		case 0xc0:
			ERR("mov eax,cr0 at 0x%08lx, EAX=0x%08lx\n",
                            context->Eip,context->Eax );
                        context->Eip += prefixlen+3;
			return TRUE;
		default:
			break; /*fallthrough to bad instruction handling */
		}
		break; /*fallthrough to bad instruction handling */
	    case 0x20: /* mov crX, eax */
	        switch (instr[2]) {
		case 0xe0: /* mov cr4, eax */
		    /* CR4 register . See linux/arch/i386/mm/init.c, X86_CR4_ defs
		     * bit 0: VME	Virtual Mode Exception ?
		     * bit 1: PVI	Protected mode Virtual Interrupt
		     * bit 2: TSD	Timestamp disable
		     * bit 3: DE	Debugging extensions
		     * bit 4: PSE	Page size extensions
		     * bit 5: PAE   Physical address extension
		     * bit 6: MCE	Machine check enable
		     * bit 7: PGE   Enable global pages
		     * bit 8: PCE	Enable performance counters at IPL3
		     */
                    ERR("mov cr4,eax at 0x%08lx\n",context->Eip);
                    context->Eax = 0;
                    context->Eip += prefixlen+3;
		    return TRUE;
		case 0xc0: /* mov cr0, eax */
                    ERR("mov cr0,eax at 0x%08lx\n",context->Eip);
                    context->Eax = 0x10; /* FIXME: set more bits ? */
                    context->Eip += prefixlen+3;
		    return TRUE;
		default: /* fallthrough to illegal instruction */
		    break;
		}
		/* fallthrough to illegal instruction */
		break;
            case 0xa1: /* pop fs */
                {
                    WORD seg = *(WORD *)get_stack( context );
                    if (INSTR_ReplaceSelector( context, &seg ))
                    {
                        context->SegFs = seg;
                        add_stack(context, long_op ? 4 : 2);
                        context->Eip += prefixlen + 2;
                        return TRUE;
                    }
                }
                break;
            case 0xa9: /* pop gs */
                {
                    WORD seg = *(WORD *)get_stack( context );
                    if (INSTR_ReplaceSelector( context, &seg ))
                    {
                        context->SegGs = seg;
                        add_stack(context, long_op ? 4 : 2);
                        context->Eip += prefixlen + 2;
                        return TRUE;
                    }
                }
                break;
            case 0xb2: /* lss addr,reg */
            case 0xb4: /* lfs addr,reg */
            case 0xb5: /* lgs addr,reg */
                if (INSTR_EmulateLDS( context, instr, long_op,
                                      long_addr, segprefix, &len ))
                {
                    context->Eip += prefixlen + len;
                    return TRUE;
                }
                break;
            }
            break;  /* Unable to emulate it */

        case 0x6c: /* insb     */
        case 0x6d: /* insw/d   */
        case 0x6e: /* outsb    */
        case 0x6f: /* outsw/d  */
	    {
	      int typ = *instr;  /* Just in case it's overwritten.  */
	      int outp = (typ >= 0x6e);
	      unsigned long count = repX ?
                          (long_addr ? context->Ecx : LOWORD(context->Ecx)) : 1;
	      int opsize = (typ & 1) ? (long_op ? 4 : 2) : 1;
	      int step = (context->EFlags & 0x400) ? -opsize : +opsize;
	      int seg = outp ? context->SegDs : context->SegEs;  /* FIXME: is this right? */

	      if (outp)
		/* FIXME: Check segment readable.  */
		(void)0;
	      else
		/* FIXME: Check segment writeable.  */
		(void)0;

	      if (repX)
              {
		if (long_addr) context->Ecx = 0;
		else SET_LOWORD(context->Ecx,0);
              }

	      while (count-- > 0)
		{
		  void *data;
                  WORD dx = LOWORD(context->Edx);
		  if (outp)
                  {
                      data = make_ptr( context, seg, context->Esi, long_addr );
                      if (long_addr) context->Esi += step;
                      else ADD_LOWORD(context->Esi,step);
                  }
		  else
                  {
                      data = make_ptr( context, seg, context->Edi, long_addr );
                      if (long_addr) context->Edi += step;
                      else ADD_LOWORD(context->Edi,step);
                  }

		  switch (typ)
                  {
		    case 0x6c:
		      *(BYTE *)data = INSTR_inport( dx, 1, context );
		      break;
		    case 0x6d:
		      if (long_op)
                          *(DWORD *)data = INSTR_inport( dx, 4, context );
		      else
                          *(WORD *)data = INSTR_inport( dx, 2, context );
		      break;
		    case 0x6e:
                        INSTR_outport( dx, 1, *(BYTE *)data, context );
                        break;
		    case 0x6f:
                        if (long_op)
                            INSTR_outport( dx, 4, *(DWORD *)data, context );
                        else
                            INSTR_outport( dx, 2, *(WORD *)data, context );
                        break;
		    }
		}
              context->Eip += prefixlen + 1;
	    }
            return TRUE;

        case 0x8e: /* mov XX,segment_reg */
            {
                WORD seg;
                BYTE *addr = INSTR_GetOperandAddr(context, instr + 1,
                                                  long_addr, segprefix, &len );
                if (!addr)
                    break;  /* Unable to emulate it */
                seg = *(WORD *)addr;
                if (!INSTR_ReplaceSelector( context, &seg ))
                    break;  /* Unable to emulate it */

                switch((instr[1] >> 3) & 7)
                {
                case 0:
                    context->SegEs = seg;
                    context->Eip += prefixlen + len + 1;
                    return TRUE;
                case 1:  /* cs */
                    break;
                case 2:
                    context->SegSs = seg;
                    context->Eip += prefixlen + len + 1;
                    return TRUE;
                case 3:
                    context->SegDs = seg;
                    context->Eip += prefixlen + len + 1;
                    return TRUE;
                case 4:
                    context->SegFs = seg;
                    context->Eip += prefixlen + len + 1;
                    return TRUE;
                case 5:
                    context->SegGs = seg;
                    context->Eip += prefixlen + len + 1;
                    return TRUE;
                case 6:  /* unused */
                case 7:  /* unused */
                    break;
                }
            }
            break;  /* Unable to emulate it */

        case 0xc4: /* les addr,reg */
        case 0xc5: /* lds addr,reg */
            if (INSTR_EmulateLDS( context, instr, long_op,
                                  long_addr, segprefix, &len ))
            {
                context->Eip += prefixlen + len;
                return TRUE;
            }
            break;  /* Unable to emulate it */

        case 0xcd: /* int <XX> */
            if (long_op)
            {
                ERR("int xx from 32-bit code is not supported.\n");
                break;  /* Unable to emulate it */
            }
            else
            {
                FARPROC16 addr = INT_GetPMHandler( instr[1] );
                WORD *stack = get_stack( context );
                if (!addr)
                {
                    FIXME("no handler for interrupt %02x, ignoring it\n", instr[1]);
                    context->Eip += prefixlen + 2;
                    return TRUE;
                }
                /* Push the flags and return address on the stack */
                *(--stack) = LOWORD(context->EFlags);
                *(--stack) = context->SegCs;
                *(--stack) = LOWORD(context->Eip) + prefixlen + 2;
                add_stack(context, -3 * sizeof(WORD));
                /* Jump to the interrupt handler */
                context->SegCs  = HIWORD(addr);
                context->Eip = LOWORD(addr);
            }
            return TRUE;

        case 0xcf: /* iret */
            if (long_op)
            {
                DWORD *stack = get_stack( context );
                context->Eip = *stack++;
                context->SegCs  = *stack++;
                context->EFlags = *stack;
                add_stack(context, 3*sizeof(DWORD));  /* Pop the return address and flags */
            }
            else
            {
                WORD *stack = get_stack( context );
                context->Eip = *stack++;
                context->SegCs  = *stack++;
                SET_LOWORD(context->EFlags,*stack);
                add_stack(context, 3*sizeof(WORD));  /* Pop the return address and flags */
            }
            return TRUE;

        case 0xe4: /* inb al,XX */
            SET_LOBYTE(context->Eax,INSTR_inport( instr[1], 1, context ));
            context->Eip += prefixlen + 2;
            return TRUE;

        case 0xe5: /* in (e)ax,XX */
            if (long_op)
                context->Eax = INSTR_inport( instr[1], 4, context );
            else
                SET_LOWORD(context->Eax, INSTR_inport( instr[1], 2, context ));
            context->Eip += prefixlen + 2;
            return TRUE;

        case 0xe6: /* outb XX,al */
            INSTR_outport( instr[1], 1, LOBYTE(context->Eax), context );
            context->Eip += prefixlen + 2;
            return TRUE;

        case 0xe7: /* out XX,(e)ax */
            if (long_op)
                INSTR_outport( instr[1], 4, context->Eax, context );
            else
                INSTR_outport( instr[1], 2, LOWORD(context->Eax), context );
            context->Eip += prefixlen + 2;
            return TRUE;

        case 0xec: /* inb al,dx */
            SET_LOBYTE(context->Eax, INSTR_inport( LOWORD(context->Edx), 1, context ) );
            context->Eip += prefixlen + 1;
            return TRUE;

        case 0xed: /* in (e)ax,dx */
            if (long_op)
                context->Eax = INSTR_inport( LOWORD(context->Edx), 4, context );
            else
                SET_LOWORD(context->Eax, INSTR_inport( LOWORD(context->Edx), 2, context ));
            context->Eip += prefixlen + 1;
            return TRUE;

        case 0xee: /* outb dx,al */
            INSTR_outport( LOWORD(context->Edx), 1, LOBYTE(context->Eax), context );
            context->Eip += prefixlen + 1;
            return TRUE;

        case 0xef: /* out dx,(e)ax */
            if (long_op)
                INSTR_outport( LOWORD(context->Edx), 4, context->Eax, context );
            else
                INSTR_outport( LOWORD(context->Edx), 2, LOWORD(context->Eax), context );
            context->Eip += prefixlen + 1;
            return TRUE;

        case 0xfa: /* cli, ignored */
            context->Eip += prefixlen + 1;
            return TRUE;

        case 0xfb: /* sti, ignored */
            context->Eip += prefixlen + 1;
            return TRUE;
    }


    /* Check for Win16 __GP handler */
    gpHandler = HasGPHandler16( MAKESEGPTR( context->SegCs, context->Eip ) );
    if (gpHandler)
    {
        WORD *stack = get_stack( context );
        *--stack = context->SegCs;
        *--stack = context->Eip;
        add_stack(context, -2*sizeof(WORD));

        context->SegCs = SELECTOROF( gpHandler );
        context->Eip   = OFFSETOF( gpHandler );
        return TRUE;
    }
    return FALSE;  /* Unable to emulate it */
}

__ASM_GLOBAL_FUNC( __get_idt, "subl $8,%esp\n\tsidt (%esp)\n\t"
			      "movl 2(%esp),%eax\n\taddl $8,%esp\n\tret" )
LPVOID __get_idt(void);

static LDT_ENTRY* idt;
static LDT_ENTRY fake_idt[256];

#define ALIGN_IDT(x) \
    if ((x >= (LPBYTE)idt) && ((x - (LPBYTE)idt) < sizeof(fake_idt))) \
        x = ((LPBYTE)&fake_idt) + (x - (LPBYTE)idt)

static BOOL INSTR_movs(CONTEXT86 *context, int len)
{
    LPBYTE src = (LPBYTE)context->Esi;
    LPBYTE dst = (LPBYTE)context->Edi;
    TRACE("src=%p, dst=%p\n", src, dst);
    ALIGN_IDT(src);
    ALIGN_IDT(dst);
    /* FIXME: should check eflags for direction flag */
    TRACE("copying %d bytes from %p to %p\n", len, src, dst);
    memcpy(dst, src, len);
    context->Esi += len;
    context->Edi += len;
    context->Eip += 1;
    return TRUE;
}

/***********************************************************************
 *           INSTR_EmulateIDT
 *
 * Emulate IDT access for programs trying to exploit Win95's weak security
 * (typical for certain copy protection software)
 */
static BOOL INSTR_IDT_Emulate(LPVOID arg, LPCVOID addr)
{
    static BOOL attempted_already = FALSE;
    unsigned long ofs, idx;
    CONTEXT86 *context = arg;
    int long_op, long_addr;
    BYTE *instr;

    if (!attempted_already) {
        ERR("Evil attempt to exploit win9x system security flaws detected\n");
        ERR("UNIX system security is too strong, can't emulate properly\n");
        attempted_already = TRUE;
    }
    ofs = (LPBYTE)addr - (LPBYTE)idt;
    idx = ofs / sizeof(LDT_ENTRY);
    TRACE("fault: ofs %08lx, descriptor %02lx, eip %08lx\n", ofs, idx, context->Eip);

    long_op = long_addr = (!ISV86(context) && IS_SELECTOR_32BIT(context->SegCs));
    instr = make_ptr( context, context->SegCs, context->Eip, TRUE );
    if (!instr) return FALSE;

    switch(*instr)
    {
    case 0xa4: /* movsb */
        TRACE("movsb\n");
        return INSTR_movs(context, 1);
    case 0xa5: /* movsw/l */
        TRACE("movs%c\n", long_op ? 'l' : 'w');
        return INSTR_movs(context, long_op ? 4 : 2);
    default:
        TRACE("unhandled op: %02x\n", *instr);
        break;
    }
    return FALSE;
}

/***********************************************************************
 *           INSTR_Init
 *
 * Get IDT address for emulation.
 */
void INSTR_Init(void)
{
    LDT_ENTRY* mem;
    BOOL ok;

#ifdef HAVE_VALGRIND_VALGRIND_H
    if (RUNNING_ON_VALGRIND > 0)
    {
        ERR ("Disabling SIDT call due to running under Valgrind!\n");
        return;
    }
#endif

    idt = __get_idt();
/* On the Mac Pro, the sidt instruction returns bad values (ones that intersect with
   the required Windows PE exe address ranges) in some cases.  Bug filed with Apple */
#if !defined(__APPLE__)
    mem = VirtualAlloc(idt, sizeof(fake_idt), MEM_RESERVE|MEM_SYSTEM, PAGE_NOACCESS);
    ok = VIRTUAL_SetFaultHandler(mem, INSTR_IDT_Emulate, NULL);
#endif
}


#else /* __i386__ */

void INSTR_Init(void)
{
  /* Do nothing as we don't do instruction emulation */
}
#endif  /* __i386__ */
