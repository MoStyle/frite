/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include <QDebug>

#include "polyline.h"
#include "piecewiselinearutils.h"
#include <iostream>

using namespace Eigen;
namespace Frite {
Polyline::Polyline(const std::vector<Point *> &pts) {
    _pts.reserve(pts.size());
    for (const Point *p : pts) {
        _pts.push_back(new Point(*p));
    }
    if (_pts.size() > 1) {
        _lengths.reserve(pts.size() + 1);
        _lengths.push_back(0);
        for (size_t i = 0; i < pts.size() - 1; ++i) _lengths.push_back(_lengths.back() + (pts[i]->pos() - pts[i + 1]->pos()).norm());
    } else {
        qCritical() << "stroke too short !\n";
        throw std::runtime_error("stroke too short");
    }
}

Polyline::Polyline(const Polyline &polyline) {
    _pts.reserve(polyline._pts.size());
    for (const Point *p : polyline._pts) {
        _pts.push_back(new Point(*p));
    }
    _lengths.reserve(polyline._lengths.size());
    for (size_t i = 0; i < polyline._lengths.size(); i++) {
        _lengths.push_back(polyline._lengths[i]);
    }
}

Polyline::~Polyline() {
    for (const Point *p : _pts) {
        delete p;
    }
}

void Polyline::addPoint(Point *point) {
    _pts.push_back(point);
    if (_pts.size() == 1)
        _lengths.push_back(0);
    else
        _lengths.push_back(_lengths.back() + (_pts[_pts.size() - 2]->pos() - _pts[_pts.size() - 1]->pos()).norm());
}

void Polyline::load(QTextStream &posStream, size_t size) {
    _pts.clear();
    _pts.reserve(size);
    for (size_t j = 0; j < size; ++j) {
        double x, y, i, p;
        posStream >> x >> y >> i >> p;
        _pts.push_back(new Point(x, y, i, p));
    }
    _lengths.clear();
    _lengths.reserve(_pts.size() + 1);
    _lengths.push_back(0);
    for (size_t i = 0; i < _pts.size() - 1; ++i) _lengths.push_back(_lengths.back() + (_pts[i]->pos() - _pts[i + 1]->pos()).norm());
}

void Polyline::clear() {
    _pts.clear();
    _lengths.clear();
}

int Polyline::paramToIdx(Point::Scalar param, Point::Scalar *outParam) const {
    int idx = (int)std::min(std::upper_bound(_lengths.begin(), _lengths.end(), param) - _lengths.begin(), (ptrdiff_t)_lengths.size() - 1) - 1;
    if (outParam) *outParam = param - _lengths[idx];
    return idx;
}

void Polyline::eval(Point::Scalar s, Point::VectorType *pos, Point::VectorType *der, Point::VectorType *der2, Point *point) const {
    Point::Scalar cParam;
    int idx = paramToIdx(s, &cParam);
    int nextIdx = (idx + 1) % _pts.size();
    Point::Scalar dist = _lengths[idx + 1] - _lengths[idx];
    Point::Scalar invLength = dist == 0.0 ? 1.0 : (1.0 / dist);
    Point::VectorType pointPos;
    Point::Scalar cParamNorm = cParam * invLength;
    if (pos) (*pos) = _pts.at(idx)->pos() + cParamNorm * (_pts.at(nextIdx)->pos() - _pts.at(idx)->pos());
    if (der) (*der) = (_pts.at(nextIdx)->pos() - _pts.at(idx)->pos()) * invLength;
    if (der2) (*der2) = Point::VectorType();  // (*der2) = 6 * ((1 - t) * _pts[0] + t * _pts[3] + (3 * t - 2) * _pts[1] + (1 - 3 * t) * _pts[2]);
    if (point) {
        // linearly interpolate all point properties
        (*point).setPos(_pts.at(idx)->pos() + cParamNorm * (_pts.at(nextIdx)->pos() - _pts.at(idx)->pos()));
        (*point).setTemporalW(_pts.at(idx)->temporalW() + cParamNorm * (_pts.at(nextIdx)->temporalW() - _pts.at(idx)->temporalW()));
        (*point).setInterval(_pts.at(idx)->interval() + cParamNorm * (_pts.at(nextIdx)->interval() - _pts.at(idx)->interval()));
        (*point).setPressure(_pts.at(idx)->pressure() + cParamNorm * (_pts.at(nextIdx)->pressure() - _pts.at(idx)->pressure()));
        // TODO: compute normals
        // also m_interval?
    }
}

Point::Scalar Polyline::project(const Point::VectorType &point) const {
    Point::Scalar bestS = 0.;
    Point::Scalar minDistSq = (point - _pts[0]->pos()).squaredNorm();

    for (size_t i = 0; i < _pts.size() - 1; ++i) {
        Point::Scalar len = (_lengths[i + 1] - _lengths[i]);
        Point::Scalar invLen = 1. / len;
        Point::VectorType der = (_pts[i + 1]->pos() - _pts[i]->pos()) * invLen;
        Point::Scalar dot = der.dot(point - _pts[i]->pos());
        if (dot < 0.) dot = 0.;
        if (dot > len) dot = len;

        Point::VectorType ptOnLine = _pts[i]->pos() + (_pts[i + 1]->pos() - _pts[i]->pos()) * (dot * invLen);
        Point::Scalar distSq = (ptOnLine - point).squaredNorm();
        if (distSq < minDistSq) {
            minDistSq = distSq;
            bestS = _lengths[i] + dot;
        }
    }

    return bestS;
}

int Polyline::pointToIdx(const Point *point) const {
    for (int i = 0; i < _pts.size(); i++) {
        if (point == _pts[i]) return i;
    }
    return -1;
}

Point::Scalar Polyline::lengthFromTo(int fromIdx, int toIdx) const {
    Point::Scalar out = _lengths[toIdx] - _lengths[fromIdx];
    if (toIdx < fromIdx) out += _lengths.back();
    return out;
}

void Polyline::trimmed(Point::Scalar from, Point::Scalar to, Polyline &trimmedPoly) const {
    Point::Scalar len = length();
    const Point::Scalar tol = 1e-10;

    from = std::max(0.0, from);
    to = std::min(len, to);
    if (from > to) std::swap(from, to);

    Point::Scalar paramRemainder;
    int startIdx = paramToIdx(from, &paramRemainder);

    trimmedPoly.addPoint(point(from));  // push the first point no matter what

    int endIdx = paramToIdx(to, &paramRemainder);

    if (startIdx != endIdx || (to + tol < from))  // add points from existing polyline if necessary
    {
        for (int i = startIdx + 1; i < endIdx; i++) {
            trimmedPoly.addPoint(new Point(*(_pts[i])));
        }
    }

    if (paramRemainder > tol)  // if there's something leftover at the end, add the endpoint
        trimmedPoly.addPoint(point(to));
}

void Polyline::subPoly(int from, int to, Polyline &subPoly) const {
    subPoly.clear();
    for (int i = from; i <= to; ++i) {
        subPoly.addPoint(new Point(*(_pts[i])));
        subPoly._pts.back()->setGroupId(-1);
    }
}

bool Polyline::removeSection(std::vector<Point *> &points, std::vector<Point *> &remainder) {
    if (points.empty()) return false;
    int fromIdx = pointToIdx(points[0]);
    int toIdx = pointToIdx(points.back());
    if (toIdx == _pts.size() - 1) {
        qDeleteAll(_pts.begin() + fromIdx, _pts.end());
        _pts.resize(fromIdx);
        return false;
    }
    if (fromIdx == 0) {
        qDeleteAll(_pts.begin(), _pts.begin() + toIdx);
        _pts.erase(_pts.begin(), _pts.begin() + toIdx);
        return false;
    }
    std::vector<Point *>::iterator remainderIt;
    remainderIt = _pts.erase(_pts.begin() + fromIdx, _pts.begin() + toIdx);
    while (remainderIt != _pts.end()) {
        remainder.push_back(*remainderIt);
        remainderIt++;
    }
    _pts.resize(fromIdx);
    return true;
}

bool Polyline::removeSection(std::vector<int> &points, std::vector<std::vector<Point *>> &remainder) {
    if (points.empty()) return false;

    std::vector<std::pair<int, int>> intervals;
    std::vector<Point *>::iterator remainderIt;
    std::vector<Point *>::iterator next;

    if (points.size() == 1) {
        int val = points[0];
        qDeleteAll(_pts.begin() + val, _pts.begin() + val);
        remainderIt = _pts.erase(_pts.begin() + val, _pts.begin() + val);
        if (val == 0 || val == _pts.size() - 1) return false;
        std::vector<Point *> remainderList;
        while (remainderIt != _pts.end()) {
            remainderList.push_back(*remainderIt);
            remainderIt++;
        }
        if (!remainderList.empty()) remainder.push_back(remainderList);
        _pts.resize(val);
        return true;
    }

    // find intervals
    int first = points[0];
    for (size_t i = 1; i < points.size(); ++i) {
        if (i > points[i - 1] + 1) {
            first = points[i];
            intervals.push_back(std::make_pair(first, points[i - 1]));
        }
        if (i == points.size() - 1) {
            intervals.push_back(std::make_pair(first, points[i]));
        }
    }

    // free points, compute remaining strokes
    bool res = false;
    for (size_t i = 0; i < intervals.size(); i++) {
        int fromIdx = intervals[i].first;
        int toIdx = intervals[i].second;
        qDeleteAll(_pts.begin() + fromIdx, _pts.begin() + toIdx);
        if (fromIdx == 0 || toIdx == _pts.size() - 1) continue;
        res = true;
        remainderIt = ++(_pts.begin() + toIdx);
        if (i == intervals.size() - 1)
            next = _pts.end();
        else
            next = _pts.begin() + intervals[i + 1].first;
        std::vector<Point *> remainderList;
        // remainderList.reserve(intervals[i + 1].first);
        while (remainderIt != next) {
            remainderList.push_back(*remainderIt);
            remainderIt++;
        }
        if (!remainderList.empty()) remainder.push_back(remainderList);
    }
    if (intervals[0].first == 0) {
        _pts.erase(_pts.begin(), _pts.begin() + intervals[0].second);
        if (intervals.size() > 1) _pts.resize(intervals[1].first);
    } else {
        _pts.resize(intervals[0].first);
    }
    return res;
}

bool Polyline::removeSection(int from, int to, std::vector<Point *> &remainder) {
    for (size_t i = to+1; i < _pts.size(); i++) remainder.push_back(_pts[i]);
    qDeleteAll(_pts.begin() + from, _pts.begin() + (to+1));
    return remainder.size() > 1;
}

void Polyline::resample(Point::Scalar maxSampling, Point::Scalar minSampling, Polyline &resampledPolyline) {
    std::vector<Point *> outPts;
    std::vector<Point *> tmp;
    PiecewiseLinearMonotone origToCur(PiecewiseLinearMonotone::POSITIVE);

    const Point::Scalar radius = 2.;
    const Point::Scalar radiusSq = radius * radius;

    // First mark the points on the original curve we definitely want to keep (e.g., corners) using Douglas-Peucker
    std::vector<bool> keep = markDouglasPeucker(_pts, 3.);

    // Go through the points and resample them at the rate of radius, discarding those that are too close
    // to the previous output point, and subdiving the line segments between points that are too far.
    // Note that we don't want to simply resample the curve by the arclength parameterization, because
    // noisy regions with too much arclength will get too many samples.
    outPts.push_back(_pts[0]);
    origToCur.add(0, 0);
    Point::Scalar lengthSoFar = 0;
    for (size_t i = 1; i < _pts.size(); ++i) {
        Point *curResampled = outPts.back();
        Point *curPt = _pts[i];

        Point::Scalar distSqToCur = (curPt->pos() - curResampled->pos()).squaredNorm();

        // If it's the last point or was marked for keeping, just output it
        if (i + 1 == _pts.size() || keep[i]) {
            lengthSoFar += sqrt(distSqToCur);
            origToCur.add(idxToParam(i), lengthSoFar);
            if (distSqToCur > 1e-16) outPts.push_back(curPt);
            continue;
        }

        // If this point is within radius of the last output point (i.e., too close), discard it.
        if (distSqToCur < radiusSq) continue;

        // The previous point has to be within radius of the previous output point
        Point *prevPt = _pts[i - 1];

        // Find the intersection of the line segment between the previous and the current point with the circle
        // centered at the last output point whose radius is "radius".
        ParametrizedLine<Point::Scalar, 2> line = ParametrizedLine<Point::Scalar, 2>::Through(prevPt->pos(), curPt->pos());

        Point::Scalar projectedLineParam = line.direction().dot(curResampled->pos() - line.origin());
        Point::VectorType closestOnLine = line.origin() + line.direction() * projectedLineParam;
        Point::Scalar distSqToLine = (curResampled->pos() - closestOnLine).squaredNorm();
        if (distSqToLine < 1e-16) {
            lengthSoFar += sqrt(distSqToCur);
            origToCur.add(idxToParam(i), lengthSoFar);
            outPts.push_back(curPt);
            continue;
        }
        Point::Scalar y = std::sqrt(std::max(0.0, radiusSq - distSqToLine));
        Point::VectorType newPt = closestOnLine + y * line.direction();  // y is positive, so this will find the correct point

        lengthSoFar += (curResampled->pos() - newPt).norm();
        origToCur.add(idxToParam(i - 1) + projectedLineParam + y, lengthSoFar);
        Point *pt = point(idxToParam(i - 1) + projectedLineParam + y);
        pt->setPos(newPt);
        outPts.push_back(pt);
        tmp.push_back(pt);
        --i;
    }

    Polyline curve(outPts);
    for (Point *p : tmp) {
        delete p;
    }

    std::vector<Point::Scalar> samples;
    Point::Scalar prevParam = 0.;
    samples.push_back(prevParam);
    for (size_t i = 1; i < curve.pts().size(); ++i) {
        Point::Scalar nextParam = curve.idxToParam(i);
        Point::Scalar diffParam = nextParam - prevParam;
        if (diffParam > maxSampling) {
            for (int j = 1; j < qRound(diffParam / maxSampling); ++j) {
                Point::Scalar newParam = prevParam + j * maxSampling;
                samples.push_back(newParam);
            }
        }
        if (diffParam < minSampling) {
            int newI = i;
            for (int j = 1; i + j < curve.pts().size(); ++j) {
                newI = i + j;
                if ((curve.idxToParam(newI) - prevParam) >= minSampling) break;
            }
            nextParam = curve.idxToParam(newI);
            samples.push_back(nextParam);
            i = newI;
        }
        if (nextParam > (samples.back() + 0.1)) samples.push_back(nextParam);
        prevParam = nextParam;
    }

    resampledPolyline.clear();
    for (int i = 0; i < (int)samples.size(); ++i) {
        resampledPolyline.addPoint(point(samples[i]));
        // if (isnan((double)resampledPolyline._pts.back()->pos().x()) || isnan((double)resampledPolyline._pts.back()->pos().y())) qDebug() << "Error after resampling";
    }
}

void Polyline::updateLengths() {
    if (_pts.size() < 2) return;
    _lengths.clear();
    _lengths.resize(_pts.size());
    _lengths[0] = 0.0;
    for (int i = 1; i < _pts.size(); ++i) {
        _lengths[i] = _lengths[i - 1] + (_pts[i]->pos() - _pts[i - 1]->pos()).norm();
    }
}

std::vector<bool> Polyline::markDouglasPeucker(const std::vector<Point *> &pts, Point::Scalar cutoff) {
    std::vector<bool> out(pts.size(), false);
    out.front() = out.back() = true;

    dpHelper(pts, out, cutoff, 0, pts.size());
    return out;
}

void Polyline::dpHelper(const std::vector<Point *> &pts, std::vector<bool> &out, Point::Scalar cutoff, int start, int end) {
    ParametrizedLine<Point::Scalar, 2> cur = ParametrizedLine<Point::Scalar, 2>::Through(pts[start]->pos(), pts[end - 1]->pos());
    Point::Scalar maxDist = cutoff;
    int mid = -1;
    for (int i = start; i < end; ++i) {
        Point::Scalar dist = cur.distance(pts[i]->pos());
        if (dist > maxDist) {
            maxDist = dist;
            mid = i;
        }
    }
    if (mid < 0) return;
    out[mid] = true;
    dpHelper(pts, out, cutoff, start, mid + 1);
    dpHelper(pts, out, cutoff, mid, end);
}
};