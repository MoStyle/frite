/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pivotscalingtool.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"



PivotScalingTool::PivotScalingTool(QObject *parent, Editor *editor) : 
    PivotToolAbstract(parent, editor) 
    , m_currentState(SCALING)
    , m_pressed(false)
    , m_point(0, 0)
    , m_firstPos(0, 0)
    { }


PivotScalingTool::~PivotScalingTool() {

}

Tool::ToolType PivotScalingTool::toolType() const {
    return Tool::PivotScaling;
}

QCursor PivotScalingTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}



void PivotScalingTool::pressed(const EventInfo& info) {
    if (m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;
    int frame = m_editor->playback()->currentFrame();
    if (!m_editor->layers()->currentLayer()->keyExists(frame)) return;
    if (!m_editor->layers()->currentLayer()->getLastKey(frame)->isTranslationExtracted()) return;

    if (info.mouseButton & Qt::LeftButton){
        m_editor->undoStack()->beginMacro("Pivot Scaling");
        // EventInfo doesn't catch the default last KeyFrame
        VectorKeyFrame * key = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
        m_firstPos = Point::VectorType(info.pos.x(), info.pos.y());
        m_currentState = SCALING;
        m_pressed = true;
    }

    if (info.mouseButton & Qt::RightButton){
        m_currentState = CONTEXT_MENU;
        m_pressed = true;
    }
}

void PivotScalingTool::moved(const EventInfo& info) {
    if (!m_pressed) return;

    if (m_currentState == SCALING){
        Point::VectorType currentPos = Point::VectorType(info.pos.x(), info.pos.y());
        int frame = m_editor->playback()->currentFrame();
        VectorKeyFrame * key = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
        KeyframedVector * scaling = key->scaling();
        scaling->frameChanged(0);
        Point::VectorType currentScaling = Point::VectorType(1, 1);
        
        Point::VectorType pivotPos = m_editor->layers()->currentLayer()->getPivotControlPoint(frame);
        Point::VectorType point(info.pos.x(), info.pos.y());
        
        if (info.modifiers & Qt::ShiftModifier){
            Point::VectorType norm = (m_firstPos - pivotPos).normalized();
            point = pivotPos + (currentPos - pivotPos).dot(norm) * norm;
        }

        Point::VectorType originalDistance = m_firstPos - pivotPos;
        Point::VectorType currentDistance = -(m_firstPos - point);
        Point::VectorType ratio(currentDistance.x() / originalDistance.x(), currentDistance.y() / originalDistance.y());

        currentScaling += Point::VectorType(currentScaling.x() * ratio.x(), currentScaling.y() * ratio.y());
        int layerIdx = m_editor->layers()->currentLayerIndex();
        m_editor->undoStack()->push(new PivotScalingCommand(m_editor, layerIdx, frame, currentScaling));

        m_point = point;
    }

}

void PivotScalingTool::released(const EventInfo& info) {
    if (m_currentState == SCALING){
        m_editor->undoStack()->endMacro();
    }

    if (m_currentState == CONTEXT_MENU){
        QMenu contextMenu;
        int frame = m_editor->playback()->currentFrame();
        int layerIdx = m_editor->layers()->currentLayerIndex();

        contextMenu.addAction(tr("Reset Scaling"), this, [&](){
            Point::VectorType scale(1, 1);
            m_editor->undoStack()->push(new PivotScalingCommand(m_editor, layerIdx, frame, scale));
            
        });

        contextMenu.addSeparator();
        
        contextMenu.addAction(tr("Apply Mirroring on X axis"), this, [&](){
            applyMirroring(frame, true, false);
        });
        contextMenu.addAction(tr("Apply Mirroring on Y axis"), this, [&](){
            applyMirroring(frame, false, true);
        });
        contextMenu.addAction(tr("Apply Mirroring on both axis"), this, [&](){
            applyMirroring(frame, true, true);
        });

        contextMenu.exec(QCursor::pos());
    }

    m_pressed = false;
}

void PivotScalingTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    static QPen penPoint(QColor(125, 125, 125), 8);
    penPoint.setCapStyle(Qt::RoundCap);
    painter.setPen(penPoint);
    if (m_pressed)
        painter.drawPoint(m_point.x(), m_point.y());

    Layer * layer = m_editor->layers()->currentLayer();
    QVector<VectorKeyFrame *> selectedKeys = layer->getSelectedKeyFrames();
    QVector<VectorKeyFrame *> keys;
    for (VectorKeyFrame * key : selectedKeys){
        if (key->isTranslationExtracted())
            keys.append(key);
    }
    if (keys.empty()) return;
    drawTrajectory(painter, keys);

    int frame = m_editor->playback()->currentFrame();
    drawPivot(painter, frame);
}


void PivotScalingTool::applyMirroring(int frame, bool xAxis, bool yAxis){
    m_editor->undoStack()->beginMacro("Mirroring");
    VectorKeyFrame * key = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
    KeyframedVector * scaling = key->scaling();
    scaling->frameChanged(0);
    Point::VectorType scale = scaling->get();
    if (xAxis) scale.y() *= -1;
    if (yAxis) scale.x() *= -1;

    int layerIdx = m_editor->layers()->currentLayerIndex();
    m_editor->undoStack()->push(new PivotScalingCommand(m_editor, layerIdx, frame, scale));
    m_editor->undoStack()->endMacro();
}