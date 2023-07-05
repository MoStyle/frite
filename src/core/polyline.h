/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef POLYLINE_H
#define POLYLINE_H

#include <QTransform>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <vector>
#include <memory>

#include "point.h"

// Must contain at least two points
// Originally from cornucopia's implementation of polylines
namespace Frite {
class Polyline {
   public:
    Polyline() { }
    Polyline(const std::vector<Point *> &pts);
    Polyline(const Polyline &polyline);
    virtual ~Polyline();

    void addPoint(Point *point);
    void load(QTextStream &posStream, size_t size);
    void clear();
    Point::Scalar length() const { return _lengths.back(); }
    size_t size() const { return _pts.size(); }
    const std::vector<Point *> &pts() const { return _pts; }
    std::vector<Point *> &pts() { return _pts; }

    // Evaluation wrappers
    Point::VectorType pos(Point::Scalar s) const {
        Point::VectorType out;
        eval(s, &out);
        return out;
    }

    Point::VectorType der(Point::Scalar s) const {
        Point::VectorType out;
        eval(s, nullptr, &out);
        return out;
    }

    Point::VectorType der2(Point::Scalar s) const {
        Point::VectorType out;
        eval(s, nullptr, nullptr, &out);
        return out;
    }

    Point *point(Point::Scalar s) const {
        Point *out = new Point();
        eval(s, nullptr, nullptr, nullptr, out);
        return out;
    }

    Point::Scalar angle(Point::Scalar s) const {
        Point::VectorType d = der(s);
        return std::atan2(d[1], d[0]);
    }

    Point::Scalar curvature(Point::Scalar s) const {
        Point::VectorType d, d2;
        eval(s, nullptr, &d, &d2);
        return d.dot(Point::VectorType(d2[1], -d2[0]));
    }

    // Utility functions
    int paramToIdx(Point::Scalar param, Point::Scalar *outParam = nullptr) const;
    Point::Scalar idxToParam(int idx) const { return _lengths[idx]; }
    bool isParamValid(Point::Scalar param) const { return param >= 0 && param <= _lengths.back(); }
    int pointToIdx(const Point *point) const;
    Point::Scalar lengthFromTo(int fromIdx, int toIdx) const;
    Point::VectorType startPos() const { return pos(0); }
    Point::VectorType endPos() const { return pos(length()); }
    Point::Scalar project(const Point::VectorType &point) const;
    Point::Scalar distanceSqTo(const Point::VectorType &point) const { return (point - pos(project(point))).squaredNorm(); }
    Point::Scalar distanceTo(const Point::VectorType &point) const { return sqrt(distanceSqTo(point)); }

    // Construct the sub polyline from the parameter values [from, to]
    void trimmed(Point::Scalar from, Point::Scalar to, Polyline &trimmedPoly) const;

    // Construct the sub polyline from the points indices [from, to]
    void subPoly(int from, int to, Polyline &subPoly) const;

    // Points must be consecutive (interval in the _pts list)
    bool removeSection(std::vector<Point *> &points, std::vector<Point *> &remainder);
    bool removeSection(std::vector<int> &points, std::vector<std::vector<Point *>> &remainder);
    bool removeSection(int from, int to, std::vector<Point *> &remainder);

    void resample(Point::Scalar maxSampling, Point::Scalar minSampling, Polyline &resampledPolyline);
    void updateLengths();

   protected:
    // Evaluates the polyline with optionally the first and second derivatives (tangent and curvature)
    void eval(Point::Scalar s, Point::VectorType *pos, Point::VectorType *der = nullptr, Point::VectorType *der2 = nullptr, Point *point = nullptr) const;
    
    // Polyline simplification
    std::vector<bool> markDouglasPeucker(const std::vector<Point *> &pts, Point::Scalar cutoff);
    void dpHelper(const std::vector<Point *> &pts, std::vector<bool> &out, Point::Scalar cutoff, int start, int end);

   private:
    std::vector<Point *> _pts;

    // lengths[x] = \sum_{i=1}^{i=x} ||pts[i]-pts[i-1]||, i.e., length up to point x
    std::vector<Point::Scalar> _lengths;
};
}

#endif
