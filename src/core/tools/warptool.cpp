/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "warptool.h"
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

extern dkSlider k_deformRange;
extern dkInt k_registrationIt;
extern dkInt k_registrationRegularizationIt;
dkBool k_drawSourceGrid("Warp->Display source grid", false);
dkBool k_drawInterpGrid("Warp->Display interpolated grid", true);
dkBool k_drawTargetGrid("Warp->Display target grid", true);
dkBool k_registerOnMove("Warp->Register while warping", false);
dkBool k_registerOnRelease("Warp->Register on release", false);

static const QStringList CONFIGURATION({"Target", "Source"});
static dkStringList k_deformConfiguration("Warp->Configuration", CONFIGURATION);

WarpTool::WarpTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click: warp the selection | Right-click: warp the selection only in the cursor footprint");
    m_pivot = Point::VectorType::Zero();
    m_contextMenuAllowed = false;
}

WarpTool::~WarpTool() {

}

Tool::ToolType WarpTool::toolType() const {
    return Tool::Warp;
}

QGraphicsItem *WarpTool::graphicsItem() {
    return nullptr;
}

QCursor WarpTool::makeCursor(float scaling) const {
    int size = k_deformRange * scaling * 2.0;
    float size_2 = size / 2.0;
    QPixmap pixmap(size, size);
    if (!pixmap.isNull()) {
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::black);
        painter.drawEllipse(QRectF(0, 0, size, size));
        painter.drawLine(size_2, size_2 - 2, size_2, size_2 + 2);
        painter.drawLine(size_2 - 2, size_2, size_2 + 2, size_2);
    }
    return QCursor(pixmap);
}

void WarpTool::toggled(bool on) {
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
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
        m_editor->tabletCanvas()->updateCurrentFrame();
    }
}

void WarpTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }

    Point::VectorType pos(info.pos.x(), info.pos.y());

    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    m_inverseRigidGlobal = info.key->rigidTransform(1.0).inverse();
    m_editor->grid()->selectGridCorner(info.key->selectedGroup(), type, m_inverseRigidGlobal * pos, info.mouseButton & Qt::RightButton);
    info.key->toggleHardConstraint(false);

    m_registerToNextKeyframe = (k_registerOnMove || k_registerOnRelease) && m_editor->registration()->registrationTargetEmpty();
    if (m_registerToNextKeyframe) m_editor->registration()->setRegistrationTarget(info.key->nextKeyframe());
}

void WarpTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }
    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    Point::VectorType pos(info.pos.x(), info.pos.y());
    m_editor->grid()->moveGridCornerPosition(info.key->selectedGroup(), type, m_inverseRigidGlobal * pos);
    info.key->selectedGroup()->lattice()->setArapDirty();
    
    if (k_registerOnMove && type == TARGET_POS) {
        m_editor->registration()->registration(info.key->selectedGroup(), TARGET_POS, TARGET_POS, false, std::min(1, k_registrationIt.value()), k_registrationRegularizationIt);
    } else if (type == REF_POS) {
        // move strokes with the grid
        info.key->selectedGroup()->strokes().forEachPoint(info.key, [&info](Point *point, unsigned int sId, unsigned int pId) {
            UVInfo uv = info.key->selectedGroup()->uvs().get(sId, pId);
            point->setPos(info.key->selectedGroup()->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
        });
    }

    info.key->makeInbetweensDirty();
}

void WarpTool::released(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }

    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    m_editor->grid()->releaseGridCorner(info.key->selectedGroup());

    if (type == REF_POS) {
        info.key->selectedGroup()->syncSourcePosition();
        info.key->selectedGroup()->syncSourcePosition(info.key->prevKeyframe());
        info.key->updateBuffers();
        info.key->selectedGroup()->recomputeBbox();
        // TODO what should we do with non-breakdown groups? because moving the REF_POS of the lattice might break some matching
    }

    if (type == TARGET_POS) {
        if (k_registerOnRelease) m_editor->registration()->registration(info.key->selectedGroup(), TARGET_POS, TARGET_POS, false);
        info.key->selectedGroup()->syncTargetPosition(info.key->nextKeyframe());
    }

    info.key->selectedGroup()->lattice()->setArapDirty();
    info.key->toggleHardConstraint(true);
    info.key->resetTrajectories();
    info.key->makeInbetweensDirty();

    if (m_registerToNextKeyframe) m_editor->registration()->clearRegistrationTarget();
}

void WarpTool::doublepressed(const EventInfo& info) {
    
}

void WarpTool::wheel(const WheelEventInfo& info) {
    if (info.modifiers & Qt::ShiftModifier && info.key->selectedGroup(POST) != nullptr) {
        PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
        if (info.modifiers & Qt::AltModifier) {
            std::vector<Corner *> corners;
            Point::VectorType pos(info.pos.x(), info.pos.y());
            for (Corner * c : info.key->selectedGroup(POST)->lattice()->corners()) {
                if ((pos - c->coord(TARGET_POS)).norm() < k_deformRange) corners.push_back(c);
            }
            if (info.delta > 0) m_editor->grid()->scaleGrid(info.key->selectedGroup(POST), 1.1f, type, corners); 
            else                m_editor->grid()->scaleGrid(info.key->selectedGroup(POST), 0.9f, type, corners);
        } else {
            if (info.delta > 0) m_editor->grid()->scaleGrid(info.key->selectedGroup(POST), 1.1f, type); 
            else                m_editor->grid()->scaleGrid(info.key->selectedGroup(POST), 0.9f, type);
        }
        if (type == TARGET_POS) {
            info.key->selectedGroup()->lattice()->setArapDirty();
            info.key->selectedGroup()->lattice()->setBackwardUVDirty(true);
        } else {
            info.key->selectedGroup()->syncSourcePosition();
            info.key->selectedGroup()->syncSourcePosition(info.key->prevKeyframe());
            info.key->updateBuffers();
            info.key->selectedGroup()->recomputeBbox();
        }
        info.key->selectedGroup()->lattice()->setArapDirty();
        info.key->resetTrajectories();
        info.key->makeInbetweensDirty();
    } else {
        if (info.delta > 0) k_deformRange.setValue(k_deformRange + 4);
        else                k_deformRange.setValue(k_deformRange - 4);
    }
    m_editor->tabletCanvas()->updateCursor();
}

void WarpTool::draw(QPainter &painter, VectorKeyFrame *key) {
    for (Group *group : key->selection().selectedPostGroups()) {
        float alphaLinear = m_editor->alpha(m_editor->playback()->currentFrame());

        if (group->lattice()->isArapPrecomputeDirty()) {
            group->lattice()->precompute();
        }

        if (k_drawInterpGrid && alphaLinear > 0.0 && alphaLinear < 1.0) {
            float spacing = group->spacingAlpha(alphaLinear);
            int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
            int inbetween = key->parentLayer()->inbetweenPosition(m_editor->playback()->currentFrame());
            m_editor->updateInbetweens(key, inbetween, stride);
            group->drawGrid(painter, inbetween, INTERP_POS);
        }

        if (k_drawSourceGrid) {
            group->drawGrid(painter, 0, REF_POS);
        }

        if (k_drawTargetGrid) {
            int stride = key->parentLayer()->stride(key->parentLayer()->getVectorKeyFramePosition(key));
            m_editor->updateInbetweens(key, stride, stride);
            group->drawGrid(painter, 0, TARGET_POS);
        }
    }
}