#include "canvascommands.h"

#include "editor.h"
#include "gridmanager.h"
#include "layer.h"
#include "layermanager.h"
#include "tabletcanvas.h"
#include "vectorkeyframe.h"
#include "viewmanager.h"
#include "playbackmanager.h"
#include "visibilitymanager.h"
#include "fixedscenemanager.h"
#include "selectionmanager.h"
#include "viewmanager.h"
#include "strokeinterval.h"
#include "toolsmanager.h"
#include "bezier2D.h"
#include "utils/stopwatch.h"

DrawCommand::DrawCommand(Editor *editor, int layer, int frame, StrokePtr stroke, int groupId, bool resample, GroupType type, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layerIndex(layer), m_frame(frame), m_stroke(new Stroke(*stroke)), m_group(groupId), m_resample(resample), m_groupType(type) {
    setText("Draw stroke");
}

DrawCommand::~DrawCommand() {}

void DrawCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);

    // Remove visibility attributes
    if (!layer->keyExists(m_frame) || m_frame == layer->getMaxKeyFramePosition() && m_group != Group::ERROR_ID) {
        Stroke *stroke = keyframe->stroke(m_stroke->id());
        Group *group = m_groupType == PRE ? keyframe->preGroups().fromId(m_group) : keyframe->postGroups().fromId(m_group);

        if (!group->strokes().contains(stroke->id())) {
            qCritical() << "Error in DrawCommand::undo: group " << group->id() << " does not contain stroke id " << stroke->id();
            return;
        }

        for (auto it = group->strokes().value(stroke->id()).cbegin(); it != group->strokes().value(stroke->id()).cend(); ++it) {
            for (unsigned int i = it->from(); i < it->to(); ++i) {
                keyframe->visibility().remove(Utils::cantor(stroke->id(), i));
            }
        }

    }

    // Remove stroke from KF and groups
    keyframe->removeLastStroke();

    // If the stroke was not linked to any group we only need to update the animation
    if (m_group == Group::ERROR_ID && m_groupType != MAIN) {
        keyframe->makeInbetweensDirty();  // TODO dirty update
        return;
    }

    Group *group = nullptr;
    if (m_groupType == PRE)
        group = keyframe->preGroups().fromId(m_group);
    else
        group = keyframe->postGroups().fromId(m_group);
    if (group == nullptr) qFatal("Error when undoing a stroke: cannot retrieve its parent group");

    // reset the group where the stroke was added
    group->bounds() = m_bounds;

    // TODO: restore intra correspondence if group isn't a breakdown
    // if (!m_breakdown) {
    // if (m_groupType == POST) keyframe->addIntraCorrespondence(m_prevCorrespondence, m_group);
    // else if (m_groupType == PRE) keyframe->addIntraCorrespondence(m_group, m_prevCorrespondence);
    // }

    emit m_editor->tabletCanvas()->groupModified(m_groupType, m_group);

    keyframe->makeInbetweensDirty();  // TODO dirty update
}

void DrawCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    StrokePtr copyStroke = std::make_shared<Stroke>(*m_stroke);

    // Add a stroke without any group
    if (m_group == Group::ERROR_ID && m_groupType != MAIN) {
        keyframe->addStroke(copyStroke, nullptr, m_resample);
        keyframe->makeInbetweensDirty();
        return;
    }

    if (m_groupType == PRE && keyframe->preGroups().empty()) qFatal("Error when drawing in pre group : no pre groups exist in this keyframe");
    if (m_groupType == POST && keyframe->postGroups().empty()) qFatal("Error when drawing in post group : no post groups exist in this keyframe");

    // find the given group from its id and type
    Group *group = nullptr;
    if (m_groupType == PRE)
        group = keyframe->preGroups().fromId(m_group);
    else
        group = keyframe->postGroups().fromId(m_group);

    copyStroke->transform(group->globalRigidTransform   (0).inverse());

    // TODO: remove intra correspondence if group isn't a breakdown
    m_breakdown = group->breakdown();

    // save the previous boundary to undo later
    m_bounds = group->bounds();

    // update the grid with only the new stroke data
    if (m_breakdown)
        addBreakdownStroke(layer, keyframe, group, copyStroke);
    else
        addNonBreakdownStroke(layer, keyframe, group, copyStroke);

    emit m_editor->tabletCanvas()->groupModified(m_groupType, m_group);
    keyframe->makeInbetweensDirty();  // TODO dirty update
}

void DrawCommand::addBreakdownStroke(Layer *layer, VectorKeyFrame *keyframe, Group *group, const StrokePtr &copyStroke) {
    // clamp stroke intervals to the lattice
    Intervals clampedStroke;
    StrokePtr newStroke = keyframe->addStroke(copyStroke, nullptr, m_resample);
    m_editor->selection()->selectStrokeSegments(
        newStroke, QRectF(m_editor->tabletCanvas()->canvasRect()),
        [&group](Point *p) {
            QuadPtr q;
            int k;
            return group->lattice()->contains(p->pos(), REF_POS, q, k);
        },
        clampedStroke);

    // add clamped stroke segments to the group
    group->addStroke(newStroke->id(),  clampedStroke);

    int preGroupId = Group::ERROR_ID;

    // if the selected group is POST, then we also add the new stroke to its corresponding PRE group in the same keyframe
    if (group->type() == POST) {
        preGroupId = keyframe->intraCorrespondences().key(group->id(), Group::ERROR_ID);
        if (preGroupId == Group::ERROR_ID) qCritical() << "Error in DrawCommand redo: breakdown key should have valid intra-correspondences (" << group->id() << ")";
        keyframe->preGroups().fromId(preGroupId)->addStroke(newStroke->id(), clampedStroke);
    } else if (group->type() == PRE) {
        preGroupId = group->id();
    }

    // dirty the previous keyframe corresponding lattice
    if (group->type() == PRE || group->type() == POST) {
        VectorKeyFrame *prev = layer->getPrevKey(keyframe);
        int prevPostGroupId = prev->correspondences().key(preGroupId, Group::ERROR_ID);
        if (prevPostGroupId == Group::ERROR_ID) qCritical() << "Error in DrawCommand redo: breakdown key should have valid correspondences (" << preGroupId << ")";
        prev->postGroups().fromId(prevPostGroupId)->lattice()->setBackwardUVDirty(true);
        prev->makeInbetweensDirty();
    }

    // bake the new stroke in the selected group lattice (without adding new quads since the topology shouldn't change at a breakdown key)
    for (Interval &interval : group->strokes()[newStroke->id()]) {
        for (int i = interval.from(); i <= interval.to(); ++i) newStroke->points()[i]->setGroupId(group->id());
        m_editor->grid()->bakeStrokeInGrid(group->lattice(), newStroke.get(), interval.from(), interval.to());
        group->lattice()->bakeForwardUV(newStroke.get(), interval, group->uvs());
    }
}

void DrawCommand::addNonBreakdownStroke(Layer *layer, VectorKeyFrame *keyframe, Group *group, const StrokePtr &copyStroke) {
    if (m_groupType != PRE) {
        if (group->size() > 0 && (!layer->keyExists(m_frame) || m_frame == layer->getMaxKeyFramePosition())) { // drawing on an inbetween
            int inbetweenNumber = keyframe->parentLayer()->inbetweenPosition(m_frame);
            int stride = keyframe->parentLayer()->stride(m_frame);
            const Inbetween &inbetween = keyframe->inbetween(inbetweenNumber);
            StrokePtr newStroke = keyframe->addStroke(copyStroke, nullptr, m_resample);
            bool res = m_editor->grid()->expandGridToFitStroke(group, inbetween, inbetweenNumber, stride, group->lattice(), newStroke.get());
            if (res) {
                group->addStroke(newStroke->id());
                Interval &interval = group->strokes()[newStroke->id()].back();
                std::vector<QuadPtr> newQuads;
                // group->lattice()->enforceManifoldness(newStroke.get(), interval, newQuads);
                m_editor->grid()->bakeStrokeInGrid(group, group->lattice(), newStroke.get(), 0, newStroke->size() - 1, inbetween);
                group->lattice()->enforceManifoldness(newStroke.get(), interval, newQuads, true);
                inbetween.bakeForwardUV(group, newStroke.get(), interval, group->uvs());
                // TODO do not delete quads that were manually added!
                group->lattice()->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0 && !q->isPivot()); });
                group->setGridDirty();
                // Set visibility
                double alpha = m_editor->alpha(m_frame);
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    UVInfo uv = group->uvs().get(newStroke->id(), i);
                    newStroke->points()[i]->setPos(group->lattice()->getWarpedPoint(newStroke->points()[i]->pos(), uv.quadKey, uv.uv, REF_POS));
                    keyframe->visibility()[Utils::cantor(newStroke->id(), i)] = alpha;
                }
                keyframe->makeInbetweensDirty();
            } else {
                keyframe->removeStroke(newStroke->id());
            }
        } else {    // drawing on a keyframe
            StrokePtr newStroke = keyframe->addStroke(copyStroke, group, m_resample);
            Interval &interval = group->strokes()[newStroke->id()].back();
            // if we're not drawing in a pre group we have to potentially add quads to the lattice
            bool newQuads = m_editor->grid()->constructGrid(group, m_editor->view(), newStroke.get(), interval);
            // ? not sure if we need to do that since this group is not a breakdown
            if (newQuads) keyframe->removeIntraCorrespondence(m_groupType == POST ? group->prevPreGroupId() : m_group);
            if (!layer->keyExists(m_frame) || m_frame == layer->getMaxKeyFramePosition()) {
                // Set visibility
                double alpha = m_editor->alpha(m_frame);
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    UVInfo uv = group->uvs().get(newStroke->id(), i);
                    newStroke->points()[i]->setPos(group->lattice()->getWarpedPoint(newStroke->points()[i]->pos(), uv.quadKey, uv.uv, REF_POS));
                    keyframe->visibility()[Utils::cantor(newStroke->id(), i)] = alpha;
                }
            }
        }
    } else {
        // if we're drawing in a pre group, then we need tell the corresponding previous post group to update its backward UVs
        if (m_frame == layer->firstKeyFramePosition()) return;
        VectorKeyFrame *prev = keyframe->prevKeyframe();
        int prePostGroupId = prev->correspondences().key(group->id(), Group::ERROR_ID);

        if (prePostGroupId == Group::ERROR_ID) {
            qWarning() << "Warning in DrawCommand: this pre group is not connected to any post group in the previous keyframe. Why does it even exist?";
        }

        Group *prevPostGroup = prev->postGroups().fromId(prePostGroupId);
        prevPostGroup->lattice()->setBackwardUVDirty(true);
        prev->makeInbetweensDirty();

        // clamp stroke intervals to the lattice
        Intervals clampedStroke;
        StrokePtr newStroke = keyframe->addStroke(copyStroke, nullptr, m_resample);
        m_editor->selection()->selectStrokeSegments(
            newStroke, QRectF(m_editor->tabletCanvas()->canvasRect()),
            [&prevPostGroup](Point *p) {
                QuadPtr q;
                int k;
                return prevPostGroup->lattice()->contains(p->pos(), TARGET_POS, q, k);
            },
            clampedStroke);

        // add clamped stroke segments to the group
        group->addStroke(newStroke->id(), clampedStroke);

        // bake stroke in the previous grid
        for (Interval interval : clampedStroke) {
            m_editor->grid()->bakeStrokeInGrid(prevPostGroup->lattice(), newStroke.get(), interval.from(), interval.to(), TARGET_POS, false);
        }
    }
}

// TODO remove stroke by id, and replace it at the same position in the vector
EraseCommand::EraseCommand(Editor *editor, int layerId, int frame, int strokeId, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layerIndex(layerId), m_stroke(strokeId), m_frame(frame), m_needCopy(true) {
    setText("Erase stroke");

    m_layer = m_editor->layers()->layerAt(m_layerIndex);
    m_keyframe = m_layer->getLastVectorKeyFrameAtFrame(m_frame, 0);

    double alpha = m_editor->alpha(m_frame);

    // Copy all StrokeIntervals referencing the deleted stroke
    // ! In this case the hash for StrokeIntervals is group ID and not the stroke ID (which we already know)
    for (Group *group : m_keyframe->postGroups()) {
        DrawingPartial &partial = group->drawingPartials().lastPartialAt(alpha);
        if (group->contains(m_stroke, alpha)) {
            m_postCopy.insert(group->id(), partial.strokes().value(m_stroke));
        }
        bool found = false;
        for (const DrawingPartial &partial : group->drawingPartials().partials()) {
            // No need to copy the stroke if it appears in multiple partials since we shouldn't it in that case
            if (found) {
                m_needCopy = false;
                return;
            }
            if (partial.strokes().contains(m_stroke)) {
                found = true;
            }
        }
    }
    for (Group *group : m_keyframe->preGroups()) {
        if (group->contains(m_stroke)) {
            m_preCopy.insert(group->id(), group->strokes().value(m_stroke));
        }
    }

    // Copy the deleted stroke
    Stroke *stroke = m_keyframe->stroke(m_stroke);
    if (m_needCopy) {
        m_strokeCopy = std::make_unique<Stroke>(*stroke);
    }
}

EraseCommand::~EraseCommand() {}

void EraseCommand::undo() {
    // Readd the deleted stroke if necessary
    Stroke *stroke = m_needCopy ? m_keyframe->addStroke(std::make_shared<Stroke>(*m_strokeCopy), nullptr, false).get() : m_keyframe->stroke(m_stroke);

    double alpha = m_editor->alpha(m_frame);

    // Restore the StrokeIntervals and update lattices and uvs
    for (auto it = m_postCopy.constBegin(); it != m_postCopy.constEnd(); ++it) {
        int groupId = it.key();
        Group *group = m_keyframe->postGroups().fromId(groupId);
        DrawingPartial &partial = group->drawingPartials().lastPartialAt(alpha);

        // Add stroke intervals to the group
        if (partial.t() == 0.0) {
            group->addStroke(m_stroke, it.value());
        } else {
            partial.strokes().insert(m_stroke, it.value());
        }
        
        // Bake UVs and add to grid if necessary
        // TODO what to if t > 0
        if (m_needCopy) {
            Intervals &intervals = group->strokes()[m_stroke];
            if (!group->breakdown() && partial.t() == 0.0) {
                for (Interval &interval : intervals) {
                    m_editor->grid()->constructGrid(group, m_editor->view(), stroke, interval);
                }
            } else {
                for (Interval &interval : intervals) {
                    m_editor->grid()->bakeStrokeInGrid(group->lattice(), stroke, interval.from(), interval.to());
                    group->lattice()->bakeForwardUV(stroke, interval, group->uvs());
                }
            }
        }
    }

    for (auto it = m_preCopy.begin(); it != m_preCopy.end(); ++it) {
        Group *group = m_keyframe->preGroups().fromId(it.key());
        group->addStroke(m_stroke, it.value());
    }

    if (m_postCopy.size() > 0) {
        emit m_editor->tabletCanvas()->groupsModified(POST);
    } else {
        updatePreGroup();
        emit m_editor->tabletCanvas()->groupsModified(PRE);
    }

    m_keyframe->makeInbetweensDirty();  // TODO dirty update
}

void EraseCommand::redo() {
    double alpha = m_editor->alpha(m_frame);

    // Only remove the stroke if there is only one drawing partial referencing it
    if (m_needCopy) {
        m_keyframe->removeStroke(m_stroke, true);
    } else {
        for (auto it = m_postCopy.constBegin(); it != m_postCopy.constEnd(); ++it) {
            Group *group = m_keyframe->postGroups().fromId(it.key());
            DrawingPartial &partial = group->drawingPartials().lastPartialAt(alpha);
            group->clearStrokes(m_stroke, partial.id(), false); // TODO remove quads if we ever add lattice partial
        }
    }

    if (m_postCopy.size() > 0) {
        emit m_editor->tabletCanvas()->groupsModified(POST);
    } else {
        // this means we're in a pre group, so we need to update the previous post group lattice backward uvs
        updatePreGroup();
        emit m_editor->tabletCanvas()->groupsModified(PRE);
    }

    m_keyframe->makeInbetweensDirty();  // TODO dirty update
}

// If we're erasing strokes in a PRE group, then we need to update the corresponding POST group in the previous frame
void EraseCommand::updatePreGroup() {
    if (m_keyframe->keyframeNumber() == m_layer->firstKeyFramePosition()) return;
    VectorKeyFrame *prev = m_keyframe->prevKeyframe();
    for (auto it = m_preCopy.constBegin(); it != m_preCopy.constEnd(); ++it) {
        int prePostGroupId = prev->correspondences().key(it.key(), Group::ERROR_ID);
        if (prePostGroupId == Group::ERROR_ID) qWarning() << "Warning in EraseCommand: this pre group is not connected to any post group in the previous keyframe. Why does it even exist?";
        Group *prevPostGroup = prev->postGroups().fromId(prePostGroupId);
        prevPostGroup->lattice()->setBackwardUVDirty(true);
    }
    prev->makeInbetweensDirty();
}

ClearCommand::ClearCommand(Editor *editor, int layer, int frame, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layerIndex(layer), m_frame(frame) {
    setText("Clear canvas");
}

ClearCommand::~ClearCommand() { delete m_prevKeyframe; }

void ClearCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layerIndex);
    layer->insertKeyFrame(m_frame, m_prevKeyframe->copy());
    layer->getVectorKeyFrameAtFrame(m_frame)->makeInbetweensDirty();
    emit m_editor->tabletCanvas()->frameModified(m_frame);
    m_editor->updateUI(m_prevKeyframe);
}

void ClearCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *key = layer->getVectorKeyFrameAtFrame(m_frame); 
    m_prevKeyframe = key->copy();
    key->clear();      
    key->makeInbetweensDirty();
    emit m_editor->tabletCanvas()->frameModified(m_frame);
    m_editor->updateUI(key);
}

PasteCommand::PasteCommand(Editor *editor, int layer, int frame, VectorKeyFrame *tobePasted, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layerIndex(layer), m_frame(frame), m_source(tobePasted->copy()) {
    setText("Paste");
}

PasteCommand::~PasteCommand() { delete m_source; }

void PasteCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layerIndex);
    layer->insertKeyFrame(m_frame, m_prevKeyframe->copy());
}

void PasteCommand::redo() {
    // Layer *layer = m_editor->layers()->layerAt(m_layerIndex);
    // VectorKeyFrame *dest = layer->getVectorKeyFrameAtFrame(m_frame);
    // m_prevKeyframe = dest->copy();
    // if (m_editor->tabletCanvas()->somethingSelected()) {
    //     dest->paste(m_source, m_editor->tabletCanvas()->getSelection()->bounds());
    // } else {
    //     dest->paste(m_source);
    // }
}

AddGroupCommand::AddGroupCommand(Editor *editor, int layer, int frame, GroupType type, QUndoCommand *parent) : QUndoCommand(parent), m_editor(editor), m_type(type) {
    setText("New group");
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    m_group = new Group(m_keyframe, type);
}

AddGroupCommand::~AddGroupCommand() { delete m_group; }

void AddGroupCommand::undo() {
    if (m_type == POST)
        m_keyframe->postGroups().removeGroup(m_group->id());
    else 
        m_keyframe->preGroups().removeGroup(m_group->id());

    emit m_editor->tabletCanvas()->frameModified(m_type);
}

void AddGroupCommand::redo() {
    if (m_type == POST)
        m_keyframe->postGroups().add(new Group(*m_group));
    else
        m_keyframe->preGroups().add(new Group(*m_group));

    emit m_editor->tabletCanvas()->frameModified(m_type);
}

RemoveGroupCommand::RemoveGroupCommand(Editor *editor, int layer, int frame, int group, GroupType type, QUndoCommand *parent) : QUndoCommand(parent), m_editor(editor), m_type(type) {
    setText("New group");
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    if (type == POST) {
        m_groupCopy = new Group(*m_keyframe->postGroups().fromId(group));
        // copy trajectories
        if ( m_keyframe->postGroups().fromId(group)->lattice() != nullptr) {
            for (unsigned int trajID : m_keyframe->postGroups().fromId(group)->lattice()->constraints()) {
                m_trajectories.push_back(m_keyframe->trajectoryConstraint(trajID));
                m_trajectories.back()->setGroup(m_groupCopy);
            }
        }
    } else if (type == PRE) {
        m_groupCopy = new Group(*m_keyframe->preGroups().fromId(group));
    }
}

RemoveGroupCommand::~RemoveGroupCommand() { delete m_groupCopy; }

void RemoveGroupCommand::undo() {
    if (m_groupCopy->id() == Group::MAIN_GROUP_ID) return;

    if (m_type == POST) {
        m_keyframe->postGroups().add(new Group(*m_groupCopy));
        Group *newGroup = m_keyframe->postGroups().lastGroup();

        // Restore stroke-grid correspondence
        for (auto it = newGroup->strokes().begin(); it != newGroup->strokes().end(); ++it) {
            for (Interval &interval : it.value()) {
                m_editor->grid()->bakeStrokeInGrid(newGroup->lattice(), m_keyframe->stroke(it.key()), interval.from(), interval.to());
            }
        }

        // Restore correspondences
        if (m_correspondingGroupId != Group::ERROR_ID) m_keyframe->addCorrespondence(m_groupCopy->id(), m_correspondingGroupId);
        if (m_intraCorrespondingGroupId != Group::ERROR_ID) m_keyframe->addIntraCorrespondence(m_intraCorrespondingGroupId, m_groupCopy->id());

        // Restore trajectories connections
        for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
            unsigned int id = m_keyframe->addTrajectoryConstraint(std::make_shared<Trajectory>(*traj));
            const std::shared_ptr<Trajectory> &newTraj = m_keyframe->trajectories()[id];
            newTraj->setGroup(newGroup);
            if (newTraj->nextTrajectory() != nullptr) m_keyframe->connectTrajectories(newTraj, newTraj->nextTrajectory(), true);
            if (newTraj->prevTrajectory() != nullptr) m_keyframe->connectTrajectories(newTraj, newTraj->prevTrajectory(), false);
        }

    } else {
        m_keyframe->preGroups().add(new Group(*m_groupCopy));
        if (m_correspondingGroupId != Group::ERROR_ID) m_keyframe->prevKeyframe()->addCorrespondence(m_correspondingGroupId, m_groupCopy->id());
        if (m_intraCorrespondingGroupId != Group::ERROR_ID) m_keyframe->addIntraCorrespondence(m_groupCopy->id(), m_intraCorrespondingGroupId);
    } 

    emit m_editor->tabletCanvas()->frameModified(m_type);
}

void RemoveGroupCommand::redo() {
    if (m_groupCopy->id() == Group::MAIN_GROUP_ID) return;

    // TODO remove correspondences
    

    if (m_type == POST) {
        m_correspondingGroupId = m_keyframe->correspondences().value(m_groupCopy->id(), Group::ERROR_ID);
        m_intraCorrespondingGroupId = m_keyframe->intraCorrespondences().key(m_groupCopy->id(), Group::ERROR_ID);
        for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
            m_keyframe->removeTrajectoryConstraint(traj->constraintID());
        }
        Group *prev = m_keyframe->postGroups().removeGroup(m_groupCopy->id());
        if (prev) delete prev;
    } else {
        m_correspondingGroupId = m_keyframe->prevKeyframe()->correspondences().key(m_groupCopy->id(), Group::ERROR_ID);  // TODO should be previous keyframe
        m_intraCorrespondingGroupId = m_keyframe->intraCorrespondences().value(m_groupCopy->id(), Group::ERROR_ID);
        Group *prev = m_keyframe->preGroups().removeGroup(m_groupCopy->id());
        if (prev) delete prev;
    }

    emit m_editor->tabletCanvas()->frameModified(m_type);
}

ClearMainGroupCommand::ClearMainGroupCommand(Editor *editor, int layer, int frame, QUndoCommand *parent) : QUndoCommand(parent), m_editor(editor) {
    setText("Clear main group");
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    m_groupCopy = new Group(*m_keyframe->postGroups().fromId(Group::MAIN_GROUP_ID));
}

ClearMainGroupCommand::~ClearMainGroupCommand() {
    delete m_groupCopy;
}

void ClearMainGroupCommand::undo() {
    m_keyframe->postGroups().add(new Group(*m_groupCopy), true);
    Group *mainGroup = m_keyframe->postGroups().fromId(Group::MAIN_GROUP_ID);
    
    // Restore strokes-grid connection
    for (auto strokeIt = mainGroup->strokes().constBegin(); strokeIt != mainGroup->strokes().constEnd(); ++strokeIt) {
        for (const Interval &interval : strokeIt.value()) {
            Stroke *stroke = m_keyframe->stroke(strokeIt.key());
            m_editor->grid()->bakeStrokeInGrid(mainGroup->lattice(), stroke, interval.from(), interval.to());
        }
    }
}

void ClearMainGroupCommand::redo() {
    m_keyframe->postGroups().fromId(Group::MAIN_GROUP_ID)->clear();
}

// TODO const StrokeIntervals&
SetGroupCommand::SetGroupCommand(Editor *editor, int layer, int frame, StrokeIntervals strokeIntervals, int groupId, GroupType type, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_frame(frame), m_group(groupId), m_strokeIntervals(strokeIntervals), m_groupType(type) {
    setText("Set group");
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);

    GroupList &groupList = m_groupType == POST ? m_keyframe->postGroups() : m_keyframe->preGroups();

    // Copy intervals of affected strokes
    for (auto groupIt = groupList.cbegin(); groupIt != groupList.cend(); ++groupIt) {
        for (auto strokeIt = m_strokeIntervals.constBegin(); strokeIt != m_strokeIntervals.constEnd(); ++strokeIt) {
            Group *group = *groupIt;
            if (!group->strokes().contains(strokeIt.key())) continue;
            m_groupIntervalsCopy[group->id()][strokeIt.key()] = group->strokes()[strokeIt.key()];
        }
    }
}

SetGroupCommand::~SetGroupCommand() {}

// TODO: macro the iteration through the qhash and its underlying qlist (e.g foreach_interval)
void SetGroupCommand::undo() {
    GroupList &groupList = m_groupType == POST ? m_keyframe->postGroups() : m_keyframe->preGroups();

    // go through all groups affected by the change
    for (auto groupCopyIt = m_groupIntervalsCopy.constBegin(); groupCopyIt != m_groupIntervalsCopy.constEnd(); ++groupCopyIt) {
        auto &groupStrokesCopy = groupCopyIt.value();
        Group *group = groupList.fromId(groupCopyIt.key());

        // go through all the strokes of this group that were affected by the change
        for (auto strokeCopyIt = groupStrokesCopy.begin(); strokeCopyIt != groupStrokesCopy.end(); ++strokeCopyIt) {
            Stroke *stroke = m_keyframe->stroke(strokeCopyIt.key());

            // restore the correct group id to each vertex
            for (const Interval &interval : strokeCopyIt.value()) {
                for (size_t i = interval.from(); i <= interval.to(); ++i) {
                    stroke->points()[i]->setGroupId(groupCopyIt.key());
                }
            }

            // restore list of intervals
            group->clearStrokes(strokeCopyIt.key());
            group->addStroke(strokeCopyIt.key(), strokeCopyIt.value());
            m_editor->grid()->constructGrid(group, m_editor->view(), group->lattice()->cellSize());
        }
    }

    m_keyframe->makeInbetweensDirty();
    emit m_editor->tabletCanvas()->groupsModified(m_groupType);
}

// TODO : lots of improvements to do here...
//  - do we allow intervals of size 1?
void SetGroupCommand::redo() {
    GroupList &groupList = m_groupType == POST ? m_keyframe->postGroups() : m_keyframe->preGroups();

    for (auto strokeIt = m_strokeIntervals.constBegin(); strokeIt != m_strokeIntervals.constEnd(); ++strokeIt) {
        Stroke *stroke = m_keyframe->stroke(strokeIt.key());

        // remove all the selected strokes from their groups, and set the new group id to all the vertices selected
        for (const Interval &interval : strokeIt.value()) {
            for (size_t i = interval.from(); i <= interval.to(); ++i) {
                if (m_groupType == POST) {
                    groupList.fromId(stroke->points()[i]->groupId())->clearStrokes(strokeIt.key());    
                }
                stroke->points()[i]->setGroupId(m_group);
            }
        }

        // with all strokes having each of their vertices linked to a group ID, we go through each strokes, creating intervals of vertices with contiguous group ID,
        // and put these intervals in their respective group (either the new group or the group they previously belonged to)
        int prevGroup = stroke->points()[0]->groupId(), from = 0, curGroup, to;
        Group *group;
        for (size_t i = 1; i < stroke->size(); ++i) {
            curGroup = stroke->points()[i]->groupId();
            // interval change or end of stroke
            if (prevGroup != curGroup || i == stroke->size() - 1) {
                to = (i == stroke->size() - 1) ? i : i - 1;
                group = groupList.fromId(prevGroup);
                Interval &interval = group->addStroke(strokeIt.key(), Interval(from, to));
                m_editor->grid()->constructGrid(group, m_editor->view(), stroke, interval);
                from = i;
            }
            prevGroup = curGroup;
        }
    }

    m_keyframe->makeInbetweensDirty();
    emit m_editor->tabletCanvas()->groupsModified(m_groupType);
}

// If the selected id is Group::ERROR_ID then it considered a "deselection"
SetSelectedGroupCommand::SetSelectedGroupCommand(Editor *editor, int layer, int frame, int newSelection, GroupType type, bool selectInAllKF, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_groupType(type), m_selectInAllKF(selectInAllKF) {
    setText("Select Group");
    m_newSelection.push_back(newSelection);
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
}

SetSelectedGroupCommand::SetSelectedGroupCommand(Editor *editor, int layer, int frame, const std::vector<int> &newSelection, GroupType type, bool selectInAllKF, QUndoCommand *parent)
    : QUndoCommand(parent),
      m_layer(layer),
      m_frame(frame),
      m_editor(editor),
      m_newSelection(newSelection),
      m_groupType(type),
      m_selectInAllKF(selectInAllKF) {
    setText("Select Group");
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
}

SetSelectedGroupCommand::~SetSelectedGroupCommand() {}

void SetSelectedGroupCommand::undo() {
    QMap<int, Group *> selection;
    for (int id : m_prevSelection) {
        if (m_groupType == POST) {
            selection.insert(id, m_keyframe->postGroups().fromId(id));
        } else {
            selection.insert(id, m_keyframe->preGroups().fromId(id));
        }
    }
    m_keyframe->selection().setGroup(selection, m_groupType); // Set keyframe's selection
    m_editor->updateUI(m_keyframe);
}

void SetSelectedGroupCommand::redo() {
    GroupList &groupList = m_groupType == POST ? m_keyframe->postGroups() : m_keyframe->preGroups();
    const QMap<int, Group *> &sel = m_groupType == POST ? m_keyframe->selection().selectedPostGroups() : m_keyframe->selection().selectedPreGroups();

    // Store the previous selection
    QList<int> keys;
    if (m_groupType == POST) {
        keys = m_keyframe->selection().selectedPostGroups().keys();
    } else {
        keys = m_keyframe->selection().selectedPreGroups().keys();
    }
    m_prevSelection = std::vector<int>(keys.begin(), keys.end());

    // Set the current selection
    QMap<int, Group *> newSelection;
    for (int id : m_newSelection) {
        if (id == Group::ERROR_ID) {
            newSelection.clear();
            break;
        }
        newSelection.insert(id, groupList.fromId(id));
    }

    // Propagate selection across keyframes if possible (breakdowns)
    bool deselect = newSelection.isEmpty();

    // If we're deselecting, remove any selected group that was empty (because there will be no way to reselect it again anyway)
    if (deselect) {
        m_editor->deleteAllEmptyGroups(m_layer, m_frame);
    }

    const QMap<int, Group *> &propagationStart = deselect ? m_keyframe->selection().selectedPostGroups() : newSelection;
    if (m_selectInAllKF && !propagationStart.isEmpty() && m_groupType == POST) {
        bool firstPass = true;
        for (Group *group : propagationStart) {
            Group *cur = group;
            while (cur->nextPostGroup() != nullptr) {
                cur = cur->nextPostGroup();
                if (deselect || firstPass)  cur->getParentKeyframe()->selection().setGroup(deselect ? newSelection : QMap<int, Group *>({std::make_pair((int)cur->id(), cur)}), POST);
                else                        cur->getParentKeyframe()->selection().addGroup(cur, POST);
            }
            cur = group;
            while (cur->prevPostGroup() != nullptr) {
                cur = cur->prevPostGroup();
                if (deselect || firstPass)  cur->getParentKeyframe()->selection().setGroup(deselect ? newSelection : QMap<int, Group *>({std::make_pair((int)cur->id(), cur)}), POST);
                else                        cur->getParentKeyframe()->selection().addGroup(cur, POST);
            }
            firstPass = false;
        }
    }


    m_keyframe->selection().setGroup(newSelection, m_groupType);    // Set keyframe's selection
    m_editor->updateUI(m_keyframe);
}


SetGridCommand::SetGridCommand(Editor *editor, Group *group, Lattice *grid, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_prevGridCopy(new Lattice(*group->lattice())), m_newGridCopy(new Lattice(*grid))
{
    setText("Set grid");
}

SetGridCommand::SetGridCommand(Editor *editor, Group *group, Lattice *grid, const std::vector<int> &quads, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_group(group), m_prevGridCopy(new Lattice(*group->lattice())), m_newGridCopy(new Lattice(*grid, quads))
{
    setText("Set grid");
}

SetGridCommand::~SetGridCommand() {
    
}

void SetGridCommand::undo() {
    m_group->setGrid(new Lattice(*m_prevGridCopy));
    m_group->setGridDirty();
    m_group->lattice()->setBackwardUVDirty(true);
}

void SetGridCommand::redo() {
    m_group->setGrid(new Lattice(*m_newGridCopy));
    m_group->setGridDirty();
    m_group->lattice()->setBackwardUVDirty(true);
}

SetSelectedTrajectoryCommand::SetSelectedTrajectoryCommand(Editor *editor, int layer, int frame, Trajectory *traj, bool selectInAllKF, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_traj(traj), m_selectInAllKF(selectInAllKF) {
    m_trajShPtr.reset((Trajectory *)nullptr);
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Select Trajectory");
}

SetSelectedTrajectoryCommand::SetSelectedTrajectoryCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, bool selectInAllKF, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_traj(traj.get()), m_trajShPtr(traj), m_selectInAllKF(selectInAllKF) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Select Trajectory");
}

SetSelectedTrajectoryCommand::~SetSelectedTrajectoryCommand() {}

void SetSelectedTrajectoryCommand::undo() {
    if (m_selectInAllKF && m_keyframe->selection().selectedTrajectoryPtr() != nullptr) {
        std::shared_ptr<Trajectory> cur = m_keyframe->selection().selectedTrajectory();
        while (cur->nextTrajectory() != nullptr) {
            cur = cur->nextTrajectory();
            cur->keyframe()->selection().setSelectedTrajectory(nullptr);
        }
        cur = m_keyframe->selection().selectedTrajectory();
        while (cur->prevTrajectory() != nullptr) {
            cur = cur->prevTrajectory();
            cur->keyframe()->selection().setSelectedTrajectory(nullptr);
        } 
    }

    m_keyframe->selection().setSelectedTrajectory((Trajectory *)nullptr);
}

void SetSelectedTrajectoryCommand::redo() {
    std::shared_ptr<Trajectory> propagationStart = m_trajShPtr;
    bool deselect = false;

    // Deselect case
    if (propagationStart == nullptr && m_traj == nullptr) {
        propagationStart = m_keyframe->selection().selectedTrajectory();
        deselect = true;
    }

    if (m_trajShPtr != nullptr) {
        m_keyframe->selection().setSelectedTrajectory(m_trajShPtr);
    } else {
        m_keyframe->selection().setSelectedTrajectory(m_traj);
    }

    if (m_selectInAllKF && propagationStart != nullptr) {
        std::shared_ptr<Trajectory> cur = propagationStart;
        while (cur->nextTrajectory() != nullptr) {
            cur = cur->nextTrajectory();
            cur->keyframe()->selection().setSelectedTrajectory(deselect ? nullptr : cur);
        }
        cur = propagationStart;
        while (cur->prevTrajectory() != nullptr) {
            cur = cur->prevTrajectory();
            cur->keyframe()->selection().setSelectedTrajectory(deselect ? nullptr : cur);
        }
    }
}

AddTrajectoryConstraintCommand::AddTrajectoryConstraintCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_traj(traj) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Add trajectory constraint");
}

AddTrajectoryConstraintCommand::AddTrajectoryConstraintCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj,
                                                               const std::shared_ptr<Trajectory> &connectedTraj, bool connectWithNext, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_traj(traj), m_connectedTraj(connectedTraj), m_connectWithNext(connectWithNext) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Add trajectory constraint");
}

AddTrajectoryConstraintCommand::~AddTrajectoryConstraintCommand() {}

void AddTrajectoryConstraintCommand::undo() {
    m_keyframe->removeTrajectoryConstraint(m_traj->constraintID());
    m_keyframe->makeInbetweensDirty();
}

void AddTrajectoryConstraintCommand::redo() {
    if (!m_traj->hardConstraint()) {
        m_keyframe->addTrajectoryConstraint(m_traj);
        m_keyframe->makeInbetweensDirty();

        if (m_connectedTraj != nullptr) {
            m_keyframe->connectTrajectories(m_traj, m_connectedTraj, m_connectWithNext);
        }
    }
}

RemoveTrajectoryConstraintCommand::RemoveTrajectoryConstraintCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_traj(traj) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    m_next = m_traj->nextTrajectory();
    m_prev = m_traj->prevTrajectory();
    setText("Remove trajectory constraint");
}

RemoveTrajectoryConstraintCommand::~RemoveTrajectoryConstraintCommand() {}

void RemoveTrajectoryConstraintCommand::undo() {
    if (!m_traj->hardConstraint()) {
        if (m_next != nullptr) m_keyframe->connectTrajectories(m_traj, m_next, true);
        if (m_prev != nullptr) m_keyframe->connectTrajectories(m_traj, m_prev, false);
        m_keyframe->addTrajectoryConstraint(m_traj);
        m_keyframe->makeInbetweensDirty();
    }
}

void RemoveTrajectoryConstraintCommand::redo() {
    m_keyframe->removeTrajectoryConstraint(m_traj->constraintID());
    m_keyframe->makeInbetweensDirty();
}

SyncTrajectoriesCommand::SyncTrajectoriesCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &trajA, const std::shared_ptr<Trajectory> &trajB,
                                                 QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_trajA(trajA), m_trajB(trajB) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Sync trajectories");
}

SyncTrajectoriesCommand::~SyncTrajectoriesCommand() {}

void SyncTrajectoriesCommand::undo() {
    if (m_trajA->nextTrajectory() == m_trajB) {
        m_trajA->setSyncNext(false);
        m_trajB->setSyncPrev(false);
        // restore tangents
        m_trajA->setP2(m_prevPA);
        m_trajB->setP1(m_prevPB);
    } else if (m_trajA->prevTrajectory() == m_trajB) {
        m_trajA->setSyncPrev(false);
        m_trajB->setSyncNext(false);
        // restore tangents
        m_trajA->setP1(m_prevPA);
        m_trajB->setP2(m_prevPB);
    } else {
        qCritical() << "SyncTrajectoriesCommand: trajA and trajB are not connected";
    }
    m_trajA->keyframe()->makeInbetweensDirty();
    m_trajB->keyframe()->makeInbetweensDirty();
}

void SyncTrajectoriesCommand::redo() {
    if (m_trajB != nullptr && m_trajA->nextTrajectory() == m_trajB) {
        m_trajA->setSyncNext(true);
        m_trajB->setSyncPrev(true);
        // save tangents before syncing
        m_prevPA = m_trajA->cubicApprox().getP2();
        m_prevPB = m_trajB->cubicApprox().getP1();
        // update tangents
        Point::VectorType tA = m_trajA->cubicApprox().getP3() - m_trajA->cubicApprox().getP2();
        Point::VectorType tB = m_trajB->cubicApprox().getP1() - m_trajB->cubicApprox().getP0();
        Point::VectorType t = (tA + tB) * 0.5;
        m_trajA->setP2(m_trajA->cubicApprox().getP3() - t);
        m_trajB->setP1(m_trajB->cubicApprox().getP0() + t);
    } else if (m_trajB != nullptr && m_trajA->prevTrajectory() == m_trajB) {
        m_trajA->setSyncPrev(true);
        m_trajB->setSyncNext(true);
        // save tangents before syncing
        m_prevPA = m_trajA->cubicApprox().getP1();
        m_prevPB = m_trajB->cubicApprox().getP2();
        // update tangents
        Point::VectorType tA = m_trajA->cubicApprox().getP1() - m_trajA->cubicApprox().getP0();
        Point::VectorType tB = m_trajB->cubicApprox().getP3() - m_trajB->cubicApprox().getP2();
        Point::VectorType t = (tA + tB) * 0.5;
        m_trajA->setP1(m_trajA->cubicApprox().getP0() + t);
        m_trajB->setP2(m_trajB->cubicApprox().getP3() - t);
    } else {
        qCritical() << "SyncTrajectoriesCommand: trajA and trajB are not connected";
    }
    m_trajA->keyframe()->makeInbetweensDirty();
    m_trajB->keyframe()->makeInbetweensDirty();
}

UnsyncTrajectoriesCommand::UnsyncTrajectoriesCommand(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &trajA, const std::shared_ptr<Trajectory> &trajB,
                                                     QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_trajA(trajA), m_trajB(trajB) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Unsync trajectories");
}

UnsyncTrajectoriesCommand::~UnsyncTrajectoriesCommand() {}

void UnsyncTrajectoriesCommand::undo() {
    if (m_trajA->nextTrajectory() == m_trajB) {
        m_trajA->setSyncNext(true);
        m_trajB->setSyncPrev(true);
        // update tangents
        Point::VectorType tA = m_trajA->cubicApprox().getP3() - m_trajA->cubicApprox().getP2();
        Point::VectorType tB = m_trajB->cubicApprox().getP1() - m_trajB->cubicApprox().getP0();
        Point::VectorType t = (tA + tB) * 0.5;
        m_trajA->setP2(m_trajA->cubicApprox().getP3() - t);
        m_trajB->setP1(m_trajB->cubicApprox().getP0() + t);
    } else if (m_trajA->prevTrajectory() == m_trajB) {
        m_trajA->setSyncPrev(true);
        m_trajB->setSyncNext(true);
        // update tangents
        Point::VectorType tA = m_trajA->cubicApprox().getP1() - m_trajA->cubicApprox().getP0();
        Point::VectorType tB = m_trajB->cubicApprox().getP3() - m_trajB->cubicApprox().getP2();
        Point::VectorType t = (tA + tB) * 0.5;
        m_trajA->setP1(m_trajA->cubicApprox().getP0() + t);
        m_trajB->setP2(m_trajB->cubicApprox().getP3() - t);
    } else {
        qCritical() << "UnsyncTrajectoriesCommand: trajA and trajB are not connected";
    }
}

void UnsyncTrajectoriesCommand::redo() {
    if (m_trajA->nextTrajectory() == m_trajB) {
        m_trajA->setSyncNext(false);
        m_trajB->setSyncPrev(false);
    } else if (m_trajA->prevTrajectory() == m_trajB) {
        m_trajA->setSyncPrev(false);
        m_trajB->setSyncNext(false);
    } else {
        qCritical() << "UnsyncTrajectoriesCommand: trajA and trajB are not connected";
    }
}

MakeTrajectoryC1Command::MakeTrajectoryC1Command(Editor *editor, int layer, int frame, const std::shared_ptr<Trajectory> &traj, QUndoCommand *parent) 
    : QUndoCommand(parent), m_editor(editor), m_traj(traj) {
    m_keyframe = m_editor->layers()->layerAt(layer)->getLastVectorKeyFrameAtFrame(frame, 0);
    setText("Make trajectory C1");
}

MakeTrajectoryC1Command::~MakeTrajectoryC1Command() {}

void MakeTrajectoryC1Command::undo() {
    Trajectory *cur = m_traj.get();
    while (cur != nullptr) {
        cur->resetLocalOffset(); // TODO restore previous offset
        cur->keyframe()->makeInbetweensDirty();
        cur = cur->nextTrajectory().get();
    }
}

void MakeTrajectoryC1Command::redo() {
    Trajectory *cur = m_traj.get();
    while (cur != nullptr) {
        cur->adjustLocalOffsetFromContuinityConstraint(); // TODO: save previous offset
        cur->keyframe()->makeInbetweensDirty();
        cur = cur->nextTrajectory().get();
    }
}

MovePivotCommand::MovePivotCommand(Editor * editor, int layer, int frame, Point::VectorType translation, QUndoCommand * parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_translation(translation){
        setText("Move pivot");
    }

MovePivotCommand::~MovePivotCommand() {}

void MovePivotCommand::undo() {
    Layer * layer = m_editor->layers()->layerAt(m_layer);
    layer->translatePivot(m_frame, -m_translation);
}

void MovePivotCommand::redo() {
    Layer * layer = m_editor->layers()->layerAt(m_layer);
    layer->translatePivot(m_frame, m_translation);
}

PivotTrajectoryCommand::PivotTrajectoryCommand(Editor * editor, int layer, int frame, Bezier2D * newTrajectory, bool breakContinuity, QUndoCommand * parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_newTrajectory(newTrajectory), m_breakContinuity(breakContinuity) {
        float t = m_editor->layers()->layerAt(m_layer)->getFrameTValue(m_frame);
        m_oldTrajectory = new Bezier2D(*m_editor->layers()->layerAt(m_layer)->getPivotCurves()->getBezier(t));
        m_oldBreakContinuity = m_editor->layers()->layerAt(m_layer)->getPivotCurves()->isContinuityBroken(t);
        m_keepPreviousTraj = m_editor->layers()->layerAt(m_layer)->getPivotCurves()->isTrajectoryKeeped(t);
        setText("Set pivot trajectory");
    }

PivotTrajectoryCommand::~PivotTrajectoryCommand() {}

void PivotTrajectoryCommand::undo() {
    Layer * layer = m_editor->layers()->layerAt(m_layer);
    float t = layer->getFrameTValue(m_frame);
    layer->getPivotCurves()->breakContinuity(t, m_oldBreakContinuity);
    layer->getPivotCurves()->replaceBezierCurve(m_oldTrajectory, layer->getFrameTValue(m_frame), !m_keepPreviousTraj);
    layer->getVectorKeyFrameAtFrame(m_frame)->updateTransforms();
    layer->getPrevKey(m_frame)->updateTransforms();
    layer->getNextKey(m_frame)->updateTransforms();
}

void PivotTrajectoryCommand::redo() {
    Layer * layer = m_editor->layers()->layerAt(m_layer);
    float t = layer->getFrameTValue(m_frame);
    layer->getPivotCurves()->breakContinuity(t, m_breakContinuity);
    layer->getPivotCurves()->replaceBezierCurve(m_newTrajectory, layer->getFrameTValue(m_frame));
    layer->getVectorKeyFrameAtFrame(m_frame)->updateTransforms();
    layer->getPrevKey(m_frame)->updateTransforms();
    layer->getNextKey(m_frame)->updateTransforms();
}

PivotScalingCommand::PivotScalingCommand(Editor * editor, int layer, int frame, Point::VectorType scale, QUndoCommand * parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_newScale(scale){
        KeyframedVector * scaling = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame)->scaling();
        scaling->frameChanged(0);
        m_oldScale = scaling->get();
        setText("Set pivot Scaling");
    }

PivotScalingCommand::~PivotScalingCommand() {}

void PivotScalingCommand::undo() {
    VectorKeyFrame * curKey = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame);
    VectorKeyFrame * prevKey = m_editor->layers()->layerAt(m_layer)->getPrevKey(curKey);
    KeyframedVector * scaling = curKey->scaling();
    scaling->set(m_oldScale);
    scaling->addKey("Scaling", 0.0);
    curKey->makeInbetweensDirty();
    for (Group * group : curKey->groups(POST).values()){
        if (group->lattice() != nullptr)
            group->setGridDirty();
    }

    if (prevKey != curKey){
        scaling = prevKey->scaling();
        scaling->set(m_oldScale);
        scaling->addKey("Scaling", 1.0);
        prevKey->makeInbetweensDirty();
        for (Group * group : prevKey->groups(POST).values()){
            if (group->lattice() != nullptr)
                group->setGridDirty();
        }
    }
}

void PivotScalingCommand::redo() {
    VectorKeyFrame * curKey = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame);
    VectorKeyFrame * prevKey = m_editor->layers()->layerAt(m_layer)->getPrevKey(curKey);
    KeyframedVector * scaling = curKey->scaling();
    scaling->set(m_newScale);
    scaling->addKey("Scaling", 0.0);
    curKey->makeInbetweensDirty();
    for (Group * group : curKey->groups(POST).values()){
        if (group->lattice() != nullptr)
            group->setGridDirty();
    }


    if (prevKey != curKey){
        scaling = prevKey->scaling();
        scaling->set(m_newScale);
        scaling->addKey("Scaling", 1.0);
        prevKey->makeInbetweensDirty();
        for (Group * group : prevKey->groups(POST).values()){
            if (group->lattice() != nullptr)
                group->setGridDirty();
        }
    }
}

PivotRotationCommand::PivotRotationCommand(Editor * editor, int layer, int frame, Point::Scalar angle, bool currentT0, bool prevT1, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_angle(angle), m_useCurrentT0(currentT0), m_usePrevT1(prevT1){
        setText("Set Pivot Rotation");
    }

PivotRotationCommand::~PivotRotationCommand() {}

void PivotRotationCommand::undo() {
    VectorKeyFrame * curKey = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame);
    VectorKeyFrame * prevKey = m_editor->layers()->layerAt(m_layer)->getPrevKey(curKey);
    KeyframedReal * rotation = curKey->rotation();
    if (m_useCurrentT0){
        rotation->frameChanged(0);
        rotation->set(-m_angle + rotation->get());
        rotation->addKey("Rotation", 0.0);
        curKey->updateTransforms();
    }

    if (prevKey != curKey && m_usePrevT1){
        rotation = prevKey->rotation();
        rotation->frameChanged(1);
        rotation->set(-m_angle + rotation->get());
        rotation->addKey("Rotation", 1.0);
        prevKey->updateTransforms();
    }
}

void PivotRotationCommand::redo() {
    VectorKeyFrame * curKey = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame);
    VectorKeyFrame * prevKey = m_editor->layers()->layerAt(m_layer)->getPrevKey(curKey);
    KeyframedReal * rotation = curKey->rotation();
    if (m_useCurrentT0){
        rotation->frameChanged(0);
        rotation->set(m_angle + rotation->get());
        rotation->addKey("Rotation", 0.0);
        curKey->updateTransforms();
    }

    if (prevKey != curKey && m_usePrevT1){
        rotation = prevKey->rotation();
        rotation->frameChanged(1);
        rotation->set(m_angle + rotation->get());
        rotation->addKey("Rotation", 1.0);
        prevKey->updateTransforms();
    }
}

PivotAlignTangentCommand::PivotAlignTangentCommand(Editor *editor, int layer, int frame, bool start, AlignTangent alignTangent, QUndoCommand * parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_start(start), m_alignTangent(alignTangent) {
        setText("Set Pivot alignment");
    }

PivotAlignTangentCommand::~PivotAlignTangentCommand() {}

void PivotAlignTangentCommand::undo() {
    VectorKeyFrame * key = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame);
    AlignTangent old = key->getAlignFrameToTangent(m_start);
    key->setAlignFrameToTangent(m_start, m_alignTangent);
    m_alignTangent = old;

    key->makeInbetweensDirty();
    for (Group * group : key->groups(POST).values()){
        if (group->lattice() != nullptr)
            group->setGridDirty();
    }
}

void PivotAlignTangentCommand::redo() {
    VectorKeyFrame * key = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame);
    AlignTangent old = key->getAlignFrameToTangent(m_start);
    key->setAlignFrameToTangent(m_start, m_alignTangent);
    m_alignTangent = old;

    key->makeInbetweensDirty();
    for (Group * group : key->groups(POST).values()){
        if (group->lattice() != nullptr)
            group->setGridDirty();
    }
}

PivotTranslationExtractionCommand::PivotTranslationExtractionCommand(Editor * editor, int layer, QVector<VectorKeyFrame * > keys, QUndoCommand * parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_keys(keys) {
        setText("Pivot Translation extraction");
    }

PivotTranslationExtractionCommand::~PivotTranslationExtractionCommand() {}

void PivotTranslationExtractionCommand::undo() {
    m_editor->layers()->layerAt(m_layer)->insertPivotTranslation(m_keys);
}

void PivotTranslationExtractionCommand::redo() {
    m_editor->layers()->layerAt(m_layer)->extractPivotTranslation(m_keys);    
}

PivotRotationExtractionCommand::PivotRotationExtractionCommand(Editor * editor, int layer, QVector<VectorKeyFrame *> keys, QVector<float> angles, QUndoCommand * parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_keys(keys), m_angles(angles) {
        setText("Pivot Rotation extraction");
    }


PivotRotationExtractionCommand::~PivotRotationExtractionCommand() {}

void PivotRotationExtractionCommand::undo() {
    m_editor->layers()->layerAt(m_layer)->insertPivotRotation(m_keys);
}

void PivotRotationExtractionCommand::redo() {
    m_editor->layers()->layerAt(m_layer)->extractPivotRotation(m_keys, m_angles);
}

LayerTranslationCommand::LayerTranslationCommand(Editor * editor, int layer, int frame, Point::VectorType translation, QUndoCommand *parent)
    : QUndoCommand(parent), m_editor(editor), m_layer(layer), m_frame(frame), m_translation(translation){
        setText("Set pivot translation");
    }

LayerTranslationCommand::~LayerTranslationCommand() {}

void LayerTranslationCommand::undo() {
    m_editor->layers()->layerAt(m_layer)->addVectorKeyFrameTranslation(m_frame, - m_translation);
}

void LayerTranslationCommand::redo() {
    m_editor->layers()->layerAt(m_layer)->addVectorKeyFrameTranslation(m_frame, m_translation);
}

// *******************************

AddOrderPartial::AddOrderPartial(Editor * editor, int layer, int frame, const OrderPartial &orderPartial, const OrderPartial &prevOrderPartial, QUndoCommand *parent) 
    : QUndoCommand(parent),
      m_editor(editor),
      m_layer(layer),
      m_frame(frame),
      m_orderPartial(orderPartial),
      m_prevOrderPartial(prevOrderPartial)
{
    setText("Add group order partial");
}

AddOrderPartial::~AddOrderPartial() {

}

void AddOrderPartial::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    if (m_prevOrderPartial.t() == m_orderPartial.t()) {
        keyframe->orderPartials().insertPartial(m_prevOrderPartial);
    } else {
        keyframe->orderPartials().removePartial(m_orderPartial.t());
    }
    m_editor->fixedScene()->updateKeyChart(keyframe);
}

void AddOrderPartial::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    keyframe->orderPartials().insertPartial(m_orderPartial);
    keyframe->orderPartials().saveState();
}

// *******************************

RemoveOrderPartial::RemoveOrderPartial(Editor * editor, int layer, int frame, double t, const OrderPartial& prevOrderPartial, QUndoCommand *parent)
 : QUndoCommand(parent),
   m_editor(editor),
   m_layer(layer),
   m_frame(frame),
   m_t(t),
   m_prevOrderPartial(prevOrderPartial)
{
    setText("Remove group order partial");
}

RemoveOrderPartial::~RemoveOrderPartial() {
    
}

void RemoveOrderPartial::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    keyframe->orderPartials().insertPartial(m_prevOrderPartial);
    m_editor->fixedScene()->updateKeyChart(keyframe);
}

void RemoveOrderPartial::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    keyframe->orderPartials().removePartial(m_t);
}

// *******************************

MoveOrderPartial::MoveOrderPartial(Editor * editor, int layer, int frame, double newT, double prevT, QUndoCommand *parent)
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_t(newT),
    m_prevT(prevT) 
{
    setText("Move order change partial");
}

MoveOrderPartial::~MoveOrderPartial() {
    
}

void MoveOrderPartial::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    if (!keyframe->orderPartials().exists(m_t)) {
        qCritical() << "Error in MoveOrderPartial::undo: no partial exists at t=" << m_t;
         return;
    }
    keyframe->orderPartials().movePartial(m_t, m_prevT);
    m_editor->fixedScene()->updateKeyChart(keyframe);
}

void MoveOrderPartial::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    if (!keyframe->orderPartials().exists(m_prevT)) return;
    keyframe->orderPartials().movePartial(m_prevT, m_t);
}

// *******************************

SyncOrderPartialCommand::SyncOrderPartialCommand(Editor * editor, int layer, int frame, const Partials<OrderPartial> &prevOrder, QUndoCommand *parent) 
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_prevOrder(prevOrder)
{
    setText("Sync order partial");
}

SyncOrderPartialCommand::~SyncOrderPartialCommand() {
    
}

void SyncOrderPartialCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    keyframe->orderPartials().set(m_prevOrder);
}

void SyncOrderPartialCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    keyframe->orderPartials().syncWithFrames(layer->stride(keyframe->keyframeNumber()));
}

// *******************************

SetOrderPartialsCommand::SetOrderPartialsCommand(Editor * editor, int layer, int frame, const Partials<OrderPartial> &prevPartials, QUndoCommand *parent) 
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_newPartials(nullptr, OrderPartial(nullptr, 0.0)),
    m_prevPartials(prevPartials)
{
    setText("Set order partials");
    Layer *lay = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = lay->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    m_newPartials = keyframe->orderPartials();
}

SetOrderPartialsCommand::~SetOrderPartialsCommand() {
    
}

void SetOrderPartialsCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    keyframe->orderPartials().set(m_prevPartials);
}

void SetOrderPartialsCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    keyframe->orderPartials().set(m_newPartials);
    keyframe->orderPartials().saveState();
}

// *******************************

AddDrawingPartial::AddDrawingPartial(Editor * editor, int layer, int frame, int groupId, const DrawingPartial&drawingPartial, const DrawingPartial&prevDrawingPartial, QUndoCommand *parent) 
    : QUndoCommand(parent),
      m_editor(editor),
      m_layer(layer),
      m_frame(frame),
      m_groupId(groupId),
      m_drawingPartial(drawingPartial),
      m_prevDrawingPartial(prevDrawingPartial)
{
    setText("Add drawing order partial");
}

AddDrawingPartial::~AddDrawingPartial() {

}

void AddDrawingPartial::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    Group *group = keyframe->postGroups().fromId(m_groupId);
    if (m_prevDrawingPartial.t() == m_drawingPartial.t()) {
        group->drawingPartials().insertPartial(m_prevDrawingPartial);
    } else {
        group->drawingPartials().removePartial(m_drawingPartial.t());
    }
    m_editor->fixedScene()->updateKeyChart(keyframe);
}

void AddDrawingPartial::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    Group *group = keyframe->postGroups().fromId(m_groupId);
    group->drawingPartials().insertPartial(m_drawingPartial);
    if (m_drawingPartial.strokes().empty()) group->drawingPartials().lastPartialAt(m_drawingPartial.t()).strokes() = m_prevDrawingPartial.strokes();
}

// *******************************

RemoveDrawingPartial::RemoveDrawingPartial(Editor * editor, int layer, int frame, int groupId, double t, const DrawingPartial& prevDrawingPartial, QUndoCommand *parent)
 : QUndoCommand(parent),
   m_editor(editor),
   m_layer(layer),
   m_frame(frame),
   m_groupId(groupId),
   m_t(t),
   m_prevDrawingPartial(prevDrawingPartial)
{
    setText("Remove group drawing partial");
}

RemoveDrawingPartial::~RemoveDrawingPartial() {
    
}

void RemoveDrawingPartial::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    Group *group = keyframe->postGroups().fromId(m_groupId);
    group->drawingPartials().insertPartial(m_prevDrawingPartial);
    m_editor->fixedScene()->updateKeyChart(keyframe);
}

void RemoveDrawingPartial::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    Group *group = keyframe->postGroups().fromId(m_groupId);
    group->drawingPartials().removePartial(m_t);
}

// *******************************

MoveDrawingPartial::MoveDrawingPartial(Editor * editor, int layer, int frame, int groupId, double newT, double prevT, QUndoCommand *parent)
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_groupId(groupId),
    m_t(newT),
    m_prevT(prevT) 
{
    setText("Move drawing change partial");
}

MoveDrawingPartial::~MoveDrawingPartial() {
    
}

void MoveDrawingPartial::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    Group *group = keyframe->postGroups().fromId(m_groupId);
    if (!group->drawingPartials().exists(m_t)) {
        qCritical() << "Error in MoveOrderPartial::undo: no partial exists at t=" << m_t;
         return;
    }
    group->drawingPartials().movePartial(m_t, m_prevT);
    m_editor->fixedScene()->updateKeyChart(keyframe);
}

void MoveDrawingPartial::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    Group *group = keyframe->postGroups().fromId(m_groupId);
    if (!group->drawingPartials().exists(m_prevT)) return;
    group->drawingPartials().movePartial(m_prevT, m_t);
}

// *******************************

SyncDrawingPartialCommand::SyncDrawingPartialCommand(Editor * editor, int layer, int frame, int groupId, const Partials<DrawingPartial> &prevDrawing, QUndoCommand *parent) 
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_groupId(groupId),
    m_prevDrawing(prevDrawing)
{
    setText("Sync drawing partial");
}

SyncDrawingPartialCommand::~SyncDrawingPartialCommand() {
    
}

void SyncDrawingPartialCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    Group *group = keyframe->postGroups().fromId(m_groupId);
    group->drawingPartials().set(m_prevDrawing);
}

void SyncDrawingPartialCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0); 
    Group *group = keyframe->postGroups().fromId(m_groupId);
    group->drawingPartials().syncWithFrames(layer->stride(keyframe->keyframeNumber()));
}

// *******************************

ComputeVisibilityCommand::ComputeVisibilityCommand(Editor * editor, int layer, int frame, QUndoCommand *parent) 
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_savedKeyframe(nullptr)
{
    setText("Compute visibility");
}

ComputeVisibilityCommand::~ComputeVisibilityCommand() {

}

void ComputeVisibilityCommand::undo() {
    if (m_savedKeyframe != nullptr) {
        Layer *layer = m_editor->layers()->layerAt(m_layer);
        VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
        layer->insertKeyFrame(keyframe->keyframeNumber(), m_savedKeyframe);
        m_savedKeyframe->makeInbetweensDirty();
    }
}

void ComputeVisibilityCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);

    if (keyframe->nextKeyframe() == nullptr){
        m_savedKeyframe = nullptr;
        return;
    }

    m_savedKeyframe = keyframe->copy();

    // Find disappearances
    StopWatch s1("Find disappearances");
    m_editor->visibility()->init(keyframe, keyframe->nextKeyframe());
    m_editor->visibility()->computePointsFirstPass(keyframe, keyframe->nextKeyframe());
    std::vector<Point *> sources;
    m_editor->visibility()->findSources(keyframe, sources);
    m_editor->visibility()->assignVisibilityThreshold(keyframe, sources);
    s1.stop();

    // Find appearances
    StopWatch s2("Find appearances");
    m_editor->visibility()->initAppearance(keyframe, keyframe->nextKeyframe());
    m_editor->visibility()->computePointsFirstPassAppearance(keyframe, keyframe->nextKeyframe());
    std::vector<Point::VectorType> sourcesAppearance;
    std::vector<int> sourcesGroupsId;
    m_editor->visibility()->findSourcesAppearance(keyframe->nextKeyframe(), sourcesAppearance);
    m_editor->visibility()->addGroupsOrBake(keyframe, keyframe->nextKeyframe(), sourcesAppearance, sourcesGroupsId);
    m_editor->visibility()->assignVisibilityThresholdAppearance(keyframe, sourcesAppearance, sourcesGroupsId);
    s2.stop();

    keyframe->makeInbetweensDirty();
}

// *******************************

SetVisibilityCommand::SetVisibilityCommand(Editor *editor, int layer, int frame, const QHash<unsigned int, double> &prevVisibility, QUndoCommand *parent)
  : QUndoCommand(parent),
    m_editor(editor),
    m_layer(layer),
    m_frame(frame),
    m_prevVisibility(prevVisibility)
{
    setText("Set visibility");
    Layer *lay = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = lay->getLastVectorKeyFrameAtFrame(m_frame, 0);
    m_newVisibility = keyframe->visibility();
}

SetVisibilityCommand::~SetVisibilityCommand() {

}

void SetVisibilityCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    keyframe->visibility() = m_prevVisibility;
    keyframe->makeInbetweensDirty();
}

void SetVisibilityCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_frame, 0);
    keyframe->visibility() = m_newVisibility;
    keyframe->makeInbetweensDirty();
}
