/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

#include <QTransform>
#include "basemanager.h"


class ViewManager : public BaseManager
{
    Q_OBJECT

public:
    explicit ViewManager(QObject* parent = 0);

    QTransform getView();
    const QTransform &getView() const;
    void resetView();

    QPointF mapCanvasToScreen(QPointF p);
    QPointF mapScreenToCanvas(QPointF p);

    QRectF mapCanvasToScreen(const QRectF& rect);
    QRectF mapScreenToCanvas(const QRectF& rect);

    QPainterPath mapCanvasToScreen(const QPainterPath& path);
    QPainterPath mapScreenToCanvas(const QPainterPath& path);

    QPointF translation() { return m_translate; }
    void translate(qreal dx, qreal dy);
    void translate(QPointF offset);

    qreal rotation() { return m_rotate; }
    void  rotate(qreal degree);

    qreal scaling() { return m_scale; }
    void scale(qreal scaleValue);

    QSize canvasSize() { return m_canvasSize; }
    void setCanvasSize(QSize size);

    bool isFlipHorizontal() { return m_isFlipHorizontal; }
    bool isFlipVertical() { return m_isFlipVertical; }

public slots:
    void scaleUp();
    void scaleDown();
    void resetScale();

    void flipHorizontal();
    void flipVertical();

    void rotateClockwise() { rotate(15); }
    void rotateCounterClockwise() { rotate(-15); }
    void resetRotate();

    void setDevicePixelRatio(qreal ratio);

signals:
    void viewChanged();

private:
    const qreal m_minScale = 0.01;
    const qreal m_maxScale = 100;

    void createViewTransform();

    QTransform m_view;

    qreal m_devicePixelRatio;

    QPointF m_translate;
    qreal m_rotate = 0.f;
    qreal m_scale = 1.f;

    QSize m_canvasSize = { 1, 1 };

    bool m_isFlipHorizontal = false;
    bool m_isFlipVertical   = false;
};

#endif // VIEWMANAGER_H
