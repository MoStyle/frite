/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "maskpentool.h"

#include "stroke.h"
#include "editor.h"
#include "colormanager.h"
#include "tabletcanvas.h"
#include "playbackmanager.h"

extern dkFloat k_penSize;
extern dkFloat k_penFalloffMin;

// TODO: ignore stroke outside of the mask

MaskPenTool::MaskPenTool(QObject *parent, Editor *editor) : PenTool(parent, editor) { 
    m_toolTips = QString("Left-click: draw new mask contour");
}


MaskPenTool::~MaskPenTool() {

}

Tool::ToolType MaskPenTool::toolType() const {
    return Tool::MaskPen;
}

void MaskPenTool::toggled(bool on) {
    Tool::toggled(on);
    m_editor->tabletCanvas()->setMaskOcclusionMode(on ? TabletCanvas::MaskGrayOut : TabletCanvas::MaskOcclude);
    m_editor->tabletCanvas()->setDisplayMask(on);
    m_editor->tabletCanvas()->setDisplaySelectedGroupsLifetime(!on);
    m_editor->tabletCanvas()->setDisplayDepth(on);
}

void MaskPenTool::pressed(const EventInfo& info) {
    if (!m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y())) || !(info.mouseButton & Qt::LeftButton) || info.key->selection().selectionEmpty()) {
        m_pressed = false;
        return;
    }
    m_pen.setWidthF(k_penSize);
    m_pen.setColor(m_editor->color()->frontColor());
    m_currentStroke = std::make_shared<Stroke>(info.key->pullMaxStrokeIdx(), Qt::darkRed, 1.0f, true);
    m_curTime = clock();
    double timeElapsed = double(m_curTime - m_startTime) / double(CLOCKS_PER_SEC);
    Point *point = new Point(info.pos.x(), info.pos.y(), timeElapsed, Geom::smoothstep(info.pressure) * (1.0f - k_penFalloffMin) + k_penFalloffMin);
    m_currentStroke->addPoint(point);
    m_pressed = true;
}


void MaskPenTool::released(const EventInfo& info) {
    if (!m_pressed) return;

    if (m_editor->tabletCanvas()->canvasRect().contains(QPoint(info.pos.x(), info.pos.y())) && info.pos != info.lastPos) {
        addPoint(info);
    }

    Group *group = info.key->selectedGroup();
    double alpha = m_editor->alpha(m_editor->playback()->currentFrame());
    if (group->lattice()->isArapPrecomputeDirty()) group->lattice()->precompute();
    if (group->lattice()->currentPrecomputedTime() != alpha) group->lattice()->interpolateARAP(alpha, group->spacingAlpha(alpha), info.key->rigidTransform(alpha));
    if (m_currentStroke->size() >= 2 && m_currentStroke->length() > 1e-3 && group->lattice()->intersects(m_currentStroke.get(), 0, m_currentStroke->size() - 1, INTERP_POS)) { // TODO adaptative constant based on current view?
        m_editor->addStroke(m_currentStroke);
    }

    if (QOpenGLContext::currentContext() != m_editor->tabletCanvas()->context()) m_editor->tabletCanvas()->makeCurrent();
    m_currentStroke->destroyBuffers();
    m_currentStroke.reset<Stroke>(nullptr);
    m_pressed = false;
}