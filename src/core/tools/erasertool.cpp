/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "erasertool.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "colormanager.h"
#include "dialsandknobs.h"
#include "canvascommands.h"

static dkFloat k_eraserSize("Eraser->Eraser size", 8, 1, 2000, 1);
static dkBool k_eraseFromSelection("Eraser->Erase only from selected groups", false);

EraserTool::EraserTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click to erase strokes");
}

Tool::ToolType EraserTool::toolType() const {
    return Tool::Eraser;
}

QGraphicsItem *EraserTool::graphicsItem() {
    return nullptr;
}

QCursor EraserTool::makeCursor(float scaling) const {
    int size = k_eraserSize * scaling;
    size /= 1.5f;
    if (size < 1) size = 1;
    QPixmap pixmap(size, size);
    if (!pixmap.isNull()) {
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::black);
        painter.drawEllipse(QRectF(0, 0, size, size));
    }
    return QCursor(pixmap);
}

void EraserTool::pressed(const EventInfo& info) {
    Layer *layer = m_editor->layers()->currentLayer();
    m_prevFrame = layer->getLastKeyFramePosition(m_editor->playback()->currentFrame());
    m_keyframe = layer->getVectorKeyFrameAtFrame(m_prevFrame);
    erase(info);
}

void EraserTool::moved(const EventInfo& info)  {
    erase(info);
}

void EraserTool::released(const EventInfo& info) {
    
}

void EraserTool::erase(const EventInfo& info) {
    int layerIdx = m_editor->layers()->currentLayerIndex();
    float sizeSq = k_eraserSize * k_eraserSize;
    Point::VectorType pos(info.pos.x(), info.pos.y());
    std::vector<int> strokes;

    // find all strokes intersecting the brush footprint
    for (const StrokePtr &stroke : m_keyframe->strokes()) {
        for (Point *p : stroke->points()) {
            if ((pos - p->pos()).squaredNorm() < sizeSq) {
                strokes.push_back(stroke->id());
                break;
            }
        }
    }

    // erase them completely
    if (strokes.size() > 0) {
        m_editor->undoStack()->beginMacro("Erase stroke");
        bool eraseStroke;
        for (int stroke : strokes) {
            eraseStroke = true;
            // do not erase strokes if they are not from a selected group
            if (k_eraseFromSelection) {
                for (Group *group : info.key->selection().selectedPostGroups()) if (!group->strokes().contains(stroke)) eraseStroke = false;
                for (Group *group : info.key->selection().selectedPreGroups()) if (!group->strokes().contains(stroke)) eraseStroke = false;
            }
            if (eraseStroke) m_editor->undoStack()->push(new EraseCommand(m_editor, layerIdx, m_prevFrame, stroke));
        }
        // TODO: in the same macro, we could erase empty groups
        m_editor->undoStack()->endMacro();
    }
}
