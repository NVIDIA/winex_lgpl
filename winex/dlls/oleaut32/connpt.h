/* connpt.h
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2015 NVIDIA CORPORATION. All rights reserved.
 */
#ifndef _CONNPT_H
#define _CONNPT_H

HRESULT CreateConnectionPoint(IUnknown *pUnk, REFIID riid, IConnectionPoint **pCP);

#endif /* _CONNPT_H */
