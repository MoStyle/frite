/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "selection.h"

#include "vectorkeyframe.h"

Selection::Selection(VectorKeyFrame *keyframe) : m_keyframe(keyframe) {
    m_selectedTrajectory.reset();
}

void Selection::addGroup(Group *group, unsigned int groupType) {
    QHash<int, Group *> &selectedGroups = groupType == POST ? m_selectedPostGroups : m_selectedPreGroups;
    selectedGroups.insert(group->id(), group);
}

void Selection::addGroups(const QHash<int, Group *> &groups, unsigned int groupType) {
    QHash<int, Group *> &selectedGroups = groupType == POST ? m_selectedPostGroups : m_selectedPreGroups;
    selectedGroups.insert(groups);
}

void Selection::setGroup(const QHash<int, Group *> &groups, unsigned int groupType) {
    QHash<int, Group *> &selectedGroups = groupType == POST ? m_selectedPostGroups : m_selectedPreGroups;
    selectedGroups = groups;
    if (groupType == POST) m_selectedPreGroups.clear();
    else if (groupType == PRE) m_selectedPostGroups.clear();
}

void Selection::addInterval(unsigned int strokeId, Interval interval) {
    m_selectedStrokeIntervals[strokeId].append(interval);
}

void Selection::addIntervals(unsigned int strokeId, Intervals intervals) {
    m_selectedStrokeIntervals[strokeId].append(intervals);
}

void Selection::setStrokeIntervals(const StrokeIntervals &strokeIntervals) {
    m_selectedStrokeIntervals = strokeIntervals;
}

void Selection::clearSelectedPostGroups() {
    m_selectedPostGroups.clear();
}

void Selection::clearSelectedPreGroups() {
    m_selectedPreGroups.clear();
}

void Selection::clearSelectedStrokeIntervals() {
    m_selectedStrokeIntervals.clear();
}

void Selection::clearSelectedTrajectory() {
    m_selectedTrajectory.reset();
}

void Selection::clearAll() {
    m_selectedPostGroups.clear();
    m_selectedPreGroups.clear();
    m_selectedStrokeIntervals.clear();
    m_selectedTrajectory.reset();
}

void Selection::drawSelection(QPainter &painter) {
    switch (m_objectSelectionMode) {
        case STROKES: 
        case SEGMENTS:
            
            break;
        default:
            break;
    }
}
