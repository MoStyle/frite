#include "grouporderingtool.h"
#include "group.h"
#include "grouporder.h"
#include "editor.h"
#include "selectionmanager.h"
#include "viewmanager.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "fixedscenemanager.h"
#include "charttickitem.h"
#include "trajectory.h"
#include "mask.h"
#include "qteigen.h"
#include "dialsandknobs.h"

#include <QFontMetrics>

static dkBool k_displayMaskOutline("Options->Drawing->Display mask outline", false);


GroupOrderingTool::GroupOrderingTool(QObject *parent, Editor *editor) : ChartTool(parent, editor) , m_prevSelectedGroup(Group::ERROR_ID), m_fontMetrics(m_editor->tabletCanvas()->canvasFont()), m_savedState(Partials<OrderPartial>(nullptr, OrderPartial(nullptr, 0.0))) {
    m_toolTips = QString("Left-click: set group above.. | Ctrl+Left-click: same depth | Shift+Left-click: set group behind..");
    m_contextMenuAllowed = true;
    m_chartMode = ChartItem::PARTIAL;
    m_partialTickPressed = false;
    m_partialTickPressedId = -1;
    m_partialTrajectoryPressed = nullptr;

    connect(&k_displayMaskOutline, SIGNAL(valueChanged(bool)), m_editor->tabletCanvas(), SLOT(updateCurrentFrame(void)));
}

GroupOrderingTool::~GroupOrderingTool() {

}

Tool::ToolType GroupOrderingTool::toolType() const {
    return Tool::GroupOrdering;
}

QCursor GroupOrderingTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void GroupOrderingTool::toggled(bool on) {
    ChartTool::toggled(on);
    m_editor->tabletCanvas()->setMouseTracking(on);
    m_editor->tabletCanvas()->setTabletTracking(on);
    m_editor->tabletCanvas()->fixedCanvasView()->setMouseTracking(on);
    m_editor->tabletCanvas()->setMaskOcclusionMode(on ? TabletCanvas::MaskGrayOut : TabletCanvas::MaskOcclude);
    m_editor->tabletCanvas()->setDisplayMask(on);
    m_editor->tabletCanvas()->setDisplaySelectedGroupsLifetime(!on);
    m_editor->tabletCanvas()->setDisplayDepth(on);
    int frame = m_editor->playback()->currentFrame();
    VectorKeyFrame *key = m_editor->layers()->currentLayer()->getLastVectorKeyFrameAtFrame(frame, 0);
    m_prevKeyframe = key;
    if (on) {
        key->orderPartials().saveState();
    } else {
        restoreAndClearState();
    }
}

void GroupOrderingTool::pressed(const EventInfo& info) {
    m_partialTickPressed = false;

    if (m_editor->currentAlpha() >= 1.0) return;

    if (info.key->selectedGroup() == nullptr) {
        return;
    }

    tickPressed(info);
}

void GroupOrderingTool::moved(const EventInfo& info) {
    if (m_editor->currentAlpha() >= 1.0) return;

    if (info.key->selectedGroup() == nullptr) {
        return;
    }

    if (m_partialTickPressed) {
        tickMoved(info);
        return;
    }

    int groupId = m_editor->selection()->selectGroups(info.key, info.alpha, info.inbetween, POST, info.pos, true);

    if (groupId == Group::ERROR_ID && m_prevSelectedGroup != Group::ERROR_ID) {
        info.key->orderPartials().restoreState();
        m_prevSelectedGroup = groupId;
        return;
    }

    if (groupId == info.key->selectedGroup()->id() || groupId == Group::ERROR_ID || groupId == m_prevSelectedGroup) return;

    double dt = 1.0 / info.stride;
    double partialAlpha = info.alpha <= 1e-6 ? 0.0 : info.alpha - dt * 0.5;

    if (info.inbetween > 0) {
        info.key->orderPartials().removeAfter(info.inbetween - 1, info.stride);
        info.key->orderPartials().insertPartial(OrderPartial(info.key, partialAlpha, GroupOrder(info.key->groupOrder())));
    }

    if (info.modifiers & Qt::ControlModifier) {
        for (Group *group : info.key->selection().selectedPostGroups()) {
            info.key->groupOrder(partialAlpha).sameDepth(groupId, group->id());
        }
    } else if (info.modifiers & Qt::ShiftModifier) {
        auto it = info.key->selection().selectedPostGroups().begin();

        info.key->groupOrder(partialAlpha).debug();
        int newDepth = info.key->groupOrder(partialAlpha).setAUnderB(it.key(), groupId);
        ++it;
        info.key->groupOrder(partialAlpha).debug();
        for (; it != info.key->selection().selectedPostGroups().end(); ++it) {
            info.key->groupOrder(partialAlpha).add(it.key(), newDepth);
        }
        info.key->groupOrder(partialAlpha).debug();
    } else {
        auto it = info.key->selection().selectedPostGroups().begin();
        int newDepth = info.key->groupOrder(partialAlpha).setAOnTopOfB(it.key(), groupId);
        ++it;
        for (; it != info.key->selection().selectedPostGroups().end(); ++it) {
            info.key->groupOrder(partialAlpha).add(it.key(), newDepth);
        }
    }

    m_prevSelectedGroup = groupId;
}

void GroupOrderingTool::released(const EventInfo& info) {
    if (m_editor->currentAlpha() >= 1.0) return;

    if (info.key->selectedGroup() == nullptr) {
        info.key->orderPartials().syncWithFrames(info.stride);
        info.key->orderPartials().saveState(); 
        return;
    }

    if (m_partialTickPressed) {
        tickReleased(info);
        return;
    }

    double dt = 1.0 / info.stride;
    double partialAlpha = info.alpha <= 1e-6 ? 0.0 : info.alpha - dt * 0.5;

    if (info.mouseButton & Qt::LeftButton) {
        int groupId = m_editor->selection()->selectGroups(info.key, info.alpha, info.inbetween, POST, info.pos, true);

        if (groupId == info.key->selectedGroup()->id() || groupId == Group::ERROR_ID) return;

        info.key->orderPartials().restoreState();
        const Partials<OrderPartial> &prevPartials = info.key->orderPartials(); 

        if (info.inbetween > 0) {
            info.key->orderPartials().removeAfter(info.inbetween - 1, info.stride);
            info.key->orderPartials().insertPartial(OrderPartial(info.key, partialAlpha, GroupOrder(info.key->groupOrder())));
        }

        if (info.modifiers & Qt::ControlModifier) {
            for (Group *group : info.key->selection().selectedPostGroups()) {
                info.key->groupOrder(partialAlpha).sameDepth(groupId, group->id());
            }
        } else if (info.modifiers & Qt::ShiftModifier) {
            auto it = info.key->selection().selectedPostGroups().begin();
            int newDepth = info.key->groupOrder(partialAlpha).setAUnderB(it.key(), groupId);
            ++it;
            for (; it != info.key->selection().selectedPostGroups().end(); ++it) {
                info.key->groupOrder(partialAlpha).add(it.key(), newDepth);
            }
        } else {
            auto it = info.key->selection().selectedPostGroups().begin();
            int newDepth = info.key->groupOrder(partialAlpha).setAOnTopOfB(it.key(), groupId);
            ++it;
            for (; it != info.key->selection().selectedPostGroups().end(); ++it) {
                info.key->groupOrder(partialAlpha).add(it.key(), newDepth);
            }
        }

        removeIdenticalPartials(info.key);
        m_editor->undoStack()->push(new SetOrderPartialsCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), prevPartials));
    } else if (info.mouseButton & Qt::RightButton && info.key->orderPartials().exists(partialAlpha)) { // TODO: change button
        info.key->orderPartials().restoreState();
        m_editor->undoStack()->push(new RemoveOrderPartial(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), partialAlpha, info.key->orderPartials().lastPartialAt(partialAlpha)));
        removeIdenticalPartials(info.key);
        info.key->orderPartials().saveState(); 
    }

    m_editor->fixedScene()->updateKeyChart(info.key);
}

void GroupOrderingTool::doublepressed(const EventInfo& info) {
    
}

void GroupOrderingTool::tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    if (tick->tickType() != ChartTickItem::TICKORDERPARTIAL) return;
    const Partial *partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
    if (partial == nullptr) return;
    m_savedState = tick->chart()->keyframe()->orderPartials();
}

void GroupOrderingTool::tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    if (tick->tickType() != ChartTickItem::TICKORDERPARTIAL) return;
    const Partial *partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
    if (partial == nullptr) return;
    ChartItem *chart = tick->chart();
    QVector2D delta(event->pos() - event->lastPos());
    double deltaX = delta.length() / chart->length();
    if (event->pos().x() < event->lastPos().x()) deltaX = -deltaX;
    tick->move(deltaX);
    chart->update();
    // remap the partial timestamp from the spacing domain to the linear one
    tick->chart()->keyframe()->orderPartials().movePartial(partial->t(), tick->chart()->spacing()->evalInverse(tick->xVal()));
}

void GroupOrderingTool::tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    if (tick->tickType() != ChartTickItem::TICKORDERPARTIAL) return;
    const Partial *partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
    if (partial == nullptr) return;
    m_editor->undoStack()->push(new SyncOrderPartialCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedState));
    removeIdenticalPartials(tick->chart()->keyframe());
    tick->chart()->keyframe()->orderPartials().saveState(); 
    m_editor->fixedScene()->updateKeyChart(tick->chart()->keyframe());

}

void GroupOrderingTool::tickPressed(const EventInfo &info) {
    m_partialTickPressed = false;
    for (auto partial : info.key->orderPartials().partials()) {
        if (partial.t() == 0.0) continue;;
        for (Group *group : info.key->selection().selectedPostGroups()) {
            for (const std::shared_ptr<Trajectory> &traj : info.key->trajectories()) {
                traj->localOffset()->frameChanged(partial.t());
                Point::VectorType p = traj->eval(traj->group()->spacingAlpha(partial.t()) + traj->localOffset()->get());
                if ((QE_POINT(info.pos) - p).squaredNorm() <= 16.0) {
                    m_partialTickPressed = true;
                    m_partialTickPressedId = partial.id();
                    m_partialTrajectoryPressed = traj.get();
                    info.key->orderPartials().saveState();
                    return;
                }
            }
        }
    }
    m_savedState = info.key->orderPartials();
}

void GroupOrderingTool::tickMoved(const EventInfo &info) {
    if (!m_partialTickPressed || m_partialTrajectoryPressed == nullptr) {
        return;
    }

    Point::VectorType deltaMouse(info.pos.x() - info.lastPos.x(), info.pos.y() - info.lastPos.y());
    Point::Scalar deltaNorm = deltaMouse.norm();

    if (deltaNorm < 1e-6) return;

    const OrderPartial *partial = info.key->orderPartials().cpartial(m_partialTickPressedId);
    if (partial == nullptr) qCritical() << "Error in GroupOrderingTool::tickMoved: partial " << m_partialTickPressedId << " doesn't exist!";
    m_partialTrajectoryPressed->localOffset()->frameChanged(partial->t());
    double tPressed = partial->t() + m_partialTrajectoryPressed->localOffset()->get();
    Point::VectorType trajectoryTangent = m_partialTrajectoryPressed->evalVelocity(tPressed);
    float sgn = deltaMouse.dot(trajectoryTangent) > 0.0 ? 1.0f : -1.0f;
    double ds = sgn * deltaNorm / m_partialTrajectoryPressed->approxPathItem().length();
    double newT = std::clamp(partial->t() + ds, 1e-6, 1.0);
    info.key->orderPartials().movePartial(partial->t(), newT);
    m_editor->fixedScene()->updateKeyChart(info.key);
}

void GroupOrderingTool::tickReleased(const EventInfo &info) {
    if (!m_partialTickPressed || m_partialTrajectoryPressed == nullptr) {
        return;
    }

    m_editor->undoStack()->push(new SyncOrderPartialCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedState));
    removeIdenticalPartials(info.key);
    info.key->orderPartials().saveState();
    m_partialTickPressed = false;
    m_partialTrajectoryPressed = nullptr;
    m_editor->fixedScene()->updateKeyChart(info.key);
}

void GroupOrderingTool::tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {

}

void GroupOrderingTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    ChartTool::drawUI(painter, key);

    if (m_editor->currentAlpha() >= 1.0) return;
    m_fontMetrics = QFontMetrics(m_editor->tabletCanvas()->canvasFont());
    QPen p(Qt::NoBrush, 2.0);
    Layer *layer = m_editor->layers()->currentLayer();
    int inb = layer->inbetweenPosition(m_editor->playback()->currentFrame());
    int stride = layer->stride(layer->getVectorKeyFramePosition(key));

    // Draw order text
    const Inbetween &inbetween = key->inbetween(inb);
    double alpha = m_editor->currentAlpha();;
    // for (Group *group : key->postGroups()) {
    //     if (group->size() == 0) continue;
    //     p.setColor(group->color());
    //     painter.setPen(p);
    //     QString str = QString("%1").arg(key->groupOrder(alpha).nbDepths() - key->groupOrder(alpha).depthOf(group->id()) - 1);
    //     QRect textRect = m_fontMetrics.tightBoundingRect(str);
    //     painter.drawText(QPointF(inbetween.centerOfMass.value(group->id()).x() - (textRect.width() / 2), inbetween.centerOfMass.value(group->id()).y() + (textRect.height() / 2)), str);
    // }


    // Draw mask outline
    if (k_displayMaskOutline) {
        p.setStyle(Qt::DashLine);
        p.setWidthF(0.8f);
        for (Group *group : key->postGroups()) {
            if (group->mask() == nullptr) continue;

            int size = key->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;

            for (int i = size; i >= 0; --i) {
                const std::vector<int> &groups = key->orderPartials().lastPartialAt(alpha).groupOrder().order().at(i);
                QColor c = m_editor->tabletCanvas()->sampleColorMap(i + 0.25);
                p.setColor(c);
                painter.setPen(p);
                for (int groupId : groups) {
                    Group *group = key->postGroups().fromId(groupId);
                    QPolygonF polygon;
                    for (int j = 0; j < group->mask()->polygon().size(); ++j) {
                        auto point = group->mask()->polygon().at(j);
                        Point::VectorType p(point.x, point.y);
                        Mask::OutlineVertexInfo vtxInfo = group->mask()->vertexInfo().at(j);
                        p = inbetween.getWarpedPoint(group, UVInfo {vtxInfo.quadKey, vtxInfo.uv});
                        polygon.append(QPointF(p.x(), p.y()));
                    }
                    painter.drawPolygon(polygon);
                }
            }
        }
        p.setWidthF(2.0);
    }

    // Draw trajectories
    p.setColor(Qt::darkGray);
    painter.setPen(p);
    for (Group *selectedGroup : key->selection().selectedPostGroups()) {
        for (const std::shared_ptr<Trajectory> &traj : key->trajectories()) {
            painter.drawPath(traj->approxPathItem());
        }
    }

    // Draw frame ticks
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::darkGray);
    qreal h;
    for (Group *selectedGroup : key->selection().selectedPostGroups()) {
        for (const std::shared_ptr<Trajectory> &traj : key->trajectories()) {
            for (int i = 0; i < stride + 1; ++i) {
                qreal alphaLinear = (qreal)i/(qreal)stride;
                traj->localOffset()->frameChanged(alphaLinear);
                Point::VectorType p = traj->eval(traj->group()->spacingAlpha(alphaLinear) + traj->localOffset()->get());
                Point::VectorType velocityDir = traj->evalVelocity(traj->group()->spacingAlpha(alphaLinear) + traj->localOffset()->get()).normalized();
                h = (i == 0 || i == stride || i == inb) ? 12.0 : 6.0;
                QTransform transform = QTransform().translate(p.x(), p.y()).rotateRadians(atan2(velocityDir.y(), velocityDir.x()));
                QRectF tick(-1.0, -h*0.5, 2.0, h);
                painter.save();
                painter.setTransform(transform, true);
                painter.drawRect(tick);
                painter.restore();
            }
        }
    }

    // Draw order partial ticks
    painter.setBrush(QColor(255, 95, 31));
    for (auto partial : key->orderPartials().partials()) {
        if (partial.t() == 0.0) continue;
        for (Group *selectedGroup : key->selection().selectedPostGroups()) {
            for (const std::shared_ptr<Trajectory> &traj : key->trajectories()) {
                traj->localOffset()->frameChanged(partial.t());
                Point::VectorType p = traj->eval(traj->group()->spacingAlpha(partial.t()) + traj->localOffset()->get());
                Point::VectorType velocity = traj->evalVelocity(traj->group()->spacingAlpha(partial.t()) + traj->localOffset()->get());
                QTransform transform = QTransform().translate(p.x(), p.y()).rotateRadians(atan2(velocity.y(), velocity.x()) + M_PI_4);
                QRectF tick(-2.0, -2.0, 4.0, 4.0);
                painter.save();
                painter.setTransform(transform, true);
                painter.drawRect(tick);
                painter.restore();
            }
        }   
    }
}

void GroupOrderingTool::contextMenu(QMenu &contextMenu) {
    contextMenu.addSection("Group ordering");
    contextMenu.addAction(tr("Reset group order"), this, [&]() { 
        int layer = m_editor->layers()->currentLayerIndex();
        int frame = m_editor->playback()->currentFrame();
        double alpha = m_editor->alpha(frame);
        VectorKeyFrame *keyframe = m_editor->prevKeyFrame();
        OrderPartial prevPartial = keyframe->orderPartials().lastPartialAt(alpha);
        OrderPartial newPartial = prevPartial;
        newPartial.groupOrder().reset();
        m_editor->undoStack()->beginMacro("Reset group order"); 
        m_editor->undoStack()->push(new AddOrderPartial(m_editor, layer, frame, newPartial, prevPartial));
        removeIdenticalPartials(keyframe);
        keyframe->orderPartials().saveState(); 
        m_editor->undoStack()->endMacro(); 
    });
}

void GroupOrderingTool::restoreAndClearState() const {
    int frame = m_editor->playback()->currentFrame();
    VectorKeyFrame *key = m_editor->layers()->currentLayer()->getLastVectorKeyFrameAtFrame(frame, 0);    
    key->orderPartials().restoreState();
    key->orderPartials().removeSavedState();
}

void GroupOrderingTool::frameChanged(int frame) {
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(frame, 0);
    if (m_prevKeyframe != key) {
        key->orderPartials().saveState();
        m_prevKeyframe = key;
    }
}

void GroupOrderingTool::removeIdenticalPartials(VectorKeyFrame *keyframe) const {
    bool update = true;
    while (update) {
        update = false;
        QMutableMapIterator<double, OrderPartial> it(keyframe->orderPartials().partials());
        it.next();
        OrderPartial *partial = &(it.value());
        while (it.hasNext()) {
            it.next();
            if (it.value().compare(*partial)) {
                it.remove();
                update = true;
            } else {
                partial = &(it.value());
            }
        }
    }
}