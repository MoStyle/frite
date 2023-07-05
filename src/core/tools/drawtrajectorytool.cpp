/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "drawtrajectorytool.h"
#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "gridmanager.h"
#include "canvasscenemanager.h"
#include "canvascommands.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "viewmanager.h"
#include "colormanager.h"
#include "qteigen.h"
#include "utils/geom.h"
#include "trajectory.h"

extern dkFloat k_penSize;
extern dkBool k_drawChain;

DrawTrajectoryTool::DrawTrajectoryTool(QObject *parent, Editor *editor) : TrajectoryTool(parent, editor) {
    m_toolTips = QString("Left-click to draw the trajectory segment");
}

DrawTrajectoryTool::~DrawTrajectoryTool() {

}

Tool::ToolType DrawTrajectoryTool::toolType() const {
    return Tool::DrawTraj;
}

QGraphicsItem *DrawTrajectoryTool::graphicsItem() {
    return nullptr;
}

QCursor DrawTrajectoryTool::makeCursor(float scaling) const {
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

void DrawTrajectoryTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        for (Group *group : keyframe->selection().selectedPostGroups()) group->setShowGrid(on);
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
        m_editor->tabletCanvas()->updateCurrentFrame();
    }
}

void DrawTrajectoryTool::pressed(const EventInfo& info) {
    m_points.clear();
    m_points.push_back(Point::VectorType(info.pos.x(), info.pos.y()));
}

void DrawTrajectoryTool::moved(const EventInfo& info) {
    m_points.push_back(Point::VectorType(info.pos.x(), info.pos.y()));
}

void DrawTrajectoryTool::released(const EventInfo& info) {
    if (m_points.size() < 2) return;

    // fit cubic to m_stroke
    m_cubicApprox.fit(m_points, false);

    // transform fitted cubic control points to align with the current selected trajectory endpoints
    // set this new cubic as the new traj
    Trajectory *traj = info.key->selection().selectedTrajectoryPtr();
    if (traj) {
        m_cubicApprox.fitExtremities(traj->cubicApprox().getP0(), traj->cubicApprox().getP3());
        traj->setCubicApprox(m_cubicApprox);
        // TODO check if already added ?
        int layerIdx = m_editor->layers()->currentLayerIndex();
        int currentFrame = m_editor->playback()->currentFrame();
        m_editor->undoStack()->push(new SyncTrajectoriesCommand(m_editor, layerIdx, currentFrame, info.key->selection().selectedTrajectory(), info.key->selection().selectedTrajectory()->nextTrajectory()));
        if (!info.key->selection().selectedTrajectoryPtr()->hardConstraint()) {
            m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, info.key->selection().selectedTrajectory()));
        }
        info.key->makeInbetweensDirty();
    }

    m_points.clear();
}

void DrawTrajectoryTool::doublepressed(const EventInfo& info) {

}

void DrawTrajectoryTool::wheel(const WheelEventInfo& info) {

}

void DrawTrajectoryTool::draw(QPainter &painter, VectorKeyFrame *key) {
    static QPen pen(QColor(200, 200, 200), 2.0);
    Layer *layer = key->parentLayer();
    int stride = layer->stride(layer->getVectorKeyFramePosition(key));
    pen.setCapStyle(Qt::RoundCap);
    
    drawNonSelectedGroupTraj(painter, pen, key, stride);

    drawSelectedTraj(painter, pen, key, k_drawChain);

    // stroke being drawn
    if (m_points.size() < 2) return;
    pen.setColor(QColor(200, 200, 200, 200));
    pen.setStyle(Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    QPointF prev(m_points.front().x(), m_points.front().y());
    for (int i = 1; i < m_points.size(); i++) {
        QPointF cur(m_points[i].x(), m_points[i].y());
        painter.drawLine(prev, cur);
        prev = cur;
    }
}