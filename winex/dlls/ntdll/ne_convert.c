/*
 * PE->NE resource conversion functions
 *
 * Copyright 1998 Ulrich Weigand
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <string.h>
#include "windef.h"
#include "wingdi.h"
#include "wine/winuser16.h"
#include "wine/unicode.h"
#include "wine/module.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(resource);

#define ADDPTR(x,y,z) ((x) = (void *)((z *)(x) + (y)))

#define ADDBP(x,y) ADDPTR(x,y,BYTE)
#define INCBP(x) ADDBP(x, 1)
#define ADDDWP(x, y) ADDPTR(x,y,DWORD)
#define INCDWP(x) ADDDWP(x, 1)
#define ADDWP(x, y) ADDPTR(x,y,WORD)
#define INCWP(x) ADDWP(x, 1)
#define ADDLPWSTR(x, y) ADDPTR(x,y,WCHAR)
#define ADDLPSTR(x, y) ADDPTR(x,y,CHAR)

/**********************************************************************
 *	    ConvertDialog32To16   (KERNEL.615)
 *	    ConvertDialog32To16   (KERNEL32.@)
 */
VOID WINAPI ConvertDialog32To16( LPVOID dialog32, DWORD size, LPVOID dialog16 )
{
    LPVOID p = dialog32;
    WORD nbItems, data, dialogEx;
    DWORD style;
    int i;

    memcpy (dialog16, p, sizeof (DWORD));
    style = *((DWORD *)p);
    INCDWP (dialog16);
    INCDWP (p);
    dialogEx = (style == 0xffff0001);  /* DIALOGEX resource */
    if (dialogEx)
    {
        memcpy (dialog16, p, sizeof (DWORD) * 3); /* helpID, exStyle, style */
        ADDDWP (dialog16, 2);
        style = *(DWORD *)dialog16;
        INCDWP (dialog16);
        ADDDWP (p, 3);
    }
    else
        INCDWP (p); /* exStyle ignored in 16-bit standard dialog */

    nbItems = *((BYTE *)dialog16) = (BYTE)*((WORD *)p);
    INCBP (dialog16);
    INCWP (p);
    memcpy (dialog16, p, sizeof (WORD) * 4); /* x, y, cx, cy */
    ADDWP (dialog16, 4);
    ADDWP (p, 4);

    /* Transfer menu & class names */
    for (i = 0; i < 2; i++)
    {
        switch (*((WORD *)p))
        {
        case 0x0000: INCWP (p); *((BYTE *)dialog16) = 0;
                     INCBP (dialog16); break;
        case 0xffff: INCWP (p); *((BYTE *)dialog16) = 0xff; INCBP (dialog16);
                     memcpy (dialog16, p, sizeof (WORD));
                     INCWP (dialog16); INCWP (p); break;
        default:     WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1,
                                          (LPSTR)dialog16, 0x7fffffff,
                                          NULL,NULL );
                     ADDLPSTR (dialog16, strlen ((LPSTR)dialog16) + 1);
                     ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);
                     break;
        }
    }

    /* Transfer window caption */
    WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1, (LPSTR)dialog16,
                         0x7fffffff, NULL,NULL );
    ADDLPSTR (dialog16, strlen ((LPSTR)dialog16) + 1);
    ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);

    /* Transfer font info */
    if (style & DS_SETFONT)
    {
        memcpy (dialog16, p, sizeof (WORD)); /* pointSize */
        INCWP (dialog16);
        INCWP (p);
        if (dialogEx)
        {
            memcpy (dialog16, p, sizeof (WORD) * 2); /* weight, italic */
            ADDWP (dialog16, 2);
            ADDWP (p, 2);
        }
        WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1, (LPSTR)dialog16,
                             0x7fffffff, NULL,NULL );  /* faceName */
        ADDLPSTR (dialog16, strlen ((LPSTR)dialog16) + 1);
        ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);
    }

    /* Transfer dialog items */
    while (nbItems)
    {
        /* align on DWORD boundary (32-bit only) */
        p = (LPVOID)((((int)p) + 3) & ~3);

        if (dialogEx)
        {
            memcpy (dialog16, p, sizeof (DWORD) * 3); /* helpID, exStyle, style */
            ADDDWP (dialog16, 3);
            ADDDWP (p, 3);
        }
        else
        {
            style = *((DWORD *)p); /* save style */
            ADDDWP (p, 2);         /* ignore exStyle */
        }

        memcpy (dialog16, p, sizeof (WORD) * 4); /* x, y, cx, cy */
        ADDWP (dialog16, 4);
        ADDWP (p, 4);

        if (dialogEx)
        {
            memcpy (dialog16, p, sizeof (DWORD)); /* ID */
            INCDWP (dialog16);
            INCDWP (p);
        }
        else
        {
            memcpy (dialog16, p, sizeof (WORD)); /* ID */
            INCWP (dialog16);
            INCWP (p);
            *((DWORD *)dialog16) = style;  /* style from above */
            INCDWP (dialog16);
        }

        /* Transfer class name */
        switch (*((WORD *)p))
        {
        case 0x0000:  INCWP (p); *((BYTE *)dialog16) = 0;
                      INCBP (dialog16); break;
        case 0xffff:  INCWP (p);
                      *((BYTE *)dialog16) = (BYTE)*((WORD *)p);
                      INCBP (dialog16); INCWP (p); break;
        default:      WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1,
                                           (LPSTR)dialog16, 0x7fffffff,
                                           NULL, NULL );
                      ADDLPSTR (dialog16, strlen ((LPSTR)dialog16) + 1);
                      ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);
                      break;
        }

        /* Transfer window name */
        switch (*((WORD *)p))
        {
        case 0x0000:  INCWP (p); *((BYTE *)dialog16) = 0;
                      INCBP (dialog16); break;
        case 0xffff:  INCWP (p); *((BYTE *)dialog16) = 0xff; INCBP (dialog16);
                      memcpy (dialog16, p, sizeof (WORD));
                      INCWP (dialog16); INCWP (p); break;
        default:      WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1,
                                           (LPSTR)dialog16, 0x7fffffff,
                                           NULL, NULL );
                      ADDLPSTR (dialog16, strlen ((LPSTR)dialog16) + 1);
                      ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);
                      break;
        }

        /* Transfer data */
        data = *((WORD *)p);
        INCWP (p);
        if (dialogEx)
        {
            *((WORD *)dialog16) = data;
            INCWP (dialog16);
        }
        else
        {
            *((BYTE *)dialog16) = (BYTE)data;
            INCBP (dialog16);
        }

        if (data)
        {
            memcpy( dialog16, p, data );
            ADDLPSTR (dialog16, data);
            ADDLPSTR (p, data);
        }

        /* Next item */
        nbItems--;
    }
}

/**********************************************************************
 *	    GetDialog32Size   (KERNEL.618)
 */
WORD WINAPI GetDialog32Size16( LPVOID dialog32 )
{
    LPVOID p = dialog32;
    WORD nbItems, data, dialogEx;
    DWORD style;
    int i;

    style = *((DWORD *)p);
    INCDWP (p);
    dialogEx = (style == 0xffff0001);  /* DIALOGEX resource */
    if (dialogEx)
    {
        ADDDWP (p, 2); /* skip helpID, exStyle */
        style = *((DWORD *)p); /* style */
        INCDWP (p);
    }
    
    else
        INCDWP (p); /* skip exStyle */

    nbItems = *((WORD *)p);
    INCWP (p);
    ADDWP (p, 4); /* x, y, cx, cy */

    /* Skip menu & class names */
    for (i = 0; i < 2; i++)
    {
        switch (*((WORD *)p))
        {
        case 0x0000:  INCWP (p); break;
        case 0xffff:  ADDWP (p, 2); break;
        default:      ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1); break;
        }
    }

    /* Skip window caption */
    ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);

    /* Skip font info */
    if (style & DS_SETFONT)
    {
        INCWP (p);  /* pointSize */
        if (dialogEx)
            ADDWP (p, 2); /* weight, italic */
        ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1); /* faceName */
    }

    /* Skip dialog items */
    while (nbItems)
    {
        /* align on DWORD boundary */
        p = (LPVOID)((((int)p) + 3) & ~3);

        if (dialogEx)
            ADDDWP (p, 3); /* helpID, exStyle, style */
        else
            ADDDWP (p, 2); /* style, exStyle */

        ADDWP (p, 4); /* x, y, cx, cy */

        if (dialogEx)
            INCDWP (p); /* ID */
        else
            INCWP (p); /* ID */

        /* Skip class & window names */
        for (i = 0; i < 2; i++)
        {
            switch (*((WORD *)p))
            {
            case 0x0000:  INCWP (p); break;
            case 0xffff:  ADDWP (p, 2); break;
            default:      ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1); break;
            }
        }

        /* Skip data */
        data = *((WORD *)p);
        INCWP (p);
        ADDLPSTR (p, data);

        /* Next item */
        nbItems--;
    }

    return (WORD)((LPSTR)p - (LPSTR)dialog32);
}

/**********************************************************************
 *	    ConvertMenu32To16   (KERNEL.616)
 */
VOID WINAPI ConvertMenu32To16( LPVOID menu32, DWORD size, LPVOID menu16 )
{
    LPVOID p = menu32;
    WORD version, headersize, flags, level = 1;

    memcpy (menu16, p, sizeof (WORD) * 2); /* copy version & headersize */
    version = *((WORD *)menu16);
    INCWP (menu16);
    headersize = *((WORD *)menu16);
    INCWP (menu16);
    ADDWP (p, 2);

    if ( headersize )
    {
        memcpy( menu16, p, headersize );
        ADDLPSTR (menu16, headersize);
        ADDLPSTR (p, headersize );
    }

    while ( level )
        if ( version == 0 )  /* standard */
        {
            memcpy (menu16, p, sizeof (WORD));
            flags = *((WORD *)menu16);
            INCWP (menu16);
            INCWP (p);
            if ( !(flags & MF_POPUP) )
            {
                memcpy (menu16, p, sizeof (WORD)); /* ID */
                INCWP (menu16);
                INCWP (p);
            }
            else
                level++;

            WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1, (LPSTR)menu16,
                                 0x7fffffff, NULL, NULL );
            ADDLPSTR (menu16, strlen ((LPSTR)menu16) + 1);
            ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);

            if ( flags & MF_END )
                level--;
        }
        else  /* extended */
        {
            memcpy (menu16, p, sizeof (DWORD) * 2); /* fType, fState */
            ADDDWP (menu16, 2);
            ADDDWP (p, 2);
            *((WORD *)menu16) = (WORD)*((DWORD *)p); /* ID */
            INCWP (menu16);
            INCDWP (p);
            flags = *((BYTE *)menu16) = (BYTE)*((WORD *)p);
            INCBP (menu16);
            INCWP (p);

            WideCharToMultiByte( CP_ACP, 0, (LPWSTR)p, -1, (LPSTR)menu16,
                                 0x7fffffff, NULL, NULL );
            ADDLPSTR (menu16, strlen ((LPSTR)menu16) + 1);
            ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);

            /* align on DWORD boundary (32-bit only) */
            p = (LPVOID)((((int)p) + 3) & ~3);

            /* If popup, transfer helpid */
            if ( flags & 1)
            {
                memcpy (menu16, p, sizeof (DWORD));
                INCDWP (menu16);
                INCDWP (p);
                level++;
            }

            if ( flags & MF_END )
                level--;
        }
}

/**********************************************************************
 *	    GetMenu32Size   (KERNEL.617)
 */
WORD WINAPI GetMenu32Size16( LPVOID menu32 )
{
    LPVOID p = menu32;
    WORD version, headersize, flags, level = 1;

    version = *((WORD *)p);
    INCWP (p);
    headersize = *((WORD *)p);
    INCWP (p);
    ADDLPSTR (p, headersize);

    while ( level )
        if ( version == 0 )  /* standard */
        {
            flags = *((WORD *)p);
            INCWP (p);
            if ( !(flags & MF_POPUP) )
                INCWP (p);  /* ID */
            else
                level++;

            ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);

            if ( flags & MF_END )
                level--;
        }
        else  /* extended */
        {
            ADDDWP (p, 3); /* fType, fState, ID */
            flags = *((WORD *)p);
            INCWP (p);

            ADDLPWSTR (p, strlenW ((LPWSTR)p) + 1);

            /* align on DWORD boundary (32-bit only) */
            p = (LPVOID)((((int)p) + 3) & ~3);

            /* If popup, skip helpid */
            if ( flags & 1)
            {
                INCDWP (p);
                level++;
            }

            if ( flags & MF_END )
                level--;
        }

    return (WORD)((LPSTR)p - (LPSTR)menu32);
}

/**********************************************************************
 *	    ConvertAccelerator32To16
 */
VOID ConvertAccelerator32To16( LPVOID acc32, DWORD size, LPVOID acc16 )
{
    int type;

    do
    {
        /* Copy type */
        memcpy (acc16, acc32, sizeof (BYTE));
        type = *((BYTE *)acc16);
        INCBP (acc16);
        INCBP (acc32);

        /* Skip padding */
        INCBP (acc32);

        /* Copy event and IDval */
        memcpy (acc16, acc32, sizeof (WORD) * 2);
        ADDWP (acc16, 2);
        ADDWP (acc32, 2);

        /* Skip padding */
        INCWP (acc32);

    } while ( !( type & 0x80 ) );
}

/**********************************************************************
 *	    NE_LoadPEResource
 */
HGLOBAL16 NE_LoadPEResource( NE_MODULE *pModule, WORD type, LPVOID bits, DWORD size )
{
    HGLOBAL16 handle;

    TRACE("module=%04x type=%04x\n", pModule->self, type );
    if (!pModule || !bits || !size) return 0;

    handle = GlobalAlloc16( 0, size );

    switch (type)
    {
    case RT_MENU16:
        ConvertMenu32To16( bits, size, GlobalLock16( handle ) );
        break;

    case RT_DIALOG16:
        ConvertDialog32To16( bits, size, GlobalLock16( handle ) );
        break;

    case RT_ACCELERATOR16:
        ConvertAccelerator32To16( bits, size, GlobalLock16( handle ) );
        break;

    case RT_STRING16:
        FIXME("not yet implemented!\n" );
        /* fall through */

    default:
        memcpy( GlobalLock16( handle ), bits, size );
        break;
    }

    return handle;
}

