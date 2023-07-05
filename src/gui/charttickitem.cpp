/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "charttickitem.h"
#include "chartitem.h"
#include "animationcurve.h"
#include "toolsmanager.h"
#include "editor.h"
#include "tools/spacingtool.h"

#include <QGraphicsSceneMouseEvent>
#include <QVector2D>
#include <QPointF>

int ChartTickItem::HEIGHT = 25;
int ChartTickItem::WIDTH = 6;
QColor ChartTickItem::colors[] = {Qt::black, QColor(0, 0, 128, 50)};
static const int CONTROL_POINTS = 4; 

ChartTickItem::ChartTickItem(ChartItem *chart, TickType type, int idx, float x, float y, float xVal, int pointIdx, bool fix) : QGraphicsRectItem() {
    m_chart = chart;
    m_type = type;
    m_idx = idx;
    m_pointIdx = pointIdx;
    m_fix = fix;
    m_x = xVal;
    m_width = WIDTH;
    m_height = (idx == 0 || idx == (chart->nbTicks() - 1)) ? HEIGHT : HEIGHT / 1.5f;
    setRect(x + m_x * m_chart->length(), y, m_width, m_height);
    float margin = m_width - 2;
    m_renderRect = rect() - QMarginsF(margin, 0, margin, 0);
    m_spacingTool = nullptr;
    updatePos();
    if (m_fix && idx != 0 && idx != (chart->nbTicks() - 1)) {
        // setAcceptHoverEvents(true);
        m_color = colors[1];
    } else {
        m_color = colors[0];
    }
    setPen(QPen(m_color));
    setBrush(m_color);
}

void ChartTickItem::updatePos() {
    int x = m_chart->pos().x();
    int y = m_chart->pos().y();
    setRect(x + m_x * m_chart->length(), y + (HEIGHT - m_height) / 2.0f, m_width, m_height);
    float margin = m_width - 2;
    m_renderRect = rect() - QMarginsF(margin, 0, margin, 0);
}

void ChartTickItem::move(float delta) {
    m_x += delta;
    m_x = std::max(m_chart->controlTickAt(m_idx - 1)->xVal() + 1e-5f, m_x);
    m_x = std::min(m_chart->controlTickAt(m_idx + 1)->xVal() - 1e-5f, m_x);
    updatePos();
}

void ChartTickItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    setBrush(QColor(78, 78, 78));
    setPen(QPen(QColor(78, 78, 78)));
    m_chart->update();
    m_spacingTool = nullptr;
    Tool *tool = m_chart->editor()->tools()->currentTool();
    if (tool != nullptr && tool->isSpacingTool()) {
        m_spacingTool = dynamic_cast<SpacingTool *>(tool);
        if (m_spacingTool != nullptr) m_spacingTool->tickPressed(event, this);
    }
    event->accept();
}

void ChartTickItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_fix) {
        event->accept();
        return; 
    }
    if (m_spacingTool != nullptr) m_spacingTool->tickMoved(event, this);
    event->accept();
}

void ChartTickItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    setBrush(m_color);
    setPen(QPen(m_color));
    if (m_fix) return;
    if (m_spacingTool != nullptr) m_spacingTool->tickReleased(event, this);
    event->accept();
}

void ChartTickItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    Tool *tool = m_chart->editor()->tools()->currentTool();
    if (tool != nullptr && tool->isSpacingTool()) {
        m_spacingTool = dynamic_cast<SpacingTool *>(tool);
        if (m_spacingTool != nullptr) m_spacingTool->tickDoubleClick(event, this);
        m_spacingTool = nullptr;
    }
    event->accept();
}

void ChartTickItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "hover";
    // setPen(QPen(QColor(50, 50, 50)));
    // setBrush(QColor(50, 50, 50));
    // update();
    event->accept();
}

void ChartTickItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "hover leave";
    // setBrush(Qt::black);
    // setPen(QPen(Qt::black));
    // update();
    event->accept();
}

void ChartTickItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawRect(m_renderRect);
}

void ChartTickItem::setDichotomicRight(int n) {
    unsigned int nbTicks = m_chart->nbTicks();
    float val = m_x;
    for (int i = m_idx - 1; i > 0; --i) {
        val *= 0.5f;
        m_chart->controlTickAt(i)->setXVal(val);
    }
    m_chart->updateSpacing(1, true);
}

void ChartTickItem::setDichotomicLeft(int n) {
    unsigned int nbTicks = m_chart->nbTicks();
    float val = (1.0f - m_x);
    for (int i = m_idx + 1; i < nbTicks - 1; ++i) {
        val *= 0.5f;
        m_chart->controlTickAt(i)->setXVal(1.0f - val);
    }
    m_chart->updateSpacing(1, true);
}