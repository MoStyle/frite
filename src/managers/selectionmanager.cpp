/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "selectionmanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "selection.h"
#include "qteigen.h"
#include "canvascommands.h"

// Fill the paramater "selectedGroups" with all groups intersecting the polygon "bounds"
void SelectionManager::selectGroups(VectorKeyFrame *key, float alpha, unsigned int groupType, const QPolygonF &bounds, bool useFilter, std::vector<int> &selectedGroups) const {
    GroupList &groupList = groupType == POST ? key->postGroups() : key->preGroups();
    const QHash<int, Group *> &filter = groupType == POST ? key->selection().selectedPostGroups() : key->selection().selectedPreGroups();
    for (Group * group : groupList) {
        if (useFilter && filter.contains(group->id())) continue;
        if (bounds.intersects(group->cbounds())) {
            selectedGroups.push_back(group->id());
        }    
    }
}

/**
 * @param key           keyframe where the selection happens
 * @param alpha         !unused for now but should specify if we're selecting from an inbetween
 * @param groupType     what type of group to select (POST/PRE)
 * @param pickPos       the point of selection
 * @param useFilter     whether or not to use the current group filter
 * @return the id of the first group that contains pickPos
 */
int SelectionManager::selectGroups(VectorKeyFrame *key, float alpha, unsigned int groupType, const QPointF &pickPos, bool useFilter) const {
    GroupList &groupList = groupType == POST ? key->postGroups() : key->preGroups();
    const QHash<int, Group *> &filter = groupType == POST ? key->selection().selectedPostGroups() : key->selection().selectedPreGroups();
    for (Group * group : groupList) {
        if (useFilter && filter.contains(group->id())) continue;
        if (group->cbounds().contains(pickPos)) {
            return group->id();
        }    
    }
    return Group::ERROR_ID;
}

void SelectionManager::selectStrokeSegments(VectorKeyFrame *keyframe, std::function<bool(const Stroke *)> predicateStroke, std::function<bool(Point *)> predicatePoint, StrokeIntervals &selection) const {
    for (const StrokePtr &stroke : keyframe->strokes()) {
        if (!predicateStroke(stroke.get())) continue;
        selectStrokeSegments(stroke, 0, stroke->size() - 1, predicatePoint, selection[stroke->id()]);
        if (selection[stroke->id()].empty()) selection.remove(stroke->id());
    }
}

void SelectionManager::selectStrokeSegments(VectorKeyFrame *keyframe, StrokeIntervals &strokes, std::function<bool(const Stroke *)> predicateStroke, std::function<bool(Point *)> predicatePoint, StrokeIntervals &selection) const {
    for (auto it = strokes.constBegin(); it != strokes.constEnd(); ++it) {
        if (!predicateStroke(keyframe->stroke(it.key()))) continue;
        selection[it.key()].clear();
        for (const Interval &interval : it.value()) {
            selectStrokeSegments(keyframe->strokes()[it.key()], interval.from(), interval.to(), predicatePoint, selection[it.key()]);
        }
        if (selection[it.key()].empty()) selection.remove(it.key());
    }
}

void SelectionManager::selectStrokeSegments(const StrokePtr stroke, int from, int to, std::function<bool(Point *)> predicate, Intervals &selection) const {
    Point *point;
    int fromIdx = -1, toIdx = -1;
    for (size_t i = from; i <= to; ++i) {
        point = stroke->points()[i];
        if (predicate(point)) {
            if (fromIdx == -1) { // start of an interval
                fromIdx = i;
            } else if (i == to && fromIdx >= from && fromIdx < to) { // end of stroke 
                selection.append(Interval(fromIdx, to));
            }
        } else if (fromIdx != -1) { // leaving an interval
            toIdx = i - 1;
            if (toIdx - fromIdx > 0) {
                selection.append(Interval(fromIdx, toIdx));
            }
            fromIdx = -1;
        }
    }
}

void SelectionManager::selectStrokeSegments(VectorKeyFrame *keyframe, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const {
    for (const StrokePtr &stroke : keyframe->strokes()) {
        selectStrokeSegments(stroke, 0, stroke->size() - 1, predicate, selection[stroke->id()]);
        if (selection[stroke->id()].empty()) selection.remove(stroke->id());
    }
}

void SelectionManager::selectStrokeSegments(VectorKeyFrame *keyframe, StrokeIntervals &strokes, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const {
    for (auto it = strokes.constBegin(); it != strokes.constEnd(); ++it) {
        selection[it.key()].clear();
        for (const Interval &interval : it.value()) {
            selectStrokeSegments(keyframe->strokes()[it.key()], interval.from(), interval.to(), predicate, selection[it.key()]);
        }
        if (selection[it.key()].empty()) selection.remove(it.key());
    }
}

void SelectionManager::selectStrokeSegments(const StrokePtr stroke, const QPolygonF &bounds, std::function<bool(Point *)> predicate, Intervals &selection) const {
    selectStrokeSegments(stroke, 0, stroke->size() - 1, [&bounds, &predicate](Point *point) {
        return bounds.containsPoint(QPointF(point->pos().x(), point->pos().y()), Qt::OddEvenFill) && predicate(point);
    }, selection);
}

void SelectionManager::selectStrokeSegments(VectorKeyFrame *keyframe, const QPolygonF &bounds, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const {
    for (const StrokePtr &stroke : keyframe->strokes()) {
        selectStrokeSegments(stroke, bounds, predicate, selection[stroke->id()]);
        if (selection[stroke->id()].empty()) selection.remove(stroke->id());
    }
}

void SelectionManager::selectStrokeSegments(VectorKeyFrame *keyframe, const QPolygonF &bounds, std::function<bool(const Stroke *)> predicateStroke, std::function<bool(Point *)> predicate, StrokeIntervals &selection) const {
    for (const StrokePtr &stroke : keyframe->strokes()) {
        if (!predicateStroke(stroke.get())) continue;
        selectStrokeSegments(stroke, bounds, predicate, selection[stroke->id()]);
        if (selection[stroke->id()].empty()) selection.remove(stroke->id());
    }
}

void SelectionManager::selectStrokes(VectorKeyFrame *keyframe, std::function<bool(const StrokePtr &stroke)> predicate, std::vector<int> &strokesIdx) {
    for (const StrokePtr &stroke : keyframe->strokes()) {
        if (predicate(stroke)) strokesIdx.push_back(stroke->id());
    }
}

void SelectionManager::selectStrokes(VectorKeyFrame *keyframe, std::function<bool(const StrokePtr &stroke)> predicate, StrokeIntervals &selection) {
    for (const StrokePtr &stroke : keyframe->strokes()) {
        if (predicate(stroke)) {
            selection[stroke->id()].clear();
            selection[stroke->id()].append(Interval(0, stroke->size() - 1));
        }
    }
}

int SelectionManager::selectTrajectoryConstraint(VectorKeyFrame *keyframe, const QPointF &pickPos, bool useFilter) {
    for (const std::shared_ptr<Trajectory> &traj : keyframe->trajectories()) {
        if (traj->approxPathHull().contains(pickPos) && (!useFilter || keyframe->selection().selectedPostGroups().contains(traj->group()->id()))) {                   
            const std::shared_ptr<Trajectory> &selectedTraj = traj->parentTrajectory() != nullptr ? traj->parentTrajectory() : traj; 
            return selectedTraj->constraintID();
        }
    }
    return -1;
}

void SelectionManager::selectTrajectoryConstraint(VectorKeyFrame *keyframe, const QPolygonF &bounds, bool useFilter) {
    // TODO
}

// for (const std::shared_ptr<Trajectory> &traj : info.key->trajectories()) {
//             if (traj->approxPathHull().contains(info.pos) && info.key->selection().selectedPostGroups().contains(traj->group()->id())) { // TODO: go through all selected groups       
//                 if (traj.get() == info.key->selection().selectedTrajectoryPtr()) {
//                     pressedHardConstraint = true;
//                     continue;
//                 } else {
//                     const std::shared_ptr<Trajectory> &selectedTraj = traj->parentTrajectory() != nullptr ? traj->parentTrajectory() : traj; 
//                     if (selectedTraj.get() == info.key->selection().selectedTrajectoryPtr()) continue;
//                     m_editor->undoStack()->push(new SetSelectedTrajectoryCommand(m_editor, layerIdx, currentFrame, selectedTraj));
//                     return;
//                 }
//             }
//         }