/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "fillgridtool.h"

#include "vectorkeyframe.h"
#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "canvasscenemanager.h"
#include "tabletcanvas.h"
#include "qteigen.h"

FillGridTool::FillGridTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click: add a quad to the grid | Ctrl+Left-click: remove a quad");
}

FillGridTool::~FillGridTool() {

}

Tool::ToolType FillGridTool::toolType() const {
    return Tool::FillGrid;
}

QGraphicsItem *FillGridTool::graphicsItem() {
    return nullptr;
}

QCursor FillGridTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void FillGridTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        keyframe->selectedGroup()->setShowGrid(on);
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
        m_editor->tabletCanvas()->updateCurrentFrame();
    }    
}

void FillGridTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || !(info.mouseButton & Qt::LeftButton)) return;
    if (info.modifiers & Qt::ControlModifier) {
        removeQuad(info.key, info.key->selectedGroup(), QE_POINT(info.pos));
    } else {
        addQuad(info.key, info.key->selectedGroup(), QE_POINT(info.pos));
    }
}

void FillGridTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || !(info.mouseButton & Qt::LeftButton)) return;
    if (info.modifiers & Qt::ControlModifier) {
        removeQuad(info.key, info.key->selectedGroup(), QE_POINT(info.pos));
    } else {
        addQuad(info.key, info.key->selectedGroup(), QE_POINT(info.pos));
    }
}

void FillGridTool::released(const EventInfo& info) {
    
}

void FillGridTool::doublepressed(const EventInfo& info) {
    
}

void FillGridTool::wheel(const WheelEventInfo& info) {
    
}

void FillGridTool::draw(QPainter &painter, VectorKeyFrame *key) {
    if (key->selectedGroup(POST) != nullptr) {
        key->selectedGroup(POST)->lattice()->drawLattice(painter, 0.0, Qt::red, REF_POS);
    }
}

bool FillGridTool::addQuad(VectorKeyFrame *keyframe, Group *group, const Point::VectorType &pos) {
    // Check if we're adding a quad adjacent to the grid
    Lattice *grid = group->lattice();
    int key = group->lattice()->posToKey(pos);
    if (!grid->contains(key - 1) && !grid->contains(key + 1) && !grid->contains(key - grid->nbCols()) && !grid->contains(key + grid->nbCols())) return false;
    bool newQuad = false;
    QuadPtr quad = group->lattice()->addQuad(pos, newQuad);
    if (newQuad) {
        group->lattice()->isConnected(); // TODO: can be optimized since we added a single quad
        group->lattice()->setArapDirty();
        keyframe->makeInbetweensDirty();
    }
    // TODO deform new quads?
    return newQuad;
}

bool FillGridTool::removeQuad(VectorKeyFrame *keyframe, Group *group, const Point::VectorType &pos) {
    QuadPtr q;
    int k;
    if (group->lattice()->contains(pos, REF_POS, q, k)){
        if (q->elements().empty()) {
            group->lattice()->deleteQuad(k);
            group->lattice()->isConnected();
        }
        return true;
    }
    return false;
}

void FillGridTool::contextMenu(QMenu &contextMenu) {
    contextMenu.addSection("Edit grid");
    contextMenu.addAction(tr("Expand grid"), m_editor, &Editor::expandGrid);
}
