/*
 * SPDX-FileCopyrightText: 2010 Ilya Baran (baran37@gmail.com)
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CORNUCOPIA_PIECEWISELINEARUTILS_H_INCLUDED
#define CORNUCOPIA_PIECEWISELINEARUTILS_H_INCLUDED

#include <vector>
#include <set>

//Approximates a monotone function piecewise-linearly.  Supports adding points and solving for x given y.
//Used for solving for the adjustment to the sampling rate that leads to an integer number of samples and
//other resampling stuff.
class PiecewiseLinearMonotone
{
public:
    enum Sign { POSITIVE, NEGATIVE };

    PiecewiseLinearMonotone(Sign sign) : _sign(sign ? -1. : 1.) {}

    void add(double x, double y);
    bool eval(double x, double &outY) const; //returns true if evaluation is successful
    bool invert(double y, double &outX) const; //returns true if inversion is successful

    double minX() const;
    double maxX() const;

    bool batchEval(std::vector<double> &inXoutY) const; //returns false if any evaluation fails
private:
    struct PLPoint //allows comparison by x or by y -- order in the map should be the same
    {
        PLPoint(double inX, double inY) : x(inX), y(inY), compareByY(false) {}
        PLPoint(double inY) : x(-1), y(inY), compareByY(true) {}

        bool operator<(const PLPoint &other) const { if(compareByY || other.compareByY) return y < other.y; else return x < other.x; }

        double x, y;
        bool compareByY;
    };

    double _sign;
    std::set<PLPoint> _points;
};

#endif //CORNUCOPIA_PIECEWISELINEARUTILS_H_INCLUDED
