/*
 * File msc.c - read VC++ debug information from COFF and eventually
 * from PDB files.
 *
 * Copyright (C) 1996,      Eric Youngdale.
 * Copyright (C) 1999-2000, Ulrich Weigand.
 * Copyright (C) 2004-2006, Eric Pouech.
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

/*
 * Note - this handles reading debug information for 32 bit applications
 * that run under Windows-NT for example.  I doubt that this would work well
 * for 16 bit applications, but I don't think it really matters since the
 * file format is different, and we should never get in here in such cases.
 *
 * TODO:
 *	Get 16 bit CV stuff working.
 *	Add symbol size to internal symbol table.
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winternl.h"

#include "wine/exception.h"
#include "wine/debug.h"
#include "dbghelp_private.h"
#include "wine/mscvpdb.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp_msc);

#define MAX_PATHNAME_LEN 1024



/*========================================================================
 * Debug file access helper routines
 */

static void dump(const void* ptr, unsigned len)
{
    unsigned int i, j;
    char        msg[128];
    const char* hexof = "0123456789abcdef";
    const BYTE* x = (const BYTE*)ptr;

    for (i = 0; i < len; i += 16)
    {
        sprintf(msg, "%08x: ", i);
        memset(msg + 10, ' ', 3 * 16 + 1 + 16);
        for (j = 0; j < min(16, len - i); j++)
        {
            msg[10 + 3 * j + 0] = hexof[x[i + j] >> 4];
            msg[10 + 3 * j + 1] = hexof[x[i + j] & 15];
            msg[10 + 3 * j + 2] = ' ';
            msg[10 + 3 * 16 + 1 + j] = (x[i + j] >= 0x20 && x[i + j] < 0x7f) ?
                x[i + j] : '.';
        }
        msg[10 + 3 * 16] = ' ';
        msg[10 + 3 * 16 + 1 + 16] = '\0';
        FIXME("%s\n", msg);
    }
}

/*========================================================================
 * Process CodeView type information.
 */

#define FIRST_DEFINABLE_TYPE    0x1000

typedef struct{
    struct symt *basic;         /* basic type (index < T_MAXBASICTYPE) */
#ifdef USE_ALL_TYPES
    struct symt *nearPtr;       /* near pointer to basic type (index & 0x0100) == 0x0100 */
    struct symt *farPtr;        /* far pointer to basic type (index & 0x0200) == 0x0200 */
    struct symt *hugePtr;       /* huge pointer to basic type (index & 0x0300) == 0x0300 */
    struct symt *near32Ptr;     /* 32-bit near pointer to basic type (index & 0x0400) == 0x0400 */
    struct symt *far32Ptr;      /* 48-bit far pointer to basic type (index & 0x0500) == 0x0500 */
    struct symt *near64Ptr;     /* 64-bit near pointer to basic type (index & 0x0600) == 0x0600 */
#else
    struct symt *near32Ptr;     /* 32-bit near pointer to basic type (index & 0x0400) == 0x0400 */
#endif
} CV_BasicTypes;


//static struct symt*     cv_basic_types[T_MAXPREDEFINEDTYPE];
static CV_BasicTypes cv_basic_types[T_MAXBASICTYPE] = {{0}};


struct cv_defined_module
{
    BOOL                allowed;
    unsigned int        num_defined_types;
    struct symt**       defined_types;
};
/* FIXME: don't make it static */
#define CV_MAX_MODULES          32
static struct cv_defined_module cv_zmodules[CV_MAX_MODULES];
static struct cv_defined_module*cv_current_module;

static void codeview_init_basic_types(struct module* module)
{
#ifdef USE_ALL_TYPES
    int i;
#endif

    /*
     * These are the common builtin types that are used by VC++.
     */

    /* non-pointer basic types */
    cv_basic_types[T_NOTYPE].basic    = NULL;
    cv_basic_types[T_ABS].basic       = NULL;
    cv_basic_types[T_SEGMENT].basic   = NULL;     /* fixme! */
    cv_basic_types[T_VOID].basic      = &symt_new_basic(module, btVoid,  "void", 0)->symt;
    cv_basic_types[T_CURRENCY].basic  = &symt_new_basic(module, btCurrency, "currency", 4)->symt; /* fixme!  This is a VB type... size and name are possibly incorrect */
    cv_basic_types[T_NBASICSTR].basic = NULL;     /* fixme!  VB type */
    cv_basic_types[T_FBASICSTR].basic = NULL;     /* fixme!  VB type */
    cv_basic_types[T_NOTTRANS].basic  = NULL;     /* error type */
    cv_basic_types[T_CHAR].basic      = &symt_new_basic(module, btChar,  "char", 1)->symt;
    cv_basic_types[T_SHORT].basic     = &symt_new_basic(module, btInt,   "short int", 2)->symt;
    cv_basic_types[T_LONG].basic      = &symt_new_basic(module, btInt,   "long int", 4)->symt;
    cv_basic_types[T_QUAD].basic      = &symt_new_basic(module, btInt,   "long long int", 8)->symt;
    cv_basic_types[T_UCHAR].basic     = &symt_new_basic(module, btUInt,  "unsigned char", 1)->symt;
    cv_basic_types[T_USHORT].basic    = &symt_new_basic(module, btUInt,  "unsigned short", 2)->symt;
    cv_basic_types[T_ULONG].basic     = &symt_new_basic(module, btUInt,  "unsigned long", 4)->symt;
    cv_basic_types[T_UQUAD].basic     = &symt_new_basic(module, btUInt,  "unsigned long long", 8)->symt;
    cv_basic_types[T_BOOL08].basic    = &symt_new_basic(module, btBool,  "bool", 1)->symt;
    cv_basic_types[T_BOOL16].basic    = &symt_new_basic(module, btBool,  "bool16", 2)->symt;
    cv_basic_types[T_BOOL32].basic    = &symt_new_basic(module, btBool,  "bool32", 4)->symt;
    cv_basic_types[T_BOOL64].basic    = &symt_new_basic(module, btBool,  "bool64", 8)->symt;
    cv_basic_types[T_REAL32].basic    = &symt_new_basic(module, btFloat, "float", 4)->symt;
    cv_basic_types[T_REAL64].basic    = &symt_new_basic(module, btFloat, "double", 8)->symt;
    cv_basic_types[T_REAL80].basic    = &symt_new_basic(module, btFloat, "long double", 10)->symt;
    cv_basic_types[T_REAL128].basic   = &symt_new_basic(module, btFloat, "long long double", 16)->symt;
    cv_basic_types[T_REAL48].basic    = &symt_new_basic(module, btFloat, "long float", 6)->symt;
    cv_basic_types[T_CPLX32].basic    = &symt_new_basic(module, btComplex, "complex", 4)->symt;       /* fixme!  This is a VB type... name is possibly incorrect */
    cv_basic_types[T_CPLX64].basic    = &symt_new_basic(module, btComplex, "complex64", 8)->symt;     /* fixme!  This is a VB type... name is possibly incorrect */
    cv_basic_types[T_CPLX80].basic    = &symt_new_basic(module, btComplex, "complex80", 10)->symt;    /* fixme!  This is a VB type... name is possibly incorrect */
    cv_basic_types[T_CPLX128].basic   = &symt_new_basic(module, btComplex, "complex128", 16)->symt;   /* fixme!  This is a VB type... name is possibly incorrect */
    cv_basic_types[T_BIT].basic       = &symt_new_basic(module, btBit,    "Bit", 1)->symt;    /* fixme!  This is a VB type... size and name are possibly incorrect */
    cv_basic_types[T_PASCHAR].basic   = NULL;     /* fixme! */
    cv_basic_types[T_RCHAR].basic     = &symt_new_basic(module, btInt,   "signed char", 1)->symt;
    cv_basic_types[T_WCHAR].basic     = &symt_new_basic(module, btWChar, "wchar_t", 2)->symt;
    cv_basic_types[T_INT2].basic      = &symt_new_basic(module, btInt,   "INT2", 2)->symt;
    cv_basic_types[T_UINT2].basic     = &symt_new_basic(module, btUInt,  "UINT2", 2)->symt;
    cv_basic_types[T_INT4].basic      = &symt_new_basic(module, btInt,   "INT4", 4)->symt;
    cv_basic_types[T_UINT4].basic     = &symt_new_basic(module, btUInt,  "UINT4", 4)->symt;
    cv_basic_types[T_INT8].basic      = &symt_new_basic(module, btInt,   "INT8", 8)->symt;
    cv_basic_types[T_UINT8].basic     = &symt_new_basic(module, btUInt,  "UINT8", 8)->symt;


    /* add 32-bit near pointers to basic types */
    cv_basic_types[T_VOID].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_VOID].basic)->symt;
    cv_basic_types[T_CHAR].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_CHAR].basic)->symt;
    cv_basic_types[T_SHORT].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_SHORT].basic)->symt;
    cv_basic_types[T_LONG].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_LONG].basic)->symt;
    cv_basic_types[T_QUAD].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_QUAD].basic)->symt;
    cv_basic_types[T_UCHAR].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_UCHAR].basic)->symt;
    cv_basic_types[T_USHORT].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_USHORT].basic)->symt;
    cv_basic_types[T_ULONG].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_ULONG].basic)->symt;
    cv_basic_types[T_UQUAD].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_UQUAD].basic)->symt;
    cv_basic_types[T_BOOL08].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_BOOL08].basic)->symt;
    cv_basic_types[T_BOOL16].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_BOOL16].basic)->symt;
    cv_basic_types[T_BOOL32].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_BOOL32].basic)->symt;
    cv_basic_types[T_BOOL64].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_BOOL64].basic)->symt;
    cv_basic_types[T_REAL32].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_REAL32].basic)->symt;
    cv_basic_types[T_REAL64].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_REAL64].basic)->symt;
    cv_basic_types[T_REAL80].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_REAL80].basic)->symt;
    cv_basic_types[T_REAL128].near32Ptr = &symt_new_pointer(module, cv_basic_types[T_REAL128].basic)->symt;
    cv_basic_types[T_REAL48].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_REAL48].basic)->symt;
    cv_basic_types[T_CPLX32].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_CPLX32].basic)->symt;
    cv_basic_types[T_CPLX64].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_CPLX64].basic)->symt;
    cv_basic_types[T_CPLX80].near32Ptr  = &symt_new_pointer(module, cv_basic_types[T_CPLX80].basic)->symt;
    cv_basic_types[T_CPLX128].near32Ptr = &symt_new_pointer(module, cv_basic_types[T_CPLX128].basic)->symt;
    cv_basic_types[T_RCHAR].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_RCHAR].basic)->symt;
    cv_basic_types[T_WCHAR].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_WCHAR].basic)->symt;
    cv_basic_types[T_INT2].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_INT2].basic)->symt;
    cv_basic_types[T_UINT2].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_UINT2].basic)->symt;
    cv_basic_types[T_INT4].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_INT4].basic)->symt;
    cv_basic_types[T_UINT4].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_UINT4].basic)->symt;
    cv_basic_types[T_INT8].near32Ptr    = &symt_new_pointer(module, cv_basic_types[T_INT8].basic)->symt;
    cv_basic_types[T_UINT8].near32Ptr   = &symt_new_pointer(module, cv_basic_types[T_UINT8].basic)->symt;

#ifdef USE_ALL_TYPES
    /* the representation of a pointer is independant of its size.  This means we can just reuse the same
       symt object for the near, far, huge, and far32 pointers of each type.  This isn't really necessary,
       it's just done for completeness. */
    for (i = 0; i < T_MAXBASICTYPE; i++){
        cv_basic_types[i].near64Ptr =   cv_basic_types[i].near32Ptr;
        cv_basic_types[i].far32Ptr =    cv_basic_types[i].near32Ptr;
        cv_basic_types[i].farPtr =      cv_basic_types[i].near32Ptr;
        cv_basic_types[i].nearPtr =     cv_basic_types[i].near32Ptr;
        cv_basic_types[i].hugePtr =     cv_basic_types[i].near32Ptr;
    }
#endif
}

static int numeric_leaf(int* value, const unsigned short int* leaf)
{
    unsigned short int type = *leaf++;
    int length = 2;

    if (type < LF_NUMERIC)
    {
        *value = type;
    }
    else
    {
        switch (type)
        {
        case LF_CHAR:
            length += 1;
            *value = *(const char*)leaf;
            break;

        case LF_SHORT:
            length += 2;
            *value = *(const short*)leaf;
            break;

        case LF_USHORT:
            length += 2;
            *value = *(const unsigned short*)leaf;
            break;

        case LF_LONG:
            length += 4;
            *value = *(const int*)leaf;
            break;

        case LF_ULONG:
            length += 4;
            *value = *(const unsigned int*)leaf;
            break;

        case LF_QUADWORD:
        case LF_UQUADWORD:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 8;
            *value = 0;    /* FIXME */
            break;

        case LF_REAL32:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 4;
            *value = 0;    /* FIXME */
            break;

        case LF_REAL48:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 6;
            *value = 0;    /* FIXME */
            break;

        case LF_REAL64:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 8;
            *value = 0;    /* FIXME */
            break;

        case LF_REAL80:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 10;
            *value = 0;    /* FIXME */
            break;

        case LF_REAL128:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 16;
            *value = 0;    /* FIXME */
            break;

        case LF_COMPLEX32:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 4;
            *value = 0;    /* FIXME */
            break;

        case LF_COMPLEX64:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 8;
            *value = 0;    /* FIXME */
            break;

        case LF_COMPLEX80:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 10;
            *value = 0;    /* FIXME */
            break;

        case LF_COMPLEX128:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 16;
            *value = 0;    /* FIXME */
            break;

        case LF_VARSTRING:
	    FIXME("Unsupported numeric leaf type %04x\n", type);
            length += 2 + *leaf;
            *value = 0;    /* FIXME */
            break;

        default:
	    FIXME("Unknown numeric leaf type %04x\n", type);
            *value = 0;
            break;
        }
    }

    return length;
}

/* convert a pascal string (as stored in debug information) into
 * a C string (null terminated).
 */
static const char* terminate_string(const struct p_string* p_name)
{
    static char symname[256];

    memcpy(symname, p_name->name, p_name->namelen);
    symname[p_name->namelen] = '\0';

    return (!*symname || strcmp(symname, "__unnamed") == 0) ? NULL : symname;
}

static struct symt*  codeview_get_type(unsigned int typeno, BOOL quiet)
{
    struct symt*        symt = NULL;

    /*
     * Convert Codeview type numbers into something we can grok internally.
     * Numbers < FIRST_DEFINABLE_TYPE are all fixed builtin types.
     * Numbers from FIRST_DEFINABLE_TYPE and up are all user defined (structs, etc).
     */
    if (typeno < FIRST_DEFINABLE_TYPE)
    {
        /* invalid type index => fail */
        if (typeno >= T_MAXPREDEFINEDTYPE)
            return NULL;

        /* type potentially points to a basic type => check if any of the pointer flags are set on the type */
        if ((typeno & T_BASICTYPE_MASK) < T_MAXBASICTYPE){
#ifdef USE_ALL_TYPES
            switch (typeno & T_MODE_MASK){
                case 0:                 symt = cv_basic_types[typeno & T_BASICTYPE_MASK].basic;     break;
                case T_NEARPTR_BITS:    symt = cv_basic_types[typeno & T_BASICTYPE_MASK].nearPtr;   break;
                case T_FARPTR_BITS:     symt = cv_basic_types[typeno & T_BASICTYPE_MASK].farPtr;    break;
                case T_HUGEPTR_BITS:    symt = cv_basic_types[typeno & T_BASICTYPE_MASK].hugePtr;   break;
                case T_NEAR32PTR_BITS:  symt = cv_basic_types[typeno & T_BASICTYPE_MASK].near32Ptr; break;
                case T_FAR32PTR_BITS:   symt = cv_basic_types[typeno & T_BASICTYPE_MASK].far32Ptr;  break;
                case T_NEAR64PTR_BITS:  symt = cv_basic_types[typeno & T_BASICTYPE_MASK].near64Ptr; break;
                default:
                    FIXME("unknown type mode: 0x%04x\n", typeno & T_MODE_MASK);
                    break;
            }
#else
            /* type is a pointer => return the pointer to the basic type */
            if (typeno & T_MODE_MASK)
                symt = cv_basic_types[typeno & T_BASICTYPE_MASK].near32Ptr;

            /* type is just a basic type */
            else
                symt = cv_basic_types[typeno & T_BASICTYPE_MASK].basic;
#endif
        }
    }
    else
    {
        unsigned        mod_index = typeno >> 24;
        unsigned        mod_typeno = typeno & 0x00FFFFFF;
        struct cv_defined_module*       mod;

        mod = (mod_index == 0) ? cv_current_module : &cv_zmodules[mod_index];

        if (mod_index >= CV_MAX_MODULES || !mod->allowed) 
            FIXME("Module of index %d isn't loaded yet (%x)\n", mod_index, typeno);
        else
        {
            if (mod_typeno - FIRST_DEFINABLE_TYPE < mod->num_defined_types)
                symt = mod->defined_types[mod_typeno - FIRST_DEFINABLE_TYPE];
        }
    }
    if (!quiet && !symt && typeno) FIXME("Returning NULL symt for type-id %x\n", typeno);
    return symt;
}

struct codeview_type_parse
{
    struct module*      module;
    const BYTE*         table;
    const DWORD*        offset;
    DWORD               first;
    DWORD               num;
};

static inline const void* codeview_jump_to_type(const struct codeview_type_parse* ctp, DWORD idx)
{
    if (idx < ctp->first){   /*FIRST_DEFINABLE_TYPE)*/
        WARN("type 0x%08x is potentially a built-in type.  Returning NULL\n", idx);

        return NULL;
    }


    idx -= ctp->first;  /*FIRST_DEFINABLE_TYPE;*/

    if (idx >= ctp->num){
        ERR("out of range type number {idx = 0x%08x, first = 0x%08x, num = 0x%08x}\n", idx, ctp->first, ctp->num);

        return NULL;
    }

    return ctp->table + ctp->offset[idx]; 
}

static int codeview_add_type(unsigned int typeno, struct symt* dt)
{
    if (typeno < FIRST_DEFINABLE_TYPE)
        FIXME("What the heck\n");
    if (!cv_current_module)
    {
        FIXME("Adding %x to non allowed module\n", typeno);
        return FALSE;
    }
    if ((typeno >> 24) != 0)
        FIXME("No module index while inserting type-id assumption is wrong %x\n",
              typeno);
    while (typeno - FIRST_DEFINABLE_TYPE >= cv_current_module->num_defined_types)
    {
        cv_current_module->num_defined_types += 0x100;
        if (cv_current_module->defined_types)
            cv_current_module->defined_types = HeapReAlloc(GetProcessHeap(),
                            HEAP_ZERO_MEMORY, cv_current_module->defined_types,
                            cv_current_module->num_defined_types * sizeof(struct symt*));
        else
            cv_current_module->defined_types = HeapAlloc(GetProcessHeap(),
                            HEAP_ZERO_MEMORY,
                            cv_current_module->num_defined_types * sizeof(struct symt*));

        if (cv_current_module->defined_types == NULL) return FALSE;
    }
    if (cv_current_module->defined_types[typeno - FIRST_DEFINABLE_TYPE])
    {
        if (cv_current_module->defined_types[typeno - FIRST_DEFINABLE_TYPE] != dt)
            FIXME("Overwritting at %x\n", typeno);
    }
    cv_current_module->defined_types[typeno - FIRST_DEFINABLE_TYPE] = dt;
    return TRUE;
}

static void codeview_clear_type_table(void)
{
    int i;

    for (i = 0; i < CV_MAX_MODULES; i++)
    {
        if (cv_zmodules[i].allowed)
            HeapFree(GetProcessHeap(), 0, cv_zmodules[i].defined_types);
        cv_zmodules[i].allowed = FALSE;
        cv_zmodules[i].defined_types = NULL;
        cv_zmodules[i].num_defined_types = 0;
    }
    cv_current_module = NULL;
}

static struct symt* codeview_parse_one_type(struct codeview_type_parse* ctp,
                                            unsigned curr_type,
                                            const union codeview_type* type, BOOL details);

static void* codeview_cast_symt(struct symt* symt, enum SymTagEnum tag)
{
    if (symt->tag != tag)
    {
        FIXME("Bad tag. Expected %d, but got %d\n", tag, symt->tag);
        return NULL;
    }   
    return symt;
}

static struct symt* codeview_fetch_type(struct codeview_type_parse* ctp,
                                        unsigned typeno)
{
    struct symt*                symt;
    const union codeview_type*  p;

    if (!typeno) return NULL;
    if ((symt = codeview_get_type(typeno, TRUE))) return symt;

    /* forward declaration */
    if (!(p = codeview_jump_to_type(ctp, typeno)))
    {
        FIXME("Cannot locate type %x\n", typeno);
        return NULL;
    }
    symt = codeview_parse_one_type(ctp, typeno, p, FALSE);
    if (!symt)
        FIXME("Couldn't load forward type %x\n", typeno);

    return symt;
}

static struct symt* codeview_add_type_pointer(struct codeview_type_parse* ctp,
                                              struct symt* existing,
                                              unsigned int pointee_type)
{
    struct symt* pointee;

    if (existing)
    {
        existing = codeview_cast_symt(existing, SymTagPointerType);
        return existing;
    }
    pointee = codeview_fetch_type(ctp, pointee_type);
    return &symt_new_pointer(ctp->module, pointee)->symt;
}

static struct symt* codeview_add_type_array(struct codeview_type_parse* ctp, 
                                            const char* name,
                                            unsigned int elemtype,
                                            unsigned int indextype,
                                            unsigned int arr_len)
{
    struct symt*        elem = codeview_fetch_type(ctp, elemtype);
    struct symt*        index = codeview_fetch_type(ctp, indextype);
    DWORD               arr_max = 0;

    if (elem)
    {
        DWORD64 elem_size;
        symt_get_info(elem, TI_GET_LENGTH, &elem_size);
        if (elem_size) arr_max = arr_len / (DWORD)elem_size;
    }
    return &symt_new_array(ctp->module, 0, arr_max, elem, index)->symt;
}

static int codeview_add_type_enum_field_list(struct module* module,
                                             struct symt_enum* symt,
                                             const union codeview_reftype* ref_type)
{
    const unsigned char*                ptr = ref_type->fieldlist.list;
    const unsigned char*                last = (const BYTE*)ref_type + ref_type->generic.len + 2;
    const union codeview_fieldtype*     type;

    while (ptr < last)
    {
        if (*ptr >= 0xf0)       /* LF_PAD... */
        {
            ptr += *ptr & 0x0f;
            continue;
        }

        type = (const union codeview_fieldtype*)ptr;

        switch (type->generic.id)
        {
        case LF_ENUMERATE_V1:
        {
            int value, vlen = numeric_leaf(&value, &type->enumerate_v1.value);
            const struct p_string* p_name = (const struct p_string*)((const unsigned char*)&type->enumerate_v1.value + vlen);

            symt_add_enum_element(module, symt, terminate_string(p_name), value);
            ptr += 2 + 2 + vlen + (1 + p_name->namelen);
            break;
        }
        case LF_ENUMERATE_V3:
        {
            int value, vlen = numeric_leaf(&value, &type->enumerate_v3.value);
            const char* name = (const char*)&type->enumerate_v3.value + vlen;

            symt_add_enum_element(module, symt, name, value);
            ptr += 2 + 2 + vlen + (1 + strlen(name));
            break;
        }

        default:
            FIXME("Unsupported type %04x in ENUM field list\n", type->generic.id);
            return FALSE;
        }
    }
    return TRUE;
}

static void codeview_add_udt_element(struct codeview_type_parse* ctp,
                                     struct symt_udt* symt, const char* name,
                                     int value, unsigned type)
{
    struct symt*                subtype;
    const union codeview_reftype*cv_type;

    if ((cv_type = codeview_jump_to_type(ctp, type)))
    {
        switch (cv_type->generic.id)
        {
        case LF_BITFIELD_V1:
            symt_add_udt_element(ctp->module, symt, name,
                                 codeview_fetch_type(ctp, cv_type->bitfield_v1.type),
                                 cv_type->bitfield_v1.bitoff,
                                 cv_type->bitfield_v1.nbits);
            return;
        case LF_BITFIELD_V2:
            symt_add_udt_element(ctp->module, symt, name,
                                 codeview_fetch_type(ctp, cv_type->bitfield_v2.type),
                                 cv_type->bitfield_v2.bitoff,
                                 cv_type->bitfield_v2.nbits);
            return;
        }
    }
    subtype = codeview_fetch_type(ctp, type);

    if (subtype)
    {
        DWORD64 elem_size = 0;
        symt_get_info(subtype, TI_GET_LENGTH, &elem_size);
        symt_add_udt_element(ctp->module, symt, name, subtype,
                             value << 3, (DWORD)elem_size << 3);
    }
}

static int codeview_add_type_struct_field_list(struct codeview_type_parse* ctp,
                                               struct symt_udt* symt,
                                               unsigned fieldlistno)
{
    const unsigned char*        ptr;
    const unsigned char*        last;
    int                         value, leaf_len;
    const struct p_string*      p_name;
    const char*                 c_name;
    const union codeview_reftype*type_ref;
    const union codeview_fieldtype* type;

    if (!fieldlistno) return TRUE;
    type_ref = codeview_jump_to_type(ctp, fieldlistno);
    ptr = type_ref->fieldlist.list;
    last = (const BYTE*)type_ref + type_ref->generic.len + 2;

    while (ptr < last)
    {
        if (*ptr >= 0xf0)       /* LF_PAD... */
        {
            ptr += *ptr & 0x0f;
            continue;
        }

        type = (const union codeview_fieldtype*)ptr;

        switch (type->generic.id)
        {
        case LF_BCLASS_V1:
            leaf_len = numeric_leaf(&value, &type->bclass_v1.offset);

            /* FIXME: ignored for now */

            ptr += 2 + 2 + 2 + leaf_len;
            break;

        case LF_BCLASS_V2:
            leaf_len = numeric_leaf(&value, &type->bclass_v2.offset);

            /* FIXME: ignored for now */

            ptr += 2 + 2 + 4 + leaf_len;
            break;

        case LF_VBCLASS_V1:
        case LF_IVBCLASS_V1:
            {
                const unsigned short int* p_vboff;
                int vpoff, vplen;
                leaf_len = numeric_leaf(&value, &type->vbclass_v1.vbpoff);
                p_vboff = (const unsigned short int*)((const char*)&type->vbclass_v1.vbpoff + leaf_len);
                vplen = numeric_leaf(&vpoff, p_vboff);

                /* FIXME: ignored for now */

                ptr += 2 + 2 + 2 + 2 + leaf_len + vplen;
            }
            break;

        case LF_VBCLASS_V2:
        case LF_IVBCLASS_V2:
            {
                const unsigned short int* p_vboff;
                int vpoff, vplen;
                leaf_len = numeric_leaf(&value, &type->vbclass_v2.vbpoff);
                p_vboff = (const unsigned short int*)((const char*)&type->vbclass_v2.vbpoff + leaf_len);
                vplen = numeric_leaf(&vpoff, p_vboff);

                /* FIXME: ignored for now */

                ptr += 2 + 2 + 4 + 4 + leaf_len + vplen;
            }
            break;

        case LF_MEMBER_V1:
            leaf_len = numeric_leaf(&value, &type->member_v1.offset);
            p_name = (const struct p_string*)((const char*)&type->member_v1.offset + leaf_len);

            codeview_add_udt_element(ctp, symt, terminate_string(p_name), value, 
                                     type->member_v1.type);

            ptr += 2 + 2 + 2 + leaf_len + (1 + p_name->namelen);
            break;

        case LF_MEMBER_V2:
            leaf_len = numeric_leaf(&value, &type->member_v2.offset);
            p_name = (const struct p_string*)((const unsigned char*)&type->member_v2.offset + leaf_len);

            codeview_add_udt_element(ctp, symt, terminate_string(p_name), value, 
                                     type->member_v2.type);

            ptr += 2 + 2 + 4 + leaf_len + (1 + p_name->namelen);
            break;

        case LF_MEMBER_V3:
            leaf_len = numeric_leaf(&value, &type->member_v3.offset);
            c_name = (const char*)&type->member_v3.offset + leaf_len;

            codeview_add_udt_element(ctp, symt, c_name, value, type->member_v3.type);

            ptr += 2 + 2 + 4 + leaf_len + (strlen(c_name) + 1);
            break;

        case LF_STMEMBER_V1:
            /* FIXME: ignored for now */
            ptr += 2 + 2 + 2 + (1 + type->stmember_v1.p_name.namelen);
            break;

        case LF_STMEMBER_V2:
            /* FIXME: ignored for now */
            ptr += 2 + 4 + 2 + (1 + type->stmember_v2.p_name.namelen);
            break;

        case LF_METHOD_V1:
            /* FIXME: ignored for now */
            ptr += 2 + 2 + 2 + (1 + type->method_v1.p_name.namelen);
            break;

        case LF_METHOD_V2:
            /* FIXME: ignored for now */
            ptr += 2 + 2 + 4 + (1 + type->method_v2.p_name.namelen);
            break;

        case LF_NESTTYPE_V1:
            /* FIXME: ignored for now */
            ptr += 2 + 2 + (1 + type->nesttype_v1.p_name.namelen);
            break;

        case LF_NESTTYPE_V2:
            /* FIXME: ignored for now */
            ptr += 2 + 2 + 4 + (1 + type->nesttype_v2.p_name.namelen);
            break;

        case LF_VFUNCTAB_V1:
            /* FIXME: ignored for now */
            ptr += 2 + 2;
            break;

        case LF_VFUNCTAB_V2:
            /* FIXME: ignored for now */
            ptr += 2 + 2 + 4;
            break;

        case LF_ONEMETHOD_V1:
            /* FIXME: ignored for now */
            switch ((type->onemethod_v1.attribute >> 2) & 7)
            {
            case 4: case 6: /* (pure) introducing virtual method */
                ptr += 2 + 2 + 2 + 4 + (1 + type->onemethod_virt_v1.p_name.namelen);
                break;

            default:
                ptr += 2 + 2 + 2 + (1 + type->onemethod_v1.p_name.namelen);
                break;
            }
            break;

        case LF_ONEMETHOD_V2:
            /* FIXME: ignored for now */
            switch ((type->onemethod_v2.attribute >> 2) & 7)
            {
            case 4: case 6: /* (pure) introducing virtual method */
                ptr += 2 + 2 + 4 + 4 + (1 + type->onemethod_virt_v2.p_name.namelen);
                break;

            default:
                ptr += 2 + 2 + 4 + (1 + type->onemethod_v2.p_name.namelen);
                break;
            }
            break;

        default:
            FIXME("Unsupported type %04x in STRUCT field list\n", type->generic.id);
            return FALSE;
        }
    }

    return TRUE;
}

static struct symt* codeview_add_type_enum(struct codeview_type_parse* ctp,
                                           struct symt* existing,
                                           const char* name,
                                           unsigned fieldlistno)
{
    struct symt_enum*   symt;

    if (existing)
    {
        if (!(symt = codeview_cast_symt(existing, SymTagEnum))) return NULL;
        /* should also check that all fields are the same */
    }
    else
    {
        symt = symt_new_enum(ctp->module, name);
        if (fieldlistno)
        {
            const union codeview_reftype* fieldlist;
            fieldlist = codeview_jump_to_type(ctp, fieldlistno);
            codeview_add_type_enum_field_list(ctp->module, symt, fieldlist);
        }
    }
    return &symt->symt;
}

static struct symt* codeview_add_type_struct(struct codeview_type_parse* ctp,
                                             struct symt* existing,
                                             const char* name, int structlen, 
                                             enum UdtKind kind)
{
    struct symt_udt*    symt;

    if (existing)
    {
        if (!(symt = codeview_cast_symt(existing, SymTagUDT))) return NULL;
        /* should also check that all fields are the same */
    }
    else symt = symt_new_udt(ctp->module, name, structlen, kind);

    return &symt->symt;
}

static struct symt* codeview_new_func_signature(struct codeview_type_parse* ctp, 
                                                struct symt* existing,
                                                enum CV_call_e call_conv)
{
    struct symt_function_signature*     sym;

    if (existing)
    {
        sym = codeview_cast_symt(existing, SymTagFunctionType);
        if (!sym) return NULL;
    }
    else
    {
        sym = symt_new_function_signature(ctp->module, NULL, call_conv);
    }
    return &sym->symt;
}

static void codeview_add_func_signature_args(struct codeview_type_parse* ctp,
                                             struct symt_function_signature* sym,
                                             unsigned ret_type,
                                             unsigned args_list)
{
    const union codeview_reftype*       reftype;

    sym->rettype = codeview_fetch_type(ctp, ret_type);
    if (args_list && (reftype = codeview_jump_to_type(ctp, args_list)))
    {
        unsigned int i;
        switch (reftype->generic.id)
        {
        case LF_ARGLIST_V1:
            for (i = 0; i < reftype->arglist_v1.num; i++)
                symt_add_function_signature_parameter(ctp->module, sym,
                                                      codeview_fetch_type(ctp, reftype->arglist_v1.args[i]));
            break;
        case LF_ARGLIST_V2:
            for (i = 0; i < reftype->arglist_v2.num; i++)
                symt_add_function_signature_parameter(ctp->module, sym,
                                                      codeview_fetch_type(ctp, reftype->arglist_v2.args[i]));
            break;
        default:
            FIXME("Unexpected leaf %x for signature's pmt\n", reftype->generic.id);
        }
    }
}

static struct symt* codeview_parse_one_type(struct codeview_type_parse* ctp,
                                            unsigned curr_type,
                                            const union codeview_type* type, BOOL details)
{
    struct symt*                symt;
    int                         value, leaf_len;
    const struct p_string*      p_name;
    const char*                 c_name;
    struct symt*                existing;

    existing = codeview_get_type(curr_type, TRUE);

    switch (type->generic.id)
    {
    case LF_MODIFIER_V1:
        /* FIXME: we don't handle modifiers, 
         * but readd previous type on the curr_type 
         */
        WARN("Modifier on 0x%04x: %s%s%s%s\n",
             type->modifier_v1.type,
             type->modifier_v1.attribute & 0x01 ? "const " : "",
             type->modifier_v1.attribute & 0x02 ? "volatile " : "",
             type->modifier_v1.attribute & 0x04 ? "unaligned " : "",
             type->modifier_v1.attribute & ~0x07 ? "unknown " : "");
        if (!(symt = codeview_get_type(type->modifier_v1.type, TRUE)))
            symt = codeview_parse_one_type(ctp, type->modifier_v1.type,
                                           codeview_jump_to_type(ctp, type->modifier_v1.type), details);
        break;
    case LF_MODIFIER_V2:
        /* FIXME: we don't handle modifiers, but readd previous type on the curr_type */
        WARN("Modifier on 0x%04x: %s%s%s%s\n",
             type->modifier_v2.type,
             type->modifier_v2.attribute & 0x01 ? "const " : "",
             type->modifier_v2.attribute & 0x02 ? "volatile " : "",
             type->modifier_v2.attribute & 0x04 ? "unaligned " : "",
             type->modifier_v2.attribute & ~0x07 ? "unknown " : "");
        if (!(symt = codeview_get_type(type->modifier_v2.type, TRUE)))
            symt = codeview_parse_one_type(ctp, type->modifier_v2.type,
                                           codeview_jump_to_type(ctp, type->modifier_v2.type), details);
        break;

    case LF_POINTER_V1:
        symt = codeview_add_type_pointer(ctp, existing, type->pointer_v1.datatype);
        break;
    case LF_POINTER_V2:
        symt = codeview_add_type_pointer(ctp, existing, type->pointer_v2.datatype);
        break;

    case LF_ARRAY_V1:
        if (existing) symt = codeview_cast_symt(existing, SymTagArrayType);
        else
        {
            leaf_len = numeric_leaf(&value, &type->array_v1.arrlen);
            p_name = (const struct p_string*)((const unsigned char*)&type->array_v1.arrlen + leaf_len);
            symt = codeview_add_type_array(ctp, terminate_string(p_name),
                                           type->array_v1.elemtype,
                                           type->array_v1.idxtype, value);
        }
        break;
    case LF_ARRAY_V2:
        if (existing) symt = codeview_cast_symt(existing, SymTagArrayType);
        else
        {
            leaf_len = numeric_leaf(&value, &type->array_v2.arrlen);
            p_name = (const struct p_string*)((const unsigned char*)&type->array_v2.arrlen + leaf_len);

            symt = codeview_add_type_array(ctp, terminate_string(p_name),
                                           type->array_v2.elemtype,
                                           type->array_v2.idxtype, value);
        }
        break;
    case LF_ARRAY_V3:
        if (existing) symt = codeview_cast_symt(existing, SymTagArrayType);
        else
        {
            leaf_len = numeric_leaf(&value, &type->array_v3.arrlen);
            c_name = (const char*)&type->array_v3.arrlen + leaf_len;

            symt = codeview_add_type_array(ctp, c_name,
                                           type->array_v3.elemtype,
                                           type->array_v3.idxtype, value);
        }
        break;

    case LF_STRUCTURE_V1:
    case LF_CLASS_V1:
        leaf_len = numeric_leaf(&value, &type->struct_v1.structlen);
        p_name = (const struct p_string*)((const unsigned char*)&type->struct_v1.structlen + leaf_len);
        symt = codeview_add_type_struct(ctp, existing, terminate_string(p_name), value,
                                        type->generic.id == LF_CLASS_V1 ? UdtClass : UdtStruct);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_type_struct_field_list(ctp, (struct symt_udt*)symt, 
                                                type->struct_v1.fieldlist);
        }
        break;

    case LF_STRUCTURE_V2:
    case LF_CLASS_V2:
        leaf_len = numeric_leaf(&value, &type->struct_v2.structlen);
        p_name = (const struct p_string*)((const unsigned char*)&type->struct_v2.structlen + leaf_len);
        symt = codeview_add_type_struct(ctp, existing, terminate_string(p_name), value,
                                        type->generic.id == LF_CLASS_V2 ? UdtClass : UdtStruct);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_type_struct_field_list(ctp, (struct symt_udt*)symt,
                                                type->struct_v2.fieldlist);
        }
        break;

    case LF_STRUCTURE_V3:
    case LF_CLASS_V3:
        leaf_len = numeric_leaf(&value, &type->struct_v3.structlen);
        c_name = (const char*)&type->struct_v3.structlen + leaf_len;
        symt = codeview_add_type_struct(ctp, existing, c_name, value,
                                        type->generic.id == LF_CLASS_V3 ? UdtClass : UdtStruct);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_type_struct_field_list(ctp, (struct symt_udt*)symt,
                                                type->struct_v3.fieldlist);
        }
        break;

    case LF_UNION_V1:
        leaf_len = numeric_leaf(&value, &type->union_v1.un_len);
        p_name = (const struct p_string*)((const unsigned char*)&type->union_v1.un_len + leaf_len);
        symt = codeview_add_type_struct(ctp, existing, terminate_string(p_name),
                                        value, UdtUnion);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_type_struct_field_list(ctp, (struct symt_udt*)symt,
                                                type->union_v1.fieldlist);
        }
        break;

    case LF_UNION_V2:
        leaf_len = numeric_leaf(&value, &type->union_v2.un_len);
        p_name = (const struct p_string*)((const unsigned char*)&type->union_v2.un_len + leaf_len);
        symt = codeview_add_type_struct(ctp, existing, terminate_string(p_name),
                                        value, UdtUnion);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_type_struct_field_list(ctp, (struct symt_udt*)symt,
                                                type->union_v2.fieldlist);
        }
        break;

    case LF_UNION_V3:
        leaf_len = numeric_leaf(&value, &type->union_v3.un_len);
        c_name = (const char*)&type->union_v3.un_len + leaf_len;
        symt = codeview_add_type_struct(ctp, existing, c_name,
                                        value, UdtUnion);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_type_struct_field_list(ctp, (struct symt_udt*)symt,
                                                type->union_v3.fieldlist);
        }
        break;

    case LF_ENUM_V1:
        symt = codeview_add_type_enum(ctp, existing,
                                      terminate_string(&type->enumeration_v1.p_name),
                                      type->enumeration_v1.fieldlist);
        break;

    case LF_ENUM_V2:
        symt = codeview_add_type_enum(ctp, existing,
                                      terminate_string(&type->enumeration_v2.p_name),
                                      type->enumeration_v2.fieldlist);
        break;

    case LF_ENUM_V3:
        symt = codeview_add_type_enum(ctp, existing, type->enumeration_v3.name,
                                      type->enumeration_v3.fieldlist);
        break;

    case LF_PROCEDURE_V1:
        symt = codeview_new_func_signature(ctp, existing, type->procedure_v1.call);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_func_signature_args(ctp,
                                             (struct symt_function_signature*)symt,
                                             type->procedure_v1.rvtype,
                                             type->procedure_v1.arglist);
        }
        break;
    case LF_PROCEDURE_V2:
        symt = codeview_new_func_signature(ctp, existing,type->procedure_v2.call);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_func_signature_args(ctp,
                                             (struct symt_function_signature*)symt,
                                             type->procedure_v2.rvtype,
                                             type->procedure_v2.arglist);
        }
        break;

    case LF_MFUNCTION_V1:
        /* FIXME: for C++, this is plain wrong, but as we don't use arg types
         * nor class information, this would just do for now
         */
        symt = codeview_new_func_signature(ctp, existing, type->mfunction_v1.call);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_func_signature_args(ctp,
                                             (struct symt_function_signature*)symt,
                                             type->mfunction_v1.rvtype,
                                             type->mfunction_v1.arglist);
        }
        break;
    case LF_MFUNCTION_V2:
        /* FIXME: for C++, this is plain wrong, but as we don't use arg types
         * nor class information, this would just do for now
         */
        symt = codeview_new_func_signature(ctp, existing, type->mfunction_v2.call);
        if (details)
        {
            codeview_add_type(curr_type, symt);
            codeview_add_func_signature_args(ctp,
                                             (struct symt_function_signature*)symt,
                                             type->mfunction_v2.rvtype,
                                             type->mfunction_v2.arglist);
        }
        break;

    case LF_VTSHAPE_V1:
        /* this is an ugly hack... FIXME when we have C++ support */
        if (!(symt = existing))
        {
            char    buf[128];
            snprintf(buf, sizeof(buf), "__internal_vt_shape_%x\n", curr_type);
            symt = &symt_new_udt(ctp->module, buf, 0, UdtStruct)->symt;
        }
        break;
    default:
        FIXME("Unsupported type-id leaf %x\n", type->generic.id);
        dump(type, 2 + type->generic.len);
        return FALSE;
    }
    return codeview_add_type(curr_type, symt) ? symt : NULL;
}

static int codeview_parse_type_table(struct codeview_type_parse* ctp)
{
    unsigned int                curr_type;
    const union codeview_type*  type;

    for (curr_type = ctp->first; curr_type < ctp->first + ctp->num; curr_type++)
    {
        type = codeview_jump_to_type(ctp, curr_type);

        /* type records we're interested in are the ones referenced by symbols
         * The known ranges are (X mark the ones we want):
         *   X  0000-0016       for V1 types
         *      0200-020c       for V1 types referenced by other types
         *      0400-040f       for V1 types (complex lists & sets)
         *   X  1000-100f       for V2 types
         *      1200-120c       for V2 types referenced by other types
         *      1400-140f       for V1 types (complex lists & sets)
         *   X  1500-150d       for V3 types
         *      8000-8010       for numeric leafes
         */
        if (type->generic.id & 0x8600) continue;
        codeview_parse_one_type(ctp, curr_type, type, TRUE);
    }

    return TRUE;
}

/*========================================================================
 * Process CodeView line number information.
 */

static struct codeview_linetab* codeview_snarf_linetab(struct module* module, 
                                                       const BYTE* linetab, int size,
                                                       BOOL pascal_str)
{
    int				file_segcount;
    char			filename[PATH_MAX];
    const unsigned int*         filetab;
    const struct p_string*      p_fn;
    int				i;
    int				k;
    struct codeview_linetab*    lt_hdr;
    const unsigned int*         lt_ptr;
    int				nfile;
    int				nseg;
    union any_size		pnt;
    union any_size		pnt2;
    const struct startend*      start;
    int				this_seg;
    unsigned                    source;

    /*
     * Now get the important bits.
     */
    pnt.uc = linetab;
    nfile = *pnt.s++;
    nseg = *pnt.s++;

    filetab = (const unsigned int*) pnt.c;

    /*
     * Now count up the number of segments in the file.
     */
    nseg = 0;
    for (i = 0; i < nfile; i++)
    {
        pnt2.uc = linetab + filetab[i];
        nseg += *pnt2.s;
    }

    /*
     * Next allocate the header we will be returning.
     * There is one header for each segment, so that we can reach in
     * and pull bits as required.
     */
    lt_hdr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                (nseg + 1) * sizeof(*lt_hdr));
    if (lt_hdr == NULL)
    {
        goto leave;
    }

    /*
     * Now fill the header we will be returning, one for each segment.
     * Note that this will basically just contain pointers into the existing
     * line table, and we do not actually copy any additional information
     * or allocate any additional memory.
     */

    this_seg = 0;
    for (i = 0; i < nfile; i++)
    {
        /*
         * Get the pointer into the segment information.
         */
        pnt2.uc = linetab + filetab[i];
        file_segcount = *pnt2.s;

        pnt2.ui++;
        lt_ptr = (const unsigned int*) pnt2.c;
        start = (const struct startend*)(lt_ptr + file_segcount);

        /*
         * Now snarf the filename for all of the segments for this file.
         */
        if (pascal_str)
        {
            p_fn = (const struct p_string*)(start + file_segcount);
            memset(filename, 0, sizeof(filename));
            memcpy(filename, p_fn->name, p_fn->namelen);
            source = source_new(module, NULL, filename);
        }
        else
            source = source_new(module, NULL, (const char*)(start + file_segcount));
        
        for (k = 0; k < file_segcount; k++, this_seg++)
        {
            pnt2.uc = linetab + lt_ptr[k];
            lt_hdr[this_seg].start      = start[k].start;
            lt_hdr[this_seg].end        = start[k].end;
            lt_hdr[this_seg].source     = source;
            lt_hdr[this_seg].segno      = *pnt2.s++;
            lt_hdr[this_seg].nline      = *pnt2.s++;
            lt_hdr[this_seg].offtab     = pnt2.ui;
            lt_hdr[this_seg].linetab    = (const unsigned short*)(pnt2.ui + lt_hdr[this_seg].nline);
        }
    }

leave:

  return lt_hdr;

}

/*========================================================================
 * Process CodeView symbol information.
 */

static unsigned int codeview_map_offset(const struct msc_debug_info* msc_dbg,
                                        unsigned int offset)
{
    int                 nomap = msc_dbg->nomap;
    const OMAP_DATA*    omapp = msc_dbg->omapp;
    int                 i;

    if (!nomap || !omapp) return offset;

    /* FIXME: use binary search */
    for (i = 0; i < nomap - 1; i++)
        if (omapp[i].from <= offset && omapp[i+1].from > offset)
            return !omapp[i].to ? 0 : omapp[i].to + (offset - omapp[i].from);

    return 0;
}

static const struct codeview_linetab*
codeview_get_linetab(const struct codeview_linetab* linetab,
                     unsigned seg, unsigned offset)
{
    /*
     * Check whether we have line number information
     */
    if (linetab)
    {
        for (; linetab->linetab; linetab++)
            if (linetab->segno == seg &&
                linetab->start <= offset && linetab->end   >  offset)
                break;
        if (!linetab->linetab) linetab = NULL;
    }
    return linetab;
}

static unsigned codeview_get_address(const struct msc_debug_info* msc_dbg, 
                                     unsigned seg, unsigned offset)
{
    int			        nsect = msc_dbg->nsect;
    const IMAGE_SECTION_HEADER* sectp = msc_dbg->sectp;

    if (!seg || seg > (unsigned)nsect) return 0;
    return (unsigned int)(msc_dbg->module->module.BaseOfImage +
        codeview_map_offset(msc_dbg, sectp[seg-1].VirtualAddress + offset));
}

static void codeview_add_func_linenum(struct module* module, 
                                      struct symt_function* func,
                                      const struct codeview_linetab* linetab,
                                      unsigned offset, unsigned size)
{
    unsigned int        i;

    if (!linetab) return;
    for (i = 0; i < linetab->nline; i++)
    {
        if (linetab->offtab[i] >= offset && linetab->offtab[i] < offset + size)
        {
            symt_add_func_line(module, func, linetab->source,
                               linetab->linetab[i], linetab->offtab[i] - offset);
        }
    }
}

static int codeview_snarf(const struct msc_debug_info* msc_dbg, const BYTE* root, 
                          int offset, int size,
                          struct codeview_linetab* linetab)
{
    struct symt_function*               curr_func = NULL;
    int                                 i, length;
    const struct codeview_linetab*      flt;
    struct symt_block*                  block = NULL;
    struct symt*                        symt;
    const char*                         name;
    struct symt_compiland*              compiland = NULL;
    struct location                     loc;

    /*
     * Loop over the different types of records and whenever we
     * find something we are interested in, record it and move on.
     */
    for (i = offset; i < size; i += length)
    {
        const union codeview_symbol* sym = (const union codeview_symbol*)(root + i);
        length = sym->generic.len + 2;
        if (i + length > size) break;
        if (length & 3) FIXME("unpadded len %u\n", length);

        switch (sym->generic.id)
        {
        /*
         * Global and local data symbols.  We don't associate these
         * with any given source file.
         */
	case S_GDATA_V1:
	case S_LDATA_V1:
            symt_new_global_variable(msc_dbg->module, compiland,
                                     terminate_string(&sym->data_v1.p_name), sym->generic.id == S_LDATA_V1,
                                     codeview_get_address(msc_dbg, sym->data_v1.segment, sym->data_v1.offset),
                                     0,
                                     codeview_get_type(sym->data_v1.symtype, FALSE));
	    break;
	case S_GDATA_V2:
	case S_LDATA_V2:
            name = terminate_string(&sym->data_v2.p_name);
            if (name)
                symt_new_global_variable(msc_dbg->module, compiland,
                                         name, sym->generic.id == S_LDATA_V2,
                                         codeview_get_address(msc_dbg, sym->data_v2.segment, sym->data_v2.offset),
                                         0,
                                         codeview_get_type(sym->data_v2.symtype, FALSE));
	    break;
	case S_GDATA_V3:
	case S_LDATA_V3:
            if (*sym->data_v3.name)
                symt_new_global_variable(msc_dbg->module, compiland,
                                         sym->data_v3.name,
                                         sym->generic.id == S_LDATA_V3,
                                         codeview_get_address(msc_dbg, sym->data_v3.segment, sym->data_v3.offset),
                                         0,
                                         codeview_get_type(sym->data_v3.symtype, FALSE));
	    break;

	case S_PUB_V1: /* FIXME is this really a 'data_v1' structure ?? */
            if (!(dbghelp_options & SYMOPT_NO_PUBLICS))
            {
                symt_new_public(msc_dbg->module, compiland,
                                terminate_string(&sym->data_v1.p_name), 
                                codeview_get_address(msc_dbg, sym->data_v1.segment, sym->data_v1.offset),
                                1, TRUE /* FIXME */, TRUE /* FIXME */);
            }
            break;
	case S_PUB_V2: /* FIXME is this really a 'data_v2' structure ?? */
            if (!(dbghelp_options & SYMOPT_NO_PUBLICS))
            {
                symt_new_public(msc_dbg->module, compiland,
                                terminate_string(&sym->data_v2.p_name), 
                                codeview_get_address(msc_dbg, sym->data_v2.segment, sym->data_v2.offset),
                                1, TRUE /* FIXME */, TRUE /* FIXME */);
            }
	    break;

        /*
         * Sort of like a global function, but it just points
         * to a thunk, which is a stupid name for what amounts to
         * a PLT slot in the normal jargon that everyone else uses.
         */
	case S_THUNK_V1:
            symt_new_thunk(msc_dbg->module, compiland,
                           terminate_string(&sym->thunk_v1.p_name), sym->thunk_v1.thtype,
                           codeview_get_address(msc_dbg, sym->thunk_v1.segment, sym->thunk_v1.offset),
                           sym->thunk_v1.thunk_len);
	    break;
	case S_THUNK_V3:
            symt_new_thunk(msc_dbg->module, compiland,
                           sym->thunk_v3.name, sym->thunk_v3.thtype,
                           codeview_get_address(msc_dbg, sym->thunk_v3.segment, sym->thunk_v3.offset),
                           sym->thunk_v3.thunk_len);
	    break;

        /*
         * Global and static functions.
         */
	case S_GPROC_V1:
	case S_LPROC_V1:
            flt = codeview_get_linetab(linetab, sym->proc_v1.segment, sym->proc_v1.offset);
            if (curr_func) FIXME("nested function\n");
            curr_func = symt_new_function(msc_dbg->module, compiland,
                                          terminate_string(&sym->proc_v1.p_name),
                                          codeview_get_address(msc_dbg, sym->proc_v1.segment, sym->proc_v1.offset),
                                          sym->proc_v1.proc_len,
                                          codeview_get_type(sym->proc_v1.proctype, FALSE));
            codeview_add_func_linenum(msc_dbg->module, curr_func, flt, 
                                      sym->proc_v1.offset, sym->proc_v1.proc_len);
            loc.kind = loc_absolute;
            loc.offset = sym->proc_v1.debug_start;
            symt_add_function_point(msc_dbg->module, curr_func, SymTagFuncDebugStart, &loc, NULL);
            loc.offset = sym->proc_v1.debug_end;
            symt_add_function_point(msc_dbg->module, curr_func, SymTagFuncDebugEnd, &loc, NULL);
	    break;
	case S_GPROC_V2:
	case S_LPROC_V2:
            flt = codeview_get_linetab(linetab, sym->proc_v2.segment, sym->proc_v2.offset);
            if (curr_func) FIXME("nested function\n");
            curr_func = symt_new_function(msc_dbg->module, compiland,
                                          terminate_string(&sym->proc_v2.p_name),
                                          codeview_get_address(msc_dbg, sym->proc_v2.segment, sym->proc_v2.offset),
                                          sym->proc_v2.proc_len,
                                          codeview_get_type(sym->proc_v2.proctype, FALSE));
            codeview_add_func_linenum(msc_dbg->module, curr_func, flt, 
                                      sym->proc_v2.offset, sym->proc_v2.proc_len);
            loc.kind = loc_absolute;
            loc.offset = sym->proc_v2.debug_start;
            symt_add_function_point(msc_dbg->module, curr_func, SymTagFuncDebugStart, &loc, NULL);
            loc.offset = sym->proc_v2.debug_end;
            symt_add_function_point(msc_dbg->module, curr_func, SymTagFuncDebugEnd, &loc, NULL);
	    break;
	case S_GPROC_V3:
	case S_LPROC_V3:
            flt = codeview_get_linetab(linetab, sym->proc_v3.segment, sym->proc_v3.offset);
            if (curr_func) FIXME("nested function\n");
            curr_func = symt_new_function(msc_dbg->module, compiland,
                                          sym->proc_v3.name,
                                          codeview_get_address(msc_dbg, sym->proc_v3.segment, sym->proc_v3.offset),
                                          sym->proc_v3.proc_len,
                                          codeview_get_type(sym->proc_v3.proctype, FALSE));
            codeview_add_func_linenum(msc_dbg->module, curr_func, flt, 
                                      sym->proc_v3.offset, sym->proc_v3.proc_len);
            loc.kind = loc_absolute;
            loc.offset = sym->proc_v3.debug_start;
            symt_add_function_point(msc_dbg->module, curr_func, SymTagFuncDebugStart, &loc, NULL);
            loc.offset = sym->proc_v3.debug_end;
            symt_add_function_point(msc_dbg->module, curr_func, SymTagFuncDebugEnd, &loc, NULL);
	    break;
        /*
         * Function parameters and stack variables.
         */
	case S_BPREL_V1:
            loc.kind = loc_regrel;
            loc.reg = 0; /* FIXME */
            loc.offset = sym->stack_v1.offset;
            symt_add_func_local(msc_dbg->module, curr_func, 
                                sym->stack_v1.offset > 0 ? DataIsParam : DataIsLocal, 
                                &loc, block,
                                codeview_get_type(sym->stack_v1.symtype, FALSE),
                                terminate_string(&sym->stack_v1.p_name));
            break;
	case S_BPREL_V2:
            loc.kind = loc_regrel;
            loc.reg = 0; /* FIXME */
            loc.offset = sym->stack_v2.offset;
            symt_add_func_local(msc_dbg->module, curr_func, 
                                sym->stack_v2.offset > 0 ? DataIsParam : DataIsLocal, 
                                &loc, block,
                                codeview_get_type(sym->stack_v2.symtype, FALSE),
                                terminate_string(&sym->stack_v2.p_name));
            break;
	case S_BPREL_V3:
            loc.kind = loc_regrel;
            loc.reg = 0; /* FIXME */
            loc.offset = sym->stack_v3.offset;
            symt_add_func_local(msc_dbg->module, curr_func, 
                                sym->stack_v3.offset > 0 ? DataIsParam : DataIsLocal, 
                                &loc, block,
                                codeview_get_type(sym->stack_v3.symtype, FALSE),
                                sym->stack_v3.name);
            break;

        case S_REGISTER_V1:
            loc.kind = loc_register;
            loc.reg = sym->register_v1.reg;
            loc.offset = 0;
            symt_add_func_local(msc_dbg->module, curr_func, 
                                DataIsLocal, &loc,
                                block, codeview_get_type(sym->register_v1.type, FALSE),
                                terminate_string(&sym->register_v1.p_name));
            break;
        case S_REGISTER_V2:
            loc.kind = loc_register;
            loc.reg = sym->register_v2.reg;
            loc.offset = 0;
            symt_add_func_local(msc_dbg->module, curr_func, 
                                DataIsLocal, &loc,
                                block, codeview_get_type(sym->register_v2.type, FALSE),
                                terminate_string(&sym->register_v2.p_name));
            break;

        case S_BLOCK_V1:
            block = symt_open_func_block(msc_dbg->module, curr_func, block, 
                                         codeview_get_address(msc_dbg, sym->block_v1.segment, sym->block_v1.offset),
                                         sym->block_v1.length);
            break;
        case S_BLOCK_V3:
            block = symt_open_func_block(msc_dbg->module, curr_func, block, 
                                         codeview_get_address(msc_dbg, sym->block_v3.segment, sym->block_v3.offset),
                                         sym->block_v3.length);
            break;

        case S_END_V1:
            if (block)
            {
                block = symt_close_func_block(msc_dbg->module, curr_func, block, 0);
            }
            else if (curr_func)
            {
                symt_normalize_function(msc_dbg->module, curr_func);
                curr_func = NULL;
            }
            break;

        case S_COMPILAND_V1:
            TRACE("S-Compiland-V1 %x %s\n",
                  sym->compiland_v1.unknown, terminate_string(&sym->compiland_v1.p_name));
            break;

        case S_COMPILAND_V2:
            TRACE("S-Compiland-V2 %s\n", terminate_string(&sym->compiland_v2.p_name));
            if (TRACE_ON(dbghelp_msc))
            {
                /* !! not sure what this is for since this will never print anything out.
                        Note that *(str + strlen(str)) == 0 always. */

                const char* ptr1 = sym->compiland_v2.p_name.name + sym->compiland_v2.p_name.namelen;
                const char* ptr2;
                while (*ptr1)
                {
                    ptr2 = ptr1 + strlen(ptr1) + 1;
                    TRACE("\t%s => %s\n", ptr1, ptr2); 
                    ptr1 = ptr2 + strlen(ptr2) + 1;
                }
            }
            break;
        case S_COMPILAND_V3:
            TRACE("S-Compiland-V3 %s\n", sym->compiland_v3.name);
            if (TRACE_ON(dbghelp_msc))
            {
                /* !! not sure what this is for since this will never print anything out.
                        Note that *(str + strlen(str)) == 0 always. */

                const char* ptr1 = sym->compiland_v3.name + strlen(sym->compiland_v3.name);
                const char* ptr2;
                while (*ptr1)
                {
                    ptr2 = ptr1 + strlen(ptr1) + 1;
                    TRACE("\t%s => %s\n", ptr1, ptr2); 
                    ptr1 = ptr2 + strlen(ptr2) + 1;
                }
            }
            break;

        case S_OBJNAME_V1:
            TRACE("S-ObjName %s\n", terminate_string(&sym->objname_v1.p_name));
            compiland = symt_new_compiland(msc_dbg->module, 0 /* FIXME */,
                                           source_new(msc_dbg->module, NULL,
                                                      terminate_string(&sym->objname_v1.p_name)));
            break;

        case S_LABEL_V1:
            if (curr_func)
            {
                loc.kind = loc_absolute;
                loc.offset = codeview_get_address(msc_dbg, sym->label_v1.segment, sym->label_v1.offset) - curr_func->address;
                symt_add_function_point(msc_dbg->module, curr_func, SymTagLabel, &loc,
                                        terminate_string(&sym->label_v1.p_name));
            }
            else{

                /* this case doesn't need to be checked for length since the max length is 255 characters and
                   the terminate_string() function takes that into account */
                FIXME("No current function for V1 label %s\n",
                        terminate_string(&sym->label_v1.p_name));
            }

            break;
        case S_LABEL_V3:
            if (curr_func)
            {
                loc.kind = loc_absolute;
                loc.offset = codeview_get_address(msc_dbg, sym->label_v3.segment, sym->label_v3.offset) - curr_func->address;
                symt_add_function_point(msc_dbg->module, curr_func, SymTagLabel, 
                                        &loc, sym->label_v3.name);
            }
            else if (TRACE_ON(dbghelp_msc)){

                /* the compiler inserts a label before every function that uses C++ exception handling (try-catch).
                   The label's name always takes the form "__ehhandler$<giant_decorated_function_name>".  Since these
                   labels being outside of a function is not actually an error, let's filter out those cases.  The
                   function names can get really really long (think member functions of nested templates being passed
                   and returning many other nested templates), and the TRACE/FIXME/ERR macros will abort the process
                   if their buffers overflow (which they do for lots of function names).  Because of all this, we'll 
                   just prevent those names from being written out at all. */
                const char *ehName = "__ehhandler$";


                /* C++ EH wrapped function => just trace out an event */
                if (!strncmp(sym->label_v3.name, ehName, strlen(ehName)))
                    TRACE("found a C++ EH wrapped function label\n");

                /* this is most often not an error or fixme case either.  The function-less labels are generally 
                   initializer labels for global or static variables.  We'll trace these out for curiosity purposes
                   just in case some bad case sneaks through. */
                else
                    FIXME("No current function for V3 label %s.  Possibly a global initializer label\n", sym->label_v3.name);
            }

            break;

        case S_CONSTANT_V1:
            {
                int                     vlen;
                const struct p_string*  name;
                struct symt*            se;
                VARIANT                 v;

                v.n1.n2.vt = VT_I4;
                vlen = numeric_leaf(&v.n1.n2.n3.intVal, &sym->constant_v1.cvalue);
                name = (const struct p_string*)((const char*)&sym->constant_v1.cvalue + vlen);
                se = codeview_get_type(sym->constant_v1.type, FALSE);

                TRACE("S-Constant-V1 %u %s %x\n",
                      v.n1.n2.n3.intVal, terminate_string(name), sym->constant_v1.type);
                symt_new_constant(msc_dbg->module, compiland, terminate_string(name),
                                  se, &v);
            }
            break;
        case S_CONSTANT_V2:
            {
                int                     vlen;
                const struct p_string*  name;
                struct symt*            se;
                VARIANT                 v;

                v.n1.n2.vt = VT_I4;
                vlen = numeric_leaf(&v.n1.n2.n3.intVal, &sym->constant_v2.cvalue);
                name = (const struct p_string*)((const char*)&sym->constant_v2.cvalue + vlen);
                se = codeview_get_type(sym->constant_v2.type, FALSE);

                TRACE("S-Constant-V2 %u %s %x\n",
                      v.n1.n2.n3.intVal, terminate_string(name), sym->constant_v2.type);
                symt_new_constant(msc_dbg->module, compiland, terminate_string(name),
                                  se, &v);
            }
            break;
        case S_CONSTANT_V3:
            {
                int                     vlen;
                const char*             name;
                struct symt*            se;
                VARIANT                 v;

                v.n1.n2.vt = VT_I4;
                vlen = numeric_leaf(&v.n1.n2.n3.intVal, &sym->constant_v3.cvalue);
                name = (const char*)&sym->constant_v3.cvalue + vlen;
                se = codeview_get_type(sym->constant_v3.type, FALSE);

                TRACE("S-Constant-V3 %u %s %x\n",
                      v.n1.n2.n3.intVal, name, sym->constant_v3.type);
                /* FIXME: we should add this as a constant value */
            }
            break;

        case S_UDT_V1:
            if (sym->udt_v1.type)
            {
                if ((symt = codeview_get_type(sym->udt_v1.type, FALSE)))
                    symt_new_typedef(msc_dbg->module, symt, 
                                     terminate_string(&sym->udt_v1.p_name));
                else
                    FIXME("S-Udt %s: couldn't find type 0x%x\n", 
                          terminate_string(&sym->udt_v1.p_name), sym->udt_v1.type);
            }
            break;
        case S_UDT_V2:
            if (sym->udt_v2.type)
            {
                if ((symt = codeview_get_type(sym->udt_v2.type, FALSE)))
                    symt_new_typedef(msc_dbg->module, symt, 
                                     terminate_string(&sym->udt_v2.p_name));
                else
                    FIXME("S-Udt %s: couldn't find type 0x%x\n", 
                          terminate_string(&sym->udt_v2.p_name), sym->udt_v2.type);
            }
            break;
        case S_UDT_V3:
            if (sym->udt_v3.type)
            {
                if ((symt = codeview_get_type(sym->udt_v3.type, FALSE)))
                    symt_new_typedef(msc_dbg->module, symt, sym->udt_v3.name);
                else
                    FIXME("S-Udt %s: couldn't find type 0x%x\n", 
                          sym->udt_v3.name, sym->udt_v3.type);
            }
            break;

         /*
         * These are special, in that they are always followed by an
         * additional length-prefixed string which is *not* included
         * into the symbol length count.  We need to skip it.
         */
	case S_PROCREF_V1:
	case S_DATAREF_V1:
	case S_LPROCREF_V1:
            name = (const char*)sym + length;
            length += (*name + 1 + 3) & ~3;
            break;

        case S_PUB_V3:
            if (!(dbghelp_options & SYMOPT_NO_PUBLICS))
            {
                symt_new_public(msc_dbg->module, compiland,
                                sym->data_v3.name, 
                                codeview_get_address(msc_dbg, sym->data_v3.segment, sym->data_v3.offset),
                                1, FALSE /* FIXME */, FALSE);
            }
            break;
        case S_PUB_FUNC1_V3:
        case S_PUB_FUNC2_V3: /* using a data_v3 isn't what we'd expect */
            if (!(dbghelp_options & SYMOPT_NO_PUBLICS))
            {
                symt_new_public(msc_dbg->module, compiland,
                                sym->data_v3.name, 
                                codeview_get_address(msc_dbg, sym->data_v3.segment, sym->data_v3.offset),
                                1, TRUE /* FIXME */, TRUE);
            }
            break;

        case S_MSTOOL_V3: /* just to silence a few warnings */
            /* compiler options and build settings 
                This is a sequence of strings starting with "Microsoft (R) Optimizing Compiler".  After the
                compiler string there is a sequence of string pairs.  The first string of each pair describes
                the purpose (ie: 'cwd', 'cl', 'cmd', 'src', 'pdb').  The second string is the value for the
                first string.  No real need to parse this block, just skip it.
            */
            break;

        case S_SSEARCH_V1:
            TRACE("Start search: seg=0x%x at offset 0x%08x\n",
                  sym->ssearch_v1.segment, sym->ssearch_v1.offset);
            break;

        case S_ALIGN_V1:
            TRACE("S-Align V1\n");
            break;

        default:
            FIXME("Unsupported symbol id %x\n", sym->generic.id);
            dump(sym, 2 + sym->generic.len);
            break;
        }
    }

    if (curr_func) symt_normalize_function(msc_dbg->module, curr_func);

    HeapFree(GetProcessHeap(), 0, linetab);
    return TRUE;
}

/*========================================================================
 * Process PDB file.
 */

static void* pdb_jg_read(const struct PDB_JG_HEADER* pdb, const WORD* block_list,
                         int size)
{
    int                         i, num_blocks;
    BYTE*                       buffer;

    if (!size) return NULL;

    num_blocks = (size + pdb->block_size - 1) / pdb->block_size;
    buffer = HeapAlloc(GetProcessHeap(), 0, num_blocks * pdb->block_size);

    for (i = 0; i < num_blocks; i++)
        memcpy(buffer + i * pdb->block_size,
               (const char*)pdb + block_list[i] * pdb->block_size, pdb->block_size);

    return buffer;
}

static void* pdb_ds_read(const struct PDB_DS_HEADER* pdb, const DWORD* block_list,
                         int size)
{
    int                         i, num_blocks;
    BYTE*                       buffer;

    if (!size) return NULL;

    num_blocks = (size + pdb->block_size - 1) / pdb->block_size;
    buffer = HeapAlloc(GetProcessHeap(), 0, num_blocks * pdb->block_size);

    for (i = 0; i < num_blocks; i++)
        memcpy(buffer + i * pdb->block_size,
               (const char*)pdb + block_list[i] * pdb->block_size, pdb->block_size);

    return buffer;
}

static void* pdb_read_jg_file(const struct PDB_JG_HEADER* pdb,
                              const struct PDB_JG_TOC* toc, DWORD file_nr)
{
    const WORD*                 block_list;
    DWORD                       i;

    if (!toc || file_nr >= toc->num_files) return NULL;

    block_list = (const WORD*) &toc->file[toc->num_files];
    for (i = 0; i < file_nr; i++)
        block_list += (toc->file[i].size + pdb->block_size - 1) / pdb->block_size;

    return pdb_jg_read(pdb, block_list, toc->file[file_nr].size);
}

static void* pdb_read_ds_file(const struct PDB_DS_HEADER* pdb,
                              const struct PDB_DS_TOC* toc, DWORD file_nr)
{
    const DWORD*                block_list;
    DWORD                       i;

    if (!toc || file_nr >= toc->num_files){
        ERR("invalid TOC or file number {toc = %p, file_nr = %u, num_files = %u}\n", toc, file_nr, toc ? toc->num_files : -1);
        return NULL;
    }

    if (toc->file_size[file_nr] == 0 || toc->file_size[file_nr] == 0xFFFFFFFF)
    {
        FIXME(">>> requesting NULL stream (%u)\n", file_nr);
        return NULL;
    }
    block_list = &toc->file_size[toc->num_files];
    for (i = 0; i < file_nr; i++)
        block_list += (toc->file_size[i] + pdb->block_size - 1) / pdb->block_size;

    return pdb_ds_read(pdb, block_list, toc->file_size[file_nr]);
}

static void* pdb_read_file(const char* image, const struct pdb_lookup* pdb_lookup,
                           DWORD file_nr)
{
    switch (pdb_lookup->kind)
    {
    case PDB_JG:
        return pdb_read_jg_file((const struct PDB_JG_HEADER*)image, 
                                pdb_lookup->u.jg.toc, file_nr);
    case PDB_DS:
        return pdb_read_ds_file((const struct PDB_DS_HEADER*)image,
                                pdb_lookup->u.ds.toc, file_nr);
    }
    return NULL;
}

static unsigned pdb_get_file_size(const struct pdb_lookup* pdb_lookup, DWORD file_nr)
{
    switch (pdb_lookup->kind)
    {
    case PDB_JG: return pdb_lookup->u.jg.toc->file[file_nr].size;
    case PDB_DS: return pdb_lookup->u.ds.toc->file_size[file_nr];
    }
    return 0;
}

static void pdb_free(void* buffer)
{
    HeapFree(GetProcessHeap(), 0, buffer);
}

static void pdb_free_lookup(const struct pdb_lookup* pdb_lookup)
{
    switch (pdb_lookup->kind)
    {
    case PDB_JG:
        pdb_free(pdb_lookup->u.jg.toc);
        break;
    case PDB_DS:
        pdb_free(pdb_lookup->u.ds.toc);
        break;
    }
}
    
static void pdb_convert_types_header(PDB_TYPES* types, const BYTE* image)
{
    memset(types, 0, sizeof(PDB_TYPES));
    if (!image) return;

    if (*(const DWORD*)image < 19960000)   /* FIXME: correct version? */
    {
        /* Old version of the types record header */
        const PDB_TYPES_OLD*    old = (const PDB_TYPES_OLD*)image;
        types->version     = old->version;
        types->type_offset = sizeof(PDB_TYPES_OLD);
        types->type_size   = old->type_size;
        types->first_index = old->first_index;
        types->last_index  = old->last_index;
        types->file        = old->file;
    }
    else
    {
        /* New version of the types record header */
        *types = *(const PDB_TYPES*)image;
    }
}

static void pdb_convert_symbols_header(PDB_SYMBOLS* symbols,
                                       int* header_size, const BYTE* image)
{
    memset(symbols, 0, sizeof(PDB_SYMBOLS));
    if (!image) return;

    if (*(const DWORD*)image != 0xffffffff)
    {
        /* Old version of the symbols record header */
        const PDB_SYMBOLS_OLD*  old = (const PDB_SYMBOLS_OLD*)image;
        symbols->version         = 0;
        symbols->module_size     = old->module_size;
        symbols->offset_size     = old->offset_size;
        symbols->hash_size       = old->hash_size;
        symbols->srcmodule_size  = old->srcmodule_size;
        symbols->pdbimport_size  = 0;
        symbols->hash1_file      = old->hash1_file;
        symbols->hash2_file      = old->hash2_file;
        symbols->gsym_file       = old->gsym_file;

        *header_size = sizeof(PDB_SYMBOLS_OLD);
    }
    else
    {
        /* New version of the symbols record header */
        *symbols = *(const PDB_SYMBOLS*)image;
        *header_size = sizeof(PDB_SYMBOLS);
    }
}

static void pdb_convert_symbol_file(const PDB_SYMBOLS* symbols, 
                                    PDB_SYMBOL_FILE_EX* sfile, 
                                    unsigned* size, const void* image)

{
    if (symbols->version < 19970000)
    {
        const PDB_SYMBOL_FILE *sym_file = (const PDB_SYMBOL_FILE*)image;
        memset(sfile, 0, sizeof(*sfile));
        sfile->file        = sym_file->file;
        sfile->range.index = sym_file->range.index;
        sfile->symbol_size = sym_file->symbol_size;
        sfile->lineno_size = sym_file->lineno_size;
        *size = sizeof(PDB_SYMBOL_FILE) - 1;
    }
    else
    {
        memcpy(sfile, image, sizeof(PDB_SYMBOL_FILE_EX));
        *size = sizeof(PDB_SYMBOL_FILE_EX) - 1;
    }
}

static HANDLE open_pdb_file(const struct process* pcs,
                            const struct pdb_lookup* lookup,
                            struct module* module)
{
    HANDLE      h;
    char        dbg_file_path[MAX_PATH];
    BOOL        ret = FALSE;

    switch (lookup->kind)
    {
    case PDB_JG:
        ret = path_find_symbol_file(pcs, lookup->filename, NULL, lookup->u.jg.timestamp,
                                    lookup->age, dbg_file_path, &module->module.PdbUnmatched);
        break;
    case PDB_DS:
        ret = path_find_symbol_file(pcs, lookup->filename, &lookup->u.ds.guid, 0,
                                    lookup->age, dbg_file_path, &module->module.PdbUnmatched);
        break;
    }
    if (!ret)
    {
        WARN("\tCouldn't find %s\n", lookup->filename);
        return 0;
    }
    h = CreateFileA(dbg_file_path, GENERIC_READ, FILE_SHARE_READ, NULL, 
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    TRACE(".PDB '%s': '%s' returns 0x%08x\n", lookup->filename, dbg_file_path, h);
    return (h == INVALID_HANDLE_VALUE) ? 0 : h;
}

static void pdb_process_types(const struct msc_debug_info* msc_dbg, 
                              const char* image, const struct pdb_lookup* pdb_lookup)
{
    BYTE*       types_image = NULL;

    types_image = pdb_read_file(image, pdb_lookup, 2);
    if (types_image)
    {
        PDB_TYPES               types;
        struct codeview_type_parse      ctp;
        DWORD                   total;
        const BYTE*             ptr;
        DWORD*                  offset;

        pdb_convert_types_header(&types, types_image);

        TRACE("type info version number is %u\n", types.version);

        /* Check for unknown versions */
        switch (types.version)
        {
        case 19950410:      /* VC 4.0 */
        case 19951122:
        case 19961031:      /* VC 5.0 / 6.0 */
        case 19990903:      /* VC 7.1 */
        case 20040203:      /* VC 8.0 */
            break;
        default:
            ERR("-Unknown type info version %u\n", types.version);
        }

        ctp.module = msc_dbg->module;
        /* reconstruct the types offset...
         * FIXME: maybe it's present in the newest PDB_TYPES structures
         */
        total = types.last_index - types.first_index + 1;
        offset = HeapAlloc(GetProcessHeap(), 0, sizeof(DWORD) * total);
        ctp.table = ptr = types_image + types.type_offset;
        ctp.num = 0;
        while (ptr < ctp.table + types.type_size && ctp.num < total)
        {
            offset[ctp.num++] = ptr - ctp.table;
            ptr += ((const union codeview_type*)ptr)->generic.len + 2;
        }
        ctp.offset = offset;
        ctp.first = types.first_index;

        if (types.last_index != ctp.first + ctp.num)
            FIXME("the type count doesn't match {first_index = 0x%08x, last_index = 0x%08x, num = 0x%08x}\n", ctp.first, types.last_index, ctp.num);

        /* sanity check -> there are still a number of places where we do not have access to a codeview_type_parse 
            object so we can't actually use the <first> member for this value */
        if (ctp.first != FIRST_DEFINABLE_TYPE)
            FIXME("this PDB has a first UDT index that is different from FIRST_DEFINABLE_TYPE!  FIX IT FIX IT FIX IT FIX IT!\n");

        /* Read type table */
        codeview_parse_type_table(&ctp);
        HeapFree(GetProcessHeap(), 0, offset);
        pdb_free(types_image);
    }
}

static const char       PDB_JG_IDENT[] = "Microsoft C/C++ program database 2.00\r\n\032JG\0";
static const char       PDB_DS_IDENT[] = "Microsoft C/C++ MSF 7.00\r\n\032DS\0";

/******************************************************************
 *		pdb_init
 *
 * Tries to load a pdb file
 * if do_fill is TRUE, then it just fills pdb_lookup with the information of the
 *      file
 * if do_fill is FALSE, then it just checks that the kind of PDB (stored in
 *      pdb_lookup) matches what's really in the file
 */
static BOOL pdb_init(struct pdb_lookup* pdb_lookup, const char* image, BOOL do_fill)
{
    BOOL        ret = TRUE;

    /* check the file header, and if ok, load the TOC */
    TRACE("PDB(%s): %.40s\n", pdb_lookup->filename, debugstr_an(image, 40));

    if (!memcmp(image, PDB_JG_IDENT, sizeof(PDB_JG_IDENT)))
    {
        const struct PDB_JG_HEADER* pdb = (const struct PDB_JG_HEADER*)image;
        struct PDB_JG_ROOT*         root;

        pdb_lookup->u.jg.toc = pdb_jg_read(pdb, pdb->toc_block, pdb->toc.size);
        root = pdb_read_jg_file(pdb, pdb_lookup->u.jg.toc, 1);
        if (!root)
        {
            ERR("-Unable to get root from .PDB in %s\n", pdb_lookup->filename);
            return FALSE;
        }
        switch (root->Version)
        {
        case 19950623:      /* VC 4.0 */
        case 19950814:
        case 19960307:      /* VC 5.0 */
        case 19970604:      /* VC 6.0 */
            break;
        default:
            ERR("-Unknown root block version %u\n", root->Version);
        }
        if (do_fill)
        {
            pdb_lookup->kind = PDB_JG;
            pdb_lookup->u.jg.timestamp = root->TimeDateStamp;
            pdb_lookup->age = root->Age;
        }
        else if (pdb_lookup->kind != PDB_JG ||
                 pdb_lookup->u.jg.timestamp != root->TimeDateStamp ||
                 pdb_lookup->age != root->Age)
            ret = FALSE;
        TRACE("found JG/%c for %s: age=%x timestamp=%x\n",
              do_fill ? 'f' : '-', pdb_lookup->filename, root->Age,
              root->TimeDateStamp);
        pdb_free(root);
    }
    else if (!memcmp(image, PDB_DS_IDENT, sizeof(PDB_DS_IDENT)))
    {
        const struct PDB_DS_HEADER* pdb = (const struct PDB_DS_HEADER*)image;
        struct PDB_DS_ROOT*         root;

        pdb_lookup->u.ds.toc = 
            pdb_ds_read(pdb, 
                        (const DWORD*)((const char*)pdb + pdb->toc_page * pdb->block_size), 
                        pdb->toc_size);
        root = pdb_read_ds_file(pdb, pdb_lookup->u.ds.toc, 1);
        if (!root)
        {
            ERR("-Unable to get root from .PDB in %s\n", pdb_lookup->filename);
            return FALSE;
        }


        switch (root->Version)
        {
        case 20000404:
            break;
        default:
            ERR("-Unknown root block version %u\n", root->Version);
        }
        if (do_fill)
        {
            pdb_lookup->kind = PDB_DS;
            pdb_lookup->u.ds.guid = root->guid;
            pdb_lookup->age = root->Age;
        }
        else if (pdb_lookup->kind != PDB_DS ||
                 memcmp(&pdb_lookup->u.ds.guid, &root->guid, sizeof(GUID)) ||
                 pdb_lookup->age != root->Age)
            ret = FALSE;
        TRACE("found DS/%c for %s: age=%x guid=%s\n",
              do_fill ? 'f' : '-', pdb_lookup->filename, root->Age,
              debugstr_guid(&root->guid));
        pdb_free(root);
    }

    if (0) /* some tool to dump the internal files from a PDB file */
    {
        const struct PDB_DS_HEADER *pdbds = (const struct PDB_DS_HEADER*)image;
        const struct PDB_JG_HEADER *pdbjg = (const struct PDB_JG_HEADER*)image;
        int                         i, num_files;
        int                         block_size;
        FILE *                      fp;
        char                        filename[MAX_PATH];
        int                         blockTotal = 0;
        int                         numBlocks;
        

        switch (pdb_lookup->kind){
            case PDB_JG:
                num_files = pdb_lookup->u.jg.toc->num_files;
                block_size = pdbjg->block_size;
                numBlocks = pdbjg->total_alloc;
                break;

            case PDB_DS:
                num_files = pdb_lookup->u.ds.toc->num_files;
                block_size = pdbds->block_size;
                numBlocks = pdbds->num_pages;
                break;
        }


        for (i = 0; i < num_files; i++)
        {
            unsigned char*  x = pdb_read_file(image, pdb_lookup, i);
            unsigned int    size = pdb_get_file_size(pdb_lookup, i);


            /* prevent this from reading NULL memory */
            if (x == NULL){
                WARN("********************** file %d is empty or missing {size = %u}\n", i, size);

                continue;
            }


            snprintf(filename, MAX_PATH, "pdbfile_%02d_%u_bytes_%d_blocks.bin", i, size, (size + block_size - 1) / block_size);
            fp = fopen(filename, "wb");
            blockTotal += (size + block_size - 1) / block_size;

            if (fp){
                FIXME("********************** [file %d]: dumping to '%s' {size = %u}\n", i, filename, size);
                fwrite(x, 1, size, fp);
                
                fclose(fp);
            }

            else{
                FIXME("********************** [file %d]: ERROR-> could not open the file '%s' to dump the data to\n", i, filename);
                dump(x, size);
            }

            pdb_free(x);
        }

        FIXME("********************** %d/%d blocks are used in the file\n", blockTotal, numBlocks);
    }
    return ret;
}

static BOOL pdb_process_internal(const struct process* pcs, 
                                 const struct msc_debug_info* msc_dbg,
                                 struct pdb_lookup* pdb_lookup,
                                 unsigned module_index);

static void pdb_process_symbol_imports(const struct process* pcs, 
                                       const struct msc_debug_info* msc_dbg,
                                       const PDB_SYMBOLS* symbols,
                                       const void* symbols_image,
                                       const char* image,
                                       const struct pdb_lookup* pdb_lookup,
                                       unsigned module_index)
{
    if (module_index == -1 && symbols && symbols->pdbimport_size)
    {
        const PDB_SYMBOL_IMPORT*imp;
        const void*             first;
        const void*             last;
        const char*             ptr;
        int                     i = 0;

        imp = (const PDB_SYMBOL_IMPORT*)((const char*)symbols_image + sizeof(PDB_SYMBOLS) + 
                                         symbols->module_size + symbols->offset_size + 
                                         symbols->hash_size + symbols->srcmodule_size);
        first = (const char*)imp;
        last = (const char*)imp + symbols->pdbimport_size;
        while (imp < (const PDB_SYMBOL_IMPORT*)last)
        {
            ptr = (const char*)imp + sizeof(*imp) + strlen(imp->filename);
            if (i >= CV_MAX_MODULES) FIXME("Out of bounds !!!\n");
            if (!strcasecmp(pdb_lookup->filename, imp->filename))
            {
                if (module_index != -1) FIXME("Twice the entry\n");
                else module_index = i;
            }
            else
            {
                struct pdb_lookup       imp_pdb_lookup;

                /* FIXME: this is an import of a JG PDB file
                 * how's a DS PDB handled ?
                 */
                imp_pdb_lookup.filename = imp->filename;
                imp_pdb_lookup.kind = PDB_JG;
                imp_pdb_lookup.u.jg.timestamp = imp->TimeDateStamp;
                imp_pdb_lookup.age = imp->Age;
                TRACE("got for %s: age=%u ts=%x\n",
                      imp->filename, imp->Age, imp->TimeDateStamp);
                pdb_process_internal(pcs, msc_dbg, &imp_pdb_lookup, i);
            }
            i++;
            imp = (const PDB_SYMBOL_IMPORT*)((const char*)first + ((ptr - (const char*)first + strlen(ptr) + 1 + 3) & ~3));
        }
    }
    cv_current_module = &cv_zmodules[(module_index == -1) ? 0 : module_index];
    if (cv_current_module->allowed) FIXME("Already allowed ??\n");
    cv_current_module->allowed = TRUE;
    pdb_process_types(msc_dbg, image, pdb_lookup);
}

static BOOL pdb_process_internal(const struct process* pcs, 
                                 const struct msc_debug_info* msc_dbg,
                                 struct pdb_lookup* pdb_lookup, 
                                 unsigned module_index)
{
    BOOL        ret = FALSE;
    HANDLE      hFile, hMap = 0;
    char*       image = NULL;
    BYTE*       symbols_image = NULL;

    TRACE("Processing PDB file %s\n", pdb_lookup->filename);

    /* Open and map() .PDB file */
    if ((hFile = open_pdb_file(pcs, pdb_lookup, msc_dbg->module)) == 0 ||
        ((hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)) == 0) ||
        ((image = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) == NULL))
    {
        WARN("Unable to open .PDB file: %s\n", pdb_lookup->filename);
        goto leave;
    }

    TRACE("initializing the PDB file '%s'\n", pdb_lookup->filename);
    pdb_init(pdb_lookup, image, FALSE);

    TRACE("reading the symbols table\n");
    symbols_image = pdb_read_file(image, pdb_lookup, 3);

    if (symbols_image)
    {
        PDB_SYMBOLS symbols;
        BYTE*       modimage;
        BYTE*       file;
        int         header_size = 0;
        

        TRACE("converting the symbols header\n");
        pdb_convert_symbols_header(&symbols, &header_size, symbols_image);


        switch (symbols.version)
        {
        case 0:            /* VC 4.0 */
        case 19960307:     /* VC 5.0 */
        case 19970606:     /* VC 6.0 */
        case 19990903:     /* VC 7.0 */
        case 20040203:     /* VC 8.0 */
            break;
        default:
            ERR("-Unknown symbol info version %u %08x\n",
                symbols.version, symbols.version);
        }

        TRACE("processing symbol imports\n");
        pdb_process_symbol_imports(pcs, msc_dbg, &symbols, symbols_image, image, pdb_lookup, module_index);

        /* Read global symbol table */
        TRACE("reading global symbol table\n");
        /* NOTE: only the low word of the gsym_file value is the file number for the symbols file.  On VC8
                 PDBs the high word always seems to be 0x002a.  On VC7 PDBs the high word is 0x0000 (which
                 is why this worked in those cases).  Similarly for the hash1_file and hash2_file values -
                 VC7 always had the high word set to 0x38a0 and 0x0c05 respectively.  VC8 always had the
                 high word set to 0x8800 and 0xc627 respectively. */
        modimage = pdb_read_file(image, pdb_lookup, symbols.gsym_file & 0x0000ffff);
        if (modimage)
        {
            TRACE("dunno what this does\n");
            codeview_snarf(msc_dbg, modimage, 0, 
                           pdb_get_file_size(pdb_lookup, symbols.gsym_file & 0x0000ffff), NULL);

            pdb_free(modimage);
        }

        /* Read per-module symbol / linenumber tables */
        TRACE("reading symbols and line numbers\n");
        file = symbols_image + header_size;
        while ((size_t)(file - symbols_image) < (size_t)(header_size + symbols.module_size))
        {
            PDB_SYMBOL_FILE_EX          sfile;
            const char*                 file_name;
            unsigned                    size;

            HeapValidate(GetProcessHeap(), 0, NULL);
            pdb_convert_symbol_file(&symbols, &sfile, &size, file);

            modimage = pdb_read_file(image, pdb_lookup, sfile.file);
            if (modimage)
            {
                struct codeview_linetab*    linetab = NULL;

                
                TRACE("processing symbol file for '%s' {file = %d, symbolSize = %u, lineNoSize = %u, unknown2 = %u, nSrcFiles = %u}\n", 
                        file + size, sfile.file, sfile.symbol_size, sfile.lineno_size, sfile.unknown2, sfile.nSrcFiles);


                if (sfile.lineno_size)
                    linetab = codeview_snarf_linetab(msc_dbg->module, 
                                                     modimage + sfile.symbol_size,
                                                     sfile.lineno_size,
                                                     pdb_lookup->kind == PDB_JG);

                if (sfile.symbol_size)
                    codeview_snarf(msc_dbg, modimage, sizeof(DWORD),
                                   sfile.symbol_size, linetab);

                pdb_free(modimage);
            }


            /* PDB files generated through VC8 use a new format to store line number information.  The 
               <lineno_size> member of the PDB_SYMBOL_FILE_EX header is 0 in this case, but the <unknown2>
               member (next member) has a value.  It isn't clear if these two have changed to a single
               64-bit value, or if the <unknown2> member has been used as-is.  In theory the line number
               information for a very poorly laid out compile unit *could* exceed 4GB, but it's highly
               unlikely.  
               Since the format of the new line number info isn't known yet (it's definitely different 
               from the VC7 line number info block), we'll just spew a fixme here and leave the line
               number info blank */
            if (sfile.unknown2 && sfile.lineno_size == 0)
                FIXME("line number information is missing, but the mystery information block is present instead\n");

            file_name = (const char*)file + size;
            file_name += strlen(file_name) + 1;
            file = (BYTE*)((DWORD)(file_name + strlen(file_name) + 1 + 3) & ~3);
        }
    }
    else{
        ERR("could not read the PDB file.  Loading imports instead\n");

        pdb_process_symbol_imports(pcs, msc_dbg, NULL, NULL, image, pdb_lookup, 
                                   module_index);
    }

    ret = TRUE;

 leave:
    /* Cleanup */
    pdb_free(symbols_image);
    pdb_free_lookup(pdb_lookup);

    if (image) UnmapViewOfFile(image);
    if (hMap) CloseHandle(hMap);
    if (hFile) CloseHandle(hFile);

    return ret;
}

static BOOL pdb_process_file(const struct process* pcs, 
                             const struct msc_debug_info* msc_dbg,
                             struct pdb_lookup* pdb_lookup)
{
    BOOL        ret;



    


    memset(cv_zmodules, 0, sizeof(cv_zmodules));

    TRACE("initializing basic types\n");
    codeview_init_basic_types(msc_dbg->module);

    TRACE("processing the PDB file\n");
    ret = pdb_process_internal(pcs, msc_dbg, pdb_lookup, -1);

    TRACE("clearing type table\n");
    codeview_clear_type_table();

    TRACE("checking status\n");
    if (ret)
    {
        msc_dbg->module->module.SymType = SymCv;
        if (pdb_lookup->kind == PDB_JG)
            msc_dbg->module->module.PdbSig = pdb_lookup->u.jg.timestamp;
        else
            msc_dbg->module->module.PdbSig70 = pdb_lookup->u.ds.guid;
        msc_dbg->module->module.PdbAge = pdb_lookup->age;
        MultiByteToWideChar(CP_ACP, 0, pdb_lookup->filename, -1,
                            msc_dbg->module->module.LoadedPdbName,
                            sizeof(msc_dbg->module->module.LoadedPdbName) / sizeof(WCHAR));
        /* FIXME: we could have a finer grain here */
        msc_dbg->module->module.LineNumbers = TRUE;
        msc_dbg->module->module.GlobalSymbols = TRUE;
        msc_dbg->module->module.TypeInfo = TRUE;
        msc_dbg->module->module.SourceIndexed = TRUE;
        msc_dbg->module->module.Publics = TRUE;

        TRACE("saved load results\n");
    }

    else
        TRACE("failed to load the PDB file\n");

    return ret;
}

BOOL pdb_fetch_file_info(struct pdb_lookup* pdb_lookup)
{
    HANDLE              hFile, hMap = 0;
    char*               image = NULL;
    BOOL                ret = TRUE;

    if ((hFile = CreateFileA(pdb_lookup->filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE ||
        ((hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL)) == 0) ||
        ((image = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) == NULL))
    {
        WARN("Unable to open .PDB file: %s\n", pdb_lookup->filename);
        ret = FALSE;
    }
    else
    {
        TRACE("opened .PDB file '%s'\n", pdb_lookup->filename);
        pdb_init(pdb_lookup, image, TRUE);
        pdb_free_lookup(pdb_lookup);
    }

    if (image) UnmapViewOfFile(image);
    if (hMap) CloseHandle(hMap);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

    return ret;
}

/*========================================================================
 * Process CodeView debug information.
 */

#define MAKESIG(a,b,c,d)        ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define CODEVIEW_NB09_SIG       MAKESIG('N','B','0','9')
#define CODEVIEW_NB10_SIG       MAKESIG('N','B','1','0')
#define CODEVIEW_NB11_SIG       MAKESIG('N','B','1','1')
#define CODEVIEW_RSDS_SIG       MAKESIG('R','S','D','S')

static BOOL codeview_process_info(const struct process* pcs, 
                                  const struct msc_debug_info* msc_dbg)
{
    const DWORD*                signature = (const DWORD*)msc_dbg->root;
    BOOL                        ret = FALSE;
    struct pdb_lookup           pdb_lookup;

    TRACE("Processing signature %.4s\n", (const char*)signature);

    switch (*signature)
    {
    case CODEVIEW_NB09_SIG:
    case CODEVIEW_NB11_SIG:
    {
        const OMFSignature*     cv = (const OMFSignature*)msc_dbg->root;
        const OMFDirHeader*     hdr = (const OMFDirHeader*)(msc_dbg->root + cv->filepos);
        const OMFDirEntry*      ent;
        const OMFDirEntry*      prev;
        const OMFDirEntry*      next;
        unsigned int                    i;


        TRACE("found %.4s type of PDB file {filepos = %ld}\n", (const char *)signature, cv->filepos);

        codeview_init_basic_types(msc_dbg->module);

        for (i = 0; i < hdr->cDir; i++)
        {
            ent = (const OMFDirEntry*)((const BYTE*)hdr + hdr->cbDirHeader + i * hdr->cbDirEntry);
            if (ent->SubSection == sstGlobalTypes)
            {
                const OMFGlobalTypes*           types;
                struct codeview_type_parse      ctp;

                types = (const OMFGlobalTypes*)(msc_dbg->root + ent->lfo);
                ctp.module = msc_dbg->module;
                ctp.offset = (const DWORD*)(types + 1);
                ctp.num    = types->cTypes;
                ctp.first  = FIRST_DEFINABLE_TYPE;
                ctp.table  = (const BYTE*)(ctp.offset + types->cTypes);

                cv_current_module = &cv_zmodules[0];
                if (cv_current_module->allowed) FIXME("Already allowed ??\n");
                cv_current_module->allowed = TRUE;

                codeview_parse_type_table(&ctp);
                break;
            }
        }

        ent = (const OMFDirEntry*)((const BYTE*)hdr + hdr->cbDirHeader);
        for (i = 0; i < hdr->cDir; i++, ent = next)
        {
            next = (i == hdr->cDir-1) ? NULL :
                   (const OMFDirEntry*)((const BYTE*)ent + hdr->cbDirEntry);
            prev = (i == 0) ? NULL :
                   (const OMFDirEntry*)((const BYTE*)ent - hdr->cbDirEntry);

            if (ent->SubSection == sstAlignSym)
            {
                /*
                 * Check the next and previous entry.  If either is a
                 * sstSrcModule, it contains the line number info for
                 * this file.
                 *
                 * FIXME: This is not a general solution!
                 */
                struct codeview_linetab*        linetab = NULL;

                if (next && next->iMod == ent->iMod && 
                    next->SubSection == sstSrcModule)
                    linetab = codeview_snarf_linetab(msc_dbg->module, 
                                                     msc_dbg->root + next->lfo, next->cb, 
                                                     TRUE);

                if (prev && prev->iMod == ent->iMod &&
                    prev->SubSection == sstSrcModule)
                    linetab = codeview_snarf_linetab(msc_dbg->module, 
                                                     msc_dbg->root + prev->lfo, prev->cb, 
                                                     TRUE);

                codeview_snarf(msc_dbg, msc_dbg->root + ent->lfo, sizeof(DWORD),
                               ent->cb, linetab);
            }
        }

        msc_dbg->module->module.SymType = SymCv;
        /* FIXME: we could have a finer grain here */
        msc_dbg->module->module.LineNumbers = TRUE;
        msc_dbg->module->module.GlobalSymbols = TRUE;
        msc_dbg->module->module.TypeInfo = TRUE;
        msc_dbg->module->module.SourceIndexed = TRUE;
        msc_dbg->module->module.Publics = TRUE;
        codeview_clear_type_table();
        ret = TRUE;
        break;
    }

    case CODEVIEW_NB10_SIG:
    {
        const CODEVIEW_PDB_DATA* pdb = (const CODEVIEW_PDB_DATA*)msc_dbg->root;

        TRACE("found NB10 type of PDB file {filePos = %ld, timestamp = %u, unknown = %u, name = '%s'}\n",
                pdb->filepos, pdb->timestamp, pdb->unknown, pdb->name);

        pdb_lookup.filename = pdb->name;
        pdb_lookup.kind = PDB_JG;
        pdb_lookup.u.jg.timestamp = pdb->timestamp;
        pdb_lookup.u.jg.toc = NULL;
        pdb_lookup.age = pdb->unknown;
        ret = pdb_process_file(pcs, msc_dbg, &pdb_lookup);
        break;
    }
    case CODEVIEW_RSDS_SIG:
    {
        const OMFSignatureRSDS* rsds = (const OMFSignatureRSDS*)msc_dbg->root;

        TRACE("Got RSDS type of PDB file: guid=%s unk=%08x name=%s\n",
              wine_dbgstr_guid(&rsds->guid), rsds->unknown, rsds->name);
        pdb_lookup.filename = rsds->name;
        pdb_lookup.kind = PDB_DS;
        pdb_lookup.u.ds.guid = rsds->guid;
        pdb_lookup.u.ds.toc = NULL;
        pdb_lookup.age = rsds->unknown;
        ret = pdb_process_file(pcs, msc_dbg, &pdb_lookup);
        break;
    }
    default:
        ERR("Unknown CODEVIEW signature %08x in module %s\n",
            *signature, debugstr_w(msc_dbg->module->module.ModuleName));
        break;
    }
    if (ret)
    {
        msc_dbg->module->module.CVSig = *signature;
        memcpy(msc_dbg->module->module.CVData, msc_dbg->root,
               sizeof(msc_dbg->module->module.CVData));
    }
    return ret;
}

/*========================================================================
 * Process debug directory.
 */
BOOL pe_load_debug_directory(const struct process* pcs, struct module* module, 
                             const BYTE* mapping,
                             const IMAGE_SECTION_HEADER* sectp, DWORD nsect,
                             const IMAGE_DEBUG_DIRECTORY* dbg, int nDbg)
{
    BOOL                        ret;
    int                         i;
    struct msc_debug_info       msc_dbg;


    msc_dbg.module = module;
    msc_dbg.nsect  = nsect;
    msc_dbg.sectp  = sectp;
    msc_dbg.nomap  = 0;
    msc_dbg.omapp  = NULL;

    __TRY
    {
        ret = FALSE;


        TRACE("{pcs = %p, module = %p, mapping = %p, sectp = %p, nsect = %u, dbg = %p, nDbg = %d}\n",
                pcs, module, mapping, sectp, nsect, dbg, nDbg);

        for (i = 0; i < nDbg; i++){
            TRACE("debugDirectory[%d]:\n"
                  "     Characteristics =   0x%08x\n"
                  "     TimeDateStamp =     %u\n"
                  "     MajorVersion =      %d\n"
                  "     MinorVersion =      %d\n"
                  "     Type =              %u\n"
                  "     SizeOfData =        %u\n"
                  "     AddressOfRawData =  0x%08x\n"
                  "     PointerToRawData =  0x%08x\n",
                  i,
                  dbg[i].Characteristics,
                  dbg[i].TimeDateStamp,
                  dbg[i].MajorVersion,
                  dbg[i].MinorVersion,
                  dbg[i].Type,
                  dbg[i].SizeOfData,
                  dbg[i].AddressOfRawData,
                  dbg[i].PointerToRawData);
        }



        /* First, watch out for OMAP data */
        TRACE("checking for OMAP data\n");
        for (i = 0; i < nDbg; i++)
        {
            if (dbg[i].Type == IMAGE_DEBUG_TYPE_OMAP_FROM_SRC)
            {
                TRACE("found OMAP data in directory entry %d\n", i);

                msc_dbg.nomap = dbg[i].SizeOfData / sizeof(OMAP_DATA);
                msc_dbg.omapp = (const OMAP_DATA*)(mapping + dbg[i].PointerToRawData);
                break;
            }
        }
  
        /* Now, try to parse CodeView debug info */
        TRACE("checking for codeview debug info\n");
        for (i = 0; i < nDbg; i++)
        {
            if (dbg[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW)
            {
                TRACE("found codeview debug info in directory entry %d\n", i);

                msc_dbg.root = mapping + dbg[i].PointerToRawData;
                if ((ret = codeview_process_info(pcs, &msc_dbg))) goto done;
            }
        }
    
        /* If not found, try to parse COFF debug info */
        TRACE("checking for COFF debug info\n");
        for (i = 0; i < nDbg; i++)
        {
            if (dbg[i].Type == IMAGE_DEBUG_TYPE_COFF)
            {
                TRACE("found COFF debug info in directory entry %d\n", i);

                msc_dbg.root = mapping + dbg[i].PointerToRawData;
                if ((ret = coff_process_info(&msc_dbg))) goto done;
            }
        }

        TRACE("no other supported debug info!\n");
    done:
	 /* FIXME: this should be supported... this is the debug information for
	  * functions compiled without a frame pointer (FPO = frame pointer omission)
	  * the associated data helps finding out the relevant information
	  */
        for (i = 0; i < nDbg; i++)
            if (dbg[i].Type == IMAGE_DEBUG_TYPE_FPO)
                FIXME("This guy has FPO information\n");
#if 0

#define FRAME_FPO   0
#define FRAME_TRAP  1
#define FRAME_TSS   2

typedef struct _FPO_DATA 
{
	DWORD       ulOffStart;            /* offset 1st byte of function code */
	DWORD       cbProcSize;            /* # bytes in function */
	DWORD       cdwLocals;             /* # bytes in locals/4 */
	WORD        cdwParams;             /* # bytes in params/4 */

	WORD        cbProlog : 8;          /* # bytes in prolog */
	WORD        cbRegs   : 3;          /* # regs saved */
	WORD        fHasSEH  : 1;          /* TRUE if SEH in func */
	WORD        fUseBP   : 1;          /* TRUE if EBP has been allocated */
	WORD        reserved : 1;          /* reserved for future use */
	WORD        cbFrame  : 2;          /* frame type */
} FPO_DATA;
#endif

    }
    __EXCEPT_PAGE_FAULT
    {
        ERR("Got a page fault while loading symbols\n");
        ret = FALSE;
    }
    __ENDTRY
    return ret;
}
