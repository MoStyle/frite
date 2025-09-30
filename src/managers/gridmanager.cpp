/*
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
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

/**
 * Add a stroke segment to a group lattice (REF_POS).
 * Add new quads if necessary, bake the stroke forward UVs update each intersected quad elements list
 */
bool GridManager::addStrokeToGrid(Group *group, Stroke *stroke, Interval &interval) {
    Lattice *grid = group->lattice();
    std::vector<QuadPtr> newQuads;
    Point *p;
    Point::VectorType pos, prevPos;
    bool newQuad = false;
    QuadPtr q;

    // Go through each point in the stroke between fromIdx and toIdx, if a point does not intersect the lattice add a new quad at the point position
    int nbCols = grid->nbCols();
    for (size_t i = interval.from(); i <= interval.to(); ++i) {
        p = stroke->points()[i];
        pos = p->pos();
        std::cout << "pos: " << pos.transpose() << std::endl;
        newQuad = false;
        q = grid->addQuad(pos, newQuad);
        if (newQuad) newQuads.push_back(q);
        q->setPivot(false);
        newQuad = false;

        // Check for "bowtie" corners and fix them by adding empty quads
        if (i > interval.from()) {
            int quadKey;
            if (grid->checkPotentialBowtie(prevPos, pos, quadKey)) {
                int x, y;
                grid->keyToCoord(quadKey, x, y);
                q = grid->addQuad(quadKey, x, y, newQuad);
                if (newQuad) {
                    newQuads.push_back(q);
                    q->setPivot(true);
                }
                newQuad = false;
            }
        }

        prevPos = pos;
    }

    // Recheck for bowtie corners
    group->lattice()->enforceManifoldness(group);

    // Propagate deformation to new quads (TARGET_POS, etc)
    if (!newQuads.empty()) {
        group->lattice()->isConnected();
        if (newQuads.size() != grid->quads().size()) {
            propagateDeformToNewQuads(group, grid, newQuads);
        }
        group->setGridDirty();
        group->lattice()->setBackwardUVDirty(true);
    }

    // Bake the new stroke segment in the lattice + compute UVs
    bakeStrokeInGrid(grid, stroke, interval.from(), interval.to());
    grid->bakeForwardUV(stroke, interval, group->uvs());

    return !newQuads.empty();
}

bool GridManager::addStrokeToGrid(Group *group, Stroke *stroke, Intervals &intervals) {
    bool res = false;
    for (Interval &interval : intervals) {
        res = res || addStrokeToGrid(group, stroke, interval);
    }
    return res;
}

// Fill the given group's lattice with the strokes in the group
bool GridManager::constructGrid(Group *group, ViewManager *view, unsigned int cellSize) {
    Lattice *grid = group->lattice();
    VectorKeyFrame *parentKey = group->getParentKeyframe();

    if (grid == nullptr) {
        group->setGrid(new Lattice(parentKey));
        grid = group->lattice();
    }

    grid->clear();
    grid->setCellSize(cellSize);
    grid->setNbCols(std::ceil((float)m_editor->tabletCanvas()->canvasRect().width() / cellSize));
    grid->setNbRows(std::ceil((float)m_editor->tabletCanvas()->canvasRect().height() / cellSize));
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

/**
 * Expand a lattice until the given stroke can fit in its deformed configuration (TARGET_POS).
 * If the stroke does not intersect the lattice it is not added.
 * The extremities of the stroke that do not intersect the lattice are removed if removeExtremities is true.
 * Otherwise, the lattice is expanded by incrementally adding one rings until all remaining stroke points intersect the lattice.
 * 
 * /!\ the stroke is not baked into the lattice! this method only adds quads!
 * 
 * Return the stroke interval that has been embedded in the lattice (or {-1,-1} if the stroke could not be embedded)
 */
std::pair<int, int> GridManager::expandTargetGridToFitStroke(Lattice *grid, Stroke *stroke, bool removeExtremities, int from, int to) {
    // Remove extremities
    if (to < 0) to = stroke->size() - 1;
    if (from < 0) from = 0;
    int startIdx = -1;
    int endIdx = -1;
    QuadPtr q;
    int k;
    bool prevPointInQuad = false, pointInQuad = false;
    QSet<int> pointsNotInGrid;
    pointsNotInGrid.reserve(stroke->size());
    for (int i = from; i <= to; ++i) {
        pointInQuad = grid->contains(stroke->points()[i]->pos(), TARGET_POS, q, k);
        // std::cout << "point " << i << " | " << stroke->points()[i]->pos().transpose() << " " << pointInQuad << std::endl;
        if (startIdx == -1 && pointInQuad) {
            startIdx = i;
            endIdx = i;
        } else if (pointInQuad) {
            endIdx = i;
        }
        if (!pointInQuad && (!removeExtremities || startIdx != -1)) {
            pointsNotInGrid.insert(i);
        }
        prevPointInQuad = pointInQuad;
    }

    qDebug() << "(" << startIdx << ", " << endIdx << ")";
    qDebug() << "pointsNotInGrid size " << pointsNotInGrid.size();

    if (startIdx == -1 || endIdx == -1) {
        qDebug() << "expandTargetGridToFitStroke: stroke cannot be embedded into the deformed grid.";
        return {-1, -1};
    }

    if (!removeExtremities) {
        startIdx = 0;
        endIdx = stroke->size() - 1;
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
    for (QuadPtr q : grid->quads()) {
        q->setMiscFlag(false);
    }
    for (Corner *c : grid->corners()) {
        c->setDeformable(false);
    }
    while (!pointsNotInGrid.empty()) {
        newQuads.clear();
        addOneRing(grid, newQuads);
        propagateDeformToOneRing(grid, newQuads);
        for (int quadKey : newQuads) grid->quad(quadKey)->setMiscFlag(false);
        Arap::regularizeLattice(*grid, REF_POS, TARGET_POS, 5000, false); 
        QMutableSetIterator<int> pointIt(pointsNotInGrid);
        while (pointIt.hasNext()) {
            int pointIdx = pointIt.next();
            if (grid->contains(stroke->points()[pointIdx]->pos(), TARGET_POS, q, k)) {
                pointIt.remove();
            }
        }
        qDebug() << "#quads: " << grid->size();
        qDebug() << "#points not in grid: " << pointsNotInGrid.size();
        ++i;
    }

    for (Corner *c : grid->corners()) {
        c->setDeformable(true);
    }

    if (!pointsNotInGrid.isEmpty()) {
        grid->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0 && !q->isPivot()); });
        return {-1, -1};
    }


    return {startIdx, endIdx};
}

bool GridManager::expandTargetGridToFitStroke(Group *group, const StrokeIntervals &intervals, StrokeIntervals &added, StrokeIntervals &notAdded) {
    if (group == nullptr || group->lattice() == nullptr) return false;

    qDebug() << "IN expandTargetGridToFitStroke";

    added.clear();
    notAdded = intervals;

    // Expand grid until all stroke points are covered or the maximum number of rings has been ad   ded
    const int maxSizeIncrementInPixel = 1920; // TODO ui option
    const int maxSizeIncrementInRings = maxSizeIncrementInPixel / group->lattice()->cellSize();
    int i = 0;

    std::vector<int> newQuads;
    for (QuadPtr q : group->lattice()->quads()) {
        q->setMiscFlag(false);          // tag quad in the last one ring
        q->setFlag(MISC2_QUAD, false);  // tag all quads that intersects at least one point of the new strokes
        q->setFlag(MISC3_QUAD, false);  // tag all quads that intersects the new strokes and form a valid path (subset of MISC2_QUAD)
        q->setFlag(DIRTY_QUAD, false);  // tag all new quads added by the grid expansion
    }
    for (Corner *c : group->lattice()->corners()) {
        c->setDeformable(false);
    }

    qDebug() << "#quads before: " << group->lattice()->size();
    qDebug() << "#notAdded: " << notAdded.nbPoints();
    qDebug() << "#added: " << added.nbPoints();
    qDebug() << "maxSizeIncrementInRings = " << maxSizeIncrementInRings;

    int nbIterationsWithNoChange = 0;
    int prevNbQuads = group->lattice()->size();
    while (nbIterationsWithNoChange < 3 && i < maxSizeIncrementInRings && !notAdded.empty()) {
        // Expand grid in REF_POS and propagate existing deformation to new quads
        newQuads.clear();
        addOneRing(group->lattice(), newQuads);
        propagateDeformToOneRing(group->lattice(), newQuads);
        for (int quadKey : newQuads) {
            group->lattice()->quad(quadKey)->setMiscFlag(false); 
            group->lattice()->quad(quadKey)->setFlag(DIRTY_QUAD, true); 
        }
        Arap::regularizeLattice(*group->lattice(), REF_POS, TARGET_POS, 1000, false); 

        // Try to bake non added strokes
        QMutableHashIterator<unsigned int, Intervals> itNotAdded(notAdded);
        while (itNotAdded.hasNext()) {
            itNotAdded.next();
            QMutableListIterator<Interval> itIntervals(itNotAdded.value());
            while (itIntervals.hasNext()) {
                const Interval &interval = itIntervals.next();
                std::set<int> intersectedQuads;

                // Tag new non-empty quads
                if (group->lattice()->intersectedQuads(group->getParentKeyframe()->stroke(itNotAdded.key()), interval.from(), interval.to(), TARGET_POS, intersectedQuads)) {
                    for (int k : intersectedQuads) {
                        group->lattice()->quad(k)->setFlag(MISC2_QUAD, true);
                    }
                }

                // Add interval if it can be fully baked
                if (group->lattice()->contains(group->getParentKeyframe()->stroke(itNotAdded.key()), interval.from(), interval.to(), TARGET_POS, true)) {
                    group->lattice()->tagValidPath(group->getParentKeyframe()->stroke(itNotAdded.key()), interval.from(), interval.to(), TARGET_POS, MISC3_QUAD);
                    added[itNotAdded.key()].append(interval);
                    itIntervals.remove();
                }
            }
            if (itNotAdded.value().isEmpty()) itNotAdded.remove();
        }

        // Remove new empty quads that are not adjacent to new non-empty quads
        group->lattice()->deleteQuadsPredicate([&](QuadPtr q) {
            bool adjToNonEmptyQuad = false;
            std::array<int, 8> keys;
            for (int j = 0; j < 8; ++j) {
                if (group->lattice()->quad(keys[j]) != nullptr && group->lattice()->quad(keys[j])->flag(MISC2_QUAD)) adjToNonEmptyQuad = true;
            }
            return q->flag(DIRTY_QUAD) && !q->flag(MISC2_QUAD) && !adjToNonEmptyQuad; 
        });

        qDebug() << "#quads: " << group->lattice()->size();
        qDebug() << "#notAdded: " << notAdded.nbPoints();
        qDebug() << "#added: " << added.nbPoints();

        if (group->lattice()->size() == prevNbQuads) ++nbIterationsWithNoChange;
        ++i;
        prevNbQuads = group->lattice()->size();
        m_editor->tabletCanvas()->update();
    }

    if (!notAdded.empty()) {
        qDebug() << "Grid expansion failed removing quads";
        group->lattice()->deleteQuadsPredicate([&](QuadPtr q) { return q->flag(DIRTY_QUAD); });
    } else {
        // Remove quads that are not in a valid path of adjacent to a valid path
        group->lattice()->deleteQuadsPredicate([&](QuadPtr q) {
            bool adjToValidQuad = false;
            std::array<int, 8> keys;
            for (int j = 0; j < 8; ++j) {
                if (group->lattice()->quad(keys[j]) != nullptr && group->lattice()->quad(keys[j])->flag(MISC3_QUAD)) adjToValidQuad = true;
            }
            return q->flag(DIRTY_QUAD) && !q->flag(MISC3_QUAD) && !adjToValidQuad; 
        });
    }

    qDebug() << "finished expansion in " << i << " iterations";
    return notAdded.empty();
}

std::pair<int, int> GridManager::expandTargetGridToFitStroke2(Lattice *grid, Stroke *stroke, bool removeExtremities, int from, int to) {
    if (grid->isEmpty()) return {-1, -1};

    std::vector<std::pair<unsigned int, unsigned int>> segments;    // first and last points are in the grid, inbetween points are not in the grid
    std::vector<std::pair<int, int>> segmentsQuadKeys;              // quad keys of the first and last point of the segment
    std::vector<double> cellSizes;                                  // default cellSize for each segment
    double length = stroke->length(from, to);

    // Compute segments
    QuadPtr q;
    int k = INT_MAX, lastKey;
    bool isLastPointIn = true, pointInQuad, intersection = false, startInGrid = true;
    for (int i = from; i <= to; ++i) {
        pointInQuad = grid->contains(stroke->points()[i]->pos(), TARGET_POS, q, k);
        if (!pointInQuad && isLastPointIn) { // segment starts
            if (i == from) startInGrid = false;
            segments.push_back({std::max(from, i - 1), -1});
            segmentsQuadKeys.push_back({lastKey, INT_MAX});
        } else if (pointInQuad && !isLastPointIn) { // segment ends
            intersection = true;
            segments.back().second = i;
            segmentsQuadKeys.back().second = k;
        }
        lastKey = k;
    }
    if (!pointInQuad && !isLastPointIn) {
        segments.back().second = to; // complete final segment
    }

    if (!intersection) {
        qDebug() << "expandTargetGridToFitStroke: stroke cannot be embedded into the deformed grid.";
        return {-1, -1};
    }

    double sizeStart, sizeEnd;
    for (auto segment : segmentsQuadKeys) {
        if (segment.first == INT_MAX && segment.second == INT_MAX) qWarning() << "Error in expandTargetGridToFitStroke2: invalid segment";
        sizeStart = segment.first == INT_MAX ? std::numeric_limits<double>::max() : grid->quad(segment.first)->averageEdgeLength(TARGET_POS);
        sizeEnd = segment.second == INT_MAX ? std::numeric_limits<double>::max() : grid->quad(segment.second)->averageEdgeLength(TARGET_POS);
        cellSizes.push_back(std::min(sizeStart, sizeEnd));
    }

    static int dx[NUM_EDGES] = {0, 1, 0, -1};   // indices correspond to Corner::EdgeIndex
    static int dy[NUM_EDGES] = {1, 0, -1, 0};

    for (int i = 0; i < segments.size(); ++i) {
        auto segment = segments[i];
        auto quadKeys = segmentsQuadKeys[i];
        double cellSize = cellSizes[i];

        // Detect with which edge we should connect to
        if (quadKeys.first != INT_MAX) {
            QuadPtr startQuad = grid->quad(quadKeys.first);
            int x, y, nx, ny, nKey;
            grid->keyToCoord(startQuad->key(), x, y);
            for (int j = 0; j < 4; ++j) {
                if (Geom::checkSegmentsIntersection(stroke->points()[segment.first + 1]->pos(), stroke->points()[segment.first]->pos(), startQuad->corners[j]->coord(TARGET_POS), startQuad->corners[(j + 1) % 4]->coord(TARGET_POS))) {
                    nx = x + dx[j]; // j corresponds to the edge index because Corner::EdgeIndex and Corner::CornerIndex match
                    ny = y + dy[j];
                    break;
                }
            }
            nKey = grid->coordToKey(nx, ny);
            if (grid->contains(nKey)) qWarning() << "Error in expandTargetGridToFitStroke2: detected edge has two adjacent quads!";

        }

    }
    return {-1, -1};
    // Remove extremities
    // if (to < 0) to = stroke->size() - 1;
    // if (from < 0) from = 0;
    // int startIdx = -1;
    // int endIdx = -1;
    // QuadPtr q;
    // int k;
    // bool prevPointInQuad = false, pointInQuad = false;
    // QSet<int> pointsNotInGrid;
    // pointsNotInGrid.reserve(stroke->size());
    // for (int i = from; i <= to; ++i) {
    //     pointInQuad = grid->contains(stroke->points()[i]->pos(), TARGET_POS, q, k);
    //     // std::cout << "point " << i << " | " << stroke->points()[i]->pos().transpose() << " " << pointInQuad << std::endl;
    //     if (startIdx == -1 && pointInQuad) {
    //         startIdx = i;
    //         endIdx = i;
    //     } else if (pointInQuad) {
    //         endIdx = i;
    //     }
    //     if (!pointInQuad && (!removeExtremities || startIdx != -1)) {
    //         pointsNotInGrid.insert(i);
    //     }
    //     prevPointInQuad = pointInQuad;
    // }

    // qDebug() << "(" << startIdx << ", " << endIdx << ")";
    // qDebug() << "pointsNotInGrid size " << pointsNotInGrid.size();

    // if (startIdx == -1 || endIdx == -1) {
    //     qDebug() << "expandTargetGridToFitStroke: stroke cannot be embedded into the deformed grid.";
    //     return {-1, -1};
    // }

    // if (!removeExtremities) {
    //     startIdx = 0;
    //     endIdx = stroke->size() - 1;
    // }

    // QMutableSetIterator<int> pointIt(pointsNotInGrid);
    // while (pointIt.hasNext()) {
    //     int pointIdx = pointIt.next();
    //     if (pointIdx > endIdx) pointIt.remove();
    // }

    // // Expand grid until all stroke points are covered or the maximum number of rings has been added
    // const int maxSizeIncrementInPixel = 200; // TODO ui option
    // const int maxSizeIncrementInRings = maxSizeIncrementInPixel / grid->cellSize();
    // int i = 0;
    // std::vector<int> newQuads;
    // for (QuadPtr q : grid->quads()) {
    //     q->setMiscFlag(false);
    // }
    // for (Corner *c : grid->corners()) {
    //     c->setDeformable(false);
    // }
    // while (!pointsNotInGrid.empty()) {
    //     newQuads.clear();
    //     addOneRing(grid, newQuads);
    //     propagateDeformToOneRing(grid, newQuads);
    //     for (int quadKey : newQuads) grid->quad(quadKey)->setMiscFlag(false);
    //     Arap::regularizeLattice(*grid, REF_POS, TARGET_POS, 5000, false); 
    //     QMutableSetIterator<int> pointIt(pointsNotInGrid);
    //     while (pointIt.hasNext()) {
    //         int pointIdx = pointIt.next();
    //         if (grid->contains(stroke->points()[pointIdx]->pos(), TARGET_POS, q, k)) {
    //             pointIt.remove();
    //         }
    //     }
    //     qDebug() << "#quads: " << grid->size();
    //     qDebug() << "#points not in grid: " << pointsNotInGrid.size();
    //     ++i;
    // }

    // for (Corner *c : grid->corners()) {
    //     c->setDeformable(true);
    // }

    // if (!pointsNotInGrid.isEmpty()) {
    //     grid->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0 && !q->isPivot()); });
    //     return {-1, -1};
    // }


    // return {startIdx, endIdx};
}


/**
 * Expand the lattice (by adding quads) so that the entirety of the stroke is inside the lattice (using its position at the given inbetween).
 * The stroke should at least have one point inside the lattice.
 * /!\ the stroke is not baked into the lattice! this method only adds quads!
 * Return true if the lattice was successfully expanded
 */
bool GridManager::expandGridToFitStroke(Group *group, const Inbetween &inbetween, int inbetweenNumber, int stride, Lattice *grid, Stroke *stroke) {
    static int dx[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
    static int dy[8] = {-1, -1, -1, 0, 1, 1, 1, 0};

    // TODO handle alrge inbetweening number

    for (QuadPtr q : grid->quads()) q->setMiscFlag(false);

    // Test if the stroke has at least one point inside the lattice
    QuadPtr q; int k;
    bool pointInside = false;
    std::set<int> intersectedQuads;
    QSet<int> pointsNotInGrid;
    for (int i = 0; i < stroke->size(); ++i) {
        if (inbetween.contains(group, stroke->points()[i]->pos(), q, k)) {
            pointInside = true;
            intersectedQuads.insert(k);
        } else {
            pointsNotInGrid.insert(i);
            // TODO: Test if the point is inside the canvas?
        }
    }
    if (!pointInside) return false;

    // qDebug() << "intersectedQuads: " << intersectedQuads.size();

    int x, y, xx, yy;
    bool isNewQuad;
    QuadPtr newQuad;
    std::vector<QuadPtr> newQuads;
    while (!pointsNotInGrid.empty()) {
        // Add adjacent quads (4-neighborhood) to all intersected quads at the previous iteration 
        for (int k : intersectedQuads) {
            grid->keyToCoord(k, x, y);
            for (int i = 0; i < 8; ++i) {
                xx = x + dx[i];
                yy = y + dy[i];
                if (xx << grid->nbCols() || yy < grid->nbRows() || xx >= 0 || yy >= 0) {
                    newQuad = grid->addQuad(grid->coordToKey(xx, yy), xx, yy, isNewQuad);
                    if (isNewQuad) newQuads.push_back(newQuad);
                }
            }
        }

        propagateDeformToNewQuads(group, grid, newQuads);

        // qDebug() << "newQuads.size() : " << newQuads.size();
        qDebug() << "pointsNotInGrid.size() : " << pointsNotInGrid.size();

        // Refresh points not in grid set
        newQuads.clear();
        intersectedQuads.clear();
        group->getParentKeyframe()->makeInbetweenDirty(inbetweenNumber);
        m_editor->updateInbetweens(group->getParentKeyframe(), inbetweenNumber, stride);
        QMutableSetIterator<int> pointIt(pointsNotInGrid);
        while (pointIt.hasNext()) {
            int pointIdx = pointIt.next();
            if (inbetween.contains(group, stroke->points()[pointIdx]->pos(), q, k)) {
                pointIt.remove();
                intersectedQuads.insert(k);
            }
        }
        // qDebug() << "intersectedQuads: " << intersectedQuads.size();
    }

    return true;
}

/**
 * Remove empty quads and make sure the grid is still manifold
 */
void GridManager::retrocomp(Group *group) {
    group->lattice()->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0); });
    group->lattice()->enforceManifoldness(group);
}

/**
 * If a quad contains a section of the stroke then that interval is baked into the quad (multiple distinct intervals may be baked into the same quad)
*/
bool GridManager::bakeStrokeInGrid(Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type, bool forward) {
    int prevKey = INT_MAX, curKey;
    int firstIdx = fromIdx;
    const std::vector<Point *> &points = stroke->points();
    int i = fromIdx, k;
    QuadPtr q;

    for (; i <= toIdx; ++i) {
        Point *point = points[i];
        if (!grid->contains(point->pos(), type, q, k)) {
            qCritical() << "Error in bakeStrokeInGrid: lattice (" << type << ") does not contain the position " << point->pos().x() << ", " << point->pos().y() << "i = " << i << " from=" << fromIdx << "  to=" << toIdx;
            Q_ASSERT_X(false, "bakeStrokeInGrid", "doesn't contain point");
        }
        curKey = q->key();
        q->setPivot(false);

        // Change quads, bake segment
        if (curKey != prevKey) {
            if (i != fromIdx) {
                QuadPtr quad = grid->operator[](prevKey);
                Interval interval(firstIdx, i - 1);
                if (forward)    quad->addForward(stroke->id(), interval);
                else            quad->addBackward(stroke->id(), interval);            

            }
            firstIdx = i;
            prevKey = curKey;
        }
    }

    if (i != fromIdx) {
        QuadPtr quad = grid->operator[](prevKey);
        Interval interval(firstIdx, points.size() - 1);
        if (forward)    quad->addForward(stroke->id(), interval);
        else            quad->addBackward(stroke->id(), interval);
    }

    return true;
}

/**
 * If a quad contains a section of the stroke then that interval is baked into the quad (multiple distinct intervals may be baked into the same quad)
*/
void GridManager::bakeStrokeInGrid(Group *group, Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, const Inbetween &inbetween, bool forward) {
    int prevKey = INT_MAX, curKey;
    int firstIdx = fromIdx;
    const std::vector<Point *> &points = stroke->points();
    int i = fromIdx;

    for (; i <= toIdx; ++i) {
        Point *point = points[i];
        QuadPtr q;
        int k;
        if (!inbetween.contains(group, point->pos(), q, k)) {
            qCritical() << "Error in bakeStrokeInGrid: the inbetween grid does not contain the position " << point->pos().x() << ", " << point->pos().y() << "i = " << i << " from=" << fromIdx << "  to=" << toIdx;
        }
        curKey = q->key();
        q->setPivot(false);
        if (curKey != prevKey) {
            if (i != fromIdx) {
                QuadPtr quad = grid->operator[](prevKey);
                Interval interval(firstIdx, i - 1);
                if (forward)    quad->addForward(stroke->id(), interval);
                else            quad->addBackward(stroke->id(), interval);            

            }
            firstIdx = i;
            prevKey = curKey;
        }
    }

    if (i != fromIdx) {
        QuadPtr quad = grid->operator[](prevKey);
        Interval interval(firstIdx, points.size() - 1);
        if (forward)    quad->addForward(stroke->id(), interval);
        else            quad->addBackward(stroke->id(), interval);
    }
}

void GridManager::bakeStrokeInGridPrecomputed(Lattice *grid, Group *group, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type, bool forward) {
    int prevKey = INT_MAX, curKey;
    int firstIdx = fromIdx;
    const std::vector<Point *> &points = stroke->points();
    int i = fromIdx;

    for (; i <= toIdx; ++i) {
        Point *point = points[i];
        if (!group->uvs().has(stroke->id(), i) || !grid->quads().contains(group->uvs().get(stroke->id(), i).quadKey)) {
            qCritical() << "Error bakeStrokeInGridPrecomputed!";
        }
        QuadPtr q = grid->quad(group->uvs().get(stroke->id(), i).quadKey);
        int k;
        if (!grid->quadContainsPoint(q, point->pos(), type)) {
            qCritical() << "Error in bakeStrokeInGridPrecomputed: lattice (" << type << ") does not contain the position " << point->pos().x() << ", " << point->pos().y() << "i = " << i << " from=" << fromIdx << "  to=" << toIdx << " | stroke " << stroke->id() << " | " << group->getParentKeyframe()->keyframeNumber();
        }
        curKey = q->key();
        q->setPivot(false);
        if (curKey != prevKey) {
            if (i != fromIdx) {
                QuadPtr quad = grid->operator[](prevKey);
                Interval interval(firstIdx, i - 1);
                if (forward)    quad->addForward(stroke->id(), interval);
                else            quad->addBackward(stroke->id(), interval);            

            }
            firstIdx = i;
            prevKey = curKey;
        }
    }

    if (i != fromIdx) {
        QuadPtr quad = grid->operator[](prevKey);
        Interval interval(firstIdx, points.size() - 1);
        if (forward)    quad->addForward(stroke->id(), interval);
        else            quad->addBackward(stroke->id(), interval);
    }
}

bool GridManager::bakeStrokeInGridWithConnectivityCheck(Lattice *grid, Stroke *stroke, int fromIdx, int toIdx, PosTypeIndex type, bool forward) {
    int prevKey = INT_MAX, curKey;
    int firstIdx = fromIdx;
    const std::vector<Point *> &points = stroke->points();
    int i = fromIdx;
    Point *point = points[i];

    for (; i <= toIdx; ++i) {
        std::set<int> quads;
        for (QuadPtr q : grid->quads()) {
            if (grid->quadContainsPoint(q, stroke->points()[i]->pos(), type)) {
                quads.insert(q->key());
            }
        }

        if (quads.empty()) {
            qCritical() << "Error in bakeStrokeInGrid: lattice (" << type << ") does not contain the position " << point->pos().x() << ", " << point->pos().y() << "i = " << i << " from=" << fromIdx << "  to=" << toIdx;
            Q_ASSERT_X(false, "bakeStrokeInGrid", "doesn't contain point");
        }

        // first point
        if (prevKey == INT_MAX) {
            curKey = *quads.begin();
            prevKey = curKey;
            grid->quad(curKey)->setPivot(false);
            continue;
        }

        // still in the same quad
        if (quads.find(prevKey) != quads.end()) {
            curKey = prevKey;
            grid->quad(curKey)->setPivot(false);
            continue;
        }

        // current quad changed, check adjacency, choose first
        bool foundQk = false;
        for (int qk : quads) {
            // changed quads, bake segment
            if (grid->areQuadsConnected(qk, prevKey)) {
                int x, y, xx, yy;
                grid->keyToCoord(qk, x, y);
                grid->keyToCoord(prevKey, xx, yy);
                foundQk = true;
                curKey = qk;
                grid->quad(curKey)->setPivot(false);
                if (i != fromIdx) {
                    QuadPtr quad = grid->operator[](prevKey);
                    Interval interval(firstIdx, i - 1);
                    if (forward)    quad->addForward(stroke->id(), interval);
                    else            quad->addBackward(stroke->id(), interval);            
                }
                firstIdx = i;
                prevKey = curKey;

                break;
            }
        }

        // point cannot doesn't intersect an adjacent quad of the previous one
        if (!foundQk) {
            Q_ASSERT_X(false, "bakeStrokeInGridConnectivityCheck", "connectivity check failed");
        }
    }

    if (i != fromIdx) {
        QuadPtr quad = grid->operator[](prevKey);
        Interval interval(firstIdx, points.size() - 1);
        if (forward)    quad->addForward(stroke->id(), interval);
        else            quad->addBackward(stroke->id(), interval);
    }

    return true;
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
        if (distance < k_deformRange * 0.5) {
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

void GridManager::scaleGrid(Group *group, float factor, PosTypeIndex type, int mode) {
    Point::VectorType prevCenter = group->lattice()->centerOfGravity(type), center;
    Point::Affine scale;
    scale.setIdentity();
    if (mode == 0) {
        scale.scale(group->lattice()->scaling()(0, 0) * factor);
        group->lattice()->setScaling(scale);
        scale.setIdentity();
        scale.scale(factor);
        // group->lattice()->setScale(group->lattice()->scale() * factor); // TODO replace with affine transform
    } else if (mode == 1) {
        scale.scale(Eigen::Vector2d(group->lattice()->scaling()(0, 0), group->lattice()->scaling()(1, 1) * factor));
        group->lattice()->setScaling(scale);
        scale.setIdentity();
        scale.scale(Eigen::Vector2d(1.0, factor));
    } else if (mode == 2) {
        scale.scale(Eigen::Vector2d(group->lattice()->scaling()(0, 0) * factor, group->lattice()->scaling()(1, 1)));
        group->lattice()->setScaling(scale);
        scale.setIdentity();
        scale.scale(Eigen::Vector2d(factor, 1.0));
    }
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

void GridManager::scaleGrid(Group *group, float factor, PosTypeIndex type, const std::vector<Corner *> &corners, int mode) {
    Point::VectorType prevCenter, center;
    Point::VectorType res = Point::VectorType::Zero();
    for (Corner *c : corners) {
        res += c->coord(type);
    }
    prevCenter = res / corners.size();
    Point::Affine scale;
    scale.setIdentity();
    if (mode == 0) {
        scale.scale(factor);
        // group->lattice()->setScale(group->lattice()->scale() * factor); // TODO replace with affine transform
    } else if (mode == 1) {
        scale.scale(Eigen::Vector2d(1.0, factor));
    } else if (mode == 2) {
        scale.scale(Eigen::Vector2d(factor, 1.0));
    }
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
 * Overwrite misc and deformable flags
 */
void GridManager::propagateDeformToNewQuads(Group *group, Lattice *grid, std::vector<QuadPtr> &newQuads) {
    // Init
    for (Corner *c : grid->corners()) {
        c->setDeformable(false);
        c->setMiscFlag(true);
    }
    for (QuadPtr q : newQuads) {
        q->setMiscFlag(false);
    }

    // Find connected components
    std::vector<std::vector<int>> connectedComponents;
    grid->getConnectedComponents(connectedComponents, false);

    for (auto cc : connectedComponents) {
        propagateDeformToConnectedComponent(grid, cc);
    }
    group->setGridDirty();

    for (QuadPtr q : grid->quads()) {
        q->setMiscFlag(false);
    }
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
    Arap::regularizeLattice(*grid, REF_POS, TARGET_POS, 20, false, true);
}

/**
 * Propagate deformation to the newly added one-ring. 
 * Assume that the newly added one-ring quads are marked true (tmp flag) 
 */
void GridManager::propagateDeformToOneRing(Lattice *grid, const std::vector<int> &oneRing) {
    // TODO: batch this over multiple quads
    std::array<bool, 4> fixedVertex;
    QuadPtr neighbor;
    QSet<Corner *> newCorners;
    for (int quadKey : oneRing) {
        QuadPtr quad = grid->quad(quadKey);
        QSet<Corner *> boundaryCorners;
        // Find boundary corners
        for (int i = 0; i < NUM_CORNERS; ++i) {
            Corner *c = quad->corners[i];
            fixedVertex[i] = false;
            for (int j = 0; j < NUM_CORNERS; ++j) {
                neighbor = c->quads((CornerIndex)j);
                if (neighbor != nullptr && !neighbor->miscFlag()) {
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
}

/**
 * Add a one-ring to the given grid.
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
                j = Utils::pmod(start + i, (int)NUM_CORNERS);
                k = Utils::pmod(start + i - 1, (int)NUM_CORNERS);
                neighborQuadKeys[j] = neighborQuadKeys[k] + neighborQuadOffset[k]; // TODO handle border cases
                grid->keyToCoord(neighborQuadKeys[j], x, y);
                QuadPtr newQuad = grid->addQuad(neighborQuadKeys[j], x, y, isNewQuad);
                if (isNewQuad) {
                    newQuad->setMiscFlag(true); 
                    newQuadsKeys.push_back(newQuad->key());
                } 
            }
        }
    }
}