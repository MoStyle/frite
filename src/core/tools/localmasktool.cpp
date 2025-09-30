/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "localmasktool.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "canvascommands.h"
#include "tabletcanvas.h"
#include "viewmanager.h"
#include "dialsandknobs.h"
#include "qteigen.h"

extern dkSlider k_deformRange;

using namespace Frite;

LocalMaskTool::LocalMaskTool(QObject *parent, Editor *editor) : Tool(parent, editor), m_fontMetrics(m_editor->tabletCanvas()->canvasFont()) {
    m_toolTips = QString("Left-click: make strokes appear | Right-click: make strokes disappear | Hold CTRL modifier: painting mode");
    m_contextMenuAllowed = false;
    m_pressed = false;
    m_pressedFirstFrameNumber = false;
    m_pressedLastFrameNumber = false;
    m_projected = false;
    m_onFrameNumber = false;
    m_alpha = 0.0;
    m_sign = 1.0;
    m_firstProjectionVisibility = 0.0;
    m_lastProjectionVisibility = 0.0;
    m_firstProjectionParam = 0.0;
    m_lastProjectionParam = 0.0;
    m_prevKeyFrame = nullptr;
    m_validatingClusters = false;
}

Tool::ToolType LocalMaskTool::toolType() const {
    return Tool::LocalMask;
}

QCursor LocalMaskTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void LocalMaskTool::toggled(bool on) {
    Tool::toggled(on);
    m_editor->tabletCanvas()->setMouseTracking(on);
    m_editor->tabletCanvas()->setTabletTracking(on);
    m_editor->tabletCanvas()->fixedCanvasView()->setAttribute(Qt::WA_TransparentForMouseEvents, on);
    m_editor->tabletCanvas()->setDisplayVisibility(on);
    setValidingCusters(false);
    m_editor->tabletCanvas()->setDisplayMode(on ? TabletCanvas::DisplayMode::VisibilityThreshold : TabletCanvas::DisplayMode::StrokeColor);
}

// TODO clear polyline when keyframe is changed

void LocalMaskTool::pressed(const EventInfo& info) {
    if (info.mouseButton & Qt::MiddleButton) return;

    m_pressed = true;
    m_savedVisibility = info.key->visibility();

    if (info.modifiers & Qt::ControlModifier) {
        paint(info);
        return;
    } 
    
    if (m_firstPointRect.contains(info.pos.x(), info.pos.y()) && m_polyline.size() > 0 && !m_editor->playback()->isPlaying() && m_projected && info.key == m_prevKeyFrame) {
        m_pressedFirstFrameNumber = true;
        m_pressedPos = info.pos;
    } else if (m_lastPointRect.contains(info.pos.x(), info.pos.y()) && m_polyline.size() > 0 && !m_editor->playback()->isPlaying() && m_projected && info.key == m_prevKeyFrame) {
        m_pressedLastFrameNumber = true;
        m_pressedPos = info.pos;
    } else {
        m_pressedFirstFrameNumber = false;
        m_pressedLastFrameNumber = false;
        m_projected = false;
        m_polyline.clear();
        Point *point = new Point(info.pos.x(), info.pos.y(), 0.0,  1.0);
        m_polyline.addPoint(point);
    }
}

void LocalMaskTool::moved(const EventInfo& info)  {
    if (info.mouseButton & Qt::MiddleButton) return;

    m_onFrameNumber = false;
    if (m_polyline.size() > 0 && !m_editor->playback()->isPlaying() && m_projected) {
        if (m_firstPointRect.contains(info.pos.x(), info.pos.y())) {
            m_onFrameNumber = true;
        } else if (m_lastPointRect.contains(info.pos.x(), info.pos.y())) {
            m_onFrameNumber = true;
        }
    } 
    m_editor->tabletCanvas()->updateCursor();

    if (!m_pressed) return;

    if (info.modifiers & Qt::ControlModifier) {
        paint(info);
        info.key->makeInbetweensDirty();
        return;
    } 
    
    if (m_pressedFirstFrameNumber || m_pressedLastFrameNumber) {
        updateGradient(info);
    } else {
        Point *point = new Point(info.pos.x(), info.pos.y(), 0.0,  1.0);
        m_polyline.addPoint(point);
    }

    info.key->makeInbetweensDirty();
}

void LocalMaskTool::released(const EventInfo& info) {
    if (!m_pressed) return;
    if (info.mouseButton & Qt::MiddleButton) return;
    m_pressed = false;

    if (info.modifiers & Qt::ControlModifier) {
        paint(info);
        m_pressedFirstFrameNumber = false;
        m_pressedLastFrameNumber = false;
        m_editor->undoStack()->push(new SetVisibilityCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedVisibility));
        return;
    } 

    if (m_pressedFirstFrameNumber || m_pressedLastFrameNumber) {
        updateGradient(info);
    } else {
        Point *point = new Point(info.pos.x(), info.pos.y(), 0.0,  1.0);
        m_polyline.addPoint(point);
        projectVisibility(info);
    }

    m_pressedFirstFrameNumber = false;
    m_pressedLastFrameNumber = false;

    m_editor->undoStack()->push(new SetVisibilityCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedVisibility));
}

void LocalMaskTool::wheel(const WheelEventInfo& info) {
    if (info.modifiers & Qt::ShiftModifier) {
        if (info.delta > 0) k_deformRange.setValue(k_deformRange + 2);
        else                k_deformRange.setValue(k_deformRange - 2);
        m_editor->tabletCanvas()->updateCursor();
    }
}

void LocalMaskTool::keyReleased(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape  && m_validatingClusters) {
        m_editor->undoStack()->undo();
        setValidingCusters(false);
    } else if (event->key() == Qt::Key_Return && m_validatingClusters) {
        setValidingCusters(false);
    } else if (event->key() == Qt::Key_F) {
        if (m_polyline.size() < 2) return;
        double alpha = m_editor->currentAlpha();
        Layer *layer = m_editor->layers()->currentLayer();
        VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);
        const QMap<int, Group *> &groups = keyframe->selection().selectedPostGroups().empty() ? keyframe->groups(POST) : keyframe->selection().selectedPostGroups();

        if (event->modifiers() & Qt::ControlModifier) {
            // Switch sign (appearance/disappearance)
            m_sign = -m_sign;
            for (Group *group : groups) {
                const StrokeIntervals &strokes = group->strokes(alpha);
                strokes.forEachPoint(keyframe, [&](Point *point, unsigned int sId, unsigned int pId) {
                    Point::Scalar projectionParam = m_polyline.project(point->pos());
                    Point::VectorType projectedPoint = m_polyline.pos(projectionParam);
                    if ((projectedPoint - point->pos()).norm() <= k_deformRange * 0.5) {
                        keyframe->visibility()[Utils::cantor(sId, pId)] = -keyframe->visibility()[Utils::cantor(sId, pId)]; 
                    }
                });
            }
        } else {
            // Switch gradient orientation
            double d = 1.0 - m_alpha;
            for (Group *group : groups) {
                const StrokeIntervals &strokes = group->strokes(alpha);
                strokes.forEachPoint(keyframe, [&](Point *point, unsigned int sId, unsigned int pId) {
                    Point::Scalar projectionParam = m_polyline.project(point->pos());
                    Point::VectorType projectedPoint = m_polyline.pos(projectionParam);
                    if ((projectedPoint - point->pos()).norm() <= k_deformRange * 0.5) {
                        keyframe->visibility()[Utils::cantor(sId, pId)] = m_sign - keyframe->visibility()[Utils::cantor(sId, pId)] + m_sign * m_alpha; 
                    }
                });
            }
        }

        keyframe->makeInbetweensDirty();
    } else if (event->key() == Qt::Key_R) {
        Layer *layer = m_editor->layers()->currentLayer();
        VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);
        for (Group *group : keyframe->postGroups()) {
            group->strokes().forEachPoint(keyframe, [&](Point *p, unsigned int sid, unsigned int pid) {
                keyframe->visibility().remove(Utils::cantor(sid, pid));
            });
        }
    }
}

void LocalMaskTool::drawUI(QPainter &painter, VectorKeyFrame *key) { 
    Tool::drawUI(painter, key);

    if (m_polyline.size() > 0 && !m_editor->playback()->isPlaying() && m_projected && key == m_prevKeyFrame) {
        QPen p(QBrush(Qt::black), 2.0);
        m_fontMetrics = QFontMetrics(m_editor->tabletCanvas()->canvasFont());
        painter.setPen(p);
        int currentFrame = m_editor->playback()->currentFrame();
        int stride = m_editor->layers()->currentLayer()->stride(currentFrame);
        int firstPoint = m_sign > 0 ? std::ceil(m_firstProjectionVisibility * stride) : std::floor(m_firstProjectionVisibility * stride); 
        int lastPoint = m_sign > 0 ? std::ceil(m_lastProjectionVisibility * stride) : std::floor(m_lastProjectionVisibility * stride); 
        QString firstPointTxt = QString("#%1").arg(std::abs(firstPoint) + key->keyframeNumber());
        QString lastPointTxt = QString("#%1").arg(std::abs(lastPoint) + key->keyframeNumber());
        painter.drawText(QPointF(m_firstProjectedPoint.x(), m_firstProjectedPoint.y()), firstPointTxt);
        painter.drawText(QPointF(m_lastProjectedPoint.x(), m_lastProjectedPoint.y()), lastPointTxt);
        m_firstPointRect = m_fontMetrics.tightBoundingRect(firstPointTxt).translated(m_firstProjectedPoint.x(), m_firstProjectedPoint.y());
        m_lastPointRect = m_fontMetrics.tightBoundingRect(lastPointTxt).translated(m_lastProjectedPoint.x(), m_lastProjectedPoint.y());
    }

    if (m_validatingClusters) {
        m_editor->tabletCanvas()->setFontSize(24.0f * (1.0f / m_editor->view()->scaling()));
        painter.setFont(m_editor->tabletCanvas()->canvasFont());
        painter.setPen(Qt::black);
        painter.drawText(m_editor->view()->mapScreenToCanvas(QPointF(50, 50)), "Confirm? [Enter/ESC]");
    }
}

void LocalMaskTool::drawGL(VectorKeyFrame *key, qreal alpha) {
    if (!m_onFrameNumber) {
        m_editor->tabletCanvas()->drawCircleCursor(QVector2D(0.0, 0.0));
    }
}

void LocalMaskTool::setValidingCusters(bool b) {
    m_validatingClusters = b;
    m_needEscapeFocus = b;
    m_needReturnFocus = b;
    m_editor->tabletCanvas()->setDisplayVisibility(b);
    m_editor->tabletCanvas()->setDisplayMode(TabletCanvas::DisplayMode::VisibilityThreshold);
    // m_editor->tabletCanvas()->setMaskOcclusionMode(TabletCanvas::MaskGrayOut);
}

void LocalMaskTool::paint(const EventInfo& info) {
    m_projected = false;
    Point::VectorType p = QE_POINT(info.pos);
    double d = info.mouseButton & Qt::RightButton ? -0.01 : 0.01;
    double rangeSq = k_deformRange * k_deformRange * 0.25;
    const QMap<int, Group *> &groups = info.key->selection().selectedPostGroups().empty() ? info.key->groups(POST) : info.key->selection().selectedPostGroups();
    const Inbetween &inbetween = info.key->inbetween(info.inbetween);
    for (Group *group : groups) {
        for (auto it = group->strokes(info.alpha).constBegin(); it != group->strokes(info.alpha).constEnd(); ++it) {
            Stroke *stroke = inbetween.strokes.value(it.key()).get();
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i){
                    if ((stroke->points()[i]->pos() - p).squaredNorm() < rangeSq) {
                        info.key->visibility()[Utils::cantor(stroke->id(), i)] = info.key->visibility()[Utils::cantor(stroke->id(), i)] == -2.0 ? -2.0 : (info.modifiers & Qt::AltModifier ? info.alpha : std::clamp(info.key->visibility()[Utils::cantor(stroke->id(), i)] + d, -1.0, 1.0));
                    }
                }
            }
        }
    }
}

void LocalMaskTool::projectVisibility(const EventInfo &info) {
    if (m_polyline.size() < 2) return;
    const QMap<int, Group *> &groups = info.key->selection().selectedPostGroups().empty() ? info.key->groups(POST) : info.key->selection().selectedPostGroups();
    m_sign = info.mouseButton & Qt::RightButton ? -1.0 : 1.0;
    m_projected = false;
    m_prevKeyFrame = info.key;
    m_firstProjectionParam = std::numeric_limits<double>::max();
    m_lastProjectionParam = 0.0;
    for (Group *group : groups) {
        const StrokeIntervals &strokes = group->strokes(info.alpha);
        strokes.forEachPoint(info.key, [&](Point *point, unsigned int sId, unsigned int pId) {
            Point::Scalar projectionParam = m_polyline.project(point->pos());
            Point::VectorType projectedPoint = m_polyline.pos(projectionParam);
            if ((projectedPoint - point->pos()).norm() <= k_deformRange * 0.5) {
                // Visibility gradient from [currentAlpha, 1.0] or [-1.0, -currentAlpha]
                info.key->visibility()[Utils::cantor(sId, pId)] =  info.key->visibility()[Utils::cantor(sId, pId)] == -2.0 ? -2.0 : m_sign * ((projectionParam / m_polyline.length()) * (1.0 - info.alpha) + info.alpha);
                qDebug() << info.key->visibility()[Utils::cantor(sId, pId)];
                if (m_sign < 0 && info.key->visibility()[Utils::cantor(sId, pId)] == 0.0) info.key->visibility()[Utils::cantor(sId, pId)] -= 1e-8;
                if (info.key->visibility()[Utils::cantor(sId, pId)] >= -1.0) {
                    m_projected = true;

                    if (projectionParam <= m_firstProjectionParam) {
                        m_firstProjectedPoint = projectedPoint;
                        m_firstProjectionVisibility = info.key->visibility()[Utils::cantor(sId, pId)];
                        m_firstProjectionParam = projectionParam;
                    }
                    if (projectionParam >= m_lastProjectionParam) {
                        m_lastProjectedPoint = projectedPoint;
                        m_lastProjectionVisibility = info.key->visibility()[Utils::cantor(sId, pId)];
                        m_lastProjectionParam = projectionParam;
                    }
                    // if (m_sign < 0 && info.key->visibility()[Utils::cantor(sId, pId)] == 0.0) info.key->visibility()[Utils::cantor(sId, pId)] = -1.5;
                }
            }
        });
    }

    if (m_sign > 0 && m_firstProjectionVisibility > m_lastProjectionVisibility) {
        std::swap(m_firstProjectionVisibility, m_lastProjectionVisibility);
        std::swap(m_firstProjectedPoint, m_lastProjectedPoint);
        std::swap(m_firstProjectionParam, m_lastProjectionParam);
    } else if (m_sign < 0 && m_firstProjectionVisibility < m_lastProjectionVisibility) {
        std::swap(m_firstProjectionVisibility, m_lastProjectionVisibility);
        std::swap(m_firstProjectedPoint, m_lastProjectedPoint);
        std::swap(m_firstProjectionParam, m_lastProjectionParam);
    }

    m_alpha = info.alpha;
}

void LocalMaskTool::updateGradient(const EventInfo &info) {
    if (!m_projected) return;
    double delta = info.pos.x() - m_pressedPos.x();
    m_pressedPos = info.pos;
    delta =  delta > 0 ? 0.01 : -0.01;
    delta *= m_sign;
    double a = std::min(m_firstProjectionVisibility, m_lastProjectionVisibility);
    double b = std::max(m_firstProjectionVisibility, m_lastProjectionVisibility);
    if (m_sign < 0) {
        a = std::clamp(a, -1.0, -1e-8);
        b = std::clamp(b, -1.0, -1e-8);
    }

    if (m_pressedFirstFrameNumber) {
        m_firstProjectionVisibility += delta;   
        m_firstProjectionVisibility = m_sign > 0 ? std::clamp(m_firstProjectionVisibility, 0.0, b) : std::clamp(m_firstProjectionVisibility, a, -1e-8);
    } else if (m_pressedLastFrameNumber) {
        m_lastProjectionVisibility += delta;   
        m_lastProjectionVisibility = m_sign > 0 ? std::clamp(m_lastProjectionVisibility, a, 1.0) : std::clamp(m_lastProjectionVisibility, -1.0, b);
    }
    double deltaParam = std::abs(m_lastProjectionParam - m_firstProjectionParam);
    const QMap<int, Group *> &groups = info.key->selection().selectedPostGroups().empty() ? info.key->groups(POST) : info.key->selection().selectedPostGroups();
    for (Group *group : groups) {
        const StrokeIntervals &strokes = group->strokes(info.alpha);
        strokes.forEachPoint(info.key, [&](Point *point, unsigned int sId, unsigned int pId) {
            Point::Scalar projectionParam = m_polyline.project(point->pos());
            Point::VectorType projectedPoint = m_polyline.pos(projectionParam);
            if ((projectedPoint - point->pos()).norm() <= k_deformRange * 0.5) {
                info.key->visibility()[Utils::cantor(sId, pId)] =  info.key->visibility()[Utils::cantor(sId, pId)] == -2.0 ? -2.0 : m_sign * (((projectionParam - m_firstProjectionParam) / (deltaParam)) * (std::abs(m_lastProjectionVisibility) - std::abs(m_firstProjectionVisibility)) + std::abs(m_firstProjectionVisibility));
                if (m_sign < 0 && info.key->visibility()[Utils::cantor(sId, pId)] == 0.0) info.key->visibility()[Utils::cantor(sId, pId)] -= 1e-8;
            }
        });
    }
}
