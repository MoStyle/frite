/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "rigiddeformtool.h"
#include "layer.h"
#include "editor.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "gridmanager.h"
#include "registrationmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "qteigen.h"

static const QStringList CONFIGURATION({"Target", "Source"});
static dkStringList k_deformConfiguration("RigidDeform->Configuration", CONFIGURATION);
dkBool k_keyframesMode("RigidDeform->Keyframes mode", false);

extern dkSlider k_deformRange;
extern dkBool k_drawSourceGrid;
extern dkBool k_drawInterpGrid;
extern dkBool k_drawTargetGrid;
extern dkBool k_registerOnMove;
extern dkBool k_registerOnRelease;

RigidDeformTool::RigidDeformTool(QObject *parent, Editor *editor) : WarpTool(parent, editor) {
    m_toolTips = QString("Left-click: translate the selection | Ctrl+Left-click: rotate the selection | Right-click: translate/rotate only in the cursor footprint");
}

RigidDeformTool::~RigidDeformTool() {

}

Tool::ToolType RigidDeformTool::toolType() const {
    return Tool::RigidDeform;
}

QCursor RigidDeformTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void RigidDeformTool::toggled(bool on) {
    Tool::toggled(on);
    m_editor->tabletCanvas()->setMouseTracking(on);
    m_editor->tabletCanvas()->setTabletTracking(on);
    m_editor->tabletCanvas()->fixedCanvasView()->setAttribute(Qt::WA_TransparentForMouseEvents, on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        keyframe->selectedGroup()->setShowGrid(on);
        for (Group *group : keyframe->selection().selectedPostGroups()) group->setShowGrid(on);
        m_editor->tabletCanvas()->updateCurrentFrame();
    }
    if (on) {
        for (Group *group : keyframe->selection().selectedPostGroups()) {
            for (Corner *corner : group->lattice()->corners()) {
                corner->setDeformable(true);
            }
        }
    }
}

void RigidDeformTool::pressed(const EventInfo& info) {
    m_nudge = QVector2D(0.0f, 0.0f);
    m_pressed = true;

    if (!k_keyframesMode && (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)) return;

    m_centerOfMass = Point::VectorType::Zero();
    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    Point::VectorType pos = QE_POINT(info.pos);

    if (info.mouseButton & Qt::LeftButton) {
        // compute the center of mass of the selected groups
        for (Group *group : info.key->selection().selectedPostGroups()) {
            m_centerOfMass += group->lattice()->centerOfGravity(type);
        }
        m_centerOfMass /= info.key->selection().selectedPostGroups().size();
    } else if (info.mouseButton & Qt::RightButton && !k_keyframesMode) {
        // select vertices inside the cursor footprint and compute the center of mass of the selected vertices
        int selectedVertices = 0;
        for (Group *group : info.key->selection().selectedPostGroups()) {
            for (Corner *corner : group->lattice()->corners()) {
                if ((corner->coord(type) - pos).norm() < k_deformRange * 0.5) {
                    corner->setDeformable(true);
                    m_centerOfMass += corner->coord(type);
                    selectedVertices++;
                } else {
                    corner->setDeformable(false);
                }      
            }
        }
        m_centerOfMass /= selectedVertices;
    }

    info.key->toggleHardConstraint(false);
}

void RigidDeformTool::moved(const EventInfo& info) {
    if (!m_pressed) {
        return;
    }
    m_nudge = QVector2D(info.pos - info.lastPos);

    if (!k_keyframesMode && (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)) return;
    QVector2D delta(info.pos - info.lastPos);
    if (delta.length() < 1e-5) return;

    // rotation TODO: eventually replace/complement with 4 "rotate" arrows
    if (info.modifiers & Qt::ControlModifier && !k_keyframesMode) {
        // positions relative to m_centerOfMass 
        Point::VectorType oPos(Point::VectorType(info.pos.x(), info.pos.y()));
        Point::VectorType oLastPos(Point::VectorType(info.lastPos.x(), info.lastPos.y()));
        oPos -= m_centerOfMass; oLastPos -= m_centerOfMass;
        oPos.normalize(); oLastPos.normalize();
        double cosA = oPos.dot(oLastPos);
        double angle = acos(cosA);
        cosA = Point::VectorType(-oLastPos.y(), oLastPos.x()).dot(oPos);
        if (cosA < 0) angle = -angle; 
        Point::Affine transform = Point::Affine(Point::Translation(m_centerOfMass) * Point::Rotation(angle) * Point::Translation(-m_centerOfMass));
        deformSelection(transform, info);
        return;
    } 

    // translation
    Point::Affine transform(Point::Translation(delta.x(), delta.y()));
    deformSelection(transform, info);
}

void RigidDeformTool::released(const EventInfo& info) {
    m_pressed = false;
    m_nudge = QVector2D(0.0f, 0.0f);

    if (!k_keyframesMode && (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)) return;

    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;

    if (info.mouseButton & Qt::RightButton && !k_keyframesMode) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            for (Corner *corner : group->lattice()->corners()) {
                corner->setDeformable(true);
            }
        }
    }

    if (type == TARGET_POS && !k_keyframesMode) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            m_editor->grid()->releaseGridCorner(group);
            if (k_registerOnRelease) {
                m_editor->registration()->registration(group, TARGET_POS, TARGET_POS, false);
            }
            group->setGridDirty();
            group->syncTargetPosition(info.key->nextKeyframe());
        }
    } else if (!k_keyframesMode) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            group->syncSourcePosition();
            group->syncSourcePosition(info.key->prevKeyframe());
            group->recomputeBbox();
            group->setGridDirty();
            info.key->updateBuffers();
            // TODO what should we do with non-breakdown groups? because moving the REF_POS of the lattice might break some matching
        }
    }

    info.key->toggleHardConstraint(true);
    info.key->resetTrajectories(true);
    info.key->makeInbetweensDirty();
}

void RigidDeformTool::doublepressed(const EventInfo& info) {

}

void RigidDeformTool::deformSelection(const Point::Affine &transform, const EventInfo &info) {
    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;

    if (!k_keyframesMode) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            // move lattice REF_POS
            group->lattice()->applyTransform(transform, type, type);
            group->setGridDirty();

            // move strokes with the lattice
            if (type == REF_POS) {
                group->strokes().forEachPoint(info.key, [&group](Point *point, unsigned int sId, unsigned int pId) {
                    UVInfo uv = group->uvs().get(sId, pId);
                    point->setPos(group->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
                });
            }
        }
        info.key->makeInbetweensDirty();
    } else {
        qDebug() << "KF Mode";
        qDebug() << "KF selected: " << info.key->parentLayer()->getSelectedKeyFrames().size();
        for (VectorKeyFrame *keyframe : info.key->parentLayer()->getSelectedKeyFrames()) {
            qDebug() << "KF " << keyframe->keyframeNumber();
            for (Group *group : keyframe->postGroups()) {
                qDebug() << "group: " << group->id();
                if (group->lattice() == nullptr) continue;
                for (Corner *corner : group->lattice()->corners()) {
                    corner->setDeformable(true);
                }
                group->lattice()->applyTransform(transform, REF_POS, REF_POS);
                std::cout << "centroid bef: " << group->lattice()->centerOfGravity(TARGET_POS).transpose() << std::endl; 
                group->lattice()->applyTransform(transform, TARGET_POS, TARGET_POS);
                std::cout << "centroid aft: " << group->lattice()->centerOfGravity(TARGET_POS).transpose() << std::endl;
                group->setGridDirty();

                // move strokes with the lattice
                group->strokes().forEachPoint(keyframe, [&group](Point *point, unsigned int sId, unsigned int pId) {
                    UVInfo uv = group->uvs().get(sId, pId);
                    point->setPos(group->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
                });

                group->setGridDirty();
                group->recomputeBbox();
            }
            keyframe->makeInbetweensDirty();            
        }
    }
}