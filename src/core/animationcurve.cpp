/*
 * SPDX-FileCopyrightText: 2013 Romain Vergne <romain.vergne@inria.fr>
 * SPDX-FileCopyrightText: 2020-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: MPL-2.0
 */

#include "animationcurve.h"

#include <iostream>
#include <QRegExp>
#include <QDebug>
#include <Eigen/Cholesky>
#include <Eigen/Geometry>
#include "chartitem.h"
#include "point.h"
#include "dialsandknobs.h"

#include "utils/geom.h"

using namespace std;
using namespace Eigen;

CurveInterpolator::CurveInterpolator(const Vector2d &pt) { _points.push_back(pt); }

CurveInterpolator::CurveInterpolator(CurveInterpolator *curve) {
    if (!curve) return;

    _points = std::vector<Vector2d>(curve->nbPoints());
    _tangents = std::vector<Vector4d>(curve->nbTangents());

    for (unsigned int i = 0; i < curve->nbPoints(); ++i) {
        _points[i] = curve->point(i);
    }

    for (unsigned int i = 0; i < curve->nbTangents(); ++i) {
        _tangents[i] = curve->tangent(i);
    }
}

double CurveInterpolator::evalInverse(double y) const {
    qCritical() << "evalInverse is only defined for monotonic interpolators";
    return 0.0;
}

int CurveInterpolator::addKeyframe(const Vector2d &pt) {
    vector<Vector2d>::iterator it = _points.begin();
    int count = 0;
    while (it != _points.end() && (*it)[0] < pt[0]) {
        it++;
        count++;
    }
    int idx = std::distance(_points.begin(), it);
    if (it != _points.end() && (*it)[0] == pt[0]) {
        // simply move the current keyframe
        (*it)[1] = pt[1];
    } else {
        // add a new control point
        _points.insert(it, pt);
    }
    return idx;
}

void CurveInterpolator::setKeyframe(const Vector2d &pt, unsigned int i) {
    assert(i < nbPoints());

    // points should be kept ordered
    if ((i > 0 && pt[0] < _points[i - 1][0]) || (i < nbPoints() - 1 && pt[0] > _points[i + 1][0])) return;

    _points[i] = pt;
}

void CurveInterpolator::setTangent(const Vector2d &pt, unsigned int i, unsigned int side) {
    if (i >= _tangents.size()) return;
    _tangents[i][2 * side] = pt[0];
    _tangents[i][2 * side + 1] = pt[1];
}

void CurveInterpolator::setTangent(const Vector4d &pt, unsigned int i) {
    if (i >= _tangents.size()) return;
    _tangents[i] = pt;
}

void CurveInterpolator::delKeyframe(unsigned int i) {
    assert(i < nbPoints());

    if (nbPoints() > 1)
        // we need to keep at least one point in a curve
        _points.erase(_points.begin() + i);
}

void CurveInterpolator::moveKeys(int offsetFirst, int offsetLast) {
    int nbPts = nbPoints();
    if (nbPts == 1) {
        setKeyframe(Vector2d(_points[0][0] + offsetFirst, _points[0][1]), 0);
    } else {
        for (int i = 0; i < nbPts; ++i) {
            double alpha = (double)i / (double)(nbPts - 1);
            double key = (1.f - alpha) * (_points[i][0] + offsetFirst) + alpha * (_points[i][0] + offsetLast);
            setKeyframe(Vector2d(key, _points[i][1]), i);
            //            _tangents[i] = ; // TODO
        }
    }
}

void CurveInterpolator::removeKeyframeBefore(int frame) {
    int idx = -1;
    for (int i = 0; i < nbPoints(); ++i) {
        if (_points[i][0] >= frame) {
            idx = i - 1;
            break;
        }
    }
    if (idx >= 0) {
        _points.erase(_points.begin(), _points.begin() + idx);
        _tangents.erase(_tangents.begin(), _tangents.begin() + idx);
    }
}

void CurveInterpolator::removeKeyframeAfter(int frame) {
    int idx = -1;
    for (int i = nbPoints() - 1; i >= 0; --i) {
        if (_points[i][0] <= frame) {
            idx = i + 1;
            break;
        }
    }
    if (idx >= 0 && idx < nbPoints()) {
        _points.erase(_points.begin() + idx, _points.end());
        _tangents.erase(_tangents.begin() + idx, _tangents.end());
    }
}
void CurveInterpolator::removeKeys() {
    // if (_points.size() > 2) {
    //     _points.erase(_points.begin() + 1, _points.begin() + _points.size() - 1);
    // }
    // if (_tangents.size() > 2) {
    //     _tangents.erase(_tangents.begin() + 1, _tangents.begin() + _tangents.size() - 1);
    // }
    _points.clear();
    _tangents.clear();
}

double CurveInterpolator::normalizeX() {
    if (nbPoints() < 2) {
        std::cerr << "Error: cannot normalize curve with less than two control points!" << std::endl;
        return 0.0;
    }

    double x0 = _points[0][0];
    double xN = _points.back()[0];
    double ratio = 1.0 / (xN - x0);

    if (x0 < 1e-5) x0 = 0.0;

    // scale x-component of positions
    for (unsigned int i = 1; i < nbPoints() - 1; ++i) {
        _points[i][0] = std::min(1.0, ratio * (_points[i][0] - x0));
    }
    
    // scale x-component of tangents
    if (useTangents()) {
        for (unsigned int i = 0; i < nbTangents(); ++i) {
            _tangents[i][0] = _tangents[i][0] * ratio;     
            _tangents[i][2] = _tangents[i][2] * ratio;     
        }
    }

    // clamp these one for precision
    _points.back()[0] = 1.0;
    _points.front()[0] = 0.0;
    return ratio;
}

void CurveInterpolator::smoothTangents() {
    if (nbPoints() <= 1) return;
    _tangents.clear();
    Eigen::Vector2d d;
    d = (_points[1] - _points[0]) * 0.5;
    _tangents.push_back(Eigen::Vector4d(d.x(), d.y(), -d.x(), -d.y()));
    for (int i = 1; i < nbPoints() - 1; ++i) {
        Eigen::Vector2d d1 = _points[i + 1] - _points[i];
        Eigen::Vector2d d2 = _points[i] - _points[i - 1];
        d = (d1 + d2) * 0.25;
        _tangents.push_back(Eigen::Vector4d(d.x(), d.y(), -d.x(), -d.y()));
    }
    d = (_points[_points.size() - 1] - _points[_points.size() - 2]) * 0.5;
    _tangents.push_back(Eigen::Vector4d(d.x(), d.y(), -d.x(), -d.y()));
}

void CurveInterpolator::scaleTangentVertical(double factor) {
    for (unsigned int i = 0; i < nbTangents(); i++) {
        _tangents[i][1] *= factor;
        _tangents[i][3] *= factor;
    }
}

int SplineInterpolator::addKeyframe(const Vector2d &pt) {
    const unsigned int nb = nbPoints();
    int idx = CurveInterpolator::addKeyframe(pt);
    if (nb != nbPoints()) {
        nbPointsChanged();
    }
    computeSolution();
    return idx;
}

void SplineInterpolator::setKeyframe(const Vector2d &pt, unsigned int i) {
    CurveInterpolator::setKeyframe(pt, i);
    computeSolution();
}

void SplineInterpolator::delKeyframe(unsigned int i) {
    const unsigned int nb = nbPoints();
    CurveInterpolator::delKeyframe(i);
    if (nb != nbPoints()) {
        nbPointsChanged();
    }
    computeSolution();
}

// !all boundary values are set to zero!
void SplineInterpolator::resample(unsigned int n) {
    std::vector<Vector2d> sampledPoints(n + 2);
    int nb = _points.size();
    for (unsigned int i = 1; i <= n; ++i) {
        double x = (double)(i)/(double)(n+1);
        sampledPoints[i] = Vector2d(x, evalAt(x));
    }

    sampledPoints.front() = Vector2d::Zero();
    sampledPoints.back() = Vector2d(1.0, 0.0);

    _points = sampledPoints;
    if (nb != nbPoints()) {
        nbPointsChanged();
    }
    computeSolution();
}

void SplineInterpolator::computeSolution() {
    if (nbPoints() < 3) return;

    const unsigned int ms = _A.rows();
    const unsigned int np = nbPoints();

    unsigned int p = 0;
    unsigned int i = 0;
    unsigned int j = 0;

    // constraint matrix
    for (unsigned int s = 0; s < np - 1; ++s) {
        // f(xi) = yi
        i++;
        _A(i, j) = 1.0;                             // 1
        _A(i, j + 1) = _points[p][0];                // x
        _A(i, j + 2) = _A(i, j + 1) * _A(i, j + 1);  // x^2
        _A(i, j + 3) = _A(i, j + 2) * _A(i, j + 1);  // x^3
        _b(i) = _points[p][1];                       // y

        // f(xi+1) = yi+1
        i++;
        p++;
        _A(i, j) = 1.0;                             // 1
        _A(i, j + 1) = _points[p][0];                // x
        _A(i, j + 2) = _A(i, j + 1) * _A(i, j + 1);  // x^2
        _A(i, j + 3) = _A(i, j + 2) * _A(i, j + 1);  // x^3
        _b(i) = _points[p][1];                       // y

        // check if we have to stop
        if (s == np - 2) break;

        // fi'(x)=fi+1'(x)
        i++;
        _A(i, j + 1) = 1.0;                     // 1
        _A(i, j + 2) = 2.0 * _points[p][0];     // 2x
        _A(i, j + 3) = 3.0 * _A(i - 1, j + 2);  // 3x^2
        _A(i, j + 5) = -_A(i, j + 1);            // -1
        _A(i, j + 6) = -_A(i, j + 2);            // -2x
        _A(i, j + 7) = -_A(i, j + 3);            // -3x^2

        // fi''(x)=fi+1''(x)
        i++;
        _A(i, j + 2) = 2.0;                  // 2
        _A(i, j + 3) = 6.0 * _points[p][0];  // 6x
        _A(i, j + 6) = -_A(i, j + 2);         // -2
        _A(i, j + 7) = -_A(i, j + 3);         // -6x

        j += 4;
    }

    // boundary conditions (natural spline)
    _A(0, 2) = 2.0;
    _A(0, 3) = 6.0 * _points[0][0];
    _A(ms - 1, ms - 2) = 2.0;
    _A(ms - 1, ms - 1) = 6.0 * _points[np - 1][0];

    _x = _A.inverse() * _b;

    // tried this, but does not work...
    //_A.ldlt().solve(_b,&_x);
}

void SplineInterpolator::nbPointsChanged() {
    if (nbPoints() < 3) return;
    const unsigned int nb = (nbPoints() - 1) * 4;
    _A = MatrixXf::Zero(nb, nb);
    _b = VectorXf::Zero(nb);
}

CubicPolynomialInterpolator::CubicPolynomialInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt) {
    _A = MatrixXf::Zero(5, 5);
    _b = VectorXf::Zero(5);
    _x = VectorXf::Zero(5);
    _constraintIdx = 2;
    _prevControlPointsY.resize(3);
    computeSolution();
}

CubicPolynomialInterpolator::CubicPolynomialInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) {
    _A = MatrixXf::Zero(5, 5);
    _b = VectorXf::Zero(5);
    _x = VectorXf::Zero(5);
    _constraintIdx = 2;
    _prevControlPointsY.resize(3);
    if (nbPoints() == 5) {
        for (int i = 1; i < 4; ++i) {
            _prevControlPointsY[i-1] = _points[i].y();
        }
    }
    computeSolution();
}

int CubicPolynomialInterpolator::addKeyframe(const Vector2d &pt) {
    int idx = CurveInterpolator::addKeyframe(pt);
    if (nbPoints() == 5) {
        for (int i = 1; i < 4; ++i) {
            _prevControlPointsY[i-1] = _points[i].y();
        }
    }
    computeSolution();
    return idx;
}

void CubicPolynomialInterpolator::setKeyframe(const Vector2d &pt, unsigned int i) {
    _constraintIdx = i;
    if (nbPoints() == 5) {
        for (int i = 1; i < 4; ++i) {
            _prevControlPointsY[i-1] = _points[i].y();
        }
    }
    CurveInterpolator::setKeyframe(pt, i);
    computeSolution();
    resampleControlPoints();
}

void CubicPolynomialInterpolator::delKeyframe(unsigned int i) {
    const unsigned int nb = nbPoints();
    CurveInterpolator::delKeyframe(i);
    computeSolution();
}

void CubicPolynomialInterpolator::setControlPoints(double p1, double p2, double p3, int fixedPointIdx) {
    if (p1 > p2 || p2 > p3) {
        qWarning() << "setControlPoint error! Given values are not correctly ordered!";
        return;
    }
    if (nbPoints() == 5) {
        for (int i = 1; i < 4; ++i) {
            _prevControlPointsY[i-1] = _points[i].y();
        }
    }
    _constraintIdx = fixedPointIdx;
    _points[1].y() = p1;
    _points[2].y() = p2;
    _points[3].y() = p3;
    computeSolution();
    resampleControlPoints();
}

CubicPolynomialInterpolator *CubicPolynomialInterpolator::splitAt(double x) {
    double al, bl, cl, ar, br, cr;
    double y = evalAt(x);
    double yComp = 1.0 - y;
    double xComp = 1.0 - x;
    double xx = x*x;
    double xxx = xx * x;

    // compute coefficients of the right half
    ar = _x[2] * xComp * xComp * xComp / yComp;
    br = (3.0 * _x[2] * x + _x[1]) * xComp * xComp / yComp;
    cr = (3.0 * _x[2] * xx + 2.0 * _x[1] * x + _x[0]) * xComp / yComp;
    CubicPolynomialInterpolator *cubicSecondHalf = new CubicPolynomialInterpolator(Eigen::Vector2d::Zero());
    cubicSecondHalf->_points.resize(5);
    cubicSecondHalf->_points[0] = Eigen::Vector2d::Zero();
    cubicSecondHalf->_points[1].x() = 0.25;
    cubicSecondHalf->_points[2].x() = 0.50;
    cubicSecondHalf->_points[3].x() = 0.75;
    cubicSecondHalf->_points[4] = Eigen::Vector2d::Ones();
    cubicSecondHalf->setCoeffs(ar, br, cr);
    cubicSecondHalf->resampleControlPoints();

    // compute coefficients of the left half
    al = xxx * _x[2] / y;
    bl = xx  * _x[1] / y;
    cl = x   * _x[0] / y;
    setCoeffs(al, bl, cl);
    resampleControlPoints();

    return cubicSecondHalf;
}

void CubicPolynomialInterpolator::computeSolution() {
    if (nbPoints() < 5) return;

    VectorXf _xPrev = _x;

    double xs[3] = {0.25, 0.50, 0.75};
    computeSolutionAux(xs[0], xs[1], xs[2]);
 
    // check if the cubic polynomial is still strictly monotonic
    // which in this case boils down to checking if the derivative
    // has any real root in the interval [0,1]
    auto isMonotonic = [&]() {
        double delta = 4.0 * _x[1] * _x[1] - 12.0 * _x[2] * _x[0];
        if (delta  < 0) return true;
        double sqDelta = std::sqrt(delta);
        double x1 = ((-2.0) * _x[1] - sqDelta) / (6 * _x[2]);
        double x2 = ((-2.0) * _x[1] + sqDelta) / (6 * _x[2]);
        if ((x1 > 0.0 && x1 < 1.0) || (x2 > 0.0 && x2 < 1.0)) {
            return false;
        }
        return true;
    };

    int attempt = 0;
    while (!isMonotonic() && attempt < 20) {
        for (int i = 1; i < 4; ++i) {
            if (i == _constraintIdx) continue;
            // double delta = _points[i].y() - _prevControlPointsY[i-1];
            // if (delta > 0) xs[i-1] -= 0.01;
            // else           xs[i-1] += 0.01;
            _points[i].y() = (_points[i].y() - _prevControlPointsY[i-1]) / 2.0 + _prevControlPointsY[i-1];
        }
        computeSolutionAux(xs[0], xs[1], xs[2]);
        attempt++;
    }
    // qDebug() << "finished at attempt " << attempt;

    if (!isMonotonic()) {
        _x = _xPrev;
    }
}

void CubicPolynomialInterpolator::computeSolutionAux(double x1, double x2, double x3) {
    if (nbPoints() < 5) return;

    MatrixXf L = MatrixXf::Zero(5, 3);
    VectorXf b = VectorXf::Zero(5);
    double xs[3] = {x1, x2, x3};

    // least squares
    int i;
    for (i = 0; i < 5; ++i) {
        if (i >= 1 && i <=3) {
            L(i, 0) = xs[i-1];        // x
        } else {
            L(i, 0) = i / 4.0;       // x
        }
        L(i, 1) = L(i, 0) * L(i, 0);  // x^2
        L(i, 2) = L(i, 1) * L(i, 0);  // x^3
    }
    b(0) = 0.0;
    b(1) = _points[1][1];
    b(2) = _points[2][1]; 
    b(3) = _points[3][1];
    b(4) = 1.0;            

    MatrixXf Lt = L.transpose();
    MatrixXf LtL = Lt * L;
    VectorXf At_b = Lt * b; // 3x5  5x1
    _b.head<3>(0) = At_b;  
    _A.block<3, 3>(0, 0) = LtL;
    
    // C
    _A(3, 0) = xs[_constraintIdx-1];  
    _A(3, 1) = _A(3, 0) * _A(3, 0);  
    _A(3, 2) = _A(3, 1) * _A(3, 0);  
    // C^T
    _A(0, 3) = xs[_constraintIdx-1];  
    _A(1, 3) = _A(0, 3) * _A(0, 3);  
    _A(2, 3) = _A(1, 3) * _A(0, 3);  
    _b(3) = _points[_constraintIdx][1];

    // f(1) = 1
    for (int j = 0; j < 3; ++j) {
        _A(4, j) = 1.0;  // C (x, x^2, x^3)
        _A(j, 4) = 1.0;  // C^T
    }
    _b(4) = 1.0;  // y

    // zeros
    for (int i = 3; i < 5; ++i) {
        for (int j = 3; j < 5; ++j) {
            _A(i, j) = 0.0;
        }
    }

    _x = _A.ldlt().solve(_b);
}

void CubicPolynomialInterpolator::resampleControlPoints() {
    for (int i = 1; i < 4; ++i) {
        _points[i][1] = evalAt(i / 4.0);
    }
}

void CubicPolynomialInterpolator::setCoeffs(double a, double b, double c) {
    _x.resize(5);
    _x[4] = 1.0;
    _x[3] = 1.0;
    _x[2] = a;
    _x[1] = b;
    _x[0] = c;
}

CubicMonotonicInterpolator::CubicMonotonicInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt) {
    if (nbPoints() == 1 && _points[0].x() == 1.0) _points[0] = Vector2d::Ones();
    makeSlopes();
}

CubicMonotonicInterpolator::CubicMonotonicInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) {
    if (nbPoints() == 1 && _points[0].x() == 1.0) _points[0] = Vector2d::Ones();
    makeSlopes();
}

double CubicMonotonicInterpolator::evalInverse(double y) const {
    for (unsigned int i = 0; i < nbPoints() - 1; ++i) {
        if (_points[i + 1].y() >= y) {
            double x = _points[i].x();
            double dx = _points[i + 1].x() - x;
            Eigen::Vector4d coeffs = Geom::hermiteCoeffs(_points[i].y(), _slopes[i] * dx, _points[i + 1].y(), _slopes[i + 1] * dx);
            if (std::abs(coeffs[0]) < 1e-8) { // degree < 3
                if (std::abs(coeffs[1]) < 1e-8) { // degree < 2
                    return ((y - coeffs[3]) / coeffs[2]) * dx + x; // x = (y - b) / a
                }
                return Utils::quadraticRoot(coeffs[1], coeffs[2], coeffs[3] - y) * dx + x;
            }
            return Utils::cubicRoot(coeffs[1] / coeffs[0], coeffs[2] / coeffs[0], (coeffs[3] - y) / coeffs[0]) * dx + x;
        }
    }
    return 0.0;
}

int CubicMonotonicInterpolator::addKeyframe(const Vector2d &pt) {
    double slope = (pt.x() >= _points.front().x() && pt.x() <= _points.back().x()) ? evalDerivativeAt(pt.x()) : 0.0;
    unsigned int sizeBefore = nbPoints();
    int idx = CurveInterpolator::addKeyframe(pt);
    if (idx == _slopes.size() || _slopes.empty()) _slopes.push_back(0.0);
    else if (sizeBefore != nbPoints()) _slopes.insert(_slopes.begin() + idx, slope); 
    if (nbPoints() == 2) {
        makeSlopes();
    } else if (idx == 0 || idx == _slopes.size() - 1) {
        updateSlope(idx, false);
    } 
    return idx;
}

void CubicMonotonicInterpolator::setKeyframe(const Eigen::Vector2d &pt, unsigned int i) {
    CurveInterpolator::setKeyframe(pt, i);
    makeNaturalC2();
}

void CubicMonotonicInterpolator::delKeyframe(unsigned int i) {
    CurveInterpolator::delKeyframe(i);
    if (nbPoints() >= 1)
        _slopes.erase(_slopes.begin() + i);
    makeNaturalC2();
}

void CubicMonotonicInterpolator::removeLastPoint() {
    CurveInterpolator::removeLastPoint();
    _slopes.resize(_slopes.size() - 1);
}

double CubicMonotonicInterpolator::normalizeX() {
    double ratioX = CurveInterpolator::normalizeX();
    if (nbPoints() < 2) return 0.0;
    // also normalize Y in this case (spacing curve)
    double y0 = _points[0].y();
    double yN = _points.back().y();
    double ratio = 1.0 / (yN - y0);

    if (y0 < 1e-5) y0 = 0.0;

    // scale y-component control points
    for (unsigned int i = 1; i < nbPoints() - 1; ++i) {
        _points[i].y() = std::min(1.0, ratio * (_points[i].y() - y0));
    }
    
    // scale slopes
    double slopeScaling = ratio / ratioX;
    for (unsigned int i = 0; i < nbPoints(); ++i) {
        _slopes[i] *= slopeScaling;
    }

    // clamp these one for precision
    _points.back().y() = 1.0;
    _points.front().y() = 0.0;
    return ratioX;
}

void CubicMonotonicInterpolator::makeSlopes() {
    if (nbPoints() < 2) return;
    unsigned int nb = nbPoints();

    // compute sucessive secant slopes
    std::vector<double> secantSlopes(nb - 1);
    for (unsigned int i = 0; i < nb - 1; ++i) {
        secantSlopes[i] = (_points[i+1].y() - _points[i].y()) / (_points[i+1].x() - _points[i].x());
    }

    // init slopes
    _slopes.resize(nb);
    _slopes[0] = secantSlopes[0];
    _slopes[nb - 1] = secantSlopes[nb - 2];
    for (unsigned int i = 1; i < nb - 1; i++) {
        _slopes[i] = (secantSlopes[i-1] + secantSlopes[i]) * 0.5;
    }

    // check and adjust for monotonicity
    for (unsigned int i = 0; i < nb - 1; ++i) {
        checkSlopeMonotonicity(i, _slopes[i], i == 0 ? secantSlopes[0] : secantSlopes[i-1], i == nb-1 ? secantSlopes.back() : secantSlopes[i]);
    }
}

void CubicMonotonicInterpolator::makeNaturalC2() {
    if (nbPoints() < 2) return;
    unsigned int nb = nbPoints();
    _slopes.resize(nb);
    
    Eigen::MatrixXd coeffs(nb, nb);
    Eigen::VectorXd points(nb);
    Eigen::VectorXd slopes(nb);

    coeffs.fill(0);

    // natural constraints
    coeffs(0, 0) = 2.0; coeffs(0, 1) = 1.0;   
    coeffs(nb-1, nb-1) = 2.0; coeffs(nb-1, nb-2) = 1.0;
    points(0) = 3.0 * (_points[1].y() - _points[0].y());
    points(nb-1) = 3.0 * (_points[nb-1].y() - _points[nb-2].y());

    // matching second derivative constraint 
    for (unsigned int i = 1; i < nb - 1; ++i) {
        coeffs(i, i-1) = 1.0;
        coeffs(i, i) = 4.0;
        coeffs(i, i+1) = 1.0;
        points(i) = 3.0 * (_points[i+1].y() - _points[i-1].y());
    }

    slopes = coeffs.ldlt().solve(points);

    // set new slopes
    double dt = _points[1].x() - _points[0].x();
    for (int i = 0; i < nb; ++i) {
        _slopes[i] = slopes(i) / dt;
    }

    // check and adjust slopes for monotonicity
    std::vector<double> secantSlopes(nb - 1);
    _slopes[0] = std::max(0.0, _slopes[0]);
    _slopes.back() = std::max(0.0, _slopes.back());
    for (unsigned int i = 0; i < nb - 1; ++i) secantSlopes[i] = (_points[i+1].y() - _points[i].y()) / (_points[i+1].x() - _points[i].x());
    for (unsigned int i = 1; i < nb - 1; ++i) checkSlopeMonotonicity(i, _slopes[i], i == 0 ? secantSlopes[0] : secantSlopes[i-1], i == nb-1 ? secantSlopes.back() : secantSlopes[i]);
}


void CubicMonotonicInterpolator::resample(unsigned int n) {
    std::vector<Vector2d> sampledPoints(n + 2);
    std::vector<double> sampledSlopes(n + 2);

    for (unsigned int i = 1; i <= n; ++i) {
        double x = (double)(i)/(double)(n+1);
        sampledPoints[i] = Vector2d(x, std::max(std::min(evalAt(x), 1.0), 0.0));
        sampledSlopes[i] = evalDerivativeAt(x);
    }

    sampledPoints.front() = Vector2d::Zero();
    sampledPoints.back() = Vector2d::Ones();
    sampledSlopes.front() = _slopes.front();
    sampledSlopes.back() = _slopes.back();

    // if (resetSlopes) {
        _slopes = sampledSlopes;
    // }
    _points = sampledPoints;
}

void CubicMonotonicInterpolator::resampleDichotomic(unsigned int maxControlPoints, unsigned int nbFrames) {
    CubicMonotonicInterpolator newCurve(Vector2d::Zero());

    newCurve._points.clear();
    newCurve._points.push_back(_points.front());
    newCurve._points.push_back(_points.back());
    newCurve._slopes.push_back(_slopes.front());
    newCurve._slopes.push_back(_slopes.back());

    double xa = newCurve._points.front().x();
    double xb = newCurve._points.back().x();
    double threshold = std::min(1.0 / nbFrames, 0.1);
    
    resampleDichotomicAddControlPoint(&newCurve, xa, xb, 0, 3, 0.003);

    _points = newCurve._points;
    _slopes = newCurve._slopes;
}

void CubicMonotonicInterpolator::debugSlopes() {
    for (int i = 0; i < _slopes.size(); ++i) {
        qDebug() << "slope " << i << " = " << _slopes[i];
        qDebug() << "der " << i << " = " << evalDerivativeAt(_points[i].x());
        qDebug() << "ratio " << i << " = " << _slopes[i] / evalDerivativeAt(_points[i].x());
    }
}

double CubicMonotonicInterpolator::smallestXInterval() const {
    double minInterval = 1.0;
    for (int i = 1; i < nbPoints(); ++i) {
        double interval = _points[i].x() - _points[i - 1].x(); 
        if (interval < minInterval) minInterval = interval;
    }
    return minInterval;
}

void CubicMonotonicInterpolator::smoothTangents() {
    if (nbSlopes() < 3) return;
    std::vector<double> slopesCopy = _slopes;
    for (int i = 1; i < nbSlopes() - 1; ++i) {
        _slopes[i] = (slopesCopy[i-1] + slopesCopy[i+1]) / 2.0;
    }
    _slopes[0] = (slopesCopy[0] + slopesCopy[1]) * 0.5;
    _slopes[nbSlopes() - 1] = (slopesCopy[nbSlopes() - 1] + slopesCopy[nbSlopes() - 2]) * 0.5;
}

void CubicMonotonicInterpolator::updateSlope(unsigned int i, bool updateNeighbors) {
    if (i < 0 || i >= nbPoints()) return;

    // update slope
    if (i == 0) {
        _slopes[0] = (_points[1].y() - _points[0].y()) / (_points[1].x() - _points[0].x());
        if (std::abs(_points[0].y() - _points[1].y()) < 1e-5) _slopes[0] = 0.0;
    } else if (i == nbPoints() - 1) {
        _slopes[nbPoints() - 1] = (_points[nbPoints() - 1].y() - _points[nbPoints() - 2].y()) / (_points[nbPoints() - 1].x() - _points[nbPoints() - 2].x());
    } else {
        double leftSecantSlope = (_points[i].y() - _points[i-1].y()) / (_points[i].x() - _points[i-1].x());
        double rightSecantSlope = (_points[i+1].y() - _points[i].y()) / (_points[i+1].x() - _points[i].x());
        _slopes[i] = (leftSecantSlope + rightSecantSlope) * 0.5;
        // adjust for monotonicity
        checkSlopeMonotonicity(i, _slopes[i], leftSecantSlope, rightSecantSlope);
        if (i - 1 >= 0 && updateNeighbors) {
            updateSlope(i - 1, false);
        }
        if (i + 1 <= nbPoints() - 1 && updateNeighbors) {
            updateSlope(i + 1, false);
        }
    }
}

void CubicMonotonicInterpolator::checkSlopeMonotonicity(unsigned int i, double slope, double leftSecantSlope, double rightSecantSlope) {
    if (i < nbPoints() - 1 && std::abs(_points[i+1].y() - _points[i].y()) < 1e-5) { // connect points at the same y with a straight line
        _slopes[i] = 0.0;
        _slopes[i + 1] = 0.0;
    } else {
        if (Utils::sgn(leftSecantSlope) != Utils::sgn(rightSecantSlope) || _slopes[i] < 0.0) { // inflexion point => set the slope to 0
            _slopes[i] = 0.0;
        } else { // bound slope magnitude by 3 times the left or right secant slope (whatever is min)
            _slopes[i] *= std::min(std::min((3.0 * leftSecantSlope / slope), (3.0 * rightSecantSlope / slope)), 1.0);
        }
    }
}

void CubicMonotonicInterpolator::resampleDichotomicAddControlPoint(CubicMonotonicInterpolator *newCurve, double xa, double xb, unsigned int level, unsigned int maxLevel, double threshold) {
    if (level >= maxLevel) {
        return;
    }

    double x = (xa + xb) / 2.0;
    double xLeft = (xa + x) / 2.0;
    double xRight = (xb + x) / 2.0;
    int idx = newCurve->addKeyframe(Vector2d(x, evalAt(x)));
    newCurve->setSlope(idx, evalDerivativeAt(x));

    double sdL = sdOnSegment(newCurve, xa, x);
    if (sdL > threshold) {
        resampleDichotomicAddControlPoint(newCurve, xa, x, level + 1, maxLevel, threshold);
    }

    double sdR = sdOnSegment(newCurve, x, xb);
    if (sdR > threshold) {
        resampleDichotomicAddControlPoint(newCurve, x, xb, level + 1, maxLevel, threshold);
    }
}

double CubicMonotonicInterpolator::meanSqErrorOnSegment(CubicMonotonicInterpolator *newCurve, double xa, double xb, unsigned int samples) {
    double step = (xb - xa) / (double)samples;
    double tot = 0.0;
    double v = 0.0;
    for (double x = xa; x <= xb; x += step) {
        v = std::abs(evalAt(x) - newCurve->evalAt(x));
        tot += v * v;
    }
    return tot / (double)samples;
}

double CubicMonotonicInterpolator::sdOnSegment(CubicMonotonicInterpolator *newCurve, double xa, double xb, unsigned int samples) {
    return sqrt(meanSqErrorOnSegment(newCurve, xa, xb, samples));
}


int HermiteInterpolator::addKeyframe(const Vector2d &pt) {
    const unsigned int nb = nbPoints();
    Vector2d t1, t2, t3, t4;
    if (nbPoints() >= 2) tangentAt(pt.x(), t1, t2, t3, t4);

    int idx = CurveInterpolator::addKeyframe(pt);
    // set smooth tangents
    if (nb != nbPoints() && nbPoints() >= 3) {
        for (unsigned int i = 0; i < nbPoints(); ++i) {
            if (_points[i] == pt) {
                if (i > 0 && i + 1 < nbPoints()) {
                    Vector4d tangent(t3.x(), t3.y(), t2.x(), t2.y());
                    _tangents.insert(_tangents.begin() + i, tangent);
                    _tangents[i-1].head<2>() = t1;
                    _tangents[i+1].tail<2>() = t4;
                } else {
                    qDebug() << "?????????";
                    Vector4d tangent(2, 0, -2, 0);
                    _tangents.insert(_tangents.begin() + i, tangent);              
                }
                break;
            }
        }
    } else if (nbPoints() <= 2) {
        for (unsigned int i = 0; i < nbPoints(); ++i) {
            if (_points[i] == pt) {
                Vector4d tangent(2, 0, -2, 0);
                _tangents.insert(_tangents.begin() + i, tangent);              
                break;
            }
        } 
    }
    return idx;
}

void HermiteInterpolator::delKeyframe(unsigned int i) {
    const unsigned int nb = nbPoints();
    CurveInterpolator::delKeyframe(i);
    if (nb != nbPoints()) _tangents.erase(_tangents.begin() + i);
}

void HermiteInterpolator::setTangent(const Vector2d &pt, unsigned int i, unsigned int side) { CurveInterpolator::setTangent(pt, i, side); }

void HermiteInterpolator::initTangents() {
    // add tangents if needed
    int nb = (int)nbPoints() - (int)_tangents.size();
    for (int i = 0; i < nb; ++i) {
        _tangents.push_back(Vector4d(0.25, 0, -0.25, 0));
    }

    // remove tangents if needed
    nb = (int)_tangents.size() - (int)nbPoints();
    for (int i = 0; i < nb; ++i) {
        _tangents.pop_back();
    }
}

void HermiteInterpolator::tangentAt(double t, unsigned int i) {
    // De Casteljau
    Vector2d pt2 = _points[i-1] + _tangents[i-1].head<2>();
    Vector2d pt3 = _points[i+1] + _tangents[i+1].tail<2>();

    Vector2d pt12 = _tangents[i-1].head<2>()*t + _points[i-1];
    Vector2d pt23 = (pt3-pt2)*t + pt2;
    Vector2d pt34 = -_tangents[i+1].tail<2>()*t + pt3;

    Vector2d pt123 = (pt23 - pt12)*t + pt12;
    Vector2d pt234 = (pt34 - pt23)*t + pt23;

    Vector2d pt1234 = (pt234 - pt123) * t + pt123;

    _tangents[i-1].head<2>() = pt12 - _points[i-1];
    _tangents[ i ].tail<2>() = pt123 - pt1234;
    _tangents[ i ].head<2>() = pt234 - pt1234;
    _tangents[i+1].tail<2>() = pt34 - _points[i+1];
}

void HermiteInterpolator::tangentAt(double x, Eigen::Vector2d &t1, Eigen::Vector2d &t2, Eigen::Vector2d &t3, Eigen::Vector2d &t4) {
    unsigned int i = 0;
    double t = findParam(x, i);
    
    Vector2d pt2 = _points[i] + _tangents[i].head<2>();
    Vector2d pt3 = _points[i+1] + _tangents[i+1].tail<2>();

    Vector2d pt12 = _tangents[i].head<2>() * t + _points[i];
    Vector2d pt23 = (pt3 - pt2) * t + pt2;
    Vector2d pt34 = -_tangents[i+1].tail<2>() * t + pt3;

    Vector2d pt123 = (pt23 - pt12) * t + pt12;
    Vector2d pt234 = (pt34 - pt23) * t + pt23;

    Vector2d pt1234 = (pt234 - pt123) * t + pt123;

    t1 = pt12 - _points[i];
    t2 = pt123 - pt1234;
    t3 = pt234 - pt1234;
    t4 = pt34 - _points[i+1];
}

void HermiteInterpolatorArcLength::updateArclengthLUT() {
    alengthLUT.resize(nbPoints());
    if (nbPoints() < 2) return;
    for (int i = 0; i < nbPoints() - 1; ++i){
        double step = (_points[i + 1][0] - _points[i][0]) / (LUT_PRECISION - 1);
        double t = 0.0;
        double s = 0.0;
        double cur, prev;

        alengthLUT[i][0][0] = 0.0;
        alengthLUT[i][1][0] = 0.0;
        alengthLUT[i][0][LUT_PRECISION - 1] = 1.0;
        alengthLUT[i][1][LUT_PRECISION - 1] = 1.0;

        prev = evalFromTandI(0., i);

        
        for (int k = 1; k < LUT_PRECISION - 1; k++) {
            t += step;
            cur = evalFromTandI(t, i);
            s += cur - prev;
            prev = cur;
            alengthLUT[i][0][k] = s;
            alengthLUT[i][1][k] = t;
        }
        cur = evalFromTandI(1.0, i);
        s += cur - prev;

        // normalize s
        for (int k = 1; k < LUT_PRECISION - 1; k++) {
            alengthLUT[i][0][k] /= s;
        }
    }
}

double HermiteInterpolatorArcLength::evalFromTandI(double t, unsigned int i){
    const Eigen::Vector2d left = _tangents[i].head<2>();
    const Eigen::Vector2d right = _tangents[i + 1].tail<2>();
    const Eigen::Vector4d by = Geom::bezierCoeffs(_points[i][1], _points[i][1] + (left[1] /* / 3.0 */), _points[i + 1][1] + (right[1]  /* / 3.0 */), _points[i + 1][1]);
    return 3 * by[0] * t * t + 2 * by[1] * t + by[2];
}

double HermiteInterpolatorArcLength::findParamArcLength(double x, unsigned int &i){
    for (i = 0; i < nbPoints() - 1; ++i){
        if (_points[i + 1][0] < x) continue;
        double s = _points[i + 1][0] - _points[i][0];
        int k = 0;
        while (alengthLUT[i][0][k] < s) ++k;
        Point::Scalar sInterp = (s - alengthLUT[i][0][k-1]) / (alengthLUT[i][0][k] - alengthLUT[i][0][k-1]);
        return alengthLUT[i][1][k-1] * (1.0 - sInterp) + alengthLUT[i][1][k] * sInterp;
    }
    return 0.f;
}

const vector<Vector2d> CurveInterpolator::samplePoints(double x1, double x2, unsigned int nb) const {
    vector<Vector2d> pts;

    const double s = fabs(x2 - x1) / (double)nb;

    for (double x = x1; x <= x2; x += s) {
        pts.push_back(Vector2d(x, evalAt(x)));
    }

    return pts;
}

const vector<Vector2d> CurveInterpolator::sampleLines(double x1, double x2, unsigned int nb) const { return samplePoints(x1, x2, nb); }

void CurveInterpolator::tangentAt(double t, unsigned int i) {
    qDebug() << "there is no implementation of tangentAt for this type of curve";
}

QStringList Curve::_interpNames = QStringList() << "Linear"
                                                << "Step"
                                                << "Shepard"
                                                << "Spline"
                                                << "Hermite"
                                                << "Spacing"
                                                << "Hermite Monotonic";

Curve::Curve(const Vector2d &pt, int interpolation) : _interpType(interpolation) { _interpolator = createInterpolator(_interpType, pt); }

Curve::Curve(const Curve &other) {
    _interpType = other._interpType;
    _interpolator = createInterpolator(_interpType, other._interpolator);
}

Curve::~Curve() { delete _interpolator; }

void Curve::setInterpolation(int interpolation) {
    if (interpolation == interpType()) return;

    if (CurveInterpolator *ci = createInterpolator(interpolation, _interpolator)) {
        delete _interpolator;
        _interpolator = ci;
        _interpType = interpolation;
    }
}

CurveInterpolator *Curve::createInterpolator(int interpolation, const Vector2d &pt) {
    switch (interpolation) {
        case LINEAR_INTERP:
            return new LinearInterpolator(pt);
            break;
        case STEP_INTERP:
            return new StepInterpolator(pt);
            break;
        case SHEPARD_INTERP:
            return new ShepardInterpolator(pt);
            break;
        case SPLINE_INTERP:
            return new SplineInterpolator(pt);
            break;
        case HERMITE_INTERP:
            return new HermiteInterpolator(pt);
            break;
        case HERMITE_ARC_LENGTH_INTERP:
            return new HermiteInterpolatorArcLength(pt);
            break;
        case CUBIC_INTERP:
            return new CubicPolynomialInterpolator(pt);
            break;
        case MONOTONIC_CUBIC_INTERP:
            return new CubicMonotonicInterpolator(pt);
            break;
        default:
            return new LinearInterpolator(pt);
            break;
    }
}

CurveInterpolator *Curve::createInterpolator(int interpolation, CurveInterpolator *curve) {
    if (!curve) return NULL;

    switch (interpolation) {
        case LINEAR_INTERP:
            return new LinearInterpolator(curve);
            break;
        case STEP_INTERP:
            return new StepInterpolator(curve);
            break;
        case SHEPARD_INTERP:
            return new ShepardInterpolator(curve);
            break;
        case SPLINE_INTERP:
            return new SplineInterpolator(curve);
            break;
        case HERMITE_INTERP:
            return new HermiteInterpolator(curve);
            break;
        case HERMITE_ARC_LENGTH_INTERP:
            return new HermiteInterpolatorArcLength(curve);
            break;
        case CUBIC_INTERP:
            return new CubicPolynomialInterpolator(curve);
            break;
        case MONOTONIC_CUBIC_INTERP:
            return new CubicMonotonicInterpolator(curve);
            break;
        default:
            return new LinearInterpolator(curve);
            break;
    }
}

// TODO should fill an array passed as reference and return nothing
const vector<Vector2d> Curve::samplePoints(double x1, double x2, unsigned int nb) const {
    vector<Vector2d> pts;

    const double s = fabs(x2 - x1) / (double)nb;

    for (double x = x1; x <= x2; x += s) {
        pts.push_back(Vector2d(x, evalAt(x)));
    }

    return pts;
}

// TODO see above
const vector<Vector2d> Curve::sampleLines(double x1, double x2, unsigned int nb) const { return samplePoints(x1, x2, nb); }

Curve *Curve::cut(int i, int j, bool resetXBoundaries) {
    assert(i < nbPoints());
    assert(j < nbPoints());
    if (j < i) std::swap(i, j);
    int idx = 0;
    Curve *cut = new Curve(point(i), interpType());
    cut->setTangent(tangent(i), idx);
    for (int k = i + 1; k <= j; k++) {
        idx++;
        cut->addKeyframe(point(k));
        cut->setTangent(tangent(k), idx);
    }

    // copy slope values if we're cutting a monotic piecewise cubic spline 
    if (_interpType == MONOTONIC_CUBIC_INTERP) {
        for (int k = i; k <= j; ++k) {
            dynamic_cast<CubicMonotonicInterpolator *>(cut->interpolator())->setSlope(k-i, dynamic_cast<CubicMonotonicInterpolator *>(_interpolator)->slopeAt(k));
        }
    }

    if (resetXBoundaries) {
        cut->normalizeX();
    }

    return cut;
}

void Curve::setPiecewiseLinear() {
    if (_interpType != HERMITE_INTERP && _interpType != HERMITE_ARC_LENGTH_INTERP) return;

    for (size_t i = 0; i < nbPoints() - 1; i++) {
        Eigen::Vector2d p0 = point(i);
        Eigen::Vector2d p1 = point(i + 1);
        Eigen::Vector2d t1 = p1 - p0;
        t1 = 0.4 * t1;
        setTangent(t1, i, 0);
        setTangent(-t1, i + 1, 1);
        if (i == 0) setTangent(Eigen::Vector2d::Zero(), i, 1);
        if (i + 1 == nbPoints() - 1) setTangent(Eigen::Vector2d::Zero(), i + 1, 0);
    }
}

QRectF Curve::getBoundingBox() {
    Eigen::AlignedBox2d BB;
    for (size_t i = 0; i < nbPoints(); ++i) BB.extend(point(i));
    QRectF box(BB.min()[0], BB.min()[1], std::max(0.1, BB.max()[0] - BB.min()[0]), std::max(0.1, BB.max()[1] - BB.min()[1]));
    return box.marginsAdded(QMarginsF(box.width() * 0.1, box.height() * 0.1, box.width() * 0.1, box.height() * 0.1));
}

void Curve::print(std::ostream &os) const {
    os << "**** Interp type: " << _interpNames[_interpType].toStdString();
    os << "**** Points:\n";
    for (size_t i = 0; i < nbPoints(); i++) os << point(i).transpose() << " ||\n";
    os << "**** Tangents:\n";
    for (size_t i = 0; i < nbTangents(); i++) os << tangent(i).transpose() << " ||\n";
    os << std::endl;
}
