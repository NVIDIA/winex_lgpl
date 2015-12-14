/*
 * File memory.c - managing memory
 *
 * Copyright (C) 2004, Eric Pouech
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
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

#include "config.h"

#include <assert.h>
#include "dbghelp_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

/******************************************************************
 *		addr_to_linear
 *
 * converts an address into its linear value
 */
DWORD WINAPI addr_to_linear(HANDLE hProcess, HANDLE hThread, ADDRESS* addr)
{
    LDT_ENTRY	le;

    switch (addr->Mode)
    {
    case AddrMode1616:
        if (GetThreadSelectorEntry(hThread, addr->Segment, &le))
            return (le.HighWord.Bits.BaseHi << 24) + 
                (le.HighWord.Bits.BaseMid << 16) + le.BaseLow + LOWORD(addr->Offset);
        break;
    case AddrMode1632:
        if (GetThreadSelectorEntry(hThread, addr->Segment, &le))
            return (le.HighWord.Bits.BaseHi << 24) + 
                (le.HighWord.Bits.BaseMid << 16) + le.BaseLow + addr->Offset;
        break;
    case AddrModeReal:
        return (DWORD)(LOWORD(addr->Segment) << 4) + addr->Offset;
    case AddrModeFlat:
        return addr->Offset;
    default:
        FIXME("Unsupported (yet) mode (%x)\n", addr->Mode);
        return 0;
    }
    FIXME("Failed to linearize address %04x:%08x (mode %x)\n",
          addr->Segment, addr->Offset, addr->Mode);
    return 0;
}
