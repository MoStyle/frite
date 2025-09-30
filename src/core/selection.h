/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __SELECTION_H__
#define __SELECTION_H__

#include <QMap>
#include "strokeinterval.h"
#include "trajectory.h"

class Group;
class VectorKeyFrame;

// Stores the current selection of a keyframe & the selection options
class Selection {
public:
    enum ObjectSelectionMode { STROKES, SEGMENTS, GROUPS };

    Selection(VectorKeyFrame *keyframe);
    ~Selection() { }

    ObjectSelectionMode objectSelectionMode() const { return m_objectSelectionMode; };
    VectorKeyFrame *keyframe() const { return m_keyframe; }

    bool selectionEmpty() const { return m_selectedPostGroups.isEmpty() && m_selectedPreGroups.isEmpty() && m_selectedStrokeIntervals.isEmpty(); }
    bool isPostGroupSelected(int groupId) const { return m_selectedPostGroups.contains(groupId); }

    const QMap<int, Group *> &selectedPostGroups() const { return m_selectedPostGroups; }
    const QMap<int, Group *> &selectedPreGroups() const { return m_selectedPreGroups; }
    const StrokeIntervals &selectedStrokeIntervals() const { return m_selectedStrokeIntervals; }
    const std::shared_ptr<Trajectory> &selectedTrajectory() const { return m_selectedTrajectory; }
    Trajectory * selectedTrajectoryPtr() const { return m_selectedTrajectory.get(); }

    void addGroup(Group *group, unsigned int groupType);
    void addGroups(const QMap<int, Group *> &groups, unsigned int groupType);
    void setGroup(const QMap<int, Group *> &groups, unsigned int groupType);
    void addStroke(unsigned int strokeId);
    void addInterval(unsigned int strokeId, Interval interval);
    void addIntervals(unsigned int strokeId, Intervals interval);
    void setStrokeIntervals(const StrokeIntervals &strokeIntervals);
    void setSelectedTrajectory(Trajectory *traj) { m_selectedTrajectory.reset(traj); }
    void setSelectedTrajectory(const std::shared_ptr<Trajectory> &traj) { m_selectedTrajectory = traj; }

    void clearSelectedPostGroups();
    void clearSelectedPreGroups();
    void clearSelectedStrokeIntervals();
    void clearSelectedTrajectory();
    void clearAll();

    void drawSelection(QPainter &painter);

signals:
    void selectionUpdated();

private:
    ObjectSelectionMode m_objectSelectionMode;

    VectorKeyFrame *m_keyframe;

    QMap<int, Group *> m_selectedPostGroups;
    QMap<int, Group *> m_selectedPreGroups;

    StrokeIntervals m_selectedStrokeIntervals;

    std::shared_ptr<Trajectory> m_selectedTrajectory;
};

#endif // __SELECTION_H__