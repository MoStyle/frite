/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

 #ifndef __UTILS_H__
#define __UTILS_H__

#if (_MSC_VER >= 1500)
#define _USE_MATH_DEFINES
#include <math.h>
#endif

#include <Eigen/Dense>

namespace Utils {

    constexpr double EPS = 1e-6;

    template<class T>
    inline bool approximatelyEqual(T a, T b, T epsilon) {
        return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
    }

    template<class T>
    inline bool essentiallyEqual(T a, T b, T epsilon) {
        return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * epsilon);
    }

    template<class T>
    inline bool definitelyGreaterThan(T a, T b, T epsilon) {
        return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
    }

    template<class T>
    inline bool definitelyLessThan(T a, T b, T epsilon) {
        return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
    }

    template<class T>
    inline int sgn(T x) { return x < 0 ? -1 : 1; }

    template<class T>
    inline int pmod(T a, T b) { return ((a % b) + b) % b; }

    inline unsigned int cantor(unsigned int a, unsigned int b) {
        return 0.5 * (a + b) * (a + b + 1) + b;
    }

    inline std::pair<unsigned int, unsigned int> invCantor(unsigned int z) {
        unsigned int w = (unsigned int)(std::sqrt(8 * z + 1) - 1) / 2; // floor w/ unsigned int division
        unsigned int t = (w * w + w) / 2; // triangle number of w
        unsigned int y = z - t;
        unsigned int x = w - y;
        return {x, y};
    }

    // Find the smallest root in [0,1] of a quadratic polynomial
    // Returns -1 if there are no roots in [0,1]
    template<class T>
    inline T quadraticRoot(T A, T B, T C) {
        const T det = B * B - T(4.0) * A * C;
        Eigen::Vector2<T> t;
        if (det > 0) {
            t[0] = T(-0.5) * (B + sqrtf(det)) / A;
            t[1] = T(-0.5) * (B - sqrtf(det)) / A;
        } else if (det < 0) {
            t[0] = T(-1.0);
            t[1] = T(-1.0);
        } else {
            t[0] = T(-0.5) * B / A;
            t[1] = T(-1.0);
        }
        if (t[0] >= T(0.0) - T(1e-5) && t[0] <= T(1.0) + T(1e-5)) return t[0];
        return t[1];
    }

    // Find the smallest root in [0,1] of a cubic polynomial using Cardano's method 
    // Return -1 if there are no roots in [0,1]
    // (1) https://math.stackexchange.com/questions/158271/relationship-between-the-coefficients-of-a-polynomial-equation-and-its-roots
    template <class T>
    inline T cubicRoot(T A, T B, T C) {
        const T AA = A * A;
        const T Q = (T(3.0) * B - AA) / T(9.0);
        const T R = (T(9.0) * A * B - T(27.0) * C - T(2.0) * A * AA) / T(54.0);
        const T QQQ = Q * Q * Q;
        const T D = QQQ + R * R;  // polynomial discriminant

        Eigen::Vector3f t;

        if (D >= T(0.0))  // complex or duplicate roots
        {
            const T Dsqrt = sqrtf(D);
            const T S = sgn(R + Dsqrt) * powf(fabs(R + Dsqrt), (T(1.0) / T(3.0)));
            const T TT = sgn(R - Dsqrt) * powf(fabs(R - Dsqrt), (T(1.0) / T(3.0)));

            t[0] = -A / T(3.0) + (S + TT);                  // real root
            t[1] = -A / T(3.0) - (S + TT) / T(2.0);         // real part of complex root
            t[2] = -A / T(3.0) - (S + TT) / T(2.0);         // real part of complex root
            const T Im = sqrt(T(3.0)) * (S - TT) / T(2.0);  // complex part of root pair

            if (Im != T(0.0))  // discard complex roots
            {
                t[1] = T(-1.0);
                t[2] = T(-1.0);
            }
        } else  // distinct real roots
        {
            const T th = acos(R / sqrt(-QQQ));
            const T Qsqrt = sqrt(-Q);

            t[0] = 2.f * Qsqrt * cosf(th / T(3.0)) - A / T(3.0);
            t[1] = 2.f * Qsqrt * cosf((th + T(2.0) * M_PI) / T(3.0)) - A / T(3.0);
            t[2] = 2.f * Qsqrt * cosf((th + T(4.0) * M_PI) / T(3.0)) - A / T(3.0);
        }

        if (t[0] >= T(0.0) - T(1e-5f) && t[0] <= T(1.0) + T(1e-5)) return t[0];
        if (t[1] >= T(0.0) - T(1e-5f) && t[1] <= T(1.0) + T(1e-5)) return t[1];
        return t[2];
    }

    template <class T>
    inline T normalPDF(T x, T mean, T sigma) {
        static const T inv_sqrt_2pi = T(0.3989422804014327);
        T a = (x - mean) / sigma;
        return inv_sqrt_2pi / sigma * std::exp(T(-0.5) * a * a);
    }

    template <class T>
    inline T gaussian(T xSq, T sigma) {
        T a = T(1.0) / sigma;
        return std::exp(T(-0.5) * xSq * a * a);
    }

    /**
     * Phase unwrap input signal
     */
    inline void unwrap(Eigen::ArrayXd &sig) {
        double prev = sig[0];
        double diff = 0.0;
        for (int i = 1; i < sig.size(); ++i) {
            diff = sig[i] - prev;
            prev = sig[i];
            diff = diff > M_PI ? diff - 2 * M_PI : (diff < -M_PI ? diff + 2 * M_PI : diff);
            sig[i] = sig[i - 1] + diff;
        }
    }

    /**
     * Linear interpolation from a to b, with interpolating factor t in [0, 1]
     */
    template <class T, class U>
    inline T lerp(T a, T b, U t) {
        return (U(1.0) - t) * a + t * b;
    }

    /**
     * Map x in [a, b] into [c, d]
     */
    template<class T>
    inline T map(T x, T a, T b, T c, T d) {
        return c + ((x - a) * (d - c) / (b - a)); 
    }
}


#endif // __UTILS_H__