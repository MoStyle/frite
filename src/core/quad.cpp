/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "quad.h"

#include "corner.h"

void Quad::clear() {
    m_elements.clear();
    for (int i = 0; i < 4; ++i) {
        m_centroid[i] = Point::VectorType::Zero();
        corners[i] = nullptr;
    }
}

/**
 * Remove a stroke embedding
 */
void Quad::removeStroke(int strokeId) {
    QMutableHashIterator<unsigned int, Intervals> it(m_elements);
    while (it.hasNext()) {
        it.next();
        if (it.key() == strokeId) it.remove();
    }
}

void Quad::pin(const Point::VectorType &uv) {
    pin(uv, getPoint(uv, TARGET_POS));
}

void Quad::pin(const Point::VectorType &uv, const Point::VectorType &pos) {
    m_pinned = true;
    m_pinUV = uv;
    m_pinPosition = pos;
}

void Quad::unpin() {
    m_pinned = false;
}

Point::VectorType Quad::biasedCentroid(PosTypeIndex type) const {
    if (!m_pinned) {
        return centroid(type);
    }

    Point::VectorType bCentroid = Point::VectorType::Zero();
    double weight = 10000.0;
    for (int i = 0; i < 4; ++i) {
        bCentroid += corners[i]->coord(type);
    }
    if (type != TARGET_POS) {
        bCentroid += getPoint(m_pinUV, type) * weight;
    } else {
        bCentroid += m_pinPosition * weight;
    }
    bCentroid /= (4 + weight);

    return bCentroid;
}

void Quad::computeCentroid(PosTypeIndex type) {
    m_centroid[type] = Point::VectorType::Zero();

    for (int i = 0; i < 4; ++i) {
        m_centroid[type] += corners[i]->coord(type);
    }

    m_centroid[type] *= 0.25;
}

void Quad::computeCentroids() {
    for (int i = 0; i < 4; ++i) {
        m_centroid[i] = Point::VectorType::Zero();
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m_centroid[i] += corners[j]->coord((PosTypeIndex)i);
        }
    }

    for (int i = 0; i < 4; ++i) {
        m_centroid[i] *= 0.25;
    }
}

Point::VectorType Quad::getPoint(const Point::VectorType &uv, PosTypeIndex type) const {
    return (corners[TOP_LEFT]->coord(type) * (1.0 - uv.x()) + corners[TOP_RIGHT]->coord(type) * uv.x()) * (1.0 - uv.y()) // top edge
         + (corners[BOTTOM_LEFT]->coord(type) * (1.0 - uv.x()) + corners[BOTTOM_RIGHT]->coord(type) * uv.x()) * uv.y();  // bottom edge
}

/**
 * Returns the optimal rigid transformation (translation + rotation) between the source and target positions
*/
Point::Affine Quad::optimalRigidTransform(PosTypeIndex source, PosTypeIndex target) {
    computeCentroids();
    double a = 0;
    double b = 0;
    for (int i = 0; i < 4; i++) {
        Corner *c = corners[i];
        Point::VectorType p_minus_pc = c->coord(source) - biasedCentroid(source);    // source position
        Point::VectorType q_minus_qc = c->coord(target) - biasedCentroid(target);    // target position
        a += q_minus_qc.dot(p_minus_pc);
        b += q_minus_qc.dot(Point::VectorType(-p_minus_pc.y(), p_minus_pc.x()));
    }
    double mu = sqrt(a * a + b * b);
    if (mu < 1e-3) mu = 1e-3;
    double r1 = a / mu;
    double r2 = -b / mu;
    Eigen::Matrix2d R;
    R << r1, r2, -r2, r1;
    Point::VectorType t = biasedCentroid(target) - R * biasedCentroid(source);
    return Point::Affine(Point::Translation(t) * Point::Rotation(R));
}

/**
 * Returns the optimal affine transformation (translation + rotation) between the source and target positions
*/
Point::Affine Quad::optimalAffineTransform(PosTypeIndex source, PosTypeIndex target) {
    computeCentroids();
    Eigen::Matrix2d PiPi, QiPi;
    PiPi.setZero();
    QiPi.setZero();
    for (int i = 0; i < 4; i++) {
        Corner *c = corners[i];
        Point::VectorType pi = c->coord(source) - biasedCentroid(source);
        Point::VectorType qi = c->coord(target) - biasedCentroid(target);
        PiPi += (pi * pi.transpose());
        QiPi += (qi * pi.transpose());
    }
    Eigen::Matrix2d R = QiPi * PiPi.inverse();
    Point::VectorType t = biasedCentroid(target) - R * biasedCentroid(source);
    return Point::Affine(Point::Translation(t) * Point::Rotation(R));   
}