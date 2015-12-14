/*
 * Read binary resource prototypes
 *
 * Copyright 1998 Bertho A. Stultiens (BS)
 *
 */

#ifndef __WRC_READRES_H
#define __WRC_READRES_H

#include <stdio.h>

#ifndef __WRC_WRCTYPES_H
#include "wrctypes.h"
#endif

resource_t *read_resfile(FILE *fp);

#endif
