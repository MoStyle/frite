/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __INBETWEENS_H__
#define __INBETWEENS_H__

#include <vector>
#include <QHash>
#include "stroke.h"

struct Inbetween {
    QHash<int, StrokePtr> strokes;
    QHash<int, StrokePtr> backwardStrokes;
    QHash<int, std::vector<Point::VectorType>> corners;

    inline Point::VectorType getWarpedPoint(Group *group, const UVInfo &info) const {
        Lattice *grid = group->lattice();
        if (!grid->contains(info.quadKey)) qCritical() << "Error in inbetween getWarpedPoint: invalid quad key " << info.quadKey;
        QuadPtr quad = grid->operator[](info.quadKey);
        Point::VectorType pos[4];
        for (int i = 0; i < 4; i++) pos[i] = corners[group->id()][quad->corners[i]->getKey()];
        Point::VectorType top =  pos[TOP_LEFT] * (1.0 - info.uv.x()) + pos[TOP_RIGHT] * info.uv.x(); 
        Point::VectorType bot =  pos[BOTTOM_LEFT] * (1.0 - info.uv.x()) + pos[BOTTOM_RIGHT] * info.uv.x();
        Point::VectorType res = top * (1.0 - info.uv.y()) + bot * info.uv.y();
        return res;
    }

    bool quadContainsPoint(Group *group, QuadPtr quad, const Point::VectorType &p) const;
    bool contains(Group *group, const Point::VectorType &p, QuadPtr &quad, int &key) const;
    Point::VectorType getUV(Group *group, const Point::VectorType &p,  int &quadKey) const;
    void destroyBuffers();
};

class Inbetweens : public std::vector<Inbetween> {
public:
    void makeDirty();
    void makeClean(size_t i) { m_dirty[i] = false; }
    bool isClean(size_t i) const { return !m_dirty[i]; }

private:
    std::vector<bool> m_dirty;
};

#endif // __INBETWEENS_H__