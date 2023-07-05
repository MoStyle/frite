/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __CANVASSCENEMANAGER_H__
#define __CANVASSCENEMANAGER_H__

#include "basemanager.h"

class Tool;
class Trajectory;
class TrajectoryTickItem;
class VectorKeyFrame;

class CanvasSceneManager : public BaseManager {
    Q_OBJECT
public:
    CanvasSceneManager(QObject *pParent);
    virtual ~CanvasSceneManager();

    void setScene(QGraphicsScene *scene);

    void selectedGroupChanged(const QHash<int, Group *> &groups);

    void selectedTrajectoryChanged(Trajectory *trajectory);

public slots:
    void toolChanged(Tool *newTool);
    void frameChanged(int frame);
    void spacingChanged();

private:
    QGraphicsScene *m_scene;

    QGraphicsItem *m_toolItem;
    QGraphicsItem *m_groupSelectionOutline;

    std::vector<TrajectoryTickItem *> m_trajectoryTicks;

    int lastFrameChange;
};

#endif // __CANVASSCENEMANAGER_H__