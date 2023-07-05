/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef CANVASCOMMANDS_H
#define CANVASCOMMANDS_H

#include <QUndoCommand>

#include "vectorkeyframe.h"
#include "stroke.h"
#include "trajectory.h"

class Editor;
class TabletCanvas;
class Stroke;
class StrokeInterval;
class StrokeIntervals;
class Layer;
class Group;

class DrawCommand : public QUndoCommand {
   public:
    DrawCommand(Editor *editor, int layer, int frame, StrokePtr stroke, int groupId=-1, bool resample=true, GroupType type = POST, QUndoCommand *parent = nullptr);
    ~DrawCommand() override;

    void undo() override;
    void redo() override;

    void addBreakdownStroke(Layer *layer, VectorKeyFrame *keyframe, Group *group, const StrokePtr &copyStroke);
    void addNonBreakdownStroke(Layer *layer, VectorKeyFrame *keyframe, Group *group, const StrokePtr &copyStroke);

   private:
    Editor *m_editor;
    int m_layerIndex;
    int m_frame;
    std::unique_ptr<Stroke> m_stroke;
    int m_group;
    bool m_resample;
    bool m_breakdown;
    int m_prevCorrespondence;
    QRectF m_bounds;
    GroupType m_groupType;
};

class EraseCommand : public QUndoCommand {
   public:
    EraseCommand(Editor *editor, int layerId, int frame, int strokeId, QUndoCommand *parent = nullptr);
    ~EraseCommand() override;

    void undo() override;
    void redo() override;

    void updatePreGroup();

   private:
    Editor *m_editor;
    Layer *m_layer;
    int m_layerIndex;
    int m_frame;
    int m_stroke;
    VectorKeyFrame *m_keyframe;
    std::unique_ptr<Stroke> m_strokeCopy;
    GroupType m_groupType;
    StrokeIntervals m_postCopy;
    StrokeIntervals m_preCopy;
};

class ClearCommand : public QUndoCommand {
   public:
    ClearCommand(Editor *editor, int layer, int frame, QUndoCommand *parent = nullptr);
    ~ClearCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor *m_editor;
    int m_layerIndex;
    int m_frame;
    VectorKeyFrame *m_prevKeyframe;
};

class PasteCommand : public QUndoCommand {
   public:
    PasteCommand(Editor *editor, int layer, int frame, VectorKeyFrame *tobePasted, QUndoCommand *parent = nullptr);
    ~PasteCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor *m_editor;
    int m_layerIndex;
    int m_frame;
    VectorKeyFrame *m_source;
    VectorKeyFrame *m_prevKeyframe;
};

class AddGroupCommand : public QUndoCommand {
   public:
    AddGroupCommand(Editor *editor, int layer, int frame, GroupType type = POST, QUndoCommand *parent = nullptr);
    ~AddGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    Group *m_group;
    GroupType m_type;
};

/**
 * TODO: where do we put the stroke intervals that were referenced by the removed group? 
 * 
 */
class RemoveGroupCommand : public QUndoCommand {
   public:
    RemoveGroupCommand(Editor *editor, int layer, int frame, int group, GroupType type, QUndoCommand *parent = nullptr);
    ~RemoveGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    Group *m_groupCopy;
    GroupType m_type;
    std::vector<std::shared_ptr<Trajectory>> m_trajectories;
    int m_correspondingGroupId;
    int m_intraCorrespondingGroupId;
};

/**
 * Put the given the stroke intervals in the given group of id "groupId" and type "type"
 * 1. We're trying to put the stroke intervals into a group of type POST
 *   If the stroke intervals were already part of a POST group or the MAIN group, then they are removed from the group they previously belonged to
 *   If the stroke intervals were part of PRE group, then they stay in this PRE group and are referenced by both the PRE and POST group
 * 2. We're trying to put the stroke intervals into a group of type PRE
 *   If the stroke intervals were part of a POST or MAIN group, then they stay in their group on top of being added to the PRE group
 *   If the stroke intervals were part of a PRE group
 */
class SetGroupCommand : public QUndoCommand {
public:
    SetGroupCommand(Editor *editor, int layer, int frame, StrokeIntervals strokeIntervals, int groupId, GroupType type = POST, QUndoCommand *parent = nullptr);
    ~SetGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    int m_frame;
    int m_group;
    StrokeIntervals m_strokeIntervals;
    // groupId -> (strokeId -> intervals)
    QHash<int, StrokeIntervals> m_groupIntervalsCopy;
    GroupType m_groupType;
};

class SetSelectedGroupCommand : public QUndoCommand {
public:
    SetSelectedGroupCommand(Editor *editor, int layer, int frame, int newSelection, GroupType type = POST, bool selectInAllKF = false, QUndoCommand *parent = nullptr);
    SetSelectedGroupCommand(Editor *editor, int layer, int frame, const std::vector<int> &newSelection, GroupType type = POST, bool selectInAllKF = false, QUndoCommand *parent = nullptr);
    ~SetSelectedGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    // int m_newSelection, m_prevSelection;
    std::vector<int> m_newSelection;
    QHash<int, Group *> m_prevSelection;
    GroupType m_groupType, m_prevGroupType;
    bool m_selectInAllKF;
};

class SetSelectedTrajectoryCommand : public QUndoCommand {
public:
    SetSelectedTrajectoryCommand(Editor *editor, int layer, int frame, Trajectory *traj, bool selectInAllKF = false, QUndoCommand *parent = nullptr);
    SetSelectedTrajectoryCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, bool selectInAllKF = false, QUndoCommand *parent = nullptr);
    ~SetSelectedTrajectoryCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    Trajectory *m_traj;
    std::shared_ptr<Trajectory> m_trajShPtr;
    bool m_selectInAllKF;
};

class AddTrajectoryConstraintCommand : public QUndoCommand {
public:
    AddTrajectoryConstraintCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, QUndoCommand *parent = nullptr);
    AddTrajectoryConstraintCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, const std::shared_ptr<Trajectory> &connectedTraj, bool connectWithNext, QUndoCommand *parent = nullptr);
    ~AddTrajectoryConstraintCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    std::shared_ptr<Trajectory> m_traj;
    std::shared_ptr<Trajectory> m_connectedTraj;
    bool m_connectWithNext;
};

class RemoveTrajectoryConstraintCommand : public QUndoCommand {
public:
    RemoveTrajectoryConstraintCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, QUndoCommand *parent = nullptr);
    ~RemoveTrajectoryConstraintCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    std::shared_ptr<Trajectory> m_traj, m_prev, m_next;
};

class SyncTrajectoriesCommand : public QUndoCommand {
public:
    SyncTrajectoriesCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &trajA, const std::shared_ptr<Trajectory> &trajB, QUndoCommand *parent = nullptr);
    ~SyncTrajectoriesCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe; // trajA KF
    std::shared_ptr<Trajectory> m_trajA, m_trajB;
    Point::VectorType m_prevPA, m_prevPB;
};

class UnsyncTrajectoriesCommand : public QUndoCommand {
public:
    UnsyncTrajectoriesCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &trajA, const std::shared_ptr<Trajectory> &trajB, QUndoCommand *parent = nullptr);
    ~UnsyncTrajectoriesCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe; // trajA KF
    std::shared_ptr<Trajectory> m_trajA, m_trajB;
};

class MakeTrajectoryC1Command : public QUndoCommand {
public:
    MakeTrajectoryC1Command(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, QUndoCommand *parent = nullptr);
    ~MakeTrajectoryC1Command() override;

    void undo() override;
    void redo() override;
private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    std::shared_ptr<Trajectory> m_traj;
};

#endif  // CANVASCOMMANDS_H
