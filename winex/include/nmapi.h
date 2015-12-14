/*
 * nmapi.h
 */

#ifndef __WINE_NMAPI_H
#define __WINE_NMAPI_H

#include "windef.h"

#include "pshpack1.h"

#define MAX_PROTOCOL_NAME_LEN 16
#define MAX_PROTOCOL_COMMENT_LEN 256

typedef HANDLE HPROTOCOL, *LPHPROTOCOL;
typedef HANDLE HFRAME, *LPHFRAME;
typedef HANDLE HPROPERTY, *LPHPROPERTY;

typedef LARGE_INTEGER* LPLARGEINT;

/* parsers */

typedef struct _PF_FOLLOWENTRY {
  char szProtocol[MAX_PROTOCOL_NAME_LEN];
} PF_FOLLOWENTRY, *PPF_FOLLOWENTRY;

typedef struct _PF_FOLLOWSET {
  DWORD nEntries;
  PF_FOLLOWENTRY Entry[0];
} PF_FOLLOWSET, *PPF_FOLLOWSET;

/* FIXME: check values */
typedef enum _PF_HANDOFFVALUEFORMATBASE {
  HANDOFF_VALUE_FORMAT_BASE_UNKNOWN,
  HANDOFF_VALUE_FORMAT_BASE_DECIMAL,
  HANDOFF_VALUE_FORMAT_BASE_HEX
} PF_HANDOFFVALUEFORMATBASE;

typedef struct _PF_HANDOFFENTRY {
  char szIniFile[MAX_PATH];
  char szIniSection[MAX_PATH];
  char szProtocol[MAX_PROTOCOL_NAME_LEN];
  DWORD dwHandOffValue;
  PF_HANDOFFVALUEFORMATBASE ValueFormatBase;
} PF_HANDOFFENTRY, *PPF_HANDOFFENTRY;

typedef struct _PF_HANDOFFSET {
  DWORD nEntries;
  PF_HANDOFFENTRY Entry[0];
} PF_HANDOFFSET, *PPF_HANDOFFSET;

typedef struct _PF_PARSERINFO {
  char szProtocolName[MAX_PROTOCOL_NAME_LEN];
  char szComment[MAX_PROTOCOL_COMMENT_LEN];
  char szHelpFile[MAX_PATH];
  PPF_FOLLOWSET pWhoCanPrecedeMe;
  PPF_FOLLOWSET pWhoCanFollowMe;
  PPF_HANDOFFSET pWhoHandsOffToMe;
  PPF_HANDOFFSET pWhoDoIHandOffTo;
} PF_PARSERINFO, *PPF_PARSERINFO;

typedef struct _PF_PARSERDLLINFO {
  DWORD nParsers;
  PF_PARSERINFO ParserInfo[0];
} PF_PARSERDLLINFO, *PPF_PARSERDLLINFO;

/* properties */

enum {
  PROP_TYPE_VOID,
  PROP_TYPE_SUMMARY = 1,
  PROP_TYPE_BYTE = 2,
  PROP_TYPE_WORD = 3,
  PROP_TYPE_DWORD = 4,
  PROP_TYPE_LARGEINT,
  PROP_TYPE_ADDR,
  PROP_TYPE_TIME,
  PROP_TYPE_STRING = 8,
  PROP_TYPE_IP_ADDRESS = 9,
  PROP_TYPE_IPX_ADDRESS = 10,
  PROP_TYPE_BYTESWAPPED_WORD = 11,
  PROP_TYPE_BYTESWAPPED_DWORD,
  PROP_TYPE_TYPED_STRING,
  PROP_TYPE_RAW_DATA,
  PROP_TYPE_COMMENT,
  PROP_TYPE_SRCFRIENDLYNAME,
  PROP_TYPE_DSTFRIENDLYNAME,
  PROP_TYPE_TOKENRING_ADDRESS,
  PROP_TYPE_FDDI_ADDRESS,
  PROP_TYPE_ETHERNET_ADDRESS,
  PROP_TYPE_OBJECT_IDENTIFIER,
  PROP_TYPE_VINES_IP_ADDRESS,
  PROP_TYPE_VAR_LEN_SMALL_INT,
};

enum {
  PROP_QUAL_NONE = 0,
  PROP_QUAL_RANGE,
  PROP_QUAL_SET,
  PROP_QUAL_BITFIELD,
  PROP_QUAL_LABELED_SET = 4,
  PROP_QUAL_LABELED_BITFIELD = 10,
  PROP_QUAL_CONST,
  PROP_QUAL_FLAGS,
  PROP_QUAL_ARRAY,
};

#define IFLAG_ERROR 1
#define IFLAG_SWAPPED 2
#define IFLAG_UNICODE 4

typedef struct _LABELED_BYTE {
  BYTE Value;
  LPSTR Label;
} LABELED_BYTE, *LPLABELED_BYTE;

typedef struct _LABELED_WORD {
  WORD Value;
  LPSTR Label;
} LABELED_WORD, *LPLABELED_WORD;

typedef struct _LABELED_DWORD {
  DWORD Value;
  LPSTR Label;
} LABELED_DWORD, *LPLABELED_DWORD;

typedef struct _LABELED_LARGEINT {
  LARGE_INTEGER Value;
  LPSTR Label;
} LABELED_LARGEINT, *LPLABELED_LARGEINT;

typedef struct _LABELED_SYSTEMTIME {
  SYSTEMTIME Value;
  LPSTR Label;
} LABELED_SYSTEMTIME, *LPLABELED_SYSTEMTIME;

typedef struct _LABELED_BIT {
  BYTE BitNumber;
  LPSTR LabelOff;
  LPSTR LabelOn;
} LABELED_BIT, *LPLABELED_BIT;

typedef LPVOID TYPED_STRING; /* FIXME */

typedef struct range {
  DWORD MinValue;
  DWORD MaxValue;
} RANGE, *LPRANGE;

typedef struct _SET {
  DWORD nEntries;
  union {
    LPBYTE lpByteTable;
    LPWORD lpWordTable;
    LPDWORD lpDwordTable;
    LPLARGEINT lpLargeIntTable;
    LPSYSTEMTIME lpSystemTimeTable;
    LPLABELED_BYTE lpLabeledByteTable;
    LPLABELED_WORD lpLabeledWordTable;
    LPLABELED_DWORD lpLabeledDwordTable;
    LPLABELED_LARGEINT lpLabeledLargeIntTable;
    LPLABELED_SYSTEMTIME lpLabeledSystemTimeTable;
    LPLABELED_BIT lpLabeledBit;
    LPVOID lpVoidTable;
  };
} SET, *LPSET;

typedef struct _PROPERTYINFO {
  HPROPERTY hProperty;
  DWORD Version;
  LPSTR Label;
  LPSTR Comment;
  BYTE DataType;
  BYTE DataQualifier;
  union {
    LPVOID lpExtendedInfo;
    LPRANGE lpRange;
    LPSET lpSet;
    DWORD Bitmask;
    DWORD Value;
  };
  WORD FormatStringSize;
  LPVOID InstanceData;
} PROPERTYINFO, *LPPROPERTYINFO;

typedef struct _PROPERTYINSTEX {
  WORD Length;
  WORD LengthEx;
  LPVOID lpData;
  union {
    BYTE Byte[0];
    WORD Word[0];
    DWORD Dword[0];
    LARGE_INTEGER LargeInt[0];
    SYSTEMTIME SysTime[0];
    TYPED_STRING TypedString;
  };
} PROPERTYINSTEX, *LPPROPERTYINSTEX;

typedef struct _PROPERTYINST {
  LPPROPERTYINFO lpPropertyInfo;
  LPSTR szPropertyText;
  union {
    LPVOID lpData;
    LPBYTE lpByte;
    LPWORD lpWord;
    LPDWORD lpDword;
    LPLARGEINT lpLargeInt;
    LPSYSTEMTIME lpSysTime;
    LPPROPERTYINSTEX lpPropertyInstEx;
  };
  WORD DataLength;
  WORD Level  :4;
  WORD HelpID  :12;
  DWORD IFlags;
} PROPERTYINST, *LPPROPERTYINST;

typedef LPVOID LPPROPERTYDATABASE; /* FIXME */

typedef struct _PROTOCOLINFO {
  DWORD ProtocolID;
  LPPROPERTYDATABASE PropertyDatabase;
  BYTE ProtocolName[16];
  BYTE HelpFile[16];
  BYTE Comment[128];
} PROTOCOLINFO, *LPPROTOCOLINFO;

/* parser entry points */

enum {
  PROTOCOL_STATUS_RECOGNIZED = 0,
  PROTOCOL_STATUS_NOT_RECOGNIZED = 1,
  PROTOCOL_STATUS_CLAIMED = 2,
  PROTOCOL_STATUS_NEXT_PROTOCOL = 3,
};

typedef PPF_PARSERDLLINFO CALLBACK (*PARSERAUTOINSTALLINFO)(void); /* ? */

typedef VOID CALLBACK (*REGISTER)(
  HPROTOCOL hProtocol);
typedef VOID CALLBACK (*DEREGISTER)(
  HPROTOCOL hProtocol);
typedef LPBYTE CALLBACK (*RECOGNIZEFRAME)(
  HFRAME hFrame,
  LPBYTE lpFrame,
  LPBYTE lpProtocol,
  DWORD MacType,
  DWORD BytesLeft,
  HPROTOCOL hPreviousProtocol,
  DWORD nPreviousProtocolOffset,
  LPDWORD ProtocolStatusCode,
  LPHPROTOCOL phNextProtocol,
  PDWORD_PTR lpInstData );
typedef DWORD CALLBACK (*ATTACHPROPERTIES)(
  HFRAME hFrame,
  LPBYTE lpFrame,
  LPBYTE lpProtocol,
  DWORD MacType,
  DWORD BytesLeft,
  HPROTOCOL hPreviousProtocol,
  DWORD nPreviousProtocolOffset,
  DWORD lpInstData);
typedef DWORD CALLBACK (*FORMATPROPERTIES)(
  HFRAME hFrame,
  LPBYTE lpFrame,
  LPBYTE lpProtocol,
  DWORD nPropertyInsts,
  LPPROPERTYINST lpPropInst);

typedef struct __ENTRYPOINTS {
  REGISTER Register;
  DEREGISTER Deregister;
  RECOGNIZEFRAME RecognizeFrame;
  ATTACHPROPERTIES AttachProperties;
  FORMATPROPERTIES FormatProperties;
} ENTRYPOINTS, *LPENTRYPOINTS;
#define ENTRYPOINTS_SIZE sizeof(ENTRYPOINTS)

/* nmapi.dll entry points */

HPROPERTY WINAPI AddProperty(
  HPROTOCOL hProtocol,
  LPPROPERTYINFO PropertyInfo );

BOOL WINAPI AttachPropertyInstance(
  HFRAME hFrame,
  HPROPERTY hProperty,
  DWORD Length,
  LPVOID lpData,
  DWORD HelpID,
  DWORD IndentLevel,
  DWORD IFlags);

BOOL WINAPI AttachPropertyInstanceEx(
  HFRAME hFrame,
  HPROPERTY hProperty,
  DWORD Length,
  LPVOID lpData,
  DWORD LengthEx,
  LPVOID lpDataEx,
  DWORD HelpID,
  DWORD IndentLevel,
  DWORD IFlags);

DWORD WINAPI CreatePropertyDatabase(
  HPROTOCOL hProtocol,
  DWORD nProperties);

HPROTOCOL WINAPI CreateProtocol(
  LPSTR ProtocolName,
  LPENTRYPOINTS lpEntryPoints,
  DWORD cbEntryPoints);

DWORD WINAPI DestroyPropertyDatabase(
  HPROTOCOL hProtocol);

VOID WINAPI DestroyProtocol(
  HPROTOCOL hProtocol);

DWORD WINAPIV FormatPropertyInstance(
  LPPROPERTYINST lpPropertyInst);

HPROTOCOL WINAPI GetProtocolFromName(
  LPSTR ProtocolName);

LPPROTOCOLINFO WINAPI GetProtocolInfo(
  HPROTOCOL hProtocol);

DWORD WINAPI GetProtocolStartOffsetHandle(
  HFRAME hFrame,
  HPROTOCOL hProtocol);

#include "poppack.h"

#endif  /* __WINE_NMAPI_H */
