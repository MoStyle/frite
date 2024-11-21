#include "arap.h"
#include "lattice.h"
#include "layer.h"
#include "dialsandknobs.h"
#include "playbackmanager.h"
#include "gridmanager.h"
#include "utils/stopwatch.h"

#include <iostream>

dkBool k_cornersFixed("Options->Grid->Exterior corners fixed", false);

#define EPSILON 0.001

using namespace Eigen;

// See Sykora et al. ARAP Image Registration for Hand-drawn Cartoon Animation (sec. 3.3)
void Arap::regularizeQuad(QuadPtr q, PosTypeIndex dstPos) {
    double a = 0;
    double b = 0;
    // double mu_part = 0;
    double weight = 0;

    // Compute the optimal rigid transform R, t from DEFORM_POS to dstPos
    for (int i = 0; i < 4; i++) {
        Corner *c = q->corners[i];
        Point::VectorType p_minus_pc = c->coord(INTERP_POS) - q->biasedCentroid(INTERP_POS);     // source pose
        Point::VectorType q_minus_qc = c->coord(dstPos) - q->biasedCentroid(dstPos);             // target pose
        a += q_minus_qc.dot(p_minus_pc);
        b += q_minus_qc.dot(Point::VectorType(-p_minus_pc.y(), p_minus_pc.x()));
        // mu_part += p_minus_pc.squaredNorm();
    }

    // If the quad is pinned we add the contribution of the pin to the minimization problem
    if (q->isPinned()) {
        Point::VectorType p_minus_pc = q->getPoint(q->pinUV(), INTERP_POS) - q->biasedCentroid(INTERP_POS);
        Point::VectorType q_minus_qc = q->pinPos() - q->biasedCentroid(dstPos);
        a += 10000.0 * q_minus_qc.dot(p_minus_pc);
        b += 10000.0 * q_minus_qc.dot(Point::VectorType(-p_minus_pc.y(), p_minus_pc.x()));
        // mu_part += p_minus_pc.squaredNorm();
    }

    double mu = sqrt(a * a + b * b);
    if (mu < EPSILON) mu = EPSILON;
    double r1 = a / mu;
    double r2 = -b / mu;
    Matrix2d R;
    R << r1, r2, -r2, r1;
    Point::VectorType t = q->biasedCentroid(dstPos) - R * q->biasedCentroid(INTERP_POS);


    // Transform corners and average
    for (int i = 0; i < 4; i++) {
        q->corners[i]->coord(DEFORM_POS) += ((R * q->corners[i]->coord(INTERP_POS) + t)) / double(q->corners[i]->nbQuads());
    }

    // Update centroids position
    q->computeCentroid(dstPos);
}

/**
 * Apply regularization on all quads, store the resulting position in dstPos.
 * Returns the maximum corner displacement (squared L2 norm)
 */
double Arap::regularizeQuads(Lattice &lattice, PosTypeIndex dstPos, bool forcePinPos) {
    // Compute ARAP deformation and average
    for (QuadPtr q : lattice.quads()) {
        regularizeQuad(q, dstPos);
    }

    if (forcePinPos) {
        lattice.displacePinsQuads(DEFORM_POS);
    }

    // Update positions and keep track of max displacement
    double maxDisp = 0;
    for (int i = 0; i < lattice.corners().size(); i++) {
        if (lattice.corners()[i]->isDeformable() && (!k_cornersFixed || lattice.corners()[i]->nbQuads() > 1)) {
            Vector2d tgt(lattice.corners()[i]->coord(dstPos).x(), lattice.corners()[i]->coord(dstPos).y());
            Vector2d nw(lattice.corners()[i]->coord(DEFORM_POS).x(), lattice.corners()[i]->coord(DEFORM_POS).y());
            double disp = (tgt - nw).squaredNorm();
            lattice.corners()[i]->coord(dstPos) = lattice.corners()[i]->coord(DEFORM_POS);
            if (disp > maxDisp) maxDisp = disp;
        }
        lattice.corners()[i]->coord(DEFORM_POS) = Point::VectorType::Zero();
    }

    return maxDisp;
}

/**
 * Iteratively regularize all quads in the lattice.
 * 
 * @param lattice 
 * @param sourcePos         The configuration of the lattice that the regularization converges to (up to a translation factor)
 * @param dstPos            Where the result configuration is stored
 * @param maxIterations     Maximum nb of iterations
 * @param allGrid           Regularize all the quads (override the deformable flag)
 * @param convergenceStop   Stops the iterative process when the maximum corner displacement falls under an hardcoded threshold. Otherwise run #maxIterations
 * @param forcePinPos       Guarantee that pinned quad contains their pin after regularization
 * @return                  Number of regularization iterations done
 */
int Arap::regularizeLattice(Lattice &lattice, PosTypeIndex sourcePos, PosTypeIndex dstPos, int maxIterations, bool allGrid, bool convergenceStop, bool forcePinPos) {
    if (maxIterations <= 0) {
        return 0;
    }

    Point::Affine scaling = sourcePos == DEFORM_POS ? Point::Affine::Identity() : lattice.scaling();

    // Initialization of interpolated position & source pos
    for (Corner *corner : lattice.corners()) {
        corner->coord(INTERP_POS) = scaling * corner->coord(sourcePos); // INTERP store the target position
        corner->coord(DEFORM_POS) = Point::VectorType::Zero();          // DEFORM store the temporary position (result of the iteration)
        if (allGrid) corner->setDeformable(true);
    }
    // Compute all quad centroids
    for (QuadPtr q : lattice.quads()) {
        q->computeCentroids();
    }

    // Apply regularization until convergence or a max number of iteration
    double maxDisp = 0;
    int i = 0;
    do {
        maxDisp = Arap::regularizeQuads(lattice, dstPos, forcePinPos);
        i++;
    } while (convergenceStop ? (i < maxIterations && sqrt(maxDisp) > 5e-3) : (i < maxIterations));

    // Save configuration for plastic deformation
    for (Corner *corner : lattice.corners()) {
        corner->coord(DEFORM_POS) = corner->coord(INTERP_POS);
    }

    return i;
}

/**
 * Computes "A" the transpose of the jacobian of the affine map between two triangles (ref pose vs target pose of a lattice cell) i.e. A is the linear part of the affine map between the two triangles.
 * A=P⁻1*Q   Eq. 2, Rigid Shape Interpolation Using Normal Equations, Baxter et al. 2008. i and j are corner indices used to determine which triangle of the quad we are using
 * 
 * @param q                     quad
 * @param i                     corner of the quad (!= BOTTOM_LEFT)
 * @param j                     corner of the quad (!= BOTTOM_LEFT)
 * @param inverseOrientation    if true the output linear transform goes from target to source (Q->P)
 * @param A                     output linear transform
 */
void Arap::computeJAM(QuadPtr q, int i, int j, bool inverseOrientation, Matrix2d &A) {
    Matrix2d P, Q;
    Point::VectorType qi, qj, qk, pi, pj, pk;

    // target pose
    qi = q->corners[i]->coord(TARGET_POS);
    qj = q->corners[j]->coord(TARGET_POS);
    qk = q->corners[BOTTOM_LEFT]->coord(TARGET_POS);
    Q << qi.x() - qk.x(), qi.y() - qk.y(), qj.x() - qk.x(), qj.y() - qk.y();

    // reference pose
    pi = q->corners[i]->coord(REF_POS);
    pj = q->corners[j]->coord(REF_POS);
    pk = q->corners[BOTTOM_LEFT]->coord(REF_POS);
    P << pi.x() - pk.x(), pi.y() - pk.y(), pj.x() - pk.x(), pj.y() - pk.y();

    if (inverseOrientation) {
        A = Q.inverse() * P;
    } else {
        A = P.inverse() * Q;
    }
}

/**
 * Computing the polar decomposition of A
 * 
 * @param A input linear transform matrix
 * @param S output shear matrix
 * @return rotation angle in rad
 */
double Arap::polarDecomp(Matrix2d &A, Matrix2d &S) {
    Matrix2d B = A.transpose();
    Matrix2d Rt;
    double angle = atan2(B(1, 0), B(0, 0));
    Rt << cos(angle), sin(angle), -sin(angle), cos(angle);
    S = Rt * B;
    return angle;
}