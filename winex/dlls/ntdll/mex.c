/*
 * MEX - Miniature Embeddable XML Parser
 *
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "mex.h"

#ifdef MEX_EMBEDDED
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* MEX_EMBEDDED */
#if 0 /* fool auto-indentation */
}
#endif

#if defined(MEX_NTDLL)
#define MEX_MALLOC(x) RtlAllocateHeap(GetProcessHeap(),0,(x))
#define MEX_REALLOC(ptr,x) RtlReAllocateHeap(GetProcessHeap(),0,(ptr),(x))
#define MEX_FREE(x) RtlFreeHeap(GetProcessHeap(),0,(x))
#elif defined(MEX_WIN32)
#define MEX_MALLOC(x) HeapAlloc(GetProcessHeap(),0,(x))
#define MEX_REALLOC(ptr,x) HeapReAlloc(GetProcessHeap(),0,(ptr),(x))
#define MEX_FREE(x) HeapFree(GetProcessHeap(),0,(x))
#else
#define MEX_MALLOC(x) malloc(x)
#define MEX_REALLOC(ptr,x) realloc(ptr,x)
#define MEX_FREE(x) free(x)
#endif /* MEX_WIN32 */

#define MEX_FREEIF(x) do { if (x) MEX_FREE(x); } while (0)
#define MEX_STREQ(x,y) (((x) == NULL && (y) == NULL) || strcmp((x),(y)) == 0)

/* In case your compiler is old enough to not support "inline" ... */
#define MEX_INLINE inline

static int MEX_error = 0;

MEX_API int MEXGetError(void)
{
    int code = MEX_error;
    MEX_error = 0;
    return code;
}

static void MEXSetError(int code)
{
    MEX_error = code;
}

static MEX_INLINE void *MEXMallocZero(int sz)
{
    void *v = MEX_MALLOC(sz);
    if (v)
        memset(v,0,sz);
    return v;
}
#define MEX_MALLOCOBJ(x) MEXMallocZero(sizeof(x))

static MEX_INLINE void *MEXStrdup(char *str, int sz)
{
    char *sn;
    if (sz < 0)
        sz = (str ? strlen(str) : 0);
    sn = MEX_MALLOC(sz+1);
    if (!sn)
        return NULL;
    memcpy(sn,str,sz);
    sn[sz] = '\0';
    return sn;
}

MEX_API void MEXStringBufStart(MEXStringBuf *sb)
{
    sb->buf = NULL;
    sb->use = 0;
    sb->spc = 0;
}

MEX_API void MEXStringBufEnd(MEXStringBuf *sb)
{
    MEX_FREEIF(sb->buf);
}

MEX_API char *MEXStringBufDup(MEXStringBuf *sb)
{
    return MEXStrdup(sb->buf,sb->use);
}

MEX_API void MEXStringBufClear(MEXStringBuf *sb)
{
    sb->use = 0;
    if (sb->spc > 0)
        sb->buf[0] = '\0';
}

/** @brief Grow a string buffer to accomodate the given number of bytes

    To be clear, this function (if it succeeds) guarantees that
    nch characters can be safely appended without further memory
    allocation.

    This may fail if malloc/realloc fails.
 */
MEX_API int MEXStringBufGrow(MEXStringBuf *sb, int nch)
{
    int needed = sb->use + nch + 1;
    if (!sb->buf)
    {
        /* initial size */
        sb->spc = 16;
        sb->buf = MEX_MALLOC(sb->spc);
        if (!sb->buf)
            return -1;
    }
    if (needed > sb->spc)
    {
        /* follow "doubling" rule-of-thumb here to minimize allocations */
        char *nbuf;
        do
            sb->spc *= 2;
        while (needed > sb->spc);
        nbuf = MEX_REALLOC(sb->buf,sb->spc);
        if (!nbuf) /* sb->buf is still valid... */
            return -1;
        /* realloc() succeeded, so sb->buf is no longer valid */
        sb->buf = nbuf;
    }
    return 0;
}

MEX_API int MEXStringBufAddChar(MEXStringBuf *sb, int ch)
{
    if (sb->use+2 > sb->spc) 
    {
        if (MEXStringBufGrow(sb,1))
            return -1;
    }
    sb->buf[sb->use++] = ch;
    sb->buf[sb->use] = '\0';
    return 0;
}

MEX_API int MEXStringBufAddString(MEXStringBuf *sb, char *str, int len)
{
    if (len < 0)
        len = (str ? strlen(str) : 0);
    if (MEXStringBufGrow(sb,len))
        return -1;
    memcpy(sb->buf+sb->use,str,len);
    sb->use += len;
    sb->buf[sb->use] = '\0';
    return 0;
}

MEX_API void MEXNodeFree(MEXNode *node) 
{
    if (node) {
        switch (node->type) {
        case MEX_TAG:
            MEX_FREEIF(node->data.tag.name);
            {
                MEXAttr *a, *an;
                a = node->data.tag.attrs;
                while (a) 
                {
                    an = a->next;
                    MEX_FREEIF(a->name);
                    MEX_FREEIF(a->value);
                    MEX_FREE(a);
                    a = an;
                }
            }
            {
                MEXNode *n, *nn;
                n = node->data.tag.childFirst;
                while (n)
                {
                    nn = n->next;
                    MEXNodeFree(n);
                    n = nn;
                }
            }
            break;

        case MEX_CDATA:
            MEX_FREEIF(node->data.cdata.buf);
            break;
        }
        MEX_FREE(node);
    }
}

MEX_API MEXNode *MEXNodeTag(char *name)
{
    MEXNode *node = MEX_MALLOCOBJ(MEXNode);
    if (!node)
    {
        MEXSetError(MEX_ERR_MALLOC);
        goto fail;
    }
    node->type = MEX_TAG;
    if (!(node->data.tag.name = MEXStrdup(name,-1)))
    {
        MEXSetError(MEX_ERR_MALLOC);
        goto fail;
    }
    return node;
    
fail:
    MEXNodeFree(node);
    return NULL;
}

MEX_API MEXNode *MEXNodeCdata(char *buf, int len)
{
    MEXNode *node = MEX_MALLOCOBJ(MEXNode);
    if (!node)
    {
        MEXSetError(MEX_ERR_MALLOC);
        goto fail;
    }
    node->type = MEX_CDATA;
    if (!(node->data.cdata.buf = MEXStrdup(buf,len)))
    {
        MEXSetError(MEX_ERR_MALLOC);
        goto fail;
    }
    node->data.cdata.len = (len < 0 ? strlen(node->data.cdata.buf) : len);
    return node;

fail:
    MEXNodeFree(node);
    return NULL;
}

MEX_API int MEXNodeSetAttr(MEXNode *node, char *nsid,
                           char *name, char *value)
{
    /* replace existing attribute if there is one */
    MEXAttr *a;
    
    if (!node || node->type != MEX_TAG || !name || !value)
    {
        MEXSetError(MEX_ERR_INVALID);
        return -1;
    }

    for (a = node->data.tag.attrs; a; a = a->next)
    {
        if (MEX_STREQ(a->name,name) &&
            MEX_STREQ(a->nsid,nsid))
        {
            /* replace existing */
            char *nvalue = MEXStrdup(value,-1);
            if (!nvalue)
            {
                MEXSetError(MEX_ERR_MALLOC);
                return -1;
            }
            MEX_FREEIF(a->value);
            a->value = nvalue;
            return 0;
        }
    }

    /* create new attribute entry */
    a = MEX_MALLOCOBJ(MEXAttr);
    if (!a)
    {
        MEXSetError(MEX_ERR_MALLOC);
        return -1;
    }
    if (!(a->name = MEXStrdup(name,-1)))
    {
        MEX_FREE(a);
        MEXSetError(MEX_ERR_MALLOC);
        return -1;
    }
    if (!(a->value = MEXStrdup(value,-1)))
    {
        MEX_FREE(a->name);
        MEX_FREE(a);
        MEXSetError(MEX_ERR_MALLOC);
        return -1;
    }
    if (nsid && !(a->nsid = MEXStrdup(nsid,-1)))
    {
        MEX_FREE(a->value);
        MEX_FREE(a->name);
        MEX_FREE(a);
        MEXSetError(MEX_ERR_MALLOC);
        return -1;
    }
    a->next = node->data.tag.attrs;
    node->data.tag.attrs = a;

    return 0;
}

MEX_API char *MEXNodeGetAttr(MEXNode *node, 
                             char *nsid,
                             char *name)
{
    MEXAttr *a;

    if (!node || node->type != MEX_TAG || !name)
    {
        MEXSetError(MEX_ERR_INVALID);
        return NULL;
    }

    for (a = node->data.tag.attrs; a; a = a->next)
    {
        if (MEX_STREQ(a->name,name) &&
            (!nsid || MEX_STREQ(a->name,name)))
            return a->value;
    }
    
    MEXSetError(MEX_ERR_NOEXIST);
    return NULL;
}

MEX_API int MEXNodeAddChild(MEXNode *parent, MEXNode *child)
{
    if (!parent || parent->type != MEX_TAG || !child)
    {
        MEXSetError(MEX_ERR_INVALID);
        return -1;
    }

    child->next = NULL;
    if (parent->data.tag.childLast)
        parent->data.tag.childLast->next = child;
    else
        parent->data.tag.childFirst = child;
    parent->data.tag.childLast = child;
    return 0;
}

/** @brief Retrieve the CDATA content of a node 

    Often, a tag node will have only CDATA as its content
    (especially for configuration-style XML documents).
 */
MEX_API MEXNode *MEXNodeContent(MEXNode *parent)
{
    if (!parent || parent->type != MEX_TAG)
    {
        MEXSetError(MEX_ERR_INVALID);
        return NULL;
    }
    /* To do this, we must have a single CDATA child */
    /* Special case: return a different code if we have no children */
    if (!parent->data.tag.childFirst)
    {
        MEXSetError(MEX_ERR_NOEXIST);
        return NULL;
    }
    if (parent->data.tag.childFirst->type != MEX_CDATA ||
        parent->data.tag.childFirst->next)
    {
        MEXSetError(MEX_ERR_INVALID);
        return NULL;
    }
    return parent->data.tag.childFirst;
}

MEX_API void MEXDocumentFree(MEXDocument *doc)
{
    if (doc)
    {
        MEX_FREEIF(doc->version);
        MEX_FREEIF(doc->encoding);
        if (doc->root)
            MEXNodeFree(doc->root);
        MEX_FREE(doc);
    }
}

/** @brief A source for parsing from */
typedef struct MEX_source
{
    /* keep data as unsigned char so that 8-bit UTF-8
       characters don't pop out as signed */
    unsigned char *buf;
    int pos, len;
    int seennl, charno, lineno;
    int flags; /* parsing flags */
} MEXSource;

static void MEXSourceStart(MEXSource *src, char *buf, int len, int flags)
{
    src->buf = (unsigned char *)buf;
    src->pos = 0;
    if (len < 0)
        src->len = (buf ? strlen(buf) : 0);
    else
        src->len = len;
    src->seennl = 1; /* pretend we saw a newline last time */
    src->charno = 0;
    src->lineno = 1;
    src->flags = flags;
}

static void MEXSourceEnd(MEXSource *src)
{
    /* place-holder in case we need to deallocate later... */
}

/** @brief Peek at next char */
static MEX_INLINE int MEXPeekChar(MEXSource *src)
{
    if (src->pos >= src->len)
        return -1;
    return src->buf[src->pos];
}

/** @brief Check to see if we start with a particular string

    If the prefix is there, it will be skipped.

    Result is 0 if prefix exists and is consumed, -1 otherwise.
*/
static MEX_INLINE int MEXSkipPrefix(MEXSource *src, char *str)
{
    int len = strlen(str);
    if ((src->len - src->pos) < len)
        return -1;
    if (strncmp((char *)src->buf+src->pos,str,len) == 0)
    {
        src->pos += len;
        return 0;
    }
    else
        return -1;
}

/** @brief Like MEXSkipPrefix but does not consume chars */
static MEX_INLINE int MEXHasPrefix(MEXSource *src, char *str)
{
    int len = strlen(str);
    if ((src->len - src->pos) < len)
        return -1;
    if (strncmp((char *)src->buf+src->pos,str,len) == 0)
        return 0;
    else
        return -1;
}

/** @brief Return next char from parse source and update position info */
static MEX_INLINE int MEXGetChar(MEXSource *src)
{
    int ch;
    if (src->pos >= src->len)
        return -1;
    ch = src->buf[src->pos++];

    if (ch == '\n')
    {
        src->lineno++;
        src->charno = 0;
        src->seennl = 1;
    }
    else
    {
        if (src->seennl)
        {
            src->charno = 0;
            src->seennl = 0;
        }
        else
            src->charno++;
    }
    return ch;
}

/** @brief Skip ahead */
static MEX_INLINE void MEXSkipChars(MEXSource *src, int n)
{
    while (n > 0)
    {
        (void) MEXGetChar(src); /* preserve position info */
        n--;
    }
}


/* White-space as defined by XML 1.0 spec */
#define MEX_IS_WSPC(x) ((x) == 0x20 || (x) == 0x9 || (x) == 0xa || (x) == 0xd)

/** @brief Find length of leading white-space 

    Returns number of characters at the beginning which are whitespace
    (as defined by XML).
 */
static MEX_INLINE int MEXWhiteSpace(MEXSource *src)
{
    int k = src->pos;
    while (k < src->len && 
           MEX_IS_WSPC(src->buf[k]))
        k++;
    return k - src->pos;
}

/** @brief Search for a character in source 

    Returns offset or -1 if not found.
*/
static MEX_INLINE int MEXFindChar(MEXSource *src, int ch)
{
    int k = src->pos;
    while (k < src->len && src->buf[k] != ch)
        k++;
    if (k < src->len)
        return k - src->pos;
    else
        return -1;
}

/** @brief Parse an entity 

    Assumes that the leading '&' has already been consumed.
 */
static MEX_INLINE int MEXParseEntity(MEXSource *src)
{
    /* predefined... */
    if (MEXSkipPrefix(src,"amp;") == 0) 
        return '&';
    else if (MEXSkipPrefix(src,"lt;") == 0)
        return '<';
    else if (MEXSkipPrefix(src,"gt;") == 0)
        return '>';
    else if (MEXSkipPrefix(src,"apos;") == 0)
        return '\'';
    else if (MEXSkipPrefix(src,"quot;") == 0)
        return '"';
    
    /* character reference is only other valid choice for us */
    if (MEXPeekChar(src) != '#')
    {
        MEXSetError(MEX_ERR_P_ENTITY);
        return -1;
    }
    (void) MEXGetChar(src);
    if (MEXPeekChar(src) == 'x') /* optional */
        (void) MEXGetChar(src);
    {
        unsigned hex = 0;
        int ch;
        while ((ch = MEXGetChar(src)) >= 0)
        {
            if (ch == ';')
                break; /* done */
            else if (ch >= '0' && ch <= '9')
                hex = (hex * 16) + (ch - '0');
            else if (ch >= 'a' && ch <= 'f')
                hex = (hex * 16) + (ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F')
                hex = (hex * 16) + (ch - 'A' + 10);
            else
                break; /* invalid character... */
        }
        if (ch != ';' || hex > 255)
        {
            MEXSetError(MEX_ERR_P_ENTITY);
            return -1;
        }
        return hex;
    }
}


/** @brief Parse a standard name
    
    FIXME: XML 1.0 allows certain multi-byte characters
    to be a valid part of a name (CombiningChar and Extender
    classes) which we do not recognize here.  So there
    are valid names that we will treat as invalid.

    Anything over the ASCII range will be fine.

    This will parse out the namespace (Namespaces in XML 1.0)
    which imposes some restrictions on the use of ':' in names.
    If a namespace is present, it is returned in the second
    argument.
 */
static MEX_INLINE char *MEXParseName(MEXSource *src,
                                     char **nspart_r)
{
    int ch;
    MEXStringBuf sb;
    char *localpart = NULL, *nspart = NULL;
    
    MEXStringBufStart(&sb);

    /* first character */
    ch = MEXPeekChar(src);
    if (!isalpha(ch) && ch != '_')
    {
        MEXSetError(MEX_ERR_P_NAME);
        goto fail;
    }
    
    if (MEXStringBufAddChar(&sb,MEXGetChar(src))) 
    {
        MEXSetError(MEX_ERR_MALLOC);
        goto fail;
    }
    while ((ch = MEXPeekChar(src)) == '_' ||
           ch == '.' ||
           ch == '-' ||
           isalnum(ch))
    {
        if (MEXStringBufAddChar(&sb,MEXGetChar(src)))
        {
            MEXSetError(MEX_ERR_MALLOC);
            goto fail;
        }
    }
    /* Okay - is this the local part or the namespace part? */
    if (MEXPeekChar(src) == ':')
    {
        /* namespace part */
        if (!(nspart = MEXStringBufDup(&sb)))
        {
            MEXSetError(MEX_ERR_MALLOC);
            goto fail;
        }
        (void) MEXGetChar(src);

        /* Read local part */
        /* first character */
        ch = MEXPeekChar(src);
        if (!isalpha(ch) && ch != '_')
        {
            MEXSetError(MEX_ERR_P_NAME);
            goto fail;
        }
    
        MEXStringBufClear(&sb);
        if (MEXStringBufAddChar(&sb,ch)) 
        {
            MEXSetError(MEX_ERR_MALLOC);
            goto fail;
        }
        while ((ch = MEXPeekChar(src)) == '_' ||
               ch == '.' ||
               ch == '-' ||
               isalnum(ch))
        {
            if (MEXStringBufAddChar(&sb,MEXGetChar(src)))
            {
                MEXSetError(MEX_ERR_MALLOC);
                goto fail;
            }
        }
    }
    
    if (!(localpart = MEXStringBufDup(&sb)))
    {
        MEXSetError(MEX_ERR_MALLOC);
        goto fail;
    }

    MEXStringBufEnd(&sb);
    if (nspart_r)
        *nspart_r = nspart;
    else
        MEX_FREEIF(nspart);
    return localpart;

fail:
    MEXStringBufEnd(&sb);
    MEX_FREEIF(localpart);
    MEX_FREEIF(nspart);
    return NULL;
}

/** @brief Parse a quoted string */
static MEX_INLINE char *MEXParseString(MEXSource *src)
{
    /* NOTE: if we have to subsitute entities, the result is
       always a smaller string */
    char *buf;
    int k, sz, ch, quote;
    
    quote = MEXGetChar(src);
    if (quote != '\'' && quote != '"') 
    {
        MEXSetError(MEX_ERR_P_STRING);
        return NULL;
    }
    sz = MEXFindChar(src,quote);
    if (sz < 0)
    {
        MEXSetError(MEX_ERR_P_STRING);
        return NULL;
    }
    /* allow space for terminating nul */
    buf = MEX_MALLOC(sz+1);
    if (!buf)
    {
        MEXSetError(MEX_ERR_MALLOC);
        return NULL;
    }
    k = 0;
    ch = MEXGetChar(src);
    while (ch >= 0 && ch != quote)
    {
        if (ch == '<') /* illegal */
        {
            MEXSetError(MEX_ERR_P_STRING);
            MEX_FREE(buf);
            return NULL;
        }
        else if (ch == '&')
        {
            ch = MEXParseEntity(src);
            if (ch < 0)
            {
                /* error set by MEXParseEntity */
                MEX_FREE(buf);
                return NULL;
            }
        }
        buf[k++] = (char)ch;
        ch = MEXGetChar(src);
    }
    if (ch < 0)
    {
        MEXSetError(MEX_ERR_P_STRING);
        MEX_FREE(buf);
        return NULL;
    }
    buf[k] = '\0';
    return buf;
}

static MEX_INLINE int MEXParseComment(MEXSource *src)
{
    char tail[3];
    if (MEXGetChar(src) != '<' ||
        MEXGetChar(src) != '!' ||
        MEXGetChar(src) != '-' ||
        MEXGetChar(src) != '-')
    {
        MEXSetError(MEX_ERR_PARSE);
        return -1;
    }
    tail[0] = MEXGetChar(src);
    tail[1] = MEXGetChar(src);
    tail[2] = MEXGetChar(src);
    while (strncmp("-->",tail,3))
    {
        int ch;
        ch = MEXGetChar(src);
        if (ch < 0)
        {
            MEXSetError(MEX_ERR_PARSE);
            return -1;
        }
        tail[0] = tail[1];
        tail[1] = tail[2];
        tail[2] = ch;
    }
    return 0;
}

static int MEXParsePI(MEXSource *src)
{
    char tail[2];
    if (MEXSkipPrefix(src,"<?"))
    {
        MEXSetError(MEX_ERR_P_PI);
        return -1;
    }
    tail[0] = MEXGetChar(src);
    tail[1] = MEXGetChar(src);
    while (strncmp("?>",tail,2))
    {
        int ch = MEXGetChar(src);
        if (ch < 0)
        {
            MEXSetError(MEX_ERR_P_PI);
            return -1;
        }
        tail[0] = tail[1];
        tail[1] = ch;
    }
    return 0;
}

static int MEXSkipMisc(MEXSource *src)
{
    while (1) {
        int spc;
        if ((spc = MEXWhiteSpace(src)) > 0)
        {
            MEXSkipChars(src,spc);
            continue;
        }
        if (MEXHasPrefix(src,"<?") == 0)
        {
            if (MEXParsePI(src))
                return -1;
            continue;
        }
        if (MEXHasPrefix(src,"<!--") == 0)
        {
            if (MEXParseComment(src))
                return -1;
            continue;
        }
        /* neither white-space, PI, nor comment */
        break;
    }
    return 0;
}

/** @brief Skip a piece that starts with "<!" and finishes with ">"
        
    We do not attempt to parse this completely.  Instead we
    simplify this as a nested set of "<! ... >" pieces.

    Thus, we should accept valid constructs but may also
    accept some invalid ones.
*/
static int MEXSkipDTDPiece(MEXSource *src)
{
    /* We do not care about what is here, but want to be wary of
       comments and PIs and nested DTD pieces */
    int ch;
    if (MEXSkipPrefix(src,"<!"))
    {
        MEXSetError(MEX_ERR_P_DTD);
        return -1;
    }
    while ((ch = MEXPeekChar(src)) >= 0 && ch != '>')
    {
        if (ch == '<')
        {
            if (MEXHasPrefix(src,"<!") == 0) 
            {
                if (MEXSkipDTDPiece(src))
                    return -1;
            }
            else if (MEXHasPrefix(src,"<?") == 0)
            {
                if (MEXParsePI(src))
                    return -1;
            }
            else if (MEXHasPrefix(src,"<!--") == 0)
            {
                if (MEXParseComment(src))
                    return -1;
            }
            else 
            {
                /* no other valid place for '<' */
                MEXSetError(MEX_ERR_P_DTD);
                return -1;
            }
        } else
            (void) MEXGetChar(src);
    }
    if (ch != '>')
    {
        MEXSetError(MEX_ERR_P_DTD);
        return -1;
    }
    (void) MEXGetChar(src);
    return 0;
}

static int MEXPrologParse(MEXSource *src, MEXDocument *doc)
{
    /* parse optional prolog */
    if (MEXSkipPrefix(src,"<?xml") == 0)
    {
        int wspc;

        /* prolog exists */

        /* version section: S 'version' S? '=' S? string */
        wspc = MEXWhiteSpace(src);
        if (wspc <= 0)
        {
            /* invalid prolog - need space after <?xml */
            MEXSetError(MEX_ERR_PARSE);
            return -1;
        }
        MEXSkipChars(src,wspc);
        if (MEXSkipPrefix(src,"version")) 
        {
            /* invalid prolog - version is required */
            MEXSetError(MEX_ERR_PARSE);
            return -1;
        }
        /* skip white-space */
        MEXSkipChars(src,MEXWhiteSpace(src));
        if (MEXGetChar(src) != '=')
        {
            /* invalid version */
            MEXSetError(MEX_ERR_PARSE);
            return -1;
        }
        /* skip white-space */
        MEXSkipChars(src,MEXWhiteSpace(src));
        doc->version = MEXParseString(src);
        if (!doc->version)
            return -1; /* MEXParseString sets error */
        /* skip white-space */
        MEXSkipChars(src,MEXWhiteSpace(src));
        if (MEXSkipPrefix(src,"encoding") == 0)
        {
            /* encoding: 'encoding' S* '=' S* string */
            /* skip white-space */
            MEXSkipChars(src,MEXWhiteSpace(src));
            if (MEXGetChar(src) != '=')
            {
                /* invalid encoding */
                MEXSetError(MEX_ERR_PARSE);
                return -1;
            }
            MEXSkipChars(src,MEXWhiteSpace(src));
            doc->encoding = MEXParseString(src);
            if (!doc->encoding)
                return -1;
            MEXSkipChars(src,MEXWhiteSpace(src));
        }
        if (MEXSkipPrefix(src,"standalone") == 0)
        {
            /* standalone: 'standlone' S* '=' S* string
               - where string must be either "yes" or "no"
            */
            char *value;
            MEXSkipChars(src,MEXWhiteSpace(src));
            if (MEXGetChar(src) != '=')
            {
                /* invalid standalone */
                MEXSetError(MEX_ERR_PARSE);
                return -1;
            }
            MEXSkipChars(src,MEXWhiteSpace(src));
            if (!(value = MEXParseString(src)))
                return -1;
            if (strcmp(value,"yes") == 0)
                doc->standalone = 1;
            else if (strcmp(value,"no") == 0)
                doc->standalone = 0;
            else
            {
                /* invalid value for standalone */
                MEX_FREE(value);
                return -1;
            }
            MEX_FREE(value);
            MEXSkipChars(src,MEXWhiteSpace(src));
        }
        if (!MEXSkipPrefix(src,"?>") == 0)
        {
            /* invalid prolog - either extra junk or missing end */
            MEXSkipChars(src,MEXWhiteSpace(src));
            return -1;
        }
    }
    if (MEXSkipMisc(src))
        return -1;
    /* Skip over doctypedecl if present */
    if (MEXHasPrefix(src,"<!DOCTYPE") == 0)
    {
        if (MEXSkipDTDPiece(src))
            return -1;
    }
    if (MEXSkipMisc(src))
        return -1;
    return 0;
}

MEX_API int MEXParseCdata(MEXSource *src, MEXNode **node_r)
{
    MEXStringBuf sb;
    int ch;
    
    MEXStringBufStart(&sb);

    while ((ch = MEXPeekChar(src)) >= 0)
    {
        if (ch == '<') 
        {
            if (MEXSkipPrefix(src,"<![CDATA[") == 0)
            {
                while ((ch = MEXGetChar(src)) >= 0)
                {
                    if (ch == ']' && MEXSkipPrefix(src,"]>") == 0)
                    {
                        /* end of CDATA section */
                        break;
                    }
                    else
                        MEXStringBufAddChar(&sb,ch);
                }
                if (ch < 0)
                {
                    MEXSetError(MEX_ERR_P_CDATA);
                    goto fail;
                }
            }
            else
                break; /* end */
        }
        else
            MEXStringBufAddChar(&sb,MEXGetChar(src));
    }

    if (sb.use == 0)
    {
        if (node_r)
            *node_r = NULL;
    }
    else
    {
        if (node_r)
            *node_r = MEXNodeCdata(sb.buf,sb.use);
    }
    MEXStringBufEnd(&sb);
    return 0;

fail:
    MEXStringBufEnd(&sb);
    return -1;
}

MEX_API MEXNode *MEXTagParse(MEXSource *src)
{
    MEXNode *tag;
    char *name, *nsid;

    if (MEXPeekChar(src) != '<')
    {
        MEXSetError(MEX_ERR_P_ATTR);
        return NULL;
    }
    (void) MEXGetChar(src);
    if (!(name = MEXParseName(src,&nsid)))
        return NULL;
    if (!(tag = MEXNodeTag(name)))
    {
        MEX_FREE(name);
        MEX_FREEIF(nsid);
        return NULL;
    }
    MEX_FREE(name);
    if (nsid)
        tag->data.tag.nsid = nsid;

    while (1) {
        /* skip white-space */
        MEXSkipChars(src,MEXWhiteSpace(src));

        if (MEXSkipPrefix(src,"/>") == 0) 
        {
            /* empty tag */
            return tag;
        }
        else if (MEXSkipPrefix(src,">") == 0)
        {
            /* end of opening tag */
            break;
        }
        else 
        {
            char *attrnsid;
            char *attrname;
            char *attrvalue;
            
            if (!(attrname = MEXParseName(src,&attrnsid)))
            {
                MEXNodeFree(tag);
                return NULL;
            }
            /* Permit white-space around '=' to match conformance tests */
            MEXSkipChars(src,MEXWhiteSpace(src));
            if (MEXGetChar(src) != '=')
            {
                MEXSetError(MEX_ERR_P_ATTR);
                MEX_FREE(attrname);
                MEX_FREEIF(attrnsid);
                MEXNodeFree(tag);
                return NULL;
            }
            MEXSkipChars(src,MEXWhiteSpace(src));
            if (!(attrvalue  = MEXParseString(src)))
            {
                MEX_FREE(attrname);
                MEX_FREEIF(attrnsid);
                MEXNodeFree(tag);
                return NULL;
            }
            if (MEXNodeSetAttr(tag,attrnsid,attrname,attrvalue))
            {
                MEX_FREE(attrname);
                MEX_FREEIF(attrnsid);
                MEX_FREE(attrvalue);
                MEXNodeFree(tag);
                return NULL;
            }
            MEX_FREE(attrname);
            MEX_FREEIF(attrnsid);
            MEX_FREE(attrvalue);
        }
    }

    /* Parse body of tag */
    {
        MEXNode *cdata;
        int ch;
        while ((ch = MEXPeekChar(src)) >= 0)
        {
            if (MEXParseCdata(src,&cdata))
            {
                MEXNodeFree(tag);
                return NULL;
            }
            if (cdata)
            {
                if (MEXNodeAddChild(tag,cdata))
                {
                    MEXNodeFree(cdata);
                    MEXNodeFree(tag);
                    return NULL;
                }
            }
            /* if we are well-formed, the following should be either
               an open tag (for a child node) or a close tag for the
               tag we are currently parsing */
            if (MEXSkipPrefix(src,"</") == 0)
            {
                char *endname, *endnsid;
                if (!(endname = MEXParseName(src,&endnsid)))
                {
                    MEXNodeFree(tag);
                    return NULL;
                }
                if (!MEX_STREQ(endname,tag->data.tag.name) ||
                    !MEX_STREQ(endnsid,tag->data.tag.nsid))
                {
                    MEXSetError(MEX_ERR_P_MISMATCH);
                    MEX_FREE(endname);
                    MEX_FREEIF(endnsid);
                    MEXNodeFree(tag);
                    return NULL;
                }
                MEX_FREE(endname);
                MEX_FREEIF(endnsid);
                MEXSkipChars(src,MEXWhiteSpace(src));
                if (MEXGetChar(src) != '>')
                {
                    MEXSetError(MEX_ERR_P_TAG);
                    MEXNodeFree(tag);
                    return NULL;
                }
                break; /* stop looping */
            }
            else if (MEXPeekChar(src) == '<')
            {
                MEXNode *subtag;
                if (!(subtag = MEXTagParse(src)))
                {
                    MEXNodeFree(tag);
                    return NULL;
                }
                if (MEXNodeAddChild(tag,subtag))
                {
                    MEXNodeFree(subtag);
                    MEXNodeFree(tag);
                    return NULL;
                }
            }
            else
            {
                MEXSetError(MEX_ERR_P_TAG);
                MEXNodeFree(tag);
                return NULL;
            }
        }
    }

    return tag;
}

MEX_API MEXDocument *MEXDocumentParse(char *parsebuf, int parselen, int flags)
{
    MEXDocument *doc;
    MEXSource srcobj, *src;

    if (!(doc = MEX_MALLOCOBJ(MEXDocument)))
    {
        MEXSetError(MEX_ERR_MALLOC);
        return NULL;
    }
    doc->standalone = 1; /* assume yes */

    src = &srcobj;
    MEXSourceStart(src,parsebuf,parselen,flags);

    if (MEXPrologParse(src,doc))
        goto fail;

    doc->root = MEXTagParse(src);
    if (!doc->root)
        goto fail;

#if 0 /* need some more debugging... */
    if (MEXSkipMisc(src))
        goto fail;

    if (MEXPeekChar(src) >= 0)
    {
        printf("Left: '%s'\n",src->buf+src->pos);
        MEXSetError(MEX_ERR_P_JUNK);
        goto fail;
    }
#endif

    MEXSourceEnd(src);
    return doc;

fail:
    MEXSourceEnd(src);
    MEXDocumentFree(doc);
    return NULL;
}

#ifdef MEX_EMBEDDED
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MEX_EMBEDDED */
