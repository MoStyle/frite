/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "drawendkeyframetool.h"

#include "editor.h"
#include "colormanager.h"
#include "stroke.h"
#include "tabletcanvas.h"
#include "utils/geom.h"

static dkFloat k_penSize("Draw end keyframe->Size", 6, 1, 2000, 1);
static dkFloat k_penFalloffMin("Draw end keyframe->Weight falloff min bound", 0.3, 0.1, 1.0, 0.05);

DrawEndKeyframeTool::DrawEndKeyframeTool(QObject *parent, Editor *editor) : 
    Tool(parent, editor),
    m_brush(Qt::black),
    m_pen(m_brush, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
    m_currentStroke(nullptr),
    m_startTime(clock()),
    m_curTime(0),
    m_pressed(false) { 
    m_toolTips = QString("Left-click to draw in the selected end keyframe");
}

DrawEndKeyframeTool::~DrawEndKeyframeTool() {

}

Tool::ToolType DrawEndKeyframeTool::toolType() const {
    return Tool::DrawEndKeyframe;
}

QCursor DrawEndKeyframeTool::makeCursor(float scaling) const {
    int size = k_penSize * scaling;
    size /= 1.5f;
    if (size < 1) size = 1;
    QPixmap pixmap(size, size);
    if (!pixmap.isNull()) {
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing, true);
        painter.setBrush(m_editor->color()->frontColor());
        painter.setPen(Qt::transparent);
        painter.drawEllipse(QRectF(0, 0, size, size));
    }
    return QCursor(pixmap);
}

void DrawEndKeyframeTool::pressed(const EventInfo& info) {
    if (!m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) {
        m_pressed = false;
        return;
    }

    m_pen.setWidthF(k_penSize);
    m_pen.setColor(m_editor->color()->frontColor());
    m_currentStroke = std::make_shared<Stroke>(info.key->pullMaxStrokeIdx(), m_editor->color()->frontColor(), k_penSize, false);
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed, Geom::smoothstep(info.pressure) * (1.0f - k_penFalloffMin) + k_penFalloffMin);
    m_currentStroke->addPoint(point);
    m_pressed = true;
}

void DrawEndKeyframeTool::moved(const EventInfo& info) {
    if (!m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed,  Geom::smoothstep(info.pressure) * (1.0f - k_penFalloffMin) + k_penFalloffMin);
    m_currentStroke->addPoint(point);
}

void DrawEndKeyframeTool::released(const EventInfo& info){
    if (!m_pressed || m_currentStroke->size() < 2) return;
    if (info.key->selection().selectedPreGroups().empty()) return;
    m_editor->addEndStroke(m_currentStroke);
    m_currentStroke.reset<Stroke>(nullptr);
    m_pressed = false;
}