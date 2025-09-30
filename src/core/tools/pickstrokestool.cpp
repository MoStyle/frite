/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pickstrokestool.h"

#include "layermanager.h"
#include "playbackmanager.h"
#include "canvascommands.h"
#include "keycommands.h"
#include "strokeinterval.h"
#include "selectionmanager.h"
#include "gridmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"

static const QStringList ONION_DIRECTION({"Forward", "Backward", "Both"});
static dkStringList k_onionDirection("CopyStrokes->Onion skin", ONION_DIRECTION);
static dkBool k_groupMode("CopyStrokes->Select group", false);

PickStrokesTool::PickStrokesTool(QObject *parent, Editor *editor) : PickTool(parent, editor) { 
    m_toolTips = QString("Left-click to select strokes from the onion skin and copy them into the currently selected group | Ctrl+Left-click to copy an entire group");

    connect(&k_onionDirection, &dkStringList::indexChanged, this, [&] { setOnionDirection(); });
}

Tool::ToolType PickStrokesTool::toolType() const {
    return Tool::CopyStrokes;
}

void PickStrokesTool::toggled(bool on) {
    Tool::toggled(on);

    // Force onion display of the next KF
    if (on) {
        m_savedEqMode = m_editor->getEqMode();
        m_savedEqValues = m_editor->getEqValues();
        m_editor->setEqMode(EqualizedMode::KEYS);
        setOnionDirection();
        m_savedLayer = m_editor->layers()->currentLayerSharedPtr();
        m_savedLayerOnionSkinStatus = m_savedLayer->showOnion();
        if (!m_savedLayerOnionSkinStatus) m_savedLayer->switchShowOnion();
    } else {
        m_editor->setEqMode(m_savedEqMode);
        m_editor->setEqValues(m_savedEqValues);
        if (m_savedLayer != nullptr && m_savedLayerOnionSkinStatus != m_savedLayer->showOnion()) {
            m_savedLayer->switchShowOnion();
        }
    }
    m_editor->tabletCanvas()->update();
} 

void PickStrokesTool::pressed(const EventInfo& info) {
    m_lasso = QPolygonF();
    m_lasso << info.pos;
}

void PickStrokesTool::moved(const EventInfo& info) {
    m_lasso << info.pos;
}

void PickStrokesTool::released(const EventInfo& info) {
    m_lasso << info.firstPos;

    bool addToRef = QApplication::keyboardModifiers() & Qt::ControlModifier;
    int layer = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();

    Layer *lay = m_editor->layers()->currentLayer();
    VectorKeyFrame *next = info.key->nextKeyframe();
    VectorKeyFrame *prev = info.key->prevKeyframe();

    qDebug() << "prev: " << prev << " | cur: " << info.key; 
    Group *group = info.key->selectedGroup();
    if (k_onionDirection.index() == 0 && next == info.key ) {
        m_lasso = QPolygonF();
        m_lassoSelectedPoints.clear();
        m_editor->tabletCanvas()->update();
        return;
    } else if (k_onionDirection.index() == 1 && prev == info.key) {
        m_lasso = QPolygonF();
        m_lassoSelectedPoints.clear();
        m_editor->tabletCanvas()->update();
        return;
    }

    // Select strokes from the the next KF
    StrokeIntervals selectionForward, selectionBackward;
    if (k_groupMode || info.modifiers & Qt::ControlModifier) {
        if ((k_onionDirection.index() == 0 || k_onionDirection.index() == 2) && next->keyframeNumber() != lay->getMaxKeyFramePosition()) {
            std::vector<int> selectedGroups;
            m_editor->selection()->selectGroups(next, 0.0, 0, (unsigned int)POST, m_lasso, false, selectedGroups);
            for (int groupId : selectedGroups) {
                selectionForward.insert(next->postGroups().fromId(groupId)->strokes());
            }
        }
        if ((k_onionDirection.index() == 1 || k_onionDirection.index() == 2) && prev != info.key) {
            std::vector<int> selectedGroups;
            m_editor->selection()->selectGroups(prev, 0.0, 0, (unsigned int)POST, m_lasso, false, selectedGroups);
            for (int groupId : selectedGroups) {
                selectionBackward.insert(prev->postGroups().fromId(groupId)->strokes());
            }            
        }
    } else {
        if ((k_onionDirection.index() == 0 || k_onionDirection.index() == 2) && next->keyframeNumber() != lay->getMaxKeyFramePosition()) {
            m_editor->selection()->selectStrokes(next, 0, [&](const StrokePtr &stroke) {
                if (next->preGroups().containsStroke(stroke->id())) return false;
                for (int i = 0; i < stroke->size(); ++i) {
                    if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                        qDebug() << "stroke " << stroke->id();
                        return true;
                    }
                }
                return false;
            }, selectionForward);
        }
        if ((k_onionDirection.index() == 1 || k_onionDirection.index() == 2) && prev != info.key) {
            m_editor->selection()->selectStrokes(prev, 0, [&](const StrokePtr &stroke) {
                if (prev->preGroups().containsStroke(stroke->id())) return false;
                for (int i = 0; i < stroke->size(); ++i) {
                    if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                        return true;
                    }
                }
                return false;
            }, selectionBackward);
        }
    }

    // Check if they intersect the TARGET_POS of the selected lattice, if so extend the grid to add the selected strokes
    if (group != nullptr) {
        bool strokeAdded = false;
        m_editor->undoStack()->beginMacro("Copy strokes");
        for (auto it = selectionForward.constBegin(); it != selectionForward.constEnd(); ++it) {
            Stroke *stroke = next->stroke(it.key());
            // TODO allow for a stroke to be added even it does not intersects the grid if at least one stroke in the selection intersects it
            auto [startIdx, endIdx] = m_editor->grid()->expandTargetGridToFitStroke(group->lattice(), stroke, false);
            if (startIdx != -1 || endIdx != -1) strokeAdded = true;
            unsigned int newId = info.key->pullMaxStrokeIdx();
            StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, startIdx, endIdx);
            m_editor->undoStack()->push(new DrawCommand(m_editor, layer, currentFrame, copiedStroke, Group::ERROR_ID, false));
            Stroke *newStroke = info.key->stroke(newId);
            std::vector<QuadPtr> newQuads;
            Interval interval(0, newStroke->size() - 1);
            group->addStroke(newId);
            m_editor->grid()->bakeStrokeInGrid(group->lattice(), newStroke, 0, newStroke->size() - 1, TARGET_POS, true);
            group->lattice()->enforceManifoldness(newStroke, interval, newQuads, true);
            group->lattice()->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0 && !q->isPivot()); });
            group->lattice()->bakeForwardUV(newStroke, interval, group->uvs(), TARGET_POS);
            // TODO move stroke pos to ref pos (see canvascommands add stroke)
            // TODO make sure the grid is still manifold?
        }
        for (auto it = selectionBackward.constBegin(); it != selectionBackward.constEnd(); ++it) {
            Stroke *stroke = prev->stroke(it.key());
            // TODO allow for a stroke to be added even it does not intersects the grid if at least one stroke in the selection intersects it
            auto [startIdx, endIdx] = m_editor->grid()->expandTargetGridToFitStroke(group->lattice(), stroke, false);
            if (startIdx != -1 || endIdx != -1) strokeAdded = true;
            unsigned int newId = info.key->pullMaxStrokeIdx();
            StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, startIdx, endIdx);
            m_editor->undoStack()->push(new DrawCommand(m_editor, layer, currentFrame, copiedStroke, Group::ERROR_ID, false));
            Stroke *newStroke = info.key->stroke(newId);
            std::vector<QuadPtr> newQuads;
            Interval interval(0, newStroke->size() - 1);
            group->addStroke(newId);
            m_editor->grid()->bakeStrokeInGrid(group->lattice(), newStroke, 0, newStroke->size() - 1, TARGET_POS, true);
            group->lattice()->enforceManifoldness(newStroke, interval, newQuads, true);
            group->lattice()->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0 && !q->isPivot()); });
            group->lattice()->bakeForwardUV(newStroke, interval, group->uvs(), TARGET_POS);
            // TODO move stroke pos to ref pos (see canvascommands add stroke)
            // TODO make sure the grid is still manifold?
        }
        m_editor->undoStack()->endMacro();
    } else {
        m_editor->undoStack()->beginMacro("Copy strokes");
        m_editor->undoStack()->push(new AddGroupCommand(m_editor, layer, currentFrame));
        Group *newGroup = info.key->postGroups().lastGroup();
        for (auto it = selectionForward.constBegin(); it != selectionForward.constEnd(); ++it) {
            Stroke *stroke = next->stroke(it.key());
            for (const Interval &interval : it.value()) {
                unsigned int newId = info.key->pullMaxStrokeIdx();
                StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to());
                m_editor->undoStack()->push(new DrawCommand(m_editor, layer, currentFrame, copiedStroke, newGroup->id(), false));
            }
        }
        for (auto it = selectionBackward.constBegin(); it != selectionBackward.constEnd(); ++it) {
            Stroke *stroke = prev->stroke(it.key());
            for (const Interval &interval : it.value()) {
                unsigned int newId = info.key->pullMaxStrokeIdx();
                StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to());
                m_editor->undoStack()->push(new DrawCommand(m_editor, layer, currentFrame, copiedStroke, newGroup->id(), false));
            }
        }
        m_editor->undoStack()->endMacro();
    }

    m_lasso = QPolygonF();
    m_lassoSelectedPoints.clear();
    m_editor->tabletCanvas()->update();
}

void PickStrokesTool::setOnionDirection() {
    EqualizerValues eqValues = m_savedEqValues;
    for (int i = -eqValues.maxDistance; i <= eqValues.maxDistance; i++) {
        eqValues.state[i] = false;
        eqValues.value[i] = 0;
    }
    eqValues.state[0] = true;
    eqValues.value[0] = 100;

    if (k_onionDirection.index() == 0 || k_onionDirection.index() == 2) {
        eqValues.state[1] = true;
        eqValues.value[1] = 100;
    }
    if (k_onionDirection.index() == 1 || k_onionDirection.index() == 2) {
        eqValues.state[-1] = true;
        eqValues.value[-1] = 100;
    }

    m_editor->setEqValues(eqValues);
}
