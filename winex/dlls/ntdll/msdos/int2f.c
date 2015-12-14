/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/*
 * DOS interrupt 2fh handler
 *
 *  Cdrom - device driver emulation - Audio features.
 * 	(c) 1998 Petr Tomasek <tomasek@etf.cuni.cz>
 *	(c) 1999,2002 Eric Pouech
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"

#include <string.h>
#include <errno.h>
#include "wine/winbase16.h"
#include "miscemu.h"
#include "wine/module.h"
/* #define DEBUG_INT */
#include "wine/hardware.h"
#include "wine/debug.h"
#include "winioctl.h"
#include "ntddstor.h"
#include "ntddcdrm.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

/* base WPROCS.DLL ordinal number for VxDs */
#define VXD_BASE 400

static void do_int2f_16( CONTEXT86 *context );
static void MSCDEX_Handler( CONTEXT86 *context );

/**********************************************************************
 *	    INT_Int2fHandler (WPROCS.147)
 *
 * Handler for int 2fh (multiplex).
 */
void WINAPI INT_Int2fHandler( CONTEXT86 *context )
{
    TRACE("Subfunction 0x%X\n", AX_reg(context));

    switch(AH_reg(context))
    {
    case 0x10:
        AL_reg(context) = 0xff; /* share is installed */
        break;

    case 0x11:  /* Network Redirector / IFSFUNC */
        switch (LOBYTE(context->Eax))
        {
        case 0x00:  /* Install check */
            /* not installed */
            break;
        case 0x80:  /* Enhanced services - Install check */
            /* not installed */
            break;
        default:
	    INT_BARF( context, 0x2f );
            break;
        }
        break;

    case 0x12:
        switch (LOBYTE(context->Eax))
        {
        case 0x2e: /* get or set DOS error table address */
            switch (DL_reg(context))
            {
            /* Four tables: even commands are 'get', odd are 'set' */
            /* DOS 5.0+ ignores "set" commands */
            case 0x01:
            case 0x03:
            case 0x05:
            case 0x07:
            case 0x09:
                break;
            /* Instead of having a message table in DOS-space, */
            /* we can use a special case for MS-DOS to force   */
            /* the secondary interface.			       */
            case 0x00:
            case 0x02:
            case 0x04:
            case 0x06:
                context->SegEs = 0x0001;
                DI_reg(context) = 0x0000;
                break;
            case 0x08:
                FIXME("No real-mode handler for errors yet! (bye!)\n");
                break;
            default:
                INT_BARF(context, 0x2f);
            }
            break;
        default:
           INT_BARF(context, 0x2f);
        }
        break;

    case 0x15: /* mscdex */
        MSCDEX_Handler(context);
        break;

    case 0x16:
        do_int2f_16( context );
        break;

    case 0x1a: /* ANSI.SYS / AVATAR.SYS Install Check */
        /* Not supported yet, do nothing */
        break;

    case 0x43:
#if 1
	switch (LOBYTE(context->Eax))
	{
	case 0x00:   /* XMS v2+ installation check */
	    WARN("XMS is not fully implemented\n");
	    AL_reg(context) = 0x80;
	    break;
	case 0x10:   /* XMS v2+ get driver address */
	{
            context->SegEs = DOSMEM_xms_seg;
            BX_reg(context) = 0;
            break;
	}
	default:
	    INT_BARF( context, 0x2f );
	}
#else
    	FIXME("check for XMS (not supported)\n");
	AL_reg(context) = 0x42; /* != 0x80 */
#endif
    	break;

    case 0x45:
       switch (LOBYTE(context->Eax))
       {
       case 0x00:
       case 0x01:
       case 0x02:
       case 0x03:
       case 0x04:
       case 0x05:
       case 0x06:
       case 0x07:
       case 0x08:
           /* Microsoft Profiler - not installed */
           break;
       default:
            INT_BARF( context, 0x2f );
       }
       break;

    case 0x4a:
        switch(LOBYTE(context->Eax))
        {
	case 0x10:  /* smartdrv */
	    break;  /* not installed */
        case 0x11:  /* dblspace */
            break;  /* not installed */
        case 0x12:  /* realtime compression interface */
            break;  /* not installed */
        case 0x32:  /* patch IO.SYS (???) */
            break;  /* we have no IO.SYS, so we can't patch it :-/ */
        default:
            INT_BARF( context, 0x2f );
        }
        break;
    case 0x4b:
	switch(LOBYTE(context->Eax))
	{
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	    FIXME("Task Switcher - not implemented\n");
	    break;
	default:
	    INT_BARF( context, 0x2f );
	}
	break;
    case 0x56:  /* INTERLNK */
	switch(LOBYTE(context->Eax))
	{
	case 0x01:  /* check if redirected drive */
	    AL_reg(context) = 0; /* not redirected */
	    break;
	default:
	    INT_BARF( context, 0x2f );
	}
	break;
    case 0x7a:  /* NOVELL NetWare */
        switch (LOBYTE(context->Eax))
        {
	case 0x0:  /* Low-level Netware installation check AL=0 not installed.*/
            AL_reg(context) = 0;
            break;
        case 0x20:  /* Get VLM Call Address */
            /* return nothing -> NetWare not installed */
            break;
        default:
	    INT_BARF( context, 0x2f );
            break;
        }
        break;
    case 0xb7:  /* append */
        SET_LOBYTE(context->Eax, 0); /* not installed */
        break;
    case 0xb8:  /* network */
        switch (LOBYTE(context->Eax))
        {
        case 0x00:  /* Install check */
            /* not installed */
            break;
        default:
	    INT_BARF( context, 0x2f );
            break;
        }
        break;
    case 0xbd:  /* some Novell network install check ??? */
        AX_reg(context) = 0xa5a5; /* pretend to have Novell IPX installed */
	break;
    case 0xbf:  /* REDIRIFS.EXE */
        switch (LOBYTE(context->Eax))
        {
        case 0x00:  /* Install check */
            /* not installed */
            break;
        default:
	    INT_BARF( context, 0x2f );
            break;
        }
        break;
    case 0xd2:
	switch(LOBYTE(context->Eax))
	{
	case 0x01: /* Quarterdeck RPCI - QEMM/QRAM - PCL-838.EXE functions */
	    if(BX_reg(context) == 0x5145 && CX_reg(context) == 0x4D4D
	      && DX_reg(context) == 0x3432)
		TRACE("Check for QEMM v5.0+ (not installed)\n");
		break;
	default:
	    INT_BARF( context, 0x2f );
	    break;
	}
	break;
    case 0xd7:  /* Banyan Vines */
        switch (LOBYTE(context->Eax))
        {
        case 0x01:  /* Install check - Get Int Number */
            /* not installed */
            break;
        default:
	    INT_BARF( context, 0x2f );
            break;
        }
        break;
    case 0xde:
	switch(LOBYTE(context->Eax))
	{
	case 0x01:   /* Quarterdeck QDPMI.SYS - DESQview */
	    if(BX_reg(context) == 0x4450 && CX_reg(context) == 0x4d49
	      && DX_reg(context) == 0x8f4f)
		TRACE("Check for QDPMI.SYS (not installed)\n");
		break;
	default:
	    INT_BARF( context, 0x2f );
	    break;
	}
	break;
    case 0xfa:  /* Watcom debugger check, returns 0x666 if installed */
        break;
    default:
        INT_BARF( context, 0x2f );
        break;
    }
}


/**********************************************************************
 *	    do_int2f_16
 */
static void do_int2f_16( CONTEXT86 *context )
{
    DWORD addr;

    switch(LOBYTE(context->Eax))
    {
    case 0x00:  /* Windows enhanced mode installation check */
        AX_reg(context) = (GetWinFlags16() & WF_ENHANCED) ?
                                                  LOWORD(GetVersion16()) : 0;
        break;

    case 0x0a:  /* Get Windows version and type */
        AX_reg(context) = 0;
        BX_reg(context) = (LOWORD(GetVersion16()) << 8) |
                          (LOWORD(GetVersion16()) >> 8);
        CX_reg(context) = (GetWinFlags16() & WF_ENHANCED) ? 3 : 2;
        break;

    case 0x0b:  /* Identify Windows-aware TSRs */
        /* we don't have any pre-Windows TSRs */
        break;

    case 0x11:  /* Get Shell Parameters - (SHELL= in CONFIG.SYS) */
        /* We can mock this up. But not today... */
        FIXME("Get Shell Parameters\n");
        break;

    case 0x80:  /* Release time-slice */
	AL_reg(context) = 0;
        break;

    case 0x81: /* Begin critical section.  */
        /* FIXME? */
        break;

    case 0x82: /* End critical section.  */
        /* FIXME? */
        break;

    case 0x83:  /* Return Current Virtual Machine ID */
        /* Virtual Machines are usually created/destroyed when Windows runs
         * DOS programs. Since we never do, we are always in the System VM.
         * According to Ralf Brown's Interrupt List, never return 0. But it
         * seems to work okay (returning 0), just to be sure we return 1.
         */
	BX_reg(context) = 1; /* VM 1 is probably the System VM */
	break;

    case 0x84:  /* Get device API entry point */
        {
            HMODULE16 mod = GetModuleHandle16("wprocs");
            if (mod < 32) mod = LoadLibrary16( "wprocs" );
            addr = (DWORD)GetProcAddress16( mod, (LPCSTR)(VXD_BASE + BX_reg(context)) );
            if (!addr)  /* not supported */
                ERR("Accessing unknown VxD %04x - Expect a failure now.\n", BX_reg(context) );
            context->SegEs = SELECTOROF(addr);
            DI_reg(context) = OFFSETOF(addr);
        }
	break;

    case 0x86:  /* DPMI detect mode */
        AX_reg(context) = 0;  /* Running under DPMI */
        break;

    case 0x87: /* DPMI installation check */
        {
	    SYSTEM_INFO si;
	    GetSystemInfo(&si);
	    AX_reg(context) = 0x0000; /* DPMI Installed */
            BX_reg(context) = 0x0001; /* 32bits available */
            CL_reg(context) = si.wProcessorLevel;
            DX_reg(context) = 0x005a; /* DPMI major/minor 0.90 */
            SI_reg(context) = 0;      /* # of para. of DOS extended private data */
            context->SegEs = DOSMEM_dpmi_seg;
            DI_reg(context) = 0;      /* ES:DI is DPMI switch entry point */
            break;
        }
    case 0x8a:  /* DPMI get vendor-specific API entry point. */
	/* The 1.0 specs say this should work with all 0.9 hosts.  */
	break;

    default:
        INT_BARF( context, 0x2f );
    }
}

/* FIXME: this macro may have to be changed on architectures where <size> reads/writes
 * must be <size> aligned
 * <size> could be WORD, DWORD...
 * in this case, we would need two macros, one for read, the other one for write
 * Note: PTR_AT can be used as an l-value
 */
#define	PTR_AT(_ptr, _ofs, _typ)	(*((_typ*)(((char*)_ptr)+(_ofs))))

/* Use #if 1 if you want full int 2f debug... normal users can leave it at 0 */
#if 0
/**********************************************************************
 *	    MSCDEX_Dump					[internal]
 *
 * Dumps mscdex requests to int debug channel.
 */
static	void	MSCDEX_Dump(char* pfx, BYTE* req, int dorealmode)
{
    int 	i;
    BYTE	buf[2048];
    BYTE*	ptr;
    BYTE*	ios;

    ptr = buf;
    ptr += sprintf(ptr, "%s\tCommand => ", pfx);
    for (i = 0; i < req[0]; i++) {
	ptr += sprintf(ptr, "%02x ", req[i]);
    }

    switch (req[2]) {
    case 3:
    case 12:
	ptr += sprintf(ptr, "\n\t\t\t\tIO_struct => ");
	ios = (dorealmode) ? PTR_REAL_TO_LIN( PTR_AT(req, 16, WORD), PTR_AT(req, 14, WORD)) :
                             MapSL(MAKESEGPTR(PTR_AT(req, 16, WORD), PTR_AT(req, 14, WORD)));

	for (i = 0; i < PTR_AT(req, 18, WORD); i++) {
	    ptr += sprintf(ptr, "%02x ", ios[i]);
	    if ((i & 0x1F) == 0x1F) {
		*ptr++ = '\n';
		*ptr = 0;
	    }
	}
	break;
    }
    TRACE("%s\n", buf);
}
#else
#define MSCDEX_Dump(pfx, req, drm)
#endif

#define CDFRAMES_PERSEC                 75
#define CDFRAMES_PERMIN                 (CDFRAMES_PERSEC * 60)
#define FRAME_OF_ADDR(a)        ((a)[1] * CDFRAMES_PERMIN + (a)[2] * CDFRAMES_PERSEC + (a)[3])
#define FRAME_OF_TOC(toc, idx)  FRAME_OF_ADDR((toc).TrackData[idx - (toc).FirstTrack].Address)
#define CTRL_OF_TOC(toc, idx)   (((toc).TrackData[idx - (toc).FirstTrack].Control << 4) | \
                                  (toc).TrackData[idx - (toc).FirstTrack].Adr)

static	void	MSCDEX_StoreMSF(DWORD frame, BYTE* val)
{
    val[3] = 0;	/* zero */
    val[2] = frame / CDFRAMES_PERMIN; /* minutes */
    val[1] = (frame / CDFRAMES_PERSEC) % 60; /* seconds */
    val[0] = frame % CDFRAMES_PERSEC; /* frames */
}

static int is_cdrom( int drive)
{
    char root[] = "A:\\";
    root[0] += drive;
    return (GetDriveTypeA(root) == DRIVE_CDROM);
}

static void MSCDEX_Handler(CONTEXT86* context)
{
    int 	drive, count;
    char*	p;

    switch (LOBYTE(context->Eax)) {
    case 0x00: /* Installation check */
	/* Count the number of contiguous CDROM drives
	 */
	for (drive = count = 0; drive < 26; drive++) {
	    if (is_cdrom(drive)) {
		while (is_cdrom(drive + count)) count++;
		break;
	    }
	}
	TRACE("Installation check: %d cdroms, starting at %d\n", count, drive);
	BX_reg(context) = count;
	CX_reg(context) = (drive < 26) ? drive : 0;
	break;

    case 0x0B: /* drive check */
	AX_reg(context) = is_cdrom(CX_reg(context));
	BX_reg(context) = 0xADAD;
	break;

    case 0x0C: /* get version */
	BX_reg(context) = 0x020a;
	TRACE("Version number => %04x\n", BX_reg(context));
	break;

    case 0x0D: /* get drive letters */
	p = CTX_SEG_OFF_TO_LIN(context, context->SegEs, context->Ebx);
	memset(p, 0, 26);
	for (drive = 0; drive < 26; drive++) {
	    if (is_cdrom(drive)) *p++ = drive;
	}
	TRACE("Get drive letters\n");
	break;

    case 0x10: /* direct driver access */
	{
	    BYTE* 	driver_request;
	    BYTE* 	io_stru;
	    BYTE 	Error = 255; /* No Error */
	    int		dorealmode = ISV86(context);
            char        devName[] = "\\\\.\\@:";
	    HANDLE      h;
            CDROM_TOC                   toc;
            CDROM_SUB_Q_DATA_FORMAT     fmt;
            SUB_Q_CHANNEL_DATA          data;
            DWORD                       br;
            DWORD                       present = TRUE;

	    driver_request = CTX_SEG_OFF_TO_LIN(context, context->SegEs, context->Ebx);

	    if (!driver_request) {
		/* FIXME - to be deleted ?? */
		ERR("ES:BX==0 ! SEGFAULT ?\n");
		ERR("-->BX=0x%04x, ES=0x%04lx, DS=0x%04lx, CX=0x%04x\n",
		    BX_reg(context), context->SegEs, context->SegDs, CX_reg(context));
		driver_request[4] |= 0x80;
		driver_request[3] = 5;	/* bad request length */
		return;
	    }
	    /* FIXME
             * the following tests are wrong because lots of functions don't require the
             * tray to be closed with a CD inside
	     */
	    TRACE("CDROM device driver -> command <%d>\n", (unsigned char)driver_request[2]);

	    if (!is_cdrom(CX_reg(context))) {
		WARN("Request made doesn't match a CD ROM drive (%d)\n", CX_reg(context));
		driver_request[4] |= 0x80;
		driver_request[3] = 1;	/* unknown unit */
		return;
	    }

	    MSCDEX_Dump("Beg", driver_request, dorealmode);

	    /* set status to 0 */
	    PTR_AT(driver_request, 3, WORD) = 0;
            devName[4] = 'A' + CX_reg(context);
            h = CreateFileA(devName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
            if (!h) {
		WARN("Couldn't open cdrom handle\n");
		driver_request[4] |= 0x80;
		driver_request[3] = 1;	/* unknown unit */
		return;
	    }

            fmt.Format = IOCTL_CDROM_CURRENT_POSITION;
            if (!DeviceIoControl(h, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), &br, NULL) ||
                !DeviceIoControl(h, IOCTL_CDROM_READ_Q_CHANNEL, &fmt, sizeof(fmt),
                                 &data, sizeof(data), &br, NULL)) {
                if (GetLastError() == STATUS_NO_MEDIA_IN_DEVICE)
                {
                    if (driver_request[2] != 6 && driver_request[2] != 15)
                    {
                        driver_request[4] |= 0x80;
                        driver_request[3] = 2;	/* drive not ready */
                        CloseHandle(h);
                        return;
                    }
                    present = FALSE;
                }
                else
                {
                    driver_request[4] |= 0x80;
                    driver_request[3] = 1;	/* unknown unit */
                    CloseHandle(h);
                    return;
                }
            }

	    switch (driver_request[2]) {
	    case 3:
		io_stru = (dorealmode) ?
                    PTR_REAL_TO_LIN( PTR_AT(driver_request, 16, WORD), PTR_AT(driver_request, 14, WORD) ) :
			MapSL( MAKESEGPTR(PTR_AT(driver_request, 16, WORD), PTR_AT(driver_request, 14, WORD)));

		TRACE(" --> IOCTL INPUT <%d>\n", io_stru[0]);
		switch (io_stru[0]) {
#if 0
		case 0: /* Get device Header */
		    {
			static	LPSTR ptr = 0;
			if (ptr == 0)	{
			    ptr = SEGPTR_ALLOC(22);
			    PTR_AT(ptr,  0, DWORD) = ~1;	/* Next Device Driver */
			    PTR_AT(ptr,  4,  WORD) = 0xC800;	/* Device attributes  */
			    PTR_AT(ptr,  6,  WORD) = 0x1234;	/* Pointer to device strategy routine: FIXME */
			    PTR_AT(ptr,  8,  WORD) = 0x3142;	/* Pointer to device interrupt routine: FIXME */
			    PTR_AT(ptr, 10,  char) = 'W';  	/* 8-byte character device name field */
			    PTR_AT(ptr, 11,  char) = 'I';
			    PTR_AT(ptr, 12,  char) = 'N';
			    PTR_AT(ptr, 13,  char) = 'E';
			    PTR_AT(ptr, 14,  char) = '_';
			    PTR_AT(ptr, 15,  char) = 'C';
			    PTR_AT(ptr, 16,  char) = 'D';
			    PTR_AT(ptr, 17,  char) = '_';
			    PTR_AT(ptr, 18,  WORD) = 0;		/* Reserved (must be zero) */
			    PTR_AT(ptr, 20,  BYTE) = 0;         /* Drive letter (must be zero) */
			    PTR_AT(ptr, 21,  BYTE) = 1;         /* Number of units supported (one or more) FIXME*/
			}
			PTR_AT(io_stru, DWORD,  0) = SEGPTR_GET(ptr);
		    }
		    break;
#endif

		case 1: /* location of head */
		    switch (io_stru[1]) {
		    case 0:
			PTR_AT(io_stru, 2, DWORD) =
                            FRAME_OF_ADDR(data.CurrentPosition.AbsoluteAddress);
			break;
		    case 1:
			MSCDEX_StoreMSF(FRAME_OF_ADDR(data.CurrentPosition.AbsoluteAddress),
                                        io_stru + 2);
			break;
		    default:
			ERR("CD-ROM driver: unsupported addressing mode !!\n");
			Error = 0x0c;
		    }
		    TRACE(" ----> HEAD LOCATION <%ld>\n", PTR_AT(io_stru, 2, DWORD));
		    break;

		case 4: /* Audio channel info */
		    io_stru[1] = 0;
		    io_stru[2] = 0xff;
		    io_stru[3] = 1;
		    io_stru[4] = 0xff;
		    io_stru[5] = 2;
		    io_stru[6] = 0;
		    io_stru[7] = 3;
		    io_stru[8] = 0;
		    TRACE(" ----> AUDIO CHANNEL INFO\n");
		    break;

		case 6: /* device status */
		    PTR_AT(io_stru, 1, DWORD) = 0x00000290;
		    /* 290 =>
		     * 1	Supports HSG and Red Book addressing modes
		     * 0	Supports audio channel manipulation
		     *
		     * 1	Supports prefetching requests
		     * 0	Reserved
		     * 0	No interleaving
		     * 1	Data read and plays audio/video tracks
		     *
		     * 0	Read only
		     * 0	Supports only cooked reading
		     * 0	Door locked
		     * 0	see below (Door closed/opened)
		     */
		    if (!present) PTR_AT(io_stru, 1, DWORD) |= 1;
		    TRACE(" ----> DEVICE STATUS <0x%08lx>\n", PTR_AT(io_stru, 1, DWORD));
		    break;

		case 8: /* Volume size */
		    PTR_AT(io_stru, 1, DWORD) = FRAME_OF_TOC(toc, toc.LastTrack + 1) -
                        FRAME_OF_TOC(toc, toc.FirstTrack) - 1;
		    TRACE(" ----> VOLUME SIZE <%ld>\n", PTR_AT(io_stru, 1, DWORD));
		    break;

		case 9: /* media changed ? */
		    /* answers don't know... -1/1 for yes/no would be better */
		    io_stru[1] = 0; /* FIXME? 1? */
		    TRACE(" ----> MEDIA CHANGED <%d>\n", io_stru[1]);
		    break;

		case 10: /* audio disk info */
		    io_stru[1] = toc.FirstTrack; /* starting track of the disc */
		    io_stru[2] = toc.LastTrack;  /* ending track */
		    MSCDEX_StoreMSF(FRAME_OF_TOC(toc, toc.LastTrack + 1) -
                                    FRAME_OF_TOC(toc, toc.FirstTrack) - 1, io_stru + 3);

		    TRACE(" ----> AUDIO DISK INFO <%d-%d/%08lx>\n",
			  io_stru[1], io_stru[2], PTR_AT(io_stru, 3, DWORD));
		    break;

		case 11: /* audio track info */
		    if (io_stru[1] >= toc.FirstTrack && io_stru[1] <= toc.LastTrack) {
			MSCDEX_StoreMSF(FRAME_OF_TOC(toc, io_stru[1]), io_stru + 2);
			/* starting point if the track */
			io_stru[6] = CTRL_OF_TOC(toc, io_stru[1]);
		    } else {
			PTR_AT(io_stru, 2, DWORD) = 0;
			io_stru[6] = 0;
		    }
		    TRACE(" ----> AUDIO TRACK INFO[%d] = [%08lx:%d]\n",
			  io_stru[1], PTR_AT(io_stru, 2, DWORD), io_stru[6]);
		    break;

		case 12: /* get Q-Channel info */
		    io_stru[1] = CTRL_OF_TOC(toc, data.CurrentPosition.TrackNumber);
		    io_stru[2] = data.CurrentPosition.TrackNumber;
		    io_stru[3] = 0; /* FIXME ?? */

		    /* why the heck did MS use another format for 0MSF information... sigh */
		    {
			BYTE	bTmp[4];

			MSCDEX_StoreMSF(FRAME_OF_ADDR(data.CurrentPosition.TrackRelativeAddress), bTmp);
			io_stru[ 4] = bTmp[2];
			io_stru[ 5] = bTmp[1];
			io_stru[ 6] = bTmp[0];
			io_stru[ 7] = 0;

			MSCDEX_StoreMSF(FRAME_OF_ADDR(data.CurrentPosition.AbsoluteAddress), bTmp);
			io_stru[ 8] = bTmp[2];
			io_stru[ 9] = bTmp[1];
			io_stru[10] = bTmp[0];
			io_stru[11] = 0;
		    }
		    TRACE("Q-Channel info: Ctrl/adr=%02x TNO=%02x X=%02x rtt=%02x:%02x:%02x rtd=%02x:%02x:%02x (cf=%08x, tp=%08x)\n",
			  io_stru[ 1], io_stru[ 2], io_stru[ 3],
			  io_stru[ 4], io_stru[ 5], io_stru[ 6],
			  io_stru[ 8], io_stru[ 9], io_stru[10],
			  FRAME_OF_ADDR(data.CurrentPosition.AbsoluteAddress),
                          FRAME_OF_TOC(toc, data.CurrentPosition.TrackNumber));
		    break;

		case 15: /* Audio status info */
		    /* !!!! FIXME FIXME FIXME !! */
		    PTR_AT(io_stru, 1,  WORD) = 2 | ((data.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_PAUSED) ? 1 : 0);
		    if (!present) {
			PTR_AT(io_stru, 3, DWORD) = 0;
			PTR_AT(io_stru, 7, DWORD) = 0;
		    } else {
			PTR_AT(io_stru, 3, DWORD) = FRAME_OF_TOC(toc, toc.FirstTrack);
			PTR_AT(io_stru, 7, DWORD) = FRAME_OF_TOC(toc, toc.LastTrack + 1);
		    }
		    TRACE("Audio status info: status=%04x startLoc=%ld endLoc=%ld\n",
			  PTR_AT(io_stru, 1, WORD), PTR_AT(io_stru, 3, DWORD), PTR_AT(io_stru, 7, DWORD));
		    break;

		default:
		    FIXME("IOCTL INPUT: Unimplemented <%d>!!\n", io_stru[0]);
		    Error = 0x0c;
		    break;
		}
		break;

	    case 12:
		io_stru = (dorealmode) ?
		    PTR_REAL_TO_LIN( PTR_AT(driver_request, 16, WORD), PTR_AT(driver_request, 14, WORD)) :
			MapSL( MAKESEGPTR(PTR_AT(driver_request, 16, WORD), PTR_AT(driver_request, 14, WORD)));

		TRACE(" --> IOCTL OUTPUT <%d>\n", io_stru[0]);
		switch (io_stru[0]) {
		case 0: /* eject */
                    DeviceIoControl(h, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &br, NULL);
		    TRACE(" ----> EJECT\n");
		    break;
		case 2: /* reset drive */
		    DeviceIoControl(h, IOCTL_STORAGE_RESET_DEVICE, NULL, 0, NULL, 0, &br, NULL);
		    TRACE(" ----> RESET\n");
		    break;
		case 3: /* Audio Channel Control */
		    FIXME(" ----> AUDIO CHANNEL CONTROL (NIY)\n");
		    break;
		case 5: /* close tray */
                    DeviceIoControl(h, IOCTL_STORAGE_LOAD_MEDIA, NULL, 0, NULL, 0, &br, NULL);
		    TRACE(" ----> CLOSE TRAY\n");
		    break;
		default:
		    FIXME(" IOCTL OUTPUT: Unimplemented <%d>!!\n", io_stru[0]);
		    Error = 0x0c;
		    break;
		}
		break;

            case 128: /* read long */
                {
                    LPVOID              dst = MapSL(MAKESEGPTR(PTR_AT(driver_request, 16, WORD),
                                                               PTR_AT(driver_request, 14, WORD)));
                    DWORD               at = PTR_AT(driver_request, 20, DWORD);
                    WORD                num = PTR_AT(driver_request, 18, WORD);
                    RAW_READ_INFO       rri;

		    switch (driver_request[13]) {
		    case 1: /* Red book addressing mode = 0:m:s:f */
			/* FIXME : frame <=> msf conversion routines could be shared
			 * between mscdex and mcicda
			 */
			at = LOBYTE(HIWORD(at)) * CDFRAMES_PERMIN +
			    HIBYTE(LOWORD(at)) * CDFRAMES_PERSEC +
			    LOBYTE(LOWORD(at));
			/* fall through */
		    case 0: /* HSG addressing mode */
                        switch (PTR_AT(driver_request, 24, BYTE))
                        {
                        case 0: /* cooked */
                            ReadFile(h, dst, num * 2048, &br, NULL);
                            break;
                        case 1:
                            /* FIXME: computation is wrong */
                            rri.DiskOffset.s.HighPart = 0;
                            rri.DiskOffset.s.LowPart = at << 11;
                            rri.TrackMode = YellowMode2;
                            rri.SectorCount = num;
                            DeviceIoControl(h, IOCTL_CDROM_RAW_READ, &rri, sizeof(rri),
                                            dst, num * 2352, &br, NULL);
			break;
                        default:
                            ERR("Unsupported read mode !!\n");
                            Error = 0x0c;
                            break;
                        }
                        break;
		    default:
			ERR("Unsupported address mode !!\n");
			Error = 0x0c;
			break;
		    }
                }
                break;

	    case 131: /* seek */
		{
		    DWORD	                at;
                    CDROM_SEEK_AUDIO_MSF        seek;

		    at = PTR_AT(driver_request, 20, DWORD);

		    TRACE(" --> SEEK AUDIO mode :<0x%02X>, [%ld]\n",
			  (BYTE)driver_request[13], at);

		    switch (driver_request[13]) {
		    case 1: /* Red book addressing mode = 0:m:s:f */
			/* FIXME : frame <=> msf conversion routines could be shared
			 * between mscdex and mcicda
			 */
			at = LOBYTE(HIWORD(at)) * CDFRAMES_PERMIN +
			    HIBYTE(LOWORD(at)) * CDFRAMES_PERSEC +
			    LOBYTE(LOWORD(at));
			/* fall through */
		    case 0: /* HSG addressing mode */
                        seek.M = at / CDFRAMES_PERMIN;
                        seek.S = (at / CDFRAMES_PERSEC) % 60;
                        seek.F = at % CDFRAMES_PERSEC;
                        DeviceIoControl(h, IOCTL_CDROM_SEEK_AUDIO_MSF, &seek, sizeof(seek),
                                        NULL, 0, &br, NULL);
			break;
		    default:
			ERR("Unsupported address mode !!\n");
			Error = 0x0c;
			break;
		    }
		}
		break;

	    case 132: /* play */
		{
		    DWORD	                beg, end;
                    CDROM_PLAY_AUDIO_MSF        play;

		    beg = end = PTR_AT(driver_request, 14, DWORD);
		    end += PTR_AT(driver_request, 18, DWORD);

		    TRACE(" --> PLAY AUDIO mode :<0x%02X>, [%ld-%ld]\n",
			  (BYTE)driver_request[13], beg, end);

		    switch (driver_request[13]) {
		    case 1:
			/* Red book addressing mode = 0:m:s:f */
			/* FIXME : frame <=> msf conversion routines could be shared
			 * between mscdex and mcicda
			 */
			beg = LOBYTE(LOWORD(beg)) * CDFRAMES_PERMIN +
			    HIBYTE(LOWORD(beg)) * CDFRAMES_PERSEC +
			    LOBYTE(HIWORD(beg));
			end = LOBYTE(LOWORD(end)) * CDFRAMES_PERMIN +
			    HIBYTE(LOWORD(end)) * CDFRAMES_PERSEC +
			    LOBYTE(HIWORD(end));
			/* fall through */
		    case 0: /* HSG addressing mode */
                        play.StartingM = beg / CDFRAMES_PERMIN;
                        play.StartingS = (beg / CDFRAMES_PERSEC) % 60;
                        play.StartingF = beg % CDFRAMES_PERSEC;
                        play.EndingM   = end / CDFRAMES_PERMIN;
                        play.EndingS   = (end / CDFRAMES_PERSEC) % 60;
                        play.EndingF   = end % CDFRAMES_PERSEC;
                        DeviceIoControl(h, IOCTL_CDROM_PLAY_AUDIO_MSF, &play, sizeof(play),
                                        NULL, 0, &br, NULL);
			break;
		    default:
			ERR("Unsupported address mode !!\n");
			Error = 0x0c;
			break;
		    }
		}
		break;

	    case 133:
		if (data.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS) {
                    DeviceIoControl(h, IOCTL_CDROM_PAUSE_AUDIO, NULL, 0, NULL, 0, &br, NULL);
		    TRACE(" --> STOP AUDIO (Paused)\n");
		} else {
                    DeviceIoControl(h, IOCTL_CDROM_STOP_AUDIO, NULL, 0, NULL, 0, &br, NULL);
		    TRACE(" --> STOP AUDIO (Stopped)\n");
		}
		break;

	    case 136:
		TRACE(" --> RESUME AUDIO\n");
                DeviceIoControl(h, IOCTL_CDROM_PAUSE_AUDIO, NULL, 0, NULL, 0, &br, NULL);
		break;

	    default:
		FIXME(" ioctl unimplemented <%d>\n", driver_request[2]);
		Error = 0x0c;
	    }

	    /* setting error codes if any */
	    if (Error < 255) {
		driver_request[4] |= 0x80;
		driver_request[3] = Error;
	    }

	    CloseHandle(h);
	    /* setting status bits
	     * 3 == playing && done
	     * 1 == done
	     */
	    driver_request[4] |=
                (data.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS) ? 3 : 1;

	    MSCDEX_Dump("End", driver_request, dorealmode);
	}
	break;
    default:
	FIXME("Unimplemented MSCDEX function 0x%02X.\n", LOBYTE(context->Eax));
	break;
    }
}
