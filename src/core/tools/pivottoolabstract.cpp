/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "pivottoolabstract.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"

static dkBool k_curveContinuity("Pivot->Continuity", true);

static dkBool k_useOnionSkin("Pivot->Use onion skin", false);


PivotToolAbstract::PivotToolAbstract(QObject *parent, Editor *editor) : 
    Tool(parent, editor) 
    { m_contextMenuAllowed = false; }


PivotToolAbstract::~PivotToolAbstract() {

}

QCursor PivotToolAbstract::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void PivotToolAbstract::drawPivot(QPainter &painter, int frame, float saturation){
    int stride = m_editor->layers()->currentLayer()->stride(frame);
    int inbetween = m_editor->layers()->currentLayer()->inbetweenPosition(frame);
    float t = stride > 1 ?  float(inbetween) / (stride - 1) : 0;
    t = inbetween >= stride ? 1. : t;

    float angle = m_editor->layers()->currentLayer()->getLastVectorKeyFrameAtFrame(frame, 0)->getFrameRotation(t);
    Layer * layer = m_editor->layers()->currentLayer();
    Point::VectorType position = layer->getPivotPosition(frame);
    drawPivot(painter, position, angle, saturation);
}

void PivotToolAbstract::drawPivot(QPainter &painter, Point::VectorType position, float angle, float saturation){
    static QPen penRed(QColor(125, 0, 0, 255), 2.0);
    penRed.setColor(QColor(255 * saturation, 0, 0, 255 * saturation));
    static QPen penGreen(QColor(0, 125, 0, 255), 2.0);
    penGreen.setColor(QColor(0, 255 * saturation, 0, 255 * saturation / 4));
    painter.setPen(penRed);
    const int lineLength = 20;
    const int arrowOffset = 4;
    const Point::Rotation rot(angle);
    const Point::Translation transl(position);


    Point::VectorType firstPoint = transl * rot * transl.inverse() * position;
    Point::VectorType lastPoint = transl * rot * transl.inverse() * (position + Point::VectorType(lineLength - 2, 0));
    Point::VectorType lastPointArrow = transl * rot * transl.inverse() * (position + Point::VectorType(lineLength, 0));
    Point::VectorType arrowPoint1 = transl * rot * transl.inverse() * (position + Point::VectorType(lineLength - arrowOffset, -arrowOffset));
    Point::VectorType arrowPoint2 = transl * rot * transl.inverse() * (position + Point::VectorType(lineLength - arrowOffset, arrowOffset));

    painter.drawLine(firstPoint.x(), firstPoint.y(), lastPoint.x(), lastPoint.y());
    painter.drawLine(arrowPoint1.x(), arrowPoint1.y(), lastPointArrow.x(), lastPointArrow.y());
    painter.drawLine(arrowPoint2.x(), arrowPoint2.y(), lastPointArrow.x(), lastPointArrow.y());

    painter.setPen(penGreen);
    lastPoint = transl * rot * transl.inverse() * (position + Point::VectorType(0, - lineLength + 2));
    lastPointArrow = transl * rot * transl.inverse() * (position + Point::VectorType(0, - lineLength));
    arrowPoint1 = transl * rot * transl.inverse() * (position + Point::VectorType(- arrowOffset, - lineLength + arrowOffset));
    arrowPoint2 = transl * rot * transl.inverse() * (position + Point::VectorType(arrowOffset, - lineLength + arrowOffset));
    painter.drawLine(firstPoint.x(), firstPoint.y(), lastPoint.x(), lastPoint.y());
    painter.drawLine(arrowPoint1.x(), arrowPoint1.y(), lastPointArrow.x(), lastPointArrow.y());
    painter.drawLine(arrowPoint2.x(), arrowPoint2.y(), lastPointArrow.x(), lastPointArrow.y());
}

void PivotToolAbstract::drawTrajectory(QPainter &painter, QVector<Bezier2D *> &beziers){
    static QPen penCurve(QColor(200, 200, 200), 2.0);
    static QPen penPoint(QColor(125, 125, 125, 125), 8);
    penPoint.setCapStyle(Qt::RoundCap);
    penCurve.setStyle(Qt::DashLine);


    // Debug
    // static QPen penDebug(QColor(125, 0, 125), 8);
    // penDebug.setCapStyle(Qt::RoundCap);
    // painter.setPen(penDebug);
    // painter.drawPoint(0, 0);

    QColor color(200, 20, 20);
    for (Bezier2D * bezier : beziers){
        int h, s, v;
        color.getHsv(&h, &s, &v);
        color.setHsv((h + 10) % 360, s, v);
        penCurve.setColor(color);
        painter.setPen(penCurve);
        QPainterPath path;
        path.moveTo(bezier->getP0().x(), bezier->getP0().y());
        path.cubicTo(bezier->getP1().x(), bezier->getP1().y(), bezier->getP2().x(), bezier->getP2().y(), bezier->getP3().x(), bezier->getP3().y());

        painter.drawPath(path);
        painter.setPen(penPoint);
        painter.drawPoint(bezier->getP0().x(), bezier->getP0().y());
        painter.setPen(penCurve);
    }
    if (!beziers.empty()){
        painter.setPen(penPoint);
        painter.drawPoint(beziers.back()->getP3().x(), beziers.back()->getP3().y());
    }
}

void PivotToolAbstract::drawTrajectory(QPainter &painter){
    CompositeBezier2D * compositeBeziers = m_editor->layers()->currentLayer()->getPivotCurves();
    QVector<Bezier2D *> beziers;
    for (Bezier2D * bezier : compositeBeziers->getBeziers()){
        beziers.append(bezier);
    }
    drawTrajectory(painter, beziers);
}

void PivotToolAbstract::drawTrajectory(QPainter &painter, QVector<VectorKeyFrame *> keys){
    Layer * layer = m_editor->layers()->currentLayer();
    CompositeBezier2D * compositeBeziers = layer->getPivotCurves();

    QVector<Bezier2D *> beziers;
    for (VectorKeyFrame * key : keys){
        int frame = layer->getVectorKeyFramePosition(key);
        beziers.append(compositeBeziers->getBezier(layer->getFrameTValue(frame)));
    }
    drawTrajectory(painter, beziers);
}