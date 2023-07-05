/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "tools/registrationlassotool.h"
#include "dialsandknobs.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "registrationmanager.h"
#include "selectionmanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "canvasscenemanager.h"

static dkBool k_strokeMode("RegistrationLasso->Stroke Mode", true);
extern dkBool k_drawGL;

RegistrationLassoTool::RegistrationLassoTool(QObject *parent, Editor *editor) : LassoTool(parent, editor) {
    m_toolTips = QString("Left-click to select the registration target");
}

Tool::ToolType RegistrationLassoTool::toolType() const {
    return Tool::RegistrationLasso;
}

void RegistrationLassoTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
    }
    m_editor->tabletCanvas()->updateCurrentFrame();
}

void RegistrationLassoTool::released(const EventInfo& info) {
    m_lasso << info.firstPos;

    VectorKeyFrame *key;

    if (info.modifiers & Qt::ControlModifier) {
        key = info.key;
    } else {
        key = info.key->nextKeyframe(); // TODO check if next keyframe exists 
    }

    if (key == nullptr) 
        return;

    auto strokePredicate = [&key](const Stroke *stroke) {
        return !key->preGroups().containsStroke(stroke->id());
    };

    StrokeIntervals selection;

    if (k_strokeMode) {
        // select all strokes intersecting the lasso
        m_editor->selection()->selectStrokes(key, [&](const StrokePtr &stroke) {
            if (key->preGroups().containsStroke(stroke->id())) return false;
            for (int i = 0; i < stroke->size(); ++i) {
                if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                    return true;
                }
            }
            return false;
        }, selection);
    } else {
        // select all stroke *segments* intersecting the lasso
        m_editor->selection()->selectStrokeSegments(key, m_lasso, strokePredicate, [](Point *point) { return true; }, selection);
    }

    if (!selection.isEmpty()) {
        m_editor->registration()->setRegistrationTarget(key, selection);
    } else {
        m_editor->registration()->clearRegistrationTarget();
    }

    m_lasso = QPolygonF();
    m_lassoSelectedPoints.clear();
}

void RegistrationLassoTool::doublepressed(const EventInfo& info) {
    m_editor->registration()->clearRegistrationTarget();
}

void RegistrationLassoTool::draw(QPainter &painter, VectorKeyFrame *key) {
    // Draw the next keyframe is it exists 
    // TODO: deactivate onion skin?
    VectorKeyFrame *next = key->nextKeyframe(); 
    if (next != nullptr) {
        Layer *layer = m_editor->layers()->currentLayer();
        int frame = layer->getVectorKeyFramePosition(next);
        m_editor->tabletCanvas()->drawKeyFrame(painter, next, frame, 0, layer->stride(frame), m_editor->forwardColor(), 0.75, m_editor->tintFactor(), k_drawGL);
    }
    // TODO: visualize registration target
}
