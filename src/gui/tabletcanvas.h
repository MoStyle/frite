/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef TABLETCANVAS_H
#define TABLETCANVAS_H

#include "point.h"
#include "stroke.h"
#include "canvasview.h"

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLPaintDevice>
#include <QOpenGLContext>
#include <QOpenGLTexture>
#include <QSurface>

#include <QList>
#include <QImage>

#include <chrono>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QString;
QT_END_NAMESPACE

class Editor;
class Layer;
class VectorKeyFrame;
class KeyFrame;
class KeyframedTransform;

class TabletCanvas : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT

   public:
    enum Valuator { PressureValuator, TangentialPressureValuator, TiltValuator, VTiltValuator, HTiltValuator, NoValuator };
    Q_ENUM(Valuator)

    enum TransformMode { Translation, Rotation, Scaling };
    Q_ENUM(TransformMode)

    enum MaskOcclusionMode { MaskOcclude=0, MaskGrayOut };
    enum DisplayMode { StrokeColor=0, PointColor, VisibilityThreshold };

    TabletCanvas();
    ~TabletCanvas();

    void setEditor(Editor *editor) { m_editor = editor; }

    QPixmap *getPixmap() { return &m_pixmap; }
    QGraphicsScene *fixedGraphicsScene() { return m_fixedGraphicsScene; }
    CanvasView *fixedCanvasView() { return m_fixedGraphicsView; }

    const QFont &canvasFont() const { return m_canvasFont; }
    int fontSize() const { return m_canvasFont.pointSize(); } 
    void setFontSize(int size) { m_canvasFont.setPointSize(size); }

    void setAlphaChannelValuator(Valuator type) { m_alphaChannelValuator = type; }
    void setColorSaturationValuator(Valuator type) { m_colorSaturationValuator = type; }
    void setLineWidthType(Valuator type) { m_lineWidthValuator = type; }

    void setTabletDevice(QTabletEvent *event) {
        m_deviceActive = event->type() == QEvent::TabletEnterProximity ? 1 : 0;
        updateCursor(event);
    }
    const QMatrix4x4 &projMat() const { return m_projMat; }
    void setCanvasRect(int width, int height);
    QRect canvasRect() const { return m_canvasRect; }

    void setDrawGroupColor(bool drawGroupColor) { m_drawGroupColor = drawGroupColor; }
    void setDrawPreGroupGhosts(bool drawPreGroupGhosts) { m_drawPreGroupGhosts = drawPreGroupGhosts; }
    void setMaskOcclusionMode(MaskOcclusionMode occlusionMode) { m_maskOcclusionMode = occlusionMode; }
    void setDisplayVisibility(bool displayVisibility) { m_displayVisibility = displayVisibility; }
    void setDisplayDepth(bool displayDepth) { m_displayDepth = displayDepth; }
    void setDisplayMask(bool displayMask) { m_displayMask = displayMask; }
    void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    void setDisplaySelectedGroupsLifetime(bool displaySelectedGroupsLifetime) { m_displaySelectedGroupsLifetime = displaySelectedGroupsLifetime; }

    void showInfoMessage(const QString &message, int durationMs);

    void initializeFBO(int w, int h);
    void paintGLInit(int offW, int offH, bool drawOffscreen, bool exportFrames=false);
    void paintGLRelease(bool drawOffscreen);

    void drawCanvas(bool exportFrames=false);
    void drawKeyFrame(VectorKeyFrame *keyframe, int frame, int inbetween, int stride, const QColor &color, qreal opacity, qreal tintFactor, bool drawMasks=true);
    void drawKeyFrame(VectorKeyFrame *keyframe, const QColor &color, qreal opacity, qreal tintFactor, GroupType type, bool drawMasks=true);
    void drawGrid(Group *group);
    void drawCircleCursor(QVector2D nudge);
    void drawToolGizmos(QPainter &painter);
    void drawBackground(QPainter &painter);
    QImage grabCanvasFramebuffer() const { return m_offscreenRenderFBO->toImage(); }
    void resolveMSFramebuffer() { QOpenGLFramebufferObject::blitFramebuffer(m_offscreenRenderFBO, m_offscreenRenderMSFBO); }
    DisplayMode displayMode() const { return m_displayMode; }

    void debugReport();
   signals:
    void scribbleSelected();
    void eraserSelected();
    void penSelected();
    void handSelected();
    void trajectorySelected();
    void lassoSelected();
    void groupModified(GroupType type, int id);  // relative to the current keyframe
    void groupsModified(GroupType type);         // relative to the current keyframe
    void groupListModified(GroupType type);      // relative to the current keyframe
    void frameModified(int frame);               // relative to the current keyframe
    void groupChanged(Group *group);             // !deprecated
    void pieMenuOn();
    void pieMenuOff();

   public slots:
    void updateCurrentFrame();
    void updateFrame(int frame);
    void updateCursor();
    void updateCursor(bool b);
    void updateDrawAggregate(bool draw);
    void selectAll();
    void loadBackgrounds(QString dir);
    void toggleDisplayMask(bool b);
    QColor sampleColorMap(double depth);

   protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
    void tabletEvent(QTabletEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void contextMenuEvent(QContextMenuEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *event) Q_DECL_OVERRIDE;

    void initializeGL() Q_DECL_OVERRIDE;
    void resizeGL(int w, int h) Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;

    bool focusNextPrevChild(bool next) Q_DECL_OVERRIDE { return false; }

   private:
    void initPixmap();
    void paintPixmap(QPainter &painter, QTabletEvent *event);
    void drawOnionSkins(Layer *layer);
    void drawSelectedGroupsLifetime(Layer *layer, VectorKeyFrame *keyframe, int frame, int inbetween, int stride);
    void drawGroup(VectorKeyFrame *keyframe, Group *group, qreal alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor=1.0);
    void drawSelectedGroups(VectorKeyFrame *keyframe, GroupType type, qreal alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor=1.0);
    void drawSelectedGroups(VectorKeyFrame *keyframe, GroupType type, double opacity, const QColor &color, double tint, double strokeWeightFactor=1.0);
    void drawMask(VectorKeyFrame *keyframe, Group *group, int inbetween, int stride, qreal alpha, int depth);
    void drawExportOnionSkins(Layer *layer);
    void startDrawSplatStrokes();
    void endDrawSplatStrokes();

    Qt::BrushStyle brushPattern(qreal value);
    void updateBrush(const QTabletEvent *event);
    void updateCursor(const QTabletEvent *event);
    QCursor crossCursor(qreal width);

    VectorKeyFrame *currentKeyFrame();
    VectorKeyFrame *prevKeyFrame();

    Valuator m_alphaChannelValuator;
    Valuator m_colorSaturationValuator;
    Valuator m_lineWidthValuator;

    QPixmap m_pixmap; // background pixmap
    QImage m_infernoColorMap;

    // !deprecated, need to remove
    QBrush m_brush;
    QPen m_pen;                     
    QFont m_canvasFont;

    QList<QPixmap> m_backgrounds;

    bool m_deviceActive, m_deviceDown;
    Qt::MouseButton m_button;

    QRect m_canvasRect;  // Represents the drawable region, centered at (0,0)

    qreal m_currentAlpha;
    int m_inbetween, m_stride;

    bool m_drawGroupColor;
    bool m_drawPreGroupGhosts;
    bool m_displayVisibility;
    bool m_displayDepth;
    bool m_displayMask;
    bool m_displaySelectedGroupsLifetime;
    bool m_temporarySelectTool;
    MaskOcclusionMode m_maskOcclusionMode;
    DisplayMode m_displayMode;

    Editor *m_editor = nullptr;

    // Scene that is fixed to the camera
    CanvasView *m_fixedGraphicsView = nullptr;
    QGraphicsScene *m_fixedGraphicsScene = nullptr;

    QString m_infoMessageText;
    QTimer m_infoMessageDuration;

    // GL
    QMatrix4x4 m_projMat;
    QOpenGLPaintDevice *m_paintDevice;
    QOpenGLShaderProgram *m_strokeProgram, *m_displayProgram, *m_maskProgram, *m_splattingProgram, *m_displayMaskProgram, *m_displayGridProgram, *m_cursorProgram;
    QOpenGLVertexArrayObject m_displayVAO, m_cursorVAO;
    QOpenGLBuffer m_displayVBO, m_cursorVBO;
    GLint m_strokeViewLocation, m_strokeProjLocation, m_strokeWinSize, m_strokeZoom, m_strokeThetaEpsilon, m_strokeColor;
    GLint m_displayTextureLocation;
    QOpenGLFramebufferObject *m_offscreenRenderMSFBO = nullptr, *m_offscreenRenderFBO = nullptr; // Multisampled and regular offscreen FBO
    GLuint m_offscreenTexture;
    GLint m_blendEq, m_sFactor, m_dFactor;
    
    QOpenGLTexture *m_pointTex, *m_maskTex;

    struct PointT {
        QPointF pixel;
        QPointF pos;
        qreal rotation;
    } lastPoint, firstPoint;
};

#endif
