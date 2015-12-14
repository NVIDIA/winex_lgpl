/*
 * Copyright (c) 2006-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include <stdlib.h>
#include "config.h"
#include "wine/port.h"

#ifndef HAVE_MKSTEMPS

/*
 * Stub for mkstemps
 */
int
mkstemps (
     char *template,
     int suffix_len)
{
    return( mkstemp(template) );
}

#endif
