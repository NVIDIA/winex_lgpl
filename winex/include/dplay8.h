#ifndef __WINE_DPLAY8_H__
#define __WINE_DPLAY8_H__

#include "ole2.h"

#include "dpaddr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CLSIDs for "main" DirectPlay8 classes */

/* {743F1DC6-5ABA-429f-8BDF-C54D03253DC2} */
DEFINE_GUID(CLSID_DirectPlay8Client,
0x743f1dc6, 0x5aba, 0x429f, 0x8b, 0xdf, 0xc5, 0x4d, 0x3, 0x25, 0x3d, 0xc2);

/* {286F484D-375E-4458-A272-B138E2F80A6A} */
DEFINE_GUID(CLSID_DirectPlay8Peer,
0x286f484d, 0x375e, 0x4458, 0xa2, 0x72, 0xb1, 0x38, 0xe2, 0xf8, 0xa, 0x6a);

/* {DA825E1B-6830-43d7-835D-0B5AD82956A2} */
DEFINE_GUID(CLSID_DirectPlay8Server,
0xda825e1b, 0x6830, 0x43d7, 0x83, 0x5d, 0xb, 0x5a, 0xd8, 0x29, 0x56, 0xa2);

/* DX9 */
/* {FC47060E-6153-4b34-B975-8E4121EB7F3C} */
DEFINE_GUID(CLSID_DirectPlay8ThreadPool,
0xfc47060e, 0x6153, 0x4b34, 0xb9, 0x75, 0x8e, 0x41, 0x21, 0xeb, 0x7f, 0x3c);

/* {E4C1D9A2-CBF7-48bd-9A69-34A55E0D8941} */
DEFINE_GUID(CLSID_DirectPlay8NATResolver,
0xe4c1d9a2, 0xcbf7, 0x48bd, 0x9a, 0x69, 0x34, 0xa5, 0x5e, 0xd, 0x89, 0x41);

/* IIDs for "main" DirectPlay8 interfaces */

/* {5102DACD-241B-11d3-AEA7-006097B01411} */
DEFINE_GUID(IID_IDirectPlay8Client,
0x5102dacd, 0x241b, 0x11d3, 0xae, 0xa7, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* {5102DACF-241B-11d3-AEA7-006097B01411} */
DEFINE_GUID(IID_IDirectPlay8Peer,
0x5102dacf, 0x241b, 0x11d3, 0xae, 0xa7, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* {5102DACE-241B-11d3-AEA7-006097B01411} */
DEFINE_GUID(IID_IDirectPlay8Server,
0x5102dace, 0x241b, 0x11d3, 0xae, 0xa7, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* DX9 */
/* {0D22EE73-4A46-4a0d-89B2-045B4D666425} */
DEFINE_GUID(IID_IDirectPlay8ThreadPool,
0xd22ee73, 0x4a46, 0x4a0d, 0x89, 0xb2, 0x4, 0x5b, 0x4d, 0x66, 0x64, 0x25);

/* {A9E213F2-9A60-486f-BF3B-53408B6D1CBB} */
DEFINE_GUID(IID_IDirectPlay8NATResolver,
0xa9e213f2, 0x9a60, 0x486f, 0xbf, 0x3b, 0x53, 0x40, 0x8b, 0x6d, 0x1c, 0xbb);

/* CLSIDs for DirectPlay8 service providers */

/* {53934290-628D-11D2-AE0F-006097B01411} */
DEFINE_GUID(CLSID_DP8SP_IPX, 0x53934290, 0x628d, 0x11d2, 0xae, 0xf, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* {6D4A3650-628D-11D2-AE0F-006097B01411} */
DEFINE_GUID(CLSID_DP8SP_MODEM, 0x6d4a3650, 0x628d, 0x11d2, 0xae, 0xf, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* {743B5D60-628D-11D2-AE0F-006097B01411} */
DEFINE_GUID(CLSID_DP8SP_SERIAL, 0x743b5d60, 0x628d, 0x11d2, 0xae, 0xf, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* {EBFE7BA0-628D-11D2-AE0F-006097B01411} */
DEFINE_GUID(CLSID_DP8SP_TCPIP, 0xebfe7ba0, 0x628d, 0x11d2, 0xae, 0xf, 0x0, 0x60, 0x97, 0xb0, 0x14, 0x11);

/* DX9 */
/* {995513AF-3027-4b9a-956E-C772B3F78006} */
DEFINE_GUID(CLSID_DP8SP_BLUETOOTH,
0x995513af, 0x3027, 0x4b9a, 0x95, 0x6e, 0xc7, 0x72, 0xb3, 0xf7, 0x80, 0x6);

/* interface pointers */

typedef	struct IDirectPlay8Peer      IDirectPlay8Peer, *PDIRECTPLAY8PEER;
typedef	struct IDirectPlay8Server    IDirectPlay8Server, *PDIRECTPLAY8SERVER;
typedef	struct IDirectPlay8Client    IDirectPlay8Client, *PDIRECTPLAY8CLIENT;

/* DX9 */
typedef struct IDirectPlay8ThreadPool  IDirectPlay8ThreadPool, *PDIRECTPLAY8THREADPOOL;
typedef struct IDirectPlay8NATResolver IDirectPlay8NATResolver, *PDIRECTPLAY8NATRESOLVER;

/* external types */

typedef struct IDirectPlay8LobbiedApplication	IDirectPlay8LobbiedApplication, *PDNLOBBIEDAPPLICATION;

/* callback functions */

typedef HRESULT (WINAPI *PFNDPNMESSAGEHANDLER)(PVOID,DWORD,PVOID);

/* non structure/message datatypes */
typedef DWORD	DPNID,      *PDPNID;

typedef	DWORD	DPNHANDLE, *PDPNHANDLE;

/* message identifiers */

#define	DPN_MSGID_OFFSET					0xFFFF0000
#define DPN_MSGID_ADD_PLAYER_TO_GROUP		( DPN_MSGID_OFFSET | 0x0001 )
#define DPN_MSGID_APPLICATION_DESC			( DPN_MSGID_OFFSET | 0x0002 )
#define DPN_MSGID_ASYNC_OP_COMPLETE			( DPN_MSGID_OFFSET | 0x0003 )
#define DPN_MSGID_CLIENT_INFO				( DPN_MSGID_OFFSET | 0x0004 )
#define DPN_MSGID_CONNECT_COMPLETE			( DPN_MSGID_OFFSET | 0x0005 )
#define DPN_MSGID_CREATE_GROUP				( DPN_MSGID_OFFSET | 0x0006 )
#define DPN_MSGID_CREATE_PLAYER				( DPN_MSGID_OFFSET | 0x0007 )
#define DPN_MSGID_DESTROY_GROUP				( DPN_MSGID_OFFSET | 0x0008 )
#define DPN_MSGID_DESTROY_PLAYER			( DPN_MSGID_OFFSET | 0x0009 )
#define DPN_MSGID_ENUM_HOSTS_QUERY			( DPN_MSGID_OFFSET | 0x000a )
#define DPN_MSGID_ENUM_HOSTS_RESPONSE		( DPN_MSGID_OFFSET | 0x000b )
#define DPN_MSGID_GROUP_INFO				( DPN_MSGID_OFFSET | 0x000c )
#define DPN_MSGID_HOST_MIGRATE				( DPN_MSGID_OFFSET | 0x000d )
#define DPN_MSGID_INDICATE_CONNECT			( DPN_MSGID_OFFSET | 0x000e )
#define DPN_MSGID_INDICATED_CONNECT_ABORTED	( DPN_MSGID_OFFSET | 0x000f )
#define DPN_MSGID_PEER_INFO					( DPN_MSGID_OFFSET | 0x0010 )
#define DPN_MSGID_RECEIVE					( DPN_MSGID_OFFSET | 0x0011 )
#define DPN_MSGID_REMOVE_PLAYER_FROM_GROUP	( DPN_MSGID_OFFSET | 0x0012 )
#define	DPN_MSGID_RETURN_BUFFER				( DPN_MSGID_OFFSET | 0x0013 )
#define DPN_MSGID_SEND_COMPLETE				( DPN_MSGID_OFFSET | 0x0014 )
#define DPN_MSGID_SERVER_INFO				( DPN_MSGID_OFFSET | 0x0015 )
#define	DPN_MSGID_TERMINATE_SESSION			( DPN_MSGID_OFFSET | 0x0016 )
/* DX9 */
#define DPN_MSGID_CREATE_THREAD				( DPN_MSGID_OFFSET | 0x0017 )
#define DPN_MSGID_DESTROY_THREAD			( DPN_MSGID_OFFSET | 0x0018 )
#define DPN_MSGID_NAT_RESOLVER_QUERY		( DPN_MSGID_OFFSET | 0x0101 )

/* constants */

#define	DPNID_ALL_PLAYERS_GROUP				0

/* reasons for DESTROY_GROUP */
#define	DPNDESTROYGROUPREASON_NORMAL				0x0001
#define DPNDESTROYGROUPREASON_AUTODESTRUCTED		0x0002
#define	DPNDESTROYGROUPREASON_SESSIONTERMINATED		0x0003

/* reasons for DESTROY_PLAYER */
#define DPNDESTROYPLAYERREASON_NORMAL				0x0001
#define DPNDESTROYPLAYERREASON_CONNECTIONLOST		0x0002
#define	DPNDESTROYPLAYERREASON_SESSIONTERMINATED	0x0003
#define	DPNDESTROYPLAYERREASON_HOSTDESTROYEDPLAYER	0x0004

/* Flags */

/* async operations */
#define DPNOP_SYNC							0x80000000

/* AddPlayerToGroup flags */
#define DPNADDPLAYERTOGROUP_SYNC			DPNOP_SYNC

/* cancel flags */
#define	DPNCANCEL_CONNECT					0x0001
#define	DPNCANCEL_ENUM						0x0002
#define	DPNCANCEL_SEND						0x0004
#define	DPNCANCEL_ALL_OPERATIONS			0x8000
/* DX9 */
#define DPNCANCEL_PLAYER_SENDS					0x80000000
#define DPNCANCEL_PLAYER_SENDS_PRIORITY_HIGH	(DPNCANCEL_PLAYER_SENDS | 0x00010000)
#define DPNCANCEL_PLAYER_SENDS_PRIORITY_NORMAL	(DPNCANCEL_PLAYER_SENDS | 0x00020000)
#define DPNCANCEL_PLAYER_SENDS_PRIORITY_LOW		(DPNCANCEL_PLAYER_SENDS | 0x00040000)

/* close flags (added in DX9) */
#define DPNCLOSE_IMMEDIATE						0x00000001

/* connection flags */
#define	DPNCONNECT_SYNC						DPNOP_SYNC
#define	DPNCONNECT_OKTOQUERYFORADDRESSING	0x0001

/* craete group flags */
#define	DPNCREATEGROUP_SYNC					DPNOP_SYNC

/* destroy group flags */
#define	DPNDESTROYGROUP_SYNC				DPNOP_SYNC

/* enum players and groups flags */
#define DPNENUM_PLAYERS						0x0001
#define DPNENUM_GROUPS						0x0010

/* enum host flags */
#define	DPNENUMHOSTS_SYNC					DPNOP_SYNC
#define	DPNENUMHOSTS_OKTOQUERYFORADDRESSING	0x0001
#define	DPNENUMHOSTS_NOBROADCASTFALLBACK	0x0002

/* enum service provider flags */
#define DPNENUMSERVICEPROVIDERS_ALL			0x0001

/* GetLocalHostAddress flags (added in DX9) */
#define DPNGETLOCALHOSTADDRESSES_COMBINED		0x0001

/* get send queue info flags */
#define	DPNGETSENDQUEUEINFO_PRIORITY_NORMAL	0x0001
#define	DPNGETSENDQUEUEINFO_PRIORITY_HIGH	0x0002
#define	DPNGETSENDQUEUEINFO_PRIORITY_LOW	0x0004

/* group info flags */
#define DPNGROUP_AUTODESTRUCT				0x0001

/* host flags */
#define	DPNHOST_OKTOQUERYFORADDRESSING		0x0001

/* set info flags */
#define	DPNINFO_NAME						0x0001
#define	DPNINFO_DATA						0x0002

/* initialize flags */
#define DPNINITIALIZE_DISABLEPARAMVAL		0x0001
/* DX9 */
#define DPNINITIALIZE_HINT_LANSESSION			0x0002
#define DPNINITIALIZE_DISABLELINKTUNING			0x0004

/* register lobby flags */
#define	DPNLOBBY_REGISTER					0x0001
#define DPNLOBBY_UNREGISTER					0x0002

/* player info/msg flags */
#define	DPNPLAYER_LOCAL						0x0002
#define	DPNPLAYER_HOST						0x0004

/* recieved indication flags (added in DX9) */
#define DPNRECEIVE_GUARANTEED					0x0001
#define DPNRECEIVE_COALESCED					0x0002

/* flags for RemovePlayerFromGroup */
#define	DPNREMOVEPLAYERFROMGROUP_SYNC		DPNOP_SYNC

/* send flags */
#define DPNSEND_SYNC						DPNOP_SYNC
#define DPNSEND_NOCOPY						0x0001
#define DPNSEND_NOCOMPLETE					0x0002
#define DPNSEND_COMPLETEONPROCESS			0x0004
#define DPNSEND_GUARANTEED					0x0008
#define	DPNSEND_NONSEQUENTIAL				0x0010
#define DPNSEND_NOLOOPBACK					0x0020
#define	DPNSEND_PRIORITY_LOW				0x0040
#define	DPNSEND_PRIORITY_HIGH				0x0080
/* DX9 */
#define DPNSEND_COALESCE						0x0100

/* send complete indications flags (added in DX9) */
#define DPNSENDCOMPLETE_GUARANTEED				0x0001
#define DPNSENDCOMPLETE_COALESCED				0x0002

/* session flags */
#define DPNSESSION_CLIENT_SERVER			0x0001
#define DPNSESSION_MIGRATE_HOST				0x0004
#define DPNSESSION_NODPNSVR					0x0040
#define DPNSESSION_REQUIREPASSWORD          0x0080
/* DX9 */
#define DPNSESSION_NOENUMS						0x0100
#define DPNSESSION_FAST_SIGNED					0x0200
#define DPNSESSION_FULL_SIGNED					0x0400

/* flags for set client info */
#define DPNSETCLIENTINFO_SYNC				DPNOP_SYNC

/* set group info flags */
#define DPNSETGROUPINFO_SYNC				DPNOP_SYNC

/* set peer info flags */
#define DPNSETPEERINFO_SYNC					DPNOP_SYNC

/* set server info flags */
#define DPNSETSERVERINFO_SYNC				DPNOP_SYNC

/* serivce provider capabilitites flags */
#define	DPNSPCAPS_SUPPORTSDPNSRV			0x0001
#define	DPNSPCAPS_SUPPORTSBROADCAST			0x0002
#define	DPNSPCAPS_SUPPORTSALLADAPTERS		0x0004
/* DX9 */
#define DPNSPCAPS_SUPPORTSTHREADPOOL			0x0008
#define DPNSPCAPS_NETWORKSIMULATOR				0x0010

/* service provider information flags (added in DX9) */
#define DPNSPINFO_NETWORKSIMULATORDEVICE		0x0001

/* non message structures */

/* app desc */
typedef struct	_DPN_APPLICATION_DESC
{
	DWORD	dwSize;
	DWORD	dwFlags;
	GUID	guidInstance;
	GUID	guidApplication;
	DWORD	dwMaxPlayers;
	DWORD	dwCurrentPlayers;
	WCHAR	*pwszSessionName;
	WCHAR	*pwszPassword;
	PVOID	pvReservedData;					
	DWORD	dwReservedDataSize;
	PVOID	pvApplicationReservedData;
	DWORD	dwApplicationReservedDataSize;
} DPN_APPLICATION_DESC, *PDPN_APPLICATION_DESC;

/* buffer desc */
typedef struct	_BUFFERDESC
{
	DWORD	dwBufferSize;		
	BYTE * 	pBufferData;		
} BUFFERDESC, DPN_BUFFER_DESC, *PDPN_BUFFER_DESC;

typedef BUFFERDESC	 * PBUFFERDESC;

/* dplay8 caps */
typedef struct	_DPN_CAPS
{
    DWORD   dwSize;
	DWORD	dwFlags;
    DWORD   dwConnectTimeout;
    DWORD   dwConnectRetries;
    DWORD   dwTimeoutUntilKeepAlive;
} DPN_CAPS, *PDPN_CAPS;

/* extended caps (added in DX9) */
typedef struct	_DPN_CAPS_EX
{
    DWORD    dwSize;
    DWORD    dwFlags;
    DWORD    dwConnectTimeout;
    DWORD    dwConnectRetries;
    DWORD    dwTimeoutUntilKeepAlive;
    DWORD    dwMaxRecvMsgSize;
    DWORD    dwNumSendRetries;
    DWORD    dwMaxSendRetryInterval;
    DWORD    dwDropThresholdRate;
    DWORD    dwThrottleRate;
    DWORD    dwNumHardDisconnectSends;
    DWORD    dwMaxHardDisconnectPeriod;
} DPN_CAPS_EX, *PDPN_CAPS_EX;


/* connection statistics */
typedef struct _DPN_CONNECTION_INFO
{
    DWORD   dwSize;
    DWORD   dwRoundTripLatencyMS;
    DWORD   dwThroughputBPS;
    DWORD	dwPeakThroughputBPS;

	DWORD	dwBytesSentGuaranteed;
	DWORD	dwPacketsSentGuaranteed;
	DWORD	dwBytesSentNonGuaranteed;
	DWORD	dwPacketsSentNonGuaranteed;

	DWORD	dwBytesRetried;
	DWORD	dwPacketsRetried;
	DWORD	dwBytesDropped;
	DWORD	dwPacketsDropped;

	DWORD	dwMessagesTransmittedHighPriority;
	DWORD	dwMessagesTimedOutHighPriority;
	DWORD	dwMessagesTransmittedNormalPriority;
	DWORD	dwMessagesTimedOutNormalPriority;
	DWORD	dwMessagesTransmittedLowPriority;
	DWORD	dwMessagesTimedOutLowPriority;

	DWORD	dwBytesReceivedGuaranteed;
	DWORD	dwPacketsReceivedGuaranteed;
	DWORD	dwBytesReceivedNonGuaranteed;
	DWORD	dwPacketsReceivedNonGuaranteed;
	DWORD	dwMessagesReceived;

} DPN_CONNECTION_INFO, *PDPN_CONNECTION_INFO;


/* group info */
typedef struct	_DPN_GROUP_INFO
{
	DWORD	dwSize;
	DWORD	dwInfoFlags;
	PWSTR	pwszName;
	PVOID	pvData;
	DWORD	dwDataSize;
	DWORD	dwGroupFlags;
} DPN_GROUP_INFO, *PDPN_GROUP_INFO;

/* player info */
typedef struct	_DPN_PLAYER_INFO
{
	DWORD	dwSize;
	DWORD	dwInfoFlags;
	PWSTR	pwszName;
	PVOID	pvData;
	DWORD	dwDataSize;
	DWORD	dwPlayerFlags;
} DPN_PLAYER_INFO, *PDPN_PLAYER_INFO;

typedef struct _DPN_SECURITY_CREDENTIALS	DPN_SECURITY_CREDENTIALS, *PDPN_SECURITY_CREDENTIALS;
typedef struct _DPN_SECURITY_DESC			DPN_SECURITY_DESC, *PDPN_SECURITY_DESC;

/* service provider info (used for enum) */
typedef struct _DPN_SERVICE_PROVIDER_INFO
{
	DWORD		dwFlags;
	GUID		guid;
	WCHAR		*pwszName;
	PVOID		pvReserved;	
	DWORD		dwReserved;
} DPN_SERVICE_PROVIDER_INFO, *PDPN_SERVICE_PROVIDER_INFO;

/* service provider caps */
typedef struct _DPN_SP_CAPS
{
	DWORD   dwSize;
	DWORD   dwFlags;
	DWORD   dwNumThreads;
	DWORD	dwDefaultEnumCount;
	DWORD	dwDefaultEnumRetryInterval;
	DWORD	dwDefaultEnumTimeout;
	DWORD	dwMaxEnumPayloadSize;
	DWORD	dwBuffersPerThread;
	DWORD	dwSystemBufferSize;
} DPN_SP_CAPS, *PDPN_SP_CAPS;



/* mesasge callback structures */

/* add player to group */
typedef struct	_DPNMSG_ADD_PLAYER_TO_GROUP
{
	DWORD	dwSize;
	DPNID	dpnidGroup;
	PVOID	pvGroupContext;
	DPNID	dpnidPlayer;
	PVOID	pvPlayerContext;
} DPNMSG_ADD_PLAYER_TO_GROUP, *PDPNMSG_ADD_PLAYER_TO_GROUP;

/* async op completed */
typedef struct	_DPNMSG_ASYNC_OP_COMPLETE
{
	DWORD		dwSize;
	DPNHANDLE	hAsyncOp;
	PVOID		pvUserContext;
	HRESULT		hResultCode;
} DPNMSG_ASYNC_OP_COMPLETE, *PDPNMSG_ASYNC_OP_COMPLETE;

/* client info */
typedef struct	_DPNMSG_CLIENT_INFO
{
	DWORD	dwSize;
	DPNID	dpnidClient;
	PVOID	pvPlayerContext;
} DPNMSG_CLIENT_INFO, *PDPNMSG_CLIENT_INFO;

/* connect complete */
typedef struct	_DPNMSG_CONNECT_COMPLETE
{
	DWORD		dwSize;
	DPNHANDLE	hAsyncOp;
	PVOID		pvUserContext;
	HRESULT		hResultCode;
	PVOID		pvApplicationReplyData;
	DWORD		dwApplicationReplyDataSize;
	/* DX9 */
	DPNID		dpnidLocal;
} DPNMSG_CONNECT_COMPLETE, *PDPNMSG_CONNECT_COMPLETE;

/* create group */
typedef struct	_DPNMSG_CREATE_GROUP
{
	DWORD	dwSize;
	DPNID	dpnidGroup;
	DPNID	dpnidOwner;
	PVOID	pvGroupContext;
	/* DX9 */
	PVOID	pvOwnerContext;
} DPNMSG_CREATE_GROUP, *PDPNMSG_CREATE_GROUP;

/* create player */
typedef struct	_DPNMSG_CREATE_PLAYER
{
	DWORD	dwSize;
	DPNID	dpnidPlayer;
	PVOID	pvPlayerContext;
} DPNMSG_CREATE_PLAYER, *PDPNMSG_CREATE_PLAYER;

/* destroy group */
typedef struct	_DPNMSG_DESTROY_GROUP
{
	DWORD	dwSize;				// Size of this structure
	DPNID	dpnidGroup;			// DPNID of destroyed group
	PVOID	pvGroupContext;		// Group context value
	DWORD	dwReason;			// Information only
} DPNMSG_DESTROY_GROUP, *PDPNMSG_DESTROY_GROUP;

/* destroy player */
typedef struct	_DPNMSG_DESTROY_PLAYER
{
	DWORD	dwSize;
	DPNID	dpnidPlayer;
	PVOID	pvPlayerContext;
	DWORD	dwReason;
} DPNMSG_DESTROY_PLAYER, *PDPNMSG_DESTROY_PLAYER;

/* enum hosts request */
typedef	struct	_DPNMSG_ENUM_HOSTS_QUERY
{
	DWORD				dwSize;
	IDirectPlay8Address *pAddressSender;
	IDirectPlay8Address	*pAddressDevice;
	PVOID				pvReceivedData;
	DWORD				dwReceivedDataSize;
	DWORD				dwMaxResponseDataSize;
	PVOID				pvResponseData;
	DWORD				dwResponseDataSize;
	PVOID				pvResponseContext;
} DPNMSG_ENUM_HOSTS_QUERY, *PDPNMSG_ENUM_HOSTS_QUERY;

/* enum hosts response */
typedef	struct	_DPNMSG_ENUM_HOSTS_RESPONSE
{
	DWORD						dwSize;
	IDirectPlay8Address			*pAddressSender;
	IDirectPlay8Address			*pAddressDevice;
	DPN_APPLICATION_DESC	*pApplicationDescription;
	PVOID						pvResponseData;
	DWORD						dwResponseDataSize;
	PVOID						pvUserContext;
    DWORD						dwRoundTripLatencyMS;
} DPNMSG_ENUM_HOSTS_RESPONSE, *PDPNMSG_ENUM_HOSTS_RESPONSE;

/* group info */
typedef struct	_DPNMSG_GROUP_INFO
{
	DWORD	dwSize;
	DPNID	dpnidGroup;
	PVOID	pvGroupContext;
} DPNMSG_GROUP_INFO, *PDPNMSG_GROUP_INFO;

/* host migrate */
typedef struct	_DPNMSG_HOST_MIGRATE
{
	DWORD	dwSize;
	DPNID	dpnidNewHost;
	PVOID	pvPlayerContext;
} DPNMSG_HOST_MIGRATE, *PDPNMSG_HOST_MIGRATE;

/* indicate connect */
typedef struct	_DPNMSG_INDICATE_CONNECT
{
	DWORD		dwSize;
	PVOID		pvUserConnectData;
	DWORD		dwUserConnectDataSize;
	PVOID		pvReplyData;
	DWORD		dwReplyDataSize;
	PVOID		pvReplyContext;
	PVOID		pvPlayerContext;
	IDirectPlay8Address	*pAddressPlayer;
	IDirectPlay8Address	*pAddressDevice;
} DPNMSG_INDICATE_CONNECT, *PDPNMSG_INDICATE_CONNECT;

/* indicated connect aborted */
typedef struct	_DPNMSG_INDICATED_CONNECT_ABORTED
{
	DWORD		dwSize;
	PVOID		pvPlayerContext;
} DPNMSG_INDICATED_CONNECT_ABORTED, *PDPNMSG_INDICATED_CONNECT_ABORTED;

/* peer info */
typedef struct	_DPNMSG_PEER_INFO
{
	DWORD	dwSize;
	DPNID	dpnidPeer;
	PVOID	pvPlayerContext;
} DPNMSG_PEER_INFO, *PDPNMSG_PEER_INFO;

/* recieve */
typedef struct	_DPNMSG_RECEIVE
{
	DWORD		dwSize;
	DPNID		dpnidSender;
	PVOID		pvPlayerContext;
	PBYTE		pReceiveData;
	DWORD		dwReceiveDataSize;
	DPNHANDLE	hBufferHandle;
	/* DX9 */
	DWORD		dwReceiveFlags;
} DPNMSG_RECEIVE, *PDPNMSG_RECEIVE;

/* remove player from group */
typedef struct	_DPNMSG_REMOVE_PLAYER_FROM_GROUP
{
	DWORD	dwSize;
	DPNID	dpnidGroup;
	PVOID	pvGroupContext;
	DPNID	dpnidPlayer;
	PVOID	pvPlayerContext;
} DPNMSG_REMOVE_PLAYER_FROM_GROUP, *PDPNMSG_REMOVE_PLAYER_FROM_GROUP;

/* return buffer */
typedef struct	_DPNMSG_RETURN_BUFFER
{
	DWORD		dwSize;
	HRESULT		hResultCode;
	PVOID		pvBuffer;
	PVOID		pvUserContext;
} DPNMSG_RETURN_BUFFER, *PDPNMSG_RETURN_BUFFER;

/* send complete */
typedef struct	_DPNMSG_SEND_COMPLETE
{
	DWORD		dwSize;
	DPNHANDLE	hAsyncOp;
	PVOID		pvUserContext;
	HRESULT		hResultCode;
	DWORD		dwSendTime;
	/* DX9 */
	DWORD				dwFirstFrameRTT;
	DWORD				dwFirstFrameRetryCount;
	DWORD				dwSendCompleteFlags;
	DPN_BUFFER_DESC		*pBuffers;
	DWORD				dwNumBuffers;
} DPNMSG_SEND_COMPLETE, *PDPNMSG_SEND_COMPLETE;

/* server info */
typedef struct	_DPNMSG_SERVER_INFO
{
	DWORD	dwSize;
	DPNID	dpnidServer;
	PVOID	pvPlayerContext;
} DPNMSG_SERVER_INFO, *PDPNMSG_SERVER_INFO;

/* terminate session */
typedef struct	_DPNMSG_TERMINATE_SESSION
{
	DWORD		dwSize;
	HRESULT		hResultCode;
	PVOID		pvTerminateData;
	DWORD		dwTerminateDataSize;
} DPNMSG_TERMINATE_SESSION, *PDPNMSG_TERMINATE_SESSION;

/* DX9 */
/* create thread - DX9 */
typedef struct	_DPNMSG_CREATE_THREAD
{
	DWORD	dwSize;
	DWORD	dwFlags;
	DWORD	dwProcessorNum;
	PVOID	pvUserContext;
} DPNMSG_CREATE_THREAD, *PDPNMSG_CREATE_THREAD;

/* destroy thread - DX9 */
typedef struct	_DPNMSG_DESTROY_THREAD
{
	DWORD	dwSize;
	DWORD	dwProcessorNum;
	PVOID	pvUserContext;
} DPNMSG_DESTROY_THREAD, *PDPNMSG_DESTROY_THREAD;

/* nat resolver query - DX9 */
typedef	struct	_DPNMSG_NAT_RESOLVER_QUERY
{
	DWORD				dwSize;
	IDirectPlay8Address	*pAddressSender;
	IDirectPlay8Address	*pAddressDevice;
	WCHAR				*pwszUserString;
} DPNMSG_NAT_RESOLVER_QUERY, *PDPNMSG_NAT_RESOLVER_QUERY;

/* functions */

/*
 * no longer supported function
 * extern HRESULT WINAPI DirectPlay8Create( const GUID * pcIID, void **ppvInterface, IUnknown *pUnknown);
 * 
 */

/********** Interfaces **************/

/* IDirectPlay8Client interface */
#define ICOM_INTERFACE IDirectPlay8Client
#define IDirectPlay8Client_METHODS \
    ICOM_METHOD3(HRESULT,Initialize,                   PVOID const, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, const DWORD, dwFlags) \
    ICOM_METHOD6(HRESULT,EnumServiceProviders,        const GUID *const, pguidServiceProvider, const GUID *const, pguidApplication, DPN_SERVICE_PROVIDER_INFO *const, pSPInfoBuffer, PDWORD const, pcbEnumData, PDWORD const, pcReturned, const DWORD, dwFlags) \
    ICOM_METHOD11(HRESULT,EnumHosts,                  PDPN_APPLICATION_DESC const, pApplicationDesc, IDirectPlay8Address *const, pAddrHost, IDirectPlay8Address *const, pDeviceInfo, PVOID const, pUserEnumData, const DWORD, dwUserEnumDataSize, const DWORD, dwEnumCount, const DWORD, dwRetryInterval, const DWORD, dwTimeOut, PVOID const, pvUserContext, DPNHANDLE *const, pAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,CancelAsyncOperation,        const DPNHANDLE, hAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD10(HRESULT,Connect,                    const DPN_APPLICATION_DESC *const, pdnAppDesc, IDirectPlay8Address *const, pHostAddr, IDirectPlay8Address *const, pDeviceInfo, const DPN_SECURITY_DESC *const, pdnSecurity, const DPN_SECURITY_CREDENTIALS *const, pdnCredentials, const void *const, pvUserConnectData, const DWORD, dwUserConnectDataSize, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD6(HRESULT,Send,                        const DPN_BUFFER_DESC *const, prgBufferDesc, const DWORD, cBufferDesc, const DWORD, dwTimeOut, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetSendQueueInfo,            DWORD *const, pdwNumMsgs, DWORD *const, pdwNumBytes, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetApplicationDesc,          DPN_APPLICATION_DESC *const, pAppDescBuffer, DWORD *const, pcbDataSize, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,SetClientInfo,               const DPN_PLAYER_INFO *const, pdpnPlayerInfo, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetServerInfo,               DPN_PLAYER_INFO *const, pdpnPlayerInfo, DWORD *const, pdwSize, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,GetServerAddress,            IDirectPlay8Address **const, pAddress, const DWORD, dwFlags) \
    ICOM_METHOD1(HRESULT,Close,                       const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,ReturnBuffer,                const DPNHANDLE, hBufferHandle, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,GetCaps,                     DPN_CAPS *const, pdpCaps, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,SetCaps,                     const DPN_CAPS *const, pdpCaps, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,SetSPCaps,                   const GUID * const, pguidSP, const DPN_SP_CAPS *const, pdpspCaps, const DWORD, dwFlags ) \
    ICOM_METHOD3(HRESULT,GetSPCaps,                   const GUID * const, pguidSP, DPN_SP_CAPS *const, pdpspCaps, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,GetConnectionInfo,           DPN_CONNECTION_INFO *const, pdpConnectionInfo, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,RegisterLobby,               const DPNHANDLE, dpnHandle, struct IDirectPlay8LobbiedApplication *const, pIDP8LobbiedApplication, const DWORD, dwFlags)
#define IDirectPlay8Client_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8Client_METHODS
ICOM_DEFINE(IDirectPlay8Client,IUnknown)
#undef ICOM_INTERFACE

  /* IUnknown methods */
#define	IDirectPlay8Client_QueryInterface(p,a,b)                ICOM_CALL2(QueryInterface,p,a,b)
#define	IDirectPlay8Client_AddRef(p)                            ICOM_CALL (AddRef,p)
#define	IDirectPlay8Client_Release(p)                           ICOM_CALL (Release,p)
   /* IDirectPlay8Client methods */
#define	IDirectPlay8Client_Initialize(p,a,b,c)                  ICOM_CALL3(Initialize,p,a,b,c)
#define	IDirectPlay8Client_EnumServiceProviders(p,a,b,c,d,e,f)  ICOM_CALL6(EnumServiceProviders,p,a,b,c,d,e,f)
#define	IDirectPlay8Client_EnumHosts(p,a,b,c,d,e,f,g,h,i,j,k)   ICOM_CALL11(EnumHosts,p,a,b,c,d,e,f,g,h,i,j,k)
#define	IDirectPlay8Client_CancelAsyncOperation(p,a,b)          ICOM_CALL2(CancelAsyncOperation,p,a,b)
#define	IDirectPlay8Client_Connect(p,a,b,c,d,e,f,g,h,i,j)       ICOM_CALL10(Connect,p,a,b,c,d,e,f,g,h,i,j)
#define	IDirectPlay8Client_Send(p,a,b,c,d,e,f)                  ICOM_CALL6(Send,p,a,b,c,d,e,f)
#define	IDirectPlay8Client_GetSendQueueInfo(p,a,b,c)            ICOM_CALL3(GetSendQueueInfo,p,a,b,c)
#define	IDirectPlay8Client_GetApplicationDesc(p,a,b,c)          ICOM_CALL3(GetApplicationDesc,p,a,b,c)
#define	IDirectPlay8Client_SetClientInfo(p,a,b,c,d)             ICOM_CALL4(SetClientInfo,p,a,b,c,d)
#define	IDirectPlay8Client_GetServerInfo(p,a,b,c)               ICOM_CALL3(GetServerInfo,p,a,b,c)
#define	IDirectPlay8Client_GetServerAddress(p,a,b)              ICOM_CALL2(GetServerAddress,p,a,b)
#define	IDirectPlay8Client_Close(p,a)                           ICOM_CALL1(Close,p,a)
#define	IDirectPlay8Client_ReturnBuffer(p,a,b)                  ICOM_CALL2(ReturnBuffer,p,a,b)
#define	IDirectPlay8Client_GetCaps(p,a,b)                       ICOM_CALL2(GetCaps,p,a,b)
#define	IDirectPlay8Client_SetCaps(p,a,b)                       ICOM_CALL2(SetCaps,p,a,b)
#define	IDirectPlay8Client_SetSPCaps(p,a,b,c)                   ICOM_CALL3(SetSPCaps,p,a,b,c)
#define	IDirectPlay8Client_GetSPCaps(p,a,b,c)                   ICOM_CALL3(GetSPCaps,p,a,b,c)
#define	IDirectPlay8Client_GetConnectionInfo(p,a,b)             ICOM_CALL2(GetConnectionInfo,p,a,b)
#define	IDirectPlay8Client_RegisterLobby(p,a,b,c)               ICOM_CALL3(RegisterLobby,p,a,b,c)


/* IDirectPlay8Server interface */
#define ICOM_INTERFACE IDirectPlay8Server
#define IDirectPlay8Server_METHODS \
    ICOM_METHOD3(HRESULT,Initialize,             PVOID const, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, const DWORD, dwFlags) \
    ICOM_METHOD6(HRESULT,EnumServiceProviders,   const GUID *const, pguidServiceProvider, const GUID *const, pguidApplication, DPN_SERVICE_PROVIDER_INFO *const, pSPInfoBuffer, PDWORD const, pcbEnumData, PDWORD const, pcReturned, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,CancelAsyncOperation,   const DPNHANDLE, hAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,GetSendQueueInfo,       const DPNID, dpnid, DWORD *const, pdwNumMsgs, DWORD *const, pdwNumBytes, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetApplicationDesc,     DPN_APPLICATION_DESC *const, pAppDescBuffer, DWORD *const, pcbDataSize, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,SetServerInfo,          const DPN_PLAYER_INFO *const, pdpnPlayerInfo, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,GetClientInfo,          const DPNID, dpnid, DPN_PLAYER_INFO *const, pdpnPlayerInfo, DWORD *const, pdwSize, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetClientAddress,       const DPNID, dpnid, IDirectPlay8Address **const, pAddress, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetLocalHostAddresses,  IDirectPlay8Address **const, prgpAddress, DWORD *const, pcAddress, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,SetApplicationDesc,     const DPN_APPLICATION_DESC *const, pad, const DWORD, dwFlags) \
    ICOM_METHOD7(HRESULT,Host,                   const DPN_APPLICATION_DESC *const, pdnAppDesc, IDirectPlay8Address **const, prgpDeviceInfo, const DWORD, cDeviceInfo, const DPN_SECURITY_DESC *const, pdnSecurity, const DPN_SECURITY_CREDENTIALS *const, pdnCredentials, void *const, pvPlayerContext, const DWORD, dwFlags) \
    ICOM_METHOD7(HRESULT,SendTo,                 const DPNID, dpnid, const DPN_BUFFER_DESC *const, prgBufferDesc, const DWORD, cBufferDesc, const DWORD, dwTimeOut, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,CreateGroup,            const DPN_GROUP_INFO *const, pdpnGroupInfo,void *const, pvGroupContext, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,DestroyGroup,           const DPNID, idGroup, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,AddPlayerToGroup,       const DPNID, idGroup, const DPNID, idClient, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,RemovePlayerFromGroup,  const DPNID, idGroup, const DPNID, idClient, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,SetGroupInfo,           const DPNID, dpnid, DPN_GROUP_INFO *const, pdpnGroupInfo, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,GetGroupInfo,           const DPNID, dpnid, DPN_GROUP_INFO *const, pdpnGroupInfo, DWORD *const, pdwSize, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,EnumPlayersAndGroups,   DPNID *const, prgdpnid, DWORD *const, pcdpnid, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,EnumGroupMembers,       const DPNID, dpnid, DPNID *const, prgdpnid, DWORD *const, pcdpnid, const DWORD, dwFlags) \
    ICOM_METHOD1(HRESULT,Close,                  const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,DestroyClient,          const DPNID, dpnidClient, const void *const, pvDestroyData, const DWORD, dwDestroyDataSize, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,ReturnBuffer,           const DPNHANDLE, hBufferHandle, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetPlayerContext,       const DPNID, dpnid, PVOID *const, ppvPlayerContext, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetGroupContext,        const DPNID, dpnid, PVOID *const, ppvGroupContext, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,GetCaps,                DPN_CAPS *const, pdpCaps, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,SetCaps,                const DPN_CAPS *const, pdpCaps, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,SetSPCaps,              const GUID * const, pguidSP, const DPN_SP_CAPS *const, pdpspCaps, const DWORD, dwFlags ) \
    ICOM_METHOD3(HRESULT,GetSPCaps,              const GUID * const, pguidSP, DPN_SP_CAPS *const, pdpspCaps,const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetConnectionInfo,      const DPNID, dpnid, DPN_CONNECTION_INFO *const, pdpConnectionInfo, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,RegisterLobby,          const DPNHANDLE, dpnHandle, struct IDirectPlay8LobbiedApplication *const, pIDP8LobbiedApplication, const DWORD, dwFlags)
#define IDirectPlay8Server_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8Server_METHODS
ICOM_DEFINE(IDirectPlay8Server, IUnknown)
#undef ICOM_INTERFACE

   /* IUnknown methods */
#define	IDirectPlay8Server_QueryInterface(p,a,b)                 ICOM_CALL2(QueryInterface,p,a,b)
#define	IDirectPlay8Server_AddRef(p)                             ICOM_CALL (AddRef,p)
#define	IDirectPlay8Server_Release(p)                            ICOM_CALL (Release,p)
   /* IDirectPlay8Server methods */
#define	IDirectPlay8Server_Initialize(p,a,b,c)                   ICOM_CALL3(Initialize,p,a,b,c)
#define	IDirectPlay8Server_EnumServiceProviders(p,a,b,c,d,e,f)   ICOM_CALL6(EnumServiceProviders,p,a,b,c,d,e,f)
#define	IDirectPlay8Server_CancelAsyncOperation(p,a,b)           ICOM_CALL2(CancelAsyncOperation,p,a,b)
#define	IDirectPlay8Server_GetSendQueueInfo(p,a,b,c,d)           ICOM_CALL4(GetSendQueueInfo,p,a,b,c,d)
#define	IDirectPlay8Server_GetApplicationDesc(p,a,b,c)           ICOM_CALL3(GetApplicationDesc,p,a,b,c)
#define	IDirectPlay8Server_SetServerInfo(p,a,b,c,d)              ICOM_CALL4(SetServerInfo,p,a,b,c,d)
#define	IDirectPlay8Server_GetClientInfo(p,a,b,c,d)              ICOM_CALL4(GetClientInfo,p,a,b,c,d)
#define	IDirectPlay8Server_GetClientAddress(p,a,b,c)             ICOM_CALL3(GetClientAddress,p,a,b,c)
#define	IDirectPlay8Server_GetLocalHostAddresses(p,a,b,c)        ICOM_CALL3(GetLocalHostAddresses,p,a,b,c)
#define	IDirectPlay8Server_SetApplicationDesc(p,a,b)             ICOM_CALL2(SetApplicationDesc,p,a,b)
#define	IDirectPlay8Server_Host(p,a,b,c,d,e,f,g)                 ICOM_CALL7(Host,p,a,b,c,d,e,f,g)
#define	IDirectPlay8Server_SendTo(p,a,b,c,d,e,f,g)               ICOM_CALL7(SendTo,p,a,b,c,d,e,f,g)
#define	IDirectPlay8Server_CreateGroup(p,a,b,c,d,e)              ICOM_CALL5(CreateGroup,p,a,b,c,d,e)
#define	IDirectPlay8Server_DestroyGroup(p,a,b,c,d)               ICOM_CALL4(DestroyGroup,p,a,b,c,d)
#define	IDirectPlay8Server_AddPlayerToGroup(p,a,b,c,d,e)         ICOM_CALL5(AddPlayerToGroup,p,a,b,c,d,e)
#define	IDirectPlay8Server_RemovePlayerFromGroup(p,a,b,c,d,e)    ICOM_CALL5(RemovePlayerFromGroup,p,a,b,c,d,e)
#define	IDirectPlay8Server_SetGroupInfo(p,a,b,c,d,e)             ICOM_CALL5(SetGroupInfo,p,a,b,c,d,e)
#define	IDirectPlay8Server_GetGroupInfo(p,a,b,c,d)               ICOM_CALL4(GetGroupInfo,p,a,b,c,d)
#define	IDirectPlay8Server_EnumPlayersAndGroups(p,a,b,c)         ICOM_CALL3(EnumPlayersAndGroups,p,a,b,c)
#define	IDirectPlay8Server_EnumGroupMembers(p,a,b,c,d)           ICOM_CALL4(EnumGroupMembers,p,a,b,c,d)
#define	IDirectPlay8Server_Close(p,a)                            ICOM_CALL1(Close,p,a)
#define	IDirectPlay8Server_DestroyClient(p,a,b,c,d)              ICOM_CALL4(DestroyClient,p,a,b,c,d)
#define	IDirectPlay8Server_ReturnBuffer(p,a,b)                   ICOM_CALL2(ReturnBuffer,p,a,b)
#define	IDirectPlay8Server_GetPlayerContext(p,a,b,c)             ICOM_CALL3(GetPlayerContext,p,a,b,c)
#define	IDirectPlay8Server_GetGroupContext(p,a,b,c)              ICOM_CALL3(GetGroupContext,p,a,b,c)
#define	IDirectPlay8Server_GetCaps(p,a,b)                        ICOM_CALL2(GetCaps,p,a,b)
#define	IDirectPlay8Server_SetCaps(p,a,b)                        ICOM_CALL2(SetCaps,p,a,b)
#define	IDirectPlay8Server_SetSPCaps(p,a,b,c)                    ICOM_CALL3(SetSPCaps,p,a,b,c)
#define	IDirectPlay8Server_GetSPCaps(p,a,b,c)                    ICOM_CALL3(GetSPCaps,p,a,b,c)
#define	IDirectPlay8Server_GetConnectionInfo(p,a,b,c)            ICOM_CALL3(GetConnectionInfo,p,a,b,c)
#define	IDirectPlay8Server_RegisterLobby(p,a,b,c)                ICOM_CALL3(RegisterLobby,p,a,b,c)


/* IDirectPlay8Peer interface */
#define ICOM_INTERFACE IDirectPlay8Peer
#define IDirectPlay8Peer_METHODS \
    ICOM_METHOD3(HRESULT,Initialize,                 PVOID const, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, const DWORD, dwFlags) \
    ICOM_METHOD6(HRESULT,EnumServiceProviders,       const GUID *const, pguidServiceProvider, const GUID *const, pguidApplication, DPN_SERVICE_PROVIDER_INFO *const, pSPInfoBuffer, DWORD *const, pcbEnumData, DWORD *const, pcReturned, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,CancelAsyncOperation,       const DPNHANDLE, hAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD11(HRESULT,Connect,                   const DPN_APPLICATION_DESC *const, pdnAppDesc,IDirectPlay8Address *const, pHostAddr, IDirectPlay8Address *const, pDeviceInfo, const DPN_SECURITY_DESC *const, pdnSecurity, const DPN_SECURITY_CREDENTIALS *const, pdnCredentials, const void *const, pvUserConnectData, const DWORD, dwUserConnectDataSize, void *const, pvPlayerContext, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD7(HRESULT,SendTo,                     const DPNID, dpnid, const DPN_BUFFER_DESC *const, prgBufferDesc, const DWORD, cBufferDesc, const DWORD, dwTimeOut, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,GetSendQueueInfo,           const DPNID, dpnid, DWORD *const, pdwNumMsgs, DWORD *const, pdwNumBytes, const DWORD, dwFlags) \
    ICOM_METHOD7(HRESULT,Host,                       const DPN_APPLICATION_DESC *const, pdnAppDesc, IDirectPlay8Address **const, prgpDeviceInfo, const DWORD, cDeviceInfo, const DPN_SECURITY_DESC *const, pdnSecurity, const DPN_SECURITY_CREDENTIALS *const, pdnCredentials, void *const, pvPlayerContext, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetApplicationDesc,         DPN_APPLICATION_DESC *const, pAppDescBuffer, DWORD *const, pcbDataSize, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,SetApplicationDesc,         const DPN_APPLICATION_DESC *const, pad, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,CreateGroup,                const DPN_GROUP_INFO *const, pdpnGroupInfo, void *const, pvGroupContext, void *const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,DestroyGroup,               const DPNID, idGroup, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,AddPlayerToGroup,           const DPNID, idGroup, const DPNID, idClient, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,RemovePlayerFromGroup,      const DPNID, idGroup, const DPNID, idClient, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD5(HRESULT,SetGroupInfo,               const DPNID, dpnid, DPN_GROUP_INFO *const, pdpnGroupInfo, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,GetGroupInfo,               const DPNID, dpnid, DPN_GROUP_INFO *const, pdpnGroupInfo, DWORD *const, pdwSize, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,EnumPlayersAndGroups,       DPNID *const, prgdpnid, DWORD *const, pcdpnid, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,EnumGroupMembers,           const DPNID, dpnid, DPNID *const, prgdpnid, DWORD *const, pcdpnid, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,SetPeerInfo,                const DPN_PLAYER_INFO *const, pdpnPlayerInfo, PVOID const, pvAsyncContext, DPNHANDLE *const, phAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,GetPeerInfo,                const DPNID, dpnid, DPN_PLAYER_INFO *const, pdpnPlayerInfo, DWORD *const, pdwSize, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetPeerAddress,             const DPNID, dpnid, IDirectPlay8Address **const, pAddress, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetLocalHostAddresses,      IDirectPlay8Address **const, prgpAddress, DWORD *const, pcAddress, const DWORD, dwFlags) \
    ICOM_METHOD1(HRESULT,Close,                      const DWORD, dwFlags) \
    ICOM_METHOD11(HRESULT,EnumHosts,                 PDPN_APPLICATION_DESC const, pApplicationDesc, IDirectPlay8Address *const, pAddrHost, IDirectPlay8Address *const, pDeviceInfo, PVOID const, pUserEnumData, const DWORD, dwUserEnumDataSize, const DWORD, dwEnumCount, const DWORD, dwRetryInterval, const DWORD, dwTimeOut, PVOID const, pvUserContext, DPNHANDLE *const, pAsyncHandle, const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,DestroyPeer,                const DPNID, dpnidClient, const void *const, pvDestroyData, const DWORD, dwDestroyDataSize, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,ReturnBuffer,               const DPNHANDLE, hBufferHandle, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetPlayerContext,           const DPNID, dpnid, PVOID *const, ppvPlayerContext, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetGroupContext,            const DPNID, dpnid, PVOID *const, ppvGroupContext, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,GetCaps,                    DPN_CAPS *const, pdpCaps, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,SetCaps,                    const DPN_CAPS *const, pdpCaps, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,SetSPCaps,                  const GUID * const, pguidSP, const DPN_SP_CAPS *const, pdpspCaps, const DWORD, dwFlags ) \
    ICOM_METHOD3(HRESULT,GetSPCaps,                  const GUID * const, pguidSP, DPN_SP_CAPS *const, pdpspCaps, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetConnectionInfo,          const DPNID, dpnid, DPN_CONNECTION_INFO *const, pdpConnectionInfo, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,RegisterLobby,              const DPNHANDLE, dpnHandle, struct IDirectPlay8LobbiedApplication *const, pIDP8LobbiedApplication, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,TerminateSession,           void *const, pvTerminateData, const DWORD, dwTerminateDataSize, const DWORD, dwFlags)
#define IDirectPlay8Peer_IMETHODS \
    IUnknown_IMETHODS \
    IDirectPlay8Peer_METHODS
ICOM_DEFINE(IDirectPlay8Peer, IUnknown)
#undef ICOM_INTERFACE

   /* IUnknown methods */
#define	IDirectPlay8Peer_QueryInterface(p,a,b)                    ICOM_CALL2(QueryInterface,p,a,b)
#define	IDirectPlay8Peer_AddRef(p)                                ICOM_CALL (AddRef,p)
#define	IDirectPlay8Peer_Release(p)                               ICOM_CALL (Release,p)
   /* IDirectPlay8Peer methods */
#define	IDirectPlay8Peer_Initialize(p,a,b,c)                      ICOM_CALL3(Initialize,p,a,b,c)
#define	IDirectPlay8Peer_EnumServiceProviders(p,a,b,c,d,e,f)      ICOM_CALL6(EnumServiceProviders,p,a,b,c,d,e,f)
#define	IDirectPlay8Peer_CancelAsyncOperation(p,a,b)              ICOM_CALL2(CancelAsyncOperation,p,a,b)
#define	IDirectPlay8Peer_Connect(p,a,b,c,d,e,f,g,h,i,j,k)         ICOM_CALL11(Connect,p,a,b,c,d,e,f,g,h,i,j,k)
#define	IDirectPlay8Peer_SendTo(p,a,b,c,d,e,f,g)                  ICOM_CALL7(SendTo,p,a,b,c,d,e,f,g)
#define	IDirectPlay8Peer_GetSendQueueInfo(p,a,b,c,d)              ICOM_CALL4(GetSendQueueInfo,p,a,b,c,d)
#define	IDirectPlay8Peer_Host(p,a,b,c,d,e,f,g)                    ICOM_CALL7(Host,p,a,b,c,d,e,f,g)
#define	IDirectPlay8Peer_GetApplicationDesc(p,a,b,c)              ICOM_CALL3(GetApplicationDesc,p,a,b,c)
#define	IDirectPlay8Peer_SetApplicationDesc(p,a,b)                ICOM_CALL2(SetApplicationDesc,p,a,b)
#define	IDirectPlay8Peer_CreateGroup(p,a,b,c,d,e)                 ICOM_CALL5(CreateGroup,p,a,b,c,d,e)
#define	IDirectPlay8Peer_DestroyGroup(p,a,b,c,d)                  ICOM_CALL4(DestroyGroup,p,a,b,c,d)
#define	IDirectPlay8Peer_AddPlayerToGroup(p,a,b,c,d,e)            ICOM_CALL5(AddPlayerToGroup,p,a,b,c,d,e)
#define	IDirectPlay8Peer_RemovePlayerFromGroup(p,a,b,c,d,e)       ICOM_CALL5(RemovePlayerFromGroup,p,a,b,c,d,e)
#define	IDirectPlay8Peer_SetGroupInfo(p,a,b,c,d,e)                ICOM_CALL5(SetGroupInfo,p,a,b,c,d,e)
#define	IDirectPlay8Peer_GetGroupInfo(p,a,b,c,d)                  ICOM_CALL4(GetGroupInfo,p,a,b,c,d)
#define	IDirectPlay8Peer_EnumPlayersAndGroups(p,a,b,c)            ICOM_CALL3(EnumPlayersAndGroups,p,a,b,c)
#define	IDirectPlay8Peer_EnumGroupMembers(p,a,b,c,d)              ICOM_CALL4(EnumGroupMembers,p,a,b,c,d)
#define	IDirectPlay8Peer_SetPeerInfo(p,a,b,c,d)	                  ICOM_CALL4(SetPeerInfo,p,a,b,c,d)
#define	IDirectPlay8Peer_GetPeerInfo(p,a,b,c,d)	                  ICOM_CALL4(GetPeerInfo,p,a,b,c,d)
#define	IDirectPlay8Peer_GetPeerAddress(p,a,b,c)                  ICOM_CALL3(GetPeerAddress,p,a,b,c)
#define	IDirectPlay8Peer_GetLocalHostAddresses(p,a,b,c)	          ICOM_CALL3(GetLocalHostAddresses,p,a,b,c)
#define	IDirectPlay8Peer_Close(p,a)                               ICOM_CALL1(Close,p,a)
#define	IDirectPlay8Peer_EnumHosts(p,a,b,c,d,e,f,g,h,i,j,k)       ICOM_CALL11(EnumHosts,p,a,b,c,d,e,f,g,h,i,j,k)
#define	IDirectPlay8Peer_DestroyPeer(p,a,b,c,d)	                  ICOM_CALL4(DestroyPeer,p,a,b,c,d)
#define	IDirectPlay8Peer_ReturnBuffer(p,a,b)                      ICOM_CALL2(ReturnBuffer,p,a,b)
#define	IDirectPlay8Peer_GetPlayerContext(p,a,b,c)                ICOM_CALL3(GetPlayerContext,p,a,b,c)
#define	IDirectPlay8Peer_GetGroupContext(p,a,b,c)                 ICOM_CALL3(GetGroupContext,p,a,b,c)
#define	IDirectPlay8Peer_GetCaps(p,a,b)	                          ICOM_CALL2(GetCaps,p,a,b)
#define	IDirectPlay8Peer_SetCaps(p,a,b)	                          ICOM_CALL2(SetCaps,p,a,b)
#define	IDirectPlay8Peer_SetSPCaps(p,a,b,c)                       ICOM_CALL3(SetSPCaps,p,a,b,c)
#define	IDirectPlay8Peer_GetSPCaps(p,a,b,c)                       ICOM_CALL3(GetSPCaps,p,a,b,c)
#define	IDirectPlay8Peer_GetConnectionInfo(p,a,b,c)               ICOM_CALL3(GetConnectionInfo,p,a,b,c)
#define	IDirectPlay8Peer_RegisterLobby(p,a,b,c)	                  ICOM_CALL3(RegisterLobby,p,a,b,c)
#define	IDirectPlay8Peer_TerminateSession(p,a,b,c)                ICOM_CALL3(TerminateSession,p,a,b,c)

/* DirectPlay8ThreadPool interface - (Added in DX9) */
#define ICOM_INTERFACE IDirectPlay8ThreadPool
#define IDirectPlay8ThreadPool_METHODS \
    ICOM_METHOD3(HRESULT,Initialize,                PVOID const, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, const DWORD, dwFlags) \
    ICOM_METHOD1(HRESULT,Close,                     const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetThreadCount,            const DWORD, dwProcessorNum, DWORD *const, pdwNumThreads, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,SetThreadCount,            const DWORD, dwProcessorNum, const DWORD, dwNumThreads, const DWORD, dwFlags) \
    ICOM_METHOD2(HRESULT,DoWork,                    const DWORD, dwAllowedTimeSlice, const DWORD, dwFlags)
#define IDirectPlay8ThreadPool_IMETHODS \
     IUnknown_IMETHODS \
     IDirectPlay8ThreadPool_METHODS
ICOM_DEFINE(IDirectPlay8ThreadPool, IUnknown)
#undef ICOM_INTERFACE

    /* IUnknown methods */
#define IDirectPlay8ThreadPool_QueryInterface(p,a,b)                    ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectPlay8ThreadPool_AddRef(p)                                ICOM_CALL (AddRef,p)
#define IDirectPlay8ThreadPool_Release(p)                               ICOM_CALL (Release,p)
    /* IDirectPlay8ThreadPool methods */
#define IDirectPlay8ThreadPool_Initialize(p,a,b,c)                      ICOM_CALL3(Initialize,p,a,b,c)
#define IDirectPlay8ThreadPool_Close(p,a)                               ICOM_CALL1(Close,p,a)
#define IDirectPlay8ThreadPool_GetThreadCount(p,a,b,c)                  ICOM_CALL3(GetThreadCount,p,a,b,c)
#define IDirectPlay8ThreadPool_SetThreadCount(p,a,b,c)                  ICOM_CALL3(SetThreadCount,p,a,b,c)
#define IDirectPlay8ThreadPool_DoWork(p,a,b)                            ICOM_CALL2(DoWork,p,a,b)


/* DirectPlay8NatResolver interface (Added in DX9) */
#define ICOM_INTERFACE IDirectPlay8NATResolver
#define IDirectPlay8NATResolver_METHODS \
    ICOM_METHOD3(HRESULT,Initialize,                PVOID const, pvUserContext, const PFNDPNMESSAGEHANDLER, pfn, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,Start,                     IDirectPlay8Address **const, ppDevices, const DWORD, dwNumDevices, const DWORD, dwFlags) \
    ICOM_METHOD1(HRESULT,Close,                     const DWORD, dwFlags) \
    ICOM_METHOD4(HRESULT,EnumDevices,               DPN_SERVICE_PROVIDER_INFO *const, pSPInfoBuffer, PDWORD const, pdwBufferSize, PDWORD const, pdwNumDevices, const DWORD, dwFlags) \
    ICOM_METHOD3(HRESULT,GetAddress,                IDirectPlay8Address **const, ppAddresses, DWORD *const, pdwNumAddresses, const DWORD, dwFlags)
#define IDirectPlay8NATResolver_IMETHODS \
     IUnknown_IMETHODS \
     IDirectPlay8NATResolver_METHODS
ICOM_DEFINE(IDirectPlay8NATResolver, IUnknown)
#undef ICOM_INTERFACE

    /* IUnknown methods */
#define IDirectPlay8NATResolver_QueryInterface(p,a,b)                    ICOM_CALL2(QueryInterface,p,a,b)
#define IDirectPlay8NATResolver_AddRef(p)                                ICOM_CALL (AddRef,p)
#define IDirectPlay8NATResolver_Release(p)                               ICOM_CALL (Release,p)
    /* IDirectPlay8NATResolver methods */
#define IDirectPlay8NATResolver_Initialize(p,a,b,c)                      ICOM_CALL3(Initialize,p,a,b,c)
#define IDirectPlay8NATResolver_Start(p,a,b,c)                           ICOM_CALL3(Start,p,a,b,c)
#define IDirectPlay8NATResolver_Close(p,a)                               ICOM_CALL1(Close,p,a)
#define IDirectPlay8NATResolver_EnumDevices(p,a,b,c,d)                   ICOM_CALL4(EnumDevices,p,a,b,c,d)
#define IDirectPlay8NATResolver_GetAddress(p,a,b,c)                      ICOM_CALL3(GetAddress,p,a,b,c)


/* error values */

#define _DPN_FACILITY_CODE	0x015
#define _DPNHRESULT_BASE		0x8000
#define MAKE_DPNHRESULT( code )			MAKE_HRESULT( 1, _DPN_FACILITY_CODE, ( code + _DPNHRESULT_BASE ) )

#define DPN_OK							S_OK

#define DPNSUCCESS_EQUAL                MAKE_HRESULT( 0, _DPN_FACILITY_CODE, ( 0x5 + _DPNHRESULT_BASE ) )
#define DPNSUCCESS_NOPLAYERSINGROUP		MAKE_HRESULT( 0, _DPN_FACILITY_CODE, ( 0x8 + _DPNHRESULT_BASE ) )
#define DPNSUCCESS_NOTEQUAL             MAKE_HRESULT( 0, _DPN_FACILITY_CODE, (0x0A + _DPNHRESULT_BASE ) )
#define DPNSUCCESS_PENDING				MAKE_HRESULT( 0, _DPN_FACILITY_CODE, (0x0e + _DPNHRESULT_BASE ) )

#define DPNERR_ABORTED					MAKE_DPNHRESULT(  0x30 )
#define DPNERR_ADDRESSING				MAKE_DPNHRESULT(  0x40 )
#define	DPNERR_ALREADYCLOSING			MAKE_DPNHRESULT(  0x50 )
#define DPNERR_ALREADYCONNECTED			MAKE_DPNHRESULT(  0x60 )
#define DPNERR_ALREADYDISCONNECTING		MAKE_DPNHRESULT(  0x70 )
#define DPNERR_ALREADYINITIALIZED		MAKE_DPNHRESULT(  0x80 )
#define DPNERR_ALREADYREGISTERED		MAKE_DPNHRESULT(  0x90 )
#define DPNERR_BUFFERTOOSMALL			MAKE_DPNHRESULT( 0x100 )
#define DPNERR_CANNOTCANCEL				MAKE_DPNHRESULT( 0x110 )
#define DPNERR_CANTCREATEGROUP			MAKE_DPNHRESULT( 0x120 )
#define DPNERR_CANTCREATEPLAYER			MAKE_DPNHRESULT( 0x130 )
#define DPNERR_CANTLAUNCHAPPLICATION	MAKE_DPNHRESULT( 0x140 )
#define DPNERR_CONNECTING				MAKE_DPNHRESULT( 0x150 )
#define DPNERR_CONNECTIONLOST			MAKE_DPNHRESULT( 0x160 )
#define DPNERR_CONVERSION				MAKE_DPNHRESULT( 0x170 )
#define	DPNERR_DATATOOLARGE				MAKE_DPNHRESULT( 0x175 )
#define DPNERR_DOESNOTEXIST				MAKE_DPNHRESULT( 0x180 )
#define DPNERR_DUPLICATECOMMAND			MAKE_DPNHRESULT( 0x190 )
#define DPNERR_ENDPOINTNOTRECEIVING		MAKE_DPNHRESULT( 0x200 )
#define	DPNERR_ENUMQUERYTOOLARGE		MAKE_DPNHRESULT( 0x210 )
#define	DPNERR_ENUMRESPONSETOOLARGE		MAKE_DPNHRESULT( 0x220 )
#define DPNERR_EXCEPTION				MAKE_DPNHRESULT( 0x230 )
#define DPNERR_GENERIC					E_FAIL
#define DPNERR_GROUPNOTEMPTY			MAKE_DPNHRESULT( 0x240 )
#define DPNERR_HOSTING                  MAKE_DPNHRESULT( 0x250 )
#define DPNERR_HOSTREJECTEDCONNECTION	MAKE_DPNHRESULT( 0x260 )
#define DPNERR_HOSTTERMINATEDSESSION	MAKE_DPNHRESULT( 0x270 )
#define DPNERR_INCOMPLETEADDRESS		MAKE_DPNHRESULT( 0x280 )
#define DPNERR_INVALIDADDRESSFORMAT		MAKE_DPNHRESULT( 0x290 )
#define DPNERR_INVALIDAPPLICATION		MAKE_DPNHRESULT( 0x300 )
#define DPNERR_INVALIDCOMMAND			MAKE_DPNHRESULT( 0x310 )
#define DPNERR_INVALIDDEVICEADDRESS		MAKE_DPNHRESULT( 0x320 )
#define DPNERR_INVALIDENDPOINT			MAKE_DPNHRESULT( 0x330 )
#define DPNERR_INVALIDFLAGS				MAKE_DPNHRESULT( 0x340 )
#define DPNERR_INVALIDGROUP			 	MAKE_DPNHRESULT( 0x350 )
#define DPNERR_INVALIDHANDLE			MAKE_DPNHRESULT( 0x360 )
#define DPNERR_INVALIDHOSTADDRESS		MAKE_DPNHRESULT( 0x370 )
#define DPNERR_INVALIDINSTANCE			MAKE_DPNHRESULT( 0x380 )
#define DPNERR_INVALIDINTERFACE			MAKE_DPNHRESULT( 0x390 )
#define DPNERR_INVALIDOBJECT			MAKE_DPNHRESULT( 0x400 )
#define DPNERR_INVALIDPARAM				E_INVALIDARG
#define DPNERR_INVALIDPASSWORD			MAKE_DPNHRESULT( 0x410 )
#define DPNERR_INVALIDPLAYER			MAKE_DPNHRESULT( 0x420 )
#define DPNERR_INVALIDPOINTER			E_POINTER
#define DPNERR_INVALIDPRIORITY			MAKE_DPNHRESULT( 0x430 )
#define DPNERR_INVALIDSTRING			MAKE_DPNHRESULT( 0x440 )
#define DPNERR_INVALIDURL				MAKE_DPNHRESULT( 0x450 )
#define	DPNERR_INVALIDVERSION			MAKE_DPNHRESULT( 0x460 )
#define DPNERR_NOCAPS					MAKE_DPNHRESULT( 0x470 )
#define DPNERR_NOCONNECTION				MAKE_DPNHRESULT( 0x480 )
#define DPNERR_NOHOSTPLAYER				MAKE_DPNHRESULT( 0x490 )
#define DPNERR_NOINTERFACE				E_NOINTERFACE
#define DPNERR_NOMOREADDRESSCOMPONENTS	MAKE_DPNHRESULT( 0x500 )
#define DPNERR_NORESPONSE				MAKE_DPNHRESULT( 0x510 )
#define DPNERR_NOTALLOWED				MAKE_DPNHRESULT( 0x520 )
#define DPNERR_NOTHOST					MAKE_DPNHRESULT( 0x530 )
#define DPNERR_NOTREADY					MAKE_DPNHRESULT( 0x540 )
#define DPNERR_NOTREGISTERED			MAKE_DPNHRESULT( 0x550 )
#define DPNERR_OUTOFMEMORY				E_OUTOFMEMORY
#define DPNERR_PENDING					DPNSUCCESS_PENDING
#define DPNERR_PLAYERALREADYINGROUP     MAKE_DPNHRESULT( 0x560 )
#define DPNERR_PLAYERLOST				MAKE_DPNHRESULT( 0x570 )
#define DPNERR_PLAYERNOTINGROUP         MAKE_DPNHRESULT( 0x580 )
#define	DPNERR_PLAYERNOTREACHABLE		MAKE_DPNHRESULT( 0x590 )
#define DPNERR_SENDTOOLARGE				MAKE_DPNHRESULT( 0x600 )
#define DPNERR_SESSIONFULL				MAKE_DPNHRESULT( 0x610 )
#define DPNERR_TABLEFULL				MAKE_DPNHRESULT( 0x620 )
#define DPNERR_TIMEDOUT					MAKE_DPNHRESULT( 0x630 )
#define DPNERR_UNINITIALIZED			MAKE_DPNHRESULT( 0x640 )
#define DPNERR_UNSUPPORTED				E_NOTIMPL
#define DPNERR_USERCANCEL				MAKE_DPNHRESULT( 0x650 )

#ifdef __cplusplus
}
#endif

#endif

