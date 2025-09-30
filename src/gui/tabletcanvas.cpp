/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
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
#include "layoutmanager.h"
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

static dkBool k_AA("Pen->AAliasing", true);

// Background options
static dkFilename k_backgroundDir("Options->Backgrounds->Directory");
static dkBool k_showBackground("Options->Backgrounds->Show", true);
static dkBool k_backgroundOnKF("Options->Backgrounds->Keyframe", true);
// Drawing options
dkBool k_drawOffscreen("Options->Drawing->Draw offscreen", true);
dkBool k_drawTess("Options->Drawing->Draw tess", false);
dkBool k_drawSplat("Options->Drawing->Draw splat", true);
dkBool k_displayMask("Options->Drawing->Display mask", false);
dkBool k_displaySelectionUI("Options->Drawing->Display selection UI", true);
dkBool k_outputMask("Options->Drawing->Output mask", false);
dkBool k_displayPrevTarget("Options->Onion skin->Display prev target", false);
dkBool k_onionOnlySelected("Options->Onion skin->Only selected groups", false);
dkFloat k_thetaEps("Options->Drawing->Stroke drawing smoothness", 0.01, 0.00001, 1.0, 0.00001);
dkSlider k_gridEdgeSize("Options->Drawing->Grid edge size", 10, 1, 100, 1);
static dkSlider k_bitToVis("Options->Drawing->Bit to vis", 1, 0, 31, 1);
static dkBool k_visBitMask("Options->Drawing->Vis bitmask", false);
static dkSlider k_depthColorScaling("Options->Drawing->Depth color scaling", 20, 1, 100, 1);
// Export options
dkBool k_exportOnionSkinMode("Options->Export->Onion skin mode", false);
dkBool k_exportOnlyKeysMode("Options->Export->Export keys only (onion export)", false);
dkInt k_exportFrom("Options->Export->Export from", 1, 1, 100, 1);
dkInt k_exportTo("Options->Export->Export to", 0, 0, 100, 1);
dkBool k_exportOnlyCurSegment("Options->Export->Only current segment", false);
dkBool k_exportGhostFrame("Options->Export->Draw ghost frame", false);

extern dkBool k_drawMainGroupGrid;
extern dkSlider k_deformRange;
extern dkBool k_useInterpolation;

TabletCanvas::TabletCanvas()
    : QOpenGLWidget(nullptr),
      m_alphaChannelValuator(TangentialPressureValuator),
      m_colorSaturationValuator(NoValuator),
      m_lineWidthValuator(PressureValuator),
      m_brush(Qt::black),
      m_pen(m_brush, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
      m_deviceActive(false),
      m_deviceDown(false),
      m_button(Qt::NoButton),
      m_canvasRect(-960, -540, 1920, 1080),
      m_currentAlpha(0.0),
      m_drawGroupColor(false),
      m_drawPreGroupGhosts(false),
      m_displayVisibility(false),
      m_displayDepth(false),
      m_displayMask(false),
      m_displaySelectedGroupsLifetime(true),
      m_temporarySelectTool(false),
      m_displayMode(DisplayMode::StrokeColor),
      m_maskOcclusionMode(MaskOcclude),
      m_cursorVBO(QOpenGLBuffer::VertexBuffer) {
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);

    m_fixedGraphicsScene = new QGraphicsScene();
    m_fixedGraphicsView = new CanvasView(m_fixedGraphicsScene, m_editor, this, true);

    m_infernoColorMap = QImage(":/inferno");

    int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/PurisaMedium.ttf"));
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    m_canvasFont = QFont(family);

    m_infoMessageDuration.setSingleShot(true);
    connect(&m_infoMessageDuration, SIGNAL(timeout(void)), this, SLOT(updateCurrentFrame(void)));

    initPixmap();
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_MouseTracking);

    connect(&k_AA, SIGNAL(valueChanged(bool)), this, SLOT(updateCursor(bool)));
    connect(&k_drawOffscreen, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_drawTess, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_displayMask, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_displayMask, SIGNAL(valueChanged(bool)), this, SLOT(toggleDisplayMask(bool)));
    connect(&k_gridEdgeSize, SIGNAL(valueChanged(int)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_bitToVis, SIGNAL(valueChanged(int)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_visBitMask, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_backgroundDir, SIGNAL(valueChanged(QString)), this, SLOT(loadBackgrounds(QString)));
    connect(&k_showBackground, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_backgroundOnKF, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_displayPrevTarget, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_displaySelectionUI, SIGNAL(valueChanged(bool)), this, SLOT(updateCurrentFrame(void)));
    connect(&k_depthColorScaling, SIGNAL(valueChanged(int)), this, SLOT(updateCurrentFrame(void)));
    
    detectWhichOSX();

    setMouseTracking(false);
    setTabletTracking(false);    
}

TabletCanvas::~TabletCanvas() {
    makeCurrent();
    delete m_strokeProgram;
    delete m_displayProgram;
    delete m_splattingProgram;
    delete m_pointTex;
    delete m_maskTex;
    delete m_offscreenRenderFBO;
    delete m_offscreenRenderMSFBO;
    doneCurrent();
}

void TabletCanvas::resizeGL(int w, int h) {
    qreal ratio = devicePixelRatio();
    m_editor->view()->setDevicePixelRatio(ratio);
    m_editor->view()->setCanvasSize(QSize(w, h));
    m_fixedGraphicsView->setFixedSize(width(), height());
    m_fixedGraphicsScene->setSceneRect(rect());
    initPixmap();
    initializeFBO(ratio * w, ratio * h);
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

void TabletCanvas::showInfoMessage(const QString &message, int durationMs) {
    m_infoMessageDuration.start(durationMs);
    m_infoMessageText = message;
    update();
}

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
    m_button = Qt::NoButton;
    if (m_deviceActive) return;
    if (!m_deviceDown) {        
        m_button = event->button();

        m_deviceDown = true;
        lastPoint.pixel = event->position();
        lastPoint.pos = m_editor->view()->mapScreenToCanvas(event->position());
        lastPoint.rotation = 0.0;
        firstPoint = lastPoint;
        m_currentAlpha = m_editor->currentAlpha();
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
        } else if (m_temporarySelectTool) {
            m_editor->tools()->tool(Tool::Select)->pressed(info);
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
    // if (m_deviceDown) {
    QPointF pos = event->position();
    QPointF smoothPos = QPointF((pos.x() + lastPoint.pixel.x()) / 2.0, (pos.y() + lastPoint.pixel.y()) / 2.0);
    QPointF newPos = m_editor->view()->mapScreenToCanvas(smoothPos);

    Tool::EventInfo info;
    info.key = prevKeyFrame();
    info.firstPos = firstPoint.pos;
    info.lastPos = lastPoint.pos;
    info.pos = newPos;
    info.rotation = lastPoint.rotation;
    info.alpha = m_editor->currentAlpha();
    info.inbetween = m_inbetween;
    info.stride = m_stride;
    info.modifiers = event->modifiers();
    info.mouseButton = m_button;

    if (info.mouseButton & Qt::MiddleButton) {
        m_editor->tools()->tool(Tool::Hand)->moved(info);
    } else if (m_temporarySelectTool) {
        m_editor->tools()->tool(Tool::Select)->moved(info);
    } else {
        m_editor->tools()->currentTool()->moved(info);
    }

    lastPoint.pixel = smoothPos;
    lastPoint.pos = m_editor->view()->mapScreenToCanvas(smoothPos);  // remap because view may have changed
    lastPoint.rotation = 0.0;

    // }
    event->accept();
    update();
}

void TabletCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (m_deviceActive) return;
    m_deviceDown = false;

    QPointF pos = event->position();
    QPointF smoothPos = QPointF((pos.x() + lastPoint.pixel.x()) / 2.0, (pos.y() + lastPoint.pixel.y()) / 2.0);
    QPointF newPos = m_editor->view()->mapScreenToCanvas(smoothPos);

    Tool::EventInfo info;
    info.key = prevKeyFrame();
    info.firstPos = firstPoint.pos;
    info.lastPos = lastPoint.pos;
    info.pos = newPos;
    info.rotation = lastPoint.rotation;
    info.alpha = m_editor->currentAlpha();
    info.inbetween = m_inbetween;
    info.stride = m_stride;
    info.modifiers = event->modifiers();
    info.mouseButton = m_button;

    // override to Pan/Rotate tool if the middle mouse button is pressed
    if (info.mouseButton & Qt::MiddleButton) {
        m_editor->tools()->tool(Tool::Hand)->released(info);
    } else if (m_temporarySelectTool) {
        m_editor->tools()->tool(Tool::Select)->released(info);
    } else {
        m_editor->tools()->currentTool()->released(info);
    }

    m_button = Qt::NoButton;

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
                m_currentAlpha = m_editor->currentAlpha();
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


                if (info.mouseButton & Qt::MiddleButton) {
                    m_editor->tools()->tool(Tool::Hand)->pressed(info);
                } else if (m_temporarySelectTool) {
                    m_editor->tools()->tool(Tool::Select)->pressed(info);
                } else {
                    m_editor->tools()->currentTool()->pressed(info);
                }
            }
            break;
        case QEvent::TabletMove:
#ifndef Q_OS_IOS
            if (event->pointingDevice() && event->pointingDevice()->capabilities().testFlag(QPointingDevice::Capability::Rotation)) updateCursor(event);
#endif
            {
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

                if (info.mouseButton & Qt::MiddleButton) {
                    m_editor->tools()->tool(Tool::Hand)->moved(info);
                } else if (m_temporarySelectTool) {
                    m_editor->tools()->tool(Tool::Select)->moved(info);
                } else {
                    m_editor->tools()->currentTool()->moved(info);
                }

                lastPoint.pixel = event->position();
                lastPoint.pos = m_editor->view()->mapScreenToCanvas(event->position());  // remap because view may have changed
                lastPoint.rotation = 0.0;
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
                info.pos = m_editor->view()->mapScreenToCanvas(event->position());
                info.rotation = event->rotation();
                info.pressure = event->pressure();
                info.alpha = m_currentAlpha;
                info.inbetween = m_inbetween;
                info.stride = m_stride;
                info.mouseButton = m_button;
                info.modifiers = event->modifiers();

                if (info.mouseButton & Qt::MiddleButton) {
                    m_editor->tools()->tool(Tool::Hand)->released(info);
                } else if (m_temporarySelectTool) {
                    m_editor->tools()->tool(Tool::Select)->released(info);
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
                m_temporarySelectTool = true;
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
                m_temporarySelectTool = false;
                if (m_editor->tools()->currentTool() != nullptr && m_editor->tools()->currentTool()->isChartTool()) {
                    m_editor->fixedScene()->updateChartMode(ChartItem::PARTIAL);
                    m_editor->fixedScene()->updateKeyChart(m_editor->prevKeyFrame());
                }
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
    if (tool == nullptr || !tool->contextMenuAllowed()) {
        return;
    }

    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *keyframe = prevKeyFrame();
    Point::VectorType pos = QE_POINT(m_editor->view()->mapScreenToCanvas(event->pos()));
    int inbetween = layer->inbetweenPosition(currentFrame);
    QuadPtr q;
    int k;
    QMenu contextMenu(this);
    bool groupFound = false;
    // auto registerRestPosition = [this]() { m_editor->registerFromRestPosition(); };
    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if ((inbetween == 0 && group->lattice()->contains(pos, REF_POS, q, k)) || (inbetween > 0 && keyframe->inbetween(inbetween).contains(group, pos, q, k))) {
            contextMenu.addSection("Group");
            contextMenu.addAction(tr("Clone groups forward"), this, [&]() { m_editor->copyGroupToNextKeyFrame(false); });
            contextMenu.addAction(tr("Clone groups forward (breakdown)"), this, [&]() { m_editor->copyGroupToNextKeyFrame(true); });
            contextMenu.addAction(tr("Split groups"), m_editor, &Editor::splitGridIntoSingleConnectedComponent);
            contextMenu.addAction(tr("Delete groups"), m_editor, &Editor::deleteGroup);
            contextMenu.addSeparator();
            contextMenu.addAction(tr("Matching"), this, [&](){ m_editor->registerFromRestPosition();});
            contextMenu.addAction(tr("Matching from current state"), m_editor, &Editor::registerFromTargetPosition);
            contextMenu.addSeparator();
            contextMenu.addAction(tr("Toggle cross-fade"), m_editor, &Editor::toggleCrossFade);
            contextMenu.addAction(tr("Fade-out"), m_editor, &Editor::makeGroupFadeOut);
            contextMenu.addAction(tr("Change grid size"), m_editor, &Editor::changeGridSize);
            groupFound = true;
            break;
        }
    }
    if (!groupFound) {
        contextMenu.addSection("Keyframe");
        contextMenu.addAction(tr("Matching"), this, [&](){ m_editor->registerFromRestPosition();});
        contextMenu.addAction(tr("Matching from current state"), m_editor, &Editor::registerFromTargetPosition);
        contextMenu.addAction(tr("Clear keyframe"), m_editor, &Editor::clearCurrentFrame);
        contextMenu.addAction(tr("Add breakdown"), m_editor, &Editor::convertToBreakdown);
        contextMenu.addAction(tr("Suggest visibility change"), m_editor, &Editor::suggestVisibilityThresholds);
        contextMenu.addAction(tr("Suggest layout change"), m_editor, &Editor::suggestLayoutChange);
        contextMenu.addAction(tr("Propagate layout forward"), m_editor, &Editor::propagateLayoutForward);
        contextMenu.addAction(tr("Propagate layout backward"), m_editor, &Editor::propagateLayoutBackward);
    }
    if (tool != nullptr) {
        tool->contextMenu(contextMenu);
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
    m_offscreenRenderFBO->addColorAttachment(w, h, GL_RG32F); // mask
    glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void TabletCanvas::initializeGL() {
    initializeOpenGLFunctions();

    qreal ratio = devicePixelRatio();
    initializeFBO(ratio * m_canvasRect.width(), ratio * m_canvasRect.height());

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
    m_strokeViewLocation = m_strokeProgram->uniformLocation("view");
    m_strokeProjLocation = m_strokeProgram->uniformLocation("proj");
    m_strokeWinSize = m_strokeProgram->uniformLocation("winSize");
    m_strokeZoom = m_strokeProgram->uniformLocation("zoom");
    m_strokeThetaEpsilon = m_strokeProgram->uniformLocation("thetaEpsilon");
    m_strokeColor = m_strokeProgram->uniformLocation("strokeColor");

    m_displayProgram = new QOpenGLShaderProgram();
    result = m_displayProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/display.vert");
    if (!result) qCritical() << m_displayProgram->log();
    result = m_displayProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/display.frag");
    if (!result) qCritical() << m_displayProgram->log();
    result = m_displayProgram->link();
    if (!result) qCritical() << m_displayProgram->log();
    m_displayProgram->setUniformValue("offscreen", 0);
    m_displayVAO.create();
    if (m_displayVAO.isCreated()) m_displayVAO.bind();
    static const GLfloat quadBufferData[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
    m_displayVBO.create();
    m_displayVBO.bind();
    m_displayVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_displayVBO.allocate(quadBufferData, sizeof(quadBufferData));
    int vertexLocation = m_displayProgram->attributeLocation("vertex");
    m_displayProgram->enableAttributeArray(vertexLocation);
    m_displayProgram->setAttributeBuffer(vertexLocation, GL_FLOAT, 0, 2);
    m_displayVBO.release();
    m_displayVAO.release();

    m_maskProgram = new QOpenGLShaderProgram();
    result = m_maskProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/mask.vert");
    if (!result) qCritical() << m_maskProgram->log();
    result = m_maskProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/mask.frag");
    if (!result) qCritical() << m_maskProgram->log();
    result = m_maskProgram->link();
    if (!result) qCritical() << m_maskProgram->log();

    m_splattingProgram = new QOpenGLShaderProgram();
    result = m_splattingProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/splatting.vert");
    if (!result) qCritical() << m_splattingProgram->log();
    result = m_splattingProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/splatting.frag");
    if (!result) qCritical() << m_splattingProgram->log();
    result = m_splattingProgram->link();
    if (!result) qCritical() << m_splattingProgram->log();

    m_displayMaskProgram = new QOpenGLShaderProgram();
    result = m_displayMaskProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/fill.vert");
    if (!result) qCritical() << m_displayMaskProgram->log();
    result = m_displayMaskProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fill.frag");
    if (!result) qCritical() << m_displayMaskProgram->log();
    result = m_displayMaskProgram->link();
    if (!result) qCritical() << m_displayMaskProgram->log();

    m_displayGridProgram = new QOpenGLShaderProgram();
    result = m_displayGridProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/latticeFill.vert");
    if (!result) qCritical() << m_displayGridProgram->log();
    result = m_displayGridProgram->addShaderFromSourceFile(QOpenGLShader::Geometry, ":/shaders/latticeFill.geom");
    if (!result) qCritical() << m_displayGridProgram->log();
    result = m_displayGridProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/latticeFill.frag");
    if (!result) qCritical() << m_displayGridProgram->log();
    result = m_displayGridProgram->link();
    if (!result) qCritical() << m_displayGridProgram->log();

    m_cursorProgram = new QOpenGLShaderProgram();
    result = m_cursorProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/cursor.vert");
    if (!result) qCritical() << m_cursorProgram->log();
    result = m_cursorProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/cursor.frag");
    if (!result) qCritical() << m_cursorProgram->log();
    result = m_cursorProgram->link();
    if (!result) qCritical() << m_cursorProgram->log();
    m_cursorVAO.create();
    if (m_cursorVAO.isCreated()) m_cursorVAO.bind();
    m_cursorVBO.create();
    m_cursorVBO.bind();
    m_cursorVBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    int cursorPosLoc = m_cursorProgram->attributeLocation("cursorPos");
    m_cursorProgram->enableAttributeArray(cursorPosLoc);
    m_cursorProgram->setAttributeBuffer(cursorPosLoc, GL_FLOAT, 0, 2);
    m_cursorVBO.release();
    m_cursorVAO.release();

    m_pointTex = new QOpenGLTexture(QImage(":/images/brush/chunky.png"));
    m_pointTex->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    m_maskTex = new QOpenGLTexture(QImage(":/images/brush/brush2.png"));
    m_maskTex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
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
        info.alpha = m_editor->currentAlpha();
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
        update();
    }

    event->accept();
}

/**
 * Texture 0: offscreen canvas (for the final display)
 * Texture 1: mask strength
 * Texture 2: brush splat 1
 * Texture 3: brush splat 2
 * 
 * @param offW 
 * @param offH 
 * @param drawOffscreen 
 * @param exportFrames 
 */
void TabletCanvas::paintGLInit(int offW, int offH, bool drawOffscreen, bool exportFrames) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    double scaleW = offW / m_canvasRect.width();
    double scaleH = offH / m_canvasRect.height();

    if (exportFrames) {
        glViewport(0, 0, offW, offH);
    }

    QTransform view = exportFrames ? QTransform().scale(scaleW, scaleH).translate(m_canvasRect.width() / 2, m_canvasRect.height() / 2) : m_editor->view()->getView();
    QMatrix4x4 proj;
    proj.ortho(QRect(0, 0, offW, offH));

    if (drawOffscreen) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[0]);

        // Clear mask buffer
        m_offscreenRenderFBO->bind();
        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, drawBuffers);
        static const float black[] = {0.0f, 0.0f, 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, black);
        glClear(GL_DEPTH_BUFFER_BIT);
        m_maskProgram->bind();
        m_maskProgram->setUniformValue("view", view);
        if (exportFrames) {
            m_maskProgram->setUniformValue("proj", proj);
        } else {
            m_maskProgram->setUniformValue("proj", m_projMat);
        }
        m_maskProgram->release();
        m_offscreenRenderFBO->release();

        // Clear canvas buffer
        if (!m_offscreenRenderMSFBO->bind()) qDebug() << "Cannot bind offscreen FBO";
        glClearColor(1.0, 1.0, 1.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    m_strokeProgram->bind();
    m_strokeProgram->setUniformValue(m_strokeViewLocation, view);
    if (exportFrames) {
        m_strokeProgram->setUniformValue(m_strokeProjLocation, proj);
        m_strokeProgram->setUniformValue(m_strokeWinSize, QVector2D(offW, offH));
        m_strokeProgram->setUniformValue(m_strokeZoom, (GLfloat)(scaleW));
    } else {
        m_strokeProgram->setUniformValue(m_strokeProjLocation, m_projMat);
        m_strokeProgram->setUniformValue(m_strokeZoom, (GLfloat)m_editor->view()->scaling());
    }
    m_strokeProgram->setUniformValue(m_strokeWinSize, QVector2D(offW, offH));
    m_strokeProgram->setUniformValue(m_strokeThetaEpsilon, (float)k_thetaEps);
    m_strokeProgram->setUniformValue("maskMode", (int)m_maskOcclusionMode);
    m_strokeProgram->setUniformValue("displayVisibility", m_displayVisibility && !m_editor->playback()->isPlaying());
    m_strokeProgram->setUniformValue("displayMode", (int)m_displayMode);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[1]);
    m_strokeProgram->setUniformValue("maskStrength", 1);
    m_strokeProgram->release();

    m_displayMaskProgram->bind();
    m_displayMaskProgram->setUniformValue("view", view);
    if (exportFrames) {
        m_displayMaskProgram->setUniformValue("proj", proj);
    } else {
        m_displayMaskProgram->setUniformValue("proj", m_projMat);
    }
    m_displayMaskProgram->release();

    m_displayGridProgram->bind();
    m_displayGridProgram->setUniformValue("view", view);
    m_displayGridProgram->setUniformValue("proj", m_projMat);
    m_displayGridProgram->release();

    m_cursorProgram->bind();
    m_cursorProgram->setUniformValue("view", view);
    m_cursorProgram->setUniformValue("proj", m_projMat);
    m_cursorProgram->setUniformValue("cursorDiameter", (GLfloat)m_editor->view()->scaling() * k_deformRange);
    m_cursorProgram->setUniformValue("winSize", QVector2D(offW, offH));
    m_cursorProgram->setUniformValue("zoom", (GLfloat)m_editor->view()->scaling());
    m_cursorProgram->release();

    if (k_drawTess) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    if (k_drawSplat) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_pointTex->textureId());
        m_splattingProgram->bind();
        m_splattingProgram->setUniformValue("tex", 2);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_maskTex->textureId());
        m_splattingProgram->setUniformValue("texMask", 3);
        m_splattingProgram->setUniformValue("winSize", QVector2D(offW, offH));
        m_splattingProgram->setUniformValue("view", view);
        if (exportFrames) {
            view = QTransform().scale(scaleW, scaleH).translate(m_canvasRect.width() / 2, m_canvasRect.height() / 2);
            m_splattingProgram->setUniformValue("view", view);
            m_splattingProgram->setUniformValue("proj", proj);
            m_splattingProgram->setUniformValue("zoom", (GLfloat)(scaleW));
        } else {
            m_splattingProgram->setUniformValue("proj", m_projMat);
            m_splattingProgram->setUniformValue("zoom", (GLfloat)m_editor->view()->scaling());
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_offscreenRenderFBO->textures()[1]);
        m_splattingProgram->setUniformValue("maskStrength", 1);
        m_splattingProgram->setUniformValue("maskMode", (int)m_maskOcclusionMode);
        m_splattingProgram->setUniformValue("displayVisibility", m_displayVisibility && !m_editor->playback()->isPlaying());
        m_splattingProgram->setUniformValue("displayMode", (int)m_displayMode);
        m_splattingProgram->release();
    }
}

void TabletCanvas::paintGLRelease(bool drawOffscreen) {
    if (k_drawTess) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    m_strokeProgram->release();
    if (drawOffscreen) {
        m_offscreenRenderMSFBO->release();
    }
}

void TabletCanvas::paintGL() {
    StopWatch sw("rendering");

    QPainter painter(this);
    QTransform view = m_editor->view()->getView();
    QRectF viewRect = painter.viewport();
    QRect boundingRect = m_editor->view()->mapScreenToCanvas(viewRect).toRect();

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw background
    painter.fillRect(QRect(0, 0, width(), height()), Qt::white);
    painter.save();
    painter.setWorldMatrixEnabled(true);
    painter.setTransform(view);
    drawBackground(painter);

    // Fill canvas exterior
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(102, 102, 102));
    QRegion rg1(boundingRect);
    QRegion rg2(m_canvasRect);
    QRegion rg3 = rg1.subtracted(rg2);
    painter.setClipRegion(rg3);
    painter.drawRect(boundingRect);
    painter.setClipping(false);

    // Draw canvas outline
    painter.setPen(Qt::black);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_canvasRect);
    painter.restore();

    // Draw canvas (potentially offscreen)
    painter.beginNativePainting();
    paintGLInit(painter.viewport().width(), painter.viewport().height(), k_drawOffscreen, false);
    drawCanvas(false);
    paintGLRelease(k_drawOffscreen);

    // If the canvas was rendered offscreen, blend it to the canvas
    if (k_drawOffscreen) {
        qreal ratio = devicePixelRatio();
        // m_offscreenRenderMSFBO->toImage(true).save(QString("MSFBO.png"));
        QOpenGLFramebufferObject::blitFramebuffer(m_offscreenRenderFBO, m_offscreenRenderMSFBO);
        QOpenGLFramebufferObject::bindDefault();
        // m_offscreenRenderFBO->toImage(true).save(QString("FBO.png"));
        m_displayProgram->bind();
        m_displayVAO.bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_displayVAO.release();
        m_displayProgram->release();
    }
    painter.endNativePainting();

    // Draw tool UI
    painter.setWorldMatrixEnabled(true);
    painter.setTransform(view);
    drawToolGizmos(painter);

    sw.stop();
}

/**
 * Draw all visible layers at the current frame
 */
void TabletCanvas::drawCanvas(bool exportFrames) {
    StopWatch s("Draw canvas");
    k_drawSplat.setValue(true);

    Layer *currentLayer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();

    for (int l = m_editor->layers()->layersCount() - 1; l >= 0; l--) {
        Layer *layer = m_editor->layers()->layerAt(l);
        if (!layer || !layer->visible()) continue;

        int inbetween = layer->inbetweenPosition(currentFrame);
        int stride = layer->stride(currentFrame);
        int nextKeyNumber = layer->getNextFrameNumber(currentFrame, true);
        qreal alphaLinear = m_editor->alpha(currentFrame, layer);
        VectorKeyFrame *prevKeyFrame = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
        VectorKeyFrame *nextKeyFrame = layer->getVectorKeyFrameAtFrame(nextKeyNumber);

        // Draw onion skin
        if (exportFrames && k_exportOnionSkinMode)
            drawExportOnionSkins(layer);
        else
            drawOnionSkins(layer);

        // Draw the selected group lifetime
        if (!m_editor->playback()->isPlaying() && !prevKeyFrame->selection().selectedPostGroups().empty() && m_displaySelectedGroupsLifetime && k_displaySelectionUI) {
            drawSelectedGroupsLifetime(layer, prevKeyFrame, currentFrame, inbetween, stride);
        }

        if (!m_editor->playback()->isPlaying() && k_displaySelectionUI && !m_displayMask) {
            // Draw all pre groups or the selected pre group
            if (m_drawPreGroupGhosts) {
                drawKeyFrame(prevKeyFrame, Qt::darkBlue, 0.75, 100.0, PRE);
            } else {
                drawSelectedGroups(prevKeyFrame, PRE, 0.75, Qt::darkBlue, 100.0, 1.0);
            }
            if (!prevKeyFrame->selection().selectedPreGroups().empty()) {
                drawSelectedGroups(prevKeyFrame, PRE, 0.75, Qt::cyan, 100.0, 0.17);
            }

            // Draw the selected group skeleton
            // if (!prevKeyFrame->selection().selectedPostGroups().empty()) {
            //     drawSelectedGroups(prevKeyFrame, POST, alphaLinear, inbetween, stride, layer->opacity(), QColor(0, 129, 189), 100.0, std::min(4.0, std::max(1.5, 1.5 / m_editor->view()->scaling())));
            // }
        }

        // Draw the current frame
        StopWatch sw("Draw frame");
        if (!k_exportOnionSkinMode) drawKeyFrame(prevKeyFrame, currentFrame, inbetween, stride, Qt::black, layer->opacity(), 0.0);
        sw.stop();

        // Draw the stroke that is currently being drawn
        if (m_editor->layers()->currentLayerIndex() == l && m_deviceDown && m_editor->tools()->currentTool()->toolType() == Tool::Pen || m_editor->tools()->currentTool()->toolType() == Tool::MaskPen) {
            PenTool *penTool = static_cast<PenTool *>(m_editor->tools()->currentTool());
            if (penTool->currentStroke() != nullptr) {
                if  (k_drawSplat && k_drawOffscreen) startDrawSplatStrokes();
                QOpenGLShaderProgram *program = k_drawSplat ? m_splattingProgram : m_strokeProgram; 
                program->bind();
                !penTool->currentStroke()->buffersCreated() ? penTool->currentStroke()->createBuffers(program, prevKeyFrame) : penTool->currentStroke()->updateBuffer(prevKeyFrame);
                int cap[2] = {0, (int)penTool->currentStroke()->size()-1};
                program->setUniformValue("jitter", QTransform());
                program->setUniformValue("strokeWeight", (float)penTool->currentStroke()->strokeWidth());
                program->setUniformValue("strokeColor", penTool->currentStroke()->color());
                program->setUniformValue("ignoreMask", true);
                program->setUniformValue("time", (GLfloat)0.0);
                program->setUniformValue("stride", stride);
                program->setUniformValueArray("capIdx", cap, 2);
                penTool->currentStroke()->render(k_drawSplat ? GL_POINTS : GL_LINE_STRIP_ADJACENCY, context()->functions());
                program->release();
                if  (k_drawSplat && k_drawOffscreen) endDrawSplatStrokes();
            }
        }

        // if (!m_editor->playback()->isPlaying() && prevKeyFrame->selectedGroup() && prevKeyFrame->selectedGroup()->lattice() &&
        // !prevKeyFrame->selectedGroup()->lattice()->isSingleConnectedComponent()) {
        //     painter.setPen(QPen(Qt::darkRed));
        //     painter.drawText(QPointF(width() / 2 - 200, height() / 2 - 100), "(!)");
        // }
        // drawMask(prevKeyFrame, inbetween, stride, alphaLinear);

        if (k_outputMask) {
            // maskOutput.save(QString("mask-output") + QString::number(inbetween) + QString(".png"));
            QImage maskOutput = m_offscreenRenderFBO->toImage(true, 1);
            maskOutput.save(QString("mask-output-%1.png").arg(currentFrame));
            // QImage maskOutput2 = m_offscreenRenderFBO->toImage(true, 2);
            // maskOutput2.save("mask-output2.png");
        }
    }

    if (m_editor->tools()->currentTool() != nullptr && !m_editor->playback()->isPlaying()) {
        m_editor->tools()->currentTool()->drawGL(prevKeyFrame(), m_editor->currentAlpha());
    }
    s.stop();
}

void TabletCanvas::drawKeyFrame(VectorKeyFrame *keyframe, int frame, int inbetween, int stride, const QColor &color, qreal opacity, qreal tintFactor, bool drawMasks) {
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    if (k_drawSplat && k_drawOffscreen) {
        startDrawSplatStrokes();
    }

    if (m_displayDepth) tintFactor = 100.0;
    QColor c = color;

    QOpenGLShaderProgram *program = k_drawSplat ? m_splattingProgram : m_strokeProgram;
    double alpha = k_useInterpolation ? ((double)inbetween / stride) : 0.0;
    int size = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;
    int d = 0;
    if (m_displayMask || k_displayMask) {

        // Draw mask with colors (back to front)
        for (int i = size; i >= 0; --i) {
            const std::vector<int> &groups = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().at(i);
            if (drawMasks) {
                for (int groupId : groups) {
                    drawMask(keyframe, keyframe->postGroups().fromId(groupId), inbetween, stride, m_editor->alpha(frame), d);
                }
            }
            d++;
        }

        // Draw mask in the special stencil framebuffer (back to front)
        bool tmp = m_displayMask;
        m_displayMask = false;
        d = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;
        for (const std::vector<int> &groups : keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order()) {
            if (drawMasks) {
                for (int groupId : groups) {
                    drawMask(keyframe, keyframe->postGroups().fromId(groupId), inbetween, stride, m_editor->alpha(frame), d);
                }
            }
            d--;
        }
        m_displayMask = tmp;
        for (int i = size; i >= 0; --i) {
            const std::vector<int> &groups = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().at(i);
            // if (m_displayDepth) c = QColor::fromHsvF(Utils::lerp(0.07777, 0.0, (double)i/size), 1.0, 0.8);
            if (m_displayDepth) c = sampleColorMap(i + 0.25);

            program->bind();
            int size = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;
            program->setUniformValue("depth", (size-i)/(float)(size + 1));
            for (int groupId : groups) {
                if (keyframe->selection().selectedPostGroups().contains(groupId)) {
                    keyframe->paintGroupGL(program, context()->functions(), m_editor->alpha(frame), keyframe->parentLayer()->opacity(), keyframe->postGroups().fromId(groupId), inbetween, QColor(0, 129, 189), 100.0, std::min(4.0, std::max(2.0, 2.0 / m_editor->view()->scaling())), false, true, true);
                }
                keyframe->paintGroupGL(program, context()->functions(), m_editor->alpha(frame), opacity, keyframe->postGroups().fromId(groupId), inbetween, c, tintFactor, 1.0, m_drawGroupColor, true, !drawMasks);
            }
            program->release();
        }
    } else {
        d = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;
        for (int i = size; i >= 0; --i) {
            const std::vector<int> &groups = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().at(i);
            if (drawMasks) {
                for (int groupId : groups) {
                    drawMask(keyframe, keyframe->postGroups().fromId(groupId), inbetween, stride, m_editor->alpha(frame), d);
                }
            }
            d--;
        }

        program->bind();
        int size = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;
        for (int i = size; i >= 0; --i) {
            const std::vector<int> &groups = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().at(i);
            if (m_displayDepth) c = sampleColorMap(i + 0.25);

            program->setUniformValue("depth", ((size - i)/(float)(size + 1)));
            for (int groupId : groups) {
                keyframe->paintGroupGL(program, context()->functions(), m_editor->alpha(frame), opacity, keyframe->postGroups().fromId(groupId), inbetween, c, tintFactor, 1.0, m_drawGroupColor, true, !drawMasks);
            }
        }
        program->release();

        // for (const std::vector<int> &groups : keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order()) {
        //     // if (m_displayDepth) c = QColor::fromHsvF(Utils::lerp(0.07777, 0.0, (double)d/size), 1.0, 0.9);
        //     if (m_displayDepth) c = sampleColorMap(keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - d);
        //     program->bind();
        //     program->setUniformValue("depth", d / (float)(keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size()));
        //     for (int groupId : groups) {
        //         keyframe->paintGroupGL(program, context()->functions(), m_editor->alpha(frame), opacity, keyframe->postGroups().fromId(groupId), inbetween, c, tintFactor, 1.0, m_drawGroupColor, true, !drawMasks);
        //     }
        //     program->release();
        //     if (drawMasks) {
        //         for (int groupId : groups) {
        //             drawMask(keyframe, keyframe->postGroups().fromId(groupId), inbetween, stride, m_editor->alpha(frame), d);
        //         }
        //     }
        //     d++;
        // }
    }

    if (k_drawSplat && k_drawOffscreen) {
        endDrawSplatStrokes();
    }
}

void TabletCanvas::drawKeyFrame(VectorKeyFrame *keyframe, const QColor &color, qreal opacity, qreal tintFactor, GroupType type, bool drawMasks) {
    if (k_drawSplat && k_drawOffscreen) {
        startDrawSplatStrokes();
    }

    QOpenGLShaderProgram *program = k_drawSplat ? m_splattingProgram : m_strokeProgram;
    int d = 0;
    for (const std::vector<int> &groups : keyframe->orderPartials().firstPartial().groupOrder().order()) {
        program->bind();
        for (int groupId : groups) {
            keyframe->paintGroupGL(program, context()->functions(), opacity, keyframe->postGroups().fromId(groupId), color, tintFactor, 1.0, m_drawGroupColor, !drawMasks);
        }
        program->release();
        if (drawMasks) {
            for (int groupId : groups) {
                drawMask(keyframe, keyframe->postGroups().fromId(groupId), 0, 0, 0.0f, d);
            }
        }
        d++;
    }

    if (k_drawSplat && k_drawOffscreen) {
        endDrawSplatStrokes();
    }
}

void TabletCanvas::drawGrid(Group *group) {
    if (group->lattice() == nullptr) return;
    m_displayGridProgram->bind();
    m_displayGridProgram->setUniformValue("latticeColor", group->color());
    m_displayGridProgram->setUniformValue("edgeSize", k_gridEdgeSize.value() / 100.f);
    m_displayGridProgram->setUniformValue("bitToVis", k_bitToVis.value());
    m_displayGridProgram->setUniformValue("visBitmask", k_visBitMask.value());
    if (!group->lattice()->isBufferCreated()) {
        group->lattice()->createBuffer(m_displayGridProgram, context()->extraFunctions());
    } else {
        group->lattice()->updateBuffer();
    }
    group->lattice()->bindVAO();
    glDrawElements(GL_LINES_ADJACENCY, group->lattice()->quads().size() * 4, GL_UNSIGNED_INT, nullptr);
    group->lattice()->releaseVAO();
    m_displayGridProgram->release();
}

void TabletCanvas::drawCircleCursor(QVector2D nudge) {
    // TODO draw a quad instead of a point because of max point size limitations on certain hardware
    m_cursorProgram->bind();
    m_cursorProgram->setUniformValue("nudge", nudge);
    m_cursorVAO.bind();
    m_cursorVBO.bind();
    QPointF pos = m_editor->view()->mapScreenToCanvas(QPointF(mapFromGlobal(QCursor::pos())));
    GLfloat cursorPos[2] = {(GLfloat)pos.x(), (GLfloat)pos.y()};
    m_cursorVBO.allocate(cursorPos, 2 * sizeof(GLfloat));
    glDrawArrays(GL_POINTS, 0, 1);
    m_cursorVBO.release();
    m_cursorVAO.release();
    m_cursorProgram->release();
}

void TabletCanvas::drawOnionSkins(Layer *layer) {
    const EqualizerValues &eqValues = m_editor->getEqValues();
    if (!layer->showOnion() || !eqValues.state[0]) return;
    // Draw previous frames
    int onionFrameNumber = m_editor->playback()->currentFrame();
    int currentFrame = m_editor->playback()->currentFrame();
    for (int i = -1; i >= -eqValues.maxDistance; i--) {
        qreal opacity = layer->opacity() * qreal(eqValues.value[i]) / 100.0;
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
                drawKeyFrame(keyframe, onionFrameNumber, 0, layer->stride(onionFrameNumber), m_editor->backwardColor(), interpOpacity * opacity, m_editor->tintFactor(), false);
            } else {
                int prevKeyNumber = layer->getLastKeyFramePosition(onionFrameNumber);
                int nextKeyNumber = layer->getNextKeyFramePosition(onionFrameNumber);
                drawKeyFrame(layer->getLastVectorKeyFrameAtFrame(onionFrameNumber, 0), onionFrameNumber, onionFrameNumber - prevKeyNumber, nextKeyNumber - prevKeyNumber, m_editor->backwardColor(), interpOpacity * opacity, m_editor->tintFactor(), false);
            }
        }
    }
    // Draw next frames
    onionFrameNumber = m_editor->playback()->currentFrame();
    for (int i = 1; i <= eqValues.maxDistance; i++) {
        qreal opacity = layer->opacity() * qreal(eqValues.value[i]) / 100.;
        onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, m_editor->getEqMode() == KEYS);
        if (layer->isVectorKeyFrameSelected(layer->getVectorKeyFrameAtFrame(currentFrame)) && layer->getLastKeyFrameSelected() == currentFrame){
            onionFrameNumber = layer->getFirstKeyFrameSelected();
        }
        if (onionFrameNumber >= layer->getMaxKeyFramePosition()) break;
        if (eqValues.state[i]) {
            VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(onionFrameNumber);
            qreal interpOpacity = (eqValues.maxDistance + 1 - i) / (float)(eqValues.maxDistance + 1);
            if (keyframe != nullptr) {
                drawKeyFrame(keyframe, onionFrameNumber, 0, layer->stride(onionFrameNumber), m_editor->forwardColor(), interpOpacity * opacity, m_editor->tintFactor(), false);
            } else {
                int prevKeyNumber = layer->getLastKeyFramePosition(onionFrameNumber);
                int nextKeyNumber = layer->getNextKeyFramePosition(onionFrameNumber);
                drawKeyFrame(layer->getLastVectorKeyFrameAtFrame(onionFrameNumber, 0), onionFrameNumber, onionFrameNumber - prevKeyNumber, nextKeyNumber - prevKeyNumber, m_editor->forwardColor(), interpOpacity * opacity, m_editor->tintFactor(), false);
            }
        }
    }

    // Draw the previous keyframe target
    if (k_displayPrevTarget) {
        onionFrameNumber = layer->getPreviousFrameNumber(currentFrame, true);
        if (onionFrameNumber >= layer->firstKeyFramePosition()) {
            VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(onionFrameNumber);
            drawKeyFrame(keyframe, onionFrameNumber, layer->stride(onionFrameNumber), layer->stride(onionFrameNumber), Qt::darkRed, 1.0f, m_editor->tintFactor(), false);
        }
    }
}

void TabletCanvas::drawSelectedGroupsLifetime(Layer *layer, VectorKeyFrame *keyframe, int frame, int inbetween, int stride) {
    int keyframeNumber = layer->getVectorKeyFramePosition(keyframe);

    for (Group *group : keyframe->selection().selectedPostGroups()) {
        if (inbetween > 0) {
            drawGroup(keyframe, group, 0.0f, 0, stride, layer->opacity(), Qt::darkGray, 100.0);
        }

        // Draw next breakdown keyframes and the last end keyframe
        Group *prev = group;
        Group *next = group->nextPostGroup();
        VectorKeyFrame *prevKey;
        int stride;
        while (next != nullptr) {
            prevKey = next->getParentKeyframe();
            stride = layer->stride(layer->getVectorKeyFramePosition(prevKey));
            drawGroup(prevKey, next, 0.0f, 0, stride, layer->opacity(), prev == group ? Qt::darkGray : Qt::lightGray, 100.0);
            prev = next;
            next = next->nextPostGroup();
        }
        prevKey = prev->getParentKeyframe();
        stride = layer->stride(layer->getVectorKeyFramePosition(prevKey));
        drawGroup(prevKey, prev, 1.0, stride, stride, 0.8, Qt::lightGray, 100.0, 0.7);

        // Draw previous breakdown keyframes
        prev = group->prevPostGroup();
        while (prev != nullptr) {
            prevKey = prev->getParentKeyframe();
            stride = layer->stride(layer->getVectorKeyFramePosition(prevKey));
            drawGroup(prevKey, prev, 0.0f, 0, stride, layer->opacity(), Qt::lightGray, 100.0);
            prev = prev->prevPostGroup();
        }
    }
}

void TabletCanvas::drawGroup(VectorKeyFrame *keyframe, Group *group, qreal alpha, int inbetween, int stride, double opacity, const QColor &color, double tint, double strokeWeightFactor) {
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    if (k_drawSplat && k_drawOffscreen) {
        startDrawSplatStrokes();
    }

    QOpenGLShaderProgram *program = k_drawSplat ? m_splattingProgram : m_strokeProgram;
    program->bind();
    keyframe->paintGroupGL(program, context()->functions(), alpha, opacity, group, inbetween, color, tint, strokeWeightFactor, false, true, true);
    program->release();

    if (k_drawSplat && k_drawOffscreen) {
        endDrawSplatStrokes();
    }
}

void TabletCanvas::drawSelectedGroups(VectorKeyFrame *keyframe, GroupType type, qreal alpha, int inbetween, int stride, double opacity, const QColor &color, double tint,
                                      double strokeWeightFactor) {
    inbetween = m_editor->updateInbetweens(keyframe, inbetween, stride);
    const QMap<int, Group *> &groups = (type == POST) ? keyframe->selection().selectedPostGroups() : keyframe->selection().selectedPreGroups();
    if (k_drawSplat && k_drawOffscreen) {
        startDrawSplatStrokes();
    }

    QOpenGLShaderProgram *program = k_drawSplat ? m_splattingProgram : m_strokeProgram;
    program->bind();
    for (Group *group : groups) {
        keyframe->paintGroupGL(program, context()->functions(), alpha, opacity, group, inbetween, color, tint, strokeWeightFactor, m_drawGroupColor, true, true);
    }
    program->release();

    if (k_drawSplat && k_drawOffscreen) {
        endDrawSplatStrokes();
    }
}

void TabletCanvas::drawSelectedGroups(VectorKeyFrame *keyframe, GroupType type, double opacity, const QColor &color, double tint, double strokeWeightFactor) {
    const QMap<int, Group *> &groups = (type == POST) ? keyframe->selection().selectedPostGroups() : keyframe->selection().selectedPreGroups();
    if (k_drawSplat && k_drawOffscreen) {
        startDrawSplatStrokes();
    }

    QOpenGLShaderProgram *program = k_drawSplat ? m_splattingProgram : m_strokeProgram;
    program->bind();
    for (Group *group : groups) {
        keyframe->paintGroupGL(program, context()->functions(), opacity, group, color, tint, strokeWeightFactor, m_drawGroupColor, true);
    }
    program->release();

    if (k_drawSplat && k_drawOffscreen) {
        endDrawSplatStrokes();
    }
}

void TabletCanvas::drawMask(VectorKeyFrame *keyframe, Group *group, int inbetween, int stride, qreal alpha, int depth) {
    if (!keyframe->parentLayer()->hasMask() || group == nullptr || group->lattice() == nullptr || !group->lattice()->isSingleConnectedComponent()) return;

    int inbetweenFrame = m_editor->updateInbetweens(keyframe, inbetween, stride);
    int size = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size() - 1;

    if (!m_displayMask) {
        GLint blendEq, sFactor, dFactor;
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEq);
        glGetIntegerv(GL_BLEND_SRC_RGB, &sFactor);
        glGetIntegerv(GL_BLEND_DST_RGB, &dFactor);

        m_offscreenRenderMSFBO->release();
        m_offscreenRenderFBO->bind();
        m_maskProgram->bind();

        // glDepthFunc(GL_ALWAYS);
        glBlendEquation(GL_MAX);
        glBlendFunc(GL_ONE, GL_ONE);
        GLenum drawBuffer[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, drawBuffer);

        m_maskProgram->setUniformValue("depth", (size - depth) / (float)(size + 1));
        group->drawMask(m_maskProgram, inbetweenFrame, alpha, group->color()); 

        m_maskProgram->release();
        m_offscreenRenderFBO->release();
        m_offscreenRenderMSFBO->bind();

        glBlendEquation(blendEq);
        glBlendFunc(sFactor, dFactor);

    }

    if (k_displayMask || m_displayMask) {
        m_displayMaskProgram->bind();
        // QColor c = QColor::fromHsvF(Utils::lerp(0.0, 0.07777, (double)depth/size), 1.0, 1.0);
        QColor c = sampleColorMap(size - depth);
        group->drawMask(m_displayMaskProgram, inbetweenFrame, alpha, c); 
        m_displayMaskProgram->release();
    }
}

void TabletCanvas::drawExportOnionSkins(Layer *layer) {
    int maxFrame = k_exportTo == 0 ? layer->getMaxKeyFramePosition() : k_exportTo;
    VectorKeyFrame *last = prevKeyFrame();
    if (!k_exportOnlyKeysMode) {
        for (int i = std::max(k_exportFrom.value(), layer->firstKeyFramePosition()); i < maxFrame; ++i) {
            if (!layer->keyExists(i)) {
                int prevKeyNumber = layer->getLastKeyFramePosition(i);
                int nextKeyNumber = layer->getNextKeyFramePosition(i);
                VectorKeyFrame *prevFrame = layer->getVectorKeyFrameAtFrame(prevKeyNumber);
                if (k_exportOnlyCurSegment && prevFrame != last) continue;
                drawKeyFrame(layer->getLastVectorKeyFrameAtFrame(i, 0), i, i - prevKeyNumber, nextKeyNumber - prevKeyNumber, m_editor->forwardColor(), 0.4, m_editor->tintFactor());
            }
        }
    }
    for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
        if (it.key() < k_exportFrom) continue;
        if (it.key() >= maxFrame) continue;
        if (layer->keyExists(it.key())) {
            VectorKeyFrame *keyframe = it.value();
            if (k_exportOnlyCurSegment && keyframe != last) continue;
            drawKeyFrame(keyframe, it.key(), 0, layer->stride(it.key()), Qt::black, layer->opacity(), 0.0);
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
                    m_strokeProgram->bind();
                    keyframe->paintGroupGL(m_strokeProgram, context()->functions(), 1.0f, 0.4, group, ib, m_editor->forwardColor(), 1.0, m_drawGroupColor, true);
                    m_strokeProgram->release();
                }
            }
        }
    }
}

void TabletCanvas::startDrawSplatStrokes() {
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_blendEq);
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_sFactor);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_dFactor);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void TabletCanvas::endDrawSplatStrokes() {
    glBlendEquation(m_blendEq);
    glBlendFunc(m_sFactor, m_dFactor);
}

void TabletCanvas::drawToolGizmos(QPainter &painter) {
    m_canvasFont.setPointSize(24);
    painter.setFont(m_canvasFont);
    if (m_editor->tools()->currentTool() != nullptr && !m_editor->playback()->isPlaying()) {
        m_editor->tools()->currentTool()->drawUI(painter, prevKeyFrame());
    }
    if (m_temporarySelectTool) {
        m_editor->tools()->tool(Tool::Select)->drawUI(painter, prevKeyFrame());
    }
    if (m_infoMessageDuration.isActive()) {
        setFontSize(24.0f * (1.0f / m_editor->view()->scaling()));
        painter.setFont(m_editor->tabletCanvas()->canvasFont());
        painter.setPen(Qt::black);
        painter.drawText(m_editor->view()->mapScreenToCanvas(QPointF(50, 50)), m_infoMessageText);
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
    if (k_showBackground) {
        painter.drawPixmap(-m_backgrounds[count].width() / 2, -m_backgrounds[count].height() / 2, m_backgrounds[count]);
    }
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
            newColor.setAlpha(std::max(abs(vValue - 127), abs(hValue - 127)));
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
            m_pen.setWidthF(std::max(abs(vValue - 127), abs(hValue - 127)) / 12);
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

QColor TabletCanvas::sampleColorMap(double depth) {
    static const int max = 0.9 * (m_infernoColorMap.width() - 1);
    static const int span = 0.7 * (m_infernoColorMap.width() - 1);
    return m_infernoColorMap.pixel(max - span * std::tanh(depth * (k_depthColorScaling / 100.0)), 0);
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
    backgroundsList.sort();
    for (QString path : backgroundsList) {
        qDebug() << "Loading background: " << k_backgroundDir.value() + QString("/") + path;
        m_backgrounds.emplace_back(QPixmap(k_backgroundDir.value() + QString("/") + path));
    }


    updateCurrentFrame();
}

void TabletCanvas::toggleDisplayMask(bool b) {
    setMaskOcclusionMode(b ? TabletCanvas::MaskGrayOut : TabletCanvas::MaskOcclude);
    setDisplayMask(b);
    setDisplayDepth(b);
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
    qDebug() << "Stride: " << layer->stride(currentFrame);
    qDebug() << "Inbetween pos: " << layer->inbetweenPosition(currentFrame);
    qDebug() << "Nb of strokes: " << key->strokes().size();
    
    int count = 0;
    for (Group *group : key->postGroups()) {
        count += group->strokes().nbPoints();
    }
    qDebug() << "Nb of strokes vertices: " << count;

    qDebug() << "Nb of post groups: " << key->postGroups().size();
    qDebug() << "Nb of pre groups: " << key->preGroups().size();
    qDebug() << "Nb of partial group order: " << key->orderPartials().size(); 
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
    key->orderPartials().debug();
    qDebug() << "** Selection";
    qDebug() << ">>> Selected post groups <<<";
    for (Group *group : key->selection().selectedPostGroups()) {
        qDebug() << "Group: " << group << "(" << group->id() << ")";
        qDebug() << "Nb of strokes: " << group->strokes().size();
        qDebug() << "Breakdown: " << group->breakdown();
        qDebug() << "Nb of partial drawing: " << group->drawingPartials().size(); 
        group->drawingPartials().debug();
        qDebug() << "Nb of forward UVs: " << group->uvs().size();
        qDebug() << "Nb of backward Uvs: " << group->backwardUVs().size();
        if (group->lattice()) {
            qDebug() << "Nb quads: " << group->lattice()->size();
            qDebug() << "Nb corners: " << group->lattice()->corners().size();
            qDebug() << "Nb of trajectory constraints (grid): " << group->lattice()->nbConstraints();
            qDebug() << "Is the grid fully connected? " << group->lattice()->isConnected();
            qDebug() << "Motion energy (center of mass): " << group->lattice()->motionEnergy2D().norm();
            qDebug() << "Pin errors: ";
            for (QuadPtr q : group->lattice()->quads()) {
                if (q->isPinned()) {
                    qDebug() << "    " << (q->pinPos() - q->getPoint(q->pinUV(), TARGET_POS)).norm();
                }
            }
        }
        qDebug() << "Nb of control points (spacing): " << group->spacing()->curve()->nbPoints();
        qDebug() << "------";
    }
    if (key->selection().selectedTrajectoryPtr() != nullptr) {
        qDebug() << ">>> Selected trajectory <<<";
        qDebug() << "Local offsets: ";
        for (int i = 0; i < key->selection().selectedTrajectory()->localOffset()->curve()->nbPoints(); ++i) {
            Eigen::Vector2d p = key->selection().selectedTrajectory()->localOffset()->curve()->point(i);
            std::cout << p.transpose() << "   ";
        }
        std::cout << std::endl;
    }

    qDebug() << "******* END DEBUG REPORT";


    // for (unsigned int i = 0; i < 10; ++i) {
    //     for (unsigned int j = 0; j < 10; ++j) {
    //         unsigned int c = Utils::cantor(i, j);
    //         auto invC = Utils::invCantor(c);
    //         if (invC.first != i || invC.second != j) {
    //             qDebug() << "cantor(" << i << ", " << j << ") = " << c;
    //             qDebug() << "but invCantor(" << c << ") = (" << invC.first << ", " << invC.second;
    //         }
    //     }
    // }
}