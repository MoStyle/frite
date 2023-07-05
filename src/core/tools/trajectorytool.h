/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __TRAJECTORYTOOL_H__
#define __TRAJECTORYTOOL_H__

#include "tool.h"
#include "keyframedparams.h"
#include "trajectory.h"

class Group;
class Layer;

class TrajectoryTool : public Tool {
    Q_OBJECT
public:
    TrajectoryTool(QObject *parent, Editor *editor);
    virtual ~TrajectoryTool();

    Tool::ToolType toolType() const override;

    QGraphicsItem *graphicsItem() override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void draw(QPainter &painter, VectorKeyFrame *key) override;

signals:
    void showKeyframedVectorCurves(KeyframedVector *curves);

protected:
    void drawNonSelectedGroupTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, int stride);
    void drawSelectedTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, bool drawFullPath);

private:
    std::shared_ptr<Trajectory> pickInGrids(VectorKeyFrame *key, float alpha, int inbetween, int layerIdx, int currentFrame, Point::VectorType pos, bool setSelection=true);
    void propagateTrajectoryForward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos);
    void propagateTrajectoryBackward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos);
    void moveLatticesTargetConfiguration();

    bool m_tickPressed;
    int m_tickPressedIdx;

    // tmp storage for visualizing a trajectory embedded in multiple lattices
    std::vector<std::shared_ptr<Trajectory>> m_trajectories;
};

#endif // __TRAJECTORYTOOL_H__