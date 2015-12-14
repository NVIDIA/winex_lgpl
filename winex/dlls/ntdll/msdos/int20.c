/*
 * DOS interrupt 20h handler (TERMINATE PROGRAM)
 */

#include <stdlib.h>
#include "winbase.h"
#include "miscemu.h"

/**********************************************************************
 *	    INT_Int20Handler (WPROCS.132)
 *
 * Handler for int 20h.
 */
void WINAPI INT_Int20Handler( CONTEXT86 *context )
{
    ExitThread( 0 );
}
