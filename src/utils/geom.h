/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __GEOM_H__
#define __GEOM_H__

#include "point.h"

// some geometry utils
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
     * Directed angle from vector a to b in the range [-PI,PI]
    */
    inline Point::Scalar polarAngle(Point::VectorType &a, Point::VectorType &b) { 
        Point::Scalar angle = std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()); 
        if (angle > M_PI) angle -= 2 * M_PI;
        else if (angle <= -M_PI) angle += 2 * M_PI;
        return angle;
    }

    inline int sgn(float x) { 
        return x < 0 ? -1 : 1; 
    }

    template<class T>
    inline T smoothstep(T x) {
        return x * x * (3 - 2 * x);
    }

    // Compute the coefficients of the cubic polynomial: z0*t^3 + z1*t^2 + z2*t + z3 from 4 cubic bezier control points coordinates
    inline Eigen::Vector4f bezierCoeffs(float p0, float p1, float p2, float p3) {
        float z0 = -p0 + 3.f * p1 - 3.f * p2 + p3;
        float z1 = 3.f * p0 - 6.f * p1 + 3.f * p2;
        float z2 = -3.f * p0 + 3.f * p1;
        float z3 = p0;
        return Eigen::Vector4f(z0, z1, z2, z3);
    }
}

#endif // __GEOM_H__