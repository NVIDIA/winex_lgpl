/*
 * Emulation of processor ioports.
 *
 * Copyright 1995 Morten Welinder
 * Copyright 1998 Andreas Mohr, Ove Kaaven
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

/* Known problems:
   - only a few ports are emulated.
   - real-time clock in "cmos" is bogus.  A nifty alarm() setup could
     fix that, I guess.
*/

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "windef.h"
#include "callback.h"
#include "options.h"
#include "miscemu.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

static struct {
    WORD	countmax;
    BOOL16	byte_toggle; /* if TRUE, then hi byte has already been written */
    WORD	latch;
    BOOL16	latched;
    BYTE	ctrlbyte_ch;
    WORD	oldval;
} tmr_8253[3] = {
    {0xFFFF,	FALSE,	0,	FALSE,	0x36,	0},
    {0x0012,	FALSE,	0,	FALSE,	0x74,	0},
    {0x0001,	FALSE,	0,	FALSE,	0xB6,	0},
};

static int dummy_ctr = 0;

static BYTE parport_8255[4] = {0x4f, 0x20, 0xff, 0xff};

static BYTE cmosaddress;

static BYTE cmosimage[64] =
{
  0x27, 0x34, 0x31, 0x47, 0x16, 0x15, 0x00, 0x01,
  0x04, 0x94, 0x26, 0x02, 0x50, 0x80, 0x00, 0x00,
  0x40, 0xb1, 0x00, 0x9c, 0x01, 0x80, 0x02, 0x00,
  0x1c, 0x00, 0x00, 0xad, 0x02, 0x10, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x03, 0x19,
  0x00, 0x1c, 0x19, 0x81, 0x00, 0x0e, 0x00, 0x80,
  0x1b, 0x7b, 0x21, 0x00, 0x00, 0x00, 0x05, 0x5f
};

#if defined(linux) && defined(__i386__)
# define DIRECT_IO_ACCESS
#else
# undef DIRECT_IO_ACCESS
# undef PP_IO_ACCESS
#endif  /* linux && __i386__ */

#ifdef DIRECT_IO_ACCESS

extern int iopl(int level);
static char do_direct_port_access = -1;
#ifdef HAVE_PPDEV
static char do_pp_port_access = -1; /* -1: uninitialized, 1: not available
				       0: available);*/
#endif
static char port_permissions[0x10000];

#define IO_READ  1
#define IO_WRITE 2

static void IO_FixCMOSCheckSum(void)
{
	WORD sum = 0;
	int i;

	for (i=0x10; i < 0x2d; i++)
		sum += cmosimage[i];
	cmosimage[0x2e] = sum >> 8; /* yes, this IS hi byte !! */
	cmosimage[0x2f] = sum & 0xff;
	TRACE("calculated hi %02x, lo %02x\n", cmosimage[0x2e], cmosimage[0x2f]);
}

#endif  /* DIRECT_IO_ACCESS */

static void set_timer_maxval(unsigned timer, unsigned maxval)
{
    switch (timer) {
        case 0: /* System timer counter divisor */
            if (Dosvm.SetTimer) Dosvm.SetTimer(maxval);
            break;
        case 1: /* RAM refresh */
            FIXME("RAM refresh counter handling not implemented !\n");
            break;
        case 2: /* cassette & speaker */
            /* speaker on ? */
            if (((BYTE)parport_8255[1] & 3) == 3)
            {
                TRACE("Beep (freq: %d) !\n", 1193180 / maxval );
                Beep(1193180 / maxval, 20);
            }
            break;
    }
}

/**********************************************************************
 *	    IO_port_init
 */

/* set_IO_permissions(int val1, int val)
 * Helper function for IO_port_init
 */
#ifdef DIRECT_IO_ACCESS
static void set_IO_permissions(int val1, int val, char rw)
{
	int j;
	if (val1 != -1) {
		if (val == -1) val = 0x3ff;
		for (j = val1; j <= val; j++)
			port_permissions[j] |= rw;

		do_direct_port_access = 1;

		val1 = -1;
	} else if (val != -1) {
		do_direct_port_access = 1;

		port_permissions[val] |= rw;
	}

}

/* do_IO_port_init_read_or_write(char* temp, char rw)
 * Helper function for IO_port_init
 */

static void do_IO_port_init_read_or_write(char* temp, char rw)
{
	int val, val1, i, len;
	if (!strcasecmp(temp, "all")) {
		MESSAGE("Warning!!! Granting FULL IO port access to"
			" windoze programs!\nWarning!!! "
			"*** THIS IS NOT AT ALL "
			"RECOMMENDED!!! ***\n");
		for (i=0; i < sizeof(port_permissions); i++)
			port_permissions[i] |= rw;

	} else if (!(!strcmp(temp, "*") || *temp == '\0')) {
		len = strlen(temp);
		val = -1;
		val1 = -1;
		for (i = 0; i < len; i++) {
			switch (temp[i]) {
			case '0':
				if (temp[i+1] == 'x' || temp[i+1] == 'X') {
					sscanf(temp+i, "%x", &val);
					i += 2;
				} else {
					sscanf(temp+i, "%d", &val);
				}
				while (isxdigit(temp[i]))
					i++;
				i--;
				break;
			case ',':
			case ' ':
			case '\t':
				set_IO_permissions(val1, val, rw);
				val1 = -1; val = -1;
				break;
			case '-':
				val1 = val;
				if (val1 == -1) val1 = 0;
				break;
			default:
				if (temp[i] >= '0' && temp[i] <= '9') {
					sscanf(temp+i, "%d", &val);
					while (isdigit(temp[i]))
						i++;
				}
			}
		}
		set_IO_permissions(val1, val, rw);
	}
}

static inline BYTE inb( WORD port )
{
    BYTE b;
    __asm__ __volatile__( "inb %w1,%0" : "=a" (b) : "d" (port) );
    return b;
}

static inline WORD inw( WORD port )
{
    WORD w;
    __asm__ __volatile__( "inw %w1,%0" : "=a" (w) : "d" (port) );
    return w;
}

static inline DWORD inl( WORD port )
{
    DWORD dw;
    __asm__ __volatile__( "inl %w1,%0" : "=a" (dw) : "d" (port) );
    return dw;
}

static inline void outb( BYTE value, WORD port )
{
    __asm__ __volatile__( "outb %b0,%w1" : : "a" (value), "d" (port) );
}

static inline void outw( WORD value, WORD port )
{
    __asm__ __volatile__( "outw %w0,%w1" : : "a" (value), "d" (port) );
}

static inline void outl( DWORD value, WORD port )
{
    __asm__ __volatile__( "outl %0,%w1" : : "a" (value), "d" (port) );
}

static void IO_port_init(void)
{
	char temp[1024];

        do_direct_port_access = 0;
	/* Can we do that? */
	if (!iopl(3)) {
		iopl(0);

		PROFILE_GetWineIniString( "ports", "read", "*",
					 temp, sizeof(temp) );
		do_IO_port_init_read_or_write(temp, IO_READ);
		PROFILE_GetWineIniString( "ports", "write", "*",
					 temp, sizeof(temp) );
		do_IO_port_init_read_or_write(temp, IO_WRITE);
	}
    IO_FixCMOSCheckSum();
}

#endif  /* DIRECT_IO_ACCESS */

/**********************************************************************
 *	    IO_inport
 *
 * Note: The size argument has to be handled correctly _externally_
 * (as we always return a DWORD)
 */
DWORD IO_inport( int port, int size )
{
    DWORD res = 0;

    TRACE("%d-byte value from port 0x%02x\n", size, port );

#ifdef HAVE_PPDEV
    if (do_pp_port_access == -1)
      do_pp_port_access =IO_pp_init();
    if ((do_pp_port_access == 0 ) && (size == 1))
      if (!IO_pp_inp(port,&res))
         return res;
#endif
#ifdef DIRECT_IO_ACCESS
    if (do_direct_port_access == -1) IO_port_init();
    if ((do_direct_port_access)
        /* Make sure we have access to the port */
        && (port_permissions[port] & IO_READ))
    {
        iopl(3);
        switch(size)
        {
        case 1: res = inb( port ); break;
        case 2: res = inw( port ); break;
        case 4: res = inl( port ); break;
        default:
            ERR("invalid data size %d\n", size);
        }
        iopl(0);
        return res;
    }
#endif

    /* first give the DOS VM a chance to handle it */
    if (Dosvm.inport && Dosvm.inport( port, size, &res )) return res;

    switch (port)
    {
    case 0x40:
    case 0x41:
    case 0x42:
    {
        BYTE chan = port & 3;
        WORD tempval = 0;
	if (tmr_8253[chan].latched)
	    tempval = tmr_8253[chan].latch;
	else
	{
	    dummy_ctr -= 1 + (int)(10.0 * rand() / (RAND_MAX + 1.0));
	    if (chan == 0) /* System timer counter divisor */
	    {
		/* FIXME: Dosvm.GetTimer() returns quite rigid values */
	        if (Dosvm.GetTimer)
		  tempval = dummy_ctr + (WORD)Dosvm.GetTimer();
		else
		  tempval = dummy_ctr;
	    }
	    else
	    {
		/* FIXME: intelligent hardware timer emulation needed */
		tempval = dummy_ctr;
	    }
	}

        switch ((tmr_8253[chan].ctrlbyte_ch & 0x30) >> 4)
        {
        case 0:
	    res = 0; /* shouldn't happen? */
	    break;
        case 1: /* read lo byte */
	    res = (BYTE)tempval;
	    tmr_8253[chan].latched = FALSE;
	    break;
        case 3: /* read lo byte, then hi byte */
            tmr_8253[chan].byte_toggle ^= TRUE; /* toggle */
            if (tmr_8253[chan].byte_toggle)
            {
                res = (BYTE)tempval;
                break;
            }
            /* else [fall through if read hi byte !] */
        case 2: /* read hi byte */
	    res = (BYTE)(tempval >> 8);
 	    tmr_8253[chan].latched = FALSE;
	    break;
        }
    }
    break;
    case 0x60:
#if 0 /* what's this port got to do with parport ? */
        res = (DWORD)parport_8255[0];
#endif
        break;
    case 0x61:
        res = (DWORD)parport_8255[1];
        break;
    case 0x62:
        res = (DWORD)parport_8255[2];
        break;
    case 0x70:
        res = (DWORD)cmosaddress;
        break;
    case 0x71:
        res = (DWORD)cmosimage[cmosaddress & 0x3f];
        break;
    case 0x200:
    case 0x201:
        res = 0xffffffff; /* no joystick */
        break;
    default:
        WARN("Direct I/O read attempted from port %x\n", port);
        res = 0xffffffff;
        break;
    }
    TRACE("  returning ( 0x%lx )\n", res );
    return res;
}


/**********************************************************************
 *	    IO_outport
 */
void IO_outport( int port, int size, DWORD value )
{
    TRACE("IO: 0x%lx (%d-byte value) to port 0x%02x\n",
                 value, size, port );

#ifdef HAVE_PPDEV
    if (do_pp_port_access == -1)
      do_pp_port_access = IO_pp_init();
    if ((do_pp_port_access == 0) && (size == 1))
      if (!IO_pp_outp(port,&value))
         return;
#endif
#ifdef DIRECT_IO_ACCESS

    if (do_direct_port_access == -1) IO_port_init();
    if ((do_direct_port_access)
        /* Make sure we have access to the port */
        && (port_permissions[port] & IO_WRITE))
    {
        iopl(3);
        switch(size)
        {
        case 1: outb( LOBYTE(value), port ); break;
        case 2: outw( LOWORD(value), port ); break;
        case 4: outl( value, port ); break;
        default:
            WARN("Invalid data size %d\n", size);
        }
        iopl(0);
        return;
    }
#endif

    /* first give the DOS VM a chance to handle it */
    if (Dosvm.outport && Dosvm.outport( port, size, value )) return;

    switch (port)
    {
    case 0x40:
    case 0x41:
    case 0x42:
    {
        BYTE chan = port & 3;

	/* we need to get the oldval before any lo/hi byte change has been made */
        if (((tmr_8253[chan].ctrlbyte_ch & 0x30) != 0x30) ||
	    !tmr_8253[chan].byte_toggle)
            tmr_8253[chan].oldval = tmr_8253[chan].countmax;
        switch ((tmr_8253[chan].ctrlbyte_ch & 0x30) >> 4)
        {
        case 0:
            break; /* shouldn't happen? */
        case 1: /* write lo byte */
            tmr_8253[chan].countmax =
                (tmr_8253[chan].countmax & 0xff00) | (BYTE)value;
            break;
        case 3: /* write lo byte, then hi byte */
            tmr_8253[chan].byte_toggle ^= TRUE; /* toggle */
            if (tmr_8253[chan].byte_toggle)
            {
                tmr_8253[chan].countmax =
                    (tmr_8253[chan].countmax & 0xff00) | (BYTE)value;
                break;
            }
            /* else [fall through if write hi byte !] */
        case 2: /* write hi byte */
            tmr_8253[chan].countmax =
                (tmr_8253[chan].countmax & 0x00ff) | ((BYTE)value << 8);
            break;
        }
	/* if programming is finished and value has changed
	   then update to new value */
        if ((((tmr_8253[chan].ctrlbyte_ch & 0x30) != 0x30) ||
	     !tmr_8253[chan].byte_toggle) &&
	    (tmr_8253[chan].countmax != tmr_8253[chan].oldval))
            set_timer_maxval(chan, tmr_8253[chan].countmax);
    }
    break;
    case 0x43:
    {
	BYTE chan = ((BYTE)value & 0xc0) >> 6;
	/* ctrl byte for specific timer channel */
	if (chan == 3)
	{
	    FIXME("8254 timer readback not implemented yet\n");
	    break;
	}
	switch (((BYTE)value & 0x30) >> 4)
	{
	case 0:	/* latch timer */
	    tmr_8253[chan].latched = TRUE;
	    dummy_ctr -= 1 + (int)(10.0 * rand() / (RAND_MAX + 1.0));
	    if (chan == 0) /* System timer divisor */
	        if (Dosvm.GetTimer)
		  tmr_8253[chan].latch = dummy_ctr + (WORD)Dosvm.GetTimer();
	        else
		  tmr_8253[chan].latch = dummy_ctr;
	    else
	    {
		/* FIXME: intelligent hardware timer emulation needed */
		tmr_8253[chan].latch = dummy_ctr;
	    }
	    break;
	case 3:	/* write lo byte, then hi byte */
	    tmr_8253[chan].byte_toggle = FALSE; /* init */
	    /* fall through */
	case 1:	/* write lo byte only */
	case 2:	/* write hi byte only */
	    tmr_8253[chan].ctrlbyte_ch = (BYTE)value;
	    break;
	}
    }
    break;
    case 0x61:
        parport_8255[1] = (BYTE)value;
        if ((((BYTE)parport_8255[1] & 3) == 3) && (tmr_8253[2].countmax != 1))
        {
            TRACE("Beep (freq: %d) !\n", 1193180 / tmr_8253[2].countmax);
            Beep(1193180 / tmr_8253[2].countmax, 20);
        }
        break;
    case 0x70:
        cmosaddress = (BYTE)value & 0x7f;
        break;
    case 0x71:
        cmosimage[cmosaddress & 0x3f] = (BYTE)value;
        break;
    default:
        WARN("Direct I/O write attempted to port %x\n", port );
        break;
    }
}
