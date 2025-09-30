/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "tangenttool.h"


#include "playbackmanager.h"
#include "layermanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"

extern dkBool k_drawChain;

TangentTool::TangentTool(QObject *parent, Editor *editor) : TrajectoryTool(parent, editor) {
    m_toolTips = QString("Left-click on tangent vectors to edit them");
}

TangentTool::~TangentTool() {

}

Tool::ToolType TangentTool::toolType() const {
    return Tool::TrajTangent;
}

QCursor TangentTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void TangentTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        for (Group *group : keyframe->selection().selectedPostGroups()) group->setShowGrid(on);
        m_editor->tabletCanvas()->updateCurrentFrame();
    }
}

void TangentTool::pressed(const EventInfo& info) {
    m_p1Pressed = false;
    m_p2Pressed = false;

    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;

    if (info.key->selection().selectedTrajectoryPtr() == nullptr || !info.key->selection().selectedTrajectory()->hardConstraint())
        return;

    Trajectory *selectedTraj = info.key->selection().selectedTrajectoryPtr();
    Point::VectorType pos(info.pos.x(), info.pos.y());
    Point::VectorType p1 = selectedTraj->cubicApprox().getP1();
    Point::VectorType p2 = selectedTraj->cubicApprox().getP2();

    if ((pos - p1).norm() < 2.0) {
        m_p1Pressed = true;
        return;
    }

    if ((pos - p2).norm() < 2.0) {
        m_p2Pressed = true;
        return;
    }
}

void TangentTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;

    Trajectory *selectedTraj = info.key->selection().selectedTrajectoryPtr();
    Point::VectorType pos(info.pos.x(), info.pos.y());

    if (m_p1Pressed) {
        selectedTraj->setP1(pos);
        // sync with prev tangent if it is connected
        if (selectedTraj->prevTrajectory() && selectedTraj->syncPrev()) {
            Point::VectorType t = selectedTraj->cubicApprox().getP1() - selectedTraj->cubicApprox().getP0(); 
            selectedTraj->prevTrajectory()->setP2(selectedTraj->prevTrajectory()->cubicApprox().getP3() - t);
            selectedTraj->prevTrajectory()->keyframe()->makeInbetweensDirty();
        }
    }

    if (m_p2Pressed) {
        selectedTraj->setP2(pos);
        // sync with next tangent if it is connected
        if (selectedTraj->nextTrajectory() && selectedTraj->syncNext()) {
            Point::VectorType t = selectedTraj->cubicApprox().getP2() - selectedTraj->cubicApprox().getP3(); 
            selectedTraj->nextTrajectory()->setP1(selectedTraj->nextTrajectory()->cubicApprox().getP0() - t);
            selectedTraj->nextTrajectory()->keyframe()->makeInbetweensDirty();
        }
    }

    if (m_p1Pressed || m_p2Pressed) {
        info.key->makeInbetweensDirty();
    }
}

void TangentTool::released(const EventInfo& info) {
    m_p1Pressed = false;
    m_p2Pressed = false;

    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;
}

void TangentTool::doublepressed(const EventInfo& info) {

}

void TangentTool::wheel(const WheelEventInfo& info) {

}

void TangentTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    static QPen pen(QColor(200, 200, 200), 2.0);
    Layer *layer = key->parentLayer();
    int stride = layer->stride(layer->getVectorKeyFramePosition(key));
    pen.setCapStyle(Qt::RoundCap);
    
    drawNonSelectedGroupTraj(painter, pen, key, stride);

    drawSelectedTraj(painter, pen, key, k_drawChain);

    // TODO: cleanup + use graphics item
    // draw selected trajectory's tangents 
    Trajectory *selectedTraj = key->selection().selectedTrajectoryPtr();
    if (selectedTraj && selectedTraj->hardConstraint()) {
        // draw tangents
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 0, 0));
        Point::VectorType p0 = selectedTraj->cubicApprox().getP0();
        Point::VectorType p1 = selectedTraj->cubicApprox().getP1();
        Point::VectorType p2 = selectedTraj->cubicApprox().getP2();
        Point::VectorType p3 = selectedTraj->cubicApprox().getP3();
        painter.fillRect(QRectF(p1.x() - 2.0, p1.y() - 2.0, 4.0, 4.0), QBrush(Qt::black));
        painter.fillRect(QRectF(p2.x() - 2.0, p2.y() - 2.0, 4.0, 4.0), QBrush(Qt::black));
        painter.setPen(QColor(40, 0, 0, 40));
        painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
        painter.drawLine(QPointF(p3.x(), p3.y()), QPointF(p2.x(), p2.y()));
    }
}