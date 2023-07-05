/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "picktool.h"
#include "group.h"
#include "editor.h"
#include "canvascommands.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "selectionmanager.h"
#include "canvassceneitems.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"

#include <QKeyEvent>

static const QStringList SHAPES({"Pick", "Lasso", "Rectangle"});
static dkStringList k_selectionShape("Select->Selection shape", SHAPES);
static const QStringList MODES({"Start keyframe", "End keyframe"});
static dkStringList k_selectionMode("Select->Mode", MODES);

PickTool::PickTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click to select groups | Shift+Left-click to add groups to the current selection");
    m_drawer = new LassoDrawer(this);
    k_selectionShape.setIndex(1);
    m_pressed = false;
    connect(&k_selectionMode, SIGNAL(indexChanged(int)), this, SLOT(setDrawEndKeyframe(int)));
}

PickTool::~PickTool() {

}

Tool::ToolType PickTool::toolType() const {
    return Tool::Select;
}

QGraphicsItem *PickTool::graphicsItem() {
    return m_drawer;
}

QCursor PickTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void PickTool::toggled(bool on) {
    Tool::toggled(on);
    if (!on) {
        m_editor->tabletCanvas()->setDrawGroupColor(false);
        m_editor->tabletCanvas()->setDrawPreGroupGhosts(false);
    } else {
        m_editor->tabletCanvas()->setDrawPreGroupGhosts(k_selectionMode.index() == 1);
    }
}

void PickTool::pressed(const EventInfo& info) {
    if (!(info.mouseButton & Qt::LeftButton)){
        m_pressed = false;
        return;
    }
    m_pressed = true;
    int layerNumber = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    GroupType type = k_selectionMode.index() == 0 ? POST : PRE;

    switch (k_selectionShape.index()) {
        case 0: // "PICK"
            {
                int selectedGroup = m_editor->selection()->selectGroups(key, info.alpha, type, info.pos, true);
                m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerNumber, currentFrame, selectedGroup, type, info.modifiers & Qt::ShiftModifier));
                emit newSelectedGroup(key->selectedGroup(type));
            }   
            break;

        case 1: // "LASSO"
            m_lasso = QPolygonF();
            m_lasso << info.pos;
            break;

        case 2: // "RECT"
            break;

        default:
            break;
    }
}

void PickTool::moved(const EventInfo& info) {
    if(!m_pressed) return;
    switch (k_selectionShape.index()) {
        case 0: // "PICK"
            break;

        case 1: // "LASSO"
            m_lasso << info.pos;
            break;

        case 2: // "RECT"
            break;

        default:
            break;
    }
}

void PickTool::released(const EventInfo& info) {
    if(!m_pressed) return;
    int layerNumber = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    GroupType type = k_selectionMode.index() == 0 ? POST : PRE;

    switch (k_selectionShape.index()) {
        case 0: // "PICK"
            break;

        case 1: // "LASSO"
            {
                m_lasso << info.firstPos;
                std::vector<int> selectedGroups;
                m_editor->selection()->selectGroups(key, info.alpha, type, m_lasso, false, selectedGroups);
                if (selectedGroups.empty())
                    m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerNumber, currentFrame, Group::ERROR_ID, type, info.modifiers & Qt::ShiftModifier));
                else
                    m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerNumber, currentFrame, selectedGroups, type, info.modifiers & Qt::ShiftModifier));
                emit newSelectedGroup(key->selectedGroup(type));
                m_lasso = QPolygonF();
            }
            break;

        case 2: // "RECT"
            break;

        default:
            break;
    }
}

void PickTool::keyPressed(QKeyEvent *event) {
    if (event->key() == Qt::Key_Alt) {
        m_editor->tabletCanvas()->setDrawGroupColor(true);
    }
} 

void PickTool::keyReleased(QKeyEvent *event) {
    if (event->key() == Qt::Key_Alt) {
        m_editor->tabletCanvas()->setDrawGroupColor(false);
    }
}

void PickTool::draw(QPainter &painter, VectorKeyFrame *key) {
    
}

void PickTool::setDrawEndKeyframe(int index) {
    if (index == 1) m_editor->tabletCanvas()->setDrawPreGroupGhosts(true);
    else m_editor->tabletCanvas()->setDrawPreGroupGhosts(false);
    m_editor->tabletCanvas()->update();
}