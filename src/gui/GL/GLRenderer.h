#ifndef GLRENDERER_H
#define GLRENDERER_H

#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLTexture>

#include "GLData.h"

class TabletCanvas;
class Editor;

class GLRenderer : protected QOpenGLFunctions {
public:
    GLRenderer(GLRenderer &other) = delete;
    void operator=(const GLRenderer &) = delete;

    void setTabletCanvas(TabletCanvas *canvas) { m_canvas = canvas; }
    void setEditor(Editor *editor) { m_editor = editor; }

    void initialize(int w, int h);
    void initializeFBO(int w, int h);
    void initializeShaders();
    void release();

    void makeCurrent();
    void doneCurrent();

    void startRender(int offW, int offH, bool drawOffscreen, bool exportFrames=false);
    void endRender();
    void render();

    QHash<QString, QOpenGLShaderProgram *> m_shaderPrograms;
private:
    GLRenderer();

    void releaseShaders();

    TabletCanvas *m_canvas;
    Editor *m_editor;

    // Textures
    QOpenGLTexture *m_brushSplatTex;

    // Draw buffers
    std::vector<GLStrokesData> m_strokesBuffers;        // Batched strokes data (in depth order)
    GLDisplayQuadData m_displayBuffer;

    // FBOs
    QOpenGLFramebufferObject *m_offscreenRenderMSFBO;   // Multisampled 
    QOpenGLFramebufferObject *m_offscreenRenderFBO;     // Where the multisampled offscreen render is resolved
};

#endif // GLRENDERER_H