#ifndef __POINTKDTREE_H__
#define __POINTKDTREE_H__

#include <QTransform>
#include "point.h"
#include "nanoflann.h"
#include "nanoflann_datasetadaptor.h"

class VectorKeyFrame;

struct PointKDTree {
    typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<Point::Scalar, DatasetAdaptorPoint>, DatasetAdaptorPoint, 2, size_t> KDTree;

    void make(VectorKeyFrame *key, int inbetween);
    void make(const std::vector<Point *> &_data);
    void make(std::vector<Point *> &&_data);

    std::unique_ptr<KDTree> kdtree;
    std::vector<Point *> data;
private:
    std::unique_ptr<DatasetAdaptorPoint> m_dataset;
};

#endif // __POINTKDTREE_H__