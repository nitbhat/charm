// Magic Software, Inc.
// http://www.magic-software.com
// Copyright (c) 2000-2003.  All Rights Reserved
//
// Source code from Magic Software is supplied under the terms of a license
// agreement and may not be copied or disclosed except in accordance with the
// terms of that agreement.  The various license agreements may be found at
// the Magic Software web site.  This file is subject to the license
//
// FREE SOURCE CODE
// http://www.magic-software.com/License/free.pdf

#ifndef MGCINTR3DTETRTETR_H
#define MGCINTR3DTETRTETR_H

#include "MgcTetrahedron.h"
#include <vector>

namespace Mgc {

MAGICFM void FindIntersection (const Tetrahedron& rkT0,
    const Tetrahedron& rkT1, std::vector<Tetrahedron>& rkIntr);

}  // namespace Mgc

#endif



