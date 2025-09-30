/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

 #ifndef LAYOUTMANAGER_H
#define LAYOUTMANAGER_H

#include <QTransform>

#include "point.h"
#include "basemanager.h"
#include "grouporder.h"
#include "pointkdtree.h"

#include <unordered_set>
#include <unordered_map>

typedef Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> LayoutAdjacencyMatrix;
typedef std::vector<std::vector<int>> Layout;

class GroupOrder; 

class LayoutManager : public BaseManager
{
    Q_OBJECT

public:
    LayoutManager(QObject* pParent);

    double computeBestLayout(VectorKeyFrame *A, VectorKeyFrame *B, GroupOrder &bestLayout);
    int computeBestLayoutChangeLocation(VectorKeyFrame *A, const GroupOrder &layoutA);
    GroupOrder propagateLayoutAtoB(VectorKeyFrame *A, VectorKeyFrame *B);
    GroupOrder propagateLayoutBtoA(VectorKeyFrame *A, VectorKeyFrame *B);

    unsigned verticesA() const { return m_nbVerticesA; };
    unsigned verticesB() const { return m_nbVerticesB; };

    std::unordered_set<unsigned int> getOccludedVertices(VectorKeyFrame *keyframe, int inbetween);
    
protected:
    double getLayoutScore(VectorKeyFrame *A, VectorKeyFrame *B, const Layout &layoutA, const Layout &layoutB, int inbetweenA,int inbetweenB, std::unordered_map<int, double> &groupScores); 
    std::unordered_set<unsigned int> computeOccludedVertices(VectorKeyFrame *keyframe, const Layout &layout, const std::unordered_map<unsigned int, std::vector<int>> &maskVertexIntersectionCache, int inbetween);
    void computeMaskVertexIntersectionCache(VectorKeyFrame *keyframe, int inbetween, std::unordered_map<unsigned int, std::vector<int>> &maskVertexIntersectionCache, std::vector<std::set<int>> &maskConnectedComponentCache, LayoutAdjacencyMatrix &maskMaskIntersectionMatrix);
    LayoutAdjacencyMatrix computeLayoutAdjacencyMatrix(const Layout &layout) const;

    void makeKDTree(VectorKeyFrame *B, int inbetween);    

    void computeMaskBins(VectorKeyFrame *A, VectorKeyFrame *B);
    std::unordered_map<int, std::vector<int>> computeExactMatchingBtoA(VectorKeyFrame *A, std::vector<int> &groupsNotMatched);
    std::unordered_map<int, std::vector<int>> computeExactMatchingAtoB(VectorKeyFrame *B, std::vector<int> &groupsNotMatched);
    GroupOrder buildMatchingBasedLayout(VectorKeyFrame *A, VectorKeyFrame *B, const std::unordered_map<int, std::vector<int>> &matching, const std::vector<int> &groupsNotMatched);
    GroupOrder buildInverseMatchingBasedLayout(VectorKeyFrame *A, VectorKeyFrame *B, const std::unordered_map<int, std::vector<int>> &matching, const std::vector<int> &groupsNotMatched);

private:
    std::vector<Layout> generateAllLayouts(VectorKeyFrame *keyframe);

    std::unordered_map<unsigned int, std::vector<int>> m_maskVertexIntersectionCacheA, m_maskVertexIntersectionCacheB;
    std::vector<std::set<int>> m_maskConnectedComponentCacheA, m_maskConnectedComponentCacheB;      // !unused
    LayoutAdjacencyMatrix m_maskMaskIntersectionMatrixA, m_maskMaskIntersectionMatrixB;             // !unused
    Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> m_maskBins;
    unsigned int m_nbVerticesA, m_nbVerticesB;

    // KD tree data
    std::vector<unsigned int> m_dataKey;
    PointKDTree m_treeTarget;
};
#endif // LAYOUTMANAGER_H
