/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pivottool.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"

PivotEditTool::PivotEditTool(QObject *parent, Editor *editor) : 
    PivotToolAbstract(parent, editor) 
    , m_pressed(false) 
    , m_currentState(PIVOT_TRANSLATION)
    , m_currentPos(0, 0)
    { }


PivotEditTool::~PivotEditTool() {

}

Tool::ToolType PivotEditTool::toolType() const {
    return Tool::PivotEdit;
}

QCursor PivotEditTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void PivotEditTool::toggled(bool on){
    
}


void PivotEditTool::pressed(const EventInfo& info){
    if (m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;
    int frame = m_editor->playback()->currentFrame();
    if (!m_editor->layers()->currentLayer()->keyExists(frame)) return;
    if (!m_editor->layers()->currentLayer()->getLastKey(frame)->isTranslationExtracted()) return;
    if (info.mouseButton & Qt::RightButton){
        if (info.modifiers & Qt::AltModifier)
            m_currentState = CONTEXT_MENU;
        else {
            bool usedShift = info.modifiers & Qt::ShiftModifier;
            m_editor->undoStack()->beginMacro(usedShift ? "Pivot Translation" : "Pivot position adjustment");
            Layer * layer = m_editor->layers()->currentLayer();
            m_currentPos = Point::VectorType(info.pos.x(), info.pos.y());
            if (usedShift && (info.modifiers & Qt::ControlModifier))
                m_currentState = LAYER_TRANSLATION_SELECTION;
            else if (usedShift)
                m_currentState = LAYER_TRANSLATION;
            else
                m_currentState = PIVOT_TRANSLATION;
        }
    }
    else if (info.mouseButton & Qt::LeftButton){
        m_currentState = PIVOT_TRAJECTORY;
        m_trajectoryPoints.clear();
        m_trajectoryPoints.push_back(Point::VectorType(info.pos.x(), info.pos.y()));
    }
    m_pressed = true;
}

void PivotEditTool::moved(const EventInfo& info){
    if (!m_pressed) return;

    if (m_currentState == LAYER_TRANSLATION || m_currentState == PIVOT_TRANSLATION || m_currentState == LAYER_TRANSLATION_SELECTION){
        int frame = m_editor->playback()->currentFrame();
        int layerIdx = m_editor->layers()->currentLayerIndex();

        Point::VectorType mousePos(info.pos.x(), info.pos.y());
        Point::VectorType translation(mousePos - m_currentPos);

        if (m_currentState == LAYER_TRANSLATION)
            m_editor->undoStack()->push(new LayerTranslationCommand(m_editor, layerIdx, frame, translation));
        if (m_currentState == PIVOT_TRANSLATION)
            m_editor->undoStack()->push(new MovePivotCommand(m_editor, layerIdx, frame, translation));
        if (m_currentState == LAYER_TRANSLATION_SELECTION){
            for (VectorKeyFrame * key : m_editor->layers()->currentLayer()->getSelectedKeyFramesWithDefault()){
                int frame = m_editor->layers()->currentLayer()->getVectorKeyFramePosition(key);
                m_editor->undoStack()->push(new LayerTranslationCommand(m_editor, layerIdx, frame, translation));
            }
        }
        m_currentPos += translation;
    }

    if (m_currentState == PIVOT_TRAJECTORY){
        m_trajectoryPoints.push_back(Point::VectorType(info.pos.x(), info.pos.y()));
    }

}

void PivotEditTool::released(const EventInfo& info){
    if (!m_pressed) return;

    m_pressed = false;

    if (m_currentState == CONTEXT_MENU){
        QMenu contextMenu;
        int frame = m_editor->playback()->currentFrame();

        contextMenu.addAction(tr("Reset pivot"), this, [&](){
            m_editor->undoStack()->beginMacro("Reset pivot");
            resetPivot(frame);
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addAction(tr("Reset ALL pivots"), this, [&](){
            Layer * layer = m_editor->layers()->currentLayer();
            m_editor->undoStack()->beginMacro("Reset all pivots");
            for (auto it = layer->keysBegin(); it != layer->keysEnd(); it++){
                resetPivotToBarycenter(it.key());
            }
            for (auto it = layer->keysBegin(); it != layer->keysEnd(); it++){
                resetPivotTranslation(it.key());
            }
            for (auto it = layer->keysBegin(); it != layer->keysEnd(); it++){
                resetPivotTrajectory(it.key());
            }

            m_editor->undoStack()->endMacro();
        });
        contextMenu.addSeparator();
        
        contextMenu.addAction(tr("Move pivot to barycenter"), this, [&](){
            resetPivotToBarycenter(frame);
        });
        contextMenu.addAction(tr("Reset pivot trajectory"), this, [&](){
            resetPivotTrajectory(frame);
        });
        contextMenu.addAction(tr("Reset pivot translation"), this, [&](){
            resetPivotTranslation(frame);
        });

        contextMenu.exec(QCursor::pos());
    }

    if (m_currentState == LAYER_TRANSLATION || m_currentState == LAYER_TRANSLATION_SELECTION || m_currentState == PIVOT_TRANSLATION){
        m_editor->undoStack()->endMacro();
    }

    if (m_currentState == PIVOT_TRAJECTORY){
        if (m_trajectoryPoints.size() < 2) return;
        // fit cubic to m_stroke
        
        Bezier2D * cubicApprox = new Bezier2D();
        cubicApprox->fit(m_trajectoryPoints, false);


        int frame = m_editor->playback()->currentFrame();
        Layer * layer = m_editor->layers()->currentLayer();
        int nextFrame = layer->getNextKeyFramePosition(frame);
        Point::VectorType P0 = layer->getPivotControlPoint(frame);
        Point::VectorType P3 = layer->getPivotControlPoint(nextFrame);
        if (!std::isnan(P0.x()) && !std::isnan(P3.x()) && frame != nextFrame){
            cubicApprox->fitExtremities(P0, P3);
            int layerIdx = m_editor->layers()->currentLayerIndex();
            m_editor->undoStack()->push(new PivotTrajectoryCommand(m_editor, layerIdx, frame, cubicApprox));
        }

        m_trajectoryPoints.clear();
    }

}

void PivotEditTool::resetPivot(int frame){
    resetPivotToBarycenter(frame);
    resetPivotTranslation(frame);
    resetPivotTrajectory(frame);
}

void PivotEditTool::resetPivotToBarycenter(int frame){
    int layerIdx = m_editor->layers()->currentLayerIndex();
    Layer * layer = m_editor->layers()->currentLayer();
    Point::VectorType barycenter;
    KeyframedVector * translation;
    if (layer->getMaxKeyFramePosition() <= frame){
        barycenter = layer->getPrevKey(layer->getMaxKeyFramePosition())->getCenterOfGravity(TARGET_POS);
        translation = layer->getPrevKey(layer->getMaxKeyFramePosition())->translation();
        translation->frameChanged(1);
    }
    else{
        barycenter = layer->getVectorKeyFrameAtFrame(frame)->getCenterOfGravity(REF_POS);
        translation = layer->getVectorKeyFrameAtFrame(frame)->translation();
        translation->frameChanged(0);
    }
    std::cout << "Barycenter = " << barycenter << std::endl;
    m_editor->undoStack()->push(new MovePivotCommand(m_editor, layerIdx, frame, translation->get() + barycenter));
}

void PivotEditTool::resetPivotTrajectory(int frame){
    Layer * layer = m_editor->layers()->currentLayer();
    int nextFrame = layer->getNextKeyFramePosition(frame);
    Point::VectorType P0 = layer->getPivotPosition(frame);
    Point::VectorType P3 = layer->getPivotPosition(nextFrame);
    const qreal alpha = 1. / 3.;
    Point::VectorType P1 = P0 * (1 - alpha) + P3 * alpha;
    Point::VectorType P2 = P3 * (1 - alpha) + P0 * alpha;

    Bezier2D * cubicApprox = new Bezier2D();
    cubicApprox->setP0(P0);
    cubicApprox->setP1(P1);
    cubicApprox->setP2(P2);
    cubicApprox->setP3(P3);    

    int layerIdx = m_editor->layers()->currentLayerIndex();
    m_editor->undoStack()->push(new PivotTrajectoryCommand(m_editor, layerIdx, frame, cubicApprox));
}

void PivotEditTool::resetPivotTranslation(int frame){
    int layerIdx = m_editor->layers()->currentLayerIndex();
    Layer * layer = m_editor->layers()->currentLayer();
    KeyframedVector * translation = layer->getVectorKeyFrameAtFrame(frame)->translation();
    translation->frameChanged(0);
    m_editor->undoStack()->push(new LayerTranslationCommand(m_editor, layerIdx, frame, - translation->get()));
}

void PivotEditTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    Layer *layer = key->parentLayer();
    QVector<VectorKeyFrame *> selectedKeys = layer->getSelectedKeyFrames();
    QVector<VectorKeyFrame *> keys;
    for (VectorKeyFrame * key : selectedKeys){
        if (key->isTranslationExtracted())
            keys.append(key);
    }
    if (keys.empty()) return;
    drawTrajectory(painter, keys);

    // draw coordinate system       
    int currentFrame = m_editor->playback()->currentFrame();
    Point::VectorType position = m_editor->layers()->currentLayer()->getPivotPosition(currentFrame);
    int stride = m_editor->layers()->currentLayer()->stride(currentFrame);
    int inbetween = m_editor->layers()->currentLayer()->inbetweenPosition(currentFrame);
    float t = stride > 1 ?  float(inbetween) / (stride - 1) : 0;
    t = inbetween >= stride ? 1. : t;

    float angle = m_editor->layers()->currentLayer()->getLastVectorKeyFrameAtFrame(currentFrame, 0)->getFrameRotation(t);
    drawPivot(painter, position, angle);
    int nextFrame = m_editor->layers()->currentLayer()->getNextKeyFramePosition(currentFrame);
    if (nextFrame != currentFrame){
        drawPivot(painter, nextFrame, 0.5);
    }

    // draw new trajectory
    if (m_currentState == PIVOT_TRAJECTORY && m_trajectoryPoints.size() >= 2){
        static QPen penCurve(QColor(200, 200, 200), 2.0);
        penCurve.setCapStyle(Qt::RoundCap);
        penCurve.setStyle(Qt::DashLine);
        painter.setPen(penCurve);
        QPointF prev(m_trajectoryPoints.front().x(), m_trajectoryPoints.front().y());
        for (int i = 1; i < m_trajectoryPoints.size(); i++) {
            QPointF cur(m_trajectoryPoints[i].x(), m_trajectoryPoints[i].y());
            painter.drawLine(prev, cur);
            prev = cur;
        }
    }
}