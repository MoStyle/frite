/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "inbetweens.h"

#include "stroke.h"
#include "utils/geom.h"

// see lattice implementation
bool Inbetween::quadContainsPoint(Group *group, QuadPtr quad, const Point::VectorType &p) const {
    if (quad == nullptr || group == nullptr) return false;
    Point::VectorType q(-1e7, -1e7);
    Point::VectorType c[4];
    const std::vector<Point::VectorType> &cornersPos = corners[group->id()];
    c[0] = cornersPos[quad->corners[TOP_RIGHT]->getKey()];
    c[1] = cornersPos[quad->corners[BOTTOM_RIGHT]->getKey()];
    c[2] = cornersPos[quad->corners[BOTTOM_LEFT]->getKey()];
    c[3] = cornersPos[quad->corners[TOP_LEFT]->getKey()];
    int num = 0;
    for (int i = 0; i < 4; i++) {
        if (Geom::checkSegmentsIntersection(p, q, c[i], c[(i + 1) % 4])) {
            if(Geom::wedge((c[i] - p), (q - p)) != 0)
                num++;
        }
    }
    return num % 2 == 1;
}

// see lattice implementation
bool Inbetween::contains(Group *group, const Point::VectorType &p, QuadPtr &quad, int &key) const {
    // TODO bounding box test before
    for (auto it = group->lattice()->hash().constBegin(); it != group->lattice()->hash().constEnd(); ++it) {
        if (quadContainsPoint(group, it.value(), p)) {
            quad = it.value();
            key = it.key();
            return true;
        }
    }
    return false;
}

// see lattice implementation
Point::VectorType Inbetween::getUV(Group *group, const Point::VectorType &p,  int &quadKey) const {
    QuadPtr quad = nullptr;
    
    // check if the given point is in the grid, if so, in which quad?
    if (!contains(group, p, quad, quadKey)) {
        quadKey = INT_MAX;
        return Point::VectorType::Zero();
    }

    const std::vector<Point::VectorType> &cornersPos = corners[group->id()];
    Point::VectorType pos[4];
    for (int i = 0; i < 4; i++) pos[i] = cornersPos[quad->corners[i]->getKey()];

    // we need three basis vectors to handle non-parallelograms
    Point::VectorType b1 = pos[BOTTOM_RIGHT] - pos[BOTTOM_LEFT];  
    Point::VectorType b2 = pos[TOP_LEFT] - pos[BOTTOM_LEFT];      
    Point::VectorType b3 = pos[TOP_RIGHT] - pos[TOP_LEFT] - b1;   
    Point::VectorType q = p - pos[BOTTOM_LEFT];
    qreal A = Geom::wedge(b2, b3);
    qreal B = Geom::wedge(b3, q) - Geom::wedge(b1, b2);
    qreal C = Geom::wedge(b1, q);
    Point::VectorType uv;

    if (std::abs(A) < 1e-4) {
        uv.y() = -C / B;
    } else {
        // solve Av^2 + Bv + C = 0 for v
        qreal discrim = std::sqrt(B * B - 4. * A * C);
        qreal y1 = 0.5 * (-B + discrim) / A;
        qreal y2 = 0.5 * (-B - discrim) / A;
        if (y1 >= 0 && y1 <= 1)
        uv.y() = y1;
        else
        uv.y() = y2;
    }

    // now that we have v we can find u
    Point::VectorType denom = b1 + uv.y() * b3;
    if (abs(denom.x()) > abs(denom.y()))
        uv.x() = (q.x() - b2.x() * uv.y()) / denom.x();
    else
        uv.x() = (q.y() - b2.y() * uv.y()) / denom.y();

    uv.y() = 1.0 - uv.y();
    return uv;
}

/**
 * Should be called in a valid OpenGL context!
*/
void Inbetween::destroyBuffers() {
    for (StrokePtr stroke : strokes) {
        stroke->destroyBuffers();
    }
    for (StrokePtr stroke : backwardStrokes) {
        stroke->destroyBuffers();
    }
}

void Inbetweens::makeDirty() {
    m_dirty.resize(size());
    std::fill(m_dirty.begin(), m_dirty.end(), true);
}