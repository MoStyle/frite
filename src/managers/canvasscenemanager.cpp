/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "canvasscenemanager.h"

#include "tools/tool.h"
#include "canvassceneitems.h"
#include "layermanager.h"
#include "chartitem.h"
#include "playbackmanager.h"
#include "trajectory.h"
#include "trajectorytickitem.h"
#include "group.h"

CanvasSceneManager::CanvasSceneManager(QObject *pParent) 
    : BaseManager(pParent),
      m_toolItem(nullptr),
      m_groupSelectionOutline(nullptr),
      lastFrameChange(-1) {
}

CanvasSceneManager::~CanvasSceneManager() {

}

void CanvasSceneManager::setScene(QGraphicsScene *scene) { 
    m_scene = scene; 
}

void CanvasSceneManager::toolChanged(Tool *newTool) {
    if (newTool == nullptr) qWarning() << "new tool is null";
    
    QGraphicsItem *toolItem = newTool->graphicsItem();
    if (newTool->graphicsItem() == nullptr) return;

    if (m_toolItem != nullptr) m_scene->removeItem(m_toolItem);
    m_toolItem = newTool->graphicsItem();
    m_scene->addItem(m_toolItem);
}

void CanvasSceneManager::selectedGroupChanged(const QHash<int, Group *> &groups) {
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);

    // do not change the scene if the selected groups are not in the currently displayed keyframe
    if (!groups.empty() && groups.constBegin().value()->getParentKeyframe() != key) return;

    if (m_groupSelectionOutline != nullptr) {
        m_scene->removeItem(m_groupSelectionOutline);
        delete m_groupSelectionOutline;
        m_groupSelectionOutline = nullptr;
    }

    if (groups.empty()) return;
    // m_groupSelectionOutline = new GroupSelectionOutline(groups);
    // m_scene->addItem(m_groupSelectionOutline);
}

void CanvasSceneManager::selectedTrajectoryChanged(Trajectory *trajectory) {
    for (auto trajTick : m_trajectoryTicks) {
        m_scene->removeItem(trajTick);
        delete trajTick;
    }
    m_trajectoryTicks.clear();

    if (trajectory == nullptr || !trajectory->hardConstraint()) {
        return;
    }

    Layer *layer = trajectory->group()->getParentKeyframe()->parentLayer();
    int currentFrame = layer->getVectorKeyFramePosition(trajectory->group()->getParentKeyframe());
    int stride = layer->stride(currentFrame);
    // for (int i = 1; i < stride; ++i) {
    //     m_trajectoryTicks.push_back(new TrajectoryTickItem(trajectory, (float)i/(float)stride, i));
    //     m_scene->addItem(m_trajectoryTicks.back());
    // }        
}

void CanvasSceneManager::frameChanged(int frame) {
    Layer *layer = m_editor->layers()->currentLayer();
    // exit if frame is an inbetween
    if (layer->getLastKeyFramePosition(frame) != layer->getLastKeyFramePosition(m_editor->playback()->currentFrame())) return;
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(frame, 0);
    selectedTrajectoryChanged(key->selection().selectedTrajectoryPtr());

    if (lastFrameChange == -1 || layer->getLastKeyFramePosition(frame) == lastFrameChange) {
        lastFrameChange = layer->getLastKeyFramePosition(frame);
        return;
    }

    // new KF, we reset the scene
    selectedGroupChanged(key->selection().selectedPostGroups());
    lastFrameChange = layer->getLastKeyFramePosition(frame);
}

void CanvasSceneManager::spacingChanged() {
    if (m_trajectoryTicks.empty()) return;
    for (auto traj : m_trajectoryTicks) {
        traj->updatePos();
    }
}