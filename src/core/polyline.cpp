#include "polyline.h"

#include "utils/geom.h"

#include <QDebug>
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
    for (const Point *p : _pts) {
        delete p;
    }
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
    Point::Scalar t = cParam * invLength;
    Point::Scalar tComp = 1.0 - t; 
    if (pos) (*pos) = tComp * _pts.at(idx)->pos() + t * _pts.at(nextIdx)->pos();
    if (der) (*der) = (_pts.at(nextIdx)->pos() - _pts.at(idx)->pos()) * invLength;
    if (der2) (*der2) = Point::VectorType();  // (*der2) = 6 * ((1 - t) * _pts[0] + t * _pts[3] + (3 * t - 2) * _pts[1] + (1 - 3 * t) * _pts[2]);
    if (point) {
        // Linearly interpolate all point properties
        (*point).setPos(tComp * _pts.at(idx)->pos() + t * _pts.at(nextIdx)->pos());
        (*point).setTemporalW(tComp * _pts.at(idx)->temporalW() + t * _pts.at(nextIdx)->temporalW());
        (*point).setInterval(tComp * _pts.at(idx)->interval() + t * _pts.at(nextIdx)->interval());
        (*point).setPressure(tComp * _pts.at(idx)->pressure() + t * _pts.at(nextIdx)->pressure());
        (*point).setPressure(tComp * _pts.at(idx)->pressure() + t * _pts.at(nextIdx)->pressure());
        (*point).setColor(QColor(
            tComp * _pts.at(idx)->getColor().redF() + t * _pts.at(nextIdx)->getColor().redF(),
            tComp * _pts.at(idx)->getColor().greenF() + t * _pts.at(nextIdx)->getColor().greenF(),
            tComp * _pts.at(idx)->getColor().blueF() + t * _pts.at(nextIdx)->getColor().blueF()
        ));
        // TODO: compute normals
        // also m_interval?
    }
}

void Polyline::eval(Point::Scalar s, Point::VectorType &outPos, Point::Scalar &outPressure, QColor &outColor) const {
    Point::Scalar cParam;
    int idx = paramToIdx(s, &cParam);
    int nextIdx = (idx + 1) % _pts.size();
    Point::Scalar dist = _lengths[idx + 1] - _lengths[idx];
    Point::Scalar invLength = dist == 0.0 ? 1.0 : (1.0 / dist);
    Point::Scalar t = cParam * invLength;
    Point::Scalar tComp = 1.0 - t;
    outPos = tComp * _pts.at(idx)->pos() + t * _pts.at(nextIdx)->pos();
    outPressure = tComp * _pts.at(idx)->pressure() + t * _pts.at(nextIdx)->pressure();
    outColor.setRgbF(
        tComp * _pts.at(idx)->getColor().redF() + t * _pts.at(nextIdx)->getColor().redF(),
        tComp * _pts.at(idx)->getColor().greenF() + t * _pts.at(nextIdx)->getColor().greenF(),
        tComp * _pts.at(idx)->getColor().blueF() + t * _pts.at(nextIdx)->getColor().blueF()
    );
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
    resampledPolyline.clear();
    const Point::Scalar radius = minSampling;
    const Point::Scalar radiusSq = radius * radius;

    // First mark the points on the original curve we definitely want to keep (e.g., corners) using Douglas-Peucker
    std::vector<bool> keep = markDouglasPeucker(.25);

    resampledPolyline.addPoint(new Point(*_pts[0]));
    Point::Scalar lengthSoFar = 0;
    for (size_t i = 1; i < _pts.size(); ++i) {
        Point *curResampled = resampledPolyline._pts[resampledPolyline.size() - 1];
        Point *curPt = _pts[i];

        Point::Scalar distSqToCur = (curPt->pos() - curResampled->pos()).squaredNorm();
        Point::Scalar dist;

        // If it's the last point or it was marked by douglas-peucker, keep it
        if (i + 1 == _pts.size() || keep[i]) {
            dist = sqrt(distSqToCur);
            lengthSoFar += dist;
            if (dist > 1e-16) {
                if (dist > radius) {
                    size_t count = dist / radius;
                    Point::Scalar s = dist / count; 
                    Point::VectorType m0, m1;
                    if (i == 1) m0 = ((_pts[1]->pos() - _pts[0]->pos()) - (_pts[2]->pos() - _pts[1]->pos()) ) / 2.0;
                    else        m0 = (_pts[i]->pos() - _pts[i - 2]->pos()) / 2.0;
                    if (i == _pts.size() - 1)   m1 = ((_pts[_pts.size() - 1]->pos() - _pts[_pts.size() - 2]->pos()) - (_pts[_pts.size() - 2]->pos() - _pts[_pts.size() - 3]->pos()) ) / 2.0;
                    else                        m1 = (_pts[i + 1]->pos() - _pts[i - 1]->pos()) / 2.0;
                    for (size_t j = 0; j < count - 1; ++j) {
                        double a = ((j + 1) * radius) / (dist);
                        Point::VectorType smoothInterpPos =  Geom::evalCubicHermite((j + 1) * s, 0.0, dist, curResampled->pos(), m0, curPt->pos(), m1);
                        Point *newPoint = new Point(smoothInterpPos.x(), smoothInterpPos.y(), curPt->interval() * a + curResampled->interval() * (1.0 - a), curPt->pressure() * a + curResampled->pressure() * (1.0 - a));
                        resampledPolyline.addPoint(newPoint);
                    }
                }
                resampledPolyline.addPoint(new Point(*curPt));
            }
        }
    }
}

void Polyline::smoothPressure() {
    std::vector<double> pressures(_pts.size());
    for (int i = 0; i < _pts.size(); ++i) {
        pressures[i] = _pts[i]->pressure();
    }
    for (int i = 1; i < _pts.size() - 1; ++i) {
        _pts[i]->setPressure((pressures[i - 1] + pressures[i] + pressures[i + 1]) / 3.0);
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

std::vector<bool> Polyline::markDouglasPeucker(Point::Scalar cutoff) {
    std::vector<bool> out(_pts.size(), false);
    out.front() = out.back() = true;
    dpHelper(out, cutoff, 0, _pts.size());
    return out;
}

void Polyline::dpHelper(std::vector<bool> &out, Point::Scalar cutoff, int start, int end) {
    ParametrizedLine<Point::Scalar, 2> cur = ParametrizedLine<Point::Scalar, 2>::Through(_pts[start]->pos(), _pts[end - 1]->pos());
    Point::Scalar maxDist = cutoff;
    int mid = -1;
    for (int i = start; i < end; ++i) {
        Point::Scalar dist = cur.distance(_pts[i]->pos());
        if (dist > maxDist) {
            maxDist = dist;
            mid = i;
        }
    }
    if (mid < 0) return;
    out[mid] = true;
    dpHelper(out, cutoff, start, mid + 1);
    dpHelper(out, cutoff, mid, end);
}

std::vector<Point::Scalar> Polyline::resampleArclength(double minSampling, double maxSampling) {
    std::vector<Point::Scalar> samples;
    samples.reserve(std::ceil(length() / maxSampling));
    Point::Scalar prevParam = 0.;
    samples.push_back(prevParam);
    for (size_t i = 1; i < _pts.size(); ++i) {
        Point::Scalar nextParam = idxToParam(i);
        Point::Scalar diffParam = nextParam - prevParam;
        // Parameter gap too big: add as many new samples as necessary to keep the gap < maxSampling 
        if (diffParam > maxSampling) {
            for (int j = 1; j < qRound(diffParam / maxSampling); ++j) {
                Point::Scalar newParam = prevParam + j * maxSampling;
                samples.push_back(newParam);
            }
        }
        // Parameter gap too small: remove as many samples as necessary to keep the gap < minSampling
        if (diffParam < minSampling) {
            int newI = i;
            for (int j = 1; i + j < _pts.size(); ++j) {
                newI = i + j;
                if ((idxToParam(newI) - prevParam) >= minSampling) break;
            }
            nextParam = idxToParam(newI);
            samples.push_back(nextParam);
            i = newI;
        }
        if (nextParam > (samples.back() + 0.1)) samples.push_back(nextParam);
        prevParam = nextParam;
    }
    return samples;
}

};