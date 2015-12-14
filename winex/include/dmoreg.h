/*
 * Copyright (C) 2002 Hidenori Takeshima
 */

#ifndef __WINE_DMOREG_H_
#define __WINE_DMOREG_H_


typedef struct
{
	GUID	type;
	GUID	subtype;
} DMO_PARTIAL_MEDIATYPE;


HRESULT WINAPI DMOEnum( REFGUID rguidCat, DWORD dwFlags, DWORD dwCountOfInTypes, const DMO_PARTIAL_MEDIATYPE* pInTypes, DWORD dwCountOfOutTypes, const DMO_PARTIAL_MEDIATYPE* pOutTypes, IEnumDMO** ppEnum );

HRESULT WINAPI DMOGetName( REFCLSID rclsid, WCHAR* pwszName );

HRESULT WINAPI DMOGetTypes( REFCLSID rclsid, unsigned long ulInputTypesReq, unsigned long* pulInputTypesRet, unsigned long ulOutputTypesReq, unsigned long* pulOutputTypesRet, const DMO_PARTIAL_MEDIATYPE* pOutTypes );

HRESULT WINAPI DMORegister( LPCWSTR pwszName, REFCLSID rclsid, REFGUID rguidCat, DWORD dwFlags, DWORD dwCountOfInTypes, const DMO_PARTIAL_MEDIATYPE* pInTypes, DWORD dwCountOfOutTypes, const DMO_PARTIAL_MEDIATYPE* pOutTypes );

HRESULT WINAPI DMOUnregister( REFCLSID rclsid, REFGUID rguidCat );


#endif	/* __WINE_DMOREG_H_ */
