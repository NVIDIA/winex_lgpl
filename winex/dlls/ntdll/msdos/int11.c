/*
 * BIOS interrupt 11h handler
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "miscemu.h"
#include "msdos.h"
#include "wine/hardware.h"
#include "wine/debug.h"
#include "options.h"

/**********************************************************************
 *	    INT_Int11Handler (WPROCS.117)
 *
 * Handler for int 11h (get equipment list).
 */
void WINAPI INT_Int11Handler( CONTEXT86 *context )
{
    int diskdrives = 0;
    int parallelports = 0;
    int serialports = 0;
    int x;

/* borrowed from Ralph Brown's interrupt lists

		    bits 15-14: number of parallel devices
		    bit     13: [Conv] Internal modem
		    bit     12: reserved
		    bits 11- 9: number of serial devices
		    bit      8: reserved
		    bits  7- 6: number of diskette drives minus one
		    bits  5- 4: Initial video mode:
				    00b = EGA,VGA,PGA
				    01b = 40 x 25 color
				    10b = 80 x 25 color
				    11b = 80 x 25 mono
		    bit      3: reserved
		    bit      2: [PS] =1 if pointing device
				[non-PS] reserved
		    bit      1: =1 if math co-processor
		    bit      0: =1 if diskette available for boot
*/
/*  Currently the only of these bits correctly set are:
		bits 15-14 		} Added by William Owen Smith,
		bits 11-9		} wos@dcs.warwick.ac.uk
		bits 7-6
		bit  2			(always set)
*/

    if (GetDriveTypeA("A:\\") == DRIVE_REMOVABLE) diskdrives++;
    if (GetDriveTypeA("B:\\") == DRIVE_REMOVABLE) diskdrives++;
    if (diskdrives) diskdrives--;

    for (x=0; x < 9; x++)
    {
        char temp[16],name[16];

        sprintf(name,"COM%d",x+1);
        PROFILE_GetWineIniString("serialports",name,"*",temp,sizeof temp);
        if(strcmp(temp,"*"))
	    serialports++;

        sprintf(name,"LPT%d",x+1);
        PROFILE_GetWineIniString("parallelports",name,"*",temp,sizeof temp);
        if(strcmp(temp,"*"))
	    parallelports++;
    }
    if (serialports > 7)		/* 3 bits -- maximum value = 7 */
        serialports=7;
    if (parallelports > 3)		/* 2 bits -- maximum value = 3 */
        parallelports=3;

    AX_reg(context) = (diskdrives << 6) | (serialports << 9) |
                      (parallelports << 14) | 0x02;
}
