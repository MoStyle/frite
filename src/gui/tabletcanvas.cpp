/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include <math.h>

#include <QtWidgets>
#include <QGraphicsOpacityEffect>
#include <QDir>
#include <QMatrix4x4>

#include "arap.h"
#include "canvascommands.h"
#include "layercommands.h"
#include "keycommands.h"
#include "colormanager.h"
#include "canvasscenemanager.h"
#include "dialsandknobs.h"
#include "editor.h"
#include "registrationmanager.h"
#include "toolsmanager.h"
#include "layer.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "fixedscenemanager.h"
#include "tabletcanvas.h"
#include "vectorkeyframe.h"
#include "viewmanager.h"
#include "gridmanager.h"
#include "grouplist.h"
#include "tools/pentool.h"
#include "utils/stopwatch.h"
#include "quad.h"
#include "qteigen.h"

#ifdef Q_OS_MAC
extern "C" {
void detectWhichOSX();
}
#else
extern "C" {
void detectWhichOSX() {}
}
#endif

static dkFilename k_backgroundDir("Options->Backgrounds->Directory");
static dkBool k_showBackground("Options->Backgrounds->Show", true);
static dkBool k_backgroundOnKF("Options->Backgrounds->Keyframe", true);
static dkBool k_AA("Pen->AAliasing", true);
dkBool k_drawGL("Options->Drawing->Draw GL", true);
static dkBool k_drawTess("Options->Drawing->Draw tess", false);
static dkBool k_drawPoints("Options->Drawing->Draw points", false);
static dkFloat k_thetaEps("Options->Drawing->Stroke drawing smoothness", 0.01, 0.00001, 1.0, 0.00001);

dkBool k_exportOnionSkinMode("Options->Export->Onion skin mode", false);
dkBool k_exportOnlyKeysMode("Options->Export->Export keys only (onion export)", false);
dkInt k_exportFrom("Options->Export->Export from", 1, 1, 100, 1);
dkInt k_exportTo("Options->Export->Export to", 0, 0, 100, 1);
dkBool k_exportOnlyCurSegment("Options->Export->Only current segment", false);
dkBool k_exportGhostFrame("Options->Export->Draw ghost frame", false);

extern dkBool k_drawMainGroupGrid;

TabletCanvas::TabletCanvas()
    : QOpenGLWidget(nullptr),
      m_alphaChannelValuator(TangentialPressureValuator),
      m_colorSaturationValuator(NoValuator),
      m_lineWidthValuator(PressureValuator),
      m_brush(Qt::black),
      m_pen(m_brush, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
      m_deviceActive(false),
      m_deviceDown(false),
      m_canvasRect(-960, -540, 1920, 1080),
      m_currentAlpha(0.0) {
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);
    m_graphicsScene = new QGraphicsScene();
    m_fixedGraphicsScene = new QGraphicsScene();
    m_graphicsView = new CanvasView(m_graphicsScene, m_editor, this, false);
    m_fixedGraphicsView = new CanvasView(m_fixedGraphicsScene, m_editor, this, false);

    initPixmap();
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_AcceptTouchEvents);

    connect(&k_AA, SIGNAL(valueChanged(bool)), this, SLOT(updateCursor(bool)));
    connect(&k_drawGL, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_drawTess, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_drawPoints, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_backgroundDir, SIGNAL(valueChanged(QString)), this, SLOT(loadBackgrounds(QString)));

    detectWhichOSX();
}

TabletCanvas::~TabletCanvas() {
    makeCurrent();
    delete m_strokeProgram;
    delete m_displayProgram;
    delete m_offscreenRenderFBO;
    doneCurrent();
}

void TabletCanvas::resizeGL(int w, int h) {
    m_editor->view()->setCanvasSize(QSize(w, h));
    m_graphicsView->setFixedSize(width(), height());
    m_fixedGraphicsView->setFixedSize(width(), height());
    m_graphicsScene->setSceneRect(m_canvasRect);
    m_fixedGraphicsScene->setSceneRect(rect());
    initPixmap();
    int side = std::min(w, h);
    glViewport(-w / 2, -h / 2, w, h);
    m_projMat.setToIdentity();
    m_projMat.ortho(QRect(0, 0, w, h));
    update();
}

void TabletCanvas::initPixmap() {
    QPixmap newPixmap = QPixmap(width(), height());
    newPixmap.fill(Qt::transparent);
    QPainter painter(&newPixmap);
    if (!m_pixmap.isNull()) painter.drawPixmap(0, 0, m_pixmap);
    painter.end();
    m_pixmap = newPixmap;
}

void TabletCanvas::setCanvasRect(int width, int height) { m_canvasRect = QRect(-width / 2, -height / 2, width, height); }

void TabletCanvas::updateCurrentFrame() {
    int currentFrame = m_editor->playback()->currentFrame();
    updateFrame(currentFrame);
}

void TabletCanvas::updateFrame(int frame) {
    Q_UNUSED(frame);
    update();
}

void TabletCanvas::mousePressEvent(QMouseEvent *event) {
    QOpenGLWidget::mousePressEvent(event);
    if (m_deviceActive) return;

    if (!m_deviceDown) {
        m_button = event->button();

        m_deviceDown = true;
        lastPoint.pixel = event->position();
        lastPoint.pos = m_editor->view()->mapScreenToCanvas(event->position());
        lastPoint.rotation = 0.0;
        firstPoint = lastPoint;
        m_currentAlpha = m_editor->alpha(m_editor->playback()->currentFrame());
        m_inbetween = m_editor->layers()->currentLayer()->inbetweenPosition(m_editor->playback()->currentFrame());
        m_stride = m_editor->layers()->currentLayer()->stride(m_editor->playback()->currentFrame());

        Tool::EventInfo info;
        info.key = prevKeyFrame();
        info.firstPos = firstPoint.pos;
        info.lastPos = lastPoint.pos;
        info.pos = info.lastPos;
        info.rotation = lastPoint.rotation;
        info.alpha = m_currentAlpha;
        info.inbetween = m_inbetween;
        info.stride = m_stride;
        info.modifiers = event->modifiers();
        info.mouseButton = event->button();

        // override to Pan/Rotate tool if the middle mouse button is pressed
        if (info.mouseButton & Qt::MiddleButton) {
            m_editor->tools()->tool(Tool::Hand)->pressed(info);
        } else {
            m_editor->tools()->currentTool()->pressed(info);
        }
    }
    event->accept();
    update();
}

void TabletCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
    Tool::EventInfo info;
    info.key = prevKeyFrame();
    info.firstPos = m_editor->view()->mapScreenToCanvas(event->position());
    info.lastPos = info.firstPos;
    info.pos = info.firstPos;
    info.rotation = 0;
    info.alpha = m_editor->alpha(m_editor->playback()->currentFrame());
    info.inbetween = m_inbetween;
    info.stride = m_stride;

    m_editor->tools()->currentTool()->doublepressed(info);
}

void TabletCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (m_deviceActive) return;
    if (m_deviceDown) {
        QPointF pos = event->position();
        QPointF smoothPos = QPointF((pos.x() + lastPoint.pixel.x()) / 2.0, (pos.y() + lastPoint.pixel.y()) / 2.0);
        QPointF newPos = m_editor->view()->mapScreenToCanvas(smoothPos);

        Tool::EventInfo info;
        info.key = prevKeyFrame();
        info.firstPos = firstPoint.pos;
        info.lastPos = lastPoint.pos;
        info.pos = newPos;
        info.rotation = lastPoint.rotation;
        info.alpha = m_currentAlpha;
        info.inbetween = m_inbetween;
        info.stride = m_stride;
        info.modifiers = event->modifiers();
        info.mouseButton = m_button;

        // override to Pan/Rotate tool if the middle mouse button is pressed
        if (info.mouseButton & Qt::MiddleButton) {
            m_editor->tools()->tool(Tool::Hand)->moved(info);
        } else {
            m_editor->tools()->currentTool()->moved(info);
        }

        lastPoint.pixel = smoothPos;
        lastPoint.pos = m_editor->view()->mapScreenToCanvas(smoothPos);  // remap because view may have changed
        lastPoint.rotation = 0.0;
    }
    event->accept();
    update();
}

void TabletCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (m_deviceActive) return;

    if (m_deviceDown && event->buttons() == Qt::NoButton) {
        m_deviceDown = false;

        Tool::EventInfo info;
        info.key = prevKeyFrame();
        info.firstPos = firstPoint.pos;
        info.lastPos = lastPoint.pos;
        info.pos = info.lastPos;
        info.rotation = lastPoint.rotation;
        info.alpha = m_currentAlpha;
        info.inbetween = m_inbetween;
        info.stride = m_stride;
        info.modifiers = event->modifiers();
        info.mouseButton = m_button;

        // override to Pan/Rotate tool if the middle mouse button is pressed
        if (info.mouseButton & Qt::MiddleButton) {
            m_editor->tools()->tool(Tool::Hand)->released(info);
        } else {
            m_editor->tools()->currentTool()->released(info);
        }
    }
    event->accept();
    update();
}

void TabletCanvas::tabletEvent(QTabletEvent *event) {
    switch (event->type()) {
        case QEvent::TabletPress:
            if (!m_deviceDown) {
                m_deviceDown = true;
                lastPoint.pixel = event->position();
                lastPoint.pos = m_editor->view()->mapScreenToCanvas(event->position());
                lastPoint.rotation = event->rotation();
                firstPoint = lastPoint;
                m_currentAlpha = m_editor->alpha(m_editor->playback()->currentFrame());
                m_inbetween = m_editor->layers()->currentLayer()->inbetweenPosition(m_editor->playback()->currentFrame());
                m_stride = m_editor->layers()->currentLayer()->stride(m_editor->playback()->currentFrame());

                m_button = event->button();

                Tool::EventInfo info;
                info.key = prevKeyFrame();
                info.firstPos = firstPoint.pos;
                info.lastPos = lastPoint.pos;
                info.pos = info.lastPos;
                info.rotation = lastPoint.rotation;
                info.pressure = event->pressure();
                info.alpha = m_currentAlpha;
                info.inbetween = m_inbetween;
                info.stride = m_stride;
                info.mouseButton = m_button;
                info.modifiers = event->modifiers();

                // override to Pan/Rotate tool if the middle mouse button is pressed
                if (info.mouseButton & Qt::MiddleButton) {
                    m_editor->tools()->tool(Tool::Hand)->pressed(info);
                } else {
                    m_editor->tools()->currentTool()->pressed(info);
                }
            }
            break;
        case QEvent::TabletMove:
#ifndef Q_OS_IOS
            if (event->pointingDevice() && event->pointingDevice()->capabilities().testFlag(QPointingDevice::Capability::Rotation)) updateCursor(event);
#endif
            if (m_deviceDown) {
                updateBrush(event);
                // TODO test smooth pos like with mouse input
                Tool::EventInfo info;
                info.key = prevKeyFrame();
                info.firstPos = firstPoint.pos;
                info.lastPos = lastPoint.pos;

                lastPoint.pixel = event->position();
                lastPoint.pos = m_editor->view()->mapScreenToCanvas(event->position());
                lastPoint.rotation = event->rotation();

                info.pos = lastPoint.pos;
                info.rotation = lastPoint.rotation;
                info.pressure = event->pressure();
                info.alpha = m_currentAlpha;
                info.inbetween = m_inbetween;
                info.stride = m_stride;
                info.mouseButton = m_button;
                info.modifiers = event->modifiers();

                // override to Pan/Rotate tool if the middle mouse button is pressed
                if (info.mouseButton & Qt::MiddleButton) {
                    m_editor->tools()->tool(Tool::Hand)->moved(info);
                } else {
                    m_editor->tools()->currentTool()->moved(info);
                }

                lastPoint.pixel = event->position();
                lastPoint.pos = m_editor->view()->mapScreenToCanvas(event->position());  // remap because view may have changed
                lastPoint.rotation = 0.0;
            } else {
                event->ignore();
                return;
            }
            break;
        case QEvent::TabletRelease:
            if (m_deviceDown && event->buttons() == Qt::NoButton) {
                m_deviceDown = false;
                // if (event->pointerType() == QPointingDevice::PointerType::Eraser && m_previousTool != Eraser) {
                //     restorePreviousTool();
                // }
                Tool::EventInfo info;
                info.key = prevKeyFrame();
                info.firstPos = firstPoint.pos;
                info.lastPos = lastPoint.pos;
                info.pos = info.lastPos;
                info.rotation = lastPoint.rotation;
                info.pressure = event->pressure();
                info.alpha = m_currentAlpha;
                info.inbetween = m_inbetween;
                info.stride = m_stride;
                info.mouseButton = m_button;
                info.modifiers = event->modifiers();

                // override to Pan/Rotate tool if the middle mouse button is pressed
                if (info.mouseButton & Qt::MiddleButton) {
                    m_editor->tools()->tool(Tool::Hand)->released(info);
                } else {
                    m_editor->tools()->currentTool()->released(info);
                }

                m_button = Qt::NoButton;
            }
            break;
        default:
            break;
    }
    event->accept();
    update();
}

void TabletCanvas::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Tab:
            if (!event->isAutoRepeat()) {
                m_editor->tools()->setTool(Tool::Select);
            }
            break;
        default:
            if (m_editor->tools()->currentTool() != nullptr) {
                m_editor->tools()->currentTool()->keyPressed(event);
            }
            break;
    }
    event->accept();
    update();
}

void TabletCanvas::keyReleaseEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Tab:
            if (!event->isAutoRepeat()) {
                m_editor->tools()->restorePreviousTool();
            }
            break;
        default:
            if (m_editor->tools()->currentTool() != nullptr) {
                m_editor->tools()->currentTool()->keyReleased(event);
            }
            break;
    }
    event->accept();
    update();
}

void TabletCanvas::contextMenuEvent(QContextMenuEvent *event) {
    Tool *tool = m_editor->tools()->currentTool();
    if (tool == nullptr && !tool->contextMenuAllowed()) {
        return;
    }

    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *keyframe = prevKeyFrame();
    Point::VectorType pos =  QE_POINT(m_editor->view()->mapScreenToCanvas(event->pos()));
    int inbetween = layer->inbetweenPosition(currentFrame);
    QuadPtr q; int k;
    QMenu contextMenu(this);
    bool groupFound = false;
    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if ((inbetween == 0 && group->lattice()->contains(pos, REF_POS, q, k)) || (inbetween > 0 && keyframe->inbetween(inbetween).contains(group, pos, q, k))) {
            contextMenu.addSection("Group");
            contextMenu.addAction(tr("Clone groups forward"), m_editor, &Editor::copyGroupToNextKeyFrame);
            contextMenu.addAction(tr("Delete groups"), m_editor, &Editor::deleteGroup);
            contextMenu.addSeparator();
            contextMenu.addAction(tr("Matching"), m_editor, &Editor::registerFromRestPosition);
            contextMenu.addAction(tr("Matching from current state"), m_editor, &Editor::registerFromTargetPosition);
            contextMenu.addSeparator();
            contextMenu.addAction(tr("Add cross-fade"), m_editor, &Editor::bakeNextPreGroup);
            contextMenu.addAction(tr("Remove cross-fade"), m_editor, &Editor::removeNextPreGroup);
            contextMenu.addAction(tr("Fade-out"), m_editor, &Editor::makeGroupFadeOut);
            if (tool != nullptr) tool->contextMenu(contextMenu);
            groupFound = true;
            break;
        }
    }
    if (!groupFound) {
        contextMenu.addSection("Keyframe");
        contextMenu.addAction(tr("Matching"), m_editor, &Editor::registerFromRestPosition);
        contextMenu.addAction(tr("Matching from current state"), m_editor, &Editor::registerFromTargetPosition);
        contextMenu.addAction(tr("Clear keyframe"), m_editor, &Editor::clearCurrentFrame);
        contextMenu.addAction(tr("Add breakdown"), m_editor, &Editor::convertToBreakdown);
    }
    contextMenu.exec(event->globalPos());
    event->accept();
}

bool TabletCanvas::event(QEvent *event) {
    // since qt doesn't want to give the release event for the TAB key, we have to do this...
    // if (event->type() == QEvent::KeyRelease) {
    //     QKeyEvent *k = static_cast<QKeyEvent *>(event);
    //     if (k->key() == Qt::Key_Tab) {
    //     }
    // }
    return this->QWidget::event(event);
}

void TabletCanvas::initializeFBO(int w, int h) {
    if (m_offscreenRenderFBO != nullptr) delete m_offscreenRenderFBO;
    QOpenGLFramebufferObjectFormat sFormat;
    sFormat.setSamples(8);
    sFormat.setTextureTarget(GL_TEXTURE_2D);
    sFormat.setInternalTextureFormat(GL_RGBA);
    m_offscreenRenderFBO = new QOpenGLFramebufferObject(w, h, sFormat);
}

void TabletCanvas::initializeGL() {
    initializeOpenGLFunctions();

    initializeFBO(m_canvasRect.width(), m_canvasRect.height());

    glEnable(GL_DEPTH_TEST);

    // Initialize shaders
    m_strokeProgram = new QOpenGLShaderProgram();
    bool result = m_strokeProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/stroke.vert");
    if (!result) qCritical() << m_strokeProgram->log();
    result = m_strokeProgram->addShaderFromSourceFile(QOpenGLShader::Geometry, ":/shaders/stroke.geom");
    if (!result) qCritical() << m_strokeProgram->log();
    result = m_strokeProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/stroke.frag");
    if (!result) qCritical() << m_strokeProgram->log();
    result = m_strokeProgram->link();
    if (!result) qCritical() << m_strokeProgram->log();
    result = m_strokeProgram->bind();
    if (!result) qCritical() << m_strokeProgram->log();
    m_strokeViewLocation = m_strokeProgram->uniformLocation("view");
    m_strokeProjLocation = m_strokeProgram->uniformLocation("proj");
    m_strokeWinSize = m_strokeProgram->uniformLocation("win_size");
    m_strokeZoom = m_strokeProgram->uniformLocation("zoom");
    m_strokeThetaEpsilon = m_strokeProgram->uniformLocation("theta_epsilon");
    m_strokeColor = m_strokeProgram->uniformLocation("stroke_color");
    m_strokeProgram->release();

    m_displayProgram = new QOpenGLShaderProgram();
    result = m_displayProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/display.vert");
    if (!result) qCritical() << m_displayProgram->log();
    result = m_displayProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/display.frag");
    if (!result) qCritical() << m_displayProgram->log();
    result = m_displayProgram->link();
    if (!result) qCritical() << m_displayProgram->log();
    result = m_displayProgram->bind();
    if (!result) qCritical() << m_displayProgram->log();
    m_displayVAO.create();
    if (m_displayVAO.isCreated()) m_displayVAO.bind();
    static const GLfloat quadBufferData[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
    m_displayVBO.create();
    m_displayVBO.bind();
    m_displayVBO.allocate(quadBufferData, sizeof(quadBufferData));
    int vertexLocation = m_displayProgram->attributeLocation("vertex");
    m_displayProgram->enableAttributeArray(vertexLocation);
    m_displayProgram->setAttributeBuffer(vertexLocation, GL_FLOAT, 0, 2);
    m_displayTextureLocation = m_displayProgram->uniformLocation("offscreen");
    m_displayVBO.release();
    m_displayVAO.release();
    m_displayProgram->release();
}

void TabletCanvas::wheelEvent(QWheelEvent *event) {
    QPoint pixels = event->pixelDelta();
    QPoint angle = event->angleDelta();
    qreal delta = 0.0;

    if (!pixels.isNull())
        delta = pixels.y();
    else if (!angle.isNull())
        delta = angle.y();

    // by default the scroll wheel scales up/down the view
    // if ctrl is pressed while scrolling the behaviour depends on which tool is currently being used
    if (m_editor->tools()->currentTool() != nullptr && QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
        Tool::WheelEventInfo info;
        info.key = prevKeyFrame();
        info.alpha = m_editor->alpha(m_editor->playback()->currentFrame());
        info.delta = delta;
        info.pos = m_editor->view()->mapScreenToCanvas(event->position());
        info.modifiers = event->modifiers();
        m_editor->tools()->currentTool()->wheel(info);
        update();
    } else {
        if (delta < 0)
            m_editor->view()->scaleDown();
        else
            m_editor->view()->scaleUp();
        updateCursor(nullptr);
    }

    event->accept();
}

void TabletCanvas::paintGLInit(int offW, int offH, bool drawOffscreen) {
    if (drawOffscreen) {
        m_offscreenRenderFBO->bind();
        glViewport(0, 0, offW, offH);
        glClearColor(1.0, 1.0, 1.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        double scaleW = offW / m_canvasRect.width();
        double scaleH = offH / m_canvasRect.height();
        m_strokeProgram->bind();
        m_strokeProgram->setUniformValue(m_strokeViewLocation, QTransform().scale(scaleW, scaleH).translate(m_canvasRect.width() / 2, m_canvasRect.height() / 2));
        QMatrix4x4 proj;
        proj.ortho(QRect(0, 0, offW, offH));
        m_strokeProgram->setUniformValue(m_strokeProjLocation, proj);
        m_strokeProgram->setUniformValue(m_strokeWinSize, QVector2D(offW, offH));
        m_strokeProgram->setUniformValue(m_strokeZoom, (GLfloat)(scaleW));
    } else {
        m_strokeProgram->bind();
        m_strokeProgram->setUniformValue(m_strokeViewLocation, m_editor->view()->getView());
        m_strokeProgram->setUniformValue(m_strokeProjLocation, m_projMat);
        m_strokeProgram->setUniformValue(m_strokeZoom, (GLfloat)m_editor->view()->scaling());
    }
    m_strokeProgram->setUniformValue(m_strokeWinSize, QVector2D(offW, offH));
    m_strokeProgram->setUniformValue(m_strokeThetaEpsilon, (float)k_thetaEps);
    m_strokeProgram->release();
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (k_drawTess) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else if (k_drawPoints) glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
}

void TabletCanvas::paintGLRelease(bool drawOffscreen) {
    if (k_drawTess || k_drawPoints) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    m_strokeProgram->release();
    if (drawOffscreen) {
        m_offscreenRenderFBO->release();
    }
}

void TabletCanvas::paintGL() {
    StopWatch sw("rendering");

    QPainter painter(this);
    const QTransform &view = m_editor->view()->getView();
    QRectF viewRect = painter.viewport();
    QRect boundingRect = m_editor->view()->mapScreenToCanvas(viewRect).toRect();

    // TODO only update this on view changed
    QTransform viewTransform = QTransform::fromScale(view.m11(), view.m22());
    QRectF boundingRectView = viewTransform.inverted().mapRect(viewRect);
    m_graphicsView->setTransform(QTransform::fromTranslate(view.m31(),view.m32()));
    // m_graphicsScene->setSceneRect(boundingRectView.translated(-(view.m31()), -(view.m32())));
    m_graphicsScene->update();

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // draw canvas
    painter.fillRect(QRect(0, 0, width(), height()), Qt::white);
    painter.save();
    painter.setWorldMatrixEnabled(true);
    painter.setTransform(view);
    drawBackground(painter);
    painter.beginNativePainting();
    paintGLInit(painter.viewport().width(), painter.viewport().height(), false);
    drawCanvas(painter, k_drawGL);
    paintGLRelease(false);
    painter.endNativePainting();

    // fill canvas exterior
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(102, 102, 102));
    QRegion rg1(boundingRect);
    QRegion rg2(m_canvasRect);
    QRegion rg3 = rg1.subtracted(rg2);
    painter.setClipRegion(rg3);
    painter.drawRect(boundingRect);
    painter.setClipping(false);

    // draw canvas outline
    painter.setPen(Qt::black);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_canvasRect);
    painter.restore();

    sw.stop();
}

void TabletCanvas::drawKeyFrame(QPainter &painter, VectorKeyFrame *keyframe, int frame, int inbetween, int stride, const QColor &color, qreal opacity, qreal tintFactor, bool useGL) {
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    if (useGL) {
        glEnable(GL_BLEND);
        m_strokeProgram->bind();
        // TODO use m_editor->alpha(frame) to control the stroke size
        keyframe->paintImageGL(m_strokeProgram, context()->functions(), m_editor->alpha(frame), opacity, inbetween, color, tintFactor, m_drawGroupColor);
        m_strokeProgram->release();
    } else {
        painter.save();
        painter.setOpacity(opacity);
        keyframe->paintImage(painter, m_editor->alpha(frame), inbetween, color, tintFactor, m_drawGroupColor);
        painter.restore();
    }
}

void TabletCanvas::drawKeyFrame(QPainter &painter, VectorKeyFrame *keyframe, const QColor &color, qreal opacity, qreal tintFactor, GroupType type, bool useGL) {
    if (useGL) {
        glEnable(GL_BLEND);
        m_strokeProgram->bind();
        keyframe->paintImageGL(m_strokeProgram, context()->functions(), opacity, color, tintFactor, m_drawGroupColor, type);
        m_strokeProgram->release();
    } else {
        // TODO
        painter.save();
        painter.setOpacity(opacity);
        keyframe->paintImage(painter, 0.0, 0.0, color, tintFactor, m_drawGroupColor);
        painter.restore();
    }
}

void TabletCanvas::drawInterpolatedKeyFrame(QPainter &painter, Layer *layer, int frame, int inbetween, int stride, qreal opacity, qreal tintFactor, bool next, bool useGL) {
    if (layer == nullptr) return;
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(frame, 0);
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    QColor color = next ? m_editor->forwardColor() : m_editor->backwardColor();

    if (useGL) {
        glEnable(GL_BLEND);
        m_strokeProgram->bind();
        keyframe->paintImageGL(m_strokeProgram, context()->functions(), m_editor->alpha(frame), opacity, inbetween, color, tintFactor, m_drawGroupColor);
        m_strokeProgram->release();
    } else {
        painter.save();
        painter.setOpacity(opacity);
        keyframe->paintImage(painter, m_editor->alpha(frame), inbetween, color, tintFactor, m_drawGroupColor);
        painter.restore();
    }
}

void TabletCanvas::drawOnionSkins(QPainter &painter, Layer *layer, bool useGL) {
    const EqualizerValues &eqValues = m_editor->getEqValues();
    if (!layer->showOnion() || !eqValues.state[0]) return;
    // Draw previous frames
    int onionFrameNumber = m_editor->playback()->currentFrame();
    int currentFrame = m_editor->playback()->currentFrame();
    for (int i = -1; i >= -eqValues.maxDistance; i--) {
        qreal opacity = layer->opacity() * qreal(eqValues.value[0] * eqValues.value[i]) / 10000.;
        if (m_editor->getEqMode() == KEYS && !layer->keyExists(onionFrameNumber)) {
            onionFrameNumber = layer->getLastKeyFramePosition(onionFrameNumber);
        } else {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, m_editor->getEqMode() == KEYS);
        }
        if (onionFrameNumber <= 0) break;
        if (eqValues.state[i]) {
            VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(onionFrameNumber);
            qreal interpOpacity = (-eqValues.maxDistance - 1 - i) / (float)(-eqValues.maxDistance - 1);
            if (keyframe != nullptr) {
                drawKeyFrame(painter, keyframe, onionFrameNumber, 0, layer->stride(onionFrameNumber), m_editor->backwardColor(), interpOpacity * opacity, m_editor->tintFactor(), useGL);
            } else {
                int prevKeyNumber = layer->getLastKeyFramePosition(onionFrameNumber);
                int nextKeyNumber = layer->getNextKeyFramePosition(onionFrameNumber);
                drawInterpolatedKeyFrame(painter, layer, onionFrameNumber, onionFrameNumber - prevKeyNumber, nextKeyNumber - prevKeyNumber, interpOpacity * opacity, m_editor->tintFactor(),
                                         false, useGL);
            }
        }
    }
    // Draw next frames
    onionFrameNumber = m_editor->playback()->currentFrame();
    for (int i = 1; i <= eqValues.maxDistance; i++) {
        qreal opacity = layer->opacity() * qreal(eqValues.value[0] * eqValues.value[i]) / 10000.;
        onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, m_editor->getEqMode() == KEYS);
        if (onionFrameNumber >= layer->getMaxKeyFramePosition()) break;
        if (eqValues.state[i]) {
            VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(onionFrameNumber);
            qreal interpOpacity = (eqValues.maxDistance + 1 - i) / (float)(eqValues.maxDistance + 1);
            if (keyframe != nullptr) {
                drawKeyFrame(painter, keyframe, onionFrameNumber, 0, layer->stride(onionFrameNumber), m_editor->forwardColor(), interpOpacity * opacity, m_editor->tintFactor(), useGL);
            } else {
                int prevKeyNumber = layer->getLastKeyFramePosition(onionFrameNumber);
                int nextKeyNumber = layer->getNextKeyFramePosition(onionFrameNumber);
                drawInterpolatedKeyFrame(painter, layer, onionFrameNumber, onionFrameNumber - prevKeyNumber, nextKeyNumber - prevKeyNumber, interpOpacity * opacity, m_editor->tintFactor(),
                                         true, useGL);
            }
        }
    }
}

void TabletCanvas::drawSelectedGroupsLifetime(QPainter &painter, Layer *layer, VectorKeyFrame *keyframe, int frame, int inbetween, int stride, bool useGL) {
    int keyframeNumber = layer->getVectorKeyFramePosition(keyframe);

    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if (inbetween > 0) {
            drawGroup(painter, keyframe, group, 0.0f, 0, stride, layer->opacity(), Qt::darkGray, 100.0);
        }

        // Draw next breakdown keyframes and the last end keyframe
        Group *prev = group;
        Group *next = group->nextPostGroup();
        VectorKeyFrame *prevKey;
        int stride;
        while (next != nullptr) {
            prevKey = next->getParentKeyframe();
            stride = layer->stride(layer->getVectorKeyFramePosition(prevKey));
            drawGroup(painter, prevKey, next, 0.0f, 0, stride, layer->opacity(), prev == group ? Qt::darkGray : Qt::lightGray, 100.0);
            prev = next;
            next = next->nextPostGroup();
        }
        prevKey = prev->getParentKeyframe();
        stride = layer->stride(layer->getVectorKeyFramePosition(prevKey));
        drawGroup(painter, prevKey, prev, 1.0, stride, stride, 0.8, Qt::lightGray, 100.0, 0.7);

        // Draw previous breakdown keyframes
        prev = group->prevPostGroup();
        while (prev != nullptr) {
            prevKey = prev->getParentKeyframe();
            stride = layer->stride(layer->getVectorKeyFramePosition(prevKey));
            drawGroup(painter, prevKey, prev, 0.0f, 0, stride, layer->opacity(), Qt::lightGray, 100.0);
            prev = prev->prevPostGroup();
        }
    }
}

void TabletCanvas::drawGroup(QPainter &painter, VectorKeyFrame *keyframe, Group *group, float alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor, bool useGL) {
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    if (useGL) {
        glEnable(GL_BLEND);
        m_strokeProgram->bind();
        keyframe->paintGroupGL(m_strokeProgram, context()->functions(), alpha, opacity, group, inbetween, color, tint, strokeWeightFactor, false);
        m_strokeProgram->release();
    } else {
        painter.save();
        painter.setOpacity(opacity);
        // TODO
        painter.restore();
    }
}

void TabletCanvas::drawSelectedGroups(QPainter &painter, VectorKeyFrame *keyframe, GroupType type, float alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor, bool useGL) {
    if (!useGL) return; // TODO
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    const QHash<int, Group *> &groups = (type == POST) ? keyframe->selection().selectedPostGroups() : keyframe->selection().selectedPreGroups();
    glEnable(GL_BLEND);
    m_strokeProgram->bind();
    for (Group * group : groups) {
        keyframe->paintGroupGL(m_strokeProgram, context()->functions(), alpha, opacity, group, inbetween, color, tint, strokeWeightFactor, m_drawGroupColor, true);
    }
    m_strokeProgram->release();
}

void TabletCanvas::drawSelectedGroups(QPainter &painter, VectorKeyFrame *keyframe, GroupType type, double opacity, const QColor &color, double tint, double strokeWeightFactor, bool useGL) {
    if (!useGL) return; // TODO
    const QHash<int, Group *> &groups = (type == POST) ? keyframe->selection().selectedPostGroups() : keyframe->selection().selectedPreGroups();
    glEnable(GL_BLEND);
    m_strokeProgram->bind();
    for (Group * group : groups) {
        keyframe->paintGroupGL(m_strokeProgram, context()->functions(), opacity, group, color, tint, strokeWeightFactor, m_drawGroupColor);
    }
    m_strokeProgram->release();
}

void TabletCanvas::drawExportOnionSkins(QPainter &painter, Layer *layer, bool useGL) {
    int maxFrame = k_exportTo == 0 ? layer->getMaxKeyFramePosition() : k_exportTo;
    VectorKeyFrame *last = prevKeyFrame();
    if (!k_exportOnlyKeysMode) {
        for (int i = std::max(k_exportFrom.value(), layer->firstKeyFramePosition()); i < maxFrame; ++i) {
            if (!layer->keyExists(i)) {
                int prevKeyNumber = layer->getLastKeyFramePosition(i);
                int nextKeyNumber = layer->getNextKeyFramePosition(i);
                VectorKeyFrame *prevFrame = layer->getVectorKeyFrameAtFrame(prevKeyNumber);
                if (k_exportOnlyCurSegment && prevFrame != last) continue;
                drawInterpolatedKeyFrame(painter, layer, i, i - prevKeyNumber, nextKeyNumber - prevKeyNumber, 0.4, m_editor->tintFactor(), true, useGL);
            }
        }
    }
    for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
        if (it.key() < k_exportFrom) continue;
        if (it.key() >= maxFrame) continue;
        if (layer->keyExists(it.key())) {
            VectorKeyFrame *keyframe = it.value();
            if (k_exportOnlyCurSegment && keyframe != last) continue;
            drawKeyFrame(painter, keyframe, it.key(), 0, layer->stride(it.key()), Qt::black, layer->opacity(), 0.0, useGL);
        }
    }
    if (k_exportGhostFrame) {
        for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
            if (it.key() < k_exportFrom) continue;
            if (it.key() >= maxFrame) continue;
            if (layer->keyExists(it.key())) {
                VectorKeyFrame *keyframe = it.value();
                if (k_exportOnlyCurSegment && keyframe != last) continue;
                // draw ghost frame of selected groups
                for (Group *group : keyframe->selection().selectedPostGroups()) {
                    int stride = layer->stride(m_editor->playback()->currentFrame());
                    int ib = m_editor->updateInbetweens(keyframe, stride, stride);
                    glEnable(GL_BLEND);
                    m_strokeProgram->bind();
                    keyframe->paintGroupGL(m_strokeProgram, context()->functions(), 1.0f, 0.4, group, ib, m_editor->forwardColor(), 1.0, m_drawGroupColor, true);
                    m_strokeProgram->release();
                }
            }
        }
    }
}

/**
 * Draw all visible layers at the current frame
 */
void TabletCanvas::drawCanvas(QPainter &painter, bool useGL, bool drawOffscreen) {
    painter.save();

    for (int l = 0; l < m_editor->layers()->layersCount(); l++) {
        Layer *layer = m_editor->layers()->layerAt(l);
        if (!layer || !layer->visible()) continue;

        int currentFrame = m_editor->playback()->currentFrame();
        int nextKeyNumber = layer->getNextFrameNumber(currentFrame, true);
        int inbetween = layer->inbetweenPosition(currentFrame);
        int stride = layer->stride(currentFrame);
        qreal alphaLinear = m_editor->alpha(currentFrame);
        Layer *currentLayer = m_editor->layers()->currentLayer();
        VectorKeyFrame *prevKeyFrame = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
        VectorKeyFrame *nextKeyFrame = layer->getVectorKeyFrameAtFrame(nextKeyNumber);

        if (drawOffscreen && k_exportOnionSkinMode)
            drawExportOnionSkins(painter, layer, useGL);
        else
            drawOnionSkins(painter, layer, useGL);

        // Draw the selected group lifetime
        if (!m_editor->playback()->isPlaying() && !prevKeyFrame->selection().selectedPostGroups().empty()) {
            drawSelectedGroupsLifetime(painter, layer, prevKeyFrame, currentFrame, inbetween, stride, useGL);
        }

        // Draw the current frame (post & main group)
        StopWatch sw("Draw frame");
        if (!k_exportOnionSkinMode) drawKeyFrame(painter, prevKeyFrame, currentFrame, inbetween, stride, Qt::black, layer->opacity(), 0.0, useGL);
        sw.stop();

        if (!m_editor->playback()->isPlaying()) {
            // Draw all pre groups or the selected pre group
            if (m_drawPreGroupGhosts) {
                drawKeyFrame(painter, prevKeyFrame, Qt::darkBlue, 0.75, 100.0, PRE, useGL);
            } else {
                drawSelectedGroups(painter, prevKeyFrame, PRE, 0.75, Qt::darkBlue, 100.0, 1.0, useGL);
            }
            if (!prevKeyFrame->selection().selectedPreGroups().empty()) {
                drawSelectedGroups(painter, prevKeyFrame, PRE, 0.75, Qt::cyan, 100.0, 0.17, useGL);
            }

            // Draw the selected group skeleton
            if (!prevKeyFrame->selection().selectedPostGroups().empty()) {
                drawSelectedGroups(painter, prevKeyFrame, POST, alphaLinear, inbetween, stride, layer->opacity(), Qt::cyan, 100.0, 0.17);
            }
        }

        // Draw the stroke that is currently being drawn
        if (m_editor->layers()->currentLayerIndex() == l && m_deviceDown && m_editor->tools()->currentTool()->toolType() == Tool::Pen) {
            painter.save();
            painter.setOpacity(m_editor->layers()->currentLayer()->opacity());
            PenTool *penTool = static_cast<PenTool *>(m_editor->tools()->currentTool());
            if (useGL && penTool->currentStroke() != nullptr) {
                glEnable(GL_BLEND);
                m_strokeProgram->bind();
                if (!penTool->currentStroke()->buffersCreated()) penTool->currentStroke()->createBuffers(m_strokeProgram);
                m_strokeProgram->setUniformValue("stroke_color", penTool->currentStroke()->color());
                m_strokeProgram->setUniformValue("stroke_weight", (float)penTool->currentStroke()->strokeWidth());
                penTool->currentStroke()->updateBuffer();
                penTool->currentStroke()->render(GL_LINE_STRIP_ADJACENCY, context()->functions());
                m_strokeProgram->release();
            } else {
                if (penTool->currentStroke() != nullptr) penTool->currentStroke()->draw(painter, penTool->pen(), 0, penTool->currentStroke()->size() - 1);
            }
            painter.restore();
        }

        if (!m_editor->playback()->isPlaying() && prevKeyFrame->selectedGroup() && prevKeyFrame->selectedGroup()->lattice() && !prevKeyFrame->selectedGroup()->lattice()->isSingleConnectedComponent()) {
            painter.setPen(QPen(Qt::darkRed));
            painter.drawText(QPointF(width() / 2 - 200, height() / 2 - 100), "(!)");
        }
    }

    if (!m_editor->playback()->isPlaying() && !drawOffscreen) {
        drawToolGizmos(painter);
    }

    painter.restore();
}

void TabletCanvas::drawToolGizmos(QPainter &painter) {
    if (m_editor->tools()->currentTool() != nullptr) {
        m_editor->tools()->currentTool()->draw(painter, prevKeyFrame());
    }
}

void TabletCanvas::drawBackground(QPainter &painter) {
    if (m_backgrounds.isEmpty()) return;
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    int frame = layer->firstKeyFramePosition();
    int count = 0;
    if (k_backgroundOnKF) {
        while (frame != currentFrame) {
            if (frame == layer->getMaxKeyFramePosition()) break;
            frame = layer->getNextKeyFramePosition(frame);
            ++count;
        }
        count = std::min(count, (int)m_backgrounds.size() - 1);
    } else {
        count = std::min(currentFrame - 1, (int)m_backgrounds.size() - 1);
    }
    if (k_showBackground) painter.drawPixmap(-m_canvasRect.width() / 2, -m_canvasRect.height() / 2, m_backgrounds[count]);
}

void TabletCanvas::paintPixmap(QPainter &painter, QTabletEvent *event) {
    painter.setRenderHint(QPainter::Antialiasing, k_AA);

    switch (event->deviceType()) {
        case QInputDevice::DeviceType::Airbrush: {
            painter.setPen(Qt::NoPen);
            QRadialGradient grad(lastPoint.pos, m_pen.widthF() * 100.0);
            QColor color = m_brush.color();
            color.setAlphaF(color.alphaF() * 0.25);
            grad.setColorAt(0, m_brush.color());
            grad.setColorAt(0.5, Qt::transparent);
            painter.setBrush(grad);
            qreal radius = grad.radius();
            painter.drawEllipse(m_editor->view()->mapScreenToCanvas(event->position()), radius, radius);
        } break;
        case QInputDevice::DeviceType::Puck:
        case QInputDevice::DeviceType::Mouse: {
            const QString error(tr("This input device is not supported by the example."));
#ifndef QT_NO_STATUSTIP
            QStatusTipEvent status(error);
            QApplication::sendEvent(this, &status);
#else
            qWarning() << error;
#endif
        } break;
        default: {
            const QString error(tr("Unknown tablet device - treating as stylus"));
#ifndef QT_NO_STATUSTIP
            QStatusTipEvent status(error);
            QApplication::sendEvent(this, &status);
#else
            qWarning() << error;
#endif
        }
            Q_FALLTHROUGH();
        case QInputDevice::DeviceType::Stylus:
            if (event->pointingDevice()->capabilities().testFlag(QPointingDevice::Capability::Rotation)) {
                m_brush.setStyle(Qt::SolidPattern);
                painter.setPen(Qt::NoPen);
                painter.setBrush(m_brush);
                QPolygonF poly;
                qreal halfWidth = m_pen.widthF();
                QPointF brushAdjust(qSin(qDegreesToRadians(lastPoint.rotation)) * halfWidth, qCos(qDegreesToRadians(lastPoint.rotation)) * halfWidth);
                poly << lastPoint.pos + brushAdjust;
                poly << lastPoint.pos - brushAdjust;
                brushAdjust = QPointF(qSin(qDegreesToRadians(event->rotation())) * halfWidth, qCos(qDegreesToRadians(event->rotation())) * halfWidth);
                poly << m_editor->view()->mapScreenToCanvas(event->position()) - brushAdjust;
                poly << m_editor->view()->mapScreenToCanvas(event->position()) + brushAdjust;
                painter.drawConvexPolygon(poly);
            } else {
                painter.setPen(m_pen);
                painter.drawLine(lastPoint.pos, m_editor->view()->mapScreenToCanvas(event->position()));
            }
            break;
    }
}

void TabletCanvas::updateBrush(const QTabletEvent *event) {
    int hue, saturation, value, alpha;
    QColor newColor = m_editor->color()->frontColor();
    newColor.getHsv(&hue, &saturation, &value, &alpha);

    int vValue = int(((event->yTilt() + 60.0) / 120.0) * 255);
    int hValue = int(((event->xTilt() + 60.0) / 120.0) * 255);

    switch (m_alphaChannelValuator) {
        case PressureValuator:
            newColor.setAlphaF(event->pressure());
            break;
        case TangentialPressureValuator:
            if (event->deviceType() == QInputDevice::DeviceType::Airbrush)
                newColor.setAlphaF(qMax(0.01, (event->tangentialPressure() + 1.0) / 2.0));
            else
                newColor.setAlpha(255);
            break;
        case TiltValuator:
            newColor.setAlpha(maximum(abs(vValue - 127), abs(hValue - 127)));
            break;
        default:
            newColor.setAlpha(255);
    }

    switch (m_colorSaturationValuator) {
        case VTiltValuator:
            newColor.setHsv(hue, vValue, value, alpha);
            break;
        case HTiltValuator:
            newColor.setHsv(hue, hValue, value, alpha);
            break;
        case PressureValuator:
            newColor.setHsv(hue, int(event->pressure() * 255.0), value, alpha);
            break;
        default:;
    }

    switch (m_lineWidthValuator) {
        case PressureValuator:
            // m_pen.setWidthF(event->pressure() * k_penSize + 1);
            break;
        case TiltValuator:
            m_pen.setWidthF(maximum(abs(vValue - 127), abs(hValue - 127)) / 12);
            break;
        default:
            m_pen.setWidthF(1);
    }

    if (event->pointerType() == QPointingDevice::PointerType::Eraser) {
        m_brush.setColor(Qt::white);
        m_pen.setColor(Qt::white);
        // m_pen.setWidthF(event->pressure() * k_eraserSize + 1);
    } else {
        m_brush.setColor(newColor);
        m_pen.setColor(newColor);
    }
    m_editor->color()->setColor(newColor);
}

void TabletCanvas::updateCursor(const QTabletEvent *event) { setCursor(m_editor->tools()->currentTool()->makeCursor(m_editor->view()->scaling())); }

void TabletCanvas::updateCursor() { updateCursor(nullptr); }

void TabletCanvas::updateCursor(bool b) {
    Q_UNUSED(b);
    updateCursor(nullptr);
}

QCursor TabletCanvas::crossCursor(qreal width) {
    QPixmap pixmap(int(width + 2), int(width + 2));
    if (!pixmap.isNull()) {
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing, false);
        painter.setPen(QPen(Qt::white, 3));
        painter.drawLine(QPointF(width / 2., 1.), QPointF(width / 2., width - 1));
        painter.drawLine(QPointF(1., width / 2.), QPointF(width - 1, width / 2.));
        painter.setPen(QPen(Qt::black, 1));
        painter.drawLine(QPointF(width / 2., 1.), QPointF(width / 2., width - 1));
        painter.drawLine(QPointF(1., width / 2.), QPointF(width - 1, width / 2.));
    }
    return QCursor(pixmap.scaledToWidth(int(m_editor->view()->scaling() * pixmap.width())));
}

void TabletCanvas::loadBackgrounds(QString dir) {
    if (k_backgroundDir.value().isNull()) return;
    QDir backgroundDir(k_backgroundDir);
    if (!backgroundDir.exists()) return;

    m_backgrounds.clear();
    if (backgroundDir.isEmpty()) {
        return;
    }

    QStringList filters = QStringList() << "*.jpg"
                                        << "*.JPG"
                                        << "*.jpeg"
                                        << "*.JPEG"
                                        << "*.png"
                                        << ".PNG";
    QStringList backgroundsList = backgroundDir.entryList(filters, QDir::Files, QDir::Name);

    for (QString path : backgroundsList) {
        qDebug() << "loading : " << k_backgroundDir.value() + QString("/") + path;
        m_backgrounds.emplace_back(QPixmap(k_backgroundDir.value() + QString("/") + path));
    }

    updateCurrentFrame();
}

void TabletCanvas::updateDrawAggregate(bool draw) { update(); }

void TabletCanvas::selectAll() {
    VectorKeyFrame *key = currentKeyFrame();
    int layer = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    if (key == nullptr) return;
    m_editor->undoStack()->beginMacro("Select All");
    std::vector<int> groupsId;
    for (Group *group : key->postGroups()) {
        if (group->size() > 0) groupsId.push_back(group->id());
    }
    m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layer, currentFrame, groupsId, POST));
    m_editor->undoStack()->endMacro();
}

void TabletCanvas::deselectAll() {
    VectorKeyFrame *key = currentKeyFrame();
    int layer = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    if (key == nullptr) return;
    m_editor->undoStack()->beginMacro("Deselect All");
    m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layer, currentFrame, Group::ERROR_ID));
    m_editor->undoStack()->push(new SetSelectedTrajectoryCommand(m_editor, layer, currentFrame, nullptr));
    m_editor->undoStack()->endMacro();
}

VectorKeyFrame *TabletCanvas::currentKeyFrame() {
    Layer *layer = m_editor->layers()->currentLayer();
    return layer->getVectorKeyFrameAtFrame(m_editor->playback()->currentFrame());
}

VectorKeyFrame *TabletCanvas::prevKeyFrame() {
    Layer *layer = m_editor->layers()->currentLayer();
    return layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);
}

void TabletCanvas::debugReport() {
    Layer *layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();
    int layerIdx = m_editor->layers()->currentLayerIndex();

    qDebug() << "******* DEBUG REPORT";
    qDebug() << "";
    qDebug() << "** OpenGL";
    qDebug() << "Current thread: " << QThread::currentThread();
    qDebug() << "Active OpenGL context: " << QOpenGLContext::currentContext();
    qDebug() << "TabletCanvas OpenGL context: " << context();
    qDebug() << "Global share opengl context: " << QOpenGLContext::globalShareContext();

    qDebug() << "";
    qDebug() << "** Canvas";
    auto it = std::find(m_editor->layers()->indices().begin(), m_editor->layers()->indices().end(), layer->id());
    qDebug() << "Canvas rect: " << m_canvasRect; 
    qDebug() << ">>> Current layer: " << layer->name() << " | id: " << layer->id() << " | order in layers list: " << (it - m_editor->layers()->indices().begin()) << " <<<";
    qDebug() << ">>> Current keyframe (pos:" << layer->getVectorKeyFramePosition(key) << ", ptr:" << key << ") <<<";
    qDebug() << "Current frame: " << currentFrame;
    qDebug() << "First keyframe: " << layer->firstKeyFramePosition();
    qDebug() << "Last keyframe (invisible): " << layer->getMaxKeyFramePosition();
    qDebug() << "Nb of strokes: " << key->strokes().size();
    qDebug() << "Nb of post groups: " << key->postGroups().size();
    qDebug() << "Nb of pre groups: " << key->preGroups().size();
    qDebug() << "Correspondences (post->pre): ";
    for (auto it = key->correspondences().constBegin(); it != key->correspondences().constEnd(); ++it) {
        qDebug() << "   " << it.key() << " -> " << it.value();
    }
    qDebug() << "Intra-correspondences (pre->post): ";
    for (auto it = key->intraCorrespondences().constBegin(); it != key->intraCorrespondences().constEnd(); ++it) {
        qDebug() << "   " << it.key() << " -> " << it.value();
    }
    qDebug() << "Nb of trajectory constraints: " << key->nbTrajectoryConstraints();

    qDebug() << "";
    qDebug() << "** Selection";
    qDebug() << ">>> Selected post groups <<<";
    for (Group *group : key->selection().selectedPostGroups()) {
        qDebug() << "Group: " << group << "(" << group->id() << ")";
        qDebug() << "Nb of strokes: " << group->strokes().size();
        qDebug() << "Breakdown: " << group->breakdown();
        qDebug() << "Nb of forward UVs: " << group->uvs().size();
        qDebug() << "Nb of backward Uvs: " << group->backwardUVs().size();
        qDebug() << "Nb of trajectory constraints (grid): " << group->lattice()->nbConstraints();
        qDebug() << "Is the grid fully connected? " << group->lattice()->isConnected();
        qDebug() << "Motion energy (center of mass): " << group->lattice()->motionEnergy2D().norm();
        if (group->spacing()->curve()->nbPoints() >= 3) {
            qDebug() << "Start motion energy: " << group->motionEnergyStart();
            qDebug() << "End motion energy: " << group->motionEnergyEnd();
        }
        qDebug() << "Nb of control points (spacing): " << group->spacing()->curve()->nbPoints();
        qDebug() << "Pin errors: ";
        for (QuadPtr q : group->lattice()->hash()) {
            if (q->isPinned()) {
                qDebug() << "    " << (q->pinPos() - q->getPoint(q->pinUV(), TARGET_POS)).norm();
            }
        }
        qDebug() << "------";
    }
    if (key->selection().selectedTrajectoryPtr() != nullptr) {
        qDebug() << ">>> Selected trajectory <<<";
        qDebug() << "Local offsets: ";
        for (int i = 0; i < key->selection().selectedTrajectory()->localOffset()->curve()->nbPoints(); ++i) {
            Eigen::Vector2f p = key->selection().selectedTrajectory()->localOffset()->curve()->point(i);
            std::cout << p.transpose() << "   ";
        }
        std::cout << std::endl;
    }
    qDebug() << "******* END DEBUG REPORT";
}