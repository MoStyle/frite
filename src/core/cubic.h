/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __CUBIC_H__
#define __CUBIC_H__

#include <QTransform>
#include "point.h"
#include <vector>

#define LUT_PRECISION 50

// Represent a 2D cubic Bézier curve segment defined by 2 endpoints and 2 control points
// The fitting of the curve to a dataset is done with a simplified version of "An Algorithm for Automatically Fitting Digitized Curves"
// by Philip J. Schneider
// from "Graphics Gems", Academic Press, 1990"
class Bezier2D {
public:
    Bezier2D();
    Bezier2D(Point::VectorType _p0, Point::VectorType _p1, Point::VectorType _p2, Point::VectorType _p3);
    Bezier2D(const Bezier2D &other);

    Point::VectorType eval(Point::Scalar t) const;
    Point::VectorType evalDer(Point::Scalar t) const;
    Point::VectorType evalArcLength(Point::Scalar s) const;
    Point::Scalar evalYFromX(Point::Scalar x);
    Point::Scalar arcLength(Point::Scalar t) const;
    Point::Scalar speed(Point::Scalar t) const;
    Point::VectorType getP0() const { return p0; }
    Point::VectorType getP1() const { return p1; }
    Point::VectorType getP2() const { return p2; }
    Point::VectorType getP3() const { return p3; }
    Point::Scalar length() const { return len; }

    void fit(const std::vector<Point::VectorType> &data, bool constrained=true);
    void fitWithParam(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u);
    void fitExtremities(Point::VectorType start, Point::VectorType end);
    void split(Point::Scalar t, Bezier2D &left, Bezier2D &right) const;
    void updateArclengthLUT();
    void setP0(Point::VectorType _p0) { p0 = _p0; }
    void setP1(Point::VectorType _p1) { p1 = _p1; }
    void setP2(Point::VectorType _p2) { p2 = _p2; }
    void setP3(Point::VectorType _p3) { p3 = _p3; }

    // get t from s
    inline Point::Scalar param(Point::Scalar s) const {
        if (s >= 1.0) return 1.0;
        if (s <= 0.0) return 0.0;
        int i = 0;
        while (alengthLUT[0][i] < s) ++i;
        Point::Scalar sInterp = (s - alengthLUT[0][i-1]) / (alengthLUT[0][i] - alengthLUT[0][i-1]);
        return alengthLUT[1][i-1] * (1.0 - sInterp) + alengthLUT[1][i] * sInterp;
    }

    Point::Scalar tFromX(Point::Scalar x);

    std::array<std::array<Point::Scalar, 50>, 2> lut() const { return alengthLUT; }
    std::array<std::array<Point::Scalar, 50>, 2> alengthLUT; // 0:s  1:t

private:
    void fitBezier(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u);
    void fitBezierConstrained(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u);
    void chordLengthParameterize(const std::vector<Point::VectorType> &data, std::vector<Point::Scalar> &u);
    void reparameterize(const std::vector<Point::VectorType> &data, std::vector<Point::Scalar> &u);
    Point::Scalar newtonRaphsonRootFind(const Point::VectorType& data, Point::Scalar param);
    Point::Scalar maxError(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u); 

    Point::VectorType p0, p1, p2, p3;
    Point::Scalar len;
};

/**
 * 1D cubic Bézier defined by 4 values
 */
class Bezier1D {
public:
    Bezier1D();
    Bezier1D(Point::Scalar _p0, Point::Scalar _p1, Point::Scalar _p2, Point::Scalar _p3);
    Bezier1D(const Bezier1D &other);

    Point::Scalar eval(Point::Scalar t) const;

    void fit(Eigen::Vector4d data, Eigen::Vector4d t);

private:
    Point::Scalar p0, p1, p2, p3;
};

#endif // __CUBIC_H__