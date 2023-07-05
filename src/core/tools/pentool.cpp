/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pentool.h"

#include "editor.h"
#include "colormanager.h"
#include "stroke.h"
#include "tabletcanvas.h"
#include "utils/geom.h"

#ifdef Q_OS_MAC
extern "C" {
void disableCoalescing();
void enableCoalescing();
}
#else
extern "C" {
void disableCoalescing() {}
void enableCoalescing() {}
}
#endif

dkFloat k_penSize("Pen->Size", 6, 1, 2000, 1);
dkFloat k_penFalloffMin("Pen->Weight falloff min bound", 0.3, 0.1, 1.0, 0.05);

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

QGraphicsItem *PenTool::graphicsItem() {
    return nullptr;
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
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed, Geom::smoothstep(info.pressure) * (1.0f - k_penFalloffMin) + k_penFalloffMin);
    m_currentStroke->addPoint(point);
    disableCoalescing();
    m_pressed = true;
}

void PenTool::moved(const EventInfo& info) {
    if (!m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y())) || info.pos == info.lastPos || !(info.mouseButton & Qt::LeftButton)) return;
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed,  Geom::smoothstep(info.pressure) * (1.0f - k_penFalloffMin) + k_penFalloffMin);
    // if (isnan((double)point->pos().x()) || isnan((double)point->pos().y())) qDebug() << "Error: Nan in PenTool";
    m_currentStroke->addPoint(point);
}

void PenTool::released(const EventInfo& info){
    if (!m_pressed || m_currentStroke->size() < 2 || m_currentStroke->length() < 1e-3 || !(info.mouseButton & Qt::LeftButton)) return;
    m_editor->addStroke(m_currentStroke);
    m_currentStroke.reset<Stroke>(nullptr);
    enableCoalescing();
    m_pressed = false;
}