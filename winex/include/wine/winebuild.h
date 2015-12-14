#ifndef _WINEBUILD_H_
#define _WINEBUILD_H_


/* entry point flags */
#define FLAG_NOIMPORT  0x01  /* don't make function available for importing */
#define FLAG_NORELAY   0x02  /* don't use relay debugging for this function */
#define FLAG_RET64     0x04  /* function returns a 64-bit value */
#define FLAG_I386      0x08  /* function is i386 only */
#define FLAG_REGISTER  0x10  /* use register calling convention */
#define FLAG_INTERRUPT 0x20  /* function is an interrupt handler */
#define FLAG_HOOKABLE  0x40  /* function needs padding so it can be hooked */


#endif
