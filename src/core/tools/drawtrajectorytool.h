/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __DRAWTRAJECTORYTOOL_H__
#define __DRAWTRAJECTORYTOOL_H__

#include "trajectorytool.h"
#include "bezier2D.h"

class Group;

class DrawTrajectoryTool : public TrajectoryTool {
    Q_OBJECT
public:
    DrawTrajectoryTool(QObject *parent, Editor *editor);
    virtual ~DrawTrajectoryTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;

private:
    std::vector<Point::VectorType> m_points;
    Bezier2D m_cubicApprox;
};

#endif // __DRAWTRAJECTORYTOOL_H__