/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "spacingtool.h"

#include "fixedscenemanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "layer.h"
#include"vectorkeyframe.h"

void SpacingTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);
    m_editor->fixedScene()->updateKeyChart(keyframe); // trigger update of timing chart (hide/show)
}