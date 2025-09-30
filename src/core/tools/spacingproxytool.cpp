/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "spacingproxytool.h"

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

SpacingProxyTool::SpacingProxyTool(QObject* parent, Editor* editor) : ChartTool(parent, editor) {
    m_toolTips = QString("Left-click: move the proxy frame");
    m_chartMode = ChartItem::PROXY;
}

SpacingProxyTool::~SpacingProxyTool() {

}

Tool::ToolType SpacingProxyTool::toolType() const { return Tool::ProxySpacing; }

QCursor SpacingProxyTool::makeCursor(float scaling) const { return QCursor(Qt::ArrowCursor); }

void SpacingProxyTool::tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { 

}

void SpacingProxyTool::tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    if (tick->tickType() != ChartTickItem::TICKPROXY) return;
    // compute mouse delta direction
    ChartItem *chart = tick->chart();
    QVector2D delta(event->pos() - event->lastPos());
    float deltaX = delta.length() / chart->length();
    if (event->pos().x() < event->lastPos().x()) deltaX = -deltaX;
    tick->move(deltaX);
    chart->updateSpacingProxy(event->modifiers() & Qt::ShiftModifier ? ChartItem::INANDOUT : ChartItem::INOROUT);
}

void SpacingProxyTool::tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { 
    if (tick->tickType() != ChartTickItem::TICKPROXY) return;
    tick->chart()->updateSpacingProxy(event->modifiers() & Qt::ShiftModifier ? ChartItem::INANDOUT : ChartItem::INOROUT);
    // emit m_editor->tabletCanvas()->groupChanged(tick->chart()->keyframe()->selectedGroup());
}

void SpacingProxyTool::tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {

}
