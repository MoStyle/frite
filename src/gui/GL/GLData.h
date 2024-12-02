#ifndef GLDATA_H
#define GLDATA_H

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

#include "strokeinterval.h"

class VectorKeyFrame;

struct GLStrokesData : QOpenGLFunctions {
    GLStrokesData() : m_vbo(QOpenGLBuffer::VertexBuffer), m_ebo(QOpenGLBuffer::IndexBuffer) { }

    void create(QOpenGLShaderProgram *program);
    void update(VectorKeyFrame *keyframe, const StrokeIntervals &strokes, float weightModifier=1.0f, bool overrideStrokeColor=false, const QColor &overrideColor=Qt::black);
    void destroy();
    void render(GLenum mode=GL_LINE_STRIP_ADJACENCY);

    GLsizei m_size;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_ebo;
};

struct GLDisplayQuadData : QOpenGLFunctions {
    GLDisplayQuadData() : m_vbo(QOpenGLBuffer::VertexBuffer) { }

    void create(QOpenGLShaderProgram *program);
    void destroy();
    void render(GLenum mode=GL_LINE_STRIP_ADJACENCY);

    GLsizei m_size;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
};

#endif // GLDATA_H