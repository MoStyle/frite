/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "registrationmanager.h"
#include "vectorkeyframe.h"
#include "group.h"
#include "arap.h"
#include "utils/stopwatch.h"
#include "utils/utils.h"
#include "dialsandknobs.h"

#include <Eigen/Geometry>
#include <cpd/rigid.hpp>
#include <cpd/affine.hpp>
#include <cpd/gauss_transform_fgt.hpp>
#include <unordered_set>

// REGISTRATION PARAMETERS
dkInt k_registrationIt("Options->Registration->Iterations", 10, 0, 1000, 1);
dkInt k_registrationRegularizationIt("Options->Registration->Regularization iterations", 20, 0, 1000, 1);
static dkBool k_useRegularisationStoppingCriterion("Options->Registration->Regularization stopping criterion", false);
static dkBool k_useFGT("Options->Registration->Use FGT", false);
static dkInt k_cpdIt("Options->Registration->CPD iterations", 10, 0, 200, 1);
static dkFloat k_proximityFactor("Options->Registration->Proximity factor", 3.0, 0.1, 50, 0.1);
static dkFloat k_stepSize("Options->Registration->Step size", 1.0, 0.001, 1.0, 0.001);
static dkBool k_useCoverageCriterion("Warp->Use coverage local criterion", false);
extern dkInt k_cellSize;

RegistrationManager::RegistrationManager(QObject* pParent) : BaseManager(pParent) {
    m_registrationTargetKey = nullptr;
    m_registrationKDTree = nullptr;
}

void RegistrationManager::preRegistration(Group *source, PosTypeIndex type) {
    StopWatch sw("Pre registration");
    source->lattice()->resetDeformation();
    if (k_cpdIt > 0) rigidCPD({{source->id(), source}});
    sw.stop();
}

void RegistrationManager::preRegistration(const QHash<int, Group *> &groups, PosTypeIndex type) {
    StopWatch sw("Pre registration");
    for (Group *group : groups) {
        group->lattice()->resetDeformation();
    }
    rigidCPD(groups);
    sw.stop();
}

void RegistrationManager::registration(Group *source, PosTypeIndex type, PosTypeIndex regularizationSource, bool usePreRegistration) {
    registration(source, type, regularizationSource, usePreRegistration, k_registrationIt, k_registrationRegularizationIt);
}

// TODO: maybe add an overloaded method that takes directly a vector of points instead of a StrokeIntervals
// Given a group and a target set of strokes, computes the group's lattice TARGET_POS that best aligns with the target strokes
// If updateSource is true, the regularization will converge towards the deformed configuration of the lattice
void RegistrationManager::registration(Group *source, PosTypeIndex type, PosTypeIndex regularizationSource, bool usePreRegistration, int registrationIt, int regularizationIt) {
    StopWatch sw0("Registration");
    if (source == nullptr || source->lattice() == nullptr || m_registrationTargetKey == nullptr || m_registrationTargetPoints.empty()) return;

    if (usePreRegistration) {
        preRegistration(source, type);
        regularizationSource = TARGET_POS;
    }

    if (registrationIt == 0) {
        source->lattice()->setArapDirty();
        source->lattice()->setBackwardUVDirty(true);
        sw0.stop();
        return;
    }

    // Set the source of the regularization
    if (regularizationSource != INTERP_POS) {
        source->lattice()->copyPositions(source->lattice(), regularizationSource, INTERP_POS);
    }

    // Alternate an iterative push phase and lattice regularization until -convergence- or a fixed max number of iterations
    StopWatch sw1("Main loop");
    int it = 0;
    do {
        StopWatch sw2("Push phase");
        if (k_useCoverageCriterion) pushPhaseWithCoverage(source);
        else                        pushPhaseWithoutCoverage(source);
        sw2.stop();
        StopWatch sw3("Regularization phase");
        Arap::regularizeLattice(*source->lattice(), INTERP_POS, type, regularizationIt, true, k_useRegularisationStoppingCriterion);
        sw3.stop();
        ++it;
    } while(it < registrationIt);
    sw1.stop();

    // Dirty flag for ARAP interpolation (recompute prefactorized matrix)
    source->lattice()->setArapDirty();
    source->lattice()->setBackwardUVDirty(true);
    sw0.stop();
}

void RegistrationManager::setRegistrationTarget(VectorKeyFrame *targetKey) {
    m_registrationTargetKey = targetKey;
    // Point::Affine globalRigidTransformInverse = m_registrationTargetKey->rigidTransform(1.0f).inverse();
    m_registrationTargetPoints.clear();
    m_registrationTargetPointsCM = Point::VectorType::Zero();
    for (const StrokePtr &stroke : m_registrationTargetKey->strokes()) {
        for (Point *point : stroke->points()) {
            m_registrationTargetPoints.push_back(point);
            m_registrationTargetPointsCM += m_registrationTargetPoints.back()->pos();
        }
    }
    m_registrationTargetPointsCM /= m_registrationTargetPoints.size();
    resetKDTree();
}

void RegistrationManager::setRegistrationTarget(VectorKeyFrame *targetKey, StrokeIntervals &targetStrokes) {
    m_registrationTargetKey = targetKey;
    Point::Affine globalRigidTransformInverse = m_registrationTargetKey->rigidTransform(1.0f).inverse();
    m_registrationTargetPoints.clear();
    m_registrationTargetPointsCM = Point::VectorType::Zero();
    targetStrokes.forEachPoint(m_registrationTargetKey, [&](Point *point) {
        m_registrationTargetPoints.push_back(point);
        m_registrationTargetPointsCM += m_registrationTargetPoints.back()->pos();
    });
    m_registrationTargetPointsCM /= m_registrationTargetPoints.size();
    resetKDTree();
}

void RegistrationManager::setRegistrationTarget(VectorKeyFrame *targetKey, const std::vector<Point *> &targetPos) {
    m_registrationTargetKey = targetKey;
    m_registrationTargetPoints.clear();
    m_registrationTargetPointsCM = Point::VectorType::Zero();
    m_registrationTargetPoints.reserve(targetPos.size());
    for (Point *p : targetPos) {
        m_registrationTargetPoints.push_back(p);
        m_registrationTargetPointsCM += m_registrationTargetPoints.back()->pos();
    }
    m_registrationTargetPointsCM /= m_registrationTargetPoints.size();
    resetKDTree();
}

void RegistrationManager::clearRegistrationTarget() { 
    m_registrationTargetKey = nullptr;
    m_registrationTargetPoints.clear(); 
}

void RegistrationManager::alignCenterOfMass(Group *source) {
    Point::VectorType diff = m_registrationTargetPointsCM - source->lattice()->refCM();
    source->lattice()->applyTransform(Point::Affine(Point::Translation(diff)), REF_POS, TARGET_POS);
}

void RegistrationManager::rigidCPD(const QHash<int, Group *> &groups) {
    cpd::Matrix targetMatrix; 
    cpd::Matrix sourceMatrix;
    Point::VectorType sourceCenterOfMass = Point::VectorType::Zero();

    // Fill targetMatrix
    targetMatrix.conservativeResize(m_registrationTargetPoints.size(), 2);
    for (int i = 0; i < m_registrationTargetPoints.size(); ++i) {
        targetMatrix.row(i) = m_registrationTargetPoints[i]->pos();
    }

    // Fill sourceMatrix
    int r = 0;
    int rows = 0;
    for (Group *group : groups) {
        rows += group->nbPoints();
    }
    sourceMatrix.conservativeResize(rows, 2);
    for (Group *group : groups) {
        group->strokes().forEachPoint(group->getParentKeyframe(), [&sourceMatrix, &r, &sourceCenterOfMass](Point *p) {
            sourceMatrix.row(r++) = p->pos();
            sourceCenterOfMass += p->pos();
        });
    }
    sourceCenterOfMass /= rows;

    // Compute optimal similarity transform
    cpd::Rigid rigid;
    if (k_useFGT) {
        rigid.gauss_transform(std::unique_ptr<cpd::GaussTransform>(new cpd::GaussTransformFgt())); // For FGT?
    }
    rigid.max_iterations(k_cpdIt);
    rigid.scale(true);
    cpd::RigidResult result = rigid.run(targetMatrix, sourceMatrix);
    Point::Affine resultTransform;
    resultTransform.matrix() = result.matrix();

    // Apply the transform to the source positions and store the result in the target positions
    for (Group *group : groups) {
        group->lattice()->applyTransform(resultTransform, REF_POS, TARGET_POS);
    }
}

/**
 * Move each quad towards the closest stroke patch in the set of target strokes.
 * This displacement does *not* preserve the rigidity of the lattice.
 * @param source the group to push
 */
void RegistrationManager::pushPhaseWithoutCoverage(Group *source) {
    const UVHash &uvs = source->uvs();

    // NN init
    Point::VectorType queryPoint, quadDisp;
    nanoflann::KNNResultSet<Point::Scalar> nnResult(1);
    size_t nnIdx; 
    Point::Scalar nnDistSq;

    size_t prevNNIdx, pointsMatched;
    const Point::Scalar cellSq = source->lattice()->cellSize() * source->lattice()->cellSize();
    const Point::Scalar searchRadiusSq = k_proximityFactor * k_proximityFactor * cellSq; // search radius = k_proximityFactor*k_cellSize
    bool found = false;
    bool diffNeighbor = false;

    // Optimal rigid transform init
    Point::VectorType sourceCenter, targetCenter, p, q, t;
    std::vector<std::pair<Point::VectorType, Point::VectorType>> matchedPoints; // List of matched point pairs (reset for each quad)
    double a, b, r1, r2, mu;
    Matrix2d R;
    Point::Affine optimalRigid;

    // All computation are done in DEFORM_POS so we initialize it with the TARGET_POS 
    for (Corner *corner : source->lattice()->corners()) {
        corner->coord(DEFORM_POS) = corner->coord(TARGET_POS);
    }

    sourceCenter = Point::VectorType::Zero();
    targetCenter = Point::VectorType::Zero();

    // Move every quad in the lattice
    for (QuadPtr quad : source->lattice()->hash()) {
        quadDisp = Point::VectorType::Zero();
        diffNeighbor = false;
        pointsMatched = 0;
        prevNNIdx = -1;
        a = b = 0.0;
        sourceCenter = targetCenter = Point::VectorType::Zero();
        matchedPoints.clear();

        // For each point in the quad, find the closest point in targetStrokes and accumulate the displacement vector
        quad->elements().forEachPoint(source->getParentKeyframe(), [&](Point *point, unsigned int sId, unsigned int pId) {
            UVInfo uv = uvs.get(sId, pId);
            queryPoint = source->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, TARGET_POS);
            nnResult.init(&nnIdx, &nnDistSq);
            found = m_registrationKDTree->findNeighbors(nnResult, &queryPoint[0], nanoflann::SearchParameters(10));

            // If we've found a neighbor and it is in the search radius, we keep its and the query point positions 
            if (nnResult.size() >= 1 && nnDistSq <= searchRadiusSq) {
                if (prevNNIdx != -1 && nnIdx != prevNNIdx) diffNeighbor = true; // diffNeighbor is false if all points in the quad share the same NN
                sourceCenter += queryPoint;
                targetCenter += m_registrationTargetPoints[nnIdx]->pos();
                prevNNIdx = nnIdx;
                matchedPoints.push_back({queryPoint, m_registrationTargetPoints[nnIdx]->pos()});
                pointsMatched += 1;
            }
        });

        if (pointsMatched == 0) continue;

        sourceCenter /= pointsMatched;
        targetCenter /= pointsMatched;

        // In these cases the optimal rigid transform is a pure translation (one point matched or all points matched to the same point)
        if (pointsMatched == 1 || !diffNeighbor) {
            // move the quad
            for (size_t i = 0; i < 4; ++i) {
                Corner *corner = quad->corners[i];
                Point::VectorType quadDisp = (targetCenter - sourceCenter) * k_stepSize;
                corner->coord(DEFORM_POS) += quadDisp / Point::Scalar(corner->nbQuads());
            }
            continue;
        }

        // Compute optimal rigid transform between the two point clouds (closed-form formula) 
        for (size_t i = 0; i < matchedPoints.size(); ++i) {
            p = matchedPoints[i].first - sourceCenter;
            q = matchedPoints[i].second - targetCenter;
            a += q.dot(p);
            b += q.dot(Point::VectorType(-p.y(), p.x()));
        }
        mu = sqrt(a * a + b * b);
        if (mu < 0.01) mu = 0.01;
        r1 = a / mu;
        r2 = -b / mu;
        R << r1, r2, -r2, r1;
        t = targetCenter - R * sourceCenter;

        optimalRigid = Point::Translation(t) * Point::Rotation(R);

        // Move the quad corners (and average for all neighbors)
        for (size_t i = 0; i < 4; ++i) {
            Corner *corner = quad->corners[i];
            Point::VectorType transformedPos = optimalRigid * corner->coord(TARGET_POS);
            Point::VectorType quadDisp = (transformedPos - corner->coord(TARGET_POS)) * k_stepSize;
            corner->coord(DEFORM_POS) += quadDisp / Point::Scalar(corner->nbQuads());
        }
    }
   
    // Copy result
    for (Corner *corner : source->lattice()->corners()) {
        corner->coord(TARGET_POS) = corner->coord(DEFORM_POS);
    }
}

/**
 * Move each quad towards the closest stroke patch in the set of target strokes that has not already been matched with a quad.
 * This displacement does *not* preserve the rigidity of the lattice.
 * @param source the group to push
 */
void RegistrationManager::pushPhaseWithCoverage(Group *source) {
    const UVHash &uvs = source->uvs();

    // Nearest-neighbor search init
    Point::VectorType queryPoint, quadDisp;

    nanoflann::KNNResultSet<Point::Scalar> nnResultPreProcess(1);
    size_t nnIdxPreProcess; 
    Point::Scalar nnDistSqPreProcess;

    nanoflann::KNNResultSet<Point::Scalar> nnResult(50);
    size_t nnIdx[50]; 
    Point::Scalar nnDistSq[50];

    size_t prevNNIdx, pointsMatched;
    const Point::Scalar cellSq = source->lattice()->cellSize() * source->lattice()->cellSize();
    const Point::Scalar searchRadiusSq = k_proximityFactor * k_proximityFactor * cellSq; // search radius = k_proximityFactor*k_cellSize
    bool found = false;
    bool diffNeighbor = false;

    // Optimal rigid transform init
    Point::VectorType sourceCenter, targetCenter, p, q, t;
    std::vector<std::pair<Point::VectorType, Point::VectorType>> matchedPoints; // List of matched point pairs (reset for each quad)
    double a, b, r1, r2, mu;
    Matrix2d R;
    Point::Affine optimalRigid;

    // All computation are done in DEFORM_POS so we initialize it with the TARGET_POS 
    for (Corner *corner : source->lattice()->corners()) {
        corner->coord(DEFORM_POS) = corner->coord(TARGET_POS);
    }

    sourceCenter = Point::VectorType::Zero();
    targetCenter = Point::VectorType::Zero();

    // Find registration order
    std::vector<std::pair<int, double>> quadIdxOrder;
    quadIdxOrder.reserve(source->lattice()->size());
    for (QuadPtr quad : source->lattice()->hash()) {
        quad->computeCentroid(TARGET_POS);
        queryPoint = quad->centroid(TARGET_POS);
        nnResultPreProcess.init(&nnIdxPreProcess, &nnDistSqPreProcess);
        found = m_registrationKDTree->findNeighbors(nnResultPreProcess, &queryPoint[0], nanoflann::SearchParameters(10));
        if (found && nnDistSqPreProcess <= searchRadiusSq) {
            quadIdxOrder.push_back({quad->key(), nnDistSqPreProcess});
        } else {
            quadIdxOrder.push_back({quad->key(), 99999999999.0});
        }
    }
    std::sort(quadIdxOrder.begin(), quadIdxOrder.end(), [](auto &a, auto &b) { return a.second < b.second; });

    // Move every quad in the lattice
    std::unordered_set<int> usedIdx;
    for (const auto& el : quadIdxOrder) {
        QuadPtr quad = source->lattice()->quad(el.first);
        quadDisp = Point::VectorType::Zero();
        diffNeighbor = false;
        pointsMatched = 0;
        prevNNIdx = -1;
        a = b = 0.0;
        sourceCenter = targetCenter = Point::VectorType::Zero();
        matchedPoints.clear();

        // For each point in the quad, find the closest point in targetStrokes and accumulate the displacement vector
        quad->elements().forEachPoint(source->getParentKeyframe(), [&](Point *point, unsigned int sId, unsigned int pId) {
            UVInfo uv = uvs.get(sId, pId);
            queryPoint = source->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, TARGET_POS);
            nnResult.init(nnIdx, nnDistSq);
            found = m_registrationKDTree->findNeighbors(nnResult, &queryPoint[0], nanoflann::SearchParameters(10));

            // find the closest non-visited point 
            int idx = -1;
            for (int i = 0; i < nnResult.size(); ++i) {
                if (usedIdx.find(nnIdx[i]) == usedIdx.end()) {
                    idx = i;
                    break;
                }
            }

            // If we've found a neighbor and it is in the search radius, we keep its and the query point positions 
            if (idx >= 0 && nnResult.size() >= 1 && nnDistSq[idx] <= searchRadiusSq) {
                if (prevNNIdx != -1 && nnIdx[idx] != prevNNIdx) diffNeighbor = true; // diffNeighbor is false if all points in the quad share the same NN
                sourceCenter += queryPoint;
                targetCenter += m_registrationTargetPoints[nnIdx[idx]]->pos();
                prevNNIdx = nnIdx[idx];
                matchedPoints.push_back({queryPoint, m_registrationTargetPoints[nnIdx[idx]]->pos()});
                pointsMatched += 1;
            }
        });

        if (pointsMatched == 0) continue;

        sourceCenter /= pointsMatched;
        targetCenter /= pointsMatched;

        // In these cases the optimal rigid transform is a pure translation (one point matched or all points matched to the same point)
        if (pointsMatched == 1 || !diffNeighbor) {
            // move the quad
            for (size_t i = 0; i < 4; ++i) {
                Corner *corner = quad->corners[i];
                Point::VectorType quadDisp = (targetCenter - sourceCenter) * k_stepSize;
                corner->coord(DEFORM_POS) += quadDisp / Point::Scalar(corner->nbQuads());
            }

            // Mark vertices as visited
            quad->computeCentroid(DEFORM_POS);
            queryPoint = quad->centroid(DEFORM_POS);
            nnResult.init(nnIdx, nnDistSq);
            found = m_registrationKDTree->findNeighbors(nnResult, &queryPoint[0], nanoflann::SearchParameters(10));
            for (int i = 0; i < nnResult.size(); ++i) {
                if (nnDistSq[i] <= cellSq) {
                    usedIdx.insert(nnIdx[i]);
                }
            }
            continue;
        }

        // Compute optimal rigid transform between the two point clouds (closed-form formula) 
        for (size_t i = 0; i < matchedPoints.size(); ++i) {
            p = matchedPoints[i].first - sourceCenter;
            q = matchedPoints[i].second - targetCenter;
            a += q.dot(p);
            b += q.dot(Point::VectorType(-p.y(), p.x()));
        }
        mu = sqrt(a * a + b * b);
        if (mu < 0.01) mu = 0.01;
        r1 = a / mu;
        r2 = -b / mu;
        R << r1, r2, -r2, r1;
        t = targetCenter - R * sourceCenter;

        optimalRigid = Point::Translation(t) * Point::Rotation(R);

        // Move the quad corners (and average for all neighbors)
        for (size_t i = 0; i < 4; ++i) {
            Corner *corner = quad->corners[i];
            Point::VectorType transformedPos = optimalRigid * corner->coord(TARGET_POS);
            Point::VectorType quadDisp = (transformedPos - corner->coord(TARGET_POS)) * k_stepSize;
            corner->coord(DEFORM_POS) += quadDisp / Point::Scalar(corner->nbQuads());
        }

        // Mark vertices as visited
        quad->computeCentroid(DEFORM_POS);
        queryPoint = quad->centroid(DEFORM_POS);
        nnResult.init(nnIdx, nnDistSq);
        found = m_registrationKDTree->findNeighbors(nnResult, &queryPoint[0], nanoflann::SearchParameters(10));
        for (int i = 0; i < nnResult.size(); ++i) {
            if (nnDistSq[i] <= cellSq) {
                usedIdx.insert(nnIdx[i]);
            }
        }
    }

    // Copy result
    for (Corner *corner : source->lattice()->corners()) {
        corner->coord(TARGET_POS) = corner->coord(DEFORM_POS);
    }
}

/**
 * Embed the current list of target points in a kd-tree
 */
void RegistrationManager::resetKDTree() {
    m_registrationDataset.reset(new DatasetAdaptorPoint(m_registrationTargetPoints));
    m_registrationKDTree.reset(new KDTree(2, *m_registrationDataset, nanoflann::KDTreeSingleIndexAdaptorParams(10)));
}
