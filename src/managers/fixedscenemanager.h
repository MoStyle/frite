/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __FIXEDSCENEMANAGER_H__
#define __FIXEDSCENEMANAGER_H__

#include "basemanager.h"
#include "chartitem.h"

class Editor;
class Tool;

class FixedSceneManager : public BaseManager {
    Q_OBJECT
public:
    FixedSceneManager(Editor *editor);
    virtual ~FixedSceneManager();

    void setScene(QGraphicsScene *scene);

    void updateKeyChart(VectorKeyFrame *keyframe);

public slots:
    void frameChanged(int frame);
    void sceneResized(const QRectF& rect);
    void updateChartMode(ChartItem::ChartMode mode);

private:
    QGraphicsScene *m_scene;

    ChartItem *m_keyChart;

    int lastFrameChange;
};

#endif // __FIXEDSCENEMANAGER_H__