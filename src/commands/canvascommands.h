#ifndef CANVASCOMMANDS_H
#define CANVASCOMMANDS_H

#include <QUndoCommand>

#include "vectorkeyframe.h"
#include "partial.h"
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
    bool m_needCopy;
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

class ClearMainGroupCommand : public QUndoCommand {
   public:
    ClearMainGroupCommand(Editor *editor, int layer, int frame, QUndoCommand *parent = nullptr);
    ~ClearMainGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    VectorKeyFrame *m_keyframe;
    Group *m_groupCopy;
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
    int m_layer, m_frame;
    VectorKeyFrame *m_keyframe;
    std::vector<int> m_newSelection;
    std::vector<int> m_prevSelection;
    GroupType m_groupType, m_prevGroupType;
    bool m_selectInAllKF;
};

class SetGridCommand : public QUndoCommand {
public:
    SetGridCommand(Editor *editor, Group *group, Lattice *newGrid, QUndoCommand *parent = nullptr);
    SetGridCommand(Editor *editor, Group *group, Lattice *newGrid, const std::vector<int> &quads, QUndoCommand *parent = nullptr);
    ~SetGridCommand() override;

    void undo() override;
    void redo() override;

private:
    Editor *m_editor;
    Group *m_group;
    std::unique_ptr<Lattice> m_prevGridCopy;
    std::unique_ptr<Lattice> m_newGridCopy;
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

class MovePivotCommand : public QUndoCommand {
public:
    MovePivotCommand(Editor * editor, int layer, int frame, Point::VectorType translation, QUndoCommand *parent = nullptr);
    ~MovePivotCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor *m_editor;
    int m_frame;
    int m_layer;
    Point::VectorType m_translation;
};

class PivotTrajectoryCommand : public QUndoCommand {
public:
    PivotTrajectoryCommand(Editor * editor, int layer, int frame, Bezier2D * newTrajectory, bool breakContinuity = false, QUndoCommand *parent = nullptr);
    ~PivotTrajectoryCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_frame;
    Bezier2D * m_newTrajectory;
    Bezier2D * m_oldTrajectory;
    bool m_keepPreviousTraj;
    bool m_oldBreakContinuity;
    bool m_breakContinuity;
    int m_layer;
};

class PivotScalingCommand : public QUndoCommand {
public:
    PivotScalingCommand(Editor * editor, int layer, int frame, Point::VectorType scale, QUndoCommand *parent = nullptr);
    ~PivotScalingCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_frame;
    int m_layer;
    Point::VectorType m_newScale, m_oldScale;
};

class PivotRotationCommand : public QUndoCommand {
public:
    PivotRotationCommand(Editor * editor, int layer, int frame, Point::Scalar angle, bool currentT0 = true, bool prevT1 = true, QUndoCommand * parent = nullptr);
    ~PivotRotationCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_frame;
    int m_layer;
    bool m_useCurrentT0;
    bool m_usePrevT1;
    Point::Scalar m_angle;
};

class PivotAlignTangentCommand : public QUndoCommand {
public:
    PivotAlignTangentCommand(Editor *editor, int layer, int frame, bool start, AlignTangent alignTangent, QUndoCommand * parent = nullptr);
    ~PivotAlignTangentCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_frame;
    int m_layer;
    bool m_start;
    AlignTangent m_alignTangent;
};

class PivotTranslationExtractionCommand : public QUndoCommand {
public:
    PivotTranslationExtractionCommand(Editor * editor, int layer, QVector<VectorKeyFrame * > keys, QUndoCommand * parent = nullptr);
    ~PivotTranslationExtractionCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    QVector<VectorKeyFrame * > m_keys;
};

class PivotRotationExtractionCommand : public QUndoCommand {
public:
    PivotRotationExtractionCommand(Editor * editor, int layer, QVector<VectorKeyFrame * > keys, QVector<float> angles, QUndoCommand * parent = nullptr);
    ~PivotRotationExtractionCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    QVector<VectorKeyFrame *> m_keys;
    QVector<float> m_angles;
};


class LayerTranslationCommand : public QUndoCommand {
public:
    LayerTranslationCommand(Editor * editor, int layer, int frame, Point::VectorType translation, QUndoCommand *parent = nullptr);
    ~LayerTranslationCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_frame;
    Point::VectorType m_translation;
    int m_layer;
};

class AddOrderPartial : public QUndoCommand {
public:
    AddOrderPartial(Editor * editor, int layer, int frame, const OrderPartial &orderPartial, const OrderPartial &prevOrderPartial, QUndoCommand *parent = nullptr);
    ~AddOrderPartial() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    OrderPartial m_orderPartial, m_prevOrderPartial;
    bool m_partialExists;
};

class RemoveOrderPartial : public QUndoCommand {
public:
    RemoveOrderPartial(Editor * editor, int layer, int frame, double t, const OrderPartial& prevOrderPartial, QUndoCommand *parent = nullptr);
    ~RemoveOrderPartial() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    double m_t;
    OrderPartial m_prevOrderPartial;
};

class MoveOrderPartial : public QUndoCommand {
public:
    MoveOrderPartial(Editor * editor, int layer, int frame, double newT, double prevT, QUndoCommand *parent = nullptr);
    ~MoveOrderPartial() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    double m_t, m_prevT;
};

class SyncOrderPartialCommand : public QUndoCommand {
public:
    SyncOrderPartialCommand(Editor * editor, int layer, int frame, const Partials<OrderPartial> &prevOrder, QUndoCommand *parent = nullptr);
    ~SyncOrderPartialCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    Partials<OrderPartial> m_prevOrder;
};

class SetOrderPartialsCommand : public QUndoCommand {
public:
    SetOrderPartialsCommand(Editor * editor, int layer, int frame, const Partials<OrderPartial> &prevPartials, QUndoCommand *parent = nullptr);
    ~SetOrderPartialsCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    Partials<OrderPartial> m_newPartials, m_prevPartials;
};

class AddDrawingPartial : public QUndoCommand {
public:
    AddDrawingPartial(Editor * editor, int layer, int frame, int groupId, const DrawingPartial &drawingPartial, const DrawingPartial &prevDrawingPartial, QUndoCommand *parent = nullptr);
    ~AddDrawingPartial() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    int m_groupId;
    DrawingPartial m_drawingPartial, m_prevDrawingPartial;
    bool m_partialExists;
};

class RemoveDrawingPartial : public QUndoCommand {
public:
    RemoveDrawingPartial(Editor * editor, int layer, int frame, int groupId, double t, const DrawingPartial& prevDrawingPartial, QUndoCommand *parent = nullptr);
    ~RemoveDrawingPartial() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    int m_groupId;
    double m_t;
    DrawingPartial m_prevDrawingPartial;
};

class MoveDrawingPartial : public QUndoCommand {
public:
    MoveDrawingPartial(Editor * editor, int layer, int frame, int groupId, double newT, double prevT, QUndoCommand *parent = nullptr);
    ~MoveDrawingPartial() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    int m_groupId;
    double m_t, m_prevT;
};

class SyncDrawingPartialCommand : public QUndoCommand {
public:
    SyncDrawingPartialCommand(Editor * editor, int layer, int frame, int groupId, const Partials<DrawingPartial> &prevDrawing, QUndoCommand *parent = nullptr);
    ~SyncDrawingPartialCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    int m_groupId;
    Partials<DrawingPartial> m_prevDrawing;
};


class ComputeVisibilityCommand : public QUndoCommand {
public:
    ComputeVisibilityCommand(Editor * editor, int layer, int frame, QUndoCommand *parent = nullptr);
    ~ComputeVisibilityCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    VectorKeyFrame *m_savedKeyframe;
};

class SetVisibilityCommand : public QUndoCommand {
public:
    SetVisibilityCommand(Editor * editor, int layer, int frame, const QHash<unsigned int, double> &prevVisibility, QUndoCommand *parent = nullptr);
    ~SetVisibilityCommand() override;

    void undo() override;
    void redo() override;
private:
    Editor * m_editor;
    int m_layer;
    int m_frame;
    QHash<unsigned int, double> m_prevVisibility, m_newVisibility;
};

#endif  // CANVASCOMMANDS_H
