/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
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
#include "bezier2D.h"
#include "qteigen.h"
#include "mask.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStack>

#include <set>
#include <unsupported/Eigen/MatrixFunctions>

typedef Eigen::Triplet<double> TripletD;

static dkBool k_useGlobalRigidTransform("Options->Drawing->Use global transform for groups", true);
static dkBool k_drawDebugLattice("Debug->Draw lattice debug", false);

struct LatticeVtx {
    GLfloat x;
    GLfloat y;
    GLubyte flags;
};

Lattice::Lattice(VectorKeyFrame *keyframe)
    : m_keyframe(keyframe),
      m_nbCols(0),
      m_nbRows(0),
      m_cellSize(0),
      m_oGrid(Eigen::Vector2i::Zero()),
      m_toRestPos(Point::Affine::Identity()),
      m_scaling(Point::Affine::Identity()),
      m_precomputeDirty(true),
      m_arapDirty(true),
      m_backwardUVDirty(true),
      m_singleConnectedComponent(false),
      m_retrocomp(false),
      m_currentPrecomputedTime(-1.0f),
      m_maxCornerKey(0),
      m_rot(0.0),
      m_scale(1.0),
      m_vbo(QOpenGLBuffer::VertexBuffer), 
      m_ebo(QOpenGLBuffer::IndexBuffer), 
      m_bufferCreated(false), 
      m_bufferDestroyed(false), 
      m_bufferDirty(true) {

      }

Lattice::Lattice(const Lattice &other)
    : m_keyframe(other.m_keyframe),
      m_nbCols(other.m_nbCols),
      m_nbRows(other.m_nbRows),
      m_cellSize(other.m_cellSize),
      m_oGrid(other.m_oGrid),
      m_toRestPos(other.m_toRestPos),
      m_scaling(other.m_scaling),
      m_precomputeDirty(true),
      m_arapDirty(true),
      m_backwardUVDirty(true),
      m_retrocomp(false),
      m_currentPrecomputedTime(-1.0),
      m_maxCornerKey(0),
      m_rot(0.0),
      m_scale(1.0), 
      m_vbo(QOpenGLBuffer::VertexBuffer), 
      m_ebo(QOpenGLBuffer::IndexBuffer), 
      m_bufferCreated(false), 
      m_bufferDestroyed(false), 
      m_bufferDirty(true) {
    // create quads and copy corners
    bool isNewQuad = false;
    int x, y;
    for (auto it = other.m_quads.constBegin(); it != other.m_quads.constEnd(); ++it) {
        keyToCoord(it.key(), x, y);
        QuadPtr quad = addQuad(it.key(), x, y, isNewQuad);
        quad->setFlags(it.value()->flags());
        for (int i = 0; i < 4; ++i) {
            quad->corners[i]->coord(REF_POS) = it.value()->corners[i]->coord(REF_POS);
            quad->corners[i]->coord(TARGET_POS) = it.value()->corners[i]->coord(TARGET_POS);
            quad->corners[i]->coord(INTERP_POS) = it.value()->corners[i]->coord(INTERP_POS);
            quad->corners[i]->coord(DEFORM_POS) = it.value()->corners[i]->coord(DEFORM_POS);
            quad->corners[i]->setFlags(it.value()->corners[i]->flags());
            quad->corners[i]->setDeformable(true);
        }
    }
    // Copy quads stroke interval?
    isConnected();
}

Lattice::Lattice(const Lattice &other, const std::vector<int> &quads)
    : m_keyframe(other.m_keyframe),
      m_nbCols(other.m_nbCols),
      m_nbRows(other.m_nbRows),
      m_cellSize(other.m_cellSize),
      m_oGrid(other.m_oGrid),
      m_scaling(Point::Affine::Identity()),
      m_precomputeDirty(true),
      m_arapDirty(true),
      m_backwardUVDirty(true),
      m_retrocomp(false),
      m_currentPrecomputedTime(-1.0),
      m_maxCornerKey(0),
      m_rot(0.0),
      m_scale(1.0), 
      m_vbo(QOpenGLBuffer::VertexBuffer), 
      m_ebo(QOpenGLBuffer::IndexBuffer), 
      m_bufferCreated(false), 
      m_bufferDestroyed(false), 
      m_bufferDirty(true) {
    bool isNewQuad = false;
    int x, y;
    for (int quadKey : quads) {
        keyToCoord(quadKey, x, y);
        if (!other.contains(quadKey)) qCritical() << "Error in Lattice constructor: quad key " << quadKey << " does not exist in the other lattice.";
        QuadPtr otherQuad = other.quad(quadKey);
        QuadPtr newQuad = addQuad(quadKey, x, y, isNewQuad);
        newQuad->setFlags(otherQuad->flags());
        for (int i = 0; i < 4; ++i) {
            newQuad->corners[i]->coord(REF_POS) = otherQuad->corners[i]->coord(REF_POS);
            newQuad->corners[i]->coord(TARGET_POS) = otherQuad->corners[i]->coord(TARGET_POS);
            newQuad->corners[i]->coord(INTERP_POS) = otherQuad->corners[i]->coord(INTERP_POS);
            newQuad->corners[i]->coord(DEFORM_POS) = otherQuad->corners[i]->coord(DEFORM_POS);
            newQuad->corners[i]->setFlags(otherQuad->corners[i]->flags());
            newQuad->corners[i]->setDeformable(true);
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
    // Save grid attributes
    // latticeElt.setAttribute("nbInterStrokes", uint(nbInterStrokes()));
    latticeElt.setAttribute("cellSize", int(cellSize()));
    latticeElt.setAttribute("nbCols", nbCols());
    latticeElt.setAttribute("nbRows", nbRows());
    latticeElt.setAttribute("origin_x", origin().x());
    latticeElt.setAttribute("origin_y", origin().y());

    // Save quads
    int count = 0;
    for (auto it = m_quads.begin(); it != m_quads.end(); ++it) {
        QDomElement quadElt = doc.createElement("quad");
        quadElt.setAttribute("key", int(it.key()));
        quadElt.setAttribute("flags", QString::fromUtf8(it.value()->flags().to_string().data(), it.value()->flags().size()));

        // TODO: save strokerangehash m_elements (or recompute them)
        latticeElt.appendChild(quadElt);
        count++;
    }

    // Save corners
    for (Corner *c : m_corners) {
        QDomElement cornerElt = doc.createElement("corner");
        cornerElt.setAttribute("key", int(c->getKey()));
        cornerElt.setAttribute("flags", QString::fromUtf8(c->flags().to_string().data(), c->flags().size()));
        cornerElt.setAttribute("coord_TARGET_POS_x", c->coord(TARGET_POS).x());
        cornerElt.setAttribute("coord_TARGET_POS_y", c->coord(TARGET_POS).y());
        cornerElt.setAttribute("coord_REF_POS_x", c->coord(REF_POS).x());
        cornerElt.setAttribute("coord_REF_POS_y", c->coord(REF_POS).y());
        cornerElt.setAttribute("quadNum", c->nbQuads());
        cornerElt.setAttribute("quadKey_0", c->quads(TOP_LEFT) == nullptr ? INT_MAX : c->quads(TOP_LEFT)->key());
        cornerElt.setAttribute("quadKey_1", c->quads(TOP_RIGHT) == nullptr ? INT_MAX : c->quads(TOP_RIGHT)->key());
        cornerElt.setAttribute("quadKey_2", c->quads(BOTTOM_RIGHT) == nullptr ? INT_MAX : c->quads(BOTTOM_RIGHT)->key());
        cornerElt.setAttribute("quadKey_3", c->quads(BOTTOM_LEFT) == nullptr ? INT_MAX : c->quads(BOTTOM_LEFT)->key());
        latticeElt.appendChild(cornerElt);
    }

    // Save toRestTransform
    QDomElement transformElt = doc.createElement("transform");
    QString string;
    QTextStream stream(&string);
    auto matrix = m_toRestPos.matrix();
    stream << matrix(0, 0) << " " << matrix(0, 1) << " " << matrix(0, 2) << " ";
    stream << matrix(1, 0) << " " << matrix(1, 1) << " " << matrix(1, 2) << " ";
    stream << matrix(2, 0) << " " << matrix(2, 1) << " " << matrix(2, 2) << " ";
    QDomText txt = doc.createTextNode(string);
    transformElt.appendChild(txt);
    latticeElt.appendChild(transformElt);    
}

void Lattice::load(QDomElement &latticeElt) {
    init(latticeElt.attribute("cellSize").toInt(), latticeElt.attribute("nbCols").toInt(), latticeElt.attribute("nbRows").toInt(),
         Eigen::Vector2i(latticeElt.attribute("origin_x").toInt(), latticeElt.attribute("origin_y").toInt()));

    // Load quads
    QDomElement quadElt = latticeElt.firstChildElement("quad");
    bool retrocomp = true;
    while (!quadElt.isNull()) {
        int key = quadElt.attribute("key").toInt();
        bool isNewQuad = false;
        QuadPtr quad = addEmptyQuad(key);
        if (quadElt.hasAttribute("antialias")) retrocomp = false;
        quad->setFlags((std::bitset<8>(quadElt.attribute("flags", "00000000").toStdString())));
        if (quadElt.hasAttribute("antialias")) {
            quad->setPivot((bool)quadElt.attribute("antialias").toInt()); // retrocomp
        }
        quadElt = quadElt.nextSiblingElement("quad");
    }
    m_retrocomp = retrocomp;

    // Load corners
    QDomElement cornerElt = latticeElt.firstChildElement("corner");
    while (!cornerElt.isNull()) {
        int key = cornerElt.attribute("key").toInt();
        Corner *c = new Corner();
        c->setKey(key);
        c->setFlags(std::bitset<8>(cornerElt.attribute("flags", "00000000").toStdString()));
        c->setNbQuads(cornerElt.attribute("quadNum").toInt());
        c->coord(TARGET_POS) = Point::VectorType(cornerElt.attribute("coord_TARGET_POS_x").toDouble(), cornerElt.attribute("coord_TARGET_POS_y").toDouble());
        c->coord(REF_POS) = Point::VectorType(cornerElt.attribute("coord_REF_POS_x").toDouble(), cornerElt.attribute("coord_REF_POS_y").toDouble());
        c->coord(DEFORM_POS) = c->coord(REF_POS);
        c->coord(INTERP_POS) = c->coord(REF_POS);
        // Set quad/corner correspondences
        for (int i = 0; i < 4; ++i) {
            int quadKey = cornerElt.attribute("quadKey_" + QString::number(i)).toInt();
            if (contains(quadKey)) {
                c->quads(CornerIndex(i)) = m_quads[quadKey];
                c->quads(CornerIndex(i))->corners[(i + 2) % 4] = c;
            }
        }
        m_corners.push_back(c);
        cornerElt = cornerElt.nextSiblingElement("corner");
    }

    // Load transform
    QDomElement transformElt = latticeElt.firstChildElement("transform");
    QString string = transformElt.text();
    QTextStream stream(&string);
    auto matrix = m_toRestPos.matrix();
    stream >> matrix(0, 0) >> matrix(0, 1) >> matrix(0, 2);
    stream >> matrix(1, 0) >> matrix(1, 1) >> matrix(1, 2);
    stream >> matrix(2, 0) >> matrix(2, 1) >> matrix(2, 2);
    m_toRestPos.matrix() = matrix;

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
    m_quads.clear();
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
    QMutableHashIterator<int, QuadPtr> it(m_quads);
    while (it.hasNext()) {
        it.next();
        it.value()->removeStroke(strokeId);
        if (it.value()->nbForwardStrokes() == 0 && it.value()->nbBackwardStrokes() == 0 && !breakdown) {
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
        // qDebug() << "already contained!";
        // for (int i = 0; i < 4; ++i) {
        //     std::cout << q->corners[i]->coord(REF_POS).transpose() << std::endl;
        // }
        return q;
    }

    int r, c;
    Point::VectorType pointRest = m_toRestPos * point;
    posToCoord(pointRest, r, c);    
    key = coordToKey(r, c);

    QuadPtr quad = addQuad(key, r, c, isNewQuad);
    if (isNewQuad){
        for (Corner * corner : quad->corners){
            if (corner->nbQuads() < 2){
                corner->coord(REF_POS) = m_toRestPos.inverse() * corner->coord(REF_POS);
                corner->coord(TARGET_POS) = m_toRestPos.inverse() * corner->coord(TARGET_POS);
            }
        }
    }

    return quad;
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
        return m_quads[key];
    }

    // Create new cell
    QuadPtr cell = std::shared_ptr<Quad>(new Quad(key));
    isNewQuad = true;

    auto linkQuad = [&](int x, int y, CornerIndex cornerIdx, CornerIndex quadIdx, CornerIndex destIdx) {
        if (x < 0 || y < 0 || x >= nbCols() || y >= nbRows()) return;
        int nKey = coordToKey(x, y);
        if (contains(nKey)) {
            QuadPtr quad = m_quads[nKey];
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
    if (contains(key)) return m_quads[key];
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
    newQuad->setForwardStrokes(quad->forwardStrokes());
    newQuad->setBackwardStrokes(quad->backwardStrokes());

    return newQuad;
}

void Lattice::deleteQuad(int key) {
    // update adjacent corners
    if (m_quads.contains(key)) {
        m_quads.value(key)->corners[TOP_LEFT]->quads(BOTTOM_RIGHT) = nullptr;
        m_quads.value(key)->corners[TOP_RIGHT]->quads(BOTTOM_LEFT) = nullptr;
        m_quads.value(key)->corners[BOTTOM_LEFT]->quads(TOP_RIGHT) = nullptr;
        m_quads.value(key)->corners[BOTTOM_RIGHT]->quads(TOP_LEFT) = nullptr;
    }
    m_quads.remove(key);
    deleteUnusedCorners();
}
void Lattice::deleteQuadsPredicate(std::function<bool(QuadPtr)> predicate) {
    QMutableHashIterator<int, QuadPtr> it(m_quads);
    while (it.hasNext()) {
        it.next();
        if (predicate(it.value())) {
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

// Works for any quad (concave or convex)
// If we can guarantee that all quads are convex at all time then we might optimize this function
bool Lattice::quadContainsPoint(QuadPtr quad, const Point::VectorType &p, PosTypeIndex cornerType) const {
    // if (quad == nullptr) return false;
    // Point::VectorType q(-1e7, -1e7);
    // Point::VectorType c[4];
    // c[0] = quad->corners[TOP_RIGHT]->coord(cornerType);
    // c[1] = quad->corners[BOTTOM_RIGHT]->coord(cornerType);
    // c[2] = quad->corners[BOTTOM_LEFT]->coord(cornerType);
    // c[3] = quad->corners[TOP_LEFT]->coord(cornerType);
    // int num = 0;
    // for (int i = 0; i < 4; i++) {
    //     if (Geom::checkSegmentsIntersection(p, q, c[i], c[(i + 1) % 4])) {
    //         if (Geom::wedge((c[i] - p), (q - p)) != 0) num++;
    //     }
    // }
    // return num % 2 == 1;


    if (quad == nullptr) {
        return false;
    }

    Point::VectorType q;
    Point::VectorType c[4];
    c[0] = quad->corners[TOP_RIGHT]->coord(cornerType);
    c[1] = quad->corners[BOTTOM_RIGHT]->coord(cornerType);
    c[2] = quad->corners[BOTTOM_LEFT]->coord(cornerType);
    c[3] = quad->corners[TOP_LEFT]->coord(cornerType);

    // Edges are considered inside
    Point::Scalar prevWedge = 0.0, curWedge;
    for (int i = 0; i < 4; i++) {
        q = (p - c[i]);
        curWedge = Geom::wedge(q, (c[(i + 1) % 4] - c[i]));
        if (prevWedge != 0.0 && curWedge != 0.0 && Utils::sgn(prevWedge) != Utils::sgn(curWedge)) {
            return false;
        }
        prevWedge = curWedge;
    }

    return true;
}

bool Lattice::contains(const Point::VectorType &p, PosTypeIndex cornerType, QuadPtr &quad, int &key) const {
    // TODO bounding box test before
    for (auto it = m_quads.cbegin(); it != m_quads.cend(); ++it) {
        if (quadContainsPoint(it.value(), p, cornerType)) {
            quad = it.value();
            key = it.key();
            return true;
        }
    }
    return false;
}

/**
 * Returns true if all points of the stroke are inside the grid at the given position
 * If checkConnectivity is true, checks that adjacent points in the stroke are inside the same quad or adjacent quads in the grid.
 */
bool Lattice::contains(Stroke *stroke, int from, int to, PosTypeIndex pos, bool checkConnectivity) const {
    QuadPtr q; int k;
    int lastQuadKey = INT_MAX;

    if (checkConnectivity) {
        for (int i = from; i <= to; ++i) {
            std::set<int> quads;
            for (QuadPtr q : m_quads) {
                if (quadContainsPoint(q, stroke->points()[i]->pos(), pos)) {
                    quads.insert(q->key());
                }
            }

            if (quads.size() > 1) return false; // TODO if we keep this we can remove the rest

            // first point
            if (lastQuadKey == INT_MAX) { 
                lastQuadKey = *quads.begin(); // TODO: reset to this point when a branch fails?
                continue; 
            }

            // still in the same quad
            if (quads.find(lastQuadKey) != quads.end()) {
                continue;
            }

            // current quad changed, check adjacency, choose first
            bool foundQk = false;
            for (int qk : quads) {
                if (areQuadsConnected(qk, lastQuadKey)) {
                    lastQuadKey = qk;
                    foundQk = true;
                    break;
                }
            }

            if (!foundQk) {
                qWarning() << "Error: failed connectivity check in Lattice::contains";
                return false;
            }
        }
    } else {
        for (int i = from; i <= to; ++i) {
            if (!contains(stroke->points()[i]->pos(), pos, q, k)) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * Returns true if all points of the stroke are inside the grid at the given position
 * If checkConnectivity is true, checks that adjacent points in the stroke are inside the same quad or adjacent quads in the grid.
 */
bool Lattice::contains(VectorKeyFrame *key, const StrokeIntervals &intervals, PosTypeIndex pos, bool checkConnectivity) const {
    for (auto it = intervals.constBegin(); it != intervals.constEnd(); ++it) {
        for (const Interval &interval : it.value()) {
            if (contains(key->stroke(it.key()), interval.from(), interval.to(), pos, checkConnectivity)) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Returns true if there is at least a point of the stroke inside the grid at the given position 
 */
bool Lattice::intersects(Stroke *stroke, int from, int to, PosTypeIndex pos) const {
    QuadPtr q; int k;
    for (int i = from; i <= to; ++i) {
        if (contains(stroke->points()[i]->pos(), pos, q, k)) {
            return true;
        }
    }
    return false;
}

/**
 * Returns true if there is at least a point of the stroke inside the grid at the given position 
 */
bool Lattice::intersects(VectorKeyFrame *key, const StrokeIntervals &intervals, PosTypeIndex pos) const {
    for (auto it = intervals.constBegin(); it != intervals.constEnd(); ++it) {
        for (const Interval &interval : it.value()) {
            if (intersects(key->stroke(it.key()), interval.from(), interval.to(), pos)) {
                return true;
            }
        }
    }
    return false; 
}

/**
 * Returns true if the stroke interval intersects the grid.
 * All intersected quads are passed in the quads parameter (even overlapping quads)
 */
bool Lattice::intersectedQuads(Stroke *stroke, int from, int to, PosTypeIndex pos, std::set<int> &quads) const {
    quads.clear();
    bool res = false;
    for (int i = from; i <= to; ++i) {
        for (auto it = m_quads.cbegin(); it != m_quads.cend(); ++it) {
            if (quadContainsPoint(it.value(), stroke->points()[i]->pos(), pos)) {
                res = true;
                quads.insert(it.key());
            }
        }
    }
    return res;
}

/**
 * Tag all quads embedding the given stroke. 
 * Returns true if the stroke can be embedded in a set of adjacent quads.
 * Quads are still tagged even if the function return false
 */
bool Lattice::tagValidPath(Stroke *stroke, int from, int to, PosTypeIndex pos, QuadFlags flag) const {
    int lastQuadKey = INT_MAX;
    for (int i = from; i <= to; ++i) {
        std::set<int> quads;
        for (QuadPtr q : m_quads) {
            if (quadContainsPoint(q, stroke->points()[i]->pos(), pos)) {
                quads.insert(q->key());
            }
        }

        // first point
        if (lastQuadKey == INT_MAX) { 
            lastQuadKey = *quads.begin();
            m_quads.value(lastQuadKey)->setFlag(flag, true);
            continue; 
        }

        // still in the same quad
        if (quads.find(lastQuadKey) != quads.end()) {
            continue;
        }

        // current quad changed, check adjacency, choose first
        bool foundQk = false;
        for (int qk : quads) {
            if (areQuadsConnected(qk, lastQuadKey)) {
                m_quads.value(qk)->setFlag(flag, true);
                lastQuadKey = qk;
                foundQk = true;
                break;
            }
        }

        if (!foundQk) {
            qWarning() << "Error: failed connectivity check in Lattice::tagValidPath";
            return false;
        }
    }
    return true;
}

Point::VectorType Lattice::getUV(const Point::VectorType &p, PosTypeIndex type, int &quadKey) {
    QuadPtr quad = nullptr;

    // check if the given point is in the grid, if so, in which quad?
    if (!contains(p, type, quad, quadKey)) {
        int r, c;
        posToCoord(p, r, c);
        qWarning() << "getUV: can't find point quad (" << p.x() << ", " << p.y() << ") cellSize=" << m_cellSize << ", nbRows=" << m_nbRows << ", nbCols=" << m_nbCols << ", r=" << r << ", c=" << c;
        quadKey = INT_MAX;
        return Point::VectorType::Zero();
    }

    return getUV(p, type, quad);
}

Point::VectorType Lattice::getUV(const Point::VectorType &p, PosTypeIndex type, QuadPtr quad) {
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
    if (!m_quads.contains(quadKey)) {
        qWarning() << "getWarpedPoint: can't find quad with key " << quadKey;
        return p;
    }
    QuadPtr quad = m_quads.value(quadKey);
    return (quad->corners[TOP_LEFT]->coord(type) * (1.0 - uv.x()) + quad->corners[TOP_RIGHT]->coord(type) * uv.x()) * (1.0 - uv.y()) 
          + (quad->corners[BOTTOM_LEFT]->coord(type) * (1.0 - uv.x()) + quad->corners[BOTTOM_RIGHT]->coord(type) * uv.x()) * uv.y();
}

bool Lattice::bakeForwardUV(const Stroke *stroke, Interval &interval, UVHash &uvs, PosTypeIndex type) {
    if (stroke == nullptr) {
        qWarning() << "Cannot compute UVs for this interval: invalid stroke: " << stroke;
        return false;
    }

    // Overshoot if possible
    QuadPtr q;
    int k, from = interval.from(), to = interval.to();
    bool isNextPointInLattice = to < stroke->size() - 1 && contains(stroke->points()[interval.to() + 1]->pos(), type, q, k);
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
        uv.uv = getUV(pos, type, key);
        uv.quadKey = key;
        uvs.add(stroke->id(), i, uv);
    }

    return true;
}

bool Lattice::bakeForwardUVConnectivityCheck(const Stroke *stroke, Interval &interval, UVHash &uvs, PosTypeIndex type) {
    if (stroke == nullptr) {
        qWarning() << "Cannot compute UVs for this interval: invalid stroke: " << stroke;
        return false;
    }

    // Overshoot if possible
    QuadPtr q;
    int k, from = interval.from(), to = interval.to();
    bool isNextPointInLattice = to < stroke->size() - 1 && contains(stroke->points()[interval.to() + 1]->pos(), type, q, k);
    if (isNextPointInLattice) {
        to += 1;
    } else if (to < stroke->size() - 1) {
        interval.setOvershoot(false);
    }

    int key, prevKey = INT_MAX;
    for (size_t i = from; i <= to; ++i) {
        std::set<int> quads;
        for (QuadPtr q : m_quads) {
            if (quadContainsPoint(q, stroke->points()[i]->pos(), type)) {
                quads.insert(q->key());
            }
        }

        Q_ASSERT_X(!quads.empty(), "bakeForwardUVConnectivityCheck", "cannot find an intersecting quad");

        
        if (prevKey == INT_MAX) {                           // First point
            key = *quads.begin();
        } else if (quads.find(prevKey) != quads.end()) {    // Same quad
            key = prevKey;
        } else {                                            // Find adjacent quad
            bool foundQk = false;
            for (int qk : quads) {
                if (areQuadsConnected(qk, prevKey)) {
                    foundQk = true;
                    key = qk;
                    break;
                }
            }
            Q_ASSERT_X(foundQk, "bakeForwardUVConnectivityCheck", "cannot find an intersecting quad adjacent to the previous one");
        }


        stroke->points()[i]->initId(stroke->id(), i);
        UVInfo uv;
        if (uvs.has(stroke->id(), i)) uv = uvs.get(stroke->id(), i);
        uv.uv = getUV(stroke->points()[i]->pos(), type, m_quads.value(key));
        uv.quadKey = key;
        uvs.add(stroke->id(), i, uv);
        prevKey = key;
    }

    return true;
}

bool Lattice::bakeForwardUVPrecomputed(const Stroke *stroke, Interval &interval, UVHash &uvs) {
    if (stroke == nullptr) {
        qWarning() << "Cannot compute UVs for this interval: invalid stroke: " << stroke;
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
        if (!uvs.has(stroke->id(), i) || !quadContainsPoint(m_quads.value(uvs.get(stroke->id(), i).quadKey), pos, REF_POS)) {
            qCritical() << "Error in bakeForwardUVPrecomputed: " << uvs.has(stroke->id(), i);
            int qq; QuadPtr qkqk;
            getUV(pos, REF_POS, qq);
            std::vector<int> goodQuads;
            for (QuadPtr q : m_quads) if (quadContainsPoint(q, pos, REF_POS)) goodQuads.push_back(q->key());
        } 
        UVInfo uv;
        uv = uvs.get(stroke->id(), i);
        uv.uv = getUV(pos, REF_POS, m_quads.value(uv.quadKey));
        uvs.add(stroke->id(), i, uv);
    }

    return true;
}

bool Lattice::bakeBackwardUV(const Stroke *stroke, Interval &interval, const Point::Affine &transform, UVHash &uvs) {
    if (stroke == nullptr) {
        qWarning() << "Cannot compute UVs for this interval: invalid stroke: " << stroke;
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
    for (QuadPtr quad : m_quads) {
        if (quad->isPinned()) {
            pinsPositions.push_back(quad->pinPos() - quad->getPoint(quad->pinUV(), REF_POS));
        }
    }
    return pinsPositions;
}

/**
 * Displace all pinned quads to their target position.
 * Change the given group's lattice dstPos, but due to the forced displacement, adjacent non-pinned quads may become degenerated. 
 * Therefore it should be followed by ARAP regularization.
 */
void Lattice::displacePinsQuads(PosTypeIndex dstPos) {
    for (QuadPtr q : m_quads) {
        if (!q->isPinned()) continue;
        Point::VectorType displacement = q->pinPos() - q->getPoint(q->pinUV(), dstPos); // Displacement from TARGET_POS to keep its orientation
        for (unsigned int i = 0; i < 4; ++i) {
            q->corners[i]->coord(dstPos) += displacement;
        }
    }
    setArapDirty();
}

void Lattice::drawLattice(QPainter &painter, qreal interpFactor, const QColor &color, PosTypeIndex type) const {
    QPen gridPen(QBrush(color, Qt::SolidPattern), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    gridPen.setColor(color);
    painter.setPen(gridPen);
    painter.setOpacity(1.0);

    const Point::Affine A = m_keyframe->rigidTransform(type == REF_POS ? 0 : 1);

    for (auto it = m_quads.constBegin(); it != m_quads.constEnd(); ++it) {
        QuadPtr quad = it.value();
        if (quad == nullptr) {
            qWarning("quad error");
            continue;
        }

        quad->computeCentroid(INTERP_POS);
        for (int i = 0; i < 4; i++) {
            Corner *c0 = quad->corners[i];
            Corner *c1 = quad->corners[(i + 1) % 4];
            if (c0 == nullptr) qWarning("Corner error");
            Point::VectorType p0 = A * c0->coord(type);
            Point::VectorType p1 = A * c1->coord(type);
            painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
            if ((quad->nbBackwardStrokes() == 0 && quad->nbForwardStrokes() == 0) && quad->isEmpty() && k_drawDebugLattice) {
                gridPen.setWidthF(0.5f);
                painter.setPen(gridPen);
                quad->computeCentroid(REF_POS);
                Point::VectorType centroid = quad->centroid(REF_POS);
                painter.drawEllipse(QPointF(centroid.x(), centroid.y()), 2, 2);
            }
            if (quad->isPivot() && k_drawDebugLattice) {
                gridPen.setWidthF(0.5f);
                gridPen.setColor(Qt::magenta);
                painter.setPen(gridPen);
                quad->computeCentroid(REF_POS);
                Point::VectorType centroid = quad->centroid(REF_POS);
                painter.drawEllipse(QPointF(centroid.x(), centroid.y()), 1, 1);
                gridPen.setColor(color);
                painter.setPen(gridPen);
            }
        }
    }
}

void Lattice::drawLattice(QPainter &painter, const QColor &color, VectorKeyFrame *keyframe, int groupID, int inbetween) const {
    QPen gridPen(QBrush(color, Qt::SolidPattern), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    gridPen.setColor(color);
    painter.setPen(gridPen);
    painter.setOpacity(1.0);

    const int stride = keyframe->inbetweens().size();
    float t = stride > 1 ?  float(inbetween) / (stride - 1) : 0.0f;
    const Point::Affine A = keyframe->rigidTransform(t);

    const std::vector<Point::VectorType> &corners = keyframe->inbetweenCorners(inbetween).value(groupID);

    for (auto it = m_quads.constBegin(); it != m_quads.constEnd(); ++it) {
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
            Point::VectorType p0 = A * corners[c0->getKey()];
            Point::VectorType p1 = A * corners[c1->getKey()];
            painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
        }
        // quad->computeCentroid(INTERP_POS);
        // painter.drawText(EQ_POINT(quad->centroid(INTERP_POS)), QString("%1 | %2").arg(quad->forwardStrokes().size(), quad->backwardStrokes().size()));
    }
}

void Lattice::drawPins(QPainter &painter) {
    static QPen p(QBrush(Qt::darkRed, Qt::SolidPattern), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(p);
    for (QuadPtr quad : m_quads) {
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


void Lattice::drawCornersTrajectories(QPainter &painter, const QColor &color, Group *group, VectorKeyFrame *key, bool linearInterpolation) {
    painter.save();
    QPen gridPen(Qt::NoBrush, 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(gridPen);
    if (linearInterpolation) {
        int i = 0;
        for (Corner *c : m_corners) {
            gridPen.setColor(QColor(40 + 180 * i / float(m_corners.size()), 20, 180 - 120 * i / float(m_corners.size())));
            painter.setPen(gridPen);
            Point::VectorType p0 = c->coord(REF_POS);
            Point::VectorType p1 = c->coord(TARGET_POS);
            painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
            i++;
        }
    } else {
        precompute();
        float t;
        Point::VectorType prev;
        int idx = 0;
        for (Corner *c : m_corners) {
            prev = c->coord(REF_POS);
            gridPen.setColor(QColor(40 + 180 * idx / float(m_corners.size()), 20, 180 - 120 * idx / float(m_corners.size())));
            painter.setPen(gridPen);
            for (int i = 1; i < 40; ++i) {
                t = (float)i / (float)40;
                interpolateARAP(t, group->spacingAlpha(t), group->globalRigidTransform(t));
                painter.drawLine(QPointF(prev.x(), prev.y()), QPointF(c->coord(INTERP_POS).x(), c->coord(INTERP_POS).y()));
                prev = c->coord(INTERP_POS);
            }
            idx++;
        }
        m_arapDirty = true;
        m_precomputeDirty = true;
    }
    painter.restore();
}

void Lattice::createBuffer(QOpenGLShaderProgram *program, QOpenGLExtraFunctions *functions) {
    if (m_bufferCreated) return;

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_ebo.create();
    m_ebo.bind();
    m_ebo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // vtx pos
    int vLoc = program->attributeLocation("vertex");
    program->enableAttributeArray(vLoc); 
    program->setAttributeBuffer(vLoc, GL_FLOAT, 0, 2, sizeof(LatticeVtx));

    // vtx flags
    int fLoc = program->attributeLocation("iFlags");
    program->enableAttributeArray(fLoc); 
    // fuck qt 
    // program->setAttributeBuffer(fLoc, GL_UNSIGNED_INT, offsetof(LatticeVtx, flags), 1, sizeof(LatticeVtx));
    functions->glVertexAttribIPointer(fLoc, 1, GL_UNSIGNED_BYTE, sizeof(LatticeVtx), (void *)offsetof(LatticeVtx, flags));

    m_vao.release();
    m_vbo.release();
    m_ebo.release();

    updateBuffer();

    m_bufferCreated = true;
    m_bufferDestroyed = false; 
}

void Lattice::destroyBuffer() {
    if (!m_bufferCreated) return;
    StopWatch s("Destroying lattice buffers");
    m_ebo.destroy();
    m_vbo.destroy();
    m_vao.destroy();
    m_bufferDestroyed = true;
    m_bufferCreated = false;
    s.stop();
}

void Lattice::updateBuffer() {
    markOutline();

    std::vector<LatticeVtx> vertices(m_corners.size());
    std::vector<unsigned int> indices(m_quads.size() * 4);

    for (int i = 0; i < m_corners.size(); ++i) {
        vertices[i].x = m_corners[i]->coord(TARGET_POS).x();
        vertices[i].y = m_corners[i]->coord(TARGET_POS).y();
        vertices[i].flags = (GLubyte)(m_corners[i]->flags().to_ulong());
    }

    int i = 0;
    for (QuadPtr quad : m_quads) {
        indices[4 * i] = quad->corners[0]->getKey();
        indices[4 * i + 1] = quad->corners[1]->getKey();
        indices[4 * i + 2] = quad->corners[3]->getKey();
        indices[4 * i + 3] = quad->corners[2]->getKey();
        ++i;
    }
    
    m_vbo.bind(); 
    m_vbo.allocate(vertices.data(), vertices.size() * sizeof(LatticeVtx));
    m_vbo.release();

    m_ebo.bind(); 
    m_ebo.allocate(indices.data(), indices.size() * sizeof(GLuint));
    m_ebo.release();    
}

// Precompute the sparse matrix P and store it as P^T and P^T*P for later computations
void Lattice::precompute() {
    if (!m_singleConnectedComponent) {
        qWarning() << "Cannot precompute a lattice with multiple connected components!";
        return;
    }

    qDebug() << "PRECOMPUTING GRID (Q: " << m_quads.size() << ", C: " << m_corners.size() << ")";
    StopWatch sw("Precompute ARAP LHS");
    std::vector<TripletD> P_triplets;
    int nQuads = m_quads.size();
    int nCorners = m_corners.size();
    int P_rows = 8 * nQuads;
    int triRow = 0;
    QuadPtr q = *m_quads.constBegin();
    double size = (q->corners[TOP_RIGHT]->coord(REF_POS) - q->corners[TOP_LEFT]->coord(REF_POS)).norm();
    double triArea = size * size / 2.0;

    // Compute P (sparse) and store its transpose to construct the RHS of the equation later
    // TODO refactorize concatenation
    for (auto it = m_quads.constBegin(); it != m_quads.constEnd(); ++it) {
        q = it.value();
        computePStar(q, TOP_LEFT, TOP_RIGHT, triRow, false, P_triplets);
        triRow++;
        computePStar(q, TOP_RIGHT, BOTTOM_RIGHT, triRow, false, P_triplets);
        triRow++;
    }
    for (auto it = m_quads.constBegin(); it != m_quads.constEnd(); ++it) {
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
        QuadPtr quad = m_quads[latticeCoord.quadKey];
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

void Lattice::interpolateARAP(qreal alphaLinear, qreal alpha, const Point::Affine &globalRigidTransform, bool useRigidTransform) {
    qDebug() << "** INTERP " << alpha;
    StopWatch sw("ARAP interpolation");

    useRigidTransform = useRigidTransform && k_useGlobalRigidTransform;
    m_currentPrecomputedTime = alpha;

    // Lattices with multiple connected components cannot be interpolated, return reference or target configuration
    if (!m_singleConnectedComponent) {
        if (alpha < 1.0) copyPositions(this, REF_POS, INTERP_POS);
        else             copyPositions(this, TARGET_POS, INTERP_POS);
        return;
    }

    int nQuads = m_quads.size();
    MatrixXd A(2, 8 * nQuads);
    qreal t = alpha;

    // Compute A(t)
    QuadPtr q;
    int i = 0;
    for (auto it = m_quads.begin(); it != m_quads.end(); ++it) {
        q = it.value();
        computeQuadA(q, A, i, t, false);
    }
    for (auto it = m_quads.begin(); it != m_quads.end(); ++it) {
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
    for (auto it = m_corners.begin(); it != m_corners.end(); ++it) {
        Corner *c;
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
 * Check on which side the quad should be added (if q1 and q2 share a stroke)
 */
bool Lattice::checkQuadsShareStroke(VectorKeyFrame *keyframe, QuadPtr q1, QuadPtr q2, std::vector<QuadPtr> &newQuads) {
    bool sharedStroke = false;
    for (auto strokeIntervals = q1->forwardStrokes().constBegin(); strokeIntervals != q1->forwardStrokes().constEnd(); ++strokeIntervals) {
        if (q2->forwardStrokes().contains(strokeIntervals.key())) {
            const Intervals intervalsQ1 = q1->forwardStrokes().value(strokeIntervals.key());
            const Intervals intervalsQ2 = q2->forwardStrokes().value(strokeIntervals.key());
            for (const Interval &intervalQ1 : intervalsQ1) {
                for (const Interval &intervalQ2 : intervalsQ2) {
                    if (intervalQ1.connected(intervalQ2)) {
                        Interval interval(std::min(intervalQ1.from(), intervalQ2.from()), std::max(intervalQ1.to(), intervalQ2.to()));
                        enforceManifoldness(keyframe->stroke(strokeIntervals.key()), interval, newQuads);
                        sharedStroke = true;
                    }
                }
            }
        }
        if (sharedStroke) break;
    }
    return sharedStroke;
}

/**
 * Compute P* for the two triangles of the given quad and add them to the sparse matrix P (via the triplet list).
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

// Compute A(t) (concatenated for the two triangles of the given quad)
void Lattice::computeQuadA(QuadPtr q, MatrixXd &At, int &i, float t, bool inverseOrientation) {
    Matrix2d A_interp, I;
    Matrix2d A, Rt, S;
    I = Matrix2d::Identity();
    if (inverseOrientation) t = 1.0f - t;

    // Compute and concatenate the interpolated linear transformation of the triangle formed by CornerA, CornerB and the bottom left corner
    // of the quad
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
        if (corner->flag(MOVABLE)) {
            corner->coord(dst) = transform * corner->coord(ref);
        }
    }
}

void Lattice::copyPositions(const Lattice *dst, PosTypeIndex srcPos, PosTypeIndex dstPos) {
    for (auto it = dst->m_quads.constBegin(); it != dst->m_quads.constEnd(); ++it) {
        QuadPtr quad = m_quads[it.key()];
        for (int i = 0; i < 4; ++i) {
            quad->corners[i]->coord(dstPos) = it.value()->corners[i]->coord(srcPos);
        }
    }
}

// assume copied lattice (same topology and quad keys)
// set this lattice's srcPos corners position to the given target lattice targetPos corners position
void Lattice::moveSrcPosTo(const Lattice *target, PosTypeIndex srcPos, PosTypeIndex targetPos) {
    std::set<int> visitedCorners;
    Point::VectorType posTarget, offset;
    for (auto it = target->m_quads.constBegin(); it != target->m_quads.constEnd(); ++it) {
        QuadPtr quadTarget = it.value();
        QuadPtr quad = m_quads[it.key()];
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
 * Remove any deformation applied to the grid
 */
void Lattice::resetDeformation() {
    for (int i = 0; i < m_corners.size(); ++i) {
        m_corners[i]->coord(INTERP_POS) = m_corners[i]->coord(REF_POS);
        m_corners[i]->coord(TARGET_POS) = m_corners[i]->coord(REF_POS);
        m_corners[i]->coord(DEFORM_POS) = m_corners[i]->coord(REF_POS);
    }
    m_scaling = Point::Affine::Identity();
    m_scale = 1.0;
    m_arapDirty = true;
    m_precomputeDirty = true;
}

/**
 * Return the projection of point p onto the closest edge of the grid.
 * @param p the point to project
 * @param outQuad quad key adjacent to the projected point
 */
Point::VectorType Lattice::projectOnEdge(const Point::VectorType &p, int &outQuad) const {
    Point::VectorType closestProj = Point::VectorType(99999, 99999);
    Point::VectorType proj;
    for (QuadPtr quad : m_quads) { // yeah it's not efficient
        for (int i = 0; i < 4; ++i) {
            proj = Geom::projectPointToSegment(quad->corners[i]->coord(REF_POS), quad->corners[(i + 1) % 4]->coord(REF_POS), p);
            if ((p - proj).squaredNorm() < (p - closestProj).squaredNorm()) {
                closestProj = proj;
                outQuad = quad->key();
            }            
        }
    }
    return closestProj;
}

/**
 * Return true if the two given quads are adjacent (8-neighborhood) 
 */
bool Lattice::areQuadsConnected(int quadKeyA, int quadKeyB) const {
    int xA, yA, xB, yB;
    keyToCoord(quadKeyA, xA, yA);
    keyToCoord(quadKeyB, xB, yB);
    return std::abs(xA - xB) <= 1 && std::abs(yA - yB) <= 1;
}

/**
 * Return a list of the adjacent quad keys of the corner. It always return all 4 keys even if the quad does not exist!
 */
void Lattice::adjacentQuads(Corner *corner, std::array<int, 4> &neighborKeys) {
    int firstAdjacentQuadPos = -1;
    std::array<int, 4> offsets = {+1, m_nbCols, -1, -m_nbCols}; // TL->TR, TR->BR, BR->BL, BL->TL
    
    for (int i = 0; i < NUM_CORNERS; ++i) {
        if (corner->quads((CornerIndex)i) != nullptr) {
            firstAdjacentQuadPos = i;
            neighborKeys[i] = corner->quads((CornerIndex)i)->key();
            break;
        }
    }

    if (firstAdjacentQuadPos == -1) {
        qCritical() << "Error in Lattice::adjacentQuads: corner does not have any adjacent quad!";
    }
    
    int prevPos = firstAdjacentQuadPos;
    int prevKey = neighborKeys[firstAdjacentQuadPos];
    for (int i = 1; i < NUM_CORNERS; ++i) {
        int curPos = Utils::pmod(firstAdjacentQuadPos + i, (int)NUM_CORNERS);
        neighborKeys[curPos] = prevKey + offsets[prevPos];
        prevPos = curPos;
        prevKey = neighborKeys[curPos];
    }
}

/**
 * Return a list of the adjacent quad keys of the corner. It always return all 8 keys even if the quad does not exist!
 */
void Lattice::adjacentQuads(QuadPtr quad, std::array<int, 8> &neighborKeys) {
    int x, y;
    keyToCoord(quad->key(), x, y);
    static int dx[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
    static int dy[8] = {-1, -1, -1, 0, 1, 1, 1, 0};
    for (int i = 0; i < 8; ++i) {
        neighborKeys[i] = coordToKey(x + dx[i], y + dy[i]);
    }
}

/**
 * Return a pointer to the adjacent quad that share the given edge.
 * Return nullptr if no such quad exists.
 */
QuadPtr Lattice::adjacentQuad(QuadPtr quad, EdgeIndex edge) {
    if (quad == nullptr) return nullptr;
    static int dx[NUM_EDGES] = {0, 1, 0, -1};
    static int dy[NUM_EDGES] = {1, 0, -1, 0};
    int x, y, k;
    keyToCoord(quad->key(), x, y);
    k = coordToKey(x + dx[(int)edge], y + dy[(int)edge]);    
    return m_quads.value(k, nullptr);
}

/**
 * Check if the lattice is a single connected component (DFS), save the result in the flag m_singleConnectedComponent
 * TODO: dirty system
 */
bool Lattice::isConnected() {
    if (m_quads.empty()) {
        m_singleConnectedComponent = false;
        return false;
    }

    for (QuadPtr &quad : m_quads) {
        quad->setMiscFlag(false);
    }

    QStack<int> toVisit;
    toVisit.push((*m_quads.begin())->key());

    // Add the quad to the stack if its coordinates are valid and if its visited flag is false
    auto pushQuad = [&](int x, int y, int key) {
        if (x >= 0 && x < nbCols() && y >= 0 && y < nbRows() && m_quads.contains(key) && !m_quads.value(key)->miscFlag()) {
            toVisit.push(m_quads.value(key)->key());
        }
    };

    int x, y;
    while (!toVisit.isEmpty()) {
        const QuadPtr &curQuad = m_quads.value(toVisit.pop());
        if (curQuad->miscFlag()) continue;
        curQuad->setMiscFlag(true);
        keyToCoord(curQuad->key(), x, y);
        for (int i = x - 1; i <= x + 1; ++i) {
            for (int j = y - 1; j <= y + 1; ++j) {
                if (i == x && j == y) continue;
                pushQuad(i, j, coordToKey(i, j));
            }
        }
    }

    for (QuadPtr &quad : m_quads) {
        if (!quad->miscFlag()) {
            m_singleConnectedComponent = false;
            return false;
        }
    }

    m_singleConnectedComponent = true;
    return true;
}

// TODO output list of lattices?
void Lattice::getConnectedComponents(std::vector<std::vector<int>> &outputComponents, bool overrideFlag) {
    outputComponents.clear();

    if (overrideFlag) {
        for (QuadPtr &quad : m_quads) {
            quad->setMiscFlag(false);
        }
    }

    QStack<int> toVisit;

    // Add the quad to the stack if its coordinate are valid and if its flag is false
    auto pushQuad = [&](int x, int y, int key) {
        if (x >= 0 && x < nbCols() && y >= 0 && y < nbRows() && m_quads.contains(key) && !m_quads.value(key)->miscFlag()) {
            toVisit.push(m_quads.value(key)->key());
        }
    };

    while (true) {
        toVisit.clear();
        std::vector<int> connectedComponent;

        // Are there any non visited quad remaining?
        Quad *start = nullptr;
        for (QuadPtr &quad : m_quads) {
            if (!quad->miscFlag()) {
                start = quad.get();
            }
        }

        if (start == nullptr)
            break;
        
        // Go through the connected component starting from the start quad
        int x, y;
        toVisit.push(start->key());
        while (!toVisit.isEmpty()) {
            const QuadPtr &curQuad = m_quads.value(toVisit.pop());
            if (curQuad->miscFlag()) continue;
            curQuad->setMiscFlag(true);
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


/**
 * Returns a corner on the outline of the grid
 * TODO: precompute when constructing the grid/editing it
 */
Corner *Lattice::findBoundaryCorner(PosTypeIndex coordType) {
    double minX = std::numeric_limits<double>::max();
    Corner *firstCorner = nullptr;

    // Find the corner with minimum x-component, no assumption about the geometry of the source grid
    for (Corner *corner : m_corners) {
        corner->setMiscFlag(false);
        corner->setFlag(BOUNDARY, false);
        if (corner->coord(coordType).x() < minX) {
            minX = corner->coord(coordType).x();
            firstCorner = corner;        
        }
    }

    return firstCorner;
}

/**
 * Given a corner on the exterior boundary of the grid, returns a non-visited adjacent corner on the exterior boundary.
 * If no such corner exists, returns nullptr
 */
Corner *Lattice::findNextBoundaryCorner(Corner *corner) {
    for (int i = 0; i < NUM_CORNERS; ++i) {
        QuadPtr quad = corner->quads((CornerIndex)i);
        if (quad == nullptr) continue;
        // Find the corner position in the quad
        int cornerPosition = NUM_CORNERS;
        for (int j = 0; j < NUM_CORNERS; ++j) {
            if (quad->corners[j] == corner) {
                cornerPosition = j;                    
                break;
            }
        }
        // Check neighbors
        Corner *right = quad->corners[((cornerPosition+1) % NUM_CORNERS)]; 
        if (!right->miscFlag() && right->nbQuads() < 4 && quadsInCommon(corner, right) == 1) {
            return right;
        }
        Corner *left = quad->corners[((cornerPosition-1+NUM_CORNERS) % NUM_CORNERS)]; 
        if (!left->miscFlag() && left->nbQuads() < 4 && quadsInCommon(corner, left) == 1) {
            return left;
        }
    }
    return (Corner *)nullptr;
}

int Lattice::quadsInCommon(Corner *c1, Corner *c2) {
    /**
     * We need this check because an edge between 2 boundary vertices can still be an interior edge
     * The edge (c1,c2) is "interior" if they c1 and c2 share 2 or more adjacent quads
     */
    int count = 0;
    for (int i = 0; i < NUM_CORNERS; ++i) {
        QuadPtr q1 = c1->quads((CornerIndex)i);
        if (q1 == nullptr) continue;
        for (int j = 0; j < NUM_CORNERS; ++j) {
            QuadPtr q2 = c2->quads((CornerIndex)j);
            if (q2 != nullptr && q1->key() == q2->key()) count += 1;
        }
    }
    return count;
}

/**
 * Set the BOUNDARY flag in all boundary corners of the grid to true
 */
void Lattice::markOutline() {
    Corner *corner = findBoundaryCorner(REF_POS);
    do {
        corner->setFlag(BOUNDARY, true);
        corner->setMiscFlag(true);
        corner = findNextBoundaryCorner(corner);
    } while (corner != nullptr);
}

/**
 * Check if a polyline segment crosses over a cell without having a vertex in it. If this is the case, set this quad key in quadKeyOut.
 * Returns true if the segment crosses over a cell, false otherwise.
 */
bool Lattice::checkPotentialBowtie(Point::VectorType &prevPoint, Point::VectorType &curPoint, int &quadKeyOut) {
    QuadPtr q;
    int keyPrev, keyCur; 
    contains(prevPoint, REF_POS, q, keyPrev);
    contains(curPoint, REF_POS, q, keyCur);

    // If the two successive keys are adjacent then the grid doesn't need refinement
    if (std::abs(keyPrev - keyCur) != nbCols() + 1 && std::abs(keyPrev - keyCur) != nbCols() - 1) {
        return false;
    }

    // In this case the grid needs refinement, we need to determine where the new cell must be added. To do this, we first identify the shared corner
    int sharedCornerKey;
    QuadPtr prevQuad = m_quads[keyPrev];
    int keyOptionsPositive, keyOptionsNegative;
    if (keyCur == keyPrev - nbCols() - 1) {
        sharedCornerKey = prevQuad->corners[TOP_LEFT]->getKey();
        keyOptionsPositive = keyPrev - 1;
        keyOptionsNegative = keyPrev - nbCols();
    } else if (keyCur == keyPrev - nbCols() + 1) {
        sharedCornerKey = prevQuad->corners[TOP_RIGHT]->getKey();
        keyOptionsPositive = keyPrev - nbCols();
        keyOptionsNegative = keyPrev + 1;
    } else if (keyCur == keyPrev + nbCols() - 1) {
        sharedCornerKey = prevQuad->corners[BOTTOM_LEFT]->getKey();
        keyOptionsPositive = keyPrev + nbCols();
        keyOptionsNegative = keyPrev - 1;
    } else if (keyCur == keyPrev + nbCols() + 1) {
        sharedCornerKey = prevQuad->corners[BOTTOM_RIGHT]->getKey();
        keyOptionsPositive = keyPrev + 1;
        keyOptionsNegative = keyPrev + nbCols();
    }

    Point::VectorType sharedCornerPos = m_corners[sharedCornerKey]->coord(REF_POS);
    Point::VectorType prevToSharedCorner = sharedCornerPos - prevPoint;
    Point::VectorType segment = curPoint - prevPoint;
    double wedge = prevToSharedCorner.x() * segment.y() - prevToSharedCorner.y() * segment.x();
    quadKeyOut = wedge < 0 ? keyOptionsPositive : keyOptionsNegative;
    return true;
}

/**
 * Add empty quads when necessary to keep the lattice manifold
 * TODO: propagate TARGET_POS coords to the new quads 
 */
void Lattice::enforceManifoldness(Group *group) {
    std::vector<QuadPtr> newQuads;
    VectorKeyFrame *keyframe = group->getParentKeyframe();
    int nbNewQuads = 0;

    // Check for bowtie corners
    QVector<Corner *> corners = m_corners;
    for (Corner *corner : corners) {
        if (corner->nbQuads() != 2) continue;
        for (int i = 0; i < NUM_CORNERS; ++i) {
            if (corner->quads((CornerIndex)i) != nullptr && corner->quads((CornerIndex)(Utils::pmod(i-1, (int)NUM_COORDS))) == nullptr && corner->quads((CornerIndex)((i + 1) % NUM_CORNERS)) == nullptr) {
                // There is a bowtie at this corner
                QuadPtr q1 = corner->quads((CornerIndex)i); 
                QuadPtr q2 = corner->quads((CornerIndex)((i + 2) % NUM_COORDS)); 

                // Check on which side the quad should be added (if q1 and q2 share a stroke)
                bool sharedStroke = checkQuadsShareStroke(keyframe, q1, q2, newQuads);

                // Otherwise just use an arbitrary side
                if (!sharedStroke) {
                    std::array<int, 4> adjQuads;
                    adjacentQuads(corner, adjQuads);
                    for (int i = 0; i < 4; ++i) {
                        if (!contains(adjQuads[i])) {
                            bool isNewQuad = false;
                            int x, y;
                            keyToCoord(adjQuads[i], x, y);
                            QuadPtr newQuad = addQuad(adjQuads[i], x, y, isNewQuad);
                            newQuad->setPivot(true);
                            newQuads.push_back(newQuad);
                            break;
                        }
                    }
                }

                if (newQuads.size() == nbNewQuads) continue;
                nbNewQuads = newQuads.size();

                // Propagate deformation to the new quad. Since we only deal with the bowtie case, new quads have 3 corners that already have a defined deformation.
                QuadPtr newQuad = newQuads.back();
                int index, newCornerIndex;
                for (index = 0; index < NUM_CORNERS; ++index) {
                    if (newQuad->corners[index]->getKey() == corner->getKey()) break;
                }
                newCornerIndex = (index + 2) % NUM_CORNERS;
                newQuad->corners[newCornerIndex]->coord(REF_POS) = newQuad->corners[(index + 1) % NUM_CORNERS]->coord(REF_POS) + newQuad->corners[Utils::pmod(index - 1, (int)NUM_CORNERS)]->coord(REF_POS) - newQuad->corners[index]->coord(REF_POS);
                newQuad->corners[newCornerIndex]->coord(TARGET_POS) = newQuad->corners[(index + 1) % NUM_CORNERS]->coord(TARGET_POS) + newQuad->corners[Utils::pmod(index - 1, (int)NUM_CORNERS)]->coord(TARGET_POS) - newQuad->corners[index]->coord(TARGET_POS);
                newQuad->corners[newCornerIndex]->coord(INTERP_POS) = newQuad->corners[newCornerIndex]->coord(TARGET_POS);
                newQuad->corners[newCornerIndex]->coord(DEFORM_POS) = newQuad->corners[newCornerIndex]->coord(TARGET_POS);
                break;
            }
        }
    }
}

void Lattice::enforceManifoldness(Stroke *stroke, Interval &interval, std::vector<QuadPtr> &newQuads, bool forceAddPivots) {
    Point::VectorType pos, prevPos = stroke->points()[interval.from()]->pos();
    bool newQuad = false;
    QuadPtr quad;
    int quadKey;

    // Go through each point in the stroke between fromIdx and toIdx, if a point is not in a quad try to add it
    int nbCols = m_nbCols;
    for (size_t i = interval.from() + 1; i <= interval.to(); ++i) {
        pos = stroke->points()[i]->pos();
        if (checkPotentialBowtie(prevPos, pos, quadKey)) {
            int x, y;
            keyToCoord(quadKey, x, y);
            quad = addQuad(quadKey, x, y, newQuad);
            if (newQuad || forceAddPivots) {
                newQuads.push_back(quad);
                quad->setPivot(true);
            }
            newQuad = false;
        }
        prevPos = pos;
    }
}

void Lattice::debug(std::ostream &os) const {
    for (Corner *corner : m_corners) {
        os << "REF(" << corner->coord(REF_POS).transpose() << ")  |   DEFORM(" << corner->coord(DEFORM_POS).transpose() << ")  | INTERP" << corner->coord(INTERP_POS).transpose()
           << ")  | TARGET(" << corner->coord(TARGET_POS).transpose() << ")" << std::endl;
    }
    os << "#corners=" << m_corners.size() << std::endl;
    os << "#quads=" << m_quads.size() << std::endl;
}

/**
 * Used for retrocompatibility with old files.
 * Restore the correct quad keys.
 */
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
    QHash<int, QuadPtr> oldHash = m_quads;
    m_quads.clear();
    for (QuadPtr q : oldHash) {
        Point::VectorType refCentroid = Point::VectorType::Zero();
        for (int i = 0; i < 4; ++i) refCentroid += q->corners[i]->coord(REF_POS);
        refCentroid *= 0.25;
        int oldKey = q->key();
        int newKey = posToKey(refCentroid);
        keysMap.insert(oldKey, newKey);
        q->setKey(newKey);
        m_quads.insert(newKey, q);
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

        oldHash = curGroup->lattice()->m_quads;
        curGroup->lattice()->m_quads.clear();
        for (QuadPtr q : curGroup->lattice()->quads()) {
            q->setKey(keysMap.value(q->key()));
            curGroup->lattice()->m_quads.insert(keysMap.value(q->key()), q);
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