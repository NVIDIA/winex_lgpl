/*
 *	SHCreateStreamOnFile
 */
#include <string.h>

#include "winerror.h"
#include "winbase.h"
#include "winreg.h"
#include "winuser.h"
#include "shlobj.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

typedef struct
{	ICOM_VFIELD(IStream);
	DWORD		ref;
	HANDLE		hFile;
} ISHFileStream;

static struct ICOM_VTABLE(IStream) rstvt;

static DWORD STGM_access(DWORD grfMode)
{
	if (grfMode & STGM_READWRITE)
		return GENERIC_READ | GENERIC_WRITE;
	if (grfMode & STGM_WRITE)
		return GENERIC_WRITE;
	return GENERIC_READ;
}

static DWORD STGM_share(DWORD grfMode)
{
	switch (grfMode & (STGM_SHARE_DENY_NONE | STGM_SHARE_DENY_READ | STGM_SHARE_DENY_WRITE | STGM_SHARE_EXCLUSIVE)) {
	case STGM_SHARE_DENY_READ:
		return FILE_SHARE_WRITE;
	case STGM_SHARE_DENY_WRITE:
		return FILE_SHARE_READ;
	case STGM_SHARE_EXCLUSIVE:
		return 0;
	default:
		return FILE_SHARE_READ | FILE_SHARE_WRITE;
	}
}

static DWORD STGM_disp(DWORD grfMode)
{
	if (grfMode & STGM_CREATE)
		return CREATE_ALWAYS;
	else /* STGM_FAILIFTHERE */
		return CREATE_NEW;
}

static DWORD STGM_flags(DWORD grfMode)
{
	DWORD flags = 0;
	if (grfMode & STGM_DELETEONRELEASE)
		flags |= FILE_FLAG_DELETE_ON_CLOSE;
	return flags;
}

/**************************************************************************
*   IStream_ConstructorA	[internal]
*/
HRESULT IStream_ConstructorA(LPCSTR pszFile, DWORD grfMode, IStream **ppstm)
{
	ISHFileStream*	rstr;

	rstr = (ISHFileStream*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(ISHFileStream));

	ICOM_VTBL(rstr)=&rstvt;
	rstr->ref = 1;

	rstr->hFile = CreateFileA(pszFile, STGM_access(grfMode), STGM_share(grfMode),
				  NULL, STGM_disp(grfMode), STGM_flags(grfMode), 0);

	*ppstm = (IStream *)rstr;
	return S_OK;
}

/**************************************************************************
*   IStream_ConstructorW	[internal]
*/
HRESULT IStream_ConstructorW(LPCWSTR pszFile, DWORD grfMode, IStream **ppstm)
{
	ISHFileStream*	rstr;

	rstr = (ISHFileStream*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(ISHFileStream));

	ICOM_VTBL(rstr)=&rstvt;
	rstr->ref = 1;

	rstr->hFile = CreateFileW(pszFile, STGM_access(grfMode), STGM_share(grfMode),
				  NULL, STGM_disp(grfMode), STGM_flags(grfMode), 0);

	*ppstm = (IStream *)rstr;
	return S_OK;
}

/**************************************************************************
*  IStream_fnQueryInterface
*/
static HRESULT WINAPI IStream_fnQueryInterface(IStream *iface, REFIID riid, LPVOID *ppvObj)
{
	ICOM_THIS(ISHFileStream, iface);

	TRACE("(%p)->(\n\tIID:\t%s,%p)\n",This,debugstr_guid(riid),ppvObj);

	*ppvObj = NULL;

	if(IsEqualIID(riid, &IID_IUnknown))	/*IUnknown*/
	{ *ppvObj = This;
	}
	else if(IsEqualIID(riid, &IID_IStream))	/*IStream*/
	{ *ppvObj = This;
	}

	if(*ppvObj)
	{
	  IStream_AddRef((IStream*)*ppvObj);
	  TRACE("-- Interface: (%p)->(%p)\n",ppvObj,*ppvObj);
	  return S_OK;
	}
	TRACE("-- Interface: E_NOINTERFACE\n");
	return E_NOINTERFACE;
}

/**************************************************************************
*  IStream_fnAddRef
*/
static ULONG WINAPI IStream_fnAddRef(IStream *iface)
{
	ICOM_THIS(ISHFileStream, iface);

	TRACE("(%p)->(count=%lu)\n",This, This->ref);

	return ++(This->ref);
}

/**************************************************************************
*  IStream_fnRelease
*/
static ULONG WINAPI IStream_fnRelease(IStream *iface)
{
	ICOM_THIS(ISHFileStream, iface);

	TRACE("(%p)->()\n",This);

	if (!--(This->ref))
	{ TRACE(" destroying SHFile IStream (%p)\n",This);

	  if (This->hFile)
	    CloseHandle(This->hFile);

	  HeapFree(GetProcessHeap(),0,This);
	  return 0;
	}
	return This->ref;
}

static HRESULT WINAPI IStream_fnRead (IStream * iface, void* pv, ULONG cb, ULONG* pcbRead)
{
	ICOM_THIS(ISHFileStream, iface);
	DWORD dwBytesRead = 0;
	BOOL ok;

	TRACE("(%p)->(%p,0x%08lx,%p)\n",This, pv, cb, pcbRead);

	if ( !pv )
	  return STG_E_INVALIDPOINTER;

	ok = ReadFile(This->hFile, pv, cb, &dwBytesRead, NULL);

	if (pcbRead) *pcbRead = dwBytesRead;

	if (!ok)
		return HRESULT_FROM_WIN32(GetLastError());

	return S_OK;
}

static HRESULT WINAPI IStream_fnWrite (IStream * iface, const void* pv, ULONG cb, ULONG* pcbWritten)
{
	ICOM_THIS(ISHFileStream, iface);
	DWORD dwBytesWritten = 0;
	BOOL ok;

	TRACE("(%p)->(%p,0x%08lx,%p)\n",This, pv, cb, pcbWritten);

	if ( !pv )
	  return STG_E_INVALIDPOINTER;

	ok = WriteFile(This->hFile, pv, cb, &dwBytesWritten, NULL);

	if (pcbWritten) *pcbWritten = dwBytesWritten;

	if (!ok)
		return HRESULT_FROM_WIN32(GetLastError());

	return S_OK;
}

static HRESULT WINAPI IStream_fnSeek (IStream * iface, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
	ICOM_THIS(ISHFileStream, iface);
	LONG high, low;
	DWORD err;

	TRACE("(%p)->(%lld,%ld,%p)\n",This, dlibMove.QuadPart, dwOrigin, plibNewPosition);

	high = dlibMove.u.HighPart;
	low = SetFilePointer(This->hFile, dlibMove.u.LowPart, &high, dwOrigin);

	if ((low == INVALID_SET_FILE_POINTER) && (err = GetLastError()))
		return HRESULT_FROM_WIN32(err);

	if (plibNewPosition) {
		plibNewPosition->u.LowPart = low;
		plibNewPosition->u.HighPart = high;
	}

	return S_OK;
}

static HRESULT WINAPI IStream_fnSetSize (IStream * iface, ULARGE_INTEGER libNewSize)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnCopyTo (IStream * iface, IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnCommit (IStream * iface, DWORD grfCommitFlags)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnRevert (IStream * iface)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnLockRegion (IStream * iface, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnUnlockRegion (IStream * iface, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnStat (IStream * iface, STATSTG*   pstatstg, DWORD grfStatFlag)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnClone (IStream * iface, IStream** ppstm)
{
	ICOM_THIS(ISHFileStream, iface);

	FIXME("(%p)\n",This);

	return E_NOTIMPL;
}

static struct ICOM_VTABLE(IStream) rstvt =
{
	ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IStream_fnQueryInterface,
	IStream_fnAddRef,
	IStream_fnRelease,
	IStream_fnRead,
	IStream_fnWrite,
	IStream_fnSeek,
	IStream_fnSetSize,
	IStream_fnCopyTo,
	IStream_fnCommit,
	IStream_fnRevert,
	IStream_fnLockRegion,
	IStream_fnUnlockRegion,
	IStream_fnStat,
	IStream_fnClone

};

/*************************************************************************
 * SHCreateStreamOnFileA				[SHLWAPI.@]
 */
HRESULT WINAPI SHCreateStreamOnFileA(
	LPCSTR pszFile,
	DWORD grfMode,
	IStream **ppstm)
{
	TRACE("(%s,0x%08lx,%p)\n",
	debugstr_a(pszFile), grfMode, ppstm);

	return IStream_ConstructorA(pszFile, grfMode, ppstm);
}

/*************************************************************************
 * SHCreateStreamOnFileW				[SHLWAPI.@]
 */
HRESULT WINAPI SHCreateStreamOnFileW(
	LPCWSTR pszFile,
	DWORD grfMode,
	IStream **ppstm)
{
	TRACE("(%s,0x%08lx,%p)\n",
	debugstr_w(pszFile), grfMode, ppstm);

	return IStream_ConstructorW(pszFile, grfMode, ppstm);
}
