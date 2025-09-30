/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "moveframestool.h"

#include "group.h"
#include "editor.h"
#include "chartitem.h"
#include "charttickitem.h"
#include "tabletcanvas.h"
#include "fixedscenemanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "layer.h"
#include "dialsandknobs.h"

#include <QGraphicsSceneMouseEvent>

static dkBool k_relative("Debug->MoveFrames->Relative", true);

MoveFramesTool::MoveFramesTool(QObject* parent, Editor* editor) : ChartTool(parent, editor) {
    m_toolTips = QString("Left-click: move a single frame | Ctrl+Left-click: expand/contract frames | Shift+Left-click: move all frames");
}

MoveFramesTool::~MoveFramesTool() {

}

Tool::ToolType MoveFramesTool::toolType() const { return Tool::MoveFrames; }

QCursor MoveFramesTool::makeCursor(float scaling) const { return QCursor(Qt::ArrowCursor); }

void MoveFramesTool::tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { 
    m_offsetLeft.clear();
    m_offsetRight.clear();
    ChartItem *chart = tick->chart();
    float xVal = tick->xVal();
    for (int i = 1; i < tick->idx(); ++i) {
        m_offsetLeft.push_back(chart->controlTickAt(i)->xVal() / xVal);
    }
    for (int i = tick->idx() + 1; i < chart->nbTicks() - 1; ++i) {
        m_offsetRight.push_back((chart->controlTickAt(i)->xVal() - xVal) / (1.0f - xVal));
    }
}

void MoveFramesTool::tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    // compute mouse delta direction
    ChartItem *chart = tick->chart();
    QVector2D delta(event->pos() - event->lastPos());
    float deltaX = delta.length() / chart->length();
    if (event->pos().x() < event->lastPos().x()) deltaX = -deltaX;
    int idx = tick->idx();

    if (event->modifiers() & Qt::ControlModifier) {     // ease-in/ease-out (dilate/contract ticks around the mouse position)
        if (idx > 0 && !chart->controlTickAt(idx - 1)->fixed()) {
            chart->controlTickAt(idx - 1)->move(-deltaX);
            for (int i = 1; i < tick->idx() - 1; ++i) {  
                if (!chart->controlTickAt(i)->fixed()) {
                    float newOffset = m_offsetLeft[i - 1] * (1.0 / m_offsetLeft[idx - 2]);
                    chart->controlTickAt(i)->setXVal(chart->controlTickAt(idx - 1)->xVal() * newOffset);
                }
            }
        }
        if (idx < chart->nbTicks() - 2 && !chart->controlTickAt(idx + 1)->fixed()) {
            chart->controlTickAt(idx + 1)->move(deltaX);
            for (int i = tick->idx() + 2; i < chart->nbTicks() - 1; ++i) {  
                if (!chart->controlTickAt(i)->fixed()) {
                    float newOffset = (1.0 - m_offsetLeft[i - tick->idx() - 1]) * (1.0 / (1.0 - m_offsetLeft[0]));
                    chart->controlTickAt(i)->setXVal(std::min((1.0 - chart->controlTickAt(idx + 1)->xVal()) * m_offsetRight[i - tick->idx() - 1] + chart->controlTickAt(idx + 1)->xVal(), 1.0));
                }
            }
        }
        chart->updateSpacing(1, true);
    } else if(event->modifiers() & Qt::ShiftModifier) { // move a single tick
        if (!k_relative) {
            for (int i = 0; i < chart->nbTicks(); ++i) {  // move all ticks in the same direction
                if (!chart->controlTickAt(i)->fixed()) {
                    chart->controlTickAt(i)->move(deltaX);
                }
            }
        } else {
            tick->move(deltaX);
            float xVal = tick->xVal();
            // move all ticks but preserve distribution relative to the selected tick and boundaries 
            for (int i = 1; i < tick->idx(); ++i) {  
                if (!chart->controlTickAt(i)->fixed()) {
                    chart->controlTickAt(i)->setXVal(xVal * m_offsetLeft[i - 1]);
                }
            }
            for (int i = tick->idx() + 1; i < chart->nbTicks() - 1; ++i) {  
                if (!chart->controlTickAt(i)->fixed()) {
                    chart->controlTickAt(i)->setXVal(std::min((1.0f - xVal) * m_offsetRight[i - tick->idx() - 1] + xVal, 1.0f));
                }
            }
        }
        chart->updateSpacing(1, true);
    } else {
        tick->move(deltaX);
        chart->updateSpacing(idx);
    }
}

void MoveFramesTool::tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { 
    tick->chart()->updateSpacing(tick->idx());
    // emit m_editor->tabletCanvas()->groupChanged(tick->chart()->keyframe()->selectedGroup());
}

void MoveFramesTool::tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    if (event->button() & Qt::LeftButton) {
        tick->chart()->resetControlTicks();
    } else {
        tick->chart()->spacing()->smoothTangents();
        emit m_editor->tabletCanvas()->groupChanged(tick->chart()->keyframe()->selectedGroup());
        // tick->chart()->updateSpacing(tick->idx());
    }
}
