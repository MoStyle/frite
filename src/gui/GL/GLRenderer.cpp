/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "GLRenderer.h"

#include "editor.h"
#include "tabletcanvas.h"
#include "viewmanager.h"
#include "dialsandknobs.h"

extern dkBool k_drawOffscreen;
extern dkBool k_drawTess;
extern dkBool k_drawSplat;
extern dkFloat k_thetaEps;

GLRenderer::GLRenderer() : m_offscreenRenderMSFBO(nullptr), m_offscreenRenderFBO(nullptr) {

}

void GLRenderer::initialize(int w, int h) {
    initializeFBO(w, h);
    initializeShaders();

    m_brushSplatTex = new QOpenGLTexture(QImage(":/images/brush/brush4.png"));
    m_brushSplatTex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
}

void GLRenderer::initializeFBO(int w, int h) {
    // Multisample FBO (where we render strokes offscreen)
    if (m_offscreenRenderMSFBO != nullptr) delete m_offscreenRenderMSFBO;
    QOpenGLFramebufferObjectFormat formatMS;
    formatMS.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    formatMS.setSamples(8);
    formatMS.setTextureTarget(GL_TEXTURE_2D);
    formatMS.setInternalTextureFormat(GL_RGBA);
    m_offscreenRenderMSFBO = new QOpenGLFramebufferObject(w, h, formatMS);
    
    // Regular FBO (where we resolve the multisampled texture from the previous FBO + one color attachment for masks)
    if (m_offscreenRenderFBO != nullptr) delete m_offscreenRenderFBO;
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setTextureTarget(GL_TEXTURE_2D);
    format.setInternalTextureFormat(GL_RGBA);
    m_offscreenRenderFBO = new QOpenGLFramebufferObject(w, h, format);
    m_offscreenRenderFBO->addColorAttachment(w, h, GL_RG32F);
    glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void GLRenderer::initializeShaders() {
    // Default stroke rendering shader
    QOpenGLShaderProgram *strokeProgram = new QOpenGLShaderProgram();
    bool result = strokeProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/stroke.vert");
    if (!result) qCritical() << strokeProgram->log();
    result = strokeProgram->addShaderFromSourceFile(QOpenGLShader::Geometry, ":/shaders/stroke.geom");
    if (!result) qCritical() << strokeProgram->log();
    result = strokeProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/stroke.frag");
    if (!result) qCritical() << strokeProgram->log();
    result = strokeProgram->link();
    if (!result) qCritical() << strokeProgram->log();
    m_shaderPrograms.insert("stroke", strokeProgram);
    
    // Display texture shader (for offscreen rendering)
    QOpenGLShaderProgram *displayProgram = new QOpenGLShaderProgram();
    result = displayProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/display.vert");
    if (!result) qCritical() << displayProgram->log();
    result = displayProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/display.frag");
    if (!result) qCritical() << displayProgram->log();
    result = displayProgram->link();
    if (!result) qCritical() << displayProgram->log();
    m_shaderPrograms.insert("display", displayProgram);

    // Mask rendering shader
    QOpenGLShaderProgram *maskProgram = new QOpenGLShaderProgram();
    result = maskProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/mask.vert");
    if (!result) qCritical() << maskProgram->log();
    result = maskProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/mask.frag");
    if (!result) qCritical() << maskProgram->log();
    result = maskProgram->link();
    if (!result) qCritical() << maskProgram->log();
    m_shaderPrograms.insert("mask", maskProgram);

    // Splat rendering shader
    QOpenGLShaderProgram *splatProgram = new QOpenGLShaderProgram();
    result = splatProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/splatting.vert");
    if (!result) qCritical() << splatProgram->log();
    result = splatProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/splatting.frag");
    if (!result) qCritical() << splatProgram->log();
    result = splatProgram->link();
    if (!result) qCritical() << splatProgram->log();
    m_shaderPrograms.insert("splat", splatProgram);
}


void GLRenderer::release() {
    releaseShaders();
}

void GLRenderer::releaseShaders() {
    for (QOpenGLShaderProgram *program : m_shaderPrograms) {
        delete program;
    }
}

void GLRenderer::makeCurrent() {
    m_canvas->makeCurrent();
}

void GLRenderer::doneCurrent() {
    m_canvas->doneCurrent();
}

void GLRenderer::startRender(int offW, int offH, bool drawOffscreen, bool exportFrames) {
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Bind offscreen textures and set mask program uniforms
    if (drawOffscreen) {
        // Bind the offscreen texture to display
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[0]);

        // Bind the mask texture
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[1]);

        // Clear mask buffer
        m_offscreenRenderFBO->bind();
        glDrawBuffer(GL_COLOR_ATTACHMENT1);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        QOpenGLShaderProgram *maskProgram = m_shaderPrograms["mask"];
        maskProgram->bind();
        maskProgram->setUniformValue("view", m_editor->view()->getView());
        maskProgram->setUniformValue("proj", m_canvas->projMat());
        maskProgram->release();
        m_offscreenRenderFBO->release();

        // Clear canvas buffer
        if (!m_offscreenRenderMSFBO->bind()) qDebug() << "Cannot bind offscreen FBO";
        glClearColor(1.0, 1.0, 1.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);        
    } 

    // Set stroke program uniforms
    QOpenGLShaderProgram *strokeProgram = m_shaderPrograms["stroke"];
    strokeProgram->bind();
    if (exportFrames) {
        glViewport(0, 0, offW, offH);
        double scaleW = offW / m_canvas->canvasRect().width();
        double scaleH = offH / m_canvas->canvasRect().height();
        QMatrix4x4 proj;
        proj.ortho(QRect(0, 0, offW, offH));
        strokeProgram->setUniformValue("view", QTransform().scale(scaleW, scaleH).translate(m_canvas->canvasRect().width() / 2, m_canvas->canvasRect().height() / 2));
        strokeProgram->setUniformValue("proj", proj);
        strokeProgram->setUniformValue("zoom", (GLfloat)(scaleW));
    } else {
        strokeProgram->setUniformValue("view", m_editor->view()->getView());
        strokeProgram->setUniformValue("proj", m_canvas->projMat());
        strokeProgram->setUniformValue("zoom", (GLfloat)m_editor->view()->scaling());
    }
    strokeProgram->setUniformValue("winSize", QVector2D(offW, offH));
    strokeProgram->setUniformValue("thetaEpsilon", (float)k_thetaEps);
    strokeProgram->setUniformValue("mask", 1);
    strokeProgram->release();

    // Bind splat texture and set up splatting program uniforms
    if (k_drawSplat) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_brushSplatTex->textureId());
        QOpenGLShaderProgram *splatProgram = m_shaderPrograms["splat"];
        splatProgram->bind();
        splatProgram->setUniformValue("tex", 3);
        splatProgram->setUniformValue("view", m_editor->view()->getView());
        splatProgram->setUniformValue("proj", m_canvas->projMat());
        splatProgram->setUniformValue("zoom", (GLfloat)m_editor->view()->scaling());
        splatProgram->release();
    }
}

void GLRenderer::endRender() {

}

void GLRenderer::render() {

}
