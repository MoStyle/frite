/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef QTEIGEN_H
#define QTEIGEN_H

#include "point.h"

// QE: QT to Eigen
// EQ: Eigen to QT

#define QE_POINT(p) Point::VectorType(p.x(), p.y()) 
#define EQ_POINT(p) QPointF(p.x(), p.y())

#define QE_RECT(r) Eigen::AlignedBox<Point::Scalar, 2>(QE_POINT(r.bottomLeft()), QE_POINT(r.topRight()))
#define EQ_RECT(r) QRectF(EQ_POINT(r.corner(Eigen::AlignedBox::TopLeft)), EQ_POINT(r.corner(Eigen::AlignedBox::BottomRight)))

#endif