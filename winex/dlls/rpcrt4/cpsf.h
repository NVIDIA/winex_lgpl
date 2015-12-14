/*
 * COM proxy definitions
 *
 * Copyright 2001 Ove Kåven, TransGaming Technologies
 */

#ifndef __WINE_CPSF_H
#define __WINE_CPSF_H

HRESULT WINAPI StdProxy_Construct(REFIID riid,
				  LPUNKNOWN pUnkOuter,
				  PCInterfaceName name,
				  CInterfaceProxyVtbl *vtbl,
				  CInterfaceStubVtbl *svtbl,
				  LPPSFACTORYBUFFER pPSFactory,
				  LPRPCPROXYBUFFER *ppProxy,
				  LPVOID *ppvObj);
HRESULT WINAPI StdProxy_GetChannel(LPVOID iface,
				   LPRPCCHANNELBUFFER *ppChannel);
HRESULT WINAPI StdProxy_GetIID(LPVOID iface,
			       const IID **piid);

HRESULT WINAPI CStdStubBuffer_Construct(REFIID riid,
					LPUNKNOWN pUnkServer,
					PCInterfaceName name,
					CInterfaceStubVtbl *vtbl,
					LPPSFACTORYBUFFER pPSFactory,
					LPRPCSTUBBUFFER *ppStub);

#endif  /* __WINE_CPSF_H */
