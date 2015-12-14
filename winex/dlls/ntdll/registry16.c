/*
 * 	Registry Functions
 *
 * Copyright 1996 Marcus Meissner
 * Copyright 1998 Matthew Becker
 * Copyright 1999 Sylvain St-Germain
 * Copyright (C) 2007 TransGaming Technologies
 *
 * December 21, 1997 - Kevin Cozens
 * Fixed bugs in the _w95_loadreg() function. Added extra information
 * regarding the format of the Windows '95 registry files.
 *
 * NOTES
 *    When changing this file, please re-run the regtest program to ensure
 *    the conditions are handled properly.
 *
 * TODO
 *    Security access
 *    Option handling
 *    Time for RegEnumKey*, RegQueryInfoKey*
 */

#include "config.h"
#include "wine/port.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "winerror.h"
#include "winnt.h"
#include "winreg.h"

#include "wine/winbase16.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "wine/file.h"
#include "drive.h"
#include "options.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);

/* FIXME: following defines should be configured global */
#define SAVE_GLOBAL_REGBRANCH_USER_DEFAULT  ETCDIR"/wine.userreg"
#define SAVE_GLOBAL_REGBRANCH_LOCAL_MACHINE ETCDIR"/wine.systemreg"

/* relative in ~user/.wine/ : */
#define SAVE_LOCAL_REGBRANCH_CURRENT_USER  "user.reg"
#define SAVE_LOCAL_REGBRANCH_USER_DEFAULT  "userdef.reg"
#define SAVE_LOCAL_REGBRANCH_LOCAL_MACHINE "system.reg"
#define SAVE_LOCAL_REGBRANCH_DYN_DATA "dyndata.reg"

/* _xmalloc [Internal] */
static void *_xmalloc( size_t size )
{
    void *res;

    res = malloc (size ? size : 1);
    if (res == NULL) {
        WARN("Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

/* _strdupnA [Internal] */
static LPSTR _strdupnA(LPCSTR str,size_t len)
{
    LPSTR ret;

    if (!str) return NULL;
    ret = _xmalloc( len + 1 );
    memcpy( ret, str, len );
    ret[len] = 0x00;
    return ret;
}

/* convert ansi string to unicode [Internal] */
static LPWSTR _strdupnAtoW(LPCSTR strA,size_t lenA)
{
    LPWSTR ret;
    size_t lenW;

    if (!strA) return NULL;
    lenW = MultiByteToWideChar(CP_ACP,0,strA,lenA,NULL,0);
    ret = _xmalloc(lenW*sizeof(WCHAR)+sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP,0,strA,lenA,ret,lenW);
    ret[lenW] = 0;
    return ret;
}

/* dump a Unicode string with proper escaping [Internal] */
/* FIXME: this code duplicates server/unicode.c */
static int _dump_strW(const WCHAR *str,size_t len,FILE *f,char escape[2])
{
    static const char escapes[32] = ".......abtnvfr.............e....";
    char buffer[256];
    LPSTR pos = buffer;
    int count = 0;

    for (; len; str++, len--)
    {
        if (pos > buffer + sizeof(buffer) - 8)
        {
            fwrite( buffer, pos - buffer, 1, f );
            count += pos - buffer;
            pos = buffer;
        }
        if (*str > 127)  /* hex escape */
        {
            if (len > 1 && str[1] < 128 && isxdigit((char)str[1]))
                pos += sprintf( pos, "\\x%04x", *str );
            else
                pos += sprintf( pos, "\\x%x", *str );
            continue;
        }
        if (*str < 32)  /* octal or C escape */
        {
            if (!*str && len == 1) continue;  /* do not output terminating NULL */
            if (escapes[*str] != '.')
                pos += sprintf( pos, "\\%c", escapes[*str] );
            else if (len > 1 && str[1] >= '0' && str[1] <= '7')
                pos += sprintf( pos, "\\%03o", *str );
            else
                pos += sprintf( pos, "\\%o", *str );
            continue;
        }
        if (*str == '\\' || *str == escape[0] || *str == escape[1]) *pos++ = '\\';
        *pos++ = *str;
    }
    fwrite( buffer, pos - buffer, 1, f );
    count += pos - buffer;
    return count;
}

/* convert ansi string to unicode and dump with proper escaping [Internal] */
static int _dump_strAtoW(LPCSTR strA,size_t len,FILE *f,char escape[2])
{
    WCHAR *strW;
    int ret;

    if (strA == NULL) return 0;
    strW = _strdupnAtoW(strA,len);
    ret = _dump_strW(strW,len,f,escape);
    free(strW);
    return ret;
}

/* a key value */
/* FIXME: this code duplicates server/registry.c */
struct key_value {
    WCHAR            *nameW;   /* value name */
    int               type;    /* value type */
    size_t            len;     /* value data length in bytes */
    void             *data;    /* pointer to value data */
};

/* dump a value to a text file */
/* FIXME: this code duplicates server/registry.c */
static void _dump_value(struct key_value *value,FILE *f)
{
    int i, count;

    if (value->nameW[0]) {
        fputc( '\"', f );
        count = 1 + _dump_strW(value->nameW,strlenW(value->nameW),f,"\"\"");
        count += fprintf( f, "\"=" );
    }
    else count = fprintf( f, "@=" );

    switch(value->type) {
        case REG_SZ:
        case REG_EXPAND_SZ:
        case REG_MULTI_SZ:
            if (value->type != REG_SZ) fprintf( f, "str(%d):", value->type );
            fputc( '\"', f );
            if (value->data) _dump_strW(value->data,value->len/sizeof(WCHAR),f,"\"\"");
            fputc( '\"', f );
            break;
        case REG_DWORD:
            if (value->len == sizeof(DWORD)) {
                DWORD dw;
                memcpy( &dw, value->data, sizeof(DWORD) );
                fprintf( f, "dword:%08lx", dw );
                break;
            }
            /* else fall through */
        default:
            if (value->type == REG_BINARY) count += fprintf( f, "hex:" );
            else count += fprintf( f, "hex(%x):", value->type );
            for (i = 0; i < value->len; i++) {
                count += fprintf( f, "%02x", *((unsigned char *)value->data + i) );
                if (i < value->len-1) {
                    fputc( ',', f );
                    if (++count > 76) {
                        fprintf( f, "\\\n  " );
                        count = 2;
                    }
                }
            }
            break;
    }
    fputc( '\n', f );
}

/******************************************************************/
/* WINDOWS 31 REGISTRY LOADER, supplied by Tor Sj�wall, tor@sn.no */
/*
    reghack - windows 3.11 registry data format demo program.

    The reg.dat file has 3 parts, a header, a table of 8-byte entries that is
    a combined hash table and tree description, and finally a text table.

    The header is obvious from the struct header. The taboff1 and taboff2
    fields are always 0x20, and their usage is unknown.

    The 8-byte entry table has various entry types.

    tabent[0] is a root index. The second word has the index of the root of
            the directory.
    tabent[1..hashsize] is a hash table. The first word in the hash entry is
            the index of the key/value that has that hash. Data with the same
            hash value are on a circular list. The other three words in the
            hash entry are always zero.
    tabent[hashsize..tabcnt] is the tree structure. There are two kinds of
            entry: dirent and keyent/valent. They are identified by context.
    tabent[freeidx] is the first free entry. The first word in a free entry
            is the index of the next free entry. The last has 0 as a link.
            The other three words in the free list are probably irrelevant.

    Entries in text table are preceded by a word at offset-2. This word
    has the value (2*index)+1, where index is the referring keyent/valent
    entry in the table. I have no suggestion for the 2* and the +1.
    Following the word, there are N bytes of data, as per the keyent/valent
    entry length. The offset of the keyent/valent entry is from the start
    of the text table to the first data byte.

    This information is not available from Microsoft. The data format is
    deduced from the reg.dat file by me. Mistakes may
    have been made. I claim no rights and give no guarantees for this program.

    Tor Sj�wall, tor@sn.no
*/

/* reg.dat header format */
struct _w31_header {
    char		cookie[8];	/* 'SHCC3.10' */
    unsigned long	taboff1;	/* offset of hash table (??) = 0x20 */
    unsigned long	taboff2;	/* offset of index table (??) = 0x20 */
    unsigned long	tabcnt;		/* number of entries in index table */
    unsigned long	textoff;	/* offset of text part */
    unsigned long	textsize;	/* byte size of text part */
    unsigned short	hashsize;	/* hash size */
    unsigned short	freeidx;	/* free index */
};

/* generic format of table entries */
struct _w31_tabent {
    unsigned short w0, w1, w2, w3;
};

/* directory tabent: */
struct _w31_dirent {
    unsigned short	sibling_idx;	/* table index of sibling dirent */
    unsigned short	child_idx;	/* table index of child dirent */
    unsigned short	key_idx;	/* table index of key keyent */
    unsigned short	value_idx;	/* table index of value valent */
};

/* key tabent: */
struct _w31_keyent {
    unsigned short	hash_idx;	/* hash chain index for string */
    unsigned short	refcnt;		/* reference count */
    unsigned short	length;		/* length of string */
    unsigned short	string_off;	/* offset of string in text table */
};

/* value tabent: */
struct _w31_valent {
    unsigned short	hash_idx;	/* hash chain index for string */
    unsigned short	refcnt;		/* reference count */
    unsigned short	length;		/* length of string */
    unsigned short	string_off;	/* offset of string in text table */
};

/* recursive helper function to display a directory tree  [Internal] */
void _w31_dumptree(unsigned short idx,unsigned char *txt,struct _w31_tabent *tab,struct _w31_header *head,HKEY hkey,time_t lastmodified, int level)
{
    struct _w31_dirent *dir;
    struct _w31_keyent *key;
    struct _w31_valent *val;
    HKEY subkey = 0;
    static char	tail[400];

    while (idx!=0) {
        dir=(struct _w31_dirent*)&tab[idx];

        if (dir->key_idx) {
            key = (struct _w31_keyent*)&tab[dir->key_idx];

            memcpy(tail,&txt[key->string_off],key->length);
            tail[key->length]='\0';
            /* all toplevel entries AND the entries in the
             * toplevel subdirectory belong to \SOFTWARE\Classes
             */
            if (!level && !strcmp(tail,".classes")) {
                _w31_dumptree(dir->child_idx,txt,tab,head,hkey,lastmodified,level+1);
                idx=dir->sibling_idx;
                continue;
            }
            if (subkey) RegCloseKey( subkey );
            if (RegCreateKeyA( hkey, tail, &subkey ) != ERROR_SUCCESS) subkey = 0;
            /* only add if leaf node or valued node */
            if (dir->value_idx!=0||dir->child_idx==0) {
                if (dir->value_idx) {
                    val=(struct _w31_valent*)&tab[dir->value_idx];
                    memcpy(tail,&txt[val->string_off],val->length);
                    tail[val->length]='\0';
                    RegSetValueA( subkey, NULL, REG_SZ, tail, 0 );
                }
            }
        } else TRACE("strange: no directory key name, idx=%04x\n", idx);
        _w31_dumptree(dir->child_idx,txt,tab,head,subkey,lastmodified,level+1);
        idx=dir->sibling_idx;
    }
    if (subkey) RegCloseKey( subkey );
}


/******************************************************************************
 * _w31_loadreg [Internal]
 */
void _w31_loadreg(void)
{
    HFILE hf;
    struct _w31_header	head;
    struct _w31_tabent	*tab;
    unsigned char		*txt;
    unsigned int		len;
    OFSTRUCT		ofs;
    BY_HANDLE_FILE_INFORMATION hfinfo;
    time_t			lastmodified;

    TRACE("(void)\n");

    hf = OpenFile("reg.dat",&ofs,OF_READ);
    if (hf==HFILE_ERROR) return;

    /* read & dump header */
    if (sizeof(head)!=_lread(hf,&head,sizeof(head))) {
        ERR("reg.dat is too short.\n");
        _lclose(hf);
        return;
    }
    if (memcmp(head.cookie, "SHCC3.10", sizeof(head.cookie))!=0) {
        ERR("reg.dat has bad signature.\n");
        _lclose(hf);
        return;
    }

    len = head.tabcnt * sizeof(struct _w31_tabent);
    /* read and dump index table */
    tab = _xmalloc(len);
    if (len!=_lread(hf,tab,len)) {
        ERR("couldn't read %d bytes.\n",len);
        free(tab);
        _lclose(hf);
        return;
    }

    /* read text */
    txt = _xmalloc(head.textsize);
    if (-1==_llseek(hf,head.textoff,SEEK_SET)) {
        ERR("couldn't seek to textblock.\n");
        free(tab);
        free(txt);
        _lclose(hf);
        return;
    }
    if (head.textsize!=_lread(hf,txt,head.textsize)) {
        ERR("textblock too short (%d instead of %ld).\n",len,head.textsize);
        free(tab);
        free(txt);
        _lclose(hf);
        return;
    }

    if (!GetFileInformationByHandle(hf,&hfinfo)) {
        ERR("GetFileInformationByHandle failed?.\n");
        free(tab);
        free(txt);
        _lclose(hf);
        return;
    }
    lastmodified = DOSFS_FileTimeToUnixTime(&hfinfo.ftLastWriteTime,NULL);
    _w31_dumptree(tab[0].w1,txt,tab,&head,HKEY_CLASSES_ROOT,lastmodified,0);
    free(tab);
    free(txt);
    _lclose(hf);
    return;
}

/***********************************************************************************/
/*                        windows 95 registry loader                               */
/***********************************************************************************/

/* SECTION 1: main header
 *
 * once at offset 0
 */
#define	W95_REG_CREG_ID	0x47455243

typedef struct {
    DWORD	id;		/* "CREG" = W95_REG_CREG_ID */
    DWORD	version;	/* ???? 0x00010000 */
    DWORD	rgdb_off;	/* 0x08 Offset of 1st RGDB-block */
    DWORD	uk2;		/* 0x0c */
    WORD	rgdb_num;	/* 0x10 # of RGDB-blocks */
    WORD	uk3;
    DWORD	uk[3];
    /* rgkn */
} _w95creg;

/* SECTION 2: Directory information (tree structure)
 *
 * once on offset 0x20
 *
 * structure: [rgkn][dke]*	(repeat till last_dke is reached)
 */
#define	W95_REG_RGKN_ID	0x4e4b4752

typedef struct {
    DWORD	id;		/*"RGKN" = W95_REG_RGKN_ID */
    DWORD	size;		/* Size of the RGKN-block */
    DWORD	root_off;	/* Rel. Offset of the root-record */
    DWORD   last_dke;       /* Offset to last DKE ? */
    DWORD	uk[4];
} _w95rgkn;

/* Disk Key Entry Structure
 *
 * the 1st entry in a "usual" registry file is a nul-entry with subkeys: the
 * hive itself. It looks the same like other keys. Even the ID-number can
 * be any value.
 *
 * The "hash"-value is a value representing the key's name. Windows will not
 * search for the name, but for a matching hash-value. if it finds one, it
 * will compare the actual string info, otherwise continue with the next key.
 * To calculate the hash initialize a D-Word with 0 and add all ASCII-values
 * of the string which are smaller than 0x80 (128) to this D-Word.
 *
 * If you want to modify key names, also modify the hash-values, since they
 * cannot be found again (although they would be displayed in REGEDIT)
 * End of list-pointers are filled with 0xFFFFFFFF
 *
 * Disk keys are layed out flat ... But, sometimes, nrLS and nrMS are both
 * 0xFFFF, which means skipping over nextkeyoffset bytes (including this
 * structure) and reading another RGDB_section.
 *
 * The last DKE (see field last_dke in _w95_rgkn) has only 3 DWORDs with
 * 0x80000000 (EOL indicator ?) as x1, the hash value and 0xFFFFFFFF as x3.
 * The remaining space between last_dke and the offset calculated from
 * rgkn->size seems to be free for use for more dke:s.
 * So it seems if more dke:s are added, they are added to that space and
 * last_dke is grown, and in case that "free" space is out, the space
 * gets grown and rgkn->size gets adjusted.
 *
 * there is a one to one relationship between dke and dkh
 */
 /* key struct, once per key */
typedef struct {
    DWORD	x1;		/* Free entry indicator(?) */
    DWORD	hash;		/* sum of bytes of keyname */
    DWORD	x3;		/* Root key indicator? usually 0xFFFFFFFF */
    DWORD	prevlvl;	/* offset of previous key */
    DWORD	nextsub;	/* offset of child key */
    DWORD	next;		/* offset of sibling key */
    WORD	nrLS;		/* id inside the rgdb block */
    WORD	nrMS;		/* number of the rgdb block */
} _w95dke;

/* SECTION 3: key information, values and data
 *
 * structure:
 *  section:	[blocks]*		(repeat creg->rgdb_num times)
 *  blocks:	[rgdb] [subblocks]* 	(repeat till block size reached )
 *  subblocks:	[dkh] [dkv]*		(repeat dkh->values times )
 *
 * An interesting relationship exists in RGDB_section. The DWORD value
 * at offset 0x10 equals the one at offset 0x04 minus the one at offset 0x08.
 * I have no idea at the moment what this means.  (Kevin Cozens)
 */

/* block header, once per block */
#define W95_REG_RGDB_ID	0x42444752

typedef struct {
    DWORD	id;	/* 0x00 'RGDB' = W95_REG_RGDB_ID */
    DWORD	size;	/* 0x04 */
    DWORD	uk1;	/* 0x08 */
    DWORD	uk2;	/* 0x0c */
    DWORD	uk3;	/* 0x10 */
    DWORD	uk4;	/* 0x14 */
    DWORD	uk5;	/* 0x18 */
    DWORD	uk6;	/* 0x1c */
    /* dkh */
} _w95rgdb;

/* Disk Key Header structure (RGDB part), once per key */
typedef	struct {
    DWORD	nextkeyoff; 	/* 0x00 offset to next dkh */
    WORD	nrLS;		/* 0x04 id inside the rgdb block */
    WORD	nrMS;		/* 0x06 number of the rgdb block */
    DWORD	bytesused;	/* 0x08 */
    WORD	keynamelen;	/* 0x0c len of name */
    WORD	values;		/* 0x0e number of values */
    DWORD	xx1;		/* 0x10 */
    char	name[1];	/* 0x14 */
    /* dkv */		/* 0x14 + keynamelen */
} _w95dkh;

/* Disk Key Value structure, once per value */
typedef	struct {
    DWORD	type;		/* 0x00 */
    DWORD	x1;		/* 0x04 */
    WORD	valnamelen;	/* 0x08 length of name, 0 is default key */
    WORD	valdatalen;	/* 0x0A length of data */
    char	name[1];	/* 0x0c */
    /* raw data */		/* 0x0c + valnamelen */
} _w95dkv;

/******************************************************************************
 * _w95_lookup_dkh [Internal]
 *
 * seeks the dkh belonging to a dke
 */
static _w95dkh *_w95_lookup_dkh(_w95creg *creg,int nrLS,int nrMS)
{
    _w95rgdb * rgdb;
    _w95dkh * dkh;
    int i;

    /* get the beginning of the rgdb datastore */
    rgdb = (_w95rgdb*)((char*)creg+creg->rgdb_off);

    /* check: requested block < last_block) */
    if (creg->rgdb_num <= nrMS) {
        ERR("registry file corrupt! requested block no. beyond end.\n");
        goto error;
    }

    /* find the right block */
    for(i=0; i<nrMS ;i++) {
        if(rgdb->id != W95_REG_RGDB_ID) {  /* check the magic */
            ERR("registry file corrupt! bad magic 0x%08lx\n", rgdb->id);
            goto error;
        }
        rgdb = (_w95rgdb*) ((char*)rgdb+rgdb->size);		/* find next block */
    }

    dkh = (_w95dkh*)(rgdb + 1);				/* first sub block within the rgdb */

    do {
        if(nrLS==dkh->nrLS ) return dkh;
        dkh = (_w95dkh*)((char*)dkh + dkh->nextkeyoff);	/* find next subblock */
    } while ((char *)dkh < ((char*)rgdb+rgdb->size));

error:
    return NULL;
}

/******************************************************************************
 * _w95_dump_dkv [Internal]
 */
static int _w95_dump_dkv(_w95dkh *dkh,int nrLS,int nrMS,FILE *f)
{
    _w95dkv * dkv;
    int i;

    /* first value block */
    dkv = (_w95dkv*)((char*)dkh+dkh->keynamelen+0x14);

    /* loop through the values */
    for (i=0; i< dkh->values; i++) {
        struct key_value value;
        WCHAR *pdata;

        value.nameW = _strdupnAtoW(dkv->name,dkv->valnamelen);
        value.type = dkv->type;
        value.len = dkv->valdatalen;

        value.data = &(dkv->name[dkv->valnamelen]);
        pdata = NULL;
        if ( (value.type==REG_SZ) || (value.type==REG_EXPAND_SZ) || (value.type==REG_MULTI_SZ) ) {
            pdata = _strdupnAtoW(value.data,value.len);
            value.len *= 2;
        }
        if (pdata != NULL) value.data = pdata;

        _dump_value(&value,f);
        free(value.nameW);
        if (pdata != NULL) free(pdata);

        /* next value */
        dkv = (_w95dkv*)((char*)dkv+dkv->valnamelen+dkv->valdatalen+0x0c);
    }
    return TRUE;
}

/******************************************************************************
 * _w95_dump_dke [Internal]
 */
static int _w95_dump_dke(LPSTR key_name,_w95creg *creg,_w95rgkn *rgkn,_w95dke *dke,FILE *f,int level)
{
    _w95dkh * dkh;
    LPSTR new_key_name = NULL;

    /* special root key */
    if (dke->nrLS == 0xffff || dke->nrMS==0xffff)		/* eg. the root key has no name */
    {
        /* parse the one subkey */
        if (dke->nextsub != 0xffffffff) return _w95_dump_dke(key_name, creg, rgkn, (_w95dke*)((char*)rgkn+dke->nextsub),f,level);
        /* has no sibling keys */
        return FALSE;
    }

    /* search subblock */
    if (!(dkh = _w95_lookup_dkh(creg, dke->nrLS, dke->nrMS))) {
        ERR("dke pointing to missing dkh !\n");
        return FALSE;
    }

    if (level <= 0) {
        /* create new subkey name */
        new_key_name = _strdupnA(key_name,strlen(key_name)+dkh->keynamelen+1);
        if (strcmp(new_key_name,"") != 0) strcat(new_key_name,"\\");
        strncat(new_key_name,dkh->name,dkh->keynamelen);

        /* walk sibling keys */
        if (dke->next != 0xffffffff ) {
            if (!_w95_dump_dke(key_name, creg, rgkn, (_w95dke*)((char*)rgkn+dke->next),f,level)) {
                free(new_key_name);
                return FALSE;
            }
        }

        /* write the key path (something like [Software\\Microsoft\\..]) only if:
           1) key has some values
           2) key has no values and no subkeys
        */
        if (dkh->values > 0) {
            /* there are some values */
            fprintf(f,"\n[");
            _dump_strAtoW(new_key_name,strlen(new_key_name),f,"[]");
            fprintf(f,"]\n");
            if (!_w95_dump_dkv(dkh, dke->nrLS, dke->nrMS,f)) {
              free(new_key_name);
              return FALSE;
            }
        }
        if ((dke->nextsub == 0xffffffff) && (dkh->values == 0)) {
            /* no subkeys and no values */
            fprintf(f,"\n[");
            _dump_strAtoW(new_key_name,strlen(new_key_name),f,"[]");
            fprintf(f,"]\n");
        }
    } else new_key_name = _strdupnA(key_name,strlen(key_name));

    /* next sub key */
    if (dke->nextsub != 0xffffffff) {
        if (!_w95_dump_dke(new_key_name, creg, rgkn, (_w95dke*)((char*)rgkn+dke->nextsub),f,level-1)) {
          free(new_key_name);
          return FALSE;
        }
    }

    free(new_key_name);
    return TRUE;
}
/* end windows 95 loader */

/***********************************************************************************/
/*                        windows NT registry loader                               */
/***********************************************************************************/

/* NT REGISTRY LOADER */

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((LPVOID)-1)
#endif

#define NT_REG_BLOCK_SIZE            0x1000

#define NT_REG_HEADER_BLOCK_ID       0x66676572	/* regf */
#define NT_REG_POOL_BLOCK_ID         0x6E696268	/* hbin */
#define NT_REG_KEY_BLOCK_ID          0x6b6e /* nk */
#define NT_REG_VALUE_BLOCK_ID        0x6b76 /* vk */

/* subblocks of nk */
#define NT_REG_HASH_BLOCK_ID         0x666c /* lf */
#define NT_REG_NOHASH_BLOCK_ID       0x696c /* li */
#define NT_REG_RI_BLOCK_ID	     0x6972 /* ri */

#define NT_REG_KEY_BLOCK_TYPE        0x20
#define NT_REG_ROOT_KEY_BLOCK_TYPE   0x2c

typedef struct {
    DWORD	id;		/* 0x66676572 'regf'*/
    DWORD	uk1;		/* 0x04 */
    DWORD	uk2;		/* 0x08 */
    FILETIME	DateModified;	/* 0x0c */
    DWORD	uk3;		/* 0x14 */
    DWORD	uk4;		/* 0x18 */
    DWORD	uk5;		/* 0x1c */
    DWORD	uk6;		/* 0x20 */
    DWORD	RootKeyBlock;	/* 0x24 */
    DWORD	BlockSize;	/* 0x28 */
    DWORD   uk7[116];
    DWORD	Checksum; /* at offset 0x1FC */
} nt_regf;

typedef struct {
    DWORD	blocksize;
    BYTE	data[1];
} nt_hbin_sub;

typedef struct {
    DWORD	id;		/* 0x6E696268 'hbin' */
    DWORD	off_prev;
    DWORD	off_next;
    DWORD	uk1;
    DWORD	uk2;		/* 0x10 */
    DWORD	uk3;		/* 0x14 */
    DWORD	uk4;		/* 0x18 */
    DWORD	size;		/* 0x1C */
    nt_hbin_sub	hbin_sub;	/* 0x20 */
} nt_hbin;

/*
 * the value_list consists of offsets to the values (vk)
 */
typedef struct {
    WORD	SubBlockId;		/* 0x00 0x6B6E */
    WORD	Type;			/* 0x02 for the root-key: 0x2C, otherwise 0x20*/
    FILETIME	writetime;	/* 0x04 */
    DWORD	uk1;			/* 0x0C */
    DWORD	parent_off;		/* 0x10 Offset of Owner/Parent key */
    DWORD	nr_subkeys;		/* 0x14 number of sub-Keys */
    DWORD	uk8;			/* 0x18 */
    DWORD	lf_off;			/* 0x1C Offset of the sub-key lf-Records */
    DWORD	uk2;			/* 0x20 */
    DWORD	nr_values;		/* 0x24 number of values */
    DWORD	valuelist_off;		/* 0x28 Offset of the Value-List */
    DWORD	off_sk;			/* 0x2c Offset of the sk-Record */
    DWORD	off_class;		/* 0x30 Offset of the Class-Name */
    DWORD	uk3;			/* 0x34 */
    DWORD	uk4;			/* 0x38 */
    DWORD	uk5;			/* 0x3c */
    DWORD	uk6;			/* 0x40 */
    DWORD	uk7;			/* 0x44 */
    WORD	name_len;		/* 0x48 name-length */
    WORD	class_len;		/* 0x4a class-name length */
    char	name[1];		/* 0x4c key-name */
} nt_nk;

typedef struct {
    DWORD	off_nk;	/* 0x00 */
    DWORD	name;	/* 0x04 */
} hash_rec;

typedef struct {
    WORD	id;		/* 0x00 0x666c */
    WORD	nr_keys;	/* 0x06 */
    hash_rec	hash_rec[1];
} nt_lf;

/*
 list of subkeys without hash

 li --+-->nk
      |
      +-->nk
 */
typedef struct {
    WORD	id;		/* 0x00 0x696c */
    WORD	nr_keys;
    DWORD	off_nk[1];
} nt_li;

/*
 this is a intermediate node

 ri --+-->li--+-->nk
      |       +
      |       +-->nk
      |
      +-->li--+-->nk
              +
	      +-->nk
 */
typedef struct {
    WORD	id;		/* 0x00 0x6972 */
    WORD	nr_li;		/* 0x02 number off offsets */
    DWORD	off_li[1];	/* 0x04 points to li */
} nt_ri;

typedef struct {
    WORD	id;		/* 0x00 'vk' */
    WORD	nam_len;
    DWORD	data_len;
    DWORD	data_off;
    DWORD	type;
    WORD	flag;
    WORD	uk1;
    char	name[1];
} nt_vk;

/*
 * gets a value
 *
 * vk->flag:
 *  0 value is a default value
 *  1 the value has a name
 *
 * vk->data_len
 *  len of the whole data block
 *  - reg_sz (unicode)
 *    bytes including the terminating \0 = 2*(number_of_chars+1)
 *  - reg_dword, reg_binary:
 *    if highest bit of data_len is set data_off contains the value
 */
static int _nt_dump_vk(LPSTR key_name, char *base, nt_vk *vk,FILE *f)
{
    BYTE *pdata = (BYTE *)(base+vk->data_off+4); /* start of data */
    struct key_value value;

    if (vk->id != NT_REG_VALUE_BLOCK_ID) {
        ERR("unknown block found (0x%04x), please report!\n", vk->id);
        return FALSE;
    }

    value.nameW = _strdupnAtoW(vk->name,vk->nam_len);
    value.type = vk->type;
    value.len = (vk->data_len & 0x7fffffff);
    value.data = (vk->data_len & 0x80000000) ? (LPBYTE)&(vk->data_off): pdata;

    _dump_value(&value,f);
    free(value.nameW);

    return TRUE;
}

/* it's called from _nt_dump_lf() */
static int _nt_dump_nk(LPSTR key_name,char *base,nt_nk *nk,FILE *f,int level);

/*
 * get the subkeys
 *
 * this structure contains the hash of a keyname and points to all
 * subkeys
 *
 * exception: if the id is 'il' there are no hash values and every
 * dword is a offset
 */
static int _nt_dump_lf(LPSTR key_name, char *base, int subkeys, nt_lf *lf, FILE *f, int level)
{
    int i;

    if (lf->id == NT_REG_HASH_BLOCK_ID) {
        if (subkeys != lf->nr_keys) goto error1;

        for (i=0; i<lf->nr_keys; i++)
            if (!_nt_dump_nk(key_name, base, (nt_nk*)(base+lf->hash_rec[i].off_nk+4), f, level)) goto error;
    } else if (lf->id == NT_REG_NOHASH_BLOCK_ID) {
        nt_li * li = (nt_li*)lf;
        if (subkeys != li->nr_keys) goto error1;

        for (i=0; i<li->nr_keys; i++)
            if (!_nt_dump_nk(key_name, base, (nt_nk*)(base+li->off_nk[i]+4), f, level)) goto error;
    } else if (lf->id == NT_REG_RI_BLOCK_ID) {  /* ri */
        nt_ri * ri = (nt_ri*)lf;
        int li_subkeys = 0;

        /* count all subkeys */
        for (i=0; i<ri->nr_li; i++) {
            nt_li * li = (nt_li*)(base+ri->off_li[i]+4);
            if(li->id != NT_REG_NOHASH_BLOCK_ID) goto error2;
            li_subkeys += li->nr_keys;
        }

        /* check number */
        if (subkeys != li_subkeys) goto error1;

        /* loop through the keys */
        for (i=0; i<ri->nr_li; i++) {
            nt_li *li = (nt_li*)(base+ri->off_li[i]+4);
            if (!_nt_dump_lf(key_name, base, li->nr_keys, (nt_lf*)li, f, level)) goto error;
        }
    } else goto error2;

    return TRUE;

error2:
    if (lf->id == 0x686c)
        FIXME("unknown Win XP node id 0x686c: do we need to add support for it ?\n");
    else
        ERR("unknown node id 0x%04x, please report!\n", lf->id);
    return TRUE;

error1:
    ERR("registry file corrupt! (inconsistent number of subkeys)\n");
    return FALSE;

error:
    ERR("error reading lf block\n");
    return FALSE;
}

/* _nt_dump_nk [Internal] */
static int _nt_dump_nk(LPSTR key_name,char *base,nt_nk *nk,FILE *f,int level)
{
    unsigned int n;
    DWORD *vl;
    LPSTR new_key_name = NULL;

    TRACE("%s\n", key_name);

    if (nk->SubBlockId != NT_REG_KEY_BLOCK_ID) {
        ERR("unknown node id 0x%04x, please report!\n", nk->SubBlockId);
        return FALSE;
    }

    if ((nk->Type!=NT_REG_ROOT_KEY_BLOCK_TYPE) && (((nt_nk*)(base+nk->parent_off+4))->SubBlockId != NT_REG_KEY_BLOCK_ID)) {
        ERR("registry file corrupt!\n");
        return FALSE;
    }

    /* create the new key */
    if (level <= 0) {
        /* create new subkey name */
        new_key_name = _strdupnA(key_name,strlen(key_name)+nk->name_len+1);
        if (strcmp(new_key_name,"") != 0) strcat(new_key_name,"\\");
        strncat(new_key_name,nk->name,nk->name_len);

        /* write the key path (something like [Software\\Microsoft\\..]) only if:
           1) key has some values
           2) key has no values and no subkeys
        */
        if (nk->nr_values > 0) {
            /* there are some values */
            fprintf(f,"\n[");
            _dump_strAtoW(new_key_name,strlen(new_key_name),f,"[]");
            fprintf(f,"]\n");
        }
        if ((nk->nr_subkeys == 0) && (nk->nr_values == 0)) {
            /* no subkeys and no values */
            fprintf(f,"\n[");
            _dump_strAtoW(new_key_name,strlen(new_key_name),f,"[]");
            fprintf(f,"]\n");
        }

        /* loop trough the value list */
        vl = (DWORD *)(base+nk->valuelist_off+4);
        for (n=0; n<nk->nr_values; n++) {
            nt_vk * vk = (nt_vk*)(base+vl[n]+4);
            if (!_nt_dump_vk(new_key_name, base, vk, f)) {
                free(new_key_name);
                return FALSE;
            }
        }
    } else new_key_name = _strdupnA(key_name,strlen(key_name));

    /* loop through the subkeys */
    if (nk->nr_subkeys) {
        nt_lf *lf = (nt_lf*)(base+nk->lf_off+4);
        if (!_nt_dump_lf(new_key_name, base, nk->nr_subkeys, lf, f, level-1)) {
            free(new_key_name);
            return FALSE;
        }
    }

    free(new_key_name);
    return TRUE;
}

/* end nt loader */

/**********************************************************************************
 * _set_registry_levels [Internal]
 *
 * set level to 0 for loading system files
 * set level to 1 for loading user files
 */
static void _set_registry_levels(int level,int saving,int period)
{
    SERVER_START_REQ( set_registry_levels )
    {
	req->current = level;
	req->saving  = saving;
        req->period  = period;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

/* _save_at_exit [Internal] */
static void _save_at_exit(HKEY hkey,LPCSTR path)
{
    LPCSTR confdir = get_config_dir();

    SERVER_START_REQ( save_registry_atexit )
    {
        req->hkey = hkey;
        wine_server_add_data( req, confdir, strlen(confdir) );
        wine_server_add_data( req, path, strlen(path)+1 );
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

/* configure save files and start the periodic saving timer [Internal] */
static void _init_registry_saving( HKEY hkey_users_default )
{
    int all;
    int period;

    all  = PROFILE_GetWineIniBool("registry","SaveOnlyUpdatedKeys",1);
    period = PROFILE_GetWineIniInt("registry","PeriodicSave",0);

    /* set saving level (0 for saving everything, 1 for saving only modified keys) */
    _set_registry_levels(1,!all,period*1000);

    if (PROFILE_GetWineIniBool("registry","WritetoHomeRegistryFiles",1))
    {
        _save_at_exit(HKEY_CURRENT_USER,"/" SAVE_LOCAL_REGBRANCH_CURRENT_USER );
        _save_at_exit(HKEY_LOCAL_MACHINE,"/" SAVE_LOCAL_REGBRANCH_LOCAL_MACHINE);
        _save_at_exit(hkey_users_default,"/" SAVE_LOCAL_REGBRANCH_USER_DEFAULT);
	/* HKEY_DYN_DATA is readonly for now */
    }

}

/******************************************************************************
 * _allocate_default_keys [Internal]
 * Registry initialisation, allocates some default keys.
 */
static void _allocate_default_keys(void) {
	HKEY	hkey;
	char	buf[200];

	TRACE("(void)\n");

	RegCreateKeyA(HKEY_DYN_DATA,"PerfStats\\StatData",&hkey);
	RegCloseKey(hkey);

        /* This was an Open, but since it is called before the real registries
           are loaded, it was changed to a Create - MTB 980507*/
	RegCreateKeyA(HKEY_LOCAL_MACHINE,"HARDWARE\\DESCRIPTION\\System",&hkey);
	RegSetValueExA(hkey,"Identifier",0,REG_SZ,(LPBYTE)"SystemType WINE",
                  strlen("SystemType WINE"));
	RegCloseKey(hkey);

	/* \\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion
	 *						CurrentVersion
	 *						CurrentBuildNumber
	 *						CurrentType
	 *					string	RegisteredOwner
	 *					string	RegisteredOrganization
	 *
	 */
	/* System\\CurrentControlSet\\Services\\SNMP\\Parameters\\RFC1156Agent
	 * 					string	SysContact
	 * 					string	SysLocation
	 * 						SysServices
	 */
	if (-1!=gethostname(buf,200)) {
		RegCreateKeyA(HKEY_LOCAL_MACHINE,"System\\CurrentControlSet\\Control\\ComputerName\\ComputerName",&hkey);
		RegSetValueExA(hkey,"ComputerName",0,REG_SZ,(LPBYTE)buf,strlen(buf)+1);
		RegCloseKey(hkey);
	}

        RegCreateKeyA(HKEY_USERS,".Default",&hkey);
        RegCloseKey(hkey);
        DRIVE_InitializeRegistryEntries ();
}

#define REG_DONTLOAD -1
#define REG_WIN31     0
#define REG_WIN95     1
#define REG_WINNT     2

/* return the type of native registry [Internal] */
static int _get_reg_type(void)
{
    char windir[MAX_PATHNAME_LEN];
    char tmp[MAX_PATHNAME_LEN];
    int ret = REG_WIN31;

    GetWindowsDirectoryA(windir,MAX_PATHNAME_LEN);

    /* test %windir%/system32/config/system --> winnt */
    strcpy(tmp, windir);
    strncat(tmp, "\\system32\\config\\system", MAX_PATHNAME_LEN - strlen(tmp) - 1);
    if(GetFileAttributesA(tmp) != (DWORD)-1) {
      ret = REG_WINNT;
    }
    else
    {
       /* test %windir%/system.dat --> win95 */
      strcpy(tmp, windir);
      strncat(tmp, "\\system.dat", MAX_PATHNAME_LEN - strlen(tmp) - 1);
      if(GetFileAttributesA(tmp) != (DWORD)-1) {
        ret = REG_WIN95;
      }
    }

    if ((ret == REG_WINNT) && (!PROFILE_GetWineIniString( "Wine", "Profile", "", tmp, MAX_PATHNAME_LEN))) {
       MESSAGE("When you are running with a native NT directory specify\n");
       MESSAGE("'Profile=<profiledirectory>' or disable loading of Windows\n");
       MESSAGE("registry (LoadWindowsRegistryFiles=N)\n");
       ret = REG_DONTLOAD;
    }

    return ret;
}

#define WINE_REG_VER_ERROR  -1
#define WINE_REG_VER_1       0
#define WINE_REG_VER_2       1
#define WINE_REG_VER_OLD     2
#define WINE_REG_VER_UNKNOWN 3

/* return the version of wine registry file [Internal] */
static int _get_wine_registry_file_format_version(LPCSTR fn)
{
    FILE *f;
    char tmp[50];
    int ver;

    if ((f=fopen(fn,"rt")) == NULL) {
        WARN("Couldn't open %s for reading: %s\n",fn,strerror(errno));
        return WINE_REG_VER_ERROR;
    }

    if (fgets(tmp,50,f) == NULL) {
        WARN("Error reading %s: %s\n",fn,strerror(errno));
        fclose(f);
        return WINE_REG_VER_ERROR;
    }
    fclose(f);

    if (sscanf(tmp,"WINE REGISTRY Version %d",&ver) != 1) return WINE_REG_VER_UNKNOWN;
    switch (ver) {
        case 1:
            return WINE_REG_VER_1;
            break;
        case 2:
            return WINE_REG_VER_2;
            break;
        default:
            return WINE_REG_VER_UNKNOWN;
    }
}

/* load the registry file in wine format [Internal] */
static void load_wine_registry(HKEY hkey,LPCSTR fn)
{
    int file_format;

    file_format = _get_wine_registry_file_format_version(fn);
    switch (file_format) {

        case WINE_REG_VER_1:
            WARN("Unable to load registry file %s: old format which is no longer supported.\n",fn);
            break;

        case WINE_REG_VER_2: {
            HANDLE file;
            if ((file = FILE_CreateFile( fn, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                              FILE_ATTRIBUTE_NORMAL, 0, DRIVE_UNKNOWN, 0 )))
            {
                SERVER_START_REQ( load_registry )
                {
                    req->hkey    = hkey;
                    req->file    = file;
                    wine_server_call( req );
                }
                SERVER_END_REQ;
                CloseHandle( file );
            }
            break;
        }

        case WINE_REG_VER_UNKNOWN:
            WARN("Unable to load registry file %s: unknown format.\n",fn);
            break;

        case WINE_REG_VER_ERROR:
            break;
    }
}

/* generate and return the name of the tmp file and associated stream [Internal] */
static LPSTR _get_tmp_fn(FILE **f)
{
    LPSTR ret;
    int tmp_fd,count;

    ret = _xmalloc(50);
    for (count = 0;;) {
        sprintf(ret,"/tmp/reg%lx%04x.tmp",(long)wine_gettid_or_pid(),count++);
        if ((tmp_fd = open(ret,O_CREAT | O_EXCL | O_WRONLY,0666)) != -1) break;
        if (errno != EEXIST) {
            ERR("Unexpected error while open() call: %s\n",strerror(errno));
            free(ret);
            *f = NULL;
            return NULL;
        }
    }

    if ((*f = fdopen(tmp_fd,"w")) == NULL) {
        ERR("Unexpected error while fdopen() call: %s\n",strerror(errno));
        close(tmp_fd);
        free(ret);
        return NULL;
    }

    return ret;
}

/* convert win95 native registry file to wine format [Internal] */
static LPSTR _convert_win95_registry_to_wine_format(LPCSTR fn,int level)
{
    int fd;
    FILE *f;
    DOS_FULL_NAME full_name;
    void *base;
    LPSTR ret = NULL;
    struct stat st;

    _w95creg *creg;
    _w95rgkn *rgkn;
    _w95dke *dke, *root_dke;

    if (!DOSFS_GetFullName( fn, 0, &full_name )) return NULL;

    /* map the registry into the memory */
    if ((fd = open(full_name.long_name, O_RDONLY | O_NONBLOCK)) == -1) return NULL;
    if ((fstat(fd, &st) == -1)) goto error1;
    if (!st.st_size) goto error1;
    if ((base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) goto error1;

    /* control signature */
    if (*(LPDWORD)base != W95_REG_CREG_ID) {
        ERR("unable to load native win95 registry file %s: unknown signature.\n",fn);
        goto error;
    }

    creg = base;
    /* load the header (rgkn) */
    rgkn = (_w95rgkn*)(creg + 1);
    if (rgkn->id != W95_REG_RGKN_ID) {
        ERR("second IFF header not RGKN, but %lx\n", rgkn->id);
        goto error;
    }
    if (rgkn->root_off != 0x20) {
        ERR("rgkn->root_off not 0x20, please report !\n");
        goto error;
    }
    if (rgkn->last_dke > rgkn->size)
    {
      ERR("registry file corrupt! last_dke > size!\n");
      goto error;
    }
    /* verify last dke */
    dke = (_w95dke*)((char*)rgkn + rgkn->last_dke);
    if (dke->x1 != 0x80000000)
    { /* wrong magic */
      ERR("last dke invalid !\n");
      goto error;
    }
    if (rgkn->size > creg->rgdb_off)
    {
      ERR("registry file corrupt! rgkn size > rgdb_off !\n");
      goto error;
    }
    root_dke = (_w95dke*)((char*)rgkn + rgkn->root_off);
    if ( (root_dke->prevlvl != 0xffffffff) || (root_dke->next != 0xffffffff) )
    {
        ERR("registry file corrupt! invalid root dke !\n");
        goto error;
    }

    if ( (ret = _get_tmp_fn(&f)) == NULL) goto error;
    fprintf(f,"WINE REGISTRY Version 2");
    _w95_dump_dke("",creg,rgkn,root_dke,f,level);
    fclose(f);

error:
    if(ret == NULL) {
        ERR("Unable to load native win95 registry file %s.\n",fn);
        ERR("Please report this.\n");
        ERR("Make a backup of the file, run a good reg cleaner program and try again!\n");
    }

    munmap(base, st.st_size);
error1:
    close(fd);
    return ret;
}

/* convert winnt native registry file to wine format [Internal] */
static LPSTR _convert_winnt_registry_to_wine_format(LPCSTR fn,int level)
{
    FILE *f;
    void *base;
    LPSTR ret = NULL;
    HANDLE hFile;
    HANDLE hMapping;

    nt_regf *regf;
    nt_hbin *hbin;
    nt_hbin_sub *hbin_sub;
    nt_nk *nk;

    TRACE("%s\n", fn);

    hFile = CreateFileA( fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0 );
    if ( hFile == INVALID_HANDLE_VALUE ) return NULL;
    hMapping = CreateFileMappingA( hFile, NULL, PAGE_READONLY|SEC_COMMIT, 0, 0, NULL );
    if (!hMapping) goto error1;
    base = MapViewOfFile( hMapping, FILE_MAP_READ, 0, 0, 0 );
    CloseHandle( hMapping );
    if (!base) goto error1;

    /* control signature */
    if (*(LPDWORD)base != NT_REG_HEADER_BLOCK_ID) {
        ERR("unable to load native winnt registry file %s: unknown signature.\n",fn);
        goto error;
    }

    /* start block */
    regf = base;

    /* hbin block */
    hbin = (nt_hbin*)((char*) base + 0x1000);
    if (hbin->id != NT_REG_POOL_BLOCK_ID) {
      ERR( "hbin block invalid\n");
      goto error;
    }

    /* hbin_sub block */
    hbin_sub = (nt_hbin_sub*)&(hbin->hbin_sub);
    if ((hbin_sub->data[0] != 'n') || (hbin_sub->data[1] != 'k')) {
      ERR( "hbin_sub block invalid\n");
      goto error;
    }

    /* nk block */
    nk = (nt_nk*)&(hbin_sub->data[0]);
    if (nk->Type != NT_REG_ROOT_KEY_BLOCK_TYPE) {
      ERR( "special nk block not found\n");
      goto error;
    }

    if ( (ret = _get_tmp_fn(&f)) == NULL) goto error;
    fprintf(f,"WINE REGISTRY Version 2");
    _nt_dump_nk("",(char*)base+0x1000,nk,f,level);
    fclose(f);

error:
    UnmapViewOfFile( base );
error1:
    CloseHandle(hFile);
    return ret;
}

/* convert native registry to wine format and load it via server call [Internal] */
static void _convert_and_load_native_registry(LPCSTR fn,HKEY hkey,int reg_type,int level)
{
    LPSTR tmp = NULL;

    switch (reg_type) {
        case REG_WINNT:
            /* FIXME: following function doesn't really convert yet */
            tmp = _convert_winnt_registry_to_wine_format(fn,level);
            break;
        case REG_WIN95:
            tmp = _convert_win95_registry_to_wine_format(fn,level);
            break;
        case REG_WIN31:
            ERR("Don't know how to convert native 3.1 registry yet.\n");
            break;
        default:
            ERR("Unknown registry format parameter (%d)\n",reg_type);
            break;
    }

    if (tmp != NULL) {
        load_wine_registry(hkey,tmp);
        TRACE("File %s successfully converted to %s and loaded to registry.\n",fn,tmp);
        unlink(tmp);
    }
    else WARN("Unable to convert %s (doesn't exist?)\n",fn);
    free(tmp);
}

/* load all native windows registry files [Internal] */
static void _load_windows_registry( HKEY hkey_users_default )
{
    int reg_type;
    char windir[MAX_PATHNAME_LEN];
    char path[MAX_PATHNAME_LEN];

    GetWindowsDirectoryA(windir,MAX_PATHNAME_LEN);

    reg_type = _get_reg_type();
    switch (reg_type) {
        case REG_WINNT: {
            HKEY hkey;

            /* user specific ntuser.dat */
            if (PROFILE_GetWineIniString( "Wine", "Profile", "", path, MAX_PATHNAME_LEN)) {
                strcat(path,"\\ntuser.dat");
                _convert_and_load_native_registry(path,HKEY_CURRENT_USER,REG_WINNT,1);
            }

            /* default user.dat */
            if (hkey_users_default) {
                strcpy(path,windir);
                strcat(path,"\\system32\\config\\default");
                _convert_and_load_native_registry(path,hkey_users_default,REG_WINNT,1);
            }

            /*
            * FIXME
            *  map HLM\System\ControlSet001 to HLM\System\CurrentControlSet
            */

            if (!RegCreateKeyA(HKEY_LOCAL_MACHINE, "SYSTEM", &hkey)) {
	      strcpy(path,windir);
	      strcat(path,"\\system32\\config\\system");
              _convert_and_load_native_registry(path,hkey,REG_WINNT,1);
              RegCloseKey(hkey);
            }

            if (!RegCreateKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE", &hkey)) {
                strcpy(path,windir);
                strcat(path,"\\system32\\config\\software");
                _convert_and_load_native_registry(path,hkey,REG_WINNT,1);
                RegCloseKey(hkey);
            }

            strcpy(path,windir);
            strcat(path,"\\system32\\config\\sam");
            _convert_and_load_native_registry(path,HKEY_LOCAL_MACHINE,REG_WINNT,0);

            strcpy(path,windir);
            strcat(path,"\\system32\\config\\security");
            _convert_and_load_native_registry(path,HKEY_LOCAL_MACHINE,REG_WINNT,0);

            /* this key is generated when the nt-core booted successfully */
            if (!RegCreateKeyA(HKEY_LOCAL_MACHINE,"System\\Clone",&hkey)) RegCloseKey(hkey);
            break;
        }

        case REG_WIN95:
            _convert_and_load_native_registry("c:\\system.1st",HKEY_LOCAL_MACHINE,REG_WIN95,0);

            strcpy(path,windir);
            strcat(path,"\\system.dat");
            _convert_and_load_native_registry(path,HKEY_LOCAL_MACHINE,REG_WIN95,0);

            if (PROFILE_GetWineIniString("Wine","Profile","",path,MAX_PATHNAME_LEN)) {
	        /* user specific user.dat */
	        strncat(path, "\\user.dat", MAX_PATHNAME_LEN - strlen(path) - 1);
                _convert_and_load_native_registry(path,HKEY_CURRENT_USER,REG_WIN95,1);

	        /* default user.dat */
	        if (hkey_users_default) {
                    strcpy(path,windir);
                    strcat(path,"\\user.dat");
                    _convert_and_load_native_registry(path,hkey_users_default,REG_WIN95,1);
                }
            } else {
                strcpy(path,windir);
                strcat(path,"\\user.dat");
                _convert_and_load_native_registry(path,HKEY_CURRENT_USER,REG_WIN95,1);
            }
            break;

        case REG_WIN31:
            /* FIXME: here we should convert to *.reg file supported by server and call REQ_LOAD_REGISTRY, see REG_WIN95 case */
            _w31_loadreg();
            break;

        case REG_DONTLOAD:
            TRACE("REG_DONTLOAD\n");
            break;

        default:
            ERR("switch: no match (%d)\n",reg_type);
            break;

    }
}

/* load global registry files (stored in /etc/wine) [Internal] */
static void _load_global_registry(void)
{
    char    HivePath[MAX_PATH];
    char    HiveUser[MAX_PATH];
    char    HiveSystem[MAX_PATH];

    TRACE("(void)\n");

    if (PROFILE_GetWineIniString( "Registry", "GlobalRegistryFilesPath", "", HivePath, sizeof(HivePath) ) && HivePath[0])
    {
        TRACE("GlobalRegistryFilesPath found...\n");

        if (HivePath[strlen(HivePath)-1]=='/')
            HivePath[strlen(HivePath)-1]=0;

        snprintf(HiveUser,   sizeof(HiveUser),   "%s/%s", HivePath, SAVE_LOCAL_REGBRANCH_CURRENT_USER);
        snprintf(HiveSystem, sizeof(HiveSystem), "%s/%s", HivePath, SAVE_LOCAL_REGBRANCH_LOCAL_MACHINE);

        TRACE("loading %s\n", HiveUser);
        load_wine_registry( HKEY_USERS,         HiveUser );
        TRACE("loading %s\n", HiveSystem);
        load_wine_registry( HKEY_LOCAL_MACHINE, HiveSystem );
    }
    else
    {
        TRACE("GlobalRegistryFilesPath not found, using old style global registry\n");

        /* Load the global HKU hive directly from sysconfdir */
        load_wine_registry( HKEY_USERS, SAVE_GLOBAL_REGBRANCH_USER_DEFAULT );

        /* Load the global machine defaults directly from sysconfdir */
        load_wine_registry( HKEY_LOCAL_MACHINE, SAVE_GLOBAL_REGBRANCH_LOCAL_MACHINE );
    }
}

/* load home registry files (stored in ~/.wine) [Internal] */
static void _load_home_registry( HKEY hkey_users_default )
{
    LPCSTR confdir = get_config_dir();
    LPSTR tmp = _xmalloc(strlen(confdir)+20);

    strcpy(tmp,confdir);
    strcat(tmp,"/" SAVE_LOCAL_REGBRANCH_USER_DEFAULT);
    load_wine_registry(hkey_users_default,tmp);

    strcpy(tmp,confdir);
    strcat(tmp,"/" SAVE_LOCAL_REGBRANCH_CURRENT_USER);
    load_wine_registry(HKEY_CURRENT_USER,tmp);

    strcpy(tmp,confdir);
    strcat(tmp,"/" SAVE_LOCAL_REGBRANCH_LOCAL_MACHINE);
    load_wine_registry(HKEY_LOCAL_MACHINE,tmp);

    strcpy(tmp,confdir);
    strcat(tmp,"/" SAVE_LOCAL_REGBRANCH_DYN_DATA);
    load_wine_registry(HKEY_DYN_DATA,tmp);

    free(tmp);
}

/* load all registry (native and global and home) */
void SHELL_LoadRegistry( void )
{
    HKEY hkey_users_default;

    TRACE("(void)\n");

    if (!CLIENT_IsBootThread()) return;  /* already loaded */

    if (RegCreateKeyA(HKEY_USERS,".Default",&hkey_users_default))
    {
        ERR("Cannot create HKEY_USERS/.Default\n" );
        ExitProcess(1);
    }

    _allocate_default_keys();
    _set_registry_levels(0,0,0);
    if (PROFILE_GetWineIniBool("Registry","LoadWindowsRegistryFiles",1))
        _load_windows_registry( hkey_users_default );
    if (PROFILE_GetWineIniBool("Registry","LoadGlobalRegistryFiles",1))
        _load_global_registry();
    _set_registry_levels(1,0,0);
    if (PROFILE_GetWineIniBool("Registry","LoadHomeRegistryFiles",1))
        _load_home_registry( hkey_users_default );
    _init_registry_saving( hkey_users_default );
    RegCloseKey(hkey_users_default);
}

/***************************************************************************/
/*                          API FUNCTIONS                                  */
/***************************************************************************/

/* 16-bit functions */

/* 0 and 1 are valid rootkeys in win16 shell.dll and are used by
 * some programs. Do not remove those cases. -MM
 */
static inline void fix_win16_hkey( HKEY *hkey )
{
    if (*hkey == 0 || *hkey == 1) *hkey = HKEY_CLASSES_ROOT;
}

/******************************************************************************
 *           RegEnumKey   [KERNEL.216]
 *           RegEnumKey   [SHELL.7]
 */
DWORD WINAPI RegEnumKey16( HKEY hkey, DWORD index, LPSTR name, DWORD name_len )
{
    fix_win16_hkey( &hkey );
    return RegEnumKeyA( hkey, index, name, name_len );
}

/******************************************************************************
 *           RegOpenKey   [KERNEL.217]
 *           RegOpenKey   [SHELL.1]
 */
DWORD WINAPI RegOpenKey16( HKEY hkey, LPCSTR name, LPHKEY retkey )
{
    fix_win16_hkey( &hkey );
    return RegOpenKeyA( hkey, name, retkey );
}

/******************************************************************************
 *           RegCreateKey   [KERNEL.218]
 *           RegCreateKey   [SHELL.2]
 */
DWORD WINAPI RegCreateKey16( HKEY hkey, LPCSTR name, LPHKEY retkey )
{
    fix_win16_hkey( &hkey );
    return RegCreateKeyA( hkey, name, retkey );
}

/******************************************************************************
 *           RegDeleteKey   [KERNEL.219]
 *           RegDeleteKey   [SHELL.4]
 */
DWORD WINAPI RegDeleteKey16( HKEY hkey, LPCSTR name )
{
    fix_win16_hkey( &hkey );
    return RegDeleteKeyA( hkey, name );
}

/******************************************************************************
 *           RegCloseKey   [KERNEL.220]
 *           RegCloseKey   [SHELL.3]
 */
DWORD WINAPI RegCloseKey16( HKEY hkey )
{
    fix_win16_hkey( &hkey );
    return RegCloseKey( hkey );
}

/******************************************************************************
 *           RegSetValue   [KERNEL.221]
 *           RegSetValue   [SHELL.5]
 */
DWORD WINAPI RegSetValue16( HKEY hkey, LPCSTR name, DWORD type, LPCSTR data, DWORD count )
{
    fix_win16_hkey( &hkey );
    return RegSetValueA( hkey, name, type, data, count );
}

/******************************************************************************
 *           RegDeleteValue  [KERNEL.222]
 */
DWORD WINAPI RegDeleteValue16( HKEY hkey, LPSTR name )
{
    fix_win16_hkey( &hkey );
    return RegDeleteValueA( hkey, name );
}

/******************************************************************************
 *           RegEnumValue   [KERNEL.223]
 */
DWORD WINAPI RegEnumValue16( HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
                             LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count )
{
    fix_win16_hkey( &hkey );
    return RegEnumValueA( hkey, index, value, val_count, reserved, type, data, count );
}

/******************************************************************************
 *           RegQueryValue   [KERNEL.224]
 *           RegQueryValue   [SHELL.6]
 *
 * NOTES
 *    Is this HACK still applicable?
 *
 * HACK
 *    The 16bit RegQueryValue doesn't handle selectorblocks anyway, so we just
 *    mask out the high 16 bit.  This (not so much incidently) hopefully fixes
 *    Aldus FH4)
 */
DWORD WINAPI RegQueryValue16( HKEY hkey, LPCSTR name, LPSTR data, LPDWORD count )
{
    fix_win16_hkey( &hkey );
    if (count) *count &= 0xffff;
    return RegQueryValueA( hkey, name, data, (LPLONG)count );
}

/******************************************************************************
 *           RegQueryValueEx   [KERNEL.225]
 */
DWORD WINAPI RegQueryValueEx16( HKEY hkey, LPCSTR name, LPDWORD reserved, LPDWORD type,
                                LPBYTE data, LPDWORD count )
{
    fix_win16_hkey( &hkey );
    return RegQueryValueExA( hkey, name, reserved, type, data, count );
}

/******************************************************************************
 *           RegSetValueEx   [KERNEL.226]
 */
DWORD WINAPI RegSetValueEx16( HKEY hkey, LPCSTR name, DWORD reserved, DWORD type,
                              CONST BYTE *data, DWORD count )
{
    fix_win16_hkey( &hkey );
    if (!count && (type==REG_SZ)) count = strlen((char *)data);
    return RegSetValueExA( hkey, name, reserved, type, data, count );
}

/******************************************************************************
 *           RegFlushKey   [KERNEL.227]
 */
DWORD WINAPI RegFlushKey16( HKEY hkey )
{
    fix_win16_hkey( &hkey );
    return RtlNtStatusToDosError (NtFlushKey (hkey));
}
