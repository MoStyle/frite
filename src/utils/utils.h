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
    inline bool approximatelyEqual(float a, float b, float epsilon) {
        return std::fabs(a - b) <= ( (std::fabs(a) < std::fabs(b) ? std::fabs(b) : std::fabs(a)) * epsilon);
    }

    inline bool essentiallyEqual(float a, float b, float epsilon) {
        return std::fabs(a - b) <= ( (std::fabs(a) > std::fabs(b) ? std::fabs(b) : std::fabs(a)) * epsilon);
    }

    inline bool definitelyGreaterThan(float a, float b, float epsilon) {
        return (a - b) > ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
    }

    inline bool definitelyLessThan(float a, float b, float epsilon) {
        return (b - a) > ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
    }

    inline int sgn(float x) { return x < 0 ? -1 : 1; }

    inline float quadraticRoot(float A, float B, float C) {
        const float det = B * B - 4.f * A * C;
        Eigen::Vector2f t;
        if (det > 0) {
            t[0] = -0.5f * (B + sqrtf(det)) / A;
            t[1] = -0.5f * (B - sqrtf(det)) / A;
        } else if (det < 0) {
            t[0] = -1;
            t[1] = -1;
        } else {
            t[0] = -0.5f * B / A;
            t[1] = -1;
        }
        if (t[0] >= 0.0f -1e-5f && t[0] <= 1.0f + 1e-5f) return t[0];
        return t[1];
    }

    inline float cubicRoot(float A, float B, float C) {
        const float AA = A * A;
        const float Q = (3.f * B - AA) / 9.f;
        const float R = (9.f * A * B - 27.f * C - 2.f * A * AA) / 54.f;
        const float QQQ = Q * Q * Q;
        const float D = QQQ + R * R;  // polynomial discriminant

        Eigen::Vector3f t;

        if (D >= 0)  // complex or duplicate roots
        {
            const float Dsqrt = sqrtf(D);
            const float S = sgn(R + Dsqrt) * powf(fabs(R + Dsqrt), (1.f / 3.f));
            const float T = sgn(R - Dsqrt) * powf(fabs(R - Dsqrt), (1.f / 3.f));

            t[0] = -A / 3.f + (S + T);                    // real root
            t[1] = -A / 3.f - (S + T) / 2.f;              // real part of complex root
            t[2] = -A / 3.f - (S + T) / 2.f;              // real part of complex root
            const float Im = sqrtf(3.f) * (S - T) / 2.f;  // complex part of root pair

            if (Im != 0)  // discard complex roots
            {
                t[1] = -1;
                t[2] = -1;
            }
        } else  // distinct real roots
        {
            const float th = acosf(R / sqrtf(-QQQ));
            const float Qsqrt = sqrtf(-Q);

            t[0] = 2.f * Qsqrt * cosf(th / 3.f) - A / 3.f;
            t[1] = 2.f * Qsqrt * cosf((th + 2.f * M_PI) / 3.f) - A / 3.f;
            t[2] = 2.f * Qsqrt * cosf((th + 4.f * M_PI) / 3.f) - A / 3.f;
        }

        if (t[0] >= 0.0f -1e-5f && t[0] <= 1.0f + 1e-5f) return t[0];
        if (t[1] >= 0.0f -1e-5f && t[1] <= 1.0f + 1e-5f) return t[1];
        return t[2];
    }

    inline double normalPDF(double x, double mean, double sigma) {
        static const double inv_sqrt_2pi = 0.3989422804014327;
        double a = (x - mean) / sigma;
        return inv_sqrt_2pi / sigma * std::exp(-0.5 * a * a);
    }

    inline double gaussian(double xSq, double sigma) {
        double a = 1.0 / sigma;
        return std::exp(-0.5 * xSq * a * a);
    }

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
}

#endif // __UTILS_H__