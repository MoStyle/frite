#ifndef VISIBILITYMANAGER_H
#define VISIBILITYMANAGER_H

#include <QTransform>

#include "point.h"
#include "basemanager.h"
#include "pointkdtree.h"

#include <unordered_map>
#include <unordered_set>

class VisibilityManager : public BaseManager
{
    Q_OBJECT

public:
    VisibilityManager(QObject* pParent);
    
    // Disappearance
    void init(VectorKeyFrame *A, VectorKeyFrame *B);
    void computePointsFirstPass(VectorKeyFrame *A, VectorKeyFrame *B);
    bool findSources(VectorKeyFrame *A, std::vector<Point *> &sources);
    void assignVisibilityThreshold(VectorKeyFrame *A, const std::vector<Point *> &sources);

    // Appearance
    void initAppearance(VectorKeyFrame *A, VectorKeyFrame *B);
    void computePointsFirstPassAppearance(VectorKeyFrame *A, VectorKeyFrame *B);
    bool findSourcesAppearance(VectorKeyFrame *B, std::vector<Point::VectorType> &sources);
    void addGroupsOrBake(VectorKeyFrame *A, VectorKeyFrame *B, std::vector<Point::VectorType> &sources, std::vector<int> &sourcesGroupsId);
    void assignVisibilityThresholdAppearance(VectorKeyFrame *A, const std::vector<Point::VectorType> &sources, const std::vector<int> sourcesGroupsId);

protected:

private:
    std::vector<Point *> m_points;
    std::vector<unsigned int> m_keys, m_pointsKeys;

    std::vector<Point *> m_pointsAppearance;
    std::vector<unsigned int> m_pointsKeysAppearance;
    StrokeIntervals m_strokesAppearance;
    std::unordered_set<unsigned int> m_appearanceSourcesKeys;
    std::unordered_map<unsigned int, unsigned int> m_appearanceKeyToIndex;

    std::vector<std::pair<unsigned int, Point *>> m_appearingPointsKeys;
    std::vector<int> m_appearingPointsCluster;
    int m_clusterIdx;

    std::unordered_map<unsigned int, double> m_radiusSq, m_radiusSqAppearance; // cantor id -> stroke width^2
    PointKDTree treeA, treeB;    
};
#endif // VISIBILITYMANAGER_H
