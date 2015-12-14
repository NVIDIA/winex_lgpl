/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2004,2005 Aric Stewart for CodeWeavers
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "winsvc.h"
#include "odbcinst.h"
#include "wine/debug.h"
#include "msidefs.h"
#include "msipriv.h"
#include "winuser.h"
#include "shlobj.h"
#include "wine/unicode.h"
#include "winver.h"
#include "oleauto.h"

#define REG_PROGRESS_VALUE 13200
#define COMPONENT_PROGRESS_VALUE 24000

WINE_DEFAULT_DEBUG_CHANNEL(msi);

/*
 * Prototypes
 */
static UINT ACTION_ProcessExecSequence(MSIPACKAGE *package, BOOL UIran);
static UINT ACTION_ProcessUISequence(MSIPACKAGE *package);
static UINT ACTION_PerformActionSequence(MSIPACKAGE *package, UINT seq, BOOL UI);
static BOOL ACTION_HandleStandardAction(MSIPACKAGE *package, LPCWSTR action, UINT* rc, BOOL force);

/*
 * consts and values used
 */
static const WCHAR c_colon[] = {'C',':','\\',0};

static const WCHAR szCreateFolders[] =
    {'C','r','e','a','t','e','F','o','l','d','e','r','s',0};
static const WCHAR szCostFinalize[] =
    {'C','o','s','t','F','i','n','a','l','i','z','e',0};
const WCHAR szInstallFiles[] =
    {'I','n','s','t','a','l','l','F','i','l','e','s',0};
const WCHAR szDuplicateFiles[] =
    {'D','u','p','l','i','c','a','t','e','F','i','l','e','s',0};
static const WCHAR szWriteRegistryValues[] =
    {'W','r','i','t','e','R','e','g','i','s','t','r','y',
            'V','a','l','u','e','s',0};
static const WCHAR szCostInitialize[] =
    {'C','o','s','t','I','n','i','t','i','a','l','i','z','e',0};
static const WCHAR szFileCost[] = 
    {'F','i','l','e','C','o','s','t',0};
static const WCHAR szInstallInitialize[] = 
    {'I','n','s','t','a','l','l','I','n','i','t','i','a','l','i','z','e',0};
static const WCHAR szInstallValidate[] = 
    {'I','n','s','t','a','l','l','V','a','l','i','d','a','t','e',0};
static const WCHAR szLaunchConditions[] = 
    {'L','a','u','n','c','h','C','o','n','d','i','t','i','o','n','s',0};
static const WCHAR szProcessComponents[] = 
    {'P','r','o','c','e','s','s','C','o','m','p','o','n','e','n','t','s',0};
static const WCHAR szRegisterTypeLibraries[] = 
    {'R','e','g','i','s','t','e','r','T','y','p','e',
            'L','i','b','r','a','r','i','e','s',0};
const WCHAR szRegisterClassInfo[] = 
    {'R','e','g','i','s','t','e','r','C','l','a','s','s','I','n','f','o',0};
const WCHAR szRegisterProgIdInfo[] = 
    {'R','e','g','i','s','t','e','r','P','r','o','g','I','d','I','n','f','o',0};
static const WCHAR szCreateShortcuts[] = 
    {'C','r','e','a','t','e','S','h','o','r','t','c','u','t','s',0};
static const WCHAR szPublishProduct[] = 
    {'P','u','b','l','i','s','h','P','r','o','d','u','c','t',0};
static const WCHAR szWriteIniValues[] = 
    {'W','r','i','t','e','I','n','i','V','a','l','u','e','s',0};
static const WCHAR szSelfRegModules[] = 
    {'S','e','l','f','R','e','g','M','o','d','u','l','e','s',0};
static const WCHAR szPublishFeatures[] = 
    {'P','u','b','l','i','s','h','F','e','a','t','u','r','e','s',0};
static const WCHAR szRegisterProduct[] = 
    {'R','e','g','i','s','t','e','r','P','r','o','d','u','c','t',0};
static const WCHAR szInstallExecute[] = 
    {'I','n','s','t','a','l','l','E','x','e','c','u','t','e',0};
static const WCHAR szInstallExecuteAgain[] = 
    {'I','n','s','t','a','l','l','E','x','e','c','u','t','e',
            'A','g','a','i','n',0};
static const WCHAR szInstallFinalize[] = 
    {'I','n','s','t','a','l','l','F','i','n','a','l','i','z','e',0};
static const WCHAR szForceReboot[] = 
    {'F','o','r','c','e','R','e','b','o','o','t',0};
static const WCHAR szResolveSource[] =
    {'R','e','s','o','l','v','e','S','o','u','r','c','e',0};
static const WCHAR szAppSearch[] = 
    {'A','p','p','S','e','a','r','c','h',0};
static const WCHAR szAllocateRegistrySpace[] = 
    {'A','l','l','o','c','a','t','e','R','e','g','i','s','t','r','y',
            'S','p','a','c','e',0};
static const WCHAR szBindImage[] = 
    {'B','i','n','d','I','m','a','g','e',0};
static const WCHAR szCCPSearch[] = 
    {'C','C','P','S','e','a','r','c','h',0};
static const WCHAR szDeleteServices[] = 
    {'D','e','l','e','t','e','S','e','r','v','i','c','e','s',0};
static const WCHAR szDisableRollback[] = 
    {'D','i','s','a','b','l','e','R','o','l','l','b','a','c','k',0};
static const WCHAR szExecuteAction[] = 
    {'E','x','e','c','u','t','e','A','c','t','i','o','n',0};
const WCHAR szFindRelatedProducts[] = 
    {'F','i','n','d','R','e','l','a','t','e','d',
            'P','r','o','d','u','c','t','s',0};
static const WCHAR szInstallAdminPackage[] = 
    {'I','n','s','t','a','l','l','A','d','m','i','n',
            'P','a','c','k','a','g','e',0};
static const WCHAR szInstallSFPCatalogFile[] = 
    {'I','n','s','t','a','l','l','S','F','P','C','a','t','a','l','o','g',
            'F','i','l','e',0};
static const WCHAR szIsolateComponents[] = 
    {'I','s','o','l','a','t','e','C','o','m','p','o','n','e','n','t','s',0};
const WCHAR szMigrateFeatureStates[] = 
    {'M','i','g','r','a','t','e','F','e','a','t','u','r','e',
            'S','t','a','t','e','s',0};
const WCHAR szMoveFiles[] = 
    {'M','o','v','e','F','i','l','e','s',0};
static const WCHAR szMsiPublishAssemblies[] = 
    {'M','s','i','P','u','b','l','i','s','h',
            'A','s','s','e','m','b','l','i','e','s',0};
static const WCHAR szMsiUnpublishAssemblies[] = 
    {'M','s','i','U','n','p','u','b','l','i','s','h',
            'A','s','s','e','m','b','l','i','e','s',0};
static const WCHAR szInstallODBC[] = 
    {'I','n','s','t','a','l','l','O','D','B','C',0};
static const WCHAR szInstallServices[] = 
    {'I','n','s','t','a','l','l','S','e','r','v','i','c','e','s',0};
const WCHAR szPatchFiles[] = 
    {'P','a','t','c','h','F','i','l','e','s',0};
static const WCHAR szPublishComponents[] = 
    {'P','u','b','l','i','s','h','C','o','m','p','o','n','e','n','t','s',0};
static const WCHAR szRegisterComPlus[] =
    {'R','e','g','i','s','t','e','r','C','o','m','P','l','u','s',0};
const WCHAR szRegisterExtensionInfo[] =
    {'R','e','g','i','s','t','e','r','E','x','t','e','n','s','i','o','n',
            'I','n','f','o',0};
static const WCHAR szRegisterFonts[] =
    {'R','e','g','i','s','t','e','r','F','o','n','t','s',0};
const WCHAR szRegisterMIMEInfo[] =
    {'R','e','g','i','s','t','e','r','M','I','M','E','I','n','f','o',0};
static const WCHAR szRegisterUser[] =
    {'R','e','g','i','s','t','e','r','U','s','e','r',0};
const WCHAR szRemoveDuplicateFiles[] =
    {'R','e','m','o','v','e','D','u','p','l','i','c','a','t','e',
            'F','i','l','e','s',0};
static const WCHAR szRemoveEnvironmentStrings[] =
    {'R','e','m','o','v','e','E','n','v','i','r','o','n','m','e','n','t',
            'S','t','r','i','n','g','s',0};
const WCHAR szRemoveExistingProducts[] =
    {'R','e','m','o','v','e','E','x','i','s','t','i','n','g',
            'P','r','o','d','u','c','t','s',0};
const WCHAR szRemoveFiles[] =
    {'R','e','m','o','v','e','F','i','l','e','s',0};
static const WCHAR szRemoveFolders[] =
    {'R','e','m','o','v','e','F','o','l','d','e','r','s',0};
static const WCHAR szRemoveIniValues[] =
    {'R','e','m','o','v','e','I','n','i','V','a','l','u','e','s',0};
static const WCHAR szRemoveODBC[] =
    {'R','e','m','o','v','e','O','D','B','C',0};
static const WCHAR szRemoveRegistryValues[] =
    {'R','e','m','o','v','e','R','e','g','i','s','t','r','y',
            'V','a','l','u','e','s',0};
static const WCHAR szRemoveShortcuts[] =
    {'R','e','m','o','v','e','S','h','o','r','t','c','u','t','s',0};
static const WCHAR szRMCCPSearch[] =
    {'R','M','C','C','P','S','e','a','r','c','h',0};
static const WCHAR szScheduleReboot[] =
    {'S','c','h','e','d','u','l','e','R','e','b','o','o','t',0};
static const WCHAR szSelfUnregModules[] =
    {'S','e','l','f','U','n','r','e','g','M','o','d','u','l','e','s',0};
static const WCHAR szSetODBCFolders[] =
    {'S','e','t','O','D','B','C','F','o','l','d','e','r','s',0};
static const WCHAR szStartServices[] =
    {'S','t','a','r','t','S','e','r','v','i','c','e','s',0};
static const WCHAR szStopServices[] =
    {'S','t','o','p','S','e','r','v','i','c','e','s',0};
static const WCHAR szUnpublishComponents[] =
    {'U','n','p','u','b','l','i','s','h',
            'C','o','m','p','o','n','e','n','t','s',0};
static const WCHAR szUnpublishFeatures[] =
    {'U','n','p','u','b','l','i','s','h','F','e','a','t','u','r','e','s',0};
const WCHAR szUnregisterClassInfo[] =
    {'U','n','r','e','g','i','s','t','e','r','C','l','a','s','s',
            'I','n','f','o',0};
static const WCHAR szUnregisterComPlus[] =
    {'U','n','r','e','g','i','s','t','e','r','C','o','m','P','l','u','s',0};
const WCHAR szUnregisterExtensionInfo[] =
    {'U','n','r','e','g','i','s','t','e','r',
            'E','x','t','e','n','s','i','o','n','I','n','f','o',0};
static const WCHAR szUnregisterFonts[] =
    {'U','n','r','e','g','i','s','t','e','r','F','o','n','t','s',0};
const WCHAR szUnregisterMIMEInfo[] =
    {'U','n','r','e','g','i','s','t','e','r','M','I','M','E','I','n','f','o',0};
const WCHAR szUnregisterProgIdInfo[] =
    {'U','n','r','e','g','i','s','t','e','r','P','r','o','g','I','d',
            'I','n','f','o',0};
static const WCHAR szUnregisterTypeLibraries[] =
    {'U','n','r','e','g','i','s','t','e','r','T','y','p','e',
            'L','i','b','r','a','r','i','e','s',0};
static const WCHAR szValidateProductID[] =
    {'V','a','l','i','d','a','t','e','P','r','o','d','u','c','t','I','D',0};
static const WCHAR szWriteEnvironmentStrings[] =
    {'W','r','i','t','e','E','n','v','i','r','o','n','m','e','n','t',
            'S','t','r','i','n','g','s',0};

/* action handlers */
typedef UINT (*STANDARDACTIONHANDLER)(MSIPACKAGE*);

struct _actions {
    LPCWSTR action;
    STANDARDACTIONHANDLER handler;
};


/********************************************************
 * helper functions
 ********************************************************/

static void ui_actionstart(MSIPACKAGE *package, LPCWSTR action)
{
    static const WCHAR Query_t[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','A','c','t','i','o', 'n','T','e','x','t','`',' ',
         'W','H','E','R','E', ' ','`','A','c','t','i','o','n','`',' ','=', 
         ' ','\'','%','s','\'',0};
    MSIRECORD * row;

    row = MSI_QueryGetRecord( package->db, Query_t, action );
    if (!row)
        return;
    MSI_ProcessMessage(package, INSTALLMESSAGE_ACTIONSTART, row);
    msiobj_release(&row->hdr);
}

static void ui_actioninfo(MSIPACKAGE *package, LPCWSTR action, BOOL start, 
                          UINT rc)
{
    MSIRECORD * row;
    static const WCHAR template_s[]=
        {'A','c','t','i','o','n',' ','s','t','a','r','t',' ','%','s',':',' ',
         '%','s', '.',0};
    static const WCHAR template_e[]=
        {'A','c','t','i','o','n',' ','e','n','d','e','d',' ','%','s',':',' ',
         '%','s', '.',' ','R','e','t','u','r','n',' ','v','a','l','u','e',' ',
         '%','i','.',0};
    static const WCHAR format[] = 
        {'H','H','\'',':','\'','m','m','\'',':','\'','s','s',0};
    WCHAR message[1024];
    WCHAR timet[0x100];

    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, NULL, format, timet, 0x100);
    if (start)
        sprintfW(message,template_s,timet,action);
    else
        sprintfW(message,template_e,timet,action,rc);
    
    row = MSI_CreateRecord(1);
    MSI_RecordSetStringW(row,1,message);
 
    MSI_ProcessMessage(package, INSTALLMESSAGE_INFO, row);
    msiobj_release(&row->hdr);
}

UINT msi_parse_command_line( MSIPACKAGE *package, LPCWSTR szCommandLine )
{
    LPCWSTR ptr,ptr2;
    BOOL quote;
    DWORD len;
    LPWSTR prop = NULL, val = NULL;

    if (!szCommandLine)
        return ERROR_SUCCESS;

    ptr = szCommandLine;
       
    while (*ptr)
    {
        if (*ptr==' ')
        {
            ptr++;
            continue;
        }

        TRACE("Looking at %s\n",debugstr_w(ptr));

        ptr2 = strchrW(ptr,'=');
        if (!ptr2)
        {
            ERR("command line contains unknown string : %s\n", debugstr_w(ptr));
            break;
        }
 
        quote = FALSE;

        len = ptr2-ptr;
        prop = msi_alloc((len+1)*sizeof(WCHAR));
        memcpy(prop,ptr,len*sizeof(WCHAR));
        prop[len]=0;
        ptr2++;
       
        len = 0; 
        ptr = ptr2; 
        while (*ptr && (quote || (!quote && *ptr!=' ')))
        {
            if (*ptr == '"')
                quote = !quote;
            ptr++;
            len++;
        }
       
        if (*ptr2=='"')
        {
            ptr2++;
            len -= 2;
        }
        val = msi_alloc((len+1)*sizeof(WCHAR));
        memcpy(val,ptr2,len*sizeof(WCHAR));
        val[len] = 0;

        if (lstrlenW(prop) > 0)
        {
            TRACE("Found commandline property (%s) = (%s)\n", 
                   debugstr_w(prop), debugstr_w(val));
            MSI_SetPropertyW(package,prop,val);
        }
        msi_free(val);
        msi_free(prop);
    }

    return ERROR_SUCCESS;
}


static LPWSTR* msi_split_string( LPCWSTR str, WCHAR sep )
{
    LPCWSTR pc;
    LPWSTR p, *ret = NULL;
    UINT count = 0;

    if (!str)
        return ret;

    /* count the number of substrings */
    for ( pc = str, count = 0; pc; count++ )
    {
        pc = strchrW( pc, sep );
        if (pc)
            pc++;
    }

    /* allocate space for an array of substring pointers and the substrings */
    ret = msi_alloc( (count+1) * sizeof (LPWSTR) +
                     (lstrlenW(str)+1) * sizeof(WCHAR) );
    if (!ret)
        return ret;

    /* copy the string and set the pointers */
    p = (LPWSTR) &ret[count+1];
    lstrcpyW( p, str );
    for( count = 0; (ret[count] = p); count++ )
    {
        p = strchrW( p, sep );
        if (p)
            *p++ = 0;
    }

    return ret;
}

static UINT msi_check_transform_applicable( MSIPACKAGE *package, IStorage *patch )
{
    WCHAR szProductCode[] = { 'P','r','o','d','u','c','t','C','o','d','e',0 };
    LPWSTR prod_code, patch_product;
    UINT ret;

    prod_code = msi_dup_property( package, szProductCode );
    patch_product = msi_get_suminfo_product( patch );

    TRACE("db = %s patch = %s\n", debugstr_w(prod_code), debugstr_w(patch_product));

    if ( strstrW( patch_product, prod_code ) )
        ret = ERROR_SUCCESS;
    else
        ret = ERROR_FUNCTION_FAILED;

    msi_free( patch_product );
    msi_free( prod_code );

    return ret;
}

static UINT msi_apply_substorage_transform( MSIPACKAGE *package,
                                 MSIDATABASE *patch_db, LPCWSTR name )
{
    UINT ret = ERROR_FUNCTION_FAILED;
    IStorage *stg = NULL;
    HRESULT r;

    TRACE("%p %s\n", package, debugstr_w(name) );

    if (*name++ != ':')
    {
        ERR("expected a colon in %s\n", debugstr_w(name));
        return ERROR_FUNCTION_FAILED;
    }

    r = IStorage_OpenStorage( patch_db->storage, name, NULL, STGM_SHARE_EXCLUSIVE, NULL, 0, &stg );
    if (SUCCEEDED(r))
    {
        ret = msi_check_transform_applicable( package, stg );
        if (ret == ERROR_SUCCESS)
            msi_table_apply_transform( package->db, stg );
        else
            TRACE("substorage transform %s wasn't applicable\n", debugstr_w(name));
        IStorage_Release( stg );
    }
    else
        ERR("failed to open substorage %s\n", debugstr_w(name));

    return ERROR_SUCCESS;
}

static UINT msi_check_patch_applicable( MSIPACKAGE *package, MSISUMMARYINFO *si )
{
    static const WCHAR szProdCode[] = { 'P','r','o','d','u','c','t','C','o','d','e',0 };
    LPWSTR guid_list, *guids, product_code;
    UINT i, ret = ERROR_FUNCTION_FAILED;

    product_code = msi_dup_property( package, szProdCode );
    if (!product_code)
    {
        /* FIXME: the property ProductCode should be written into the DB somewhere */
        ERR("no product code to check\n");
        return ERROR_SUCCESS;
    }

    guid_list = msi_suminfo_dup_string( si, PID_TEMPLATE );
    guids = msi_split_string( guid_list, ';' );
    for ( i = 0; guids[i] && ret != ERROR_SUCCESS; i++ )
    {
        if (!lstrcmpW( guids[i], product_code ))
            ret = ERROR_SUCCESS;
    }
    msi_free( guids );
    msi_free( guid_list );
    msi_free( product_code );

    return ret;
}

static UINT msi_parse_patch_summary( MSIPACKAGE *package, MSIDATABASE *patch_db )
{
    MSISUMMARYINFO *si;
    LPWSTR str, *substorage;
    UINT i, r = ERROR_SUCCESS;

    si = MSI_GetSummaryInformationW( patch_db->storage, 0 );
    if (!si)
        return ERROR_FUNCTION_FAILED;

    msi_check_patch_applicable( package, si );

    /* enumerate the substorage */
    str = msi_suminfo_dup_string( si, PID_LASTAUTHOR );
    substorage = msi_split_string( str, ';' );
    for ( i = 0; substorage && substorage[i] && r == ERROR_SUCCESS; i++ )
        r = msi_apply_substorage_transform( package, patch_db, substorage[i] );
    msi_free( substorage );
    msi_free( str );

    /* FIXME: parse the sources in PID_REVNUMBER and do something with them... */

    msiobj_release( &si->hdr );

    return r;
}

static UINT msi_apply_patch_package( MSIPACKAGE *package, LPCWSTR file )
{
    MSIDATABASE *patch_db = NULL;
    UINT r;

    TRACE("%p %s\n", package, debugstr_w( file ) );

    /* FIXME:
     *  We probably want to make sure we only open a patch collection here.
     *  Patch collections (.msp) and databases (.msi) have different GUIDs
     *  but currently MSI_OpenDatabaseW will accept both.
     */
    r = MSI_OpenDatabaseW( file, MSIDBOPEN_READONLY, &patch_db );
    if ( r != ERROR_SUCCESS )
    {
        ERR("failed to open patch collection %s\n", debugstr_w( file ) );
        return r;
    }

    msi_parse_patch_summary( package, patch_db );

    /*
     * There might be a CAB file in the patch package,
     * so append it to the list of storage to search for streams.
     */
    append_storage_to_db( package->db, patch_db->storage );

    msiobj_release( &patch_db->hdr );

    return ERROR_SUCCESS;
}

/* get the PATCH property, and apply all the patches it specifies */
static UINT msi_apply_patches( MSIPACKAGE *package )
{
    static const WCHAR szPatch[] = { 'P','A','T','C','H',0 };
    LPWSTR patch_list, *patches;
    UINT i, r = ERROR_SUCCESS;

    patch_list = msi_dup_property( package, szPatch );

    TRACE("patches to be applied: %s\n", debugstr_w( patch_list ) );

    patches = msi_split_string( patch_list, ';' );
    for( i=0; patches && patches[i] && r == ERROR_SUCCESS; i++ )
        r = msi_apply_patch_package( package, patches[i] );

    msi_free( patches );
    msi_free( patch_list );

    return r;
}

static UINT msi_apply_transforms( MSIPACKAGE *package )
{
    static const WCHAR szTransforms[] = {
        'T','R','A','N','S','F','O','R','M','S',0 };
    LPWSTR xform_list, *xforms;
    UINT i, r = ERROR_SUCCESS;

    xform_list = msi_dup_property( package, szTransforms );
    xforms = msi_split_string( xform_list, ';' );

    for( i=0; xforms && xforms[i] && r == ERROR_SUCCESS; i++ )
    {
        if (xforms[i][0] == ':')
            r = msi_apply_substorage_transform( package, package->db, xforms[i] );
        else
            r = MSI_DatabaseApplyTransformW( package->db, xforms[i], 0 );
    }

    msi_free( xforms );
    msi_free( xform_list );

    return r;
}

static BOOL ui_sequence_exists( MSIPACKAGE *package )
{
    MSIQUERY *view;
    UINT rc;

    static const WCHAR ExecSeqQuery [] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','s','t','a','l','l',
         'U','I','S','e','q','u','e','n','c','e','`',
         ' ','W','H','E','R','E',' ',
         '`','S','e','q','u','e','n','c','e','`',' ',
         '>',' ','0',' ','O','R','D','E','R',' ','B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc == ERROR_SUCCESS)
    {
        msiobj_release(&view->hdr);
        return TRUE;
    }

    return FALSE;
}

static UINT msi_set_sourcedir_props(MSIPACKAGE *package, BOOL replace)
{
    LPWSTR p, db;
    LPWSTR source, check;
    DWORD len;

    static const WCHAR szOriginalDatabase[] =
        {'O','r','i','g','i','n','a','l','D','a','t','a','b','a','s','e',0};

    db = msi_dup_property( package, szOriginalDatabase );
    if (!db)
        return ERROR_OUTOFMEMORY;

    p = strrchrW( db, '\\' );
    if (!p)
    {
        p = strrchrW( db, '/' );
        if (!p)
        {
            msi_free(db);
            return ERROR_SUCCESS;
        }
    }

    len = p - db + 2;
    source = msi_alloc( len * sizeof(WCHAR) );
    lstrcpynW( source, db, len );

    check = msi_dup_property( package, cszSourceDir );
    if (!check || replace)
        MSI_SetPropertyW( package, cszSourceDir, source );

    msi_free( check );

    check = msi_dup_property( package, cszSOURCEDIR );
    if (!check || replace)
        MSI_SetPropertyW( package, cszSOURCEDIR, source );

    msi_free( check );
    msi_free( source );
    msi_free( db );

    return ERROR_SUCCESS;
}

/****************************************************
 * TOP level entry points 
 *****************************************************/

UINT MSI_InstallPackage( MSIPACKAGE *package, LPCWSTR szPackagePath,
                         LPCWSTR szCommandLine )
{
    UINT rc;
    BOOL ui = FALSE, ui_exists;
    static const WCHAR szUILevel[] = {'U','I','L','e','v','e','l',0};
    static const WCHAR szAction[] = {'A','C','T','I','O','N',0};
    static const WCHAR szInstall[] = {'I','N','S','T','A','L','L',0};

    MSI_SetPropertyW(package, szAction, szInstall);

    package->script = msi_alloc_zero(sizeof(MSISCRIPT));

    package->script->InWhatSequence = SEQUENCE_INSTALL;

    if (szPackagePath)   
    {
        LPWSTR p, dir;
        LPCWSTR file;

        dir = strdupW(szPackagePath);
        p = strrchrW(dir, '\\');
        if (p)
        {
            *(++p) = 0;
            file = szPackagePath + (p - dir);
        }
        else
        {
            msi_free(dir);
            dir = msi_alloc(MAX_PATH*sizeof(WCHAR));
            GetCurrentDirectoryW(MAX_PATH, dir);
            lstrcatW(dir, cszbs);
            file = szPackagePath;
        }

        msi_free( package->PackagePath );
        package->PackagePath = msi_alloc((lstrlenW(dir) + lstrlenW(file) + 1) * sizeof(WCHAR));
        if (!package->PackagePath)
        {
            msi_free(dir);
            return ERROR_OUTOFMEMORY;
        }

        lstrcpyW(package->PackagePath, dir);
        lstrcatW(package->PackagePath, file);
        msi_free(dir);

        msi_set_sourcedir_props(package, FALSE);
    }

    msi_parse_command_line( package, szCommandLine );

    msi_apply_transforms( package );
    msi_apply_patches( package );

    /* properties may have been added by a transform */
    msi_clone_properties( package );

    if ( (msi_get_property_int(package, szUILevel, 0) & INSTALLUILEVEL_MASK) >= INSTALLUILEVEL_REDUCED )
    {
        package->script->InWhatSequence |= SEQUENCE_UI;
        rc = ACTION_ProcessUISequence(package);
        ui = TRUE;
        ui_exists = ui_sequence_exists(package);
        if (rc == ERROR_SUCCESS || !ui_exists)
        {
            package->script->InWhatSequence |= SEQUENCE_EXEC;
            rc = ACTION_ProcessExecSequence(package,ui_exists);
        }
    }
    else
        rc = ACTION_ProcessExecSequence(package,FALSE);

    package->script->CurrentlyScripting= FALSE;

    /* process the ending type action */
    if (rc == ERROR_SUCCESS)
        ACTION_PerformActionSequence(package,-1,ui);
    else if (rc == ERROR_INSTALL_USEREXIT) 
        ACTION_PerformActionSequence(package,-2,ui);
    else if (rc == ERROR_INSTALL_SUSPEND) 
        ACTION_PerformActionSequence(package,-4,ui);
    else  /* failed */
        ACTION_PerformActionSequence(package,-3,ui);

    /* finish up running custom actions */
    ACTION_FinishCustomActions(package);
    
    return rc;
}

static UINT ACTION_PerformActionSequence(MSIPACKAGE *package, UINT seq, BOOL UI)
{
    UINT rc = ERROR_SUCCESS;
    MSIRECORD * row = 0;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','s','t','a','l','l','E','x','e','c','u','t','e',
         'S','e','q','u','e','n','c','e','`',' ', 'W','H','E','R','E',' ',
         '`','S','e','q','u','e','n','c','e','`',' ', '=',' ','%','i',0};

    static const WCHAR UISeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
     '`','I','n','s','t','a','l','l','U','I','S','e','q','u','e','n','c','e',
     '`', ' ', 'W','H','E','R','E',' ','`','S','e','q','u','e','n','c','e','`',
	 ' ', '=',' ','%','i',0};

    if (UI)
        row = MSI_QueryGetRecord(package->db, UISeqQuery, seq);
    else
        row = MSI_QueryGetRecord(package->db, ExecSeqQuery, seq);

    if (row)
    {
        LPCWSTR action, cond;

        TRACE("Running the actions\n"); 

        /* check conditions */
        cond = MSI_RecordGetString(row,2);

        /* this is a hack to skip errors in the condition code */
        if (MSI_EvaluateConditionW(package, cond) == MSICONDITION_FALSE)
            goto end;

        action = MSI_RecordGetString(row,1);
        if (!action)
        {
            ERR("failed to fetch action\n");
            rc = ERROR_FUNCTION_FAILED;
            goto end;
        }

        if (UI)
            rc = ACTION_PerformUIAction(package,action,-1);
        else
            rc = ACTION_PerformAction(package,action,-1,FALSE);
end:
        msiobj_release(&row->hdr);
    }
    else
        rc = ERROR_SUCCESS;

    return rc;
}

typedef struct {
    MSIPACKAGE* package;
    BOOL UI;
} iterate_action_param;

static UINT ITERATE_Actions(MSIRECORD *row, LPVOID param)
{
    iterate_action_param *iap= (iterate_action_param*)param;
    UINT rc;
    LPCWSTR cond, action;

    action = MSI_RecordGetString(row,1);
    if (!action)
    {
        ERR("Error is retrieving action name\n");
        return ERROR_FUNCTION_FAILED;
    }

    /* check conditions */
    cond = MSI_RecordGetString(row,2);

    /* this is a hack to skip errors in the condition code */
    if (MSI_EvaluateConditionW(iap->package, cond) == MSICONDITION_FALSE)
    {
        TRACE("Skipping action: %s (condition is false)\n", debugstr_w(action));
        return ERROR_SUCCESS;
    }

    if (iap->UI)
        rc = ACTION_PerformUIAction(iap->package,action,-1);
    else
        rc = ACTION_PerformAction(iap->package,action,-1,FALSE);

    msi_dialog_check_messages( NULL );

    if (iap->package->CurrentInstallState != ERROR_SUCCESS )
        rc = iap->package->CurrentInstallState;

    if (rc == ERROR_FUNCTION_NOT_CALLED)
        rc = ERROR_SUCCESS;

    if (rc != ERROR_SUCCESS)
        ERR("Execution halted, action %s returned %i\n", debugstr_w(action), rc);

    return rc;
}

UINT MSI_Sequence( MSIPACKAGE *package, LPCWSTR szTable, INT iSequenceMode )
{
    MSIQUERY * view;
    UINT r;
    static const WCHAR query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','%','s','`',
         ' ','W','H','E','R','E',' ', 
         '`','S','e','q','u','e','n','c','e','`',' ',
         '>',' ','0',' ','O','R','D','E','R',' ','B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`',0};
    iterate_action_param iap;

    /*
     * FIXME: probably should be checking UILevel in the
     *       ACTION_PerformUIAction/ACTION_PerformAction
     *       rather than saving the UI level here. Those
     *       two functions can be merged too.
     */
    iap.package = package;
    iap.UI = TRUE;

    TRACE("%p %s %i\n", package, debugstr_w(szTable), iSequenceMode );

    r = MSI_OpenQuery( package->db, &view, query, szTable );
    if (r == ERROR_SUCCESS)
    {
        r = MSI_IterateRecords( view, NULL, ITERATE_Actions, &iap );
        msiobj_release(&view->hdr);
    }

    return r;
}

static UINT ACTION_ProcessExecSequence(MSIPACKAGE *package, BOOL UIran)
{
    MSIQUERY * view;
    UINT rc;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','I','n','s','t','a','l','l','E','x','e','c','u','t','e',
         'S','e','q','u','e','n','c','e','`',' ', 'W','H','E','R','E',' ',
         '`','S','e','q','u','e','n','c','e','`',' ', '>',' ','%','i',' ',
         'O','R','D','E','R',' ', 'B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`',0 };
    MSIRECORD * row = 0;
    static const WCHAR IVQuery[] =
        {'S','E','L','E','C','T',' ','`','S','e','q','u','e','n','c','e','`',
         ' ', 'F','R','O','M',' ','`','I','n','s','t','a','l','l',
         'E','x','e','c','u','t','e','S','e','q','u','e','n','c','e','`',' ',
         'W','H','E','R','E',' ','`','A','c','t','i','o','n','`',' ','=',
         ' ','\'', 'I','n','s','t','a','l','l',
         'V','a','l','i','d','a','t','e','\'', 0};
    INT seq = 0;
    iterate_action_param iap;

    iap.package = package;
    iap.UI = FALSE;

    if (package->script->ExecuteSequenceRun)
    {
        TRACE("Execute Sequence already Run\n");
        return ERROR_SUCCESS;
    }

    package->script->ExecuteSequenceRun = TRUE;

    /* get the sequence number */
    if (UIran)
    {
        row = MSI_QueryGetRecord(package->db, IVQuery);
        if( !row )
            return ERROR_FUNCTION_FAILED;
        seq = MSI_RecordGetInteger(row,1);
        msiobj_release(&row->hdr);
    }

    rc = MSI_OpenQuery(package->db, &view, ExecSeqQuery, seq);
    if (rc == ERROR_SUCCESS)
    {
        TRACE("Running the actions\n");

        rc = MSI_IterateRecords(view, NULL, ITERATE_Actions, &iap);
        msiobj_release(&view->hdr);
    }

    return rc;
}

static UINT ACTION_ProcessUISequence(MSIPACKAGE *package)
{
    MSIQUERY * view;
    UINT rc;
    static const WCHAR ExecSeqQuery [] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','s','t','a','l','l',
         'U','I','S','e','q','u','e','n','c','e','`',
         ' ','W','H','E','R','E',' ', 
         '`','S','e','q','u','e','n','c','e','`',' ',
         '>',' ','0',' ','O','R','D','E','R',' ','B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`',0};
    iterate_action_param iap;

    iap.package = package;
    iap.UI = TRUE;

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    
    if (rc == ERROR_SUCCESS)
    {
        TRACE("Running the actions\n"); 

        rc = MSI_IterateRecords(view, NULL, ITERATE_Actions, &iap);
        msiobj_release(&view->hdr);
    }

    return rc;
}

/********************************************************
 * ACTION helper functions and functions that perform the actions
 *******************************************************/
static BOOL ACTION_HandleCustomAction( MSIPACKAGE* package, LPCWSTR action,
                                       UINT* rc, UINT script, BOOL force )
{
    BOOL ret=FALSE;
    UINT arc;

    arc = ACTION_CustomAction(package, action, script, force);

    if (arc != ERROR_CALL_NOT_IMPLEMENTED)
    {
        *rc = arc;
        ret = TRUE;
    }
    return ret;
}

/* 
 * A lot of actions are really important even if they don't do anything
 * explicit... Lots of properties are set at the beginning of the installation
 * CostFinalize does a bunch of work to translate the directories and such
 * 
 * But until I get write access to the database that is hard, so I am going to
 * hack it to see if I can get something to run.
 */
UINT ACTION_PerformAction(MSIPACKAGE *package, const WCHAR *action, UINT script, BOOL force)
{
    UINT rc = ERROR_SUCCESS; 
    BOOL handled;

    TRACE("Performing action (%s)\n",debugstr_w(action));

    handled = ACTION_HandleStandardAction(package, action, &rc, force);

    if (!handled)
        handled = ACTION_HandleCustomAction(package, action, &rc, script, force);

    if (!handled)
    {
        WARN("unhandled msi action %s\n",debugstr_w(action));
        rc = ERROR_FUNCTION_NOT_CALLED;
    }

    return rc;
}

UINT ACTION_PerformUIAction(MSIPACKAGE *package, const WCHAR *action, UINT script)
{
    UINT rc = ERROR_SUCCESS;
    BOOL handled = FALSE;

    TRACE("Performing action (%s)\n",debugstr_w(action));

    handled = ACTION_HandleStandardAction(package, action, &rc,TRUE);

    if (!handled)
        handled = ACTION_HandleCustomAction(package, action, &rc, script, FALSE);

    if( !handled && ACTION_DialogBox(package,action) == ERROR_SUCCESS )
        handled = TRUE;

    if (!handled)
    {
        WARN("unhandled msi action %s\n",debugstr_w(action));
        rc = ERROR_FUNCTION_NOT_CALLED;
    }

    return rc;
}


/*
 * Actual Action Handlers
 */

static UINT ITERATE_CreateFolders(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPCWSTR dir;
    LPWSTR full_path;
    MSIRECORD *uirow;
    MSIFOLDER *folder;

    dir = MSI_RecordGetString(row,1);
    if (!dir)
    {
        ERR("Unable to get folder id\n");
        return ERROR_SUCCESS;
    }

    full_path = resolve_folder(package,dir,FALSE,FALSE,TRUE,&folder);
    if (!full_path)
    {
        ERR("Unable to resolve folder id %s\n",debugstr_w(dir));
        return ERROR_SUCCESS;
    }

    TRACE("Folder is %s\n",debugstr_w(full_path));

    /* UI stuff */
    uirow = MSI_CreateRecord(1);
    MSI_RecordSetStringW(uirow,1,full_path);
    ui_actiondata(package,szCreateFolders,uirow);
    msiobj_release( &uirow->hdr );

    if (folder->State == 0)
        create_full_pathW(full_path);

    folder->State = 3;

    msi_free(full_path);
    return ERROR_SUCCESS;
}

/* FIXME: probably should merge this with the above function */
static UINT msi_create_directory( MSIPACKAGE* package, LPCWSTR dir )
{
    UINT rc = ERROR_SUCCESS;
    MSIFOLDER *folder;
    LPWSTR install_path;

    install_path = resolve_folder(package, dir, FALSE, FALSE, TRUE, &folder);
    if (!install_path)
        return ERROR_FUNCTION_FAILED; 

    /* create the path */
    if (folder->State == 0)
    {
        create_full_pathW(install_path);
        folder->State = 2;
    }
    msi_free(install_path);

    return rc;
}

UINT msi_create_component_directories( MSIPACKAGE *package )
{
    MSICOMPONENT *comp;

    /* create all the folders required by the components are going to install */
    LIST_FOR_EACH_ENTRY( comp, &package->components, MSICOMPONENT, entry )
    {
        if (!ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL))
            continue;
        msi_create_directory( package, comp->Directory );
    }

    return ERROR_SUCCESS;
}

/*
 * Also we cannot enable/disable components either, so for now I am just going 
 * to do all the directories for all the components.
 */
static UINT ACTION_CreateFolders(MSIPACKAGE *package)
{
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ',
         '`','D','i','r','e','c','t','o','r','y','_','`',
         ' ','F','R','O','M',' ',
         '`','C','r','e','a','t','e','F','o','l','d','e','r','`',0 };
    UINT rc;
    MSIQUERY *view;

    /* create all the empty folders specified in the CreateFolder table */
    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view );
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_CreateFolders, package);
    msiobj_release(&view->hdr);

    msi_create_component_directories( package );

    return rc;
}

static UINT load_component( MSIRECORD *row, LPVOID param )
{
    MSIPACKAGE *package = param;
    MSICOMPONENT *comp;

    comp = msi_alloc_zero( sizeof(MSICOMPONENT) );
    if (!comp)
        return ERROR_FUNCTION_FAILED;

    list_add_tail( &package->components, &comp->entry );

    /* fill in the data */
    comp->Component = msi_dup_record_field( row, 1 );

    TRACE("Loading Component %s\n", debugstr_w(comp->Component));

    comp->ComponentId = msi_dup_record_field( row, 2 );
    comp->Directory = msi_dup_record_field( row, 3 );
    comp->Attributes = MSI_RecordGetInteger(row,4);
    comp->Condition = msi_dup_record_field( row, 5 );
    comp->KeyPath = msi_dup_record_field( row, 6 );

    comp->Installed = INSTALLSTATE_UNKNOWN;
    msi_component_set_state( comp, INSTALLSTATE_UNKNOWN );

    return ERROR_SUCCESS;
}

static UINT load_all_components( MSIPACKAGE *package )
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R', 'O','M',' ', 
         '`','C','o','m','p','o','n','e','n','t','`',0 };
    MSIQUERY *view;
    UINT r;

    if (!list_empty(&package->components))
        return ERROR_SUCCESS;

    r = MSI_DatabaseOpenViewW( package->db, query, &view );
    if (r != ERROR_SUCCESS)
        return r;

    r = MSI_IterateRecords(view, NULL, load_component, package);
    msiobj_release(&view->hdr);
    return r;
}

typedef struct {
    MSIPACKAGE *package;
    MSIFEATURE *feature;
} _ilfs;

static UINT add_feature_component( MSIFEATURE *feature, MSICOMPONENT *comp )
{
    ComponentList *cl;

    cl = msi_alloc( sizeof (*cl) );
    if ( !cl )
        return ERROR_NOT_ENOUGH_MEMORY;
    cl->component = comp;
    list_add_tail( &feature->Components, &cl->entry );

    return ERROR_SUCCESS;
}

static UINT add_feature_child( MSIFEATURE *parent, MSIFEATURE *child )
{
    FeatureList *fl;

    fl = msi_alloc( sizeof(*fl) );
    if ( !fl )
        return ERROR_NOT_ENOUGH_MEMORY;
    fl->feature = child;
    list_add_tail( &parent->Children, &fl->entry );

    return ERROR_SUCCESS;
}

static UINT iterate_load_featurecomponents(MSIRECORD *row, LPVOID param)
{
    _ilfs* ilfs= (_ilfs*)param;
    LPCWSTR component;
    MSICOMPONENT *comp;

    component = MSI_RecordGetString(row,1);

    /* check to see if the component is already loaded */
    comp = get_loaded_component( ilfs->package, component );
    if (!comp)
    {
        ERR("unknown component %s\n", debugstr_w(component));
        return ERROR_FUNCTION_FAILED;
    }

    add_feature_component( ilfs->feature, comp );
    comp->Enabled = TRUE;

    return ERROR_SUCCESS;
}

static MSIFEATURE *find_feature_by_name( MSIPACKAGE *package, LPCWSTR name )
{
    MSIFEATURE *feature;

    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        if ( !lstrcmpW( feature->Feature, name ) )
            return feature;
    }

    return NULL;
}

static UINT load_feature(MSIRECORD * row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    MSIFEATURE* feature;
    static const WCHAR Query1[] = 
        {'S','E','L','E','C','T',' ',
         '`','C','o','m','p','o','n','e','n','t','_','`',
         ' ','F','R','O','M',' ','`','F','e','a','t','u','r','e',
         'C','o','m','p','o','n','e','n','t','s','`',' ',
         'W','H','E','R','E',' ',
         '`','F','e', 'a','t','u','r','e','_','`',' ','=','\'','%','s','\'',0};
    MSIQUERY * view;
    UINT    rc;
    _ilfs ilfs;

    /* fill in the data */

    feature = msi_alloc_zero( sizeof (MSIFEATURE) );
    if (!feature)
        return ERROR_NOT_ENOUGH_MEMORY;

    list_init( &feature->Children );
    list_init( &feature->Components );
    
    feature->Feature = msi_dup_record_field( row, 1 );

    TRACE("Loading feature %s\n",debugstr_w(feature->Feature));

    feature->Feature_Parent = msi_dup_record_field( row, 2 );
    feature->Title = msi_dup_record_field( row, 3 );
    feature->Description = msi_dup_record_field( row, 4 );

    if (!MSI_RecordIsNull(row,5))
        feature->Display = MSI_RecordGetInteger(row,5);
  
    feature->Level= MSI_RecordGetInteger(row,6);
    feature->Directory = msi_dup_record_field( row, 7 );
    feature->Attributes = MSI_RecordGetInteger(row,8);

    feature->Installed = INSTALLSTATE_UNKNOWN;
    msi_feature_set_state( feature, INSTALLSTATE_UNKNOWN );

    list_add_tail( &package->features, &feature->entry );

    /* load feature components */

    rc = MSI_OpenQuery( package->db, &view, Query1, feature->Feature );
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    ilfs.package = package;
    ilfs.feature = feature;

    MSI_IterateRecords(view, NULL, iterate_load_featurecomponents , &ilfs);
    msiobj_release(&view->hdr);

    return ERROR_SUCCESS;
}

static UINT find_feature_children(MSIRECORD * row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    MSIFEATURE *parent, *child;

    child = find_feature_by_name( package, MSI_RecordGetString( row, 1 ) );
    if (!child)
        return ERROR_FUNCTION_FAILED;

    if (!child->Feature_Parent)
        return ERROR_SUCCESS;

    parent = find_feature_by_name( package, child->Feature_Parent );
    if (!parent)
        return ERROR_FUNCTION_FAILED;

    add_feature_child( parent, child );
    return ERROR_SUCCESS;
}

static UINT load_all_features( MSIPACKAGE *package )
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
        '`','F','e','a','t','u','r','e','`',' ','O','R','D','E','R',
        ' ','B','Y',' ','`','D','i','s','p','l','a','y','`',0};
    MSIQUERY *view;
    UINT r;

    if (!list_empty(&package->features))
        return ERROR_SUCCESS;
 
    r = MSI_DatabaseOpenViewW( package->db, query, &view );
    if (r != ERROR_SUCCESS)
        return r;

    r = MSI_IterateRecords( view, NULL, load_feature, package );
    if (r != ERROR_SUCCESS)
        return r;

    r = MSI_IterateRecords( view, NULL, find_feature_children, package );
    msiobj_release( &view->hdr );

    return r;
}

static LPWSTR folder_split_path(LPWSTR p, WCHAR ch)
{
    if (!p)
        return p;
    p = strchrW(p, ch);
    if (!p)
        return p;
    *p = 0;
    return p+1;
}

static UINT load_file_hash(MSIPACKAGE *package, MSIFILE *file)
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
        '`','M','s','i','F','i','l','e','H','a','s','h','`',' ',
        'W','H','E','R','E',' ','`','F','i','l','e','_','`',' ','=',' ','\'','%','s','\'',0};
    MSIQUERY *view = NULL;
    MSIRECORD *row = NULL;
    UINT r;

    TRACE("%s\n", debugstr_w(file->File));

    r = MSI_OpenQuery(package->db, &view, query, file->File);
    if (r != ERROR_SUCCESS)
        goto done;

    r = MSI_ViewExecute(view, NULL);
    if (r != ERROR_SUCCESS)
        goto done;

    r = MSI_ViewFetch(view, &row);
    if (r != ERROR_SUCCESS)
        goto done;

    file->hash.dwFileHashInfoSize = sizeof(MSIFILEHASHINFO);
    file->hash.dwData[0] = MSI_RecordGetInteger(row, 3);
    file->hash.dwData[1] = MSI_RecordGetInteger(row, 4);
    file->hash.dwData[2] = MSI_RecordGetInteger(row, 5);
    file->hash.dwData[3] = MSI_RecordGetInteger(row, 6);

done:
    if (view) msiobj_release(&view->hdr);
    if (row) msiobj_release(&row->hdr);
    return r;
}

static UINT load_file(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    LPCWSTR component;
    MSIFILE *file;

    /* fill in the data */

    file = msi_alloc_zero( sizeof (MSIFILE) );
    if (!file)
        return ERROR_NOT_ENOUGH_MEMORY;
 
    file->File = msi_dup_record_field( row, 1 );

    component = MSI_RecordGetString( row, 2 );
    file->Component = get_loaded_component( package, component );

    if (!file->Component)
        ERR("Unfound Component %s\n",debugstr_w(component));

    file->FileName = msi_dup_record_field( row, 3 );
    reduce_to_longfilename( file->FileName );

    file->ShortName = msi_dup_record_field( row, 3 );
    file->LongName = strdupW( folder_split_path(file->ShortName, '|'));
    
    file->FileSize = MSI_RecordGetInteger( row, 4 );
    file->Version = msi_dup_record_field( row, 5 );
    file->Language = msi_dup_record_field( row, 6 );
    file->Attributes = MSI_RecordGetInteger( row, 7 );
    file->Sequence = MSI_RecordGetInteger( row, 8 );

    file->state = msifs_invalid;

    /* if the compressed bits are not set in the file attributes,
     * then read the information from the package word count property
     */
    if (file->Attributes & msidbFileAttributesCompressed)
    {
        file->IsCompressed = TRUE;
    }
    else if (file->Attributes & msidbFileAttributesNoncompressed)
    {
        file->IsCompressed = FALSE;
    }
    else
    {
        file->IsCompressed = package->WordCount & MSIWORDCOUNT_COMPRESSED;
    }

    load_file_hash(package, file);

    TRACE("File Loaded (%s)\n",debugstr_w(file->File));  

    list_add_tail( &package->files, &file->entry );
 
    return ERROR_SUCCESS;
}

static UINT load_all_files(MSIPACKAGE *package)
{
    MSIQUERY * view;
    UINT rc;
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','F','i','l','e','`',' ', 'O','R','D','E','R',' ','B','Y',' ',
         '`','S','e','q','u','e','n','c','e','`', 0};

    if (!list_empty(&package->files))
        return ERROR_SUCCESS;

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, load_file, package);
    msiobj_release(&view->hdr);

    return ERROR_SUCCESS;
}

static UINT load_folder( MSIRECORD *row, LPVOID param )
{
    MSIPACKAGE *package = param;
    static const WCHAR szDot[] = { '.',0 };
    static WCHAR szEmpty[] = { 0 };
    LPWSTR p, tgt_short, tgt_long, src_short, src_long;
    MSIFOLDER *folder;

    folder = msi_alloc_zero( sizeof (MSIFOLDER) );
    if (!folder)
        return ERROR_NOT_ENOUGH_MEMORY;

    folder->Directory = msi_dup_record_field( row, 1 );

    TRACE("%s\n", debugstr_w(folder->Directory));

    p = msi_dup_record_field(row, 3);

    /* split src and target dir */
    tgt_short = p;
    src_short = folder_split_path( p, ':' );

    /* split the long and short paths */
    tgt_long = folder_split_path( tgt_short, '|' );
    src_long = folder_split_path( src_short, '|' );

    /* check for no-op dirs */
    if (!lstrcmpW(szDot, tgt_short))
        tgt_short = szEmpty;
    if (!lstrcmpW(szDot, src_short))
        src_short = szEmpty;

    if (!tgt_long)
        tgt_long = tgt_short;

    if (!src_short) {
        src_short = tgt_short;
        src_long = tgt_long;
    }

    if (!src_long)
        src_long = src_short;

    /* FIXME: use the target short path too */
    folder->TargetDefault = strdupW(tgt_long);
    folder->SourceShortPath = strdupW(src_short);
    folder->SourceLongPath = strdupW(src_long);
    msi_free(p);

    TRACE("TargetDefault = %s\n",debugstr_w( folder->TargetDefault ));
    TRACE("SourceLong = %s\n", debugstr_w( folder->SourceLongPath ));
    TRACE("SourceShort = %s\n", debugstr_w( folder->SourceShortPath ));

    folder->Parent = msi_dup_record_field( row, 2 );

    folder->Property = msi_dup_property( package, folder->Directory );

    list_add_tail( &package->folders, &folder->entry );

    TRACE("returning %p\n", folder);

    return ERROR_SUCCESS;
}

static UINT load_all_folders( MSIPACKAGE *package )
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R', 'O','M',' ',
         '`','D','i','r','e','c','t','o','r','y','`',0 };
    MSIQUERY *view;
    UINT r;

    if (!list_empty(&package->folders))
        return ERROR_SUCCESS;

    r = MSI_DatabaseOpenViewW( package->db, query, &view );
    if (r != ERROR_SUCCESS)
        return r;

    r = MSI_IterateRecords(view, NULL, load_folder, package);
    msiobj_release(&view->hdr);
    return r;
}

/*
 * I am not doing any of the costing functionality yet.
 * Mostly looking at doing the Component and Feature loading
 *
 * The native MSI does A LOT of modification to tables here. Mostly adding
 * a lot of temporary columns to the Feature and Component tables.
 *
 *    note: Native msi also tracks the short filename. But I am only going to
 *          track the long ones.  Also looking at this directory table
 *          it appears that the directory table does not get the parents
 *          resolved base on property only based on their entries in the
 *          directory table.
 */
static UINT ACTION_CostInitialize(MSIPACKAGE *package)
{
    static const WCHAR szCosting[] =
        {'C','o','s','t','i','n','g','C','o','m','p','l','e','t','e',0 };
    static const WCHAR szZero[] = { '0', 0 };

    MSI_SetPropertyW(package, szCosting, szZero);
    MSI_SetPropertyW(package, cszRootDrive, c_colon);

    load_all_components( package );
    load_all_features( package );
    load_all_files( package );
    load_all_folders( package );

    return ERROR_SUCCESS;
}

static UINT execute_script(MSIPACKAGE *package, UINT script )
{
    UINT i;
    UINT rc = ERROR_SUCCESS;

    TRACE("Executing Script %i\n",script);

    if (!package->script)
    {
        ERR("no script!\n");
        return ERROR_FUNCTION_FAILED;
    }

    for (i = 0; i < package->script->ActionCount[script]; i++)
    {
        LPWSTR action;
        action = package->script->Actions[script][i];
        ui_actionstart(package, action);
        TRACE("Executing Action (%s)\n",debugstr_w(action));
        rc = ACTION_PerformAction(package, action, script, TRUE);
        if (rc != ERROR_SUCCESS)
            break;
    }
    msi_free_action_script(package, script);
    return rc;
}

static UINT ACTION_FileCost(MSIPACKAGE *package)
{
    return ERROR_SUCCESS;
}

static void ACTION_GetComponentInstallStates(MSIPACKAGE *package)
{
    MSICOMPONENT *comp;

    LIST_FOR_EACH_ENTRY( comp, &package->components, MSICOMPONENT, entry )
    {
        INSTALLSTATE res;

        if (!comp->ComponentId)
            continue;

        res = MsiGetComponentPathW( package->ProductCode,
                                    comp->ComponentId, NULL, NULL);
        if (res < 0)
            res = INSTALLSTATE_ABSENT;
        comp->Installed = res;
    }
}

/* scan for and update current install states */
static void ACTION_UpdateFeatureInstallStates(MSIPACKAGE *package)
{
    MSICOMPONENT *comp;
    MSIFEATURE *feature;

    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        ComponentList *cl;
        INSTALLSTATE res = INSTALLSTATE_ABSENT;

        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            comp= cl->component;

            if (!comp->ComponentId)
            {
                res = INSTALLSTATE_ABSENT;
                break;
            }

            if (res == INSTALLSTATE_ABSENT)
                res = comp->Installed;
            else
            {
                if (res == comp->Installed)
                    continue;

                if (res != INSTALLSTATE_DEFAULT && res != INSTALLSTATE_LOCAL &&
                    res != INSTALLSTATE_SOURCE)
                {
                    res = INSTALLSTATE_INCOMPLETE;
                }
            }
        }
        feature->Installed = res;
    }
}

static BOOL process_state_property (MSIPACKAGE* package, LPCWSTR property, 
                                    INSTALLSTATE state)
{
    static const WCHAR all[]={'A','L','L',0};
    LPWSTR override;
    MSIFEATURE *feature;

    override = msi_dup_property( package, property );
    if (!override)
        return FALSE;

    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        if (strcmpiW(override,all)==0)
            msi_feature_set_state( feature, state );
        else
        {
            LPWSTR ptr = override;
            LPWSTR ptr2 = strchrW(override,',');

            while (ptr)
            {
                if ((ptr2 && strncmpW(ptr,feature->Feature, ptr2-ptr)==0)
                    || (!ptr2 && strcmpW(ptr,feature->Feature)==0))
                {
                    msi_feature_set_state( feature, state );
                    break;
                }
                if (ptr2)
                {
                    ptr=ptr2+1;
                    ptr2 = strchrW(ptr,',');
                }
                else
                    break;
            }
        }
    }
    msi_free(override);

    return TRUE;
}

UINT MSI_SetFeatureStates(MSIPACKAGE *package)
{
    int install_level;
    static const WCHAR szlevel[] =
        {'I','N','S','T','A','L','L','L','E','V','E','L',0};
    static const WCHAR szAddLocal[] =
        {'A','D','D','L','O','C','A','L',0};
    static const WCHAR szAddSource[] =
        {'A','D','D','S','O','U','R','C','E',0};
    static const WCHAR szRemove[] =
        {'R','E','M','O','V','E',0};
    static const WCHAR szReinstall[] =
        {'R','E','I','N','S','T','A','L','L',0};
    BOOL override = FALSE;
    MSICOMPONENT* component;
    MSIFEATURE *feature;


    /* I do not know if this is where it should happen.. but */

    TRACE("Checking Install Level\n");

    install_level = msi_get_property_int( package, szlevel, 1 );

    /* ok here is the _real_ rub
     * all these activation/deactivation things happen in order and things
     * later on the list override things earlier on the list.
     * 1) INSTALLLEVEL processing
     * 2) ADDLOCAL
     * 3) REMOVE
     * 4) ADDSOURCE
     * 5) ADDDEFAULT
     * 6) REINSTALL
     * 7) COMPADDLOCAL
     * 8) COMPADDSOURCE
     * 9) FILEADDLOCAL
     * 10) FILEADDSOURCE
     * 11) FILEADDDEFAULT
     * I have confirmed that if ADDLOCAL is stated then the INSTALLLEVEL is
     * ignored for all the features. seems strange, especially since it is not
     * documented anywhere, but it is how it works.
     *
     * I am still ignoring a lot of these. But that is ok for now, ADDLOCAL and
     * REMOVE are the big ones, since we don't handle administrative installs
     * yet anyway.
     */
    override |= process_state_property(package,szAddLocal,INSTALLSTATE_LOCAL);
    override |= process_state_property(package,szRemove,INSTALLSTATE_ABSENT);
    override |= process_state_property(package,szAddSource,INSTALLSTATE_SOURCE);
    override |= process_state_property(package,szReinstall,INSTALLSTATE_LOCAL);

    if (!override)
    {
        LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
        {
            BOOL feature_state = ((feature->Level > 0) &&
                             (feature->Level <= install_level));

            if ((feature_state) && (feature->Action == INSTALLSTATE_UNKNOWN))
            {
                if (feature->Attributes & msidbFeatureAttributesFavorSource)
                    msi_feature_set_state( feature, INSTALLSTATE_SOURCE );
                else if (feature->Attributes & msidbFeatureAttributesFavorAdvertise)
                    msi_feature_set_state( feature, INSTALLSTATE_ADVERTISED );
                else
                    msi_feature_set_state( feature, INSTALLSTATE_LOCAL );
            }
        }

        /* disable child features of unselected parent features */
        LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
        {
            FeatureList *fl;

            if (feature->Level > 0 && feature->Level <= install_level)
                continue;

            LIST_FOR_EACH_ENTRY( fl, &feature->Children, FeatureList, entry )
                msi_feature_set_state( fl->feature, INSTALLSTATE_UNKNOWN );
        }
    }
    else
    {
        /* set the Preselected Property */
        static const WCHAR szPreselected[] = {'P','r','e','s','e','l','e','c','t','e','d',0};
        static const WCHAR szOne[] = { '1', 0 };

        MSI_SetPropertyW(package,szPreselected,szOne);
    }

    /*
     * now we want to enable or disable components base on feature
     */

    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        ComponentList *cl;

        TRACE("Examining Feature %s (Installed %i, Action %i)\n",
              debugstr_w(feature->Feature), feature->Installed, feature->Action);

        /* features with components that have compressed files are made local */
        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            if (cl->component->Enabled &&
                cl->component->ForceLocalState &&
                feature->Action == INSTALLSTATE_SOURCE)
            {
                msi_feature_set_state( feature, INSTALLSTATE_LOCAL );
                break;
            }
        }

        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            component = cl->component;

            if (!component->Enabled)
                continue;

            switch (feature->Action)
            {
            case INSTALLSTATE_ABSENT:
                component->anyAbsent = 1;
                break;
            case INSTALLSTATE_ADVERTISED:
                component->hasAdvertiseFeature = 1;
                break;
            case INSTALLSTATE_SOURCE:
                component->hasSourceFeature = 1;
                break;
            case INSTALLSTATE_LOCAL:
                component->hasLocalFeature = 1;
                break;
            case INSTALLSTATE_DEFAULT:
                if (feature->Attributes & msidbFeatureAttributesFavorAdvertise)
                    component->hasAdvertiseFeature = 1;
                else if (feature->Attributes & msidbFeatureAttributesFavorSource)
                    component->hasSourceFeature = 1;
                else
                    component->hasLocalFeature = 1;
                break;
            default:
                break;
            }
        }
    }

    LIST_FOR_EACH_ENTRY( component, &package->components, MSICOMPONENT, entry )
    {
        /* if the component isn't enabled, leave it alone */
        if (!component->Enabled)
            continue;

        /* check if it's local or source */
        if (!(component->Attributes & msidbComponentAttributesOptional) &&
             (component->hasLocalFeature || component->hasSourceFeature))
        {
            if ((component->Attributes & msidbComponentAttributesSourceOnly) &&
                 !component->ForceLocalState)
                msi_component_set_state( component, INSTALLSTATE_SOURCE );
            else
                msi_component_set_state( component, INSTALLSTATE_LOCAL );
            continue;
        }

        /* if any feature is local, the component must be local too */
        if (component->hasLocalFeature)
        {
            msi_component_set_state( component, INSTALLSTATE_LOCAL );
            continue;
        }

        if (component->hasSourceFeature)
        {
            msi_component_set_state( component, INSTALLSTATE_SOURCE );
            continue;
        }

        if (component->hasAdvertiseFeature)
        {
            msi_component_set_state( component, INSTALLSTATE_ADVERTISED );
            continue;
        }

        TRACE("nobody wants component %s\n", debugstr_w(component->Component));
        if (component->anyAbsent)
            msi_component_set_state(component, INSTALLSTATE_ABSENT);
    }

    LIST_FOR_EACH_ENTRY( component, &package->components, MSICOMPONENT, entry )
    {
        if (component->Action == INSTALLSTATE_DEFAULT)
        {
            TRACE("%s was default, setting to local\n", debugstr_w(component->Component));
            msi_component_set_state( component, INSTALLSTATE_LOCAL );
        }

        TRACE("Result: Component %s (Installed %i, Action %i)\n",
            debugstr_w(component->Component), component->Installed, component->Action);
    }


    return ERROR_SUCCESS;
}

static UINT ITERATE_CostFinalizeDirectories(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPCWSTR name;
    LPWSTR path;
    MSIFOLDER *f;

    name = MSI_RecordGetString(row,1);

    f = get_loaded_folder(package, name);
    if (!f) return ERROR_SUCCESS;

    /* reset the ResolvedTarget */
    msi_free(f->ResolvedTarget);
    f->ResolvedTarget = NULL;

    /* This helper function now does ALL the work */
    TRACE("Dir %s ...\n",debugstr_w(name));
    path = resolve_folder(package,name,FALSE,TRUE,TRUE,NULL);
    TRACE("resolves to %s\n",debugstr_w(path));
    msi_free(path);

    return ERROR_SUCCESS;
}

static UINT ITERATE_CostFinalizeConditions(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPCWSTR name;
    MSIFEATURE *feature;

    name = MSI_RecordGetString( row, 1 );

    feature = get_loaded_feature( package, name );
    if (!feature)
        ERR("FAILED to find loaded feature %s\n",debugstr_w(name));
    else
    {
        LPCWSTR Condition;
        Condition = MSI_RecordGetString(row,3);

        if (MSI_EvaluateConditionW(package,Condition) == MSICONDITION_TRUE)
        {
            int level = MSI_RecordGetInteger(row,2);
            TRACE("Reseting feature %s to level %i\n", debugstr_w(name), level);
            feature->Level = level;
        }
    }
    return ERROR_SUCCESS;
}

static LPWSTR msi_get_disk_file_version( LPCWSTR filename )
{
    static const WCHAR name_fmt[] =
        {'%','u','.','%','u','.','%','u','.','%','u',0};
    static WCHAR name[] = {'\\',0};
    VS_FIXEDFILEINFO *lpVer;
    WCHAR filever[0x100];
    LPVOID version;
    DWORD versize;
    DWORD handle;
    UINT sz;

    TRACE("%s\n", debugstr_w(filename));

    versize = GetFileVersionInfoSizeW( filename, &handle );
    if (!versize)
        return NULL;

    version = msi_alloc( versize );
    GetFileVersionInfoW( filename, 0, versize, version );

    if (!VerQueryValueW( version, name, (LPVOID*)&lpVer, &sz ))
    {
        msi_free( version );
        return NULL;
    }

    sprintfW( filever, name_fmt,
        HIWORD(lpVer->dwFileVersionMS),
        LOWORD(lpVer->dwFileVersionMS),
        HIWORD(lpVer->dwFileVersionLS),
        LOWORD(lpVer->dwFileVersionLS));

    msi_free( version );

    return strdupW( filever );
}

static UINT msi_check_file_install_states( MSIPACKAGE *package )
{
    LPWSTR file_version;
    MSIFILE *file;

    LIST_FOR_EACH_ENTRY( file, &package->files, MSIFILE, entry )
    {
        MSICOMPONENT* comp = file->Component;
        LPWSTR p;

        if (!comp)
            continue;

        if (file->IsCompressed)
            comp->ForceLocalState = TRUE;

        /* calculate target */
        p = resolve_folder(package, comp->Directory, FALSE, FALSE, TRUE, NULL);

        msi_free(file->TargetPath);

        TRACE("file %s is named %s\n",
               debugstr_w(file->File), debugstr_w(file->FileName));

        file->TargetPath = build_directory_name(2, p, file->FileName);

        msi_free(p);

        TRACE("file %s resolves to %s\n",
               debugstr_w(file->File), debugstr_w(file->TargetPath));

        /* don't check files of components that aren't installed */
        if (comp->Installed == INSTALLSTATE_UNKNOWN ||
            comp->Installed == INSTALLSTATE_ABSENT)
        {
            file->state = msifs_missing;  /* assume files are missing */
            continue;
        }

        if (GetFileAttributesW(file->TargetPath) == INVALID_FILE_ATTRIBUTES)
        {
            file->state = msifs_missing;
            comp->Cost += file->FileSize;
            comp->Installed = INSTALLSTATE_INCOMPLETE;
            continue;
        }

        if (file->Version &&
            (file_version = msi_get_disk_file_version( file->TargetPath )))
        {
            TRACE("new %s old %s\n", debugstr_w(file->Version),
                  debugstr_w(file_version));
            /* FIXME: seems like a bad way to compare version numbers */
            if (lstrcmpiW(file_version, file->Version)<0)
            {
                file->state = msifs_overwrite;
                comp->Cost += file->FileSize;
                comp->Installed = INSTALLSTATE_INCOMPLETE;
            }
            else
                file->state = msifs_present;
            msi_free( file_version );
        }
        else
            file->state = msifs_present;
    }

    return ERROR_SUCCESS;
}

/*
 * A lot is done in this function aside from just the costing.
 * The costing needs to be implemented at some point but for now I am going
 * to focus on the directory building
 *
 */
static UINT ACTION_CostFinalize(MSIPACKAGE *package)
{
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','D','i','r','e','c','t','o','r','y','`',0};
    static const WCHAR ConditionQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','C','o','n','d','i','t','i','o','n','`',0};
    static const WCHAR szCosting[] =
        {'C','o','s','t','i','n','g','C','o','m','p','l','e','t','e',0 };
    static const WCHAR szlevel[] =
        {'I','N','S','T','A','L','L','L','E','V','E','L',0};
    static const WCHAR szOne[] = { '1', 0 };
    MSICOMPONENT *comp;
    UINT rc;
    MSIQUERY * view;
    LPWSTR level;

    if ( 1 == msi_get_property_int( package, szCosting, 0 ) )
        return ERROR_SUCCESS;

    TRACE("Building Directory properties\n");

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_IterateRecords(view, NULL, ITERATE_CostFinalizeDirectories,
                        package);
        msiobj_release(&view->hdr);
    }

    /* read components states from the registry */
    ACTION_GetComponentInstallStates(package);

    TRACE("File calculations\n");
    msi_check_file_install_states( package );

    TRACE("Evaluating Condition Table\n");

    rc = MSI_DatabaseOpenViewW(package->db, ConditionQuery, &view);
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_IterateRecords(view, NULL, ITERATE_CostFinalizeConditions,
                    package);
        msiobj_release(&view->hdr);
    }

    TRACE("Enabling or Disabling Components\n");
    LIST_FOR_EACH_ENTRY( comp, &package->components, MSICOMPONENT, entry )
    {
        if (MSI_EvaluateConditionW(package, comp->Condition) == MSICONDITION_FALSE)
        {
            TRACE("Disabling component %s\n", debugstr_w(comp->Component));
            comp->Enabled = FALSE;
        }
    }

    MSI_SetPropertyW(package,szCosting,szOne);
    /* set default run level if not set */
    level = msi_dup_property( package, szlevel );
    if (!level)
        MSI_SetPropertyW(package,szlevel, szOne);
    msi_free(level);

    ACTION_UpdateFeatureInstallStates(package);

    return MSI_SetFeatureStates(package);
}

/* OK this value is "interpreted" and then formatted based on the 
   first few characters */
static LPSTR parse_value(MSIPACKAGE *package, LPCWSTR value, DWORD *type, 
                         DWORD *size)
{
    LPSTR data = NULL;

    if (value[0]=='#' && value[1]!='#' && value[1]!='%')
    {
        if (value[1]=='x')
        {
            LPWSTR ptr;
            CHAR byte[5];
            LPWSTR deformated = NULL;
            int count;

            deformat_string(package, &value[2], &deformated);

            /* binary value type */
            ptr = deformated;
            *type = REG_BINARY;
            if (strlenW(ptr)%2)
                *size = (strlenW(ptr)/2)+1;
            else
                *size = strlenW(ptr)/2;

            data = msi_alloc(*size);

            byte[0] = '0'; 
            byte[1] = 'x'; 
            byte[4] = 0; 
            count = 0;
            /* if uneven pad with a zero in front */
            if (strlenW(ptr)%2)
            {
                byte[2]= '0';
                byte[3]= *ptr;
                ptr++;
                data[count] = (BYTE)strtol(byte,NULL,0);
                count ++;
                TRACE("Uneven byte count\n");
            }
            while (*ptr)
            {
                byte[2]= *ptr;
                ptr++;
                byte[3]= *ptr;
                ptr++;
                data[count] = (BYTE)strtol(byte,NULL,0);
                count ++;
            }
            msi_free(deformated);

            TRACE("Data %i bytes(%i)\n",*size,count);
        }
        else
        {
            LPWSTR deformated;
            LPWSTR p;
            DWORD d = 0;
            deformat_string(package, &value[1], &deformated);

            *type=REG_DWORD; 
            *size = sizeof(DWORD);
            data = msi_alloc(*size);
            p = deformated;
            if (*p == '-')
                p++;
            while (*p)
            {
                if ( (*p < '0') || (*p > '9') )
                    break;
                d *= 10;
                d += (*p - '0');
                p++;
            }
            if (deformated[0] == '-')
                d = -d;
            *(LPDWORD)data = d;
            TRACE("DWORD %i\n",*(LPDWORD)data);

            msi_free(deformated);
        }
    }
    else
    {
        static const WCHAR szMulti[] = {'[','~',']',0};
        LPCWSTR ptr;
        *type=REG_SZ;

        if (value[0]=='#')
        {
            if (value[1]=='%')
            {
                ptr = &value[2];
                *type=REG_EXPAND_SZ;
            }
            else
                ptr = &value[1];
         }
         else
            ptr=value;

        if (strstrW(value,szMulti))
            *type = REG_MULTI_SZ;

        /* remove initial delimiter */
        if (!strncmpW(value, szMulti, 3))
            ptr = value + 3;

        *size = deformat_string(package, ptr,(LPWSTR*)&data);

        /* add double NULL terminator */
        if (*type == REG_MULTI_SZ)
        {
            *size += 2 * sizeof(WCHAR); /* two NULL terminators */
            data = msi_realloc_zero(data, *size);
        }
    }
    return data;
}

static UINT ITERATE_WriteRegistryValues(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    static const WCHAR szHCR[] = 
        {'H','K','E','Y','_','C','L','A','S','S','E','S','_',
         'R','O','O','T','\\',0};
    static const WCHAR szHCU[] =
        {'H','K','E','Y','_','C','U','R','R','E','N','T','_',
         'U','S','E','R','\\',0};
    static const WCHAR szHLM[] =
        {'H','K','E','Y','_','L','O','C','A','L','_',
         'M','A','C','H','I','N','E','\\',0};
    static const WCHAR szHU[] =
        {'H','K','E','Y','_','U','S','E','R','S','\\',0};

    LPSTR value_data = NULL;
    HKEY  root_key, hkey;
    DWORD type,size;
    LPWSTR  deformated;
    LPCWSTR szRoot, component, name, key, value;
    MSICOMPONENT *comp;
    MSIRECORD * uirow;
    LPWSTR uikey;
    INT   root;
    BOOL check_first = FALSE;
    UINT rc;

    ui_progress(package,2,0,0,0);

    value = NULL;
    key = NULL;
    uikey = NULL;
    name = NULL;

    component = MSI_RecordGetString(row, 6);
    comp = get_loaded_component(package,component);
    if (!comp)
        return ERROR_SUCCESS;

    if (!ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL))
    {
        TRACE("Skipping write due to disabled component %s\n",
                        debugstr_w(component));

        comp->Action = comp->Installed;

        return ERROR_SUCCESS;
    }

    comp->Action = INSTALLSTATE_LOCAL;

    name = MSI_RecordGetString(row, 4);
    if( MSI_RecordIsNull(row,5) && name )
    {
        /* null values can have special meanings */
        if (name[0]=='-' && name[1] == 0)
                return ERROR_SUCCESS;
        else if ((name[0]=='+' && name[1] == 0) || 
                 (name[0] == '*' && name[1] == 0))
                name = NULL;
        check_first = TRUE;
    }

    root = MSI_RecordGetInteger(row,2);
    key = MSI_RecordGetString(row, 3);

    /* get the root key */
    switch (root)
    {
        case -1: 
            {
                static const WCHAR szALLUSER[] = {'A','L','L','U','S','E','R','S',0};
                LPWSTR all_users = msi_dup_property( package, szALLUSER );
                if (all_users && all_users[0] == '1')
                {
                    root_key = HKEY_LOCAL_MACHINE;
                    szRoot = szHLM;
                }
                else
                {
                    root_key = HKEY_CURRENT_USER;
                    szRoot = szHCU;
                }
                msi_free(all_users);
            }
                 break;
        case 0:  root_key = HKEY_CLASSES_ROOT; 
                 szRoot = szHCR;
                 break;
        case 1:  root_key = HKEY_CURRENT_USER;
                 szRoot = szHCU;
                 break;
        case 2:  root_key = HKEY_LOCAL_MACHINE;
                 szRoot = szHLM;
                 break;
        case 3:  root_key = HKEY_USERS; 
                 szRoot = szHU;
                 break;
        default:
                 ERR("Unknown root %i\n",root);
                 root_key=NULL;
                 szRoot = NULL;
                 break;
    }
    if (!root_key)
        return ERROR_SUCCESS;

    deformat_string(package, key , &deformated);
    size = strlenW(deformated) + strlenW(szRoot) + 1;
    uikey = msi_alloc(size*sizeof(WCHAR));
    strcpyW(uikey,szRoot);
    strcatW(uikey,deformated);

    if (RegCreateKeyW( root_key, deformated, &hkey))
    {
        ERR("Could not create key %s\n",debugstr_w(deformated));
        msi_free(deformated);
        msi_free(uikey);
        return ERROR_SUCCESS;
    }
    msi_free(deformated);

    value = MSI_RecordGetString(row,5);
    if (value)
        value_data = parse_value(package, value, &type, &size); 
    else
    {
        static const WCHAR szEmpty[] = {0};
        value_data = (LPSTR)strdupW(szEmpty);
        size = sizeof(WCHAR);
        type = REG_SZ;
    }

    deformat_string(package, name, &deformated);

    if (!check_first)
    {
        TRACE("Setting value %s of %s\n",debugstr_w(deformated),
                        debugstr_w(uikey));
        msi_reg_set_val(hkey, deformated, 0, type, (LPBYTE)value_data, size);
    }
    else
    {
        DWORD sz = 0;
        rc = RegQueryValueExW(hkey, deformated, NULL, NULL, NULL, &sz);
        if (rc == ERROR_SUCCESS || rc == ERROR_MORE_DATA)
        {
            TRACE("value %s of %s checked already exists\n",
                            debugstr_w(deformated), debugstr_w(uikey));
        }
        else
        {
            TRACE("Checked and setting value %s of %s\n",
                            debugstr_w(deformated), debugstr_w(uikey));
            if (deformated || size)
                msi_reg_set_val(hkey, deformated, 0, type, (LPBYTE) value_data, size);
        }
    }
    RegCloseKey(hkey);

    uirow = MSI_CreateRecord(3);
    MSI_RecordSetStringW(uirow,2,deformated);
    MSI_RecordSetStringW(uirow,1,uikey);

    if (type == REG_SZ)
        MSI_RecordSetStringW(uirow,3,(LPWSTR)value_data);
    else
        MSI_RecordSetStringW(uirow,3,value);

    ui_actiondata(package,szWriteRegistryValues,uirow);
    msiobj_release( &uirow->hdr );

    msi_free(value_data);
    msi_free(deformated);
    msi_free(uikey);

    return ERROR_SUCCESS;
}

static UINT ACTION_WriteRegistryValues(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','R','e','g','i','s','t','r','y','`',0 };

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    /* increment progress bar each time action data is sent */
    ui_progress(package,1,REG_PROGRESS_VALUE,1,0);

    rc = MSI_IterateRecords(view, NULL, ITERATE_WriteRegistryValues, package);

    msiobj_release(&view->hdr);
    return rc;
}

static UINT ACTION_InstallInitialize(MSIPACKAGE *package)
{
    package->script->CurrentlyScripting = TRUE;

    return ERROR_SUCCESS;
}


static UINT ACTION_InstallValidate(MSIPACKAGE *package)
{
    MSICOMPONENT *comp;
    DWORD progress = 0;
    DWORD total = 0;
    static const WCHAR q1[]=
        {'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
         '`','R','e','g','i','s','t','r','y','`',0};
    UINT rc;
    MSIQUERY * view;
    MSIFEATURE *feature;
    MSIFILE *file;

    TRACE("InstallValidate\n");

    rc = MSI_DatabaseOpenViewW(package->db, q1, &view);
    if (rc == ERROR_SUCCESS)
    {
        MSI_IterateRecords( view, &progress, NULL, package );
        msiobj_release( &view->hdr );
        total += progress * REG_PROGRESS_VALUE;
    }

    LIST_FOR_EACH_ENTRY( comp, &package->components, MSICOMPONENT, entry )
        total += COMPONENT_PROGRESS_VALUE;

    LIST_FOR_EACH_ENTRY( file, &package->files, MSIFILE, entry )
        total += file->FileSize;

    ui_progress(package,0,total,0,0);

    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        TRACE("Feature: %s; Installed: %i; Action %i; Request %i\n",
            debugstr_w(feature->Feature), feature->Installed, feature->Action,
            feature->ActionRequest);
    }
    
    return ERROR_SUCCESS;
}

static UINT ITERATE_LaunchConditions(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    LPCWSTR cond = NULL; 
    LPCWSTR message = NULL;
    UINT r;

    static const WCHAR title[]=
        {'I','n','s','t','a','l','l',' ','F','a', 'i','l','e','d',0};

    cond = MSI_RecordGetString(row,1);

    r = MSI_EvaluateConditionW(package,cond);
    if (r == MSICONDITION_FALSE)
    {
        if ((gUILevel & INSTALLUILEVEL_MASK) != INSTALLUILEVEL_NONE)
        {
            LPWSTR deformated;
            message = MSI_RecordGetString(row,2);
            deformat_string(package,message,&deformated);
            MessageBoxW(NULL,deformated,title,MB_OK);
            msi_free(deformated);
        }

        return ERROR_INSTALL_FAILURE;
    }

    return ERROR_SUCCESS;
}

static UINT ACTION_LaunchConditions(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view = NULL;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','L','a','u','n','c','h','C','o','n','d','i','t','i','o','n','`',0};

    TRACE("Checking launch conditions\n");

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_LaunchConditions, package);
    msiobj_release(&view->hdr);

    return rc;
}

static LPWSTR resolve_keypath( MSIPACKAGE* package, MSICOMPONENT *cmp )
{

    if (!cmp->KeyPath)
        return resolve_folder(package,cmp->Directory,FALSE,FALSE,TRUE,NULL);

    if (cmp->Attributes & msidbComponentAttributesRegistryKeyPath)
    {
        MSIRECORD * row = 0;
        UINT root,len;
        LPWSTR deformated,buffer,deformated_name;
        LPCWSTR key,name;
        static const WCHAR ExecSeqQuery[] =
            {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
             '`','R','e','g','i','s','t','r','y','`',' ',
             'W','H','E','R','E',' ', '`','R','e','g','i','s','t','r','y','`',
             ' ','=',' ' ,'\'','%','s','\'',0 };
        static const WCHAR fmt[]={'%','0','2','i',':','\\','%','s','\\',0};
        static const WCHAR fmt2[]=
            {'%','0','2','i',':','\\','%','s','\\','%','s',0};

        row = MSI_QueryGetRecord(package->db, ExecSeqQuery,cmp->KeyPath);
        if (!row)
            return NULL;

        root = MSI_RecordGetInteger(row,2);
        key = MSI_RecordGetString(row, 3);
        name = MSI_RecordGetString(row, 4);
        deformat_string(package, key , &deformated);
        deformat_string(package, name, &deformated_name);

        len = strlenW(deformated) + 6;
        if (deformated_name)
            len+=strlenW(deformated_name);

        buffer = msi_alloc( len *sizeof(WCHAR));

        if (deformated_name)
            sprintfW(buffer,fmt2,root,deformated,deformated_name);
        else
            sprintfW(buffer,fmt,root,deformated);

        msi_free(deformated);
        msi_free(deformated_name);
        msiobj_release(&row->hdr);

        return buffer;
    }
    else if (cmp->Attributes & msidbComponentAttributesODBCDataSource)
    {
        FIXME("UNIMPLEMENTED keypath as ODBC Source\n");
        return NULL;
    }
    else
    {
        MSIFILE *file = get_loaded_file( package, cmp->KeyPath );

        if (file)
            return strdupW( file->TargetPath );
    }
    return NULL;
}

static HKEY openSharedDLLsKey(void)
{
    HKEY hkey=0;
    static const WCHAR path[] =
        {'S','o','f','t','w','a','r','e','\\',
         'M','i','c','r','o','s','o','f','t','\\',
         'W','i','n','d','o','w','s','\\',
         'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
         'S','h','a','r','e','d','D','L','L','s',0};

    RegCreateKeyW(HKEY_LOCAL_MACHINE,path,&hkey);
    return hkey;
}

static UINT ACTION_GetSharedDLLsCount(LPCWSTR dll)
{
    HKEY hkey;
    DWORD count=0;
    DWORD type;
    DWORD sz = sizeof(count);
    DWORD rc;
    
    hkey = openSharedDLLsKey();
    rc = RegQueryValueExW(hkey, dll, NULL, &type, (LPBYTE)&count, &sz);
    if (rc != ERROR_SUCCESS)
        count = 0;
    RegCloseKey(hkey);
    return count;
}

static UINT ACTION_WriteSharedDLLsCount(LPCWSTR path, UINT count)
{
    HKEY hkey;

    hkey = openSharedDLLsKey();
    if (count > 0)
        msi_reg_set_val_dword( hkey, path, count );
    else
        RegDeleteValueW(hkey,path);
    RegCloseKey(hkey);
    return count;
}

/*
 * Return TRUE if the count should be written out and FALSE if not
 */
static void ACTION_RefCountComponent( MSIPACKAGE* package, MSICOMPONENT *comp )
{
    MSIFEATURE *feature;
    INT count = 0;
    BOOL write = FALSE;

    /* only refcount DLLs */
    if (comp->KeyPath == NULL || 
        comp->Attributes & msidbComponentAttributesRegistryKeyPath || 
        comp->Attributes & msidbComponentAttributesODBCDataSource)
        write = FALSE;
    else
    {
        count = ACTION_GetSharedDLLsCount( comp->FullKeypath);
        write = (count > 0);

        if (comp->Attributes & msidbComponentAttributesSharedDllRefCount)
            write = TRUE;
    }

    /* increment counts */
    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        ComponentList *cl;

        if (!ACTION_VerifyFeatureForAction( feature, INSTALLSTATE_LOCAL ))
            continue;

        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            if ( cl->component == comp )
                count++;
        }
    }

    /* decrement counts */
    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        ComponentList *cl;

        if (!ACTION_VerifyFeatureForAction( feature, INSTALLSTATE_ABSENT ))
            continue;

        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            if ( cl->component == comp )
                count--;
        }
    }

    /* ref count all the files in the component */
    if (write)
    {
        MSIFILE *file;

        LIST_FOR_EACH_ENTRY( file, &package->files, MSIFILE, entry )
        {
            if (file->Component == comp)
                ACTION_WriteSharedDLLsCount( file->TargetPath, count );
        }
    }
    
    /* add a count for permanent */
    if (comp->Attributes & msidbComponentAttributesPermanent)
        count ++;
    
    comp->RefCount = count;

    if (write)
        ACTION_WriteSharedDLLsCount( comp->FullKeypath, comp->RefCount );
}

/*
 * Ok further analysis makes me think that this work is
 * actually done in the PublishComponents and PublishFeatures
 * step, and not here.  It appears like the keypath and all that is
 * resolved in this step, however actually written in the Publish steps.
 * But we will leave it here for now because it is unclear
 */
static UINT ACTION_ProcessComponents(MSIPACKAGE *package)
{
    WCHAR squished_pc[GUID_SIZE];
    WCHAR squished_cc[GUID_SIZE];
    UINT rc;
    MSICOMPONENT *comp;
    HKEY hkey=0,hkey2=0;

    TRACE("\n");

    /* writes the Component and Features values to the registry */

    rc = MSIREG_OpenComponents(&hkey);
    if (rc != ERROR_SUCCESS)
        return rc;

    squash_guid(package->ProductCode,squished_pc);
    ui_progress(package,1,COMPONENT_PROGRESS_VALUE,1,0);

    LIST_FOR_EACH_ENTRY( comp, &package->components, MSICOMPONENT, entry )
    {
        MSIRECORD * uirow;

        ui_progress(package,2,0,0,0);
        if (!comp->ComponentId)
            continue;

        squash_guid(comp->ComponentId,squished_cc);

        msi_free(comp->FullKeypath);
        comp->FullKeypath = resolve_keypath( package, comp );

        /* do the refcounting */
        ACTION_RefCountComponent( package, comp );

        TRACE("Component %s (%s), Keypath=%s, RefCount=%i\n",
                            debugstr_w(comp->Component),
                            debugstr_w(squished_cc),
                            debugstr_w(comp->FullKeypath),
                            comp->RefCount);
        /*
         * Write the keypath out if the component is to be registered
         * and delete the key if the component is to be unregistered
         */
        if (ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL))
        {
            rc = RegCreateKeyW(hkey,squished_cc,&hkey2);
            if (rc != ERROR_SUCCESS)
                continue;

            if (!comp->FullKeypath)
                continue;

            msi_reg_set_val_str( hkey2, squished_pc, comp->FullKeypath );

            if (comp->Attributes & msidbComponentAttributesPermanent)
            {
                static const WCHAR szPermKey[] =
                    { '0','0','0','0','0','0','0','0','0','0','0','0',
                      '0','0','0','0','0','0','0','0','0','0','0','0',
                      '0','0','0','0','0','0','0','0',0 };

                msi_reg_set_val_str( hkey2, szPermKey, comp->FullKeypath );
            }

            RegCloseKey(hkey2);

            rc = MSIREG_OpenUserDataComponentKey(comp->ComponentId, &hkey2, TRUE);
            if (rc != ERROR_SUCCESS)
                continue;

            msi_reg_set_val_str(hkey2, squished_pc, comp->FullKeypath);
            RegCloseKey(hkey2);
        }
        else if (ACTION_VerifyComponentForAction( comp, INSTALLSTATE_ABSENT))
        {
            DWORD res;

            rc = RegOpenKeyW(hkey,squished_cc,&hkey2);
            if (rc != ERROR_SUCCESS)
                continue;

            RegDeleteValueW(hkey2,squished_pc);

            /* if the key is empty delete it */
            res = RegEnumKeyExW(hkey2,0,NULL,0,0,NULL,0,NULL);
            RegCloseKey(hkey2);
            if (res == ERROR_NO_MORE_ITEMS)
                RegDeleteKeyW(hkey,squished_cc);

            MSIREG_DeleteUserDataComponentKey(comp->ComponentId);
        }

        /* UI stuff */
        uirow = MSI_CreateRecord(3);
        MSI_RecordSetStringW(uirow,1,package->ProductCode);
        MSI_RecordSetStringW(uirow,2,comp->ComponentId);
        MSI_RecordSetStringW(uirow,3,comp->FullKeypath);
        ui_actiondata(package,szProcessComponents,uirow);
        msiobj_release( &uirow->hdr );
    }
    RegCloseKey(hkey);
    return rc;
}

typedef struct {
    CLSID       clsid;
    LPWSTR      source;

    LPWSTR      path;
    ITypeLib    *ptLib;
} typelib_struct;

static BOOL CALLBACK Typelib_EnumResNameProc( HMODULE hModule, LPCWSTR lpszType, 
                                       LPWSTR lpszName, LONG_PTR lParam)
{
    TLIBATTR *attr;
    typelib_struct *tl_struct = (typelib_struct*) lParam;
    static const WCHAR fmt[] = {'%','s','\\','%','i',0};
    int sz; 
    HRESULT res;

    if (!IS_INTRESOURCE(lpszName))
    {
        ERR("Not Int Resource Name %s\n",debugstr_w(lpszName));
        return TRUE;
    }

    sz = strlenW(tl_struct->source)+4;
    sz *= sizeof(WCHAR);

    if ((INT_PTR)lpszName == 1)
        tl_struct->path = strdupW(tl_struct->source);
    else
    {
        tl_struct->path = msi_alloc(sz);
        sprintfW(tl_struct->path,fmt,tl_struct->source, lpszName);
    }

    TRACE("trying %s\n", debugstr_w(tl_struct->path));
    res = LoadTypeLib(tl_struct->path,&tl_struct->ptLib);
    if (!SUCCEEDED(res))
    {
        msi_free(tl_struct->path);
        tl_struct->path = NULL;

        return TRUE;
    }

    ITypeLib_GetLibAttr(tl_struct->ptLib, &attr);
    if (IsEqualGUID(&(tl_struct->clsid),&(attr->guid)))
    {
        ITypeLib_ReleaseTLibAttr(tl_struct->ptLib, attr);
        return FALSE;
    }

    msi_free(tl_struct->path);
    tl_struct->path = NULL;

    ITypeLib_ReleaseTLibAttr(tl_struct->ptLib, attr);
    ITypeLib_Release(tl_struct->ptLib);

    return TRUE;
}

static UINT ITERATE_RegisterTypeLibraries(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    LPCWSTR component;
    MSICOMPONENT *comp;
    MSIFILE *file;
    typelib_struct tl_struct;
    HMODULE module;
    static const WCHAR szTYPELIB[] = {'T','Y','P','E','L','I','B',0};

    component = MSI_RecordGetString(row,3);
    comp = get_loaded_component(package,component);
    if (!comp)
        return ERROR_SUCCESS;

    if (!ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL))
    {
        TRACE("Skipping typelib reg due to disabled component\n");

        comp->Action = comp->Installed;

        return ERROR_SUCCESS;
    }

    comp->Action = INSTALLSTATE_LOCAL;

    file = get_loaded_file( package, comp->KeyPath ); 
    if (!file)
        return ERROR_SUCCESS;

    module = LoadLibraryExW( file->TargetPath, NULL, LOAD_LIBRARY_AS_DATAFILE );
    if (module)
    {
        LPCWSTR guid;
        guid = MSI_RecordGetString(row,1);
        CLSIDFromString((LPWSTR)guid, &tl_struct.clsid);
        tl_struct.source = strdupW( file->TargetPath );
        tl_struct.path = NULL;

        EnumResourceNamesW(module, szTYPELIB, Typelib_EnumResNameProc,
                        (LONG_PTR)&tl_struct);

        if (tl_struct.path)
        {
            LPWSTR help = NULL;
            LPCWSTR helpid;
            HRESULT res;

            helpid = MSI_RecordGetString(row,6);

            if (helpid)
                help = resolve_folder(package,helpid,FALSE,FALSE,TRUE,NULL);
            res = RegisterTypeLib(tl_struct.ptLib,tl_struct.path,help);
            msi_free(help);

            if (!SUCCEEDED(res))
                ERR("Failed to register type library %s\n",
                        debugstr_w(tl_struct.path));
            else
            {
                ui_actiondata(package,szRegisterTypeLibraries,row);

                TRACE("Registered %s\n", debugstr_w(tl_struct.path));
            }

            ITypeLib_Release(tl_struct.ptLib);
            msi_free(tl_struct.path);
        }
        else
            ERR("Failed to load type library %s\n",
                    debugstr_w(tl_struct.source));

        FreeLibrary(module);
        msi_free(tl_struct.source);
    }
    else
        ERR("Could not load file! %s\n", debugstr_w(file->TargetPath));

    return ERROR_SUCCESS;
}

static UINT ACTION_RegisterTypeLibraries(MSIPACKAGE *package)
{
    /* 
     * OK this is a bit confusing.. I am given a _Component key and I believe
     * that the file that is being registered as a type library is the "key file
     * of that component" which I interpret to mean "The file in the KeyPath of
     * that component".
     */
    UINT rc;
    MSIQUERY * view;
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','T','y','p','e','L','i','b','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_RegisterTypeLibraries, package);
    msiobj_release(&view->hdr);
    return rc;
}

static UINT ITERATE_CreateShortcuts(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPWSTR target_file, target_folder, filename;
    LPCWSTR buffer, extension;
    MSICOMPONENT *comp;
    static const WCHAR szlnk[]={'.','l','n','k',0};
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    HRESULT res;

    buffer = MSI_RecordGetString(row,4);
    comp = get_loaded_component(package,buffer);
    if (!comp)
        return ERROR_SUCCESS;

    if (!ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL ))
    {
        TRACE("Skipping shortcut creation due to disabled component\n");

        comp->Action = comp->Installed;

        return ERROR_SUCCESS;
    }

    comp->Action = INSTALLSTATE_LOCAL;

    ui_actiondata(package,szCreateShortcuts,row);

    res = CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                    &IID_IShellLinkW, (LPVOID *) &sl );

    if (FAILED( res ))
    {
        ERR("CLSID_ShellLink not available\n");
        goto err;
    }

    res = IShellLinkW_QueryInterface( sl, &IID_IPersistFile,(LPVOID*) &pf );
    if (FAILED( res ))
    {
        ERR("QueryInterface(IID_IPersistFile) failed\n");
        goto err;
    }

    buffer = MSI_RecordGetString(row,2);
    target_folder = resolve_folder(package, buffer,FALSE,FALSE,TRUE,NULL);

    /* may be needed because of a bug somewhere else */
    create_full_pathW(target_folder);

    filename = msi_dup_record_field( row, 3 );
    reduce_to_longfilename(filename);

    extension = strchrW(filename,'.');
    if (!extension || strcmpiW(extension,szlnk))
    {
        int len = strlenW(filename);
        filename = msi_realloc(filename, len * sizeof(WCHAR) + sizeof(szlnk));
        memcpy(filename + len, szlnk, sizeof(szlnk));
    }
    target_file = build_directory_name(2, target_folder, filename);
    msi_free(target_folder);
    msi_free(filename);

    buffer = MSI_RecordGetString(row,5);
    if (strchrW(buffer,'['))
    {
        LPWSTR deformated;
        deformat_string(package,buffer,&deformated);
        IShellLinkW_SetPath(sl,deformated);
        msi_free(deformated);
    }
    else
    {
        FIXME("poorly handled shortcut format, advertised shortcut\n");
        IShellLinkW_SetPath(sl,comp->FullKeypath);
    }

    if (!MSI_RecordIsNull(row,6))
    {
        LPWSTR deformated;
        buffer = MSI_RecordGetString(row,6);
        deformat_string(package,buffer,&deformated);
        IShellLinkW_SetArguments(sl,deformated);
        msi_free(deformated);
    }

    if (!MSI_RecordIsNull(row,7))
    {
        buffer = MSI_RecordGetString(row,7);
        IShellLinkW_SetDescription(sl,buffer);
    }

    if (!MSI_RecordIsNull(row,8))
        IShellLinkW_SetHotkey(sl,MSI_RecordGetInteger(row,8));

    if (!MSI_RecordIsNull(row,9))
    {
        LPWSTR Path;
        INT index; 

        buffer = MSI_RecordGetString(row,9);

        Path = build_icon_path(package,buffer);
        index = MSI_RecordGetInteger(row,10);

        /* no value means 0 */
        if (index == MSI_NULL_INTEGER)
            index = 0;

        IShellLinkW_SetIconLocation(sl,Path,index);
        msi_free(Path);
    }

    if (!MSI_RecordIsNull(row,11))
        IShellLinkW_SetShowCmd(sl,MSI_RecordGetInteger(row,11));

    if (!MSI_RecordIsNull(row,12))
    {
        LPWSTR Path;
        buffer = MSI_RecordGetString(row,12);
        Path = resolve_folder(package, buffer, FALSE, FALSE, TRUE, NULL);
        if (Path)
            IShellLinkW_SetWorkingDirectory(sl,Path);
        msi_free(Path);
    }

    TRACE("Writing shortcut to %s\n",debugstr_w(target_file));
    IPersistFile_Save(pf,target_file,FALSE);

    msi_free(target_file);    

err:
    if (pf)
        IPersistFile_Release( pf );
    if (sl)
        IShellLinkW_Release( sl );

    return ERROR_SUCCESS;
}

static UINT ACTION_CreateShortcuts(MSIPACKAGE *package)
{
    UINT rc;
    HRESULT res;
    MSIQUERY * view;
    static const WCHAR Query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','S','h','o','r','t','c','u','t','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    res = CoInitialize( NULL );
    if (FAILED (res))
    {
        ERR("CoInitialize failed\n");
        return ERROR_FUNCTION_FAILED;
    }

    rc = MSI_IterateRecords(view, NULL, ITERATE_CreateShortcuts, package);
    msiobj_release(&view->hdr);

    CoUninitialize();

    return rc;
}

static UINT ITERATE_PublishProduct(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE* package = (MSIPACKAGE*)param;
    HANDLE the_file;
    LPWSTR FilePath;
    LPCWSTR FileName;
    CHAR buffer[1024];
    DWORD sz;
    UINT rc;
    MSIRECORD *uirow;

    FileName = MSI_RecordGetString(row,1);
    if (!FileName)
    {
        ERR("Unable to get FileName\n");
        return ERROR_SUCCESS;
    }

    FilePath = build_icon_path(package,FileName);

    TRACE("Creating icon file at %s\n",debugstr_w(FilePath));

    the_file = CreateFileW(FilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);

    if (the_file == INVALID_HANDLE_VALUE)
    {
        ERR("Unable to create file %s\n",debugstr_w(FilePath));
        msi_free(FilePath);
        return ERROR_SUCCESS;
    }

    do 
    {
        DWORD write;
        sz = 1024;
        rc = MSI_RecordReadStream(row,2,buffer,&sz);
        if (rc != ERROR_SUCCESS)
        {
            ERR("Failed to get stream\n");
            CloseHandle(the_file);  
            DeleteFileW(FilePath);
            break;
        }
        WriteFile(the_file,buffer,sz,&write,NULL);
    } while (sz == 1024);

    msi_free(FilePath);

    CloseHandle(the_file);

    uirow = MSI_CreateRecord(1);
    MSI_RecordSetStringW(uirow,1,FileName);
    ui_actiondata(package,szPublishProduct,uirow);
    msiobj_release( &uirow->hdr );

    return ERROR_SUCCESS;
}

static BOOL msi_check_publish(MSIPACKAGE *package)
{
    MSIFEATURE *feature;

    LIST_FOR_EACH_ENTRY(feature, &package->features, MSIFEATURE, entry)
    {
        if (feature->ActionRequest == INSTALLSTATE_LOCAL)
            return TRUE;
    }

    return FALSE;
}

/*
 * 99% of the work done here is only done for 
 * advertised installs. However this is where the
 * Icon table is processed and written out
 * so that is what I am going to do here.
 */
static UINT ACTION_PublishProduct(MSIPACKAGE *package)
{
    UINT rc;
    LPWSTR packname;
    MSIQUERY * view;
    MSISOURCELISTINFO *info;
    MSIMEDIADISK *disk;
    static const WCHAR Query[]=
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','c','o','n','`',0};
    /* for registry stuff */
    HKEY hkey=0;
    HKEY hukey=0;
    HKEY hudkey=0, props=0;
    HKEY source;
    static const WCHAR szProductLanguage[] =
        {'P','r','o','d','u','c','t','L','a','n','g','u','a','g','e',0};
    static const WCHAR szARPProductIcon[] =
        {'A','R','P','P','R','O','D','U','C','T','I','C','O','N',0};
    static const WCHAR szProductVersion[] =
        {'P','r','o','d','u','c','t','V','e','r','s','i','o','n',0};
    static const WCHAR szSourceList[] =
        {'S','o','u','r','c','e','L','i','s','t',0};
    static const WCHAR szEmpty[] = {0};
    DWORD langid;
    LPWSTR buffer;
    DWORD size;
    MSIHANDLE hDb, hSumInfo;

    /* FIXME: also need to publish if the product is in advertise mode */
    if (!msi_check_publish(package))
        return ERROR_SUCCESS;

    /* write out icon files */

    rc = MSI_DatabaseOpenViewW(package->db, Query, &view);
    if (rc == ERROR_SUCCESS)
    {
        MSI_IterateRecords(view, NULL, ITERATE_PublishProduct, package);
        msiobj_release(&view->hdr);
    }

    /* ok there is a lot more done here but i need to figure out what */

    if (package->Context == MSIINSTALLCONTEXT_MACHINE)
    {
        rc = MSIREG_OpenLocalClassesProductKey(package->ProductCode, &hukey, TRUE);
        if (rc != ERROR_SUCCESS)
            goto end;

        rc = MSIREG_OpenLocalSystemInstallProps(package->ProductCode, &props, TRUE);
        if (rc != ERROR_SUCCESS)
            goto end;
    }
    else
    {
        rc = MSIREG_OpenProductsKey(package->ProductCode,&hkey,TRUE);
        if (rc != ERROR_SUCCESS)
            goto end;

        rc = MSIREG_OpenUserProductsKey(package->ProductCode,&hukey,TRUE);
        if (rc != ERROR_SUCCESS)
            goto end;

        rc = MSIREG_OpenCurrentUserInstallProps(package->ProductCode, &props, TRUE);
        if (rc != ERROR_SUCCESS)
            goto end;
    }

    rc = RegCreateKeyW(hukey, szSourceList, &source);
    if (rc != ERROR_SUCCESS)
        goto end;

    RegCloseKey(source);

    rc = MSIREG_OpenUserDataProductKey(package->ProductCode,&hudkey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    buffer = msi_dup_property( package, INSTALLPROPERTY_PRODUCTNAMEW );
    msi_reg_set_val_str( hukey, INSTALLPROPERTY_PRODUCTNAMEW, buffer );
    msi_free(buffer);

    langid = msi_get_property_int( package, szProductLanguage, 0 );
    msi_reg_set_val_dword( hukey, INSTALLPROPERTY_LANGUAGEW, langid );

    packname = strrchrW( package->PackagePath, '\\' ) + 1;
    msi_reg_set_val_str( hukey, INSTALLPROPERTY_PACKAGENAMEW, packname );

    /* FIXME */
    msi_reg_set_val_dword( hukey, INSTALLPROPERTY_AUTHORIZED_LUA_APPW, 0 );
    msi_reg_set_val_dword( props, INSTALLPROPERTY_INSTANCETYPEW, 0 );

    buffer = msi_dup_property( package, szARPProductIcon );
    if (buffer)
    {
        LPWSTR path = build_icon_path(package,buffer);
        msi_reg_set_val_str( hukey, INSTALLPROPERTY_PRODUCTICONW, path );
        msi_free( path );
    }
    msi_free(buffer);

    buffer = msi_dup_property( package, szProductVersion );
    if (buffer)
    {
        DWORD verdword = msi_version_str_to_dword(buffer);
        msi_reg_set_val_dword( hukey, INSTALLPROPERTY_VERSIONW, verdword );
    }
    msi_free(buffer);

    buffer = strrchrW( package->PackagePath, '\\') + 1;
    rc = MsiSourceListSetInfoW( package->ProductCode, NULL,
                                package->Context, MSICODE_PRODUCT,
                                INSTALLPROPERTY_PACKAGENAMEW, buffer );
    if (rc != ERROR_SUCCESS)
        goto end;

    rc = MsiSourceListSetInfoW( package->ProductCode, NULL,
                                package->Context, MSICODE_PRODUCT,
                                INSTALLPROPERTY_MEDIAPACKAGEPATHW, szEmpty );
    if (rc != ERROR_SUCCESS)
        goto end;

    rc = MsiSourceListSetInfoW( package->ProductCode, NULL,
                                package->Context, MSICODE_PRODUCT,
                                INSTALLPROPERTY_DISKPROMPTW, szEmpty );
    if (rc != ERROR_SUCCESS)
        goto end;

    /* FIXME: Need to write more keys to the user registry */
  
    hDb= alloc_msihandle( &package->db->hdr );
    if (!hDb) {
        rc = ERROR_NOT_ENOUGH_MEMORY;
        goto end;
    }
    rc = MsiGetSummaryInformationW(hDb, NULL, 0, &hSumInfo); 
    MsiCloseHandle(hDb);
    if (rc == ERROR_SUCCESS)
    {
        WCHAR guidbuffer[0x200];
        size = 0x200;
        rc = MsiSummaryInfoGetPropertyW(hSumInfo, 9, NULL, NULL, NULL,
                                        guidbuffer, &size);
        if (rc == ERROR_SUCCESS)
        {
            /* for now we only care about the first guid */
            LPWSTR ptr = strchrW(guidbuffer,';');
            if (ptr) *ptr = 0;
            msi_reg_set_val_str( hukey, INSTALLPROPERTY_PACKAGECODEW, guidbuffer );
        }
        else
        {
            ERR("Unable to query Revision_Number...\n");
            rc = ERROR_SUCCESS;
        }
        MsiCloseHandle(hSumInfo);
    }
    else
    {
        ERR("Unable to open Summary Information\n");
        rc = ERROR_SUCCESS;
    }

    /* publish the SourceList info */
    LIST_FOR_EACH_ENTRY(info, &package->sourcelist_info, MSISOURCELISTINFO, entry)
    {
        if (!lstrcmpW(info->property, INSTALLPROPERTY_LASTUSEDSOURCEW))
            msi_set_last_used_source(package->ProductCode, NULL, info->context,
                                     info->options, info->value);
        else
            MsiSourceListSetInfoW(package->ProductCode, NULL,
                                  info->context, info->options,
                                  info->property, info->value);
    }

    LIST_FOR_EACH_ENTRY(disk, &package->sourcelist_media, MSIMEDIADISK, entry)
    {
        MsiSourceListAddMediaDiskW(package->ProductCode, NULL,
                                   disk->context, disk->options,
                                   disk->disk_id, disk->volume_label, disk->disk_prompt);
    }

end:
    RegCloseKey(hkey);
    RegCloseKey(hukey);
    RegCloseKey(hudkey);
    RegCloseKey(props);

    return rc;
}

static UINT ITERATE_WriteIniValues(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPCWSTR component,section,key,value,identifier,filename,dirproperty;
    LPWSTR deformated_section, deformated_key, deformated_value;
    LPWSTR folder, fullname = NULL;
    MSIRECORD * uirow;
    INT action;
    MSICOMPONENT *comp;
    static const WCHAR szWindowsFolder[] =
          {'W','i','n','d','o','w','s','F','o','l','d','e','r',0};

    component = MSI_RecordGetString(row, 8);
    comp = get_loaded_component(package,component);

    if (!ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL))
    {
        TRACE("Skipping ini file due to disabled component %s\n",
                        debugstr_w(component));

        comp->Action = comp->Installed;

        return ERROR_SUCCESS;
    }

    comp->Action = INSTALLSTATE_LOCAL;

    identifier = MSI_RecordGetString(row,1); 
    filename = MSI_RecordGetString(row,2);
    dirproperty = MSI_RecordGetString(row,3);
    section = MSI_RecordGetString(row,4);
    key = MSI_RecordGetString(row,5);
    value = MSI_RecordGetString(row,6);
    action = MSI_RecordGetInteger(row,7);

    deformat_string(package,section,&deformated_section);
    deformat_string(package,key,&deformated_key);
    deformat_string(package,value,&deformated_value);

    if (dirproperty)
    {
        folder = resolve_folder(package, dirproperty, FALSE, FALSE, TRUE, NULL);
        if (!folder)
            folder = msi_dup_property( package, dirproperty );
    }
    else
        folder = msi_dup_property( package, szWindowsFolder );

    if (!folder)
    {
        ERR("Unable to resolve folder! (%s)\n",debugstr_w(dirproperty));
        goto cleanup;
    }

    fullname = build_directory_name(2, folder, filename);

    if (action == 0)
    {
        TRACE("Adding value %s to section %s in %s\n",
                debugstr_w(deformated_key), debugstr_w(deformated_section),
                debugstr_w(fullname));
        WritePrivateProfileStringW(deformated_section, deformated_key,
                                   deformated_value, fullname);
    }
    else if (action == 1)
    {
        WCHAR returned[10];
        GetPrivateProfileStringW(deformated_section, deformated_key, NULL,
                                 returned, 10, fullname);
        if (returned[0] == 0)
        {
            TRACE("Adding value %s to section %s in %s\n",
                    debugstr_w(deformated_key), debugstr_w(deformated_section),
                    debugstr_w(fullname));

            WritePrivateProfileStringW(deformated_section, deformated_key,
                                       deformated_value, fullname);
        }
    }
    else if (action == 3)
        FIXME("Append to existing section not yet implemented\n");

    uirow = MSI_CreateRecord(4);
    MSI_RecordSetStringW(uirow,1,identifier);
    MSI_RecordSetStringW(uirow,2,deformated_section);
    MSI_RecordSetStringW(uirow,3,deformated_key);
    MSI_RecordSetStringW(uirow,4,deformated_value);
    ui_actiondata(package,szWriteIniValues,uirow);
    msiobj_release( &uirow->hdr );
cleanup:
    msi_free(fullname);
    msi_free(folder);
    msi_free(deformated_key);
    msi_free(deformated_value);
    msi_free(deformated_section);
    return ERROR_SUCCESS;
}

static UINT ACTION_WriteIniValues(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','I','n','i','F','i','l','e','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
    {
        TRACE("no IniFile table\n");
        return ERROR_SUCCESS;
    }

    rc = MSI_IterateRecords(view, NULL, ITERATE_WriteIniValues, package);
    msiobj_release(&view->hdr);
    return rc;
}

static UINT ITERATE_SelfRegModules(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPCWSTR filename;
    LPWSTR FullName;
    MSIFILE *file;
    DWORD len;
    static const WCHAR ExeStr[] =
        {'r','e','g','s','v','r','3','2','.','e','x','e',' ','\"',0};
    static const WCHAR close[] =  {'\"',0};
    STARTUPINFOW si;
    PROCESS_INFORMATION info;
    BOOL brc;
    MSIRECORD *uirow;
    LPWSTR uipath, p;

    memset(&si,0,sizeof(STARTUPINFOW));

    filename = MSI_RecordGetString(row,1);
    file = get_loaded_file( package, filename );

    if (!file)
    {
        ERR("Unable to find file id %s\n",debugstr_w(filename));
        return ERROR_SUCCESS;
    }

    len = strlenW(ExeStr) + strlenW( file->TargetPath ) + 2;

    FullName = msi_alloc(len*sizeof(WCHAR));
    strcpyW(FullName,ExeStr);
    strcatW( FullName, file->TargetPath );
    strcatW(FullName,close);

    TRACE("Registering %s\n",debugstr_w(FullName));
    brc = CreateProcessW(NULL, FullName, NULL, NULL, FALSE, 0, NULL, c_colon,
                    &si, &info);

    if (brc)
        msi_dialog_check_messages(info.hProcess);

    msi_free(FullName);

    /* the UI chunk */
    uirow = MSI_CreateRecord( 2 );
    uipath = strdupW( file->TargetPath );
    p = strrchrW(uipath,'\\');
    if (p)
        p[0]=0;
    MSI_RecordSetStringW( uirow, 1, &p[1] );
    MSI_RecordSetStringW( uirow, 2, uipath);
    ui_actiondata( package, szSelfRegModules, uirow);
    msiobj_release( &uirow->hdr );
    msi_free( uipath );
    /* FIXME: call ui_progress? */

    return ERROR_SUCCESS;
}

static UINT ACTION_SelfRegModules(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] = 
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','S','e','l','f','R','e','g','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
    {
        TRACE("no SelfReg table\n");
        return ERROR_SUCCESS;
    }

    MSI_IterateRecords(view, NULL, ITERATE_SelfRegModules, package);
    msiobj_release(&view->hdr);

    return ERROR_SUCCESS;
}

static UINT ACTION_PublishFeatures(MSIPACKAGE *package)
{
    MSIFEATURE *feature;
    UINT rc;
    HKEY hkey=0;
    HKEY hukey=0;
    HKEY userdata=0;

    if (!msi_check_publish(package))
        return ERROR_SUCCESS;

    rc = MSIREG_OpenFeaturesKey(package->ProductCode,&hkey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    rc = MSIREG_OpenUserFeaturesKey(package->ProductCode,&hukey,TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    rc = MSIREG_OpenUserDataFeaturesKey(package->ProductCode, &userdata, TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    /* here the guids are base 85 encoded */
    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        ComponentList *cl;
        LPWSTR data = NULL;
        GUID clsid;
        INT size;
        BOOL absent = FALSE;
        MSIRECORD *uirow;

        if (!ACTION_VerifyFeatureForAction( feature, INSTALLSTATE_LOCAL ) &&
            !ACTION_VerifyFeatureForAction( feature, INSTALLSTATE_SOURCE ) &&
            !ACTION_VerifyFeatureForAction( feature, INSTALLSTATE_ADVERTISED ))
            absent = TRUE;

        size = 1;
        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            size += 21;
        }
        if (feature->Feature_Parent)
            size += strlenW( feature->Feature_Parent )+2;

        data = msi_alloc(size * sizeof(WCHAR));

        data[0] = 0;
        LIST_FOR_EACH_ENTRY( cl, &feature->Components, ComponentList, entry )
        {
            MSICOMPONENT* component = cl->component;
            WCHAR buf[21];

            buf[0] = 0;
            if (component->ComponentId)
            {
                TRACE("From %s\n",debugstr_w(component->ComponentId));
                CLSIDFromString(component->ComponentId, &clsid);
                encode_base85_guid(&clsid,buf);
                TRACE("to %s\n",debugstr_w(buf));
                strcatW(data,buf);
            }
        }

        if (feature->Feature_Parent)
        {
            static const WCHAR sep[] = {'\2',0};
            strcatW(data,sep);
            strcatW(data,feature->Feature_Parent);
        }

        msi_reg_set_val_str( hkey, feature->Feature, data );
        msi_reg_set_val_str( userdata, feature->Feature, data );
        msi_free(data);

        size = 0;
        if (feature->Feature_Parent)
            size = strlenW(feature->Feature_Parent)*sizeof(WCHAR);
        if (!absent)
        {
            msi_reg_set_val_str( hukey, feature->Feature, data );
        }
        else
        {
            size += 2*sizeof(WCHAR);
            data = msi_alloc(size);
            data[0] = 0x6;
            data[1] = 0;
            if (feature->Feature_Parent)
                strcpyW( &data[1], feature->Feature_Parent );
            msi_reg_set_val_str( hukey, feature->Feature, data );
            msi_free(data);
        }

        /* the UI chunk */
        uirow = MSI_CreateRecord( 1 );
        MSI_RecordSetStringW( uirow, 1, feature->Feature );
        ui_actiondata( package, szPublishFeatures, uirow);
        msiobj_release( &uirow->hdr );
        /* FIXME: call ui_progress? */
    }

end:
    RegCloseKey(hkey);
    RegCloseKey(hukey);
    return rc;
}

static UINT msi_unpublish_feature(MSIPACKAGE *package, MSIFEATURE *feature)
{
    UINT r;
    HKEY hkey;

    TRACE("unpublishing feature %s\n", debugstr_w(feature->Feature));

    r = MSIREG_OpenUserFeaturesKey(package->ProductCode, &hkey, FALSE);
    if (r == ERROR_SUCCESS)
    {
        RegDeleteValueW(hkey, feature->Feature);
        RegCloseKey(hkey);
    }

    r = MSIREG_OpenUserDataFeaturesKey(package->ProductCode, &hkey, FALSE);
    if (r == ERROR_SUCCESS)
    {
        RegDeleteValueW(hkey, feature->Feature);
        RegCloseKey(hkey);
    }

    return ERROR_SUCCESS;
}

static BOOL msi_check_unpublish(MSIPACKAGE *package)
{
    MSIFEATURE *feature;

    LIST_FOR_EACH_ENTRY(feature, &package->features, MSIFEATURE, entry)
    {
        if (feature->ActionRequest != INSTALLSTATE_ABSENT)
            return FALSE;
    }

    return TRUE;
}

static UINT ACTION_UnpublishFeatures(MSIPACKAGE *package)
{
    MSIFEATURE *feature;

    if (!msi_check_unpublish(package))
        return ERROR_SUCCESS;

    LIST_FOR_EACH_ENTRY(feature, &package->features, MSIFEATURE, entry)
    {
        msi_unpublish_feature(package, feature);
    }

    return ERROR_SUCCESS;
}

static UINT msi_get_local_package_name( LPWSTR path )
{
    static const WCHAR szInstaller[] = {
        '\\','I','n','s','t','a','l','l','e','r','\\',0};
    static const WCHAR fmt[] = { '%','x','.','m','s','i',0};
    DWORD time, len, i;
    HANDLE handle;

    time = GetTickCount();
    GetWindowsDirectoryW( path, MAX_PATH );
    lstrcatW( path, szInstaller );
    CreateDirectoryW( path, NULL );

    len = lstrlenW(path);
    for (i=0; i<0x10000; i++)
    {
        snprintfW( &path[len], MAX_PATH - len, fmt, (time+i)&0xffff );
        handle = CreateFileW( path, GENERIC_WRITE, 0, NULL,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_SHARING_VIOLATION)
            return ERROR_FUNCTION_FAILED;
    }

    return ERROR_SUCCESS;
}

static UINT msi_make_package_local( MSIPACKAGE *package, HKEY hkey )
{
    WCHAR packagefile[MAX_PATH];
    HKEY props;
    UINT r;

    r = msi_get_local_package_name( packagefile );
    if (r != ERROR_SUCCESS)
        return r;

    TRACE("Copying to local package %s\n",debugstr_w(packagefile));

    r = CopyFileW( package->db->path, packagefile, FALSE);

    if (!r)
    {
        ERR("Unable to copy package (%s -> %s) (error %d)\n",
            debugstr_w(package->db->path), debugstr_w(packagefile), GetLastError());
        return ERROR_FUNCTION_FAILED;
    }

    msi_reg_set_val_str( hkey, INSTALLPROPERTY_LOCALPACKAGEW, packagefile );

    r = MSIREG_OpenCurrentUserInstallProps(package->ProductCode, &props, TRUE);
    if (r != ERROR_SUCCESS)
        return r;

    msi_reg_set_val_str(props, INSTALLPROPERTY_LOCALPACKAGEW, packagefile);
    RegCloseKey(props);
    return ERROR_SUCCESS;
}

static UINT msi_write_uninstall_property_vals( MSIPACKAGE *package, HKEY hkey )
{
    LPWSTR prop, val, key;
    static const LPCSTR propval[] = {
        "ARPAUTHORIZEDCDFPREFIX", "AuthorizedCDFPrefix",
        "ARPCONTACT",             "Contact",
        "ARPCOMMENTS",            "Comments",
        "ProductName",            "DisplayName",
        "ProductVersion",         "DisplayVersion",
        "ARPHELPLINK",            "HelpLink",
        "ARPHELPTELEPHONE",       "HelpTelephone",
        "ARPINSTALLLOCATION",     "InstallLocation",
        "SourceDir",              "InstallSource",
        "Manufacturer",           "Publisher",
        "ARPREADME",              "Readme",
        "ARPSIZE",                "Size",
        "ARPURLINFOABOUT",        "URLInfoAbout",
        "ARPURLUPDATEINFO",       "URLUpdateInfo",
        NULL,
    };
    const LPCSTR *p = propval;

    while( *p )
    {
        prop = strdupAtoW( *p++ );
        key = strdupAtoW( *p++ );
        val = msi_dup_property( package, prop );
        msi_reg_set_val_str( hkey, key, val );
        msi_free(val);
        msi_free(key);
        msi_free(prop);
    }
    return ERROR_SUCCESS;
}

static UINT ACTION_RegisterProduct(MSIPACKAGE *package)
{
    HKEY hkey=0;
    HKEY hudkey=0, props=0;
    LPWSTR buffer = NULL;
    UINT rc;
    DWORD langid;
    static const WCHAR szWindowsInstaller[] = 
        {'W','i','n','d','o','w','s','I','n','s','t','a','l','l','e','r',0};
    static const WCHAR szUpgradeCode[] = 
        {'U','p','g','r','a','d','e','C','o','d','e',0};
    static const WCHAR modpath_fmt[] = 
        {'M','s','i','E','x','e','c','.','e','x','e',' ',
         '/','I','[','P','r','o','d','u','c','t','C','o','d','e',']',0};
    static const WCHAR szModifyPath[] = 
        {'M','o','d','i','f','y','P','a','t','h',0};
    static const WCHAR szUninstallString[] = 
        {'U','n','i','n','s','t','a','l','l','S','t','r','i','n','g',0};
    static const WCHAR szEstimatedSize[] = 
        {'E','s','t','i','m','a','t','e','d','S','i','z','e',0};
    static const WCHAR szProductLanguage[] =
        {'P','r','o','d','u','c','t','L','a','n','g','u','a','g','e',0};
    static const WCHAR szProductVersion[] =
        {'P','r','o','d','u','c','t','V','e','r','s','i','o','n',0};
    static const WCHAR szProductName[] =
        {'P','r','o','d','u','c','t','N','a','m','e',0};
    static const WCHAR szDisplayName[] =
        {'D','i','s','p','l','a','y','N','a','m','e',0};
    static const WCHAR szDisplayVersion[] =
        {'D','i','s','p','l','a','y','V','e','r','s','i','o','n',0};
    static const WCHAR szManufacturer[] =
        {'M','a','n','u','f','a','c','t','u','r','e','r',0};

    SYSTEMTIME systime;
    static const WCHAR date_fmt[] = {'%','i','%','0','2','i','%','0','2','i',0};
    LPWSTR upgrade_code;
    WCHAR szDate[9];

    /* FIXME: also need to publish if the product is in advertise mode */
    if (!msi_check_publish(package))
        return ERROR_SUCCESS;

    rc = MSIREG_OpenUninstallKey(package->ProductCode,&hkey,TRUE);
    if (rc != ERROR_SUCCESS)
        return rc;

    if (package->Context == MSIINSTALLCONTEXT_MACHINE)
    {
        rc = MSIREG_OpenLocalSystemInstallProps(package->ProductCode, &props, TRUE);
        if (rc != ERROR_SUCCESS)
            return rc;
    }
    else
    {
        rc = MSIREG_OpenCurrentUserInstallProps(package->ProductCode, &props, TRUE);
        if (rc != ERROR_SUCCESS)
            return rc;
    }

    /* dump all the info i can grab */
    /* FIXME: Flesh out more information */

    msi_write_uninstall_property_vals( package, hkey );

    msi_reg_set_val_dword( hkey, szWindowsInstaller, 1 );
    
    msi_make_package_local( package, hkey );

    /* do ModifyPath and UninstallString */
    deformat_string(package,modpath_fmt,&buffer);
    msi_reg_set_val_expand_str(hkey,szModifyPath,buffer);
    msi_reg_set_val_expand_str(hkey,szUninstallString,buffer);
    msi_free(buffer);

    /* FIXME: Write real Estimated Size when we have it */
    msi_reg_set_val_dword( hkey, szEstimatedSize, 0 );

    buffer = msi_dup_property( package, szProductName );
    msi_reg_set_val_str( props, szDisplayName, buffer );
    msi_free(buffer);

    buffer = msi_dup_property( package, cszSourceDir );
    msi_reg_set_val_str( props, INSTALLPROPERTY_INSTALLSOURCEW, buffer);
    msi_free(buffer);

    buffer = msi_dup_property( package, szManufacturer );
    msi_reg_set_val_str( props, INSTALLPROPERTY_PUBLISHERW, buffer);
    msi_free(buffer);
   
    GetLocalTime(&systime);
    sprintfW(szDate,date_fmt,systime.wYear,systime.wMonth,systime.wDay);
    msi_reg_set_val_str( hkey, INSTALLPROPERTY_INSTALLDATEW, szDate );
    msi_reg_set_val_str( props, INSTALLPROPERTY_INSTALLDATEW, szDate );
   
    langid = msi_get_property_int( package, szProductLanguage, 0 );
    msi_reg_set_val_dword( hkey, INSTALLPROPERTY_LANGUAGEW, langid );

    buffer = msi_dup_property( package, szProductVersion );
    msi_reg_set_val_str( props, szDisplayVersion, buffer );
    if (buffer)
    {
        DWORD verdword = msi_version_str_to_dword(buffer);

        msi_reg_set_val_dword( hkey, INSTALLPROPERTY_VERSIONW, verdword );
        msi_reg_set_val_dword( props, INSTALLPROPERTY_VERSIONW, verdword );
        msi_reg_set_val_dword( hkey, INSTALLPROPERTY_VERSIONMAJORW, verdword>>24 );
        msi_reg_set_val_dword( props, INSTALLPROPERTY_VERSIONMAJORW, verdword>>24 );
        msi_reg_set_val_dword( hkey, INSTALLPROPERTY_VERSIONMINORW, (verdword>>16)&0x00FF );
        msi_reg_set_val_dword( props, INSTALLPROPERTY_VERSIONMINORW, (verdword>>16)&0x00FF );
    }
    msi_free(buffer);
    
    /* Handle Upgrade Codes */
    upgrade_code = msi_dup_property( package, szUpgradeCode );
    if (upgrade_code)
    {
        HKEY hkey2;
        WCHAR squashed[33];
        MSIREG_OpenUpgradeCodesKey(upgrade_code, &hkey2, TRUE);
        squash_guid(package->ProductCode,squashed);
        msi_reg_set_val_str( hkey2, squashed, NULL );
        RegCloseKey(hkey2);
        MSIREG_OpenUserUpgradeCodesKey(upgrade_code, &hkey2, TRUE);
        squash_guid(package->ProductCode,squashed);
        msi_reg_set_val_str( hkey2, squashed, NULL );
        RegCloseKey(hkey2);

        msi_free(upgrade_code);
    }

    RegCloseKey(hkey);

    rc = MSIREG_OpenUserDataProductKey(package->ProductCode, &hudkey, TRUE);
    if (rc != ERROR_SUCCESS)
        return rc;

    RegCloseKey(hudkey);

    msi_reg_set_val_dword( props, szWindowsInstaller, 1 );
    RegCloseKey(props);

    return ERROR_SUCCESS;
}

static UINT ACTION_InstallExecute(MSIPACKAGE *package)
{
    return execute_script(package,INSTALL_SCRIPT);
}

static UINT msi_unpublish_product(MSIPACKAGE *package)
{
    LPWSTR remove = NULL;
    LPWSTR *features = NULL;
    BOOL full_uninstall = TRUE;
    MSIFEATURE *feature;

    static const WCHAR szRemove[] = {'R','E','M','O','V','E',0};
    static const WCHAR szAll[] = {'A','L','L',0};

    remove = msi_dup_property(package, szRemove);
    if (!remove)
        return ERROR_SUCCESS;

    features = msi_split_string(remove, ',');
    if (!features)
    {
        msi_free(remove);
        ERR("REMOVE feature list is empty!\n");
        return ERROR_FUNCTION_FAILED;
    }

    if (!lstrcmpW(features[0], szAll))
        full_uninstall = TRUE;
    else
    {
        LIST_FOR_EACH_ENTRY(feature, &package->features, MSIFEATURE, entry)
        {
            if (feature->Action != INSTALLSTATE_ABSENT)
                full_uninstall = FALSE;
        }
    }

    if (!full_uninstall)
        goto done;

    MSIREG_DeleteProductKey(package->ProductCode);
    MSIREG_DeleteUserProductKey(package->ProductCode);
    MSIREG_DeleteUserDataProductKey(package->ProductCode);
    MSIREG_DeleteUserFeaturesKey(package->ProductCode);
    MSIREG_DeleteUninstallKey(package->ProductCode);

done:
    msi_free(remove);
    msi_free(features);
    return ERROR_SUCCESS;
}

static UINT ACTION_InstallFinalize(MSIPACKAGE *package)
{
    UINT rc;

    rc = msi_unpublish_product(package);
    if (rc != ERROR_SUCCESS)
        return rc;

    /* turn off scheduling */
    package->script->CurrentlyScripting= FALSE;

    /* first do the same as an InstallExecute */
    rc = ACTION_InstallExecute(package);
    if (rc != ERROR_SUCCESS)
        return rc;

    /* then handle Commit Actions */
    rc = execute_script(package,COMMIT_SCRIPT);

    return rc;
}

UINT ACTION_ForceReboot(MSIPACKAGE *package)
{
    static const WCHAR RunOnce[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'R','u','n','O','n','c','e',0};
    static const WCHAR InstallRunOnce[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'I','n','s','t','a','l','l','e','r','\\',
    'R','u','n','O','n','c','e','E','n','t','r','i','e','s',0};

    static const WCHAR msiexec_fmt[] = {
    '%','s',
    '\\','M','s','i','E','x','e','c','.','e','x','e',' ','/','@',' ',
    '\"','%','s','\"',0};
    static const WCHAR install_fmt[] = {
    '/','I',' ','\"','%','s','\"',' ',
    'A','F','T','E','R','R','E','B','O','O','T','=','1',' ',
    'R','U','N','O','N','C','E','E','N','T','R','Y','=','\"','%','s','\"',0};
    WCHAR buffer[256], sysdir[MAX_PATH];
    HKEY hkey;
    WCHAR squished_pc[100];

    squash_guid(package->ProductCode,squished_pc);

    GetSystemDirectoryW(sysdir, sizeof(sysdir)/sizeof(sysdir[0]));
    RegCreateKeyW(HKEY_LOCAL_MACHINE,RunOnce,&hkey);
    snprintfW(buffer,sizeof(buffer)/sizeof(buffer[0]),msiexec_fmt,sysdir,
     squished_pc);

    msi_reg_set_val_str( hkey, squished_pc, buffer );
    RegCloseKey(hkey);

    TRACE("Reboot command %s\n",debugstr_w(buffer));

    RegCreateKeyW(HKEY_LOCAL_MACHINE,InstallRunOnce,&hkey);
    sprintfW(buffer,install_fmt,package->ProductCode,squished_pc);

    msi_reg_set_val_str( hkey, squished_pc, buffer );
    RegCloseKey(hkey);

    return ERROR_INSTALL_SUSPEND;
}

static UINT ACTION_ResolveSource(MSIPACKAGE* package)
{
    DWORD attrib;
    UINT rc;

    /*
     * We are currently doing what should be done here in the top level Install
     * however for Administrative and uninstalls this step will be needed
     */
    if (!package->PackagePath)
        return ERROR_SUCCESS;

    msi_set_sourcedir_props(package, TRUE);

    attrib = GetFileAttributesW(package->db->path);
    if (attrib == INVALID_FILE_ATTRIBUTES)
    {
        LPWSTR prompt;
        LPWSTR msg;
        DWORD size = 0;

        rc = MsiSourceListGetInfoW(package->ProductCode, NULL, 
                package->Context, MSICODE_PRODUCT,
                INSTALLPROPERTY_DISKPROMPTW,NULL,&size);
        if (rc == ERROR_MORE_DATA)
        {
            prompt = msi_alloc(size * sizeof(WCHAR));
            MsiSourceListGetInfoW(package->ProductCode, NULL, 
                    package->Context, MSICODE_PRODUCT,
                    INSTALLPROPERTY_DISKPROMPTW,prompt,&size);
        }
        else
            prompt = strdupW(package->db->path);

        msg = generate_error_string(package,1302,1,prompt);
        while(attrib == INVALID_FILE_ATTRIBUTES)
        {
            rc = MessageBoxW(NULL,msg,NULL,MB_OKCANCEL);
            if (rc == IDCANCEL)
            {
                rc = ERROR_INSTALL_USEREXIT;
                break;
            }
            attrib = GetFileAttributesW(package->db->path);
        }
        msi_free(prompt);
        rc = ERROR_SUCCESS;
    }
    else
        return ERROR_SUCCESS;

    return rc;
}

static UINT ACTION_RegisterUser(MSIPACKAGE *package)
{
    HKEY hkey=0;
    LPWSTR buffer;
    LPWSTR productid;
    UINT rc,i;

    static const WCHAR szPropKeys[][80] = 
    {
        {'P','r','o','d','u','c','t','I','D',0},
        {'U','S','E','R','N','A','M','E',0},
        {'C','O','M','P','A','N','Y','N','A','M','E',0},
        {0},
    };

    static const WCHAR szRegKeys[][80] = 
    {
        {'P','r','o','d','u','c','t','I','D',0},
        {'R','e','g','O','w','n','e','r',0},
        {'R','e','g','C','o','m','p','a','n','y',0},
        {0},
    };

    if (msi_check_unpublish(package))
    {
        MSIREG_DeleteUserDataProductKey(package->ProductCode);
        return ERROR_SUCCESS;
    }

    productid = msi_dup_property( package, INSTALLPROPERTY_PRODUCTIDW );
    if (!productid)
        return ERROR_SUCCESS;

    rc = MSIREG_OpenCurrentUserInstallProps(package->ProductCode, &hkey, TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;

    for( i = 0; szPropKeys[i][0]; i++ )
    {
        buffer = msi_dup_property( package, szPropKeys[i] );
        msi_reg_set_val_str( hkey, szRegKeys[i], buffer );
        msi_free( buffer );
    }

end:
    msi_free(productid);
    RegCloseKey(hkey);

    /* FIXME: call ui_actiondata */

    return rc;
}


static UINT ACTION_ExecuteAction(MSIPACKAGE *package)
{
    UINT rc;

    package->script->InWhatSequence |= SEQUENCE_EXEC;
    rc = ACTION_ProcessExecSequence(package,FALSE);
    return rc;
}


static UINT ITERATE_PublishComponent(MSIRECORD *rec, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPCWSTR compgroupid=NULL;
    LPCWSTR feature=NULL;
    LPCWSTR text = NULL;
    LPCWSTR qualifier = NULL;
    LPCWSTR component = NULL;
    LPWSTR advertise = NULL;
    LPWSTR output = NULL;
    HKEY hkey;
    UINT rc = ERROR_SUCCESS;
    MSICOMPONENT *comp;
    DWORD sz = 0;
    MSIRECORD *uirow;

    component = MSI_RecordGetString(rec,3);
    comp = get_loaded_component(package,component);

    if (!ACTION_VerifyComponentForAction( comp, INSTALLSTATE_LOCAL ) && 
       !ACTION_VerifyComponentForAction( comp, INSTALLSTATE_SOURCE ) &&
       !ACTION_VerifyComponentForAction( comp, INSTALLSTATE_ADVERTISED ))
    {
        TRACE("Skipping: Component %s not scheduled for install\n",
                        debugstr_w(component));

        return ERROR_SUCCESS;
    }

    compgroupid = MSI_RecordGetString(rec,1);
    qualifier = MSI_RecordGetString(rec,2);

    rc = MSIREG_OpenUserComponentsKey(compgroupid, &hkey, TRUE);
    if (rc != ERROR_SUCCESS)
        goto end;
    
    text = MSI_RecordGetString(rec,4);
    feature = MSI_RecordGetString(rec,5);
  
    advertise = create_component_advertise_string(package, comp, feature);

    sz = strlenW(advertise);

    if (text)
        sz += lstrlenW(text);

    sz+=3;
    sz *= sizeof(WCHAR);
           
    output = msi_alloc_zero(sz);
    strcpyW(output,advertise);
    msi_free(advertise);

    if (text)
        strcatW(output,text);

    msi_reg_set_val_multi_str( hkey, qualifier, output );
    
end:
    RegCloseKey(hkey);
    msi_free(output);

    /* the UI chunk */
    uirow = MSI_CreateRecord( 2 );
    MSI_RecordSetStringW( uirow, 1, compgroupid );
    MSI_RecordSetStringW( uirow, 2, qualifier);
    ui_actiondata( package, szPublishComponents, uirow);
    msiobj_release( &uirow->hdr );
    /* FIXME: call ui_progress? */

    return rc;
}

/*
 * At present I am ignorning the advertised components part of this and only
 * focusing on the qualified component sets
 */
static UINT ACTION_PublishComponents(MSIPACKAGE *package)
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','P','u','b','l','i','s','h',
         'C','o','m','p','o','n','e','n','t','`',0};
    
    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_PublishComponent, package);
    msiobj_release(&view->hdr);

    return rc;
}

static UINT ITERATE_InstallService(MSIRECORD *rec, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    MSIRECORD *row;
    MSIFILE *file;
    SC_HANDLE hscm, service = NULL;
    LPCWSTR comp, depends, pass;
    LPWSTR name = NULL, disp = NULL;
    LPCWSTR load_order, serv_name, key;
    DWORD serv_type, start_type;
    DWORD err_control;

    static const WCHAR query[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R', 'O','M',' ',
         '`','C','o','m','p','o','n','e','n','t','`',' ',
         'W','H','E','R','E',' ',
         '`','C','o','m','p','o','n','e','n','t','`',' ',
         '=','\'','%','s','\'',0};

    hscm = OpenSCManagerW(NULL, SERVICES_ACTIVE_DATABASEW, GENERIC_WRITE);
    if (!hscm)
    {
        ERR("Failed to open the SC Manager!\n");
        goto done;
    }

    start_type = MSI_RecordGetInteger(rec, 5);
    if (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START)
        goto done;

    depends = MSI_RecordGetString(rec, 8);
    if (depends && *depends)
        FIXME("Dependency list unhandled!\n");

    deformat_string(package, MSI_RecordGetString(rec, 2), &name);
    deformat_string(package, MSI_RecordGetString(rec, 3), &disp);
    serv_type = MSI_RecordGetInteger(rec, 4);
    err_control = MSI_RecordGetInteger(rec, 6);
    load_order = MSI_RecordGetString(rec, 7);
    serv_name = MSI_RecordGetString(rec, 9);
    pass = MSI_RecordGetString(rec, 10);
    comp = MSI_RecordGetString(rec, 12);

    /* fetch the service path */
    row = MSI_QueryGetRecord(package->db, query, comp);
    if (!row)
    {
        ERR("Control query failed!\n");
        goto done;
    }

    key = MSI_RecordGetString(row, 6);

    file = get_loaded_file(package, key);
    msiobj_release(&row->hdr);
    if (!file)
    {
        ERR("Failed to load the service file\n");
        goto done;
    }

    service = CreateServiceW(hscm, name, disp, GENERIC_ALL, serv_type,
                             start_type, err_control, file->TargetPath,
                             load_order, NULL, NULL, serv_name, pass);
    if (!service)
    {
        if (GetLastError() != ERROR_SERVICE_EXISTS)
            ERR("Failed to create service %s: %d\n", debugstr_w(name), GetLastError());
    }

done:
    CloseServiceHandle(service);
    CloseServiceHandle(hscm);
    msi_free(name);
    msi_free(disp);

    return ERROR_SUCCESS;
}

static UINT ACTION_InstallServices( MSIPACKAGE *package )
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         'S','e','r','v','i','c','e','I','n','s','t','a','l','l',0};
    
    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_InstallService, package);
    msiobj_release(&view->hdr);

    return rc;
}

/* converts arg1[~]arg2[~]arg3 to a list of ptrs to the strings */
static LPCWSTR *msi_service_args_to_vector(LPWSTR args, DWORD *numargs)
{
    LPCWSTR *vector, *temp_vector;
    LPWSTR p, q;
    DWORD sep_len;

    static const WCHAR separator[] = {'[','~',']',0};

    *numargs = 0;
    sep_len = sizeof(separator) / sizeof(WCHAR) - 1;

    if (!args)
        return NULL;

    vector = msi_alloc(sizeof(LPWSTR));
    if (!vector)
        return NULL;

    p = args;
    do
    {
        (*numargs)++;
        vector[*numargs - 1] = p;

        if ((q = strstrW(p, separator)))
        {
            *q = '\0';

            temp_vector = msi_realloc(vector, (*numargs + 1) * sizeof(LPWSTR));
            if (!temp_vector)
            {
                msi_free(vector);
                return NULL;
            }
            vector = temp_vector;

            p = q + sep_len;
        }
    } while (q);

    return vector;
}

static UINT ITERATE_StartService(MSIRECORD *rec, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE *)param;
    MSICOMPONENT *comp;
    SC_HANDLE scm, service = NULL;
    LPCWSTR name, *vector = NULL;
    LPWSTR args;
    DWORD event, numargs;
    UINT r = ERROR_FUNCTION_FAILED;

    comp = get_loaded_component(package, MSI_RecordGetString(rec, 6));
    if (!comp || comp->Action == INSTALLSTATE_UNKNOWN || comp->Action == INSTALLSTATE_ABSENT)
        return ERROR_SUCCESS;

    name = MSI_RecordGetString(rec, 2);
    event = MSI_RecordGetInteger(rec, 3);
    args = strdupW(MSI_RecordGetString(rec, 4));

    if (!(event & msidbServiceControlEventStart))
        return ERROR_SUCCESS;

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm)
    {
        ERR("Failed to open the service control manager\n");
        goto done;
    }

    service = OpenServiceW(scm, name, SERVICE_START);
    if (!service)
    {
        ERR("Failed to open service %s\n", debugstr_w(name));
        goto done;
    }

    vector = msi_service_args_to_vector(args, &numargs);

    if (!StartServiceW(service, numargs, vector))
    {
        ERR("Failed to start service %s\n", debugstr_w(name));
        goto done;
    }

    r = ERROR_SUCCESS;

done:
    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    msi_free(args);
    msi_free(vector);
    return r;
}

static UINT ACTION_StartServices( MSIPACKAGE *package )
{
    UINT rc;
    MSIQUERY *view;

    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'S','e','r','v','i','c','e','C','o','n','t','r','o','l',0 };

    rc = MSI_DatabaseOpenViewW(package->db, query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_StartService, package);
    msiobj_release(&view->hdr);

    return rc;
}

static BOOL stop_service_dependents(SC_HANDLE scm, SC_HANDLE service)
{
    DWORD i, needed, count;
    ENUM_SERVICE_STATUSW *dependencies;
    SERVICE_STATUS ss;
    SC_HANDLE depserv;

    if (EnumDependentServicesW(service, SERVICE_ACTIVE, NULL,
                               0, &needed, &count))
        return TRUE;

    if (GetLastError() != ERROR_MORE_DATA)
        return FALSE;

    dependencies = msi_alloc(needed);
    if (!dependencies)
        return FALSE;

    if (!EnumDependentServicesW(service, SERVICE_ACTIVE, dependencies,
                                needed, &needed, &count))
        goto error;

    for (i = 0; i < count; i++)
    {
        depserv = OpenServiceW(scm, dependencies[i].lpServiceName,
                               SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (!depserv)
            goto error;

        if (!ControlService(depserv, SERVICE_CONTROL_STOP, &ss))
            goto error;
    }

    return TRUE;

error:
    msi_free(dependencies);
    return FALSE;
}

static UINT ITERATE_StopService(MSIRECORD *rec, LPVOID param)
{
    MSIPACKAGE *package = (MSIPACKAGE *)param;
    MSICOMPONENT *comp;
    SERVICE_STATUS status;
    SERVICE_STATUS_PROCESS ssp;
    SC_HANDLE scm = NULL, service = NULL;
    LPWSTR name, args;
    DWORD event, needed;

    event = MSI_RecordGetInteger(rec, 3);
    if (!(event & msidbServiceControlEventStop))
        return ERROR_SUCCESS;

    comp = get_loaded_component(package, MSI_RecordGetString(rec, 6));
    if (!comp || comp->Action == INSTALLSTATE_UNKNOWN || comp->Action == INSTALLSTATE_ABSENT)
        return ERROR_SUCCESS;

    deformat_string(package, MSI_RecordGetString(rec, 2), &name);
    deformat_string(package, MSI_RecordGetString(rec, 4), &args);
    args = strdupW(MSI_RecordGetString(rec, 4));

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
    {
        WARN("Failed to open the SCM: %d\n", GetLastError());
        goto done;
    }

    service = OpenServiceW(scm, name,
                           SERVICE_STOP |
                           SERVICE_QUERY_STATUS |
                           SERVICE_ENUMERATE_DEPENDENTS);
    if (!service)
    {
        WARN("Failed to open service (%s): %d\n",
              debugstr_w(name), GetLastError());
        goto done;
    }

    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                              sizeof(SERVICE_STATUS_PROCESS), &needed))
    {
        WARN("Failed to query service status (%s): %d\n",
             debugstr_w(name), GetLastError());
        goto done;
    }

    if (ssp.dwCurrentState == SERVICE_STOPPED)
        goto done;

    stop_service_dependents(scm, service);

    if (!ControlService(service, SERVICE_CONTROL_STOP, &status))
        WARN("Failed to stop service (%s): %d\n", debugstr_w(name), GetLastError());

done:
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    msi_free(name);
    msi_free(args);

    return ERROR_SUCCESS;
}

static UINT ACTION_StopServices( MSIPACKAGE *package )
{
    UINT rc;
    MSIQUERY *view;

    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'S','e','r','v','i','c','e','C','o','n','t','r','o','l',0 };

    rc = MSI_DatabaseOpenViewW(package->db, query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_StopService, package);
    msiobj_release(&view->hdr);

    return rc;
}

static MSIFILE *msi_find_file( MSIPACKAGE *package, LPCWSTR filename )
{
    MSIFILE *file;

    LIST_FOR_EACH_ENTRY(file, &package->files, MSIFILE, entry)
    {
        if (!lstrcmpW(file->File, filename))
            return file;
    }

    return NULL;
}

static UINT ITERATE_InstallODBCDriver( MSIRECORD *rec, LPVOID param )
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPWSTR driver, driver_path, ptr;
    WCHAR outpath[MAX_PATH];
    MSIFILE *driver_file, *setup_file;
    LPCWSTR desc;
    DWORD len, usage;
    UINT r = ERROR_SUCCESS;

    static const WCHAR driver_fmt[] = {
        'D','r','i','v','e','r','=','%','s',0};
    static const WCHAR setup_fmt[] = {
        'S','e','t','u','p','=','%','s',0};
    static const WCHAR usage_fmt[] = {
        'F','i','l','e','U','s','a','g','e','=','1',0};

    desc = MSI_RecordGetString(rec, 3);

    driver_file = msi_find_file(package, MSI_RecordGetString(rec, 4));
    setup_file = msi_find_file(package, MSI_RecordGetString(rec, 5));

    if (!driver_file || !setup_file)
    {
        ERR("ODBC Driver entry not found!\n");
        return ERROR_FUNCTION_FAILED;
    }

    len = lstrlenW(desc) + lstrlenW(driver_fmt) + lstrlenW(driver_file->FileName) +
          lstrlenW(setup_fmt) + lstrlenW(setup_file->FileName) +
          lstrlenW(usage_fmt) + 1;
    driver = msi_alloc(len * sizeof(WCHAR));
    if (!driver)
        return ERROR_OUTOFMEMORY;

    ptr = driver;
    lstrcpyW(ptr, desc);
    ptr += lstrlenW(ptr) + 1;

    sprintfW(ptr, driver_fmt, driver_file->FileName);
    ptr += lstrlenW(ptr) + 1;

    sprintfW(ptr, setup_fmt, setup_file->FileName);
    ptr += lstrlenW(ptr) + 1;

    lstrcpyW(ptr, usage_fmt);
    ptr += lstrlenW(ptr) + 1;
    *ptr = '\0';

    driver_path = strdupW(driver_file->TargetPath);
    ptr = strrchrW(driver_path, '\\');
    if (ptr) *ptr = '\0';

    if (!SQLInstallDriverExW(driver, driver_path, outpath, MAX_PATH,
                             NULL, ODBC_INSTALL_COMPLETE, &usage))
    {
        ERR("Failed to install SQL driver!\n");
        r = ERROR_FUNCTION_FAILED;
    }

    msi_free(driver);
    msi_free(driver_path);

    return r;
}

static UINT ITERATE_InstallODBCTranslator( MSIRECORD *rec, LPVOID param )
{
    MSIPACKAGE *package = (MSIPACKAGE*)param;
    LPWSTR translator, translator_path, ptr;
    WCHAR outpath[MAX_PATH];
    MSIFILE *translator_file, *setup_file;
    LPCWSTR desc;
    DWORD len, usage;
    UINT r = ERROR_SUCCESS;

    static const WCHAR translator_fmt[] = {
        'T','r','a','n','s','l','a','t','o','r','=','%','s',0};
    static const WCHAR setup_fmt[] = {
        'S','e','t','u','p','=','%','s',0};

    desc = MSI_RecordGetString(rec, 3);

    translator_file = msi_find_file(package, MSI_RecordGetString(rec, 4));
    setup_file = msi_find_file(package, MSI_RecordGetString(rec, 5));

    if (!translator_file || !setup_file)
    {
        ERR("ODBC Translator entry not found!\n");
        return ERROR_FUNCTION_FAILED;
    }

    len = lstrlenW(desc) + lstrlenW(translator_fmt) + lstrlenW(translator_file->FileName) +
          lstrlenW(setup_fmt) + lstrlenW(setup_file->FileName) + 1;
    translator = msi_alloc(len * sizeof(WCHAR));
    if (!translator)
        return ERROR_OUTOFMEMORY;

    ptr = translator;
    lstrcpyW(ptr, desc);
    ptr += lstrlenW(ptr) + 1;

    sprintfW(ptr, translator_fmt, translator_file->FileName);
    ptr += lstrlenW(ptr) + 1;

    sprintfW(ptr, setup_fmt, setup_file->FileName);
    ptr += lstrlenW(ptr) + 1;
    *ptr = '\0';

    translator_path = strdupW(translator_file->TargetPath);
    ptr = strrchrW(translator_path, '\\');
    if (ptr) *ptr = '\0';

    if (!SQLInstallTranslatorExW(translator, translator_path, outpath, MAX_PATH,
                                 NULL, ODBC_INSTALL_COMPLETE, &usage))
    {
        ERR("Failed to install SQL translator!\n");
        r = ERROR_FUNCTION_FAILED;
    }

    msi_free(translator);
    msi_free(translator_path);

    return r;
}

static UINT ITERATE_InstallODBCDataSource( MSIRECORD *rec, LPVOID param )
{
    LPWSTR attrs;
    LPCWSTR desc, driver;
    WORD request = ODBC_ADD_SYS_DSN;
    INT registration;
    DWORD len;
    UINT r = ERROR_SUCCESS;

    static const WCHAR attrs_fmt[] = {
        'D','S','N','=','%','s',0 };

    desc = MSI_RecordGetString(rec, 3);
    driver = MSI_RecordGetString(rec, 4);
    registration = MSI_RecordGetInteger(rec, 5);

    if (registration == msidbODBCDataSourceRegistrationPerMachine) request = ODBC_ADD_SYS_DSN;
    else if (registration == msidbODBCDataSourceRegistrationPerUser) request = ODBC_ADD_DSN;

    len = lstrlenW(attrs_fmt) + lstrlenW(desc) + 1 + 1;
    attrs = msi_alloc(len * sizeof(WCHAR));
    if (!attrs)
        return ERROR_OUTOFMEMORY;

    sprintfW(attrs, attrs_fmt, desc);
    attrs[len - 1] = '\0';

    if (!SQLConfigDataSourceW(NULL, request, driver, attrs))
    {
        ERR("Failed to install SQL data source!\n");
        r = ERROR_FUNCTION_FAILED;
    }

    msi_free(attrs);

    return r;
}

static UINT ACTION_InstallODBC( MSIPACKAGE *package )
{
    UINT rc;
    MSIQUERY *view;

    static const WCHAR driver_query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'O','D','B','C','D','r','i','v','e','r',0 };

    static const WCHAR translator_query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'O','D','B','C','T','r','a','n','s','l','a','t','o','r',0 };

    static const WCHAR source_query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'O','D','B','C','D','a','t','a','S','o','u','r','c','e',0 };

    rc = MSI_DatabaseOpenViewW(package->db, driver_query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_InstallODBCDriver, package);
    msiobj_release(&view->hdr);

    rc = MSI_DatabaseOpenViewW(package->db, translator_query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_InstallODBCTranslator, package);
    msiobj_release(&view->hdr);

    rc = MSI_DatabaseOpenViewW(package->db, source_query, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_InstallODBCDataSource, package);
    msiobj_release(&view->hdr);

    return rc;
}

#define ENV_ACT_SETALWAYS   0x1
#define ENV_ACT_SETABSENT   0x2
#define ENV_ACT_REMOVE      0x4
#define ENV_ACT_REMOVEMATCH 0x8

#define ENV_MOD_MACHINE     0x20000000
#define ENV_MOD_APPEND      0x40000000
#define ENV_MOD_PREFIX      0x80000000
#define ENV_MOD_MASK        0xC0000000

#define check_flag_combo(x, y) ((x) & ~(y)) == (y)

static LONG env_set_flags( LPCWSTR *name, LPCWSTR *value, DWORD *flags )
{
    LPCWSTR cptr = *name;
    LPCWSTR ptr = *value;

    static const WCHAR prefix[] = {'[','~',']',0};
    static const int prefix_len = 3;

    *flags = 0;
    while (*cptr)
    {
        if (*cptr == '=')
            *flags |= ENV_ACT_SETALWAYS;
        else if (*cptr == '+')
            *flags |= ENV_ACT_SETABSENT;
        else if (*cptr == '-')
            *flags |= ENV_ACT_REMOVE;
        else if (*cptr == '!')
            *flags |= ENV_ACT_REMOVEMATCH;
        else if (*cptr == '*')
            *flags |= ENV_MOD_MACHINE;
        else
            break;

        cptr++;
        (*name)++;
    }

    if (!*cptr)
    {
        ERR("Missing environment variable\n");
        return ERROR_FUNCTION_FAILED;
    }

    if (!strncmpW(ptr, prefix, prefix_len))
    {
        *flags |= ENV_MOD_APPEND;
        *value += lstrlenW(prefix);
    }
    else if (lstrlenW(*value) >= prefix_len)
    {
        ptr += lstrlenW(ptr) - prefix_len;
        if (!lstrcmpW(ptr, prefix))
        {
            *flags |= ENV_MOD_PREFIX;
            /* the "[~]" will be removed by deformat_string */;
        }
    }

    if (!*flags ||
        check_flag_combo(*flags, ENV_ACT_SETALWAYS | ENV_ACT_SETABSENT) ||
        check_flag_combo(*flags, ENV_ACT_REMOVEMATCH | ENV_ACT_SETABSENT) ||
        check_flag_combo(*flags, ENV_ACT_REMOVEMATCH | ENV_ACT_SETALWAYS) ||
        check_flag_combo(*flags, ENV_ACT_SETABSENT | ENV_MOD_MASK))
    {
        ERR("Invalid flags: %08x\n", *flags);
        return ERROR_FUNCTION_FAILED;
    }

    return ERROR_SUCCESS;
}

static UINT ITERATE_WriteEnvironmentString( MSIRECORD *rec, LPVOID param )
{
    MSIPACKAGE *package = param;
    LPCWSTR name, value, comp;
    LPWSTR data = NULL, newval = NULL;
    LPWSTR deformatted = NULL, ptr;
    DWORD flags, type, size;
    LONG res;
    HKEY env = NULL, root;
    LPCWSTR environment;

    static const WCHAR user_env[] =
        {'E','n','v','i','r','o','n','m','e','n','t',0};
    static const WCHAR machine_env[] =
        {'S','y','s','t','e','m','\\',
         'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
         'C','o','n','t','r','o','l','\\',
         'S','e','s','s','i','o','n',' ','M','a','n','a','g','e','r','\\',
         'E','n','v','i','r','o','n','m','e','n','t',0};
    static const WCHAR semicolon[] = {';',0};

    name = MSI_RecordGetString(rec, 2);
    value = MSI_RecordGetString(rec, 3);
    comp = MSI_RecordGetString(rec, 4);

    res = env_set_flags(&name, &value, &flags);
    if (res != ERROR_SUCCESS)
       goto done;

    deformat_string(package, value, &deformatted);
    if (!deformatted)
    {
        res = ERROR_OUTOFMEMORY;
        goto done;
    }

    value = deformatted;

    if (flags & ENV_MOD_MACHINE)
    {
        environment = machine_env;
        root = HKEY_LOCAL_MACHINE;
    }
    else
    {
        environment = user_env;
        root = HKEY_CURRENT_USER;
    }

    res = RegCreateKeyExW(root, environment, 0, NULL, 0,
                          KEY_ALL_ACCESS, NULL, &env, NULL);
    if (res != ERROR_SUCCESS)
        goto done;

    if (flags & ENV_ACT_REMOVE)
        FIXME("Not removing environment variable on uninstall!\n");

    size = 0;
    res = RegQueryValueExW(env, name, NULL, &type, NULL, &size);
    if ((res != ERROR_SUCCESS && res != ERROR_FILE_NOT_FOUND) ||
        (res == ERROR_SUCCESS && type != REG_SZ && type != REG_EXPAND_SZ))
        goto done;

    if (res != ERROR_FILE_NOT_FOUND)
    {
        if (flags & ENV_ACT_SETABSENT)
        {
            res = ERROR_SUCCESS;
            goto done;
        }

        data = msi_alloc(size);
        if (!data)
        {
            RegCloseKey(env);
            return ERROR_OUTOFMEMORY;
        }

        res = RegQueryValueExW(env, name, NULL, &type, (LPVOID)data, &size);
        if (res != ERROR_SUCCESS)
            goto done;

        if (flags & ENV_ACT_REMOVEMATCH && (!value || !lstrcmpW(data, value)))
        {
            res = RegDeleteKeyW(env, name);
            goto done;
        }

        size =  (lstrlenW(value) + 1 + size) * sizeof(WCHAR);
        newval =  msi_alloc(size);
        ptr = newval;
        if (!newval)
        {
            res = ERROR_OUTOFMEMORY;
            goto done;
        }

        if (!(flags & ENV_MOD_MASK))
            lstrcpyW(newval, value);
        else
        {
            if (flags & ENV_MOD_PREFIX)
            {
                lstrcpyW(newval, value);
                lstrcatW(newval, semicolon);
                ptr = newval + lstrlenW(value) + 1;
            }

            lstrcpyW(ptr, data);

            if (flags & ENV_MOD_APPEND)
            {
                lstrcatW(newval, semicolon);
                lstrcatW(newval, value);
            }
        }
    }
    else
    {
        size = (lstrlenW(value) + 1) * sizeof(WCHAR);
        newval = msi_alloc(size);
        if (!newval)
        {
            res = ERROR_OUTOFMEMORY;
            goto done;
        }

        lstrcpyW(newval, value);
    }

    TRACE("setting %s to %s\n", debugstr_w(name), debugstr_w(newval));
    res = msi_reg_set_val(env, name, 0, type, (LPVOID)newval, size);

done:
    if (env) RegCloseKey(env);
    msi_free(deformatted);
    msi_free(data);
    msi_free(newval);
    return res;
}

static UINT ACTION_WriteEnvironmentStrings( MSIPACKAGE *package )
{
    UINT rc;
    MSIQUERY * view;
    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','E','n','v','i','r','o','n','m','e','n','t','`',0};
    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_WriteEnvironmentString, package);
    msiobj_release(&view->hdr);

    return rc;
}

#define is_dot_dir(x) ((x[0] == '.') && ((x[1] == 0) || ((x[1] == '.') && (x[2] == 0))))

typedef struct
{
    struct list entry;
    LPWSTR sourcename;
    LPWSTR destname;
    LPWSTR source;
    LPWSTR dest;
} FILE_LIST;

static BOOL msi_move_file(LPCWSTR source, LPCWSTR dest, int options)
{
    BOOL ret;

    if (GetFileAttributesW(source) == FILE_ATTRIBUTE_DIRECTORY ||
        GetFileAttributesW(dest) == FILE_ATTRIBUTE_DIRECTORY)
    {
        WARN("Source or dest is directory, not moving\n");
        return FALSE;
    }

    if (options == msidbMoveFileOptionsMove)
    {
        TRACE("moving %s -> %s\n", debugstr_w(source), debugstr_w(dest));
        ret = MoveFileExW(source, dest, MOVEFILE_REPLACE_EXISTING);
        if (!ret)
        {
            WARN("MoveFile failed: %d\n", GetLastError());
            return FALSE;
        }
    }
    else
    {
        TRACE("copying %s -> %s\n", debugstr_w(source), debugstr_w(dest));
        ret = CopyFileW(source, dest, FALSE);
        if (!ret)
        {
            WARN("CopyFile failed: %d\n", GetLastError());
            return FALSE;
        }
    }

    return TRUE;
}

static LPWSTR wildcard_to_file(LPWSTR wildcard, LPWSTR filename)
{
    LPWSTR path, ptr;
    DWORD dirlen, pathlen;

    ptr = strrchrW(wildcard, '\\');
    dirlen = ptr - wildcard + 1;

    pathlen = dirlen + lstrlenW(filename) + 1;
    path = msi_alloc(pathlen * sizeof(WCHAR));

    lstrcpynW(path, wildcard, dirlen + 1);
    lstrcatW(path, filename);

    return path;
}

static void free_file_entry(FILE_LIST *file)
{
    msi_free(file->source);
    msi_free(file->dest);
    msi_free(file);
}

static void free_list(FILE_LIST *list)
{
    while (!list_empty(&list->entry))
    {
        FILE_LIST *file = LIST_ENTRY(list_head(&list->entry), FILE_LIST, entry);

        list_remove(&file->entry);
        free_file_entry(file);
    }
}

static BOOL add_wildcard(FILE_LIST *files, LPWSTR source, LPWSTR dest)
{
    FILE_LIST *new, *file;
    LPWSTR ptr, filename;
    DWORD size;

    new = msi_alloc_zero(sizeof(FILE_LIST));
    if (!new)
        return FALSE;

    new->source = strdupW(source);
    ptr = strrchrW(dest, '\\') + 1;
    filename = strrchrW(new->source, '\\') + 1;

    new->sourcename = filename;

    if (*ptr)
        new->destname = ptr;
    else
        new->destname = new->sourcename;

    size = (ptr - dest) + lstrlenW(filename) + 1;
    new->dest = msi_alloc(size * sizeof(WCHAR));
    if (!new->dest)
    {
        free_file_entry(new);
        return FALSE;
    }

    lstrcpynW(new->dest, dest, ptr - dest + 1);
    lstrcatW(new->dest, filename);

    if (list_empty(&files->entry))
    {
        list_add_head(&files->entry, &new->entry);
        return TRUE;
    }

    LIST_FOR_EACH_ENTRY(file, &files->entry, FILE_LIST, entry)
    {
        if (lstrcmpW(source, file->source) < 0)
        {
            list_add_before(&file->entry, &new->entry);
            return TRUE;
        }
    }

    list_add_after(&file->entry, &new->entry);
    return TRUE;
}

static BOOL move_files_wildcard(LPWSTR source, LPWSTR dest, int options)
{
    WIN32_FIND_DATAW wfd;
    HANDLE hfile;
    LPWSTR path;
    BOOL res;
    FILE_LIST files, *file;
    DWORD size;

    hfile = FindFirstFileW(source, &wfd);
    if (hfile == INVALID_HANDLE_VALUE) return FALSE;

    list_init(&files.entry);

    for (res = TRUE; res; res = FindNextFileW(hfile, &wfd))
    {
        if (is_dot_dir(wfd.cFileName)) continue;

        path = wildcard_to_file(source, wfd.cFileName);
        if (!path)
        {
            res = FALSE;
            goto done;
        }

        add_wildcard(&files, path, dest);
        msi_free(path);
    }

    /* no files match the wildcard */
    if (list_empty(&files.entry))
        goto done;

    /* only the first wildcard match gets renamed to dest */
    file = LIST_ENTRY(list_head(&files.entry), FILE_LIST, entry);
    size = (strrchrW(file->dest, '\\') - file->dest) + lstrlenW(file->destname) + 2;
    file->dest = msi_realloc(file->dest, size * sizeof(WCHAR));
    if (!file->dest)
    {
        res = FALSE;
        goto done;
    }

    lstrcpyW(strrchrW(file->dest, '\\') + 1, file->destname);

    while (!list_empty(&files.entry))
    {
        file = LIST_ENTRY(list_head(&files.entry), FILE_LIST, entry);

        msi_move_file((LPCWSTR)file->source, (LPCWSTR)file->dest, options);

        list_remove(&file->entry);
        free_file_entry(file);
    }

    res = TRUE;

done:
    free_list(&files);
    FindClose(hfile);
    return res;
}

static UINT ITERATE_MoveFiles( MSIRECORD *rec, LPVOID param )
{
    MSIPACKAGE *package = param;
    MSICOMPONENT *comp;
    LPCWSTR sourcename, destname;
    LPWSTR sourcedir = NULL, destdir = NULL;
    LPWSTR source = NULL, dest = NULL;
    int options;
    DWORD size;
    BOOL ret, wildcards;

    static const WCHAR backslash[] = {'\\',0};

    comp = get_loaded_component(package, MSI_RecordGetString(rec, 2));
    if (!comp || !comp->Enabled ||
        !(comp->Action & (INSTALLSTATE_LOCAL | INSTALLSTATE_SOURCE)))
    {
        TRACE("Component not set for install, not moving file\n");
        return ERROR_SUCCESS;
    }

    sourcename = MSI_RecordGetString(rec, 3);
    destname = MSI_RecordGetString(rec, 4);
    options = MSI_RecordGetInteger(rec, 7);

    sourcedir = msi_dup_property(package, MSI_RecordGetString(rec, 5));
    if (!sourcedir)
        goto done;

    destdir = msi_dup_property(package, MSI_RecordGetString(rec, 6));
    if (!destdir)
        goto done;

    if (!sourcename)
    {
        if (GetFileAttributesW(sourcedir) == INVALID_FILE_ATTRIBUTES)
            goto done;

        source = strdupW(sourcedir);
        if (!source)
            goto done;
    }
    else
    {
        size = lstrlenW(sourcedir) + lstrlenW(sourcename) + 2;
        source = msi_alloc(size * sizeof(WCHAR));
        if (!source)
            goto done;

        lstrcpyW(source, sourcedir);
        if (source[lstrlenW(source) - 1] != '\\')
            lstrcatW(source, backslash);
        lstrcatW(source, sourcename);
    }

    wildcards = strchrW(source, '*') || strchrW(source, '?');

    if (!destname && !wildcards)
    {
        destname = strdupW(sourcename);
        if (!destname)
            goto done;
    }

    size = 0;
    if (destname)
        size = lstrlenW(destname);

    size += lstrlenW(destdir) + 2;
    dest = msi_alloc(size * sizeof(WCHAR));
    if (!dest)
        goto done;

    lstrcpyW(dest, destdir);
    if (dest[lstrlenW(dest) - 1] != '\\')
        lstrcatW(dest, backslash);

    if (destname)
        lstrcatW(dest, destname);

    if (GetFileAttributesW(destdir) == INVALID_FILE_ATTRIBUTES)
    {
        ret = CreateDirectoryW(destdir, NULL);
        if (!ret)
        {
            WARN("CreateDirectory failed: %d\n", GetLastError());
            return ERROR_SUCCESS;
        }
    }

    if (!wildcards)
        msi_move_file(source, dest, options);
    else
        move_files_wildcard(source, dest, options);

done:
    msi_free(sourcedir);
    msi_free(destdir);
    msi_free(source);
    msi_free(dest);

    return ERROR_SUCCESS;
}

static UINT ACTION_MoveFiles( MSIPACKAGE *package )
{
    UINT rc;
    MSIQUERY *view;

    static const WCHAR ExecSeqQuery[] =
        {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
         '`','M','o','v','e','F','i','l','e','`',0};

    rc = MSI_DatabaseOpenViewW(package->db, ExecSeqQuery, &view);
    if (rc != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    rc = MSI_IterateRecords(view, NULL, ITERATE_MoveFiles, package);
    msiobj_release(&view->hdr);

    return rc;
}

static UINT msi_unimplemented_action_stub( MSIPACKAGE *package,
                                           LPCSTR action, LPCWSTR table )
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ',
        'F','R','O','M',' ','`','%','s','`',0 };
    MSIQUERY *view = NULL;
    DWORD count = 0;
    UINT r;
    
    r = MSI_OpenQuery( package->db, &view, query, table );
    if (r == ERROR_SUCCESS)
    {
        r = MSI_IterateRecords(view, &count, NULL, package);
        msiobj_release(&view->hdr);
    }

    if (count)
        FIXME("%s -> %u ignored %s table values\n",
              action, count, debugstr_w(table));

    return ERROR_SUCCESS;
}

static UINT ACTION_AllocateRegistrySpace( MSIPACKAGE *package )
{
    TRACE("%p\n", package);
    return ERROR_SUCCESS;
}

static UINT ACTION_RemoveIniValues( MSIPACKAGE *package )
{
    static const WCHAR table[] =
         {'R','e','m','o','v','e','I','n','i','F','i','l','e',0 };
    return msi_unimplemented_action_stub( package, "RemoveIniValues", table );
}

static UINT ACTION_PatchFiles( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'P','a','t','c','h',0 };
    return msi_unimplemented_action_stub( package, "PatchFiles", table );
}

static UINT ACTION_BindImage( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'B','i','n','d','I','m','a','g','e',0 };
    return msi_unimplemented_action_stub( package, "BindImage", table );
}

static UINT ACTION_IsolateComponents( MSIPACKAGE *package )
{
    static const WCHAR table[] = {
        'I','s','o','l','a','t','e','C','o','m','p','o','n','e','n','t',0 };
    return msi_unimplemented_action_stub( package, "IsolateComponents", table );
}

static UINT ACTION_MigrateFeatureStates( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'U','p','g','r','a','d','e',0 };
    return msi_unimplemented_action_stub( package, "MigrateFeatureStates", table );
}

static UINT ACTION_SelfUnregModules( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'S','e','l','f','R','e','g',0 };
    return msi_unimplemented_action_stub( package, "SelfUnregModules", table );
}

static UINT ACTION_DeleteServices( MSIPACKAGE *package )
{
    static const WCHAR table[] = {
        'S','e','r','v','i','c','e','C','o','n','t','r','o','l',0 };
    return msi_unimplemented_action_stub( package, "DeleteServices", table );
}
static UINT ACTION_ValidateProductID( MSIPACKAGE *package )
{
	static const WCHAR table[] = {
		'P','r','o','d','u','c','t','I','D',0 };
	return msi_unimplemented_action_stub( package, "ValidateProductID", table );
}

static UINT ACTION_RemoveEnvironmentStrings( MSIPACKAGE *package )
{
    static const WCHAR table[] = {
        'E','n','v','i','r','o','n','m','e','n','t',0 };
    return msi_unimplemented_action_stub( package, "RemoveEnvironmentStrings", table );
}

static UINT ACTION_MsiPublishAssemblies( MSIPACKAGE *package )
{
    static const WCHAR table[] = {
        'M','s','i','A','s','s','e','m','b','l','y',0 };
    return msi_unimplemented_action_stub( package, "MsiPublishAssemblies", table );
}

static UINT ACTION_MsiUnpublishAssemblies( MSIPACKAGE *package )
{
    static const WCHAR table[] = {
        'M','s','i','A','s','s','e','m','b','l','y',0 };
    return msi_unimplemented_action_stub( package, "MsiUnpublishAssemblies", table );
}

static UINT ACTION_UnregisterFonts( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'F','o','n','t',0 };
    return msi_unimplemented_action_stub( package, "UnregisterFonts", table );
}

static UINT ACTION_RMCCPSearch( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'C','C','P','S','e','a','r','c','h',0 };
    return msi_unimplemented_action_stub( package, "RMCCPSearch", table );
}

static UINT ACTION_RegisterComPlus( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'C','o','m','p','l','u','s',0 };
    return msi_unimplemented_action_stub( package, "RegisterComPlus", table );
}

static UINT ACTION_UnregisterComPlus( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'C','o','m','p','l','u','s',0 };
    return msi_unimplemented_action_stub( package, "UnregisterComPlus", table );
}

static UINT ACTION_InstallSFPCatalogFile( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'S','F','P','C','a','t','a','l','o','g',0 };
    return msi_unimplemented_action_stub( package, "InstallSFPCatalogFile", table );
}

static UINT ACTION_RemoveDuplicateFiles( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'D','u','p','l','i','c','a','t','e','F','i','l','e',0 };
    return msi_unimplemented_action_stub( package, "RemoveDuplicateFiles", table );
}

static UINT ACTION_RemoveExistingProducts( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'U','p','g','r','a','d','e',0 };
    return msi_unimplemented_action_stub( package, "RemoveExistingProducts", table );
}

static UINT ACTION_RemoveFolders( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'C','r','e','a','t','e','F','o','l','d','e','r',0 };
    return msi_unimplemented_action_stub( package, "RemoveFolders", table );
}

static UINT ACTION_RemoveODBC( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'O','D','B','C','D','r','i','v','e','r',0 };
    return msi_unimplemented_action_stub( package, "RemoveODBC", table );
}

static UINT ACTION_RemoveRegistryValues( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'R','e','m','o','v','e','R','e','g','i','s','t','r','y',0 };
    return msi_unimplemented_action_stub( package, "RemoveRegistryValues", table );
}

static UINT ACTION_RemoveShortcuts( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'S','h','o','r','t','c','u','t',0 };
    return msi_unimplemented_action_stub( package, "RemoveShortcuts", table );
}

static UINT ACTION_UnpublishComponents( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'P','u','b','l','i','s','h','C','o','m','p','o','n','e','n','t',0 };
    return msi_unimplemented_action_stub( package, "UnpublishComponents", table );
}

static UINT ACTION_UnregisterClassInfo( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'A','p','p','I','d',0 };
    return msi_unimplemented_action_stub( package, "UnregisterClassInfo", table );
}

static UINT ACTION_UnregisterExtensionInfo( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'E','x','t','e','n','s','i','o','n',0 };
    return msi_unimplemented_action_stub( package, "UnregisterExtensionInfo", table );
}

static UINT ACTION_UnregisterMIMEInfo( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'M','I','M','E',0 };
    return msi_unimplemented_action_stub( package, "UnregisterMIMEInfo", table );
}

static UINT ACTION_UnregisterProgIdInfo( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'P','r','o','g','I','d',0 };
    return msi_unimplemented_action_stub( package, "UnregisterProgIdInfo", table );
}

static UINT ACTION_UnregisterTypeLibraries( MSIPACKAGE *package )
{
    static const WCHAR table[] = { 'T','y','p','e','L','i','b',0 };
    return msi_unimplemented_action_stub( package, "UnregisterTypeLibraries", table );
}

static const struct _actions StandardActions[] = {
    { szAllocateRegistrySpace, ACTION_AllocateRegistrySpace },
    { szAppSearch, ACTION_AppSearch },
    { szBindImage, ACTION_BindImage },
    { szCCPSearch, ACTION_CCPSearch },
    { szCostFinalize, ACTION_CostFinalize },
    { szCostInitialize, ACTION_CostInitialize },
    { szCreateFolders, ACTION_CreateFolders },
    { szCreateShortcuts, ACTION_CreateShortcuts },
    { szDeleteServices, ACTION_DeleteServices },
    { szDisableRollback, NULL },
    { szDuplicateFiles, ACTION_DuplicateFiles },
    { szExecuteAction, ACTION_ExecuteAction },
    { szFileCost, ACTION_FileCost },
    { szFindRelatedProducts, ACTION_FindRelatedProducts },
    { szForceReboot, ACTION_ForceReboot },
    { szInstallAdminPackage, NULL },
    { szInstallExecute, ACTION_InstallExecute },
    { szInstallExecuteAgain, ACTION_InstallExecute },
    { szInstallFiles, ACTION_InstallFiles},
    { szInstallFinalize, ACTION_InstallFinalize },
    { szInstallInitialize, ACTION_InstallInitialize },
    { szInstallSFPCatalogFile, ACTION_InstallSFPCatalogFile },
    { szInstallValidate, ACTION_InstallValidate },
    { szIsolateComponents, ACTION_IsolateComponents },
    { szLaunchConditions, ACTION_LaunchConditions },
    { szMigrateFeatureStates, ACTION_MigrateFeatureStates },
    { szMoveFiles, ACTION_MoveFiles },
    { szMsiPublishAssemblies, ACTION_MsiPublishAssemblies },
    { szMsiUnpublishAssemblies, ACTION_MsiUnpublishAssemblies },
    { szInstallODBC, ACTION_InstallODBC },
    { szInstallServices, ACTION_InstallServices },
    { szPatchFiles, ACTION_PatchFiles },
    { szProcessComponents, ACTION_ProcessComponents },
    { szPublishComponents, ACTION_PublishComponents },
    { szPublishFeatures, ACTION_PublishFeatures },
    { szPublishProduct, ACTION_PublishProduct },
    { szRegisterClassInfo, ACTION_RegisterClassInfo },
    { szRegisterComPlus, ACTION_RegisterComPlus},
    { szRegisterExtensionInfo, ACTION_RegisterExtensionInfo },
    { szRegisterFonts, ACTION_RegisterFonts },
    { szRegisterMIMEInfo, ACTION_RegisterMIMEInfo },
    { szRegisterProduct, ACTION_RegisterProduct },
    { szRegisterProgIdInfo, ACTION_RegisterProgIdInfo },
    { szRegisterTypeLibraries, ACTION_RegisterTypeLibraries },
    { szRegisterUser, ACTION_RegisterUser },
    { szRemoveDuplicateFiles, ACTION_RemoveDuplicateFiles },
    { szRemoveEnvironmentStrings, ACTION_RemoveEnvironmentStrings },
    { szRemoveExistingProducts, ACTION_RemoveExistingProducts },
    { szRemoveFiles, ACTION_RemoveFiles },
    { szRemoveFolders, ACTION_RemoveFolders },
    { szRemoveIniValues, ACTION_RemoveIniValues },
    { szRemoveODBC, ACTION_RemoveODBC },
    { szRemoveRegistryValues, ACTION_RemoveRegistryValues },
    { szRemoveShortcuts, ACTION_RemoveShortcuts },
    { szResolveSource, ACTION_ResolveSource },
    { szRMCCPSearch, ACTION_RMCCPSearch },
    { szScheduleReboot, NULL },
    { szSelfRegModules, ACTION_SelfRegModules },
    { szSelfUnregModules, ACTION_SelfUnregModules },
    { szSetODBCFolders, NULL },
    { szStartServices, ACTION_StartServices },
    { szStopServices, ACTION_StopServices },
    { szUnpublishComponents, ACTION_UnpublishComponents },
    { szUnpublishFeatures, ACTION_UnpublishFeatures },
    { szUnregisterClassInfo, ACTION_UnregisterClassInfo },
    { szUnregisterComPlus, ACTION_UnregisterComPlus },
    { szUnregisterExtensionInfo, ACTION_UnregisterExtensionInfo },
    { szUnregisterFonts, ACTION_UnregisterFonts },
    { szUnregisterMIMEInfo, ACTION_UnregisterMIMEInfo },
    { szUnregisterProgIdInfo, ACTION_UnregisterProgIdInfo },
    { szUnregisterTypeLibraries, ACTION_UnregisterTypeLibraries },
    { szValidateProductID, ACTION_ValidateProductID },
    { szWriteEnvironmentStrings, ACTION_WriteEnvironmentStrings },
    { szWriteIniValues, ACTION_WriteIniValues },
    { szWriteRegistryValues, ACTION_WriteRegistryValues },
    { NULL, NULL },
};

static BOOL ACTION_HandleStandardAction(MSIPACKAGE *package, LPCWSTR action,
                                        UINT* rc, BOOL force )
{
    BOOL ret = FALSE;
    BOOL run = force;
    int i;

    if (!run && !package->script->CurrentlyScripting)
        run = TRUE;

    if (!run)
    {
        if (strcmpW(action,szInstallFinalize) == 0 ||
            strcmpW(action,szInstallExecute) == 0 ||
            strcmpW(action,szInstallExecuteAgain) == 0)
                run = TRUE;
    }

    i = 0;
    while (StandardActions[i].action != NULL)
    {
        if (strcmpW(StandardActions[i].action, action)==0)
        {
            if (!run)
            {
                ui_actioninfo(package, action, TRUE, 0);
                *rc = schedule_action(package,INSTALL_SCRIPT,action);
                ui_actioninfo(package, action, FALSE, *rc);
            }
            else
            {
                ui_actionstart(package, action);
                if (StandardActions[i].handler)
                {
                    *rc = StandardActions[i].handler(package);
                }
                else
                {
                    FIXME("unhandled standard action %s\n",debugstr_w(action));
                    *rc = ERROR_SUCCESS;
                }
            }
            ret = TRUE;
            break;
        }
        i++;
    }
    return ret;
}
