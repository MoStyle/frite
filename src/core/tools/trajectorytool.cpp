/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "trajectorytool.h"
#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "gridmanager.h"
#include "canvasscenemanager.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "selectionmanager.h"
#include "canvascommands.h"
#include "viewmanager.h"
#include "cubic.h"
#include "qteigen.h"
#include "utils/geom.h"

static dkBool k_showOriginalTraj("Options->Trajectory->Show original piecewise trajectory", false);
dkBool k_drawChain("Options->Trajectory->Draw full chain", true);

TrajectoryTool::TrajectoryTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click: visualize trajectory | Ctrl+Left-click: add/remove trajectory constraint");
}

TrajectoryTool::~TrajectoryTool() {

}

Tool::ToolType TrajectoryTool::toolType() const {
    return Tool::Traj;
}

QGraphicsItem *TrajectoryTool::graphicsItem() {
    return nullptr;
}

QCursor TrajectoryTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void TrajectoryTool::toggled(bool on) {
    Tool::toggled(on);
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    if (keyframe->selectedGroup() != nullptr) {
        for (Group *group : keyframe->selection().selectedPostGroups()) group->setShowGrid(on);
        m_editor->scene()->selectedGroupChanged(on ? QHash<int, Group *>() : keyframe->selection().selectedPostGroups());
        m_editor->tabletCanvas()->updateCurrentFrame();
    }
    // curve editor
    // if (on && m_currentTrajectory != nullptr) {
    //     int currentFrame = m_editor->playback()->currentFrame();
    //     emit showKeyframedVectorCurves(m_currentTrajectory->curve());
    // } else {
    //     emit showKeyframedVectorCurves(nullptr);
    // }
}

void TrajectoryTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;

    int layerIdx = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = info.key->parentLayer();
    bool leftButtonPressed = info.mouseButton & Qt::LeftButton;
    bool controlPressed = info.modifiers & Qt::ControlModifier;

    if (leftButtonPressed && !controlPressed) {
        Group *selectedGroup = info.key->selectedGroup();
        Point::VectorType pos = QE_POINT(info.pos);
        m_tickPressed = false;

        int selectedTrajectoryId = m_editor->selection()->selectTrajectoryConstraint(info.key, info.pos, true);
        if (selectedTrajectoryId >= 0) {
            bool pickedAlreadySelected = info.key->selection().selectedTrajectoryPtr() != nullptr && selectedTrajectoryId == info.key->selection().selectedTrajectory()->constraintID();
            const std::shared_ptr<Trajectory> &selectedTrajectory = info.key->trajectoryConstraint(selectedTrajectoryId);
            m_editor->undoStack()->push(new SetSelectedTrajectoryCommand(m_editor, layerIdx, currentFrame, selectedTrajectory, true));

            // Check if a tick was clicked
            if (pickedAlreadySelected) {
                int stride = layer->stride(layer->getVectorKeyFramePosition(info.key));
                for (int i = 1; i < stride; ++i) {
                    float alphaLinear = (float)i/(float)stride;
                    float alpha = info.key->selectedGroup()->spacingAlpha(alphaLinear);
                    selectedTrajectory->localOffset()->frameChanged(alphaLinear);
                    Point::VectorType p = selectedTrajectory->eval(alpha + selectedTrajectory->localOffset()->get());
                    if ((pos - p).norm() <= 2.0) {
                        m_tickPressed = true;
                        m_tickPressedIdx = i;
                        break;
                    }
                }

                // Propagate the trajectory forward if the endpoint is clicked
                if (!m_tickPressed && (pos - selectedTrajectory->cubicApprox().getP3()).norm() < 2.0) {
                    propagateTrajectoryForward(layer, info.key, layerIdx, currentFrame, selectedTrajectory->cubicApprox().getP3());
                }
            }
        } else {
            // Pick a point a lattice (or multiple) and visualize its trajectory
            pickInGrids(info.key, info.alpha, info.inbetween, layerIdx, currentFrame, pos);
        }
    } else if (leftButtonPressed && controlPressed && info.key->selection().selectedTrajectoryPtr() && !info.key->selection().selectedTrajectoryPtr()->hardConstraint()) {
        if (!m_trajectories.empty()) {
            for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
                m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, traj));
            }
            moveLatticesTargetConfiguration();
        } else {
            m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, info.key->selection().selectedTrajectory()));
        }
        m_trajectories.clear();
    } else if (leftButtonPressed && controlPressed && info.key->selection().selectedTrajectoryPtr()) {
        // TODO: cleanup
        // Reset local offset
        int stride = layer->stride(layer->getVectorKeyFramePosition(info.key));
        Trajectory *traj = info.key->selection().selectedTrajectoryPtr();
        for (int i = 1; i < stride; ++i) {
            float alphaLinear = (float)i/(float)stride;
            float alpha = info.key->selectedGroup()->spacingAlpha(alphaLinear);
            traj->localOffset()->frameChanged(alphaLinear);
            Point::VectorType p = traj->eval(alpha + info.key->selection().selectedTrajectory()->localOffset()->get());
            if ((Point::VectorType(info.pos.x(), info.pos.y()) - p).norm() <= 2.0) {
                for (int j = 0; j < traj->localOffset()->curve()->nbPoints(); ++j) {
                    Eigen::Vector2f p = traj->localOffset()->curve()->point(j);
                    traj->localOffset()->curve()->setKeyframe(Eigen::Vector2f(p.x(), 0.0f), j);
                }
                info.key->makeInbetweensDirty();
                return;
            }
        }
        m_editor->undoStack()->push(new RemoveTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, info.key->selection().selectedTrajectory()));
    }
}

void TrajectoryTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;

    if (m_tickPressed) {
        Trajectory *traj = info.key->selection().selectedTrajectoryPtr();
        Layer *layer = m_editor->layers()->currentLayer();
        int stride = layer->stride(layer->getVectorKeyFramePosition(info.key));
        QVector2D disp(info.pos - info.lastPos);
        Point::VectorType dispE(disp.x(), disp.y());
        float delta = disp.length();
        float len = traj->approxPathItem().length();
        float ds = delta / len;
        float alphaLinear = (float)m_tickPressedIdx / (float)stride;

        traj->localOffset()->frameChanged(alphaLinear);
        Point::VectorType t = traj->cubicApprox().evalDer(traj->group()->spacingAlpha(alphaLinear) + traj->localOffset()->get());
        float sgn = dispE.dot(t) > 0.0 ? 1.0 : -1.0;

        // TODO: cleanup
        Eigen::Vector2f p = traj->localOffset()->curve()->point(m_tickPressedIdx);
        Eigen::Vector2f prevOffset = traj->localOffset()->curve()->point(m_tickPressedIdx-1);
        Eigen::Vector2f nextOffset = traj->localOffset()->curve()->point(m_tickPressedIdx+1);
        Eigen::Vector2f pPrevSpacing = traj->group()->spacing()->curve()->point(m_tickPressedIdx-1);
        Eigen::Vector2f pSpacing = traj->group()->spacing()->curve()->point(m_tickPressedIdx);
        Eigen::Vector2f pNextSpacing = traj->group()->spacing()->curve()->point(m_tickPressedIdx+1);
        if (pSpacing.y() + p.y() + ds * sgn <= pPrevSpacing.y() + prevOffset.y() + 1e-5 || pSpacing.y() + p.y() + ds * sgn >= pNextSpacing.y() + nextOffset.y() - 1e-5) return;
        info.key->selection().selectedTrajectory()->localOffset()->curve()->setKeyframe(Eigen::Vector2f(p.x(), p.y() += ds * sgn), m_tickPressedIdx);
        info.key->selection().selectedTrajectory()->localOffset()->frameChanged(1.0);
        info.key->makeInbetweensDirty();
        return;
    }
}

void TrajectoryTool::released(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;
    if (m_tickPressed) m_tickPressed = false;
}

void TrajectoryTool::doublepressed(const EventInfo& info) {

}

void TrajectoryTool::wheel(const WheelEventInfo& info) {

}

void TrajectoryTool::draw(QPainter &painter, VectorKeyFrame *key) {
    static QPen pen(QColor(200, 200, 200), 2.0);
    Layer *layer = key->parentLayer();
    int stride = layer->stride(layer->getVectorKeyFramePosition(key));
    Trajectory *selectedTraj = key->selection().selectedTrajectoryPtr();
    pen.setCapStyle(Qt::RoundCap);

    drawNonSelectedGroupTraj(painter, pen, key, stride);

    drawSelectedTraj(painter, pen, key, k_drawChain);

    // draw tick
    // TODO cleanup
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(40, 0, 0));
    if (selectedTraj && selectedTraj->hardConstraint()) {
        for (int i = 0; i < stride + 1; ++i) {
            float alphaLinear = (float)i/(float)stride;
            selectedTraj->localOffset()->frameChanged(alphaLinear);
            Point::VectorType p = selectedTraj->eval(selectedTraj->group()->spacingAlpha(alphaLinear) + selectedTraj->localOffset()->get());
            painter.drawEllipse(QRectF(p.x() - 2.0, p.y() - 2.0, 4.0, 4.0));
        }
    }
}

void TrajectoryTool::drawNonSelectedGroupTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, int stride) {
    Trajectory *selectedTraj = key->selection().selectedTrajectoryPtr();

    m_editor->updateInbetweens(key, stride, stride);

    // draw all trajectories of selected groups
    for (Group *selectedGroup : key->selection().selectedPostGroups()) {
        // force the display of the last and next KF 
        selectedGroup->drawGrid(painter, 0, REF_POS);
        selectedGroup->drawGrid(painter, 0, TARGET_POS);

        // draw all constrained trajectories
        pen.setColor(QColor(200, 200, 200));
        painter.setPen(pen);
        for (const std::shared_ptr<Trajectory> &traj : key->trajectories()) {
            if (traj->group() != selectedGroup || traj.get() == selectedTraj) continue;
            painter.drawPath(traj->approxPathItem());
            Trajectory *t = traj.get();
            while (t->nextTrajectory() != nullptr) {
                t = t->nextTrajectory().get();
                painter.drawPath(t->approxPathItem());
            }
            t = traj.get();
            while (t->prevTrajectory() != nullptr) {
                t = t->prevTrajectory().get();
                painter.drawPath(t->approxPathItem());
            }
        }

        // (TMP) draw the more accurate piecewise traj
        if (k_showOriginalTraj && selectedTraj) {
            pen.setColor(QColor(255, 128, 0, 40));
            painter.setPen(pen);
            painter.drawPath(selectedTraj->pathItem());
        }
    }
}

void TrajectoryTool::drawSelectedTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, bool drawFullPath) {
    // draw the currently selected trajectory
    Trajectory *selectedTraj = key->selection().selectedTrajectoryPtr();

    if (selectedTraj) {
        pen.setColor(QColor(200, 20, 30));
        painter.setPen(pen);
        painter.drawPath(selectedTraj->approxPathItem());

        // Draw the complete trajectory (with previous and next segments)
        if (drawFullPath) {
            Trajectory *traj = key->selection().selectedTrajectoryPtr();
            while (traj->nextTrajectory() != nullptr) {
                traj = traj->nextTrajectory().get();
                painter.drawPath(traj->approxPathItem());
            } 
            traj = key->selection().selectedTrajectoryPtr();
            while (traj->prevTrajectory() != nullptr) {
                traj = traj->prevTrajectory().get();
                painter.drawPath(traj->approxPathItem());
            } 
        }
    }
}

/**
 * Construct the set of trajectories starting from the point "pos" in the selected grid(s).
 * If there are multiple grids selected and "pos" intersects at least 2, then one is designated as the "parent trajectory".
 * All other trajectories are set to follow the path of the parent trajectory.
 * If "setSelection" is true the parent trajectory is selected in the interface.
 * All trajectories starting from "pos" are stored in the class member "m_trajectories"
 * 
 * @param key the current KF (or last KF if inbetween>0)
 * @param alpha linear interpolation factor (0.0 if KF)
 * @param inbetween the current inbetween number (0 if KF)
 * @param layerIdx the current layer idx in the timeline
 * @param currentFrame the current frame number in the timeline
 * @param pos the query position
 * @param setSelection if true the parent trajectory is selected in the interface
 * @return the parent trajectory
 */
std::shared_ptr<Trajectory> TrajectoryTool::pickInGrids(VectorKeyFrame *key, float alpha, int inbetween, int layerIdx, int currentFrame, Point::VectorType pos, bool setSelection) {
    m_trajectories.clear();

    // Check if pos intersects one or more grid (from the selected groups)
    QHash<int, UVInfo> latticeCoords;
    for (Group *group : key->selection().selectedPostGroups()) {
        UVInfo latticeCoord;
        if (inbetween == 0) latticeCoord.uv = group->lattice()->getUV(pos, REF_POS, latticeCoord.quadKey);
        else                latticeCoord.uv = key->inbetweens()[inbetween - 1].getUV(group, pos, latticeCoord.quadKey);
        if (latticeCoord.quadKey == INT_MAX) continue;
        latticeCoords.insert(group->id(), latticeCoord);
    }

    // Create the trajectory (or trajectories) from starting from pos, they are not constraint but simply for visualization 
    std::shared_ptr<Trajectory> parent;
    if (!latticeCoords.empty()) {
        // Sample and store all trajectories starting from pos in the selected grids
        for (auto it = latticeCoords.constBegin(); it != latticeCoords.constEnd(); ++it) {
            m_trajectories.push_back(std::make_shared<Trajectory>(key, key->postGroups().fromId(it.key()), it.value()));
        }

        // Determine the parent trajectory
        // TODO: should be user determined
        int minId = std::numeric_limits<int>::max();
        for (const std::shared_ptr<Trajectory> &t : m_trajectories) {
            if (t->group()->id() < minId) {
                minId = t->group()->id();
                parent = t;
            }
        }

        // Set parent/child relationship between the trajectories
        for (int i = 0; i < m_trajectories.size(); ++i) {
            if (m_trajectories[i] == parent) continue;
            parent->addChild(m_trajectories[i]);
            m_trajectories[i]->setParent(parent);
        }
    }
    
    if (setSelection) {
        m_editor->undoStack()->push(new SetSelectedTrajectoryCommand(m_editor, layerIdx, currentFrame, parent, parent == nullptr));
    } 

    return parent;
}

// for now, we only consider the *selected* groups in the next KF
void TrajectoryTool::propagateTrajectoryForward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos) {
    int nextFrame = layer->getNextFrameNumber(frame, true);
    int maxFrame = layer->getMaxKeyFramePosition();
    VectorKeyFrame *next = layer->getNextKey(key);

    if (next == nullptr || next == key) {
        qCritical() << "Error in propagate trajectory. Next frame: " << next << " (" << layer->getVectorKeyFramePosition(next) << ")";
        return;
    }
    
    // either propagate the trajectory to the next KF or if it was already propagated sync/unsync the connected tangents
    if (nextFrame != maxFrame) {
        const auto &selectedTraj = key->selection().selectedTrajectory();
        if (selectedTraj->nextTrajectory() != nullptr) {
            // toggle sync with next trajectory
            if (!selectedTraj->syncNext()) {
                m_editor->undoStack()->push(new SyncTrajectoriesCommand(m_editor, layerIdx, frame, selectedTraj, selectedTraj->nextTrajectory()));
            } else {
                m_editor->undoStack()->push(new UnsyncTrajectoriesCommand(m_editor, layerIdx, frame, selectedTraj, selectedTraj->nextTrajectory()));
            }
        } else {
            // propagate selected trajectory
            // TODO propagation in multiple groups
            m_trajectories.clear();
            std::shared_ptr<Trajectory> propagatedTraj = pickInGrids(next, 0.0f, 0, layerIdx, frame, pos, false);
            if (!m_trajectories.empty()) {
                for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
                    if (traj == propagatedTraj) m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, nextFrame, traj, selectedTraj, false));
                    else m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, nextFrame, traj));
                }
                moveLatticesTargetConfiguration();
            } else {
                if (propagatedTraj != nullptr) m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, nextFrame, propagatedTraj, selectedTraj, false));
            }
            m_trajectories.clear();
        }

    }
}

void propagateTrajectoryBackward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos) {

}

// assume new trajectories constraint have been added
void TrajectoryTool::moveLatticesTargetConfiguration() {
    if (m_trajectories.empty()) return;
    Group *group;
    Point::Affine globalRigidTransform = m_trajectories[0]->keyframe()->rigidTransform(1.0);
    VectorKeyFrame *next = m_trajectories[0]->group()->getParentKeyframe()->nextKeyframe();
    for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
        group = traj->group();
        group->lattice()->precompute();
        group->lattice()->interpolateARAP(1.0, 1.0, globalRigidTransform);
        group->lattice()->copyPositions(group->lattice(), INTERP_POS, TARGET_POS);
        group->lattice()->setArapDirty();
        group->syncTargetPosition(next);
    }
}
