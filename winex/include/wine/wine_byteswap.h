/* 

  Header for defining simple inline byteswap functions

*/
#ifndef __WINE_BYTESWAP_H__
#define __WINE_BYTESWAP_H__

static inline unsigned long _load_swap_32(void *base_addr, int index)
{
   register unsigned long val;
#if defined __ppc__
   __asm__ volatile ("lwbrx %0,%1,%2" : "=r" (val) : "b" (index), "r" (base_addr) : "memory");
#elif defined __i386__
   val = *(unsigned long*)((char *)base_addr + index);
#else
	#error "Define Byteswapping for this platform"
#endif
   return(val);
}

static inline unsigned long _load_swap_16(void *base_addr, int index)
{  
   register unsigned short val; 
#if defined __ppc__
   __asm__ volatile ("lhbrx %0,%1,%2" : "=r" (val) : "b" (index), "r" (base_addr) : "memory");
#elif defined __i386__
   val = *(unsigned short*)((char *)base_addr + index);
#else
	#error "Define Byteswapping for this platform"
#endif
   return(val);
}


/* This isn't done yet 
static inline unsigned long _store_swap_32(void *base_addr, int index)
{  
   register unsigned long val; 
#if defined __ppc__
   __asm__ ("lwbrx %0,%1,%2" : "=r" (val) : "b" (index), "r" (base_addr) : "memory");
#endif
   return(val);
}

static inline unsigned long _store_swap_16(void *base_addr, int index) 
{ 
   register unsigned short val;
#if defined __ppc__
   __asm__ ("lhbrx %0,%1,%2" : "=r" (val) : "b" (index), "r" (base_addr) : "memory");
#endif
   return(val);
}

*/

static inline void _swapinplace32(void *base)
{
#ifndef __i386__
    unsigned long x = _load_swap_32(base, 0);
    *(unsigned long *)base = x;
#endif
}

static inline void _swapinplace16(void *base)
{
#ifndef __i386__
    unsigned short x = _load_swap_16(base, 0);
    *(unsigned short *)base = x;
#endif
}

#endif /* __WINE_BYTESWAP_H__ */
