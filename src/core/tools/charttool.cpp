/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "charttool.h"

#include "fixedscenemanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "tabletcanvas.h"
#include "layer.h"
#include"vectorkeyframe.h"

void ChartTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);
    m_editor->tabletCanvas()->fixedCanvasView()->setAttribute(Qt::WA_TransparentForMouseEvents, !on);
    m_editor->fixedScene()->updateChartMode(m_chartMode);
    m_editor->fixedScene()->updateKeyChart(keyframe); // trigger update of timing chart (hide/show)
}