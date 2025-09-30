/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "visibilitytool.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "visibilitymanager.h"
#include "canvascommands.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "viewmanager.h"

VisibilityTool::VisibilityTool(QObject *parent, Editor *editor) : PickTool(parent, editor) {
    m_toolTips = QString("TODO");
    m_contextMenuAllowed = true;
    m_pressed = false;
    m_validatingClusters = false;
}

Tool::ToolType VisibilityTool::toolType() const {
    return Tool::Visibility;
}

QCursor VisibilityTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void VisibilityTool::toggled(bool on) {
    Tool::toggled(on);
    m_editor->tabletCanvas()->update();
    setValidingCusters(false);
}

void VisibilityTool::pressed(const EventInfo& info) {
    if (info.mouseButton & Qt::MiddleButton || info.mouseButton & Qt::RightButton) return;

    m_pressed = true;
    m_lasso = QPolygonF();
    m_lasso << info.pos;
}

void VisibilityTool::moved(const EventInfo& info)  {
    if (!m_pressed) return;

    m_lasso << info.pos;
}

void VisibilityTool::released(const EventInfo& info) {
    if (!m_pressed) return;

    m_lasso << info.firstPos;

    if (!m_validatingClusters) {
        Layer *layer = m_editor->layers()->currentLayer();
        m_editor->undoStack()->push(new ComputeVisibilityCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame()));
        setValidingCusters(true);
    }

    m_lasso.clear();
}

void VisibilityTool::wheel(const WheelEventInfo& info) {

}

void VisibilityTool::keyReleased(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape  && m_validatingClusters) {
        m_editor->undoStack()->undo();
        setValidingCusters(false);
    } else if (event->key() == Qt::Key_Return && m_validatingClusters) {
        setValidingCusters(false);
    }
}

void VisibilityTool::drawUI(QPainter &painter, VectorKeyFrame *key) { 
    PickTool::drawUI(painter, key);

    if (m_validatingClusters) {
        m_editor->tabletCanvas()->setFontSize(24.0f * (1.0f / m_editor->view()->scaling()));
        painter.setFont(m_editor->tabletCanvas()->canvasFont());
        painter.drawText(m_editor->view()->mapScreenToCanvas(QPointF(50, 100)), "Confirm? [Enter/ESC]");
    }
}

void VisibilityTool::drawGL(VectorKeyFrame *key, qreal alpha) {

}


void VisibilityTool::frameChanged(int frame) {

}

void VisibilityTool::setValidingCusters(bool b) {
    m_validatingClusters = b;
    m_needEscapeFocus = b;
    m_needReturnFocus = b;
    m_editor->tabletCanvas()->setDisplayVisibility(b);
    m_editor->tabletCanvas()->setDisplayMode(b ? TabletCanvas::DisplayMode::VisibilityThreshold : TabletCanvas::DisplayMode::StrokeColor);
    m_editor->tabletCanvas()->setMaskOcclusionMode(b ? TabletCanvas::MaskGrayOut : TabletCanvas::MaskOcclude);
}
