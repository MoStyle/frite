/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pentool.h"

#include "editor.h"
#include "colormanager.h"
#include "stroke.h"
#include "tabletcanvas.h"
#include "utils/geom.h"

dkFloat k_penSize("Pen->Size", 6, 1, 2000, 1);
dkFloat k_penFalloffMin("Pen->Weight falloff min bound", 0.1, 0.05, 1.0, 0.05);

PenTool::PenTool(QObject *parent, Editor *editor) : 
    Tool(parent, editor),
    m_brush(Qt::black),
    m_pen(m_brush, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
    m_currentStroke(nullptr),
    m_startTime(clock()),
    m_curTime(0),
    m_pressed(false) { 
    m_toolTips = QString("Left-click to draw");
}

PenTool::~PenTool() {

}

Tool::ToolType PenTool::toolType() const {
    return Tool::Pen;
}

QCursor PenTool::makeCursor(float scaling) const {
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

void PenTool::pressed(const EventInfo& info) {
    if (!m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y())) || !(info.mouseButton & Qt::LeftButton)) {
        m_pressed = false;
        return;
    }
    m_pen.setWidthF(k_penSize);
    m_pen.setColor(m_editor->color()->frontColor());
    m_currentStroke = std::make_shared<Stroke>(info.key->pullMaxStrokeIdx(), m_editor->color()->frontColor(), k_penSize, false);
    addPoint(info);
    m_pressed = true;
}

void PenTool::moved(const EventInfo& info) {
    if (!m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y())) || info.pos == info.lastPos || info.pressure == 0.0 || !(info.mouseButton & Qt::LeftButton)) return;
    addPoint(info);
}

void PenTool::released(const EventInfo& info){
    if (!m_pressed) return;

    if (m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y())) && info.pos != info.lastPos) {
        addPoint(info);
    }

    if (m_currentStroke->size() >= 2 && m_currentStroke->length() > 1e-3) { // TODO adaptative constant based on current view?
        m_editor->addStroke(m_currentStroke);
    }

    if (QOpenGLContext::currentContext() != m_editor->tabletCanvas()->context()) m_editor->tabletCanvas()->makeCurrent();
    m_currentStroke->destroyBuffers();
    m_currentStroke.reset<Stroke>(nullptr);
    m_pressed = false;
}

void PenTool::wheel(const WheelEventInfo& info) {
    if (info.delta > 0) k_penSize.setValue(k_penSize + 0.5);
    else                k_penSize.setValue(k_penSize - 0.5);
    m_editor->tabletCanvas()->updateCursor();
}

void PenTool::addPoint(const EventInfo &info) {
    m_curTime = clock();
    m_currentStroke->addPoint(new Point(info.pos.x(), info.pos.y(), double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC),  Geom::smoothconc(info.pressure) * (1.0f - k_penFalloffMin) + k_penFalloffMin));
}