/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "stroke.h"

#include <QtGui>
#include <QPainterPath>
#include <QLinearGradient>

#include "vectorkeyframe.h"
#include "dialsandknobs.h"
#include "qteigen.h"
#include "utils/stopwatch.h"

extern dkBool k_drawSplat;
extern dkFloat k_penFalloffMin;
extern dkSlider k_splatSamplingRate;

using namespace Frite;

static const unsigned int BUFFER_STRIDE = 8;

Stroke::Stroke(unsigned int id, const QColor &c, double thickness, bool _isInvisible) 
    : m_id(id),
      m_color(c), 
      m_strokeWidth(thickness), 
      m_isInvisible(_isInvisible),
      m_centroid(Point::VectorType::Zero()),
      m_centroidDirty(true),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ebo(QOpenGLBuffer::IndexBuffer),
      m_vboPoints(QOpenGLBuffer::VertexBuffer),
      m_eboPoints(QOpenGLBuffer::IndexBuffer),
      m_bufferCreated(false),
      m_bufferDestroyed(false) 
{ 
    
}

Stroke::Stroke(const Stroke &s)
    : m_points(s.m_points),
      m_color(s.m_color),
      m_strokeWidth(s.m_strokeWidth),
      m_isInvisible(s.m_isInvisible),
      m_id(s.m_id),
      m_centroid(s.m_centroid),
      m_centroidDirty(s.m_centroidDirty),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ebo(QOpenGLBuffer::IndexBuffer),
      m_vboPoints(QOpenGLBuffer::VertexBuffer),
      m_eboPoints(QOpenGLBuffer::IndexBuffer),
      m_bufferCreated(false),
      m_bufferDestroyed(false) 
{
}

Stroke::Stroke(const Stroke &s, unsigned int id, int from, int to) 
    : m_color(s.m_color),
      m_strokeWidth(s.m_strokeWidth),
      m_isInvisible(s.m_isInvisible),
      m_id(id),
      m_centroid(Point::VectorType::Zero()),
      m_centroidDirty(true),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ebo(QOpenGLBuffer::IndexBuffer),
      m_vboPoints(QOpenGLBuffer::VertexBuffer),
      m_eboPoints(QOpenGLBuffer::IndexBuffer),
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

Point::Scalar Stroke::length(int from, int to) const {
    return m_points.lengthFromTo(from, to);
}

void Stroke::createBuffers(QOpenGLShaderProgram *program, VectorKeyFrame *keyframe) {
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
    program->setAttributeBuffer(0, GL_FLOAT, 0, 2, BUFFER_STRIDE * sizeof(GLfloat));

    // pressure
    program->enableAttributeArray(1);
    program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 1, BUFFER_STRIDE * sizeof(GLfloat));

    // visibility
    program->enableAttributeArray(2);
    program->setAttributeBuffer(2, GL_FLOAT, 3 * sizeof(GLfloat), 1, BUFFER_STRIDE * sizeof(GLfloat));

    // color
    program->enableAttributeArray(3);
    program->setAttributeBuffer(3, GL_FLOAT, 4 * sizeof(GLfloat), 4, BUFFER_STRIDE * sizeof(GLfloat));

    m_vao.release();
    m_vbo.release();
    m_ebo.release();

    updateBuffer(keyframe);

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

void Stroke::updateBuffer(VectorKeyFrame *keyframe) {
    std::vector<GLfloat> data;
    std::vector<GLuint> dataElt;

    if (!k_drawSplat) {
        data.resize(size() * BUFFER_STRIDE);
        dataElt.resize(size() + 2);
        dataElt[0] = (GLuint)(0);
        for (size_t i = 0; i < size(); ++i) {
            data[BUFFER_STRIDE * i] = m_points.pts()[i]->pos().x();
            data[BUFFER_STRIDE * i + 1] = m_points.pts()[i]->pos().y();
            data[BUFFER_STRIDE * i + 2] = m_points.pts()[i]->pressure();
            data[BUFFER_STRIDE * i + 3] = keyframe->visibility().contains(Utils::cantor(m_id, i)) ? keyframe->visibility().value(Utils::cantor(m_id, i)) : 0.0;
            data[BUFFER_STRIDE * i + 4] = m_points.pts()[i]->getColor().redF();
            data[BUFFER_STRIDE * i + 5] = m_points.pts()[i]->getColor().greenF();
            data[BUFFER_STRIDE * i + 6] = m_points.pts()[i]->getColor().blueF();
            data[BUFFER_STRIDE * i + 7] = m_points.pts()[i]->getColor().alphaF();
            dataElt[i + 1] = (GLuint)i;
        }
        dataElt[size() + 1] = (GLuint)(size() - 1);

        // Apply spacing function on point visibility
        for (Group *group : keyframe->postGroups()) {
            if (!group->strokes().contains(m_id)) continue;
            for (const Interval &interval : group->strokes().value(m_id)) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    if (data[BUFFER_STRIDE * i + 3] >= -1.0 && keyframe->visibility().contains(Utils::cantor(m_id, i))) {
                        data[BUFFER_STRIDE * i + 3] = Utils::sgn(keyframe->visibility()[Utils::cantor(m_id, i)]) * group->spacingAlpha(std::abs(keyframe->visibility()[Utils::cantor(m_id, i)]));
                    }
                }
            }
        }
    } else {
        double s = k_splatSamplingRate / 10.0;
        int maxStep = std::ceil(length() / s);
        data.resize(maxStep * BUFFER_STRIDE);
        dataElt.resize(maxStep);
        Point::VectorType pos;
        double pressure, curParam, curViz, nextViz;
        Point::Scalar outParam;
        int lastIdx, nextIdx;
        QColor col;
        for (int i = 0; i < maxStep; ++i) {
            curParam = std::min(length(), i * s);
            m_points.sample(curParam, pos, pressure, col);
            lastIdx = m_points.paramToIdx(curParam, &outParam);
            nextIdx = (lastIdx + 1 >= m_points.size()) ? lastIdx : lastIdx + 1;
            curViz = keyframe->visibility().contains(Utils::cantor(m_id, lastIdx)) ? keyframe->visibility().value(Utils::cantor(m_id, lastIdx)) : 0.0;
            nextViz = keyframe->visibility().contains(Utils::cantor(m_id, nextIdx)) ? keyframe->visibility().value(Utils::cantor(m_id, nextIdx)) : 0.0;
            data[BUFFER_STRIDE * i] = pos.x();
            data[BUFFER_STRIDE * i + 1] = pos.y();
            data[BUFFER_STRIDE * i + 2] = pressure;
            data[BUFFER_STRIDE * i + 3] = curViz;/*nextIdx == lastIdx ? curViz : curViz + (nextViz - curViz) * (outParam / (m_points.idxToParam(nextIdx) - m_points.idxToParam(lastIdx)));*/
            data[BUFFER_STRIDE * i + 4] = col.redF();
            data[BUFFER_STRIDE * i + 5] = col.greenF();
            data[BUFFER_STRIDE * i + 6] = col.blueF();
            data[BUFFER_STRIDE * i + 7] = col.alphaF();
            dataElt[i] = (GLuint)i;
        }

        // Apply spacing function on point visibility
        for (Group *group : keyframe->postGroups()) {
            if (!group->strokes().contains(m_id)) continue;
            for (const Interval &interval : group->strokes().value(m_id)) {
                int paramA = std::round(m_points.idxToParam(interval.from()) / s);
                int paramB = std::min((int)std::round(m_points.idxToParam(interval.to()) / s), maxStep - 1);
                for (unsigned int i = paramA; i <= paramB; ++i) {
                    curParam = std::min(length(), i * s);
                    lastIdx = m_points.paramToIdx(curParam);
                    if (data[8 * i + 3] >= -1.0 && keyframe->visibility().contains(Utils::cantor(m_id, lastIdx))) {
                        data[8 * i + 3] = Utils::sgn(keyframe->visibility()[Utils::cantor(m_id, lastIdx)]) * group->spacingAlpha(std::abs(keyframe->visibility()[Utils::cantor(m_id, lastIdx)]));
                    }
                }
            }
        }
    }

    m_vbo.bind(); 
    m_vbo.allocate(data.data(), data.size() * sizeof(GLfloat));
    m_vbo.release();

    m_ebo.bind(); 
    m_ebo.allocate(dataElt.data(), dataElt.size() * sizeof(GLuint));
    m_ebo.release();
}

void Stroke::render(GLenum mode, QOpenGLFunctions *functions) {
    m_vao.bind();
    if (!k_drawSplat) {
        functions->glDrawElements(mode, size() + 2, GL_UNSIGNED_INT, nullptr);
    } else {
        double s = k_splatSamplingRate / 10.0;
        functions->glDrawElements(GL_POINTS, std::ceil(length() / s), GL_UNSIGNED_INT, nullptr);
    }                
    m_vao.release();
}

void Stroke::render(GLenum mode, QOpenGLFunctions *functions, const Interval &interval, bool overshoot) {
    m_vao.bind();
    if (!k_drawSplat) {
        GLsizei count = interval.to() - interval.from() + 3;
        if (overshoot && interval.canOvershoot() && interval.to() < size() - 1) count += 1;
        functions->glDrawElements(GL_LINE_STRIP_ADJACENCY, count, GL_UNSIGNED_INT, (const void *)(interval.from() * sizeof(GL_UNSIGNED_INT)));
    } else {
        double s = k_splatSamplingRate / 10.0;
        double maxStep = std::ceil(length() / s);
        int paramA = std::round(m_points.idxToParam(interval.from()) / s);
        int paramB = std::min(std::round(m_points.idxToParam(interval.to()) / s), maxStep);
        int count = std::min(std::round((paramB - paramA) + 1), maxStep - paramA);
        if (overshoot && interval.canOvershoot() && paramB < maxStep - 1) count += 1;
        functions->glDrawElements(GL_POINTS, count, GL_UNSIGNED_INT, (const void *)(paramA * sizeof(GL_UNSIGNED_INT)));
    }
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
    strokeElt.setAttribute("invisible", m_isInvisible);
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
    if (m_isInvisible) {
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