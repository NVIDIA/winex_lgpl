/*
 * File minidump.c - management of dumps (read & write)
 *
 * Copyright (C) 2004-2005, Eric Pouech
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <time.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

/*#include "ntstatus.h"*/
#include "winnt.h"
#define WIN32_NO_STATUS
#include "dbghelp_private.h"
#include "winternl.h"
#include "psapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);


#define MINIDUMP_MEMINFO_INITIAL_COUNT  64
#define MINIDUMP_MEMINFO_INCREMENT      32


struct dump_memory
{
    ULONG64                             base;
    ULONG                               size;
    ULONG                               rva;
    MINIDUMP_MEMORY_INFO *              info;   /* info block that contains this memory block */
};

struct dump_module
{
    unsigned                            is_elf;
    ULONG_PTR                           base;
    ULONG                               size;
    DWORD                               timestamp;
    DWORD                               checksum;
    WCHAR                               name[MAX_PATH];
};

struct dump_context
{
    /* process & thread information */
    HANDLE                              hProcess;
    DWORD                               pid;
    void*                               pcs_buffer;
    SYSTEM_PROCESS_INFORMATION*         spi;
    /* module information */
    struct dump_module*                 modules;
    unsigned                            num_modules;
    /* exception information */
    /* output information */
    MINIDUMP_TYPE                       type;
    HANDLE                              hFile;
    RVA                                 rva;
    struct dump_memory*                 mem;
    unsigned                            num_mem;
    /* callback information */
    MINIDUMP_CALLBACK_INFORMATION*      cb;
};


/******************************** memory block list utility functions ****************************************/
static MINIDUMP_MEMORY_INFO *getMemoryInfoTable(const MINIDUMP_MEMORY_INFO_LIST *infoList, unsigned i);

/* getNextMemInfo(): grows the memory info list block if needed and returns the next block
     in the list.  The list is stored in the buffer pointed to by <block>.  The counter
     indicating the next item to return in the list is stored internally in the <NumberOfEntries>
     member of the structure stored in <block>.  The current maximum capacity of the table
     is passed in through <_max> and the new maximum size is returned through the same value.
     Returns a pointer to the next item in the list and increments the internal counter. */
static MINIDUMP_MEMORY_INFO *getNextMemInfo(MINIDUMP_MEMORY_INFO_LIST **block, ULONG64 *_max){
    MINIDUMP_MEMORY_INFO_LIST * tmp = *block;
    ULONG64                     max = *_max;
    MINIDUMP_MEMORY_INFO *      result;


    /* the block has not been allocated yet => allocate and return the address of the first item */
    if (max == 0 || tmp == NULL){
        max = MINIDUMP_MEMINFO_INITIAL_COUNT;

        tmp = (MINIDUMP_MEMORY_INFO_LIST *)HeapAlloc(GetProcessHeap(), 
                                                     0,
                                                     (SIZE_T)(sizeof(MINIDUMP_MEMORY_INFO_LIST) + 
                                                              sizeof(MINIDUMP_MEMORY_INFO) * max));

        /* initialize the header */
        tmp->NumberOfEntries = 0;
        tmp->SizeOfEntry = sizeof(MINIDUMP_MEMORY_INFO);
        tmp->SizeOfHeader = sizeof(MINIDUMP_MEMORY_INFO_LIST);
    }

    /* the block is already allocated => return the next item or resize */
    else{

        /* the block is full => resize it and return the first new item */
        if (tmp->NumberOfEntries >= max){
            max += MINIDUMP_MEMINFO_INCREMENT;

            tmp = (MINIDUMP_MEMORY_INFO_LIST *)HeapReAlloc(GetProcessHeap(), 
                                                           0,
                                                           tmp,
                                                           (SIZE_T)(sizeof(MINIDUMP_MEMORY_INFO_LIST) + 
                                                                    sizeof(MINIDUMP_MEMORY_INFO) * max));
        }
    }


    /* grab the next item in the table */
    result = getMemoryInfoTable(tmp, tmp->NumberOfEntries++);

    /* update the list parameters */
    *_max = max;
    *block = tmp;

    return result;
}

/* getMemoryInfoTable(): returns a pointer to the first memory info block in a memory
     info list.  The info list begins immediately after the list header. */
static MINIDUMP_MEMORY_INFO *getMemoryInfoTable(const MINIDUMP_MEMORY_INFO_LIST *infoList, unsigned i){
    return &((MINIDUMP_MEMORY_INFO *)(infoList + 1))[i];
}


/* writeBufferToFile(): writes the single buffer identified by <mem> to the dump file
     identified by <dc>.  The block is written starting at the offset <rva>.  Returns
     the number of bytes that was actually written to the file. */
static ULONG64 writeBufferToFile(struct dump_context* dc, RVA64 rva, const MINIDUMP_MEMORY_DESCRIPTOR64 *mem){
    DWORD   written;
    LONG    rvaHigh = (LONG)((rva >> 32) & 0xffffffff);
    ULONG64 totalSize = 0;
    ULONG64 pos;
    DWORD   len;
    SIZE_T  read;
    BYTE    buffer[1024];


    TRACE("writing the memory buffer 0x%016llx to file {dataSize = %lld bytes, rva = 0x%016llx}\n",
            mem->StartOfMemoryRange,
            mem->DataSize,
            rva);


    /* move the file pointer to the requested location */
    SetFilePointer(dc->hFile, (LONG)(rva & 0xffffffff), &rvaHigh, FILE_BEGIN);

    /* copy the memory block into the file one buffer at a time */
    for (pos = 0; pos < mem->DataSize; pos += sizeof(buffer)){
        len = (DWORD)min(mem->DataSize - pos, sizeof(buffer));

        /* successfully read the next buffer => write it to the file */
        if (ReadProcessMemory(  dc->hProcess,
                                (void *)(DWORD_PTR)(mem->StartOfMemoryRange + pos),
                                buffer,
                                len,
                                &read))
        {
            TRACE("successfully read %tu bytes from address %p {len = %d, pos = %lld}\n", read, (void *)(DWORD_PTR)(mem->StartOfMemoryRange + pos), len, pos);
            WriteFile(dc->hFile, buffer, read, &written, NULL);
            totalSize += written;
        }

        else
            ERR("failed to read the block from address %p {len = %d, pos = %lld}\n", (void *)(DWORD_PTR)(mem->StartOfMemoryRange + pos), len, pos);
    }

    TRACE("    wrote %lld bytes to the file\n", totalSize);

    /* return the total number of bytes written */
    return totalSize;
}


int memBlockCompare(const void *_a, const void *_b){
    const struct dump_memory *a = (const struct dump_memory *)_a;
    const struct dump_memory *b = (const struct dump_memory *)_b;


    if (a->base < b->base)
        return -1;

    if (a->base > b->base)
        return 1;

    return 0;
}

/* gatherBlockInfo(): gathers information about a single block of memory.  The block is first
     queried to see if there is full virtual memory information about it.  If it does exist,
     the information is simply copied into the next entry in the <infoList> array (<max> will
     also be updated in this case).  If the buffer does not have virtual memory information,
     but a size has been specified for the block (ie: it is already known to exist, but not
     necessarily be known to the windows side of the app), a dummy info block is filled in
     saying it is a committed read/write block of the specified size. */
MINIDUMP_MEMORY_INFO *gatherBlockInfo(ULONG64                     base, 
                                      ULONG                       size, 
                                      HANDLE                      process, 
                                      MINIDUMP_MEMORY_INFO_LIST **infoList, 
                                      ULONG64 *                   max)
{
    MEMORY_BASIC_INFORMATION    memInfo = {0};
    SIZE_T                      success;
    MINIDUMP_MEMORY_INFO *      info;


    TRACE("{base = 0x%016llx, size = %d, process = %p, infoList = %p, max = %p}\n",
            base, size, process, infoList, max);


    /* grab information about the next block of memory */
    /* FIXME: add support for MEMORY_BASIC_INFORMATION64 to VirtualQuery[Ex]() and
              use that struct here instead. */
    success = VirtualQueryEx(process, (LPVOID)(DWORD_PTR)base, &memInfo, sizeof(memInfo));

    /* successfully grabbed info about a memory region => parse it into the list */
    if (success)
    {
        TRACE("successfully grabbed the info for memory block at %p {AllocationProtect = 0x%04x, Protect = 0x%04x, RegionSize = %d, State = %d, Type = %d, size = %d}\n",
                memInfo.AllocationBase, memInfo.AllocationProtect, memInfo.Protect, memInfo.RegionSize, memInfo.State, memInfo.Type, size);

        /* grow the list and grab the next block */
        info = getNextMemInfo(infoList, max);

        /* fill in the info block.  If a block size was specified in the <size> parameter
           we'll override the reported region size if <size> is larger.  This allows us
           to report the entire known block as a single memory block. */
        info->AllocationBase =      (ULONG64)(DWORD_PTR)memInfo.AllocationBase;
        info->AllocationProtect =   memInfo.AllocationProtect;
        info->BaseAddress =         (ULONG64)(DWORD_PTR)memInfo.BaseAddress;
        info->Protect =             memInfo.Protect;
        info->RegionSize =          max(size, memInfo.RegionSize);
        info->State =               memInfo.State;
        info->Type =                memInfo.Type;
        info->__alignment1 =        0;
        info->__alignment2 =        0;

        /* return the info block that was filled in */
        return info;
    }

    /* couldn't grab info about the block but we were told about its size => create a 
         dummy info block for the memory region */
    else if (size)
    {
        /* grow the list and grab the next block */
        info = getNextMemInfo(infoList, max);

        TRACE("failed to grab info for memory block at 0x%08llx but pretending it did anyway {size = %d, AllocationProtect = 0x%04x}\n",
                base, size, info->AllocationProtect);

        /* fill in dummy information for the block */
        info->AllocationBase =      base;
        info->AllocationProtect =   PAGE_READWRITE;
        info->BaseAddress =         base;
        info->Protect =             info->AllocationProtect;
        info->RegionSize =          size;
        info->State =               MEM_COMMIT;
        info->Type =                MEM_IMAGE;
        info->__alignment1 =        0;
        info->__alignment2 =        0;

        /* return the info block that was filled in */
        return info;        
    }


    /* couldn't get info on the block and it wasn't a known non-windows side block */
    return NULL;
}

/* gatherMemoryInfoList(): gathers a list of all memory blocks in use by the process
     <process>.  This includes pages that are marked as free, reserved, and committed.
     Returns a pointer to the header that contains all the block information.  The
     getMemoryInfoTable() function can be used to access the actual table of block
     info structures.  The list must be disposed of using the destroyMemoryInfoList()
     function. */
static MINIDUMP_MEMORY_INFO_LIST *gatherMemoryInfoList(const struct dump_context *dc, HANDLE process){
    ULONG64                     base;
    ULONG64                     stop;
    ULONG64                     result;
    ULONG64                     max = 0;
    MINIDUMP_MEMORY_INFO_LIST * infoList = NULL;
    MINIDUMP_MEMORY_INFO *      info;
    SYSTEM_INFO                 sysInfo;
    unsigned int                i = 0;
    ULONG32                     size;


    TRACE("gathering the memory block list for process %p\n", process);

    /* grab some system info so we can figure out the min and max addresses for
       the process, and the page size for the process. */
    GetSystemInfo(&sysInfo);

    /* set the starting and ending addresses */
    base = (ULONG64)(DWORD_PTR)sysInfo.lpMinimumApplicationAddress;
    stop = (ULONG64)(DWORD_PTR)sysInfo.lpMaximumApplicationAddress;

    TRACE("retrieved system info {pageSize = %d bytes, baseAddr = 0x%016llx, topAddr = 0x%016llx}\n", sysInfo.dwPageSize, base, stop);


    /* make sure the collected memory blocks list is sorted so it can be easily merged. */
    if (dc->num_mem){
        TRACE("sorting the collected memory block list\n");

        /* NOTE: this is rather bizarre, but "sizeof(*dc->mem)" is not the same as 
                 "sizeof(struct dump_memory)" even though <dc::mem> is a member of
                 type "struct dump_memory *".  Using sizeof(*dc->mem) returns 16 
                 bytes, and sizeof(struct dump_memory) returns 20 bytes.  However,
                 all pointer math is done using a struct size of 16 bytes (ie: 
                 "dc->mem[1] == dc->mem[0] + 0x10").  This throws off our sort and
                 any pointer math done on the buffer for that matter. */
        qsort(dc->mem, dc->num_mem, sizeof(*dc->mem), memBlockCompare);
    }


    i = 0;

    /* query every page address for its information.  Unallocated pages will fail
       to retrieve information and the base pointer will be incremented by one page.
       When a block is successfully queried, its information is stored and testing
       resumes at the address immediately following the block. */
    while (base < stop){
        size = 0;

        /* we have to merge in the collected memory blocks list.  Make sure the 
           list stays in numerical order. */
        if (i < dc->num_mem){

            /* the next collected block matches the current test block => pass in 
                 the size of the collected block so it can be included in the info */
            if (base == dc->mem[i].base)
                size = dc->mem[i].size;
        }


        info = gatherBlockInfo(base, size, process, &infoList, &max);
        result = info->RegionSize;


        /* info block was successfully gathered => increment by the region size */
        if (info){

            /* check to see if any collected blocks were contained by the last test block */
            while (i < dc->num_mem){

                /* the block was fully contained in the test block => mark it as contained and continue */
                if (dc->mem[i].base >= base && dc->mem[i].base + dc->mem[i].size <= base + info->RegionSize){
                    TRACE("the block {base = 0x%016llx, size = %u, rva = %u} was fully contained in the block {base = 0x%016llx, size = %llu}.  Linking to info block %p\n",
                            dc->mem[i].base, dc->mem[i].size, dc->mem[i].rva,
                            base, info->RegionSize,
                            info);

                    dc->mem[i].info = info;
                    i++;

                    continue;
                }

                /* the block straddled the end of the test block => extend the test block to include it and continue */
                else if (dc->mem[i].base >= base && 
                         dc->mem[i].base < base + info->RegionSize && 
                         dc->mem[i].base + dc->mem[i].size > base + info->RegionSize)
                {
                    TRACE("the block {base = 0x%016llx, size = %u, rva = %u} was partially contained in the block {base = 0x%016llx, size = %llu}.  Extending by %u bytes.  Linking to info block %p\n",
                            dc->mem[i].base, dc->mem[i].size, dc->mem[i].rva,
                            base, info->RegionSize,
                            (ULONG32)((dc->mem[i].base + dc->mem[i].size) - (base + info->RegionSize)),
                            info);

                    info->RegionSize += (dc->mem[i].base + dc->mem[i].size) - (base + info->RegionSize);
                    dc->mem[i].info = info;

                    i++;

                    continue;
                }


                break;
            }

            /* increment by the original size of the block.  This is done in case the
               next memory block is not contiguous with the updated size of the buffer
               after checking on the collected blocks.  This will only be different
               from <info::RegionSize> if the collected block straddled the end of the
               test block.  In this case there will be some overlap in the dump file 
               if the next address is also allocated. */
            base += result;
        }

        /* couldn't get info on the block => increment by the page size */
        else
            base += sysInfo.dwPageSize;
    }


    TRACE("found %llu blocks in use\n", infoList->NumberOfEntries);

    return infoList;
}

/* destroyMemoryInfoList(): destroys the memory info list <memList> that was allocated
     by a previous call to gatherMemoryInfoList(). */
static void destroyMemoryInfoList(MINIDUMP_MEMORY_INFO_LIST *memList){
    TRACE("destroying the memory info list %p\n", memList);

    if (memList)
        HeapFree(GetProcessHeap(), 0, memList);
}


/* reorganizeDirectoryCompare(): comparison helper function for qsort().  This organizes
     the directory entries according to the <StreamType> member.  The Memory64ListStream
     is treated the same as the MemoryListStream type.  All UnusedStream streams are 
     moved to the end of the directory. */
static int reorganizeDirectoryCompare(const void *a, const void *b){
    ULONG32 cmpA = ((const MINIDUMP_DIRECTORY *)a)->StreamType;
    ULONG32 cmpB = ((const MINIDUMP_DIRECTORY *)b)->StreamType;


    /* when doing a full memory dump the Memory64ListStream stream takes the
       place of the MemoryListStream stream in the directory.  We'll use that
       identifier for a sort key instead. */
    if (cmpA == Memory64ListStream)
        cmpA = MemoryListStream;

    if (cmpB == Memory64ListStream)
        cmpB = MemoryListStream;

    /* unused streams must be at the end of the directory */
    if (cmpA == UnusedStream){
        if (cmpB == UnusedStream)
            return 0;

        return 1;
    }

    if (cmpB == UnusedStream)
        return -1;


    return cmpA - cmpB;
}

/* reorganizeDirectory(): reorganizes the stream directory to match the order that windbg
     expects it.  This is sorted by stream number with a couple exceptions.  All unused
     directory entries will be cleared to UnusedStream. */
static void reorganizeDirectory(MINIDUMP_DIRECTORY *mdDir, DWORD nStreams, DWORD idx_stream){
    MINIDUMP_DIRECTORY      emptyDir = {UnusedStream, {0, 0}};
    DWORD                   i;


    /* make sure to clear all unused entries. */
    for (i = idx_stream; i < nStreams; i++)
        memcpy(&mdDir[i], &emptyDir, sizeof(MINIDUMP_DIRECTORY));


    /* sort the directory */
    qsort(mdDir, nStreams, sizeof(MINIDUMP_DIRECTORY), reorganizeDirectoryCompare);
}

/*************************************************************************************************************/

/******************************************************************
 *		fetch_processes_info
 *
 * reads system wide process information, and make spi point to the record
 * for process of id 'pid'
 */
static BOOL fetch_processes_info(struct dump_context* dc)
{
    ULONG       buf_size = 0x1000;
    NTSTATUS    nts;

    dc->pcs_buffer = NULL;
    if (!(dc->pcs_buffer = HeapAlloc(GetProcessHeap(), 0, buf_size))) return FALSE;
    for (;;)
    {
        nts = NtQuerySystemInformation(SystemProcessInformation, 
                                       dc->pcs_buffer, buf_size, NULL);
        if (nts != STATUS_INFO_LENGTH_MISMATCH) break;
        dc->pcs_buffer = HeapReAlloc(GetProcessHeap(), 0, dc->pcs_buffer, 
                                     buf_size *= 2);
        if (!dc->pcs_buffer) return FALSE;
    }

    if (nts == STATUS_SUCCESS)
    {
        dc->spi = dc->pcs_buffer;
        for (;;)
        {
            if (dc->spi->dwProcessID == dc->pid) return TRUE;
            if (!dc->spi->dwOffset) break;
            dc->spi = (SYSTEM_PROCESS_INFORMATION*)     
                ((char*)dc->spi + dc->spi->dwOffset);
        }
    }
    HeapFree(GetProcessHeap(), 0, dc->pcs_buffer);
    dc->pcs_buffer = NULL;
    dc->spi = NULL;
    return FALSE;
}

static void fetch_thread_stack(struct dump_context* dc, const void* teb_addr,
                               const CONTEXT* ctx, MINIDUMP_MEMORY_DESCRIPTOR* mmd)
{
    NT_TIB      tib;

    if (ReadProcessMemory(dc->hProcess, teb_addr, &tib, sizeof(tib), NULL))
    {
#ifdef __i386__
        /* limiting the stack dumping to the size actually used */
        if (ctx->Esp){

            /* make sure ESP is within the established range of the stack.  It could have
               been clobbered by whatever caused the original exception. */
            if (ctx->Esp - 4 < (ULONG_PTR)tib.StackLimit || ctx->Esp - 4 > (ULONG_PTR)tib.StackBase)
                mmd->StartOfMemoryRange = (ULONG_PTR)tib.StackLimit;

            else
                mmd->StartOfMemoryRange = (ctx->Esp - 4);
        }

        else
            mmd->StartOfMemoryRange = (ULONG_PTR)tib.StackLimit;

#elif defined(__powerpc__)
        if (ctx->Iar){

            /* make sure IAR is within the established range of the stack.  It could have
               been clobbered by whatever caused the original exception. */
            if (ctx->Iar - 4 < (ULONG_PTR)tib.StackLimit || ctx->Iar - 4 > (ULONG_PTR)tib.StackBase)
                mmd->StartOfMemoryRange = (ULONG_PTR)tib.StackLimit;

            else
                mmd->StartOfMemoryRange = (ctx->Iar - 4);
        }

        else
            mmd->StartOfMemoryRange = (ULONG_PTR)tib.StackLimit;

#elif defined(__x86_64__)
        if (ctx->Rsp){

            /* make sure RSP is within the established range of the stack.  It could have
               been clobbered by whatever caused the original exception. */
            if (ctx->Rsp - 8 < (ULONG_PTR)tib.StackLimit || ctx->Rsp - 8 > (ULONG_PTR)tib.StackBase)
                mmd->StartOfMemoryRange = (ULONG_PTR)tib.StackLimit;

            else
                mmd->StartOfMemoryRange = (ctx->Rsp - 8);
        }

        else
            mmd->StartOfMemoryRange = (ULONG_PTR)tib.StackLimit;

#else
#error unsupported CPU
#endif
        mmd->Memory.DataSize = (ULONG32)(ULONG_PTR)((ULONG_PTR)tib.StackBase - mmd->StartOfMemoryRange);
    }
}

/******************************************************************
 *		fetch_thread_info
 *
 * fetches some information about thread of id 'tid'
 */
static BOOL fetch_thread_info(struct dump_context* dc, int thd_idx,
                              const MINIDUMP_EXCEPTION_INFORMATION* except,
                              MINIDUMP_THREAD* mdThd, CONTEXT* ctx)
{
    DWORD                       tid = dc->spi->ti[thd_idx].dwThreadID;
    HANDLE                      hThread;
    THREAD_BASIC_INFORMATION    tbi;

    memset(ctx, 0, sizeof(*ctx));

    mdThd->ThreadId = dc->spi->ti[thd_idx].dwThreadID;
    mdThd->SuspendCount = 0;
    mdThd->Teb = 0;
    mdThd->Stack.StartOfMemoryRange = 0;
    mdThd->Stack.Memory.DataSize = 0;
    mdThd->Stack.Memory.Rva = 0;
    mdThd->ThreadContext.DataSize = 0;
    mdThd->ThreadContext.Rva = 0;
    mdThd->PriorityClass = dc->spi->ti[thd_idx].dwBasePriority; /* FIXME */
    mdThd->Priority = dc->spi->ti[thd_idx].dwCurrentPriority;


    if ((hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, tid)) == 0)
    {
        FIXME("Couldn't open thread %u (%u)\n",
              dc->spi->ti[thd_idx].dwThreadID, GetLastError());
        return FALSE;
    }
    
    if (NtQueryInformationThread(hThread, ThreadBasicInformation,
                                 &tbi, sizeof(tbi), NULL) == STATUS_SUCCESS)
    {
        mdThd->Teb = (ULONG_PTR)tbi.TebBaseAddress;
        if (tbi.ExitStatus == STILL_ACTIVE)
        {
            if (tid != GetCurrentThreadId() &&
                (mdThd->SuspendCount = SuspendThread(hThread)) != (DWORD)-1)
            {
                ctx->ContextFlags = CONTEXT_FULL;
                if (!GetThreadContext(hThread, ctx))
                    memset(ctx, 0, sizeof(*ctx));

                fetch_thread_stack(dc, tbi.TebBaseAddress, ctx, &mdThd->Stack);
                ResumeThread(hThread);
            }
            else if (tid == GetCurrentThreadId() && except)
            {
                CONTEXT lctx, *pctx;
                mdThd->SuspendCount = 1;

                /* make sure the <ClientPointers> flag isn't set incorrectly.  It shouldn't be a problem
                   if it is set incorrectly, it'll just go down a slower path in that case since it has
                   to use ReadProcessMemory() to access all thread contexts. */
                if (except->ClientPointers && dc->hProcess != GetCurrentProcess())
                {
                    EXCEPTION_POINTERS      ep;

                    ReadProcessMemory(dc->hProcess, except->ExceptionPointers,
                                      &ep, sizeof(ep), NULL);
                    ReadProcessMemory(dc->hProcess, ep.ContextRecord,
                                      &lctx, sizeof(lctx), NULL);
                    pctx = &lctx;
                }
                else
                    pctx = except->ExceptionPointers->ContextRecord;

                memcpy(ctx, pctx, sizeof(*ctx));
                fetch_thread_stack(dc, tbi.TebBaseAddress, pctx, &mdThd->Stack);
            }
            else mdThd->SuspendCount = 0;
        }
    }
    CloseHandle(hThread);
    return TRUE;
}

/******************************************************************
 *		add_module
 *
 * Add a module to a dump context
 */
static BOOL add_module(struct dump_context* dc, const WCHAR* name,
                       DWORD_PTR base, DWORD size, DWORD timestamp, DWORD checksum,
                       BOOL is_elf)
{
    if (!dc->modules)
        dc->modules = HeapAlloc(GetProcessHeap(), 0,
                                ++dc->num_modules * sizeof(*dc->modules));
    else
        dc->modules = HeapReAlloc(GetProcessHeap(), 0, dc->modules,
                                  ++dc->num_modules * sizeof(*dc->modules));
    if (!dc->modules) return FALSE;
    if (is_elf ||
        !GetModuleFileNameExW(dc->hProcess, (HMODULE)base,
                              dc->modules[dc->num_modules - 1].name,
                              sizeof(dc->modules[dc->num_modules - 1].name) / sizeof(WCHAR)))
        lstrcpynW(dc->modules[dc->num_modules - 1].name, name,
                  sizeof(dc->modules[dc->num_modules - 1].name) / sizeof(WCHAR));
    dc->modules[dc->num_modules - 1].base = base;
    dc->modules[dc->num_modules - 1].size = size;
    dc->modules[dc->num_modules - 1].timestamp = timestamp;
    dc->modules[dc->num_modules - 1].checksum = checksum;
    dc->modules[dc->num_modules - 1].is_elf = is_elf;

    return TRUE;
}

/******************************************************************
 *		fetch_pe_module_info_cb
 *
 * Callback for accumulating in dump_context a PE modules set
 */
static BOOL WINAPI fetch_pe_module_info_cb(PCWSTR name, DWORD64 base, ULONG size,
                                           PVOID user)
{
    struct dump_context*        dc = (struct dump_context*)user;
    IMAGE_NT_HEADERS            nth;

    if (!validate_addr64(base)) return FALSE;
    base = normalize_addr64(base);

    if (pe_load_nt_header(dc->hProcess, base, &nth))
        add_module((struct dump_context*)user, name, base, size,
                   nth.FileHeader.TimeDateStamp, nth.OptionalHeader.CheckSum,
                   FALSE);
    return TRUE;
}

/******************************************************************
 *		fetch_elf_module_info_cb
 *
 * Callback for accumulating in dump_context an ELF modules set
 */
static BOOL fetch_elf_module_info_cb(const WCHAR* name, unsigned long base,
                                     void* user)
{
    struct dump_context*        dc = (struct dump_context*)user;
    DWORD                       rbase, size, checksum;

    /* FIXME: there's no relevant timestamp on ELF modules */
    /* NB: if we have a non-null base from the live-target use it (whenever
     * the ELF module is relocatable or not). If we have a null base (ELF
     * module isn't relocatable) then grab its base address from ELF file
     */
    if (!elf_fetch_file_info(name, &rbase, &size, &checksum))
        size = checksum = 0;
    add_module(dc, name, base ? base : rbase, size, 0 /* FIXME */, checksum, TRUE);
    return TRUE;
}

static void fetch_modules_info(struct dump_context* dc)
{
    EnumerateLoadedModulesW64(dc->hProcess, fetch_pe_module_info_cb, dc);
    /* Since we include ELF modules in a separate stream from the regular PE ones,
     * we can always include those ELF modules (they don't eat lots of space)
     * And it's always a good idea to have a trace of the loaded ELF modules for
     * a given application in a post mortem debugging condition.
     */
    elf_enum_modules(dc->hProcess, fetch_elf_module_info_cb, dc);
}

static void fetch_module_versioninfo(LPCWSTR filename, VS_FIXEDFILEINFO* ffi)
{
    DWORD       handle;
    DWORD       sz;
    static const WCHAR backslashW[] = {'\\', '\0'};

    memset(ffi, 0, sizeof(*ffi));
    if ((sz = GetFileVersionInfoSizeW(filename, &handle)))
    {
        void*   info = HeapAlloc(GetProcessHeap(), 0, sz);
        if (info && GetFileVersionInfoW(filename, handle, sz, info))
        {
            VS_FIXEDFILEINFO*   ptr;
            UINT    len;

            if (VerQueryValueW(info, backslashW, (void*)&ptr, &len))
                memcpy(ffi, ptr, min(len, sizeof(*ffi)));
        }
        HeapFree(GetProcessHeap(), 0, info);
    }
}

/******************************************************************
 *		add_memory_block
 *
 * Add a memory block to be dumped in a minidump
 * If rva is non 0, it's the rva in the minidump where has to be stored
 * also the rva of the memory block when written (this allows to reference
 * a memory block from outside the list of memory blocks).
 */
static void add_memory_block(struct dump_context* dc, ULONG64 base, ULONG size, ULONG rva)
{
    TRACE("adding the memory block 0x%08llx-0x%08llx {size = %d, rva = %d, num_mem = %d, mem = %p}\n",
            base, base + size - 1, size, rva, dc->num_mem, dc->mem);

    if (dc->mem)
        dc->mem = HeapReAlloc(GetProcessHeap(), 0, dc->mem, 
                              ++dc->num_mem * sizeof(*dc->mem));
    else
        dc->mem = HeapAlloc(GetProcessHeap(), 0, ++dc->num_mem * sizeof(*dc->mem));
    if (dc->mem)
    {
        dc->mem[dc->num_mem - 1].base = base;
        dc->mem[dc->num_mem - 1].size = size;
        dc->mem[dc->num_mem - 1].rva  = rva;
        dc->mem[dc->num_mem - 1].info = NULL;
    }
    else
    {
        ERR("failed to allocate memory for the new block info\n");
        dc->num_mem = 0;
    }
}

/******************************************************************
 *		writeat
 *
 * Writes a chunk of data at a given position in the minidump
 */
static void writeat(struct dump_context* dc, RVA rva, const void* data, unsigned size)
{
    DWORD       written;

    SetFilePointer(dc->hFile, rva, NULL, FILE_BEGIN);
    WriteFile(dc->hFile, data, size, &written, NULL);
}

/******************************************************************
 *		append
 *
 * writes a new chunk of data to the minidump, increasing the current
 * rva in dc
 */
static void append(struct dump_context* dc, const void* data, unsigned size)
{
    writeat(dc, dc->rva, data, size);
    dc->rva += size;
}

/******************************************************************
 *		dump_exception_info
 *
 * Write in File the exception information from pcs
 */
static  void    dump_exception_info(struct dump_context* dc,
                                    const MINIDUMP_EXCEPTION_INFORMATION* except,
                                    ULONG64 *streamSize)
{
    MINIDUMP_EXCEPTION_STREAM   mdExcpt;
    EXCEPTION_RECORD            rec, *prec;
    CONTEXT                     ctx, *pctx;
    ULONG32                     i;

    mdExcpt.ThreadId = except->ThreadId;
    mdExcpt.__alignment = 0;

    /* make sure the <ClientPointers> flag isn't set incorrectly.  It shouldn't be a problem
       if it is set incorrectly, it'll just go down a slower path in that case since it has
       to use ReadProcessMemory() to access all thread contexts. */
    if (except->ClientPointers && dc->hProcess != GetCurrentProcess())
    {
        EXCEPTION_POINTERS      ep;


        TRACE("reading exception pointers from the remote process\n");
        ReadProcessMemory(dc->hProcess, 
                          except->ExceptionPointers, &ep, sizeof(ep), NULL);
        ReadProcessMemory(dc->hProcess, 
                          ep.ExceptionRecord, &rec, sizeof(rec), NULL);
        ReadProcessMemory(dc->hProcess, 
                          ep.ContextRecord, &ctx, sizeof(ctx), NULL);
        prec = &rec;
        pctx = &ctx;
    }
    else
    {
        prec = except->ExceptionPointers->ExceptionRecord;
        pctx = except->ExceptionPointers->ContextRecord;
        TRACE("exception pointers are local to this process.  Reading directly {ExceptionRecord = %p, ContextRecord = %p}\n", prec, pctx);
    }

    /* dump out the exception record that was read/copied. */
    if (TRACE_ON(dbghelp))
    {
        TRACE("ExceptionRecord:\n");
        TRACE("    ExceptionCode =      0x%08x\n", prec->ExceptionCode);
        TRACE("    ExceptionFlags =     0x%08x\n", prec->ExceptionFlags);
        TRACE("    ExceptionRecord =    %p\n", prec->ExceptionRecord);
        TRACE("    ExceptionAddress =   %p\n", prec->ExceptionAddress);
        TRACE("    NumberParameters =   0x%08x\n", prec->NumberParameters);
        for (i = 0; i < min(prec->NumberParameters, EXCEPTION_MAXIMUM_PARAMETERS); i++)
            TRACE("        ExceptionInformation[%d] = 0x%08lx\n", i, prec->ExceptionInformation[i]);
    }

    mdExcpt.ExceptionRecord.ExceptionCode = prec->ExceptionCode;
    mdExcpt.ExceptionRecord.ExceptionFlags = prec->ExceptionFlags;
    mdExcpt.ExceptionRecord.ExceptionRecord = (DWORD_PTR)prec->ExceptionRecord;
    mdExcpt.ExceptionRecord.ExceptionAddress = (DWORD_PTR)prec->ExceptionAddress;
    mdExcpt.ExceptionRecord.NumberParameters = min(prec->NumberParameters, EXCEPTION_MAXIMUM_PARAMETERS);
    mdExcpt.ExceptionRecord.__unusedAlignment = 0;
    for (i = 0; i < mdExcpt.ExceptionRecord.NumberParameters; i++)
        mdExcpt.ExceptionRecord.ExceptionInformation[i] = (DWORD_PTR)prec->ExceptionInformation[i];
    mdExcpt.ThreadContext.DataSize = sizeof(*pctx);
    mdExcpt.ThreadContext.Rva = dc->rva + sizeof(mdExcpt);

    /* the size of this stream should be reported as the size of just the struct.  The thread context
       block exists outside this stream and is referenced by just its RVA. */
    *streamSize = sizeof(mdExcpt);

    append(dc, &mdExcpt, sizeof(mdExcpt));
    append(dc, pctx, sizeof(*pctx));

    TRACE("wrote out an exception info block {streamSize = %llu, extraSpace = %lu}\n", *streamSize, sizeof(*pctx));
}

/******************************************************************
 *		dump_modules
 *
 * Write in File the modules from pcs
 */
static  void    dump_modules(struct dump_context* dc, BOOL dump_elf, ULONG64 *streamSize)
{
    MINIDUMP_MODULE             mdModule;
    MINIDUMP_MODULE_LIST        mdModuleList;
    char                        tmp[1024];
    MINIDUMP_STRING*            ms = (MINIDUMP_STRING*)tmp;
    ULONG                       i, nmod;
    RVA                         rva_base;
    DWORD                       flags_out;

    for (i = nmod = 0; i < dc->num_modules; i++)
    {
        if ((dc->modules[i].is_elf && dump_elf) ||
            (!dc->modules[i].is_elf && !dump_elf))
            nmod++;
    }

    mdModuleList.NumberOfModules = 0;
    /* reserve space for mdModuleList
     * FIXME: since we don't support 0 length arrays, we cannot use the
     * size of mdModuleList
     * FIXME: if we don't ask for all modules in cb, we'll get a hole in the file
     */
    rva_base = dc->rva;
    dc->rva += sizeof(mdModuleList.NumberOfModules) + sizeof(mdModule) * nmod;


    for (i = 0; i < dc->num_modules; i++)
    {
        if ((dc->modules[i].is_elf && !dump_elf) ||
            (!dc->modules[i].is_elf && dump_elf))
            continue;

        flags_out = ModuleWriteModule | ModuleWriteMiscRecord | ModuleWriteCvRecord;
        if (dc->type & MiniDumpWithDataSegs)
            flags_out |= ModuleWriteDataSeg;
        if (dc->type & MiniDumpWithProcessThreadData)
            flags_out |= ModuleWriteTlsData;
        if (dc->type & MiniDumpWithCodeSegs)
            flags_out |= ModuleWriteCodeSegs;
        ms->Length = (lstrlenW(dc->modules[i].name) + 1) * sizeof(WCHAR);
        if (sizeof(ULONG) + ms->Length > sizeof(tmp))
            FIXME("Buffer overflow!!!\n");
        lstrcpyW(ms->Buffer, dc->modules[i].name);

        if (dc->cb)
        {
            MINIDUMP_CALLBACK_INPUT     cbin;
            MINIDUMP_CALLBACK_OUTPUT    cbout;

            cbin.ProcessId = dc->pid;
            cbin.ProcessHandle = dc->hProcess;
            cbin.CallbackType = ModuleCallback;

            cbin.u.Module.FullPath = ms->Buffer;
            cbin.u.Module.BaseOfImage = dc->modules[i].base;
            cbin.u.Module.SizeOfImage = dc->modules[i].size;
            cbin.u.Module.CheckSum = dc->modules[i].checksum;
            cbin.u.Module.TimeDateStamp = dc->modules[i].timestamp;
            memset(&cbin.u.Module.VersionInfo, 0, sizeof(cbin.u.Module.VersionInfo));
            cbin.u.Module.CvRecord = NULL;
            cbin.u.Module.SizeOfCvRecord = 0;
            cbin.u.Module.MiscRecord = NULL;
            cbin.u.Module.SizeOfMiscRecord = 0;

            cbout.u.ModuleWriteFlags = flags_out;
            if (!dc->cb->CallbackRoutine(dc->cb->CallbackParam, &cbin, &cbout))
                continue;
            flags_out &= cbout.u.ModuleWriteFlags;
        }
        if (flags_out & ModuleWriteModule)
        {
            mdModule.BaseOfImage = dc->modules[i].base;
            mdModule.SizeOfImage = dc->modules[i].size;
            mdModule.CheckSum = dc->modules[i].checksum;
            mdModule.TimeDateStamp = dc->modules[i].timestamp;
            mdModule.ModuleNameRva = dc->rva;
            ms->Length -= sizeof(WCHAR);
            append(dc, ms, sizeof(ULONG) + ms->Length + sizeof(WCHAR));
            fetch_module_versioninfo(ms->Buffer, &mdModule.VersionInfo);
            mdModule.CvRecord.DataSize = 0; /* FIXME */
            mdModule.CvRecord.Rva = 0; /* FIXME */
            mdModule.MiscRecord.DataSize = 0; /* FIXME */
            mdModule.MiscRecord.Rva = 0; /* FIXME */
            mdModule.Reserved0 = 0; /* FIXME */
            mdModule.Reserved1 = 0; /* FIXME */
            writeat(dc,
                    rva_base + sizeof(mdModuleList.NumberOfModules) + 
                        mdModuleList.NumberOfModules++ * sizeof(mdModule), 
                    &mdModule, sizeof(mdModule));
        }
    }
    writeat(dc, rva_base, &mdModuleList.NumberOfModules, 
            sizeof(mdModuleList.NumberOfModules));

    /* the stream size is just the size of the module index.  It does not include the data for the
       names of each module.  *Technically* the names are supposed to go into the common string table
       in the minidump file.  Since each string is referenced by RVA they can all safely be located 
       anywhere between streams in the file, so the end of this stream is sufficient. */
    *streamSize = sizeof(mdModuleList.NumberOfModules) + sizeof(mdModule) * mdModuleList.NumberOfModules;

    TRACE("wrote out %u module info blocks {streamSize = %llu, extraSpace = %u}\n", mdModuleList.NumberOfModules, *streamSize, dc->rva - rva_base - (ULONG)(*streamSize));
}

/* Calls cpuid with an eax of 'ax' and returns the 16 bytes in *p
 * We are compiled with -fPIC, so we can't clobber ebx.
 */
static inline void do_x86cpuid(unsigned int ax, unsigned int *p)
{
#if defined(__GNUC__) && defined(__i386__)
    __asm__("pushl %%ebx\n\t"
            "cpuid\n\t"
            "movl %%ebx, %%esi\n\t"
            "popl %%ebx"
            : "=a" (p[0]), "=S" (p[1]), "=c" (p[2]), "=d" (p[3])
            :  "0" (ax));
#endif
}

/* From xf86info havecpuid.c 1.11 */
static inline int have_x86cpuid(void)
{
#if defined(__GNUC__) && defined(__i386__)
    unsigned int f1, f2;
    __asm__("pushfl\n\t"
            "pushfl\n\t"
            "popl %0\n\t"
            "movl %0,%1\n\t"
            "xorl %2,%0\n\t"
            "pushl %0\n\t"
            "popfl\n\t"
            "pushfl\n\t"
            "popl %0\n\t"
            "popfl"
            : "=&r" (f1), "=&r" (f2)
            : "ir" (0x00200000));
    return ((f1^f2) & 0x00200000) != 0;
#else
    return 0;
#endif
}

/******************************************************************
 *		dump_system_info
 *
 * Dumps into File the information about the system
 */
static  void    dump_system_info(struct dump_context* dc, ULONG64 *streamSize)
{
    MINIDUMP_SYSTEM_INFO        mdSysInfo;
    SYSTEM_INFO                 sysInfo;
    OSVERSIONINFOW              osInfo;
    DWORD                       written;
    ULONG                       slen;

    GetSystemInfo(&sysInfo);
    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    GetVersionExW(&osInfo);

    mdSysInfo.ProcessorArchitecture = sysInfo.u.s.wProcessorArchitecture;
    mdSysInfo.ProcessorLevel = sysInfo.wProcessorLevel;
    mdSysInfo.ProcessorRevision = sysInfo.wProcessorRevision;
    mdSysInfo.u.s.NumberOfProcessors = (UCHAR)sysInfo.dwNumberOfProcessors;
    mdSysInfo.u.s.ProductType = VER_NT_WORKSTATION; /* FIXME */
    mdSysInfo.MajorVersion = osInfo.dwMajorVersion;
    mdSysInfo.MinorVersion = osInfo.dwMinorVersion;
    mdSysInfo.BuildNumber = osInfo.dwBuildNumber;
    mdSysInfo.PlatformId = osInfo.dwPlatformId;

    mdSysInfo.CSDVersionRva = dc->rva + sizeof(mdSysInfo);
    mdSysInfo.u1.Reserved1 = 0;
    mdSysInfo.u1.s.SuiteMask = VER_SUITE_TERMINAL;

    if (have_x86cpuid())
    {
        unsigned        regs0[4], regs1[4];

        do_x86cpuid(0, regs0);
        mdSysInfo.Cpu.X86CpuInfo.VendorId[0] = regs0[1];
        mdSysInfo.Cpu.X86CpuInfo.VendorId[1] = regs0[2];
        mdSysInfo.Cpu.X86CpuInfo.VendorId[2] = regs0[3];
        do_x86cpuid(1, regs1);
        mdSysInfo.Cpu.X86CpuInfo.VersionInformation = regs1[0];
        mdSysInfo.Cpu.X86CpuInfo.FeatureInformation = regs1[3];
        mdSysInfo.Cpu.X86CpuInfo.AMDExtendedCpuFeatures = 0;
        if (regs0[1] == 0x68747541 /* "Auth" */ &&
            regs0[3] == 0x69746e65 /* "enti" */ &&
            regs0[2] == 0x444d4163 /* "cAMD" */)
        {
            do_x86cpuid(0x80000000, regs1);  /* get vendor cpuid level */
            if (regs1[0] >= 0x80000001)
            {
                do_x86cpuid(0x80000001, regs1);  /* get vendor features */
                mdSysInfo.Cpu.X86CpuInfo.AMDExtendedCpuFeatures = regs1[3];
            }
        }
    }
    else
    {
        unsigned        i;
        ULONG64         one = 1;

        mdSysInfo.Cpu.OtherCpuInfo.ProcessorFeatures[0] = 0;
        mdSysInfo.Cpu.OtherCpuInfo.ProcessorFeatures[1] = 0;

        for (i = 0; i < sizeof(mdSysInfo.Cpu.OtherCpuInfo.ProcessorFeatures[0]) * 8; i++)
            if (IsProcessorFeaturePresent(i))
                mdSysInfo.Cpu.OtherCpuInfo.ProcessorFeatures[0] |= one << i;
    }
    append(dc, &mdSysInfo, sizeof(mdSysInfo));


    *streamSize = sizeof(MINIDUMP_SYSTEM_INFO);

    /* write the service pack version string after this stream.  It is referenced within the
       stream by its RVA in the file. */
    slen = lstrlenW(osInfo.szCSDVersion) * sizeof(WCHAR);
    WriteFile(dc->hFile, &slen, sizeof(slen), &written, NULL);
    WriteFile(dc->hFile, osInfo.szCSDVersion, slen, &written, NULL);
    dc->rva += sizeof(ULONG) + slen;

    TRACE("wrote out a system information block {streamSize = %llu, extraSpace = %lu}\n", *streamSize, sizeof(ULONG) + slen);
}

/******************************************************************
 *		dump_threads
 *
 * Dumps into File the information about running threads
 */
static  void    dump_threads(struct dump_context* dc,
                             const MINIDUMP_EXCEPTION_INFORMATION* except,
                             ULONG64 *streamSize)
{
    MINIDUMP_THREAD             mdThd;
    MINIDUMP_THREAD_LIST        mdThdList;
    unsigned                    i;
    RVA                         rva_base;
    DWORD                       flags_out;
    CONTEXT                     ctx;


    mdThdList.NumberOfThreads = 0;

    rva_base = dc->rva;
    dc->rva += sizeof(mdThdList.NumberOfThreads) +
        dc->spi->dwThreadCount * sizeof(mdThd);


    for (i = 0; i < dc->spi->dwThreadCount; i++)
    {
        fetch_thread_info(dc, i, except, &mdThd, &ctx);

        flags_out = ThreadWriteThread | ThreadWriteStack | ThreadWriteContext |
            ThreadWriteInstructionWindow;
        if (dc->type & MiniDumpWithProcessThreadData)
            flags_out |= ThreadWriteThreadData;
        if (dc->type & MiniDumpWithThreadInfo)
            flags_out |= ThreadWriteThreadInfo;

        if (dc->cb)
        {
            MINIDUMP_CALLBACK_INPUT     cbin;
            MINIDUMP_CALLBACK_OUTPUT    cbout;

            cbin.ProcessId = dc->pid;
            cbin.ProcessHandle = dc->hProcess;
            cbin.CallbackType = ThreadCallback;
            cbin.u.Thread.ThreadId = dc->spi->ti[i].dwThreadID;
            cbin.u.Thread.ThreadHandle = 0; /* FIXME */
            memcpy(&cbin.u.Thread.Context, &ctx, sizeof(CONTEXT));
            cbin.u.Thread.SizeOfContext = sizeof(CONTEXT);
            cbin.u.Thread.StackBase = mdThd.Stack.StartOfMemoryRange;
            cbin.u.Thread.StackEnd = mdThd.Stack.StartOfMemoryRange +
                mdThd.Stack.Memory.DataSize;

            cbout.u.ThreadWriteFlags = flags_out;
            if (!dc->cb->CallbackRoutine(dc->cb->CallbackParam, &cbin, &cbout))
                continue;
            flags_out &= cbout.u.ThreadWriteFlags;
        }
        if (flags_out & ThreadWriteThread)
        {
            if (ctx.ContextFlags && (flags_out & ThreadWriteContext))
            {
                mdThd.ThreadContext.Rva = dc->rva;
                mdThd.ThreadContext.DataSize = sizeof(CONTEXT);
                append(dc, &ctx, sizeof(CONTEXT));
            }
            if (mdThd.Stack.Memory.DataSize && (flags_out & ThreadWriteStack))
            {
                add_memory_block(dc, mdThd.Stack.StartOfMemoryRange, mdThd.Stack.Memory.DataSize,
                                 rva_base + sizeof(mdThdList.NumberOfThreads) +
                                     mdThdList.NumberOfThreads * sizeof(mdThd) +
                                     FIELD_OFFSET(MINIDUMP_THREAD, Stack.Memory.Rva));
            }
            writeat(dc, 
                    rva_base + sizeof(mdThdList.NumberOfThreads) +
                        mdThdList.NumberOfThreads * sizeof(mdThd),
                    &mdThd, sizeof(mdThd));
            mdThdList.NumberOfThreads++;
        }
        if (ctx.ContextFlags && (flags_out & ThreadWriteInstructionWindow))
        {
            /* FIXME: - Native dbghelp also dumps 0x80 bytes around EIP
             *        - also crop values across module boundaries, 
             *        - and don't make it i386 dependent 
             */
            /* add_memory_block(dc, ctx.Eip - 0x80, ctx.Eip + 0x80, 0); */
        }
    }
    writeat(dc, rva_base,
            &mdThdList.NumberOfThreads, sizeof(mdThdList.NumberOfThreads));


    /* size of this stream is the total size of the thread index.  The stack memory
       data and the context records are not included in this count */
    *streamSize = sizeof(mdThdList.NumberOfThreads) +
        mdThdList.NumberOfThreads * sizeof(mdThd);

    TRACE("wrote out %u thread info blocks {streamSize = %llu, extraSpace = %u}\n", mdThdList.NumberOfThreads, *streamSize, dc->rva - rva_base - (ULONG)(*streamSize));
}

/******************************************************************
 *		dump_memory_info
 *
 * dumps information about the memory of the process (stack of the threads)
 */
static void dump_memory_info(struct dump_context* dc, ULONG64 *streamSize)
{
    MINIDUMP_MEMORY_LIST        mdMemList;
    MINIDUMP_MEMORY_DESCRIPTOR  mdMem;
    DWORD                       written;
    DWORD64                     totalSize = 0;
    unsigned                    i, pos, len;
    RVA                         rva_base;
    char                        tmp[1024];


    mdMemList.NumberOfMemoryRanges = dc->num_mem;
    append(dc, &mdMemList.NumberOfMemoryRanges,
           sizeof(mdMemList.NumberOfMemoryRanges));
    rva_base = dc->rva;
    dc->rva += mdMemList.NumberOfMemoryRanges * sizeof(mdMem);

    /* the size of this stream only includes the memory list index.  It does not include
       the size of each memory block.  Memory blocks are referenced by RVA so they can
       safely be stored starting at the end of this stream. */
    *streamSize = sizeof(mdMemList.NumberOfMemoryRanges) + mdMemList.NumberOfMemoryRanges * sizeof(mdMem);

    TRACE("writing %d memory blocks streams\n", dc->num_mem);

    for (i = 0; i < dc->num_mem; i++)
    {
        TRACE("    writing memory block %d {addr = 0x%08llx, size = %u bytes, RVA = 0x%x}\n", i, dc->mem[i].base, dc->mem[i].size, dc->rva);
        mdMem.StartOfMemoryRange = dc->mem[i].base;
        mdMem.Memory.Rva = dc->rva;
        mdMem.Memory.DataSize = dc->mem[i].size;
        SetFilePointer(dc->hFile, dc->rva, NULL, FILE_BEGIN);
        for (pos = 0; pos < dc->mem[i].size; pos += sizeof(tmp))
        {
            len = min(dc->mem[i].size - pos, sizeof(tmp));
            if (ReadProcessMemory(dc->hProcess, 
                                  (void*)(ULONG_PTR)(dc->mem[i].base + pos), 
                                  tmp, len, NULL))
                WriteFile(dc->hFile, tmp, len, &written, NULL);
        }
        
        dc->rva += mdMem.Memory.DataSize;
        totalSize += mdMem.Memory.DataSize;

        writeat(dc, rva_base + i * sizeof(mdMem), &mdMem, sizeof(mdMem));
        if (dc->mem[i].rva)
        {
            writeat(dc, dc->mem[i].rva, &mdMem.Memory.Rva, sizeof(mdMem.Memory.Rva));
        }
    }

    TRACE("wrote out %u blocks of memory {streamSize = %llu, extraSpace = %llu}\n", dc->num_mem, *streamSize, totalSize);
}

static void dump_misc_info(struct dump_context* dc, ULONG64 *streamSize)
{
    MINIDUMP_MISC_INFO  mmi;

    mmi.SizeOfInfo = sizeof(mmi);
    mmi.Flags1 = MINIDUMP_MISC1_PROCESS_ID;
    mmi.ProcessId = dc->pid;

    /* FIXME: create/user/kernel time */
    mmi.ProcessCreateTime = 0;
    mmi.ProcessKernelTime = 0;
    mmi.ProcessUserTime = 0;

    append(dc, &mmi, sizeof(mmi));

    *streamSize = sizeof(mmi);
    TRACE("wrote out a misc info block {streamSize = %llu}\n", *streamSize);
}


static void updateMemBlockRVAs(struct dump_context *dc){
    RVA         memRva;
    RVA64       tmpRva;
    unsigned    i;


    TRACE("updating RVAs for collected memory blocks\n");

    /* update the RVAs of the locations that reference the dump context's memory 
       list blocks.  Now that they are known, they can be written. */
    for (i = 0; i < dc->num_mem; i++){


        /* no RVA to update => skip the block */
        if (dc->mem[i].rva == 0)
            continue;


        /* the info block does no exist for some reason => report and skip it (this shouldn't happen) */
        if (dc->mem[i].info == NULL){
            ERR("no info block?!?\n");

            continue;
        }


        /* piece together the 64-bit RVA of the base of the memory block */
        tmpRva = dc->mem[i].info->__alignment1 | (((ULONG64)dc->mem[i].info->__alignment2) << 32);

        /* calculate the offset 32-bit RVA of the start of the current collected block */
        memRva = (RVA)(tmpRva + (dc->mem[i].base - dc->mem[i].info->BaseAddress));

        TRACE("updating the RVA for the block {base = 0x%016llx, size = %u, rva = %u} to point into the block {base = 0x%016llx, size = %llu} at RVA 0x%08x {baseRva = 0x%016llx}\n",
                dc->mem[i].base, dc->mem[i].size, dc->mem[i].rva,
                dc->mem[i].info->BaseAddress, dc->mem[i].info->RegionSize,
                memRva, tmpRva);

        /* the original storage location that references the memory block stored at <memRva>
           only provides storage for a 32-bit RVA.  This is a limitation of the native file
           format and not [necessarily] a failing on our part.  This will have to fixed here
           if/when we decide to support 64-bit apps or figure out if native stores thread 
           info blocks differently on 64-bit systems.  If the RVA does actually require
           more than 32 bits we'll spew a warning and just write the truncated address. */
        if (tmpRva >> 32)
            WARN("the RVA was more than 32-bits and will be truncated on write.  Expect errors in windbg!\n");


        /* write the RVA to the file at the requested location */
        writeat(dc, dc->mem[i].rva, &memRva, sizeof(memRva));

        /* clear the memory block pointer */
        dc->mem[i].info->__alignment1 = 0;
        dc->mem[i].info->__alignment2 = 0;
        dc->mem[i].info = NULL;
    }
}

/* dumpMemoryBlocks(): dumps the contents of all currently committed memory pages used
     in the <infoList> table.  The <infoList> table will contain information about 
     reserved and free memory blocks as well, but only the committed ones will be read
     from and written to file.  The size of the stream not including the actual memory
     blocks (ie: just the header and block list) is returned through <streamSize>.  The
     memory block table will be written out to the stream preceeding the actual block
     data.  This stream must always be last in the file. */
static void dumpMemoryBlocks(struct dump_context* dc, MINIDUMP_MEMORY_INFO_LIST *infoList, ULONG64 *streamSize){
    MINIDUMP_MEMORY_INFO *          info;
    ULONG64                         count = 0;
    MINIDUMP_MEMORY64_LIST *        memList;
    ULONG64                         i;
    ULONG64                         j;
    RVA64                           rva = dc->rva;
    RVA                             listRva = dc->rva;
    SIZE_T                          memListSize;
    ULONG64                         totalSize = 0;
    ULONG64                         written;


    TRACE("writing memory blocks to the dump file {numBlocks = %llu}\n", infoList->NumberOfEntries);


    info = getMemoryInfoTable(infoList, 0);

    /* we only care about committed pages right now.  Count how many we have */
    for (i = 0; i < infoList->NumberOfEntries; i++){

        /* uncommited or no-access page -> don't care => skip it */
        if (info[i].State == MEM_COMMIT && !(info[i].Protect & PAGE_NOACCESS))
            count++;
    }


    TRACE("counted %llu committed blocks to be written out\n", count);

    /* allocate a buffer for the memory buffer list */
    memListSize = (SIZE_T)(sizeof(MINIDUMP_MEMORY64_LIST) +
                           (sizeof(MINIDUMP_MEMORY_DESCRIPTOR64) * count));

    memList = (MINIDUMP_MEMORY64_LIST *)HeapAlloc(  GetProcessHeap(), 
                                                    0,
                                                    memListSize);


    /* fill in the header */
    rva += memListSize;
    totalSize += memListSize;

    memList->NumberOfMemoryRanges = count;
    memList->BaseRva =  rva;


    /* make another pass through the list, fill in the buffer list, and write out the buffers */
    for (i = 0, j = 0; i < infoList->NumberOfEntries; i++){

        /* uncommited or no-access page -> don't care => skip it */
        if (info[i].State != MEM_COMMIT || (info[i].Protect & PAGE_NOACCESS))
            continue;


        /* fill in the memory descriptor for this block */
        memList->MemoryRanges[j].StartOfMemoryRange =   info[i].BaseAddress;
        memList->MemoryRanges[j].DataSize =             info[i].RegionSize;


        /* write the buffer to the file at the current RVA */
        TRACE("writing the buffer 0x%016llx {regionSize = 0x%016llx, state = 0x%04x, protect = 0x%04x, type = 0x%04x}\n",
                info[i].BaseAddress,
                info[i].RegionSize,
                info[i].State,
                info[i].Protect,
                info[i].Type);

        written = writeBufferToFile(dc, rva, &memList->MemoryRanges[j]);

        /* store the 64-bit RVA of the block in the info block.
           NOTE: this will not be 64-bit safe when it is written out because
                 the original block that required the updated RVA only supports
                 a 32-bit RVA.  Thus the address could be written incorrectly
                 for very large minidumps.  The native file format is the issue
                 here, not our implementation.  We'll preserve the full 64-bit
                 RVA as long as possible then generate a warning if more than
                 32-bits are required on write. */
        info[i].__alignment1 = (ULONG32)(rva & 0xffffffffll);
        info[i].__alignment2 = (ULONG32)((rva >> 32) & 0xffffffffll);


        /* the whole block was not written => spew an warning and correct the block size */
        if (written != memList->MemoryRanges[j].DataSize){
            WARN("couldn't write the entire buffer to file! {addr = 0x%016llx, size = %lld, written = %lld}\n",
                    memList->MemoryRanges[j].StartOfMemoryRange,
                    memList->MemoryRanges[j].DataSize,
                    written);


            /* make sure to set the correct block size in the table */
            memList->MemoryRanges[j].DataSize = written;
            totalSize += written;
            rva += written;

            /* something was written to the file => update the block and RVA */
            if (written > 0)
                j++;

            /* nothing was written to the file => reuse the descriptor block */
            else if (written == 0){
                memListSize -= sizeof(MINIDUMP_MEMORY_DESCRIPTOR64);
                memList->NumberOfMemoryRanges--;
                count--;
            }

            continue;
        }

        else
            totalSize += memList->MemoryRanges[j].DataSize;

        /* advance the RVA in the file by the data size */
        rva += memList->MemoryRanges[j].DataSize;
        j++;        
    }


    /* write list block to the file */
    writeat(dc, listRva, memList, (unsigned int)memListSize);

    TRACE("wrote %lld bytes to the dump file in %lld blocks {headerSize = %tu bytes}\n",
            totalSize, 
            memList->NumberOfMemoryRanges,
            memListSize);

    /* clean up */
    HeapFree(GetProcessHeap(), 0, memList);


    /* update all the RVAs for other streams that referenced memory block locations. */
    updateMemBlockRVAs(dc);


    /* NOTE: this is not 64-bit safe.  Fortunately it doesn't so much matter at 
             this point because the full memory blocks stream is always the last
             in the minidump file. */
    dc->rva = (RVA)rva;
    *streamSize = memListSize;

    TRACE("    wrote %llu bytes to the stream {rva = %u, realRVA = %llu}\n", *streamSize, dc->rva, rva);
}

/* dumpMemoryInfoList(): dump the memory info list stream to the dump file.  This will be
     done with the MiniDumpWithFullMemoryInfo dump type flag.  It writes out information 
     about all memory pages used by the application.  The info block that is written to
     file contains all the same information as the MEMORY_BASIC_INFORMATION structure
     contains, just with some padding dwords.  The size of the entire stream is returned
     through the <streamSize> parameter.  This includes the header and all memory block
     descriptors that follow. */
static void dumpMemoryInfoList(struct dump_context* dc, const MINIDUMP_MEMORY_INFO_LIST *infoList, ULONG64 *streamSize){
    MINIDUMP_MEMORY_INFO *table = getMemoryInfoTable(infoList, 0);


    TRACE("writing out memory info list {numBlocks = %lld}\n", infoList->NumberOfEntries);


    /* write the header and the memory info table to the file */
    append(dc, infoList, sizeof(MINIDUMP_MEMORY_INFO_LIST));
    append(dc, table, (unsigned int)(sizeof(MINIDUMP_MEMORY_INFO) * infoList->NumberOfEntries));

    *streamSize = sizeof(MINIDUMP_MEMORY_INFO_LIST) + sizeof(MINIDUMP_MEMORY_INFO) * infoList->NumberOfEntries;

    TRACE("    wrote %llu bytes to the stream\n", *streamSize);
}



/******************************************************************
 *		MiniDumpWriteDump (DEBUGHLP.@)
 *
 */
BOOL WINAPI MiniDumpWriteDump(HANDLE hProcess, DWORD pid, HANDLE hFile,
                              MINIDUMP_TYPE DumpType,
                              PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                              PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                              PMINIDUMP_CALLBACK_INFORMATION CallbackParam)
{
    MINIDUMP_HEADER         mdHead;
    MINIDUMP_DIRECTORY *    mdDir;
    DWORD                   i;
    DWORD                   nStreams;
    DWORD                   idx_stream;
    ULONG64                 streamSize;
    struct dump_context     dc;



    TRACE("creating a minidump (hProcess = %p, pid = 0x%08x, hFile = %p, DumpType = 0x%08x, exceptParam = %p, userStream = %p, callbackParam = %p)\n",
            hProcess, pid, hFile, DumpType, ExceptionParam, UserStreamParam, CallbackParam);

    if (g_minidumpTypeFlags)
        TRACE("    adding the dump type flags 0x%08x\n", g_minidumpTypeFlags);

    DumpType = (MINIDUMP_TYPE)(((DWORD)DumpType) | g_minidumpTypeFlags);


    dc.hProcess = hProcess;
    dc.hFile = hFile;
    dc.pid = pid;
    dc.modules = NULL;
    dc.num_modules = 0;
    dc.cb = CallbackParam;
    dc.type = DumpType;
    dc.mem = NULL;
    dc.num_mem = 0;
    dc.rva = 0;


    /* 0) gather required system info */
    if (!fetch_processes_info(&dc)){
        ERR("could not retrieve my process info! {pid = %u}\n", pid);

        return FALSE;
    }

    fetch_modules_info(&dc);



    /* 1) init */
    /* the 5 standard streams are ThreadListStream, ModuleListStream, MemoryListStream, 
       SystemInfoStream, and MiscInfoStream.  We add one custom stream that we call
       NonNativeModuleInfoStream.  If specified, the exception information is dumped
       to ExceptionStream.  A series of user streams can be dumped next, followed by
       an optional full memory dump at the end. */
    nStreams =  5 + 
                1 + 
                (ExceptionParam ? 1 : 0) +
                (UserStreamParam ? UserStreamParam->UserStreamCount : 0) +
                ((DumpType & MiniDumpWithFullMemory) ? 1 : 0) +
                ((DumpType & MiniDumpWithFullMemoryInfo) ? 1 : 0);

    TRACE("calculated the need for %d streams\n", nStreams);

    /* pad the directory size to a multiple of 4 for alignment purposes */
    nStreams = (nStreams + 3) & ~3;


    /* spew messages for dump types we don't handle */
    if (DumpType & MiniDumpWithDataSegs)
        FIXME("NIY MiniDumpWithDataSegs\n");
    if (DumpType & MiniDumpWithHandleData)
        FIXME("NIY MiniDumpWithHandleData\n");
    if (DumpType & MiniDumpFilterMemory)
        FIXME("NIY MiniDumpFilterMemory\n");
    if (DumpType & MiniDumpScanMemory)
        FIXME("NIY MiniDumpScanMemory\n");



    /* 2) write header */
    mdHead.Signature = MINIDUMP_SIGNATURE;
    mdHead.Version = MINIDUMP_VERSION;  /* NOTE: native puts in an 'implementation specific' value in the high order word of this member */
    mdHead.NumberOfStreams = nStreams;
    mdHead.CheckSum = 0;                /* native sets a 0 checksum in its files */
    mdHead.StreamDirectoryRva = sizeof(mdHead);
    mdHead.u.TimeDateStamp = (ULONG32)time(NULL);
    mdHead.Flags = DumpType;
    append(&dc, &mdHead, sizeof(mdHead));



    /* 3) create the stream directory and set the starting stream location */
    mdDir = (MINIDUMP_DIRECTORY *)HeapAlloc(GetProcessHeap(), 0, sizeof(MINIDUMP_DIRECTORY) * nStreams);
    dc.rva += nStreams * sizeof(MINIDUMP_DIRECTORY);
    idx_stream = 0;



    /* 4) write out all the stream data */

    /* write out the system info stream */
    TRACE("dumping system info stream\n");
    mdDir[idx_stream].StreamType = SystemInfoStream;
    mdDir[idx_stream].Location.Rva = dc.rva;
    dump_system_info(&dc, &streamSize);
    mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
    idx_stream++;

    TRACE("    wrote system info at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %u}\n", 
            mdDir[idx_stream - 1].Location.Rva, 
            mdDir[idx_stream - 1].StreamType, 
            mdDir[idx_stream - 1].Location.DataSize, 
            idx_stream - 1);


    /* write out the misc info stream */
    TRACE("dumping misc info stream\n");
    mdDir[idx_stream].StreamType = MiscInfoStream;
    mdDir[idx_stream].Location.Rva = dc.rva;
    dump_misc_info(&dc, &streamSize);
    mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
    idx_stream++;

    TRACE("    wrote misc info at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %u}\n", 
            mdDir[idx_stream - 1].Location.Rva, 
            mdDir[idx_stream - 1].StreamType, 
            mdDir[idx_stream - 1].Location.DataSize, 
            idx_stream - 1);


    /* write out the exception parameters if specified */
    if (ExceptionParam)
    {
        TRACE("dumping exception info stream\n");
        mdDir[idx_stream].StreamType = ExceptionStream;
        mdDir[idx_stream].Location.Rva = dc.rva;
        dump_exception_info(&dc, ExceptionParam, &streamSize);
        mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
        idx_stream++;

        TRACE("    wrote exception info at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %u}\n", 
                mdDir[idx_stream - 1].Location.Rva, 
                mdDir[idx_stream - 1].StreamType, 
                mdDir[idx_stream - 1].Location.DataSize, 
                idx_stream - 1);
    }


    /* write out the thread list stream */
    TRACE("dumping thread list stream\n");
    mdDir[idx_stream].StreamType = ThreadListStream;
    mdDir[idx_stream].Location.Rva = dc.rva;
    dump_threads(&dc, ExceptionParam, &streamSize);
    mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
    idx_stream++;

    TRACE("    wrote thread list at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %u}\n", 
            mdDir[idx_stream - 1].Location.Rva, 
            mdDir[idx_stream - 1].StreamType, 
            mdDir[idx_stream - 1].Location.DataSize, 
            idx_stream - 1);
                 

    /* write out the module list stream */
    TRACE("dumping module list stream\n");                                                                  
    mdDir[idx_stream].StreamType = ModuleListStream;                                                                    
    mdDir[idx_stream].Location.Rva = dc.rva;                                                                            
    dump_modules(&dc, FALSE, &streamSize);                                                                  
    mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
    idx_stream++;

    TRACE("    wrote module list at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %u}\n", 
            mdDir[idx_stream - 1].Location.Rva, 
            mdDir[idx_stream - 1].StreamType, 
            mdDir[idx_stream - 1].Location.DataSize, 
            idx_stream - 1);


    /* minidumps with full memory blocks aren't allowed to have memory lists => skip this */
    if ((DumpType & MiniDumpWithFullMemory) == 0){

        /* write out the memory list stream */
        TRACE("dumping memory list stream\n");
        mdDir[idx_stream].StreamType = MemoryListStream;
        mdDir[idx_stream].Location.Rva = dc.rva;
        dump_memory_info(&dc, &streamSize);
        mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
        idx_stream++;

        TRACE("    wrote memory list at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %u}\n", 
                mdDir[idx_stream - 1].Location.Rva, 
                mdDir[idx_stream - 1].StreamType, 
                mdDir[idx_stream - 1].Location.DataSize, 
                idx_stream - 1);
    }


    /* write user defined streams (if any) */
    if (UserStreamParam)
    {
        TRACE("dumping %u user info streams\n", UserStreamParam->UserStreamCount);
        for (i = 0; i < UserStreamParam->UserStreamCount; i++)
        {
            mdDir[idx_stream].StreamType = UserStreamParam->UserStreamArray[i].Type;
            mdDir[idx_stream].Location.DataSize = UserStreamParam->UserStreamArray[i].BufferSize;
            mdDir[idx_stream].Location.Rva = dc.rva;
            append(&dc, UserStreamParam->UserStreamArray[i].Buffer, 
                   UserStreamParam->UserStreamArray[i].BufferSize);
            idx_stream++;

            TRACE("    wrote user stream %u at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %d}\n", 
                    i, 
                    mdDir[idx_stream - 1].Location.Rva, 
                    mdDir[idx_stream - 1].StreamType,
                    mdDir[idx_stream - 1].Location.DataSize,
                    idx_stream - 1);
        }
    }

    /********************* INSERT NEW STREAMS HERE ***************************/

    /* always write out custom stream last since windbg seems to bork on it sometimes */
    TRACE("dumping native module list stream\n");
    mdDir[idx_stream].StreamType = NativeModuleInfoStream;
    mdDir[idx_stream].Location.Rva = dc.rva;
    dump_modules(&dc, TRUE, &streamSize);
    mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
    idx_stream++;

    TRACE("    wrote native module list at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %d}\n", 
            mdDir[idx_stream - 1].Location.Rva, 
            mdDir[idx_stream - 1].StreamType, 
            mdDir[idx_stream - 1].Location.DataSize, 
            idx_stream - 1);

    
    /* we were asked for either full memory or full memory info => gather memory information
         and write out the requested streams. */
    if (DumpType & (MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo)){
        MINIDUMP_MEMORY_INFO_LIST *list = gatherMemoryInfoList(&dc, hProcess);


        /* asked to dump full memory info blocks => write in the stream header and data */
        if (DumpType & MiniDumpWithFullMemoryInfo){
            TRACE("dumping full memory info for the process\n");

            mdDir[idx_stream].StreamType = MemoryInfoListStream;
            mdDir[idx_stream].Location.Rva = dc.rva;
            dumpMemoryInfoList(&dc, list, &streamSize);
            mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
            idx_stream++;

            TRACE("    wrote full memory info stream at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %d}\n", 
                    mdDir[idx_stream - 1].Location.Rva, 
                    mdDir[idx_stream - 1].StreamType, 
                    mdDir[idx_stream - 1].Location.DataSize, 
                    idx_stream - 1);
        }

        /* asked to dump full memory blocks => write in the stream header and data */
        if (DumpType & MiniDumpWithFullMemory){
            TRACE("dumping full memory for the process\n");

            mdDir[idx_stream].StreamType = Memory64ListStream;
            mdDir[idx_stream].Location.Rva = dc.rva;
            dumpMemoryBlocks(&dc, list, &streamSize);
            mdDir[idx_stream].Location.DataSize = (ULONG32)streamSize;
            idx_stream++;

            TRACE("    wrote full memory blocks at 0x%08x {streamType = %u, dataSize = %u bytes, idx_stream = %d}\n", 
                    mdDir[idx_stream - 1].Location.Rva, 
                    mdDir[idx_stream - 1].StreamType, 
                    mdDir[idx_stream - 1].Location.DataSize, 
                    idx_stream - 1);
        }

        /* clean up the memory info list */
        destroyMemoryInfoList(list);
    }



    /* 5) sort the directory entries and write them out to file */
    TRACE("sorting the directory entries\n");

    /* reorganize the stream directory in a way that windbg will put up with */
    reorganizeDirectory(mdDir, nStreams, idx_stream);

    /* write the stream directory to the file */
    writeat(&dc, mdHead.StreamDirectoryRva, mdDir, sizeof(MINIDUMP_DIRECTORY) * nStreams);



    /* 6) clean up */
    HeapFree(GetProcessHeap(), 0, dc.pcs_buffer);
    HeapFree(GetProcessHeap(), 0, dc.mem);
    HeapFree(GetProcessHeap(), 0, dc.modules);
    HeapFree(GetProcessHeap(), 0, mdDir);

    TRACE("minidump performed successfully\n");
    return TRUE;
}

/******************************************************************
 *		MiniDumpReadDumpStream (DEBUGHLP.@)
 *
 *
 */
BOOL WINAPI MiniDumpReadDumpStream(PVOID base, ULONG str_idx,
                                   PMINIDUMP_DIRECTORY* pdir,
                                   PVOID* stream, ULONG* size)
{
    MINIDUMP_HEADER*    mdHead = (MINIDUMP_HEADER*)base;

    if (mdHead->Signature == MINIDUMP_SIGNATURE)
    {
        MINIDUMP_DIRECTORY* dir;
        ULONG32             i;

        dir = (MINIDUMP_DIRECTORY*)((char*)base + mdHead->StreamDirectoryRva);
        for (i = 0; i < mdHead->NumberOfStreams; i++, dir++)
        {
            if (dir->StreamType == str_idx)
            {
                *pdir = dir;
                *stream = (char*)base + dir->Location.Rva;
                *size = dir->Location.DataSize;
                return TRUE;
            }
        }
    }
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}
