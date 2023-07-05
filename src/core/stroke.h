/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef STROKE_H
#define STROKE_H

#include <QTransform>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "point.h"
#include "group.h"
#include "polyline.h"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QTextStream>
#include <QtXml>
#include <QDomElement>

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>

class Point;
class Group;

class Stroke {
   public:
    Stroke(unsigned int id, const QColor &c, double thickness = 1.5, bool _isScribble = false);
    Stroke(const Stroke &s);
    Stroke(const Stroke &s, unsigned int id, int from, int to);
    ~Stroke();

    const std::vector<Point *> &points() const { return m_points.pts(); }
    std::vector<Point *> &points() { return m_points.pts(); }
    Frite::Polyline &polyline() { return m_points; }
    const Frite::Polyline &polyline() const { return m_points; }
    QColor color() const { return m_color; }
    bool isScribble() const { return m_isScribble; }
    double strokeWidth() const { return m_strokeWidth; }
    void setStrokeWidth(double width) { m_strokeWidth = width; }
    Point::Scalar length() const;
    size_t size() const { return m_points.pts().size(); }
    unsigned int id() const { return m_id; }
    int canHashId() const { return m_canHashId; }
    void resetID(unsigned int id) { m_id = id; }

    // OpenGL stuff
    void createBuffers(QOpenGLShaderProgram *program);
    void destroyBuffers();
    void updateBuffer();
    void render(GLenum mode, QOpenGLFunctions *functions);
    void render(GLenum mode, QOpenGLFunctions *functions, const Interval &interval, bool overshoot);
    bool buffersCreated() const { return m_bufferCreated; }

    void addPoint(Point *point);
    void setColor(const QColor &color) { m_color = color; }
    void setPolyline(const Frite::Polyline &polyline) { m_points = polyline; }
    void setCanHashId(int id) { m_canHashId = id; }

    void load(QTextStream &posStream, size_t size);
    void save(QDomDocument &doc, QDomElement strokesElt) const;
    void draw(QPainter &painter, QPen &pen, int fromIdx, int toIdx, qreal scaleFactor = 1.0f, bool overshoot=true) const;
    void drawPolygon(QPainter &painter, QPen &pen, bool useGroupColor = false) const;
    void drawAsScribble(QPainter &painter, QPen &pen) const;
    void computeNormals();
    void computeOutline();
    void trimmed(Point::Scalar from, Point::Scalar to, const std::shared_ptr<Stroke> &trimmedStroke) const;
    void subPoly(int from, int to, const std::shared_ptr<Stroke> &trimmedStroke) const;

    std::shared_ptr<Stroke> resample(Point::Scalar maxSampling, Point::Scalar minSampling) {
        std::shared_ptr<Stroke> resampledStroke = std::make_shared<Stroke>(*this);
        m_points.resample(maxSampling, minSampling, resampledStroke->polyline());
        return resampledStroke;
    }

    inline Point::VectorType centroid() const {
        Point::VectorType center = Point::VectorType::Zero();
        for (Point *point : m_points.pts()) {
            center += point->pos();
        }
        return center / m_points.size();
    }

    inline void transform(const Eigen::Affine2d &T) {
        for (Point *point : m_points.pts()) {
            point->pos() = T * point->pos();
        }
    }

   private:
    Frite::Polyline m_points;

    // stroke properties
    QColor m_color;
    double m_strokeWidth;
    bool m_isScribble;
    QPolygonF m_outline;

    // buffers
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo, m_ebo;
    bool m_bufferCreated, m_bufferDestroyed;

    unsigned int m_id;
    int m_canHashId = -1;
};

typedef std::shared_ptr<Stroke> StrokePtr;

#endif