/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "rigiddeformtool.h"
#include "layer.h"
#include "editor.h"
#include "playbackmanager.h"
#include "canvasscenemanager.h"
#include "layermanager.h"
#include "gridmanager.h"
#include "registrationmanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"

static const QStringList CONFIGURATION({"Target", "Source"});
static dkStringList k_deformConfiguration("RigidDeform->Configuration", CONFIGURATION);

extern dkBool k_drawSourceGrid;
extern dkBool k_drawInterpGrid;
extern dkBool k_drawTargetGrid;
extern dkBool k_registerOnMove;
extern dkBool k_registerOnRelease;

RigidDeformTool::RigidDeformTool(QObject *parent, Editor *editor) : WarpTool(parent, editor) {
    m_toolTips = QString("Left-click: translate the selection | Ctrl+Left-click: rotate the selection");
}

RigidDeformTool::~RigidDeformTool() {

}

Tool::ToolType RigidDeformTool::toolType() const {
    return Tool::RigidDeform;
}

QGraphicsItem *RigidDeformTool::graphicsItem() {
    return nullptr;
}

QCursor RigidDeformTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void RigidDeformTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        keyframe->selectedGroup()->setShowGrid(on);
        for (Group *group : keyframe->selection().selectedPostGroups()) group->setShowGrid(on);
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
        m_editor->tabletCanvas()->updateCurrentFrame();
    }
}

void RigidDeformTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) return;

    m_centerOfMass = Point::VectorType::Zero();

    // compute the center of mass of the selected groups
    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    for (Group *group : info.key->selection().selectedPostGroups()) {
        m_centerOfMass += group->lattice()->centerOfGravity(type);
    }
    m_centerOfMass /= info.key->selection().selectedPostGroups().size();

    info.key->toggleHardConstraint(false);
}

void RigidDeformTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) return;
    QVector2D delta(info.pos - info.lastPos);
    if (delta.length() < 1e-5) return;

    // rotation TODO: eventually replace/complement with 4 "rotate" arrows
    if (info.modifiers & Qt::ControlModifier) {
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
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) return;

    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;

    if (type == TARGET_POS) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            m_editor->grid()->releaseGridCorner(group);
            if (k_registerOnRelease) {
                m_editor->registration()->registration(group, TARGET_POS, TARGET_POS, false);
            }
            group->lattice()->setArapDirty();
            group->syncTargetPosition(info.key->nextKeyframe());
        }
    } else {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            group->syncSourcePosition();
            group->syncSourcePosition(info.key->prevKeyframe());
            group->recomputeBbox();
            group->lattice()->setArapDirty();
            info.key->updateBuffers();
            // TODO what should we do with non-breakdown groups? because moving the REF_POS of the lattice might break some matching
        }
    }
    info.key->toggleHardConstraint(true);
    info.key->resetTrajectories();
    info.key->makeInbetweensDirty();
    m_editor->scene()->selectedGroupChanged(info.key->selection().selectedPostGroups());
}

void RigidDeformTool::doublepressed(const EventInfo& info) {

}

void RigidDeformTool::draw(QPainter &painter, VectorKeyFrame *key) {
    if (key->selectedGroup() != nullptr) {
        float alphaLinear = m_editor->alpha(m_editor->playback()->currentFrame());

        for (Group *group : key->selection().selectedPostGroups()) {
            if (group->lattice()->isArapPrecomputeDirty()) {
                group->lattice()->precompute();
            }

            if (k_deformConfiguration.index() == 0 && k_drawTargetGrid) {
                float spacing = group->spacingAlpha(1.0);
                int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
                if (group->lattice()->currentPrecomputedTime() != spacing || group->lattice()->isArapInterpDirty()) 
                    group->lattice()->interpolateARAP(1.0, 1.0, key->rigidTransform(1.0));
                m_editor->updateInbetweens(key, stride, stride);
                group->drawGrid(painter, 0, TARGET_POS);
            } else if (k_deformConfiguration.index() == 1 && k_drawSourceGrid) {
                group->drawGrid(painter, 0, REF_POS);
            }
        }
    }
}

void RigidDeformTool::deformSelection(const Point::Affine &transform, const EventInfo &info) {
    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    for (Group *group : info.key->selection().selectedPostGroups()) {
        // move lattice REF_POS
        group->lattice()->applyTransform(transform, type, type);
        group->lattice()->setArapDirty();

        // move strokes with the lattice
        if (type == REF_POS) {
            group->strokes().forEachPoint(info.key, [&group](Point *point, unsigned int sId, unsigned int pId) {
                UVInfo uv = group->uvs().get(sId, pId);
                point->setPos(group->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
            });
        }
    }

    info.key->makeInbetweensDirty();
}