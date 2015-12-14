/*
 * Copyright 2006 TransGaming Technologies
 */

#include <stdlib.h>
#include "config.h"
#include "wine/port.h"

#ifndef HAVE_MKSTEMPS

/*
 * Stub for mkstemps
 */
int
mkstemps (
     char *template,
     int suffix_len)
{
    return( mkstemp(template) );
}

#endif
