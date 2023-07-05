/*
 * SPDX-FileCopyrightText: 2010 Ilya Baran (baran37@gmail.com)
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "piecewiselinearutils.h"
#include <limits>
#include <QDebug>

using namespace std;

void PiecewiseLinearMonotone::add(double x, double y)
{
    const double tol = 1e-8;

    y *= _sign;

    set<PLPoint>::const_iterator it = _points.insert(PLPoint(x, y)).first;
    set<PLPoint>::const_iterator it2 = it;

    //self test
    if(it != _points.begin() && y + tol < (--it)->y)
        qDebug() << "ERROR: Not monotone w.r.t. prev!\n";
    if(++it2 != _points.end() && y - tol > it2->y)
        qDebug() << "ERROR: Not monotone w.r.t. next!\n";
}

bool PiecewiseLinearMonotone::eval(double x, double &outY) const
{
    const double tol = 1e-8;

    if(_points.empty())
        return false;

    //check if we're near the ends of the range
    if(std::abs(x - _points.begin()->x) < tol)
    {
        outY = _sign * _points.begin()->y;
        return true;
    }
    set<PLPoint>::const_iterator it = _points.end();
    --it; //last element
    if(std::abs(x - it->x) < tol)
    {
        outY = _sign * it->y;
        return true;
    }

    //now look for the point for real
    it = _points.lower_bound(PLPoint(x, 0.));

    //check if we're out of range
    if(it == _points.begin())
    {
        outY = _sign * it->y;
        return false;
    }
    if(it == _points.end())
    {
        outY = _sign * (--it)->y;
        return false;
    }

    set<PLPoint>::const_iterator prev = it;
    --prev;

    if(it->x - prev->x < tol) //if the slope is infinite or somehow negative
    {
        outY = _sign * prev->y;
        return true;
    }

    outY = prev->y + (it->y - prev->y) * (x - prev->x) / (it->x - prev->x);
    outY *= _sign;
    return true;
}

bool PiecewiseLinearMonotone::invert(double y, double &outX) const
{
    y *= _sign;

    const double tol = 1e-8;

    if(_points.empty())
        return false;

    //check if we're near the ends of the range
    if(std::abs(y - _points.begin()->y) < tol)
    {
        outX = _points.begin()->x;
        return true;
    }
    set<PLPoint>::const_iterator it = _points.end();
    --it; //last element
    if(std::abs(y - it->y) < tol)
    {
        outX = it->x;
        return true;
    }

    //now look for the point for real
    it = _points.lower_bound(PLPoint(y));

    //check if we're out of range
    if(it == _points.begin())
    {
        outX = it->x;
        return false;
    }
    if(it == _points.end())
    {
        outX = (--it)->x;
        return false;
    }

    set<PLPoint>::const_iterator prev = it;
    --prev;

    if(it->y - prev->y < tol) //if the slope is small or somehow negative
    {
        outX = prev->x;
        return true;
    }

    outX = prev->x + (it->x - prev->x) * (y - prev->y) / (it->y - prev->y);
    return true;
}

double PiecewiseLinearMonotone::minX() const
{
    if(_points.empty())
        return numeric_limits<double>::max();
    return _points.begin()->x;
}

double PiecewiseLinearMonotone::maxX() const
{
    if(_points.empty())
        return -numeric_limits<double>::max();
    return (--_points.end())->x;
}

bool PiecewiseLinearMonotone::batchEval(vector<double> &inXoutY) const
{
    if(_points.empty())
        return false;

    bool allGood = true;
    for(int i = 0; i < (int)inXoutY.size(); ++i)
    {
        if(!eval(inXoutY[i], inXoutY[i]))
        {
            qDebug() << "PiecewiseLinearMonotone evaluation error!\n";
            allGood = false;
        }
    }
    return allGood;
}
