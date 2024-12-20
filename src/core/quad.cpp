#include "quad.h"

#include "corner.h"

void Quad::clear() {
    m_forwardStrokes.clear();
    m_backwardStrokes.clear();
    for (int i = 0; i < 4; ++i) {
        m_centroid[i] = Point::VectorType::Zero();
        corners[i] = nullptr;
    }
}

/**
 * Remove a stroke embedding
 */
void Quad::removeStroke(int strokeId) {
    m_forwardStrokes.remove(strokeId);
    m_backwardStrokes.remove(strokeId);
}

double Quad::averageEdgeLength(PosTypeIndex pos) const {
    double tot = 0.0;
    for (int i = 0; i < 4; ++i) {
        tot += (corners[(i + 1) % 4]->coord(pos) - corners[i]->coord(pos)).norm();
    }
    return tot * 0.25;
}

void Quad::pin(const Point::VectorType &uv) {
    pin(uv, getPoint(uv, TARGET_POS));
}

void Quad::pin(const Point::VectorType &uv, const Point::VectorType &pos) {
    m_flags.set(PINNED, true);
    m_pinUV = uv;
    m_pinPosition = pos;
}

void Quad::unpin() {
    m_flags.set(PINNED, false);
}

Point::VectorType Quad::biasedCentroid(PosTypeIndex type) const {
    if (!m_flags.test(PINNED)) {
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
 * Returns the optimal affine transformation between the source and target positions
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

/**
 * Returns the optimal affine transformation between the original quad location (axis-aligned) and the target quad position
 */
Point::Affine Quad::optimalAffineTransformFromOriginalQuad(int x, int y, int cellSize, Eigen::Vector2i origin, PosTypeIndex target) {
    Point::VectorType positions[4] = {Point::VectorType(x, y), Point::VectorType(x + 1, y), Point::VectorType(x + 1, y + 1), Point::VectorType(x, y + 1)};
    Point::VectorType originalPositions[4];
    Point::VectorType centroid = Point::VectorType::Zero();
    for (int i = 0; i < 4; ++i) {
        originalPositions[i] = cellSize * positions[i] + Point::VectorType(origin.x(), origin.y());
        centroid += originalPositions[i];
    }

    computeCentroid(target);
    Eigen::Matrix2d PiPi, QiPi;
    PiPi.setZero();
    QiPi.setZero();
    for (int i = 0; i < 4; i++) {
        Corner *c = corners[i];
        Point::VectorType pi = originalPositions[i] - centroid;
        Point::VectorType qi = c->coord(target) - biasedCentroid(target);
        PiPi += (pi * pi.transpose());
        QiPi += (qi * pi.transpose());
    }
    Eigen::Matrix2d R = QiPi * PiPi.inverse();
    Point::VectorType t = biasedCentroid(target) - R * centroid;
    return Point::Affine(Point::Translation(t) * Point::Rotation(R));   
}