/*
 * NT basis DLL
 *
 * This file contains the Rtl* API functions. These should be implementable.
 *
 * Copyright 1996-1998 Marcus Meissner
 *		  1999 Alex Korobka
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wine/debug.h"
#include "windef.h"
#include "winerror.h"
#include "wine/stackframe.h"

#include "winternl.h"
#include "winreg.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);


static RTL_CRITICAL_SECTION peb_lock;

void PROCESS_Init(void)
{
    RTL_CRITICAL_SECTION_DEFINE( &peb_lock );
}

/*
 *	resource functions
 */

/***********************************************************************
 *           RtlInitializeResource	(NTDLL.@)
 *
 * xxxResource() functions implement multiple-reader-single-writer lock.
 * The code is based on information published in WDJ January 1999 issue.
 */
void WINAPI RtlInitializeResource(LPRTL_RWLOCK rwl)
{
    if( rwl )
    {
	rwl->iNumberActive = 0;
	rwl->uExclusiveWaiters = 0;
	rwl->uSharedWaiters = 0;
	rwl->hOwningThreadId = 0;
	rwl->dwTimeoutBoost = 0; /* no info on this one, default value is 0 */
	RtlInitializeCriticalSection( &rwl->rtlCS );
        NtCreateSemaphore( &rwl->hExclusiveReleaseSemaphore, 0, NULL, 0, 65535 );
        NtCreateSemaphore( &rwl->hSharedReleaseSemaphore, 0, NULL, 0, 65535 );
    }
}


/***********************************************************************
 *           RtlDeleteResource		(NTDLL.@)
 */
void WINAPI RtlDeleteResource(LPRTL_RWLOCK rwl)
{
    if( rwl )
    {
	RtlEnterCriticalSection( &rwl->rtlCS );
	if( rwl->iNumberActive || rwl->uExclusiveWaiters || rwl->uSharedWaiters )
	    MESSAGE("Deleting active MRSW lock (%p), expect failure\n", rwl );
	rwl->hOwningThreadId = 0;
	rwl->uExclusiveWaiters = rwl->uSharedWaiters = 0;
	rwl->iNumberActive = 0;
	NtClose( rwl->hExclusiveReleaseSemaphore );
	NtClose( rwl->hSharedReleaseSemaphore );
	RtlLeaveCriticalSection( &rwl->rtlCS );
	RtlDeleteCriticalSection( &rwl->rtlCS );
    }
}


/***********************************************************************
 *          RtlAcquireResourceExclusive	(NTDLL.@)
 */
BYTE WINAPI RtlAcquireResourceExclusive(LPRTL_RWLOCK rwl, BYTE fWait)
{
    BYTE retVal = 0;
    if( !rwl ) return 0;

start:
    RtlEnterCriticalSection( &rwl->rtlCS );
    if( rwl->iNumberActive == 0 ) /* lock is free */
    {
	rwl->iNumberActive = -1;
	retVal = 1;
    }
    else if( rwl->iNumberActive < 0 ) /* exclusive lock in progress */
    {
	 if( rwl->hOwningThreadId == GetCurrentThreadId() )
	 {
	     retVal = 1;
	     rwl->iNumberActive--;
	     goto done;
	 }
wait:
	 if( fWait )
	 {
	     rwl->uExclusiveWaiters++;

	     RtlLeaveCriticalSection( &rwl->rtlCS );
	     if( WaitForSingleObject( rwl->hExclusiveReleaseSemaphore, INFINITE ) == WAIT_FAILED )
		 goto done;
	     goto start; /* restart the acquisition to avoid deadlocks */
	 }
    }
    else  /* one or more shared locks are in progress */
	 if( fWait )
	     goto wait;

    if( retVal == 1 )
	rwl->hOwningThreadId = GetCurrentThreadId();
done:
    RtlLeaveCriticalSection( &rwl->rtlCS );
    return retVal;
}

/***********************************************************************
 *          RtlAcquireResourceShared	(NTDLL.@)
 */
BYTE WINAPI RtlAcquireResourceShared(LPRTL_RWLOCK rwl, BYTE fWait)
{
    DWORD dwWait = WAIT_FAILED;
    BYTE retVal = 0;
    if( !rwl ) return 0;

start:
    RtlEnterCriticalSection( &rwl->rtlCS );
    if( rwl->iNumberActive < 0 )
    {
	if( rwl->hOwningThreadId == GetCurrentThreadId() )
	{
	    rwl->iNumberActive--;
	    retVal = 1;
	    goto done;
	}

	if( fWait )
	{
	    rwl->uSharedWaiters++;
	    RtlLeaveCriticalSection( &rwl->rtlCS );
	    if( (dwWait = WaitForSingleObject( rwl->hSharedReleaseSemaphore, INFINITE )) == WAIT_FAILED )
		goto done;
	    goto start;
	}
    }
    else
    {
	if( dwWait != WAIT_OBJECT_0 ) /* otherwise RtlReleaseResource() has already done it */
	    rwl->iNumberActive++;
	retVal = 1;
    }
done:
    RtlLeaveCriticalSection( &rwl->rtlCS );
    return retVal;
}


/***********************************************************************
 *           RtlReleaseResource		(NTDLL.@)
 */
void WINAPI RtlReleaseResource(LPRTL_RWLOCK rwl)
{
    RtlEnterCriticalSection( &rwl->rtlCS );

    if( rwl->iNumberActive > 0 ) /* have one or more readers */
    {
	if( --rwl->iNumberActive == 0 )
	{
	    if( rwl->uExclusiveWaiters )
	    {
wake_exclusive:
		rwl->uExclusiveWaiters--;
		NtReleaseSemaphore( rwl->hExclusiveReleaseSemaphore, 1, NULL );
	    }
	}
    }
    else
    if( rwl->iNumberActive < 0 ) /* have a writer, possibly recursive */
    {
	if( ++rwl->iNumberActive == 0 )
	{
	    rwl->hOwningThreadId = 0;
	    if( rwl->uExclusiveWaiters )
		goto wake_exclusive;
	    else
		if( rwl->uSharedWaiters )
		{
		    UINT n = rwl->uSharedWaiters;
		    rwl->iNumberActive = rwl->uSharedWaiters; /* prevent new writers from joining until
							       * all queued readers have done their thing */
		    rwl->uSharedWaiters = 0;
		    NtReleaseSemaphore( rwl->hSharedReleaseSemaphore, n, NULL );
		}
	}
    }
    RtlLeaveCriticalSection( &rwl->rtlCS );
}


/***********************************************************************
 *           RtlDumpResource		(NTDLL.@)
 */
void WINAPI RtlDumpResource(LPRTL_RWLOCK rwl)
{
    if( rwl )
    {
	MESSAGE("RtlDumpResource(%p):\n\tactive count = %i\n\twaiting readers = %i\n\twaiting writers = %i\n",
		rwl, rwl->iNumberActive, rwl->uSharedWaiters, rwl->uExclusiveWaiters );
	if( rwl->iNumberActive )
	    MESSAGE("\towner thread = %08x\n", rwl->hOwningThreadId );
    }
}

/*
 *	misc functions
 */

/******************************************************************************
 *	DbgPrint	[NTDLL.@]
 */
void WINAPIV DbgPrint(LPCSTR fmt, ...)
{
       char buf[512];
       va_list args;

       va_start(args, fmt);
       vsprintf(buf,fmt, args);
       va_end(args);

	MESSAGE("DbgPrint says: %s",buf);
	/* hmm, raise exception? */
}

/******************************************************************************
 *  RtlAcquirePebLock		[NTDLL.@]
 */
VOID WINAPI RtlAcquirePebLock(void)
{
    RtlEnterCriticalSection( &peb_lock );
}

/******************************************************************************
 *  RtlReleasePebLock		[NTDLL.@]
 */
VOID WINAPI RtlReleasePebLock(void)
{
    RtlLeaveCriticalSection( &peb_lock );
}

/******************************************************************************
 *  RtlIntegerToChar	[NTDLL.@]
 */
DWORD WINAPI RtlIntegerToChar(DWORD x1,DWORD x2,DWORD x3,DWORD x4) {
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4);
	return 0;
}
/******************************************************************************
 *  RtlSetEnvironmentVariable		[NTDLL.@]
 */
DWORD WINAPI RtlSetEnvironmentVariable(DWORD x1,PUNICODE_STRING key,PUNICODE_STRING val) {
	FIXME("(0x%08lx,%s,%s),stub!\n",x1,debugstr_w(key->Buffer),debugstr_w(val->Buffer));
	return 0;
}

/******************************************************************************
 *  RtlNewSecurityObject		[NTDLL.@]
 */
DWORD WINAPI RtlNewSecurityObject(DWORD x1,DWORD x2,DWORD x3,DWORD x4,DWORD x5,DWORD x6) {
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx),stub!\n",x1,x2,x3,x4,x5,x6);
	return 0;
}

/******************************************************************************
 *  RtlDeleteSecurityObject		[NTDLL.@]
 */
DWORD WINAPI RtlDeleteSecurityObject(DWORD x1) {
	FIXME("(0x%08lx),stub!\n",x1);
	return 0;
}

/**************************************************************************
 *                 RtlNormalizeProcessParams		[NTDLL.@]
 */
LPVOID WINAPI RtlNormalizeProcessParams(LPVOID x)
{
    FIXME("(%p), stub\n",x);
    return x;
}

/**************************************************************************
 *                 RtlGetNtProductType			[NTDLL.@]
 */
BOOLEAN WINAPI RtlGetNtProductType(LPDWORD type)
{
    FIXME("(%p): stub\n", type);
    *type=3; /* dunno. 1 for client, 3 for server? */
    return 1;
}

/**************************************************************************
 *                 _chkstk				[NTDLL.@]
 *
 * Glorified "enter xxxx".
 */
void WINAPI NTDLL_chkstk( CONTEXT86 *context )
{
    context->Esp -= context->Eax;
}

/**************************************************************************
 *                 _alloca_probe		        [NTDLL.@]
 *
 * Glorified "enter xxxx".
 */
void WINAPI NTDLL_alloca_probe( CONTEXT86 *context )
{
    context->Esp -= context->Eax;
}

/**************************************************************************
 *                 RtlDosPathNameToNtPathName_U		[NTDLL.@]
 *
 * FIXME: convert to UNC or whatever is expected here
 */
BOOLEAN  WINAPI RtlDosPathNameToNtPathName_U(
	LPWSTR from,PUNICODE_STRING us,DWORD x2,DWORD x3)
{
    FIXME("(%s,%p,%08lx,%08lx)\n",debugstr_w(from),us,x2,x3);
    if (us) RtlCreateUnicodeString( us, from );
    return TRUE;
}


/***********************************************************************
 *           RtlImageNtHeader   (NTDLL.@)
 */
PIMAGE_NT_HEADERS WINAPI RtlImageNtHeader(HMODULE hModule)
{
    IMAGE_NT_HEADERS *ret = NULL;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)hModule;

    if (dos->e_magic == IMAGE_DOS_SIGNATURE)
    {
        ret = (IMAGE_NT_HEADERS *)((char *)dos + dos->e_lfanew);
        if (ret->Signature != IMAGE_NT_SIGNATURE) ret = NULL;
    }
    return ret;
}

/***********************************************************************
 *          RtlImageRvaToSection    (NTDLL.@)
 */
PIMAGE_SECTION_HEADER WINAPI RtlImageRvaToSection(const IMAGE_NT_HEADERS *ntHeaders, HMODULE base, DWORD rva){
    int                     i;
    int                     sectionCount;
    IMAGE_SECTION_HEADER *  nextSection = NULL;


    /* make sure we got a valid header */
    if (ntHeaders == NULL)
        return NULL;

    /* this parameter is reserved for future use */
    /*if (base != 0)
        return NULL;*/


    /* MSDN says the section table starts immediately after the file and optional headers */
    sectionCount = ntHeaders->FileHeader.NumberOfSections;
    nextSection = (IMAGE_SECTION_HEADER *)(((BYTE *)&ntHeaders->OptionalHeader) + ntHeaders->FileHeader.SizeOfOptionalHeader);


    for (i = 0; i < sectionCount; i++){

        /* each section table entry conveniently contains its mapped virtual address and size on disk.  Just test to
           see if the address we're interested in is in the range of this section.  All addresses seem to be relative
           to the image's base address (which is ignored in this call) */
        if (rva >= nextSection->VirtualAddress && rva < nextSection->VirtualAddress + nextSection->SizeOfRawData)
            return nextSection;

        nextSection++;
    }

    return NULL;
}

/***********************************************************************
 *          RtlImageRvaToVa    (NTDLL.@)
 */
PVOID WINAPI RtlImageRvaToVa(const IMAGE_NT_HEADERS *ntHeaders, HMODULE base, DWORD rva, IMAGE_SECTION_HEADER **lastRvaSection){
    IMAGE_SECTION_HEADER *section = NULL;


    /* test the last used section for the address first.  We could luck out and the new 
       RVA will also be in the same section. */
    if (lastRvaSection)
        section = *lastRvaSection;

    /* a previous section was passed in => test that one first */
    if (section){

        /* test to see if the requested RVA is in the section that was last used */
        if (rva >= section->VirtualAddress && rva < section->VirtualAddress + section->SizeOfRawData){

            /* the address will be the memory mapped base of the section plus the RVA's 
               offset into the section.  In this case <lastRvaSection> will keep the
               same value. */
            return (PVOID)(((ULONG_PTR)base + section->PointerToRawData) + (rva - section->VirtualAddress));
        }
    }

    
    /* find the section that the requested RVA is in */
    section = RtlImageRvaToSection(ntHeaders, base, rva);


    /* section not found => fail */
    if (section == NULL)
        return NULL;

    /* save the last used section for an optimized search next time */
    if (lastRvaSection)
        *lastRvaSection = section;


    /* the address will be the memory mapped base of the section plus the RVA's 
       offset into the section. */
    return (PVOID)(((ULONG_PTR)base + section->PointerToRawData) + (rva - section->VirtualAddress));
}

/***********************************************************************
 *          RtlImageDirectoryEntryToData    (NTDLL.@)
 */
PVOID WINAPI RtlImageDirectoryEntryToData(HMODULE base, BOOL mappedAsImage, WORD directoryEntry, ULONG *size){
    IMAGE_NT_HEADERS *  ntHeader;
    DWORD               va;


    /* invalid image base => fail */
    if (!base)
        return NULL;


    /* attempt to retrieve the PE header */
    ntHeader = RtlImageNtHeader(base);

    /* could not retrieve header => fail */
    if (ntHeader == NULL)
        return NULL;


    /* invalid directory entry name => fail */
    if (directoryEntry >= ntHeader->OptionalHeader.NumberOfRvaAndSizes)
        return NULL;


    /* grab the virtual address and section size */
    va = ntHeader->OptionalHeader.DataDirectory[directoryEntry].VirtualAddress;

    if (size)
        *size = (ULONG)ntHeader->OptionalHeader.DataDirectory[directoryEntry].Size;

    /* the section does not exist in this module => fail */
    if (va == 0)
        return NULL;


    /* the file is already mapped as a loaded executeable => the VA will already have been updated */
    if (mappedAsImage)
        return (PVOID)(((ULONG_PTR)base) + va);


    /* the file is not mapped -> the listed VA is actually an RVA => convert it */
    return RtlImageRvaToVa(ntHeader, base, va, NULL);
}


/******************************************************************************
 *  RtlCreateEnvironment		[NTDLL.@]
 */
DWORD WINAPI RtlCreateEnvironment(DWORD x1,DWORD x2) {
	FIXME("(0x%08lx,0x%08lx),stub!\n",x1,x2);
	return 0;
}


/******************************************************************************
 *  RtlDestroyEnvironment		[NTDLL.@]
 */
DWORD WINAPI RtlDestroyEnvironment(DWORD x) {
	FIXME("(0x%08lx),stub!\n",x);
	return 0;
}

/******************************************************************************
 *  RtlQueryEnvironmentVariable_U		[NTDLL.@]
 */
DWORD WINAPI RtlQueryEnvironmentVariable_U(DWORD x1,PUNICODE_STRING key,PUNICODE_STRING val) {
	FIXME("(0x%08lx,%s,%p),stub!\n",x1,debugstr_w(key->Buffer),val);
	return 0;
}
/******************************************************************************
 *  RtlInitializeGenericTable		[NTDLL.@]
 */
DWORD WINAPI RtlInitializeGenericTable(void)
{
	FIXME("\n");
	return 0;
}

/******************************************************************************
 *  RtlCopyMemory   [NTDLL]
 *
 */
#undef RtlCopyMemory
VOID WINAPI RtlCopyMemory( VOID *Destination, CONST VOID *Source, SIZE_T Length )
{
    memcpy(Destination, Source, Length);
}

/******************************************************************************
 *  RtlMoveMemory   [NTDLL.@]
 */
#undef RtlMoveMemory
VOID WINAPI RtlMoveMemory( VOID *Destination, CONST VOID *Source, SIZE_T Length )
{
    memmove(Destination, Source, Length);
}

/******************************************************************************
 *  RtlFillMemory   [NTDLL.@]
 */
#undef RtlFillMemory
VOID WINAPI RtlFillMemory( VOID *Destination, SIZE_T Length, BYTE Fill )
{
    memset(Destination, Fill, Length);
}

/******************************************************************************
 *  RtlZeroMemory   [NTDLL.@]
 */
#undef RtlZeroMemory
VOID WINAPI RtlZeroMemory( VOID *Destination, SIZE_T Length )
{
    memset(Destination, 0, Length);
}

/******************************************************************************
 *  RtlCompareMemory   [NTDLL.@]
 */
SIZE_T WINAPI RtlCompareMemory( const VOID *Source1, const VOID *Source2, SIZE_T Length)
{
    int i;
    for(i=0; (i<Length) && (((LPBYTE)Source1)[i]==((LPBYTE)Source2)[i]); i++);
    return i;
}

/******************************************************************************
 *  RtlAssert                           [NTDLL.@]
 *
 * Not implemented in non-debug versions.
 */
void WINAPI RtlAssert(LPVOID x1,LPVOID x2,DWORD x3, DWORD x4)
{
	FIXME("(%p,%p,0x%08lx,0x%08lx),stub\n",x1,x2,x3,x4);
}

/******************************************************************************
 *  RtlGetNtVersionNumbers       [NTDLL.@]
 */
void WINAPI RtlGetNtVersionNumbers(LPDWORD lpMajor, LPDWORD lpMinor, LPDWORD lpBuild)
{
    OSVERSIONINFOA osversion;
    osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

    TRACE("(%p,%p,%p)\n", lpMajor, lpMinor, lpBuild );

    /* lets just use GetVersion, since that already has everything figured out */ 
    GetVersionExA( &osversion );

    if( lpMajor ) {
      *lpMajor = osversion.dwMajorVersion;
    }

    if( lpMinor ) {
      *lpMinor = osversion.dwMinorVersion;
    }

    if( lpBuild ) {
      *lpBuild = osversion.dwBuildNumber;
    }
}

/*************************************************************************
 * RtlFillMemoryUlong   [NTDLL.@]
 *
 * Fill memory with a 32 bit (dword) value.
 *
 * PARAMS
 *  lpDest  [I] Bitmap pointer
 *  ulCount [I] Number of dwords to write
 *  ulValue [I] Value to fill with
 *
 * RETURNS
 *  Nothing.
 */
VOID WINAPI RtlFillMemoryUlong(ULONG* lpDest, ULONG ulCount, ULONG ulValue)
{
  TRACE("(%p,%ld,%ld)\n", lpDest, ulCount, ulValue);

  while(ulCount--)
    *lpDest++ = ulValue;
}

/*************************************************************************
 * RtlGetLongestNtPathLength    [NTDLL.@]
 *
 * Get the longest allowed path length
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  The longest allowed path length (277 characters under Win2k).
 */
DWORD WINAPI RtlGetLongestNtPathLength(void)
{
  TRACE("()\n");
  return 277;
}

/************************************************************************
 * RtlCreateQueryDebugBuffer
 */
void * RtlCreateQueryDebugBuffer(ULONG size, BOOLEAN eventpair) 
{
  FIXME("(%lu, %d) stub!\n", size, eventpair);
  return NULL;
}
