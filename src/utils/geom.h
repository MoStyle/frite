#ifndef __GEOM_H__
#define __GEOM_H__

#include "point.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>

namespace Geom {
    
    /**
     * a^b > 0 => a is to the left of b
     * a^b < 0 => a is to the right of b
     * a^b == 0 => a and b are parallel or 0
     */
    inline Point::Scalar wedge(const Point::VectorType &a, const Point::VectorType &b) {
        return a.x() * b.y() - a.y() * b.x();
    }

    /**
     * Returns a positive value if p1,p2,p3 are clockwise
     * Returns a negative value if p1,p2,p3 are counter-clockwise
     * Returns 0 if p1,p2,p3 are collinear
     * Basically (p2-p1)^(p3-p1)
     */
    inline Point::Scalar wedge2(Point::VectorType &p1, Point::VectorType &p2, Point::VectorType &p3) {
        return (p2.x() - p1.x()) * (p3.y() - p1.y()) - (p2.y() - p1.y()) * (p3.x() - p1.x());
    }

    /**
     * Returns true if segment [p1,p2] intersects segment [q1,q2]
     * The segments must NOT be collinear
     */
    inline bool checkSegmentsIntersection(const Point::VectorType &p1, const Point::VectorType &p2,
                                         const Point::VectorType &q1, const Point::VectorType &q2) {
        return (wedge((p1 - q1), (q2 - q1)) * wedge((p2 - q1), (q2 - q1)) <= 0 &&
                wedge((q1 - p1), (p2 - p1)) * wedge((q2 - p1), (p2 - p1)) <= 0);
    }

    /**
     * Returns the projection of point P onto the line segment [A,B] 
     */
    inline Point::VectorType projectPointToSegment(const Point::VectorType &A, const Point::VectorType &B, const Point::VectorType &P) {
        Point::VectorType AB = B - A;
        Point::Scalar AB_sq = AB.dot(AB);
        if (AB_sq == 0) return A;
        Point::Scalar t = (P - A).dot(AB) / AB_sq;
        if (t < 0) return A;
        else if (t > 1.0) return B;
        return A + AB * t;
    }

    /**
     * Directed angle from vector a to b in the range [-PI,PI]
     */
    inline Point::Scalar polarAngle(Point::VectorType &a, Point::VectorType &b) { 
        Point::Scalar angle = std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()); 
        if (angle > M_PI) angle -= 2 * M_PI;
        else if (angle <= -M_PI) angle += 2 * M_PI;
        return angle;
    }

    template<class T>
    inline T smoothstep(T x) {
        return x * x * (3 - 2 * x);
    }

    template<class T>
    inline T smoothconc(T x) {
        return 1.0 - (1.0 - x) * (1.0 - x);
    }

    // https://www.desmos.com/calculator/e3bitc8c2q
    template<class T>
    inline T easeInOrOut(T x, T b) {
        x = std::clamp(x, T(0.0), T(1.0));
        return x / (-std::exp(b) * (x - 1) + x); 
    }

    // https://www.desmos.com/calculator/bdxyp7wo7c
    template<class T>
    inline T easeInAndOut(T x, T b) {
        x = std::clamp(x, T(0.0), T(1.0));
        return  x <= 0.5 ? easeInOrOut(2 * x, b) / 2 : easeInOrOut(2 * x - 1, -b) / 2 + 0.5;
    }

    // https://www.desmos.com/calculator/vj7z1yg3p5
    template<class T>
    inline T expblend(T x, T b, T p, T yLow, T yHigh) {
        if (x < 0) return yLow;
        else if (x > 1) return yHigh;
        T q = 2 / (1 - b) - 1;
        if (x <= p) return std::pow(x, q) / std::pow(p, q - 1) * (yHigh - yLow) + yLow;
        return (1 - std::pow(1 - x, q) / std::pow(1 - p, q - 1)) * (yHigh - yLow) + yLow;
    }

    // Compute the coefficients of the polynomial form of a cubic bezier curve its 4 control points
    // p(t) = c3*t^3 + c2*t^2 + c1*t + c 
    template<class T>
    inline Eigen::Vector4<T> bezierCoeffs(T p0, T p1, T p2, T p3) {
        T c3 = -p0 + T(3.0) * p1 - T(3.0) * p2 + p3;
        T c2 = T(3.0) * p0 - T(6.0) * p1 + T(3.0) * p2;
        T c1 = -T(3.0) * p0 + T(3.0) * p1;
        T c0 = p0;
        return Eigen::Vector4<T>(c3, c2, c1, c0);
    }

    // Compute the coefficients of the polynomial form of a cubic hermite curve from its 2 control points "p" and 2 tangents "m"
    template<class T>
    inline Eigen::Vector4<T> hermiteCoeffs(T p0, T m0, T p1, T m1) {
        T c3 = T(2.0) * p0 + m0 - T(2.0) * p1 + m1;
        T c2 = T(-3.0) * p0 + T(3.0) * p1 - T(2.0) * m0 - m1;
        T c1 = m0;
        T c0 = p0;
        return Eigen::Vector4<T>(c3, c2, c1, c0);
    }

    /**
     * t in [0,1]
     * p0 and p1 are the starting and ending points respectively
     * m0 and m1 are the tangents at p0 and p1 respectively
     */
    template<class T>
    inline Eigen::Vector2<T> evalCubicHermite(T t, const Eigen::Vector2<T>& p0, const Eigen::Vector2<T>& m0, const Eigen::Vector2<T>& p1, const Eigen::Vector2<T>& m1) {
        T tt = t * t;
        T ttt = tt * t;
        return (T(2.0)* ttt - T(3.0) * tt + 1) * p0 + (ttt - T(2.0) * tt + t) * m0 + (-T(2.0) * ttt + T(3.0) * tt) * p1 + (ttt - tt) * m1;
    }

    /**
     * t in [t0,t1]
     * p0 and p1 are the starting and ending points respectively
     * m0 and m1 are the tangents at p0 and p1 respectively
     */
    template<class T>
    inline Eigen::Vector2<T> evalCubicHermite(T t, T t0, T t1, const Eigen::Vector2<T>& p0, const Eigen::Vector2<T>& m0, const Eigen::Vector2<T>& p1, const Eigen::Vector2<T>& m1) {
        t = (t - t0) / (t1 - t0);
        T tt = t * t;
        T ttt = tt * t;
        return (T(2.0)* ttt - T(3.0) * tt + 1) * p0 + (ttt - T(2.0) * tt + t) * m0 + (-T(2.0) * ttt + T(3.0) * tt) * p1 + (ttt - tt) * m1;
    }
}

#endif // __GEOM_H__