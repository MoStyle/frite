#include "movepartialstool.h"
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
#include "qteigen.h"

MovePartialsTool::MovePartialsTool(QObject *parent, Editor *editor) : ChartTool(parent, editor) , m_prevSelectedGroup(Group::ERROR_ID), m_savedStateOrder(Partials<OrderPartial>(nullptr, OrderPartial(nullptr, 0.0))), m_savedStateDrawing(Partials<DrawingPartial>(nullptr, DrawingPartial(nullptr, 0.0))) {
    m_toolTips = QString("Left-click to move partials on the chart or trajectories | Double click partial to remove it");
    m_contextMenuAllowed = false;
    m_chartMode = ChartItem::PARTIAL;
    m_partialTickPressed = false;
    m_partialTickPressedId = -1;
    m_partialTrajectoryPressed = nullptr;
}

MovePartialsTool::~MovePartialsTool() {

}

Tool::ToolType MovePartialsTool::toolType() const {
    return Tool::MovePartials;
}

QCursor MovePartialsTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void MovePartialsTool::tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    const Partial *partial;
    if (tick->tickType() == ChartTickItem::TICKORDERPARTIAL) {
        partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
        m_savedStateOrder = tick->chart()->keyframe()->orderPartials();
    } else if (tick->tickType() == ChartTickItem::TICKDRAWINGPARTIAL && tick->chart()->keyframe()->selectedGroup() != nullptr) {
        partial = tick->chart()->keyframe()->selectedGroup()->drawingPartials().cpartial(tick->elementIdx());
        m_savedStateDrawing = tick->chart()->keyframe()->selectedGroup()->drawingPartials();
    } else {
        return;
    }
}

void MovePartialsTool::tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    const Partial *partial;
    if (tick->tickType() == ChartTickItem::TICKORDERPARTIAL) {
        partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
    } else if (tick->tickType() == ChartTickItem::TICKDRAWINGPARTIAL && tick->chart()->keyframe()->selectedGroup() != nullptr) {
        partial = tick->chart()->keyframe()->selectedGroup()->drawingPartials().cpartial(tick->elementIdx());
    } else {
        return;
    }
    if (partial == nullptr) return;
    ChartItem *chart = tick->chart();
    QVector2D delta(event->pos() - event->lastPos());
    double deltaX = delta.length() / chart->length();
    if (event->pos().x() < event->lastPos().x()) deltaX = -deltaX;
    tick->move(deltaX);
    chart->update();
    // remap the partial timestamp from the spacing domain to the linear one
    if (tick->tickType() == ChartTickItem::TICKORDERPARTIAL) {
       tick->chart()->keyframe()->orderPartials().movePartial(partial->t(), tick->chart()->spacing()->evalInverse(tick->xVal()));
    } else if (tick->tickType() == ChartTickItem::TICKDRAWINGPARTIAL) {
       tick->chart()->keyframe()->selectedGroup()->drawingPartials().movePartial(partial->t(), tick->chart()->spacing()->evalInverse(tick->xVal()));
    }
}

void MovePartialsTool::tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    const Partial *partial;
    if (tick->tickType() == ChartTickItem::TICKORDERPARTIAL) {
        partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
        if (partial == nullptr) return;
        m_editor->undoStack()->push(new SyncOrderPartialCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedStateOrder));
        removeIdenticalOrderPartials(tick->chart()->keyframe());
        tick->chart()->keyframe()->orderPartials().saveState(); 
    } else if (tick->tickType() == ChartTickItem::TICKDRAWINGPARTIAL && tick->chart()->keyframe()->selectedGroup() != nullptr) {
        partial = tick->chart()->keyframe()->selectedGroup()->drawingPartials().cpartial(tick->elementIdx());
        if (partial == nullptr) return;
        m_editor->undoStack()->push(new SyncDrawingPartialCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), tick->chart()->keyframe()->selectedGroup()->id(), m_savedStateDrawing));
        removeIdenticalDrawingPartials(tick->chart()->keyframe());
        tick->chart()->keyframe()->makeInbetweensDirty();
    } else {
        return;
    }
    m_editor->fixedScene()->updateKeyChart(tick->chart()->keyframe());
}

void MovePartialsTool::tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) {
    const Partial *partial;
    if (tick->tickType() == ChartTickItem::TICKORDERPARTIAL) {
        partial = tick->chart()->keyframe()->orderPartials().cpartial(tick->elementIdx());
        m_savedStateOrder = tick->chart()->keyframe()->orderPartials();
        m_editor->undoStack()->push(new RemoveOrderPartial(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), partial->t(), tick->chart()->keyframe()->orderPartials().lastPartialAt(partial->t())));
        removeIdenticalOrderPartials(tick->chart()->keyframe());
    } else if (tick->tickType() == ChartTickItem::TICKDRAWINGPARTIAL && tick->chart()->keyframe()->selectedGroup() != nullptr) {
        partial = tick->chart()->keyframe()->selectedGroup()->drawingPartials().cpartial(tick->elementIdx());
        m_savedStateDrawing = tick->chart()->keyframe()->selectedGroup()->drawingPartials();
        Group *group = tick->chart()->keyframe()->selectedGroup();
        m_editor->undoStack()->push(new RemoveDrawingPartial(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), group->id(), partial->t(), group->drawingPartials().lastPartialAt(partial->t())));
        removeIdenticalDrawingPartials(tick->chart()->keyframe());
    } else {
        return;
    }
    m_editor->fixedScene()->updateKeyChart(tick->chart()->keyframe());
}

void MovePartialsTool::tickPressed(const EventInfo &info) {
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
    m_savedStateOrder = info.key->orderPartials();
}

void MovePartialsTool::tickMoved(const EventInfo &info) {
    if (!m_partialTickPressed || m_partialTrajectoryPressed == nullptr) {
        return;
    }

    Point::VectorType deltaMouse(info.pos.x() - info.lastPos.x(), info.pos.y() - info.lastPos.y());
    Point::Scalar deltaNorm = deltaMouse.norm();

    if (deltaNorm < 1e-6) return;

    const OrderPartial *partial = info.key->orderPartials().cpartial(m_partialTickPressedId);
    if (partial == nullptr) qCritical() << "Error in MovePartialsTool::tickMoved: partial " << m_partialTickPressedId << " doesn't exist!";
    m_partialTrajectoryPressed->localOffset()->frameChanged(partial->t());
    double tPressed = partial->t() + m_partialTrajectoryPressed->localOffset()->get();
    Point::VectorType trajectoryTangent = m_partialTrajectoryPressed->evalVelocity(tPressed);
    float sgn = deltaMouse.dot(trajectoryTangent) > 0.0 ? 1.0f : -1.0f;
    double ds = sgn * deltaNorm / m_partialTrajectoryPressed->approxPathItem().length();
    double newT = std::clamp(partial->t() + ds, 1e-6, 1.0);
    info.key->orderPartials().movePartial(partial->t(), newT);
    m_editor->fixedScene()->updateKeyChart(info.key);
}

void MovePartialsTool::tickReleased(const EventInfo &info) {
    if (!m_partialTickPressed || m_partialTrajectoryPressed == nullptr) {
        return;
    }

    m_editor->undoStack()->push(new SyncOrderPartialCommand(m_editor, m_editor->layers()->currentLayerIndex(), m_editor->playback()->currentFrame(), m_savedStateOrder));
    removeIdenticalOrderPartials(info.key);
    removeIdenticalDrawingPartials(info.key);
    info.key->orderPartials().saveState();
    m_partialTickPressed = false;
    m_partialTrajectoryPressed = nullptr;
    m_editor->fixedScene()->updateKeyChart(info.key);
}

void MovePartialsTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    if (m_editor->currentAlpha() >= 1.0) return;
    QPen p(Qt::NoBrush, 2.0);
    Layer *layer = m_editor->layers()->currentLayer();
    int inb = layer->inbetweenPosition(m_editor->playback()->currentFrame());
    int stride = layer->stride(layer->getVectorKeyFramePosition(key));

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
    painter.setBrush(QColor(255, 204, 0));
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

void MovePartialsTool::restoreAndClearState() const {
    int frame = m_editor->playback()->currentFrame();
    VectorKeyFrame *key = m_editor->layers()->currentLayer()->getLastVectorKeyFrameAtFrame(frame, 0);    
    key->orderPartials().restoreState();
    key->orderPartials().removeSavedState();
}

void MovePartialsTool::removeIdenticalOrderPartials(VectorKeyFrame *keyframe) const {
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

void MovePartialsTool::removeIdenticalDrawingPartials(VectorKeyFrame *keyframe) const {
    for (Group *group : keyframe->postGroups()) {
        bool update = true;
        while (update) {
            update = false;
            QMutableMapIterator<double, DrawingPartial> it(group->drawingPartials().partials());
            it.next();
            DrawingPartial *partial = &(it.value());
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
}