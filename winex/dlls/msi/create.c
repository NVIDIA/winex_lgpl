/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2004 Mike McCormack for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wine/debug.h"
#include "msi.h"
#include "msiquery.h"
#include "objbase.h"
#include "objidl.h"
#include "msipriv.h"
#include "winnls.h"

#include "query.h"

WINE_DEFAULT_DEBUG_CHANNEL(msidb);


/* below is the query interface to a table */

typedef struct tagMSICREATEVIEW
{
    MSIVIEW          view;
    MSIDATABASE     *db;
    LPWSTR           name;
    BOOL             bIsTemp;
    column_info     *col_info;
} MSICREATEVIEW;

static UINT CREATE_fetch_int( struct tagMSIVIEW *view, UINT row, UINT col, UINT *val )
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;

    TRACE("%p %d %d %p\n", cv, row, col, val );

    return ERROR_FUNCTION_FAILED;
}

static UINT CREATE_execute( struct tagMSIVIEW *view, MSIRECORD *record )
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;
    MSITABLE *table;

    TRACE("%p Table %s (%s)\n", cv, debugstr_w(cv->name), 
          cv->bIsTemp?"temporary":"permanent");

    return msi_create_table( cv->db, cv->name, cv->col_info, !cv->bIsTemp, &table);
}

static UINT CREATE_close( struct tagMSIVIEW *view )
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;

    TRACE("%p\n", cv);

    return ERROR_SUCCESS;
}

static UINT CREATE_get_dimensions( struct tagMSIVIEW *view, UINT *rows, UINT *cols )
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;

    TRACE("%p %p %p\n", cv, rows, cols );

    return ERROR_FUNCTION_FAILED;
}

static UINT CREATE_get_column_info( struct tagMSIVIEW *view,
                UINT n, LPWSTR *name, UINT *type )
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;

    TRACE("%p %d %p %p\n", cv, n, name, type );

    return ERROR_FUNCTION_FAILED;
}

static UINT CREATE_modify( struct tagMSIVIEW *view, MSIMODIFY eModifyMode,
                           MSIRECORD *rec, UINT row)
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;

    TRACE("%p %d %p\n", cv, eModifyMode, rec );

    return ERROR_FUNCTION_FAILED;
}

static UINT CREATE_delete( struct tagMSIVIEW *view )
{
    MSICREATEVIEW *cv = (MSICREATEVIEW*)view;

    TRACE("%p\n", cv );

    msiobj_release( &cv->db->hdr );
    msi_free( cv );

    return ERROR_SUCCESS;
}

static const MSIVIEWOPS create_ops =
{
    CREATE_fetch_int,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    CREATE_execute,
    CREATE_close,
    CREATE_get_dimensions,
    CREATE_get_column_info,
    CREATE_modify,
    CREATE_delete,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static UINT check_columns( column_info *col_info )
{
    column_info *c1, *c2;

    /* check for two columns with the same name */
    for( c1 = col_info; c1; c1 = c1->next )
        for( c2 = c1->next; c2; c2 = c2->next )
            if (!lstrcmpW(c1->column, c2->column))
                return ERROR_BAD_QUERY_SYNTAX;

    return ERROR_SUCCESS;
}

UINT CREATE_CreateView( MSIDATABASE *db, MSIVIEW **view, LPWSTR table,
                        column_info *col_info, BOOL hold )
{
    MSICREATEVIEW *cv = NULL;
    UINT r;
    const column_info *col;
    BOOL temp = TRUE;

    TRACE("%p\n", cv );

    r = check_columns( col_info );
    if( r != ERROR_SUCCESS )
        return r;

    cv = msi_alloc_zero( sizeof *cv );
    if( !cv )
        return ERROR_FUNCTION_FAILED;

    for( col = col_info; col; col = col->next )
        if( !col->temporary )
        {
            temp = FALSE;
            break;
        }

    /* fill the structure */
    cv->view.ops = &create_ops;
    msiobj_addref( &db->hdr );
    cv->db = db;
    cv->name = table;
    cv->col_info = col_info;
    cv->bIsTemp = temp;
    *view = (MSIVIEW*) cv;

    return ERROR_SUCCESS;
}
