/*
 * Misc marshalling routines
 *
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 */
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"

#include "ole2.h"
#include "oleauto.h"
#include "rpcproxy.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/* FIXME: not supposed to be here, will be removed soon */

const CLSID CLSID_PSDispatch = {
  0x20420, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}
};

static CStdPSFactoryBuffer PSFactoryBuffer;

CSTDSTUBBUFFERRELEASE(&PSFactoryBuffer)

extern const ExtendedProxyFileInfo oaidl_ProxyFileInfo;

const ProxyFileInfo* OLEAUT32_ProxyFileList[] = {
  &oaidl_ProxyFileInfo,
  NULL
};

HRESULT OLEAUTPS_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
  return NdrDllGetClassObject(rclsid, riid, ppv, OLEAUT32_ProxyFileList,
                              &CLSID_PSDispatch, &PSFactoryBuffer);
}

/* CLEANLOCALSTORAGE */
/* I'm not sure how this is supposed to work yet */

ULONG WINAPI CLEANLOCALSTORAGE_UserSize(ULONG *pFlags, ULONG Start, CLEANLOCALSTORAGE *pstg)
{
  return Start + sizeof(DWORD);
}

unsigned char * WINAPI CLEANLOCALSTORAGE_UserMarshal(ULONG *pFlags, unsigned char *Buffer, CLEANLOCALSTORAGE *pstg)
{
  *(DWORD*)Buffer = 0;
  return Buffer + sizeof(DWORD);
}

unsigned char * WINAPI CLEANLOCALSTORAGE_UserUnmarshal(ULONG *pFlags, unsigned char *Buffer, CLEANLOCALSTORAGE *pstr)
{
  return Buffer + sizeof(DWORD);
}

void WINAPI CLEANLOCALSTORAGE_UserFree(ULONG *pFlags, CLEANLOCALSTORAGE *pstr)
{
}

/* BSTR */

ULONG WINAPI BSTR_UserSize(ULONG *pFlags, ULONG Start, BSTR *pstr)
{
  TRACE("(%x,%u,%p) => %p\n", *pFlags, Start, pstr, *pstr);
  if (*pstr) TRACE("string=%s\n", debugstr_w(*pstr));
  Start += sizeof(FLAGGED_WORD_BLOB) + sizeof(OLECHAR) * (SysStringLen(*pstr) - 1);
  TRACE("returning %u\n", Start);
  return Start;
}

unsigned char * WINAPI BSTR_UserMarshal(ULONG *pFlags, unsigned char *Buffer, BSTR *pstr)
{
  wireBSTR str = (wireBSTR)Buffer;

  TRACE("(%x,%p,%p) => %p\n", *pFlags, Buffer, pstr, *pstr);
  if (*pstr) TRACE("string=%s\n", debugstr_w(*pstr));
  str->fFlags = 0;
  str->clSize = SysStringLen(*pstr);
  if (str->clSize)
    memcpy(&str->asData, *pstr, sizeof(OLECHAR) * str->clSize);
  return Buffer + sizeof(FLAGGED_WORD_BLOB) + sizeof(OLECHAR) * (str->clSize - 1);
}

unsigned char * WINAPI BSTR_UserUnmarshal(ULONG *pFlags, unsigned char *Buffer, BSTR *pstr)
{
  wireBSTR str = (wireBSTR)Buffer;
  TRACE("(%x,%p,%p) => %p\n", *pFlags, Buffer, pstr, *pstr);
  if (str->clSize) {
    SysReAllocStringLen(pstr, (OLECHAR*)&str->asData, str->clSize);
  }
  else if (*pstr) {
    SysFreeString(*pstr);
    *pstr = NULL;
  }
  if (*pstr) TRACE("string=%s\n", debugstr_w(*pstr));
  return Buffer + sizeof(FLAGGED_WORD_BLOB) + sizeof(OLECHAR) * (str->clSize - 1);
}

void WINAPI BSTR_UserFree(ULONG *pFlags, BSTR *pstr)
{
  TRACE("(%x,%p) => %p\n", *pFlags, pstr, *pstr);
  if (*pstr) {
    SysFreeString(*pstr);
    *pstr = NULL;
  }
}

/* VARIANT */
/* I'm not too sure how to do this yet */

#define VARIANT_wiresize sizeof(struct _wireVARIANT)

static unsigned wire_size(VARTYPE vt)
{
  if (vt & VT_ARRAY) return 0;

  switch (vt & ~VT_BYREF) {
  case VT_EMPTY:
  case VT_NULL:
    return 0;
  case VT_I1:
  case VT_UI1:
    return sizeof(CHAR);
  case VT_I2:
  case VT_UI2:
    return sizeof(SHORT);
  case VT_I4:
  case VT_UI4:
    return sizeof(LONG);
  case VT_INT:
  case VT_UINT:
    return sizeof(INT);
  case VT_R4:
    return sizeof(FLOAT);
  case VT_R8:
    return sizeof(DOUBLE);
  case VT_BOOL:
    return sizeof(VARIANT_BOOL);
  case VT_ERROR:
    return sizeof(SCODE);
  case VT_DATE:
    return sizeof(DATE);
  case VT_CY:
    return sizeof(CY);
  case VT_DECIMAL:
    return sizeof(DECIMAL);
  case VT_BSTR:
  case VT_VARIANT:
  case VT_UNKNOWN:
  case VT_DISPATCH:
  case VT_SAFEARRAY:
  case VT_RECORD:
    return 0;
  default:
    FIXME("unhandled VT %d\n", vt);
    return 0;
  }
}

static unsigned size_interface(ULONG *pFlags, LPVOID pv, REFIID riid)
{
  ULONG size;
  HRESULT hr;
  hr = CoGetMarshalSizeMax(&size, riid, pv, LOWORD(*pFlags), NULL, MSHLFLAGS_NORMAL);
  if (FAILED(hr)) return 0;
  return size;
}

static unsigned wire_extra(ULONG *pFlags, VARIANT *pvar)
{
  if (V_VT(pvar) & VT_ARRAY) {
    FIXME("wire-size safearray\n");
    return 0;
  }
  switch (V_VT(pvar)) {
  case VT_BSTR:
    return BSTR_UserSize(pFlags, 0, &V_BSTR(pvar));
  case VT_BSTR | VT_BYREF:
    return BSTR_UserSize(pFlags, 0, V_BSTRREF(pvar));
  case VT_SAFEARRAY:
  case VT_SAFEARRAY | VT_BYREF:
    FIXME("wire-size safearray\n");
    return 0;
  case VT_VARIANT | VT_BYREF:
    return VARIANT_UserSize(pFlags, 0, V_VARIANTREF(pvar));
  case VT_UNKNOWN:
    return size_interface(pFlags, V_UNKNOWN(pvar), &IID_IUnknown);
    break;
  case VT_UNKNOWN | VT_BYREF:
    return size_interface(pFlags, *V_UNKNOWNREF(pvar), &IID_IUnknown);
    break;
  case VT_DISPATCH:
    return size_interface(pFlags, V_DISPATCH(pvar), &IID_IDispatch);
    break;
  case VT_DISPATCH | VT_BYREF:
    return size_interface(pFlags, *V_DISPATCHREF(pvar), &IID_IDispatch);
    break;
  case VT_RECORD:
    FIXME("wire-size record\n");
    return 0;
  default:
    return 0;
  }
}

static unsigned char * marshal_interface(PULONG pFlags, unsigned char *Buffer, unsigned size, LPVOID pv, REFIID riid)
{
  LPSTREAM pStm = NULL;
  HGLOBAL hGlobal = 0;
  HRESULT hr;
  hr = CreateStreamOnHGlobal(0, TRUE, &pStm);
  if (SUCCEEDED(hr))
    hr = CoMarshalInterface(pStm, riid, pv, LOWORD(*pFlags), NULL, MSHLFLAGS_NORMAL);
  if (SUCCEEDED(hr)) {
    GetHGlobalFromStream(pStm, &hGlobal);
    memcpy(Buffer, GlobalLock(hGlobal), size);
    GlobalUnlock(hGlobal);
    IStream_Release(pStm);
    return Buffer + size;
  }
  RpcRaiseException(hr); /* FIXME: translate exception? */
  /* won't return */
  return Buffer;
}

static unsigned char * unmarshal_interface(PULONG pFlags, unsigned char *Buffer, unsigned size, LPVOID *ppv, REFIID riid)
{
  LPSTREAM pStm = NULL;
  ULARGE_INTEGER ss;
  HGLOBAL hGlobal = 0;
  HRESULT hr;
  ss.s.HighPart = 0;
  ss.s.LowPart = size;
  hr = CreateStreamOnHGlobal(0, TRUE, &pStm);
  if (SUCCEEDED(hr)) {
    IStream_SetSize(pStm, ss);
    GetHGlobalFromStream(pStm, &hGlobal);
    memcpy(GlobalLock(hGlobal), Buffer, size);
    GlobalUnlock(hGlobal);
    hr = CoUnmarshalInterface(pStm, riid, ppv);
    IStream_Release(pStm);
  }
  if (SUCCEEDED(hr)) {
    return Buffer + size;
  }
  RpcRaiseException(hr); /* FIXME: translate exception? */
  /* won't return */
  return Buffer;
}

ULONG WINAPI VARIANT_UserSize(ULONG *pFlags, ULONG Start, VARIANT *pvar)
{
  TRACE("(%x,%u,%p)\n", *pFlags, Start, pvar);
  TRACE("vt=%04x\n", V_VT(pvar));
  Start += VARIANT_wiresize + wire_extra(pFlags, pvar);
  TRACE("returning %u\n", Start);
  return Start;
}

unsigned char * WINAPI VARIANT_UserMarshal(ULONG *pFlags, unsigned char *Buffer, VARIANT *pvar)
{
  wireVARIANT var = (wireVARIANT)Buffer;
  unsigned size, extra;
  unsigned char *Pos = Buffer + VARIANT_wiresize;

  TRACE("(%x,%p,%p)\n", *pFlags, Buffer, pvar);
  TRACE("vt=%04x\n", V_VT(pvar));

  memset(var, 0, sizeof(*var));
  var->clSize = sizeof(*var);
  var->vt = pvar->n1.n2.vt;

  var->rpcReserved = var->vt;
  if ((var->vt & VT_ARRAY) ||
      ((var->vt & VT_TYPEMASK) == VT_SAFEARRAY))
    var->vt = VT_ARRAY | (var->vt & VT_BYREF);

  if (var->vt == VT_DECIMAL) {
    /* special case because decVal is on a different level */
    var->u.decVal = pvar->n1.decVal;
    return Pos;
  }

  size = wire_size(V_VT(pvar));
  extra = wire_extra(pFlags, pvar);
  var->wReserved1 = pvar->n1.n2.wReserved1;
  var->wReserved2 = pvar->n1.n2.wReserved2;
  var->wReserved3 = pvar->n1.n2.wReserved3;
  if (size) {
    if (var->vt & VT_BYREF)
      memcpy(&var->u.cVal, pvar->n1.n2.n3.byref, size);
    else
      memcpy(&var->u.cVal, &pvar->n1.n2.n3, size);
  }
  if (!extra) return Pos;

  switch (var->vt) {
  case VT_BSTR:
    Pos = BSTR_UserMarshal(pFlags, Pos, &V_BSTR(pvar));
    break;
  case VT_BSTR | VT_BYREF:
    Pos = BSTR_UserMarshal(pFlags, Pos, V_BSTRREF(pvar));
    break;
  case VT_VARIANT | VT_BYREF:
    Pos = VARIANT_UserMarshal(pFlags, Pos, V_VARIANTREF(pvar));
    break;
  case VT_UNKNOWN:
    Pos = marshal_interface(pFlags, Pos, extra, V_UNKNOWN(pvar), &IID_IUnknown);
    break;
  case VT_UNKNOWN | VT_BYREF:
    Pos = marshal_interface(pFlags, Pos, extra, *V_UNKNOWNREF(pvar), &IID_IUnknown);
    break;
  case VT_DISPATCH:
    Pos = marshal_interface(pFlags, Pos, extra, V_DISPATCH(pvar), &IID_IDispatch);
    break;
  case VT_DISPATCH | VT_BYREF:
    Pos = marshal_interface(pFlags, Pos, extra, *V_DISPATCHREF(pvar), &IID_IDispatch);
    break;
  case VT_RECORD:
    FIXME("handle BRECORD by val\n");
    break;
  case VT_RECORD | VT_BYREF:
    FIXME("handle BRECORD by ref\n");
    break;
  default:
    FIXME("handle unknown complex type\n");
    break;
  }
  var->clSize = Pos - Buffer;
  TRACE("marshaled size=%u\n", var->clSize);
  return Pos;
}

unsigned char * WINAPI VARIANT_UserUnmarshal(ULONG *pFlags, unsigned char *Buffer, VARIANT *pvar)
{
  wireVARIANT var = (wireVARIANT)Buffer;
  unsigned size, extra;
  unsigned char *Pos = Buffer + VARIANT_wiresize;

  TRACE("(%x,%p,%p)\n", *pFlags, Buffer, pvar);
  VariantInit(pvar);
  pvar->n1.n2.vt = var->rpcReserved;
  TRACE("marshaled: clSize=%u, vt=%04x\n", var->clSize, var->vt);
  TRACE("vt=%04x\n", V_VT(pvar));
  TRACE("reserved: %d, %d, %d\n", var->wReserved1, var->wReserved2, var->wReserved3);
  TRACE("val: %d\n", var->u.lVal);

  if (var->vt == VT_DECIMAL) {
    /* special case because decVal is on a different level */
    pvar->n1.decVal = var->u.decVal;
    return Pos;
  }

  size = wire_size(V_VT(pvar));
  pvar->n1.n2.wReserved1 = var->wReserved1;
  pvar->n1.n2.wReserved2 = var->wReserved2;
  pvar->n1.n2.wReserved3 = var->wReserved3;
  if (size) {
    if (var->vt & VT_BYREF) {
      pvar->n1.n2.n3.byref = CoTaskMemAlloc(size);
      memcpy(pvar->n1.n2.n3.byref, &var->u.cVal, size);
    }
    else
      memcpy(&pvar->n1.n2.n3, &var->u.cVal, size);
  }
  if (var->clSize <= VARIANT_wiresize) return Pos;
  extra = var->clSize - VARIANT_wiresize;

  switch (var->vt) {
  case VT_BSTR:
    Pos = BSTR_UserUnmarshal(pFlags, Pos, &V_BSTR(pvar));
    break;
  case VT_BSTR | VT_BYREF:
    pvar->n1.n2.n3.byref = CoTaskMemAlloc(sizeof(BSTR));
    *(BSTR*)pvar->n1.n2.n3.byref = NULL;
    Pos = BSTR_UserUnmarshal(pFlags, Pos, V_BSTRREF(pvar));
    break;
  case VT_VARIANT | VT_BYREF:
    pvar->n1.n2.n3.byref = CoTaskMemAlloc(sizeof(VARIANT));
    Pos = VARIANT_UserUnmarshal(pFlags, Pos, V_VARIANTREF(pvar));
    break;
  case VT_UNKNOWN:
    Pos = unmarshal_interface(pFlags, Pos, extra, (LPVOID*)&V_UNKNOWN(pvar), &IID_IUnknown);
    break;
  case VT_UNKNOWN | VT_BYREF:
    pvar->n1.n2.n3.byref = CoTaskMemAlloc(sizeof(LPUNKNOWN));
    Pos = unmarshal_interface(pFlags, Pos, extra, (LPVOID*)V_UNKNOWNREF(pvar), &IID_IUnknown);
    break;
  case VT_DISPATCH:
    Pos = unmarshal_interface(pFlags, Pos, extra, (LPVOID*)&V_DISPATCH(pvar), &IID_IDispatch);
    break;
  case VT_DISPATCH | VT_BYREF:
    pvar->n1.n2.n3.byref = CoTaskMemAlloc(sizeof(LPDISPATCH));
    Pos = unmarshal_interface(pFlags, Pos, extra, (LPVOID*)V_DISPATCHREF(pvar), &IID_IDispatch);
    break;
  case VT_RECORD:
    FIXME("handle BRECORD by val\n");
    break;
  case VT_RECORD | VT_BYREF:
    FIXME("handle BRECORD by ref\n");
    break;
  default:
    FIXME("handle unknown complex type\n");
    break;
  }
  if (Pos != Buffer + var->clSize) {
    ERR("size difference during unmarshal\n");
  }
  return Buffer + var->clSize;
}

void WINAPI VARIANT_UserFree(ULONG *pFlags, VARIANT *pvar)
{
  VARTYPE vt = V_VT(pvar);
  PVOID ref = NULL;

  TRACE("(%x,%p)\n", *pFlags, pvar);
  TRACE("vt=%04x\n", V_VT(pvar));

  if (vt & VT_BYREF) ref = pvar->n1.n2.n3.byref;

  VariantClear(pvar);
  if (!ref) return;

  switch (vt) {
  case VT_BSTR | VT_BYREF:
    BSTR_UserFree(pFlags, ref);
    break;
  case VT_VARIANT | VT_BYREF:
    VARIANT_UserFree(pFlags, ref);
    break;
  case VT_UNKNOWN | VT_BYREF:
    IUnknown_Release(*(LPUNKNOWN*)ref);
    break;
  case VT_DISPATCH | VT_BYREF:
    IDispatch_Release(*(LPDISPATCH*)ref);
    break;
  case VT_RECORD | VT_BYREF:
    FIXME("handle BRECORD by ref\n");
    break;
  }

  CoTaskMemFree(ref);
}

/* IDispatch */
/* exactly how Invoke is marshalled is not very clear to me yet,
 * but the way I've done it seems to work for me */

HRESULT CALLBACK IDispatch_Invoke_Proxy(
    IDispatch* This,
    DISPID dispIdMember,
    REFIID riid,
    LCID lcid,
    WORD wFlags,
    DISPPARAMS* pDispParams,
    VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo,
    UINT* puArgErr)
{
  HRESULT hr;
  VARIANT VarResult;
  UINT* rgVarRefIdx = NULL;
  VARIANTARG* rgVarRef = NULL;
  UINT u, cVarRef;

  TRACE("(%p)->(%u,%s,%x,%x,%p,%p,%p,%p)\n", This,
        dispIdMember, debugstr_guid(riid),
        lcid, wFlags, pDispParams, pVarResult,
        pExcepInfo, puArgErr);
  TRACE(" DispParams: cArgs=%d, rgvarg=%p\n", pDispParams->cArgs, pDispParams->rgvarg);
  TRACE(" DispParams: cNamedArgs=%d, rgdispidNamedArgs=%p\n", pDispParams->cNamedArgs, pDispParams->rgdispidNamedArgs);
  for (u=0; u<pDispParams->cArgs; u++) {
    VARIANTARG* arg = &pDispParams->rgvarg[u];
    TRACE("  rgvarg[%d]: vt=%d\n", u, V_VT(arg));
  }

  /* [out] args can't be null, use dummy vars if needed */
  if (!pVarResult) pVarResult = &VarResult;

  /* count by-ref args */
  for (cVarRef=0,u=0; u<pDispParams->cArgs; u++) {
    VARIANTARG* arg = &pDispParams->rgvarg[u];
    if (V_VT(arg) & VT_BYREF) {
      cVarRef++;
    }
  }
  if (cVarRef) {
    rgVarRefIdx = CoTaskMemAlloc(sizeof(UINT)*cVarRef);
    rgVarRef = CoTaskMemAlloc(sizeof(VARIANTARG)*cVarRef);
    /* make list of by-ref args */
    for (cVarRef=0,u=0; u<pDispParams->cArgs; u++) {
      VARIANTARG* arg = &pDispParams->rgvarg[u];
      if (V_VT(arg) & VT_BYREF) {
	rgVarRefIdx[cVarRef] = u;
	VariantInit(&rgVarRef[cVarRef]);
	cVarRef++;
      }
    }
  } else {
    /* [out] args still can't be null,
     * but we can point these anywhere in this case,
     * since they won't be written to when cVarRef is 0 */
    rgVarRefIdx = puArgErr;
    rgVarRef = pVarResult;
  }
  TRACE("passed by ref: %d args\n", cVarRef);
  hr = IDispatch_RemoteInvoke_Proxy(This,
				    dispIdMember,
				    riid,
				    lcid,
				    wFlags,
				    pDispParams,
				    pVarResult,
				    pExcepInfo,
				    puArgErr,
				    cVarRef,
				    rgVarRefIdx,
				    rgVarRef);
  if (cVarRef) {
    for (u=0; u<cVarRef; u++) {
      unsigned i = rgVarRefIdx[u];
      VariantCopy(&pDispParams->rgvarg[i],
		  &rgVarRef[u]);
      VariantClear(&rgVarRef[u]);
    }
    CoTaskMemFree(rgVarRef);
    CoTaskMemFree(rgVarRefIdx);
  }
  TRACE("return 0x%08x\n", hr);
  return hr;
}

HRESULT __RPC_STUB IDispatch_Invoke_Stub(
    IDispatch* This,
    DISPID dispIdMember,
    REFIID riid,
    LCID lcid,
    DWORD dwFlags,
    DISPPARAMS* pDispParams,
    VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo,
    UINT* pArgErr,
    UINT cVarRef,
    UINT* rgVarRefIdx,
    VARIANTARG* rgVarRef)
{
  HRESULT hr;
  VARIANTARG *rgvarg, *arg;
  UINT u;

  /* let the real Invoke operate on a copy of the in parameters,
   * so we don't risk losing pointers to allocated memory */
  rgvarg = pDispParams->rgvarg;
  arg = CoTaskMemAlloc(sizeof(VARIANTARG)*pDispParams->cArgs);
  for (u=0; u<pDispParams->cArgs; u++) {
    VariantInit(&arg[u]);
    VariantCopy(&arg[u], &rgvarg[u]);
  }
  pDispParams->rgvarg = arg;

  /* initialize out parameters, so that they can be marshalled
   * in case the real Invoke doesn't initialize them */
  VariantInit(pVarResult);
  memset(pExcepInfo, 0, sizeof(*pExcepInfo));
  *pArgErr = 0;

  hr = IDispatch_Invoke(This,
			dispIdMember,
			riid,
			lcid,
			dwFlags,
			pDispParams,
			pVarResult,
			pExcepInfo,
			pArgErr);

  /* copy ref args to out list */
  for (u=0; u<cVarRef; u++) {
    unsigned i = rgVarRefIdx[u];
    VariantInit(&rgVarRef[u]);
    VariantCopy(&rgVarRef[u], &arg[i]);
    /* clear original if equal, to avoid double-free */
    if (V_BYREF(&rgVarRef[u]) == V_BYREF(&rgvarg[i]))
      VariantClear(&rgvarg[i]);
  }
  /* clear the duplicate argument list */
  for (u=0; u<pDispParams->cArgs; u++) {
    VariantClear(&arg[u]);
  }
  pDispParams->rgvarg = rgvarg;
  CoTaskMemFree(arg);

  return hr;
}

/* IEnumVARIANT */

HRESULT CALLBACK IEnumVARIANT_Next_Proxy(
    IEnumVARIANT* This,
    ULONG celt,
    VARIANT* rgVar,
    ULONG* pCeltFetched)
{
  ULONG fetched;
  if (!pCeltFetched)
    pCeltFetched = &fetched;
  return IEnumVARIANT_RemoteNext_Proxy(This,
				       celt,
				       rgVar,
				       pCeltFetched);
}

HRESULT __RPC_STUB IEnumVARIANT_Next_Stub(
    IEnumVARIANT* This,
    ULONG celt,
    VARIANT* rgVar,
    ULONG* pCeltFetched)
{
  HRESULT hr;
  *pCeltFetched = 0;
  hr = IEnumVARIANT_Next(This,
			 celt,
			 rgVar,
			 pCeltFetched);
  if (hr == S_OK) *pCeltFetched = celt;
  return hr;
}

/* ITypeComp */

HRESULT CALLBACK ITypeComp_Bind_Proxy(
    ITypeComp* This,
    LPOLESTR szName,
    ULONG lHashVal,
    WORD wFlags,
    ITypeInfo** ppTInfo,
    DESCKIND* pDescKind,
    BINDPTR* pBindPtr)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeComp_Bind_Stub(
    ITypeComp* This,
    LPOLESTR szName,
    ULONG lHashVal,
    WORD wFlags,
    ITypeInfo** ppTInfo,
    DESCKIND* pDescKind,
    LPFUNCDESC* ppFuncDesc,
    LPVARDESC* ppVarDesc,
    ITypeComp** ppTypeComp,
    CLEANLOCALSTORAGE* pDummy)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeComp_BindType_Proxy(
    ITypeComp* This,
    LPOLESTR szName,
    ULONG lHashVal,
    ITypeInfo** ppTInfo,
    ITypeComp** ppTComp)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeComp_BindType_Stub(
    ITypeComp* This,
    LPOLESTR szName,
    ULONG lHashVal,
    ITypeInfo** ppTInfo)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

/* ITypeInfo */

HRESULT CALLBACK ITypeInfo_GetTypeAttr_Proxy(
    ITypeInfo* This,
    TYPEATTR** ppTypeAttr)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetTypeAttr_Stub(
    ITypeInfo* This,
    LPTYPEATTR* ppTypeAttr,
    CLEANLOCALSTORAGE* pDummy)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetFuncDesc_Proxy(
    ITypeInfo* This,
    UINT index,
    FUNCDESC** ppFuncDesc)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetFuncDesc_Stub(
    ITypeInfo* This,
    UINT index,
    LPFUNCDESC* ppFuncDesc,
    CLEANLOCALSTORAGE* pDummy)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetVarDesc_Proxy(
    ITypeInfo* This,
    UINT index,
    VARDESC** ppVarDesc)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetVarDesc_Stub(
    ITypeInfo* This,
    UINT index,
    LPVARDESC* ppVarDesc,
    CLEANLOCALSTORAGE* pDummy)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetNames_Proxy(
    ITypeInfo* This,
    MEMBERID memid,
    BSTR* rgBstrNames,
    UINT cMaxNames,
    UINT* pcNames)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetNames_Stub(
    ITypeInfo* This,
    MEMBERID memid,
    BSTR* rgBstrNames,
    UINT cMaxNames,
    UINT* pcNames)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetIDsOfNames_Proxy(
    ITypeInfo* This,
    LPOLESTR* rgszNames,
    UINT cNames,
    MEMBERID* pMemId)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetIDsOfNames_Stub(
    ITypeInfo* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_Invoke_Proxy(
    ITypeInfo* This,
    PVOID pvInstance,
    MEMBERID memid,
    WORD wFlags,
    DISPPARAMS* pDispParams,
    VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo,
    UINT* puArgErr)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_Invoke_Stub(
    ITypeInfo* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetDocumentation_Proxy(
    ITypeInfo* This,
    MEMBERID memid,
    BSTR* pBstrName,
    BSTR* pBstrDocString,
    DWORD* pdwHelpContext,
    BSTR* pBstrHelpFile)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetDocumentation_Stub(
    ITypeInfo* This,
    MEMBERID memid,
    DWORD refPtrFlags,
    BSTR* pBstrName,
    BSTR* pBstrDocString,
    DWORD* pdwHelpContext,
    BSTR* pBstrHelpFile)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetDllEntry_Proxy(
    ITypeInfo* This,
    MEMBERID memid,
    INVOKEKIND invKind,
    BSTR* pBstrDllName,
    BSTR* pBstrName,
    WORD* pwOrdinal)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetDllEntry_Stub(
    ITypeInfo* This,
    MEMBERID memid,
    INVOKEKIND invKind,
    DWORD refPtrFlags,
    BSTR* pBstrDllName,
    BSTR* pBstrName,
    WORD* pwOrdinal)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_AddressOfMember_Proxy(
    ITypeInfo* This,
    MEMBERID memid,
    INVOKEKIND invKind,
    PVOID* ppv)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_AddressOfMember_Stub(
    ITypeInfo* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_CreateInstance_Proxy(
    ITypeInfo* This,
    IUnknown* pUnkOuter,
    REFIID riid,
    PVOID* ppvObj)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_CreateInstance_Stub(
    ITypeInfo* This,
    REFIID riid,
    IUnknown** ppvObj)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeInfo_GetContainingTypeLib_Proxy(
    ITypeInfo* This,
    ITypeLib** ppTLib,
    UINT* pIndex)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo_GetContainingTypeLib_Stub(
    ITypeInfo* This,
    ITypeLib** ppTLib,
    UINT* pIndex)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

void CALLBACK ITypeInfo_ReleaseTypeAttr_Proxy(
    ITypeInfo* This,
    TYPEATTR* pTypeAttr)
{
  FIXME("not implemented\n");
}

HRESULT __RPC_STUB ITypeInfo_ReleaseTypeAttr_Stub(
    ITypeInfo* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

void CALLBACK ITypeInfo_ReleaseFuncDesc_Proxy(
    ITypeInfo* This,
    FUNCDESC* pFuncDesc)
{
  FIXME("not implemented\n");
}

HRESULT __RPC_STUB ITypeInfo_ReleaseFuncDesc_Stub(
    ITypeInfo* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

void CALLBACK ITypeInfo_ReleaseVarDesc_Proxy(
    ITypeInfo* This,
    VARDESC* pVarDesc)
{
  FIXME("not implemented\n");
}

HRESULT __RPC_STUB ITypeInfo_ReleaseVarDesc_Stub(
    ITypeInfo* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}


/* ITypeInfo2 */

HRESULT CALLBACK ITypeInfo2_GetDocumentation2_Proxy(
    ITypeInfo2* This,
    MEMBERID memid,
    LCID lcid,
    BSTR* pbstrHelpString,
    DWORD* pdwHelpStringContext,
    BSTR* pbstrHelpStringDll)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeInfo2_GetDocumentation2_Stub(
    ITypeInfo2* This,
    MEMBERID memid,
    LCID lcid,
    DWORD refPtrFlags,
    BSTR* pbstrHelpString,
    DWORD* pdwHelpStringContext,
    BSTR* pbstrHelpStringDll)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

/* ITypeLib */

UINT CALLBACK ITypeLib_GetTypeInfoCount_Proxy(
    ITypeLib* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib_GetTypeInfoCount_Stub(
    ITypeLib* This,
    UINT* pcTInfo)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeLib_GetLibAttr_Proxy(
    ITypeLib* This,
    TLIBATTR** ppTLibAttr)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib_GetLibAttr_Stub(
    ITypeLib* This,
    LPTLIBATTR* ppTLibAttr,
    CLEANLOCALSTORAGE* pDummy)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeLib_GetDocumentation_Proxy(
    ITypeLib* This,
    INT index,
    BSTR* pBstrName,
    BSTR* pBstrDocString,
    DWORD* pdwHelpContext,
    BSTR* pBstrHelpFile)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib_GetDocumentation_Stub(
    ITypeLib* This,
    INT index,
    DWORD refPtrFlags,
    BSTR* pBstrName,
    BSTR* pBstrDocString,
    DWORD* pdwHelpContext,
    BSTR* pBstrHelpFile)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeLib_IsName_Proxy(
    ITypeLib* This,
    LPOLESTR szNameBuf,
    ULONG lHashVal,
    BOOL* pfName)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib_IsName_Stub(
    ITypeLib* This,
    LPOLESTR szNameBuf,
    ULONG lHashVal,
    BOOL* pfName,
    BSTR* pBstrLibName)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeLib_FindName_Proxy(
    ITypeLib* This,
    LPOLESTR szNameBuf,
    ULONG lHashVal,
    ITypeInfo** ppTInfo,
    MEMBERID* rgMemId,
    USHORT* pcFound)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib_FindName_Stub(
    ITypeLib* This,
    LPOLESTR szNameBuf,
    ULONG lHashVal,
    ITypeInfo** ppTInfo,
    MEMBERID* rgMemId,
    USHORT* pcFound,
    BSTR* pBstrLibName)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

void CALLBACK ITypeLib_ReleaseTLibAttr_Proxy(
    ITypeLib* This,
    TLIBATTR* pTLibAttr)
{
  FIXME("not implemented\n");
}

HRESULT __RPC_STUB ITypeLib_ReleaseTLibAttr_Stub(
    ITypeLib* This)
{
  FIXME("not implemented\n");
  return E_FAIL;
}


/* ITypeLib2 */

HRESULT CALLBACK ITypeLib2_GetLibStatistics_Proxy(
    ITypeLib2* This,
    ULONG* pcUniqueNames,
    ULONG* pcchUniqueNames)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib2_GetLibStatistics_Stub(
    ITypeLib2* This,
    ULONG* pcUniqueNames,
    ULONG* pcchUniqueNames)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT CALLBACK ITypeLib2_GetDocumentation2_Proxy(
    ITypeLib2* This,
    INT index,
    LCID lcid,
    BSTR* pbstrHelpString,
    DWORD* pdwHelpStringContext,
    BSTR* pbstrHelpStringDll)
{
  FIXME("not implemented\n");
  return E_FAIL;
}

HRESULT __RPC_STUB ITypeLib2_GetDocumentation2_Stub(
    ITypeLib2* This,
    INT index,
    LCID lcid,
    DWORD refPtrFlags,
    BSTR* pbstrHelpString,
    DWORD* pdwHelpStringContext,
    BSTR* pbstrHelpStringDll)
{
  FIXME("not implemented\n");
  return E_FAIL;
}
