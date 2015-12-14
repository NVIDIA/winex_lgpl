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

/*
  This is a minimal implementation of support for the WinSxS (side-by-side)
  assembly model.

  This package exposes a number of functions for internal consumption.
  (Actual Windows APIs provide "activation contexts".)

  Some Windows details are (surprise, surprise) undocumented.

  The code below is roughly split into two parts.
  The first part deals primarily with loading and parsing assembly, 
  application and policy manifests.

  The second part deals with searching through the WinSxS area and
  picking paths in which to search for DLLs.

  (Note that the entire schema of the manifest is not implemented, but
  unsupported tags should be skipped over by the processor
  and a FIXME generated.)

*/
#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "winbase.h"
#include "winuser.h"
#include "winternl.h"
#include "wine/file.h"
#include "wine/debug.h"
#include "wine/sxs.h"

WINE_DEFAULT_DEBUG_CHANNEL(module);

/* MEX - Miniature Embeddable XML parser 
   - MEX gets included in this file so its symbols are completely private
 */
#define MEX_EMBEDDED 1
#define MEX_NTDLL 1
#include "mex.h"
#include "mex.c"

/* We need to guard the parser and the cache */
static RTL_CRITICAL_SECTION sxs_parser_critsect;
static RTL_CRITICAL_SECTION sxs_cache_critsect;

#define STREQ(x,y) (((x) == NULL && (y) == NULL)||((x)&&(y)&&strcmp(x,y) == 0))


/* Check for a supported UTF marker.  If present (and supported),
   strip it and return TRUE. */
static BOOL SXS_CheckBOM(char **bufp, int *sizep)
{
    unsigned char *buf = (unsigned char *)*bufp;
    int size = *sizep;

    if (size > 0 && buf[0] == '<')
        return TRUE; /* looks like a straight-up XML document */

    if (size >= 4) {
        if (buf[0] == 0x00 &&
            buf[1] == 0x00 &&
            buf[2] == 0xfe &&
            buf[3] == 0xff)
        {
            /* UTF-32, BE */
            FIXME("unsupported UTF-32 big-endian encoding in manifest\n");
            (*bufp) += 4;
            (*sizep) -= 4;
            return FALSE;
        }
        if (buf[0] == 0xff &&
            buf[1] == 0xfe &&
            buf[2] == 0x00 &&
            buf[3] == 0x00)
        {
            /* UTF-32, LE */
            FIXME("unsupported UTF-32 little-endian encoding in manifest\n");
            (*bufp) += 4;
            (*sizep) += 4;
            return FALSE;
        }
    }
    if (size >= 2) {
        if (buf[0] == 0xfe &&
            buf[1] == 0xff)
        {
            /* UTF-16, BE */
            FIXME("unsupported UTF-16 big-endian encoding in manifest\n");
            (*bufp) += 2;
            (*sizep) -= 2;
            return FALSE;
        }
        if (buf[0] == 0xff &&
            buf[1] == 0xfe)
        {
            /* UTF-16, LE */
            FIXME("unsupported UTF-16 little-endian encoding in manifest\n");
            (*bufp) += 2;
            (*sizep) -= 2;
            return FALSE;
        }
    }
    if (size >= 3) {
        if (buf[0] == 0xef &&
            buf[1] == 0xbb &&
            buf[2] == 0xbf)
        {
            /* UTF-8 */
            (*bufp) += 3;
            (*sizep) -= 3;
            return TRUE;
        }
    }
    ERR("unrecognized characters at beginning of manifest\n");
    return FALSE;
}

static inline char *SXS_Strdup(char *str, int len)
{
    char *dup;
    if (len < 0)
        len = (str ? strlen(str) : -1);
    dup = RtlAllocateHeap(GetProcessHeap(),0,len+1);
    if (!dup)
        return NULL;
    memcpy(dup,str,len);
    dup[len] = '\0';
    return dup;
}

/* Safely store a copy of a string, ensuring we remove any previously
   allocated entry */
static inline int SXS_SetString(char **strp, char *newstr, int newlen)
{
    char *nstr;
    if (!strp)
        return -1; /* what were you thinking? */
    if (!(nstr = SXS_Strdup(newstr,newlen)))
        return -1;
    RtlFreeHeap(GetProcessHeap(),0,*strp);
    *strp = nstr;
    return 0;
}

static void SXS_FreeProgid(SXS_PROGID *progid)
{
    if (progid)
    {
        RtlFreeHeap(GetProcessHeap(),0,progid->id);
        RtlFreeHeap(GetProcessHeap(),0,progid);
    }
}

static void SXS_FreeComClass(SXS_COM_CLASS *comclass)
{
    if (comclass)
    {
        RtlFreeHeap(GetProcessHeap(),0,comclass->clsid);
        RtlFreeHeap(GetProcessHeap(),0,comclass->threadingModel);
        RtlFreeHeap(GetProcessHeap(),0,comclass->tlbid);
        RtlFreeHeap(GetProcessHeap(),0,comclass->description);
        {
            SXS_PROGID *p, *pnext;
            p = comclass->progid;
            while (p)
            {
                pnext = p->next;
                SXS_FreeProgid(p);
                p = pnext;
            }
        }
        RtlFreeHeap(GetProcessHeap(),0,comclass);
    }
}

static void SXS_FreeBindingRedirect(SXS_BINDING_REDIRECT *redirect)
{
    if (redirect)
    {
        RtlFreeHeap(GetProcessHeap(),0,redirect->oldVersion);
        RtlFreeHeap(GetProcessHeap(),0,redirect->newVersion);
        RtlFreeHeap(GetProcessHeap(),0,redirect);
    }
}

static void SXS_FreeAssemblyIdentity(SXS_ASSEMBLY_IDENTITY *id)
{
    if (id)
    {
        RtlFreeHeap(GetProcessHeap(),0,id->type);
        RtlFreeHeap(GetProcessHeap(),0,id->name);
        RtlFreeHeap(GetProcessHeap(),0,id->language);
        RtlFreeHeap(GetProcessHeap(),0,id->processorArchitecture);
        RtlFreeHeap(GetProcessHeap(),0,id->version);
        RtlFreeHeap(GetProcessHeap(),0,id->publicKeyToken);
        RtlFreeHeap(GetProcessHeap(),0,id);
    }
}

static void SXS_FreeDependentAssembly(SXS_DEPENDENT_ASSEMBLY *dep)
{
    if (dep)
    {
        if (dep->identity)
            SXS_FreeAssemblyIdentity(dep->identity);
        {
            SXS_BINDING_REDIRECT *rd, *rdnext;
            rd = dep->redirect;
            while (rd)
            {
                rdnext = rd->next;
                SXS_FreeBindingRedirect(rd);
                rd = rdnext;
            }
        }
        RtlFreeHeap(GetProcessHeap(),0,dep);
    }
}

static void SXS_FreeFile(SXS_FILE *file)
{
    if (file)
    {
        RtlFreeHeap(GetProcessHeap(),0,file->name);
        RtlFreeHeap(GetProcessHeap(),0,file->hashalg);
        RtlFreeHeap(GetProcessHeap(),0,file->hash);
        {
            SXS_COM_CLASS *cc, *ccnext;
            cc = file->comClass;
            while (cc)
            {
                ccnext = cc->next;
                SXS_FreeComClass(cc);
                cc = ccnext;
            }
        }
        RtlFreeHeap(GetProcessHeap(),0,file);
    }
}

void SXS_FreeAssembly(SXS_ASSEMBLY *assembly)
{
    if (assembly)
    {
        RtlFreeHeap(GetProcessHeap(),0,assembly->manifestVersion);
        if (assembly->identity)
            SXS_FreeAssemblyIdentity(assembly->identity);
        {
            SXS_DEPENDENT_ASSEMBLY *dep, *depnext;
            dep = assembly->dependencies;
            while (dep)
            {
                depnext = dep->next;
                SXS_FreeDependentAssembly(dep);
                dep = depnext;
            }
        }
        if (assembly->file)
            SXS_FreeFile(assembly->file);
        RtlFreeHeap(GetProcessHeap(),0,assembly);
    }
}

static int SXS_ComClassAddProgid(SXS_COM_CLASS *comclass,
                                 char *progid)
{
    SXS_PROGID *p;
    if (!(p = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_PROGID))))
    {
        ERR("memory allocation failure\n");
        return -1;
    }
    if (SXS_SetString(&p->id,progid,-1))
    {
        ERR("memory allocation failure\n");
        return -1;
    }
    p->next = NULL;
    if (comclass->progidLast)
        comclass->progidLast->next = p;
    else
        comclass->progid = p;
    comclass->progidLast = p;
    return 0;
}

static SXS_COM_CLASS *SXS_ComClassFromNode(MEXNode *node)
{
    SXS_COM_CLASS *comclass = NULL;
    char *value;

    if (!node || node->type != MEX_TAG || 
        !STREQ(node->data.tag.name,"comClass"))
    {
        ERR("invalid comClass node\n");
        goto fail;
    }

    if (!(comclass = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_COM_CLASS))))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }

    if (!(value = MEXNodeGetAttr(node,NULL,"clsid")))
    {
        ERR("invalid comClass - missing clsid\n");
        goto fail;
    }
    if (SXS_SetString(&comclass->clsid,value,-1))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    if ((value = MEXNodeGetAttr(node,NULL,"description")))
    {
        if (SXS_SetString(&comclass->description,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"threadingModel")))
    {
        if (SXS_SetString(&comclass->threadingModel,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"tlbid")))
    {
        if (SXS_SetString(&comclass->tlbid,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    /* progid can be specified in two ways:
       - as an attribute
       - as children "progid" nodes
    */
    if ((value = MEXNodeGetAttr(node,NULL,"progid")))
    {
        if (SXS_ComClassAddProgid(comclass,value))
        {
            ERR("comclass - failed to add progid");
            goto fail;
        }
    }
    {
        MEXNode *subnode;
        for (subnode = node->data.tag.childFirst;
             subnode;
             subnode = subnode->next)
        {
            if (subnode->type == MEX_TAG &&
                STREQ(subnode->data.tag.name,"progid"))
            {
                MEXNode *cdata;
                if (!(cdata = MEXNodeContent(subnode)))
                {
                    ERR("progid node has no content\n");
                    /* ignore */
                }
                else
                {
                    assert(cdata->type == MEX_CDATA);
                    if (SXS_ComClassAddProgid(comclass,
                                              cdata->data.cdata.buf))
                    {
                        ERR("comclass - failed to add progid");
                        goto fail;
                    }
                }
            }
        }
    }
    return comclass;

fail:
    SXS_FreeComClass(comclass);
    return NULL;
}

static SXS_ASSEMBLY_IDENTITY *SXS_AssemblyIdentityFromNode(MEXNode *node)
{
    SXS_ASSEMBLY_IDENTITY *identity = NULL;
    char *value;

    if (!node || node->type != MEX_TAG || 
        !STREQ(node->data.tag.name,"assemblyIdentity"))
    {
        ERR("invalid assemblyIdentity node\n");
        goto fail;
    }

    if (!(identity = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_ASSEMBLY_IDENTITY))))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    
    /* required values */
    if (!(value = MEXNodeGetAttr(node,NULL,"name")))
    { 
        ERR("assemblyIdentity missing name attribute\n");
        goto fail; /* missing name */
    }
    if (SXS_SetString(&identity->name,value,-1))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    if (!(value = MEXNodeGetAttr(node,NULL,"type")))
    {
        ERR("assemblyIdentity missing type attribute\n");
        goto fail;
    }
    if (SXS_SetString(&identity->type,value,-1))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    /* version is not required (apparently) in a policy... */
    if ((value = MEXNodeGetAttr(node,NULL,"version")))
    {
        if (SXS_SetString(&identity->version,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"language")))
    {
        if (SXS_SetString(&identity->language,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"processorArchitecture")))
    {
        if (SXS_SetString(&identity->processorArchitecture,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"publicKeyToken")))
    {
        if (SXS_SetString(&identity->publicKeyToken,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    
    return identity;

fail:
    SXS_FreeAssemblyIdentity(identity);
    return NULL;
}

static SXS_BINDING_REDIRECT *SXS_BindingRedirectFromNode(MEXNode *node)
{
    SXS_BINDING_REDIRECT *redirect = NULL;
    char *value;

    if (!node || node->type != MEX_TAG ||
        !STREQ(node->data.tag.name,"bindingRedirect"))
    {
        ERR("invalid bindingRedirect node\n");
        goto fail;
    }
    
    if (!(redirect = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_BINDING_REDIRECT))))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    if (!(value = MEXNodeGetAttr(node,NULL,"oldVersion")))
    {
        ERR("invalid bindingRedirect - oldVersion is missing\n");
        goto fail;
    }
    if (SXS_SetString(&redirect->oldVersion,value,-1))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    if (!(value = MEXNodeGetAttr(node,NULL,"newVersion")))
    {
        ERR("invalid bindingRedirect - newVersion is missing\n");
        goto fail;
    }
    if (SXS_SetString(&redirect->newVersion,value,-1))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    return redirect;

fail:
    SXS_FreeBindingRedirect(redirect);
    return NULL;
}

static SXS_DEPENDENT_ASSEMBLY *SXS_DependentAssemblyFromNode(MEXNode *node)
{
    SXS_DEPENDENT_ASSEMBLY *dep = NULL;
    MEXNode *subnode;

    if (!node || node->type != MEX_TAG ||
        !STREQ(node->data.tag.name,"dependentAssembly"))
    {
        ERR("invalid dependentAssembly node\n");
        goto fail;
    }
    
    if (!(dep = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_DEPENDENT_ASSEMBLY))))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }

    for (subnode = node->data.tag.childFirst;
         subnode;
         subnode = subnode->next)
    {
        if (subnode->type != MEX_TAG)
            continue;
        if (STREQ(subnode->data.tag.name,"assemblyIdentity"))
        {
            if (dep->identity)
            {
                ERR("dependentAssembly has multiple assemblyIdentity tags\n");
                /* ignore */
            }
            else
            {
                if (!(dep->identity = SXS_AssemblyIdentityFromNode(subnode)))
                {
                    ERR("failed to parse assemblyIdentity node\n");
                    goto fail;
                }
            }
        }
        else if (STREQ(subnode->data.tag.name,"bindingRedirect"))
        {
            SXS_BINDING_REDIRECT *redirect;
            if (!(redirect = SXS_BindingRedirectFromNode(subnode)))
            {
                ERR("failed to parse bindingRedirect node\n");
                goto fail;
            }
            /* add to end of list */
            redirect->next = NULL;
            if (dep->redirectLast)
                dep->redirectLast->next = redirect;
            else
                dep->redirect = redirect;
            dep->redirectLast = redirect;
            
        }
        else
        {
            FIXME("unsupported tag in dependentAssembly: %s\n",
                  subnode->data.tag.name);
        }
    }
    return dep;

fail:
    SXS_FreeDependentAssembly(dep);
    return NULL;
}

static SXS_FILE *SXS_FileFromNode(MEXNode *node)
{
    SXS_FILE *file = NULL;
    char *value;

    if (!node || node->type != MEX_TAG || 
        !STREQ(node->data.tag.name,"file"))
    {
        ERR("invalid file node\n");
        goto fail;
    }

    if (!(file = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_FILE))))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }

    /* all values are optional */
    if ((value = MEXNodeGetAttr(node,NULL,"name")))
    {
        if (SXS_SetString(&file->name,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"hashalg")))
    {
        if (SXS_SetString(&file->hashalg,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }
    if ((value = MEXNodeGetAttr(node,NULL,"hash")))
    {
        if (SXS_SetString(&file->hash,value,-1))
        {
            ERR("memory allocation failure\n");
            goto fail;
        }
    }

    {
        MEXNode *subnode;
        for (subnode = node->data.tag.childFirst;
             subnode;
             subnode = subnode->next)
        {
            if (subnode->type != MEX_TAG)
                continue;
            if (STREQ(subnode->data.tag.name,"comClass"))
            {
                SXS_COM_CLASS *comclass;
                if (!(comclass = SXS_ComClassFromNode(subnode)))
                {
                    ERR("failed to parse comClass node");
                    goto fail;
                }
                /* add to end of list */
                comclass->next = NULL;
                if (file->comClassLast)
                    file->comClassLast->next = comclass;
                else
                    file->comClass = comclass;
                file->comClassLast = comclass;
            }
            else
            {
                FIXME("unsupported tag in file: %s",subnode->data.tag.name);
                /* ignore */
            }
        }
    }

    return file;

fail:
    SXS_FreeFile(file);
    return NULL;
}

static SXS_ASSEMBLY *SXS_AssemblyFromDoc(MEXDocument *doc)
{
    SXS_ASSEMBLY *assembly = NULL;
    char *value;
    /* ignore namespace */
    if (!doc || !doc->root)
    {
        ERR("attempt to build assembly from invalid document\n");
        goto fail;
    }
    if (doc->root->type != MEX_TAG)
    {
        ERR("invalid document - root is not a tag\n");
        goto fail;
    }
    if (!STREQ(doc->root->data.tag.name,"assembly"))
    {
        ERR("invalid document - root is not assembly: %s\n",
            doc->root->data.tag.name);
        goto fail; /* not an assembly document */
    }
    if (!(value = MEXNodeGetAttr(doc->root,NULL,"manifestVersion")))
    {
        ERR("invalid document - manifestVersion is missing\n");
        goto fail; /* manifestVersion is required */
    }
    if (!(STREQ(value,"1.0")))
    {
        ERR("invalid document - unsupported manifestVersion: %s\n",
            value);
        goto fail; /* unsupported manifestVersion */
    }
    
    assembly = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_ASSEMBLY));
    if (!assembly)
    {
        ERR("memory allocation failure\n");
        goto fail; /* memory allocation failure */
    }
    if (SXS_SetString(&assembly->manifestVersion,value,-1))
    {
        ERR("memory allocation failure\n");
        goto fail;
    }
    
    {
        MEXNode *node;
        for (node = doc->root->data.tag.childFirst;
             node;
             node = node->next)
        {
            if (node->type != MEX_TAG)
                continue; /* ignore */
            if (STREQ(node->data.tag.name,"noInherit"))
                assembly->noInherit = 1;
            else if (STREQ(node->data.tag.name,"noInheritable"))
                assembly->noInheritable = 1;
            else if (STREQ(node->data.tag.name,"assemblyIdentity"))
            {
                if (assembly->identity)
                {
                    ERR("assembly has multiple assemblyIdentity entries\n");
                    /* ignore */
                }
                else if (!(assembly->identity = SXS_AssemblyIdentityFromNode(node)))
                {
                    ERR("failed to parse assemblyIdentity node\n");
                    goto fail;
                }
            }
            else if (STREQ(node->data.tag.name,"dependency"))
            {
                /* look for dependentAssemblies */
                MEXNode *subnode;
                for (subnode = node->data.tag.childFirst;
                     subnode;
                     subnode = subnode->next)
                {
                    if (subnode->type != MEX_TAG)
                        continue;
                    if (STREQ(subnode->data.tag.name,"dependentAssembly"))
                    {
                        SXS_DEPENDENT_ASSEMBLY *dep;
                        if (!(dep = SXS_DependentAssemblyFromNode(subnode)))
                        {
                            ERR("failed to parse dependentAssembly node\n");
                            goto fail;
                        }
                        dep->next = NULL;
                        /* add to list */
                        if (assembly->dependenciesLast)
                            assembly->dependenciesLast->next = dep;
                        else
                            assembly->dependencies = dep;
                        assembly->dependenciesLast = dep;
                    }
                    else
                    {
                        FIXME("unsupported tag in dependency: %s\n",
                              subnode->data.tag.name);
                    }
                }
            }
            else if (STREQ(node->data.tag.name,"file"))
            {
                if (assembly->file)
                {
                    ERR("assembly has multiple file tags\n");
                    /* ignore */
                }
                else if (!(assembly->file = SXS_FileFromNode(node)))
                {
                    ERR("failed to parse file node\n");
                    goto fail;
                }
            }
            else
            {
                FIXME("unsupported node in assembly: %s\n",
                      node->data.tag.name);
            }
        }
    }

    return assembly;

fail:
    SXS_FreeAssembly(assembly);
    return NULL;
}

/*
 * Loading manifests/assemblies:
 *
 * There are two places that manifests/assemblies come from:
 *
 * - standalone XML files
 * - as resource id 1, type RT_MANIFEST (24)
 */

#define READSZ 1024
BOOL SXS_ReadAssemblyFileA(LPCSTR path, SXS_ASSEMBLY **assembly_r)
{
    HANDLE hfile;
    char buf[READSZ];
    MEXStringBuf sb;
    DWORD bytesRead;
    BOOL readResult;

    hfile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING, 0, 0);
    if (hfile == INVALID_HANDLE_VALUE)
    {
        TRACE("failed to open %s\n",path);
        return FALSE;
    }

    MEXStringBufStart(&sb);
    while ((readResult = ReadFile(hfile,buf,READSZ,&bytesRead,NULL)) && 
           bytesRead > 0)
        MEXStringBufAddString(&sb,buf,bytesRead);
    if (!readResult)
    {
        ERR("failed while reading %s: error=%d\n", path, (int)GetLastError());
        CloseHandle(hfile);
        return FALSE;
    }
        
    CloseHandle(hfile);

    TRACE("parsing %s\n",path);
    {
        /* Now try parsing it */
        MEXDocument *doc;
        SXS_ASSEMBLY *assembly;
        char *resbuf = sb.buf;
        int ressz = sb.use;
        
        /* We are going to ignore the result of CheckBOM for now, and
           proceed anyways - worst case is that the XML parser bombs out,
           but the error message from CheckBOM should tell us why. */
        (void) SXS_CheckBOM(&resbuf,&ressz);

        RtlEnterCriticalSection(&sxs_parser_critsect);
        doc = MEXDocumentParse(resbuf,ressz,0);
        /* FIXME: the MEX parser is thread-safe,
           but the error code is not... */
        if (!doc) {
            ERR("failed to parse assembly: MEX error=%d\n",MEXGetError());
            RtlLeaveCriticalSection(&sxs_parser_critsect);
            MEXStringBufEnd(&sb);
            return FALSE;
        }
        RtlLeaveCriticalSection(&sxs_parser_critsect);
        if (!(assembly = SXS_AssemblyFromDoc(doc)))
        {
            MEXDocumentFree(doc);
            MEXStringBufEnd(&sb);
            return FALSE;
        }
        /* success */
        MEXDocumentFree(doc);
        if (assembly_r)
            *assembly_r = assembly;
        else
            SXS_FreeAssembly(assembly);
    }
    MEXStringBufEnd(&sb);
    return TRUE;
}

BOOL SXS_ReadAssemblyModule(HMODULE module, SXS_ASSEMBLY **assembly_r)
{
    /* First, we need to read the resource */
    HRSRC res, resLoad;
    res = FindResourceA(module,(LPCSTR)1,RT_MANIFESTA);
    if (!res)
    {
        /* This is okay - it just means we do not have an assembly */
        if (assembly_r)
            *assembly_r = NULL;
        return TRUE;
    }
    resLoad = LoadResource(module,res);
    if (!resLoad)
    {
        ERR("found assembly but unable to load: %d\n",
            (int)GetLastError());
        return FALSE;
    }
    {
        /* Now try parsing it */
        MEXDocument *doc;
        SXS_ASSEMBLY *assembly;
        char *resbuf = LockResource(resLoad);
        int ressz = SizeofResource(module,res);

        /* We are going to ignore the result of CheckBOM for now, and
           proceed anyways - worst case is that the XML parser bombs out,
           but the error message from CheckBOM should tell us why. */
        (void) SXS_CheckBOM(&resbuf,&ressz);
        
        /* Older code to check for junk at the beginning of the manifest.
           Left here to squawk if we encounter trouble. */
        if (ressz > 0 && resbuf[0] != '<') {
            /* Observed: manifest resource with junk before the actual
               manifest (a few bytes).  Is this a resource retrieval
               issue or another bug? 
               
               To help work around this, we'll search ahead for an
               angle bracket...
            */
            int compensation = 0;
            TRACE("suspicious manifest - corrupt? - compensating\n");
            while (ressz > 0 && *resbuf != '<')
            {
                resbuf++;
                compensation++;
                ressz--;
            }
            if (ressz == 0)
            {
                ERR("corrupt manifest - could not compensate\n");
            }
            else
            {
                ERR("compensated by %d bytes for corrupt manifest\n",
                    compensation);
            }
        }
        RtlEnterCriticalSection(&sxs_parser_critsect);
        doc = MEXDocumentParse(resbuf,ressz,0);
        /* FIXME: the MEX parser is thread-safe,
           but the error code is not... */
        if (!doc) {
            ERR("failed to parse assembly: MEX error=%d\n",MEXGetError());
            RtlLeaveCriticalSection(&sxs_parser_critsect);
            return FALSE;
        }
        RtlLeaveCriticalSection(&sxs_parser_critsect);
        if (!(assembly = SXS_AssemblyFromDoc(doc)))
        {
            MEXDocumentFree(doc);
            return FALSE;
        }
        /* success */
        MEXDocumentFree(doc);
        if (assembly_r)
            *assembly_r = assembly;
        else
            SXS_FreeAssembly(assembly);
    }
    return TRUE;
}

/*
 * Searching for DLLs in the SxS (side-by-side) area:
 *
 * The WinSxS directory has the following structure:
 * - located in c:/windows/WinSxS/
 *
 * Manifests/<magic-name>.cat                       - assembly certificate
 * Manifests/<magic-name>.manifest                  - assembly manifest
 * Policies/<magic-policy-name>/<version>.cat       - policy certificate
 * Policies/<magic-policy-name>/<version>.policy    - policy manifest
 * <magic-name>/                                    - the actual DLLs
 *
 * Each assembly has an identifier, made up of the following fields:
 *    (plus a few I've omitted):
 *    type - ("win32" for DLLs, "win32_policy" for policies)
 *    name - name of the assembly
 *    processorArchitecture - "x86"
 *    version - a 4-part dotted version string (i.e. xxxxx.yyyyy.zzzzzz.wwwww)
 *    publicKeyToken - identifier for public key (for signing)
 *
 * This information, along with a "culture" and a "hash" are used to
 * create the magic names.  (Policy names use a subset of information,
 * including only the major two components of the version.)
 *
 * In addition to locating an appropriate DLL, there is sufficient
 * information in the manifests to verify the SHA-1 hash of the DLL, but
 * this code does not consider that.
 *
 * Policies are important as they contain redirection information.
 * That is, an application manifest may reference a particular version of
 * a DLL, but that specific version may not be present in the WinSxS area.
 * However, binding redirections (stored in the policies) can map old versions
 * to new ones.
 *
 *
 * How this works:
 * - the code that searches for a DLL calls SXS_LocateModuleA() to see
 *   if it exists in WinSxS.
 * - if it has not previously been processed, the manifest for the current
 *   application is read and processed to add potential search paths to
 *   the cache
 * - since there is no assembly identity associated directly with the DLL
 *   name, we simply search all directories currently in our cache
 *   (which will be smaller than the entire WinSxS, since we only search
 *    directories that matched a dependent assembly section in the application
 *    manifest)
 * 
 * [ The following is not currently being done.  It probably *should* be
 *   done, but the appropriate place to hook it in needs to be found.
 *   It will probably need to be hooked into pe_image.c somewhere. ]
 * - once a module is successfully loaded, the loader should then
 *   call SXS_FillCacheFromModule() before loading that modules dependencies
 *   (e.g. the MFC 8.0 DLLs have embedded manifests that are used to locate
 *    its localized components - MFCLOC)
 *
 */

/* The cache for the SXS information */
/* Since we only cache those assemblies that the application asks for
   (as opposed to the entire WinSxS directory) keeping this as a linear
   search is unlikely to be a major issue. 

   The cache is kept in memory for the duration of execution.
*/

/* A search path for a particular version */
typedef struct SXS_cachePath
{
    struct SXS_cachePath *next;
    char *path; /* path relative to WinSxS directory
                   where DLLs are located
                 */
} SXS_CACHE_PATH;

/* An entry in the cache may have multiple options */
typedef struct SXS_cacheEntry 
{
    struct SXS_cacheEntry *next;
    /* multiple by which we group entries */
    char *name;
    char *processorArchitecture;
    char *publicKeyToken;
    SXS_CACHE_PATH *path, *pathLast;
} SXS_CACHE_ENTRY;

static SXS_CACHE_ENTRY *sxs_cache = NULL;

static void SXS_FreeCachePath(SXS_CACHE_PATH *path)
{
    if (path)
    {
        RtlFreeHeap(GetProcessHeap(),0,path->path);
        RtlFreeHeap(GetProcessHeap(),0,path);
    }
}

static void SXS_FreeCacheEntry(SXS_CACHE_ENTRY *entry)
{
    if (entry)
    {
        RtlFreeHeap(GetProcessHeap(),0,entry->name);
        RtlFreeHeap(GetProcessHeap(),0,entry->processorArchitecture);
        RtlFreeHeap(GetProcessHeap(),0,entry->publicKeyToken);
        {
            SXS_CACHE_PATH *p, *pnext;
            p = entry->path;
            while (p)
            {
                pnext = p->next;
                SXS_FreeCachePath(p);
                p = pnext;
            }
        }
        RtlFreeHeap(GetProcessHeap(),0,entry);
    }
}

void SXS_EmptyCache(void)
{
    SXS_CACHE_ENTRY *e, *enext;

    RtlEnterCriticalSection(&sxs_cache_critsect);
    e = sxs_cache;
    while (e)
    {
        enext = e->next;
        SXS_FreeCacheEntry(e);
        e = enext;
    }
    sxs_cache = NULL;
    RtlLeaveCriticalSection(&sxs_cache_critsect);
}

/*  

On determining assembly names:

  http://blogs.msdn.com/jonwis/archive/2005/12/28/507863.aspx

*/

#define SXS_VERS_SZ 16

typedef struct SXS_parsedVersion
{
    struct {
        char str[SXS_VERS_SZ];
        unsigned val;
    } part[4];
} SXS_ParsedVersion;

static BOOL SXS_ParseVersion(char *version, SXS_ParsedVersion *v)
{
    int k;
    char *ptr = version, *dot;
    for (k = 0; k < 3; k++)
    {
        if (!(dot = strchr(ptr,'.')))
            return FALSE;
        int sz = (dot - ptr);
        if ((dot - ptr) > SXS_VERS_SZ-1)
            return FALSE;
        memcpy(v->part[k].str,ptr,sz);
        v->part[k].str[sz] = '\0';
        ptr = dot+1;
    }
    if (strlen(ptr) > SXS_VERS_SZ-1)
        return FALSE;
    strcpy(v->part[3].str,ptr);

    for (k = 0; k < 4; k++)
    {
        char *chk = NULL;
        v->part[k].val = strtol(v->part[k].str,&chk,10);
        if (!chk || *chk)
            return FALSE;
    }

    return TRUE;
}

/* missing string should be substitued with "none" */
#define SXS_STR(x) ((x)?(x):(char *)"none")

static void SXS_AppendPathPart(MEXStringBuf *psb,
                               char *str,
                               int max)
{
    char *x;

    x = (str ? str : "none");
    while (*x && max != 0) {
        if (!isalnum(*x) && *x != '.' && *x != '\\' &&
            *x != '-' && *x != '_')
        {
            /* invalid character */
            x++;
            continue;
        }
        MEXStringBufAddChar(psb,*(x++));
        if (max > 0)
            max--;
    }
}

/* Appends a prefix (not an exact name) since we do not
   know the hash (and we pretend we do not know the culture,
   although we could probably guess) */
static void SXS_AppendPathName(MEXStringBuf *sb,
                               SXS_ASSEMBLY_IDENTITY *id,
                               char *altversion) 
{
    SXS_AppendPathPart(sb,id->processorArchitecture,8);
    MEXStringBufAddChar(sb,'_');
    SXS_AppendPathPart(sb,id->name,64);
    MEXStringBufAddChar(sb,'_');
    SXS_AppendPathPart(sb,id->publicKeyToken,-1);
    MEXStringBufAddChar(sb,'_');
    SXS_AppendPathPart(sb,(altversion ?
                           altversion : id->version),-1);
    MEXStringBufAddChar(sb,'_');
}


static void SXS_AppendPolicyName(MEXStringBuf *psb,
                                 SXS_ASSEMBLY_IDENTITY *id)
{
    SXS_ParsedVersion v;
    SXS_AppendPathPart(psb,id->processorArchitecture,8);
    MEXStringBufAddString(psb,"_policy.",-1);
    if (!SXS_ParseVersion(id->version,&v))
    {
        ERR("unparsable version string: '%s'",id->version);
        /* This will return a shorter string than normal, which will
           read *all* policy statements.  Less efficient, but better
           than reading nothing at all. */
        return;
    }
    MEXStringBufAddString(psb,v.part[0].str,-1);
    MEXStringBufAddChar(psb,'.');
    MEXStringBufAddString(psb,v.part[1].str,-1);
    MEXStringBufAddChar(psb,'.');
    SXS_AppendPathPart(psb,id->name,64);
    MEXStringBufAddChar(psb,'_');
    SXS_AppendPathPart(psb,id->publicKeyToken,-1);
    MEXStringBufAddChar(psb,'_');
    /* after this is culture and hash */
}

static BOOL SXS_AppendWinSxSDir(MEXStringBuf *psb)
{
    UINT sz = GetWindowsDirectoryA(NULL,0);
    MEXStringBufGrow(psb,sz-1);
    GetWindowsDirectoryA(psb->buf+psb->use,sz);
    psb->use += sz-1;
    MEXStringBufAddChar(psb,'\\');
    MEXStringBufAddString(psb,"WinSxS\\",-1);
    return TRUE;
}

static BOOL SXS_AddCachePathA(SXS_CACHE_ENTRY *entry,
                              char *path)
{
    SXS_CACHE_PATH *p;
    
    if (!(p = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_CACHE_PATH))))
    {
        ERR("memory allocation failure\n");
        return FALSE;
    }
    if (SXS_SetString(&p->path,path,-1))
    {
        ERR("memory allocation failure\n");
        RtlFreeHeap(GetProcessHeap(),0,p);
        return FALSE;
    }
    /* add to list */
    p->next = NULL;
    if (entry->pathLast)
        entry->pathLast->next = p;
    else
        entry->path = p;
    entry->pathLast = p;

    TRACE("adding DLL search path: %s\n",path);

    return TRUE;
}

static BOOL SXS_AddCacheDirsA(SXS_CACHE_ENTRY *entry, char *match)
{
    HANDLE hFind;
    WIN32_FIND_DATAA found;
    BOOL result = TRUE;

    hFind = FindFirstFileA(match,&found);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                /* add this path */
                if (!SXS_AddCachePathA(entry,found.cFileName))
                {
                    ERR("failed to add DLL path: %s\n",
                        found.cFileName);
                    result = FALSE;
                }
            }
        } while (FindNextFileA(hFind,&found));
        FindClose(hFind);
    }
    return FALSE;
}

/* Like strcmp, return -1 if v1 < v2, 1 if v1 > v2, 0 if they are equal */
static int SXS_CompareVersions(SXS_ParsedVersion *v1,
                               SXS_ParsedVersion *v2)
{
    int k;
    for (k = 0; k < 4; k++)
    {
        if (v1->part[k].val > v2->part[k].val)
            return 1;
        else if (v1->part[k].val < v2->part[k].val)
            return -1;
    }
    return 0;
}

static BOOL SXS_MatchVersion(char *versrange_in, char *version)
{
    char *versrange, *dash;
    SXS_ParsedVersion vmin, vmax, vcur;

    if (!SXS_ParseVersion(version,&vcur))
        return FALSE;

    versrange = SXS_Strdup(versrange_in,-1);
    if (!(dash = strchr(versrange,'-')))
    {
        RtlFreeHeap(GetProcessHeap(),0,versrange);
        return FALSE;
    }
    *dash = '\0';
    if (!SXS_ParseVersion(versrange,&vmin) ||
        !SXS_ParseVersion(dash+1,&vmax))
    {
        RtlFreeHeap(GetProcessHeap(),0,versrange);
        return FALSE;
    }
    RtlFreeHeap(GetProcessHeap(),0,versrange);
    
    return (SXS_CompareVersions(&vmin,&vcur) <= 0 &&
            SXS_CompareVersions(&vmax,&vcur) >= 0);
}

/* FIXME: we are not scanning for duplicate entries in the cache.
   If we end up using manifests from included DLLs, this could
   be an issue (more likely efficiency than correctness) */
BOOL SXS_FillCacheFromManifest(SXS_ASSEMBLY *manifest)
{
    MEXStringBuf sb;
    SXS_DEPENDENT_ASSEMBLY *dep;

    if (!manifest)
        return FALSE;

    MEXStringBufStart(&sb);

    for (dep = manifest->dependencies;
         dep;
         dep = dep->next)
    {
        SXS_ASSEMBLY_IDENTITY *id = dep->identity;
        SXS_CACHE_ENTRY *entry;

        if (!id)
            continue;

        entry = RtlAllocateHeap(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SXS_CACHE_ENTRY));
        if (!entry)
        {
            ERR("memory allocation failure\n");
            return FALSE;
        }

        /* Step 1 - Look for specific matches by version */
        MEXStringBufClear(&sb);
        SXS_AppendWinSxSDir(&sb);
        SXS_AppendPathName(&sb,id,NULL);
        MEXStringBufAddChar(&sb,'*');
        (void) SXS_AddCacheDirsA(entry,sb.buf);

        /* Step 2 - Scan through matching policies looking for redirections */
        MEXStringBufClear(&sb);
        SXS_AppendWinSxSDir(&sb);
        MEXStringBufAddString(&sb,"Policies\\",-1);
        SXS_AppendPolicyName(&sb,id);
        MEXStringBufAddChar(&sb,'*');
        TRACE("searching for policies in %s\n",sb.buf);
        {
            HANDLE hFind;
            WIN32_FIND_DATAA found;

            hFind = FindFirstFileA(sb.buf,&found);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                do {
                    MEXStringBuf policymatch;
                    HANDLE hPolicyFind;
                    WIN32_FIND_DATAA policyfound;
                    
                    MEXStringBufStart(&policymatch);
                    SXS_AppendWinSxSDir(&policymatch);
                    MEXStringBufAddString(&policymatch,"Policies\\",-1);
                    MEXStringBufAddString(&policymatch,found.cFileName,-1);
                    MEXStringBufAddString(&policymatch,"\\*.policy",-1);
                    hPolicyFind = FindFirstFileA(policymatch.buf,
                                                 &policyfound);
                    if (hPolicyFind != INVALID_HANDLE_VALUE)
                    {
                        SXS_ASSEMBLY *policy = NULL;

                        MEXStringBufClear(&policymatch);
                        SXS_AppendWinSxSDir(&policymatch);
                        MEXStringBufAddString(&policymatch,"Policies\\",-1);
                        MEXStringBufAddString(&policymatch,found.cFileName,-1);
                        MEXStringBufAddChar(&policymatch,'\\');
                        MEXStringBufAddString(&policymatch,policyfound.cFileName,-1);

                        TRACE("reading policy %s\n",policymatch.buf);
                        if (SXS_ReadAssemblyFileA(policymatch.buf,&policy) &&
                            policy != NULL)
                        {
                            /* Look for a dependent assembly that matches */
                            SXS_DEPENDENT_ASSEMBLY *policydep;
                            for (policydep = policy->dependencies;
                                 policydep;
                                 policydep = policydep->next)
                            {
                                SXS_ASSEMBLY_IDENTITY *policyid = policydep->identity;
                                if (policyid &&
                                    STREQ(policyid->processorArchitecture,
                                          id->processorArchitecture) &&
                                    STREQ(policyid->name,id->name) &&
                                    STREQ(policyid->publicKeyToken,id->publicKeyToken))
                                {
                                    /* This matches - look for a version mapping */
                                    SXS_BINDING_REDIRECT *redirect;
                                    for (redirect = policydep->redirect;
                                         redirect;
                                         redirect = redirect->next)
                                    {
                                        // Do we match versions?
                                        if (SXS_MatchVersion(redirect->oldVersion,
                                                             id->version))
                                        {
                                            // Yes we do
                                            MEXStringBufClear(&sb);
                                            SXS_AppendWinSxSDir(&sb);
                                            SXS_AppendPathName(&sb,id,
                                                               redirect->newVersion);
                                            MEXStringBufAddChar(&sb,'*');
                                            (void) SXS_AddCacheDirsA(entry,sb.buf);
                                        }
                                    }
                                }
                            }
                            SXS_FreeAssembly(policy);
                        }
                    } while (FindNextFileA(hPolicyFind,&policyfound));
                    FindClose(hPolicyFind);
                } while (FindNextFileA(hFind,&found));
                FindClose(hFind);
            }
        }

        RtlEnterCriticalSection(&sxs_cache_critsect);
        entry->next = sxs_cache;
        sxs_cache = entry;
        RtlLeaveCriticalSection(&sxs_cache_critsect);
    }

    MEXStringBufEnd(&sb);

    return TRUE;
}

BOOL SXS_FillCacheFromModule(HMODULE hModule)
{
    SXS_ASSEMBLY *manifest = NULL;
    BOOL result;

    TRACE("loading from module: 0x%08x\n",(unsigned)hModule);

    if (!SXS_ReadAssemblyModule(hModule,&manifest))
    {
        TRACE("failed to read assembly from module\n");
        return FALSE;
    }
    if (!manifest)
    {
        /* this is okay - it just means we do not have a manifest */
        TRACE("module has no manifest\n");
        return TRUE;
    }
    result = SXS_FillCacheFromManifest(manifest);
    SXS_FreeAssembly(manifest);
    return result;
}

/* flag to indicate if we have loaded the main manifest for the exe */
static BOOL SXS_mainManifestLoaded = FALSE;

BOOL SXS_LocateModuleA(LPCSTR libname, LPCSTR filename)
{
    /* Make sure we have kernel32 loaded or we can run into trouble
       inside of FindResource... */
    BOOL found = FALSE;

    if (!SXS_mainManifestLoaded && GetModuleHandleA("KERNEL32"))
    {
        /* Try to load the application's manifest */
        (void) SXS_FillCacheFromModule(GetModuleHandleA(NULL));
        SXS_mainManifestLoaded = TRUE;
    }

    {
        SXS_CACHE_ENTRY *entry;
        MEXStringBuf sb;

        MEXStringBufStart(&sb);
        RtlEnterCriticalSection(&sxs_cache_critsect);
        /* NB: this lock is okay so long as nothing inside this
           loop attempts to pull in any more modules (or we are
           likely to deadlock) */
        for (entry = sxs_cache; entry && !found; entry = entry->next)
        {
            SXS_CACHE_PATH *path;
            for (path = entry->path; path && !found; path = path->next)
            {
                DOS_FULL_NAME fullname;

                MEXStringBufClear(&sb);
                SXS_AppendWinSxSDir(&sb);
                MEXStringBufAddString(&sb,path->path,-1);
                MEXStringBufAddChar(&sb,'\\');
                MEXStringBufAddString(&sb,(char *)libname,-1);

                if (sb.use <= MAX_PATH &&
                    DOSFS_GetFullName(sb.buf,TRUE,&fullname))
                {
                    TRACE("found in SxS: %s\n",sb.buf);
                    if (filename)
                        strcpy((char *)filename,sb.buf);
                    found = TRUE;
                }
            }
        }
        RtlLeaveCriticalSection(&sxs_cache_critsect);
        MEXStringBufEnd(&sb);
    }
    return found;
}

void SXS_Init(void)
{
    RtlInitializeCriticalSection(&sxs_parser_critsect);
    RtlInitializeCriticalSection(&sxs_cache_critsect);
}
