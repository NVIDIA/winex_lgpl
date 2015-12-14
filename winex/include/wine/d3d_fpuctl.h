/* d3d9stats.h
 *
 * Helper header to abstract away FPU control-word functionality.
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef WINE_D3D_FPUCTL_INCLUDED
#define WINE_D3D_FPUCTL_INCLUDED 

#include "config.h"

#if defined (HAVE_FPU_CONTROL)
 #include <fpu_control.h>
#else /* HAVE_FPU_CONTROL */
 #ifdef HAVE_SYS_TYPES_H
 # include <sys/types.h>
 #endif

/*
 * The hardware default control word for i387's and later coprocessors is
 * 0x37F, giving:
 *
 *      round to nearest
 *      64-bit precision
 *      all exceptions masked.
 *
 * We modify the affine mode bit and precision bits in this to give:
 *
 *      affine mode for 287's (if they work at all) (1 in bitfield 1<<12)
 *      53-bit precision (2 in bitfield 3<<8)
 *
 * 64-bit precision often gives bad results with high level languages
 * because it makes the results of calculations depend on whether
 * intermediate values are stored in memory or in FPU registers.
 */
#define __INITIAL_NPXCW__       0x127F
#define __INITIAL_MXCSR__       0x1F80

 typedef u_int16_t fpu_control_t;
 #define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
 #define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))
 #define _FPU_DEFAULT  __INITIAL_NPXCW__
 
#endif /* HAVE_FPU_CONTROL */


#endif /* WINE_D3D_FPUCTL_INCLUDED */

