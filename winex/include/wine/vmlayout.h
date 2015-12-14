/* Virtual Memory Layout
 *
 * Copyright (c) 2011-2015 NVIDIA CORPORATION. All rights reserved.
 * 
 * WARNING: When changing values in this file, appropriate changes
 *          to mac.s must be made!
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#ifndef __WINE_VMLAYOUT_H
#define __WINE_VMLAYOUT_H

#include "config.h"

/*
 * System heap base
 */
#define SYSTEM_HEAP_BASE    ((void*)0x65430000)
#define SYSTEM_HEAP_SIZE    0x00100000   /* Default heap size = 1Mb */
#define SYSTEM_HEAP_RANGE   0x0aac0000

/*
 *  SHM server address
 */
#ifdef __INTEL_CE__
  #define SHMSERVER_ADDR    0x7d000000
#else
  #ifdef __APPLE__
    #define SHMSERVER_ADDR  0xF0000000
  #else
    #define SHMSERVER_ADDR  0x90000000
  #endif
#endif

#define SHMSERVER_SIZE      (1024 * 1024 * 32)

/*
 * Dos area
 */
#define DOSAREA_ADDR        0x00000000
#define DOSAREA_RANGE       0x00110000

/*
 * Shared heap
 */
#define SHAREDHEAP_RANGE    0x01000000
#ifdef __INTEL_CE__
  #define SHAREDHEAP_ADDR   0x60000000
#else
  #define SHAREDHEAP_ADDR   0x80000000
#endif

/*
 * PE EXE range
 * 
 * NOTE: if these values change the corresponding values must change in mac.s!
 */
#define PE_EXE_ADDR         0x00110000
#ifdef __INTEL_CE__
  #define PE_EXE_RANGE      0x2a99b000
#else
  #define PE_EXE_RANGE      0x6fef0000
#endif

/*
 * CRT range
 * 
 * The C and C++ runtimes request to be loaded above the 0x7c000000 range
 */
#define CRTS_ADDR           0x7c000000
#define CRTS_RANGE          0x01400000

/*
 * KUSERDATA_ADDR
 */ 
#define KUSERDATA_ADDR      0x7ffe0000
#define KUSERDATA_RANGE     0x00001000

#endif	/* __WINE_VMLAYOUT_H */
