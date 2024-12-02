#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include <Eigen/Eigen>
#include <Eigen/SparseCore>

#include "basemanager.h"
#include "vectorkeyframe.h"
#include "layer.h"
#include "corner.h"
#include "lattice.h"
#include "nanoflann.h"
#include "nanoflann_datasetadaptor.h"

using namespace Eigen;

class ViewManager;
class Corner;

class GridManager : public BaseManager
{
    Q_OBJECT

public :
    typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<Point::Scalar, DatasetAdaptor>, DatasetAdaptor, 2, size_t> KDTree;

    GridManager(QObject* pParent);

    // Construction
    bool constructGrid(Group *group, ViewManager *view, unsigned int cellSize);
    bool constructGrid(Group *group, ViewManager *view, Stroke *stroke, Interval &interval);
    bool addStrokeToGrid(Group *group, Stroke *stroke, Interval &interval);
    bool addStrokeToGrid(Group *group, Stroke *stroke, Intervals &intervals);
    bool bakeStrokeInGrid(Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type=REF_POS, bool forward=true);
    void bakeStrokeInGrid(Group *group, Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, const Inbetween &inbetween, bool forward=true);
    void bakeStrokeInGridPrecomputed(Lattice *grid, Group *group, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type=REF_POS, bool forward=true);
    bool bakeStrokeInGridWithConnectivityCheck(Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type=REF_POS, bool forward=true);
    std::pair<int, int> expandTargetGridToFitStroke(Lattice *grid, Stroke *stroke, bool removeExtremities=true, int from=0, int to=-1);
    bool expandTargetGridToFitStroke(Group *group, const StrokeIntervals &intervals, StrokeIntervals &added, StrokeIntervals &notAdded);
    std::pair<int, int> expandTargetGridToFitStroke2(Lattice *grid, Stroke *stroke, bool removeExtremities=true, int from=0, int to=-1);
    bool expandGridToFitStroke(Group *group, const Inbetween &inbetween, int inbetweenNumber, int stride, Lattice *grid, Stroke *stroke);
    void retrocomp(Group *group);

    // Deformation
    Corner* selectedCorner() { return m_selectedCorner; }
    int getDeformRange() { return m_deformRange; }
    bool isDeformed() {return m_deformed; }
    void selectGridCorner(Group* group, PosTypeIndex type, const Point::VectorType &lastPos, bool constrained);
    void moveGridCorner(Group* group, PosTypeIndex type, const Point::VectorType &delta, bool rot, Point::Affine &transformation);
    void moveGridCornerPosition(Group* group, PosTypeIndex type, const Point::VectorType &pos);
    void releaseGridCorner(Group* group);
    void scaleGrid(Group *group, float factor, PosTypeIndex type, int mode=0);
    void scaleGrid(Group *group, float factor, PosTypeIndex type, const std::vector<Corner *> &corners, int mode=0);

    void addOneRing(Lattice *grid, std::vector<int> &newQuadsKeys);
    void propagateDeformToOneRing(Lattice *grid, const std::vector<int> &oneRing);    

public slots:
    void setDeformRange(int k);

private:
    void propagateDeformToNewQuads(Group *group, Lattice *grid, std::vector<QuadPtr> &newQuads);    
    void propagateDeformToConnectedComponent(Lattice *grid, const std::vector<int> &quads);    

    int m_deformRange;
    bool m_deformed;
    Corner* m_selectedCorner = nullptr;
    QList<QPair<int, float>> m_cornersSelected;
    Point::VectorType m_lastPos;
};
#endif //GRIDMANAGER_H
