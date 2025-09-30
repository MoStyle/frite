/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "directmatchingtool.h"

#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "gridmanager.h"
#include "registrationmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "viewmanager.h"
#include "arap.h"
#include "qteigen.h"
#include "dialsandknobs.h"

static dkBool k_registerAfterPin("DirectMatching->Register after pinning", false);

DirectMatchingTool::DirectMatchingTool(QObject *parent, Editor *editor) : Tool(parent, editor), m_addPinCommand(false), m_addCorrespondencePinCommand(false) {
    m_toolTips = QString("Left-click and drag to establish a correspondence point");
}

DirectMatchingTool::~DirectMatchingTool() {

}

Tool::ToolType DirectMatchingTool::toolType() const {
    return Tool::DirectMatching;
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
        m_editor->tabletCanvas()->updateCurrentFrame();
    }

    // unpin all quads when leaving the tool
    if (!on) {
        // for (Group *group : keyframe->postGroups()) {
        //     if (group->lattice() == nullptr) continue;
        //     for (QuadPtr q : group->lattice()->quads()) {
        //         q->unpin();
        //     }
        // }
    }
    // m_pinUVs.clear();
}

void DirectMatchingTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr || !(info.mouseButton & Qt::LeftButton)) {
        return;
    }

    Point::VectorType pos(info.pos.x(), info.pos.y());
    m_addPinCommand = false;
    m_addCorrespondencePinCommand = false;
    m_movingExistingPin = false;
    m_existingPinMoved.clear();

    for (Group *group : info.key->selection().selectedPostGroups()) {
        // Checking if we're clicking on an already existing pin
        if (group->lattice() != nullptr) {
            for (QuadPtr q : group->lattice()->quads()) {
                if ((pos - q->pinPos()).norm() < 3.0) {
                    m_movingExistingPin = true;
                    m_existingPinMoved[group->id()] = UVInfo{q->key(), q->pinPos()};
                }
            }
        }


        // Adding new pin
        // Clicking on the REF_POS configuration of the grid add a correspondence pin via click-and-drag
        if (!m_movingExistingPin) {
            int quadKey;
            m_pinUVs[group->id()].uv = group->lattice()->getUV(pos, REF_POS, quadKey);
            if (quadKey != INT_MAX) {
                m_pinUVs[group->id()].quadKey = quadKey;
                m_addCorrespondencePinCommand = true;
                m_addPinCommand = false;
                if (info.modifiers & Qt::ShiftModifier) m_addPinCommand = true;
                // continue;
            }
        }

        m_firstPos = info.firstPos;
        m_curPos = m_firstPos;
        
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

    if (!m_movingExistingPin) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            // Pin correspondence (adding the pin)
            if (m_addCorrespondencePinCommand && m_pinUVs.contains(group->id())) {
                const UVInfo &uv = m_pinUVs[group->id()];
                group->lattice()->quad(uv.quadKey)->pin(uv.uv, QE_POINT(info.pos));

                m_editor->registration()->applyOptimalRigidTransformBasedOnPinnedQuads(group);
                group->lattice()->displacePinsQuads(TARGET_POS);
                if (m_editor->registration()->registrationTargetEmpty()) int i = Arap::regularizeLattice(*group->lattice(), REF_POS, TARGET_POS, 500, true, true, false);
                group->lattice()->displacePinsQuads(TARGET_POS);
                // else if (k_registerAfterPin) m_editor->registration()->registration(group, TARGET_POS, REF_POS, false);
                if (m_addPinCommand && k_registerAfterPin) {
                    m_editor->registration()->setRegistrationTarget(info.key->nextKeyframe());
                    m_editor->registration()->registration(group, TARGET_POS, REF_POS, false); 
                    m_editor->registration()->clearRegistrationTarget();
                }
                group->setGridDirty();
                info.key->resetTrajectories(true);
                info.key->makeInbetweensDirty();
            }
        }
        m_addCorrespondencePinCommand = false;
    } else {
        QPointF deltaMouse =  m_curPos - m_firstPos; 
        for (Group *group : info.key->selection().selectedPostGroups()) {
            // Pin correspondence (moving pins position)
            if (m_existingPinMoved.contains(group->id())) {
                int key = m_existingPinMoved[group->id()].quadKey;
                Point::VectorType curPos = group->lattice()->quad(key)->pinPos();
                Point::VectorType newPos = m_existingPinMoved[group->id()].uv;
                // group->lattice()->quad(uv.quadKey)->pin(uv.uv, QE_POINT(info.pos));
                qDebug() << deltaMouse;
                group->lattice()->quad(key)->setPinPosition(curPos + QE_POINT(deltaMouse));

                m_editor->registration()->applyOptimalRigidTransformBasedOnPinnedQuads(group);
                group->lattice()->displacePinsQuads(TARGET_POS);
                if (m_editor->registration()->registrationTargetEmpty()) int i = Arap::regularizeLattice(*group->lattice(), REF_POS, TARGET_POS, 500, true, true, false);
                group->lattice()->displacePinsQuads(TARGET_POS);
                // else if (k_registerAfterPin) m_editor->registration()->registration(group, TARGET_POS, REF_POS, false);
                if (k_registerAfterPin) {
                    m_editor->registration()->setRegistrationTarget(info.key->nextKeyframe());
                    m_editor->registration()->registration(group, TARGET_POS, REF_POS, false); 
                    m_editor->registration()->clearRegistrationTarget();
                }
                group->setGridDirty();
                info.key->resetTrajectories(true);
                info.key->makeInbetweensDirty();
            }
        }
        m_movingExistingPin = false;
        m_existingPinMoved.clear();
    }

}

void DirectMatchingTool::doublepressed(const EventInfo& info) {

}

void DirectMatchingTool::wheel(const WheelEventInfo& info) {

}

void DirectMatchingTool::keyReleased(QKeyEvent *event) {
    Tool::keyReleased(event);
    if (event->key() == Qt::Key_R) {
        Layer *layer = m_editor->layers()->currentLayer();
        int currentFrame = m_editor->playback()->currentFrame();
        VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
        for (Group *group : keyframe->postGroups()) {
            if (group->lattice() == nullptr) continue;
            for (QuadPtr q : group->lattice()->quads()) {
                q->unpin();
            }
        }
    }
}


void DirectMatchingTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    for (Group *group : key->selection().selectedPostGroups()) {
        qreal alphaLinear = m_editor->currentAlpha();

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