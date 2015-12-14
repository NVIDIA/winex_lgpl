/*
 * Typelib Marshaler
 *
 * Copyright 2001 TransGaming Technologies, Ove Kåven
 */

#include "config.h"

#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "wine/debug.h"
#include "ole2.h"
#include "oleauto.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

static HRESULT OA_LoadInterfaceTypeLib(REFIID riid, LPTYPELIB *ppTlib)
{
  HKEY key;
  char keyName[100], ver[100];
  WCHAR keyVal[100];
  int Maj, Min;
  GUID iid;
  DWORD len;
  HRESULT hr;
  sprintf(keyName, "Interface\\{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\TypeLib",
	  riid->Data1, riid->Data2, riid->Data3,
	  riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3],
	  riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7]);
  if (RegCreateKeyExA(HKEY_CLASSES_ROOT,keyName,0,NULL,0,KEY_READ,NULL,&key,NULL)) {
    ERR("typelib entry not found for interface %s\n", debugstr_guid(riid));
    return E_FAIL;
  }
  len = sizeof(keyVal);
  if (RegQueryValueExW(key,NULL,NULL,NULL,(LPBYTE)&keyVal,&len)) {
    ERR("typelib GUID not found for interface %s\n", debugstr_guid(riid));
    RegCloseKey(key);
    return E_FAIL;
  }
  hr = CLSIDFromString(keyVal, &iid);
  if (FAILED(hr)) {
    ERR("unparsable typelib GUID for interface %s\n", debugstr_guid(riid));
    RegCloseKey(key);
    return hr;
  }
  len = sizeof(ver);
  if (RegQueryValueExA(key,"Version",NULL,NULL,ver,&len)) {
    ERR("typelib version not found for interface %s\n", debugstr_guid(riid));
    RegCloseKey(key);
    return hr;
  }
  if (sscanf(ver,"%d.%d",&Maj,&Min)!=2) {
    ERR("unparsable typelib version for interface %s\n", debugstr_guid(riid));
    RegCloseKey(key);
    return hr;
  }
  RegCloseKey(key);
  return LoadRegTypeLib(&iid, Maj, Min, 0, ppTlib);
}

/******************** MARSHALLER ********************/
/* FIXME: should layer on top of rpcrt4 */

static DWORD TL_SizeOfType(LPTYPEINFO pInfo,
                           TYPEDESC *pType)
{
  TRACE( "%p %p(vt=%d)\n", pInfo, pType, pType->vt );

  switch (pType->vt) {
  case VT_I1:
  case VT_UI1:
    return sizeof(BYTE);
  case VT_I2:
  case VT_UI2:
    return sizeof(WORD);
  case VT_I4:
  case VT_UI4:
  case VT_INT:
  case VT_UINT:
    return sizeof(DWORD);
  case VT_BOOL:
    return sizeof(VARIANT_BOOL);
  case VT_VARIANT:
    return sizeof(VARIANT);
  case VT_BSTR:
  case VT_PTR:
  case VT_DISPATCH:
  case VT_UNKNOWN:
    return sizeof(LPVOID);
  case VT_USERDEFINED:
    {
      HREFTYPE hType = pType->u.hreftype;
      LPTYPEINFO pTInfo;
      LPTYPEATTR pAttr;
      DWORD size = 0;
      if (FAILED(ITypeInfo_GetRefTypeInfo(pInfo, hType, &pTInfo))) return 0;
      if (FAILED(ITypeInfo_GetTypeAttr(pTInfo, &pAttr))) {
        ITypeInfo_Release(pTInfo);
        return 0;
      }
      TRACE( "VT_USERDEFINED of kind %d\n", pAttr->typekind );
      switch (pAttr->typekind) {
      case TKIND_RECORD:
      case TKIND_ENUM:
        size = pAttr->cbSizeInstance;
        break;
      case TKIND_INTERFACE:
        size = 0; /* must not be handled here */
        break;
      case TKIND_ALIAS:
        size = TL_SizeOfType( pTInfo, &pAttr->tdescAlias );
        break;
      default:
        ERR("unknown size for typekind %d\n", pAttr->typekind);
        break;
      }
      ITypeInfo_ReleaseTypeAttr(pTInfo, pAttr);
      ITypeInfo_Release(pTInfo);
      return size;
    }
    break;
  default:
    FIXME("unknown type size for type %d\n", pType->vt);
  case VT_VOID:
    return 0;
  }
}

/* Size in DWORDs rounded up */
static inline DWORD TL_ArgSiz(DWORD size)
{
  return (size+(sizeof(DWORD)-1))/sizeof(DWORD);
}

static WCHAR magic_guid[] = {'G','U','I','D',0};
static WCHAR magic_riid[] = {'r','i','i','d',0};

static HRESULT TL_MarshalType(LPSTREAM pStm,
			      LPTYPEINFO pInfo,
			      TYPEDESC *pType,
			      DWORD *args,
			      BOOL first,
			      GUID *is_iid)
{
  HRESULT hr = S_OK;
  switch (pType->vt) {
  case VT_I1:
  case VT_UI1:
    {
      BYTE v = *(BYTE*)args;
      TRACE("   marshaling byte %d\n", v);
      IStream_Write(pStm, &v, sizeof(v), NULL);
    }
    break;
  case VT_I2:
  case VT_UI2:
    {
      WORD v = *(WORD*)args;
      TRACE("   marshaling short %d\n", v);
      IStream_Write(pStm, &v, sizeof(v), NULL);
    }
    break;
  case VT_I4:
  case VT_UI4:
  case VT_INT:
  case VT_UINT:
    {
      DWORD v = *(DWORD*)args;
      TRACE("   marshaling int %ld\n", v);
      IStream_Write(pStm, &v, sizeof(v), NULL);
    }
    break;
  case VT_BOOL:
    {
      VARIANT_BOOL v = *(VARIANT_BOOL*)args;
      TRACE("   marshaling bool %d\n", v);
      IStream_Write(pStm, &v, sizeof(v), NULL);
    }
    break;
  case VT_VARIANT:
    {
      LPVARIANT v = (LPVARIANT)args;
      TYPEDESC vType;
      vType.vt = V_VT(v);
      vType.u.hreftype = 0;
      TRACE("   marshaling VARIANT %p type %d\n", v, vType.vt);
      IStream_Write(pStm, &vType.vt, sizeof(vType.vt), NULL);
      hr = TL_MarshalType(pStm, pInfo, &vType, (DWORD*)&V_UNION(v,cVal), FALSE, is_iid);
    }
    break;
  case VT_BSTR:
    {
      BSTR v = *(BSTR*)args;
      int len = SysStringLen(v);
      TRACE("   marshaling BSTR %s\n", debugstr_w(v));
      IStream_Write(pStm, &len, sizeof(len), NULL);
      if (len) IStream_Write(pStm, v, len*sizeof(OLECHAR), NULL);
    }
    break;
  case VT_PTR:
    {
      LPVOID v = *(LPVOID*)args;
      DWORD valid = (v != NULL);
      TRACE("   dereferencing PTR %p <= %p\n", v, args);
      IStream_Write(pStm, &valid, sizeof(valid), NULL);
      if (valid) hr = TL_MarshalType(pStm, pInfo, pType->u.lptdesc, (DWORD*)v, FALSE, is_iid);
    }
    break;
  case VT_DISPATCH:
  case VT_UNKNOWN:
  case VT_VOID: /* <= InstallShield hack */
    {
      LPVOID pv = *(LPVOID*)args;
      const IID *piid;
      if (pType->vt == VT_DISPATCH) piid = &IID_IDispatch; else
      if (pType->vt == VT_UNKNOWN) piid = &IID_IUnknown; else
      {
        pv = (LPVOID)args;
        piid = is_iid;
      }
      TRACE("   marshaling INTERFACE %p %s\n", pv, debugstr_guid(piid));
      hr = CoMarshalInterface(pStm, piid, pv, CLSCTX_LOCAL_SERVER, NULL, MSHLFLAGS_NORMAL);
    }
    break;
  case VT_USERDEFINED:
    {
      LPVOID pv = (LPVOID)args;
      HREFTYPE hType = pType->u.hreftype;
      LPTYPEINFO pTInfo;
      LPTYPEATTR pAttr;
      BSTR tname = NULL;
      BOOL is_guid = FALSE;
      TRACE("   reftype: %ld\n", hType);
      hr = ITypeInfo_GetRefTypeInfo(pInfo, hType, &pTInfo);
      if (FAILED(hr)) {
        ERR("failed to retrieve reftype %ld\n", hType);
        return hr;
      }
      ITypeInfo_GetDocumentation(pTInfo, -1, &tname, NULL, NULL, NULL);
      if (tname) {
        TRACE("   name   : %s\n", debugstr_w(tname));
        is_guid = tname ? (lstrcmpW(tname,magic_guid)==0) : FALSE;
        SysFreeString(tname);
      }
      hr = ITypeInfo_GetTypeAttr(pTInfo, &pAttr);
      if (FAILED(hr)) {
        ITypeInfo_Release(pTInfo);
        return hr;
      }
      TRACE("   kind   : %d\n", pAttr->typekind);
      switch (pAttr->typekind) {
      case TKIND_RECORD:
        {
	      /* FIXME: hope that the record contains no pointers */
	      WARN("   marshaling RECORD %p [size=%ld]\n", pv, pAttr->cbSizeInstance);
	      if (is_guid) TRACE("    guid=%s\n", debugstr_guid((GUID*)pv));
	      IStream_Write(pStm, pv, pAttr->cbSizeInstance, NULL);
        }
        break;
      case TKIND_INTERFACE:
        {
          TRACE("   marshaling INTERFACE %p %s\n", pv, debugstr_guid(&pAttr->guid));
          hr = CoMarshalInterface(pStm, &pAttr->guid, pv, CLSCTX_LOCAL_SERVER, NULL, MSHLFLAGS_NORMAL);
        }
	    break;
      case TKIND_ENUM:
        {
	      TRACE("   marshaling ENUM %p [size=%ld]\n", pv, pAttr->cbSizeInstance);
	      IStream_Write(pStm, pv, pAttr->cbSizeInstance, NULL);
        }
        break;
      default:
	    ERR("unhandled typekind %d\n", pAttr->typekind);
	    break;
      }
      ITypeInfo_ReleaseTypeAttr(pTInfo, pAttr);
      ITypeInfo_Release(pTInfo);
    }
    break;
  default:
    FIXME("marshal type %d\n", pType->vt);
  }
  return hr;
}

static HRESULT TL_CleanupType(LPTYPEINFO pInfo,
			      TYPEDESC *pType,
			      DWORD *args,
			      BOOL first)
{
  HRESULT hr = S_OK;
  switch (pType->vt) {
  case VT_VARIANT:
    {
      LPVARIANT v = (LPVARIANT)args;
#if 0
      TYPEDESC vType;
      vType.vt = V_VT(v);
      vType.u.hreftype = 0;
#endif
      TRACE("   freeing VARIANT %p type %d\n", v, V_VT(v));
#if 0
      hr = TL_CleanupType(pInfo, &vType, (DWORD*)&V_UNION(v,cVal), FALSE);
#endif
      VariantClear(v);
    }
    break;
  case VT_BSTR:
    {
      BSTR v = *(BSTR*)args;
      TRACE("   freeing BSTR %p\n", v);
      SysFreeString(v);
    }
    break;
  case VT_PTR:
    {
      LPVOID v = *(LPVOID*)args;
      TRACE("   freeing PTR %p\n", v);
      if (v) {
        hr = TL_CleanupType(pInfo, pType->u.lptdesc, (DWORD*)v, FALSE);
        if (hr == S_OK) CoTaskMemFree(v);
      }
    }
    break;
  case VT_DISPATCH:
  case VT_UNKNOWN:
    {
      LPVOID pv = *(LPVOID*)args;
      if (pv) IUnknown_Release((LPUNKNOWN)pv);
    }
    break;
  case VT_VOID: /* <= InstallShield hack */
    {
      LPVOID pv = (LPVOID)args;
      if (pv) IUnknown_Release((LPUNKNOWN)pv);
      hr = S_FALSE;
    }
    break;
  case VT_USERDEFINED:
    {
      LPVOID pv = (LPVOID)args;
      HREFTYPE hType = pType->u.hreftype;
      LPTYPEINFO pTInfo;
      LPTYPEATTR pAttr;
      if (FAILED(ITypeInfo_GetRefTypeInfo(pInfo, hType, &pTInfo))) return 0;
      if (FAILED(ITypeInfo_GetTypeAttr(pTInfo, &pAttr))) {
        ITypeInfo_Release(pTInfo);
        return 0;
      }
      switch (pAttr->typekind) {
      case TKIND_ENUM:
        break;
      case TKIND_RECORD:
        /* hope that the record contains no pointers */
        break;
      case TKIND_INTERFACE:
        if (pv) IUnknown_Release((LPUNKNOWN)pv);
        hr = S_FALSE;
        break;
      default:
        ERR("unknown cleanup for typekind %d\n", pAttr->typekind);
        break;
      }
      ITypeInfo_ReleaseTypeAttr(pTInfo, pAttr);
      ITypeInfo_Release(pTInfo);
    }
    break;
  default:
    break;
  }
  return hr;
}

static HRESULT TL_Marshal(LPSTREAM pStm,
			  LPTYPEINFO pInfo,
			  LPFUNCDESC pDesc,
			  DWORD dwFlag,
			  DWORD *args,
			  HRESULT res,
			  GUID *is_iid)
{
  int p, cc;
  BSTR names[16];
  UINT mnames = 0;
  BOOL is_riid;
  TRACE(" Marshaling %p\n", args);
  args++;
  ITypeInfo_GetNames(pInfo, pDesc->memid, names, 16, &mnames);
  if (mnames) SysFreeString(names[0]);
  for (p=0; p<pDesc->cParams; p++) {
    ELEMDESC *par = &pDesc->lprgelemdescParam[p];
    USHORT wFlag = par->u.paramdesc.wParamFlags;
    TRACE("  parameter %d\n", p);
    if (p<=mnames) {
      TRACE("   name   : %s\n", debugstr_w(names[p+1]));
      is_riid = names[p+1] ? (lstrcmpW(names[p+1],magic_riid)==0) : FALSE;
      SysFreeString(names[p+1]);
    }
    else is_riid = FALSE;
    TRACE("   type   : %d\n", par->tdesc.vt);
    TRACE("   flags  : %02x\n", wFlag);
    if (par->u.paramdesc.pparamdescex)
      TRACE("   bytes  : %ld\n", par->u.paramdesc.pparamdescex->cBytes);
    if (is_riid) {
      memcpy(is_iid, *(GUID**)args, sizeof(GUID));
      TRACE(" is_riid=%s\n", debugstr_guid(is_iid));
    }
    if (wFlag & dwFlag) {
      TL_MarshalType(pStm, pInfo, &par->tdesc, args, FALSE, is_iid);
    }
    if (dwFlag & PARAMFLAG_FOUT) {
      /* cleanup after Unmarshal etc */
      TL_CleanupType(pInfo, &par->tdesc, args, TRUE);
    }
    cc = TL_ArgSiz(TL_SizeOfType(pInfo, &par->tdesc));
    args += cc;
  }
  if (dwFlag & PARAMFLAG_FOUT) {
    /* marshal HRESULT */
    IStream_Write(pStm, &res, sizeof(res), NULL);
  }
  return S_OK;
}

static HRESULT TL_UnmarshalType(LPSTREAM pStm,
				LPTYPEINFO pInfo,
				TYPEDESC *pType,
				DWORD *args,
				BOOL first,
	 			LPVOID *argp,
 				LPRPCCHANNELBUFFER chan,
 				GUID *is_iid)
{
  HRESULT hr = S_OK;
  switch (pType->vt) {
  case VT_I1:
  case VT_UI1:
    {
      BYTE v;
      IStream_Read(pStm, &v, sizeof(v), NULL);
      TRACE("   unmarshaled byte %d\n", v);
      *args = v;
    }
    break;
  case VT_I2:
  case VT_UI2:
    {
      WORD v;
      IStream_Read(pStm, &v, sizeof(v), NULL);
      TRACE("   unmarshaled short %d\n", v);
      *args = v;
    }
    break;
  case VT_I4:
  case VT_UI4:
  case VT_INT:
  case VT_UINT:
    {
      DWORD v;
      IStream_Read(pStm, &v, sizeof(v), NULL);
      TRACE("   unmarshaled int %ld\n", v);
      *args = v;
    }
    break;
  case VT_BOOL:
    {
      VARIANT_BOOL v;
      IStream_Read(pStm, &v, sizeof(v), NULL);
      TRACE("   unmarshaled bool %d\n", v);
      *args = v;
    }
    break;
  case VT_VARIANT:
    {
      LPVARIANT v = (LPVARIANT)args;
      TYPEDESC vType;
      vType.vt = VT_EMPTY;
      vType.u.hreftype = 0;
      IStream_Read(pStm, &vType.vt, sizeof(vType.vt), NULL);
      TRACE("   variant type: %d\n", vType.vt);
      VariantInit(v);
      V_VT(v) = vType.vt;
      hr = TL_UnmarshalType(pStm, pInfo, &vType, (DWORD*)&V_UNION(v,cVal), FALSE, NULL, chan, is_iid);
    }
    break;
  case VT_BSTR:
    {
      BSTR v;
      int len;
      IStream_Read(pStm, &len, sizeof(len), NULL);
      v = SysAllocStringLen(NULL, len);
      if (len) IStream_Read(pStm, v, len*sizeof(OLECHAR), NULL);
      TRACE("   unmarshaled BSTR %s => %p\n", debugstr_w(v), v);
      *(BSTR*)args = v;
    }
    break;
  case VT_PTR:
    {
      LPVOID v;
      DWORD valid;
      IStream_Read(pStm, &valid, sizeof(valid), NULL);
      if (first) {
	v = *(LPVOID*)args;
	TRACE("   dereferenced PTR %p <= %p\n", v, args);
      }
      else {
	DWORD size = valid ? TL_SizeOfType(pInfo, pType->u.lptdesc) : 0;
	if (size) v = CoTaskMemAlloc(size);
	else v = NULL;
	*(LPVOID*)args = v;
	TRACE("   allocated PTR [size=%ld] => %p => %p\n", size, v, args);
      }
      if (valid) hr = TL_UnmarshalType(pStm, pInfo, pType->u.lptdesc, (DWORD*)v, FALSE, (LPVOID*)args, chan, is_iid);
    }
    break;
  case VT_DISPATCH:
  case VT_UNKNOWN:
  case VT_VOID: /* <= InstallShield hack */
    {
      LPVOID *iptr = (LPVOID*)args;
      const IID *piid;
      if (pType->vt == VT_DISPATCH) piid = &IID_IDispatch; else
      if (pType->vt == VT_UNKNOWN) piid = &IID_IUnknown; else
      {
        iptr = argp ? argp : (LPVOID*)args;
        piid = is_iid;
      }
      hr = CoUnmarshalInterface(pStm, piid, iptr);
      TRACE("   unmarshaled INTERFACE %s => %p => %p\n", debugstr_guid(piid), *iptr, iptr);
    }
    break;
  case VT_USERDEFINED:
    {
      LPVOID pv = (LPVOID)args;
      HREFTYPE hType = pType->u.hreftype;
      LPTYPEINFO pTInfo;
      LPTYPEATTR pAttr;
      BSTR tname = NULL;
      BOOL is_guid = FALSE;
      TRACE("   reftype: %ld\n", hType);
      hr = ITypeInfo_GetRefTypeInfo(pInfo, hType, &pTInfo);
      if (FAILED(hr)) {
        ERR("failed to retrieve reftype %ld\n", hType);
        return hr;
      }
      ITypeInfo_GetDocumentation(pTInfo, -1, &tname, NULL, NULL, NULL);
      if (tname) {
        TRACE("   name   : %s\n", debugstr_w(tname));
        is_guid = tname ? (lstrcmpW(tname,magic_guid)==0) : FALSE;
        SysFreeString(tname);
      }
      hr = ITypeInfo_GetTypeAttr(pTInfo, &pAttr);
      if (FAILED(hr)) {
        ITypeInfo_Release(pTInfo);
        return hr;
      }
      TRACE("   kind   : %d\n", pAttr->typekind);
      switch (pAttr->typekind) {
      case TKIND_RECORD:
        {
          /* FIXME: hope that the record contains no pointers */
          IStream_Read(pStm, pv, pAttr->cbSizeInstance, NULL);
          WARN("   unmarshaled RECORD %p [size=%ld]\n", pv, pAttr->cbSizeInstance);
          if (is_guid) TRACE("    guid=%s\n", debugstr_guid((GUID*)pv));
        }
        break;
      case TKIND_ENUM:
        {
          IStream_Read(pStm, pv, pAttr->cbSizeInstance, NULL);
          TRACE("   unmarshaled ENUM %p [size=%ld]\n", pv, pAttr->cbSizeInstance);
        }
        break;
      case TKIND_INTERFACE:
	    {
          LPVOID *iptr = argp ? argp : (LPVOID*)args;
          hr = CoUnmarshalInterface(pStm, &pAttr->guid, iptr);
          TRACE("   unmarshaled INTERFACE %s => %p => %p\n", debugstr_guid(&pAttr->guid), *iptr, iptr);
        }
        break;
      default:
        ERR("unhandled typekind %d\n", pAttr->typekind);
        break;
      }
      ITypeInfo_ReleaseTypeAttr(pTInfo, pAttr);
      ITypeInfo_Release(pTInfo);
    }
    break;
  default:
    FIXME("unmarshal type %d\n", pType->vt);
  }
  return hr;
}

static HRESULT TL_Unmarshal(LPSTREAM pStm,
			    LPTYPEINFO pInfo,
			    LPFUNCDESC pDesc,
			    DWORD dwFlag,
			    DWORD *args,
			    HRESULT *ret,
			    LPRPCCHANNELBUFFER chan,
			    GUID *is_iid)
{
  int p, pc, cc;
  BSTR names[16];
  UINT mnames = 0;
  BOOL is_riid;
  TRACE(" Unmarshaling %p\n", args);
  args++; pc=1;
  ITypeInfo_GetNames(pInfo, pDesc->memid, names, 16, &mnames);
  if (mnames) SysFreeString(names[0]);
  for (p=0; p<pDesc->cParams; p++) {
    ELEMDESC *par = &pDesc->lprgelemdescParam[p];
    USHORT wFlag = par->u.paramdesc.wParamFlags;
    TRACE("  parameter %d\n", p);
    if (p<=mnames) {
      TRACE("   name   : %s\n", debugstr_w(names[p+1]));
      is_riid = names[p+1] ? (lstrcmpW(names[p+1],magic_riid)==0) : FALSE;
      SysFreeString(names[p+1]);
    }
    else is_riid = FALSE;
    TRACE("   type   : %d\n", par->tdesc.vt);
    TRACE("   flags  : %02x\n", wFlag);
    if (par->u.paramdesc.pparamdescex)
      TRACE("   bytes  : %ld\n", par->u.paramdesc.pparamdescex->cBytes);
    if (wFlag & dwFlag) {
      TL_UnmarshalType(pStm, pInfo, &par->tdesc, args, !(dwFlag & PARAMFLAG_FIN), NULL, chan, is_iid);
    }
    else if (dwFlag & PARAMFLAG_FIN) {
      /* for out parameters, we must still initialize pointers */
      switch (par->tdesc.vt) {
      case VT_PTR:
	{
	  TYPEDESC *pType = par->tdesc.u.lptdesc;
	  DWORD size = TL_SizeOfType(pInfo, pType);
	  LPVOID v = CoTaskMemAlloc(size);
	  TRACE("   initialize PTR [size=%ld] type=%d => %p\n", size, pType->vt, v);
	  memset(v, 0, size); /* just in case */
	  *(LPVOID*)args = v;
	}
	break;
      default:
        break;
      }
    }
    if (is_riid) {
      memcpy(is_iid, *(GUID**)args, sizeof(GUID));
      TRACE(" is_riid=%s\n", debugstr_guid(is_iid));
    }
    cc = TL_ArgSiz(TL_SizeOfType(pInfo, &par->tdesc));
    args += cc; pc += cc;
  }
  if (dwFlag & PARAMFLAG_FOUT) {
    /* unmarshal HRESULT */
    IStream_Read(pStm, ret, sizeof(*ret), NULL);
  }
  else *ret = pc;
  return S_OK;
}

static HRESULT TL_GetInterface(LPTYPEINFO *ppInfo, TYPEKIND *kind)
{
  LPTYPEATTR pAttr;
  IID iid;
  HRESULT hr;

  hr = ITypeInfo_GetTypeAttr(*ppInfo, &pAttr);
  if (FAILED(hr)) {
    ERR("could not get type attr for interface\n");
    return hr;
  }
  memcpy(&iid, &pAttr->guid, sizeof(iid));
  *kind = pAttr->typekind;
  ITypeInfo_ReleaseTypeAttr(*ppInfo, pAttr);

  if (*kind == TKIND_DISPATCH) {
    HREFTYPE hType;
    /* if this is a dual interface, look for TKIND_INTERFACE part,
     * otherwise just return the dispinterface */
    hr = ITypeInfo_GetRefTypeOfImplType(*ppInfo, -1, &hType);
    if (SUCCEEDED(hr)) {
      LPTYPEINFO pDInfo;
      hr = ITypeInfo_GetRefTypeInfo(*ppInfo, hType, &pDInfo);
      if (SUCCEEDED(hr)) {
        TRACE("using real interface (%ld) for IID %s\n", hType, debugstr_guid(&iid));
        ITypeInfo_Release(*ppInfo);
        *ppInfo = pDInfo;
        /* this is supposed to be is an interface.. */
        *kind = TKIND_INTERFACE;
      } else {
        TRACE("failed to locate real interface (%ld) for IID %s\n", hType, debugstr_guid(&iid));
      }
    } else {
      TRACE("no real interface for IID %s\n", debugstr_guid(&iid));
    }
  } else
  if (*kind != TKIND_INTERFACE) {
    ERR("invalid type kind %d for IID %s\n", *kind, debugstr_guid(&iid));
    return E_FAIL;
  }

  return S_OK;
}


/******************** STUB ********************/
/* FIXME: should layer on top of rpcrt4 */

typedef struct {
  ICOM_VTABLE(IRpcStubBuffer) *lpVtbl;
  DWORD ref;
  IID iid;
  LPUNKNOWN pUnkServer;
  LPTYPEINFO pInfo;
  int fs;
} PSOAStubImpl;

static HRESULT WINAPI PSOAStub_QueryInterface(LPRPCSTUBBUFFER iface,
					      REFIID riid,
					      LPVOID *obj)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IRpcStubBuffer,riid)) {
    *obj = This;
    This->ref++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI PSOAStub_AddRef(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->AddRef()\n",This);
  return ++(This->ref);
}

static ULONG WINAPI PSOAStub_Release(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->Release()\n",This);
  if (!--(This->ref)) {
    DWORD rc = IUnknown_Release(This->pUnkServer);
    TRACE("server=%p, refcount=%ld\n", This->pUnkServer, rc);
    HeapFree(GetProcessHeap(),0,This);
    return 0;
  }
  return This->ref;
}

static HRESULT WINAPI PSOAStub_Connect(LPRPCSTUBBUFFER iface,
				       LPUNKNOWN lpUnkServer)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->Connect(%p)\n",This,lpUnkServer);
  This->pUnkServer = lpUnkServer;
  return S_OK;
}

static void WINAPI PSOAStub_Disconnect(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->Disconnect()\n",This);
  This->pUnkServer = NULL;
}

typedef HRESULT WINAPI (*HRESULT_FARPROC)();

static HRESULT WINAPI PSOAStub_Invoke(LPRPCSTUBBUFFER iface,
				      PRPCOLEMESSAGE pMsg,
				      LPRPCCHANNELBUFFER pChannel)
{
  ICOM_THIS(PSOAStubImpl,iface);
  int fn = pMsg->iMethod & 0xfff;
  int rec = pMsg->iMethod >> 12;
  int fs = This->fs;
  LPTYPEINFO pInfo;
  LPFUNCDESC pDesc;
  HRESULT hr;
  GUID is_iid;

  TRACE("(%p)->(%d:%d)(...)\n",This,rec,fn);

if (pMsg->iMethod == (ULONG)-1) {
  /* QueryInterface */
  IID iid;
  LPVOID pvObject = NULL;
  HRESULT hr;
  TRACE("RemoteQueryInterface(...)\n");
  if (pMsg->cbBuffer<sizeof(iid)) return E_FAIL;
  memcpy(&iid,pMsg->Buffer,sizeof(iid));
  TRACE("riid=%s, server=%p\n", debugstr_guid(&iid), This->pUnkServer);
  hr = IUnknown_QueryInterface(This->pUnkServer, &iid, &pvObject);
  TRACE("returns %p\n", pvObject);
  if (pvObject) {
    LPSTREAM pStm = NULL;
    STATSTG stat;
    HGLOBAL hGlobal = 0;
    CreateStreamOnHGlobal(0, TRUE, &pStm);
    CoMarshalInterface(pStm, &iid, pvObject, MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    IUnknown_Release((LPUNKNOWN)pvObject);
    IStream_Stat(pStm, &stat, 0);
    pMsg->cbBuffer = stat.cbSize.s.LowPart;
    IRpcChannelBuffer_GetBuffer(pChannel, pMsg, &This->iid);
    GetHGlobalFromStream(pStm, &hGlobal);
    memcpy(pMsg->Buffer, GlobalLock(hGlobal), pMsg->cbBuffer);
    GlobalUnlock(hGlobal);
    IStream_Release(pStm);
  } else {
    HeapFree(GetProcessHeap(),0,pMsg->Buffer);
    pMsg->Buffer = NULL;
    pMsg->cbBuffer = 0;
  }
  return S_OK;
}

  pInfo = This->pInfo;
  ITypeInfo_AddRef(pInfo);
  for (; rec; rec--) {
    HREFTYPE hType;
    /* find ancestor interface */
    hr = ITypeInfo_GetRefTypeOfImplType(pInfo, 0, &hType);
    if (SUCCEEDED(hr)) {
      LPTYPEINFO pParInfo;
      hr = ITypeInfo_GetRefTypeInfo(pInfo, hType, &pParInfo);
      if (SUCCEEDED(hr)) {
	LPTYPEATTR pAttr;
        TRACE("located ancestor (%ld) for IID %s\n", hType, debugstr_guid(&This->iid));
        ITypeInfo_Release(pInfo);
        pInfo = pParInfo;
	hr = ITypeInfo_GetTypeAttr(pInfo, &pAttr);
	if (FAILED(hr)) {
	  ERR("could not get type attr for IID %s\n", debugstr_guid(&This->iid));
	  ITypeInfo_Release(pInfo);
	  return hr;
	}
	fs -= pAttr->cFuncs;
	ITypeInfo_ReleaseTypeAttr(pInfo, pAttr);
        continue;
      }
    }
    ERR("failed to locate ancestor (%ld) for IID %s\n", hType, debugstr_guid(&This->iid));
    ITypeInfo_Release(pInfo);
    return hr;
  }
  memcpy(&is_iid, &IID_IUnknown, sizeof(GUID));
  hr = ITypeInfo_GetFuncDesc(pInfo, fn, &pDesc);
  if (SUCCEEDED(hr)) {
    LPSTREAM pStm = NULL;
    STATSTG stat;
    HGLOBAL hGlobal = 0;
    DWORD args[32];
    HRESULT argc;
    LPVOID vtbl = This->pUnkServer->lpVtbl;
    DWORD idx = fn + fs;
#ifdef ICOM_MSVTABLE_COMPAT
    idx += 2;
#endif

    CreateStreamOnHGlobal(0, TRUE, &pStm);
    stat.cbSize.s.HighPart = 0;
    stat.cbSize.s.LowPart = pMsg->cbBuffer;
    IStream_SetSize(pStm, stat.cbSize);
    GetHGlobalFromStream(pStm, &hGlobal);
    if (pMsg->cbBuffer) {
      memcpy(GlobalLock(hGlobal), pMsg->Buffer, pMsg->cbBuffer);
      GlobalUnlock(hGlobal);
    }
    memset(args, 0, sizeof(args));
    args[0] = (DWORD)This->pUnkServer;
    TL_Unmarshal(pStm, pInfo, pDesc, PARAMFLAG_FIN, args, &argc, pChannel, &is_iid);

    /* Invoke! */
    hr = E_FAIL;
    TRACE("invoking method idx %ld (argc=%ld) => %p\n", idx, argc, ((LPVOID*)vtbl)[idx]);
    if (pDesc->callconv == CC_STDCALL) {
      HRESULT_FARPROC func;
      func = ((LPVOID*)vtbl)[idx];
      switch (argc) {
      case 1: hr = func(args[0]); break;
      case 2: hr = func(args[0],args[1]); break;
      case 3: hr = func(args[0],args[1],args[2]); break;
      case 4: hr = func(args[0],args[1],args[2],args[3]); break;
      case 5: hr = func(args[0],args[1],args[2],args[3],args[4]); break;
      case 6: hr = func(args[0],args[1],args[2],args[3],args[4],args[5]); break;
      case 7: hr = func(args[0],args[1],args[2],args[3],args[4],args[5],args[6]); break;
      case 8: hr = func(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]); break;
      case 9: hr = func(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8]); break;
      default:
	FIXME("unimplemented nr of args: %ld\n", argc);
      }
    }
    else FIXME("callconv %d not supported\n", pDesc->callconv);
    TRACE("invoked method returned with HRESULT %08lx\n", hr);

    stat.cbSize.s.HighPart = 0;
    stat.cbSize.s.LowPart = 0;
    IStream_Seek(pStm, *(LARGE_INTEGER*)&stat.cbSize, STREAM_SEEK_SET, NULL);
    IStream_SetSize(pStm, stat.cbSize);
    TL_Marshal(pStm, pInfo, pDesc, PARAMFLAG_FOUT, args, hr, &is_iid);
    IStream_Stat(pStm, &stat, 0);
    GetHGlobalFromStream(pStm, &hGlobal);
    pMsg->cbBuffer = stat.cbSize.s.LowPart;
    IRpcChannelBuffer_GetBuffer(pChannel, pMsg, &This->iid);
    if (pMsg->cbBuffer) {
      memcpy(pMsg->Buffer, GlobalLock(hGlobal), pMsg->cbBuffer);
      GlobalUnlock(hGlobal);
    }
    IStream_Release(pStm);
    ITypeInfo_ReleaseFuncDesc(pInfo, pDesc);
    hr = S_OK;
  }
  ITypeInfo_Release(pInfo);
  return hr;
}

static LPRPCSTUBBUFFER WINAPI PSOAStub_IsIIDSupported(LPRPCSTUBBUFFER iface,
						      REFIID riid)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->IsIIDSupported(%s)\n",This,debugstr_guid(riid));
  return IsEqualGUID(&This->iid,riid) ? iface : NULL;
}

static ULONG WINAPI PSOAStub_CountRefs(LPRPCSTUBBUFFER iface)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->CountRefs()\n",This);
  return This->ref;
}

static HRESULT WINAPI PSOAStub_DebugServerQueryInterface(LPRPCSTUBBUFFER iface,
							 LPVOID *ppv)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->DebugServerQueryInterface(%p)\n",This,ppv);
  return S_OK;
}

static void WINAPI PSOAStub_DebugServerRelease(LPRPCSTUBBUFFER iface,
					       LPVOID pv)
{
  ICOM_THIS(PSOAStubImpl,iface);
  TRACE("(%p)->DebugServerRelease(%p)\n",This,pv);
}

static ICOM_VTABLE(IRpcStubBuffer) PSOAStub_VTable =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  PSOAStub_QueryInterface,
  PSOAStub_AddRef,
  PSOAStub_Release,
  PSOAStub_Connect,
  PSOAStub_Disconnect,
  PSOAStub_Invoke,
  PSOAStub_IsIIDSupported,
  PSOAStub_CountRefs,
  PSOAStub_DebugServerQueryInterface,
  PSOAStub_DebugServerRelease
};

/******************** PROXY ********************/
/* FIXME: should layer on top of rpcrt4 */

typedef struct {
  void *lpVtbl;
  ICOM_VTABLE(IRpcProxyBuffer) *pVtbl;
  DWORD ref;
  IID iid;
  LPUNKNOWN pUnkOuter;
  LPTYPEINFO pInfo;
  LPRPCCHANNELBUFFER pChannel;
} PSOAProxyImpl;

#define VTBL_OFS(impl) (((LPBYTE)(&((impl*)0)->pVtbl)) - (LPBYTE)0)
#define VTBL_UPCAST(impl,iface) (impl*)(((LPBYTE)iface) - VTBL_OFS(impl))

static HRESULT WINAPI PSOAProxy_QueryInterface(LPRPCPROXYBUFFER iface,
					       REFIID riid,
					       LPVOID *obj)
{
  PSOAProxyImpl *This = VTBL_UPCAST(PSOAProxyImpl,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&This->iid,riid)) {
    *obj = &This->lpVtbl;
    This->ref++;
    return S_OK;
  }
  if (IsEqualGUID(&IID_IRpcProxyBuffer,riid)) {
    *obj = &This->pVtbl;
    This->ref++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI PSOAProxy_AddRef(LPRPCPROXYBUFFER iface)
{
  PSOAProxyImpl *This = VTBL_UPCAST(PSOAProxyImpl,iface);
  TRACE("(%p)->AddRef()\n",This);
  return ++(This->ref);
}

static ULONG WINAPI PSOAProxy_Release(LPRPCPROXYBUFFER iface)
{
  PSOAProxyImpl *This = VTBL_UPCAST(PSOAProxyImpl,iface);
  TRACE("(%p)->Release()\n",This);
  if (!--(This->ref)) {
    /* FIXME: we may be leaking memory here; */
    /* should deallocate vtbl and release typeinfo */
    HeapFree(GetProcessHeap(),0,This);
    return 0;
  }
  return This->ref;
}

static HRESULT WINAPI PSOAProxy_Connect(LPRPCPROXYBUFFER iface,
					LPRPCCHANNELBUFFER pChannel)
{
  PSOAProxyImpl *This = VTBL_UPCAST(PSOAProxyImpl,iface);
  TRACE("(%p)->Connect(%p)\n",This,pChannel);
  This->pChannel = pChannel;
  return S_OK;
}

static VOID WINAPI PSOAProxy_Disconnect(LPRPCPROXYBUFFER iface)
{
  PSOAProxyImpl *This = VTBL_UPCAST(PSOAProxyImpl,iface);
  TRACE("(%p)->Disconnect()\n",This);
  This->pChannel = NULL;
}

static ICOM_VTABLE(IRpcProxyBuffer) PSOAProxy_VTable =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  PSOAProxy_QueryInterface,
  PSOAProxy_AddRef,
  PSOAProxy_Release,
  PSOAProxy_Connect,
  PSOAProxy_Disconnect
};

static HRESULT WINAPI OAProxy_QueryInterface(LPRPCPROXYBUFFER iface,
                                              REFIID riid,
                                              LPVOID *obj)
{
  ICOM_THIS(PSOAProxyImpl,iface);
  TRACE("(%p)->QueryInterface(%s,%p)\n",This,debugstr_guid(riid),obj);
  return IUnknown_QueryInterface(This->pUnkOuter,riid,obj);
}

static ULONG WINAPI OAProxy_AddRef(LPRPCPROXYBUFFER iface)
{
  ICOM_THIS(PSOAProxyImpl,iface);
  TRACE("(%p)\n",This);
  return IUnknown_AddRef(This->pUnkOuter);
}

static ULONG WINAPI OAProxy_Release(LPRPCPROXYBUFFER iface)
{
  ICOM_THIS(PSOAProxyImpl,iface);
  TRACE("(%p)\n",This);
  return IUnknown_Release(This->pUnkOuter);
}

static HRESULT WINAPI TLProxy(DWORD meth)
{
  DWORD *args = &meth + 2;
  LPRPCPROXYBUFFER iface = (LPRPCPROXYBUFFER)(args[0]);
  ICOM_THIS(PSOAProxyImpl,iface);
  int fn = meth & 0xfff;
  int rec = meth >> 12;
  LPTYPEINFO pInfo;
  LPFUNCDESC pDesc;
  HRESULT hr;
  BSTR name, desc;
  GUID is_iid;

  TRACE("(%p)->(%d:%d)(...) ret=%08lx\n",This,rec,fn,*(&meth + 1));
  pInfo = This->pInfo;
  name = NULL;
  desc = NULL;
  ITypeInfo_AddRef(pInfo);
  hr = ITypeInfo_GetDocumentation(pInfo, -1, &name, &desc, NULL, NULL);
  if (SUCCEEDED(hr)) {
    TRACE(" interface name: %s\n", name ? debugstr_w(name) : "(unknown)");
    if (desc)
      TRACE(" interface desc: %s\n", debugstr_w(desc));
    SysFreeString(name);
    SysFreeString(desc);
  }
  for (; rec; rec--) {
    HREFTYPE hType;
    /* find ancestor interface */
    hr = ITypeInfo_GetRefTypeOfImplType(pInfo, 0, &hType);
    if (SUCCEEDED(hr)) {
      LPTYPEINFO pParInfo;
      hr = ITypeInfo_GetRefTypeInfo(pInfo, hType, &pParInfo);
      if (SUCCEEDED(hr)) {
        TRACE("located ancestor (%ld) for IID %s\n", hType, debugstr_guid(&This->iid));
        ITypeInfo_Release(pInfo);
        pInfo = pParInfo;
        continue;
      }
    }
    ERR("failed to locate ancestor (%ld) for IID %s\n", hType, debugstr_guid(&This->iid));
    ITypeInfo_Release(pInfo);
    return hr;
  }
  memcpy(&is_iid, &IID_IUnknown, sizeof(GUID));
  hr = ITypeInfo_GetFuncDesc(pInfo, fn, &pDesc);
  if (SUCCEEDED(hr)) {
    LPSTREAM pStm = NULL;
    STATSTG stat;
    HGLOBAL hGlobal = 0;
    RPCOLEMESSAGE msg;
    ULONG status;

    name = NULL;
    desc = NULL;
    hr = ITypeInfo_GetDocumentation(pInfo, pDesc->memid, &name, &desc, NULL, NULL);
    if (SUCCEEDED(hr)) {
      TRACE(" method name: %s\n", name ? debugstr_w(name) : "(unknown)");
      if (desc)
	TRACE(" method desc: %s\n", debugstr_w(desc));
      SysFreeString(name);
      SysFreeString(desc);
    }

    msg.reserved1 = This; /* BAD hack */
    msg.iMethod = meth;
    msg.Buffer = NULL;

    CreateStreamOnHGlobal(0, TRUE, &pStm);
    GetHGlobalFromStream(pStm, &hGlobal);
    TL_Marshal(pStm, pInfo, pDesc, PARAMFLAG_FIN, args, 0, &is_iid);
    IStream_Stat(pStm, &stat, 0);
    msg.cbBuffer = stat.cbSize.s.LowPart;
    IRpcChannelBuffer_GetBuffer(This->pChannel, &msg, &This->iid);
    memcpy(msg.Buffer, GlobalLock(hGlobal), msg.cbBuffer);
    GlobalUnlock(hGlobal);

    hr = IRpcChannelBuffer_SendReceive(This->pChannel, &msg, &status);
    if (FAILED(hr)) goto proxy_error;
    if (hr == RPC_S_CALL_FAILED) {
      RpcRaiseException(*(DWORD*)msg.Buffer);
      goto proxy_error;
    }

    stat.cbSize.s.HighPart = 0;
    stat.cbSize.s.LowPart = 0;
    IStream_Seek(pStm, *(LARGE_INTEGER*)&stat.cbSize, STREAM_SEEK_SET, NULL);
    stat.cbSize.s.HighPart = 0;
    stat.cbSize.s.LowPart = msg.cbBuffer;
    IStream_SetSize(pStm, stat.cbSize);
    GetHGlobalFromStream(pStm, &hGlobal);
    memcpy(GlobalLock(hGlobal), msg.Buffer, msg.cbBuffer);
    GlobalUnlock(hGlobal);
    hr = E_FAIL;
    TL_Unmarshal(pStm, pInfo, pDesc, PARAMFLAG_FOUT, args, &hr, This->pChannel, &is_iid);
proxy_error:
    IStream_Release(pStm);
    IRpcChannelBuffer_FreeBuffer(This->pChannel, &msg);
    ITypeInfo_ReleaseFuncDesc(pInfo, pDesc);
  }
  ITypeInfo_Release(pInfo);
  TRACE("returning %08lx\n", hr);
  return hr;
}

typedef struct {
  BYTE push WINE_PACKED;
  DWORD meth WINE_PACKED;
  BYTE call WINE_PACKED;
  int handler WINE_PACKED;
  BYTE ret WINE_PACKED;
  WORD bytes WINE_PACKED;
  BYTE pad0, pad1, pad2;
} TLThunk;

static TLThunk TLstdcall = {
  0x68, 0,          /* pushl $0 */
  0xe8, -10,        /* call 0 */
  0xc2, 0,          /* ret $0 */
  0x8d, 0x76, 0x00  /* leal (%esi),%esi */
};

static LPVOID OA_BuildProxyVtbl(LPTYPEINFO pInfo, int *pfs, int rec)
{
  LPVOID vtbl = NULL;
  IID iid;
  LPTYPEATTR pAttr;
  TYPEKIND kind;
  int types, funcs, tfs, fs = pfs ? *pfs : 0;
  int fn, p, pb;
  MEMBERID mid;
  HREFTYPE hType;
  BSTR name, desc;
  HRESULT hr;

  hr = ITypeInfo_GetTypeAttr(pInfo, &pAttr);
  if (FAILED(hr)) {
    ERR("could not get type attr for interface\n");
    return NULL;
  }
  memcpy(&iid, &pAttr->guid, sizeof(iid));
  kind = pAttr->typekind;
  types = pAttr->cImplTypes;
  funcs = pAttr->cFuncs;
  ITypeInfo_ReleaseTypeAttr(pInfo, pAttr);

  if (kind != TKIND_INTERFACE) {
    ERR("invalid type kind %d for IID %s\n", kind, debugstr_guid(&iid));
    return NULL;
  }
  tfs = funcs + fs;
  /* find ancestor interface */
  hr = ITypeInfo_GetRefTypeOfImplType(pInfo, 0, &hType);
  if (SUCCEEDED(hr)) {
    LPTYPEINFO pParInfo;
    hr = ITypeInfo_GetRefTypeInfo(pInfo, hType, &pParInfo);
    if (SUCCEEDED(hr)) {
      int nfs = funcs + fs;
      TRACE("located ancestor (%ld) for IID %s\n", hType, debugstr_guid(&iid));
      vtbl = OA_BuildProxyVtbl(pParInfo, &nfs, rec+1);
      ITypeInfo_Release(pParInfo);
      fs = nfs;
    } else {
      ERR("failed to locate ancestor (%ld) for IID %s\n", hType, debugstr_guid(&iid));
      MESSAGE("Please make sure a copy of stdole32.tlb is installed.\n");
      tfs += 3; /* assume IUnknown for now, but it could be IDispatch, don't know how to find out */
      fs = 3;
    }
  }
  else {
    TRACE("no ancestor for IID %s\n", debugstr_guid(&iid));
    fs = 0;
  }
  if (pfs) *pfs = fs + funcs;
  /* if we have no ancestor, allocate vtable */
  if (!vtbl) {
    ICOM_VTABLE(IUnknown) *vt;
    vt = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(LPVOID)*tfs);
    vt->QueryInterface = OAProxy_QueryInterface;
    vt->AddRef = OAProxy_AddRef;
    vt->Release = OAProxy_Release;
    vtbl = vt;
    TRACE("  vtbl IUnknown entries filled\n");
    if (IsEqualGUID(&iid, &IID_IUnknown)) return vtbl;
  }
  /* parse descendant interface */
  TRACE("typelib data for IID %s:\n", debugstr_guid(&iid));
  name = NULL;
  desc = NULL;
  hr = ITypeInfo_GetDocumentation(pInfo, -1, &name, &desc, NULL, NULL);
  if (SUCCEEDED(hr)) {
    TRACE(" name: %s\n", name ? debugstr_w(name) : "(unknown)");
    if (desc)
      TRACE(" desc: %s\n", debugstr_w(desc));
    SysFreeString(name);
    SysFreeString(desc);
  }
  TRACE(" types: %d\n", types);
  TRACE(" funcs: %d\n", funcs);
  for (fn=0; fn<funcs; fn++) {
    LPFUNCDESC pDesc;
    hr = ITypeInfo_GetFuncDesc(pInfo, fn, &pDesc);
    if (SUCCEEDED(hr)) {
      BSTR names[16];
      UINT mnames = 0;
      mid = pDesc->memid;
      TRACE(" function %d (member ID %ld) (vofs %d)\n", fn, mid, (fn+fs)*sizeof(FARPROC));
      name = NULL;
      desc = NULL;
      hr = ITypeInfo_GetDocumentation(pInfo, mid, &name, &desc, NULL, NULL);
      if (SUCCEEDED(hr)) {
	TRACE("  name: %s\n", name ? debugstr_w(name) : "(unknown)");
	if (desc)
	  TRACE("  desc: %s\n", debugstr_w(desc));
	SysFreeString(name);
	SysFreeString(desc);
      }
      TRACE("  funckind: %d\n", pDesc->funckind); /* 1 = PURE VIRTUAL */
      TRACE("  invkind : %d\n", pDesc->invkind);  /* 1 = FUNC, 2 = PROPGET, 3 = PROPPUT */
      TRACE("  callconv: %d\n", pDesc->callconv); /* 4 = STDCALL */
      TRACE("  params  : %d\n", pDesc->cParams);
      TRACE("  params 2: %d\n", pDesc->cParamsOpt);
      TRACE("  vft     : %d\n", pDesc->oVft);
      TRACE("  scodes  : %d\n", pDesc->cScodes);
      TRACE("  flags   : %x\n", pDesc->wFuncFlags);
      TRACE("  return type: %d\n", pDesc->elemdescFunc.tdesc.vt); /* 25 = HRESULT */
      ITypeInfo_GetNames(pInfo, mid, names, 16, &mnames);
      if (mnames) SysFreeString(names[0]);
      pb = sizeof(LPVOID); /* count argument bytes */
      for (p=0; p<pDesc->cParams; p++) {
	ELEMDESC *par = &pDesc->lprgelemdescParam[p];
	TRACE("  parameter %d\n", p);
	if (p<=mnames) {
	  TRACE("   name   : %s\n", debugstr_w(names[p+1]));
	  SysFreeString(names[p+1]);
	}
	TRACE("   type   : %d\n", par->tdesc.vt);
	TRACE("   flags  : %02x\n", par->u.paramdesc.wParamFlags);
	if (par->u.paramdesc.pparamdescex)
	  TRACE("   bytes  : %ld\n", par->u.paramdesc.pparamdescex->cBytes);
	pb += sizeof(DWORD)*TL_ArgSiz(TL_SizeOfType(pInfo, &par->tdesc));
      }
      if ((fn + fs) < 3) {
	/* this better not happen */
      }
      else if (pDesc->callconv == CC_STDCALL) {
	/* OK, create thunk */
	TLThunk *thunk = HeapAlloc(GetProcessHeap(),0,sizeof(TLThunk));
	DWORD idx = fn + fs;
	memcpy(thunk, &TLstdcall, sizeof(TLThunk));
	thunk->meth = fn | (rec << 12);
	thunk->handler += (LPBYTE)TLProxy - (LPBYTE)thunk;
	thunk->bytes = pb;
	TRACE("  vtbl entry %ld -> thunk %p, argsiz %d\n", idx, thunk, pb);
#ifdef ICOM_MSVTABLE_COMPAT
	idx += 2;
#endif
	((LPVOID*)vtbl)[idx] = thunk;
      }
      else FIXME("callconv %d not supported\n", pDesc->callconv);
      ITypeInfo_ReleaseFuncDesc(pInfo, pDesc);
    }
  }
  return vtbl;
}

/*******************************************************************************
 * PSOAFactoryBuffer
 */
extern const CLSID CLSID_PSDispatch;
extern HRESULT OLEAUTPS_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv);

static HRESULT WINAPI PSOAFactory_QueryInterface(LPPSFACTORYBUFFER iface,
						 REFIID riid,
						 LPVOID *obj) {
  TRACE("(%p)->QueryInterface(%s,%p)\n",iface,debugstr_guid(riid),obj);
  if (IsEqualGUID(&IID_IUnknown,riid) ||
      IsEqualGUID(&IID_IPSFactoryBuffer,riid)) {
    *obj = iface;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI PSOAFactory_AddRef(LPPSFACTORYBUFFER iface) {
  TRACE("(%p)->AddRef()\n",iface);
  return 2;
}

static ULONG WINAPI PSOAFactory_Release(LPPSFACTORYBUFFER iface) {
  TRACE("(%p)->Release()\n",iface);
  return 1;
}

static HRESULT WINAPI PSOAFactory_CreateProxy(LPPSFACTORYBUFFER iface,
					      LPUNKNOWN pUnkOuter,
					      REFIID riid,
					      LPRPCPROXYBUFFER *ppProxy,
					      LPVOID *ppv)
{
  PSOAProxyImpl *proxy;
  LPTYPELIB pTlib;
  LPTYPEINFO pInfo;
  HRESULT hr;
  TRACE("(%p)->CreateProxy(%p,%s,%p,%p)\n",iface,pUnkOuter,
        debugstr_guid(riid),ppProxy,ppv);
  hr = OA_LoadInterfaceTypeLib(riid, &pTlib);
  if (SUCCEEDED(hr)) {
    TYPEKIND kind;
    hr = ITypeLib_GetTypeInfoOfGuid(pTlib, riid, &pInfo);
    ITypeLib_Release(pTlib);
    if (FAILED(hr)) {
      return E_FAIL;
    }
    hr = TL_GetInterface(&pInfo, &kind);
    if (FAILED(hr)) {
      ITypeInfo_Release(pInfo);
      return hr;
    }

    if (kind == TKIND_DISPATCH) {
      /* create standard IDispatch proxy */
      LPPSFACTORYBUFFER pPS;
      hr = OLEAUTPS_DllGetClassObject(&CLSID_PSDispatch, &IID_IPSFactoryBuffer, (LPVOID *)&pPS);
      if (SUCCEEDED(hr)) {
        hr = IPSFactoryBuffer_CreateProxy(pPS, pUnkOuter, &IID_IDispatch, ppProxy, ppv);
        IPSFactoryBuffer_Release(pPS);
      }
      ITypeInfo_Release(pInfo);
      return hr;
    }

    proxy = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(PSOAProxyImpl));
    if (!proxy) {
      ITypeInfo_Release(pInfo);
      return E_OUTOFMEMORY;
    }
    proxy->pVtbl = &PSOAProxy_VTable;
    proxy->ref = 1;
    memcpy(&proxy->iid, riid, sizeof(proxy->iid));
    proxy->pUnkOuter = pUnkOuter;
    proxy->pInfo = pInfo;
    proxy->lpVtbl = OA_BuildProxyVtbl(pInfo, NULL, 0);
    *ppProxy = (LPRPCPROXYBUFFER)&proxy->pVtbl;
    *ppv = proxy;
    TRACE("created typelib-based interface proxy\n");
    return S_OK;
  }
  return E_FAIL;
}

static HRESULT WINAPI PSOAFactory_CreateStub(LPPSFACTORYBUFFER iface,
					     REFIID riid,
					     LPUNKNOWN pUnkServer,
					     LPRPCSTUBBUFFER *ppStub)
{
  PSOAStubImpl *stub;
  LPTYPELIB pTlib;
  LPTYPEINFO pInfo;
  HRESULT hr;
  TRACE("(%p)->CreateStub(%s,%p,%p)\n",iface,debugstr_guid(riid),
        pUnkServer,ppStub);
  hr = OA_LoadInterfaceTypeLib(riid, &pTlib);
  if (SUCCEEDED(hr)) {
    TYPEKIND kind;
    hr = ITypeLib_GetTypeInfoOfGuid(pTlib, riid, &pInfo);
    ITypeLib_Release(pTlib);
    if (FAILED(hr)) {
      return E_FAIL;
    }
    hr = TL_GetInterface(&pInfo, &kind);
    if (FAILED(hr)) {
      ITypeInfo_Release(pInfo);
      return hr;
    }

    if (kind == TKIND_DISPATCH) {
      /* create standard IDispatch stub */
      LPPSFACTORYBUFFER pPS;
      hr = OLEAUTPS_DllGetClassObject(&CLSID_PSDispatch, &IID_IPSFactoryBuffer, (LPVOID *)&pPS);
      if (SUCCEEDED(hr)) {
        hr = IPSFactoryBuffer_CreateStub(pPS, &IID_IDispatch, pUnkServer, ppStub);
        IPSFactoryBuffer_Release(pPS);
      }
      ITypeInfo_Release(pInfo);
      return hr;
    }

    stub = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(PSOAStubImpl));
    if (!stub) {
      ITypeInfo_Release(pInfo);
      return E_OUTOFMEMORY;
    }
    stub->lpVtbl = &PSOAStub_VTable;
    stub->ref = 1;
    memcpy(&stub->iid, riid, sizeof(stub->iid));
    IUnknown_AddRef(pUnkServer);
    stub->pUnkServer = pUnkServer;
    stub->pInfo = pInfo;
    stub->fs = 0;
    /* count methods in ancestor classes */
    ITypeInfo_AddRef(pInfo);
    while (TRUE) {
      HREFTYPE hType;
      HRESULT hr = ITypeInfo_GetRefTypeOfImplType(pInfo, 0, &hType);
      if (SUCCEEDED(hr)) {
	LPTYPEINFO pParInfo;
	hr = ITypeInfo_GetRefTypeInfo(pInfo, hType, &pParInfo);
	if (SUCCEEDED(hr)) {
	  LPTYPEATTR pAttr;
	  ITypeInfo_Release(pInfo);
	  pInfo = pParInfo;
	  hr = ITypeInfo_GetTypeAttr(pInfo, &pAttr);
	  if (FAILED(hr)) {
	    ERR("could not get type attr for interface\n");
	    break;
	  }
	  stub->fs += pAttr->cFuncs;
	  ITypeInfo_ReleaseTypeAttr(pInfo, pAttr);
	} else {
	  stub->fs += 3; /* assume IUnknown for now */
	  break;
	}
      }
      else break;
    }
    ITypeInfo_Release(pInfo);
    TRACE(" pfs=%d\n", stub->fs);
    /* done */
    *ppStub = (LPRPCSTUBBUFFER)stub;
    TRACE("created typelib-based interface stub\n");
    return S_OK;
  }
  return E_FAIL;
}

static ICOM_VTABLE(IPSFactoryBuffer) PSOAFactoryBuffer_VTable =
{
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  PSOAFactory_QueryInterface,
  PSOAFactory_AddRef,
  PSOAFactory_Release,
  PSOAFactory_CreateProxy,
  PSOAFactory_CreateStub
};

static ICOM_VTABLE(IPSFactoryBuffer) *PSOAFactoryBuffer = &PSOAFactoryBuffer_VTable;

void _get_OLEAUT_PSF(LPVOID *ppv) { *ppv = (LPVOID)&PSOAFactoryBuffer; }
