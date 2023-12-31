/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "canvasview.h"
#include "tabletcanvas.h"
#include "editor.h"
#include "layermanager.h"
#include "vectorkeyframe.h"

CanvasView::CanvasView(QGraphicsScene *scene, Editor *editor, QWidget *parent, bool mouseEventsTransparent) : QGraphicsView(scene, parent), m_editor(editor), lastFrame(-1) {
    init(mouseEventsTransparent);
}

CanvasView::CanvasView(Editor *editor, QWidget *parent) : CanvasView(nullptr, editor, parent) {
    
}

CanvasView::~CanvasView() {

}

void CanvasView::init(bool mouseEventTransparent) {
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (mouseEventTransparent) setAttribute(Qt::WA_TransparentForMouseEvents);
    viewport()->setAutoFillBackground(false);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    verticalScrollBar()->blockSignals(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    horizontalScrollBar()->blockSignals(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void CanvasView::mousePressEvent(QMouseEvent *event) {
    QGraphicsView::mousePressEvent(event);
    if (!event->isAccepted()) {
        event->ignore();
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent *event) {
    QGraphicsView::mouseMoveEvent(event);
    // qDebug() << "move event accepted (view) ? " << event->isAccepted();
    event->ignore();
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event) {
    QGraphicsView::mouseReleaseEvent(event);
    event->ignore();
}

void CanvasView::wheelEvent(QWheelEvent *event) {
    QGraphicsView::wheelEvent(event);
    if (!event->isAccepted()) {
        event->ignore();
    }
}

void CanvasView::tabletEvent(QTabletEvent *event) {
    QGraphicsView::tabletEvent(event);
    if (!event->isAccepted()) {
        event->ignore();
    }
}