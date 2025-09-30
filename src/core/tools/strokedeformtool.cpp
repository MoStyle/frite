/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "strokedeformtool.h"
#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "registrationmanager.h"

#include "viewmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "colormanager.h"
#include "qteigen.h"

extern dkFloat k_penSize;
extern dkBool k_drawTargetGrid;

StrokeDeformTool::StrokeDeformTool(QObject *parent, Editor *editor) : 
    Tool(parent, editor),
    m_brush(Qt::black),
    m_pen(m_brush, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
    m_currentStroke(nullptr),
    m_startTime(clock()),
    m_curTime(0),
    m_pressed(false) { 
        m_toolTips = QString("Left-click to draw a guide stroke");
}

StrokeDeformTool::~StrokeDeformTool() {

}

Tool::ToolType StrokeDeformTool::toolType() const {
    return Tool::StrokeDeform;
}

QCursor StrokeDeformTool::makeCursor(float scaling) const {
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

void StrokeDeformTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if (group != nullptr) {
            group->setShowGrid(on);
        }
    }
}

void StrokeDeformTool::pressed(const EventInfo& info) {
    if (!m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) {
        m_pressed = false;
        return;
    }

    m_pen.setWidthF(k_penSize);
    m_pen.setColor(m_editor->color()->frontColor());
    m_currentStroke = std::make_shared<Stroke>(-1, m_editor->color()->frontColor(), k_penSize, false);
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed, 1.0);
    m_currentStroke->addPoint(point);
    m_pressed = true;
}

void StrokeDeformTool::moved(const EventInfo& info) {
    if (!m_pressed || !m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y()))) return;
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed, 1.0);
    m_currentStroke->addPoint(point);
}

void StrokeDeformTool::released(const EventInfo& info) {
    if (!m_pressed || m_currentStroke->size() < 2) return;
    std::vector<Point *> targetPos;
    targetPos.reserve(m_currentStroke->size());
    for (int i = 0; i < m_currentStroke->size(); ++i) targetPos.push_back(m_currentStroke->points()[i]);
    for (Group *group : info.key->selection().selectedPostGroups()) {
        m_editor->registration()->setRegistrationTarget(info.key, targetPos);
        m_editor->registration()->registration(group, TARGET_POS, TARGET_POS, false);
        m_editor->registration()->clearRegistrationTarget();
    }
    info.key->resetTrajectories(true);
    info.key->makeInbetweensDirty();
    m_currentStroke.reset<Stroke>(nullptr);
    m_pressed = false;
}

void StrokeDeformTool::doublepressed(const EventInfo& info) {
    
}

void StrokeDeformTool::wheel(const WheelEventInfo& info) {

}

void StrokeDeformTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    for (Group *group : key->selection().selectedPostGroups()) {
        qreal alphaLinear = m_editor->currentAlpha();

        if (group->lattice()->isArapPrecomputeDirty()) {
            group->lattice()->precompute();
        }

        if (k_drawTargetGrid) {
            float spacing = group->spacingAlpha(1.0);
            int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
            if (group->lattice()->currentPrecomputedTime() != spacing || group->lattice()->isArapInterpDirty()) 
                group->lattice()->interpolateARAP(1.0, 1.0, group->globalRigidTransform(1.0));
            m_editor->updateInbetweens(key, stride, stride);
            group->drawGrid(painter, 0, TARGET_POS);
        }

        group->lattice()->drawPins(painter);
    }

    if (m_currentStroke != nullptr) {
        m_currentStroke->draw(painter, m_pen, 0, m_currentStroke->size() - 1);
    }
}