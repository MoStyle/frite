/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "erasertool.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "colormanager.h"
#include "dialsandknobs.h"
#include "canvascommands.h"
#include "keycommands.h"
#include "tabletcanvas.h"
#include "qteigen.h"

extern dkSlider k_deformRange;
static dkBool k_eraseFromSelection("Eraser->Erase only from selected groups", false);

EraserTool::EraserTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click to erase strokes");
    m_contextMenuAllowed = false;
    m_pressed = false;
}

Tool::ToolType EraserTool::toolType() const {
    return Tool::Eraser;
}

QCursor EraserTool::makeCursor(float scaling) const {
    return QCursor(Qt::BlankCursor);
}

void EraserTool::toggled(bool on) {
    Tool::toggled(on);
    m_editor->tabletCanvas()->setMouseTracking(on);
    m_editor->tabletCanvas()->setTabletTracking(on);
    m_editor->tabletCanvas()->fixedCanvasView()->setAttribute(Qt::WA_TransparentForMouseEvents, on);
}

void EraserTool::pressed(const EventInfo& info) {
    Layer *layer = m_editor->layers()->currentLayer();
    m_frame = m_editor->playback()->currentFrame();
    m_prevFrame = layer->getLastKeyFramePosition(m_editor->playback()->currentFrame());
    m_keyframe = layer->getVectorKeyFrameAtFrame(m_prevFrame);

    if (info.mouseButton & Qt::LeftButton) {
        m_savedVisibility = info.key->visibility();
        (info.modifiers & Qt::ControlModifier) ? erase(info) : eraseSegments(info);
        m_pressed = true;
    }
}

void EraserTool::moved(const EventInfo& info)  {
    if (!m_pressed) return;
    (info.modifiers & Qt::ControlModifier) ? erase(info) : eraseSegments(info);
}

void EraserTool::released(const EventInfo& info) {
    if (!m_pressed) return;
    if (info.modifiers & Qt::ControlModifier) {
        erase(info);
    } else {
        eraseSegments(info);
        m_editor->undoStack()->push(new SetVisibilityCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedVisibility));
    }
    m_pressed = false;
}

void EraserTool::wheel(const WheelEventInfo& info) {
    if (info.modifiers & Qt::ShiftModifier) {
        if (info.delta > 0) k_deformRange.setValue(k_deformRange + 4);
        else                k_deformRange.setValue(k_deformRange - 4);
        m_editor->tabletCanvas()->updateCursor();
    }
}

void EraserTool::drawGL(VectorKeyFrame *key, qreal alpha) {
    m_editor->tabletCanvas()->drawCircleCursor(QVector2D(0.0, 0.0));
}

void EraserTool::erase(const EventInfo& info) {
    int layerIdx = m_editor->layers()->currentLayerIndex();
    float sizeSq = k_deformRange * k_deformRange;
    Point::VectorType pos(info.pos.x(), info.pos.y());
    std::vector<int> strokes;

    // Find all strokes intersecting the brush footprint
    const Inbetween &inbetween = info.key->inbetween(info.inbetween);
    for (const StrokePtr &stroke : inbetween.strokes) {
        for (Point *p : stroke->points()) {
            if ((pos - p->pos()).norm() < k_deformRange * 0.5) {
                strokes.push_back(stroke->id());
                break;
            }
        }
    }

    // Erase them completely
    if (strokes.size() > 0) {
        m_editor->undoStack()->beginMacro("Erase stroke");

        Layer *layer = m_editor->layers()->currentLayer();
        int currentFrame = m_editor->playback()->currentFrame();
        VectorKeyFrame *keyframe = info.key;
        double dt = 1.0 / layer->stride(currentFrame);
        double a = m_editor->alpha(currentFrame);
        double partialAlpha = a <= 1e-6 ? 0.0 : a - dt * 0.5;

        // TODO: when k_eraseFromSelection is true, we should only erase the stroke segments in the group and not the whole stroke

        bool eraseStroke;
        for (int stroke : strokes) {
            eraseStroke = true;
            // Do not erase strokes if they are not from a selected group
            if (k_eraseFromSelection) {
                for (Group *group : info.key->selection().selectedPostGroups()) if (!group->strokes().contains(stroke)) eraseStroke = false;
                for (Group *group : info.key->selection().selectedPreGroups()) if (!group->strokes().contains(stroke)) eraseStroke = false;
            }
            if (eraseStroke) {
                // int groupId = Group::ERROR_ID;
                // for (Group *group : info.key->postGroups()) {
                //     if (group->drawingPartials().lastPartialAt(partialAlpha).strokes().contains(stroke)) {
                //         groupId = group->id();
                //         break;
                //     }
                // }
                // if (groupId != Group::ERROR_ID) {
                //     DrawingPartial prevPartial = keyframe->postGroups().fromId(groupId)->drawingPartials().lastPartialAt(partialAlpha);
                //     if (prevPartial.t() != partialAlpha) {
                //         m_editor->undoStack()->push(new AddDrawingPartial(m_editor, m_editor->layers()->currentLayerIndex(), currentFrame, groupId, DrawingPartial(keyframe, partialAlpha), prevPartial));
                //     }
                // }
                m_editor->undoStack()->push(new EraseCommand(m_editor, layerIdx, m_frame, stroke));
            }
        }

        // Remove adjacent identical partials
        // for (Group *group : keyframe->postGroups()) {
        //     bool update = true;
        //     while (update) {
        //         update = false;
        //         QMutableMapIterator<double, DrawingPartial> it(group->drawingPartials().partials());
        //         it.next();
        //         DrawingPartial *partial = &(it.value());
        //         while (it.hasNext()) {
        //             it.next();
        //             if (it.value().compare(*partial)) {
        //                 it.remove();
        //                 update = true;
        //             } else {
        //                 partial = &(it.value());
        //             }
        //         }
        //     }
        // }

        // TODO: in the same macro, we could erase empty groups
        m_editor->undoStack()->endMacro();
    }
    // TODO: if we're erasing on a partial frame we should draw (with transparency) the previous drawing partial so that users still the erased strokes
}

void EraserTool::eraseSegments(const EventInfo& info) {
    Point::VectorType p = QE_POINT(info.pos);
    double rangeSq = k_deformRange * k_deformRange * 0.25;
    const QMap<int, Group *> &groups = info.key->selection().selectedPostGroups().empty() ? info.key->groups(POST) : info.key->selection().selectedPostGroups();
    const Inbetween &inbetween = info.key->inbetween(info.inbetween);
    double vis = info.alpha == 0.0 ? -2.0 : -info.alpha;
    if (info.modifiers & Qt::ShiftModifier) vis = 0.0; // uneraser
    for (Group *group : groups) {
        for (auto it = group->strokes(info.alpha).constBegin(); it != group->strokes(info.alpha).constEnd(); ++it) {
            Stroke *stroke = inbetween.strokes.value(it.key()).get();
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i){
                    if ((stroke->points()[i]->pos() - p).squaredNorm() < rangeSq) {
                        info.key->visibility()[Utils::cantor(stroke->id(), i)] = vis;
                    }
                }
            }
        }
    }
    info.key->makeInbetweensDirty();
}