/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "cubic.h"
#include "utils/utils.h"
#include "utils/geom.h"
#include "dialsandknobs.h"
#include <iostream>

Bezier2D::Bezier2D() : p0(Point::VectorType::Zero()), p1(Point::VectorType(0.5, 0.5)), p2(Point::VectorType(0.5, 0.5)), p3(Point::VectorType(1.0, 1.0)) {
    updateArclengthLUT();
}

Bezier2D::Bezier2D(Point::VectorType _p0, Point::VectorType _p1, Point::VectorType _p2, Point::VectorType _p3) 
    : p0(_p0), p1(_p1), p2(_p2), p3(_p3) {
    updateArclengthLUT();
}

Bezier2D::Bezier2D(const Bezier2D &other) 
    : p0(other.p0), p1(other.p1), p2(other.p2), p3(other.p3) {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < LUT_PRECISION; ++j) {
            alengthLUT[i][j] = other.alengthLUT[i][j];
        }
    }
}

Point::VectorType Bezier2D::eval(Point::Scalar t) const {
    Point::Scalar tx = 1.0 - t;
    return p0 * (tx * tx * tx) +
           p1 * (3.0 * tx * tx * t) +
           p2 * (3.0 * tx * t * t) +
           p3 * (t * t * t);
}

Point::VectorType Bezier2D::evalDer(Point::Scalar t) const {
    Point::Scalar tx = 1.0 - t;
    return (p1 - p0) * (3.0 * tx * tx) +
           (p2 - p1) * (6.0 * tx * t) + 
           (p3 - p2) * (3.0 * t * t);
}

Point::VectorType Bezier2D::evalArcLength(Point::Scalar s) const {
    return eval(param(s));
}

Point::Scalar Bezier2D::evalYFromX(Point::Scalar x) {
    Point::Scalar t = tFromX(x);
    Eigen::Vector4f coeffs = Geom::bezierCoeffs(p0.y(), p1.y(), p2.y(), p3.y());
    return (t * t * t * coeffs[0]) + (t * t * coeffs[1]) + (t * coeffs[2]) + coeffs[3];
}

// get s from t
Point::Scalar Bezier2D::arcLength(Point::Scalar t) const {
    return 0.0;   
}

Point::Scalar Bezier2D::speed(Point::Scalar t) const {
    return evalDer(t).norm();
}

void Bezier2D::fit(const std::vector<Point::VectorType> &data, bool constrained) {
    std::vector<Point::Scalar> u;
    chordLengthParameterize(data, u);

    constrained ? fitBezierConstrained(data, u) : fitBezier(data, u);
    Point::Scalar err = maxError(data, u);

    for (int i = 0; i < 4; ++i) {
        reparameterize(data, u);
        constrained ? fitBezierConstrained(data, u) : fitBezier(data, u);
        maxError(data, u);
    }
}

void Bezier2D::fitWithParam(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    fitBezierConstrained(data, u);
    maxError(data, u);
}

// transform the current control points so that the endpoints best align with the given start and end points
void Bezier2D::fitExtremities(Point::VectorType start, Point::VectorType end) {
    // original basis
    Point::VectorType t1 = (p3 - p0).normalized();
    Point::VectorType t2(-t1.y(), t1.x());
    
    // diff coordinates
    Point::VectorType p1Diff, p2Diff;
    p1Diff = Point::VectorType((p1 - p0).dot(t1), (p1 - p0).dot(t2));
    p2Diff = Point::VectorType((p2 - p0).dot(t1), (p2 - p0).dot(t2));

    // new basis
    t1 = (end - start).normalized();
    t2 = Point::VectorType(-t1.y(), t1.x());

    // scaling factor
    Point::Scalar l1 = (p3 - p0).norm();
    Point::Scalar l2 = (end - start).norm();
    Point::Scalar l;
    if (l2 == 0) l = 1.0;
    else l = l2 / l1;
    p1Diff *= l;
    p2Diff *= l;

    // new control points
    p0 = start;
    p1 = p0 + t1 * p1Diff.x() + t2 * p1Diff.y();
    p2 = p0 + t1 * p2Diff.x() + t2 * p2Diff.y();
    p3 = end;
}

void Bezier2D::split(Point::Scalar t, Bezier2D &left, Bezier2D &right) const {
    // de casteljau
    Point::VectorType p01 = (p1 - p0) * t + p0;
    Point::VectorType p12 = (p2 - p1) * t + p1;
    Point::VectorType p23 = (p3 - p2) * t + p2;
    Point::VectorType p012 = (p12 - p01) * t + p01;
    Point::VectorType p123 = (p23 - p12) * t + p12;
    Point::VectorType p0123 = (p123 - p012) * t + p012;

    left.setP0(p0);
    left.setP1(p01);
    left.setP2(p012);
    left.setP3(p0123);
    right.setP0(p0123);
    right.setP1(p123);
    right.setP2(p23);
    right.setP3(p3);
}

void Bezier2D::updateArclengthLUT() {
    double step = 1.0 / (LUT_PRECISION - 1);
    double t = 0.0;
    double s = 0.0;
    Point::VectorType cur, prev;

    alengthLUT[0][0] = 0.0;
    alengthLUT[1][0] = 0.0;
    alengthLUT[0][LUT_PRECISION - 1] = 1.0;
    alengthLUT[1][LUT_PRECISION - 1] = 1.0;

    prev = p0;
    
    for (int i = 1; i < LUT_PRECISION - 1; i++) {
        t += step;
        cur = eval(t);
        s += (cur - prev).norm();
        prev = cur;
        alengthLUT[0][i] = s;
        alengthLUT[1][i] = t;
    }
    cur = eval(1.0);
    s += (cur - prev).norm();
    len = s;

    // normalize s
    for (int i = 1; i < LUT_PRECISION - 1; i++) {
        alengthLUT[0][i] /= s;
    }
}

Point::Scalar Bezier2D::tFromX(Point::Scalar x) {
    // TODO test x boundaries
    Eigen::Vector4f coeffs = Geom::bezierCoeffs(p0.x(), p1.x(), p2.x(), p3.x());

    if (std::abs(coeffs[0]) < 1e-8) {
        if (std::abs(coeffs[1]) < 1e-8) { // linear
            return (x - coeffs[3]) / coeffs[2];
        }
        return Utils::quadraticRoot(coeffs[1], coeffs[2], coeffs[3] - x);
    }

    return Utils::cubicRoot(coeffs[1] / coeffs[0], coeffs[2] / coeffs[0], (coeffs[3] - x) / coeffs[0]);
}

void Bezier2D::fitBezier(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    Eigen::Matrix4d M;  // bezier coeffs
    Eigen::MatrixXd T;  // param
    Eigen::MatrixXd D;  // data points
    Eigen::MatrixXd P;  // control points (unknowns)

    M << -1.0, 3.0, -3.0, 1.0,
          3.0, -6.0, 3.0, 0.0,
         -3.0, 3.0, 0.0, 0.0,
          1.0, 0.0, 0.0, 0.0;

    T = Eigen::MatrixXd(data.size(), 4);
    D = Eigen::MatrixXd(data.size(), 2);
    for (int i = 0; i < data.size(); ++i) {
        D.row(i) = data[i];
        T.coeffRef(i, 3) = 1.0;
        for (int j = 2; j >= 0; --j) {
            T.coeffRef(i, j) = u[i] * T.coeffRef(i, j + 1);
        }
    }

    P = Eigen::MatrixXd(4, 2);
    P = (T * M).colPivHouseholderQr().solve(D);

    p0 = P.row(0);
    p1 = P.row(1);
    p2 = P.row(2);
    p3 = P.row(3);
}

/**
 * TODO refactor constraint row/col
 * Cubic fit with fixed boudary constraints
 */
void Bezier2D::fitBezierConstrained(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    Eigen::Matrix4d M;  // bezier coeffs
    Eigen::MatrixXd T;  // param
    Eigen::MatrixXd D;  // data points
    Eigen::MatrixXd P;  // control points (unknowns)

    M << -1.0, 3.0, -3.0, 1.0,
          3.0, -6.0, 3.0, 0.0,
         -3.0, 3.0, 0.0, 0.0,
          1.0, 0.0, 0.0, 0.0;

    T = Eigen::MatrixXd(data.size(), 4);
    D = Eigen::MatrixXd(data.size(), 2);
    for (int i = 0; i < data.size(); ++i) {
        D.row(i) = data[i];
        T.coeffRef(i, 3) = 1.0;
        for (int j = 2; j >= 0; --j) {
            T.coeffRef(i, j) = u[i] * T.coeffRef(i, j + 1);
        }
    }

    Eigen::MatrixXd A = T * M;
    Eigen::MatrixXd ATA = A.transpose() * A;
    Eigen::MatrixXd DD = A.transpose() * D;
    Eigen::MatrixXd B(ATA.rows() + 2, ATA.cols() + 2);
    Eigen::MatrixXd E(DD.rows() + 2, DD.cols());
    Eigen::Vector<double, 6> V1(1.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    Eigen::Vector<double, 6> V2(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    B.topLeftCorner<>(ATA.rows(), ATA.cols()) = ATA;
    B.row(ATA.rows()) = V1;
    B.row(ATA.rows()+1) = V2;
    B.col(ATA.cols()) = V1.transpose();
    B.col(ATA.cols()+1) = V2.transpose();

    E.topLeftCorner(DD.rows(), DD.cols()) = DD;
    E.row(DD.rows()) = data[0];
    E.row(DD.rows()+1) = data.back();

    P = Eigen::MatrixXd(6, 2);
    P = (B).ldlt().solve(E);

    p0 = P.row(0);
    p1 = P.row(1);
    p2 = P.row(2);
    p3 = P.row(3);
}


void Bezier2D::chordLengthParameterize(const std::vector<Point::VectorType> &data, std::vector<Point::Scalar> &u) {
    if (data.empty()) {
        std::cout << "chordLengthParameterize: insufficient data points" << std::endl;
    }

    u.resize(data.size());
    u[0] = Point::Scalar(0.0);

    for (int i = 1; i < data.size(); ++i) {
        u[i] = (data[i] - data[i - 1]).norm() + u[i - 1];
    }

    for (int i = 1; i < data.size(); ++i) {
        u[i] /= u.back();
    }
}

void Bezier2D::reparameterize(const std::vector<Point::VectorType> &data, std::vector<Point::Scalar> &u) {
    for (int i = 0; i < data.size(); ++i) {
        u[i] = newtonRaphsonRootFind(data[i], u[i]);
    }
}

Point::Scalar Bezier2D::newtonRaphsonRootFind(const Point::VectorType& data, Point::Scalar param) {
    Point::Scalar num, den;

    Point::VectorType p = eval(param);
    Point::VectorType _pp[3];
    Point::VectorType _ppp[2];

    _pp[0] = (p1 - p0) * 3.0;
    _pp[1] = (p2 - p1) * 3.0;
    _pp[2] = (p3 - p2) * 3.0;

    _ppp[0] = (_pp[1] - _pp[0]) * 2.0;
    _ppp[1] = (_pp[2] - _pp[1]) * 2.0;

    Point::Scalar ux = 1.0 - param;
    Point::VectorType pp = ux * ux * _pp[0] + 2.0 * ux * param * _pp[1] + param * param * _pp[2];
    Point::VectorType ppp = _ppp[0] * ux + _ppp[1] * param;

    num = (p.x() - data.x()) * pp.x() + (p.y() - data.y()) * pp.y();
    den = pp.x() * pp.x() + pp.y() * pp.y() + (p.x() - data.x()) * ppp.x() + (p.y() - data.y()) * ppp.y();

    if (den < 1e-6) return param;
    return param - (num / den);
}

/**
 * Returns the maximum error (L2 norm) between the data and the fitted cubic 
 */
Point::Scalar Bezier2D::maxError(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    Point::Scalar maxError = std::numeric_limits<Point::Scalar>::min();
    Point::Scalar error;
    // std::cout << "*********"<< std::endl;
    for (int i = 0; i < data.size(); ++i) {
        error = (data[i] - eval(u[i])).norm();
        // std::cout << "data[i] = " << data[i].transpose() << "     vs      " <<  eval(u[i]).transpose()  << std::endl;
        // std::cout << "error at u=" << u[i] << " : " << error<< std::endl;
        if (error > maxError) maxError = error;
    }
    // std::cout << "Max error = " << maxError << std::endl;
    return maxError;
}

/******************************************************************************************************************************/

Bezier1D::Bezier1D() : p0(0.0), p1(0.33), p2(0.66), p3(1.0) {

}

Bezier1D::Bezier1D(Point::Scalar _p0, Point::Scalar _p1, Point::Scalar _p2, Point::Scalar _p3) : p0(_p0), p1(_p1), p2(_p2), p3(_p3) {

}   

Bezier1D::Bezier1D(const Bezier1D &other) : p0(other.p0), p1(other.p1), p2(other.p2), p3(other.p3) {

}

Point::Scalar Bezier1D::eval(Point::Scalar t) const {
    Point::Scalar tx = 1.0 - t;
    return p0 * (tx * tx * tx) +
           p1 * (3.0 * tx * tx * t) +
           p2 * (3.0 * tx * t * t) +
           p3 * (t * t * t);
}

// solve T*M*P=data for P
void Bezier1D::fit(Eigen::Vector4d data, Eigen::Vector4d t) {
    Eigen::Matrix4d M;  // bezier coeffs
    Eigen::Matrix4d T;  // param (|data| x |coeffs|)
    Eigen::Vector4d P;  // control points (unknowns)
 
    M << -1.0, 3.0, -3.0, 1.0,
          3.0, -6.0, 3.0, 0.0,
         -3.0, 3.0, 0.0, 0.0,
          1.0, 0.0, 0.0, 0.0;

    for (int i = 0; i < 4; ++i) {
        T.coeffRef(i, 3) = 1.0;
        for (int j = 2; j >= 0; --j) {
            T.coeffRef(i, j) = t[i] * T.coeffRef(i, j + 1);
        }
    }

    P = (T * M).colPivHouseholderQr().solve(data);

    p0 = P(0);
    p1 = P(1);
    p2 = P(2);
    p3 = P(3);
}