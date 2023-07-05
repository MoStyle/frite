/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __NANOFLANN_DATASETADAPTOR_H__
#define __NANOFLANN_DATASETADAPTOR_H__

#include "point.h"
#include <vector>

class DatasetAdaptor {
public:
    DatasetAdaptor(const std::vector<Point::VectorType>& data) : m_data(data) { }

    // Must return the number of data poins
    inline size_t kdtree_get_point_count() const { return m_data.size(); }

    // Must return the dim'th component of the idx'th point in the class:
    inline Point::Scalar kdtree_get_pt(const size_t idx, const size_t dim) const { return m_data[idx][dim]; }

    // Optional bounding-box computation: return false to default to a standard bbox computation loop.
    //   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
    //   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
    template <class BBOX>
    bool kdtree_get_bbox(BBOX& /*bb*/) const {
        return false;
    }
private: 
    const std::vector<Point::VectorType> m_data;
};

class DatasetAdaptorPoint {
public:
    DatasetAdaptorPoint(const std::vector<Point *>& data) : m_data(data) { }

    // Must return the number of data poins
    inline size_t kdtree_get_point_count() const { return m_data.size(); }

    // Must return the dim'th component of the idx'th point in the class:
    inline Point::Scalar kdtree_get_pt(const size_t idx, const size_t dim) const { return m_data[idx]->pos()[dim]; }

    // Optional bounding-box computation: return false to default to a standard bbox computation loop.
    //   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
    //   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
    template <class BBOX>
    bool kdtree_get_bbox(BBOX& /*bb*/) const {
        return false;
    }
private: 
    const std::vector<Point *> m_data;
};

#endif  // __NANOFLANN_DATASETADAPTOR_H__