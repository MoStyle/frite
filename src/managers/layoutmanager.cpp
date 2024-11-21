#include "layoutmanager.h"

#include "vectorkeyframe.h"
#include "group.h"
#include "mask.h"
#include "utils/utils.h"

#include <clipper2/clipper.h>

/**
 * Optimizations: 
 * - Perform radius search first and tag all points that have at least one match
 * - Use tagged points to determine which point visibility need to be computed
 */

LayoutManager::LayoutManager(QObject* pParent) : BaseManager(pParent), m_nbVerticesA(0), m_nbVerticesB(0) {
    
}

/**
 * Given 2 successive keyframes A and B and their layout L_A and L_B, this method tries to find if there exists a layout of A  L_A' != L_A
 * such that L_A' has a lower visibility score than L_A.
 * Return the visibility score of L_A' if it is the optimal layout, otherwise return a negative value if L_A is already the optimal layout.
 * The optimal layout is returned via the "optimalLayout" parameter
 * 
 * TODO: Use the last partial layout change as the baseline layout?
 */
double LayoutManager::computeBestLayout(VectorKeyFrame *A, VectorKeyFrame *B, GroupOrder &optimalLayout) {
    // Precompute caches
    int maxIdA = A->postGroups().lastKey() + 2;
    int maxIdB = B->postGroups().lastKey() + 2;
    int stride = A->parentLayer()->stride(A->keyframeNumber());
    unsigned int nbVerticesA, nbVerticesB;
    m_maskMaskIntersectionMatrixA = LayoutAdjacencyMatrix::Zero(maxIdA, maxIdA);
    m_maskMaskIntersectionMatrixB = LayoutAdjacencyMatrix::Zero(maxIdB, maxIdB);
    m_maskBins = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>::Zero(maxIdA, maxIdB);
    computeMaskVertexIntersectionCache(A, stride, m_maskVertexIntersectionCacheA, m_maskConnectedComponentCacheA, m_maskMaskIntersectionMatrixA);
    computeMaskVertexIntersectionCache(B, 0, m_maskVertexIntersectionCacheB, m_maskConnectedComponentCacheB, m_maskMaskIntersectionMatrixB);
    m_nbVerticesA = A->inbetween(stride).nbVertices;
    m_nbVerticesB = B->inbetween(0).nbVertices;
    makeKDTree(B, 0);
    // TODO: make sure group id is baked in points

    // Get visibility score for the baseline layout
    std::unordered_map<int, double> groupScores;
    double baselineScore = getLayoutScore(A, B, A->orderPartials().firstPartial().groupOrder().order(), B->orderPartials().firstPartial().groupOrder().order(), stride, 0, groupScores);

    qDebug() << "mask bins " << m_maskBins.rows() << ", " << m_maskBins.cols() << " : ";
    std::cout << m_maskBins << std::endl;
    // TODO: normalize bins by vtx count in B? i.e how much is gA covering gB

    // Compute explicit matching between A to B (many to one) based on stroke coverage
    std::vector<int> noCorresp;
    std::unordered_map<int, std::vector<int>> BtoACorrespondence = computeExactMatchingBtoA(A, noCorresp);

    // Get new layout for A based on its coverage of B
    GroupOrder coverageBasedLayout = buildMatchingBasedLayout(A, B, BtoACorrespondence, noCorresp);

    double coverageBasedLayoutScore = getLayoutScore(A, B, coverageBasedLayout.order(), B->orderPartials().firstPartial().groupOrder().order(), stride, 0, groupScores);

    // TODO: simplify layout by collapsing adjacent depths that do not contain intersecting masks

    optimalLayout = coverageBasedLayoutScore < baselineScore ? coverageBasedLayout : A->orderPartials().firstPartial().groupOrder();
    qDebug() << "coverageBasedLayoutScore " << coverageBasedLayoutScore << " vs baselineScore" << baselineScore;
    return (coverageBasedLayoutScore < baselineScore) ? coverageBasedLayoutScore : -1.0;
}

/**
 * Return the optimal inbetween frame (relative to keyframe A) for the given dynamic layout change 
 */
int LayoutManager::computeBestLayoutChangeLocation(VectorKeyFrame *A, const GroupOrder &layoutA) {
    int stride = A->parentLayer()->stride(A->keyframeNumber());
    int maxIdA = A->postGroups().lastKey() + 2;

    std::unordered_map<int, double> groupScores;
    double minScore  = std::numeric_limits<double>::max();
    double curScore;
    int optimalInbetween = stride;
    
    // Check layout score for each inbetween
    for (int i = 0; i < stride; ++i) { // TODO: what would it mean if the optimal layout t is right before the keyframe switch (i.e. i == stride) or even at i == 0?
        m_maskMaskIntersectionMatrixA = LayoutAdjacencyMatrix::Zero(maxIdA, maxIdA);
        m_maskMaskIntersectionMatrixB = LayoutAdjacencyMatrix::Zero(maxIdA, maxIdA);
        m_maskBins = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>::Zero(maxIdA, maxIdA);
        computeMaskVertexIntersectionCache(A, i, m_maskVertexIntersectionCacheA, m_maskConnectedComponentCacheA, m_maskMaskIntersectionMatrixA);
        computeMaskVertexIntersectionCache(A, i, m_maskVertexIntersectionCacheB, m_maskConnectedComponentCacheB, m_maskMaskIntersectionMatrixB);
        makeKDTree(A, i);

        curScore = getLayoutScore(A, A, A->orderPartials().firstPartial().groupOrder().order(), layoutA.order(), i, i, groupScores);

        qDebug() << "   score at inbetween " << i << " = " << curScore;
        
        if (curScore < minScore) {
            minScore = curScore;
            optimalInbetween = i;
        }
    }

    qDebug() << "return best inbetween is " << optimalInbetween << " | stride = " << stride;

    return optimalInbetween;
}

/**
 * Return a new layout for keyframe B based on how it is matched with the strokes of keyframe A.
 */
GroupOrder LayoutManager::propagateLayoutAtoB(VectorKeyFrame *A, VectorKeyFrame *B) {
    int maxIdA = A->postGroups().lastKey() + 2;
    int maxIdB = B->postGroups().lastKey() + 2;
    m_maskMaskIntersectionMatrixA = LayoutAdjacencyMatrix::Zero(maxIdA, maxIdA);
    m_maskMaskIntersectionMatrixB = LayoutAdjacencyMatrix::Zero(maxIdB, maxIdB);
    m_maskBins = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>::Zero(maxIdA, maxIdB);

    computeMaskBins(A, B);

    // Compute explicit matching from A to B (many to one) based on stroke coverage
    std::vector<int> noCorresp;
    std::unordered_map<int, std::vector<int>> AtoBCorrespondence = computeExactMatchingAtoB(B, noCorresp);
    
    generateAllLayouts(B);

    // Compute and return the new layout for B
    GroupOrder newBLayout = buildInverseMatchingBasedLayout(A, B, AtoBCorrespondence, noCorresp);
    return newBLayout;
}

/**
 * Return a new layout for keyframe A based on how it is matched with the strokes of keyframe B.
 */
GroupOrder LayoutManager::propagateLayoutBtoA(VectorKeyFrame *A, VectorKeyFrame *B) {
    int maxIdA = A->postGroups().lastKey() + 2;
    int maxIdB = B->postGroups().lastKey() + 2;
    m_maskMaskIntersectionMatrixA = LayoutAdjacencyMatrix::Zero(maxIdA, maxIdA);
    m_maskMaskIntersectionMatrixB = LayoutAdjacencyMatrix::Zero(maxIdB, maxIdB);
    m_maskBins = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>::Zero(maxIdA, maxIdB);

    computeMaskBins(A, B);

    // Compute explicit matching from B to A (many to one) based on stroke coverage
    std::vector<int> noCorresp;
    std::unordered_map<int, std::vector<int>> BtoACorrespondence = computeExactMatchingBtoA(A, noCorresp);
    
    // Compute and return the new layout for A
    GroupOrder newBLayout = buildMatchingBasedLayout(A, B, BtoACorrespondence, noCorresp);
    return newBLayout;
}

/**
 * Return the set of occluded stroke vertices at given inbetween based on the keyframe layout.
 */
std::unordered_set<unsigned int> LayoutManager::getOccludedVertices(VectorKeyFrame *keyframe, int inbetween) {
    // Precompute caches
    int maxId = keyframe->postGroups().lastKey() + 2;
    int stride = keyframe->parentLayer()->stride(keyframe->keyframeNumber());
    double alpha = inbetween / stride;
    m_maskMaskIntersectionMatrixA = LayoutAdjacencyMatrix::Zero(maxId, maxId);
    computeMaskVertexIntersectionCache(keyframe, stride, m_maskVertexIntersectionCacheA, m_maskConnectedComponentCacheA, m_maskMaskIntersectionMatrixA);
    return computeOccludedVertices(keyframe, keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order(), m_maskVertexIntersectionCacheA, inbetween);
}

/**
 * Return the sum of the absolute value of visibility scores in A. 
 * The visibility score of $v_A$, a stroke vertex in A, is the average difference of visibility between $v_A$ and all vertices in B that are within a fixed radius of $v_A$.
 * The visibility of a vertex is either 0 or 1, therefore the visibility score of a vertex is a real number in [-1, 1]
 * The visibility score (signed) for each group is stored in the map given as parameter.
 * 
 * TODO: select interp time of A (default is t=1)
 */
double LayoutManager::getLayoutScore(VectorKeyFrame *A, VectorKeyFrame *B, const Layout &layoutA, const Layout &layoutB, int inbetweenA, int inbetweenB, std::unordered_map<int, double> &groupScores) {
    std::unordered_set<unsigned int> visibilityA = computeOccludedVertices(A, layoutA, m_maskVertexIntersectionCacheA, inbetweenA);
    std::unordered_set<unsigned int> visibilityB = computeOccludedVertices(B, layoutB, m_maskVertexIntersectionCacheB, inbetweenB);
    groupScores.clear();

    // const double radSq = 10.0;
    double radSq = 10.0;
    std::vector<std::pair<size_t, Point::Scalar>> res;
    const Inbetween &inb = A->inbetween(inbetweenA);
    const Inbetween &inb0 = A->inbetween(0);
    double score = 0.0, scoreAbs = 0.0, diffAbs = 0.0, diff = 0.0;

    // Compute score for each group of A
    for (Group *group : A->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            if (group->size() > 0) groupScores[group->id()] = 0.0;
            const StrokePtr &stroke = inb.strokes[it.key()];
            double rad = stroke->strokeWidth() + 2; 
            radSq = rad * rad;
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    unsigned int count = m_treeTarget.kdtree->radiusSearch(&stroke->points()[i]->pos()[0], radSq, res, nanoflann::SearchParams(10));
                    diff = 0.0;
                    diffAbs = 0.0;
                    for (unsigned int j = 0; j < count; ++j) {
                        m_maskBins(group->id() + 1, m_treeTarget.data[res[j].first]->groupId() + 1) += 1;
                        diff += (int)(visibilityB.find(m_dataKey[res[j].first]) != visibilityB.end()) - (int)(visibilityA.find(Utils::cantor(stroke->id(), i)) != visibilityA.end());
                        diffAbs += std::abs(diff);
                    }
                    if (count > 0) {
                        diff /= count;
                        diffAbs /= count;
                    } else if (visibilityA.find(Utils::cantor(stroke->id(), i)) == visibilityA.end()) { // Vertex has no match in B but is visible => it will pop out so we need to penalize it
                        diff = 5;
                        diffAbs = 5;
                    }                    
                    score += diff;
                    scoreAbs += diffAbs;
                    groupScores[group->id()] += diffAbs;
                    QColor c = QColor(diffAbs < 0.1 ? 0.0 : (128 + diffAbs * 5), 0, 0);
                    stroke->points()[i]->setColor(c);
                    if (inbetweenA == 0) {
                        inb0.strokes[it.key()]->points()[i]->setColor(c);
                        A->strokes().value(it.key())->points()[i]->setColor(c);
                    }
                    // stroke->points()[i]->setTemporalW(diffAbs);
                }
            }
        }
    }

    qDebug() << "score: " << score;
    qDebug() << "scoreAbs: " << scoreAbs;
    return scoreAbs;
}

/**
 * Compute the visibility of every stroke vertices of the given keyframe based on the given mask layout.
 */
std::unordered_set<unsigned int> LayoutManager::computeOccludedVertices(VectorKeyFrame *keyframe, const Layout &layout, const std::unordered_map<unsigned int, std::vector<int>> &maskVertexIntersectionCache, int inbetween) {
    unsigned int key;
    unsigned int totalVertices = 0;
    std::unordered_set<unsigned int> occludedVertices;
    const Inbetween &inb = keyframe->inbetween(inbetween);
    LayoutAdjacencyMatrix adj = computeLayoutAdjacencyMatrix(layout);

    // Using the precomputed vertex-mask intersection cache and the layout adjacency matrix, test if a vertex is visible or not.
    for (Group *group: keyframe->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const StrokePtr &stroke = inb.strokes[it.key()];
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    key = Utils::cantor(it.key(), i);
                    totalVertices++;
                    auto vertexIntersections = maskVertexIntersectionCache.find(key);
                    if (vertexIntersections != maskVertexIntersectionCache.end()) { // vertex intersects a mask (except its own)
                        for (int groupId : vertexIntersections->second) {
                            if (adj(groupId + 1, group->id() + 1) == 1) {
                                occludedVertices.insert(key);   
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "occluded vertices: " << occludedVertices.size() << "/" << totalVertices << std::endl; 
    return occludedVertices; // moved by return value optimization
}

/**
 * Precompute a cache storing all the stroke vertex -> mask polygon intersections.
 * A stroke vertex can intersect multiple masks.
 * Masks are represented with their group id.
 * Use the parameter nbVertices if you know in advance the number of vertices of the keyframe, otherwise keep it at -1
 * 
 * Return the number of vertices in the keyframe.
 */
void LayoutManager::computeMaskVertexIntersectionCache(VectorKeyFrame *keyframe, int inbetween, std::unordered_map<unsigned int, std::vector<int>> &maskVertexIntersectionCache, std::vector<std::set<int>> &maskConnectedComponentCache, Eigen::MatrixX<bool> &maskMaskIntersectionMatrix) {
    std::unordered_map<int, std::set<int>> maskMaskIntersection;
    maskConnectedComponentCache.clear();
    maskMaskIntersectionMatrix.setZero();
    maskVertexIntersectionCache.clear();
    int stride = keyframe->parentLayer()->stride(keyframe->keyframeNumber());
    m_editor->updateInbetweens(keyframe, inbetween, stride);
    const Inbetween &inb = keyframe->inbetween(inbetween);
    maskVertexIntersectionCache.reserve(inb.nbVertices);

    // Compute masks outline at t=1 as clipper2 paths
    QHash<int, Clipper2Lib::PathD> masks;
    for (Group *group : keyframe->postGroups()) {
        if (group->size() == 0) continue;
        if (group->mask()->isDirty()) group->mask()->computeOutline();
        masks.insert(group->id(), Clipper2Lib::PathD());
        for (int i = 0; i < group->mask()->polygon().size(); ++i) {
            const Clipper2Lib::PointD &point = group->mask()->polygon().at(i);
            Point::VectorType warpedPoint = inb.getWarpedPoint(group, {group->mask()->vertexInfo().at(i).quadKey, group->mask()->vertexInfo().at(i).uv});
            Clipper2Lib::PointD p(warpedPoint.x(), warpedPoint.y());
            masks[group->id()].push_back(p);
        }
    }

    // For each stroke vertex, test if it is inside another group's mask. If it does, store that information in the cache
    unsigned int key;
    Clipper2Lib::PointD pClipper;
    Point *p;
    for (Group *group : keyframe->postGroups()) {
        maskMaskIntersection[group->id()].insert(group->id());
        if (group->size() == 0) continue;
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const StrokePtr &stroke = inb.strokes[it.key()];
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    key = Utils::cantor(it.key(), i);
                    p = stroke->points()[i];
                    pClipper = Clipper2Lib::PointD(p->pos().x(), p->pos().y());
                    for (Group *groupTest : keyframe->postGroups()) {
                        if (group == groupTest) continue;
                        auto res = Clipper2Lib::PointInPolygon(pClipper, masks[groupTest->id()]); // TODO: batch
                        if (res != Clipper2Lib::PointInPolygonResult::IsOutside) {
                            maskVertexIntersectionCache[key].push_back(groupTest->id());
                            maskMaskIntersection[group->id()].insert(groupTest->id());
                            maskMaskIntersectionMatrix(group->id() + 1, groupTest->id() + 1) = true;
                            maskMaskIntersectionMatrix(groupTest->id() + 1, group->id() + 1) = true;
                        }
                    }

                }
            }
        }
    }

    // Compute mask connected components
    for (auto it = maskMaskIntersection.begin(); it != maskMaskIntersection.end(); ++it) {
        bool merged = false;;
        for (auto cc : maskConnectedComponentCache) {
            for (int i : cc) {
                if (it->second.find(i) != it->second.end()) {
                    std::set cpy = it->second;
                    cc.merge(cpy);
                    merged = true;
                    break;
                }
            }
            if (merged) break;
        }

        if (!merged) {
            maskConnectedComponentCache.push_back(it->second);
        }
    }

    // std::cout << "mask mask intersection matrix: " << std::endl;
    // std::cout << maskMaskIntersectionMatrix << std::endl;
    // std::cout << "mask vertex intersection size: " << maskVertexIntersectionCache.size() << std::endl;
}

/**
 * Every group id is shifted by +Group::MAIN_GROUP_ID yes it's dumb
 */
LayoutAdjacencyMatrix LayoutManager::computeLayoutAdjacencyMatrix(const Layout &layout) const {
    unsigned int max = 0;
    for (const auto &depth : layout) {
        if (depth.empty()) continue;
        max = std::max((unsigned int)(*(std::max_element(depth.begin(), depth.end())) + 1), max);
    }
    LayoutAdjacencyMatrix adj = LayoutAdjacencyMatrix::Zero(max+1, max+1);
    for (unsigned int depth = 0; depth < layout.size(); depth++) {
        for (int idOccluder : layout[depth]) {
            for (unsigned int depthOccluded = depth + 1; depthOccluded < layout.size(); depthOccluded++) {
                for (int idOccluded : layout[depthOccluded]) {
                    adj(idOccluder + 1, idOccluded + 1) = 1;
                }
            }
        }
    }
    return adj;
}

/**
 * Construct KD-tree from keyframe B's vertices 
 */
void LayoutManager::makeKDTree(VectorKeyFrame *B, int inbetween) {
    const Inbetween &inb = B->inbetween(inbetween);
    std::vector<Point *> data;
    data.reserve(inb.nbVertices);
    m_dataKey.clear();
    m_dataKey.reserve(inb.nbVertices);

    for (Group *group : B->postGroups()) {
        if (group->size() == 0) continue;
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const StrokePtr &stroke = inb.strokes[it.key()];
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    data.push_back(stroke->points()[i]); 
                    m_dataKey.push_back(Utils::cantor(stroke->id(), i));
                }
            }
        }
    }

    m_treeTarget.make(std::move(data));
}

/**
 *  
 */
void LayoutManager::computeMaskBins(VectorKeyFrame *A, VectorKeyFrame *B) {
    double radSq = 10.0;
    std::vector<std::pair<size_t, Point::Scalar>> res;
    const int strideA = A->parentLayer()->stride(A->keyframeNumber());
    const int strideB = B->parentLayer()->stride(A->keyframeNumber());
    m_editor->updateInbetweens(A, strideA, strideA);
    m_editor->updateInbetweens(B, 0, strideB);
    const Inbetween &inb = A->inbetween(strideA);

    makeKDTree(B, 0);

    for (Group *group : A->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const StrokePtr &stroke = inb.strokes[it.key()];
            double rad = stroke->strokeWidth() + 2; 
            radSq = rad * rad;
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    unsigned int count = m_treeTarget.kdtree->radiusSearch(&stroke->points()[i]->pos()[0], radSq, res, nanoflann::SearchParams(10));
                    for (unsigned int j = 0; j < count; ++j) {
                        m_maskBins(group->id() + 1, m_treeTarget.data[res[j].first]->groupId() + 1) += 1;
                    }
                }
            }
        }
    }
}

/**
 * Return the many to one matching from B to A based on the current mask bins.
 * Groups in A that cannot be matched are put in the groupsNotMatched parameter.
 */
std::unordered_map<int, std::vector<int>> LayoutManager::computeExactMatchingBtoA(VectorKeyFrame *A, std::vector<int> &groupsNotMatched) {
    std::unordered_map<int, std::vector<int>> correspondence;
    for (int i = 0; i < m_maskBins.rows(); ++i) { // A
        int id = Group::ERROR_ID;
        int max = m_maskBins.row(i).maxCoeff(&id);
        if (max > 0) {
            correspondence[id - 1].push_back(i - 1); // B to A
            qDebug() << "A " << (i - 1) << "is linked to " << (id - 1);
        } else if (A->postGroups().fromId(i - 1) != nullptr) {
            groupsNotMatched.push_back(i - 1); // group in A not matched
            qDebug() << "A " << (i - 1) << "is not linked";
        }
    }
    return correspondence;
}

/**
 * Return the many to one matching from A to B based on the current mask bins.
 * Groups in B that cannot be matched are put in the groupsNotMatched parameter.
 */
std::unordered_map<int, std::vector<int>> LayoutManager::computeExactMatchingAtoB(VectorKeyFrame *B, std::vector<int> &groupsNotMatched) {
    std::unordered_map<int, std::vector<int>> correspondence;
    for (int i = 0; i < m_maskBins.cols(); ++i) { // B
        int id = Group::ERROR_ID;
        int max = m_maskBins.col(i).maxCoeff(&id); // A
        if (max > 0) {
            correspondence[id - 1].push_back(i - 1); // A to B
            qDebug() << "B " << (i - 1) << "is linked to " << (id - 1);
        } else if (B->postGroups().fromId(i - 1) != nullptr) {
            groupsNotMatched.push_back(i - 1); // group in B not matched
            qDebug() << "B " << (i - 1) << "is not linked";
        }
    }
    return correspondence;
}

/**
 * Build and return a matching based layout for the keyframe A based on its matching with keyframe B.
 */
GroupOrder LayoutManager::buildMatchingBasedLayout(VectorKeyFrame *A, VectorKeyFrame *B, const std::unordered_map<int, std::vector<int>> &matching, const std::vector<int> &groupsNotMatched) {
    GroupOrder order(A);
    int curDepth = 0;

    // Build new layout for A based on its coverage of B
    for (const auto &depth : B->orderPartials().firstPartial().groupOrder().order()) {
        bool moveDepth = false;
        for (int id : depth) {
            if (matching.find(id) != matching.end()) {
                moveDepth = true;
                const auto &mactchedGroups = matching.at(id);
                for (int i = 0; i < mactchedGroups.size(); ++i) {
                    order.add(mactchedGroups[i], curDepth);
                }
            }
        }
        if (moveDepth) ++curDepth;
    }

    // Unmatched groups are added to the back layer
    for (int id : groupsNotMatched) {
        order.add(id, order.nbDepths() - 1);
    }

    return order;
}

/**
 * Build and return a matching based layout for the keyframe B based on its matching with keyframe A.
 */
GroupOrder LayoutManager::buildInverseMatchingBasedLayout(VectorKeyFrame *A, VectorKeyFrame *B, const std::unordered_map<int, std::vector<int>> &matching, const std::vector<int> &groupsNotMatched) {
    GroupOrder order(B);
    int curDepth = 0;

    // Build new layout for B based on its coverage of A
    for (const auto &depthA : A->orderPartials().lastPartialAt(1.0).groupOrder().order()) {
        bool moveDepth = false;
        for (int idA : depthA) {
            if (matching.find(idA) != matching.end()) {
                moveDepth = true;
                const auto &mactchedGroups = matching.at(idA);
                for (int i = 0; i < mactchedGroups.size(); ++i) {
                    order.add(mactchedGroups[i], curDepth);
                }
            }
        }
        if (moveDepth) ++curDepth;
    }

    // Unmatched groups are added to the back layer
    for (int id : groupsNotMatched) {
        order.add(id, order.nbDepths() - 1);
    }

    return order; 
}

/**
 * Generate all possible layouts of the given keyframe
 */
std::vector<Layout> LayoutManager::generateAllLayouts(VectorKeyFrame *keyframe) {
    std::vector<Layout> layouts;
    std::vector<std::vector<int>> possibleDepths;


    // Merge depth i with depth j in the given layout. Returns a new layout with one less depth.
    auto merge = [&](std::vector<std::vector<int>> layout, int i, int j) {
        Layout newLayout = layout;
        for (int k : layout[j]) {
            newLayout[i].push_back(k);
        }
        auto it = newLayout.begin();
        std::advance(it, j);
        newLayout.erase(it);
        return newLayout;
    };

    // Power set
    Layout baseLayout;
    for (Group *group : keyframe->postGroups()) {
        baseLayout.push_back({group->id()});
    }
    std::vector<Layout> allLayouts;
    int index = 0;
    allLayouts.push_back(baseLayout);
    while (index < allLayouts.size()) {
        Layout l = allLayouts[index];
        if (l.size() == 1) {
            ++index;
            continue;
        }
        for (int i = 0; i < l.size() - 1; ++i) {
            for (int j = i + 1; j < l.size(); ++j) {
                Layout newL = merge(l, i, j);
                allLayouts.push_back(newL);
            }
        }
        ++index;
    }

    // Permutations
    for (auto l : allLayouts) {
        if (l.size() == 1) {
            layouts.push_back(l);
            continue;
        }

        auto lCopy = l;
        do {
            layouts.push_back(lCopy);
        } while (std::next_permutation(lCopy.begin(), lCopy.end()));
    }

    qDebug() << "layouts size: " << layouts.size();

    return layouts;
}