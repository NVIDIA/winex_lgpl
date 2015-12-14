/*
 * GUID definitions
 */

#include "initguid.h"

/* GUIDs defined in uuids.lib */

DEFINE_GUID(GUID_NULL,0,0,0,0,0,0,0,0,0,0,0);

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "objbase.h"
#include "servprov.h"

#include "oleauto.h"
#include "oleidl.h"
#include "objidl.h"
#include "olectl.h"

#include "ocidl.h"

#include "docobj.h"

#include "urlmon.h"

#include "shlguid.h"
#include "shlobj.h"

#include "hlguids.h"
#include "hlink.h"

#include "optary.h"
#include "objsafe.h"

#include "exdisp.h"
#include "htmlguid.h"
#include "mshtml.h"
#include "intshcut.h"
#include "activscp.h"
#include "activdbg.h"
#include "dispex.h"
#include "mshtmhst.h"
#include "urlhist.h"
#include "htiframe.h"

/* FIXME: cguids declares GUIDs but does not define their values */



/* GUIDs defined in dxguid.lib */

#include "d3d9.h"
#include "d3dx9.h"
#include "d3d8.h"
#include "d3d.h"
#include "ddraw.h"
#include "dsound.h"
#include "dplay.h"
#include "dplobby.h"
#include "dinput.h"
#include "dmusicc.h"

#include "ddrawi.h"

#include "dplay8.h"
#include "dplobby8.h"
#include "dpaddr.h"

#include "dxdiag.h"
#include "rmxfguid.h"

/* other GUIDs */

#include "vfw.h"

/* for dshow */
#include "strmif.h"
#include "control.h"
#include "dmusici.h"
#include "uuids.h"

/* for mlang */
#include "mlang.h"
 
/* WINE internal GUIDs */
#include "wine/d3dhalgl.h"

/* GUIDs not declared in an exported header file */
DEFINE_GUID(FMTID_SummaryInformation,0xF29F85E0,0x4FF9,0x1068,0xAB,0x91,0x08,0x00,0x2B,0x27,0xB3,0xD9);
DEFINE_GUID(FMTID_DocSummaryInformation,0xD5CDD502,0x2E9C,0x101B,0x93,0x97,0x08,0x00,0x2B,0x2C,0xF9,0xAE);
DEFINE_GUID(FMTID_UserDefinedProperties,0xD5CDD505,0x2E9C,0x101B,0x93,0x97,0x08,0x00,0x2B,0x2C,0xF9,0xAE);
DEFINE_GUID(IID_IDirectPlaySP,0xc9f6360,0xcc61,0x11cf,0xac,0xec,0x00,0xaa,0x00,0x68,0x86,0xe3);
DEFINE_GUID(IID_ISFHelper,0x1fe68efb,0x1874,0x9812,0x56,0xdc,0x00,0x00,0x00,0x00,0x00,0x00);
DEFINE_GUID(IID_IDPLobbySP,0x5a4e5a20,0x2ced,0x11d0,0xa8,0x89,0x00,0xa0,0xc9,0x05,0x43,0x3c);
DEFINE_GUID(IID_IDirectPlay8ThreadPool_Internal,0x12f40839,0xe7a3,0x399b,0xab,0x45,0x9e,0x43,0x3d,0x8b,0x31,0x96);
DEFINE_GUID(IID_IDirectPlay8ServiceProvider,0x985e2c76,0x66bc,0x4d66,0xad,0x8e,0xed,0x38,0x1,0xf1,0xb4,0x59);
DEFINE_GUID(IID_IBindStatusCallbackHolder,0x79eac9cc,0xbaf9,0x11ce,0x8c,0x82,0x00,0xaa,0x00,0x4b,0xa9,0x0b);

