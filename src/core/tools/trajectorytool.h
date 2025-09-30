/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __TRAJECTORYTOOL_H__
#define __TRAJECTORYTOOL_H__

#include "tool.h"
#include "keyframedparams.h"
#include "trajectory.h"

#include <chrono>

class Group;
class Layer;

class TrajectoryTool : public Tool {
    Q_OBJECT
public:
    TrajectoryTool(QObject *parent, Editor *editor);
    virtual ~TrajectoryTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;

signals:
    void showKeyframedVectorCurves(KeyframedVector *curves);

protected:
    void drawNonSelectedGroupTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, int stride);
    void drawSelectedTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, bool drawFullPath);

private:
    std::shared_ptr<Trajectory> pickInGrids(VectorKeyFrame *key, qreal alpha, int inbetween, int layerIdx, int currentFrame, Point::VectorType pos, bool setSelection=true);
    void propagateTrajectoryForward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos);
    void propagateTrajectoryBackward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos);
    void moveLatticesTargetConfiguration();
    QPainterPath applyRigidTransformToPathFromTraj(Trajectory * traj);

    bool m_tickPressed, m_tangentControlPressed;
    int m_tickPressedIdx, m_tangentControlPressedIdx;
    
    std::chrono::system_clock::time_point m_lastMoveTick;

    // tmp storage for visualizing a trajectory embedded in multiple lattices
    std::vector<std::shared_ptr<Trajectory>> m_trajectories;
};

#endif // __TRAJECTORYTOOL_H__