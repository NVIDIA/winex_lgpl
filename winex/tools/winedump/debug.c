/*
 * Made after:
 *	CVDump - Parses through a Visual Studio .DBG file in CodeView 4 format
 * 	and dumps the info to STDOUT in a human-readable format
 *
 * 	Copyright 2000 John R. Sheets
 */

#include "config.h"
#include "wine/port.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <fcntl.h>

#include "winnt.h"
#include "winedump.h"
#include "pe.h"
#include "cvinclude.h"

/*
 * .DBG File Layout:
 *
 * IMAGE_SEPARATE_DEBUG_HEADER
 * IMAGE_SECTION_HEADER[]
 * IMAGE_DEBUG_DIRECTORY[]
 * OMFSignature
 * debug data (typical example)
 *   - IMAGE_DEBUG_TYPE_MISC
 *   - IMAGE_DEBUG_TYPE_FPO
 *   - IMAGE_DEBUG_TYPE_CODEVIEW
 * OMFDirHeader
 * OMFDirEntry[]
 */

/*
 * Descriptions:
 *
 * (hdr)  IMAGE_SEPARATE_DEBUG_HEADER - .DBG-specific file header; holds info that
 *        applies to the file as a whole, including # of COFF sections, file offsets, etc.
 * (hdr)  IMAGE_SECTION_HEADER - list of COFF sections copied verbatim from .EXE;
 *        although this directory contains file offsets, these offsets are meaningless
 *        in the context of the .DBG file, because only the section headers are copied
 *        to the .DBG file...not the binary data it points to.
 * (hdr)  IMAGE_DEBUG_DIRECTORY - list of different formats of debug info contained in file
 *        (see IMAGE_DEBUG_TYPE_* descriptions below); tells where each section starts
 * (hdr)  OMFSignature (CV) - Contains "NBxx" signature, plus file offset telling how far
 *        into the IMAGE_DEBUG_TYPE_CODEVIEW section the OMFDirHeader and OMFDirEntry's sit
 * (data) IMAGE_DEBUG_TYPE_MISC - usually holds name of original .EXE file
 * (data) IMAGE_DEBUG_TYPE_FPO - Frame Pointer Optimization data; used for dealing with
 *        optimized stack frames (optional)
 * (data) IMAGE_DEBUG_TYPE_CODEVIEW - *** THE GOOD STUFF ***
 *        This block of data contains all the symbol tables, line number info, etc.,
 *        that the Visual C++ debugger needs.
 * (hdr)  OMFDirHeader (CV) -
 * (hdr)  OMFDirEntry (CV) - list of subsections within CodeView debug data section
 */

/*
 * The .DBG file typically has three arrays of directory entries, which tell
 * the OS or debugger where in the file to look for the actual data
 *
 * IMAGE_SECTION_HEADER - number of entries determined by:
 *    (IMAGE_SEPARATE_DEBUG_HEADER.NumberOfSections)
 *
 * IMAGE_DEBUG_DIRECTORY - number of entries determined by:
 *    (IMAGE_SEPARATE_DEBUG_HEADER.DebugDirectorySize / sizeof (IMAGE_DEBUG_DIRECTORY))
 *
 * OMFDirEntry - number of entries determined by:
 *    (OMFDirHeader.cDir)
 */

static	void*		cv_base /* = 0 */;

static int dump_cv_sst_module(OMFDirEntry* omfde)
{
    OMFModule*	module;
    OMFSegDesc*	segDesc;
    int		i;

    module = PRD(Offset(cv_base) + omfde->lfo, sizeof(OMFModule));
    if (!module) {printf("Can't get the OMF-Module, aborting\n"); return FALSE;}

    printf("    olvNumber:          %u\n", module->ovlNumber);
    printf("    iLib:               %u\n", module->iLib);
    printf("    cSeg:               %u\n", module->cSeg);
    printf("    Style:              %c%c\n", module->Style[0], module->Style[1]);
    printf("    Name:               %.*s\n",
	   *(BYTE*)((char*)(module + 1) + sizeof(OMFSegDesc) * module->cSeg),
	   (char*)(module + 1) + sizeof(OMFSegDesc) * module->cSeg + 1);

    segDesc = PRD(Offset(module + 1), sizeof(OMFSegDesc) * module->cSeg);
    if (!segDesc) {printf("Can't get the OMF-SegDesc, aborting\n"); return FALSE;}

    for (i = 0; i < module->cSeg; i++)
    {
	printf ("      segment #%2d: offset = [0x%8lx], size = [0x%8lx]\n",
		segDesc->Seg, segDesc->Off, segDesc->cbSeg);
	segDesc++;
    }
    return TRUE;
}

static int dump_cv_sst_global_pub(OMFDirEntry* omfde)
{
    long	fileoffset;
    OMFSymHash* header;
    BYTE*	symbols;
    BYTE*	curpos;
    PUBSYM32*	sym;
    unsigned 	symlen;
    int		recordlen;
    char 	nametmp[256];

    fileoffset = Offset(cv_base) + omfde->lfo;
    printf ("    GlobalPub section starts at file offset 0x%lx\n", fileoffset);
    printf ("    Symbol table starts at 0x%lx\n", fileoffset + sizeof (OMFSymHash));

    printf ("\n                           ----- Begin Symbol Table -----\n");
    printf ("      (type)      (symbol name)                 (offset)      (len) (seg) (ind)\n");

    header = PRD(fileoffset, sizeof(OMFSymHash));
    if (!header) {printf("Can't get OMF-SymHash, aborting\n");return FALSE;}

    symbols = PRD(fileoffset + sizeof(OMFSymHash), header->cbSymbol);
    if (!symbols) {printf("Can't OMF-SymHash details, aborting\n"); return FALSE;}

    /* We don't know how many symbols are in this block of memory...only what
     * the total size of the block is.  Because the symbol's name is tacked
     * on to the end of the PUBSYM32 struct, each symbol may take up a different
     * # of bytes.  This makes it harder to parse through the symbol table,
     * since we won't know the exact location of the following symbol until we've
     * already parsed the current one.
     */
    for (curpos = symbols; curpos < symbols + header->cbSymbol; curpos += recordlen)
    {
	/* Point to the next PUBSYM32 in the table.
	 */
	sym = (PUBSYM32*)curpos;

	if (sym->reclen < sizeof(PUBSYM32)) break;

	symlen = sym->reclen - sizeof(PUBSYM32) + 1;
	if (symlen > sizeof(nametmp)) {printf("\nsqueeze%d\n", symlen);symlen = sizeof(nametmp) - 1;}

	memcpy(nametmp, curpos + sizeof (PUBSYM32) + 1, symlen);
	nametmp[symlen] = '\0';

	printf ("      0x%04x  %-30.30s  [0x%8lx]  [0x%4x]  %d     %ld\n",
		sym->rectyp, nametmp, sym->off, sym->reclen, sym->seg, sym->typind);

	/* The entire record is null-padded to the nearest 4-byte
	 * boundary, so we must do a little extra math to keep things straight.
	 */
	recordlen = (sym->reclen + 3) & ~3;
    }

    return TRUE;
}

static int dump_cv_sst_global_sym(OMFDirEntry* omfde)
{
    /*** NOT YET IMPLEMENTED ***/
    return TRUE;
}

static int dump_cv_sst_static_sym(OMFDirEntry* omfde)
{
    /*** NOT YET IMPLEMENTED ***/
    return TRUE;
}

static int dump_cv_sst_libraries(OMFDirEntry* omfde)
{
    /*** NOT YET IMPLEMENTED ***/
    return TRUE;
}

static int dump_cv_sst_global_types(OMFDirEntry* omfde)
{
    /*** NOT YET IMPLEMENTED ***/
    return TRUE;
}

static int dump_cv_sst_seg_map(OMFDirEntry* omfde)
{
    OMFSegMap*		segMap;
    OMFSegMapDesc*	segMapDesc;
    int		i;

    segMap = PRD(Offset(cv_base) + omfde->lfo, sizeof(OMFSegMap));
    if (!segMap) {printf("Can't get SegMap, aborting\n");return FALSE;}

    printf("    cSeg:          %u\n", segMap->cSeg);
    printf("    cSegLog:       %u\n", segMap->cSegLog);

    segMapDesc = PRD(Offset(segMap + 1), segMap->cSeg * sizeof(OMFSegDesc));
    if (!segMapDesc) {printf("Can't get SegDescr array, aborting\n");return FALSE;}

    for (i = 0; i < segMap->cSeg; i++)
    {
	printf("    SegDescr #%2d\n", i + 1);
	printf("      flags:         %04X\n", segMapDesc[i].flags);
	printf("      ovl:           %u\n", segMapDesc[i].ovl);
	printf("      group:         %u\n", segMapDesc[i].group);
	printf("      frame:         %u\n", segMapDesc[i].frame);
	printf("      iSegName:      %u\n", segMapDesc[i].iSegName);
	printf("      iClassName:    %u\n", segMapDesc[i].iClassName);
	printf("      offset:        %lu\n", segMapDesc[i].offset);
	printf("      cbSeg:         %lu\n", segMapDesc[i].cbSeg);
    }

    return TRUE;
}

static int dump_cv_sst_file_index(OMFDirEntry* omfde)
{
    /*** NOT YET IMPLEMENTED ***/
    return TRUE;
}

static int dump_cv_sst_src_module(OMFDirEntry* omfde)
{
    int 		i, j;
    BYTE*		rawdata;
    unsigned long*	seg_info_dw;
    unsigned short*	seg_info_w;
    unsigned		ofs;
    OMFSourceModule*	sourceModule;
    OMFSourceFile*	sourceFile;
    OMFSourceLine*	sourceLine;

    rawdata = PRD(Offset(cv_base) + omfde->lfo, omfde->cb);
    if (!rawdata) {printf("Can't get srcModule subsection details, aborting\n");return FALSE;}

    /* FIXME: check ptr validity */
    sourceModule = (void*)rawdata;
    printf ("    Module table: Found %d file(s) and %d segment(s)\n",
	    sourceModule->cFile, sourceModule->cSeg);
    for (i = 0; i < sourceModule->cFile; i++)
    {
	printf ("      File #%2d begins at an offset of 0x%lx in this section\n",
		i + 1, sourceModule->baseSrcFile[i]);
    }

    /* FIXME: check ptr validity */
    seg_info_dw = (void*)((char*)(sourceModule + 1) +
			  sizeof(unsigned long) * (sourceModule->cFile - 1));
    seg_info_w = (unsigned short*)(&seg_info_dw[sourceModule->cSeg * 2]);
    for (i = 0; i <  sourceModule->cSeg; i++)
    {
	printf ("      Segment #%2d start = 0x%lx, end = 0x%lx, seg index = %u\n",
		i + 1, seg_info_dw[i * 2], seg_info_dw[(i * 2) + 1],
		seg_info_w[i]);
    }
    ofs = sizeof(OMFSourceModule) + sizeof(unsigned long) * (sourceModule->cFile - 1) +
	sourceModule->cSeg * (2 * sizeof(unsigned long) + sizeof(unsigned short));
    ofs = (ofs + 3) & ~3;

    /* the OMFSourceFile is quite unpleasant to use:
     * we have first:
     * 	unsigned short	number of segments
     *	unsigned short	reservered
     *	unsigned long	baseSrcLn[# segments]
     *  unsigned long	offset[2 * #segments]
     *				odd indices are start offsets
     *				even indices are end offsets
     * 	unsigned char	string length for file name
     *	char		file name (length is previous field)
     */
    /* FIXME: check ptr validity */
    sourceFile = (void*)(rawdata + ofs);
    seg_info_dw = (void*)((char*)sourceFile + 2 * sizeof(unsigned short) +
			  sourceFile->cSeg * sizeof(unsigned long));

    ofs += 2 * sizeof(unsigned short) + 3 * sourceFile->cSeg * sizeof(unsigned long);

    printf("    File table: %.*s\n",
	   *(BYTE*)((char*)sourceModule + ofs), (char*)sourceModule + ofs + 1);

    for (i = 0; i < sourceFile->cSeg; i++)
    {
	printf ("      Segment #%2d start = 0x%lx, end = 0x%lx, offset = 0x%lx\n",
		i + 1, seg_info_dw[i * 2], seg_info_dw[(i * 2) + 1], sourceFile->baseSrcLn[i]);
    }
    /* add file name length */
    ofs += *(BYTE*)((char*)sourceModule + ofs) + 1;
    ofs = (ofs + 3) & ~3;

    for (i = 0; i < sourceModule->cSeg; i++)
    {
	sourceLine = (void*)(rawdata + ofs);
	seg_info_dw = (void*)((char*)sourceLine + 2 * sizeof(unsigned short));
	seg_info_w = (void*)(&seg_info_dw[sourceLine->cLnOff]);

	printf ("    Line table #%2d: Found %d line numbers for segment index %d\n",
		i, sourceLine->cLnOff, sourceLine->Seg);

	for (j = 0; j < sourceLine->cLnOff; j++)
	{
	    printf ("      Pair #%2d: offset = [0x%8lx], linenumber = %d\n",
		    j + 1, seg_info_dw[j], seg_info_w[j]);
	}
	ofs += 2 * sizeof(unsigned short) +
	    sourceLine->cLnOff * (sizeof(unsigned long) + sizeof(unsigned short));
	ofs = (ofs + 3) & ~3;
    }

    return TRUE;
}

static int dump_cv_sst_align_sym(OMFDirEntry* omfde)
{
    /*** NOT YET IMPLEMENTED ***/

    return TRUE;
}

static void dump_codeview_all_modules(OMFDirHeader *omfdh)
{
    unsigned 		i;
    OMFDirEntry	       *dirEntry;
    const char*		str;

    if (!omfdh || !omfdh->cDir) return;

    dirEntry = PRD(Offset(omfdh + 1), omfdh->cDir * sizeof(OMFDirEntry));
    if (!dirEntry) {printf("Can't read DirEntry array, aborting\n"); return;}

    for (i = 0; i < omfdh->cDir; i++)
    {
	switch (dirEntry[i].SubSection)
	{
	case sstModule:		str = "sstModule";	break;
	case sstAlignSym:	str = "sstAlignSym";	break;
	case sstSrcModule:	str = "sstSrcModule";	break;
	case sstLibraries:	str = "sstLibraries";	break;
	case sstGlobalSym:	str = "sstGlobalSym";	break;
	case sstGlobalPub:	str = "sstGlobalPub";	break;
	case sstGlobalTypes:	str = "sstGlobalTypes";	break;
	case sstSegMap:		str = "sstSegMap";	break;
	case sstFileIndex:	str = "sstFileIndex";	break;
	case sstStaticSym:	str = "sstStaticSym";	break;
	default:		str = "<undefined>";	break;
	}
	printf("Module #%2d (%p)\n", i + 1, &dirEntry[i]);
	printf("  SubSection:            %04X (%s)\n", dirEntry[i].SubSection, str);
	printf("  iMod:                  %d\n", dirEntry[i].iMod);
	printf("  lfo:                   %ld\n", dirEntry[i].lfo);
	printf("  cb:                    %lu\n", dirEntry[i].cb);

	switch (dirEntry[i].SubSection)
	{
	case sstModule:		dump_cv_sst_module(&dirEntry[i]);	break;
	case sstAlignSym:	dump_cv_sst_align_sym(&dirEntry[i]);	break;
	case sstSrcModule:	dump_cv_sst_src_module(&dirEntry[i]);	break;
	case sstLibraries:	dump_cv_sst_libraries(&dirEntry[i]);	break;
	case sstGlobalSym:	dump_cv_sst_global_sym(&dirEntry[i]);	break;
	case sstGlobalPub:	dump_cv_sst_global_pub(&dirEntry[i]);	break;
	case sstGlobalTypes:	dump_cv_sst_global_types(&dirEntry[i]);	break;
	case sstSegMap:		dump_cv_sst_seg_map(&dirEntry[i]);	break;
	case sstFileIndex:	dump_cv_sst_file_index(&dirEntry[i]);	break;
	case sstStaticSym:	dump_cv_sst_static_sym(&dirEntry[i]);	break;
	default:		printf("unsupported type %x\n", dirEntry[i].SubSection); break;
	}
	printf("\n");
    }

    return;
}

static void dump_codeview_headers(unsigned long base, unsigned long len)
{
    OMFDirHeader	*dirHeader;
    OMFSignature	*signature;
    OMFDirEntry		*dirEntry;
    unsigned		i;
    int modulecount = 0, alignsymcount = 0, srcmodulecount = 0, librariescount = 0;
    int globalsymcount = 0, globalpubcount = 0, globaltypescount = 0;
    int segmapcount = 0, fileindexcount = 0, staticsymcount = 0;

    cv_base = PRD(base, len);
    if (!cv_base) {printf("Can't get full debug content, aborting\n");return;}

    signature = cv_base;

    printf("    CodeView Data\n");

    printf("      Signature:         %.4s\n", signature->Signature);
    printf("      Filepos:           0x%08lX\n", signature->filepos);

    if (memcmp(signature->Signature, "NB10", 4) == 0)
    {
	struct {DWORD TimeStamp; DWORD  Dunno; char Name[1];}* pdb_data;
	pdb_data = (void*)(signature + 1);

	printf("        TimeStamp:            %08lX (%s)\n",
	       pdb_data->TimeStamp, get_time_str(pdb_data->TimeStamp));
	printf("        Dunno:                %08lX\n", pdb_data->Dunno);
	printf("        Filename:             %s\n", pdb_data->Name);
	return;
    }

    if (memcmp(signature->Signature, "NB09", 4) != 0 && memcmp(signature->Signature, "NB11", 4) != 0)
    {
	printf("Unsupported signature, aborting\n");
	return;
    }

    dirHeader = PRD(Offset(cv_base) + signature->filepos, sizeof(OMFDirHeader));
    if (!dirHeader) {printf("Can't get debug header, aborting\n"); return;}

    printf("      Size of header:    0x%4X\n", dirHeader->cbDirHeader);
    printf("      Size per entry:    0x%4X\n", dirHeader->cbDirEntry);
    printf("      # of entries:      0x%8lX (%ld)\n", dirHeader->cDir, dirHeader->cDir);
    printf("      Offset to NextDir: 0x%8lX\n", dirHeader->lfoNextDir);
    printf("      Flags:             0x%8lX\n", dirHeader->flags);

    if (!dirHeader->cDir) return;

    dirEntry = PRD(Offset(dirHeader + 1), sizeof(OMFDirEntry) * dirHeader->cDir);
    if (!dirEntry) {printf("Can't get DirEntry array, aborting\n");return;}

    for (i = 0; i < dirHeader->cDir; i++)
    {
	switch (dirEntry[i].SubSection)
	{
	case sstModule:		modulecount++;		break;
	case sstAlignSym:	alignsymcount++;	break;
	case sstSrcModule:	srcmodulecount++;	break;
	case sstLibraries:	librariescount++;	break;
	case sstGlobalSym:	globalsymcount++;	break;
	case sstGlobalPub:	globalpubcount++;	break;
	case sstGlobalTypes:	globaltypescount++;	break;
	case sstSegMap:		segmapcount++;		break;
	case sstFileIndex:	fileindexcount++;	break;
	case sstStaticSym:	staticsymcount++;	break;
	}
    }

    /* This one has to be > 0
     */
    printf ("\nFound: %d sstModule subsections\n", modulecount);

    if (alignsymcount > 0)    printf ("       %d sstAlignSym subsections\n", alignsymcount);
    if (srcmodulecount > 0)   printf ("       %d sstSrcModule subsections\n", srcmodulecount);
    if (librariescount > 0)   printf ("       %d sstLibraries subsections\n", librariescount);
    if (globalsymcount > 0)   printf ("       %d sstGlobalSym subsections\n", globalsymcount);
    if (globalpubcount > 0)   printf ("       %d sstGlobalPub subsections\n", globalpubcount);
    if (globaltypescount > 0) printf ("       %d sstGlobalTypes subsections\n", globaltypescount);
    if (segmapcount > 0)      printf ("       %d sstSegMap subsections\n", segmapcount);
    if (fileindexcount > 0)   printf ("       %d sstFileIndex subsections\n", fileindexcount);
    if (staticsymcount > 0)   printf ("       %d sstStaticSym subsections\n", staticsymcount);

    dump_codeview_all_modules(dirHeader);
}

void	dump_codeview(unsigned long base, unsigned long len)
{
    dump_codeview_headers(base, len);
}

