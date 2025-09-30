/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

 #include "GLData.h"

#include "vectorkeyframe.h"
#include "utils/stopwatch.h"

// GLStrokesData

void GLStrokesData::create(QOpenGLShaderProgram *program) {
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
    program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 8*sizeof(GLfloat));

    // pressure
    program->enableAttributeArray(1);
    program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 1, 8*sizeof(GLfloat));

    // caps
    program->enableAttributeArray(2);
    program->setAttributeBuffer(2, GL_FLOAT, 3 * sizeof(GLfloat), 1, 8*sizeof(GLfloat));

    // color
    program->enableAttributeArray(3); 
    program->setAttributeBuffer(3, GL_FLOAT, 4 * sizeof(GLfloat), 4, 8*sizeof(GLfloat));

    m_vao.release();
    m_vbo.release();
    m_ebo.release();
}

void GLStrokesData::update(VectorKeyFrame *keyframe, const StrokeIntervals &strokes, float weightModifier, bool overrideStrokeColor, const QColor &overrideColor) {
    std::vector<GLfloat> data;
    std::vector<GLuint> dataIdx;
    int nbPoints = strokes.nbPoints();
    int pIdx = 0;
    QColor color;
    data.reserve(nbPoints * 4);
    dataIdx.reserve(nbPoints + strokes.nbIntervals());
    for (auto it = strokes.begin(); it != strokes.end(); ++it) {
        Stroke *stroke = keyframe->stroke(it.key()); 
        color = overrideStrokeColor ? overrideColor : stroke->color(); 
        const Intervals &intervals = it.value();
        for (const Interval &interval : intervals) {
            for (unsigned int i = interval.from(); i <= interval.to(); i++) {
                data.push_back(stroke->points()[i]->pos().x());
                data.push_back(stroke->points()[i]->pos().y());
                data.push_back(stroke->points()[i]->pressure() * weightModifier);
                data.push_back((i == 0 || i == stroke->size() - 1) ? 1.0f : 0.0f);
                data.push_back(color.redF());
                data.push_back(color.greenF());
                data.push_back(color.blueF());
                data.push_back(color.alphaF());
                dataIdx.push_back((GLuint)pIdx);
                pIdx++;
            }
            dataIdx.push_back((GLuint)(pIdx - 1));
        }
    }

    m_vbo.bind(); 
    m_vbo.allocate(data.data(), data.size() * sizeof(GLfloat));
    m_vbo.release();

    m_ebo.bind(); 
    m_ebo.allocate(dataIdx.data(), dataIdx.size() * sizeof(GLuint));
    m_ebo.release();
}

void GLStrokesData::destroy() {
    StopWatch s("Destroying stroke buffers");
    m_ebo.destroy();
    m_vbo.destroy();
    m_vao.destroy();
    s.stop();
}

void GLStrokesData::render(GLenum mode) {
    m_vao.bind();
    glDrawElements(mode, m_size, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
}

// GLDisplayQuadData

void GLDisplayQuadData::create(QOpenGLShaderProgram *program) {
    m_vao.create();
    if (m_vao.isCreated()) m_vao.bind();
    static const GLfloat quadBufferData[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vbo.allocate(quadBufferData, sizeof(quadBufferData));
    int vertexLocation = program->attributeLocation("vertex");
    program->enableAttributeArray(vertexLocation);
    program->setAttributeBuffer(vertexLocation, GL_FLOAT, 0, 2);
    m_vbo.release();
    m_vao.release();
}

void GLDisplayQuadData::destroy() {
    m_vbo.destroy();
    m_vao.destroy();
}

void GLDisplayQuadData::render(GLenum mode) {

}