/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "directmatchingtool.h"

#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "gridmanager.h"
#include "canvasscenemanager.h"
#include "registrationmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "viewmanager.h"
#include "arap.h"
#include "qteigen.h"

DirectMatchingTool::DirectMatchingTool(QObject *parent, Editor *editor) : Tool(parent, editor), m_addPinCommand(false), m_addCorrespondencePinCommand(false) {
    m_toolTips = QString("Left-click and drag to establish a correspondence point");
}

DirectMatchingTool::~DirectMatchingTool() {

}

Tool::ToolType DirectMatchingTool::toolType() const {
    return Tool::DirectMatching;
}

QGraphicsItem *DirectMatchingTool::graphicsItem() {
    return nullptr;
}

QCursor DirectMatchingTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void DirectMatchingTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        keyframe->selectedGroup()->setShowGrid(on);
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
        m_editor->tabletCanvas()->updateCurrentFrame();
    }

    // unpin all quads when leaving the tool
    if (!on) {
        for (Group *group : keyframe->postGroups()) {
            if (group->lattice() == nullptr) continue;
            for (QuadPtr q : group->lattice()->hash()) {
                q->unpin();
            }
        }
    }
    m_pinUVs.clear();
}

void DirectMatchingTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr || !(info.mouseButton & Qt::LeftButton)) {
        return;
    }

    Point::VectorType pos(info.pos.x(), info.pos.y());
    m_addPinCommand = false;
    m_addCorrespondencePinCommand = false;

    for (Group *group : info.key->selection().selectedPostGroups()) {
        // Adding pin
        // Clicking on the REF_POS configuration of the grid add a correspondence pin via click-and-drag
        int quadKey;
        m_pinUVs[group->id()].uv = group->lattice()->getUV(pos, REF_POS, quadKey);
        if (quadKey != INT_MAX) {
            m_pinUVs[group->id()].quadKey = quadKey;
            m_addCorrespondencePinCommand = true;
            m_addPinCommand = false;
            if (info.modifiers & Qt::ShiftModifier) m_addPinCommand = true;
            m_firstPos = info.firstPos;
            m_curPos = m_firstPos;
            // continue;
        }
        
        // Clicking on the TARGET_POS configuration of the grid add a pin at that location
        // if (!m_addCorrespondencePinCommand) {
        //     m_pinUV = group->lattice()->getUV(pos, TARGET_POS, quadKey);
        //     if (quadKey != INT_MAX) {
        //         QuadPtr quad = group->lattice()->quad(quadKey);
        //         if (!quad->isPinned())  quad->pin(m_pinUV);
        //         else                    quad->unpin();
        //     }
        //     m_addPinCommand = true;
        //     m_addCorrespondencePinCommand = false;
        // }
    }
}

void DirectMatchingTool::moved(const EventInfo& info) {
    m_curPos = info.pos;
}

void DirectMatchingTool::released(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr || !(info.mouseButton & Qt::LeftButton)) {
        return;
    }

    m_curPos = info.pos;
    for (Group *group : info.key->selection().selectedPostGroups()) {
        // Pin correspondence (adding the pin)
        if (m_addCorrespondencePinCommand && m_pinUVs.contains(group->id())) {
            const UVInfo &uv = m_pinUVs[group->id()];
            group->lattice()->quad(uv.quadKey)->pin(uv.uv, QE_POINT(info.pos));

            std::vector<Point::VectorType> disp = group->lattice()->pinsDisplacementVectors();
            if (disp.size() == 1) {
                group->lattice()->applyTransform(Point::Affine(Point::Translation(disp[0])), REF_POS, TARGET_POS);
            }

            if (m_editor->registration()->registrationTargetEmpty()) int i = Arap::regularizeLattice(*group->lattice(), REF_POS, TARGET_POS, 1000, true, true);
            else m_editor->registration()->registration(group, TARGET_POS, REF_POS, false);
            if (m_addPinCommand) {
                m_editor->registration()->setRegistrationTarget(info.key->nextKeyframe());
                m_editor->registration()->registration(group, TARGET_POS, REF_POS, false); 
                m_editor->registration()->clearRegistrationTarget();
            }
            group->lattice()->setArapDirty();
            info.key->resetTrajectories();
            info.key->makeInbetweensDirty();
            m_addCorrespondencePinCommand = false;
        }
    }

}

void DirectMatchingTool::doublepressed(const EventInfo& info) {

}

void DirectMatchingTool::wheel(const WheelEventInfo& info) {

}

void DirectMatchingTool::draw(QPainter &painter, VectorKeyFrame *key) {
    for (Group *group : key->selection().selectedPostGroups()) {
        float alphaLinear = m_editor->alpha(m_editor->playback()->currentFrame());

        if (group->lattice()->isArapPrecomputeDirty()) {
            group->lattice()->precompute();
        }

        group->drawGrid(painter, 0, REF_POS);

        // Draw grid target configuration
        // float spacing = group->spacingAlpha(1.0);
        // int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
        // group->drawGrid(painter, 0, TARGET_POS);

        group->lattice()->drawPins(painter);

        // Draw pin correspondence line
        if (m_addCorrespondencePinCommand) {
            static QPen pen(QBrush(Qt::NoBrush), 1.0, Qt::DashLine);
            pen.setColor(Qt::darkRed);
            painter.setPen(pen);
            painter.drawLine(m_firstPos, m_curPos);
        }
    }
}