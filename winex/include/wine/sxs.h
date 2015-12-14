#ifndef SXS_H
#define SXS_H

/*
 * Side-by-Side Assemblies
 *
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

/* SXS structures and functions visible to other parts of
   NTDLL.

   These are not Windows-defined structures.

   Concern is for assembly components that are most used in application
   manifests, with an eye to extending to assembly manifests.

   SXS_ASSEMBLY is the root object.
*/

/* The following structures are analogues to the schema of an
   assembly file. */

typedef struct SXS_progid
{
    struct SXS_progid *next;
    char *id;
} SXS_PROGID;

typedef struct SXS_comClass
{
    struct SXS_comClass *next;
    char *clsid;
    char *threadingModel;
    char *tlbid;
    char *description;
    /* ignore misc fields for now */
    SXS_PROGID *progid, *progidLast;
} SXS_COM_CLASS;

typedef struct SXS_file
{
    char *name;
    char *hashalg;
    char *hash;
    SXS_COM_CLASS *comClass, *comClassLast;
} SXS_FILE;

typedef struct SXS_assemblyIdentity
{
    char *type;
    char *name;
    char *language;
    char *processorArchitecture;
    char *version;
    char *publicKeyToken;
} SXS_ASSEMBLY_IDENTITY;

typedef struct SXS_bindingRedirect
{
    struct SXS_bindingRedirect *next;
    char *oldVersion;
    char *newVersion;
} SXS_BINDING_REDIRECT;

typedef struct SXS_dependentAssembly
{
    struct SXS_dependentAssembly *next;
    SXS_ASSEMBLY_IDENTITY *identity;
    SXS_BINDING_REDIRECT *redirect, *redirectLast;
} SXS_DEPENDENT_ASSEMBLY;

typedef struct SXS_assembly
{
    char *manifestVersion;
    int noInherit;
    int noInheritable;
    SXS_ASSEMBLY_IDENTITY *identity;
    SXS_DEPENDENT_ASSEMBLY *dependencies, *dependenciesLast;
    SXS_FILE *file;
} SXS_ASSEMBLY;

/* functions visible to other modules */

/* Read a raw manifest file */
extern BOOL SXS_ReadAssemblyFileA(LPCSTR filename, SXS_ASSEMBLY **);
/* Extract manifest resource from a loaded/opened module */
extern BOOL SXS_ReadAssemblyModule(HMODULE, SXS_ASSEMBLY **);
/* Free a previously retrieved assembly */
extern void SXS_FreeAssembly(SXS_ASSEMBLY *);

/* SXS searching and location

   The goal is to isolate as many of the details about how SXS
   actually works and is organized within this module.

   manifest is the application manifest (likely retrieved via
   SXS_ReadAssemblyModule()).
*/

/* Populate the SXS cache from an application manifest */
extern BOOL SXS_FillCacheFromManifest(SXS_ASSEMBLY *manifest);

/* Populate the SXS cache from an application's module handle */
extern BOOL SXS_FillCacheFromModule(HMODULE);

/* Empty the in-memory SXS cache */
extern void SXS_EmptyCache(void);

/*
   Returns true if the DLL was located (with path returned in filename).
   filename should have space for MAX_PATH chars plus a terminating null.
*/

extern BOOL SXS_LocateModuleA(LPCSTR libname, LPCSTR filename);

extern void SXS_Init(void);

#endif /* SXS_H */
