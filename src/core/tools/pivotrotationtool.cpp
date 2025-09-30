/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pivotrotationtool.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"

static dkBool k_alignToTangent("PivotRotation->Align to tangent", false);
static dkBool k_orientedAlignment("PivotRotation->Oriented alignment", false);
static dkBool k_alignYaxis("PivotRotation->Align Y axis", false);

PivotRotationTool::PivotRotationTool(QObject *parent, Editor *editor) : 
    PivotToolAbstract(parent, editor) 
    , m_currentState(ROTATION)
    , m_pressed(false)
    , m_currentPos(0, 0)
    , m_initialDir(0, 0)
    { }


PivotRotationTool::~PivotRotationTool() {

}

Tool::ToolType PivotRotationTool::toolType() const {
    return Tool::PivotRotation;
}

QCursor PivotRotationTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}


void PivotRotationTool::pressed(const EventInfo& info) {
    if (m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;
    int frame = m_editor->playback()->currentFrame();
    if (!m_editor->layers()->currentLayer()->keyExists(frame)) return;
    if (!m_editor->layers()->currentLayer()->getLastKey(frame)->isTranslationExtracted()) return;
    
    if (info.mouseButton & Qt::LeftButton){
        int frame = m_editor->playback()->currentFrame();
        VectorKeyFrame * key = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
        m_currentPos = Point::VectorType(info.pos.x(), info.pos.y());
        m_currentState = ROTATION;
        m_initialDir = (m_currentPos - key->getPivotCurve()->eval(0)).normalized();
        m_editor->undoStack()->beginMacro("Pivot rotation");
    }
    if (info.mouseButton & Qt::RightButton){
        m_currentState = CONTEXT_MENU;
    }

    m_pressed = true;
}

void PivotRotationTool::moved(const EventInfo& info) {
    if (!m_pressed) return;

    if (m_currentState == ROTATION){       
        int frame = m_editor->playback()->currentFrame();
        VectorKeyFrame * key = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
        m_currentPos = Point::VectorType(info.pos.x(), info.pos.y());
        Point::VectorType currentDir = (m_currentPos - key->getPivotCurve()->eval(0)).normalized();
        Point::Scalar angle = atan2(m_initialDir.x() * currentDir.y() - m_initialDir.y() * currentDir.x(), m_initialDir.dot(currentDir));

        int layerIdx = m_editor->layers()->currentLayerIndex();
        bool useCurrent = !((info.modifiers & Qt::ShiftModifier) && !(info.modifiers & Qt::ControlModifier));
        bool usePrev = !((info.modifiers & Qt::ShiftModifier) && (info.modifiers & Qt::ControlModifier));
        m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, angle, useCurrent, usePrev));
        m_initialDir = currentDir;
    }
}

void PivotRotationTool::released(const EventInfo& info) {
    if (!m_pressed) return;

    if (m_currentState == ROTATION){
        m_editor->undoStack()->endMacro();
    }

    if (m_currentState == CONTEXT_MENU){
        QMenu contextMenu;
        int frame = m_editor->playback()->currentFrame();
        Layer * layer = m_editor->layers()->currentLayer();
        int layerIdx = m_editor->layers()->currentLayerIndex();
        VectorKeyFrame * key = layer->getVectorKeyFrameAtFrame(frame);

        contextMenu.addAction(tr("Reset Rotation"), this, [&](){
            float currentAngle = key->getFrameRotation(0);
            m_editor->undoStack()->beginMacro("Reset rotation");
            m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, -currentAngle, true, false));
            currentAngle = layer->getPrevKey(key)->getFrameRotation(1);
            m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, -currentAngle, false, true));
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addAction(tr("Reset Rotation of all key frames"), this, [&](){
            m_editor->undoStack()->beginMacro("Reset all rotation");
            for (auto it = layer->keysBegin(); it != layer->keysEnd(); it++){
                frame = it.key();
                float currentAngle = it.value()->getFrameRotation(0);
                m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, -currentAngle, true, false));
                currentAngle = layer->getPrevKey(it.value())->getFrameRotation(1);
                m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, -currentAngle, false, true));
            }
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addSeparator();

        QStringList items;
        items << tr("X") << tr("-X") << tr("Y") << tr("-Y");
        bool ok;
        auto getAxis = [&]{
            QString item = QInputDialog::getItem(nullptr, tr("Select axis to align"), tr("Axis"), items, 0, false, &ok);
            int x = item == "X" ? 1 : 0;
            x = item == "-X" ? -1 : x;
            // y axis is inverted
            int y = item == "Y" ? -1 : 0;
            y = item == "-Y" ? 1 : y;
            return Point::VectorType(x, y);
        };
        contextMenu.addAction(tr("Align layer to tangent"), this, [&](){
            Point::VectorType axis = getAxis();
            if (!ok) return;
            m_editor->undoStack()->beginMacro("Set pivot alignment");
            for (auto it = layer->keysBegin(); it != layer->keysEnd(); it++){
                frame = it.key();
                m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, frame, true, AlignTangent(true, axis)));
                m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, frame, false, AlignTangent(true, axis)));
            }
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addAction(tr("Not align layer to tangent"), this, [&](){
            m_editor->undoStack()->beginMacro("Set pivot alignment");
            for (auto it = layer->keysBegin(); it != layer->keysEnd(); it++){
                frame = it.key();
                m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, frame, true, AlignTangent(false, Point::VectorType(1, 0))));
                m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, frame, false, AlignTangent(false, Point::VectorType(1, 0))));
            }
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addSeparator();

        contextMenu.addAction(tr("Invert alignment"), this, [&](){
            m_editor->undoStack()->beginMacro("Set pivot alignment");
            VectorKeyFrame * currentKey = layer->getVectorKeyFrameAtFrame(frame);
            VectorKeyFrame * nextKey = currentKey;
            AlignTangent currentAlignment = currentKey->getAlignFrameToTangent(true);
            currentAlignment.m_axis = - currentAlignment.m_axis;
            do{
                currentKey = nextKey;
                int currentFrame = layer->getVectorKeyFramePosition(currentKey);
                m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, currentFrame, true, currentAlignment));
                m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, currentFrame, false, currentAlignment));
                nextKey = layer->getNextKey(currentKey);
            } while(currentKey != nextKey && nextKey->getAlignFrameToTangent(true).m_axis != currentAlignment.m_axis);
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addAction(tr("Not aligned"), this, [&](){
            m_editor->undoStack()->beginMacro("Set Pivot alignment");
            m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, frame, true, AlignTangent(false, Point::VectorType(1, 0))));
            m_editor->undoStack()->push(new PivotAlignTangentCommand(m_editor, layerIdx, frame, false, AlignTangent(false, Point::VectorType(1, 0))));
            m_editor->undoStack()->endMacro();
        });
        contextMenu.addSeparator();

        auto getAngle = [&](double value){
            double deg = QInputDialog::getDouble(nullptr, tr("Enter the angle (deg)"), tr("Angle"), value * 180.0 / M_PI);
            return deg * M_PI / 180.0;
        };
        contextMenu.addAction(tr("Set left rotation"), this, [&](){
            float currentAngle = layer->getPrevKey(key)->getFrameRotation(1);
            float desiredAngle = getAngle(currentAngle);
            m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, desiredAngle - currentAngle, false, true));
        });
        contextMenu.addAction(tr("Set right rotation"), this, [&](){
            float currentAngle = key->getFrameRotation(0);
            float desiredAngle = getAngle(currentAngle);
            m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, desiredAngle - currentAngle, true, false));
        });
        contextMenu.addAction(tr("Set both rotation"), this, [&](){
            float currentAngle = key->getFrameRotation(0);
            float desiredAngle = getAngle(currentAngle);
            m_editor->undoStack()->beginMacro("Set Pivot Rotation");
            m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, desiredAngle - currentAngle, true, false));
            currentAngle = layer->getPrevKey(key)->getFrameRotation(1);
            m_editor->undoStack()->push(new PivotRotationCommand(m_editor, layerIdx, frame, desiredAngle - currentAngle, false, true));
            m_editor->undoStack()->endMacro();
        });

        contextMenu.exec(QCursor::pos());
    }
    m_pressed = false;
}

void PivotRotationTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
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


    const QPen penForward(m_editor->forwardColor(), 2.0);
    const QPen penBackward(m_editor->backwardColor(), 2.0);
    static const Point::VectorType posDeltaForward = Point::VectorType(20, -20);
    static const Point::VectorType posDeltaBackward = Point::VectorType(-30, -20);
    Point::VectorType position = layer->getPivotPosition(frame);
    QString text;
    QTextStream stream(&text);
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(1);

    float t = 0.0;
    if (layer->stride(frame) > 0 && layer->getLastKeyFramePosition(frame) != frame && frame < layer->getMaxKeyFramePosition())
        t = float(layer->inbetweenPosition(frame)) / layer->stride(frame);


    painter.setPen(penForward);
    if (frame >= layer->getMaxKeyFramePosition())
        key = layer->getVectorKeyFrameAtFrame(layer->getMaxKeyFramePosition());
    float angle = key->getFrameRotation(t);
    stream << (angle * 180.0 / M_PI) << "°";
    Point::VectorType textPosition = position + posDeltaForward;
    painter.drawText(textPosition.x(), textPosition.y(), text);

    painter.setPen(penBackward);
    if (t == 0.0 && frame > 1)
        angle = layer->getPrevKey(key)->getFrameRotation(1);
    text.clear();
    stream << (angle * 180.0 / M_PI) << "°";
    text = *(stream.string());
    textPosition = position + posDeltaBackward;
    painter.drawText(textPosition.x(), textPosition.y(), text);
}