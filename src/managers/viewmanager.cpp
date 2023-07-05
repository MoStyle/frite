/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "viewmanager.h"

static qreal gZoomingList[] =
{
    .01, .02, .04, .06, .08, .12, .16, .25, .33, .5, .75,
    1., 1.5, 2., 3., 4., 5., 6., 8., 16., 32., 48., 64., 96.
};

ViewManager::ViewManager(QObject *parent) : BaseManager(parent)
{
    resetView();
}

QPointF ViewManager::mapCanvasToScreen(QPointF p)
{
    return m_view.map(p);
}

QPointF ViewManager::mapScreenToCanvas(QPointF p)
{
    return m_view.inverted().map(p);
}

QPainterPath ViewManager::mapCanvasToScreen(const QPainterPath& path)
{
    return m_view.map(path);
}

QRectF ViewManager::mapCanvasToScreen(const QRectF& rect)
{
    return m_view.mapRect(rect) ;
}

QRectF ViewManager::mapScreenToCanvas(const QRectF& rect)
{
    return m_view.inverted().mapRect(rect) ;
}

QPainterPath ViewManager::mapScreenToCanvas(const QPainterPath& path)
{
    return m_view.inverted().map(path);
}

QTransform ViewManager::getView()
{
    return m_view;
}

const QTransform &ViewManager::getView() const
{
    return m_view;
}

void ViewManager::createViewTransform()
{
    QTransform c;
    c.translate(m_canvasSize.width() / 2, m_canvasSize.height() / 2);
    
    QTransform t;
    t.translate(m_translate.x(), m_translate.y());

    QTransform r;
    r.rotate(m_rotate);
    
    qreal flipX = m_isFlipHorizontal ? -1.0 : 1.0;
    qreal flipY = m_isFlipVertical ? -1.0 : 1.0;

    QTransform s;
    s.scale(m_scale * flipX, m_scale * flipY);

    m_view = t * s * r * c;
}

void ViewManager::translate(qreal dx, qreal dy)
{
    m_translate = QPointF(dx, dy);
    createViewTransform();
    emit viewChanged();
}

void ViewManager::translate(QPointF offset)
{
    translate(offset.x(), offset.y());
}

void ViewManager::rotate(qreal degree)
{
    m_rotate += degree;
    createViewTransform();
    emit viewChanged();
}

void ViewManager::scaleUp()
{
    int listLength = sizeof(gZoomingList)/sizeof(qreal);
    for(int i = 0; i < listLength; i++)
    {
        if (m_scale < gZoomingList[i])
        {
            scale(gZoomingList[i]);
            return;
        }
    }

    // scale is not in the list.
    scale(m_scale * 2.0);
}

void ViewManager::scaleDown()
{
    int listLength = sizeof(gZoomingList)/sizeof(qreal);
    for(int i = listLength-1; i > 0; i--)
    {
        if (m_scale > gZoomingList[i])
        {
            scale(gZoomingList[i]);
            return;
        }
    }

    // scale is not in the list.
    scale(m_scale * 0.8333);
}

void ViewManager::scale(qreal scaleValue)
{
    if(scaleValue < m_minScale)
    {
        scaleValue = m_minScale;
    }
    else if(scaleValue > m_maxScale)
    {
        scaleValue = m_maxScale;
    }
    else if((m_maxScale-scaleValue) < 1e-4 || (scaleValue-m_minScale) < 1e-4) {
        return;
    }
    m_scale = scaleValue;
    createViewTransform();
    emit viewChanged();
}

void ViewManager::flipHorizontal()
{
    m_isFlipHorizontal = !m_isFlipHorizontal;
    createViewTransform();
    emit viewChanged();
}

void ViewManager::flipVertical()
{
    m_isFlipVertical = !m_isFlipVertical;
    createViewTransform();
    emit viewChanged();
}

void ViewManager::setCanvasSize(QSize size)
{
    m_canvasSize = size;
    createViewTransform();
    emit viewChanged();
}

void ViewManager::resetScale() {
    m_scale = 1.0;
    createViewTransform();
    emit viewChanged();
}


void ViewManager::resetRotate() {
    m_rotate = 0.0;
    createViewTransform();
    emit viewChanged();
}

void ViewManager::resetView()
{
    m_rotate = 0.0;
    m_scale = 1.0;
    translate(0, 0);
    m_isFlipHorizontal = false;
    m_isFlipVertical = false;
    createViewTransform();
    emit viewChanged();
}
