/*
 * SPDX-FileCopyrightText: 2013 Romain Vergne <romain.vergne@inria.fr>
 * SPDX-FileCopyrightText: 2020-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
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

CurveInterpolator::CurveInterpolator(const Vector2f &pt) { _points.push_back(pt); }

CurveInterpolator::CurveInterpolator(CurveInterpolator *curve) {
    if (!curve) return;

    _points = std::vector<Vector2f>(curve->nbPoints());
    _tangents = std::vector<Vector4f>(curve->nbTangents());

    for (unsigned int i = 0; i < curve->nbPoints(); ++i) {
        _points[i] = curve->point(i);
    }

    for (unsigned int i = 0; i < curve->nbTangents(); ++i) {
        _tangents[i] = curve->tangent(i);
    }
}

int CurveInterpolator::addKeyframe(const Vector2f &pt) {
    vector<Vector2f>::iterator it = _points.begin();
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

void CurveInterpolator::setKeyframe(const Vector2f &pt, unsigned int i) {
    assert(i < nbPoints());

    // points should be kept ordered
    if ((i > 0 && pt[0] < _points[i - 1][0]) || (i < nbPoints() - 1 && pt[0] > _points[i + 1][0])) return;

    _points[i] = pt;
}

void CurveInterpolator::setTangent(const Vector2f &pt, unsigned int i, unsigned int side) {
    if (i >= _tangents.size()) return;
    _tangents[i][2 * side] = pt[0];
    _tangents[i][2 * side + 1] = pt[1];
}

void CurveInterpolator::setTangent(const Vector4f &pt, unsigned int i) {
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
        setKeyframe(Vector2f(_points[0][0] + offsetFirst, _points[0][1]), 0);
    } else {
        for (int i = 0; i < nbPts; ++i) {
            float alpha = (float)i / (float)(nbPts - 1);
            float key = (1.f - alpha) * (_points[i][0] + offsetFirst) + alpha * (_points[i][0] + offsetLast);
            setKeyframe(Vector2f(key, _points[i][1]), i);
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

float CurveInterpolator::normalizeX() {
    if (nbPoints() < 2) {
        std::cerr << "Error: cannot normalize curve with less than two control points!" << std::endl;
        return 0.0f;
    }

    float x0 = _points[0][0];
    float xN = _points.back()[0];
    float ratio = 1.0f / (xN - x0);

    if (x0 < 1e-5) x0 = 0.0f;

    // scale x-component of positions
    for (unsigned int i = 1; i < nbPoints() - 1; ++i) {
        _points[i][0] = std::min(1.0f, ratio * (_points[i][0] - x0));
    }
    
    // scale x-component of tangents
    if (useTangents()) {
        for (unsigned int i = 0; i < nbTangents(); ++i) {
            _tangents[i][0] = _tangents[i][0] * ratio;     
            _tangents[i][2] = _tangents[i][2] * ratio;     
        }
    }

    // clamp these one for precision
    _points.back()[0] = 1.0f;
    _points.front()[0] = 0.0f;
    return ratio;
}

void CurveInterpolator::smoothTangents() {
    if (nbPoints() <= 1) return;
    _tangents.clear();
    Eigen::Vector2f d;
    d = (_points[1] - _points[0]) * 0.5f;
    _tangents.push_back(Eigen::Vector4f(d.x(), d.y(), -d.x(), -d.y()));
    for (int i = 1; i < nbPoints() - 1; ++i) {
        Eigen::Vector2f d1 = _points[i + 1] - _points[i];
        Eigen::Vector2f d2 = _points[i] - _points[i - 1];
        d = (d1 + d2) * 0.25f;
        _tangents.push_back(Eigen::Vector4f(d.x(), d.y(), -d.x(), -d.y()));
    }
    d = (_points[_points.size() - 1] - _points[_points.size() - 2]) * 0.5f;
    _tangents.push_back(Eigen::Vector4f(d.x(), d.y(), -d.x(), -d.y()));
}

void CurveInterpolator::scaleTangentVertical(float factor) {
    for (unsigned int i = 0; i < nbTangents(); i++) {
        _tangents[i][1] *= factor;
        _tangents[i][3] *= factor;
    }
}

int SplineInterpolator::addKeyframe(const Vector2f &pt) {
    const unsigned int nb = nbPoints();
    int idx = CurveInterpolator::addKeyframe(pt);
    if (nb != nbPoints()) {
        nbPointsChanged();
    }
    computeSolution();
    return idx;
}

void SplineInterpolator::setKeyframe(const Vector2f &pt, unsigned int i) {
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
    std::vector<Vector2f> sampledPoints(n + 2);
    int nb = _points.size();
    for (unsigned int i = 1; i <= n; ++i) {
        float x = (float)(i)/(float)(n+1);
        sampledPoints[i] = Vector2f(x, evalAt(x));
    }

    sampledPoints.front() = Vector2f::Zero();
    sampledPoints.back() = Vector2f(1.0f, 0.0f);

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
        _A(i, j) = 1.0f;                             // 1
        _A(i, j + 1) = _points[p][0];                // x
        _A(i, j + 2) = _A(i, j + 1) * _A(i, j + 1);  // x^2
        _A(i, j + 3) = _A(i, j + 2) * _A(i, j + 1);  // x^3
        _b(i) = _points[p][1];                       // y

        // f(xi+1) = yi+1
        i++;
        p++;
        _A(i, j) = 1.0f;                             // 1
        _A(i, j + 1) = _points[p][0];                // x
        _A(i, j + 2) = _A(i, j + 1) * _A(i, j + 1);  // x^2
        _A(i, j + 3) = _A(i, j + 2) * _A(i, j + 1);  // x^3
        _b(i) = _points[p][1];                       // y

        // check if we have to stop
        if (s == np - 2) break;

        // fi'(x)=fi+1'(x)
        i++;
        _A(i, j + 1) = 1.0f;                     // 1
        _A(i, j + 2) = 2.0f * _points[p][0];     // 2x
        _A(i, j + 3) = 3.0f * _A(i - 1, j + 2);  // 3x^2
        _A(i, j + 5) = -_A(i, j + 1);            // -1
        _A(i, j + 6) = -_A(i, j + 2);            // -2x
        _A(i, j + 7) = -_A(i, j + 3);            // -3x^2

        // fi''(x)=fi+1''(x)
        i++;
        _A(i, j + 2) = 2.0f;                  // 2
        _A(i, j + 3) = 6.0f * _points[p][0];  // 6x
        _A(i, j + 6) = -_A(i, j + 2);         // -2
        _A(i, j + 7) = -_A(i, j + 3);         // -6x

        j += 4;
    }

    // boundary conditions (natural spline)
    _A(0, 2) = 2.0f;
    _A(0, 3) = 6.0f * _points[0][0];
    _A(ms - 1, ms - 2) = 2.0f;
    _A(ms - 1, ms - 1) = 6.0f * _points[np - 1][0];

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

CubicPolynomialInterpolator::CubicPolynomialInterpolator(const Eigen::Vector2f &pt) : CurveInterpolator(pt) {
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

int CubicPolynomialInterpolator::addKeyframe(const Vector2f &pt) {
    int idx = CurveInterpolator::addKeyframe(pt);
    if (nbPoints() == 5) {
        for (int i = 1; i < 4; ++i) {
            _prevControlPointsY[i-1] = _points[i].y();
        }
    }
    computeSolution();
    return idx;
}

void CubicPolynomialInterpolator::setKeyframe(const Vector2f &pt, unsigned int i) {
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

void CubicPolynomialInterpolator::setControlPoints(float p1, float p2, float p3, int fixedPointIdx) {
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

CubicPolynomialInterpolator *CubicPolynomialInterpolator::splitAt(float x) {
    float al, bl, cl, ar, br, cr;
    float y = evalAt(x);
    float yComp = 1.0f - y;
    float xComp = 1.0f - x;
    float xx = x*x;
    float xxx = xx * x;

    // compute coefficients of the right half
    ar = _x[2] * xComp * xComp * xComp / yComp;
    br = (3.0f * _x[2] * x + _x[1]) * xComp * xComp / yComp;
    cr = (3.0f * _x[2] * xx + 2.0f * _x[1] * x + _x[0]) * xComp / yComp;
    CubicPolynomialInterpolator *cubicSecondHalf = new CubicPolynomialInterpolator(Eigen::Vector2f::Zero());
    cubicSecondHalf->_points.resize(5);
    cubicSecondHalf->_points[0] = Eigen::Vector2f::Zero();
    cubicSecondHalf->_points[1].x() = 0.25f;
    cubicSecondHalf->_points[2].x() = 0.50f;
    cubicSecondHalf->_points[3].x() = 0.75f;
    cubicSecondHalf->_points[4] = Eigen::Vector2f::Ones();
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

    float xs[3] = {0.25f, 0.50f, 0.75f};
    computeSolutionAux(xs[0], xs[1], xs[2]);
 
    // check if the cubic polynomial is still strictly monotonic
    // which in this case boils down to checking if the derivative
    // has any real root in the interval [0,1]
    auto isMonotonic = [&]() {
        float delta = 4.0f * _x[1] * _x[1] - 12.0f * _x[2] * _x[0];
        if (delta  < 0) return true;
        float sqDelta = std::sqrt(delta);
        float x1 = ((-2.0f) * _x[1] - sqDelta) / (6 * _x[2]);
        float x2 = ((-2.0f) * _x[1] + sqDelta) / (6 * _x[2]);
        if ((x1 > 0.0f && x1 < 1.0f) || (x2 > 0.0f && x2 < 1.0f)) {
            return false;
        }
        return true;
    };

    int attempt = 0;
    while (!isMonotonic() && attempt < 20) {
        for (int i = 1; i < 4; ++i) {
            if (i == _constraintIdx) continue;
            // float delta = _points[i].y() - _prevControlPointsY[i-1];
            // if (delta > 0) xs[i-1] -= 0.01f;
            // else           xs[i-1] += 0.01f;
            _points[i].y() = (_points[i].y() - _prevControlPointsY[i-1]) / 2.0f + _prevControlPointsY[i-1];
        }
        computeSolutionAux(xs[0], xs[1], xs[2]);
        attempt++;
    }
    // qDebug() << "finished at attempt " << attempt;

    if (!isMonotonic()) {
        _x = _xPrev;
    }
}

void CubicPolynomialInterpolator::computeSolutionAux(float x1, float x2, float x3) {
    if (nbPoints() < 5) return;

    MatrixXf L = MatrixXf::Zero(5, 3);
    VectorXf b = VectorXf::Zero(5);
    float xs[3] = {x1, x2, x3};

    // least squares
    int i;
    for (i = 0; i < 5; ++i) {
        if (i >= 1 && i <=3) {
            L(i, 0) = xs[i-1];        // x
        } else {
            L(i, 0) = i / 4.0f;       // x
        }
        L(i, 1) = L(i, 0) * L(i, 0);  // x^2
        L(i, 2) = L(i, 1) * L(i, 0);  // x^3
    }
    b(0) = 0.0f;
    b(1) = _points[1][1];
    b(2) = _points[2][1]; 
    b(3) = _points[3][1];
    b(4) = 1.0f;            

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
        _A(4, j) = 1.0f;  // C (x, x^2, x^3)
        _A(j, 4) = 1.0f;  // C^T
    }
    _b(4) = 1.0f;  // y

    // zeros
    for (int i = 3; i < 5; ++i) {
        for (int j = 3; j < 5; ++j) {
            _A(i, j) = 0.0f;
        }
    }

    _x = _A.ldlt().solve(_b);
}

void CubicPolynomialInterpolator::resampleControlPoints() {
    for (int i = 1; i < 4; ++i) {
        _points[i][1] = evalAt(i / 4.0f);
    }
}

void CubicPolynomialInterpolator::setCoeffs(float a, float b, float c) {
    _x.resize(5);
    _x[4] = 1.0f;
    _x[3] = 1.0f;
    _x[2] = a;
    _x[1] = b;
    _x[0] = c;
}

CubicMonotonicInterpolator::CubicMonotonicInterpolator(const Eigen::Vector2f &pt) : CurveInterpolator(pt) {
    if (nbPoints() == 1 && _points[0].x() == 1.0f) _points[0] = Vector2f::Ones();
    makeSlopes();
}

CubicMonotonicInterpolator::CubicMonotonicInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) {
    if (nbPoints() == 1 && _points[0].x() == 1.0f) _points[0] = Vector2f::Ones();
    makeSlopes();
}

int CubicMonotonicInterpolator::addKeyframe(const Vector2f &pt) {
    float slope = (pt.x() >= _points.front().x() && pt.x() <= _points.back().x()) ? evalDerivativeAt(pt.x()) : 0.0f;
    unsigned int sizeBefore = nbPoints();
    int idx = CurveInterpolator::addKeyframe(pt);
    if (idx == _slopes.size() || _slopes.empty()) _slopes.push_back(0.0f);
    else if (sizeBefore != nbPoints()) _slopes.insert(_slopes.begin() + idx, slope); 
    if (nbPoints() == 2) {
        makeSlopes();
    } else if (idx == 0 || idx == _slopes.size() - 1) {
        updateSlope(idx, false);
    } 
    return idx;
}

void CubicMonotonicInterpolator::setKeyframe(const Eigen::Vector2f &pt, unsigned int i) {
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

float CubicMonotonicInterpolator::normalizeX() {
    float ratioX = CurveInterpolator::normalizeX();
    if (nbPoints() < 2) return 0.0f;
    // also normalize Y in this case (spacing curve)
    float y0 = _points[0].y();
    float yN = _points.back().y();
    float ratio = 1.0f / (yN - y0);

    if (y0 < 1e-5) y0 = 0.0f;

    // scale y-component control points
    for (unsigned int i = 1; i < nbPoints() - 1; ++i) {
        _points[i].y() = std::min(1.0f, ratio * (_points[i].y() - y0));
    }
    
    // scale slopes
    float slopeScaling = ratio / ratioX;
    for (unsigned int i = 0; i < nbPoints(); ++i) {
        _slopes[i] *= slopeScaling;
    }

    // clamp these one for precision
    _points.back().y() = 1.0f;
    _points.front().y() = 0.0f;
    return ratioX;
}

void CubicMonotonicInterpolator::makeSlopes() {
    if (nbPoints() < 2) return;
    unsigned int nb = nbPoints();

    // compute sucessive secant slopes
    std::vector<float> secantSlopes(nb - 1);
    for (unsigned int i = 0; i < nb - 1; ++i) {
        secantSlopes[i] = (_points[i+1].y() - _points[i].y()) / (_points[i+1].x() - _points[i].x());
    }

    // init slopes
    _slopes.resize(nb);
    _slopes[0] = secantSlopes[0];
    _slopes[nb - 1] = secantSlopes[nb - 2];
    for (unsigned int i = 1; i < nb - 1; i++) {
        _slopes[i] = (secantSlopes[i-1] + secantSlopes[i]) * 0.5f;
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
    float dt = _points[1].x() - _points[0].x();
    for (int i = 0; i < nb; ++i) {
        _slopes[i] = slopes(i) / dt;
    }

    // check and adjust slopes for monotonicity
    std::vector<float> secantSlopes(nb - 1);
    _slopes[0] = std::max(0.0f, _slopes[0]);
    _slopes.back() = std::max(0.0f, _slopes.back());
    for (unsigned int i = 0; i < nb - 1; ++i) secantSlopes[i] = (_points[i+1].y() - _points[i].y()) / (_points[i+1].x() - _points[i].x());
    for (unsigned int i = 1; i < nb - 1; ++i) checkSlopeMonotonicity(i, _slopes[i], i == 0 ? secantSlopes[0] : secantSlopes[i-1], i == nb-1 ? secantSlopes.back() : secantSlopes[i]);
}


void CubicMonotonicInterpolator::resample(unsigned int n) {
    std::vector<Vector2f> sampledPoints(n + 2);
    std::vector<float> sampledSlopes(n + 2);

    for (unsigned int i = 1; i <= n; ++i) {
        float x = (float)(i)/(float)(n+1);
        sampledPoints[i] = Vector2f(x, std::max(std::min(evalAt(x), 1.0f), 0.0f));
        sampledSlopes[i] = evalDerivativeAt(x);
    }

    sampledPoints.front() = Vector2f::Zero();
    sampledPoints.back() = Vector2f::Ones();
    sampledSlopes.front() = _slopes.front();
    sampledSlopes.back() = _slopes.back();

    // if (resetSlopes) {
        _slopes = sampledSlopes;
    // }
    _points = sampledPoints;
}

void CubicMonotonicInterpolator::resampleDichotomic(unsigned int maxControlPoints, unsigned int nbFrames) {
    CubicMonotonicInterpolator newCurve(Vector2f::Zero());

    newCurve._points.clear();
    newCurve._points.push_back(_points.front());
    newCurve._points.push_back(_points.back());
    newCurve._slopes.push_back(_slopes.front());
    newCurve._slopes.push_back(_slopes.back());

    float xa = newCurve._points.front().x();
    float xb = newCurve._points.back().x();
    float threshold = std::min(1.0f / nbFrames, 0.1f);
    
    resampleDichotomicAddControlPoint(&newCurve, xa, xb, 0, 3, 0.003f);

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

float CubicMonotonicInterpolator::smallestXInterval() const {
    float minInterval = 1.0f;
    for (int i = 1; i < nbPoints(); ++i) {
        float interval = _points[i].x() - _points[i - 1].x(); 
        if (interval < minInterval) minInterval = interval;
    }
    return minInterval;
}

void CubicMonotonicInterpolator::smoothTangents() {
    if (nbSlopes() < 3) return;
    std::vector<float> slopesCopy = _slopes;
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
        if (std::abs(_points[0].y() - _points[1].y()) < 1e-5) _slopes[0] = 0.0f;
    } else if (i == nbPoints() - 1) {
        _slopes[nbPoints() - 1] = (_points[nbPoints() - 1].y() - _points[nbPoints() - 2].y()) / (_points[nbPoints() - 1].x() - _points[nbPoints() - 2].x());
    } else {
        float leftSecantSlope = (_points[i].y() - _points[i-1].y()) / (_points[i].x() - _points[i-1].x());
        float rightSecantSlope = (_points[i+1].y() - _points[i].y()) / (_points[i+1].x() - _points[i].x());
        _slopes[i] = (leftSecantSlope + rightSecantSlope) * 0.5f;
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

void CubicMonotonicInterpolator::checkSlopeMonotonicity(unsigned int i, float slope, float leftSecantSlope, float rightSecantSlope) {
    if (i < nbPoints() - 1 && std::abs(_points[i+1].y() - _points[i].y()) < 1e-5) { // connect points at the same y with a straight line
        _slopes[i] = 0.0f;
        _slopes[i + 1] = 0.0f;
    } else {
        if (Geom::sgn(leftSecantSlope) != Geom::sgn(rightSecantSlope) || _slopes[i] < 0.0f) { // inflexion point => set the slope to 0
            _slopes[i] = 0.0f;
        } else { // bound slope magnitude by 3 times the left or right secant slope (whatever is min)
            _slopes[i] *= std::min(std::min((3.0f * leftSecantSlope / slope), (3.0f * rightSecantSlope / slope)), 1.0f);
        }
    }
}

void CubicMonotonicInterpolator::resampleDichotomicAddControlPoint(CubicMonotonicInterpolator *newCurve, float xa, float xb, unsigned int level, unsigned int maxLevel, float threshold) {
    if (level >= maxLevel) {
        return;
    }

    float x = (xa + xb) / 2.0f;
    float xLeft = (xa + x) / 2.0f;
    float xRight = (xb + x) / 2.0f;
    int idx = newCurve->addKeyframe(Vector2f(x, evalAt(x)));
    newCurve->setSlope(idx, evalDerivativeAt(x));

    float sdL = sdOnSegment(newCurve, xa, x);
    if (sdL > threshold) {
        resampleDichotomicAddControlPoint(newCurve, xa, x, level + 1, maxLevel, threshold);
    }

    float sdR = sdOnSegment(newCurve, x, xb);
    if (sdR > threshold) {
        resampleDichotomicAddControlPoint(newCurve, x, xb, level + 1, maxLevel, threshold);
    }
}

float CubicMonotonicInterpolator::meanSqErrorOnSegment(CubicMonotonicInterpolator *newCurve, float xa, float xb, unsigned int samples) {
    float step = (xb - xa) / (float)samples;
    float tot = 0.0f;
    float v = 0.0f;
    for (float x = xa; x <= xb; x += step) {
        v = std::abs(evalAt(x) - newCurve->evalAt(x));
        tot += v * v;
    }
    return tot / (float)samples;
}

float CubicMonotonicInterpolator::sdOnSegment(CubicMonotonicInterpolator *newCurve, float xa, float xb, unsigned int samples) {
    return sqrt(meanSqErrorOnSegment(newCurve, xa, xb, samples));
}

int HermiteInterpolator::addKeyframe(const Vector2f &pt) {
    const unsigned int nb = nbPoints();
    Vector2f t1, t2, t3, t4;
    if (nbPoints() >= 2) tangentAt(pt.x(), t1, t2, t3, t4);

    int idx = CurveInterpolator::addKeyframe(pt);
    // set smooth tangents
    if (nb != nbPoints() && nbPoints() >= 3) {
        for (unsigned int i = 0; i < nbPoints(); ++i) {
            if (_points[i] == pt) {
                if (i > 0 && i + 1 < nbPoints()) {
                    Vector4f tangent(t3.x(), t3.y(), t2.x(), t2.y());
                    _tangents.insert(_tangents.begin() + i, tangent);
                    _tangents[i-1].head<2>() = t1;
                    _tangents[i+1].tail<2>() = t4;
                } else {
                    qDebug() << "?????????";
                    Vector4f tangent(2, 0, -2, 0);
                    _tangents.insert(_tangents.begin() + i, tangent);              
                }
                break;
            }
        }
    } else if (nbPoints() <= 2) {
        for (unsigned int i = 0; i < nbPoints(); ++i) {
            if (_points[i] == pt) {
                Vector4f tangent(2, 0, -2, 0);
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

void HermiteInterpolator::setTangent(const Vector2f &pt, unsigned int i, unsigned int side) { CurveInterpolator::setTangent(pt, i, side); }

void HermiteInterpolator::initTangents() {
    // add tangents if needed
    int nb = (int)nbPoints() - (int)_tangents.size();
    for (int i = 0; i < nb; ++i) {
        _tangents.push_back(Vector4f(0.25, 0, -0.25, 0));
    }

    // remove tangents if needed
    nb = (int)_tangents.size() - (int)nbPoints();
    for (int i = 0; i < nb; ++i) {
        _tangents.pop_back();
    }
}

void HermiteInterpolator::tangentAt(float t, unsigned int i) {
    // De Casteljau
    Vector2f pt2 = _points[i-1] + _tangents[i-1].head<2>();
    Vector2f pt3 = _points[i+1] + _tangents[i+1].tail<2>();

    Vector2f pt12 = _tangents[i-1].head<2>()*t + _points[i-1];
    Vector2f pt23 = (pt3-pt2)*t + pt2;
    Vector2f pt34 = -_tangents[i+1].tail<2>()*t + pt3;

    Vector2f pt123 = (pt23 - pt12)*t + pt12;
    Vector2f pt234 = (pt34 - pt23)*t + pt23;

    Vector2f pt1234 = (pt234 - pt123) * t + pt123;

    _tangents[i-1].head<2>() = pt12 - _points[i-1];
    _tangents[ i ].tail<2>() = pt123 - pt1234;
    _tangents[ i ].head<2>() = pt234 - pt1234;
    _tangents[i+1].tail<2>() = pt34 - _points[i+1];
}

void HermiteInterpolator::tangentAt(float x, Eigen::Vector2f &t1, Eigen::Vector2f &t2, Eigen::Vector2f &t3, Eigen::Vector2f &t4) {
    unsigned int i = 0;
    float t = findParam(x, i);
    
    Vector2f pt2 = _points[i] + _tangents[i].head<2>();
    Vector2f pt3 = _points[i+1] + _tangents[i+1].tail<2>();

    Vector2f pt12 = _tangents[i].head<2>() * t + _points[i];
    Vector2f pt23 = (pt3 - pt2) * t + pt2;
    Vector2f pt34 = -_tangents[i+1].tail<2>() * t + pt3;

    Vector2f pt123 = (pt23 - pt12) * t + pt12;
    Vector2f pt234 = (pt34 - pt23) * t + pt23;

    Vector2f pt1234 = (pt234 - pt123) * t + pt123;

    t1 = pt12 - _points[i];
    t2 = pt123 - pt1234;
    t3 = pt234 - pt1234;
    t4 = pt34 - _points[i+1];
}

const vector<Vector2f> CurveInterpolator::samplePoints(float x1, float x2, unsigned int nb) const {
    vector<Vector2f> pts;

    const float s = fabs(x2 - x1) / (float)nb;

    for (float x = x1; x <= x2; x += s) {
        pts.push_back(Vector2f(x, evalAt(x)));
    }

    return pts;
}

const vector<Vector2f> CurveInterpolator::sampleLines(float x1, float x2, unsigned int nb) const { return samplePoints(x1, x2, nb); }

void CurveInterpolator::tangentAt(float t, unsigned int i) {
    qDebug() << "there is no implementation of tangentAt for this type of curve";
}

QStringList Curve::_interpNames = QStringList() << "Linear"
                                                << "Step"
                                                << "Shepard"
                                                << "Spline"
                                                << "Hermite"
                                                << "Spacing"
                                                << "Hermite Monotonic";

Curve::Curve(const Vector2f &pt, int interpolation) : _interpType(interpolation) { _interpolator = createInterpolator(_interpType, pt); }

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

CurveInterpolator *Curve::createInterpolator(int interpolation, const Vector2f &pt) {
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
const vector<Vector2f> Curve::samplePoints(float x1, float x2, unsigned int nb) const {
    vector<Vector2f> pts;

    const float s = fabs(x2 - x1) / (float)nb;

    for (float x = x1; x <= x2; x += s) {
        pts.push_back(Vector2f(x, evalAt(x)));
    }

    return pts;
}

// TODO see above
const vector<Vector2f> Curve::sampleLines(float x1, float x2, unsigned int nb) const { return samplePoints(x1, x2, nb); }

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
    if (_interpType != HERMITE_INTERP) return;

    for (size_t i = 0; i < nbPoints() - 1; i++) {
        Eigen::Vector2f p0 = point(i);
        Eigen::Vector2f p1 = point(i + 1);
        Eigen::Vector2f t1 = p1 - p0;
        t1 = 0.4f * t1;
        setTangent(t1, i, 0);
        setTangent(-t1, i + 1, 1);
        if (i == 0) setTangent(Eigen::Vector2f::Zero(), i, 1);
        if (i + 1 == nbPoints() - 1) setTangent(Eigen::Vector2f::Zero(), i + 1, 0);
    }
}

QRectF Curve::getBoundingBox() {
    Eigen::AlignedBox2f BB;
    for (size_t i = 0; i < nbPoints(); ++i) BB.extend(point(i));
    QRectF box(BB.min()[0], BB.min()[1], std::max(0.1f, BB.max()[0] - BB.min()[0]), std::max(0.1f, BB.max()[1] - BB.min()[1]));
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
