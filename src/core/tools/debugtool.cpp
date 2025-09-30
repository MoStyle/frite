/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "debugtool.h"
#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "gridmanager.h"
#include "visibilitymanager.h"
#include "tabletcanvas.h"

#include "registrationmanager.h"
#include "viewmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "viewmanager.h"
#include "arap.h"
#include "qteigen.h"
#include "utils/geom.h"

DebugTool::DebugTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("");
}

DebugTool::~DebugTool() {

}

Tool::ToolType DebugTool::toolType() const {
    return Tool::Debug;
}

QCursor DebugTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void DebugTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if (group != nullptr) {
            group->setShowGrid(on);
        }
    }
    if (!keyframe->selection().selectionEmpty()) {
        m_editor->tabletCanvas()->updateCurrentFrame();
    }

    m_editor->tabletCanvas()->setDisplayMode(on ? TabletCanvas::DisplayMode::PointColor : TabletCanvas::DisplayMode::StrokeColor);
}

void DebugTool::pressed(const EventInfo& info) {
    qDebug() << info.pos.x() << ", " << info.pos.y();

    if (!(info.modifiers & Qt::ControlModifier)) {
        m_editor->visibility()->initAppearance(info.key, info.key->nextKeyframe());
        m_editor->visibility()->computePointsFirstPassAppearance(info.key, info.key->nextKeyframe());

    } else {
        std::vector<Point::VectorType> sourcesAppearance;
        std::vector<int> sourcesGroupsId;
        m_editor->visibility()->findSourcesAppearance(info.key->nextKeyframe(), sourcesAppearance);
        m_editor->visibility()->addGroupsOrBake(info.key, info.key->nextKeyframe(), sourcesAppearance, sourcesGroupsId);
        // m_editor->visibility()->assignVisibilityThresholdAppearance(info.key, sourcesAppearance, sourcesGroupsId);
    }

    info.key->makeInbetweensDirty();

    return;

    Point::VectorType pos(info.pos.x(), info.pos.y());
    Lattice *grid = info.key->selectedGroup()->lattice();
    QuadPtr quad; int quadKey;
    int x, y;
    int k;

    if (grid->contains(pos, REF_POS, quad, quadKey)) {
        // qDebug() << "quad key: " << quadKey;
        info.key->selectedGroup()->setSticker(!info.key->selectedGroup()->isSticker());
        qDebug() << "isSticker: "<< info.key->selectedGroup()->isSticker();
    }

    m_p = grid->projectOnEdge(pos, quadKey);
    quad = grid->quad(quadKey);
    quad->computeCentroid(REF_POS);
    m_p += (quad->centroid(REF_POS) - m_p).normalized() * 0.1;

    // qDebug() << "proj in grid? " << grid->contains(m_p, REF_POS, quad, quadKey);

    // k = grid->posToKey(pos);
    // qDebug() << "x: " << x << ", y: " << y;
    // qDebug() << "k: " << k;

    // if (grid->contains(pos, REF_POS, quad, quadKey)) {
    //     Corner *tlCorner = quad->corners[TOP_LEFT];
    //     std::array<int, 4> adjQuads = {-1, -1, -1, -1};
    //     grid->adjacentQuads(tlCorner, adjQuads);
    //     for (int i = 0; i < NUM_CORNERS; ++i) {
    //         qDebug() << adjQuads[i];
    //     }

    //     // int x, y;
    //     // grid->keyToCoord(quadKey, x, y);
    //     // qDebug() << "QK: " << quadKey;
    //     // qDebug() << "LEFT QK: " << grid->quads().value(grid->coordToKey(x-1, y))->key();
    //     // qDebug() << "TOP LEFT QK: " << grid->quads().value(grid->coordToKey(x-1, y-1))->key();
    //     // qDebug() << "TOP QK: " << grid->quads().value(grid->coordToKey(x, y-1))->key();
    //     // qDebug() << "-----------";
    // }
}

void DebugTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }

    Point::VectorType pos(info.pos.x(), info.pos.y());
    Lattice *grid = info.key->selectedGroup()->lattice();
    QuadPtr quad; int quadKey;
    int x, y;
    int k;

    m_p = grid->projectOnEdge(pos, quadKey);
    quad = grid->quad(quadKey);
    quad->computeCentroid(REF_POS);
    m_p += (quad->centroid(REF_POS) - m_p).normalized() * 0.1;

    // m_p = Point::VectorType(9999, 9999);
    // Point::VectorType proj;
    // for (QuadPtr quad : grid->quads()) {
    //     for (int i = 0; i < 4; ++i) {
    //         Point::VectorType A = quad->corners[i]->coord(REF_POS);
    //         Point::VectorType B = quad->corners[(i + 1) % 4]->coord(REF_POS);
    //         proj = Geom::projectPointToSegment(A, B, pos);
    //         if ((pos - proj).squaredNorm() < (pos - m_p).squaredNorm()) {
    //             m_p = proj;
    //         }
    //     }
    // }
    // qDebug() << "proj in grid? " << grid->contains(m_p, REF_POS, quad, quadKey);
}

void DebugTool::released(const EventInfo& info) {
    // if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
    //     return;
    // }

}

void DebugTool::doublepressed(const EventInfo& info) {
    
}

void DebugTool::wheel(const WheelEventInfo& info) {

}

void DebugTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    for (Group *group : key->selection().selectedPostGroups()) {
        qreal alphaLinear = m_editor->currentAlpha();

        if (group->lattice()->isArapPrecomputeDirty()) {
            group->lattice()->precompute();
        }

        // if (k_drawInterpGrid && alphaLinear > 0.0 && alphaLinear < 1.0) {
        //     float spacing = group->spacingAlpha(alphaLinear);
        //     int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
        //     int inbetween = key->parentLayer()->inbetweenPosition(m_editor->playback()->currentFrame());
        //     m_editor->updateInbetweens(key, inbetween, stride);
        //     group->drawGrid(painter, inbetween, INTERP_POS);
        // }

        // if (k_drawSourceGrid) {
            group->drawGrid(painter, 0, REF_POS);
        // }

        QPen pen(QBrush(Qt::red), 1.0);
        painter.setPen(pen);
        painter.drawEllipse(QPointF(m_p.x(), m_p.y()), 4, 4);

        // if (k_drawTargetGrid) {
        //     int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
        //     m_editor->updateInbetweens(key, stride, stride);
        //     group->drawGrid(painter, 0, TARGET_POS);
        // }
    }
}