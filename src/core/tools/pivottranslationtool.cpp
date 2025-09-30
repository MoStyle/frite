/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pivottranslationtool.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"



PivotTranslationTool::PivotTranslationTool(QObject *parent, Editor *editor) : 
    Tool(parent, editor) 
    , m_currentState(TRAJECTORY)
    , m_mouseTranslation(0, 0)
    , m_angle(0.f)
    , m_deltaAngle(0.f)
    , m_pressed(false)
    , m_currentPos(0, 0)
    { }


PivotTranslationTool::~PivotTranslationTool() {

}

Tool::ToolType PivotTranslationTool::toolType() const {
    return Tool::PivotTranslation;
}

QCursor PivotTranslationTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void PivotTranslationTool::pressed(const EventInfo& info){
    if (m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;

    if (info.mouseButton & Qt::LeftButton){
        KeyframedVector * translation = info.key->translation();
        if (info.modifiers & Qt::ShiftModifier){
            m_currentState = MOVE_FIRST_POINT;
            translation->frameChanged(0);
            m_currentPos = translation->get();
            m_mouseTranslation = m_currentPos - Point::VectorType(info.pos.x(), info.pos.y());
            m_deltaAngle = 0.f;
        }
        else{
            m_currentState = TRAJECTORY;
            m_trajectoryPoints.clear();
            m_trajectoryPoints.push_back(Point::VectorType(info.pos.x(), info.pos.y()));
        }
        
    }

    else if (info.mouseButton & Qt::RightButton){
        m_currentState = CONTEXT_MENU;
    }
    m_pressed = true;

}

void PivotTranslationTool::moved(const EventInfo& info){
    if (!m_pressed) return;

    if (m_currentState == TRAJECTORY){
        m_trajectoryPoints.push_back(Point::VectorType(info.pos.x(), info.pos.y()));
    }

    if (m_currentState == MOVE_FIRST_POINT){
        Point::VectorType newPos = m_mouseTranslation + Point::VectorType(info.pos.x(), info.pos.y());
        if(!m_editor->tabletCanvas()->canvasRect().contains(QPoint(newPos.x(), newPos.y()))) return;
        m_currentPos = newPos;
    }

}

void PivotTranslationTool::released(const EventInfo& info){
    if (!m_pressed) return;

    m_pressed = false;

    if (m_currentState == MOVE_FIRST_POINT){
        float t = 0;
        KeyframedVector *translation = info.key->translation();
        translation->frameChanged(t);
        translation->set(m_currentPos);
        translation->addKey("Translation", t);
        m_currentPos = Point::VectorType(0, 0);
    }


    if (m_currentState == TRAJECTORY){
        if (m_trajectoryPoints.size() < 2) return;
        KeyframedVector * translation = info.key->translation();
        Point::VectorType firstPoint = info.key->getCenterOfGravity(REF_POS);
        Point::VectorType lastPoint = info.key->getCenterOfGravity(TARGET_POS);
        
        CompositeBezier2D composite;
        Bezier2D * cubicApprox = new Bezier2D();
        cubicApprox->fit(m_trajectoryPoints, false);
        translation->frameChanged(0);
        Point::VectorType P0 = firstPoint + translation->get();
        translation->frameChanged(1);
        Point::VectorType P3 = lastPoint + translation->get();

        cubicApprox->fitExtremities(P0, P3);
        composite.replaceBezierCurve(cubicApprox, 0);

        std::vector<Point::VectorType> samples;
        int nbSamples = 50;
        float step = composite.sampleArcLength(0, 1, nbSamples, samples);
        translation->removeKeys("Translation");
        for (int i = 0; i < nbSamples; ++i){
            float t = float(i) * step;
            Point::VectorType toCenterOfGravity = firstPoint * (1 - t) + lastPoint * t;
            translation->set(samples[i] - toCenterOfGravity);
            translation->addKey("Translation", t);
        }
        m_trajectoryPoints.clear();
    }
    info.key->makeInbetweensDirty();
}



void PivotTranslationTool::drawUI(QPainter &painter, VectorKeyFrame *key) {

    static QPen pen(QColor(200, 200, 200), 2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(10);
    painter.setPen(pen);


    // draw coordinate system 
    int currentFrame = m_editor->playback()->currentFrame();
    int stride = m_editor->layers()->currentLayer()->stride(currentFrame);
    int inbetweenPosition = m_editor->layers()->currentLayer()->inbetweenPosition(currentFrame);
    float t = float(inbetweenPosition) / stride;

    Point::VectorType firstPoint = key->getCenterOfGravity(REF_POS);
    Point::VectorType lastPoint = key->getCenterOfGravity(TARGET_POS);
    KeyframedReal * rotation = key->rotation();
    KeyframedVector * translation = key->translation();
    rotation->frameChanged(t);
    translation->frameChanged(t);
    Point::VectorType position = firstPoint * (1 - t) + lastPoint * t + translation->get();
    float angle = rotation->get();
    painter.drawPoint(position.x(), position.y());
    
    rotation->frameChanged(0);
    translation->frameChanged(0);
    if (m_currentState == MOVE_FIRST_POINT && m_pressed)
        position = firstPoint + m_currentPos;
    else
        position = firstPoint + translation->get();    
    angle = rotation->get();
    painter.drawPoint(position.x(), position.y());

    rotation->frameChanged(1);
    translation->frameChanged(1);
    position = lastPoint + translation->get();  
    angle = rotation->get();
    painter.drawPoint(position.x(), position.y());

    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);
    painter.setPen(pen);
    drawTrajectory(painter, key);

    if (m_trajectoryPoints.size() >= 2){
        QPointF prev(m_trajectoryPoints.front().x(), m_trajectoryPoints.front().y());
        for (int i = 1; i < m_trajectoryPoints.size(); i++) {
            QPointF cur(m_trajectoryPoints[i].x(), m_trajectoryPoints[i].y());
            painter.drawLine(prev, cur);
            prev = cur;
        }
    }

}

void PivotTranslationTool::drawTrajectory(QPainter &painter, VectorKeyFrame * keyFrame){
    static const int samples = 100;
    Point::VectorType prev, cur;
    KeyframedVector * translation = keyFrame->translation();
    QPainterPath path;

    Point::VectorType firstPoint = keyFrame->getCenterOfGravity(REF_POS);
    Point::VectorType lastPoint = keyFrame->getCenterOfGravity(TARGET_POS);
    
    translation->frameChanged(0);
    cur = translation->get() + firstPoint;
    path.moveTo(cur.x(), cur.y());
    for (int i = 1; i < samples; i++){
        float t = float(i) / (samples - 1);
        Point::VectorType point = firstPoint * (1 - t) + lastPoint * t;
        translation->frameChanged(t);
        prev = cur;
        cur = point + translation->get();
        path.lineTo(cur.x(), cur.y());
    }
    painter.drawPath(path);
}