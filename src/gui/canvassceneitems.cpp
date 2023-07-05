/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "canvassceneitems.h"
#include "group.h"
#include "vectorkeyframe.h"
#include "grouplist.h"
#include "tools/picktool.h"

GroupSelectionOutline::GroupSelectionOutline(const QHash<int, Group *> &groups) 
    : m_groups(groups) { 
    // compute groups bounding rect
    for (Group *group : m_groups) {
        m_boundingRect = m_boundingRect.united(group->cbounds());
    }
}

QRectF GroupSelectionOutline::boundingRect() const {
    return m_boundingRect;
}

void GroupSelectionOutline::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QPen outlinePen(QBrush(Qt::NoBrush), 2, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    outlinePen.setCosmetic(true);
    outlinePen.setColor(QColor(200, 20, 20, 200));

    painter->save();
    painter->setPen(outlinePen);
    painter->setOpacity(0.65f);

    QRectF bounds = m_boundingRect + QMargins(4, 4, 4, 4); 
    QPointF tl = bounds.topLeft(); QPointF tr = bounds.topRight();
    QPointF bl = bounds.bottomLeft(); QPointF br = bounds.bottomRight();
    QVector2D w((tr - tl));
    QVector2D h((bl - tl));
    float length = std::min(bounds.width(), bounds.height()) * 0.07f;
    w = w.normalized() * length; 
    h = h.normalized() * length;

    painter->drawLine(tl, tl + w.toPointF());
    painter->drawLine(tl, tl + h.toPointF());
    painter->drawLine(tr, tr - w.toPointF());
    painter->drawLine(tr, tr + h.toPointF());
    painter->drawLine(br, br - w.toPointF());
    painter->drawLine(br, br - h.toPointF());
    painter->drawLine(bl, bl + w.toPointF());
    painter->drawLine(bl, bl - h.toPointF());

    painter->restore();;
}


QRectF LassoDrawer::boundingRect() const { 
    return m_tool->selectionPolygon().boundingRect(); 
}   

void LassoDrawer::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    QPen lassoPen(QBrush(QColor(Qt::black), Qt::NoBrush), 1, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    lassoPen.setColor(QColor(Qt::black));
    lassoPen.setCosmetic(true);
    painter->save();
    painter->setPen(lassoPen);
    painter->drawPolygon(m_tool->selectionPolygon());
    painter->restore();
}