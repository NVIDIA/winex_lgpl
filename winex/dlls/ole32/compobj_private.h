/*
 * Copyright 1995 Martin von Loewis
 * Copyright 1998 Justin Bradford
 * Copyright 1999 Francis Beaudet
 * Copyright 1999 Sylvain St-Germain
 * Copyright 2002 Marcus Meissner
 * Copyright 2003 Ove Kåven, TransGaming Technologies
 */

#ifndef __WINE_OLE_COMPOBJ_H
#define __WINE_OLE_COMPOBJ_H

/* All private prototype functions used by OLE will be added to this header file */

#include "wtypes.h"
#include "dcom.h"
#include "thread.h"

/* exported interface */
typedef struct tagXIF {
  struct tagXIF *next;
  LPVOID iface;            /* interface pointer */
  IID iid;                 /* interface ID */
  IPID ipid;               /* exported interface ID */
  LPRPCSTUBBUFFER stub;    /* interface stub */
  DWORD refs;              /* external reference count */
  HRESULT hres;            /* result of stub creation attempt */
} XIF;

/* exported object */
typedef struct tagXOBJECT {
  ICOM_VTABLE(IRpcStubBuffer) *lpVtbl;
  struct tagAPARTMENT *parent;
  struct tagXOBJECT *next;
  LPUNKNOWN obj;           /* object identity (IUnknown) */
  OID oid;                 /* object ID */
  DWORD ifc;               /* interface ID counter */
  XIF *ifaces;             /* exported interfaces */
  DWORD refs;              /* external reference count */
} XOBJECT;

/* imported interface */
typedef struct tagIIF {
  struct tagIIF *next;
  LPVOID iface;            /* interface pointer */
  IID iid;                 /* interface ID */
  IPID ipid;               /* imported interface ID */
  LPRPCPROXYBUFFER proxy;  /* interface proxy */
  DWORD refs;              /* imported (public) references */
  HRESULT hres;            /* result of proxy creation attempt */
} IIF;

/* imported object */
typedef struct tagIOBJECT {
  ICOM_VTABLE(IRemUnknown) *lpVtbl;
  struct tagAPARTMENT *parent;
  struct tagIOBJECT *next;
  LPRPCCHANNELBUFFER chan; /* channel to object */
  OXID oxid;               /* object exported ID */
  OID oid;                 /* object ID */
  IPID ipid;               /* first imported interface ID */
  IIF *ifaces;             /* imported interfaces */
  DWORD refs;              /* proxy reference count */
} IOBJECT;

/* apartment */
typedef struct tagAPARTMENT {
  struct tagAPARTMENT *next, *prev, *parent;
  DWORD model;             /* threading model */
  DWORD inits;             /* CoInitialize count */
  DWORD tid;               /* thread id */
  HANDLE thread;           /* thread handle */
  OXID oxid;               /* object exporter ID */
  OID oidc;                /* object ID counter */
  HWND win;                /* message window */
  CRITICAL_SECTION cs;     /* thread safety */
  LPMESSAGEFILTER filter;  /* message filter */
  XOBJECT *objs;           /* exported objects */
  IOBJECT *proxies;        /* imported objects */
  LPVOID ErrorInfo;        /* thread error info */
  EXCEPTION_RECORD *except; /* exception storage */
} APARTMENT;

extern APARTMENT MTA, *apts;

extern HRESULT WINE_StringFromCLSID(const CLSID *id,LPSTR idstr);
extern HRESULT create_marshalled_proxy(REFCLSID rclsid, REFIID iid, LPVOID *ppv);

inline static HRESULT
get_facbuf_for_iid(REFIID riid,IPSFactoryBuffer **facbuf) {
    HRESULT       hres;
    CLSID         pxclsid;

    if ((hres = CoGetPSClsid(riid,&pxclsid)))
	return hres;
    return CoGetClassObject(&pxclsid,CLSCTX_INPROC_SERVER,NULL,&IID_IPSFactoryBuffer,(LPVOID*)facbuf);
}

#define PIPEPREF "\\\\.\\pipe\\"
#define OLESTUBMGR PIPEPREF"WINE_OLE_StubMgr"
/* Standard Marshaling definitions */
typedef struct _wine_marshal_id {
    DWORD	processid;
    DWORD	objectid;	/* unique value corresp. IUnknown of object */
    IID		iid;
} wine_marshal_id;

inline static BOOL
MARSHAL_Compare_Mids(wine_marshal_id *mid1,wine_marshal_id *mid2) {
    return
	(mid1->processid == mid2->processid)	&&
	(mid1->objectid == mid2->objectid)	&&
	IsEqualIID(&(mid1->iid),&(mid2->iid))
    ;
}

/* compare without interface compare */
inline static BOOL
MARSHAL_Compare_Mids_NoInterface(wine_marshal_id *mid1, wine_marshal_id *mid2) {
    return
	(mid1->processid == mid2->processid)	&&
	(mid1->objectid == mid2->objectid)
    ;
}

HRESULT MARSHAL_Find_Stub_Buffer(wine_marshal_id *mid,IRpcStubBuffer **stub);
HRESULT MARSHAL_Find_Stub_Server(wine_marshal_id *mid,LPUNKNOWN *punk);
HRESULT MARSHAL_Register_Stub(wine_marshal_id *mid,LPUNKNOWN punk, IRpcStubBuffer *stub);

HRESULT MARSHAL_GetStandardMarshalCF(LPVOID *ppv);

typedef struct _wine_marshal_data {
    DWORD	dwDestContext;
    DWORD	mshlflags;
} wine_marshal_data;


#define REQTYPE_REQUEST		0
typedef struct _wine_rpc_request_header {
    DWORD		reqid;
    wine_marshal_id	mid;
    DWORD		iMethod;
    DWORD		cbBuffer;
} wine_rpc_request_header;

#define REQTYPE_RESPONSE	1
typedef struct _wine_rpc_response_header {
    DWORD		reqid;
    DWORD		cbBuffer;
    DWORD		retval;
} wine_rpc_response_header;

#define REQSTATE_START			0
#define REQSTATE_REQ_QUEUED		1
#define REQSTATE_REQ_WAITING_FOR_REPLY	2
#define REQSTATE_REQ_GOT		3
#define REQSTATE_INVOKING		4
#define REQSTATE_RESP_QUEUED		5
#define REQSTATE_RESP_GOT		6
#define REQSTATE_DONE			6

void STUBMGR_Start();

extern HRESULT PIPE_GetNewPipeBuf(wine_marshal_id *mid, IRpcChannelBuffer **pipebuf);

/* This function initialize the Running Object Table */
HRESULT WINAPI RunningObjectTableImpl_Initialize();

/* This function uninitialize the Running Object Table */
HRESULT WINAPI RunningObjectTableImpl_UnInitialize();

/* This function decomposes a String path to a String Table containing all the elements ("\" or "subDirectory" or "Directory" or "FileName") of the path */
int WINAPI FileMonikerImpl_DecomposePath(LPCOLESTR str, LPOLESTR** stringTable);

/*
 * Per-thread values are stored in the TEB on offset 0xF80,
 * see http://www.microsoft.com/msj/1099/bugslayer/bugslayer1099.htm
 */
static inline APARTMENT* COM_CurrentInfo(void) WINE_UNUSED;
static inline APARTMENT* COM_CurrentInfo(void)
{
  APARTMENT* apt = NtCurrentTeb()->ErrorInfo;
  return apt;
}
static inline APARTMENT* COM_CurrentApt(void) WINE_UNUSED;
static inline APARTMENT* COM_CurrentApt(void)
{
  APARTMENT* apt = COM_CurrentInfo();
  if (apt && apt->parent) apt = apt->parent;
  return apt;
}

/* proxy.c */
void COM_RpcInit(void);
void COM_RpcExportClass(REFCLSID rclsid, DWORD dwCtx);
void COM_RpcUnexportClass(REFCLSID rclsid, DWORD dwCtx);
HRESULT COM_RpcImportClass(LPVOID *ppv, REFCLSID rclsid, REFIID riid, LPSTR server);

HRESULT COM_GetStdObjRef(IStream *pStm, STDOBJREF *pObjRef, LPIID piid);

LRESULT CALLBACK COM_AptWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* compobj.c */
HWND COM_GetApartmentWin(OXID oxid);
HRESULT COM_GetRegisteredClassObject(REFCLSID rclsid, LPUNKNOWN* ppUnk, STDOBJREF* pObjRef, MInterfacePointer** ppMIF);

#endif /* __WINE_OLE_COMPOBJ_H */
