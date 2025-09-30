/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
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


static dkBool k_strokeMode("RegistrationLasso->Stroke Mode", true);
static dkBool k_groupMode("RegistrationLasso->Group Mode", false);

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
    m_editor->tabletCanvas()->updateCurrentFrame();
}

void RegistrationLassoTool::released(const EventInfo& info) {
    m_lasso << info.firstPos;

    VectorKeyFrame *key;
    Layer * layer = m_editor->layers()->currentLayer();

    if (info.modifiers & Qt::ControlModifier) {
        key = info.key;
    } else {
        key = info.key->nextKeyframe(); // TODO check if next keyframe exists 
        int currentFrame = layer->getVectorKeyFramePosition(info.key);
        if (layer->isVectorKeyFrameSelected(info.key) && layer->getLastKeyFrameSelected() == currentFrame){
            int frame = layer->getFirstKeyFrameSelected();
            key = layer->getVectorKeyFrameAtFrame(frame);
        }
    }

    if (key == nullptr) 
        return;

    auto strokePredicate = [&key](const Stroke *stroke) {
        return !key->preGroups().containsStroke(stroke->id());
    };

    StrokeIntervals selection;

    if (k_groupMode) {
        std::vector<int> selectedGroups;
        auto checkGroup = [&](Group *group) {
            for (auto intervals = group->strokes().constBegin(); intervals != group->strokes().constEnd(); ++intervals) {
                Stroke *stroke = key->stroke(intervals.key());
                for (const Interval &interval : intervals.value()) {
                    for (int i = interval.from(); i <= interval.to(); ++i) {
                        if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        for (Group *group : key->postGroups()) {
            if (checkGroup(group)) {
                for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
                    selection.insert(it.key(), it.value());
                }
            }
        }
    } else if (k_strokeMode) {
        // select all strokes intersecting the lasso
        m_editor->selection()->selectStrokes(key, 0, [&](const StrokePtr &stroke) {
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

void RegistrationLassoTool::drawGL(VectorKeyFrame *key, qreal alpha) {
    // Draw the next keyframe is it exists 
    // TODO: deactivate onion skin?
    Layer * layer = m_editor->layers()->currentLayer();
    int currentFrame = layer->getVectorKeyFramePosition(key);

    VectorKeyFrame *next = key->nextKeyframe();
    if (layer->isVectorKeyFrameSelected(key) && layer->getLastKeyFrameSelected() == currentFrame){
        int frame = layer->getFirstKeyFrameSelected();
        next = layer->getVectorKeyFrameAtFrame(frame);
    }
    if (next != nullptr) {
        int frame = layer->getVectorKeyFramePosition(next);
        m_editor->tabletCanvas()->drawKeyFrame(next, frame, 0, layer->stride(frame), m_editor->forwardColor(), 0.75, m_editor->tintFactor());
    }
    // TODO: visualize registration target
}
