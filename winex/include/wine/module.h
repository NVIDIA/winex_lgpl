/*
 * Module definitions
 *
 * Copyright 1995 Alexandre Julliard
 */

#ifndef __WINE_MODULE_H
#define __WINE_MODULE_H

#include "windef.h"
#include "winbase.h"
#include "wine/windef16.h"
#include "wine/winbase16.h"

  /* In-memory module structure. See 'Windows Internals' p. 219 */
typedef struct _NE_MODULE
{
    WORD    magic;            /* 00 'NE' signature */
    WORD    count;            /* 02 Usage count */
    WORD    entry_table;      /* 04 Near ptr to entry table */
    HMODULE16  next;          /* 06 Selector to next module */
    WORD    dgroup_entry;     /* 08 Near ptr to segment entry for DGROUP */
    WORD    fileinfo;         /* 0a Near ptr to file info (OFSTRUCT) */
    WORD    flags;            /* 0c Module flags */
    WORD    dgroup;           /* 0e Logical segment for DGROUP */
    WORD    heap_size;        /* 10 Initial heap size */
    WORD    stack_size;       /* 12 Initial stack size */
    WORD    ip;               /* 14 Initial ip */
    WORD    cs;               /* 16 Initial cs (logical segment) */
    WORD    sp;               /* 18 Initial stack pointer */
    WORD    ss;               /* 1a Initial ss (logical segment) */
    WORD    seg_count;        /* 1c Number of segments in segment table */
    WORD    modref_count;     /* 1e Number of module references */
    WORD    nrname_size;      /* 20 Size of non-resident names table */
    WORD    seg_table;        /* 22 Near ptr to segment table */
    WORD    res_table;        /* 24 Near ptr to resource table */
    WORD    name_table;       /* 26 Near ptr to resident names table */
    WORD    modref_table;     /* 28 Near ptr to module reference table */
    WORD    import_table;     /* 2a Near ptr to imported names table */
    DWORD   nrname_fpos;      /* 2c File offset of non-resident names table */
    WORD    moveable_entries; /* 30 Number of moveable entries in entry table*/
    WORD    alignment;        /* 32 Alignment shift count */
    WORD    truetype;         /* 34 Set to 2 if TrueType font */
    BYTE    os_flags;         /* 36 Operating system flags */
    BYTE    misc_flags;       /* 37 Misc. flags */
    HANDLE16   dlls_to_init;  /* 38 List of DLLs to initialize */
    HANDLE16   nrname_handle; /* 3a Handle to non-resident name table */
    WORD    min_swap_area;    /* 3c Min. swap area size */
    WORD    expected_version; /* 3e Expected Windows version */
    /* From here, these are extra fields not present in normal Windows */
    HMODULE  module32;      /* 40 PE module handle for Win32 modules */
    HMODULE16  self;          /* 44 Handle for this module */
    WORD    self_loading_sel; /* 46 Selector used for self-loading apps. */
    LPVOID  hRsrcMap;         /* HRSRC 16->32 map (for 32-bit modules) */
} NE_MODULE;


typedef struct {
    BYTE type;
    BYTE flags;
    BYTE segnum;
    WORD offs WINE_PACKED;
} ET_ENTRY;

typedef struct {
    WORD first; /* ordinal */
    WORD last; /* ordinal */
    WORD next; /* bundle */
} ET_BUNDLE;


  /* In-memory segment table */
typedef struct
{
    WORD      filepos;   /* Position in file, in sectors */
    WORD      size;      /* Segment size on disk */
    WORD      flags;     /* Segment flags */
    WORD      minsize;   /* Min. size of segment in memory */
    HANDLE16  hSeg;      /* Selector or handle (selector - 1) */
                         /* of segment in memory */
} SEGTABLEENTRY;


  /* Self-loading modules contain this structure in their first segment */

#include "pshpack1.h"

typedef struct
{
    WORD      version;       /* Must be "A0" (0x3041) */
    WORD      reserved;
    FARPROC16 BootApp;       /* startup procedure */
    FARPROC16 LoadAppSeg;    /* procedure to load a segment */
    FARPROC16 reserved2;
    FARPROC16 MyAlloc;       /* memory allocation procedure,
                              * wine must write this field */
    FARPROC16 EntryAddrProc;
    FARPROC16 ExitProc;      /* exit procedure */
    WORD      reserved3[4];
    FARPROC16 SetOwner;      /* Set Owner procedure, exported by wine */
} SELFLOADHEADER;

typedef struct
{
    LPSTR lpEnvAddress;
    LPSTR lpCmdLine;
    UINT16 *lpCmdShow;
    DWORD dwReserved;
} LOADPARAMS;

#include "poppack.h"

/* internal representation of 32bit modules. per process. */
typedef struct _wine_modref
{
	struct _wine_modref *next;
	struct _wine_modref *prev;
	HMODULE              module;
	HMODULE16            hDummyMod; /* Win16 dummy module */
	void                *dlhandle;  /* handle returned by dlopen() */
	int                  tlsindex;  /* TLS index or -1 if none */

	FARPROC            (*find_export)( struct _wine_modref *wm, LPCSTR func, BOOL snoop );

	int			nDeps;
	struct _wine_modref	**deps;

	int			flags;
	int			refCount;

	char			*filename;
	char			*modname;
	char			*short_filename;
	char			*short_modname;

    char data[1];  /* space for storing filename and short_filename */
} WINE_MODREF;

#define WINE_MODREF_INTERNAL              0x00000001
#define WINE_MODREF_NO_DLL_CALLS          0x00000002
#define WINE_MODREF_PROCESS_ATTACHED      0x00000004
#define WINE_MODREF_DONT_RESOLVE_REFS     0x00000020
#define WINE_MODREF_MARKER                0x80000000

extern WINE_MODREF *MODULE_modref_list;

/* Resource types */

#define NE_SEG_TABLE(pModule) \
    ((SEGTABLEENTRY *)((char *)(pModule) + (pModule)->seg_table))

#define NE_MODULE_TABLE(pModule) \
    ((WORD *)((char *)(pModule) + (pModule)->modref_table))

#define NE_MODULE_NAME(pModule) \
    (((OFSTRUCT *)((char*)(pModule) + (pModule)->fileinfo))->szPathName)

#define PE_HEADER(module) \
    ((IMAGE_NT_HEADERS*)((LPBYTE)(module) + \
                         (((IMAGE_DOS_HEADER*)(module))->e_lfanew)))

#define PE_SECTIONS(module) \
    ((IMAGE_SECTION_HEADER*)((LPBYTE)&PE_HEADER(module)->OptionalHeader + \
                           PE_HEADER(module)->FileHeader.SizeOfOptionalHeader))

enum loadorder_type
{
    LOADORDER_INVALID = 0, /* Must be 0 */
    LOADORDER_DLL,         /* Native DLLs */
    LOADORDER_SO,          /* Native .so libraries */
    LOADORDER_BI,          /* Built-in modules */
    LOADORDER_NTYPES
};

/* module.c */
extern WINE_MODREF *MODULE_AllocModRef( HMODULE hModule, LPCSTR filename );
extern FARPROC MODULE_GetProcAddress( HMODULE hModule, LPCSTR function, BOOL snoop );
extern BOOL MODULE_DllProcessAttach( WINE_MODREF *wm, LPVOID lpReserved );
extern void MODULE_DllProcessDetach( BOOL bForceDetach, LPVOID lpReserved );
extern void MODULE_DllThreadAttach( LPVOID lpReserved );
extern void MODULE_DllThreadDetach( LPVOID lpReserved );
extern WINE_MODREF *MODULE_LoadLibraryExA( LPCSTR libname, HFILE hfile, DWORD flags );
extern BOOL MODULE_FreeLibrary( WINE_MODREF *wm );
extern WINE_MODREF *MODULE_FindModule( LPCSTR path );
extern HMODULE16 MODULE_CreateDummyModule( LPCSTR filename, HMODULE module32 );
extern FARPROC16 WINAPI WIN32_GetProcAddress16( HMODULE hmodule, LPCSTR name );
extern SEGPTR WINAPI HasGPHandler16( SEGPTR address );
extern void MODULE_WalkModref( DWORD id );

/* loader/ne/module.c */
extern NE_MODULE *NE_GetPtr( HMODULE16 hModule );
extern void NE_DumpModule( HMODULE16 hModule );
extern void NE_WalkModules(void);
extern void NE_RegisterModule( NE_MODULE *pModule );
extern WORD NE_GetOrdinal( HMODULE16 hModule, const char *name );
extern FARPROC16 WINAPI NE_GetEntryPoint( HMODULE16 hModule, WORD ordinal );
extern FARPROC16 NE_GetEntryPointEx( HMODULE16 hModule, WORD ordinal, BOOL16 snoop );
extern BOOL16 NE_SetEntryPoint( HMODULE16 hModule, WORD ordinal, WORD offset );
extern HANDLE NE_OpenFile( NE_MODULE *pModule );
extern DWORD NE_StartTask(void);

/* loader/ne/resource.c */
extern HGLOBAL16 WINAPI NE_DefResourceHandler(HGLOBAL16,HMODULE16,HRSRC16);
extern BOOL NE_InitResourceHandler( HMODULE16 hModule );
extern HRSRC16 NE_FindResource( NE_MODULE *pModule, LPCSTR name, LPCSTR type );
extern DWORD NE_SizeofResource( NE_MODULE *pModule, HRSRC16 hRsrc );
extern HGLOBAL16 NE_LoadResource( NE_MODULE *pModule, HRSRC16 hRsrc );
extern BOOL16 NE_FreeResource( NE_MODULE *pModule, HGLOBAL16 handle );
extern NE_TYPEINFO *NE_FindTypeSection( LPBYTE pResTab, NE_TYPEINFO *pTypeInfo, LPCSTR typeId );
extern NE_NAMEINFO *NE_FindResourceFromType( LPBYTE pResTab, NE_TYPEINFO *pTypeInfo, LPCSTR resId );

/* loader/ne/segment.c */
extern BOOL NE_LoadSegment( NE_MODULE *pModule, WORD segnum );
extern BOOL NE_LoadAllSegments( NE_MODULE *pModule );
extern BOOL NE_CreateSegment( NE_MODULE *pModule, int segnum );
extern BOOL NE_CreateAllSegments( NE_MODULE *pModule );
extern HINSTANCE16 NE_GetInstance( NE_MODULE *pModule );
extern void NE_InitializeDLLs( HMODULE16 hModule );
extern void NE_DllProcessAttach( HMODULE16 hModule );

/* loader/ne/convert.c */
HGLOBAL16 NE_LoadPEResource( NE_MODULE *pModule, WORD type, LPVOID bits, DWORD size );

/* loader/pe_resource.c */
extern HRSRC PE_FindResourceW(HMODULE,LPCWSTR,LPCWSTR);
extern HRSRC PE_FindResourceExW(HMODULE,LPCWSTR,LPCWSTR,WORD);
extern DWORD PE_SizeofResource(HRSRC);
extern HGLOBAL PE_LoadResource(HMODULE,HRSRC);

/* loader/pe_image.c */
extern WINE_MODREF *PE_LoadLibraryExA(LPCSTR, DWORD);
extern HMODULE PE_LoadImage( HANDLE hFile, LPCSTR filename, DWORD flags );
extern WINE_MODREF *PE_CreateModule( HMODULE hModule, LPCSTR filename,
                                     DWORD flags, HANDLE hFile, BOOL builtin );
extern void PE_InitTls(void);
extern BOOL PE_InitDLL( const WINE_MODREF* wm, DWORD type, LPVOID lpReserved );
extern DWORD PE_fixup_imports(WINE_MODREF *wm);

/* loader/loadorder.c */
extern void MODULE_InitLoadOrder(void);
extern void MODULE_GetLoadOrder( enum loadorder_type plo[], const char *path, BOOL win32 );
extern void MODULE_AddLoadOrderOption( const char *option );

/* loader/elf.c */
extern WINE_MODREF *ELF_LoadLibraryExA( LPCSTR libname, DWORD flags);

/* relay32/builtin.c */
extern WINE_MODREF *BUILTIN32_LoadLibraryExA(LPCSTR name, DWORD flags);
extern HMODULE BUILTIN32_LoadExeModule( HMODULE main );
extern void *BUILTIN32_dlopen( const char *name );
extern int BUILTIN32_dlclose( void *handle );

/* USER signal proc flags and codes */
/* See PROCESS_CallUserSignalProc for comments */
#define USIG_FLAGS_WIN32          0x0001
#define USIG_FLAGS_GUI            0x0002
#define USIG_FLAGS_FEEDBACK       0x0004
#define USIG_FLAGS_FAULT          0x0008

#define USIG_DLL_UNLOAD_WIN16     0x0001
#define USIG_DLL_UNLOAD_WIN32     0x0002
#define USIG_FAULT_DIALOG_PUSH    0x0003
#define USIG_FAULT_DIALOG_POP     0x0004
#define USIG_DLL_UNLOAD_ORPHANS   0x0005
#define USIG_THREAD_INIT          0x0010
#define USIG_THREAD_EXIT          0x0020
#define USIG_PROCESS_CREATE       0x0100
#define USIG_PROCESS_INIT         0x0200
#define USIG_PROCESS_EXIT         0x0300
#define USIG_PROCESS_DESTROY      0x0400
#define USIG_PROCESS_RUNNING      0x0500
#define USIG_PROCESS_LOADED       0x0600

/* scheduler/process.c */
extern void PROCESS_CallUserSignalProc( UINT uCode, HMODULE16 hModule );
extern BOOL PROCESS_Create( HANDLE hFile, LPCSTR filename, LPSTR cmd_line, LPCSTR env,
                            LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                            BOOL inherit, DWORD flags,
                            STARTUPINFOA *startup, PROCESS_INFORMATION *info,
                            LPCSTR lpCurrentDirectory );

#endif  /* __WINE_MODULE_H */
