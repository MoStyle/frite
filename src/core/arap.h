#ifndef ARAP_H
#define ARAP_H

#include "lattice.h"
#include "corner.h"
#include "editor.h"

using namespace Eigen;

namespace Arap {
    void regularizeQuad(QuadPtr q, PosTypeIndex dstPos);
    double regularizeQuads(Lattice &lattice, PosTypeIndex dstPos, bool forcePinPos);
    int regularizeLattice(Lattice &lattice, PosTypeIndex sourcePos, PosTypeIndex dstPos, int maxIterations, bool allGrid=false, bool convergenceStop=true, bool forcePinPos=false);
    void computeJAM(QuadPtr q, int i, int j, bool inverseOrientation, Matrix2d &A);
    double polarDecomp(Matrix2d &A, Matrix2d &S);
};

#endif  // ARAP_H
