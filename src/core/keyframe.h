/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KEYFRAME_H
#define KEYFRAME_H

#include "lattice.h"
#include "keyframedparams.h"

#include <cpd/rigid.hpp>

class KeyFrame {
   public:
    KeyFrame() : m_topSelected(false), m_bottomSelected(false) {}

    virtual ~KeyFrame() {}

    virtual bool load(QDomElement &element, const QString &path, Editor *editor) = 0;
    virtual bool save(QDomDocument &doc, QDomElement &root, const QString &path, int layer, int frame) const = 0;

    virtual void transform(QRectF newBoundaries, bool smoothTransform) = 0;

    bool isTopSelected() const { return m_topSelected; }
    void setTopSelected(bool b) { m_topSelected = b; }

    bool isBottomSelected() const { return m_bottomSelected; }
    void setBottomSelected(bool b) { m_bottomSelected = b; }

    // Bounds
    double top() { return m_bounds.top(); }
    void setTop(double x) { m_bounds.setTop(x); }
    double left() { return m_bounds.left(); }
    void setLeft(double y) { m_bounds.setLeft(y); }
    QPointF topLeft() { return m_bounds.topLeft(); }
    QRectF &bounds() { return m_bounds; }
    const QRectF &bounds() const { return m_bounds; }
    double width() { return m_bounds.width(); }
    double height() { return m_bounds.height(); }
    void moveTopLeft(QPoint point) { m_bounds.moveTopLeft(point); }
    void moveTopLeft(QPointF point) { moveTopLeft(point.toPoint()); }

   protected:
    // top/bottom square on the keyframe cell in the timeline gui
    bool m_topSelected;
    bool m_bottomSelected;

    QRectF m_bounds;
};

#endif  // KEYFRAME_H
