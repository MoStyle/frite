/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "handtool.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"

#include "viewmanager.h"
#include "tabletcanvas.h"
#include "arap.h"

HandTool::HandTool(QObject *parent, Editor *editor) : Tool(parent, editor), m_pressed(false) {
    m_toolTips = QString("Left-click to pan | Ctrl+Left-click to rotate the canvas");
}

HandTool::~HandTool() {

}

Tool::ToolType HandTool::toolType() const {
    return Tool::Hand;
}

QCursor HandTool::makeCursor(float scaling) const {
    return m_pressed ? QCursor(Qt::ClosedHandCursor) : QCursor(Qt::OpenHandCursor);
}

void HandTool::pressed(const EventInfo& info) {
    m_pressed = true;
    m_editor->tabletCanvas()->updateCursor();
}

void HandTool::moved(const EventInfo& info) {
    bool isTranslate = !(info.modifiers & Qt::ControlModifier);
    if (isTranslate) {
        QPointF d = info.pos - info.lastPos;
        QPointF offset = m_editor->view()->translation() + d;
        m_editor->view()->translate(offset);
    } else {
        QVector2D prevVec(info.lastPos);
        prevVec.normalize();
        QVector2D currentVec(info.pos);
        currentVec.normalize();
        qreal dotprod = qreal(QVector2D::dotProduct(prevVec, currentVec));
        qreal angle = acos(dotprod) * 180.0 / M_PI;
        dotprod = qreal(QVector2D::dotProduct(QVector2D(-prevVec[1], prevVec[0]), currentVec));
        if (dotprod < 0) angle = 360.0 - angle;
        if (!std::isnan(angle)) m_editor->view()->rotate(angle);
    }
}

void HandTool::released(const EventInfo& info) {
    m_pressed = false;
    m_editor->tabletCanvas()->updateCursor();
}

void HandTool::doublepressed(const EventInfo& info) {
    m_editor->view()->resetView();
}