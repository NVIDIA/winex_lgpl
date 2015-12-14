#ifndef __WINE_FCI_H
#define __WINE_FCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pshpack4.h>

#ifndef INCLUDED_TYPES_FCI_FDI
#define INCLUDED_TYPES_FCI_FDI 1

typedef unsigned long CHECKSUM;

typedef unsigned long UOFF;
typedef unsigned long COFF;

typedef struct {
    int   erfOper;
    int   erfType;
    BOOL  fError;
} ERF, *PERF;

#define CB_MAX_CHUNK         32768U
#define CB_MAX_DISK          0x7fffffffL
#define CB_MAX_FILENAME      256
#define CB_MAX_CABINET_NAME  256
#define CB_MAX_CAB_PATH      256
#define CB_MAX_DISK_NAME     256

typedef unsigned short TCOMP;

#define tcompMASK_TYPE          0x000F
#define tcompTYPE_NONE          0x0000
#define tcompTYPE_MSZIP         0x0001
#define tcompTYPE_QUANTUM       0x0002
#define tcompTYPE_LZX           0x0003
#define tcompBAD                0x000F

#define tcompMASK_LZX_WINDOW    0x1F00
#define tcompLZX_WINDOW_LO      0x0F00
#define tcompLZX_WINDOW_HI      0x1500
#define tcompSHIFT_LZX_WINDOW        8

#define tcompMASK_QUANTUM_LEVEL 0x00F0
#define tcompQUANTUM_LEVEL_LO   0x0010
#define tcompQUANTUM_LEVEL_HI   0x0070
#define tcompSHIFT_QUANTUM_LEVEL     4

#define tcompMASK_QUANTUM_MEM   0x1F00
#define tcompQUANTUM_MEM_LO     0x0A00
#define tcompQUANTUM_MEM_HI     0x1500
#define tcompSHIFT_QUANTUM_MEM       8

#define tcompMASK_RESERVED      0xE000


#define CompressionTypeFromTCOMP(tc) \
    ((tc) & tcompMASK_TYPE)

#define CompressionLevelFromTCOMP(tc) \
    (((tc) & tcompMASK_QUANTUM_LEVEL) >> tcompSHIFT_QUANTUM_LEVEL)

#define CompressionMemoryFromTCOMP(tc) \
    (((tc) & tcompMASK_QUANTUM_MEM) >> tcompSHIFT_QUANTUM_MEM)

#define TCOMPfromTypeLevelMemory(t, l, m) \
    (((m) << tcompSHIFT_QUANTUM_MEM  ) | \
     ((l) << tcompSHIFT_QUANTUM_LEVEL) | \
     ( t                             ))

#define LZXCompressionWindowFromTCOMP(tc) \
    (((tc) & tcompMASK_LZX_WINDOW) >> tcompSHIFT_LZX_WINDOW)

#define TCOMPfromLZXWindow(w) \
    (((w) << tcompSHIFT_LZX_WINDOW) | \
     ( tcompTYPE_LZX              ))

#endif


typedef enum {
    FCIERR_NONE,
    FCIERR_OPEN_SRC,
    FCIERR_READ_SRC,
    FCIERR_ALLOC_FAIL,
    FCIERR_TEMP_FILE,
    FCIERR_BAD_COMPR_TYPE,
    FCIERR_CAB_FILE,
    FCIERR_USER_ABORT,
    FCIERR_MCI_FAIL,
} FCIERROR;

#ifndef _A_NAME_IS_UTF
#define _A_NAME_IS_UTF  0x80
#endif

#ifndef _A_EXEC
#define _A_EXEC         0x40
#endif

typedef void *HFCI;

typedef struct {
    ULONG cb;
    ULONG cbFolderThresh;

    UINT  cbReserveCFHeader;
    UINT  cbReserveCFFolder;
    UINT  cbReserveCFData;
    int   iCab;
    int   iDisk;
#ifndef REMOVE_CHICAGO_M6_HACK
    int   fFailOnIncompressible;
#endif

    USHORT setID;

    char szDisk[CB_MAX_DISK_NAME];
    char szCab[CB_MAX_CABINET_NAME];
    char szCabPath[CB_MAX_CAB_PATH];
} CCAB, *PCCAB;


typedef void * (__cdecl *PFNFCIALLOC)(ULONG cb);
#define FNFCIALLOC(fn) void * __cdecl fn(ULONG cb)

typedef void (__cdecl *PFNFCIFREE)(void *memory);
#define FNFCIFREE(fn) void __cdecl fn(void *memory)

typedef INT_PTR (__cdecl *PFNFCIOPEN) (char *pszFile, int oflag, int pmode, int *err, void *pv);
#define FNFCIOPEN(fn) INT_PTR __cdecl fn(char *pszFile, int oflag, int pmode, int *err, void *pv)

typedef UINT (__cdecl *PFNFCIREAD) (INT_PTR hf, void *memory, UINT cb, int *err, void *pv);
#define FNFCIREAD(fn) UINT __cdecl fn(INT_PTR hf, void *memory, UINT cb, int *err, void *pv)

typedef UINT (__cdecl *PFNFCIWRITE)(INT_PTR hf, void *memory, UINT cb, int *err, void *pv);
#define FNFCIWRITE(fn) UINT __cdecl fn(INT_PTR hf, void *memory, UINT cb, int *err, void *pv)

typedef int  (__cdecl *PFNFCICLOSE)(INT_PTR hf, int *err, void *pv);
#define FNFCICLOSE(fn) int __cdecl fn(INT_PTR hf, int *err, void *pv)

typedef long (__cdecl *PFNFCISEEK) (INT_PTR hf, long dist, int seektype, int *err, void *pv);
#define FNFCISEEK(fn) long __cdecl fn(INT_PTR hf, long dist, int seektype, int *err, void *pv)

typedef int  (__cdecl *PFNFCIDELETE) (char *pszFile, int *err, void *pv);
#define FNFCIDELETE(fn) int __cdecl fn(char *pszFile, int *err, void *pv)

typedef BOOL (__cdecl *PFNFCIGETNEXTCABINET)(PCCAB pccab, ULONG  cbPrevCab, void *pv);
#define FNFCIGETNEXTCABINET(fn) BOOL __cdecl fn(PCCAB pccab, \
						ULONG  cbPrevCab, \
						void *pv)

typedef int (__cdecl *PFNFCIFILEPLACED)(PCCAB pccab,
					char *pszFile,
					long  cbFile,
					BOOL  fContinuation,
					void *pv);
#define FNFCIFILEPLACED(fn) int __cdecl fn(PCCAB pccab, \
					   char *pszFile, \
					   long  cbFile, \
					   BOOL  fContinuation, \
					   void *pv)

typedef INT_PTR (__cdecl *PFNFCIGETOPENINFO)(char *pszName,
					     USHORT *pdate,
					     USHORT *ptime,
					     USHORT *pattribs,
					     int *err,
					     void *pv);
#define FNFCIGETOPENINFO(fn) INT_PTR __cdecl fn(char *pszName, \
						USHORT *pdate, \
						USHORT *ptime, \
						USHORT *pattribs, \
						int *err, \
						void *pv)

#define statusFile     0
#define statusFolder   1
#define statusCabinet  2

typedef long (__cdecl *PFNFCISTATUS)(UINT typeStatus,
				     ULONG cb1,
				     ULONG cb2,
				     void *pv);
#define FNFCISTATUS(fn) long __cdecl fn(UINT typeStatus, \
					ULONG  cb1, \
					ULONG  cb2, \
					void *pv)

typedef BOOL (__cdecl *PFNFCIGETTEMPFILE)(char *pszTempName,
					  int   cbTempName,
					  void *pv);
#define FNFCIGETTEMPFILE(fn) BOOL __cdecl fn(char *pszTempName, \
                                             int   cbTempName, \
                                             void *pv)


HFCI __cdecl FCICreate(PERF, PFNFCIFILEPLACED, PFNFCIALLOC, PFNFCIFREE,
		       PFNFCIOPEN, PFNFCIREAD, PFNFCIWRITE, PFNFCICLOSE,
		       PFNFCISEEK, PFNFCIDELETE, PFNFCIGETTEMPFILE, PCCAB,
		       void *);
BOOL __cdecl FCIAddFile(HFCI, char *, char *, BOOL, PFNFCIGETNEXTCABINET,
			PFNFCISTATUS, PFNFCIGETOPENINFO, TCOMP);
BOOL __cdecl FCIFlushCabinet(HFCI, BOOL, PFNFCIGETNEXTCABINET, PFNFCISTATUS);
BOOL __cdecl FCIFlushFolder(HFCI, PFNFCIGETNEXTCABINET, PFNFCISTATUS);
BOOL __cdecl FCIDestroy(HFCI hfci);

#include <poppack.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
