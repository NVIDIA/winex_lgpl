/*
 *  TG Types ('tg_types.h')
 *
 *  Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 *
 *  contains type definitions for common types.  The types in this file are intended
 *  to be used for functions that must be shared between windows and mac code but
 *  need data consistent types.  It is also to provide types to replace those that
 *  are defined differently between mac & windows headers (ie: BOOL).
 *
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#ifndef  __TG_TYPES_H__
# define __TG_TYPES_H__

#define BEGINNAMEMAP(name, prefix)      { size_t _prefixLength = sizeof(#prefix) - 1; switch (name) {
#define BEGINNAMEMAPNOPREFIX(name)      { size_t _prefixLength = 0; switch (name) {
#define GETNAME(value)                  case value: return &(#value)[_prefixLength]
#define ENDNAMEMAP(errMsg)              default: return errMsg; } }

#endif
