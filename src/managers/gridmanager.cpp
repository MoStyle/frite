/*
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include <limits>
#include <iostream>
#include <QColor>

#include "arap.h"
#include "corner.h"
#include "gridmanager.h"
#include "lattice.h"
#include "layer.h"
#include "quad.h"
#include "editor.h"
#include "viewmanager.h"
#include "layermanager.h"
#include "dialsandknobs.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "utils/stopwatch.h"

#include <QSet>

extern dkSlider k_deformRange;
static dkBool k_arap("Warp->ARAP", true);
static dkSlider k_iterationGrid("Warp->Rigidity (#regularization)", 20, 1, 450, 1);
dkInt k_cellSize("Options->Grid->Cell Size", 16, 1, 64, 1);
dkBool k_useDeformAsSource("Warp->Plastic deformation", false);

GridManager::GridManager(QObject *pParent) : BaseManager(pParent) {
    m_deformRange = k_deformRange;
    m_deformed = false;
    m_lastPos = Point::VectorType::Zero();
    connect(&k_deformRange, SIGNAL(valueChanged(int)), this, SLOT(setDeformRange(int)));
}

void GridManager::setDeformRange(int k) {
    m_deformRange = k;
    m_deformed = false;
    k_deformRange.setValue(k);
}

// Add the given stroke segment to the group's lattice
// Add new quads if necessary, bake the stroke forward UVs update each intersected quad elements list
bool GridManager::addStrokeToGrid(Group *group, Stroke *stroke, Interval &interval) {
    Lattice *grid = group->lattice();
    std::vector<QuadPtr> newQuads;
    Point *p;
    Point::VectorType pos, prevPos;
    bool newQuad = false;
    QuadPtr q;

    // go through each point in the stroke between fromIdx and toIdx, if a point is not in a quad try to add it
    int nbCols = grid->nbCols();
    for (size_t i = interval.from(); i <= interval.to(); ++i) {
        p = stroke->points()[i];
        pos = p->pos();
        newQuad = false;
        q = grid->addQuad(pos, newQuad);
        if (newQuad) newQuads.push_back(q);
        newQuad = false;

        // check if grid needs a "refining" quad to avoid single pivot point
        if (i > interval.from()) {
            int quadKey;
            if (needRefinement(grid, prevPos, pos, quadKey)) {
                int x, y;
                grid->keyToCoord(quadKey, x, y);
                q = grid->addQuad(quadKey, x, y, newQuad);
                if (newQuad) newQuads.push_back(q);
                newQuad = false;
            }
        }

        prevPos = pos;
    }

    if (!newQuads.empty()) {
        group->lattice()->isConnected();
        if (newQuads.size() != grid->hash().size()) {
            propagateDeformToNewQuads(grid, newQuads);
        }
    }

    bakeStrokeInGrid(grid, stroke, interval.from(), interval.to());
    grid->bakeForwardUV(stroke, interval, group->uvs());

    return !newQuads.empty();
}

// Fill the given group's lattice with the strokes in the group
bool GridManager::constructGrid(Group *group, ViewManager *view) {
    Lattice *grid = group->lattice();
    VectorKeyFrame *parentKey = group->getParentKeyframe();

    if (grid == nullptr) {
        group->setGrid(new Lattice(parentKey));
        grid = group->lattice();
    }

    grid->clear();
    int cellsize = k_cellSize;
    grid->setCellSize(cellsize);
    grid->setNbCols(std::ceil((float)m_editor->tabletCanvas()->canvasRect().width() / k_cellSize));
    grid->setNbRows(std::ceil((float)m_editor->tabletCanvas()->canvasRect().height() / k_cellSize));
    grid->setOrigin(Eigen::Vector2i(m_editor->tabletCanvas()->canvasRect().x(), m_editor->tabletCanvas()->canvasRect().y()));

    bool newQuads = false;
    for (auto it = group->strokes().begin(); it != group->strokes().end(); ++it) {
        for (Interval &interval : it.value()) {
            newQuads = newQuads | addStrokeToGrid(group, parentKey->stroke(it.key()), interval);
        }
    }
    return newQuads;
}

bool GridManager::constructGrid(Group *group, ViewManager *view, Stroke *stroke, Interval &interval) {
    Lattice *grid = group->lattice();
    if (grid == nullptr) {
        group->setGrid(new Lattice(group->getParentKeyframe()));
        grid = group->lattice();
        grid->setCellSize(k_cellSize);
        grid->setNbCols(std::ceil((float)m_editor->tabletCanvas()->canvasRect().width() / k_cellSize));
        grid->setNbRows(std::ceil((float)m_editor->tabletCanvas()->canvasRect().height() / k_cellSize));
        grid->setOrigin(Eigen::Vector2i(m_editor->tabletCanvas()->canvasRect().x(), m_editor->tabletCanvas()->canvasRect().y()));
    }
    bool newQuads = addStrokeToGrid(group, stroke, interval);
    return newQuads;
}

std::pair<int, int> GridManager::addStrokeToDeformedGrid(Lattice *grid, Stroke *stroke) {
    // Remove extremities
    int startIdx = -1;
    int endIdx = -1;
    QuadPtr q;
    int k;
    bool prevPointInQuad = false, pointInQuad = false;
    QSet<int> pointsNotInGrid;
    pointsNotInGrid.reserve(stroke->size());
    for (int i = 0; i < stroke->size(); ++i) {
        pointInQuad = grid->contains(stroke->points()[i]->pos(), TARGET_POS, q, k);
        if (startIdx == -1 && pointInQuad) {
            startIdx = i;
        } else if (pointInQuad) {
            endIdx = i;
        }
        if (!pointInQuad && startIdx != -1) {
            pointsNotInGrid.insert(i);
        }
        prevPointInQuad = pointInQuad;
    }
    if (startIdx == -1 || endIdx == -1) {
        qDebug() << "addStrokeToDeformedGrid: stroke cannot be embedded into the deformed grid.";
        return {-1, -1};
    }

    QMutableSetIterator<int> pointIt(pointsNotInGrid);
    while (pointIt.hasNext()) {
        int pointIdx = pointIt.next();
        if (pointIdx > endIdx) pointIt.remove();
    }

    // Expand grid until all stroke points are covered or the maximum number of rings has been added
    const int maxSizeIncrementInPixel = 200; // TODO ui option
    const int maxSizeIncrementInRings = maxSizeIncrementInPixel / grid->cellSize();
    int i = 0;
    std::vector<int> newQuads;
    for (QuadPtr q : grid->quads()) q->setFlag(false);
    while (i < maxSizeIncrementInRings && !pointsNotInGrid.empty()) {
        newQuads.clear();
        addOneRing(grid, newQuads);
        propagateDeformToOneRing(grid, newQuads);
        for (int quadKey : newQuads) grid->quad(quadKey)->setFlag(false);
        QMutableSetIterator<int> pointIt(pointsNotInGrid);
        while (pointIt.hasNext()) {
            int pointIdx = pointIt.next();
            if (grid->contains(stroke->points()[pointIdx]->pos(), TARGET_POS, q, k)) {
                pointIt.remove();
            }
        }
        ++i;
    }

    if (!pointsNotInGrid.isEmpty()) {
        grid->deleteEmptyVolatileQuads();
        return {-1, -1};
    }

    return {startIdx, endIdx};
}

/**
 * If a grid quad contains a section of the stroke then that interval is baked into the quad (multiple distinct intervals may be baked into the same quad)
*/
void GridManager::bakeStrokeInGrid(Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type) {
    int prevKey = INT_MAX, curKey;
    int firstIdx = fromIdx;
    const std::vector<Point *> &points = stroke->points();
    int i = fromIdx;
    bool res = false;

    for (; i <= toIdx; ++i) {
        Point *point = points[i];
        QuadPtr q;
        int k;
        if (!grid->contains(point->pos(), type, q, k)) {
            qCritical() << "Error in createStrokeInterval: lattice (REF) does not contain the position " << point->pos().x() << ", " << point->pos().y() << "i = " << i << " from=" << fromIdx << "  to=" << toIdx;
        }
        curKey = q->key();
        if (curKey != prevKey) {
            if (i != fromIdx) {
                QuadPtr quad = grid->operator[](prevKey);
                Interval interval(firstIdx, i - 1);
                quad->add(stroke->id(), interval);
            }
            firstIdx = i;
            prevKey = curKey;
        }
    }

    if (i != fromIdx) {
        QuadPtr quad = grid->operator[](prevKey);
        Interval interval(firstIdx, points.size() - 1);
        quad->add(stroke->id(), interval);
    }
}

void GridManager::selectGridCorner(Group *group, PosTypeIndex type, const Point::VectorType &lastPos, bool constrained) {
    m_selectedCorner = nullptr;
    m_cornersSelected.clear();
    m_lastPos = lastPos;
    Lattice *grid = group->lattice();

    if (grid == nullptr) {
        qWarning() << "Error in selectGridCorner: invalid lattice";
        return;
    }

    // Find all corners in the selection footprint
    QVector<Corner *> &corner = grid->corners();
    Point::VectorType cornerPos;
    double distance;
    double minDist = std::numeric_limits<double>::max();
    double maxDist = std::numeric_limits<double>::min();
    for (int i = 0; i < corner.size(); i++) {
        cornerPos = corner[i]->coord(type);
        distance = (lastPos - cornerPos).norm();
        if (distance < k_deformRange) {
            corner[i]->setDeformable(true);
            m_cornersSelected.append(QPair<int, float>(i, distance));
            if (distance < minDist) {
                m_selectedCorner = corner[i];
                minDist = distance;
            }
            if (distance > maxDist) {
                maxDist = distance;
            }
        } else {
            corner[i]->setDeformable(!constrained);
        }
    }

    // Normalize and square distance
    for (QPair<int, float> &c : m_cornersSelected) {
        c.second /= maxDist;
        c.second *= c.second;
    }

    // Save the lattice configuration
    if (k_useDeformAsSource || type == REF_POS) {
        for (Corner *c : group->lattice()->corners()) {
            c->coord(DEFORM_POS) = c->coord(type);
        }
    }
}

void GridManager::moveGridCornerPosition(Group *group, PosTypeIndex type, const Point::VectorType &pos) {
    Lattice *grid = group->lattice();
    Point::VectorType targetCG = Point::VectorType::Zero();
    for (auto c : m_cornersSelected) {
        targetCG += grid->corners()[c.first]->coord(type);
    }
    targetCG /= m_cornersSelected.size();
    Point::VectorType delta = pos - targetCG;
    for (auto c : m_cornersSelected) {
        grid->corners()[c.first]->coord(type) += delta;
    }
    if (!m_cornersSelected.empty() && k_arap) {
        Arap::regularizeLattice(*grid, k_useDeformAsSource || type == REF_POS ? DEFORM_POS : REF_POS, type, k_iterationGrid);
    }
}

void GridManager::releaseGridCorner(Group *group) {
    if (group->lattice() == nullptr) {
        qCritical() << "Error in releaseGridCorner: invalid lattice";
    }
}

void GridManager::scaleGrid(Group *group, float factor, PosTypeIndex type) {
    Point::VectorType prevCenter = group->lattice()->centerOfGravity(type), center;
    Point::Affine scale;
    scale.setIdentity();
    scale.scale(factor);
    for (Corner *c : group->lattice()->corners()) {
        c->coord(type) = scale * c->coord(type);
    }
    center = group->lattice()->centerOfGravity(type);
    Point::VectorType trans = prevCenter - center;
    for (Corner *c : group->lattice()->corners()) {
        c->coord(type) += trans;
        c->coord(DEFORM_POS) = c->coord(type);
    }
}

void GridManager::scaleGrid(Group *group, float factor, PosTypeIndex type, const std::vector<Corner *> &corners) {
    Point::VectorType prevCenter, center;
    Point::VectorType res = Point::VectorType::Zero();
    for (Corner *c : corners) {
        res += c->coord(type);
    }
    prevCenter = res / corners.size();
    Point::Affine scale;
    scale.setIdentity();
    scale.scale(factor);
    for (Corner *c : corners) {
        c->coord(type) = scale * c->coord(type);
    }
    res = Point::VectorType::Zero();
    for (Corner *c : corners) {
        res += c->coord(type);
    }
    center = res / corners.size();
    Point::VectorType trans = prevCenter - center;
    for (Corner *c : corners) {
        c->coord(type) += trans;
        c->coord(DEFORM_POS) = c->coord(type);
    }
}

/**
 * If a lattice is already deformed, newly added quads are also deformed by first computing the affine transformation of the boundary between
 * new and existing quads in the least square sense and then optionally applying a few ARAP regularization iterations on the new quads + boundary
 */
void GridManager::propagateDeformToNewQuads(Lattice *grid, std::vector<QuadPtr> &newQuads) {
    // Init
    for (Corner *c : grid->corners()) {
        c->setDeformable(false);
        c->setFlag(true);
    }
    for (QuadPtr q : newQuads) {
        q->setFlag(false);
    }

    // Find connected components
    std::vector<std::vector<int>> connectedComponents;
    grid->getConnectedComponents(connectedComponents, false);

    for (auto cc : connectedComponents) {
        propagateDeformToConnectedComponent(grid, cc);
    }
    grid->setArapDirty();
}

void GridManager::propagateDeformToConnectedComponent(Lattice *grid, const std::vector<int> &quads) {
    int count = 0;
    QSet<Corner *> boundaryCorners;
    QList<Corner *> newCorners;

    // Compute boundary vertices between old & new quads and set new quads as deformable
    for (int quadKey : quads) {
        QuadPtr q = grid->quad(quadKey);
        int countDeformedCorners = 0;
        for (int j = 0; j < 4; ++j) {
            Corner *c = q->corners[j];
            if (c->coord(REF_POS) != c->coord(TARGET_POS)) {
                boundaryCorners.insert(c);
                for (int k = 0; k < 4; ++k) {
                    QuadPtr qN = c->quads((CornerIndex)k);
                    if (qN == nullptr) continue;
                    for (Corner *cN : qN->corners) {
                        if (cN->coord(REF_POS) != cN->coord(TARGET_POS)) boundaryCorners.insert(cN);
                    }
                }
                count++;
                countDeformedCorners++;
            } else {
                newCorners.append(c);
                c->setDeformable(true);
            }
        }
    }

    if (count == 0 || count == quads.size() * 4) return;

    // Compute affine transformation between boundary vertices
    Point::VectorType meanRef = Point::VectorType::Zero();
    Point::VectorType meanTgt = Point::VectorType::Zero();
    for (Corner *c : boundaryCorners) {
        meanRef += c->coord(REF_POS);
        meanTgt += c->coord(TARGET_POS);
    }
    meanRef /= (double)boundaryCorners.size();
    meanTgt /= (double)boundaryCorners.size();

    Matrix2d PiPi, QiPi;
    PiPi.setZero();
    QiPi.setZero();
    for (Corner *c : boundaryCorners) {
        Point::VectorType pi = c->coord(REF_POS) - meanRef;
        Point::VectorType qi = c->coord(TARGET_POS) - meanTgt;
        Matrix2d pipi = pi * pi.transpose();
        Matrix2d qipi = qi * pi.transpose();
        PiPi += pipi;
        QiPi += qipi;
    }
    Matrix2d R = QiPi * PiPi.inverse();
    Point::VectorType t = meanTgt - R * meanRef;
    for (Corner *c : newCorners) {
        c->coord(DEFORM_POS) = (R * c->coord(REF_POS) + t);
        c->coord(TARGET_POS) = (R * c->coord(REF_POS) + t);
    }
    // Arap::regularizeLattice(*grid, REF_POS, TARGET_POS, 20, false, true);
}

/**
 * Propagate deformation to the newly added one-ring. 
 * Assume that the newly added one-ring quads are marked true (tmp flag) 
 */
void GridManager::propagateDeformToOneRing(Lattice *grid, const std::vector<int> &oneRing) {
    // TODO: this is a dirty fix
    int neighborQuadOffset[8] = {1, grid->nbCols(), -1, -grid->nbCols(), -grid->nbCols()-1, -grid->nbCols()+1, grid->nbCols()-1, grid->nbCols()+1};
    for (int quadKey : oneRing) {
        int x, y;
        grid->keyToCoord(quadKey, x, y);
        QuadPtr quad = grid->quad(quadKey);
        QuadPtr neighbor;
        Point::Affine origToRef = Point::Affine::Identity();
        Point::VectorType positions[4] = {Point::VectorType(x, y), Point::VectorType(x + 1, y), Point::VectorType(x + 1, y + 1), Point::VectorType(x, y + 1)};
        for (int i = 0; i < 8; ++i) {
            int neighborKey = quadKey + neighborQuadOffset[i];
            if (neighborKey < 0 || neighborKey > grid->maxQuadKey() || grid->quad(neighborKey) == nullptr || grid->quad(neighborKey)->flag()) continue;
            origToRef = grid->quadRefTransformation(neighborKey);
            break;
        }
        for (int i = 0; i < NUM_CORNERS; ++i) {
            Corner *c = quad->corners[i];
            bool fixed = false;
            for (int j = 0; j < NUM_CORNERS; ++j) {
                neighbor = c->quads((CornerIndex)j);
                if (neighbor != nullptr && !neighbor->flag()) {
                    fixed = true;
                    break;
                }
            }
            if (!fixed) {
                c->coord(REF_POS) = origToRef * (grid->cellSize() * positions[i] + Point::VectorType(grid->origin().x(), grid->origin().y())); 
            }
        }
    }
    

    // TODO: batch this over multiple quads
    QuadPtr neighbor;
    QSet<Corner *> newCorners;
    std::array<bool, 4> fixedVertex;
    for (int quadKey : oneRing) {
        QuadPtr quad = grid->quad(quadKey);
        int x, y;
        grid->keyToCoord(quadKey, x, y);
        QSet<Corner *> boundaryCorners;
        // Find boundary corners
        for (int i = 0; i < NUM_CORNERS; ++i) {
            Corner *c = quad->corners[i];
            fixedVertex[i] = false;
            for (int j = 0; j < NUM_CORNERS; ++j) {
                neighbor = c->quads((CornerIndex)j);
                if (neighbor != nullptr && !neighbor->flag()) {
                    fixedVertex[i] = true;
                    for (int k = 0; k < NUM_COORDS; ++k) {
                        boundaryCorners.insert(c->quads((CornerIndex)j)->corners[k]);
                    }
                }
            }
            if (!fixedVertex[i] && newCorners.find(c) == newCorners.end()) {
                newCorners.insert(c);
                c->coord(DEFORM_POS) = Point::VectorType::Zero();
                c->coord(TARGET_POS) = Point::VectorType::Zero();
            }
        }
        // Find optimal affine transform 
        Point::VectorType meanRef = Point::VectorType::Zero();
        Point::VectorType meanTgt = Point::VectorType::Zero();
        for (Corner *c : boundaryCorners) {
            meanRef += c->coord(REF_POS);
            meanTgt += c->coord(TARGET_POS);
        }
        meanRef /= (double)boundaryCorners.size();
        meanTgt /= (double)boundaryCorners.size();
        Matrix2d PiPi, QiPi;
        PiPi.setZero();
        QiPi.setZero();
        for (Corner *c : boundaryCorners) {
            Point::VectorType pi = c->coord(REF_POS) - meanRef;
            Point::VectorType qi = c->coord(TARGET_POS) - meanTgt;
            Matrix2d pipi = pi * pi.transpose();
            Matrix2d qipi = qi * pi.transpose();
            PiPi += pipi;
            QiPi += qipi;
        }
        Matrix2d R = QiPi * PiPi.inverse();
        Point::VectorType t = meanTgt - R * meanRef;
        for (int i = 0; i < NUM_CORNERS; ++i) {
            if (fixedVertex[i]) continue;
            Corner *c = quad->corners[i];
            c->coord(DEFORM_POS) += (R * c->coord(REF_POS) + t) / double(c->nbQuads());
            c->coord(TARGET_POS) += (R * c->coord(REF_POS) + t) / double(c->nbQuads());
        }
    }
    grid->setArapDirty();
}

/**
 * Check if a polyline segment crosses over a cell without having a vertex in it. If this is the case, an empty cell is added at that location.
 * Returns true if a new cell is added, false otherwise.
 */
bool GridManager::needRefinement(Lattice *grid, Point::VectorType &prevPoint, Point::VectorType &curPoint, int &quadKeyOut) {
    if (grid == nullptr) {
        qCritical() << "Invalid grid (null)";
        return false;
    }
    int keyPrev = grid->posToKey(prevPoint);
    int keyCur = grid->posToKey(curPoint);
    int nbCols = grid->nbCols();

    // If the two successive keys are adjacent then the grid doesn't need refinement
    if (std::abs(keyPrev - keyCur) != nbCols + 1 && std::abs(keyPrev - keyCur) != nbCols - 1) {
        return false;
    }

    // In this case the grid needs refinement, we need to determine where the new cell must be added. To do this, we first identify the shared corner
    int sharedCornerKey;
    QuadPtr prevQuad = (*grid)[keyPrev];
    int keyOptionsPositive, keyOptionsNegative;
    if (keyCur == keyPrev - nbCols - 1) {
        sharedCornerKey = prevQuad->corners[TOP_LEFT]->getKey();
        keyOptionsPositive = keyPrev - 1;
        keyOptionsNegative = keyPrev - nbCols;
    } else if (keyCur == keyPrev - nbCols + 1) {
        sharedCornerKey = prevQuad->corners[TOP_RIGHT]->getKey();
        keyOptionsPositive = keyPrev - nbCols;
        keyOptionsNegative = keyPrev + 1;
    } else if (keyCur == keyPrev + nbCols - 1) {
        sharedCornerKey = prevQuad->corners[BOTTOM_LEFT]->getKey();
        keyOptionsPositive = keyPrev + nbCols;
        keyOptionsNegative = keyPrev - 1;
    } else if (keyCur == keyPrev + nbCols + 1) {
        sharedCornerKey = prevQuad->corners[BOTTOM_RIGHT]->getKey();
        keyOptionsPositive = keyPrev + 1;
        keyOptionsNegative = keyPrev + nbCols;
    }

    Point::VectorType sharedCornerPos = grid->corners()[sharedCornerKey]->coord(REF_POS);
    Point::VectorType prevToSharedCorner = sharedCornerPos - prevPoint;
    Point::VectorType segment = curPoint - prevPoint;
    double wedge = prevToSharedCorner.x() * segment.y() - prevToSharedCorner.y() * segment.x();
    quadKeyOut = wedge < 0 ? keyOptionsPositive : keyOptionsNegative;
    return true;
}

/**
 * Add a one-ring to the given grid.
 * New quads are marked as volatile
 */
void GridManager::addOneRing(Lattice *grid, std::vector<int> &newQuadsKeys) {
    int neighborQuadKeys[4] = {-1, -1, -1, -1};
    int neighborQuadOffset[4] = {1, grid->nbCols(), -1, -grid->nbCols()};  // TL->TR, TR->BR, BR->BL, BL->TL
    int x, y, j, k;
    bool isNewQuad;
    QVector<Corner *> corners = grid->corners();
    for (Corner *corner : corners) {
        if (corner->nbQuads() != NUM_CORNERS) {
            // Find the quad keys of the 4 neighboring quads of the corner
            int start = -1;
            for (int i = 0; i < NUM_CORNERS; ++i) {
                if (corner->quads((CornerIndex)i) != nullptr) {
                    neighborQuadKeys[i] = corner->quads((CornerIndex)i)->key();
                    start = i;
                    break;
                }
            }
            if (start == -1) qDebug() << "addOneRing: corner doesn't have any neighboring quad";
            for (int i = 1; i < NUM_CORNERS; ++i) {
                j = (start + i) % NUM_CORNERS; 
                k = (start + i - 1) % NUM_CORNERS;
                neighborQuadKeys[j] = neighborQuadKeys[k] + neighborQuadOffset[k]; // TODO handle border cases
                grid->keyToCoord(neighborQuadKeys[j], x, y);
                QuadPtr newQuad = grid->addQuad(neighborQuadKeys[j], x, y, isNewQuad);
                if (isNewQuad) {
                    newQuad->setVolatile(true);
                    newQuad->setFlag(true); 
                    newQuadsKeys.push_back(newQuad->key());
                } 
            }
        }
    }
}