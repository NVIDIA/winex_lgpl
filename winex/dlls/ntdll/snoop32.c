/*
 * 386-specific Win32 dll<->dll snooping functions
 *
 * Copyright 1998 Marcus Meissner
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "winbase.h"
#include "winnt.h"
#include "snoop.h"
#include "stackframe.h"
#include "wine/port.h"
#include "wine/debug.h"
#include "wine/exception.h"
#include "msvcrt/excpt.h"

WINE_DEFAULT_DEBUG_CHANNEL(snoop);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);

static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ||
        GetExceptionCode() == EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

char **debug_snoop_excludelist = NULL, **debug_snoop_includelist = NULL;

#ifdef __i386__

extern void WINAPI SNOOP_Entry();
extern void WINAPI SNOOP_Return();

#ifdef NEED_UNDERSCORE_PREFIX
# define PREFIX "_"
#else
# define PREFIX
#endif

#include "pshpack1.h"

typedef	struct tagSNOOP_FUN {
	/* code part */
	BYTE		lcall;		/* 0xe8 call snoopentry (relative) */
	/* NOTE: If you move snoopentry OR nrofargs fix the relative offset
	 * calculation!
	 */
	DWORD		snoopentry;	/* SNOOP_Entry relative */
	/* unreached */
	int		nrofargs;
	FARPROC	origfun;
	char		*name;
} SNOOP_FUN;

typedef struct tagSNOOP_DLL {
	HMODULE	hmod;
	SNOOP_FUN	*funs;
	DWORD		ordbase;
	DWORD		nrofordinals;
	struct tagSNOOP_DLL	*next;
	char name[1];
} SNOOP_DLL;

typedef struct tagSNOOP_RETURNENTRY {
	/* code part */
	BYTE		lcall;		/* 0xe8 call snoopret relative*/
	/* NOTE: If you move snoopret OR origreturn fix the relative offset
	 * calculation!
	 */
	DWORD		snoopret;	/* SNOOP_Ret relative */
	/* unreached */
	FARPROC	origreturn;
	SNOOP_DLL	*dll;
	DWORD		ordinal;
	DWORD		origESP;
	DWORD		*args;		/* saved args across a stdcall */
} SNOOP_RETURNENTRY;

#define NUMENTRIES 10

typedef struct tagSNOOP_RETURNENTRIES {
	SNOOP_RETURNENTRY entry[NUMENTRIES];
	struct tagSNOOP_RETURNENTRIES	*next;
} SNOOP_RETURNENTRIES;

#include "poppack.h"

#define TMPBUFSIZE 256

typedef struct {
   char TmpBuf[TMPBUFSIZE];
   SNOOP_RETURNENTRIES Entries;
} SNOOP_ThreadData_t;

static	SNOOP_DLL		*firstdll = NULL;
static  DWORD                    SnoopTLS = TLS_OUT_OF_INDEXES;

/***********************************************************************
 *          SNOOP_ShowDebugmsgSnoop
 *
 * Simple function to decide if a particular debugging message is
 * wanted.
 */
int SNOOP_ShowDebugmsgSnoop(const char *dll, int ord, const char *fname) {

  if(debug_snoop_excludelist || debug_snoop_includelist) {
    char **listitem;
    char buf[80];
    int len, len2, itemlen, show;

    if(debug_snoop_excludelist) {
      show = 1;
      listitem = debug_snoop_excludelist;
    } else {
      show = 0;
      listitem = debug_snoop_includelist;
    }
    len = strlen(dll);
    assert(len < 64);
    sprintf(buf, "%s.%d", dll, ord);
    len2 = strlen(buf);
    for(; *listitem; listitem++) {
      itemlen = strlen(*listitem);
      if((itemlen == len && !strncasecmp(*listitem, buf, len)) ||
         (itemlen == len2 && !strncasecmp(*listitem, buf, len2)) ||
         !strcasecmp(*listitem, fname)) {
        show = !show;
       break;
      }
    }
    return show;
  }
  return 1;
}

void
SNOOP_RegisterDLL(HMODULE hmod,LPCSTR name,DWORD ordbase,DWORD nrofordinals) {
	SNOOP_DLL	**dll = &(firstdll);
	char		*s;

	if (!TRACE_ON(snoop)) return;
	while (*dll) {
		if ((*dll)->hmod == hmod)
			return; /* already registered */
		dll = &((*dll)->next);
	}

        if (SnoopTLS == TLS_OUT_OF_INDEXES)
           SnoopTLS = TlsAlloc ();
        if (SnoopTLS == TLS_OUT_OF_INDEXES)
        {
           ERR ("Unable to allocate TLS entry for snoop!\n");
           return;
        }

	*dll = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SNOOP_DLL)+strlen(name));
	(*dll)->next	= NULL;
	(*dll)->hmod	= hmod;
	(*dll)->ordbase = ordbase;
	(*dll)->nrofordinals = nrofordinals;
	strcpy( (*dll)->name, name );
	if ((s=strrchr((*dll)->name,'.')))
		*s='\0';
	(*dll)->funs = VirtualAlloc(NULL,nrofordinals*sizeof(SNOOP_FUN),MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
	memset((*dll)->funs,0,nrofordinals*sizeof(SNOOP_FUN));
	if (!(*dll)->funs) {
		HeapFree(GetProcessHeap(),0,*dll);
		FIXME("out of memory\n");
		return;
	}
}

FARPROC
SNOOP_GetProcAddress(HMODULE hmod,LPCSTR name,DWORD ordinal,FARPROC origfun) {
	SNOOP_DLL			*dll = firstdll;
	SNOOP_FUN			*fun;
	int				j;
	IMAGE_SECTION_HEADER		*pe_seg = PE_SECTIONS(hmod);

	if (!TRACE_ON(snoop)) return origfun;
	if (!*(LPBYTE)origfun) /* 0x00 is an imposs. opcode, poss. dataref. */
		return origfun;
	for (j=0;j<PE_HEADER(hmod)->FileHeader.NumberOfSections;j++)
		/* 0x42 is special ELF loader tag */
		if ((pe_seg[j].VirtualAddress==0x42) ||
		    (((DWORD)origfun-hmod>=pe_seg[j].VirtualAddress)&&
		     ((DWORD)origfun-hmod <pe_seg[j].VirtualAddress+
		    		   pe_seg[j].SizeOfRawData
		   ))
		)
			break;
	/* If we looked through all sections (and didn't find one)
	 * or if the sectionname contains "data", we return the
	 * original function since it is most likely a datareference.
	 */
	if (	(j==PE_HEADER(hmod)->FileHeader.NumberOfSections)	||
		(strstr((char *)pe_seg[j].Name,"data"))			||
		!(pe_seg[j].Characteristics & IMAGE_SCN_CNT_CODE)
	)
		return origfun;

	while (dll) {
		if (hmod == dll->hmod)
			break;
		dll=dll->next;
	}
	if (!dll)	/* probably internal */
		return origfun;
	if (!SNOOP_ShowDebugmsgSnoop(dll->name,ordinal,name))
		return origfun;
	assert(ordinal < dll->nrofordinals);
	fun = dll->funs+ordinal;
	if (!fun->name)
	  {
	    fun->name = HeapAlloc(GetProcessHeap(),0,strlen(name)+1);
	    strcpy( fun->name, name );
	    fun->lcall	= 0xe8;
	    /* NOTE: origreturn struct member MUST come directly after snoopentry */
	    fun->snoopentry	= (char*)SNOOP_Entry-((char*)(&fun->nrofargs));
	    fun->origfun	= origfun;
	    fun->nrofargs	= -1;
	  }
	return (FARPROC)&(fun->lcall);
}


void SNOOP_ThreadExit ()
{
   SNOOP_ThreadData_t *pThreadData;
   SNOOP_RETURNENTRIES *entries;

   pThreadData = TlsGetValue (SnoopTLS);
   if (pThreadData == NULL)
      return;

   entries = pThreadData->Entries.next;
   while (entries)
   {
      SNOOP_RETURNENTRIES *next = entries->next;
      HeapFree (GetProcessHeap (), 0, entries);
      entries = next;
   }
   HeapFree (GetProcessHeap (), 0, pThreadData);
}


static char*
SNOOP_PrintArg(DWORD x, SNOOP_ThreadData_t *pThreadData) {
	char *buf;
	int		i,nostring;
	char * volatile ret=0;

        buf = pThreadData->TmpBuf;

	/* Strings have a non zero HIWORD. Is this trivially a number? */
	if ( !HIWORD(x) ) {
	    sprintf(buf,"%08lx",x);
	    return buf;
	}
	__TRY{
		LPBYTE	s=(LPBYTE)x;
		i=0;nostring=0;
		while (i<80) {
			if (s[i]==0) break;
			if (s[i]<0x20) {nostring=1;break;}
			if (s[i]>=0x80) {nostring=1;break;}
			i++;
		}
		if (!nostring) {
			if (i>5) {
				snprintf(buf,TMPBUFSIZE,"%08lx %s",x,debugstr_an((LPSTR)x,TMPBUFSIZE-10));
				ret=buf;
			}
		}
	}
	__EXCEPT(page_fault){}
	__ENDTRY
	if (ret)
	  return ret;
	__TRY{
		LPWSTR	s=(LPWSTR)x;
		i=0;nostring=0;
		while (i<80) {
			if (s[i]==0) break;
			if (s[i]<0x20) {nostring=1;break;}
			if (s[i]>0x100) {nostring=1;break;}
			i++;
		}
		if (!nostring) {
			if (i>5) {
                           snprintf(buf,TMPBUFSIZE, "%08lx %s", x, debugstr_wn((LPWSTR)x, TMPBUFSIZE-10));
                           ret=buf;
			}
		}
	}
	__EXCEPT(page_fault){}
	__ENDTRY
	if (ret)
	  return ret;
	sprintf(buf,"%08lx",x);
	return buf;
}

#define CALLER1REF (*(DWORD*)context->Esp)

void WINAPI SNOOP_DoEntry( CONTEXT86 *context )
{
	DWORD		ordinal=0,entry = context->Eip - 5; /* 5 is sizeof(BYTE)+sizeof(DWORD) which is call instruction on x86 */
	SNOOP_DLL	*dll = firstdll;
	SNOOP_FUN	*fun = NULL;
	SNOOP_RETURNENTRIES	*rets,*prev;
	SNOOP_RETURNENTRY	*ret;
	int		i=0, max;
        SNOOP_ThreadData_t *pThreadData;

        pThreadData = TlsGetValue (SnoopTLS);
        if (pThreadData == NULL)
        {
           pThreadData = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY,
                                    sizeof (*pThreadData));
           if (pThreadData == NULL)
           {
              ERR ("Unable to allocate thread data for snoop!\n");
              return;
           }
           TlsSetValue (SnoopTLS, pThreadData);
        }
        rets = &pThreadData->Entries;
        prev = NULL;

	while (dll) {
		if (	((char*)entry>=(char*)dll->funs)	&&
			((char*)entry<=(char*)(dll->funs+dll->nrofordinals))
		) {
			fun = (SNOOP_FUN*)entry;
			ordinal = fun-dll->funs;
			break;
		}
		dll=dll->next;
	}
	if (!dll) {
		FIXME("entrypoint 0x%08lx not found\n",entry);
		return; /* oops */
	}
	/* guess cdecl ... */
	if (fun->nrofargs<0) {
		/* Typical cdecl return frame is:
		 * 	add esp, xxxxxxxx
		 * which has (for xxxxxxxx up to 255 the opcode "83 C4 xx".
		 * (after that 81 C2 xx xx xx xx)
		 */
		LPBYTE	reteip = (LPBYTE)CALLER1REF;

		if (reteip) {
			if ((reteip[0]==0x83)&&(reteip[1]==0xc4))
				fun->nrofargs=reteip[2]/4;
		}
	}

	while (rets) {
           for (i = 0; i < NUMENTRIES; i++)
           {
              if (!rets->entry[i].origreturn)
                 break;
           }
           if (i != NUMENTRIES)
              break;
           prev = rets;
           rets = rets->next;
	}
	if (!rets) {
           rets = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY,
                             sizeof (SNOOP_RETURNENTRIES));
           i = 0;	/* entry 0 is free */
           prev->next = rets;
	}
	ret = &(rets->entry[i]);
	ret->lcall	= 0xe8;
	/* NOTE: origreturn struct member MUST come directly after snoopret */
	ret->snoopret	= ((char*)SNOOP_Return)-(char*)(&ret->origreturn);
	ret->origreturn	= (FARPROC)CALLER1REF;
	CALLER1REF	= (DWORD)&ret->lcall;
	ret->dll	= dll;
	ret->args	= NULL;
	ret->ordinal	= ordinal;
	ret->origESP	= context->Esp;

	context->Eip = (DWORD)fun->origfun;

	if( TRACE_ON(timestamp) ) DPRINTF( "%ld - \n", NtGetTickCount() );
	DPRINTF("%04lx:CALL(%u) %s.%ld: %s(",GetCurrentThreadId(), (NtCurrentTeb()->uRelayLevel)++,
	               dll->name,dll->ordbase+ordinal,fun->name);
	if (fun->nrofargs>0) {
		max = fun->nrofargs; if (max>16) max=16;
		for (i=0;i<max;i++)
                   DPRINTF("%s%s",SNOOP_PrintArg(*(DWORD*)(context->Esp + 4 + sizeof(DWORD)*i), pThreadData),(i<fun->nrofargs-1)?",":"");
		if (max!=fun->nrofargs)
			DPRINTF(" ...");
	} else if (fun->nrofargs<0) {
		DPRINTF("<unknown, check return>");
		ret->args = HeapAlloc(GetProcessHeap(),0,16*sizeof(DWORD));
		memcpy(ret->args,(LPBYTE)(context->Esp + 4),sizeof(DWORD)*16);
	}
	DPRINTF(") ret=%08lx\n",(DWORD)ret->origreturn);
}


void WINAPI SNOOP_DoReturn( CONTEXT86 *context )
{
	SNOOP_RETURNENTRY	*ret = (SNOOP_RETURNENTRY*)(context->Eip - 5);
        SNOOP_ThreadData_t *pThreadData;

        pThreadData = TlsGetValue (SnoopTLS);

	/* We haven't found out the nrofargs yet. If we called a cdecl
	 * function it is too late anyway and we can just set '0' (which
	 * will be the difference between orig and current ESP
	 * If stdcall -> everything ok.
	 */
	if (ret->dll->funs[ret->ordinal].nrofargs<0)
		ret->dll->funs[ret->ordinal].nrofargs=(context->Esp - ret->origESP-4)/4;
	context->Eip = (DWORD)ret->origreturn;
	
	if( TRACE_ON(timestamp) ) DPRINTF( "%ld - \n", NtGetTickCount() );
	if (ret->args) {
		int	i,max;

		DPRINTF("%04lx:RET (%u) %s.%ld: %s(",
		        GetCurrentThreadId(), --(NtCurrentTeb()->uRelayLevel),
		        ret->dll->name,ret->dll->ordbase+ret->ordinal,ret->dll->funs[ret->ordinal].name);
		max = ret->dll->funs[ret->ordinal].nrofargs;
		if (max>16) max=16;

		for (i=0;i<max;i++)
                   DPRINTF("%s%s",SNOOP_PrintArg(ret->args[i], pThreadData),(i<max-1)?",":"");
		DPRINTF(") retval = %08lx ret=%08lx\n",
			context->Eax,(DWORD)ret->origreturn );
		HeapFree(GetProcessHeap(),0,ret->args);
		ret->args = NULL;
	} else
		DPRINTF("%04lx:RET (%u) %s.%ld: %s() retval = %08lx ret=%08lx\n",
			GetCurrentThreadId(), --(NtCurrentTeb()->uRelayLevel),
			ret->dll->name,ret->dll->ordbase+ret->ordinal,ret->dll->funs[ret->ordinal].name,
			context->Eax, (DWORD)ret->origreturn);
	ret->origreturn = NULL; /* mark as empty */
}

/* assembly wrappers that save the context */
__ASM_GLOBAL_FUNC( SNOOP_Entry,
                   "call " __ASM_NAME("__wine_call_from_32_regs") "\n\t"
                   ".long " __ASM_NAME("SNOOP_DoEntry") ",0" );
__ASM_GLOBAL_FUNC( SNOOP_Return,
                   "call " __ASM_NAME("__wine_call_from_32_regs") "\n\t"
                   ".long " __ASM_NAME("SNOOP_DoReturn") ",0" );

#else	/* !__i386__ */
void SNOOP_RegisterDLL(HMODULE hmod,LPCSTR name,DWORD ordbase,DWORD nrofordinals) {
	if (!TRACE_ON(snoop)) return;
	FIXME("snooping works only on i386 for now.\n");
}

FARPROC SNOOP_GetProcAddress(HMODULE hmod,LPCSTR name,DWORD ordinal,FARPROC origfun) {
	return origfun;
}
#endif	/* !__i386__ */
