/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "logarithmicspiral.h"

#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>

Point::VectorType LogarithmicSpiral::eval(double t) const {
     Point::VectorType d = m_start - m_origin;
     Eigen::Matrix2d r, s;
     r << cos(m_rot * t), -sin(m_rot * t),
         sin(m_rot * t),  cos(m_rot * t);
     s << m_scale, 0.0,
         0.0, m_scale;
     return m_origin + r * s.pow(t) * d;
}

void LogarithmicSpiral::make(const Point::VectorType &start, const Point::VectorType &end, double rot, double scale) {
     m_rot = rot;
     m_scale = scale;
     m_start = start;
     m_end = end;
     Eigen::Matrix2d r, s, rs;
     r << cos(m_rot), -sin(m_rot),
          sin(m_rot),  cos(m_rot);
     s << m_scale, 0.0,
         0.0, m_scale;
     rs = r * s;
     m_origin = start - rs.inverse() * end;
}