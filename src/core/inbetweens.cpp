#include "inbetweens.h"

#include "stroke.h"
#include "utils/geom.h"

// Point::VectorType Inbetween::getWarpedPoint(Group *group, Point::VectorType p) const {
//     int quadKey;
//     Point::VectorType uv = getUV(group, p, quadKey);
//     if (quadKey == INT_MAX) {
//         qCritical() << "Error in inbetween getWarpedPoint (inbetween): invalid pos: " <<  p.x() << ", " << p.y();
//         return p;
//     }
//     return getWarpedPoint(group, {quadKey, uv});
// }

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
    for (auto it = group->lattice()->quads().constBegin(); it != group->lattice()->quads().constEnd(); ++it) {
        if (quadContainsPoint(group, it.value(), p)) {
            quad = it.value();
            key = it.key();
            return true;
        }
    }
    return false;
}

// see lattice implementation
Point::VectorType Inbetween::getUV(Group *group, const Point::VectorType &p, int &quadKey) const {
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

bool Inbetween::bakeForwardUV(Group *group, const Stroke *stroke, Interval &interval, UVHash &uvs) const {
    if (stroke == nullptr) {
        qWarning() << "cannot compute UVs for this interval: invalid stroke: " << stroke;
        return false;
    }

    // overshoot if possible
    QuadPtr q;
    int k, from = interval.from(), to = interval.to();
    bool isNextPointInLattice = to < stroke->size() - 1 && contains(group, stroke->points()[interval.to() + 1]->pos(), q, k);
    if (isNextPointInLattice) {
        to += 1;
    } else if (to < stroke->size() - 1) {
        interval.setOvershoot(false);
    }

    int key;
    for (size_t i = from; i <= to; ++i) {
        const Point::VectorType &pos = stroke->points()[i]->pos();
        stroke->points()[i]->initId(stroke->id(), i);
        UVInfo uv;
        if (uvs.has(stroke->id(), i)) uv = uvs.get(stroke->id(), i);
        uv.uv = getUV(group, pos, key);
        uv.quadKey = key;
        uvs.add(stroke->id(), i, uv);
    }

    return true;
}
/**
 * Should be called in a valid OpenGL context!
*/
void Inbetween::clear() {
    destroyBuffers();
    strokes.clear();
    backwardStrokes.clear();
    corners.clear();
    centerOfMass.clear();
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