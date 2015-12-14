#ifndef __WINE_DPADDR_H
#define __WINE_DPADDR_H

#include "ole2.h"

#include "winsock2.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "dplay8.h"

/* DirecyPlay8Address CLSID */


/* {934A9523-A3CA-4bc5-ADA0-D6D95D979421} */
DEFINE_GUID(CLSID_DirectPlay8Address,
0x934a9523, 0xa3ca, 0x4bc5, 0xad, 0xa0, 0xd6, 0xd9, 0x5d, 0x97, 0x94, 0x21);

/* IIDs for DirectPlay8 interfaces */

/* {83783300-4063-4c8a-9DB3-82830A7FEB31} */
DEFINE_GUID(IID_IDirectPlay8Address,
0x83783300, 0x4063, 0x4c8a, 0x9d, 0xb3, 0x82, 0x83, 0xa, 0x7f, 0xeb, 0x31);

/* {E5A0E990-2BAD-430b-87DA-A142CF75DE58} */
DEFINE_GUID(IID_IDirectPlay8AddressIP,
0xe5a0e990, 0x2bad, 0x430b, 0x87, 0xda, 0xa1, 0x42, 0xcf, 0x75, 0xde, 0x58);


/* interface pointers */

typedef struct IDirectPlay8Address              IDirectPlay8Address, *PDIRECTPLAY8ADDRESS, *LPDIRECTPLAY8ADDRESS;
typedef struct IDirectPlay8AddressIP		IDirectPlay8AddressIP, *PDIRECTPLAY8ADDRESSIP, *LPDIRECTPLAY8ADDRESSIP;

/* external types */

/* constants */

/* async flags */
#define DPNA_DATATYPE_STRING				0x00000001
#define DPNA_DATATYPE_DWORD					0x00000002
#define DPNA_DATATYPE_GUID					0x00000003
#define DPNA_DATATYPE_BINARY				0x00000004
#define DPNA_DATATYPE_STRING_ANSI           0x00000005

#define DPNA_DPNSVR_PORT					6073

#define DPNA_INDEX_INVALID					0xFFFFFFFF

#if defined(__WINE__)
/* yeah, ugly hack, but not everyone has '-fshort-wchar' :( */
static const WCHAR DPNA_SEPARATOR_KEYVALUE = '=';
static const WCHAR DPNA_SEPARATOR_USERDATA = '#';
static const WCHAR DPNA_SEPARATOR_COMPONENT = ';';
static const WCHAR DPNA_ESCAPE = '%';

static const WCHAR DPNA_HEADER[] = {'x', '-', 'd', 'i', 'r', 'e', 'c', 't', 'p', 'l', 'a', 'y', ':', '/', 0};

static const WCHAR DPNA_KEY_APPLICATION_INSTANCE[] = {'a', 'p', 'p', 'l', 'i', 'c', 'a', 't', 'i', 'o', 'n', 'i', 'n', 's', 't', 'a', 'n', 'c', 'e', 0};
static const WCHAR DPNA_KEY_BAUD[] = {'b', 'a', 'u', 'd', 0};
static const WCHAR DPNA_KEY_DEVICE[] = {'d', 'e', 'v', 'i', 'c', 'e', 0};
static const WCHAR DPNA_KEY_FLOWCONTROL[] = {'f', 'l', 'o', 'w', 'c', 'o', 'n', 't', 'r', 'o', 'l', 0};
static const WCHAR DPNA_KEY_HOSTNAME[] = {'h', 'o', 's', 't', 'n', 'a', 'm', 'e', 0};
static const WCHAR DPNA_KEY_NAMEINFO[] = {'n', 'a', 'm', 'e', 'i', 'n', 'f', 'o', 0};
static const WCHAR DPNA_KEY_NAT_RESOLVER[] = {'n', 'a', 't', 'r', 'e', 's', 'o', 'l', 'v', 'e', 'r', 0};
static const WCHAR DPNA_KEY_NAT_RESOLVER_USER_STRING[] = {'n', 'a', 't', 'r', 'e', 's', 'o', 'l', 'v', 'e', 'r', 'u', 's', 'e', 'r', 's', 't', 'r', 'i', 'n', 'g', 0};
static const WCHAR DPNA_KEY_PARITY[] = {'p', 'a', 'r', 'i', 't', 'y', 0};
static const WCHAR DPNA_KEY_PHONENUMBER[] = {'p', 'h', 'o', 'n', 'e', 'n', 'u', 'm', 'b', 'e', 'r', 0};
static const WCHAR DPNA_KEY_PORT[] = {'p', 'o', 'r', 't', 0};
static const WCHAR DPNA_KEY_PROGRAM[] = {'p', 'r', 'o', 'g', 'r', 'a', 'm', 0};
static const WCHAR DPNA_KEY_PROVIDER[] = {'p', 'r', 'o', 'v', 'i', 'd', 'e', 'r', 0};
static const WCHAR DPNA_KEY_SCOPE[] = {'s', 'c', 'o', 'p', 'e', 0};
static const WCHAR DPNA_KEY_STOPBITS[] = {'s', 't', 'o', 'p', 'b', 'i', 't', 's', 0};
static const WCHAR DPNA_KEY_TRAVERSALMODE[] = {'t', 'r', 'a', 'v', 'e', 'r', 's', 'a', 'l', 'm', 'o', 'd', 'e', 0};

#else /* defined(__WINE__) */


/* address elements */
#define DPNA_SEPARATOR_KEYVALUE				L'='
#define DPNA_SEPARATOR_USERDATA				L'#'
#define DPNA_SEPARATOR_COMPONENT			L';'
#define DPNA_ESCAPECHAR						L'%'

#define DPNA_HEADER		                    L"x-directplay:/"

/* address components */
#define DPNA_KEY_APPLICATION_INSTANCE		L"applicationinstance"
#define DPNA_KEY_BAUD						L"baud"
#define DPNA_KEY_DEVICE						L"device"
#define DPNA_KEY_FLOWCONTROL				L"flowcontrol"
#define DPNA_KEY_HOSTNAME					L"hostname"
#define DPNA_KEY_NAMEINFO					L"nameinfo"
#define DPNA_KEY_NAT_RESOLVER					L"natresolver"
#define DPNA_KEY_NAT_RESOLVER_USER_STRING			L"natresolveruserstring"
#define DPNA_KEY_PARITY						L"parity"
#define DPNA_KEY_PHONENUMBER				L"phonenumber"
#define DPNA_KEY_PORT						L"port"
#define DPNA_KEY_PROCESSOR					L"processor"
#define DPNA_KEY_PROGRAM					L"program"
#define DPNA_KEY_PROVIDER					L"provider"
#define DPNA_KEY_SCOPE						L"scope"
#define DPNA_KEY_STOPBITS					L"stopbits"
#define DPNA_KEY_TRAVERSALMODE					L"traversalmode"

#endif /* defined(__WINE__) */


/* baud rate values */
#define DPNA_BAUD_RATE_9600					9600
#define DPNA_BAUD_RATE_14400				14400
#define DPNA_BAUD_RATE_19200				19200
#define DPNA_BAUD_RATE_38400				38400
#define DPNA_BAUD_RATE_56000				56000
#define DPNA_BAUD_RATE_57600				57600
#define DPNA_BAUD_RATE_115200				115200

#if defined(__WINE__)
/* see above */
static const WCHAR DPNA_STOP_BITS_ONE[] = {'1', 0};
static const WCHAR DPNA_STOP_BITS_ONE_FINE[] = {'1', '.', '5', 0};
static const WCHAR DPNA_STOP_BITS_TWO[] = {'2', 0};

static const WCHAR DPNA_PARITY_NONE[] = {'N', 'O', 'N', 'E', 0};
static const WCHAR DPNA_PARITY_EVEN[] = {'E', 'V', 'E', 'N', 0};
static const WCHAR DPNA_PARITY_ODD[] = {'O', 'D', 'D', 0};
static const WCHAR DPNA_PARITY_MARK[] = {'M', 'A', 'R', 'K', 0};
static const WCHAR DPNA_PARITY_SPACE[] = {'S', 'P', 'A', 'C', 'E', 0};

static const WCHAR DPNA_VALUE_TCPIPPROVIDER[] = {'I', 'P', 0};
static const WCHAR DPNA_VALUE_IPXPROVIDER[] = {'I', 'P', 'X', 0};
static const WCHAR DPNA_VALUE_MODEMPROVIDER[] = {'M', 'O', 'D', 'E', 'M', 0};
static const WCHAR DPNA_VALUE_SERIALPROVIDER[] = {'S', 'E', 'R', 'I', 'A', 'L', 0};


#else /* defined(__WINE__) */

/* stop bits */
#define DPNA_STOP_BITS_ONE					L"1"
#define DPNA_STOP_BITS_ONE_FIVE				L"1.5"
#define DPNA_STOP_BITS_TWO					L"2"

/* parity */
#define DPNA_PARITY_NONE					L"NONE"
#define DPNA_PARITY_EVEN					L"EVEN"
#define DPNA_PARITY_ODD						L"ODD"
#define DPNA_PARITY_MARK					L"MARK"
#define DPNA_PARITY_SPACE					L"SPACE"

/* flow control */
#define DPNA_FLOW_CONTROL_NONE				L"NONE"
#define DPNA_FLOW_CONTROL_XONXOFF			L"XONXOFF"
#define DPNA_FLOW_CONTROL_RTS				L"RTS"
#define DPNA_FLOW_CONTROL_DTR				L"DTR"
#define DPNA_FLOW_CONTROL_RTSDTR			L"RTSDTR"

/* not really sure what these do, I think used instead of CLSID_... */
#define DPNA_VALUE_TCPIPPROVIDER            L"IP"
#define DPNA_VALUE_IPXPROVIDER              L"IPX"
#define DPNA_VALUE_MODEMPROVIDER            L"MODEM"
#define DPNA_VALUE_SERIALPROVIDER           L"SERIAL"

#endif /* defined(__WINE__) */

/* ANSI stuff of the above (non wchar) */

/* header */
#define DPNA_HEADER_A						"x-directplay:/"
#define DPNA_SEPARATOR_KEYVALUE_A			'='
#define DPNA_SEPARATOR_USERDATA_A			'#'
#define DPNA_SEPARATOR_COMPONENT_A			';'
#define DPNA_ESCAPECHAR_A					'%'

/* address components */
#define DPNA_KEY_APPLICATION_INSTANCE_A		"applicationinstance"
#define DPNA_KEY_BAUD_A						"baud"
#define DPNA_KEY_DEVICE_A					"device"
#define DPNA_KEY_FLOWCONTROL_A				"flowcontrol"
#define DPNA_KEY_HOSTNAME_A					"hostname"
#define DPNA_KEY_PARITY_A					"parity"
#define DPNA_KEY_PHONENUMBER_A				"phonenumber"
#define DPNA_KEY_PORT_A						"port"
#define DPNA_KEY_PROGRAM_A					"program"
#define DPNA_KEY_PROVIDER_A					"provider"
#define DPNA_KEY_STOPBITS_A					"stopbits"

/* stop bits */
#define DPNA_STOP_BITS_ONE_A				"1"
#define DPNA_STOP_BITS_ONE_FIVE_A			"1.5"
#define DPNA_STOP_BITS_TWO_A				"2"

/* parity */
#define DPNA_PARITY_NONE_A					"NONE"
#define DPNA_PARITY_EVEN_A					"EVEN"
#define DPNA_PARITY_ODD_A					"ODD"
#define DPNA_PARITY_MARK_A					"MARK"
#define DPNA_PARITY_SPACE_A					"SPACE"

/* flow control */
#define DPNA_FLOW_CONTROL_NONE_A			"NONE"
#define DPNA_FLOW_CONTROL_XONXOFF_A 		"XONXOFF"
#define DPNA_FLOW_CONTROL_RTS_A				"RTS"
#define DPNA_FLOW_CONTROL_DTR_A				"DTR"
#define DPNA_FLOW_CONTROL_RTSDTR_A			"RTSDTR"

/* alternatives.. */
#define DPNA_VALUE_TCPIPPROVIDER_A          "IP"
#define DPNA_VALUE_IPXPROVIDER_A            "IPX"
#define DPNA_VALUE_MODEMPROVIDER_A          "MODEM"
#define DPNA_VALUE_SERIALPROVIDER_A         "SERIAL"

/* functions */

/*
 * no longer supported function
 * HRESULT WINAPI DirectPlay8AddressCreate( const GUID * pcIID, void **ppvInterface, IUnknown *pUnknown);
 *
 */

/********** Interfaces ***********/

/* IDirectPlay8Address interface */
#define ICOM_INTERFACE IDirectPlay8Address
#define IDirectPlay8Address_METHODS \
    ICOM_METHOD1(HRESULT,BuildFromURLW,            WCHAR *, pwszSourceURL ) \
    ICOM_METHOD1(HRESULT,BuildFromURLA,            CHAR *, pszSourceURL ) \
    ICOM_METHOD1(HRESULT,Duplicate,                PDIRECTPLAY8ADDRESS *, ppdpaNewAddress ) \
    ICOM_METHOD1(HRESULT,SetEqual,                 PDIRECTPLAY8ADDRESS, pdpaAddress ) \
    ICOM_METHOD1(HRESULT,IsEqual,                  PDIRECTPLAY8ADDRESS, pdpaAddress ) \
    ICOM_METHOD (HRESULT,Clear) \
    ICOM_METHOD2(HRESULT,GetURLW,                  WCHAR *, pwszURL, PDWORD, pdwNumChars ) \
    ICOM_METHOD2(HRESULT,GetURLA,                  CHAR *, pszURL, PDWORD, pdwNumChars) \
    ICOM_METHOD1(HRESULT,GetSP,                    GUID *, pguidSP ) \
    ICOM_METHOD2(HRESULT,GetUserData,              void *, pvUserData, PDWORD, pdwBufferSize) \
    ICOM_METHOD1(HRESULT,SetSP,                    const GUID * const, pguidSP ) \
    ICOM_METHOD2(HRESULT,SetUserData,              const void * const, pvUserData, const DWORD, dwDataSize) \
    ICOM_METHOD1(HRESULT,GetNumComponents,         PDWORD, pdwNumComponents ) \
    ICOM_METHOD4(HRESULT,GetComponentByName,       const WCHAR * const, pwszName, void *, pvBuffer, PDWORD, pdwBufferSize, PDWORD, pdwDataType ) \
    ICOM_METHOD6(HRESULT,GetComponentByIndex,      const DWORD, dwComponentID, WCHAR *, pwszName, PDWORD, pdwNameLen, void *, pvBuffer, PDWORD, pdwBufferSize, PDWORD, pdwDataType ) \
    ICOM_METHOD4(HRESULT,AddComponent,             const WCHAR * const, pwszName, const void * const, lpvData, const DWORD, dwDataSize, const DWORD, dwDataType ) \
    ICOM_METHOD1(HRESULT,GetDevice,                GUID *, ) \
    ICOM_METHOD1(HRESULT,SetDevice,                const GUID * const,) \
    ICOM_METHOD2(HRESULT,BuildFromDPADDRESS,       LPVOID, pvAddress, DWORD, dwDataSize )
#define IDirectPlay8Address_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8Address_METHODS
ICOM_DEFINE(IDirectPlay8Address, IUnknown)
#undef ICOM_INTERFACE

  /* IUnknown methods */
#define IDirectPlay8Address_QueryInterface(p,a,b)                   ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectPlay8Address_AddRef(p)                               ICOM_CALL (AddRef,p)
#define IDirectPlay8Address_Release(p)                              ICOM_CALL (Release,p)
  /* IDirectPlay8Address methods */
#define IDirectPlay8Address_BuildFromURLW(p,a)                      ICOM_CALL1(BuildFromURLW,p,a)
#define IDirectPlay8Address_BuildFromURLA(p,a)                      ICOM_CALL1(BuildFromURLA,p,a)
#define IDirectPlay8Address_Duplicate(p,a)                          ICOM_CALL1(Duplicate,p,a)
#define IDirectPlay8Address_SetEqual(p,a)                           ICOM_CALL1(SetEqual,p,a)
#define IDirectPlay8Address_IsEqual(p,a)                            ICOM_CALL1(IsEqual,p,a)
#define IDirectPlay8Address_Clear(p)                                ICOM_CALL (Clear,p)
#define IDirectPlay8Address_GetURLW(p,a,b)                          ICOM_CALL2(GetURLW,p,a,b)
#define IDirectPlay8Address_GetURLA(p,a,b)                          ICOM_CALL2(GetURLA,p,a,b)
#define IDirectPlay8Address_GetSP(p,a)                              ICOM_CALL1(GetSP,p,a)
#define IDirectPlay8Address_GetUserData(p,a,b)                      ICOM_CALL2(GetUserData,p,a,b)
#define IDirectPlay8Address_SetSP(p,a)                              ICOM_CALL1(SetSP,p,a)
#define IDirectPlay8Address_SetUserData(p,a,b)                      ICOM_CALL2(SetUserData,p,a,b)
#define IDirectPlay8Address_GetNumComponents(p,a)                   ICOM_CALL1(GetNumComponents,p,a)
#define IDirectPlay8Address_GetComponentByName(p,a,b,c,d)           ICOM_CALL4(GetComponentByName,p,a,b,c,d)
#define IDirectPlay8Address_GetComponentByIndex(p,a,b,c,d,e,f)      ICOM_CALL6(GetComponentByIndex,p,a,b,c,d,e,f)
#define IDirectPlay8Address_AddComponent(p,a,b,c,d)                 ICOM_CALL4(AddComponent,p,a,b,c,d)
#define IDirectPlay8Address_SetDevice(p,a)                          ICOM_CALL1(SetDevice,p,a)
#define IDirectPlay8Address_GetDevice(p,a)                          ICOM_CALL1(GetDevice,p,a)
#define IDirectPlay8Address_BuildFromDirectPlay4Address(p,a,b)      ICOM_CALL2(BuildFromDirectPlay4Address,p,a,b)

/* IDirectPlay8AddressIP interface */
#define ICOM_INTERFACE IDirectPlay8AddressIP
#define IDirectPlay8AddressIP_METHODS \
    ICOM_METHOD1(HRESULT,BuildFromSockAddr,      const SOCKADDR * const, ) \
    ICOM_METHOD2(HRESULT,BuildAddress,           const WCHAR * const, wszAddress, const USHORT, usPort ) \
    ICOM_METHOD2(HRESULT,BuildLocalAddress,      const GUID * const, pguidAdapter, const USHORT, usPort ) \
    ICOM_METHOD2(HRESULT,GetSockAddress,         SOCKADDR *,, PDWORD, ) \
    ICOM_METHOD2(HRESULT,GetLocalAddress,        GUID *, pguidAdapter, USHORT*, pusPort ) \
    ICOM_METHOD3(HRESULT,GetAddress,             WCHAR *, wszAddress, PDWORD, pdwAddressLength, USHORT *, psPort )
#define IDirectPlay8AddressIP_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8AddressIP_METHODS
ICOM_DEFINE(IDirectPlay8AddressIP, IUnknown)
#undef ICOM_INTERFACE

  /* IUnknown methods */
#define IDirectPlay8AddressIP_QueryInterface(p,a,b)        ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectPlay8AddressIP_AddRef(p)                    ICOM_CALL (AddRef,p)
#define IDirectPlay8AddressIP_Release(p)                   ICOM_CALL (Release,p)
  /* IDirectPlay8AddressIP methods */
#define IDirectPlay8AddressIP_BuildFromSockAddr(p,a)       ICOM_CALL1(BuildFromSockAddr,p,a)
#define IDirectPlay8AddressIP_BuildAddress(p,a,b)          ICOM_CALL2(BuildAddress,p,a,b)
#define IDirectPlay8AddressIP_BuildLocalAddress(p,a,b)     ICOM_CALL2(BuildLocalAddress,p,a,b)
#define IDirectPlay8AddressIP_GetSockAddress(p,a,b)        ICOM_CALL2(GetSockAddress,p,a,b)
#define IDirectPlay8AddressIP_GetLocalAddress(p,a,b)       ICOM_CALL2(GetLocalAddress,p,a,b)
#define IDirectPlay8AddressIP_GetAddress(p,a,b,c)          ICOM_CALL3(GetAddress,p,a,b,c)

#ifdef __cplusplus
}
#endif

#endif


