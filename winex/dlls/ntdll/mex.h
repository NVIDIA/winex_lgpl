#ifndef MEX_H
#define MEX_H

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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if 0 /* fool auto-indentation */
}
#endif

/* A minimal XML parser - builds a DOM-like tree out of a well-formed
 * XML document. 
 *
 * Limitations:
 * - only UTF-8 encoding (not UTF-16)
 * - no DTD or external entities
 * - parses out of memory all in one shot
 *
 * Target usage:
 * - relatively small structural XML (e.g. configs, specs, etc.)
 *   as opposed to full documents (e.g. XHTML)
 */

/* Namespace hiding
 * - to embed this parser in another C file, define MEX_EMBEDDED
 *   before including this file (and do not compile in mex.c)
 */
#ifdef MEX_EMBEDDED
#define MEX_API static
#else
#define MEX_API
#endif /* MEX_EMBEDDED */

/* error codes */
#define MEX_ERR_NONE      0  /* no error */
#define MEX_ERR_MALLOC    1  /* malloc failed */
#define MEX_ERR_NOEXIST   2  /* attribute does not exist */
#define MEX_ERR_INVALID   3  /* operation with null param or wrong node type */
#define MEX_ERR_INTERN    4  /* an internal screw-up - should never happen */
#define MEX_ERR_PARSE     5  /* general parse failures (not covered elsewhere) */
/* other parse errors */
#define MEX_ERR_P_ENTITY    6  /* invalid entity */
#define MEX_ERR_P_STRING    7  /* badly-formed string/attribute value */
#define MEX_ERR_P_PI        8  /* badly-formed processing instruction */
#define MEX_ERR_P_DTD       9  /* badly-formed DTD section */
#define MEX_ERR_P_NAME     10  /* badly-formed name */
#define MEX_ERR_P_TAG      11  /* badly-formed tag */
#define MEX_ERR_P_ATTR     12  /* badly-formed attribute */
#define MEX_ERR_P_CDATA    13  /* badly-formed CDATA section */
#define MEX_ERR_P_MISMATCH 14  /* mismatch between start-end tags */
#define MEX_ERR_P_JUNK     15  /* junk after document */

/*
  Error handling:
  - convention (unless specified otherwise)
      - functions that return pointers return NULL on error
      - functions that return ints return >= 0 on success, < 0 on error

  - calling MEXGetError() gets last reported error code.
  - error code only gets cleared by calling MEXGetError()
 */
MEX_API int MEXGetError(void);

#define MEX_TAG     0
#define MEX_CDATA   1

/** @brief An attribute for an XML tag */
typedef struct MEX_attr
{
    struct MEX_attr *next;
    /** @brief Namespace ID of the attribute 
        
        May be NULL if the namespace is not specified.
     */
    char *nsid;
    /** @brief Name of the attribute 

        Just the local part without the namespace part.
     */
    char *name;
    /** @brief Value of the attribute */
    char *value; 
} MEXAttr;

/** @brief A node in the XML tree 

    A node can represent either a tag or text (CDATA).
*/
typedef struct MEX_node
{
    /** @brief Next sibling node (linked-list) */
    struct MEX_node *next;
    /** @brief Type of the node 

        A value of MEX_TAG means that this node represents a tag.
        A value of MEX_CDATA means that this node represents text.
     */
    int type;
    union {
        struct {
            /** @brief Namespace ID of the tag 

                May be NULL if this tag has no explicit namespace.
             */
            char *nsid;
            /** @brief Name of the tag 

                Just the local part without the namespace part.
            */
            char *name;
            /** @brief List of attributes for the tag */
            MEXAttr *attrs;
            /** @brief List of children for the tag 

                Children nodes are all nodes that are contained
                with the tag, both tags and text.
             */
            struct MEX_node *childFirst, *childLast;
        } tag;
        struct {
            /** @brief The contents of the text (CDATA) 
                
                This buffer is null-terminated.
             */
            char *buf;
            /** @brief The length of the text (CDATA) in bytes */
            int len;
        } cdata;
    } data;
} MEXNode;

typedef struct MEX_document
{
    /* prologue information */
    char *version;
    char *encoding;
    int standalone;
    MEXNode *root;
} MEXDocument;


/** @brief A simple dynamic string buffer */
typedef struct MEX_stringbuf
{
    /** @brief A pointer to the actual buffer */
    char *buf;
    /** @brief How many meaningful characters are in the buffer?
        
        Does not include terminating nul character.
    */
    int use;
    /** @brief How many bytes are allocated for buf */
    int spc;
} MEXStringBuf;

MEX_API void MEXStringBufStart(MEXStringBuf *);
MEX_API void MEXStringBufEnd(MEXStringBuf *);
MEX_API char *MEXStringBufDup(MEXStringBuf *);
MEX_API void MEXStringBufClear(MEXStringBuf *);
MEX_API int MEXStringBufGrow(MEXStringBuf *, int nch);
MEX_API int MEXStringBufAddChar(MEXStringBuf *, int);
MEX_API int MEXStringBufAddString(MEXStringBuf *, char *, int);

MEX_API void MEXNodeFree(MEXNode *node);
MEX_API MEXNode *MEXNodeTag(char *name);
MEX_API MEXNode *MEXNodeCdata(char *buf, int len);
MEX_API int MEXNodeSetAttr(MEXNode *, char *nsid,
                           char *name, char *value);
MEX_API char *MEXNodeGetAttr(MEXNode *, char *nsid,
                             char *name);
MEX_API int MEXNodeAddChild(MEXNode *parent, MEXNode *child);
MEX_API MEXNode *MEXNodeContent(MEXNode *node);

/* "flags" - currently unused - pass 0 as argument */
MEX_API MEXDocument *MEXDocumentParse(char *buf, int len, int flags);
MEX_API void MEXDocumentFree(MEXDocument *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MEX_H */
