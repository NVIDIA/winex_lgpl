#ifndef	__WINE_DPLOBBY8_H__
#define	__WINE_DPLOBBY8_H__

#include "ole2.h"

#include "dplay8.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CLSIDs for DirectPlay8 lobby classes */

/* {667955AD-6B3B-43ca-B949-BC69B5BAFF7F} */
DEFINE_GUID(CLSID_DirectPlay8LobbiedApplication, 
0x667955ad, 0x6b3b, 0x43ca, 0xb9, 0x49, 0xbc, 0x69, 0xb5, 0xba, 0xff, 0x7f);

/* {3B2B6775-70B6-45af-8DEA-A209C69559F3} */
DEFINE_GUID(CLSID_DirectPlay8LobbyClient, 
0x3b2b6775, 0x70b6, 0x45af, 0x8d, 0xea, 0xa2, 0x9, 0xc6, 0x95, 0x59, 0xf3);

/* IIDs for DirectPlay8 lobby interfaces */

/* {819074A3-016C-11d3-AE14-006097B01411} */
DEFINE_GUID(IID_IDirectPlay8LobbiedApplication,
0x819074a3, 0x16c, 0x11d3, 0xae, 0x14, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* {819074A2-016C-11d3-AE14-006097B01411} */
DEFINE_GUID(IID_IDirectPlay8LobbyClient,
0x819074a2, 0x16c, 0x11d3, 0xae, 0x14, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* interface pointers */

typedef struct IDirectPlay8LobbiedApplication    *PDIRECTPLAY8LOBBIEDAPPLICATION;
typedef struct IDirectPlay8LobbyClient           IDirectPlay8LobbyClient, *PDIRECTPLAY8LOBBYCLIENT;

/* message identifiers */

#define	DPL_MSGID_LOBBY						0x8000
#define	DPL_MSGID_RECEIVE					(0x0001 | DPL_MSGID_LOBBY)
#define	DPL_MSGID_CONNECT					(0x0002 | DPL_MSGID_LOBBY)
#define DPL_MSGID_DISCONNECT				(0x0003 | DPL_MSGID_LOBBY)
#define	DPL_MSGID_SESSION_STATUS			(0x0004 | DPL_MSGID_LOBBY)
#define DPL_MSGID_CONNECTION_SETTINGS       (0x0005 | DPL_MSGID_LOBBY)

/* constants */

#define DPLHANDLE_ALLCONNECTIONS			0xFFFFFFFF
#define	DPLSESSION_CONNECTED				0x0001
#define	DPLSESSION_COULDNOTCONNECT			0x0002
#define	DPLSESSION_DISCONNECTED				0x0003
#define	DPLSESSION_TERMINATED				0x0004
#define DPLSESSION_HOSTMIGRATED				0x0005
#define DPLSESSION_HOSTMIGRATEDHERE			0x0006


/* Flags */

#define DPLAVAILABLE_ALLOWMULTIPLECONNECT   0x0001
#define	DPLCONNECT_LAUNCHNEW				0x0001
#define	DPLCONNECT_LAUNCHNOTFOUND			0x0002
#define DPLCONNECTSETTINGS_HOST             0x0001
#define DPLINITIALIZE_DISABLEPARAMVAL		0x0001

/* non message structs */

/* app / game info? */
typedef struct _DPL_APPLICATION_INFO {
	GUID	guidApplication;
	PWSTR	pwszApplicationName;
	DWORD	dwNumRunning;
	DWORD	dwNumWaiting;
	DWORD	dwFlags;
} DPL_APPLICATION_INFO,  *PDPL_APPLICATION_INFO;

/* connection settings */
typedef struct _DPL_CONNECTION_SETTINGS {
    DWORD                   dwSize;
    DWORD                   dwFlags;
    DPN_APPLICATION_DESC    dpnAppDesc;
    IDirectPlay8Address     *pdp8HostAddress;
    IDirectPlay8Address     **ppdp8DeviceAddresses;
    DWORD                   cNumDeviceAddresses;
	PWSTR					pwszPlayerName;
} DPL_CONNECTION_SETTINGS, *PDPL_CONNECTION_SETTINGS;

/* connect info */
typedef struct _DPL_CONNECT_INFO {
	DWORD	                    dwSize;
	DWORD	                    dwFlags;
	GUID	                    guidApplication;
    PDPL_CONNECTION_SETTINGS	pdplConnectionSettings;
	PVOID	                    pvLobbyConnectData;
	DWORD	                    dwLobbyConnectDataSize;
} DPL_CONNECT_INFO,  *PDPL_CONNECT_INFO;

/* RegisterApplication info */
typedef struct  _DPL_PROGRAM_DESC {
	DWORD	dwSize;
	DWORD	dwFlags;
	GUID	guidApplication;
	PWSTR	pwszApplicationName;
	PWSTR	pwszCommandLine;
	PWSTR	pwszCurrentDirectory;
	PWSTR	pwszDescription;
	PWSTR	pwszExecutableFilename;
	PWSTR	pwszExecutablePath;
	PWSTR	pwszLauncherFilename;
	PWSTR	pwszLauncherPath;
} DPL_PROGRAM_DESC, *PDPL_PROGRAM_DESC;

/* message (callback?) structures */

/* connect message */
typedef struct _DPL_MESSAGE_CONNECT
{
	DWORD		                dwSize;
	DPNHANDLE	                hConnectId;
    PDPL_CONNECTION_SETTINGS	pdplConnectionSettings;
	PVOID		                pvLobbyConnectData;
	DWORD		                dwLobbyConnectDataSize;
	PVOID						pvConnectionContext;
} DPL_MESSAGE_CONNECT, *PDPL_MESSAGE_CONNECT;

/* connection settings updated */
typedef struct _DPL_MESSAGE_CONNECTION_SETTINGS
{
    DWORD                       dwSize;
    DPNHANDLE                   hSender;
    PDPL_CONNECTION_SETTINGS    pdplConnectionSettings;
	PVOID					    pvConnectionContext;
} DPL_MESSAGE_CONNECTION_SETTINGS, *PDPL_MESSAGE_CONNECTION_SETTINGS;

/* disconnection */
typedef struct _DPL_MESSAGE_DISCONNECT
{
	DWORD		dwSize;
	DPNHANDLE	hDisconnectId;
	HRESULT     hrReason;
	PVOID		pvConnectionContext;
} DPL_MESSAGE_DISCONNECT, *PDPL_MESSAGE_DISCONNECT;

/* message recieved */
typedef struct _DPL_MESSAGE_RECEIVE
{
	DWORD		dwSize;
	DPNHANDLE	hSender;
	BYTE		*pBuffer;
	DWORD		dwBufferSize;
	PVOID		pvConnectionContext;
} DPL_MESSAGE_RECEIVE, *PDPL_MESSAGE_RECEIVE;

/* session status */
typedef struct _DPL_MESSAGE_SESSION_STATUS
{
	DWORD		dwSize;
	DPNHANDLE	hSender;
	DWORD		dwStatus;
	PVOID		pvConnectionContext;
} DPL_MESSAGE_SESSION_STATUS, *PDPL_MESSAGE_SESSION_STATUS;

/* DirectPlay8LobbyCreate, dont think this should be used anymore */
extern HRESULT WINAPI DirectPlay8LobbyCreate( const GUID * pcIID, void **ppvInterface, IUnknown *pUnknown);

/******** Interfaces */

/* IDirectPlay8LobbyClient interface */
#define ICOM_INTERFACE IDirectPlay8LobbyClient
#define IDirectPlay8LobbyClient_METHODS \
    ICOM_METHOD3(HRESULT,Initialize,              const PVOID, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,EnumLocalPrograms,       GUID *const, pGuidApplication, BYTE *const, pEnumData, DWORD *const, pdwEnumData, DWORD *const, pdwItems, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,ConnectApplication,      DPL_CONNECT_INFO *const, pdplConnectionInfo, const PVOID, pvConnectionContext, DPNHANDLE *const, hApplication, const DWORD, dwTimeOut, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,Send,                    const DPNHANDLE, hConnection, BYTE *const, pBuffer, const DWORD, pBufferSize, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,ReleaseApplication,      const DPNHANDLE, hConnection, const DWORD, dwFlags ) \
    ICOM_METHOD1(HRESULT,Close,                   const DWORD, dwFlags ) \
    ICOM_METHOD4(HRESULT,GetConnectionSettings,   const DPNHANDLE, hConnection, DPL_CONNECTION_SETTINGS * const, pdplSessionInfo, DWORD, *pdwInfoSize, const DWORD, dwFlags ) \
    ICOM_METHOD3(HRESULT,SetConnectionSettings,   const DPNHANDLE, hConnection, const DPL_CONNECTION_SETTINGS * const, pdplSessionInfo, const DWORD, dwFlags )
#define IDirectPlay8LobbyClient_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8LobbyClient_METHODS
ICOM_DEFINE(IDirectPlay8LobbyClient, IUnknown)
#undef ICOM_INTERFACE

  /* IUnknown methods */
#define IDirectPlay8LobbyClient_QueryInterface(p,a,b)                ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectPlay8LobbyClient_AddRef(p)                            ICOM_CALL (AddRef,p)
#define IDirectPlay8LobbyClient_Release(p)                           ICOM_CALL (Release,p)
  /* IDirectPlay8LobbyClient methods */
#define IDirectPlay8LobbyClient_Initialize(p,a,b,c)                  ICOM_CALL3(Initialize,p,a,b,c)
#define IDirectPlay8LobbyClient_EnumLocalPrograms(p,a,b,c,d,e)       ICOM_CALL5(EnumLocalPrograms,p,a,b,c,d,e)
#define IDirectPlay8LobbyClient_ConnectApplication(p,a,b,c,d,e)      ICOM_CALL5(ConnectApplication,p,a,b,c,d,e)
#define IDirectPlay8LobbyClient_Send(p,a,b,c,d)                      ICOM_CALL4(Send,p,a,b,c,d)
#define IDirectPlay8LobbyClient_ReleaseApplication(p,a,b)            ICOM_CALL2(ReleaseApplication,p,a,b)
#define IDirectPlay8LobbyClient_Close(p,a)                           ICOM_CALL1(Close,p,a)
#define IDirectPlay8LobbyClient_GetConnectionSettings(p,a,b,c,d)     ICOM_CALL4(GetConnectionSettings,p,a,b,c,d)
#define IDirectPlay8LobbyClient_SetConnectionSettings(p,a,b,c)       ICOM_CALL3(SetConnectionSettings,p,a,b,c)

/* IDirectPlayLobbiedApplication interaface */
#define ICOM_INTERFACE IDirectPlay8LobbiedApplication
#define IDirectPlay8LobbiedApplication_METHODS \
    ICOM_METHOD4(HRESULT,Initialize,               const PVOID, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, DPNHANDLE * const, pdpnhConnection, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,RegisterProgram,          PDPL_PROGRAM_DESC, pdplProgramDesc, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,UnRegisterProgram,        GUID, *pguidApplication, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,Send,                     const DPNHANDLE, hConnection, BYTE *const, pBuffer, const DWORD, pBufferSize, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,SetAppAvailable,          const BOOL, fAvailable, const DWORD, dwFlags ) \
    ICOM_METHOD3(HRESULT,UpdateStatus,             const DPNHANDLE, hConnection, const DWORD, dwStatus, const DWORD, dwFlags ) \
    ICOM_METHOD1(HRESULT,Close,                    const DWORD, dwFlags ) \
    ICOM_METHOD4(HRESULT,GetConnectionSettings,    const DPNHANDLE, hConnection, DPL_CONNECTION_SETTINGS * const, pdplSessionInfo, DWORD, *pdwInfoSize, const DWORD, dwFlags ) \
    ICOM_METHOD3(HRESULT,SetConnectionSettings,    const DPNHANDLE, hConnection, const DPL_CONNECTION_SETTINGS * const, pdplSessionInfo, const DWORD, dwFlags )
#define IDirectPlay8LobbiedApplication_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8LobbiedApplication_METHODS
ICOM_DEFINE(IDirectPlay8LobbiedApplication, IUnknown)
#undef ICOM_INTERFACE

  /* IUnknown methods */
#define IDirectPlay8LobbiedApplication_QueryInterface(p,a,b)               ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectPlay8LobbiedApplication_AddRef(p)                           ICOM_CALL (AddRef,p)
#define IDirectPlay8LobbiedApplication_Release(p)                          ICOM_CALL (Release,p)
  /* IDirectPlay8LobbiedApplication methods */
#define IDirectPlay8LobbiedApplication_Initialize(p,a,b,c,d)               ICOM_CALL4(Initialize,p,a,b,c,d)
#define IDirectPlay8LobbiedApplication_RegisterProgram(p,a,b)              ICOM_CALL2(RegisterProgram,p,a,b)
#define IDirectPlay8LobbiedApplication_UnRegisterProgram(p,a,b)            ICOM_CALL2(UnRegisterProgram,p,a,b)
#define IDirectPlay8LobbiedApplication_Send(p,a,b,c,d)                     ICOM_CALL4(Send,p,a,b,c,d)
#define IDirectPlay8LobbiedApplication_SetAppAvailable(p,a,b)              ICOM_CALL2(SetAppAvailable,p,a,b)
#define IDirectPlay8LobbiedApplication_UpdateStatus(p,a,b,c)               ICOM_CALL3(UpdateStatus,p,a,b,c)
#define IDirectPlay8LobbiedApplication_Close(p,a)                          ICOM_CALL1(Close,p,a)
#define IDirectPlay8LobbiedApplication_GetConnectionSettings(p,a,b,c,d)    ICOM_CALL4(GetConnectionSettings,p,a,b,c,d)
#define IDirectPlay8LobbiedApplication_SetConnectionSettings(p,a,b,c)      ICOM_CALL3(SetConnectionSettings,p,a,b,c)

#ifdef __cplusplus
}
#endif

#endif	// __DPLOBBY_H__

