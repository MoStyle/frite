/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pivottangenttool.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"



PivotTangentTool::PivotTangentTool(QObject *parent, Editor *editor) : 
    PivotToolAbstract(parent, editor) 
    , m_p1Pressed(false)
    , m_p2Pressed(false)
    { }


PivotTangentTool::~PivotTangentTool() {

}

Tool::ToolType PivotTangentTool::toolType() const {
    return Tool::PivotTangent;
}

QCursor PivotTangentTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}


void PivotTangentTool::pressed(const EventInfo& info) {
    if (!m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;
    int frame = m_editor->playback()->currentFrame();
    if (!m_editor->layers()->currentLayer()->keyExists(frame)) return;
    if (!m_editor->layers()->currentLayer()->getLastKey(frame)->isTranslationExtracted()) return;

    m_p1Pressed = false;
    m_p2Pressed = false;

    Bezier2D * bezier = info.key->getPivotCurve();
    Point::VectorType pos(info.pos.x(), info.pos.y());
    Point::VectorType p1 = bezier->getP1();
    Point::VectorType p2 = bezier->getP2();

    m_editor->undoStack()->beginMacro("Edit tangent");

    if ((pos - p1).norm() < 8.0) {
        m_p1Pressed = true;
        return;
    }

    if ((pos - p2).norm() < 8.0) {
        m_p2Pressed = true;
        return;
    }
}

void PivotTangentTool::moved(const EventInfo& info) {
    if (!m_p1Pressed && !m_p2Pressed) return;
    Layer * layer = m_editor->layers()->currentLayer();
    int layerIdx = m_editor->layers()->currentLayerIndex();
    int keyFramePosition = layer->getVectorKeyFramePosition(info.key);
    int previousFrame = layer->getPreviousKeyFramePosition(keyFramePosition);
    int nextFrame = layer->getNextKeyFramePosition(keyFramePosition);
    int lastFrame = layer->getMaxKeyFramePosition();

    bool keepContinuity = !(info.modifiers & Qt::ShiftModifier);
    
    Bezier2D * bezier(info.key->getPivotCurve());
    Point::VectorType pos(info.pos.x(), info.pos.y());

    if (m_p1Pressed) {
        bezier->setP1(pos);
        // sync with prev tangent if it is connected
        if (previousFrame != keyFramePosition && keepContinuity) {
            Point::VectorType delta = bezier->getP1() - bezier->getP0(); 
            Bezier2D * previousBezier(layer->getVectorKeyFrameAtFrame(previousFrame)->getPivotCurve());
            previousBezier->setP2(previousBezier->getP3() - delta);
            m_editor->undoStack()->push(new PivotTrajectoryCommand(m_editor, layerIdx, previousFrame, previousBezier));
        }
    }

    if (m_p2Pressed) {
        bezier->setP2(pos);
        // sync with next tangent if it is connected
        if (nextFrame != keyFramePosition && nextFrame != lastFrame && keepContinuity) {
            Point::VectorType delta = bezier->getP2() - bezier->getP3();
            float tNext = layer->getFrameTValue(nextFrame);
            Bezier2D * nextBezier(layer->getVectorKeyFrameAtFrame(nextFrame)->getPivotCurve());
            nextBezier->setP1(nextBezier->getP0() - delta);
            m_editor->undoStack()->push(new PivotTrajectoryCommand(m_editor, layerIdx, nextFrame, nextBezier));
        }
    }

    if (m_p1Pressed || m_p2Pressed) {
        m_editor->undoStack()->push(new PivotTrajectoryCommand(m_editor, layerIdx, keyFramePosition, bezier, !keepContinuity));
    }
}

void PivotTangentTool::released(const EventInfo& info) {
    if (!m_p1Pressed && !m_p2Pressed) return;
    m_p1Pressed = false;
    m_p2Pressed = false;
    m_editor->undoStack()->endMacro();
}

void PivotTangentTool::drawUI(QPainter &painter, VectorKeyFrame *key) {

    Layer *layer = key->parentLayer();
    // draw pivot trajectory
    QVector<VectorKeyFrame *> selectedKeys = layer->getSelectedKeyFrames();
    QVector<VectorKeyFrame *> keys;
    for (VectorKeyFrame * key : selectedKeys){
        if (key->isTranslationExtracted())
            keys.append(key);
    }

    if (keys.empty()) return;    
    drawTrajectory(painter, keys);

    // draw coordinate system
    int frame = m_editor->playback()->currentFrame();
    drawPivot(painter, frame);
    int nextFrame = m_editor->layers()->currentLayer()->getNextKeyFramePosition(frame);
    if (nextFrame != frame){
        drawPivot(painter, nextFrame, 0.5);
    }

    if (!m_editor->layers()->currentLayer()->keyExists(frame)) return;


    static QPen pen(QColor(200, 200, 200), 2.0);
    int stride = layer->stride(layer->getVectorKeyFramePosition(key));
    pen.setCapStyle(Qt::RoundCap);
    

    // TODO: cleanup + use graphics item
    // draw selected trajectory's tangents 
    float t = m_editor->layers()->currentLayer()->getFrameTValue(frame);
    Bezier2D * bezier = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame)->getPivotCurve();

    // draw tangents
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(40, 0, 0));
    Point::VectorType p0 = bezier->getP0();
    Point::VectorType p1 = bezier->getP1();
    Point::VectorType p2 = bezier->getP2();
    Point::VectorType p3 = bezier->getP3();
    painter.fillRect(QRectF(p1.x() - 2.0, p1.y() - 2.0, 4.0, 4.0), QBrush(m_p1Pressed ? Qt::red : Qt::black));
    painter.fillRect(QRectF(p2.x() - 2.0, p2.y() - 2.0, 4.0, 4.0), QBrush(m_p2Pressed ? Qt::red : Qt::black));
    painter.setPen(QColor(40, 0, 0, 40));
    painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
    painter.drawLine(QPointF(p3.x(), p3.y()), QPointF(p2.x(), p2.y()));
}