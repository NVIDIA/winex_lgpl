/*
 * i386 signal handling routines
 *
 * Copyright 1999 Alexandre Julliard
 * Copyright (c) 2001-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#ifdef __i386__

#include "config.h"
#include "wine/port.h"
#include "wine/hardware.h"

#include "wine/server.h"
#include "wine/file.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else
# ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif

#ifdef HAVE_SYS_VM86_H
# include <sys/vm86.h>
#endif

#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#include "winternl.h"
#include "winnt.h"

#include "selectors.h"

/***********************************************************************
 * signal context platform-specific definitions
 */

extern void __wine_ret_to_32_regs( CONTEXT *context ) WINE_NORETURN;

#ifdef HAVE_SIGALTSTACK
#ifdef SIGALTSTACK_TAKES_STACK_T
static int wine_sigaltstack( const stack_t *new,
                             stack_t *old )
#else
static int wine_sigaltstack( const struct sigaltstack *new,
                             struct sigaltstack *old )
#endif
{
    int ret;

    ret = sigaltstack(new, old);
    if (ret >= 0) return 0;

#ifdef linux
/* direct syscall for sigaltstack: glibc 2.0 just has an ENOSYS stub. */
    __asm__ __volatile__( "pushl %%ebx\n\t"
                          "movl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "popl %%ebx"
                          : "=a" (ret)
                          : "0" (SYS_sigaltstack), "r" (new), "c" (old) );
    if (ret >= 0) return 0;
    errno = -ret;
#endif // linux
    return -1;
}
#endif // linux

#ifdef linux
typedef struct
{
    unsigned short sc_gs, __gsh;
    unsigned short sc_fs, __fsh;
    unsigned short sc_es, __esh;
    unsigned short sc_ds, __dsh;
    unsigned long sc_edi;
    unsigned long sc_esi;
    unsigned long sc_ebp;
    unsigned long sc_esp;
    unsigned long sc_ebx;
    unsigned long sc_edx;
    unsigned long sc_ecx;
    unsigned long sc_eax;
    unsigned long sc_trapno;
    unsigned long sc_err;
    unsigned long sc_eip;
    unsigned short sc_cs, __csh;
    unsigned long sc_eflags;
    unsigned long esp_at_signal;
    unsigned short sc_ss, __ssh;
    unsigned long i387;
    unsigned long oldmask;
    unsigned long cr2;
} SIGCONTEXT;

#define HANDLER_DEF(name) void name( int __signal, SIGCONTEXT __context )
#define HANDLER_CONTEXT (&__context)

#if !defined( USE_PTHREADS )
/* this is the sigaction structure from the Linux 2.1.20 kernel.  */
struct kernel_sigaction
{
    void (*ksa_handler)();
    unsigned long ksa_mask;
    unsigned long ksa_flags;
    void *ksa_restorer;
};

/* Similar to the sigaction function in libc, except it leaves alone the
   restorer field, which is used to specify the signal stack address */
static inline int wine_sigaction( int sig, struct kernel_sigaction *new,
                                  struct kernel_sigaction *old )
{
    __asm__ __volatile__( "pushl %%ebx\n\t"
                          "movl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "popl %%ebx"
                          : "=a" (sig)
                          : "0" (SYS_sigaction), "r" (sig), "c" (new), "d" (old) );
    if (sig>=0) return 0;
    errno = -sig;
    return -1;
}
#endif // !USE_PTHREADS


#define VM86_EAX 0 /* the %eax value while vm86_enter is executing */

int vm86_enter( void **vm86_ptr );
void vm86_return(void);
void vm86_return_end(void);
__ASM_GLOBAL_FUNC(vm86_enter,
                  "pushl %ebp\n\t"
                  "movl %esp, %ebp\n\t"
                  "movl $166,%eax\n\t"  /*SYS_vm86*/
                  "movl 8(%ebp),%ecx\n\t" /* vm86_ptr */
                  "movl (%ecx),%ecx\n\t"
                  "pushl %ebx\n\t"
                  "movl $1,%ebx\n\t"    /*VM86_ENTER*/
                  "pushl %ecx\n\t"      /* put vm86plus_struct ptr somewhere we can find it */
                  "pushl %fs\n\t"
                  "int $0x80\n"
                  ".globl " __ASM_NAME("vm86_return") "\n\t"
                  ".type " __ASM_NAME("vm86_return") ",@function\n"
                  __ASM_NAME("vm86_return") ":\n\t"
                  "popl %fs\n\t"
                  "popl %ecx\n\t"
                  "popl %ebx\n\t"
                  "popl %ebp\n\t"
                  "testl %eax,%eax\n\t"
                  "jl 0f\n\t"
                  "cmpb $0,%al\n\t" /* VM86_SIGNAL */
                  "je " __ASM_NAME("vm86_enter") "\n\t"
                  "0:\n\t"
                  "movl 4(%esp),%ecx\n\t"  /* vm86_ptr */
                  "movl $0,(%ecx)\n\t"
                  ".globl " __ASM_NAME("vm86_return_end") "\n\t"
                  ".type " __ASM_NAME("vm86_return_end") ",@function\n"
                  __ASM_NAME("vm86_return_end") ":\n\t"
                  "ret" );

#define __HAVE_VM86

#endif  /* linux */

#ifdef BSDI

#define EAX_sig(context)     ((context)->tf_eax)
#define EBX_sig(context)     ((context)->tf_ebx)
#define ECX_sig(context)     ((context)->tf_ecx)
#define EDX_sig(context)     ((context)->tf_edx)
#define ESI_sig(context)     ((context)->tf_esi)
#define EDI_sig(context)     ((context)->tf_edi)
#define EBP_sig(context)     ((context)->tf_ebp)

#define CS_sig(context)      ((context)->tf_cs)
#define DS_sig(context)      ((context)->tf_ds)
#define ES_sig(context)      ((context)->tf_es)
#define SS_sig(context)      ((context)->tf_ss)

#include <machine/frame.h>
typedef struct trapframe SIGCONTEXT;

#define HANDLER_DEF(name) void name( int __signal, int code, SIGCONTEXT *__context )
#define HANDLER_CONTEXT __context

#define EFL_sig(context)     ((context)->tf_eflags)

#define EIP_sig(context)     (*((unsigned long*)&(context)->tf_eip))
#define ESP_sig(context)     (*((unsigned long*)&(context)->tf_esp))

#endif /* bsdi */

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)

typedef struct sigcontext SIGCONTEXT;

#define HANDLER_DEF(name) void name( int __signal, int code, SIGCONTEXT *__context )
#define HANDLER_CONTEXT __context

#endif  /* FreeBSD */

#if defined(__svr4__) || defined(_SCO_DS) || defined(__sun)

#ifdef _SCO_DS
#include <sys/regset.h>
#endif
/* Solaris kludge */
#undef ERR
#include <sys/ucontext.h>
#undef ERR
typedef struct ucontext SIGCONTEXT;

#define HANDLER_DEF(name) void name( int __signal, void *__siginfo, SIGCONTEXT *__context )
#define HANDLER_CONTEXT __context

#endif  /* svr4 || SCO_DS */

#ifdef __EMX__

typedef struct
{
    unsigned long ContextFlags;
    FLOATING_SAVE_AREA sc_float;
    unsigned long sc_gs;
    unsigned long sc_fs;
    unsigned long sc_es;
    unsigned long sc_ds;
    unsigned long sc_edi;
    unsigned long sc_esi;
    unsigned long sc_eax;
    unsigned long sc_ebx;
    unsigned long sc_ecx;
    unsigned long sc_edx;
    unsigned long sc_ebp;
    unsigned long sc_eip;
    unsigned long sc_cs;
    unsigned long sc_eflags;
    unsigned long sc_esp;
    unsigned long sc_ss;
} SIGCONTEXT;

#endif  /* __EMX__ */

#ifdef __APPLE__
# include <sys/ucontext.h>
# include <signal.h>
# include <mach/mach.h>
# include <mach/mach_init.h>
#if TG_MAC_OS_X_SDK_VERSION >= TG_MAC_OS_X_VERSION_MAVERICKS
#include <mach/thread_act.h>
#include <mach/thread_status.h>
#else
#include <mach/i386/thread_act.h>
#include <mach/i386/thread_status.h>
#endif
# include "exc_server.defs.h"

typedef ucontext_t SIGCONTEXT;

/* if enabled, allows gdb to get exceptions & breakpoints; however, it
   breaks copy protection */
/* #define ENABLE_MAC_DEBUGGING */

/* NOTE - if you add or change any of these, check build_fake_signal_context()
   and friend to make sure it's implemented in our mach exceptions */

#ifdef MACH_X86_STATE_NEEDS_PREFIX
#define DREG(a) __##a
#else
#define DREG(a) a
#endif

#define EAX_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(eax))
#define EBX_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(ebx))
#define ECX_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(ecx))
#define EDX_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(edx))
#define ESI_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(esi))
#define EDI_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(edi))
#define EBP_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(ebp))

#define CS_sig(context)      ((context)->uc_mcontext->DREG(ss).DREG(cs))
#define DS_sig(context)      ((context)->uc_mcontext->DREG(ss).DREG(ds))
#define ES_sig(context)      ((context)->uc_mcontext->DREG(ss).DREG(es))
#define SS_sig(context)      ((context)->uc_mcontext->DREG(ss).DREG(ss))

#define FS_sig(context)      ((context)->uc_mcontext->DREG(ss).DREG(fs))
#define GS_sig(context)      ((context)->uc_mcontext->DREG(ss).DREG(gs))

#define EFL_sig(context)     ((context)->uc_mcontext->DREG(ss).DREG(eflags))

#define EIP_sig(context)     (*((unsigned long*)&(context)->uc_mcontext->DREG(ss).DREG(eip)))
#define ESP_sig(context)     (*((unsigned long*)&(context)->uc_mcontext->DREG(ss).DREG(esp)))
#define TRAP_sig(context)    ((context)->uc_mcontext->DREG(es).DREG(trapno))
#define ERROR_sig(context)   ((context)->uc_mcontext->DREG(es).DREG(err))
#define CR2_sig(context)    ((context)->uc_mcontext->DREG(es).DREG(faultvaddr))

#define HANDLER_DEF(name) void __dynamicstackalign name( int __signal, siginfo_t *__siginfo, SIGCONTEXT *__context )
#define HANDLER_CONTEXT (__context)

static mach_port_t gExceptionPort;

#endif /* __APPLE__ */


#if defined(linux) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMX__)

#define EAX_sig(context)     ((context)->sc_eax)
#define EBX_sig(context)     ((context)->sc_ebx)
#define ECX_sig(context)     ((context)->sc_ecx)
#define EDX_sig(context)     ((context)->sc_edx)
#define ESI_sig(context)     ((context)->sc_esi)
#define EDI_sig(context)     ((context)->sc_edi)
#define EBP_sig(context)     ((context)->sc_ebp)

#define CS_sig(context)      ((context)->sc_cs)
#define DS_sig(context)      ((context)->sc_ds)
#define ES_sig(context)      ((context)->sc_es)
#define SS_sig(context)      ((context)->sc_ss)

/* FS and GS are now in the sigcontext struct of FreeBSD, but not
 * saved by the exception handling. duh.
 * Actually they are in -current (have been for a while), and that
 * patch now finally has been MFC'd to -stable too (Nov 15 1999).
 * If you're running a system from the -stable branch older than that,
 * like a 3.3-RELEASE, grab the patch from the ports tree:
 * ftp://ftp.freebsd.org/pub/FreeBSD/FreeBSD-current/ports/emulators/wine/files/patch-3.3-sys-fsgs
 * (If its not yet there when you look, go here:
 * http://www.jelal.kn-bremen.de/freebsd/ports/emulators/wine/files/ )
 */
#ifdef __FreeBSD__
#define FS_sig(context)      ((context)->sc_fs)
#define GS_sig(context)      ((context)->sc_gs)
#endif

#ifdef linux
#define FS_sig(context)      ((context)->sc_fs)
#define GS_sig(context)      ((context)->sc_gs)
#define CR2_sig(context)     ((context)->cr2)
#define TRAP_sig(context)    ((context)->sc_trapno)
#define ERROR_sig(context)   ((context)->sc_err)
#define FPU_sig(context)     ((FLOATING_SAVE_AREA*)((context)->i387))
#endif

#ifndef __FreeBSD__
#define EFL_sig(context)     ((context)->sc_eflags)
#else
#define EFL_sig(context)     ((context)->sc_efl)
/* FreeBSD, see i386/i386/traps.c::trap_pfault va->err kludge  */
#define CR2_sig(context)     ((context)->sc_err)
#define TRAP_sig(context)    ((context)->sc_trapno)
#endif

#define EIP_sig(context)     (*((unsigned long*)&(context)->sc_eip))
#define ESP_sig(context)     (*((unsigned long*)&(context)->sc_esp))

#endif  /* linux || __NetBSD__ || __FreeBSD__ || __OpenBSD__ */

#if defined(__svr4__) || defined(_SCO_DS) || defined(__sun)

#ifdef _SCO_DS
#define gregs regs
#endif

#define EAX_sig(context)     ((context)->uc_mcontext.gregs[EAX])
#define EBX_sig(context)     ((context)->uc_mcontext.gregs[EBX])
#define ECX_sig(context)     ((context)->uc_mcontext.gregs[ECX])
#define EDX_sig(context)     ((context)->uc_mcontext.gregs[EDX])
#define ESI_sig(context)     ((context)->uc_mcontext.gregs[ESI])
#define EDI_sig(context)     ((context)->uc_mcontext.gregs[EDI])
#define EBP_sig(context)     ((context)->uc_mcontext.gregs[EBP])

#define CS_sig(context)      ((context)->uc_mcontext.gregs[CS])
#define DS_sig(context)      ((context)->uc_mcontext.gregs[DS])
#define ES_sig(context)      ((context)->uc_mcontext.gregs[ES])
#define SS_sig(context)      ((context)->uc_mcontext.gregs[SS])

#define FS_sig(context)      ((context)->uc_mcontext.gregs[FS])
#define GS_sig(context)      ((context)->uc_mcontext.gregs[GS])

#define EFL_sig(context)     ((context)->uc_mcontext.gregs[EFL])

#define EIP_sig(context)     ((context)->uc_mcontext.gregs[EIP])
#ifdef R_ESP
#define ESP_sig(context)     ((context)->uc_mcontext.gregs[R_ESP])
#else
#define ESP_sig(context)     ((context)->uc_mcontext.gregs[ESP])
#endif
#ifdef TRAPNO
#define TRAP_sig(context)     ((context)->uc_mcontext.gregs[TRAPNO])
#endif

#endif  /* svr4 || SCO_DS */


/* exception code definitions (already defined by FreeBSD/NetBSD) */
#if !defined(__FreeBSD__) && !defined(__NetBSD__) /* FIXME: other BSDs? */
#define T_DIVIDE        0   /* Division by zero exception */
#define T_TRCTRAP       1   /* Single-step exception */
#define T_NMI           2   /* NMI interrupt */
#define T_BPTFLT        3   /* Breakpoint exception */
#define T_OFLOW         4   /* Overflow exception */
#define T_BOUND         5   /* Bound range exception */
#define T_PRIVINFLT     6   /* Invalid opcode exception */
#define T_DNA           7   /* Device not available exception */
#define T_DOUBLEFLT     8   /* Double fault exception */
#define T_FPOPFLT       9   /* Coprocessor segment overrun */
#define T_TSSFLT        10  /* Invalid TSS exception */
#define T_SEGNPFLT      11  /* Segment not present exception */
#define T_STKFLT        12  /* Stack fault */
#define T_PROTFLT       13  /* General protection fault */
#define T_PAGEFLT       14  /* Page fault */
#define T_RESERVED      15  /* Unknown exception */
#define T_ARITHTRAP     16  /* Floating point exception */
#define T_ALIGNFLT      17  /* Alignment check exception */
#define T_MCHK          18  /* Machine check exception */
#define T_CACHEFLT      19  /* Cache flush exception */
#endif
#if defined(__NetBSD__)
#define T_MCHK          19  /* Machine check exception */
#endif

#define T_UNKNOWN     (-1)  /* Unknown fault (TRAP_sig not defined) */

#include "wine/exception.h"
#include "stackframe.h"
#include "global.h"
#include "miscemu.h"
#include "syslevel.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

static sigset_t all_sigs;
static unsigned watches;


/***********************************************************************
 *           get_trap_code
 *
 * Get the trap code for a signal.
 */
static inline int get_trap_code( const SIGCONTEXT *sigcontext )
{
#ifdef TRAP_sig
    return TRAP_sig(sigcontext);
#else
    return T_UNKNOWN;  /* unknown trap code */
#endif
}

/***********************************************************************
 *           get_error_code
 *
 * Get the error code for a signal.
 */
static inline int get_error_code( const SIGCONTEXT *sigcontext )
{
#ifdef ERROR_sig
    return ERROR_sig(sigcontext);
#else
    return 0;
#endif
}

/***********************************************************************
 *           get_cr2_value
 *
 * Get the CR2 value for a signal.
 */
static inline void *get_cr2_value( const SIGCONTEXT *sigcontext )
{
#ifdef CR2_sig
    return (void *)CR2_sig(sigcontext);
#else
    return NULL;
#endif
}


#ifdef __HAVE_VM86
/***********************************************************************
 *           save_vm86_context
 *
 * Set the register values from a vm86 structure.
 */
static void save_vm86_context( CONTEXT *context, const struct vm86plus_struct *vm86 )
{
    context->Eax    = vm86->regs.eax;
    context->Ebx    = vm86->regs.ebx;
    context->Ecx    = vm86->regs.ecx;
    context->Edx    = vm86->regs.edx;
    context->Esi    = vm86->regs.esi;
    context->Edi    = vm86->regs.edi;
    context->Esp    = vm86->regs.esp;
    context->Ebp    = vm86->regs.ebp;
    context->Eip    = vm86->regs.eip;
    context->SegCs  = vm86->regs.cs;
    context->SegDs  = vm86->regs.ds;
    context->SegEs  = vm86->regs.es;
    context->SegFs  = vm86->regs.fs;
    context->SegGs  = vm86->regs.gs;
    context->SegSs  = vm86->regs.ss;
    context->EFlags = vm86->regs.eflags;
}


/***********************************************************************
 *           restore_vm86_context
 *
 * Build a vm86 structure from the register values.
 */
static void restore_vm86_context( const CONTEXT *context, struct vm86plus_struct *vm86 )
{
    vm86->regs.eax    = context->Eax;
    vm86->regs.ebx    = context->Ebx;
    vm86->regs.ecx    = context->Ecx;
    vm86->regs.edx    = context->Edx;
    vm86->regs.esi    = context->Esi;
    vm86->regs.edi    = context->Edi;
    vm86->regs.esp    = context->Esp;
    vm86->regs.ebp    = context->Ebp;
    vm86->regs.eip    = context->Eip;
    vm86->regs.cs     = context->SegCs;
    vm86->regs.ds     = context->SegDs;
    vm86->regs.es     = context->SegEs;
    vm86->regs.fs     = context->SegFs;
    vm86->regs.gs     = context->SegGs;
    vm86->regs.ss     = context->SegSs;
    vm86->regs.eflags = context->EFlags;
}
#endif /* __HAVE_VM86 */


/***********************************************************************
 *           save_context_fast
 */
static void save_context_fast( const SIGCONTEXT *sigcontext, WORD *old_fs )
{
    WORD fs;
    /* get %fs at time of the fault */
#ifdef FS_sig
    fs = FS_sig(sigcontext);
#else
    fs = __get_fs();
#endif

    *old_fs = fs;

    /* now restore a proper %fs for the fault handler */
    if (!IS_SELECTOR_SYSTEM(CS_sig(sigcontext)))  /* 16-bit mode */
    {
        fs = SYSLEVEL_Win16CurrentTeb;
    }
#ifdef __HAVE_VM86
    else if ((void *)EIP_sig(sigcontext) == vm86_return)  /* vm86 mode */
    {
        /* fetch the saved %fs on the stack */
        fs = *(unsigned int *)ESP_sig(sigcontext);
    }
#endif  /* __HAVE_VM86 */

    __set_fs(fs);
}


/***********************************************************************
 *           restore_context_fast
 */
static void restore_context_fast( SIGCONTEXT *sigcontext, WORD fs )
{
#ifdef __HAVE_VM86
    /* check if exception occurred in vm86 mode */
    if ((void *)EIP_sig(sigcontext) == vm86_return &&
        IS_SELECTOR_SYSTEM(CS_sig(sigcontext)) &&
        EAX_sig(sigcontext) == VM86_EAX)
    {
        return;
    }
#endif /* __HAVE_VM86 */

#ifndef FS_sig
    __set_fs( fs );
#endif
}


/***********************************************************************
 *           save_fpu
 *
 * Set the FPU context from a sigcontext.
 */
inline static void save_fpu( CONTEXT *context, const SIGCONTEXT *sigcontext )
{
#ifdef FPU_sig
    if (FPU_sig(sigcontext))
    {
        context->FloatSave = *FPU_sig(sigcontext);
        return;
    }
#endif  /* FPU_sig */
#ifdef __GNUC__
    __asm__ __volatile__( "fnsave %0; fwait" : "=m" (context->FloatSave) );
#endif  /* __GNUC__ */
}


/***********************************************************************
 *           restore_fpu
 *
 * Restore the FPU context to a sigcontext.
 */
inline static void restore_fpu( CONTEXT *context, const SIGCONTEXT *sigcontext )
{
    /* reset the current interrupt status */
    context->FloatSave.StatusWord &= context->FloatSave.ControlWord | 0xffffff80;
#ifdef FPU_sig
    if (sigcontext && FPU_sig(sigcontext))
    {
        *FPU_sig(sigcontext) = context->FloatSave;
        return;
    }
#endif  /* FPU_sig */
#ifdef __GNUC__
    /* avoid nested exceptions */
    __asm__ __volatile__( "frstor %0; fwait" : : "m" (context->FloatSave) );
#endif  /* __GNUC__ */
}


/**********************************************************************
 *		get_fpu_code
 *
 * Get the FPU exception code from the FPU status.
 */
static inline DWORD get_fpu_code( const CONTEXT *context )
{
    DWORD status = context->FloatSave.StatusWord;

    if (status & 0x01)  /* IE */
    {
        if (status & 0x40)  /* SF */
            return EXCEPTION_FLT_STACK_CHECK;
        else
            return EXCEPTION_FLT_INVALID_OPERATION;
    }
    if (status & 0x02) return EXCEPTION_FLT_DENORMAL_OPERAND;  /* DE flag */
    if (status & 0x04) return EXCEPTION_FLT_DIVIDE_BY_ZERO;    /* ZE flag */
    if (status & 0x08) return EXCEPTION_FLT_OVERFLOW;          /* OE flag */
    if (status & 0x10) return EXCEPTION_FLT_UNDERFLOW;         /* UE flag */
    if (status & 0x20) return EXCEPTION_FLT_INEXACT_RESULT;    /* PE flag */
    return EXCEPTION_FLT_INVALID_OPERATION;  /* generic error */
}


/***********************************************************************
 *           SIGNAL_Unblock
 *
 * Unblock signals. Called from EXC_RtlRaiseException.
 */
void SIGNAL_Unblock( void )
{
    SYSDEPS_sigprocmask( SIG_UNBLOCK, &all_sigs, NULL );
}


typedef void (WINAPI *raise_func)( EXCEPTION_RECORD *rec, CONTEXT *context );

/**********************************************************************
 *		make_exception
 *
 * Create a stack frame with an exception record.
 *
 * Stack layout:
 *   ...
 * Exception record
 * Context record
 * Unused return address for __wine_ret_to_32_regs
 * Pointer to context record
 * Pointer to exception record
 * Return address for EXC_RtlRaiseException
 */
static EXCEPTION_RECORD* make_exception( SIGCONTEXT *sigcontext, WORD fs,
                                         raise_func func )
{
    void *stack_top;
    void *stack;
    EXCEPTION_RECORD *record;
    CONTEXT *context;
    extern char Call16_Start, Call16_End;

    stack_top = (void*)ESP_sig(sigcontext);

    /* Check for 16-bit mode, and get the 32-bit stack if so */
    if (!IS_SELECTOR_SYSTEM (CS_sig (sigcontext)))
        stack_top = (void*)NtCurrentTeb()->WOW32Reserved;

    /* Handle exception occurring in the middle of relay code */
    else if (((char *)EIP_sig (sigcontext) >= &Call16_Start) &&
             ((char *)EIP_sig (sigcontext) <  &Call16_End))
        stack_top = (void*)NtCurrentTeb()->WOW32Reserved;

    /* Check for real-mode, and (again) get the 32-bit stack */
    else if (EFL_sig (sigcontext) & 0x00020000)
        stack_top = (void*)NtCurrentTeb()->WOW32Reserved;

    stack = stack_top;

    stack = record = ((EXCEPTION_RECORD *)stack) - 1;
    stack = context = ((CONTEXT *)stack) - 1;
    stack = ((void**)stack) - 1;
    *(void**)stack = 0;
    stack = ((void**)stack) - 1;
    *(void**)stack = context;
    stack = ((void**)stack) - 1;
    *(void**)stack = record;
    stack = ((void**)stack) - 1;
    *(void**)stack = __wine_ret_to_32_regs;

    /* Check for NT bit having been left around by a sloppy program */
    if (EFL_sig (sigcontext) & 0x4000)
       EFL_sig (sigcontext) &= ~0x4000;

    /* save context */
    context->Eax    = EAX_sig(sigcontext);
    context->Ebx    = EBX_sig(sigcontext);
    context->Ecx    = ECX_sig(sigcontext);
    context->Edx    = EDX_sig(sigcontext);
    context->Esi    = ESI_sig(sigcontext);
    context->Edi    = EDI_sig(sigcontext);
    context->Ebp    = EBP_sig(sigcontext);
    context->EFlags = EFL_sig(sigcontext);
    context->Eip    = EIP_sig(sigcontext);
    context->Esp    = ESP_sig(sigcontext);
    context->SegCs  = LOWORD(CS_sig(sigcontext));
    context->SegDs  = LOWORD(DS_sig(sigcontext));
    context->SegEs  = LOWORD(ES_sig(sigcontext));
    context->SegSs  = LOWORD(SS_sig(sigcontext));
    context->SegFs  = fs;

#ifdef GS_sig
    context->SegGs  = LOWORD(GS_sig(sigcontext));
#else
    context->SegGs  = __get_gs();
#endif

    /* change context to return to exception handler */
    ESP_sig(sigcontext) = (DWORD)stack;
    SS_sig(sigcontext) = __get_ss();
    EIP_sig(sigcontext) = (DWORD)func;
    CS_sig(sigcontext) = __get_cs();
    DS_sig(sigcontext) = __get_ds();
    ES_sig(sigcontext) = __get_es();
    FS_sig(sigcontext) = __get_fs();
    GS_sig(sigcontext) = __get_gs();

    /* setup default exception record values */
    record->ExceptionRecord = NULL;
    record->ExceptionFlags = EXCEPTION_CONTINUABLE;
    record->ExceptionAddress = (LPVOID)context->Eip;
    record->NumberParameters = 0;

    return record;
}

/**********************************************************************
 *              store_debug_regs
 *
 * - needs to be called from the _raiser() functions - ie, in the
 * context of the thread where the exception occurred
 */

static void store_debug_regs (CONTEXT *context)
{
   /* Since Dr6 is changed by the CPU, we retrieve the debug registers.
    * It's possible that we might avoid this on non-debug exceptions,
    * but those aren't performance-critical yet.
    */
#ifdef __APPLE__
   x86_debug_state32_t debug_regs;
   mach_msg_type_number_t sc = x86_DEBUG_STATE32_COUNT;

   if (!thread_get_state (mach_thread_self (), x86_DEBUG_STATE32,
                          (thread_state_t)&debug_regs, &sc))
   {
      context->Dr0 = debug_regs.DREG(dr0);
      context->Dr1 = debug_regs.DREG(dr1);
      context->Dr2 = debug_regs.DREG(dr2);
      context->Dr3 = debug_regs.DREG(dr3);
      context->Dr6 = debug_regs.DREG(dr6);
      context->Dr7 = debug_regs.DREG(dr7);
   }
#else
   CONTEXT debug_regs;
   debug_regs.ContextFlags = CONTEXT_DEBUG_REGISTERS;
   if (GetThreadContext (GetCurrentThread (), &debug_regs))
   {
      context->Dr0 = debug_regs.Dr0;
      context->Dr1 = debug_regs.Dr1;
      context->Dr2 = debug_regs.Dr2;
      context->Dr3 = debug_regs.Dr3;
      context->Dr6 = debug_regs.Dr6;
      context->Dr7 = debug_regs.Dr7;
   }
#endif
   else
      ERR ("could not get debug registers\n");
}


#define CONTEXT_DEBUG_REGISTERS_SAME(_c1, _c2)				\
	!(((_c1)->Dr0 ^ (_c2)->Dr0) | ((_c1)->Dr1 ^ (_c2)->Dr1)		\
	  | ((_c1)->Dr2 ^ (_c2)->Dr2) | ((_c1)->Dr3 ^ (_c2)->Dr3)	\
	  | ((_c1)->Dr6 ^ (_c2)->Dr6) | ((_c1)->Dr7 ^ (_c2)->Dr7))


/**********************************************************************
 *              restore_debug_regs
 */
static void restore_debug_regs (CONTEXT *context, CONTEXT *orig_context)
{
   if (orig_context && CONTEXT_DEBUG_REGISTERS_SAME (context, orig_context))
      return;

#ifdef __APPLE__
   x86_debug_state32_t debug_regs;
   mach_msg_type_number_t sc = x86_DEBUG_STATE32_COUNT;

   debug_regs.DREG(dr0) = context->Dr0;
   debug_regs.DREG(dr1) = context->Dr1;
   debug_regs.DREG(dr2) = context->Dr2;
   debug_regs.DREG(dr3) = context->Dr3;
   debug_regs.DREG(dr6) = context->Dr6;
   debug_regs.DREG(dr7) = context->Dr7;
   debug_regs.DREG(dr4) = 0;
   debug_regs.DREG(dr5) = 0;

   if (thread_set_state (mach_thread_self (), x86_DEBUG_STATE32,
                         (thread_state_t)&debug_regs, sc))
      ERR ("couldl not set debug registers\n");
#else
   CONTEXT debug_regs;

   debug_regs.ContextFlags = CONTEXT_DEBUG_REGISTERS;
   debug_regs.Dr0 = context->Dr0;
   debug_regs.Dr1 = context->Dr1;
   debug_regs.Dr2 = context->Dr2;
   debug_regs.Dr3 = context->Dr3;
   debug_regs.Dr6 = context->Dr6;
   debug_regs.Dr7 = context->Dr7;

   if (!SetThreadContext (GetCurrentThread (), &debug_regs))
      ERR ("could not set debug registers\n");
#endif
}


/**********************************************************************
 *		segv_raiser
 */
static void WINAPI segv_raiser( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    CONTEXT orig_context;

    store_debug_regs (context);
    orig_context = *context;

    switch (rec->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        if (rec->NumberParameters == 2) {
            rec->ExceptionCode = VIRTUAL_HandleFault( (LPVOID) rec->ExceptionInformation[1], context );
            if (!rec->ExceptionCode)
            {
               restore_debug_regs (context, &orig_context);
               return;
            }
        }
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        /* FIXME: pass through exception handler first? */
        if (context->EFlags & 0x00040000)
        {
            /* Disable AC flag, return */
            context->EFlags &= ~0x00040000;
            restore_debug_regs (context, &orig_context);
            return;
        }
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        if (INSTR_EmulateInstruction( context ))
        {
           restore_debug_regs (context, &orig_context);
           return;
        }
        break;
    }
    EXC_RtlRaiseException( rec, context );
    restore_debug_regs (context, &orig_context);
}

/**********************************************************************
 *		segv_handler
 *
 * Handler for SIGSEGV and related errors.
 */

HANDLER_DEF(segv_handler)
{
    EXCEPTION_RECORD *rec;
    WORD fs;

    save_context_fast( HANDLER_CONTEXT, &fs );
    rec = make_exception( HANDLER_CONTEXT, fs, segv_raiser );

    switch(get_trap_code(HANDLER_CONTEXT))
    {
    case T_OFLOW:   /* Overflow exception */
        rec->ExceptionCode = EXCEPTION_INT_OVERFLOW;
        break;
    case T_BOUND:   /* Bound range exception */
        rec->ExceptionCode = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
        break;
    case T_PRIVINFLT:   /* Invalid opcode exception */
        rec->ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    case T_STKFLT:  /* Stack fault */
        rec->ExceptionCode = EXCEPTION_STACK_OVERFLOW;
        break;
    case T_SEGNPFLT:  /* Segment not present exception */
    case T_PROTFLT:   /* General protection fault */
    case T_UNKNOWN:   /* Unknown fault code */
	if (get_error_code(HANDLER_CONTEXT) == 0xA)
	{
	    /* The app tried to execute an int 1, but the DPL on the
	     * the IDT entry was too high (GPF/T_PROTFLT) or marked
	     * not present (NP/T_SEGNPFLT).
	     */
	    rec->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
	}
        else
            rec->ExceptionCode = EXCEPTION_PRIV_INSTRUCTION;
        break;
    case T_PAGEFLT:  /* Page fault */
#ifdef CR2_sig
        rec->NumberParameters = 2;
        rec->ExceptionInformation[0] = (get_error_code(HANDLER_CONTEXT) & 2) != 0;
        rec->ExceptionInformation[1] = (DWORD)get_cr2_value(HANDLER_CONTEXT);
#endif /* CR2_sig */
        rec->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        break;
    case T_ALIGNFLT:  /* Alignment check exception */
        rec->ExceptionCode = EXCEPTION_DATATYPE_MISALIGNMENT;
        break;
    default:
        ERR( "Got unexpected trap %d\n", get_trap_code(HANDLER_CONTEXT) );
        /* fall through */
    case T_NMI:       /* NMI interrupt */
    case T_DNA:       /* Device not available exception */
    case T_DOUBLEFLT: /* Double fault exception */
    case T_TSSFLT:    /* Invalid TSS exception */
    case T_RESERVED:  /* Unknown exception */
    case T_MCHK:      /* Machine check exception */
#ifdef T_CACHEFLT
    case T_CACHEFLT:  /* Cache flush exception */
#endif
        rec->ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    }
    restore_context_fast( HANDLER_CONTEXT, fs );
}


/**********************************************************************
 *		trap_raiser
 */
static void WINAPI trap_raiser( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    CONTEXT orig_context;

    store_debug_regs (context);
    orig_context = *context;

    /* Single-step exceptions take precedence over hardware breakpoints
     * in Windows98. It actually seems that the CPU doesn't report the
     * breakpoint (in Dr6).
     */
    if (context->Dr6 & 0xF)
    {
        if (watches) {
            unsigned n;
            for (n=0; n<4; n++) if (context->Dr6 & (1<<n)) {
                LPVOID addr = (LPVOID)((&context->Dr0)[n]);
                LPDWORD stack = (LPVOID)context->Esp;
                TRACE("watchpoint %d, address %p: code at %p wrote 0x%08lx\n", n, addr,
                      (LPVOID)context->Eip, *(LPDWORD)addr);
                TRACE("stack (%p): 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n", stack,
                      stack[0], stack[1], stack[2], stack[3]);
            }
            restore_debug_regs (context, &orig_context);
            return;
        }

        rec->ExceptionCode = EXCEPTION_SINGLE_STEP;

	/* Disable the breakpoint register that triggered the exception.
	 *
	 * This is a guess. I have an exception handler that returns
	 * ExceptionContinueExecution without doing anything to the
	 * debug registers.
	 */
	if (context->Dr6 & 1)
	    context->Dr7 &= ~(3 << 0);
	if (context->Dr6 & 2)
	    context->Dr7 &= ~(3 << 1);
	if (context->Dr6 & 4)
	    context->Dr7 &= ~(3 << 2);
	if (context->Dr6 & 8)
	    context->Dr7 &= ~(3 << 3);
    }
    else if (context->Dr6 & 0x4000)
    {
	/* single-step due to trap flag */
        rec->ExceptionCode = EXCEPTION_SINGLE_STEP;
        context->EFlags &= ~0x100;  /* clear trap flag */
    }
    else if (rec->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
	/* breakpoint due to int3 (0xCC) instruction */
	/* back up over the int3 instruction */
        rec->ExceptionAddress = (char *)rec->ExceptionAddress - 1;
#ifdef __APPLE__
        context->Eip = context->Eip - 1;
#endif
	/* FIXME: I forgot to check whether or not context->Eip is
	 * modified as well. */
     }
    else
    {
	/* This shouldn't happen.
	 * The CPU can also generate breakpoint exceptions due to:
	 * - general detect (only for ring 0)
	 * - task switch to debugged task (rarely used)
	 * - int 1 (doesn't happen under Linux since the DPL for the
	 *	    interrupt gate is too high)
	 */
	WARN("unknown exception reason: 0x%08lx, 0x%08lx\n", context->Dr6,
	     context->Dr7);
	rec->ExceptionCode = EXCEPTION_BREAKPOINT;
    }

    EXC_RtlRaiseException( rec, context );
    restore_debug_regs (context, &orig_context);
}


/**********************************************************************
 *		trap_handler
 *
 * Handler for SIGTRAP.
 */
HANDLER_DEF(trap_handler)
{
    EXCEPTION_RECORD *rec;
    WORD fs;

    save_context_fast( HANDLER_CONTEXT, &fs );
    rec = make_exception( HANDLER_CONTEXT, fs, trap_raiser );

    if (get_trap_code (HANDLER_CONTEXT) == T_BPTFLT)
       rec->ExceptionCode = EXCEPTION_BREAKPOINT;
    else
       rec->ExceptionCode = 0;

    /* ensure trap flag isn't set so that trap_raiser can actually run;
       it'll get reset appropriately if needed in __wine_ret_to_32_regs */
    if (EFL_sig (HANDLER_CONTEXT) & 0x100)
       EFL_sig (HANDLER_CONTEXT) &= ~0x100;

    restore_context_fast( HANDLER_CONTEXT, fs );
}

/**********************************************************************
 *		fpe_raiser
 */
static void WINAPI fpe_raiser( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    store_debug_regs (context);
    EXC_RtlRaiseException( rec, context );
    restore_fpu( context, NULL );
    restore_debug_regs (context, NULL);
}


/**********************************************************************
 *		fpe_handler
 *
 * Handler for SIGFPE.
 */
HANDLER_DEF(fpe_handler)
{
    EXCEPTION_RECORD *rec;
    CONTEXT *context;
    WORD fs;

    save_context_fast( HANDLER_CONTEXT, &fs );
    rec = make_exception( HANDLER_CONTEXT, fs, fpe_raiser );
    /* make_exception puts the context right before the exception record */
    context = ((CONTEXT*)rec) - 1;
    save_fpu( context, HANDLER_CONTEXT );

    switch(get_trap_code(HANDLER_CONTEXT)) {
    case T_DIVIDE:   /* Division by zero exception */
        rec->ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO;
        break;
    case T_FPOPFLT:   /* Coprocessor segment overrun */
        rec->ExceptionCode = EXCEPTION_FLT_INVALID_OPERATION;
        break;
    case T_ARITHTRAP:  /* Floating point exception */
    case T_UNKNOWN:    /* Unknown fault code */
        rec->ExceptionCode = get_fpu_code( context );
        break;
    default:
        ERR( "Got unexpected trap %d\n", get_trap_code(HANDLER_CONTEXT) );
        rec->ExceptionCode = EXCEPTION_FLT_INVALID_OPERATION;
        break;
    }
    restore_context_fast( HANDLER_CONTEXT, fs );
}


#ifdef __HAVE_VM86
/**********************************************************************
 *		vm86_pend_raiser
 *
 * Handler for SIGUSR2, which we use to set the vm86 pending flag.
 */
static void WINAPI vm86_pend_raiser( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    TEB *teb = NtCurrentTeb();
    struct vm86plus_struct *vm86 = (struct vm86plus_struct*)(teb->vm86_ptr);

    store_debug_regs (context);
    rec->NumberParameters        = 1;
    rec->ExceptionInformation[0] = 0;

    /* __wine_enter_vm86() merges the vm86_pending flag in safely */
    teb->vm86_pending |= VIP_MASK;
    /* see if we were in VM86 mode */
    if (context->EFlags & 0x00020000)
    {
        /* seems so, also set flag in signal context */
        if (context->EFlags & VIP_MASK)
        {
           restore_debug_regs (context, NULL);
           return;
        }
        context->EFlags |= VIP_MASK;
        vm86->regs.eflags |= VIP_MASK; /* no exception recursion */
        if (context->EFlags & VIF_MASK) {
            /* VIF is set, throw exception */
            teb->vm86_pending = 0;
            teb->vm86_ptr = NULL;
            rec->ExceptionAddress = (LPVOID)context->Eip;
            EXC_RtlRaiseException( rec, context );
            teb->vm86_ptr = vm86;
        }
    }
    else if (vm86)
    {
        /* not in VM86, but possibly setting up for it */
        if (vm86->regs.eflags & VIP_MASK)
        {
            restore_debug_regs (context, NULL);
            return;
        }
        vm86->regs.eflags |= VIP_MASK;
        if (((char*)context->Eip >= (char*)vm86_return) &&
            ((char*)context->Eip <= (char*)vm86_return_end) &&
            (VM86_TYPE(context->Eax) != VM86_SIGNAL)) {
            /* exiting from VM86, can't throw */
            restore_debug_regs (context, NULL);
            return;
        }
        if (vm86->regs.eflags & VIF_MASK) {
            /* VIF is set, throw exception */
            CONTEXT vcontext;
            teb->vm86_pending = 0;
            teb->vm86_ptr = NULL;
            save_vm86_context( &vcontext, vm86 );
            rec->ExceptionAddress = (LPVOID)vcontext.Eip;
            EXC_RtlRaiseException( rec, &vcontext );
            teb->vm86_ptr = vm86;
            restore_vm86_context( &vcontext, vm86 );
        }
    }
    restore_debug_regs (context, NULL);
}
#endif /* __HAVE_VM86 */

#if 0 && defined( __HAVE_VM86 ) /* Now using SIGUSR2 for eject handling */
/**********************************************************************
 *		usr2_handler
 *
 * Handler for SIGUSR2.
 * We use it to signal that the running __wine_enter_vm86() should
 * immediately set VIP_MASK, causing pending events to be handled
 * as early as possible.
 */
HANDLER_DEF(usr2_handler)
{
    EXCEPTION_RECORD *rec;
    WORD fs;

    save_context_fast( HANDLER_CONTEXT, &fs );
    rec = make_exception( HANDLER_CONTEXT, fs, vm86_pend_raiser );
    rec->ExceptionCode = EXCEPTION_VM86_STI;
    restore_context_fast( HANDLER_CONTEXT, fs );
}

#else /* Now using SIGUSR2 for eject handling */
                                                                                     
/**********************************************************************
 *             usr2_handler
 *
 * Handler for SIGUSR2.  Call the wineserver to deal with eject request.
 * Grab the drive to eject from a file in the config dir
 *
 */
const char *client_cdrom_drive_filename = "wineserver_client_eject_drive";

static int get_cdrom_eject_drive(void)
{
    char drive_letter[1];
    char drive_mounted = 0;
    char *filebuf;
    size_t len = 0;
    FILE *cdrom_drive_file = NULL;
    char tidstring[255];
        
    /* Get a string representing the tid */
    sprintf(tidstring, "%lu", NtCurrentTeb()->unix_tid_or_pid);

    len = strlen(get_config_dir()) + strlen(client_cdrom_drive_filename) + strlen(tidstring) +  1 + 1 /* '/0' */;
    filebuf = (char*)malloc(len);
    if (!filebuf)
        return -1;

    strcpy(filebuf, get_config_dir());
    strcat(filebuf, "/");
    strcat(filebuf, client_cdrom_drive_filename);
    strcat(filebuf, tidstring);

    TRACE("Reading: %s\n", filebuf);

    /* Read the drive letter out of the tmp file */
    cdrom_drive_file = fopen (filebuf, "r");

    if (cdrom_drive_file)
    {
        /* Get Drive Letter */
        len = fread(drive_letter, 1, 1, cdrom_drive_file);

        /* Get whether drive was mounted or unmounted */
        if (len)
            len = fread(&drive_mounted, 1, 1, cdrom_drive_file);
        
        fclose(cdrom_drive_file);

        /* Clean up after ourselves */
        unlink(filebuf);
       
    }

    free(filebuf);

    if (len && cdrom_drive_file && drive_mounted)
    {
        TRACE("Got mount drive letter: %c\n", drive_letter[0]);
        return -2;
    }
    
    if (len && cdrom_drive_file)
    {
        TRACE("Got eject drive letter: %c\n", drive_letter[0]);
        return drive_letter[0];
    }

    TRACE("Got no eject drive letter\n");

    /* If we get here, eject all disks */
    return -1; 
}


HANDLER_DEF(usr2_handler)
{
    int drive = get_cdrom_eject_drive();
    
    if (drive != -2)
        FILE_ForceEjectCDDrive(drive);
    else
        DRIVE_UpdateCDMountpoints();
}
#endif /* 0 && __HAVE_VM86 */




#if defined( __HAVE_VM86 )
/**********************************************************************
 *		alrm_handler
 *
 * Handler for SIGALRM.
 * Increases the alarm counter and sets the vm86 pending flag.
 */
HANDLER_DEF(alrm_handler)
{
    EXCEPTION_RECORD *rec;
    WORD fs;

    save_context_fast( HANDLER_CONTEXT, &fs );
    NtCurrentTeb()->alarms++;
    rec = make_exception( HANDLER_CONTEXT, fs, vm86_pend_raiser );
    rec->ExceptionCode = EXCEPTION_VM86_STI;
    restore_context_fast( HANDLER_CONTEXT, fs );
}
#endif /* __HAVE_VM86 */

/**********************************************************************
 *		int_handler
 *
 * Handler for SIGINT.
 */
HANDLER_DEF(int_handler)
{
    WORD fs;

    extern int CONSOLE_HandleCtrlC(void);

    save_context_fast( HANDLER_CONTEXT, &fs );

    if (!CONSOLE_HandleCtrlC())
    {
        EXCEPTION_RECORD *rec;

        rec = make_exception( HANDLER_CONTEXT, fs, EXC_RtlRaiseException );
        rec->ExceptionCode = CONTROL_C_EXIT;
    }

    restore_context_fast( HANDLER_CONTEXT, fs );
}

/**********************************************************************
 *		abrt_handler
 *
 * Handler for SIGABRT.
 */
HANDLER_DEF(abrt_handler)
{
    EXCEPTION_RECORD *rec;
    WORD fs;

    save_context_fast( HANDLER_CONTEXT, &fs );
    rec = make_exception( HANDLER_CONTEXT, fs, EXC_RtlRaiseException );
    rec->ExceptionCode  = EXCEPTION_WINE_ASSERTION;
    rec->ExceptionFlags = EH_NONCONTINUABLE;
    restore_context_fast( HANDLER_CONTEXT, fs );
}

/**********************************************************************
 *             sigterm_handler
 *
 * Handler for SIGTERM.
 *
 * The wineserver will send a term signal to threads under certain
 * circumstances thinking that only the thread will get the
 * signal rather than the whole process. Rather than have the default
 * handler which will print "Terminated" at the console, attempt a
 * slightly nicer shutdown.
 */
#ifdef USE_PTHREADS
HANDLER_DEF(sigterm_handler)
{
    SYSDEPS_AbortThread( 0 );
}
#endif

/**********************************************************************
 *		usr1_handler
 *
 * Handler for SIGUSR1.
 * The window driver (x11drv) use it for interthread signaling.
 */
extern int __wine_recv_unix_signal(int *code, size_t *size, void *data);
extern void __wine_valloc(void *data);
extern void __wine_vfree(void *data);
extern void __wine_mkthread(void *data);
extern void __wine_clearFileCache(void *data);
static void (*usr1_callback)(CONTEXT*);

HANDLER_DEF(usr1_handler)
{
    WORD fs;
    int err, code = 0;
    size_t size = 0;
    void* data = NULL;

    save_context_fast( HANDLER_CONTEXT, &fs );

    do {
        err = __wine_recv_unix_signal(&code, &size, data);
        if (err == STATUS_BUFFER_OVERFLOW) {
            void *ndata;
            if (size) size *= 2; else size = 128;
            /* can't use heap routines from here */
            ndata = realloc( data, size );
            if (!ndata) {
                ERR("out of memory\n");
            }
            data = ndata;
            continue;
        }
        switch (code) {
        case -1:
            /* unhandled error */
            TRACE("error %08x\n", err);
            break;
        case USC_NONE:
            /* all done */
            break;
        case USC_CALLBACK:
            if (usr1_callback) usr1_callback( NULL );
            break;
        case USC_CDEJECT:
            /* usr2 stuff should be moved here, hasn't been done yet */
            break;
        case USC_VALLOC:
            __wine_valloc(data);
            break;
        case USC_VFREE:
            __wine_vfree(data);
            break;
        case USC_MKTHREAD:
            __wine_mkthread(data);
            break;
        case USC_CLRFILECACHE:
            __wine_clearFileCache(data);
            break;
        }
        if (size) {
            free( data );
            data = NULL;
            size = 0;
        }
    } while (code);

    restore_context_fast( HANDLER_CONTEXT, fs );
}



/***********************************************************************
 *           set_handler
 *
 * Set a signal handler
 */
static int set_handler( int sig, int have_sigaltstack, void (*func)() )
{
    struct sigaction sig_act;

#if defined( linux ) && !defined( USE_PTHREADS )
    if (!have_sigaltstack && NtCurrentTeb()->signal_stack)
    {
        struct kernel_sigaction sig_act;
        sig_act.ksa_handler = func;
        sig_act.ksa_flags   = SA_RESTART;
        sig_act.ksa_mask    = (1 << (SIGINT-1)) |
                              (1 << (SIGALRM-1)) |
                              (1 << (SIGUSR1-1)) |
                              (1 << (SIGSEGV-1));
        /* point to the top of the stack */
        sig_act.ksa_restorer = (char *)NtCurrentTeb()->signal_stack + SIGNAL_STACK_SIZE;
        return wine_sigaction( sig, &sig_act, NULL );
    }
#endif  /* linux */
    sig_act.sa_handler = func;
    sigemptyset( &sig_act.sa_mask );
    sigaddset( &sig_act.sa_mask, SIGINT );
    sigaddset( &sig_act.sa_mask, SIGALRM );
    sigaddset( &sig_act.sa_mask, SIGUSR1 );
    /* blocking SIGSEGV is nasty, but there's some strange SIGSEGVs going on in the SIGUSR1
     * handler that I don't know where comes from, usually when running copy protection code,
     * and blocking SIGSEGV seems to avoid it... */
    sigaddset( &sig_act.sa_mask, SIGSEGV );

#ifdef linux
    sig_act.sa_flags = SA_RESTART;
#elif defined (__svr4__) || defined(_SCO_DS) || defined(__APPLE__)
    sig_act.sa_flags = SA_SIGINFO | SA_RESTART;
#else
    sig_act.sa_flags = 0;
#endif

#ifdef SA_ONSTACK
    if (have_sigaltstack) sig_act.sa_flags |= SA_ONSTACK;
#endif
    return sigaction( sig, &sig_act, NULL );
}

/* Setup the signal stack to use new_sas if provided. */
int SIGNAL_SetupSignalStack( void )
{
#ifdef HAVE_SIGALTSTACK
#ifdef SIGALTSTACK_TAKES_STACK_T
    stack_t ss;
#else
    struct sigaltstack ss;
#endif

    ss.ss_sp = NtCurrentTeb()->signal_stack;
    ss.ss_size = SIGNAL_STACK_SIZE;
    ss.ss_flags = 0;

    if (!NtCurrentTeb()->signal_stack) return 0;

    return wine_sigaltstack(&ss, NULL) == 0;
#else

    return 0;

#endif
}

int SIGNAL_RestoreSignalStack( void )
{
    return SIGNAL_SetupSignalStack();
}

int SIGNAL_SuspendSignalStack( void )
{
#ifdef HAVE_SIGALTSTACK
#ifdef SIGALTSTACK_TAKES_STACK_T
    stack_t nosas;
#else
    struct sigaltstack nosas;
#endif

    if ( !NtCurrentTeb()->signal_stack ) return 0;

    nosas.ss_sp = NULL;
    nosas.ss_size = 0;
    nosas.ss_flags = SS_DISABLE;

    return wine_sigaltstack( &nosas, NULL ) == 0;

#else

    return 0;

#endif
}


#ifdef __APPLE__

static void build_fake_signal_context (x86_thread_state32_t *thread_state,
                                       ucontext_t *context)
{
   context->uc_mcontext->DREG(ss).DREG(eax) = thread_state->DREG(eax);
   context->uc_mcontext->DREG(ss).DREG(ebx) = thread_state->DREG(ebx);
   context->uc_mcontext->DREG(ss).DREG(ecx) = thread_state->DREG(ecx);
   context->uc_mcontext->DREG(ss).DREG(edx) = thread_state->DREG(edx);
   context->uc_mcontext->DREG(ss).DREG(edi) = thread_state->DREG(edi);
   context->uc_mcontext->DREG(ss).DREG(esi) = thread_state->DREG(esi);
   context->uc_mcontext->DREG(ss).DREG(ebp) = thread_state->DREG(ebp);
   context->uc_mcontext->DREG(ss).DREG(esp) = thread_state->DREG(esp);
   context->uc_mcontext->DREG(ss).DREG(ss) = thread_state->DREG(ss);
   context->uc_mcontext->DREG(ss).DREG(eflags) = thread_state->DREG(eflags);
   context->uc_mcontext->DREG(ss).DREG(eip) = thread_state->DREG(eip);
   context->uc_mcontext->DREG(ss).DREG(cs) = thread_state->DREG(cs);
   context->uc_mcontext->DREG(ss).DREG(ds) = thread_state->DREG(ds);
   context->uc_mcontext->DREG(ss).DREG(es) = thread_state->DREG(es);
   context->uc_mcontext->DREG(ss).DREG(fs) = thread_state->DREG(fs);
   context->uc_mcontext->DREG(ss).DREG(gs) = thread_state->DREG(gs);
}


static void restore_from_signal_context(x86_thread_state32_t *thread_state,
                                        ucontext_t *context)
{
   thread_state->DREG(eax) = context->uc_mcontext->DREG(ss).DREG(eax);
   thread_state->DREG(ebx) = context->uc_mcontext->DREG(ss).DREG(ebx);
   thread_state->DREG(ecx) = context->uc_mcontext->DREG(ss).DREG(ecx);
   thread_state->DREG(edx) = context->uc_mcontext->DREG(ss).DREG(edx);
   thread_state->DREG(edi) = context->uc_mcontext->DREG(ss).DREG(edi);
   thread_state->DREG(esi) = context->uc_mcontext->DREG(ss).DREG(esi);
   thread_state->DREG(ebp) = context->uc_mcontext->DREG(ss).DREG(ebp);
   thread_state->DREG(esp) = context->uc_mcontext->DREG(ss).DREG(esp);
   thread_state->DREG(ss) = context->uc_mcontext->DREG(ss).DREG(ss);
   thread_state->DREG(eflags) = context->uc_mcontext->DREG(ss).DREG(eflags);
   thread_state->DREG(eip) = context->uc_mcontext->DREG(ss).DREG(eip);
   thread_state->DREG(cs) = context->uc_mcontext->DREG(ss).DREG(cs);
   thread_state->DREG(ds) = context->uc_mcontext->DREG(ss).DREG(ds);
   thread_state->DREG(es) = context->uc_mcontext->DREG(ss).DREG(es);
   thread_state->DREG(fs) = context->uc_mcontext->DREG(ss).DREG(fs);
   thread_state->DREG(gs) = context->uc_mcontext->DREG(ss).DREG(gs);
}


kern_return_t catch_exception_raise (mach_port_t ExceptionPort,
                                     mach_port_t ExceptionThread,
                                     mach_port_t ExceptionTask,
                                     exception_type_t Exception,
                                     exception_data_t Data,
                                     mach_msg_type_number_t DataCount)
{
   return KERN_NOT_SUPPORTED;
}


kern_return_t catch_exception_raise_state (mach_port_t ExceptionPort,
                                           exception_type_t Exception,
                                           exception_data_t Data,
                                           mach_msg_type_number_t DataCount,
                                           int *Flavour,
                                           thread_state_t OldState,
                                           mach_msg_type_number_t OldStateCount,
                                           thread_state_t NewState,
                                           mach_msg_type_number_t *NewStateCount)
{
   return KERN_NOT_SUPPORTED;
}


kern_return_t
catch_exception_raise_state_identity (mach_port_t ExceptionPort,
                                      mach_port_t ExceptionThread,
                                      mach_port_t ExceptionTask,
                                      exception_type_t Exception,
                                      exception_data_t Data,
                                      mach_msg_type_number_t DataCount,
                                      int *Flavour,
                                      thread_state_t OldState,
                                      mach_msg_type_number_t OldStateCount,
                                      thread_state_t NewState,
                                      mach_msg_type_number_t *NewStateCount)
{
   mach_msg_type_number_t sc = x86_EXCEPTION_STATE32_COUNT;
   kern_return_t res;
   ucontext_t ucontext;
   mcontext_t uc_mcontext;
   x86_thread_state32_t x86State;

   if ((OldStateCount * sizeof (natural_t)) < sizeof (x86_thread_state32_t))
   {
      fprintf (stderr, "Old state size too small!\n");
      return KERN_INVALID_ARGUMENT;
   }

   if (((*NewStateCount) * sizeof (natural_t)) < sizeof (x86_thread_state32_t))
   {
      fprintf (stderr, "New state size too small!\n");
      return KERN_INVALID_ARGUMENT;
   }

   x86State = *(x86_thread_state32_t *)OldState;
   uc_mcontext = alloca (sizeof (*uc_mcontext));
   memset (uc_mcontext, 0, sizeof (*uc_mcontext));
   memset (&ucontext, 0, sizeof (ucontext));
   ucontext.uc_mcontext = uc_mcontext;

   build_fake_signal_context (&x86State, &ucontext);

   res = thread_get_state (ExceptionThread, x86_EXCEPTION_STATE32,
                           (thread_state_t)&uc_mcontext->DREG(es), &sc);
   if (res)
      fprintf (stderr,
               "thread_get_state x86_EXCEPTION_STATE32 failed! res: 0x%x\n",
               res);

   switch (Exception)
   {
      case EXC_BREAKPOINT:
         trap_handler (SIGTRAP, NULL, &ucontext);
         break;

      case EXC_BAD_ACCESS:
      case EXC_BAD_INSTRUCTION:
         segv_handler (SIGSEGV, NULL, &ucontext);
         break;

      case EXC_ARITHMETIC:
         fpe_handler (SIGFPE, NULL, &ucontext);
         break;

      default:
         return KERN_NOT_SUPPORTED;
   }

   restore_from_signal_context (&x86State, &ucontext);

   *(x86_thread_state32_t *)NewState = x86State;
   *NewStateCount = sizeof (x86_thread_state32_t) / sizeof (natural_t);
   return KERN_SUCCESS;
}


void *MachExceptionThread (void *Data)
{
   sigset_t SigSet;

   /* Block all signals so that they go to the Windows threads */
   sigfillset (&SigSet);
   if (pthread_sigmask (SIG_BLOCK, &SigSet, NULL))
      fprintf (stderr,
               "WARNING: MachExceptionThread isn't blocking all signals!\n");

   mach_msg_server (exc_server, 2048, gExceptionPort, 0);
   return NULL;
}

                                      
#endif

/**********************************************************************
 *		SIGNAL_Init
 */
BOOL SIGNAL_Init(void)
{
    int have_sigaltstack = SIGNAL_SetupSignalStack();

#if defined(__APPLE__) && !defined(ENABLE_MAC_DEBUGGING)
    kern_return_t res;

    if (!gExceptionPort)
    {
       pthread_t thread;

       res = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE,
                                 &gExceptionPort);
       if (res != KERN_SUCCESS)
       {
          fprintf (stderr, "mach_port_allocate failed: 0x%x\n", res);
          return FALSE;
       }

       res = mach_port_insert_right (mach_task_self (), gExceptionPort,
                                     gExceptionPort, MACH_MSG_TYPE_MAKE_SEND);
       if (res != KERN_SUCCESS)
       {
          fprintf (stderr, "mach_port_insert_right failed: 0x%x\n", res);
          return FALSE;
       }

       if (pthread_create (&thread, NULL, MachExceptionThread, NULL))
       {
          fprintf (stderr, "pthread_create failed: 0x%x\n", errno);
          return FALSE;
       }
    }

    res = thread_set_exception_ports (mach_thread_self (),
                                      EXC_MASK_BREAKPOINT | EXC_MASK_BAD_ACCESS |
                                      EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC,
                                      gExceptionPort,
                                      EXCEPTION_STATE_IDENTITY,
                                      x86_THREAD_STATE32);
    if (res != KERN_SUCCESS)
    {
       fprintf (stderr, "thread_set_exception_ports failed: 0x%x\n", res);
       return FALSE;
    }

#endif

    sigfillset( &all_sigs );

    /* automatic child reaping to avoid zombies */
    signal( SIGCHLD, SIG_IGN );

#if !defined(__APPLE__) || defined(ENABLE_MAC_DEBUGGING)
    if (set_handler( SIGFPE,  have_sigaltstack, (void (*)())fpe_handler ) == -1) goto error;
    if (set_handler( SIGSEGV, have_sigaltstack, (void (*)())segv_handler ) == -1) goto error;
#if defined(SIGTRAP)
    if (set_handler( SIGTRAP, have_sigaltstack, (void (*)())trap_handler ) == -1) goto error;
#endif
#endif
    if (set_handler( SIGINT,  have_sigaltstack, (void (*)())int_handler ) == -1) goto error;
    if (set_handler( SIGILL,  have_sigaltstack, (void (*)())segv_handler ) == -1) goto error;
    if (set_handler( SIGABRT, have_sigaltstack, (void (*)())abrt_handler ) == -1) goto error;
#ifdef SIGBUS
    if (set_handler( SIGBUS,  have_sigaltstack, (void (*)())segv_handler ) == -1) goto error;
#endif


    if (set_handler( SIGUSR1, have_sigaltstack, (void (*)())usr1_handler ) == -1) goto error;
#ifdef __HAVE_VM86
    if (set_handler( SIGALRM, have_sigaltstack, (void (*)())alrm_handler ) == -1) goto error;
#endif

#if 0 && defined( __HAVE_VM86 ) /* Now using SIGUSR2 for eject handling, not vm86 */
    if (set_handler( SIGUSR2, have_sigaltstack, (void (*)())usr2_handler ) == -1) goto error;
#else
    if (set_handler( SIGUSR2, have_sigaltstack, (void (*)())usr2_handler ) == -1) goto error;
#endif

#ifdef USE_PTHREADS
    if (set_handler( SIGTERM, have_sigaltstack, (void (*)())sigterm_handler ) == -1) goto error;
#endif

    return TRUE;

 error:
    perror("sigaction");
    return FALSE;
}


/**********************************************************************
 *		SIGNAL_Reset
 */
void SIGNAL_Reset(void)
{
    sigset_t block_set;

    /* block the async signals */
    sigemptyset( &block_set );
    sigaddset( &block_set, SIGALRM );
    sigaddset( &block_set, SIGIO );
    sigaddset( &block_set, SIGHUP );
    sigaddset( &block_set, SIGUSR1 );
    sigaddset( &block_set, SIGUSR2 );

    SYSDEPS_sigprocmask( SIG_BLOCK, &block_set, NULL );

    /* restore default handlers. Do not do this as signal handlers are
     * shared if we're using pthreads.
     */
#if !defined( USE_PTHREADS )
      signal( SIGINT, SIG_DFL );
      signal( SIGFPE, SIG_DFL );
      signal( SIGSEGV, SIG_DFL );
      signal( SIGILL, SIG_DFL );
# ifdef SIGBUS
      signal( SIGBUS, SIG_DFL );
# endif
# ifdef SIGTRAP
      signal( SIGTRAP, SIG_DFL );
# endif
#endif /* USE_PTHREADS */

    SIGNAL_SuspendSignalStack();
}


void __wine_hook_user_signal( void (*func)(CONTEXT*) )
{
    usr1_callback = func;
}

void __wine_send_user_signal( DWORD tid )
{
    SERVER_START_REQ( send_fast_unix_signal )
    {
        req->thread = tid;
        req->code = USC_CALLBACK;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

void wine_start_memwatch( unsigned n, const void * a, unsigned size )
{
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(GetCurrentThread(), &ctx);
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (ctx.Dr7 & (0x3 << (n*2))) watches--;
    ctx.Dr7 &= ~((0xf0000 << (n*4)) | (0x3 << (n*2)));
    (&ctx.Dr0)[n] = (DWORD)a;
    ctx.Dr7 |= (0x10000 << (n*4)) | (0x1 << (n*2));
    ctx.Dr7 |= (size-1) << (18+n*4);
    watches++;
    SetThreadContext(GetCurrentThread(), &ctx);
}

void wine_stop_memwatch( unsigned n )
{
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(GetCurrentThread(), &ctx);
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (ctx.Dr7 & (0x3 << (n*2))) watches--;
    ctx.Dr7 &= ~((0xf0000 << (n*4)) | (0x3 << (n*2)));
    SetThreadContext(GetCurrentThread(), &ctx);
}

#ifdef __HAVE_VM86
/**********************************************************************
 *		__wine_enter_vm86
 *
 * Enter vm86 mode with the specified register context.
 */
void __wine_enter_vm86( CONTEXT *context )
{
    EXCEPTION_RECORD rec;
    TEB *teb = NtCurrentTeb();
    int res;
    struct vm86plus_struct vm86;

    memset( &vm86, 0, sizeof(vm86) );
    for (;;)
    {
        restore_vm86_context( context, &vm86 );
        /* Linux doesn't preserve pending flag (VIP_MASK) on return,
         * so save it on entry, just in case */
        teb->vm86_pending |= (context->EFlags & VIP_MASK);
        /* Work around race conditions with signal handler
         * (avoiding sigprocmask for performance reasons) */
        teb->vm86_ptr = &vm86;
        vm86.regs.eflags |= teb->vm86_pending;
        /* Check for VIF|VIP here, since vm86_enter doesn't */
        if ((vm86.regs.eflags & (VIF_MASK|VIP_MASK)) == (VIF_MASK|VIP_MASK)) {
            teb->vm86_ptr = NULL;
            teb->vm86_pending = 0;
            context->EFlags |= VIP_MASK;
            rec.ExceptionCode = EXCEPTION_VM86_STI;
            rec.ExceptionInformation[0] = 0;
            goto cancel_vm86;
        }

        do
        {
            res = vm86_enter( &teb->vm86_ptr ); /* uses and clears teb->vm86_ptr */
            if (res < 0)
            {
                errno = -res;
                return;
            }
        } while (VM86_TYPE(res) == VM86_SIGNAL);

        save_vm86_context( context, &vm86 );
        context->EFlags |= teb->vm86_pending;

        rec.ExceptionFlags          = EXCEPTION_CONTINUABLE;
        rec.ExceptionRecord         = NULL;
        rec.ExceptionAddress        = (LPVOID)context->Eip;
        rec.NumberParameters        = 0;

        switch(VM86_TYPE(res))
        {
        case VM86_UNKNOWN: /* unhandled GP fault - IO-instruction or similar */
            rec.ExceptionCode = EXCEPTION_PRIV_INSTRUCTION;
            segv_raiser( &rec, context );
            continue;
        case VM86_TRAP: /* return due to DOS-debugger request */
            rec.ExceptionCode = VM86_ARG (res);
            trap_raiser( &rec, context );
            continue;
        case VM86_INTx: /* int3/int x instruction (ARG = x) */
            rec.ExceptionCode = EXCEPTION_VM86_INTx;
            break;
        case VM86_STI: /* sti/popf/iret instruction enabled virtual interrupts */
            teb->vm86_pending = 0;
            rec.ExceptionCode = EXCEPTION_VM86_STI;
            break;
        case VM86_PICRETURN: /* return due to pending PIC request */
            rec.ExceptionCode = EXCEPTION_VM86_PICRETURN;
            break;
        default:
            ERR( "unhandled result from vm86 mode %x\n", res );
            continue;
        }
        rec.ExceptionInformation[0] = VM86_ARG(res);
cancel_vm86:
        rec.NumberParameters        = 1;
        EXC_RtlRaiseException( &rec, context );
    }
}

#else /* __HAVE_VM86 */
void __wine_enter_vm86( CONTEXT *context )
{
    MESSAGE("vm86 mode not supported on this platform\n");
}
#endif /* __HAVE_VM86 */

/**********************************************************************
 *		DbgBreakPoint   (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( DbgBreakPoint, "int $3; ret");

/**********************************************************************
 *		DbgUserBreakPoint   (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( DbgUserBreakPoint, "int $3; ret");

#endif  /* __i386__ */
