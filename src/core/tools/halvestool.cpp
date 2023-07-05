/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "halvestool.h"

#include "group.h"
#include "editor.h"
#include "chartitem.h"
#include "charttickitem.h"
#include "fixedscenemanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "layer.h"

#include <QGraphicsSceneMouseEvent>

HalvesTool::HalvesTool(QObject* parent, Editor* editor) : SpacingTool(parent, editor) {
    m_spacingTool = true;
    m_toolTips = QString("Left-click: move a single frame and set halves pattern on the left side | Right-click: move a single frame and set halves pattern on the right side");
}

HalvesTool::~HalvesTool() {

}

Tool::ToolType HalvesTool::toolType() const { return Tool::Halves; }

QGraphicsItem* HalvesTool::graphicsItem() { return nullptr; }

QCursor HalvesTool::makeCursor(float scaling) const { return QCursor(Qt::ArrowCursor); }

void HalvesTool::tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { 
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

void HalvesTool::tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    // compute mouse delta direction
    ChartItem *chart = tick->chart();
    QVector2D delta(event->pos() - event->lastPos());
    float deltaX = delta.length() / chart->length();
    if (event->pos().x() < event->lastPos().x()) deltaX = -deltaX;
    int idx = tick->idx();

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
    chart->updateSpacing(1, true);
}

void HalvesTool::tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { 
    if (event->button() & Qt::RightButton) {
        tick->setDichotomicLeft();
    } else {
        tick->setDichotomicRight();
    }
}

void HalvesTool::tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {

}
