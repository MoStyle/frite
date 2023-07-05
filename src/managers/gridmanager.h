/*
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include <Eigen/Eigen>
#include <Eigen/SparseCore>

#include "basemanager.h"
#include "vectorkeyframe.h"
#include "layer.h"
#include "corner.h"
#include "lattice.h"
#include "nanoflann_datasetadaptor.h"

#include <nanoflann.hpp>

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
    bool constructGrid(Group *group, ViewManager *view);
    bool constructGrid(Group *group, ViewManager *view, Stroke *stroke, Interval &interval);
    void bakeStrokeInGrid(Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type=REF_POS);
    std::pair<int, int> addStrokeToDeformedGrid(Lattice *grid, Stroke *stroke);

    // Deformation
    Corner* selectedCorner() { return m_selectedCorner; }
    int getDeformRange() { return m_deformRange; }
    bool isDeformed() {return m_deformed; }
    void selectGridCorner(Group* group, PosTypeIndex type, const Point::VectorType &lastPos, bool constrained);
    void moveGridCorner(Group* group, PosTypeIndex type, const Point::VectorType &delta, bool rot, Point::Affine &transformation);
    void moveGridCornerPosition(Group* group, PosTypeIndex type, const Point::VectorType &pos);
    void releaseGridCorner(Group* group);
    void scaleGrid(Group *group, float factor, PosTypeIndex type);
    void scaleGrid(Group *group, float factor, PosTypeIndex type, const std::vector<Corner *> &corners);

    void addOneRing(Lattice *grid, std::vector<int> &newQuadsKeys);
    void propagateDeformToOneRing(Lattice *grid, const std::vector<int> &oneRing);    

public slots:
    void setDeformRange(int k);

private:
    bool addStrokeToGrid(Group *group, Stroke *stroke, Interval &interval);
    void propagateDeformToNewQuads(Lattice *grid, std::vector<QuadPtr> &newQuads);    
    void propagateDeformToConnectedComponent(Lattice *grid, const std::vector<int> &quads);    
    bool needRefinement(Lattice *grid, Point::VectorType &prevPoint, Point::VectorType &curPoint, int &quadKeyOut);

    int m_deformRange;
    bool m_deformed;
    Corner* m_selectedCorner = nullptr;
    QList<QPair<int, float>> m_cornersSelected;
    Point::VectorType m_lastPos;
};
#endif //GRIDMANAGER_H
