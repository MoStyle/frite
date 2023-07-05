/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "lattice.h"
#include "stroke.h"
#include "arap.h"
#include "utils/stopwatch.h"
#include "utils/geom.h"
#include "dialsandknobs.h"
#include "trajectory.h"
#include "tabletcanvas.h"
#include "cubic.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStack>

#include <set>
#include <unsupported/Eigen/MatrixFunctions>

typedef Eigen::Triplet<double> TripletD;

static dkBool k_useGlobalRigidTransform("Options->Drawing->Use global transform for groups", true);

Lattice::Lattice(VectorKeyFrame *keyframe)
    : m_keyframe(keyframe),
      m_nbCols(0),
      m_nbRows(0),
      m_cellSize(0),
      m_oGrid(Eigen::Vector2i::Zero()),
      m_precomputeDirty(true),
      m_arapDirty(true),
      m_backwardUVDirty(true),
      m_singleConnectedComponent(false),
      m_currentPrecomputedTime(-1.0f),
      m_maxCornerKey(0),
      m_rot(0.0),
      m_scale(1.0) {}

Lattice::Lattice(const Lattice &other)
    : m_keyframe(other.m_keyframe),
      m_nbCols(other.m_nbCols),
      m_nbRows(other.m_nbRows),
      m_cellSize(other.m_cellSize),
      m_oGrid(other.m_oGrid),
      m_precomputeDirty(true),
      m_arapDirty(true),
      m_backwardUVDirty(true),
      m_currentPrecomputedTime(-1.0),
      m_maxCornerKey(0),
      m_rot(0.0),
      m_scale(1.0) {
    // create quads and copy corners
    bool isNewQuad = false;
    int x, y;
    for (auto it = other.m_hashTable.constBegin(); it != other.m_hashTable.constEnd(); ++it) {
        keyToCoord(it.key(), x, y);
        QuadPtr quad = addQuad(it.key(), x, y, isNewQuad);
        for (int i = 0; i < 4; ++i) {
            quad->corners[i]->coord(TARGET_POS) = it.value()->corners[i]->coord(TARGET_POS);
            quad->corners[i]->coord(INTERP_POS) = it.value()->corners[i]->coord(INTERP_POS);
            quad->corners[i]->coord(DEFORM_POS) = it.value()->corners[i]->coord(DEFORM_POS);
        }
    }
    isConnected();
}

void Lattice::init(int cellsize, int nbCols, int nbRows, Eigen::Vector2i origin) {
    clear();
    m_cellSize = cellsize;
    m_nbCols = nbCols;
    m_nbRows = nbRows;
    m_oGrid = origin;
}

void Lattice::save(QDomDocument &doc, QDomElement &latticeElt) const {
    // save attributes
    // latticeElt.setAttribute("nbInterStrokes", uint(nbInterStrokes()));
    latticeElt.setAttribute("cellSize", int(cellSize()));
    latticeElt.setAttribute("nbCols", nbCols());
    latticeElt.setAttribute("nbRows", nbRows());
    latticeElt.setAttribute("origin_x", origin().x());
    latticeElt.setAttribute("origin_y", origin().y());

    // save quads
    int count = 0;
    for (auto it = m_hashTable.begin(); it != m_hashTable.end(); ++it) {
        QDomElement quadElt = doc.createElement("quad");
        quadElt.setAttribute("key", int(it.key()));
        // TODO: save strokerangehash m_elements (or recompute them)
        latticeElt.appendChild(quadElt);
        count++;
    }

    // save corners
    for (Corner *c : m_corners) {
        QDomElement cornerElt = doc.createElement("corner");
        cornerElt.setAttribute("key", int(c->getKey()));
        cornerElt.setAttribute("deformable", bool(c->isDeformable()));
        cornerElt.setAttribute("coord_TARGET_POS_x", c->coord(TARGET_POS).x());
        cornerElt.setAttribute("coord_TARGET_POS_y", c->coord(TARGET_POS).y());
        cornerElt.setAttribute("coord_REF_POS_x", c->coord(REF_POS).x());
        cornerElt.setAttribute("coord_REF_POS_y", c->coord(REF_POS).y());
        cornerElt.setAttribute("coord_INTERP_POS_x", c->coord(INTERP_POS).x());
        cornerElt.setAttribute("coord_INTERP_POS_y", c->coord(INTERP_POS).y());
        cornerElt.setAttribute("coord_DEFORM_POS_x", c->coord(DEFORM_POS).x());
        cornerElt.setAttribute("coord_DEFORM_POS_y", c->coord(DEFORM_POS).y());
        cornerElt.setAttribute("quadNum", c->nbQuads());
        cornerElt.setAttribute("quadKey_0", c->quads(TOP_LEFT) == nullptr ? INT_MAX : c->quads(TOP_LEFT)->key());
        cornerElt.setAttribute("quadKey_1", c->quads(TOP_RIGHT) == nullptr ? INT_MAX : c->quads(TOP_RIGHT)->key());
        cornerElt.setAttribute("quadKey_2", c->quads(BOTTOM_RIGHT) == nullptr ? INT_MAX : c->quads(BOTTOM_RIGHT)->key());
        cornerElt.setAttribute("quadKey_3", c->quads(BOTTOM_LEFT) == nullptr ? INT_MAX : c->quads(BOTTOM_LEFT)->key());
        latticeElt.appendChild(cornerElt);
    }
}

void Lattice::load(QDomElement &latticeElt) {
    init(latticeElt.attribute("cellSize").toInt(), latticeElt.attribute("nbCols").toInt(), latticeElt.attribute("nbRows").toInt(),
         Eigen::Vector2i(latticeElt.attribute("origin_x").toInt(), latticeElt.attribute("origin_y").toInt()));

    // Load quads
    QDomElement quadElt = latticeElt.firstChildElement("quad");
    while (!quadElt.isNull()) {
        int key = quadElt.attribute("key").toInt();
        bool isNewQuad = false;
        addEmptyQuad(key);
        quadElt = quadElt.nextSiblingElement("quad");
    }

    // Load corners
    QDomElement cornerElt = latticeElt.firstChildElement("corner");
    while (!cornerElt.isNull()) {
        int key = cornerElt.attribute("key").toInt();
        Corner *c = new Corner();
        c->setKey(key);
        c->setDeformable(cornerElt.attribute("deformable").toInt());
        c->setNbQuads(cornerElt.attribute("quadNum").toInt());
        c->coord(TARGET_POS) = Point::VectorType(cornerElt.attribute("coord_TARGET_POS_x").toDouble(), cornerElt.attribute("coord_TARGET_POS_y").toDouble());
        c->coord(REF_POS) = Point::VectorType(cornerElt.attribute("coord_REF_POS_x").toDouble(), cornerElt.attribute("coord_REF_POS_y").toDouble());
        c->coord(INTERP_POS) = Point::VectorType(cornerElt.attribute("coord_INTERP_POS_x").toDouble(), cornerElt.attribute("coord_INTERP_POS_y").toDouble());
        c->coord(DEFORM_POS) = Point::VectorType(cornerElt.attribute("coord_DEFORM_POS_x").toDouble(), cornerElt.attribute("coord_DEFORM_POS_y").toDouble());
        // set quad/corner correspondences
        for (int i = 0; i < 4; ++i) {
            int quadKey = cornerElt.attribute("quadKey_" + QString::number(i)).toInt();
            if (contains(quadKey)) {
                c->quads(CornerIndex(i)) = m_hashTable[quadKey];
                c->quads(CornerIndex(i))->corners[(i + 2) % 4] = c;
            }
        }
        m_corners.push_back(c);
        cornerElt = cornerElt.nextSiblingElement("corner");
    }

    m_maxCornerKey = m_corners.size();
    m_arapDirty = true;
    m_backwardUVDirty = true;
    m_currentPrecomputedTime = -1.0f;
    isConnected();
}

void Lattice::clear() {
    for (QuadPtr quad : quads()) {
        quad.reset();
    }
    qDeleteAll(m_corners);
    m_hashTable.clear();
    m_corners.clear();
    m_maxCornerKey = 0;
    m_arapDirty = true;
    m_backwardUVDirty = true;
    m_singleConnectedComponent = false;
    m_currentPrecomputedTime = -1.0f;
    m_rot = 0.0;
    m_scale = 1.0;
}

/**
 * Remove the reference to a stroke. The stroke itself is not removed, just its embedding in the lattice. 
 */
void Lattice::removeStroke(int strokeId, bool breakdown) {
    QMutableHashIterator<int, QuadPtr> it(m_hashTable);
    while (it.hasNext()) {
        it.next();
        it.value()->removeStroke(strokeId);
        if (it.value()->nbElements() == 0 && !breakdown) {
            it.value()->corners[TOP_LEFT]->quads(BOTTOM_RIGHT) = nullptr;
            it.value()->corners[TOP_RIGHT]->quads(BOTTOM_LEFT) = nullptr;
            it.value()->corners[BOTTOM_LEFT]->quads(TOP_RIGHT) = nullptr;
            it.value()->corners[BOTTOM_RIGHT]->quads(TOP_LEFT) = nullptr;
            it.remove();
        }
    }
    deleteUnusedCorners();
    isConnected();
}

void Lattice::setArapDirty() {
    m_precomputeDirty = true;
    m_arapDirty = true;
}

/**
 * Add a quad at the given screen space location. If a quad already exists, return the existing quad. 
 */
QuadPtr Lattice::addQuad(const Point::VectorType &point, bool &isNewQuad) {
    QuadPtr q;
    int key;

    if (contains(point, REF_POS, q, key)) {
        isNewQuad = false;
        return q;
    }

    int r, c;
    posToCoord(point, r, c);
    key = coordToKey(r, c);

    return addQuad(key, r, c, isNewQuad);
}

/**
 * Add a quad at the given lattice location, create corners if they do not exist.
 */
QuadPtr Lattice::addQuad(int key, int x, int y, bool &isNewQuad) {
    if (x >= nbCols() || y >= nbRows() || x < 0 || y < 0) {
        qCritical() << "Error in addQuad: invalid quad coordinate: " << x << ", " << y;
        return nullptr;
    }
    if (coordToKey(x, y) != key) qCritical() << "Error in addQuad: discrepancy between the given key and lattice coord (" << key << " != " << coordToKey(x, y) << ")";

    if (contains(key)) {
        isNewQuad = false;
        return m_hashTable[key];
    }

    // Create new cell
    QuadPtr cell = std::shared_ptr<Quad>(new Quad(key));
    isNewQuad = true;

    auto linkQuad = [&](int x, int y, CornerIndex cornerIdx, CornerIndex quadIdx, CornerIndex destIdx) {
        if (x < 0 || y < 0 || x >= nbCols() || y >= nbRows()) return;
        int nKey = coordToKey(x, y);
        if (contains(nKey)) {
            QuadPtr quad = m_hashTable[nKey];
            Corner *corner = quad->corners[cornerIdx];
            if (corner->quads(quadIdx) == nullptr) {
                corner->quads(quadIdx) = cell;
                corner->incrNbQuads();
                cell->corners[destIdx] = corner;
            }
        }
    };

    // Iterate over potential neighbors
    linkQuad(x - 1, y - 1, BOTTOM_RIGHT, BOTTOM_RIGHT, TOP_LEFT);
    linkQuad(x, y - 1, BOTTOM_LEFT, BOTTOM_RIGHT, TOP_LEFT);
    linkQuad(x, y - 1, BOTTOM_RIGHT, BOTTOM_LEFT, TOP_RIGHT);
    linkQuad(x + 1, y - 1, BOTTOM_LEFT, BOTTOM_LEFT, TOP_RIGHT);
    linkQuad(x + 1, y, TOP_LEFT, BOTTOM_LEFT, TOP_RIGHT);
    linkQuad(x + 1, y, BOTTOM_LEFT, TOP_LEFT, BOTTOM_RIGHT);
    linkQuad(x - 1, y, TOP_RIGHT, BOTTOM_RIGHT, TOP_LEFT);
    linkQuad(x - 1, y, BOTTOM_RIGHT, TOP_RIGHT, BOTTOM_LEFT);
    linkQuad(x - 1, y + 1, TOP_RIGHT, TOP_RIGHT, BOTTOM_LEFT);
    linkQuad(x, y + 1, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT);
    linkQuad(x, y + 1, TOP_RIGHT, TOP_LEFT, BOTTOM_RIGHT);
    linkQuad(x + 1, y + 1, TOP_LEFT, TOP_LEFT, BOTTOM_RIGHT);

    // Create missing corners
    Point::VectorType positions[4] = {Point::VectorType(x, y), Point::VectorType(x + 1, y), Point::VectorType(x + 1, y + 1), Point::VectorType(x, y + 1)};
    for (int i = 0; i < 4; ++i) {
        if (cell->corners[i] == nullptr) {
            Corner *corner = new Corner();
            Point::VectorType pos = m_cellSize * positions[i] + Point::VectorType(m_oGrid.x(), m_oGrid.y());
            corner->coord(REF_POS) = pos;
            corner->coord(TARGET_POS) = pos;
            corner->coord(INTERP_POS) = pos;
            corner->coord(DEFORM_POS) = pos;
            corner->quads(CornerIndex((i + 2) % 4)) = cell;
            corner->setNbQuads(1);
            corner->setKey(m_maxCornerKey);
            m_maxCornerKey++;
            cell->corners[i] = corner;
            m_corners.append(corner);
        }
    }

    insert(key, cell);
    m_precomputeDirty = true;
    m_arapDirty = true;
    return cell;
}

// Add an empty quad object (no corners, no elements)
QuadPtr Lattice::addEmptyQuad(int key) {
    if (contains(key)) return m_hashTable[key];
    QuadPtr cell = std::shared_ptr<Quad>(new Quad(key));
    insert(key, cell);
    return cell;
}

// Add a copy of the given quad
QuadPtr Lattice::addQuad(QuadPtr &quad) {
    if (contains(quad->key())) {
        deleteQuad(quad->key());
    }
    
    // Add a quad at the same position in the grid
    int x, y;
    bool newQuadAdded = false;
    keyToCoord(quad->key(), x, y);
    QuadPtr newQuad = addQuad(quad->key(), x, y, newQuadAdded);

    // Copy coordinates and elements
    for (int i = 0; i < 4; ++i) {
        newQuad->corners[i]->coord(TARGET_POS) = quad->corners[i]->coord(TARGET_POS);
        newQuad->corners[i]->coord(INTERP_POS) = quad->corners[i]->coord(INTERP_POS);
        newQuad->corners[i]->coord(DEFORM_POS) = quad->corners[i]->coord(DEFORM_POS);
    }
    newQuad->setElements(quad->elements());

    return newQuad;
}

void Lattice::deleteQuad(int key) {
    // update adjacent corners
    if (m_hashTable.contains(key)) {
        m_hashTable.value(key)->corners[TOP_LEFT]->quads(BOTTOM_RIGHT) = nullptr;
        m_hashTable.value(key)->corners[TOP_RIGHT]->quads(BOTTOM_LEFT) = nullptr;
        m_hashTable.value(key)->corners[BOTTOM_LEFT]->quads(TOP_RIGHT) = nullptr;
        m_hashTable.value(key)->corners[BOTTOM_RIGHT]->quads(TOP_LEFT) = nullptr;
    }
    m_hashTable.remove(key);
    deleteUnusedCorners();
}

void Lattice::deleteVolatileQuads() {
    QMutableHashIterator<int, QuadPtr> it(m_hashTable);
    while (it.hasNext()) {
        it.next();
        if (it.value()->isVolatile()) {
            it.value()->corners[TOP_LEFT]->quads(BOTTOM_RIGHT) = nullptr;
            it.value()->corners[TOP_RIGHT]->quads(BOTTOM_LEFT) = nullptr;
            it.value()->corners[BOTTOM_LEFT]->quads(TOP_RIGHT) = nullptr;
            it.value()->corners[BOTTOM_RIGHT]->quads(TOP_LEFT) = nullptr;
            it.remove();
        }
    }
    deleteUnusedCorners();
}

void Lattice::deleteEmptyVolatileQuads() {
    QMutableHashIterator<int, QuadPtr> it(m_hashTable);
    while (it.hasNext()) {
        it.next();
        if (it.value()->isVolatile() && it.value()->isEmpty()) {
            it.value()->corners[TOP_LEFT]->quads(BOTTOM_RIGHT) = nullptr;
            it.value()->corners[TOP_RIGHT]->quads(BOTTOM_LEFT) = nullptr;
            it.value()->corners[BOTTOM_LEFT]->quads(TOP_RIGHT) = nullptr;
            it.value()->corners[BOTTOM_RIGHT]->quads(TOP_LEFT) = nullptr;
            it.remove();
        }
    }
    deleteUnusedCorners();
}

void Lattice::deleteUnusedCorners() {
    int size = m_corners.size();
    for (int i = size - 1; i >= 0; i--) {
        Corner *c = m_corners[i];
        // remove corners with no neighboring quads
        if ((c->quads(TOP_LEFT) == nullptr) && (c->quads(TOP_RIGHT) == nullptr) && (c->quads(BOTTOM_LEFT) == nullptr) && (c->quads(BOTTOM_RIGHT) == nullptr)) {
            m_corners.removeAt(i);
            delete c;
            continue;
        }
        // check consistency and remove references to non existing quads
        for (int j = 0; j < 4; ++j) {
            if (c->quads(CornerIndex(j)) != nullptr && c->quads(CornerIndex(j))->corners[(j + 2) % 4] != c) {
                m_corners.removeAt(i);
                delete c;
                break;
            }
            if (c->quads(CornerIndex(j)) != nullptr && !contains(c->quads(CornerIndex(j))->key())) {
                c->quads(CornerIndex(j)) = nullptr;
            }
        }
        // adjust neighbors count
        int nCount = 0;
        for (int j = 0; j < 4; ++j) {
            if (c->quads(CornerIndex(j)) != nullptr) nCount++;
        }
        c->setNbQuads(nCount);
    }

    // restore correct corner key
    for (int i = 0; i < m_corners.size(); i++) {
        m_corners[i]->setKey(i);
    }
    m_maxCornerKey = m_corners.size();
}

/**
 * Works for any simple convex or concave quads
 * If we can guarantee that all quads are convex at all time then we might optimize this function
 */
bool Lattice::quadContainsPoint(QuadPtr quad, const Point::VectorType &p, PosTypeIndex cornerType) {
    if (quad == nullptr) return false;
    Point::VectorType q(-1e7, -1e7);
    Point::VectorType c[4];
    c[0] = quad->corners[TOP_RIGHT]->coord(cornerType);
    c[1] = quad->corners[BOTTOM_RIGHT]->coord(cornerType);
    c[2] = quad->corners[BOTTOM_LEFT]->coord(cornerType);
    c[3] = quad->corners[TOP_LEFT]->coord(cornerType);
    int num = 0;
    for (int i = 0; i < 4; i++) {
        if (Geom::checkSegmentsIntersection(p, q, c[i], c[(i + 1) % 4])) {
            if (Geom::wedge((c[i] - p), (q - p)) != 0) num++;
        }
    }
    return num % 2 == 1;
}

bool Lattice::contains(const Point::VectorType &p, PosTypeIndex cornerType, QuadPtr &quad, int &key) {
    // TODO bounding box test before
    for (auto it = m_hashTable.cbegin(); it != m_hashTable.cend(); ++it) {
        if (quadContainsPoint(it.value(), p, cornerType)) {
            quad = it.value();
            key = it.key();
            return true;
        }
    }
    return false;
}

Point::VectorType Lattice::getUV(const Point::VectorType &p, PosTypeIndex type, int &quadKey) {
    QuadPtr quad = nullptr;

    // check if the given point is in the grid, if so, in which quad?
    if (!contains(p, type, quad, quadKey)) {
        int r, c;
        posToCoord(p, r, c);
        qWarning() << "getUV: can't find point quad (" << p.x() << ", " << p.y() << ") cellSize=" << m_cellSize << ", nbRows=" << m_nbRows << ", nbCols=" << m_nbCols << ", r=" << r
                   << ", c=" << c;
        quadKey = INT_MAX;
        return Point::VectorType::Zero();
    }

    Point::VectorType pos[4];
    for (int i = 0; i < 4; i++) pos[i] = quad->corners[i]->coord(type);

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

    // std::cout << "UV: " << uv.transpose() << std::endl;
    return uv;
}

/**
 * Returns the position of the point in the lattice at the coordinates 'uv' relative to the quad 'quadKey'
 * p is the fallback position in case 'quadKey' is invalid
 */
Point::VectorType Lattice::getWarpedPoint(const Point::VectorType &p, int quadKey, const Point::VectorType &uv, PosTypeIndex type) {
    if (!m_hashTable.contains(quadKey)) {
        qWarning() << "getWarpedPoint: can't find quad with key " << quadKey;
        return p;
    }
    QuadPtr quad = m_hashTable.value(quadKey);
    return (quad->corners[TOP_LEFT]->coord(type) * (1.0 - uv.x()) + quad->corners[TOP_RIGHT]->coord(type) * uv.x()) * (1.0 - uv.y()) 
          + (quad->corners[BOTTOM_LEFT]->coord(type) * (1.0 - uv.x()) + quad->corners[BOTTOM_RIGHT]->coord(type) * uv.x()) * uv.y();
}

bool Lattice::bakeForwardUV(const Stroke *stroke, Interval &interval, UVHash &uvs) {
    if (stroke == nullptr) {
        qWarning() << "cannot compute UVs for this interval: invalid stroke: " << stroke;
        return false;
    }

    // overshoot if possible
    QuadPtr q;
    int k, from = interval.from(), to = interval.to();
    bool isNextPointInLattice = to < stroke->size() - 1 && contains(stroke->points()[interval.to() + 1]->pos(), REF_POS, q, k);
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
        uv.uv = getUV(pos, REF_POS, key);
        uv.quadKey = key;
        uvs.add(stroke->id(), i, uv);
    }

    return true;
}

bool Lattice::bakeBackwardUV(const Stroke *stroke, Interval &interval, const Point::Affine &transform, UVHash &uvs) {
    if (stroke == nullptr) {
        qWarning() << "cannot compute UVs for this interval: invalid stroke: " << stroke;
        return false;
    }

    // overshoot if possible
    QuadPtr q;
    int k, from = interval.from(), to = interval.to();
    bool isNextPointInLattice = to < stroke->size() - 1 && contains(stroke->points()[interval.to() + 1]->pos(), TARGET_POS, q, k);
    if (isNextPointInLattice)
        to += 1;
    else if (to < stroke->size() - 1)
        interval.setOvershoot(false);

    int keyBack = INT_MIN;
    for (size_t i = from; i <= to; ++i) {
        const Point::VectorType &pos = stroke->points()[i]->pos();
        stroke->points()[i]->initId(stroke->id(), i);
        UVInfo uv;
        if (uvs.has(stroke->id(), i)) uv = uvs.get(stroke->id(), i);
        uv.uv = getUV(transform * pos, TARGET_POS, keyBack);
        uv.quadKey = keyBack;
        if (uv.quadKey == INT_MAX) std::cout << "Error bakeBackwardUVs: " << stroke->id() << ": " << i << std::endl;
        uvs.add(stroke->id(), i, uv);
    }

    return true;
}

std::vector<Point::VectorType> Lattice::pinsDisplacementVectors() const {
    std::vector<Point::VectorType> pinsPositions;
    for (QuadPtr quad : m_hashTable) {
        if (quad->isPinned()) {
            pinsPositions.push_back(quad->pinPos() - quad->getPoint(quad->pinUV(), REF_POS));
        }
    }
    return pinsPositions;
}

void Lattice::resetDeformation() {
    for (int i = 0; i < m_corners.size(); ++i) {
        m_corners[i]->coord(INTERP_POS) = m_corners[i]->coord(REF_POS);
        m_corners[i]->coord(TARGET_POS) = m_corners[i]->coord(REF_POS);
        m_corners[i]->coord(DEFORM_POS) = m_corners[i]->coord(REF_POS);
    }
    m_arapDirty = true;
    m_precomputeDirty = true;
}

void Lattice::drawLattice(QPainter &painter, qreal interpFactor, const QColor &color, PosTypeIndex type) const {
    QPen gridPen(QBrush(color, Qt::SolidPattern), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    gridPen.setColor(color);
    painter.setPen(gridPen);
    painter.setOpacity(1.0);

    for (auto it = m_hashTable.constBegin(); it != m_hashTable.constEnd(); ++it) {
        QuadPtr quad = it.value();
        if (quad == nullptr) {
            qWarning("quad error");
            continue;
        }

        for (int i = 0; i < 4; i++) {
            Corner *c0 = quad->corners[i];
            Corner *c1 = quad->corners[(i + 1) % 4];
            if (c0 == nullptr) qWarning("Corner error");
            Point::VectorType p0 = c0->coord(type);
            Point::VectorType p1 = c1->coord(type);
            painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
        }
    }
}

void Lattice::drawLattice(QPainter &painter, const QColor &color, VectorKeyFrame *keyframe, int groupID, int inbetween) const {
    QPen gridPen(QBrush(color, Qt::SolidPattern), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    gridPen.setColor(color);
    painter.setPen(gridPen);
    painter.setOpacity(0.5);

    const std::vector<Point::VectorType> &corners = keyframe->inbetweenCorners(inbetween - 1).value(groupID);

    for (auto it = m_hashTable.constBegin(); it != m_hashTable.constEnd(); ++it) {
        QuadPtr quad = it.value();
        if (quad == nullptr) {
            qCritical() << "drawLattice: null quad";
            continue;
        }
        for (int i = 0; i < 4; i++) {
            Corner *c0 = quad->corners[i];
            Corner *c1 = quad->corners[(i + 1) % 4];
            if (c0 == nullptr || c1 == nullptr) {
                qCritical() << "drawLattice: null corner";
            }
            if (c0->getKey() >= corners.size() || c1->getKey() >= corners.size()) {
                qCritical() << "drawLattice: invalid corner id";
            }
            Point::VectorType p0 = corners[c0->getKey()];
            Point::VectorType p1 = corners[c1->getKey()];
            painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
        }
    }
}

void Lattice::drawPins(QPainter &painter) {
    static QPen p(QBrush(Qt::darkRed, Qt::SolidPattern), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(p);
    for (QuadPtr quad : m_hashTable) {
        if (quad->isPinned()) {
            QPointF pinPos = QPointF(quad->pinPos().x(), quad->pinPos().y());
            Point::VectorType pinUV = quad->getPoint(quad->pinUV(), TARGET_POS);
            QPointF pinUVPos = QPointF(pinUV.x(), pinUV.y());
            p.setColor(Qt::darkRed);
            painter.setPen(p);
            painter.drawEllipse(pinPos, 1.0, 1.0);
            p.setColor(Qt::black);
            painter.setPen(p);
            painter.drawEllipse(pinUVPos, 1.0, 1.0);
            painter.drawLine(pinPos, pinUVPos);
        }
    }
}

/**
 * Precompute the sparse matrices P^T and prefactor P^T*P for later computations
 * See Baxter et al. 2008
 */
void Lattice::precompute() {
    if (!m_singleConnectedComponent) {
        qWarning() << "Cannot precompute a lattice with multiple connected components! ";
        return;
    }

    qDebug() << "PRECOMPUTING GRID (Q: " << m_hashTable.size() << ", C: " << m_corners.size() << ")";
    StopWatch sw("Precompute ARAP LHS");
    std::vector<TripletD> P_triplets;
    int nQuads = m_hashTable.size();
    int nCorners = m_corners.size();
    int P_rows = 8 * nQuads;
    int triRow = 0;
    QuadPtr q = *m_hashTable.constBegin();
    double size = (q->corners[TOP_RIGHT]->coord(REF_POS) - q->corners[TOP_LEFT]->coord(REF_POS)).norm();
    double triArea = size * size / 2.0;

    // Compute P (sparse) and store its transpose to construct the RHS of the equation later
    // TODO refactorize concatenation
    for (auto it = m_hashTable.constBegin(); it != m_hashTable.constEnd(); ++it) {
        q = it.value();
        computePStar(q, TOP_LEFT, TOP_RIGHT, triRow, false, P_triplets);
        triRow++;
        computePStar(q, TOP_RIGHT, BOTTOM_RIGHT, triRow, false, P_triplets);
        triRow++;
    }
    for (auto it = m_hashTable.constBegin(); it != m_hashTable.constEnd(); ++it) {
        q = it.value();
        computePStar(q, TOP_LEFT, TOP_RIGHT, triRow, true, P_triplets);
        triRow++;
        computePStar(q, TOP_RIGHT, BOTTOM_RIGHT, triRow, true, P_triplets);
        triRow++;
    }
    SparseMatrix<double, ColMajor> P(P_rows, nCorners);
    P.setFromTriplets(P_triplets.begin(), P_triplets.end());
    m_Pt = P.transpose();

    // Assembling diagonal W matrix
    m_W = VectorXd(P_rows);
    for (int i = 0; i < P_rows; ++i) {
        m_W[i] = triArea;
    }

    // Assembling LHS (with constraint)
    unsigned int nbConstraint = m_constraintsIdx.size() > 0 ? m_constraintsIdx.size() : 1;
    unsigned int idx = m_corners.size();
    SparseMatrix<double, ColMajor> PTP = m_Pt * m_W.asDiagonal() * P;
    SparseMatrix<double, ColMajor> LHS(m_corners.size() + nbConstraint, m_corners.size() + nbConstraint);

    // main constraint (linear interp of center of mass)
    LHS.innerVectors(0, m_corners.size()) = PTP.innerVectors(0, m_corners.size());  // TODO: is there a more efficient way to do this than using the intermediate var PTP?
    if (m_constraintsIdx.size() == 0) {
        float constraintMean = 1.0 / m_corners.size();
        for (int i = 0; i < m_corners.size(); ++i) {
            LHS.insert(idx, i) = constraintMean;
            LHS.insert(i, idx) = constraintMean;
        }
        ++idx;
    }

    // user defined hard constraints
    for (unsigned int constraintIdx : m_constraintsIdx) {
        const Trajectory *traj = m_keyframe->trajectoryConstraintPtr(constraintIdx);
        const UVInfo &latticeCoord = traj->latticeCoord();

        QuadPtr quad = m_hashTable[latticeCoord.quadKey];
        // the constraint coeff vector and its transpose are set at the same time
        LHS.insert(idx, quad->corners[TOP_LEFT]->getKey()) = LHS.insert(quad->corners[TOP_LEFT]->getKey(), idx) = (1.0 - latticeCoord.uv.x()) * (1.0 - latticeCoord.uv.y());
        LHS.insert(idx, quad->corners[TOP_RIGHT]->getKey()) = LHS.insert(quad->corners[TOP_RIGHT]->getKey(), idx) = latticeCoord.uv.x() * (1.0 - latticeCoord.uv.y());
        LHS.insert(idx, quad->corners[BOTTOM_RIGHT]->getKey()) = LHS.insert(quad->corners[BOTTOM_RIGHT]->getKey(), idx) = latticeCoord.uv.x() * latticeCoord.uv.y();
        LHS.insert(idx, quad->corners[BOTTOM_LEFT]->getKey()) = LHS.insert(quad->corners[BOTTOM_LEFT]->getKey(), idx) = (1.0 - latticeCoord.uv.x()) * latticeCoord.uv.y();
        ++idx;
    }

    // Factorization of LHS
    LHS.makeCompressed();
    m_LU.compute(LHS);
    if (m_LU.info() != Success) {
        std::cout << "ERROR DURING FACTORIZATION" << std::endl;
        std::cout << LHS << std::endl;
        assert(0);
    }

    // Compute ref and target center of mass
    m_refCM = centerOfGravity(REF_POS);
    m_tgtCM = centerOfGravity(TARGET_POS);

    m_precomputeDirty = false;
    m_arapDirty = true;
    sw.stop();
}

/**
 * Compute the interpolation of the lattice between its REF_POS and TARGET_POS.
 * Stores the results in INTERP_POS.
 * The resulting interpolated lattice can be additionally transformed by a given rigid transformation. 
 * 
 * @param alphaLinear Linear interpolating factor between the two adjacent keyframes (from the timeline: (curFrame - prevKeyFrame) / (nextKeyFrame - prevKeyFrame))
 * @param alpha Remapping of the linear interpolating factor by the group's spacing function. This is what controls the interpolation.  
 * @param globalRigidTransform Global rigid transformation applied after the interpolation.
 * @param useRigidTransform If true the global rigid transformation is applied.
 */
void Lattice::interpolateARAP(float alphaLinear, float alpha, const Point::Affine &globalRigidTransform, bool useRigidTransform) {
    qDebug() << "** Interpolating lattice at t=" << alpha;
    StopWatch sw("ARAP interpolation");

    useRigidTransform = useRigidTransform && k_useGlobalRigidTransform;
    m_currentPrecomputedTime = alpha;

    // Lattices with multiple connected components cannot be interpolated, return reference or target configuration
    if (!m_singleConnectedComponent) {
        if (alpha < 1.0) copyPositions(this, REF_POS, INTERP_POS);
        else             copyPositions(this, TARGET_POS, INTERP_POS);
        return;
    }

    int nQuads = m_hashTable.size();
    MatrixXd A(2, 8 * nQuads);
    qreal t = alpha;

    // Compute A(t)
    QuadPtr q;
    int i = 0;
    for (auto it = m_hashTable.begin(); it != m_hashTable.end(); ++it) {
        q = it.value();
        computeQuadA(q, A, i, t, false);
    }
    for (auto it = m_hashTable.begin(); it != m_hashTable.end(); ++it) {
        q = it.value();
        computeQuadA(q, A, i, t, true);
    }

    // Assembling final RHS matrix and concatenating constraints values
    unsigned int nbConstraint = m_constraintsIdx.size() > 0 ? m_constraintsIdx.size() : 1;
    unsigned int idx = m_corners.size();
    MatrixXd PTAD(m_corners.size() + nbConstraint, 2);
    PTAD.block(0, 0, m_corners.size(), 2) = m_Pt * m_W.asDiagonal() * A.transpose();

    // Main constraint (linear interp of center of mass)
    if (m_constraintsIdx.size() == 0) {
        PTAD(idx, 0) = m_refCM.x() * (1.0 - t) + m_tgtCM.x() * t;
        PTAD(idx, 1) = m_refCM.y() * (1.0 - t) + m_tgtCM.y() * t;
        ++idx;
    }

    // User defined constraints values
    float offset;
    for (unsigned int constraintIdx : m_constraintsIdx) {
        Trajectory *traj = m_keyframe->trajectoryConstraintPtr(constraintIdx);
        traj->localOffset()->frameChanged(alphaLinear);
        offset = traj->localOffset()->get();
        Point::VectorType pos = traj->eval(t + (std::abs(offset) < 1e-5f ? 0.0f : offset));
        PTAD(idx, 0) = pos.x();
        PTAD(idx, 1) = pos.y();
        ++idx;
    }

    MatrixXd V = m_LU.solve(PTAD).eval();
    if (m_LU.info() != Success) {
        std::cout << "ERROR DURING SOLVE" << std::endl;
        assert(0);
    }

    // Setting new interpolated vertices in corners INTERP_POS coordinates
    Corner *c;
    for (auto it = m_corners.begin(); it != m_corners.end(); ++it) {
        c = *it;
        i = c->getKey();
        c->coord(INTERP_POS) = V.row(i);
        if (useRigidTransform) {
            c->coord(INTERP_POS) = globalRigidTransform * c->coord(INTERP_POS);
        }
    }

    m_arapDirty = false;
    sw.stop();
}

/** 
 * Compute P* for the two triangles of the given quad and add them to the sparse matrix P (via the triplet list)
 * See Baxter et al. 2008
 */
void Lattice::computePStar(QuadPtr q, int cornerI, int cornerJ, int triRow, bool inverseOrientation, std::vector<TripletD> &P_triplets) {
    MatrixXd P(3, 2), D(2, 3), P_star(2, 3);
    D << 1, 0, -1, 0, 1, -1;
    int i, j, k;

    PosTypeIndex posType = inverseOrientation ? TARGET_POS : REF_POS;

    P(0, 0) = q->corners[cornerI]->coord(posType).x();
    P(0, 1) = q->corners[cornerI]->coord(posType).y();
    P(1, 0) = q->corners[cornerJ]->coord(posType).x();
    P(1, 1) = q->corners[cornerJ]->coord(posType).y();
    P(2, 0) = q->corners[BOTTOM_LEFT]->coord(posType).x();
    P(2, 1) = q->corners[BOTTOM_LEFT]->coord(posType).y();

    P_star = (D * P).inverse() * D;

    i = q->corners[cornerI]->getKey();
    j = q->corners[cornerJ]->getKey();
    k = q->corners[BOTTOM_LEFT]->getKey();

    P_triplets.push_back(TripletD(2 * triRow, i, P_star(0, 0)));
    P_triplets.push_back(TripletD(2 * triRow, j, P_star(0, 1)));
    P_triplets.push_back(TripletD(2 * triRow, k, P_star(0, 2)));
    P_triplets.push_back(TripletD(2 * triRow + 1, i, P_star(1, 0)));
    P_triplets.push_back(TripletD(2 * triRow + 1, j, P_star(1, 1)));
    P_triplets.push_back(TripletD(2 * triRow + 1, k, P_star(1, 2)));
}

/**
 * Compute interpolated target linear maps A(t) (concatenated for the two triangles of the given quad)
 * See Baxter et al. 2008
 */

void Lattice::computeQuadA(QuadPtr q, MatrixXd &At, int &i, float t, bool inverseOrientation) {
    Matrix2d A_interp, I;
    Matrix2d A, Rt, S;
    I = Matrix2d::Identity();
    if (inverseOrientation) t = 1.0f - t;

    // Compute and concatenate the interpolated linear transformation of the triangle formed by CornerA, CornerB and the bottom left corner of the quad
    auto computeTriangleA = [&](CornerIndex cornerA, CornerIndex cornerB) {
        Arap::computeJAM(q, cornerA, cornerB, inverseOrientation, A);
        double angle = Arap::polarDecomp(A, S);
        // Interpolated rotation matrix
        Rt << cos(angle * t), -sin(angle * t), sin(angle * t), cos(angle * t);
        // Interpolated linear transformation of the triangle (rotation and shearing interpolated independently)
        // A_interp = Rt * ((1 - t) * I + t * S);
        A_interp = Rt * S.pow(t);
        // Concatenate transposed result to the input matrix A(t)
        At(0, i) = A_interp(0, 0);
        At(1, i) = A_interp(1, 0);
        i++;
        At(0, i) = A_interp(0, 1);
        At(1, i) = A_interp(1, 1);
        i++;
    };

    computeTriangleA(TOP_LEFT, TOP_RIGHT);
    computeTriangleA(TOP_RIGHT, BOTTOM_RIGHT);
}

void Lattice::applyTransform(const Point::Affine &transform, PosTypeIndex ref, PosTypeIndex dst) {
    for (Corner *corner : m_corners) {
        corner->coord(dst) = transform * corner->coord(ref);
    }
}

void Lattice::copyPositions(const Lattice *dst, PosTypeIndex srcPos, PosTypeIndex dstPos) {
    for (auto it = dst->m_hashTable.constBegin(); it != dst->m_hashTable.constEnd(); ++it) {
        QuadPtr quad = m_hashTable[it.key()];
        for (int i = 0; i < 4; ++i) {
            quad->corners[i]->coord(dstPos) = it.value()->corners[i]->coord(srcPos);
        }
    }
}

/**
 * Assume target is a copy of the lattice (same topology and quad keys)
 * Set srcPos corners position to the given target lattice targetPos corners position
 */
void Lattice::moveSrcPosTo(const Lattice *target, PosTypeIndex srcPos, PosTypeIndex targetPos) {
    std::set<int> visitedCorners;
    Point::VectorType posTarget, offset;
    for (auto it = target->m_hashTable.constBegin(); it != target->m_hashTable.constEnd(); ++it) {
        QuadPtr quadTarget = it.value();
        QuadPtr quad = m_hashTable[it.key()];
        for (int i = 0; i < 4; ++i) {
            if (!visitedCorners.insert(quad->corners[i]->getKey()).second) continue;
            quad->corners[i]->coord(srcPos) = quadTarget->corners[i]->coord(targetPos);
        }
    }
    setArapDirty();
}

Point::VectorType Lattice::centerOfGravity(PosTypeIndex type) {
    Point::VectorType p = Point::VectorType::Zero();
    for (Corner *corner : m_corners) {
        p += corner->coord(type);
    }
    return p /= m_corners.size();
}

/**
 * Returns true if the 2 given quads are adjacent
 */
bool Lattice::areQuadsConnected(int quadKeyA, int quadKeyB) {
    int xA, yA, xB, yB;
    keyToCoord(quadKeyA, xA, yA);
    keyToCoord(quadKeyB, xB, yB);
    return std::abs(xA - xB) <= 1 || std::abs(yA - yB) <= 1;
}

/**
 * Check if the lattice is a single connected component (DFS), save the result in the flag m_singleConnectedComponent
 */
bool Lattice::isConnected() {
    if (m_hashTable.empty()) {
        m_singleConnectedComponent = false;
        return false;
    }

    for (QuadPtr &quad : m_hashTable) {
        quad->setFlag(false);
    }

    QStack<int> toVisit;
    toVisit.push((*m_hashTable.begin())->key());

    // Add the quad to the stack if its coordinates are valid and if its visited flag is false
    auto pushQuad = [&](int x, int y, int key) {
        if (x >= 0 && x < nbCols() && y >= 0 && y < nbRows() && m_hashTable.contains(key) && !m_hashTable.value(key)->flag()) {
            toVisit.push(m_hashTable.value(key)->key());
        }
    };

    int x, y;
    while (!toVisit.isEmpty()) {
        const QuadPtr &curQuad = m_hashTable.value(toVisit.pop());
        if (curQuad->flag()) continue;
        curQuad->setFlag(true);
        keyToCoord(curQuad->key(), x, y);
        for (int i = x - 1; i <= x + 1; ++i) {
            for (int j = y - 1; j <= y + 1; ++j) {
                if (i == x && j == y) continue;
                pushQuad(i, j, coordToKey(i, j));
            }
        }
    }

    for (QuadPtr &quad : m_hashTable) {
        if (!quad->flag()) {
            m_singleConnectedComponent = false;
            return false;
        }
    }

    m_singleConnectedComponent = true;
    return true;
}

/**
 * Output list of connected components as lists of quad keys
 * TODO output list of lattices?
 */
void Lattice::getConnectedComponents(std::vector<std::vector<int>> &outputComponents, bool overrideFlag) {
    outputComponents.clear();

    if (overrideFlag) {
        for (QuadPtr &quad : m_hashTable) {
            quad->setFlag(false);
        }
    }

    QStack<int> toVisit;

    // Add the quad to the stack if its coordinate are valid and if its flag is false
    auto pushQuad = [&](int x, int y, int key) {
        if (x >= 0 && x < nbCols() && y >= 0 && y < nbRows() && m_hashTable.contains(key) && !m_hashTable.value(key)->flag()) {
            toVisit.push(m_hashTable.value(key)->key());
        }
    };

    while (true) {
        toVisit.clear();
        std::vector<int> connectedComponent;

        // Are there any non visited quad remaining?
        Quad *start = nullptr;
        for (QuadPtr &quad : m_hashTable) {
            if (!quad->flag()) {
                start = quad.get();
            }
        }

        if (start == nullptr)
            break;
        
        // Go through the connected component starting from the start quad
        int x, y;
        toVisit.push(start->key());
        while (!toVisit.isEmpty()) {
            const QuadPtr &curQuad = m_hashTable.value(toVisit.pop());
            if (curQuad->flag()) continue;
            curQuad->setFlag(true);
            connectedComponent.push_back(curQuad->key());
            keyToCoord(curQuad->key(), x, y);
            for (int i = x - 1; i <= x + 1; ++i) {
                for (int j = y - 1; j <= y + 1; ++j) {
                    if (i == x && j == y) continue;
                    pushQuad(i, j, coordToKey(i, j));
                }
            }
        }

        // Add the connected component to the output list
        outputComponents.push_back(connectedComponent);
    }
}

void Lattice::debug(std::ostream &os) const {
    for (Corner *corner : m_corners) {
        os << "REF(" << corner->coord(REF_POS).transpose() << ")  |   DEFORM(" << corner->coord(DEFORM_POS).transpose() << ")  | INTERP" << corner->coord(INTERP_POS).transpose()
           << ")  | TARGET(" << corner->coord(TARGET_POS).transpose() << ")" << std::endl;
    }
    os << "#corners=" << m_corners.size() << std::endl;
    os << "#quads=" << m_hashTable.size() << std::endl;
}

void Lattice::restoreKeysRetrocomp(Group *group, Editor *editor) {
    QHash<int, int> keysMap;

    auto updateTrajectories = [](Group *group, const QHash<int, int> &keysMap) {
        VectorKeyFrame *key = group->getParentKeyframe();
        for (const std::shared_ptr<Trajectory> &traj : key->trajectories()) {
            if (traj->group()->id() == group->id()) {
                traj->setQuadKey(keysMap.value(traj->latticeCoord().quadKey));
            }
        }
    };

    m_oGrid = Eigen::Vector2i(editor->tabletCanvas()->canvasRect().x(), editor->tabletCanvas()->canvasRect().y());
    m_nbCols = std::ceil((float)editor->tabletCanvas()->canvasRect().width() / m_cellSize);
    m_nbRows = std::ceil((float)editor->tabletCanvas()->canvasRect().height() / m_cellSize);

    // update the non-breakdown group
    QHash<int, QuadPtr> oldHash = m_hashTable;
    m_hashTable.clear();
    for (QuadPtr q : oldHash) {
        Point::VectorType refCentroid = Point::VectorType::Zero();
        for (int i = 0; i < 4; ++i) refCentroid += q->corners[i]->coord(REF_POS);
        refCentroid *= 0.25;
        int oldKey = q->key();
        int newKey = posToKey(refCentroid);
        keysMap.insert(oldKey, newKey);
        q->setKey(newKey);
        m_hashTable.insert(newKey, q);
    }
    group->uvs().clear();

    for (auto it = group->strokes().begin(); it != group->strokes().end(); ++it) {
        for (Interval &interval : it.value()) {
            bakeForwardUV(group->getParentKeyframe()->stroke(it.key()), interval, group->uvs());
        }
    }
    updateTrajectories(group, keysMap);

    // update following breakdowns
    Group *curGroup = group;
    while (curGroup->nextPostGroup() != nullptr) {
        curGroup->lattice()->setOrigin(Eigen::Vector2i(editor->tabletCanvas()->canvasRect().x(), editor->tabletCanvas()->canvasRect().y()));
        curGroup->lattice()->m_nbCols = std::ceil((float)editor->tabletCanvas()->canvasRect().width() / curGroup->lattice()->m_cellSize);
        curGroup->lattice()->m_nbRows = std::ceil((float)editor->tabletCanvas()->canvasRect().height() / curGroup->lattice()->m_cellSize);

        oldHash = curGroup->lattice()->m_hashTable;
        curGroup->lattice()->m_hashTable.clear();
        for (QuadPtr q : curGroup->lattice()->hash()) {
            q->setKey(keysMap.value(q->key()));
            curGroup->lattice()->m_hashTable.insert(keysMap.value(q->key()), q);
        }
        curGroup->uvs().clear();
        for (auto it = curGroup->strokes().begin(); it != curGroup->strokes().end(); ++it) {
            for (Interval &interval : it.value()) {
                bakeForwardUV(curGroup->getParentKeyframe()->stroke(it.key()), interval, curGroup->uvs());
            }
        }
        updateTrajectories(curGroup, keysMap);
    }
}