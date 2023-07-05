/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "lassotool.h"

#include "layermanager.h"
#include "playbackmanager.h"
#include "canvascommands.h"
#include "keycommands.h"
#include "strokeinterval.h"
#include "canvassceneitems.h"
#include "selectionmanager.h"
#include "dialsandknobs.h"

static dkBool k_strokeMode("Lasso->Stroke Mode", true);
static dkBool k_onlyMainGroup("Lasso->Only default group", false);

LassoTool::LassoTool(QObject *parent, Editor *editor) : PickTool(parent, editor) { 
    m_toolTips = QString("Left-click to create a group, hold Shift to add a strokes to the selected group.");
}


Tool::ToolType LassoTool::toolType() const {
    return Tool::Lasso;
}

void LassoTool::pressed(const EventInfo& info) {
    m_lasso = QPolygonF();
    m_lasso << info.pos;
}

void LassoTool::moved(const EventInfo& info) {
    m_lasso << info.pos;
}

void LassoTool::released(const EventInfo& info) {
    m_lasso << info.firstPos;

    GroupType type = QApplication::keyboardModifiers() & Qt::ControlModifier ? PRE : POST;
    bool additiveMode = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    int layer = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    
    // check if we can create a correspondence
    VectorKeyFrame *prev;
    int prevFrame;
    if (type == PRE) {
        Layer *layer = m_editor->layers()->currentLayer();
        prevFrame = layer->getPreviousFrameNumber(currentFrame, true);
        prev = layer->getVectorKeyFrameAtFrame(prevFrame);
        if (prev == nullptr || prev->selectedGroup(POST) == nullptr) {
            m_lasso = QPolygonF();
            m_lassoSelectedPoints.clear();
            return;
        }
    }

    // check if there is a group selected if we're in additive mode
    if (additiveMode && info.key->selectedGroup(type) == nullptr) {
        return;
    }

    // identify intervals of a stroke that are in the lasso selection 
    StrokeIntervals selection;
    makeSelection(info, type, prev, selection);

    if (!selection.isEmpty()) {
        m_editor->undoStack()->beginMacro("Lasso");

        // clone strokes that be will only referenced by the new pre groups
        if (type == PRE) {
            cloneSelection(info, selection);
        }
        
        // create a new group by default (if we're not in additiveMode)
        Group *newGroup;
        if (!additiveMode) {
            m_editor->undoStack()->push(new AddGroupCommand(m_editor, layer, currentFrame, type)); 
            newGroup = type == POST ? info.key->postGroups().lastGroup() : info.key->preGroups().lastGroup();
        } else {
            newGroup = info.key->selectedGroup(type);
        }
        // add the selected strokes to this group
        m_editor->undoStack()->push(new SetGroupCommand(m_editor, layer, currentFrame, selection, newGroup->id(), type));
        // set this new group as selected
        m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layer, currentFrame, newGroup->id(), type));
        // if we're creating a "pre" group we create a correspondence with the previous selected "post" group if there is any
        if (type == PRE) {
            m_editor->undoStack()->push(new SetCorrespondenceCommand(m_editor, layer, prevFrame, currentFrame, prev->selectedGroup(POST)->id(), newGroup->id()));
        }
        m_editor->undoStack()->endMacro();
    } else {
        m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layer, currentFrame, -1, type));
        if (type == PRE) m_editor->undoStack()->push(new RemoveCorrespondenceCommand(m_editor, layer, prevFrame, prev->selectedGroup(POST)->id()));
    }

    m_lasso = QPolygonF();
    m_lassoSelectedPoints.clear();
}

void LassoTool::makeSelection(const EventInfo& info, GroupType type, VectorKeyFrame *prev, StrokeIntervals &selection) {
    // In all the following selection we do not consider strokes that are reference by a pre group
    auto strokePredicate = [&info](const Stroke *stroke) {
        return true;
    };

    // Complete stroke selection
    if (k_strokeMode) {
        // select strokes that have at least one point in the lasso
        m_editor->selection()->selectStrokes(info.key, [&](const StrokePtr &stroke) {
            if (info.key->preGroups().containsStroke(stroke->id())) return false;
            for (int i = 0; i < stroke->size(); ++i) {
                if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                    return true;
                }
            }
            return false;
        }, selection);

        // remove segments not in the previous KF's selected group lattice 
        if (type == PRE) {
            StrokeIntervals copy = selection;
            m_editor->selection()->selectStrokeSegments(info.key, copy, strokePredicate, [&prev](Point *point) {
                QuadPtr q; int k;
                return prev->selectedGroup()->lattice()->contains(point->pos(), TARGET_POS, q, k);
            }, selection);
        }
        return;
    }

    // Stroke segments selection
    if (type == PRE) {
        m_editor->selection()->selectStrokeSegments(info.key, m_lasso, strokePredicate, [&prev](Point *point) {
            QuadPtr q; int k;
            return prev->selectedGroup()->lattice()->contains(point->pos(), TARGET_POS, q, k);
        }, selection);
    } else {
        m_editor->selection()->selectStrokeSegments(info.key, m_lasso, strokePredicate, [](Point *point) { return true; }, selection);
    }
}

void LassoTool::cloneSelection(const EventInfo& info, StrokeIntervals &selection) {
    int layer = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();

    StrokeIntervals newSelection;

    // clone all segments as new strokes, these new strokes are added to the keyframe but not to a group
    for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
        Stroke *stroke = info.key->stroke(it.key());
        for (const Interval &interval : it.value()) {
            unsigned int newId = info.key->pullMaxStrokeIdx();
            m_editor->undoStack()->push(new DrawCommand(m_editor, layer, currentFrame, std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to()), Group::ERROR_ID, false));
            newSelection[newId].append(Interval(0, interval.to() - interval.from()));
        }
    }
    
    selection.swap(newSelection);
}