#ifndef _FDI_H
#define _FDI_H

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

#endif /* INCLUDED_TYPES_FCI_FDI */


typedef enum {
    FDIERROR_NONE,
    FDIERROR_CABINET_NOT_FOUND,
    FDIERROR_NOT_A_CABINET,
    FDIERROR_UNKNOWN_CABINET_VERSION,
    FDIERROR_CORRUPT_CABINET,
    FDIERROR_ALLOC_FAIL,
    FDIERROR_BAD_COMPR_TYPE,
    FDIERROR_MDI_FAIL,
    FDIERROR_TARGET_FILE,
    FDIERROR_RESERVE_MISMATCH,
    FDIERROR_WRONG_CABINET,
    FDIERROR_USER_ABORT,
} FDIERROR;


#ifndef _A_NAME_IS_UTF
#define _A_NAME_IS_UTF  0x80
#endif

#ifndef _A_EXEC
#define _A_EXEC         0x40
#endif

typedef void *HFDI;

typedef struct {
    long    cbCabinet;  
    USHORT  cFolders;  
    USHORT  cFiles;   
    USHORT  setID;    
    USHORT  iCabinet; 
    BOOL    fReserve; 
    BOOL    hasprev;  
    BOOL    hasnext;  
} FDICABINETINFO, *PFDICABINETINFO; 

typedef enum {
    fdidtNEW_CABINET,  
    fdidtNEW_FOLDER,  
    fdidtDECRYPT,    
} FDIDECRYPTTYPE;


typedef struct {
    FDIDECRYPTTYPE fdidt; 

    void *pvUser; 

    union {
        struct {                      
	    void   *pHeaderReserve;  
	    USHORT  cbHeaderReserve; 
	    USHORT  setID;           
	    int     iCabinet;        
        } cabinet;

        struct {                    
	    void   *pFolderReserve;
	    USHORT  cbFolderReserve;
	    USHORT  iFolder;
        } folder;

        struct {
	    void   *pDataReserve;
	    USHORT  cbDataReserve;
	    void   *pbData;
	    USHORT  cbData;
	    BOOL    fSplit;
	    USHORT  cbPartial;
        } decrypt;
    } DUMMYUNIONNAME;
} FDIDECRYPT, *PFDIDECRYPT;


typedef void * (__cdecl *PFNALLOC)(ULONG cb);
#define FNALLOC(fn) void * __cdecl fn(ULONG cb)

typedef void (__cdecl *PFNFREE)(void *pv);
#define FNFREE(fn) void __cdecl fn(void *pv)

typedef INT_PTR (__cdecl *PFNOPEN) (char *pszFile, int oflag, int pmode);
typedef UINT (__cdecl *PFNREAD) (INT_PTR hf, void *pv, UINT cb);
typedef UINT (__cdecl *PFNWRITE)(INT_PTR hf, void *pv, UINT cb);
typedef int  (__cdecl *PFNCLOSE)(INT_PTR hf);
typedef long (__cdecl *PFNSEEK) (INT_PTR hf, long dist, int seektype);
#define FNOPEN(fn) INT_PTR __cdecl fn(char *pszFile, int oflag, int pmode)
#define FNREAD(fn) UINT __cdecl fn(INT_PTR hf, void *pv, UINT cb)
#define FNWRITE(fn) UINT __cdecl fn(INT_PTR hf, void *pv, UINT cb)
#define FNCLOSE(fn) int __cdecl fn(INT_PTR hf)
#define FNSEEK(fn) long __cdecl fn(INT_PTR hf, long dist, int seektype)

typedef int (__cdecl *PFNFDIDECRYPT)(PFDIDECRYPT pfdid);
#define FNFDIDECRYPT(fn) int __cdecl fn(PFDIDECRYPT pfdid)

typedef struct {
    long  cb;
    char *psz1;
    char *psz2;
    char *psz3;
    void *pv;

    INT_PTR hf;

    USHORT date;
    USHORT time;
    USHORT attribs;

    USHORT setID;
    USHORT iCabinet;
    USHORT iFolder;

    FDIERROR fdie;
} FDINOTIFICATION, *PFDINOTIFICATION;

typedef enum {
    fdintCABINET_INFO,     
    fdintPARTIAL_FILE,    
    fdintCOPY_FILE,      
    fdintCLOSE_FILE_INFO,
    fdintNEXT_CABINET,   
    fdintENUMERATE,     
} FDINOTIFICATIONTYPE;

typedef INT_PTR (__cdecl *PFNFDINOTIFY)(FDINOTIFICATIONTYPE fdint,
					PFDINOTIFICATION  pfdin);
#define FNFDINOTIFY(fn) INT_PTR __cdecl fn(FDINOTIFICATIONTYPE fdint, \
					   PFDINOTIFICATION pfdin)

#include <pshpack1.h>

typedef struct {
    char ach[2]; 
    long cbFile;
} FDISPILLFILE, *PFDISPILLFILE;

#include <poppack.h>

#define cpuUNKNOWN (-1)
#define cpu80286   (0)
#define cpu80386   (1) 


HFDI __cdecl FDICreate(PFNALLOC, PFNFREE, PFNOPEN, PFNREAD, PFNWRITE,
		       PFNCLOSE, PFNSEEK, int, PERF);
BOOL __cdecl FDIIsCabinet(HFDI, INT_PTR, PFDICABINETINFO);
BOOL __cdecl FDICopy(HFDI, char *, char *, int, PFNFDINOTIFY,
		     PFNFDIDECRYPT, void *pvUser);
BOOL __cdecl FDIDestroy(HFDI);
BOOL __cdecl FDITruncateCabinet(HFDI, char *, USHORT);

#include <poppack.h>

#ifdef __cplusplus
} /* extern "C" */
#endif 

#endif  /* _FDI_H */
