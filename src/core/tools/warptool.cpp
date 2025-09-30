/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "warptool.h"
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

extern dkSlider k_deformRange;
extern dkInt k_registrationIt;
extern dkInt k_registrationRegularizationIt;
extern dkBool k_keyframesMode;

dkBool k_displayGrids("Warp->Display grids", true);
dkBool k_drawSourceGrid("Warp->Display source grid", false);
dkBool k_drawInterpGrid("Warp->Display interpolated grid", false);
dkBool k_drawTargetGrid("Warp->Display target grid", true);
dkBool k_registerOnMove("Warp->Register while warping", false);
dkBool k_registerOnRelease("Warp->Register on release", false);

static const QStringList CONFIGURATION({"Target", "Source"});
static const QStringList SCALING_MODE({"Uniform", "Vertical", "Horizontal"});
static dkStringList k_deformConfiguration("Warp->Configuration", CONFIGURATION);
static dkStringList k_scalingMode("Warp->Scaling mode", SCALING_MODE);
static dkBool k_regularizeAfterScaling("Warp->Regularize after scaling", false);

WarpTool::WarpTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click: warp the selection | Right-click: warp the selection only in the cursor footprint | Ctrl+Right-click: regularize selection");
    m_pivot = Point::VectorType::Zero();
    m_nudge = QVector2D(0.0f, 0.0f);
    m_contextMenuAllowed = false;
    m_pressed = false;
    connect(&k_displayGrids, SIGNAL(valueChanged(bool)), m_editor->tabletCanvas(), SLOT(updateCurrentFrame(void)));
    connect(&k_drawSourceGrid, SIGNAL(valueChanged(bool)), m_editor->tabletCanvas(), SLOT(updateCurrentFrame(void)));
    connect(&k_drawInterpGrid, SIGNAL(valueChanged(bool)), m_editor->tabletCanvas(), SLOT(updateCurrentFrame(void)));
    connect(&k_drawTargetGrid, SIGNAL(valueChanged(bool)), m_editor->tabletCanvas(), SLOT(updateCurrentFrame(void)));
}

WarpTool::~WarpTool() {

}

Tool::ToolType WarpTool::toolType() const {
    return Tool::Warp;
}

QCursor WarpTool::makeCursor(float scaling) const {
    return QCursor(Qt::BlankCursor);
}

void WarpTool::toggled(bool on) {
    Tool::toggled(on);
    m_editor->tabletCanvas()->setMouseTracking(on);
    m_editor->tabletCanvas()->setTabletTracking(on);
    m_editor->tabletCanvas()->fixedCanvasView()->setAttribute(Qt::WA_TransparentForMouseEvents, on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if (group != nullptr) {
            group->setShowGrid(on);
        }
    }
}

void WarpTool::pressed(const EventInfo& info) {
    m_nudge = QVector2D(0.0f, 0.0f);
    m_pressed = true;

    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }

    Point::VectorType pos(info.pos.x(), info.pos.y());

    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    m_inverseRigidGlobal = info.key->rigidTransform(1.0).inverse();
    m_editor->grid()->selectGridCorner(info.key->selectedGroup(), type, info.key->selectedGroup()->globalRigidTransform(1.0f).inverse() * pos, info.mouseButton & Qt::RightButton);
    info.key->toggleHardConstraint(false);

    m_registerToNextKeyframe = (k_registerOnMove || k_registerOnRelease) && m_editor->registration()->registrationTargetEmpty();
    if (m_registerToNextKeyframe) m_editor->registration()->setRegistrationTarget(info.key->nextKeyframe());
}

void WarpTool::moved(const EventInfo& info) {
    if (!m_pressed) {
        return;
    }
    m_nudge = QVector2D(info.pos - info.lastPos);
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }

    PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
    Point::VectorType pos(info.pos.x(), info.pos.y());

    if (info.mouseButton & Qt::RightButton && info.modifiers & Qt::ControlModifier) {
        Arap::regularizeLattice(*info.key->selectedGroup()->lattice(), type == REF_POS ? DEFORM_POS : REF_POS, type, 10);
        info.key->selectedGroup()->setGridDirty();
    } else {
        m_editor->grid()->moveGridCornerPosition(info.key->selectedGroup(), type, m_inverseRigidGlobal * pos);
        info.key->selectedGroup()->setGridDirty();
        
        if (k_registerOnMove && type == TARGET_POS) {
            m_editor->registration()->registration(info.key->selectedGroup(), TARGET_POS, TARGET_POS, false, std::min(1, k_registrationIt.value()), k_registrationRegularizationIt);
        } else if (type == REF_POS) {
            // move strokes with the grid
            info.key->selectedGroup()->strokes().forEachPoint(info.key, [&info](Point *point, unsigned int sId, unsigned int pId) {
                UVInfo uv = info.key->selectedGroup()->uvs().get(sId, pId);
                point->setPos(info.key->selectedGroup()->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
            });
        }
    }

    info.key->makeInbetweensDirty();
}

void WarpTool::released(const EventInfo& info) {
    m_pressed = false;
    m_nudge = QVector2D(0.0f, 0.0f);

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
        if (k_registerOnRelease && info.mouseButton & Qt::LeftButton) m_editor->registration()->registration(info.key->selectedGroup(), TARGET_POS, TARGET_POS, false);
        info.key->selectedGroup()->syncTargetPosition(info.key->nextKeyframe());
    }

    info.key->selectedGroup()->setGridDirty();
    info.key->toggleHardConstraint(true);
    info.key->resetTrajectories(true);
    info.key->makeInbetweensDirty();

    if (m_registerToNextKeyframe) m_editor->registration()->clearRegistrationTarget();
}

void WarpTool::doublepressed(const EventInfo& info) {
    
}

void WarpTool::wheel(const WheelEventInfo& info) {
    if (info.modifiers & Qt::ShiftModifier) {
        PosTypeIndex type = k_deformConfiguration.index() == 0 ? TARGET_POS : REF_POS;
        int scaleMode = k_scalingMode.index();

        if (!k_keyframesMode) {
            for (Group *group : info.key->selection().selectedPostGroups()) {
                if (info.modifiers & Qt::AltModifier) {
                    std::vector<Corner *> corners;
                    Point::VectorType pos(info.pos.x(), info.pos.y());
                    for (Corner *c : group->lattice()->corners()) {
                        if ((pos - c->coord(TARGET_POS)).norm() < k_deformRange * 0.5) corners.push_back(c);
                    }
                    if (info.delta > 0) m_editor->grid()->scaleGrid(group, 1.1f, type, corners, scaleMode); 
                    else                m_editor->grid()->scaleGrid(group, 0.9f, type, corners, scaleMode);
                    if (k_regularizeAfterScaling) Arap::regularizeLattice(*group->lattice(), REF_POS, type, 200, true);
                } else {
                    if (info.delta > 0) m_editor->grid()->scaleGrid(group, 1.1f, type, scaleMode); 
                    else                m_editor->grid()->scaleGrid(group, 0.9f, type, scaleMode);
                }
                if (type == TARGET_POS) {
                    group->setGridDirty();
                    group->lattice()->setBackwardUVDirty(true);
                } else {
                    group->syncSourcePosition();
                    group->syncSourcePosition(info.key->prevKeyframe());
                    info.key->updateBuffers();
                    group->recomputeBbox();
                }
                group->setGridDirty();
                info.key->resetTrajectories(true);
                info.key->makeInbetweensDirty();
            }
        } else {
            for (VectorKeyFrame *keyframe : info.key->parentLayer()->getSelectedKeyFrames()) {
                for (Group *group : keyframe->postGroups()) {
                    if (info.modifiers & Qt::AltModifier) {
                        std::vector<Corner *> corners;
                        Point::VectorType pos(info.pos.x(), info.pos.y());
                        for (Corner *c : group->lattice()->corners()) {
                            if ((pos - c->coord(TARGET_POS)).norm() < k_deformRange * 0.5) corners.push_back(c);
                        }
                        if (info.delta > 0) {
                            m_editor->grid()->scaleGrid(group, 1.1f, TARGET_POS, corners, scaleMode); 
                            m_editor->grid()->scaleGrid(group, 1.1f, REF_POS, corners, scaleMode); 
                        }
                        else {
                            m_editor->grid()->scaleGrid(group, 0.9f, TARGET_POS, corners, scaleMode);
                            m_editor->grid()->scaleGrid(group, 0.9f, REF_POS, corners, scaleMode);
                        }
                        if (k_regularizeAfterScaling) Arap::regularizeLattice(*group->lattice(), REF_POS, type, 200, true);
                    } else {
                        if (info.delta > 0) {
                            m_editor->grid()->scaleGrid(group, 1.1f, TARGET_POS, scaleMode); 
                            m_editor->grid()->scaleGrid(group, 1.1f, REF_POS, scaleMode); 
                        }
                        else {
                            m_editor->grid()->scaleGrid(group, 0.9f, TARGET_POS, scaleMode);
                            m_editor->grid()->scaleGrid(group, 0.9f, REF_POS, scaleMode);
                        }
                    }
                    group->setGridDirty();
                    group->lattice()->setBackwardUVDirty(true);
                    group->recomputeBbox();
                    keyframe->resetTrajectories(true);
                    keyframe->makeInbetweensDirty();
                }           
            }
        }

    } else {
        if (info.delta > 0) k_deformRange.setValue(k_deformRange + 5);
        else                k_deformRange.setValue(k_deformRange - 5);
    }
    m_editor->tabletCanvas()->updateCursor();
}

void WarpTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    for (Group *group : key->selection().selectedPostGroups()) {
        if (group->lattice() == nullptr) continue;
        qreal alphaLinear = m_editor->currentAlpha();

        if (group->lattice()->isArapPrecomputeDirty()) {
            group->lattice()->precompute();
        }

        if (k_drawInterpGrid) {
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
    // if (key->selectedGroup() != nullptr && key->selectedGroup()->lattice() != nullptr) {
    //     painter.drawText(QPoint(0, 0), QString("%1").arg(key->selectedGroup()->lattice()->scale()));
    // }
}

void WarpTool::drawGL(VectorKeyFrame *key, qreal alpha) {
    for (Group *group : key->selection().selectedPostGroups()) {
        m_editor->tabletCanvas()->drawGrid(group);
    }
    m_editor->tabletCanvas()->drawCircleCursor(m_nudge);
}
