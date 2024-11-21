 #include "trajectorytickitem.h"

#include "trajectory.h"
#include "group.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QVector2D>
#include <QPointF>

TrajectoryTickItem::TrajectoryTickItem(Trajectory *traj, qreal linearAlpha, int idx) : QGraphicsRectItem(), m_traj(traj), m_linearAlpha(linearAlpha), m_idx(idx) {
    updatePos();
}

void TrajectoryTickItem::updatePos() {
    qreal alpha = m_traj->group()->spacingAlpha(m_linearAlpha);
    m_pos = m_traj->eval(alpha);
    setRect(m_pos.x() - 2, m_pos.y() - 2, 4, 4);
}

void TrajectoryTickItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    qDebug() << "TRAJ TICK PRESSED";
    event->accept();
}

void TrajectoryTickItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QVector2D delta(event->pos() - event->lastPos());
    float deltaX = delta.length() / m_traj->approxPathItem().length();
    qDebug() << "tot length (path) = " << m_traj->approxPathItem().length();
    qDebug() << "tot length (cubic) = " << m_traj->cubicApprox().length();
    qDebug() << "deltaX = " << deltaX;
    event->accept();
}

void TrajectoryTickItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    event->accept();
}

void TrajectoryTickItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    event->accept();
}

void TrajectoryTickItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->accept();
}

void TrajectoryTickItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept();
}

void TrajectoryTickItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setPen(QPen(QBrush(Qt::black), 2.0));
    painter->drawRect(rect());
}