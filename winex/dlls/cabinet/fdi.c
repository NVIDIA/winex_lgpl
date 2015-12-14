/*
 * File Decompression Interface
 *
 * Copyright 2000-2002 Stuart Caie
 * Copyright 2002 Patrik Stridvall
 * Copyright 2003 Greg Turner
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * This is a largely redundant reimplementation of the stuff in cabextract.c.  It
 * would be theoretically preferable to have only one, shared implementation, however
 * there are semantic differences which may discourage efforts to unify the two.  It
 * should be possible, if awkward, to go back and reimplement cabextract.c using FDI.
 * But this approach would be quite a bit less performant.  Probably a better way
 * would be to create a "library" of routines in cabextract.c which do the actual
 * decompression, and have both fdi.c and cabextract share those routines.  The rest
 * of the code is not sufficiently similar to merit a shared implementation.
 *
 * The worst thing about this API is the bug.  "The bug" is this: when you extract a
 * cabinet, it /always/ informs you (via the hasnext field of PFDICABINETINFO), that
 * there is no subsequent cabinet, even if there is one.  wine faithfully reproduces
 * this behavior.
 *
 * TODO:
 *
 * Wine does not implement the AFAIK undocumented "enumerate" callback during
 * FDICopy.  It is implemented in Windows and therefore worth investigating...
 *
 * Lots of pointers flying around here... am I leaking RAM?
 *
 * WTF is FDITruncate?
 *
 * Probably, I need to weed out some dead code-paths.
 *
 * Test unit(s).
 *
 * The fdintNEXT_CABINET callbacks are probably not working quite as they should.
 * There are several FIXME's in the source describing some of the deficiencies in
 * some detail.  Additionally, we do not do a very good job of returning the right
 * error codes to this callback.
 *
 * FDICopy and fdi_decomp are incomprehensibly large; separating these into smaller
 * functions would be nice.
 *
 *   -gmt
 */

#include "config.h"

#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "fdi.h"
#include "cabinet.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cabinet);

THOSE_ZIP_CONSTS;

struct fdi_file {
  struct fdi_file *next;               /* next file in sequence          */
  LPCSTR filename;                     /* output name of file            */
  int    fh;                           /* open file handle or NULL       */
  cab_ULONG length;                    /* uncompressed length of file    */
  cab_ULONG offset;                    /* uncompressed offset in folder  */
  cab_UWORD index;                     /* magic index number of folder   */
  cab_UWORD time, date, attribs;       /* MS-DOS time/date/attributes    */
  BOOL oppressed;                      /* never to be processed          */
};

struct fdi_folder {
  struct fdi_folder *next;
  cab_off_t offset;                    /* offset to data blocks (32 bit) */
  cab_UWORD comp_type;                 /* compression format/window size */
  cab_ULONG comp_size;                 /* compressed size of folder      */
  cab_UBYTE num_splits;                /* number of split blocks + 1     */
  cab_UWORD num_blocks;                /* total number of blocks         */
};

/*
 * this structure fills the gaps between what is available in a PFDICABINETINFO
 * vs what is needed by FDICopy.  Memory allocated for these becomes the responsibility
 * of the caller to free.  Yes, I am aware that this is totally, utterly inelegant.
 * To make things even more unnecessarily confusing, we now attach these to the
 * fdi_decomp_state.
 */
typedef struct {
   char *prevname, *previnfo;
   char *nextname, *nextinfo;
   BOOL hasnext;  /* bug free indicator */
   int folder_resv, header_resv;
   cab_UBYTE block_resv;
} MORE_ISCAB_INFO, *PMORE_ISCAB_INFO;

/*
 * ugh, well, this ended up being pretty damn silly...
 * now that I've conceded to build equivalent structures to struct cab.*,
 * I should have just used those, or, better yet, unified the two... sue me.
 * (Note to Microsoft: That's a joke.  Please /don't/ actually sue me! -gmt).
 * Nevertheless, I've come this far, it works, so I'm not gonna change it
 * for now.  This implementation has significant semantic differences anyhow.
 */

typedef struct fdi_cds_fwd {
  void *hfdi;                      /* the hfdi we are using                 */
  int filehf, cabhf;               /* file handle we are using              */
  struct fdi_folder *current;      /* current folder we're extracting from  */
  cab_ULONG offset;                /* uncompressed offset within folder     */
  cab_UBYTE *outpos;               /* (high level) start of data to use up  */
  cab_UWORD outlen;                /* (high level) amount of data to use up */
  int (*decompress)(int, int, struct fdi_cds_fwd *); /* chosen compress fn  */
  cab_UBYTE inbuf[CAB_INPUTMAX+2]; /* +2 for lzx bitbuffer overflows!       */
  cab_UBYTE outbuf[CAB_BLOCKMAX];
  union {
    struct ZIPstate zip;
    struct QTMstate qtm;
    struct LZXstate lzx;
  } methods;
  /* some temp variables for use during decompression */
  cab_UBYTE q_length_base[27], q_length_extra[27], q_extra_bits[42];
  cab_ULONG q_position_base[42];
  cab_ULONG lzx_position_base[51];
  cab_UBYTE extra_bits[51];
  USHORT  setID;                   /* Cabinet set ID */
  USHORT  iCabinet;                /* Cabinet number in set (0 based) */
  struct fdi_cds_fwd *decomp_cab;
  MORE_ISCAB_INFO mii;
  struct fdi_folder *firstfol; 
  struct fdi_file   *firstfile;
  struct fdi_cds_fwd *next;
} fdi_decomp_state;

/***********************************************************************
 *		FDICreate (CABINET.20)
 *
 * Provided with several callbacks (all of them are mandatory),
 * returns a handle which can be used to perform operations
 * on cabinet files.
 *
 * PARAMS
 *   pfnalloc [I]  A pointer to a function which allocates ram.  Uses
 *                 the same interface as malloc.
 *   pfnfree  [I]  A pointer to a function which frees ram.  Uses the
 *                 same interface as free.
 *   pfnopen  [I]  A pointer to a function which opens a file.  Uses
 *                 the same interface as _open.
 *   pfnread  [I]  A pointer to a function which reads from a file into
 *                 a caller-provided buffer.  Uses the same interface
 *                 as _read
 *   pfnwrite [I]  A pointer to a function which writes to a file from
 *                 a caller-provided buffer.  Uses the same interface
 *                 as _write.
 *   pfnclose [I]  A pointer to a function which closes a file handle.
 *                 Uses the same interface as _close.
 *   pfnseek  [I]  A pointer to a function which seeks in a file.
 *                 Uses the same interface as _lseek.
 *   cpuType  [I]  The type of CPU; ignored in wine (recommended value:
 *                 cpuUNKNOWN, aka -1).
 *   perf     [IO] A pointer to an ERF structure.  When FDICreate
 *                 returns an error condition, error information may
 *                 be found here as well as from GetLastError.
 *
 * RETURNS
 *   On success, returns an FDI handle of type HFDI.
 *   On failure, the NULL file handle is returned. Error
 *   info can be retrieved from perf.
 *
 * INCLUDES
 *   fdi.h
 * 
 */
HFDI __cdecl FDICreate(
	PFNALLOC pfnalloc,
	PFNFREE  pfnfree,
	PFNOPEN  pfnopen,
	PFNREAD  pfnread,
	PFNWRITE pfnwrite,
	PFNCLOSE pfnclose,
	PFNSEEK  pfnseek,
	int      cpuType,
	PERF     perf)
{
  HFDI rv;

  TRACE("(pfnalloc == ^%p, pfnfree == ^%p, pfnopen == ^%p, pfnread == ^%p, pfnwrite == ^%p, \
        pfnclose == ^%p, pfnseek == ^%p, cpuType == %d, perf == ^%p)\n", 
        pfnalloc, pfnfree, pfnopen, pfnread, pfnwrite, pfnclose, pfnseek,
        cpuType, perf);

  if ((!pfnalloc) || (!pfnfree)) {
    perf->erfOper = FDIERROR_NONE;
    perf->erfType = ERROR_BAD_ARGUMENTS;
    perf->fError = TRUE;

    SetLastError(ERROR_BAD_ARGUMENTS);
    return NULL;
  }

  if (!((rv = ((HFDI) (*pfnalloc)(sizeof(FDI_Int)))))) {
    perf->erfOper = FDIERROR_ALLOC_FAIL;
    perf->erfType = ERROR_NOT_ENOUGH_MEMORY;
    perf->fError = TRUE;

    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }
  
  PFDI_INT(rv)->FDI_Intmagic = FDI_INT_MAGIC;
  PFDI_INT(rv)->pfnalloc = pfnalloc;
  PFDI_INT(rv)->pfnfree = pfnfree;
  PFDI_INT(rv)->pfnopen = pfnopen;
  PFDI_INT(rv)->pfnread = pfnread;
  PFDI_INT(rv)->pfnwrite = pfnwrite;
  PFDI_INT(rv)->pfnclose = pfnclose;
  PFDI_INT(rv)->pfnseek = pfnseek;
  /* no-brainer: we ignore the cpu type; this is only used
     for the 16-bit versions in Windows anyhow... */
  PFDI_INT(rv)->perf = perf;

  return rv;
}

/*******************************************************************
 * FDI_getoffset (internal)
 *
 * returns the file pointer position of a file handle.
 */
static long FDI_getoffset(HFDI hfdi, INT_PTR hf)
{
  return PFDI_SEEK(hfdi, hf, 0L, SEEK_CUR);
}

/**********************************************************************
 * FDI_realloc (internal)
 *
 * we can't use _msize; the user might not be using malloc, so we require
 * an explicit specification of the previous size.  inefficient.
 */
static void *FDI_realloc(HFDI hfdi, void *mem, size_t prevsize, size_t newsize)
{
  void *rslt = NULL;
  char *irslt, *imem;
  size_t copysize = (prevsize < newsize) ? prevsize : newsize;
  if (prevsize == newsize) return mem;
  rslt = PFDI_ALLOC(hfdi, newsize); 
  if (rslt)
    for (irslt = (char *)rslt, imem = (char *)mem; (copysize); copysize--)
      *irslt++ = *imem++;
  PFDI_FREE(hfdi, mem);
  return rslt;
}

/**********************************************************************
 * FDI_read_string (internal)
 *
 * allocate and read an arbitrarily long string from the cabinet
 */
static char *FDI_read_string(HFDI hfdi, INT_PTR hf, long cabsize)
{
  size_t len=256,
         oldlen = 0,
         base = FDI_getoffset(hfdi, hf),
         maxlen = cabsize - base;
  BOOL ok = FALSE;
  unsigned int i;
  cab_UBYTE *buf = NULL;

  TRACE("(hfdi == ^%p, hf == %d)\n", hfdi, hf);

  do {
    if (len > maxlen) len = maxlen;
    if (!(buf = FDI_realloc(hfdi, buf, oldlen, len))) break;
    oldlen = len;
    if (!PFDI_READ(hfdi, hf, buf, len)) break;

    /* search for a null terminator in what we've just read */
    for (i=0; i < len; i++) {
      if (!buf[i]) {ok=TRUE; break;}
    }

    if (!ok) {
      if (len == maxlen) {
        ERR("cabinet is truncated\n");
        break;
      }
      len += 256;
      PFDI_SEEK(hfdi, hf, base, SEEK_SET);
    }
  } while (!ok);

  if (!ok) {
    if (buf)
      PFDI_FREE(hfdi, buf);
    else
      ERR("out of memory!\n");
    return NULL;
  }

  /* otherwise, set the stream to just after the string and return */
  PFDI_SEEK(hfdi, hf, base + ((cab_off_t) strlen((char *) buf)) + 1, SEEK_SET);

  return (char *) buf;
}

/******************************************************************
 * FDI_read_entries (internal)
 *
 * process the cabinet header in the style of FDIIsCabinet, but
 * without the sanity checks (and bug)
 */
static BOOL FDI_read_entries(
        HFDI             hfdi,
        INT_PTR          hf,
        PFDICABINETINFO  pfdici,
        PMORE_ISCAB_INFO pmii)
{
  int num_folders, num_files, header_resv, folder_resv = 0;
  LONG base_offset, cabsize;
  USHORT setid, cabidx, flags;
  cab_UBYTE buf[64], block_resv;
  char *prevname = NULL, *previnfo = NULL, *nextname = NULL, *nextinfo = NULL;

  TRACE("(hfdi == ^%p, hf == %d, pfdici == ^%p)\n", hfdi, hf, pfdici);

  /* 
   * FIXME: I just noticed that I am memorizing the initial file pointer
   * offset and restoring it before reading in the rest of the header
   * information in the cabinet.  Perhaps that's correct -- that is, perhaps
   * this API is supposed to support "streaming" cabinets which are embedded
   * in other files, or cabinets which begin at file offsets other than zero.
   * Otherwise, I should instead go to the absolute beginning of the file.
   * (Either way, the semantics of wine's FDICopy require me to leave the
   * file pointer where it is afterwards -- If Windows does not do so, we
   * ought to duplicate the native behavior in the FDIIsCabinet API, not here.
   * 
   * So, the answer lies in Windows; will native cabinet.dll recognize a
   * cabinet "file" embedded in another file?  Note that cabextract.c does
   * support this, which implies that Microsoft's might.  I haven't tried it
   * yet so I don't know.  ATM, most of wine's FDI cabinet routines (except
   * this one) would not work in this way.  To fix it, we could just make the
   * various references to absolute file positions in the code relative to an
   * initial "beginning" offset.  Because the FDICopy API doesn't take a
   * file-handle like this one, we would therein need to search through the
   * file for the beginning of the cabinet (as we also do in cabextract.c).
   * Note that this limits us to a maximum of one cabinet per. file: the first.
   *
   * So, in summary: either the code below is wrong, or the rest of fdi.c is
   * wrong... I cannot imagine that both are correct ;)  One of these flaws
   * should be fixed after determining the behavior on Windows.   We ought
   * to check both FDIIsCabinet and FDICopy for the right behavior.
   *
   * -gmt
   */

  /* get basic offset & size info */
  base_offset = FDI_getoffset(hfdi, hf);

  if (PFDI_SEEK(hfdi, hf, 0, SEEK_END) == -1) {
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NOT_A_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }

  cabsize = FDI_getoffset(hfdi, hf);

  if ((cabsize == -1) || (base_offset == -1) || 
      ( PFDI_SEEK(hfdi, hf, base_offset, SEEK_SET) == -1 )) {
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NOT_A_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }

  /* read in the CFHEADER */
  if (PFDI_READ(hfdi, hf, buf, cfhead_SIZEOF) != cfhead_SIZEOF) {
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NOT_A_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }
  
  /* check basic MSCF signature */
  if (EndGetI32(buf+cfhead_Signature) != 0x4643534d) {
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NOT_A_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }

  /* get the number of folders */
  num_folders = EndGetI16(buf+cfhead_NumFolders);
  if (num_folders == 0) {
    /* PONDERME: is this really invalid? */
    WARN("weird cabinet detect failure: no folders in cabinet\n");
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NOT_A_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }

  /* get the number of files */
  num_files = EndGetI16(buf+cfhead_NumFiles);
  if (num_files == 0) {
    /* PONDERME: is this really invalid? */
    WARN("weird cabinet detect failure: no files in cabinet\n");
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NOT_A_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }

  /* setid */
  setid = EndGetI16(buf+cfhead_SetID);

  /* cabinet (set) index */
  cabidx = EndGetI16(buf+cfhead_CabinetIndex);

  /* check the header revision */
  if ((buf[cfhead_MajorVersion] > 1) ||
      (buf[cfhead_MajorVersion] == 1 && buf[cfhead_MinorVersion] > 3))
  {
    WARN("cabinet format version > 1.3\n");
    if (pmii) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_UNKNOWN_CABINET_VERSION;
      PFDI_INT(hfdi)->perf->erfType = 0; /* ? */
      PFDI_INT(hfdi)->perf->fError = TRUE;
    }
    return FALSE;
  }

  /* pull the flags out */
  flags = EndGetI16(buf+cfhead_Flags);

  /* read the reserved-sizes part of header, if present */
  if (flags & cfheadRESERVE_PRESENT) {
    if (PFDI_READ(hfdi, hf, buf, cfheadext_SIZEOF) != cfheadext_SIZEOF) {
      ERR("bunk reserve-sizes?\n");
      if (pmii) {
        PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
        PFDI_INT(hfdi)->perf->erfType = 0; /* ? */
        PFDI_INT(hfdi)->perf->fError = TRUE;
      }
      return FALSE;
    }

    header_resv = EndGetI16(buf+cfheadext_HeaderReserved);
    if (pmii) pmii->header_resv = header_resv;
    folder_resv = buf[cfheadext_FolderReserved];
    if (pmii) pmii->folder_resv = folder_resv;
    block_resv  = buf[cfheadext_DataReserved];
    if (pmii) pmii->block_resv = block_resv;

    if (header_resv > 60000) {
      WARN("WARNING; header reserved space > 60000\n");
    }

    /* skip the reserved header */
    if ((header_resv) && (PFDI_SEEK(hfdi, hf, header_resv, SEEK_CUR) == -1)) {
      ERR("seek failure: header_resv\n");
      if (pmii) {
        PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
        PFDI_INT(hfdi)->perf->erfType = 0; /* ? */
        PFDI_INT(hfdi)->perf->fError = TRUE;
      }
      return FALSE;
    }
  }

  if (flags & cfheadPREV_CABINET) {
    prevname = FDI_read_string(hfdi, hf, cabsize);
    if (!prevname) {
      if (pmii) {
        PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
        PFDI_INT(hfdi)->perf->erfType = 0; /* ? */
        PFDI_INT(hfdi)->perf->fError = TRUE;
      }
      return FALSE;
    } else
      if (pmii)
        pmii->prevname = prevname;
      else
        PFDI_FREE(hfdi, prevname);
    previnfo = FDI_read_string(hfdi, hf, cabsize);
    if (previnfo) {
      if (pmii) 
        pmii->previnfo = previnfo;
      else
        PFDI_FREE(hfdi, previnfo);
    }
  }

  if (flags & cfheadNEXT_CABINET) {
    if (pmii)
      pmii->hasnext = TRUE;
    nextname = FDI_read_string(hfdi, hf, cabsize);
    if (!nextname) {
      if ((flags & cfheadPREV_CABINET) && pmii) {
        if (pmii->prevname) PFDI_FREE(hfdi, prevname);
        if (pmii->previnfo) PFDI_FREE(hfdi, previnfo);
      }
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0; /* ? */
      PFDI_INT(hfdi)->perf->fError = TRUE;
      return FALSE;
    } else
      if (pmii)
        pmii->nextname = nextname;
      else
        PFDI_FREE(hfdi, nextname);
    nextinfo = FDI_read_string(hfdi, hf, cabsize);
    if (nextinfo) {
      if (pmii)
        pmii->nextinfo = nextinfo;
      else
        PFDI_FREE(hfdi, nextinfo);
    }
  }

  /* we could process the whole cabinet searching for problems;
     instead lets stop here.  Now let's fill out the paperwork */
  pfdici->cbCabinet = cabsize;
  pfdici->cFolders  = num_folders;
  pfdici->cFiles    = num_files;
  pfdici->setID     = setid;
  pfdici->iCabinet  = cabidx;
  pfdici->fReserve  = (flags & cfheadRESERVE_PRESENT) ? TRUE : FALSE;
  pfdici->hasprev   = (flags & cfheadPREV_CABINET) ? TRUE : FALSE;
  pfdici->hasnext   = (flags & cfheadNEXT_CABINET) ? TRUE : FALSE;
  return TRUE;
}

/***********************************************************************
 *		FDIIsCabinet (CABINET.21)
 *
 * Informs the caller as to whether or not the provided file handle is
 * really a cabinet or not, filling out the provided PFDICABINETINFO
 * structure with information about the cabinet.  Brief explanations of
 * the elements of this structure are available as comments accompanying
 * its definition in wine's include/fdi.h.
 *
 * PARAMS
 *   hfdi   [I]  An HFDI from FDICreate
 *   hf     [I]  The file handle about which the caller inquires
 *   pfdici [IO] Pointer to a PFDICABINETINFO structure which will
 *               be filled out with information about the cabinet
 *               file indicated by hf if, indeed, it is determined
 *               to be a cabinet.
 * 
 * RETURNS
 *   TRUE  if the file is a cabinet.  The info pointed to by pfdici will
 *         be provided.
 *   FALSE if the file is not a cabinet, or if an error was encountered
 *         while processing the cabinet.  The PERF structure provided to
 *         FDICreate can be queried for more error information.
 *
 * INCLUDES
 *   fdi.c
 */
BOOL __cdecl FDIIsCabinet(
	HFDI            hfdi,
	INT_PTR         hf,
	PFDICABINETINFO pfdici)
{
  BOOL rv;

  TRACE("(hfdi == ^%p, hf == ^%d, pfdici == ^%p)\n", hfdi, hf, pfdici);

  if (!REALLY_IS_FDI(hfdi)) {
    ERR("REALLY_IS_FDI failed on ^%p\n", hfdi);
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  if (!hf) {
    ERR("(!hf)!\n");
    /* PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CABINET_NOT_FOUND;
    PFDI_INT(hfdi)->perf->erfType = ERROR_INVALID_HANDLE;
    PFDI_INT(hfdi)->perf->fError = TRUE; */
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  if (!pfdici) {
    ERR("(!pfdici)!\n");
    /* PFDI_INT(hfdi)->perf->erfOper = FDIERROR_NONE;
    PFDI_INT(hfdi)->perf->erfType = ERROR_BAD_ARGUMENTS;
    PFDI_INT(hfdi)->perf->fError = TRUE; */
    SetLastError(ERROR_BAD_ARGUMENTS);
    return FALSE;
  }
  rv = FDI_read_entries(hfdi, hf, pfdici, NULL); 

  if (rv)
    pfdici->hasnext = FALSE; /* yuck. duplicate apparent cabinet.dll bug */

  return rv;
}

/******************************************************************
 * QTMfdi_initmodel (internal)
 *
 * Initialize a model which decodes symbols from [s] to [s]+[n]-1
 */
static void QTMfdi_initmodel(struct QTMmodel *m, struct QTMmodelsym *sym, int n, int s) {
  int i;
  m->shiftsleft = 4;
  m->entries    = n;
  m->syms       = sym;
  memset(m->tabloc, 0xFF, sizeof(m->tabloc)); /* clear out look-up table */
  for (i = 0; i < n; i++) {
    m->tabloc[i+s]     = i;   /* set up a look-up entry for symbol */
    m->syms[i].sym     = i+s; /* actual symbol */
    m->syms[i].cumfreq = n-i; /* current frequency of that symbol */
  }
  m->syms[n].cumfreq = 0;
}

/******************************************************************
 * QTMfdi_init (internal)
 */
static int QTMfdi_init(int window, int level, fdi_decomp_state *decomp_state) {
  unsigned int wndsize = 1 << window;
  int msz = window * 2, i;
  cab_ULONG j;

  /* QTM supports window sizes of 2^10 (1Kb) through 2^21 (2Mb) */
  /* if a previously allocated window is big enough, keep it    */
  if (window < 10 || window > 21) return DECR_DATAFORMAT;
  if (QTM(actual_size) < wndsize) {
    if (QTM(window)) PFDI_FREE(CAB(hfdi), QTM(window));
    QTM(window) = NULL;
  }
  if (!QTM(window)) {
    if (!(QTM(window) = PFDI_ALLOC(CAB(hfdi), wndsize))) return DECR_NOMEMORY;
    QTM(actual_size) = wndsize;
  }
  QTM(window_size) = wndsize;
  QTM(window_posn) = 0;

  /* initialize static slot/extrabits tables */
  for (i = 0, j = 0; i < 27; i++) {
    CAB(q_length_extra)[i] = (i == 26) ? 0 : (i < 2 ? 0 : i - 2) >> 2;
    CAB(q_length_base)[i] = j; j += 1 << ((i == 26) ? 5 : CAB(q_length_extra)[i]);
  }
  for (i = 0, j = 0; i < 42; i++) {
    CAB(q_extra_bits)[i] = (i < 2 ? 0 : i-2) >> 1;
    CAB(q_position_base)[i] = j; j += 1 << CAB(q_extra_bits)[i];
  }

  /* initialize arithmetic coding models */

  QTMfdi_initmodel(&QTM(model7), &QTM(m7sym)[0], 7, 0);

  QTMfdi_initmodel(&QTM(model00), &QTM(m00sym)[0], 0x40, 0x00);
  QTMfdi_initmodel(&QTM(model40), &QTM(m40sym)[0], 0x40, 0x40);
  QTMfdi_initmodel(&QTM(model80), &QTM(m80sym)[0], 0x40, 0x80);
  QTMfdi_initmodel(&QTM(modelC0), &QTM(mC0sym)[0], 0x40, 0xC0);

  /* model 4 depends on table size, ranges from 20 to 24  */
  QTMfdi_initmodel(&QTM(model4), &QTM(m4sym)[0], (msz < 24) ? msz : 24, 0);
  /* model 5 depends on table size, ranges from 20 to 36  */
  QTMfdi_initmodel(&QTM(model5), &QTM(m5sym)[0], (msz < 36) ? msz : 36, 0);
  /* model 6pos depends on table size, ranges from 20 to 42 */
  QTMfdi_initmodel(&QTM(model6pos), &QTM(m6psym)[0], msz, 0);
  QTMfdi_initmodel(&QTM(model6len), &QTM(m6lsym)[0], 27, 0);

  return DECR_OK;
}

/************************************************************
 * LZXfdi_init (internal)
 */
static int LZXfdi_init(int window, fdi_decomp_state *decomp_state) {
  cab_ULONG wndsize = 1 << window;
  int i, j, posn_slots;

  /* LZX supports window sizes of 2^15 (32Kb) through 2^21 (2Mb) */
  /* if a previously allocated window is big enough, keep it     */
  if (window < 15 || window > 21) return DECR_DATAFORMAT;
  if (LZX(actual_size) < wndsize) {
    if (LZX(window)) PFDI_FREE(CAB(hfdi), LZX(window));
    LZX(window) = NULL;
  }
  if (!LZX(window)) {
    if (!(LZX(window) = PFDI_ALLOC(CAB(hfdi), wndsize))) return DECR_NOMEMORY;
    LZX(actual_size) = wndsize;
  }
  LZX(window_size) = wndsize;

  /* initialize static tables */
  for (i=0, j=0; i <= 50; i += 2) {
    CAB(extra_bits)[i] = CAB(extra_bits)[i+1] = j; /* 0,0,0,0,1,1,2,2,3,3... */
    if ((i != 0) && (j < 17)) j++; /* 0,0,1,2,3,4...15,16,17,17,17,17... */
  }
  for (i=0, j=0; i <= 50; i++) {
    CAB(lzx_position_base)[i] = j; /* 0,1,2,3,4,6,8,12,16,24,32,... */
    j += 1 << CAB(extra_bits)[i]; /* 1,1,1,1,2,2,4,4,8,8,16,16,32,32,... */
  }

  /* calculate required position slots */
       if (window == 20) posn_slots = 42;
  else if (window == 21) posn_slots = 50;
  else posn_slots = window << 1;

  /*posn_slots=i=0; while (i < wndsize) i += 1 << CAB(extra_bits)[posn_slots++]; */

  LZX(R0)  =  LZX(R1)  = LZX(R2) = 1;
  LZX(main_elements)   = LZX_NUM_CHARS + (posn_slots << 3);
  LZX(header_read)     = 0;
  LZX(frames_read)     = 0;
  LZX(block_remaining) = 0;
  LZX(block_type)      = LZX_BLOCKTYPE_INVALID;
  LZX(intel_curpos)    = 0;
  LZX(intel_started)   = 0;
  LZX(window_posn)     = 0;

  /* initialize tables to 0 (because deltas will be applied to them) */
  for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS; i++) LZX(MAINTREE_len)[i] = 0;
  for (i = 0; i < LZX_LENGTH_MAXSYMBOLS; i++)   LZX(LENGTH_len)[i]   = 0;

  return DECR_OK;
}

/****************************************************
 * NONEfdi_decomp(internal)
 */
static int NONEfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state)
{
  if (inlen != outlen) return DECR_ILLEGALDATA;
  memcpy(CAB(outbuf), CAB(inbuf), (size_t) inlen);
  return DECR_OK;
}

/********************************************************
 * Ziphuft_free (internal)
 */
static void fdi_Ziphuft_free(HFDI hfdi, struct Ziphuft *t)
{
  register struct Ziphuft *p, *q;

  /* Go through linked list, freeing from the allocated (t[-1]) address. */
  p = t;
  while (p != (struct Ziphuft *)NULL)
  {
    q = (--p)->v.t;
    PFDI_FREE(hfdi, p);
    p = q;
  } 
}

/*********************************************************
 * fdi_Ziphuft_build (internal)
 */
static cab_LONG fdi_Ziphuft_build(cab_ULONG *b, cab_ULONG n, cab_ULONG s, cab_UWORD *d, cab_UWORD *e,
struct Ziphuft **t, cab_LONG *m, fdi_decomp_state *decomp_state)
{
  cab_ULONG a;                   	/* counter for codes of length k */
  cab_ULONG el;                  	/* length of EOB code (value 256) */
  cab_ULONG f;                   	/* i repeats in table every f entries */
  cab_LONG g;                    	/* maximum code length */
  cab_LONG h;                    	/* table level */
  register cab_ULONG i;          	/* counter, current code */
  register cab_ULONG j;          	/* counter */
  register cab_LONG k;           	/* number of bits in current code */
  cab_LONG *l;                  	/* stack of bits per table */
  register cab_ULONG *p;         	/* pointer into ZIP(c)[],ZIP(b)[],ZIP(v)[] */
  register struct Ziphuft *q;           /* points to current table */
  struct Ziphuft r;                     /* table entry for structure assignment */
  register cab_LONG w;                  /* bits before this table == (l * h) */
  cab_ULONG *xp;                 	/* pointer into x */
  cab_LONG y;                           /* number of dummy codes added */
  cab_ULONG z;                   	/* number of entries in current table */

  l = ZIP(lx)+1;

  /* Generate counts for each bit length */
  el = n > 256 ? b[256] : ZIPBMAX; /* set length of EOB code, if any */

  for(i = 0; i < ZIPBMAX+1; ++i)
    ZIP(c)[i] = 0;
  p = b;  i = n;
  do
  {
    ZIP(c)[*p]++; p++;               /* assume all entries <= ZIPBMAX */
  } while (--i);
  if (ZIP(c)[0] == n)                /* null input--all zero length codes */
  {
    *t = (struct Ziphuft *)NULL;
    *m = 0;
    return 0;
  }

  /* Find minimum and maximum length, bound *m by those */
  for (j = 1; j <= ZIPBMAX; j++)
    if (ZIP(c)[j])
      break;
  k = j;                        /* minimum code length */
  if ((cab_ULONG)*m < j)
    *m = j;
  for (i = ZIPBMAX; i; i--)
    if (ZIP(c)[i])
      break;
  g = i;                        /* maximum code length */
  if ((cab_ULONG)*m > i)
    *m = i;

  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= ZIP(c)[j]) < 0)
      return 2;                 /* bad input: more codes than bits */
  if ((y -= ZIP(c)[i]) < 0)
    return 2;
  ZIP(c)[i] += y;

  /* Generate starting offsets LONGo the value table for each length */
  ZIP(x)[1] = j = 0;
  p = ZIP(c) + 1;  xp = ZIP(x) + 2;
  while (--i)
  {                 /* note that i == g from above */
    *xp++ = (j += *p++);
  }

  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do{
    if ((j = *p++) != 0)
      ZIP(v)[ZIP(x)[j]++] = i;
  } while (++i < n);


  /* Generate the Huffman codes and for each, make the table entries */
  ZIP(x)[0] = i = 0;                 /* first Huffman code is zero */
  p = ZIP(v);                        /* grab values in bit order */
  h = -1;                       /* no tables yet--level -1 */
  w = l[-1] = 0;                /* no bits decoded yet */
  ZIP(u)[0] = (struct Ziphuft *)NULL;   /* just to keep compilers happy */
  q = (struct Ziphuft *)NULL;      /* ditto */
  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++)
  {
    a = ZIP(c)[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l[h])
      {
        w += l[h++];            /* add bits already decoded */

        /* compute minimum size table less than or equal to *m bits */
        z = (z = g - w) > (cab_ULONG)*m ? *m : z;        /* upper limit */
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = ZIP(c) + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if ((f <<= 1) <= *++xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        if ((cab_ULONG)w + j > el && (cab_ULONG)w < el)
          j = el - w;           /* make EOB code end at table */
        z = 1 << j;             /* table entries for j-bit table */
        l[h] = j;               /* set table size in stack */

        /* allocate and link in new table */
        if (!(q = (struct Ziphuft *) PFDI_ALLOC(CAB(hfdi), (z + 1)*sizeof(struct Ziphuft))))
        {
          if(h)
            fdi_Ziphuft_free(CAB(hfdi), ZIP(u)[0]);
          return 3;             /* not enough memory */
        }
        *t = q + 1;             /* link to list for Ziphuft_free() */
        *(t = &(q->v.t)) = (struct Ziphuft *)NULL;
        ZIP(u)[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          ZIP(x)[h] = i;              /* save pattern for backing up */
          r.b = (cab_UBYTE)l[h-1];    /* bits to dump before this table */
          r.e = (cab_UBYTE)(16 + j);  /* bits in this table */
          r.v.t = q;                  /* pointer to this table */
          j = (i & ((1 << w) - 1)) >> (w - l[h-1]);
          ZIP(u)[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = (cab_UBYTE)(k - w);
      if (p >= ZIP(v) + n)
        r.e = 99;               /* out of values--invalid code */
      else if (*p < s)
      {
        r.e = (cab_UBYTE)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
        r.v.n = *p++;           /* simple code is just the value */
      }
      else
      {
        r.e = (cab_UBYTE)e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != ZIP(x)[h])
        w -= l[--h];            /* don't need to update q */
    }
  }

  /* return actual size of base table */
  *m = l[0];

  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}

/*********************************************************
 * fdi_Zipinflate_codes (internal)
 */
cab_LONG fdi_Zipinflate_codes(struct Ziphuft *tl, struct Ziphuft *td,
  cab_LONG bl, cab_LONG bd, fdi_decomp_state *decomp_state)
{
  register cab_ULONG e;  /* table entry flag/number of extra bits */
  cab_ULONG n, d;        /* length and index for copy */
  cab_ULONG w;           /* current window position */
  struct Ziphuft *t;     /* pointer to table entry */
  cab_ULONG ml, md;      /* masks for bl and bd bits */
  register cab_ULONG b;  /* bit buffer */
  register cab_ULONG k;  /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = ZIP(window_posn);                       /* initialize window position */

  /* inflate the coded data */
  ml = Zipmask[bl];           	/* precompute masks for speed */
  md = Zipmask[bd];

  for(;;)
  {
    ZIPNEEDBITS((cab_ULONG)bl)
    if((e = (t = tl + ((cab_ULONG)b & ml))->e) > 16)
      do
      {
        if (e == 99)
          return 1;
        ZIPDUMPBITS(t->b)
        e -= 16;
        ZIPNEEDBITS(e)
      } while ((e = (t = t->v.t + ((cab_ULONG)b & Zipmask[e]))->e) > 16);
    ZIPDUMPBITS(t->b)
    if (e == 16)                /* then it's a literal */
      CAB(outbuf)[w++] = (cab_UBYTE)t->v.n;
    else                        /* it's an EOB or a length */
    {
      /* exit if end of block */
      if(e == 15)
        break;

      /* get length of block to copy */
      ZIPNEEDBITS(e)
      n = t->v.n + ((cab_ULONG)b & Zipmask[e]);
      ZIPDUMPBITS(e);

      /* decode distance of block to copy */
      ZIPNEEDBITS((cab_ULONG)bd)
      if ((e = (t = td + ((cab_ULONG)b & md))->e) > 16)
        do {
          if (e == 99)
            return 1;
          ZIPDUMPBITS(t->b)
          e -= 16;
          ZIPNEEDBITS(e)
        } while ((e = (t = t->v.t + ((cab_ULONG)b & Zipmask[e]))->e) > 16);
      ZIPDUMPBITS(t->b)
      ZIPNEEDBITS(e)
      d = w - t->v.n - ((cab_ULONG)b & Zipmask[e]);
      ZIPDUMPBITS(e)
      do
      {
        n -= (e = (e = ZIPWSIZE - ((d &= ZIPWSIZE-1) > w ? d : w)) > n ?n:e);
        do
        {
          CAB(outbuf)[w++] = CAB(outbuf)[d++];
        } while (--e);
      } while (n);
    }
  }

  /* restore the globals from the locals */
  ZIP(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;

  /* done */
  return 0;
}

/***********************************************************
 * Zipinflate_stored (internal)
 */
static cab_LONG fdi_Zipinflate_stored(fdi_decomp_state *decomp_state)
/* "decompress" an inflated type 0 (stored) block. */
{
  cab_ULONG n;           /* number of bytes in block */
  cab_ULONG w;           /* current window position */
  register cab_ULONG b;  /* bit buffer */
  register cab_ULONG k;  /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = ZIP(window_posn);              /* initialize window position */

  /* go to byte boundary */
  n = k & 7;
  ZIPDUMPBITS(n);

  /* get the length and its complement */
  ZIPNEEDBITS(16)
  n = ((cab_ULONG)b & 0xffff);
  ZIPDUMPBITS(16)
  ZIPNEEDBITS(16)
  if (n != (cab_ULONG)((~b) & 0xffff))
    return 1;                   /* error in compressed data */
  ZIPDUMPBITS(16)

  /* read and output the compressed data */
  while(n--)
  {
    ZIPNEEDBITS(8)
    CAB(outbuf)[w++] = (cab_UBYTE)b;
    ZIPDUMPBITS(8)
  }

  /* restore the globals from the locals */
  ZIP(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;
  return 0;
}

/******************************************************
 * fdi_Zipinflate_fixed (internal)
 */
static cab_LONG fdi_Zipinflate_fixed(fdi_decomp_state *decomp_state)
{
  struct Ziphuft *fixed_tl;
  struct Ziphuft *fixed_td;
  cab_LONG fixed_bl, fixed_bd;
  cab_LONG i;                /* temporary variable */
  cab_ULONG *l;

  l = ZIP(ll);

  /* literal table */
  for(i = 0; i < 144; i++)
    l[i] = 8;
  for(; i < 256; i++)
    l[i] = 9;
  for(; i < 280; i++)
    l[i] = 7;
  for(; i < 288; i++)          /* make a complete, but wrong code set */
    l[i] = 8;
  fixed_bl = 7;
  if((i = fdi_Ziphuft_build(l, 288, 257, (cab_UWORD *) Zipcplens,
  (cab_UWORD *) Zipcplext, &fixed_tl, &fixed_bl, decomp_state)))
    return i;

  /* distance table */
  for(i = 0; i < 30; i++)      /* make an incomplete code set */
    l[i] = 5;
  fixed_bd = 5;
  if((i = fdi_Ziphuft_build(l, 30, 0, (cab_UWORD *) Zipcpdist, (cab_UWORD *) Zipcpdext,
  &fixed_td, &fixed_bd, decomp_state)) > 1)
  {
    fdi_Ziphuft_free(CAB(hfdi), fixed_tl);
    return i;
  }

  /* decompress until an end-of-block code */
  i = fdi_Zipinflate_codes(fixed_tl, fixed_td, fixed_bl, fixed_bd, decomp_state);

  fdi_Ziphuft_free(CAB(hfdi), fixed_td);
  fdi_Ziphuft_free(CAB(hfdi), fixed_tl);
  return i;
}

/**************************************************************
 * fdi_Zipinflate_dynamic (internal)
 */
static cab_LONG fdi_Zipinflate_dynamic(fdi_decomp_state *decomp_state)
 /* decompress an inflated type 2 (dynamic Huffman codes) block. */
{
  cab_LONG i;          	/* temporary variables */
  cab_ULONG j;
  cab_ULONG *ll;
  cab_ULONG l;           	/* last length */
  cab_ULONG m;           	/* mask for bit lengths table */
  cab_ULONG n;           	/* number of lengths to get */
  struct Ziphuft *tl;           /* literal/length code table */
  struct Ziphuft *td;           /* distance code table */
  cab_LONG bl;                  /* lookup bits for tl */
  cab_LONG bd;                  /* lookup bits for td */
  cab_ULONG nb;          	/* number of bit length codes */
  cab_ULONG nl;          	/* number of literal/length codes */
  cab_ULONG nd;          	/* number of distance codes */
  register cab_ULONG b;         /* bit buffer */
  register cab_ULONG k;	        /* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);
  ll = ZIP(ll);

  /* read in table lengths */
  ZIPNEEDBITS(5)
  nl = 257 + ((cab_ULONG)b & 0x1f);      /* number of literal/length codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(5)
  nd = 1 + ((cab_ULONG)b & 0x1f);        /* number of distance codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(4)
  nb = 4 + ((cab_ULONG)b & 0xf);         /* number of bit length codes */
  ZIPDUMPBITS(4)
  if(nl > 288 || nd > 32)
    return 1;                   /* bad lengths */

  /* read in bit-length-code lengths */
  for(j = 0; j < nb; j++)
  {
    ZIPNEEDBITS(3)
    ll[Zipborder[j]] = (cab_ULONG)b & 7;
    ZIPDUMPBITS(3)
  }
  for(; j < 19; j++)
    ll[Zipborder[j]] = 0;

  /* build decoding table for trees--single level, 7 bit lookup */
  bl = 7;
  if((i = fdi_Ziphuft_build(ll, 19, 19, NULL, NULL, &tl, &bl, decomp_state)) != 0)
  {
    if(i == 1)
      fdi_Ziphuft_free(CAB(hfdi), tl);
    return i;                   /* incomplete code set */
  }

  /* read in literal and distance code lengths */
  n = nl + nd;
  m = Zipmask[bl];
  i = l = 0;
  while((cab_ULONG)i < n)
  {
    ZIPNEEDBITS((cab_ULONG)bl)
    j = (td = tl + ((cab_ULONG)b & m))->b;
    ZIPDUMPBITS(j)
    j = td->v.n;
    if (j < 16)                 /* length of code in bits (0..15) */
      ll[i++] = l = j;          /* save last length in l */
    else if (j == 16)           /* repeat last length 3 to 6 times */
    {
      ZIPNEEDBITS(2)
      j = 3 + ((cab_ULONG)b & 3);
      ZIPDUMPBITS(2)
      if((cab_ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = l;
    }
    else if (j == 17)           /* 3 to 10 zero length codes */
    {
      ZIPNEEDBITS(3)
      j = 3 + ((cab_ULONG)b & 7);
      ZIPDUMPBITS(3)
      if ((cab_ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
    else                        /* j == 18: 11 to 138 zero length codes */
    {
      ZIPNEEDBITS(7)
      j = 11 + ((cab_ULONG)b & 0x7f);
      ZIPDUMPBITS(7)
      if ((cab_ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
  }

  /* free decoding table for trees */
  fdi_Ziphuft_free(CAB(hfdi), tl);

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* build the decoding tables for literal/length and distance codes */
  bl = ZIPLBITS;
  if((i = fdi_Ziphuft_build(ll, nl, 257, (cab_UWORD *) Zipcplens, (cab_UWORD *) Zipcplext,
                        &tl, &bl, decomp_state)) != 0)
  {
    if(i == 1)
      fdi_Ziphuft_free(CAB(hfdi), tl);
    return i;                   /* incomplete code set */
  }
  bd = ZIPDBITS;
  fdi_Ziphuft_build(ll + nl, nd, 0, (cab_UWORD *) Zipcpdist, (cab_UWORD *) Zipcpdext,
                &td, &bd, decomp_state);

  /* decompress until an end-of-block code */
  if(fdi_Zipinflate_codes(tl, td, bl, bd, decomp_state))
    return 1;

  /* free the decoding tables, return */
  fdi_Ziphuft_free(CAB(hfdi), tl);
  fdi_Ziphuft_free(CAB(hfdi), td);
  return 0;
}

/*****************************************************
 * fdi_Zipinflate_block (internal)
 */
static cab_LONG fdi_Zipinflate_block(cab_LONG *e, fdi_decomp_state *decomp_state) /* e == last block flag */
{ /* decompress an inflated block */
  cab_ULONG t;           	/* block type */
  register cab_ULONG b;     /* bit buffer */
  register cab_ULONG k;     /* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);

  /* read in last block bit */
  ZIPNEEDBITS(1)
  *e = (cab_LONG)b & 1;
  ZIPDUMPBITS(1)

  /* read in block type */
  ZIPNEEDBITS(2)
  t = (cab_ULONG)b & 3;
  ZIPDUMPBITS(2)

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* inflate that block type */
  if(t == 2)
    return fdi_Zipinflate_dynamic(decomp_state);
  if(t == 0)
    return fdi_Zipinflate_stored(decomp_state);
  if(t == 1)
    return fdi_Zipinflate_fixed(decomp_state);
  /* bad block type */
  return 2;
}

/****************************************************
 * ZIPfdi_decomp(internal)
 */
static int ZIPfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state)
{
  cab_LONG e;               /* last block flag */

  TRACE("(inlen == %d, outlen == %d)\n", inlen, outlen);

  ZIP(inpos) = CAB(inbuf);
  ZIP(bb) = ZIP(bk) = ZIP(window_posn) = 0;
  if(outlen > ZIPWSIZE)
    return DECR_DATAFORMAT;

  /* CK = Chris Kirmse, official Microsoft purloiner */
  if(ZIP(inpos)[0] != 0x43 || ZIP(inpos)[1] != 0x4B)
    return DECR_ILLEGALDATA;
  ZIP(inpos) += 2;

  do {
    if(fdi_Zipinflate_block(&e, decomp_state))
      return DECR_ILLEGALDATA;
  } while(!e);

  /* return success */
  return DECR_OK;
}

/*******************************************************************
 * QTMfdi_decomp(internal)
 */
static int QTMfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state)
{
  cab_UBYTE *inpos  = CAB(inbuf);
  cab_UBYTE *window = QTM(window);
  cab_UBYTE *runsrc, *rundest;

  cab_ULONG window_posn = QTM(window_posn);
  cab_ULONG window_size = QTM(window_size);

  /* used by bitstream macros */
  register int bitsleft, bitrun, bitsneed;
  register cab_ULONG bitbuf;

  /* used by GET_SYMBOL */
  cab_ULONG range;
  cab_UWORD symf;
  int i;

  int extra, togo = outlen, match_length = 0, copy_length;
  cab_UBYTE selector, sym;
  cab_ULONG match_offset = 0;

  cab_UWORD H = 0xFFFF, L = 0, C;

  TRACE("(inlen == %d, outlen == %d)\n", inlen, outlen);

  /* read initial value of C */
  Q_INIT_BITSTREAM;
  Q_READ_BITS(C, 16);

  /* apply 2^x-1 mask */
  window_posn &= window_size - 1;
  /* runs can't straddle the window wraparound */
  if ((window_posn + togo) > window_size) {
    TRACE("straddled run\n");
    return DECR_DATAFORMAT;
  }

  while (togo > 0) {
    GET_SYMBOL(model7, selector);
    switch (selector) {
    case 0:
      GET_SYMBOL(model00, sym); window[window_posn++] = sym; togo--;
      break;
    case 1:
      GET_SYMBOL(model40, sym); window[window_posn++] = sym; togo--;
      break;
    case 2:
      GET_SYMBOL(model80, sym); window[window_posn++] = sym; togo--;
      break;
    case 3:
      GET_SYMBOL(modelC0, sym); window[window_posn++] = sym; togo--;
      break;

    case 4:
      /* selector 4 = fixed length of 3 */
      GET_SYMBOL(model4, sym);
      Q_READ_BITS(extra, CAB(q_extra_bits)[sym]);
      match_offset = CAB(q_position_base)[sym] + extra + 1;
      match_length = 3;
      break;

    case 5:
      /* selector 5 = fixed length of 4 */
      GET_SYMBOL(model5, sym);
      Q_READ_BITS(extra, CAB(q_extra_bits)[sym]);
      match_offset = CAB(q_position_base)[sym] + extra + 1;
      match_length = 4;
      break;

    case 6:
      /* selector 6 = variable length */
      GET_SYMBOL(model6len, sym);
      Q_READ_BITS(extra, CAB(q_length_extra)[sym]);
      match_length = CAB(q_length_base)[sym] + extra + 5;
      GET_SYMBOL(model6pos, sym);
      Q_READ_BITS(extra, CAB(q_extra_bits)[sym]);
      match_offset = CAB(q_position_base)[sym] + extra + 1;
      break;

    default:
      TRACE("Selector is bogus\n");
      return DECR_ILLEGALDATA;
    }

    /* if this is a match */
    if (selector >= 4) {
      rundest = window + window_posn;
      togo -= match_length;

      /* copy any wrapped around source data */
      if (window_posn >= match_offset) {
        /* no wrap */
        runsrc = rundest - match_offset;
      } else {
        runsrc = rundest + (window_size - match_offset);
        copy_length = match_offset - window_posn;
        if (copy_length < match_length) {
          match_length -= copy_length;
          window_posn += copy_length;
          while (copy_length-- > 0) *rundest++ = *runsrc++;
          runsrc = window;
        }
      }
      window_posn += match_length;

      /* copy match data - no worries about destination wraps */
      while (match_length-- > 0) *rundest++ = *runsrc++;
    }
  } /* while (togo > 0) */

  if (togo != 0) {
    TRACE("Frame overflow, this_run = %d\n", togo);
    return DECR_ILLEGALDATA;
  }

  memcpy(CAB(outbuf), window + ((!window_posn) ? window_size : window_posn) -
    outlen, outlen);

  QTM(window_posn) = window_posn;
  return DECR_OK;
}

/************************************************************
 * fdi_lzx_read_lens (internal)
 */
static int fdi_lzx_read_lens(cab_UBYTE *lens, cab_ULONG first, cab_ULONG last, struct lzx_bits *lb,
                  fdi_decomp_state *decomp_state) {
  cab_ULONG i,j, x,y;
  int z;

  register cab_ULONG bitbuf = lb->bb;
  register int bitsleft = lb->bl;
  cab_UBYTE *inpos = lb->ip;
  cab_UWORD *hufftbl;
  
  for (x = 0; x < 20; x++) {
    READ_BITS(y, 4);
    LENTABLE(PRETREE)[x] = y;
  }
  BUILD_TABLE(PRETREE);

  for (x = first; x < last; ) {
    READ_HUFFSYM(PRETREE, z);
    if (z == 17) {
      READ_BITS(y, 4); y += 4;
      while (y--) lens[x++] = 0;
    }
    else if (z == 18) {
      READ_BITS(y, 5); y += 20;
      while (y--) lens[x++] = 0;
    }
    else if (z == 19) {
      READ_BITS(y, 1); y += 4;
      READ_HUFFSYM(PRETREE, z);
      z = lens[x] - z; if (z < 0) z += 17;
      while (y--) lens[x++] = z;
    }
    else {
      z = lens[x] - z; if (z < 0) z += 17;
      lens[x++] = z;
    }
  }

  lb->bb = bitbuf;
  lb->bl = bitsleft;
  lb->ip = inpos;
  return 0;
}

/*******************************************************
 * LZXfdi_decomp(internal)
 */
static int LZXfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state) {
  cab_UBYTE *inpos  = CAB(inbuf);
  cab_UBYTE *endinp = inpos + inlen;
  cab_UBYTE *window = LZX(window);
  cab_UBYTE *runsrc, *rundest;
  cab_UWORD *hufftbl; /* used in READ_HUFFSYM macro as chosen decoding table */

  cab_ULONG window_posn = LZX(window_posn);
  cab_ULONG window_size = LZX(window_size);
  cab_ULONG R0 = LZX(R0);
  cab_ULONG R1 = LZX(R1);
  cab_ULONG R2 = LZX(R2);

  register cab_ULONG bitbuf;
  register int bitsleft;
  cab_ULONG match_offset, i,j,k; /* ijk used in READ_HUFFSYM macro */
  struct lzx_bits lb; /* used in READ_LENGTHS macro */

  int togo = outlen, this_run, main_element, aligned_bits;
  int match_length, copy_length, length_footer, extra, verbatim_bits;

  TRACE("(inlen == %d, outlen == %d)\n", inlen, outlen);

  INIT_BITSTREAM;

  /* read header if necessary */
  if (!LZX(header_read)) {
    i = j = 0;
    READ_BITS(k, 1); if (k) { READ_BITS(i,16); READ_BITS(j,16); }
    LZX(intel_filesize) = (i << 16) | j; /* or 0 if not encoded */
    LZX(header_read) = 1;
  }

  /* main decoding loop */
  while (togo > 0) {
    /* last block finished, new block expected */
    if (LZX(block_remaining) == 0) {
      if (LZX(block_type) == LZX_BLOCKTYPE_UNCOMPRESSED) {
        if (LZX(block_length) & 1) inpos++; /* realign bitstream to word */
        INIT_BITSTREAM;
      }

      READ_BITS(LZX(block_type), 3);
      READ_BITS(i, 16);
      READ_BITS(j, 8);
      LZX(block_remaining) = LZX(block_length) = (i << 8) | j;

      switch (LZX(block_type)) {
      case LZX_BLOCKTYPE_ALIGNED:
        for (i = 0; i < 8; i++) { READ_BITS(j, 3); LENTABLE(ALIGNED)[i] = j; }
        BUILD_TABLE(ALIGNED);
        /* rest of aligned header is same as verbatim */

      case LZX_BLOCKTYPE_VERBATIM:
        READ_LENGTHS(MAINTREE, 0, 256, fdi_lzx_read_lens);
        READ_LENGTHS(MAINTREE, 256, LZX(main_elements), fdi_lzx_read_lens);
        BUILD_TABLE(MAINTREE);
        if (LENTABLE(MAINTREE)[0xE8] != 0) LZX(intel_started) = 1;

        READ_LENGTHS(LENGTH, 0, LZX_NUM_SECONDARY_LENGTHS, fdi_lzx_read_lens);
        BUILD_TABLE(LENGTH);
        break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
        LZX(intel_started) = 1; /* because we can't assume otherwise */
        ENSURE_BITS(16); /* get up to 16 pad bits into the buffer */
        if (bitsleft > 16) inpos -= 2; /* and align the bitstream! */
        R0 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        R1 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        R2 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        break;

      default:
        return DECR_ILLEGALDATA;
      }
    }

    /* buffer exhaustion check */
    if (inpos > endinp) {
      /* it's possible to have a file where the next run is less than
       * 16 bits in size. In this case, the READ_HUFFSYM() macro used
       * in building the tables will exhaust the buffer, so we should
       * allow for this, but not allow those accidentally read bits to
       * be used (so we check that there are at least 16 bits
       * remaining - in this boundary case they aren't really part of
       * the compressed data)
       */
      if (inpos > (endinp+2) || bitsleft < 16) return DECR_ILLEGALDATA;
    }

    while ((this_run = LZX(block_remaining)) > 0 && togo > 0) {
      if (this_run > togo) this_run = togo;
      togo -= this_run;
      LZX(block_remaining) -= this_run;

      /* apply 2^x-1 mask */
      window_posn &= window_size - 1;
      /* runs can't straddle the window wraparound */
      if ((window_posn + this_run) > window_size)
        return DECR_DATAFORMAT;

      switch (LZX(block_type)) {

      case LZX_BLOCKTYPE_VERBATIM:
        while (this_run > 0) {
          READ_HUFFSYM(MAINTREE, main_element);

          if (main_element < LZX_NUM_CHARS) {
            /* literal: 0 to LZX_NUM_CHARS-1 */
            window[window_posn++] = main_element;
            this_run--;
          }
          else {
            /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
            main_element -= LZX_NUM_CHARS;
  
            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              READ_HUFFSYM(LENGTH, length_footer);
              match_length += length_footer;
            }
            match_length += LZX_MIN_MATCH;
  
            match_offset = main_element >> 3;
  
            if (match_offset > 2) {
              /* not repeated offset */
              if (match_offset != 3) {
                extra = CAB(extra_bits)[match_offset];
                READ_BITS(verbatim_bits, extra);
                match_offset = CAB(lzx_position_base)[match_offset] 
                               - 2 + verbatim_bits;
              }
              else {
                match_offset = 1;
              }
  
              /* update repeated offset LRU queue */
              R2 = R1; R1 = R0; R0 = match_offset;
            }
            else if (match_offset == 0) {
              match_offset = R0;
            }
            else if (match_offset == 1) {
              match_offset = R1;
              R1 = R0; R0 = match_offset;
            }
            else /* match_offset == 2 */ {
              match_offset = R2;
              R2 = R0; R0 = match_offset;
            }

            rundest = window + window_posn;
            this_run -= match_length;

            /* copy any wrapped around source data */
            if (window_posn >= match_offset) {
              /* no wrap */
              runsrc = rundest - match_offset;
            } else {
              runsrc = rundest + (window_size - match_offset);
              copy_length = match_offset - window_posn;
              if (copy_length < match_length) {
                match_length -= copy_length;
                window_posn += copy_length;
                while (copy_length-- > 0) *rundest++ = *runsrc++;
                runsrc = window;
              }
            }
            window_posn += match_length;

            /* copy match data - no worries about destination wraps */
            while (match_length-- > 0) *rundest++ = *runsrc++;
          }
        }
        break;

      case LZX_BLOCKTYPE_ALIGNED:
        while (this_run > 0) {
          READ_HUFFSYM(MAINTREE, main_element);
  
          if (main_element < LZX_NUM_CHARS) {
            /* literal: 0 to LZX_NUM_CHARS-1 */
            window[window_posn++] = main_element;
            this_run--;
          }
          else {
            /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
            main_element -= LZX_NUM_CHARS;
  
            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              READ_HUFFSYM(LENGTH, length_footer);
              match_length += length_footer;
            }
            match_length += LZX_MIN_MATCH;
  
            match_offset = main_element >> 3;
  
            if (match_offset > 2) {
              /* not repeated offset */
              extra = CAB(extra_bits)[match_offset];
              match_offset = CAB(lzx_position_base)[match_offset] - 2;
              if (extra > 3) {
                /* verbatim and aligned bits */
                extra -= 3;
                READ_BITS(verbatim_bits, extra);
                match_offset += (verbatim_bits << 3);
                READ_HUFFSYM(ALIGNED, aligned_bits);
                match_offset += aligned_bits;
              }
              else if (extra == 3) {
                /* aligned bits only */
                READ_HUFFSYM(ALIGNED, aligned_bits);
                match_offset += aligned_bits;
              }
              else if (extra > 0) { /* extra==1, extra==2 */
                /* verbatim bits only */
                READ_BITS(verbatim_bits, extra);
                match_offset += verbatim_bits;
              }
              else /* extra == 0 */ {
                /* ??? */
                match_offset = 1;
              }
  
              /* update repeated offset LRU queue */
              R2 = R1; R1 = R0; R0 = match_offset;
            }
            else if (match_offset == 0) {
              match_offset = R0;
            }
            else if (match_offset == 1) {
              match_offset = R1;
              R1 = R0; R0 = match_offset;
            }
            else /* match_offset == 2 */ {
              match_offset = R2;
              R2 = R0; R0 = match_offset;
            }

            rundest = window + window_posn;
            this_run -= match_length;

            /* copy any wrapped around source data */
            if (window_posn >= match_offset) {
              /* no wrap */
              runsrc = rundest - match_offset;
            } else {
              runsrc = rundest + (window_size - match_offset);
              copy_length = match_offset - window_posn;
              if (copy_length < match_length) {
                match_length -= copy_length;
                window_posn += copy_length;
                while (copy_length-- > 0) *rundest++ = *runsrc++;
                runsrc = window;
              }
            }
            window_posn += match_length;

            /* copy match data - no worries about destination wraps */
            while (match_length-- > 0) *rundest++ = *runsrc++;
          }
        }
        break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
        if ((inpos + this_run) > endinp) return DECR_ILLEGALDATA;
        memcpy(window + window_posn, inpos, (size_t) this_run);
        inpos += this_run; window_posn += this_run;
        break;

      default:
        return DECR_ILLEGALDATA; /* might as well */
      }

    }
  }

  if (togo != 0) return DECR_ILLEGALDATA;
  memcpy(CAB(outbuf), window + ((!window_posn) ? window_size : window_posn) -
    outlen, (size_t) outlen);

  LZX(window_posn) = window_posn;
  LZX(R0) = R0;
  LZX(R1) = R1;
  LZX(R2) = R2;

  /* intel E8 decoding */
  if ((LZX(frames_read)++ < 32768) && LZX(intel_filesize) != 0) {
    if (outlen <= 6 || !LZX(intel_started)) {
      LZX(intel_curpos) += outlen;
    }
    else {
      cab_UBYTE *data    = CAB(outbuf);
      cab_UBYTE *dataend = data + outlen - 10;
      cab_LONG curpos    = LZX(intel_curpos);
      cab_LONG filesize  = LZX(intel_filesize);
      cab_LONG abs_off, rel_off;

      LZX(intel_curpos) = curpos + outlen;

      while (data < dataend) {
        if (*data++ != 0xE8) { curpos++; continue; }
        abs_off = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
        if ((abs_off >= -curpos) && (abs_off < filesize)) {
          rel_off = (abs_off >= 0) ? abs_off - curpos : abs_off + filesize;
          data[0] = (cab_UBYTE) rel_off;
          data[1] = (cab_UBYTE) (rel_off >> 8);
          data[2] = (cab_UBYTE) (rel_off >> 16);
          data[3] = (cab_UBYTE) (rel_off >> 24);
        }
        data += 4;
        curpos += 5;
      }
    }
  }
  return DECR_OK;
}

/**********************************************************
 * fdi_decomp (internal)
 *
 * Decompress the requested number of bytes.  If savemode is zero,
 * do not save the output anywhere, just plow through blocks until we
 * reach the specified (uncompressed) distance from the starting point,
 * and remember the position of the cabfile pointer (and which cabfile)
 * after we are done; otherwise, save the data out to CAB(filehf),
 * decompressing the requested number of bytes and writing them out.  This
 * is also where we jump to additional cabinets in the case of split
 * cab's, and provide (some of) the NEXT_CABINET notification semantics.
 */
static int fdi_decomp(struct fdi_file *fi, int savemode, fdi_decomp_state *decomp_state,
  char *pszCabPath, PFNFDINOTIFY pfnfdin, void *pvUser)
{
  cab_ULONG bytes = savemode ? fi->length : fi->offset - CAB(offset);
  cab_UBYTE buf[cfdata_SIZEOF], *data;
  cab_UWORD inlen, len, outlen, cando;
  cab_ULONG cksum;
  cab_LONG err;
  fdi_decomp_state *cab = (savemode && CAB(decomp_cab)) ? CAB(decomp_cab) : decomp_state;

  TRACE("(fi == ^%p, savemode == %d, bytes == %d)\n", fi, savemode, bytes);

  while (bytes > 0) {
    /* cando = the max number of bytes we can do */
    cando = CAB(outlen);
    if (cando > bytes) cando = bytes;

    /* if cando != 0 */
    if (cando && savemode)
      PFDI_WRITE(CAB(hfdi), CAB(filehf), CAB(outpos), cando);

    CAB(outpos) += cando;
    CAB(outlen) -= cando;
    bytes -= cando; if (!bytes) break;

    /* we only get here if we emptied the output buffer */

    /* read data header + data */
    inlen = outlen = 0;
    while (outlen == 0) {
      /* read the block header, skip the reserved part */
      if (PFDI_READ(CAB(hfdi), cab->cabhf, buf, cfdata_SIZEOF) != cfdata_SIZEOF)
        return DECR_INPUT;

      if (PFDI_SEEK(CAB(hfdi), cab->cabhf, cab->mii.block_resv, SEEK_CUR) == -1)
        return DECR_INPUT;

      /* we shouldn't get blocks over CAB_INPUTMAX in size */
      data = CAB(inbuf) + inlen;
      len = EndGetI16(buf+cfdata_CompressedSize);
      inlen += len;
      if (inlen > CAB_INPUTMAX) return DECR_INPUT;
      if (PFDI_READ(CAB(hfdi), cab->cabhf, data, len) != len)
        return DECR_INPUT;

      /* clear two bytes after read-in data */
      data[len+1] = data[len+2] = 0;

      /* perform checksum test on the block (if one is stored) */
      cksum = EndGetI32(buf+cfdata_CheckSum);
      if (cksum && cksum != checksum(buf+4, 4, checksum(data, len, 0)))
        return DECR_CHECKSUM; /* checksum is wrong */

      outlen = EndGetI16(buf+cfdata_UncompressedSize);

      /* outlen=0 means this block was the last contiguous part
         of a split block, continued in the next cabinet */
      if (outlen == 0) {
        int pathlen, filenamelen, idx, i, cabhf;
        char fullpath[MAX_PATH], userpath[256];
        FDINOTIFICATION fdin;
        FDICABINETINFO fdici;
        char emptystring = '\0';
        cab_UBYTE buf2[64];
        int success = FALSE;
        struct fdi_folder *fol = NULL, *linkfol = NULL; 
        struct fdi_file   *file = NULL, *linkfile = NULL;

        tryanothercab:

        /* set up the next decomp_state... */
        if (!(cab->next)) {
          if (!cab->mii.hasnext) return DECR_INPUT;

          if (!((cab->next = PFDI_ALLOC(CAB(hfdi), sizeof(fdi_decomp_state)))))
            return DECR_NOMEMORY;
        
          ZeroMemory(cab->next, sizeof(fdi_decomp_state));

          /* copy pszCabPath to userpath */
          ZeroMemory(userpath, 256);
          pathlen = (pszCabPath) ? strlen(pszCabPath) : 0;
          if (pathlen) {
            if (pathlen < 256) {
              for (i = 0; i <= pathlen; i++)
                userpath[i] = pszCabPath[i];
            } /* else we are in a weird place... let's leave it blank and see if the user fixes it */
          } 

          /* initial fdintNEXT_CABINET notification */
          ZeroMemory(&fdin, sizeof(FDINOTIFICATION));
          fdin.psz1 = (cab->mii.nextname) ? cab->mii.nextname : &emptystring;
          fdin.psz2 = (cab->mii.nextinfo) ? cab->mii.nextinfo : &emptystring;
          fdin.psz3 = &userpath[0];
          fdin.fdie = FDIERROR_NONE;
          fdin.pv = pvUser;

          if (((*pfnfdin)(fdintNEXT_CABINET, &fdin))) return DECR_USERABORT;

          do {

            pathlen = (userpath) ? strlen(userpath) : 0;
            filenamelen = (cab->mii.nextname) ? strlen(cab->mii.nextname) : 0;

            /* slight overestimation here to save CPU cycles in the developer's brain */
            if ((pathlen + filenamelen + 3) > MAX_PATH) {
              ERR("MAX_PATH exceeded.\n");
              return DECR_ILLEGALDATA;
            }

            /* paste the path and filename together */
            idx = 0;
            if (pathlen) {
              for (i = 0; i < pathlen; i++) fullpath[idx++] = userpath[i];
              if (fullpath[idx - 1] != '\\') fullpath[idx++] = '\\';
            }
            if (filenamelen) for (i = 0; i < filenamelen; i++) fullpath[idx++] = cab->mii.nextname[i];
            fullpath[idx] = '\0';
        
            TRACE("full cab path/file name: %s\n", debugstr_a(fullpath));
        
            /* try to get a handle to the cabfile */
            cabhf = PFDI_OPEN(CAB(hfdi), fullpath, 32768, _S_IREAD | _S_IWRITE);
            if (cabhf == -1) {
              /* no file.  allow the user to try again */
              fdin.fdie = FDIERROR_CABINET_NOT_FOUND;
              if (((*pfnfdin)(fdintNEXT_CABINET, &fdin))) return DECR_USERABORT;
              continue;
            }
        
            if (cabhf == 0) {
              ERR("PFDI_OPEN returned zero for %s.\n", fullpath);
              fdin.fdie = FDIERROR_CABINET_NOT_FOUND;
              if (((*pfnfdin)(fdintNEXT_CABINET, &fdin))) return DECR_USERABORT;
              continue;
            }
 
            /* check if it's really a cabfile. Note that this doesn't implement the bug */
            if (!FDI_read_entries(CAB(hfdi), cabhf, &fdici, &(cab->next->mii))) {
              WARN("FDIIsCabinet failed.\n");
              PFDI_CLOSE(CAB(hfdi), cabhf);
              fdin.fdie = FDIERROR_NOT_A_CABINET;
              if (((*pfnfdin)(fdintNEXT_CABINET, &fdin))) return DECR_USERABORT;
              continue;
            }

            if ((fdici.setID != cab->setID) || (fdici.iCabinet != (cab->iCabinet + 1))) {
              WARN("Wrong Cabinet.\n");
              PFDI_CLOSE(CAB(hfdi), cabhf);
              fdin.fdie = FDIERROR_WRONG_CABINET;
              if (((*pfnfdin)(fdintNEXT_CABINET, &fdin))) return DECR_USERABORT;
              continue;
            }
           
            break;

          } while (1);
          
          /* cabinet notification */
          ZeroMemory(&fdin, sizeof(FDINOTIFICATION));
          fdin.setID = fdici.setID;
          fdin.iCabinet = fdici.iCabinet;
          fdin.pv = pvUser;
          fdin.psz1 = (cab->next->mii.nextname) ? cab->next->mii.nextname : &emptystring;
          fdin.psz2 = (cab->next->mii.nextinfo) ? cab->next->mii.nextinfo : &emptystring;
          fdin.psz3 = pszCabPath;
        
          if (((*pfnfdin)(fdintCABINET_INFO, &fdin))) return DECR_USERABORT;
          
          cab->next->setID = fdici.setID;
          cab->next->iCabinet = fdici.iCabinet;
          cab->next->hfdi = CAB(hfdi);
          cab->next->filehf = CAB(filehf);
          cab->next->cabhf = cabhf;
          cab->next->decompress = CAB(decompress); /* crude, but unused anyhow */

          cab = cab->next; /* advance to the next cabinet */

          /* read folders */
          for (i = 0; i < fdici.cFolders; i++) {
            if (PFDI_READ(CAB(hfdi), cab->cabhf, buf2, cffold_SIZEOF) != cffold_SIZEOF) 
              return DECR_INPUT;

            if (cab->mii.folder_resv > 0)
              PFDI_SEEK(CAB(hfdi), cab->cabhf, cab->mii.folder_resv, SEEK_CUR);
        
            fol = (struct fdi_folder *) PFDI_ALLOC(CAB(hfdi), sizeof(struct fdi_folder));
            if (!fol) {
              ERR("out of memory!\n");
              return DECR_NOMEMORY;
            }
            ZeroMemory(fol, sizeof(struct fdi_folder));
            if (!(cab->firstfol)) cab->firstfol = fol;
        
            fol->offset = (cab_off_t) EndGetI32(buf2+cffold_DataOffset);
            fol->num_blocks = EndGetI16(buf2+cffold_NumBlocks);
            fol->comp_type  = EndGetI16(buf2+cffold_CompType);
        
            if (linkfol)
              linkfol->next = fol; 
            linkfol = fol;
          }
        
          /* read files */
          for (i = 0; i < fdici.cFiles; i++) {
            if (PFDI_READ(CAB(hfdi), cab->cabhf, buf2, cffile_SIZEOF) != cffile_SIZEOF)
              return DECR_INPUT;
              
            file = (struct fdi_file *) PFDI_ALLOC(CAB(hfdi), sizeof(struct fdi_file));
            if (!file) {
              ERR("out of memory!\n"); 
              return DECR_NOMEMORY;
            }
            ZeroMemory(file, sizeof(struct fdi_file));
            if (!(cab->firstfile)) cab->firstfile = file;
              
            file->length   = EndGetI32(buf2+cffile_UncompressedSize);
            file->offset   = EndGetI32(buf2+cffile_FolderOffset);
            file->index    = EndGetI16(buf2+cffile_FolderIndex);
            file->time     = EndGetI16(buf2+cffile_Time);
            file->date     = EndGetI16(buf2+cffile_Date);
            file->attribs  = EndGetI16(buf2+cffile_Attribs);
            file->filename = FDI_read_string(CAB(hfdi), cab->cabhf, fdici.cbCabinet);
        
            if (!file->filename) return DECR_INPUT;
        
            if (linkfile)
              linkfile->next = file;
            linkfile = file;
          }
        
        } else 
            cab = cab->next; /* advance to the next cabinet */

        /* iterate files -- if we encounter the continued file, process it --
           otherwise, jump to the label above and keep looking */

        for (file = cab->firstfile; (file); file = file->next) {
          if ((file->index & cffileCONTINUED_FROM_PREV) == cffileCONTINUED_FROM_PREV) {
            /* check to ensure a real match */
            if (strcasecmp(fi->filename, file->filename) == 0) {
              success = TRUE;
              if (PFDI_SEEK(CAB(hfdi), cab->cabhf, cab->firstfol->offset, SEEK_SET) == -1)
                return DECR_INPUT;
              break;
            }
          }
        }
        if (!success) goto tryanothercab; /* FIXME: shouldn't this trigger
                                             "Wrong Cabinet" notification? */
      }
    }

    /* decompress block */
    if ((err = CAB(decompress)(inlen, outlen, decomp_state)))
      return err;
    CAB(outlen) = outlen;
    CAB(outpos) = CAB(outbuf);
  }
  
  CAB(decomp_cab) = cab;
  return DECR_OK;
}

/***********************************************************************
 *		FDICopy (CABINET.22)
 *
 * Iterates through the files in the Cabinet file indicated by name and
 * file-location.  May chain forward to additional cabinets (typically
 * only one) if files which begin in this Cabinet are continued in another
 * cabinet.  For each file which is partially contained in this cabinet,
 * and partially contained in a prior cabinet, provides fdintPARTIAL_FILE
 * notification to the pfnfdin callback.  For each file which begins in
 * this cabinet, fdintCOPY_FILE notification is provided to the pfnfdin
 * callback, and the file is optionally decompressed and saved to disk.
 * Notification is not provided for files which are not at least partially
 * contained in the specified cabinet file.
 *
 * See below for a thorough explanation of the various notification
 * callbacks.
 *
 * PARAMS
 *   hfdi       [I] An HFDI from FDICreate
 *   pszCabinet [I] C-style string containing the filename of the cabinet
 *   pszCabPath [I] C-style string containing the file path of the cabinet
 *   flags      [I] "Decoder parameters".  Ignored.  Suggested value: 0.
 *   pfnfdin    [I] Pointer to a notification function.  See CALLBACKS below.
 *   pfnfdid    [I] Pointer to a decryption function.  Ignored.  Suggested
 *                  value: NULL.
 *   pvUser     [I] arbitrary void * value which is passed to callbacks.
 *
 * RETURNS
 *   TRUE if successful.
 *   FALSE if unsuccessful (error information is provided in the ERF structure
 *     associated with the provided decompression handle by FDICreate).
 *
 * CALLBACKS
 *
 *   Two pointers to callback functions are provided as parameters to FDICopy:
 *   pfnfdin(of type PFNFDINOTIFY), and pfnfdid (of type PFNFDIDECRYPT).  These
 *   types are as follows:
 *
 *     typedef INT_PTR (__cdecl *PFNFDINOTIFY)  ( FDINOTIFICATIONTYPE fdint,
 *                                               PFDINOTIFICATION  pfdin );
 *
 *     typedef int     (__cdecl *PFNFDIDECRYPT) ( PFDIDECRYPT pfdid );
 *
 *   You can create functions of this type using the FNFDINOTIFY() and
 *   FNFDIDECRYPT() macros, respectively.  For example:
 *
 *     FNFDINOTIFY(mycallback) {
 *       / * use variables fdint and pfdin to process notification * /
 *     }
 *
 *   The second callback, which could be used for decrypting encrypted data,
 *   is not used at all.
 *
 *   Each notification informs the user of some event which has occurred during
 *   decompression of the cabinet file; each notification is also an opportunity
 *   for the callee to abort decompression.  The information provided to the
 *   callback and the meaning of the callback's return value vary drastically
 *   across the various types of notification.  The type of notification is the
 *   fdint parameter; all other information is provided to the callback in
 *   notification-specific parts of the FDINOTIFICATION structure pointed to by
 *   pfdin.  The only part of that structure which is assigned for every callback
 *   is the pv element, which contains the arbitrary value which was passed to
 *   FDICopy in the pvUser argument (psz1 is also used each time, but its meaning
 *   is highly dependent on fdint).
 *   
 *   If you encounter unknown notifications, you should return zero if you want
 *   decompression to continue (or -1 to abort).  All strings used in the
 *   callbacks are regular C-style strings.  Detailed descriptions of each
 *   notification type follow:
 *
 *   fdintCABINET_INFO:
 * 
 *     This is the first notification provided after calling FDICopy, and provides
 *     the user with various information about the cabinet.  Note that this is
 *     called for each cabinet FDICopy opens, not just the first one.  In the
 *     structure pointed to by pfdin, psz1 contains a pointer to the name of the
 *     next cabinet file in the set after the one just loaded (if any), psz2
 *     contains a pointer to the name or "info" of the next disk, psz3
 *     contains a pointer to the file-path of the current cabinet, setID
 *     contains an arbitrary constant associated with this set of cabinet files,
 *     and iCabinet contains the numerical index of the current cabinet within
 *     that set.  Return zero, or -1 to abort.
 *
 *   fdintPARTIAL_FILE:
 *
 *     This notification is provided when FDICopy encounters a part of a file
 *     contained in this cabinet which is missing its beginning.  Files can be
 *     split across cabinets, so this is not necessarily an abnormality; it just
 *     means that the file in question begins in another cabinet.  No file
 *     corresponding to this notification is extracted from the cabinet.  In the
 *     structure pointed to by pfdin, psz1 contains a pointer to the name of the
 *     partial file, psz2 contains a pointer to the file name of the cabinet in
 *     which this file begins, and psz3 contains a pointer to the disk name or
 *     "info" of the cabinet where the file begins. Return zero, or -1 to abort.
 *
 *   fdintCOPY_FILE:
 *
 *     This notification is provided when FDICopy encounters a file which starts
 *     in the cabinet file, provided to FDICopy in pszCabinet.  (FDICopy will not
 *     look for files in cabinets after the first one).  One notification will be
 *     sent for each such file, before the file is decompressed.  By returning
 *     zero, the callback can instruct FDICopy to skip the file.  In the structure
 *     pointed to by pfdin, psz1 contains a pointer to the file's name, cb contains
 *     the size of the file (uncompressed), attribs contains the file attributes,
 *     and date and time contain the date and time of the file.  attributes, date,
 *     and time are of the 16-bit ms-dos variety.  Return -1 to abort decompression
 *     for the entire cabinet, 0 to skip just this file but continue scanning the
 *     cabinet for more files, or an FDIClose()-compatible file-handle.
 *
 *   fdintCLOSE_FILE_INFO:
 *
 *     This notification is important, don't forget to implement it.  This
 *     notification indicates that a file has been successfully uncompressed and
 *     written to disk.  Upon receipt of this notification, the callee is expected
 *     to close the file handle, to set the attributes and date/time of the
 *     closed file, and possibly to execute the file.  In the structure pointed to
 *     by pfdin, psz1 contains a pointer to the name of the file, hf will be the
 *     open file handle (close it), cb contains 1 or zero, indicating respectively
 *     that the callee should or should not execute the file, and date, time
 *     and attributes will be set as in fdintCOPY_FILE.  Bizarrely, the Cabinet SDK
 *     specifies that _A_EXEC will be xor'ed out of attributes!  wine does not do
 *     do so.  Return TRUE, or FALSE to abort decompression.
 *
 *   fdintNEXT_CABINET:
 *
 *     This notification is called when FDICopy must load in another cabinet.  This
 *     can occur when a file's data is "split" across multiple cabinets.  The
 *     callee has the opportunity to request that FDICopy look in a different file
 *     path for the specified cabinet file, by writing that data into a provided
 *     buffer (see below for more information).  This notification will be received
 *     more than once per-cabinet in the instance that FDICopy failed to find a
 *     valid cabinet at the location specified by the first per-cabinet
 *     fdintNEXT_CABINET notification.  In such instances, the fdie element of the
 *     structure pointed to by pfdin indicates the error which prevented FDICopy
 *     from proceeding successfully.  Return zero to indicate success, or -1 to
 *     indicate failure and abort FDICopy.
 *
 *     Upon receipt of this notification, the structure pointed to by pfdin will
 *     contain the following values: psz1 pointing to the name of the cabinet
 *     which FDICopy is attempting to open, psz2 pointing to the name ("info") of
 *     the next disk, psz3 pointing to the presumed file-location of the cabinet,
 *     and fdie containing either FDIERROR_NONE, or one of the following: 
 *
 *       FDIERROR_CABINET_NOT_FOUND, FDIERROR_NOT_A_CABINET,
 *       FDIERROR_UNKNOWN_CABINET_VERSION, FDIERROR_CORRUPT_CABINET,
 *       FDIERROR_BAD_COMPR_TYPE, FDIERROR_RESERVE_MISMATCH, and 
 *       FDIERROR_WRONG_CABINET.
 *
 *     The callee may choose to change the path where FDICopy will look for the
 *     cabinet after this notification.  To do so, the caller may write the new
 *     pathname to the buffer pointed to by psz3, which is 256 characters in
 *     length, including the terminating null character, before returning zero.
 *
 *   fdintENUMERATE:
 *
 *     Undocumented and unimplemented in wine, this seems to be sent each time
 *     a cabinet is opened, along with the fdintCABINET_INFO notification.  It
 *     probably has an interface similar to that of fdintCABINET_INFO; maybe this
 *     provides information about the current cabinet instead of the next one....
 *     this is just a guess, it has not been looked at closely.
 *
 * INCLUDES
 *   fdi.c
 */
BOOL __cdecl FDICopy(
        HFDI           hfdi,
        char          *pszCabinet,
        char          *pszCabPath,
        int            flags,
        PFNFDINOTIFY   pfnfdin,
        PFNFDIDECRYPT  pfnfdid,
        void          *pvUser)
{ 
  FDICABINETINFO    fdici;
  FDINOTIFICATION   fdin;
  int               cabhf, filehf, idx;
  unsigned int      i;
  char              fullpath[MAX_PATH];
  size_t            pathlen, filenamelen;
  char              emptystring = '\0';
  cab_UBYTE         buf[64];
  struct fdi_folder *fol = NULL, *linkfol = NULL; 
  struct fdi_file   *file = NULL, *linkfile = NULL;
  fdi_decomp_state _decomp_state;
  fdi_decomp_state *decomp_state = &_decomp_state;

  TRACE("(hfdi == ^%p, pszCabinet == ^%p, pszCabPath == ^%p, flags == %0d, \
        pfnfdin == ^%p, pfnfdid == ^%p, pvUser == ^%p)\n",
        hfdi, pszCabinet, pszCabPath, flags, pfnfdin, pfnfdid, pvUser);

  if (!REALLY_IS_FDI(hfdi)) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  ZeroMemory(decomp_state, sizeof(fdi_decomp_state));

  pathlen = (pszCabPath) ? strlen(pszCabPath) : 0;
  filenamelen = (pszCabinet) ? strlen(pszCabinet) : 0;

  /* slight overestimation here to save CPU cycles in the developer's brain */
  if ((pathlen + filenamelen + 3) > MAX_PATH) {
    ERR("MAX_PATH exceeded.\n");
    PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CABINET_NOT_FOUND;
    PFDI_INT(hfdi)->perf->erfType = ERROR_FILE_NOT_FOUND;
    PFDI_INT(hfdi)->perf->fError = TRUE;
    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
  }

  /* paste the path and filename together */
  idx = 0;
  if (pathlen) {
    for (i = 0; i < pathlen; i++) fullpath[idx++] = pszCabPath[i];
    if (fullpath[idx - 1] != '\\') fullpath[idx++] = '\\';
  }
  if (filenamelen) for (i = 0; i < filenamelen; i++) fullpath[idx++] = pszCabinet[i];
  fullpath[idx] = '\0';

  TRACE("full cab path/file name: %s\n", debugstr_a(fullpath));

  /* get a handle to the cabfile */
  cabhf = PFDI_OPEN(hfdi, fullpath, 32768, _S_IREAD | _S_IWRITE);
  if (cabhf == -1) {
    PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CABINET_NOT_FOUND;
    PFDI_INT(hfdi)->perf->erfType = ERROR_FILE_NOT_FOUND;
    PFDI_INT(hfdi)->perf->fError = TRUE;
    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
  }

  if (cabhf == 0) {
    ERR("PFDI_OPEN returned zero for %s.\n", fullpath);
    PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CABINET_NOT_FOUND;
    PFDI_INT(hfdi)->perf->erfType = ERROR_FILE_NOT_FOUND;
    PFDI_INT(hfdi)->perf->fError = TRUE;
    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
  }

  /* check if it's really a cabfile. Note that this doesn't implement the bug */
  if (!FDI_read_entries(hfdi, cabhf, &fdici, &(CAB(mii)))) {
    ERR("FDIIsCabinet failed.\n");
    PFDI_CLOSE(hfdi, cabhf);
    return FALSE;
  }
   
  /* cabinet notification */
  ZeroMemory(&fdin, sizeof(FDINOTIFICATION));
  fdin.setID = fdici.setID;
  fdin.iCabinet = fdici.iCabinet;
  fdin.pv = pvUser;
  fdin.psz1 = (CAB(mii).nextname) ? CAB(mii).nextname : &emptystring;
  fdin.psz2 = (CAB(mii).nextinfo) ? CAB(mii).nextinfo : &emptystring;
  fdin.psz3 = pszCabPath;

  if (((*pfnfdin)(fdintCABINET_INFO, &fdin))) {
    PFDI_INT(hfdi)->perf->erfOper = FDIERROR_USER_ABORT;
    PFDI_INT(hfdi)->perf->erfType = 0;
    PFDI_INT(hfdi)->perf->fError = TRUE;
    goto bail_and_fail;
  }

  CAB(setID) = fdici.setID;
  CAB(iCabinet) = fdici.iCabinet;

  /* read folders */
  for (i = 0; i < fdici.cFolders; i++) {
    if (PFDI_READ(hfdi, cabhf, buf, cffold_SIZEOF) != cffold_SIZEOF) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
      goto bail_and_fail;
    }

    if (CAB(mii).folder_resv > 0)
      PFDI_SEEK(hfdi, cabhf, CAB(mii).folder_resv, SEEK_CUR);

    fol = (struct fdi_folder *) PFDI_ALLOC(hfdi, sizeof(struct fdi_folder));
    if (!fol) {
      ERR("out of memory!\n");
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_ALLOC_FAIL;
      PFDI_INT(hfdi)->perf->erfType = ERROR_NOT_ENOUGH_MEMORY;
      PFDI_INT(hfdi)->perf->fError = TRUE;
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      goto bail_and_fail;
    }
    ZeroMemory(fol, sizeof(struct fdi_folder));
    if (!CAB(firstfol)) CAB(firstfol) = fol;

    fol->offset = (cab_off_t) EndGetI32(buf+cffold_DataOffset);
    fol->num_blocks = EndGetI16(buf+cffold_NumBlocks);
    fol->comp_type  = EndGetI16(buf+cffold_CompType);

    if (linkfol)
      linkfol->next = fol; 
    linkfol = fol;
  }

  /* read files */
  for (i = 0; i < fdici.cFiles; i++) {
    if (PFDI_READ(hfdi, cabhf, buf, cffile_SIZEOF) != cffile_SIZEOF) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
      goto bail_and_fail;
    }

    file = (struct fdi_file *) PFDI_ALLOC(hfdi, sizeof(struct fdi_file));
    if (!file) { 
      ERR("out of memory!\n"); 
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_ALLOC_FAIL;
      PFDI_INT(hfdi)->perf->erfType = ERROR_NOT_ENOUGH_MEMORY;
      PFDI_INT(hfdi)->perf->fError = TRUE;
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      goto bail_and_fail;
    }
    ZeroMemory(file, sizeof(struct fdi_file));
    if (!CAB(firstfile)) CAB(firstfile) = file;
      
    file->length   = EndGetI32(buf+cffile_UncompressedSize);
    file->offset   = EndGetI32(buf+cffile_FolderOffset);
    file->index    = EndGetI16(buf+cffile_FolderIndex);
    file->time     = EndGetI16(buf+cffile_Time);
    file->date     = EndGetI16(buf+cffile_Date);
    file->attribs  = EndGetI16(buf+cffile_Attribs);
    file->filename = FDI_read_string(hfdi, cabhf, fdici.cbCabinet);

    if (!file->filename) {
      PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
      PFDI_INT(hfdi)->perf->erfType = 0;
      PFDI_INT(hfdi)->perf->fError = TRUE;
      goto bail_and_fail;
    }

    if (linkfile)
      linkfile->next = file;
    linkfile = file;
  }

  for (file = CAB(firstfile); (file); file = file->next) {

    /*
     * FIXME: This implementation keeps multiple cabinet files open at once
     * when encountering a split cabinet.  It is a quirk of this implementation
     * that sometimes we decrypt the same block of data more than once, to find
     * the right starting point for a file, moving the file-pointer backwards.
     * If we kept a cache of certain file-pointer information, we could eliminate
     * that behavior... in fact I am not sure that the caching we already have
     * is not sufficient.
     * 
     * The current implementation seems to work fine in straightforward situations
     * where all the cabinet files needed for decryption are simultaneously
     * available.  But presumably, the API is supposed to support cabinets which
     * are split across multiple CDROMS; we may need to change our implementation
     * to strictly serialize it's file usage so that it opens only one cabinet
     * at a time.  Some experimentation with Windows is needed to figure out the
     * precise semantics required.  The relevant code is here and in fdi_decomp().
     */

    /* partial-file notification */
    if ((file->index & cffileCONTINUED_FROM_PREV) == cffileCONTINUED_FROM_PREV) {
      /*
       * FIXME: Need to create a Cabinet with a single file spanning multiple files
       * and perform some tests to figure out the right behavior.  The SDK says
       * FDICopy will notify the user of the filename and "disk name" (info) of
       * the cabinet where the spanning file /started/.
       *
       * That would certainly be convenient for the API-user, who could abort,
       * everything (or parallelize, if that's allowed (it is in wine)), and call
       * FDICopy again with the provided filename, so as to avoid partial file
       * notification and successfully unpack.  This task could be quite unpleasant
       * from wine's perspective: the information specifying the "start cabinet" for
       * a file is associated nowhere with the file header and is not to be found in
       * the cabinet header.  We have only the index of the cabinet wherein the folder
       * begins, which contains the file.  To find that cabinet, we must consider the
       * index of the current cabinet, and chain backwards, cabinet-by-cabinet (for
       * each cabinet refers to its "next" and "previous" cabinet only, like a linked
       * list).
       *
       * Bear in mind that, in the spirit of CABINET.DLL, we must assume that any
       * cabinet other than the active one might be at another filepath than the
       * current one, or on another CDROM. This could get rather dicey, especially
       * if we imagine parallelized access to the FDICopy API.
       *
       * The current implementation punts -- it just returns the previous cabinet and
       * it's info from the header of this cabinet.  This provides the right answer in
       * 95% of the cases; its worth checking if Microsoft cuts the same corner before
       * we "fix" it.
       */
      ZeroMemory(&fdin, sizeof(FDINOTIFICATION));
      fdin.pv = pvUser;
      fdin.psz1 = (char *)file->filename;
      fdin.psz2 = (CAB(mii).prevname) ? CAB(mii).prevname : &emptystring;
      fdin.psz3 = (CAB(mii).previnfo) ? CAB(mii).previnfo : &emptystring;

      if (((*pfnfdin)(fdintPARTIAL_FILE, &fdin))) {
        PFDI_INT(hfdi)->perf->erfOper = FDIERROR_USER_ABORT;
        PFDI_INT(hfdi)->perf->erfType = 0;
        PFDI_INT(hfdi)->perf->fError = TRUE;
        goto bail_and_fail;
      }
      /* I don't think we are supposed to decompress partial files.  This prevents it. */
      file->oppressed = TRUE;
    }
    if (file->oppressed) {
      filehf = 0;
    } else {
      ZeroMemory(&fdin, sizeof(FDINOTIFICATION));
      fdin.pv = pvUser;
      fdin.psz1 = (char *)file->filename;
      fdin.cb = file->length;
      fdin.date = file->date;
      fdin.time = file->time;
      fdin.attribs = file->attribs;
      if ((filehf = ((*pfnfdin)(fdintCOPY_FILE, &fdin))) == -1) {
        PFDI_INT(hfdi)->perf->erfOper = FDIERROR_USER_ABORT;
        PFDI_INT(hfdi)->perf->erfType = 0;
        PFDI_INT(hfdi)->perf->fError = TRUE;
        goto bail_and_fail;
      }
    }

    /* find the folder for this file if necc. */
    if (filehf) {
      int i2;

      fol = CAB(firstfol);
      if ((file->index & cffileCONTINUED_TO_NEXT) == cffileCONTINUED_TO_NEXT) {
        /* pick the last folder */
        while (fol->next) fol = fol->next;
      } else {
        for (i2 = 0; (i2 < file->index); i2++)
          if (fol->next) /* bug resistance, should always be true */
            fol = fol->next;
      }
    }

    if (filehf) {
      cab_UWORD comptype = fol->comp_type;
      int ct1 = comptype & cffoldCOMPTYPE_MASK;
      int ct2 = CAB(current) ? (CAB(current)->comp_type & cffoldCOMPTYPE_MASK) : 0;
      int err = 0;

      TRACE("Extracting file %s as requested by callee.\n", debugstr_a(file->filename));

      /* set up decomp_state */
      CAB(hfdi) = hfdi;
      CAB(filehf) = filehf;
      CAB(cabhf) = cabhf;

      /* Was there a change of folder?  Compression type?  Did we somehow go backwards? */
      if ((ct1 != ct2) || (CAB(current) != fol) || (file->offset < CAB(offset))) {

        TRACE("Resetting folder for file %s.\n", debugstr_a(file->filename));

        /* free stuff for the old decompresser */
        switch (ct2) {
        case cffoldCOMPTYPE_LZX:
          if (LZX(window)) {
            PFDI_FREE(hfdi, LZX(window));
            LZX(window) = NULL;
          }
          break;
        case cffoldCOMPTYPE_QUANTUM:
          if (QTM(window)) {
            PFDI_FREE(hfdi, QTM(window));
            QTM(window) = NULL;
          }
          break;
        }

        CAB(decomp_cab) = NULL;
        PFDI_SEEK(CAB(hfdi), CAB(cabhf), fol->offset, SEEK_SET);
        CAB(offset) = 0;
        CAB(outlen) = 0;

        /* initialize the new decompresser */
        switch (ct1) {
        case cffoldCOMPTYPE_NONE:
          CAB(decompress) = NONEfdi_decomp;
          break;
        case cffoldCOMPTYPE_MSZIP:
          CAB(decompress) = ZIPfdi_decomp;
          break;
        case cffoldCOMPTYPE_QUANTUM:
          CAB(decompress) = QTMfdi_decomp;
          err = QTMfdi_init((comptype >> 8) & 0x1f, (comptype >> 4) & 0xF, decomp_state);
          break;
        case cffoldCOMPTYPE_LZX:
          CAB(decompress) = LZXfdi_decomp;
          err = LZXfdi_init((comptype >> 8) & 0x1f, decomp_state);
          break;
        default:
          err = DECR_DATAFORMAT;
        }
      }

      CAB(current) = fol;

      switch (err) {
        case DECR_OK:
          break;
        case DECR_NOMEMORY:
          PFDI_INT(hfdi)->perf->erfOper = FDIERROR_ALLOC_FAIL;
          PFDI_INT(hfdi)->perf->erfType = ERROR_NOT_ENOUGH_MEMORY;
          PFDI_INT(hfdi)->perf->fError = TRUE;
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);
          goto bail_and_fail;
        default:
          PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
          PFDI_INT(hfdi)->perf->erfOper = 0;
          PFDI_INT(hfdi)->perf->fError = TRUE;
          goto bail_and_fail;
      }

      if (file->offset > CAB(offset)) {
        /* decode bytes and send them to /dev/null */
        switch ((err = fdi_decomp(file, 0, decomp_state, pszCabPath, pfnfdin, pvUser))) {
          case DECR_OK:
            break;
          case DECR_USERABORT:
            PFDI_INT(hfdi)->perf->erfOper = FDIERROR_USER_ABORT;
            PFDI_INT(hfdi)->perf->erfType = 0;
            PFDI_INT(hfdi)->perf->fError = TRUE;
            goto bail_and_fail;
          case DECR_NOMEMORY:
            PFDI_INT(hfdi)->perf->erfOper = FDIERROR_ALLOC_FAIL;
            PFDI_INT(hfdi)->perf->erfType = ERROR_NOT_ENOUGH_MEMORY;
            PFDI_INT(hfdi)->perf->fError = TRUE;
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto bail_and_fail;
          default:
            PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
            PFDI_INT(hfdi)->perf->erfOper = 0;
            PFDI_INT(hfdi)->perf->fError = TRUE;
            goto bail_and_fail;
        }
        CAB(offset) = file->offset;
      }

      /* now do the actual decompression */
      err = fdi_decomp(file, 1, decomp_state, pszCabPath, pfnfdin, pvUser);
      if (err) CAB(current) = NULL; else CAB(offset) += file->length;

      switch (err) {
        case DECR_OK:
          break;
        case DECR_USERABORT:
          PFDI_INT(hfdi)->perf->erfOper = FDIERROR_USER_ABORT;
          PFDI_INT(hfdi)->perf->erfType = 0;
          PFDI_INT(hfdi)->perf->fError = TRUE;
          goto bail_and_fail;
        case DECR_NOMEMORY:
          PFDI_INT(hfdi)->perf->erfOper = FDIERROR_ALLOC_FAIL;
          PFDI_INT(hfdi)->perf->erfType = ERROR_NOT_ENOUGH_MEMORY;
          PFDI_INT(hfdi)->perf->fError = TRUE;
          SetLastError(ERROR_NOT_ENOUGH_MEMORY);
          goto bail_and_fail;
        default:
          PFDI_INT(hfdi)->perf->erfOper = FDIERROR_CORRUPT_CABINET;
          PFDI_INT(hfdi)->perf->erfOper = 0;
          PFDI_INT(hfdi)->perf->fError = TRUE;
          goto bail_and_fail;
      }

      /* fdintCLOSE_FILE_INFO notification */
      ZeroMemory(&fdin, sizeof(FDINOTIFICATION));
      fdin.pv = pvUser;
      fdin.psz1 = (char *)file->filename;
      fdin.hf = filehf;
      fdin.cb = (file->attribs & cffile_A_EXEC) ? TRUE : FALSE; /* FIXME: is that right? */
      fdin.date = file->date;
      fdin.time = file->time;
      fdin.attribs = file->attribs; /* FIXME: filter _A_EXEC? */
      err = ((*pfnfdin)(fdintCLOSE_FILE_INFO, &fdin));
      if (err == FALSE || err == -1) {
        /*
         * SDK states that even though they indicated failure,
         * we are not supposed to try and close the file, so we
         * just treat this like all the others
         */
        PFDI_INT(hfdi)->perf->erfOper = FDIERROR_USER_ABORT;
        PFDI_INT(hfdi)->perf->erfType = 0;
        PFDI_INT(hfdi)->perf->fError = TRUE;
        goto bail_and_fail;
      }
    }
  }

  /* free decompression temps */
  switch (fol->comp_type & cffoldCOMPTYPE_MASK) {
  case cffoldCOMPTYPE_LZX:
    if (LZX(window)) {
      PFDI_FREE(hfdi, LZX(window));
      LZX(window) = NULL;
    }
    break;
  case cffoldCOMPTYPE_QUANTUM:
    if (QTM(window)) {
      PFDI_FREE(hfdi, QTM(window));
      QTM(window) = NULL;
    }
    break;
  }

  while (decomp_state) {
    fdi_decomp_state *prev_fds;

    PFDI_CLOSE(hfdi, CAB(cabhf));

    /* free the storage remembered by mii */
    if (CAB(mii).nextname) PFDI_FREE(hfdi, CAB(mii).nextname);
    if (CAB(mii).nextinfo) PFDI_FREE(hfdi, CAB(mii).nextinfo);
    if (CAB(mii).prevname) PFDI_FREE(hfdi, CAB(mii).prevname);
    if (CAB(mii).previnfo) PFDI_FREE(hfdi, CAB(mii).previnfo);

    while (CAB(firstfol)) {
      fol = CAB(firstfol);
      CAB(firstfol) = CAB(firstfol)->next;
      PFDI_FREE(hfdi, fol);
    }
    while (CAB(firstfile)) {
      file = CAB(firstfile);
      if (file->filename) PFDI_FREE(hfdi, (void *)file->filename);
      CAB(firstfile) = CAB(firstfile)->next;
      PFDI_FREE(hfdi, file);
    }
    prev_fds = decomp_state;
    decomp_state = CAB(next);
    if (prev_fds != &_decomp_state)
      PFDI_FREE(hfdi, prev_fds);
  }
 
  return TRUE;

  bail_and_fail: /* here we free ram before error returns */

  /* free decompression temps */
  switch (fol->comp_type & cffoldCOMPTYPE_MASK) {
  case cffoldCOMPTYPE_LZX:
    if (LZX(window)) {
      PFDI_FREE(hfdi, LZX(window));
      LZX(window) = NULL;
    }
    break;
  case cffoldCOMPTYPE_QUANTUM:
    if (QTM(window)) {
      PFDI_FREE(hfdi, QTM(window));
      QTM(window) = NULL;
    }
    break;
  }

  while (decomp_state) {
    fdi_decomp_state *prev_fds;

    PFDI_CLOSE(hfdi, CAB(cabhf));

    /* free the storage remembered by mii */
    if (CAB(mii).nextname) PFDI_FREE(hfdi, CAB(mii).nextname);
    if (CAB(mii).nextinfo) PFDI_FREE(hfdi, CAB(mii).nextinfo);
    if (CAB(mii).prevname) PFDI_FREE(hfdi, CAB(mii).prevname);
    if (CAB(mii).previnfo) PFDI_FREE(hfdi, CAB(mii).previnfo);

    while (CAB(firstfol)) {
      fol = CAB(firstfol);
      CAB(firstfol) = CAB(firstfol)->next;
      PFDI_FREE(hfdi, fol);
    }
    while (CAB(firstfile)) {
      file = CAB(firstfile);
      if (file->filename) PFDI_FREE(hfdi, (void *)file->filename);
      CAB(firstfile) = CAB(firstfile)->next;
      PFDI_FREE(hfdi, file);
    }
    prev_fds = decomp_state;
    decomp_state = CAB(next);
    if (prev_fds != &_decomp_state)
      PFDI_FREE(hfdi, prev_fds);
  }

  return FALSE;
}

/***********************************************************************
 *		FDIDestroy (CABINET.23)
 *
 * Frees a handle created by FDICreate.  Do /not/ call this in the middle
 * of FDICopy.  Only reason for failure would be an invalid handle.
 * 
 * PARAMS
 *   hfdi [I] The HFDI to free
 *
 * RETURNS
 *   TRUE for success
 *   FALSE for failure
 */
BOOL __cdecl FDIDestroy(HFDI hfdi)
{
  TRACE("(hfdi == ^%p)\n", hfdi);
  if (REALLY_IS_FDI(hfdi)) {
    PFDI_INT(hfdi)->FDI_Intmagic = 0; /* paranoia */
    PFDI_FREE(hfdi, hfdi); /* confusing, but correct */
    return TRUE;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
}

/***********************************************************************
 *		FDITruncateCabinet (CABINET.24)
 *
 * Undocumented and unimplemented.
 */
BOOL __cdecl FDITruncateCabinet(
	HFDI    hfdi,
	char   *pszCabinetName,
	USHORT  iFolderToDelete)
{
  FIXME("(hfdi == ^%p, pszCabinetName == %s, iFolderToDelete == %hu): stub\n",
    hfdi, debugstr_a(pszCabinetName), iFolderToDelete);

  if (!REALLY_IS_FDI(hfdi)) {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }

  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}
