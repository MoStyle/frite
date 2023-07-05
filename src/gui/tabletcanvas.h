/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
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
#include <QSurface>

#include <QList>
#include <QImage>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QString;
QT_END_NAMESPACE

class Editor;
class Layer;
class VectorKeyFrame;
class KeyFrame;
class KeyframedTransform;

class TabletCanvas : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

   public:
    enum Valuator { PressureValuator, TangentialPressureValuator, TiltValuator, VTiltValuator, HTiltValuator, NoValuator };
    Q_ENUM(Valuator)

    enum TransformMode { Translation, Rotation, Scaling };
    Q_ENUM(TransformMode)

    TabletCanvas();
    ~TabletCanvas();

    void setEditor(Editor *editor) { m_editor = editor; }

    QPixmap *getPixmap() { return &m_pixmap; }
    QGraphicsScene *graphicsScene() { return m_graphicsScene; }
    QGraphicsScene *fixedGraphicsScene() { return m_fixedGraphicsScene; }
    CanvasView *canvasView() { return m_graphicsView; }

    void setAlphaChannelValuator(Valuator type) { m_alphaChannelValuator = type; }
    void setColorSaturationValuator(Valuator type) { m_colorSaturationValuator = type; }
    void setLineWidthType(Valuator type) { m_lineWidthValuator = type; }

    void setTabletDevice(QTabletEvent *event) {
        m_deviceActive = event->type() == QEvent::TabletEnterProximity ? 1 : 0;
        updateCursor(event);
    }
    void setCanvasRect(int width, int height);
    QRect canvasRect() const { return m_canvasRect; }
    int maximum(int a, int b) { return a > b ? a : b; }

    void setDrawGroupColor(bool drawGroupColor) { m_drawGroupColor = drawGroupColor; }
    void setDrawPreGroupGhosts(bool drawPreGroupGhosts) { m_drawPreGroupGhosts = drawPreGroupGhosts; }

    void initializeFBO(int w, int h);
    void paintGLInit(int offW, int offH, bool drawOffscreen);
    void paintGLRelease(bool drawOffscreen);

    void drawCanvas(QPainter &painter, bool useGL = true, bool drawOffscreen = false);
    void drawKeyFrame(QPainter &painter, VectorKeyFrame *keyframe, int frame, int inbetween, int stride, const QColor &color, qreal opacity, qreal tintFactor, bool useGL = true);
    void drawKeyFrame(QPainter &painter, VectorKeyFrame *keyframe, const QColor &color, qreal opacity, qreal tintFactor, GroupType type, bool useGL = true);
    void drawToolGizmos(QPainter &painter);
    void drawBackground(QPainter &painter);
    QImage grabCanvasFramebuffer() const { return m_offscreenRenderFBO->toImage(); }

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
    void deselectAll();
    void loadBackgrounds(QString dir);


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
    void drawInterpolatedKeyFrame(QPainter &painter, Layer *layer, int frame, int inbetween, int stride, qreal opacity, qreal tintFactor, bool next, bool useGL = true);
    void drawOnionSkins(QPainter &painter, Layer *layer, bool useGL = true);
    void drawSelectedGroupsLifetime(QPainter &painter, Layer *layer, VectorKeyFrame *keyframe, int frame, int inbetween, int stride, bool useGL = true);
    void drawGroup(QPainter &painter, VectorKeyFrame *keyframe, Group *group, float alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor=1.0, bool useGL = true);
    void drawSelectedGroups(QPainter &painter, VectorKeyFrame *keyframe, GroupType type, float alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor=1.0, bool useGL = true);
    void drawSelectedGroups(QPainter &painter, VectorKeyFrame *keyframe, GroupType type, double opacity, const QColor &color, double tint, double strokeWeightFactor=1.0, bool useGL = true);
    void drawExportOnionSkins(QPainter &painter, Layer *layer, bool useGL = true);

    Qt::BrushStyle brushPattern(qreal value);
    void updateBrush(const QTabletEvent *event);
    void updateCursor(const QTabletEvent *event);
    QCursor crossCursor(qreal width);
    void updateSelectionZones();

    VectorKeyFrame *currentKeyFrame();
    VectorKeyFrame *prevKeyFrame();

    Valuator m_alphaChannelValuator;
    Valuator m_colorSaturationValuator;
    Valuator m_lineWidthValuator;

    QPixmap m_pixmap; // background pixmap

    // !deprecated, need to remove
    QBrush m_brush;
    QPen m_pen;                     

    QList<QPixmap> m_backgrounds;

    bool m_deviceActive, m_deviceDown;
    Qt::MouseButton m_button;

    QRect m_canvasRect;  // Represents the drawable region, centered at (0,0)

    qreal m_currentAlpha;
    int m_inbetween, m_stride;

    bool m_drawGroupColor = false;
    bool m_drawPreGroupGhosts = false;

    Editor *m_editor = nullptr;

    // Scene that follows the canvas
    CanvasView *m_graphicsView = nullptr;
    QGraphicsScene *m_graphicsScene = nullptr;

    // Scene that is fixed to the camera
    CanvasView *m_fixedGraphicsView = nullptr;
    QGraphicsScene *m_fixedGraphicsScene = nullptr;

    // GL
    QMatrix4x4 m_projMat;
    QOpenGLPaintDevice *m_paintDevice;
    QOpenGLShaderProgram *m_strokeProgram, *m_displayProgram;
    QOpenGLVertexArrayObject m_displayVAO;
    QOpenGLBuffer m_displayVBO;
    GLint m_strokeViewLocation, m_strokeProjLocation, m_strokeWinSize, m_strokeZoom, m_strokeThetaEpsilon, m_strokeColor;
    GLint m_displayTextureLocation;
    QOpenGLFramebufferObject *m_offscreenRenderFBO = nullptr;

    struct PointT {
        QPointF pixel;
        QPointF pos;
        qreal rotation;
    } lastPoint, firstPoint;
};

#endif
