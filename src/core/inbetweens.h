/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __INBETWEENS_H__
#define __INBETWEENS_H__

#include <vector>
#include <QHash>
#include "stroke.h"

struct Inbetween {
    QHash<int, StrokePtr> strokes;                          // stroke id -> stroke
    QHash<int, StrokePtr> backwardStrokes;                  // stroke id -> stroke
    QHash<int, std::vector<Point::VectorType>> corners;     // group id  -> list of corner positions
    QHash<int, Point::VectorType> centerOfMass;             // group id  -> center of mass
    QHash<int, QRectF> aabbs;                               // group id  -> aabb
    QHash<int, bool> fullyVisible;                          // group id  -> are all visibility threshold 0?
    unsigned int nbVertices;
 
    inline Point::VectorType getWarpedPoint(Group *group, const UVInfo &info) const {
        Lattice *grid = group->lattice();
        if (!grid->contains(info.quadKey)) qCritical() << "Error in inbetween getWarpedPoint (inbetween): invalid quad key " << info.quadKey;
        QuadPtr quad = grid->operator[](info.quadKey);
        Point::VectorType pos[4];
        for (int i = 0; i < 4; i++) pos[i] = corners[group->id()][quad->corners[i]->getKey()];
        Point::VectorType top =  pos[TOP_LEFT] * (1.0 - info.uv.x()) + pos[TOP_RIGHT] * info.uv.x(); 
        Point::VectorType bot =  pos[BOTTOM_LEFT] * (1.0 - info.uv.x()) + pos[BOTTOM_RIGHT] * info.uv.x();
        Point::VectorType res = top * (1.0 - info.uv.y()) + bot * info.uv.y();
        return res;
    }

    // Point::VectorType getWarpedPoint(Group *group, Point::VectorType p) const;
    bool quadContainsPoint(Group *group, QuadPtr quad, const Point::VectorType &p) const;
    bool contains(Group *group, const Point::VectorType &p, QuadPtr &quad, int &key) const;
    Point::VectorType getUV(Group *group, const Point::VectorType &p, int &quadKey) const;
    bool bakeForwardUV(Group *group, const Stroke *stroke, Interval &interval, UVHash &uvs) const;
    void clear();
    void destroyBuffers();
};

class Inbetweens : public std::vector<Inbetween> {
public:
    void makeDirty();
    void makeDirty(size_t i) { m_dirty[i] = true; }
    void makeClean(size_t i) { m_dirty[i] = false; }
    bool isClean(size_t i) const { return !m_dirty[i]; }

private:
    std::vector<bool> m_dirty;
};

#endif // __INBETWEENS_H__