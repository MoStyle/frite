/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef POINT_H
#define POINT_H

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <QColor>

class Point {
   public:
    enum { Dim = 2 };
    typedef qreal Scalar;
    typedef Eigen::Matrix<Scalar, Dim, 1> VectorType;
    typedef Eigen::Matrix<Scalar, Dim, 2> MatrixType;
    typedef Eigen::Translation<Point::Scalar, Point::Dim> Translation;
    typedef Eigen::Rotation2D<Point::Scalar> Rotation;
    typedef Eigen::Transform<Point::Scalar, Point::Dim, Eigen::Affine> Affine;

    Point(const VectorType &_pos = VectorType::Zero(), const VectorType &_normal = VectorType::Zero(),
          Scalar _temporal_w = 1)
        : m_pos(_pos),
          m_normal(_normal),
          m_temporal_w(_temporal_w),
          m_pressure(1.0),
          m_interval(0.0),
          m_color(Qt::black),
          m_groupId(-1),
          m_id(UINT_MAX) {}

    Point(double argx, double argy, double argInterval = 0.0, double argPressure = 1.0,
          const VectorType &_normal = VectorType::Zero(), Scalar _temporal_w = 1.0)
        : m_pos(argx, argy),
          m_normal(_normal),
          m_temporal_w(_temporal_w),
          m_pressure(argPressure),
          m_interval(argInterval),
          m_color(Qt::black),
          m_groupId(-1),
          m_id(UINT_MAX) {}

    Point(const Point &other)
        : m_pos(other.m_pos),
          m_normal(other.m_normal),
          m_temporal_w(other.m_temporal_w),
          m_pressure(other.m_pressure),
          m_interval(other.m_interval),
          m_color(other.m_color),
          m_groupId(other.m_groupId),
          m_id(other.m_id) {
    }

    VectorType &pos() { return m_pos; }
    VectorType &normal() { return m_normal; }

    const VectorType &pos() const { return m_pos; }
    const VectorType &normal() const { return m_normal; }
    Scalar temporalW() const { return m_temporal_w; }
    void setTemporalW(const Scalar &temporal_w) { m_temporal_w = temporal_w; }

    Scalar x() const { return m_pos.x(); }
    Scalar y() const { return m_pos.y(); }
    void setPos(const VectorType &pos) { m_pos = pos; }

    Scalar operator[](unsigned int i) { return m_pos[i]; }
    Scalar operator[](unsigned int i) const { return m_pos[i]; }

    Scalar interval() const { return m_interval; }
    void setInterval(Scalar interval) { m_interval = interval; }

    Scalar pressure() const { return m_pressure; }
    void setPressure(Scalar pressure) { m_pressure = pressure; }

    QColor getColor() const { return m_color; }
    void setColor(QColor color) { m_color = color; }

    int groupId() const { return m_groupId; }
    void setGroupId(int id) { m_groupId = id; }

    unsigned int id() const { return m_id; }
    void initId(unsigned int s, unsigned int p) { m_id = 0.5 * (s + p) * (s + p + 1) + p; }

   private:
    VectorType m_pos, m_normal;
    Scalar m_temporal_w;
    Scalar m_pressure;
    Scalar m_interval;
    QColor m_color;  // temp, for smart scribbles
    int m_groupId;
    unsigned int m_id;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

#endif  // POINT_H
