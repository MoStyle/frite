#ifndef QTEIGEN_H
#define QTEIGEN_H

#include "point.h"

// #include <Eigen/Geometry>
// #include <QPoint>

// QE: QT to Eigen
// EQ: Eigen to QT

#define QE_POINT(p) Point::VectorType(p.x(), p.y()) 
#define EQ_POINT(p) QPointF(p.x(), p.y())

#define QE_RECT(r) Eigen::AlignedBox<Point::Scalar, 2>(QE_POINT(r.bottomLeft()), QE_POINT(r.topRight()))
#define EQ_RECT(r) QRectF(EQ_POINT(r.corner(Eigen::AlignedBox::TopLeft)), EQ_POINT(r.corner(Eigen::AlignedBox::BottomRight)))

#endif