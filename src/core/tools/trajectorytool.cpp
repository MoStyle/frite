/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "trajectorytool.h"
#include "group.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "gridmanager.h"

#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "selectionmanager.h"
#include "canvascommands.h"
#include "viewmanager.h"
#include "bezier2D.h"
#include "qteigen.h"
#include "utils/geom.h"

typedef std::chrono::milliseconds ms;
typedef std::chrono::duration<double> dsec;
typedef std::chrono::system_clock _clock;

static dkBool k_showOriginalTraj("Options->Trajectory->Show original piecewise trajectory", false);
dkBool k_drawChain("Options->Trajectory->Draw full chain", true);

TrajectoryTool::TrajectoryTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click: visualize trajectory | Ctrl+Left-click: add/remove trajectory constraint");
    m_lastMoveTick = _clock::now();
}

TrajectoryTool::~TrajectoryTool() {

}

Tool::ToolType TrajectoryTool::toolType() const {
    return Tool::Traj;
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
    }
}

void TrajectoryTool::pressed(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr) {
        return;
    }

    int layerIdx = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = info.key->parentLayer();
    Group *selectedGroup = info.key->selectedGroup();
    Trajectory *traj = info.key->selection().selectedTrajectoryPtr();
    bool leftButtonPressed = info.mouseButton & Qt::LeftButton;
    bool controlPressed = info.modifiers & Qt::ControlModifier;
    bool trajectorySelected = traj != nullptr;
    m_tickPressed = false;
    m_tangentControlPressed = false;

    if (leftButtonPressed && !controlPressed) {
        Point::VectorType pos = QE_POINT(info.pos);
        pos = selectedGroup->globalRigidTransform(info.alpha).inverse() * pos;

        if (trajectorySelected) {
            Point::VectorType p1 = traj->cubicApprox().getP1();
            Point::VectorType p2 = traj->cubicApprox().getP2();
            if ((pos - p1).norm() < 4.0) {
                m_tangentControlPressed = true;
                m_tangentControlPressedIdx = 0;
                return;
            }
            if ((pos - p2).norm() < 4.0) {
                m_tangentControlPressed = true;
                m_tangentControlPressedIdx = 1;
                return;
            }
        }

        int selectedTrajectoryId = m_editor->selection()->selectTrajectoryConstraint(info.key, info.pos, true);
        // Clicked on a trajectory
        if (selectedTrajectoryId >= 0) { 
            bool pickedAlreadySelected = trajectorySelected && selectedTrajectoryId == traj->constraintID();
            const std::shared_ptr<Trajectory> &selectedTrajectory = info.key->trajectoryConstraint(selectedTrajectoryId);
            m_editor->undoStack()->push(new SetSelectedTrajectoryCommand(m_editor, layerIdx, currentFrame, selectedTrajectory, true));

            // Check if a tangent control was clicked 
            if (pickedAlreadySelected) {
                // Check if a tick was clicked
                int stride = layer->stride(layer->getVectorKeyFramePosition(info.key));
                for (int i = 1; i < stride; ++i) {
                    qreal alphaLinear = (float)i/(float)stride;
                    qreal alpha = info.key->selectedGroup()->spacingAlpha(alphaLinear);
                    selectedTrajectory->localOffset()->frameChanged(alphaLinear);
                    Point::VectorType p = selectedTrajectory->eval(alpha + selectedTrajectory->localOffset()->get());
                    if ((pos - p).norm() <= 2.0) {
                        m_tickPressed = true;
                        m_tickPressedIdx = i;
                        break;
                    }
                }

                // Propagate the trajectory forward if the endpoint is clicked
                float r = (m_tickPressedIdx == 0 || m_tickPressedIdx == stride) ? 4.0 : 2.0;
                if (!m_tickPressed && (pos - selectedTrajectory->cubicApprox().getP3()).norm() < 2.0) {
                    propagateTrajectoryForward(layer, info.key, layerIdx, currentFrame, selectedTrajectory->cubicApprox().getP3());
                }
            }
        } else {
            // Pick a point a lattice (or multiple) and visualize its trajectory
            pickInGrids(info.key, info.alpha, info.inbetween, layerIdx, currentFrame, pos);
        }
        return;
    } 
    
    // Add hard constraint
    if (leftButtonPressed && controlPressed && trajectorySelected && !traj->hardConstraint()) {
        if (!m_trajectories.empty()) {
            for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
                m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, traj));
            }
            moveLatticesTargetConfiguration();
        } else {
            m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, info.key->selection().selectedTrajectory()));
        }
        m_trajectories.clear();
        return;
    }
    
    // Remove hard constraint (Ctrl+Left click)
    if (leftButtonPressed && controlPressed && trajectorySelected) {
        // TODO: cleanup
        // Reset local offset
        int stride = layer->stride(layer->getVectorKeyFramePosition(info.key));
        for (int i = 1; i < stride; ++i) {
            qreal alphaLinear = (qreal)i/(qreal)stride;
            qreal alpha = info.key->selectedGroup()->spacingAlpha(alphaLinear);
            traj->localOffset()->frameChanged(alphaLinear);
            Point::VectorType p = traj->eval(alpha + info.key->selection().selectedTrajectory()->localOffset()->get());
            if ((Point::VectorType(info.pos.x(), info.pos.y()) - p).norm() <= 2.0) {
                for (int j = 0; j < traj->localOffset()->curve()->nbPoints(); ++j) {
                    Eigen::Vector2d p = traj->localOffset()->curve()->point(j);
                    traj->localOffset()->curve()->setKeyframe(Eigen::Vector2d(p.x(), 0.0f), j);
                }
                info.key->makeInbetweensDirty();
                return;
            }
        }
        m_editor->undoStack()->push(new RemoveTrajectoryConstraintCommand(m_editor, layerIdx, currentFrame, info.key->selection().selectedTrajectory()));
        return;
    }
}

void TrajectoryTool::moved(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr || dsec(_clock::now() - m_lastMoveTick).count() * 1000 < 8)
        return;

    Trajectory *traj = info.key->selection().selectedTrajectoryPtr();
    Point::VectorType pos(info.pos.x(), info.pos.y());

    if (m_tangentControlPressed) {
        if (m_tangentControlPressedIdx == 0) {
            traj->setP1(pos);
            // sync with prev tangent if it is connected
            if (traj->prevTrajectory() && traj->syncPrev()) {
                Point::VectorType t = traj->cubicApprox().getP1() - traj->cubicApprox().getP0(); 
                traj->prevTrajectory()->setP2(traj->prevTrajectory()->cubicApprox().getP3() - t);
                traj->prevTrajectory()->keyframe()->makeInbetweensDirty();
            }
        } else if (m_tangentControlPressedIdx == 1) {
            traj->setP2(pos);
            // sync with next tangent if it is connected
            if (traj->nextTrajectory() && traj->syncNext()) {
                Point::VectorType t = traj->cubicApprox().getP2() - traj->cubicApprox().getP3(); 
                traj->nextTrajectory()->setP1(traj->nextTrajectory()->cubicApprox().getP0() - t);
                traj->nextTrajectory()->keyframe()->makeInbetweensDirty();
            }            
        }
        info.key->makeInbetweensDirty();
        return;
    }

    if (m_tickPressed) {
        Layer *layer = m_editor->layers()->currentLayer();
        int stride = layer->stride(layer->getVectorKeyFramePosition(info.key));
        QVector2D disp(info.pos - info.lastPos);
        Point::VectorType dispE(disp.x(), disp.y());
        float delta = disp.length();
        float len = traj->approxPathItem().length();
        float ds = delta / len;
        qreal alphaLinear = (float)m_tickPressedIdx / (float)stride;

        traj->localOffset()->frameChanged(alphaLinear);
        Point::VectorType t = traj->cubicApprox().evalDer(traj->group()->spacingAlpha(alphaLinear) + traj->localOffset()->get());
        float sgn = dispE.dot(t) > 0.0 ? 1.0 : -1.0;

        // TODO: cleanup
        Eigen::Vector2d p = traj->localOffset()->curve()->point(m_tickPressedIdx);
        Eigen::Vector2d prevOffset = traj->localOffset()->curve()->point(m_tickPressedIdx-1);
        Eigen::Vector2d nextOffset = traj->localOffset()->curve()->point(m_tickPressedIdx+1);
        Eigen::Vector2d pPrevSpacing = traj->group()->spacing()->curve()->point(m_tickPressedIdx-1);
        Eigen::Vector2d pSpacing = traj->group()->spacing()->curve()->point(m_tickPressedIdx);
        Eigen::Vector2d pNextSpacing = traj->group()->spacing()->curve()->point(m_tickPressedIdx+1);
        if (pSpacing.y() + p.y() + ds * sgn <= pPrevSpacing.y() + prevOffset.y() + 1e-5 || pSpacing.y() + p.y() + ds * sgn >= pNextSpacing.y() + nextOffset.y() - 1e-5) return;
        info.key->selection().selectedTrajectory()->localOffset()->curve()->setKeyframe(Eigen::Vector2d(p.x(), p.y() += ds * sgn), m_tickPressedIdx);
        info.key->selection().selectedTrajectory()->localOffset()->frameChanged(1.0);
        info.key->makeInbetweensDirty();
        return;
    }

    if (!m_tickPressed && (info.key->selection().selectedTrajectoryPtr() == nullptr || !info.key->selection().selectedTrajectory()->hardConstraint())) {
        int layerIdx = m_editor->layers()->currentLayerIndex();
        int currentFrame = m_editor->playback()->currentFrame();
        pickInGrids(info.key, info.alpha, info.inbetween, layerIdx, currentFrame, QE_POINT(info.pos));
    }

    m_lastMoveTick = _clock::now();
}

void TrajectoryTool::released(const EventInfo& info) {
    if (info.key->selectedGroup() == nullptr || info.key->selectedGroup()->lattice() == nullptr)
        return;
    m_tickPressed = false;
    m_tangentControlPressed = false;
}

void TrajectoryTool::doublepressed(const EventInfo& info) {

}

void TrajectoryTool::wheel(const WheelEventInfo& info) {

}

void TrajectoryTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
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
        float r;
        for (int i = 0; i < stride + 1; ++i) {
            qreal alphaLinear = (qreal)i/(qreal)stride;
            selectedTraj->localOffset()->frameChanged(alphaLinear);
            Point::VectorType p = selectedTraj->eval(selectedTraj->group()->spacingAlpha(alphaLinear) + selectedTraj->localOffset()->get());
            r = (i == 0 || i == stride) ? 8.0 : 4.0;
            painter.drawEllipse(QRectF(p.x() - r * 0.5, p.y() - r * 0.5, r, r));
        }
    }

    if (selectedTraj && selectedTraj->hardConstraint()) {
        // draw tangents
        Point::VectorType p0 = selectedTraj->cubicApprox().getP0();
        Point::VectorType p1 = selectedTraj->cubicApprox().getP1();
        Point::VectorType p2 = selectedTraj->cubicApprox().getP2();
        Point::VectorType p3 = selectedTraj->cubicApprox().getP3();
        pen.setColor(QColor(0, 0, 0, 40));
        pen.setWidthF(1.5);
        painter.setPen(pen);
        painter.drawLine(QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y()));
        painter.drawLine(QPointF(p3.x(), p3.y()), QPointF(p2.x(), p2.y()));
        // painter.setPen(Qt::black);
        painter.fillRect(QRectF(p1.x() - 4.0, p1.y() - 4.0, 8.0, 8.0), QBrush(QColor(0, 0, 0, 40)));
        painter.fillRect(QRectF(p2.x() - 4.0, p2.y() - 4.0, 8.0, 8.0), QBrush(QColor(0, 0, 0, 40)));
    }
}

void TrajectoryTool::drawNonSelectedGroupTraj(QPainter &painter, QPen &pen, VectorKeyFrame *key, int stride) {
    Trajectory *selectedTraj = key->selection().selectedTrajectoryPtr();

    m_editor->updateInbetweens(key, stride, stride);

    // draw all trajectories of selected groups
    for (Group *selectedGroup : key->selection().selectedPostGroups()) {
        // force the display of the last and next KF 
        // selectedGroup->drawGrid(painter, 0, REF_POS);
        // selectedGroup->drawGrid(painter, 0, TARGET_POS);

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
        QPainterPath path = applyRigidTransformToPathFromTraj(selectedTraj);
        // painter.drawPath(selectedTraj->pathItem());
        painter.drawPath(path);

        // Draw the complete trajectory (with previous and next segments)
        if (drawFullPath) {
            Trajectory *traj = key->selection().selectedTrajectoryPtr();
            while (traj->nextTrajectory() != nullptr) {
                traj = traj->nextTrajectory().get();
                QPainterPath path = applyRigidTransformToPathFromTraj(traj);
                painter.drawPath(path);
            } 
            traj = key->selection().selectedTrajectoryPtr();
            while (traj->prevTrajectory() != nullptr) {
                traj = traj->prevTrajectory().get();
                QPainterPath path = applyRigidTransformToPathFromTraj(traj);
                painter.drawPath(path);
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
std::shared_ptr<Trajectory> TrajectoryTool::pickInGrids(VectorKeyFrame *key, qreal alpha, int inbetween, int layerIdx, int currentFrame, Point::VectorType pos, bool setSelection) {
    m_trajectories.clear();

    // Check if pos intersects one or more grid (from the selected groups)
    QHash<int, UVInfo> latticeCoords;
    for (Group *group : key->selection().selectedPostGroups()) {
        UVInfo latticeCoord;
        if (inbetween == 0) latticeCoord.uv = group->lattice()->getUV(pos, REF_POS, latticeCoord.quadKey);
        else                latticeCoord.uv = key->inbetweens()[inbetween].getUV(group, pos, latticeCoord.quadKey);
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

void TrajectoryTool::propagateTrajectoryBackward(Layer *layer, VectorKeyFrame *key, int layerIdx, int frame, Point::VectorType pos) {

}

// assume new trajectories constraint have been added
void TrajectoryTool::moveLatticesTargetConfiguration() {
    if (m_trajectories.empty()) return;
    Group *group;
    VectorKeyFrame *next = m_trajectories[0]->group()->getParentKeyframe()->nextKeyframe();
    for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
        group = traj->group();
        group->lattice()->precompute();
        group->lattice()->interpolateARAP(1.0, 1.0, group->globalRigidTransform(1.0f));
        group->lattice()->copyPositions(group->lattice(), INTERP_POS, TARGET_POS);
        group->setGridDirty();
        group->syncTargetPosition(next);
    }
}

QPainterPath TrajectoryTool::applyRigidTransformToPathFromTraj(Trajectory * traj){
    static int samples = 100;
    QPainterPath path;
    Bezier2D cubicApprox = traj->cubicApprox();
    Point::VectorType point = traj->group()->globalRigidTransform(0) * cubicApprox.evalArcLength(0);
    path.moveTo(QE_POINT(point).x(), QE_POINT(point).y());
    for (int i = 1; i < samples; ++i){
        qreal alpha = qreal(i) / (samples - 1);
        Point::VectorType point = traj->group()->globalRigidTransform(alpha) * cubicApprox.evalArcLength(alpha);
        path.lineTo(QE_POINT(point).x(), QE_POINT(point).y());
    }
    return path;
}