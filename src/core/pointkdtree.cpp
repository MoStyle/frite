/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pointkdtree.h"
#include "vectorkeyframe.h"
#include "inbetweens.h"

void PointKDTree::make(VectorKeyFrame *key, int inbetween) {
    const Inbetween &inb = key->inbetween(inbetween);
    data.clear();
    data.reserve(inb.nbVertices <= 0 ? key->nbStrokes() * 20 : inb.nbVertices);
    for (Group *group : key->postGroups()) {
        if (group->size() == 0) continue;
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const StrokePtr &stroke = inb.strokes[it.key()];
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    data.push_back(stroke->points()[i]); 
                }
            }
        }
    }
    data.shrink_to_fit();
    m_dataset.reset(new DatasetAdaptorPoint(data));
    kdtree.reset(new KDTree(2, *m_dataset, nanoflann::KDTreeSingleIndexAdaptorParams(10))); 
}

void PointKDTree::make(const std::vector<Point *> &_data) {
    data = _data;
    m_dataset.reset(new DatasetAdaptorPoint(data));
    kdtree.reset(new KDTree(2, *m_dataset, nanoflann::KDTreeSingleIndexAdaptorParams(10)));  
}

void PointKDTree::make(std::vector<Point *> &&_data) {
    data = std::vector<Point *>(std::move(_data));
    m_dataset.reset(new DatasetAdaptorPoint(data));
    kdtree.reset(new KDTree(2, *m_dataset, nanoflann::KDTreeSingleIndexAdaptorParams(10)));  
}
