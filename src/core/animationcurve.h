/*
 * SPDX-FileCopyrightText: 2013 Romain Vergne <romain.vergne@inria.fr>
 * SPDX-FileCopyrightText: 2020-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef CURVE
#define CURVE

#if (_MSC_VER >= 1500)
#define _USE_MATH_DEFINES
#include <math.h>
#endif

#include <QTransform>
#include <Eigen/Dense>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <assert.h>
#include <iostream>
#include <vector>
#include <iomanip>

#include "utils/utils.h"
#include "utils/geom.h"

#define LUT_PRECISION 50

class Curve;

// Generic curve interpolator
class CurveInterpolator {
   public:
    CurveInterpolator(const Eigen::Vector2d &pt = Eigen::Vector2d(0, 0));
    CurveInterpolator(CurveInterpolator *curve);
    virtual ~CurveInterpolator() {
        _points.clear();
        _tangents.clear();
    }

    // evaluate
    virtual double evalAt(double x) const = 0;
    virtual double evalDerivativeAt(double x) const = 0;
    virtual double evalInverse(double y) const; // only for monotonic interpolators

    // keyframing
    virtual int addKeyframe(const Eigen::Vector2d &pt);
    virtual void setKeyframe(const Eigen::Vector2d &pt, unsigned int i = 0);
    virtual void delKeyframe(unsigned int i);
    virtual void setTangent(const Eigen::Vector2d &pt, unsigned int i = 0, unsigned int side = 0);
    virtual void setTangent(const Eigen::Vector4d &pt, unsigned int i = 0);
    virtual void moveKeys(int offsetFirst, int offsetLast);
    virtual void removeKeyframeBefore(int frame);
    virtual void removeKeyframeAfter(int frame);
    virtual void removeKeys();
    virtual double normalizeX();
    virtual void smoothTangents();
    virtual void resample(unsigned int n) { std::cout << "not implemented" << std::endl; }
    
    inline virtual void removeLastPoint() {
        if (!_points.empty()) {
            _points.resize(_points.size() - 1);
            if (useTangents()) _tangents.resize(_tangents.size() - 1);
            if (_points.size() >= 2) normalizeX();
        }
    }


    // return points and lines that describe the curve
    virtual const std::vector<Eigen::Vector2d> samplePoints(double x1, double x2, unsigned int nb = 100) const;
    virtual const std::vector<Eigen::Vector2d> sampleLines(double x1, double x2, unsigned int nb = 100) const;
    virtual void tangentAt(double t, unsigned int i);
    virtual void scaleTangentVertical(double factor);

    // accessors
    inline unsigned int nbPoints() const { return _points.size(); }
    inline unsigned int nbTangents() const { return _tangents.size(); }
    inline const std::vector<Eigen::Vector2d> &points() const { return _points; }
    inline const std::vector<Eigen::Vector4d> &tangents() const { return _tangents; }
    inline const Eigen::Vector2d &point(unsigned int i = 0) const {
        assert(i < nbPoints());
        return _points[i];
    }
    inline const Eigen::Vector4d &tangent(unsigned int i = 0) const {
        assert(i < nbTangents());
        return _tangents[i];
    }

    inline virtual bool useTangents() const { return false; }

   protected:
    std::vector<Eigen::Vector2d> _points;
    std::vector<Eigen::Vector4d> _tangents;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

// Linear interpolation
class LinearInterpolator : public CurveInterpolator {
   public:
    LinearInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt) {}
    LinearInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) {}

    inline double evalAt(double x) const {
        assert(!_points.empty());
        for (unsigned int i = 0; i < nbPoints() - 1; ++i) {
            if (_points[i + 1][0] >= x) {
                return _points[i][1] + (_points[i + 1][1] - _points[i][1]) * ((x - _points[i][0]) / (_points[i + 1][0] - _points[i][0]));
            }
        }
        return _points[0][1];
    }

    inline double evalDerivativeAt(double x) const {
        assert(!_points.empty());
        for (unsigned int i = 0; i < nbPoints() - 1; ++i) {
            if (_points[i + 1][0] >= x) {
                return (_points[i+1][1] - _points[i][1]) / (_points[i+1][0] - _points[i][0]);
            }
        }
        return 0;
    }

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

// Step interpolation
class StepInterpolator : public CurveInterpolator {
   public:
    StepInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt) {}
    StepInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) {}

    inline double evalAt(double x) const {
        assert(!_points.empty());
        for (unsigned int i = 0; i < nbPoints() - 1; ++i) {
            if (_points[i + 1][0] >= x) {
                return _points[i][1];
            }
        }
        return _points[0][1];
    }

    inline double evalDerivativeAt(double x) const {
        assert(!_points.empty());
        return 0;
    }

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

// Shepard interpolation
class ShepardInterpolator : public CurveInterpolator {
   public:
    ShepardInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt), _p(2.0), _eps(1.0e-10) {}
    ShepardInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve), _p(2.0), _eps(1.0e-10) {}

    inline double evalAt(double x) const {
        assert(!_points.empty());

        double r = 0.0;
        for (unsigned int i = 0; i < nbPoints(); ++i) {
            double d = 0.0;
            for (unsigned int j = 0; j < nbPoints(); ++j) {
                d += w(x, _points[j][0]);
            }
            r += (w(x, _points[i][0]) * _points[i][1]) / std::max(d, _eps);
        }

        return (double)r;
    }


    inline double evalDerivativeAt(double x) const {
        assert(!_points.empty());
        double v1 = evalAt(x);
        if (x == _points.back()[0]) {
            double v2 = evalAt(x - _eps);
            return (v1 - v2) / _eps;
        } else {
            double v2 = evalAt(x + _eps);
            return (v2 - v1) / _eps;
        }
    }

   private:
    inline double w(double x, double xi) const { return pow(std::max((double)fabs(xi - x), _eps), -_p); }
    const double _p;
    const double _eps;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

// Spline interpolation
class SplineInterpolator : public CurveInterpolator {
   public:
    SplineInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt) {
        nbPointsChanged();
        computeSolution();
    }
    SplineInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) {
        nbPointsChanged();
        computeSolution();
    }

    virtual int addKeyframe(const Eigen::Vector2d &pt);
    virtual void setKeyframe(const Eigen::Vector2d &pt, unsigned int i);
    virtual void delKeyframe(unsigned int i);
    virtual void resample(unsigned int n);

    inline double evalAt(double x) const {
        assert(!_points.empty());

        if (nbPoints() == 1) {
            // 1 point: return the current value
            return _points[0][1];
        }

        if (nbPoints() == 2) {
            // 2 points: return the linear interpolation
            return _points[0][1] + (_points[1][1] - _points[0][1]) * ((x - _points[0][0]) / (_points[1][0] - _points[0][0]));
        }

        // 3 or more points: cubic spline interpolation
        for (unsigned int i = 0; i < nbPoints() - 1; ++i) {
            if (_points[i + 1][0] >= x) {
                const unsigned int index = i * 4;
                const double x2 = x * x;
                const double x3 = x2 * x;
                return _x[index] + _x[index + 1] * x + _x[index + 2] * x2 + _x[index + 3] * x3;
            }
        }

        // default
        return _points[0][1];
    }

    inline double evalDerivativeAt(double x) const {
        assert(!_points.empty());

        if (nbPoints() == 1) {
            return 0;
        }

        if (nbPoints() == 2) {
            return (_points[1][1] - _points[0][1]) / (_points[1][0] - _points[0][0]);
        }

        for (unsigned int i = 0; i < nbPoints() - 1; ++i) {
            if (_points[i + 1][0] >= x) {
                const unsigned int index = i * 4;
                const double x2 = x * x;
                return _x[index + 1] + 2 * _x[index + 2] * x + 3 * _x[index + 3] * x2;
            }
        }

        // default
        return 0;
    }

   private:
    void computeSolution();
    void nbPointsChanged();

    Eigen::MatrixXf _A;
    Eigen::VectorXf _b;
    Eigen::VectorXf _x;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

/**
 * !deprecated, use CubicMonotonicInterpolator
 * This class is used to represent a strictly monotonic cubic polynomial of the form
 * P(x) = ax^3 + bx^2 + cx  with  0 <= x <= 1 and 0 <= P(x) <= 1
 * used to model the spacing function.
 * 
 * The cubic is approximated from 5 control points
 *   - the 2 extremities are fixed to (0,0) and (1,1)
 *   - the three middle control points have a fixed x-component of respectively 0.25, 0.5 and 0.75
 *   - the y-component of the middle control points are set by the user
 * 
 * The a, b and c coefficients of the cubic are computed by trying to fit a cubic in the least-squares
 * sense to all control points with 2 hard constraint: (1,1) and one of the middle control point (chosen by the user)
 * 
 * After computing the coefficients, if the resulting cubic is not monotonic, the cubic is not updated.
 * Thus the initial coefficients should be valid.
 * 
 * If the resulting cubic is monotonic, then the middle control points are reprojected onto the cubic.
 */
class CubicPolynomialInterpolator : public CurveInterpolator {
   public:
    CubicPolynomialInterpolator(const Eigen::Vector2d &pt);

    CubicPolynomialInterpolator(CurveInterpolator *curve);

    virtual int addKeyframe(const Eigen::Vector2d &pt);
    virtual void setKeyframe(const Eigen::Vector2d &pt, unsigned int i);
    virtual void delKeyframe(unsigned int i);

    inline double evalAt(double x) const {
        assert(_points.size() == 5);
        const double x2 = x * x;
        const double x3 = x2 * x;
        return _x[0] * x + _x[1] * x2 + _x[2] * x3;
    }

    inline double evalDerivativeAt(double x) const {
        assert(_points.size() == 5);
        const double x2 = x * x;
        return _x[0] + 2 * _x[1] * x + 3 * _x[2] * x2;
    }

    /**
     * fixedPointIdx is either 1, 2 or 3
     * after setting the new values of the control points 1, 2 and 3 
     * the new polynomial is approximated and the control points 
     * are reprojected onto the new curve 
     */
    void setControlPoints(double p1, double p2, double p3, int fixedPointIdx);

    /**
     * Splits the current cubic polynomial in two at x
     * Both halves are then remapped to the interval [0,1] (domain and codomain)
     * 
     * The second half (right) is returned by the function
     * The first half (left) is the current instance
     */
    CubicPolynomialInterpolator *splitAt(double x);

   private:
    void computeSolution();
    void computeSolutionAux(double x1, double x2, double x3);
    void resampleControlPoints();
    void setCoeffs(double a, double b, double c);

    int _constraintIdx;

    Eigen::MatrixXf _A;
    Eigen::VectorXf _b;
    Eigen::VectorXf _x;

    std::vector<double> _prevControlPointsY;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

/**
 * Monotonic Cubic Hermite Spline (1D)
*/
class CubicMonotonicInterpolator : public CurveInterpolator {
   public:
    CubicMonotonicInterpolator(const Eigen::Vector2d &pt);
    CubicMonotonicInterpolator(CurveInterpolator *curve);

    virtual int addKeyframe(const Eigen::Vector2d &pt) override;
    virtual void setKeyframe(const Eigen::Vector2d &pt, unsigned int i) override;
    virtual void delKeyframe(unsigned int i) override;
    virtual void removeLastPoint() override;
    virtual double normalizeX() override;

    inline double evalAt(double x) const override{
        assert(_points.size() >= 2);
        for (unsigned int i = 0; i < nbPoints()- 1; ++i) {
            if (_points[i + 1].x() >= x) {
                double dx = _points[i + 1].x() -_points[i].x();
                double t = (x - _points[i].x()) / dx;
                double tt = t*t;
                double ttt = t*tt;
                return (2.0 * ttt - 3.0 * tt + 1) * _points[i].y() + 
                       (ttt - 2.0 * tt + t) * dx * _slopes[i] +
                       (-2.0 * ttt + 3.0 * tt) * _points[i + 1].y() +
                       (ttt - tt) * dx * _slopes[i + 1];
            }
        }
        return _points[0].y();
    }

    inline double evalDerivativeAt(double x) const override {
        for (unsigned int i = 0; i < nbPoints()- 1; ++i) {
            if (_points[i + 1].x() >= x) {
                double dx = _points[i + 1].x() -_points[i].x();
                double t = (x - _points[i].x()) / dx;
                double tt = t*t;
                return ((6.0 * tt - 6.0 * t) * _points[i].y() + 
                       (3.0 * tt - 4.0 * t + 1) * dx * _slopes[i] +
                       (-6.0 * tt + 6.0 * t) * _points[i + 1].y() +
                       (3.0 * tt - 2.0 * t) * dx * _slopes[i + 1]) /
                       dx;
            }
        }
        return _slopes.back();
    }

    double evalInverse(double y) const override;

    /**
     * resample the current curve with n regularly spaced control points (not taking into account the first and last control points)
     */
    virtual void resample(unsigned int n) override;

    /**
     * resample the current curve by adding new control points in a dichotomic way
     * until the error between the actual curve and the resampled one falls under the
     * given threshold
     */
    void resampleDichotomic(unsigned int maxControlPoints, unsigned int nbFrames);

    double slopeAt(unsigned int i) const { return _slopes[i]; }
    void setSlope(unsigned int i, double slope) { _slopes[i] = slope; }
    unsigned int nbSlopes() const { return _slopes.size(); }
    void debugSlopes();

    double smallestXInterval() const;

    void smoothTangents() override;
    
   private:
    void makeSlopes();
    void makeNaturalC2();
    void updateSlope(unsigned int i, bool updateNeighbors=true);
    void checkSlopeMonotonicity(unsigned int i, double slope, double leftSecantSlope, double rightSecantSlope);
    void resampleDichotomicAddControlPoint(CubicMonotonicInterpolator *newCurve, double xa, double xb, unsigned int level, unsigned int maxLevel, double threshold);
    double meanSqErrorOnSegment(CubicMonotonicInterpolator *newCurve, double xa, double xb, unsigned int samples=10);
    double sdOnSegment(CubicMonotonicInterpolator *newCurve, double xa, double xb, unsigned int samples=10);

    std::vector<double> _slopes;
    
   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


// Hermite interpolation
class HermiteInterpolator : public CurveInterpolator {
   public:
    HermiteInterpolator(const Eigen::Vector2d &pt) : CurveInterpolator(pt) { initTangents(); }
    HermiteInterpolator(CurveInterpolator *curve) : CurveInterpolator(curve) { initTangents(); }
    virtual int addKeyframe(const Eigen::Vector2d &pt);
    virtual void delKeyframe(unsigned int i);
    virtual void setTangent(const Eigen::Vector2d &pt, unsigned int i = 0, unsigned int side = 0);
    inline virtual bool useTangents() const { return true; }
    virtual void tangentAt(double t, unsigned int i);
    virtual void tangentAt(double , Eigen::Vector2d &t1, Eigen::Vector2d &t2, Eigen::Vector2d &t3, Eigen::Vector2d &t4);

    inline double evalAt(double x) const {
        unsigned int i = 0;
        double t = findParam(x, i);
        if (x == 1.0) t = 1.0;  // FIXME: error with at least 3 controls points, findParam(1.0) returns -1.0! alsso findParam(0.0) returns -1.0 too! 
        t = std::min(std::max(t, 0.0), 1.0);

        const Eigen::Vector2d left = _tangents[i].head<2>();
        const Eigen::Vector2d right = _tangents[i + 1].tail<2>();

        const Eigen::Vector4d by = Geom::bezierCoeffs(_points[i][1], _points[i][1] + (left[1] /* / 3.0 */), _points[i + 1][1] + (right[1] /* / 3.0 */), _points[i + 1][1]);
        return by[0] * t * t * t + by[1] * t * t + by[2] * t + by[3];
    }

    inline double evalDerivativeAt(double x) const {
        unsigned int i = 0;
        double t = findParam(x, i);
        if (x == 1.0) t = 1.0;  // FIXME: error with at least 3 controls points, findParam(1.0) returns -1.0! also findParam(0.0) returns -1.0 too! 
        t = std::min(std::max(t, 0.0), 1.0);

        const Eigen::Vector2d left = _tangents[i].head<2>();
        const Eigen::Vector2d right = _tangents[i + 1].tail<2>();
        const Eigen::Vector4d by = Geom::bezierCoeffs(_points[i][1], _points[i][1] + (left[1] /* / 3.0 */), _points[i + 1][1] + (right[1]  /* / 3.0 */), _points[i + 1][1]);
        return 3 * by[0] * t * t + 2 * by[1] * t + by[2];
    }

    // find t given x
    inline double findParam(double x, unsigned int &i) const {
        // assert(!_points.empty());
        for (i = 0; i < nbPoints() - 1; ++i) {
            if (_points[i + 1][0] >= x) {
                const Eigen::Vector2d left = _tangents[i].head<2>();
                const Eigen::Vector2d right = _tangents[i + 1].tail<2>();
                const Eigen::Vector4d bx = Geom::bezierCoeffs(_points[i][0], _points[i][0] + (left[0] /* / 3.0 */) + 1e-8, _points[i + 1][0] + (right[0] /* / 3.0 */) - 1e-8, _points[i + 1][0]); // FIXME: try to remove/balance the epsilon
                if (fabs(bx[0]) < 1e-8) return Utils::quadraticRoot(bx[1], bx[2], (bx[3] - x));
                return Utils::cubicRoot(bx[1] / bx[0], bx[2] / bx[0], (bx[3] - x) / bx[0]);
            }
        }
        return 0;
    }

   private:
    void initTangents();

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class HermiteInterpolatorArcLength : public HermiteInterpolator {
    public:
        HermiteInterpolatorArcLength(const Eigen::Vector2d &pt) : HermiteInterpolator(pt) { updateArclengthLUT(); }
        HermiteInterpolatorArcLength(CurveInterpolator *curve) : HermiteInterpolator(curve) { updateArclengthLUT(); }

        inline int addKeyframe(const Eigen::Vector2d &pt){ 
            int ret = HermiteInterpolator::addKeyframe(pt); 
            updateArclengthLUT();
            return ret;
        }
        void updateArclengthLUT();
        virtual double evalFromTandI(double t, unsigned int i);
        virtual double findParamArcLength(double x, unsigned int &i);
        inline double evalArcLengthAt(double x){
            unsigned int i = 0;
            double t = findParamArcLength(x, i);
            return evalFromTandI(t, i);
        }
        inline double evalAt(double x){ return evalArcLengthAt(x); }

    private:
        std::vector<std::array<std::array<double, LUT_PRECISION>, 2>> alengthLUT; // 0:s  1:t
};

// Curve...
class Curve {
   public:
    enum { LINEAR_INTERP = 0, STEP_INTERP = 1, SHEPARD_INTERP = 2, SPLINE_INTERP = 3, HERMITE_INTERP = 4,
            CUBIC_INTERP = 5, MONOTONIC_CUBIC_INTERP = 6,  HERMITE_ARC_LENGTH_INTERP = 7 };

    Curve(const Eigen::Vector2d &pt, int interpolation = 0);
    Curve(const Curve &other);
    virtual ~Curve();

    CurveInterpolator *interpolator() const { return _interpolator; }

    // evaluation
    inline double evalAt(double x) const {
        if (x < point(0)[0]) return point(0)[1];
        if (x > point(nbPoints() - 1)[0]) return point(nbPoints() - 1)[1];
        return _interpolator->evalAt(x);
    }

    inline double evalDerivativeAt(double x) const {
        if (x < point(0)[0] || x > point(nbPoints() - 1)[0]) return 0.0; // or evalDerivativeAt extremity?
        return _interpolator->evalDerivativeAt(x);
    }

    inline double evalInverse(double y) const {
        return _interpolator->evalInverse(y);
    }

    // keyframing
    inline int addKeyframe(const Eigen::Vector2d &pt) { return _interpolator->addKeyframe(pt); }
    inline void setKeyframe(const Eigen::Vector2d &pt, unsigned int i = 0) { _interpolator->setKeyframe(pt, i); }
    inline void setTangent(const Eigen::Vector2d &pt, unsigned int i = 0, unsigned int side = 0) { _interpolator->setTangent(pt, i, side); }
    inline void setTangent(const Eigen::Vector4d &pt, unsigned int i = 0) { _interpolator->setTangent(pt, i); }
    inline void delKeyframe(unsigned int i) { _interpolator->delKeyframe(i); }
    inline void moveKeys(int offsetFirst, int offsetLast) { _interpolator->moveKeys(offsetFirst, offsetLast); }
    inline void removeKeyframeBefore(int frame) { _interpolator->removeKeyframeBefore(frame); }
    inline void removeKeyframeAfter(int frame) { _interpolator->removeKeyframeAfter(frame); }
    inline void removeKeys() { _interpolator->removeKeys(); }
    inline void removeLastPoint() { _interpolator->removeLastPoint(); }

    // specify the interpolation mode (see enum above)
    void setInterpolation(int interpolation);
    void setInterpolator(CurveInterpolator *interpolator) { _interpolator = interpolator; }

    // curve samples
    void resample(unsigned int n) { _interpolator->resample(n); }
    const std::vector<Eigen::Vector2d> samplePoints(double x1, double x2, unsigned int nb = 100) const;
    const std::vector<Eigen::Vector2d> sampleLines(double x1, double x2, unsigned int nb = 100) const;

    // extract a "sub-curve" from the points i to j (inclusive) of this curve
    // optionally remaps the x-axis boundaries of the curve to be [0,1] (and y-axis if the curve is monotonic piecewise cubic)
    Curve *cut(int i, int j, bool resetXBoundaries = true);

    // normalize x-component
    inline void normalizeX() { _interpolator->normalizeX(); }
    // normalize both components of the curve
    inline void normalize() { }

    // computes the tangents of the curve at the give point
    inline void tangentAt(double t, unsigned int i) {
        return _interpolator->tangentAt(t, i);
    }

    // scale vertical component of tangents
    void scaleTangentVertical(double factor = 1.0) { _interpolator->scaleTangentVertical(factor); }

    // set the curve to be piecewise linear
    void setPiecewiseLinear();

    // set the tangent of each control points based on its neighbors
    void smoothTangents() { _interpolator->smoothTangents(); }

    // accessors
    inline int interpType() const { return _interpType; }
    inline const QString &interpName() const { return _interpNames[_interpType]; }
    inline unsigned int nbPoints() const { return _interpolator->nbPoints(); }
    inline unsigned int nbTangents() const { return _interpolator->nbTangents(); }
    inline const std::vector<Eigen::Vector2d> &points() const { return _interpolator->points(); }
    inline const std::vector<Eigen::Vector4d> &tangents() const { return _interpolator->tangents(); }
    inline const Eigen::Vector2d &point(unsigned int i = 0) const { return _interpolator->point(i); }
    inline const Eigen::Vector4d &tangent(unsigned int i = 0) const { return _interpolator->tangent(i); }
    inline bool useTangents() const { return _interpolator->useTangents(); }
    static const QStringList &interpNames() { return _interpNames; }
    QRectF getBoundingBox();

    void print(std::ostream &os) const;

   private:
    CurveInterpolator *createInterpolator(int interpolation, const Eigen::Vector2d &pt);
    CurveInterpolator *createInterpolator(int interpolation, CurveInterpolator *curve);
    int _interpType;

    CurveInterpolator *_interpolator;

    static QStringList _interpNames;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

#endif  // CURVE
