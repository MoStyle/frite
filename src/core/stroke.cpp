/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "stroke.h"

#include <QtGui>
#include <QPainterPath>
#include <QLinearGradient>
#include "dialsandknobs.h"
#include "qteigen.h"
#include "utils/stopwatch.h"

Stroke::Stroke(unsigned int id, const QColor &c, double thickness, bool _isScribble) 
    : m_id(id),
      m_color(c), 
      m_strokeWidth(thickness), 
      m_isScribble(_isScribble),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ebo(QOpenGLBuffer::IndexBuffer),
      m_bufferCreated(false),
      m_bufferDestroyed(false) 
{ 
    
}

Stroke::Stroke(const Stroke &s)
    : m_points(s.m_points),
      m_color(s.m_color),
      m_strokeWidth(s.m_strokeWidth),
      m_isScribble(s.m_isScribble),
      m_id(s.m_id),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ebo(QOpenGLBuffer::IndexBuffer),
      m_bufferCreated(false),
      m_bufferDestroyed(false) 
{
}

Stroke::Stroke(const Stroke &s, unsigned int id, int from, int to) 
    : m_color(s.m_color),
      m_strokeWidth(s.m_strokeWidth),
      m_isScribble(s.m_isScribble),
      m_id(id),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ebo(QOpenGLBuffer::IndexBuffer),
      m_bufferCreated(false),
      m_bufferDestroyed(false) {
    s.m_points.subPoly(from, to, m_points);
}

Stroke::~Stroke() {
    if (m_bufferCreated && !m_bufferDestroyed) qDebug() << "Stroke buffer is not destroyed!";
}

Point::Scalar Stroke::length() const {
    return m_points.length();
}

void Stroke::createBuffers(QOpenGLShaderProgram *program) {
    if (m_bufferCreated) return;

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_ebo.create();
    m_ebo.bind();
    m_ebo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // vtx
    program->enableAttributeArray(0); 
    program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4*sizeof(GLfloat));

    // pressure
    program->enableAttributeArray(1);
    program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 1, 4*sizeof(GLfloat));

    // start or end vtx
    program->enableAttributeArray(2);
    program->setAttributeBuffer(2, GL_FLOAT, 3 * sizeof(GLfloat), 1, 4*sizeof(GLfloat));

    m_vao.release();
    m_vbo.release();
    m_ebo.release();

    updateBuffer();

    m_bufferCreated = true;
    m_bufferDestroyed = false;
}

void Stroke::destroyBuffers() {
    if (!m_bufferCreated) return;
    StopWatch s("Destroying buffers");
    m_ebo.destroy();
    m_vbo.destroy();
    m_vao.destroy();
    m_bufferDestroyed = true;
    m_bufferCreated = false;
    s.stop();
}

void Stroke::updateBuffer() {
    std::vector<GLfloat> data;
    std::vector<GLuint> dataElt;
    data.resize(size()*4);
    dataElt.resize(size() + 1);

    for (size_t i = 0; i < size(); ++i) {
        data[4 * i] = m_points.pts()[i]->pos().x();
        data[4 * i + 1] = m_points.pts()[i]->pos().y();
        data[4 * i + 2] = m_points.pts()[i]->pressure();
        data[4 * i + 3] = (i == 0 || i == size() - 1) ? 1.0f : 0.0f; // TODO: not just stroke endpoints but we should mark all *intervals* endpoints
        dataElt[i] = (GLuint)i;
    }
    dataElt[size()] = (GLuint)(size() - 1);

    m_vbo.bind(); 
    m_vbo.allocate(data.data(), data.size() * sizeof(GLfloat));
    m_vbo.release();

    m_ebo.bind(); 
    m_ebo.allocate(dataElt.data(), dataElt.size() * sizeof(GLuint));
    m_ebo.release();
}

void Stroke::render(GLenum mode, QOpenGLFunctions *functions) {
    m_vao.bind();
    functions->glDrawElements(mode, size()+1, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
}

void Stroke::render(GLenum mode, QOpenGLFunctions *functions, const Interval &interval, bool overshoot) {
    GLsizei count = interval.to() - interval.from() + 2; // |ebo|=|vbo|+1
    if (overshoot && interval.canOvershoot() && interval.to() < size() - 1) count += 1;
    m_vao.bind();
    functions->glDrawElements(mode, count, GL_UNSIGNED_INT, (const void *)(interval.from() * sizeof(GL_UNSIGNED_INT)));
    m_vao.release();
}

void Stroke::addPoint(Point *point) { 
    m_points.addPoint(point);
}

void Stroke::load(QTextStream &posStream, size_t size) { 
    m_points.load(posStream, size); 
}

void Stroke::save(QDomDocument &doc, QDomElement strokesElt) const {
    QDomElement strokeElt = doc.createElement("stroke");
    strokeElt.setAttribute("id", uint(m_id));
    strokeElt.setAttribute("size", uint(points().size()));
    strokeElt.setAttribute("color", QString::number(m_color.rgba(), 16));
    strokeElt.setAttribute("thickness", m_strokeWidth);
    QString string;
    QTextStream startPos(&string);
    for (const Point *p : points()) {
        startPos << p->x() << " " << p->y() << " " << p->interval() << " " << p->pressure() << " ";
    }
    QDomText txt = doc.createTextNode(string);
    strokeElt.appendChild(txt);
    strokesElt.appendChild(strokeElt);
}

void Stroke::draw(QPainter &painter, QPen &pen, int fromIdx, int toIdx, qreal scaleFactor, bool overshoot) const {
    if (scaleFactor * m_strokeWidth < 0.1f  || fromIdx == toIdx) return;
    
    int end = (toIdx == size() - 1 || !overshoot) ? toIdx - 1 : toIdx; 
    for (size_t i = fromIdx; i <= end; ++i) {
        QPointF lastPoint = QPointF(m_points.pts()[i]->x(), m_points.pts()[i]->y());
        QPointF newPoint = QPointF(m_points.pts()[i + 1]->x(), m_points.pts()[i + 1]->y());
        double p = m_points.pts()[i + 1]->pressure();
        pen.setWidthF(p * m_strokeWidth * scaleFactor);
        painter.setPen(pen);
        painter.drawLine(lastPoint, newPoint);
    }
}

void Stroke::drawPolygon(QPainter &painter, QPen &pen, bool useGroupColor) const {
    // temporary, uniformize the drawing procedure between normal strokes and scribbles
    // draw scribble as qpath
    if (m_isScribble) {
        drawAsScribble(painter, pen);
        return;
    }

    // draw normal stroke as series of line
    painter.setPen(pen);
    painter.setBrush(Qt::SolidPattern);
    painter.setBrush(pen.color());
    painter.drawPolygon(m_outline, Qt::WindingFill);
    pen.setWidthF(1);
    pen.setCosmetic(true);
    Point *first = m_points.pts()[0];
    Point *last = m_points.pts().back();
    painter.drawEllipse(QPointF(first->x(), first->y()), (first->pressure() * m_strokeWidth + 1) * 0.5f,
                        (first->pressure() * m_strokeWidth + 1) * 0.5f);
    painter.drawEllipse(QPointF(last->x(), last->y()), (last->pressure() * m_strokeWidth + 1) * 0.5f,
                        (last->pressure() * m_strokeWidth + 1) * 0.5f);
}

void Stroke::drawAsScribble(QPainter &painter, QPen &pen) const {
    pen.setWidthF(m_strokeWidth);
    painter.setPen(pen);
    QPainterPath path(QPointF(m_points.pts()[0]->pos().x(), m_points.pts()[0]->pos().y()));
    for (size_t i = 1; i < m_points.size(); i++) {
        path.lineTo(QPointF(m_points.pts()[i]->pos().x(), m_points.pts()[i]->pos().y()));
    }
    painter.drawPath(path);
}

void Stroke::computeNormals() {
    for (size_t i = 0; i < m_points.size(); i++) {
        Point *newPoint = m_points.pts()[i];
        Point *lastPoint, *nextPoint;
        Point::VectorType normal1 = Point::VectorType::Zero(), normal2 = Point::VectorType::Zero();
        if (i < m_points.size() - 1) {
            nextPoint = m_points.pts()[i + 1];
            Point::VectorType t2 = (nextPoint->pos() - newPoint->pos());
            double norm = t2.norm();
            if (norm < 1e-6) continue;
            normal1 = Point::VectorType(-t2.y(), t2.x()) / std::sqrt(norm);
        }
        if (i > 0) {
            lastPoint = m_points.pts()[i - 1];
            Point::VectorType t1 = (newPoint->pos() - lastPoint->pos());
            double norm = t1.norm();
            if (norm < 1e-6) continue;
            normal2 = Point::VectorType(-t1.y(), t1.x()) / std::sqrt(norm);
        }
        newPoint->normal() = (normal1 + normal2).normalized();
    }
}

void Stroke::computeOutline() {
    QPolygonF bottom_outline;
    const std::vector<Point *> &pts = m_points.pts();
    double p = pts[0]->pressure();
    double thickness = (p * m_strokeWidth + 1) * 0.5f;
    QPointF normal(pts[0]->normal().x(), pts[0]->normal().y());
    m_outline << QPointF(pts[0]->x(), pts[0]->y()) + normal * thickness;
    bottom_outline << QPointF(pts[0]->x(), pts[0]->y()) - normal * thickness;
    for (size_t j = 1; j < pts.size(); j++) {
        const Point *point = pts[j];
        p = point->pressure();
        thickness = (p * m_strokeWidth + 1) * 0.5;

        QPointF pos(point->x(), point->y());
        normal = QPointF(point->normal().x(), point->normal().y());

        m_outline << pos + normal * thickness;
        bottom_outline << pos - normal * thickness;
    }
    std::reverse(bottom_outline.begin(), bottom_outline.end());
    // m_outline << QPointF(pts.back()->x(), pts.back()->y());
    m_outline.append(bottom_outline);
}

void Stroke::trimmed(Point::Scalar from, Point::Scalar to, const std::shared_ptr<Stroke> &trimmedStroke) const {
    m_points.trimmed(from, to, trimmedStroke->polyline());
}

void Stroke::subPoly(int from, int to, const std::shared_ptr<Stroke> &trimmedStroke) const {
    m_points.subPoly(from, to, trimmedStroke->polyline());
}