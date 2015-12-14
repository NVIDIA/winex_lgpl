#ifndef __HARDWARE_H
#define __HARDWARE_H

#include "wine/port.h"

/* Macros for easier access to i386 context registers */
#if defined(__i386__)

#define AX_reg(context)      (*(WORD*)&(context)->Eax)
#define BX_reg(context)      (*(WORD*)&(context)->Ebx)
#define CX_reg(context)      (*(WORD*)&(context)->Ecx)
#define DX_reg(context)      (*(WORD*)&(context)->Edx)
#define SI_reg(context)      (*(WORD*)&(context)->Esi)
#define DI_reg(context)      (*(WORD*)&(context)->Edi)
#define BP_reg(context)      (*(WORD*)&(context)->Ebp)

#define AL_reg(context)      (*(BYTE*)&(context)->Eax)
#define AH_reg(context)      (*((BYTE*)&(context)->Eax + 1))
#define BL_reg(context)      (*(BYTE*)&(context)->Ebx)
#define BH_reg(context)      (*((BYTE*)&(context)->Ebx + 1))
#define CL_reg(context)      (*(BYTE*)&(context)->Ecx)
#define CH_reg(context)      (*((BYTE*)&(context)->Ecx + 1))
#define DL_reg(context)      (*(BYTE*)&(context)->Edx)
#define DH_reg(context)      (*((BYTE*)&(context)->Edx + 1))

#define SET_CFLAG(context)   ((context)->EFlags |= 0x0001)
#define RESET_CFLAG(context) ((context)->EFlags &= ~0x0001)
#define SET_ZFLAG(context)   ((context)->EFlags |= 0x0040)
#define RESET_ZFLAG(context) ((context)->EFlags &= ~0x0040)
#define ISV86(context)       ((context)->EFlags & 0x00020000)

#endif /* defined(__i386__) */

#if defined(__i386__)

/* Segment register access */
#ifdef __GNUC__
# define __DEFINE_GET_SEG(seg) \
    extern inline unsigned short __get_##seg(void) \
    { unsigned short res; __asm__("movw %%" #seg ",%w0" : "=r"(res)); return res; }
# define __DEFINE_SET_SEG(seg) \
    extern inline void __set_##seg(int val) { __asm__("movw %w0,%%" #seg : : "r" (val)); }
#elif defined(_MSC_VER)
# define __DEFINE_GET_SEG(seg) \
    extern inline unsigned short __get_##seg(void) { unsigned short res; __asm { mov res, seg } return res; }
# define __DEFINE_SET_SEG(seg) \
    extern inline void __set_##seg(unsigned short val) { __asm { mov seg, val } }
#else  /* __GNUC__ || _MSC_VER */
# define __DEFINE_GET_SEG(seg) extern unsigned short __get_##seg(void);
# define __DEFINE_SET_SEG(seg) extern void __set_##seg(unsigned int);
#endif /* __GNUC__ || _MSC_VER */

#else  /* defined(__i386__) */

# define __DEFINE_GET_SEG(seg) inline static unsigned short __get_##seg(void) { return 0; }
# define __DEFINE_SET_SEG(seg) /* nothing */

#endif /* defined(__i386__) */

__DEFINE_GET_SEG(cs)
__DEFINE_GET_SEG(ds)
__DEFINE_GET_SEG(es)
__DEFINE_GET_SEG(fs)
__DEFINE_GET_SEG(gs)
__DEFINE_GET_SEG(ss)
__DEFINE_SET_SEG(fs)
__DEFINE_SET_SEG(gs)

#undef __DEFINE_GET_SEG
#undef __DEFINE_SET_SEG

#ifdef __i386__

#define _DEFINE_REGS_ENTRYPOINT( name, fn, args ) \
    __ASM_GLOBAL_FUNC( name, \
                       "call " __ASM_NAME("__wine_call_from_32_regs") "\n\t" \
                       ".long " __ASM_NAME(#fn) "\n\t" \
                       ".byte " #args ", " #args )
#define DEFINE_REGS_ENTRYPOINT_0( name, fn ) \
  extern void WINAPI name(void); \
  _DEFINE_REGS_ENTRYPOINT( name, fn, 0 )
#define DEFINE_REGS_ENTRYPOINT_1( name, fn, t1 ) \
  extern void WINAPI name( t1 a1 ); \
  _DEFINE_REGS_ENTRYPOINT( name, fn, 4 )
#define DEFINE_REGS_ENTRYPOINT_2( name, fn, t1, t2 ) \
  extern void WINAPI name( t1 a1, t2 a2 ); \
  _DEFINE_REGS_ENTRYPOINT( name, fn, 8 )
#define DEFINE_REGS_ENTRYPOINT_3( name, fn, t1, t2, t3 ) \
  extern void WINAPI name( t1 a1, t2 a2, t3 a3 ); \
  _DEFINE_REGS_ENTRYPOINT( name, fn, 12 )
#define DEFINE_REGS_ENTRYPOINT_4( name, fn, t1, t2, t3, t4 ) \
  extern void WINAPI name( t1 a1, t2 a2, t3 a3, t4 a4 ); \
  _DEFINE_REGS_ENTRYPOINT( name, fn, 16 )

#endif  /* __i386__ */

#ifdef __sparc__
/* FIXME: use getcontext() to retrieve full context */
#define _GET_CONTEXT \
    CONTEXT context;   \
    do { memset(&context, 0, sizeof(CONTEXT));            \
         context.ContextFlags = CONTEXT_CONTROL;          \
         context.pc = (DWORD)__builtin_return_address(0); \
       } while (0)

#define DEFINE_REGS_ENTRYPOINT_0( name, fn ) \
  void WINAPI name ( void ) \
  { _GET_CONTEXT; fn( &context ); }
#define DEFINE_REGS_ENTRYPOINT_1( name, fn, t1 ) \
  void WINAPI name ( t1 a1 ) \
  { _GET_CONTEXT; fn( a1, &context ); }
#define DEFINE_REGS_ENTRYPOINT_2( name, fn, t1, t2 ) \
  void WINAPI name ( t1 a1, t2 a2 ) \
  { _GET_CONTEXT; fn( a1, a2, &context ); }
#define DEFINE_REGS_ENTRYPOINT_3( name, fn, t1, t2, t3 ) \
  void WINAPI name ( t1 a1, t2 a2, t3 a3 ) \
  { _GET_CONTEXT; fn( a1, a2, a3, &context ); }
#define DEFINE_REGS_ENTRYPOINT_4( name, fn, t1, t2, t3, t4 ) \
  void WINAPI name ( t1 a1, t2 a2, t3 a3, t4 a4 ) \
  { _GET_CONTEXT; fn( a1, a2, a3, a4, &context ); }

#endif /* __sparc__ */

#ifdef __PPC__

/* those are i386 macros that don't make sense on a powerpc. However code using this is never 
 * exercised on the ppc so this is just appeasing the compiler, really */
#define AX_reg(context)      (*(WORD*)&(context)->Eax)
#define BX_reg(context)      (*(WORD*)&(context)->Ebx)
#define CX_reg(context)      (*(WORD*)&(context)->Ecx)
#define DX_reg(context)      (*(WORD*)&(context)->Edx)
#define SI_reg(context)      (*(WORD*)&(context)->Esi)
#define DI_reg(context)      (*(WORD*)&(context)->Edi)
#define BP_reg(context)      (*(WORD*)&(context)->Ebp)

#define AL_reg(context)      (*(BYTE*)&(context)->Eax)
#define AH_reg(context)      (*((BYTE*)&(context)->Eax + 1))
#define BL_reg(context)      (*(BYTE*)&(context)->Ebx)
#define BH_reg(context)      (*((BYTE*)&(context)->Ebx + 1))
#define CL_reg(context)      (*(BYTE*)&(context)->Ecx)
#define CH_reg(context)      (*((BYTE*)&(context)->Ecx + 1))
#define DL_reg(context)      (*(BYTE*)&(context)->Edx)
#define DH_reg(context)      (*((BYTE*)&(context)->Edx + 1))

#define SET_CFLAG(context)   ((context)->EFlags |= 0x0001)
#define RESET_CFLAG(context) ((context)->EFlags &= ~0x0001)
#define SET_ZFLAG(context)   ((context)->EFlags |= 0x0040)
#define RESET_ZFLAG(context) ((context)->EFlags &= ~0x0040)
#define ISV86(context)       ((context)->EFlags & 0x00020000)

/* FIXME: use getcontext() to retrieve full context */
#define _GET_CONTEXT \
    CONTEXT context;   \
    do { memset(&context, 0, sizeof(CONTEXT));            \
         context.ContextFlags = CONTEXT_CONTROL;          \
       } while (0)

#define DEFINE_REGS_ENTRYPOINT_0( name, fn ) \
  void WINAPI name ( void ) \
  { _GET_CONTEXT; fn( &context ); }
#define DEFINE_REGS_ENTRYPOINT_1( name, fn, t1 ) \
  void WINAPI name ( t1 a1 ) \
  { _GET_CONTEXT; fn( a1, &context ); }
#define DEFINE_REGS_ENTRYPOINT_2( name, fn, t1, t2 ) \
  void WINAPI name ( t1 a1, t2 a2 ) \
  { _GET_CONTEXT; fn( a1, a2, &context ); }
#define DEFINE_REGS_ENTRYPOINT_3( name, fn, t1, t2, t3 ) \
  void WINAPI name ( t1 a1, t2 a2, t3 a3 ) \
  { _GET_CONTEXT; fn( a1, a2, a3, &context ); }
#define DEFINE_REGS_ENTRYPOINT_4( name, fn, t1, t2, t3, t4 ) \
  void WINAPI name ( t1 a1, t2 a2, t3 a3, t4 a4 ) \
  { _GET_CONTEXT; fn( a1, a2, a3, a4, &context ); }

#endif /* __PPC__ */


#ifndef DEFINE_REGS_ENTRYPOINT_0
#error You need to define DEFINE_REGS_ENTRYPOINT macros for your CPU
#endif

#endif /* __HARDWARE_H */

