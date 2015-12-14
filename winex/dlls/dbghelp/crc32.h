/*
 *          CRC32 functions ('crc32.h')
 *
 *  Copyright (c) 2009-2015 NVIDIA CORPORATION. All rights reserved.
 *
 *
 *  provides CRC32 calculation functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#ifndef  __CRC32_H__
# define __CRC32_H__

# include <stddef.h>


typedef unsigned long crc32;

crc32 calc_crc32_fd(int fd);
crc32 calc_crc32_buf(const void *buf, size_t n);


#endif
