/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __SELECTIONMANAGER_H__
#define __SELECTIONMANAGER_H__

#include "basemanager.h"
#include "stroke.h"

class Selection;

class SelectionManager : public BaseManager {
    Q_OBJECT

public:
    enum SelectionMode { SELECT, DESELECT };
    enum SelectionShape { LASSO, RECT, PICK };
    enum SelectionFilter { NONE, MAIN, INGROUP, EXGROUP };

    SelectionManager(QObject *pParent) : BaseManager(pParent) { }

    SelectionMode selectionMode() const { return m_selectionMode; };
    SelectionShape selectionShape() const { return m_selectionShape; };
    SelectionFilter selectionFilter() const { return m_selectionFilter; };
    void setSelectionMode(SelectionMode mode) { m_selectionMode = mode; } 

    // Select groups from a point or a shape selection
    void selectGroups(VectorKeyFrame *key, float alpha, unsigned int groupType, const QPolygonF &bounds, bool useFilter, std::vector<int> &selectedGroups) const;
    int selectGroups(VectorKeyFrame *key, float alpha, unsigned int groupType, const QPointF &pickPos, bool useFilter) const;

    // Modify the group filter
    void addToGroupFilter(int groupID) { m_groupFilter.push_back(groupID); }
    void setGroupFilter(const std::vector<int> &filter) { m_groupFilter = filter; }
    void clearGroupFilter() { m_groupFilter.clear(); }

    // Select stroke segments from a predicate function (on points or strokes)
    void selectStrokeSegments(VectorKeyFrame *keyframe, std::function<bool(const Stroke *)> predicateStroke, std::function<bool(Point *)> predicatePoint, StrokeIntervals &selection) const;
    void selectStrokeSegments(VectorKeyFrame *keyframe, StrokeIntervals &strokes, std::function<bool(const Stroke *)> predicateStroke, std::function<bool(Point *)> predicatePoint, StrokeIntervals &selection) const;
    void selectStrokeSegments(const StrokePtr stroke, int from, int to, std::function<bool(Point *)> predicate, Intervals &selection) const;
    void selectStrokeSegments(VectorKeyFrame *keyframe, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const;
    void selectStrokeSegments(VectorKeyFrame *keyframe, StrokeIntervals &strokes, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const;

    // Select stroke segments intersecting a shape (+ predicate function)
    void selectStrokeSegments(const StrokePtr stroke, const QPolygonF &bounds, std::function<bool(Point *)> predicate, Intervals &selection) const;
    void selectStrokeSegments(VectorKeyFrame *keyframe, const QPolygonF &bounds, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const;
    void selectStrokeSegments(VectorKeyFrame *keyframe, const QPolygonF &bounds, std::function<bool(const Stroke *)> predicateStroke, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const;

    // Select strokes from a predicate function
    void selectStrokes(VectorKeyFrame *keyframe, std::function<bool(const StrokePtr &stroke)> predicate, std::vector<int> &strokesIdx);
    void selectStrokes(VectorKeyFrame *keyframe, std::function<bool(const StrokePtr &stroke)> predicate, StrokeIntervals &selection);

    // Select trajectory constraint
    int selectTrajectoryConstraint(VectorKeyFrame *keyframe, const QPointF &pickPos, bool useFilter);
    void selectTrajectoryConstraint(VectorKeyFrame *keyframe, const QPolygonF &bounds, bool useFilter);

private:
    SelectionMode m_selectionMode;      // !unused
    SelectionShape m_selectionShape;    // !unused
    SelectionFilter m_selectionFilter;  // !unused

    std::vector<int> m_groupFilter;     // if used, only group with an id in this vector can be selected
};


#endif // __SELECTIONMANAGER_H__